/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Authors:
 *   Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 */

#ifndef _COGL_TEXTURE_RECTANGLE_NOP_PRIVATE_H_
#define _COGL_TEXTURE_RECTANGLE_NOP_PRIVATE_H_

#include "cogl-types.h"
#include "cogl-context-private.h"
#include "cogl-texture.h"
#include "cogl-texture-rectangle.h"

void
_cogl_texture_rectangle_nop_free (CoglTextureRectangle *tex_rect);

void
_cogl_texture_rectangle_nop_init (CoglTextureRectangle *tex_rect);

CoglBool
_cogl_texture_rectangle_nop_allocate (CoglTexture *tex,
                                      CoglError **error);

unsigned int
_cogl_texture_rectangle_nop_get_gl_handle (CoglTextureRectangle *tex_rect);

GLenum
_cogl_texture_rectangle_nop_get_gl_format (CoglTextureRectangle *tex_rect);

void
_cogl_texture_rectangle_nop_get_data (CoglTextureRectangle *tex_rect,
                                      CoglPixelFormat format,
                                      int rowstride,
                                      uint8_t *data);

#endif /* _COGL_TEXTURE_RECTANGLE_NOP_PRIVATE_H_ */
