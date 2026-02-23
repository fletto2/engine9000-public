/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include <stddef.h>

struct target_iface;

void
rom_config_loadSettingsForSelectedRom(void);

void
rom_config_loadRuntimeStateOnBoot(void);

void
rom_config_syncActiveFromCurrentSystem(void);

void
rom_config_saveOnExit(void);

void
rom_config_saveCurrentRomSettings(void);

void
rom_config_saveSettingsForRom(const char *saveDir, const char *romPath,
                              struct target_iface *targetIface,
                              const char *elfPath, const char *sourceDir,
                              const char *toolchainPrefix);

int
rom_config_loadSettingsForRom(const char *saveDir, const char *romPath,
                              struct target_iface *targetIface,
                              char *outElfPath, size_t elfCap,
                              char *outSourceDir, size_t sourceCap,
                              char *outToolchainPrefix, size_t toolchainCap,
                              int *outHasElf, int *outHasSource, int *outHasToolchain);

const char *
rom_config_getActiveInputBindingValue(const char *key);

void
rom_config_setActiveInputBindingValue(const char *key, const char *value);

void
rom_config_clearActiveInputBindings(void);

extern char rom_config_activeElfPath[PATH_MAX];
extern char rom_config_activeSourceDir[PATH_MAX];
extern char rom_config_activeToolchainPrefix[PATH_MAX];
extern int rom_config_activeInit;
