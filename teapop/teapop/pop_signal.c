/* $Id: //depot/Teapop/0.3/teapop/pop_signal.c#5 $ */

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
#include <sys/wait.h>

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

#include "config.h"
#include "teapop.h"
#include "pop_lock.h"
#include "pop_signal.h"

jmp_buf env;

void
pop_signal_sigpipe(signo)
	int signo;
{

	pop_unlock_maildrop();
	syslog(LOG_ERR, "Caught SIGPIPE (signal = %d) - Lost connection",
	    signo);
	closelog();
	exit(1);
}

void
pop_signal_sigalrm(signo)
	int signo;
{

	longjmp(env,1);
}

void
pop_signal_sigchld(signo)
	int signo;
{
	int save_errno = errno;

	signal(SIGCHLD, pop_signal_sigchld);
	while (wait3(NULL, WNOHANG, NULL) > 0) ;
	errno = save_errno;
}

void
pop_signal_sigterm(signo)
	int signo;
{

	sigterm_seen = 1;
	syslog(LOG_ERR, "Caught SIGTERM (signal = %d) - Cleaning up and "
	    "exiting.", signo);
}
