#pragma once

#include <stddef.h>

int
atarist_coreOptionsDirty(void);

void
atarist_coreOptionsClear(void);

const char *
atarist_coreOptionsGetValue(const char *key);

void
atarist_coreOptionsSetValue(const char *key, const char *value);

int
atarist_coreOptionsBuildPath(char *out, size_t cap, const char *saveDir, const char *romPath);

int
atarist_coreOptionsLoadFromFile(const char *saveDir, const char *romPath);

int
atarist_coreOptionsWriteToFile(const char *saveDir, const char *romPath);

int
atarist_coreOptionsApplyFileToHost(const char *saveDir, const char *romPath);
