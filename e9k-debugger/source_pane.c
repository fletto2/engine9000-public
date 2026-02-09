/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include "e9ui.h"
#include "config.h"
#include "debugger.h"
#include "source.h"
#include "source_pane.h"
#include "dasm.h"
#include "addr2line.h"
#include "machine.h"
#include "breakpoints.h"
#include "libretro_host.h"
#include "debug.h"
#include "file.h"
#include "syntax_highlight.h"
#include "print_eval.h"

typedef struct source_pane_state {
    source_pane_mode_t viewMode; // C vs ASM vs HEX
    int               scrollLine; // 1-based first line for C view
    int               scrollLineValid;
    int               scrollIndex; // 0-based first instruction for ASM view
    int               scrollIndexValid;
    uint64_t          lastPcAddr;
    uint64_t          lastResolvedPc;
    uint64_t          overrideAddr;
    int               overrideActive;
    int               frozenActive;
    uint64_t          frozenPcAddr;
    int               frozenAsmStartIndex;
    int               frozenAsmMaxLines;
    char            **frozenAsmLines;
    uint64_t         *frozenAsmAddrs;
    int               frozenAsmCount;
    char              curSrcPath[PATH_MAX];
    int               curSrcLine;
    char*             toggleBtnMeta;
    char*             lockBtnMeta;  
    int               gutterPending;
    int               gutterLine;
    uint32_t          gutterAddr;
    int               gutterDownX;
    int               gutterDownY;
    source_pane_mode_t gutterMode;
    int               bucketSource;
    int               bucketAddr;
    char*             fileSelectMeta;
    char             *manualSrcPath;
    int               manualSrcActive;
    char            **sourceFiles;
    char            **sourceLabels;
    e9ui_textbox_option_t *sourceOptions;
    int               sourceFileCount;
    int               sourceFileCap;
    int               sourceFilesLoaded;
    char              sourceFilesElf[PATH_MAX];
    char              sourceFilesToolchain[PATH_MAX];
    char             *functionSelectMeta;
    char            **sourceFunctionNames;
    char            **sourceFunctionFiles;
    char            **sourceFunctionLabels;
    char            **sourceFunctionValues;
    int              *sourceFunctionLines;
    e9ui_textbox_option_t *sourceFunctionOptions;
    int               sourceFunctionCount;
    int               sourceFunctionCap;
    int               sourceFunctionsLoaded;
    char              sourceFunctionsElf[PATH_MAX];
    char              sourceFunctionsToolchain[PATH_MAX];
    char              sourceFunctionsFile[PATH_MAX];
    int               functionScrollLock;
    e9ui_component_t *ownerPane;
    char              hoverExpr[256];
    char              hoverTip[1024];
    int               hoverLine;
    int               hoverCol;
    uint32_t          hoverPc;
    uint32_t          hoverTick;
    int               hoverActive;
} source_pane_state_t;

typedef struct source_pane_line_metrics {
    int maxLines;
    int lineHeight;
    int innerHeight;
} source_pane_line_metrics_t;

static void
source_pane_updateSourceLocation(source_pane_state_t *st, int allowWhileRunning);

static void
source_pane_followCurrent(source_pane_state_t *st);

static void
source_pane_setModeInternal(e9ui_component_t *comp, source_pane_mode_t mode, int enforceElfValid);

static void
source_pane_refreshSourceFiles(e9ui_component_t *comp, source_pane_state_t *st);

static void
source_pane_syncFileSelect(e9ui_component_t *comp, source_pane_state_t *st);

static void
source_pane_syncFunctionSelect(e9ui_component_t *comp, source_pane_state_t *st);

static void
source_pane_clearFunctionScrollLock(source_pane_state_t *st);

static void
source_pane_trackCurrentFunction(e9ui_component_t *comp, source_pane_state_t *st, const char *path, int line);

static const char *
source_pane_basename(const char *path)
{
    if (!path || !path[0]) {
        return NULL;
    }
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    const char *best = slash > back ? slash : back;
    return best ? best + 1 : path;
}

static int
source_pane_isAbsolutePath(const char *path)
{
    if (!path || !path[0]) {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }
    if (isalpha((unsigned char)path[0]) && path[1] == ':') {
        return 1;
    }
    return 0;
}

static void
source_pane_resolveSourcePath(const char *path, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!path || !path[0]) {
        return;
    }
    if (source_pane_isAbsolutePath(path)) {
        strncpy(out, path, out_cap - 1);
        out[out_cap - 1] = '\0';
        return;
    }
    const char *src = debugger.libretro.sourceDir;
    if (!src || !src[0]) {
        strncpy(out, path, out_cap - 1);
        out[out_cap - 1] = '\0';
        return;
    }
    size_t src_len = strlen(src);
    int need_sep = 1;
    if (src_len > 0) {
        char c = src[src_len - 1];
        if (c == '/' || c == '\\') {
            need_sep = 0;
        }
    }
    snprintf(out, out_cap, "%s%s%s", src, need_sep ? "/" : "", path);
}

static int
source_pane_parseHex(const char *s, uint32_t *out)
{
    if (!s || !*s || !out) {
        return 0;
    }
    char buf[32];
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        len--;
    }
    if (len > 0 && s[len - 1] == ':') {
        len--;
    }
    if (len == 0 || len >= sizeof(buf)) {
        return 0;
    }
    memcpy(buf, s, len);
    buf[len] = '\0';
    const char *p = buf;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    if (!*p) {
        return 0;
    }
    for (const char *q = p; *q; ++q) {
        if (!isxdigit((unsigned char)*q)) {
            return 0;
        }
    }
    errno = 0;
    unsigned long v = strtoul(buf, NULL, 16);
    if (errno != 0) {
        return 0;
    }
    *out = (uint32_t)(v & 0x00ffffffu);
    return 1;
}

static int
source_pane_parseHex64(const char *s, uint64_t *out)
{
    if (!s || !*s || !out) {
        return 0;
    }
    char buf[32];
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        len--;
    }
    if (len > 0 && s[len - 1] == ':') {
        len--;
    }
    if (len == 0 || len >= sizeof(buf)) {
        return 0;
    }
    memcpy(buf, s, len);
    buf[len] = '\0';
    const char *p = buf;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    if (!*p) {
        return 0;
    }
    for (const char *q = p; *q; ++q) {
        if (!isxdigit((unsigned char)*q)) {
            return 0;
        }
    }
    errno = 0;
    unsigned long long v = strtoull(buf, NULL, 16);
    if (errno != 0) {
        return 0;
    }
    *out = (uint64_t)v;
    return 1;
}

static int
source_pane_fileMatches(const char *a, const char *b)
{
    if (!a || !b) {
        return 0;
    }
    if (strcmp(a, b) == 0) {
        return 1;
    }
    const char *ba = source_pane_basename(a);
    const char *bb = source_pane_basename(b);
    if (ba && bb && strcmp(ba, bb) == 0) {
        return 1;
    }
    const char *src = debugger.libretro.sourceDir;
    if (src && *src) {
        size_t src_len = strlen(src);
        if (strncmp(a, src, src_len) == 0) {
            const char *rest = a + src_len;
            if (*rest == '/' || *rest == '\\') {
                rest++;
            }
            if (strcmp(rest, b) == 0) {
                return 1;
            }
        }
        if (strncmp(b, src, src_len) == 0) {
            const char *rest = b + src_len;
            if (*rest == '/' || *rest == '\\') {
                rest++;
            }
            if (strcmp(rest, a) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int
source_pane_hasCSourceExtension(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    const char *dot = strrchr(path, '.');
    if (!dot || !dot[1]) {
        return 0;
    }
    return strcmp(dot, ".c") == 0 || strcmp(dot, ".cc") == 0 ||
           strcmp(dot, ".cpp") == 0 || strcmp(dot, ".cxx") == 0;
}

static char *
source_pane_parseValueAfterColon(const char *line)
{
    if (!line) {
        return NULL;
    }
    const char *colon = strrchr(line, ':');
    if (!colon || !colon[1]) {
        return NULL;
    }
    const char *start = colon + 1;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (!*start) {
        return NULL;
    }
    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) {
        len--;
    }
    if (len == 0) {
        return NULL;
    }
    char *out = (char*)alloc_calloc(len + 1, 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static void
source_pane_clearSourceFiles(source_pane_state_t *st)
{
    if (!st) {
        return;
    }
    if (st->sourceFiles) {
        for (int i = 0; i < st->sourceFileCount; ++i) {
            alloc_free(st->sourceFiles[i]);
            alloc_free(st->sourceLabels[i]);
        }
    }
    alloc_free(st->sourceFiles);
    alloc_free(st->sourceLabels);
    alloc_free(st->sourceOptions);
    st->sourceFiles = NULL;
    st->sourceLabels = NULL;
    st->sourceOptions = NULL;
    st->sourceFileCount = 0;
    st->sourceFileCap = 0;
    st->sourceFilesLoaded = 0;
    st->sourceFilesElf[0] = '\0';
    st->sourceFilesToolchain[0] = '\0';
}

static void
source_pane_clearSourceFunctions(source_pane_state_t *st)
{
    if (!st) {
        return;
    }
    if (st->sourceFunctionNames) {
        for (int i = 0; i < st->sourceFunctionCount; ++i) {
            alloc_free(st->sourceFunctionNames[i]);
            alloc_free(st->sourceFunctionFiles[i]);
            alloc_free(st->sourceFunctionLabels[i]);
            alloc_free(st->sourceFunctionValues[i]);
        }
    }
    alloc_free(st->sourceFunctionNames);
    alloc_free(st->sourceFunctionFiles);
    alloc_free(st->sourceFunctionLabels);
    alloc_free(st->sourceFunctionValues);
    alloc_free(st->sourceFunctionLines);
    alloc_free(st->sourceFunctionOptions);
    st->sourceFunctionNames = NULL;
    st->sourceFunctionFiles = NULL;
    st->sourceFunctionLabels = NULL;
    st->sourceFunctionValues = NULL;
    st->sourceFunctionLines = NULL;
    st->sourceFunctionOptions = NULL;
    st->sourceFunctionCount = 0;
    st->sourceFunctionCap = 0;
    st->sourceFunctionsLoaded = 0;
    st->sourceFunctionsElf[0] = '\0';
    st->sourceFunctionsToolchain[0] = '\0';
    st->sourceFunctionsFile[0] = '\0';
}

static int
source_pane_parseFunctionValue(const char *value, int *out_line, const char **out_file)
{
    if (!value || !*value || !out_line) {
        return 0;
    }
    const char *sep = strchr(value, '|');
    if (!sep) {
        return 0;
    }
    size_t len = (size_t)(sep - value);
    if (len == 0 || len > 15) {
        return 0;
    }
    char line_buf[16];
    memcpy(line_buf, value, len);
    line_buf[len] = '\0';
    int line = (int)strtol(line_buf, NULL, 10);
    if (line <= 0) {
        return 0;
    }
    *out_line = line;
    if (out_file) {
        const char *file = sep + 1;
        *out_file = strchr(file, '|') ? file : NULL;
    }
    return 1;
}

static int
source_pane_addSourceFunction(source_pane_state_t *st, const char *filePath, const char *name, int line)
{
    if (!st || !filePath || !*filePath || !name || !*name || line <= 0) {
        return 0;
    }
    for (int i = 0; i < st->sourceFunctionCount; ++i) {
        if (st->sourceFunctionLines[i] == line &&
            strcmp(st->sourceFunctionFiles[i], filePath) == 0 &&
            strcmp(st->sourceFunctionNames[i], name) == 0) {
            return 0;
        }
    }
    if (st->sourceFunctionCount >= st->sourceFunctionCap) {
        int next_cap = st->sourceFunctionCap > 0 ? st->sourceFunctionCap * 2 : 64;
        char **next_names = (char**)alloc_calloc((size_t)next_cap, sizeof(*next_names));
        char **next_files = (char**)alloc_calloc((size_t)next_cap, sizeof(*next_files));
        char **next_labels = (char**)alloc_calloc((size_t)next_cap, sizeof(*next_labels));
        char **next_values = (char**)alloc_calloc((size_t)next_cap, sizeof(*next_values));
        int *next_lines = (int*)alloc_calloc((size_t)next_cap, sizeof(*next_lines));
        e9ui_textbox_option_t *next_options =
            (e9ui_textbox_option_t*)alloc_calloc((size_t)next_cap, sizeof(*next_options));
        if (!next_names || !next_files || !next_labels || !next_values || !next_lines || !next_options) {
            alloc_free(next_names);
            alloc_free(next_files);
            alloc_free(next_labels);
            alloc_free(next_values);
            alloc_free(next_lines);
            alloc_free(next_options);
            return 0;
        }
        if (st->sourceFunctionCount > 0) {
            size_t count = (size_t)st->sourceFunctionCount;
            memcpy(next_names, st->sourceFunctionNames, sizeof(*next_names) * count);
            memcpy(next_files, st->sourceFunctionFiles, sizeof(*next_files) * count);
            memcpy(next_labels, st->sourceFunctionLabels, sizeof(*next_labels) * count);
            memcpy(next_values, st->sourceFunctionValues, sizeof(*next_values) * count);
            memcpy(next_lines, st->sourceFunctionLines, sizeof(*next_lines) * count);
            memcpy(next_options, st->sourceFunctionOptions, sizeof(*next_options) * count);
        }
        alloc_free(st->sourceFunctionNames);
        alloc_free(st->sourceFunctionFiles);
        alloc_free(st->sourceFunctionLabels);
        alloc_free(st->sourceFunctionValues);
        alloc_free(st->sourceFunctionLines);
        alloc_free(st->sourceFunctionOptions);
        st->sourceFunctionNames = next_names;
        st->sourceFunctionFiles = next_files;
        st->sourceFunctionLabels = next_labels;
        st->sourceFunctionValues = next_values;
        st->sourceFunctionLines = next_lines;
        st->sourceFunctionOptions = next_options;
        st->sourceFunctionCap = next_cap;
    }

    char value_buf[PATH_MAX + 64];
    snprintf(value_buf, sizeof(value_buf), "%d|%s|%s", line, filePath, name);
    char *name_dup = alloc_strdup(name);
    char *file_dup = alloc_strdup(filePath);
    char *label_dup = alloc_strdup(name);
    char *value_dup = alloc_strdup(value_buf);
    if (!name_dup || !file_dup || !label_dup || !value_dup) {
        alloc_free(name_dup);
        alloc_free(file_dup);
        alloc_free(label_dup);
        alloc_free(value_dup);
        return 0;
    }

    int insert_at = st->sourceFunctionCount;
    while (insert_at > 0) {
        int prev = insert_at - 1;
        int prev_line = st->sourceFunctionLines[prev];
        int cmp = strcasecmp(st->sourceFunctionNames[prev], name_dup);
        if (cmp < 0 || (cmp == 0 && prev_line <= line)) {
            break;
        }
        st->sourceFunctionNames[insert_at] = st->sourceFunctionNames[prev];
        st->sourceFunctionFiles[insert_at] = st->sourceFunctionFiles[prev];
        st->sourceFunctionLabels[insert_at] = st->sourceFunctionLabels[prev];
        st->sourceFunctionValues[insert_at] = st->sourceFunctionValues[prev];
        st->sourceFunctionLines[insert_at] = st->sourceFunctionLines[prev];
        st->sourceFunctionOptions[insert_at] = st->sourceFunctionOptions[prev];
        insert_at--;
    }

    st->sourceFunctionNames[insert_at] = name_dup;
    st->sourceFunctionFiles[insert_at] = file_dup;
    st->sourceFunctionLabels[insert_at] = label_dup;
    st->sourceFunctionValues[insert_at] = value_dup;
    st->sourceFunctionLines[insert_at] = line;
    st->sourceFunctionOptions[insert_at].value = value_dup;
    st->sourceFunctionOptions[insert_at].label = label_dup;
    st->sourceFunctionCount++;
    return 1;
}

static int
source_pane_addSourceFile(source_pane_state_t *st, const char *path)
{
    if (!st || !path || !*path) {
        return 0;
    }
    char resolved[PATH_MAX];
    source_pane_resolveSourcePath(path, resolved, sizeof(resolved));
    if (!source_pane_hasCSourceExtension(resolved)) {
        return 0;
    }
    for (int i = 0; i < st->sourceFileCount; ++i) {
        if (source_pane_fileMatches(st->sourceFiles[i], resolved)) {
            return 0;
        }
    }
    if (st->sourceFileCount >= st->sourceFileCap) {
        int nextCap = st->sourceFileCap > 0 ? st->sourceFileCap * 2 : 32;
        char **nextFiles = (char**)alloc_calloc((size_t)nextCap, sizeof(*nextFiles));
        char **nextLabels = (char**)alloc_calloc((size_t)nextCap, sizeof(*nextLabels));
        e9ui_textbox_option_t *nextOptions =
            (e9ui_textbox_option_t*)alloc_calloc((size_t)nextCap, sizeof(*nextOptions));
        if (!nextFiles || !nextLabels || !nextOptions) {
            alloc_free(nextFiles);
            alloc_free(nextLabels);
            alloc_free(nextOptions);
            return 0;
        }
        if (st->sourceFileCount > 0) {
            memcpy(nextFiles, st->sourceFiles, sizeof(*nextFiles) * (size_t)st->sourceFileCount);
            memcpy(nextLabels, st->sourceLabels, sizeof(*nextLabels) * (size_t)st->sourceFileCount);
            memcpy(nextOptions, st->sourceOptions, sizeof(*nextOptions) * (size_t)st->sourceFileCount);
        }
        alloc_free(st->sourceFiles);
        alloc_free(st->sourceLabels);
        alloc_free(st->sourceOptions);
        st->sourceFiles = nextFiles;
        st->sourceLabels = nextLabels;
        st->sourceOptions = nextOptions;
        st->sourceFileCap = nextCap;
    }
    char *pathDup = alloc_strdup(resolved);
    if (!pathDup) {
        return 0;
    }
    const char *base = source_pane_basename(resolved);
    char *labelDup = alloc_strdup(base && *base ? base : resolved);
    if (!labelDup) {
        alloc_free(pathDup);
        return 0;
    }
    int idx = st->sourceFileCount++;
    st->sourceFiles[idx] = pathDup;
    st->sourceLabels[idx] = labelDup;
    st->sourceOptions[idx].value = st->sourceFiles[idx];
    st->sourceOptions[idx].label = st->sourceLabels[idx];
    return 1;
}

static void
source_pane_prependBlankSourceOption(source_pane_state_t *st)
{
    if (!st) {
        return;
    }
    if (st->sourceFileCount > 0 && st->sourceFiles && st->sourceFiles[0] && st->sourceFiles[0][0] == '\0') {
        return;
    }
    if (!st->sourceFiles || !st->sourceOptions || !st->sourceLabels) {
        int cap = st->sourceFileCap > 0 ? st->sourceFileCap : 4;
        st->sourceFiles = (char**)alloc_calloc((size_t)cap, sizeof(*st->sourceFiles));
        st->sourceLabels = (char**)alloc_calloc((size_t)cap, sizeof(*st->sourceLabels));
        st->sourceOptions = (e9ui_textbox_option_t*)alloc_calloc((size_t)cap, sizeof(*st->sourceOptions));
        if (!st->sourceFiles || !st->sourceLabels || !st->sourceOptions) {
            alloc_free(st->sourceFiles);
            alloc_free(st->sourceLabels);
            alloc_free(st->sourceOptions);
            st->sourceFiles = NULL;
            st->sourceLabels = NULL;
            st->sourceOptions = NULL;
            st->sourceFileCount = 0;
            st->sourceFileCap = 0;
            return;
        }
        st->sourceFileCap = cap;
    } else if (st->sourceFileCount >= st->sourceFileCap) {
        int nextCap = st->sourceFileCap > 0 ? st->sourceFileCap * 2 : 32;
        char **nextFiles = (char**)alloc_calloc((size_t)nextCap, sizeof(*nextFiles));
        char **nextLabels = (char**)alloc_calloc((size_t)nextCap, sizeof(*nextLabels));
        e9ui_textbox_option_t *nextOptions =
            (e9ui_textbox_option_t*)alloc_calloc((size_t)nextCap, sizeof(*nextOptions));
        if (!nextFiles || !nextLabels || !nextOptions) {
            alloc_free(nextFiles);
            alloc_free(nextLabels);
            alloc_free(nextOptions);
            return;
        }
        memcpy(nextFiles, st->sourceFiles, sizeof(*nextFiles) * (size_t)st->sourceFileCount);
        memcpy(nextLabels, st->sourceLabels, sizeof(*nextLabels) * (size_t)st->sourceFileCount);
        memcpy(nextOptions, st->sourceOptions, sizeof(*nextOptions) * (size_t)st->sourceFileCount);
        alloc_free(st->sourceFiles);
        alloc_free(st->sourceLabels);
        alloc_free(st->sourceOptions);
        st->sourceFiles = nextFiles;
        st->sourceLabels = nextLabels;
        st->sourceOptions = nextOptions;
        st->sourceFileCap = nextCap;
    }
    for (int i = st->sourceFileCount; i > 0; --i) {
        st->sourceFiles[i] = st->sourceFiles[i - 1];
        st->sourceLabels[i] = st->sourceLabels[i - 1];
        st->sourceOptions[i] = st->sourceOptions[i - 1];
    }
    st->sourceFiles[0] = alloc_strdup("");
    st->sourceLabels[0] = alloc_strdup("");
    if (!st->sourceFiles[0] || !st->sourceLabels[0]) {
        alloc_free(st->sourceFiles[0]);
        alloc_free(st->sourceLabels[0]);
        for (int i = 0; i < st->sourceFileCount; ++i) {
            st->sourceFiles[i] = st->sourceFiles[i + 1];
            st->sourceLabels[i] = st->sourceLabels[i + 1];
            st->sourceOptions[i] = st->sourceOptions[i + 1];
        }
        return;
    }
    st->sourceOptions[0].value = st->sourceFiles[0];
    st->sourceOptions[0].label = st->sourceLabels[0];
    st->sourceFileCount++;
}

static int
source_pane_collectReadelfFiles(source_pane_state_t *st, const char *elfPath)
{
    if (!st || !elfPath || !*elfPath) {
        return 0;
    }
    char readelf[PATH_MAX];
    if (!debugger_toolchainBuildBinary(readelf, sizeof(readelf), "readelf")) {
        debug_error("source_pane: failed to build readelf tool name (prefix='%s')",
                    debugger.libretro.toolchainPrefix);
        return 0;
    }
    char readelfExe[PATH_MAX];
    if (!file_findInPath(readelf, readelfExe, sizeof(readelfExe))) {
        debug_error("source_pane: readelf not found: '%s' (prefix='%s')",
                    readelf, debugger.libretro.toolchainPrefix);
        return 0;
    }
    char cmd[PATH_MAX * 2];
    if (!debugger_platform_formatToolCommand(cmd, sizeof(cmd), readelfExe, "--debug-dump=info", elfPath, 1)) {
        return 0;
    }
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return 0;
    }

    int added = 0;
    char line[1024];
    int cuOpen = 0;
    int cuDepth = 0;
    char cuName[PATH_MAX];
    char cuDir[PATH_MAX];
    cuName[0] = '\0';
    cuDir[0] = '\0';

    while (fgets(line, sizeof(line), fp)) {
        int depth = -1;
        if (sscanf(line, " <%d><", &depth) == 1) {
            int isCompileUnit = strstr(line, "DW_TAG_compile_unit") != NULL;
            if (cuOpen && (isCompileUnit || depth <= cuDepth)) {
                if (cuName[0]) {
                    char fullPath[PATH_MAX];
                    if (cuDir[0] && !source_pane_isAbsolutePath(cuName)) {
                        snprintf(fullPath, sizeof(fullPath), "%s/%s", cuDir, cuName);
                    } else {
                        strncpy(fullPath, cuName, sizeof(fullPath) - 1);
                        fullPath[sizeof(fullPath) - 1] = '\0';
                    }
                    added += source_pane_addSourceFile(st, fullPath);
                }
                cuOpen = 0;
                cuName[0] = '\0';
                cuDir[0] = '\0';
            }
            if (isCompileUnit) {
                cuOpen = 1;
                cuDepth = depth;
                cuName[0] = '\0';
                cuDir[0] = '\0';
                continue;
            }
        }
        if (!cuOpen) {
            continue;
        }
        if (!cuName[0] && strstr(line, "DW_AT_name")) {
            char *name = source_pane_parseValueAfterColon(line);
            if (name) {
                if (strcmp(name, "<artificial>") != 0) {
                    strncpy(cuName, name, sizeof(cuName) - 1);
                    cuName[sizeof(cuName) - 1] = '\0';
                }
                alloc_free(name);
            }
            continue;
        }
        if (!cuDir[0] && strstr(line, "DW_AT_comp_dir")) {
            char *dir = source_pane_parseValueAfterColon(line);
            if (dir) {
                strncpy(cuDir, dir, sizeof(cuDir) - 1);
                cuDir[sizeof(cuDir) - 1] = '\0';
                alloc_free(dir);
            }
            continue;
        }
    }

    if (cuOpen && cuName[0]) {
        char fullPath[PATH_MAX];
        if (cuDir[0] && !source_pane_isAbsolutePath(cuName)) {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", cuDir, cuName);
        } else {
            strncpy(fullPath, cuName, sizeof(fullPath) - 1);
            fullPath[sizeof(fullPath) - 1] = '\0';
        }
        added += source_pane_addSourceFile(st, fullPath);
    }

    pclose(fp);
    return added;
}

static int
source_pane_collectStabsFiles(source_pane_state_t *st, const char *elfPath)
{
    if (!st || !elfPath || !*elfPath) {
        return 0;
    }
    char objdump[PATH_MAX];
    if (!debugger_toolchainBuildBinary(objdump, sizeof(objdump), "objdump")) {
        debug_error("source_pane: failed to build objdump tool name (prefix='%s')",
                    debugger.libretro.toolchainPrefix);
        return 0;
    }
    char objdumpExe[PATH_MAX];
    if (!file_findInPath(objdump, objdumpExe, sizeof(objdumpExe))) {
        debug_error("source_pane: objdump not found: '%s' (prefix='%s')",
                    objdump, debugger.libretro.toolchainPrefix);
        return 0;
    }
    char cmd[PATH_MAX * 2];
    if (!debugger_platform_formatToolCommand(cmd, sizeof(cmd), objdumpExe, "-G", elfPath, 1)) {
        return 0;
    }
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return 0;
    }

    int added = 0;
    char currentDir[PATH_MAX];
    currentDir[0] = '\0';
    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        char *tokens[12];
        int count = 0;
        char *cursor = line;
        while (*cursor && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (!*cursor) {
            continue;
        }
        while (count < (int)(sizeof(tokens) / sizeof(tokens[0]))) {
            while (*cursor && isspace((unsigned char)*cursor)) {
                cursor++;
            }
            if (!*cursor) {
                break;
            }
            tokens[count++] = cursor;
            while (*cursor && !isspace((unsigned char)*cursor)) {
                cursor++;
            }
            if (*cursor) {
                *cursor++ = '\0';
            }
        }
        if (count < 7) {
            continue;
        }
        const char *stabType = tokens[1];
        const char *stabStr = tokens[count - 1];
        if (!stabType || !stabStr || !*stabStr) {
            continue;
        }
        if (strcmp(stabType, "SO") != 0 && strcmp(stabType, "SOL") != 0) {
            continue;
        }
        if (strcmp(stabStr, "./") == 0 || strcmp(stabStr, ".\\") == 0) {
            strncpy(currentDir, stabStr, sizeof(currentDir) - 1);
            currentDir[sizeof(currentDir) - 1] = '\0';
            continue;
        }
        size_t len = strlen(stabStr);
        if (strcmp(stabType, "SO") == 0 && len > 0 &&
            (stabStr[len - 1] == '/' || stabStr[len - 1] == '\\')) {
            strncpy(currentDir, stabStr, sizeof(currentDir) - 1);
            currentDir[sizeof(currentDir) - 1] = '\0';
            continue;
        }
        char fullPath[PATH_MAX];
        if (!source_pane_isAbsolutePath(stabStr) && currentDir[0]) {
            snprintf(fullPath, sizeof(fullPath), "%s%s", currentDir, stabStr);
        } else {
            strncpy(fullPath, stabStr, sizeof(fullPath) - 1);
            fullPath[sizeof(fullPath) - 1] = '\0';
        }
        added += source_pane_addSourceFile(st, fullPath);
    }
    pclose(fp);
    return added;
}

static void
source_pane_syncFileSelect(e9ui_component_t *comp, source_pane_state_t *st)
{
    if (!comp || !st || !st->fileSelectMeta) {
        return;
    }
    e9ui_component_t *select = e9ui_child_find(comp, st->fileSelectMeta);
    if (!select) {
        return;
    }
    e9ui_setHidden(select, st->viewMode == source_pane_mode_c ? 0 : 1);
    if (st->viewMode != source_pane_mode_c) {
        return;
    }
    int editingSelect = (e9ui && e9ui_getFocus(&e9ui->ctx) == select) ? 1 : 0;
    if (!editingSelect) {
        e9ui_textbox_setOptions(select, st->sourceOptions, st->sourceFileCount);
    }
    select->disabled = st->sourceFileCount <= 1 ? 1 : 0;

    const char *displayPath = NULL;
    if (st->manualSrcActive && st->manualSrcPath) {
        displayPath = st->manualSrcPath;
    } else if (st->curSrcPath[0]) {
        displayPath = st->curSrcPath;
    }
    if (!displayPath || !*displayPath) {
        e9ui_textbox_setSelectedValue(select, "");
        return;
    }
    if (editingSelect) {
        return;
    }
    for (int i = 0; i < st->sourceFileCount; ++i) {
        if (source_pane_fileMatches(st->sourceFiles[i], displayPath)) {
            e9ui_textbox_setSelectedValue(select, st->sourceFiles[i]);
            return;
        }
    }
    e9ui_textbox_setSelectedValue(select, "");
}

static void
source_pane_syncFunctionSelect(e9ui_component_t *comp, source_pane_state_t *st)
{
    if (!comp || !st || !st->functionSelectMeta) {
        return;
    }
    e9ui_component_t *select = e9ui_child_find(comp, st->functionSelectMeta);
    if (!select) {
        return;
    }
    e9ui_setHidden(select, st->viewMode == source_pane_mode_c ? 0 : 1);
    if (st->viewMode != source_pane_mode_c) {
        return;
    }
    int editingSelect = (e9ui && e9ui_getFocus(&e9ui->ctx) == select) ? 1 : 0;
    if (!editingSelect) {
        e9ui_textbox_setOptions(select, st->sourceFunctionOptions, st->sourceFunctionCount);
    }
    select->disabled = st->sourceFunctionCount <= 0 ? 1 : 0;
}

static void
source_pane_clearFunctionScrollLock(source_pane_state_t *st)
{
    if (!st) {
        return;
    }
    st->functionScrollLock = 0;
}

static void
source_pane_trackCurrentFunction(e9ui_component_t *comp, source_pane_state_t *st, const char *path, int line)
{
    if (!comp || !st || !path || !path[0] || line <= 0 || !st->functionSelectMeta) {
        return;
    }
    if (st->sourceFunctionCount <= 0) {
        return;
    }
    if (st->sourceFunctionsFile[0] && !source_pane_fileMatches(st->sourceFunctionsFile, path)) {
        return;
    }
    e9ui_component_t *select = e9ui_child_find(comp, st->functionSelectMeta);
    if (!select) {
        return;
    }
    if (e9ui && e9ui_getFocus(&e9ui->ctx) == select) {
        return;
    }

    int best = -1;
    for (int i = 0; i < st->sourceFunctionCount; ++i) {
        if (!source_pane_fileMatches(st->sourceFunctionFiles[i], path)) {
            continue;
        }
        if (st->sourceFunctionLines[i] <= line) {
            if (best < 0 || st->sourceFunctionLines[i] >= st->sourceFunctionLines[best]) {
                best = i;
            }
        }
    }
    if (best < 0) {
        for (int i = 0; i < st->sourceFunctionCount; ++i) {
            if (source_pane_fileMatches(st->sourceFunctionFiles[i], path)) {
                best = i;
                break;
            }
        }
    }
    if (best < 0) {
        for (int i = 0; i < st->sourceFunctionCount; ++i) {
            best = i;
            if (st->sourceFunctionLines[i] <= line) {
                if (st->sourceFunctionLines[i] >= st->sourceFunctionLines[best]) {
                    best = i;
                }
            }
        }
    }
    if (best < 0) {
        return;
    }
    e9ui_textbox_setSelectedValue(select, st->sourceFunctionValues[best]);
}

static int
source_pane_collectFunctionSymbols(source_pane_state_t *st, const char *elf_path, const char *source_file)
{
    if (!st || !elf_path || !*elf_path || !debugger.elfValid) {
        return 0;
    }

    char readelf[PATH_MAX];
    if (!debugger_toolchainBuildBinary(readelf, sizeof(readelf), "readelf")) {
        return 0;
    }
    char readelf_exe[PATH_MAX];
    if (!file_findInPath(readelf, readelf_exe, sizeof(readelf_exe))) {
        return 0;
    }

    char cmd[PATH_MAX * 2];
    if (!debugger_platform_formatToolCommand(cmd, sizeof(cmd), readelf_exe, "-Ws", elf_path, 1)) {
        return 0;
    }
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return 0;
    }

    int added = 0;
    char line[2048];
    char resolved_source[PATH_MAX];
    if (source_file && *source_file) {
        source_pane_resolveSourcePath(source_file, resolved_source, sizeof(resolved_source));
    } else {
        resolved_source[0] = '\0';
    }
    if (!addr2line_start(elf_path)) {
        pclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *tokens[12];
        int count = 0;
        char *cursor = line;
        while (*cursor && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (!*cursor || !isdigit((unsigned char)*cursor)) {
            continue;
        }
        while (count < (int)(sizeof(tokens) / sizeof(tokens[0]))) {
            while (*cursor && isspace((unsigned char)*cursor)) {
                cursor++;
            }
            if (!*cursor) {
                break;
            }
            tokens[count++] = cursor;
            while (*cursor && !isspace((unsigned char)*cursor)) {
                cursor++;
            }
            if (*cursor) {
                *cursor++ = '\0';
            }
        }
        if (count < 8) {
            continue;
        }
        if (strcmp(tokens[3], "FUNC") != 0) {
            continue;
        }
        if (strcmp(tokens[6], "UND") == 0) {
            continue;
        }
        const char *symbol_name = tokens[7];
        if (!symbol_name || !*symbol_name || strcmp(symbol_name, "<null>") == 0) {
            continue;
        }
        uint64_t symbol_addr = 0;
        if (!source_pane_parseHex64(tokens[1], &symbol_addr) || symbol_addr == 0) {
            continue;
        }

        char resolved_file[PATH_MAX];
        char function_name[1024];
        int function_line = 0;
        if (!addr2line_resolveDetailed(symbol_addr, resolved_file, sizeof(resolved_file),
                                       &function_line, function_name, sizeof(function_name))) {
            continue;
        }
        if (function_line <= 0 || !resolved_file[0]) {
            continue;
        }
        char resolved_path[PATH_MAX];
        source_pane_resolveSourcePath(resolved_file, resolved_path, sizeof(resolved_path));
        if (resolved_source[0] && !source_pane_fileMatches(resolved_path, resolved_source)) {
            continue;
        }
        const char *display_name = function_name[0] ? function_name : symbol_name;
        if (strcmp(display_name, "??") == 0) {
            continue;
        }
        added += source_pane_addSourceFunction(st, resolved_path, display_name, function_line);
    }

    pclose(fp);
    return added;
}

static int
source_pane_parseStabStringName(const char *stab_str, char *out_name, size_t cap)
{
    if (!stab_str || !*stab_str || !out_name || cap == 0) {
        return 0;
    }
    out_name[0] = '\0';
    const char *end = strchr(stab_str, ':');
    if (!end) {
        end = stab_str + strlen(stab_str);
    }
    size_t len = (size_t)(end - stab_str);
    if (len == 0) {
        return 0;
    }
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(out_name, stab_str, len);
    out_name[len] = '\0';
    return out_name[0] != '\0';
}

static int
source_pane_collectStabsFunctions(source_pane_state_t *st, const char *elf_path, const char *source_file)
{
    if (!st || !elf_path || !*elf_path || !debugger.elfValid) {
        return 0;
    }
    char objdump[PATH_MAX];
    if (!debugger_toolchainBuildBinary(objdump, sizeof(objdump), "objdump")) {
        return 0;
    }
    char objdump_exe[PATH_MAX];
    if (!file_findInPath(objdump, objdump_exe, sizeof(objdump_exe))) {
        return 0;
    }
    char cmd[PATH_MAX * 2];
    if (!debugger_platform_formatToolCommand(cmd, sizeof(cmd), objdump_exe, "-G", elf_path, 1)) {
        return 0;
    }
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return 0;
    }

    int added = 0;
    char line[2048];
    char resolved_source[PATH_MAX];
    if (source_file && *source_file) {
        source_pane_resolveSourcePath(source_file, resolved_source, sizeof(resolved_source));
    } else {
        resolved_source[0] = '\0';
    }
    if (!addr2line_start(elf_path)) {
        pclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *tokens[12];
        int count = 0;
        char *cursor = line;
        while (*cursor && isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (!*cursor) {
            continue;
        }
        while (count < (int)(sizeof(tokens) / sizeof(tokens[0]))) {
            while (*cursor && isspace((unsigned char)*cursor)) {
                ++cursor;
            }
            if (!*cursor) {
                break;
            }
            tokens[count++] = cursor;
            while (*cursor && !isspace((unsigned char)*cursor)) {
                ++cursor;
            }
            if (*cursor) {
                *cursor++ = '\0';
            }
        }
        if (count < 7) {
            continue;
        }
        const char *stab_type = tokens[1];
        const char *stab_str = tokens[count - 1];
        if (!stab_type || !stab_str || strcmp(stab_type, "FUN") != 0) {
            continue;
        }
        char parsed_name[1024];
        if (!source_pane_parseStabStringName(stab_str, parsed_name, sizeof(parsed_name))) {
            continue;
        }

        uint64_t n_value = 0;
        if (!source_pane_parseHex64(tokens[4], &n_value) || n_value == 0) {
            continue;
        }
        char resolved_file[PATH_MAX];
        char function_name[1024];
        int function_line = 0;
        if (!addr2line_resolveDetailed(n_value, resolved_file, sizeof(resolved_file),
                                       &function_line, function_name, sizeof(function_name))) {
            continue;
        }
        if (function_line <= 0 || !resolved_file[0]) {
            continue;
        }
        char resolved_path[PATH_MAX];
        source_pane_resolveSourcePath(resolved_file, resolved_path, sizeof(resolved_path));
        if (resolved_source[0] && !source_pane_fileMatches(resolved_path, resolved_source)) {
            continue;
        }
        const char *display_name = function_name[0] ? function_name : parsed_name;
        if (!display_name[0] || strcmp(display_name, "??") == 0) {
            continue;
        }
        added += source_pane_addSourceFunction(st, resolved_path, display_name, function_line);
    }

    pclose(fp);
    return added;
}

static void
source_pane_refreshSourceFunctions(e9ui_component_t *comp, source_pane_state_t *st, const char *source_file)
{
    if (!comp || !st) {
        return;
    }
    e9ui_component_t *select = NULL;
    if (st->functionSelectMeta) {
        select = e9ui_child_find(comp, st->functionSelectMeta);
    }
    const char *elf = debugger.libretro.exePath;
    const char *toolchain = debugger.libretro.toolchainPrefix;
    char resolved_source_file[PATH_MAX];
    resolved_source_file[0] = '\0';
    if (source_file && *source_file) {
        source_pane_resolveSourcePath(source_file, resolved_source_file, sizeof(resolved_source_file));
    }
    if (!debugger.elfValid || !elf || !*elf) {
        if (select) {
            e9ui_textbox_setOptions(select, NULL, 0);
        }
        source_pane_clearSourceFunctions(st);
        source_pane_syncFunctionSelect(comp, st);
        return;
    }

    if (st->sourceFunctionsLoaded &&
        strcmp(st->sourceFunctionsElf, elf) == 0 &&
        strcmp(st->sourceFunctionsToolchain, toolchain ? toolchain : "") == 0 &&
        strcmp(st->sourceFunctionsFile, resolved_source_file) == 0) {
        source_pane_syncFunctionSelect(comp, st);
        return;
    }

    if (select) {
        e9ui_textbox_setOptions(select, NULL, 0);
    }
    source_pane_clearSourceFunctions(st);
    int added = source_pane_collectFunctionSymbols(st, elf, source_file);
    if (added == 0) {
        added += source_pane_collectStabsFunctions(st, elf, source_file);
    }
    st->sourceFunctionsLoaded = 1;
    strncpy(st->sourceFunctionsElf, elf, sizeof(st->sourceFunctionsElf) - 1);
    st->sourceFunctionsElf[sizeof(st->sourceFunctionsElf) - 1] = '\0';
    if (toolchain) {
        strncpy(st->sourceFunctionsToolchain, toolchain, sizeof(st->sourceFunctionsToolchain) - 1);
        st->sourceFunctionsToolchain[sizeof(st->sourceFunctionsToolchain) - 1] = '\0';
    } else {
        st->sourceFunctionsToolchain[0] = '\0';
    }
    if (resolved_source_file[0]) {
        strncpy(st->sourceFunctionsFile, resolved_source_file, sizeof(st->sourceFunctionsFile) - 1);
        st->sourceFunctionsFile[sizeof(st->sourceFunctionsFile) - 1] = '\0';
    } else {
        st->sourceFunctionsFile[0] = '\0';
    }
    source_pane_syncFunctionSelect(comp, st);
}

static void
source_pane_refreshSourceFiles(e9ui_component_t *comp, source_pane_state_t *st)
{
    if (!comp || !st) {
        return;
    }
    const char *elf = debugger.libretro.exePath;
    const char *toolchain = debugger.libretro.toolchainPrefix;
    e9ui_component_t *select = NULL;
    if (st->fileSelectMeta) {
        select = e9ui_child_find(comp, st->fileSelectMeta);
    }
    if (!debugger.elfValid || !elf || !*elf) {
        if (select) {
            e9ui_textbox_setOptions(select, NULL, 0);
        }
        source_pane_clearSourceFiles(st);
        source_pane_syncFileSelect(comp, st);
        return;
    }
    if (st->sourceFilesLoaded &&
        strcmp(st->sourceFilesElf, elf) == 0 &&
        strcmp(st->sourceFilesToolchain, toolchain ? toolchain : "") == 0) {
        if (select) {
            e9ui_setHidden(select, st->viewMode == source_pane_mode_c ? 0 : 1);
            if (st->viewMode == source_pane_mode_c) {
                if (e9ui && e9ui_getFocus(&e9ui->ctx) == select) {
                    return;
                }
                const char *displayPath = NULL;
                if (st->manualSrcActive && st->manualSrcPath) {
                    displayPath = st->manualSrcPath;
                } else if (st->curSrcPath[0]) {
                    displayPath = st->curSrcPath;
                }
                if (!displayPath || !*displayPath) {
                    e9ui_textbox_setSelectedValue(select, "");
                } else {
                    for (int i = 0; i < st->sourceFileCount; ++i) {
                        if (source_pane_fileMatches(st->sourceFiles[i], displayPath)) {
                            e9ui_textbox_setSelectedValue(select, st->sourceFiles[i]);
                            break;
                        }
                    }
                }
            }
        }
        return;
    }

    if (select) {
        e9ui_textbox_setOptions(select, NULL, 0);
    }
    source_pane_clearSourceFiles(st);
    (void)source_pane_collectReadelfFiles(st, elf);
    (void)source_pane_collectStabsFiles(st, elf);
    int foundSourceFiles = st->sourceFileCount;
    if (foundSourceFiles <= 0) {
        debug_error("source_pane: no source files collected (elf='%s', sourceDir='%s', toolchain='%s')",
                    elf,
                    debugger.libretro.sourceDir,
                    debugger.libretro.toolchainPrefix);
    }
    source_pane_prependBlankSourceOption(st);
    st->sourceFilesLoaded = 1;
    strncpy(st->sourceFilesElf, elf, sizeof(st->sourceFilesElf) - 1);
    st->sourceFilesElf[sizeof(st->sourceFilesElf) - 1] = '\0';
    if (toolchain) {
        strncpy(st->sourceFilesToolchain, toolchain, sizeof(st->sourceFilesToolchain) - 1);
        st->sourceFilesToolchain[sizeof(st->sourceFilesToolchain) - 1] = '\0';
    } else {
        st->sourceFilesToolchain[0] = '\0';
    }
    source_pane_syncFileSelect(comp, st);
}

static int
source_pane_resolveFileLine(const char *elf, const char *file, int line_no, uint32_t *out_addr)
{
    if (!elf || !*elf || !debugger.elfValid || !file || !*file || line_no <= 0 || !out_addr) {
        return 0;
    }
    char cmd[PATH_MAX * 2];
    char objdump[PATH_MAX];
    if (!debugger_toolchainBuildBinary(objdump, sizeof(objdump), "objdump")) {
        debug_error("break: failed to resolve objdump");
        return 0;
    }
    char objdumpExe[PATH_MAX];
    if (!file_findInPath(objdump, objdumpExe, sizeof(objdumpExe))) {
        debug_error("break: objdump not found in PATH: %s", objdump);
        return 0;
    }
    if (!debugger_platform_formatToolCommand(cmd, sizeof(cmd), objdumpExe, "-l -d", elf, 0)) {
        return 0;
    }
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        debug_error("break: failed to run objdump");
        return 0;
    }
    char line[1024];
    int want_addr = 0;
    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }
        if (line[0] == '\0') {
            want_addr = 0;
            continue;
        }
        if (line[0] != ' ') {
            const char *colon = strrchr(line, ':');
            if (!colon || !colon[1]) {
                want_addr = 0;
                continue;
            }
            int got_line = atoi(colon + 1);
            if (got_line != line_no) {
                want_addr = 0;
                continue;
            }
            char file_buf[PATH_MAX];
            size_t len = (size_t)(colon - line);
            if (len >= sizeof(file_buf)) {
                len = sizeof(file_buf) - 1;
            }
            memcpy(file_buf, line, len);
            file_buf[len] = '\0';
            want_addr = source_pane_fileMatches(file_buf, file);
            continue;
        }
        if (want_addr) {
            char addr_buf[32];
            const char *p = line;
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }
            size_t i = 0;
            while (*p && !isspace((unsigned char)*p) && i + 1 < sizeof(addr_buf)) {
                addr_buf[i++] = *p++;
            }
            addr_buf[i] = '\0';
            uint32_t addr = 0;
            if (source_pane_parseHex(addr_buf, &addr)) {
                *out_addr = addr;
                pclose(fp);
                return 1;
            }
        }
    }
    pclose(fp);
    return 0;
}

static machine_breakpoint_t *
source_pane_findBreakpointForLine(const char *path, int line,
                                  const machine_breakpoint_t *bps, int count)
{
    if (!path || line <= 0) {
        return NULL;
    }
    for (int i = 0; i < count; ++i) {
        machine_breakpoint_t *bp = (machine_breakpoint_t*)&bps[i];
        if (bp->line == line && source_pane_fileMatches(bp->file, path)) {
            return bp;
        }
    }
    return NULL;
}


static source_pane_line_metrics_t
source_pane_computeLineMetrics(e9ui_component_t *self, TTF_Font *font, int padPx)
{
    source_pane_line_metrics_t out = {0};
    if (!self || !font) {
        out.lineHeight = 16;
        out.maxLines = 1;
        return out;
    }
    out.lineHeight = TTF_FontHeight(font);
    if (out.lineHeight <= 0) {
        out.lineHeight = 16;
    }
    out.innerHeight = self->bounds.h - padPx * 2;
    if (out.innerHeight <= 0) {
        out.maxLines = 0;
        return out;
    }
    out.maxLines = out.innerHeight / out.lineHeight;
    if (out.maxLines <= 0) {
        out.maxLines = 1;
    }
    return out;
}

static TTF_Font *
source_pane_resolveFont(const e9ui_context_t *ctx)
{
    if (e9ui->theme.text.source) {
        return e9ui->theme.text.source;
    }
    if (ctx) {
        return ctx->font;
    }
    return NULL;
}

static int
source_pane_pointInBounds(const e9ui_component_t *comp, int x, int y)
{
    if (!comp) {
        return 0;
    }
    return x >= comp->bounds.x && x < comp->bounds.x + comp->bounds.w &&
           y >= comp->bounds.y && y < comp->bounds.y + comp->bounds.h;
}

static void
source_pane_pushRenderClip(e9ui_context_t *ctx, const SDL_Rect *clipRect, SDL_bool *hadClip, SDL_Rect *prevClip)
{
    if (!ctx || !ctx->renderer || !clipRect || !hadClip || !prevClip) {
        return;
    }
    *hadClip = SDL_RenderIsClipEnabled(ctx->renderer);
    if (*hadClip) {
        SDL_RenderGetClipRect(ctx->renderer, prevClip);
    }
    if (clipRect->w > 0 && clipRect->h > 0) {
        SDL_RenderSetClipRect(ctx->renderer, clipRect);
    } else {
        SDL_RenderSetClipRect(ctx->renderer, NULL);
    }
}

static void
source_pane_popRenderClip(e9ui_context_t *ctx, SDL_bool hadClip, const SDL_Rect *prevClip)
{
    if (!ctx || !ctx->renderer) {
        return;
    }
    if (hadClip && prevClip) {
        SDL_RenderSetClipRect(ctx->renderer, prevClip);
    } else {
        SDL_RenderSetClipRect(ctx->renderer, NULL);
    }
}

static void
source_pane_renderStatusMessage(e9ui_context_t *ctx, TTF_Font *font, SDL_Rect area, int padPx,
                                SDL_Color color, const char *msg)
{
    if (!ctx || !ctx->renderer || !font || !msg || !msg[0]) {
        return;
    }
    int tw = 0;
    int th = 0;
    SDL_Texture *t = e9ui_text_cache_getText(ctx->renderer, font, msg, color, &tw, &th);
    if (!t) {
        return;
    }
    int y = area.y + area.h - padPx - th;
    if (y < area.y + padPx) {
        y = area.y + padPx;
    }
    SDL_Rect r = {area.x + padPx, y, tw, th};
    SDL_RenderCopy(ctx->renderer, t, NULL, &r);
}

static SDL_Color
source_pane_syntaxColor(syntax_highlight_kind_t kind)
{
    switch (kind) {
        case syntax_highlight_kind_keyword:
            return (SDL_Color){220, 170, 90, 255};
        case syntax_highlight_kind_type:
            return (SDL_Color){120, 190, 255, 255};
        case syntax_highlight_kind_string:
            return (SDL_Color){200, 210, 120, 255};
        case syntax_highlight_kind_comment:
            return (SDL_Color){120, 150, 120, 255};
        case syntax_highlight_kind_number:
            return (SDL_Color){210, 150, 120, 255};
        case syntax_highlight_kind_preproc:
            return (SDL_Color){175, 145, 220, 255};
        case syntax_highlight_kind_function:
            return (SDL_Color){110, 215, 195, 255};
        default:
            return (SDL_Color){220, 220, 220, 255};
    }
}

static int
source_pane_copySegment(const char *line, int start, int len, char *stackBuf, int stackCap, char **outBuf)
{
    if (!outBuf || !line || start < 0 || len <= 0) {
        return 0;
    }
    *outBuf = NULL;
    if (len + 1 <= stackCap) {
        memcpy(stackBuf, line + start, (size_t)len);
        stackBuf[len] = '\0';
        *outBuf = stackBuf;
        return 1;
    }
    char *heap = (char*)alloc_alloc((size_t)len + 1);
    if (!heap) {
        return 0;
    }
    memcpy(heap, line + start, (size_t)len);
    heap[len] = '\0';
    *outBuf = heap;
    return 1;
}

static int
source_pane_measureSegment(TTF_Font *font, const char *line, int start, int len)
{
    if (!font || !line || start < 0 || len <= 0) {
        return 0;
    }
    char stackBuf[256];
    char *seg = NULL;
    if (!source_pane_copySegment(line, start, len, stackBuf, (int)sizeof(stackBuf), &seg)) {
        return 0;
    }
    int w = 0;
    int h = 0;
    TTF_SizeText(font, seg, &w, &h);
    if (seg != stackBuf) {
        alloc_free(seg);
    }
    return w;
}

static void
source_pane_drawSegment(e9ui_context_t *ctx, e9ui_component_t *owner, TTF_Font *font, const char *line,
                        int start, int len, SDL_Color color, int x, int y, int lineHeight)
{
    if (!ctx || !owner || !font || !line || len <= 0) {
        return;
    }
    char stackBuf[256];
    char *seg = NULL;
    if (!source_pane_copySegment(line, start, len, stackBuf, (int)sizeof(stackBuf), &seg)) {
        return;
    }
    e9ui_text_select_drawText(ctx, owner, font, seg, color, x, y, lineHeight, 0, owner, 0, 0);
    if (seg != stackBuf) {
        alloc_free(seg);
    }
}

static void
source_pane_renderHighlightedLine(e9ui_context_t *ctx, e9ui_component_t *owner, TTF_Font *font,
                                  const char *path, int lineNo, const char *line, SDL_Color baseColor,
                                  int textX, int y, int lineHeight, int hitW, void *sourceBucket)
{
    if (!ctx || !owner || !font || !line) {
        return;
    }
    e9ui_text_select_drawText(ctx, owner, font, line, baseColor, textX, y,
                              lineHeight, hitW, sourceBucket, 0, 1);
    const syntax_highlight_span_t *spans = NULL;
    int spanCount = 0;
    if (!syntax_highlight_getLineSpans(path, lineNo, &spans, &spanCount) || !spans || spanCount <= 0) {
        return;
    }
    int lineLen = (int)strlen(line);
    int cursorIndex = 0;
    int cursorX = textX;
    for (int i = 0; i < spanCount; ++i) {
        int start = spans[i].startColumn;
        int len = spans[i].length;
        if (start < cursorIndex) {
            int trim = cursorIndex - start;
            if (trim >= len) {
                continue;
            }
            start += trim;
            len -= trim;
        }
        if (start >= lineLen || len <= 0) {
            continue;
        }
        if (start > cursorIndex) {
            int gap = source_pane_measureSegment(font, line, cursorIndex, start - cursorIndex);
            cursorX += gap;
            cursorIndex = start;
        }
        if (start + len > lineLen) {
            len = lineLen - start;
        }
        if (len <= 0) {
            continue;
        }
        SDL_Color color = source_pane_syntaxColor(spans[i].kind);
        source_pane_drawSegment(ctx, owner, font, line, start, len, color, cursorX, y, lineHeight);
        int segw = source_pane_measureSegment(font, line, start, len);
        cursorX += segw;
        cursorIndex = start + len;
    }
}

static void
source_pane_clearHover(e9ui_component_t *self, source_pane_state_t *st)
{
    if (!self || !st) {
        return;
    }
    st->hoverExpr[0] = '\0';
    st->hoverTip[0] = '\0';
    st->hoverLine = 0;
    st->hoverCol = 0;
    st->hoverPc = 0;
    st->hoverTick = 0;
    st->hoverActive = 0;
    e9ui_setTooltip(self, NULL);
}

static int
source_pane_columnFromX(TTF_Font *font, const char *line, int relX)
{
    if (!font || !line) {
        return 0;
    }
    if (relX <= 0) {
        return 0;
    }
    int lineLen = (int)strlen(line);
    if (lineLen <= 0) {
        return 0;
    }
    int bestCol = lineLen;
    for (int i = 1; i <= lineLen; ++i) {
        int w = source_pane_measureSegment(font, line, 0, i);
        if (w >= relX) {
            bestCol = i - 1;
            break;
        }
    }
    if (bestCol < 0) {
        bestCol = 0;
    }
    if (bestCol > lineLen) {
        bestCol = lineLen;
    }
    return bestCol;
}

static int
source_pane_extractHoverExprFallback(const char *line, int column, char *outExpr, int outCap)
{
    if (!line || !outExpr || outCap <= 1 || column < 0) {
        return 0;
    }
    outExpr[0] = '\0';
    int lineLen = (int)strlen(line);
    if (lineLen <= 0) {
        return 0;
    }
    int pivot = column;
    if (pivot >= lineLen) {
        pivot = lineLen - 1;
    }
    if (pivot < 0) {
        return 0;
    }
    if (!(isalnum((unsigned char)line[pivot]) || line[pivot] == '_')) {
        if (pivot > 0 && (isalnum((unsigned char)line[pivot - 1]) || line[pivot - 1] == '_')) {
            pivot -= 1;
        } else {
            return 0;
        }
    }
    int start = pivot;
    while (start > 0 && (isalnum((unsigned char)line[start - 1]) || line[start - 1] == '_')) {
        start--;
    }
    int end = pivot + 1;
    while (end < lineLen && (isalnum((unsigned char)line[end]) || line[end] == '_')) {
        end++;
    }
    int len = end - start;
    if (len <= 0) {
        return 0;
    }
    if (len >= outCap) {
        len = outCap - 1;
    }
    memcpy(outExpr, line + start, (size_t)len);
    outExpr[len] = '\0';
    return 1;
}

static void
source_pane_updateHoverTooltip(e9ui_component_t *self, e9ui_context_t *ctx, source_pane_state_t *st,
                               const e9ui_event_t *ev)
{
    if (!self || !ctx || !st || !ev || ev->type != SDL_MOUSEMOTION) {
        return;
    }
    if (st->viewMode != source_pane_mode_c) {
        source_pane_clearHover(self, st);
        return;
    }
    if (machine_getRunning(debugger.machine) && !st->frozenActive) {
        source_pane_clearHover(self, st);
        return;
    }
    int mx = ev->motion.x;
    int my = ev->motion.y;
    if (!source_pane_pointInBounds(self, mx, my)) {
        source_pane_clearHover(self, st);
        return;
    }
    TTF_Font *useFont = source_pane_resolveFont(ctx);
    if (!useFont) {
        source_pane_clearHover(self, st);
        return;
    }
    const int padPx = 10;
    source_pane_updateSourceLocation(st, 0);
    int manualView = st->manualSrcActive && st->manualSrcPath;
    const char *path = manualView ? st->manualSrcPath : st->curSrcPath;
    int curLine = manualView ? 0 : st->curSrcLine;
    if (!path || !path[0] || (!manualView && curLine <= 0)) {
        source_pane_clearHover(self, st);
        return;
    }
    source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(self, useFont, padPx);
    if (metrics.innerHeight <= 0 || metrics.lineHeight <= 0) {
        source_pane_clearHover(self, st);
        return;
    }
    int maxLines = metrics.maxLines > 0 ? metrics.maxLines : 1;
    int start = 1;
    if (!manualView) {
        start = curLine - (maxLines / 2);
        if (start < 1) {
            start = 1;
        }
    }
    if (st->scrollLineValid) {
        start = st->scrollLine;
        if (start < 1) {
            start = 1;
        }
    }
    int end = start + maxLines - 1;
    const char **lines = NULL;
    int count = 0;
    int first = 0;
    int total = 0;
    if (!source_getRange(path, start, end, &lines, &count, &first, &total) || count <= 0) {
        source_pane_clearHover(self, st);
        return;
    }
    if (count < maxLines && total > 0) {
        int missing = maxLines - count;
        int altStart = first - missing;
        if (altStart < 1) {
            altStart = 1;
        }
        int altEnd = altStart + maxLines - 1;
        if (altEnd > total) {
            altEnd = total;
        }
        source_getRange(path, altStart, altEnd, &lines, &count, &first, &total);
    }
    int digits = 1;
    int tmpTotal = (total > 0) ? total : (first + count - 1);
    for (int v = tmpTotal; v >= 10; v /= 10) {
        digits++;
    }
    if (digits < 3) {
        digits = 3;
    }
    char zeros[16];
    if (digits >= (int)sizeof(zeros)) {
        digits = (int)sizeof(zeros) - 1;
    }
    for (int i = 0; i < digits; ++i) {
        zeros[i] = '8';
    }
    zeros[digits] = '\0';
    int gutterW = 0;
    int th = 0;
    TTF_SizeText(useFont, zeros, &gutterW, &th);
    int gutterPad = e9ui_scale_px(ctx, 16);
    int textX = self->bounds.x + padPx + gutterW + gutterPad;
    int relY = my - (self->bounds.y + padPx);
    if (relY < 0) {
        source_pane_clearHover(self, st);
        return;
    }
    int row = relY / metrics.lineHeight;
    if (row < 0 || row >= count) {
        source_pane_clearHover(self, st);
        return;
    }
    if (mx < textX) {
        source_pane_clearHover(self, st);
        return;
    }
    const char *line = lines[row] ? lines[row] : "";
    int lineNo = first + row;
    int column = source_pane_columnFromX(useFont, line, mx - textX);
    char expr[256];
    expr[0] = '\0';
    if (!syntax_highlight_getHoverExpr(path, lineNo, column, expr, sizeof(expr))) {
        if (!source_pane_extractHoverExprFallback(line, column, expr, (int)sizeof(expr))) {
            source_pane_clearHover(self, st);
            return;
        }
    }
    unsigned long pcReg = 0;
    (void)machine_findReg(&debugger.machine, "PC", &pcReg);
    uint32_t pc = (uint32_t)pcReg;
    uint32_t now = SDL_GetTicks();
    if (st->hoverActive &&
        st->hoverLine == lineNo &&
        st->hoverCol == column &&
        st->hoverPc == pc &&
        strcmp(st->hoverExpr, expr) == 0 &&
        (now - st->hoverTick) < 120u) {
        return;
    }
    char tip[1024];
    tip[0] = '\0';
    if (!print_eval_eval(expr, tip, sizeof(tip))) {
        source_pane_clearHover(self, st);
        return;
    }
    for (char *p = tip; *p; ++p) {
        if (*p == '\n' || *p == '\r') {
            *p = ' ';
        }
    }
    strncpy(st->hoverExpr, expr, sizeof(st->hoverExpr) - 1);
    st->hoverExpr[sizeof(st->hoverExpr) - 1] = '\0';
    strncpy(st->hoverTip, tip, sizeof(st->hoverTip) - 1);
    st->hoverTip[sizeof(st->hoverTip) - 1] = '\0';
    st->hoverLine = lineNo;
    st->hoverCol = column;
    st->hoverPc = pc;
    st->hoverTick = now;
    st->hoverActive = 1;
    e9ui_setTooltip(self, st->hoverTip);
}

static void
source_pane_adjustScroll(source_pane_state_t *st, source_pane_mode_t mode, int delta)
{
    if (!st || delta == 0) {
        return;
    }
    if (mode == source_pane_mode_c) {
        int dest = st->scrollLine + delta;
        if (dest < 1) {
            dest = 1;
        }
        st->scrollLine = dest;
        st->scrollLineValid = 1;
        st->gutterPending = 0;
        return;
    }
    int dest = st->scrollIndex + delta;
    if (dest < 0 && !(dasm_getFlags() & DASM_IFACE_FLAG_STREAMING)) {
        dest = 0;
    }
    st->scrollIndex = dest;
    st->scrollIndexValid = 1;
    st->gutterPending = 0;
}

static void
source_pane_scrollToStart(source_pane_state_t *st, source_pane_mode_t mode)
{
    if (!st) {
        return;
    }
    if (mode == source_pane_mode_c) {
        st->scrollLine = 1;
        st->scrollLineValid = 1;
        st->gutterPending = 0;
        return;
    }
    if (dasm_getFlags() & DASM_IFACE_FLAG_STREAMING) {
        source_pane_followCurrent(st);
        return;
    }
    st->scrollIndex = 0;
    st->scrollIndexValid = 1;
    st->gutterPending = 0;
}

static void
source_pane_scrollToEnd(source_pane_state_t *st, source_pane_mode_t mode, int maxLines)
{
    if (!st) {
        return;
    }
    if (maxLines <= 0) {
        maxLines = 1;
    }
    if (mode == source_pane_mode_c) {
        source_pane_updateSourceLocation(st, 0);
        int total = 0;
        if (st->curSrcPath[0]) {
            total = source_getTotalLines(st->curSrcPath);
        }
        if (total <= 0) {
            st->scrollLine = 1;
        } else {
            int dest = total - maxLines + 1;
            if (dest < 1) {
                dest = 1;
            }
            st->scrollLine = dest;
        }
        st->scrollLineValid = 1;
        st->gutterPending = 0;
        return;
    }
    if (dasm_getFlags() & DASM_IFACE_FLAG_STREAMING) {
        return;
    }
    int total = dasm_getTotal();
    int dest = total - maxLines;
    if (dest < 0) {
        dest = 0;
    }
    st->scrollIndex = dest;
    st->scrollIndexValid = 1;
    st->gutterPending = 0;
}

static void
source_pane_followCurrent(source_pane_state_t *st)
{
    if (!st) {
        return;
    }
    source_pane_clearFunctionScrollLock(st);
    st->scrollLineValid = 0;
    st->scrollIndexValid = 0;
    st->overrideActive = 0;
    st->gutterPending = 0;
    st->manualSrcActive = 0;
}

static void
source_pane_freeFrozenAsm(source_pane_state_t *st)
{
    if (!st) {
        return;
    }
    if (st->frozenAsmLines) {
        for (int i = 0; i < st->frozenAsmCount; ++i) {
            alloc_free(st->frozenAsmLines[i]);
        }
        alloc_free(st->frozenAsmLines);
    }
    alloc_free(st->frozenAsmAddrs);
    st->frozenAsmLines = NULL;
    st->frozenAsmAddrs = NULL;
    st->frozenAsmCount = 0;
    st->frozenAsmStartIndex = 0;
    st->frozenAsmMaxLines = 0;
}

static void
source_pane_trackPosition(source_pane_state_t *st)
{
    if (!st) {
        return;
    }
    if (st->overrideActive) {
        return;
    }
    unsigned long curAddr = 0;
    (void)machine_findReg(&debugger.machine, "PC", &curAddr);
    curAddr &= 0x00ffffffu;
    if (curAddr != st->lastPcAddr) {
        st->scrollLineValid = 0;
        st->scrollIndexValid = 0;
        source_pane_clearFunctionScrollLock(st);
        st->manualSrcActive = 0;
    }
    st->lastPcAddr = curAddr;
}

static void
source_pane_updateSourceLocation(source_pane_state_t *st, int allowWhileRunning)
{
    if (!st) {
        return;
    }
    if (!allowWhileRunning && !st->overrideActive && machine_getRunning(debugger.machine)) {
        return;
    }
    unsigned long pc = 0;
    if (st->overrideActive) {
        pc = (unsigned long)st->overrideAddr;
    } else {
        (void)machine_findReg(&debugger.machine, "PC", &pc);
    }
    pc &= 0x00ffffffu;
    if (st->lastResolvedPc == (uint64_t)pc && st->curSrcLine > 0 && st->curSrcPath[0]) {
        return;
    }
    st->lastResolvedPc = (uint64_t)pc;
    st->curSrcLine = 0;
    st->curSrcPath[0] = '\0';

    const char *elf = debugger.libretro.exePath;
    if (!elf || !*elf || !debugger.elfValid) {
        return;
    }
    if (!addr2line_start(elf)) {
        return;
    }
    int line = 0;
    char path[PATH_MAX];
    if (addr2line_resolve((uint64_t)pc, path, sizeof(path), &line)) {
        source_pane_resolveSourcePath(path, st->curSrcPath, sizeof(st->curSrcPath));
        st->curSrcLine = line;
    }
}

static int
source_pane_preferredHeight(e9ui_component_t *self, e9ui_context_t *ctx, int availW)
{
    (void)self; (void)ctx; (void)availW;
    return 0;
}

static void
source_pane_layoutComp(e9ui_component_t *self, e9ui_context_t *ctx, e9ui_rect_t bounds)
{
    (void)ctx;
    self->bounds = bounds;
}

static const char *
source_pane_modeValue(source_pane_mode_t mode)
{
    if (mode == source_pane_mode_c) {
        return "c";
    }
    if (mode == source_pane_mode_h) {
        return "hex";
    }
    return "asm";
}

static source_pane_mode_t
source_pane_modeFromValue(const char *value)
{
    if (!value || !*value) {
        return source_pane_mode_a;
    }
    if (strcmp(value, "c") == 0) {
        return source_pane_mode_c;
    }
    if (strcmp(value, "hex") == 0) {
        return source_pane_mode_h;
    }
    return source_pane_mode_a;
}

static int
source_pane_modePersistValue(source_pane_mode_t mode)
{
    if (mode == source_pane_mode_c) {
        return 0;
    }
    if (mode == source_pane_mode_h) {
        return 3;
    }
    return 2;
}

static void
source_pane_persistSave(e9ui_component_t *self, e9ui_context_t *ctx, FILE *f)
{
  (void)ctx;
  if (!self || !self->persist_id) {
    return;
  }
  source_pane_state_t *st = (source_pane_state_t*)self->state;
  int m = st ? source_pane_modePersistValue(st->viewMode) : 0;
  fprintf(f, "comp.%s.mode=%d\n", self->persist_id, m);
}

static void
source_pane_persistLoad(e9ui_component_t *self, e9ui_context_t *ctx, const char *key, const char *value)
{
  (void)ctx;
  if (!self || !key || !value) {
    return;
  }
  if (strcmp(key, "mode") == 0) {
    int m = (int)strtol(value, NULL, 10);

    source_pane_mode_t mode = source_pane_mode_a;
	    if (m == 0) {
	        mode = source_pane_mode_c;
	    } else if (m == 3) {
	        mode = source_pane_mode_h;
	    }
	    source_pane_setModeInternal(self, mode, 0);
	  }
	}

static int
source_pane_getAsmWindow(source_pane_state_t *st, int maxLines, uint64_t *out_curAddr,
                         const char ***out_lines, const uint64_t **out_addrs, int *out_count)
{
    if (out_curAddr) {
        *out_curAddr = 0;
    }
    if (out_lines) {
        *out_lines = NULL;
    }
    if (out_addrs) {
        *out_addrs = NULL;
    }
    if (out_count) {
        *out_count = 0;
    }

    if (!st || maxLines <= 0 || !out_lines || !out_addrs || !out_count || !out_curAddr) {
        return 0;
    }

    int streaming = (dasm_getFlags() & DASM_IFACE_FLAG_STREAMING) ? 1 : 0;
    int total = dasm_getTotal();
    if (!streaming && total <= 0) {
        return 0;
    }

    int freezeWhileRunning = 0;
    if (!st->overrideActive && machine_getRunning(debugger.machine)) {
        freezeWhileRunning = 1;
    }
    if (st->frozenActive && !freezeWhileRunning) {
        st->frozenActive = 0;
        source_pane_freeFrozenAsm(st);
    }
    if (freezeWhileRunning && !st->frozenActive) {
        unsigned long pcAddr = 0;
        (void)machine_findReg(&debugger.machine, "PC", &pcAddr);
        pcAddr &= 0x00fffffful;
        pcAddr &= ~1ul;
        st->frozenPcAddr = (uint64_t)pcAddr;
        st->frozenActive = 1;
        st->frozenAsmStartIndex = INT_MIN;
        st->frozenAsmMaxLines = 0;
        source_pane_freeFrozenAsm(st);
    }

    uint64_t curAddr = 0;
    if (freezeWhileRunning) {
        curAddr = st->frozenPcAddr;
    } else {
        unsigned long pcAddr = 0;
        (void)machine_findReg(&debugger.machine, "PC", &pcAddr);
        pcAddr &= 0x00fffffful;
        pcAddr &= ~1ul;
        curAddr = (uint64_t)pcAddr;
    }
    *out_curAddr = curAddr;

    int startIndex = 0;
    if (st->scrollIndexValid) {
        startIndex = st->scrollIndex;
    } else {
        int curIndex = 0;
        if (!dasm_findIndexForAddr(curAddr, &curIndex) && !streaming) {
            curIndex = 0;
        }
        startIndex = curIndex - (maxLines / 2);
    }
    if (startIndex < 0 && !streaming) {
        startIndex = 0;
    }
    if (!streaming && startIndex >= total) {
        startIndex = total - 1;
    }
    if (freezeWhileRunning && !st->scrollIndexValid) {
        st->scrollIndex = startIndex;
        st->scrollIndexValid = 1;
    }
    int endIndex = startIndex + maxLines - 1;
    if (!streaming && endIndex >= total) {
        endIndex = total - 1;
    }

    const char **lines = NULL;
    const uint64_t *addrs = NULL;
    int first = 0;
    int count = 0;
    if (freezeWhileRunning && st->frozenActive && st->frozenAsmLines &&
        st->frozenAsmStartIndex == startIndex && st->frozenAsmMaxLines == maxLines) {
        lines = (const char**)st->frozenAsmLines;
        addrs = (const uint64_t*)st->frozenAsmAddrs;
        first = st->frozenAsmStartIndex;
        count = st->frozenAsmCount;
    } else {
        if (freezeWhileRunning) {
            int dummy = 0;
            (void)dasm_findIndexForAddr(curAddr, &dummy);
        } else {
            source_pane_trackPosition(st);
        }
        if (!dasm_getRangeByIndex(startIndex, endIndex, &lines, &addrs, &first, &count)) {
            return 0;
        }

        if (!streaming && count < maxLines && total > 0) {
            int missing = maxLines - count;
            int altStart = first - missing;
            if (altStart < 0) {
                altStart = 0;
            }
            int altEnd = altStart + maxLines - 1;
            if (altEnd >= total) {
                altEnd = total - 1;
            }
            dasm_getRangeByIndex(altStart, altEnd, &lines, &addrs, &first, &count);
        }

        if (freezeWhileRunning) {
            st->scrollIndex = first;
            st->scrollIndexValid = 1;
        } else if (st->scrollIndexValid) {
            st->scrollIndex = first;
        }

        if (freezeWhileRunning && st->frozenActive && (st->frozenAsmStartIndex != first ||
            st->frozenAsmCount != count || st->frozenAsmMaxLines != maxLines)) {
            source_pane_freeFrozenAsm(st);
            st->frozenAsmLines = (char**)alloc_calloc((size_t)count, sizeof(*st->frozenAsmLines));
            st->frozenAsmAddrs = (uint64_t*)alloc_calloc((size_t)count, sizeof(*st->frozenAsmAddrs));
            if (st->frozenAsmLines && st->frozenAsmAddrs) {
                for (int i = 0; i < count; ++i) {
                    st->frozenAsmLines[i] = alloc_strdup(lines[i] ? lines[i] : "");
                    st->frozenAsmAddrs[i] = addrs[i];
                }
                st->frozenAsmCount = count;
                st->frozenAsmStartIndex = first;
                st->frozenAsmMaxLines = maxLines;
                lines = (const char**)st->frozenAsmLines;
                addrs = (const uint64_t*)st->frozenAsmAddrs;
                first = st->frozenAsmStartIndex;
                count = st->frozenAsmCount;
            }
        }
    }

    *out_lines = lines;
    *out_addrs = addrs;
    *out_count = count;
    (void)first;
    return 1;
}

static void
source_pane_renderAsm(e9ui_component_t *self, e9ui_context_t *ctx)
{
    TTF_Font *useFont = source_pane_resolveFont(ctx);
    SDL_Rect area = { self->bounds.x, self->bounds.y, self->bounds.w, self->bounds.h };
    const int padPx = 10;
    SDL_SetRenderDrawColor(ctx->renderer, 20, 20, 24, 255);
    SDL_RenderFillRect(ctx->renderer, &area);
    if (!useFont) {
        return;
    }

    source_pane_state_t *st = (source_pane_state_t*)self->state;
    source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(self, useFont, padPx);
    if (metrics.innerHeight <= 0) {
        return;
    }
    int maxLines = metrics.maxLines;
    if (maxLines <= 0) {
        maxLines = 1;
    }
    int drawMaxLines = maxLines + 1;

    const char **lines = NULL;
    const uint64_t *addrs = NULL;
    int count = 0;
    uint64_t curAddr = 0;
    if (!source_pane_getAsmWindow(st, drawMaxLines, &curAddr, &lines, &addrs, &count)) {
        SDL_Color icol = (SDL_Color){200,160,160,255};
        source_pane_renderStatusMessage(ctx, useFont, area, padPx, icol, "No disassembly available");
        return;
    }

    int hexw = dasm_getAddrHexWidth();
    if (hexw < 6) {
        hexw = 6;
    }
    if (hexw > 16) {
        hexw = 16;
    }
    char sample[32];
    for (int i = 0; i < hexw; ++i) {
        sample[i] = 'F';
    }
    sample[hexw] = '\0';
    int gutterW = 0;
    int th = 0;
    TTF_SizeText(useFont, sample, &gutterW, &th);
    int gutterPad = e9ui_scale_px(ctx, 16);
    SDL_SetRenderDrawColor(ctx->renderer, 26, 26, 30, 255);
    SDL_Rect gutter = { area.x, area.y, padPx + gutterW + gutterPad, area.h };
    SDL_RenderFillRect(ctx->renderer, &gutter);

    SDL_Color txt = (SDL_Color){220,220,220,255};
    SDL_Color lno = (SDL_Color){160,160,200,255};
    SDL_Color lno_bp_on = (SDL_Color){120,200,120,255};
    SDL_Color lno_bp_off = (SDL_Color){200,140,60,255};
    int textX = area.x + padPx + gutterW + gutterPad;
    int hitW = area.x + area.w - textX - padPx;
    if (hitW < 0) {
        hitW = 0;
    }
    int y = area.y + padPx;
    int clipBottom = area.y + area.h + metrics.lineHeight;
    for (int i = 0; i < count; ++i) {
        uint64_t a = addrs[i];
        const char *ins = lines[i] ? lines[i] : "";
        if (a == curAddr) {
            SDL_SetRenderDrawColor(ctx->renderer, 40, 72, 138, 255);
            SDL_Rect hl = { area.x + 2, y - 2, area.w - 4, metrics.lineHeight + 4 };
            SDL_RenderFillRect(ctx->renderer, &hl);
        }
        char abuf[32];
        snprintf(abuf, sizeof(abuf), "%0*llX", hexw, (unsigned long long)a);
        int nw = 0;
        int nh = 0;
        TTF_SizeText(useFont, abuf, &nw, &nh);
        int lnx = area.x + padPx + (gutterW - nw);
        SDL_Color use_col = lno;
        machine_breakpoint_t *bp = machine_findBreakpointByAddr(&debugger.machine, (uint32_t)a);
        if (bp) {
            use_col = bp->enabled ? lno_bp_on : lno_bp_off;
        }
        void *addrBucket = st ? (void*)&st->bucketAddr : (void*)self;
        void *sourceBucket = st ? (void*)&st->bucketSource : (void*)self;
        e9ui_text_select_drawText(ctx, self, useFont, abuf, use_col, lnx, y,
                                  metrics.lineHeight, 0, addrBucket, 1, 1);
        e9ui_text_select_drawText(ctx, self, useFont, ins, txt, textX, y,
                                  metrics.lineHeight, hitW, sourceBucket, 0, 1);
        y += metrics.lineHeight;
        if (y > clipBottom) {
            break;
        }
    }
}

static void
source_pane_renderHex(e9ui_component_t *self, e9ui_context_t *ctx)
{
    TTF_Font *useFont = source_pane_resolveFont(ctx);
    SDL_Rect area = { self->bounds.x, self->bounds.y, self->bounds.w, self->bounds.h };
    const int padPx = 10;
    SDL_SetRenderDrawColor(ctx->renderer, 20, 20, 24, 255);
    SDL_RenderFillRect(ctx->renderer, &area);
    if (!useFont) {
        return;
    }

    source_pane_state_t *st = (source_pane_state_t*)self->state;
    source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(self, useFont, padPx);
    if (metrics.innerHeight <= 0) {
        return;
    }
    int maxLines = metrics.maxLines;
    if (maxLines <= 0) {
        maxLines = 1;
    }
    int drawMaxLines = maxLines + 1;

    const char **lines = NULL;
    const uint64_t *addrs = NULL;
    int count = 0;
    uint64_t curAddr = 0;
    if (!source_pane_getAsmWindow(st, drawMaxLines, &curAddr, &lines, &addrs, &count)) {
        SDL_Color icol = (SDL_Color){200,160,160,255};
        source_pane_renderStatusMessage(ctx, useFont, area, padPx, icol, "No disassembly available");
        return;
    }

    int hexw = dasm_getAddrHexWidth();
    if (hexw < 6) {
        hexw = 6;
    }
    if (hexw > 16) {
        hexw = 16;
    }
    char sample[32];
    for (int i = 0; i < hexw; ++i) {
        sample[i] = 'F';
    }
    sample[hexw] = '\0';
    int gutterW = 0;
    int th = 0;
    TTF_SizeText(useFont, sample, &gutterW, &th);
    int gutterPad = e9ui_scale_px(ctx, 16);
    SDL_SetRenderDrawColor(ctx->renderer, 26, 26, 30, 255);
    SDL_Rect gutter = { area.x, area.y, padPx + gutterW + gutterPad, area.h };
    SDL_RenderFillRect(ctx->renderer, &gutter);

    SDL_Color txt = (SDL_Color){220,220,220,255};
    SDL_Color lno = (SDL_Color){160,160,200,255};
    SDL_Color lno_bp_on = (SDL_Color){120,200,120,255};
    SDL_Color lno_bp_off = (SDL_Color){200,140,60,255};
    int textX = area.x + padPx + gutterW + gutterPad;
    int hitW = area.x + area.w - textX - padPx;
    if (hitW < 0) {
        hitW = 0;
    }

    int y = area.y + padPx;
    int clipBottom = area.y + area.h + metrics.lineHeight;
    for (int i = 0; i < count; ++i) {
        uint64_t a = addrs[i];
        const char *ins = lines[i] ? lines[i] : "";
        if (a == curAddr) {
            SDL_SetRenderDrawColor(ctx->renderer, 40, 72, 138, 255);
            SDL_Rect hl = { area.x + 2, y - 2, area.w - 4, metrics.lineHeight + 4 };
            SDL_RenderFillRect(ctx->renderer, &hl);
        }

        char abuf[32];
        snprintf(abuf, sizeof(abuf), "%0*llX", hexw, (unsigned long long)a);
        int nw = 0;
        int nh = 0;
        TTF_SizeText(useFont, abuf, &nw, &nh);
        int lnx = area.x + padPx + (gutterW - nw);
        SDL_Color use_col = lno;
        machine_breakpoint_t *bp = machine_findBreakpointByAddr(&debugger.machine, (uint32_t)a);
        if (bp) {
            use_col = bp->enabled ? lno_bp_on : lno_bp_off;
        }

        size_t wantBytes = 2;
        if (i + 1 < count) {
            uint64_t diff = addrs[i + 1] - addrs[i];
            if (diff > 0 && diff <= 64) {
                wantBytes = (size_t)diff;
            }
        } else {
            char tmp[64];
            size_t len = 0;
            if (libretro_host_debugDisassembleQuick((uint32_t)a, tmp, sizeof(tmp), &len) && len > 0 && len <= 64) {
                wantBytes = len;
            }
        }
        if (wantBytes > 16) {
            wantBytes = 16;
        }

        uint8_t bytes[16];
        memset(bytes, 0, sizeof(bytes));
        int gotBytes = libretro_host_debugReadMemory((uint32_t)a, bytes, wantBytes) ? 1 : 0;

        const int padBytes = 12;
        char hexbuf[padBytes * 3 + 1];
        size_t pos = 0;
        for (size_t b = 0; b < (size_t)padBytes; ++b) {
            if (b < wantBytes && gotBytes) {
                pos += (size_t)snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02X ", (unsigned)bytes[b]);
            } else {
                pos += (size_t)snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "   ");
            }
            if (pos >= sizeof(hexbuf)) {
                break;
            }
        }
        hexbuf[sizeof(hexbuf) - 1] = '\0';

        char linebuf[512];
        snprintf(linebuf, sizeof(linebuf), "%s%s", hexbuf, ins);
        linebuf[sizeof(linebuf) - 1] = '\0';

        void *addrBucket = st ? (void*)&st->bucketAddr : (void*)self;
        void *sourceBucket = st ? (void*)&st->bucketSource : (void*)self;
        e9ui_text_select_drawText(ctx, self, useFont, abuf, use_col, lnx, y,
                                  metrics.lineHeight, 0, addrBucket, 1, 1);
        e9ui_text_select_drawText(ctx, self, useFont, linebuf, txt, textX, y,
                                  metrics.lineHeight, hitW, sourceBucket, 0, 1);

        y += metrics.lineHeight;
        if (y > clipBottom) {
            break;
        }
    }
}

static void
source_pane_render(e9ui_component_t *self, e9ui_context_t *ctx)
{
    TTF_Font *useFont = e9ui->theme.text.source ? e9ui->theme.text.source : ctx->font;
    SDL_Rect area = { self->bounds.x, self->bounds.y, self->bounds.w, self->bounds.h };
    source_pane_state_t *st = (source_pane_state_t*)self->state;
    int padPx = 10;
    SDL_bool hadClip = SDL_FALSE;
    SDL_Rect prevClip = {0};
    source_pane_pushRenderClip(ctx, &area, &hadClip, &prevClip);
    SDL_SetRenderDrawColor(ctx->renderer, 20, 20, 20, 255);
    SDL_RenderFillRect(ctx->renderer, &area);

    if (!useFont) {
        goto done;
    }
    int freezeWhileRunning = 0;
    if (st && !st->overrideActive && machine_getRunning(debugger.machine)) {
        freezeWhileRunning = 1;
    }
    if (st && st->frozenActive && !freezeWhileRunning) {
        st->frozenActive = 0;
        source_pane_freeFrozenAsm(st);
    }
    if (st && st->viewMode != source_pane_mode_a && st->viewMode != source_pane_mode_h &&
        (st->frozenActive || st->frozenAsmLines)) {
        st->frozenActive = 0;
        source_pane_freeFrozenAsm(st);
    }
    if (st && st->viewMode == source_pane_mode_a) {
        source_pane_renderAsm(self, ctx);
        goto done;
    }
    if (st && st->viewMode == source_pane_mode_h) {
        source_pane_renderHex(self, ctx);
        goto done;
    }
    if (st && !freezeWhileRunning) {
        source_pane_trackPosition(st);
    }
    if (st) {
        source_pane_updateSourceLocation(st, 0);
        source_pane_refreshSourceFiles(self, st);
    }
    int manualView = st && st->manualSrcActive && st->manualSrcPath;
    const char *path = manualView ? st->manualSrcPath : (st ? st->curSrcPath : NULL);
    int curLine = manualView ? 0 : (st ? st->curSrcLine : 0);
    if (st) {
        const char *functionPath = path;
        if (st->manualSrcActive && st->manualSrcPath && st->manualSrcPath[0] == '\0') {
            functionPath = NULL;
        }
        source_pane_refreshSourceFunctions(self, st, functionPath);
        if (!st->functionScrollLock && !manualView) {
            source_pane_trackCurrentFunction(self, st, path, curLine);
        }
    }
    if (!path || !*path || (!manualView && curLine <= 0)) {
        SDL_Color icol = (SDL_Color){200,160,160,255};
        source_pane_renderStatusMessage(ctx, useFont, area, padPx, icol, "No source code available");
        goto done;
    }
    source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(self, useFont, padPx);
    if (metrics.innerHeight <= 0) {
        goto done;
    }
    int maxLines = metrics.maxLines;
    if (maxLines <= 0) {
        maxLines = 1;
    }
    int drawMaxLines = maxLines + 1;
    int start = 1;
    if (!manualView) {
        start = curLine - (maxLines / 2);
        if (start < 1) {
            start = 1;
        }
    }
    if (st && st->scrollLineValid) {
        start = st->scrollLine;
        if (start < 1) {
            start = 1;
        }
    }
    int end = start + drawMaxLines - 1;

    const char **lines = NULL;
    int count = 0;
    int first = 0;
    int total = 0;
    if (!source_getRange(path, start, end, &lines, &count, &first, &total)) {
        SDL_Color icol = (SDL_Color){200,160,160,255};
        source_pane_renderStatusMessage(ctx, useFont, area, padPx, icol, "Failed to load source code");
        goto done;
    }
    // Adjust start if near end of file for better centering
    if (count < drawMaxLines && total > 0) {
        int missing = drawMaxLines - count;
        int altStart = first - missing;
        if (altStart < 1) {
            altStart = 1;
        }
        int altEnd = altStart + drawMaxLines - 1;
        if (altEnd > total) {
            altEnd = total;
        }
        source_getRange(path, altStart, altEnd, &lines, &count, &first, &total);
    }

    if (st) {
        st->scrollLine = first;
    }

    // Compute gutter width based on total line count
    int digits = 1;
    int tmp_total = (total > 0) ? total : (first + count - 1);
    for (int v = tmp_total; v >= 10; v /= 10) {
        digits++;
    }
    if (digits < 3) {
        digits = 3;
    }
    char zeros[16];
    if (digits >= (int)sizeof(zeros)) {
        digits = (int)sizeof(zeros) - 1;
    }
    for (int i=0; i<digits; ++i) {
        zeros[i] = '8';
    }
    zeros[digits] = '\0';
    int gutterW = 0, th = 0;
    TTF_SizeText(useFont, zeros, &gutterW, &th);
    int gutterPad = e9ui_scale_px(ctx, 16);
    // Draw gutter background
    SDL_SetRenderDrawColor(ctx->renderer, 26, 26, 30, 255);
    SDL_Rect gutter = { area.x, area.y, padPx + gutterW + gutterPad, area.h };
    SDL_RenderFillRect(ctx->renderer, &gutter);

    int y = area.y + padPx;
    int lineHeight = metrics.lineHeight;
    int clipBottom = area.y + area.h + lineHeight;
    SDL_Color txt = (SDL_Color){220,220,220,255};
    SDL_Color lno = (SDL_Color){160,160,180,255};
    SDL_Color lno_bp_on = (SDL_Color){120,200,120,255};
    SDL_Color lno_bp_off = (SDL_Color){200,140,60,255};
    int textX = area.x + padPx + gutterW + gutterPad;
    int hitW = area.x + area.w - textX - padPx;
    if (hitW < 0) {
        hitW = 0;
    }
    const machine_breakpoint_t *bps = NULL;
    int bp_count = 0;
    if (machine_getBreakpoints(&debugger.machine, &bps, &bp_count)) {
        for (int i = 0; i < bp_count; ++i) {
            machine_breakpoint_t *bp = (machine_breakpoint_t*)&bps[i];
            if (bp->line <= 0 || !bp->file[0]) {
                breakpoints_resolveLocation(bp);
            }
        }
    } else {
        bps = NULL;
        bp_count = 0;
    }
    for (int i=0; i<count; ++i) {
        const char *ln = lines[i] ? lines[i] : "";
        int lineNo = first + i;
        // Highlight current line (blue shade)
        if (curLine > 0 && lineNo == curLine) {
            SDL_SetRenderDrawColor(ctx->renderer, 40, 72, 138, 255);
            SDL_Rect hl = { area.x + 2, y - 2, area.w - 4, lineHeight + 4 };
            SDL_RenderFillRect(ctx->renderer, &hl);
        }
        // Line number (right-aligned in gutter)
        char numbuf[16];
        snprintf(numbuf, sizeof(numbuf), "%d", lineNo);
        int nw = 0, nh = 0;
        if (useFont) {
            TTF_SizeText(useFont, numbuf, &nw, &nh);
        }
        int lnx = area.x + padPx + (gutterW - nw);
        if (useFont) {
            int nsw = 0, nsh = 0;
            SDL_Color use_col = lno;
            machine_breakpoint_t *bp = source_pane_findBreakpointForLine(path, lineNo, bps, bp_count);
            if (bp) {
                use_col = bp->enabled ? lno_bp_on : lno_bp_off;
            }
            SDL_Texture *nt = e9ui_text_cache_getText(ctx->renderer, useFont, numbuf, use_col, &nsw, &nsh);
            if (nt) {
                SDL_Rect nr = { lnx, y, nsw, nsh };
                SDL_RenderCopy(ctx->renderer, nt, NULL, &nr);
            }
        }
        void *sourceBucket = st ? (void*)&st->bucketSource : (void*)self;
        source_pane_renderHighlightedLine(ctx, self, useFont, path, lineNo, ln, txt,
                                          textX, y, lineHeight, hitW, sourceBucket);
        y += lineHeight;
        if (y > clipBottom) {
            break;
        }
    }

 done:
    {
      e9ui_component_t* overlay = e9ui_child_find(self, st->toggleBtnMeta);
      e9ui_component_t* fileSelect = st && st->fileSelectMeta ? e9ui_child_find(self, st->fileSelectMeta) : NULL;
      e9ui_component_t* functionSelect = st && st->functionSelectMeta ? e9ui_child_find(self, st->functionSelectMeta) : NULL;
      source_pane_mode_t mode = source_pane_getMode(self);
      int rowX = self->bounds.x;
      int rowY = self->bounds.y;
      int rowW = self->bounds.w;
      int rowH = e9ui_scale_px(ctx, 30);
      int modeW = e9ui_scale_px(ctx, 24);
      if (overlay && overlay->preferredHeight) {
          int ph = overlay->preferredHeight(overlay, ctx, rowW);
          if (ph > 0) {
              rowH = ph;
          }
      }
      {
          TTF_Font *modeFont = e9ui->theme.text.prompt ? e9ui->theme.text.prompt : ctx->font;
          int hexW = 0;
          if (modeFont && TTF_SizeText(modeFont, "HEX", &hexW, NULL) == 0) {
              int padW = e9ui_scale_px(ctx, 16);
              modeW = hexW + padW;
          }
      }
      if (modeW > rowW) {
          modeW = rowW;
      }
      int leftW = rowW - modeW;
      if (leftW < 0) {
          leftW = 0;
      }
      int sourceW = leftW / 2;
      int functionW = leftW - sourceW;

      if (overlay && modeW > 0) {
          e9ui_rect_t bounds = {
              rowX + leftW,
              rowY,
              modeW,
              rowH
          };
          if (overlay->layout) {
              overlay->layout(overlay, ctx, bounds);
          } else {
              overlay->bounds = bounds;
          }
          overlay->render(overlay, ctx);
      }
      if (functionSelect) {
          e9ui_setHidden(functionSelect, mode == source_pane_mode_c ? 0 : 1);
          if (mode == source_pane_mode_c && functionW > 0) {
              e9ui_rect_t bounds = {
                  rowX + sourceW,
                  rowY,
                  functionW,
                  rowH
              };
              if (functionSelect->layout) {
                  functionSelect->layout(functionSelect, ctx, bounds);
              } else {
                  functionSelect->bounds = bounds;
              }
              functionSelect->render(functionSelect, ctx);
          }
      }
      if (fileSelect) {
          e9ui_setHidden(fileSelect, mode == source_pane_mode_c ? 0 : 1);
          if (mode == source_pane_mode_c && sourceW > 0) {
              e9ui_rect_t bounds = {
                  rowX,
                  rowY,
                  sourceW,
                  rowH
              };
              if (fileSelect->layout) {
                  fileSelect->layout(fileSelect, ctx, bounds);
              } else {
                  fileSelect->bounds = bounds;
              }
              fileSelect->render(fileSelect, ctx);
          }
      }
    }
    source_pane_popRenderClip(ctx, hadClip, &prevClip);
}

static int
source_pane_handleEventComp(e9ui_component_t *self, e9ui_context_t *ctx, const e9ui_event_t *ev)
{
    if (!self || !ev) {
        return 0;
    }
    source_pane_state_t *st = (source_pane_state_t*)self->state;
    source_pane_mode_t mode = st ? st->viewMode : source_pane_mode_c;
    if (ev->type == SDL_MOUSEMOTION) {
        if (st && st->gutterPending) {
            int slop = ctx ? e9ui_scale_px(ctx, 4) : 4;
            int dx = ev->motion.x - st->gutterDownX;
            int dy = ev->motion.y - st->gutterDownY;
            if (dx * dx + dy * dy >= slop * slop) {
                st->gutterPending = 0;
            }
        }
        source_pane_updateHoverTooltip(self, ctx, st, ev);
        return 0;
    }
    if (ev->type == SDL_MOUSEBUTTONUP && ev->button.button == SDL_BUTTON_LEFT) {
        if (!st || !st->gutterPending) {
            return 0;
        }
        st->gutterPending = 0;
        int slop = ctx ? e9ui_scale_px(ctx, 4) : 4;
        int dx = ev->button.x - st->gutterDownX;
        int dy = ev->button.y - st->gutterDownY;
        if (dx * dx + dy * dy >= slop * slop) {
            return 0;
        }
        if (st->gutterMode == source_pane_mode_c) {
            const char *path = NULL;
            if (st->manualSrcActive && st->manualSrcPath) {
                path = st->manualSrcPath;
            } else {
                path = st->curSrcPath;
            }
            int lineNo = st->gutterLine;
            if (!path || !path[0] || lineNo <= 0) {
                return 0;
            }
            const machine_breakpoint_t *bps = NULL;
            int bp_count = 0;
            if (machine_getBreakpoints(&debugger.machine, &bps, &bp_count)) {
                for (int i = 0; i < bp_count; ++i) {
                    machine_breakpoint_t *bp = (machine_breakpoint_t*)&bps[i];
                    if (bp->line <= 0 || !bp->file[0]) {
                        breakpoints_resolveLocation(bp);
                    }
                }
            } else {
                bps = NULL;
                bp_count = 0;
            }
            machine_breakpoint_t *existing = source_pane_findBreakpointForLine(path, lineNo, bps, bp_count);
            if (existing) {
                uint32_t addr = (uint32_t)existing->addr;
                if (machine_removeBreakpointByAddr(&debugger.machine, addr)) {
                    libretro_host_debugRemoveBreakpoint(addr);
                    breakpoints_markDirty();
                }
                return 1;
            }
            uint32_t addr = 0;
            if (!source_pane_resolveFileLine(debugger.libretro.exePath, path, lineNo, &addr)) {
                return 0;
            }
            addr = (uint32_t)(((uint64_t)addr + (uint64_t)debugger.machine.textBaseAddr) & 0x00ffffffu);
            machine_breakpoint_t *bp = machine_addBreakpoint(&debugger.machine, addr, 1);
            if (bp) {
                strncpy(bp->file, path, sizeof(bp->file) - 1);
                bp->file[sizeof(bp->file) - 1] = '\0';
                bp->line = lineNo;
                libretro_host_debugAddBreakpoint(addr);
                breakpoints_markDirty();
                return 1;
            }
            return 0;
        }
        if (st->gutterMode == source_pane_mode_a || st->gutterMode == source_pane_mode_h) {
            uint32_t addr = st->gutterAddr;
            machine_breakpoint_t *existing = machine_findBreakpointByAddr(&debugger.machine, addr);
            if (existing) {
                if (machine_removeBreakpointByAddr(&debugger.machine, addr)) {
                    libretro_host_debugRemoveBreakpoint(addr);
                    breakpoints_markDirty();
                }
                return 1;
            }
            machine_breakpoint_t *bp = machine_addBreakpoint(&debugger.machine, addr, 1);
            if (bp) {
                breakpoints_resolveLocation(bp);
                libretro_host_debugAddBreakpoint(addr);
                breakpoints_markDirty();
                return 1;
            }
            return 0;
        }
        return 0;
    }
    if (ev->type == SDL_MOUSEWHEEL) {
        int mx = e9ui->mouseX;
        int my = e9ui->mouseY;
        if (source_pane_pointInBounds(self, mx, my)) {
            int wheelY = ev->wheel.y;
            if (ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                wheelY = -wheelY;
            }
            if (wheelY != 0) {
                const int linesPerTick = 1;
                int delta = wheelY * linesPerTick;
                source_pane_adjustScroll(st, mode, delta);
            }
            return 1;
        }
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        int mx = ev->button.x;
        int my = ev->button.y;
        if (!source_pane_pointInBounds(self, mx, my)) {
            return 0;
        }
        TTF_Font *useFont = source_pane_resolveFont(ctx);
        if (!useFont) {
            return 0;
        }
        const int padPx = 10;
        if (mode == source_pane_mode_c) {
            source_pane_updateSourceLocation(st, 0);
            int manualView = st && st->manualSrcActive && st->manualSrcPath;
            const char *path = manualView ? st->manualSrcPath : (st ? st->curSrcPath : NULL);
            int curLine = manualView ? 0 : (st ? st->curSrcLine : 0);
            if (!path || !path[0] || (!manualView && curLine <= 0)) {
                return 0;
            }
            source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(self, useFont, padPx);
            if (metrics.innerHeight <= 0) {
                return 0;
            }
            int maxLines = metrics.maxLines > 0 ? metrics.maxLines : 1;
            int start = 1;
            if (!manualView) {
                start = curLine - (maxLines / 2);
                if (start < 1) {
                    start = 1;
                }
            }
            if (st && st->scrollLineValid) {
                start = st->scrollLine;
                if (start < 1) {
                    start = 1;
                }
            }
            int end = start + maxLines - 1;
            const char **lines = NULL;
            int count = 0;
            int first = 0;
            int total = 0;
            if (!source_getRange(path, start, end, &lines, &count, &first, &total)) {
                return 0;
            }
            if (count < maxLines && total > 0) {
                int missing = maxLines - count;
                int altStart = first - missing;
                if (altStart < 1) {
                    altStart = 1;
                }
                int altEnd = altStart + maxLines - 1;
                if (altEnd > total) {
                    altEnd = total;
                }
                source_getRange(path, altStart, altEnd, &lines, &count, &first, &total);
            }

            int digits = 1;
            int tmp_total = (total > 0) ? total : (first + count - 1);
            for (int v = tmp_total; v >= 10; v /= 10) {
                digits++;
            }
            if (digits < 3) {
                digits = 3;
            }
            char zeros[16];
            if (digits >= (int)sizeof(zeros)) {
                digits = (int)sizeof(zeros) - 1;
            }
            for (int i = 0; i < digits; ++i) {
                zeros[i] = '8';
            }
            zeros[digits] = '\0';
            int gutterW = 0;
            int th = 0;
            TTF_SizeText(useFont, zeros, &gutterW, &th);
            int gutterPad = e9ui_scale_px(ctx, 16);
            int gutterRight = self->bounds.x + padPx + gutterW + gutterPad;
            if (mx >= gutterRight) {
                return 0;
            }
            int row = (my - (self->bounds.y + padPx)) / metrics.lineHeight;
            if (row < 0 || row >= count) {
                return 0;
            }
            int lineNo = first + row;
            st->gutterPending = 1;
            st->gutterMode = source_pane_mode_c;
            st->gutterLine = lineNo;
            st->gutterDownX = mx;
            st->gutterDownY = my;
            return 1;
        }
        if (mode == source_pane_mode_a || mode == source_pane_mode_h) {
            source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(self, useFont, padPx);
            if (metrics.innerHeight <= 0) {
                return 0;
            }
            int maxLines = metrics.maxLines > 0 ? metrics.maxLines : 1;
            int streaming = (dasm_getFlags() & DASM_IFACE_FLAG_STREAMING) ? 1 : 0;
            int total = dasm_getTotal();
            if (!streaming && total <= 0) {
                return 0;
            }
            int curIndex = 0;
            unsigned long curAddr = 0;
            (void)machine_findReg(&debugger.machine, "PC", &curAddr);
            if (!dasm_findIndexForAddr(curAddr, &curIndex) && !streaming) {
                curIndex = 0;
            }
            int startIndex = curIndex - (maxLines / 2);
            if (st && st->scrollIndexValid) {
                startIndex = st->scrollIndex;
            }
            if (startIndex < 0 && !streaming) {
                startIndex = 0;
            }
            if (!streaming && startIndex >= total) {
                startIndex = total - 1;
            }
            int endIndex = startIndex + maxLines - 1;
            if (!streaming && endIndex >= total) {
                endIndex = total - 1;
            }
            const char **lines = NULL;
            const uint64_t *addrs = NULL;
            int first = 0;
            int count = 0;
            if (!dasm_getRangeByIndex(startIndex, endIndex, &lines, &addrs, &first, &count)) {
                return 0;
            }
            if (!streaming && count < maxLines && total > 0) {
                int missing = maxLines - count;
                int altStart = first - missing;
                if (altStart < 0) {
                    altStart = 0;
                }
                int altEnd = altStart + maxLines - 1;
                if (altEnd >= total) {
                    altEnd = total - 1;
                }
                dasm_getRangeByIndex(altStart, altEnd, &lines, &addrs, &first, &count);
            }
            int hexw = dasm_getAddrHexWidth();
            if (hexw < 6) {
                hexw = 6;
            }
            if (hexw > 16) {
                hexw = 16;
            }
            char sample[32];
            for (int i = 0; i < hexw; ++i) {
                sample[i] = 'F';
            }
            sample[hexw] = '\0';
            int gutterW = 0;
            int th = 0;
            TTF_SizeText(useFont, sample, &gutterW, &th);
            int gutterPad = e9ui_scale_px(ctx, 16);
            int gutterRight = self->bounds.x + padPx + gutterW + gutterPad;
            if (mx >= gutterRight) {
                return 0;
            }
            int row = (my - (self->bounds.y + padPx)) / metrics.lineHeight;
            if (row < 0 || row >= count) {
                return 0;
            }
            st->gutterPending = 1;
            st->gutterMode = mode;
            st->gutterAddr = (uint32_t)(addrs[row] & 0x00ffffffu);
            st->gutterDownX = mx;
            st->gutterDownY = my;
            return 1;
        }
    }
    if (ev->type == SDL_KEYDOWN && ctx && e9ui_getFocus(ctx) == self) {
        const int padPx = 10;
        TTF_Font *useFont = source_pane_resolveFont(ctx);
        int maxLines = 1;
        if (useFont) {
            source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(self, useFont, padPx);
            maxLines = metrics.maxLines;
        }
        if (maxLines <= 0) {
            maxLines = 1;
        }
        SDL_Keycode kc = ev->key.keysym.sym;
        switch (kc) {
        case SDLK_PAGEUP:
            source_pane_adjustScroll(st, mode, -maxLines);
            return 1;
        case SDLK_PAGEDOWN:
            source_pane_adjustScroll(st, mode, maxLines);
            return 1;
        case SDLK_UP:
            source_pane_adjustScroll(st, mode, -1);
            return 1;
        case SDLK_DOWN:
            source_pane_adjustScroll(st, mode, 1);
            return 1;
        case SDLK_HOME:
            source_pane_scrollToStart(st, mode);
            return 1;
        case SDLK_END:
            source_pane_scrollToEnd(st, mode, maxLines);
            return 1;
        case SDLK_f:
            source_pane_followCurrent(st);
            return 1;
        default:
            break;
        }
    }
    return 0;
}

static void
source_pane_modeSelectChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *value, void *user)
{
    (void)ctx;
    (void)comp;
    e9ui_component_t *pane = (e9ui_component_t*)user;
    if (!pane || !value || !*value) {
        return;
    }
    source_pane_setMode(pane, source_pane_modeFromValue(value));
    config_saveConfig();
}

static void
source_pane_sourceSelectChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *value, void *user)
{
    (void)ctx;
    (void)comp;
    source_pane_state_t *st = (source_pane_state_t*)user;
    if (!st || !value) {
        return;
    }
    alloc_free(st->manualSrcPath);
    st->manualSrcPath = alloc_strdup(value);
    if (!st->manualSrcPath) {
        st->manualSrcActive = 0;
        return;
    }
    st->manualSrcActive = 1;
    st->scrollLine = 1;
    st->scrollLineValid = 1;
    st->gutterPending = 0;
    source_pane_clearFunctionScrollLock(st);
    if (st->ownerPane && st->functionSelectMeta) {
        e9ui_component_t *function_select = e9ui_child_find(st->ownerPane, st->functionSelectMeta);
        if (function_select) {
            e9ui_textbox_setText(function_select, "");
        }
    }
    source_pane_clearSourceFunctions(st);
    if (st->ownerPane) {
        if (st->manualSrcPath[0]) {
            source_pane_refreshSourceFunctions(st->ownerPane, st, st->manualSrcPath);
        } else {
            source_pane_refreshSourceFunctions(st->ownerPane, st, NULL);
        }
    }
    source_pane_syncFunctionSelect(st->ownerPane, st);
}

static void
source_pane_functionSelectChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *value, void *user)
{
    (void)comp;
    source_pane_state_t *st = (source_pane_state_t*)user;
    if (!st || !value || !*value) {
        return;
    }
    int line = 0;
    const char *selected_file = NULL;
    if (!source_pane_parseFunctionValue(value, &line, &selected_file)) {
        return;
    }
    if (selected_file && selected_file[0]) {
        const char *end = strchr(selected_file, '|');
        if (end) {
            size_t len = (size_t)(end - selected_file);
            if (len > 0 && len < PATH_MAX) {
                char file_path[PATH_MAX];
                memcpy(file_path, selected_file, len);
                file_path[len] = '\0';
                alloc_free(st->manualSrcPath);
                st->manualSrcPath = alloc_strdup(file_path);
                if (st->manualSrcPath) {
                    st->manualSrcActive = 1;
                }
            }
        }
    }
    int max_lines = 1;
    if (ctx && st->ownerPane) {
        TTF_Font *use_font = source_pane_resolveFont(ctx);
        if (use_font) {
            source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(st->ownerPane, use_font, 10);
            if (metrics.maxLines > 0) {
                max_lines = metrics.maxLines;
            }
        }
    }
    int start = line - (max_lines / 2);
    if (start < 1) {
        start = 1;
    }
    st->scrollLine = start;
    st->scrollLineValid = 1;
    st->gutterPending = 0;
    st->functionScrollLock = 1;
}

static void
source_pane_dtor(e9ui_component_t *self, e9ui_context_t *ctx)
{
    (void)ctx;
    if (!self || !self->state) {
        return;
    }
    source_pane_state_t *st = (source_pane_state_t*)self->state;
    source_pane_freeFrozenAsm(st);
    source_pane_clearSourceFiles(st);
    source_pane_clearSourceFunctions(st);
    alloc_free(st->manualSrcPath);
    st->manualSrcPath = NULL;
    // Child metadata keys are owned/freed by e9ui child container teardown.
    st->toggleBtnMeta = NULL;
    st->fileSelectMeta = NULL;
    st->functionSelectMeta = NULL;
}

e9ui_component_t *
source_pane_make(void)
{
  static const e9ui_textbox_option_t modeOptions[] = {
      { .value = "c",   .label = "C" },
      { .value = "asm", .label = "ASM" },
      { .value = "hex", .label = "HEX" },
  };
  e9ui_component_t *c = (e9ui_component_t*)alloc_calloc(1, sizeof(*c));
  c->name = "source_pane";
  source_pane_state_t *st = (source_pane_state_t*)alloc_calloc(1, sizeof(source_pane_state_t));
  st->viewMode = source_pane_mode_c;
  st->scrollLine = 1;
  st->scrollLineValid = 0;
  st->scrollIndex = 0;
  st->scrollIndexValid = 0;
  st->ownerPane = c;
  c->state = st;
  c->focusable = 1;
  c->preferredHeight = source_pane_preferredHeight;
  c->layout = source_pane_layoutComp;
  c->render = source_pane_render;
  c->handleEvent = source_pane_handleEventComp;
  c->persistSave = source_pane_persistSave;
  c->persistLoad = source_pane_persistLoad;
  c->dtor = source_pane_dtor;

  e9ui_component_t *modeSelect = e9ui_textbox_make(16, NULL, NULL, NULL);
  if (modeSelect) {
      e9ui_textbox_setReadOnly(modeSelect, 1);
      e9ui_textbox_setOptions(modeSelect, modeOptions, (int)(sizeof(modeOptions) / sizeof(modeOptions[0])));
      e9ui_textbox_setOnOptionSelected(modeSelect, source_pane_modeSelectChanged, c);
      e9ui_textbox_setSelectedValue(modeSelect, source_pane_modeValue(st->viewMode));
      st->toggleBtnMeta = alloc_strdup("toggle");
      e9ui_child_add(c, modeSelect, st->toggleBtnMeta);
  }

  e9ui_component_t *fileSelect = e9ui_textbox_make(512, NULL, NULL, NULL);
  if (fileSelect) {
      e9ui_textbox_setPlaceholder(fileSelect, "source file");
      e9ui_textbox_setOnOptionSelected(fileSelect, source_pane_sourceSelectChanged, st);
      st->fileSelectMeta = alloc_strdup("source_select");
      e9ui_child_add(c, fileSelect, st->fileSelectMeta);
  }
  e9ui_component_t *functionSelect = e9ui_textbox_make(1024, NULL, NULL, NULL);
  if (functionSelect) {
      e9ui_textbox_setPlaceholder(functionSelect, "function");
      e9ui_textbox_setOnOptionSelected(functionSelect, source_pane_functionSelectChanged, st);
      st->functionSelectMeta = alloc_strdup("function_select");
      e9ui_child_add(c, functionSelect, st->functionSelectMeta);
  }
  
  return c;
}

static void
source_pane_setModeInternal(e9ui_component_t *comp, source_pane_mode_t mode, int enforceElfValid)
{
    if (!comp || !comp->state) {
        return;
    }
    source_pane_state_t *st = (source_pane_state_t*)comp->state;
    if (mode != source_pane_mode_c && mode != source_pane_mode_a && mode != source_pane_mode_h) {
        mode = source_pane_mode_a;
    }
    if (enforceElfValid && !debugger.elfValid && mode == source_pane_mode_c) {
        mode = source_pane_mode_a;
    }
    st->viewMode = mode;
    st->gutterPending = 0;
    source_pane_clearHover(comp, st);
    source_pane_clearFunctionScrollLock(st);
    if (mode != source_pane_mode_c) {
        st->manualSrcActive = 0;
    }

    if (mode != source_pane_mode_a && mode != source_pane_mode_h) {
        st->frozenActive = 0;
        source_pane_freeFrozenAsm(st);
    }

    if (st->toggleBtnMeta) {
        e9ui_component_t *select = e9ui_child_find(comp, st->toggleBtnMeta);
        if (select) {
            e9ui_textbox_setSelectedValue(select, source_pane_modeValue(mode));
        }
    }
}

void
source_pane_setMode(e9ui_component_t *comp, source_pane_mode_t mode)
{
    source_pane_setModeInternal(comp, mode, 1);
}

source_pane_mode_t source_pane_getMode(e9ui_component_t *comp)
{
    if (!comp || !comp->state) {
        return source_pane_mode_c;
    }
    source_pane_state_t *st = (source_pane_state_t*)comp->state;
    return st->viewMode;
}

void
source_pane_setToggleVisible(e9ui_component_t *comp, int visible)
{
    if (!comp || !comp->state) {
        return;
    }
    source_pane_state_t *st = (source_pane_state_t*)comp->state;
    if (!st->toggleBtnMeta) {
        return;
    }
    e9ui_component_t *overlay = e9ui_child_find(comp, st->toggleBtnMeta);
    if (!overlay) {
        return;
    }
    e9ui_setHidden(overlay, visible ? 0 : 1);
}

void source_pane_markNeedsRefresh(e9ui_component_t *comp)
{
    if (!comp || !comp->state) {
        return;
    }
    source_pane_state_t *st = (source_pane_state_t*)comp->state;
    st->scrollLineValid = 0;
    st->scrollIndexValid = 0;
    st->scrollLine = 1;
    st->scrollIndex = 0;
    st->gutterPending = 0;
    source_pane_clearFunctionScrollLock(st);
    st->sourceFilesLoaded = 0;
    st->sourceFunctionsLoaded = 0;
}

void
source_pane_centerOnAddress(e9ui_component_t *comp, e9ui_context_t *ctx, uint32_t addr)
{
    if (!comp) {
        return;
    }
    source_pane_state_t *st = (source_pane_state_t*)comp->state;
    if (!st) {
        return;
    }
    st->overrideActive = 1;
    st->overrideAddr = (uint64_t)(addr & 0x00ffffffu);
    st->lastResolvedPc = 0;
    st->manualSrcActive = 0;

    TTF_Font *useFont = source_pane_resolveFont(ctx);
    int maxLines = 1;
    if (useFont) {
        source_pane_line_metrics_t metrics = source_pane_computeLineMetrics(comp, useFont, 10);
        maxLines = metrics.maxLines > 0 ? metrics.maxLines : 1;
    }

    st->curSrcLine = 0;
    st->curSrcPath[0] = '\0';
    source_pane_updateSourceLocation(st, 1);
    if (st->curSrcLine > 0) {
        int start = st->curSrcLine - (maxLines / 2);
        if (start < 1) {
            start = 1;
        }
        st->scrollLine = start;
        st->scrollLineValid = 1;
    }

    int idx = 0;
    if (dasm_findIndexForAddr((uint64_t)addr, &idx)) {
        int start = idx - (maxLines / 2);
        if (start < 0 && !(dasm_getFlags() & DASM_IFACE_FLAG_STREAMING)) {
            start = 0;
        }
        st->scrollIndex = start;
        st->scrollIndexValid = 1;
    }
    st->gutterPending = 0;
}

int
source_pane_getCurrentFile(e9ui_component_t *comp, char *out, size_t cap)
{
    if (!out || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!comp) {
        return 0;
    }
    source_pane_state_t *st = (source_pane_state_t*)comp->state;
    if (!st || st->viewMode != source_pane_mode_c) {
        return 0;
    }
    if (st->manualSrcActive && st->manualSrcPath && st->manualSrcPath[0]) {
        strncpy(out, st->manualSrcPath, cap - 1);
        out[cap - 1] = '\0';
        return 1;
    }
    if (!st->overrideActive && machine_getRunning(debugger.machine)) {
        if (!st->curSrcPath[0]) {
            return 0;
        }
    } else {
        source_pane_updateSourceLocation(st, 0);
    }
    if (!st->curSrcPath[0]) {
        return 0;
    }
    strncpy(out, st->curSrcPath, cap - 1);
    out[cap - 1] = '\0';
    return 1;
}
