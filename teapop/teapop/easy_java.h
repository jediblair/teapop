/*
 * Copyright (c) 1999-2001 Ivan Francolin Martinez
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

#ifndef __EASY_JAVA_H__
#define __EASY_JAVA_H__

#include "jni.h"

/** Java Virtual Machine **/
extern JavaVM *ej_jvm;
/** Java Environment **/
extern JNIEnv *ej_env;

/** Initialize JVM for use **/
int	ej_init_jvm(void);
/** finish JVM **/
void	ej_finish_jvm(void);
/** Adjust classname replace '.' with '/' **/
void    ej_adjust_classname(char *classname);

/** Return a Java Class (Remember to use '/' instead of '.')  **/
#define ej_FindClass(classname) \
		(*ej_env)->FindClass(ej_env, classname)

/** Return the class of object **/
#define ej_GetObjectClass(obj) \
		(*ej_env)->GetObjectClass(ej_env, obj)

/** Return the method with specified parameters **/
#define ej_GetMethodId(class, method, id) \
		(*ej_env)->GetMethodID(ej_env, class, method, id)
#define ej_GetMethodIdObj(obj, method, id) \
		ej_GetMethodId(ej_GetObjectClass(obj), method, id)

/** Return the constructor with specified parameters **/
#define ej_GetConstructor(class, id) \
		ej_GetMethodId(class, "<init>", id)

/** Return the default constructor of class **/
#define ej_GetDefaultConstructor(class) \
		ej_GetConstructor(class, "()V")

/** Create a new object using specified constructor **/
#define ej_NewObjectC(class, constructor) \
		(*ej_env)->NewObject(ej_env, class, constructor)

/** Create a new object using standard constructor **/
#define ej_NewObject(class) \
		ej_NewObjectC(class, ej_GetDefaultConstructor(class))

/** Chama metodo que retorna boolean **/
#define ej_CallBooleanMethod(object, method) \
		(*ej_env)->CallBooleanMethod(ej_env, object, method)

/** Call an object Method **/
#define ej_CallObjectMethod(object, method, ...) \
		(*ej_env)->CallObjectMethod(ej_env, object, method, __VA_ARGS__)
		
/** Create an java String **/
#define ej_NewString(str) \
		(*ej_env)->NewStringUTF(ej_env, str)

/** Return a pointer to chars of Java String **/
#define ej_GetStringUTFChars(str, isCopy) \
		(*ej_env)->GetStringUTFChars(ej_env, str, isCopy)
		
/** Check if JavaException has Occurred **/
#define ej_ExceptionOccurred() \
		(*ej_env)->ExceptionOccurred(ej_env)

/** Clear JavaException  **/
#define ej_ExceptionClear() \
		(*ej_env)->ExceptionClear(ej_env)

#endif
