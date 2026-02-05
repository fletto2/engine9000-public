/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "memory.h"
#include "e9ui_context.h"
#include "e9ui_stack.h"
#include "e9ui_textbox.h"
#include "e9ui_text_cache.h"
#include "debugger.h"
#include "libretro_host.h"

typedef struct memory_view_state {
    unsigned int   base;
    unsigned int   size;
    unsigned char *data;
    e9ui_component_t *textbox;
    char           error[128];
} memory_view_state_t;

static memory_view_state_t *g_memory_view_state = NULL;

static void
memory_fillFromram(memory_view_state_t *st, unsigned int base);

static int
memory_getAddressLimits(uint32_t *outMinAddr, uint32_t *outMaxAddr)
{
    if (outMinAddr) {
        *outMinAddr = 0;
    }
    if (outMaxAddr) {
        *outMaxAddr = 0x00ffffffu;
    }
    if (target && target->memoryGetLimits) {
        return target->memoryGetLimits(outMinAddr, outMaxAddr);
    }
    return 0;
}

static void
memory_syncTextboxFromBase(memory_view_state_t *st)
{
    if (!st || !st->textbox) {
        return;
    }
    char addrText[32];
    snprintf(addrText, sizeof(addrText), "0x%06X", st->base & 0x00ffffffu);
    e9ui_textbox_setText(st->textbox, addrText);
}

static uint32_t
memory_clampBaseForView(memory_view_state_t *st, uint32_t base)
{
    if (!st) {
        return base & 0x00ffffffu;
    }
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    if (!memory_getAddressLimits(&minAddr, &maxAddr)) {
        minAddr = 0;
        maxAddr = 0x00ffffffu;
    }
    uint64_t span = st->size > 0 ? (uint64_t)(st->size - 1u) : 0ull;
    uint32_t maxBase = maxAddr;
    if ((uint64_t)maxAddr >= span) {
        maxBase = (uint32_t)((uint64_t)maxAddr - span);
    } else {
        maxBase = minAddr;
    }
    if (maxBase < minAddr) {
        maxBase = minAddr;
    }
    uint32_t base24 = base & 0x00ffffffu;
    if (base24 < minAddr) {
        base24 = minAddr;
    }
    if (base24 > maxBase) {
        base24 = maxBase;
    }
    return base24;
}

static void
memory_scrollRows(memory_view_state_t *st, int rows)
{
    if (!st || rows == 0) {
        return;
    }
    int64_t delta = (int64_t)rows * 16ll;
    int64_t rawBase = (int64_t)(uint32_t)(st->base & 0x00ffffffu) + delta;
    if (rawBase < 0) {
        rawBase = 0;
    }
    uint32_t clamped = memory_clampBaseForView(st, (uint32_t)rawBase);
    if (clamped == st->base) {
        return;
    }
    st->base = clamped;
    memory_syncTextboxFromBase(st);
    memory_fillFromram(st, st->base);
}

static void
memory_setError(memory_view_state_t *panel, const char *fmt, ...)
{
    if (!panel) {
        return;
    }
    panel->error[0] = '\0';
    if (!fmt || !*fmt) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(panel->error, sizeof(panel->error), fmt, ap);
    va_end(ap);
    panel->error[sizeof(panel->error) - 1] = '\0';
}

static void
memory_clearData(memory_view_state_t *panel)
{
    if (!panel || !panel->data) {
        return;
    }
    memset(panel->data, 0, panel->size);
}

static int
memory_parseAddress(memory_view_state_t *st, unsigned int *out_addr)
{
    if (!st || !st->textbox || !out_addr) {
        return 0;
    }
    const char *t = e9ui_textbox_getText(st->textbox);
    if (!t || !*t) {
        memory_setError(st, "Invalid address: empty input");
        return 0;
    }
    char *end = NULL;
    unsigned long long val = strtoull(t, &end, 0);
    if (!end || end == t) {
        memory_setError(st, "Invalid address: \"%s\"", t);
        return 0;
    }
    while (*end && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end) {
        memory_setError(st, "Invalid address: \"%s\"", t);
        return 0;
    }
    if (val > 0x00ffffffull) {
        memory_setError(st, "Address outside 24-bit range (0x000000-0xFFFFFF)");
        return 0;
    }
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    int hasLimits = memory_getAddressLimits(&minAddr, &maxAddr);
    if (hasLimits && ((uint32_t)val < minAddr || (uint32_t)val > maxAddr)) {
        memory_setError(st, "Address outside range (0x%06X-0x%06X)", minAddr, maxAddr);
        return 0;
    }
    *out_addr = (unsigned int)val;
    return 1;
}

static void
memory_fillFromram(memory_view_state_t *st, unsigned int base)
{
    if (!st || !st->data) {
        return;
    }
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    int hasLimits = memory_getAddressLimits(&minAddr, &maxAddr);
    memory_setError(st, NULL);
    int range_error = 0;
    for (unsigned int i = 0; i < st->size; ++i) {
        uint32_t addr = base + i;
        if (addr > 0x00ffffffu) {
            st->data[i] = 0;
            range_error = 1;
            continue;
        }
        if (hasLimits && (addr < minAddr || addr > maxAddr)) {
            st->data[i] = 0;
            range_error = 1;
            continue;
        }
        if (!libretro_host_debugReadMemory(addr, &st->data[i], 1)) {
            st->data[i] = 0;
            range_error = 1;
        }
    }
    if (range_error) {
        if (hasLimits) {
            memory_setError(st, "Range exceeds limits (0x%06X-0x%06X)", minAddr, maxAddr);
        } else {
            memory_setError(st, "Range exceeds 24-bit address space (0x000000-0xFFFFFF)");
        }
    }
}

static void
memory_onAddressSubmit(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    memory_view_state_t *st = (memory_view_state_t*)user;
    if (!st || !st->textbox) {
        return;
    }
    unsigned int addr = 0;
    if (!memory_parseAddress(st, &addr)) {
        return;
    }
    st->base = memory_clampBaseForView(st, addr);
    memory_syncTextboxFromBase(st);
    memory_fillFromram(st, st->base);
}

static int
memory_handleEvent(e9ui_component_t *self, e9ui_context_t *ctx, const e9ui_event_t *ev)
{
    if (!self || !ctx || !ev) {
        return 0;
    }
    memory_view_state_t *st = (memory_view_state_t*)self->state;
    if (!st) {
        return 0;
    }

    if (ev->type == SDL_MOUSEWHEEL) {
        int mx = ctx->mouseX;
        int my = ctx->mouseY;
        if (mx < self->bounds.x || mx >= self->bounds.x + self->bounds.w ||
            my < self->bounds.y || my >= self->bounds.y + self->bounds.h) {
            return 0;
        }
        if (ev->wheel.y != 0) {
            memory_scrollRows(st, -ev->wheel.y * 3);
        }
        return 1;
    }
    if (ev->type == SDL_KEYDOWN && ctx && e9ui_getFocus(ctx) == self) {
        SDL_Keycode kc = ev->key.keysym.sym;
        if (kc == SDLK_PAGEUP) {
            memory_scrollRows(st, -32);
            return 1;
        }
        if (kc == SDLK_PAGEDOWN) {
            memory_scrollRows(st, 32);
            return 1;
        }
        if (kc == SDLK_UP) {
            memory_scrollRows(st, -1);
            return 1;
        }
        if (kc == SDLK_DOWN) {
            memory_scrollRows(st, 1);
            return 1;
        }
    }
    return 0;
}

static int
memory_preferredHeight(e9ui_component_t *self, e9ui_context_t *ctx, int availW)
{
    (void)self; (void)ctx; (void)availW;
    return 0;
}

static void
memory_layout(e9ui_component_t *self, e9ui_context_t *ctx, e9ui_rect_t bounds)
{
    (void)ctx;
    self->bounds = bounds;
}

static void
memory_render(e9ui_component_t *self, e9ui_context_t *ctx)
{
    if (!self || !ctx || !ctx->renderer) {
        return;
    }
    memory_view_state_t *st = (memory_view_state_t*)self->state;
    if (!st) {
        return;
    }
    SDL_Rect r = { self->bounds.x, self->bounds.y, self->bounds.w, self->bounds.h };
    SDL_SetRenderDrawColor(ctx->renderer, 20, 22, 20, 255);
    SDL_RenderFillRect(ctx->renderer, &r);
    TTF_Font *font = e9ui->theme.text.source ? e9ui->theme.text.source : ctx->font;
    if (!font || !st->data) {
        return;
    }
    int lh = TTF_FontHeight(font);
    if (lh <= 0) {
        lh = 16;
    }
    unsigned int addr = st->base;
    int pad = 8;
    int y = r.y + pad;
    if (st->error[0]) {
        SDL_Color err = {220, 80, 80, 255};
        int tw = 0, th = 0;
        SDL_Texture *t = e9ui_text_cache_getText(ctx->renderer, font, st->error, err, &tw, &th);
        if (t) {
            SDL_Rect tr = { r.x + pad, y, tw, th };
            SDL_RenderCopy(ctx->renderer, t, NULL, &tr);
        }
        y += lh;
    }
    char line[256];
    for (unsigned int off = 0; off < st->size; off += 16) {
        int n = snprintf(line, sizeof(line), "%08X: ", addr + off);
        for (unsigned int i=0; i<16; ++i) {
            if (off + i < st->size) {
                n += snprintf(line + n, sizeof(line) - n, "%02X ", st->data[off + i]);
            } else {
                n += snprintf(line + n, sizeof(line) - n, "   ");
            }
        }
        n += snprintf(line + n, sizeof(line) - n, " ");
        for (unsigned int i=0; i<16 && off + i < st->size; ++i) {
            unsigned char c = st->data[off + i];
            line[n++] = (c >= 32 && c <= 126) ? (char)c : '.';
        }
        line[n] = '\0';
        SDL_Color col = {200,220,200,255};
        int tw = 0, th = 0;
        SDL_Texture *t = e9ui_text_cache_getText(ctx->renderer, font, line, col, &tw, &th);
        if (t) {
            SDL_Rect tr = { r.x + pad, y, tw, th };
            SDL_RenderCopy(ctx->renderer, t, NULL, &tr);
        }
        y += lh;
        if (y > r.y + r.h - pad) {
            break;
        }
    }
}

static void
memory_dtor(e9ui_component_t *self, e9ui_context_t *ctx)
{
  (void)ctx;
  if (!self) {
    return;
  }
  memory_view_state_t *st = (memory_view_state_t*)self->state;
  if (st) {
    alloc_free(st->data);
  }
}

e9ui_component_t *
memory_makeComponent(void)
{
    e9ui_component_t *stack = e9ui_stack_makeVertical();
    e9ui_component_t *c = (e9ui_component_t*)alloc_calloc(1, sizeof(*c));
    if (!c) {
        return NULL;
    }
    memory_view_state_t *st = (memory_view_state_t*)alloc_calloc(1, sizeof(*st));
    if (!st) {
        alloc_free(c);
        return NULL;
    }
    c->name = "memory_view";
    c->state = st;
    c->preferredHeight = memory_preferredHeight;
    c->layout = memory_layout;
    c->render = memory_render;
    c->handleEvent = memory_handleEvent;
    c->dtor = memory_dtor;
    c->focusable = 1;
    
    uint32_t minAddr = 0;
    uint32_t maxAddr = 0x00ffffffu;
    if (memory_getAddressLimits(&minAddr, &maxAddr)) {
        st->base = minAddr;
    } else {
        st->base = 0;
    }
    st->base = memory_clampBaseForView(st, st->base);
    st->size = 16u * 32u;
    st->data = (unsigned char*)alloc_alloc(st->size);
    memory_clearData(st);
    memory_setError(st, NULL);

    st->textbox = e9ui_textbox_make(32, memory_onAddressSubmit, NULL, st);
    e9ui_setDisableVariable(st->textbox, machine_getRunningState(debugger.machine), 1);        

    e9ui_textbox_setPlaceholder(st->textbox, "Base address (hex)");
    memory_syncTextboxFromBase(st);
    
    e9ui_stack_addFixed(stack, st->textbox);

    e9ui_stack_addFlex(stack, c);

    g_memory_view_state = st;

    return stack;
}

void
memory_refreshOnBreak(void)
{
    if (!g_memory_view_state) {
        return;
    }
    unsigned int addr = 0;
    if (!memory_parseAddress(g_memory_view_state, &addr)) {
        return;
    }
    g_memory_view_state->base = addr;
    memory_fillFromram(g_memory_view_state, g_memory_view_state->base);
}
