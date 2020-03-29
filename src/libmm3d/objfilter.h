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


#ifndef __OBJFILTER_H
#define __OBJFILTER_H

#include "modelfilter.h"
#include "datadest.h"
#include "datasource.h"

#include <stdint.h>
//#include <stdio.h> //???
#include <string>
#include <vector>

class ObjFilter : public ModelFilter
{
public:

	// For a class derived from ModelFilter::Options,you'll
	// want to add member variables for everything the user
	// can control.  Whether or not you add accessors
	// for the member variables is left to your discretion.
	//
	// Set the default values for the filter options in
	// the constructor.
	class ObjOptions : public Options
	{
	public:

		ObjOptions();

		bool m_saveNormals;
		int m_places;
		int m_texPlaces;
		int m_normalPlaces;

		virtual void setOptionsFromModel(Model*){}
	};

	// Note, filename interacts with FileFactory:
	// https://github.com/zturtleman/mm3d/issues/70
	// It would be best to pass DataDest/DataSource
	// by reference, except for multipart models is
	// an open problem.
	//
	Model::ModelErrorE readFile(Model *model, const char *const filename);
	Model::ModelErrorE writeFile(Model *model, const char *const filename, Options &o);

	const char *getReadTypes(){ return "OBJ"; }
	const char *getWriteTypes(){ return "OBJ"; }

	// Create a new options object that is specific to this filter
	Options *getDefaultOptions(){ return new ObjOptions; };

	class ObjMaterial
	{
		public:
			ObjMaterial();

			std::string name;
			float		 diffuse[4];
			float		 ambient[4];
			float		 specular[4];
			float		 shininess;
			float		 alpha;
			std::string textureMap;
	};

	struct UvDataT
	{
		float u,v;
	};
	typedef std::vector<UvDataT> UvDataList;

	struct MaterialGroupT
	{
		unsigned material,group;
	};
	typedef std::vector<MaterialGroupT> MaterialGroupList;

protected:
	bool readLine(char *line);
	bool readVertex(char *line);
	bool readTextureCoord(char *line);
	bool readFace(char *line);
	bool readGroup(char *line);
	bool readLibrary(char *line);
	bool readMaterial(char *line);

	bool readMaterialLibrary(const char *filename);

	void addObjMaterial(ObjMaterial *mat);
	char *skipSpace(char *str);

	bool writeLine(const char *line,...);
	bool writeStripped(const char *line,...);
	bool writeHeader();
	bool writeMaterials();
	bool writeGroups();

	Model		 *m_model;
	ObjOptions  *m_options;
	DataSource  *m_src;
	DataDest	 *m_dst;
	int			  m_curGroup;
	int			  m_curMaterial;
	bool			 m_needGroup;
	int			  m_vertices;
	int			  m_faces;
	int			  m_groups;
	UvDataList	 m_uvList;
	MaterialGroupList m_mgList;

	std::string  m_groupName;

	std::string  m_modelPath;
	std::string  m_modelBaseName;
	std::string  m_modelFullName;
	std::string  m_materialFile;
	std::string  m_materialFullFile;
	std::vector<std::string>m_materialNames;
};

#endif // __OBJFILTER_H
