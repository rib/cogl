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

#ifndef _COGL_TEXTURE_3D_GL_PRIVATE_H_
#define _COGL_TEXTURE_3D_GL_PRIVATE_H_

#include "cogl-types.h"
#include "cogl-context-private.h"
#include "cogl-texture.h"
#include "cogl-texture-3d.h"

void
_cogl_texture_3d_gl_free (CoglTexture3D *tex_3d);

void
_cogl_texture_3d_gl_init (CoglTexture3D *tex_3d);

CoglBool
_cogl_texture_3d_gl_allocate (CoglTexture *tex,
                              CoglError **error);

GLenum
_cogl_texture_3d_gl_get_gl_format (CoglTexture3D *tex_3d);

unsigned int
_cogl_texture_3d_gl_get_gl_handle (CoglTexture3D *tex_3d);

void
_cogl_texture_3d_gl_generate_mipmap (CoglTexture3D *tex_3d);

void
_cogl_texture_3d_gl_flush_legacy_texobj_filters (CoglTexture3D *tex_3d,
                                                 GLenum min_filter,
                                                 GLenum mag_filter);

void
_cogl_texture_3d_gl_flush_legacy_texobj_wrap_modes (CoglTexture3D *tex_3d,
                                                    GLenum wrap_mode_s,
                                                    GLenum wrap_mode_t,
                                                    GLenum wrap_mode_p);

#endif /* _COGL_TEXTURE_3D_GL_PRIVATE_H_ */
