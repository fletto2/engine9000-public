NEOGEO_TESTS=test-neogeosavestate test-neogeostepping
NEOGEO_REMAKE=remake-test-neogeosavestate remake-test-neogeostepping

# makers

make-test-neogeosavestate: all 
	./e9k-debugger --neogeo --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/neogeo/savestate

make-test-neogeostepping: tests/neogeo/stepping/build/rom.elf
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/stepping --elf=./tests/neogeo/stepping/build/rom.elf --rom=./tests/neogeo/stepping/build/stepping.neo --make-test tests/results/neogeo/stepping


# remakers

remake-test-neogeosavestate: all 
	./e9k-debugger --neogeo --volume=0 --rom=./tests/neogeo/basic/basic.neo --remake-test tests/results/neogeo/savestate

remake-test-neogeostepping: tests/neogeo/stepping/build/rom.elf
	./e9k-debugger --neogeo --volume=0 --source-dir=./tests/neogeo/stepping --elf=./tests/neogeo/stepping/build/rom.elf --rom=./tests/neogeo/stepping/build/stepping.neo --remake-test tests/results/neogeo/stepping


# testers


test-neogeosavestate: all
	@printf "NEO GEO SAVE STATE ($@) ..." 
	@./e9k-debugger $(HEADLESS) --neogeo --rom=./tests/neogeo/basic/basic.neo --test tests/results/neogeo/savestate >> test.log 2>&1
	@echo "PASSED ✅"

test-neogeostepping: tests/neogeo/stepping/build/rom.elf
	@printf "NEO GEO STEPPING ($@) ..." 
	@./e9k-debugger $(HEADLESS) --neogeo --source-dir=./tests/neogeo/stepping --elf=./tests/neogeo/stepping/build/rom.elf --rom=./tests/neogeo/stepping/build/stepping.neo --test tests/results/neogeo/stepping  >> test.log 2>&1
	@echo "PASSED ✅"

# assets

tests/neogeo/stepping/build/rom.elf:
	make -C ./tests/neogeo/stepping/
