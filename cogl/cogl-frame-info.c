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

#include "cogl-frame-info-private.h"

static void _cogl_frame_info_free (CoglFrameInfo *info);

COGL_OBJECT_DEFINE (FrameInfo, frame_info);

CoglFrameInfo *
_cogl_frame_info_new (void)
{
  CoglFrameInfo *info;

  info = g_slice_new0 (CoglFrameInfo);

  return _cogl_frame_info_object_new (info);
}

static void
_cogl_frame_info_free (CoglFrameInfo *info)
{
  g_slice_free (CoglFrameInfo, info);
}

CoglBool
cogl_frame_info_get_complete (CoglFrameInfo *info)
{
  return info->complete;
}

int64_t
cogl_frame_info_get_frame_counter (CoglFrameInfo *info)
{
  return info->frame_counter;
}

int64_t
cogl_frame_info_get_frame_time (CoglFrameInfo *info)
{
  return info->frame_time;
}

int64_t
cogl_frame_info_get_presentation_time (CoglFrameInfo *info)
{
  return info->presentation_time;
}

int64_t
cogl_frame_info_get_refresh_interval (CoglFrameInfo *info)
{
  return info->refresh_interval;
}
