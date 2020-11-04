/* $Id: //depot/Teapop/0.3/teapop/pop_maildir.c#9 $ */

/*
 * Copyright (c) 2000-2003 ToonTown Consulting
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
 
/*
 * Information about Maildir can be found at :
 *     http://cr.yp.to/proto/maildir.html
 *     http://mirrors.dataloss.nl/www.qmail.org/man/man5/maildir.html
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_MD5_H
#include <md5.h>
#else
#include "md5.h"
#endif /* HAVE_MD5 */

#include "teapop.h"
#include "pop_maildir.h"

/*
 * TODO: support for info flags in message filename
 * Check with people if old UIDL calculation can be removed to leave
 * only FASTMAILDIR implementation which is working very fine here
 */

/*
 * Temporary storage for work in add_maildir_message
 * This structure has the big fields and the necessary
 * fields to be used between calls to add_maildir_message
 */
typedef struct _maildir_info
{
	POP_MSG *lastmsg;
	char buf[1024];
	char realname[256];
/*
 * Used to name the message files with unique name when necessary
 * not used directly here, but in patches for this code
 */
	char hostname[128];
	int pid;
	int msgcount;
}
MAILDIR_INFO;

/*
 * This routine checks a message and adds it to user messages
 * It received the directory and filename as separate to make it
 * easy to make checks directly in filename
 */
int
add_maildir_message(minfo, pinfo, dir, fname)
MAILDIR_INFO *minfo;
POP_INFO *pinfo;
char *dir;
char *fname;
{
	int counter, finduidl, retval, i;
	char *ptr;
	unsigned char digest[16];
	FILE *fd;
	MD5_CTX ctx;
	POP_MSG *curmsg;
	struct stat sb;

	/*
	 * Define the full name of message file
	 */
	strncpy(minfo->realname, dir, sizeof(minfo->realname));
	strncat(minfo->realname, fname,
	    sizeof(minfo->realname) - strlen(minfo->realname));
	minfo->realname[sizeof(minfo->realname)] = '\0';

	retval = lstat(minfo->realname, &sb);
	if (retval != 0 || !S_ISREG(sb.st_mode))
		return (0);

	/*
	 * XXX - Add a sanity check so the filename
	 * is in the correct format, or ignore the
	 * file.
	 */
	/*
	 * Maildir specification say to ignore files beginning with '.'
	 */
	if (fname[0] == '.')
		return (0);

	if ((curmsg = malloc(sizeof(POP_MSG))) == NULL) {
		syslog(LOG_ERR,
		    "Error allocating memory in add_maildir_message : %d",
		    errno);
		return (1);
	}
	memset(curmsg, 0, sizeof(POP_MSG));
	if ((ptr = malloc((strlen(minfo->realname) + 1) *
		    sizeof(char))) == NULL) {
		free(curmsg);
		syslog(LOG_ERR,
		    "Error allocating memory in add_maildir_message : %d",
		    errno);
		return (1);
	}
	(void)strcpy(ptr, minfo->realname);
	curmsg->file = ptr;

	/* 
	 * Get the timestamp from the filename 
	 * standard defines filename as time.pid.host
	 * if it does not work use value from lstat
	 */
	curmsg->created = (time_t)strtol(fname, (char **)NULL, 10);
	if (curmsg->created == 0)
		curmsg->created = sb.st_mtime;

	finduidl = pinfo->useuidl;
	MD5Init(&ctx);
/*
 * When FAST_MAILDIR is defined it will calculate the UIDL from filename
 * instead of using the data on file
 * Using a number of lines greater than real message size will work
 */	
#ifdef FAST_MAILDIR
        ptr = strchr(fname, ':');
        if (ptr == NULL)
		MD5Update(&ctx, fname, strlen(fname));
	else
		MD5Update(&ctx, fname, (ptr - fname) );
	curmsg->size = sb.st_size;
	curmsg->lines = 999999999;
#else
	if ((fd = fopen(curmsg->file, "r")) == NULL) {
		syslog(LOG_ERR, "Error opening message file : %s %d",
		    curmsg->file, errno);
		free(ptr);
		free(curmsg);
		return (0);
	}
	while (fgets(minfo->buf, sizeof(minfo->buf), fd)) {
		/* Find all NULL characters and replace with questionmarks */
		i = strlen(minfo->buf);
		while ((i == 0 || minfo->buf[i-1] != '\n') && i < (sizeof(minfo->buf) - 1)) {
			minfo->buf[i] = '?';
			i = strlen(minfo->buf);
		}
 
		/*
		 * If line only ends with LF, we need to add one to the size
		 * since we will be sending CRLF to the client.
		 * Note we need to check that the number of characters in
		 * buf is high enough (found in i) to not cause buffer
		 * underflow.
		 */
		if (i > 0 && minfo->buf[i-1] == '\n') {
			curmsg->lines++;
			if (i < 1 || minfo->buf[i-2] != '\r')
				curmsg->size++;
		}

		if (finduidl == 1 && !strncasecmp(minfo->buf, "X-UIDL: ", 8)) {
			minfo->buf[strlen(minfo->buf) - 1] = '\0';
			strncpy(curmsg->uidl, &minfo->buf[8],
			    sizeof(curmsg->uidl));
			curmsg->uidl[sizeof(curmsg->uidl) - 1] = '\0';
			finduidl = 0;
		}
		curmsg->size += strlen(minfo->buf);
		MD5Update(&ctx, (unsigned char *) minfo->buf,
		    strlen(minfo->buf));
	}
	(void) fclose(fd);
#endif

	MD5Final(digest, &ctx);
	if (!(finduidl == 0 && pinfo->useuidl == 1)) {
		ptr = curmsg->uidl;
		for (counter = 0; counter < (int) sizeof(digest);
		    counter++, ptr += 2)
			sprintf(ptr, "%02x", digest[counter] & 0xff);
		*ptr = '\0';
	}

	if (pinfo->firstmsg == NULL)
		pinfo->firstmsg = minfo->lastmsg = curmsg;
	else
		minfo->lastmsg = minfo->lastmsg->nextmsg = curmsg;
	minfo->msgcount += 1;

	return (0);
}

int 
pop_maildir_load_dir(minfo, pinfo, dir)
MAILDIR_INFO *minfo;
POP_INFO *pinfo;
char *dir;
{
	char procdir[BIGSTRING];
	struct dirent *dp;
	DIR *dirp;

	dirp = opendir(dir);
	(void) strncpy(procdir, dir, sizeof(procdir));
	(void) strncat(procdir, "/", sizeof(procdir)-strlen(procdir));

	if (dirp == NULL) {
		syslog(LOG_ERR, "can't open maildir %s (%d)", 
		    procdir, errno);
		return (1);
	}

	while ((dp = readdir(dirp)) != NULL) {

		if (add_maildir_message(minfo, pinfo, procdir,
			dp->d_name) == 1) {
			break;
		}
	}
	(void) closedir(dirp);
	
	return (0);
}

#ifdef SORT_MAILDIR
typedef struct _sort_data
{
	POP_MSG *msg;
}
SORT_DATA;


/*
 * Make comparision for qsort
 */
int 
compare_msg(sd1, sd2)
SORT_DATA *sd1;
SORT_DATA *sd2;
{
    if (sd1->msg->created < sd2->msg->created) 
    	return (-1);
    else if (sd1->msg->created == sd2->msg->created)
    	return (0);
    else
    	return (1);
    	
}

/*
 * Sort message list by creating date
 * create array with the pointers and use qsort to sort messages
 */
void 
pop_maildir_sort(pinfo, cnt)
POP_INFO *pinfo;
int cnt;
{
	POP_MSG *msg;
	
	if (cnt > 1) {
		int x = 0;
		SORT_DATA *data;

		data = malloc(sizeof(SORT_DATA)*cnt);
		msg = pinfo->firstmsg;
		while (msg) {
			data[x].msg = msg;
			x += 1;
			msg = msg->nextmsg;
		}

		qsort(data, cnt, sizeof(SORT_DATA), compare_msg);
		
		x = 0;
		pinfo->firstmsg = data[x].msg;
		while (x < (cnt-1)) {
			data[x].msg->nextmsg = data[x+1].msg;
			data[x+1].msg->nextmsg = NULL;
			x += 1;
		}

		free(data);
	}
}

#endif /* SORT_MAILDIR */

int
pop_maildir_get_status(pinfo)
POP_INFO *pinfo;
{

	MAILDIR_INFO minfo;

	/*
	 * Initialize the MAILDIR_INFO
	 */
	minfo.pid = getpid();
	gethostname((char *) &minfo.hostname, sizeof(minfo.hostname));
	minfo.msgcount = 0;
	/*
	 * This is not really needed, but it makes -Wall happy, AND
	 * it's there if Teapop ever in the future would support a
	 * user with several mailboxes.
	 */
	if (pinfo->firstmsg == NULL)
		minfo.lastmsg = NULL;
	else {
		minfo.lastmsg = pinfo->firstmsg;
		while (minfo.lastmsg->nextmsg != NULL) {
			minfo.lastmsg = minfo.lastmsg->nextmsg;
			minfo.msgcount += 1;
		}
	}

	(void) pop_maildir_load_dir(&minfo, pinfo, "cur");
	(void) pop_maildir_load_dir(&minfo, pinfo, "new");

#ifdef SORT_MAILDIR	
	pop_maildir_sort(pinfo, minfo.msgcount);
#endif	

	return (0);
}
