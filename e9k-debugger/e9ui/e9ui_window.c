/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <SDL.h>
#include <stdint.h>
#include <string.h>

#include "alloc.h"
#include "e9ui.h"
#include "e9ui_window.h"

typedef struct e9ui_window_embedded_state
{
    e9ui_window_t *owner;
    e9ui_rect_t rect;
    e9ui_rect_t dragStartRect;
    char title[128];
    e9ui_component_t *body;
    SDL_Rect closeRect;
    int dragging;
    int dragOffsetX;
    int dragOffsetY;
    int resizing;
    int resizeMask;
    int resizeStartMouseX;
    int resizeStartMouseY;
    e9ui_window_close_cb_t onClose;
    void *onCloseUser;
} e9ui_window_embedded_state_t;

struct e9ui_window
{
    e9ui_window_backend_t backend;
    SDL_Window *sdlWindow;
    SDL_Renderer *sdlRenderer;
    uint32_t sdlWindowId;
    e9ui_component_t *embeddedHost;
    int open;
    int mainWindowFocused;
    int selfFocused;
    int alwaysOnTopState;
};

static SDL_Cursor *e9ui_window_embeddedCursorArrow = NULL;
static SDL_Cursor *e9ui_window_embeddedCursorHand = NULL;
static SDL_Cursor *e9ui_window_embeddedCursorMove = NULL;
static SDL_Cursor *e9ui_window_embeddedCursorNs = NULL;
static SDL_Cursor *e9ui_window_embeddedCursorEw = NULL;
static SDL_Cursor *e9ui_window_embeddedCursorNwse = NULL;
static SDL_Cursor *e9ui_window_embeddedCursorNesw = NULL;

enum
{
    E9UI_WINDOW_EMBEDDED_RESIZE_LEFT   = 1 << 0,
    E9UI_WINDOW_EMBEDDED_RESIZE_RIGHT  = 1 << 1,
    E9UI_WINDOW_EMBEDDED_RESIZE_TOP    = 1 << 2,
    E9UI_WINDOW_EMBEDDED_RESIZE_BOTTOM = 1 << 3
};

static void
e9ui_window_embeddedEnsureCursors(void)
{
    if (!e9ui_window_embeddedCursorArrow) {
        e9ui_window_embeddedCursorArrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    }
    if (!e9ui_window_embeddedCursorHand) {
        e9ui_window_embeddedCursorHand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    }
    if (!e9ui_window_embeddedCursorMove) {
        e9ui_window_embeddedCursorMove = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    }
    if (!e9ui_window_embeddedCursorNs) {
        e9ui_window_embeddedCursorNs = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    }
    if (!e9ui_window_embeddedCursorEw) {
        e9ui_window_embeddedCursorEw = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    }
    if (!e9ui_window_embeddedCursorNwse) {
        e9ui_window_embeddedCursorNwse = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    }
    if (!e9ui_window_embeddedCursorNesw) {
        e9ui_window_embeddedCursorNesw = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    }
}

static SDL_Cursor *
e9ui_window_embeddedResizeCursorForMask(int resizeMask)
{
    int horiz = resizeMask & (E9UI_WINDOW_EMBEDDED_RESIZE_LEFT | E9UI_WINDOW_EMBEDDED_RESIZE_RIGHT);
    int vert = resizeMask & (E9UI_WINDOW_EMBEDDED_RESIZE_TOP | E9UI_WINDOW_EMBEDDED_RESIZE_BOTTOM);
    if (horiz && vert) {
        int leftTop = (resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_LEFT) && (resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_TOP);
        int rightBottom = (resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_RIGHT) && (resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_BOTTOM);
        if (leftTop || rightBottom) {
            return e9ui_window_embeddedCursorNwse;
        }
        return e9ui_window_embeddedCursorNesw;
    }
    if (horiz) {
        return e9ui_window_embeddedCursorEw;
    }
    if (vert) {
        return e9ui_window_embeddedCursorNs;
    }
    return e9ui_window_embeddedCursorArrow;
}

static SDL_Texture *
e9ui_window_embeddedGetCloseIcon(SDL_Renderer *renderer, int *outW, int *outH)
{
    static SDL_Texture *icon = NULL;
    static int iconW = 0;
    static int iconH = 0;
    if (!renderer) {
        return NULL;
    }
    if (icon) {
        if (outW) {
            *outW = iconW;
        }
        if (outH) {
            *outH = iconH;
        }
        return icon;
    }
    char path[PATH_MAX];
    if (!file_getAssetPath("assets/icons/close.png", path, sizeof(path))) {
        return NULL;
    }
    SDL_Surface *s = IMG_Load(path);
    if (!s) {
        debug_error("e9ui_window: failed to load close icon %s: %s", path, IMG_GetError());
        return NULL;
    }
    icon = SDL_CreateTextureFromSurface(renderer, s);
    iconW = s->w;
    iconH = s->h;
    SDL_FreeSurface(s);
    if (outW) {
        *outW = iconW;
    }
    if (outH) {
        *outH = iconH;
    }
    return icon;
}

static int
e9ui_window_embeddedTitlebarHeight(e9ui_context_t *ctx)
{
    TTF_Font *font = e9ui->theme.text.source ? e9ui->theme.text.source : (ctx ? ctx->font : NULL);
    int textH = font ? TTF_FontHeight(font) : 16;
    if (textH <= 0) {
        textH = 16;
    }
    int padY = e9ui_scale_px(ctx, 4);
    return textH + padY * 2;
}

static int
e9ui_window_embeddedPreferredHeight(e9ui_component_t *self, e9ui_context_t *ctx, int availW)
{
    (void)self;
    (void)ctx;
    (void)availW;
    return 0;
}

static void
e9ui_window_embeddedLayout(e9ui_component_t *self, e9ui_context_t *ctx, e9ui_rect_t bounds)
{
    (void)bounds;
    if (!self || !self->state) {
        return;
    }
    e9ui_window_embedded_state_t *st = (e9ui_window_embedded_state_t *)self->state;
    self->bounds = st->rect;
    int titleH = e9ui_window_embeddedTitlebarHeight(ctx);
    int frameInset = e9ui_scale_px(ctx, 4);
    int bodyX = st->rect.x + frameInset;
    int bodyW = st->rect.w - frameInset * 2;
    int bodyY = st->rect.y + titleH;
    int bodyH = st->rect.h - titleH - frameInset;
    if (bodyW < 0) {
        bodyW = 0;
    }
    if (bodyH < 0) {
        bodyH = 0;
    }
    if (st->body && st->body->layout) {
        st->body->layout(st->body, ctx, (e9ui_rect_t){ bodyX, bodyY, bodyW, bodyH });
    }
}

static void
e9ui_window_embeddedDrawTitlebar(e9ui_window_embedded_state_t *st, e9ui_context_t *ctx, SDL_Rect rect)
{
    const e9k_theme_titlebar_t *theme = &e9ui->theme.titlebar;
    SDL_SetRenderDrawColor(ctx->renderer, theme->background.r, theme->background.g, theme->background.b, theme->background.a);
    SDL_RenderFillRect(ctx->renderer, &rect);

    int closePad = e9ui_scale_px(ctx, 6);
    int closeSize = rect.h - closePad * 2;
    if (closeSize < e9ui_scale_px(ctx, 12)) {
        closeSize = e9ui_scale_px(ctx, 12);
    }
    int closeX = rect.x + rect.w - closePad - closeSize;
    int closeY = rect.y + (rect.h - closeSize) / 2;
    st->closeRect = (SDL_Rect){ closeX, closeY, closeSize, closeSize };

    int padX = e9ui_scale_px(ctx, 8);
    TTF_Font *font = e9ui->theme.text.source ? e9ui->theme.text.source : (ctx ? ctx->font : NULL);
    if (font && st->title[0]) {
        int tw = 0;
        int th = 0;
        SDL_Texture *tex = e9ui_text_cache_getText(ctx->renderer, font, st->title, theme->text, &tw, &th);
        if (tex) {
            int textY = rect.y + (rect.h - th) / 2;
            if (textY < rect.y) {
                textY = rect.y;
            }
            int titleRightGap = e9ui_scale_px(ctx, 8);
            int maxTextW = (closeX - titleRightGap) - (rect.x + padX);
            if (maxTextW > 0) {
                SDL_Rect dst = { rect.x + padX, textY, tw, th };
                if (dst.w > maxTextW) {
                    SDL_Rect src = { 0, 0, maxTextW, th };
                    dst.w = maxTextW;
                    SDL_RenderCopy(ctx->renderer, tex, &src, &dst);
                } else {
                    SDL_RenderCopy(ctx->renderer, tex, NULL, &dst);
                }
            }
        }
    }
    SDL_Texture *icon = e9ui_window_embeddedGetCloseIcon(ctx->renderer, NULL, NULL);
    if (icon) {
        SDL_Rect dst = st->closeRect;
        SDL_RenderCopy(ctx->renderer, icon, NULL, &dst);
    } else {
        SDL_SetRenderDrawColor(ctx->renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(ctx->renderer, &st->closeRect);
        SDL_RenderDrawLine(ctx->renderer, closeX + 3, closeY + 3, closeX + closeSize - 4, closeY + closeSize - 4);
        SDL_RenderDrawLine(ctx->renderer, closeX + closeSize - 4, closeY + 3, closeX + 3, closeY + closeSize - 4);
    }
}

static void
e9ui_window_embeddedRender(e9ui_component_t *self, e9ui_context_t *ctx)
{
    if (!self || !self->state || !ctx || !ctx->renderer) {
        return;
    }
    e9ui_window_embedded_state_t *st = (e9ui_window_embedded_state_t *)self->state;
    SDL_Rect bg = { self->bounds.x, self->bounds.y, self->bounds.w, self->bounds.h };
    int hadClip = SDL_RenderIsClipEnabled(ctx->renderer) ? 1 : 0;
    SDL_Rect prevClip = { 0, 0, 0, 0 };
    if (hadClip) {
        SDL_RenderGetClipRect(ctx->renderer, &prevClip);
        SDL_Rect clipped = bg;
        if (SDL_IntersectRect(&prevClip, &bg, &clipped)) {
            SDL_RenderSetClipRect(ctx->renderer, &clipped);
        } else {
            SDL_RenderSetClipRect(ctx->renderer, &bg);
        }
    } else {
        SDL_RenderSetClipRect(ctx->renderer, &bg);
    }
    SDL_SetRenderDrawColor(ctx->renderer, 24, 24, 24, 255);
    SDL_RenderFillRect(ctx->renderer, &bg);
    SDL_SetRenderDrawColor(ctx->renderer, 70, 70, 70, 255);
    SDL_RenderDrawRect(ctx->renderer, &bg);

    SDL_Rect titleRect = { self->bounds.x, self->bounds.y, self->bounds.w, e9ui_window_embeddedTitlebarHeight(ctx) };
    e9ui_window_embeddedDrawTitlebar(st, ctx, titleRect);
    if (st->body && st->body->render) {
        st->body->render(st->body, ctx);
    }
    if (hadClip) {
        SDL_RenderSetClipRect(ctx->renderer, &prevClip);
    } else {
        SDL_RenderSetClipRect(ctx->renderer, NULL);
    }
}

static int
e9ui_window_embeddedPointInRect(const SDL_Rect *rect, int x, int y)
{
    if (!rect) {
        return 0;
    }
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static int
e9ui_window_embeddedPointInE9Rect(const e9ui_rect_t *rect, int x, int y)
{
    if (!rect) {
        return 0;
    }
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static void
e9ui_window_embeddedRaiseToFront(e9ui_component_t *self)
{
    if (!self || !e9ui || !e9ui->root) {
        return;
    }
    e9ui_component_t *root = e9ui->root;
    e9ui_component_child_t *container = NULL;
    for (list_t *ptr = root->children; ptr; ptr = ptr->next) {
        e9ui_component_child_t *it = (e9ui_component_child_t *)ptr->data;
        if (it && it->component == self) {
            container = it;
            break;
        }
    }
    if (!container) {
        return;
    }
    list_t *last = list_last(root->children);
    if (last && last->data == container) {
        return;
    }
    list_remove(&root->children, container, 0);
    list_append(&root->children, container);
}

static int
e9ui_window_componentContainsComponent(const e9ui_component_t *root, const e9ui_component_t *needle)
{
    if (!root || !needle) {
        return 0;
    }
    if (root == needle) {
        return 1;
    }
    for (list_t *ptr = root->children; ptr; ptr = ptr->next) {
        e9ui_component_child_t *container = (e9ui_component_child_t *)ptr->data;
        if (!container || !container->component) {
            continue;
        }
        if (e9ui_window_componentContainsComponent(container->component, needle)) {
            return 1;
        }
    }
    return 0;
}

static int
e9ui_window_embeddedResizeMaskAt(const e9ui_window_embedded_state_t *st, const e9ui_context_t *ctx, int x, int y)
{
    if (!st || !ctx) {
        return 0;
    }
    if (!e9ui_window_embeddedPointInE9Rect(&st->rect, x, y)) {
        return 0;
    }
    int hit = e9ui_scale_px(ctx, 6);
    if (hit < 2) {
        hit = 2;
    }
    int left = (x - st->rect.x) <= hit ? 1 : 0;
    int right = (st->rect.x + st->rect.w - 1 - x) <= hit ? 1 : 0;
    int top = (y - st->rect.y) <= hit ? 1 : 0;
    int bottom = (st->rect.y + st->rect.h - 1 - y) <= hit ? 1 : 0;
    int mask = 0;
    if (left) {
        mask |= E9UI_WINDOW_EMBEDDED_RESIZE_LEFT;
    }
    if (right) {
        mask |= E9UI_WINDOW_EMBEDDED_RESIZE_RIGHT;
    }
    if (top) {
        mask |= E9UI_WINDOW_EMBEDDED_RESIZE_TOP;
    }
    if (bottom) {
        mask |= E9UI_WINDOW_EMBEDDED_RESIZE_BOTTOM;
    }
    return mask;
}

static void
e9ui_window_embeddedClampRectToBounds(e9ui_rect_t *rect, const e9ui_context_t *ctx, int minW, int minH)
{
    if (!rect) {
        return;
    }
    if (rect->w < minW) {
        rect->w = minW;
    }
    if (rect->h < minH) {
        rect->h = minH;
    }
    if (rect->x < 0) {
        rect->x = 0;
    }
    if (rect->y < 0) {
        rect->y = 0;
    }
    if (ctx && ctx->winW > 0 && rect->w > ctx->winW) {
        rect->w = ctx->winW;
    }
    if (ctx && ctx->winH > 0 && rect->h > ctx->winH) {
        rect->h = ctx->winH;
    }
    if (ctx && ctx->winW > 0 && rect->x + rect->w > ctx->winW) {
        rect->x = ctx->winW - rect->w;
    }
    if (ctx && ctx->winH > 0 && rect->y + rect->h > ctx->winH) {
        rect->y = ctx->winH - rect->h;
    }
    if (rect->x < 0) {
        rect->x = 0;
    }
    if (rect->y < 0) {
        rect->y = 0;
    }
}

static void
e9ui_window_embeddedApplyResizeDrag(e9ui_window_embedded_state_t *st, const e9ui_context_t *ctx, int mouseX, int mouseY)
{
    if (!st || !ctx || !st->resizing || !st->resizeMask) {
        return;
    }
    int minW = e9ui_scale_px(ctx, 360);
    int minH = e9ui_scale_px(ctx, 320);
    int dx = mouseX - st->resizeStartMouseX;
    int dy = mouseY - st->resizeStartMouseY;

    int left = st->dragStartRect.x;
    int top = st->dragStartRect.y;
    int right = st->dragStartRect.x + st->dragStartRect.w;
    int bottom = st->dragStartRect.y + st->dragStartRect.h;

    if (st->resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_LEFT) {
        left = st->dragStartRect.x + dx;
    }
    if (st->resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_RIGHT) {
        right = st->dragStartRect.x + st->dragStartRect.w + dx;
    }
    if (st->resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_TOP) {
        top = st->dragStartRect.y + dy;
    }
    if (st->resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_BOTTOM) {
        bottom = st->dragStartRect.y + st->dragStartRect.h + dy;
    }

    if (ctx->winW > 0) {
        if (left < 0) {
            left = 0;
        }
        if (right > ctx->winW) {
            right = ctx->winW;
        }
    }
    if (ctx->winH > 0) {
        if (top < 0) {
            top = 0;
        }
        if (bottom > ctx->winH) {
            bottom = ctx->winH;
        }
    }

    if (right - left < minW) {
        if (st->resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_LEFT) {
            left = right - minW;
        } else {
            right = left + minW;
        }
    }
    if (bottom - top < minH) {
        if (st->resizeMask & E9UI_WINDOW_EMBEDDED_RESIZE_TOP) {
            top = bottom - minH;
        } else {
            bottom = top + minH;
        }
    }

    if (ctx->winW > 0) {
        if (left < 0) {
            left = 0;
            if (right - left < minW) {
                right = left + minW;
            }
        }
        if (right > ctx->winW) {
            right = ctx->winW;
            if (right - left < minW) {
                left = right - minW;
            }
        }
    }
    if (ctx->winH > 0) {
        if (top < 0) {
            top = 0;
            if (bottom - top < minH) {
                bottom = top + minH;
            }
        }
        if (bottom > ctx->winH) {
            bottom = ctx->winH;
            if (bottom - top < minH) {
                top = bottom - minH;
            }
        }
    }

    st->rect.x = left;
    st->rect.y = top;
    st->rect.w = right - left;
    st->rect.h = bottom - top;
    e9ui_window_embeddedClampRectToBounds(&st->rect, ctx, minW, minH);
}

static void
e9ui_window_embeddedUpdateCursor(e9ui_component_t *self,
                                 e9ui_window_embedded_state_t *st,
                                 e9ui_context_t *ctx,
                                 int mx,
                                 int my)
{
    if (!self || !st || !ctx) {
        return;
    }
    e9ui_window_embeddedEnsureCursors();
    int titleH = e9ui_window_embeddedTitlebarHeight(ctx);
    SDL_Rect titleRect = { self->bounds.x, self->bounds.y, self->bounds.w, titleH };
    int hoverClose = e9ui_window_embeddedPointInRect(&st->closeRect, mx, my);
    int hoverResizeMask = hoverClose ? 0 : e9ui_window_embeddedResizeMaskAt(st, ctx, mx, my);
    int hoverTitle = e9ui_window_embeddedPointInRect(&titleRect, mx, my) ? 1 : 0;
    if (hoverTitle && (hoverClose || hoverResizeMask)) {
        hoverTitle = 0;
    }
    SDL_Cursor *cursor = NULL;
    if (st->resizing) {
        cursor = e9ui_window_embeddedResizeCursorForMask(st->resizeMask);
    } else if (st->dragging) {
        cursor = e9ui_window_embeddedCursorMove;
    } else if (hoverClose) {
        cursor = e9ui_window_embeddedCursorHand ? e9ui_window_embeddedCursorHand : e9ui_window_embeddedCursorArrow;
    } else if (hoverResizeMask) {
        cursor = e9ui_window_embeddedResizeCursorForMask(hoverResizeMask);
    } else if (hoverTitle) {
        cursor = e9ui_window_embeddedCursorMove ? e9ui_window_embeddedCursorMove : e9ui_window_embeddedCursorArrow;
    } else if (e9ui_window_embeddedPointInE9Rect(&st->rect, mx, my)) {
        cursor = e9ui_window_embeddedCursorArrow;
    }
    if (cursor) {
        ctx->cursorOverride = 1;
        SDL_SetCursor(cursor);
    }
}

static int
e9ui_window_embeddedHandleEvent(e9ui_component_t *self, e9ui_context_t *ctx, const e9ui_event_t *ev)
{
    if (!self || !self->state || !ctx || !ev) {
        return 0;
    }
    e9ui_window_embedded_state_t *st = (e9ui_window_embedded_state_t *)self->state;
    int titleH = e9ui_window_embeddedTitlebarHeight(ctx);
    SDL_Rect titleRect = { self->bounds.x, self->bounds.y, self->bounds.w, titleH };
    if (ev->type == SDL_MOUSEMOTION) {
        e9ui_window_embeddedUpdateCursor(self, st, ctx, ev->motion.x, ev->motion.y);
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        int mx = ev->button.x;
        int my = ev->button.y;
        if (e9ui_window_embeddedPointInE9Rect(&st->rect, mx, my)) {
            e9ui_window_embeddedRaiseToFront(self);
        }
        if (e9ui_window_embeddedPointInRect(&st->closeRect, mx, my)) {
            if (st->onClose) {
                st->onClose(st->owner, st->onCloseUser);
            }
            return 1;
        }
        int resizeMask = e9ui_window_embeddedResizeMaskAt(st, ctx, mx, my);
        if (resizeMask) {
            st->resizing = 1;
            st->resizeMask = resizeMask;
            st->resizeStartMouseX = mx;
            st->resizeStartMouseY = my;
            st->dragStartRect = st->rect;
            st->dragging = 0;
            return 1;
        }
        if (e9ui_window_embeddedPointInRect(&titleRect, mx, my)) {
            st->dragging = 1;
            st->dragStartRect = st->rect;
            st->dragOffsetX = mx - st->rect.x;
            st->dragOffsetY = my - st->rect.y;
            return 1;
        }
    }
    if (ev->type == SDL_MOUSEBUTTONUP && ev->button.button == SDL_BUTTON_LEFT) {
        if (st->resizing) {
            st->resizing = 0;
            st->resizeMask = 0;
            e9ui_window_embeddedUpdateCursor(self, st, ctx, ev->button.x, ev->button.y);
            return 1;
        }
        if (st->dragging) {
            st->dragging = 0;
            e9ui_window_embeddedUpdateCursor(self, st, ctx, ev->button.x, ev->button.y);
            return 1;
        }
        e9ui_window_embeddedUpdateCursor(self, st, ctx, ev->button.x, ev->button.y);
    }
    if (ev->type == SDL_KEYDOWN) {
        SDL_Keycode key = ev->key.keysym.sym;
        SDL_Keymod mods = ev->key.keysym.mod;
        int accel = (mods & KMOD_GUI) || (mods & KMOD_CTRL);
        if (!accel && key == SDLK_TAB) {
            e9ui_component_t *focus = e9ui_getFocus(ctx);
            int focusInWindow = (focus && e9ui_window_componentContainsComponent(self, focus)) ? 1 : 0;
            int reverse = (mods & KMOD_SHIFT) ? 1 : 0;
            e9ui_component_t *next = e9ui_focusFindNext(self, focusInWindow ? focus : NULL, reverse);
            if (next) {
                e9ui_setFocus(ctx, next);
                return 1;
            }
        }
    }
    if (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_ESCAPE) {
        e9ui_component_t *focus = e9ui_getFocus(ctx);
        if (focus && e9ui_window_componentContainsComponent(self, focus)) {
            if (st->onClose) {
                st->onClose(st->owner, st->onCloseUser);
            }
            return 1;
        }
    }
    if (ev->type == SDL_MOUSEMOTION && st->resizing) {
        e9ui_window_embeddedApplyResizeDrag(st, ctx, ev->motion.x, ev->motion.y);
        return 1;
    }
    if (ev->type == SDL_MOUSEMOTION && st->dragging) {
        int nextX = ev->motion.x - st->dragOffsetX;
        int nextY = ev->motion.y - st->dragOffsetY;
        st->rect.x = nextX;
        st->rect.y = nextY;
        return 1;
    }
    if (ev->type == SDL_MOUSEMOTION) {
        if (e9ui_window_embeddedPointInE9Rect(&st->rect, ev->motion.x, ev->motion.y)) {
            return 1;
        }
    } else if (ev->type == SDL_MOUSEBUTTONDOWN || ev->type == SDL_MOUSEBUTTONUP) {
        if (e9ui_window_embeddedPointInE9Rect(&st->rect, ev->button.x, ev->button.y)) {
            return 1;
        }
    } else if (ev->type == SDL_MOUSEWHEEL) {
        if (e9ui_window_embeddedPointInE9Rect(&st->rect, ev->wheel.mouseX, ev->wheel.mouseY)) {
            return 1;
        }
    }
    return 0;
}

static int
e9ui_window_dispatchEmbeddedKeydownRecursive(e9ui_component_t *comp, e9ui_context_t *ctx, const e9ui_event_t *ev)
{
    if (!comp || e9ui_getHidden(comp)) {
        return 0;
    }
    e9ui_child_reverse_iterator iter;
    if (e9ui_child_iterateChildrenReverse(comp, &iter)) {
        for (e9ui_child_reverse_iterator *it = e9ui_child_iteratePrev(&iter);
             it;
             it = e9ui_child_iteratePrev(&iter)) {
            if (!it->child) {
                continue;
            }
            if (e9ui_window_dispatchEmbeddedKeydownRecursive(it->child, ctx, ev)) {
                return 1;
            }
        }
    }
    if (comp->name &&
        strcmp(comp->name, "e9ui_window_embedded") == 0 &&
        comp->handleEvent) {
        if (comp->handleEvent(comp, ctx, ev)) {
            return 1;
        }
    }
    return 0;
}

static void
e9ui_window_embeddedDtor(e9ui_component_t *self, e9ui_context_t *ctx)
{
    (void)ctx;
    if (!self || !self->state) {
        return;
    }
    e9ui_window_embedded_state_t *st = (e9ui_window_embedded_state_t *)self->state;
    if (st->owner) {
        st->owner->embeddedHost = NULL;
        st->owner->open = 0;
    }
}

static e9ui_component_t *
e9ui_window_embeddedMake(e9ui_window_t *owner,
                         const char *title,
                         e9ui_rect_t rect,
                         e9ui_component_t *body,
                         e9ui_window_close_cb_t onClose,
                         void *onCloseUser)
{
    e9ui_component_t *comp = (e9ui_component_t *)alloc_calloc(1, sizeof(*comp));
    if (!comp) {
        return NULL;
    }
    e9ui_window_embedded_state_t *st = (e9ui_window_embedded_state_t *)alloc_calloc(1, sizeof(*st));
    if (!st) {
        alloc_free(comp);
        return NULL;
    }
    st->owner = owner;
    st->rect = rect;
    st->body = body;
    st->onClose = onClose;
    st->onCloseUser = onCloseUser;
    if (title && *title) {
        strncpy(st->title, title, sizeof(st->title) - 1);
        st->title[sizeof(st->title) - 1] = '\0';
    }
    comp->name = "e9ui_window_embedded";
    comp->state = st;
    comp->preferredHeight = e9ui_window_embeddedPreferredHeight;
    comp->layout = e9ui_window_embeddedLayout;
    comp->render = e9ui_window_embeddedRender;
    comp->handleEvent = e9ui_window_embeddedHandleEvent;
    comp->dtor = e9ui_window_embeddedDtor;
    if (body) {
        e9ui_child_add(comp, body, alloc_strdup("window_body"));
    }
    return comp;
}

e9ui_window_t *
e9ui_windowCreate(e9ui_window_backend_t backend)
{
    e9ui_window_t *window = (e9ui_window_t *)alloc_calloc(1, sizeof(*window));
    if (!window) {
        return NULL;
    }
    window->backend = backend;
    window->alwaysOnTopState = -1;
    return window;
}

void
e9ui_windowDestroy(e9ui_window_t *window)
{
    if (!window) {
        return;
    }
    e9ui_windowClose(window);
    alloc_free(window);
}

int
e9ui_windowOpenSdl(e9ui_window_t *window,
                   const char *title,
                   int x,
                   int y,
                   int w,
                   int h,
                   uint32_t windowFlags)
{
    if (!window || window->backend != e9ui_window_backend_sdl) {
        return 0;
    }
    if (window->sdlWindow) {
        return 1;
    }
    SDL_Window *sdlWindow = SDL_CreateWindow(title, x, y, w, h, windowFlags);
    if (!sdlWindow) {
        return 0;
    }
    window->sdlWindow = sdlWindow;
    window->sdlWindowId = SDL_GetWindowID(sdlWindow);
    window->open = 1;
    window->alwaysOnTopState = -1;
    window->selfFocused = 0;
    window->mainWindowFocused = 0;
    return 1;
}

int
e9ui_windowCreateSdlRenderer(e9ui_window_t *window, int rendererIndex, uint32_t rendererFlags)
{
    if (!window || window->backend != e9ui_window_backend_sdl || !window->sdlWindow) {
        return 0;
    }
    if (window->sdlRenderer) {
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window->sdlWindow, rendererIndex, rendererFlags);
    if (!renderer) {
        return 0;
    }
    window->sdlRenderer = renderer;
    return 1;
}

int
e9ui_windowOpenEmbedded(e9ui_window_t *window,
                        const char *title,
                        e9ui_rect_t rect,
                        e9ui_component_t *body,
                        e9ui_window_close_cb_t onClose,
                        void *onCloseUser,
                        e9ui_context_t *ctx)
{
    if (!window || window->backend != e9ui_window_backend_embedded || !ctx || !e9ui || !e9ui->root) {
        return 0;
    }
    if (window->embeddedHost) {
        return 1;
    }
    e9ui_component_t *host = e9ui_window_embeddedMake(window, title, rect, body, onClose, onCloseUser);
    if (!host) {
        return 0;
    }
    if (e9ui->root->name && strcmp(e9ui->root->name, "e9ui_stack") == 0) {
        e9ui_stack_addFixed(e9ui->root, host);
    } else {
        e9ui_child_add(e9ui->root, host, alloc_strdup("e9ui_window_embedded"));
    }
    window->embeddedHost = host;
    window->open = 1;
    window->sdlWindowId = 0;
    return 1;
}

void
e9ui_windowClose(e9ui_window_t *window)
{
    if (!window) {
        return;
    }
    if (window->sdlRenderer) {
        SDL_DestroyRenderer(window->sdlRenderer);
        window->sdlRenderer = NULL;
    }
    if (window->sdlWindow) {
        SDL_DestroyWindow(window->sdlWindow);
        window->sdlWindow = NULL;
    }
    if (window->embeddedHost && e9ui && e9ui->ctx.window) {
        e9ui_component_t *root = e9ui->root;
        if (root) {
            if (root->name && strcmp(root->name, "e9ui_stack") == 0) {
                e9ui_stack_remove(root, &e9ui->ctx, window->embeddedHost);
            } else {
                e9ui_childRemove(root, window->embeddedHost, &e9ui->ctx);
            }
        }
        window->embeddedHost = NULL;
    }
    window->sdlWindowId = 0;
    window->open = 0;
    window->mainWindowFocused = 0;
    window->selfFocused = 0;
    window->alwaysOnTopState = 0;
}

int
e9ui_windowIsOpen(const e9ui_window_t *window)
{
    if (!window) {
        return 0;
    }
    return window->open ? 1 : 0;
}

SDL_Window *
e9ui_windowGetSdlWindow(const e9ui_window_t *window)
{
    if (!window) {
        return NULL;
    }
    return window->sdlWindow;
}

SDL_Renderer *
e9ui_windowGetSdlRenderer(const e9ui_window_t *window)
{
    if (!window) {
        return NULL;
    }
    return window->sdlRenderer;
}

uint32_t
e9ui_windowGetWindowId(const e9ui_window_t *window)
{
    if (!window) {
        return 0;
    }
    return window->sdlWindowId;
}

int
e9ui_windowIsEmbedded(const e9ui_window_t *window)
{
    if (!window) {
        return 0;
    }
    return window->backend == e9ui_window_backend_embedded ? 1 : 0;
}

e9ui_rect_t
e9ui_windowGetEmbeddedRect(const e9ui_window_t *window)
{
    e9ui_rect_t rect = { 0, 0, 0, 0 };
    if (!window || !window->embeddedHost || !window->embeddedHost->state) {
        return rect;
    }
    e9ui_window_embedded_state_t *st = (e9ui_window_embedded_state_t *)window->embeddedHost->state;
    return st->rect;
}

int
e9ui_windowDispatchEmbeddedKeydown(e9ui_component_t *root, e9ui_context_t *ctx, const e9ui_event_t *ev)
{
    if (!root || !ctx || !ev || ev->type != SDL_KEYDOWN) {
        return 0;
    }
    return e9ui_window_dispatchEmbeddedKeydownRecursive(root, ctx, ev);
}

void
e9ui_windowSetMainWindowFocused(e9ui_window_t *window, int focused)
{
    if (!window) {
        return;
    }
    window->mainWindowFocused = focused ? 1 : 0;
}

void
e9ui_windowSetSelfFocused(e9ui_window_t *window, int focused)
{
    if (!window) {
        return;
    }
    window->selfFocused = focused ? 1 : 0;
}

void
e9ui_windowRefreshSelfFocusedFromFlags(e9ui_window_t *window)
{
    if (!window || !window->sdlWindow) {
        return;
    }
    uint32_t flags = SDL_GetWindowFlags(window->sdlWindow);
    window->selfFocused = (flags & SDL_WINDOW_INPUT_FOCUS) ? 1 : 0;
}

void
e9ui_windowUpdateAlwaysOnTop(e9ui_window_t *window)
{
#ifndef E9K_DISABLE_ALWAYS_ON_TOP
    if (!window || window->backend != e9ui_window_backend_sdl || !window->sdlWindow) {
        return;
    }
    int shouldStayOnTop = (window->mainWindowFocused || window->selfFocused) ? 1 : 0;
    if (window->alwaysOnTopState == shouldStayOnTop) {
        return;
    }
    SDL_SetWindowAlwaysOnTop(window->sdlWindow, shouldStayOnTop ? SDL_TRUE : SDL_FALSE);
    window->alwaysOnTopState = shouldStayOnTop;
#else
    (void)window;
#endif
}
