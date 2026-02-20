/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>

#include "range_bar.h"
#include "alloc.h"
#include "e9ui_text_cache.h"

typedef struct range_bar_state {
    float startPercent;
    float endPercent;
    int dragging;
    int draggingHandle;
    int marginTop;
    int marginBottom;
    int marginSide;
    int width;
    int hoverMargin;
    range_bar_side_t side;
    range_bar_change_cb_t cb;
    void *cbUser;
    range_bar_drag_cb_t dragCb;
    void *dragUser;
    range_bar_tooltip_cb_t tooltipCb;
    void *tooltipUser;
} range_bar_state_t;

enum
{
    RANGE_BAR_HANDLE_NONE = 0,
    RANGE_BAR_HANDLE_START = 1,
    RANGE_BAR_HANDLE_END = 2
};

static float
range_bar_clampPercent(float percent)
{
    if (percent < 0.0f) {
        return 0.0f;
    }
    if (percent > 1.0f) {
        return 1.0f;
    }
    return percent;
}

static int
range_bar_percentToY(const e9ui_rect_t *bounds, float percent)
{
    if (!bounds || bounds->h <= 1) {
        return bounds ? bounds->y : 0;
    }
    float p = range_bar_clampPercent(percent);
    return bounds->y + (int)(p * (float)(bounds->h - 1) + 0.5f);
}

static float
range_bar_yToPercent(const e9ui_rect_t *bounds, int y)
{
    if (!bounds || bounds->h <= 1) {
        return 0.0f;
    }
    float percent = (float)(y - bounds->y) / (float)(bounds->h - 1);
    return range_bar_clampPercent(percent);
}

static void
range_bar_emitChange(range_bar_state_t *st)
{
    if (!st || !st->cb) {
        return;
    }
    st->cb(st->startPercent, st->endPercent, st->cbUser);
}

static void
range_bar_emitDrag(range_bar_state_t *st, int dragging)
{
    if (!st || !st->dragCb) {
        return;
    }
    st->dragCb(dragging ? 1 : 0, st->startPercent, st->endPercent, st->dragUser);
}

static int
range_bar_pickHandle(const e9ui_rect_t *bounds, float startPercent, float endPercent, int y)
{
    int startY = range_bar_percentToY(bounds, startPercent);
    int endY = range_bar_percentToY(bounds, endPercent);
    int startDistance = y - startY;
    int endDistance = y - endY;
    if (startDistance < 0) {
        startDistance = -startDistance;
    }
    if (endDistance < 0) {
        endDistance = -endDistance;
    }
    if (startDistance <= endDistance) {
        return RANGE_BAR_HANDLE_START;
    }
    return RANGE_BAR_HANDLE_END;
}

static void
range_bar_updateFromY(range_bar_state_t *st, const e9ui_rect_t *bounds, int handle, int y)
{
    if (!st || !bounds) {
        return;
    }
    float nextPercent = range_bar_yToPercent(bounds, y);
    if (handle == RANGE_BAR_HANDLE_START) {
        if (nextPercent > st->endPercent) {
            nextPercent = st->endPercent;
        }
        if (nextPercent != st->startPercent) {
            st->startPercent = nextPercent;
            range_bar_emitChange(st);
        }
        return;
    }
    if (handle == RANGE_BAR_HANDLE_END) {
        if (nextPercent < st->startPercent) {
            nextPercent = st->startPercent;
        }
        if (nextPercent != st->endPercent) {
            st->endPercent = nextPercent;
            range_bar_emitChange(st);
        }
    }
}

static int
range_bar_handleEvent(e9ui_component_t *self, e9ui_context_t *ctx, const e9ui_event_t *ev)
{
    if (!self || !ctx || !self->state || !ev) {
        return 0;
    }
    range_bar_state_t *st = (range_bar_state_t*)self->state;
    int grab = e9ui_scale_px(ctx, 6);
    if (grab < 0) {
        grab = 0;
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        int mx = ev->button.x;
        int my = ev->button.y;
        int inside = (mx >= self->bounds.x - grab && mx < self->bounds.x + self->bounds.w + grab &&
                      my >= self->bounds.y - grab && my < self->bounds.y + self->bounds.h + grab);
        if (!inside) {
            return 0;
        }
        st->dragging = 1;
        st->draggingHandle = range_bar_pickHandle(&self->bounds, st->startPercent, st->endPercent, my);
        range_bar_emitDrag(st, 1);
        range_bar_updateFromY(st, &self->bounds, st->draggingHandle, my);
        return 1;
    }
    if (ev->type == SDL_MOUSEMOTION) {
        if (!st->dragging) {
            return 0;
        }
        range_bar_updateFromY(st, &self->bounds, st->draggingHandle, ev->motion.y);
        return 1;
    }
    if (ev->type == SDL_MOUSEBUTTONUP && ev->button.button == SDL_BUTTON_LEFT) {
        if (!st->dragging) {
            return 0;
        }
        st->dragging = 0;
        st->draggingHandle = RANGE_BAR_HANDLE_NONE;
        range_bar_emitDrag(st, 0);
        return 1;
    }
    return 0;
}

static void
range_bar_renderTooltip(e9ui_component_t *self, e9ui_context_t *ctx, const range_bar_state_t *st, int anchorY)
{
    if (!self || !ctx || !ctx->renderer || !ctx->font || !st || !st->dragging) {
        return;
    }
    char text[128];
    text[0] = '\0';
    if (st->tooltipCb) {
        st->tooltipCb(st->startPercent, st->endPercent, text, sizeof(text), st->tooltipUser);
    }
    if (!text[0]) {
        snprintf(text, sizeof(text), "%.1f%%..%.1f%%", st->startPercent * 100.0f, st->endPercent * 100.0f);
    }
    SDL_Color color = { 255, 255, 255, 255 };
    int textW = 0;
    int textH = 0;
    SDL_Texture *texture = e9ui_text_cache_getText(ctx->renderer, ctx->font, text, color, &textW, &textH);
    if (!texture) {
        return;
    }
    int padX = e9ui_scale_px(ctx, 6);
    int padY = e9ui_scale_px(ctx, 4);
    int offsetX = e9ui_scale_px(ctx, 8);
    int tipW = textW + padX * 2;
    int tipH = textH + padY * 2;
    int tipX = 0;
    if (st->side == range_bar_sideLeft) {
        tipX = self->bounds.x + self->bounds.w + offsetX;
    } else {
        tipX = self->bounds.x - tipW - offsetX;
    }
    int tipY = anchorY - tipH / 2;
    if (tipX < 0) {
        tipX = 0;
    }
    if (tipY < 0) {
        tipY = 0;
    }
    if (ctx->winW > 0 && tipX + tipW > ctx->winW) {
        tipX = ctx->winW - tipW;
    }
    if (ctx->winH > 0 && tipY + tipH > ctx->winH) {
        tipY = ctx->winH - tipH;
    }
    SDL_Rect bg = { tipX, tipY, tipW, tipH };
    SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 30, 230);
    SDL_RenderFillRect(ctx->renderer, &bg);
    SDL_Rect dst = { tipX + padX, tipY + padY, textW, textH };
    SDL_RenderCopy(ctx->renderer, texture, NULL, &dst);
}

static void
range_bar_render(e9ui_component_t *self, e9ui_context_t *ctx)
{
    if (!self || !ctx || !ctx->renderer || !self->state) {
        return;
    }
    if (e9ui_getHidden(self)) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)self->state;
    int x = self->bounds.x;
    int y = self->bounds.y;
    int w = self->bounds.w;
    int h = self->bounds.h;
    if (w <= 0 || h <= 0) {
        return;
    }

    int trackW = w / 3;
    if (trackW < 3) {
        trackW = 3;
    }
    int trackX = x + (w - trackW) / 2;
    int startY = range_bar_percentToY(&self->bounds, st->startPercent);
    int endY = range_bar_percentToY(&self->bounds, st->endPercent);
    if (endY < startY) {
        int temp = startY;
        startY = endY;
        endY = temp;
    }

    SDL_Rect track = { trackX, y, trackW, h };
    SDL_SetRenderDrawColor(ctx->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(ctx->renderer, &track);

    int rangeH = endY - startY + 1;
    if (rangeH < 1) {
        rangeH = 1;
    }
    SDL_Rect range = { trackX, startY, trackW, rangeH };
    SDL_SetRenderDrawColor(ctx->renderer, 230, 33, 23, 220);
    SDL_RenderFillRect(ctx->renderer, &range);

    int handleH = e9ui_scale_px(ctx, 6);
    if (handleH < 4) {
        handleH = 4;
    }
    SDL_Rect startHandle = { x, startY - handleH / 2, w, handleH };
    SDL_Rect endHandle = { x, endY - handleH / 2, w, handleH };
    SDL_SetRenderDrawColor(ctx->renderer, 250, 250, 250, 255);
    SDL_RenderFillRect(ctx->renderer, &startHandle);
    SDL_RenderFillRect(ctx->renderer, &endHandle);

    if (st->dragging) {
        int anchorY = (st->draggingHandle == RANGE_BAR_HANDLE_START) ? startY : endY;
        range_bar_renderTooltip(self, ctx, st, anchorY);
    }
}

e9ui_component_t *
range_bar_make(void)
{
    e9ui_component_t *comp = (e9ui_component_t*)alloc_calloc(1, sizeof(*comp));
    if (!comp) {
        return NULL;
    }
    range_bar_state_t *st = (range_bar_state_t*)alloc_calloc(1, sizeof(*st));
    if (!st) {
        alloc_free(comp);
        return NULL;
    }
    st->startPercent = 0.2f;
    st->endPercent = 0.8f;
    st->dragging = 0;
    st->draggingHandle = RANGE_BAR_HANDLE_NONE;
    st->marginTop = 10;
    st->marginBottom = 10;
    st->marginSide = 10;
    st->width = 12;
    st->hoverMargin = 18;
    st->side = range_bar_sideLeft;

    comp->name = "range_bar";
    comp->state = st;
    comp->render = range_bar_render;
    comp->handleEvent = range_bar_handleEvent;
    return comp;
}

void
range_bar_setSide(e9ui_component_t *comp, range_bar_side_t side)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    st->side = side;
}

void
range_bar_setMargins(e9ui_component_t *comp, int top, int bottom, int side)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    st->marginTop = top;
    st->marginBottom = bottom;
    st->marginSide = side;
}

void
range_bar_setWidth(e9ui_component_t *comp, int width)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    st->width = width;
}

void
range_bar_setHoverMargin(e9ui_component_t *comp, int margin)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    st->hoverMargin = margin;
}

int
range_bar_getHoverMargin(e9ui_component_t *comp)
{
    if (!comp || !comp->state) {
        return 0;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    return st->hoverMargin;
}

void
range_bar_setCallback(e9ui_component_t *comp, range_bar_change_cb_t cb, void *user)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    st->cb = cb;
    st->cbUser = user;
}

void
range_bar_setDragCallback(e9ui_component_t *comp, range_bar_drag_cb_t cb, void *user)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    st->dragCb = cb;
    st->dragUser = user;
}

void
range_bar_setTooltipCallback(e9ui_component_t *comp, range_bar_tooltip_cb_t cb, void *user)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    st->tooltipCb = cb;
    st->tooltipUser = user;
}

void
range_bar_setRangePercent(e9ui_component_t *comp, float startPercent, float endPercent)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    float nextStart = range_bar_clampPercent(startPercent);
    float nextEnd = range_bar_clampPercent(endPercent);
    if (nextEnd < nextStart) {
        float temp = nextStart;
        nextStart = nextEnd;
        nextEnd = temp;
    }
    st->startPercent = nextStart;
    st->endPercent = nextEnd;
}

int
range_bar_isDragging(e9ui_component_t *comp)
{
    if (!comp || !comp->state) {
        return 0;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    return st->dragging ? 1 : 0;
}

void
range_bar_layoutInParent(e9ui_component_t *comp, e9ui_context_t *ctx, e9ui_rect_t parent)
{
    if (!comp || !comp->state) {
        return;
    }
    range_bar_state_t *st = (range_bar_state_t*)comp->state;
    int top = st->marginTop;
    int bottom = st->marginBottom;
    int side = st->marginSide;
    int width = st->width;
    if (ctx) {
        top = e9ui_scale_px(ctx, top);
        bottom = e9ui_scale_px(ctx, bottom);
        side = e9ui_scale_px(ctx, side);
        width = e9ui_scale_px(ctx, width);
    }
    int h = parent.h - top - bottom;
    if (h < 1) {
        h = 1;
    }
    if (width < 1) {
        width = 1;
    }
    comp->bounds.y = parent.y + top;
    comp->bounds.h = h;
    comp->bounds.w = width;
    if (st->side == range_bar_sideRight) {
        comp->bounds.x = parent.x + parent.w - side - width;
    } else {
        comp->bounds.x = parent.x + side;
    }
}
