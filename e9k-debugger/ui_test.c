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
#include "config.h"
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
static int ui_test_pendingMissingFrameValid = 0;
static uint64_t ui_test_pendingMissingFrame = 0;
static uint8_t *ui_test_captureFrameBuf = NULL;
static size_t ui_test_captureFrameCap = 0;
static const char ui_test_sessionConfigName[] = ".e9k-debugger.cfg";

static void
ui_test_clearTempConfigOnFirstRun(void);

static int
ui_test_restartCount(void)
{
    int count = debugger_getTestRestartCount();
    return count > 0 ? count : 0;
}

static void
ui_test_prefix(char *out, size_t cap)
{
    if (!out || cap == 0) {
        return;
    }
    int restartCount = ui_test_restartCount();
    if (restartCount <= 0) {
        out[0] = '\0';
        return;
    }
    snprintf(out, cap, "r%d-", restartCount);
}

static void
ui_test_formatFrameName(char *out, size_t cap, uint64_t frame)
{
    if (!out || cap == 0) {
        return;
    }
    char prefix[32];
    ui_test_prefix(prefix, sizeof(prefix));
    if (prefix[0]) {
        snprintf(out, cap, "%s%llu.png", prefix, (unsigned long long)frame);
    } else {
        snprintf(out, cap, "%llu.png", (unsigned long long)frame);
    }
}

static void
ui_test_formatMismatchName(char *out, size_t cap, uint64_t frame, const char *suffix)
{
    if (!out || cap == 0) {
        return;
    }
    char prefix[32];
    ui_test_prefix(prefix, sizeof(prefix));
    if (prefix[0]) {
        snprintf(out, cap, "%smismatch-%llu%s", prefix, (unsigned long long)frame, suffix ? suffix : "");
    } else {
        snprintf(out, cap, "mismatch-%llu%s", (unsigned long long)frame, suffix ? suffix : "");
    }
}

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

static void
ui_test_clearPngOnly(const char *path)
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
        if (!ext || _stricmp(ext, ".png") != 0) {
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
        if (!ext || strcmp(ext, ".png") != 0) {
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
    ui_test_pendingMissingFrameValid = 0;
    ui_test_pendingMissingFrame = 0;
    if (!ui_test_folder[0]) {
        ui_test_enabled = 0;
        return 1;
    }
    if (!ui_test_makeDir(ui_test_folder)) {
        debug_error("ui-test: failed to create folder %s", ui_test_folder);
        ui_test_enabled = 0;
        return 0;
    }
    ui_test_clearTempConfigOnFirstRun();
    if (ui_test_mode == UI_TEST_MODE_RECORD && ui_test_restartCount() == 0) {
        ui_test_clearFolder(ui_test_folder);
    } else if (ui_test_mode == UI_TEST_MODE_REMAKE && ui_test_restartCount() == 0) {
        ui_test_clearPngOnly(ui_test_folder);
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
    ui_test_pendingMissingFrameValid = 0;
    ui_test_pendingMissingFrame = 0;
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

static int
ui_test_copyFile(const char *srcPath, const char *dstPath)
{
    if (!srcPath || !*srcPath || !dstPath || !*dstPath) {
        return 0;
    }

    FILE *src = fopen(srcPath, "rb");
    if (!src) {
        return 0;
    }
    FILE *dst = fopen(dstPath, "wb");
    if (!dst) {
        fclose(src);
        return 0;
    }

    int ok = 1;
    char buf[8192];
    while (!feof(src)) {
        size_t n = fread(buf, 1, sizeof(buf), src);
        if (n == 0) {
            if (ferror(src)) {
                ok = 0;
            }
            break;
        }
        if (fwrite(buf, 1, n, dst) != n) {
            ok = 0;
            break;
        }
    }

    fclose(dst);
    fclose(src);
    return ok;
}

static int
ui_test_copySessionConfigForRecord(void)
{
    if (!ui_test_folder[0]) {
        return 0;
    }

    char dstPath[PATH_MAX];
    if (!debugger_platform_pathJoin(dstPath, sizeof(dstPath), ui_test_folder, ui_test_sessionConfigName)) {
        return 0;
    }

    struct stat st;
    if (stat(dstPath, &st) == 0) {
        if (!S_ISREG(st.st_mode)) {
            debug_error("ui-test: config path is not a file: %s", dstPath);
            return 0;
        }
        return 1;
    }

    const char *srcPath = debugger_defaultConfigPath();
    if (srcPath && *srcPath) {
        if (ui_test_copyFile(srcPath, dstPath)) {
            return 1;
        }
    }

    FILE *dst = fopen(dstPath, "w");
    if (!dst) {
        debug_error("ui-test: failed to create session config %s", dstPath);
        return 0;
    }
    config_persistConfig(dst);
    fclose(dst);
    debug_printf("ui-test: created session config %s", dstPath);
    return 1;
}

static void
ui_test_clearTempConfigOnFirstRun(void)
{
    if (ui_test_restartCount() != 0) {
        return;
    }
    const char *tempPath = debugger_configTempPath();
    if (!tempPath || !*tempPath) {
        return;
    }

    {
        char tempSaveDir[PATH_MAX];
        int written = snprintf(tempSaveDir, sizeof(tempSaveDir), "%s.rom", tempPath);
        if (written > 0 && (size_t)written < sizeof(tempSaveDir)) {
            struct stat st;
            if (stat(tempSaveDir, &st) == 0 && S_ISDIR(st.st_mode)) {
#ifdef _WIN32
                char pattern[PATH_MAX];
                snprintf(pattern, sizeof(pattern), "%s\\*.*", tempSaveDir);
                WIN32_FIND_DATAA data;
                HANDLE h = FindFirstFileA(pattern, &data);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
                            continue;
                        }
                        char full[PATH_MAX];
                        if (!debugger_platform_pathJoin(full, sizeof(full), tempSaveDir, data.cFileName)) {
                            continue;
                        }
                        DeleteFileA(full);
                    } while (FindNextFileA(h, &data));
                    FindClose(h);
                }
                RemoveDirectoryA(tempSaveDir);
#else
                DIR *dir = opendir(tempSaveDir);
                if (dir) {
                    struct dirent *ent;
                    while ((ent = readdir(dir)) != NULL) {
                        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                            continue;
                        }
                        char full[PATH_MAX];
                        if (!debugger_platform_pathJoin(full, sizeof(full), tempSaveDir, ent->d_name)) {
                            continue;
                        }
                        struct stat entSt;
                        if (stat(full, &entSt) != 0) {
                            continue;
                        }
                        if (S_ISDIR(entSt.st_mode)) {
                            continue;
                        }
                        unlink(full);
                    }
                    closedir(dir);
                }
                rmdir(tempSaveDir);
#endif
            }
        }
    }

    errno = 0;
    if (remove(tempPath) != 0) {
        if (errno != ENOENT) {
            debug_error("ui-test: failed to clear temp config %s: %s", tempPath, strerror(errno));
        }
        return;
    }
}

static int
ui_test_verifySessionConfigForReplay(void)
{
    if (!ui_test_folder[0]) {
        return 0;
    }

    char cfgPath[PATH_MAX];
    if (!debugger_platform_pathJoin(cfgPath, sizeof(cfgPath), ui_test_folder, ui_test_sessionConfigName)) {
        return 0;
    }

    struct stat st;
    if (stat(cfgPath, &st) != 0) {
        debug_error("ui-test: missing %s (create with --make-test first)", cfgPath);
        return 0;
    }
    if (!S_ISREG(st.st_mode)) {
        debug_error("ui-test: config path is not a file: %s", cfgPath);
        return 0;
    }
    return 1;
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
    } else if (ui_test_mode == UI_TEST_MODE_COMPARE || ui_test_mode == UI_TEST_MODE_REMAKE) {
        if (debugger.recordPath[0] || debugger.playbackPath[0]) {
            debug_error("test: cannot combine with --record or --playback");
            return 0;
        }
    }

    srand(0u);
    if (!ui_test_init()) {
        return 0;
    }
    input_record_setUiEventQueueMode(ui_test_mode == UI_TEST_MODE_COMPARE || ui_test_mode == UI_TEST_MODE_REMAKE);

    char path[PATH_MAX];
    if (ui_test_getRecordPath(path, sizeof(path))) {
        if (ui_test_mode == UI_TEST_MODE_RECORD) {
            if (!ui_test_copySessionConfigForRecord()) {
                return 0;
            }
            ui_test_copyPath(debugger.recordPath, sizeof(debugger.recordPath), path);
        } else if (ui_test_mode == UI_TEST_MODE_COMPARE || ui_test_mode == UI_TEST_MODE_REMAKE) {
            if (!ui_test_verifySessionConfigForReplay()) {
                return 0;
            }
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
    if ((ui_test_mode != UI_TEST_MODE_COMPARE && ui_test_mode != UI_TEST_MODE_REMAKE) || ui_test_failed) {
        return 0;
    }
    if (!input_record_isUiEventPlaybackComplete()) {
        return 0;
    }
    if (ui_test_pendingMissingFrameValid) {
        debug_printf("ui-test: ignoring missing final reference frame #%llu",
                     (unsigned long long)ui_test_pendingMissingFrame);
        ui_test_pendingMissingFrameValid = 0;
    }
    ui_test_exitCode = 0;
    if (ui_test_mode == UI_TEST_MODE_COMPARE) {
        debug_printf("*** UI TEST PASSED ***");
    }
    return 1;
}

int
ui_test_getRecordPath(char *out, size_t cap)
{
    if (!out || cap == 0 || !ui_test_folder[0]) {
        return 0;
    }
    char name[128];
    char prefix[32];
    ui_test_prefix(prefix, sizeof(prefix));
    if (prefix[0]) {
        snprintf(name, sizeof(name), "%suitest.inp", prefix);
    } else {
        snprintf(name, sizeof(name), "uitest.inp");
    }
    return debugger_platform_pathJoin(out, cap, ui_test_folder, name);
}

int
ui_test_getUiEventPath(char *out, size_t cap)
{
    if (!out || cap == 0 || !ui_test_folder[0]) {
        return 0;
    }
    char name[128];
    char prefix[32];
    ui_test_prefix(prefix, sizeof(prefix));
    if (prefix[0]) {
        snprintf(name, sizeof(name), "%suitest.evt", prefix);
    } else {
        snprintf(name, sizeof(name), "uitest.evt");
    }
    return debugger_platform_pathJoin(out, cap, ui_test_folder, name);
}

static int
ui_test_writeFrame(uint64_t frame, const uint8_t *data, int width, int height, size_t pitch)
{
    char name[128];
    ui_test_formatFrameName(name, sizeof(name), frame);
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
    char name[128];
    ui_test_formatMismatchName(name, sizeof(name), frame, ".png");
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

    char compareName[128];
    ui_test_formatMismatchName(compareName, sizeof(compareName), frame, "-compare.png");
    char comparePath[PATH_MAX];
    if (!debugger_platform_pathJoin(comparePath, sizeof(comparePath), ui_test_folder, compareName)) {
        return 0;
    }

    char montageName[128];
    ui_test_formatMismatchName(montageName, sizeof(montageName), frame, "-triple.png");
    char montagePath[PATH_MAX];
    if (!debugger_platform_pathJoin(montagePath, sizeof(montagePath), ui_test_folder, montageName)) {
        return 0;
    }

#ifdef _WIN32
    char scriptName[128];
    ui_test_formatMismatchName(scriptName, sizeof(scriptName), frame, ".cmd");
#else
    char scriptName[128];
    ui_test_formatMismatchName(scriptName, sizeof(scriptName), frame, ".sh");
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
    char name[128];
    ui_test_formatFrameName(name, sizeof(name), frame);
    char path[PATH_MAX];
    if (!debugger_platform_pathJoin(path, sizeof(path), ui_test_folder, name)) {
        return 0;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            ui_test_pendingMissingFrameValid = 1;
            ui_test_pendingMissingFrame = frame;
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
    int different = ui_test_isDifferentToPrev(data, width, height, pitch);
    if (ui_test_mode == UI_TEST_MODE_COMPARE) {
        int r = 0;
        if (different) {
            if (ui_test_pendingMissingFrameValid) {
                debug_error("ui-test: missing reference frame #%llu is not final (next changed frame #%llu)",
                            (unsigned long long)ui_test_pendingMissingFrame,
                            (unsigned long long)frame);
                ui_test_pendingMissingFrameValid = 0;
                ui_test_failed = 1;
                ui_test_exitCode = 1;
                ui_test_updatePrevFrame(data, width, height, pitch);
                return 1;
            }
            r = ui_test_compareFrame(frame, data, width, height, pitch);
            if (r == 1) {
                ui_test_failed = 1;
                ui_test_exitCode = 1;
            }
        }
        ui_test_updatePrevFrame(data, width, height, pitch);
        return r;
    }
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
    SDL_RenderFlush(renderer);
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_XRGB8888,
                             ui_test_captureFrameBuf, (int)pitch) != 0) {
        debug_error("ui-test: SDL_RenderReadPixels failed: %s", SDL_GetError());
        return 0;
    }
    int different = ui_test_isDifferentToPrev(ui_test_captureFrameBuf, width, height, pitch);
    if (ui_test_mode == UI_TEST_MODE_COMPARE) {
        int r = 0;
        if (different) {
            if (ui_test_pendingMissingFrameValid) {
                debug_error("ui-test: missing reference frame #%llu is not final (next changed frame #%llu)",
                            (unsigned long long)ui_test_pendingMissingFrame,
                            (unsigned long long)frame);
                ui_test_pendingMissingFrameValid = 0;
                ui_test_failed = 1;
                ui_test_exitCode = 1;
                ui_test_updatePrevFrame(ui_test_captureFrameBuf, width, height, pitch);
                return 1;
            }
            r = ui_test_compareFrame(frame, ui_test_captureFrameBuf, width, height, pitch);
            if (r == 1) {
                ui_test_failed = 1;
                ui_test_exitCode = 1;
            }
        }
        ui_test_updatePrevFrame(ui_test_captureFrameBuf, width, height, pitch);
        return r;
    }
    if (different) {
        ui_test_writeFrame(frame, ui_test_captureFrameBuf, width, height, pitch);
    }
    ui_test_updatePrevFrame(ui_test_captureFrameBuf, width, height, pitch);
    return 0;
}
