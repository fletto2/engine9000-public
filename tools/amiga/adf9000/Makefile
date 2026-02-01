CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2
EXE=adf9000
W64_EXE=adf9000.exe

all: $(EXE)

w64: $(W64_EXE)

$(EXE): main.c
	$(CC) $(CFLAGS) -o $@ $<

$(W64_EXE): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXE)

test: $(EXE)
	./$(EXE) disk.adf --time 1769975282 create --label TEST
	./$(EXE) disk.adf --time 1769975282 mkdir /S
	./$(EXE) disk.adf --time 1769975282 mkdir /C
	./$(EXE) disk.adf --time 1769975282 cp ./test/Startup-Sequence /S/Startup-Sequence
	./$(EXE) disk.adf --time 1769975282 cp ./test/load9000 /C/load9000
	./$(EXE) disk.adf --time 1769975282 cp ./test/example /example
	diff disk.adf ./test/disk.adf
