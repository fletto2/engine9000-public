/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include "e9ui.h"

struct e9k_amiga_config;

typedef struct settings_romselect_state {
    char *romPath;
    char *romFolder;
    char *corePath;
    e9ui_component_t *romSelect;
    e9ui_component_t *folderSelect;
    e9ui_component_t *coreSelect;
    e9ui_component_t *elfSelect;
    e9ui_component_t *sourceSelect;
    e9ui_component_t *toolchainSelect;
    void *targetUser;
    int suppress;
} settings_romselect_state_t;


typedef struct settings_coresystem_state {
    e9ui_component_t       *neogeoCheckbox;
    e9ui_component_t       *amigaCheckbox;
    e9ui_component_t       *megadriveCheckbox;
    struct target_iface    *target;
    int                    updating;
    int                    allowRebuild;
} settings_coresystem_state_t;

void
settings_uiOpen(e9ui_context_t *ctx, void *user);

void
settings_cancelModal(void);

int
settings_configIsOk(void);

void
settings_updateButton(int settingsOk);

void
settings_applyToolbarMode(void);

void
settings_pollRebuild(e9ui_context_t *ctx);

void
settings_refreshSaveLabel(void);

void
settings_markCoreOptionsDirty(void);

void
settings_clearCoreOptionsDirty(void);

void
settings_pathChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user);

void
settings_romPathChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user);

void
settings_romFolderChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user);

void
settings_romSelectUpdateAllowEmpty(settings_romselect_state_t *st);

int
settings_pathExistsFile(const char *path);

int
settings_pathHasUaeExtension(const char *path);

int
settings_validateUaeConfig(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user);

e9ui_component_t *
settings_uaeExtensionWarning_make(void);

int
settings_pathExistsDir(const char *path);

int
settings_audioBufferNormalized(int value);

void
settings_copyPath(char *dest, size_t capacity, const char *src);

void
settings_config_setPath(char *dest, size_t capacity, const char *value);

void
settings_config_setValue(char *dest, size_t capacity, const char *value);

void
settings_updateSaveLabel(void);

void
settings_toolchainPrefixChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user);

void
settings_audioChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user);

extern int settings_coreOptionsDirty;
