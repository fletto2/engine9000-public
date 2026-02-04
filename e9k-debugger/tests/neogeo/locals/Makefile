CC=/opt/homebrew/bin/m68k-neogeo-elf-gcc
OBJCOPY=/opt/homebrew/bin/m68k-neogeo-elf-objcopy
NM=/opt/homebrew/bin/m68k-neogeo-elf-nm
OBJDUMP=/opt/homebrew/bin/m68k-neogeo-elf-objdump
PKG_CONFIG=/opt/homebrew/bin/pkg-config

GAME=locals
ROM_DIR=build/rom
GAME_DIR=$(ROM_DIR)/$(GAME)
ROM_P1=$(GAME_DIR)/$(GAME).p1

SRC=main.c
OBJ=build/main.o

OPT_FLAGS=-g -O2 -fomit-frame-pointer
LDFLAGS_MAP=-Wl,-Map,build/rom.map -Wl,--cref
PKG_CFLAGS=$(shell $(PKG_CONFIG) --cflags ngdevkit)
PKG_LIBS=$(shell $(PKG_CONFIG) --libs ngdevkit)
DEBUG_PREFIX_MAP=-fdebug-prefix-map=$(shell pwd)=.
CFLAGS=-DNEOGEO $(OPT_FLAGS) -I. -Isrc $(PKG_CFLAGS) -std=c99 -fomit-frame-pointer -Wall -Wextra -Werror $(DEBUG_PREFIX_MAP)

DWARF=1
ifeq ($(DWARF),1)
CFLAGS+=-fno-eliminate-unused-debug-types
endif

.PHONY: all clean neo

all: neo
neo: build/$(GAME).neo

build $(ROM_DIR) $(GAME_DIR):
	@mkdir -p $@

$(OBJ): $(SRC) | build
	$(CC) $(CFLAGS) -c $< -o $@

build/rom.elf: $(OBJ) | $(GAME_DIR)
	$(CC) $(DEBUG_PREFIX_MAP) -g $(OPT_FLAGS) $(LDFLAGS_MAP) -o $@ $(OBJ) $(PKG_LIBS)
	-@$(NM) -n -S $@ > build/rom.sym || true
	-@$(OBJDUMP) -h -t $@ > build/rom.dump || true

$(ROM_P1): build/rom.elf | $(GAME_DIR)
	$(OBJCOPY) -O binary -S -R .text2 --gap-fill 0xff --pad-to 524288 build/rom.elf $@.bin
	dd if=$@.bin of=$@ conv=notrunc,swab status=none
	rm $@.bin

$(ROM_DIR)/%: assets/roms/% | $(ROM_DIR)
	cp $< $@

$(GAME_DIR)/%: assets/roms/% | $(GAME_DIR)
	cp $< $@

build/$(GAME).neo: $(ROM_P1) | $(ROM_DIR)
	@echo "[neosd] Preparing NeoSD image from $(GAME_DIR)"
	@tmpdir=build/neosd/$(GAME)_std && \
	  rm -rf "$$tmpdir" && mkdir -p "$$tmpdir" && \
	  echo "[neosd] Staging ROMs in $$tmpdir" && \
	  set -e; \
	  for part in p1; do \
	    src="$(GAME_DIR)/$(GAME).$$part"; \
	    if [ -f "$$src" ]; then \
	      dst="$$tmpdir/000-$$part.$$part"; \
	      cp "$$src" "$$dst"; \
	      echo "  + $$src -> $$dst"; \
	    fi; \
	  done && \
	  echo "[neosd] Running neosdconv" && \
	  neosdconv -i "$$tmpdir" -o "$$tmpdir/$(GAME)_std.neo" -n "$(shell echo $(GAME) | tr a-z A-Z)" && \
	  cp "$$tmpdir/$(GAME)_std.neo" "$@" && \
	  echo "[neosd] Wrote $@"

clean:
	rm -rf build
