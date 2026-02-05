#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "printf.h"

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
