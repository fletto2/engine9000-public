#include "debugger.h"
#include "target.h"
#include "emu_geo.h"
#include "rom_config.h"
#include "debugger_platform.h"
#include "core_options.h"
#include "libretro_host.h"
#include "system_badge.h"
#include "alloc.h"

static void
target_megadrive_settingsClearOptions(void)
{

}

static void
target_megadrive_settingsBuildModal(e9ui_context_t *ctx, target_settings_modal_t *out)
{
    if (!out || !ctx) {
        return;
    }
    out->body = NULL;
    out->footerWarning = NULL;

    const char *romExts[] = { "*.bin", "*.gen", "*.md", "*.smd", "*.sms", "*.zip" };
    const char *elfExts[] = { "*.elf" };

    settings_romselect_state_t *romState = (settings_romselect_state_t *)alloc_calloc(1, sizeof(*romState));
    if (romState) {
        romState->romPath = debugger.settingsEdit.megadrive.libretro.romPath;
        romState->corePath = debugger.settingsEdit.megadrive.libretro.corePath;
    }

    e9ui_component_t *fsRom = e9ui_fileSelect_make("ROM", 120, 600, "...", romExts, (int)countof(romExts), E9UI_FILESELECT_FILE);
    e9ui_component_t *fsElf = e9ui_fileSelect_make("ELF", 120, 600, "...", elfExts, 1, E9UI_FILESELECT_FILE);
    e9ui_component_t *fsBios = e9ui_fileSelect_make("BIOS FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsSaves = e9ui_fileSelect_make("SAVES FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsSource = e9ui_fileSelect_make("SOURCE FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsCore = e9ui_fileSelect_make("CORE", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FILE);

    if (fsRom) {
        e9ui_fileSelect_setText(fsRom, debugger.settingsEdit.megadrive.libretro.romPath);
        e9ui_fileSelect_setOnChange(fsRom, settings_romPathChanged, romState);
    }
    if (fsElf) {
        e9ui_fileSelect_setAllowEmpty(fsElf, 1);
        e9ui_fileSelect_setText(fsElf, debugger.settingsEdit.megadrive.libretro.exePath);
        e9ui_fileSelect_setOnChange(fsElf, settings_pathChanged, debugger.settingsEdit.megadrive.libretro.exePath);
    }
    if (fsBios) {
        e9ui_fileSelect_setText(fsBios, debugger.settingsEdit.megadrive.libretro.systemDir);
        e9ui_fileSelect_setOnChange(fsBios, settings_pathChanged, debugger.settingsEdit.megadrive.libretro.systemDir);
    }
    if (fsSaves) {
        e9ui_fileSelect_setText(fsSaves, debugger.settingsEdit.megadrive.libretro.saveDir);
        e9ui_fileSelect_setOnChange(fsSaves, settings_pathChanged, debugger.settingsEdit.megadrive.libretro.saveDir);
    }
    if (fsSource) {
        e9ui_fileSelect_setText(fsSource, debugger.settingsEdit.megadrive.libretro.sourceDir);
        e9ui_fileSelect_setOnChange(fsSource, settings_pathChanged, debugger.settingsEdit.megadrive.libretro.sourceDir);
    }
    if (fsCore) {
        e9ui_fileSelect_setText(fsCore, debugger.settingsEdit.megadrive.libretro.corePath);
        e9ui_fileSelect_setOnChange(fsCore, settings_pathChanged, debugger.settingsEdit.megadrive.libretro.corePath);
    }

    e9ui_component_t *ltToolchain = e9ui_labeled_textbox_make("TOOLCHAIN PREFIX",
                                                               120,
                                                               600,
                                                               settings_toolchainPrefixChanged,
                                                               debugger.settingsEdit.megadrive.libretro.toolchainPrefix);
    if (ltToolchain) {
        e9ui_labeled_textbox_setText(ltToolchain, debugger.settingsEdit.megadrive.libretro.toolchainPrefix);
    }

    e9ui_component_t *ltAudio = e9ui_labeled_textbox_make("AUDIO BUFFER MS",
                                                           120,
                                                           600,
                                                           settings_audioChanged,
                                                           &debugger.settingsEdit.megadrive.libretro.audioBufferMs);
    if (ltAudio) {
        char buf[32];
        int audioValue = debugger.settingsEdit.megadrive.libretro.audioBufferMs;
        if (audioValue > 0) {
            snprintf(buf, sizeof(buf), "%d", audioValue);
            e9ui_labeled_textbox_setText(ltAudio, buf);
        } else {
            e9ui_labeled_textbox_setText(ltAudio, "");
        }
        e9ui_component_t *tbComp = e9ui_labeled_textbox_getTextbox(ltAudio);
        if (tbComp) {
            e9ui_textbox_setNumericOnly(tbComp, 1);
        }
    }

    if (romState) {
        romState->romSelect = fsRom;
        romState->coreSelect = fsCore;
        romState->elfSelect = fsElf;
        romState->sourceSelect = fsSource;
        romState->toolchainSelect = ltToolchain;
        settings_romSelectUpdateAllowEmpty(romState);
    }

    e9ui_component_t *body = e9ui_stack_makeVertical();
    if (body) {
        int first = 1;
        if (fsRom) {
            e9ui_stack_addFixed(body, fsRom);
            first = 0;
        }
        if (fsElf) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsElf);
            first = 0;
        }
        if (ltToolchain) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, ltToolchain);
            first = 0;
        }
        if (fsSource) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsSource);
            first = 0;
        }
        if (fsBios) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsBios);
            first = 0;
        }
        if (fsSaves) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsSaves);
            first = 0;
        }
        if (fsCore) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsCore);
            first = 0;
        }
        if (ltAudio) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, ltAudio);
            first = 0;
        }
    }
    out->body = body;
}

static void
target_megadrive_applyActiveSettingsToCurrentSystem(void)
{
    strncpy(debugger.config.megadrive.libretro.exePath, rom_config_activeElfPath, sizeof(debugger.config.megadrive.libretro.exePath) - 1);
    strncpy(debugger.config.megadrive.libretro.sourceDir, rom_config_activeSourceDir, sizeof(debugger.config.megadrive.libretro.sourceDir) - 1);
    strncpy(debugger.config.megadrive.libretro.toolchainPrefix, rom_config_activeToolchainPrefix, sizeof(debugger.config.megadrive.libretro.toolchainPrefix) - 1);
    debugger.config.megadrive.libretro.exePath[sizeof(debugger.config.megadrive.libretro.exePath) - 1] = '\0';
    debugger.config.megadrive.libretro.sourceDir[sizeof(debugger.config.megadrive.libretro.sourceDir) - 1] = '\0';
    debugger.config.megadrive.libretro.toolchainPrefix[sizeof(debugger.config.megadrive.libretro.toolchainPrefix) - 1] = '\0';
}

static void
target_megadrive_setActiveDefaultsFromCurrentSystem(void)
{
    strncpy(rom_config_activeElfPath, debugger.config.megadrive.libretro.exePath, sizeof(rom_config_activeElfPath) - 1);
    strncpy(rom_config_activeSourceDir, debugger.config.megadrive.libretro.sourceDir, sizeof(rom_config_activeSourceDir) - 1);
    strncpy(rom_config_activeToolchainPrefix, debugger.config.megadrive.libretro.toolchainPrefix, sizeof(rom_config_activeToolchainPrefix) - 1);
}

static int
target_megadrive_configMissingPaths(const e9k_megadrive_config_t *cfg)
{
    if (!cfg) {
        return 1;
    }
    if (!cfg->libretro.corePath[0] ||
        !cfg->libretro.romPath[0] ||
        !cfg->libretro.systemDir[0] ||
        !cfg->libretro.saveDir[0] ||
        !settings_pathExistsFile(cfg->libretro.corePath) ||
        !settings_pathExistsFile(cfg->libretro.romPath) ||
        !settings_pathExistsDir(cfg->libretro.systemDir) ||
        !settings_pathExistsDir(cfg->libretro.saveDir)) {
        return 1;
    }
    if (cfg->libretro.exePath[0] && !settings_pathExistsFile(cfg->libretro.exePath)) {
        return 1;
    }
    if (cfg->libretro.sourceDir[0] && !settings_pathExistsDir(cfg->libretro.sourceDir)) {
        return 1;
    }
    return 0;
}

static int
target_megadrive_configIsOk(void)
{
    return target_megadrive_configMissingPaths(&debugger.config.megadrive) ? 0 : 1;
}

static int
target_megadrive_configIsOkFor(const e9k_megadrive_config_t *cfg)
{
    return target_megadrive_configMissingPaths(cfg) ? 0 : 1;
}

static int
target_megadrive_restartNeededForMegaDrive(const e9k_megadrive_config_t *before, const e9k_megadrive_config_t *after)
{
    if (!before || !after) {
        return 1;
    }
    int romChanged = strcmp(before->libretro.romPath, after->libretro.romPath) != 0;
    int elfChanged = strcmp(before->libretro.exePath, after->libretro.exePath) != 0;
    int toolchainChanged = strcmp(before->libretro.toolchainPrefix, after->libretro.toolchainPrefix) != 0;
    int biosChanged = strcmp(before->libretro.systemDir, after->libretro.systemDir) != 0;
    int savesChanged = strcmp(before->libretro.saveDir, after->libretro.saveDir) != 0;
    int sourceChanged = strcmp(before->libretro.sourceDir, after->libretro.sourceDir) != 0;
    int coreChanged = strcmp(before->libretro.corePath, after->libretro.corePath) != 0;
    int audioBefore = settings_audioBufferNormalized(before->libretro.audioBufferMs);
    int audioAfter = settings_audioBufferNormalized(after->libretro.audioBufferMs);
    int audioChanged = audioBefore != audioAfter;
    return romChanged || elfChanged || toolchainChanged || biosChanged || savesChanged || sourceChanged || coreChanged || audioChanged;
}

static int
target_megadrive_needsRestart(void)
{
    int configChanged = target_megadrive_restartNeededForMegaDrive(&debugger.config.megadrive, &debugger.settingsEdit.megadrive);
    int okBefore = target_megadrive_configIsOkFor(&debugger.config.megadrive);
    int okAfter = target_megadrive_configIsOkFor(&debugger.settingsEdit.megadrive);
    return configChanged || (!okBefore && okAfter);
}

static int
target_megadrive_settingsSaveButtonDisabled(void)
{
    return 0;
}

static void
target_megadrive_validateSettings(void)
{
    if (debugger.settingsEdit.megadrive.libretro.audioBufferMs <= 0) {
        debugger.settingsEdit.megadrive.libretro.audioBufferMs = 50;
    }
    const char *saveDir = debugger.settingsEdit.megadrive.libretro.saveDir[0] ?
        debugger.settingsEdit.megadrive.libretro.saveDir : debugger.settingsEdit.megadrive.libretro.systemDir;
    const char *romPath = debugger.settingsEdit.megadrive.libretro.romPath;
    rom_config_saveSettingsForRom(saveDir, romPath,
                                  debugger.settingsEdit.megadrive.libretro.exePath,
                                  debugger.settingsEdit.megadrive.libretro.sourceDir,
                                  debugger.settingsEdit.megadrive.libretro.toolchainPrefix);
}

static void
target_megadrive_settingsDefault(void)
{
    char romPath[PATH_MAX];
    char elfPath[PATH_MAX];
    settings_copyPath(romPath, sizeof(romPath), debugger.settingsEdit.megadrive.libretro.romPath);
    settings_copyPath(elfPath, sizeof(elfPath), debugger.settingsEdit.megadrive.libretro.exePath);
    int audioEnabled = debugger.settingsEdit.megadrive.libretro.audioEnabled;
    debugger_platform_setDefaultsMegaDrive(&debugger.settingsEdit.megadrive);
    debugger.settingsEdit.megadrive.libretro.audioEnabled = audioEnabled;
    settings_copyPath(debugger.settingsEdit.megadrive.libretro.romPath, sizeof(debugger.settingsEdit.megadrive.libretro.romPath), romPath);
    settings_copyPath(debugger.settingsEdit.megadrive.libretro.exePath, sizeof(debugger.settingsEdit.megadrive.libretro.exePath), elfPath);
}

static void
target_megadrive_applyRomConfigForSelection(settings_romselect_state_t *st, const char **saveDirP, const char **romPathP)
{
    (void)st;
    *saveDirP = debugger.settingsEdit.megadrive.libretro.saveDir[0] ?
        debugger.settingsEdit.megadrive.libretro.saveDir : debugger.settingsEdit.megadrive.libretro.systemDir;
    *romPathP = debugger.settingsEdit.megadrive.libretro.romPath;
}

static void
target_megadrive_settingsSetConfigPaths(int hasElf, const char *elfPath, int hasSource, const char *sourceDir, int hasToolchain, const char *toolchainPrefix)
{
    settings_config_setPath(debugger.settingsEdit.megadrive.libretro.exePath, PATH_MAX, hasElf ? elfPath : "");
    settings_config_setPath(debugger.settingsEdit.megadrive.libretro.sourceDir, PATH_MAX, hasSource ? sourceDir : "");
    settings_config_setValue(debugger.settingsEdit.megadrive.libretro.toolchainPrefix, PATH_MAX, hasToolchain ? toolchainPrefix : "");
}

static const char *
target_megadrive_defaultCorePath(void)
{
    return "./system/blastem_libretro.dylib";
}

static void
target_megadrive_settingsRomPathChanged(settings_romselect_state_t *st)
{
    (void)st;
}

static void
target_megadrive_settingsFolderChanged(void)
{

}

static void
target_megadrive_settingsCoreChanged(void)
{
    const char *defaultCore = target_megadrive_defaultCorePath();
    if (defaultCore &&
        defaultCore[0] &&
        (!debugger.settingsEdit.megadrive.libretro.corePath[0] ||
         target_isDefaultCorePath(debugger.settingsEdit.megadrive.libretro.corePath))) {
        settings_config_setPath(debugger.settingsEdit.megadrive.libretro.corePath, PATH_MAX, defaultCore);
    }
}

static void
target_megadrive_settingsLoadOptions(e9k_system_config_t *st)
{
    (void)st;
}

const e9k_libretro_config_t *
target_megadrive_selectLibretroConfig(const e9k_system_config_t *cfg)
{
    return &cfg->megadrive.libretro;
}

static int
target_megadrive_coreOptionsHasGeneral(const core_options_modal_state_t *st)
{
    (void)st;
    return 1;
}

static void
target_megadrive_coreOptionsSaveClicked(e9ui_context_t *ctx, core_options_modal_state_t *st)
{
    (void)ctx;
    (void)st;
    settings_refreshSaveLabel();
    e9ui_showTransientMessage("CORE OPTIONS: NO CHANGES");
    core_options_closeModal();
}

const char *
target_megadrive_coreOptionGetValue(const char *key)
{
    (void)key;
    return NULL;
}

static e9k_libretro_config_t *
target_megadrive_getLibretroCliConfig(void)
{
    return &debugger.cliConfig.megadrive.libretro;
}

static void
target_megadrive_onVblank(void)
{

}

static void
target_megadrive_libretroSelectConfig(void)
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
target_megadrive_pickElfToolchainPaths(const char **rawElf, const char **toolchainPrefix)
{
    *rawElf = debugger.config.megadrive.libretro.exePath;
    *toolchainPrefix = debugger.config.megadrive.libretro.toolchainPrefix;
}

static void
target_megadrive_applyCoreOptions(void)
{

}

static void
target_megadrive_validateAPI(void)
{

}

static int
target_megadrive_audioEnabled(void)
{
    return debugger.config.megadrive.libretro.audioEnabled;
}

static void
target_megadrive_audioEnable(int enabled)
{
    debugger.config.megadrive.libretro.audioEnabled = enabled;
}

static SDL_Texture *
target_megadrive_getBadgeTexture(SDL_Renderer *renderer, target_iface_t *t, int *outW, int *outH)
{
    if (t->badge && t->badgeRenderer != renderer) {
        SDL_DestroyTexture(t->badge);
        t->badge = NULL;
    }
    t->badgeRenderer = renderer;
    if (!t->badge) {
        t->badge = system_badge_loadTexture(renderer, "assets/neogeo.png", &t->badgeW, &t->badgeH);
    }

    if (t->badge) {
        if (outW) {
            *outW = t->badgeW;
        }
        if (outH) {
            *outH = t->badgeH;
        }
        return t->badge;
    }

    return t->badge;
}

static void
target_megadrive_configControllerPorts(void)
{

}

static int
target_megadrive_memoryGetLimits(uint32_t *outMinAddr, uint32_t *outMaxAddr)
{
    if (outMinAddr) {
        *outMinAddr = 0x00000000u;
    }
    if (outMaxAddr) {
        *outMaxAddr = 0x00ffffffu;
    }
    return 1;
}

static int
target_megadrive_controllerMapButton(SDL_GameControllerButton button, unsigned *outId)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A: *outId = RETRO_DEVICE_ID_JOYPAD_B; return 1;
    case SDL_CONTROLLER_BUTTON_B: *outId = RETRO_DEVICE_ID_JOYPAD_A; return 1;
    case SDL_CONTROLLER_BUTTON_X: *outId = RETRO_DEVICE_ID_JOYPAD_Y; return 1;
    case SDL_CONTROLLER_BUTTON_Y: *outId = RETRO_DEVICE_ID_JOYPAD_X; return 1;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: *outId = RETRO_DEVICE_ID_JOYPAD_L; return 1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: *outId = RETRO_DEVICE_ID_JOYPAD_R; return 1;
    case SDL_CONTROLLER_BUTTON_START: *outId = RETRO_DEVICE_ID_JOYPAD_START; return 1;
    case SDL_CONTROLLER_BUTTON_BACK: *outId = RETRO_DEVICE_ID_JOYPAD_SELECT; return 1;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: *outId = RETRO_DEVICE_ID_JOYPAD_UP; return 1;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: *outId = RETRO_DEVICE_ID_JOYPAD_DOWN; return 1;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: *outId = RETRO_DEVICE_ID_JOYPAD_LEFT; return 1;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: *outId = RETRO_DEVICE_ID_JOYPAD_RIGHT; return 1;
    default:
        break;
    }

    return 0;
}

static target_iface_t _target_megadrive = {
    .name = "MEGA DRIVE",
    .dasm = &dasm_geo_iface,
    .emu = &emu_geo_iface,
    .setActiveDefaultsFromCurrentSystem = target_megadrive_setActiveDefaultsFromCurrentSystem,
    .applyActiveSettingsToCurrentSystem = target_megadrive_applyActiveSettingsToCurrentSystem,
    .configIsOk = target_megadrive_configIsOk,
    .needsRestart = target_megadrive_needsRestart,
    .settingsSaveButtonDisabled = target_megadrive_settingsSaveButtonDisabled,
    .validateSettings = target_megadrive_validateSettings,
    .settingsDefaults = target_megadrive_settingsDefault,
    .applyRomConfigForSelection = target_megadrive_applyRomConfigForSelection,
    .settingsSetConfigPaths = target_megadrive_settingsSetConfigPaths,
    .defaultCorePath = target_megadrive_defaultCorePath,
    .settingsRomPathChanged = target_megadrive_settingsRomPathChanged,
    .settingsRomFolderChanged = target_megadrive_settingsFolderChanged,
    .settingsCoreChanged = target_megadrive_settingsCoreChanged,
    .settingsClearOptions = target_megadrive_settingsClearOptions,
    .settingsLoadOptions = target_megadrive_settingsLoadOptions,
    .settingsBuildModal = target_megadrive_settingsBuildModal,
    .selectLibretroConfig = target_megadrive_selectLibretroConfig,
    .coreOptionsHasGeneral = target_megadrive_coreOptionsHasGeneral,
    .coreOptionsSaveClicked = target_megadrive_coreOptionsSaveClicked,
    .coreOptionGetValue = target_megadrive_coreOptionGetValue,
    .getLibretroCliConfig = target_megadrive_getLibretroCliConfig,
    .onVblank = target_megadrive_onVblank,
    .coreIndex = TARGET_MEGADRIVE,
    .libretroSelectConfig = target_megadrive_libretroSelectConfig,
    .pickElfToolchainPaths = target_megadrive_pickElfToolchainPaths,
    .applyCoreOptions = target_megadrive_applyCoreOptions,
    .validateAPI = target_megadrive_validateAPI,
    .audioEnabled = target_megadrive_audioEnabled,
    .audioEnable = target_megadrive_audioEnable,
    .mousePort = -1,
    .memoryGetLimits = target_megadrive_memoryGetLimits,
    .getBadgeTexture = target_megadrive_getBadgeTexture,
    .configControllerPorts = target_megadrive_configControllerPorts,
    .controllerMapButton = target_megadrive_controllerMapButton,
};

target_iface_t *
target_megadrive(void)
{
    return &_target_megadrive;
}
