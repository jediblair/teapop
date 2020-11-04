/* $Id: //depot/Teapop/0.3/teapop/pop_pgsql.c#5 $ */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <libpq-fe.h>

#ifndef HAVE_MD5_H
#include "md5.h"
#else
#include <md5.h>
#endif

#include "teapop.h"

int
pop_password_pgsql(pinfo, auth, isapop, passwd)
        POP_INFO *pinfo;
        POP_AUTH *auth;
        int isapop;
        char *passwd;
{
	char	*ptr, buf2[512], *ppass, md5pass[32], euserid[BIGSTRING*2+1];
	int	nTuples, counter;
	unsigned char digest[16];

	PGconn		*conn;
	PGresult	*res;
	POP_AUTH_SQL	*psql;
	MD5_CTX		ctx;

	psql = auth->extra;

	conn = PQsetdbLogin(psql->host, psql->port, NULL, NULL, psql->db,
	    psql->username, psql->password);

	/* Did we manage to connect? */
	if (PQstatus(conn) == CONNECTION_BAD) {
		syslog(LOG_ERR, "Could not connect to PostgreSQL server");
		PQfinish(conn);
		return (1);
	}

	res = PQexec(conn, "BEGIN");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		syslog(LOG_ERR,
		    "Problems communicating with PostgreSQL server (BEGIN)");
		PQclear(res);
		PQfinish(conn);
		return (1);
	}
	PQclear(res);
	
	if ((ptr = malloc(256)) == NULL) {
		syslog(LOG_CRIT, "memory problems");
		return (1);
	}

	PQescapeString(euserid, pinfo->userid, strlen(pinfo->userid));
	snprintf(ptr, 256, "SELECT %s, %s%s%s FROM %s WHERE %s = '%s'",
	    psql->userrow, psql->passrow, (*(psql->mailrow) == NULL ? " " : ", "),
	    psql->mailrow, psql->table, psql->userrow, euserid);
	res = PQexec(conn, ptr);

	free(ptr);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		syslog(LOG_ERR,
		    "Problems communicating with PostgreSQL server (SELECT)");
		PQclear(res);
		PQfinish(conn);
		return (1);
	}
	
	nTuples = PQntuples(res);
	if (nTuples == 0) {
		PQclear(res);
		PQfinish(conn);
		return (1);
	}
	ppass = PQgetvalue(res,0,1);

	if (isapop == 1) {
		snprintf(buf2, sizeof(buf2), "%s%s", pinfo->apopstr, ppass);
		MD5Init(&ctx);
		MD5Update(&ctx, (unsigned char *)buf2, strlen(buf2));
		MD5Final(digest, &ctx);
		ptr = md5pass;
		for (counter = 0; counter < sizeof(digest); counter++, ptr += 2)
			sprintf(ptr, "%02x", digest[counter] & 0xFF);
		*ptr   = '\0';
	}

	if ((isapop == 0 && !strcmp(passwd, ppass)) ||
	    (isapop == 1 && !strcmp(passwd, md5pass))) {
		strncpy(pinfo->chroot, auth->maildrop, sizeof(pinfo->chroot));
		if (psql->mailrow[0] != '\0') {
			ptr = PQgetvalue(res, 0, 2);
			if (ptr[0] != '\0')
				strncpy(pinfo->maildrop, ptr,
				    sizeof(pinfo->maildrop));
			else
				strncpy(pinfo->maildrop, pinfo->userid,
				    sizeof(pinfo->maildrop));
		} else
			strncpy(pinfo->maildrop, pinfo->userid,
			    sizeof(pinfo->maildrop));
		PQclear(res);
		PQfinish(conn);
		return (0);
	}

	PQclear(res);
	PQfinish(conn);
	return (1);
}
