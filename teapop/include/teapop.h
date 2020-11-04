/* $Id: //depot/Teapop/0.3/include/teapop.h#7 $ */

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

#ifndef __TEAPOP_H__
#define __TEAPOP_H__

/*
 * The values in the section below may be of interest when tweaking Teapop.
 * It is ok to change them and still have something close to supportable.
 * But be warned; Bad values may cause funny behaviour or even crash your
 * system.
 */
#define MAXTRIES	3	/* Failed login attempts allowed per sess */
#define POP3PORT	110	/* Port to listen to in standalone mode */

/*
 * Anything below shouldn't be changed, unless you are doing hefty
 * modifications to Teapop.
 */

#define	BIGSTRING	255
#define SMALLSTRING	50

/* Locktypes */
#define	LOCK_DOTLOCK	0x01
#define	LOCK_FCNTL	0x02
#define	LOCK_FLOCK	0x04
#define	LOCK_LOCKF	0x08

/*
typedef struct	_pop_auth_radius {
} POP_AUTH_RADIUS;
*/

typedef struct _pop_auth_sql {
	char host[BIGSTRING];
	char *port;
	char db[SMALLSTRING];
	char username[SMALLSTRING];
	char password[SMALLSTRING];
	char table[SMALLSTRING];
	char userrow[SMALLSTRING];
	char passrow[SMALLSTRING];
	char mailrow[SMALLSTRING];
}             POP_AUTH_SQL;

typedef struct _pop_auth_ldap {
	char host[BIGSTRING];
	int port;
	char rootdn[BIGSTRING];		/* Root DN of tree */
	char attributes[BIGSTRING];	/* Not used */
	int authmethod;			/* Type of authentication used */
	int useTLS;
} POP_AUTH_LDAP;

typedef struct _pop_auth_text {
	char file[BIGSTRING];
	int max;
}              POP_AUTH_TEXT;

typedef struct _pop_auth {
	char domain[BIGSTRING];
	char localip[16];
	char maildrop[BIGSTRING];
	char authmethod;
	int hash;
	char uid[SMALLSTRING];
	char gid[SMALLSTRING];

	void *extra;

	struct _pop_auth *nextauth;
}         POP_AUTH;
#define MSG_READ	0x01
#define MSG_DELETED	0x02

typedef struct _pop_msg {
	unsigned long som;	/* mbox only */
	unsigned long offset;	/* mbox only */
	unsigned long realsize;	/* mbox only */
	unsigned long size;
	unsigned long lines;
	time_t created;		/* Timestamp of creation date of mail */
	int flags;
	char uidl[SMALLSTRING];
	char *file;		/* Maildir only */
	struct _pop_msg *nextmsg;
}        POP_MSG;

typedef struct _pop_info {
	int insck;
	int outsck;
	int autodelete;
	int ignoreimap;
	int timeout;
	int locktimeout;
	int nodns;
	int useuidl;
	int locktrack;
	int mboxperm;
	int expire;
	int ssl;
	int softlock;
	unsigned short localport;
	char drachost[BIGSTRING];
	char apopstr[BIGSTRING];
	char userid[BIGSTRING];
	char domain[BIGSTRING];
	char maildrop[BIGSTRING];
	char dotlock[BIGSTRING+20];
	char mboxtype;		/* 0 = mbox, 1 = Maildir */
	char chroot[BIGSTRING];
	char localip[40];
	char remoteip[40];
	char remotehost[BIGSTRING];
	FILE *lock;
	FILE *mbox;
	FILE *out;
	POP_MSG *firstmsg;
	POP_AUTH *firstauth;
	void *smask;
}         POP_INFO;

extern volatile int sigterm_seen;

#endif				/* __TEAPOP_H__ */
