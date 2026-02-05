AMIGA_GCC=/opt/amiga/bin/m68k-amigaos-gcc
DEBUG_PREFIX_MAP=-fdebug-prefix-map=$(shell pwd)=.
CFLAGS=-fomit-frame-pointer -noixemul -Wall -Werror $(DEBUG_PREFIX_MAP)
ADF=files.adf
ADF9000=../../../../tools/amiga/adf9000/adf9000
LOAD9000=../../../../tools/amiga/load9000/load9000

SRCS=main.c printf.c fun.c

all: $(ADF)

files: $(SRCS) Makefile
	$(AMIGA_GCC) $(CFLAGS) -g -fomit-frame-pointer $(SRCS) -o $@

clean:
	rm files $(ADF)

$(ADF): files
	$(ADF9000) $(ADF) create --label TEST
	$(ADF9000) $(ADF) mkdir /S
	$(ADF9000) $(ADF) mkdir /C
	$(ADF9000) $(ADF) cp Startup-Sequence /S/Startup-Sequence
	$(ADF9000) $(ADF) cp $(LOAD9000) /C/load9000
	$(ADF9000) $(ADF) cp files /files
