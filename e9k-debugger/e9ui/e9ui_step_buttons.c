/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include "e9ui.h"
#include "e9ui_step_buttons.h"

typedef struct e9ui_step_buttons_rects {
    SDL_Rect pageUpRect;
    SDL_Rect lineUpRect;
    SDL_Rect lineDownRect;
    SDL_Rect pageDownRect;
} e9ui_step_buttons_rects_t;

static int
e9ui_step_buttons_pointInRect(SDL_Rect rect, int x, int y)
{
    return x >= rect.x && x < (rect.x + rect.w) &&
           y >= rect.y && y < (rect.y + rect.h);
}

static e9ui_step_buttons_action_t
e9ui_step_buttons_hit(const e9ui_step_buttons_rects_t *rects, int x, int y)
{
    if (!rects) {
        return e9ui_step_buttons_action_none;
    }
    if (e9ui_step_buttons_pointInRect(rects->pageUpRect, x, y)) {
        return e9ui_step_buttons_action_page_up;
    }
    if (e9ui_step_buttons_pointInRect(rects->lineUpRect, x, y)) {
        return e9ui_step_buttons_action_line_up;
    }
    if (e9ui_step_buttons_pointInRect(rects->lineDownRect, x, y)) {
        return e9ui_step_buttons_action_line_down;
    }
    if (e9ui_step_buttons_pointInRect(rects->pageDownRect, x, y)) {
        return e9ui_step_buttons_action_page_down;
    }
    return e9ui_step_buttons_action_none;
}

static int
e9ui_step_buttons_scrollbarLikeThickness(e9ui_context_t *ctx, e9ui_rect_t bounds)
{
    int thickness = e9ui_scale_px(ctx, 8);
    if (thickness < 4) {
        thickness = 4;
    }
    if (bounds.w > 0 && thickness >= bounds.w) {
        thickness = bounds.w > 1 ? bounds.w - 1 : 1;
    }
    if (bounds.h > 0 && thickness >= bounds.h) {
        thickness = bounds.h > 1 ? bounds.h - 1 : 1;
    }
    return thickness > 0 ? thickness : 0;
}

static int
e9ui_step_buttons_scrollbarLikeMargin(e9ui_context_t *ctx, e9ui_rect_t bounds)
{
    int margin = e9ui_scale_px(ctx, 4);
    if (margin < 0) {
        margin = 0;
    }
    if (bounds.w > 0 && margin >= bounds.w) {
        margin = bounds.w > 1 ? bounds.w - 1 : 0;
    }
    return margin;
}

static int
e9ui_step_buttons_computeRects(e9ui_context_t *ctx,
                               e9ui_rect_t bounds,
                               int topInsetPx,
                               e9ui_step_buttons_rects_t *out)
{
    if (!ctx || !out || bounds.w <= 0 || bounds.h <= 0) {
        return 0;
    }
    SDL_memset(out, 0, sizeof(*out));
    int thickness = e9ui_step_buttons_scrollbarLikeThickness(ctx, bounds);
    int buttonW = thickness * 2;
    int margin = e9ui_step_buttons_scrollbarLikeMargin(ctx, bounds);
    int buttonH = e9ui_scale_px(ctx, 18);
    int gap = e9ui_scale_px(ctx, 2);
    if (buttonH < 8) {
        buttonH = 8;
    }
    if (gap < 1) {
        gap = 1;
    }
    if (thickness <= 0 || buttonW <= 0) {
        return 0;
    }
    if (buttonW > bounds.w) {
        buttonW = bounds.w;
    }
    if (topInsetPx < 0) {
        topInsetPx = 0;
    }
    int groupH = buttonH * 4 + gap * 3;
    int x = bounds.x + bounds.w - margin - buttonW;
    int topY = bounds.y + topInsetPx + margin;
    if (x < bounds.x) {
        x = bounds.x;
    }
    if (topY + groupH > bounds.y + bounds.h - margin) {
        return 0;
    }

    out->pageUpRect = (SDL_Rect){ x, topY, buttonW, buttonH };
    out->lineUpRect = out->pageUpRect;
    out->lineUpRect.y += buttonH + gap;
    out->lineDownRect = out->lineUpRect;
    out->lineDownRect.y += buttonH + gap;
    out->pageDownRect = out->lineDownRect;
    out->pageDownRect.y += buttonH + gap;
    return 1;
}

static void
e9ui_step_buttons_drawArrowTriangle(SDL_Renderer *renderer, SDL_Rect rect, int directionUp)
{
    if (!renderer || rect.w <= 0 || rect.h <= 0) {
        return;
    }
    int insetX = rect.w / 4;
    int insetY = rect.h / 5;
    if (insetX < 1) {
        insetX = 1;
    }
    if (insetY < 1) {
        insetY = 1;
    }
    int left = rect.x + insetX;
    int right = rect.x + rect.w - insetX - 1;
    int top = rect.y + insetY;
    int bottom = rect.y + rect.h - insetY - 1;
    if (right < left) {
        right = left;
    }
    if (bottom < top) {
        bottom = top;
    }
    int width = right - left + 1;
    int height = bottom - top + 1;
    if (width <= 0 || height <= 0) {
        return;
    }
    int centerX = left + (width / 2);
    for (int row = 0; row < height; ++row) {
        int y = directionUp ? (top + row) : (bottom - row);
        int span = 1;
        if (height > 1) {
            span = 1 + ((width - 1) * row) / (height - 1);
        }
        if (span < 1) {
            span = 1;
        }
        if (span > width) {
            span = width;
        }
        int x0 = centerX - (span / 2);
        int x1 = x0 + span - 1;
        if (x0 < left) {
            x0 = left;
        }
        if (x1 > right) {
            x1 = right;
        }
        SDL_RenderDrawLine(renderer, x0, y, x1, y);
    }
}

static void
e9ui_step_buttons_drawButton(SDL_Renderer *renderer, SDL_Rect rect, int directionUp, int doubleArrow)
{
    if (!renderer || rect.w <= 0 || rect.h <= 0) {
        return;
    }
    if (!doubleArrow) {
        e9ui_step_buttons_drawArrowTriangle(renderer, rect, directionUp);
        return;
    }
    int halfH = rect.h / 2;
    if (halfH < 2) {
        halfH = rect.h;
    }
    SDL_Rect a = { rect.x, rect.y, rect.w, halfH };
    SDL_Rect b = { rect.x, rect.y + rect.h - halfH, rect.w, halfH };
    e9ui_step_buttons_drawArrowTriangle(renderer, a, directionUp);
    e9ui_step_buttons_drawArrowTriangle(renderer, b, directionUp);
}

void
e9ui_step_buttons_clearHold(e9ui_step_buttons_state_t *state)
{
    if (!state) {
        return;
    }
    state->holdAction = (int)e9ui_step_buttons_action_none;
    state->repeatTick = 0;
}

static void
e9ui_step_buttons_invokeAction(void *user, e9ui_step_buttons_action_fn onAction, e9ui_step_buttons_action_t action)
{
    if (!onAction || action == e9ui_step_buttons_action_none) {
        return;
    }
    (void)onAction(user, action);
}

void
e9ui_step_buttons_tick(e9ui_context_t *ctx,
                       e9ui_rect_t bounds,
                       int topInsetPx,
                       int enabled,
                       e9ui_step_buttons_state_t *state,
                       void *user,
                       e9ui_step_buttons_action_fn onAction)
{
    const uint32_t repeatDelayTicks = 18u;
    const uint32_t repeatIntervalTicks = 3u;
    if (!state) {
        return;
    }
    state->uiTick++;
    if (!enabled || !ctx) {
        e9ui_step_buttons_clearHold(state);
        return;
    }
    if (state->holdAction == (int)e9ui_step_buttons_action_none) {
        return;
    }
    e9ui_step_buttons_rects_t rects;
    if (!e9ui_step_buttons_computeRects(ctx, bounds, topInsetPx, &rects)) {
        e9ui_step_buttons_clearHold(state);
        return;
    }
    e9ui_step_buttons_action_t hoverAction = e9ui_step_buttons_hit(&rects, ctx->mouseX, ctx->mouseY);
    if (hoverAction != (e9ui_step_buttons_action_t)state->holdAction) {
        e9ui_step_buttons_clearHold(state);
        return;
    }
    uint32_t now = state->uiTick;
    if (state->repeatTick == 0) {
        state->repeatTick = now + repeatDelayTicks;
        return;
    }
    int guard = 0;
    while ((int32_t)(now - state->repeatTick) >= 0 && guard < 8) {
        e9ui_step_buttons_invokeAction(user, onAction, (e9ui_step_buttons_action_t)state->holdAction);
        state->repeatTick += repeatIntervalTicks;
        guard++;
    }
}

void
e9ui_step_buttons_render(e9ui_context_t *ctx,
                         e9ui_rect_t bounds,
                         int topInsetPx,
                         int enabled,
                         e9ui_step_buttons_state_t *state)
{
    if (!ctx || !ctx->renderer || !enabled) {
        return;
    }
    e9ui_step_buttons_rects_t rects;
    if (!e9ui_step_buttons_computeRects(ctx, bounds, topInsetPx, &rects)) {
        return;
    }
    e9ui_step_buttons_action_t pressedAction = e9ui_step_buttons_action_none;
    if (state && state->holdAction != (int)e9ui_step_buttons_action_none) {
        e9ui_step_buttons_action_t hoverAction = e9ui_step_buttons_hit(&rects, ctx->mouseX, ctx->mouseY);
        if (hoverAction == (e9ui_step_buttons_action_t)state->holdAction) {
            pressedAction = hoverAction;
        }
    }

    SDL_BlendMode prevBlend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(ctx->renderer, &prevBlend);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

#define E9UI_STEP_BUTTON_DRAW(rectField, actionId, upFlag, dblFlag) \
    do { \
        int bgAlpha = (pressedAction == (actionId)) ? 64 : 48; \
        int fg = (pressedAction == (actionId)) ? 191 : 255; \
        int fgAlpha = (pressedAction == (actionId)) ? 99 : 132; \
        SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, bgAlpha); \
        SDL_RenderFillRect(ctx->renderer, &rects.rectField); \
        SDL_SetRenderDrawColor(ctx->renderer, fg, fg, fg, fgAlpha); \
        e9ui_step_buttons_drawButton(ctx->renderer, rects.rectField, (upFlag), (dblFlag)); \
    } while (0)

    E9UI_STEP_BUTTON_DRAW(pageUpRect, e9ui_step_buttons_action_page_up, 1, 1);
    E9UI_STEP_BUTTON_DRAW(lineUpRect, e9ui_step_buttons_action_line_up, 1, 0);
    E9UI_STEP_BUTTON_DRAW(lineDownRect, e9ui_step_buttons_action_line_down, 0, 0);
    E9UI_STEP_BUTTON_DRAW(pageDownRect, e9ui_step_buttons_action_page_down, 0, 1);

#undef E9UI_STEP_BUTTON_DRAW

    SDL_SetRenderDrawBlendMode(ctx->renderer, prevBlend);
}

int
e9ui_step_buttons_handleEvent(e9ui_context_t *ctx,
                              const e9ui_event_t *ev,
                              e9ui_rect_t bounds,
                              int topInsetPx,
                              int enabled,
                              e9ui_step_buttons_state_t *state,
                              void *user,
                              e9ui_step_buttons_action_fn onAction)
{
    if (!ev || !state) {
        return 0;
    }
    if (!enabled || !ctx) {
        if (state->holdAction != (int)e9ui_step_buttons_action_none) {
            e9ui_step_buttons_clearHold(state);
        }
        return 0;
    }

    e9ui_step_buttons_rects_t rects;
    if (!e9ui_step_buttons_computeRects(ctx, bounds, topInsetPx, &rects)) {
        if (state->holdAction != (int)e9ui_step_buttons_action_none) {
            e9ui_step_buttons_clearHold(state);
        }
        return 0;
    }

    if (ev->type == SDL_MOUSEMOTION && state->holdAction != (int)e9ui_step_buttons_action_none) {
        e9ui_step_buttons_action_t hoverAction = e9ui_step_buttons_hit(&rects, ev->motion.x, ev->motion.y);
        if (hoverAction != (e9ui_step_buttons_action_t)state->holdAction) {
            e9ui_step_buttons_clearHold(state);
        }
    }

    if (ev->type == SDL_MOUSEBUTTONUP && ev->button.button == SDL_BUTTON_LEFT &&
        state->holdAction != (int)e9ui_step_buttons_action_none) {
        e9ui_step_buttons_clearHold(state);
        return 1;
    }

    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        e9ui_step_buttons_action_t action = e9ui_step_buttons_hit(&rects, ev->button.x, ev->button.y);
        if (action != e9ui_step_buttons_action_none) {
            e9ui_step_buttons_invokeAction(user, onAction, action);
            state->holdAction = (int)action;
            state->repeatTick = 0;
            return 1;
        }
    }
    return 0;
}
