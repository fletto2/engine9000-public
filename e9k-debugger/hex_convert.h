/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include <stdio.h>

#include "e9ui.h"

int
hex_convert_isOpen(void);

void
hex_convert_close(void);

void
hex_convert_toggle(e9ui_context_t *ctx);

void
hex_convert_persistConfig(FILE *file);

int
hex_convert_loadConfigProperty(const char *prop, const char *value);
