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
#include "e9ui_types.h"

typedef enum e9ui_window_backend
{
    e9ui_window_backend_sdl = 0,
    e9ui_window_backend_embedded = 1
} e9ui_window_backend_t;

typedef struct e9ui_window e9ui_window_t;

typedef void (*e9ui_window_close_cb_t)(e9ui_window_t *window, void *user);

e9ui_window_t *
e9ui_windowCreate(e9ui_window_backend_t backend);

void
e9ui_windowDestroy(e9ui_window_t *window);

int
e9ui_windowOpenSdl(e9ui_window_t *window,
                   const char *title,
                   int x,
                   int y,
                   int w,
                   int h,
                   uint32_t windowFlags);

int
e9ui_windowCreateSdlRenderer(e9ui_window_t *window, int rendererIndex, uint32_t rendererFlags);

int
e9ui_windowOpenEmbedded(e9ui_window_t *window,
                        const char *title,
                        e9ui_rect_t rect,
                        e9ui_component_t *body,
                        e9ui_window_close_cb_t onClose,
                        void *onCloseUser,
                        e9ui_context_t *ctx);

void
e9ui_windowClose(e9ui_window_t *window);

int
e9ui_windowIsOpen(const e9ui_window_t *window);

SDL_Window *
e9ui_windowGetSdlWindow(const e9ui_window_t *window);

SDL_Renderer *
e9ui_windowGetSdlRenderer(const e9ui_window_t *window);

uint32_t
e9ui_windowGetWindowId(const e9ui_window_t *window);

int
e9ui_windowIsEmbedded(const e9ui_window_t *window);

e9ui_rect_t
e9ui_windowGetEmbeddedRect(const e9ui_window_t *window);

int
e9ui_windowDispatchEmbeddedKeydown(e9ui_component_t *root, e9ui_context_t *ctx, const e9ui_event_t *ev);

void
e9ui_windowSetMainWindowFocused(e9ui_window_t *window, int focused);

void
e9ui_windowSetSelfFocused(e9ui_window_t *window, int focused);

void
e9ui_windowRefreshSelfFocusedFromFlags(e9ui_window_t *window);

void
e9ui_windowUpdateAlwaysOnTop(e9ui_window_t *window);
