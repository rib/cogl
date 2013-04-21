/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2013 Intel Corporation.
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
 */

#include <config.h>

#include "cogl-mode-private.h"
#include "cogl-object-private.h"

static void _cogl_mode_free (CoglMode *mode);

COGL_OBJECT_DEFINE (Mode, mode);

static void
_cogl_mode_free (CoglMode *mode)
{
  g_free (mode->name);
  g_slice_free (CoglMode, mode);
}

CoglMode *
_cogl_mode_new (const char *name)
{
  CoglMode *mode = g_slice_new0 (CoglMode);

  mode->name = g_strdup (name);

  return _cogl_mode_object_new (mode);
}

const char *
cogl_mode_get_name (CoglMode *mode)
{
  return mode->name;
}

float
cogl_mode_get_refresh_rate (CoglMode *mode)
{
  return mode->refresh_rate;
}

int
cogl_mode_get_width (CoglMode *mode)
{
  return mode->width;
}

int
cogl_mode_get_height (CoglMode *mode)
{
  return mode->height;
}
