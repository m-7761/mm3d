/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2004-2007 Kevin Worcester
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not,write to the Free Software
 * Foundation,Inc.,59 Temple Place-Suite 330,Boston,MA 02111-1307,
 * USA.
 *
 * See the COPYING file for full license text.
 */

#include "mm3dtypes.h" //PCH

#include "log.h"

#ifdef CODE_DEBUG
static bool log_log_show_debug	= true;
static bool log_log_show_warning = true;
static bool log_log_show_error	= true;
#else
static bool log_log_show_debug	= false;
static bool log_log_show_warning = false;
static bool log_log_show_error	= false;
#endif // CODE_DEBUG

void log_enable_debug(bool o)
{
	log_log_show_debug = o;
}

void log_enable_warning(bool o)
{
	log_log_show_warning = o;
}

void log_enable_error(bool o)
{
	log_log_show_error = o;
}

void log_debug(const char *fmt,...)
{
	if(log_log_show_debug)
	{
		va_list ap;

		fprintf(stderr,"debug:	");

		va_start(ap,fmt);
		vfprintf(stderr,fmt,ap);
	}
}

void log_warning(const char *fmt,...)
{
	if(log_log_show_warning)
	{
		va_list ap;

		fprintf(stderr,"warning: ");

		va_start(ap,fmt);
		vfprintf(stderr,fmt,ap);
	}
}

void log_error(const char *fmt,...)
{
	if(log_log_show_error)
	{
		va_list ap;

		fprintf(stderr,"error:	");

		va_start(ap,fmt);
		vfprintf(stderr,fmt,ap);
	}
}

#ifdef __DO_PROFILE

static FILE *log_logProfileFP = nullptr;

void log_profile_init(const char *filename)
{
	if(log_logProfileFP==nullptr)
	{
		log_logProfileFP = fopen(filename,"w");
	}
}

void log_profile_shutdown()
{
	if(log_logProfileFP)
	{
		fclose(log_logProfileFP);
		log_logProfileFP = nullptr;
	}
}

#endif // __DO_PROFILE
