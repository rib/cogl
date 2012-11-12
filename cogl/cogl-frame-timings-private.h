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

#ifndef __COGL_FRAME_TIMINGS_PRIVATE_H
#define __COGL_FRAME_TIMINGS_PRIVATE_H

#include "cogl-frame-timings.h"
#include "cogl-object-private.h"

struct _CoglFrameTimings
{
  CoglObject _parent;

  int64_t frame_counter;
  int64_t frame_time;
  int64_t presentation_time;
  int64_t refresh_interval;

  guint complete : 1;
};

CoglFrameTimings *_cogl_frame_timings_new (void);

#endif /* __COGL_FRAME_TIMINGS_PRIVATE_H */
