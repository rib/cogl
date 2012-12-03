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

#ifndef __COGL_SWAP_INFO_PRIVATE_H
#define __COGL_SWAP_INFO_PRIVATE_H

#include "cogl-swap-info.h"
#include "cogl-object-private.h"

struct _CoglSwapInfo
{
  CoglObject _parent;

  int64_t frame_counter;
  int64_t frame_time;
  int64_t presentation_time;
  int64_t refresh_interval;

  unsigned int complete : 1;
};

CoglSwapInfo *_cogl_swap_info_new (void);

#endif /* __COGL_SWAP_INFO_PRIVATE_H */
