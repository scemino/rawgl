
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "resource.h"
#include "file.h"
#include "unpack.h"
#include "util.h"
#include "video.h"
#include "raw-data.h"

static const char *atariDemo = "aw.tos";

Resource::Resource(Video *vid, const char *dataDir)
	: _vid(vid), _dataDir(dataDir), _currentPart(0), _nextPart(0), _dataType(DT_DOS) {
	_bankPrefix = "bank";
	_hasPasswordScreen = true;
	memset(_memList, 0, sizeof(_memList));
	_numMemList = 0;
	if (!_dataDir) {
		_dataDir = ".";
	}
	_lang = LANG_FR;
	_amigaMemList = 0;
	memset(&_demo3Joy, 0, sizeof(_demo3Joy));
    _memPtrStart = (uint8_t *)_mem;
	_scriptBakPtr = _scriptCurPtr = _memPtrStart;
	_vidCurPtr = _memPtrStart + MEM_BLOCK_SIZE - (320 * 200 / 2); // 4bpp bitmap
	_useSegVideo2 = false;
}

Resource::~Resource() {
	free(_demo3Joy.bufPtr);
}

bool Resource::readBank(const MemEntry *me, uint8_t *dstBuf) {
	bool ret = false;
	static uint8_t* banks[] = {
        dump_BANK01,
        dump_BANK02,
        NULL,
        NULL,
        dump_BANK05,
        dump_BANK06,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        dump_BANK0D
    };

	File f;
    // TODO: (_dataType == DT_ATARI_DEMO && f.open(atariDemo, _dataDir))
	{
        f.setBuffer(banks[me->bankNum-1], sizeof(banks[me->bankNum-1]));
		f.seek(me->bankPos);
		const size_t count = f.read(dstBuf, me->packedSize);
		ret = (count == me->packedSize);
		if (ret && me->packedSize != me->unpackedSize) {
			ret = bytekiller_unpack(dstBuf, me->unpackedSize, dstBuf, me->packedSize);
		}
	}
	return ret;
}

//static const AmigaMemEntry *detectAmigaAtari(File &f, const char *dataDir) {
//	static const struct {
//		uint32_t bank01Size;
//		const AmigaMemEntry *entries;
//	} _files[] = {
//		{ 244674, Resource::_memListAmigaFR },
//		{ 244868, Resource::_memListAmigaEN },
//		{ 227142, Resource::_memListAtariEN },
//		{ 0, 0 }
//	};
//	if (f.open("bank01", dataDir)) {
//		const uint32_t size = f.size();
//		for (int i = 0; _files[i].entries; ++i) {
//			if (_files[i].bank01Size == size) {
//				return _files[i].entries;
//			}
//		}
//	}
//	return 0;
//}

void Resource::detectVersion() {
	File f;
	// if (f.open("memlist.bin", _dataDir)) {
	_dataType = DT_DOS;
	debug(DBG_INFO, "Using DOS data files");
	// } else if ((_amigaMemList = detectAmigaAtari(f, _dataDir)) != 0) {
	// 	if (_amigaMemList == _memListAtariEN) {
	// 		_dataType = DT_ATARI;
	// 		debug(DBG_INFO, "Using Atari data files");
	// 	} else {
	// 		_dataType = DT_AMIGA;
	// 		debug(DBG_INFO, "Using Amiga data files");
	// 	}
	// } else if (f.open(atariDemo, _dataDir) && f.size() == 96513) {
	// 	_dataType = DT_ATARI_DEMO;
	// 	debug(DBG_INFO, "Using Atari demo file");
	// } else {
	// 	error("No data files found in '%s'", _dataDir);
	// }
}

static const char *kGameTitleEU = "Another World";
static const char *kGameTitleUS = "Out Of This World";

const char *Resource::getGameTitle(Language lang) const {
	switch (_dataType) {
	case DT_DOS:
		if (lang == LANG_US) {
			return kGameTitleUS;
		}
		/* fall-through */
	default:
		break;
	}
	return kGameTitleEU;
}

void Resource::readEntries() {
	switch (_dataType) {
	case DT_AMIGA:
	case DT_ATARI:
		assert(_amigaMemList);
		readEntriesAmiga(_amigaMemList, ENTRIES_COUNT);
		return;
	case DT_DOS: {
			_hasPasswordScreen = false; // DOS demo versions do not have the resources
			File f;
			// if (f.open("demo01", _dataDir)) {
			// 	_bankPrefix = "demo";
			// }
            {
				MemEntry *me = _memList;
                f.setBuffer(dump_MEMLIST_BIN, sizeof(dump_MEMLIST_BIN));
				while (1) {
					assert(_numMemList < ARRAYSIZE(_memList));
					me->status = f.readByte();
					me->type = f.readByte();
					me->bufPtr = 0; f.readUint32BE();
					me->rankNum = f.readByte();
					me->bankNum = f.readByte();
					me->bankPos = f.readUint32BE();
					me->packedSize = f.readUint32BE();
					me->unpackedSize = f.readUint32BE();
					if (me->status == 0xFF) {
						const int num = _memListParts[8][1]; // 16008 bytecode
						assert(num < _numMemList);
						char bank[16];
						snprintf(bank, sizeof(bank), "%s%02x", _bankPrefix, _memList[num].bankNum);
                        _hasPasswordScreen = false; // dump_BANK09 != NULL;
						return;
					}
					++_numMemList;
					++me;
				}
			}
		}
		break;
	case DT_ATARI_DEMO: {
			File f;
			if (f.open(atariDemo, _dataDir)) {
				static const struct {
					uint8_t type;
					uint8_t num;
					uint32_t offset;
					uint16_t size;
				} data[] = {
					{ RT_SHAPE, 0x19, 0x50f0, 65146 },
					{ RT_PALETTE, 0x17, 0x14f6a, 2048 },
					{ RT_BYTECODE, 0x18, 0x1576a, 8368 }
				};
				_numMemList = ENTRIES_COUNT;
				for (int i = 0; i < 3; ++i) {
					MemEntry *entry = &_memList[data[i].num];
					entry->type = data[i].type;
					entry->bankNum = 15;
					entry->bankPos = data[i].offset;
					entry->packedSize = entry->unpackedSize = data[i].size;
				}
				return;
			}
		}
		break;
	}
	error("No data files found in '%s'", _dataDir);
}

void Resource::readEntriesAmiga(const AmigaMemEntry *entries, int count) {
	_numMemList = count;
	for (int i = 0; i < count; ++i) {
		_memList[i].type = entries[i].type;
		_memList[i].bankNum = entries[i].bank;
		_memList[i].bankPos = entries[i].offset;
		_memList[i].packedSize = entries[i].packedSize;
		_memList[i].unpackedSize = entries[i].unpackedSize;
	}
	_memList[count].status = 0xFF;
}

void Resource::load() {
	while (1) {
		MemEntry *me = 0;

		// get resource with max rankNum
		uint8_t maxNum = 0;
		for (int i = 0; i < _numMemList; ++i) {
			MemEntry *it = &_memList[i];
			if (it->status == STATUS_TOLOAD && maxNum <= it->rankNum) {
				maxNum = it->rankNum;
				me = it;
			}
		}
		if (me == 0) {
			break; // no entry found
		}

		const int resourceNum = me - _memList;

		uint8_t *memPtr = 0;
		if (me->type == RT_BITMAP) {
			memPtr = _vidCurPtr;
		} else {
			memPtr = _scriptCurPtr;
			const uint32_t avail = uint32_t(_vidCurPtr - _scriptCurPtr);
			if (me->unpackedSize > avail) {
				warning("Resource::load() not enough memory, available=%d", avail);
				me->status = STATUS_NULL;
				continue;
			}
		}
		if (me->bankNum == 0) {
			warning("Resource::load() ec=0x%X (me->bankNum == 0)", 0xF00);
			me->status = STATUS_NULL;
		} else {
			debug(DBG_BANK, "Resource::load() bufPos=0x%X size=%d type=%d pos=0x%X bankNum=%d", memPtr - _memPtrStart, me->packedSize, me->type, me->bankPos, me->bankNum);
			if (readBank(me, memPtr)) {
				if (me->type == RT_BITMAP) {
					_vid->copyBitmapPtr(_vidCurPtr, me->unpackedSize);
					me->status = STATUS_NULL;
				} else {
					me->bufPtr = memPtr;
					me->status = STATUS_LOADED;
					_scriptCurPtr += me->unpackedSize;
				}
			} else {
				if (_dataType == DT_DOS && me->bankNum == 12 && me->type == RT_BANK) {
					// DOS demo version does not have the bank for this resource
					// this should be safe to ignore as the resource does not appear to be used by the game code
					me->status = STATUS_NULL;
					continue;
				}
				error("Unable to read resource %d from bank %d", resourceNum, me->bankNum);
			}
		}
	}
}

void Resource::invalidateRes() {
	for (int i = 0; i < _numMemList; ++i) {
		MemEntry *me = &_memList[i];
		if (me->type <= 2 || me->type > 6) {
			me->status = STATUS_NULL;
		}
	}
	_scriptCurPtr = _scriptBakPtr;
	_vid->_currentPal = 0xFF;
}

void Resource::invalidateAll() {
	for (int i = 0; i < _numMemList; ++i) {
		_memList[i].status = STATUS_NULL;
	}
	_scriptCurPtr = _memPtrStart;
	_vid->_currentPal = 0xFF;
}

void Resource::update(uint16_t num) {
	if (num > 16000) {
		_nextPart = num;
		return;
	}

    MemEntry *me = &_memList[num];
    if (me->status == STATUS_NULL) {
        me->status = STATUS_TOLOAD;
        load();
    }
}

const uint8_t Resource::_memListParts[][4] = {
	{ 0x14, 0x15, 0x16, 0x00 }, // 16000 - protection screens
	{ 0x17, 0x18, 0x19, 0x00 }, // 16001 - introduction
	{ 0x1A, 0x1B, 0x1C, 0x11 }, // 16002 - water
	{ 0x1D, 0x1E, 0x1F, 0x11 }, // 16003 - jail
	{ 0x20, 0x21, 0x22, 0x11 }, // 16004 - 'cite'
	{ 0x23, 0x24, 0x25, 0x00 }, // 16005 - 'arene'
	{ 0x26, 0x27, 0x28, 0x11 }, // 16006 - 'luxe'
	{ 0x29, 0x2A, 0x2B, 0x11 }, // 16007 - 'final'
	{ 0x7D, 0x7E, 0x7F, 0x00 }, // 16008 - password screen
	{ 0x7D, 0x7E, 0x7F, 0x00 }  // 16009 - password screen
};

void Resource::setupPart(int ptrId) {
	if (ptrId != _currentPart) {
        uint8_t ipal = 0;
        uint8_t icod = 0;
        uint8_t ivd1 = 0;
        uint8_t ivd2 = 0;
        if (ptrId >= 16000 && ptrId <= 16009) {
            uint16_t part = ptrId - 16000;
            ipal = _memListParts[part][0];
            icod = _memListParts[part][1];
            ivd1 = _memListParts[part][2];
            ivd2 = _memListParts[part][3];
        } else {
            error("Resource::setupPart() ec=0x%X invalid part", 0xF07);
        }
        invalidateAll();
        _memList[ipal].status = STATUS_TOLOAD;
        _memList[icod].status = STATUS_TOLOAD;
        _memList[ivd1].status = STATUS_TOLOAD;
        if (ivd2 != 0) {
            _memList[ivd2].status = STATUS_TOLOAD;
        }
        load();
        _segVideoPal = _memList[ipal].bufPtr;
        _segCode = _memList[icod].bufPtr;
        _segVideo1 = _memList[ivd1].bufPtr;
        if (ivd2 != 0) {
            _segVideo2 = _memList[ivd2].bufPtr;
        }
        _currentPart = ptrId;
    }
    _scriptBakPtr = _scriptCurPtr;
}

void Resource::readDemo3Joy() {
	static const char *filename = "demo3.joy";
	File f;
	if (dump_DEMO3_JOY) {
        f.setBuffer(dump_DEMO3_JOY, sizeof(dump_DEMO3_JOY));
		const uint32_t fileSize = f.size();
		_demo3Joy.bufPtr = (uint8_t *)malloc(fileSize);
		if (_demo3Joy.bufPtr) {
			_demo3Joy.bufSize = f.read(_demo3Joy.bufPtr, fileSize);
			_demo3Joy.bufPos = -1;
		}
	} else {
		warning("Unable to open '%s'", filename);
	}
}
