/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "debugger.h"
#include "tinyfiledialogs.h"
#include "ui_test.h"

static const char debugger_testConfigName[] = ".e9k-debugger.cfg";
static const char debugger_testTempConfigPrefix[] = "e9k-debugger-test-";
static const char debugger_testTempConfigSuffix[] = ".cfg";

static int
debugger_platform_configExists(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISREG(st.st_mode) ? 1 : 0;
}

static uint64_t
debugger_platform_hashPath(const char *path)
{
    uint64_t hash = 1469598103934665603ull;
    if (!path) {
        return hash;
    }
    while (*path) {
        hash ^= (uint8_t)(*path++);
        hash *= 1099511628211ull;
    }
    return hash;
}

int
debugger_platform_pathJoin(char *out, size_t cap, const char *dir, const char *name)
{
    if (!out || cap == 0 || !dir || !*dir || !name || !*name) {
        return 0;
    }
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    int need_sep = (dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\');
    size_t total = dlen + (need_sep ? 1 : 0) + nlen;
    if (total + 1 > cap) {
        return 0;
    }
    memcpy(out, dir, dlen);
    size_t pos = dlen;
    if (need_sep) {
        out[pos++] = '/';
    }
    memcpy(out + pos, name, nlen);
    out[pos + nlen] = '\0';
    return 1;
}

int
debugger_platform_scanFolder(const char *folder, int (*cb)(const char *path, void *user), void *user)
{
    if (!folder || !*folder || !cb) {
        return 0;
    }
    DIR *dir = opendir(folder);
    if (!dir) {
        return 0;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        char full[PATH_MAX];
        if (!debugger_platform_pathJoin(full, sizeof(full), folder, ent->d_name)) {
            continue;
        }
        if (!cb(full, user)) {
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return 1;
}

int
debugger_platform_caseInsensitivePaths(void)
{
    return 0;
}

char
debugger_platform_preferredPathSeparator(void)
{
    return '/';
}

int
debugger_platform_formatToolCommand(char *out,
                                    size_t cap,
                                    const char *toolPath,
                                    const char *toolArgs,
                                    const char *targetPath,
                                    int suppressStderr)
{
    if (!out || cap == 0 || !toolPath || !*toolPath || !targetPath || !*targetPath) {
        return 0;
    }
    out[0] = '\0';
    const char *args = (toolArgs && *toolArgs) ? toolArgs : NULL;
    if (args) {
        if (suppressStderr) {
            snprintf(out, cap, "%s %s '%s' 2>/dev/null", toolPath, args, targetPath);
        } else {
            snprintf(out, cap, "%s %s '%s'", toolPath, args, targetPath);
        }
    } else {
        if (suppressStderr) {
            snprintf(out, cap, "%s '%s' 2>/dev/null", toolPath, targetPath);
        } else {
            snprintf(out, cap, "%s '%s'", toolPath, targetPath);
        }
    }
    return out[0] != '\0';
}

int
debugger_platform_getExeDir(char *out, size_t cap)
{
    if (!out || cap == 0) {
        return 0;
    }
#ifdef __APPLE__
    char path[PATH_MAX];
    uint32_t sz = (uint32_t)sizeof(path);
    if (_NSGetExecutablePath(path, &sz) != 0) {
        return 0;
    }
    char resolvedPath[PATH_MAX];
    const char *resolved = realpath(path, resolvedPath);
    const char *fullPath = resolved ? resolved : path;
    size_t len = strlen(fullPath);
    while (len > 0 && fullPath[len - 1] != '/') {
        len--;
    }
    if (len == 0) {
        return 0;
    }
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(out, fullPath, len);
    out[len] = '\0';
    return 1;
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count <= 0) {
        return 0;
    }
    path[count] = '\0';
    size_t len = (size_t)count;
    while (len > 0 && path[len - 1] != '/') {
        len--;
    }
    if (len == 0) {
        return 0;
    }
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return 1;
#endif
}

int
debugger_platform_isExecutableFile(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    if (!S_ISREG(st.st_mode)) {
        return 0;
    }
    if (access(path, X_OK) != 0) {
        return 0;
    }
    return 1;
}

int
debugger_platform_getHomeDir(char *out, size_t cap)
{
    if (!out || cap == 0) {
        return 0;
    }
    const char *home = getenv("HOME");
    if (!home || !*home) {
        out[0] = '\0';
        return 0;
    }
    size_t len = strlen(home);
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(out, home, len);
    out[len] = '\0';
    return 1;
}

int
debugger_platform_getCurrentDir(char *out, size_t cap)
{
    if (!out || cap == 0) {
        return 0;
    }
    if (!getcwd(out, cap)) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

char
debugger_platform_pathListSeparator(void)
{
    return ':';
}

int
debugger_platform_makeDir(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    if (mkdir(path, 0755) == 0) {
        return 1;
    }
    return errno == EEXIST ? 1 : 0;
}

int
debugger_platform_removeDir(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    return rmdir(path) == 0 ? 1 : 0;
}

int
debugger_platform_makeTempFilePath(char *out, size_t cap, const char *prefix, const char *suffix)
{
    if (!out || cap == 0) {
        return 0;
    }
    const char *tmpDir = getenv("TMPDIR");
    if (!tmpDir || !*tmpDir) {
        tmpDir = "/tmp";
    }
    const char *filePrefix = (prefix && *prefix) ? prefix : "e9k";
    const char *fileSuffix = suffix ? suffix : "";
    int written = snprintf(out, cap, "%s/%s-XXXXXX%s", tmpDir, filePrefix, fileSuffix);
    if (written <= 0 || (size_t)written >= cap) {
        return 0;
    }
    int suffixLen = (int)strlen(fileSuffix);
    int fd = mkstemps(out, suffixLen);
    if (fd < 0) {
        return 0;
    }
    close(fd);
    return 1;
}

int
debugger_platform_replaceFile(const char *srcPath, const char *dstPath)
{
    if (!srcPath || !*srcPath || !dstPath || !*dstPath) {
        return 0;
    }
    return rename(srcPath, dstPath) == 0 ? 1 : 0;
}

int
debugger_platform_normalizeMouseWheelY(int value)
{
    return value;
}

int
debugger_platform_glCompositeNeedsOpenGLHint(void)
{
#ifdef __APPLE__
    return 1;
#else
    return 0;
#endif
}

const char *
debugger_platform_windowIconAssetPath(void)
{
#ifdef __APPLE__
    return "assets/icons/osx/engine9000.png";
#else
    return "assets/icons/osx/engine9000.png";
#endif
}

const char *
debugger_platform_selectFolderDialog(const char *title, const char *defaultPath)
{
    return tinyfd_selectFolderDialog(title, defaultPath);
}

const char *
debugger_platform_openFileDialog(const char *title,
                                 const char *defaultPathAndFile,
                                 int numOfFilterPatterns,
                                 const char * const *filterPatterns,
                                 const char *singleFilterDescription,
                                 int allowMultipleSelects)
{
    return tinyfd_openFileDialog(title,
                                 defaultPathAndFile,
                                 numOfFilterPatterns,
                                 filterPatterns,
                                 singleFilterDescription,
                                 allowMultipleSelects);
}

const char *
debugger_platform_saveFileDialog(const char *title,
                                 const char *defaultPathAndFile,
                                 int numOfFilterPatterns,
                                 const char * const *filterPatterns,
                                 const char *singleFilterDescription)
{
    return tinyfd_saveFileDialog(title,
                                 defaultPathAndFile,
                                 numOfFilterPatterns,
                                 filterPatterns,
                                 singleFilterDescription);
}

void *
debugger_platform_loadSharedLibrary(const char *path)
{
    if (!path || !*path) {
        return NULL;
    }
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void
debugger_platform_closeSharedLibrary(void *handle)
{
    if (!handle) {
        return;
    }
    dlclose(handle);
}

void *
debugger_platform_loadSharedSymbol(void *handle, const char *name)
{
    if (!handle || !name || !*name) {
        return NULL;
    }
    dlerror();
    return dlsym(handle, name);
}

ssize_t
debugger_platform_getline(char **lineptr, size_t *n, FILE *stream)
{
    return getline(lineptr, n, stream);
}

char *
debugger_configPath(void)
{
    static char pathbuf[1024];
    ui_test_mode_t mode = ui_test_getMode();
    const char *testTempPath = debugger_configTempPath();
    if (mode != UI_TEST_MODE_NONE && debugger_getLoadTestTempConfig() && debugger_platform_configExists(testTempPath)) {
        return (char *)testTempPath;
    }
    if (mode == UI_TEST_MODE_RECORD || mode == UI_TEST_MODE_COMPARE || mode == UI_TEST_MODE_REMAKE) {
        const char *folder = ui_test_getFolder();
        if (folder && *folder) {
            if (debugger_platform_pathJoin(pathbuf, sizeof(pathbuf), folder, debugger_testConfigName)) {
                return pathbuf;
            }
        }
    }
    return debugger_defaultConfigPath();
}

char *
debugger_defaultConfigPath(void)
{
    static char pathbuf[1024];
    const char *home = getenv("HOME");
    if (!home || !*home) {
        return NULL;
    }
    snprintf(pathbuf, sizeof(pathbuf), "%s/.e9k-debugger.cfg", home);
    return pathbuf;
}

char *
debugger_configTempPath(void)
{
    static char pathbuf[1024];
    static char namebuf[96];
    const char *folder = ui_test_getFolder();
    if (!folder || !*folder) {
        return NULL;
    }
    const char *tmpDir = getenv("TMPDIR");
    if (!tmpDir || !*tmpDir) {
        tmpDir = "/tmp";
    }
    uint64_t folderHash = debugger_platform_hashPath(folder);
    snprintf(namebuf, sizeof(namebuf), "%s%016llx%s",
             debugger_testTempConfigPrefix,
             (unsigned long long)folderHash,
             debugger_testTempConfigSuffix);
    if (!debugger_platform_pathJoin(pathbuf, sizeof(pathbuf), tmpDir, namebuf)) {
        return NULL;
    }
    return pathbuf;
}

void
debugger_platform_setDefaults(e9k_neogeo_config_t *config)
{
    if (!config) {
        return;
    }
    snprintf(config->libretro.systemDir, sizeof(config->libretro.systemDir), "./system");
    snprintf(config->libretro.saveDir, sizeof(config->libretro.saveDir), "./saves");
    snprintf(config->libretro.sourceDir, sizeof(config->libretro.sourceDir), ".");
    snprintf(config->libretro.toolchainPrefix, sizeof(config->libretro.toolchainPrefix), "m68k-neogeo-elf");
    config->libretro.audioBufferMs = 250;
    config->skipBiosLogo = 0;
    strncpy(config->systemType, "aes", sizeof(config->systemType) - 1);
    config->systemType[sizeof(config->systemType) - 1] = '\0';
    config->libretro.exePath[0] = '\0';
}

void
debugger_platform_setDefaultsAmiga(e9k_amiga_config_t *config)
{
    if (!config) {
        return;
    }
    snprintf(config->libretro.systemDir, sizeof(config->libretro.systemDir), "./system");
    snprintf(config->libretro.saveDir, sizeof(config->libretro.saveDir), "./saves");
    snprintf(config->libretro.sourceDir, sizeof(config->libretro.sourceDir), ".");
    snprintf(config->libretro.toolchainPrefix, sizeof(config->libretro.toolchainPrefix), "m68k-amigaos-");
    config->libretro.audioBufferMs = 250;
    config->libretro.exePath[0] = '\0';
}

void
debugger_platform_setDefaultsMegaDrive(e9k_megadrive_config_t *config)
{
    if (!config) {
        return;
    }
#if E9K_ENABLE_MEGADRIVE
    snprintf(config->libretro.systemDir, sizeof(config->libretro.systemDir), "./system");
    snprintf(config->libretro.saveDir, sizeof(config->libretro.saveDir), "./saves");
    snprintf(config->libretro.sourceDir, sizeof(config->libretro.sourceDir), ".");
    snprintf(config->libretro.toolchainPrefix, sizeof(config->libretro.toolchainPrefix), "m68k-elf");
    config->libretro.audioBufferMs = 250;
    config->libretro.exePath[0] = '\0';
    config->romFolder[0] = '\0';
#else
    memset(config, 0, sizeof(*config));
#endif
}
