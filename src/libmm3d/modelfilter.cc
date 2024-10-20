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

#include "modelfilter.h"

#include "datasource.h"
#include "datadest.h"
#include "log.h"

int ModelFilter::Options::s_allocated = 0;

ModelFilter::ModelFilter()
	: m_promptFunc(),
	  m_defaultFactory(),
	  m_factory(&m_defaultFactory)
{
}

static struct NullOptions : ModelFilter::Options
{		
	NullOptions(){ s_allocated--; }

	virtual void release(){}
	virtual void setOptionsFromModel(Model*){}

}modelfilter_nopts;

ModelFilter::Options *ModelFilter::getDefaultOptions()
{
	return &modelfilter_nopts;
}

ModelFilter::Options::Options()
{
	s_allocated++;
}
ModelFilter::Options::~Options()
{
	s_allocated--;
}

void ModelFilter::Options::stats()
{
	log_debug("Filter Options: %d\n",s_allocated);
}

extern bool texmgr_can_read_or_write(const char*,const char*);
bool ModelFilter::canRead(const char *file)
{
	return texmgr_can_read_or_write(getReadTypes(),file);
}
bool ModelFilter::canWrite(const char *file)
{
	return texmgr_can_read_or_write(getWriteTypes(),file);
}
bool ModelFilter::canExport(const char *file)
{
	return texmgr_can_read_or_write(getExportTypes(),file);
}
bool ModelFilter::isSupported(const char *file)
{
	const char *p[3] = 
	{
		getReadTypes(),getWriteTypes(),getExportTypes()
	};
	for(int i=2;i>0;i--) if(p[i]==p[i-1]) p[i] = nullptr;
	for(int i=0;i<3;i++)
	if(p[i]&&texmgr_can_read_or_write(p[i],file)) return true; return false;
}

DataSource *ModelFilter::openInput(const char *filename, Model::ModelErrorE &err)
{
	DataSource *src = m_factory->getSource(filename);
	if(src->errorOccurred())
	{
		if(src->unexpectedEof())
		err = Model::ERROR_UNEXPECTED_EOF;
		else
		err = errnoToModelError(src->getErrno(),Model::ERROR_FILE_OPEN);
	}
	return src;
}

DataDest *ModelFilter::openOutput(const char *filename, Model::ModelErrorE &err)
{
	DataDest *dst = m_factory->getDest(filename);
	if(dst->errorOccurred())
	{
		if(dst->atFileLimit())
		err = Model::ERROR_UNEXPECTED_EOF;
		else
		err = errnoToModelError(dst->getErrno(),Model::ERROR_FILE_OPEN);
	}
	return dst;
}

/* static */
Model::ModelErrorE ModelFilter::errnoToModelError(int err, Model::ModelErrorE defaultError)
{
	switch (err)
	{
	case 0:
		return Model::ERROR_NONE;
	case EINVAL:
		return Model::ERROR_BAD_ARGUMENT;
	case EACCES:
	case EPERM:
		return Model::ERROR_NO_ACCESS;
	case ENOENT:
	case EBADF:
		return Model::ERROR_NO_FILE;
	case EISDIR:
		return Model::ERROR_BAD_DATA;
	default:
		break;
	}
	return defaultError;
}

