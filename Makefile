MINGW_CC=x86_64-w64-mingw32-gcc 
JOBS= -j12
MEGA9000_DIR=mega9000
MEGA9000_MAKEFILE=$(MEGA9000_DIR)/Makefile.libretro
PUBLIC_DIR=../engine9000-public
PUBLIC_MODULES=e9k-debugger geo9000 ami9000
PUBLIC_SHARED=e9k-lib tools e9ui
HOST_OS := $(shell uname -s)
LIBRETRO_PLATFORM := unix
ifeq ($(HOST_OS),Darwin)
LIBRETRO_PLATFORM := osx
endif

.PHONY: all w64 release-w64 clean test mega9000 mega9000-w64 mega9000-clean mega9000-support st9000 st9000-w64 st9000-clean update-public

all:
	$(MAKE) $(JOBS) -C ami9000 platform=$(LIBRETRO_PLATFORM)
	$(MAKE) $(JOBS) -C geo9000/libretro platform=$(LIBRETRO_PLATFORM)
	$(MAKE) mega9000
	$(MAKE) st9000
	$(MAKE) $(JOBS) -C e9k-debugger
	$(MAKE) $(JOBS) -C tools/amiga/adf9000
	$(MAKE) $(JOBS) -C tools/amiga/hdf9000
	$(MAKE) $(JOBS) -C tools/amiga/v-hunk

w64:
	$(MAKE) $(JOBS) -C ami9000 platform=win CC=$(MINGW_CC)
	$(MAKE) $(JOBS) -C geo9000/libretro platform=win64 CC=$(MINGW_CC)
	$(MAKE) mega9000-w64
	$(MAKE) $(JOBS) -C e9k-debugger w64
	$(MAKE) $(JOBS) -C tools/amiga/adf9000 w64 CC=$(MINGW_CC)
	$(MAKE) $(JOBS) -C tools/amiga/hdf9000 w64 CC=$(MINGW_CC)
	$(MAKE) $(JOBS) -C tools/amiga/v-hunk w64 CC=$(MINGW_CC)

release-w64: w64
	$(MAKE) -C e9k-debugger release-w64-package

clean:
	$(MAKE) $(JOBS) -C ami9000 platform=win CC=$(MINGW_CC) clean
	$(MAKE) $(JOBS) -C ami9000 clean
	$(MAKE) $(JOBS) -C geo9000/libretro platform=win64 CC=$(MINGW_CC) clean
	$(MAKE) $(JOBS) -C geo9000/libretro clean
	$(MAKE) mega9000-clean
	$(MAKE) st9000-clean
	$(MAKE) $(JOBS) -C e9k-debugger clean
	$(MAKE) $(JOBS) -C tools/amiga/adf9000 clean
	$(MAKE) $(JOBS) -C tools/amiga/hdf9000 clean
	$(MAKE) $(JOBS) -C tools/amiga/v-hunk clean

test:
	$(MAKE) -C e9k-debugger test
	$(MAKE) -C tools/amiga/adf9000 test

mega9000-support:
	@if [ -d $(MEGA9000_DIR) ]; then \
		echo "mega9000 already present"; \
	else \
		echo "Cloning mega9000..."; \
		git clone https://github.com/alpine9000/mega9000.git $(MEGA9000_DIR) && \
		echo "mega9000 support is ready"; \
	fi

mega9000:
	@if [ -f $(MEGA9000_MAKEFILE) ]; then \
		$(MAKE) $(JOBS) -C $(MEGA9000_DIR) -f Makefile.libretro platform=$(LIBRETRO_PLATFORM); \
	else \
		echo "mega9000 is skipped (repo not present). Run 'make mega9000-support' to pull submodule support."; \
	fi

mega9000-w64:
	@if [ -f $(MEGA9000_MAKEFILE) ]; then \
		$(MAKE) $(JOBS) -C $(MEGA9000_DIR) -f Makefile.libretro platform=win64 CC=$(MINGW_CC); \
	else \
		echo "mega9000 is skipped (repo not present). Run 'make mega9000-support' to pull submodule support."; \
	fi

mega9000-clean:
	@if [ -f $(MEGA9000_MAKEFILE) ]; then \
		$(MAKE) $(JOBS) -C $(MEGA9000_DIR) -f Makefile.libretro clean; \
	else \
		echo "mega9000 is skipped (repo not present)."; \
	fi

ST9000_DIR=st9000
ST9000_MAKEFILE=$(ST9000_DIR)/Makefile.libretro

st9000:
	@if [ -f $(ST9000_MAKEFILE) ]; then \
		$(MAKE) $(JOBS) -C $(ST9000_DIR) -f Makefile.libretro platform=$(LIBRETRO_PLATFORM); \
		cp $(ST9000_DIR)/hatari_libretro.so e9k-debugger/system/st9000.so 2>/dev/null || \
		cp $(ST9000_DIR)/hatari_libretro.dylib e9k-debugger/system/st9000.dylib 2>/dev/null || \
		true; \
	else \
		echo "st9000 is skipped (repo not present)."; \
	fi

st9000-w64:
	@if [ -f $(ST9000_MAKEFILE) ]; then \
		$(MAKE) $(JOBS) -C $(ST9000_DIR) -f Makefile.libretro platform=win64 CC=$(MINGW_CC); \
	else \
		echo "st9000 is skipped (repo not present)."; \
	fi

st9000-clean:
	@if [ -f $(ST9000_MAKEFILE) ]; then \
		$(MAKE) $(JOBS) -C $(ST9000_DIR) -f Makefile.libretro clean; \
	else \
		echo "st9000 is skipped (repo not present)."; \
	fi

update-public:
	@mkdir -p "$(PUBLIC_DIR)"
	@rsync -a --delete --exclude='.git/' --exclude='.git' Makefile "$(PUBLIC_DIR)/"
	@for d in $(PUBLIC_MODULES) $(PUBLIC_SHARED); do \
		if [ -d "$$d" ]; then \
			rsync -a --delete \
				--exclude='.git/' \
				--exclude='.git' \
				--exclude='build/' \
				"$$d/" "$(PUBLIC_DIR)/$$d/"; \
		else \
			echo "$$d is skipped (repo not present)."; \
		fi; \
	done
