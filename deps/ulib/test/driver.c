/*
 * ULib Unit Test Driver
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
#include "test.h"
#include "tests.h"

#include <stdio.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#ifndef DRIVER_NAME
#define DRIVER_NAME "EGlib"
#endif

typedef struct _StringArray {
	char **strings;
	int length;
} StringArray;

static StringArray *
string_array_append(StringArray *array, char *string)
{
	if(array == NULL) {
		array = u_new0(StringArray, 1);
		array->length = 1;
		array->strings = u_malloc(sizeof(char *) * 2);
	} else {
		array->length++;
		array->strings = u_realloc(array->strings, sizeof(char *) 
			* (array->length + 1));
	}
	
	array->strings[array->length - 1] = string;
	array->strings[array->length] = NULL;

	return array;
}

int global_passed = 0, global_tests = 0;

static void
string_array_free(StringArray *array)
{
	u_free(array->strings);
	u_free(array);
}

static void print_help(char *s)
{
	int i;
	
	printf("Usage: %s [OPTION]... [TESTGROUP]...\n\n", s);
	printf("OPTIONS are:\n");
	printf("  -h, --help          show this help\n");
	printf("  -t, --time          time the tests\n");
	printf("  -i, --iterations    number of times to run tests\n");
	printf("  -q, --quiet         do not print test results; "
		"final time always prints\n");
	printf("  -n, --no-labels     print final time without labels, "
		"nice for scripts\n");
	printf("  -d, --debug         do not run tests, "
		"debug the driver itself for valgrind\n\n");
	printf("TESTGROUPS available:\n");

	for(i = 0; test_groups[i].name != NULL; i++) {
		if(test_groups[i].handler != fake_tests_init) {
			printf("  %s\n", test_groups[i].name);
		}
	}

	printf("\n");
}

int main(int argc, char **argv)
{
	int i, j, c, iterations = 1;
	StringArray *tests_to_run = NULL;
	double time_start;
	uboolean report_time = FALSE;
	uboolean quiet = FALSE;
	uboolean global_failure = FALSE;
	uboolean no_final_time_labels = FALSE;
	uboolean debug = FALSE;

#if HAVE_GETOPT_H
	static struct option long_options [] = {
		{"help",       no_argument,       0, 'h'},
		{"time",       no_argument,       0, 't'},
		{"quiet",      no_argument,       0, 'q'},
		{"iterations", required_argument, 0, 'i'},
		{"debug",      no_argument,       0, 'd'},
		{"no-labels",  no_argument,       0, 'n'},
		{0, 0, 0, 0}
	};

	while((c = getopt_long(argc, argv, "dhtqni:", long_options, NULL)) != -1) {			switch(c) {
			case 'h':
				print_help(argv[0]);
				return 1;
			case 't':
				report_time = TRUE;
				break;
			case 'i':
				iterations = atoi(optarg);
				break;
			case 'q':
				quiet = TRUE;
				break;
			case 'n':
				no_final_time_labels = TRUE;
				break;
			case 'd':
				debug = TRUE;
				break;
		}
	}

	for(i = optind; i < argc; i++) {
		if(argv[i][0] == '-') {
			continue;
		}

		tests_to_run = string_array_append(tests_to_run, argv[i]);
	}
#endif

	time_start = get_timestamp();
	
	for(j = 0; test_groups[j].name != NULL; j++) {
		uboolean run = TRUE;
		char *tests = NULL;
		char *group = NULL;
		
		if(tests_to_run != NULL) {
			int k;
			run = FALSE;
			
			for(k = 0; k < tests_to_run->length; k++) {	
				char *user = tests_to_run->strings[k];
				const char *table = test_groups[j].name;
				size_t user_len = strlen(user);
				size_t table_len = strlen(table);
				
				if(strncmp(user, table, table_len) == 0) {
					if(user_len > table_len && user[table_len] != ':') {
						break;
					}
					
					run = TRUE;
					group = tests_to_run->strings[k];
					break;
				}
			}
		}
	
		if(run) {
			uboolean passed;
			char **split = NULL;
			
			if(debug && test_groups[j].handler != fake_tests_init) {
				printf("Skipping %s, in driver debug mode\n", 
					test_groups[j].name);
				continue;
			} else if(!debug && test_groups[j].handler == fake_tests_init) {
				continue;
			}

			if(group != NULL) {
				split = eg_strsplit(group, ":", -1);	
				if(split != NULL) {
					int m;
					for(m = 0; split[m] != NULL; m++) {
						if(m == 1) {
							tests = strdup(split[m]);
							break;
						}
					}
					eg_strfreev(split);
				}
			}
			
			passed = run_group(&(test_groups[j]), 
				iterations, quiet, report_time, tests);

			if(tests != NULL) {
				u_free(tests);
			}

			if(!passed && !global_failure) {
				global_failure = TRUE;
			}
		}
	}
	
	if(!quiet) {
		double pass_percentage = ((double)global_passed / (double)global_tests) * 100.0;
		printf("=============================\n");
		printf("Overall result: %s : %d / %d (%g%%)\n", global_failure ? "FAILED" : "OK", global_passed, global_tests, pass_percentage);
	}
	
	if(report_time) {
		double duration = get_timestamp() - time_start;
		if(no_final_time_labels) {
			printf("%g\n", duration);
		} else {
			printf("%s Total Time: %g\n", DRIVER_NAME, duration);
		}
	}

	if(tests_to_run != NULL) {
		string_array_free(tests_to_run);
	}

	return global_tests - global_passed;
}


