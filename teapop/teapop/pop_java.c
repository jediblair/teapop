/*
 * Copyright (c) 1999-2003 Ivan Francolin Martinez
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
#include "easy_java.h"

#include "config.h"

#include "teapop.h"

/**
JAVA_AUTHCLASS comes from configure script
*/
char *authClassName = JAVA_AUTHCLASS;
jclass authClass;

int init() {
	jint res;
	char *tmp;
	
	if (authClass != NULL) {
		return (0);
	}
	res = ej_init_jvm();
	if (res == 0) {
		ej_adjust_classname(authClassName);
		authClass = ej_FindClass(authClassName);
		if (authClass == NULL) {
			syslog(LOG_ERR,"Cant find java class : %s \n",authClassName);
			return (1);
		}
	} else {
		syslog(LOG_ERR,"Can't create Java VM : %d\n",res);
		return (1);
	}
	return (0);

}

/**
Authentication of the user.
There is no disposal of Java objects, Java garbage collector will
dispose it because they are not referenced in Java context
**/
#define AUTHSIGNATURE "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)Ljava/lang/Object;"
int pop_password_java(pinfo,auth,isapop,passwd)
	POP_INFO *pinfo;
	POP_AUTH *auth;
	int isapop;
	char * passwd;
{

	jobject obj;
	jobject user = NULL;
	jmethodID authMethod;
	jboolean ret = JNI_FALSE;
	jstring userid;
	jstring domain;
	jstring password;
	jstring apopstr;
	jstring mailbox = NULL;
	jboolean jisapop;

        if (init() != 0) {
        	return (1);
        }
	obj = ej_NewObject(authClass);
	if (obj != NULL) {
		/*
			Java Method to authenticate
			public boolean doPOPAuth(String userid,
						String domain,
						String password,
						String apopstr,
						boolean isapop)
		*/	
		authMethod = ej_GetMethodId(authClass, "doPOPAuth", AUTHSIGNATURE);
		if (authMethod != NULL) {
		        userid = ej_NewString(pinfo->userid);
			domain = ej_NewString(pinfo->domain);
			password = ej_NewString(passwd);
			apopstr = ej_NewString(pinfo->apopstr);
			/* Checking for APOP auth */
			jisapop = JNI_FALSE;
			if (isapop == 1) {
			    jisapop = JNI_TRUE;
			}
			user = ej_CallObjectMethod(obj,
						   authMethod,
						   userid,
						   domain,
						   password,
						   apopstr,
						   jisapop);
			if (user != NULL) {
				authMethod = ej_GetMethodIdObj(user,
							       "getMailbox",
							       "()Ljava/lang/String;");
				if (authMethod != NULL) {
	        	                mailbox = ej_CallObjectMethod(user,
	        	                			      authMethod,
	        	                			      userid);
	        	                strncpy(pinfo->chroot,
	        	                	auth->maildrop,
	        	                	sizeof(pinfo->chroot));     		
        		                strncpy(pinfo->maildrop,
						ej_GetStringUTFChars(mailbox, &ret),
        		                	sizeof(pinfo->maildrop));	
					ret = JNI_TRUE;
				} else {
					syslog(LOG_ERR,"method : public String getMailbox() "
					               "not found in user object");
				}
			}
		} else {
			syslog(LOG_ERR,"method : public Object "
			       "doPOPAuth(userid,domain,password,isapop) not found");
		}

	}

        if (ret == JNI_TRUE) {
		return (0);
	} else {
		return (1);
	}
}

