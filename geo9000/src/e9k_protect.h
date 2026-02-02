#ifndef E9K_PROTECT_H
#define E9K_PROTECT_H

#include <stddef.h>
#include <stdint.h>
#include "e9k-lib.h"

void
e9k_protect_reset(void);

int
e9k_protect_add(uint32_t addr24, uint32_t sizeBits, uint32_t mode, uint32_t value);

void
e9k_protect_remove(uint32_t index);

size_t
e9k_protect_read(e9k_debug_protect_t *out, size_t cap);

uint64_t
e9k_protect_getEnabledMask(void);

void
e9k_protect_setEnabledMask(uint64_t mask);

void
e9k_protect_filterWrite(uint32_t addr24, uint32_t sizeBits, uint32_t oldValue, int oldValueValid, uint32_t *inoutValue);

#endif // E9K_PROTECT_H
