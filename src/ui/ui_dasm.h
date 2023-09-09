#pragma once
/*#
    # ui_dasm.h

    Disassembler UI using Dear ImGui.

    Do this:
    ~~~C
    #define GAME_UI_IMPL
    ~~~
    before you include this file in *one* C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

    ~~~C
    GAME_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    You need to include the following headers before including the
    *implementation*:

        - imgui.h
        - ui_util.h

    All strings provided to ui_dasm_init() must remain alive until
    ui_dasm_discard() is called!

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* callback for reading a byte from memory */
typedef uint8_t (*ui_dasm_read_t)(int layer, uint16_t addr, bool* valid, void* user_data);
typedef const char* (*ui_dasm_getstr_t)(uint16_t id, void* user_data);

#define UI_DASM_MAX_LAYERS (16)
#define UI_DASM_MAX_STRLEN (32*4)
#define UI_DASM_MAX_BINLEN (16)
#define UI_DASM_NUM_LINES  (4096)
#define UI_DASM_MAX_STACK  (128)

/* setup parameters for ui_dasm_init()

    NOTE: all strings must be static!
*/
typedef struct {
    const char* title;  /* window title */
    const char* layers[UI_DASM_MAX_LAYERS];   /* memory system layer names */
    uint16_t start_addr;
    ui_dasm_read_t read_cb;
    ui_dasm_getstr_t getstr_cb;
    void* user_data;
    int x, y;           /* initial window pos */
    int w, h;           /* initial window size or 0 for default size */
    bool open;          /* initial open state */
} ui_dasm_desc_t;

typedef struct {
    const char* title;
    ui_dasm_read_t read_cb;
    ui_dasm_getstr_t getstr_cb;
    int cur_layer;
    int num_layers;
    const char* layers[UI_DASM_MAX_LAYERS];
    void* user_data;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool valid;
    uint16_t start_addr;
    uint16_t cur_addr;
    int str_pos;
    char str_buf[UI_DASM_MAX_STRLEN];
    int bin_pos;
    uint8_t bin_buf[UI_DASM_MAX_BINLEN];
    int stack_num;
    int stack_pos;
    uint16_t stack[UI_DASM_MAX_STACK];
    uint16_t highlight_addr;
    uint32_t highlight_color;
} ui_dasm_t;

void ui_dasm_init(ui_dasm_t* win, const ui_dasm_desc_t* desc);
void ui_dasm_discard(ui_dasm_t* win);
void ui_dasm_draw(ui_dasm_t* win);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION (include in C++ source) ----------------------------------*/
#ifdef GAME_UI_IMPL
#ifndef __cplusplus
#error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#include <stdio.h>  /* sscanf, sprintf (ImGui memory editor) */
#ifndef GAME_ASSERT
    #include <assert.h>
    #define GAME_ASSERT(c) assert(c)
#endif

void ui_dasm_init(ui_dasm_t* win, const ui_dasm_desc_t* desc) {
    GAME_ASSERT(win && desc);
    GAME_ASSERT(desc->title);
    memset(win, 0, sizeof(ui_dasm_t));
    win->title = desc->title;
    win->read_cb = desc->read_cb;
    win->getstr_cb = desc->getstr_cb;
    win->start_addr = desc->start_addr;
    win->user_data = desc->user_data;
    win->init_x = (float) desc->x;
    win->init_y = (float) desc->y;
    win->init_w = (float) ((desc->w == 0) ? 400 : desc->w);
    win->init_h = (float) ((desc->h == 0) ? 256 : desc->h);
    win->open = desc->open;
    win->highlight_color = 0xFF30FF30;
    for (int i = 0; i < UI_DASM_MAX_LAYERS; i++) {
        if (desc->layers[i]) {
            win->num_layers++;
            win->layers[i] = desc->layers[i];
        }
        else {
            break;
        }
    }
    win->valid = true;
}

void ui_dasm_discard(ui_dasm_t* win) {
    GAME_ASSERT(win && win->valid);
    win->valid = false;
}

/* disassembler callback to fetch the next instruction byte */
static uint8_t _ui_dasm_in_cb(void* user_data) {
    ui_dasm_t* win = (ui_dasm_t*) user_data;
    bool valid;
    uint8_t val = win->read_cb(win->cur_layer, win->cur_addr++, &valid, win->user_data);
    if (!valid) return 0;
    if (win->bin_pos < UI_DASM_MAX_BINLEN) {
        win->bin_buf[win->bin_pos++] = val;
    }
    return val;
}

/* disassembler callback to output a character */
static void _ui_dasm_out_cb(char c, void* user_data) {
    ui_dasm_t* win = (ui_dasm_t*) user_data;
    if ((win->str_pos+1) < UI_DASM_MAX_STRLEN) {
        win->str_buf[win->str_pos++] = c;
        win->str_buf[win->str_pos] = 0;
    }
}

static const char* _ui_dasm_getstr_cb(uint16_t id, void* user_data) {
    ui_dasm_t* win = (ui_dasm_t*) user_data;
    return win->getstr_cb(id, win->user_data);
}

/* disassemble the next instruction */
static void _ui_dasm_disasm(ui_dasm_t* win) {
    win->str_pos = 0;
    win->bin_pos = 0;
    raw_dasm_op(win->cur_addr, _ui_dasm_in_cb, _ui_dasm_out_cb, _ui_dasm_getstr_cb, win);
}

static bool _ui_dasm_jumptarget(ui_dasm_t* win, uint16_t pc, uint16_t* out_addr) {
    (void)pc;
    switch (win->bin_buf[0]) {
        case 0x04: // jsr
        case 0x07: // jmp
            *out_addr = win->bin_buf[2] | win->bin_buf[1] << 8;
            return true;
        case 0x08: // setvec
            *out_addr = win->bin_buf[3] | win->bin_buf[2]  << 8;
            return true;
        case 0x09: // if var
            *out_addr = win->bin_buf[3] | win->bin_buf[2]  << 8;
            return true;
        case 0x0a: // if exp
            const uint8_t op = win->bin_buf[1];
            const uint8_t off = (op & 0x40) ? 5 : 4;
            *out_addr = win->bin_buf[off+1] | win->bin_buf[off] << 8;
            return true;
    }
    return false;
}

/* push an address on the bookmark stack */
static void _ui_dasm_stack_push(ui_dasm_t* win, uint16_t addr) {
    if (win->stack_num < UI_DASM_MAX_STACK) {
        /* ignore if the same address is already on top of stack */
        if ((win->stack_num > 0) && (addr == win->stack[win->stack_num-1])) {
            return;
        }
        win->stack_pos = win->stack_num;
        win->stack[win->stack_num++] = addr;
    }
}

/* return current address on stack, and set pos to previous */
static bool _ui_dasm_stack_back(ui_dasm_t* win, uint16_t* addr) {
    if (win->stack_num > 0) {
        *addr = win->stack[win->stack_pos];
        if (win->stack_pos > 0) {
            win->stack_pos--;
        }
        return true;
    }
    *addr = 0;
    return false;
}

/* goto to address, op address on stack */
static void _ui_dasm_goto(ui_dasm_t* win, uint16_t addr) {
    win->start_addr = addr;
}

/* draw the address entry field and layer combo */
static void _ui_dasm_draw_controls(ui_dasm_t* win) {
    win->start_addr = ui_util_input_u16("##addr", win->start_addr);
    ImGui::SameLine();
    uint16_t addr = 0;
    if (ImGui::ArrowButton("##back", ImGuiDir_Left)) {
        if (_ui_dasm_stack_back(win, &addr)) {
            _ui_dasm_goto(win, addr);
        }
    }
    if (ImGui::IsItemHovered() && (win->stack_num > 0)) {
        ImGui::SetTooltip("Goto %04X", win->stack[win->stack_pos]);
    }
    ImGui::SameLine();
    ImGui::SameLine();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::Combo("##layer", &win->cur_layer, win->layers, win->num_layers);
    ImGui::PopItemWidth();
}

/* draw the disassembly column */
static void _ui_dasm_draw_disasm(ui_dasm_t* win) {
    ImGui::BeginChild("##dasmbox", ImVec2(0, 0), true);
    _ui_dasm_draw_controls(win);

    ImGui::BeginChild("##dasm", ImVec2(0, 0), false);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
    const float line_height = ImGui::GetTextLineHeight();
    const float glyph_width = ImGui::CalcTextSize("F").x;
    const float cell_width = 3 * glyph_width;
    ImGuiListClipper clipper;
    clipper.Begin(UI_DASM_NUM_LINES, line_height);
    clipper.Step();

    /* skip hidden lines */
    win->cur_addr = win->start_addr;
    for (int line_i = 0; (line_i < clipper.DisplayStart) && (line_i < UI_DASM_NUM_LINES); line_i++) {
        _ui_dasm_disasm(win);
    }

    /* visible items */
    for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) {
        const uint16_t op_addr = win->cur_addr;
        _ui_dasm_disasm(win);
        const int num_bytes = win->bin_pos;
        if(!num_bytes) break;

        /* highlight current hovered address */
        bool highlight = false;
        if (win->highlight_addr == op_addr) {
            ImGui::PushStyleColor(ImGuiCol_Text, win->highlight_color);
            highlight = true;
        }

        /* address */
        ImGui::Text("%04X: ", op_addr);
        ImGui::SameLine();

        /* instruction bytes */
        const float line_start_x = ImGui::GetCursorPosX();
        for (int n = 0; n < num_bytes; n++) {
            ImGui::SameLine(line_start_x + cell_width * n);
            ImGui::Text("%02X ", win->bin_buf[n]);
        }

        /* disassembled instruction */
        ImGui::SameLine(line_start_x + cell_width*6 + glyph_width*2);
        ImGui::Text("%s", win->str_buf);

        if (highlight) {
            ImGui::PopStyleColor();
        }

        /* check for jump instruction and draw an arrow  */
        uint16_t jump_addr = 0;
        if (_ui_dasm_jumptarget(win, win->cur_addr, &jump_addr)) {
            ImGui::SameLine(line_start_x + cell_width*8 + glyph_width*2 + glyph_width*20);
            ImGui::PushID(line_i);
            if (ImGui::ArrowButton("##btn", ImGuiDir_Right)) {
                ImGui::SetScrollY(0);
                _ui_dasm_goto(win, jump_addr);
                _ui_dasm_stack_push(win, op_addr);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Goto %04X", jump_addr);
                win->highlight_addr = jump_addr;
            }
            ImGui::PopID();
        }
    }
    clipper.End();
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::EndChild();
}

/* draw the stack */
static void _ui_dasm_draw_stack(ui_dasm_t* win) {
    ImGui::BeginChild("##stackbox", ImVec2(72, 0), true);
    if (ImGui::Button("Clear")) {
        win->stack_num = 0;
    }
    char buf[5] = { 0 };
    if (ImGui::BeginListBox("##stack", ImVec2(-1,-1))) {
        for (int i = 0; i < win->stack_num; i++) {
            snprintf(buf, sizeof(buf), "%04X", win->stack[i]);
            ImGui::PushID(i);
            if (ImGui::Selectable(buf, i == win->stack_pos)) {
                win->stack_pos = i;
                _ui_dasm_goto(win, win->stack[i]);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Goto %04X", win->stack[i]);
                win->highlight_addr = win->stack[i];
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    ImGui::EndChild();
}

void ui_dasm_draw(ui_dasm_t* win) {
    GAME_ASSERT(win && win->valid && win->title);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_Once);
    if (ImGui::Begin(win->title, &win->open)) {
        _ui_dasm_draw_stack(win);
        ImGui::SameLine();
        _ui_dasm_draw_disasm(win);
    }
    ImGui::End();
}
#endif /* GAME_UI_IMPL */
