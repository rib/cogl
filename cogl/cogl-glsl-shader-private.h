/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#ifndef _COGL_GLSL_SHADER_PRIVATE_H_
#define _COGL_GLSL_SHADER_PRIVATE_H_

typedef enum {
  COGL_GLSL_SHADER_TYPE_VERTEX,
  COGL_GLSL_SHADER_TYPE_FRAGMENT
} CoglGlslShaderType;

GString *
_cogl_glsl_shader_get_source_with_boilerplate (CoglContext *ctx,
                                               CoglGlslShaderType shader_type,
                                               CoglPipeline *pipeline,
                                               int count_in,
                                               const char **strings_in,
                                               const int *lengths_in);

GString *
_cogl_glsl_vulkan_shader_get_source_with_boilerplate (CoglContext *ctx,
                                                      CoglGlslShaderType shader_type,
                                                      CoglPipeline *pipeline,
                                                      GString *block,
                                                      GString *global,
                                                      GString *source);

#endif /* _COGL_GLSL_SHADER_PRIVATE_H_ */
