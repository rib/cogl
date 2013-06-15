/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright 2001 VA Linux Systems Inc., Fremont, California.
Copyright © 2002 by David Dawes
Copyright © 2010 Intel Corporation

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/


#include <config.h>

#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-private.h"
#include "cogl-error-private.h"
#include "cogl-poll-private.h"

#include "cogl-udev-private.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <xf86drm.h>

#ifdef COGL_HAS_X11_SUPPORT
#include <xcb/xcb.h>
#include <xcb/dri2.h>
#endif

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>

typedef struct _IntelChipset
{
  uint32_t chip_id;
  const struct intel_device_info *info;
  const char *name;
} IntelChipset;

struct _CoglUdevDrmDevice
{
  int fd;

  struct udev *udev;
  struct udev_monitor *udev_monitor;

  CoglUdevDrmHotplugCallback hotplug_callback;
  void *hotplug_user_data;
};

void
cogl_udev_drm_device_destroy (CoglUdevDrmDevice *udev_drm_device)
{
  udev_monitor_unref (udev_drm_device->udev_monitor);
  udev_unref (udev_drm_device->udev);

  close (udev_drm_device->fd);

  g_slice_free (CoglUdevDrmDevice, udev_drm_device);
}

static void
handle_udev_monitor_event (void *user_data, int revents)
{
  CoglUdevDrmDevice *udev_drm_device = user_data;
  struct udev_device *dev;
  dev_t devnum;
  struct stat st;
  const char *hotplug;

  if (!udev_drm_device->hotplug_callback)
    return;

  dev = udev_monitor_receive_device (udev_drm_device->udev_monitor);
  if (!dev)
    return;

  devnum = udev_device_get_devnum (dev);
  if (fstat (udev_drm_device->fd, &st) ||
      memcmp (&st.st_rdev, &devnum, sizeof (dev_t)) != 0)
    {
      udev_device_unref (dev);
      return;
    }

  hotplug = udev_device_get_property_value (dev, "HOTPLUG");
  if (hotplug && atoi (hotplug) == 1)
    udev_drm_device->hotplug_callback (udev_drm_device->hotplug_user_data);

  udev_device_unref (dev);
}

static CoglBool
authenticate_drm_fd (CoglRenderer *render,
                     int fd,
                     CoglError **error)
{
#ifdef COGL_HAS_X11_SUPPORT
  xcb_connection_t *connection;
#endif
  uint32_t magic;
  CoglBool ret = FALSE;

  if (drmGetMagic (fd, &magic) < 0)
    {
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to get DRM magic number for authentication");
      return FALSE;
    }

  /* First try and self authenticate */
  if (drmAuthMagic (fd, magic) == 0)
    return TRUE;

  /* If that fails then we next try and authenticate via an X server */
#ifdef COGL_HAS_X11_SUPPORT
  connection = xcb_connect (NULL, NULL);
  if (!xcb_connection_has_error (connection))
    {
      xcb_screen_t *screen =
        xcb_setup_roots_iterator (xcb_get_setup (connection)).data;
      xcb_dri2_authenticate_cookie_t authenticate_cookie =
        xcb_dri2_authenticate (connection, screen->root, magic);
      xcb_dri2_authenticate_reply_t *authenticate =
        xcb_dri2_authenticate_reply (connection, authenticate_cookie, 0);

      if (authenticate && authenticate->authenticated)
        ret = TRUE;

      free (authenticate);
    }
  xcb_disconnect (connection);
#endif

  /* TODO: If that fails then try authenticating via a Wayland server */

  if (!ret)
    {
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to authenticate DRM device with magic number");
    }

  return ret;
}

CoglUdevDrmDevice *
cogl_udev_drm_device_open (CoglRenderer *renderer,
                           CoglError **error)
{
  struct udev *udev;
  struct udev_enumerate *e;
  struct udev_list_entry *entry;
  int fd = -1;
  struct udev_monitor *monitor;
  CoglUdevDrmDevice *udev_drm_device;

  udev = udev_new ();
  if (udev == NULL)
    {
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to init udev api");
      return FALSE;
    }

  e = udev_enumerate_new (udev);
  udev_enumerate_add_match_subsystem (e, "drm");
  udev_enumerate_scan_devices (e);
  udev_list_entry_foreach (entry, udev_enumerate_get_list_entry (e))
    {
      struct udev_device *udev_device =
        udev_device_new_from_syspath (udev, udev_list_entry_get_name (entry));

      /* TODO: probe device for more detailed information */

      const char *path = udev_device_get_devnode (udev_device);
      if (path)
        {
          fd = open (path, O_RDWR);
          if (fd != -1)
            {
              udev_device_unref (udev_device);
              break;
            }
          else
            g_warning ("Failed to open device node %s: %m", path);
        }
      else
        g_warning ("udev device with no corresponding devnode");

      udev_device_unref (udev_device);
    }

  udev_enumerate_unref (e);

  if (fd == -1)
    {
      udev_unref (udev);
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to find suitable drm device");
      return FALSE;
    }

  if (!authenticate_drm_fd (renderer, fd, error))
    return FALSE;

  monitor = udev_monitor_new_from_netlink (udev, "udev");
  if (!monitor)
    {
      close (fd);
      udev_unref (udev);
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to create udev monitor");
      return FALSE;
    }

  if (udev_monitor_filter_add_match_subsystem_devtype (monitor,
                                                       "drm",
                                                       "drm_minor") < 0 ||
      udev_monitor_enable_receiving (monitor) < 0)
    {
      udev_monitor_unref (monitor);
      close (fd);
      udev_unref (udev);
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to enable udev monitor");
      return FALSE;
    }

  /* XXX: Note that the udev monitor has an internal pointer to udev
   * but udev_monitor_new_from_netlink doesn't take a references on
   * the udev object so we don't release our udev reference */

  udev_drm_device = g_slice_new0 (CoglUdevDrmDevice);

  udev_drm_device->fd = fd;
  udev_drm_device->udev = udev;
  udev_drm_device->udev_monitor = monitor;

  _cogl_poll_renderer_add_fd (renderer,
                              udev_monitor_get_fd (monitor),
                              COGL_POLL_FD_EVENT_IN,
                              NULL, /* no prepare function */
                              handle_udev_monitor_event,
                              udev_drm_device);

  return udev_drm_device;
}

int
cogl_udev_drm_device_get_fd (CoglUdevDrmDevice *udev_drm_device)
{
  return udev_drm_device->fd;
}

void
cogl_udev_drm_device_set_hotplug_callback (CoglUdevDrmDevice *udev_drm_device,
                                           CoglUdevDrmHotplugCallback callback,
                                           void *user_data)
{
  udev_drm_device->hotplug_callback = callback;
  udev_drm_device->hotplug_user_data = user_data;
}
