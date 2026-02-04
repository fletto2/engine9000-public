#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uint32_t a;
  uint32_t b;
} example_t;

volatile example_t example = {
  .a = 0xDEADBEEF,
  .b = 0xF00DD00F,
};

volatile uint8_t vblank=0;

void
rom_callback_VBlank(void)
{
  vblank=1;
}

void
waitVblank(void)
{
  while (!vblank);
  vblank=0;
}

void
debug_printf(const char* fmt, ...)
{
  if (!fmt) {
    return;
  }

  volatile int x = 0;
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  x = strlen(buf);
  volatile uint8_t* console = (uint8_t*)0xFFFF0;
  (void)x;
  char* ptr = buf;
  while (*ptr != 0) {
    *console = *ptr++;
  }
}

int
function(int i)
{
  debug_printf("function: %d %08x\n", i, example.b);
  example.a++;
  example.b++;
  return 1;
}

int
funtimes(int x, int y)
{
  volatile int j, k;
  j = x;
  k = y;
  function(j);
  function(k);
  return k;
}

int main(void)
{
  volatile int start = 1;
  
  if (start) {
    start = 2;
  }
  
  debug_printf("STEPPING TEST\n");
    
  volatile int one = 1, two = 2;

  while(1) {
    volatile int fun = funtimes(one, two);
    debug_printf("fun = %d\n", fun);
    waitVblank();
  }

  return 0;
}
