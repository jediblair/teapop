/* $Id: //depot/Teapop/0.3/teapop/pop_cmd_retr.c#4 $ */

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
#include <syslog.h>

#include "config.h"
#include "teapop.h"
#include "pop_cmds.h"
#include "pop_dnld.h"
#include "pop_socket.h"
#include "pop_strings.h"

void
pop_cmd_retr(arg, pinfo)
	char *arg;
	POP_INFO *pinfo;
{
	char *ep;
	unsigned long msg;

	errno = 0;		/* strtoul() do not set errno when successful */
	msg = strtoul(arg, &ep, 10);

	if (arg[0] == '\0' || *ep != '\0') {
		pop_socket_send(pinfo->out, "%s %s", POP_ERR, POP_RETR_BAD);
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

	pop_dnldmsg(pinfo, msg, 0UL);
}
