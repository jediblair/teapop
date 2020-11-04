/* $Id: //depot/Teapop/0.3/teapop/pop_lock.c#11 $ */

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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "teapop.h"
#ifdef NEED_FLOCK
#include "flock.h"
#endif
#include "pop_lock.h"

POP_INFO *xpinfo;

int
pop_lock_maildrop(pinfo, loop)
	POP_INFO *pinfo;
	int loop;
{
	int retval = 0;
	int locks = 0;

	struct stat sb;

	/* This is for reference in pop_unlock_maildrop() */
	xpinfo = pinfo;

	/* Used for blocking signals in pop_unlock_maildrop() */
	/* Only block SIGPIPE for now */
	pinfo->smask = (void *)malloc(sizeof(sigset_t));
	sigemptyset((sigset_t *)pinfo->smask);
	sigaddset((sigset_t *)pinfo->smask, SIGPIPE);

	/*
	 * An ugly hack to set locks according to configure
	 * flags.
	 */

	/*
	 * this was setting pinfo->locktrack - we only *need*
	 * pinfo->locktrack to track which locks we have. If
	 * we're not going to bother doing that, dump it
	 * altogether...
	 */
#ifdef DOTLOCK
	locks |= LOCK_DOTLOCK;
#endif
#ifdef FCNTL
	locks |= LOCK_FCNTL;
#endif
#ifdef FLOCK
	locks |= LOCK_FLOCK;
#endif
#ifdef LOCKF
	locks |= LOCK_LOCKF;
#endif

	/*
	 * loop is set to 0 if we are still root. If that's the case,
	 * we will only do dotlock for mbox.
	 */
	/* XXX - NOT YET! */
/*	if (loop == 0) { */
	if (loop == 1 && (locks & LOCK_DOTLOCK)) {
		if (pinfo->mboxtype == 0 && locks & LOCK_DOTLOCK) {
			(void)snprintf(pinfo->dotlock,
			    sizeof(pinfo->dotlock) - 1, "%s.lock",
			    pinfo->maildrop);
			retval = pop_lock_dotlock(pinfo);
		}
		/* XXX - DO NOT RETURN BEFORE WE RUN THIS TWICE! */
/*		return (retval); */
	}

	/*
	 * loop is not set to 0, so we have dropped privs and can do
	 * normal locking now.
	 */

	/* If it's Maildir/ we do dotlock and return */
	if (pinfo->mboxtype == 1) {
		(void)snprintf(pinfo->dotlock, sizeof(pinfo->dotlock) - 1,
		    ".lock");

		/*
		 * With Maildir we do a chdir(2) to the directory,
		 * because it makes it easier in the rest of the
		 * code.
		 */
		if (chdir(pinfo->maildrop) != 0) {
			syslog(LOG_ERR, "Can't chdir to %s at chroot %s "
			    "(errno = %d)", pinfo->maildrop,
			    (pinfo->chroot[0] == '\0' ? "NO_CHROOT" :
			    pinfo->chroot), errno);
			return (2);
		}

		retval = pop_lock_dotlock(pinfo);
		return (retval);
	}

	/* If it's mbox, do fcntl/flock if wanted. */
	if (pinfo->mboxtype == 0) {
		if ((pinfo->mbox = fopen(pinfo->maildrop, "r+")) == NULL) {
			if (errno != ENOENT) {
				syslog(LOG_NOTICE, "Problems opening mailbox "
				    "%s in %s (errno = %d)", pinfo->maildrop,
				    pinfo->chroot, errno);
				return (2);
			}
			return (0);
		}
		
		if ((locks & LOCK_FLOCK) && (retval == 0))
			retval = pop_lock_flock(pinfo);
		if ((locks & LOCK_FCNTL) && (retval == 0))
			retval = pop_lock_fcntl(pinfo);
		if ((locks & LOCK_LOCKF) && (retval == 0))
			retval = pop_lock_lockf(pinfo);

		/*
		 * Save permissions for the mbox, so we can reset it, if
		 * needed, later. If fstat fails, mboxperm is set to 0
		 * and no attempt to reset permissions will be done later.
		 */
		pinfo->mboxperm = fstat(fileno(pinfo->mbox), &sb) < 0 ?
		    0 : sb.st_mode;
		pinfo->mboxperm &= (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU |
		    S_IRWXG | S_IRWXO);

	}

	/*
	 * Make sure we don't leave any locks (especially dotlocks)
	 * lying around if any necessary lock failed...
	 */
	if (retval != 0)
		pop_unlock_maildrop();

	return (retval);
}

int
pop_lock_dotlock(pinfo)
	POP_INFO *pinfo;
{
	int fd;
	char hostname[128];

	struct stat sb;
	time_t current;

	/*
	 * Try creating a lockfile with O_EXCL | O_CREAT and catch
	 * EEXIST to see if it exists.
	 */
	if ((fd = open(pinfo->dotlock, O_CREAT | O_EXCL | O_RDWR,
	     S_IRUSR | S_IWUSR)) == -1) {
		/* Creation failed. If it's not EEXIST, report error. */
		if (errno != EEXIST) {
			/*
			 * If chroot already has the ending slash this can
			 * show 2 slashes in syslog but its only visual problem
			 */
			syslog(LOG_ERR, "Can't create lockfile %s in %s/%s "
			    "(errno = %d)", pinfo->dotlock, 
			    pinfo->chroot, pinfo->maildrop,
			    errno);
			return (2);
		}

		/*
		 * XXX - A sanity check should go here that makes sure
		 *       the right owner created the lockfile.
 		 */
		if (pinfo->locktimeout > 0) {
			/* Checking for, and remove, stale lock */
			if ((stat(pinfo->dotlock, &sb)) != -1) {
				current = time(NULL);
				if ((sb.st_mtime + pinfo->locktimeout) 
				    < current) {
					if (unlink(pinfo->dotlock) == -1) {
						syslog(LOG_ERR, 
						   "Error removing "
						   "lockfile %s in %s/%s "
						   " (errno = %d)",
						   pinfo->dotlock,
						   pinfo->chroot,
						   pinfo->maildrop,
						   errno);
						return (1);
					} else {
						if (pinfo->domain[0] != '\0')
							syslog(LOG_ERR,
							   "Lockfile removed for %s@%s",
							   pinfo->userid,
							   pinfo->domain);
						else
							syslog(LOG_ERR,
							   "Lockfile removed for %s",
							   pinfo->userid);
						/*
						 * lockfile can be created now
						 */ 
						return pop_lock_dotlock(pinfo);
					}
				}
			} else {
				/* We can't check the lock file!? */
				syslog(LOG_ERR, "Failed doing stat() on "
				    "lockfile %s in %s/%s (errno = %d)",
				    pinfo->dotlock, 
				    pinfo->chroot, pinfo->maildrop, 
				    errno);
				return (2);
			}
		}
		syslog(LOG_ERR, "Lockfile %s already exists in %s/%s",
		    pinfo->dotlock, pinfo->chroot, pinfo->maildrop);
		return (1);
	}

	/* Associate the stream pinfo->lock with the file descriptor. */
	if ((pinfo->lock = fdopen(fd, "w")) == NULL) {
		syslog(LOG_NOTICE, "Problems with lockfile %s/%s in %s "
		    "(errno = %d)", pinfo->maildrop, pinfo->dotlock,
		    pinfo->chroot, errno);
		return (2);
	}

	(void)gethostname((char *)&hostname, sizeof(hostname));
	(void)fprintf(pinfo->lock, "%d %s %s %s\n", (int)getpid(), hostname, 
	     pinfo->remotehost, pinfo->remoteip);
	(void)fflush(pinfo->lock);
	
	xpinfo->locktrack |= LOCK_DOTLOCK;

	return (0);
}

int
pop_lock_fcntl(pinfo)
	POP_INFO *pinfo;
{
	struct flock lock_data;

	lock_data.l_type = F_WRLCK;
	lock_data.l_whence = lock_data.l_start = lock_data.l_len = 0;

	if ((fcntl(fileno(pinfo->mbox), F_SETLK, &lock_data)) == -1) {
		(void)fclose(pinfo->mbox);
		return (1);
	}

	xpinfo->locktrack |= LOCK_FCNTL;
	return (0);
}

int
pop_lock_flock(pinfo)
	POP_INFO *pinfo;
{
	if ((flock(fileno(pinfo->mbox), LOCK_EX | LOCK_NB)) == -1) {
		(void)fclose(pinfo->mbox);
		return (1);
	}
	xpinfo->locktrack |= LOCK_FLOCK;
	return (0);
}

int
pop_lock_lockf(pinfo)
	POP_INFO *pinfo;
{

	if ((lockf(fileno(pinfo->mbox), F_TLOCK, 0)) == -1) {
		(void)fclose(pinfo->mbox);
		return (1);
	}

	xpinfo->locktrack |= LOCK_LOCKF;
	return (0);
}

void
pop_unlock_maildrop(void)
{
	struct flock lock_data;

	/* xpinfo is unset if no mailbox is opened */
	if (xpinfo == NULL)
		return;

	/* Block all signals to avoid this function being run twice */
	sigprocmask(SIG_BLOCK, (sigset_t *)xpinfo->smask, NULL);

	/*
	 * Reset permissions on mbox, which might get lost when writing,
	 * but only if we have valid info to set (fstat earlier worked).
	 */
	if (xpinfo->mboxtype == 0) {
		fflush(xpinfo->mbox);
		if (xpinfo->mboxperm != 0)
			fchmod(fileno(xpinfo->mbox), xpinfo->mboxperm);
	}
 
	/* Only unlock each lock if it's locked */

	if (xpinfo->locktrack & LOCK_DOTLOCK) {
		(void)unlink(xpinfo->dotlock);
		(void)fclose(xpinfo->lock);
		xpinfo->locktrack &= ~LOCK_DOTLOCK;
	}

	if (xpinfo->locktrack & LOCK_FLOCK) {
		(void)flock(fileno(xpinfo->mbox), LOCK_UN);
		xpinfo->locktrack &= ~LOCK_FLOCK;
	}		
	if (xpinfo->locktrack & LOCK_FCNTL) {
		lock_data.l_type = F_UNLCK;
		lock_data.l_whence = lock_data.l_start = lock_data.l_len = 0;
		(void)fcntl(fileno(xpinfo->mbox), F_SETLK, &lock_data);
		xpinfo->locktrack &= ~LOCK_FCNTL;
	}
	if (xpinfo->locktrack & LOCK_LOCKF) {
		(void)lockf(fileno(xpinfo->mbox), F_ULOCK, 0);
		xpinfo->locktrack &= ~LOCK_LOCKF;
	}		

	/* We've forgotten to implement an unlock type if this triggers */
	/* assert(xpinfo->locktrack == 0); */
}
