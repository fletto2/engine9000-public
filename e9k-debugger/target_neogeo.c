#include "debugger.h"
#include "target.h"
#include "emu_geo.h"
#include "rom_config.h"
#include "amiga_uae_options.h"
#include "neogeo_core_options.h"
#include "debugger_platform.h"
#include "core_options.h"
#include "libretro_host.h"
#include "romset.h"
#include "system_badge.h"
#include "alloc.h"

typedef struct target_neogeo_systemtype_state {
    e9ui_component_t *aesCheckbox;
    e9ui_component_t *mvsCheckbox;
    char *systemType;
    int updating;
} target_neogeo_systemtype_state_t;


static void
target_neogeo_settingsClearOptions(void)
{
    neogeo_coreOptionsClear();
}

static void
target_neogeo_systemTypeSync(target_neogeo_systemtype_state_t *st, const char *value, e9ui_context_t *ctx)
{
    if (!st || !st->systemType) {
        return;
    }
    st->updating = 1;
    settings_config_setValue(st->systemType, 16, value);
    int aesSelected = (strcmp(st->systemType, "aes") == 0);
    int mvsSelected = (strcmp(st->systemType, "mvs") == 0);
    if (st->aesCheckbox) {
        e9ui_checkbox_setSelected(st->aesCheckbox, aesSelected, ctx);
    }
    if (st->mvsCheckbox) {
        e9ui_checkbox_setSelected(st->mvsCheckbox, mvsSelected, ctx);
    }
    st->updating = 0;
    settings_updateSaveLabel();
}

static void
target_neogeo_systemTypeAesChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    target_neogeo_systemtype_state_t *st = (target_neogeo_systemtype_state_t *)user;
    if (!st || st->updating) {
        return;
    }
    if (selected) {
        target_neogeo_systemTypeSync(st, "aes", ctx);
    } else if (st->mvsCheckbox && e9ui_checkbox_isSelected(st->mvsCheckbox)) {
        target_neogeo_systemTypeSync(st, "mvs", ctx);
    } else {
        target_neogeo_systemTypeSync(st, "", ctx);
    }
}

static void
target_neogeo_systemTypeMvsChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    target_neogeo_systemtype_state_t *st = (target_neogeo_systemtype_state_t *)user;
    if (!st || st->updating) {
        return;
    }
    if (selected) {
        target_neogeo_systemTypeSync(st, "mvs", ctx);
    } else if (st->aesCheckbox && e9ui_checkbox_isSelected(st->aesCheckbox)) {
        target_neogeo_systemTypeSync(st, "aes", ctx);
    } else {
        target_neogeo_systemTypeSync(st, "", ctx);
    }
}

static void
target_neogeo_skipBiosChanged(e9ui_component_t *self, e9ui_context_t *ctx, int selected, void *user)
{
    (void)self;
    (void)ctx;
    int *dest = (int*)user;
    if (!dest) {
        return;
    }
    *dest = selected ? 1 : 0;
    settings_updateSaveLabel();
}


static void
target_neogeo_settingsBuildModal(e9ui_context_t *ctx, target_settings_modal_t *out)
{
    if (!out || !ctx) {
        return;
    }
    out->body = NULL;
    out->footerWarning = NULL;

    const char *romExts[] = { "*.neo" };
    const char *elfExts[] = { "*.elf" };

    settings_romselect_state_t *romState = (settings_romselect_state_t *)alloc_calloc(1, sizeof(*romState));
    if (romState) {
        romState->romPath = debugger.settingsEdit.neogeo.libretro.romPath;
        romState->romFolder = debugger.settingsEdit.neogeo.romFolder;
        romState->corePath = debugger.settingsEdit.neogeo.libretro.corePath;
    }

    e9ui_component_t *fsRom = e9ui_fileSelect_make("ROM", 120, 600, "...", romExts, 1, E9UI_FILESELECT_FILE);
    e9ui_component_t *fsRomFolder = e9ui_fileSelect_make("ROM FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsElf = e9ui_fileSelect_make("ELF", 120, 600, "...", elfExts, 1, E9UI_FILESELECT_FILE);
    e9ui_component_t *fsBios = e9ui_fileSelect_make("BIOS FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsSaves = e9ui_fileSelect_make("SAVES FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsSource = e9ui_fileSelect_make("SOURCE FOLDER", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FOLDER);
    e9ui_component_t *fsCore = e9ui_fileSelect_make("CORE", 120, 600, "...", NULL, 0, E9UI_FILESELECT_FILE);

    if (fsRom) {
        e9ui_fileSelect_setText(fsRom, debugger.settingsEdit.neogeo.libretro.romPath);
        e9ui_fileSelect_setOnChange(fsRom, settings_romPathChanged, romState);
    }
    if (fsRomFolder) {
        e9ui_fileSelect_setText(fsRomFolder, debugger.settingsEdit.neogeo.romFolder);
        e9ui_fileSelect_setOnChange(fsRomFolder, settings_romFolderChanged, romState);
    }
    if (fsElf) {
        e9ui_fileSelect_setAllowEmpty(fsElf, 1);
        e9ui_fileSelect_setText(fsElf, debugger.settingsEdit.neogeo.libretro.exePath);
        e9ui_fileSelect_setOnChange(fsElf, settings_pathChanged, debugger.settingsEdit.neogeo.libretro.exePath);
    }
    if (fsBios) {
        e9ui_fileSelect_setText(fsBios, debugger.settingsEdit.neogeo.libretro.systemDir);
        e9ui_fileSelect_setOnChange(fsBios, settings_pathChanged, debugger.settingsEdit.neogeo.libretro.systemDir);
    }
    if (fsSaves) {
        e9ui_fileSelect_setText(fsSaves, debugger.settingsEdit.neogeo.libretro.saveDir);
        e9ui_fileSelect_setOnChange(fsSaves, settings_pathChanged, debugger.settingsEdit.neogeo.libretro.saveDir);
    }
    if (fsSource) {
        e9ui_fileSelect_setText(fsSource, debugger.settingsEdit.neogeo.libretro.sourceDir);
        e9ui_fileSelect_setOnChange(fsSource, settings_pathChanged, debugger.settingsEdit.neogeo.libretro.sourceDir);
    }
    if (fsCore) {
        e9ui_fileSelect_setText(fsCore, debugger.settingsEdit.neogeo.libretro.corePath);
        e9ui_fileSelect_setOnChange(fsCore, settings_pathChanged, debugger.settingsEdit.neogeo.libretro.corePath);
    }

    e9ui_component_t *ltToolchain = e9ui_labeled_textbox_make("TOOLCHAIN PREFIX",
                                                              120,
                                                              600,
                                                              settings_toolchainPrefixChanged,
                                                              debugger.settingsEdit.neogeo.libretro.toolchainPrefix);
    if (ltToolchain) {
        e9ui_labeled_textbox_setText(ltToolchain, debugger.settingsEdit.neogeo.libretro.toolchainPrefix);
    }

    e9ui_component_t *ltAudio = e9ui_labeled_textbox_make("AUDIO BUFFER MS",
                                                          120,
                                                          600,
                                                          settings_audioChanged,
                                                          &debugger.settingsEdit.neogeo.libretro.audioBufferMs);
    if (ltAudio) {
        char buf[32];
        int audioValue = debugger.settingsEdit.neogeo.libretro.audioBufferMs;
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
        romState->folderSelect = fsRomFolder;
        romState->coreSelect = fsCore;
        romState->elfSelect = fsElf;
        romState->sourceSelect = fsSource;
        romState->toolchainSelect = ltToolchain;
        settings_romSelectUpdateAllowEmpty(romState);
    }

    // System row: [MVS] [AES] [SKIP BIOS LOGO]
    e9ui_component_t *rowSystem = e9ui_flow_make();
    e9ui_component_t *rowSystemCenter = NULL;
    if (rowSystem) {
        e9ui_flow_setWrap(rowSystem, 1);
        e9ui_flow_setSpacing(rowSystem, 12);
        target_neogeo_systemtype_state_t *sys = (target_neogeo_systemtype_state_t *)alloc_calloc(1, sizeof(*sys));
        if (sys) {
            sys->systemType = debugger.settingsEdit.neogeo.systemType;
        }
        int aesSelected = (strcmp(debugger.settingsEdit.neogeo.systemType, "aes") == 0);
        int mvsSelected = (strcmp(debugger.settingsEdit.neogeo.systemType, "mvs") == 0);
        e9ui_component_t *cbMvs = e9ui_checkbox_make("MVS", mvsSelected, target_neogeo_systemTypeMvsChanged, sys);
        e9ui_component_t *cbAes = e9ui_checkbox_make("AES", aesSelected, target_neogeo_systemTypeAesChanged, sys);
        e9ui_component_t *cbSkip = e9ui_checkbox_make("SKIP BIOS LOGO",
                                                      debugger.settingsEdit.neogeo.skipBiosLogo,
                                                      target_neogeo_skipBiosChanged,
                                                      &debugger.settingsEdit.neogeo.skipBiosLogo);
        if (sys) {
            sys->aesCheckbox = cbAes;
            sys->mvsCheckbox = cbMvs;
        }
        if (cbMvs) {
            e9ui_flow_add(rowSystem, cbMvs);
        }
        if (cbAes) {
            e9ui_flow_add(rowSystem, cbAes);
        }
        if (cbSkip) {
            e9ui_flow_add(rowSystem, cbSkip);
        }
        rowSystemCenter = e9ui_center_make(rowSystem);
        if (rowSystemCenter) {
            int rowW = 0;
            int rowH = 0;
            e9ui_flow_measure(rowSystem, ctx, &rowW, &rowH);
            (void)rowH;
            if (rowW > 0) {
                e9ui_center_setSize(rowSystemCenter, e9ui_unscale_px(ctx, rowW), 0);
            }
        }
    }

    e9ui_component_t *body = e9ui_stack_makeVertical();
    if (body) {
        int first = 1;
        if (fsRom) {
            e9ui_stack_addFixed(body, fsRom);
            first = 0;
        }
        if (fsRomFolder) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, fsRomFolder);
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
        if (rowSystemCenter) {
            if (!first) {
                e9ui_stack_addFixed(body, e9ui_vspacer_make(12));
            }
            e9ui_stack_addFixed(body, rowSystemCenter);
            first = 0;
        }
    }
    out->body = body;
}

static void
target_neogeo_applyActiveSettingsToCurrentSystem(void)
{
    strncpy(debugger.config.neogeo.libretro.exePath, rom_config_activeElfPath, sizeof(debugger.config.neogeo.libretro.exePath) - 1);
    strncpy(debugger.config.neogeo.libretro.sourceDir, rom_config_activeSourceDir, sizeof(debugger.config.neogeo.libretro.sourceDir) - 1);
    strncpy(debugger.config.neogeo.libretro.toolchainPrefix, rom_config_activeToolchainPrefix, sizeof(debugger.config.neogeo.libretro.toolchainPrefix) - 1);
    debugger.config.neogeo.libretro.exePath[sizeof(debugger.config.neogeo.libretro.exePath) - 1] = '\0';
    debugger.config.neogeo.libretro.sourceDir[sizeof(debugger.config.neogeo.libretro.sourceDir) - 1] = '\0';
    debugger.config.neogeo.libretro.toolchainPrefix[sizeof(debugger.config.neogeo.libretro.toolchainPrefix) - 1] = '\0';
}


static void
target_neogeo_setActiveDefaultsFromCurrentSystem(void)
{
  strncpy(rom_config_activeElfPath, debugger.config.neogeo.libretro.exePath, sizeof(rom_config_activeElfPath) - 1);
  strncpy(rom_config_activeSourceDir, debugger.config.neogeo.libretro.sourceDir, sizeof(rom_config_activeSourceDir) - 1);
  strncpy(rom_config_activeToolchainPrefix, debugger.config.neogeo.libretro.toolchainPrefix, sizeof(rom_config_activeToolchainPrefix) - 1);
}




static int
target_neogeo_configMissingPaths(const e9k_neogeo_config_t *cfg)
{
    if (!cfg) {
        return 1;
    }
    if (!cfg->libretro.corePath[0] ||
        (!cfg->libretro.romPath[0] && !cfg->romFolder[0]) ||
        !cfg->libretro.systemDir[0] ||
        !cfg->libretro.saveDir[0] ||
        !settings_pathExistsFile(cfg->libretro.corePath) ||
        !settings_pathExistsDir(cfg->libretro.systemDir) ||
        !settings_pathExistsDir(cfg->libretro.saveDir)) {
        return 1;
    }
    if (cfg->libretro.romPath[0] && !settings_pathExistsFile(cfg->libretro.romPath)) {
        return 1;
    }
    if (cfg->romFolder[0] && !settings_pathExistsDir(cfg->romFolder)) {
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
target_neogeo_configIsOk(void)
{
  return target_neogeo_configMissingPaths(&debugger.config.neogeo) ? 0 : 1;
}


static int
target_negeo_restartNeededForNeogeo(const e9k_neogeo_config_t *before, const e9k_neogeo_config_t *after)
{
    if (!before || !after) {
        return 1;
    }
    int romChanged = strcmp(before->libretro.romPath, after->libretro.romPath) != 0 ||
                     strcmp(before->romFolder, after->romFolder) != 0;
    int elfChanged = strcmp(before->libretro.exePath, after->libretro.exePath) != 0;
    int toolchainChanged = strcmp(before->libretro.toolchainPrefix, after->libretro.toolchainPrefix) != 0;
    int biosChanged = strcmp(before->libretro.systemDir, after->libretro.systemDir) != 0;
    int savesChanged = strcmp(before->libretro.saveDir, after->libretro.saveDir) != 0;
    int sourceChanged = strcmp(before->libretro.sourceDir, after->libretro.sourceDir) != 0;
    int coreChanged = strcmp(before->libretro.corePath, after->libretro.corePath) != 0;
    int sysChanged = strcmp(before->systemType, after->systemType) != 0;
    int audioBefore = settings_audioBufferNormalized(before->libretro.audioBufferMs);
    int audioAfter = settings_audioBufferNormalized(after->libretro.audioBufferMs);
    int audioChanged = audioBefore != audioAfter;
    return romChanged || elfChanged || toolchainChanged || biosChanged || savesChanged || sourceChanged || coreChanged || sysChanged || audioChanged;
}


static int
target_neogeo_settings_configIsOkFor(const e9k_neogeo_config_t *cfg)
{
    return target_neogeo_configMissingPaths(cfg) ? 0 : 1;
}


static int
target_neogeo_needsRestart(void)
{
    int configChanged = target_negeo_restartNeededForNeogeo(&debugger.config.neogeo, &debugger.settingsEdit.neogeo);
    if (settings_coreOptionsDirty) {
      configChanged = 1;
    }
    int okBefore = target_neogeo_settings_configIsOkFor(&debugger.config.neogeo);
    int okAfter = target_neogeo_settings_configIsOkFor(&debugger.settingsEdit.neogeo);

    return configChanged ||  (!okBefore && okAfter);    
}


static int
target_neogeo_settingsSaveButtonDisabled(void)
{
  return 0;
}


static int
target_neogeo_effectiveRomPath(const e9k_neogeo_config_t *cfg, char *out, size_t cap)
{
    if (!out || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!cfg) {
        return 0;
    }
    if (cfg->libretro.romPath[0]) {
        snprintf(out, cap, "%s", cfg->libretro.romPath);
        out[cap - 1] = '\0';
        return 1;
    }
    if (!cfg->romFolder[0]) {
        return 0;
    }
    const char *base = cfg->libretro.saveDir[0] ? cfg->libretro.saveDir : cfg->libretro.systemDir;
    if (!base || !*base) {
        return 0;
    }
    char sep = '/';
    if (strchr(base, '\\')) {
        sep = '\\';
    }
    int needsSep = 1;
    size_t len = strlen(base);
    if (len > 0 && (base[len - 1] == '/' || base[len - 1] == '\\')) {
        needsSep = 0;
    }
    int written = 0;
    if (needsSep) {
        written = snprintf(out, cap, "%s%c%s", base, sep, "e9k-romfolder.neo");
    } else {
        written = snprintf(out, cap, "%s%s", base, "e9k-romfolder.neo");
    }
    if (written < 0 || (size_t)written >= cap) {
        out[cap - 1] = '\0';
        return 0;
    }
    return 1;
}


static void
target_neogeo_validateSettings(void)
{
  if (debugger.settingsEdit.neogeo.libretro.audioBufferMs <= 0) {
    debugger.settingsEdit.neogeo.libretro.audioBufferMs = 50;
  }
  char romPath[PATH_MAX];
  if (target_neogeo_effectiveRomPath(&debugger.settingsEdit.neogeo, romPath, sizeof(romPath))) {
    if (!neogeo_coreOptionsWriteToFile(debugger.settingsEdit.neogeo.libretro.saveDir, romPath)) {
      e9ui_showTransientMessage("CORE OPTIONS SAVE FAILED");
      return;
    }
  }
  neogeo_coreOptionsClear();
  const char *saveDir = debugger.settingsEdit.neogeo.libretro.saveDir[0] ?
    debugger.settingsEdit.neogeo.libretro.saveDir : debugger.settingsEdit.neogeo.libretro.systemDir;

  if (target_neogeo_effectiveRomPath(&debugger.settingsEdit.neogeo, romPath, sizeof(romPath))) {
    rom_config_saveSettingsForRom(saveDir, romPath,
				  debugger.settingsEdit.neogeo.libretro.exePath,
				  debugger.settingsEdit.neogeo.libretro.sourceDir,
				  debugger.settingsEdit.neogeo.libretro.toolchainPrefix);
  }
  
}


static void
target_neogeo_settingsDefault(void)
{
    char romPath[PATH_MAX];
    char romFolder[PATH_MAX];
    char elfPath[PATH_MAX];
    settings_copyPath(romPath, sizeof(romPath), debugger.settingsEdit.neogeo.libretro.romPath);
    settings_copyPath(romFolder, sizeof(romFolder), debugger.settingsEdit.neogeo.romFolder);
    settings_copyPath(elfPath, sizeof(elfPath), debugger.settingsEdit.neogeo.libretro.exePath);
    int audioEnabled = debugger.settingsEdit.neogeo.libretro.audioEnabled;
    debugger_platform_setDefaults(&debugger.settingsEdit.neogeo);
    debugger.settingsEdit.neogeo.libretro.audioEnabled = audioEnabled;
    settings_copyPath(debugger.settingsEdit.neogeo.libretro.romPath, sizeof(debugger.settingsEdit.neogeo.libretro.romPath), romPath);
    settings_copyPath(debugger.settingsEdit.neogeo.romFolder, sizeof(debugger.settingsEdit.neogeo.romFolder), romFolder);
    settings_copyPath(debugger.settingsEdit.neogeo.libretro.exePath, sizeof(debugger.settingsEdit.neogeo.libretro.exePath), elfPath);
}


static void
target_neogeo_applyRomConfigForSelection(settings_romselect_state_t *st, const char** saveDirP, const char** romPathP)
{
    (void)st;
    *saveDirP = debugger.settingsEdit.neogeo.libretro.saveDir[0] ?
    debugger.settingsEdit.neogeo.libretro.saveDir : debugger.settingsEdit.neogeo.libretro.systemDir;
    static char romPathBuf[PATH_MAX];
    if (target_neogeo_effectiveRomPath(&debugger.settingsEdit.neogeo, romPathBuf, sizeof(romPathBuf))) {
        *romPathP = romPathBuf;
    }
}


static void
target_neoge_settingsSetConfigPaths(int hasElf, const char* elfPath, int hasSource, const char* sourceDir, int hasToolchain, const char*toolchainPrefix)
{
  settings_config_setPath(debugger.settingsEdit.neogeo.libretro.exePath, PATH_MAX, hasElf ? elfPath : "");
  settings_config_setPath(debugger.settingsEdit.neogeo.libretro.sourceDir, PATH_MAX, hasSource ? sourceDir : "");
  settings_config_setValue(debugger.settingsEdit.neogeo.libretro.toolchainPrefix, PATH_MAX, hasToolchain ? toolchainPrefix : "");
}


static const char *
target_neogeo_defaultCorePath(void)
{
  return "./system/geolith_libretro.dylib";
}


static void
target_neogeo_settingsRomPathChanged(settings_romselect_state_t* st)
{
  (void)st;
  char romPath[PATH_MAX];
  if (target_neogeo_effectiveRomPath(&debugger.settingsEdit.neogeo, romPath, sizeof(romPath))) {
    neogeo_coreOptionsLoadFromFile(debugger.settingsEdit.neogeo.libretro.saveDir, romPath);
  } else {
    neogeo_coreOptionsClear();
  }
}


static void
target_neogeo_settingsFolderChanged(void)
{
    char romPath[PATH_MAX];
    if (target_neogeo_effectiveRomPath(&debugger.settingsEdit.neogeo, romPath, sizeof(romPath))) {
        neogeo_coreOptionsLoadFromFile(debugger.settingsEdit.neogeo.libretro.saveDir, romPath);
    } else {
        neogeo_coreOptionsClear();
    }
}


static void
target_neogeo_settingsCoreChanged(void)
{
    const char *defaultCore = target_neogeo_defaultCorePath();
    if (defaultCore &&
        defaultCore[0] &&
        (!debugger.settingsEdit.neogeo.libretro.corePath[0] ||
         target_isDefaultCorePath(debugger.settingsEdit.neogeo.libretro.corePath))) {
        settings_config_setPath(debugger.settingsEdit.neogeo.libretro.corePath, PATH_MAX, defaultCore);
    }
    //TODO ??
    amiga_uaeClearPuaeOptions();
    char romPath[PATH_MAX];
    if (target_neogeo_effectiveRomPath(&debugger.settingsEdit.neogeo, romPath, sizeof(romPath))) {
        neogeo_coreOptionsLoadFromFile(debugger.settingsEdit.neogeo.libretro.saveDir, romPath);
    } else {
        neogeo_coreOptionsClear();
    }
}


static void
target_neogeo_settingsLoadOptions(e9k_system_config_t * st)
{
  (void)st;
  char romPath[PATH_MAX];
  if (target_neogeo_effectiveRomPath(&debugger.settingsEdit.neogeo, romPath, sizeof(romPath))) {
    neogeo_coreOptionsLoadFromFile(debugger.settingsEdit.neogeo.libretro.saveDir, romPath);
  }
}


const e9k_libretro_config_t*
target_neogeo_selectLibretroConfig(const e9k_system_config_t *cfg)
{
  return &cfg->neogeo.libretro;
}


static int
target_neogeo_coreOptionsHasGeneral(const core_options_modal_state_t *st)
{
  (void)st;
  return 1;
}


static void
target_neogeo_coreOptionsSaveClicked(e9ui_context_t *ctx,core_options_modal_state_t *st)
{
  (void)ctx;
  (void)st;
  int anyChange = 0;
  
  for (size_t i = 0; i < st->entryCount; ++i) {
    const char *key = st->entries[i].key;
    const char *value = st->entries[i].value;
    if (!key || !*key) {
      continue;
    }
    if (strcmp(key, "geolith_system_type") == 0) {
      continue;
    }
    const char *defValue = core_options_findDefaultValue(st, key);
    const char *desired = NULL;
    if (!defValue || !value || strcmp(defValue, value) != 0) {
      desired = value ? value : "";
    }
    const char *existing = neogeo_coreOptionsGetValue(key);
    if (!desired) {
      if (!existing) {
	continue;
      }
      neogeo_coreOptionsSetValue(key, NULL);
      anyChange = 1;
    } else {
      if (existing && core_options_stringsEqual(existing, desired)) {
	continue;
      }
      neogeo_coreOptionsSetValue(key, desired);
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
target_neogeo_coreOptionGetValue(const char* key)
{
  return  neogeo_coreOptionsGetValue(key);
}


static e9k_libretro_config_t *
target_neogeo_getLibretroCliConfig(void)
{
  return &debugger.cliConfig.neogeo.libretro;
}


static void
target_neogeo_onVblank(void)
{
  e9k_debug_sprite_state_t spriteState;
  if (libretro_host_debugGetSpriteState(&spriteState) && spriteState.vram && spriteState.vram_words) {
    size_t byteCount = spriteState.vram_words * sizeof(uint16_t);
    if (!debugger.spriteShadowVram || debugger.spriteShadowWords != spriteState.vram_words) {
      void *nextBuffer = realloc(debugger.spriteShadowVram, byteCount);
      if (nextBuffer) {
	debugger.spriteShadowVram = (uint16_t *)nextBuffer;
	debugger.spriteShadowWords = spriteState.vram_words;
      }
    }
    if (debugger.spriteShadowVram && debugger.spriteShadowWords == spriteState.vram_words) {
      memcpy(debugger.spriteShadowVram, spriteState.vram, byteCount);
      debugger.spriteShadow = spriteState;
      debugger.spriteShadow.vram = debugger.spriteShadowVram;
      debugger.spriteShadow.vram_words = debugger.spriteShadowWords;
      debugger.spriteShadowReady = 1;
    }
  }
}



static void
target_neogeo_libretroSelectConfig(void)
{
    debugger.libretro.audioBufferMs = debugger.config.neogeo.libretro.audioBufferMs;
    debugger.libretro.audioEnabled = debugger.config.neogeo.libretro.audioEnabled;
    debugger_copyPath(debugger.libretro.sourceDir, sizeof(debugger.libretro.sourceDir), debugger.config.neogeo.libretro.sourceDir);
    debugger_copyPath(debugger.libretro.exePath, sizeof(debugger.libretro.exePath), debugger.config.neogeo.libretro.exePath);
    debugger_copyPath(debugger.libretro.toolchainPrefix, sizeof(debugger.libretro.toolchainPrefix), debugger.config.neogeo.libretro.toolchainPrefix);
    debugger_copyPath(debugger.libretro.corePath, sizeof(debugger.libretro.corePath), debugger.config.neogeo.libretro.corePath);
    debugger_copyPath(debugger.libretro.romPath, sizeof(debugger.libretro.romPath), debugger.config.neogeo.libretro.romPath);
    debugger_copyPath(debugger.libretro.systemDir, sizeof(debugger.libretro.systemDir), debugger.config.neogeo.libretro.systemDir);
    debugger_copyPath(debugger.libretro.saveDir, sizeof(debugger.libretro.saveDir), debugger.config.neogeo.libretro.saveDir);
    if (debugger.config.neogeo.romFolder[0]) {
        char neo_path[PATH_MAX];
        if (romset_buildNeoFromFolder(debugger.config.neogeo.romFolder, neo_path, sizeof(neo_path))) {
            debugger_copyPath(debugger.libretro.romPath, sizeof(debugger.libretro.romPath), neo_path);
        } else {
            debugger.libretro.romPath[0] = '\0';
        }
    }
}


static void
target_neogeo_pickElfToolchainPaths(const char** rawElf, const char** toolchainPrefix)
{
  *rawElf = debugger.config.neogeo.libretro.exePath;
  *toolchainPrefix = debugger.config.neogeo.libretro.toolchainPrefix;
}



static void
target_neogeo_applyCoreOptions(void)
{
  const char *romPath = debugger.libretro.romPath[0] ? debugger.libretro.romPath : debugger.config.neogeo.libretro.romPath;
  const char *saveDir = debugger.libretro.saveDir[0] ? debugger.libretro.saveDir : debugger.config.neogeo.libretro.saveDir;
  if (romPath && *romPath && saveDir && *saveDir) {
    neogeo_coreOptionsApplyFileToHost(saveDir, romPath);
  }
}


static void
target_neogeo_validateAPI(void)
{
  
}


static int
target_neogeo_audioEnabled(void)
{
  return debugger.config.neogeo.libretro.audioEnabled;
}


static void
target_neogeo_audioEnable(int enabled)
{
  debugger.config.neogeo.libretro.audioEnabled = enabled;
}



static SDL_Texture *
target_neogeo_getBadgeTexture(SDL_Renderer *renderer, target_iface_t* t, int* outW, int* outH)
{
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
target_neogeo_configControllerPorts(void)
{  

}


static  int
target_neogeo_controllerMapButton(SDL_GameControllerButton button, unsigned *outId)
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

static target_iface_t _target_neogeo = {
    .name = "NEO GEO",
    .dasm = &dasm_geo_iface,
    .emu = &emu_geo_iface,
    .setActiveDefaultsFromCurrentSystem = target_neogeo_setActiveDefaultsFromCurrentSystem,
    .applyActiveSettingsToCurrentSystem = target_neogeo_applyActiveSettingsToCurrentSystem,
    .configIsOk = target_neogeo_configIsOk,
    .needsRestart = target_neogeo_needsRestart,
    .settingsSaveButtonDisabled = target_neogeo_settingsSaveButtonDisabled,
    .validateSettings = target_neogeo_validateSettings,
    .settingsDefaults = target_neogeo_settingsDefault,
    .applyRomConfigForSelection = target_neogeo_applyRomConfigForSelection,
    .settingsSetConfigPaths = target_neoge_settingsSetConfigPaths,
    .defaultCorePath = target_neogeo_defaultCorePath,
    .settingsRomPathChanged = target_neogeo_settingsRomPathChanged,
    .settingsRomFolderChanged = target_neogeo_settingsFolderChanged,
    .settingsCoreChanged = target_neogeo_settingsCoreChanged,
    .settingsClearOptions = target_neogeo_settingsClearOptions,
    .settingsLoadOptions = target_neogeo_settingsLoadOptions,
    .settingsBuildModal = target_neogeo_settingsBuildModal,
    .selectLibretroConfig = target_neogeo_selectLibretroConfig,
    .coreOptionsHasGeneral = target_neogeo_coreOptionsHasGeneral,
    .coreOptionsSaveClicked = target_neogeo_coreOptionsSaveClicked,
    .coreOptionGetValue = target_neogeo_coreOptionGetValue,
    .getLibretroCliConfig = target_neogeo_getLibretroCliConfig,
    .onVblank = target_neogeo_onVblank,
    .coreIndex = TARGET_NEOGEO,
    .libretroSelectConfig = target_neogeo_libretroSelectConfig,
    .pickElfToolchainPaths = target_neogeo_pickElfToolchainPaths,
    .applyCoreOptions = target_neogeo_applyCoreOptions,
    .validateAPI = target_neogeo_validateAPI,
    .audioEnabled = target_neogeo_audioEnabled,
    .audioEnable = target_neogeo_audioEnable,
    .mousePort = -1,
    .getBadgeTexture = target_neogeo_getBadgeTexture,
    .configControllerPorts = target_neogeo_configControllerPorts,
    .controllerMapButton = target_neogeo_controllerMapButton,

};

target_iface_t*
target_neogeo(void)
{
  return &_target_neogeo;
}
  
