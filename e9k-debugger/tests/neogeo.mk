NEOGEO_TESTS=test-neogeobasic

# makers

make-test-neogeobasic: all 
	./e9k-debugger --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --make-test tests/results/neogeo/basic

# testers

test-neogeobasic: all
	@printf "NEO GEO SOURCE LEVEL DEBUG EXAMPLE..."
	@./e9k-debugger --headless --neogeo --source-dir=./tests/neogeo/basic --elf=./tests/neogeo/basic/basic.elf --rom=./tests/neogeo/basic/basic.neo --test tests/results/neogeo/basic 2> /dev/null > /dev/null
	@echo "PASSED ✅"
