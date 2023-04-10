/*
   UI implementation for nes.c, this must live in a .cc file.
*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gfx.h"
#include "game.h"
#include "script.h"
#define GAME_UI_IMPL
#include "imgui.h"
#define CHIPS_UI_IMPL
#include "resource.h"
#include "ui/ui_util.h"
#include "ui/ui_game.h"
