#include "debugger.h"
#include "target.h"
#include "emu_ami.h"
#include "rom_config.h"
#include "amiga_uae_options.h"
#include "neogeo_core_options.h"
#include "debugger_platform.h"
#include "core_options.h"
#include "libretro_host.h"
#include "system_badge.h"
#include "alloc.h"
#include "file.h"

static const char *
target_amiga_defaultCorePath(void);

static void
target_amiga_setConfigDefaults(e9k_system_config_t *config)
{
    if (!config) {
        return;
    }
    snprintf(config->amiga.libretro.systemDir, sizeof(config->amiga.libretro.systemDir), "./system");
    snprintf(config->amiga.libretro.saveDir, sizeof(config->amiga.libretro.saveDir), "./saves");
    snprintf(config->amiga.libretro.sourceDir, sizeof(config->amiga.libretro.sourceDir), ".");
    snprintf(config->amiga.libretro.toolchainPrefix, sizeof(config->amiga.libretro.toolchainPrefix), "m68k-amigaos-");
    config->amiga.libretro.audioBufferMs = 250;
    config->amiga.libretro.exePath[0] = '\0';
}


typedef struct target_amiga_romselect_extra {
    e9ui_component_t *df0Select;
    e9ui_component_t *df1Select;
    e9ui_component_t *hd0Select;
} target_amiga_romselect_extra_t;



static void
target_amiga_applyActiveSettingsToCurrentSystem(void)
{
    strncpy(debugger.config.amiga.libretro.exePath, rom_config_activeElfPath, sizeof(debugger.config.amiga.libretro.exePath) - 1);
    strncpy(debugger.config.amiga.libretro.sourceDir, rom_config_activeSourceDir, sizeof(debugger.config.amiga.libretro.sourceDir) - 1);
    strncpy(debugger.config.amiga.libretro.toolchainPrefix, rom_config_activeToolchainPrefix, sizeof(debugger.config.amiga.libretro.toolchainPrefix) - 1);
    debugger.config.amiga.libretro.exePath[sizeof(debugger.config.amiga.libretro.exePath) - 1] = '\0';
    debugger.config.amiga.libretro.sourceDir[sizeof(debugger.config.amiga.libretro.sourceDir) - 1] = '\0';
    debugger.config.amiga.libretro.toolchainPrefix[sizeof(debugger.config.amiga.libretro.toolchainPrefix) - 1] = '\0';
}

static void
target_amiga_setActiveDefaultsFromCurrentSystem(void)
{
  strncpy(rom_config_activeElfPath, debugger.config.amiga.libretro.exePath, sizeof(rom_config_activeElfPath) - 1);
  strncpy(rom_config_activeSourceDir, debugger.config.amiga.libretro.sourceDir, sizeof(rom_config_activeSourceDir) - 1);
  strncpy(rom_config_activeToolchainPrefix, debugger.config.amiga.libretro.toolchainPrefix, sizeof(rom_config_activeToolchainPrefix) - 1);
}

static int
target_amiga_configMissingPaths(const e9k_amiga_config_t *cfg)
{
    if (!cfg) {
        return 1;
    }
    const char *corePath = target_amiga_defaultCorePath();
    if (!corePath || !*corePath ||
        !cfg->libretro.romPath[0] ||
        !cfg->libretro.systemDir[0] ||
        !cfg->libretro.saveDir[0] ||
        !settings_pathExistsFile(corePath) ||
        !settings_pathHasUaeExtension(cfg->libretro.romPath) ||
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
target_amiga_configIsOk(void)
{
    return target_amiga_configMissingPaths(&debugger.config.amiga) ? 0 : 1;
}

static int
target_amiga_configIsOkForAmiga(const e9k_amiga_config_t *cfg)
{
    return target_amiga_configMissingPaths(cfg) ? 0 : 1;
}

static int
target_amiga_restartNeededForAmiga(const e9k_amiga_config_t *before, const e9k_amiga_config_t *after)
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
target_amiga_needsRestart(void)
{
    int  configChanged = target_amiga_restartNeededForAmiga(&debugger.config.amiga, &debugger.settingsEdit.amiga);
    if (amiga_uaeUaeOptionsDirty()) {
      configChanged = 1;
    }
    if (settings_coreOptionsDirty) {
      configChanged = 1;
    }
    int okBefore = target_amiga_configIsOkForAmiga(&debugger.config.amiga);
    int okAfter = target_amiga_configIsOkForAmiga(&debugger.settingsEdit.amiga);

    return configChanged ||  (!okBefore && okAfter);
}

static int
target_amiga_settingsSaveButtonDisabled(void)
{
  const char *uaePath = debugger.settingsEdit.amiga.libretro.romPath;
  return (!uaePath || !uaePath[0] || !settings_pathHasUaeExtension(uaePath)) ? 1 : 0;
}

static void
target_amiga_validateSettings(void)
{
  if (debugger.settingsEdit.amiga.libretro.audioBufferMs <= 0) {
    debugger.settingsEdit.amiga.libretro.audioBufferMs = 50;
  }
  const char *uaePath = debugger.settingsEdit.amiga.libretro.romPath;
  if (uaePath && *uaePath) {
    if (!settings_pathHasUaeExtension(uaePath)) {
      e9ui_showTransientMessage("UAE CONFIG MUST END WITH .uae");
      return;
    }
    if (!amiga_uaeWriteUaeOptionsToFile(uaePath)) {
      e9ui_showTransientMessage("UAE SAVE FAILED");
      return;
    }
  }
  amiga_uaeClearPuaeOptions();
  const char *saveDir = debugger.settingsEdit.amiga.libretro.saveDir[0] ?
    debugger.settingsEdit.amiga.libretro.saveDir : debugger.settingsEdit.amiga.libretro.systemDir;
  const char *romPath = debugger.settingsEdit.amiga.libretro.romPath;
  rom_config_saveSettingsForRom(saveDir, romPath,
				debugger.settingsEdit.amiga.libretro.exePath,
				debugger.settingsEdit.amiga.libretro.sourceDir,
				debugger.settingsEdit.amiga.libretro.toolchainPrefix);
  
}

static void
target_amiga_settingsDefault(void)
{  
    char uaePath[PATH_MAX];
    char elfPath[PATH_MAX];
    settings_copyPath(uaePath, sizeof(uaePath), debugger.settingsEdit.amiga.libretro.romPath);
    settings_copyPath(elfPath, sizeof(elfPath), debugger.settingsEdit.amiga.libretro.exePath);
    int audioEnabled = debugger.settingsEdit.amiga.libretro.audioEnabled;
    target_amiga_setConfigDefaults(&debugger.settingsEdit);
    debugger.settingsEdit.amiga.libretro.audioEnabled = audioEnabled;
    settings_copyPath(debugger.settingsEdit.amiga.libretro.romPath, sizeof(debugger.settingsEdit.amiga.libretro.romPath), uaePath);
    settings_copyPath(debugger.settingsEdit.amiga.libretro.exePath, sizeof(debugger.settingsEdit.amiga.libretro.exePath), elfPath);
    amiga_uaeClearPuaeOptions();
    if (debugger.settingsEdit.amiga.libretro.romPath[0]) {
      amiga_uaeLoadUaeOptions(debugger.settingsEdit.amiga.libretro.romPath);
    }
}

static void
target_amiga_applyRomConfigForSelection(settings_romselect_state_t *st, const char** saveDirP, const char** romPathP)
{
  (void)st;
    *saveDirP = debugger.settingsEdit.amiga.libretro.saveDir[0] ?
    debugger.settingsEdit.amiga.libretro.saveDir : debugger.settingsEdit.amiga.libretro.systemDir;
    *romPathP = debugger.settingsEdit.amiga.libretro.romPath;

}

static void
  target_amiga_settingsSetConfigPaths(int hasElf, const char* elfPath, int hasSource, const char* sourceDir, int hasToolchain, const char*toolchainPrefix)
{
  settings_config_setPath(debugger.settingsEdit.amiga.libretro.exePath, PATH_MAX, hasElf ? elfPath : "");
  settings_config_setPath(debugger.settingsEdit.amiga.libretro.sourceDir, PATH_MAX, hasSource ? sourceDir : "");
  settings_config_setValue(debugger.settingsEdit.amiga.libretro.toolchainPrefix, PATH_MAX, hasToolchain ? toolchainPrefix : "");
}


static const char *
target_amiga_defaultCorePath(void)
{
  static char corePath[PATH_MAX];
  if (file_getAssetPath("system/ami9000.dylib", corePath, sizeof(corePath))) {
    return corePath;
  }
  return "./system/ami9000.dylib";
  
}

static void
target_amiga_settingsRomPathChanged(settings_romselect_state_t* st)  
{
  amiga_uaeLoadUaeOptions(st->romPath);
  target_amiga_romselect_extra_t *extra = st ? (target_amiga_romselect_extra_t *)st->targetUser : NULL;
  if (extra && extra->df0Select) {
    const char *df0 = amiga_uaeGetFloppyPath(0);
    e9ui_fileSelect_setText(extra->df0Select, df0 ? df0 : "");
  }
  if (extra && extra->df1Select) {
    const char *df1 = amiga_uaeGetFloppyPath(1);
    e9ui_fileSelect_setText(extra->df1Select, df1 ? df1 : "");
  }
  if (extra && extra->hd0Select) {
    const char *hd0 = amiga_uaeGetHardDriveFolderPath();
    e9ui_fileSelect_setText(extra->hd0Select, hd0 ? hd0 : "");
  }
  settings_updateSaveLabel();
}

static void
target_amiga_settingsFolderChanged(void)
{

}

static void
target_amiga_settingsCoreChanged(void)
{
    amiga_uaeLoadUaeOptions(debugger.settingsEdit.amiga.libretro.romPath);
    neogeo_coreOptionsClear();
}

static void
target_amiga_settingsLoadOptions(e9k_system_config_t * st)
{
  (void)st;
  amiga_uaeLoadUaeOptions(debugger.settingsEdit.amiga.libretro.romPath);
}


const e9k_libretro_config_t*
target_amiga_selectLibretroConfig(const e9k_system_config_t *cfg)
{
  return &cfg->amiga.libretro;
}


static int
target_amiga_coreOptionsHasGeneral(const core_options_modal_state_t *st)
{
  if (!st || !st->defs) {
    return 0;
  }
  for (size_t i = 0; i < st->defCount; ++i) {
    const struct retro_core_option_v2_definition *def = &st->defs[i];
    if (st->targetCoreRunning && def && def->key) {
      if (!libretro_host_isCoreOptionVisible(def->key)) {
	continue;
      }
    }
    const char *defCat = def ? def->category_key : NULL;
    if (!defCat || !*defCat) {
      if (def && def->key) {
	if (strcmp(def->key, "puae_video_options_display") == 0 ||
	    strcmp(def->key, "puae_audio_options_display") == 0 ||
	    strcmp(def->key, "puae_mapping_options_display") == 0 ||
	    strcmp(def->key, "puae_model_options_display") == 0) {
	  return 1;
	}
      }
    }
  }

  return 0;  
}

static void
target_amiga_coreOptionsSaveClicked(e9ui_context_t *ctx,core_options_modal_state_t *st)
{
  (void)ctx;
  int anyChange = 0;
  
  for (size_t i = 0; i < st->entryCount; ++i) {
    const char *key = st->entries[i].key;
    const char *value = st->entries[i].value;
    if (!key || !*key) {
      continue;
    }
    const char *defValue = core_options_findDefaultValue(st, key);
    const char *desired = NULL;
    if (!defValue || !value || strcmp(defValue, value) != 0) {
      desired = value ? value : "";
    }
    const char *existing = amiga_uaeGetPuaeOptionValue(key);
    if (!desired) {
      if (!existing) {
	continue;
      }
      amiga_uaeSetPuaeOptionValue(key, NULL);
      anyChange = 1;
    } else {
      if (existing && core_options_stringsEqual(existing, desired)) {
	continue;
      }
      amiga_uaeSetPuaeOptionValue(key, desired);
      anyChange = 1;
    }
  }
  if (anyChange) {
    settings_markCoreOptionsDirty();
  }
  if (anyChange && e9ui->settingsSaveButton) {
    e9ui_button_setGlowPulse(e9ui->settingsSaveButton, 1);
  }
  settings_refreshSaveLabel();
  e9ui_showTransientMessage(anyChange ? "CORE OPTIONS STAGED" : "CORE OPTIONS: NO CHANGES");
  core_options_closeModal();
}


const char*
target_amiga_coreOptionGetValue(const char* key)
{
  return amiga_uaeGetPuaeOptionValue(key);
}


static e9k_libretro_config_t *
target_amiga_getLibretroCliConfig(void)
{
  return &debugger.cliConfig.amiga.libretro;
}


static void target_amiga_onVblank(void) {}

static void
target_amiga_libretroSelectConfig(void)
{
    debugger.libretro.audioBufferMs = debugger.config.amiga.libretro.audioBufferMs;
    debugger.libretro.audioEnabled = debugger.config.amiga.libretro.audioEnabled;
    debugger_copyPath(debugger.libretro.sourceDir, sizeof(debugger.libretro.sourceDir), debugger.config.amiga.libretro.sourceDir);
    debugger_copyPath(debugger.libretro.exePath, sizeof(debugger.libretro.exePath), debugger.config.amiga.libretro.exePath);
    debugger_copyPath(debugger.libretro.toolchainPrefix, sizeof(debugger.libretro.toolchainPrefix), debugger.config.amiga.libretro.toolchainPrefix);
    debugger_copyPath(debugger.libretro.romPath, sizeof(debugger.libretro.romPath), debugger.config.amiga.libretro.romPath);
    debugger_copyPath(debugger.libretro.systemDir, sizeof(debugger.libretro.systemDir), debugger.config.amiga.libretro.systemDir);
    debugger_copyPath(debugger.libretro.saveDir, sizeof(debugger.libretro.saveDir), debugger.config.amiga.libretro.saveDir);
}

static void
target_amiga_pickElfToolchainPaths(const char** rawElf, const char** toolchainPrefix)
{
  *rawElf = debugger.config.amiga.libretro.exePath;
  *toolchainPrefix = debugger.config.amiga.libretro.toolchainPrefix;
}


static void
target_amiga_applyCoreOptions(void)
{
  const char *uaePath = debugger.libretro.romPath[0] ? debugger.libretro.romPath : debugger.config.amiga.libretro.romPath;
  if (uaePath && *uaePath) {
    amiga_uaeApplyPuaeOptionsToHost(uaePath);
  }
}

static void
target_amiga_validateAPI(void)
{
  libretro_host_unbindNeogeoDebugApis();
  if (!libretro_host_setDebugBaseCallback(debugger_onSetDebugBaseFromCore)) {
    debug_error("debug_base: core does not expose e9k_debug_set_debug_base_callback");
  }
  if (!libretro_host_setDebugBreakpointCallback(debugger_onAddBreakpointFromCore)) {
    debug_error("breakpoint: core does not expose e9k_debug_set_debug_breakpoint_callback");
  }
  int *debugDma = NULL;
  if (libretro_host_debugGetAmigaDebugDmaAddr(&debugDma)) {
    debugger.amigaDebug.debugDma = debugDma;
  }
}


static int
target_amiga_audioEnabled(void)
{
  return debugger.config.amiga.libretro.audioEnabled;
}

static void
target_amiga_audioEnable(int enabled)
{
  debugger.config.amiga.libretro.audioEnabled = enabled;
}

static SDL_Texture *
target_amiga_getBadgeTexture(SDL_Renderer *renderer, target_iface_t* t, int* outW, int* outH)
{
  if (t->badge && t->badgeRenderer != renderer) {
    SDL_DestroyTexture(t->badge);
    t->badge = NULL;
  }
  t->badgeRenderer = renderer;
  if (!t->badge) {
    t->badge = system_badge_loadTexture(renderer, "assets/amiga.png", &t->badgeW, &t->badgeH);
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
target_amiga_configControllerPorts(void)
{  
  libretro_host_setControllerPortDevice(0, RETRO_DEVICE_JOYPAD);
  libretro_host_setControllerPortDevice(1, RETRO_DEVICE_MOUSE);
}


static  int
target_amiga_controllerMapButton(SDL_GameControllerButton button, unsigned *outId)
{
  switch (button) {
  case SDL_CONTROLLER_BUTTON_A: *outId = RETRO_DEVICE_ID_JOYPAD_B; return 1;
  case SDL_CONTROLLER_BUTTON_B: *outId = RETRO_DEVICE_ID_JOYPAD_A; return 1;
  case SDL_CONTROLLER_BUTTON_DPAD_UP: *outId = RETRO_DEVICE_ID_JOYPAD_UP; return 1;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN: *outId = RETRO_DEVICE_ID_JOYPAD_DOWN; return 1;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: *outId = RETRO_DEVICE_ID_JOYPAD_LEFT; return 1;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: *outId = RETRO_DEVICE_ID_JOYPAD_RIGHT; return 1;
  default:
    break;
  }
  return 0;
}

static void
target_amiga_settingsFloppyChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    int drive = (int)(intptr_t)user;
    amiga_uaeSetFloppyPath(drive, text ? text : "");
    settings_updateSaveLabel();
}

static void
target_amiga_settingsHardDriveFolderChanged(e9ui_context_t *ctx, e9ui_component_t *comp, const char *text, void *user)
{
    (void)ctx;
    (void)comp;
    (void)user;
    amiga_uaeSetHardDriveFolderPath(text ? text : "");
    settings_updateSaveLabel();
}

static void
target_amiga_settingsBuildModal(e9ui_context_t *ctx, target_settings_modal_t *out)
{
    if (!out || !ctx) {
        return;
    }
    out->body = NULL;
    out->footerWarning = NULL;

    const char *romExts[] = { "*.uae" };
    const char *floppyExts[] = { "*.adf", "*.adz", "*.fdi", "*.dms", "*.ipf", "*.raw" };
    const char *elfExts[] = { "*.elf" };

    settings_romselect_state_t *romState = (settings_romselect_state_t *)alloc_calloc(1, sizeof(*romState));
    if (romState) {
        romState->romPath = debugger.settingsEdit.amiga.libretro.romPath;
        romState->romFolder = NULL;
    }

    e9ui_component_t *fsRom = e9ui_fileSelect_make("UAE CONFIG", 120, 600, "...", romExts, 1, E9UI_FILESELECT_FILE);
    if (fsRom) {
        e9ui_fileSelect_enableNewButton(fsRom, "NEW");
        e9ui_fileSelect_setValidate(fsRom, settings_validateUaeConfig, NULL);
        e9ui_fileSelect_setText(fsRom, debugger.settingsEdit.amiga.libretro.romPath);
        e9ui_fileSelect_setOnChange(fsRom, settings_romPathChanged, romState);
    }

    e9ui_component_t *fsDf0 = e9ui_fileSelect_make("DF0", 120, 600, "...", floppyExts, 6, E9UI_FILESELECT_FILE);
    e9ui_component_t *fsDf1 = e9ui_fileSelect_make("DF1", 120, 600, "...", floppyExts, 6, E9UI_FILESELECT_FILE);
    e9ui_component_t *fsHd0 = e9ui_fileSelect_make("HD0 FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);

    if (fsDf0) {
        const char *df0 = amiga_uaeGetFloppyPath(0);
        e9ui_fileSelect_setAllowEmpty(fsDf0, 1);
        e9ui_fileSelect_setText(fsDf0, df0 ? df0 : "");
        e9ui_fileSelect_setOnChange(fsDf0, target_amiga_settingsFloppyChanged, (void *)(intptr_t)0);
    }
    if (fsDf1) {
        const char *df1 = amiga_uaeGetFloppyPath(1);
        e9ui_fileSelect_setAllowEmpty(fsDf1, 1);
        e9ui_fileSelect_setText(fsDf1, df1 ? df1 : "");
        e9ui_fileSelect_setOnChange(fsDf1, target_amiga_settingsFloppyChanged, (void *)(intptr_t)1);
    }
    if (fsHd0) {
        const char *hd0 = amiga_uaeGetHardDriveFolderPath();
        e9ui_fileSelect_setAllowEmpty(fsHd0, 1);
        e9ui_fileSelect_setText(fsHd0, hd0 ? hd0 : "");
        e9ui_fileSelect_setOnChange(fsHd0, target_amiga_settingsHardDriveFolderChanged, NULL);
    }

    e9ui_component_t *fsElf = e9ui_fileSelect_make("EXE", 120, 600, "...", elfExts, 0, E9UI_FILESELECT_FILE);
    if (fsElf) {
        e9ui_fileSelect_setAllowEmpty(fsElf, 1);
        e9ui_fileSelect_setText(fsElf, debugger.settingsEdit.amiga.libretro.exePath);
        e9ui_fileSelect_setOnChange(fsElf, settings_pathChanged, debugger.settingsEdit.amiga.libretro.exePath);
    }

    e9ui_component_t *ltToolchain = e9ui_labeled_textbox_make("TOOLCHAIN PREFIX",
                                                              120,
                                                              600,
                                                              settings_toolchainPrefixChanged,
                                                              debugger.settingsEdit.amiga.libretro.toolchainPrefix);
    if (ltToolchain) {
        e9ui_labeled_textbox_setText(ltToolchain, debugger.settingsEdit.amiga.libretro.toolchainPrefix);
    }

    e9ui_component_t *fsBios = e9ui_fileSelect_make("KICKSTART FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsSaves = e9ui_fileSelect_make("SAVES FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsSource = e9ui_fileSelect_make("SOURCE FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);

    if (fsBios) {
        e9ui_fileSelect_setText(fsBios, debugger.settingsEdit.amiga.libretro.systemDir);
        e9ui_fileSelect_setOnChange(fsBios, settings_pathChanged, debugger.settingsEdit.amiga.libretro.systemDir);
    }
    if (fsSaves) {
        e9ui_fileSelect_setText(fsSaves, debugger.settingsEdit.amiga.libretro.saveDir);
        e9ui_fileSelect_setOnChange(fsSaves, settings_pathChanged, debugger.settingsEdit.amiga.libretro.saveDir);
    }
    if (fsSource) {
        e9ui_fileSelect_setText(fsSource, debugger.settingsEdit.amiga.libretro.sourceDir);
        e9ui_fileSelect_setOnChange(fsSource, settings_pathChanged, debugger.settingsEdit.amiga.libretro.sourceDir);
    }
    e9ui_component_t *ltAudio = e9ui_labeled_textbox_make("AUDIO BUFFER MS",
                                                          120,
                                                          600,
                                                          settings_audioChanged,
                                                          &debugger.settingsEdit.amiga.libretro.audioBufferMs);
    if (ltAudio) {
        char buf[32];
        int audioValue = debugger.settingsEdit.amiga.libretro.audioBufferMs;
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

    target_amiga_romselect_extra_t *extra = (target_amiga_romselect_extra_t *)alloc_calloc(1, sizeof(*extra));
    if (extra) {
        extra->df0Select = fsDf0;
        extra->df1Select = fsDf1;
        extra->hd0Select = fsHd0;
        if (romState) {
            romState->targetUser = extra;
        }
    }

    if (romState) {
        romState->romSelect = fsRom;
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
        if (fsDf0) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsDf0);
            first = 0;
        }
        if (fsDf1) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsDf1);
            first = 0;
        }
        if (fsHd0) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsHd0);
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
            first = 0;
        }
    }
    out->body = body;
    out->footerWarning = settings_uaeExtensionWarning_make();
}

static void
target_amiga_settingsClearOptions(void)
{
    amiga_uaeClearPuaeOptions();
}

static int
target_amiga_memoryGetLimits(uint32_t *outMinAddr, uint32_t *outMaxAddr)
{
    (void)outMinAddr;
    (void)outMaxAddr;
    return 0;
}

static target_iface_t _target_amiga = {
    .name = "AMIGA",
    .dasm = &dasm_ami_iface,
    .emu = &emu_ami_iface,
    .setConfigDefaults = target_amiga_setConfigDefaults,
    .setActiveDefaultsFromCurrentSystem = target_amiga_setActiveDefaultsFromCurrentSystem,
    .applyActiveSettingsToCurrentSystem = target_amiga_applyActiveSettingsToCurrentSystem,
    .configIsOk = target_amiga_configIsOk,
    .needsRestart = target_amiga_needsRestart,
    .settingsSaveButtonDisabled = target_amiga_settingsSaveButtonDisabled,
    .validateSettings = target_amiga_validateSettings,
    .settingsDefaults = target_amiga_settingsDefault,
    .applyRomConfigForSelection = target_amiga_applyRomConfigForSelection,
    .settingsSetConfigPaths = target_amiga_settingsSetConfigPaths,
    .defaultCorePath = target_amiga_defaultCorePath,
    .settingsRomPathChanged = target_amiga_settingsRomPathChanged,
    .settingsRomFolderChanged = target_amiga_settingsFolderChanged,
    .settingsCoreChanged = target_amiga_settingsCoreChanged,
    .settingsClearOptions = target_amiga_settingsClearOptions,
    .settingsLoadOptions = target_amiga_settingsLoadOptions,
    .settingsBuildModal = target_amiga_settingsBuildModal,
    .selectLibretroConfig = target_amiga_selectLibretroConfig,
    .coreOptionsHasGeneral = target_amiga_coreOptionsHasGeneral,
    .coreOptionsSaveClicked = target_amiga_coreOptionsSaveClicked,
    .coreOptionGetValue = target_amiga_coreOptionGetValue,
    .getLibretroCliConfig = target_amiga_getLibretroCliConfig,
    .onVblank = target_amiga_onVblank,
    .coreIndex = TARGET_AMIGA,
    .libretroSelectConfig = target_amiga_libretroSelectConfig,
    .pickElfToolchainPaths = target_amiga_pickElfToolchainPaths,
    .applyCoreOptions = target_amiga_applyCoreOptions,
    .validateAPI = target_amiga_validateAPI,
    .audioEnabled = target_amiga_audioEnabled,
    .audioEnable = target_amiga_audioEnable,
    .mousePort = LIBRETRO_HOST_MAX_PORTS,
    .memoryGetLimits = target_amiga_memoryGetLimits,
    .getBadgeTexture = target_amiga_getBadgeTexture,
    .configControllerPorts = target_amiga_configControllerPorts,
    .controllerMapButton = target_amiga_controllerMapButton,
  };

target_iface_t *target_amiga(void) { return &_target_amiga; }  
