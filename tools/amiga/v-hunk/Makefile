CC ?= cc
W64_CC ?= x86_64-w64-mingw32-gcc
CFLAGS ?= -std=c99 -Wall -Wextra -O2
ADDR2LINE_EXE=v-hunk-addr2line
OBJDUMP_EXE=v-hunk-objdump
ADDR2LINE_W64_EXE=v-hunk-addr2line.exe
OBJDUMP_W64_EXE=v-hunk-objdump.exe

all: $(ADDR2LINE_EXE) $(OBJDUMP_EXE)
	cp $(ADDR2LINE_EXE) $(OBJDUMP_EXE) ../../../e9k-debugger/system 

w64: $(ADDR2LINE_W64_EXE) $(OBJDUMP_W64_EXE)
	cp $(ADDR2LINE_W64_EXE) $(OBJDUMP_W64_EXE) ../../../e9k-debugger/system 

$(ADDR2LINE_EXE): addr2line.c
	$(CC) $(CFLAGS) -o $@ $<

$(OBJDUMP_EXE): objdump.c
	$(CC) $(CFLAGS) -o $@ $<

$(ADDR2LINE_W64_EXE): addr2line.c
	$(W64_CC) $(CFLAGS) -o $@ $<

$(OBJDUMP_W64_EXE): objdump.c
	$(W64_CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(ADDR2LINE_EXE) $(OBJDUMP_EXE) $(ADDR2LINE_W64_EXE) $(OBJDUMP_W64_EXE)
