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
 */

#ifndef _COGL_UDEV_DRM_PRIVATE_H_
#define _COGL_UDEV_DRM_PRIVATE_H_

typedef struct _CoglUdevDrmDevice CoglUdevDrmDevice;

CoglUdevDrmDevice *
cogl_udev_drm_device_open (CoglRenderer *renderer,
                           CoglError **error);

void
cogl_udev_drm_device_destroy (CoglUdevDrmDevice *udev_drm_device);

int
cogl_udev_drm_device_get_fd (CoglUdevDrmDevice *udev_drm_device);

typedef void (*CoglUdevDrmHotplugCallback) (void *user_data);

void
cogl_udev_drm_device_set_hotplug_callback (CoglUdevDrmDevice *udev_drm_device,
                                           CoglUdevDrmHotplugCallback callback,
                                           void *user_data);

#endif /* _COGL_UDEV_DRM_PRIVATE_H_ */
