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

#include "model.h"
#include "filedatadest.h"
#include "filedatasource.h"
#include "texture.h"
#include "log.h"
#include "misc.h"
#include "mm3dport.h"
#include "msg.h"
#include "translate.h"
#include "file_closer.h"

namespace 
{
	//NOTE: Misfit Model 3D doesn't check the version number
	//but it does check this magic-number. 
	static const char MISFIT3D[] = "MISFIT3D";	
	static const char MM3D2020[] = "MM3D2020";

	// Misfit MM3D format:
	//
	// File Header
	//
	// MAGIC_NUMBER	 8 bytes  "MISFIT3D"
	// MAJOR_VERSION	uint8	 0x01
	// MINOR_VERSION	uint8	 0x05
	// MODEL_FLAGS	  uint8	 0x00 (reserved)
	// OFFSET_COUNT	 uint8	 [offc]
	//
	//	 [OFFSET_COUNT] instances of:
	//	 Offset header list
	//	 OFFSET_HEADER	6 bytes *[offc]
	//
	//	 Offset Header
	//	 OFFSET_TYPE	  uint16 (highest bit 0 = Data type A,1 = data type B)
	//	 OFFSET_VALUE	 uint32 
	//
	// Data header A (Variable data size)
	// DATA_FLAGS		uint16 
	// DATA_COUNT		uint32 
	//
	// Data block A
	//
	// [DATA_COUNT] instances of:
	// DATA_SIZE		 uint32 bytes
	// DATA_BLOCK_A	 [DATA_SIZE] bytes
	//
	// Data header B (Uniform data size)
	// DATA_FLAGS		uint16 
	// DATA_COUNT		uint32 
	// DATA_SIZE		 uint32 
	//
	// Data block B
	//
	// [DATA_COUNT] instances of:
	// DATA_BLOCK_B	[DATA_SIZE] bytes
	//
	//
	// Offset  Types (A is Variable, B is Uniform)
	//	0x0001	U		Vertices
	//	0x0021	U		Triangles
	//	0x0026	U		Triangle Normals
	//	0x0041	U		Joints
	//	0x0046	U		Joint Vertices (LEGACY)
	//	0x0061	U		Points
	//	0x0101	 V		 Groups
	//	0x0106	U		Smooth Angles (STOP USING ME)
	//	0x0121	U		Texture Coordinates
	//	0x0141	 V		 Embedded textures (UNIMPLEMENTED)
	//	0x0142	 V		 External textures
	//  0x0146	U	    Weighted Influences
	//	0x0161	 V		 Materials
	//	0x0168	U		Texture Projections
	//	0x016c	 V		 Texture Projection Triangles
	//	0x0180	 V		 Scale Factors (2021)
	//	0x0191	 V		 Canvas background images
	//	0x0301	 V		 Skeletal Animations (LEGACY)
	//	0x0321	 V		 Frame Animations (LEGACY)
	//	0x0326	 V		 Frame Animation Points (LEGACY)
	//	0x0341	 V		 Frame Relative Animations (UNIMPLEMENTED)
	//	0x0342	 V		 Animations (2021)
	//	0x1001	 V		 Meta information
	//	0x1002	 V		 Unknown type information
	// 
	// Meta information data
	//	KEY				 ASCIIZ <=1024 (may not be empty)
	//	VALUE			  ASCIIZ <=1024 (may be empty)
	//
	// Unknown type information
	//	OFFSET_TYPE	  uint16
	//	INFO_STRING	  ASCIIZ<=256 (inform user how to support this data)
	//
	//
	// Vertex data
	//	FLAGS			  uint16
	//	X_COORD			float32
	//	Y_COORD			float32
	//	Z_COORD			float32
	//
	//
	// Triangle data
	//	FLAGS			  uint16
	//	VERTEX1_INDEX	uint32
	//	VERTEX2_INDEX	uint32
	//	VERTEX3_INDEX	uint32
	//
	// Group data
	//	FLAGS			  uint16
	//	NAME				ASCIIZ<=inf
	//	TRI_COUNT		 uint32
	//	TRI_INDICES	  uint32 *[TRI_COUNT]
	//	SMOOTHNESS		uint8
	//	MATERIAL_INDEX  uint32
	//	
	// Smooth Angles (maximum angle between smoothed normals for a group)
	//	GROUP_INDEX	  uint32
	//	SMOOTHNESS		uint8
	//	
	// Weighted Influences
	//	POS_TYPE		  uint8
	//	POS_INDEX		 uint32
	//	INF_INDEX		 uint32
	//	INF_TYPE		  uint8
	//	INF_WEIGHT		uint8
	//	
	// External texture data
	//	FLAGS			  uint16
	//	FILENAME		  ASCIIZ
	//
	// Embedded texture data
	//	FLAGS			  uint16
	//	FORMAT			 char8 *4  ('JPEG','PNG ','TGA ',etc...)
	//	TEXTURE_SIZE	 uint32
	//	DATA				uint8 *[TEXTURE_SIZE]
	//
	// Material
	//	FLAGS			  uint16
	//	TEXTURE_INDEX	uint32
	//	NAME				ASCIIZ
	//	AMBIENT			float32 *4
	//	DIFFUSE			float32 *4
	//	SPECULAR		  float32 *4
	//	EMISSIVE		  float32 *4
	//	SHININESS		 float32
	//	
	// Projection Triangles
	//	PROJECTION_INDEX  uint32
	//	TRI_COUNT			uint32
	//	TRI_INDICES		 uint32 *[TRI_COUNT]
	//	
	// Canvas background image
	//	FLAGS			  uint16
	//	VIEW_INDEX		uint8
	//	SCALE			  float32
	//	CENTER_X		  float32
	//	CENTER_X		  float32
	//	CENTER_X		  float32
	//	FILENAME		  ASCIIZ
	//
	// Texture coordinates
	//	FLAGS			  uint16
	//	TRI_INDEX		 uint32
	//	COORD_S			float32 *3
	//	COORD_T			float32 *3
	//
	// Joint data
	//	FLAGS			  uint16
	//	NAME				char8 *40
	//	PARENT_INDEX	 int32
	//	LOCAL_ROT		 float32 *3
	//	LOCAL_TRANS	  float32 *3
	//
	// Joint Vertices
	//	VERTEX_INDEX	 uint32
	//	JOINT_INDEX	  int32
	//
	// Point data
	//	FLAGS			  uint16
	//	NAME				char8 *40
	//	TYPE				int32
	//	BONE_INDEX		int32
	//	ROTATION		  float32 *3
	//	TRANSLATION	  float32 *3
	//
	// Texture Projection 
	//	FLAGS			  uint16
	//	NAME				char8 *40
	//	TYPE				int32
	//	POSITION		  float32 *3
	//	UP VECTOR		 float32 *3
	//	SEAM VECTOR	  float32 *3
	//	U MIN			  float32
	//	V MIN			  float32
	//	U MAX			  float32
	//	V MAX			  float32
	//
	// Skeletal animation
	//	FLAGS			  uint16
	//	NAME				ASCIIZ
	//	FPS				 float32
	//	FRAME_COUNT	  uint32
	//
	//		[FRAME_COUNT] instances of:
	//		KEYFRAME_COUNT  uint32
	//
	//		  [KEYFRAME_COUNT] instances of:
	//		  JOINT_INDEX	  uint32
	//		  KEYFRAME_TYPE	uint8  (0 = rotation,1 = translation)
	//		  PARAM			  float32 *3
	//
	// Frame animation
	//	FLAGS			  uint16
	//	NAME				ASCIIZ<=64
	//	FPS				 float32
	//	FRAME_COUNT	  uint32
	//
	//		[FRAME_COUNT] instances of:
	//		
	//			[VERTEX_COUNT] instances of:
	//			COORD_X			float32
	//			COORD_Y			float32
	//			COORD_Z			float32
	// 
	// Frame animation points
	//	FLAGS				 uint16
	//	FRAME_ANIM_INDEX  uint32
	//	FRAME_COUNT		 uint32
	//
	//		[FRAME_COUNT] instances of:
	//		
	//			[POINT_COUNT] instances of:
	//			ROT_X			float32
	//			ROT_Y			float32
	//			ROT_Z			float32
	//			TRANS_X		 float32
	//			TRANS_Y		 float32
	//			TRANS_Z		 float32
	// 
	// Frame relative animation
	//	FLAGS			  uint16
	//	NAME				ASCIIZ<=64
	//	FPS				 float32
	//	FRAME_COUNT	  uint32
	//
	//		[FRAME_COUNT] instances of:
	//		FVERT_COUNT	  uint32
	//		
	//			[FVERT_COUNT] instances of:
	//			VERTEX_INDEX
	//			COORD_X_OFFSET  float32
	//			COORD_Y_OFFSET  float32
	//			COORD_Z_OFFSET  float32
	// 


	enum
	{
		OFFSET_TYPE_MASK=0x3fff, 
		OFFSET_UNI_MASK=0x8000, 
		OFFSET_DIRTY_MASK=0x4000 
	};

	struct MisfitOffsetT
	{
		uint16_t offsetType;
		uint32_t offsetValue;

		bool uniform(){ return 0!=(offsetType&OFFSET_UNI_MASK); }
		bool variable(){ return 0==(offsetType&OFFSET_UNI_MASK); }
	};
	typedef std::vector<MisfitOffsetT> MisfitOffsetList;

	enum MisfitDataTypesE
	{
		MDT_Vertices,
		MDT_Triangles,
		MDT_TriangleNormals, //IGNORED
		MDT_Joints,
		MDT_JointVertices, //LEGACY
		MDT_Points,
		MDT_Groups,
		MDT_SmoothAngles, //???
		MDT_TexCoords,
		//MDT_EmbTextures, //UNIMPLEMENTED
		MDT_ExtTextures,
		MDT_WeightedInfluences,
		MDT_Materials,
		MDT_TexProjections,
		MDT_ProjectionTriangles,
		MDT_ScaleFactors, //2021
		MDT_CanvasBackgrounds,
		MDT_SkelAnims,
		MDT_FrameAnims,
		MDT_FrameAnimPoints,
		//MDT_RelativeAnims, //UNIMPLEMENTED
		MDT_Animations, //2021
		MDT_Meta,
		MDT_TypeInfo,		
		MDT_EndOfFile, // End of list
		MDT_MAX
	};
	static struct
	{	
		size_t f;
		const char *s; 
	}MisfitOffsetTypes[MDT_MAX] = 
	{
	{	0x0001, /*U*/ "Vertices" }, 
	{	0x0021, /*U*/ "Triangles" }, 
	{	0x0026, /*U*/ "Triangle Normals" }, 
	{	0x0041, /*U*/ "Joints" }, 
	{	0x0046, /*U*/ "Joint Vertices" }, //LEGACY
	{	0x0061, /*U*/ "Points" }, 
	{	0x0101,       "Groups" }, 
	{	0x0106, /*U*/ "Smooth Angles" }, 
	{	0x0121, /*U*/ "Texture Coordinates" }, 
//	{	0x0141,       "Embedded Textures" }, //UNIMPLEMENTED
	{	0x0142,       "External Textures" }, 
	{	0x0146, /*U*/ "Weighted Influences" }, 
	{	0x0161,       "Materials" }, 
	{	0x0168, /*U*/ "Texture Projections" }, 
	{	0x016c,       "Texture Projection Triangles" }, 
	{	0x0180, /*U*/ "Scale Factors" }, //2021
	{	0x0191,       "Canvas Background Images" }, 
	{	0x0301,       "Skeletal Animations" }, //LEGACY
	{	0x0321,       "Frame Animations" },  //LEGACY
	{	0x0326,       "Frame Animation Points" },  //LEGACY
//	{	0x0341,       "Frame Relative Animations" }, //UNIMPLEMENTED
	{	0x0342,       "Animations" }, //2021
	{	0x1001,       "Meta Information" },
	{	0x1002,       "Type Identity" },
	{	0x3fff,       "EOF" }, //"End of File"
	};

	enum MisfitFlagsE
	{
		MF_HIDDEN	 = 1, // powers of 2
		MF_SELECTED  = 2,
		MF_VERTFREE  = 4, // vertex does not have to be conntected to a face

		// Type-specific flags
		MF_MAT_CLAMP_S = 16,
		MF_MAT_CLAMP_T = 32,
	};

	enum MisfitFrameAnimFlagsE
	{
		MFAF_ANIM_WRAP = 0x0001,
	};

	enum MisfitSkelAnimFlagsE
	{
		MSAF_ANIM_WRAP = 0x0001,
	};
	
	enum MM3DFILE_AnimFlagsE //2021
	{
		MAF_ANIM_TYPE = 0x0007,
		MAF_ANIM_WRAP = 0x0008,
	};

	// File header
	struct MM3DFILE_HeaderT
	{
		char magic[8];
		uint8_t versionMajor;
		uint8_t versionMinor;
		uint8_t modelFlags; //UNUSED
		uint8_t offsetCount;
	};

	// Data header A (Variable data size)
	struct MM3DFILE_DataHeaderAT
	{
		uint16_t flags;
		uint32_t count;
	};

	// Data header B (Uniform data size)
	struct MM3DFILE_DataHeaderBT
	{
		uint16_t flags;
		uint32_t count;
		uint32_t size;
	};

	struct MM3DFILE_VertexT
	{
		uint16_t  flags;
		float32_t coord[3];
	};

	const size_t FILE_VERTEX_SIZE = 14;

	struct MM3DFILE_TriangleT
	{
		uint16_t  flags;
		uint32_t  vertex[3];
	};

	const size_t FILE_TRIANGLE_SIZE = 14;

	struct MM3DFILE_TriangleNormalsT
	{
		uint16_t	flags;
		uint32_t	index;
		float32_t  normal[3][3];
	};

	const size_t FILE_TRIANGLE_NORMAL_SIZE = 42;

	struct MM3DFILE_JointT
	{
		uint16_t  flags;
		char		name[40];
		int32_t	parentIndex;
		float32_t localRot[3];
		float32_t localTrans[3];
		float32_t localScale[3]; //mm3d2020 only (REMOVE ME)
	};

	const size_t FILE_JOINT_SIZE = 70; //70+12;

	struct MM3DFILE_JointVertexT
	{
		uint32_t  vertexIndex;
		int32_t	jointIndex;
	};

	const size_t FILE_JOINT_VERTEX_SIZE = 8;

	struct MM3DFILE_WeightedInfluenceT
	{
		uint8_t	posType;
		uint32_t  posIndex;
		uint32_t  infIndex;
		uint8_t	infType;
		int8_t	 infWeight;
	};

	const size_t FILE_WEIGHTED_INFLUENCE_SIZE = 11;

	struct MM3DFILE_PointT
	{
		uint16_t  flags;
		char		name[40];
		int32_t	type; //UNUSED
		int32_t	boneIndex; //UNUSED
		float32_t rot[3];
		float32_t trans[3];
		float32_t scale[3]; //mm3d2020 only (REMOVE ME)
	};

	const size_t FILE_POINT_SIZE = 74; //74+12;

	struct MM3DFILE_SmoothAngleT
	{
		uint32_t  groupIndex;
		uint8_t	angle;
	};

	const size_t FILE_SMOOTH_ANGLE_SIZE = 5;

	struct MM3DFILE_CanvasBackgroundT
	{
		uint16_t  flags;
		uint8_t	viewIndex;
		float32_t scale;
		float32_t center[3];
	};

	const size_t FILE_CANVAS_BACKGROUND_SIZE = 19;

	struct MM3DFILE_KeyframeT
	{
		//2020: high-order 16b hold type/interp mode.
		uint32_t objectIndex;
		//0 is Rotate, 1 Translate, 2 Scale
		uint8_t	 keyframeType;
		float32_t  param[3];
	};

	const size_t FILE_KEYFRAME_SIZE = 17;

	struct MM3DFILE_TexCoordT
	{
		uint16_t  flags;
		uint32_t  triangleIndex;
		float32_t sCoord[3];
		float32_t tCoord[3];
	};

	const size_t FILE_TEXCOORD_SIZE = 30;

	const size_t FILE_TEXTURE_PROJECTION_SIZE = 98;

	struct MM3DFILE_ScaleFactorT
	{
		uint16_t type;
		uint16_t index;
		float32_t scale[3];
	};

	const size_t FILE_SCALE_FACTOR_SIZE = 16;

	struct UnknownDataT
	{
		uint16_t offsetType;
		uint32_t offsetValue;
		uint32_t dataLen;
	};

	typedef std::vector<UnknownDataT> UnknownDataList;

	template<int I> struct mm3dfilter_cmp_t //convertAnimToFrame
	{
		float32_t params[3*I]; int diff;

		float32_t *compare(float32_t(&cmp)[3*I], int &prev, int &curr)
		{
			curr = 0;
			for(int i=0;i<I;i++)
			if(memcmp(params+i*3,cmp+i*3,sizeof(*cmp)*3)) 
			curr|=1<<i;
			prev = curr&~diff; diff = curr; return params;
		}
	};
	
	//#include "mm3dfilter.h"
	class MisfitFilter : public ModelFilter
	{
	public:

		//static const uint8_t WRITE_VERSION_MAJOR = 1;
		//static const uint8_t WRITE_VERSION_MINOR = 7;
		static const uint8_t WRITE_VERSION_MAJOR = 2;
		//static const uint8_t WRITE_VERSION_MINOR = 0;
		static const uint8_t WRITE_VERSION_MINOR = 1; //MDT_Animations/MDT_ScaleFactors

		virtual Model::ModelErrorE readFile(Model *model, const char *const filename);
		virtual Model::ModelErrorE writeFile(Model *model, const char *const filename, Options&);

		virtual const char *getReadTypes(){ return "MM3D"; }
		virtual const char *getWriteTypes(){ return "MM3D"; }

	protected:

		DataSource *m_src;
		DataDest *m_dst;
		size_t m_readLength;
		MisfitOffsetList m_offsetList;

		void writeHeaderA(uint16_t flags,uint32_t count)
		{
			m_dst->write(flags); m_dst->write(count);
		}
		void writeHeaderB(uint16_t flags,uint32_t count,uint32_t size)
		{
			m_dst->write(flags); m_dst->write(count);
			m_dst->write(size);
		}

		void addOffset(MisfitDataTypesE type, bool include)
		{
			//if(include) log_debug("adding offset type %04X\n",MisfitOffsetTypes[type]);
			if(include) m_offsetList.push_back({(uint16_t)MisfitOffsetTypes[type].f,0});
		}
		bool setOffset(MisfitDataTypesE type, bool uniform)
		{
			auto cmp = MisfitOffsetTypes[type].f;
			for(auto&ea:m_offsetList)
			if(cmp==(ea.offsetType&OFFSET_TYPE_MASK))
			{
				ea.offsetValue = m_dst->offset();
				//log_debug("updated offset for %04X to %08X\n",ea.offsetType,ea.offsetValue);
				if(uniform)
				{
				//	log_debug("before uniform: %04X\n",ea.offsetType);
					ea.offsetType |= OFFSET_UNI_MASK;
				//	log_debug("after uniform: %04X\n",ea.offsetType);
				}
				else
				{
				//	log_debug("before variable: %04X\n",ea.offsetType);
					ea.offsetType &= OFFSET_TYPE_MASK;
				//	log_debug("after variable: %04X\n",ea.offsetType);
				}
				return true;
			}
			return false;
		}
		MisfitOffsetT *seekOffset(MisfitDataTypesE type)
		{
			auto cmp = MisfitOffsetTypes[type].f;
			for(auto&ea:m_offsetList)
			if(cmp==(ea.offsetType&OFFSET_TYPE_MASK))
			{
				m_src->seek(ea.offsetValue); return &ea;
			}
			return nullptr;
		}
	};

}  // namespace

Model::ModelErrorE MisfitFilter::readFile(Model *model, const char *const filename)
{
	/*if(sizeof(float32_t)!=4)
	{
		msg_error(TRANSLATE("LowLevel","MM3D encountered an unexpected data size problem\nSee Help->About to contact the developers")).c_str());
		return Model::ERROR_FILE_OPEN;
	}*/
	int compile[4==sizeof(float32_t)]; (void)compile;

	Model::ModelErrorE err = Model::ERROR_NONE;
	m_src = openInput(filename,err);
	SourceCloser fc(m_src);
	if(err!=Model::ERROR_NONE)
	return err;

	std::string modelPath = "";
	std::string modelBaseName = "";
	std::string modelFullName = "";
	normalizePath(filename,modelFullName,modelPath,modelBaseName);
	model->setFilename(modelFullName.c_str());

	unsigned fileLength = m_src->getFileSize();

	MM3DFILE_HeaderT fileHeader;
	m_src->readBytes(fileHeader.magic,sizeof(fileHeader.magic));
	m_src->read(fileHeader.versionMajor);
	m_src->read(fileHeader.versionMinor);
	m_src->read(fileHeader.modelFlags); //UNUSED
	m_src->read(fileHeader.offsetCount);

	bool mm3d2020 = !memcmp(fileHeader.magic,MM3D2020,strlen(MISFIT3D));
	if(!mm3d2020)
	if(memcmp(fileHeader.magic,MISFIT3D,strlen(MISFIT3D))) //MISFIT3D?
	{
		log_warning("bad magic number file\n");
		return Model::ERROR_BAD_MAGIC;
	}
	bool mm3d2021 = mm3d2020
	&&(fileHeader.versionMajor>2||fileHeader.versionMinor>=1);

	//FIX ME
	//Assuming can't read if MISFIT3D is newer than 1.7.
	const int major = mm3d2020?WRITE_VERSION_MAJOR:1;
	const int minor = mm3d2020?WRITE_VERSION_MINOR:7;

	if(fileHeader.versionMajor>major)
	{
		return Model::ERROR_UNSUPPORTED_VERSION;
	}
	else if(fileHeader.versionMajor==major) //2020
	{
		if(fileHeader.versionMinor>minor)
		{
			return Model::ERROR_UNSUPPORTED_VERSION;
		}
	}

	unsigned offsetCount = fileHeader.offsetCount;

	log_output("MM3D file:\n");
	log_output("  major:	%d\n",fileHeader.versionMajor);
	log_output("  minor:	%d\n",fileHeader.versionMinor);
	log_debug("  offsets: %d\n",fileHeader.offsetCount);

	log_debug("Offset information:\n");

	//MisfitOffsetList offsetList;
	m_offsetList.clear();

	unsigned lastOffset = 0;
	bool lastUnknown = false;
	UnknownDataList unknownList;

	for(unsigned t = 0; t<offsetCount; t++)
	{
		MisfitOffsetT mo;
		m_src->read(mo.offsetType);
		m_src->read(mo.offsetValue);

		if(mo.offsetValue<lastOffset)
		{
			log_error("Offset are out of order\n");
			return Model::ERROR_BAD_DATA;
		}

		if(lastUnknown)
		{
			unknownList.back().dataLen = mo.offsetValue-lastOffset;
			log_warning("unknown data size = %d\n",unknownList.back().dataLen);
			lastUnknown = false;
		}
		lastOffset = mo.offsetValue;

		m_offsetList.push_back(mo);

		bool found = false;
		for(unsigned e = 0; !found&&e<MDT_MAX; e++)
		if(MisfitOffsetTypes[e].f==(mo.offsetType &OFFSET_TYPE_MASK))
		{
			log_debug("  %08X %s\n",mo.offsetValue,MisfitOffsetTypes[e].s);
			found = true;

			if(e==MDT_EndOfFile)		
			if(mo.offsetValue>fileLength)
			{
				return Model::ERROR_UNEXPECTED_EOF;
			}
			else if(mo.offsetValue<fileLength)
			{
				log_warning("EOF offset and file size do not match (%d and %d)\n",mo.offsetValue,fileLength);
				return Model::ERROR_BAD_DATA;
			}
		}

		if(!found)
		{
			log_debug("  %08X Unknown type %04X\n",mo.offsetValue,mo.offsetType);

			lastUnknown = true;
			UnknownDataT ud;
			ud.offsetType = mo.offsetType;
			ud.offsetValue = mo.offsetValue;
			ud.dataLen = 0;

			unknownList.push_back(ud);
		}
	}

	auto &modelVerts = model->getVertexList();
	auto &modelTriangles = model->getTriangleList();
	auto &modelGroups = model->getGroupList();
	auto &modelMaterials = (Model::_MaterialList&)model->getMaterialList();
	auto &modelJoints = model->getJointList();
	auto &modelPoints = model->getPointList();
	auto &modelAnims = model->getAnimationList();

	// Used to track whether indices are valid
	bool missingElements = false;

	// Meta data
	if(auto*os=seekOffset(MDT_Meta))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned m = 0; m<count; m++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			unsigned len;
			char key[1024];
			char value[1024];

			m_src->readAsciiz(key,sizeof(key));
			utf8chrtrunc(key,sizeof(key)-1);

			len = strlen(key)+1;

			m_src->readAsciiz(value,sizeof(value));
			utf8chrtrunc(value,sizeof(value)-1);

			len = strlen(value)+1;

			model->addMetaData(key,value);
		}
	}

	// Vertices
	if(auto*os=seekOffset(MDT_Vertices))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned v = 0; v<count; v++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_VertexT fileVert;

			m_src->read(fileVert.flags);
			m_src->read(fileVert.coord[0]);
			m_src->read(fileVert.coord[1]);
			m_src->read(fileVert.coord[2]);

			//vert->m_boneId = -1;
			model->addVertex(fileVert.coord[0],fileVert.coord[1],fileVert.coord[2]);			
			if(fileVert.flags&MF_SELECTED) model->selectVertex(v);
			if(fileVert.flags&MF_HIDDEN) model->hideVertex(v);
			//2020: this should be implicit
			//if(fileVert.flags&MF_VERTFREE) model->setVertexFree(v,true);
		}
	}
	unsigned vcount = modelVerts.size(); //2020

	// Triangles
	if(auto*os=seekOffset(MDT_Triangles))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned t = 0; t<count; t++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_TriangleT fileTri;
			m_src->read(fileTri.flags);
			m_src->read(fileTri.vertex[0]);
			m_src->read(fileTri.vertex[1]);
			m_src->read(fileTri.vertex[2]);

			int tri = model->addTriangle(fileTri.vertex[0],fileTri.vertex[1],fileTri.vertex[2]);
			if(MF_SELECTED&fileTri.flags) model->selectTriangle(tri);
			if(MF_HIDDEN&fileTri.flags) model->hideTriangle(tri);
		}
	}

	/*#if 0
	// Triangle Normals
	if(auto*os=seekOffset(MDT_TriangleNormals))
	{
		// Just for debugging... we don't actually use any of this

		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned t = 0; t<count; t++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_TriangleNormalsT fileTri;
			m_src->read(fileTri.flags);
			m_src->read(fileTri.index);
			m_src->read(fileTri.normal[0][0]);
			m_src->read(fileTri.normal[0][1]);
			m_src->read(fileTri.normal[0][2]);
			m_src->read(fileTri.normal[1][0]);
			m_src->read(fileTri.normal[1][1]);
			m_src->read(fileTri.normal[1][2]);
			m_src->read(fileTri.normal[2][0]);
			m_src->read(fileTri.normal[2][1]);
			m_src->read(fileTri.normal[2][2]);

			log_debug("triangle %d normals:\n",fileTri.index);

			for(unsigned v = 0; v<3; v++)
			{
				log_debug("  v %d:  %f %f %f\n",v,
						fileTri.normal[v][0],
						fileTri.normal[v][1],
						fileTri.normal[v][2]);
			}
		}
	}
	#endif // 0*/

	/*https://github.com/zturtleman/mm3d/issues/130)
	// Groups
	if(auto*os=seekOffset(MDT_Groups))*/

	// External Textures
	std::vector<std::string> texNames;
	if(auto*os=seekOffset(MDT_ExtTextures))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned t = 0; t<count; t++)
		{
			log_debug("reading external texture %d/%d\n",t,count);
			if(os->variable())
			{
				m_src->read(size);
			}

			uint16_t flags; //SHADOWING
			char filename[PATH_MAX];

			m_src->read(flags);

			m_src->readAsciiz(filename,sizeof(filename));
			utf8chrtrunc(filename,sizeof(filename)-1);

			replaceBackslash(filename);
			log_debug("  filename is %s\n",filename);

			std::string fullpath = getAbsolutePath(modelPath.c_str(),filename);

			texNames.push_back(fullpath);
		}
	}

	// Materials
	if(auto*os=seekOffset(MDT_Materials))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned m = 0; m<count; m++)
		{
			log_debug("reading material %d/%d\n",m,count);
			if(os->variable())
			{
				m_src->read(size);
			}

			uint16_t flags = 0; //SHADOWING
			uint32_t texIndex = 0;
			char name[1024];

			Model::Material *mat = Model::Material::get();

			m_src->read(flags);
			m_src->read(texIndex);

			m_src->readAsciiz(name,sizeof(name));
			utf8chrtrunc(name,sizeof(name)-1);

			log_debug("  material name: %s\n",name);

			mat->m_name = name;
			switch (flags &0x0f)
			{
			case 0:
				log_debug("  got external texture %d\n",texIndex);
				mat->m_type = Model::Material::MATTYPE_TEXTURE;
				if(texIndex<texNames.size())
				{
					mat->m_filename = texNames[texIndex];
				}
				else
				{
					mat->m_filename = "";
				}
				break;
			case 13: //UNUSED
				mat->m_type = Model::Material::MATTYPE_COLOR;
				mat->m_filename = "";
				memset(mat->m_color,255,sizeof(mat->m_color));
				break;
			case 14: //UNUSED
				mat->m_type = Model::Material::MATTYPE_GRADIENT;
				mat->m_filename = "";
				memset(mat->m_color,255,sizeof(mat->m_color));
				break;
			case 15:
				mat->m_type = Model::Material::MATTYPE_BLANK;
				mat->m_filename = "";
				memset(mat->m_color,255,sizeof(mat->m_color));
				break;
			default:
				log_debug("  got unknown material type\n",texIndex);
				mat->m_type = Model::Material::MATTYPE_BLANK;
				mat->m_filename = "";
				memset(mat->m_color,255,sizeof(mat->m_color));
				break;
			}

			mat->m_sClamp = ((flags &MF_MAT_CLAMP_S)!=0);
			mat->m_tClamp = ((flags &MF_MAT_CLAMP_T)!=0);

			unsigned i = 0;
			float32_t lightProp = 0;
			for(i = 0; i<4; i++)
			{
				m_src->read(lightProp);
				mat->m_ambient[i] = lightProp;
			}
			for(i = 0; i<4; i++)
			{
				m_src->read(lightProp);
				mat->m_diffuse[i] = lightProp;
			}
			for(i = 0; i<4; i++)
			{
				m_src->read(lightProp);
				mat->m_specular[i] = lightProp;
			}
			for(i = 0; i<4; i++)
			{
				m_src->read(lightProp);
				mat->m_emissive[i] = lightProp;
			}
			m_src->read(lightProp);
			mat->m_shininess = lightProp;

			modelMaterials.push_back(mat); //REMOVE ME
		}
	}
	if(auto*os=seekOffset(MDT_Groups))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned g = 0; g<count; g++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}
			
			uint16_t flags; //SHADOWING
			uint32_t triCount;
			uint8_t  smoothness;
			int32_t materialIndex; //uint32_t
			char name[1024];

			m_src->read(flags);

			m_src->readAsciiz(name,sizeof(name));
			utf8chrtrunc(name,sizeof(name)-1);

			m_src->read(triCount);

			model->addGroup(name);
			for(unsigned t=triCount;t-->0;)
			{
				uint32_t triIndex;
				if(m_src->read(triIndex))
				model->addTriangleToGroup(g,triIndex);			
			}

			m_src->read(smoothness);
			m_src->read(materialIndex);

			model->setGroupSmooth(g,smoothness);
			if(flags&MF_SELECTED) model->selectGroup(g);
			//if(flags&MF_HIDDEN) model->hideGroup(g); //???

			if(materialIndex>=0)
			model->setGroupTextureId(g,materialIndex);
		}
	}//REMOVE ME
	// Smooth Angles
	if(auto*os=seekOffset(MDT_SmoothAngles))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned t = 0; t<count; t++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_SmoothAngleT fileSa;
			m_src->read(fileSa.groupIndex);
			m_src->read(fileSa.angle);

			if(fileSa.angle>180)
			{
				fileSa.angle = 180;
			}
			if(fileSa.groupIndex<modelGroups.size())
			{
				model->setGroupAngle(fileSa.groupIndex,fileSa.angle);
			}
		}

		log_debug("read %d group smoothness angles\n",count);
	}

	// Texture coordinates
	if(auto*os=seekOffset(MDT_TexCoords))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned c = 0; c<count; c++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_TexCoordT tc;
			m_src->read(tc.flags);
			m_src->read(tc.triangleIndex);
			m_src->read(tc.sCoord[0]);
			m_src->read(tc.sCoord[1]);
			m_src->read(tc.sCoord[2]);
			m_src->read(tc.tCoord[0]);
			m_src->read(tc.tCoord[1]);
			m_src->read(tc.tCoord[2]);

			for(unsigned v=0;v<3;v++)				
			model->setTextureCoords(tc.triangleIndex,v,tc.sCoord[v],tc.tCoord[v]);			
		}
	}

	// Canvas Background Images
	if(auto*os=seekOffset(MDT_CanvasBackgrounds))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned g = 0; g<count; g++)
		{
			log_debug("reading canvas background %d/%d\n",g,count);
			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_CanvasBackgroundT cb;
			m_src->read(cb.flags);
			m_src->read(cb.viewIndex);
			m_src->read(cb.scale);
			m_src->read(cb.center[0]);
			m_src->read(cb.center[1]);
			m_src->read(cb.center[2]);

			char name[PATH_MAX];

			m_src->readAsciiz(name,sizeof(name));
			utf8chrtrunc(name,sizeof(name)-1);

			replaceBackslash(name);
			std::string fileStr = getAbsolutePath(modelPath.c_str(),name);

			model->setBackgroundImage(cb.viewIndex,fileStr.c_str());
			model->setBackgroundScale(cb.viewIndex,cb.scale);
			model->setBackgroundCenter(cb.viewIndex,
					cb.center[0],cb.center[1],cb.center[2]);
		}
	}

	// Joints
	if(auto*os=seekOffset(MDT_Joints))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		std::vector<int> j2; //2021

		for(unsigned j=0;j<count;j++)
		{
			log_debug("reading joint %d/%d\n",j,count);

			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_JointT fileJoint;
			m_src->read(fileJoint.flags);
			int jointFlags = fileJoint.flags;
			m_src->readBytes(fileJoint.name,sizeof(fileJoint.name));			
			m_src->read(fileJoint.parentIndex);
			m_src->read(fileJoint.localRot[0]);
			m_src->read(fileJoint.localRot[1]);
			m_src->read(fileJoint.localRot[2]);
			m_src->read(fileJoint.localTrans[0]);
			m_src->read(fileJoint.localTrans[1]);
			m_src->read(fileJoint.localTrans[2]);
			if(mm3d2020&&!mm3d2021) //REMOVE ME
			{
				m_src->read(fileJoint.localScale[0]);
				m_src->read(fileJoint.localScale[1]);
				m_src->read(fileJoint.localScale[2]);
			}
			if(auto&jj=fileJoint.parentIndex) if(jj>=(signed)count)
			{
				if(!mm3d2020) //https://github.com/zturtleman/mm3d/issues/136
				{
					log_warning("Joint %d has bad parent joint, checking endianness\n",j);

					// Misfit Model 3D 1.1.7 to 1.1.9 wrote joint parent as 
					// native endian while the rest of the file was little endian.
					m_src->swapEndianness(jj);
				}
				if(jj>=(signed)count)
				{
					missingElements = true;
					
					log_error("Joint %d has missing parent joint %d\n",j,jj);
				}
			}
			fileJoint.name[sizeof(fileJoint.name)-1] = '\0';

			int jn = model->addBoneJoint(fileJoint.name,fileJoint.parentIndex);	

			if(jn==-1) //out of order?
			{
				jn = model->addBoneJoint(fileJoint.name,-1);	

				j2.push_back(jn); j2.push_back(fileJoint.parentIndex);
			}

			//HACK: Previously addBoneJoint was bypassed. 
			auto joint = (Model::Joint*)modelJoints[jn];
			for(int i=3;i-->0;)
			{
				joint->m_rel[i] = fileJoint.localTrans[i]; 
				joint->m_rot[i] = fileJoint.localRot[i]; 
				if(mm3d2020&&!mm3d2021) //REMOVE ME			
				joint->m_xyz[i] = fileJoint.localScale[i];
			}			
			if(jointFlags&MF_SELECTED)
			joint->m_selected = true;
			if(jointFlags&MF_HIDDEN)
			joint->m_visible = false;
			if(mm3d2020)
			if(jointFlags&MF_VERTFREE) //2020 (draw as line?)
			joint->m_bone = false;
		}

		//I messed up by not seeing that joints are reordered
		//however it's probably for the best to do this since
		//it's more flexible. It might be nice to reorder the
		//joints in writeFile, but it will also need to remap
		//all references to joints if so... I think it's best
		//to think of it as an isolated implementation detail.
		for(unsigned i=0;i<j2.size();i+=2)
		model->parentBoneJoint2(j2[i],j2[i+1]);
		if(!j2.empty()) model->validateSkel();

		log_debug("read %d joints\n",count);
	}

	// Points
	if(auto*os=seekOffset(MDT_Points))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned j = 0; j<count; j++)
		{
			log_debug("reading Point %d/%d\n",j,count);
			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_PointT filePoint;
			m_src->read(filePoint.flags);
			int pointFlags = filePoint.flags;
			m_src->readBytes(filePoint.name,sizeof(filePoint.name));
			m_src->read(filePoint.type); //UNUSED
			m_src->read(filePoint.boneIndex); //UNUSED
			m_src->read(filePoint.rot[0]);
			m_src->read(filePoint.rot[1]);
			m_src->read(filePoint.rot[2]);
			m_src->read(filePoint.trans[0]);
			m_src->read(filePoint.trans[1]);
			m_src->read(filePoint.trans[2]);
			if(mm3d2020&&!mm3d2021) //REMOVE ME
			{
				m_src->read(filePoint.scale[0]);
				m_src->read(filePoint.scale[1]);
				m_src->read(filePoint.scale[2]);
			}

			filePoint.name[sizeof(filePoint.name)-1] = '\0';

			float rot[3];
			float trans[3];
			for(unsigned i = 0; i<3; i++)
			{
				rot[i]	= filePoint.rot[i];
				trans[i] = filePoint.trans[i];
			}

			//2020: addPoint ignored this parameter so
			//I removed it.
			//int boneIndex = filePoint.boneIndex;

			int p = model->addPoint(filePoint.name,
					trans[0],trans[1],trans[2],
					rot[0],rot[1],rot[2]/*,boneIndex*/);

			if(mm3d2020&&!mm3d2021) //REMOVE ME
			{
				double s[3];
				for(int i=3;i-->0;)
				s[i] = filePoint.scale[i];
				model->setPointScale(p,s);
			}

			//model->setPointType(p,filePoint.type); //UNUSED
						
			if(pointFlags&MF_SELECTED)
			{
				model->selectPoint(p);
			}
			if(pointFlags&MF_HIDDEN)
			{
				model->hidePoint(p);
			}
		}

		log_debug("read %d points\n");
	}
	unsigned pcount = modelPoints.size(); //2020	

	// Weighted influences
	if(auto*os=seekOffset(MDT_WeightedInfluences))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned t = 0; t<count; t++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			MM3DFILE_WeightedInfluenceT fileWi;
			m_src->read(fileWi.posType);
			m_src->read(fileWi.posIndex);
			m_src->read(fileWi.infIndex);
			m_src->read(fileWi.infType);
			m_src->read(fileWi.infWeight);

			if(fileWi.posType==Model::PT_Vertex
			 ||fileWi.posType==Model::PT_Point)
			{
				Model::Position pos;
				pos.type = (Model::PositionTypeE)fileWi.posType;
				pos.index = fileWi.posIndex;

				Model::InfluenceTypeE type = Model::IT_Custom;

				switch(fileWi.infType)
				{
				case Model::IT_Custom:
				case Model::IT_Auto:
				case Model::IT_Remainder:
					type = static_cast<Model::InfluenceTypeE>(fileWi.infType);
					break;
				default:
					log_error("unknown influence type %d\n",fileWi.infType);
					break;
				}

				double weight = fileWi.infWeight/100.0;

				log_debug("adding position influence %d,%d,%f\n",
						pos.index,(int)type,(float)weight);
				model->addPositionInfluence(pos,
						fileWi.infIndex,type,weight);
			}
		}
	}
	else if(auto*os=seekOffset(MDT_JointVertices)) // Joint Vertices
	{
		// Newer versions of the file format use MDT_WeightedInfluences instead.
		// We still want to read this data to use as a default in case the
		// weighted influences are not present.

		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned t = 0; t<count; t++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			uint32_t vertexIndex = 0;
			int32_t jointIndex	= 0;
			m_src->read(vertexIndex);
			m_src->read(jointIndex);

			if(vertexIndex<vcount&&(unsigned)jointIndex<modelJoints.size())
			{
				//modelVerts[vertexIndex]->m_boneId = jointIndex;
				model->addVertexInfluence(vertexIndex,jointIndex,Model::IT_Custom,1.0);
			}
			else
			{
				missingElements = true;
				log_error("vertex %d or joint %d out of range\n",vertexIndex,jointIndex);
			}
		}

		log_debug("read %d joints vertices\n",count);
	}

	// Texture Projections
	if(auto*os=seekOffset(MDT_TexProjections))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned j = 0; j<count; j++)
		{
			log_debug("reading Projection %d/%d\n",j,count);
			if(os->variable())
			{
				m_src->read(size);
			}

			uint16_t flags = 0; //SHADOWING
			m_src->read(flags);

			char nameBytes[40];
			m_src->readBytes(nameBytes,sizeof(nameBytes));
			std::string name;
			name.assign((char *)nameBytes,40);

			int32_t type = 0;
			m_src->read(type);

			double seam[3],up[3];
			float  fvec[3];

			unsigned i = 0;

			// position
			for(unsigned i = 0; i<3; i++)
			{
				m_src->read(fvec[i]);
				seam[i] = fvec[i];
			}

			unsigned proj = model->addProjection
			(name.c_str(),type,seam[0],seam[1],seam[2]);

			// up vector (or scale if newer file)
			for(unsigned i = 0; i<3; i++)
			{
				m_src->read(fvec[i]);
				up[i] = fvec[i];
			}

			if(mm3d2020) 
			{				
				//HACK: Go ahead and set all three values.
				//model->setProjectionScale(proj,up[0]);
				model->setPositionScale({Model::PT_Projection,proj},up);				
			}
			else
			{
				//model->setProjectionUp(proj,up);
				model->setProjectionScale(proj,mag3(up));
			}

			// seam vector (or rotation if newer file)
			for(unsigned i = 0; i<3; i++)
			{
				m_src->read(fvec[i]);
				seam[i] = fvec[i];
			}

			if(!mm3d2020) //Older file?
			{
				//model->setProjectionSeam(proj,seam);
				//NOTE: The "seam" is reversed here
				//so it matches the viewport matrix.				
				//NOTE: The "seam" isn't normalized.
				normalize3(seam);
				normalize3(up);
				double m[4][4] = 
				{{1,0,0,0},{up[0],up[1],up[2]},
				{-seam[0],-seam[1],-seam[2]},{0,0,0,1}};
				calculate_normal(m[0],m[3],m[1],m[2]);
				((Matrix*)m)->getRotation(seam);
			}
			model->setProjectionRotation(proj,seam);

			double uv[2][2];
			float  fuv[2][2];

			// texture coordinate range
			for(i = 0; i<2; i++)
			{
				for(int j = 0; j<2; j++)
				{
					m_src->read(fuv[i][j]);
					uv[i][j] = fuv[i][j];
				}
			}

			model->setProjectionRange(proj,uv[0][0],uv[0][1],uv[1][0],uv[1][1]);
		}

		log_debug("read %d projections\n");
	}

	// Texture Projection Triangles (have to read this after projections)
	if(auto*os=seekOffset(MDT_ProjectionTriangles))
	{
		uint16_t flags = 0;
		uint32_t count = 0;

		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned c = 0; c<count; c++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			uint32_t proj = 0;
			uint32_t triCount = 0;
			uint32_t tri = 0;

			m_src->read(proj);
			m_src->read(triCount);

			uint32_t t;
			for(t = 0; t<triCount; t++)
			{
				m_src->read(tri);
				model->setTriangleProjection(tri,proj);
			}
		}
	}

	//2021: Scale
	//https://github.com/zturtleman/mm3d/issues/149
	if(mm3d2021)
	if(auto*os=seekOffset(MDT_ScaleFactors))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned i=0;i<count;i++)
		{
			log_debug("reading Scale Factor %d/%d\n",i,count);
			if(os->variable())
			{
				m_src->read(size);
			}

			Model::Position pos;
			uint16_t tmp16;
			m_src->read(tmp16);
			pos.type = (Model::PositionTypeE)tmp16;
			m_src->read(tmp16); 
			pos.index = tmp16;
			 
			double scale[3]; 
			float32_t tmpf;
			for(unsigned i=0;i<3;i++)			
			{
				m_src->read(tmpf); 
				scale[i] = tmpf;
			}
			model->setPositionScale(pos,scale);
		}
	}

	// Skeletal Animations (LEGACY)
	uint32_t sa_count = 0;
	if(!mm3d2021)
	if(auto*os=seekOffset(MDT_SkelAnims))
	{
		uint16_t flags = 0;
		//uint32_t count = 0;
		m_src->read(flags);
		m_src->read(sa_count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		std::vector<bool> sparse;
		for(unsigned a = 0; a<sa_count; a++)
		{
			log_debug("reading skel anim %d/%d\n",a,sa_count);
			if(os->variable())
			{
				m_src->read(size);
			}

			uint16_t flags; //SHADOWING
			char name[1024];
			float32_t fps;
			uint32_t frameCount;

			m_src->read(flags);		  
			m_src->readAsciiz(name,sizeof(name));
			utf8chrtrunc(name,sizeof(name)-1);

			log_debug("  name is %s\n",name);

			m_src->read(fps);
			m_src->read(frameCount);
			
			unsigned anim = model->addAnimation(Model::ANIMMODE_SKELETAL,name);
			model->setAnimFPS(anim,fps);
			model->setAnimFrameCount(anim,frameCount);
			model->setAnimWrap(anim,(flags&MSAF_ANIM_WRAP)!=0);
			
			if(mm3d2020) //REMOVE ME
			{				
				float32_t frame2020;
				for(uint32_t f=0;f<frameCount;f++)
				{
					m_src->read(frame2020);
					model->setAnimFrameTime(anim,f,frame2020);
				}
				m_src->read(frame2020);
				model->setAnimTimeFrame(anim,frame2020);
			}
			else 
			{
				model->setAnimTimeFrame(anim,frameCount);

				sparse.assign(frameCount,true);
			}

			//NOTE: The number of frames is at most the same
			//as the number of joints, so it might be better
			//to not filter by frames, but vertex-data is so
			for(unsigned f=0;f<frameCount;f++)
			{
				uint32_t keyframeCount;
				m_src->read(keyframeCount);

				if(!keyframeCount) //Legacy?
				{
					//Note: Assume intentionally empty if MM3D2020
					if(!mm3d2020) sparse[f] = false;
				}

				for(unsigned k=0;k<keyframeCount;k++)
				{
					MM3DFILE_KeyframeT fileKf;
					m_src->read(fileKf.objectIndex);
					m_src->read(fileKf.keyframeType);
					m_src->read(fileKf.param[0]);
					m_src->read(fileKf.param[1]);
					m_src->read(fileKf.param[2]);

					auto oi = fileKf.objectIndex;
					auto e = Model::InterpolateLerp;
					if(mm3d2020) //REMOVE ME
					{
						assert((oi>>16&0xFF)==Model::PT_Joint);
						e = (Model::Interpolate2020E)(oi>>24&0xFF);
						oi&=0xFFFF;

						//HACK: This is an error, it's not a problem but it's
						//junk in the file from an erroneous program. Log it?
						if(e<=0||e>Model::InterpolateLerp) continue;
					}

					auto t = Model::KeyTranslate;
					if(~fileKf.keyframeType&1) 
					t = fileKf.keyframeType?Model::KeyScale:Model::KeyRotate;

					model->setKeyframe(anim,f,{Model::PT_Joint,oi},t,
					fileKf.param[0],fileKf.param[1],fileKf.param[2],e);
				}
			}			

			if(!mm3d2020) //REMOVE ME
			{
				//2020: Remove unused frames to improve editing experience
			
				for(unsigned count=frameCount,f=count;f-->0;)
				if(!sparse[f]) 
				model->setAnimFrameCount(anim,--count,f,nullptr);
			}
		}
	}

	// Frame Animations (LEGACY)
	if(!mm3d2021)
	if(auto*os=seekOffset(MDT_FrameAnims))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		std::vector<mm3dfilter_cmp_t<1>> cmp;
		for(unsigned a = 0; a<count; a++)
		{
			log_debug("reading frame animation %d/%d\n",a,count);

			if(os->variable())
			{
				m_src->read(size);
			}

			if(size>m_src->getRemaining())
			{
				log_error("Size of frame animation is too large for file data (%d>%d)\n",
						size,m_src->getRemaining());
				return Model::ERROR_BAD_DATA;
			}

			uint16_t flags; //SHADOWING
			char name[1024];
			float32_t fps;
			uint32_t  frameCount;

			m_src->read(flags);
			m_src->readAsciiz(name,sizeof(name));
			utf8chrtrunc(name,sizeof(name)-1);
			log_debug("anim name '%s' size %d\n",name,size);

			m_src->read(fps);
			log_debug("fps %f\n",fps);
			m_src->read(frameCount);
			log_debug("frame count %u\n",frameCount);
						
			if(!mm3d2020) //REMOVE ME
			if(frameCount>size //???
			 ||frameCount*vcount*sizeof(float32_t)*3>size-10)
			{
				log_error("Frame count for animation is too large for file data\n");
				return Model::ERROR_BAD_DATA;
			}

			unsigned anim = model->addAnimation(Model::ANIMMODE_FRAME,name);
			model->setAnimFPS(anim,fps);
			model->setAnimFrameCount(anim,frameCount);
			model->setAnimWrap(anim,(flags&MFAF_ANIM_WRAP)!=0);

			if(mm3d2020) //REMOVE ME
			{	
				float32_t frame2020;
				for(uint32_t f=0;f<frameCount;f++)
				{
					m_src->read(frame2020);
					model->setAnimFrameTime(anim,f,frame2020);
				}
				m_src->read(frame2020);
				model->setAnimTimeFrame(anim,frame2020);
			}
			else model->setAnimTimeFrame(anim,frameCount);

			auto *fa = modelAnims[sa_count+a];

			if(!mm3d2020) //LEGACY?
			{
				cmp.resize(vcount);
				for(unsigned v=vcount;v-->0;)
				{
					auto *vt = modelVerts[v];
					auto *params = cmp[v].params;
					for(int i=3;i-->0;)
					params[i] = (float32_t)vt->m_coord[i];					
					cmp[v].diff = ~0;
				}
			}
			for(unsigned f=0;f<frameCount;f++)
			{
				unsigned vM,vN,v = 0;

				vM = vcount; if(mm3d2020) //REMOVE ME
				{
					vN = 0;

				restart2020:

					uint32_t m,n;					
					m_src->read(m);
					m_src->read(n);
					vN+=n;

					if(vN>vM) 
					{
						missingElements = true; //HACK
						log_error("Vertex count for frame animation %d, frame %d has %d vertices, should be %d\n",anim,f,vN,vM);

						//HACK: InterpolateCopy code below is bypassing safe API.
						break;
					}

					auto e = (Model::Interpolate2020E)m;

					//NOTE: Current data structure leaves dummies in
					//place. They could point to sentinel objects or 
					//they could store their values to not recompute.
					if(e<=Model::InterpolateCopy) 
					{
						if(e==Model::InterpolateCopy)
						for(auto fp=fa->_frame0(model)+f;v<vN;v++)
						{
							//FIX ME
							//UNSAFE: Need an API for this!
							modelVerts[v]->m_frames[fp]->m_interp2020 = Model::InterpolateCopy;
						}
						else //InterpolateNone?
						{
							v+=n; assert(!e);
						}
						if(v<vM) goto restart2020;
					}

					float32_t coord[3]; for(;v<vN;v++)
					{
						for(int i=0;i<3;i++) m_src->read(coord[i]);
						model->setQuickFrameAnimVertexCoords(anim,f,v,coord[0],coord[1],coord[2],e);
					}
				}
				else
				{
					vN = vM;

					float32_t coord[3]; for(;v<vN;v++)
					{
						for(int i=0;i<3;i++) m_src->read(coord[i]);
						int prev, curr;
						float32_t *pcoord = cmp[v].compare(coord,prev,curr);
						//model->setFrameAnimVertexCoords(anim,f,v,coord[0],coord[1],coord[2]);
						if(prev&1) model->setQuickFrameAnimVertexCoords(anim,f-1,v,pcoord[0],pcoord[1],pcoord[2]);
						if(curr&1) model->setQuickFrameAnimVertexCoords(anim,f,v,coord[0],coord[1],coord[2]);
						memcpy(pcoord,coord,sizeof(coord));				
					}
				}
				if(vN<vM) goto restart2020;
			}
		}
	}

	// Frame Animation Points (LEGACY)
	if(!mm3d2021)
	if(auto*os=seekOffset(MDT_FrameAnimPoints))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		std::vector<mm3dfilter_cmp_t<2>> cmpt;
		for(unsigned a = 0; a<count; a++)
		{
			if(os->variable())
			{
				m_src->read(size);
			}

			uint16_t flags; //SHADOWING
			uint32_t anim;
			uint32_t frameCount;

			m_src->read(flags);
			m_src->read(anim);
			anim+=sa_count;
			m_src->read(frameCount);

			if(!mm3d2020) //LEGACY?
			{
				cmpt.resize(pcount);
				for(unsigned p=pcount;p-->0;)
				{
					auto *pt = modelPoints[p];
					auto *params = cmpt[p].params;
					for(int i=3;i-->0;)
					{
						params[0+i] = (float32_t)pt->m_abs[i];
						params[3+i] = (float32_t)pt->m_rot[i];
					}
					cmpt[p].diff = ~0;
				}
			}

			//NOTE: The number of frames is at most the same
			//as the number of points, so it might be better
			//to not filter by frames, but vertex-data is so
			for(unsigned f=0;f<frameCount;f++)
			{
				if(mm3d2020) //REMOVE ME
				{							
					uint32_t keyframeCount;
					m_src->read(keyframeCount);

					for(unsigned k=0;k<keyframeCount;k++)
					{
						MM3DFILE_KeyframeT fileKf;
						m_src->read(fileKf.objectIndex);
						m_src->read(fileKf.keyframeType);
						m_src->read(fileKf.param[0]);
						m_src->read(fileKf.param[1]);
						m_src->read(fileKf.param[2]);

						auto oi = fileKf.objectIndex;
						assert((oi>>16&0xFF)==Model::PT_Point);
						auto e = (Model::Interpolate2020E)(oi>>24&0xFF);
						oi&=0xFFFF;
						//HACK: This is an error, it's not a problem but it's
						//junk in the file from an erroneous program. Log it?
						if(e<=0||e>Model::InterpolateLerp) continue;

						auto t = Model::KeyTranslate;
						if(~fileKf.keyframeType&1) 
						t = fileKf.keyframeType?Model::KeyScale:Model::KeyRotate;

						model->setKeyframe(anim,f,{Model::PT_Point,oi},t,
						fileKf.param[0],fileKf.param[1],fileKf.param[2],e);
					}
				}
				else for(Model::Position p{Model::PT_Point,0};p<pcount;p++) //LEGACY
				{
					float32_t params[3+3];
					for(int i=0;i<3;i++) m_src->read(params[3+i]);					
					for(int i=0;i<3;i++) m_src->read(params[0+i]);
					int prev, curr;
					float32_t *pparams = cmpt[p].compare(params,prev,curr);						
					//model->setQuickFrameAnimPoint(anim,f,p,trans[0],trans[1],trans[2],rot[0],rot[1],rot[2]);
					if(prev&1) model->setKeyframe(anim,f-1,p,Model::KeyTranslate,pparams[0],pparams[1],pparams[2]);
					if(prev&2) model->setKeyframe(anim,f-1,p,Model::KeyRotate,pparams[3],pparams[4],pparams[5]);
					if(curr&1) model->setKeyframe(anim,f,p,Model::KeyTranslate,params[0],params[1],params[2]);					
					if(curr&2) model->setKeyframe(anim,f,p,Model::KeyRotate,params[3],params[4],params[5]);					
					memcpy(pparams,params,sizeof(params));
				}
			}
		}
	}

	//2021: Animations
	//https://github.com/zturtleman/mm3d/issues/149
	if(mm3d2021)
	if(auto*os=seekOffset(MDT_Animations))
	{
		uint16_t flags = 0;
		uint32_t count = 0;
		m_src->read(flags);
		m_src->read(count);

		uint32_t size = 0;
		if(os->uniform())
		{
			m_src->read(size);
		}

		for(unsigned a=0;a<count;a++)
		{
			log_debug("reading animation %d/%d\n",a,count);
			if(os->variable())
			{
				m_src->read(size);
			}
			
			if(size>m_src->getRemaining())
			{
				log_error("Size of animation is too large for file data (%d>%d)\n",
						size,m_src->getRemaining());
				return Model::ERROR_BAD_DATA;
			}

			uint16_t flags; //SHADOWING
			char name[1024];
			float32_t fps;
			uint32_t frameCount;

			m_src->read(flags);
			m_src->readAsciiz(name,sizeof(name));
			utf8chrtrunc(name,sizeof(name)-1);
			log_debug("anim name '%s' size %d\n",name,size);

			m_src->read(fps);
			log_debug("fps %f\n",fps);
			m_src->read(frameCount);
			log_debug("frame count %u\n",frameCount);
		
			auto type = (Model::AnimationModeE)(flags&MAF_ANIM_TYPE); //7

			int anim = model->addAnimation(type,name); if(anim<0)
			{
				log_error("Type of animation was unrecognized (%d of %d)\n",
						type,flags);
				return Model::ERROR_BAD_DATA;
			}
			model->setAnimFPS(anim,fps);
			model->setAnimFrameCount(anim,frameCount);
			model->setAnimWrap(anim,(flags&MAF_ANIM_WRAP)!=0); //8

			float32_t frame2020;
			for(uint32_t f=0;f<frameCount;f++)
			{
				m_src->read(frame2020);
				model->setAnimFrameTime(anim,f,frame2020);
			}
			m_src->read(frame2020);
			model->setAnimTimeFrame(anim,frame2020);
			
			auto *ab = modelAnims[anim];

			uint32_t keyframeMask;
			m_src->read(keyframeMask);
			if(keyframeMask&Model::KM_Vertex) //1
			for(unsigned f=0;f<frameCount;f++)
			{
				unsigned vM,vN,v = 0;

				vM = vcount;
				{
					vN = 0;

				restart2021:

					uint32_t m,n;
					uint8_t fd[4];
					//m_src->read(m);
					m_src->readBytes(fd,4); m = fd[0];
					m_src->read(n);
					vN+=n;

					//NOTE: A few development/demonstration files may
					//have size 0 for InterpolateStep/InterpolateLerp. 
					if(fd[1]||m>Model::InterpolateLerp||fd[3]!=3&&fd[3])
					{
						/*Can try to recover here, but if the file is newer
						//than WRITE_VERSION_MAJOR/WRITE_VERSION_MINOR then
						//it should have been rejected.
						if(0) 
						{
							m_src->advanceBytes(4*fd[3]); continue;
						}*/
						log_error("Vertex keyframe format was unrecognized (%d-%d-%d-%d)\n",
							fd[0],fd[1],fd[2],fd[3]);
						return Model::ERROR_BAD_DATA;
					}

					if(vN>vM) 
					{
						missingElements = true; //HACK
						log_error("Vertex count for frame animation %d, frame %d has %d vertices, should be %d\n",anim,f,vN,vM);

						//HACK: InterpolateCopy code below is bypassing safe API.
						break;
					}

					auto e = (Model::Interpolate2020E)m;

					//NOTE: Current data structure leaves dummies in
					//place. They could point to sentinel objects or 
					//they could store their values to not recompute.
					if(e<=Model::InterpolateCopy) 
					{
						if(e==Model::InterpolateCopy)
						for(auto fp=ab->_frame0(model)+f;v<vN;v++)
						{
							//FIX ME
							//UNSAFE: Need an API for this!
							modelVerts[v]->m_frames[fp]->m_interp2020 = Model::InterpolateCopy;
						}
						else //InterpolateNone?
						{
							v+=n; assert(!e);
						}
						if(v<vM) goto restart2021;
					}

					float32_t coord[3]; for(;v<vN;v++)
					{
						for(int i=0;i<3;i++) m_src->read(coord[i]);
						model->setQuickFrameAnimVertexCoords(anim,f,v,coord[0],coord[1],coord[2],e);
					}
				}
				if(vN<vM) goto restart2021;
			}

			for(int i=1;i<=2;i++) if(keyframeMask&1<<i) //PM_Joint/PM_Point
			{
				Model::Position pos;
				pos.type = (Model::PositionTypeE)i;

				uint32_t keyframeCount;
				m_src->read(keyframeCount); while(keyframeCount-->0)
				{
					uint8_t fd[4];
					m_src->readBytes(fd,4);

					if(fd[1]||fd[0]>Model::InterpolateLerp||fd[3]!=4)
					{
						/*Can try to recover here, but if the file is newer
						//than WRITE_VERSION_MAJOR/WRITE_VERSION_MINOR then
						//it should have been rejected.
						if(0) 
						{
							m_src->advanceBytes(4*fd[3]); continue;
						}*/
						log_error("Object keyframe format was unrecognized (%d-%d-%d-%d)\n",
							fd[0],fd[1],fd[2],fd[3]);
						return Model::ERROR_BAD_DATA;
					}

					uint16_t tmp16;
					m_src->read(tmp16);
					pos.index = tmp16;
					m_src->read(tmp16); //frame
					float32_t tmpf[3];
					m_src->read(tmpf[0]);
					m_src->read(tmpf[1]);
					m_src->read(tmpf[2]);
					auto kt = (Model::KeyType2020E)fd[2];
					auto et = (Model::Interpolate2020E)fd[0];
					model->setKeyframe(anim,tmp16,pos,kt,tmpf[0],tmpf[1],tmpf[2],et);
				}
			}
		}
	}

	// Read unknown data
	UnknownDataList::iterator it;
	for(it = unknownList.begin(); it!=unknownList.end(); it++)
	{
		Model::FormatData *fd = new Model::FormatData;
		fd->offsetType = (*it).offsetType;
		m_src->seek((*it).offsetValue);

		log_debug("unknown data is type %x...\n",fd->offsetType);

		fd->data = new uint8_t[(*it).dataLen];
		fd->len  = (*it).dataLen;
		m_src->readBytes(fd->data,(*it).dataLen);

		if(model->addFormatData(fd)<0)
		{
			delete fd;
		}
	}

	// Account for missing elements (vertices,triangles,textures,joints)
	{
		unsigned vcount = modelVerts.size();
		unsigned tcount = modelTriangles.size();
		unsigned gcount = modelGroups.size();
		unsigned jcount = modelJoints.size();
		unsigned mcount = modelMaterials.size();

//		for(unsigned v = 0; v<vcount; v++)
//		{
//			if(modelVerts[v]->m_boneId>=(signed)jcount)
//			{
//				missingElements = true;
//				log_error("Vertex %d uses missing bone joint %d\n",v,modelVerts[v]->m_boneId);
//			}
//		}

		for(unsigned t = 0; t<tcount; t++)
		{
			for(unsigned i = 0; i<3; i++)
			{
				if(modelTriangles[t]->m_vertexIndices[i]>=vcount)
				{
					missingElements = true;
					log_error("Triangle %d uses missing vertex %d\n",t,modelTriangles[t]->m_vertexIndices[i]);
				}
			}
		}

		for(unsigned g = 0; g<gcount; g++)
		{
			for(int i:modelGroups[g]->m_triangleIndices)
			{
				if(i>=(signed)tcount)
				{
					missingElements = true;
					log_error("Group %d uses missing triangle %d\n",g,*it);
				}
			}

			if(modelGroups[g]->m_materialIndex>=(signed)mcount)
			{
				missingElements = true;
				log_error("Group %d uses missing texture %d\n",g,modelGroups[g]->m_materialIndex);
			}
		}
	}
	if(missingElements)
	{
		log_warning("missing elements in file\n");
		return Model::ERROR_BAD_DATA;
	}

	return Model::ERROR_NONE;
}

Model::ModelErrorE MisfitFilter::writeFile(Model *model, const char *const filename, Options&)
{
	/*if(sizeof(float32_t)!=4)
	{
		msg_error(TRANSLATE("LowLevel","MM3D encountered an unexpected data size problem\nSee Help->About to contact the developers")).c_str());
		return Model::ERROR_FILE_OPEN;
	}*/
	int compile[4==sizeof(float32_t)]; (void)compile;

	Model::ModelErrorE err = Model::ERROR_NONE;
	m_dst = openOutput(filename,err);
	DestCloser fc(m_dst);

	if(err!=Model::ERROR_NONE) return err;

	std::string modelPath = "";
	std::string modelBaseName = "";
	std::string modelFullName = "";
	normalizePath(filename,modelFullName,modelPath,modelBaseName);

	// Get model data

	auto &modelVerts = model->getVertexList();
	auto &modelTriangles = model->getTriangleList();
	auto &modelGroups = model->getGroupList();
	auto &modelMaterials = model->getMaterialList();
	auto &modelJoints = model->getJointList();
	auto &modelPoints = model->getPointList();
	auto &modelProjections = model->getProjectionList();
	auto &modelBackgrounds = model->getBackgroundList();
	auto &modelAnims = model->getAnimationList();

	bool haveProjectionTriangles = false;
	if(modelProjections.size())
	for(auto*ea:modelTriangles) if(ea->m_projection>=0)
	{
		haveProjectionTriangles = true; break;
	}
	
	int backgrounds = 0;
	for(auto*ea:modelBackgrounds) if(!ea->m_name.empty())
	{
		backgrounds++;
	}

	int basescaled = 0;
	for(auto*ea:modelJoints)
	if(1!=ea->m_xyz[0]||1!=ea->m_xyz[1]||1!=ea->m_xyz[2])
	basescaled++; 
	for(auto*ea:modelPoints)
	if(1!=ea->m_xyz[0]||1!=ea->m_xyz[1]||1!=ea->m_xyz[2])
	basescaled++; 

	int influenced = 0;		
	for(auto*ea:modelVerts) influenced+=ea->m_influences.size();
	for(auto*ea:modelPoints) influenced+=ea->m_influences.size();

	// Write header
	//MisfitOffsetList offsetList;
	m_offsetList.clear();
	// Find out what sections we need to write
	addOffset(MDT_Meta				  , 0!=model->getMetaDataCount());
	addOffset(MDT_Vertices			  , !modelVerts.empty());
	addOffset(MDT_Triangles			  , !modelTriangles.empty());
	addOffset(MDT_TriangleNormals	  , !modelTriangles.empty());
	addOffset(MDT_Materials			  , !modelMaterials.empty());
	addOffset(MDT_Groups			  , !modelGroups.empty());
	addOffset(MDT_SmoothAngles		  , !modelGroups.empty());
	addOffset(MDT_ExtTextures		  , !modelMaterials.empty());
	// Some users map texture coordinates before assigning a texture (think: paint texture)
	addOffset(MDT_TexCoords			  , !modelTriangles.empty()); 
	addOffset(MDT_ProjectionTriangles , haveProjectionTriangles);
	addOffset(MDT_CanvasBackgrounds	  , 0!=backgrounds);
	addOffset(MDT_Joints			  , !modelJoints.empty());
	//addOffset(MDT_JointVertices	  , !modelJoints.empty());
	addOffset(MDT_Points			  , !modelPoints.empty());
	addOffset(MDT_WeightedInfluences  , 0!=influenced);
	addOffset(MDT_TexProjections	  , !modelProjections.empty());
	//addOffset(MDT_SkelAnims		  , !modelSkelAnims.empty());
	//addOffset(MDT_FrameAnims		  , !modelFrameAnims.empty());
	//addOffset(MDT_FrameAnimPoints	  , !modelFrameAnims.empty()&&!modelPoints.empty());
	addOffset(MDT_ScaleFactors        , 0!=basescaled);
	addOffset(MDT_Animations		  , !modelAnims.empty());
	for(int f=0,fc=model->getFormatDataCount();f<fc;f++)
	{
		Model::FormatData *fd = model->getFormatData(f);
		if(uint16_t ot=fd->offsetType)
		{
			log_warning("adding unknown data type %04x\n",ot);
			m_offsetList.push_back({ot|=OFFSET_DIRTY_MASK,0});			
		}
	}
	addOffset(MDT_EndOfFile,true);

	//m_dst->writeBytes(MISFIT3D,strlen(MISFIT3D));
	m_dst->writeBytes(MM3D2020,strlen(MISFIT3D));
	m_dst->write(WRITE_VERSION_MAJOR);
	m_dst->write(WRITE_VERSION_MINOR);
	m_dst->write((uint8_t)/*modelFlags*/0); //UNUSED
	m_dst->write((uint8_t)m_offsetList.size());

	for(auto&ea:m_offsetList)
	{
		m_dst->write(ea.offsetType);
		m_dst->write(ea.offsetValue);
	}
	log_debug("wrote %d offsets\n",m_offsetList.size());

	// Write data

	// Meta data
	if(setOffset(MDT_Meta,false))
	{
		unsigned count = model->getMetaDataCount();

		writeHeaderA(0x0000,count);

		for(unsigned m = 0; m<count; m++)
		{
			char key[1024];
			char value[1024];

			model->getMetaData(m,key,sizeof(key),value,sizeof(value));

			uint32_t keyLen = strlen(key)+1;
			uint32_t valueLen = strlen(value)+1;

			m_dst->write(keyLen+valueLen);

			m_dst->writeBytes(key,keyLen);
			m_dst->writeBytes(value,valueLen);
		}
		log_debug("wrote %d meta data pairs\n",count);
	}

	// Vertices
	if(setOffset(MDT_Vertices,true))
	{
		unsigned count = modelVerts.size();

		writeHeaderB(0x0000,count,FILE_VERTEX_SIZE);

		for(unsigned v = 0; v<count; v++)
		{
			MM3DFILE_VertexT fileVertex;

			fileVertex.flags  = 0x0000;
			if(!modelVerts[v]->m_visible)
			fileVertex.flags |= MF_HIDDEN;
			if(modelVerts[v]->m_selected)
			fileVertex.flags |= MF_SELECTED;
			//if(modelVerts[v]->m_free)
			if(modelVerts[v]->m_faces.empty())
			fileVertex.flags |= MF_VERTFREE;

			fileVertex.coord[0] = (float32_t)modelVerts[v]->m_coord[0];
			fileVertex.coord[1] = (float32_t)modelVerts[v]->m_coord[1];
			fileVertex.coord[2] = (float32_t)modelVerts[v]->m_coord[2];

			m_dst->write(fileVertex.flags);
			m_dst->write(fileVertex.coord[0]);
			m_dst->write(fileVertex.coord[1]);
			m_dst->write(fileVertex.coord[2]);
		}
		log_debug("wrote %d vertices\n",count);
	}

	// Triangles
	if(setOffset(MDT_Triangles,true))
	{
		unsigned count = modelTriangles.size();

		writeHeaderB(0x0000,count,FILE_TRIANGLE_SIZE);

		for(unsigned t = 0; t<count; t++)
		{
			MM3DFILE_TriangleT fileTriangle;

			fileTriangle.flags = 0x0000;
			if(!modelTriangles[t]->m_visible)
			fileTriangle.flags |= MF_HIDDEN;
			if(modelTriangles[t]->m_selected)
			fileTriangle.flags |= MF_SELECTED;
			fileTriangle.flags = fileTriangle.flags;

			fileTriangle.vertex[0] = modelTriangles[t]->m_vertexIndices[0];
			fileTriangle.vertex[1] = modelTriangles[t]->m_vertexIndices[1];
			fileTriangle.vertex[2] = modelTriangles[t]->m_vertexIndices[2];

			m_dst->write(fileTriangle.flags);
			m_dst->write(fileTriangle.vertex[0]);
			m_dst->write(fileTriangle.vertex[1]);
			m_dst->write(fileTriangle.vertex[2]);
		}
		log_debug("wrote %d triangles\n",count);
	}

	// Triangle Normals
	if(setOffset(MDT_TriangleNormals,true))
	{
		unsigned count = modelTriangles.size();

		writeHeaderB(0x0000,count,FILE_TRIANGLE_NORMAL_SIZE);

		for(unsigned t = 0; t<count; t++)
		{
			//REFERENCE
			//MM3DFILE_TriangleNormalsT fileNormals;
			//fileNormals.flags = 0x0000;
			//fileNormals.index = t;
			//for(unsigned v=0;v<3;v++)
			{
				//Can this source from m_finalNormals instead?
				//NOTE: m_vertexNormals did not factor in smoothing... it can be
				//disabled by calculateNormals if necessary.
				//fileNormals.normal[v][...] = modelTriangles[t]->m_vertexNormals[v][...];
			}
			m_dst->write((uint16_t)0);
			m_dst->write((uint32_t)t);
			for(unsigned v=0;v<3;v++)
			for(unsigned i=0;i<3;i++)
			m_dst->write((float32_t)modelTriangles[t]->m_finalNormals[v][i]);
		}
		log_debug("wrote %d triangle normals\n",count);
	}

	/*https://github.com/zturtleman/mm3d/issues/130
	// Groups
	if(setOffset(MDT_Groups)*/	

	std::unordered_map<std::string,uint32_t> texMap;

	// Materials
	if(setOffset(MDT_Materials,false))
	{
		unsigned count = modelMaterials.size();

		writeHeaderA(0x0000,count);

		unsigned baseSize = sizeof(uint16_t)+sizeof(uint32_t)
		  +(sizeof(float32_t)*17);

		int texNum = 0;
		for(unsigned m = 0; m<count; m++)
		{
			auto mat = modelMaterials[m];
			uint32_t matSize = baseSize+mat->m_name.size()+1;

			uint16_t flags = 0x0000;
			uint32_t texIndex = ~0;  // TODO deal with embedded textures

			switch (mat->m_type)
			{
			case Model::Material::MATTYPE_COLOR: //UNUSED
				flags = 0x000d;
				break;
			case Model::Material::MATTYPE_GRADIENT: //UNUSED
				flags = 0x000e;
				break;
			case Model::Material::MATTYPE_BLANK:
				flags = 0x000f;
				break;
			case Model::Material::MATTYPE_TEXTURE:
			{
				flags = 0x0000;
				auto ins = texMap.insert({mat->m_filename,texNum});
				if(ins.second) texNum++;
				texIndex = ins.first->second;
				break;
			}}

			if(mat->m_sClamp) flags |= MF_MAT_CLAMP_S;
			if(mat->m_tClamp) flags |= MF_MAT_CLAMP_T;

			m_dst->write(matSize);
			m_dst->write(flags);
			m_dst->write(texIndex);
			m_dst->writeBytes(mat->m_name.c_str(),mat->m_name.size()+1);

			unsigned i = 0;
			float32_t lightProp = 0;
			for(i = 0; i<4; i++)
			{
				lightProp = mat->m_ambient[i];
				m_dst->write(lightProp);
			}
			for(i = 0; i<4; i++)
			{
				lightProp = mat->m_diffuse[i];
				m_dst->write(lightProp);
			}
			for(i = 0; i<4; i++)
			{
				lightProp = mat->m_specular[i];
				m_dst->write(lightProp);
			}
			for(i = 0; i<4; i++)
			{
				lightProp = mat->m_emissive[i];
				m_dst->write(lightProp);
			}
			lightProp = mat->m_shininess;
			m_dst->write(lightProp);

		}
		log_debug("wrote %d materials with %d internal textures\n",count,count-texNum);
	}
	if(setOffset(MDT_Groups,false))
	{
		unsigned count = modelGroups.size();

		writeHeaderA(0x0000,count);

		unsigned baseSize = sizeof(uint16_t)+sizeof(uint32_t)
		  +sizeof(uint8_t)+sizeof(uint32_t);

		for(unsigned g = 0; g<count; g++)
		{
			auto *grp = modelGroups[g];
			uint32_t groupSize = baseSize+grp->m_name.size()+1 
			  +(grp->m_triangleIndices.size()*sizeof(uint32_t));

			uint16_t flags = 0x0000;
		//	if((modelGroups[g]->m_visible) //UNUSED
		//	flags |= MF_HIDDEN; 
			if(modelGroups[g]->m_selected)
			flags |= MF_SELECTED;

			uint32_t triCount = grp->m_triangleIndices.size();

			m_dst->write(groupSize);
			m_dst->write(flags);
			m_dst->writeBytes(grp->m_name.c_str(),grp->m_name.size()+1);
			m_dst->write(triCount);

			for(int i:grp->m_triangleIndices)
			{
				m_dst->write((uint32_t)i);
			}
			uint8_t  smoothness = grp->m_smooth;
			uint32_t materialIndex = grp->m_materialIndex;
			m_dst->write(smoothness);
			m_dst->write(materialIndex);

		}
		log_debug("wrote %d groups\n",count);
	}
	//TODO: Why not expand MDT_Groups size???
	if(setOffset(MDT_SmoothAngles,true))
	{
		unsigned count = modelGroups.size();

		writeHeaderB(0x0000,count,FILE_SMOOTH_ANGLE_SIZE);

		for(unsigned t = 0; t<count; t++)
		{
			MM3DFILE_SmoothAngleT fileSa;
			fileSa.groupIndex = t;
			fileSa.angle = model->getGroupAngle(t);

			m_dst->write(fileSa.groupIndex);
			m_dst->write(fileSa.angle);
		}
		log_debug("wrote %d group smoothness angles\n",count);
	}

	// External Textures
	if(setOffset(MDT_ExtTextures,false))
	{
		//unsigned count = modelMaterials.size();
		unsigned count = texMap.size();

		writeHeaderA(0x0000,count);

		unsigned baseSize = sizeof(uint16_t);

		for(unsigned m=0;m<count;m++)
		for(auto&ea:texMap) 
		{
			if(m!=ea.second) continue;

			std::string fileStr = getRelativePath(modelPath.c_str(),ea.first.c_str());

			char filename[PATH_MAX];
			strncpy(filename,fileStr.c_str(),PATH_MAX);
			utf8chrtrunc(filename,sizeof(filename)-1);

			replaceSlash(filename);

			size_t len = strlen(filename)+1;
			uint32_t texSize = baseSize+len;

			uint16_t flags = 0x0000;

			m_dst->write(texSize);
			m_dst->write(flags);
			m_dst->writeBytes(filename,len);

			log_debug("texture file is %s\n",filename);

			break;
		}
		log_debug("wrote %d external textures\n",count);
	}

	// Texture Coordinates
	if(setOffset(MDT_TexCoords,true))
	{
		unsigned count = modelTriangles.size();

		writeHeaderB(0x0000,count,FILE_TEXCOORD_SIZE);

		for(unsigned t = 0; t<count; t++)
		{
			auto *tri = modelTriangles[t];
			MM3DFILE_TexCoordT tc;

			tc.flags = 0x0000;
			tc.triangleIndex = t;
			for(unsigned v = 0; v<3; v++)
			{
				tc.sCoord[v] = tri->m_s[v];
				tc.tCoord[v] = tri->m_t[v];
			}

			m_dst->write(tc.flags);
			m_dst->write(tc.triangleIndex);
			m_dst->write(tc.sCoord[0]);
			m_dst->write(tc.sCoord[1]);
			m_dst->write(tc.sCoord[2]);
			m_dst->write(tc.tCoord[0]);
			m_dst->write(tc.tCoord[1]);
			m_dst->write(tc.tCoord[2]);
		}
		log_debug("wrote %d texture coordinates\n",count);
	}

	// Texture Projection Triangles
	if(setOffset(MDT_ProjectionTriangles,false))
	{
		unsigned pcount = model->getProjectionCount();

		writeHeaderA(0x0000,pcount);

		unsigned log_tris = 0;

		for(unsigned p=0;p<pcount; p++)
		{
			unsigned wcount = 0; // triangles to write
			unsigned tcount = model->getTriangleCount();
			unsigned t;
			for(t = 0; t<tcount; t++)
			if(model->getTriangleProjection(t)==(int)p)
			{
				wcount++;
			}
			log_tris+=wcount;

			uint32_t triSize = sizeof(uint32_t)*2+sizeof(uint32_t)*wcount;
			uint32_t writeProj  = (uint32_t)p;
			uint32_t writeCount = (uint32_t)wcount;

			m_dst->write(triSize);
			m_dst->write(writeProj);
			m_dst->write(writeCount);

			for(t = 0; t<tcount; t++)
			if(model->getTriangleProjection(t)==(int)p)
			{
				uint32_t tri = t;
				m_dst->write(tri);
			}
		}
		log_debug("wrote %d triangles affected by a texture projection\n",log_tris);
	}

	// Canvas Backgrounds
	if(setOffset(MDT_CanvasBackgrounds,false))
	{
		unsigned count = backgrounds;

		writeHeaderA(0x0000,count);

		unsigned baseSize = FILE_CANVAS_BACKGROUND_SIZE;

		for(unsigned b = 0; b<6; b++)
		{
			const char *file = model->getBackgroundImage(b);
			if(!*file) continue;
			
			MM3DFILE_CanvasBackgroundT cb;
			cb.flags = 0x0000;
			cb.viewIndex = b;

			cb.scale = (float)model->getBackgroundScale(b);
			model->getBackgroundCenter(b,cb.center[0],cb.center[1],cb.center[2]);

			std::string relFile = 
			getRelativePath(modelPath.c_str(),file);
			uint32_t backSize = baseSize+relFile.size()+1;

			//char *filedup = strdup(file.c_str()); //free(filedup);
			char *filedup = (char*)relFile.c_str();
			replaceSlash(filedup);
			utf8chrtrunc(filedup,relFile.size()-1);

			m_dst->write(backSize);
			m_dst->write(cb.flags);
			m_dst->write(cb.viewIndex);
			m_dst->write(cb.scale);
			m_dst->write(cb.center[0]);
			m_dst->write(cb.center[1]);
			m_dst->write(cb.center[2]);
			m_dst->writeBytes(filedup,relFile.size()+1);
		}
		log_debug("wrote %d canvas backgrounds\n",count);
	}

	// Joints
	if(setOffset(MDT_Joints,true))
	{
		unsigned count = modelJoints.size();

		writeHeaderB(0x0000,count,FILE_JOINT_SIZE);

		for(unsigned j=0;j<count;j++)
		{
			MM3DFILE_JointT fileJoint;
			auto joint = modelJoints[j];

			fileJoint.flags = 0x0000;
			if(!modelJoints[j]->m_visible)
			fileJoint.flags |= MF_HIDDEN;
			if(modelJoints[j]->m_selected)
			fileJoint.flags |= MF_SELECTED;
			if(!modelJoints[j]->m_bone)
			fileJoint.flags |= MF_VERTFREE; //2020 (draw as line?)

			strncpy(fileJoint.name,joint->m_name.c_str(),sizeof(fileJoint.name));
			utf8chrtrunc(fileJoint.name,sizeof(fileJoint.name)-1);

			fileJoint.parentIndex = joint->m_parent;
			for(int i=3;i-->0;)
			{
				fileJoint.localRot[i]	= (float)joint->m_rot[i];
				fileJoint.localTrans[i] = (float)joint->m_rel[i];
			}

			m_dst->write(fileJoint.flags);
			m_dst->writeBytes(fileJoint.name,sizeof(fileJoint.name));
			m_dst->write(fileJoint.parentIndex);
			m_dst->write(fileJoint.localRot[0]);
			m_dst->write(fileJoint.localRot[1]);
			m_dst->write(fileJoint.localRot[2]);
			m_dst->write(fileJoint.localTrans[0]);
			m_dst->write(fileJoint.localTrans[1]);
			m_dst->write(fileJoint.localTrans[2]);
		}
		log_debug("wrote %d joints\n",count);
	}

	// Points
	if(setOffset(MDT_Points,true))
	{
		unsigned count = model->getPointCount();

		writeHeaderB(0x0000,count,FILE_POINT_SIZE);

		for(unsigned p = 0; p<count; p++)
		{
			MM3DFILE_PointT filePoint;

			filePoint.flags = 0x0000;
			if(!model->isPointVisible(p))
			filePoint.flags |= MF_HIDDEN;
			if(model->isPointSelected(p))
			filePoint.flags |= MF_SELECTED;

			strncpy(filePoint.name,model->getPointName(p),sizeof(filePoint.name));
			utf8chrtrunc(filePoint.name,sizeof(filePoint.name)-1);

			//MM3D2020: Same deal as MDT_JointVertices?
			//filePoint.boneIndex = model->getPrimaryPointInfluence(p);

			auto point = modelPoints[p];
			for(int i=3;i-->0;)
			{
				filePoint.rot[i] = (float)point->m_rot[i];
				filePoint.trans[i] = (float)point->m_abs[i];
			}

			m_dst->write(filePoint.flags);
			m_dst->writeBytes(filePoint.name,sizeof(filePoint.name));
			//NOTE: This previously wrote uninitialized garbage data.
			m_dst->write(filePoint.type=0);
			m_dst->write(filePoint.boneIndex=-1); //MM3D2020
			m_dst->write(filePoint.rot[0]);
			m_dst->write(filePoint.rot[1]);
			m_dst->write(filePoint.rot[2]);
			m_dst->write(filePoint.trans[0]);
			m_dst->write(filePoint.trans[1]);
			m_dst->write(filePoint.trans[2]);
		}
		log_debug("wrote %d points\n",count);
	}	

	// Weighted influences
	if(setOffset(MDT_WeightedInfluences,true))
	{
		writeHeaderB(0x0000,influenced,FILE_WEIGHTED_INFLUENCE_SIZE);

		Model::Position pos; auto f = [&](const infl_list &infl)
		{
			for(auto&ea:infl)
			{
				MM3DFILE_WeightedInfluenceT fileWi;
				fileWi.posType = pos.type;
				fileWi.posIndex = pos.index;
				fileWi.infType = ea.m_type;
				fileWi.infIndex = ea.m_boneId;
				fileWi.infWeight = (int)lround(ea.m_weight*100.0);

				m_dst->write(fileWi.posType);
				m_dst->write(fileWi.posIndex);
				m_dst->write(fileWi.infIndex);
				m_dst->write(fileWi.infType);
				m_dst->write(fileWi.infWeight);
			}
			pos.index++;
		};
		pos = {Model::PT_Vertex,0}; for(auto*ea:modelVerts) f(ea->m_influences);
		pos = {Model::PT_Point,0}; for(auto*ea:modelPoints) f(ea->m_influences);

		log_debug("wrote %d weighted influences\n",influenced);
	}

	// Texture Projections
	if(setOffset(MDT_TexProjections,true))
	{
		unsigned count = model->getProjectionCount();

		writeHeaderB(0x0000,count,FILE_TEXTURE_PROJECTION_SIZE);

		for(unsigned p = 0; p<count; p++)
		{
			uint16_t flags = 0;
			m_dst->write(flags);

			char name[40];
			snprintf(name,sizeof(name),"%s",model->getProjectionName(p));
			utf8chrtrunc(name,sizeof(name)-1);
			m_dst->writeBytes(name,sizeof(name));

			int32_t type = model->getProjectionType(p);
			m_dst->write(type);

			double coord[3];
			model->getProjectionCoords(p,coord);
			m_dst->write((float32_t)coord[0]);
			m_dst->write((float32_t)coord[1]);
			m_dst->write((float32_t)coord[2]);

			//MM3D2020			
			//model->getProjectionUp(p,coord);
			//HACK: Go ahead and get all three values.
			model->getPositionScale({Model::PT_Projection,p},coord);
			m_dst->write((float32_t)coord[0]);
			m_dst->write((float32_t)coord[1]);
			m_dst->write((float32_t)coord[2]);

			//MM3D2020
			//model->getProjectionSeam(p,coord);
			model->getProjectionRotation(p,coord);
			m_dst->write((float32_t)coord[0]);
			m_dst->write((float32_t)coord[1]);
			m_dst->write((float32_t)coord[2]);

			double uv[2][2];
			model->getProjectionRange(p,uv[0][0],uv[0][1],uv[1][0],uv[1][1]);			
			m_dst->write((float32_t)uv[0][0]);
			m_dst->write((float32_t)uv[0][1]);
			m_dst->write((float32_t)uv[1][0]);
			m_dst->write((float32_t)uv[1][1]);
		}
		log_debug("wrote %d texture projections\n",count);
	}

	//2021: Scale
	if(setOffset(MDT_ScaleFactors,true))
	{
		writeHeaderB(0x0000,basescaled,FILE_SCALE_FACTOR_SIZE);

		Model::Position pos;
		auto f = [&](const Model::Object2020 *ea)
		{
			if(1!=ea->m_xyz[0]||1!=ea->m_xyz[1]||1!=ea->m_xyz[2])
			{
				m_dst->write((uint16_t)pos.type);
				m_dst->write((uint16_t)pos.index);
				for(unsigned i=0;i<3;i++)
				m_dst->write((float32_t)ea->m_xyz[i]);
			}
			pos.index++;
		};
		pos = {Model::PT_Joint,0}; for(auto*ea:modelJoints) f(ea);
		pos = {Model::PT_Point,0}; for(auto*ea:modelPoints) f(ea);
	}

	//2021: Animations
	if(setOffset(MDT_Animations,false))
	{
		writeHeaderA(0x0000,modelAnims.size());

		int anim = -1; for(auto*ab:modelAnims)
		{
			anim++;

			//2020: Filled in afterward.
			auto off1 = m_dst->offset(); 
			m_dst->write((uint32_t)0); //animSize
			
			uint32_t frameCount = ab->_frame_count();

			uint16_t flags = ab->_type;
			if(ab->m_wrap) flags|=MAF_ANIM_WRAP; //8
			
			m_dst->write(flags);
			m_dst->writeBytes(ab->m_name.c_str(),ab->m_name.size()+1);
			m_dst->write((float32_t)ab->m_fps);
			m_dst->write(frameCount);			
			for(uint32_t f=0;f<frameCount;f++)
			m_dst->write((float32_t)ab->m_timetable2020[f]);
			m_dst->write((float32_t)ab->_time_frame());

			uint32_t keyframeMask = model->hasKeyframeData
			(anim,Model::KM_Vertex|Model::KM_Joint|Model::KM_Point);

			m_dst->write(keyframeMask);

			if(keyframeMask&Model::KM_Vertex)
			{
				unsigned fp = ab->m_frame0;
				size_t vcount = modelVerts.size();
				auto *vdata = modelVerts.data();
				for(unsigned f=0;f<frameCount;f++,fp++)				
				for(size_t w=0,v=0;v<vcount;)
				{
					auto cmp = vdata[v]->m_frames[fp]->m_interp2020;
					for(w++;w<vcount&&cmp==vdata[w]->m_frames[fp]->m_interp2020;)
					w++;

					/*Almost forgot this should be extensible like
					//the object keyframes, and it certainly isn't
					//a good idea to waste 3 bytes.
					m_dst->write((uint32_t)cmp);*/
					uint8_t format_descriptor[4] = 
					{
						//3 is the size of each vertex 4B multiple.
						(char)cmp,0,0,cmp>Model::InterpolateCopy?3:0
					};
					m_dst->writeBytes(format_descriptor,4);

					m_dst->write((uint32_t)(w-v));

					if(cmp>Model::InterpolateCopy) for(;v<w;v++)
					{
						//WARNING: This depends on the interpolation model.
						double *coord = vdata[v]->m_frames[fp]->m_coord;
						for(unsigned i=0;i<3;i++) m_dst->write((float32_t)coord[i]);
					}
					else v = w;
				}
			}

			for(int i=1;i<=2;i++) if(keyframeMask&1<<i)
			{				
				uint32_t objectKeyframes = 0;
				for(auto&ea:ab->m_keyframes)
				if(i==ea.first.type)
				objectKeyframes+=ea.second.size();

				m_dst->write(objectKeyframes);

				for(auto&ea:ab->m_keyframes)
				if(i==ea.first.type) for(auto*kf:ea.second)
				{
					//A MOMENTARY FIX
					//MM3D doesn't need this but it's helpful so third-party 
					//loaders don't have to compute it.
					if(kf->m_interp2020==Model::InterpolateCopy)
					{
						double *x[3] = {}; x[kf->m_isRotation>>1] = kf->m_parameter;
						model->interpKeyframe(anim,kf->m_frame,ea.first,x[0],x[1],x[2]);
					}

					//EXTENSIBLE DATA
					//This is an extensible data strategy.
					//Byte 4 is how many 4B words comprise 
					//the rest of the keyframe object. The 
					//rest of the keyframe object's format 
					//is described by Byte 1 & 2 as a unit.
					uint8_t format_descriptor[4] = 
					{
						(char)kf->m_interp2020,0,(char)kf->m_isRotation,4
					};
					m_dst->writeBytes(format_descriptor,4);
					m_dst->write((uint16_t)ea.first.index);
					m_dst->write((uint16_t)kf->m_frame);					
					for(int i=0;i<3;i++) 
					m_dst->write((float32_t)kf->m_parameter[i]);
				}			
			}

			//TODO: Can MisfitOffsetList do this?
			auto off2 = m_dst->offset();
			uint32_t animSize = off2-off1-sizeof(animSize);
			m_dst->seek(off1);
			m_dst->write(animSize);
			m_dst->seek(off2);
		}
		log_debug("wrote %d animations\n",modelAnims.size());
	}

	// Write unknown data (add dirty flag to offset type)
	for(int f=0,fc=model->getFormatDataCount();f<fc;f++)
	{
		Model::FormatData *fd = model->getFormatData(f);
		uint16_t thisType = (fd->offsetType | OFFSET_DIRTY_MASK);
		if(fd->offsetType!=0)
		for(auto&ea:m_offsetList) if(ea.offsetType==thisType)
		{
			ea.offsetValue = m_dst->offset();
			log_warning("setting uknown data type %04x offset at %08x\n",ea.offsetType,ea.offsetValue);
			m_dst->writeBytes(fd->data,fd->len);
			break;
		}
	}

	// Re-write header with offsets

	setOffset(MDT_EndOfFile,false);

	m_dst->seek(12);
	for(auto&ea:m_offsetList)
	{
		m_dst->write(ea.offsetType);
		m_dst->write(ea.offsetValue);
	}
	log_debug("wrote %d updated offsets\n",m_offsetList.size());

	return Model::ERROR_NONE;
}

extern ModelFilter *mm3dfilter(ModelFilter::PromptF f)
{
	auto o = new MisfitFilter; o->setOptionsPrompt(f); return o;
}