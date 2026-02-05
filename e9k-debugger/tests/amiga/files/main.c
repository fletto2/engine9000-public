#include <exec/memory.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "files.h"
#include "printf.h"
#include "fun.h"

volatile example_t example = {
  .a = 0xDEADBEEF,
  .b = 0xF00DD00F,
};


int main(int argc, char **argv)
{  
  volatile int start = 1;

  if (start) {
    start = 2;
  }

  _printf("STEPPING TEST\n");
  
  volatile APTR apointer;
  if (!(apointer = AllocMem(100, MEMF_ANY))) {
    _printf("AllocMem failed\n");
  } else {
    _printf("AllocMem allocated!\n");
  }

  if (apointer) {
    FreeMem(apointer, 100);
  }
  
  volatile int one = 1, two = 2;

  while(1) {
    volatile int fun = funtimes(one, two);
    _printf("fun = %d\n", fun);
  }
  
  return 0;
}
