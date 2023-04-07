#include "aifcplayer.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "util.h"


Mixer::Mixer(SfxPlayer *sfx) {
}

void Mixer::init(MixerType mixerType) {
}

void Mixer::quit() {
}

void Mixer::update() {
}

void Mixer::playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::playChannel(%d, %d, %d)", channel, freq, volume);
}

void Mixer::playSoundWav(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume, uint8_t loop) {
	debug(DBG_SND, "Mixer::playSoundWav(%d, %d, %d)", channel, volume, loop);
}

void Mixer::stopSound(uint8_t channel) {
	debug(DBG_SND, "Mixer::stopChannel(%d)", channel);
}

void Mixer::setChannelVolume(uint8_t channel, uint8_t volume) {
	debug(DBG_SND, "Mixer::setChannelVolume(%d, %d)", channel, volume);
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
}

void Mixer::stopSfxMusic() {
	debug(DBG_SND, "Mixer::stopSfxMusic()");
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
}

void Mixer::preloadSoundAiff(uint8_t num, const uint8_t *data) {
	debug(DBG_SND, "Mixer::preloadSoundAiff(num:%d, data:%p)", num, data);
}

void Mixer::playSoundAiff(uint8_t channel, uint8_t num, uint8_t volume) {
	debug(DBG_SND, "Mixer::playSoundAiff()");
}
