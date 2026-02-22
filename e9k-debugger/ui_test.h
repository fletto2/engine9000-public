/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <SDL.h>

typedef enum ui_test_mode {
    UI_TEST_MODE_NONE = 0,
    UI_TEST_MODE_RECORD,
    UI_TEST_MODE_COMPARE,
    UI_TEST_MODE_REMAKE
} ui_test_mode_t;

#ifndef UI_TEST_DISABLED

void
ui_test_setFolder(const char *path);

void
ui_test_setMode(ui_test_mode_t mode);

ui_test_mode_t
ui_test_getMode(void);

void
ui_test_registerRequestedMode(const char *folder, ui_test_mode_t mode);

const char *
ui_test_getFolder(void);

int
ui_test_bootstrap(void);

int
ui_test_init(void);

void
ui_test_shutdown(void);

int
ui_test_isEnabled(void);

int
ui_test_hasFailed(void);

int
ui_test_getExitCode(void);

int
ui_test_checkPlaybackComplete(void);

int
ui_test_getRecordPath(char *out, size_t cap);

int
ui_test_getUiEventPath(char *out, size_t cap);

int
ui_test_captureFrame(uint64_t frame);

int
ui_test_captureWindowFrame(uint64_t frame, SDL_Renderer *renderer);

#else

static inline void
ui_test_setFolder(const char *path)
{
    (void)path;
}

static inline void
ui_test_setMode(ui_test_mode_t mode)
{
    (void)mode;
}

static inline ui_test_mode_t
ui_test_getMode(void)
{
    return UI_TEST_MODE_NONE;
}

static inline void
ui_test_registerRequestedMode(const char *folder, ui_test_mode_t mode)
{
    (void)folder;
    (void)mode;
}

static inline const char *
ui_test_getFolder(void)
{
    return NULL;
}

static inline int
ui_test_bootstrap(void)
{
    return 1;
}

static inline int
ui_test_init(void)
{
    return 1;
}

static inline void
ui_test_shutdown(void)
{
}

static inline int
ui_test_isEnabled(void)
{
    return 0;
}

static inline int
ui_test_hasFailed(void)
{
    return 0;
}

static inline int
ui_test_getExitCode(void)
{
    return -1;
}

static inline int
ui_test_checkPlaybackComplete(void)
{
    return 0;
}

static inline int
ui_test_getRecordPath(char *out, size_t cap)
{
    (void)out;
    (void)cap;
    return 0;
}

static inline int
ui_test_getUiEventPath(char *out, size_t cap)
{
    (void)out;
    (void)cap;
    return 0;
}

static inline int
ui_test_captureFrame(uint64_t frame)
{
    (void)frame;
    return 0;
}

static inline int
ui_test_captureWindowFrame(uint64_t frame, SDL_Renderer *renderer)
{
    (void)frame;
    (void)renderer;
    return 0;
}

#endif
