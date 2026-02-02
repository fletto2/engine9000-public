AMIGA_TESTS=test-amigasmoke test-amigacoreoptions test-amigaexample

# makers

make-test-amigaexample: all tests/amiga/example/example.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --make-test tests/results/amiga/example

make-test-amigacoreoptions: all tests/amiga/example/example.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --make-test tests/results/amiga/amigacoreoptions

make-test-amigasmoke: all tests/amiga/smoke/smoke.adf
	./e9k-debugger --amiga --uae=./tests/amiga/smoke/smoke.uae --make-smoke tests/results/amiga/smoke

# testers

test-amigaexample: all tests/amiga/example/example.adf
	@printf "AMIGA SOURCE LEVEL DEBUG EXAMPLE..."
	@./e9k-debugger --headless --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --test tests/results/amiga/example > /dev/null 2> /dev/null
	@echo " PASSED ✅"

test-amigacoreoptions: all tests/amiga/example/example.adf
	@printf "AMIGA CORE OPTIONS..."
	@./e9k-debugger --headless --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --test tests/results/amiga/amigacoreoptions  > /dev/null 2> /dev/null
	@echo "PASSED ✅"

test-amigasmoke: all tests/amiga/smoke/smoke.adf
	@printf "AMIGA SMOKE TEST..."
	@./e9k-debugger --headless --audio-volume=0 --amiga --uae=./tests/amiga/smoke/smoke.uae --smoke-test tests/results/amiga/smoke > /dev/null 2> /dev/null
	@echo "PASSED ✅"

# assets

tests/amiga/example/example.adf:
	make -C tests/amiga/example
