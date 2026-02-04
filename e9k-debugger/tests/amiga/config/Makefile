AMIGA_GCC=/opt/amiga/bin/m68k-amigaos-gcc
CFLAGS=-fomit-frame-pointer -noixemul -Wall -Werror
ADF=config.adf
ADF9000=../../../../tools/amiga/adf9000/adf9000
LOAD9000=../../../../tools/amiga/load9000/load9000

all: config $(ADF)

config: config.c Makefile
	$(AMIGA_GCC) $(CFLAGS) -g $< -o $@

clean:
	rm config $(ADF)

$(ADF):
	$(ADF9000) $(ADF) create --label TEST
	$(ADF9000) $(ADF) mkdir /S
	$(ADF9000) $(ADF) mkdir /C
	$(ADF9000) $(ADF) cp Startup-Sequence /S/Startup-Sequence
	$(ADF9000) $(ADF) cp $(LOAD9000) /C/load9000
	$(ADF9000) $(ADF) cp config /config
