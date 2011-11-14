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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef _COGL_VIRTUAL_FRAMEBUFFER_H_
#define _COGL_VIRTUAL_FRAMEBUFFER_H_

#include <cogl/cogl-context.h>
#include <cogl/cogl-framebuffer.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _CoglVirtualFramebuffer CoglVirtualFramebuffer;
#define COGL_VIRTUAL_FRAMEBUFFER(X) ((CoglVirtualFramebuffer *)(X))

#define cogl_is_virtual_framebuffer cogl_is_virtual_framebuffer_EXP
/**
 * cogl_is_virtual_framebuffer:
 * @object: A pointer to a #CoglObject
 *
 * Gets whether the given object implements the virtual framebuffer
 * interface.
 *
 * Return value: %TRUE if object implements the
 * #CoglVirtualFramebuffer interface %FALSE otherwise.
 *
 * Since: 1.10
 * Stability: Unstable
 */
gboolean
cogl_is_virtual_framebuffer (void *object);

CoglVirtualFramebuffer *
cogl_virtual_framebuffer_new (CoglContext *context,
                              int width,
                              int height);

void
cogl_virtual_framebuffer_add_slice (CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice,
                                    int sample_x,
                                    int sample_y,
                                    int sample_width,
                                    int sample_height,
                                    int virtual_x,
                                    int virtual_y);
void
cogl_virtual_framebuffer_remove_slice (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice);
void
cogl_virtual_framebuffer_move_slice (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice,
                                    int virtual_x,
                                    int virtual_y);
void
cogl_virtual_framebuffer_set_slice_sample_region (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice,
                                    int x,
                                    int y,
                                    int width,
                                    int height);
int
cogl_virtual_framebuffer_get_slice_sample_x (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice);
int
cogl_virtual_framebuffer_get_slice_sample_y (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice);

int
cogl_virtual_framebuffer_get_slice_sample_width (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice);

int
cogl_virtual_framebuffer_get_slice_sample_height (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice);

void
cogl_virtual_framebuffer_set_size (CoglVirtualFramebuffer *virtual_framebuffer,
                                   int width,
                                   int height);

G_END_DECLS

#endif /* _COGL_VIRTUAL_FRAMEBUFFER_H_ */
