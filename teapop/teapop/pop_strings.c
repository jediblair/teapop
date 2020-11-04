/* $Id: //depot/Teapop/0.3/teapop/pop_strings.c#5 $ */

/*
 * Copyright (c) 2000-2003 ToonTown Consulting
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "teapop.h"
#include "pop_strings.h"

/*
 * Function : pop_string_find
 * Arguments: const char *string, *tok
 * Returns  : Pointer to where first token character is found in string
 */
char *
pop_string_find(string, tok)
const char *string, *tok;
{
	char *buf, *ptr;
	unsigned int i;

	buf = (char *)&string[strlen(string)];

	for (i = 0; i < (unsigned int)strlen(tok); i++) {
		ptr = strchr(string, tok[i]);
		if ((ptr != NULL) && (ptr < buf))
			buf = ptr;
	}

	return (buf);
}

/*
 * Function : pop_string_mktime
 * Arguments: const char *buf
 * Returns  : A unix timestamp or 0 on error
 */
time_t
pop_string_mktime(buf)
const char *buf;
{
	int i;
	const char *pdate;

	struct tm foo;

	pdate = buf;
	memset(&foo, 0, sizeof(foo));

	/* The date starts after the second space */
	for (i = 0; i < 2; i++) {
		if ((pdate = strchr(pdate, ' ')) == NULL)
			return (0);
		pdate++;
	}

	/* Remove all leading spaces */
	while (*pdate == ' ')
		pdate++;

	/* First three characters are weekday; we can skip those */
	pdate += 4;

	/* Next three is month */
	if (!strncmp("Jan ", pdate, 4))
		foo.tm_mon = 0;
	else if (!strncmp("Feb ", pdate, 4))
		foo.tm_mon = 1;
	else if (!strncmp("Mar ", pdate, 4))
		foo.tm_mon = 2;
	else if (!strncmp("Apr ", pdate, 4))
		foo.tm_mon = 3;
	else if (!strncmp("May ", pdate, 4))
		foo.tm_mon = 4;
	else if (!strncmp("Jun ", pdate, 4))
		foo.tm_mon = 5;
	else if (!strncmp("Jul ", pdate, 4))
		foo.tm_mon = 6;
	else if (!strncmp("Aug ", pdate, 4))
		foo.tm_mon = 7;
	else if (!strncmp("Sep ", pdate, 4))
		foo.tm_mon = 8;
	else if (!strncmp("Oct ", pdate, 4))
		foo.tm_mon = 9;
	else if (!strncmp("Nov ", pdate, 4))
		foo.tm_mon = 10;
	else if (!strncmp("Dec ", pdate, 4))
		foo.tm_mon = 11;
	else
		return (0);
	pdate += 4;

	/* Next two characters is day of month */
	foo.tm_mday = atoi(pdate);
	if (foo.tm_mday < 1 || foo.tm_mday > 31)
		return (0);
	pdate += 3;

	/* Next two characters is hour */
	foo.tm_hour = atoi(pdate);
	if (foo.tm_hour < 0 || foo.tm_hour > 23)
		return (0);
	pdate += 3;

	/* Next two characters is minute */
	foo.tm_min = atoi(pdate);
	if (foo.tm_min < 0 || foo.tm_hour > 59)
		return (0);
	pdate += 3;

	/* Next two is second */
	foo.tm_sec = atoi(pdate);
	if (foo.tm_sec < 0 || foo.tm_sec > 59)
		return (0);
	pdate += 3;

	/* Next four is the year */
	foo.tm_year = atoi(pdate) - 1900;
	if (foo.tm_year < 70)
		return (0);
	pdate += 4;

	/* pdate should be empty if we had a proper From line */
	/* We don't, but could, check and return 0 if not so */

	return (mktime(&foo));
}

/*
 * Function : pop_string_dtot
 * Arguments: const char *buf
 * Returns  : An int or 0 on error
 */

int
pop_string_dtot(buf)
const char *buf;
{
	const char *ptr;
	int days, number;

	days = 0;
	ptr = buf;

	while (*ptr) {
		/* First get the digit */
		number = atoi(ptr);

		/* Then move the pointer to the token */
		while (isdigit((int)*ptr))
			ptr++;

		/* Use token to count up variable days */
		switch (*ptr) {
		case 'D':
		case 'd':
			days += number;
			break;
		case 'W':
		case 'w':
			days += 7 * number;
			break;
		case 'M':
		case 'm':
			days += 30 * number;
			break;
		case 'Y':
		case 'y':
			days += 365 * number;
			break;
		default:
			return (0);
		}
		/* Move pointer to character after token */
		ptr++;
	}

	return (days);
}
