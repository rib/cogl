/*
 * Copyright © 2012 Benjamin Franzke
 * Copyright © 2010,2012,2013 Intel Corporation
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
 *
 * Authors:
 *   Benjamin Franzke
 *   Kristian Høgsberg
 *   Robert Bragg
 */

#define _GNU_SOURCE

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <error.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <termios.h>
#include <linux/vt.h>
#include <linux/major.h>
#include <linux/kd.h>

#include <pwd.h>
#include <grp.h>
#include <security/pam_appl.h>

#include <xf86drm.h>

#include <systemd/sd-login.h>
#include <systemd/sd-journal.h>

#include "vts.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

struct _VTSServer
{
  bool stop_child_enabled;
  char *privileged_group;
  bool require_systemd_session;
  bool kernel_key_bindings_enabled;
  bool preserve_environment;

  bool pam_enabled;
  struct pam_conv pc;
  pam_handle_t *ph;
  int pam_status;
  bool pam_session_opened;

  char *tty_name;
  int tty;
  int ttynr;
  struct termios terminal_attributes;
  bool saved_terminal_attributes;
  int kb_mode;
  bool saved_kb_mode;
  bool handling_vt_leave;

  bool watchdog_enabled;
  uint64_t watchdog_timestamp;
  uint64_t watchdog_timeout;

  struct sigaction old_sigchld_action;
  bool setup_sigchld_action;
  sigset_t old_signal_mask;
  bool masked_signals;
  int signalfd;

  int sock[2];
  struct passwd *pw;

  int epollfd;

  pid_t child;
  int child_wait_status;

  bool quit;
};

typedef void (*VTSSwitchCallback) (VTSClient *client, void *user_data);

struct _VTSClient
{
  int fd;
  int epollfd;

  VTSSwitchCallback vt_enter_callback;
  void *vt_enter_user_data;

  VTSSwitchCallback vt_leave_callback;
  void *vt_leave_user_data;
};

typedef enum
{
  VTS_MESSAGE_ID_VT_SWITCH = 1,
  VTS_MESSAGE_ID_VT_LEAVE_ACCEPT,
  VTS_MESSAGE_ID_VT_LEAVE_DENY,
  VTS_MESSAGE_ID_OPEN_INPUT,
  VTS_MESSAGE_ID_SET_MASTER
} VTSMessageID;

typedef struct
{
  VTSMessageID id;
} VTSMessage;

typedef struct
{
  VTSMessage header;
  int flags;
  char path[0];
} VTSOpenInputMessage;

typedef struct
{
  VTSMessage header;
  bool set_master;
} VTSSetMasterMessage;

typedef enum {
  VTS_VT_SWITCH_DIRECTION_ENTER,
  VTS_VT_SWITCH_DIRECTION_LEAVE
} VTSVTSwitchDirection;

typedef struct
{
  VTSMessage header;
  VTSVTSwitchDirection direction;
} VTSVTSwitchMessage;

union cmsg_data { unsigned char b[4]; int fd; };

#define vts_debug(x, a...) \
  fprintf (stderr, "[DEBUG] " x "\n", ##a)
//  sd_journal_print (LOG_DEBUG, x, ##a)

#define vts_warning(x, a...) \
  fprintf (stderr, "[WARNING] " x "\n", ##a)
//  sd_journal_print (LOG_WARNING, x, ##a)

static void *
xmalloc (size_t size)
{
  void *ptr = malloc (size);
  if (!ptr)
    abort ();
  return ptr;
}

static void *
xmalloc0 (size_t size)
{
  void *ptr = xmalloc (size);
  memset (ptr, 0, size);
  return ptr;
}

#define xnew0(TYPE, COUNT) xmalloc0 (sizeof(TYPE) * COUNT);

static char *
xstrdup (const char *str)
{
  char *ret;
  if (str)
    {
      ret = strdup (str);
      if (!ret)
        abort ();
    }
  else
    ret = NULL;
  return ret;
}

static void
set_error (VTSError **error,
           int status,
           const char *format,
           ...)
{
  va_list args;
  VTSError *err;

  va_start (args, format);

  if (error == NULL)
    {
      //vts_warning (format, args);
      exit (EXIT_FAILURE);
    }

  if (*error != NULL)
    {
      vts_warning ("Spurious attempt to overwrite error message:");
      //vts_warning (format, args);
      return;
    }

  err = xnew0 (VTSError, 1);
  err->status = status;
  if (vasprintf (&err->message, format, args) < 0)
    err->message = NULL;
  *error = err;

  va_end (args);
}

#define set_error_from_errno(error, format, a...) \
  set_error (error, errno, "%s: " format, strerror(errno), ##a)

void
vts_error_free (VTSError *error)
{
  free (error->message);
  free (error);
}

uint64_t
get_time (void)
{
  struct timespec ts;

  clock_gettime (CLOCK_MONOTONIC, &ts);

  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

VTSServer *
vts_server_new (void)
{
  VTSServer *server = xnew0 (VTSServer, 1);

  server->tty = -1;
  server->epollfd = -1;
  server->signalfd = -1;
  server->sock[0] = -1;
  server->sock[1] = -1;
  server->pam_status = PAM_SUCCESS;
  server->watchdog_timeout = 10000;

  return server;
}

void
vts_server_set_pam_enabled (VTSServer *server, bool enabled)
{
  server->pam_enabled = enabled;
}

void
vts_server_set_tty (VTSServer *server, const char *tty)
{
  free (server->tty_name);
  server->tty_name = xstrdup (tty);
}

void
vts_server_set_privileged_group (VTSServer *server,
                                 const char *group)
{
  free (server->privileged_group);
  server->privileged_group = xstrdup (group);
}

#warning "think about this a bit more..."
void
vts_server_set_require_systemd_session (VTSServer *server,
                                        bool session_required)
{
  server->require_systemd_session = session_required;
}

void
vts_server_set_kernel_key_bindings_enabled (VTSServer *server,
                                            bool enabled)
{
  server->kernel_key_bindings_enabled = enabled;
}

void
vts_server_set_watchdog_timeout_secs (VTSServer *server,
                                      int timeout)
{
  server->watchdog_timeout = timeout * 1000;
}

/* If TRUE then the child process will be sent a SIGSTOP enabling a
 * debugger to be attached before the child is setup.
 */
void
vts_server_set_stop_child_enabled (VTSServer *server,
                                   bool stop_child_enabled)
{
  server->stop_child_enabled = stop_child_enabled;
}

void
vts_server_set_preserve_environment (VTSServer *server,
                                     bool preserve_environment)
{
  server->preserve_environment = preserve_environment;
}

static gid_t *
read_groups (VTSError **error)
{
  int n;
  gid_t *groups;

  n = getgroups (0, NULL);

  if (n < 0)
    {
      set_error_from_errno (error, "Unable to retrieve groups");
      return NULL;
    }

  groups = xnew0 (gid_t, n);
  if (!groups)
    return NULL;

  if (getgroups (n, groups) < 0)
    {
      set_error_from_errno (error, "Unable to retrieve groups");
      free (groups);
      return NULL;
    }

  return groups;
}

/* XXX: Note that a return value of FALSE here does not imply an error
 * (unlike most other functions that take an error pointer) and so
 * error should be explicitly check after calling this api.
 */
static bool
in_priviledged_group (VTSServer *server, VTSError **error)
{
  struct group *gr;
  gid_t *groups;
  int i;

  if (getuid () == 0)
    return TRUE;

  gr = getgrnam (server->privileged_group);
  if (!gr)
    {
      set_error_from_errno (error, "Failed to lookup '%s' in group database",
                            server->privileged_group);
      return FALSE;
    }

  groups = read_groups (error);
  if (!groups)
    return FALSE;

  for (i = 0; groups[i]; i++)
    {
      if (groups[i] == gr->gr_gid)
        {
          free (groups);
          return TRUE;
        }
    }
  free (groups);
#warning "For consistency we could return a VTSError here?"

  return FALSE;
}

bool
have_systemd_seat_session (VTSServer *server)
{
  char *session, *seat;
  int err;

  err = sd_pid_get_session (getpid (), &session);
  if (err == 0 && session)
    {
      if (sd_session_is_active (session) &&
          sd_session_get_seat (session, &seat) == 0)
        {
          free (seat);
          free (session);
          return TRUE;
        }
      free (session);
    }

  return FALSE;
}

static int
pam_conversation_fn (int msg_count,
                     const struct pam_message **messages,
                     struct pam_response **responses,
                     void *user_data)
{
  return PAM_SUCCESS;
}

static bool
open_pam_login_session (VTSServer *server, VTSError **error)
{
  server->pc.conv = pam_conversation_fn;
  server->pc.appdata_ptr = server;

  vts_debug ("Opening PAM session...");

  server->pam_status = pam_start ("login",
                                  server->pw->pw_name,
                                  &server->pc, &server->ph);
  if (server->pam_status != PAM_SUCCESS)
    {
      set_error (error, 0,
                 "Failed to start pam transaction: %d: %s\n",
                 server->pam_status,
                 pam_strerror (server->ph, server->pam_status));
      return FALSE;
    }

  vts_debug ("PAM_TTY=%s", ttyname (server->tty));
  server->pam_status = pam_set_item (server->ph,
                                     PAM_TTY, ttyname (server->tty));
  if (server->pam_status != PAM_SUCCESS)
    {
      set_error (error, 0,
                 "Failed to set PAM_TTY item: %d: %s\n",
                 server->pam_status,
                 pam_strerror (server->ph, server->pam_status));
      return FALSE;
    }

  server->pam_status = pam_open_session (server->ph, 0);
  if (server->pam_status != PAM_SUCCESS)
    {
      set_error (error, 0,
                 "Failed to open pam session: %d: %s\n",
                 server->pam_status,
                 pam_strerror (server->ph, server->pam_status));
      return FALSE;
    }

  server->pam_session_opened = TRUE;

  return TRUE;
}

static bool
create_socketpair (VTSServer *server,
                   VTSError **error)
{
  struct epoll_event ev;

  if (socketpair (AF_LOCAL, SOCK_DGRAM, 0, server->sock) < 0)
    {
      set_error_from_errno (error, "Failed to create socketpair");
      return FALSE;
    }

  fcntl (server->sock[0], F_SETFD, O_CLOEXEC);

  memset (&ev, 0, sizeof ev);
  ev.events = EPOLLIN;
  ev.data.fd = server->sock[0];
  if (epoll_ctl (server->epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0)
    {
      set_error_from_errno (error, "Failed to setup epoll fd");
      close (server->sock[0]);
      close (server->sock[1]);
      return FALSE;
    }

  return TRUE;
}

static void
reset_signals (VTSServer *server)
{
  close (server->signalfd);
  server->signalfd = -1;

  if (server->masked_signals)
    {
      sigprocmask (SIG_SETMASK, &server->old_signal_mask, NULL);
      server->masked_signals = FALSE;
    }

  if (server->setup_sigchld_action)
    {
      sigaction (SIGCHLD, &server->old_sigchld_action, NULL);
      server->setup_sigchld_action = FALSE;
    }
}

static bool
setup_signals (VTSServer *server, VTSError **error)
{
  sigset_t mask;
  struct sigaction sa;
  struct epoll_event ev;
  int err;

  memset (&sa, 0, sizeof sa);
  sa.sa_handler = SIG_DFL;
  sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  if (sigaction (SIGCHLD, &sa, &server->old_sigchld_action) < 0)
    {
      set_error_from_errno (error, "Failed to set SIGCHLD action");
      goto error;
    }
  server->setup_sigchld_action = TRUE;

  err = sigemptyset (&mask);
  assert (err == 0);
  sigaddset (&mask, SIGCHLD);
  sigaddset (&mask, SIGINT);
  sigaddset (&mask, SIGTERM);
  sigaddset (&mask, SIGUSR1); /* VT release */
  sigaddset (&mask, SIGUSR2); /* VT acquire */
  if (sigprocmask (SIG_BLOCK, &mask, &server->old_signal_mask) < 0)
    {
      set_error_from_errno (error, "Failed to set signal mask");
      goto error;
    }
  server->masked_signals = TRUE;

  server->signalfd = signalfd (-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
  if (server->signalfd < 0)
    {
      set_error_from_errno (error, "Failed to get signal file descriptor");
      goto error;
    }

  memset (&ev, 0, sizeof ev);
  ev.events = EPOLLIN;
  ev.data.fd = server->signalfd;
  if (epoll_ctl (server->epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0)
    {
      set_error_from_errno (error, "Failed to add signal fd to epoll fd");
      goto error;
    }

  return TRUE;

error:

  reset_signals (server);

  return FALSE;
}

static bool
open_tty (VTSServer *server, VTSError **error)
{
  if (server->tty_name)
    {
      char *t = ttyname (STDIN_FILENO);
      if (t && strcmp (t, server->tty_name) == 0)
        server->tty = STDIN_FILENO; /* XXX: should we fcntl (O_CLOEXEC) ? */
      else
        {
          struct stat buf;

          if (stat (server->tty_name, &buf) < 0)
            {
              set_error_from_errno (error, "Failed to stat '%s'",
                                    server->tty_name);
              return FALSE;
            }

          if (major (buf.st_rdev) != TTY_MAJOR)
            {
              set_error (error, 0, "Invalid tty device: %s", server->tty_name);
              return FALSE;
            }

          server->ttynr = minor (buf.st_rdev);
        }

      server->tty = open (server->tty_name, O_RDWR | O_NOCTTY | O_CLOEXEC);
    }
  else
    {
      int tty0 = open ("/dev/tty0", O_WRONLY | O_CLOEXEC);
      char filename[16];

      if (tty0 < 0)
        {
          set_error_from_errno (error, "Could not open tty0");
          return FALSE;
        }

      if (ioctl (tty0, VT_OPENQRY, &server->ttynr) < 0 || server->ttynr == -1)
        {
          set_error_from_errno (error, "failed to find free console");
          close (tty0);
          return FALSE;
        }

      close (tty0);

      snprintf (filename, sizeof filename, "/dev/tty%d", server->ttynr);
      server->tty = open (filename, O_RDWR | O_NOCTTY);
    }

  if (server->tty < 0)
    {
      set_error_from_errno (error, "Failed to open tty%d", server->ttynr);
      return FALSE;
    }

  return TRUE;
}

static void
on_tty_input (VTSServer *server)
{
  /* Ignore input to tty */
  tcflush (server->tty, TCIFLUSH);
}

static void
reset_tty (VTSServer *server)
{
  if (server->tty >= 0)
    {
      if (ioctl (server->tty, KDSETMODE, KD_TEXT))
        vts_warning ("failed to set KD_TEXT mode on tty: %m\n");

      if (server->saved_kb_mode &&
          ioctl (server->tty, KDSKBMODE, server->kb_mode))
        vts_warning ("failed to restore keyboard mode: %m\n");

      if (server->saved_terminal_attributes &&
          tcsetattr (server->tty, TCSANOW, &server->terminal_attributes) < 0)
        vts_warning ("Could not restore original tty attributes\n");
    }
}

static bool
setup_tty (VTSServer *server, VTSError **error)
{
  struct termios raw_attributes;
  struct vt_mode mode = { 0 };
  int ret;

  if (ioctl (server->tty, VT_ACTIVATE, server->ttynr) < 0 ||
      ioctl (server->tty, VT_WAITACTIVE, server->ttynr) < 0)
    {
      set_error_from_errno (error, "Failed to switch to tty");
      return FALSE;
    }

  if (tcgetattr (server->tty, &server->terminal_attributes) < 0)
    {
      set_error_from_errno (error, "Could not get terminal attributes");
      return FALSE;
    }
  server->saved_terminal_attributes = TRUE;

  /* Note: up until this point errors return immediately instead of
   * jumping to ERROR: since that relies on calling vts_tty_destroy()
   * which would try and restore the terminal attributes which, up
   * until this point, were in an undefined state. */

  /* Ignore control characters and disable echo */
  raw_attributes = server->terminal_attributes;
  cfmakeraw (&raw_attributes);

  /* Fix up line endings to be normal (cfmakeraw hoses them) */
  raw_attributes.c_oflag |= OPOST | OCRNL;

  if (tcsetattr (server->tty, TCSANOW, &raw_attributes) < 0)
    {
      set_error_from_errno (error, "Could not put terminal into raw mode");
      goto error;
    }

  ioctl (server->tty, KDGKBMODE, &server->kb_mode);
  server->saved_kb_mode = TRUE;

#if 0
  if (!server->kernel_key_bindings_enabled &&
      ioctl (server->tty, KDSKBMODE, K_OFF));
    {
      ret = ioctl (server->tty, KDSKBMODE, K_RAW);
      if (ret)
        {
          set_error_from_errno (error, "failed to set keyboard mode on tty");
          goto error;
        }
    }
#endif

  ret = ioctl (server->tty, KDSETMODE, KD_GRAPHICS);
  if (ret)
    {
      set_error_from_errno (error, "Failed to set KD_GRAPHICS mode on tty");
      goto error;
    }

  mode.mode = VT_PROCESS;
  mode.relsig = SIGUSR1;
  mode.acqsig = SIGUSR2;
  if (ioctl (server->tty, VT_SETMODE, &mode) < 0)
    {
      set_error_from_errno (error, "Failed to take control of vt handling");
      goto error;
    }

  return TRUE;

error:

  reset_tty (server);

  return FALSE;
}

static bool
server_handle_setmaster (VTSServer *server,
                         struct msghdr *msg,
                         ssize_t len,
                         VTSError **error)
{
  struct cmsghdr *cmsg;
  VTSSetMasterMessage *message;
  union cmsg_data *data;
  int fd = -1;
  int ret;
  bool status = FALSE;

  if (len != sizeof (*message))
    {
      set_error (error, 0, "Spurious SetMaster message size");
      goto exit;
    }

  message = msg->msg_iov->iov_base;

  cmsg = CMSG_FIRSTHDR (msg);
  if (!cmsg ||
      cmsg->cmsg_level != SOL_SOCKET ||
      cmsg->cmsg_type != SCM_RIGHTS)
    {
      set_error (error, 0, "Invalid control message");
      goto exit;
    }

  data = (union cmsg_data *) CMSG_DATA (cmsg);
  if (data->fd == -1)
    {
      set_error (error, 0, "missing drm fd for SetMessage request");
      goto exit;
    }
  fd = data->fd;

#warning "XXX: how can we validate that this drm device belongs to the session-seat before we SetMaster?"

  if (message->set_master)
    ret = drmSetMaster (fd);
  else
    ret = drmDropMaster (fd);

  if (ret == -1)
    {
      set_error_from_errno (error, "Failed to Set/Drop drm master");
      goto exit;
    }

  status = TRUE;

exit:
  close (fd);

  do {
    len = send (server->sock[0], &ret, sizeof ret, 0);
  } while (len < 0 && errno == EINTR);

  if (len < sizeof ret)
    {
      set_error_from_errno (error, "Failed to send SetMaster reply");
      return FALSE;
    }

  return status;
}

static bool
server_handle_open_input (VTSServer *server,
                          struct msghdr *msg,
                          ssize_t len,
                          VTSError **error)
{
  int fd = -1, ret = -1;
  char control[CMSG_SPACE (sizeof (fd))];
  struct cmsghdr *cmsg;
  struct stat s;
  struct msghdr nmsg;
  struct iovec iov;
  VTSOpenInputMessage *message;
  union cmsg_data *data;
  bool status = FALSE;

  message = msg->msg_iov->iov_base;
  if ((size_t)len < sizeof (*message))
    {
      set_error (error, 0, "Unexpected OpenInput message size");
      goto exit;
    }

  /* Ensure path is null-terminated */
  ((char *) message)[len-1] = '\0';

  if (stat (message->path, &s) < 0)
    {
      set_error_from_errno (error, "Failed to stat input device");
      goto exit;
    }

#warning "XXX: the input device might not belong to this seat!?"
  if (major (s.st_rdev) != INPUT_MAJOR)
    {
      set_error (error, 0, "Unexpected OpenInput message size");
      goto exit;
    }

  fd = open (message->path,
             message->flags & (O_RDONLY | O_WRONLY | O_RDWR | O_NONBLOCK));
  if (fd < 0)
    {
      set_error_from_errno (error, "Failed to open input device");
      goto exit;
    }

  status = TRUE;

exit:
  memset (&nmsg, 0, sizeof nmsg);
  nmsg.msg_iov = &iov;
  nmsg.msg_iovlen = 1;
  if (fd != -1)
    {
      nmsg.msg_control = control;
      nmsg.msg_controllen = sizeof control;
      cmsg = CMSG_FIRSTHDR (&nmsg);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = CMSG_LEN (sizeof (fd));
      data = (union cmsg_data *) CMSG_DATA(cmsg);
      data->fd = fd;
      nmsg.msg_controllen = cmsg->cmsg_len;
      ret = 0;
    }
  iov.iov_base = &ret;
  iov.iov_len = sizeof ret;

  vts_debug ("Opened %s: ret: %d, fd: %d\n",
             message->path, ret, fd);
  do {
    len = sendmsg (server->sock[0], &nmsg, 0);
  } while (len < 0 && errno == EINTR);

  if (len < 0)
    {
      set_error_from_errno (error, "Failed to send reply");
      return FALSE;
    }

  return status;
}

static bool
server_handle_vt_leave_response (VTSServer *server,
                                 struct msghdr *msg,
                                 ssize_t len,
                                 VTSError **error)
{
  VTSMessage *message;

  if (!server->handling_vt_leave)
    {
      set_error (error, 0, "Spurious VT leave response");
      return FALSE;
    }

  if ((size_t)len < sizeof (VTSMessage))
    {
      set_error (error, 0, "Unexpected VT leave response size");
      return FALSE;
    }

  message = msg->msg_iov->iov_base;

  if (message->id == VTS_MESSAGE_ID_VT_LEAVE_ACCEPT)
    {
#warning "Check in this case that drmDropMaster() was called if necessary"
      vts_debug ("VT leave accept received");
      ioctl (server->tty, VT_RELDISP, 1);
    }
  else
    {
      vts_debug ("VT leave deny received");
      ioctl (server->tty, VT_RELDISP, 0);
    }

  server->watchdog_enabled = FALSE;
  server->handling_vt_leave = FALSE;

  return TRUE;
}

static bool
server_handle_message (VTSServer *server, VTSError **error)
{
  char control[CMSG_SPACE (sizeof (int))];
  char buf[BUFSIZ];
  struct msghdr msg;
  struct iovec iov;
  ssize_t len;
  VTSMessage *message;

  memset (&msg, 0, sizeof (msg));
  iov.iov_base = buf;
  iov.iov_len  = sizeof buf;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof control;

  do {
    len = recvmsg (server->sock[0], &msg, 0);
  } while (len < 0 && errno == EINTR);

  if (len < 1)
    {
      set_error_from_errno (error, "Failed to receive server message");
      return FALSE;
    }

  message = (void *) buf;
  switch (message->id)
    {
    case VTS_MESSAGE_ID_VT_LEAVE_ACCEPT:
    case VTS_MESSAGE_ID_VT_LEAVE_DENY:
      return server_handle_vt_leave_response (server, &msg, len, error);
    case VTS_MESSAGE_ID_OPEN_INPUT:
      return server_handle_open_input (server, &msg, len, error);
    case VTS_MESSAGE_ID_SET_MASTER:
      return server_handle_setmaster (server, &msg, len, error);
    default:
      set_error (error, 0, "Unknown message type received by server");
      return FALSE;
    }
}

static bool
send_vt_switch_message (VTSServer *server,
                        VTSVTSwitchDirection direction,
                        VTSError **error)
{
  VTSVTSwitchMessage message;
  ssize_t len;

  message.header.id = VTS_MESSAGE_ID_VT_SWITCH;
  message.direction = direction;

  do {
    len = send (server->sock[0], &message, sizeof (message), 0);
  } while (len < 0 && errno == EINTR);

  if (len < 0)
    {
      set_error_from_errno (error, "Failed to send VT switch message");
      return FALSE;
    }

  return TRUE;
}

static bool
server_handle_vt_signal (VTSServer *server,
                         VTSVTSwitchDirection direction,
                         VTSError **error)
{
  vts_debug ("VT switch signal received");

  if (server->handling_vt_leave)
    {
      /* XXX: The kernel may pile up VT release signals but in
       * that case only one ACK is required so we ignore additional
       * release requests while we are waiting for an ACK.
       *
       * In the case of acquire signals it looks like the kernel
       * kindly blocks those for us while a release is pending
       * but since that doesn't seem to be documented anywhere
       * we won't assume that behaviour.
       */
      vts_warning ("Ignoring vt switch signal while handling switch");
      return TRUE;
    }

  if (direction == VTS_VT_SWITCH_DIRECTION_LEAVE)
    {
      if (!send_vt_switch_message (server,
                                   VTS_VT_SWITCH_DIRECTION_LEAVE, error))
        return FALSE;

      if (server->watchdog_timeout > 0)
        {
          vts_debug ("Watchdog enabled for %d seconds",
                     (int)server->watchdog_timeout / 1000);
          server->watchdog_timestamp = get_time ();
          server->watchdog_enabled = TRUE;
        }

      server->handling_vt_leave = TRUE;
    }
  else
    {
      ioctl (server->tty, VT_RELDISP, VT_ACKACQ);

      if (!send_vt_switch_message (server,
                                   VTS_VT_SWITCH_DIRECTION_ENTER, error))
        return FALSE;
    }

  return TRUE;
}

static bool
server_handle_signal (VTSServer *server, VTSError **error)
{
  struct signalfd_siginfo sig;
  int pid, status;

  if (read (server->signalfd, &sig, sizeof sig) != sizeof sig)
    {
      set_error_from_errno (error, "Failed to read signal fd");
      return FALSE;
    }

  vts_debug ("Signal received: %s", strsignal (sig.ssi_signo));

  switch (sig.ssi_signo)
    {
    case SIGCHLD:
      do {
        pid = wait (&status);
      } while (pid == -1 && errno == EINTR);

      if (pid == (pid_t)-1)
        {
          set_error_from_errno (error, "Failed to wait for child exit status");
          return FALSE;
        }
      else if (pid == server->child)
        {
          server->child = 0;
          server->child_wait_status = status;

          server->quit = TRUE;
          return TRUE;
        }
      break;
    case SIGTERM:
      if (server->child)
        kill (server->child, SIGTERM);

      server->quit = TRUE;
      return TRUE;
    case SIGINT:
      if (server->child)
        {
          printf ("Killing child...\n");
          kill (server->child, SIGTERM);
        }
      return TRUE;
    case SIGUSR1:
      return server_handle_vt_signal (server, VTS_VT_SWITCH_DIRECTION_LEAVE,
                                      error);
    case SIGUSR2:
      return server_handle_vt_signal (server, VTS_VT_SWITCH_DIRECTION_ENTER,
                                      error);
    default:
      vts_warning ("Spurious signal received %d", sig.ssi_signo);
      return TRUE;
    }

  return TRUE;
}

static void
setup_child (VTSServer *server, char **saved_environ)
{
  char **env;

  vts_debug ("Spawned child with pid: %d\n", getpid ());

  reset_signals (server);

#warning "Have a server_close_fds() function for this..."
  close (server->sock[0]);
  close (server->epollfd);

  if (setgid (server->pw->pw_gid) < 0 ||
      setuid (server->pw->pw_uid) < 0)
    {
      vts_warning ("Failed to drop privileges");
      exit (EXIT_FAILURE);
    }

  if (server->stop_child_enabled)
    {
      vts_debug ("Stopping child\n");
      raise (SIGSTOP);
    }

  if (saved_environ)
    {
      int i;
      vts_debug ("Restoring saved environment variables...");
      for (i = 0; saved_environ[i]; i++)
        {
          vts_debug ("putenv (%s)", saved_environ[i]);
          if (putenv (saved_environ[i]) < 0)
            vts_warning ("putenv %s failed", saved_environ[i]);
        }
    }

  env = pam_getenvlist (server->ph);
  if (env)
    {
      int i;
      vts_debug ("setting PAM environment variables...");

      for (i = 0; env[i]; i++)
        {
          vts_debug ("putenv (%s)", env[i]);

          if (putenv (env[i]) < 0)
            vts_warning ("putenv %s failed", env[i]);
        }
      free (env);
    }
}

VTSClient *
vts_client_new (int fd, VTSError **error)
{
  int epollfd = -1;
  struct epoll_event ev;
  VTSClient *client;

  epollfd = epoll_create1 (EPOLL_CLOEXEC);
  if (epollfd < 0)
    {
      set_error_from_errno (error, "Failed to create epoll fd");
      return NULL;
    }

  memset (&ev, 0, sizeof ev);
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  if (epoll_ctl (epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0)
    {
      set_error_from_errno (error, "Failed to setup epoll fd");
      return NULL;
    }

  client = xnew0 (VTSClient, 1);
  client->fd = fd;
  client->epollfd = epollfd;

  return client;
}

VTSClient *
vts_server_run (VTSServer *server, VTSError **error)
{
  char **saved_environ = NULL;

  /* For the convenience of not needing to setuid a launcher during
   * development we also check the SUDO_USER variable if the real user
   * is root.
   */
  if (getuid () == 0 && getenv ("SUDO_USER"))
    server->pw = getpwnam (getenv ("SUDO_USER"));
  else
    server->pw = getpwuid (getuid ());
  if (server->pw == NULL)
    {
      set_error_from_errno (error, "Failed to get username");
      return NULL;
    }

  /* We need to clear the environment before running privileged code
   * but optionally we save the environment to be restored once
   * privileges have been dropped which can be useful for keeping
   * variables that affect the non-privileged client, such as debug
   * options.
   */
  if (server->preserve_environment)
    {
      int i, j;

      for (i = 0; environ[i]; i++)
        ;
      saved_environ = xmalloc (sizeof (char *) * (i + 1));
      for (j = 0; j < i; j++)
        saved_environ[i] = xstrdup (environ[i]);
      saved_environ[j] = NULL;
    }

  clearenv ();

  setenv ("USER", server->pw->pw_name, 1);
  setenv ("LOGNAME", server->pw->pw_name, 1);
  setenv ("HOME", server->pw->pw_dir, 1);
  setenv ("SHELL", server->pw->pw_shell, 1);

  vts_debug ("USER=%s", server->pw->pw_name);
  vts_debug ("HOME=%s", server->pw->pw_dir);
  vts_debug ("SHELL=%s", server->pw->pw_shell);

  if (server->privileged_group && !in_priviledged_group (server, error))
    {
      if (error && *error)
        return NULL;

      set_error (error, EPERM,
                 "Permission denied.\n"
                 "User does not belong to privileged '%s' group.\n",
                 server->privileged_group);
      return NULL;
    }

  vts_debug ("Privileges OK\n");

  if (server->require_systemd_session && !have_systemd_seat_session (server))
    {
      set_error (error, EPERM,
                 "Permission denied. You must run from an active "
                 "and local systemd session:\n");
      return NULL;
    }

  if (!open_tty (server, error))
    goto server_shutdown;

  vts_warning ("TTY %d opened", server->ttynr);

  if (server->pam_enabled && !open_pam_login_session (server, error))
    goto server_shutdown;

  if (!setup_tty (server, error))
    goto server_shutdown;

#warning "XXX: Do we need to setup signals/epollfd before setup_tty?"
  server->epollfd = epoll_create1 (EPOLL_CLOEXEC);
  if (server->epollfd < 0)
    {
      set_error_from_errno (error, "Failed to create epoll fd");
      goto server_shutdown;
    }

  if (!create_socketpair (server, error))
    goto server_shutdown;

  if (!setup_signals (server, error))
    goto server_shutdown;

  server->child_wait_status = 0;

  vts_warning ("Forking to run client");

  switch ((server->child = fork()))
    {
    case -1:
      set_error_from_errno (error, "Fork failed");
      goto server_shutdown;
    case 0:
      setup_child (server, saved_environ);
      return vts_client_new (server->sock[1], NULL);
    default:
      close (server->sock[1]);

      vts_debug ("Running mainloop...");
      while (!server->quit)
        {
          struct epoll_event ev;
          int n;

          do
            {
              int timeout = -1;

              if (server->watchdog_enabled)
                {
                  uint64_t elapsed = get_time () - server->watchdog_timestamp;
                  timeout = server->watchdog_timeout - elapsed;
                  if (timeout <= 0)
                    {
                      vts_warning ("Watchdog triggered shutdown!");
                      kill (server->child, SIGTERM);
                      set_error (error, 0, "Watchdog triggered shutdown");
                      goto server_shutdown;
                    }
                }

              n = epoll_wait (server->epollfd, &ev, 1, timeout);
            }
          while (n == 0 || n < 0 && errno == EINTR);

          if (n < 0)
            {
              set_error_from_errno (error, "epoll_wait failed");
              goto server_shutdown;
            }

          vts_debug ("Iterating mainloop");
          if (ev.data.fd == server->sock[0])
            {
              if (!server_handle_message (server, error))
                goto server_shutdown;
            }
          else if (ev.data.fd == server->signalfd)
            {
              if (!server_handle_signal (server, error))
                goto server_shutdown;
            }
          else if (ev.data.fd == server->tty)
            on_tty_input (server);
        }
      break;
    }

server_shutdown:

  if (server->child_wait_status)
    {
      if (WIFEXITED (server->child_wait_status))
        vts_warning ("Child exited with status %d",
                     WEXITSTATUS (server->child_wait_status));
      else if (WIFSIGNALED (server->child_wait_status))
        vts_warning ("Child exited with signal %s",
                     strsignal (WTERMSIG (server->child_wait_status)));
    }
  else if (server->child)
    kill (server->child, SIGTERM);
#warning "Should we kill the child here?"

  if (server->pam_enabled)
    {
      if (server->pam_session_opened)
        {
          server->pam_status = pam_close_session (server->ph, 0);
          server->pam_session_opened = FALSE;
        }

      pam_end (server->ph, server->pam_status);
    }

  if (server->tty >= 0)
    {
      reset_tty (server);

      if (server->tty != STDIN_FILENO)
        close (server->tty);
    }

  reset_signals (server);

  close (server->epollfd);
  close (server->sock[0]);

  return NULL;
}

int
vts_server_get_child_wait_status (VTSServer *server)
{
  return server->child_wait_status;
}

void
vts_client_set_vt_enter_callback (VTSClient *client,
                                  VTSSwitchCallback callback,
                                  void *user_data)
{
  client->vt_enter_callback = callback;
  client->vt_enter_user_data = user_data;
}

void
vts_client_set_vt_leave_callback (VTSClient *client,
                                  VTSSwitchCallback callback,
                                  void *user_data)
{
  client->vt_leave_callback = callback;
  client->vt_leave_user_data = user_data;
}

int
vts_client_get_poll_fd (VTSClient *client)
{
  return client->fd;
}

static bool
client_handle_vt_switch (VTSClient *client,
                         struct msghdr *msg,
                         ssize_t len,
                         VTSError **error)
{
  VTSVTSwitchMessage *message;

  if (len != sizeof (*message))
    {
      set_error (error, 0, "Spurious VTSwitch message size");
      return FALSE;
    }

  message = msg->msg_iov->iov_base;

  if (message->direction == VTS_VT_SWITCH_DIRECTION_LEAVE)
    {
      if (client->vt_leave_callback)
        client->vt_leave_callback (client, client->vt_leave_user_data);
    }
  else
    {
      if (client->vt_enter_callback)
        client->vt_enter_callback (client, client->vt_enter_user_data);
    }

  return TRUE;
}

static bool
client_handle_message (VTSClient *client, VTSError **error)
{
  char control[CMSG_SPACE (sizeof (int))];
  char buf[BUFSIZ];
  struct msghdr msg;
  struct iovec iov;
  ssize_t len;
  VTSMessage *message;

  memset (&msg, 0, sizeof (msg));
  iov.iov_base = buf;
  iov.iov_len  = sizeof buf;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof control;

  do {
    len = recvmsg (client->fd, &msg, 0);
  } while (len < 0 && errno == EINTR);

  if (len < 1)
    {
      set_error_from_errno (error, "Failed to receive server message");
      return FALSE;
    }

  message = (void *) buf;
  switch (message->id)
    {
    case VTS_MESSAGE_ID_VT_SWITCH:
      return client_handle_vt_switch (client, &msg, len, error);
    default:
      set_error (error, 0, "Unknown message type received by client");
      return FALSE;
    }
}

bool
vts_client_dispatch (VTSClient *client, VTSError **error)
{
  while (1)
    {
      struct epoll_event ev;
      int n;

      do {
        n = epoll_wait (client->epollfd, &ev, 1, 0);
      } while (n < 0 && errno == EINTR);

      if (n < 0)
        vts_warning ("epoll_wait failed");

      if (n == 0)
        return TRUE;

      if (ev.data.fd == client->fd)
        {
          if (!client_handle_message (client, error))
            return FALSE;
        }
    }
}

static bool
send_simple_message (VTSClient *client,
                     VTSMessageID id,
                     VTSError **error)
{
  VTSMessage message;
  int len;

  message.id = id;

  do {
    len = send (client->fd, &message, sizeof message, 0);
  } while (len < 0 && errno == EINTR);

  if (len < sizeof message)
    {
      set_error_from_errno (error, "Failed to send message");
      return FALSE;
    }

  return TRUE;
}

bool
vts_client_accept_vt_leave (VTSClient *client, VTSError **error)
{
  return send_simple_message (client,
                              VTS_MESSAGE_ID_VT_LEAVE_ACCEPT,
                              error);
}

bool
vts_client_deny_vt_leave (VTSClient *client, VTSError **error)
{
  return send_simple_message (client,
                              VTS_MESSAGE_ID_VT_LEAVE_DENY,
                              error);
}

bool
vts_client_drm_set_master (VTSClient *client,
                           int drm_fd,
                           bool master,
                           VTSError **error)
{
  struct msghdr msg;
  struct cmsghdr *cmsg;
  struct iovec iov;
  char control[CMSG_SPACE (sizeof (drm_fd))];
  int ret;
  ssize_t len;
  VTSSetMasterMessage message;
  union cmsg_data *data;

  memset (&msg, 0, sizeof msg);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof control;
  cmsg = CMSG_FIRSTHDR (&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN (sizeof (drm_fd));

  data = (union cmsg_data *) CMSG_DATA (cmsg);
  data->fd = drm_fd;
  msg.msg_controllen = cmsg->cmsg_len;

  iov.iov_base = &message;
  iov.iov_len = sizeof message;

  message.header.id= VTS_MESSAGE_ID_SET_MASTER;
  message.set_master = master;

  do {
    len = sendmsg (client->fd, &msg, 0);
  } while (len < 0 && errno == EINTR);

  if (len < 0)
    {
      set_error_from_errno (error, "Failed to send SetMaster message");
      return FALSE;
    }

  do {
    len = recv (client->fd, &ret, sizeof ret, 0);
  } while (len < 0 && errno == EINTR);

  if (len < 0)
    {
      set_error (error, 0, "SetMaster request failed");
      return FALSE;
    }

  return TRUE;
}

int
vts_client_open_input_device (VTSClient *client,
                              const char *device,
                              int open_flags,
                              VTSError **error)
{
  int sock = client->fd;
  int n, ret = -1;
  struct msghdr msg;
  struct cmsghdr *cmsg;
  struct iovec iov;
  union cmsg_data *data;
  char control[CMSG_SPACE (sizeof data->fd)];
  ssize_t len;
  VTSOpenInputMessage *message;

  n = sizeof (*message) + strlen (device) + 1;
  message = malloc (n);
  if (!message)
    {
      set_error_from_errno (error, "Failed to allocate OpenInput message");
      return FALSE;
    }

  message->header.id = VTS_MESSAGE_ID_OPEN_INPUT;
  /* NB: The server will effectively  */
  message->flags = open_flags;
  strcpy (message->path, device);

  do {
    len = send (sock, message, n, 0);
  } while (len < 0 && errno == EINTR);

  if (len < 0)
    {
      set_error_from_errno (error, "Failed to send OpenInput message");
      return -1;
    }

  free (message);

  memset (&msg, 0, sizeof msg);
  iov.iov_base = &ret;
  iov.iov_len = sizeof ret;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof control;

  do {
    len = recvmsg (sock, &msg, MSG_CMSG_CLOEXEC);
  } while (len < 0 && errno == EINTR);

  if (len != sizeof ret || ret < 0)
    {
      set_error (error, 0, "Failed to retrieve OpenInput return value");
      return -1;
    }

  cmsg = CMSG_FIRSTHDR (&msg);
  if (!cmsg ||
      cmsg->cmsg_level != SOL_SOCKET ||
      cmsg->cmsg_type != SCM_RIGHTS)
    {
      set_error (error, 0, "Invalid control message");
      return -1;
    }

  data = (union cmsg_data *) CMSG_DATA (cmsg);
  if (data->fd == -1)
    {
      set_error (error, 0, "missing fd in OpenInput reply\n");
      return -1;
    }

  return data->fd;
}
