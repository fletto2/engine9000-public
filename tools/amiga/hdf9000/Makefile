CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2
EXE=hdf9000
W64_EXE=hdf9000.exe

all: $(EXE)

w64: $(W64_EXE)

$(EXE): main.c
	$(CC) $(CFLAGS) -o $@ $<

$(W64_EXE): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXE) $(W64_EXE)
