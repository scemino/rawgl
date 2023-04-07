#pragma once

struct SokolSystem: SystemStub {
    void setBuffer(uint8_t* fb, uint32_t* palette);

    virtual void init(const char *title, const DisplayMode *dm);
	virtual void fini();

	virtual void prepareScreen(int &w, int &h, float ar[4]);
	virtual void updateScreen();
	virtual void setScreenPixels555(const uint16_t *data, int w, int h);
    virtual void setScreenPixels(const uint8_t *data, int w, int h);
    virtual void setPalette(const uint32_t *data);

	virtual void processEvents();
	virtual void sleep(uint32_t duration);
	virtual uint32_t getTimeStamp();

    uint8_t* _fb;
    uint32_t* _palette;
    uint32_t _elapsed;
    uint32_t _sleep;
};
