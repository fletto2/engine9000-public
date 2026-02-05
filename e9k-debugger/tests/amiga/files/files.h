#pragma once
#include <exec/types.h>


typedef struct {
  ULONG a;
  ULONG b;
} example_t;


extern volatile example_t example;
