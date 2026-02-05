UI_TESTS=test-uibasic test-uihelp
UI_REMAKE=remake-test-uibasic remake-test-uihelp

# makers

make-test-uibasic: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/ui/basic

make-test-uihelp: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/ui/help


# remakers

remake-test-uibasic: all 
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/ui/basic

remake-test-uihelp: all 
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/ui/help


# testers

test-uibasic: all
	@printf "UI BASIC EXAMPLE ($@) ..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/ui/basic >> test.log 2>&1
	@echo "PASSED ✅"

test-uihelp: all
	@printf "UI HELP ($@) ..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/ui/help >> test.log 2>&1
	@echo "PASSED ✅"

