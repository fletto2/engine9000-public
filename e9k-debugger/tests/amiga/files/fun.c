#include "printf.h"
#include "fun.h"
#include "files.h"
#include "printf.h"

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
