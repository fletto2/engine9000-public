ifeq ($(UNAME_S),Darwin)

UI_TESTS=test-uibasic test-uitabbing
UI_REMAKE=remake-test-uibasic remake-test-uitabbing

# makers

make-test-uibasic: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/ui/basic


make-test-uitabbing: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/tabbing.neo --make-test tests/results/ui/tabbing


# remakers

remake-test-uibasic: all 
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/ui/basic

remake-test-uitabbing: all 
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/tabbing.neo --remake-test tests/results/ui/tabbing

# testers

test-uibasic: all
	@printf "UI BASIC EXAMPLE ($@) ..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/ui/basic >> test.log 2>&1
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
