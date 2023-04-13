
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef VIDEO_H__
#define VIDEO_H__

#include "intern.h"

struct StrEntry {
	uint16_t id;
	const char *str;
};

struct Graphics;
struct Resource;
struct Scaler;
struct SystemStub;

struct Video {

	enum {
		BITMAP_W = 320,
		BITMAP_H = 200
	};

	static const StrEntry _stringsTableFr[];
	static const StrEntry _stringsTableEng[];
	static const StrEntry _stringsTableDemo[];
	static const char *_str0x194AtariDemo;
	static const uint8_t _paletteEGA[];

	static bool _useEGA;

	Resource *_res;
	Graphics *_graphics;

	uint8_t _nextPal, _currentPal;
	uint8_t _buffers[3];
	Ptr _pData;
	uint8_t *_dataBuf;
	const StrEntry *_stringsTable;
	uint8_t _tempBitmap[BITMAP_W * BITMAP_H];

	Video(Resource *res);
	~Video();
	void init();

	void setDefaultFont();
	void setDataBuffer(uint8_t *dataBuf, uint16_t offset);
	void drawShape(uint8_t color, uint16_t zoom, const Point *pt);
	void fillPolygon(uint16_t color, uint16_t zoom, const Point *pt);
	void drawShapeParts(uint16_t zoom, const Point *pt);
	void drawString(uint8_t color, uint16_t x, uint16_t y, uint16_t strId);
	uint8_t getPagePtr(uint8_t page);
	void setWorkPagePtr(uint8_t page);
	void fillPage(uint8_t page, uint8_t color);
	void copyPage(uint8_t src, uint8_t dst, int16_t vscroll);
	void scaleBitmap(const uint8_t *src, int fmt);
	void copyBitmapPtr(const uint8_t *src, uint32_t size = 0);
	void changePal(uint8_t pal);
	void updateDisplay(uint8_t page, SystemStub *stub);
	void captureDisplay();
	void setPaletteColor(uint8_t color, int r, int g, int b);
};

#endif
