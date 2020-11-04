/* $Id: //depot/Teapop/0.3/teapop/pop_auth.c#6 $ */

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
#include <syslog.h>
#include <unistd.h>

#include "config.h"
#include "teapop.h"
#include "pop_auth.h"
#include "pop_cmds.h"
#include "pop_hello.h"
#include "pop_passwd.h"
#include "pop_socket.h"
#include "pop_strings.h"

int
pop_auth (pinfo)
	POP_INFO  *pinfo;
{
	int attempts, retval;
#ifdef ALLOW_APOP
	int isapop=0;
#endif
	char passwd[BIGSTRING];
#if defined(ALLOW_APOP) || defined(VPOP)
	char *ptr;
#endif

	pop_send_hello(pinfo);

	/* Loop until we get a good USER or APOP */
	for (attempts = 0; attempts < MAXTRIES; attempts++) {
		for (;;) {
#ifdef ALLOW_APOP
			isapop = 0;
#endif
			retval = pop_wait_for_commands(pinfo->timeout,
			    sizeof(pinfo->userid), pinfo->userid,
			    "USER", "CAPA", "QUIT",
#ifdef ALLOW_APOP
			    "APOP",
#endif
			    NULL);
			switch(retval) {
			case -1:
				syslog(LOG_ERR, "Something went wrong.");
				pop_socket_send(pinfo->out, "%s %s", POP_ERR,
				    POP_WRONG);
				exit(0);
				/* NOTREACHED */
			case 0:
				pop_socket_send(pinfo->out, "%s %s", POP_ERR,
				    POP_TIMEOUT);
				exit(0);
				/* NOTREACHED */
			case 1: /* USER */
				if (strlen(pinfo->userid) == 0) {
					pop_socket_send(pinfo->out,
					    "%s %s", POP_ERR, POP_USER_ARGS);
					continue;
				}
				pop_socket_send(pinfo->out, "%s %s", POP_OK,
				    POP_USER_OK);
                                break;
			case 2:	/* CAPA */
				pop_cmd_capa(pinfo->userid, pinfo);
				continue;
			case 3:
				return (1);
#ifdef ALLOW_APOP
			case 4:
				ptr = strchr(pinfo->userid, ' ');
				if (ptr == NULL) {
					pop_socket_send(pinfo->out,
					    "%s %s", POP_ERR, POP_APOP_BAD);
					continue;
				}
				ptr++;
				strcpy(passwd, ptr);
				*(--ptr) = '\0';
				isapop = 1;
				break;
#endif /* ALLOW_APOP */
			}
			/* If we get this far we have a good USER or APOP */
			break;
		}
		
#ifdef ALLOW_APOP
		/* Following loop is skipped if we got APOP insted of USER */
		for(; isapop == 0;) {
#else
		for(;;) {
#endif
			retval = pop_wait_for_commands(pinfo->timeout,
			    sizeof(passwd), passwd, "PASS", "CAPA", "QUIT",
			    NULL);
			switch(retval) {
			case -1:
				syslog(LOG_ERR, "Something went wrong.");
				pop_socket_send(pinfo->out, "%s %s", POP_ERR,
				    POP_WRONG);
				exit(0);
				/* NOTREACHED */
			case 0:
				pop_socket_send(pinfo->out, "%s %s", POP_ERR,
				    POP_TIMEOUT);
				exit(0);
				/* NOTREACHED */
			case 1:
				if (strlen(passwd) == 0) {
					pop_socket_send(pinfo->out, "%s %s",
					    POP_ERR, POP_PASS_ARGS);
                                }
				break;
			case 2:
				pop_cmd_capa(NULL, pinfo);
				continue;
			case 3:
				return (1);
			}
			break;
		}

		/* Help dumb users who insist on adding spaces */
		while(pinfo->userid[strlen(pinfo->userid)-1] == ' ')
			pinfo->userid[strlen(pinfo->userid)-1] = '\0';

#ifdef VPOP
		ptr = pop_string_find(pinfo->userid, DIVIDERS);
		if (ptr == NULL) {
			syslog(LOG_CRIT, "memory problems");
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_WRONG);
			exit(0);
		}
		if (*ptr != '\0') {
			/* domain delimiters found */
			strcpy(pinfo->domain, ptr+1);
			*ptr = '\0';
		} else
			pinfo->domain[0] = '\0';
#endif
		retval = pop_verify_password(pinfo, passwd,
#ifdef ALLOW_APOP
		    isapop);
#else
		    0);
#endif
		if (retval == 0) {
#ifdef VPOP
			if (pinfo->domain[0] != '\0')
				syslog(LOG_INFO,
				    "Successful login for %s@%s [%s] from %s [%s]",
				    pinfo->userid, pinfo->domain,
				    pinfo->localip, pinfo->remotehost,
				    pinfo->remoteip);
			else
#endif /* VPOP */
				syslog(LOG_INFO,
				    "Successful login for %s [%s] from %s [%s]",
				    pinfo->userid, pinfo->localip,
				    pinfo->remotehost, pinfo->remoteip);
			return (0);
		}

		sleep(3);
		pop_socket_send(pinfo->out, "%s %s", POP_ERR, POP_PASS_BAD);
#ifdef VPOP
		if (pinfo->domain[0] != '\0')
			syslog(LOG_WARNING,
			    "Failed login for %s@%s [%s] from %s [%s]",
			    pinfo->userid, pinfo->domain, pinfo->localip,
			    pinfo->remotehost, pinfo->remoteip);
		else
#endif
			syslog(LOG_WARNING,
			    "Failed login for %s [%s] from %s [%s]",
			    pinfo->userid, pinfo->localip, pinfo->remotehost,
			    pinfo->remoteip);
	}

	/*
         * If we get this far, the user has had enough tries sending a
         * valid USER/PASS (or APOP if enabled). Return with errorlevel
         * saying it's ok to drop the connection.
         */
	syslog(LOG_WARNING, "Three failed login attempts from %s [%s] - "
	    "Possible account probe", pinfo->remotehost, pinfo->remoteip);

	return (2);
}

