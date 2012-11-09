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
 *
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-private.h"
#include "cogl-winsys-drm-private.h"
#include "cogl-error-private.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>

static int _cogl_winsys_drm_dummy_ptr;

typedef struct _CoglRendererDRM
{
  dev_t devnum;
  int vendor_id;
  int chip_id;
  int fd;
} CoglRendererDRM;

typedef struct _CoglDisplayDRM
{
  int padding;
} CoglDisplayDRM;

/* This provides a NOP winsys. This can be useful for debugging or for
 * integrating with toolkits that already have window system
 * integration code.
 */

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name,
                                        CoglBool in_core)
{
  static GModule *module = NULL;

  /* this should find the right function if the program is linked against a
   * library providing it */
  if (G_UNLIKELY (module == NULL))
    module = g_module_open (NULL, 0);

  if (module)
    {
      void *symbol;

      if (g_module_symbol (module, name, &symbol))
        return symbol;
    }

  return NULL;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererDRM *drm_renderer = renderer->winsys;

  close (drm_renderer->fd);

  g_slice_free (CoglRendererDRM, drm_renderer);

  renderer->winsys = NULL;
}

static const char *
get_udev_property (struct udev_device *device,
                   const char *name)
{
  struct udev_list_entry *entry;

  udev_list_entry_foreach (entry,
                           udev_device_get_properties_list_entry (device))
    {
      if (strcmp (udev_list_entry_get_name (entry), name) == 0)
        return udev_list_entry_get_value (entry);
    }

  return NULL;
}

static char *
match_device (struct udev_device *device,
              dev_t *devnum,
              uint32_t *vendor_id,
              uint32_t *chip_id)
{
  static const struct _Device {
      uint32_t vendor_id;
      uint32_t chip_id;
  } devices[] = {
        { 0x8086, 0x29a2 }, /* I965 G */
        { 0x8086, 0x2982 }, /* G35 G */
        { 0x8086, 0x2992 }, /* I965 Q */
        { 0x8086, 0x2972 }, /* I946 GZ */
        { 0x8086, 0x2a02 }, /* I965 GM */
        { 0x8086, 0x2a12 }, /* I965 GME */
        { 0x8086, 0x2e02 }, /* IGD E G */
        { 0x8086, 0x2e22 }, /* G45 G */
        { 0x8086, 0x2e12 }, /* Q45 G */
        { 0x8086, 0x2e32 }, /* G41 G */
        { 0x8086, 0x2a42 }, /* GM45 GM */

        { 0x8086, 0x2582 }, /* I915 G */
        { 0x8086, 0x2592 }, /* I915 GM */
        { 0x8086, 0x258a }, /* E7221 G */
        { 0x8086, 0x2772 }, /* I945 G */
        { 0x8086, 0x27a2 }, /* I945 GM */
        { 0x8086, 0x27ae }, /* I945 GME */
        { 0x8086, 0x29c2 }, /* G33 G */
        { 0x8086, 0x29b2 }, /* Q35 G */
        { 0x8086, 0x29d2 }, /* Q33 G */
        { 0x8086, 0xa011 }, /* IGD GM */
        { 0x8086, 0xa001 }, /* IGD G */

        /* XXX i830 */

        { 0x8086, ~0 }, /* intel */
  };

  struct udev_device *parent;
  const char *pci_id;
  const char *path;
  int i;

  *devnum = udev_device_get_devnum (device);

  parent = udev_device_get_parent (device);
  pci_id = get_udev_property (parent, "PCI_ID");
  if (pci_id == NULL || sscanf (pci_id, "%x:%x", vendor_id, chip_id) != 2)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (devices); i++)
    {
      if (devices[i].vendor_id == *vendor_id &&
          (devices[i].chip_id == ~0U || devices[i].chip_id == *chip_id))
        break;
    }

  if (i == G_N_ELEMENTS (devices))
    return NULL;

  path = udev_device_get_devnode (device);
  if (path == NULL)
    path = "/dev/dri/card0"; /* XXX buggy udev? */

  return g_strdup (path);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  struct udev *udev;
  struct udev_enumerate *e;
  struct udev_list_entry *entry;
  dev_t devnum;
  int vendor_id;
  int chip_id;
  int fd = -1;
  CoglRendererDRM *drm_renderer;

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
      struct udev_device *device =
        udev_device_new_from_syspath (udev, udev_list_entry_get_name (entry));
      char *path = match_device (device, &devnum, &vendor_id, &chip_id);

      if (path)
        {
          g_print ("Matched device: %s\n", path);
          fd = open (path, O_RDWR);
          if (fd == -1)
            {
              g_warning ("Failed to open device node %s: %m", path);
              continue;
            }

          break;
        }

      //g_print ("device %s\n", udev_list_entry_get_name (entry));
      udev_device_unref (device);
    }

  udev_enumerate_unref (e);
  udev_unref (udev);

  if (fd == -1)
    return FALSE;

  drm_renderer = g_slice_new0 (CoglRendererDRM);
  drm_renderer->devnum = denum;
  drm_renderer->vendor_id = vendor_id;
  drm_renderer->chip_id = chip_id;
  drm_renderer->fd = fd;

  renderer->winsys = drm_renderer;

  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  display->winsys = NULL;
}

static CoglBool
_cogl_winsys_display_setup (CoglDisplay *display,
                            CoglError **error)
{
  display->winsys = &_cogl_winsys_drm_dummy_ptr;
  return TRUE;
}

static CoglBool
_cogl_winsys_context_init (CoglContext *context, CoglError **error)
{
  context->winsys = &_cogl_winsys_drm_dummy_ptr;

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  return TRUE;
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  context->winsys = NULL;
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            CoglError **error)
{
  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      CoglBool visibility)
{
}

const CoglWinsysVtable *
_cogl_winsys_drm_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  /* It would be nice if we could use C99 struct initializers here
     like the GLX backend does. However this code is more likely to be
     compiled using Visual Studio which (still!) doesn't support them
     so we initialize it in code instead */

  if (!vtable_inited)
    {
      memset (&vtable, 0, sizeof (vtable));

      vtable.id = COGL_WINSYS_ID_DRM;
      vtable.name = "DRM";
      vtable.renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address;
      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;
      vtable.display_setup = _cogl_winsys_display_setup;
      vtable.display_destroy = _cogl_winsys_display_destroy;
      vtable.context_init = _cogl_winsys_context_init;
      vtable.context_deinit = _cogl_winsys_context_deinit;

      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;
      vtable.onscreen_bind = _cogl_winsys_onscreen_bind;
      vtable.onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers;
      vtable.onscreen_update_swap_throttled =
        _cogl_winsys_onscreen_update_swap_throttled;
      vtable.onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility;

      vtable_inited = TRUE;
    }

  return &vtable;
}
