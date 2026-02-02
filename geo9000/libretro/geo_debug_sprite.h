#pragma once

#include <stddef.h>
#include <stdint.h>
#include "e9k-geo.h"
#ifdef __cplusplus
extern "C" {
#endif
  
size_t
e9k_debug_neogeo_get_sprite_state(e9k_debug_sprite_state_t *out, size_t cap);

#ifdef __cplusplus
}
#endif
