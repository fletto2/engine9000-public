/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "debugger_input_bindings.h"
#include "emu_st.h"
#include "e9ui.h"
#include "libretro.h"
#include "target.h"

static int
emu_st_mapKeyToJoypad(SDL_Keycode key, unsigned *id)
{
    return debugger_input_bindings_mapKeyToJoypad(TARGET_ATARIST,
                                                  (target && target->coreOptionGetValue)
                                                      ? target->coreOptionGetValue
                                                      : NULL,
                                                  key,
                                                  id);
}

static uint16_t
emu_st_translateModifiers(SDL_Keymod mod)
{
    uint16_t out = 0;
    if (mod & KMOD_SHIFT) {
        out |= RETROKMOD_SHIFT;
    }
    if (mod & KMOD_CTRL) {
        out |= RETROKMOD_CTRL;
    }
    if (mod & KMOD_ALT) {
        out |= RETROKMOD_ALT;
    }
    if (mod & KMOD_GUI) {
        out |= RETROKMOD_META;
    }
    if (mod & KMOD_NUM) {
        out |= RETROKMOD_NUMLOCK;
    }
    if (mod & KMOD_CAPS) {
        out |= RETROKMOD_CAPSLOCK;
    }
    return out;
}

static uint32_t
emu_st_translateCharacter(SDL_Keycode key, SDL_Keymod mod)
{
    if (key < 32 || key >= 127) {
        return 0;
    }
    int shift = (mod & KMOD_SHIFT) ? 1 : 0;
    int caps = (mod & KMOD_CAPS) ? 1 : 0;
    if (key >= 'a' && key <= 'z') {
        if (shift ^ caps) {
            return (uint32_t)toupper((int)key);
        }
        return (uint32_t)key;
    }
    if (!shift) {
        return (uint32_t)key;
    }
    switch (key) {
    case '1': return '!';
    case '2': return '@';
    case '3': return '#';
    case '4': return '$';
    case '5': return '%';
    case '6': return '^';
    case '7': return '&';
    case '8': return '*';
    case '9': return '(';
    case '0': return ')';
    case '-': return '_';
    case '=': return '+';
    case '[': return '{';
    case ']': return '}';
    case '\\': return '|';
    case ';': return ':';
    case '\'': return '"';
    case ',': return '<';
    case '.': return '>';
    case '/': return '?';
    case '`': return '~';
    default:
        break;
    }
    return (uint32_t)key;
}

static unsigned
emu_st_translateKey(SDL_Keycode key)
{
    if (key >= 32 && key < 127) {
        if (key >= 'A' && key <= 'Z') {
            return (unsigned)tolower((int)key);
        }
        return (unsigned)key;
    }
    switch (key) {
    case SDLK_BACKSPACE: return RETROK_BACKSPACE;
    case SDLK_TAB: return RETROK_TAB;
    case SDLK_RETURN: return RETROK_RETURN;
    case SDLK_ESCAPE: return RETROK_ESCAPE;
    case SDLK_DELETE: return RETROK_DELETE;
    case SDLK_INSERT: return RETROK_INSERT;
    case SDLK_HOME: return RETROK_HOME;
    case SDLK_END: return RETROK_END;
    case SDLK_PAGEUP: return RETROK_PAGEUP;
    case SDLK_PAGEDOWN: return RETROK_PAGEDOWN;
    case SDLK_UP: return RETROK_UP;
    case SDLK_DOWN: return RETROK_DOWN;
    case SDLK_LEFT: return RETROK_LEFT;
    case SDLK_RIGHT: return RETROK_RIGHT;
    case SDLK_F1: return RETROK_F1;
    case SDLK_F2: return RETROK_F2;
    case SDLK_F3: return RETROK_F3;
    case SDLK_F4: return RETROK_F4;
    case SDLK_F5: return RETROK_F5;
    case SDLK_F6: return RETROK_F6;
    case SDLK_F7: return RETROK_F7;
    case SDLK_F8: return RETROK_F8;
    case SDLK_F9: return RETROK_F9;
    case SDLK_F10: return RETROK_F10;
    case SDLK_F11: return RETROK_F11;
    case SDLK_F12: return RETROK_F12;
    case SDLK_LSHIFT: return RETROK_LSHIFT;
    case SDLK_RSHIFT: return RETROK_RSHIFT;
    case SDLK_LCTRL: return RETROK_LCTRL;
    case SDLK_RCTRL: return RETROK_RCTRL;
    case SDLK_LALT: return RETROK_LALT;
    case SDLK_RALT: return RETROK_RALT;
    case SDLK_LGUI: return RETROK_LMETA;
    case SDLK_RGUI: return RETROK_RMETA;
    default:
        break;
    }
    return RETROK_UNKNOWN;
}

const emu_system_iface_t emu_st_iface = {
    .translateCharacter = emu_st_translateCharacter,
    .translateModifiers = emu_st_translateModifiers,
    .translateKey = emu_st_translateKey,
    .mapKeyToJoypad = emu_st_mapKeyToJoypad,
    .createOverlays = NULL,
    .render = NULL,
    .destroy = NULL,
};
