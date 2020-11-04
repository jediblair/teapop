/* $Id: //depot/Teapop/0.3/teapop/pop_mysql.c#5 $ */

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

#include "config.h"

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#ifndef HAVE_MD5_H
#include "md5.h"
#else
#include <md5.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

#include <mysql.h>

#include "teapop.h"

#ifndef SOCKETFILE
#define SOCKETFILE NULL
#endif

extern int he_is_mysql_user;

#define LEN_SERV   60
#define LEN_LOGIN  16
#define LEN_TABLE  64

#define LEN_BUFFER LEN_TABLE+20

int	mysql_keepopen = 0;		/* Keep connection open */
int	mysql_acct_keepopen;
int	check_host = 0;			/* Check host validity */
char	sep_char = '@';
int	check_quota = 0;			/* Check quota */
int	ignore_validity = 1;		/* Check "date expiration" and "activity" status */

MYSQL	real_mysql, *mysql = NULL;
MYSQL_RES      *result;

#define TRY_N 10

/**********************************************/
int
Mysql_Reconnect(psql)
POP_AUTH_SQL	*psql;
{
	int i ;

	i = 0;
	while (i < TRY_N) {	/* Try it 10 Times  */
		mysql_init(&real_mysql);
		if (!(mysql=mysql_real_connect(&real_mysql, psql->host,
		    psql->username, psql->password, NULL, atoi(psql->port),
		    SOCKETFILE, 0)))
			mysql = NULL;
		else {
			mysql = &real_mysql;
			break;
		}
		i++;
		sleep(1);
	}

	if (mysql != NULL) {
		i = 0;
		while (i < TRY_N) {	/* Try it 10 Times */
			if  (mysql_select_db(mysql, psql->db)) {
			} else {
				return (0);
			}
			i++;
			sleep(1);
		}
	}

	return (-1);
}


/*******************************/
void
My_Mysql_Close(int keepopen_flag)
{
	if (keepopen_flag == 0) {
		/* Close the connection after each record */
		if (mysql != NULL) {
			mysql_close(mysql);
		}
		mysql = NULL;
	}
}


int
pop_password_mysql(pinfo, auth, isapop, passwd)
	POP_INFO *pinfo;
	POP_AUTH *auth;
	int isapop;
	char *passwd;
{
	POP_AUTH_SQL	*psql;
	MD5_CTX		ctx;
	char	*ptr, buf2[512], *ppass, md5pass[32], euserid[BIGSTRING*2+1];
	int	counter;
	unsigned char digest[16];
	MYSQL_RES *result;
	MYSQL_ROW row;
	char mybuf[1024];
	int i, c, err, len;
	char *pname; char *pgecos; char *encpw;

	psql = auth->extra;

	/* ===================== */
	if (mysql == NULL) {
		err = Mysql_Reconnect(psql);
		if (err==-1) {
			syslog(LOG_ERR,
			    "Cannot connect to MySQL Server (ret -1)");
			return (1);
		}
	}
	if (!mysql) {
		syslog(LOG_ERR, "Cannot connect to MySQL Server (ret 0)");
		return (1);
	}

	/* ===================== */
	mysql_real_escape_string(mysql, euserid, pinfo->userid,
	    strlen(pinfo->userid));
	sprintf(mybuf, "SELECT %s, %s%s%s FROM %s WHERE %s = '%s'",
	    psql->userrow, psql->passrow, (*(psql->mailrow) == NULL ? " " : ", "),
	    psql->mailrow, psql->table, psql->userrow, euserid);

	len = strlen(psql->userrow);
	if (check_host) {
		i = 0;
		while (i < len && (c = *(psql->userrow + i)) != sep_char)
			i++;
		if (i < len && isgraph(*(psql->userrow+i)) ) {
			sprintf(mybuf, "%s AND mbox_host = '%s'", mybuf,
			    (psql->userrow + i + 1));
		} else {
			strcat(mybuf, "' AND (mbox_host IS NULL OR mbox_host='')" );
		}
	}

	/* --- ignore_validity --- */
	if (ignore_validity == 1) {
		strcat(mybuf, " AND active=1" );	
	} else {
		strcat(mybuf, " AND active=1 AND start_date<=curdate() AND expire_date>curdate()" );
	}

	/* --- Query! --- */
	if (mysql_query(mysql, mybuf) < 0) {
		My_Mysql_Close(mysql_keepopen);
		syslog(LOG_ERR, "Error in Query record (check fields names)");
		return (1);
	}
	if (!(result=mysql_store_result(mysql))) {
		My_Mysql_Close(mysql_keepopen);
		syslog(LOG_ERR, "Query returned nothing - check MySQL server");
		return (1);
	}
	if (mysql_num_rows(result) < 1) {
		My_Mysql_Close(mysql_keepopen);
		syslog(LOG_ERR, "Query result is incomplete");
		return (1);
	}

	/* Successful query, close the connection nicely */
	My_Mysql_Close (mysql_keepopen);

	/* OK. Parse result */
	row = mysql_fetch_row(result);
	len = strlen(row[0]) + 1;
	pname = (char *)malloc(sizeof(char) * len);
	pgecos = (char *)malloc(sizeof(char) * len);
	strcpy(pname, row[0]);
	strcpy(pgecos,row[0]);

	len = strlen(row[1]) + 1;
	ppass = (char *)malloc(sizeof(char) * len);
	strcpy(ppass,row[1]);

	if (isapop == 1) { /* Was 1 */
		snprintf(buf2, sizeof(buf2), "%s%s", pinfo->apopstr, ppass);
		MD5Init(&ctx);
		MD5Update(&ctx, (unsigned char *)buf2, strlen(buf2));
		MD5Final(digest, &ctx);
		ptr = md5pass;
		for (counter = 0; counter < sizeof(digest); counter++, ptr += 2)
			sprintf(ptr, "%02x", digest[counter] & 0xFF);
		*ptr = '\0';
	}

	encpw = crypt((char *)passwd, (char *)ppass);

	if ((isapop == 0 && !strcmp(passwd, ppass)) ||
	   (isapop == 1 && !strcmp(passwd, md5pass)) ||
	   !(strcmp (encpw, ppass))) {
		strncpy(pinfo->chroot, auth->maildrop, sizeof(pinfo->chroot));
		if (psql->mailrow[0] != '\0') {
			ptr = row[2];
			if (ptr[0] != '\0')
				strncpy(pinfo->maildrop, ptr,
				    sizeof(pinfo->maildrop));
			else
				strncpy(pinfo->maildrop, pinfo->userid,
				    sizeof(pinfo->maildrop));
		} else {
			strncpy(pinfo->maildrop, pinfo->userid,
			    sizeof(pinfo->maildrop));
		}
		return (0);
	}
	return (1);
}
