/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "settings.h"
#include "alloc.h"
#include "crt.h"
#include "core_options.h"
#include "debugger.h"
#include "config.h"
#include "list.h"
#include "system_badge.h"
#include "rom_config.h"

//static
//TODO    


static void
settings_rebuildModalBody(e9ui_context_t *ctx);

static e9ui_component_t *
settings_makeSystemBadge(e9ui_context_t *ctx, target_iface_t* system);

static e9ui_component_t *
settings_findFirstVisibleTextbox(e9ui_component_t *comp)
{
    if (!comp) {
        return NULL;
    }
    if (!e9ui_getHidden(comp) && !comp->disabled && !comp->collapsed) {
        if (comp->name && strcmp(comp->name, "e9ui_textbox") == 0) {
            return comp;
        }
        e9ui_child_iterator iter;
        if (e9ui_child_iterateChildren(comp, &iter)) {
            for (e9ui_child_iterator *it = e9ui_child_interateNext(&iter);
                 it;
                 it = e9ui_child_interateNext(&iter)) {
                if (!it->child) {
                    continue;
                }
                e9ui_component_t *found = settings_findFirstVisibleTextbox(it->child);
                if (found) {
                    return found;
                }
            }
        }
    }
    return NULL;
}

static void
settings_focusFirstVisibleTextbox(e9ui_context_t *ctx)
{
    if (!ctx || !e9ui->settingsModal) {
        return;
    }
    e9ui_component_t *firstTextbox = settings_findFirstVisibleTextbox(e9ui->settingsModal);
    if (firstTextbox) {
        e9ui_setFocus(ctx, firstTextbox);
    }
}

static int settings_pendingRebuild = 0;
int settings_coreOptionsDirty = 0;
static int settings_coreOptionsRestartDirty = 0;

void
settings_markCoreOptionsDirtyWithRestart(int restartRequired);

void
settings_markCoreOptionsDirty(void)
{
    settings_markCoreOptionsDirtyWithRestart(1);
}

void
settings_markCoreOptionsDirtyWithRestart(int restartRequired)
{
    settings_coreOptionsDirty = 1;
    if (restartRequired) {
        settings_coreOptionsRestartDirty = 1;
    }
}

void
settings_clearCoreOptionsDirty(void)
{
    settings_coreOptionsDirty = 0;
    settings_coreOptionsRestartDirty = 0;
}

int
settings_coreOptionsNeedsRestart(void)
{
    return settings_coreOptionsRestartDirty ? 1 : 0;
}

// TODO
//static
int
settings_pathExistsFile(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    struct stat statBuffer;
    if (stat(path, &statBuffer) != 0) {
        return 0;
    }
    return S_ISREG(statBuffer.st_mode) ? 1 : 0;
}

// static
// TODO
int
settings_pathExistsDir(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    struct stat statBuffer;
    if (stat(path, &statBuffer) != 0) {
        return 0;
    }
    return S_ISDIR(statBuffer.st_mode) ? 1 : 0;
}


//static
void
settings_copyPath(char *dest, size_t capacity, const char *src)
{
    if (!dest || capacity == 0) {
        return;
    }
    if (!src || !*src) {
        dest[0] = '\0';
        return;
    }
    if (src[0] == '~' && (src[1] == '/' || src[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && *home) {
            int written = snprintf(dest, capacity, "%s%s", home, src + 1);
            if (written < 0 || (size_t)written >= capacity) {
                dest[capacity - 1] = '\0';
            }
            return;
        }
    }
    strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

void
settings_config_setPath(char *dest, size_t capacity, const char *value)
{
    if (!dest || capacity == 0) {
        return;
    }
    if (!value || !*value) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, value, capacity - 1);
    dest[capacity - 1] = '\0';
}


void
settings_config_setValue(char *dest, size_t capacity, const char *value)
{
    if (!dest || capacity == 0) {
        return;
    }
    if (!value || !*value) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, value, capacity - 1);
    dest[capacity - 1] = '\0';
}

static void
settings_copyConfig(e9k_system_config_t *dest, const e9k_system_config_t *src)
{
    if (!dest || !src) {
        return;
    }
    memcpy(dest, src, sizeof(e9k_system_config_t));
}

static void
settings_closeModal(void)
{
    if (!e9ui->settingsModal) {
        return;
    }
    settings_clearCoreOptionsDirty();
    settings_pendingRebuild = 0;
    e9ui_setHidden(e9ui->settingsModal, 1);
    if (!e9ui->pendingRemove) {
        e9ui->pendingRemove = e9ui->settingsModal;
    }
    e9ui->settingsModal = NULL;
    e9ui->settingsSaveButton = NULL;
}

void
settings_cancelModal(void)
{
    if (!e9ui->settingsModal) {
        return;
    }
    settings_copyConfig(&debugger.settingsEdit, &debugger.config);
    target_settingsClearAllOptions();
    settings_clearCoreOptionsDirty();
    settings_closeModal();
}

void
settings_updateButton(int settingsOk)
{
    if (!e9ui->settingsButton) {
        return;
    }
    if (!settingsOk) {
        e9ui_button_setTheme(e9ui->settingsButton, e9ui_theme_button_preset_red());
        e9ui_button_setGlowPulse(e9ui->settingsButton, 1);
    } else {
        e9ui_button_clearTheme(e9ui->settingsButton);
        e9ui_button_setGlowPulse(e9ui->settingsButton, 0);
    }
}


int
settings_configIsOk(void)
{
  return target->configIsOk();
}

int
settings_audioBufferNormalized(int value)
{
    return value > 0 ? value : 50;
}

// TODO
//static

// static
// TODO

static int
settings_needsRestart(void)
{
    target_iface_t *selectedTarget = debugger.settingsEdit.target ? debugger.settingsEdit.target : target;
    int coreSystemChanged = (target != selectedTarget);
    if (!selectedTarget) {
        return 1;
    }

    return coreSystemChanged || selectedTarget->needsRestart();
}

void
settings_updateSaveLabel(void)
{
    if (!e9ui->settingsSaveButton) {
        return;
    }
    target_iface_t *selectedTarget = debugger.settingsEdit.target ? debugger.settingsEdit.target : target;
    const char *label = settings_needsRestart() ? "Save and Restart" : "Save";
    e9ui_button_setLabel(e9ui->settingsSaveButton, label);
    if (selectedTarget) {
        e9ui->settingsSaveButton->disabled = selectedTarget->settingsSaveButtonDisabled();
    }

}



void
settings_refreshSaveLabel(void)
{
    settings_updateSaveLabel();
}

void
settings_applyToolbarMode(void)
{
    if (!e9ui->toolbar || !e9ui->settingsButton) {
        return;
    }
    if (target->configIsOk()) {
        return;
    }
    int childCount = list_count(e9ui->toolbar->children);
    if (childCount <= 0) {
        return;
    }
    e9ui_component_t **kids = (e9ui_component_t**)alloc_calloc((size_t)childCount, sizeof(*kids));
    if (!kids) {
        return;
    }
    int childTotal = e9ui_child_enumerateREMOVETHIS(e9ui->toolbar, &e9ui->ctx, kids, childCount);
    for (int childIndex = 0; childIndex < childTotal; ++childIndex) {
        e9ui_component_t *child = kids[childIndex];
        if (!child) {
            continue;
        }
        if (child == e9ui->settingsButton) {
            continue;
        }
        if (child->name && strcmp(child->name, "e9ui_button") != 0) {
            continue;
        }
        e9ui_childRemove(e9ui->toolbar, child, &e9ui->ctx);
    }
    alloc_free(kids);
    e9ui->profileButton = NULL;
    e9ui->analyseButton = NULL;
    e9ui->speedButton = NULL;
    e9ui->restartButton = NULL;
    e9ui->resetButton = NULL;
}

static int
settings_checkboxGetMargin(const e9ui_context_t *ctx)
{
    int base = e9ui->theme.checkbox.margin;
    if (base <= 0) {
        base = E9UI_THEME_CHECKBOX_MARGIN;
    }
    int scaled = e9ui_scale_px(ctx, base);
    return scaled > 0 ? scaled : base;
}

static int
settings_checkboxGetTextGap(const e9ui_context_t *ctx)
{
    int base = e9ui->theme.checkbox.textGap;
    if (base <= 0) {
        base = E9UI_THEME_CHECKBOX_TEXT_GAP;
    }
    int scaled = e9ui_scale_px(ctx, base);
    return scaled > 0 ? scaled : base;
}

static int
settings_checkboxMeasureWidth(const char *label, e9ui_context_t *ctx)
{
    TTF_Font *font = e9ui->theme.text.source ? e9ui->theme.text.source : (ctx ? ctx->font : NULL);
    int textW = 0;
    int textH = 0;
    if (font && label && *label) {
        TTF_SizeText(font, label, &textW, &textH);
    }
    int lineHeight = font ? TTF_FontHeight(font) : 16;
    int pad = settings_checkboxGetMargin(ctx);
    int height = pad + lineHeight + pad;
    int size = height > 24 ? 24 : (height - 4 > 0 ? height - 4 : 16);
    int gap = settings_checkboxGetTextGap(ctx);
    return size + gap + textW;
}

static int
settings_componentPreferredHeight(e9ui_component_t *comp, e9ui_context_t *ctx, int availW)
{
    if (!comp || !comp->preferredHeight) {
        return 0;
    }
    int h = comp->preferredHeight(comp, ctx, availW);
    return h > 0 ? h : 0;
}

static int
settings_measureTargetBodyHeight(target_iface_t *system, e9ui_context_t *ctx, int availW)
{
    if (!system || !system->settingsBuildModal || !ctx) {
        return 0;
    }
    target_settings_modal_t modal = {0};
    system->settingsBuildModal(ctx, &modal);
    int height = settings_componentPreferredHeight(modal.body, ctx, availW);
    if (modal.body) {
        e9ui_childDestroy(modal.body, ctx);
    }
    if (modal.footerWarning) {
        e9ui_childDestroy(modal.footerWarning, ctx);
    }
    return height;
}

static void
settings_cancel(void)
{
    settings_copyConfig(&debugger.settingsEdit, &debugger.config);
    target_settingsClearAllOptions();
    settings_clearCoreOptionsDirty();
    settings_closeModal();
}

static void
settings_save(void)
{
    int needsRestart = settings_needsRestart();

    target_iface_t *selectedTarget = debugger.settingsEdit.target ? debugger.settingsEdit.target : target;
    if (selectedTarget) {
        selectedTarget->validateSettings();
    }
    settings_copyConfig(&debugger.config, &debugger.settingsEdit);
    target_setTarget(debugger.settingsEdit.target);
    crt_setEnabled(debugger.config.crtEnabled ? 1 : 0);
    debugger_libretroSelectConfig();
    rom_config_syncActiveFromCurrentSystem();
    debugger_applyCoreOptions();
    debugger_refreshElfValid();
    debugger.settingsOk = settings_configIsOk();
    settings_updateButton(debugger.settingsOk);
    settings_applyToolbarMode();
    config_saveConfig();
    if (needsRestart) {
        debugger.restartRequested = 1;
    }
    settings_closeModal();
}

static void
settings_uiClosed(e9ui_component_t *modal, void *user)
{
    (void)modal;
    (void)user;
    settings_cancel();
}

static void
settings_uiCancel(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    (void)user;
    settings_cancel();
}

static void
settings_uiSave(e9ui_context_t *ctx, void *user)
{
    (void)ctx;
    (void)user;
    settings_save();
}

static void
settings_uiDefaults(e9ui_context_t *ctx, void *user)
{
    (void)user;
    if (!ctx || !e9ui->settingsModal) {
        return;
    }
    target_iface_t *selectedTarget = debugger.settingsEdit.target ? debugger.settingsEdit.target : target;
    if (selectedTarget) {
        selectedTarget->settingsDefaults();
    }
    settings_clearCoreOptionsDirty();
    target_settingsClearAllOptions();
    if (selectedTarget) {
        selectedTarget->settingsLoadOptions(&debugger.settingsEdit);
    }
    settings_pendingRebuild = 1;
    e9ui_showTransientMessage("DEFAULTS RESTORED");
}

void
settings_pathChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    char *dest = (char*)user;
    if (!dest) {
        return;
    }
    settings_config_setPath(dest, PATH_MAX, text);
    settings_updateSaveLabel();
}

void
settings_romSelectUpdateAllowEmpty(settings_romselect_state_t *st)
{
    if (!st) {
        return;
    }
    int hasRom = st->romPath && st->romPath[0];
    int hasFolder = st->romFolder && st->romFolder[0];
    int allowRomEmpty = hasFolder ? 1 : 0;
    int allowFolderEmpty = hasRom ? 1 : 0;
    if (st->romSelect) {
        e9ui_fileSelect_setAllowEmpty(st->romSelect, allowRomEmpty);
    }
    if (st->folderSelect) {
        e9ui_fileSelect_setAllowEmpty(st->folderSelect, allowFolderEmpty);
    }
}

static void
settings_applyRomConfigForSelection(settings_romselect_state_t *st)
{
    if (!st) {
        return;
    }
    target_iface_t *selectedTarget = debugger.settingsEdit.target ? debugger.settingsEdit.target : target;
    if (!selectedTarget) {
        return;
    }
    const char *saveDir = NULL;
    const char *romPath = NULL;

    selectedTarget->applyRomConfigForSelection(st, &saveDir, &romPath);
    
    if (!romPath || !*romPath || !saveDir || !*saveDir) {
        return;
    }
    char elfPath[PATH_MAX];
    char sourceDir[PATH_MAX];
    char toolchainPrefix[PATH_MAX];
    int hasElf = 0;
    int hasSource = 0;
    int hasToolchain = 0;
    if (!rom_config_loadSettingsForRom(saveDir, romPath,
                                       elfPath, sizeof(elfPath),
                                       sourceDir, sizeof(sourceDir),
                                       toolchainPrefix, sizeof(toolchainPrefix),
                                       &hasElf, &hasSource, &hasToolchain)) {
        return;
    }
    selectedTarget->settingsSetConfigPaths(hasElf, elfPath, hasSource, sourceDir, hasToolchain, toolchainPrefix);
    
    if (st->elfSelect) {
        e9ui_fileSelect_setText(st->elfSelect, hasElf ? elfPath : "");
    }
    if (st->sourceSelect) {
        e9ui_fileSelect_setText(st->sourceSelect, hasSource ? sourceDir : "");
    }
    if (st->toolchainSelect) {
        e9ui_labeled_textbox_setText(st->toolchainSelect, hasToolchain ? toolchainPrefix : "");
    }
}


void
settings_toolchainPrefixChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    char *prefix = (char*)user;
    if (!prefix) {
        return;
    }
    settings_config_setValue(prefix, PATH_MAX, text ? text : "");
    settings_updateSaveLabel();
}

void
settings_romPathChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    settings_romselect_state_t *st = (settings_romselect_state_t *)user;
    if (!st || st->suppress) {
        return;
    }
    if (!st->romPath) {
        return;
    }
    settings_config_setPath(st->romPath, PATH_MAX, text);
    if (text && *text) {
        st->suppress = 1;
        if (st->romFolder) {
            settings_config_setPath(st->romFolder, PATH_MAX, "");
            if (st->folderSelect) {
                e9ui_fileSelect_setText(st->folderSelect, "");
            }
        }
        st->suppress = 0;
    }
    settings_romSelectUpdateAllowEmpty(st);
    settings_applyRomConfigForSelection(st);
    settings_updateSaveLabel();

    {
        target_iface_t *selectedTarget = debugger.settingsEdit.target ? debugger.settingsEdit.target : target;
        if (selectedTarget) {
            selectedTarget->settingsRomPathChanged(st);
        }
    }

}

void
settings_romFolderChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    settings_romselect_state_t *st = (settings_romselect_state_t *)user;
    if (!st || st->suppress) {
        return;
    }
    if (!st->romFolder) {
        return;
    }
    settings_config_setPath(st->romFolder, PATH_MAX, text);
    if (text && *text) {
        st->suppress = 1;
        if (st->romPath) {
            settings_config_setPath(st->romPath, PATH_MAX, "");
            if (st->romSelect) {
                e9ui_fileSelect_setText(st->romSelect, "");
            }
        }
        st->suppress = 0;
    }
    settings_romSelectUpdateAllowEmpty(st);
    settings_applyRomConfigForSelection(st);
    settings_updateSaveLabel();
    {
        target_iface_t *selectedTarget = debugger.settingsEdit.target ? debugger.settingsEdit.target : target;
        if (selectedTarget) {
            selectedTarget->settingsRomFolderChanged();
        }
    }
}

void
settings_audioChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    int *dest = (int*)user;
    if (!dest) {
        return;
    }
    if (!text || !*text) {
        *dest = 0;
        settings_updateSaveLabel();
        return;
    }
    char *end = NULL;
    long ms = strtol(text, &end, 10);
    if (!end || end == text) {
        *dest = 0;
        settings_updateSaveLabel();
        return;
    }
    if (ms < 0) {
        ms = 0;
    }
    if (ms > INT_MAX) {
        ms = INT_MAX;
    }
    *dest = (int)ms;
    settings_updateSaveLabel();
}

static void
settings_funChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    (void)user;
    if (selected) {
        if (e9ui->transition.mode == e9k_transition_none) {
            e9ui->transition.mode = e9k_transition_random;
        }
    } else {
        e9ui->transition.mode = e9k_transition_none;
    }
    e9ui->transition.fullscreenModeSet = 0;
    config_saveConfig();
}

static void
settings_crtChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    int *flag = (int *)user;
    if (!flag) {
        return;
    }
    *flag = selected ? 1 : 0;
    settings_updateSaveLabel();
}

static void
settings_coreSystemSync(settings_coresystem_state_t *st, target_iface_t* system, e9ui_context_t *ctx)
{
    if (!st || !system) {
        return;
    }
    st->updating = 1;
    target_iface_t* current = st->target;
    int systemChanged = (current != system);
    st->target = system;
    debugger.settingsEdit.target = system;

    system->settingsCoreChanged();

    int amigaSelected = (system == target_amiga());
    int neogeoSelected = (system == target_neogeo());
#if E9K_ENABLE_MEGADRIVE
    int megadriveSelected = (system == target_megadrive());
#endif
    if (st->allowRebuild && systemChanged) {
        st->updating = 0;
        settings_pendingRebuild = 1;
        return;
    }
    if (st->neogeoCheckbox) {
        e9ui_checkbox_setSelected(st->neogeoCheckbox, neogeoSelected, ctx);
    }
    if (st->amigaCheckbox) {
        e9ui_checkbox_setSelected(st->amigaCheckbox, amigaSelected, ctx);
    }
    if (st->megadriveCheckbox) {
#if E9K_ENABLE_MEGADRIVE
        e9ui_checkbox_setSelected(st->megadriveCheckbox, megadriveSelected, ctx);
#else
        e9ui_checkbox_setSelected(st->megadriveCheckbox, 0, ctx);
#endif
    }
    st->updating = 0;
    settings_updateSaveLabel();
}

static void
settings_coreSystemNeoGeoChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    settings_coresystem_state_t *st = (settings_coresystem_state_t *)user;
    if (!st || st->updating) {
        return;
    }
    if (selected) {
        settings_coreSystemSync(st, target_neogeo(), ctx);
    } else if (st->target == target_neogeo()) {
        settings_coreSystemSync(st, target_amiga(), ctx);
    }
}

static void
settings_coreSystemAmigaChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    settings_coresystem_state_t *st = (settings_coresystem_state_t *)user;
    if (!st || st->updating) {
        return;
    }
    if (selected) {
        settings_coreSystemSync(st, target_amiga(), ctx);
    } else if (st->target == target_amiga()) {
        settings_coreSystemSync(st, target_neogeo(), ctx);
    }
}

#if E9K_ENABLE_MEGADRIVE
static void
settings_coreSystemMegaDriveChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    settings_coresystem_state_t *st = (settings_coresystem_state_t *)user;
    if (!st || st->updating) {
        return;
    }
    if (selected) {
        settings_coreSystemSync(st, target_megadrive(), ctx);
    } else if (st->target == target_megadrive()) {
        settings_coreSystemSync(st, target_amiga(), ctx);
    }
}
#endif

static e9ui_component_t *
settings_makeSystemBadge(e9ui_context_t *ctx, target_iface_t* system)
{
    if (!ctx || !ctx->renderer) {
        return NULL;
    }
    int w = 0;
    int h = 0;
    SDL_Texture *tex = system_badge_getTexture(ctx->renderer, system, &w, &h);
    if (!tex) {
        return NULL;
    }
    e9ui_component_t *img = e9ui_image_makeFromTexture(tex, w, h);
    if (!img) {
        return NULL;
    }
    e9ui_component_t *box = e9ui_box_make(img);
    if (!box) {
        return img;
    }
    e9ui_box_setWidth(box, e9ui_dim_fixed, 139);
    e9ui_box_setHeight(box, e9ui_dim_fixed, 48);
    return box;
}

static e9ui_component_t *
settings_buildModalBody(e9ui_context_t *ctx)
{
    if (!ctx) {
        return NULL;
    }
    target_iface_t *selectedTarget = debugger.settingsEdit.target ? debugger.settingsEdit.target : target;
#if !E9K_ENABLE_MEGADRIVE
    if (selectedTarget == target_megadrive()) {
        selectedTarget = target_amiga();
    }
#endif
    int amigaSelected = (selectedTarget == target_amiga()) ? 1 : 0;
    int neogeoSelected = (selectedTarget == target_neogeo()) ? 1 : 0;
    int megadriveSelected = 0;
#if E9K_ENABLE_MEGADRIVE
    megadriveSelected = (selectedTarget == target_megadrive()) ? 1 : 0;
#endif
    if (!amigaSelected && !neogeoSelected && !megadriveSelected) {
        amigaSelected = 1;
    }

    settings_coresystem_state_t *coreState = (settings_coresystem_state_t *)alloc_calloc(1, sizeof(*coreState));
    e9ui_component_t *cbNeogeo = e9ui_checkbox_make("NEO GEO", neogeoSelected, settings_coreSystemNeoGeoChanged, coreState);
    e9ui_component_t *cbAmiga = e9ui_checkbox_make("AMIGA", amigaSelected, settings_coreSystemAmigaChanged, coreState);
    e9ui_component_t *cbMegaDrive = NULL;
#if E9K_ENABLE_MEGADRIVE
    cbMegaDrive = e9ui_checkbox_make("MEGA DRIVE", megadriveSelected, settings_coreSystemMegaDriveChanged, coreState);
#endif
    e9ui_component_t *rowCore = e9ui_hstack_make();
    e9ui_component_t *rowCoreCenter = rowCore ? e9ui_center_make(rowCore) : NULL;
    e9ui_component_t *btnCoreOptionsTop = e9ui_button_make("Core Options", core_options_uiOpen, NULL);
    e9ui_setTooltip(btnCoreOptionsTop, "Libretro core options");

    e9ui_component_t *badge = settings_makeSystemBadge(ctx, selectedTarget ? selectedTarget : target);
    e9ui_component_t *rowHeader = NULL;
    if (badge && rowCoreCenter) {
        rowHeader = e9ui_hstack_make();
        if (rowHeader) {
            int badgeWPx = e9ui_scale_px(ctx, 139);
            int gapPx = e9ui_scale_px(ctx, 12);
            e9ui_hstack_addFixed(rowHeader, badge, badgeWPx);
            e9ui_hstack_addFixed(rowHeader, e9ui_spacer_make(gapPx), gapPx);
            e9ui_hstack_addFlex(rowHeader, rowCoreCenter);
        } else {
            e9ui_childDestroy(badge, ctx);
            badge = NULL;
        }
    }

    int funSelected = (e9ui->transition.mode != e9k_transition_none);
    e9ui_component_t *cbFun = e9ui_checkbox_make("FUN", funSelected, settings_funChanged, NULL);
    e9ui_component_t *cbCrt = e9ui_checkbox_make("CRT",
                                                 debugger.settingsEdit.crtEnabled,
                                                 settings_crtChanged,
                                                 &debugger.settingsEdit.crtEnabled);
    e9ui_component_t *rowGlobal = e9ui_hstack_make();
    e9ui_component_t *rowGlobalCenter = rowGlobal ? e9ui_center_make(rowGlobal) : NULL;

    if (coreState) {
        coreState->neogeoCheckbox = cbNeogeo;
        coreState->amigaCheckbox = cbAmiga;
        coreState->megadriveCheckbox = cbMegaDrive;
        coreState->target = selectedTarget;
        coreState->allowRebuild = 0;
        settings_coreSystemSync(coreState, selectedTarget ? selectedTarget : target, ctx);
        coreState->allowRebuild = 1;
    }

    if (rowCore && ctx) {
        int gap = e9ui_scale_px(ctx, 12);
        int wNeogeo = cbNeogeo ? settings_checkboxMeasureWidth("NEO GEO", ctx) : 0;
        int wAmiga = cbAmiga ? settings_checkboxMeasureWidth("AMIGA", ctx) : 0;
        int wMegaDrive = cbMegaDrive ? settings_checkboxMeasureWidth("MEGA DRIVE", ctx) : 0;
        int wCoreOptions = 0;
        int hCoreOptions = 0;
        if (btnCoreOptionsTop) {
            e9ui_button_measure(btnCoreOptionsTop, ctx, &wCoreOptions, &hCoreOptions);
            (void)hCoreOptions;
        }
        int totalW = 0;
        if (cbNeogeo) {
            e9ui_hstack_addFixed(rowCore, cbNeogeo, wNeogeo);
            totalW += wNeogeo;
        }
        if (cbAmiga) {
            if (totalW > 0) {
                e9ui_hstack_addFixed(rowCore, e9ui_spacer_make(gap), gap);
                totalW += gap;
            }
            e9ui_hstack_addFixed(rowCore, cbAmiga, wAmiga);
            totalW += wAmiga;
        }
    if (cbMegaDrive) {
            if (totalW > 0) {
                e9ui_hstack_addFixed(rowCore, e9ui_spacer_make(gap), gap);
                totalW += gap;
            }
            e9ui_hstack_addFixed(rowCore, cbMegaDrive, wMegaDrive);
            totalW += wMegaDrive;
        }
        if (btnCoreOptionsTop && wCoreOptions > 0) {
            if (totalW > 0) {
                e9ui_hstack_addFixed(rowCore, e9ui_spacer_make(gap), gap);
                totalW += gap;
            }
            e9ui_hstack_addFixed(rowCore, btnCoreOptionsTop, wCoreOptions);
            totalW += wCoreOptions;
        }
        if (rowCoreCenter) {
            e9ui_center_setSize(rowCoreCenter, e9ui_unscale_px(ctx, totalW), 0);
        }
    }
    if (rowGlobal && ctx) {
        int gap = e9ui_scale_px(ctx, 12);
        int wFun = cbFun ? settings_checkboxMeasureWidth("FUN", ctx) : 0;
        int wCrt = cbCrt ? settings_checkboxMeasureWidth("CRT", ctx) : 0;
        int totalW = 0;
        if (cbFun) {
            e9ui_hstack_addFixed(rowGlobal, cbFun, wFun);
            totalW += wFun;
        }
        if (cbCrt) {
            if (totalW > 0) {
                e9ui_hstack_addFixed(rowGlobal, e9ui_spacer_make(gap), gap);
                totalW += gap;
            }
            e9ui_hstack_addFixed(rowGlobal, cbCrt, wCrt);
            totalW += wCrt;
        }
        if (rowGlobalCenter) {
            e9ui_center_setSize(rowGlobalCenter, e9ui_unscale_px(ctx, totalW), 0);
        }
    }

    target_settings_modal_t targetModal = {0};
    if (selectedTarget && selectedTarget->settingsBuildModal) {
        selectedTarget->settingsBuildModal(ctx, &targetModal);
    }

    e9ui_component_t *stack = e9ui_stack_makeVertical();
    if (rowHeader) {
        e9ui_stack_addFixed(stack, rowHeader);
        e9ui_stack_addFixed(stack, e9ui_vspacer_make(12));
    } else if (rowCoreCenter) {
        e9ui_stack_addFixed(stack, rowCoreCenter);
        e9ui_stack_addFixed(stack, e9ui_vspacer_make(12));
    } else if (badge) {
        e9ui_stack_addFixed(stack, badge);
        e9ui_stack_addFixed(stack, e9ui_vspacer_make(12));
    }
    if (targetModal.body) {
        e9ui_stack_addFixed(stack, targetModal.body);
    }
    if (rowGlobalCenter) {
        e9ui_stack_addFixed(stack, e9ui_vspacer_make(12));
        e9ui_stack_addFixed(stack, rowGlobalCenter);
    }

    int contentWidth = e9ui_scale_px(ctx, 640);
    int selectedBodyHeight = settings_componentPreferredHeight(targetModal.body, ctx, contentWidth);
    int maxBodyHeight = selectedBodyHeight;
    {
        int h = settings_measureTargetBodyHeight(target_amiga(), ctx, contentWidth);
        if (h > maxBodyHeight) {
            maxBodyHeight = h;
        }
    }
    {
        int h = settings_measureTargetBodyHeight(target_neogeo(), ctx, contentWidth);
        if (h > maxBodyHeight) {
            maxBodyHeight = h;
        }
    }
#if E9K_ENABLE_MEGADRIVE
    {
        int h = settings_measureTargetBodyHeight(target_megadrive(), ctx, contentWidth);
        if (h > maxBodyHeight) {
            maxBodyHeight = h;
        }
    }
#endif

    e9ui_component_t *center = e9ui_center_make(stack);
    if (center) {
        int fixedHeight = 0;
        int stackHeight = settings_componentPreferredHeight(stack, ctx, contentWidth);
        if (stackHeight > 0 && selectedBodyHeight > 0 && maxBodyHeight > selectedBodyHeight) {
            fixedHeight = stackHeight + (maxBodyHeight - selectedBodyHeight);
            if (fixedHeight < stackHeight) {
                fixedHeight = stackHeight;
            }
        }
        e9ui_center_setSize(center, 640, fixedHeight > 0 ? e9ui_unscale_px(ctx, fixedHeight) : 0);
    }
    e9ui_component_t *btnDefaults = e9ui_button_make("Defaults", settings_uiDefaults, NULL);
    e9ui_component_t *btnSave = e9ui_button_make("Save", settings_uiSave, NULL);
    e9ui_component_t *btnCancel = e9ui_button_make("Cancel", settings_uiCancel, NULL);
    e9ui->settingsSaveButton = btnSave;
    settings_updateSaveLabel();
    e9ui_component_t *buttons = e9ui_flow_make();
    e9ui_flow_setPadding(buttons, 0);
    e9ui_flow_setSpacing(buttons, 8);
    e9ui_flow_setWrap(buttons, 0);
    if (btnSave) {
        e9ui_button_setTheme(btnSave, e9ui_theme_button_preset_green());
        e9ui_button_setGlowPulse(btnSave, 1);
        e9ui_flow_add(buttons, btnSave);
    }
    if (btnDefaults) {
        e9ui_flow_add(buttons, btnDefaults);
    }    
    if (btnCancel) {
        e9ui_button_setTheme(btnCancel, e9ui_theme_button_preset_red());
        e9ui_button_setGlowPulse(btnCancel, 1);
        e9ui_flow_add(buttons, btnCancel);
    }
    e9ui_component_t *footer = e9ui_stack_makeVertical();
    if (targetModal.footerWarning) {
        e9ui_stack_addFixed(footer, targetModal.footerWarning);
    }
    if (buttons) {
        e9ui_stack_addFixed(footer, buttons);
    }
    e9ui_component_t *overlay = e9ui_overlay_make(center, footer);
    e9ui_overlay_setAnchor(overlay, e9ui_anchor_bottom_right);
    e9ui_overlay_setMargin(overlay, 12);
    return overlay;
}

static void
settings_rebuildModalBody(e9ui_context_t *ctx)
{
    if (!e9ui->settingsModal || !ctx) {
        return;
    }
    e9ui_component_t *overlay = settings_buildModalBody(ctx);
    if (overlay) {
        e9ui_modal_setBodyChild(e9ui->settingsModal, overlay, ctx);
        settings_focusFirstVisibleTextbox(ctx);
    }
}

void
settings_pollRebuild(e9ui_context_t *ctx)
{
    if (!settings_pendingRebuild) {
        return;
    }
    settings_pendingRebuild = 0;
    if (!e9ui->settingsModal || !ctx) {
        return;
    }
    if (e9ui->pendingRemove == e9ui->settingsModal) {
        return;
    }
    settings_rebuildModalBody(ctx);
}

void
settings_uiOpen(e9ui_context_t *ctx, void *user)
{
    (void)user;
    if (!ctx) {
        return;
    }
    if (e9ui->settingsModal) {
        return;
    }
    settings_clearCoreOptionsDirty();
    int margin = e9ui_scale_px(ctx, 32);
    int modalWidth = ctx->winW - margin * 2;
    int modalHeight = ctx->winH - margin * 2;
    if (modalWidth < 1) modalWidth = 1;
    if (modalHeight < 1) modalHeight = 1;
    e9ui_rect_t rect = { margin, margin, modalWidth, modalHeight };
    settings_copyConfig(&debugger.settingsEdit, &debugger.config);

    debugger.settingsEdit.target = target;
    target_settingsClearAllOptions();
    if (debugger.settingsEdit.target) {
        debugger.settingsEdit.target->settingsLoadOptions(&debugger.settingsEdit);
    }
    
    e9ui->settingsModal = e9ui_modal_show(ctx, "Settings", rect, settings_uiClosed, NULL);
    if (e9ui->settingsModal) {
        e9ui_component_t *overlay = settings_buildModalBody(ctx);
        if (overlay) {
            e9ui_modal_setBodyChild(e9ui->settingsModal, overlay, ctx);
            settings_focusFirstVisibleTextbox(ctx);
        }
    }
}
