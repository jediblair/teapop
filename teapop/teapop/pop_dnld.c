/* $Id: //depot/Teapop/0.3/teapop/pop_dnld.c#7 $ */

/*
 * Copyright (c) 1999-2002 ToonTown Consulting
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

#include "config.h"
#include "teapop.h"
#include "pop_dnld.h"
#include "pop_socket.h"
#include "pop_strings.h"

void
pop_dnldmsg(pinfo, msg, lines)
	POP_INFO *pinfo;
	unsigned long msg, lines;
{
	unsigned long counter;
	register unsigned long linesleft;
	register unsigned int addcr, len;
	char buf[1024];

	POP_MSG *curmsg;

	curmsg = pinfo->firstmsg;
	for (counter = 1UL; counter < msg && curmsg != NULL; counter++)
		curmsg = curmsg->nextmsg;

	/*
	 * If user requested message 0 or a message that doesn't exist,
	 * whine some and bail out message downloading.
	 */
	if (curmsg == NULL || msg <= 0UL) {
		pop_socket_send(pinfo->out, "%s %s", POP_ERR, POP_MSG_NONE);
		return;
	}

	/*
	 * If the user already flagged the message for deletion, whine
	 * and then bail out the message downloading.
	 */
	if (curmsg->flags & MSG_DELETED) {
		pop_socket_send(pinfo->out, "%s %s", POP_ERR, POP_MSG_GONE);
		return;
	}

	/*
	 * mbox and Maildir use different ways to get to the message
	 * that the user wants to download (duh), but the actual sending
	 * is the same. Here we find the message...
	 */
	if (pinfo->mboxtype == 0) {
		/* mbox */
		if ((fseek(pinfo->mbox,(long)curmsg->offset,SEEK_SET)) != 0) {
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_PROBLEM);
			return;
		}
	} else {
		/* Maildir */
		if ((pinfo->mbox = fopen(curmsg->file, "r")) == NULL) {
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_PROBLEM);
			return;
		}
	}

	/* Tell user the message is coming now */
	pop_socket_send(pinfo->out, "%s %lu", POP_OK, curmsg->size);

	/* Print Headers */
	linesleft = curmsg->lines;
	while (fgets(buf, sizeof(buf), pinfo->mbox)) {
		if (linesleft <= 0UL)
			break;
		len = strlen(buf);
		/* To read CRLF files like LF only files**/
		if ((len >= 2) && (buf[len-2] == '\r') && (buf[len-1] == '\n')) {
		    buf[len-2] = '\n';
		    buf[len-1] = '\0';
		    len = len-1;
		}
		/* If there's a NULL in the message, replace it with a ? */
		if (buf[len-1] != '\n' && len != (sizeof(buf)-1)) {
			buf[len] = '?';
			len = strlen(buf);
		}

		if (buf[len-1] == '\n') {
			addcr = 1;
			buf[len-1] = '\0';
		} else
			addcr = 0;

		/*
		 * If line starts with the termination octet, a dot,
		 * send a dot first, as per RFC, followed by the line.
		 */
		if (buf[0] == '.')
			(void)fputs(".", pinfo->out);
		pop_socket_rawsend(pinfo->out, buf);
		if (addcr == 1) {
			pop_socket_rawsend(pinfo->out, "\r\n");
			linesleft--;
		}
		if (buf[0] == '\0')
			break;
	}

	if (lines != 0UL && linesleft >= lines)
		linesleft = lines - 1UL;
	else if (lines == 0UL || pinfo->autodelete == 2)
		curmsg->flags = curmsg->flags | MSG_READ;

	/* Now print body */
	while (fgets(buf, sizeof(buf), pinfo->mbox)) {
		if (linesleft <= 0UL)
			break;
		len = strlen(buf);
		/* To read CRLF files like LF only files**/
		if ((len >= 2) && (buf[len-2] == '\r') && (buf[len-1] == '\n')) {
		    buf[len-2] = '\n';
		    buf[len-1] = '\0';
		    len = len-1;
		}
		/* If there's a NULL in the message, replace it with a ? */
		if (buf[len-1] != '\n' && len != (sizeof(buf)-1)) {
			buf[len] = '?';
			len = strlen(buf);
		}
		if (buf[len-1] == '\n') {
			addcr = 1;
			buf[len-1] = '\0';
		} else
			addcr = 0;
		/*
		 * If line starts with the termination octet, a dot,
		 * send a dot first, as per RFC, followed by the line.
		 */
		if (buf[0] == '.')
			(void)fputs(".", pinfo->out);
		pop_socket_rawsend(pinfo->out, buf);
		if (addcr == 1) {
			pop_socket_rawsend(pinfo->out, "\r\n");
			linesleft--;
		}
	}

	pop_socket_send(pinfo->out, ".");
	(void)fflush(pinfo->out);

	/* If we are using Maildir, close the message. */
	if (pinfo->mboxtype == 1)
		(void)fclose(pinfo->mbox);
}
