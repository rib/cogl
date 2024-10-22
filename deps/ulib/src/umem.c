/*
 * gmem.c: memory utility functions
 *
 * Author:
 * 	Gonzalo Paniagua Javier (gonzalo@novell.com)
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

#include <stdio.h>
#include <string.h>
#include <ulib.h>

void
u_free (void *ptr)
{
	if (ptr != NULL)
		free (ptr);
}

void *
u_memdup (const void * mem, unsigned int byte_size)
{
	void * ptr;

	if (mem == NULL)
		return NULL;

	ptr = u_malloc (byte_size);
	if (ptr != NULL)
		memcpy (ptr, mem, byte_size);

	return ptr;
}

void * u_realloc (void * obj, size_t size)
{
	void * ptr;
	if (!size) {
		u_free (obj);
		return 0;
	}
	ptr = realloc (obj, size);
	if (ptr)
		return ptr;
	u_error ("Could not allocate %i bytes", size);
}

void * 
u_malloc (size_t x) 
{ 
	void * ptr;
	if (!x)
		return 0;
	ptr = malloc (x);
	if (ptr) 
		return ptr;
	u_error ("Could not allocate %i bytes", x);
}

void * u_malloc0 (size_t x) 
{ 
	void * ptr; 
	if (!x) 
		return 0; 
	ptr = calloc(1,x); 
	if (ptr) 
		return ptr; 
	u_error ("Could not allocate %i bytes", x);
}

void * u_try_malloc (size_t x) 
{
	if (x)
		return malloc (x);
	return 0;
}


void * u_try_realloc (void * obj, size_t size)
{ 
	if (!size) {
		u_free (obj);
		return 0;
	} 
	return realloc (obj, size);
}
