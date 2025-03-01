/*
 * gutf8.c: UTF-8 conversion
 *
 * Author:
 *   Atsushi Enomoto  <atsushi@ximian.com>
 *
 * (C) 2006 Novell, Inc.
 * Copyright 2012 Xamarin Inc
 */


#include <config.h>

#include <stdio.h>
#include <ulib.h>

/*
 * Index into the table below with the first byte of a UTF-8 sequence to get
 * the number of bytes that are supposed to follow it to complete the sequence.
 *
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is left
 * as-is for anyone who may want to do such conversion, which was allowed in
 * earlier algorithms.
*/
const unsigned char u_utf8_jump_table[256] = {
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

static char *
utf8_case_conv (const char *str, ussize len, uboolean upper)
{
	uunichar *ustr;
	long i, ulen;
	char *utf8;
	
	ustr = u_utf8_to_ucs4_fast (str, (long) len, &ulen);
	for (i = 0; i < ulen; i++)
		ustr[i] = upper ? u_unichar_toupper (ustr[i]) : u_unichar_tolower (ustr[i]);
	utf8 = u_ucs4_to_utf8 (ustr, ulen, NULL, NULL, NULL);
	u_free (ustr);
	
	return utf8;
}

char *
u_utf8_strup (const char *str, ussize len)
{
	return utf8_case_conv (str, len, TRUE);
}

char *
u_utf8_strdown (const char *str, ussize len)
{
	return utf8_case_conv (str, len, FALSE);
}

static uboolean
utf8_validate (const unsigned char *inptr, size_t len)
{
	const unsigned char *ptr = inptr + len;
	unsigned char c;
	
	/* Everything falls through when TRUE... */
	switch (len) {
	default:
		return FALSE;
	case 4:
		if ((c = (*--ptr)) < 0x80 || c > 0xBF)
			return FALSE;
		
		if ((c == 0xBF || c == 0xBE) && ptr[-1] == 0xBF) {
			if (ptr[-2] == 0x8F || ptr[-2] == 0x9F ||
			    ptr[-2] == 0xAF || ptr[-2] == 0xBF)
				return FALSE;
		}
	case 3:
		if ((c = (*--ptr)) < 0x80 || c > 0xBF)
			return FALSE;
	case 2:
		if ((c = (*--ptr)) < 0x80 || c > 0xBF)
			return FALSE;
		
		/* no fall-through in this inner switch */
		switch (*inptr) {
		case 0xE0: if (c < 0xA0) return FALSE; break;
		case 0xED: if (c > 0x9F) return FALSE; break;
		case 0xEF: if (c == 0xB7 && (ptr[1] > 0x8F && ptr[1] < 0xB0)) return FALSE;
			if (c == 0xBF && (ptr[1] == 0xBE || ptr[1] == 0xBF)) return FALSE;
			break;
		case 0xF0: if (c < 0x90) return FALSE; break;
		case 0xF4: if (c > 0x8F) return FALSE; break;
		default:   if (c < 0x80) return FALSE; break;
		}
	case 1: if (*inptr >= 0x80 && *inptr < 0xC2) return FALSE;
	}
	
	if (*inptr > 0xF4)
		return FALSE;
	
	return TRUE;
}

/**
 * u_utf8_validate:
 * @str: a utf-8 encoded string
 * @max_len: max number of bytes to validate (or -1 to validate the entire null-terminated string)
 * @end: output parameter to mark the end of the valid input
 *
 * Checks @utf for being valid UTF-8. @str is assumed to be
 * null-terminated. This function is not super-strict, as it will
 * allow longer UTF-8 sequences than necessary. Note that Java is
 * capable of producing these sequences if provoked. Also note, this
 * routine checks for the 4-byte maximum size, but does not check for
 * 0x10ffff maximum value.
 *
 * Return value: %TRUE if @str is valid or %FALSE otherwise.
 **/
uboolean
u_utf8_validate (const char *str, ussize max_len, const char **end)
{
	unsigned char *inptr = (unsigned char *) str;
	uboolean valid = TRUE;
	unsigned int length, min;
	ussize n = 0;
	
	if (max_len == 0)
		return FALSE;
	
	if (max_len < 0) {
		while (*inptr != 0) {
			length = u_utf8_jump_table[*inptr];
			if (!utf8_validate (inptr, length)) {
				valid = FALSE;
				break;
			}
			
			inptr += length;
		}
	} else {
		while (n < max_len) {
			if (*inptr == 0) {
				/* Note: return FALSE if we encounter nul-byte
				 * before max_len is reached. */
				valid = FALSE;
				break;
			}
			
			length = u_utf8_jump_table[*inptr];
			min = MIN (length, max_len - n);
			
			if (!utf8_validate (inptr, min)) {
				valid = FALSE;
				break;
			}
			
			if (min < length) {
				valid = FALSE;
				break;
			}
			
			inptr += length;
			n += length;
		}
	}
	
	if (end != NULL)
		*end = (char *) inptr;
	
	return valid;
}

uunichar
u_utf8_get_char_validated (const char *str, ussize max_len)
{
	unsigned char *inptr = (unsigned char *) str;
	uunichar u = *inptr;
	int n, i;
	
	if (max_len == 0)
		return -2;
	
	if (u < 0x80) {
		/* simple ascii case */
		return u;
	} else if (u < 0xc2) {
		return -1;
	} else if (u < 0xe0) {
		u &= 0x1f;
		n = 2;
	} else if (u < 0xf0) {
		u &= 0x0f;
		n = 3;
	} else if (u < 0xf8) {
		u &= 0x07;
		n = 4;
	} else if (u < 0xfc) {
		u &= 0x03;
		n = 5;
	} else if (u < 0xfe) {
		u &= 0x01;
		n = 6;
	} else {
		return -1;
	}
	
	if (max_len > 0) {
		if (!utf8_validate (inptr, MIN (max_len, n)))
			return -1;
		
		if (max_len < n)
			return -2;
	} else {
		if (!utf8_validate (inptr, n))
			return -1;
	}
	
	for (i = 1; i < n; i++)
		u = (u << 6) | (*++inptr ^ 0x80);
	
	return u;
}

long
u_utf8_strlen (const char *str, ussize max_len)
{
	const unsigned char *inptr = (const unsigned char *) str;
	long clen = 0, len = 0, n;
	
	if (max_len == 0)
		return 0;
	
	if (max_len < 0) {
		while (*inptr) {
			inptr += u_utf8_jump_table[*inptr];
			len++;
		}
	} else {
		while (len < max_len && *inptr) {
			n = u_utf8_jump_table[*inptr];
			if ((clen + n) > max_len)
				break;
			
			inptr += n;
			clen += n;
			len++;
		}
	}
	
	return len;
}

uunichar
u_utf8_get_char (const char *src)
{
	unsigned char *inptr = (unsigned char *) src;
	uunichar u = *inptr;
	int n, i;
	
	if (u < 0x80) {
		/* simple ascii case */
		return u;
	} else if (u < 0xe0) {
		u &= 0x1f;
		n = 2;
	} else if (u < 0xf0) {
		u &= 0x0f;
		n = 3;
	} else if (u < 0xf8) {
		u &= 0x07;
		n = 4;
	} else if (u < 0xfc) {
		u &= 0x03;
		n = 5;
	} else {
		u &= 0x01;
		n = 6;
	}
	
	for (i = 1; i < n; i++)
		u = (u << 6) | (*++inptr ^ 0x80);
	
	return u;
}

char *
u_utf8_find_prev_char (const char *str, const char *p)
{
	while (p > str) {
		p--;
		if ((*p & 0xc0) != 0xb0)
			return (char *)p;
	}
	return NULL;
}

char *
u_utf8_prev_char (const char *str)
{
	const char *p = str;
	do {
		p--;
	} while ((*p & 0xc0) == 0xb0);
	
	return (char *)p;
}

char *
u_utf8_offset_to_pointer (const char *str, long offset)
{
	const char *p = str;

	if (offset > 0) {
		do {
			p = u_utf8_next_char (p);
			offset --;
		} while (offset > 0);
	}
	else if (offset < 0) {
		const char *jump = str;
		do {
			// since the minimum size of a character is 1
			// we know we can step back at least offset bytes
			jump = jump + offset;
			
			// if we land in the middle of a character
			// walk to the beginning
			while ((*jump & 0xc0) == 0xb0)
				jump --;
			
			// count how many characters we've actually walked
			// by going forward
			p = jump;
			do {
				p = u_utf8_next_char (p);
				offset ++;
			} while (p < jump);
			
		} while (offset < 0);
	}
	
	return (char *)p;
}

long
u_utf8_pointer_to_offset (const char *str, const char *pos)
{
	const char *inptr, *inend;
	long offset = 0;
	long sign = 1;
	
	if (pos == str)
		return 0;
	
	if (str < pos) {
		inptr = str;
		inend = pos;
	} else {
		inptr = pos;
		inend = str;
		sign = -1;
	}
	
	do {
		inptr = u_utf8_next_char (inptr);
		offset++;
	} while (inptr < inend);
	
	return offset * sign;
}
