/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include <SDL.h>
#include <stdint.h>

int
custom_ui_init(void);

void
custom_ui_shutdown(void);

void
custom_ui_toggle(void);

int
custom_ui_isOpen(void);

uint32_t
custom_ui_getWindowId(void);

void
custom_ui_handleEvent(SDL_Event *ev);

void
custom_ui_render(void);
