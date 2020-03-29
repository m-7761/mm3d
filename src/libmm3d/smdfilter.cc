/*  SmdFilter plugin for Maverick Model 3D
 *
 * Copyright (c)2018 Zack Middleton
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

#include "smdfilter.h"

#include "texture.h"
#include "texmgr.h"
#include "misc.h"
#include "log.h"
#include "translate.h"
#include "modelstatus.h"
#include "mesh.h"

#include "mm3dport.h"
#include "datadest.h"

SmdFilter::SmdOptions::SmdOptions()
	: m_saveMeshes(true),
	  m_savePointsJoint(false),
	  m_multipleVertexInfluences(false),
	  m_animations()
{}

void SmdFilter::SmdOptions::setOptionsFromModel(Model *m)
{
	char value[128];
	if(m->getMetaData("smd_vertex_format",value,sizeof(value)))
	{
		if(PORT_strcasecmp(value,"goldsrc")==0||PORT_strcasecmp(value,"goldsource")==0)
		{
			m_multipleVertexInfluences = false;
		}
		else //if(PORT_strcasecmp(value,"source")==0)
		{
			m_multipleVertexInfluences = true;
		}
	}
	else
	{
		// No vertex format defined. Use goldsrc unless we have multiple bone joint
		// influences for any vertex.
		m_multipleVertexInfluences = false;
		unsigned vcount = m->getVertexCount();
		infl_list il;
		for(unsigned v = 0; m_multipleVertexInfluences==false&&v<vcount; v++)
		{
			m->getVertexInfluences(v,il);
			if(il.size()>1)
			{
				m_multipleVertexInfluences = true;
			}
		}
	}

	if(m->getMetaData("smd_points_as_joints",value,sizeof(value))&&atoi(value)!=0)
	{
		m_savePointsJoint = true;
	}
	else
	{
		m_savePointsJoint = false;
	}
}

Model::ModelErrorE SmdFilter::readFile(Model *model, const char *const filename)
{
	return Model::ERROR_UNSUPPORTED_OPERATION;
}

Model::ModelErrorE SmdFilter::writeFile(Model *model, const char *const filename, Options &o)
{
	m_options = o.getOptions<SmdOptions>();

	// Check for identical bone joint names
	{
		unsigned c = model->getBoneJointCount();
		for(unsigned i = 0; i<c; i++)
		for(unsigned j = i+1; j<c; j++)
		if(strcmp(model->getBoneJointName(i),model->getBoneJointName(j))==0)
		{
			model->setFilterSpecificError(TRANSLATE("LowLevel","Bone joints must have unique names for SMD export."));
			return Model::ERROR_FILTER_SPECIFIC;
		}
	}

	if(m_options->m_savePointsJoint)
	{
		unsigned pcount = model->getPointCount();
		for(unsigned p = 0; p<pcount; ++p)
		{
			//infl_list il;
			//model->getPointInfluences(p,il);
			//if(il.size()>1)
			if(1<model->getPointInfluences(p).size())
			{
				model->setFilterSpecificError(TRANSLATE("LowLevel","SMD export requires points to only have one bone influence."));
				return Model::ERROR_FILTER_SPECIFIC;
			}
		}
	}

	Model::ModelErrorE err = Model::ERROR_NONE;
	DataDest *dst = openOutput(filename,err);
	DestCloser fc(dst);

	if(err!=Model::ERROR_NONE)
		return err;

	// Use the load matrix and then invert it
	Matrix saveMatrix;
	saveMatrix.setRotationInDegrees(-90,0,0);
	saveMatrix = saveMatrix.getInverse();

	unsigned boneCount = model->getBoneJointCount();
	unsigned pointCount = model->getPointCount();
	bool defaultBoneJoint = false;

	if(boneCount==0&&m_options->m_saveMeshes&&model->getTriangleCount()>0)
	{
		defaultBoneJoint = true;
	}

	//
	// Write Header
	//
	writeLine(dst,"version 1");

	//
	// Write Joints list
	//
	writeLine(dst,"nodes");

	if(defaultBoneJoint)
	{
		writeLine(dst,"0 \"root\" -1");
	}
	else
	{
		for(int bone = 0; bone<boneCount; bone++)
		{
			int parent = model->getBoneJointParent(bone);

			writeLine(dst,"%d \"%s\" %d",bone,model->getBoneJointName(bone),parent);
		}
	}

	if(m_options->m_savePointsJoint)
	{
		for(unsigned point = 0; point<pointCount; point++)
		{
			int parent = model->getPrimaryPointInfluence(point);

			writeLine(dst,"%d \"%s\" %d",(int)defaultBoneJoint+boneCount+point,model->getPointName(point),parent);
		}
	}

	writeLine(dst,"end");

	//
	// Write Bind Pose
	//
	if(m_options->m_animations.size()>0&&m_options->m_animations[0]==UINT_MAX)
	{
		writeLine(dst,"skeleton");

		// write base pose as frame 0
		writeLine(dst,"time 0");

		if(defaultBoneJoint)
		{
			writeLine(dst,"%d %.6f %.6f %.6f %.6f %.6f %.6f",
							0,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f);
		}
		else
		{
			for(int bone = 0; bone<boneCount; bone++)
			{
				int parent = model->getBoneJointParent(bone);

				Matrix m;
				model->getBoneJointAbsoluteMatrix(bone,m);
				m = m *saveMatrix;

				Matrix pinv;
				if(parent>=0)
				{
					model->getBoneJointAbsoluteMatrix(parent,pinv);
					pinv = pinv *saveMatrix;
					pinv = pinv.getInverse();
				}

				Matrix lm;
				lm = m *pinv;

				double trans[3];
				double rot[3];
				lm.getTranslation(trans);
				lm.getRotation(rot);

				writeLine(dst,"%d %.6f %.6f %.6f %.6f %.6f %.6f",bone,
								(float)trans[0],(float)trans[1],(float)trans[2],
								(float)rot[0],(float)rot[1],(float)rot[2]);
			}
		}

		if(m_options->m_savePointsJoint)
		{
			for(int point = 0; point<pointCount; point++)
			{
				int parent = model->getPrimaryPointInfluence(point);

				Matrix m;
				model->getPointAbsoluteMatrix(point,m);
				m = m *saveMatrix;

				Matrix pinv;
				if(parent>=0)
				{
					model->getBoneJointAbsoluteMatrix(parent,pinv);
					pinv = pinv *saveMatrix;
					pinv = pinv.getInverse();
				}

				Matrix lm;
				lm = m *pinv;

				double trans[3];
				double rot[3];
				lm.getTranslation(trans);
				lm.getRotation(rot);

				writeLine(dst,"%d %.6f %.6f %.6f %.6f %.6f %.6f",boneCount+point,
								(float)trans[0],(float)trans[1],(float)trans[2],
								(float)rot[0],(float)rot[1],(float)rot[2]);
			}
		}

		writeLine(dst,"end");
	}

	//
	// Write Animations
	//
	if(m_options->m_animations.size()>0)
	{
		std::vector<Matrix> poseFinal;

		poseFinal.reserve(boneCount);

		// Animation numbers to export are set by smdprompt_show()
		// Note: Prompt is limited to only selecting one animation as SMD doesn't officially support multiple animations in one SMD.
		std::vector<unsigned>::iterator it;
		for(it = m_options->m_animations.begin(); it!=m_options->m_animations.end(); it++)
		{
			if(*it==UINT_MAX)
			{
				// Bind pose was already written
				continue;
			}

			unsigned anim = *it;
			//const char *animName = model->getAnimName(Model::ANIMMODE_SKELETAL,anim);
			float fps = model->getAnimFPS(Model::ANIMMODE_SKELETAL,anim);
			unsigned frameCount = model->getAnimFrameCount(Model::ANIMMODE_SKELETAL,anim);
			//bool loop = model->getAnimWrap(Model::ANIMMODE_SKELETAL,anim);

			if(frameCount==0)
			{
				continue;
			}

			//writeLine(dst,"// animation: %s,framerate %.6f%s",animName,fps,loop ? ",looping" : "");
			writeLine(dst,"skeleton");

			//FIX ME (Needs sparse frames logic)
			for(unsigned frame = 0; frame<frameCount; frame++)
			{
			//	double frameTime = frame/(double)fps;

				// "time N" must be written even if there are no keyframes for this frame.
				writeLine(dst,"time %d",frame);

				if(defaultBoneJoint)
				{
					writeLine(dst,"%d %.6f %.6f %.6f %.6f %.6f %.6f",
									0,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f);
				}
				else
				{
					for(unsigned bone = 0; bone<boneCount; bone++)
					{
						int parent = model->getBoneJointParent(bone);

						Matrix transform;
				//		model->interpKeyframeTime(anim,frameTime,loop,bone,transform);
						model->interpKeyframe(anim,frame,{Model::PT_Joint,bone},transform);

						Matrix rm;
						model->getBoneJointRelativeMatrix(bone,rm);

						Matrix relativeFinal = transform *rm;

						if(parent==-1)
						{
							poseFinal[bone] = relativeFinal *saveMatrix;
						}
						else
						{
							poseFinal[bone] = relativeFinal *poseFinal[parent];
						}

						Matrix m = poseFinal[bone];

						Matrix pinv;
						if(parent>=0)
						{
							pinv = poseFinal[parent];
							pinv = pinv.getInverse();
						}

						Matrix lm;
						lm = m *pinv;

						double trans[3];
						double rot[3];
						lm.getTranslation(trans);
						lm.getRotation(rot);

						// Studiomdl copies previous keyframe when a joint keyframes is not specified
						// instead of lerping between specified keyframes. So just write all joint
						// keyframes for all frames.

						// FIXME?: Printf %.6f can round "rot" to 0.000001 greater than M_PI.
						writeLine(dst,"%d %.6f %.6f %.6f %.6f %.6f %.6f",bone,
										(float)trans[0],(float)trans[1],(float)trans[2],
										(float)rot[0],(float)rot[1],(float)rot[2]);
					}
				}

				// This is the same as point joint matricies. There is
				// no animation. It's only useful for compatibility with
				// programs that require all joints to be animated.
				if(m_options->m_savePointsJoint&&pointCount>0)
				{
					for(int point = 0; point<pointCount; point++)
					{
						int parent = model->getPrimaryPointInfluence(point);

						Matrix m;
						model->getPointAbsoluteMatrix(point,m);
						m = m *saveMatrix;

						Matrix pinv;
						if(parent>=0)
						{
							model->getBoneJointAbsoluteMatrix(parent,pinv);
							pinv = pinv *saveMatrix;
							pinv = pinv.getInverse();
						}

						Matrix lm;
						lm = m *pinv;

						double trans[3];
						double rot[3];
						lm.getTranslation(trans);
						lm.getRotation(rot);

						// FIXME?: Printf %.6f can round "rot" to 0.000001 greater than M_PI.
						writeLine(dst,"%d %.6f %.6f %.6f %.6f %.6f %.6f",boneCount+point,
										(float)trans[0],(float)trans[1],(float)trans[2],
										(float)rot[0],(float)rot[1],(float)rot[2]);
					}
				}
			}

			writeLine(dst,"end");
		}
	}

	//
	// Write Meshes
	//
	if(m_options->m_saveMeshes&&model->getTriangleCount()>0)
	{
		writeLine(dst,"triangles");

		std::vector<Model::Material*> &modelMaterials = getMaterialList(model);

		int groupCount = model->getGroupCount();
		for(int g = 0; g<groupCount+1; g++)
		{
			int matId;
			int_list faces;
			int_list::iterator fit;
			std::string materialName;

			if(g==groupCount)
			{
				matId = -1;
				faces = model->getUngroupedTriangles();
			}
			else
			{
				matId = model->getGroupTextureId(g);
				faces = model->getGroupTriangles(g);
			}

			if(matId!=-1)
			{
				// TODO: Add option to save material name instead of texture name?
				//materialName = modelMaterials[matId]->m_name;
				materialName = PORT_basename(modelMaterials[matId]->m_filename.c_str());
			}

			if(materialName.length()==0)
			{
				materialName = "default.bmp";
			}

			for(fit = faces.begin(); fit!=faces.end(); fit++)
			{
				writeLine(dst,"%s",materialName.c_str());

				for(int vindex = 0; vindex<3; vindex++)
				{
					int vert = model->getTriangleVertex(*fit,vindex);
					double meshVec[4] = { 0,0,0,1 };
					double meshNor[4] = { 0,0,0,1 };
					float uv[4] = { 0,0 };

					model->getVertexCoordsUnanimated(vert,meshVec);
					model->getNormalUnanimated(*fit,vindex,meshNor);
					model->getTextureCoords(*fit,vindex,uv[0],uv[1]);

					saveMatrix.apply(meshVec);
					saveMatrix.apply(meshNor);

					// TODO: Move to Model::getVertexInfluencesNormalizedNonZero(vertex,list,epsilon)or something.
					//		 model->getPrimaryVertexInfluence(vert) returns a bone even if the weight is zero or would be zero in printf.
					infl_list influences;
					infl_list::iterator it;
					model->getVertexInfluences(vert,influences);

					// Sort highest weight first
					//influences.sort(std::greater<Model::InfluenceT>());
					std::sort(influences.begin(),influences.end(),std::greater<Model::InfluenceT>());

					// Our weights don't always equal 100%,get total weigth so we can normalize
					double total = 0.0;
					for(it = influences.begin(); it!=influences.end(); it++)
					{
						total += it->m_weight;
					}

					// Don't allow negative weights,or divide by zero
					if(total<0.0005)
					{
						total = 1.0;
					}

					size_t weightCount = 0;
					for(it = influences.begin(); it!=influences.end(); it++,weightCount++)
					{
						it->m_weight = (it->m_weight/total);

						// Lower than 0.0000005 round to 0 in prinf %.6f
						if(it->m_weight<5e-7)
						{
							// TODO: For each previous weight add it->m_weight/weightCount ?
							influences.resize(weightCount);
							break;
						}
					}

					int vertBone = 0;
					if(!m_options->m_multipleVertexInfluences)
					{
						if(influences.size()>0)
						{
							vertBone = influences.begin()->m_boneId;
						}
					}

					dst->writePrintf("%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f",
						vertBone,
						(float)meshVec[0],(float)meshVec[1],(float)meshVec[2],
						meshNor[0],meshNor[1],meshNor[2],
						uv[0],uv[1]);

					if(m_options->m_multipleVertexInfluences)
					{
						dst->writePrintf(" %d",(int)influences.size());
						for(it = influences.begin(); it!=influences.end(); it++)
						{
							dst->writePrintf(" %d %.6f",it->m_boneId,(float)it->m_weight);
						}
					}

					dst->writePrintf("\r\n");
				}
			}
		}

		writeLine(dst,"end");
	}

	if(m_options->m_multipleVertexInfluences)
	{
		model->updateMetaData("smd_vertex_format","source");
	}
	else
	{
		model->updateMetaData("smd_vertex_format","goldsrc");
	}

	if(m_options->m_savePointsJoint)
	{
		model->updateMetaData("smd_points_as_joints","1");
	}
	else
	{
		model->updateMetaData("smd_points_as_joints","0");
	}

	model->operationComplete(TRANSLATE("LowLevel","Set meta data for SMD export"));

	return Model::ERROR_NONE;
}

bool SmdFilter::writeLine(DataDest *dst, const char *line,...)
{
	va_list ap;
	va_start(ap,line);
	dst->writeVPrintf(line,ap);
	va_end(ap);
	dst->writePrintf("\r\n");
	return true;
}
