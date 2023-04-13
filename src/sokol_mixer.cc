#include <map>
#include "mixer.h"
#include "sfxplayer.h"
#include "util.h"
#include "gfx.h"
#include "game.h"

static const bool kAmigaStereoChannels = false; // 0,3:left 1,2:right

static int16_t toS16(int a) { return ((a << 8) | a) - 32768; }

static int16_t mixS16(int sample1, int sample2) {
  const int sample = sample1 + sample2;
  return sample < -32768 ? -32768 : ((sample > 32767 ? 32767 : sample));
}

struct MixerChannel {
  const uint8_t *_data;
  Frac _pos;
  uint32_t _len;
  uint32_t _loopLen, _loopPos;
  int _volume;
  void (MixerChannel::*_mixWav)(int16_t *sample, int count);

  void initRaw(const uint8_t *data, int freq, int volume, int mixingFreq) {
    _data = data + 8;
    _pos.reset(freq, mixingFreq);

    const int len = READ_BE_UINT16(data) * 2;
    _loopLen = READ_BE_UINT16(data + 2) * 2;
    _loopPos = _loopLen ? len : 0;
    _len = len;

    _volume = volume;
  }

  void mixRaw(int16_t &sample) {
    if (_data) {
      uint32_t pos = _pos.getInt();
      _pos.offset += _pos.inc;
      if (_loopLen != 0) {
        if (pos >= _loopPos + _loopLen) {
          pos = _loopPos;
          _pos.offset = (_loopPos << Frac::BITS) + _pos.inc;
        }
      } else {
        if (pos >= _len) {
          _data = 0;
          return;
        }
      }
      sample = mixS16(sample, toS16(_data[pos] ^ 0x80) * _volume / 64);
    }
  }

  template <int bits, bool stereo> void mixWav(int16_t *samples, int count) {
    for (int i = 0; i < count; i += 2) {
      uint32_t pos = _pos.getInt();
      _pos.offset += _pos.inc;
      if (pos >= _len) {
        if (_loopLen != 0) {
          pos = 0;
          _pos.offset = _pos.inc;
        } else {
          _data = 0;
          break;
        }
      }
      if (stereo) {
        pos *= 2;
      }
      int valueL;
      if (bits == 8) { // U8
        valueL = toS16(_data[pos]) * _volume / 64;
      } else { // S16
        valueL = ((int16_t)READ_LE_UINT16(&_data[pos * sizeof(int16_t)])) *
                 _volume / 64;
      }
      *samples = mixS16(*samples, valueL);
      ++samples;

      int valueR;
      if (!stereo) {
        valueR = valueL;
      } else {
        if (bits == 8) { // U8
          valueR = toS16(_data[pos + 1]) * _volume / 64;
        } else { // S16
          valueR =
              ((int16_t)READ_LE_UINT16(&_data[(pos + 1) * sizeof(int16_t)])) *
              _volume / 64;
        }
      }
      *samples = mixS16(*samples, valueR);
      ++samples;
    }
  }
};

struct Mixer_impl {

  static const int kMixFreq = 44100;
  static const int kMixBufSize = 4096*8;
  static const int kMixChannels = 4;

  MixerChannel _channels[kMixChannels];
  SfxPlayer *_sfx;
  game_audio_callback_t callback;
  float sample_buffer[GAME_MAX_AUDIO_SAMPLES];
  int16_t samples[kMixBufSize];

  void init(game_audio_callback_t callback) {
    memset(_channels, 0, sizeof(_channels));
    this->callback = callback;
    for (int i = 0; i < kMixChannels; ++i) {
      _channels[i]._mixWav = &MixerChannel::mixWav<8, false>;
    }
    _sfx = 0;
  }

  void quit() { stopAll(); }

  void update(int num_samples) {
    assert(num_samples < kMixBufSize);
    assert(num_samples < GAME_MAX_AUDIO_SAMPLES);
    memset(samples, 0, num_samples*sizeof(int16_t));
    mixChannels(samples, num_samples);
    if (_sfx) {
        _sfx->readSamples((int16_t *)samples, num_samples);
    }
    for (int i=0; i<num_samples; i++) {
        sample_buffer[i] = ((((float)samples[i])+32768.f)/32768.f) - 1.0f;
    }
    callback.func(sample_buffer, num_samples, callback.user_data);
  }

  void playSoundRaw(uint8_t channel, const uint8_t *data, int freq,
                    uint8_t volume) {
    _channels[channel].initRaw(data, freq, volume, kMixFreq);
  }

  void stopSound(uint8_t channel) {
    _channels[channel]._data = 0;
  }

  void setChannelVolume(uint8_t channel, uint8_t volume) {
    _channels[channel]._volume = volume;
  }

  void playSfxMusic(SfxPlayer *sfx) {
    _sfx = sfx;
    _sfx->play(kMixFreq);
  }
  void stopSfxMusic() {
    if (_sfx) {
      _sfx->stop();
      _sfx = 0;
    }
  }

  void mixChannels(int16_t *samples, int count) {
    if (kAmigaStereoChannels) {
     for (int i = 0; i < count; i += 2) {
       _channels[0].mixRaw(*samples);
       _channels[3].mixRaw(*samples);
       ++samples;
       _channels[1].mixRaw(*samples);
       _channels[2].mixRaw(*samples);
       ++samples;
     }
   } else {
     for (int i = 0; i < count; i += 2) {
       for (int j = 0; j < kMixChannels; ++j) {
         _channels[j].mixRaw(samples[i]);
       }
       samples[i + 1] = samples[i];
     }
   }
  }

  void stopAll() {
    for (int i = 0; i < kMixChannels; ++i) {
      stopSound(i);
    }
  }

};

Mixer::Mixer(SfxPlayer *sfx) : _sfx(sfx) {}

void Mixer::init() {
  _impl = new Mixer_impl();
  _impl->init(callback);
}

void Mixer::quit() {
  stopAll();
  if (_impl) {
    _impl->quit();
    delete _impl;
  }
}

void Mixer::update(int num_samples) {
  if (_impl) {
    _impl->update(num_samples);
  }
}

void Mixer::playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq,
                         uint8_t volume) {
  debug(DBG_SND, "Mixer::playChannel(%d, %d, %d)", channel, freq, volume);
  if (_impl) {
    return _impl->playSoundRaw(channel, data, freq, volume);
  }
}

void Mixer::stopSound(uint8_t channel) {
  debug(DBG_SND, "Mixer::stopChannel(%d)", channel);
  if (_impl) {
    return _impl->stopSound(channel);
  }
}

void Mixer::setChannelVolume(uint8_t channel, uint8_t volume) {
  debug(DBG_SND, "Mixer::setChannelVolume(%d, %d)", channel, volume);
  if (_impl) {
    return _impl->setChannelVolume(channel, volume);
  }
}

void Mixer::playMusic(const char *path, uint8_t loop) {
  debug(DBG_SND, "Mixer::playMusic(%s, %d)", path, loop);
}

void Mixer::stopMusic() {
  debug(DBG_SND, "Mixer::stopMusic()");
}

void Mixer::playSfxMusic(int num) {
  debug(DBG_SND, "Mixer::playSfxMusic(%d)", num);
  if (_impl && _sfx) {
    return _impl->playSfxMusic(_sfx);
  }
}

void Mixer::stopSfxMusic() {
  debug(DBG_SND, "Mixer::stopSfxMusic()");
  if (_impl && _sfx) {
    return _impl->stopSfxMusic();
  }
}

void Mixer::stopAll() {
  debug(DBG_SND, "Mixer::stopAll()");
  if (_impl) {
    return _impl->stopAll();
  }
}
