/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include "target.h"
#include "debugger.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>

static void
target_atarist_stubSetConfigDefaults(e9k_system_config_t *config)
{
    if (!config) {
        return;
    }
    memset(&config->atarist, 0, sizeof(config->atarist));
}

static const char *
target_atarist_stubDefaultCorePath(void)
{
    debug_error("BUG: target_atarist_stubDefaultCorePath called with E9K_ENABLE_ATARIST=0");
    abort();
    return NULL;
}

static void
target_atarist_stubSetActiveDefaultsFromCurrentSystem(void)
{
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static void
target_atarist_stubApplyActiveSettingsToCurrentSystem(void)
{
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static int
target_atarist_stubConfigIsOk(void)
{
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
    return 0;
}

static int
target_atarist_stubNeedsRestart(void)
{
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
    return 0;
}

static int
target_atarist_stubSettingsSaveButtonDisabled(void)
{
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
    return 1;
}

static void
target_atarist_stubValidateSettings(void)
{
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static void
target_atarist_stubSettingsDefaults(void)
{
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static void
target_atarist_stubApplyRomConfigForSelection(settings_romselect_state_t *st, const char **saveDirP, const char **romPathP)
{
    (void)st; (void)saveDirP; (void)romPathP;
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static void
target_atarist_stubSettingsSetConfigPaths(int hasElf, const char *elfPath, int hasSource, const char *sourceDir, int hasToolchain, const char *toolchainPrefix)
{
    (void)hasElf; (void)elfPath; (void)hasSource; (void)sourceDir; (void)hasToolchain; (void)toolchainPrefix;
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static void
target_atarist_stubSettingsRomPathChanged(settings_romselect_state_t *st)
{
    (void)st;
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static void target_atarist_stubSettingsRomFolderChanged(void) { debug_error("BUG: target_atarist stub"); abort(); }
static void target_atarist_stubSettingsCoreChanged(void) { debug_error("BUG: target_atarist stub"); abort(); }
static void target_atarist_stubSettingsClearOptions(void) { debug_error("BUG: target_atarist stub"); abort(); }

static void
target_atarist_stubSettingsLoadOptions(struct e9k_system_config *st)
{
    (void)st;
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static void
target_atarist_stubSettingsBuildModal(e9ui_context_t *ctx, target_settings_modal_t *out)
{
    (void)ctx; (void)out;
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
}

static const struct e9k_libretro_config *
target_atarist_stubSelectLibretroConfig(const struct e9k_system_config *cfg)
{
    (void)cfg;
    debug_error("BUG: target_atarist stub called with E9K_ENABLE_ATARIST=0");
    abort();
    return NULL;
}

static int target_atarist_stubCoreOptionsHasGeneral(const struct core_options_modal_state *st) { (void)st; abort(); return 0; }
static void target_atarist_stubCoreOptionsSaveClicked(e9ui_context_t *ctx, struct core_options_modal_state *st) { (void)ctx; (void)st; abort(); }
static const char *target_atarist_stubCoreOptionGetValue(const char *key) { (void)key; abort(); return NULL; }
static struct e9k_libretro_config *target_atarist_stubGetLibretroCliConfig(void) { abort(); return NULL; }
static void target_atarist_stubOnVblank(void) { abort(); }
static void target_atarist_stubLibretroSelectConfig(void) { abort(); }
static void target_atarist_stubPickElfToolchainPaths(const char **rawElf, const char **toolchainPrefix) { (void)rawElf; (void)toolchainPrefix; abort(); }
static void target_atarist_stubApplyCoreOptions(void) { abort(); }
static void target_atarist_stubValidateAPI(void) { abort(); }
static int target_atarist_stubAudioEnabled(void) { abort(); return 0; }
static void target_atarist_stubAudioEnable(int enabled) { (void)enabled; abort(); }
static int target_atarist_stubMemoryGetLimits(uint32_t *outMinAddr, uint32_t *outMaxAddr) { (void)outMinAddr; (void)outMaxAddr; abort(); return 0; }
static int target_atarist_stubMemoryTrackGetRanges(target_memory_range_t *outRanges, size_t cap, size_t *outCount) { (void)outRanges; (void)cap; (void)outCount; abort(); return 0; }
static SDL_Texture *target_atarist_stubGetBadgeTexture(SDL_Renderer *renderer, target_iface_t *t, int *outW, int *outH) { (void)renderer; (void)t; (void)outW; (void)outH; abort(); return NULL; }
static void target_atarist_stubConfigControllerPorts(void) { abort(); }
static int target_atarist_stubControllerMapButton(SDL_GameControllerButton button, unsigned *outId) { (void)button; (void)outId; abort(); return 0; }

static target_iface_t target_atarist_stubTarget = {
    .name = "ATARI ST (DISABLED)",
    .defaultCorePath = target_atarist_stubDefaultCorePath,
    .setConfigDefaults = target_atarist_stubSetConfigDefaults,
    .setActiveDefaultsFromCurrentSystem = target_atarist_stubSetActiveDefaultsFromCurrentSystem,
    .applyActiveSettingsToCurrentSystem = target_atarist_stubApplyActiveSettingsToCurrentSystem,
    .configIsOk = target_atarist_stubConfigIsOk,
    .needsRestart = target_atarist_stubNeedsRestart,
    .settingsSaveButtonDisabled = target_atarist_stubSettingsSaveButtonDisabled,
    .validateSettings = target_atarist_stubValidateSettings,
    .settingsDefaults = target_atarist_stubSettingsDefaults,
    .applyRomConfigForSelection = target_atarist_stubApplyRomConfigForSelection,
    .settingsSetConfigPaths = target_atarist_stubSettingsSetConfigPaths,
    .settingsRomPathChanged = target_atarist_stubSettingsRomPathChanged,
    .settingsRomFolderChanged = target_atarist_stubSettingsRomFolderChanged,
    .settingsCoreChanged = target_atarist_stubSettingsCoreChanged,
    .settingsClearOptions = target_atarist_stubSettingsClearOptions,
    .settingsLoadOptions = target_atarist_stubSettingsLoadOptions,
    .settingsBuildModal = target_atarist_stubSettingsBuildModal,
    .selectLibretroConfig = target_atarist_stubSelectLibretroConfig,
    .coreOptionsHasGeneral = target_atarist_stubCoreOptionsHasGeneral,
    .coreOptionsSaveClicked = target_atarist_stubCoreOptionsSaveClicked,
    .coreOptionGetValue = target_atarist_stubCoreOptionGetValue,
    .getLibretroCliConfig = target_atarist_stubGetLibretroCliConfig,
    .onVblank = target_atarist_stubOnVblank,
    .libretroSelectConfig = target_atarist_stubLibretroSelectConfig,
    .pickElfToolchainPaths = target_atarist_stubPickElfToolchainPaths,
    .applyCoreOptions = target_atarist_stubApplyCoreOptions,
    .validateAPI = target_atarist_stubValidateAPI,
    .audioEnabled = target_atarist_stubAudioEnabled,
    .audioEnable = target_atarist_stubAudioEnable,
    .coreIndex = TARGET_ATARIST,
    .mousePort = -1,
    .memoryGetLimits = target_atarist_stubMemoryGetLimits,
    .memoryTrackGetRanges = target_atarist_stubMemoryTrackGetRanges,
    .getBadgeTexture = target_atarist_stubGetBadgeTexture,
    .configControllerPorts = target_atarist_stubConfigControllerPorts,
    .controllerMapButton = target_atarist_stubControllerMapButton,
};

target_iface_t *
target_atarist(void)
{
    return &target_atarist_stubTarget;
}
