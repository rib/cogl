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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Rob Bradford <rob@linux.intel.com>
 *   Kristian Høgsberg (from eglkms.c)
 *   Benjamin Franzke (from eglkms.c)
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <glib.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "cogl-winsys-egl-kms-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-kms-renderer.h"
#include "cogl-kms-display.h"
#include "cogl-version.h"
#include "cogl-error-private.h"
#include "cogl-poll-private.h"
#include "cogl-udev-private.h"
#include "cogl-output-private.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

static const CoglWinsysVtable *parent_vtable;

typedef struct _CoglRendererKMS
{
  CoglUdevDrmDevice *udev_drm_device;
  int fd;
  struct gbm_device *gbm;
  CoglClosure *swap_notify_idle;
} CoglRendererKMS;

typedef struct _CoglOutputKMS
{
  uint32_t connector_id;

  drmModeConnector *connector;
  drmModeEncoder *encoder;
  drmModeCrtc *saved_crtc;
  drmModeModeInfo *modes;
  int n_modes;
  drmModeModeInfo mode;
} CoglOutputKMS;

typedef struct _CoglDisplayKMS
{
  GList *outputs;
  GList *crtcs;

  int width, height;
  CoglBool pending_set_crtc;
  struct gbm_surface *dummy_gbm_surface;

  CoglOnscreen *onscreen;
} CoglDisplayKMS;

typedef struct _CoglFlipKMS
{
  CoglOnscreen *onscreen;
  int pending;
} CoglFlipKMS;

typedef struct _CoglOnscreenKMS
{
  struct gbm_surface *surface;
  uint32_t current_fb_id;
  uint32_t next_fb_id;
  struct gbm_bo *current_bo;
  struct gbm_bo *next_bo;
  CoglBool pending_swap_notify;
} CoglOnscreenKMS;

static const char device_name[] = "/dev/dri/card0";

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  if (egl_renderer->edpy)
    eglTerminate (egl_renderer->edpy);

  if (kms_renderer->gbm)
    gbm_device_destroy (kms_renderer->gbm);

  if (kms_renderer->udev_drm_device)
    cogl_udev_drm_device_destroy (kms_renderer->udev_drm_device);

  g_slice_free (CoglRendererKMS, kms_renderer);
  g_slice_free (CoglRendererEGL, egl_renderer);
}

static void
flush_pending_swap_notify_cb (void *data,
                              void *user_data)
{
  CoglFramebuffer *framebuffer = data;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;

      if (kms_onscreen->pending_swap_notify)
        {
          CoglFrameInfo *info = g_queue_pop_head (&onscreen->pending_frame_infos);

          _cogl_onscreen_notify_frame_sync (onscreen, info);
          _cogl_onscreen_notify_complete (onscreen, info);
          kms_onscreen->pending_swap_notify = FALSE;

          cogl_object_unref (info);
        }
    }
}

static void
flush_pending_swap_notify_idle (void *user_data)
{
  CoglContext *context = user_data;
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  /* This needs to be disconnected before invoking the callbacks in
   * case the callbacks cause it to be queued again */
  _cogl_closure_disconnect (kms_renderer->swap_notify_idle);
  kms_renderer->swap_notify_idle = NULL;

  g_list_foreach (context->framebuffers,
                  flush_pending_swap_notify_cb,
                  NULL);
}

static void
free_current_bo (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  if (kms_onscreen->current_fb_id)
    {
      drmModeRmFB (kms_renderer->fd,
                   kms_onscreen->current_fb_id);
      kms_onscreen->current_fb_id = 0;
    }
  if (kms_onscreen->current_bo)
    {
      gbm_surface_release_buffer (kms_onscreen->surface,
                                  kms_onscreen->current_bo);
      kms_onscreen->current_bo = NULL;
    }
}

static void
page_flip_handler (int fd,
                   unsigned int frame,
                   unsigned int sec,
                   unsigned int usec,
                   void *data)
{
  CoglFlipKMS *flip = data;

  /* We're only ready to dispatch a swap notification once all outputs
   * have flipped... */
  flip->pending--;
  if (flip->pending == 0)
    {
      CoglOnscreen *onscreen = flip->onscreen;
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
      CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
      CoglRenderer *renderer = context->display->renderer;
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglRendererKMS *kms_renderer = egl_renderer->platform;

      /* We only want to notify that the swap is complete when the
       * application calls cogl_context_dispatch so instead of
       * immediately notifying we queue an idle callback */
      if (!kms_renderer->swap_notify_idle)
        {
          kms_renderer->swap_notify_idle =
            _cogl_poll_renderer_add_idle (renderer,
                                          flush_pending_swap_notify_idle,
                                          context,
                                          NULL);
        }

      kms_onscreen->pending_swap_notify = TRUE;

      free_current_bo (onscreen);

      kms_onscreen->current_fb_id = kms_onscreen->next_fb_id;
      kms_onscreen->next_fb_id = 0;

      kms_onscreen->current_bo = kms_onscreen->next_bo;
      kms_onscreen->next_bo = NULL;

      cogl_object_unref (flip->onscreen);

      g_slice_free (CoglFlipKMS, flip);
    }
}

static void
handle_drm_event (CoglRendererKMS *kms_renderer)
{
  drmEventContext evctx;

  memset (&evctx, 0, sizeof evctx);
  evctx.version = DRM_EVENT_CONTEXT_VERSION;
  evctx.page_flip_handler = page_flip_handler;
  drmHandleEvent (kms_renderer->fd, &evctx);
}

static void
dispatch_kms_events (void *user_data, int revents)
{
  CoglRenderer *renderer = user_data;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  if (!revents)
    return;

  handle_drm_event (kms_renderer);
}

static int
compare_outputs (CoglOutput *a, CoglOutput *b)
{
  CoglOutputKMS *kms_output_a = a->winsys;
  CoglOutputKMS *kms_output_b = b->winsys;
  return kms_output_a->connector_id - kms_output_b->connector_id;
}

static void
kms_output_destroy_cb (void *user_data)
{
  CoglOutputKMS *kms_output = user_data;

  if (kms_output->saved_crtc)
    drmModeFreeCrtc (kms_output->saved_crtc);

  if (kms_output->encoder)
    drmModeFreeEncoder (kms_output->encoder);

  if (kms_output->connector)
    drmModeFreeConnector (kms_output->connector);

  g_slice_free (CoglOutputKMS, user_data);
}

static const char *kms_connector_types[] = {
    "Unknown",
    "VGA",
    "DVII",
    "DVID",
    "DVIA",
    "Composite",
    "SVIDEO",
    "LVDS",
    "Component",
    "9PinDIN",
    "DisplayPort",
    "HDMIA",
    "HDMIB",
    "TV",
    "eDP"
};

static void
update_outputs (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  int fd = kms_renderer->fd;
  drmModeRes *resources;
  GList *new_outputs = NULL;
  int i;

  resources = drmModeGetResources (fd);
  if (!resources)
    {
      g_warning ("Failed to query KMS resources");
      return;
    }

  for (i = 0; i < resources->count_connectors; i++)
    {
      CoglOutput *output = NULL;
      CoglOutputKMS *kms_output;
      GList *l;

      drmModeConnector *connector =
        drmModeGetConnector (fd, resources->connectors[i]);
      if (!connector)
        {
          g_warning ("Failed to query KMS connector");
          continue;
        }

      /* Connectors not actually connected to a display aren't very
       * interesting */
      if (connector->connection == DRM_MODE_DISCONNECTED ||
          connector->count_modes == 0 ||
          connector->count_encoders == 0)
        {
          drmModeFreeConnector (connector);
          continue;
        }

      /* If we already have a CoglOutput corresponding to this
       * connector id then we keep it... */
      for (l = renderer->outputs; l; l = l->next)
        {
          CoglOutput *existing_output = l->data;
          CoglOutputKMS *existing_kms_output = existing_output->winsys;

          if (existing_kms_output->connector_id == connector->connector_id)
            {
              renderer->outputs = g_list_delete_link (renderer->outputs, l);
              output = existing_output;
              kms_output = output->winsys;

              if (output->pending != output->state)
                {
                  g_warning ("Unexpected pending state associated with CoglOutput "
                             "%s while processing events. Pending output state "
                             "shouldn't be maintained between mainloop "
                             "iterations\n", output->state->name);
                }

              break;
            }
        }

      if (!output)
        {
          const char *type_name;

          if (connector->connector_type < G_N_ELEMENTS (kms_connector_types))
            type_name = kms_connector_types[connector->connector_type];
          else
            type_name = kms_connector_types[0];

          output = _cogl_output_new (type_name);

          kms_output = g_slice_new0 (CoglOutputKMS);
          kms_output->connector_id = connector->connector_id;
          kms_output->connector = connector;

          _cogl_output_set_winsys_data (output,
                                        kms_output,
                                        kms_output_destroy_cb);
        }

      for (j = 0; j < connector->count_modes; j++)
        {
          drmModeModeInfo *info = &connector->modes[j];

          CoglMode *mode = _cogl_mode_new (info->name);
          mode->width = info->hdisplay;
          mode->height = info->vdisplay;
          mode->refresh_rate =
            (info->clock / ((float)info->htotal * info->vtotal));
          new_modes = g_list_prepend (new_modes, mode);
        }

      if (output->modes)
        g_list_free_full (output->modes, cogl_object_unref);
      output->modes = g_list_reverse (new_modes);

      /* We can't determinine anything about the relative position
       * of the outputs... */
      output->state->x = output->state->y = 0;

      output->state->mm_width = connector->mmWidth;
      output->state->mm_height = connector->mmHeight;

      switch (connector->subpixel)
        {
        case DRM_MODE_SUBPIXEL_UNKNOWN:
          output->state->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;
          break;
        case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
          output->state->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB;
          break;
        case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
          output->state->subpixel_order = COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR;
          break;
        case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
          output->state->subpixel_order = COGL_SUBPIXEL_ORDER_VERTICAL_RGB;
          break;
        case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
          output->state->subpixel_order = COGL_SUBPIXEL_ORDER_VERTICAL_BGR;
          break;
        case DRM_MODE_SUBPIXEL_NONE:
          output->state->subpixel_order = COGL_SUBPIXEL_ORDER_NONE;
          break;
        }

      /* To set a width/height we need to find a crtc mode that is
       * associated with this connector. First we look at the current
       * encoder. */

      if (connector->encoder_id)
        {
          kms_output->encoder = drmModeGetEncoder (fd, connector->encoder_id);
          if (!kms_output->encoder)
            g_warning ("Failed to get encoder description from KMS");
        }

      if (kms_output->encoder)
        {
          drmModeEncoder *encoder = kms_output->encoder;

          if (encoder->crtc_id)
            {
              kms_output->saved_crtc = drmModeGetCrtc (fd, encoder->crtc_id);
              if (!kms_output->saved_crtc)
                g_warning ("Failed to get crtc description from KMS");
            }
        }

      if (output->state->mode)
        cogl_object_unref (output->state->mode);
      output->state->mode = NULL;

      if (kms_output->saved_crtc &&
          kms_output->saved_crtc->mode_valid)
        {
          output->state->mode =
            find_mode (output->modes, kms_output->saved_crtc->mode.name);
          cogl_object_ref (output->state->mode);

          g_warn_if_fail (output->state->mode);
        }

      new_outputs = g_list_prepend (new_outputs, output);
    }

  new_outputs = g_list_sort (new_outputs, (GCompareFunc)compare_outputs);

  /* Any remaining outputs listed under renderer->outputs don't
   * correspond to KMS connector ids that are still valid so we can
   * free them... */
  g_list_free_full (renderer->outputs, (GDestroyNotify)cogl_object_unref);

  renderer->outputs = new_outputs;

  drmModeFreeResources (resources);

  _cogl_renderer_notify_outputs_changed (renderer);
}

static void
handle_hotplug (void *user_data)
{
  update_outputs (user_data);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglRendererKMS *kms_renderer;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;
  egl_renderer->platform = g_slice_new0 (CoglRendererKMS);
  kms_renderer = egl_renderer->platform;

  kms_renderer->udev_drm_device = cogl_udev_drm_device_open (renderer, error);
  if (!kms_renderer->udev_drm_device)
    goto error;

  cogl_udev_drm_device_set_hotplug_callback (kms_renderer->udev_drm_device,
                                             handle_hotplug,
                                             renderer);

  kms_renderer->fd =
    cogl_udev_drm_device_get_fd (kms_renderer->udev_drm_device);

  update_outputs (renderer);

  kms_renderer->gbm = gbm_create_device (kms_renderer->fd);
  if (kms_renderer->gbm == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Couldn't create gbm device");
      goto error;
    }

  /* The EGL API doesn't provide for a way to explicitly select a
   * platform when the driver can support multiple. Mesa allows
   * selection using an environment variable though so that's what
   * we're doing here... */
  g_setenv ("EGL_PLATFORM", "gbm", 1);

  egl_renderer->edpy = eglGetDisplay ((EGLNativeDisplayType)kms_renderer->gbm);
  if (egl_renderer->edpy == EGL_NO_DISPLAY)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't get eglDisplay");
      goto error;
    }

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

  _cogl_poll_renderer_add_fd (renderer,
                              kms_renderer->fd,
                              COGL_POLL_FD_EVENT_IN,
                              NULL, /* no prepare callback */
                              dispatch_kms_events,
                              renderer);

  return TRUE;

error:

  _cogl_winsys_renderer_disconnect (renderer);

  return FALSE;
}

static CoglBool
is_connector_excluded (int id,
                       int *excluded_connectors,
                       int n_excluded_connectors)
{
  int i;
  for (i = 0; i < n_excluded_connectors; i++)
    if (excluded_connectors[i] == id)
      return TRUE;
  return FALSE;
}

static drmModeConnector *
find_connector (int fd,
                drmModeRes *resources,
                int *excluded_connectors,
                int n_excluded_connectors)
{
  int i;

  for (i = 0; i < resources->count_connectors; i++)
    {
      drmModeConnector *connector =
        drmModeGetConnector (fd, resources->connectors[i]);

      if (connector &&
          connector->connection == DRM_MODE_CONNECTED &&
          connector->count_modes > 0 &&
          !is_connector_excluded (connector->connector_id,
                                  excluded_connectors,
                                  n_excluded_connectors))
        return connector;
      drmModeFreeConnector(connector);
    }
  return NULL;
}

static CoglBool
find_mirror_modes (drmModeModeInfo *modes0,
                   int n_modes0,
                   drmModeModeInfo *modes1,
                   int n_modes1,
                   drmModeModeInfo *mode1_out,
                   drmModeModeInfo *mode0_out)
{
  int i;
  for (i = 0; i < n_modes0; i++)
    {
      int j;
      drmModeModeInfo *mode0 = &modes0[i];
      for (j = 0; j < n_modes1; j++)
        {
          drmModeModeInfo *mode1 = &modes1[j];
          if (mode1->hdisplay == mode0->hdisplay &&
              mode1->vdisplay == mode0->vdisplay)
            {
              *mode0_out = *mode0;
              *mode1_out = *mode1;
              return TRUE;
            }
        }
    }
  return FALSE;
}

static drmModeModeInfo builtin_1024x768 =
{
        63500,                        /* clock */
        1024, 1072, 1176, 1328, 0,
        768, 771, 775, 798, 0,
        59920,
        DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
        0,
        "1024x768"
};

static CoglBool
is_panel (int type)
{
  return (type == DRM_MODE_CONNECTOR_LVDS ||
          type == DRM_MODE_CONNECTOR_eDP);
}

static CoglOutputKMS *
find_output (int _index,
             int fd,
             drmModeRes *resources,
             int *excluded_connectors,
             int n_excluded_connectors,
             CoglError **error)
{
  char *connector_env_name = g_strdup_printf ("COGL_KMS_CONNECTOR%d", _index);
  char *mode_env_name;
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  CoglOutputKMS *output;
  drmModeModeInfo *modes;
  int n_modes;

  if (getenv (connector_env_name))
    {
      unsigned long id = strtoul (getenv (connector_env_name), NULL, 10);
      connector = drmModeGetConnector (fd, id);
    }
  else
    connector = NULL;
  g_free (connector_env_name);

  if (connector == NULL)
    connector = find_connector (fd, resources,
                                excluded_connectors, n_excluded_connectors);
  if (connector == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "No currently active connector found");
      return NULL;
    }

  /* XXX: At this point it seems connector->encoder_id may be an invalid id of 0
   * even though the connector is marked as connected. Referencing ->encoders[0]
   * seems more reliable. */
  encoder = drmModeGetEncoder (fd, connector->encoders[0]);

  output = g_slice_new0 (CoglOutputKMS);
  output->connector = connector;
  output->encoder = encoder;
  output->saved_crtc = drmModeGetCrtc (fd, encoder->crtc_id);

  if (is_panel (connector->connector_type))
    {
      n_modes = connector->count_modes + 1;
      modes = g_new (drmModeModeInfo, n_modes);
      memcpy (modes, connector->modes,
              sizeof (drmModeModeInfo) * connector->count_modes);
      /* TODO: parse EDID */
      modes[n_modes - 1] = builtin_1024x768;
    }
  else
    {
      n_modes = connector->count_modes;
      modes = g_new (drmModeModeInfo, n_modes);
      memcpy (modes, connector->modes,
              sizeof (drmModeModeInfo) * n_modes);
    }

  mode_env_name = g_strdup_printf ("COGL_KMS_CONNECTOR%d_MODE", _index);
  if (getenv (mode_env_name))
    {
      const char *name = getenv (mode_env_name);
      int i;
      CoglBool found = FALSE;
      drmModeModeInfo mode;

      for (i = 0; i < n_modes; i++)
        {
          if (strcmp (modes[i].name, name) == 0)
            {
              found = TRUE;
              break;
            }
        }
      if (!found)
        {
          g_free (mode_env_name);
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "COGL_KMS_CONNECTOR%d_MODE of %s could not be found",
                       _index, name);
          return NULL;
        }
      n_modes = 1;
      mode = modes[i];
      g_free (modes);
      modes = g_new (drmModeModeInfo, 1);
      modes[0] = mode;
    }
  g_free (mode_env_name);

  output->modes = modes;
  output->n_modes = n_modes;

  return output;
}

static void
setup_crtc_modes (CoglDisplay *display, int fb_id)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  GList *l;

  for (l = kms_display->crtcs; l; l = l->next)
    {
      CoglKmsCrtc *crtc = l->data;

      int ret = drmModeSetCrtc (kms_renderer->fd,
                                crtc->id,
                                fb_id, crtc->x, crtc->y,
                                crtc->connectors, crtc->count,
                                crtc->count ? &crtc->mode : NULL);
      if (ret)
        g_warning ("Failed to set crtc mode %s: %m", crtc->mode.name);
    }
}

static void
flip_all_crtcs (CoglDisplay *display, CoglFlipKMS *flip, int fb_id)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  GList *l;

  for (l = kms_display->crtcs; l; l = l->next)
    {
      CoglKmsCrtc *crtc = l->data;
      int ret;

      if (crtc->count == 0)
        continue;

      ret = drmModePageFlip (kms_renderer->fd,
                             crtc->id, fb_id,
                             DRM_MODE_PAGE_FLIP_EVENT, flip);

      if (ret)
        {
          g_warning ("Failed to flip: %m");
          continue;
        }

      flip->pending++;
    }
}

static void
crtc_free (CoglKmsCrtc *crtc)
{
  g_free (crtc->connectors);
  g_slice_free (CoglKmsCrtc, crtc);
}

static CoglKmsCrtc *
crtc_copy (CoglKmsCrtc *from)
{
  CoglKmsCrtc *new;

  new = g_slice_new (CoglKmsCrtc);

  *new = *from;
  new->connectors = g_memdup (from->connectors, from->count * sizeof(uint32_t));

  return new;
}

static CoglBool
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  drmModeRes *resources;
  CoglOutputKMS *output0, *output1;
  CoglBool mirror;
  CoglKmsCrtc *crtc0, *crtc1;

  kms_display = g_slice_new0 (CoglDisplayKMS);
  egl_display->platform = kms_display;

  resources = drmModeGetResources (kms_renderer->fd);
  if (!resources)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "drmModeGetResources failed");
      return FALSE;
    }

  output0 = find_output (0,
                         kms_renderer->fd,
                         resources,
                         NULL,
                         0, /* n excluded connectors */
                         error);
  kms_display->outputs = g_list_append (kms_display->outputs, output0);
  if (!output0)
    return FALSE;

  if (getenv ("COGL_KMS_MIRROR"))
    mirror = TRUE;
  else
    mirror = FALSE;

  if (mirror)
    {
      int exclude_connector = output0->connector->connector_id;
      output1 = find_output (1,
                             kms_renderer->fd,
                             resources,
                             &exclude_connector,
                             1, /* n excluded connectors */
                             error);
      if (!output1)
        return FALSE;

      kms_display->outputs = g_list_append (kms_display->outputs, output1);

      if (!find_mirror_modes (output0->modes, output0->n_modes,
                              output1->modes, output1->n_modes,
                              &output0->mode,
                              &output1->mode))
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to find matching modes for mirroring");
          return FALSE;
        }
    }
  else
    {
      output0->mode = output0->modes[0];
      output1 = NULL;
    }

  crtc0 = g_slice_new (CoglKmsCrtc);
  crtc0->id = output0->encoder->crtc_id;
  crtc0->x = 0;
  crtc0->y = 0;
  crtc0->mode = output0->mode;
  crtc0->connectors = g_new (uint32_t, 1);
  crtc0->connectors[0] = output0->connector->connector_id;
  crtc0->count = 1;
  kms_display->crtcs = g_list_prepend (kms_display->crtcs, crtc0);

  if (output1)
    {
      crtc1 = g_slice_new (CoglKmsCrtc);
      crtc1->id = output1->encoder->crtc_id;
      crtc1->x = 0;
      crtc1->y = 0;
      crtc1->mode = output1->mode;
      crtc1->connectors = g_new (uint32_t, 1);
      crtc1->connectors[0] = output1->connector->connector_id;
      crtc1->count = 1;
      kms_display->crtcs = g_list_prepend (kms_display->crtcs, crtc1);
    }

  kms_display->width = output0->mode.hdisplay;
  kms_display->height = output0->mode.vdisplay;

  /* We defer setting the crtc modes until the first swap_buffers request of a
   * CoglOnscreen framebuffer. */
  kms_display->pending_set_crtc = TRUE;

  return TRUE;
}

static void
output_free (int fd, CoglOutputKMS *output)
{
  if (output->modes)
    g_free (output->modes);

  if (output->encoder)
    drmModeFreeEncoder (output->encoder);

  if (output->connector)
    {
      if (output->saved_crtc)
        {
          int ret = drmModeSetCrtc (fd,
                                    output->saved_crtc->crtc_id,
                                    output->saved_crtc->buffer_id,
                                    output->saved_crtc->x,
                                    output->saved_crtc->y,
                                    &output->connector->connector_id, 1,
                                    &output->saved_crtc->mode);
          if (ret)
            g_warning (G_STRLOC ": Error restoring saved CRTC");
        }
      drmModeFreeConnector (output->connector);
    }

  g_slice_free (CoglOutputKMS, output);
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  GList *l;

  for (l = kms_display->outputs; l; l = l->next)
    output_free (kms_renderer->fd, l->data);
  g_list_free (kms_display->outputs);
  kms_display->outputs = NULL;

  g_list_free_full (kms_display->crtcs, (GDestroyNotify) crtc_free);

  g_slice_free (CoglDisplayKMS, egl_display->platform);
}

static CoglBool
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  kms_display->dummy_gbm_surface = gbm_surface_create (kms_renderer->gbm,
                                                       16, 16,
                                                       GBM_FORMAT_XRGB8888,
                                                       GBM_BO_USE_RENDERING);
  if (!kms_display->dummy_gbm_surface)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to create dummy GBM surface");
      return FALSE;
    }

  egl_display->dummy_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) kms_display->dummy_gbm_surface,
                            NULL);
  if (egl_display->dummy_surface == EGL_NO_SURFACE)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to create dummy EGL surface");
      return FALSE;
    }

  if (!_cogl_winsys_egl_make_current (display,
                                      egl_display->dummy_surface,
                                      egl_display->dummy_surface,
                                      egl_display->egl_context))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to make context current");
      return FALSE;
    }

  return TRUE;
}

static void
_cogl_winsys_egl_cleanup_context (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }

  if (kms_display->dummy_gbm_surface != NULL)
    {
      gbm_surface_destroy (kms_display->dummy_gbm_surface);
      kms_display->dummy_gbm_surface = NULL;
    }
}

static void
_cogl_winsys_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                                const int *rectangles,
                                                int n_rectangles)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
  uint32_t handle, stride;
  CoglFlipKMS *flip;

  /* If we already have a pending swap then block until it completes */
  while (kms_onscreen->next_fb_id != 0)
    handle_drm_event (kms_renderer);

  parent_vtable->onscreen_swap_buffers_with_damage (onscreen,
                                                    rectangles,
                                                    n_rectangles);

  /* Now we need to set the CRTC to whatever is the front buffer */
  kms_onscreen->next_bo = gbm_surface_lock_front_buffer (kms_onscreen->surface);

#if (COGL_VERSION_ENCODE (COGL_GBM_MAJOR, COGL_GBM_MINOR, COGL_GBM_MICRO) >= \
     COGL_VERSION_ENCODE (8, 1, 0))
  stride = gbm_bo_get_stride (kms_onscreen->next_bo);
#else
  stride = gbm_bo_get_pitch (kms_onscreen->next_bo);
#endif
  handle = gbm_bo_get_handle (kms_onscreen->next_bo).u32;

  if (drmModeAddFB (kms_renderer->fd,
                    kms_display->width,
                    kms_display->height,
                    24, /* depth */
                    32, /* bpp */
                    stride,
                    handle,
                    &kms_onscreen->next_fb_id))
    {
      g_warning ("Failed to create new back buffer handle: %m");
      gbm_surface_release_buffer (kms_onscreen->surface,
                                  kms_onscreen->next_bo);
      kms_onscreen->next_bo = NULL;
      kms_onscreen->next_fb_id = 0;
      return;
    }

  /* If this is the first framebuffer to be presented then we now setup the
   * crtc modes, else we flip from the previous buffer */
  if (kms_display->pending_set_crtc)
    {
      setup_crtc_modes (context->display, kms_onscreen->next_fb_id);
      kms_display->pending_set_crtc = FALSE;
    }

  flip = g_slice_new0 (CoglFlipKMS);
  flip->onscreen = onscreen;

  flip_all_crtcs (context->display, flip, kms_onscreen->next_fb_id);

  if (flip->pending == 0)
    {
      drmModeRmFB (kms_renderer->fd, kms_onscreen->next_fb_id);
      gbm_surface_release_buffer (kms_onscreen->surface,
                                  kms_onscreen->next_bo);
      kms_onscreen->next_bo = NULL;
      kms_onscreen->next_fb_id = 0;
      g_slice_free (CoglFlipKMS, flip);
      flip = NULL;
    }
  else
    {
      /* Ensure the onscreen remains valid while it has any pending flips... */
      cogl_object_ref (flip->onscreen);
    }
}

static CoglBool
_cogl_winsys_egl_context_init (CoglContext *context,
                               CoglError **error)
{
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                  TRUE);

  return TRUE;
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen;
  CoglOnscreenKMS *kms_onscreen;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  if (kms_display->onscreen)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Cannot have multiple onscreens in the KMS platform");
      return FALSE;
    }

  kms_display->onscreen = onscreen;

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  egl_onscreen = onscreen->winsys;

  kms_onscreen = g_slice_new0 (CoglOnscreenKMS);
  egl_onscreen->platform = kms_onscreen;

  kms_onscreen->surface =
    gbm_surface_create (kms_renderer->gbm,
                        kms_display->width,
                        kms_display->height,
                        GBM_BO_FORMAT_XRGB8888,
                        GBM_BO_USE_SCANOUT |
                        GBM_BO_USE_RENDERING);

  if (!kms_onscreen->surface)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) kms_onscreen->surface,
                            NULL);
  if (egl_onscreen->egl_surface == EGL_NO_SURFACE)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        kms_display->width,
                                        kms_display->height);

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen;

  /* If we never successfully allocated then there's nothing to do */
  if (egl_onscreen == NULL)
    return;

  kms_display->onscreen = NULL;

  kms_onscreen = egl_onscreen->platform;

  /* flip state takes a reference on the onscreen so there should
   * never be outstanding flips when we reach here. */
  g_return_if_fail (kms_onscreen->next_fb_id == 0);

  free_current_bo (onscreen);

  if (egl_onscreen->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_onscreen->egl_surface);
      egl_onscreen->egl_surface = EGL_NO_SURFACE;
    }

  if (kms_onscreen->surface)
    {
      gbm_surface_destroy (kms_onscreen->surface);
      kms_onscreen->surface = NULL;
    }

  g_slice_free (CoglOnscreenKMS, kms_onscreen);
  g_slice_free (CoglOnscreenEGL, onscreen->winsys);
  onscreen->winsys = NULL;
}

static drmModeProperty *
get_connector_property (int fd,
                        drmModeConnector *connector,
                        const char *name)
{
  drmModeProperty *property;
  int i;

  for (i = 0; i < connector->count_props; i++)
    {
      property = drmModeGetProperty (fd, connector->props[i]);
      if (!property)
        continue;

      if (strcmp (property->name, name) == 0)
        return props;

      drmModeFreeProperty (property);
    }

  return NULL;
}

/* XXX: NB: Don't assume that the output->state is a reliable cache
 * of the real hardware state such that state changes can be avoided
 * by looking for NOP changes between ->state and ->pending since we
 * sometimes have to deal with the display being changed behind our
 * back.
 *
 * TODO: Support incremental updates in certain cases
 *
 * XXX: The corner cases to consider...
 *
 * Q: How do we _commit an overlay configuration?
 * A: During a _commit we always check that each overlay has
 *    an associated framebuffer source that has a valid
 *    ->next_bo or ->current_bo (otherwise we report an error)
 *    since we can't set a mode without a framebuffer and we
 *    don't want to be automatically allocating place holder
 *    framebuffers in corner cases or displaying undefined
 *    buffer contents.
 *
 *    If we need to call drmModeSetCrtc() we will pass ->next_bo
 *    if available, otherwise ->current_bo.
 *
 *    We always assume there could be outstanding rendering
 *    associated with a bo when calling drmModeSetCrtc() and so
 *    we explicitly synchronize with the GPU to make sure all
 *    rendering to the buffer is complete first.
 *
 *    If we are using ->next_bo that implies there is currently
 *    a pending flip that hasn't completed. We should mark the
 *    pending flip as a "stale" flip by incrementing
 *    ->stale_flips++. Later when we reach page_flip_handler()
 *    we will check the ->stale_flips counter before
 *    decrementing it and whenever it is set we should only make
 *    sure to issue any necessary _FRAME_SYNC events but we
 *    should avoid making any updates to ->current_bo/->next_bo
 *    pointers.
 *
 *XXX: why does stale_flips need to be a counter instead of just
 *     a boolean
 *
 *    After successfully calling drmModeSetCrtc() we insert a
 *    reference to the overlay in kms_onscreen->overlays.
 *
 * Q: what if there is an error when calling drmModeSetCrtc?
 * A: We roll-back the whole configuration
 *
 * Q: What are the semantics for calling drmModeSetCrtc while
 *    there are pending page flips?
 * A: Looking at the intel drm driver it looks like setting
 *    a mode starts by disabling the crtc, which involves
 *    waiting for pending flips so I think we can assume
 *    page flip events will *always* be delivered by drm.
 *
 * Q: How do we deal with roll-back when this is the first
 *    configuration to be committed?
 * A: We should be assuming that update_outputs has been
 *    called before any configuration so we should know the
 *    previous state. XXX: what about previous framebuffer
 *    state? It seems likely that we'll need to special case
 *    restoring a saved state where the saved drmModeFB doesn't
 *    correspond to a CoglFramebuffer.
 *
 * Q: What if we disassociate a framebuffer from an overlay
 *    while it still has a pending flip? (considering what
 *    might go wrong if we then immediately tried to associate
 *    the framebuffer with a different overlay)
 * A: XXX
 *
 * Q: When do we remove references from the
 *    kms_onscreen->overlays list?
 * A: XXX
 *
 * Q: How do we handle _swap_buffers
 * A: We iterate through each output that is associated with
 *    the onscreen framebuffer and for the first output
 *    we use drmModePageFlip (passing DRM_MODE_PAGE_FLIP_EVENT)
 *    to post the framebuffer for overlay0 and use
 *    drmModeSetPlane() for any additional overlays. For
 *    all other outputs we use drmModeSetCrtc to post the
 *    framebuffer for overlay0 while also using
 *    drmModeSetPlane() for additional overlays.
 *
 *    Note: until we have the atomic page flipping api we
 *    don't have a very sane way of synchronizing changes
 *    to overlay planes.
 *
 *    Note: we are intentionally only trying to synchronize
 *    with one output but we will probably want to provide
 *    api for controlling which output is synchronized.
 *
 * Q: Would it be better when handling multiple outputs to
 *    call drmModeSetCrtc() after being notified of the flip
 *    for the first output?
 * A: Since using drmModeSetCrtc() implies needing to
 *    synchronize with the GPU to make sure rendering is
 *    complete, then waiting until the flip should minimize
 *    blocking on the CPU, especially in the case where the same
 *    buffer is being posted to multiple outputs since the flip
 *    should end up waiting for the GPU to finish for us so we
 *    won't have to.
 *
 *    We should create a queue_swap_outputs utility for us to
 *    queue the swapping of all unsynchronized outputs.
 *
 *
 * Q: What if we call _swap_buffers() before we have committed
 *    an output configuration associating the framebuffer with
 *    a hardware overlay?
 * A: In this case we'll see that kms_output->overlays is NULL.
 *    We lock the front buffer, set it on ->next_bo and directly
 *    call page_flip_handler() to behave as if the flip
 *    completed immediately. This will make sure we issue a
 *    _FRAME_SYNC event for the swap and move the bo to
 *    ->current_bo.
 *
 *
 * Q: How do we deal with an error in drmModePageFlip() also
 *    considering that after the error we might commit a
 *    new display configuration which will want to find
 *    a next/current_bo to reference.
 * A: We can pretend there was no error and that the page
 *    flip completed immediately by calling page_flip_handler()
 *    directly in this case. This will make sure cogl
 *    dispatches a _FRAME_SYNC event for the frame otherwise
 *    applications may freeze. The main problem here is that
 *    we don't have meaningful timestamp data to pass to
 *    page_flip_handler(). XXX: wont this potentially break the
 *    invariable that we should be able to assume ->current_bo
 *    has no outstanding rendering?
 *
 * Q: How do we handle an onscreen framebuffer resize?
 * A: We don't, you just have to create a new framebuffer
 */
static void
_cogl_winsys_commit_outputs (CoglRenderer *renderer,
                             CoglError **error)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  GList *l;
  CoglBool roll_back = FALSE;


  /* If we hit an error at any point while committing the new
   * configuration then we revert back to here with roll_back
   * set to TRUE and instead re-commit the previous
   * configuration */
ROLL_BACK:


  for (l = renderer->outputs; l; l = l->next)
    {
      CoglOutput *output = l->data;
      CoglOutputKMS *kms_output = output->winsys;
      CoglOutputState *state = roll_back ? output->state : output->pending;
      CoglOverlay *overlay0;
      drmModeConnector *connector;
      drmModeProperty *property;
      int i;

      connector = drmModeGetConnector (kms_renderer->fd,
                                       kms_output->connector_id);
      if (!connector)
        {
          g_warn_if_reached ();
          continue;
        }

      if (state->overlays)
        overlay0 = state->overlays->data;
      else
        overlay0 = NULL;

      /* If the output has no associated overlays then we assume
       * it's ok to try and put it into DPMS_MODE_OFF */
      dpms_mode = overlay0_new ? state_new->dpms_mode : DRM_MODE_DPMS_OFF;

      property = get_connector_property (kms_renderer->fd,
                                         connector,
                                         "DPMS");
      if (property)
        {
          int status = drmModeConnectorSetProperty (kms_renderer->fd,
                                                    connector->connector_id,
                                                    property->prop_id,
                                                    dpms_mode);
          drmModeFreeProperty (property);

          if (status < 0)
            {
              const char *dpms_mode_names[] = {
                  "ON",
                  "STANDBY",
                  "SUSPEND",
                  "OFF"
              };

              g_return_if_fail (roll_back == FALSE);

              roll_back = TRUE;
              roll_back_error =
                g_strdup_printf ("Failed to set DPMS state for "
                                 "connector %d to %s",
                                 kms_output->connector_id,
                                 dpms_mode_names[dpms_mode]);
              goto ROLL_BACK;
            }
        }

      if (!overlay0)
        {
          drmModeFreeConnector (connector);
          continue;
        }

      if (!overlay0->onscreen_source)
        {
          g_return_if_fail (roll_back == FALSE);

          roll_back = TRUE;
          roll_back_error =
            g_strdup_printf ("An overlay must be associated with an "
                             "onscreen framebuffer as a color source");
          goto ROLL_BACK;
        }

      for (i = 0; i < connector->count_modes; i++)
        {
          drmModeInfo *mode_info = &connector->modes[i];
          int n_planes;

          if (strcmp (mode_info->name, state->mode->name) == 0)
            {
              CoglOnscreen *onscreen = overlay0->onscreen_source;
              CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
              CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
              uint32_t fb_id;
              int status;

              if (overlay0->src_x != 0 ||
                  overlay0->src_y != 0 ||
                  overlay0->src_width != mode_info->width ||
                  overlay0->src_height != mode_info->height)
                {
                  g_return_if_fail (roll_back == FALSE);

                  roll_back = TRUE;
                  roll_back_error =
                    g_strdup_printf ("On KMS the first overlay's source can "
                                     "not be offset and its size must match "
                                     "the mode resolution");
                  goto ROLL_BACK;
                }

              if (kms_onscreen->next_fb_id)
                fb_id = kms_onscreen->next_fb_id;
              else if (kms_onscreen->current_fb_id)
                fb_id = kms_onscreen->current_fb_id;
              else
                {
                  g_return_if_fail (roll_back == FALSE);

                  roll_back = TRUE;
                  roll_back_error =
                    g_strdup_printf ("Can't commit output changes involving "
                                     "an uninitialized (un-swapped) onscreen "
                                     "framebuffer since it has no actual "
                                     "data to scan out yet");
                  goto ROLL_BACK;
                }

              /* drmModeSetCrtc isn't synchronized with the GPU
               * pipeline so we explicitly wait for any out standing
               * rendering to complete before setting the new
               * mode... */
              cogl_framebuffer_finish (overlay0->onscreen_source);

              status = drmModeSetCrtc (kms_renderer->fd,
                                       kms_output->encoder->crtc_id,
                                       fb_id, 0, 0,
                                       &kms_output->connector_id, 1,
                                       &kms_mode_info);
              if (status < 0)
                {
                  g_return_if_fail (roll_back == FALSE);

                  roll_back = TRUE;
                  roll_back_error =
                    g_strdup_printf ("KMS was unable to set the requested "
                                     "mode (%s) on connector %d, possibly "
                                     "due to a hardware limitation",
                                     dpms_mode_names[dpms_mode],
                                     kms_output->connector_id);
                  goto ROLL_BACK;
                }
            }

          /*
           * The remaining overlays should be setup via drmModeSetPlane()
           */

          for (n_planes = 0, l = state->overlays->next;
               l;
               n_planes++, l = l->next)
            ;

          if (n_planes)
            {
              drmModePlaneRes *planes =
                drmModeGetPlaneResources (kms_renderer->fd);

              if (!planes)
                g_warn_if_reached ();

              if (planes == NULL || planes->count_planes < n_planes)
                {
                  g_return_if_fail (roll_back == FALSE);

                  roll_back = TRUE;
                  roll_back_error =
                    g_strdup_printf ("Hardware doesn't support enough "
                                     "overlays to support configuration.");
                  goto ROLL_BACK;
                }

              for (n = 0, l = state->overlays->next; l; n++, l = l->next)
                {
                  CoglOverlay *overlay = l->data;
                  CoglOnscreen *onscreen = overlay0->onscreen_source;
                  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
                  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
                  uint32_t fb_id;

                  if (!overlay->onscreen_source)
                    {
                      g_return_if_fail (roll_back == FALSE);

                      roll_back = TRUE;
                      roll_back_error =
                        g_strdup_printf ("An overlay must be associated with an "
                                         "onscreen framebuffer as a color source");
                      goto ROLL_BACK;
                    }

                  if (kms_onscreen->next_fb_id)
                    fb_id = kms_onscreen->next_fb_id;
                  else if (kms_onscreen->current_fb_id)
                    fb_id = kms_onscreen->current_fb_id;
                  else
                    {
                      g_return_if_fail (roll_back == FALSE);

                      roll_back = TRUE;
                      roll_back_error =
                        g_strdup_printf ("Can't commit output changes involving "
                                         "an uninitialized (un-swapped) onscreen "
                                         "framebuffer since it has no actual "
                                         "data to scan out yet");
                      goto ROLL_BACK;
                    }

                  if (drmModeSetPlane (kms_renderer->fd,
                                       planes->planes[n],
                                       kms_output->encoder->crtc_id,
                                       fb_id,
                                       /* FIXME: flags? */,
                                       /* FIXME: ctc_x/y/w/h,
                                        * src_x/y/w/h */) < 0)
                    {
                      g_return_if_fail (roll_back == FALSE);

                      roll_back = TRUE;
                      roll_back_error =
                        g_strdup_printf ("KMS was unable to setup the "
                                         "overlays as requested, possibly "
                                         "due to a hardware limitation.");
                      goto ROLL_BACK;
                    }
                }

              drmModeFreePlaneResources (planes);
            }
        }

      drmModeFreeConnector (connector);

      _cogl_output_update_state (output);
    }
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .context_created = _cogl_winsys_egl_context_created,
    .cleanup_context = _cogl_winsys_egl_cleanup_context,
    .context_init = _cogl_winsys_egl_context_init
  };

const CoglWinsysVtable *
_cogl_winsys_egl_kms_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_KMS winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      parent_vtable = _cogl_winsys_egl_get_vtable ();
      vtable = *parent_vtable;

      vtable.id = COGL_WINSYS_ID_EGL_KMS;
      vtable.name = "EGL_KMS";

      vtable.constraints |= COGL_RENDERER_CONSTRAINT_USES_KMS;

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;

      /* The KMS winsys doesn't support swap region */
      vtable.onscreen_swap_region = NULL;
      vtable.onscreen_swap_buffers_with_damage =
        _cogl_winsys_onscreen_swap_buffers_with_damage;

      vtable.commit_outputs = _cogl_winsys_commit_outputs;

      vtable_inited = TRUE;
    }

  return &vtable;
}

int
cogl_kms_renderer_get_kms_fd (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), -1);

  if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglRendererKMS *kms_renderer = egl_renderer->platform;
      return kms_renderer->fd;
    }
  else
    return -1;
}

void
cogl_kms_display_queue_modes_reset (CoglDisplay *display)
{
  if (display->setup)
    {
      CoglDisplayEGL *egl_display = display->winsys;
      CoglDisplayKMS *kms_display = egl_display->platform;
      kms_display->pending_set_crtc = TRUE;
    }
}

CoglBool
cogl_kms_display_set_layout (CoglDisplay *display,
                             int width,
                             int height,
                             CoglKmsCrtc **crtcs,
                             int n_crtcs,
                             CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  GList *crtc_list;
  int i;

  if ((width != kms_display->width ||
       height != kms_display->height) &&
      kms_display->onscreen)
    {
      CoglOnscreenEGL *egl_onscreen = kms_display->onscreen->winsys;
      CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
      struct gbm_surface *new_surface;
      EGLSurface new_egl_surface;

      /* Need to drop the GBM surface and create a new one */

      new_surface = gbm_surface_create (kms_renderer->gbm,
                                        width, height,
                                        GBM_BO_FORMAT_XRGB8888,
                                        GBM_BO_USE_SCANOUT |
                                        GBM_BO_USE_RENDERING);

      if (!new_surface)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                           "Failed to allocate new surface");
          return FALSE;
        }

      new_egl_surface =
        eglCreateWindowSurface (egl_renderer->edpy,
                                egl_display->egl_config,
                                (NativeWindowType) new_surface,
                                NULL);
      if (new_egl_surface == EGL_NO_SURFACE)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                           "Failed to allocate new surface");
          gbm_surface_destroy (new_surface);
          return FALSE;
        }

      eglDestroySurface (egl_renderer->edpy, egl_onscreen->egl_surface);
      gbm_surface_destroy (kms_onscreen->surface);

      kms_onscreen->surface = new_surface;
      egl_onscreen->egl_surface = new_egl_surface;

      _cogl_framebuffer_winsys_update_size (COGL_FRAMEBUFFER (kms_display->onscreen), width, height);
    }

  kms_display->width = width;
  kms_display->height = height;

  g_list_free_full (kms_display->crtcs, (GDestroyNotify) crtc_free);

  crtc_list = NULL;
  for (i = 0; i < n_crtcs; i++)
    {
      crtc_list = g_list_prepend (crtc_list, crtc_copy (crtcs[i]));
    }
  crtc_list = g_list_reverse (crtc_list);
  kms_display->crtcs = crtc_list;

  kms_display->pending_set_crtc = TRUE;

  return TRUE;
}
