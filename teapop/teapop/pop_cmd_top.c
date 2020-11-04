/* $Id: //depot/Teapop/0.3/teapop/pop_cmd_top.c#5 $ */

/*
 * Copyright (c) 2001-2002 ToonTown Consulting
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
#include <string.h>
#include <syslog.h>

#include "config.h"
#include "teapop.h"
#include "pop_cmds.h"
#include "pop_dnld.h"
#include "pop_socket.h"
#include "pop_strings.h"

void
pop_cmd_top(arg, pinfo)
	char *arg;
	POP_INFO *pinfo;
{
	char *ep, *ep2, *ptr;
	unsigned long msg, lines;

	/* Make sure we have at least two arguments */
	if ((ptr = strchr(arg, ' ')) == NULL) {
		pop_socket_send(pinfo->out, "%s %s", POP_ERR, POP_TOP_BAD);
		return;
	}
	*ptr++ = '\0';

	errno = 0;		/* strtoul() do not set errno when successful */

	msg = strtoul(arg, &ep, 10);
	lines = strtoul(ptr, &ep2, 10);

	if (arg[0] == '\0' || *ep != '\0' || ptr[0] == '\0' || *ep2 != '\0') {
		pop_socket_send(pinfo->out, "%s %s", POP_ERR, POP_TOP_BAD);
		return;
	}

	if (errno == ERANGE && msg == ULONG_MAX) {
		pop_socket_send(pinfo->out, "%s %s", POP_ERR, POP_MSG_BIG);
#ifdef VPOP
		if (pinfo->domain[0] != '\0')
			syslog(LOG_ALERT,
			    "%s@%s[%s] is sending abnormal big numbers",
			    pinfo->userid, pinfo->domain, pinfo->remoteip);
		else
#endif /* VPOP */
			syslog(LOG_ALERT,
			    "%s[%s] is sending abnormal big numbers",
			    pinfo->userid, pinfo->remoteip);
		return;
	}

	/*
	 * We need to add one to the number of lines since pop_dnldmsg()
	 * will show one line less than it's asked to. This is so 0 will
	 * be treated as the whole email. 1 will mean only headers and
	 * nothing of the email.
	 *
	 * Note that counting lines up by one can cause it to start over
	 * from 0, if it's set to max (ULONG_MAX). This is ok since it will
	 * mean that the whole letter will be shown then, and no letter can
	 * have more lines then ULONG_MAX anyway. So it will return what the
	 * user is expecting, anyway.
	 */
	lines++;
	pop_dnldmsg(pinfo, msg, lines);
}
