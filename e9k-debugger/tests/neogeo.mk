NEOGEO_TESTS=test-neogeobasic test-neogeosavestate

HEADLESS=
HEADLESS+=--headless

# makers

make-test-neogeobasic: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/neogeo/basic

make-test-neogeosavestate: all 
	./e9k-debugger --neogeo --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/neogeo/savestate

# remakers

remake-test-neogeobasic: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/neogeo/basic

remake-test-neogeosavestate: all 
	./e9k-debugger --neogeo --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/neogeo/savestate

# testers

test-neogeobasic: all
	@printf "NEO GEO SOURCE LEVEL DEBUG EXAMPLE..."
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/neogeo/basic >> test.log 2>&1
	@echo "PASSED ✅"

test-neogeosavestate: all
	@printf "NEO GEO SAVE STATE..." 
	@./e9k-debugger $(HEADLESS) --neogeo --rom=./tests/neogeo/basic/basic.neo --test tests/results/neogeo/savestate >> test.log 2>&1
	@echo "PASSED ✅"
