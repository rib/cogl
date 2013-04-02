/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _VTS_H_
#define _VTS_H_

#include <stdbool.h>

typedef struct
{
  int status;
  char *message;
} VTSError;

void
vts_error_free (VTSError *error);

typedef struct _VTSServer VTSServer;
typedef struct _VTSClient VTSClient;

VTSServer *
vts_server_new (void);

/* Should we print out any diagnosis messages? */
void
vts_server_set_verbose (VTSServer *server, bool verbose);

/* Should a PAM session be opened, or can we assume that something
 * else will have opened a PAM session for us?
 */
void
vts_server_set_pam_enabled (VTSServer *server, bool enabled);

/* What TTY device should be opened? */
void
vts_server_set_tty (VTSServer *server,
                    const char *tty);

void
vts_server_set_privileged_group (VTSServer *server,
                                 const char *group);

#warning "think about this a bit more..."
/* XXX: set_require_local_seat() might be more descriptive */
void
vts_server_set_require_systemd_session (VTSServer *server,
                                        bool session_required);

void
vts_server_set_kernel_key_bindings_enabled (VTSServer *server,
                                            bool enabled);

/* This timeout is used while a VT leave is in progress in case the
 * client has hung and in that case the server will shutdown, killing
 * the client and reseting the tty.
 *
 * A timeout <= 0 will disable the watchdog
 */
void
vts_server_set_watchdog_timeout_secs (VTSServer *server,
                                      int timeout);

/* Should the original environment be preserved for the un-privileged
 * client code? (Note the environment is unconditionally cleared
 * before running the priviledged server code)
 */
void
vts_server_set_preserve_environment (VTSServer *server,
                                     bool preserve_environment);

VTSClient *
vts_server_run (VTSServer *server, VTSError **error);

typedef void (*VTSSwitchCallback) (VTSClient *client, void *user_data);

/* Optional.
 *
 * If exec is called by the child process after returning from
 * vts_server_run() then a VTSClient can be re-created so long as the
 * socket fd is known... */
VTSClient *
vts_client_new (int fd, VTSError **error);

void
vts_client_set_vt_enter_callback (VTSClient *client,
                                  VTSSwitchCallback callback,
                                  void *user_data);

void
vts_client_set_vt_leave_callback (VTSClient *client,
                                  VTSSwitchCallback callback,
                                  void *user_data);

bool
vts_client_accept_vt_leave (VTSClient *client, VTSError **error);

bool
vts_client_deny_vt_leave (VTSClient *client, VTSError **error);

bool
vts_client_drm_set_master (VTSClient *client,
                           int drm_fd,
                           bool master,
                           VTSError **error);

/*
 * The open_flags are restricted to:
 *  O_RDONLY, O_WRONLY, O_RDWR and O_NONBLOCK
 *
 * Returns -1 on error
 */
int
vts_client_open_input_device (VTSClient *client,
                              const char *device,
                              int open_flags,
                              VTSError **error);

/* Returns an epoll fd
 *
 * NB: If using poll(2), then it's only necessary to poll for POLLIN
 */
int
vts_client_get_poll_fd (VTSClient *client);

bool
vts_client_dispatch (VTSClient *client, VTSError **error);

/* Can be called after vts_server_run() returns to determine how the
 * client exited. */
int
vts_server_get_child_wait_status (VTSServer *server);

#endif
