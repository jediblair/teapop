/* $Id: //depot/Teapop/0.3/teapop/pop_passwd.c#6 $ */

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
#include <sys/socket.h>
#include <sys/stat.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif

#ifndef HAVE_MD5_H
#include "md5.h"
#else
#include <md5.h>
#endif

#ifdef HAVE_LDAP
#include <ldap.h>
#endif

#include "teapop.h"
#include "pop_popsmtp.h"

#define PWDINFO ETC_DIR"/teapop.passwd"

enum authtypes {
	NOTSET,
	SYSPASSWD,
	TEXTFILE,
	HTPASSWD,
	MYSQL,
	PGSQL,
	ORACLE,
	JAVA,
	LDAPAUTH,
	REJECT
};

int pop_droppriv __P((POP_INFO *, POP_AUTH *));
int pop_password_ldap __P((POP_INFO *, POP_AUTH *, int, char *));
int pop_password_mysql __P((POP_INFO *, POP_AUTH *, int, char *));
int pop_password_passwd __P((POP_INFO *, POP_AUTH *, int, char *));
int pop_password_pgsql __P((POP_INFO *, POP_AUTH *, int, char *));
int pop_password_textfile __P((POP_INFO *, POP_AUTH *, int, char *, int));

int
pop_read_pwdinfo(pinfo)
	POP_INFO *pinfo;
{
	int line;
	char buf[512], *ptr, *pdomain, *plocalip, *pauth, *pmail, *phash;
	char *puid, *pgid;
	char *pfile, *pmax;			/* Used for TEXTFILE */
	char *phostname, *pport, *pdatabase;	/* Used for SQLs */
	char *pdbuser, *pdbpass, *ptable;	/*     ditto     */
	char *puserrow, *ppassrow, *pmailrow;	/*     ditto     */
#ifdef HAVE_LDAP
	char *prootdn, *pbind;			/* Used for LDAPAUTH */
#endif

	FILE *fd;
	POP_AUTH *tmpauth, *lastauth;
	POP_AUTH_TEXT *ptextfile, *phtpasswd;
	POP_AUTH_SQL *psql;
#ifdef HAVE_LDAP
	POP_AUTH_LDAP *pldap;
#endif

	struct hostent *hp;
	lastauth = NULL;	/* Make -Wall happy */

	if ((tmpauth = malloc(sizeof(POP_AUTH))) == NULL) {
		syslog(LOG_CRIT, "memory problems");
		return (1);
	}
	memset(tmpauth, 0, sizeof(POP_AUTH));

	if ((fd = fopen(PWDINFO, "r")) == NULL) {
		/*
		 * There is no teapop.passwd file which we can read, yet we
		 * kinda need a default authmethod. Lets use SYSPASSWD for
		 * userid/password check and MAILBOX or MAILSPOOL, depending
		 * on what was specified during ./configure, as the place
		 * to find the mailbox.
		 */
		strcpy(tmpauth->domain, "empty");
		strcpy(tmpauth->localip, "*");
		tmpauth->authmethod = SYSPASSWD;
#if HOMEDIRSPOOL
		snprintf(tmpauth->maildrop, sizeof(tmpauth->maildrop),
		    "~/%s", MAILBOX);
#else
		strncpy(tmpauth->maildrop, MAILSPOOL,
		    sizeof(tmpauth->maildrop));
#endif /* HOMEDIRSPOOL */
		strcpy(tmpauth->uid, "+");
		strcpy(tmpauth->gid, "+");

		pinfo->firstauth = tmpauth;

		syslog(LOG_NOTICE,
		    "%s not found; using built-in authentication schema.",
		    PWDINFO);
		return (0);
	}

	line = 0;
	while (fgets(buf, sizeof(buf), fd)) {
		line++;

		/* lines longer then 512 are evil */
		if (buf[strlen(buf)-1] != '\n') {
			syslog(LOG_ERR, "line %d too long; ignoring it", line);
			while(fgets(buf, sizeof(buf), fd))
				if (buf[strlen(buf)-1] == '\n')
					break;
			continue;
	 	}

		/* Weed out comments */
		buf[strlen(buf)-1] = '\0';
		if ((ptr = strchr(buf, ';')) != NULL)
			*ptr = '\0';
		if ((ptr = strchr(buf, '#')) != NULL)
			*ptr = '\0';
		while (buf[0] != '\0' && buf[strlen(buf)-1] == ' ')
			buf[strlen(buf)-1] = '\0';
		while (buf[0] != '\0' && buf[0] == ' ')
			strcpy(buf, &buf[1]);
		if (buf[0] == '\0')
			continue;

		/* Start doing serious stuff */
		/* First extract the domain part from the line */
		pdomain = buf;
		if ((plocalip = strchr(buf, ':')) == NULL) {
			syslog(LOG_ERR, "line %d corrupt; please check the "
			    "syntax", line);
			continue;
		}
		*plocalip++ = '\0';

		/* Second is the localip part */
		if ((ptr = strchr(plocalip, ':')) == NULL) {
			syslog(LOG_ERR, "line %d corrupt; please check the "
			    "syntax", line);
			continue;
		}
		*ptr++ = '\0';
		if (plocalip[0] == '\0' || plocalip[0] == '*')
			strncpy(tmpauth->localip, "*",
			    sizeof(tmpauth->localip));
		else {
			/* by sg - added name resolving */
			if (pinfo->nodns == 0)
				hp = gethostbyname(plocalip);
			else
				hp = NULL;

			if (hp != NULL)
				strncpy(tmpauth->localip,
				    inet_ntoa(*(struct in_addr *)
				    hp->h_addr_list[0]),
				    sizeof(tmpauth->localip));
			else
				strncpy(tmpauth->localip, plocalip,
				    sizeof(tmpauth->localip));
		}

		/* 3rd arg is authtype */
		pauth = ptr;
		if ((ptr = strchr(pauth, ':')) == NULL &&
		    strcasecmp(pauth, "reject")) {
			syslog(LOG_ERR, "line %d corrupt; please check the "
			    "syntax", line);
			continue;
		}
		if (ptr != NULL)	/* ptr is NULL if authtype is reject */
			*ptr++ = '\0';

		/* If the authtype is reject there is no more args */
		if (!strcasecmp(pauth, "reject")) {
			strncpy(tmpauth->domain, pdomain,
			    sizeof(tmpauth->domain));
			tmpauth->authmethod = REJECT;
			if (pinfo->firstauth == NULL)
				pinfo->firstauth = tmpauth;
			else
				lastauth->nextauth = tmpauth;
			lastauth = tmpauth;
			if ((tmpauth = malloc(sizeof(POP_AUTH))) == NULL) {
				syslog(LOG_CRIT, "memory problems");
				return (1);
			}
			memset(tmpauth, 0, sizeof(POP_AUTH));
			continue;
		}

		/* 4th arg is path to mailbox */
		pmail = ptr;
		if ((ptr = strchr(pmail, ':')) == NULL) {
			syslog(LOG_ERR, "line %d corrupt; please check the "
			    "syntax", line);
			continue;
		}
		*ptr++ = '\0';

		/* 5th arg is hash-level */
		phash = ptr;
		if ((ptr = strchr(phash, ':')) == NULL) {
			syslog(LOG_ERR, "line %d corrupt; please check the "
			    "syntax", line);
			continue;
		}
		*ptr++ = '\0';

		/*
		 * That's all common args for the different authtypes.
		 * Put what we have so far into tmpauth since it's supposed
		 * to go there sooner or later anyway. Now it's just to start
		 * doing the parsing of the specific args.
		 */
		strncpy(tmpauth->domain, pdomain, sizeof(tmpauth->domain));
		strncpy(tmpauth->maildrop, pmail, sizeof(tmpauth->maildrop));
		tmpauth->hash = atoi(phash);
		if (!strcasecmp(pauth, "passwd")) {
			/* ***************** */
			/* AUTHYPE IS PASSWD */
			/* ***************** */
			tmpauth->authmethod = SYSPASSWD;
		
			/* yes I know this next bit is ugly */

			/* 6th arg is optional UID */
			puid = ptr;
			if ((ptr = strchr(puid, ':')) == NULL) {
				strcpy(tmpauth->uid, "+");
				strcpy(tmpauth->gid, "+");
			} else {
				*ptr++ = '\0';

				/* deal with blanks as for default */
				strcpy(tmpauth->uid,
				       (strlen(puid) == 0)?"+":puid);

				/* 7th arg is optional GID */
				pgid = ptr;
				if ((ptr = strchr(pgid, ':')) == NULL) {
					syslog(LOG_ERR, "line %d corrupt; "
					    "please check the syntax", line);
					continue;
				}
				*ptr++ = '\0';

				/* deal with blanks as for default */
				strcpy(tmpauth->gid,
				       (strlen(pgid) == 0)?"+":pgid);
			}
			/* end of ugly bit ;) */

		} else if (!strcasecmp(pauth, "textfile")) {
			/* ******************* */
			/* AUTHYPE IS TEXTFILE */
			/* ******************* */
			tmpauth->authmethod = TEXTFILE;

			/* 6th arg is UID */
			puid = ptr;
			if ((ptr = strchr(puid, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 7th arg is GID */
			pgid = ptr;
			if ((ptr = strchr(pgid, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 8th arg is the password file */
			pfile = ptr;
			if ((ptr = strchr(pfile, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 9th arg is max users */
			pmax = ptr;
			if ((ptr = strchr(pmax, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			strcpy(tmpauth->uid, puid);
			strcpy(tmpauth->gid, pgid);

			ptextfile = malloc(sizeof(POP_AUTH_TEXT));
			if (ptextfile == NULL) {
				memset(tmpauth, 0, sizeof(POP_AUTH));
				syslog(LOG_CRIT, "memory problems");
				return (1);
			}
			memset(ptextfile, 0, sizeof(POP_AUTH_TEXT));
			strcpy(ptextfile->file, pfile);
			ptextfile->max = atoi(pmax);
			tmpauth->extra = ptextfile;
		} else if (!strcasecmp(pauth, "htpasswd")) {
			/* ******************* */
			/* AUTHYPE IS HTPASSWD */
			/* ******************* */
			tmpauth->authmethod = HTPASSWD;

			/* 6th arg is UID */
			puid = ptr;
			if ((ptr = strchr(puid, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 7th arg is GID */
			pgid = ptr;
			if ((ptr = strchr(pgid, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 8th arg is the password file */
			pfile = ptr;
			if ((ptr = strchr(pfile, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 9th arg is max users */
			pmax = ptr;
			if ((ptr = strchr(pmax, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			strcpy(tmpauth->uid, puid);
			strcpy(tmpauth->gid, pgid);

			phtpasswd = malloc(sizeof(POP_AUTH_TEXT));
			if (phtpasswd == NULL) {
				memset(tmpauth, 0, sizeof(POP_AUTH));
				syslog(LOG_CRIT, "memory problems");
				return (1);
			}
			memset(phtpasswd, 0, sizeof(POP_AUTH_TEXT));
			strcpy(phtpasswd->file, pfile);
			phtpasswd->max = atoi(pmax);
			tmpauth->extra = phtpasswd;
		} else if (0 ||
#ifdef HAVE_MYSQL
		    !strcasecmp(pauth, "mysql") ||
#endif /* HAVE_MYSQL */
#ifdef HAVE_PGSQL
		    !strcasecmp(pauth, "pgsql") ||
#endif /* HAVE_PGSQL */
#ifdef HAVE_ORACLE
		    !strcasecmp(pauth, "oracle") ||
#endif /* HAVE_ORACLE */
		    0) {
			/* ********************* */
			/* AUTHYPE IS A DATABASE */
			/* ********************* */

			/*
			 * Since all databases require the same info we'll do
			 * them all in the same place. This will however need
			 * an extra if-statement, but the few extra CPU cycles
			 * it will use is well worth the code reusing.
			 */
			/* 6th arg is UID */
			puid = ptr;
			if ((ptr = strchr(puid, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 7th arg is GID */
			pgid = ptr;
			if ((ptr = strchr(pgid, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 8th arg is HOSTNAME of database server */
			phostname = ptr;
			if ((ptr = strchr(phostname, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 9th arg is port */
			pport = ptr;
			if ((ptr = strchr(pport, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 10th arg is the name of the database */
			pdatabase = ptr;
			if ((ptr = strchr(pdatabase, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 11th arg is the login name to the db */
			pdbuser = ptr;
			if ((ptr = strchr(pdbuser, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 12th arg is the password to the db */
			pdbpass = ptr;
			if ((ptr = strchr(pdbpass, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 13th arg is the table name */
			ptable = ptr;
			if ((ptr = strchr(ptable, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 14th arg is name of the row that contains the uid */
			puserrow = ptr;
			if ((ptr = strchr(puserrow, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 15th arg is name of the row that contains password */
			ppassrow = ptr;
			if ((ptr = strchr(ppassrow, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 16th arg is name of the row with maildrop info */
			pmailrow = ptr;
			if ((ptr = strchr(pmailrow, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			psql = malloc(sizeof(POP_AUTH_SQL));
			if (psql == NULL) {
				memset(tmpauth, 0, sizeof(POP_AUTH));
				syslog(LOG_CRIT, "memory problems");
				return (1);
			}
			memset(psql, 0, sizeof(POP_AUTH_SQL));
			strcpy(tmpauth->uid, puid);
			strcpy(tmpauth->gid, pgid);
			if (!strcasecmp(pauth, "mysql"))
				tmpauth->authmethod = MYSQL;
			else if (!strcasecmp(pauth, "pgsql"))
				tmpauth->authmethod = PGSQL;
			else if (!strcasecmp(pauth, "oracle"))
				tmpauth->authmethod = ORACLE;
			strncpy(psql->host, phostname, sizeof(psql->host) - 1);
			psql->port = strdup(pport);
			strncpy(psql->db, pdatabase, sizeof(psql->db) - 1);
			strncpy(psql->username, pdbuser,
			    sizeof(psql->username) - 1);
			strncpy(psql->password, pdbpass,
			    sizeof(psql->password) - 1);
			strncpy(psql->table, ptable, sizeof(psql->table) - 1);
			strncpy(psql->userrow, puserrow,
			    sizeof(psql->userrow) - 1);
			strncpy(psql->passrow, ppassrow,
			    sizeof(psql->passrow) - 1);
			strncpy(psql->mailrow, pmailrow,
			    sizeof(psql->mailrow) - 1);
			tmpauth->extra = psql;
		} else if ( 0 ||
#ifdef HAVE_JAVA
		!strcasecmp(pauth,"java") ||
#endif /* HAVE_JAVA */
		0) {
			tmpauth->authmethod = JAVA;
			/* 6th arg is UID */
			puid = ptr;
			if ((ptr = strchr(puid, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 7th arg is GID */
			pgid = ptr;
			if ((ptr = strchr(pgid, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			strcpy(tmpauth->uid, puid);
			strcpy(tmpauth->gid, pgid);

#ifdef HAVE_LDAP
		} else if (!strcasecmp(pauth,"ldap")) {
			/* *************** */
			/* AUTHYPE IS LDAP */
			/* *************** */

			/* 6th is hostname */
			phostname = ptr;
			if ((ptr = strchr(phostname, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 7th is port */
			pport = ptr;
			if ((ptr = strchr(pport, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 8th is root DN */
			prootdn = ptr;
			if ((ptr = strchr(prootdn, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/* 9th is bind method */
			pbind = ptr;
			if ((ptr = strchr(pbind, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt; please "
				    "check the syntax", line);
				continue;
			}
			*ptr++ = '\0';

			/*
			 * TODO: Add argument to define wich attributes
			 *       gets from LDAP
			 */
			strcpy(tmpauth->uid, "+");
			strcpy(tmpauth->gid, "+");
			
			/* Create POP_AUTH_LDAP object */
			pldap = (POP_AUTH_LDAP *)malloc(sizeof(POP_AUTH_LDAP));
			if (pldap == NULL) {
				memset(tmpauth, 0, sizeof(POP_AUTH));
				syslog(LOG_CRIT, "memory problem");
				return (1);
			}
			memset(pldap, 0, sizeof(POP_AUTH_LDAP));

			/* Fill data from config file */
			strncpy(pldap->host, phostname, sizeof(pldap->host)-1);
			pldap->port = atoi(pport);
			strncpy(pldap->rootdn, prootdn,
			    sizeof(pldap->rootdn)-1);

			/* Detect bind method */
			/* uses of TLS */
			if (!strcasecmp(pbind, "simple"))
				pldap->authmethod = LDAP_AUTH_SIMPLE;
			else if (!strcasecmp(pbind, "tls")) {
				pldap->authmethod = LDAP_AUTH_SIMPLE;
				pldap->useTLS = 1;
			}

			tmpauth->extra = pldap;
			tmpauth->authmethod = LDAPAUTH;
#endif /* HAVE_LDAP */
		} else {
			/* *************** */
			/* INVALID AUTHYPE */
			/* *************** */
			syslog(LOG_ERR, "invalid authtype on line %d", line);
			continue;
		}

		/*
		 * Ok, that's it. We now have a full POP_AUTH structure. Add
		 * it into the list of authinfo and allocate memory for a
		 * new authtype.
		 */
		if (pinfo->firstauth == NULL)
		 	pinfo->firstauth = tmpauth;
		else
			lastauth->nextauth = tmpauth;
		lastauth = tmpauth;
		if ((tmpauth = malloc(sizeof(POP_AUTH))) == NULL) {
			syslog(LOG_CRIT, "memory problems");
			return (1);
		}
		memset(tmpauth, 0, sizeof(POP_AUTH));
	}

	/* There will always be one unused but malloc()'ed tmpauth left */
	free(tmpauth);

	/* Close PWDINFO file (rude not to) */
	fclose(fd);

	return (0);
}

int
pop_verify_password(pinfo, passwd, isapop)
	POP_INFO *pinfo;
	char *passwd;
	int isapop;
{
	int state;

	POP_AUTH *curauth;
	
	/*
	 * state is somewhat a magic variable. It tells the loop what to do
	 * when curauth becomes NULL. It must always start defined as 2 (two).
	 *
	 * The different values mean:
	 * 0 - When we've reached NULL, end the loop. This is used when we
	 *     don't want to try "default"-entries. It's set to 0 after finding
	 *     one entry which matches the domain the user sent, unless
	 *     --enable-keeptrying and --enable-trydefault was used.
	 * 1 - When NULL is reached start again from the beginning but this
	 *     time look for "default"-entries instead of an entry that
	 *     matches the domain. There are two different ways state can
	 *     be set to 1. The first way is that we've gone through all
	 *     auth-records and not found one that matched the domain the
	 *     user sent. The second requires that teapop was configured
	 *     with both --enable-keeptrying and --enable-trydefault,
	 *     which then will cause state to be set to 1 when we've checked
	 *     all matching auth-entries and not found one that had the
	 *     same user/pass as the user sent.
	 * 2 - This is the default, and causes the loop to check matching
	 *     auth-records.
	 * 3 - User is authenticated (got correct USER/PASS or APOP).
	 */
	state = 2;

	curauth = pinfo->firstauth;
	while (curauth != NULL && state != 3) {
		if (((!strcasecmp(curauth->domain, pinfo->domain) && state != 1)
		    || (pinfo->domain[0] == '\0' && !strcasecmp("empty",
		    curauth->domain) && state != 1) ||
		    (!strcasecmp("default", curauth->domain) && state == 1)) &&
		    (!strcasecmp("*", curauth->localip) ||
		    !strcasecmp(curauth->localip,pinfo->localip))) {
			switch (curauth->authmethod) {
			case SYSPASSWD:
				if (pop_password_passwd(pinfo, curauth,
				    isapop, passwd) == 0)
					state = 3;
				break;
			case TEXTFILE:
				if (pop_password_textfile(pinfo, curauth,
				    isapop, passwd, 0) == 0)
					state = 3;
				break;
			case HTPASSWD:
				if (pop_password_textfile(pinfo, curauth,
				    isapop, passwd, 1) == 0)
					state = 3;
				break;
#ifdef HAVE_PGSQL
			case PGSQL:
				if (pop_password_pgsql(pinfo, curauth, isapop,
				    passwd) == 0)
					state = 3;
				break;
#endif /* HAVE_PGSQL */
#ifdef HAVE_MYSQL
			case MYSQL:
				if (pop_password_mysql(pinfo, curauth, isapop,
				    passwd) == 0)
					state = 3;
				break;
#endif /* HAVE_MYSQL */
#ifdef HAVE_ORACLE
			case ORACLE:
				if (pop_password_oracle(pinfo, curauth, isapop,
				    passwd) == 0)
					state = 3;
				break;
#endif /* HAVE_ORACLE */
#ifdef HAVE_JAVA
			case JAVA:
				if (pop_password_java(pinfo, curauth, isapop,
				    passwd) == 0)
					state = 3;
				break;
#endif /* HAVE_JAVA */
#ifdef HAVE_LDAP
			case LDAPAUTH:
				if (pop_password_ldap(pinfo, curauth, isapop,
				    passwd) == 0)
					state = 3;
				break;
#endif /* HAVE_LDAP */
			case REJECT:	/* FALLTHROUGH */
			default:
				break;
			}
#ifndef KEEPTRYING
			if (state != 3)
				return (1);
#endif /* KEEPTRYING */
#ifndef TRYDEFAULT
			if (state == 2)
				state = 0;
#endif
		}
		if (state != 3) {
			curauth =  curauth->nextauth;
			if (curauth == NULL && state == 2) {
				state = 1;
				curauth = pinfo->firstauth;
			}
		}
	}

	if (state == 3) {
		if (pinfo->maildrop[strlen(pinfo->maildrop)-1] == '/')
			pinfo->mboxtype = 1;

		/* First run POP-before-SMTP routines and then drop privs */
		pop_pop_before_smtp(pinfo);
		return (pop_droppriv(pinfo, curauth));
	}

	return (1);
}

int
pop_password_passwd(pinfo, auth, isapop, passwd)
	POP_INFO *pinfo;
	POP_AUTH *auth;
	int isapop;
	char *passwd;
{
	char *encpw;

	struct passwd *userinfo;
#ifdef HAVE_SHADOW_H
	struct spwd *suserinfo;
#endif

	/* We can't do APOP with system password, bail out. */
	if (isapop == 1)
		return (1);

	if ((userinfo = getpwnam(pinfo->userid)) == NULL)
		return (1);
#ifdef HAVE_SHADOW_H
	if ((suserinfo = getspnam(pinfo->userid)) == NULL)
		return (1);
	encpw = crypt(passwd, suserinfo->sp_pwdp);
	if (strcmp(encpw, suserinfo->sp_pwdp))
		return (1);
#else
	encpw = crypt(passwd, userinfo->pw_passwd);
	if (strcmp(encpw, userinfo->pw_passwd))
		return (1);
#endif

	if (auth->maildrop == NULL) {
		/* We are running w/o a teapop.passwd file */
#ifdef HOMEDIRSPOOL
		strncpy(pinfo->chroot, userinfo->pw_dir,
		    sizeof(pinfo->chroot) - 1);
		pinfo->chroot[sizeof(pinfo->chroot) - 1] = '\0';
		strncpy(pinfo->maildrop, MAILBOX, sizeof(pinfo->maildrop) - 1);
		pinfo->maildrop[sizeof(pinfo->maildrop) - 1] = '\0';
#else
		strncpy(pinfo->chroot, MAILSPOOL, sizeof(pinfo->chroot) - 1);
		pinfo->chroot[sizeof(pinfo->chroot) - 1] = '\0';
		strncpy(pinfo->maildrop, pinfo->userid,
		    sizeof(pinfo->maildrop) - 1);
		pinfo->maildrop[sizeof(pinfo->maildrop) - 1] = '\0';
#endif /* HOMEDIRSPOOL */
	} else {
		if (auth->maildrop[0] == '~') {
			strncpy(pinfo->chroot, userinfo->pw_dir,
			    sizeof(pinfo->chroot) - 1);
			pinfo->chroot[sizeof(pinfo->chroot) - 1] = '\0';
			strncpy(pinfo->maildrop, &auth->maildrop[1],
			    sizeof(pinfo->maildrop) - 1);
			pinfo->maildrop[sizeof(pinfo->maildrop) - 1] = '\0';
		} else {
			strncpy(pinfo->chroot, auth->maildrop,
			    sizeof(pinfo->chroot) - 1);
			pinfo->chroot[sizeof(pinfo->chroot) - 1] = '\0';
			strncpy(pinfo->maildrop, pinfo->userid,
			    sizeof(pinfo->maildrop) - 1);
			pinfo->maildrop[sizeof(pinfo->maildrop) - 1] = '\0';
		}
	}

	/*
	 * If pinfo->chroot ends with a slash, the admin configured
	 * teapop.passwd to use Maildir. However, we need to move that
	 * slash over to pinfo->maildrop, since that's what Teapop looks
	 * for.
	 */
	if (pinfo->chroot[0] != '\0' &&
	    pinfo->chroot[strlen(pinfo->chroot) - 1] == '/') {
		pinfo->chroot[strlen(pinfo->chroot) - 1] = '\0';
		strncat(pinfo->maildrop, "/", sizeof(pinfo->maildrop));
	}

	return (0);
}

int
pop_password_textfile(pinfo, auth, isapop, passwd, htpasswd)
	POP_INFO *pinfo;
	POP_AUTH *auth;
	int isapop;
	char *passwd;
	int htpasswd;
{
	int lines, readlines, counter;
	char *puser, *ppass, *pmbox = NULL, *ptr;
	char buf[512], buf2[512], md5pass[32];
	unsigned char digest[16];

	FILE *fd;
	POP_AUTH_TEXT	*ptext;
	MD5_CTX ctx;

	struct stat sb;

	ptext = auth->extra;

	if ((fd = fopen(ptext->file, "r")) == NULL) {
		syslog(LOG_ERR, "Password file %s doesn't exist "
		    "or isn't readable.", ptext->file);
		return (1);
	}
	fstat(fileno(fd), &sb);
	if (!S_ISREG(sb.st_mode) || sb.st_nlink != 1) {
		fclose(fd);
		syslog(LOG_ERR, "Password file %s not a normal file",
		    ptext->file);
		return (1);
	}

	lines = readlines = 0;
	while (fgets(buf, sizeof(buf), fd)) {
		readlines++;
		if (buf[strlen(buf)-1] != '\n') {
			while (buf[strlen(buf)-1] != '\n' && !feof(fd))
				fgets(buf, sizeof(buf), fd);
			continue;
		}
		while (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';
		while (buf[0] != '\0' && isspace((int)buf[0]))
			strcpy(&buf[0], &buf[1]);
		if (buf[0] == '#' || buf[0] == ';')
			continue;
		if (++lines > ptext->max) {
			syslog(LOG_ERR, "Reached max number of users for %s",
			    auth->domain);
			return (1);

		}

		puser = buf;
		if ((ppass = strchr(puser, ':')) == NULL) {
			syslog(LOG_ERR, "line %d corrupt in password file for "
			    "%s", readlines, auth->domain);
			return (1);
		}
		*ppass++ = '\0';
		if (htpasswd == 0) {
			if ((pmbox = strchr(ppass, ':')) == NULL) {
				syslog(LOG_ERR, "line %d corrupt in password "
				    "file for %s", readlines, auth->domain);
				return (1);
			}
			*pmbox++ = '\0';
		}

		if (!strcmp(pinfo->userid, puser)) {
			if (isapop == 1) {
				snprintf(buf2, sizeof(buf2), "%s%s",
				    pinfo->apopstr, ppass);
				MD5Init(&ctx);
				MD5Update(&ctx, (unsigned char *)buf2 ,
				    strlen(buf2));
				MD5Final(digest, &ctx);

				ptr = md5pass;
	                        for (counter = 0; counter < sizeof(digest);
        	                    counter++, ptr += 2)
                	                sprintf(ptr, "%02x",
					    digest[counter] & 0xFF);
				*ptr   = '\0';
			}

			if (htpasswd == 0) {
				if ((isapop == 0 && !strcmp(passwd, ppass)) ||
				    (isapop == 1 && !strcmp(passwd, md5pass))) {
					strncpy(pinfo->chroot, auth->maildrop,
					    sizeof(pinfo->chroot));
					strncpy(pinfo->maildrop, pmbox,
					    sizeof(pinfo->maildrop));
					fclose(fd);
					return (0);
				}
			} else {
				if ((ptr = strchr(ppass, ':')) != NULL)
					*ptr = '\0';
				ptr = crypt(passwd, ppass);
				if (!strcmp(ptr, ppass) && isapop == 0) {
					strncpy(pinfo->chroot, auth->maildrop,
					    sizeof(pinfo->chroot));
					strncpy(pinfo->maildrop, puser,
					    sizeof(pinfo->maildrop));
					fclose(fd);
					return (0);
				}
			}
		}
	}

	fclose(fd);
	return (1);
}
