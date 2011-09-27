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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "cogl-loose-region.h"

/* This specifies the fraction of the size of the new rectangle that
   will permit a bounding box to grow by before we consider starting a
   new bounding box. Eg, 1.1 means we'll allow the bounding box to
   grow by the size of the rectangle + 10% waste */
#define SIZE_INCREASE_THRESHOLD 2.0f

void
_cogl_loose_region_init (CoglLooseRegion *region)
{
  region->n_rects = 0;
}

static float
_cogl_loose_region_get_size (const CoglLooseRegionRectangle *rect)
{
  float width = rect->x_2 - rect->x_1;
  float height = rect->y_2 - rect->y_1;

  return width * height;
}

static void
_cogl_loose_region_union_rectangles (const CoglLooseRegionRectangle *a,
                                     const CoglLooseRegionRectangle *b,
                                     CoglLooseRegionRectangle *out)
{
  out->x_1 = MIN (a->x_1, b->x_1);
  out->y_1 = MIN (a->y_1, b->y_1);
  out->x_2 = MAX (a->x_2, b->x_2);
  out->y_2 = MAX (a->y_2, b->y_2);
}

void
_cogl_loose_region_add_rectangle (CoglLooseRegion *region,
                                  const CoglLooseRegionRectangle *rect)
{
  int best_index = -1;
  float best_increase = G_MAXFLOAT;
  float rect_size;
  int i;

  rect_size = _cogl_loose_region_get_size (rect);

  /* Look for an already existing bounding box that we can expand to
     fit with the maximum threshold of the size increase */
  for (i = 0; i < region->n_rects; i++)
    {
      CoglLooseRegionRectangle new_rectangle;
      float size_increase;

      _cogl_loose_region_union_rectangles (region->rects + i,
                                           rect,
                                           &new_rectangle);

      size_increase = (_cogl_loose_region_get_size (&new_rectangle) -
                       _cogl_loose_region_get_size (region->rects + i));

      if (size_increase <= SIZE_INCREASE_THRESHOLD * rect_size)
        {
          /* We've found an acceptable bounding box so we'll stop
             looking */
          region->rects[i] = new_rectangle;
          return;
        }

      if (size_increase < best_increase)
        {
          best_index = i;
          best_increase = size_increase;
        }
    }

  /* If we make it here then we didn't find a suitable existing
     bounding box. If we can make a new one then we will, otherwise
     we'll just add it to the best box */
  if (region->n_rects < COGL_LOOSE_REGION_N_RECTANGLES)
    region->rects[region->n_rects++] = *rect;
  else
    {
      g_assert (best_index >= 0 && best_index < COGL_LOOSE_REGION_N_RECTANGLES);
      _cogl_loose_region_union_rectangles (region->rects + best_index,
                                           rect,
                                           region->rects + best_index);
    }
}

gboolean
_cogl_loose_region_intersects (const CoglLooseRegion *region,
                               const CoglLooseRegionRectangle *rect)
{
  int i;

  for (i = 0; i < region->n_rects; i++)
    {
      const CoglLooseRegionRectangle *r_rect = region->rects + i;

      if (rect->x_2 > r_rect->x_1 &&
          rect->x_1 < r_rect->x_2 &&
          rect->y_2 > r_rect->y_1 &&
          rect->y_1 < r_rect->y_2)
        return TRUE;
    }

  return FALSE;
}

