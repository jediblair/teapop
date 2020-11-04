/* $Id: //depot/Teapop/0.3/teapop/pop_dele.c#7 $ */

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

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "teapop.h"
#ifdef NEED_FLOCK
#include "flock.h"
#endif
#include "pop_socket.h"
#include "pop_strings.h"

int
pop_update(pinfo)
	POP_INFO *pinfo;
{
	char buf[512];
	unsigned long i, retr=0, retrsize=0, dele=0, delesize=0;
	unsigned long left=0, leftsize=0;

	FILE *fd;
	POP_MSG *curmsg;
	sigset_t smask, osmask;
	time_t expiredate = 0;

	/*
	 * Ok, there's a goto here...So sue me =p
	 * It makes the code so much more readable and doesn't use
	 * extra CPUs as other solutions.
	 */
	if (pinfo->firstmsg == NULL)
		goto eofupdate;

	/*
	 * Block ALL signals while doing this, to avoid corrupt
	 * mailboxes.
	 */
	sigfillset((sigset_t *)&smask);
	sigemptyset((sigset_t *)&osmask);
	sigprocmask(SIG_BLOCK, (sigset_t *)&smask, (sigset_t *)&osmask);

	/* All messages created before this date should be deleted */
	if (pinfo->expire != 0)
		expiredate = (time_t)(time(NULL) -
		    (time_t)(pinfo->expire * 86400));

	if (pinfo->mboxtype == 0) {
#ifdef FLOCK
		flock(fileno(pinfo->mbox), LOCK_SH|LOCK_NB);
#endif
		fd = fopen(pinfo->maildrop, "r");
		rewind(fd); rewind(pinfo->mbox);
		fseek(fd, (long)pinfo->firstmsg->som, SEEK_SET);
		fseek(pinfo->mbox, (long)pinfo->firstmsg->som, SEEK_SET);
		curmsg = pinfo->firstmsg;
		while(curmsg != NULL) {
			if (curmsg->flags & MSG_READ) {
				retr++;
				retrsize += curmsg->realsize;
			}
			if (!(curmsg->flags & MSG_DELETED) &&
			    !((curmsg->flags & MSG_READ) &&
			    (pinfo->autodelete > 0)) &&
			    !(pinfo->expire != 0 &&
			    (curmsg->created < expiredate))) {
				for (i = 0UL; i < curmsg->realsize;
				    i += strlen(buf)) {
					fgets(buf, sizeof(buf), fd);
					if (!feof(fd))
						fputs(buf, pinfo->mbox);
				}
				left++;
				leftsize += curmsg->realsize;
			} else {
				fseek(fd, (long)curmsg->realsize, SEEK_CUR);
				dele++;
				delesize += curmsg->realsize;
			}
			curmsg = curmsg->nextmsg;
		}
		while (!feof(fd)) {
			fgets(buf, sizeof(buf), fd);
			if (!feof(fd))
				fputs(buf, pinfo->mbox);
		}
		fclose(fd);
		ftruncate(fileno(pinfo->mbox), (off_t)ftell(pinfo->mbox));
	} else {
		/* Maildir */
		curmsg = pinfo->firstmsg;
		while(curmsg != NULL) {
			if (curmsg->flags & MSG_READ) {
				retr++;
				retrsize += curmsg->size;
			}
			if (!(curmsg->flags & MSG_DELETED) &&
			    !((curmsg->flags & MSG_READ) &&
			    (pinfo->autodelete > 0)) && 
			    !(pinfo->expire != 0 &&
			    (curmsg->created < expiredate))) {
				left++;
				leftsize += curmsg->size;
			} else {
				unlink(curmsg->file);
				dele++;
				delesize += curmsg->size;
			}
			curmsg = curmsg->nextmsg;
		}
	}

	/*
	 * All critical updates done, we can now release signal
	 * handling again.
	 */
	sigprocmask(SIG_SETMASK, (sigset_t *)&osmask, NULL);

eofupdate:
#ifdef VPOP
	if (pinfo->domain[0] != '\0')
		syslog(LOG_INFO, "%s@%s [%s] R%lu(%lu) D%lu(%lu) L%lu(%lu)",
		    pinfo->userid, pinfo->domain, pinfo->remoteip, retr,
		    retrsize, dele, delesize, left, leftsize);
	else
#endif
		syslog(LOG_INFO, "%s [%s] R%lu(%lu) D%lu(%lu) L%lu(%lu)",
		    pinfo->userid, pinfo->remoteip, retr, retrsize, dele,
		    delesize, left, leftsize);

	return (0);
}
