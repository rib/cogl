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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-frame-timings-private.h"

static void _cogl_frame_timings_free (CoglFrameTimings *frame_timings);

COGL_OBJECT_DEFINE (FrameTimings, frame_timings);

CoglFrameTimings *
_cogl_frame_timings_new (void)
{
  CoglFrameTimings *timings;

  timings = g_slice_new0 (CoglFrameTimings);

  return _cogl_frame_timings_object_new (timings);
}

static void
_cogl_frame_timings_free (CoglFrameTimings *timings)
{
  g_slice_free (CoglFrameTimings, timings);
}

CoglBool
cogl_frame_timings_get_complete (CoglFrameTimings *timings)
{
  return timings->complete;
}

gint64
cogl_frame_timings_get_frame_counter (CoglFrameTimings *timings)
{
  return timings->frame_counter;
}

gint64
cogl_frame_timings_get_frame_time (CoglFrameTimings *timings)
{
  return timings->frame_time;
}

gint64
cogl_frame_timings_get_presentation_time (CoglFrameTimings *timings)
{
  return timings->presentation_time;
}

gint64
cogl_frame_timings_get_refresh_interval (CoglFrameTimings *timings)
{
  return timings->refresh_interval;
}
