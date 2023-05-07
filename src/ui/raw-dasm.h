#pragma once
/*#
    # raw-dasm.h

    ## zlib/libpng license

    Copyright (c) 2023 Scemino
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

/* the input callback type */
typedef uint8_t (*raw_dasm_input_t)(void* user_data);
/* the output callback type */
typedef void (*raw_dasm_output_t)(char c, void* user_data);

/* disassemble a single Another World instruction into a stream of ASCII characters */
uint16_t raw_dasm_op(uint16_t pc, raw_dasm_input_t in_cb, raw_dasm_output_t out_cb, void* user_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef GAME_UI_IMPL
#ifndef GAME_ASSERT
    #include <assert.h>
    #define GAME_ASSERT(c) assert(c)
#endif

/* fetch unsigned 8-bit value and track pc */
#ifdef _FETCH_U8
#undef _FETCH_U8
#endif
#define _FETCH_U8(v) v=in_cb(user_data);pc++;
/* fetch signed 8-bit value and track pc */
#ifdef _FETCH_I8
#undef _FETCH_I8
#endif
#define _FETCH_I8(v) v=(int8_t)in_cb(user_data);pc++;
/* fetch unsigned 16-bit value and track pc */
#ifdef _FETCH_U16
#undef _FETCH_U16
#endif
#define _FETCH_U16(v) v=(in_cb(user_data))<<8;v|=in_cb(user_data);pc+=2;
/* fetch usigned 16-bit value and track pc */
#ifdef _FETCH_I16
#undef _FETCH_I16
#endif
#define _FETCH_I16(v) v=(int16_t)((in_cb(user_data)<<8)|(in_cb(user_data)));pc+=2;
/* output character */
#ifdef _CHR
#undef _CHR
#endif
#define _CHR(c) if (out_cb) { out_cb(c,user_data); }
/* output string */
#ifdef _STR
#undef _STR
#endif
#define _STR(s) _raw_dasm_str(s,out_cb,user_data);
/* output number as unsigned 8-bit string (hex) */
#ifdef _STR_U8
#undef _STR_U8
#endif
#define _STR_U8(u8) _raw_dasm_u8((uint8_t)(u8),out_cb,user_data);
/* output number number as unsigned 16-bit string (hex) */
#ifdef _STR_U16
#undef _STR_U16
#endif
#define _STR_U16(u16) _raw_dasm_u16((uint16_t)(u16),out_cb,user_data);

static const char* _raw_dasm_hex = "0123456789ABCDEF";

/* helper function to output string */
static void _raw_dasm_str(const char* str, raw_dasm_output_t out_cb, void* user_data) {
    if (out_cb) {
        char c;
        while (0 != (c = *str++)) {
            out_cb(c, user_data);
        }
    }
}

/* helper function to output an unsigned 8-bit value as hex string */
static void _raw_dasm_hex8(uint8_t val, raw_dasm_output_t out_cb, void* user_data) {
    if (out_cb) {
        out_cb('$', user_data);
        for (int i = 1; i >= 0; i--) {
            out_cb(_raw_dasm_hex[(val>>(i*4)) & 0xF], user_data);
        }
    }
}

static void _raw_dasm_u8(uint8_t val, raw_dasm_output_t out_cb, void* user_data) {
    if (out_cb) {
        if (val == 0) {
            out_cb(_raw_dasm_hex[0], user_data);
        } else {
            uint8_t div = 100;
            bool b = false;
            for(int i=0; i<3; i++) {
                uint8_t v = (val / div) % 10;
                if(b || (v > 0)) {
                    b = true;
                    out_cb(_raw_dasm_hex[v & 0xF], user_data);
                }
                div /= 10;
            }
        }
    }
}

/* helper function to output an unsigned 16-bit value as hex string */
static void _raw_dasm_u16(uint16_t val, raw_dasm_output_t out_cb, void* user_data) {
    if (out_cb) {
        out_cb('$', user_data);
        for (int i = 3; i >= 0; i--) {
            out_cb(_raw_dasm_hex[(val>>(i*4)) & 0xF], user_data);
        }
    }
}

/* main disassembler function */
uint16_t raw_dasm_op(uint16_t pc, raw_dasm_input_t in_cb, raw_dasm_output_t out_cb, void* user_data) {
    GAME_ASSERT(in_cb);
    uint8_t op;
    uint8_t u8; uint16_t u16; int16_t i16;
    _FETCH_U8(op);

    /* opcode name */
    switch (op) {
        case 0x00: _FETCH_U8(u8); _STR("set v"); _STR_U8(u8); _CHR(' '); _FETCH_I16(i16); _STR_U16(i16); break; // mov const
        case 0x01: _FETCH_U8(u8); _STR("seti v"); _STR_U8(u8); _STR(" v"); _FETCH_U8(u8); _STR_U8(u8); break; // mov
        case 0x02: _FETCH_U8(u8); _STR("addi "); _STR_U8(u8); _STR(" v"); _FETCH_U8(u8); _STR_U8(u8); break; // add
        case 0x03: _FETCH_U8(u8); _STR("addi "); _STR_U8(u8); _CHR(' '); _FETCH_I16(i16); _STR_U16(i16); break; // add const
        case 0x04: _FETCH_U16(u16); _STR("jsr "); _STR_U16(u16); break; // call
        case 0x05: _STR("return"); break; // ret
        case 0x06: _STR("break"); break; // yield task
        case 0x07: _FETCH_U16(u16); _STR("jmp "); _STR_U16(u16); break; // jmp
        case 0x08: _FETCH_U8(u8); _STR("setvec "); _STR_U8(u8); _CHR(' '); _FETCH_U16(u16); _STR_U16(u16); break; // install task
        case 0x09: _FETCH_U8(u8); _STR("if v"); _STR_U8(u8); _CHR(' '); _FETCH_U16(u16); _STR_U16(u16);  break; // jmpIfVar
        case 0x0a: {
            uint8_t op, v;
            _FETCH_U8(op); _FETCH_U8(v);
            _STR("if (v"); _STR_U8(v);
            switch (op & 7) {
                case 0: _STR(" == "); break;
                case 1: _STR(" != "); break;
                case 2: _STR(" > "); break;
                case 3: _STR(" >= "); break;
                case 4: _STR(" < "); break;
                case 5: _STR(" <= "); break;
                default: _STR("???"); break;
            }
            if (op & 0x80) {
                uint8_t a; _FETCH_U8(a);
                _STR("v"); _STR_U8(a);
            } else if (op & 0x40) {
                _FETCH_I16(i16); _STR_U16(i16);
            } else {
                uint8_t a;
                _FETCH_U8(a); _STR_U8(a);
            }
             _STR(") jmp ");
            _FETCH_I16(i16); _STR_U16(i16);
        } break;
        case 0x0b: _FETCH_U16(u16); _STR("fade "); _STR_U16(u16);  break; // setPalette
        case 0x0c: _FETCH_U8(u8); _STR("vec "); _STR_U8(u8); _CHR(','); _FETCH_U8(u8); _STR_U8(u8); _CHR(','); _FETCH_U8(u8); _STR_U8(u8); break; // changeTasksState
        case 0x0d: _FETCH_U8(u8); _STR("setws "); _STR_U8(u8); break; // selectPage
        case 0x0e: _FETCH_U8(u8); _STR("clr "); _STR_U8(u8); _FETCH_U8(u8); _STR_U8(u8); break; // fillPage
        case 0x0f: _FETCH_U8(u8); _STR("copy "); _STR_U8(u8); _FETCH_U8(u8); _STR_U8(u8); break; // copyPage
        case 0x10: _FETCH_U8(u8); _STR("show "); _STR_U8(u8); break; // updateDisplay
        case 0x11: _STR("bigend"); break; // removeTask
        case 0x12: _FETCH_U16(u16); _STR("text "); _STR_U16(u16); _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); break; // text "text number", x, y, color
        case 0x13: _FETCH_U8(u8); _STR("v"); _STR_U8(u8); _STR("-="); _FETCH_U8(u8); _STR("v "); _STR_U8(u8); break; // sub
        case 0x14: _FETCH_U8(u8); _STR("v"); _STR_U8(u8); _STR("&="); _FETCH_U16(u16); _STR_U16(u16); break; // and
        case 0x15: _FETCH_U8(u8); _STR("v"); _STR_U8(u8); _STR("|="); _FETCH_U16(u16); _STR_U16(u16); break; // or
        case 0x16: _FETCH_U8(u8); _STR("v"); _STR_U8(u8); _STR("<<="); _FETCH_U16(u16); _STR_U16(u16); break; // shl
        case 0x17: _FETCH_U8(u8); _STR("v"); _STR_U8(u8); _STR(">>="); _FETCH_U16(u16); _STR_U16(u16); break; // shr
        case 0x18: _FETCH_U16(u16); _STR("play "); _STR_U16(u16); _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); break; // playSound
        case 0x19: _FETCH_U16(u16); _STR("load "); _STR_U16(u16); break; // updateResource
        case 0x1a: _FETCH_U16(u16); _STR("song "); _STR_U16(u16); _CHR(' '); _FETCH_U16(u16); _STR_U16(u16); _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); break; // playMusic
        default:
            if(op & 0x80) {
                uint8_t x, y;
                _FETCH_U8(u8); _FETCH_U8(x); _FETCH_U8(y);
                _STR("spr ");  _STR_U8(x); _CHR(' '); _STR_U8(x); _STR(" "); _STR_U8(y); _CHR(' ');
            } else if(op & 0x40) {
                _FETCH_U8(u8); _FETCH_U8(u8);
                _STR("spr ");
                if (!(op & 0x20)) {
                    if (!(op & 0x10)) {
                        int16_t x;
                        _FETCH_I16(x);
                        _STR_U16(x);
                    } else {
                        _FETCH_U8(u8);
                        _STR("v"); _STR_U8(u8);
                    }
                } else {
                    _FETCH_U8(u8);
                    int16_t x = u8;
                    if (op & 0x10) {
                        x += 0x100;
                    }
                    _STR_U16(x);
                }
                _STR(" ");
                _FETCH_U8(u8);
                int16_t y = u8;
                if (!(op & 8)) {
                    if (!(op & 4)) {
                        _FETCH_U8(u8);
                        y = (y << 8) | u8;
                        _STR_U16(y);
                    } else {
                        _STR("v"); _STR_U8(u8);
                    }
                } else {
                    _STR_U16(y);
                }
                _STR(" ");
                if (!(op & 2)) {
                    if (op & 1) {
                        _FETCH_U8(u8);
                        _STR("v"); _STR_U8(u8);
                    } else {
                        _STR("64");
                    }
                } else {
                    if (!(op & 1)) {
                        _FETCH_U8(u8);
                        _STR_U8(u8);
                    } else {
                        _STR("64");
                    }
                }
            } else {
                _STR("???");
            }
    }
    return pc;
}

#undef _FETCH_I8
#undef _FETCH_U8
#undef _FETCH_I16
#undef _FETCH_U16
#undef _CHR
#undef _STR
#undef _STR_U8
#undef _STR_U16
#undef A____
#undef A_IMM
#undef A_ZER
#undef A_ZPX
#undef A_ZPY
#undef A_ABS
#undef A_ABX
#undef A_ABY
#undef A_IDX
#undef A_IDY
#undef A_JMP
#undef A_JSR
#undef A_INV
#undef M___
#undef M_R_
#undef M__W
#undef M_RW
#endif /* GAME_UTIL_IMPL */
