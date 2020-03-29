/*  Md3Filter plugin for Maverick Model 3D
 *
 * Copyright (c)2005-2007 Russell Valentine and Kevin Worcester
 * Copyright (c)2009-2015 Zack Middleton
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

/*
 *Original version by Russell Valentine (russ@coldstonelabs.org)
 *Plugin based off lwofilter.cc and md2filter.cc by Kevin Worcester.
 *Spec: http://linux.ucla.edu/~phaethon/q3/formats/md3format.html
 */
#ifndef __MD3FILTER_H
#define __MD3FILTER_H

#include "modelfilter.h"
#include "mesh.h"

#define MD3_VERSION 15
#define MAX_QPATH 64
#define MD3_MAX_FRAMES 1024
#define MD3_MAX_TAGS 16
#define MD3_MAX_SURFACES 32
#define MD3_MAX_SHADERS 256
#define MD3_MAX_VERTS 4096
#define MD3_MAX_TRIANGLES 8192
#define MD3_XYZ_SCALE (1.0/64)

class Md3Filter : public ModelFilter
{
public:

	Model::ModelErrorE readFile(Model *model, const char *const filename);
	Model::ModelErrorE writeFile(Model *model, const char *const filename, Options&);

	const char *getReadTypes(){ return "MD3"; }
	const char *getWriteTypes(){ return "MD3"; }

protected:

	enum MeshSectionE
	{
		MS_None = -1,
		MS_Lower = 0,
		MS_Upper,
		MS_Head,
		MS_MAX
	};

	// the order is important,used for writing continue frames by type
	enum MeshAnimationTypeE
	{
		MA_All,
		MA_Both,
		MA_Torso,
		MA_Legs,
		// NOTE: Team Arena has extra torso animations after legs
		MA_Head,
		MA_MAX
	};

	struct MeshVectorInfoT
	{
		int32_t id;
		int8_t lat;
		int8_t lng;
		float s;
		float t;
	};

	struct VertexTexCheckT
	{
		unsigned id;
		float s;
		float t;
	};

	struct Md3FileDataT
	{
		MeshSectionE section;
		std::string modelBaseName;
		std::string modelFile;
		std::string tag;
		int32_t tagPoint;
		DataSource *src;
		int32_t offsetMeshes;
		int32_t numMeshes;
		int32_t offsetTags;
		int32_t numTags;
		int32_t numFrames;
		MeshVectorInfoT ** meshVecInfos;
	};
	typedef std::list<Md3FileDataT> Md3FileDataList;

	struct Md3PathT
	{
		MeshSectionE section;
		int material;
		std::string path;
	};
	typedef std::vector<Md3PathT>Md3PathList;

	unsigned readString(char *dest, size_t len);

	bool	  readAnimations(bool create);

	void	  setMeshes(MeshSectionE section, int32_t offsetMeshes, int32_t numMeshes, int32_t parentTag, int32_t animIndex);
	int32_t  setSkins(char *meshName);
	void	  setPoints(MeshSectionE section, int32_t offsetTags, int32_t numTags, int32_t numFrames, int32_t parentTag, int32_t animIndex);
	std::string findTexture(std::string baseName,std::string shaderFullName);
	int32_t  materialsCheck(std::string textureFullName);

	std::string getSafeName(unsigned int anim);

	// Returns MD3 frame number for a specific animation frame
	int		animToFrame(MeshSectionE section, int anim, int frame);

	// Indicates if the animation specified is contained in the 
	// specified MD3 model section 
	bool	  animInSection(std::string animName,MeshSectionE section);
	bool	  groupInSection(std::string groupName,MeshSectionE section);
	bool	  tagInSection(std::string tagName,MeshSectionE section);
	bool	  tagIsSectionRoot(std::string tagName,MeshSectionE section);

	// Path handling functions
	std::string extractPath(const char *md3DataPath);
	std::string materialToPath(int materialIndex);
	std::string sectionToPath(int materialIndex);
	std::string defaultPath();

	Model		*m_model;
	DataSource *m_src;
	//		int32_t	** meshVecIds;
	MeshVectorInfoT	** m_meshVecInfos;
	std::string	 m_modelPath;
	std::string	 m_modelBaseName;
	std::vector<int> m_animStartFrame;
	int				  m_standFrame; // torso
	int				  m_idleFrame; // legs
	int				  m_headFrame; // head
	std::string		m_lastMd3Path;
	Md3PathList		m_pathList;

	//writes
	Model::ModelErrorE writeSectionFile(const char *filename,MeshSectionE section,MeshList &meshes);
	bool	  writeAnimations();
	DataDest *m_dst;

	//writes util
	bool	  animSyncWarning(std::string name);
	//bool	  getVertexNormal(Model *model, int groupId, int vertexId, float *normal);
	void resetVertexNormals(Model *model, Mesh &mesh);
	double	greater(double a, double b);
	double	smaller(double a, double b);
	Matrix	getMatrixFromPoint(int anim, int frame, int point);
	MeshAnimationTypeE getAnimationType(const char *animName);
	bool getExportAnimData(int modelAnim,
			int &fileFrame, int &frameCount, int &fps);
};
#endif // __MD3FILTER_H
