/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#pragma once

#include "e9ui_component.h"

e9ui_component_t *
e9ui_slider_make(const char *label,
                 int labelWidthPx,
                 int gapPx,
                 int rowPaddingPx,
                 int barHeightPx,
                 int rightMarginPx,
                 e9ui_component_t **outBar);
