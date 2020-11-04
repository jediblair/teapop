/* $Id$ */

/*
 * Copyright (c) 2001 ToonTown Consulting
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
#include <fcntl.h>
#include <unistd.h>

#include "lockf.h"

/*
 * The third argument should be named size, but it's way to close to
 * reserved keywords, so it get to be length.
 */

int
lockf(filedes, function, length)
	int filedes, function;
	off_t length;
{
	int retval, cmd;
	struct flock lock_data;

	lock_data.l_whence = SEEK_CUR;
	lock_data.l_start = 0;
	lock_data.l_len = length;

	switch (function) {
	case F_ULOCK:
		cmd = F_SETLK;
		lock_data.l_type = F_UNLCK;
		break;

	case F_LOCK:
		cmd = F_SETLKW;
		lock_data.l_type = F_WRLCK;
		break;

	case F_TLOCK:
		cmd = F_SETLK;
		lock_data.l_type = F_WRLCK;
		break;

	case F_TEST:
		/* We don't have a test yet */
		/* FALLTHROUGH */
	default:
		errno = EINVAL;
		return (-1);
		/* NOT REACHED */
	}

	retval = fcntl(filedes, cmd, &lock_data);

	if (retval == -1)
		switch (errno) {
		case EAGAIN:
		case EDEADLK:
			errno = EACCES;
			break;
	}

	return (retval);
}
