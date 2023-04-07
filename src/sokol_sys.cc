#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "systemstub.h"
#include "sokol_sys.h"

void SokolSystem::setBuffer(uint8_t* fb, uint32_t* palette) {
    _fb = fb;
    _palette = palette;
}

void SokolSystem::init(const char *title, const DisplayMode *dm) {
    _elapsed = 0;
    _sleep = 0;
}

void SokolSystem::fini() {}

void SokolSystem::prepareScreen(int &w, int &h, float ar[4]) {}

void SokolSystem::updateScreen() {}

void SokolSystem::setScreenPixels555(const uint16_t *data, int w, int h) {
    uint32_t* fb = (uint32_t*)_fb;
    for(int i=0; i<w*h; i++) {
        uint32_t col = data[i];
        col = ((col & 0x7c00) >> 7) | ((col & 0x03e0) << 6) | ((col & 0x001f) << 19);
        fb[i] = col;
    }
}

void SokolSystem::setScreenPixels(const uint8_t *data, int w, int h) {
    memcpy(_fb, data, w*h);
}

void SokolSystem::setPalette(const uint32_t *data) {
    memcpy(_palette, data, 16 * sizeof(uint32_t));
}

void SokolSystem::processEvents() {}

void SokolSystem::sleep(uint32_t duration) {
    _sleep += duration;
}

uint32_t SokolSystem::getTimeStamp() { return _elapsed; }
