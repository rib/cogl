
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>

#include <vts.h>

static void
on_enter (VTSClient *client, void *user_data)
{
  printf ("VT Enter\n");
}

static void
on_leave (VTSClient *client, void *user_data)
{
  printf ("VT Leave\n");

  vts_client_accept_vt_leave (client, NULL);
}

static void
run_client (VTSClient *client)
{
  struct pollfd pollfd;

  printf ("Run Client\n");

  vts_client_set_vt_enter_callback (client, on_enter, NULL);
  vts_client_set_vt_leave_callback (client, on_leave, NULL);

  pollfd.fd = vts_client_get_poll_fd (client);
  pollfd.events = POLLIN;

  while (1)
    {
      int n;
      VTSError *error = NULL;

      do {
        n = poll (&pollfd, 1, -1);
      } while (n < 0 && errno == EINTR);

      if (n < 0)
        fprintf (stderr, "epoll_wait failed: %m");

      printf ("client dispatch\n");
      if (n)
        {
          if (!vts_client_dispatch (client, &error))
            {
              fprintf (stderr, "Failed to dispatch vts events: %s\n",
                       error->message);
              exit (1);
            }
        }
    }
}

int
main (int argc, char **argv)
{
  VTSServer *server = vts_server_new ();
  VTSClient *client;
  VTSError *error = NULL;

  vts_server_set_pam_enabled (server, 1);
  vts_server_set_kernel_key_bindings_enabled (server, 1);

  client = vts_server_run (server, &error);

  if (error)
    {
      fprintf (stderr, "Failed to run server: %s\n", error->message);
      vts_error_free (error);
      return EXIT_FAILURE;
    }

  if (client)
    run_client (client);
}
