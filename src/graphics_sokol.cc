
#include <math.h>
#include "graphics_sokol.h"
#include "graphics.h"
#include "util.h"
#include "systemstub.h"
#include "sokol_sys.h"

GraphicsSokol::GraphicsSokol() {
	_fixUpPalette = FIXUP_PALETTE_NONE;
	memset(_pagePtrs, 0, sizeof(_pagePtrs));
	memset(_pal, 0, sizeof(_pal));
    memset(_palette, 0, sizeof(_palette));

    _u = (GFX_W << 16) / GFX_W;
	_v = (GFX_H << 16) / GFX_H;
}

void GraphicsSokol::init() {
    Graphics::init();
    for (int i = 0; i < 4; ++i) {
        memset(_pagePtrs[i], 0, getPageSize());
    }
    setWorkPagePtr(2);
}

static uint32_t calcStep(const Point &p1, const Point &p2, uint16_t &dy) {
	dy = p2.y - p1.y;
	uint16_t delta = (dy <= 1) ? 1 : dy;
	return ((p2.x - p1.x) * (0x4000 / delta)) << 2;
}

void GraphicsSokol::drawPolygon(uint8_t color, const QuadStrip &quadStrip) {
	QuadStrip qs = quadStrip;

	int i = 0;
	int j = qs.numVertices - 1;

	int16_t x2 = qs.vertices[i].x;
	int16_t x1 = qs.vertices[j].x;
	int16_t hliney = MIN(qs.vertices[i].y, qs.vertices[j].y);

	++i;
	--j;

	drawLine pdl;
	switch (color) {
	default:
		pdl = &GraphicsSokol::drawLineN;
		break;
	case COL_PAGE:
		pdl = &GraphicsSokol::drawLineP;
		break;
	case COL_ALPHA:
		pdl = &GraphicsSokol::drawLineT;
		break;
	}

	uint32_t cpt1 = x1 << 16;
	uint32_t cpt2 = x2 << 16;

	int numVertices = qs.numVertices;
	while (1) {
		numVertices -= 2;
		if (numVertices == 0) {
			return;
		}
		uint16_t h;
		uint32_t step1 = calcStep(qs.vertices[j + 1], qs.vertices[j], h);
		uint32_t step2 = calcStep(qs.vertices[i - 1], qs.vertices[i], h);

		++i;
		--j;

		cpt1 = (cpt1 & 0xFFFF0000) | 0x7FFF;
		cpt2 = (cpt2 & 0xFFFF0000) | 0x8000;

		if (h == 0) {
			cpt1 += step1;
			cpt2 += step2;
		} else {
			while (h--) {
				if (hliney >= 0) {
					x1 = cpt1 >> 16;
					x2 = cpt2 >> 16;
					if (x1 < GFX_W && x2 >= 0) {
						if (x1 < 0) x1 = 0;
						if (x2 >= GFX_W) x2 = GFX_W - 1;
						(this->*pdl)(x1, x2, hliney, color);
					}
				}
				cpt1 += step1;
				cpt2 += step2;
				++hliney;
				if (hliney >= GFX_H) return;
			}
		}
	}
}

void GraphicsSokol::drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color) {
	if (x <= GFX_W - 8 && y <= GFX_H - 8) {
		x = xScale(x);
		y = yScale(y);
		const uint8_t *ft = _font + (c - 0x20) * 8;
		const int offset = (x + y * GFX_W);
        for (int j = 0; j < 8; ++j) {
            const uint8_t ch = ft[j];
            for (int i = 0; i < 8; ++i) {
                if (ch & (1 << (7 - i))) {
                    _drawPagePtr[offset + j * GFX_W + i] = color;
                }
            }
        }
	}
}
void GraphicsSokol::drawSpriteMask(int x, int y, uint8_t color, const uint8_t *data) {
	const int w = *data++;
	x = xScale(x - w / 2);
	const int h = *data++;
	y = yScale(y - h / 2);
	for (int j = 0; j < h; ++j) {
		const int yoffset = y + j;
		for (int i = 0; i <= w / 16; ++i) {
			const uint16_t mask = READ_BE_UINT16(data); data += 2;
			if (yoffset < 0 || yoffset >= GFX_H) {
				continue;
			}
			const int xoffset = x + i * 16;
			for (int b = 0; b < 16; ++b) {
				if (xoffset + b < 0 || xoffset + b >= GFX_W) {
					continue;
				}
				if (mask & (1 << (15 - b))) {
					_drawPagePtr[yoffset * GFX_W + xoffset + b] = color;
				}
			}
		}
	}
}

void GraphicsSokol::drawPoint(int16_t x, int16_t y, uint8_t color) {
	x = xScale(x);
	y = yScale(y);
	const int offset = (y * GFX_W + x);
    switch (color) {
    case COL_ALPHA:
        _drawPagePtr[offset] |= 8;
        break;
    case COL_PAGE:
        _drawPagePtr[offset] = *(_pagePtrs[0] + offset);
        break;
    default:
        _drawPagePtr[offset] = color;
        break;
    }
}

void GraphicsSokol::drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	int w = xmax - xmin + 1;
	const int offset = (y * GFX_W + xmin);
	for (int i = 0; i < w; ++i) {
        _drawPagePtr[offset + i] |= 8;
    }
}

void GraphicsSokol::drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int offset = (y * GFX_W + xmin);
	memset(_drawPagePtr + offset, color, w);
}

void GraphicsSokol::drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	if (_drawPagePtr == _pagePtrs[0]) {
		return;
	}
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int offset = (y * GFX_W + xmin);
	memcpy(_drawPagePtr + offset, _pagePtrs[0] + offset, w);
}

uint8_t *GraphicsSokol::getPagePtr(uint8_t page) {
	assert(page >= 0 && page < 4);
	return _pagePtrs[page];
}

void GraphicsSokol::setWorkPagePtr(uint8_t page) {
	_drawPagePtr = getPagePtr(page);
}

void GraphicsSokol::setFont(const uint8_t *src, int w, int h) {
	if (_is1991) {
		// no-op for 1991
	}
}

void GraphicsSokol::setPalette(const Color *colors, int count) {
    assert(count <= 16);
	memcpy(_pal, colors, sizeof(Color) * count);
    for(int i=0; i<count; i++) {
        _palette[i] = 0xFF000000 | (((uint32_t)colors[i].r)) | (((uint32_t)colors[i].g) << 8) | (((uint32_t)colors[i].b) << 16);
    }
    _stub->setPalette(_palette);
}

void GraphicsSokol::setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize) {
	if (_is1991) {
		// no-op for 1991
	}
}

void GraphicsSokol::drawSprite(int buffer, int num, const Point *pt, uint8_t color) {
	if (_is1991) {
		if (num < _shapesMaskCount) {
			setWorkPagePtr(buffer);
			const uint8_t *data = _shapesMaskData + _shapesMaskOffset[num];
			drawSpriteMask(pt->x, pt->y, color, data);
		}
	}
}

void GraphicsSokol::drawBitmap(int buffer, const uint8_t *data, int w, int h, int fmt) {
    if (fmt == FMT_CLUT && GFX_W == w && GFX_H == h) {
        memcpy(getPagePtr(buffer), data, w * h);
        return;
    }
	warning("GraphicsSokol::drawBitmap() unhandled fmt %d w %d h %d", fmt, w, h);
}

void GraphicsSokol::drawPoint(int buffer, uint8_t color, const Point *pt) {
	setWorkPagePtr(buffer);
	drawPoint(pt->x, pt->y, color);
}

void GraphicsSokol::drawQuadStrip(int buffer, uint8_t color, const QuadStrip *qs) {
	setWorkPagePtr(buffer);
	drawPolygon(color, *qs);
}

void GraphicsSokol::drawStringChar(int buffer, uint8_t color, char c, const Point *pt) {
	setWorkPagePtr(buffer);
	drawChar(c, pt->x, pt->y, color);
}

void GraphicsSokol::clearBuffer(int num, uint8_t color) {
	memset(getPagePtr(num), color, getPageSize());
}

void GraphicsSokol::copyBuffer(int dst, int src, int vscroll) {
	if (vscroll == 0) {
		memcpy(getPagePtr(dst), getPagePtr(src), getPageSize());
	} else if (vscroll >= -199 && vscroll <= 199) {
		const int dy = yScale(vscroll);
		if (dy < 0) {
			memcpy(getPagePtr(dst), getPagePtr(src) - dy * GFX_W, (GFX_H + dy) * GFX_W);
		} else {
			memcpy(getPagePtr(dst) + dy * GFX_W, getPagePtr(src), (GFX_H - dy) * GFX_W);
		}
	}
}

static void dumpPalette(uint8_t *dst, int w, const Color *pal) {
	static const int SZ = 16;
	for (int color = 0; color < 16; ++color) {
		uint8_t *p = dst + (color & 7) * SZ;
		for (int y = 0; y < SZ; ++y) {
			for (int x = 0; x < SZ; ++x) {
				p[x] = color;
			}
			p += w;
		}
		if (color == 7) {
			dst += SZ * w;
		}
	}
}

void GraphicsSokol::drawBuffer(int num, SystemStub *stub) {
	int w, h;
	float ar[4];
	stub->prepareScreen(w, h, ar);

    const uint8_t *src = getPagePtr(num);
    memcpy(_colorBuffer, src, GFX_W * GFX_H);
    if (0) {
        dumpPalette(_colorBuffer, GFX_W, _pal);
    }
    _stub->setScreenPixels(_colorBuffer, GFX_W, GFX_H);

	stub->updateScreen();
}

void GraphicsSokol::drawBitmapOverlay(const uint8_t *data, int w, int h, int fmt, SystemStub *stub) {
	if (fmt == FMT_RGB555) {
		stub->setScreenPixels555((const uint16_t *)data, w, h);
		stub->updateScreen();
	}
}

void GraphicsSokol::setBuffers(gfx_fb* fb) {
    for(int i=0; i<4; i++) {
        _pagePtrs[i] = fb->buffer[i];
    }
}
