#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gfx.h"
#include "common.h"
#include "game.h"
#include "graphics.h"
#include "graphics_sokol.h"
#include "systemstub.h"
#include "sokol_audio.h"
#include "sokol_sys.h"
#include "util.h"
#include "file.h"
#include "raw-data.h"
#include "unpack.h"
#include "bitmap.h"

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

// from https://en.wikipedia.org/wiki/Enhanced_Graphics_Adapter
const uint8_t _paletteEGA[] = {
        0x00, 0x00, 0x00, // black #0
        0x00, 0x00, 0xAA, // blue #1
        0x00, 0xAA, 0x00, // green #2
        0x00, 0xAA, 0xAA, // cyan #3
        0xAA, 0x00, 0x00, // red #4
        0xAA, 0x00, 0xAA, // magenta #5
        0xAA, 0x55, 0x00, // yellow, brown #20
        0xAA, 0xAA, 0xAA, // white, light gray #7
        0x55, 0x55, 0x55, // dark gray, bright black #56
        0x55, 0x55, 0xFF, // bright blue #57
        0x55, 0xFF, 0x55, // bright green #58
        0x55, 0xFF, 0xFF, // bright cyan #59
        0xFF, 0x55, 0x55, // bright red #60
        0xFF, 0x55, 0xFF, // bright magenta #61
        0xFF, 0xFF, 0x55, // bright yellow #62
        0xFF, 0xFF, 0xFF, // bright white #63
};

static const game_str_entry_t _strings_table_fr[] = {
	{ 0x001, "P E A N U T  3000" },
	{ 0x002, "Copyright  } 1990 Peanut Computer, Inc.\nAll rights reserved.\n\nCDOS Version 5.01" },
	{ 0x003, "2" },
	{ 0x004, "3" },
	{ 0x005, "." },
	{ 0x006, "A" },
	{ 0x007, "@" },
	{ 0x008, "PEANUT 3000" },
	{ 0x00A, "R" },
	{ 0x00B, "U" },
	{ 0x00C, "N" },
	{ 0x00D, "P" },
	{ 0x00E, "R" },
	{ 0x00F, "O" },
	{ 0x010, "J" },
	{ 0x011, "E" },
	{ 0x012, "C" },
	{ 0x013, "T" },
	{ 0x014, "Shield 9A.5f Ok" },
	{ 0x015, "Flux % 5.0177 Ok" },
	{ 0x016, "CDI Vector ok" },
	{ 0x017, " %%%ddd ok" },
	{ 0x018, "Race-Track ok" },
	{ 0x019, "SYNCHROTRON" },
	{ 0x01A, "E: 23%\ng: .005\n\nRK: 77.2L\n\nopt: g+\n\n Shield:\n1: OFF\n2: ON\n3: ON\n\nP~: 1\n" },
	{ 0x01B, "ON" },
	{ 0x01C, "-" },
	{ 0x021, "|" },
	{ 0x022, "--- Etude theorique ---" },
	{ 0x023, " L'EXPERIENCE DEBUTERA DANS    SECONDES." },
	{ 0x024, "20" },
	{ 0x025, "19" },
	{ 0x026, "18" },
	{ 0x027, "4" },
	{ 0x028, "3" },
	{ 0x029, "2" },
	{ 0x02A, "1" },
	{ 0x02B, "0" },
	{ 0x02C, "L E T ' S   G O" },
	{ 0x031, "- Phase 0:\nINJECTION des particules\ndans le synchrotron" },
	{ 0x032, "- Phase 1:\nACCELERATION des particules." },
	{ 0x033, "- Phase 2:\nEJECTION des particules\nsur le bouclier." },
	{ 0x034, "A  N  A  L  Y  S  E" },
	{ 0x035, "- RESULTAT:\nProbabilites de creer de:\n ANTI-MATIERE: 91.V %\n NEUTRINO 27:  0.04 %\n NEUTRINO 424: 18 %\n" },
	{ 0x036, "Verification par la pratique O/N ?" },
	{ 0x037, "SUR ?" },
	{ 0x038, "MODIFICATION DES PARAMETRES\nRELATIFS A L'ACCELERATEUR\nDE PARTICULES (SYNCHROTRON)." },
	{ 0x039, "SIMULATION DE L'EXPERIENCE ?" },
	{ 0x03C, "t---t" },
	{ 0x03D, "000 ~" },
	{ 0x03E, ".20x14dd" },
	{ 0x03F, "gj5r5r" },
	{ 0x040, "tilgor 25%" },
	{ 0x041, "12% 33% checked" },
	{ 0x042, "D=4.2158005584" },
	{ 0x043, "d=10.00001" },
	{ 0x044, "+" },
	{ 0x045, "*" },
	{ 0x046, "% 304" },
	{ 0x047, "gurgle 21" },
	{ 0x048, "{{{{" },
	{ 0x049, "Delphine Software" },
	{ 0x04A, "By Eric Chahi" },
	{ 0x04B, "5" },
	{ 0x04C, "17" },
	{ 0x12C, "0" },
	{ 0x12D, "1" },
	{ 0x12E, "2" },
	{ 0x12F, "3" },
	{ 0x130, "4" },
	{ 0x131, "5" },
	{ 0x132, "6" },
	{ 0x133, "7" },
	{ 0x134, "8" },
	{ 0x135, "9" },
	{ 0x136, "A" },
	{ 0x137, "B" },
	{ 0x138, "C" },
	{ 0x139, "D" },
	{ 0x13A, "E" },
	{ 0x13B, "F" },
	{ 0x13C, "       CODE D'ACCES:" },
	{ 0x13D, "PRESSEZ LE BOUTON POUR CONTINUER" },
	{ 0x13E, "   ENTRER LE CODE D'ACCES" },
	{ 0x13F, "MOT DE PASSE INVALIDE !" },
	{ 0x140, "ANNULER" },
	{ 0x141, "     INSEREZ LA DISQUETTE ?\n\n\n\n\n\n\n\n\nPRESSEZ UNE TOUCHE POUR CONTINUER" },
	{ 0x142, "SELECTIONNER LES SYMBOLES CORRESPONDANTS\nA LA POSITION\nDE LA ROUE DE PROTECTION" },
	{ 0x143, "CHARGEMENT..." },
	{ 0x144, "             ERREUR" },
	{ 0x15E, "LDKD" },
	{ 0x15F, "HTDC" },
	{ 0x160, "CLLD" },
	{ 0x161, "FXLC" },
	{ 0x162, "KRFK" },
	{ 0x163, "XDDJ" },
	{ 0x164, "LBKG" },
	{ 0x165, "KLFB" },
	{ 0x166, "TTCT" },
	{ 0x167, "DDRX" },
	{ 0x168, "TBHK" },
	{ 0x169, "BRTD" },
	{ 0x16A, "CKJL" },
	{ 0x16B, "LFCK" },
	{ 0x16C, "BFLX" },
	{ 0x16D, "XJRT" },
	{ 0x16E, "HRTB" },
	{ 0x16F, "HBHK" },
	{ 0x170, "JCGB" },
	{ 0x171, "HHFL" },
	{ 0x172, "TFBB" },
	{ 0x173, "TXHF" },
	{ 0x174, "JHJL" },
	{ 0x181, "PAR" },
	{ 0x182, "ERIC CHAHI" },
	{ 0x183, "          MUSIQUES ET BRUITAGES" },
	{ 0x184, "DE" },
	{ 0x185, "JEAN-FRANCOIS FREITAS" },
	{ 0x186, "VERSION IBM PC" },
	{ 0x187, "      PAR" },
	{ 0x188, " DANIEL MORAIS" },
	{ 0x18B, "PUIS PRESSER LE BOUTON" },
	{ 0x18C, "POSITIONNER LE JOYSTICK EN HAUT A GAUCHE" },
	{ 0x18D, " POSITIONNER LE JOYSTICK AU CENTRE" },
	{ 0x18E, " POSITIONNER LE JOYSTICK EN BAS A DROITE" },
	{ 0x258, "       Conception ..... Eric Chahi" },
	{ 0x259, "    Programmation ..... Eric Chahi" },
	{ 0x25A, "     Graphismes ....... Eric Chahi" },
	{ 0x25B, "Musique de ...... Jean-francois Freitas" },
	{ 0x25C, "              Bruitages" },
	{ 0x25D, "        Jean-Francois Freitas\n             Eric Chahi" },
	{ 0x263, "               Merci a" },
	{ 0x264, "           Jesus Martinez\n\n          Daniel Morais\n\n        Frederic Savoir\n\n      Cecile Chahi\n\n    Philippe Delamarre\n\n  Philippe Ulrich\n\nSebastien Berthet\n\nPierre Gousseau" },
	{ 0x265, "Now Go Back To Another Earth" },
	{ 0x190, "Bonsoir professeur." },
	{ 0x191, "Je vois que Monsieur a pris\nsa Ferrari." },
	{ 0x192, "IDENTIFICATION" },
	{ 0x193, "Monsieur est en parfaite sante." },
	{ 0x194, "O" },
	{ 0x193, "AU BOULOT !!!\n" },
	{ 0xFFFF, 0 }
};

const game_str_entry_t _strings_table_eng[] = {
	{ 0x001, "P E A N U T  3000" },
	{ 0x002, "Copyright  } 1990 Peanut Computer, Inc.\nAll rights reserved.\n\nCDOS Version 5.01" },
	{ 0x003, "2" },
	{ 0x004, "3" },
	{ 0x005, "." },
	{ 0x006, "A" },
	{ 0x007, "@" },
	{ 0x008, "PEANUT 3000" },
	{ 0x00A, "R" },
	{ 0x00B, "U" },
	{ 0x00C, "N" },
	{ 0x00D, "P" },
	{ 0x00E, "R" },
	{ 0x00F, "O" },
	{ 0x010, "J" },
	{ 0x011, "E" },
	{ 0x012, "C" },
	{ 0x013, "T" },
	{ 0x014, "Shield 9A.5f Ok" },
	{ 0x015, "Flux % 5.0177 Ok" },
	{ 0x016, "CDI Vector ok" },
	{ 0x017, " %%%ddd ok" },
	{ 0x018, "Race-Track ok" },
	{ 0x019, "SYNCHROTRON" },
	{ 0x01A, "E: 23%\ng: .005\n\nRK: 77.2L\n\nopt: g+\n\n Shield:\n1: OFF\n2: ON\n3: ON\n\nP~: 1\n" },
	{ 0x01B, "ON" },
	{ 0x01C, "-" },
	{ 0x021, "|" },
	{ 0x022, "--- Theoretical study ---" },
	{ 0x023, " THE EXPERIMENT WILL BEGIN IN    SECONDS" },
	{ 0x024, "  20" },
	{ 0x025, "  19" },
	{ 0x026, "  18" },
	{ 0x027, "  4" },
	{ 0x028, "  3" },
	{ 0x029, "  2" },
	{ 0x02A, "  1" },
	{ 0x02B, "  0" },
	{ 0x02C, "L E T ' S   G O" },
	{ 0x031, "- Phase 0:\nINJECTION of particles\ninto synchrotron" },
	{ 0x032, "- Phase 1:\nParticle ACCELERATION." },
	{ 0x033, "- Phase 2:\nEJECTION of particles\non the shield." },
	{ 0x034, "A  N  A  L  Y  S  I  S" },
	{ 0x035, "- RESULT:\nProbability of creating:\n ANTIMATTER: 91.V %\n NEUTRINO 27:  0.04 %\n NEUTRINO 424: 18 %\n" },
	{ 0x036, "   Practical verification Y/N ?" },
	{ 0x037, "SURE ?" },
	{ 0x038, "MODIFICATION OF PARAMETERS\nRELATING TO PARTICLE\nACCELERATOR (SYNCHROTRON)." },
	{ 0x039, "       RUN EXPERIMENT ?" },
	{ 0x03C, "t---t" },
	{ 0x03D, "000 ~" },
	{ 0x03E, ".20x14dd" },
	{ 0x03F, "gj5r5r" },
	{ 0x040, "tilgor 25%" },
	{ 0x041, "12% 33% checked" },
	{ 0x042, "D=4.2158005584" },
	{ 0x043, "d=10.00001" },
	{ 0x044, "+" },
	{ 0x045, "*" },
	{ 0x046, "% 304" },
	{ 0x047, "gurgle 21" },
	{ 0x048, "{{{{" },
	{ 0x049, "Delphine Software" },
	{ 0x04A, "By Eric Chahi" },
	{ 0x04B, "  5" },
	{ 0x04C, "  17" },
	{ 0x12C, "0" },
	{ 0x12D, "1" },
	{ 0x12E, "2" },
	{ 0x12F, "3" },
	{ 0x130, "4" },
	{ 0x131, "5" },
	{ 0x132, "6" },
	{ 0x133, "7" },
	{ 0x134, "8" },
	{ 0x135, "9" },
	{ 0x136, "A" },
	{ 0x137, "B" },
	{ 0x138, "C" },
	{ 0x139, "D" },
	{ 0x13A, "E" },
	{ 0x13B, "F" },
	{ 0x13C, "        ACCESS CODE:" },
	{ 0x13D, "PRESS BUTTON OR RETURN TO CONTINUE" },
	{ 0x13E, "   ENTER ACCESS CODE" },
	{ 0x13F, "   INVALID PASSWORD !" },
	{ 0x140, "ANNULER" },
	{ 0x141, "      INSERT DISK ?\n\n\n\n\n\n\n\n\nPRESS ANY KEY TO CONTINUE" },
	{ 0x142, " SELECT SYMBOLS CORRESPONDING TO\n THE POSITION\n ON THE CODE WHEEL" },
	{ 0x143, "    LOADING..." },
	{ 0x144, "              ERROR" },
	{ 0x15E, "LDKD" },
	{ 0x15F, "HTDC" },
	{ 0x160, "CLLD" },
	{ 0x161, "FXLC" },
	{ 0x162, "KRFK" },
	{ 0x163, "XDDJ" },
	{ 0x164, "LBKG" },
	{ 0x165, "KLFB" },
	{ 0x166, "TTCT" },
	{ 0x167, "DDRX" },
	{ 0x168, "TBHK" },
	{ 0x169, "BRTD" },
	{ 0x16A, "CKJL" },
	{ 0x16B, "LFCK" },
	{ 0x16C, "BFLX" },
	{ 0x16D, "XJRT" },
	{ 0x16E, "HRTB" },
	{ 0x16F, "HBHK" },
	{ 0x170, "JCGB" },
	{ 0x171, "HHFL" },
	{ 0x172, "TFBB" },
	{ 0x173, "TXHF" },
	{ 0x174, "JHJL" },
	{ 0x181, " BY" },
	{ 0x182, "ERIC CHAHI" },
	{ 0x183, "         MUSIC AND SOUND EFFECTS" },
	{ 0x184, " " },
	{ 0x185, "JEAN-FRANCOIS FREITAS" },
	{ 0x186, "IBM PC VERSION" },
	{ 0x187, "      BY" },
	{ 0x188, " DANIEL MORAIS" },
	{ 0x18B, "       THEN PRESS FIRE" },
	{ 0x18C, " PUT THE PADDLE ON THE UPPER LEFT CORNER" },
	{ 0x18D, "PUT THE PADDLE IN CENTRAL POSITION" },
	{ 0x18E, "PUT THE PADDLE ON THE LOWER RIGHT CORNER" },
	{ 0x258, "      Designed by ..... Eric Chahi" },
	{ 0x259, "    Programmed by...... Eric Chahi" },
	{ 0x25A, "      Artwork ......... Eric Chahi" },
	{ 0x25B, "Music by ........ Jean-francois Freitas" },
	{ 0x25C, "            Sound effects" },
	{ 0x25D, "        Jean-Francois Freitas\n             Eric Chahi" },
	{ 0x263, "              Thanks To" },
	{ 0x264, "           Jesus Martinez\n\n          Daniel Morais\n\n        Frederic Savoir\n\n      Cecile Chahi\n\n    Philippe Delamarre\n\n  Philippe Ulrich\n\nSebastien Berthet\n\nPierre Gousseau" },
	{ 0x265, "Now Go Out Of This World" },
	{ 0x190, "Good evening professor." },
	{ 0x191, "I see you have driven here in your\nFerrari." },
	{ 0x192, "IDENTIFICATION" },
	{ 0x193, "Monsieur est en parfaite sante." },
	{ 0x194, "Y\n" },
	{ 0x193, "AU BOULOT !!!\n" },
	{ 0xFFFF, 0 }
};

const game_str_entry_t _stringsTableDemo[] = {
	{ 0x1F4, "Over Two Years in the Making" },
	{ 0x1F5, "   A New, State\nof the Art, Polygon\n  Graphics System" },
	{ 0x1F6, "   Comes to the\nComputer With Full\n Screen Graphics" },
	{ 0x1F7, "While conducting a nuclear fission\nexperiment at your local\nparticle accelerator ..." },
	{ 0x1F8, "Nature decides to put a little\n    extra spin on the ball" },
	{ 0x1F9, "And sends you ..." },
	{ 0x1FA, "     Out of this World\nA Cinematic Action Adventure\n from Interplay Productions\n                    \n       By Eric CHAHI      \n\n  IBM version : D.MORAIS\n" },
	{ 0xFFFF, 0 }
};

static const char* _str0x194AtariDemo = "Je signale que Monsieur\na tout de meme 40 minutes\net 21 secondes de retard.";

const uint8_t _mem_list_parts[][4] = {
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

static const char *_kGameTitleEU = "Another World";
static const char *_kGameTitleUS = "Out Of This World";

typedef void (*_opcode_func)(game_t* game);

class GameState {
public:
    GraphicsSokol _graphics;
    SokolSystem _sys;
    int _part_num;
};

GameState* _state;
bool Graphics::_is1991 = false;

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

static const char* _find_string(const game_str_entry_t* strings_table, int id) {
	for (const game_str_entry_t *se = strings_table; se->id != 0xFFFF; ++se) {
		if (se->id == id) {
			return se->str;
		}
	}
	return 0;
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
			game_mem_entry_t *me = &game->res.mem_list[resNum];
			if (me->status == GAME_RES_STATUS_LOADED && me->type == RT_SOUND) {
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
	game_mem_entry_t *me = &game->res.mem_list[resNum];
	if (me->status == GAME_RES_STATUS_LOADED && me->type == RT_MUSIC) {
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

// Video
static void _game_video_read_palette_ega(const uint8_t *buf, int num, Color pal[16]) {
	const uint8_t *p = buf + num * 16 * sizeof(uint16_t);
	p += 1024; // EGA colors are stored after VGA (Amiga)
	for (int i = 0; i < 16; ++i) {
		const uint16_t color = READ_BE_UINT16(p); p += 2;
		if (1) {
			const uint8_t *ega = &_paletteEGA[3 * ((color >> 12) & 15)];
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

static void _game_video_read_palette_amiga(const uint8_t *buf, int num, Color pal[16]) {
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

static void _game_video_change_pal(game_t* game, uint8_t palNum) {
    if (palNum < 32 && palNum != game->video.current_pal) {
		Color pal[16];
		if (game->res.data_type == DT_DOS && game->video.use_ega) {
			_game_video_read_palette_ega(game->res.seg_video_pal, palNum, pal);
		} else {
			_game_video_read_palette_amiga(game->res.seg_video_pal, palNum, pal);
		}
		_state->_graphics.setPalette(pal, 16);
		game->video.current_pal = palNum;
	}
}

uint8_t _game_video_get_page_ptr(game_t* game, uint8_t page) {
	uint8_t p;
	if (page <= 3) {
		p = page;
	} else {
		switch (page) {
		case 0xFF:
			p = game->video.buffers[2];
			break;
		case 0xFE:
			p = game->video.buffers[1];
			break;
		default:
			p = 0; // XXX check
			warning("Video::getPagePtr() p != [0,1,2,3,0xFF,0xFE] == 0x%X", page);
			break;
		}
	}
	return p;
}

static void _game_video_set_work_page_ptr(game_t* game, uint8_t page) {
	debug(DBG_VIDEO, "Video::setWorkPagePtr(%d)", page);
	game->video.buffers[0] = _game_video_get_page_ptr(game, page);
}

static void _game_video_fill_page(game_t* game, uint8_t page, uint8_t color) {
	debug(DBG_VIDEO, "Video::fillPage(%d, %d)", page, color);
	_state->_graphics.clearBuffer(_game_video_get_page_ptr(game, page), color);
}

static void _game_video_copy_page(game_t* game, uint8_t src, uint8_t dst, int16_t vscroll) {
	debug(DBG_VIDEO, "Video::copyPage(%d, %d)", src, dst);
	if (src >= 0xFE || ((src &= ~0x40) & 0x80) == 0) { // no vscroll
		_state->_graphics.copyBuffer(_game_video_get_page_ptr(game, dst), _game_video_get_page_ptr(game, src));
	} else {
		uint8_t sl = _game_video_get_page_ptr(game, src & 3);
		uint8_t dl = _game_video_get_page_ptr(game, dst);
		if (sl != dl && vscroll >= -199 && vscroll <= 199) {
			_state->_graphics.copyBuffer(dl, sl, vscroll);
		}
	}
}

static void _game_video_update_display(game_t* game, uint8_t page, SystemStub *stub) {
	debug(DBG_VIDEO, "Video::updateDisplay(%d)", page);
	if (page != 0xFE) {
		if (page == 0xFF) {
			SWAP(game->video.buffers[1], game->video.buffers[2]);
		} else {
			game->video.buffers[1] = _game_video_get_page_ptr(game, page);
		}
	}
	if (game->video.next_pal != 0xFF) {
        _game_video_change_pal(game, game->video.next_pal);
		game->video.next_pal = 0xFF;
	}
	_state->_graphics.drawBuffer(game->video.buffers[1], stub);
}

static void _game_video_draw_string(game_t* game, uint8_t color, uint16_t x, uint16_t y, uint16_t strId) {
	bool escapedChars = false;
	const char *str = 0;
	if (game->res.data_type == DT_ATARI_DEMO && strId == 0x194) {
		str = _str0x194AtariDemo;
	} else {
		str = _find_string(game->video.strings_table, strId);
		if (!str && game->res.data_type == DT_DOS) {
			str = _find_string(_stringsTableDemo, strId);
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
			_state->_graphics.drawStringChar(game->video.buffers[0], color, str[i], &pt);
			++x;
		}
	}
}

static void _game_video_set_data_buffer(game_t* game, uint8_t *dataBuf, uint16_t offset) {
    game->video.data_buf = dataBuf;
	game->video.p_data.pc = dataBuf + offset;
}

static void _game_video_fill_polygon(game_t* game, uint16_t color, uint16_t zoom, const Point *pt) {
	const uint8_t *p = game->video.p_data.pc;

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
		_state->_graphics.drawPoint(game->video.buffers[0], color, pt);
	} else {
		_state->_graphics.drawQuadStrip(game->video.buffers[0], color, &qs);
	}
}

static void _game_video_draw_shape(game_t* game, uint8_t color, uint16_t zoom, const Point *pt);

static void _game_video_draw_shape_parts(game_t* game, uint16_t zoom, const Point *pgc) {
	Point pt;
	pt.x = pgc->x - fetchByte(&game->video.p_data) * zoom / 64;
	pt.y = pgc->y - fetchByte(&game->video.p_data) * zoom / 64;
	int16_t n = fetchByte(&game->video.p_data);
	debug(DBG_VIDEO, "Video::drawShapeParts n=%d", n);
	for ( ; n >= 0; --n) {
		uint16_t offset = fetchWord(&game->video.p_data);
		Point po(pt);
		po.x += fetchByte(&game->video.p_data) * zoom / 64;
		po.y += fetchByte(&game->video.p_data) * zoom / 64;
		uint16_t color = 0xFF;
		if (offset & 0x8000) {
			color = fetchByte(&game->video.p_data);
			const int num = fetchByte(&game->video.p_data);
			if (Graphics::_is1991) {
				if ((color & 0x80) != 0) {
					_state->_graphics.drawSprite(game->video.buffers[0], num, &po, color & 0x7F);
					continue;
				}
			}
			color &= 0x7F;
		}
		offset <<= 1;
		uint8_t *bak = game->video.p_data.pc;
		game->video.p_data.pc = game->video.data_buf + offset;
		_game_video_draw_shape(game, color, zoom, &po);
		game->video.p_data.pc = bak;
	}
}

static void _game_video_draw_shape(game_t* game, uint8_t color, uint16_t zoom, const Point *pt) {
    uint8_t i = fetchByte(&game->video.p_data);
	if (i >= 0xC0) {
		if (color & 0x80) {
			color = i & 0x3F;
		}
		_game_video_fill_polygon(game, color, zoom, pt);
	} else {
		i &= 0x3F;
		if (i == 1) {
			warning("Video::drawShape() ec=0x%X (i != 2)", 0xF80);
		} else if (i == 2) {
			_game_video_draw_shape_parts(game, zoom, pt);
		} else {
			warning("Video::drawShape() ec=0x%X (i != 2)", 0xFBB);
		}
	}
}

static void _game_video_init(game_t* game) {
	game->video.next_pal = game->video.current_pal = 0xFF;
	game->video.buffers[2] = _game_video_get_page_ptr(game, 1);
	game->video.buffers[1] = _game_video_get_page_ptr(game, 2);
	_game_video_set_work_page_ptr(game, 0xfe);
}

static void _decode_amiga(const uint8_t *src, uint8_t *dst) {
	static const int plane_size = GFX_H * GFX_W / 8;
	for (int y = 0; y < GFX_H; ++y) {
		for (int x = 0; x < GFX_W; x += 8) {
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

static void _decode_atari(const uint8_t *src, uint8_t *dst) {
	for (int y = 0; y < GFX_H; ++y) {
		for (int x = 0; x < GFX_W; x += 16) {
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

static void _yflip(const uint8_t *src, int w, int h, uint8_t *dst) {
	for (int y = 0; y < h; ++y) {
		memcpy(dst + (h - 1 - y) * w, src, w);
		src += w;
	}
}

static void _game_video_scale_bitmap(game_t* game, const uint8_t *src, int fmt) {
    _state->_graphics.drawBitmap(game->video.buffers[0], src, GFX_H, GFX_H, fmt);
}

static void _game_video_copy_bitmap_ptr(game_t* game, const uint8_t *src, uint32_t size) {
	if (game->res.data_type == DT_DOS || game->res.data_type == DT_AMIGA) {
		_decode_amiga(src, game->video.temp_bitmap);
		_game_video_scale_bitmap(game, game->video.temp_bitmap, FMT_CLUT);
	} else if (game->res.data_type == DT_ATARI) {
		_decode_atari(src, game->video.temp_bitmap);
		_game_video_scale_bitmap(game, game->video.temp_bitmap, FMT_CLUT);
	} else { // .BMP
		if (Graphics::_is1991) {
			const int w = READ_LE_UINT32(src + 0x12);
			const int h = READ_LE_UINT32(src + 0x16);
			if (w == GFX_W && h == GFX_H) {
				const uint8_t *data = src + READ_LE_UINT32(src + 0xA);
				_yflip(data, w, h, game->video.temp_bitmap);
				_game_video_scale_bitmap(game, game->video.temp_bitmap, FMT_CLUT);
			}
		} else {
			int w, h;
			uint8_t *buf = decode_bitmap(src, &w, &h);
			if (buf) {
				_state->_graphics.drawBitmap(game->video.buffers[0], buf, w, h, FMT_RGB);
				free(buf);
			}
		}
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

// Res

static const char* _game_res_get_game_title(game_t* game) {
	switch (game->res.data_type) {
	case DT_DOS:
		if (game->res.lang == LANG_US) {
			return _kGameTitleUS;
		}
		/* fall-through */
	default:
		break;
	}
	return _kGameTitleEU;
}

static void _game_res_read_entries(game_t* game) {
	switch (game->res.data_type) {
    // TODO:
	// case DT_AMIGA:
	// case DT_ATARI:
	// 	assert(game->res.amigaMemList);
	// 	readEntriesAmiga(game->res.amigaMemList, ENTRIES_COUNT);
	// 	return;
	case DT_DOS: {
			game->res.has_password_screen = false; // DOS demo versions do not have the resources
			File f;
			// if (f.open("demo01", _dataDir)) {
			// 	_bankPrefix = "demo";
			// }
            {
				game_mem_entry_t *me = game->res.mem_list;
                f.setBuffer(dump_MEMLIST_BIN, sizeof(dump_MEMLIST_BIN));
				while (1) {
					assert(game->res.num_mem_list < ARRAYSIZE(game->res.mem_list));
					me->status = f.readByte();
					me->type = f.readByte();
					me->bufPtr = 0; f.readUint32BE();
					me->rankNum = f.readByte();
					me->bankNum = f.readByte();
					me->bankPos = f.readUint32BE();
					me->packedSize = f.readUint32BE();
					me->unpackedSize = f.readUint32BE();
					if (me->status == 0xFF) {
						game->res.has_password_screen = false; // dump_BANK09 != NULL;
						return;
					}
					++game->res.num_mem_list;
					++me;
				}
			}
		}
		break;
    // TODO:
	// case DT_ATARI_DEMO: {
	// 		File f;
	// 		if (f.open(atariDemo, _dataDir)) {
	// 			static const struct {
	// 				uint8_t type;
	// 				uint8_t num;
	// 				uint32_t offset;
	// 				uint16_t size;
	// 			} data[] = {
	// 				{ RT_SHAPE, 0x19, 0x50f0, 65146 },
	// 				{ RT_PALETTE, 0x17, 0x14f6a, 2048 },
	// 				{ RT_BYTECODE, 0x18, 0x1576a, 8368 }
	// 			};
	// 			_numMemList = ENTRIES_COUNT;
	// 			for (int i = 0; i < 3; ++i) {
	// 				MemEntry *entry = &_memList[data[i].num];
	// 				entry->type = data[i].type;
	// 				entry->bankNum = 15;
	// 				entry->bankPos = data[i].offset;
	// 				entry->packedSize = entry->unpackedSize = data[i].size;
	// 			}
	// 			return;
	// 		}
	// 	}
	// 	break;
	}
	error("No data files found");
}

static void _game_res_invalidate(game_t* game) {
	for (int i = 0; i < game->res.num_mem_list; ++i) {
		game_mem_entry_t *me = &game->res.mem_list[i];
		if (me->type <= 2 || me->type > 6) {
			me->status = GAME_RES_STATUS_NULL;
		}
	}
	game->res.script_cur_ptr = game->res.script_bak_ptr;
	game->video.current_pal = 0xFF;
}

static bool _game_res_read_bank(game_t* game, const game_mem_entry_t *me, uint8_t *dstBuf) {
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

static void _game_res_invalidate_all(game_t* game) {
	for (int i = 0; i < game->res.num_mem_list; ++i) {
		game->res.mem_list[i].status = GAME_RES_STATUS_NULL;
	}
	game->res.script_cur_ptr = game->res.mem;
	game->video.current_pal = 0xFF;
}

static void _game_res_load(game_t* game) {
	while (1) {
		game_mem_entry_t *me = 0;

		// get resource with max rankNum
		uint8_t maxNum = 0;
		for (int i = 0; i < game->res.num_mem_list; ++i) {
			game_mem_entry_t *it = &game->res.mem_list[i];
			if (it->status == GAME_RES_STATUS_TOLOAD && maxNum <= it->rankNum) {
				maxNum = it->rankNum;
				me = it;
			}
		}
		if (me == 0) {
			break; // no entry found
		}

		const int resourceNum = me - game->res.mem_list;

		uint8_t *memPtr = 0;
		if (me->type == RT_BITMAP) {
			memPtr = game->res.vid_cur_ptr;
		} else {
			memPtr = game->res.script_cur_ptr;
			const uint32_t avail = uint32_t(game->res.vid_cur_ptr - game->res.script_cur_ptr);
			if (me->unpackedSize > avail) {
				warning("Resource::load() not enough memory, available=%d", avail);
				me->status = GAME_RES_STATUS_NULL;
				continue;
			}
		}
		if (me->bankNum == 0) {
			warning("Resource::load() ec=0x%X (me->bankNum == 0)", 0xF00);
			me->status = GAME_RES_STATUS_NULL;
		} else {
			debug(DBG_BANK, "Resource::load() bufPos=0x%X size=%d type=%d pos=0x%X bankNum=%d", memPtr - game->res.mem, me->packedSize, me->type, me->bankPos, me->bankNum);
			if (_game_res_read_bank(game, me, memPtr)) {
				if (me->type == RT_BITMAP) {
					_game_video_copy_bitmap_ptr(game, game->res.vid_cur_ptr, me->unpackedSize);
					me->status = GAME_RES_STATUS_NULL;
				} else {
					me->bufPtr = memPtr;
					me->status = GAME_RES_STATUS_LOADED;
					game->res.script_cur_ptr += me->unpackedSize;
				}
			} else {
				if (game->res.data_type == DT_DOS && me->bankNum == 12 && me->type == RT_BANK) {
					// DOS demo version does not have the bank for this resource
					// this should be safe to ignore as the resource does not appear to be used by the game code
					me->status = GAME_RES_STATUS_NULL;
					continue;
				}
				error("Unable to read resource %d from bank %d", resourceNum, me->bankNum);
			}
		}
	}
}

static void _game_res_update(game_t* game, uint16_t num) {
	if (num > 16000) {
		game->res.next_part = num;
		return;
	}

    game_mem_entry_t *me = &game->res.mem_list[num];
    if (me->status == GAME_RES_STATUS_NULL) {
        me->status = GAME_RES_STATUS_TOLOAD;
        _game_res_load(game);
    }
}

static void _game_res_setup_part(game_t* game, int ptrId) {
    if (ptrId != game->res.current_part) {
        uint8_t ipal = 0;
        uint8_t icod = 0;
        uint8_t ivd1 = 0;
        uint8_t ivd2 = 0;
        if (ptrId >= 16000 && ptrId <= 16009) {
            uint16_t part = ptrId - 16000;
            ipal = _mem_list_parts[part][0];
            icod = _mem_list_parts[part][1];
            ivd1 = _mem_list_parts[part][2];
            ivd2 = _mem_list_parts[part][3];
        } else {
            error("Resource::setupPart() ec=0x%X invalid part", 0xF07);
        }
        _game_res_invalidate_all(game);
        game->res.mem_list[ipal].status = GAME_RES_STATUS_TOLOAD;
        game->res.mem_list[icod].status = GAME_RES_STATUS_TOLOAD;
        game->res.mem_list[ivd1].status = GAME_RES_STATUS_TOLOAD;
        if (ivd2 != 0) {
            game->res.mem_list[ivd2].status = GAME_RES_STATUS_TOLOAD;
        }
        _game_res_load(game);
        game->res.seg_video_pal = game->res.mem_list[ipal].bufPtr;
        game->res.seg_code = game->res.mem_list[icod].bufPtr;
        game->res.seg_video1 = game->res.mem_list[ivd1].bufPtr;
        if (ivd2 != 0) {
            game->res.seg_video2 = game->res.mem_list[ivd2].bufPtr;
        }
        game->res.current_part = ptrId;
    }
    game->res.script_bak_ptr = game->res.script_cur_ptr;
}

static void _game_res_detect_version(game_t* game) {
	//File f;
	// if (f.open("memlist.bin", _dataDir)) {
	game->res.data_type = DT_DOS;
	debug(DBG_INFO, "Using DOS data files");
	// } else if ((_amigaMemList = detectAmigaAtari(f, _dataDir)) != 0) {
	// 	if (_amigaMemList == _memListAtariEN) {
	// 		game->res.data_type = DT_ATARI;
	// 		debug(DBG_INFO, "Using Atari data files");
	// 	} else {
	// 		game->res.data_type = DT_AMIGA;
	// 		debug(DBG_INFO, "Using Amiga data files");
	// 	}
	// } else if (f.open(atariDemo, _dataDir) && f.size() == 96513) {
	// 	game->res.data_type = DT_ATARI_DEMO;
	// 	debug(DBG_INFO, "Using Atari demo file");
	// } else {
	// 	error("No data files found in '%s'", _dataDir);
	// }
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
	switch (game->res.data_type) {
	case DT_AMIGA:
	case DT_ATARI:
	case DT_ATARI_DEMO:
	case DT_DOS: {
			game_mem_entry_t *me = &game->res.mem_list[resNum];
			if (me->status == GAME_RES_STATUS_LOADED) {
                _game_audio_play_sound_raw(game, channel, me->bufPtr, _get_sound_freq(freq), vol);
			}
		}
		break;
	}
}

static void _op_add_const(game_t* game) {
	if (game->res.data_type == DT_DOS || game->res.data_type == DT_AMIGA || game->res.data_type == DT_ATARI) {
		if (game->res.current_part == 16006 && game->vm.ptr.pc == game->res.seg_code + 0x6D48) {
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
	game->vm.stack_calls[game->vm.stack_ptr] = game->vm.ptr.pc - game->res.seg_code;
	++game->vm.stack_ptr;
	game->vm.ptr.pc = game->res.seg_code + off;
}

static void _op_ret(game_t* game) {
	debug(DBG_SCRIPT, "Script::op_ret()");
	if (game->vm.stack_ptr == 0) {
		error("Script::op_ret() ec=0x%X stack underflow", 0x8F);
	}
	--game->vm.stack_ptr;
	game->vm.ptr.pc = game->res.seg_code + game->vm.stack_calls[game->vm.stack_ptr];
}

static void _op_yieldTask(game_t* game) {
	debug(DBG_SCRIPT, "Script::op_yieldTask()");
	game->vm.paused = true;
}

static void _op_jmp(game_t* game) {
	uint16_t off = fetchWord(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_jmp(0x%02X)", off);
	game->vm.ptr.pc = game->res.seg_code + off;
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

static void _fixUpPalette_changeScreen(game_t* game, int part, int screen) {
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
		_game_video_change_pal(game, pal);
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
		if (game->res.current_part == kPartCopyProtection) {
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
			_fixUpPalette_changeScreen(game, game->res.current_part, game->vm.vars[VAR_SCREEN_NUM]);
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
	if (_state->_graphics._fixUpPalette == FIXUP_PALETTE_REDRAW) {
		if (game->res.current_part == 16001) {
			if (num == 10 || num == 16) {
				return;
			}
		}
		game->video.next_pal = num;
	} else {
		game->video.next_pal = num;
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
	_game_video_set_work_page_ptr(game, i);
}

static void _op_fillPage(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint8_t color = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_fillPage(%d, %d)", i, color);
	_game_video_fill_page(game, i, color);
}

static void _op_copyPage(game_t* game) {
	uint8_t i = fetchByte(&game->vm.ptr);
	uint8_t j = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_copyPage(%d, %d)", i, j);
    _game_video_copy_page(game, i, j, game->vm.vars[VAR_SCROLL_Y]);
}

static void _inp_handleSpecialKeys(game_t* game) {
	if (_state->_sys._pi.pause) {
		if (game->res.current_part != kPartCopyProtection && game->res.current_part != kPartIntro) {
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
		if (game->res.has_password_screen) {
			if (game->res.current_part != kPartPassword && game->res.current_part != kPartCopyProtection) {
				game->res.next_part = kPartPassword;
			}
		}
	}
}

static void _op_updateDisplay(game_t* game) {
	uint8_t page = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_updateDisplay(%d)", page);
	_inp_handleSpecialKeys(game);

#ifndef BYPASS_PROTECTION
	// entered protection symbols match the expected values
	if (game->res.current_part == kPartCopyProtection && game->vm.vars[0x67] == 1) {
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

	_game_video_update_display(game, page, &_state->_sys);
}

static void _op_removeTask(game_t* game) {
	debug(DBG_SCRIPT, "Script::op_removeTask()");
	game->vm.ptr.pc = game->res.seg_code + 0xFFFF;
	game->vm.paused = true;
}

static void _op_drawString(game_t* game) {
	uint16_t strId = fetchWord(&game->vm.ptr);
	uint16_t x = fetchByte(&game->vm.ptr);
	uint16_t y = fetchByte(&game->vm.ptr);
	uint16_t col = fetchByte(&game->vm.ptr);
	debug(DBG_SCRIPT, "Script::op_drawString(0x%03X, %d, %d, %d)", strId, x, y, col);
	_game_video_draw_string(game, col, x, y, strId);
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
		_game_res_invalidate(game);
	} else {
		_game_res_update(game, num);
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
	if (game->res.data_type == DT_DOS && part == kPartCopyProtection) {
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
		const bool awTitleScreen = (game->video.strings_table == _strings_table_fr);
		game->vm.vars[0x54] = awTitleScreen ? 0x1 : 0x81;
	}
	_game_res_setup_part(game, part);
	memset(game->vm.tasks, 0xFF, sizeof(game->vm.tasks));
	memset(game->vm.states, 0, sizeof(game->vm.states));
	game->vm.tasks[0][0] = 0;
	game->vm.screen_num = -1;
	if (pos >= 0) {
		game->vm.vars[0] = pos;
	}
	game->vm.start_time = game->vm.time_stamp = _state->_sys.getTimeStamp();
	if (part == kPartWater) {
        // TODO:
		// if (game->res._demo3Joy.start()) {
		// 	memset(game->vm.vars, 0, sizeof(game->vm.vars));
		// }
	}
}

static void _game_vm_setup_tasks(game_t* game) {
	if (game->res.next_part != 0) {
        _game_vm_restart_at(game, game->res.next_part, -1);
		game->res.next_part = 0;
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
	if (game->res.current_part == kPartPassword) {
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
	if (!(game->res.data_type == DT_AMIGA || game->res.data_type == DT_ATARI)) {
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
	if (game->res.current_part == kPartWater) {
        // TODO:
		// const uint8_t mask = game->res._demo3Joy.update();
		// if (mask != 0) {
		// 	game->vm.vars[VAR_HERO_ACTION_POS_MASK] = mask;
		// 	game->vm.vars[VAR_HERO_POS_MASK] = mask & 15;
		// 	game->vm.vars[VAR_HERO_POS_LEFT_RIGHT] = 0;
		// 	if (mask & 1) {
		// 		game->vm.vars[VAR_HERO_POS_LEFT_RIGHT] = 1;
		// 	}
		// 	if (mask & 2) {
		// 		game->vm.vars[VAR_HERO_POS_LEFT_RIGHT] = -1;
		// 	}
		// 	game->vm.vars[VAR_HERO_POS_JUMP_DOWN] = 0;
		// 	if (mask & 4) {
		// 		game->vm.vars[VAR_HERO_POS_JUMP_DOWN] = 1;
		// 	}
		// 	if (mask & 8) {
		// 		game->vm.vars[VAR_HERO_POS_JUMP_DOWN] = -1;
		// 	}
		// 	game->vm.vars[VAR_HERO_ACTION] = (mask >> 7);
		// }
	}
}

static void _game_vm_execute_task(game_t* game) {
	while (!game->vm.paused) {
		uint8_t opcode = fetchByte(&game->vm.ptr);
		if (opcode & 0x80) {
			const uint16_t off = ((opcode << 8) | fetchByte(&game->vm.ptr)) << 1;
			game->res.use_seg_video2 = false;
			Point pt;
			pt.x = fetchByte(&game->vm.ptr);
			pt.y = fetchByte(&game->vm.ptr);
			int16_t h = pt.y - 199;
			if (h > 0) {
				pt.y = 199;
				pt.x += h;
			}
			debug(DBG_VIDEO, "vid_opcd_0x80 : opcode=0x%X off=0x%X x=%d y=%d", opcode, off, pt.x, pt.y);
			_game_video_set_data_buffer(game, game->res.seg_video1, off);
			_game_video_draw_shape(game, 0xFF, 64, &pt);
		} else if (opcode & 0x40) {
			Point pt;
			const uint8_t offsetHi = fetchByte(&game->vm.ptr);
			const uint16_t off = ((offsetHi << 8) | fetchByte(&game->vm.ptr)) << 1;
			pt.x = fetchByte(&game->vm.ptr);
			game->res.use_seg_video2 = false;
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
					game->res.use_seg_video2 = true;
				} else {
					zoom = fetchByte(&game->vm.ptr);
				}
			}
			debug(DBG_VIDEO, "vid_opcd_0x40 : off=0x%X x=%d y=%d", off, pt.x, pt.y);
			_game_video_set_data_buffer(game, game->res.use_seg_video2 ? game->res.seg_video2 : game->res.seg_video1, off);
			_game_video_draw_shape(game, 0xFF, zoom, &pt);
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
				game->vm.ptr.pc = game->res.seg_code + n;
				game->vm.stack_ptr = 0;
				game->vm.paused = false;
				debug(DBG_SCRIPT, "Script::runTasks() i=0x%02X n=0x%02X", i, n);
                _game_vm_execute_task(game);
				game->vm.tasks[0][i] = game->vm.ptr.pc - game->res.seg_code;
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
    _state = new GameState();
    _state->_part_num = desc->part_num;
    _game_res_detect_version(game);
    _game_video_init(game);
    game->res.has_password_screen = true;
    game->res.script_bak_ptr = game->res.script_cur_ptr = game->res.mem;
    game->res.vid_cur_ptr = game->res.mem + GAME_MEM_BLOCK_SIZE - (320 * 200 / 2); // 4bpp bitmap
    game->res.lang = desc->lang;
	_game_res_read_entries(game);

	debug(DBG_INFO, "Using sokol graphics");
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
	_state->_graphics.setFont(0, 0, 0);

    // TODO:
    // if (desc->demo3_joy_inputs && game->res.data_type == DT_DOS) {
	// 	game->res.readDemo3Joy();
	// }

    _state->_sys.setBuffer(game->gfx.fb, game->gfx.palette);
    switch (game->res.data_type) {
    case DT_DOS:
    case DT_AMIGA:
    case DT_ATARI:
    case DT_ATARI_DEMO:
        switch (game->res.lang) {
        case LANG_FR:
            game->video.strings_table = _strings_table_fr;
            break;
        case LANG_US:
        default:
            game->video.strings_table = _strings_table_eng;
            break;
        }
        break;
    }
    game->audio.callback = desc->audio.callback;
    _game_audio_init(game, desc->audio.callback);

    game->video.use_ega = desc->use_ega;
    // bypass protection ?
#ifndef BYPASS_PROTECTION
        switch (game->res.data_type) {
        case Resource::DT_DOS:
            if (!game->res._hasPasswordScreen) {
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
    game->title = _game_res_get_game_title(game);
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
            game->debug.callback.func(game->debug.callback.user_data, game->vm.ptr.pc - game->res.seg_code);
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
    *res = game->res.mem_list;
}

void game_get_vars(game_t* game, int16_t** vars) {
    *vars = &game->vm.vars[0];
}

uint8_t* game_get_pc(game_t* game) {
    return game->res.seg_code;
}

bool game_get_res_buf(game_t* game, int id, uint8_t* dst) {
    game_mem_entry_t* me = &game->res.mem_list[id];
    return _game_res_read_bank(game, me, dst);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
