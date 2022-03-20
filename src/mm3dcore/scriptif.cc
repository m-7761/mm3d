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

#include "scriptif.h"
#include "model.h"
#include "filtermgr.h"
#include "glmath.h"
#include "weld.h"
#include "texture.h"
#include "texmgr.h"
#include "log.h"

typedef int_list IntList;

static std::string scriptif_getWriteFileName(const char *file)
{
	std::string s = file;
	const char *ext = strrchr(file,'.');

	if(ext==nullptr)
	{
		ext = ".ms3d";
	}

	struct stat buf;
	if(stat(s.c_str(),&buf)==0)
	{
		// File exists,pick another name
		char append[10];

		// At 99 we give up,and use it whether it's taken or not
		for(int num = 1; num<99; num++)
		{
			sprintf(append,".%02d",num);
			s = std::string(file);
			s+=append;
			s+=ext;

			if(stat(s.c_str(),&buf)!=0)
			{
				// This one isn't in use,use it
				break;
			}
		}
	}

	// We should have a valid file name to save as
	return s;
}

const char *scriptif_modelGetName(Model *model)
{
	return model->getFilename();
}

bool scriptif_modelSaveAs(Model *model, const char *f)
{
	if(model)
	{
		std::string filename = scriptif_getWriteFileName(f);
		//log_debug("saving file as %s\n",filename.c_str());
		Model::ModelErrorE err = FilterManager::getInstance()->writeFile(model,filename.c_str(),true,FilterManager::WO_ModelNoPrompt);
		if(err==Model::ERROR_NONE)
		{
			//log_debug("  success\n");
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Primitive Creation
//-----------------------------------------------------------------------------

MeshRectangle *scriptif_modelCreateMeshRectangle(Model *model, double x0, double y0, double z0,
		double x1, double y1, double z1,
		double x2, double y2, double z2,
		double x3, double y3, double z3,
		unsigned segments)
{
	MeshRectangle *mesh = nullptr;

	if(segments>=1)
	{
		unsigned rowCount = segments+1;

		std::vector<IntList> grid;

		unsigned y;
		unsigned x;

		mesh = new MeshRectangle();

		for(y = 0; y<rowCount; y++)
		{
			IntList row;

			// Left border coordinates
			double blx  = x0+(x3-x0)*((double)y/segments); 
			double bly  = y0+(y3-y0)*((double)y/segments); 
			double blz  = z0+(z3-z0)*((double)y/segments); 

			// Right border coordinates
			double brx  = x1+(x2-x1)*((double)y/segments); 
			double bry  = y1+(y2-y1)*((double)y/segments); 
			double brz  = z1+(z2-z1)*((double)y/segments); 

			// Create row
			for(x = 0; x<rowCount; x++)
			{
				double xpos = blx+(brx-blx)*((double)x/segments);
				double ypos = bly+(bry-bly)*((double)x/segments);
				double zpos = blz+(brz-blz)*((double)x/segments);

				int v = model->addVertex(xpos,ypos,zpos);
				row.push_back(v);
				mesh->m_vertices.push_back(row.back());
			}

			grid.push_back(row);
		}

		for(y = 1; y<rowCount; y++)
		{
			for(x = 1; x<rowCount; x++)
			{
				int tri = model->addTriangle(grid[y][x], 
							grid[y-1][x],grid[y-1][x-1]);
				mesh->m_faces.push_back(tri);
			}
			for(x = 1; x<rowCount; x++)
			{
				int tri = model->addTriangle(grid[y-1][x-1],
							grid[y][x-1],grid[y][x]);
				mesh->m_faces.push_back(tri);
			}
		}

		for(x = 0; x<mesh->m_faces.size(); x++)
		{
			model->selectTriangle(mesh->m_faces[x]);
		}
	}

	return mesh;
}

int scriptif_modelCreateBoneJoint(Model *model, const char *name,
		double x, double y, double z/*, double xrot, double yrot, double zrot*/, int parent)
{
	return model->addBoneJoint(name,x,y,z/*,xrot,yrot,zrot*/,parent);
}

//-----------------------------------------------------------------------------
// Primitive Manipulation
//-----------------------------------------------------------------------------

void scriptif_vertexSetCoords(Model *model, unsigned vertexIndex, double x, double y, double z)
{
	model->moveVertex(vertexIndex,x,y,z);
}

int scriptif_modelAddTexture(Model *model, const char *filename)
{
	int id = -1;
	Texture *tex = TextureManager::getInstance()->getTexture(filename);
	if(tex)
	{
		id = model->addTexture(tex);
	}
	return id;
}

void scriptif_groupSetTexture(Model *model, unsigned groupId, unsigned textureId)
{
	model->setGroupTextureId(groupId,textureId);
}

void scriptif_faceSetTextureCoords(Model *model, unsigned faceId, unsigned vertexIndex,
	  double s, double t)
{
	model->setTextureCoords(faceId,vertexIndex,(float)s,(float)t);
}

void scriptif_selectedRotate(Model *model, double x, double y, double z)
{
	double rot[3] = { 0,0,0 };
	double vec[3] = { 0,0,0 };

	rot[0] = x *PIOVER180;
	rot[1] = y *PIOVER180;
	rot[2] = z *PIOVER180;

	Matrix m;
	m.setRotation(rot);
	model->rotateSelected(m,vec);
}

void scriptif_selectedTranslate(Model *model, double x, double y, double z)
{
	double trans[3] = { x,y,z };

	//FIX ME: Translate via vector.
	//Matrix m; m.setTranslation(trans);
	//model->translateSelected(m);
	model->translateSelected(trans);
}

void scriptif_selectedScale(Model *model, double x, double y, double z)
{
	double scale[3] = { 0,0,0 };

	scale[0] = x;
	scale[1] = y;
	scale[2] = z;

	Matrix m;
	m.set(0,0,x);
	m.set(1,1,y);
	m.set(2,2,z);

	double point[3] = { 0,0,0 };
	model->rotateSelected(m,point);
}

extern void scriptif_selectedApplyMatrix(Model *model,
		double m0, double m1, double m2, double m3,
		double m4, double m5, double m6, double m7,
		double m8, double m9, double m10, double m11,
		double m12, double m13, double m14, double m15)
{
	Matrix m;
	m.set(0,0,m0);  m.set(0,1,m1);  m.set(0,2,m2);  m.set(0,3,m3);
	m.set(1,0,m4);  m.set(1,1,m5);  m.set(1,2,m6);  m.set(1,3,m7);
	m.set(2,0,m8);  m.set(2,1,m9);  m.set(2,2,m10); m.set(2,3,m11);
	m.set(3,0,m12); m.set(3,1,m13); m.set(3,2,m14); m.set(3,3,m15);

	double point[3] = { 0,0,0 };
	model->rotateSelected(m,point);
}

void scriptif_selectedDelete(Model *model)
{
	model->deleteSelected();
}

void scriptif_selectedWeldVertices(Model *model)
{
	weldSelectedVertices(model);
}

void scriptif_selectedInvertNormals(Model *model)
{
	int_list triangles;
	model->getSelectedTriangles(triangles);
	for(int_list::iterator it = triangles.begin(); it!=triangles.end(); it++)
	{
		model->invertNormals(*it);
	}
}

int scriptif_selectedGroupFaces(Model *model, const char *name)
{
	if(model&&name&&name[0])
	{
		int groupNumber = model->getGroupByName(name);

		if(groupNumber<0)
		{
			groupNumber = model->addGroup(name);
		}
		else
		{
			log_warning("group %s already exists: group # %d\n",name,groupNumber);
		}

		if(groupNumber>=0)
		{
			model->addSelectedToGroup(groupNumber);
		}

		return groupNumber;
	}

	return -1;
}

void scriptif_selectedAddToGroup(Model *model, int groupId)
{
	model->addSelectedToGroup(groupId);
}

//-----------------------------------------------------------------------------
// Informational
//-----------------------------------------------------------------------------

int scriptif_modelGetVertexCount(Model *model)
{
	return model->getVertexCount();
}

int scriptif_modelGetFaceCount(Model *model)
{
	return model->getTriangleCount();
}

int scriptif_modelGetGroupCount(Model *model)
{
	return model->getGroupCount();
}

int scriptif_modelGetJointCount(Model *model)
{
	return model->getBoneJointCount();
}

int scriptif_modelGetTextureCount(Model *model)
{
	return model->getTextureCount();
}

int scriptif_modelGetGroupByName(Model *model, const char *name)
{
	return model->getGroupByName(name);
}

const char *scriptif_groupGetName(Model *model, unsigned groupId)
{
	return model->getGroupName(groupId);
}

int scriptif_modelGetTextureByName(Model *model, const char *name)
{
	unsigned count = model->getTextureCount();

	for(unsigned t = 0; t<count; t++)
	{
		const char *tname = model->getTextureName(t);
		if(strcmp(name,tname)==0)
		{
			return t;
		}
	}

	return -1;
}

const char *scriptif_textureGetName(Model *model, unsigned textureId)
{
	return model->getTextureName(textureId);
}

const char *scriptif_textureGetFilename(Model *model, unsigned textureId)
{
	return model->getTextureFilename(textureId);
}

//-----------------------------------------------------------------------------
// Animation
//-----------------------------------------------------------------------------

bool scriptif_setAnimByIndex(Model *model, Model::AnimationModeE mode, unsigned anim)
{
	return model->setCurrentAnimation(anim,mode);
}

bool scriptif_setAnimByName(Model *model, Model::AnimationModeE mode, const char *name)
{
	int i = -1; for(auto*ea:model->getAnimationList())
	{
		i++; if(ea->_type==mode&&ea->m_name==name)
		{
			return model->setCurrentAnimation(i);
		}
	}
	return false;
}

void scriptif_animSetFrame(Model *model, unsigned frame)
{
	model->setCurrentAnimationFrame(frame);
}

void scriptif_setAnimTime(Model *model, double seconds)
{
	model->setCurrentAnimationTime(seconds);
}

int scriptif_modelCreateAnimation(Model *model,Model::AnimationModeE mode,
		const char *name, unsigned frameCount, double fps)
{
	int index = -1;

	index = model->addAnimation(mode,name);

	if(index>=0)
	{
		model->setAnimFrameCount(model->getAnim(mode,index),frameCount);
		model->setAnimFPS(model->getAnim(mode,index),fps);
	}

	return index;
}

int scriptif_animGetCount(Model *model,Model::AnimationModeE mode)
{
	return model->getAnimationCount(mode);
}

const char *scriptif_animGetName(Model *model,Model::AnimationModeE mode, unsigned animIndex)
{
	return model->getAnimName(model->getAnim(mode,animIndex));
}

int scriptif_animGetFrameCount(Model *model,Model::AnimationModeE mode, unsigned animIndex)
{
	return model->getAnimFrameCount(model->getAnim(mode,animIndex));
}

void scriptif_animSetName(Model *model,Model::AnimationModeE mode,
		unsigned animIndex, const char *name)
{
	model->setAnimName(model->getAnim(mode,animIndex),name);
}

void scriptif_animSetFrameCount(Model *model,Model::AnimationModeE mode,
		unsigned animIndex, unsigned frameCount)
{
	model->setAnimFrameCount(model->getAnim(mode,animIndex),frameCount);
}

void scriptif_animSetFPS(Model *model,Model::AnimationModeE mode,
		unsigned animIndex, double fps)
{
	model->setAnimFPS(model->getAnim(mode,animIndex),fps);
}

void scriptif_animClearFrame(Model *model,Model::AnimationModeE mode,
		unsigned animIndex, unsigned frame)
{
	model->deleteAnimFrame(model->getAnim(mode,animIndex),frame);
}

void scriptif_animCopyFrame(Model *model,Model::AnimationModeE mode,
		unsigned animIndex, unsigned src, unsigned dest)
{
	switch (mode)
	{
		case Model::ANIMMODE_FRAME:
			{
				unsigned count = model->getVertexCount();
				for(unsigned v = 0; v<count; v++)
				{
					double x = 0.0;
					double y = 0.0;
					double z = 0.0;

					auto e = model->getFrameAnimVertexCoords(animIndex,src,v,x,y,z);
					
					model->setFrameAnimVertexCoords(animIndex,dest,v,x,y,z,e);
				}
				#ifdef NDEBUG
//				#error And points?
				#endif
			}
			break;
		case Model::ANIMMODE_SKELETAL:
			{
				unsigned count = model->getBoneJointCount();
				for(Model::Position j{Model::PT_Joint,0}; j<count; j++)
				{
					double x = 0.0;
					double y = 0.0;
					double z = 0.0;

					auto e = model->getKeyframe(animIndex,src,j,Model::KeyRotate,x,y,z);
					
					model->setKeyframe(animIndex,dest,j,Model::KeyRotate,x,y,z,e);
					
					e = model->getKeyframe(animIndex,src,j,Model::KeyTranslate,x,y,z);
					
					model->setKeyframe(animIndex,dest,j,Model::KeyTranslate,x,y,z,e);
				}
			}
			break;
		//case ANIMMODE_FRAMERELATIVE: //UNIMPLMENTED
			//break;
		default:
			break;
	}
}

void scriptif_animMove(Model *model,Model::AnimationModeE mode, unsigned animIndex1, unsigned animIndex2)
{
	model->moveAnimation(model->getAnim(mode,animIndex1),model->getAnim(mode,animIndex2));
}

int scriptif_animCopy(Model *model,Model::AnimationModeE mode, unsigned animIndex, const char *name)
{
	return model->copyAnimation(model->getAnim(mode,animIndex),name);
}

int scriptif_animSplit(Model *model,Model::AnimationModeE mode, unsigned animIndex, const char *name, unsigned frame)
{
	return model->splitAnimation(model->getAnim(mode,animIndex),name,frame);
}

void scriptif_animJoin(Model *model,Model::AnimationModeE mode, unsigned anim1, unsigned anim2)
{
	model->joinAnimations(model->getAnim(mode,anim1),model->getAnim(mode,anim2));
}

int scriptif_animConvertToFrame(Model *model,Model::AnimationModeE mode, unsigned animIndex, const char *newName, unsigned frameCount, Model::Interpolate2020E how)
{
	return model->convertAnimToFrame(model->getAnim(mode,animIndex),newName,frameCount,how);
}

// Skeletal Animation

void scriptif_skelAnimSetKeyframe(Model *model,
		unsigned animIndex, unsigned frame, unsigned joint, Model::KeyType2020E isRotation,
		double x, double y, double z, Model::Interpolate2020E e)
{
	if(isRotation)
	{
		x *= PIOVER180;
		y *= PIOVER180;
		z *= PIOVER180;
	}

	model->setKeyframe(animIndex,frame,{Model::PT_Joint,joint},isRotation,x,y,z,e);
}

void scriptif_skelAnimDeleteKeyframe(Model *model,
		unsigned animIndex, unsigned frame, unsigned joint, Model::KeyType2020E isRotation)
{
	model->deleteKeyframe(animIndex,frame,{Model::PT_Joint,joint},isRotation);
}

// Frame Animation

void scriptif_frameAnimSetVertex(Model *model, unsigned animIndex,
		unsigned frame, unsigned v, double x, double y, double z, Model::Interpolate2020E e)
{
	model->setFrameAnimVertexCoords(animIndex,frame,v,x,y,z,e);
}

//-----------------------------------------------------------------------------
// Selection
//-----------------------------------------------------------------------------

void scriptif_modelSelectAll(Model *model)
{
	unsigned count = 0;
	unsigned t = 0;

	count = model->getTriangleCount();
	for(t = 0; t<count; t++)
	{
		model->selectTriangle(t);
	}
	
	count = model->getBoneJointCount();
	for(t = 0; t<count; t++)
	{
		model->selectBoneJoint(t);
	}
}

void scriptif_modelSelectAllVertices(Model *model)
{
	unsigned count = model->getVertexCount();
	for(unsigned t = 0; t<count; t++)
	{
		model->selectVertex(t);
	}
}

void scriptif_modelSelectAllFaces(Model *model)
{
	unsigned count = model->getTriangleCount();
	for(unsigned t = 0; t<count; t++)
	{
		model->selectTriangle(t);
	}
}

void scriptif_modelSelectAllGroups(Model *model)
{
	unsigned count = model->getGroupCount();
	for(unsigned t = 0; t<count; t++)
	{
		model->selectGroup(t);
	}
}

void scriptif_modelSelectAllJoints(Model *model)
{
	unsigned count = model->getBoneJointCount();
	for(unsigned t = 0; t<count; t++)
	{
		model->selectBoneJoint(t);
	}
}

void scriptif_modelSelectVertex(Model *model, int vertex)
{
	model->selectVertex(vertex);
}

void scriptif_modelSelectFace(Model *model, int face)
{
	model->selectTriangle(face);
}

void scriptif_modelSelectGroup(Model *model, int group)
{
	model->selectGroup(group);
}

void scriptif_modelSelectJoint(Model *model, int joint)
{
	model->selectBoneJoint(joint);
}

void scriptif_modelUnselectAll(Model *model)
{
	model->unselectAll();
}

//-----------------------------------------------------------------------------
// Logging
//-----------------------------------------------------------------------------

void scriptif_logDebug(const char *str)
{
	//log_debug("script: %s\n",str);
}

void scriptif_logWarning(const char *str)
{
	log_warning("script: %s\n",str);
}

void scriptif_logError(const char *str)
{
	log_error("script: %s\n",str);
}

