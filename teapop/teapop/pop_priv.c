/* $Id: //depot/Teapop/0.3/teapop/pop_priv.c#5 $ */

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

#include "config.h"

#include <sys/types.h>

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "teapop.h"

char *pop_hashed __P((char *, int));

int
pop_droppriv(pinfo, curauth)
	POP_INFO *pinfo;
	POP_AUTH *curauth;
{
	int tmpid;

	struct passwd *userinfo;
	struct group *groupinfo;

	/* Add hash string to the mailspool directory. */
	(void)strncat(pinfo->chroot, pop_hashed(pinfo->maildrop,
	    curauth->hash), sizeof(pinfo->chroot) - 1);
	pinfo->chroot[sizeof(pinfo->chroot) - 1] = '\0';

	/*
	 * If we don't have superuser priv, chdir() instead of chroot()
	 * and skip the rest here, since it will fail anyway.
	 */
	if (getuid() != 0) {
		if (pinfo->chroot[0] != '\0')
			if ((chdir(pinfo->chroot)) == -1) {
				syslog(LOG_ERR, "Can't chdir to directory "
				    "[%s] for domain %s", pinfo->chroot,
				    pinfo->domain);
				return (1);
			}
		return (0);
	}

	/* Try to figure out what uid to drop privs to. */
	if (curauth->uid[0] == '+')
		userinfo = getpwnam(pinfo->userid);
	else
		userinfo = getpwnam(curauth->uid);

	if (userinfo == NULL) {
		if (curauth->uid[0] != '+')
			/*
			 * Try getpwuid() too, incase the userid field is
			 * the uid and not username. We do not allow uid 0,
			 * since that's what we get from a failed atoi().
			 * We do not try getpwuid() with the username the
			 * client sent, since the client might be evil.
			 */
			if ((tmpid = (int)atoi(curauth->uid)) > 0)
				userinfo = getpwuid(tmpid);

		if (userinfo == NULL) {
			if (curauth->uid[0] == '+')
				syslog(LOG_ERR, "Can't drop privs to "
				    "nonexisting user, %s", pinfo->userid);
			else
				syslog(LOG_ERR, "Can't drop privs to "
				    "nonexisting user, %s", curauth->uid);
			return (1);
		}
	}

	/* Try to figure out which gid to drop privs to. */
	if (curauth->gid[0] == '+')
		groupinfo = getgrgid((gid_t)userinfo->pw_gid);
	else
		groupinfo = getgrnam(curauth->gid);

	if (groupinfo == NULL) {
		if (curauth->gid[0] != '+')
			/* Try getgrgid() too - See getpwuid note above. */
			if ((tmpid = (int)atoi(curauth->gid)) > 0)
				groupinfo = getgrgid(tmpid);

		if (groupinfo == NULL) {
			if (curauth->gid[0] == '+')
				syslog(LOG_ERR, "Can't drop privs to "
				    "nonexisting group, %d",
				    (int)userinfo->pw_gid);
			else
				syslog(LOG_ERR, "Can't drop privs to "
				    "nonexisting group, %s",
				    curauth->gid);
			return (1);
		}
	}

	/* Enter chroot()-jail */
	if (pinfo->chroot[0] != '\0') {
		if ((chroot(pinfo->chroot)) == -1) {
			syslog(LOG_ERR, "Can't chroot to directory [%s] for "
			    "domain %s", pinfo->chroot, pinfo->domain);
			return (1);
		}
		chdir("/");	/* Needed since we can be outside chroot-tree */
	}

	/* Drop group privs */
	setgid((gid_t)groupinfo->gr_gid);
	/* Initialize supplementary groups for user (by sg@ur.ru) */
	initgroups(userinfo->pw_name, (gid_t)groupinfo->gr_gid);

	/* Drop user privs; finally */
	setuid((uid_t)userinfo->pw_uid);

	return (0);
}

char *
pop_hashed(string, hashlevel)
	char *string;
	int hashlevel;
{
	int i;
	static char hash[1024] = "";

	if (hashlevel == 0)
		return ("");

	/* Avoid overflow of string */
	if (hashlevel >= ((sizeof(hash) / 2) - 1)) {
		syslog(LOG_ERR, "Ignoring hashlevel %d (>= %d)", hashlevel,
		    (int)((sizeof(hash) / 2) - 1));
		return ("");
	}

	for (i = 0; i < hashlevel && string[i] != '\0'; i++) {
		(void)strcat(hash, "/");
		(void)strncat(hash, &string[i], 1);
	}
	return (hash);
}
