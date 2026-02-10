CC ?= cc
PKG_CONFIG ?= pkg-config
MINGW_HOME ?= /usr/local/mingw
E9K_ENABLE_MEGADRIVE ?= 1
E9K_ENABLE_GL_COMPOSITE ?= 1

#INCLUDES += -I../geo9000/libretro
#INCLUDES += -I../geo9000/src
INCLUDES += -I../e9k-lib
INCLUDES += -I./e9ui
INCLUDES += -I./lib9000
INCLUDES += -I.

UNAME_S := $(shell uname -s)

DEBUG_FLAGS=-g -fno-omit-frame-pointer
SANITIZER_LIBS=
ifeq ($(UNAME_S),Darwin)
DEBUG_FLAGS += -fsanitize=address,undefined
SANITIZER_LIBS += -fsanitize=address,undefined
endif
CFLAGS += $(DEBUG_FLAGS) -Wall -Wextra -std=c99 -MMD -MP -Werror -pthread
CFLAGS += -O2 -flto
ifeq ($(UNAME_S),Linux)
CFLAGS += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
endif
CFLAGS += -DE9K_ENABLE_MEGADRIVE=$(E9K_ENABLE_MEGADRIVE)
CFLAGS += -DE9K_ENABLE_GL_COMPOSITE=$(E9K_ENABLE_GL_COMPOSITE)
CFLAGS += $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_image SDL2_ttf libpng)
CFLAGS += $(INCLUDES)

LIBS   += $(shell $(PKG_CONFIG) --libs sdl2 SDL2_image SDL2_ttf libpng)
LIBS   += -ldl
LIBS   += $(SANITIZER_LIBS)
LIBS   += -lreadline
LIBS   += -pthread
LIBS   += -lm

ifeq ($(UNAME_S),Darwin)
LIBS   += -framework Cocoa
ifeq ($(E9K_ENABLE_GL_COMPOSITE),1)
LIBS   += -framework OpenGL
endif
else
ifeq ($(E9K_ENABLE_GL_COMPOSITE),1)
LIBS   += -lGL
endif
endif

TARGET = e9k-debugger
BUILD_DIR = build
W64_CC ?= x86_64-w64-mingw32-gcc
W64_TARGET = dist/e9kd/e9k-debugger.exe
W64_BUILD_DIR = build-w64
W64_CFLAGS = -flto -O2 -Wall -Wextra -std=c99 -MMD -MP
W64_CFLAGS += $(INCLUDES)
W64_CFLAGS += -I$(MINGW_HOME)/include/SDL2 -I$(MINGW_HOME)/include -I$(MINGW_HOME)/include/readline -D_THREAD_SAFE
W64_CFLAGS += -DE9K_ENABLE_MEGADRIVE=$(E9K_ENABLE_MEGADRIVE)
W64_CFLAGS += -DE9K_ENABLE_GL_COMPOSITE=$(E9K_ENABLE_GL_COMPOSITE)
W64_CFLAGS += $(W64_EXTRA_CFLAGS)
W64_LIBS =
W64_LIBS += -L$(MINGW_HOME)/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lreadline -lole32 -lcomdlg32 -lpng -lz -lopengl32
W64_LIBS += $(W64_EXTRA_LIBS)
W64_LDFLAGS = -s -flto -mconsole

BUNDLE_NAME = Engine9000.app
BUNDLE_DIR = $(BUNDLE_NAME)/Contents
BUNDLE_MACOS = $(BUNDLE_DIR)/MacOS
BUNDLE_RESOURCES = $(BUNDLE_DIR)/Resources

LIB9000_SRCS=alloc.c list.c resource.c file.c

E9UI_SRCS=e9ui_textbox.c e9ui_text_cache.c e9ui_text_select.c e9ui_text.c e9ui_scroll.c e9ui.c e9ui_split.c e9ui_split_stack.c e9ui_stack.c\
	  e9ui_image.c e9ui_box.c e9ui_hstack.c e9ui_button.c e9ui_checkbox.c e9ui_flow.c e9ui_header_flow.c e9ui_spacer.c e9ui_vspacer.c e9ui_separator.c\
          e9ui_badge.c e9ui_overlay.c e9ui_center.c e9ui_fileselect.c e9ui_labeled_textbox.c e9ui_labeled_checkbox.c e9ui_labeled_select.c e9ui_event.c \
	  e9ui_theme.c e9ui_link.c e9ui_modal.c e9ui_child.c 

PLATFORM_POSIX_SRCS = platform/posix/addr2line.c platform/posix/analyse.c platform/posix/profile_view.c platform/posix/signal.c platform/posix/debugger_platform.c
PLATFORM_LINUX_SRCS = platform/posix/clipboard.c
PLATFORM_W64_SRCS = platform/w64/addr2line.c platform/w64/analyse.c platform/w64/profile_view.c platform/w64/signal.c platform/w64/debugger_platform.c platform/w64/clipboard.c
PLATFORM_MACOS_SRCS = platform/macos/clipboard.m platform/macos/tinyfiledialogs.m

SRCS = main.c source_pane.c source.c dasm.c dasm_geo.c dasm_ami.c elfutil.c console_cmd.c print_eval.c print_debuginfo_readelf.c print_debuginfo_readelf_frames.c print_debuginfo_objdump_stabs.c prompt.c console.c registers.c memory.c stack.c breakpoints.c trainer.c linebuf.c debug_font.c debug.c machine.c profile.c profile_list.c profile_hotspot.c profile_checkpoints.c input_record.c hotkeys.c smoke_test.c ui_test.c snapshot.c rom_config.c romset.c settings.c settings_amiga.c target.c target_amiga.c target_neogeo.c core_options.c core_config.c amiga_uae_options.c neogeo_core_options.c megadrive_core_options.c cli.c runtime.c config.c ui.c debugger.c help.c emu.c emu_geo.c emu_ami.c emu_mega.c sprite_debug.c seek_bar.c system_badge.c libretro_host.c clipboard.c state_wrap.c state_buffer.c tinyfiledialogs.c transition.c transition_doom.c transition_slide.c transition_explode.c transition_flip.c transition_rbar.c crt.c shader_ui.c custom_ui.c memory_track_ui.c shader_base.c shader_advanced.c shader_bloom.c train.c protect.c status_bar.c
ifeq ($(E9K_ENABLE_GL_COMPOSITE),1)
SRCS += gl_composite.c
else
SRCS += gl_composite_stub.c
endif
ifeq ($(E9K_ENABLE_MEGADRIVE),1)
SRCS += target_megadrive.c
else
SRCS += target_megadrive_stub.c
endif
ifeq ($(UNAME_S),Darwin)
SRCS += $(PLATFORM_POSIX_SRCS) $(PLATFORM_MACOS_SRCS)
else
SRCS += $(PLATFORM_POSIX_SRCS) $(PLATFORM_LINUX_SRCS)
endif
SRCS += syntax_highlight.c

TREE_SITTER_CORE_DIR = deps/tree-sitter/lib
TREE_SITTER_C_DIR = deps/tree-sitter-c

ifneq ($(wildcard $(TREE_SITTER_CORE_DIR)/src/lib.c),)
ifneq ($(wildcard $(TREE_SITTER_C_DIR)/src/parser.c),)
CFLAGS := -I$(TREE_SITTER_CORE_DIR)/include $(CFLAGS)
W64_CFLAGS := -I$(TREE_SITTER_CORE_DIR)/include $(W64_CFLAGS)
CFLAGS += -I$(TREE_SITTER_CORE_DIR)/src
W64_CFLAGS += -I$(TREE_SITTER_CORE_DIR)/src
CFLAGS += -DE9K_USE_TREE_SITTER
W64_CFLAGS += -DE9K_USE_TREE_SITTER
SRCS += $(TREE_SITTER_CORE_DIR)/src/lib.c
SRCS += $(TREE_SITTER_C_DIR)/src/parser.c
endif
endif

W64_SRCS = $(filter-out $(PLATFORM_POSIX_SRCS) $(PLATFORM_LINUX_SRCS) $(PLATFORM_MACOS_SRCS),$(SRCS)) $(PLATFORM_W64_SRCS) $(addprefix e9ui/, $(E9UI_SRCS)) $(addprefix lib9000/, $(LIB9000_SRCS))
SRCS_C = $(filter %.c,$(SRCS)) $(addprefix e9ui/, $(E9UI_SRCS)) $(addprefix lib9000/, $(LIB9000_SRCS))
SRCS_M = $(filter %.m,$(SRCS))
OBJS = $(SRCS_C:%.c=$(BUILD_DIR)/%.o) $(SRCS_M:%.m=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)
W64_OBJS = $(W64_SRCS:%.c=$(W64_BUILD_DIR)/%.o)
W64_DEPS = $(W64_OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(W64_TARGET): $(W64_OBJS)
	@mkdir -p $(dir $@)
	$(W64_CC) $(W64_CFLAGS) $(W64_LDFLAGS) -o $@ $(W64_OBJS) $(W64_LIBS)

w64: $(W64_TARGET)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: e9ui/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: lib9000/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.m
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(W64_BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(W64_CC) $(W64_CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(W64_TARGET) $(OBJS) $(DEPS) $(W64_OBJS) $(W64_DEPS)
	rm -rf $(BUILD_DIR) $(W64_BUILD_DIR)

bundle: $(TARGET)
	@mkdir -p $(BUNDLE_MACOS) $(BUNDLE_RESOURCES)
	@cp $(TARGET) $(BUNDLE_MACOS)/e9k-debugger
	@rm -rf $(BUNDLE_MACOS)/assets $(BUNDLE_MACOS)/system
	@cp -R assets $(BUNDLE_MACOS)/assets
	@cp -R dist/e9kd/system $(BUNDLE_MACOS)/system
	@cp assets/icons/osx/Engine9000.icns $(BUNDLE_RESOURCES)/Engine9000.icns
	@printf '%s\n' '<?xml version="1.0" encoding="UTF-8"?>' \
		'<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
		'<plist version="1.0">' \
		'<dict>' \
		'  <key>CFBundleName</key>' \
		'  <string>Engine9000</string>' \
		'  <key>CFBundleDisplayName</key>' \
		'  <string>Engine9000</string>' \
		'  <key>CFBundleIdentifier</key>' \
		'  <string>com.engine9000.debugger</string>' \
		'  <key>CFBundleExecutable</key>' \
		'  <string>e9k-debugger</string>' \
		'  <key>CFBundleIconFile</key>' \
		'  <string>Engine9000</string>' \
		'  <key>CFBundlePackageType</key>' \
		'  <string>APPL</string>' \
		'  <key>CFBundleVersion</key>' \
		'  <string>1.0</string>' \
		'  <key>CFBundleShortVersionString</key>' \
		'  <string>1.0</string>' \
		'</dict>' \
		'</plist>' > $(BUNDLE_DIR)/Info.plist

.PHONY: all clean macos-app

-include $(DEPS)
-include $(W64_DEPS)


HEADLESS=
HEADLESS+=--headless

include tests/neogeo.mk
include tests/amiga.mk
include tests/ui.mk

test:  $(AMIGA_TESTS) $(NEOGEO_TESTS) $(UI_TESTS)
	@echo
	@echo "✅✅✅ TESTS PASSED ✅✅✅"

test-ui: $(UI_TESTS)
	@echo
	@echo "✅✅✅ UI TESTS PASSED ✅✅✅"

test-amiga: $(AMIGA_TESTS)
	@echo
	@echo "✅✅✅ AMIGA TESTS PASSED ✅✅✅"

test-neogeo: $(NEOGEO_TESTS)
	@echo
	@echo "✅✅✅ NEO GEO TESTS PASSED ✅✅✅"

test-remake: $(AMIGA_REMAKE) $(UI_REMAKE) $(NEOGEO_REMAKE)
	@echo
	@echo "✅✅✅ REMAKE COMPLETE ✅✅✅"

test-remake-amiga: $(AMIGA_REMAKE)
	@echo
	@echo "✅✅✅ REMAKE AMIGA COMPLETE ✅✅✅"

test-remake-ui: $(UI_REMAKE)
	@echo
	@echo "✅✅✅ REMAKE UI COMPLETE ✅✅✅"

test-remake-neogeo: $(NEOGEO_REMAKE)
	@echo
	@echo "✅✅✅ REMAKE NEOGEO COMPLETE ✅✅✅"

test-show: HEADLESS :=
test-show: test
