/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "video.h"
#include "bitmap.h"
#include "graphics.h"
#include "resource.h"
#include "systemstub.h"
#include "util.h"


Video::Video(Resource *res)
	: _res(res), _graphics(0) {
}

Video::~Video() {
}

void Video::init() {
	_nextPal = _currentPal = 0xFF;
	_buffers[2] = getPagePtr(1);
	_buffers[1] = getPagePtr(2);
	setWorkPagePtr(0xFE);
}

void Video::setDefaultFont() {
	_graphics->setFont(0, 0, 0);
}

void Video::setDataBuffer(uint8_t *dataBuf, uint16_t offset) {
	_dataBuf = dataBuf;
	_pData.pc = dataBuf + offset;
}

void Video::drawShape(uint8_t color, uint16_t zoom, const Point *pt) {
	uint8_t i = _pData.fetchByte();
	if (i >= 0xC0) {
		if (color & 0x80) {
			color = i & 0x3F;
		}
		fillPolygon(color, zoom, pt);
	} else {
		i &= 0x3F;
		if (i == 1) {
			warning("Video::drawShape() ec=0x%X (i != 2)", 0xF80);
		} else if (i == 2) {
			drawShapeParts(zoom, pt);
		} else {
			warning("Video::drawShape() ec=0x%X (i != 2)", 0xFBB);
		}
	}
}

void Video::fillPolygon(uint16_t color, uint16_t zoom, const Point *pt) {
	const uint8_t *p = _pData.pc;

	uint16_t bbw = (*p++) * zoom / 64;
	uint16_t bbh = (*p++) * zoom / 64;

	int16_t x1 = pt->x - bbw / 2;
	int16_t x2 = pt->x + bbw / 2;
	int16_t y1 = pt->y - bbh / 2;
	int16_t y2 = pt->y + bbh / 2;

	if (x1 > 319 || x2 < 0 || y1 > 199 || y2 < 0)
		return;

	QuadStrip qs;
	qs.numVertices = *p++;
	if ((qs.numVertices & 1) != 0) {
		warning("Unexpected number of vertices %d", qs.numVertices);
		return;
	}
	assert(qs.numVertices < QuadStrip::MAX_VERTICES);

	for (int i = 0; i < qs.numVertices; ++i) {
		Point *v = &qs.vertices[i];
		v->x = x1 + (*p++) * zoom / 64;
		v->y = y1 + (*p++) * zoom / 64;
	}

	if (qs.numVertices == 4 && bbw == 0 && bbh <= 1) {
		_graphics->drawPoint(_buffers[0], color, pt);
	} else {
		_graphics->drawQuadStrip(_buffers[0], color, &qs);
	}
}

void Video::drawShapeParts(uint16_t zoom, const Point *pgc) {
	Point pt;
	pt.x = pgc->x - _pData.fetchByte() * zoom / 64;
	pt.y = pgc->y - _pData.fetchByte() * zoom / 64;
	int16_t n = _pData.fetchByte();
	debug(DBG_VIDEO, "Video::drawShapeParts n=%d", n);
	for ( ; n >= 0; --n) {
		uint16_t offset = _pData.fetchWord();
		Point po(pt);
		po.x += _pData.fetchByte() * zoom / 64;
		po.y += _pData.fetchByte() * zoom / 64;
		uint16_t color = 0xFF;
		if (offset & 0x8000) {
			color = _pData.fetchByte();
			const int num = _pData.fetchByte();
			if (Graphics::_is1991) {
				if ((color & 0x80) != 0) {
					_graphics->drawSprite(_buffers[0], num, &po, color & 0x7F);
					continue;
				}
			}
			color &= 0x7F;
		}
		offset <<= 1;
		uint8_t *bak = _pData.pc;
		_pData.pc = _dataBuf + offset;
		drawShape(color, zoom, &po);
		_pData.pc = bak;
	}
}

static const char *findString(const StrEntry *stringsTable, int id) {
	for (const StrEntry *se = stringsTable; se->id != 0xFFFF; ++se) {
		if (se->id == id) {
			return se->str;
		}
	}
	return 0;
}

void Video::drawString(uint8_t color, uint16_t x, uint16_t y, uint16_t strId) {
	bool escapedChars = false;
	const char *str = 0;
	if (_res->getDataType() == Resource::DT_ATARI_DEMO && strId == 0x194) {
		str = _str0x194AtariDemo;
	} else {
		str = findString(_stringsTable, strId);
		if (!str && _res->getDataType() == Resource::DT_DOS) {
			str = findString(_stringsTableDemo, strId);
		}
	}
	if (!str) {
		warning("Unknown string id %d", strId);
		return;
	}
	debug(DBG_VIDEO, "drawString(%d, %d, %d, '%s')", color, x, y, str);
	uint16_t xx = x;
    size_t len = strlen(str);
	for (size_t i = 0; i < len; ++i) {
		if (str[i] == '\n' || str[i] == '\r') {
			y += 8;
			x = xx;
		} else if (str[i] == '\\' && escapedChars) {
			++i;
			if (i < len) {
				switch (str[i]) {
				case 'n':
					y += 8;
					x = xx;
					break;
				}
			}
		} else {
			Point pt(x * 8, y);
			_graphics->drawStringChar(_buffers[0], color, str[i], &pt);
			++x;
		}
	}
}

uint8_t Video::getPagePtr(uint8_t page) {
	uint8_t p;
	if (page <= 3) {
		p = page;
	} else {
		switch (page) {
		case 0xFF:
			p = _buffers[2];
			break;
		case 0xFE:
			p = _buffers[1];
			break;
		default:
			p = 0; // XXX check
			warning("Video::getPagePtr() p != [0,1,2,3,0xFF,0xFE] == 0x%X", page);
			break;
		}
	}
	return p;
}

void Video::setWorkPagePtr(uint8_t page) {
	debug(DBG_VIDEO, "Video::setWorkPagePtr(%d)", page);
	_buffers[0] = getPagePtr(page);
}

void Video::fillPage(uint8_t page, uint8_t color) {
	debug(DBG_VIDEO, "Video::fillPage(%d, %d)", page, color);
	_graphics->clearBuffer(getPagePtr(page), color);
}

void Video::copyPage(uint8_t src, uint8_t dst, int16_t vscroll) {
	debug(DBG_VIDEO, "Video::copyPage(%d, %d)", src, dst);
	if (src >= 0xFE || ((src &= ~0x40) & 0x80) == 0) { // no vscroll
		_graphics->copyBuffer(getPagePtr(dst), getPagePtr(src));
	} else {
		uint8_t sl = getPagePtr(src & 3);
		uint8_t dl = getPagePtr(dst);
		if (sl != dl && vscroll >= -199 && vscroll <= 199) {
			_graphics->copyBuffer(dl, sl, vscroll);
		}
	}
}

static void decode_amiga(const uint8_t *src, uint8_t *dst) {
	static const int plane_size = 200 * 320 / 8;
	for (int y = 0; y < 200; ++y) {
		for (int x = 0; x < 320; x += 8) {
			for (int b = 0; b < 8; ++b) {
				const int mask = 1 << (7 - b);
				uint8_t color = 0;
				for (int p = 0; p < 4; ++p) {
					if (src[p * plane_size] & mask) {
						color |= 1 << p;
					}
				}
				*dst++ = color;
			}
			++src;
		}
	}
}

static void decode_atari(const uint8_t *src, uint8_t *dst) {
	for (int y = 0; y < 200; ++y) {
		for (int x = 0; x < 320; x += 16) {
			for (int b = 0; b < 16; ++b) {
				const int mask = 1 << (15 - b);
				uint8_t color = 0;
				for (int p = 0; p < 4; ++p) {
					if (READ_BE_UINT16(src + p * 2) & mask) {
						color |= 1 << p;
					}
				}
				*dst++ = color;
			}
			src += 8;
		}
	}
}

static void yflip(const uint8_t *src, int w, int h, uint8_t *dst) {
	for (int y = 0; y < h; ++y) {
		memcpy(dst + (h - 1 - y) * w, src, w);
		src += w;
	}
}

void Video::scaleBitmap(const uint8_t *src, int fmt) {
    _graphics->drawBitmap(_buffers[0], src, BITMAP_W, BITMAP_H, fmt);
}

void Video::copyBitmapPtr(const uint8_t *src, uint32_t size) {
	if (_res->getDataType() == Resource::DT_DOS || _res->getDataType() == Resource::DT_AMIGA) {
		decode_amiga(src, _tempBitmap);
		scaleBitmap(_tempBitmap, FMT_CLUT);
	} else if (_res->getDataType() == Resource::DT_ATARI) {
		decode_atari(src, _tempBitmap);
		scaleBitmap(_tempBitmap, FMT_CLUT);
	} else { // .BMP
		if (Graphics::_is1991) {
			const int w = READ_LE_UINT32(src + 0x12);
			const int h = READ_LE_UINT32(src + 0x16);
			if (w == BITMAP_W && h == BITMAP_H) {
				const uint8_t *data = src + READ_LE_UINT32(src + 0xA);
				yflip(data, w, h, _tempBitmap);
				scaleBitmap(_tempBitmap, FMT_CLUT);
			}
		} else {
			int w, h;
			uint8_t *buf = decode_bitmap(src, &w, &h);
			if (buf) {
				_graphics->drawBitmap(_buffers[0], buf, w, h, FMT_RGB);
				free(buf);
			}
		}
	}
}

static void readPaletteEGA(const uint8_t *buf, int num, Color pal[16]) {
	const uint8_t *p = buf + num * 16 * sizeof(uint16_t);
	p += 1024; // EGA colors are stored after VGA (Amiga)
	for (int i = 0; i < 16; ++i) {
		const uint16_t color = READ_BE_UINT16(p); p += 2;
		if (1) {
			const uint8_t *ega = &Video::_paletteEGA[3 * ((color >> 12) & 15)];
			pal[i].r = ega[0];
			pal[i].g = ega[1];
			pal[i].b = ega[2];
		} else { // lower 12 bits hold other colors
			const uint8_t r = (color >> 8) & 0xF;
			const uint8_t g = (color >> 4) & 0xF;
			const uint8_t b =  color       & 0xF;
			pal[i].r = (r << 4) | r;
			pal[i].g = (g << 4) | g;
			pal[i].b = (b << 4) | b;
		}
	}
}

static void readPaletteAmiga(const uint8_t *buf, int num, Color pal[16]) {
	const uint8_t *p = buf + num * 16 * sizeof(uint16_t);
	for (int i = 0; i < 16; ++i) {
		const uint16_t color = READ_BE_UINT16(p); p += 2;
		const uint8_t r = (color >> 8) & 0xF;
		const uint8_t g = (color >> 4) & 0xF;
		const uint8_t b =  color       & 0xF;
		pal[i].r = (r << 4) | r;
		pal[i].g = (g << 4) | g;
		pal[i].b = (b << 4) | b;
	}
}

void Video::changePal(uint8_t palNum) {
	if (palNum < 32 && palNum != _currentPal) {
		Color pal[16];
		if (_res->getDataType() == Resource::DT_DOS && _useEGA) {
			readPaletteEGA(_res->_segVideoPal, palNum, pal);
		} else {
			readPaletteAmiga(_res->_segVideoPal, palNum, pal);
		}
		_graphics->setPalette(pal, 16);
		_currentPal = palNum;
	}
}

void Video::updateDisplay(uint8_t page, SystemStub *stub) {
	debug(DBG_VIDEO, "Video::updateDisplay(%d)", page);
	if (page != 0xFE) {
		if (page == 0xFF) {
			SWAP(_buffers[1], _buffers[2]);
		} else {
			_buffers[1] = getPagePtr(page);
		}
	}
	if (_nextPal != 0xFF) {
		changePal(_nextPal);
		_nextPal = 0xFF;
	}
	_graphics->drawBuffer(_buffers[1], stub);
}

void Video::captureDisplay() {
	_graphics->_screenshot = true;
}

void Video::setPaletteColor(uint8_t color, int r, int g, int b) {
	Color c;
	c.r = r;
	c.g = g;
	c.b = b;
	_graphics->setPalette(&c, 1);
}

