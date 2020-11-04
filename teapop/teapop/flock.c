/* $Id$ */

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

#include "config.h"

#include <errno.h>
#include <unistd.h>

#include "flock.h"

/* There is a 0.000001% chance this errno-code might not exist. */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

int
flock(fd, operation)
	int fd, operation;
{
	int retval;

	/*
	 * Since lockf does not have shared lock, we will in the
	 * switch() fallthrough to exclusive instead.
	 */
	switch (operation) {
	/* Blocking locking */
	case LOCK_SH:
		/* FALLTHROUGH */
	case LOCK_EX:
		retval = lockf(fd, F_LOCK, 0);
		break;

	/* Non-blocking locking */
	case LOCK_SH | LOCK_NB:
		/* FALLTHROUGH */
	case LOCK_EX | LOCK_NB:
		retval = lockf(fd, F_TLOCK, 0);
		break;

	/* Unlock */
	case LOCK_UN:
		retval = lockf(fd, F_ULOCK, 0);
		break;

	/* Bad operation */
	default:
		errno = EINVAL;
		retval = -1;
		break;
	}

	/* Fix errno so it's flock-compatible */
	if (retval == -1)
		switch (errno) {
		case EDEADLK:
		case ENOLCK:
			errno = EOPNOTSUPP;
			break;
		case EAGAIN:
			errno = EWOULDBLOCK;
			break;
		}

	return (retval);
}
