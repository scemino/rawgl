#pragma once
#include "graphics.h"

struct SokolSystem;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct  {
    uint8_t* buffer[4];
} gfx_fb;

#ifdef __cplusplus
} /* extern "C" */
#endif

struct GraphicsSokol: Graphics {
	typedef void (GraphicsSokol::*drawLine)(int16_t x1, int16_t x2, int16_t y, uint8_t col);

	uint8_t *_pagePtrs[4];
	uint8_t *_drawPagePtr;
	int _u, _v;
	Color _pal[16];
    uint32_t _palette[16];
	uint8_t _colorBuffer[GFX_W * GFX_H];
	SokolSystem *_stub;

	GraphicsSokol();

	int xScale(int x) const { return (x * _u) >> 16; }
	int yScale(int y) const { return (y * _v) >> 16; }

    void setBuffers(gfx_fb* fb);
	void drawPolygon(uint8_t color, const QuadStrip &qs);
	void drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color);
	void drawSpriteMask(int x, int y, uint8_t color, const uint8_t *data);
	void drawPoint(int16_t x, int16_t y, uint8_t color);
	void drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	uint8_t *getPagePtr(uint8_t page);
	int getPageSize() const { return GFX_W * GFX_H; }
	void setWorkPagePtr(uint8_t page);
    void setSystem(SokolSystem* sys) { _stub = sys; }

    virtual void init();
	virtual void setFont(const uint8_t *src, int w, int h);
	virtual void setPalette(const Color *colors, int count);
	virtual void setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize);
	virtual void drawSprite(int buffer, int num, const Point *pt, uint8_t color);
	virtual void drawBitmap(int buffer, const uint8_t *data, int w, int h, int fmt);
	virtual void drawPoint(int buffer, uint8_t color, const Point *pt);
	virtual void drawQuadStrip(int buffer, uint8_t color, const QuadStrip *qs);
	virtual void drawStringChar(int buffer, uint8_t color, char c, const Point *pt);
	virtual void clearBuffer(int num, uint8_t color);
	virtual void copyBuffer(int dst, int src, int vscroll = 0);
	virtual void drawBuffer(int num, SystemStub *stub);
	virtual void drawBitmapOverlay(const uint8_t *data, int w, int h, int fmt, SystemStub *stub);
};
