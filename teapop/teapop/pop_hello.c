/* $Id: //depot/Teapop/0.3/teapop/pop_hello.c#5 $ */

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
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "teapop.h"
#include "version.h"
#include "pop_strings.h"
#include "pop_socket.h"

void
pop_send_hello(pinfo)
	POP_INFO *pinfo;
{
#ifdef ALLOW_APOP
	char tmpstr[BIGSTRING];

#ifndef HAVE_ARC4
	srandom((unsigned int)getpid());
#endif

	(void)snprintf(tmpstr, sizeof(tmpstr)-1, "<%ld.%lX@%s>",
	    (long)time(NULL),
#ifdef HAVE_ARC4
	    (long)arc4random(),
#else
	    (long)random(),
#endif
	    POP_APOP_HOST);
	(void)strcpy(pinfo->apopstr, tmpstr);
#else
	(void)strcpy(pinfo->apopstr, "");
#endif /* ALLOW_APOP */

	pop_socket_send(pinfo->out, "%s Teapop%s [%s] - %s %s", POP_OK,
	    ((pinfo->ssl != 0) ? "SSL" : ""),
	    POP_VERSION, POP_BANNER, pinfo->apopstr);
}
