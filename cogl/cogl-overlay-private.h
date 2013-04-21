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

#ifndef _COGL_OVERLAY_PRIVATE_H_
#define _COGL_OVERLAY_PRIVATE_H_

#include <cogl/cogl-overlay.h>

typedef enum _CoglOverlayChange
{
  COGL_OVERLAY_CHANGE_SOURCE = 1<<0,
  COGL_OVERLAY_CHANGE_TRANSFORM = 1<<1,
} CoglOverlayChange;

typedef struct _CoglOverlayState
{
  /* NB: to avoid a circular reference we don't keep a reference
   * here... */
  CoglOutput *output;

  /* Note: In the future we might support other sources, such as for
   * video */
  CoglOnscreen *onscreen_source;

  /* XXX: src_x must be the first member for _cogl_overlay_equal() to
   * work and all remaining members should be comparable via
   * memcpy() */

  /* What region of the source should be overlayed? */
  int src_x;
  int src_y;
  int src_width;
  int src_height;

  int dst_x;
  int dst_y;

} CoglOverlayState;

struct _CoglOverlay
{
  CoglObjectClass _parent;

  CoglOverlayState *state;
  CoglOverlayState *pending;
};

void
_cogl_overlay_update_state (CoglOverlay *overlay);

CoglBool
_cogl_overlay_equal (CoglOverlay *overlay0,
                     CoglOverlay *overlay1);

#endif /* _COGL_OVERLAY_PRIVATE_H_ */
