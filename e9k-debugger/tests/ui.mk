UI_TESTS=test-uibasic

# makers

make-test-uibasic: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/ui/basic

# remakers

remake-test-uibasic: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/ui/basic


# testers

test-uibasic: all
	@printf "UI BASIC EXAMPLE ($@) ..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/ui/basic >> test.log 2>&1
	@echo "PASSED ✅"

