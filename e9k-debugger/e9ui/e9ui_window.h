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
    e9ui_window_backend_overlay = 0
} e9ui_window_backend_t;

typedef struct e9ui_window e9ui_window_t;

typedef void (*e9ui_window_close_cb_t)(e9ui_window_t *window, void *user);

e9ui_window_t *
e9ui_windowCreate(e9ui_window_backend_t backend);

void
e9ui_windowDestroy(e9ui_window_t *window);

int
e9ui_windowOpen(e9ui_window_t *window,
                const char *title,
                e9ui_rect_t rect,
                e9ui_component_t *body,
                e9ui_window_close_cb_t onClose,
                void *onCloseUser,
                e9ui_context_t *ctx);

void
e9ui_windowClose(e9ui_window_t *window);

void
e9ui_windowCloseAllOverlay(void);

int
e9ui_windowIsOpen(const e9ui_window_t *window);

e9ui_rect_t
e9ui_windowGetRect(const e9ui_window_t *window);

int
e9ui_windowCaptureRectToInts(const e9ui_window_t *window,
                             const e9ui_context_t *ctx,
                             int *outX,
                             int *outY,
                             int *outW,
                             int *outH);

e9ui_rect_t
e9ui_windowRestoreRect(const e9ui_context_t *ctx,
                             e9ui_rect_t defaultRect,
                             int hasPos,
                             int hasSize,
                             int x,
                             int y,
                             int w,
                             int h);

void
e9ui_windowClampRectSize(e9ui_rect_t *rect,
                         const e9ui_context_t *ctx,
                         int minWidthPx,
                         int minHeightPx);

int
e9ui_windowCaptureRectChanged(e9ui_window_t *window,
                              const e9ui_context_t *ctx,
                              int *hasSaved,
                              int *x,
                              int *y,
                              int *w,
                              int *h);

int
e9ui_windowCaptureRectSnapshot(const e9ui_window_t *window,
                                  const e9ui_context_t *ctx,
                                  int *hasSaved,
                                  int *x,
                                  int *y,
                                  int *w,
                                  int *h);

e9ui_rect_t
e9ui_windowResolveOpenRect(const e9ui_context_t *ctx,
                                    e9ui_rect_t defaultRect,
                                    int minWidthPx,
                                    int minHeightPx,
                                    int centerWhenNoSaved,
                                    int hasPos,
                                    int hasSize,
                                    int x,
                                    int y,
                                    int w,
                                    int h);

int
e9ui_windowDispatchKeydown(e9ui_component_t *root, e9ui_context_t *ctx, const e9ui_event_t *ev);
