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


#ifndef __TEXTURE_H
#define __TEXTURE_H

//#include <time.h> //???

#include "mm3dtypes.h"

class Texture
{
	public:
		enum ErrorE
		{
			ERROR_NONE = 0,
			ERROR_NO_FILE,
			ERROR_NO_ACCESS,
			ERROR_FILE_OPEN,
			ERROR_FILE_READ,
			ERROR_FILE_WRITE,
			ERROR_BAD_MAGIC,
			ERROR_UNSUPPORTED_VERSION,
			ERROR_BAD_DATA,
			ERROR_UNEXPECTED_EOF,
			ERROR_UNSUPPORTED_OPERATION,
			ERROR_BAD_ARGUMENT,
			ERROR_UNKNOWN
		};

		enum FormatE
		{
			FORMAT_RGB, //TODO: GL_RGB?
			FORMAT_RGBA	//TODO: GL_RGBA?
		};

		struct CompareResultT
		{
			bool comparable;
			unsigned pixelCount;
			unsigned matchCount;
			unsigned fuzzyCount;
		};

		Texture(); ~Texture();

		static bool compare(Texture *t1,Texture *t2,CompareResultT *res, unsigned fuzzyValue);

		bool compare(Texture *tex,CompareResultT *res, unsigned fuzzyValue);

		static const char *errorToString(Texture::ErrorE);

		void removeOpaqueAlphaChannel(); //??? //JUSTIFY ME

		std::string m_name;
		std::string m_filename;
		std::string m_origFormat; //NEW

		bool		m_isBad;
		int		 m_height;
		int		 m_width;
		FormatE	m_format;
		std::vector<uint8_t> m_data;

		int		 m_origWidth;
		int		 m_origHeight;

		time_t	 m_loadTime;

		static int s_allocated;
};

//errorobj.cc
extern const char *textureErrStr(Texture::ErrorE);

#endif // __TEXTURE_H
