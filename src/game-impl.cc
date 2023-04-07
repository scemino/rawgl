#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gfx.h"
#include "game.h"
#include "graphics.h"
#include "mixer.h"
#include "resource.h"
#include "resource_nth.h"
#include "script.h"
#include "sfxplayer.h"
#include "systemstub.h"
#include "sokol_sys.h"
#include "util.h"
#include "video.h"

static const int _restartPos[36 * 2] = {
	16008,  0, 16001,  0, 16002, 10, 16002, 12, 16002, 14,
	16003, 20, 16003, 24, 16003, 26, 16004, 30, 16004, 31,
	16004, 32, 16004, 33, 16004, 34, 16004, 35, 16004, 36,
	16004, 37, 16004, 38, 16004, 39, 16004, 40, 16004, 41,
	16004, 42, 16004, 43, 16004, 44, 16004, 45, 16004, 46,
	16004, 47, 16004, 48, 16004, 49, 16006, 64, 16006, 65,
	16006, 66, 16006, 67, 16006, 68, 16005, 50, 16006, 60,
	16007, 0
};

enum {
    kStateLogo3DO,
    kStateTitle3DO,
    kStateEnd3DO,
    kStateGame
};

class GameState {
public:
    Resource _res;
    Video _vid;
    SfxPlayer _ply;
    Script _script;
    Graphics *_graphics;
    SokolSystem _sys;
    int _state;
    int _part_num;

public:
    GameState(const char *dataDir):
    _res(&_vid, dataDir),
    _vid(&_res),
    _ply(&_res),
    _script(NULL, &_res, &_ply, &_vid),
    _graphics(NULL) {
    }
};

GameState* _state;
bool Script::_useRemasteredAudio = true;
bool Graphics::_is1991 = false;
bool Graphics::_use555 = false;
bool Video::_useEGA = false;
Difficulty Script::_difficulty = DIFFICULTY_NORMAL;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GAME_ASSERT
    #include <assert.h>
    #define GAME_ASSERT(c) assert(c)
#endif

void game_init(game_t* game, const game_desc_t* desc) {
    GAME_ASSERT(game && desc);
    memset(game, 0, sizeof(game_t));
    game->valid = true;

    g_debugMask = DBG_INFO | DBG_VIDEO | DBG_SND | DBG_SCRIPT | DBG_BANK | DBG_SER;
    Language lang = (Language)desc->lang;
    _state = new GameState(desc->data_dir);
    _state->_part_num = desc->part_num;
    _state->_res.detectVersion();
    _state->_vid.init();
    _state->_res._lang = lang;
    _state->_res.allocMemBlock();
	_state->_res.readEntries();
	_state->_res.dumpEntries();

    const int scalerFactor = 1;
    const bool isNth = !Graphics::_is1991 && (_state->_res.getDataType() == Resource::DT_15TH_EDITION || _state->_res.getDataType() == Resource::DT_20TH_EDITION);
	int w = GFX_W * scalerFactor;
	int h = GFX_H * scalerFactor;
	if (isNth) {
		// get HD background bitmaps resolution
		_state->_res._nth->getBitmapSize(&w, &h);
	}
	debug(DBG_INFO, "Using sokol graphics");
    _state->_vid._graphics = _state->_graphics = GraphicsSokol_create(&_state->_sys);
    _state->_graphics->init(w, h);
	if (isNth) {
		_state->_res.loadFont();
		_state->_res.loadHeads();
	} else {
		_state->_vid.setDefaultFont();
	}

    if (desc->demo3_joy_inputs && _state->_res.getDataType() == Resource::DT_DOS) {
		_state->_res.readDemo3Joy();
	}

    _state->_sys.setBuffer(game->fb, game->palette);
    _state->_script._stub = &_state->_sys;
	_state->_script.init();
    MixerType mixerType = kMixerTypeRaw;
    switch (_state->_res.getDataType()) {
    case Resource::DT_DOS:
    case Resource::DT_AMIGA:
    case Resource::DT_ATARI:
    case Resource::DT_ATARI_DEMO:
        mixerType = kMixerTypeRaw;
        switch (lang) {
        case LANG_FR:
            _state->_vid._stringsTable = Video::_stringsTableFr;
            break;
        case LANG_US:
        default:
            _state->_vid._stringsTable = Video::_stringsTableEng;
            break;
        }
        break;
    case Resource::DT_WIN31:
    case Resource::DT_15TH_EDITION:
    case Resource::DT_20TH_EDITION:
        mixerType = kMixerTypeWav;
        break;
    case Resource::DT_3DO:
        mixerType = kMixerTypeAiff;
        break;
    }
    Video::_useEGA = desc->use_ega;
    // bypass protection ?
    if(!desc->bypass_protection) {
        switch (_state->_res.getDataType()) {
        case Resource::DT_DOS:
            if (!_state->_res._hasPasswordScreen) {
                break;
            }
            /* fall-through */
        case Resource::DT_AMIGA:
        case Resource::DT_ATARI:
        case Resource::DT_WIN31:
            _state->_part_num = kPartCopyProtection;
            break;
        default:
            break;
        }
    }
	if (_state->_res.getDataType() == Resource::DT_3DO && _state->_part_num == kPartIntro) {
		_state->_state = kStateLogo3DO;
	} else {
		_state->_state = kStateGame;
		const int num = _state->_part_num;
		if (num < 36) {
			_state->_script.restartAt(_restartPos[num * 2], _restartPos[num * 2 + 1]);
		} else {
			_state->_script.restartAt(num);
		}
	}
    game->title = _state->_res.getGameTitle(lang);
}

void game_exec(game_t* game) {
    GAME_ASSERT(game && game->valid);
    (void)game;
	switch (_state->_state) {
    // TODO: 3DO
	// case kStateLogo3DO:
	// 	doThreeScreens();
	// 	scrollText(0, 380, Video::_noteText3DO);
	// 	playCinepak("Logo.Cine");
	// 	playCinepak("Spintitle.Cine");
	// 	break;
	// case kStateTitle3DO:
	// 	titlePage();
	// 	break;
	// case kStateEnd3DO:
	// 	doEndCredits();
	// 	break;
	case kStateGame:
		_state->_script.setupTasks();
		_state->_script.updateInput();
		//processInput();
		_state->_script.runTasks();
		//_state->_mix.update();
		if (_state->_res.getDataType() == Resource::DT_3DO) {
			switch (_state->_res._nextPart) {
			case 16009:
				_state->_state = kStateEnd3DO;
				break;
			case 16000:
				_state->_state = kStateTitle3DO;
				break;
			}
		}
		break;
	}
}

void game_cleanup(game_t* game) {
    GAME_ASSERT(game && game->valid);
    (void)game;
    delete _state;
}

gfx_display_info_t game_display_info(game_t* game) {
    GAME_ASSERT(game && game->valid);
    const gfx_display_info_t res = {
        .frame = {
            .dim = {
                .width = 320,
                .height = 200,
            },
            .bytes_per_pixel = 1,
            .buffer = {
                .ptr = game->fb,
                .size = 320*200,
            }
        },
        .screen = {
            .x = 0,
            .y = 0,
            .width = 320,
            .height = 200,
        },
        .palette = {
            .ptr = game->palette,
            .size = 1024,
        }
    };
    return res;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

