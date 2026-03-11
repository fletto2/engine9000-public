#include "debugger.h"
#include "target.h"
#include "emu_st.h"
#include "rom_config.h"
#include "debugger_input_bindings.h"
#include "atarist_core_options.h"
#include "debugger_platform.h"
#include "core_options.h"
#include "libretro_host.h"
#include "system_badge.h"
#include "alloc.h"
#include "file.h"
#include "strutil.h"

static const char *
target_atarist_defaultCorePath(void);

static void
target_atarist_setConfigDefaults(e9k_system_config_t *config)
{
    if (!config) {
        return;
    }
    snprintf(config->atarist.libretro.systemDir, sizeof(config->atarist.libretro.systemDir), "./system");
    snprintf(config->atarist.libretro.saveDir, sizeof(config->atarist.libretro.saveDir), "./saves");
    config->atarist.libretro.sourceDir[0] = '\0';
    snprintf(config->atarist.libretro.toolchainPrefix, sizeof(config->atarist.libretro.toolchainPrefix), "m68k-atari-mint");
    config->atarist.libretro.audioBufferMs = 250;
    config->atarist.libretro.exePath[0] = '\0';
    config->atarist.romFolder[0] = '\0';
}

static void
target_atarist_settingsClearOptions(void)
{
    atarist_coreOptionsClear();
}

static void
target_atarist_settingsBuildModal(e9ui_context_t *ctx, target_settings_modal_t *out)
{
    if (!out || !ctx) {
        return;
    }
    out->body = NULL;
    out->footerWarning = NULL;

    const char *romExts[] = { "*.st", "*.stx", "*.msa", "*.dim", "*.ipf", "*.zip" };
    const char *elfExts[] = { "*.elf", "*.tos", "*.prg" };

    settings_romselect_state_t *romState = (settings_romselect_state_t *)alloc_calloc(1, sizeof(*romState));
    if (romState) {
        romState->romPath = debugger.settingsEdit.atarist.libretro.romPath;
    }

    e9ui_component_t *fsRom = e9ui_fileSelect_make("ROM", 120, 600, "...", romExts, (int)countof(romExts), E9UI_FILESELECT_FILE);
    e9ui_component_t *fsElf = e9ui_fileSelect_make("ELF", 120, 600, "...", elfExts, (int)countof(elfExts), E9UI_FILESELECT_FILE);
    e9ui_component_t *fsBios = e9ui_fileSelect_make("TOS ROM FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsSaves = e9ui_fileSelect_make("SAVES FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsSource = e9ui_fileSelect_make("SOURCE FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);

    if (fsRom) {
        e9ui_fileSelect_setText(fsRom, debugger.settingsEdit.atarist.libretro.romPath);
        e9ui_fileSelect_setOnChange(fsRom, settings_romPathChanged, romState);
    }
    if (fsElf) {
        e9ui_fileSelect_setAllowEmpty(fsElf, 1);
        e9ui_fileSelect_setText(fsElf, debugger.settingsEdit.atarist.libretro.exePath);
        e9ui_fileSelect_setOnChange(fsElf, settings_pathChanged, debugger.settingsEdit.atarist.libretro.exePath);
    }
    if (fsBios) {
        e9ui_fileSelect_setText(fsBios, debugger.settingsEdit.atarist.libretro.systemDir);
        e9ui_fileSelect_setOnChange(fsBios, settings_pathChanged, debugger.settingsEdit.atarist.libretro.systemDir);
    }
    if (fsSaves) {
        e9ui_fileSelect_setText(fsSaves, debugger.settingsEdit.atarist.libretro.saveDir);
        e9ui_fileSelect_setOnChange(fsSaves, settings_pathChanged, debugger.settingsEdit.atarist.libretro.saveDir);
    }
    if (fsSource) {
        e9ui_fileSelect_setAllowEmpty(fsSource, 1);
        e9ui_fileSelect_setText(fsSource, debugger.settingsEdit.atarist.libretro.sourceDir);
        e9ui_fileSelect_setOnChange(fsSource, settings_pathChanged, debugger.settingsEdit.atarist.libretro.sourceDir);
    }
    e9ui_component_t *ltToolchain = e9ui_labeled_textbox_make("TOOLCHAIN PREFIX",
                                                               120,
                                                               600,
                                                               settings_toolchainPrefixChanged,
                                                               debugger.settingsEdit.atarist.libretro.toolchainPrefix);
    if (ltToolchain) {
        e9ui_labeled_textbox_setText(ltToolchain, debugger.settingsEdit.atarist.libretro.toolchainPrefix);
    }

    e9ui_component_t *ltAudio = e9ui_labeled_textbox_make("AUDIO BUFFER MS",
                                                           120,
                                                           600,
                                                           settings_audioChanged,
                                                           &debugger.settingsEdit.atarist.libretro.audioBufferMs);
    if (ltAudio) {
        char buf[32];
        int audioValue = debugger.settingsEdit.atarist.libretro.audioBufferMs;
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
        romState->elfSelect = fsElf;
        romState->sourceSelect = fsSource;
        romState->toolchainSelect = ltToolchain;
        settings_romSelectUpdateAllowEmpty(romState);
        settings_romSelectRefreshRecents(romState);
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
        if (ltAudio) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, ltAudio);
        }
    }
    out->body = body;
}

static void
target_atarist_applyActiveSettingsToCurrentSystem(void)
{
    strutil_strlcpy(debugger.config.atarist.libretro.exePath,
                      sizeof(debugger.config.atarist.libretro.exePath),
                      rom_config_activeElfPath);
    strutil_strlcpy(debugger.config.atarist.libretro.sourceDir,
                      sizeof(debugger.config.atarist.libretro.sourceDir),
                      rom_config_activeSourceDir);
    strutil_strlcpy(debugger.config.atarist.libretro.toolchainPrefix,
                      sizeof(debugger.config.atarist.libretro.toolchainPrefix),
                      rom_config_activeToolchainPrefix);
}

static void
target_atarist_setActiveDefaultsFromCurrentSystem(void)
{
    strutil_strlcpy(rom_config_activeElfPath, sizeof(rom_config_activeElfPath),
                      debugger.config.atarist.libretro.exePath);
    strutil_strlcpy(rom_config_activeSourceDir, sizeof(rom_config_activeSourceDir),
                      debugger.config.atarist.libretro.sourceDir);
    strutil_strlcpy(rom_config_activeToolchainPrefix, sizeof(rom_config_activeToolchainPrefix),
                      debugger.config.atarist.libretro.toolchainPrefix);
}

static int
target_atarist_configMissingPaths(const e9k_atarist_config_t *cfg)
{
    if (!cfg) {
        return 1;
    }
    const char *corePath = target_atarist_defaultCorePath();
    if (!corePath || !*corePath ||
        !cfg->libretro.romPath[0] ||
        !cfg->libretro.systemDir[0] ||
        !cfg->libretro.saveDir[0] ||
        !settings_pathExistsFile(corePath) ||
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
target_atarist_configIsOk(void)
{
    return target_atarist_configMissingPaths(&debugger.config.atarist) ? 0 : 1;
}

static int
target_atarist_configIsOkFor(const e9k_atarist_config_t *cfg)
{
    return target_atarist_configMissingPaths(cfg) ? 0 : 1;
}

static int
target_atarist_restartNeeded(const e9k_atarist_config_t *before, const e9k_atarist_config_t *after)
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
    int audioBefore = settings_audioBufferNormalized(before->libretro.audioBufferMs);
    int audioAfter = settings_audioBufferNormalized(after->libretro.audioBufferMs);
    int audioChanged = audioBefore != audioAfter;
    return romChanged || elfChanged || toolchainChanged || biosChanged || savesChanged || sourceChanged || audioChanged;
}

static int
target_atarist_needsRestart(void)
{
    int configChanged = target_atarist_restartNeeded(&debugger.config.atarist, &debugger.settingsEdit.atarist);
    if (settings_coreOptionsDirty) {
        configChanged = 1;
    }
    int okBefore = target_atarist_configIsOkFor(&debugger.config.atarist);
    int okAfter = target_atarist_configIsOkFor(&debugger.settingsEdit.atarist);
    return configChanged || (!okBefore && okAfter);
}

static int
target_atarist_settingsSaveButtonDisabled(void)
{
    return 0;
}

static void
target_atarist_validateSettings(void)
{
    if (debugger.settingsEdit.atarist.libretro.audioBufferMs <= 0) {
        debugger.settingsEdit.atarist.libretro.audioBufferMs = 50;
    }
    const char *saveDir = debugger.settingsEdit.atarist.libretro.saveDir[0] ?
        debugger.settingsEdit.atarist.libretro.saveDir : debugger.settingsEdit.atarist.libretro.systemDir;
    const char *romPath = debugger.settingsEdit.atarist.libretro.romPath;
    if (romPath && *romPath) {
        if (!atarist_coreOptionsWriteToFile(saveDir, romPath)) {
            e9ui_showTransientMessage("CORE OPTIONS SAVE FAILED");
            return;
        }
    }
    atarist_coreOptionsClear();
    rom_config_saveSettingsForRom(saveDir, romPath, target_atarist(),
                                  debugger.settingsEdit.atarist.libretro.exePath,
                                  debugger.settingsEdit.atarist.libretro.sourceDir,
                                  debugger.settingsEdit.atarist.libretro.toolchainPrefix);
}

static void
target_atarist_settingsDefault(void)
{
    char romPath[PATH_MAX];
    char elfPath[PATH_MAX];
    settings_copyPath(romPath, sizeof(romPath), debugger.settingsEdit.atarist.libretro.romPath);
    settings_copyPath(elfPath, sizeof(elfPath), debugger.settingsEdit.atarist.libretro.exePath);
    int audioEnabled = debugger.settingsEdit.atarist.libretro.audioEnabled;
    target_atarist_setConfigDefaults(&debugger.settingsEdit);
    debugger.settingsEdit.atarist.libretro.audioEnabled = audioEnabled;
    settings_copyPath(debugger.settingsEdit.atarist.libretro.romPath, sizeof(debugger.settingsEdit.atarist.libretro.romPath), romPath);
    settings_copyPath(debugger.settingsEdit.atarist.libretro.exePath, sizeof(debugger.settingsEdit.atarist.libretro.exePath), elfPath);
}

static void
target_atarist_applyRomConfigForSelection(settings_romselect_state_t *st, const char **saveDirP, const char **romPathP)
{
    (void)st;
    *saveDirP = debugger.settingsEdit.atarist.libretro.saveDir[0] ?
        debugger.settingsEdit.atarist.libretro.saveDir : debugger.settingsEdit.atarist.libretro.systemDir;
    *romPathP = debugger.settingsEdit.atarist.libretro.romPath;
}

static void
target_atarist_settingsSetConfigPaths(int hasElf, const char *elfPath, int hasSource, const char *sourceDir, int hasToolchain, const char *toolchainPrefix)
{
    settings_config_setPath(debugger.settingsEdit.atarist.libretro.exePath, PATH_MAX, hasElf ? elfPath : "");
    settings_config_setPath(debugger.settingsEdit.atarist.libretro.sourceDir, PATH_MAX, hasSource ? sourceDir : "");
    settings_config_setValue(debugger.settingsEdit.atarist.libretro.toolchainPrefix, PATH_MAX, hasToolchain ? toolchainPrefix : "");
}

static const char *
target_atarist_defaultCorePath(void)
{
    static char corePath[PATH_MAX];
    static char fallbackPath[PATH_MAX];
    char relPath[PATH_MAX];
#if defined(_WIN32)
    const char *ext = "dll";
#elif defined(__APPLE__)
    const char *ext = "dylib";
#else
    const char *ext = "so";
#endif
    snprintf(relPath, sizeof(relPath), "system/st9000.%s", ext);
    if (file_getAssetPath(relPath, corePath, sizeof(corePath))) {
        return corePath;
    }
    snprintf(fallbackPath, sizeof(fallbackPath), "./system/st9000.%s", ext);
    return fallbackPath;
}

static void
target_atarist_settingsRomPathChanged(settings_romselect_state_t *st)
{
    const char *saveDir = debugger.settingsEdit.atarist.libretro.saveDir[0] ?
        debugger.settingsEdit.atarist.libretro.saveDir : debugger.settingsEdit.atarist.libretro.systemDir;
    if (!st || !st->romPath || !st->romPath[0]) {
        atarist_coreOptionsClear();
        return;
    }
    atarist_coreOptionsLoadFromFile(saveDir, st->romPath);
}

static void
target_atarist_settingsFolderChanged(void)
{
}

static void
target_atarist_settingsCoreChanged(void)
{
    const char *saveDir = debugger.settingsEdit.atarist.libretro.saveDir[0] ?
        debugger.settingsEdit.atarist.libretro.saveDir : debugger.settingsEdit.atarist.libretro.systemDir;
    if (debugger.settingsEdit.atarist.libretro.romPath[0]) {
        atarist_coreOptionsLoadFromFile(saveDir, debugger.settingsEdit.atarist.libretro.romPath);
    } else {
        atarist_coreOptionsClear();
    }
}

static void
target_atarist_settingsLoadOptions(e9k_system_config_t *st)
{
    (void)st;
    const char *saveDir = debugger.settingsEdit.atarist.libretro.saveDir[0] ?
        debugger.settingsEdit.atarist.libretro.saveDir : debugger.settingsEdit.atarist.libretro.systemDir;
    if (debugger.settingsEdit.atarist.libretro.romPath[0]) {
        atarist_coreOptionsLoadFromFile(saveDir, debugger.settingsEdit.atarist.libretro.romPath);
    }
}

static const e9k_libretro_config_t *
target_atarist_selectLibretroConfig(const e9k_system_config_t *cfg)
{
    return &cfg->atarist.libretro;
}

static int
target_atarist_coreOptionsHasGeneral(const core_options_modal_state_t *st)
{
    (void)st;
    return 0;
}

static void
target_atarist_coreOptionsSaveClicked(e9ui_context_t *ctx, core_options_modal_state_t *st)
{
    (void)ctx;
    int anyChange = 0;
    int anyRomConfigBindingChange = 0;
    int anyCoreOptionChange = 0;

    for (size_t i = 0; i < st->entryCount; ++i) {
        const char *key = st->entries[i].key;
        const char *value = st->entries[i].value;
        if (!key || !*key) {
            continue;
        }
        if (debugger_input_bindings_isOptionKey(key)) {
            const char *existingBinding = rom_config_getActiveInputBindingValue(key);
            const char *desiredBinding = (value && *value) ? value : NULL;
            if (desiredBinding) {
                const char *defValue = core_options_findDefaultValue(st, key);
                if (defValue && strcmp(defValue, desiredBinding) == 0) {
                    desiredBinding = NULL;
                }
            }
            if (desiredBinding) {
                if (!existingBinding || !core_options_stringsEqual(existingBinding, desiredBinding)) {
                    rom_config_setActiveInputBindingValue(key, desiredBinding);
                    anyChange = 1;
                    anyRomConfigBindingChange = 1;
                }
            } else if (existingBinding) {
                rom_config_setActiveInputBindingValue(key, NULL);
                anyChange = 1;
                anyRomConfigBindingChange = 1;
            }
            continue;
        }
        const char *defValue = core_options_findDefaultValue(st, key);
        const char *desired = NULL;
        if (!defValue || !value || strcmp(defValue, value) != 0) {
            desired = value ? value : "";
        }
        const char *existing = atarist_coreOptionsGetValue(key);
        if (!desired) {
            if (!existing) {
                continue;
            }
            atarist_coreOptionsSetValue(key, NULL);
            anyChange = 1;
            anyCoreOptionChange = 1;
        } else {
            if (existing && core_options_stringsEqual(existing, desired)) {
                continue;
            }
            atarist_coreOptionsSetValue(key, desired);
            anyChange = 1;
            anyCoreOptionChange = 1;
        }
    }
    if (anyChange) {
        settings_markCoreOptionsDirtyWithRestart(anyCoreOptionChange ? 1 : 0);
    }
    if (anyRomConfigBindingChange && (!e9ui || !e9ui->settingsModal)) {
        rom_config_saveCurrentRomSettings();
    }
    settings_refreshSaveLabel();
    if (anyChange) {
        e9ui_showTransientMessage((!e9ui || !e9ui->settingsModal) ? "CORE OPTIONS APPLIED" : "CORE OPTIONS STAGED");
    } else {
        e9ui_showTransientMessage("CORE OPTIONS: NO CHANGES");
    }
    core_options_closeModal();
}

static const char *
target_atarist_coreOptionGetValue(const char *key)
{
    if (debugger_input_bindings_isOptionKey(key)) {
        return rom_config_getActiveInputBindingValue(key);
    }
    return atarist_coreOptionsGetValue(key);
}

static e9k_libretro_config_t *
target_atarist_getLibretroCliConfig(void)
{
    return &debugger.cliConfig.atarist.libretro;
}

static void
target_atarist_onVblank(void)
{
}

static void
target_atarist_libretroSelectConfig(void)
{
    debugger.libretro.audioBufferMs = debugger.config.atarist.libretro.audioBufferMs;
    debugger.libretro.audioEnabled = debugger.config.atarist.libretro.audioEnabled;
    debugger_copyPath(debugger.libretro.sourceDir, sizeof(debugger.libretro.sourceDir), debugger.config.atarist.libretro.sourceDir);
    debugger_copyPath(debugger.libretro.exePath, sizeof(debugger.libretro.exePath), debugger.config.atarist.libretro.exePath);
    debugger_copyPath(debugger.libretro.toolchainPrefix, sizeof(debugger.libretro.toolchainPrefix), debugger.config.atarist.libretro.toolchainPrefix);
    debugger_copyPath(debugger.libretro.romPath, sizeof(debugger.libretro.romPath), debugger.config.atarist.libretro.romPath);
    debugger_copyPath(debugger.libretro.systemDir, sizeof(debugger.libretro.systemDir), debugger.config.atarist.libretro.systemDir);
    debugger_copyPath(debugger.libretro.saveDir, sizeof(debugger.libretro.saveDir), debugger.config.atarist.libretro.saveDir);
}

static void
target_atarist_pickElfToolchainPaths(const char **rawElf, const char **toolchainPrefix)
{
    *rawElf = debugger.config.atarist.libretro.exePath;
    *toolchainPrefix = debugger.config.atarist.libretro.toolchainPrefix;
}

static void
target_atarist_applyCoreOptions(void)
{
    const char *romPath = debugger.libretro.romPath[0] ? debugger.libretro.romPath : debugger.config.atarist.libretro.romPath;
    const char *saveDir = debugger.libretro.saveDir[0] ? debugger.libretro.saveDir : debugger.config.atarist.libretro.saveDir;
    if (romPath && *romPath && saveDir && *saveDir) {
        atarist_coreOptionsApplyFileToHost(saveDir, romPath);
    }
}

static void
target_atarist_validateAPI(void)
{
    libretro_host_unbindNeogeoDebugApis();
    libretro_host_unbindMegaDebugApis();
}

static int
target_atarist_audioEnabled(void)
{
    return debugger.config.atarist.libretro.audioEnabled;
}

static void
target_atarist_audioEnable(int enabled)
{
    debugger.config.atarist.libretro.audioEnabled = enabled;
}

static SDL_Texture *
target_atarist_getBadgeTexture(SDL_Renderer *renderer, target_iface_t *t, int *outW, int *outH)
{
    if (t->badge && t->badgeRenderer != renderer) {
        SDL_DestroyTexture(t->badge);
        t->badge = NULL;
    }
    t->badgeRenderer = renderer;
    if (!t->badge) {
        t->badge = system_badge_loadTexture(renderer, "assets/atarist.png", &t->badgeW, &t->badgeH);
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
target_atarist_configControllerPorts(void)
{
}

/* Atari ST memory map: 68000 with 24-bit addressing */
static int
target_atarist_memoryGetLimits(uint32_t *outMinAddr, uint32_t *outMaxAddr)
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
target_atarist_memoryTrackGetRanges(target_memory_range_t *outRanges, size_t cap, size_t *outCount)
{
    /* ST RAM: typically 512KB-4MB starting at 0 */
    if (outCount) {
        *outCount = 1;
    }
    if (!outRanges || cap == 0) {
        return 1;
    }
    outRanges[0].baseAddr = 0x00000000u;
    outRanges[0].size = 0x00100000u; /* 1MB default */
    return 1;
}

static int
target_atarist_controllerMapButton(SDL_GameControllerButton button, unsigned *outId)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A: *outId = RETRO_DEVICE_ID_JOYPAD_B; return 1;
    case SDL_CONTROLLER_BUTTON_B: *outId = RETRO_DEVICE_ID_JOYPAD_A; return 1;
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

static target_iface_t _target_atarist = {
    .name = "ATARI ST",
    .dasm = &dasm_ami_iface,
    .emu = &emu_st_iface,
    .setConfigDefaults = target_atarist_setConfigDefaults,
    .setActiveDefaultsFromCurrentSystem = target_atarist_setActiveDefaultsFromCurrentSystem,
    .applyActiveSettingsToCurrentSystem = target_atarist_applyActiveSettingsToCurrentSystem,
    .configIsOk = target_atarist_configIsOk,
    .needsRestart = target_atarist_needsRestart,
    .settingsSaveButtonDisabled = target_atarist_settingsSaveButtonDisabled,
    .validateSettings = target_atarist_validateSettings,
    .settingsDefaults = target_atarist_settingsDefault,
    .applyRomConfigForSelection = target_atarist_applyRomConfigForSelection,
    .settingsSetConfigPaths = target_atarist_settingsSetConfigPaths,
    .defaultCorePath = target_atarist_defaultCorePath,
    .settingsRomPathChanged = target_atarist_settingsRomPathChanged,
    .settingsRomFolderChanged = target_atarist_settingsFolderChanged,
    .settingsCoreChanged = target_atarist_settingsCoreChanged,
    .settingsClearOptions = target_atarist_settingsClearOptions,
    .settingsLoadOptions = target_atarist_settingsLoadOptions,
    .settingsBuildModal = target_atarist_settingsBuildModal,
    .selectLibretroConfig = target_atarist_selectLibretroConfig,
    .coreOptionsHasGeneral = target_atarist_coreOptionsHasGeneral,
    .coreOptionsSaveClicked = target_atarist_coreOptionsSaveClicked,
    .coreOptionGetValue = target_atarist_coreOptionGetValue,
    .getLibretroCliConfig = target_atarist_getLibretroCliConfig,
    .onVblank = target_atarist_onVblank,
    .coreIndex = TARGET_ATARIST,
    .libretroSelectConfig = target_atarist_libretroSelectConfig,
    .pickElfToolchainPaths = target_atarist_pickElfToolchainPaths,
    .applyCoreOptions = target_atarist_applyCoreOptions,
    .validateAPI = target_atarist_validateAPI,
    .audioEnabled = target_atarist_audioEnabled,
    .audioEnable = target_atarist_audioEnable,
    .mousePort = 0,
    .memoryGetLimits = target_atarist_memoryGetLimits,
    .memoryTrackGetRanges = target_atarist_memoryTrackGetRanges,
    .getBadgeTexture = target_atarist_getBadgeTexture,
    .configControllerPorts = target_atarist_configControllerPorts,
    .controllerMapButton = target_atarist_controllerMapButton,
};

target_iface_t *
target_atarist(void)
{
    return &_target_atarist;
}
