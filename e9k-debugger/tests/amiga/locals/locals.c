#include <exec/memory.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int
_printf(const char *fmt, ...)
{
  static char  buf[255];
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vsprintf(buf, fmt, ap);   /* no bounds checking */
    va_end(ap);

    volatile char* debug = (char*)0xFC0000;
    char* p = buf;
    for (int i = 0; i < ret; i++) {
      *debug = *p++;
    }
    
    return ret; /* number of chars written */
}

typedef struct {
  ULONG a;
  ULONG b;
} example_t;

volatile example_t example = {
  .a = 0xDEADBEEF,
  .b = 0xF00DD00F,
};


int function(int i)
{
  _printf("function: %d %08x\n", i, example.b);
  example.a++;
  example.b++;
  return 1;
}

int funtimes(int x, int y)
{
  volatile int j, k;
  j = x;
  k = y;
  function(j);
  function(k);
  return k;
}

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
