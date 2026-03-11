/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include "atarist_core_options.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "libretro_host.h"

typedef struct atarist_core_options_kv {
    char *key;
    char *value;
} atarist_core_options_kv_t;

static atarist_core_options_kv_t *atarist_coreOptions_entries = NULL;
static size_t atarist_coreOptions_entryCount = 0;
static size_t atarist_coreOptions_entryCap = 0;
static int atarist_coreOptions_dirty = 0;

static const char *
atarist_coreOptions_basename(const char *path)
{
    if (!path || !*path) {
        return NULL;
    }
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    const char *sep = slash > back ? slash : back;
    return sep ? sep + 1 : path;
}

static void
atarist_coreOptions_trimRight(char *text)
{
    if (!text) {
        return;
    }
    size_t len = strlen(text);
    while (len > 0) {
        char c = text[len - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            text[len - 1] = '\0';
            len--;
            continue;
        }
        break;
    }
}

static char *
atarist_coreOptions_trimLeft(char *text)
{
    if (!text) {
        return NULL;
    }
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return text;
}

static atarist_core_options_kv_t *
atarist_coreOptions_findEntry(const char *key)
{
    if (!key || !*key) {
        return NULL;
    }
    for (size_t i = 0; i < atarist_coreOptions_entryCount; ++i) {
        if (atarist_coreOptions_entries[i].key && strcmp(atarist_coreOptions_entries[i].key, key) == 0) {
            return &atarist_coreOptions_entries[i];
        }
    }
    return NULL;
}

static atarist_core_options_kv_t *
atarist_coreOptions_getOrAddEntry(const char *key)
{
    if (!key || !*key) {
        return NULL;
    }
    atarist_core_options_kv_t *existing = atarist_coreOptions_findEntry(key);
    if (existing) {
        return existing;
    }
    if (atarist_coreOptions_entryCount >= atarist_coreOptions_entryCap) {
        size_t nextCap = atarist_coreOptions_entryCap ? atarist_coreOptions_entryCap * 2 : 64;
        atarist_core_options_kv_t *next =
            (atarist_core_options_kv_t *)alloc_realloc(atarist_coreOptions_entries, nextCap * sizeof(*next));
        if (!next) {
            return NULL;
        }
        atarist_coreOptions_entries = next;
        atarist_coreOptions_entryCap = nextCap;
    }
    atarist_core_options_kv_t *entry = &atarist_coreOptions_entries[atarist_coreOptions_entryCount++];
    memset(entry, 0, sizeof(*entry));
    entry->key = alloc_strdup(key);
    entry->value = alloc_strdup("");
    return entry;
}

static void
atarist_coreOptions_removeEntry(const char *key)
{
    if (!key || !*key || !atarist_coreOptions_entries) {
        return;
    }
    for (size_t i = 0; i < atarist_coreOptions_entryCount; ++i) {
        if (atarist_coreOptions_entries[i].key && strcmp(atarist_coreOptions_entries[i].key, key) == 0) {
            alloc_free(atarist_coreOptions_entries[i].key);
            alloc_free(atarist_coreOptions_entries[i].value);
            for (size_t j = i + 1; j < atarist_coreOptions_entryCount; ++j) {
                atarist_coreOptions_entries[j - 1] = atarist_coreOptions_entries[j];
            }
            atarist_coreOptions_entryCount--;
            return;
        }
    }
}

static int
atarist_coreOptions_parseLine(const char *line, char *outKey, size_t keyCap, char *outValue, size_t valueCap)
{
    if (!line || !outKey || keyCap == 0 || !outValue || valueCap == 0) {
        return 0;
    }
    outKey[0] = '\0';
    outValue[0] = '\0';

    const char *cursor = line;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor == '\0' || *cursor == ';' || *cursor == '#') {
        return 0;
    }

    const char *eq = strchr(cursor, '=');
    if (!eq) {
        return 0;
    }

    size_t keyLen = (size_t)(eq - cursor);
    if (keyLen >= keyCap) {
        keyLen = keyCap - 1;
    }
    memcpy(outKey, cursor, keyLen);
    outKey[keyLen] = '\0';
    atarist_coreOptions_trimRight(outKey);
    char *trimmedKey = atarist_coreOptions_trimLeft(outKey);
    if (trimmedKey != outKey) {
        memmove(outKey, trimmedKey, strlen(trimmedKey) + 1);
    }
    if (!outKey[0]) {
        return 0;
    }

    const char *value = eq + 1;
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    strncpy(outValue, value, valueCap - 1);
    outValue[valueCap - 1] = '\0';
    atarist_coreOptions_trimRight(outValue);
    return 1;
}

int
atarist_coreOptionsDirty(void)
{
    return atarist_coreOptions_dirty ? 1 : 0;
}

void
atarist_coreOptionsClear(void)
{
    if (atarist_coreOptions_entries) {
        for (size_t i = 0; i < atarist_coreOptions_entryCount; ++i) {
            alloc_free(atarist_coreOptions_entries[i].key);
            alloc_free(atarist_coreOptions_entries[i].value);
        }
        alloc_free(atarist_coreOptions_entries);
        atarist_coreOptions_entries = NULL;
    }
    atarist_coreOptions_entryCount = 0;
    atarist_coreOptions_entryCap = 0;
    atarist_coreOptions_dirty = 0;
}

const char *
atarist_coreOptionsGetValue(const char *key)
{
    atarist_core_options_kv_t *entry = atarist_coreOptions_findEntry(key);
    return entry ? entry->value : NULL;
}

void
atarist_coreOptionsSetValue(const char *key, const char *value)
{
    if (!key || !*key) {
        return;
    }
    if (!value) {
        atarist_coreOptions_removeEntry(key);
        atarist_coreOptions_dirty = 1;
        return;
    }
    atarist_core_options_kv_t *entry = atarist_coreOptions_getOrAddEntry(key);
    if (!entry) {
        return;
    }
    alloc_free(entry->value);
    entry->value = alloc_strdup(value);
    atarist_coreOptions_dirty = 1;
}

int
atarist_coreOptionsBuildPath(char *out, size_t cap, const char *saveDir, const char *romPath)
{
    if (!out || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!saveDir || !*saveDir || !romPath || !*romPath) {
        return 0;
    }
    const char *base = atarist_coreOptions_basename(romPath);
    if (!base || !*base) {
        return 0;
    }
    char sep = '/';
    if (strchr(saveDir, '\\')) {
        sep = '\\';
    }
    int needsSep = 1;
    size_t len = strlen(saveDir);
    if (len > 0 && (saveDir[len - 1] == '/' || saveDir[len - 1] == '\\')) {
        needsSep = 0;
    }
    int written = 0;
    if (needsSep) {
        written = snprintf(out, cap, "%s%c%s.core_options", saveDir, sep, base);
    } else {
        written = snprintf(out, cap, "%s%s.core_options", saveDir, base);
    }
    if (written < 0 || (size_t)written >= cap) {
        out[cap - 1] = '\0';
        return 0;
    }
    return 1;
}

int
atarist_coreOptionsLoadFromFile(const char *saveDir, const char *romPath)
{
    char path[PATH_MAX];
    if (!atarist_coreOptionsBuildPath(path, sizeof(path), saveDir, romPath)) {
        atarist_coreOptionsClear();
        return 0;
    }
    atarist_coreOptionsClear();
    FILE *file = fopen(path, "r");
    if (!file) {
        atarist_coreOptions_dirty = 0;
        return 1;
    }
    char line[4096];
    while (fgets(line, sizeof(line), file)) {
        char key[1024];
        char value[3072];
        if (!atarist_coreOptions_parseLine(line, key, sizeof(key), value, sizeof(value))) {
            continue;
        }
        atarist_core_options_kv_t *entry = atarist_coreOptions_getOrAddEntry(key);
        if (!entry) {
            continue;
        }
        alloc_free(entry->value);
        entry->value = alloc_strdup(value);
    }
    fclose(file);
    atarist_coreOptions_dirty = 0;
    return 1;
}

int
atarist_coreOptionsWriteToFile(const char *saveDir, const char *romPath)
{
    char path[PATH_MAX];
    if (!atarist_coreOptionsBuildPath(path, sizeof(path), saveDir, romPath)) {
        return 0;
    }
    if (atarist_coreOptions_entryCount == 0) {
        remove(path);
        atarist_coreOptions_dirty = 0;
        return 1;
    }
    char tmpPath[PATH_MAX];
    int tmpWritten = snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
    if (tmpWritten < 0 || tmpWritten >= (int)sizeof(tmpPath)) {
        return 0;
    }
    FILE *out = fopen(tmpPath, "w");
    if (!out) {
        return 0;
    }
    for (size_t i = 0; i < atarist_coreOptions_entryCount; ++i) {
        const char *key = atarist_coreOptions_entries[i].key;
        const char *value = atarist_coreOptions_entries[i].value;
        if (!key || !*key) {
            continue;
        }
        fprintf(out, "%s=%s\n", key, value ? value : "");
    }
    fclose(out);
    remove(path);
    if (rename(tmpPath, path) != 0) {
        remove(tmpPath);
        return 0;
    }
    atarist_coreOptions_dirty = 0;
    return 1;
}

int
atarist_coreOptionsApplyFileToHost(const char *saveDir, const char *romPath)
{
    char path[PATH_MAX];
    if (!atarist_coreOptionsBuildPath(path, sizeof(path), saveDir, romPath)) {
        return 0;
    }
    FILE *file = fopen(path, "r");
    if (!file) {
        return 1;
    }
    char line[4096];
    while (fgets(line, sizeof(line), file)) {
        char key[1024];
        char value[3072];
        if (!atarist_coreOptions_parseLine(line, key, sizeof(key), value, sizeof(value))) {
            continue;
        }
        libretro_host_setCoreOption(key, value);
    }
    fclose(file);
    return 1;
}
