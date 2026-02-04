AMIGA_TESTS=test-amigasmoke \
	    test-amigacoreoptions \
            test-amigaexample \
            test-amigasavestate \
            test-amigaconfig \
            test-amigalocals \
            test-amigastepping

AMIGA_REMAKE=remake-test-amigacoreoptions \
             remake-test-amigaexample \
             remake-test-amigasavestate \
             remake-test-amigaconfig \
	     remake-test-amigalocals \
	     remake-test-amigastepping

# makers

make-test-amigalocals: all tests/amiga/locals/locals.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/locals/ --uae=./tests/amiga/locals/locals.uae --hunk=./tests/amiga/locals/locals --make-test tests/results/amiga/locals

make-test-amigaconfig: all 
	./e9k-debugger --amiga --make-test tests/results/amiga/config

make-test-amigaexample: all tests/amiga/example/example.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --make-test tests/results/amiga/example

make-test-amigacoreoptions: all tests/amiga/example/example.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --make-test tests/results/amiga/amigacoreoptions

make-test-amigasmoke: all tests/amiga/smoke/smoke.adf
	./e9k-debugger --amiga --uae=./tests/amiga/smoke/smoke.uae --make-smoke tests/results/amiga/smoke

make-test-amigasavestate: all tests/amiga/smoke/smoke.adf
	./e9k-debugger --amiga --uae=./tests/amiga/smoke/smoke.uae --make-test tests/results/amiga/savestate

make-test-amigastepping: all tests/amiga/stepping/stepping.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/stepping/ --uae=./tests/amiga/stepping/stepping.uae \
	--hunk=./tests/amiga/stepping/stepping --make-test tests/results/amiga/stepping

# remakers

remake-test-amigalocals: all tests/amiga/locals/locals.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/locals/ --uae=./tests/amiga/locals/locals.uae --hunk=./tests/amiga/locals/locals --remake-test tests/results/amiga/locals

remake-test-amigaconfig: all 
	./e9k-debugger --amiga --remake-test tests/results/amiga/config

remake-test-amigaexample: all tests/amiga/example/example.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --remake-test tests/results/amiga/example

remake-test-amigacoreoptions: all tests/amiga/example/example.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --remake-test tests/results/amiga/amigacoreoptions

remake-test-amigasavestate: all tests/amiga/smoke/smoke.adf
	./e9k-debugger --amiga --uae=./tests/amiga/smoke/smoke.uae --remake-test tests/results/amiga/savestate

remake-test-amigastepping: all tests/amiga/stepping/stepping.adf
	./e9k-debugger --amiga --source-dir=./tests/amiga/stepping/ --uae=./tests/amiga/stepping/stepping.uae \
	--hunk=./tests/amiga/stepping/stepping --remake-test tests/results/amiga/stepping


# testers

test-amigaconfig: all
	@printf "AMIGA CONFIG ($@) ..."
	@./e9k-debugger $(HEADLESS) --amiga --test tests/results/amiga/config >> test.log 2>&1
	@echo " PASSED ✅"

test-amigaexample: all tests/amiga/example/example.adf
	@printf "AMIGA SOURCE LEVEL DEBUG EXAMPLE ($@) ..."
	@./e9k-debugger $(HEADLESS) --volume=0 --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --test tests/results/amiga/example >> test.log 2>&1
	@echo " PASSED ✅"

test-amigalocals: all tests/amiga/locals/locals.adf
	@printf "AMIGA LOCALS ($@) ..."
	@./e9k-debugger $(HEADLESS) --volume=0 --amiga --source-dir=./tests/amiga/locals/ --uae=./tests/amiga/locals/locals.uae --hunk=./tests/amiga/locals/locals --test tests/results/amiga/locals >> test.log 2>&1
	@echo " PASSED ✅"

test-amigacoreoptions: all tests/amiga/example/example.adf
	@printf "AMIGA CORE OPTIONS ($@) ..."
	@./e9k-debugger $(HEADLESS) --amiga --source-dir=./tests/amiga/example/ --uae=./tests/amiga/example/example.uae --hunk=./tests/amiga/example/example --test tests/results/amiga/amigacoreoptions  >> test.log 2>&1
	@echo "PASSED ✅"

test-amigasmoke: all tests/amiga/smoke/smoke.adf
	@printf "AMIGA SMOKE TEST ($@) ..."
	@./e9k-debugger $(HEADLESS) --volume=0 --amiga --uae=./tests/amiga/smoke/smoke.uae --smoke-test tests/results/amiga/smoke>> test.log 2>&1
	@echo "PASSED ✅"

test-amigasavestate: all tests/amiga/smoke/smoke.adf
	@printf "AMIGA SAVESTATE ($@) ..."
	@./e9k-debugger $(HEADLESS) --volume=0 --amiga --uae=./tests/amiga/smoke/smoke.uae --test tests/results/amiga/savestate  >> test.log 2>&1
	@echo "PASSED ✅"

test-amigastepping: all tests/amiga/stepping/stepping.adf
	@printf "AMIGA STEPPING ($@) ..."
	@./e9k-debugger $(HEADLESS) --volume=0 --amiga --source-dir=./tests/amiga/stepping/ --uae=./tests/amiga/stepping/stepping.uae \
	--hunk=./tests/amiga/stepping/stepping --test tests/results/amiga/stepping >> test.log 2>&1
	@echo "PASSED ✅"
# assets

tests/amiga/example/example.adf:
	make -C tests/amiga/example
