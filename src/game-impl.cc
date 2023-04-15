#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gfx.h"
#include "common.h"
#include "game.h"
#include "graphics.h"
#include "graphics_sokol.h"
#include "resource.h"
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

static const uint16_t _periodTable[] = {
	1076, 1016,  960,  906,  856,  808,  762,  720,  678,  640,
	 604,  570,  538,  508,  480,  453,  428,  404,  381,  360,
	 339,  320,  302,  285,  269,  254,  240,  226,  214,  202,
	 190,  180,  170,  160,  151,  143,  135,  127,  120,  113
};

typedef void (*_opcode_func)(game_t* game);

class GameState {
public:
    Resource _res;
    Video _vid;
    GraphicsSokol _graphics;
    SokolSystem _sys;
    int _part_num;

public:
    GameState(const char *dataDir):
    _res(&_vid, dataDir),
    _vid(&_res) {
    }
};

GameState* _state;
bool Graphics::_is1991 = false;
bool Video::_useEGA = false;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GAME_ASSERT
    #include <assert.h>
    #define GAME_ASSERT(c) assert(c)
#endif

#define _GAME_DEFAULT(val,def) (((val) != 0) ? (val) : (def))

uint8_t fetchByte(game_pc_t* ptr) {
    return *ptr->pc++;
}

uint16_t fetchWord(game_pc_t* ptr) {
    const uint16_t i = READ_BE_UINT16(ptr->pc);
    ptr->pc += 2;
    return i;
}

// Frac

void frac_reset(game_frac_t* frac, int n, int d) {
    frac->inc = (((int64_t)n) << FRAC_BITS) / d;
    frac->offset = 0;
}

uint32_t frac_get_int(game_frac_t* frac) {
    return frac->offset >> FRAC_BITS;
}
uint32_t frac_get_frac(game_frac_t* frac) {
    return frac->offset & FRAC_MASK;
}
int frac_interpolate(game_frac_t* frac, int sample1, int sample2) {
    const int fp = frac_get_frac(frac);
    return (sample1 * (FRAC_MASK - fp) + sample2 * fp) >> FRAC_BITS;
}

// SFX Player

static void _game_audio_sfx_set_events_delay(game_t* game, uint16_t delay) {
	debug(DBG_SND, "SfxPlayer::setEventsDelay(%d)", delay);
	game->audio.sfx_player.delay = delay;
}

static void _game_audio_sfx_prepare_instruments(game_t* game, const uint8_t* p) {
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
    memset(player->sfx_mod.samples, 0, sizeof(player->sfx_mod.samples));
	for (int i = 0; i < 15; ++i) {
		game_audio_sfx_instrument_t *ins = &player->sfx_mod.samples[i];
		uint16_t resNum = READ_BE_UINT16(p); p += 2;
		if (resNum != 0) {
			ins->volume = READ_BE_UINT16(p);
			MemEntry *me = &_state->_res._memList[resNum];
			if (me->status == Resource::STATUS_LOADED && me->type == Resource::RT_SOUND) {
				ins->data = me->bufPtr;
				debug(DBG_SND, "Loaded instrument 0x%X n=%d volume=%d", resNum, i, ins->volume);
			} else {
				error("Error loading instrument 0x%X", resNum);
			}
		}
		p += 2; // skip volume
	}
}

static void _game_audio_sfx_load_module(game_t* game, uint16_t resNum, uint16_t delay, uint8_t pos) {
	debug(DBG_SND, "SfxPlayer::loadSfxModule(0x%X, %d, %d)", resNum, delay, pos);
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
	MemEntry *me = &_state->_res._memList[resNum];
	if (me->status == Resource::STATUS_LOADED && me->type == Resource::RT_MUSIC) {
		memset(&player->sfx_mod, 0, sizeof(game_audio_sfx_module_t));
		player->sfx_mod.cur_order = pos;
		player->sfx_mod.num_order = me->bufPtr[0x3F];
		debug(DBG_SND, "SfxPlayer::loadSfxModule() curOrder = 0x%X numOrder = 0x%X", player->sfx_mod.cur_order, player->sfx_mod.num_order);
		player->sfx_mod.order_table = me->bufPtr + 0x40;
		if (delay == 0) {
			player->delay = READ_BE_UINT16(me->bufPtr);
		} else {
			player->delay = delay;
		}
		player->sfx_mod.data = me->bufPtr + 0xC0;
		debug(DBG_SND, "SfxPlayer::loadSfxModule() eventDelay = %d ms", player->delay);
		_game_audio_sfx_prepare_instruments(game, me->bufPtr + 2);
	} else {
		warning("SfxPlayer::loadSfxModule() ec=0x%X", 0xF8);
	}
}

static void _game_audio_sfx_play(game_t* game, int rate) {
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
	player->playing = true;
	player->rate = rate;
	player->samples_left = 0;
	memset(player->channels, 0, sizeof(player->channels));
}

static int16_t _to_i16(int a) {
	return ((a << 8) | a) - 32768;
}

static void _game_audio_sfx_mix_channel(int16_t* s, game_audio_sfx_channel_t* ch) {
	if (ch->sample_len == 0) {
		return;
	}
	int pos1 = ch->pos.offset >> Frac::BITS;
	ch->pos.offset += ch->pos.inc;
	int pos2 = pos1 + 1;
	if (ch->sample_loop_len != 0) {
		if (pos1 >= ch->sample_loop_pos + ch->sample_loop_len - 1) {
			pos2 = ch->sample_loop_pos;
			ch->pos.offset = pos2 << Frac::BITS;
		}
	} else {
		if (pos1 >= ch->sample_len - 1) {
			ch->sample_len = 0;
			return;
		}
	}
    int sample = frac_interpolate(&ch->pos, (int8_t)ch->sample_data[pos1], (int8_t)ch->sample_data[pos2]);
	sample = *s + _to_i16(sample * ch->volume / 64);
	*s = (sample < -32768 ? -32768 : (sample > 32767 ? 32767 : sample));
}

static void _game_audio_sfx_start(game_t* game) {
	debug(DBG_SND, "SfxPlayer::start()");
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
	player->sfx_mod.cur_pos = 0;
}

static void _game_audio_sfx_stop(game_t* game) {
	debug(DBG_SND, "SfxPlayer::stop()");
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
	player->playing = false;
}

static void _game_audio_sfx_handle_pattern(game_t* game, uint8_t channel, const uint8_t *data) {
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
	game_audio_sfx_pattern_t pat;
	memset(&pat, 0, sizeof(game_audio_sfx_pattern_t));
	pat.note_1 = READ_BE_UINT16(data + 0);
	pat.note_2 = READ_BE_UINT16(data + 2);
	if (pat.note_1 != 0xFFFD) {
		uint16_t sample = (pat.note_2 & 0xF000) >> 12;
		if (sample != 0) {
			uint8_t *ptr = player->sfx_mod.samples[sample - 1].data;
			if (ptr != 0) {
				debug(DBG_SND, "SfxPlayer::handlePattern() preparing sample %d", sample);
				pat.sample_volume = player->sfx_mod.samples[sample - 1].volume;
				pat.sample_start = 8;
				pat.sample_buffer = ptr;
				pat.sample_len = READ_BE_UINT16(ptr) * 2;
				uint16_t loopLen = READ_BE_UINT16(ptr + 2) * 2;
				if (loopLen != 0) {
					pat.loop_pos = pat.sample_len;
					pat.loop_len = loopLen;
				} else {
					pat.loop_pos = 0;
					pat.loop_len = 0;
				}
				int16_t m = pat.sample_volume;
				uint8_t effect = (pat.note_2 & 0x0F00) >> 8;
				if (effect == 5) { // volume up
					uint8_t volume = (pat.note_2 & 0xFF);
					m += volume;
					if (m > 0x3F) {
						m = 0x3F;
					}
				} else if (effect == 6) { // volume down
					uint8_t volume = (pat.note_2 & 0xFF);
					m -= volume;
					if (m < 0) {
						m = 0;
					}
				}
				player->channels[channel].volume = m;
				pat.sample_volume = m;
			}
		}
	}
	if (pat.note_1 == 0xFFFD) {
		debug(DBG_SND, "SfxPlayer::handlePattern() _syncVar = 0x%X", pat.note_2);
		game->vm.vars[VAR_MUSIC_SYNC] = pat.note_2;
	} else if (pat.note_1 == 0xFFFE) {
		player->channels[channel].sample_len = 0;
	} else if (pat.note_1 != 0 && pat.sample_buffer != 0) {
		assert(pat.note_1 >= 0x37 && pat.note_1 < 0x1000);
		// convert Amiga period value to hz
		const int freq = kPaulaFreq / (pat.note_1 * 2);
		debug(DBG_SND, "SfxPlayer::handlePattern() adding sample freq = 0x%X", freq);
		game_audio_sfx_channel_t* ch = &player->channels[channel];
		ch->sample_data = pat.sample_buffer + pat.sample_start;
		ch->sample_len = pat.sample_len;
		ch->sample_loop_pos = pat.loop_pos;
		ch->sample_loop_len = pat.loop_len;
		ch->volume = pat.sample_volume;
		ch->pos.offset = 0;
		ch->pos.inc = (freq << Frac::BITS) / player->rate;
	}
}

static void _game_audio_sfx_handle_events(game_t* game) {
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
	uint8_t order = player->sfx_mod.order_table[player->sfx_mod.cur_order];
	const uint8_t *patternData = player->sfx_mod.data + player->sfx_mod.cur_pos + order * 1024;
	for (uint8_t ch = 0; ch < 4; ++ch) {
		_game_audio_sfx_handle_pattern(game, ch, patternData);
		patternData += 4;
	}
	player->sfx_mod.cur_pos += 4 * 4;
	debug(DBG_SND, "SfxPlayer::handleEvents() order = 0x%X curPos = 0x%X", order, player->sfx_mod.cur_pos);
	if (player->sfx_mod.cur_pos >= 1024) {
		player->sfx_mod.cur_pos = 0;
		order = player->sfx_mod.cur_order + 1;
		if (order == player->sfx_mod.num_order) {
			player->playing = false;
		}
		player->sfx_mod.cur_order = order;
	}
}

static void _game_audio_sfx_mix_samples(game_t* game, int16_t* buf, int len) {
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
	while (len != 0) {
		if (player->samples_left == 0) {
            _game_audio_sfx_handle_events(game);
			const int samplesPerTick = player->rate * (player->delay * 60 * 1000 / kPaulaFreq) / 1000;
			player->samples_left = samplesPerTick;
		}
		int count = player->samples_left;
		if (count > len) {
			count = len;
		}
		player->samples_left -= count;
		len -= count;
		for (int i = 0; i < count; ++i) {
            _game_audio_sfx_mix_channel(buf, &player->channels[0]);
            _game_audio_sfx_mix_channel(buf, &player->channels[3]);
			++buf;
            _game_audio_sfx_mix_channel(buf, &player->channels[1]);
            _game_audio_sfx_mix_channel(buf, &player->channels[2]);
			++buf;
		}
	}
}

static void _game_audio_sfx_read_samples(game_t* game, int16_t *buf, int len) {
    game_audio_sfx_player_t* player = &game->audio.sfx_player;
	if (player->delay != 0) {
		_game_audio_sfx_mix_samples(game, buf, len / 2);
	}
}

// Audio

static void _game_audio_stop_sound(game_t* game, uint8_t channel) {
    game->audio.channels[channel].data = 0;
}

static void _game_audio_init_raw(game_audio_channel_t* chan, const uint8_t *data, int freq, int volume, int mixingFreq) {
    chan->data = data + 8;
    frac_reset(&chan->pos, freq, mixingFreq);

    const int len = READ_BE_UINT16(data) * 2;
    chan->loop_len = READ_BE_UINT16(data + 2) * 2;
    chan->loop_pos = chan->loop_len ? len : 0;
    chan->len = len;

    chan->volume = volume;
}

static void _game_audio_play_sound_raw(game_t* game, uint8_t channel, const uint8_t *data, int freq, uint8_t volume) {
    game_audio_channel_t* chan = &game->audio.channels[channel];
    _game_audio_init_raw(chan, data, freq, volume, MIX_FREQ);
}

static void _game_play_sfx_music(game_t* game) {
    _game_audio_sfx_play(game, MIX_FREQ);
}

static void _game_audio_stop_sfx_music(game_t* game) {
    game->audio.sfx_player.playing = false;
}

static void _game_audio_stop_all(game_t* game) {
    for (uint8_t i = 0; i < MIX_CHANNELS; ++i) {
        _game_audio_stop_sound(game, i);
    }
    _game_audio_stop_sfx_music(game);
}

static void _game_audio_init(game_t* game, game_audio_callback_t callback) {
    memset(game->audio.channels, 0, sizeof(game->audio.channels));
    game->audio.callback = callback;
}

static const bool kAmigaStereoChannels = false;

static int16_t mix_i16(int sample1, int sample2) {
	const int sample = sample1 + sample2;
	return sample < -32768 ? -32768 : ((sample > 32767 ? 32767 : sample));
}

void _game_audio_mix_raw(game_audio_channel_t* chan, int16_t* sample) {
    if (chan->data) {
        uint32_t pos = frac_get_int(&chan->pos);
        chan->pos.offset += chan->pos.inc;
        if (chan->loop_len != 0) {
            if (pos >= chan->loop_pos + chan->loop_len) {
                pos = chan->loop_pos;
                chan->pos.offset = (chan->loop_pos << Frac::BITS) + chan->pos.inc;
            }
        } else {
            if (pos >= chan->len) {
                chan->data = 0;
                return;
            }
        }
        *sample = mix_i16(*sample, _to_i16(chan->data[pos] ^ 0x80) * chan->volume / 64);
    }
}

static void _game_audio_mix_channels(game_t* game, int16_t *samples, int count) {
    if (kAmigaStereoChannels) {
     for (int i = 0; i < count; i += 2) {
        _game_audio_mix_raw(&game->audio.channels[0], samples);
        _game_audio_mix_raw(&game->audio.channels[3], samples);
       ++samples;
       _game_audio_mix_raw(&game->audio.channels[1], samples);
       _game_audio_mix_raw(&game->audio.channels[2], samples);
       ++samples;
     }
   } else {
     for (int i = 0; i < count; i += 2) {
       for (int j = 0; j < MIX_CHANNELS; ++j) {
            _game_audio_mix_raw(&game->audio.channels[j], &samples[i]);
       }
       samples[i + 1] = samples[i];
     }
   }
  }

static void _game_audio_update(game_t* game, int num_samples) {
    assert(num_samples < MIX_BUF_SIZE);
    assert(num_samples < GAME_MAX_AUDIO_SAMPLES);
    memset(game->audio.samples, 0, num_samples*sizeof(int16_t));
    _game_audio_mix_channels(game, game->audio.samples, num_samples);
    _game_audio_sfx_read_samples(game, (int16_t *)game->audio.samples, num_samples);
    for (int i=0; i<num_samples; i++) {
        game->audio.sample_buffer[i] = ((((float)game->audio.samples[i])+32768.f)/32768.f) - 1.0f;
    }
    game->audio.callback.func(game->audio.sample_buffer, num_samples, game->audio.callback.user_data);
}

// VM
static void _op_mov_const(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	int16_t n = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_movConst(0x%02X, %d)", i, n);
	game->vm.vars[i] = n;
}

static void _op_mov(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint8_t j = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_mov(0x%02X, 0x%02X)", i, j);
	game->vm.vars[i] = game->vm.vars[j];
}

static void _op_add(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint8_t j = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_add(0x%02X, 0x%02X)", i, j);
	game->vm.vars[i] += game->vm.vars[j];
}

static int _get_sound_freq(uint8_t period) {
	return kPaulaFreq / (_periodTable[period] * 2);
}

static void _snd_playSound(game_t* game, uint16_t resNum, uint8_t freq, uint8_t vol, uint8_t channel) {
	debug(DBG_SND, "snd_playSound(0x%X, %d, %d, %d)", resNum, freq, vol, channel);
	if (vol == 0) {
		_game_audio_stop_sound(game, channel);
		return;
	}
	if (vol > 63) {
		vol = 63;
	}
	if (freq > 39) {
		freq = 39;
	}
	channel &= 3;
	switch (_state->_res.getDataType()) {
	case Resource::DT_AMIGA:
	case Resource::DT_ATARI:
	case Resource::DT_ATARI_DEMO:
	case Resource::DT_DOS: {
			MemEntry *me = &_state->_res._memList[resNum];
			if (me->status == Resource::STATUS_LOADED) {
                _game_audio_play_sound_raw(game, channel, me->bufPtr, _get_sound_freq(freq), vol);
			}
		}
		break;
	}
}

static void _op_add_const(game_t* game) {
	if (_state->_res.getDataType() == Resource::DT_DOS || _state->_res.getDataType() == Resource::DT_AMIGA || _state->_res.getDataType() == Resource::DT_ATARI) {
		if (_state->_res._currentPart == 16006 && game->vm.ptr.pc == _state->_res._segCode + 0x6D48) {
			warning("Script::op_addConst() workaround for infinite looping gun sound");
			// The script 0x27 slot 0x17 doesn't stop the gun sound from looping.
			// This is a bug in the original game code, confirmed by Eric Chahi and
			// addressed with the anniversary editions.
			// For older releases (DOS, Amiga), we play the 'stop' sound like it is
			// done in other part of the game code.
			//
			//  6D43: jmp(0x6CE5)
			//  6D46: break
			//  6D47: VAR(0x06) -= 50
			//
			_snd_playSound(game, 0x5B, 1, 64, 1);
		}
	}
	uint8_t i = fetchByte(&game->vm.ptr);
	int16_t n = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_addConst(0x%02X, %d)", i, n);
	game->vm.vars[i] += n;
}

static void _op_call(game_t* game) {
	uint16_t off = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_call(0x%X)", off);
	if (game->vm.stack_ptr == 0x40) {
		error("Script::op_call() ec=0x%X stack overflow", 0x8F);
	}
	game->vm.stack_calls[game->vm.stack_ptr] = game->vm.ptr.pc - _state->_res._segCode;
	++game->vm.stack_ptr;
	game->vm.ptr.pc = _state->_res._segCode + off;
}

static void _op_ret(game_t* game) {
	debug(DBG_SCRIPT, "Script::op_ret()");
	if (game->vm.stack_ptr == 0) {
		error("Script::op_ret() ec=0x%X stack underflow", 0x8F);
	}
	--game->vm.stack_ptr;
	game->vm.ptr.pc = _state->_res._segCode + game->vm.stack_calls[game->vm.stack_ptr];
}

static void _op_yieldTask(game_t* game) {
	debug(DBG_SCRIPT, "Script::op_yieldTask()");
	game->vm.paused = true;
}

static void _op_jmp(game_t* game) {
	uint16_t off = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_jmp(0x%02X)", off);
	game->vm.ptr.pc = _state->_res._segCode + off;
}

static void _op_install_task(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint16_t n = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_installTask(0x%X, 0x%X)", i, n);
	GAME_ASSERT(i < 0x40);
	game->vm.tasks[1][i] = n;
}

static void _op_jmp_if_var(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_jmpIfVar(0x%02X)", i);
	--game->vm.vars[i];
	if (game->vm.vars[i] != 0) {
		_op_jmp(game);
	} else {
		fetchWord(&game->vm.ptr);
	}
}

static void _fixUpPalette_changeScreen(int part, int screen) {
	int pal = -1;
	switch (part) {
	case 16004:
		if (screen == 0x47) { // bitmap resource #68
			pal = 8;
		}
		break;
	case 16006:
		if (screen == 0x4A) { // bitmap resources #144, #145
			pal = 1;
		}
		break;
	}
	if (pal != -1) {
		debug(DBG_SCRIPT, "Setting palette %d for part %d screen %d", pal, part, screen);
		_state->_vid.changePal(pal);
	}
}

static void _op_cond_jmp(game_t* game) {
	uint8_t op = fetchByte(&game->vm.ptr);
	const uint8_t var = fetchByte(&game->vm.ptr);;
	int16_t b = game->vm.vars[var];
	int16_t a;
	if (op & 0x80) {
		a = game->vm.vars[fetchByte(&game->vm.ptr)];
	} else if (op & 0x40) {
		a = fetchWord(&game->vm.ptr);
	} else {
		a = fetchByte(&game->vm.ptr);
	}
	debug(DBG_SCRIPT, "Script::op_condJmp(%d, 0x%02X, 0x%02X) var=0x%02X", op, b, a, var);
	bool expr = false;
	switch (op & 7) {
	case 0:
		expr = (b == a);
#ifdef BYPASS_PROTECTION
		if (_state->_res._currentPart == kPartCopyProtection) {
			//
			// 0CB8: jmpIf(VAR(0x29) == VAR(0x1E), @0CD3)
			// ...
			//
			if (var == 0x29 && (op & 0x80) != 0) {
				// 4 symbols
				game->vm.vars[0x29] = game->vm.vars[0x1E];
				game->vm.vars[0x2A] = game->vm.vars[0x1F];
				game->vm.vars[0x2B] = game->vm.vars[0x20];
				game->vm.vars[0x2C] = game->vm.vars[0x21];
				// counters
				game->vm.vars[0x32] = 6;
				game->vm.vars[0x64] = 20;
				warning("Script::op_condJmp() bypassing protection");
				expr = true;
			}
		}
#endif
		break;
	case 1:
		expr = (b != a);
		break;
	case 2:
		expr = (b > a);
		break;
	case 3:
		expr = (b >= a);
		break;
	case 4:
		expr = (b < a);
		break;
	case 5:
		expr = (b <= a);
		break;
	default:
		warning("Script::op_condJmp() invalid condition %d", (op & 7));
		break;
	}
	if (expr) {
		_op_jmp(game);
		if (var == VAR_SCREEN_NUM && game->vm.screen_num != game->vm.vars[VAR_SCREEN_NUM]) {
			_fixUpPalette_changeScreen(_state->_res._currentPart, game->vm.vars[VAR_SCREEN_NUM]);
			game->vm.screen_num = game->vm.vars[VAR_SCREEN_NUM];
		}
	} else {
		fetchWord(&game->vm.ptr);
	}
}

static void _op_set_palette(game_t* game) {
	uint16_t i = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_changePalette(%d)", i);
	const int num = i >> 8;
	if (_state->_vid._graphics->_fixUpPalette == FIXUP_PALETTE_REDRAW) {
		if (_state->_res._currentPart == 16001) {
			if (num == 10 || num == 16) {
				return;
			}
		}
		_state->_vid._nextPal = num;
	} else {
		_state->_vid._nextPal = num;
	}
}

static void _op_changeTasksState(game_t* game) {
	uint8_t start = fetchByte(&game->vm.ptr);
	uint8_t end = fetchByte(&game->vm.ptr);
	if (end < start) {
		warning("Script::op_changeTasksState() ec=0x%X (end < start)", 0x880);
		return;
	}
	uint8_t state = fetchByte(&game->vm.ptr);

	debug(DBG_SCRIPT, "Script::op_changeTasksState(%d, %d, %d)", start, end, state);

	if (state == 2) {
		for (; start <= end; ++start) {
			game->vm.tasks[1][start] = 0xFFFE;
		}
	} else if (state < 2) {
		for (; start <= end; ++start) {
			game->vm.states[1][start] = state;
		}
	}
}

static void _op_selectPage(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_selectPage(%d)", i);
	_state->_vid.setWorkPagePtr(i);
}

static void _op_fillPage(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint8_t color = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_fillPage(%d, %d)", i, color);
	_state->_vid.fillPage(i, color);
}

static void _op_copyPage(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint8_t j = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_copyPage(%d, %d)", i, j);
	_state->_vid.copyPage(i, j, game->vm.vars[VAR_SCROLL_Y]);
}

static void _inp_handleSpecialKeys() {
	if (_state->_sys._pi.pause) {
		if (_state->_res._currentPart != kPartCopyProtection && _state->_res._currentPart != kPartIntro) {
			_state->_sys._pi.pause = false;
			while (!_state->_sys._pi.pause && !_state->_sys._pi.quit) {
				_state->_sys.processEvents();
				_state->_sys.sleep(50);
			}
		}
		_state->_sys._pi.pause = false;
	}
	if (_state->_sys._pi.back) {
		_state->_sys._pi.back = false;
    }
	if (_state->_sys._pi.code) {
		_state->_sys._pi.code = false;
		if (_state->_res._hasPasswordScreen) {
			if (_state->_res._currentPart != kPartPassword && _state->_res._currentPart != kPartCopyProtection) {
				_state->_res._nextPart = kPartPassword;
			}
		}
	}
}

static void _op_updateDisplay(game_t* game) {
	uint8_t page = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_updateDisplay(%d)", page);
	_inp_handleSpecialKeys();

#ifndef BYPASS_PROTECTION
	// entered protection symbols match the expected values
	if (_state->_res._currentPart == kPartCopyProtection && game->vm.vars[0x67] == 1) {
		game->vm.vars[0xDC] = 33;
	}
#endif

	const int frameHz = 50;
	if (!game->vm.fast_mode && game->vm.vars[VAR_PAUSE_SLICES] != 0) {
		const int delay = _state->_sys.getTimeStamp() - game->vm.time_stamp;
		const int pause = game->vm.vars[VAR_PAUSE_SLICES] * 1000 / frameHz - delay;
		if (pause > 0) {
			_state->_sys.sleep(pause);
		}
	}
	game->vm.time_stamp = _state->_sys.getTimeStamp();
	game->vm.vars[0xF7] = 0;

	_state->_vid.updateDisplay(page, &_state->_sys);
}

static void _op_removeTask(game_t* game) {
	debug(DBG_SCRIPT, "Script::op_removeTask()");
	game->vm.ptr.pc = _state->_res._segCode + 0xFFFF;
	game->vm.paused = true;
}

static void _op_drawString(game_t* game) {
	uint16_t strId = fetchWord(&game->vm.ptr);
	uint16_t x = fetchByte(&game->vm.ptr);
	uint16_t y = fetchByte(&game->vm.ptr);
	uint16_t col = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_drawString(0x%03X, %d, %d, %d)", strId, x, y, col);
	_state->_vid.drawString(col, x, y, strId);
}

static void _op_sub(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint8_t j = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_sub(0x%02X, 0x%02X)", i, j);
	game->vm.vars[i] -= game->vm.vars[j];
}

static void _op_and(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint16_t n = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_and(0x%02X, %d)", i, n);
	game->vm.vars[i] = (uint16_t)game->vm.vars[i] & n;
}

static void _op_or(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint16_t n = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_or(0x%02X, %d)", i, n);
	game->vm.vars[i] = (uint16_t)game->vm.vars[i] | n;
}

static void _op_shl(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint16_t n = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_shl(0x%02X, %d)", i, n);
	game->vm.vars[i] = (uint16_t)game->vm.vars[i] << n;
}

static void _op_shr(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint16_t n = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_shr(0x%02X, %d)", i, n);
	game->vm.vars[i] = (uint16_t)game->vm.vars[i] >> n;
}

static void _op_playSound(game_t* game) {
	uint16_t resNum = fetchWord(&game->vm.ptr);
	uint8_t freq = fetchByte(&game->vm.ptr);
	uint8_t vol = fetchByte(&game->vm.ptr);
	uint8_t channel = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_playSound(0x%X, %d, %d, %d)", resNum, freq, vol, channel);
	_snd_playSound(game, resNum, freq, vol, channel);
}

static void _op_updateResources(game_t* game) {
	uint16_t num = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_updateResources(%d)", num);
	if (num == 0) {
		_game_audio_stop_all(game);
		_state->_res.invalidateRes();
	} else {
		_state->_res.update(num);
	}
}

static void _snd_playMusic(game_t* game, uint16_t resNum, uint16_t delay, uint8_t pos) {
	debug(DBG_SND, "snd_playMusic(0x%X, %d, %d)", resNum, delay, pos);
	// DT_AMIGA, DT_ATARI, DT_DOS
    if (resNum != 0) {
        _game_audio_sfx_load_module(game, resNum, delay, pos);
        _game_audio_sfx_start(game);
        _game_play_sfx_music(game);
    } else if (delay != 0) {
        _game_audio_sfx_set_events_delay(game, delay);
    } else {
        _game_audio_stop_sfx_music(game);
    }
}

static void _op_playMusic(game_t* game) {
	uint16_t resNum = fetchWord(&game->vm.ptr);
	uint16_t delay = fetchWord(&game->vm.ptr);
	uint8_t pos = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_playMusic(0x%X, %d, %d)", resNum, delay, pos);
	_snd_playMusic(game, resNum, delay, pos);
}

static const _opcode_func _opTable[] = {
	/* 0x00 */
	&_op_mov_const,
	&_op_mov,
	&_op_add,
	&_op_add_const,
	/* 0x04 */
	&_op_call,
	&_op_ret,
    &_op_yieldTask,
	&_op_jmp,
	/* 0x08 */
	&_op_install_task,
	&_op_jmp_if_var,
	&_op_cond_jmp,
	&_op_set_palette,
	/* 0x0C */
	&_op_changeTasksState,
	&_op_selectPage,
	&_op_fillPage,
	&_op_copyPage,
	/* 0x10 */
	&_op_updateDisplay,
	&_op_removeTask,
	&_op_drawString,
	&_op_sub,
	/* 0x14 */
	&_op_and,
	&_op_or,
	&_op_shl,
	&_op_shr,
	/* 0x18 */
	&_op_playSound,
	&_op_updateResources,
	&_op_playMusic
};

// VM
static void _game_vm_restart_at(game_t* game, int part, int pos) {
    _game_audio_stop_all(game);
	if (_state->_res.getDataType() == Resource::DT_DOS && part == kPartCopyProtection) {
		// VAR(0x54) indicates if the "Out of this World" title screen should be presented
		//
		//   0084: jmpIf(VAR(0x54) < 128, @00C4)
		//   ..
		//   008D: setPalette(num=0)
		//   0090: updateResources(res=18)
		//   ...
		//   00C4: setPalette(num=23)
		//   00CA: updateResources(res=71)

		// Use "Another World" title screen if language is set to French
		const bool awTitleScreen = (_state->_vid._stringsTable == Video::_stringsTableFr);
		game->vm.vars[0x54] = awTitleScreen ? 0x1 : 0x81;
	}
	_state->_res.setupPart(part);
	memset(game->vm.tasks, 0xFF, sizeof(game->vm.tasks));
	memset(game->vm.states, 0, sizeof(game->vm.states));
	game->vm.tasks[0][0] = 0;
	game->vm.screen_num = -1;
	if (pos >= 0) {
		game->vm.vars[0] = pos;
	}
	game->vm.start_time = game->vm.time_stamp = _state->_sys.getTimeStamp();
	if (part == kPartWater) {
		if (_state->_res._demo3Joy.start()) {
			memset(game->vm.vars, 0, sizeof(game->vm.vars));
		}
	}
}

static void _game_vm_setup_tasks(game_t* game) {
	if (_state->_res._nextPart != 0) {
        _game_vm_restart_at(game, _state->_res._nextPart, -1);
		_state->_res._nextPart = 0;
	}
	for (int i = 0; i < 0x40; ++i) {
		game->vm.states[0][i] = game->vm.states[1][i];
		uint16_t n = game->vm.tasks[1][i];
		if (n != 0xFFFF) {
			game->vm.tasks[0][i] = (n == 0xFFFE) ? 0xFFFF : n;
			game->vm.tasks[1][i] = 0xFFFF;
		}
	}
}

static void _game_vm_update_input(game_t* game) {
	_state->_sys.processEvents();
	if (_state->_res._currentPart == kPartPassword) {
		char c = _state->_sys._pi.lastChar;
		if (c == 8 || /*c == 0xD ||*/ c == 0 || (c >= 'a' && c <= 'z')) {
			game->vm.vars[VAR_LAST_KEYCHAR] = c & ~0x20;
			_state->_sys._pi.lastChar = 0;
		}
	}
	int16_t lr = 0;
	int16_t m = 0;
	int16_t ud = 0;
	int16_t jd = 0;
	if (_state->_sys._pi.dirMask & PlayerInput::DIR_RIGHT) {
		lr = 1;
		m |= 1;
	}
	if (_state->_sys._pi.dirMask & PlayerInput::DIR_LEFT) {
		lr = -1;
		m |= 2;
	}
	if (_state->_sys._pi.dirMask & PlayerInput::DIR_DOWN) {
		ud = jd = 1;
		m |= 4; // crouch
	}
    if (_state->_sys._pi.dirMask & PlayerInput::DIR_UP) {
        ud = jd = -1;
        m |= 8; // jump
    }
	if (!(_state->_res.getDataType() == Resource::DT_AMIGA || _state->_res.getDataType() == Resource::DT_ATARI)) {
		game->vm.vars[VAR_HERO_POS_UP_DOWN] = ud;
	}
	game->vm.vars[VAR_HERO_POS_JUMP_DOWN] = jd;
	game->vm.vars[VAR_HERO_POS_LEFT_RIGHT] = lr;
	game->vm.vars[VAR_HERO_POS_MASK] = m;
	int16_t action = 0;
	if (_state->_sys._pi.action) {
		action = 1;
		m |= 0x80;
	}
	game->vm.vars[VAR_HERO_ACTION] = action;
	game->vm.vars[VAR_HERO_ACTION_POS_MASK] = m;
	if (_state->_res._currentPart == kPartWater) {
		const uint8_t mask = _state->_res._demo3Joy.update();
		if (mask != 0) {
			game->vm.vars[VAR_HERO_ACTION_POS_MASK] = mask;
			game->vm.vars[VAR_HERO_POS_MASK] = mask & 15;
			game->vm.vars[VAR_HERO_POS_LEFT_RIGHT] = 0;
			if (mask & 1) {
				game->vm.vars[VAR_HERO_POS_LEFT_RIGHT] = 1;
			}
			if (mask & 2) {
				game->vm.vars[VAR_HERO_POS_LEFT_RIGHT] = -1;
			}
			game->vm.vars[VAR_HERO_POS_JUMP_DOWN] = 0;
			if (mask & 4) {
				game->vm.vars[VAR_HERO_POS_JUMP_DOWN] = 1;
			}
			if (mask & 8) {
				game->vm.vars[VAR_HERO_POS_JUMP_DOWN] = -1;
			}
			game->vm.vars[VAR_HERO_ACTION] = (mask >> 7);
		}
	}
}

static void _game_vm_execute_task(game_t* game) {
	while (!game->vm.paused) {
		uint8_t opcode = fetchByte(&game->vm.ptr);
		if (opcode & 0x80) {
			const uint16_t off = ((opcode << 8) | fetchByte(&game->vm.ptr)) << 1;
			_state->_res._useSegVideo2 = false;
			Point pt;
			pt.x = fetchByte(&game->vm.ptr);
			pt.y = fetchByte(&game->vm.ptr);
			int16_t h = pt.y - 199;
			if (h > 0) {
				pt.y = 199;
				pt.x += h;
			}
			debug(DBG_VIDEO, "vid_opcd_0x80 : opcode=0x%X off=0x%X x=%d y=%d", opcode, off, pt.x, pt.y);
			_state->_vid.setDataBuffer(_state->_res._segVideo1, off);
			_state->_vid.drawShape(0xFF, 64, &pt);
		} else if (opcode & 0x40) {
			Point pt;
			const uint8_t offsetHi = fetchByte(&game->vm.ptr);
			const uint16_t off = ((offsetHi << 8) | fetchByte(&game->vm.ptr)) << 1;
			pt.x = fetchByte(&game->vm.ptr);
			_state->_res._useSegVideo2 = false;
			if (!(opcode & 0x20)) {
				if (!(opcode & 0x10)) {
					pt.x = (pt.x << 8) | fetchByte(&game->vm.ptr);
				} else {
					pt.x = game->vm.vars[pt.x];
				}
			} else {
				if (opcode & 0x10) {
					pt.x += 0x100;
				}
			}
			pt.y = fetchByte(&game->vm.ptr);
			if (!(opcode & 8)) {
				if (!(opcode & 4)) {
					pt.y = (pt.y << 8) | fetchByte(&game->vm.ptr);
				} else {
					pt.y = game->vm.vars[pt.y];
				}
			}
			uint16_t zoom = 64;
			if (!(opcode & 2)) {
				if (opcode & 1) {
					zoom = game->vm.vars[fetchByte(&game->vm.ptr)];
				}
			} else {
				if (opcode & 1) {
					_state->_res._useSegVideo2 = true;
				} else {
					zoom = fetchByte(&game->vm.ptr);
				}
			}
			debug(DBG_VIDEO, "vid_opcd_0x40 : off=0x%X x=%d y=%d", off, pt.x, pt.y);
			_state->_vid.setDataBuffer(_state->_res._useSegVideo2 ? _state->_res._segVideo2 : _state->_res._segVideo1, off);
			_state->_vid.drawShape(0xFF, zoom, &pt);
		} else {
			if (opcode > 0x1A) {
				error("Script::executeTask() ec=0x%X invalid opcode=0x%X", 0xFFF, opcode);
			} else {
				(*_opTable[opcode])(game);
			}
		}
	}
}

static void _game_vm_run(game_t* game) {
	for (int i = 0; i < 0x40 && !_state->_sys._pi.quit; ++i) {
		if (game->vm.states[0][i] == 0) {
			uint16_t n = game->vm.tasks[0][i];
			if (n != 0xFFFF) {
				game->vm.ptr.pc = _state->_res._segCode + n;
				game->vm.stack_ptr = 0;
				game->vm.paused = false;
				debug(DBG_SCRIPT, "Script::runTasks() i=0x%02X n=0x%02X", i, n);
                _game_vm_execute_task(game);
				game->vm.tasks[0][i] = game->vm.ptr.pc - _state->_res._segCode;
				debug(DBG_SCRIPT, "Script::runTasks() i=0x%02X pos=0x%X", i, game->vm.tasks[0][i]);
			}
		}
	}
}

// Game
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
	_state->_res.readEntries();

	debug(DBG_INFO, "Using sokol graphics");
    _state->_vid._graphics = &_state->_graphics;
    _state->_graphics.setSystem(&_state->_sys);
    gfx_fb fbs = {
        .buffer = {
            game->gfx.fbs[0].buffer,
            game->gfx.fbs[1].buffer,
            game->gfx.fbs[2].buffer,
            game->gfx.fbs[3].buffer
        }
    };
    _state->_graphics.setBuffers(&fbs);
    _state->_graphics.init();
	_state->_vid.setDefaultFont();

    if (desc->demo3_joy_inputs && _state->_res.getDataType() == Resource::DT_DOS) {
		_state->_res.readDemo3Joy();
	}

    _state->_sys.setBuffer(game->gfx.fb, game->gfx.palette);
    switch (_state->_res.getDataType()) {
    case Resource::DT_DOS:
    case Resource::DT_AMIGA:
    case Resource::DT_ATARI:
    case Resource::DT_ATARI_DEMO:
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
    }
    game->audio.callback = desc->audio.callback;
    _game_audio_init(game, desc->audio.callback);

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
    const int num = _state->_part_num;
    if (num < 36) {
        _game_vm_restart_at(game, _restartPos[num * 2], _restartPos[num * 2 + 1]);
    } else {
        _game_vm_restart_at(game, num, -1);
    }
    game->title = _state->_res.getGameTitle(lang);
}

void _game_exec(game_t* game, uint32_t ms) {
    (void)game;
    _state->_sys._elapsed += ms;
    if(_state->_sys._sleep) {
        if(ms > _state->_sys._sleep) {
            _state->_sys._sleep = 0;
        } else {
            _state->_sys._sleep -= ms;
        }
        return;
    }

    _game_vm_setup_tasks(game);
    _game_vm_update_input(game);
    _game_vm_run(game);
    const int num_frames = saudio_expect();
    if (num_frames > 0) {
        const int num_samples = num_frames * saudio_channels();
        _game_audio_update(game, num_samples);
    }

    _state->_sys._sleep += 16;
}

void game_exec(game_t* game, uint32_t ms) {
    GAME_ASSERT(game && game->valid);

    if (0 == game->debug.callback.func) {
        // run without debug hook
        _game_exec(game, ms);
    } else {
        // run with debug hook
        if(!(*game->debug.stopped)) {
            _game_exec(game, ms);
            game->debug.callback.func(game->debug.callback.user_data, game->vm.ptr.pc - _state->_res._segCode);
        }
    }
}

void game_cleanup(game_t* game) {
    GAME_ASSERT(game && game->valid);
    (void)game;
    _state->_graphics.fini();
    _game_audio_stop_all(game);
    delete _state;
}

gfx_display_info_t game_display_info(game_t* game) {
    GAME_ASSERT(game && game->valid);
    const gfx_display_info_t res = {
        .frame = {
            .dim = {
                .width = GFX_W,
                .height = GFX_H,
            },
            .bytes_per_pixel = 1,
            .buffer = {
                .ptr = game->gfx.fb,
                .size = GFX_W*GFX_H,
            }
        },
        .screen = {
            .x = 0,
            .y = 0,
            .width = GFX_W,
            .height = GFX_H,
        },
        .palette = {
            .ptr = game->gfx.palette,
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
    (void)game;
    _state->_sys._pi.lastChar = (char)c;
}

void game_get_resources(game_t* game, game_mem_entry_t** res) {
    (void)game;
    *res = (game_mem_entry_t*)&_state->_res._memList[0];
}

void game_get_vars(game_t* game, int16_t** vars) {
    *vars = &game->vm.vars[0];
}

uint8_t* game_get_pc(game_t* game) {
    (void)game;
    return _state->_res._segCode;
}

bool game_get_res_buf(int id, uint8_t* dst) {
    MemEntry* me = &_state->_res._memList[id];
    return _state->_res.readBank(me, dst);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

