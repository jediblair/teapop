/* $Id: //depot/Teapop/0.3/teapop/pop_mbox.c#6 $ */

/*
 * Copyright (c) 1999-2003 ToonTown Consulting
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

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "config.h"
#include "teapop.h"
#include "pop_mbox.h"
#include "pop_socket.h"
#include "pop_strings.h"

#ifndef HAVE_MD5_H
#include "md5.h"
#else
#include <md5.h>
#endif

time_t pop_string_mktime(const char *);

void
pop_mbox_get_status(pinfo)
	POP_INFO *pinfo;
{
	char buf[1024], *ptr, *tmpstr;
	int doingline = 0, isimap = 0, gotuidl = 0, gotmsg = 0, inheader = 1;
	int counter, i;
	unsigned long lastftell;
	unsigned char digest[16];
	time_t timestamp;

	MD5_CTX	ctx;
	POP_MSG *curmsg, *lastmsg;

	/* If there is no mailbox don't waste any more time here */
	if (pinfo->mbox == NULL)
		return;

	/* Mostly for the future if we allow multiple mailboxes */
	if (pinfo->firstmsg == NULL)
		lastmsg = NULL;
	else {
		lastmsg = pinfo->firstmsg;
		while (lastmsg->nextmsg != NULL)
			lastmsg = lastmsg->nextmsg;
	}
	if ((curmsg = malloc(sizeof(POP_MSG))) == NULL) {
		syslog(LOG_CRIT, "memory problems");
		return;
	}
	memset(curmsg, 0, sizeof(POP_MSG));

	rewind(pinfo->mbox);
	lastftell = 0;

	while (fgets(buf, sizeof(buf), pinfo->mbox)) {
		/* Just to be sure nothing funny happens */
		if (feof(pinfo->mbox))
			break;

		/* Check if it's a new letter */
		if (!strncmp(buf, "From ", 5) && !doingline &&
		    ((timestamp = pop_string_mktime(buf)) != 0)) {
			MD5Final(digest, &ctx);
			if (gotmsg == 0 || (isimap == 1 &&
			    pinfo->ignoreimap == 1)) {
				/* It's UW-IMAPs message, ignore */
				isimap = 0;
				memset(curmsg, 0, sizeof(POP_MSG));
				curmsg->offset = lastftell =
				    (unsigned long)ftell(pinfo->mbox);
				curmsg->realsize = (unsigned long)strlen(buf);
				curmsg->som = (unsigned long)
				    (lastftell - curmsg->realsize);
				curmsg->created = timestamp;
				MD5Init(&ctx);
				MD5Update(&ctx, (unsigned char *)buf,
				    strlen(buf));
				gotmsg = 1;
				continue;
			}
			if (!(gotuidl == 1 && pinfo->useuidl == 1)) {
				ptr = curmsg->uidl;
				for (counter = 0;
				    counter < (int)sizeof(digest);
				    counter++, ptr += 2)
					sprintf(ptr, "%02x",
					    digest[counter] & 0xff);
				*ptr = '\0';
			}

			if (pinfo->firstmsg == NULL)
				pinfo->firstmsg = lastmsg = curmsg;
			else
				lastmsg = lastmsg->nextmsg = curmsg;

			if ((curmsg = malloc(sizeof(POP_MSG))) == NULL) {
				syslog(LOG_CRIT, "memory problems");
				return;
			}
			memset(curmsg, 0, sizeof(POP_MSG));

			curmsg->som = (unsigned long)lastftell;
			curmsg->offset = lastftell =
			    (unsigned long)ftell(pinfo->mbox);
			curmsg->realsize = (unsigned long)strlen(buf);
			curmsg->created = timestamp;
			inheader = 1;
			gotuidl = 0;
			MD5Init(&ctx);
			MD5Update(&ctx, (unsigned char *)buf, strlen(buf));
			continue;
		}

		/* Is this the first letter an UW-IMAPs message? */
		if (!doingline && pinfo->firstmsg == NULL &&
		    pinfo->ignoreimap == 1 && !strncmp(buf, "X-IMAP: ", 8))
			isimap = 1;

		/* Find all NULL characters and replace with questionmarks */
		i = strlen(buf);
		while ((i == 0 || buf[i-1] != '\n') && i < (sizeof(buf) - 1)) {
			buf[i] = '?';
			i = strlen(buf);
		}

		/*
		 * If line only ends with LF, we need to add one to the size
		 * since we will be sending CRLF to the client.
		 * Note we need to check that the number of characters in
		 * buf is high enough (found in i) to not cause buffer
		 * underflow.
		 */
		if ((i > 0 && buf[i-1] == '\n') && (i < 1 || buf[i-2] != '\r'))
			curmsg->size++;

		curmsg->size += strlen(buf);
		curmsg->realsize += strlen(buf);

		if (!doingline && gotuidl == 0 && pinfo->useuidl == 1 &&
		    inheader && !strncasecmp(buf, "X-UIDL: ", 8)) {
			strncpy(curmsg->uidl, &buf[8], sizeof(curmsg->uidl));
			/*
			 * westr: Make sure terminated by \0, then
			 * search for \n and terminate there instead if
			 * applicable.
			 */
			curmsg->uidl[sizeof(curmsg->uidl)-1] = '\0';
			tmpstr = strchr(curmsg->uidl, (int)'\n');
			if (tmpstr != NULL)
				tmpstr[0] = '\0';

			gotuidl = 1;
		}
		/*
		 * When calculating the UIDLs we need to skip some
		 * headers that might change in the message. Noteable
		 * (X-)Status, Lines and Content-Length.
		 */
		if (!doingline && gotuidl == 0 &&
		    (strncasecmp(buf, "Status: ", 8) &&
		    strncasecmp(buf, "X-Status: ", 10) &&
		    strncasecmp(buf, "Lines: ", 7) &&
		    strncasecmp(buf, "Content-Length: ", 15)))
			MD5Update(&ctx, (unsigned char *)buf, strlen(buf));

		if (!doingline && buf[0] == '\n')
			inheader = 0;

		if (buf[strlen(buf)-1] != '\n')
			doingline = 1;
		else {
			doingline = 0;
			curmsg->lines++;
		}
	}
	MD5Final(digest, &ctx);

	/* No mail or just one which is UW-IMAPs message */
	if ((isimap == 1 && pinfo->ignoreimap == 1) || gotmsg == 0) {
		free(curmsg);
		return;
	}

	if (!(gotuidl == 1 && pinfo->useuidl == 1)) {
		ptr = curmsg->uidl;
		for (counter = 0; counter < (int)sizeof(digest);
		    counter++, ptr += 2)
			sprintf(ptr, "%02x", digest[counter] & 0xff);
		*ptr = '\0';
	}

	if (pinfo->firstmsg == NULL)
		pinfo->firstmsg = lastmsg = curmsg;
	else
		lastmsg = lastmsg->nextmsg = curmsg;
}
