/* $Id: //depot/Teapop/0.3/teapop/pop_popsmtp_whoson.c#5 $ */

/*
 * Copyright (c) 2001-2003 ToonTown Consulting
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
#include <syslog.h>
#include <string.h>

#include "config.h"
#include "teapop.h"

#include <whoson.h>

/*
 */
void
pop_pop_before_smtp_whoson(pinfo)
	POP_INFO *pinfo;
{
	int retval;
	char err[256];
	char *name;

	memset(err, 0, sizeof(err));
	if ((name = malloc((strlen(pinfo->userid) + strlen(pinfo->domain) + 5)
	    * sizeof(char))) == NULL) {
		syslog(LOG_CRIT, "memory problems");
		return;
	}
	strcpy(name, pinfo->userid);
#ifdef VPOP
	if (pinfo->domain[0] != '\0') {
		strcat(name, "@");
		strcat(name, pinfo->domain);
	}
#endif /* VPOP */
	retval = wso_login(pinfo->remoteip, name, err, 200);
	if (retval < 0)
		syslog(LOG_ERR, "Problems with Whoson: %s", err);
	free(name);
}
