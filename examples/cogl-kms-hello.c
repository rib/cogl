#include <glib.h>
#include <stdio.h>
#include <stdbool.h>

#include <cogl/cogl.h>
#include <vts.h>

typedef struct _Data
{
  VTSClient *vts_client;
  int kms_fd;
  CoglContext *ctx;
  CoglDisplay *display;
  CoglFramebuffer *fb;
  CoglPrimitive *triangle;
  CoglPipeline *pipeline;
  bool ready;
  bool active;
} Data;

static void
paint (Data *data)
{
  if (!data->active)
    {
      g_print ("SKIP Paint\n");
      return;
    }

  g_print ("Paint\n");
  cogl_framebuffer_clear4f (data->fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);
  cogl_framebuffer_draw_primitive (data->fb, data->pipeline, data->triangle);
  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));
  data->ready = FALSE;
}

static void
frame_event_cb (CoglOnscreen *onscreen,
                CoglFrameEvent event,
                CoglFrameInfo *info,
                void *user_data)
{
  if (event == COGL_FRAME_EVENT_SYNC)
    {
      Data *data = user_data;
      data->ready = TRUE;
      paint (user_data);
    }
}

static void
set_drm_master (Data *data)
{
  VTSError *error = NULL;

  if (!vts_client_drm_set_master (data->vts_client, data->kms_fd, TRUE, &error))
    {
      fprintf (stderr, "Failed to become DRM master: %s\n", error->message);
      vts_error_free (error);
    }

  cogl_kms_display_queue_modes_reset (data->display);
}

static void
app_init (Data *data)
{
  CoglRenderer *renderer;
  CoglOnscreen *onscreen;
  CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };

  printf ("App Init\n");

  renderer = cogl_renderer_new ();
  cogl_renderer_add_constraint (renderer, COGL_RENDERER_CONSTRAINT_USES_KMS);
  cogl_renderer_connect (renderer, NULL);
  data->kms_fd = cogl_kms_renderer_get_kms_fd (renderer);

  data->display = cogl_display_new (renderer, NULL);
  cogl_object_unref (renderer);

  set_drm_master (data);

  data->ctx = cogl_context_new (data->display, NULL);

  printf ("Cogl context created\n");

  onscreen = cogl_onscreen_new (data->ctx, 640, 480);
  cogl_onscreen_show (onscreen);
  data->fb = COGL_FRAMEBUFFER (onscreen);

  cogl_onscreen_set_resizable (onscreen, TRUE);

  data->triangle = cogl_primitive_new_p2c4 (data->ctx,
                                            COGL_VERTICES_MODE_TRIANGLES,
                                            3, triangle_vertices);
  data->pipeline = cogl_pipeline_new (data->ctx);

  cogl_onscreen_add_frame_callback (COGL_ONSCREEN (data->fb),
                                    frame_event_cb,
                                    data,
                                    NULL); /* destroy notify */
}

#if 0
static void
app_fini (Data *data)
{
  cogl_object_unref (data->pipeline);
  cogl_object_unref (data->triangle);
  cogl_object_unref (data->fb);

  cogl_object_unref (data->ctx);
  cogl_object_unref (data->display);

  printf ("Cogl context freed\n");
}
#endif

static void
on_enter (VTSClient *client, void *user_data)
{
  Data *data = user_data;

  printf ("VT Enter\n");

  set_drm_master (data);

  data->active = TRUE;

  if (data->ready)
    {
      printf ("Paint on enter\n");
      paint (data);
    }
}

static void
on_leave (VTSClient *client, void *user_data)
{
  Data *data = user_data;
  VTSError *error = NULL;

  printf ("VT Leave\n");

#warning "XXX: do we need to inform Cogl that we are reliquishing DRM master?"

  if (!vts_client_drm_set_master (client, data->kms_fd, FALSE, &error))
    {
      fprintf (stderr, "Failed to drop DRM master: %s\n", error->message);
      vts_error_free (error);
      vts_client_deny_vt_leave (client, NULL);
    }

  data->active = FALSE;

  vts_client_accept_vt_leave (client, NULL);
}

static void
run_client (VTSClient *client)
{
  Data data;
  int poll_fd_state_age = 0;
  CoglPollFD *poll_fds = NULL;
  int n_poll_fds = 0;
  int vts_fd;

  memset (&data, 0, sizeof (data));

  data.vts_client = client;

  vts_client_set_vt_enter_callback (client, on_enter, &data);
  vts_client_set_vt_leave_callback (client, on_leave, &data);

  vts_fd = vts_client_get_poll_fd (client);

  app_init (&data);
  data.active = TRUE;
  data.ready = TRUE;

  paint (&data);

  printf ("Running mainloop\n");
  while (1)
    {
      CoglPollFD *cogl_poll_fds;
      int n_cogl_poll_fds;
      int64_t timeout = -1;
      int age;
      VTSError *vts_error = NULL;

      if (data.active)
        {
          age = cogl_poll_renderer_get_info (cogl_context_get_renderer (data.ctx),
                                             &cogl_poll_fds,
                                             &n_cogl_poll_fds,
                                             &timeout);

          /* We also need to listen for libvts messages... */
          if (age != poll_fd_state_age)
            {
              free (poll_fds);
              n_poll_fds = n_cogl_poll_fds + 1;
              poll_fds = malloc (sizeof (CoglPollFD) * n_poll_fds);
              memcpy (poll_fds, cogl_poll_fds,
                      sizeof (CoglPollFD) * n_cogl_poll_fds);
              poll_fds[n_cogl_poll_fds].fd = vts_fd;
              poll_fds[n_cogl_poll_fds].events = G_IO_IN;
              poll_fd_state_age = age;
            }
        }
      else if (n_poll_fds != 1)
        {
          free (poll_fds);
          poll_fds = malloc (sizeof (CoglPollFD));
          poll_fds->fd = vts_fd;
          poll_fds->events = G_IO_IN;
          n_poll_fds = 1;
          poll_fd_state_age = 0;
        }

      g_poll ((GPollFD *) poll_fds, n_poll_fds,
              timeout == -1 ? -1 : timeout / 1000);

      /* NB: It's important not to try and dispatch Cogl events while
       * we aren't active since we can end up blocked inside
       * drmHandleEvent() */
      if (data.active)
        {
          cogl_poll_renderer_dispatch (cogl_context_get_renderer (data.ctx),
                                       poll_fds, n_poll_fds);
        }

      if (!vts_client_dispatch (client, &vts_error))
        {
          fprintf (stderr, "Failed to dispatch vts events: %s\n",
                   vts_error->message);
          exit (1);
        }
    }
}

int
main (int argc, char **argv)
{
  VTSServer *server;
  VTSClient *client;
  VTSError *error = NULL;

  server = vts_server_new ();
  vts_server_set_pam_enabled (server, TRUE);
  vts_server_set_kernel_key_bindings_enabled (server, TRUE);

  client = vts_server_run (server, &error);

  if (error)
    {
      fprintf (stderr, "Failed to run VTS server: %s\n", error->message);
      vts_error_free (error);
      return EXIT_FAILURE;
    }

  if (client)
    run_client (client);

  return EXIT_SUCCESS;
}
