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

#ifndef _COGL_MODE_PRIVATE_H_
#define _COGL_MODE_PRIVATE_H_

#include "cogl-mode.h"
#include "cogl-object-private.h"

struct _CoglMode
{
  CoglObjectClass _parent;

  char *name;

  /* NB: _cogl_mode_equal() expects everything from the width member
   * to the end of the struct to be comparable using memcmp().
   */
  int width;
  int height;

  float refresh_rate;

#if 0
  uint32_t clock;
  uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
  uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

  uint32_t flags;
  uint32_t type;
#endif
};

CoglMode *
_cogl_mode_new (const char *name);

#endif /* _COGL_MODE_PRIVATE_H_ */
