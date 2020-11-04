/* $Id: //depot/Teapop/0.3/teapop/pop_parse.c#4 $ */

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

#include <errno.h>
#include <stdio.h>
#include <syslog.h>

#include "config.h"
#include "teapop.h"
#include "pop_cmds.h"
#include "pop_lock.h"
#include "pop_socket.h"
#include "pop_strings.h"

int
pop_parse_cmds(pinfo)
	POP_INFO *pinfo;
{
	char buf[512];

	for (;;) {
		switch(pop_wait_for_commands(pinfo->timeout, sizeof(buf), buf,
		    "STAT", "LIST", "RETR", "DELE", "NOOP", "RSET", "TOP",
		    "UIDL", "LAST", "CAPA", "QUIT", NULL)) {
		case -1:/* Something went wrong */
			syslog(LOG_ERR, "Something went wrong. (%d)", errno);
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_WRONG);
			pop_unlock_maildrop();
			exit(0);
			/* NOTREACHED */
		case 0: /* Timeout occured waiting for input */
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_TIMEOUT);
			pop_unlock_maildrop();
			exit(0);
			/* NOTREACHED */
		case 1:	/* STAT */
			pop_cmd_stat(buf, pinfo);
			break;
		case 2:	/* LIST */
			pop_cmd_list(buf, pinfo);
			break;
		case 3:	/* RETR */
			pop_cmd_retr(buf, pinfo);
			break;
		case 4: /* DELE */
			pop_cmd_dele(buf, pinfo);
			break;
		case 5:	/* NOOP */
			pop_cmd_noop(buf, pinfo);
			break;
		case 6:	/* RSET */
			pop_cmd_rset(buf, pinfo);
			break;
		case 7: /* TOP */
			pop_cmd_top(buf, pinfo);
			break;
		case 8:	/* UIDL */
			pop_cmd_uidl(buf, pinfo);
			break;
		case 9:	/* LAST */
			pop_cmd_last(buf, pinfo);
			break;
		case 10:/* CAPA */
			pop_cmd_capa(buf, pinfo);
			break;
		case 11:/* QUIT */
			return (0);
		default:
			pop_socket_send(pinfo->out, "%s Not implemented yet.",
			    POP_ERR);
			break;
		}
	}
}
