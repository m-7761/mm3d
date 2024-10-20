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

#include "model.h"

#include "modelundo.h"
#include "modelstatus.h"
#include "log.h"

#ifdef MM3D_EDIT //???

// TODO centralize this
static void model_meta_safe_strcpy(char *dest, const char *src, size_t len)
{
	if(len>0)
	{
		strncpy(dest,src,len);
		dest[len-1] = '\0';
	}
	else dest[0] = '\0';
}

void Model::addMetaData(const char *key, const char *value)
{
	MetaData md;
	md.key = key;
	md.value = value;

	Undo<MU_AddMetaData>(this,md.key,md.value);

	m_metaData.push_back(md);
}

void Model::updateMetaData(const char *key, const char *value)
{
	auto it = std::find_if
	(m_metaData.begin(),m_metaData.end(),[key](auto &md)
	{
		return md.key==key;
	});
	if(it==m_metaData.end())
	{
		MetaData md;
		md.key = key;
		md.value = value;

		Undo<MU_AddMetaData>(this,md.key,md.value);

		m_metaData.push_back(md);
		return;
	}

	Undo<MU_UpdateMetaData>(this,key,value,it->value);

	it->value = value;
}

bool Model::getMetaData(const char *key, char *value, size_t valueLen)const
{
	auto it = std::find_if
	(m_metaData.begin(),m_metaData.end(),[key](auto &md)
	{
		return md.key==key;
	});
	if(it==m_metaData.end())
	return false;
	model_meta_safe_strcpy(value,it->value.c_str(),valueLen);
	return true;
}

//REMOVE ME (TRUNCATES)
bool Model::getMetaData(unsigned int index, char *key, size_t keyLen, char *value, size_t valueLen)const
{
	if(index<m_metaData.size())
	{
		model_meta_safe_strcpy(key,  m_metaData[index].key.c_str(),  keyLen);
		model_meta_safe_strcpy(value,m_metaData[index].value.c_str(),valueLen);
		return true;
	}
	return false;
}

unsigned int Model::getMetaDataCount()const
{
	return m_metaData.size();
}

void Model::clearMetaData()
{
	Undo<MU_ClearMetaData>(this,m_metaData);
		
	m_metaData.clear();
}

void Model::removeLastMetaData()
{
	// INTERNAL USE ONLY!!!

	// This is just for undo purposes, so we don't need an undo

	if(!m_metaData.empty()) m_metaData.pop_back();
}

#endif // MM3D_EDIT
