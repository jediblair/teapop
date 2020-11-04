/* $Id: //depot/Teapop/0.3/teapop/pop_cmd_capa.c#5 $ */

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

#include "config.h"
#include "teapop.h"
#include "pop_cmds.h"
#include "pop_socket.h"
#include "pop_strings.h"
#include "version.h"

void
pop_cmd_capa(arg, pinfo)
	char *arg;
	POP_INFO *pinfo;
{
	if (arg[0] != '\0') {
		pop_socket_send(pinfo->out, "%s %s", POP_ERR, POP_CAPA_BAD);
		return;
	}

	pop_socket_send(pinfo->out, "%s %s", POP_OK, POP_CAPA_OK);
	pop_socket_send(pinfo->out, "TOP");
	pop_socket_send(pinfo->out, "USER");
	pop_socket_send(pinfo->out, "LOGIN-DELAY 900");
	pop_socket_send(pinfo->out, "UIDL");
	/*
	 * The EXPIRE response must always be the lowest value we can
	 * guarantee a message is retained. This means that if we are
	 * running with -d/-D the response will be 0, even if there is
	 * a -w specified.
	 */
	if (pinfo->autodelete != 0)
		pop_socket_send(pinfo->out, "EXPIRE 0");
	else if (pinfo->expire != 0)
		pop_socket_send(pinfo->out, "EXPIRE %d", pinfo->expire);
	else
		pop_socket_send(pinfo->out, "EXPIRE NEVER");
	pop_socket_send(pinfo->out, "IMPLEMENTATION Teapop%s-%s",
	     ((pinfo->ssl != 0) ? "SSL" : ""), POP_VERSION);
	pop_socket_send(pinfo->out, ".");
}
