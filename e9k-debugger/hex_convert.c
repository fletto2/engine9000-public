/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "hex_convert.h"

typedef struct hex_convert_state {
    e9ui_window_t *windowHost;
    e9ui_component_t *decimalTextbox;
    e9ui_component_t *hexTextbox;
    int winX;
    int winY;
    int winW;
    int winH;
    int winHasSaved;
    int syncing;
} hex_convert_state_t;

static hex_convert_state_t hex_convert_state = {0};

static void
hex_convert_setHexFromDecimal(void);

static void
hex_convert_setDecimalFromHex(void);

static int
hex_convert_parseInt(const char *value, int *outValue);

static e9ui_window_backend_t
hex_convert_windowBackend(void);

static e9ui_rect_t
hex_convert_defaultRect(e9ui_context_t *ctx, int contentWidthPx);

static int
hex_convert_parseInt(const char *value, int *outValue)
{
    if (!value || !outValue) {
        return 0;
    }
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (!end || end == value || *end != '\0') {
        return 0;
    }
    if (parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }
    *outValue = (int)parsed;
    return 1;
}

static e9ui_window_backend_t
hex_convert_windowBackend(void)
{
    return e9ui_window_backend_overlay;
}

static e9ui_rect_t
hex_convert_defaultRect(e9ui_context_t *ctx, int contentWidthPx)
{
    e9ui_rect_t rect = { 0, 0, contentWidthPx + e9ui_scale_px(ctx, 20), e9ui_scale_px(ctx, 140) };
    if (rect.w < e9ui_scale_px(ctx, 260)) {
        rect.w = e9ui_scale_px(ctx, 260);
    }
    if (rect.h < e9ui_scale_px(ctx, 120)) {
        rect.h = e9ui_scale_px(ctx, 120);
    }
    return rect;
}

static int
hex_convert_parseU64(const char *text, int base, uint64_t *outValue)
{
    if (!text || !outValue) {
        return 0;
    }
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (base == 16 && *p == '$') {
        p++;
    }
    if (!*p) {
        return 0;
    }
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(p, &end, base);
    if (errno != 0 || end == p) {
        return 0;
    }
    while (*end && isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return 0;
    }
    *outValue = (uint64_t)parsed;
    return 1;
}

typedef enum hex_convert_selection_kind {
    hex_convert_selection_none = 0,
    hex_convert_selection_decimal,
    hex_convert_selection_hex
} hex_convert_selection_kind_t;

static int
hex_convert_trimText(const char *src, char *dst, int dstLen)
{
    if (dst && dstLen > 0) {
        dst[0] = '\0';
    }
    if (!src || !dst || dstLen <= 0) {
        return 0;
    }
    const char *start = src;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) {
        end--;
    }
    int len = (int)(end - start);
    if (len <= 0) {
        return 0;
    }
    if (len >= dstLen) {
        len = dstLen - 1;
    }
    memcpy(dst, start, (size_t)len);
    dst[len] = '\0';
    return len;
}

static hex_convert_selection_kind_t
hex_convert_classifySelection(const char *text)
{
    if (!text || !text[0]) {
        return hex_convert_selection_none;
    }
    const char *p = text;
    if (*p == '+' || *p == '-') {
        p++;
    }
    if (!*p) {
        return hex_convert_selection_none;
    }
    if ((p[0] == '0') && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        if (!*p) {
            return hex_convert_selection_none;
        }
        while (*p) {
            if (!isxdigit((unsigned char)*p)) {
                return hex_convert_selection_none;
            }
            p++;
        }
        return hex_convert_selection_hex;
    }
    if (*p == '$') {
        p++;
        if (!*p) {
            return hex_convert_selection_none;
        }
        while (*p) {
            if (!isxdigit((unsigned char)*p)) {
                return hex_convert_selection_none;
            }
            p++;
        }
        return hex_convert_selection_hex;
    }

    int allDigits = 1;
    int allHexDigits = 1;
    int hasHexAlpha = 0;
    for (const char *q = p; *q; ++q) {
        unsigned char ch = (unsigned char)*q;
        if (!isdigit(ch)) {
            allDigits = 0;
        }
        if (!isxdigit(ch)) {
            allHexDigits = 0;
            break;
        }
        if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
            hasHexAlpha = 1;
        }
    }
    if (allDigits) {
        return hex_convert_selection_decimal;
    }
    if (allHexDigits && hasHexAlpha) {
        return hex_convert_selection_hex;
    }
    return hex_convert_selection_none;
}

static int
hex_convert_applySelection(e9ui_context_t *ctx)
{
    if (!ctx || !hex_convert_state.decimalTextbox || !hex_convert_state.hexTextbox) {
        return 0;
    }
    char raw[512];
    raw[0] = '\0';
    int found = 0;
    e9ui_component_t *focus = e9ui_getFocus(ctx);
    if (focus && focus->name && strcmp(focus->name, "e9ui_textbox") == 0) {
        if (e9ui_textbox_getSelectedText(focus, raw, (int)sizeof(raw)) > 0) {
            found = 1;
        }
    }
    if (!found) {
        if (e9ui_text_select_getSelectionText(raw, (int)sizeof(raw)) > 0) {
            found = 1;
        }
    }
    char selected[512];
    if (!found || hex_convert_trimText(raw, selected, (int)sizeof(selected)) <= 0) {
        return 0;
    }
    hex_convert_selection_kind_t kind = hex_convert_classifySelection(selected);
    if (kind == hex_convert_selection_hex) {
        e9ui_textbox_setText(hex_convert_state.hexTextbox, selected);
        hex_convert_state.syncing = 1;
        hex_convert_setDecimalFromHex();
        hex_convert_state.syncing = 0;
        e9ui_setFocus(ctx, hex_convert_state.decimalTextbox);
        e9ui_textbox_selectAllExternal(hex_convert_state.decimalTextbox);
        return 1;
    } else if (kind == hex_convert_selection_decimal) {
        e9ui_textbox_setText(hex_convert_state.decimalTextbox, selected);
        hex_convert_state.syncing = 1;
        hex_convert_setHexFromDecimal();
        hex_convert_state.syncing = 0;
        e9ui_setFocus(ctx, hex_convert_state.hexTextbox);
        e9ui_textbox_selectAllExternal(hex_convert_state.hexTextbox);
        return 1;
    }
    return 0;
}

static void
hex_convert_setHexFromDecimal(void)
{
    if (!hex_convert_state.decimalTextbox || !hex_convert_state.hexTextbox) {
        return;
    }
    const char *decimalText = e9ui_textbox_getText(hex_convert_state.decimalTextbox);
    if (!decimalText || !decimalText[0]) {
        e9ui_textbox_setText(hex_convert_state.hexTextbox, "");
        return;
    }
    uint64_t value = 0;
    if (!hex_convert_parseU64(decimalText, 10, &value)) {
        e9ui_textbox_setText(hex_convert_state.hexTextbox, "");
        return;
    }
    char hexBuffer[64];
    snprintf(hexBuffer, sizeof(hexBuffer), "0x%" PRIX64, value);
    e9ui_textbox_setText(hex_convert_state.hexTextbox, hexBuffer);
}

static void
hex_convert_setDecimalFromHex(void)
{
    if (!hex_convert_state.decimalTextbox || !hex_convert_state.hexTextbox) {
        return;
    }
    const char *hexText = e9ui_textbox_getText(hex_convert_state.hexTextbox);
    if (!hexText || !hexText[0]) {
        e9ui_textbox_setText(hex_convert_state.decimalTextbox, "");
        return;
    }
    uint64_t value = 0;
    if (!hex_convert_parseU64(hexText, 16, &value)) {
        e9ui_textbox_setText(hex_convert_state.decimalTextbox, "");
        return;
    }
    char decBuffer[64];
    snprintf(decBuffer, sizeof(decBuffer), "%" PRIu64, value);
    e9ui_textbox_setText(hex_convert_state.decimalTextbox, decBuffer);
}

static void
hex_convert_decimalChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    (void)text;
    (void)user;
    if (hex_convert_state.syncing) {
        return;
    }
    hex_convert_state.syncing = 1;
    hex_convert_setHexFromDecimal();
    hex_convert_state.syncing = 0;
}

static void
hex_convert_hexChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    (void)text;
    (void)user;
    if (hex_convert_state.syncing) {
        return;
    }
    hex_convert_state.syncing = 1;
    hex_convert_setDecimalFromHex();
    hex_convert_state.syncing = 0;
}

static void
hex_convert_closeModal(void)
{
    if (!hex_convert_state.windowHost) {
        return;
    }
    if (e9ui_windowIsOpen(hex_convert_state.windowHost)) {
        (void)e9ui_windowCaptureRectSnapshot(hex_convert_state.windowHost,
                                             e9ui ? &e9ui->ctx : NULL,
                                             &hex_convert_state.winHasSaved,
                                             &hex_convert_state.winX,
                                             &hex_convert_state.winY,
                                             &hex_convert_state.winW,
                                             &hex_convert_state.winH);
        e9ui_windowClose(hex_convert_state.windowHost);
    }
    e9ui_windowDestroy(hex_convert_state.windowHost);
    hex_convert_state.windowHost = NULL;
    e9ui_setFocus(&e9ui->ctx, NULL);
    hex_convert_state.decimalTextbox = NULL;
    hex_convert_state.hexTextbox = NULL;
    hex_convert_state.syncing = 0;
    config_saveConfig();
}

static void
hex_convert_uiClosed(e9ui_window_t *window, void *user)
{
    (void)window;
    (void)user;
    hex_convert_closeModal();
}

static void
hex_convert_show(e9ui_context_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (hex_convert_state.windowHost && e9ui_windowIsOpen(hex_convert_state.windowHost)) {
        e9ui_setFocus(ctx, hex_convert_state.decimalTextbox);
        return;
    }
    int labelWidth = 36;
    int totalWidth = 170;
    int totalWidthScaled = e9ui_scale_px(ctx, totalWidth);
    int rowGapScaled = e9ui_scale_px(ctx, 16);
    int boxPadScaled = e9ui_scale_px(ctx, 14);
    int innerWidthScaled = totalWidthScaled * 2 + rowGapScaled + boxPadScaled * 2;
    hex_convert_state.windowHost = e9ui_windowCreate(hex_convert_windowBackend());
    if (!hex_convert_state.windowHost) {
        return;
    }
    e9ui_windowSetMinSize(hex_convert_state.windowHost, 260, 120);
    e9ui_component_t *row = e9ui_hstack_make();
    e9ui_component_t *rowDecimal = e9ui_labeled_textbox_make("Dec", labelWidth, totalWidth, NULL, NULL);
    e9ui_component_t *rowHex = e9ui_labeled_textbox_make("Hex", labelWidth, totalWidth, NULL, NULL);
    if (!row || !rowDecimal || !rowHex) {
        hex_convert_closeModal();
        return;
    }
    hex_convert_state.decimalTextbox = e9ui_labeled_textbox_getTextbox(rowDecimal);
    hex_convert_state.hexTextbox = e9ui_labeled_textbox_getTextbox(rowHex);
    if (!hex_convert_state.decimalTextbox || !hex_convert_state.hexTextbox) {
        hex_convert_closeModal();
        return;
    }
    e9ui_textbox_setPlaceholder(hex_convert_state.decimalTextbox, "Decimal");
    e9ui_textbox_setPlaceholder(hex_convert_state.hexTextbox, "Hex");
    e9ui_textbox_setText(hex_convert_state.decimalTextbox, "0");
    hex_convert_state.syncing = 1;
    hex_convert_setHexFromDecimal();
    hex_convert_state.syncing = 0;
    e9ui_labeled_textbox_setOnChange(rowDecimal, hex_convert_decimalChanged, NULL);
    e9ui_labeled_textbox_setOnChange(rowHex, hex_convert_hexChanged, NULL);
    e9ui_hstack_addFixed(row, rowDecimal, totalWidthScaled);
    e9ui_hstack_addFixed(row, e9ui_spacer_make(16), rowGapScaled);
    e9ui_hstack_addFixed(row, rowHex, totalWidthScaled);

    e9ui_component_t *padded = e9ui_box_make(row);
    e9ui_box_setPadding(padded, 14);
    e9ui_component_t *center = e9ui_center_make(padded);
    e9ui_center_setSize(center, e9ui_unscale_px(ctx, innerWidthScaled), 0);
    e9ui_rect_t rect = e9ui_windowResolveOpenRect(ctx,
                                                  hex_convert_defaultRect(ctx, innerWidthScaled),
                                                  260,
                                                  120,
                                                  1,
                                                  hex_convert_state.winHasSaved ? 1 : 0,
                                                  (hex_convert_state.winHasSaved &&
                                                   hex_convert_state.winW > 0 &&
                                                   hex_convert_state.winH > 0) ? 1 : 0,
                                                  hex_convert_state.winX,
                                                  hex_convert_state.winY,
                                                  hex_convert_state.winW,
                                                  hex_convert_state.winH);
    if (!e9ui_windowOpen(hex_convert_state.windowHost,
                         "DECIMAL <-> HEX",
                         rect,
                         center,
                         hex_convert_uiClosed,
                         NULL,
                         ctx)) {
        hex_convert_closeModal();
        return;
    }
    if (!hex_convert_applySelection(ctx)) {
        e9ui_setFocus(ctx, hex_convert_state.decimalTextbox);
    }
}

int
hex_convert_isOpen(void)
{
    return (hex_convert_state.windowHost && e9ui_windowIsOpen(hex_convert_state.windowHost)) ? 1 : 0;
}

void
hex_convert_close(void)
{
    hex_convert_closeModal();
}

void
hex_convert_toggle(e9ui_context_t *ctx)
{
    if (hex_convert_isOpen()) {
        hex_convert_closeModal();
    } else {
        hex_convert_show(ctx);
    }
}

void
hex_convert_persistConfig(FILE *file)
{
    if (!file) {
        return;
    }
    if (hex_convert_state.windowHost && e9ui_windowIsOpen(hex_convert_state.windowHost)) {
        (void)e9ui_windowCaptureRectSnapshot(hex_convert_state.windowHost,
                                             e9ui ? &e9ui->ctx : NULL,
                                             &hex_convert_state.winHasSaved,
                                             &hex_convert_state.winX,
                                             &hex_convert_state.winY,
                                             &hex_convert_state.winW,
                                             &hex_convert_state.winH);
    }
    if (!hex_convert_state.winHasSaved) {
        return;
    }
    fprintf(file, "comp.hex_convert.win_x=%d\n", hex_convert_state.winX);
    fprintf(file, "comp.hex_convert.win_y=%d\n", hex_convert_state.winY);
    fprintf(file, "comp.hex_convert.win_w=%d\n", hex_convert_state.winW);
    fprintf(file, "comp.hex_convert.win_h=%d\n", hex_convert_state.winH);
}

int
hex_convert_loadConfigProperty(const char *prop, const char *value)
{
    if (!prop || !value) {
        return 0;
    }
    int parsed = 0;
    if (strcmp(prop, "win_x") == 0) {
        if (!hex_convert_parseInt(value, &parsed)) {
            return 0;
        }
        hex_convert_state.winX = parsed;
    } else if (strcmp(prop, "win_y") == 0) {
        if (!hex_convert_parseInt(value, &parsed)) {
            return 0;
        }
        hex_convert_state.winY = parsed;
    } else if (strcmp(prop, "win_w") == 0) {
        if (!hex_convert_parseInt(value, &parsed)) {
            return 0;
        }
        hex_convert_state.winW = parsed;
    } else if (strcmp(prop, "win_h") == 0) {
        if (!hex_convert_parseInt(value, &parsed)) {
            return 0;
        }
        hex_convert_state.winH = parsed;
    } else {
        return 0;
    }
    hex_convert_state.winHasSaved = 1;
    return 1;
}
