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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

/* CoglLooseRegion is a data structure that is a cross between a
   simple bounding box and a region. Instead of keeping all of the
   rectangles necessary to track an exact region, it only keeps around
   a few bounding boxes. This can be checked for intersection much
   more quickly than a complicated region but it will have some false
   positives. */

#ifndef __COGL_LOOSE_REGION_H__
#define __COGL_LOOSE_REGION_H__

#include <glib.h>

#define COGL_LOOSE_REGION_N_RECTANGLES 4

typedef struct _CoglLooseRegionRectangle
{
  float x_1, y_1;
  float x_2, y_2;
} CoglLooseRegionRectangle;

typedef struct _CoglLooseRegion
{
  int n_rects;
  CoglLooseRegionRectangle rects[COGL_LOOSE_REGION_N_RECTANGLES];
} CoglLooseRegion;

void
_cogl_loose_region_init (CoglLooseRegion *region);

void
_cogl_loose_region_add_rectangle (CoglLooseRegion *region,
                                  const CoglLooseRegionRectangle *rect);

gboolean
_cogl_loose_region_intersects (const CoglLooseRegion *region,
                               const CoglLooseRegionRectangle *rect);

#endif /* __COGL_LOOSE_REGION_H__ */
