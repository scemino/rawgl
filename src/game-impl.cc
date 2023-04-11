#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gfx.h"
#include "common.h"
#include "game.h"
#include "graphics.h"
#include "graphics_sokol.h"
#include "mixer.h"
#include "resource.h"
#include "resource_nth.h"
#include "script.h"
#include "sfxplayer.h"
#include "systemstub.h"
#include "sokol_audio.h"
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
    Mixer _mix;
    Script _script;
    GraphicsSokol _graphics;
    SokolSystem _sys;
    int _state;
    int _part_num;

public:
    GameState(const char *dataDir):
    _res(&_vid, dataDir),
    _vid(&_res),
    _ply(&_res),
    _mix(&_ply),
    _script(&_mix, &_res, &_ply, &_vid) {
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

#define _GAME_DEFAULT(val,def) (((val) != 0) ? (val) : (def))

void game_init(game_t* game, const game_desc_t* desc) {
    GAME_ASSERT(game && desc);
    if (desc->debug.callback.func) { GAME_ASSERT(desc->debug.stopped); }
    memset(game, 0, sizeof(game_t));
    game->valid = true;
    game->debug = desc->debug;

    g_debugMask = DBG_INFO | DBG_VIDEO | DBG_SND | DBG_SCRIPT | DBG_BANK | DBG_SER;
    Language lang = (Language)desc->lang;
    _state = new GameState(desc->data_dir);
    _state->_part_num = desc->part_num;
    _state->_res.detectVersion();
    _state->_vid.init();
    _state->_res._lang = lang;
    _state->_res.allocMemBlock();
	_state->_res.readEntries();

	debug(DBG_INFO, "Using sokol graphics");
    _state->_vid._graphics = &_state->_graphics;
    _state->_graphics.setSystem(&_state->_sys);
    gfx_fb fbs = {
        .buffer = {
            game->fbs[0].buffer,
            game->fbs[1].buffer,
            game->fbs[2].buffer,
            game->fbs[3].buffer
        }
    };
    _state->_graphics.setBuffers(&fbs);
    _state->_graphics.init(GFX_W, GFX_H);
	_state->_vid.setDefaultFont();

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
    _state->_mix.callback = desc->audio.callback;
    _state->_mix.init(mixerType);

    Video::_useEGA = desc->use_ega;
    // bypass protection ?
#ifndef BYPASS_PROTECTION
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
#endif
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

void _game_exec(game_t* game, uint32_t ms) {
    _state->_sys._elapsed += ms;
    if(_state->_sys._sleep) {
        if(ms > _state->_sys._sleep) {
            _state->_sys._sleep = 0;
        } else {
            _state->_sys._sleep -= ms;
        }
        return;
    }

    _state->_script.setupTasks();
    _state->_script.updateInput();
    _state->_script.runTasks();
    const int num_frames = saudio_expect();
    if (num_frames > 0) {
        const int num_samples = num_frames * saudio_channels();
        _state->_mix.update(num_samples);
    }
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

    _state->_sys._sleep += 16;
}

void game_exec(game_t* game, uint32_t ms) {
    GAME_ASSERT(game && game->valid);
    (void)game;

    if (0 == game->debug.callback.func) {
        // run without debug hook
        _game_exec(game, ms);
    } else {
        // run with debug hook
        if(!(*game->debug.stopped)) {
            _game_exec(game, ms);
            game->debug.callback.func(game->debug.callback.user_data, _state->_script._scriptPtr.pc - _state->_res._segCode);
        }
    }
}

void game_cleanup(game_t* game) {
    GAME_ASSERT(game && game->valid);
    (void)game;
    _state->_graphics.fini();
	_state->_ply.stop();
	_state->_mix.quit();
	_state->_res.freeMemBlock();
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

void game_key_down(game_t* game, game_input_t input) {
    (void)game;
    switch(input) {
        case GAME_INPUT_LEFT:   _state->_sys._pi.dirMask |= PlayerInput::DIR_LEFT; break;
        case GAME_INPUT_RIGHT:  _state->_sys._pi.dirMask |= PlayerInput::DIR_RIGHT; break;
        case GAME_INPUT_UP:     _state->_sys._pi.dirMask |= PlayerInput::DIR_UP; break;
        case GAME_INPUT_DOWN:   _state->_sys._pi.dirMask |= PlayerInput::DIR_DOWN; break;
        case GAME_INPUT_JUMP:   _state->_sys._pi.jump = true; break;
        case GAME_INPUT_ACTION: _state->_sys._pi.action = true; break;
        case GAME_INPUT_BACK:   _state->_sys._pi.back = true; break;
        case GAME_INPUT_CODE:   _state->_sys._pi.code = true; break;
        case GAME_INPUT_PAUSE:  _state->_sys._pi.pause = true; break;
    }
}

void game_key_up(game_t* game, game_input_t input) {
    (void)game;
    switch(input) {
        case GAME_INPUT_LEFT:   _state->_sys._pi.dirMask &= ~PlayerInput::DIR_LEFT; break;
        case GAME_INPUT_RIGHT:  _state->_sys._pi.dirMask &= ~PlayerInput::DIR_RIGHT; break;
        case GAME_INPUT_UP:     _state->_sys._pi.dirMask &= ~PlayerInput::DIR_UP; break;
        case GAME_INPUT_DOWN:   _state->_sys._pi.dirMask &= ~PlayerInput::DIR_DOWN; break;
        case GAME_INPUT_JUMP:   _state->_sys._pi.jump = false; break;
        case GAME_INPUT_ACTION: _state->_sys._pi.action = false; break;
        case GAME_INPUT_BACK:   _state->_sys._pi.back = false; break;
        case GAME_INPUT_CODE:   _state->_sys._pi.code = false; break;
        case GAME_INPUT_PAUSE:  _state->_sys._pi.pause = false; break;
    }
}

void game_char_pressed(game_t* game, int c) {
    _state->_sys._pi.lastChar = (char)c;
}

void game_get_resources(game_t* game, game_mem_entry_t** res) {
    //memcpy(res->mem_entries, _state->_res._memList, sizeof(_state->_res._memList));
    *res = (game_mem_entry_t*)&_state->_res._memList[0];
}

void game_get_vars(game_t* game, int16_t** vars) {
    *vars = &_state->_script._scriptVars[0];
}

uint8_t* game_get_pc(game_t* game) {
    return _state->_res._segCode;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

