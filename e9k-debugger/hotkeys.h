/*
 * COPYRIGHT (C) 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include "e9ui.h"


int
hotkeys_registerHotkey(e9ui_context_t *ctx, SDL_Keycode key, SDL_Keymod modMask, SDL_Keymod modValue,
                        void (*cb)(e9ui_context_t *ctx, void *user), void *user);

void
hotkeys_unregisterHotkey(e9ui_context_t *ctx, int id);

int
hotkeys_dispatchHotkey(e9ui_context_t *ctx, const SDL_KeyboardEvent *kev);

int
hotkeys_handleKeydown(e9ui_context_t *ctx, const SDL_KeyboardEvent *kev);

void
hotkeys_shutdown(void);
