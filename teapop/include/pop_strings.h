/* $Id: //depot/Teapop/0.3/include/pop_strings.h#7 $ */

/*
 * Copyright (c) 1999-2001 ToonTown Consulting
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

#ifndef __POP_STRINGS_H__
#define __POP_STRINGS_H__

/*
 * The following two strings are /REQUIRED/ by various rfc's too
 * look exactly like this. Don't change them if you want pop3-clients
 * to work with your popserver.
 */
#define	POP_OK		"+OK"	/* DO /NOT/ CHANGE */
#define	POP_ERR		"-ERR"	/* DO /NOT/ CHANGE */

/* Feel free to change following strings if it makes you happy */
#define POP_APOP_BAD	"Syntax: APOP <name> <digest>"
#define POP_APOP_HOST	"Llywellyn"
#define POP_BANNER	"Teaspoon stirs around again"
#define POP_CAPA_BAD	"Syntax: CAPA"
#define POP_CAPA_OK	"These are my limits, Sir"
#define POP_DELE_BAD	"Syntax: DELE <message number>"
#define POP_DELE_GONE	"This one was already sent away."
#define POP_DELE_NOMSG	"Message? What message?"
#define POP_DELE_OK	"Bye bye, letter"
#define POP_LAST_BAD	"Syntax: LAST"
#define POP_LIST_BAD	"Syntax: LIST [<message number>]"
#define POP_LIST_GONE	"The message took a hike."
#define POP_LIST_NOMSG	"To be or not to be ... this message is a not."
#define POP_LIST_OK	"These are my measures."
#define POP_LOCK_ERR	"Mailbox already locked."
#define POP_MSG_BIG	"Possible security violation reported."
#define POP_MSG_GONE	"It got scared away and is gone"
#define POP_MSG_NONE	"Nope, never heard of it."
#define POP_NOOP_BAD	"Syntax: NOOP"
#define POP_NOOP_OK	"Please don't leave me alone for long."
#define POP_PASS_ARGS	"You don't want to share your secret with me?"
#define POP_PASS_BAD	"Incorrect password...or maybe you don't even have an account here?"
#define POP_PROBLEM	"Something odd is happening. You might want to disconnect and try again."
#define POP_READY	"I'm ready to serve you, Master."
#define POP_RETR_BAD	"Syntax: RETR <message number>"
#define POP_RSET_BAD	"Syntax: RSET"
#define POP_RSET_OK	"Bye Bye flags."
#define	POP_STAT_BAD	"Syntax: STAT"
#define POP_TIMEOUT	"Master, did you fall asleep? Bye."
#define POP_TOP_BAD	"Syntax: TOP <message number> <number of lines>."
#define POP_UIDL_BAD	"Syntax: UIDL [<message number>]"
#define POP_UIDL_GONE	"Message is deleted"
#define POP_UIDL_NOMSG	"That message doesn't exist."
#define POP_UIDL_OK	"Here comes the id's."
#define POP_UNKNOWN	"%s? I'm not quite sure what you mean, Master."
#define POP_USER_ARGS	"I won't serve a Master who doesn't know their own name."
#define POP_USER_OK	"Welcome, do you have any type of ID?"
#define POP_WRONG	"Something went wrong"
#define POP_QUIT_AUTH	"I hope you will be back for your mail later, Sir."
#define POP_QUIT_MAXTRY	"Humpf. What do you take me for?"
#define POP_QUIT_OK	"It has been a pleasure serving you."
#define POP_QUIT_ERR	"Ehum..Bye"

char *pop_string_find __P((const char *, const char *));
int pop_string_dtot __P((const char *));
#endif				/* __POP_STRINGS_H__ */
