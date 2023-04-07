#include <map>
#include "aifcplayer.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "util.h"
#include "gfx.h"
#include "game.h"

enum {
  TAG_RIFF = 0x46464952,
  TAG_WAVE = 0x45564157,
  TAG_fmt = 0x20746D66,
  TAG_data = 0x61746164
};

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

  void initWav(const uint8_t *data, int freq, int volume, int mixingFreq,
               int len, bool bits16, bool stereo, bool loop) {
    _data = data;
    _pos.reset(freq, mixingFreq);

    _len = len;
    _loopLen = loop ? len : 0;
    _loopPos = 0;
    _volume = volume;
    _mixWav = bits16 ? (stereo ? &MixerChannel::mixWav<16, true>
                               : &MixerChannel::mixWav<16, false>)
                     : (stereo ? &MixerChannel::mixWav<8, true>
                               : &MixerChannel::mixWav<8, false>);
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

static const uint8_t *loadWav(const uint8_t *data, int &freq, int &len,
                              bool &bits16, bool &stereo) {
  uint32_t riffMagic = READ_LE_UINT32(data);
  if (riffMagic != TAG_RIFF)
    return 0;
  uint32_t riffLength = READ_LE_UINT32(data + 4);
  uint32_t waveMagic = READ_LE_UINT32(data + 8);
  if (waveMagic != TAG_WAVE)
    return 0;
  uint32_t offset = 12;
  uint32_t chunkMagic, chunkLength = 0;
  // find fmt chunk
  do {
    offset += chunkLength + (chunkLength & 1);
    if (offset >= riffLength)
      return 0;
    chunkMagic = READ_LE_UINT32(data + offset);
    chunkLength = READ_LE_UINT32(data + offset + 4);
    offset += 8;
  } while (chunkMagic != TAG_fmt);

  if (chunkLength < 14)
    return 0;
  if (offset + chunkLength >= riffLength)
    return 0;

  // read format
  int formatTag = READ_LE_UINT16(data + offset);
  int channels = READ_LE_UINT16(data + offset + 2);
  int samplesPerSec = READ_LE_UINT32(data + offset + 4);
  int bitsPerSample = 0;
  if (chunkLength >= 16) {
    bitsPerSample = READ_LE_UINT16(data + offset + 14);
  } else if (formatTag == 1 && channels != 0) {
    int blockAlign = READ_LE_UINT16(data + offset + 12);
    bitsPerSample = (blockAlign * 8) / channels;
  }
  // check supported format
  if ((formatTag != 1) ||                            // PCM
      (channels != 1 && channels != 2) ||            // mono or stereo
      (bitsPerSample != 8 && bitsPerSample != 16)) { // 8bit or 16bit
    warning("Unsupported wave file");
    return 0;
  }

  // find data chunk
  do {
    offset += chunkLength + (chunkLength & 1);
    if (offset >= riffLength)
      return 0;
    chunkMagic = READ_LE_UINT32(data + offset);
    chunkLength = READ_LE_UINT32(data + offset + 4);
    offset += 8;
  } while (chunkMagic != TAG_data);

  uint32_t lengthSamples = chunkLength;
  if (offset + lengthSamples - 4 > riffLength) {
    lengthSamples = riffLength + 4 - offset;
  }
  if (channels == 2)
    lengthSamples >>= 1;
  if (bitsPerSample == 16)
    lengthSamples >>= 1;

  freq = samplesPerSec;
  len = lengthSamples;
  bits16 = (bitsPerSample == 16);
  stereo = (channels == 2);

  return data + offset;
}

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

  void playSoundWav(uint8_t channel, const uint8_t *data, int freq,
                    uint8_t volume, bool loop) {
    int wavFreq, len;
    bool bits16, stereo;
    const uint8_t *wavData = loadWav(data, wavFreq, len, bits16, stereo);
    if (!wavData)
      return;

    if (wavFreq == 22050 || wavFreq == 44100 || wavFreq == 48000) {
      freq = (int)(freq * (wavFreq / 9943.0f));
    }

    _channels[channel].initWav(wavData, freq, volume, kMixFreq, len, bits16,
                               stereo, loop);
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

  void mixChannelsWav(int16_t *samples, int count) {
    for (int i = 0; i < kMixChannels; ++i) {
      if (_channels[i]._data) {
        (_channels[i].*_channels[i]._mixWav)(samples, count);
      }
    }
  }

  static void mixAudioWav(void *data, uint8_t *s16buf, int len) {
    Mixer_impl *mixer = (Mixer_impl *)data;
    mixer->mixChannelsWav((int16_t *)s16buf, len / sizeof(int16_t));
  }

  void stopAll() {
    for (int i = 0; i < kMixChannels; ++i) {
      stopSound(i);
    }
  }

};

Mixer::Mixer(SfxPlayer *sfx) : _aifc(0), _sfx(sfx) {}

void Mixer::init(MixerType mixerType) {
  _impl = new Mixer_impl();
  _impl->init(callback);
}

void Mixer::quit() {
  stopAll();
  if (_impl) {
    _impl->quit();
    delete _impl;
  }
  delete _aifc;
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

void Mixer::playSoundWav(uint8_t channel, const uint8_t *data, uint16_t freq,
                         uint8_t volume, uint8_t loop) {
  debug(DBG_SND, "Mixer::playSoundWav(%d, %d, %d)", channel, volume, loop);
  if (_impl) {
    return _impl->playSoundWav(channel, data, freq, volume, loop);
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

void Mixer::playAifcMusic(const char *path, uint32_t offset) {
  debug(DBG_SND, "Mixer::playAifcMusic(%s)", path);
}

void Mixer::stopAifcMusic() {
  debug(DBG_SND, "Mixer::stopAifcMusic()");
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

void Mixer::preloadSoundAiff(uint8_t num, const uint8_t *data) {
  debug(DBG_SND, "Mixer::preloadSoundAiff(num:%d, data:%p)", num, data);
}

void Mixer::playSoundAiff(uint8_t channel, uint8_t num, uint8_t volume) {
  debug(DBG_SND, "Mixer::playSoundAiff()");
}
