/*
 * Directory utility functions.
 *
 * Author:
 *   Gonzalo Paniagua Javier (gonzalo@novell.com)
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

#include <ulib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

struct _UDir {
	DIR *dir;
#ifndef HAVE_REWINDDIR
	char *path;
#endif
};

UDir *
u_dir_open (const char *path, unsigned int flags, UError **error)
{
	UDir *dir;

	u_return_val_if_fail (path != NULL, NULL);
	u_return_val_if_fail (error == NULL || *error == NULL, NULL);

	(void) flags; /* this is not used */
	dir = u_new (UDir, 1);
	dir->dir = opendir (path);
	if (dir->dir == NULL) {
		if (error) {
			int err = errno;
			*error = u_error_new (U_FILE_ERROR,
                                              u_file_error_from_errno (err),
                                              u_strerror (err));
		}
		u_free (dir);
		return NULL;
	}
#ifndef HAVE_REWINDDIR
	dir->path = u_strdup (path);
#endif
	return dir;
}

const char *
u_dir_read_name (UDir *dir)
{
	struct dirent *entry;

	u_return_val_if_fail (dir != NULL && dir->dir != NULL, NULL);
	do {
		entry = readdir (dir->dir);
		if (entry == NULL)
			return NULL;
	} while ((strcmp (entry->d_name, ".") == 0) || (strcmp (entry->d_name, "..") == 0));

	return entry->d_name;
}

void
u_dir_rewind (UDir *dir)
{
	u_return_if_fail (dir != NULL && dir->dir != NULL);
#ifndef HAVE_REWINDDIR
	closedir (dir->dir);
	dir->dir = opendir (dir->path);
#else
	rewinddir (dir->dir);
#endif
}

void
u_dir_close (UDir *dir)
{
	u_return_if_fail (dir != NULL && dir->dir != 0);
	closedir (dir->dir);
#ifndef HAVE_REWINDDIR
	u_free (dir->path);
#endif
	dir->dir = NULL;
	u_free (dir);
}

int
u_mkdir_with_parents (const char *pathname, int mode)
{
	char *path, *d;
	int rv;
	
	if (!pathname || *pathname == '\0') {
		errno = EINVAL;
		return -1;
	}
	
	d = path = u_strdup (pathname);
	if (*d == '/')
		d++;
	
	while (TRUE) {
		if (*d == '/' || *d == '\0') {
		  char orig = *d;
		  *d = '\0';
		  rv = mkdir (path, mode);
		  if (rv == -1 && errno != EEXIST) {
		  	u_free (path);
			return -1;
		  }

		  *d++ = orig;
		  while (orig == '/' && *d == '/')
		  	d++;
		  if (orig == '\0')
		  	break;
		} else {
			d++;
		}
	}
	
	u_free (path);
	
	return 0;
}
