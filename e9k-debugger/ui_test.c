/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <SDL.h>
#include <SDL_image.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "ui_test.h"
#include "debugger.h"
#include "debug.h"
#include "input_record.h"
#include "libretro_host.h"

static char ui_test_folder[PATH_MAX];
static int ui_test_enabled = 0;
static ui_test_mode_t ui_test_mode = UI_TEST_MODE_NONE;
static int ui_test_failed = 0;
static int ui_test_exitCode = -1;
static uint8_t *ui_test_prevFrame = NULL;
static size_t ui_test_prevStride = 0;
static int ui_test_prevWidth = 0;
static int ui_test_prevHeight = 0;
static int ui_test_prevValid = 0;
static uint8_t *ui_test_captureFrameBuf = NULL;
static size_t ui_test_captureFrameCap = 0;

void
ui_test_setFolder(const char *path)
{
    if (!path || !*path) {
        ui_test_folder[0] = '\0';
        ui_test_enabled = 0;
        return;
    }
    strncpy(ui_test_folder, path, sizeof(ui_test_folder) - 1);
    ui_test_folder[sizeof(ui_test_folder) - 1] = '\0';
}

void
ui_test_setMode(ui_test_mode_t mode)
{
    ui_test_mode = mode;
}

ui_test_mode_t
ui_test_getMode(void)
{
    return ui_test_mode;
}

void
ui_test_registerRequestedMode(const char *folder, ui_test_mode_t mode)
{
    ui_test_setFolder(folder);
    ui_test_setMode(mode);
}

const char *
ui_test_getFolder(void)
{
    return ui_test_folder[0] ? ui_test_folder : NULL;
}

int
ui_test_hasFailed(void)
{
    return ui_test_failed ? 1 : 0;
}

int
ui_test_getExitCode(void)
{
    return ui_test_exitCode;
}

static int
ui_test_makeDir(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
#ifdef _WIN32
    if (_mkdir(path) == 0) {
        return 1;
    }
    if (errno == EEXIST) {
        return 1;
    }
    return 0;
#else
    if (mkdir(path, 0755) == 0) {
        return 1;
    }
    if (errno == EEXIST) {
        return 1;
    }
    return 0;
#endif
}

static void
ui_test_clearFolder(const char *path)
{
    if (!path || !*path) {
        return;
    }
#ifdef _WIN32
    char pattern[PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s\\*.*", path);
    WIN32_FIND_DATAA data;
    HANDLE h = FindFirstFileA(pattern, &data);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }
        const char *ext = strrchr(data.cFileName, '.');
        if (!ext) {
            continue;
        }
        if (_stricmp(ext, ".png") != 0 && _stricmp(ext, ".inp") != 0 &&
            _stricmp(ext, ".evt") != 0 && _stricmp(ext, ".json") != 0) {
            continue;
        }
        char full[PATH_MAX];
        if (!debugger_platform_pathJoin(full, sizeof(full), path, data.cFileName)) {
            continue;
        }
        DeleteFileA(full);
    } while (FindNextFileA(h, &data));
    FindClose(h);
#else
    DIR *dir = opendir(path);
    if (!dir) {
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        const char *ext = strrchr(ent->d_name, '.');
        if (!ext) {
            continue;
        }
        if (strcmp(ext, ".png") != 0 && strcmp(ext, ".inp") != 0 &&
            strcmp(ext, ".evt") != 0 && strcmp(ext, ".json") != 0) {
            continue;
        }
        char full[PATH_MAX];
        if (!debugger_platform_pathJoin(full, sizeof(full), path, ent->d_name)) {
            continue;
        }
        unlink(full);
    }
    closedir(dir);
#endif
}

int
ui_test_init(void)
{
    ui_test_prevValid = 0;
    ui_test_prevStride = 0;
    ui_test_prevWidth = 0;
    ui_test_prevHeight = 0;
    ui_test_failed = 0;
    ui_test_exitCode = -1;
    if (!ui_test_folder[0]) {
        ui_test_enabled = 0;
        return 1;
    }
    if (!ui_test_makeDir(ui_test_folder)) {
        debug_error("ui-test: failed to create folder %s", ui_test_folder);
        ui_test_enabled = 0;
        return 0;
    }
    if (ui_test_mode == UI_TEST_MODE_RECORD) {
        ui_test_clearFolder(ui_test_folder);
    }
    if (ui_test_mode == UI_TEST_MODE_NONE) {
        ui_test_enabled = 0;
        return 1;
    }
    ui_test_enabled = 1;
    return 1;
}

void
ui_test_shutdown(void)
{
    ui_test_enabled = 0;
    ui_test_mode = UI_TEST_MODE_NONE;
    ui_test_failed = 0;
    ui_test_exitCode = -1;
    if (ui_test_prevFrame) {
        free(ui_test_prevFrame);
        ui_test_prevFrame = NULL;
    }
    if (ui_test_captureFrameBuf) {
        free(ui_test_captureFrameBuf);
        ui_test_captureFrameBuf = NULL;
    }
    ui_test_captureFrameCap = 0;
    ui_test_prevStride = 0;
    ui_test_prevWidth = 0;
    ui_test_prevHeight = 0;
    ui_test_prevValid = 0;
}

int
ui_test_isEnabled(void)
{
    return ui_test_enabled ? 1 : 0;
}

static void
ui_test_copyPath(char *dest, size_t cap, const char *src)
{
    if (!dest || cap == 0) {
        return;
    }
    if (!src || !*src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, cap - 1);
    dest[cap - 1] = '\0';
}

int
ui_test_bootstrap(void)
{
    if (ui_test_mode == UI_TEST_MODE_NONE) {
        return 1;
    }
    if (ui_test_mode == UI_TEST_MODE_RECORD) {
        if (debugger.playbackPath[0]) {
            debug_error("make-test: cannot use --playback with --make-test");
            return 0;
        }
    } else if (ui_test_mode == UI_TEST_MODE_COMPARE) {
        if (debugger.recordPath[0] || debugger.playbackPath[0]) {
            debug_error("test: cannot combine with --record or --playback");
            return 0;
        }
    }

    srand(0u);
    if (!ui_test_init()) {
        return 0;
    }
    input_record_setUiEventQueueMode(ui_test_mode == UI_TEST_MODE_COMPARE);

    char path[PATH_MAX];
    if (ui_test_getRecordPath(path, sizeof(path))) {
        if (ui_test_mode == UI_TEST_MODE_RECORD) {
            ui_test_copyPath(debugger.recordPath, sizeof(debugger.recordPath), path);
        } else if (ui_test_mode == UI_TEST_MODE_COMPARE) {
            ui_test_copyPath(debugger.playbackPath, sizeof(debugger.playbackPath), path);
        }
    }
    if (ui_test_getUiEventPath(path, sizeof(path))) {
        input_record_setUiEventPath(path);
    }
    return 1;
}

int
ui_test_checkPlaybackComplete(void)
{
    if (ui_test_mode != UI_TEST_MODE_COMPARE || ui_test_failed) {
        return 0;
    }
    if (!input_record_isUiEventPlaybackComplete()) {
        return 0;
    }
    ui_test_exitCode = 0;
    debug_printf("*** UI TEST PASSED ***");
    return 1;
}

int
ui_test_getRecordPath(char *out, size_t cap)
{
    if (!out || cap == 0 || !ui_test_folder[0]) {
        return 0;
    }
    return debugger_platform_pathJoin(out, cap, ui_test_folder, "uitest.inp");
}

int
ui_test_getUiEventPath(char *out, size_t cap)
{
    if (!out || cap == 0 || !ui_test_folder[0]) {
        return 0;
    }
    return debugger_platform_pathJoin(out, cap, ui_test_folder, "uitest.evt");
}

static int
ui_test_writeFrame(uint64_t frame, const uint8_t *data, int width, int height, size_t pitch)
{
    char name[64];
    snprintf(name, sizeof(name), "%llu.png", (unsigned long long)frame);
    char path[PATH_MAX];
    if (!debugger_platform_pathJoin(path, sizeof(path), ui_test_folder, name)) {
        return 0;
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)data, width, height, 32, (int)pitch, SDL_PIXELFORMAT_XRGB8888);
    if (!surface) {
        debug_error("ui-test: SDL_CreateRGBSurfaceWithFormatFrom failed: %s", SDL_GetError());
        return 0;
    }
    if (IMG_SavePNG(surface, path) != 0) {
        debug_error("ui-test: IMG_SavePNG failed: %s", IMG_GetError());
    }
    SDL_FreeSurface(surface);
    return 1;
}

static int
ui_test_writeMismatchImage(uint64_t frame, const uint8_t *data, int width, int height, size_t pitch,
                            char *outPath, size_t cap)
{
    char name[64];
    snprintf(name, sizeof(name), "mismatch-%llu.png", (unsigned long long)frame);
    char path[PATH_MAX];
    if (!debugger_platform_pathJoin(path, sizeof(path), ui_test_folder, name)) {
        return 0;
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)data, width, height, 32, (int)pitch, SDL_PIXELFORMAT_XRGB8888);
    if (!surface) {
        return 0;
    }
    if (IMG_SavePNG(surface, path) == 0 && outPath && cap > 0) {
        strncpy(outPath, path, cap - 1);
        outPath[cap - 1] = '\0';
    }
    SDL_FreeSurface(surface);
    return 1;
}

static int
ui_test_writeDiffScript(uint64_t frame, const char *refPath, const char *testPath,
                        char *outMontage, size_t cap)
{
    if (outMontage && cap > 0) {
        outMontage[0] = '\0';
    }
    if (!refPath || !*refPath || !testPath || !*testPath) {
        return 0;
    }

    char compareName[64];
    snprintf(compareName, sizeof(compareName), "mismatch-%llu-compare.png", (unsigned long long)frame);
    char comparePath[PATH_MAX];
    if (!debugger_platform_pathJoin(comparePath, sizeof(comparePath), ui_test_folder, compareName)) {
        return 0;
    }

    char montageName[64];
    snprintf(montageName, sizeof(montageName), "mismatch-%llu-triple.png", (unsigned long long)frame);
    char montagePath[PATH_MAX];
    if (!debugger_platform_pathJoin(montagePath, sizeof(montagePath), ui_test_folder, montageName)) {
        return 0;
    }

#ifdef _WIN32
    char scriptName[64];
    snprintf(scriptName, sizeof(scriptName), "mismatch-%llu.cmd", (unsigned long long)frame);
#else
    char scriptName[64];
    snprintf(scriptName, sizeof(scriptName), "mismatch-%llu.sh", (unsigned long long)frame);
#endif
    char scriptPath[PATH_MAX];
    if (!debugger_platform_pathJoin(scriptPath, sizeof(scriptPath), ui_test_folder, scriptName)) {
        return 0;
    }

    FILE *fp = fopen(scriptPath, "w");
    if (!fp) {
        return 0;
    }
#ifndef _WIN32
    fprintf(fp, "#!/bin/sh\n");
#endif
    fprintf(fp, "magick compare -metric AE \"%s\" \"%s\" \"%s\"\n",
            refPath, testPath, comparePath);
    fprintf(fp, "magick montage \"%s\" \"%s\" \"%s\" -tile 3x1 -geometry +0+0 \"%s\"\n",
            refPath, testPath, comparePath, montagePath);
    fclose(fp);

#ifdef _WIN32
    char cmdCompare[PATH_MAX * 3];
    char cmdMontage[PATH_MAX * 4];
    snprintf(cmdCompare, sizeof(cmdCompare),
             "magick compare -metric AE \"%s\" \"%s\" \"%s\"",
             refPath, testPath, comparePath);
    snprintf(cmdMontage, sizeof(cmdMontage),
             "magick montage \"%s\" \"%s\" \"%s\" -tile 3x1 -geometry +0+0 \"%s\"",
             refPath, testPath, comparePath, montagePath);
#else
    char cmdCompare[PATH_MAX * 3];
    char cmdMontage[PATH_MAX * 4];
    snprintf(cmdCompare, sizeof(cmdCompare),
             "magick compare -metric AE \"%s\" \"%s\" \"%s\" >/dev/null 2>&1",
             refPath, testPath, comparePath);
    snprintf(cmdMontage, sizeof(cmdMontage),
             "magick montage \"%s\" \"%s\" \"%s\" -tile 3x1 -geometry +0+0 \"%s\" >/dev/null 2>&1",
             refPath, testPath, comparePath, montagePath);
#endif
    system(cmdCompare);
    system(cmdMontage);

    if (outMontage && cap > 0) {
        strncpy(outMontage, montagePath, cap - 1);
        outMontage[cap - 1] = '\0';
    }
    return 1;
}

static int
ui_test_isDifferentToPrev(const uint8_t *data, int width, int height, size_t pitch)
{
    if (!ui_test_prevValid || width != ui_test_prevWidth || height != ui_test_prevHeight) {
        return 1;
    }
    const uint32_t mask = 0x00ffffffu;
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = data + (size_t)y * pitch;
        const uint8_t *prev = ui_test_prevFrame + (size_t)y * ui_test_prevStride;
        for (int x = 0; x < width; ++x) {
            const uint32_t *pa = (const uint32_t *)(row + (size_t)x * 4);
            const uint32_t *pb = (const uint32_t *)(prev + (size_t)x * 4);
            if (((*pa) & mask) != ((*pb) & mask)) {
                return 1;
            }
        }
    }
    return 0;
}

static void
ui_test_updatePrevFrame(const uint8_t *data, int width, int height, size_t pitch)
{
    if (!data || width <= 0 || height <= 0) {
        ui_test_prevValid = 0;
        return;
    }
    size_t stride = (size_t)width * 4;
    size_t bytes = stride * (size_t)height;
    if (!ui_test_prevFrame || ui_test_prevStride != stride ||
        ui_test_prevWidth != width || ui_test_prevHeight != height) {
        uint8_t *buf = (uint8_t *)realloc(ui_test_prevFrame, bytes);
        if (!buf) {
            ui_test_prevValid = 0;
            return;
        }
        ui_test_prevFrame = buf;
        ui_test_prevStride = stride;
        ui_test_prevWidth = width;
        ui_test_prevHeight = height;
    }
    for (int y = 0; y < height; ++y) {
        memcpy(ui_test_prevFrame + (size_t)y * ui_test_prevStride,
               data + (size_t)y * pitch,
               ui_test_prevStride);
    }
    ui_test_prevValid = 1;
}

static int
ui_test_compareFrame(uint64_t frame, const uint8_t *data, int width, int height, size_t pitch)
{
    char name[64];
    snprintf(name, sizeof(name), "%llu.png", (unsigned long long)frame);
    char path[PATH_MAX];
    if (!debugger_platform_pathJoin(path, sizeof(path), ui_test_folder, name)) {
        return 0;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            return 0;
        }
        char diffPath[PATH_MAX];
        char montagePath[PATH_MAX];
        diffPath[0] = '\0';
        montagePath[0] = '\0';
        ui_test_writeMismatchImage(frame, data, width, height, pitch, diffPath, sizeof(diffPath));
        ui_test_writeDiffScript(frame, path, diffPath, montagePath, sizeof(montagePath));
        debug_error("ui-test: stat failed for %s (%s)", path,
                    montagePath[0] ? montagePath : (diffPath[0] ? diffPath : "diff unavailable"));
        return 1;
    }
    SDL_Surface *loaded = IMG_Load(path);
    if (!loaded) {
        char diffPath[PATH_MAX];
        char montagePath[PATH_MAX];
        diffPath[0] = '\0';
        montagePath[0] = '\0';
        ui_test_writeMismatchImage(frame, data, width, height, pitch, diffPath, sizeof(diffPath));
        ui_test_writeDiffScript(frame, path, diffPath, montagePath, sizeof(montagePath));
        debug_error("ui-test: IMG_Load failed: %s (%s)", IMG_GetError(),
                    montagePath[0] ? montagePath : (diffPath[0] ? diffPath : "diff unavailable"));
        return 1;
    }
    SDL_Surface *converted = loaded;
    if (loaded->format->format != SDL_PIXELFORMAT_XRGB8888) {
        converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_XRGB8888, 0);
        SDL_FreeSurface(loaded);
    }
    if (!converted) {
        char diffPath[PATH_MAX];
        char montagePath[PATH_MAX];
        diffPath[0] = '\0';
        montagePath[0] = '\0';
        ui_test_writeMismatchImage(frame, data, width, height, pitch, diffPath, sizeof(diffPath));
        ui_test_writeDiffScript(frame, path, diffPath, montagePath, sizeof(montagePath));
        debug_error("ui-test: SDL_ConvertSurfaceFormat failed: %s (%s)", SDL_GetError(),
                    montagePath[0] ? montagePath : (diffPath[0] ? diffPath : "diff unavailable"));
        return 1;
    }
    int fail = 0;
    if (SDL_MUSTLOCK(converted)) {
        if (SDL_LockSurface(converted) != 0) {
            char diffPath[PATH_MAX];
            char montagePath[PATH_MAX];
            diffPath[0] = '\0';
            montagePath[0] = '\0';
            ui_test_writeMismatchImage(frame, data, width, height, pitch, diffPath, sizeof(diffPath));
            ui_test_writeDiffScript(frame, path, diffPath, montagePath, sizeof(montagePath));
            debug_error("ui-test: SDL_LockSurface failed: %s (%s)", SDL_GetError(),
                        montagePath[0] ? montagePath : (diffPath[0] ? diffPath : "diff unavailable"));
            SDL_FreeSurface(converted);
            return 1;
        }
    }
    if (converted->w != width || converted->h != height) {
        fail = 1;
    } else {
        const uint8_t *src = (const uint8_t *)converted->pixels;
        const uint32_t mask = 0x00ffffffu;
        for (int y = 0; y < height && !fail; ++y) {
            const uint8_t *row_a = src + (size_t)y * (size_t)converted->pitch;
            const uint8_t *row_b = data + (size_t)y * pitch;
            for (int x = 0; x < width; ++x) {
                const uint32_t *pa = (const uint32_t *)(row_a + (size_t)x * 4);
                const uint32_t *pb = (const uint32_t *)(row_b + (size_t)x * 4);
                if (((*pa) & mask) != ((*pb) & mask)) {
                    fail = 1;
                    break;
                }
            }
        }
    }
    if (SDL_MUSTLOCK(converted)) {
        SDL_UnlockSurface(converted);
    }
    SDL_FreeSurface(converted);
    if (fail) {
        char diffPath[PATH_MAX];
        char montagePath[PATH_MAX];
        diffPath[0] = '\0';
        montagePath[0] = '\0';
        ui_test_writeMismatchImage(frame, data, width, height, pitch, diffPath, sizeof(diffPath));
        ui_test_writeDiffScript(frame, path, diffPath, montagePath, sizeof(montagePath));
        debug_error("ui-test: mismatch at frame #%llu (%s)",
                    (unsigned long long)frame,
                    montagePath[0] ? montagePath : (diffPath[0] ? diffPath : "diff unavailable"));
        return 1;
    }
    return 0;
}

int
ui_test_captureFrame(uint64_t frame)
{
    if (!ui_test_enabled) {
        return 0;
    }
    const uint8_t *data = NULL;
    int width = 0;
    int height = 0;
    size_t pitch = 0;
    if (!libretro_host_getFrame(&data, &width, &height, &pitch)) {
        return 0;
    }
    if (ui_test_mode == UI_TEST_MODE_COMPARE) {
        int r = ui_test_compareFrame(frame, data, width, height, pitch);
        if (r == 1) {
            ui_test_failed = 1;
            ui_test_exitCode = 1;
        }
        return r;
    }
    int different = ui_test_isDifferentToPrev(data, width, height, pitch);
    if (different) {
        ui_test_writeFrame(frame, data, width, height, pitch);
    }
    ui_test_updatePrevFrame(data, width, height, pitch);
    return 0;
}

int
ui_test_captureWindowFrame(uint64_t frame, SDL_Renderer *renderer)
{
    if (!ui_test_enabled || !renderer) {
        return 0;
    }
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) {
        return 0;
    }
    size_t pitch = (size_t)width * 4;
    size_t needed = pitch * (size_t)height;
    if (needed == 0) {
        return 0;
    }
    if (!ui_test_captureFrameBuf || ui_test_captureFrameCap < needed) {
        uint8_t *buf = (uint8_t *)realloc(ui_test_captureFrameBuf, needed);
        if (!buf) {
            return 0;
        }
        ui_test_captureFrameBuf = buf;
        ui_test_captureFrameCap = needed;
    }
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_XRGB8888,
                             ui_test_captureFrameBuf, (int)pitch) != 0) {
        debug_error("ui-test: SDL_RenderReadPixels failed: %s", SDL_GetError());
        return 0;
    }
    if (ui_test_mode == UI_TEST_MODE_COMPARE) {
        int r = ui_test_compareFrame(frame, ui_test_captureFrameBuf, width, height, pitch);
        if (r == 1) {
            ui_test_failed = 1;
            ui_test_exitCode = 1;
        }
        return r;
    }
    int different = ui_test_isDifferentToPrev(ui_test_captureFrameBuf, width, height, pitch);
    if (different) {
        ui_test_writeFrame(frame, ui_test_captureFrameBuf, width, height, pitch);
    }
    ui_test_updatePrevFrame(ui_test_captureFrameBuf, width, height, pitch);
    return 0;
}
