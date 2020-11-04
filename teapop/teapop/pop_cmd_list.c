/* $Id: //depot/Teapop/0.3/teapop/pop_cmd_list.c#4 $ */

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

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "config.h"
#include "teapop.h"
#include "pop_cmds.h"
#include "pop_socket.h"
#include "pop_strings.h"

void
pop_cmd_list(arg, pinfo)
	char *arg;
	POP_INFO *pinfo;
{
	char *ep;
	unsigned long msg, counter;

	POP_MSG *curmsg;

	curmsg = pinfo->firstmsg;

	if (arg[0] != '\0') {
		/* Show stat for just one letter */
		errno = 0;	/* strtoul() do not set errno when successful */
		msg = strtoul(arg, &ep, 10);

		if (*ep != '\0') {
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_LIST_BAD);
			return;
		}

		if (errno == ERANGE && msg == ULONG_MAX) {
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_MSG_BIG);
#ifdef VPOP
			if (pinfo->domain[0] != '\0')
				syslog(LOG_ALERT,
				    "%s@%s[%s] is sending abnormal big numbers",
				    pinfo->userid, pinfo->domain,
				    pinfo->remoteip);
			else
#endif /* VPOP */
				syslog(LOG_ALERT,
				    "%s[%s] is sending abnormal big numbers",
				    pinfo->userid, pinfo->remoteip);
			return;
		}

		for (counter = 1UL; counter < msg && curmsg != NULL; counter++)
			curmsg = curmsg->nextmsg;
		if (curmsg == NULL) {
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_LIST_NOMSG);
			return;
		}
		if (curmsg->flags & MSG_DELETED) {
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_LIST_GONE);
			return;
		}
		pop_socket_send(pinfo->out, "%s %lu %lu", POP_OK, msg,
		    curmsg->size);
		return;
	}

	msg = 0UL;
	pop_socket_send(pinfo->out, "%s %s", POP_OK, POP_LIST_OK);
	while (curmsg != NULL) {
		msg++;
		if (!(curmsg->flags & MSG_DELETED))
			pop_socket_send(pinfo->out, "%lu %lu", msg,
			    curmsg->size);
		curmsg = curmsg->nextmsg;
	}
	pop_socket_send(pinfo->out, ".");
}
