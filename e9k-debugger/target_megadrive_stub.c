/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include <string.h>

#include "target.h"
#include "debugger.h"
#include "debugger_platform.h"

static void
target_megadrive_stubCopyPath(char *dest, size_t cap, const char *src)
{
    if (!dest || cap == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, cap - 1);
    dest[cap - 1] = '\0';
}

static const char *
target_megadrive_stubDefaultCorePath(void)
{
    return NULL;
}

static void
target_megadrive_stubSetActiveDefaultsFromCurrentSystem(void)
{
}

static void
target_megadrive_stubApplyActiveSettingsToCurrentSystem(void)
{
}

static int
target_megadrive_stubConfigIsOk(void)
{
    return 0;
}

static int
target_megadrive_stubNeedsRestart(void)
{
    return 0;
}

static int
target_megadrive_stubSettingsSaveButtonDisabled(void)
{
    return 1;
}

static void
target_megadrive_stubValidateSettings(void)
{
}

static void
target_megadrive_stubSettingsDefaults(void)
{
    debugger_platform_setDefaultsMegaDrive(&debugger.settingsEdit.megadrive);
}

static void
target_megadrive_stubApplyRomConfigForSelection(settings_romselect_state_t *st, const char **saveDirP, const char **romPathP)
{
    (void)st;
    if (saveDirP) {
        *saveDirP = debugger.settingsEdit.megadrive.libretro.saveDir;
    }
    if (romPathP) {
        *romPathP = debugger.settingsEdit.megadrive.libretro.romPath;
    }
}

static void
target_megadrive_stubSettingsSetConfigPaths(int hasElf,
                                            const char *elfPath,
                                            int hasSource,
                                            const char *sourceDir,
                                            int hasToolchain,
                                            const char *toolchainPrefix)
{
    target_megadrive_stubCopyPath(debugger.settingsEdit.megadrive.libretro.exePath,
                                  sizeof(debugger.settingsEdit.megadrive.libretro.exePath),
                                  hasElf ? elfPath : "");
    target_megadrive_stubCopyPath(debugger.settingsEdit.megadrive.libretro.sourceDir,
                                  sizeof(debugger.settingsEdit.megadrive.libretro.sourceDir),
                                  hasSource ? sourceDir : "");
    target_megadrive_stubCopyPath(debugger.settingsEdit.megadrive.libretro.toolchainPrefix,
                                  sizeof(debugger.settingsEdit.megadrive.libretro.toolchainPrefix),
                                  hasToolchain ? toolchainPrefix : "");
}

static void
target_megadrive_stubSettingsRomPathChanged(settings_romselect_state_t *st)
{
    (void)st;
}

static void
target_megadrive_stubSettingsRomFolderChanged(void)
{
}

static void
target_megadrive_stubSettingsCoreChanged(void)
{
}

static void
target_megadrive_stubSettingsClearOptions(void)
{
}

static void
target_megadrive_stubSettingsLoadOptions(struct e9k_system_config *st)
{
    (void)st;
}

static void
target_megadrive_stubSettingsBuildModal(e9ui_context_t *ctx, target_settings_modal_t *out)
{
    (void)ctx;
    if (!out) {
        return;
    }
    out->body = NULL;
    out->footerWarning = NULL;
}

static const struct e9k_libretro_config *
target_megadrive_stubSelectLibretroConfig(const struct e9k_system_config *cfg)
{
    if (!cfg) {
        return NULL;
    }
    return &cfg->megadrive.libretro;
}

static int
target_megadrive_stubCoreOptionsHasGeneral(const struct core_options_modal_state *st)
{
    (void)st;
    return 0;
}

static void
target_megadrive_stubCoreOptionsSaveClicked(e9ui_context_t *ctx, struct core_options_modal_state *st)
{
    (void)ctx;
    (void)st;
}

static const char *
target_megadrive_stubCoreOptionGetValue(const char *key)
{
    (void)key;
    return NULL;
}

static struct e9k_libretro_config *
target_megadrive_stubGetLibretroCliConfig(void)
{
    return &debugger.cliConfig.megadrive.libretro;
}

static void
target_megadrive_stubOnVblank(void)
{
}

static void
target_megadrive_stubLibretroSelectConfig(void)
{
    debugger.libretro.audioBufferMs = debugger.config.megadrive.libretro.audioBufferMs;
    debugger.libretro.audioEnabled = debugger.config.megadrive.libretro.audioEnabled;
    debugger_copyPath(debugger.libretro.sourceDir, sizeof(debugger.libretro.sourceDir), debugger.config.megadrive.libretro.sourceDir);
    debugger_copyPath(debugger.libretro.exePath, sizeof(debugger.libretro.exePath), debugger.config.megadrive.libretro.exePath);
    debugger_copyPath(debugger.libretro.toolchainPrefix, sizeof(debugger.libretro.toolchainPrefix), debugger.config.megadrive.libretro.toolchainPrefix);
    debugger_copyPath(debugger.libretro.corePath, sizeof(debugger.libretro.corePath), debugger.config.megadrive.libretro.corePath);
    debugger_copyPath(debugger.libretro.romPath, sizeof(debugger.libretro.romPath), debugger.config.megadrive.libretro.romPath);
    debugger_copyPath(debugger.libretro.systemDir, sizeof(debugger.libretro.systemDir), debugger.config.megadrive.libretro.systemDir);
    debugger_copyPath(debugger.libretro.saveDir, sizeof(debugger.libretro.saveDir), debugger.config.megadrive.libretro.saveDir);
}

static void
target_megadrive_stubPickElfToolchainPaths(const char **rawElf, const char **toolchainPrefix)
{
    if (rawElf) {
        *rawElf = debugger.config.megadrive.libretro.exePath;
    }
    if (toolchainPrefix) {
        *toolchainPrefix = debugger.config.megadrive.libretro.toolchainPrefix;
    }
}

static void
target_megadrive_stubApplyCoreOptions(void)
{
}

static void
target_megadrive_stubValidateAPI(void)
{
}

static int
target_megadrive_stubAudioEnabled(void)
{
    return debugger.config.megadrive.libretro.audioEnabled;
}

static void
target_megadrive_stubAudioEnable(int enabled)
{
    debugger.config.megadrive.libretro.audioEnabled = enabled ? 1 : 0;
}

static int
target_megadrive_stubMemoryGetLimits(uint32_t *outMinAddr, uint32_t *outMaxAddr)
{
    if (outMinAddr) {
        *outMinAddr = 0;
    }
    if (outMaxAddr) {
        *outMaxAddr = 0;
    }
    return 0;
}

static SDL_Texture *
target_megadrive_stubGetBadgeTexture(SDL_Renderer *renderer, target_iface_t *t, int *outW, int *outH)
{
    (void)renderer;
    (void)t;
    if (outW) {
        *outW = 0;
    }
    if (outH) {
        *outH = 0;
    }
    return NULL;
}

static void
target_megadrive_stubConfigControllerPorts(void)
{
}

static int
target_megadrive_stubControllerMapButton(SDL_GameControllerButton button, unsigned *outId)
{
    (void)button;
    (void)outId;
    return 0;
}

static target_iface_t target_megadrive_stubTarget = {
    .name = "MEGA DRIVE (DISABLED)",
    .defaultCorePath = target_megadrive_stubDefaultCorePath,
    .setActiveDefaultsFromCurrentSystem = target_megadrive_stubSetActiveDefaultsFromCurrentSystem,
    .applyActiveSettingsToCurrentSystem = target_megadrive_stubApplyActiveSettingsToCurrentSystem,
    .configIsOk = target_megadrive_stubConfigIsOk,
    .needsRestart = target_megadrive_stubNeedsRestart,
    .settingsSaveButtonDisabled = target_megadrive_stubSettingsSaveButtonDisabled,
    .validateSettings = target_megadrive_stubValidateSettings,
    .settingsDefaults = target_megadrive_stubSettingsDefaults,
    .applyRomConfigForSelection = target_megadrive_stubApplyRomConfigForSelection,
    .settingsSetConfigPaths = target_megadrive_stubSettingsSetConfigPaths,
    .settingsRomPathChanged = target_megadrive_stubSettingsRomPathChanged,
    .settingsRomFolderChanged = target_megadrive_stubSettingsRomFolderChanged,
    .settingsCoreChanged = target_megadrive_stubSettingsCoreChanged,
    .settingsClearOptions = target_megadrive_stubSettingsClearOptions,
    .settingsLoadOptions = target_megadrive_stubSettingsLoadOptions,
    .settingsBuildModal = target_megadrive_stubSettingsBuildModal,
    .selectLibretroConfig = target_megadrive_stubSelectLibretroConfig,
    .coreOptionsHasGeneral = target_megadrive_stubCoreOptionsHasGeneral,
    .coreOptionsSaveClicked = target_megadrive_stubCoreOptionsSaveClicked,
    .coreOptionGetValue = target_megadrive_stubCoreOptionGetValue,
    .getLibretroCliConfig = target_megadrive_stubGetLibretroCliConfig,
    .onVblank = target_megadrive_stubOnVblank,
    .libretroSelectConfig = target_megadrive_stubLibretroSelectConfig,
    .pickElfToolchainPaths = target_megadrive_stubPickElfToolchainPaths,
    .applyCoreOptions = target_megadrive_stubApplyCoreOptions,
    .validateAPI = target_megadrive_stubValidateAPI,
    .audioEnabled = target_megadrive_stubAudioEnabled,
    .audioEnable = target_megadrive_stubAudioEnable,
    .coreIndex = TARGET_MEGADRIVE,
    .mousePort = -1,
    .memoryGetLimits = target_megadrive_stubMemoryGetLimits,
    .getBadgeTexture = target_megadrive_stubGetBadgeTexture,
    .configControllerPorts = target_megadrive_stubConfigControllerPorts,
    .controllerMapButton = target_megadrive_stubControllerMapButton,
};

target_iface_t *
target_megadrive(void)
{
    return &target_megadrive_stubTarget;
}
