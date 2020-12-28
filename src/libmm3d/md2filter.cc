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

//#include "md2filter.h"
class Md2Filter : public ModelFilter
{
public:
	Md2Filter();
	virtual ~Md2Filter();

	Model::ModelErrorE readFile(Model *model, const char *const filename);
	Model::ModelErrorE writeFile(Model *model, const char *const filename, Options&);

	const char *getReadTypes(){ return "MD2"; }
	const char *getWriteTypes(){ return "MD2"; }

protected:

	int addNeededAnimFrame(Model *model, const char *name);

	std::string m_lastAnimFrame;
	int			m_lastAnimIndex;
	int			m_animFrame;
};

#include "texture.h"
#include "texmgr.h"
#include "misc.h"
#include "log.h"
//#include "triprim.h" //TriPrim
#include "translate.h"
#include "modelstatus.h"

#include "mm3dport.h"
#include "datasource.h"
#include "datadest.h"
  
//2019: No other files use this class. It can be made public
//if it needs to be shared.
class TriPrim 
{
public:

	virtual ~TriPrim(){}

	enum TriPrimTypeE
	{
		TRI_PRIM_STRIP,
		TRI_PRIM_FAN,
		TRI_PRIM_NONE
	};

	struct TriangleVertexT
	{
		int	vert;
		float s;
		float t;
	};
	
	typedef std::vector<TriangleVertexT> TriangleVertexList;

	typedef TriPrim *(*NewTriPrimFunc)();

	// grouped:
	//	 true  = textures and texture coordinates must match
	//	 false = ignore texture information
	//
	// Returns:
	//	 true  = found strips,fans
	//	 false = error parsing triangle data
	virtual bool findPrimitives(Model *model,bool grouped = true)= 0;

	virtual void resetList()= 0; // Reset to start of list

	virtual TriPrimTypeE getNextPrimitive(TriangleVertexList &tvl)= 0;

	static void registerTriPrimFunction(NewTriPrimFunc newFunc); //UNUSED

	static TriPrim *newTriPrim()
	{
		//return s_newFunc?s_newFunc():nullptr
		return s_newFunc();
	}

protected:

	static NewTriPrimFunc s_newFunc;
};
class SimpleTriPrim : public TriPrim
{
public:

	SimpleTriPrim():m_model(),m_triNumber()
	{}

	bool findPrimitives(Model *model,bool grouped = true)
	{
		m_model = model; return true;
	}

	void resetList(){ m_triNumber = 0; }; // Reset to start of list

	TriPrimTypeE getNextPrimitive(TriangleVertexList &tvl)
	{
		tvl.clear();
		int count = m_model->getTriangleCount();
		log_debug("getting triangle %d of %d\n",m_triNumber+1,count);
		if(m_model!=nullptr&&m_triNumber<m_model->getTriangleCount())
		{
			TriPrim::TriangleVertexT tv;
			for(unsigned v = 0; v<3; v++)
			{
				tv.vert = m_model->getTriangleVertex(m_triNumber,v);
				m_model->getTextureCoords(m_triNumber,v,tv.s,tv.t);
				tvl.push_back(tv);
			}
			m_triNumber++;
			return TRI_PRIM_FAN;
		}
		else return TRI_PRIM_NONE;
	}

protected:

	Model *m_model; int m_triNumber;
};
static TriPrim *md2filter_newSimpleTriPrimFunc(){ return new SimpleTriPrim(); }
TriPrim::NewTriPrimFunc TriPrim::s_newFunc = md2filter_newSimpleTriPrimFunc;
void TriPrim::registerTriPrimFunction(NewTriPrimFunc newFunc) 
{
	s_newFunc = newFunc?newFunc:md2filter_newSimpleTriPrimFunc;
}

const unsigned MAX_QUAKE_NORMALS = 162;
static float s_quakeNormals[MAX_QUAKE_NORMALS][3] = 
{{-0.525731f,0.000000f,0.850651f},
{-0.442863f,0.238856f,0.864188f},
{-0.295242f,0.000000f,0.955423f},
{-0.309017f,0.500000f,0.809017f},
{-0.162460f,0.262866f,0.951056f},
{0.000000f,0.000000f,1.000000f},
{0.000000f,0.850651f,0.525731f},
{-0.147621f,0.716567f,0.681718f},
{0.147621f,0.716567f,0.681718f},
{0.000000f,0.525731f,0.850651f},
{0.309017f,0.500000f,0.809017f},
{0.525731f,0.000000f,0.850651f},
{0.295242f,0.000000f,0.955423f},
{0.442863f,0.238856f,0.864188f},
{0.162460f,0.262866f,0.951056f},
{-0.681718f,0.147621f,0.716567f},
{-0.809017f,0.309017f,0.500000f},
{-0.587785f,0.425325f,0.688191f},
{-0.850651f,0.525731f,0.000000f},
{-0.864188f,0.442863f,0.238856f},
{-0.716567f,0.681718f,0.147621f},
{-0.688191f,0.587785f,0.425325f},
{-0.500000f,0.809017f,0.309017f},
{-0.238856f,0.864188f,0.442863f},
{-0.425325f,0.688191f,0.587785f},
{-0.716567f,0.681718f,-0.147621f},
{-0.500000f,0.809017f,-0.309017f},
{-0.525731f,0.850651f,0.000000f},
{0.000000f,0.850651f,-0.525731f},
{-0.238856f,0.864188f,-0.442863f},
{0.000000f,0.955423f,-0.295242f},
{-0.262866f,0.951056f,-0.162460f},
{0.000000f,1.000000f,0.000000f},
{0.000000f,0.955423f,0.295242f},
{-0.262866f,0.951056f,0.162460f},
{0.238856f,0.864188f,0.442863f},
{0.262866f,0.951056f,0.162460f},
{0.500000f,0.809017f,0.309017f},
{0.238856f,0.864188f,-0.442863f},
{0.262866f,0.951056f,-0.162460f},
{0.500000f,0.809017f,-0.309017f},
{0.850651f,0.525731f,0.000000f},
{0.716567f,0.681718f,0.147621f},
{0.716567f,0.681718f,-0.147621f},
{0.525731f,0.850651f,0.000000f},
{0.425325f,0.688191f,0.587785f},
{0.864188f,0.442863f,0.238856f},
{0.688191f,0.587785f,0.425325f},
{0.809017f,0.309017f,0.500000f},
{0.681718f,0.147621f,0.716567f},
{0.587785f,0.425325f,0.688191f},
{0.955423f,0.295242f,0.000000f},
{1.000000f,0.000000f,0.000000f},
{0.951056f,0.162460f,0.262866f},
{0.850651f,-0.525731f,0.000000f},
{0.955423f,-0.295242f,0.000000f},
{0.864188f,-0.442863f,0.238856f},
{0.951056f,-0.162460f,0.262866f},
{0.809017f,-0.309017f,0.500000f},
{0.681718f,-0.147621f,0.716567f},
{0.850651f,0.000000f,0.525731f},
{0.864188f,0.442863f,-0.238856f},
{0.809017f,0.309017f,-0.500000f},
{0.951056f,0.162460f,-0.262866f},
{0.525731f,0.000000f,-0.850651f},
{0.681718f,0.147621f,-0.716567f},
{0.681718f,-0.147621f,-0.716567f},
{0.850651f,0.000000f,-0.525731f},
{0.809017f,-0.309017f,-0.500000f},
{0.864188f,-0.442863f,-0.238856f},
{0.951056f,-0.162460f,-0.262866f},
{0.147621f,0.716567f,-0.681718f},
{0.309017f,0.500000f,-0.809017f},
{0.425325f,0.688191f,-0.587785f},
{0.442863f,0.238856f,-0.864188f},
{0.587785f,0.425325f,-0.688191f},
{0.688191f,0.587785f,-0.425325f},
{-0.147621f,0.716567f,-0.681718f},
{-0.309017f,0.500000f,-0.809017f},
{0.000000f,0.525731f,-0.850651f},
{-0.525731f,0.000000f,-0.850651f},
{-0.442863f,0.238856f,-0.864188f},
{-0.295242f,0.000000f,-0.955423f},
{-0.162460f,0.262866f,-0.951056f},
{0.000000f,0.000000f,-1.000000f},
{0.295242f,0.000000f,-0.955423f},
{0.162460f,0.262866f,-0.951056f},
{-0.442863f,-0.238856f,-0.864188f},
{-0.309017f,-0.500000f,-0.809017f},
{-0.162460f,-0.262866f,-0.951056f},
{0.000000f,-0.850651f,-0.525731f},
{-0.147621f,-0.716567f,-0.681718f},
{0.147621f,-0.716567f,-0.681718f},
{0.000000f,-0.525731f,-0.850651f},
{0.309017f,-0.500000f,-0.809017f},
{0.442863f,-0.238856f,-0.864188f},
{0.162460f,-0.262866f,-0.951056f},
{0.238856f,-0.864188f,-0.442863f},
{0.500000f,-0.809017f,-0.309017f},
{0.425325f,-0.688191f,-0.587785f},
{0.716567f,-0.681718f,-0.147621f},
{0.688191f,-0.587785f,-0.425325f},
{0.587785f,-0.425325f,-0.688191f},
{0.000000f,-0.955423f,-0.295242f},
{0.000000f,-1.000000f,0.000000f},
{0.262866f,-0.951056f,-0.162460f},
{0.000000f,-0.850651f,0.525731f},
{0.000000f,-0.955423f,0.295242f},
{0.238856f,-0.864188f,0.442863f},
{0.262866f,-0.951056f,0.162460f},
{0.500000f,-0.809017f,0.309017f},
{0.716567f,-0.681718f,0.147621f},
{0.525731f,-0.850651f,0.000000f},
{-0.238856f,-0.864188f,-0.442863f},
{-0.500000f,-0.809017f,-0.309017f},
{-0.262866f,-0.951056f,-0.162460f},
{-0.850651f,-0.525731f,0.000000f},
{-0.716567f,-0.681718f,-0.147621f},
{-0.716567f,-0.681718f,0.147621f},
{-0.525731f,-0.850651f,0.000000f},
{-0.500000f,-0.809017f,0.309017f},
{-0.238856f,-0.864188f,0.442863f},
{-0.262866f,-0.951056f,0.162460f},
{-0.864188f,-0.442863f,0.238856f},
{-0.809017f,-0.309017f,0.500000f},
{-0.688191f,-0.587785f,0.425325f},
{-0.681718f,-0.147621f,0.716567f},
{-0.442863f,-0.238856f,0.864188f},
{-0.587785f,-0.425325f,0.688191f},
{-0.309017f,-0.500000f,0.809017f},
{-0.147621f,-0.716567f,0.681718f},
{-0.425325f,-0.688191f,0.587785f},
{-0.162460f,-0.262866f,0.951056f},
{0.442863f,-0.238856f,0.864188f},
{0.162460f,-0.262866f,0.951056f},
{0.309017f,-0.500000f,0.809017f},
{0.147621f,-0.716567f,0.681718f},
{0.000000f,-0.525731f,0.850651f},
{0.425325f,-0.688191f,0.587785f},
{0.587785f,-0.425325f,0.688191f},
{0.688191f,-0.587785f,0.425325f},
{-0.955423f,0.295242f,0.000000f},
{-0.951056f,0.162460f,0.262866f},
{-1.000000f,0.000000f,0.000000f},
{-0.850651f,0.000000f,0.525731f},
{-0.955423f,-0.295242f,0.000000f},
{-0.951056f,-0.162460f,0.262866f},
{-0.864188f,0.442863f,-0.238856f},
{-0.951056f,0.162460f,-0.262866f},
{-0.809017f,0.309017f,-0.500000f},
{-0.864188f,-0.442863f,-0.238856f},
{-0.951056f,-0.162460f,-0.262866f},
{-0.809017f,-0.309017f,-0.500000f},
{-0.681718f,0.147621f,-0.716567f},
{-0.681718f,-0.147621f,-0.716567f},
{-0.850651f,0.000000f,-0.525731f},
{-0.688191f,0.587785f,-0.425325f},
{-0.587785f,0.425325f,-0.688191f},
{-0.425325f,0.688191f,-0.587785f},
{-0.425325f,-0.688191f,-0.587785f},
{-0.587785f,-0.425325f,-0.688191f},
{-0.688191f,-0.587785f,-0.425325f}};

static void md2filter_invertModelNormals(Model *model)
{
	size_t tcount = model->getTriangleCount();
	for(size_t t = 0; t<tcount; t++)
	{
		model->invertNormals(t);
	}
}

static unsigned bestNormal(float *norm)
{
	float bestDistance = 10000; //3 should do it?
	unsigned bestIndex = 0;

	for(unsigned t = 0; t<MAX_QUAKE_NORMALS; t++)
	{
		float x = norm[0]-s_quakeNormals[t][0];
		float y = norm[1]-s_quakeNormals[t][1];
		float z = norm[2]-s_quakeNormals[t][2];

		//float distance = sqrt(x*x+y*y+z*z);
		float distance = x*x+y*y+z*z;

		if(distance<bestDistance)
		{
			bestIndex = t;
			bestDistance = distance;
		}
	}

	return bestIndex;
}

struct md2filter_TexCoordT{ float s,t; };

Md2Filter::Md2Filter()
	: m_lastAnimFrame(""),
	  m_lastAnimIndex(-1)
{
}

Md2Filter::~Md2Filter()
{
}

Model::ModelErrorE Md2Filter::readFile(Model *model, const char *const filename)
{
	Model::ModelErrorE err = Model::ERROR_NONE;
	DataSource* src = openInput(filename,err);
	SourceCloser fc(src);

	if(err!=Model::ERROR_NONE)
		return err;

	std::string modelPath = "";
	std::string modelBaseName = "";
	std::string modelFullName = "";

	normalizePath(filename,modelFullName,modelPath,modelBaseName);
	modelPath = modelPath+std::string("/");
		
	model->setFilename(modelFullName.c_str());

	unsigned fileLength = src->getFileSize();

	int32_t magic = 0;
	int32_t version = 0;
	int32_t skinWidth = 0;
	int32_t skinHeight = 0;
	int32_t frameSize = 0;
	int32_t numSkins = 0;
	int32_t numVertices = 0;
	int32_t numTexCoords = 0;
	int32_t numTriangles = 0;
	int32_t numGlCommands = 0;
	int32_t numFrames = 0;
	int32_t offsetSkins = 0;
	int32_t offsetTexCoords = 0;
	int32_t offsetTriangles = 0;
	int32_t offsetFrames = 0;
	int32_t offsetGlCommands = 0;
	int32_t offsetEnd = 0;

	src->read(magic);
	src->read(version);
	src->read(skinWidth);
	src->read(skinHeight);
	src->read(frameSize);
	src->read(numSkins);
	src->read(numVertices);
	src->read(numTexCoords);
	src->read(numTriangles);
	src->read(numGlCommands);
	src->read(numFrames);
	src->read(offsetSkins);
	src->read(offsetTexCoords);
	src->read(offsetTriangles);
	src->read(offsetFrames);
	src->read(offsetGlCommands);
	src->read(offsetEnd);

	Matrix loadMatrix;
	loadMatrix.setRotationInDegrees(-90,-90,0);

	if(magic!=0x32504449)
	{
		return Model::ERROR_BAD_MAGIC;
	}

	if(version!=8)
	{
		return Model::ERROR_UNSUPPORTED_VERSION;
	}

	log_debug("Magic: %08X\n",	 (unsigned)magic);
	log_debug("Version: %d\n",	 version);
	log_debug("Vertices: %d\n",	numVertices);
	log_debug("Faces: %d\n",		numTriangles);
	log_debug("Skins: %d\n",		numSkins);
	log_debug("Frames: %d\n",	  numFrames);
	log_debug("GL Commands: %d\n",numGlCommands);
	log_debug("Frame Size: %d\n", frameSize);
	log_debug("TexCoords: %d\n",  numTexCoords);
	log_debug("Skin Width: %d\n", skinWidth);
	log_debug("Skin Height: %d\n",skinHeight);

	log_debug("Offset Skins: %d\n",	  offsetSkins);
	log_debug("Offset TexCoords: %d\n", offsetTexCoords);
	log_debug("Offset Triangles: %d\n", offsetTriangles);
	log_debug("Offset Frames: %d\n",	 offsetFrames);
	log_debug("Offset GlCommands: %d\n",offsetGlCommands);
	log_debug("Offset End: %d\n",		 offsetEnd);

	log_debug("File Length: %d\n",		fileLength);

	int32_t i;
	unsigned t;
	unsigned vertex;

	int animIndex = -1;
	m_lastAnimIndex = -1;
	m_animFrame = -1;
	m_lastAnimFrame = "";
		
	// Read first frame to get vertices
	src->seek(offsetFrames);
		
	float scale[3];
	float translate[3];
	char name[64];
	for(t = 0; t<3; t++)
		src->read(scale[t]);
	for(t = 0; t<3; t++)
		src->read(translate[t]);

	//loadMatrix.apply3(scale);
	//loadMatrix.apply3(translate);

	src->readBytes(name,16);

	for(i = 0; i<numVertices; i++)
	{
		uint8_t coord[3];
		uint8_t normal;
		for(t = 0; t<3; t++)
		{
			src->read(coord[t]);
		}
		src->read(normal);

		double vec[3];
		vec[0] = coord[0] *scale[0]+translate[0];
		vec[1] = coord[1] *scale[1]+translate[1];
		vec[2] = coord[2] *scale[2]+translate[2];
		loadMatrix.apply3(vec);

		vertex = model->addVertex(
				vec[0],
				vec[1],
				vec[2]);
	}

	// Now read all frames to get animation vertices
	src->seek(offsetFrames);
		
	if(numFrames>1)
	{
		for(int n = 0; n<numFrames; n++)
		{
			for(t = 0; t<3; t++)
				src->read(scale[t]);
			for(t = 0; t<3; t++)
				src->read(translate[t]);

			src->readBytes(name,16);

			animIndex = addNeededAnimFrame(model,name);

			for(i = 0; i<numVertices; i++)
			{
				uint8_t coord[3];
				uint8_t normal;
				for(t = 0; t<3; t++)
					src->read(coord[t]);
				src->read(normal);

				double vec[3];
				vec[0] = coord[0] *scale[0]+translate[0];
				vec[1] = coord[1] *scale[1]+translate[1];
				vec[2] = coord[2] *scale[2]+translate[2];
				loadMatrix.apply3(vec);

				//2020: Use Model::InterpolateStep mode.
				//model->setFrameAnimVertexCoords(animIndex,m_animFrame,i,
				model->setQuickFrameAnimVertexCoords(animIndex,m_animFrame,i,
						vec[0],
						vec[1],
						vec[2]); //Lerp?
			}
		}
	}

	// Read texture coords so that we have them for our triangles
	auto texCoordsList = new md2filter_TexCoordT[numTexCoords];
	src->seek(offsetTexCoords);

	for(i = 0; i<numTexCoords; i++)
	{
		int16_t s;
		int16_t t;
		src->read(s);
		src->read(t);
		texCoordsList[i].s = (float)s/skinWidth;
		texCoordsList[i].t = 1.0-(float)t/skinHeight;
	}

	// Create group for all triangles
	model->addGroup("Group");

	// Now read triangles
	src->seek(offsetTriangles);

	for(i = 0; i<numTriangles; i++)
	{
		uint16_t vertexIndices[3];
		uint16_t textureIndices[3];

		for(t = 0; t<3; t++)
			src->read(vertexIndices[t]);

		model->addTriangle(vertexIndices[0],vertexIndices[1],vertexIndices[2]);
		model->addTriangleToGroup(0,i);

		for(t = 0; t<3; t++)
		{
			src->read(textureIndices[t]);
			model->setTextureCoords(i,t,
					texCoordsList[textureIndices[t]].s,
					texCoordsList[textureIndices[t]].t);
		}
	}

	// Now read skins
	src->seek(offsetSkins);

	TextureManager *texmgr = TextureManager::getInstance();
	bool haveSkin = false;

	//std::vector<Model::Material*> &modelMaterials = getMaterialList(model);
	auto &modelMaterials = *(Model::_MaterialList*)&model->getMaterialList();

	log_debug("Skin data:\n");
	for(i = 0; i<numSkins; i++)
	{
		src->readBytes(name,64);
		name[63] = '\0';
		log_debug("Skin %d: %s\n",i,name);

		std::string tempStr = name;
		replaceBackslash(tempStr);

		char tempPath[64];
		if(!model->getMetaData("MD2_PATH",tempPath,sizeof(tempPath)))
		{
			std::string md2path = tempStr;
			size_t slashChar = md2path.rfind('/');
			log_debug("The '/' char is at %d\n",slashChar);
			if(slashChar<md2path.size())
			{
				md2path.resize(slashChar+1);
				log_debug("setting MD2 path to %s\n",md2path.c_str());
				model->addMetaData("MD2_PATH",md2path.c_str());
			}
		}

		std::string fullName;
		std::string fullPath;
		std::string baseName;

		normalizePath(tempStr.c_str(),fullName,fullPath,baseName);

		log_debug("base model skin name is %s\n",baseName.c_str());

		std::string skin = modelPath+baseName;
		log_debug("full model skin path is %s\n",skin.c_str());

		Model::Material *mat = Model::Material::get();

		mat->m_name = "skin";

		for(int m = 0; m<3; m++)
		{
			mat->m_ambient[m] = 0.2;
			mat->m_diffuse[m] = 0.8;
			mat->m_specular[m] = 0.0;
			mat->m_emissive[m] = 0.0;
		}
		mat->m_ambient[3]  = 1.0;
		mat->m_diffuse[3]  = 1.0;
		mat->m_specular[3] = 1.0;
		mat->m_emissive[3] = 1.0;

		mat->m_shininess = 0.0;

		//replaceBackslash(material->m_texture);
		//replaceBackslash(material->m_alphamap);

		// Get absolute path for texture
		std::string texturePath = skin;

		texturePath = fixAbsolutePath(modelPath.c_str(),texturePath.c_str());
		texturePath = getAbsolutePath(modelPath.c_str(),texturePath.c_str());

		mat->m_filename  = texturePath;

		mat->m_alphaFilename = "";

		if(texmgr->getTexture(texturePath.c_str()))
		{
			log_debug("skin %d: '%s'\n",i,texturePath.c_str());
			haveSkin = true;
			modelMaterials.push_back(mat);
		}
		else
		{
			mat->release();
		}
	}

	// If we don't have any skins,lets try to find some
	if(modelMaterials.size()==0)
	{
		char *noext = strdup(modelBaseName.c_str());

		char *ext = strrchr(noext,'.');
		if(ext)
		{
			ext[0] = '\0';
		}

		log_debug("no skins defined,looking for some....\n");
		std::list<std::string> files;
		getFileList(files,modelPath.c_str(),noext);
		std::list<std::string>::iterator it;

		free(noext);

		for(it = files.begin(); it!=files.end(); it++)
		{
			std::string texturePath = modelPath+(*it);

			texturePath = getAbsolutePath(modelPath.c_str(),texturePath.c_str());

			log_debug("checking %s\n",texturePath.c_str());
			if(texmgr->getTexture(texturePath.c_str()))
			{
				log_debug("  %s is a skin\n",texturePath.c_str());
				haveSkin = true;
				Model::Material *mat = Model::Material::get();

				replaceBackslash(texturePath);

				mat->m_name = getFileNameFromPath(texturePath.c_str());

				for(int m = 0; m<3; m++)
				{
					mat->m_ambient[m] = 0.2;
					mat->m_diffuse[m] = 0.8;
					mat->m_specular[m] = 0.0;
					mat->m_emissive[m] = 0.0;
				}
				mat->m_ambient[3]  = 1.0;
				mat->m_diffuse[3]  = 1.0;
				mat->m_specular[3] = 1.0;
				mat->m_emissive[3] = 1.0;

				mat->m_shininess = 0.0;

				//replaceBackslash(material->m_texture);
				//replaceBackslash(material->m_alphamap);

				// Get absolute path for texture

				mat->m_filename  = texturePath;

				mat->m_alphaFilename = "";

				modelMaterials.push_back(mat);
			}
		}
	}

	if(haveSkin)
	{
		model->setGroupTextureId(0,0);
	}

	// Now read strips/fans
	src->seek(offsetGlCommands);

	unsigned numStrips = 0;
	unsigned numFans	= 0;
	unsigned tcount	 = 0;

	for(i = 0; i<numGlCommands; i++)
	{
		int32_t  numVertices = 0;
		bool	  isStrip = true;

		uint32_t  vertexIndex = 0;
		float u = 0.0;
		float v = 0.0;

		src->read(numVertices);

		if(numVertices!=0)
		{
			if(numVertices<0)
			{
				numVertices = -numVertices;
				isStrip = false;

				numFans++;
			}
			else
			{
				numStrips++;
			}

			tcount += numVertices-2;

			for(t = 0; t<(unsigned)numVertices; t++)
			{
				src->read(u);
				src->read(v);
				src->read(vertexIndex);
			}
		}
		else
		{
			break;
		}
	}

	log_debug("strip count: %d\n",numStrips);
	log_debug("fan count:	%d\n",numFans  );
	log_debug("tri count:	%d\n",tcount	);

	delete[] texCoordsList;

	md2filter_invertModelNormals(model);

	return Model::ERROR_NONE;
}

// FIXME test this! //???
Model::ModelErrorE Md2Filter::writeFile(Model *model, const char *const filename, Options&)
{
	{
		int groupTexture = -1;
		int gcount = model->getGroupCount();
		for(int g = 0; g<gcount; g++)
		{
			int tex = model->getGroupTextureId(g);
			if(tex>=0)
			{
				if(groupTexture>=0&&groupTexture!=tex){
					model->setFilterSpecificError(TRANSLATE("LowLevel","MD2 requires all groups to have the same material."));
					return Model::ERROR_FILTER_SPECIFIC;
				}
				groupTexture = tex;
			}
		}
	}

	{
		unsigned tcount = model->getTriangleCount();
		for(unsigned t = 0; t<tcount; ++t)
		{
			if(model->getTriangleGroup(t)<0)
			{
				model->setFilterSpecificError(TRANSLATE("LowLevel","MD2 export requires all faces to be grouped."));
				return Model::ERROR_FILTER_SPECIFIC;
			}
		}
	}

	md2filter_invertModelNormals(model);

	Model::ModelErrorE err = Model::ERROR_NONE;
	DataDest *dst = openOutput(filename,err);
	DestCloser fc(dst);

	if(err!=Model::ERROR_NONE)
		return err;

	std::string modelPath = "";
	std::string modelBaseName = "";
	std::string modelFullName = "";

	normalizePath(filename,modelFullName,modelPath,modelBaseName);
		
	auto &modelVertices = model->getVertexList();
	auto &modelTriangles = model->getTriangleList();
	//std::vector<Model::Material*> &modelMaterials = getMaterialList(model);
	auto &modelMaterials = *(Model::_MaterialList*)&model->getMaterialList();

	unsigned numStrips = 0;
	unsigned numFans	= 0;
	unsigned tcount	 = 0;

	TriPrim *tp = TriPrim::newTriPrim();

	std::vector<TriPrim::TriangleVertexList> stripList;
	std::vector<TriPrim::TriangleVertexList> fanList;
	log_debug("finding triangle primitives\n");
	if(tp->findPrimitives(model,true))
	{
		TriPrim::TriangleVertexList tvl;

		TriPrim::TriPrimTypeE tpt;
		while((tpt = tp->getNextPrimitive(tvl))!=TriPrim::TRI_PRIM_NONE)
		{
			//log_debug("got triangle %s with %d triangles\n",tpt==TriPrim::TRI_PRIM_STRIP ? "strip" : "fan",tvl.size());
			tcount += tvl.size()-2;

			if(tpt==TriPrim::TRI_PRIM_STRIP)
			{
				numStrips++;
				stripList.push_back(tvl);
			}
			else
			{
				numFans++;
				fanList.push_back(tvl);
			}
		}

		numStrips = stripList.size();
		numFans	= fanList.size();
	}
	delete tp;
	tp = nullptr;

	log_debug("strip count: %d\n",numStrips);
	log_debug("fan count:	%d\n",numFans  );
	log_debug("tri count:	%d\n",tcount	);

	int32_t magic = 0x32504449;
	int32_t version = 8;
	int32_t skinWidth = 256;
	int32_t skinHeight = 256;
	int32_t frameSize = 0;
	int32_t numSkins = modelMaterials.size();
	int32_t numVertices = modelVertices.size();
	int32_t numTexCoords = modelTriangles.size()*3;
	int32_t numTriangles = modelTriangles.size();
	int32_t numGlCommands = (numTriangles+numStrips *2+numFans *2)*3 
		+numStrips+numFans+1;
	int32_t numFrames = 0;
	int32_t offsetSkins = 68;
	int32_t offsetTexCoords = 68;
	int32_t offsetTriangles = 0;
	int32_t offsetFrames = 0;
	int32_t offsetGlCommands = 0;
	int32_t offsetEnd = 0;

	// Use the load matrix and then invert it
	Matrix saveMatrix;
	saveMatrix.setRotationInDegrees(-90,-90,0);
	saveMatrix = saveMatrix.getInverse();

	bool noAnim = false;

	frameSize = (4 *numVertices)+40;

	unsigned i,animCount = 
	model->getAnimationCount(Model::ANIMMODE_FRAME);
	for(i=0;i<animCount;i++)
	if(model->getAnimType(i)&Model::ANIMMODE_FRAME)
	{
		numFrames+=model->getAnimFrameCount(i);
	}
	if(numFrames==0)
	{
		numFrames = 1;
		noAnim = true;
	}
	//if(animCount==0)
	{
	//	animCount = 1;
	}

	if(!modelMaterials.empty()&&modelMaterials[0]->m_textureData)
	{
		skinWidth  = modelMaterials[0]->m_textureData->m_width;
		skinHeight = modelMaterials[0]->m_textureData->m_height;
	}

	// Write header
	dst->write(magic);
	dst->write(version);
	dst->write(skinWidth);
	dst->write(skinHeight);
	dst->write(frameSize);
	dst->write(numSkins);
	dst->write(numVertices);
	dst->write(numTexCoords);
	dst->write(numTriangles);
	dst->write(numGlCommands);
	dst->write(numFrames);
	dst->write(offsetSkins);
	dst->write(offsetTexCoords);
	dst->write(offsetTriangles);
	dst->write(offsetFrames);
	dst->write(offsetGlCommands);
	dst->write(offsetEnd);


	// Write skins
	offsetSkins = dst->offset();
	for(i = 0; i<(unsigned)numSkins; i++)
	{
		int8_t skinpath[64]; // must be 64 bytes for skin writing to work

		std::string fullName,fullPath,baseName;
		normalizePath(modelMaterials[i]->m_filename.c_str(),fullName,fullPath,baseName);

		char *noext = strdup(modelBaseName.c_str());

		char *ext = strrchr(noext,'.');
		if(ext)
		{
			ext[0] = '\0';
		}

		char md2path[64];
		if(model->getMetaData("MD2_PATH",md2path,sizeof(md2path)))
		{
			snprintf((char *)skinpath,sizeof(skinpath),"%s%s",md2path,baseName.c_str());
		}
		else
		{
			// Assume player model
			snprintf((char *)skinpath,sizeof(skinpath),"players/%s/%s",noext,baseName.c_str());
		}
		log_debug("writing skin %s as %s\n",baseName.c_str(),skinpath);

		dst->writeBytes(skinpath,sizeof(skinpath));

		free(noext);
	}

	// Write texture coordinates
	// TODO might be able to optimize by finding shared texture coordinates
	offsetTexCoords = dst->offset();

	for(i = 0; i<(unsigned)numTriangles; i++)
	{
		for(unsigned n = 0; n<3; n++)
		{
			float	sd;
			float	td;
			uint16_t si;
			uint16_t ti;
			model->getTextureCoords(i,n,sd,td);

			td = 1.0-td;

			si = (int16_t)(sd *(double)skinWidth);
			dst->write(si);
			ti = (int16_t)(td *(double)skinHeight);
			dst->write(ti);
		}
	}

	// Write Triangles
	offsetTriangles = dst->offset();
	for(i = 0; i<(unsigned)numTriangles; i++)
	{
		for(unsigned v = 0; v<3; v++)
		{
			uint16_t vertex = (uint16_t)model->getTriangleVertex(i,v);
			dst->write(vertex);
		}
		for(unsigned v = 0; v<3; v++)
		{
			uint16_t texindex = (uint16_t)i *3+v;
			dst->write(texindex);
		}
	}

	// Write Frames
	offsetFrames = dst->offset();

	https://github.com/zturtleman/mm3d/issues/109
	std::vector<float> vecNormals(3*numVertices);
	float *avgNormals = vecNormals.data();

	unsigned anim = noAnim?animCount:0;
	if(noAnim) goto noAnim; //2021
	for(;anim<animCount;anim++)
	if(model->getAnimType(anim)&Model::ANIMMODE_FRAME) noAnim:
	{
		// Heh... oops
		//model->calculateFrameNormals(anim);

		std::string animname = noAnim ? "Frame" : model->getAnimName(anim);

		// Check for animation name ending with a number
		size_t len = strlen(animname.c_str());
		log_debug("last char of animname is '%c'\n",animname[len-1]);
		if(len>0&&isdigit(animname[len-1]))
		{
			log_debug("appending underscore\n");
			// last char is number,append an underscore as a separator
			animname += '_';
		}

		if(!noAnim) //2020: Generate normals.
		{
			model->setCurrentAnimation(anim);
		}
		
		unsigned aFrameCount = model->getAnimFrameCount(anim);
		if(noAnim||0==aFrameCount)
		{
			aFrameCount = 1;
		}

		for(unsigned i=0;i<aFrameCount;i++)
		{
			if(!noAnim) //2020: Generate normals.
			{
				model->setCurrentAnimationFrame(i);
			}
			
			memset(avgNormals,0x00,sizeof(float)*3*numVertices);
			for(unsigned i=model->getTriangleCount();i-->0;)
			{
				for(unsigned j=0;j<3;j++)
				{
					double n[3];
					model->getNormal(i,j,n);
					int avg = 3*model->getTriangleVertex(i,j);

					for(int k=0;k<3;k++)
					avgNormals[avg+k]+=n[k];
				}
			}			

			double min[3] = {+DBL_MAX,+DBL_MAX,+DBL_MAX};
			double max[3] = {-DBL_MAX,-DBL_MAX,-DBL_MAX};
			
			for(int32_t v=0;v<numVertices;v++)
			{
				double coord[3]; //double x,y,z;
				//model->getFrameAnimVertexCoords(anim,i,v,x,y,z);
				model->getVertexCoords(v,coord);

				for(int i=3;i-->0;)
				{
					min[i] = std::min(min[i],coord[i]);
					max[i] = std::max(max[i],coord[i]);
				}
			}

			float scale[3] = {};
			float translate[3] = {};
			if(numVertices) for(int i=3;i-->0;)
			{
				scale[i] = (float)((max[i]-min[i])/255);
				//translate[i] = (float)(0-(-min[i])); //???
				translate[i] = (float)min[i];
			}

			saveMatrix.apply3(scale);
			saveMatrix.apply3(translate);

			dst->write(scale[0]);
			dst->write(scale[1]);
			dst->write(scale[2]);
			dst->write(translate[0]);
			dst->write(translate[1]);
			dst->write(translate[2]);


			char namestr[16];
			snprintf(namestr,16,"%s%03d",animname.c_str(),i);
			namestr[15] = '\0';
			dst->writeBytes(namestr,sizeof(namestr));

			//double vertNormal[3] = {0,0,0};
			for(int32_t v=0;v<numVertices;v++)
			{
				double vec[3] = {0,0,0};
				//model->getFrameAnimVertexCoords(anim,i,v,vec[0],vec[1],vec[2]);
				model->getVertexCoords(v,vec);
				saveMatrix.apply3(vec);
				uint8_t xi = (uint8_t)((vec[0]-translate[0])/scale[0]+0.5);
				uint8_t yi = (uint8_t)((vec[1]-translate[1])/scale[1]+0.5);
				uint8_t zi = (uint8_t)((vec[2]-translate[2])/scale[2]+0.5);
				uint8_t normal = 0;

				//https://github.com/zturtleman/mm3d/issues/109
				//model->getFrameAnimVertexNormal(anim,i,v,vertNormal[0],vertNormal[1],vertNormal[2]);
				float *vertNormal = avgNormals+v*3; normalize3(vertNormal);

				saveMatrix.apply3(vertNormal);

				// Have to invert normal
				vertNormal[0] = -vertNormal[0];
				vertNormal[1] = -vertNormal[1];
				vertNormal[2] = -vertNormal[2];

				normal = bestNormal(vertNormal);

				dst->write(xi);
				dst->write(yi);
				dst->write(zi);
				dst->write(normal);
			}
		}
	}

	// Write GL Commands
	offsetGlCommands = dst->offset();

	if(numTriangles>0)
	{
		if(numStrips>0||numFans>0)
		{
			unsigned t = 0;
			for(t = 0; t<numStrips; t++)
			{
				int32_t  count = stripList[t].size();
				dst->write(count);

				for(unsigned i = 0; i<(unsigned)count; i++)
				{
					uint32_t v1 = stripList[t][i].vert;
					float	 s1 = stripList[t][i].s;
					float	 t1 = 1.0f-stripList[t][i].t;

					dst->write(s1);
					dst->write(t1);
					dst->write(v1);
				}
			}

			for(t = 0; t<numFans; t++)
			{
				int32_t  count = fanList[t].size();

				dst->write((int32_t)-count);

				for(unsigned i = 0; i<(unsigned)count; i++)
				{
					uint32_t v1 = fanList[t][i].vert;
					float	 s1 = fanList[t][i].s;
					float	 t1 = 1.0f-fanList[t][i].t;

					dst->write(s1);
					dst->write(t1);
					dst->write(v1);
				}
			}
		}
	}

	// Write GL command end marker
	int32_t end = 0;
	dst->write(end);

	// Get file length
	offsetEnd = dst->offset();

	// Re-write header
	dst->seek(0);
	dst->write(magic);
	dst->write(version);
	dst->write(skinWidth);
	dst->write(skinHeight);
	dst->write(frameSize);
	dst->write(numSkins);
	dst->write(numVertices);
	dst->write(numTexCoords);
	dst->write(numTriangles);
	dst->write(numGlCommands);
	dst->write(numFrames);
	dst->write(offsetSkins);
	dst->write(offsetTexCoords);
	dst->write(offsetTriangles);
	dst->write(offsetFrames);
	dst->write(offsetGlCommands);
	dst->write(offsetEnd);

	md2filter_invertModelNormals(model);
	model->operationComplete("Invert normals for save");

	return Model::ERROR_NONE;
}

int Md2Filter::addNeededAnimFrame(Model *model, const char *name)
{
	char *temp = strdup(name);

	int t = strlen(temp)-1;
	for(; t>0&&isdigit(temp[t]); t--)
	{
		temp[t] = '\0';
	}
	if(t>0&&temp[t]=='_'){
		temp[t] = '\0';
	}

	if(m_lastAnimIndex==-1||PORT_strcasecmp(temp,m_lastAnimFrame.c_str())!=0)
	{
		m_lastAnimFrame = temp;
		m_lastAnimIndex = model->addAnimation(Model::ANIMMODE_FRAME,temp);
		model->setAnimFPS(m_lastAnimIndex,10.0);
		m_animFrame = 0;
	}
	else
	{
		m_animFrame++;
	}
	model->setAnimFrameCount(m_lastAnimIndex,m_animFrame+1);

	free(temp); return m_lastAnimIndex;
}

extern ModelFilter *md2filter(ModelFilter::PromptF f)
{
	auto o = new Md2Filter; o->setOptionsPrompt(f); return o;
}
