/*
 * gmisc.c: Misc functions with no place to go (right now)
 *
 * Author:
 *   Aaron Bockover (abockover@novell.com)
 *
 * (C) 2006 Novell, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <config.h>
#include <stdlib.h>
#include <ulib.h>
#include <pthread.h>

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


const char *
u_getenv(const char *variable)
{
	return getenv(variable);
}

uboolean
u_setenv(const char *variable, const char *value, uboolean overwrite)
{
	return setenv(variable, value, overwrite) == 0;
}

void
u_unsetenv(const char *variable)
{
	unsetenv(variable);
}

char*
u_win32_getlocale(void)
{
	return NULL;
}

uboolean
u_path_is_absolute (const char *filename)
{
	u_return_val_if_fail (filename != NULL, FALSE);

	return (*filename == '/');
}

static pthread_mutex_t pw_lock = PTHREAD_MUTEX_INITIALIZER;
static const char *home_dir;
static const char *user_name;

static void
get_pw_data (void)
{
#ifdef HAVE_UETPWUID_R
	struct passwd pw;
	struct passwd *result;
	char buf [4096];
#endif

	if (user_name != NULL)
		return;

	pthread_mutex_lock (&pw_lock);
	if (user_name != NULL) {
		pthread_mutex_unlock (&pw_lock);
		return;
	}
#ifdef HAVE_UETPWUID_R
	if (getpwuid_r (getuid (), &pw, buf, 4096, &result) == 0) {
		home_dir = u_strdup (pw.pw_dir);
		user_name = u_strdup (pw.pw_name);
	}
#endif
	if (home_dir == NULL)
		home_dir = u_getenv ("HOME");

	if (user_name == NULL) {
		user_name = u_getenv ("USER");
		if (user_name == NULL)
			user_name = "somebody";
	}
	pthread_mutex_unlock (&pw_lock);
}

/* Uive preference to /etc/passwd than HOME */
const char *
u_get_home_dir (void)
{
	get_pw_data ();
	return home_dir;
}

const char *
u_get_user_name (void)
{
	get_pw_data ();
	return user_name;
}

static const char *tmp_dir;

static pthread_mutex_t tmp_lock = PTHREAD_MUTEX_INITIALIZER;

const char *
u_get_tmp_dir (void)
{
	if (tmp_dir == NULL){
		pthread_mutex_lock (&tmp_lock);
		if (tmp_dir == NULL){
			tmp_dir = u_getenv ("TMPDIR");
			if (tmp_dir == NULL){
				tmp_dir = u_getenv ("TMP");
				if (tmp_dir == NULL){
					tmp_dir = u_getenv ("TEMP");
					if (tmp_dir == NULL)
						tmp_dir = "/tmp";
				}
			}
		}
		pthread_mutex_unlock (&tmp_lock);
	}
	return tmp_dir;
}

