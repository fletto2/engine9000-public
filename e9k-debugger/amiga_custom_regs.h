#pragma once

#include <stdint.h>

const char *
amiga_custom_regs_nameForOffset(uint16_t regOffset);

const char *
amiga_custom_regs_descriptionForOffset(uint16_t regOffset);

uint32_t
amiga_custom_regs_addressFromOffset(uint16_t regOffset);

uint32_t
amiga_custom_regs_colorForOffset(uint16_t regOffset);

const char *
amiga_custom_regs_valueTooltipForName(const char *regName, uint16_t value);

const char *
amiga_custom_regs_valueTooltipForOffset(uint16_t regOffset, uint16_t value);
