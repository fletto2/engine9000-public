/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include <stddef.h>

void
strutil_strlcpy(char *dst, size_t dstCap, const char *src);

void
strutil_join2Trunc(char *out, size_t outCap, const char *a, const char *b);

void
strutil_join3Trunc(char *out, size_t outCap, const char *a, const char *b, const char *c);

void
strutil_pathJoinTrunc(char *out, size_t outCap, const char *dir, const char *leaf);

