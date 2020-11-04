/* $Id: //depot/Teapop/0.3/teapop/pop_socket.c#9 $ */

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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "config.h"
#include "teapop.h"
#include "pop_signal.h"
#include "pop_socket.h"
#include "pop_strings.h"

/** BASED ON sshd.c FROM openssh.com */
#ifdef HAVE_TCPD_H
#include <tcpd.h>
#endif
#ifdef WITH_TCPD
int allow_severity = LOG_INFO;
int deny_severity = LOG_WARNING;
static char *progname = "teapop";
#endif

void
#if __STDC__
pop_socket_send(FILE * fd, const char *fmt, ...)
#else
pop_socket_send(fd, fmt, va_alist)
FILE *fd;
char *fmt;
va_dcl
#endif
{
	char buf[512];

	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vsnprintf(buf, 508, fmt, ap);
	if (!(buf[strlen(buf - 2)] == '\r' && buf[strlen(buf - 1)] == '\n'))
		strcat(buf, "\r\n");
	va_end(ap);

	(void) fputs(buf, fd);
	(void) fflush(fd);
}

int
pop_socket_rawsend(fd, buffer)
FILE *fd;
char *buffer;
{
	register int retval;

	retval = fputs(buffer, fd);
	return (retval == -1 ? 1 : 0);
}

/* UGH! */
int
#if __STDC__
pop_wait_for_commands(int timeout, int size, char *arguments, ...)
#else
pop_wait_for_commands(timeout, size, arguments, va_alist)
char *arguments;
int timeout, size;
va_dcl
#endif
{
	char buf[300], slask[512];
	static char *ptr;
	int arg;
	static va_list ap;
#if __STDC__
	va_start(ap, arguments);
#else
	va_start(ap, arguments);
#endif
	if ((ptr = va_arg(ap, char *)) == NULL)
	{
		va_end(ap);
		return (-1);
	}

	for (;;) {
		alarm(timeout);
		if (setjmp(env)) {
			va_end(ap);
			return (0);
		}
		if ((fgets(buf, 255, stdin)) == NULL) {
			if (feof(stdin))
				pop_signal_sigpipe(SIGPIPE);
			else
				return (-1);
		}

		if (buf[strlen(buf) - 1] != '\n') {
			slask[0] = '\0';
			while (slask[strlen(slask) - 1] != '\n')
				fgets(slask, 500, stdin);
		}
		alarm(0);

		while (buf[strlen(buf) - 1] == '\n' ||
		    buf[strlen(buf) - 1] == '\r') buf[strlen(buf) - 1] = '\0';
		arg = 1;
		while (ptr != NULL) {
			if (!strncasecmp(ptr, buf, strlen(ptr)) &&
			    (strlen(buf) == strlen(ptr) ||
				buf[strlen(ptr)] == ' ')) {
				strncpy(arguments,
				    buf + strlen(ptr) + 1, size - 1);
				arguments[size - 1] = '\0';
				return (arg);
			}
			ptr = va_arg(ap, char *);
			arg++;
		}
		ptr = strchr(buf, ' ');
		if (ptr != NULL)
			*ptr = '\0';
		fprintf(stderr, "%s " POP_UNKNOWN "\r\n", POP_ERR, buf);
		va_end(ap);
		va_start(ap, arguments);
		ptr = va_arg(ap, char *);
	}

	va_end(ap);
	return (-1);
}

int
pop_socket_init(pinfo)
POP_INFO *pinfo;
{
	socklen_t len;

#ifdef INET6
	struct sockaddr_storage ss;
#else
	struct sockaddr_in name;
	struct hostent *hp;
#endif

#ifdef INET6
	len = sizeof(ss);
#else
	len = sizeof(name);
#endif


	/* Get our own IP */
#ifdef INET6
	if (getsockname(pinfo->insck, (struct sockaddr *)&ss, &len) == -1) {
		syslog(LOG_ERR, "can't do getsockname() (errno = %d)", errno);
		return (1);
	}
#else
	if (getsockname(pinfo->insck, (struct sockaddr *)&name, &len) == -1) {
		syslog(LOG_ERR, "can't do getsockname() (errno = %d)", errno);
		return (1);
	}
#endif

#ifdef INET6
	/* XXX - Is there an AF-independent way of doing this?? */
	switch (ss.ss_family) {
	case AF_INET:
		inet_ntop(ss.ss_family,
		    &((struct sockaddr_in *)&ss)->sin_addr,
		    pinfo->localip, sizeof(pinfo->localip));
		break;
	case AF_INET6:
		inet_ntop(ss.ss_family,
		    &((struct sockaddr_in6 *)&ss)->sin6_addr,
		    pinfo->localip, sizeof(pinfo->localip));
		break;
	default:
		syslog(LOG_ERR, "Unknown AF type on socket");
		return (1);
	}
#else
	strncpy(pinfo->localip, (char *)inet_ntoa(name.sin_addr),
	    sizeof(pinfo->localip));
#endif
	pinfo->localip[sizeof(pinfo->localip)] = '\0';

	/* Get IP of the remote client */
#ifdef INET6
	if (getpeername(pinfo->insck, (struct sockaddr *)&ss, &len) == -1) {
		syslog(LOG_ERR, "can't get socket and/or do getpeername "
		    "(error = %d)", errno);
		return (1);
	}
	if (getnameinfo((struct sockaddr *)&ss, len,
	    pinfo->remoteip, sizeof(pinfo->remoteip),
	    NULL, 0, NI_NUMERICHOST) != 0) {
		syslog(LOG_ERR, "can't get IP of remote client "
		    "(error = %d)", errno);
		return (1);
	}
#else
	if (getpeername(pinfo->insck, (struct sockaddr *)&name, &len) == -1) {
		syslog(LOG_ERR, "can't get socket and/or do getpeername "
		    "(error = %d)", errno);
		return (1);
	}
	strncpy(pinfo->remoteip, (char *) inet_ntoa(name.sin_addr),
	    sizeof(pinfo->remoteip));
	pinfo->remoteip[sizeof(pinfo->remoteip)] = '\0';
#endif

	/* Now resolv the IP and see what we get */
	if (pinfo->nodns < 2) {
#ifdef INET6
		if (getnameinfo((struct sockaddr *)&ss, len,
		    pinfo->remotehost, sizeof(pinfo->remotehost),
		    NULL, 0, NI_NAMEREQD) != 0) {
			syslog(LOG_NOTICE,
			    "can't do reverse dns on client (error = %d)",
			    errno);
			pinfo->remotehost[0] = '\0';
		}
#else
		hp = gethostbyaddr((char *)&name.sin_addr,
		    sizeof(name.sin_addr), AF_INET);
		if (hp == NULL) {
			syslog(LOG_NOTICE,
			    "can't do reverse dns on client (error = %d)",
			    errno);
		} else {
			strncpy(pinfo->remotehost, hp->h_name,
			    sizeof(pinfo->remotehost));
			pinfo->remotehost[sizeof(pinfo->remotehost)] = '\0';
		}
#endif
	}
	if (pinfo->remotehost[0] == '\0') {
		strncpy(pinfo->remotehost, "unknown",
		    sizeof(pinfo->remotehost));
		pinfo->remotehost[sizeof(pinfo->remotehost)] = '\0';
	}

#ifdef WITH_TCPD
	{
		struct request_info req;

		request_init(&req, RQ_DAEMON, progname, RQ_FILE, pinfo->insck, NULL);
		fromhost(&req);

		if (!hosts_access(&req)) {
			syslog(LOG_ERR,
			    "tcp_wrappers connection refused %s %s ",
			    pinfo->remoteip,pinfo->remotehost);
			return (2);
		}
	}
#endif
	pinfo->out = fdopen(pinfo->outsck, "wb");
	return (0);
}

int
pop_socket_bind(port)
unsigned short port;
{
	int sckfd, sck_opt = 1;

	struct sockaddr_in name;

	if (port == 0) port = POP3PORT;

	if ((sckfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		syslog(LOG_ERR, "Problem calling socket(): %s",
		    strerror(errno));
		perror("socket");
		return (0);
	}
	/* westr - allow startup with reusing the sockets */
	if ((setsockopt(sckfd, SOL_SOCKET, SO_REUSEADDR, (char *)&sck_opt,
	    sizeof(sck_opt))) < 0) {
		perror("setsockopt");
		return (0);
	}
	memset((char *)&name, 0, sizeof(struct sockaddr_in));
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	if ((bind(sckfd, (struct sockaddr *)&name, sizeof(name))) < 0) {
		syslog(LOG_ERR, "Problem calling bind(): %s",
		    strerror(errno));
		perror("bind");
		return (0);
	}
	if ((listen(sckfd, 10)) < 0) {
		syslog(LOG_ERR, "Problem calling listen(): %s",
		    strerror(errno));
		perror("listen");
		return (0);
	}
	/*
	 * XXX - for now we do not set the socket nonblock since it
	 * causes more, or rather worse, problems then it fixes. - ibo
	 */
/*
	if (fcntl(sckfd, F_SETFL, O_NONBLOCK) != 0) {
		syslog(LOG_ERR, "Problem calling fcntl(): %s",
		    strerror(errno));
		perror("fcntl");
		return (0);
	}
*/
	return (sckfd);
}

int
pop_socket_wait(sckfd)
int sckfd;
{
       fd_set rfds;
       /* struct timeval tv; */

       FD_ZERO(&rfds);
       FD_SET(sckfd, &rfds);
       /*
         tv.tv_sec = 5;
         tv.tv_usec = 0;
       */

       /*
         Wait for sckfd to be ready and return.
         Will return number of FDs ready (1) on success.
         Will return 0 on timeout, -1 on error.
       */
       return (select(sckfd+1,&rfds,0,0,NULL));
}
