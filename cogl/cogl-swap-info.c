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

#include "cogl-swap-info-private.h"

static void _cogl_swap_info_free (CoglSwapInfo *info);

COGL_OBJECT_DEFINE (SwapInfo, swap_info);

CoglSwapInfo *
_cogl_swap_info_new (void)
{
  CoglSwapInfo *info;

  info = g_slice_new0 (CoglSwapInfo);

  return _cogl_swap_info_object_new (info);
}

static void
_cogl_swap_info_free (CoglSwapInfo *info)
{
  g_slice_free (CoglSwapInfo, info);
}

CoglBool
cogl_swap_info_get_complete (CoglSwapInfo *info)
{
  return info->complete;
}

int64_t
cogl_swap_info_get_frame_counter (CoglSwapInfo *info)
{
  return info->frame_counter;
}

int64_t
cogl_swap_info_get_frame_time (CoglSwapInfo *info)
{
  return info->frame_time;
}

int64_t
cogl_swap_info_get_presentation_time (CoglSwapInfo *info)
{
  return info->presentation_time;
}

int64_t
cogl_swap_info_get_refresh_interval (CoglSwapInfo *info)
{
  return info->refresh_interval;
}
