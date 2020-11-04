/* $Id$ */

/*
 * Copyright (c) 2001 Stephan Uhlmann <su@su2.info>
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

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "teapop.h"

#ifndef POPAUTH_FILE
#define POPAUTH_FILE	VAR_DIR"/popauth"
#endif

#define DATA_FILE	VAR_DIR"/popauth.dat"

/*
 * writes the authentificated IP address along with a timestamp to a file
 */
void
pop_pop_before_smtp_popauth_file(pinfo)
	POP_INFO *pinfo;
{
    FILE* datafile;
    FILE* datatempfile;
    FILE* popauthfile;
    FILE* popauthtempfile;
    char datatempfilename[256];
    char popauthtempfilename[256];
    char record[256];
    char newrecord[256];
    int fd;


    strncpy(datatempfilename,VAR_DIR,248);
    strcat(datatempfilename,"/.XXXXXX");
    fd=mkstemp(datatempfilename);
    if (fd < 0) {
        syslog(LOG_ERR, "Error creating temporary data file (%s): %s", datatempfilename, strerror(errno));
        return;
    }
    datatempfile=fdopen(fd,"w");
    if (datatempfile == (FILE *)0) {
        syslog(LOG_ERR, "Error opening temporary data file (%s): %s", datatempfilename, strerror(errno));
        return;
    }

    strncpy(popauthtempfilename,VAR_DIR,248);
    strcat(popauthtempfilename,"/.XXXXXX");
    fd=mkstemp(popauthtempfilename);
    if (fd < 0) {
        syslog(LOG_ERR, "Error creating temporary popauth file (%s): %s", popauthtempfilename, strerror(errno));
        return;
    }
    popauthtempfile=fdopen(fd,"w");
    if (popauthtempfile == (FILE *)0) {
        syslog(LOG_ERR, "Error opening temporary popauth file (%s): %s", popauthtempfilename, strerror(errno));
        return;
    }

    datafile=fopen(DATA_FILE,"r");


    snprintf(newrecord,256,"%s:%d\n",pinfo->remoteip,time((time_t*)0));

    /* copy old entries, skip record if IP matches the current one */
    if (datafile != (FILE *)0) {
        while (fgets(record,256,datafile)!=NULL) {
	    if (strncmp(record,pinfo->remoteip,strlen(pinfo->remoteip))!=0) {
		fputs(record,datatempfile);
		fputs(strtok(record,":"),popauthtempfile); fputs("\n",popauthtempfile);
	    }
	}
	fclose(datafile);
    }

    /* append new record */
    fputs(newrecord,datatempfile);
    fclose(datatempfile);
    fputs(pinfo->remoteip,popauthtempfile); fputs("\n",popauthtempfile);
    fclose(popauthtempfile);

    rename(popauthtempfilename,POPAUTH_FILE);
    rename(datatempfilename,DATA_FILE);
}

