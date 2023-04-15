/*
   UI implementation for nes.c, this must live in a .cc file.
*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gfx.h"
#include "common.h"
#include "game.h"
#define GAME_UI_IMPL
#include "imgui.h"
#include "ui/ui_util.h"
#include "ui/raw-dasm.h"
#include "ui/ui_dbg.h"
#include "ui/ui_dasm.h"
#include "ui/ui_game.h"
