/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 *
 * Authors:
 *   Owen Taylor <otaylor@redhat.com>
 */
#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_OUTPUT_H
#define __COGL_OUTPUT_H

#include <cogl/cogl-types.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _CoglOutput CoglOutput;
#define COGL_OUTPUT(X) ((CoglOutput *)(X))

typedef enum {
  COGL_SUBPIXEL_ORDER_UNKNOWN,
  COGL_SUBPIXEL_ORDER_NONE,
  COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB,
  COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR,
  COGL_SUBPIXEL_ORDER_VERTICAL_RGB,
  COGL_SUBPIXEL_ORDER_VERTICAL_BGR
} CoglSubpixelOrder;

/**
 * cogl_is_output:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglOutput.
 *
 * Return value: %TRUE if the object references a #CoglOutput
 *   and %FALSE otherwise.
 * Since: 2.0
 * Stability: unstable
 */
CoglBool
cogl_is_output (void *object);

int
cogl_output_get_x (CoglOutput *output);
int
cogl_output_get_y (CoglOutput *output);
int
cogl_output_get_width (CoglOutput *output);
int
cogl_output_get_height (CoglOutput *output);
int
cogl_output_get_mm_width (CoglOutput *output);
int
cogl_output_get_mm_height (CoglOutput *output);
CoglSubpixelOrder
cogl_output_get_subpixel_order (CoglOutput *output);
float
cogl_output_get_refresh_rate (CoglOutput *output);

G_END_DECLS

#endif /* __COGL_OUTPUT_H */



