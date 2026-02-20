/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include "e9ui_component.h"
#include "e9ui_context.h"
#include "e9ui.h"
#include <stddef.h>

typedef enum range_bar_side
{
    range_bar_sideLeft = 0,
    range_bar_sideRight = 1
} range_bar_side_t;

typedef void (*range_bar_change_cb_t)(float startPercent, float endPercent, void *user);
typedef void (*range_bar_drag_cb_t)(int dragging, float startPercent, float endPercent, void *user);
typedef void (*range_bar_tooltip_cb_t)(float startPercent, float endPercent, char *out, size_t cap, void *user);

e9ui_component_t *
range_bar_make(void);

void
range_bar_setSide(e9ui_component_t *comp, range_bar_side_t side);

void
range_bar_setMargins(e9ui_component_t *comp, int top, int bottom, int side);

void
range_bar_setWidth(e9ui_component_t *comp, int width);

void
range_bar_setHoverMargin(e9ui_component_t *comp, int margin);

int
range_bar_getHoverMargin(e9ui_component_t *comp);

void
range_bar_setCallback(e9ui_component_t *comp, range_bar_change_cb_t cb, void *user);

void
range_bar_setDragCallback(e9ui_component_t *comp, range_bar_drag_cb_t cb, void *user);

void
range_bar_setTooltipCallback(e9ui_component_t *comp, range_bar_tooltip_cb_t cb, void *user);

void
range_bar_setRangePercent(e9ui_component_t *comp, float startPercent, float endPercent);

int
range_bar_isDragging(e9ui_component_t *comp);

void
range_bar_layoutInParent(e9ui_component_t *comp, e9ui_context_t *ctx, e9ui_rect_t parent);
