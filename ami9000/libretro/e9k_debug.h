#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uae/types.h"
#include "e9k-lib.h"

// Debug base register sections (passed to e9k_debug_set_debug_base_callback()).
#define E9K_DEBUG_BASE_SECTION_TEXT 0u
#define E9K_DEBUG_BASE_SECTION_DATA 1u
#define E9K_DEBUG_BASE_SECTION_BSS  2u

int
e9k_debug_instructionHook(uaecptr pc, uae_u16 opcode);

void
e9k_debug_pause(void);

void
e9k_debug_resume(void);

int
e9k_debug_is_paused(void);

void
e9k_debug_step_instr(void);

void
e9k_debug_step_line(void);

void
e9k_debug_step_next(void);

void
e9k_debug_step_out(void);

size_t
e9k_debug_read_callstack(uint32_t *out, size_t cap);

size_t
e9k_debug_read_regs(uint32_t *out, size_t cap);

size_t
e9k_debug_read_memory(uint32_t addr, uint8_t *out, size_t cap);

int
e9k_debug_write_memory(uint32_t addr, uint32_t value, size_t size);

size_t
e9k_debug_disassemble_quick(uint32_t pc, char *out, size_t cap);

uint64_t
e9k_debug_read_cycle_count(void);

void
e9k_debug_add_breakpoint(uint32_t addr);

void
e9k_debug_remove_breakpoint(uint32_t addr);

void
e9k_debug_add_temp_breakpoint(uint32_t addr);

void
e9k_debug_remove_temp_breakpoint(uint32_t addr);

// Optional host callback invoked once per vblank/frame.
void
e9k_debug_set_vblank_callback(void (*cb)(void *), void *user);

void
e9k_vblank_notify(void);

// Optional host callback invoked when the target writes a new relocatable base.
void
e9k_debug_set_debug_base_callback(void (*cb)(uint32_t section, uint32_t base));

// Optional host callback invoked when the target requests a breakpoint via a fake debug peripheral.
void
e9k_debug_set_debug_breakpoint_callback(void (*cb)(uint32_t addr));

// Optional host callback used for source location resolution in cores that support source-line stepping.
void
e9k_debug_set_source_location_resolver(int (*resolver)(uint32_t pc24, uint64_t *out_location, void *user), void *user);

void
e9k_debug_set_debug_option(e9k_debug_option_t option, uint32_t argument, void *user);
