ifeq ($(UNAME_S),Darwin)

UI_TESTS=test-uibasic test-uitabbing test-uishader test-uihotkeys
UI_REMAKE=remake-test-uibasic remake-test-uitabbing remake-test-uishader remake-test-uihotkeys

# makers

make-test-uibasic: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/ui/basic


make-test-uihotkeys: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/ui/hotkeys


make-test-uishader: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/ui/shader


make-test-uitabbing: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/tabbing.neo --make-test tests/results/ui/tabbing


# remakers

remake-test-uibasic: all 
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/ui/basic

remake-test-uihotkeys: all 
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/ui/hotkeys

remake-test-uishader: all 
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/ui/shader

remake-test-uitabbing: all 
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/tabbing.neo --remake-test tests/results/ui/tabbing

# testers

test-uibasic: all
	@printf "UI BASIC EXAMPLE ($@) ..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/ui/basic >> test.log 2>&1
	@echo "PASSED ✅"

test-uihotkeys: all
	@printf "UI HOTKEYS EXAMPLE ($@) ..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/ui/hotkeys >> test.log 2>&1
	@echo "PASSED ✅"

test-uishader: all
	@printf "UI SHADER EXAMPLE ($@) ..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/ui/shader >> test.log 2>&1
	@echo "PASSED ✅"

test-uitabbing: all
	@printf "UI TABBING EXAMPLE ($@) ..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/tabbing.neo --test tests/results/ui/tabbing >> test.log 2>&1
	@echo "PASSED ✅"

else

UI_TESTS=
UI_REMAKE=

make-test-uibasic:
	@echo "SKIPPED (macOS only)"

remake-test-uibasic:
	@echo "SKIPPED (macOS only)"

test-uibasic:
	@echo "SKIPPED (macOS only)"

endif
