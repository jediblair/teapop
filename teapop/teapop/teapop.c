/* $Id: //depot/Teapop/0.3/teapop/teapop.c#8 $ */

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
#include <sys/socket.h>
#include <sys/stat.h>

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "teapop.h"
#include "version.h"
#include "pop_auth.h"
#include "pop_dele.h"
#include "pop_file.h"
#include "pop_lock.h"
#include "pop_parse.h"
#include "pop_passwd.h"
#include "pop_signal.h"
#include "pop_socket.h"
#include "pop_stat.h"
#include "pop_strings.h"

#ifdef PROFIL
void etext __P((int));
void s_monitor __P((int));
#endif

int teapop __P((POP_INFO *));
int teapop_s __P((POP_INFO *));
void usage __P((void));

volatile int sigterm_seen = 0;

int
main(argc, argv)
int argc;
char *argv[];
{
	extern char *optarg;
	int ch, standalone, retval;
	char syslogname[20];

	POP_INFO pinfo;

	argv[0] = "teapop";
	memset(&pinfo, 0, sizeof(pinfo));
	standalone = 0;		/* runs from inetd if there is no -s */
	pinfo.ignoreimap = 0;	/* Don't ignore UW IMAP's message by default */
	pinfo.timeout = 900;	/* Disconnect an inactive user after 15 mins */
	pinfo.locktimeout = 0;	/* Don't remove dotlock files by default */

#ifdef PROFIL
	monstartup(0, etext);
	moncontrol(1);
	signal(SIGUSR1, s_monitor);
#endif

	strcpy(syslogname, "teapop");
	tzset();

	while ((ch = getopt(argc, argv, "dDe:hil:LnNp:P:sSt:vu")) != -1)
		switch (ch) {
		case 'd':
			pinfo.autodelete = 1;
			break;
		case 'D':
			pinfo.autodelete = 2;
			break;
		case 'e':
			pinfo.expire = (int)pop_string_dtot(optarg);
			break;
		case 'i':
			pinfo.ignoreimap = 1;
			break;
		case 'l':
			pinfo.locktimeout = (int) (atoi(optarg)*60);
			break;
		case 'L':
			pinfo.softlock = 1;
			break;
		case 'n':
			pinfo.nodns = 2;
			break;
		case 'N':
			if (pinfo.nodns != 2)
				pinfo.nodns = 1;
			break;
		case 'p':
			strncpy(pinfo.drachost, optarg,
			    sizeof(pinfo.drachost));
			pinfo.drachost[sizeof(pinfo.drachost) - 1] = '\0';
			break;
		case 'P':
			pinfo.localport = (unsigned short) atoi(optarg);
			break;
		case 's':
			standalone = 1;
			break;
		case 'S':
			pinfo.ssl = 1;
			strcpy(syslogname, "teapop_ssl");
			break;
		case 't':
			pinfo.timeout = (int) atoi(optarg);
			break;
		case 'v':
			version();
			/* NOT REACHED */
		case 'u':
			pinfo.useuidl = 1;
			break;
		case 'h':
		default:
			usage();
			/* NOT REACHED */
		}

	openlog(syslogname, LOG_PID | LOG_NDELAY, LOG_LOCAL0);
	pop_read_pwdinfo(&pinfo);

	pinfo.insck = fileno(stdin);
	pinfo.outsck = fileno(stderr);

	if (standalone == 1)
		retval = teapop_s(&pinfo);
	else
		retval = teapop(&pinfo);

	closelog();

	return (retval);
}

int
teapop(pinfo)
POP_INFO *pinfo;
{
	int retval;

	signal(SIGPIPE, pop_signal_sigpipe);
	signal(SIGALRM, pop_signal_sigalrm);

	/* Make sure stdin is a socket */
	if ((retval = pop_socket_init(pinfo)) != 0) {
		if (retval == 2) {
			fprintf(stderr,"connection refused by tcp_wrappers\r\n");
		} else {
			fprintf(stderr, "stdin is not a socket\r\n");
			fprintf(stderr, "If you want to run teapop in standalone " \
			    "mode you have to use the -s argument.\r\n");
		}
		return (retval);
	}

	switch (pop_auth(pinfo)) {
	case 0:		/* USER IS AUTH'ED */
		if ((retval = pop_lock_maildrop(pinfo, 1)) != 0) {
			if (retval == 3) {
				pop_socket_send(pinfo->out, "%s ehum?",
				    POP_ERR);
				pop_unlock_maildrop();
				break;
			} else if (pinfo->softlock != 1) {
				pop_socket_send(pinfo->out, "%s %s", POP_ERR,
				    POP_LOCK_ERR);
				break;
			}
			/*
			 * If we are using softlock, make sure mbox is
			 * NULL, to trick Teapop to believe the mailbox
			 * is empty.
			 */
			pinfo->mbox = NULL;
		}
		pop_get_status(pinfo);
		pop_socket_send(pinfo->out, "%s %s", POP_OK, POP_READY);
		pop_parse_cmds(pinfo);
		if ((pop_update(pinfo)) == 0)
			pop_socket_send(pinfo->out, "%s %s", POP_OK,
			    POP_QUIT_OK);
		else
			pop_socket_send(pinfo->out, "%s %s", POP_ERR,
			    POP_QUIT_ERR);
		pop_unlock_maildrop();
		break;
	case 1:		/* USER SENT QUIT IN AUTH-MODE */
		pop_socket_send(pinfo->out, "%s %s", POP_OK, POP_QUIT_AUTH);
		break;
	case 2:		/* MAX TRIES REACHED */
		pop_socket_send(pinfo->out, "%s %s", POP_OK, POP_QUIT_MAXTRY);
		break;
	default:		/* NEVER REACHED */
		break;
	}

	return (0);
}

int
teapop_s(pinfo)
POP_INFO *pinfo;
{
	int pid, sckfd, acptfd;

	if ((pid = fork()) < 0) {
		syslog(LOG_ERR, "Problem when doing fork() : %s",
		    strerror(errno));
		perror("fork");
		return (3);
	}
	setsid();

	if (pid > 0)
		exit(0);	/* exit main process */

	logpid();

	if ((sckfd = pop_socket_bind(pinfo->localport)) == 0)
		return (1);

	signal(SIGCHLD, pop_signal_sigchld);
	signal(SIGTERM, pop_signal_sigterm);

	for (;(sigterm_seen == 0);) {
		if (pop_socket_wait(sckfd) < 1)
			continue;
		if ((acptfd = accept(sckfd, NULL, NULL)) < 0)
			continue;

		switch (pid = fork()) {
		case -1:	/* fork error */
			syslog(LOG_ERR, "Problem when doing fork() : %s",
			    strerror(errno));
			return (3);
			/* NOT REACHED */
		case 0:	/* child */
			setsid();
			(void) dup2(acptfd, 0);
			(void) dup2(acptfd, 1);
			(void) dup2(acptfd, 2);
			close(sckfd);
			teapop(pinfo);
			close(acptfd);
			close(0);
			close(1);
			close(2);
			return (0);
			/* NOT REACHED */
		default:	/* parent */
			close(acptfd);
		}
	}

	/* clean up - closelog done in main() after return */
	close(sckfd);
	return (0);
}

void
usage(void)
{

	printf("Syntax: teapop [-dDhinNsSuv] [-e age] [ -l minutes ] "
	    "[-p hostname] [-P port] [-t seconds]\n");

	exit(0);
}
