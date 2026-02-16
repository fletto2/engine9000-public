/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum base_map_mode {
    BASE_MAP_MODE_BASIC = 0,
    BASE_MAP_MODE_STACK = 1
} base_map_mode_t;

typedef enum base_map_section {
    BASE_MAP_SECTION_TEXT = 0,
    BASE_MAP_SECTION_DATA = 1,
    BASE_MAP_SECTION_BSS = 2,
    BASE_MAP_SECTION_COUNT = 3
} base_map_section_t;

#define BASE_MAP_INVALID_SIZE 0xffffffffu

void
base_map_reset(void);

base_map_mode_t
base_map_getMode(void);

int
base_map_sectionFromIndex(uint32_t index, base_map_section_t *outSection);

void
base_map_setBasicBase(base_map_section_t section, uint32_t base);

uint32_t
base_map_getBasicBase(base_map_section_t section);

void
base_map_setBasicBases(uint32_t textBase, uint32_t dataBase, uint32_t bssBase);

void
base_map_getBasicBases(uint32_t *outTextBase, uint32_t *outDataBase, uint32_t *outBssBase);

int
base_map_push(base_map_section_t section, uint32_t base, uint32_t size);

size_t
base_map_getStackCount(void);

int
base_map_getStackEntry(size_t index, base_map_section_t *outSection, uint32_t *outBase, uint32_t *outSize);

int
base_map_runtimeToDebug(base_map_section_t section, uint32_t runtimeAddr, uint32_t *outDebugAddr);

int
base_map_runtimeToDebugWithIndex(base_map_section_t section, uint32_t runtimeAddr,
                                 uint32_t *outDebugAddr, size_t *outIndex);

int
base_map_debugToRuntime(base_map_section_t section, uint32_t debugAddr, uint32_t *outRuntimeAddr);

int
base_map_debugToRuntimeWithIndex(size_t index, uint32_t debugAddr, uint32_t *outRuntimeAddr);

int
base_map_symbolToRuntime(const char *sectionName, uint32_t symAddr, uint32_t *outRuntimeAddr);

int
base_map_symbolToRuntimeHunk(const char *sectionName, uint32_t symAddr, uint32_t *outRuntimeAddr);
