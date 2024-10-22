#include <config.h>
#include <ulib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef U_OS_UNIX
#include <pthread.h>
#endif
#include "test.h"

/* This test is just to be used with valgrind */
RESULT
test_dir ()
{
	UDir *dir;
	UError *error;
	const char *name;

	/*
	dir = u_dir_open (NULL, 0, NULL);
	*/
	dir = u_dir_open ("", 0, NULL);
	if (dir != NULL)
		return FAILED ("1 Should be an error");

	dir = u_dir_open ("", 9, NULL);
	if (dir != NULL)
		return FAILED ("2 Should be an error");

	error = NULL;
	dir = u_dir_open (".ljasdslakjd", 9, &error);
	if (dir != NULL)
		return FAILED ("3 opendir should fail");
	if (error == NULL)
		return FAILED ("4 got no error");
	u_error_free (error);
	error = NULL;
	dir = u_dir_open (u_get_tmp_dir (), 9, &error);
	if (dir == NULL)
		return FAILED ("5 opendir should succeed");
	if (error != NULL)
		return FAILED ("6 got an error");
	name = NULL;
	name = u_dir_read_name (dir);
	if (name == NULL)
		return FAILED ("7 didn't read a file name");
	while ((name = u_dir_read_name (dir)) != NULL) {
		if (strcmp (name, ".") == 0)
			return FAILED (". directory found");
		if (strcmp (name, "..") == 0)
			return FAILED (".. directory found");
	}
	u_dir_close (dir);
	return OK;
}

static Test dir_tests [] = {
	{"u_dir_*", test_dir},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(dir_tests_init, dir_tests)


