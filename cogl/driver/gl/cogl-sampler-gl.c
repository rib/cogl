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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-sampler-gl-private.h"
#include "cogl-util-gl-private.h"

static void
set_wrap_mode (CoglContext *context,
               GLuint sampler_object,
               GLenum param,
               CoglSamplerCacheWrapMode wrap_mode)
{
  GE( context, glSamplerParameteri (sampler_object,
                                    param,
                                    wrap_mode) );
}

void
_cogl_sampler_gl_create (CoglContext *context, CoglSamplerCacheEntry *entry)
{
  GLuint gl_sampler_object;

  GE( context, glGenSamplers (1, &gl_sampler_object) );

  GE( context, glSamplerParameteri (gl_sampler_object,
                                    GL_TEXTURE_MIN_FILTER,
                                    entry->min_filter) );
  GE( context, glSamplerParameteri (gl_sampler_object,
                                    GL_TEXTURE_MAG_FILTER,
                                    entry->mag_filter) );

  set_wrap_mode (context, gl_sampler_object,
                 GL_TEXTURE_WRAP_S, entry->wrap_mode_s);
  set_wrap_mode (context, gl_sampler_object,
                 GL_TEXTURE_WRAP_T, entry->wrap_mode_t);
  set_wrap_mode (context, gl_sampler_object,
                 GL_TEXTURE_WRAP_R, entry->wrap_mode_p);

  entry->winsys = GUINT_TO_POINTER (gl_sampler_object);
}

void
_cogl_sampler_gl_destroy (CoglContext *context, CoglSamplerCacheEntry *entry)
{
  GLuint gl_sampler_object = GPOINTER_TO_UINT (entry->winsys);

  GE( context, glDeleteSamplers (1, &gl_sampler_object) );
}
