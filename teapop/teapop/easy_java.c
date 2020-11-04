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

#include <stdlib.h>
#include "easy_java.h"

/**
Variables used to access/control JVM
**/
JavaVM * ej_jvm = NULL;
JNIEnv * ej_env = NULL;
static JavaVMInitArgs vm_args;
static JavaVMOption options[10];
static char cpbuffer[2048];


int ej_init_jvm(void) {
	jint res;
	char *classpath;
	
	if (ej_jvm != NULL) {
		return (0);
	}

	JNI_GetDefaultJavaVMInitArgs(&vm_args);
	vm_args.version = JNI_VERSION_1_2;
	vm_args.nOptions = 0;
	classpath = getenv("CLASSPATH");
	if (classpath != NULL) {
		strcpy(&cpbuffer[0], "-Djava.class.path=");
		strncat(&cpbuffer[0], classpath, 
			(sizeof(cpbuffer) - strlen(cpbuffer)) );
		options[vm_args.nOptions].optionString = &cpbuffer[0];
		vm_args.nOptions = vm_args.nOptions + 1;
	}
	vm_args.options = &options[0];

	res = JNI_CreateJavaVM(&ej_jvm, (void **)&ej_env, &vm_args);
	if (res < 0) {
		return (res);
	}
        return (0);
}

void ej_finish_jvm() {
	if (ej_jvm != NULL) {
		(*ej_jvm)->DestroyJavaVM(ej_jvm);
   	}
}

void ej_adjust_classname(char *classname) {
	char *tmp;
	
	/** replace dots with slashes in class name **/
	tmp = classname;
	while (tmp != '\0') {
		if (*tmp == '.') {
			*tmp = '/';
		}
		tmp++;
	}
}

jthrowable ej_CheckException() {
	jthrowable jexp = ej_ExceptionOccurred();
	if (jexp != NULL) {
		ej_ExceptionClear();
	}
	return (jexp);
}

