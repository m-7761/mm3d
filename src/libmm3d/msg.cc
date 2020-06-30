/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2004-2007 Kevin Worcester
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place-Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * See the COPYING file for full license text.
 */

#include "mm3dtypes.h" //PCH

#include "msg.h"

static msg_func msg_user_info;
static msg_func msg_user_warn;
static msg_func msg_user_err;

static msg_prompt_func msg_user_info_prompt;
static msg_prompt_func msg_user_warn_prompt;
static msg_prompt_func msg_user_err_prompt;

extern "C" void msg_register
(msg_func infomsg, msg_func warnmsg, msg_func errmsg)
{
	msg_user_info = infomsg; msg_user_warn = warnmsg; msg_user_err = errmsg;
}

extern "C" void msg_register_prompt
(msg_prompt_func infomsg, msg_prompt_func warnmsg, msg_prompt_func errmsg)
{
	msg_user_info_prompt = infomsg;
	msg_user_warn_prompt = warnmsg; msg_user_err_prompt = errmsg;
}

extern "C" void msg_info(const char *str)
{
	msg_user_info?msg_user_info(str):(void)printf("info: %s\n",str);
}
extern "C" void msg_warning(const char *str)
{
	msg_user_warn?msg_user_warn(str):(void)printf("warning: %s\n",str);
}
extern "C" void msg_error(const char *str)
{
	msg_user_err?msg_user_err(str):(void)printf("error: %s\n",str);
}

static char return_caps(const char *opts)
{
	if(!opts) return '\0';

	size_t len = strlen(opts);

	if(!len) return '\0';

	for(size_t n=0;n<len;n++)
	{
		if(isupper(opts[n])) return toupper(opts[n]);
	}
	return toupper(opts[0]);
}

//FIX ME (swap arguments order)
extern "C" char msg_info_prompt(const char *str, const char *opts)
{
	if(!msg_user_info_prompt)
	{
		printf("%s\n",str); return return_caps(opts);		
	}
	else return msg_user_info_prompt(str,opts);
}
extern "C" char msg_warning_prompt(const char *str, const char *opts)
{
	if(!msg_user_warn_prompt)
	{
		printf("%s\n",str); return return_caps(opts);		
	}
	else return msg_user_warn_prompt(str,opts);
}
extern "C" char msg_error_prompt(const char *str, const char *opts)
{
	if(!msg_user_err_prompt)
	{
		printf("%s\n",str); return return_caps(opts);
	}
	else return msg_user_err_prompt(str,opts);
}

