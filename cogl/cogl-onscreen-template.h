/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ONSCREEN_TEMPLATE_H__
#define __COGL_ONSCREEN_TEMPLATE_H__

#include <cogl/cogl-swap-chain.h>

G_BEGIN_DECLS

typedef struct _CoglOnscreenTemplate	      CoglOnscreenTemplate;

#define COGL_ONSCREEN_TEMPLATE(OBJECT) ((CoglOnscreenTemplate *)OBJECT)

#define cogl_onscreen_template_new cogl_onscreen_template_new_EXP
CoglOnscreenTemplate *
cogl_onscreen_template_new (CoglSwapChain *swap_chain);

/**
 * cogl_onscreen_template_set_point_samples_per_pixel:
 * @onscreen: A #CoglOnscreenTemplate template framebuffer
 * @n: The minimum number of samples per pixel
 *
 * Requires that any future CoglOnscreen framebuffers derived from
 * this template must support making @n point samples per pixel which
 * will all contribute to the final resolved color for that pixel.
 *
 * By default sampling is not based on point samples but rather by
 * considering the whole rectangular area of the current pixel, so an
 * @n value of %1 is not equivalent to the default behaviour. A value
 * of %0 can be used to explicitly request non point based sampling.
 */
void
cogl_onscreen_template_set_point_samples_per_pixel (
                                          CoglOnscreenTemplate *onscreen_template,
                                          int n);

G_END_DECLS

#endif /* __COGL_ONSCREEN_TEMPLATE_H__ */
