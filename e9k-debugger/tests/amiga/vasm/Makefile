ADF=vasm.adf
ADF9000=../../../../tools/amiga/adf9000/adf9000
LOAD9000=../../../../tools/amiga/load9000/load9000
OBJS=build/vasm.o

all: $(ADF)

build/%.o: %.s Makefile
	@mkdir -p build
	vasmm68k_mot -Fhunk $< -o $@

vasm: $(OBJS)
	/usr/local/amiga/vgcc/bin/vlink $(OBJS) -o vasm

clean:
	rm -rf build/* vasm $(ADF)

$(ADF): vasm
	$(ADF9000) $(ADF) create --label TEST
	$(ADF9000) $(ADF) mkdir /S
	$(ADF9000) $(ADF) mkdir /C
	$(ADF9000) $(ADF) cp Startup-Sequence /S/Startup-Sequence
	$(ADF9000) $(ADF) cp $(LOAD9000) /C/load9000
	$(ADF9000) $(ADF) cp vasm /vasm
