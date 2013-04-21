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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_OUTPUT_PRIVATE_H
#define __COGL_OUTPUT_PRIVATE_H

#include <glib.h>

#include "cogl-output.h"
#include "cogl-object-private.h"

#warning "TODO: remove if not used in the end"
typedef enum _CoglOutputChange
{
  COGL_OUTPUT_CHANGE_OVERLAYS = 1<<0,
  COGL_OUTPUT_CHANGE_MODE = 1<<1,
} CoglOutputChange;

typedef struct _CoglOutputState
{
  char *name;

  GList *overlays;

  CoglMode *mode;

  /* x must be first field for _cogl_output_state_equal()
   * and all following members should be comparable using
   * memcmp() */
  int x;
  int y;
  int mm_width;
  int mm_height;
  CoglSubpixelOrder subpixel_order;

  CoglDpmsMode dpms_mode;

#warning "TODO: remove if not used in the end"
  CoglOutputChange changes;

} CoglOutputState;

struct _CoglOutput
{
  CoglObject _parent;

  GList *modes;

  CoglOutputState *pending;
  CoglOutputState *state;

  void *winsys;
  CoglUserDataDestroyCallback winsys_destroy_callback;
};

CoglOutput *
_cogl_output_new (const char *name);

void
_cogl_output_set_winsys_data (CoglOutput *output,
                              void *winsys,
                              CoglUserDataDestroyCallback destroy_callback);

void
_cogl_output_update_state (CoglOutput *output);

CoglBool
_cogl_output_equal (CoglOutput *output,
                    CoglOutput *other);

#endif /* __COGL_OUTPUT_PRIVATE_H */
