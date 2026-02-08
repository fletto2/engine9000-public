/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include "gl_composite.h"

int
gl_composite_init(SDL_Window *window, SDL_Renderer *renderer)
{
    (void)window;
    (void)renderer;
    return 0;
}

void
gl_composite_shutdown(void)
{
}

int
gl_composite_isActive(void)
{
    return 0;
}

void
gl_composite_renderFrame(SDL_Renderer *renderer, const uint8_t *data, int width, int height,
                         size_t pitch, const SDL_Rect *dst)
{
    (void)renderer;
    (void)data;
    (void)width;
    (void)height;
    (void)pitch;
    (void)dst;
}

int
gl_composite_captureToRenderer(SDL_Renderer *renderer, const uint8_t *data, int width, int height,
                               size_t pitch, const SDL_Rect *dst)
{
    (void)renderer;
    (void)data;
    (void)width;
    (void)height;
    (void)pitch;
    (void)dst;
    return 0;
}

int
gl_composite_isCrtShaderAdvanced(void)
{
    return 0;
}

int
gl_composite_toggleCrtShaderAdvanced(void)
{
    return 0;
}
