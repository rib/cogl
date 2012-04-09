/*
 * Example test program that just uses the tokenization and
 * preprocessing phases, and prints out the results.
 *
 * Copyright (C) 2003 Transmeta Corp.
 *               2003 Linus Torvalds
 *
 *  Licensed under the Open Software License version 1.1
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include "lib.h"
#include "token.h"
#include "allocate.h"

int main(int argc, char **argv)
{
	struct string_list *filelist = NULL;
	char *file;

	keep_comment_tokens = 1;
	sparse_initialize(argc, argv, &filelist);
	FOR_EACH_PTR_NOTAG(filelist, file) {
		struct token *token = tokenize_keep_tokens(file);
		for (token = preprocess(token);
		     !eof_token(token);
		     token = token->next)
		{
			if (token_type(token) == TOKEN_COMMENT) {
				const char *comment = token->comment->data;
				if (strncmp (comment, "/**\n", 4) == 0)
					printf("%s\n%c", token->comment->data, '\0');

			}
		}
		clear_token_alloc();
	} END_FOR_EACH_PTR_NOTAG(file);
	return 0;
}
