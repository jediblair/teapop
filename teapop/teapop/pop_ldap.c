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

/* LDAP support auth mech
 * Alexandre Ghisoli (alex@ycom.ch)
 * This is the first version of support. Also, I'm not a LDAP programmer,
 * and there is many imporvment to make.
 * Please feel free to patch this code.
 * Let me informed.
 */

#include "config.h"

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <ldap.h>

#include "teapop.h"

#ifndef SOCKETFILE
#define SOCKETFILE NULL
#endif

/* Global if someone is ready to code ldap_rebind */
LDAP *ld;
LDAPMessage *result;
LDAPMessage *entry;

int
pop_password_ldap (POP_INFO *pinfo, POP_AUTH *auth, int isapop, char *passwd)
{

	int ldapversion;
	int res;
	char cred[255];			/* Credentials */
	char msg[1024];
	char search[255];		/* Search string */

	POP_AUTH_LDAP *pldap;

	/* Get LDAP sepcific data */
	pldap = auth->extra;

	if (pinfo->domain == "") {
		syslog(LOG_ERR, "Domain Empty");
		return (1);
	}

	/* Try to open LDAP connexion */
	/* Uses of ldap_init() is preferred by OpenLDAP staff */
	ld = ldap_init(pldap->host, pldap->port);
	if (ld == NULL) {
		syslog(LOG_ERR, "Cannot ldap_init()");
		return (1);
	}

	/* Start TLS if required */
	if (pldap->useTLS == 1) {
		/* syslog(LOG_DEBUG, "Setup TLS"); */
		ldapversion = LDAP_VERSION3;
		if (ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION,
		    &ldapversion) != LDAP_OPT_SUCCESS) {
			syslog(LOG_ERR, "Cannot set LDAP_VERSION3");
			return (1);
		}

		if (ldap_start_tls_s (ld, NULL, NULL) != LDAP_SUCCESS) {
			syslog(LOG_ERR, "Cannot start TLS");
			return (1);
		}
	}

	/* Bind to server */

	/* ERROR : Must be hardcoded, why ? */
	pldap->authmethod = LDAP_AUTH_SIMPLE;

	/* Some infos :
	 * This is an anonymous bind to LDAP server.
	 * The first connection return the mailRoutingAddress for user.
	 * This *must* be the username of the user. If not, this is not
	 * a local mailbox. Then, after lookup for the username (userid),
	 * we made another bind, with the user supplied cred / pasword.
	 * If this bind is done, then user is identified.
	 */
	res = ldap_bind_s(ld, NULL, NULL, pldap->authmethod);
	if (res != LDAP_SUCCESS) {
		snprintf(msg, sizeof(msg), ldap_err2string(res));
		msg[sizeof(msg)-1] = '\0';
		syslog(LOG_ERR, "LDAP error- %s", msg);
		return (1);
	}

	/* Create search pattern */
	snprintf(search, sizeof(search), "(mailLocalAddress=%s@%s)",
	    pinfo->userid, pinfo->domain);
	search[sizeof(search) - 1] = '\0';

	if (ldap_search(ld, NULL, LDAP_SCOPE_SUBTREE, search, NULL, 0) == -1) {
		syslog(LOG_ERR, "LDAP error- Search failed");
		return (1);
	}

	/* Get results */
	if (ldap_result(ld, LDAP_RES_ANY, 1, NULL,  &result) == -1) {
		syslog(LOG_ERR, "LDAP error- Get entries failed");
		return (1);
	}

	if (ldap_count_entries(ld, result) == 0)
		return (1);

	/* We have at least 1 reccord */
	entry = ldap_first_entry(ld, result);
	strcpy(pinfo->userid, *ldap_get_values(ld, entry, "uid"));

	ldap_msgfree(result);
	ldap_unbind_s(ld);

	/* Open the second connection, for authenticate the user */
	ld = ldap_init(pldap->host, pldap->port);
	if (ld == NULL) {
		syslog(LOG_ERR, "Cannot ldap_init()");
		return (1);
	}

	/* Start TLS if required */
	if (pldap->useTLS == 1) {
		/* syslog (LOG_DEBUG, "Setup TLS"); */
		ldapversion = LDAP_VERSION3;
		if (ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION,
		    &ldapversion) != LDAP_OPT_SUCCESS) {
			syslog(LOG_ERR, "Cannot set LDAP_VERSION3");
			return (1);
		}

		if (ldap_start_tls_s(ld, NULL, NULL) != LDAP_SUCCESS) {
			syslog (LOG_ERR, "Cannot start TLS");
			return (1);
		}
	}

	/* Bind to server */
	/* ERROR : Must be hardcoded, why ? */
	pldap->authmethod = LDAP_AUTH_SIMPLE;
	/* syslog (LOG_DEBUG, "Create Credentials"); */
	snprintf(cred, sizeof(cred),  "uid=%s,%s", pinfo->userid,
	    pldap->rootdn);
	cred[sizeof(cred) - 1] = '\0';
	/*  syslog (LOG_DEBUG, "Cred - DN> %s", cred); */

	res = ldap_bind_s(ld, cred, passwd, pldap->authmethod);
	if (res == LDAP_INVALID_CREDENTIALS) {
		syslog(LOG_DEBUG, "Wrong password for %s", pinfo->userid);
		return (1);
	}  else if (res != LDAP_SUCCESS) {
		syslog (LOG_DEBUG, "!! - %s", ldap_err2string (res));
		syslog (LOG_ERR, "Error on ldap_bind()");
		return (1);
	}

	/*   syslog(LOG_DEBUG, "User OK!!!"); */
	ldap_unbind_s(ld);

	/* Give info about MailBox */
	pinfo->mboxtype = 0;
	strncpy(pinfo->chroot, auth->maildrop, sizeof(pinfo->chroot));
	strncpy (pinfo->maildrop, pinfo->userid, sizeof(pinfo->maildrop));

	return (0);
}
