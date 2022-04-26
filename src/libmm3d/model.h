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


#ifndef __MODEL_H
#define __MODEL_H

#include "mm3dtypes.h"

#include "glheaders.h"
#include "glmath.h"
#include "bsptree.h"
#include "drawcontext.h"
#include "sorted_list.h"
#include "texture.h"

#ifdef MM3D_EDIT
#include "undomgr.h"
#endif // MM3D_EDIT

// Warning: Here be dragons.
//
// The Model class is the core of MM3D. It contains both the oldest code and
// the newest code. Some very early design decisions were bad decisions. Some of
// those bad decisions are still here causing headaches. I've tried to add some
// documentation to this header file. Despite that,the best way to understand
// how to use the model class is to find reference code elsewhere in MM3D that
// does something similar to what you want to do.
//
// The model represents everything about how the model is rendered as well as some
// properties that are not visible to the user. A model primitive is any sub-object
// within the model (vertices,triangles,bone joints,points,texture projections,
// etc).
//
// All faces in the Model are triangles.
//
// A group can be thought of as a mesh made up of triangles. Materials can be
// assigned to groups (so all triangles in the group share the same material).
// Unlike meshes in some implementations,vertices in MM3D models may be shared
// by triangles in different groups.
//
// A material defines how lighting is reflected off of a group of faces. A material
// may have an associated texture for texture mapping. Occasionally you will see
// the term "texture" used where "material" is meant.
//
// The Model implementation is split among several C++ files,listed below:
//
//	 model.cc: A catch-all for core Model functionality.
//	 model_anim.cc: Animation
//	 model_bool.cc: Boolean operations (union,subtract, intersect)
//	 model_draw.cc: Visual rendering of the model
//	 model_group.cc: Group/Mesh-related functionality
//	 model_influence.cc: Bone joint influences on vertices and points
//	 model_inner.cc: Core functionality for Model's inner classes
//	 model_insert.cc: Undo/redo book-keeping for adjusting indices in primitive lists
//	 model_meta.cc: MetaData methods
//	 model_ops.cc: Model comparison,merging,simplification
//	 model_proj.cc: Texture projections
//	 model_select.cc: Selection logic
//	 model_test.cc: Old tests for file format loading/saving,being replaced
//	 model_texture.cc: Material and texture code
//
// TODO rename Texture -> Material where appropriate
// TODO Make texture creation more consistent

class Model
{
	static unsigned _op; //2022: operationComplete increments this.
	static bool _op_ccw; //2022: Control getOrderOfSelection order.

public:

	enum
	{
		//2022: Going to assume this is obsolete since readFile 
		//uses a 1024B buffer and writeFile imposes no limit.
		//MAX_GROUP_NAME_LEN = 32,
		//2022: Basing this on some structures in mm3dfilter.cc.
		MAX_NAME_LEN = 39,
		MAX_BACKGROUND_IMAGES = 6,
	//	MAX_INFLUENCES = 4, //REMOVE ME
	};
		
	// ChangeBits is used to tell Model::Observer objects what has changed
	// about the model.
	enum ChangeBits
	{
		SelectionChange      = 0x000000FF, // Non-UV selection change
		SelectionVertices    = 0x00000001, // Vertices selection changed
		SelectionFaces       = 0x00000002, // Faces selection changed
		SelectionGroups      = 0x00000004, // Groups selection changed
		SelectionJoints	     = 0x00000008, // Joints selection changed
		SelectionPoints      = 0x00000010, // Points selection changed
		SelectionProjections = 0x00000020, // Projections selection changed
		SelectionUv          = 0x00000100, // setSelectedUv			
		AddGeometry          = 0x00000200, // Add/removed verts/faces/indices
		AddAnimation		 = 0x00000400, // Add/name/move/removed animation
		AddUtility			 = 0x00000800, // addUtility (2022)
		AddOther			 = 0x00001000, // Add/name/removed object/group/material
		SetGroup			 = 0x00002000, // Group membership changed
		SetInfluence         = 0x00004000, // Vertex/point weight changed
		SetTexture           = 0x00008000, // Texture source file changed/refreshed
		MoveGeometry		 = 0x00010000, // Moved vertex
		MoveNormals			 = 0x00020000, // Surface normals changed
		MoveTexture			 = 0x00040000, // Texture coordinates changed
		MoveOther			 = 0x00080000, // Moved object
		AnimationMode        = 0x00100000, // Changed animation mode (or bind mode)
		AnimationSet		 = 0x00200000, // Changed current animation
		AnimationFrame       = 0x00400000, // Changed current animation frame
		AnimationProperty    = 0x00800000, // Set animation times/frames/fps/wrap
		ShowJoints           = 0x01000000, // Joints forced visible
		ShowProjections      = 0x02000000, // Projections forced visible
		ChangeAll			 = 0xFFFFFFFF, // All of the above

		AnimationChange = AnimationMode|AnimationSet|AnimationFrame|AnimationProperty,
	};
	//EXPERIMENTAL
	unsigned getChangeBits(){ return m_changeBits; }
	void setChangeBits(unsigned cb){ m_changeBits|=cb; }
	void unsetChangeBits(unsigned cb){ m_changeBits&=~cb; }

	// FIXME remove this when new equal routines are ready
	enum CompareBits
	{
		CompareGeometry	=	0x01, // Vertices and Faces match
		CompareFaces		=	0x02, // Faces match,vertices may not
		CompareGroups	  =	0x04, // Groups match
		CompareSkeleton	=	0x08, // Bone joints hierarchy matches
		CompareMaterials  =	0x10, // Material properties and group material assignments match
		CompareAnimSets	=	0x20, // Number of animations,frame counts,and fps match
		CompareAnimData	=	0x40, // Animation movements match
		CompareMeta		 =	0x80, // Names and other non-visible data match
		ComparePoints	  =  0x100, // Points match
		CompareInfluences =  0x200, // Vertex and point influences match
		CompareTextures	=  0x400, // Texture coordinates and texture data match
		CompareAll		  = 0xFFFF	// All of the above
	};

	enum ComparePartsE
	{
		PartVertices	= 0x0001, // 
		PartFaces		= 0x0002, // 
		PartGroups		= 0x0004, //NOTE: mm3dfilter.cc IS WRITING THIS TO FILE 
		PartMaterials	= 0x0008, // 
		PartTextures	= 0x0010, // 
		PartJoints		= 0x0020, // 
		PartPoints		= 0x0040, // 
		PartProjections = 0x0080, // 
		PartBackgrounds = 0x0100, // 
		PartMeta		= 0x0200, // 
		PartSkelAnims	= 0x0400, // 
		PartFrameAnims  = 0x0800, // 
		PartAnims       = 0x1000, //2021
		PartFormatData  = 0x2000, // 
		PartFilePaths	= 0x4000, // 
		PartUtilities	= 0x8000, // 
		PartAll			= 0xFFFF, // 

		// These are combinations of parts above, for convenience
		PartGeometry	 = PartVertices | PartFaces | PartGroups, // 
		PartTextureMap  = PartFaces | PartGroups | PartMaterials | PartTextures | PartProjections, // 
		PartAnimations  = PartSkelAnims | PartFrameAnims | PartAnims, // 
	};

	enum PartPropertiesE
	{
		PropName		  = 0x000001, // 
		PropType		  = 0x000002, // 
		//2022: I'm moving these out of 
		//range of PropAllSuitableSuitable
	//	PropSelection	= 0x000004, // should propEqual test this?
	//	PropVisibility  = 0x000008, // 
		PropSelection	= 0x1000004, // should propEqual test this?
		PropVisibility  = 0x1000008, // 
	//	PropFree		  = 0x000010, // 
		PropCoords		= 0x000020, // 
		PropRotation	 = 0x000040, // 
		PropScale		 = 0x000080, // 
		PropInfluences  = 0x000100, // 
		PropWeights	  = 0x000200, // 
		PropNormals	  = 0x000400, // 
		PropTexCoords	= 0x000800, // 
		PropMaterials	= 0x001000, // 
		PropProjections = 0x002000, // 
		PropVertices	 = 0x004000, // 
		PropPoints		= 0x008000, // 
		PropTriangles	= 0x010000, // 
		PropLighting	 = 0x020000, // 
		PropClamp		 = 0x040000, // 
		PropPaths		 = 0x080000, // 
		PropDimensions  = 0x100000, // 
		PropPixels		= 0x200000, // 
		PropTime		  = 0x400000, // 
		PropWrap		  = 0x800000, // 
		PropTimestamps	  = 0x1000000, //2020
		PropGroups		= 0x2000000, //2022
		//PropAllSuitable			= 0xFFFFFF, // 
		PropAllSuitable			= 0xFFFFFFFF, // 
		PropAllSuitableSuitable	= 0xFFFFFF, //2022

		// These are combinations of parts above, for convenience
		/*NOTTE: these properties are most like foils for propEqual tests*/
	//	PropFlags = PropSelection | PropVisibility | PropFree,

		PropKeyframes = PropCoords|PropRotation|PropScale|PropTime|PropType, //2021
	};

	enum PositionTypeE
	{
		PT_Vertex,
		PT_Joint, //1
		PT_Point, //2
		PT_Projection,
		PT_MAX,
		//For the undo system.
		_OT_Background_=PT_MAX,
		PT_ALL = -1,
	};

	enum OperationScopeE
	{
		OS_Selected,
		OS_Global
	};

	enum TextureProjectionTypeE
	{
		TPT_Custom		= -1, // No projection
		TPT_Cylinder	 = 0,
		TPT_Sphere		= 1,
		TPT_Plane		 = 2,
		//TPT_Icosahedron = 3,  // Not yet implemented //???
		TPT_MAX
	};

	enum InfluenceTypeE
	{
		IT_Custom = 0,
		IT_Auto,
		IT_Remainder,
		IT_MAX
	};

	struct InfluenceT
	{
		int m_boneId;
		InfluenceTypeE m_type;
		double m_weight;

		bool operator==(const struct InfluenceT &rhs)const
		{
			return m_boneId==rhs.m_boneId
				&&m_type==rhs.m_type
				&&fabs(m_weight-rhs.m_weight)<0.00001;
		}

		bool operator!=(const struct InfluenceT &rhs)const
		{
			return !(*this==rhs);
		}

		bool operator<(const struct InfluenceT &rhs)const
		{
			if(fabs(m_weight-rhs.m_weight)<0.00001)
				return m_boneId<rhs.m_boneId;
			return m_weight<rhs.m_weight;
		}

		bool operator>(const struct InfluenceT &rhs)const
		{
			if(fabs(m_weight-rhs.m_weight)<0.00001)
				return m_boneId>rhs.m_boneId;
			return m_weight>rhs.m_weight;
		}
	};
	//typedef std::list<InfluenceT> InfluenceList;
	typedef std::vector<InfluenceT> infl_list;
		
	// A position is a common way to manipulate any object that has a
	// single position value. See the PositionTypeE values.
	// The index property is the index in the the list that corresponds
	// to the type.
	class Position
	{
	public:

		PositionTypeE type; unsigned index;

		//2020: I'm not sure if C++ constrains ++ via a 
		//reference operator, so just to be safe...
		unsigned operator++(int){ return index++; }
		//2020
		operator unsigned&(){ return index; } 
		operator unsigned()const{ return index; }

		//SO ANNOYING
		//Dangerous? Does MU_SetObjectKeyframe use this?
		bool operator<(const Position &cmp)const
		{
			return type==cmp.type?index<cmp.index:type<cmp.type;
		}
		bool operator>(const Position &cmp)const
		{
			return type==cmp.type?index>cmp.index:type>cmp.type;
		}
		bool operator==(const Position &cmp)const
		{
			return type==cmp.type&&index==cmp.index;
		}
		bool operator<=(const Position &cmp)const
		{
			return type==cmp.type?index<=cmp.index:type<=cmp.type;
		}
		bool operator>=(const Position &cmp)const
		{
			return type==cmp.type?index>=cmp.index:type>=cmp.type;
		}
		bool operator!=(const Position &cmp)const
		{
			return type!=cmp.type||index!=cmp.index;
		}

		struct hash
		{
			size_t operator()(const Position &p)const
			{	
				return p.type<<24|p.index;
			}
		};
	};
	typedef std::vector<Position> pos_list;

	enum AnimationModeE
	{
		ANIMMODE_NONE = 0,
		ANIMMODE_JOINT, //1 //ANIMMODE_SKELETAL
		ANIMMODE_FRAME, //2
		ANIMMODE = ANIMMODE_JOINT|ANIMMODE_FRAME, //3
		ANIMMODE_Max,
	};

	//RENAME US
	//https://github.com/zturtleman/mm3d/issues/106
	enum Interpolate2020E
	{
		InterpolateKopy =-3, // HACK: See setKeyframe
		InterpolateKeep =-2, // HACK: Default argument
		InterpolateVoid =-1, // HACK: Return value
		InterpolateNone = 0, // Enable sparse animation
		InterpolateCopy = 1, // Enable sparse animation
		InterpolateStep = 2, // Old way before 2020 mod
		InterpolateLerp = 3, // Linear interpolate			
	};		
	enum Interpolant2020E
	{
		InterpolantCoords = 0, //KeyTranslate>>1
		InterpolantRotation = 1, //KeyRotate>>1
		InterpolantScale = 2, //KeyScale>>1 
	};
				
	// Describes the position and normal for a vertex in a frame animation.
	class FrameAnimVertex
	{
	public:
		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static FrameAnimVertex *get();
		void release();
		void sprint(std::string &dest);

		double m_coord[3];
	//	double m_normal[3]; //https://github.com/zturtleman/mm3d/issues/109

		Interpolate2020E m_interp2020;

		bool propEqual(const FrameAnimVertex &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const FrameAnimVertex &rhs)const{ return propEqual(rhs); }

	protected:
		FrameAnimVertex(),~FrameAnimVertex();
		void init();

		static std::vector<FrameAnimVertex*> s_recycle;
		static int s_allocated;
	};

	typedef std::vector<FrameAnimVertex*> FrameAnimVertexList;
		
	struct _OrderedSelection //EXPERIMENTAL
	{
		//2022: This is a helper to implement PolyTool.

	//	static unsigned _op;
		unsigned _select_op;
		operator bool()const{ return _select_op!=0; }
		bool operator=(bool s){ _select_op=s?_op:0; return s; }

	//REMOVE ME //begin/endSelectionDifference?

		union Marker //mutable 
		{
			bool _bool; unsigned _op;

			operator bool&(){ return _bool; }

			void operator=(bool m){ _bool = m; } //C++
			void operator=(_OrderedSelection &s){ _op = s._select_op; }
			bool operator!=(_OrderedSelection &s){ return _op!=s._select_op; }
			bool operator==(_OrderedSelection &s){ return _op==s._select_op; }
		};
		bool operator!=(Marker &m){ return m._op!=_select_op; }
		bool operator==(Marker &m){ return m._op==_select_op; }
	};

	// A triangle represents faces in the model. All faces are triangles.
	// The vertices the triangle is attached to are in m_vertexIndices.
	class Triangle
	{
	public:
		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static Triangle *get();
		void release();
		void sprint(std::string &dest);

		unsigned m_vertexIndices[3];

		// Texture coordinate 0,0 is in the lower left corner of
		// the texture.
		float m_s[3];  // Horizontal,one for each vertex.
		float m_t[3];  // Vertical,one for each vertex.

		double m_finalNormals[3][3];	 // Final normals to draw
	//	double m_vertexNormals[3][3];	// Normals blended for each face attached to the vertex
		double m_flatNormal[3];		  // Normal for this triangle
		double m_vertAngles[3];			// Angle of vertices
		double m_kfFlatNormal[3];		// Flat normal, rotated relative to the animating bone joints
		double m_kfNormals[3][3];		 // Final normals, rotated relative to the animating bone joints
		double m_kfVertAngles[3];
		//Can these be one pointer?
		double *m_flatSource;			 // Either m_flatNormal or m_kfFlatNormal
		double *m_normalSource[3];	  // Either m_finalNormals or m_kfNormals
		double *m_angleSource;			 // Either m_vertAngles or m_kfVertAngles				
		bool m_selected;
		bool m_visible;
		mutable bool m_marked;
		mutable bool m_marked2;
		mutable bool m_userMarked;

		mutable int m_user; //2020: associate an index with a pointer

		int	m_projection;  // Index of texture projection (-1 for none)

		int m_group; //2022

		bool propEqual(const Triangle &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const Triangle &rhs)const{ return propEqual(rhs); }

		void _source(AnimationModeE);

		//NOTE: Flat is a "degenerated" triangle by virtue of 
		//its indices alone. See too deleteFlattenedTriangles.
		bool _flattened()const
		{
			auto *tv = m_vertexIndices; 
			return tv[0]==tv[1]||tv[0]==tv[2]||tv[1]==tv[2];
		}

	protected:
		Triangle(),~Triangle();
		void init();

		static std::vector<Triangle*> s_recycle;
		static int s_allocated;
	};

	typedef std::pair<Triangle*,unsigned> Face;

	// A vertex defines a polygon corner. The position is in m_coord.
	// All triangles in all groups (meshes)references triangles from this
	// one list.
	class Vertex
	{
	public:
		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static Vertex *get(AnimationModeE);
		void release();
		void releaseData(); //2020 ???
		void sprint(std::string &dest);

		double m_coord[3];	  // Absolute vertex location
		double m_kfCoord[3];	// Animated position
		double *m_absSource;  // Points to m_coord or m_kfCoord for drawing
		//mutable bool m_selected;
		mutable _OrderedSelection m_selected;		
		//mutable bool m_marked;
		mutable _OrderedSelection::Marker m_marked;
		mutable bool m_marked2;
		bool m_visible;

		unsigned getOrderOfSelection()const
		{
			return m_selected._select_op; 
		}
		static bool &getOrderOfSelectionCCW()
		{
			return Model::_op_ccw;
		}

		// If m_free is false, the vertex will be implicitly deleted when
		// all faces using it are deleted
		//bool	m_free;
				
		// List of bone joints that move the vertex in skeletal animations.
		infl_list m_influences;

		//Each of these is a triangle and the index held by this vertex in
		//that triangle is encoded in the two bits
		//NOTE: Hoping "small string optimization" is in play for vertices
		//that don't touch many triangles, but the terminator wastes space
		//In that case 4 probably allocates memory
		//https://github.com/zturtleman/mm3d/issues/109
		//Can't be stored as indices.
		//std::basic_string<unsigned> m_faces;
		std::vector<Face> m_faces;
		void _erase_face(Triangle*,unsigned);

		//This change enables editing a model having vertex animation data
		//https://github.com/zturtleman/mm3d/issues/87
		FrameAnimVertexList m_frames;

		bool propEqual(const Vertex &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const Vertex &rhs)const{ return propEqual(rhs); }

		void _source(AnimationModeE),_resample(Model&,unsigned);

	protected:
		Vertex(),~Vertex();
		void init();

		static std::vector<Vertex*> s_recycle;
		static int s_allocated;
	};

	// Group of triangles. All triangles in a group share a material (if one
	// is assigned to the Group). Vertices assigned to the triangles may
	// be shared between triangles in different groups. You can change how
	// normals are blended by modifying m_angle and m_smooth.
	class Group
	{
	public:
		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static Group *get();
		void release();
		void sprint(std::string &dest);

		std::string m_name;
		int m_materialIndex;	 // Material index (-1 for none)

		//FIX ME (TEST ME)
		//Draw code is order-independent. 
		//std::set<int> m_triangleIndices;  // List of triangles in this group
		//std::unordered_set<int> m_triangleIndices;  // List of triangles in this group
		int_list m_triangleIndices;

		// Percentage of blending between flat normals and smooth normals
		// 0 = 0%,255 = 100%
		uint8_t m_smooth;

		// Maximum angle around which triangle normals will be blended
		// (ie,if m_angle = 90,triangles with an edge that forms an
		// angle greater than 90 will not be blended).
		uint8_t m_angle;

		bool m_selected;
	//	bool m_visible; //UNUSED
		mutable bool m_marked;

		bool propEqual(const Group &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const Group &rhs)const{ return propEqual(rhs); }

		int_list m_utils; //2022

		bool _assoc_util(unsigned index, bool how);

	protected:
		Group(),~Group();
		void init();

		static std::vector<Group*> s_recycle;
		static int s_allocated;
	};

	// The Material defines how lighting is reflected off of triangles and
	// may include a texture map.
	class Material
	{
	public:
		enum MaterialTypeE
		{
			MATTYPE_TEXTURE  =  0,  // Texture mapped material
			MATTYPE_BLANK	 =  1,  // Blank texture,lighting only
			MATTYPE_COLOR	 =  2,  // Unused
			MATTYPE_GRADIENT =  3,  // Unused
			MATTYPE_Max		=  4	 // For convenience
		};

		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static Material *get();
		void release();
		void sprint(std::string &dest);

		std::string	m_name;
		MaterialTypeE m_type;			// See MaterialTypeE above

		// Lighting values (RGBA,0.0 to 1.0)
		float m_ambient[4];
		float m_diffuse[4];
		float m_specular[4];
		float m_emissive[4];

		// Lighting value 0 to 100.0
		float m_shininess;

		uint8_t m_color[4][4];  // Unused


		// The clamp properties determine if the texture map wraps when crossing
		// the 0.0 or 1.0 boundary (false)or if the pixels along the edge are
		// used for coordinates outside the 0.0-1.0 limits (true).
		bool m_sClamp;  // horizontal wrap/clamp
		bool m_tClamp;  // vertical wrap/clamp

		bool m_accumulate; //2021: standard "additive" blend model

		// Open GL texture index (not actually used in model editing
		// viewports since each viewport needs its own texture index)
		GLuint m_texture;

		std::string	m_filename;		 // Absolute path to texture file (for MATTYPE_TEXTURE)
		std::string	m_alphaFilename;  // Unused (ms3dfilter.cc)
		Texture *m_textureData;	 // Texture data (for MATTYPE_TEXTURE)

		bool propEqual(const Material &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const Material &rhs)const{ return propEqual(rhs); }

		//2021: model_draw.cc was only considering RGBA textures
		bool needsAlpha() 
		{
			//Note, glMaterial docs say only diffuse alpha matters
			return m_accumulate||m_diffuse[3]!=1.0f
			||m_type==MATTYPE_TEXTURE //m_textureData?
			&&m_textureData->m_format==Texture::FORMAT_RGBA;
		}

	protected:
		Material(),~Material();
		void init();

		static std::vector<Material*> s_recycle;
		static int s_allocated;
	};
				
	enum KeyType2020E
	{
		KeyAny = -1,
		KeyTranslate = 1, //InterpolantCoords<<1
		KeyRotate = 2, //InterpolantRotation<<1
		KeyScale = 4, //InterpolantScale<<1
	};
	//Note: I didn't want to all this PositionMask
	//because PM_Vertex seemed too easily confused
	//for PT_Vertex, too easy to input by accident
	enum KeyMask2021E
	{
		KM_None = 0,
		KM_Vertex = 1<<PT_Vertex,
		KM_Joint = 1<<PT_Joint,
		KM_Point = 1<<PT_Point,
	};
	// A keyframe for a single joint in a single frame. Keyframes may be rotation or 
	// translation (you can set one without setting the other).
	class Keyframe
	{
	public:
		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static Keyframe *get();

		void release();
		void sprint(std::string &dest);

		Position m_objectIndex; // Joint that this keyframe affects
							//on a per frame basis and frame numbers are implicit.
		unsigned m_frame;	 // Frame number for this keyframe

//		double m_time;	// Time for this keyframe in seconds

		double m_parameter[3];  // Translation or rotation (radians),see m_isRotation

		//bool m_isRotation;
		KeyType2020E m_isRotation;	 // Indicates if m_parameter describes rotation (true)or translation (false)

		Interpolate2020E m_interp2020;

		//CAREFUL: This doesn't work with bitwise combinations of KeyType2020E.
		bool operator<(const Keyframe &rhs)const
		{
			return m_frame!=rhs.m_frame?m_frame<rhs.m_frame:m_isRotation<rhs.m_isRotation;
		}
		bool operator==(const Keyframe &rhs)const
		{
			return m_frame==rhs.m_frame&&m_isRotation&rhs.m_isRotation;
		}
		bool propEqual(const Keyframe &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;

//	protected:

		Keyframe(),~Keyframe();

	protected:

		void init(); //UNUSED (NOP)

		static std::vector<Keyframe*> s_recycle;
		static int s_allocated;
	};
				
	struct Object2020 //RENAME ME
	{
		//https://github.com/zturtleman/mm3d/issues/114
		
		std::string m_name;

		//NOTE: calculateAnim assumes stored back-to-back.
		double m_abs[3]; //Absolute position
		double m_rot[3]; //Relative rotation
		double m_xyz[3]; //Nonunimform scale

		double *m_absSource; //Absolute position
		double *m_rotSource; //Relative rotation
		double *m_xyzSource; //Nonunimform scale

		Matrix getMatrix()const;
		Matrix getMatrixUnanimated()const;

		bool m_selected;

		PositionTypeE m_type; //getParams

		//WARNING: Return relative values for keyframes
		const double *getParams(Interpolant2020E)const;
		const double *getParamsUnanimated(Interpolant2020E)const;
		void getParams(double abs[3], double rot[3], double xyz[3])const;

	protected:

		void init(PositionTypeE t);
	};

	// A bone joint in the skeletal structure,used for skeletal animations.
	// When a vertex or point is assigned to one or more bone joints with a specified
	// weight,the joint is referred to as an Influence.
	class Joint : public Object2020
	{
	public:

		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static Joint *get(AnimationModeE);
		void release();
		void sprint(std::string &dest);
			
		//2020: This is complicated because historically
		//getPositionCoords, etc. works in absolute coordinates
		//for translation but not rotation.
		//double m_localRotation[3];     // Rotation relative to parent joint (or origin if no parent)
		//double m_localTranslation[3];  // Translation relative to parent joint (or origin if no parent)
		//TODO: Computable?
		double m_rel[3];
		double m_kfRel[3];
		//m_rotSource COMPAT			
		double m_kfRot[3];
		double m_kfXyz[3];
		//MEMORY OPTIMIZATION
		double *m_kfAbs(){ return m_final.getVector(3); }

		Matrix m_absolute;  // Absolute matrix for the joint's original position and orientation
		Matrix m_relative;  // Matrix relative to parent joint
		Matrix m_final;	  // Final animated absolute position for bone joint

		int m_parent;  // Parent joint index (-1 for no parent)

		//bool m_selected;
		bool m_visible;
		mutable bool m_marked;

		//2020 (drawJoints)
		//TODO: Needs Undo objects. Remove it mutable status.
		mutable bool m_bone;

		//TODO: If Euler angles are easily inverted it makes
		//more sense to just calculate m_inv in validateSkel.
		int _dirty_mask;
		Matrix _dirty_mats[3];
		Matrix &getAbsoluteInverse() //2020
		{
			if(_dirty_mask&1)
			{
				_dirty_mask&=~1;
				_dirty_mats[0] = m_absolute.getInverse();
			}
			return _dirty_mats[0];
		}
		Matrix &getSkinMatrix() //2021
		{
			if(_dirty_mask&2)
			{
				_dirty_mask&=~2;
				_dirty_mats[1] = getAbsoluteInverse()*m_final;
			}
			return _dirty_mats[1];
		}
		Matrix &getSkinverseMatrix() //2021
		{
			if(_dirty_mask&4)
			{
				_dirty_mask&=~4;
				_dirty_mats[2] = getSkinMatrix().getInverse();
			}
			return _dirty_mats[2];
		}

		bool propEqual(const Joint &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const Joint &rhs)const{ return propEqual(rhs); }

		void _source(AnimationModeE);

	protected:

		Joint(),~Joint();
		void init();

		static std::vector<Joint*> s_recycle;
		static int s_allocated;
	};

	class Point : public Object2020
	{
	public:

		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static Point *get(AnimationModeE);
		void release();
		void sprint(std::string &dest);
	
		//UNUSED: Note old MM3D files have garbage written to this field
		//since the MM3DFILE_PointT wasn't zero-initalized.
		//int m_type;

		//double m_trans[3];    // Position
        //double m_rot[3];      // Rotation

		//NOTE: calculateAnim assumes stored back-to-back.
		double m_kfAbs[3];  // Animated position
		double m_kfRot[3];	 // Animated rotation
		double m_kfXyz[3]; // Animated scale

		// These pointers point to the unanimated or animated properties
		// depending on whether or not the model is animated when it is drawn.
		//double *m_absSource;  // m_abs or m_kfAbs
		//double *m_rotSource;	// m_rot or m_kfRot

		//bool m_selected;
		bool m_visible;
		mutable bool m_marked;

		// List of bone joints that move the point in skeletal animations.
		infl_list m_influences;

		void _resample(Model&,unsigned); //2020

		bool propEqual(const Point &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const Point &rhs)const{ return propEqual(rhs); }

		void _source(AnimationModeE);

	protected:

		Point(),~Point();
		void init();

		static std::vector<Point*> s_recycle;
		static int s_allocated;
	};

	// A TextureProjection is used automatically map texture coordinates to a group
	// of vertices. Common projection types are Sphere,Cylinder,and Plane.
	class TextureProjection : public Object2020
	{
	public:

		static int flush();
		static int allocated(){ return s_allocated; }
		static void stats();
		static TextureProjection *get();
		void release();
		void sprint(std::string &dest);

		int m_type;				// See TextureProjectionTypeE
			
		//double m_abs[3];

		//Object2020 makes these fields editor friendly.
		//https://github.com/zturtleman/mm3d/issues/114
		//double m_upVec[3];	  // Vector that defines "up" for this projection
		//double m_seamVec[3];	// Vector that indicates where the texture wraps from 1.0 back to 0.0

		double m_range[2][2];  // min/max,x/y

		//bool m_selected;
		mutable bool m_marked;

		bool propEqual(const TextureProjection &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const TextureProjection &rhs)const{ return propEqual(rhs); }

	protected:

		TextureProjection(),~TextureProjection();
		void init();

		static int s_allocated;
	};

	// TODO: Probably should use a map for the KeyframeList
	typedef sorted_ptr_list<Keyframe*> KeyframeList;		
	//typedef std::vector<KeyframeList> ObjectKeyframeList;
	typedef std::unordered_map<Position,KeyframeList,Position::hash> ObjectKeyframeList;

	class Animation //RENAME ME
	{
	public:

		int _type; //AnimationModeE

		std::string m_name;
			
		//TODO: Seems a map is called for?
		ObjectKeyframeList m_keyframes;

		double m_fps;  // Frames per second
		bool m_wrap; // Whether or not the animation uses wraparound keyframe interpotion
		//UNUSED
		//unsigned m_frameCount;	 // Number of frames in the animation				
		//bool	  m_validNormals;  // Whether or not the normals have been calculated for the current animation frame

		//https://github.com/zturtleman/mm3d/issues/106
		//Usually use _time_frame() to access this 
		//If -1 (less than 0) m_timetable2020.size is used
		//to support older MM3D files that didn't have any
		//timestamps
		double m_frame2020;
		std::vector<double> m_timetable2020;

		//UPDATE: Since mode 3 is added this is ~0 for animations without 
		//vertex data. If ~m_frame0 is 0 it must be left alone.
		// Each element in m_frameData is one frame. The frames hold lists of
		// all vertex positions and point positions.
		//FrameAnimDataList m_frameData;
		unsigned m_frame0;
			
		size_t _frame_count()const
		{
			return m_timetable2020.size(); 
		}
		double _time_frame()const //REMOVE ME
		{
			//TODO: this workaround can be retired if legacy
			//loaders did setAnimTimeFrame(frames)
			return m_frame2020>=0?m_frame2020:_frame_count();
		}
		double _frame_time(unsigned frame)const
		{
			return frame<_frame_count()?m_timetable2020[frame]:0;
		}
		unsigned _frame0(Model *m)const
		{
			if(0==~m_frame0) m->_anim_valloc(this); return m_frame0;
		}
						
		bool propEqual(const Animation &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const Animation &rhs)const{ return propEqual(rhs); }

	public:
		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static Animation *get();
		void release();
		void releaseData();
		void sprint(std::string &dest);

	protected:
		Animation(),~Animation();
		void init();

		static std::vector<Animation*> s_recycle;
		static int s_allocated;
	};
	//TEMPORARY FIX
	Animation *_anim(unsigned,AnimationModeE=ANIMMODE_NONE)const;
	Animation *_anim(unsigned,unsigned,Position,bool=true)const;
	bool _anim_check(bool model_status_report=false);
	void _anim_valloc(const Animation *lazy_mutable);
	bool _skel_xform_abs(int inv,infl_list&,Vector&v);
	bool _skel_xform_rot(int inv,infl_list&,Matrix&m);
	bool _skel_xform_mat(int inv,infl_list&,Matrix&m);

	/*REFERENCE
	// Describes a skeletal animation.
	class SkelAnim : public Animation
	{
	public:
		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static SkelAnim *get();
		void release();
		//void releaseData();
		void sprint(std::string &dest);

	protected:
		SkelAnim(),~SkelAnim();
		//void init();

		static std::vector<SkelAnim*> s_recycle;
		static int s_allocated;
	};*/
		
	/*REFERENCE
	// Contains the list of vertex positions and point positions for each vertex
	// and point for one animation frame.
	class FrameAnimData
	{
	public:
		FrameAnimData():m_frameVertices(),m_framePoints()
		{}
		FrameAnimVertexList m_frameVertices;
		FrameAnimPointList m_framePoints;

		//https://github.com/zturtleman/mm3d/issues/106				  
		double m_frame2020;

		void releaseData();

		bool propEqual(const FrameAnimData &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const FrameAnimData &rhs)const{ return propEqual(rhs); }
	};
	typedef std::vector<FrameAnimData*> FrameAnimDataList;*/

	/*REFERENCE
	// Describes a frame animation (also known as "Mesh Deformation Animation").
	// This object contains a list of vertex positions for each vertex for every
	// frame (and also every point for every frame).
	class FrameAnim : public Animation
	{
	public:
		static int flush();
		static int allocated(){ return s_allocated; }
		static int recycled(){ return s_recycle.size(); }
		static void stats();
		static FrameAnim *get();
		void release();
		//void releaseData();
		void sprint(std::string &dest);

		// Each element in m_frameData is one frame. The frames hold lists of
		// all vertex positions and point positions.
		//FrameAnimDataList m_frameData;
		unsigned m_frame0;

	protected:
		FrameAnim(),~FrameAnim();
		//void init();

		static std::vector<FrameAnim*> s_recycle;
		static int s_allocated;
	};*/

	// Reference background images for canvas viewports.
	class BackgroundImage : public Object2020
	{
	public:
		
		BackgroundImage()
		{
			Object2020::init(_OT_Background_);

			m_xyz[0] = 30; /*???*/ 
		}

		//std::string m_filename;
		//float m_scale;      // 1.0 means 1 GL unit from the center to the edges of the image
		//float m_center[3];  // Point in the viewport where the image is centered

		void sprint(std::string &dest);

		bool propEqual(const BackgroundImage &rhs, int propBits=PropAllSuitable, double tolerance=0.00001)const;
		bool operator==(const BackgroundImage &rhs)const{ return propEqual(rhs); }
	};

	//https://github.com/zturtleman/mm3d/issues/56
	struct ViewportUnits
	{
		enum{ VertexSnap=1,UnitSnap=2 };
		enum{ UvSnap=4,SubpixelSnap=8 };

		int snap;
			
		enum{ BinaryGrid=0,DecimalGrid,FixedGrid };

		int grid;

		int lines3d;

		double inc,inc3d,ptsz3d;

		int xyz3d; //1|2|4

		int unitsUv;

		double snapUv[2];

		bool no_overlay_option;

		ViewportUnits(){ memset(this,0x00,sizeof(*this)); }
	};

	typedef Model::Vertex *VertexPtr;
	typedef Model::Triangle *TrianglePtr;
	typedef Model::Group *GroupPtr;
	typedef Model::Material *MaterialPtr;
	//typedef Model::Joint *JointPtr;

	// FormatData is used to store data that is used by specific file formats
	// that is not understood by MM3D.
	class FormatData
	{
	public:

		FormatData():offsetType(),index(),len(),data()
		{};
		virtual ~FormatData(); //???

		uint16_t offsetType;  // 0 = none,is valid
		std::string	format;	 // Should not be empty
		uint32_t index;		// for formats with multiple data sets
		uint32_t len;		// length of data in 'data'
		uint8_t *data;		// pointer to data

		virtual void serialize();
	};

	// Arbitrary key/value string pairs. This is used to provide a simple interface
	// for model attributes that are not manipulated by commands/tools. Often
	// user-editable format-specific data is stored as MetaData key/value pairs
	// (for example texture paths in MD2 and MD3 models).
	class MetaData
	{
	public: std::string key,value;
	};
	typedef std::vector<MetaData> MetaDataList;

	enum UtilityTypeE
	{
		UT_NONE=0,
		UT_UvAnimation, //NOTE: Written to file.
		UT_MAX
	};

	//2022: A utility is a more obscure feature that's not a plugin (although
	//it may have a plugin interface if necessary) or key-value pair MetaData.
	class Utility
	{
	public:

		//These aren't recycled right now.
		static Utility *get(UtilityTypeE); //model_meta.cc
		void release();

		//This is for access to m_undoMgr/m_changeBits
		//to avoid passing (or mixing) a Model pointer.
		Model * model; 

		std::string name;

		const UtilityTypeE type;

		const int assoc;

		Utility(UtilityTypeE t, int a)
		:model(),type(t),assoc((ComparePartsE)a)
		{}
		virtual ~Utility(){ /*NOP*/ }
	
	protected:
		
		template<class T>
		inline void _set(const T *m, const T &v)const
		{
			_set(m,&v,sizeof(T));
		}
		template<class T>
		inline bool _set_if(bool cond, const T *m, const T &v)const
		{
			if(!cond){ assert(0); return false; }
			else _set(m,&v,sizeof(T)); return true;
		}
		template<int N,class T> 
		static inline T(&_pack(T*v))[N]{ return *(T(*)[N])v; }

		template<class T> 
		static inline T &_set_cast(const T &m){ return const_cast<T&>(m); }

		void _set(const void *p, const void *cp, size_t sz)const;
	};
	class UvAnimation : public Utility
	{
	public:

		double fps;
		double frames;
		double unit,vnit;
		bool wrap;
		struct Key /* Nothing too complicated? */
		{
			double frame;

			//I guess rotate, scale, translate order
			//since it's what the existing animation
			//code does. I thought about using a 2x3 
			//matrix to shear but I worry there's no
			//way to interpolate it, with or without
			//a rotation component.
			//double mat[3][2];
			double rz,sx,sy,tx,ty;			
			Interpolate2020E r,s,t; 

			bool operator<(const Key &b)const
			{
				return frame<b.frame;
			}

			Key():rz(),sx(1),sy(1),tx(),ty()
			{
				frame = 1; //For basic 0~1 animation.
				r = s = t = InterpolateNone;
			}
		};
		std::vector<Key> keys;

		UvAnimation():Utility(UT_UvAnimation,PartGroups),
		fps(1),frames(1),unit(1),vnit(1),wrap()
		{}

		bool set_fps(double)const;
		bool set_frames(double)const;
		bool set_units(double,double)const;
		void set_wrap(bool)const;
		int set_key(const Key&)const;
		bool delete_key(unsigned)const;

	public: //These aren't saved state.

		mutable Key _cur_key; //validateAnim intermediate values

		mutable Matrix _cur_texture_matrix; //validateAnim matrix

		double _cur_time;
		void _dirty()const,_make_cur()const;
		void _interp_keys(Key&,double)const;
		void _refresh_texture_matrix()const;
	};

	// See errorToString()
	enum ModelErrorE
	{
		ERROR_NONE = 0,
		ERROR_CANCEL,
		ERROR_UNKNOWN_TYPE,
		ERROR_UNSUPPORTED_OPERATION,
		ERROR_BAD_ARGUMENT,
		ERROR_NO_FILE,
		ERROR_NO_ACCESS,
		ERROR_FILE_OPEN,
		ERROR_FILE_READ,
		ERROR_BAD_MAGIC,
		ERROR_UNSUPPORTED_VERSION,
		ERROR_BAD_DATA,
		ERROR_UNEXPECTED_EOF,
		ERROR_FILTER_SPECIFIC,
		ERROR_EXPORT_ONLY,
		ERROR_UNKNOWN
	};

	enum SelectionModeE
	{
		SelectNone,
		SelectVertices,
		SelectTriangles,
		SelectConnected,
		SelectGroups,
		SelectJoints,
		SelectPoints,
		SelectProjections,
	};

	//REMOVE ME
	/*2019: Removing this to remove dependency on ViewRight/Left.
	//It is used only by implui/texturecoord.cc. It's not a big enough piece of Model to warrant
	//existing. It is an argument to Model::getVertexCoords2d.
	// Canvas drawing projections (not related to TextureProjections)
	enum ProjectionDirectionE
	{
		View3d = 0,
		ViewFront,
		ViewBack,
		ViewLeft,
		ViewRight,
		ViewTop,
		ViewBottom
	};*/

	// Drawing options. These are defined as powers of 2 so that they
	// can be combined with a bitwise OR.
	enum
	{
		DO_NONE			  = 0x00, // FLAT
		DO_TEXTURE		  = 0x01,
		DO_SMOOTHING		= 0x02, // Normal smoothing/blending between faces
		DO_WIREFRAME		= 0x04,
		DO_BADTEX			= 0x08, // Render bad textures,or render as no texture
		DO_ALPHA			 = 0x10, // Do alpha blending
		DO_BACKFACECULL	= 0x20, // Do not render triangles that face away from the camera

		DO_BONES = 0x40, //2019 //Removing DrawJointModeE

		DO_ALPHA_DEFER_BSP = 0x80, //2021: Use draw_bspTree to manually draw transparency

		DO_TEXTURE_MATRIX = 0x100, //2022: UvAnimation mode
	};

	//REMOVE ME
	/*https://github.com/zturtleman/mm3d/issues/56
	enum DrawJointModeE
	{
		JOINTMODE_NONE = 0,
		JOINTMODE_LINES,
		JOINTMODE_BONES,
		JOINTMODE_Max
	};*/

	enum AnimationMergeE
	{
		AM_NONE = 0,
		AM_ADD,
		AM_MERGE
	};

	enum BooleanOpE
	{
		BO_Union,
		BO_UnionRemove,
		BO_Subtraction,
		BO_Intersection,
		BO_MAX
	};

#ifdef MM3D_EDIT
	// Register an observer if you have an object that must be notified when the
	// model changes. The modelChanged function will be called with changeBits
	// set to describe (in general terms)what changed. See ChangeBits.
	class Observer
	{
		public:
			//virtual ~Observer(){} //???
			virtual void modelChanged(int changeBits)= 0;
	};
	typedef std::vector<Observer*> ObserverList;

#endif // MM3D_EDIT

	// ==================================================================
	// Public methods
	// ==================================================================

	Model(),~Model(); //virtual ~Model();

	static const char *errorToString(Model::ModelErrorE,Model *model = nullptr);
	static bool operationFailed(Model::ModelErrorE);

	// Returns if models are visually equivalent
	bool equivalent(const Model *model, double tolerance = 0.00001)const;

	// Compares if two models are equal. Returns true of all specified
	// properties of all specified parts match. See ComparePartsE and
	// PartPropertiesE.
	bool propEqual(const Model *model, int partBits = PartAll, int propBits = PropAllSuitable,
			double tolerance = 0.00001)const;

	//model_print.cc
	void sprint(std::string &dest); //???

	// ------------------------------------------------------------------
	// "Meta" data,model information that is not rendered in a viewport.
	// ------------------------------------------------------------------

	// Indicates if the model has changed since the last time it was saved.
	void setSaved(){ m_undoMgr->setSaved(); }
	bool getSaved()const{ return m_undoMgr->isSaved(); }
	bool getEdited()const{ return m_undoMgr->isEdited(); }

	//RENAME ME ("filename" is misleading.)
	const char *getFilename()const
	{
		return m_filename.c_str(); 
	}
	void setFilename(const char *filename)
	{ 
		if(filename&&*filename) m_filename = filename; 
	}

	const char *getExportFile()const
	{
		return m_exportFile.c_str(); 
	}
	void setExportFile(const char *filename)
	{ 
		if(filename&&*filename) m_exportFile = filename;
	}

	void setFilterSpecificError(const char *str){ s_lastFilterError = m_filterSpecificError = str; };
	const char *getFilterSpecificError()const { return m_filterSpecificError.c_str(); };
	static const char *getLastFilterSpecificError(){ return s_lastFilterError.c_str(); };

	// Observers are notified when the model changes. See the Observer class
	// for details.
	void addObserver(Observer *o);
	void removeObserver(Observer *o);

	// See the MetaData class.
	void addMetaData(const char *key, const char *value);
	void updateMetaData(const char *key, const char *value);

	// Look-up by key
	bool getMetaData(const char *key,char *value, size_t valueLen)const;
	// Look-up by index
	//REMOVE ME (TRUNCATES, INEFFICIENT)
	bool getMetaData(unsigned int index,char *key, size_t keyLen,char *value, size_t valueLen)const;
	//NEW
	MetaData &getMetaData(unsigned int index){ return m_metaData[index]; }
	const MetaData &getMetaData(unsigned int index)const{ return m_metaData[index]; }

	unsigned int getMetaDataCount()const;
	void clearMetaData();
	void removeLastMetaData();  // For undo only!

	//2022: Utilities are adjacent to MetaData.
	//
	// NOTE: Use getUtilityList to read values.
	// 
	// So far there's only one type of utility.
	// I think I might add one for controlling
	// surface normals. I'm not sure what else.
	//
	int addUtility(UtilityTypeE type, const char *name);
	bool deleteUtility(unsigned index);
	void _insertUtil(unsigned,Utility*),_removeUtil(unsigned);
	bool setUtilityName(unsigned index, const char *name);
	bool convertUtilityToType(Model::UtilityTypeE, unsigned index);
	bool moveUtility(unsigned oldIndex, unsigned newIndex);
	bool addGroupToUtility(unsigned index, unsigned group, bool how=true);
	bool removeGroupFromUtility(unsigned index, unsigned group) //UNUSED
	{
		return addGroupToUtility(index,group,false);
	}

	// Background image accessors. See the BackgroundImage class.
	bool setBackgroundImage(unsigned index, const char *str);
	bool setBackgroundScale(unsigned index, double scale);
	bool setBackgroundCoords(unsigned index, const double abs[3]);
	bool setBackgroundCenter(unsigned index, double x, double y, double z); //LEGACY

	const char *getBackgroundImage(unsigned index)const;
	double getBackgroundScale(unsigned index)const;
	bool getBackgroundCoords(unsigned index, double abs[3])const;
	bool getBackgroundCenter(unsigned index, float &x, float &y, float &z)const; //LEGACY

	// These are used to store status messages when the model does not have a status
	// bar. When a model is assigned to a viewport window,the messages will be
	// displayed in the status bar.
	bool hasErrors()const{ return !m_loadErrors.empty(); }
	void pushError(const std::string &err);
	bool popError(std::string &err);

	// Use these functions to preserve data that MM3D doesn't support natively
	int  addFormatData(FormatData *fd);
	bool deleteFormatData(unsigned index);
	unsigned getFormatDataCount()const;
	FormatData *getFormatData(unsigned index)const;
	FormatData *getFormatDataByFormat(const char *format, unsigned index = 0)const; // not case sensitive

	// ------------------------------------------------------------------
	// Rendering functions
	// ------------------------------------------------------------------

	// Functions for rendering the model in a viewport
	void draw(unsigned drawOptions=DO_TEXTURE,ContextT context=nullptr, double viewPoint[3]=nullptr);
	void draw_bspTree(unsigned drawOptions, ContextT context, double viewPoint[3]);
	BspTree &draw_bspTree(){ return m_bspTree; }
	void drawLines(float alpha=1);
	void drawVertices(float alpha=1);
	void _drawPolygons(int); //2019
	void drawPoints();
	void drawProjections();
	void drawJoints(float alpha=1, float axis=0);
	struct _draw;
	void _drawMaterial(_draw&, int g);

	//NOTE: m is ModelViewport::ViewOptionsE
	void setCanvasDrawMode(int m){ m_canvasDrawMode = m; };
	int getCanvasDrawMode()const { return m_canvasDrawMode; };

	//https://github.com/zturtleman/mm3d/issues/56
	void setDrawOption(int mode, bool set)
	{
		if(set) m_drawOptions|=mode;
		if(!set) m_drawOptions&=~mode;
	}
	int getDrawOptions()const{ return m_drawOptions; };

	void setPerspectiveDrawMode(int m){ m_perspectiveDrawMode = m; };
	int getPerspectiveDrawMode()const{ return m_perspectiveDrawMode; };

	void setDrawJoints(bool o, int cb=ShowJoints)
	{
		if(!cb||m_drawJoints!=o)
		{
			m_drawJoints = o; m_changeBits|=cb;
		}
	}
	bool getDrawJoints()const{ return m_drawJoints; }

	void setDrawSelection(bool o){ m_drawSelection = o; }
	bool getDrawSelection()const{ return m_drawSelection; }
		
	void setDrawProjections(bool o, int cb=ShowProjections)
	{
		if(!cb||m_drawProjections!=o)
		{
			m_drawProjections = o; m_changeBits|=cb;
		}
	};
	bool getDrawProjections()const{ return m_drawProjections; };

	// Open GL needs textures allocated for each viewport that renders the textures.
	// A ContextT associates a set of OpenGL textures with a viewport.
	bool loadTextures(ContextT context = nullptr);
	void removeContext(ContextT context);

	// Forces a reload and re-initialization of all textures in all
	// viewports (contexts)
	void invalidateTextures(); //OVERKILL

	// Counter that indicates how many OpenGL textures are currently allocated.
	static int	 s_glTextures;

	// The local matrix is used by non-mm3d applications that use this
	// library to render MM3D models. In MM3D it should always be set to the
	// identity matrix. Other apps use it to move an animated model to a new
	// location in space, it only affects rendering of skeletal animations.
	void setLocalMatrix(const Matrix &m){ m_localMatrix = m; };

	// ------------------------------------------------------------------
	// New "Position" functions (2020)
	// ------------------------------------------------------------------

	Object2020 *getPositionObject(const Position &pos);
	const Object2020 *getPositionObject(const Position &pos)const
	{
		return ((Model*)this)->getPositionObject(pos);
	}

	bool setPositionName(const Position &pos, const char *name);		
	bool setPositionCoords(const Position &pos, const double coords[3]);
	bool setPositionCoordsUnanimated(const Position &pos, const double coords[3]);
	bool setPositionRotation(const Position &pos, const double rot[3]);
	bool setPositionRotationUnanimated(const Position &pos, const double rot[3]);
	bool setPositionScale(const Position &pos, const double scale[3]);
	bool setPositionScaleUnanimated(const Position &pos, const double scale[3]);

	const char *getPositionName(const Position &pos)const;
	bool getPositionCoords(const Position &pos, double coords[3])const;
	bool getPositionCoordsUnanimated(const Position &pos, double coords[3])const;
	bool getPositionRotation(const Position &pos, double rot[3])const;
	bool getPositionRotationUnanimated(const Position &pos, double rot[3])const;
	bool getPositionScale(const Position &pos, double scale[3])const;
	bool getPositionScaleUnanimated(const Position &pos, double scale[3])const;		

	// ------------------------------------------------------------------
	// Animation functions
	// ------------------------------------------------------------------

	struct RestorePoint //2021
	{
		unsigned anim; AnimationModeE mode; double time; 

		bool operator==(const RestorePoint &cmp)const
		{
			return 0==memcmp(this,&cmp,sizeof(*this));
		}
		bool operator!=(const RestorePoint &cmp)const
		{
			return 0!=memcmp(this,&cmp,sizeof(*this));
		}
	};
	RestorePoint makeRestorePoint()const
	{
		return{m_currentAnim,m_animationMode,m_elapsedTime};
	}

	//WARNING: This does not call validatAnim, so getVertexCoords
	//for example will hold old values. Setting the time or frame
	//will force validation.
	bool setCurrentAnimation(unsigned index, AnimationModeE=ANIMMODE);
	bool setCurrentAnimation(const RestorePoint&);
	bool setAnimationMode(AnimationModeE m)
	{
		auto rp = makeRestorePoint(); rp.mode = m;
		return setCurrentAnimation(rp);
	}

	enum AnimationTimeE //OPTIMIZATION
	{	
		//NOTE: These are bitwise but precedence is enforced so that
		//it doesn't make sense to mix them.
		//TODO: Might want to offer a mode that keeps normals static.
		//Problem is the BSP-tree feature needs face-normals, so the
		//face-normals must have their own "m_valid" status to do so.

		AT_calculateAnim=1, //Ignore normals calculation.
		AT_calculateNormals=1|2, //Calculate all (default).
		AT_invalidateNormals=1, //Ignore normals calculation.
		AT_invalidateAnim=0, //Just set the time.
	};

	bool setCurrentAnimationFrame(unsigned frame, AnimationTimeE=AT_calculateNormals);
	bool setCurrentAnimationFrameTime(double time, AnimationTimeE=AT_calculateNormals); 
	bool setCurrentAnimationElapsedFrameTime(double time, AnimationTimeE=AT_calculateNormals); 
	bool setCurrentAnimationTime(double seconds, int loop=-1, AnimationTimeE=AT_calculateNormals);

	//2020: Try to make a new frame at the current time.
	//The returned value is the new current frame which
	//may or may not be a different value since usually
	//the value will be 1 more than the current but not
	//always when inserting to the front.
	unsigned makeCurrentAnimationFrame()
	{
		if(!m_animationMode) return 0; //2021
		auto cmp = insertAnimFrame(m_currentAnim,m_currentTime);
		if(cmp!=m_currentFrame) setCurrentAnimationFrame(cmp,AT_invalidateAnim);
		return cmp;
	}

	unsigned getCurrentAnimation()const{ return m_currentAnim; }
	unsigned getCurrentAnimationFrame()const{ return m_currentFrame; }
	//2020: Internally the time is stored in frame units so it's possible to compare
	//the value to the frames' timestamps.
	double getCurrentAnimationFrameTime()const{ return m_currentTime; }
	double getCurrentAnimationElapsedFrameTime()const{ return m_elapsedTime; }
	double getCurrentAnimationTime()const;

	// Stop animation mode; Go back to standard pose editing.
	void setNoAnimation(){ setCurrentAnimation(m_currentAnim,ANIMMODE_NONE); }
	AnimationModeE getAnimationMode()const{ return m_animationMode; }
	bool inAnimationMode()const{ return m_animationMode!=ANIMMODE_NONE; }	
	bool inFrameAnimMode()const{ return 0!=(m_animationMode&ANIMMODE_FRAME); }
	bool inJointAnimMode()const{ return 0!=(m_animationMode&ANIMMODE_JOINT); }
	bool inSkeletalMode()const{ return m_skeletalMode; }
	bool setSkeletalModeEnabled(bool); //2022
	bool getSkeletalModeEnabled(bool){ return m_skeletalMode2; }

	// Common animation properties
	int addAnimation(AnimationModeE mode, const char *name);
	void deleteAnimation(unsigned index);

	//THESE ARE NEW TO ALLOW FOR ABSOLUTE ANIMATION INDICES.
	//"getAnimCount" IS RENAMED SO "getAnim" METHODS ALWAYS
	//GET ANIMATION MEMBER DATA.

	//2021: Converts an animation type into the first valid
	//absolute index of that type. The number of animations
	//is got by getAnimationCount. (Which may be 0!)
	unsigned getAnimationIndex(AnimationModeE)const;
	unsigned getAnimationCount(AnimationModeE)const; //getAnimCount
	unsigned getAnimationCount()const{ return (unsigned)m_anims.size(); }
	//2021: Converts the old-style type-based index into an
	//absolute index, or -1 if this animation doesn't exist.
	int getAnim(AnimationModeE type, unsigned subindex)const;
	AnimationModeE getAnimType(unsigned anim)const;

	const char *getAnimName(unsigned anim)const;
	bool setAnimName(unsigned anim, const char *name);

	enum{ setAnimFPS_minimum=1 }; //2022
	double getAnimFPS(unsigned anim)const;
	bool setAnimFPS(unsigned anim, double fps);
		
	bool getAnimWrap(unsigned anim)const;
	bool setAnimWrap(unsigned anim, bool wrap);

	//https://github.com/zturtleman/mm3d/issues/106
	//2020: Get m_frame2020 for sparse animation data. Units are in frames.
	double getAnimTimeFrame(unsigned anim)const;
	bool setAnimTimeFrame(unsigned anim, double time);
	//NOTE: Units are frames. Caller is responsible for uniqueness/sorting.
	double getAnimFrameTime(unsigned anim, unsigned frame)const;
	bool setAnimFrameTime(unsigned anim, unsigned frame, double time);	

	//2020: WHAT VERB IS NORMATIVE IN THIS CASE?
	//Efficiently get frame/convenience function. Units are in frames.
	unsigned getAnimFrame(unsigned anim, double time)const;

	unsigned getAnimFrameCount(unsigned anim)const;
		
	typedef std::vector<FrameAnimVertex*> FrameAnimData;
	bool setAnimFrameCount(unsigned anim, unsigned count);
	bool setAnimFrameCount(unsigned anim, unsigned count, unsigned where, FrameAnimData*);

	//2020: Formerly clearAnimFrame.
	bool deleteAnimFrame(unsigned anim, unsigned frame);

	//2020: Same deal as makeCurrentAnimationFrame
	unsigned insertAnimFrame(unsigned anim, double time);

	//2021: Adding for parity with other "Coords" and getVertexCoords.
	bool setVertexCoords(unsigned vertex, const double *abs)
	{
		return moveVertex(vertex,abs[0],abs[1],abs[2]);
	}
	bool setFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
			double x, double y, double z, Interpolate2020E interp=InterpolateKeep);
	//TODO: Remove ability to return InterpolateVoid.
	//CAUTION: xyz HOLD JUNK IF RETURNED VALUE IS 0!!
	Interpolate2020E getFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
			double &x, double &y, double &z)const;
	//2020: These won't return InterpolateVoid.
	Interpolate2020E hasFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex)const;

	//REMOVE ME
	// Not undo-able
	//NOTE: Historically InterpolateStep is a more correct default but my sense is
	//users would prefer importers to defaul to InterpolateLerp.
	void setQuickFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
			double x, double y, double z, Interpolate2020E interp=InterpolateLerp);
	void setQuickFrameAnimVertexCoords_final() //undo
	{
		invalidateAnim(); m_changeBits|=MoveGeometry; 
	}
		
	//TEMPORARY API
	//PT_Joint: Skeletal animation keyframes 
	//PT_Point: Frame animation keyframes 
	int setKeyframe(unsigned anim, unsigned frame, Position, KeyType2020E isRotation,
			double x, double y, double z, Interpolate2020E interp=InterpolateKeep);
	Interpolate2020E getKeyframe(unsigned anim, unsigned frame,
			Position, KeyType2020E isRotation,
			double &x, double &y, double &z)const;
	//2020
	//NOTE: Returns the highest value if type is left unspecified.
	//For now these should be equal for a joint for a given frame.
	Interpolate2020E hasKeyframe(unsigned anim, unsigned frame,
			Position, KeyType2020E isRotation=KeyAny)const;

	//2021: Tells if keyframes of a given type exists. Set "incl"
	//to combination of PositionMaskE to only perform some tests.
	KeyMask2021E hasKeyframeData(unsigned anim, int incl=~0)const;

	//MEMORY LEAK (removeKeyframe leaks if not using undo system.)
	bool deleteKeyframe(unsigned anim, unsigned frame, Position joint, KeyType2020E isRotation=KeyAny);

	//TODO: It would be nice to have a way to sample the vertex/object vis-a-vis a skeleton.
	int interpKeyframe(unsigned anim, unsigned frame, Position, Matrix &relativeFinal)const;
	int interpKeyframe(unsigned anim, unsigned frame, double frameTime, Position, Matrix &relativeFinal)const;
	int interpKeyframe(unsigned anim, unsigned frame, Position, double trans[3], double rot[3], double scale[3])const;
	int interpKeyframe(unsigned anim, unsigned frame, double frameTime, Position, double trans[3], double rot[3], double scale[3])const;
	int interpKeyframe(unsigned anim, unsigned frame, unsigned vertex, double trans[3])const;
	int interpKeyframe(unsigned anim, unsigned frame, double frameTime, unsigned vertex, double trans[3])const;

	// Animation set operations
	unsigned _dup(Animation *copy, bool keyframes=true);
	int copyAnimation(unsigned anim, const char *newName=nullptr);
	int splitAnimation(unsigned anim, const char *newName, unsigned frame);
	bool joinAnimations(unsigned anim1, unsigned anim2);
	bool mergeAnimations(unsigned anim1, unsigned anim2);
	//2020: "how" is what interploation mode to use, "how2" is what to use for frames that retain
	//the same value, i.e. InterpolateCopy. The default behavior is to use "how" for this purpose.
	int convertAnimToFrame(unsigned anim1, const char *newName, unsigned frameCount, Interpolate2020E how, Interpolate2020E how2=InterpolateNone);
	//2021: Destructively switch to/from the new "ANIMMODE" type.
	//Use hasKeyframeData to test compatability prior to call.
	int convertAnimToType(AnimationModeE, unsigned anim);

	//MU_MoveAnimation and convertAnimToType use this version.
	void _moveAnimation(unsigned oldIndex, unsigned newIndex, int typeDiff);
	bool moveAnimation(unsigned oldIndex, unsigned newIndex);		

	// For undo,don't call these directly
	bool insertKeyframe(unsigned anim, Keyframe *keyframe);
	bool removeKeyframe(unsigned anim, Keyframe *keyframe); //2021
	bool removeKeyframe(unsigned anim, unsigned frame, Position, KeyType2020E isRotation, bool release=false);

	// Merge all animations from model into this model.
	// For skeletal,skeletons must match
	bool mergeAnimations(Model *model);

	// ------------------------------------------------------------------
	// Normal functions
	// ------------------------------------------------------------------

	//FIX US: Historically these behave very differently. Presently getNormal
	//returns the current normal valid or invalid. getFlatNormal now returns
	//a valid normal or else calculates one. It used to always calculate them.
	//I think probably they should validate if required and not calcuate. The
	//"Unanimated" forms are new. Previously the normals were never animated.
	//
	// https://github.com/zturtleman/mm3d/issues/116
	//
	//NOTE: doesn't call validateNormals
	bool getNormalUnanimated(unsigned triangleNum, unsigned vertexIndex, double *normal)const;
	//NOTE: doesn't call validateNormals
	bool getNormal(unsigned triangleNum, unsigned vertexIndex, double *normal)const;
	//NOTE: doesn't depend on validateNormals
	bool getFlatNormalUnanimated(unsigned triangleNum, double *normal)const;
	//NOTE: doesn't depend on validateNormals
	bool getFlatNormal(unsigned triangleNum, double *normal)const;
	//2022: support code
	double calculateFlatNormal(unsigned vertices[3], double normal[3])const;
	double calculateFlatNormalUnanimated(unsigned vertices[3], double normal[3])const;

	//CAUTION: This now uses the "source" pointers to fill out 
	//normals either for animations or the base model.
	void calculateNormals();

	//NEW: Invalidates normals both for animations and base model.
	//TODO: Replace these with spot fix repair system.
	void invalidateNormals(),invalidateAnimNormals();

	//NEW: Calls calculateNormals if current normals requier repairs.
	bool validateNormals()const;

	// Call this when a bone joint is moved, rotated, added, or deleted to
	// recalculate the skeletal structure.
	void validateSkel()const,calculateSkel(); //setupJoints
	void invalidateSkel();
	// This does the first half (joints) of validate/calculateAnim but not
	// the second half (vertex/points coordinates)
	void validateAnimSkel()const,calculateAnimSkel();
	void invalidateAnimSkel(){ m_validAnimJoints = false; }

	void invalidateAnim();
	void invalidateAnim(unsigned,unsigned);
	//NEW: Defer animation calculations same as normals calculations.
	void validateAnim()const,calculateAnim();

	void invertNormals(unsigned triangleNum);
	bool triangleFacesIn(unsigned triangleNum);

	// ------------------------------------------------------------------
	// Geometry functions
	// ------------------------------------------------------------------

	int getVertexCount()const{ return (unsigned)m_vertices.size(); }
	int getTriangleCount()const{ return (unsigned)m_triangles.size(); }
	int getBoneJointCount()const{ return (unsigned)m_joints.size(); }
	int getPointCount()const{ return (unsigned)m_points.size(); }
	int getProjectionCount()const{ return (unsigned)m_projections.size(); }

	//2020: This is to replace ModelFilter providing access to the data
	//lists. The lists are now const so encapsulation isn't compromised.
	typedef std::vector<Triangle*> _TriangleList;
	typedef const std::vector<const Triangle*> TriangleList;
	TriangleList &getTriangleList()const{ return *(TriangleList*)&m_triangles; };
	typedef std::vector<Vertex*> _VertexList;
	typedef const std::vector<const Vertex*> VertexList;
	VertexList &getVertexList()const{ return *(VertexList*)&m_vertices; };
	typedef std::vector<Group*> _GroupList;
	typedef const std::vector<const Group*> GroupList;
	GroupList &getGroupList(){ return *(GroupList*)&m_groups; };
	typedef std::vector<Material*> _MaterialList;
	typedef const std::vector<const Material*> MaterialList;
	MaterialList &getMaterialList(){ return *(MaterialList*)&m_materials; };
	typedef std::vector<Joint*> _JointList;
	typedef const std::vector<const Joint*> JointList;
	JointList &getJointList(){ return *(JointList*)&m_joints; };
	typedef std::vector<std::pair<unsigned,Joint*>> _JointList2;
	typedef const std::vector<std::pair<unsigned,Joint*>> JointList2;
	JointList2 &getFlatJointList(){ return *(JointList2*)&m_joints2; };
	typedef std::vector<Point*> _PointList;
	typedef const std::vector<const Point*> PointList;
	PointList &getPointList(){ return *(PointList*)&m_points; };
	typedef std::vector<TextureProjection*> _ProjectionList;
	typedef const std::vector<const TextureProjection*> ProjectionList;
	ProjectionList &getProjectionList(){ return *(ProjectionList*)&m_projections; };
	typedef std::vector<BackgroundImage*> _BackgroundList;
	typedef const std::vector<const BackgroundImage*> BackgroundList;
	BackgroundList &getBackgroundList(){ return *(BackgroundList*)&m_background; };
	/*REMOVE ME
	typedef std::vector<SkelAnim*> _SkelAnimList;
	typedef const std::vector<const SkelAnim*> SkelAnimList;
	SkelAnimList &getSkelList(){ return *(SkelAnimList*)&m_skelAnims; };
	typedef std::vector<FrameAnim*> _FrameAnimList;
	typedef const std::vector<const FrameAnim*> FrameAnimList;
	FrameAnimList &getFrameList(){ return *(FrameAnimList*)&m_frameAnims; };*/
	typedef std::vector<Animation*> _AnimationList;
	typedef const std::vector<const Animation*> AnimationList;
	AnimationList &getAnimationList(){ return *(AnimationList*)&m_anims; };
	typedef std::vector<Utility*> _UtilityList;
	typedef const std::vector<const Utility*> UtilityList;
	UtilityList &getUtilityList(){ return *(UtilityList*)&m_utils; };
	const ObserverList &getObserverList(){ return m_observers; };

	int addVertex(double x, double y, double z);
	int addVertex(int copy, const double pos[3]=0);
	int addTriangle(unsigned vert1, unsigned vert2, unsigned vert3);

	//2020: This API leaves dangling references to vertices! (UNSAFE)
	void deleteVertex(unsigned vertex);
	void deleteTriangle(unsigned triangle);

	//2021: Manipulates drawing order for correcting for depth-test issues
	//in games.
	void prioritizeSelectedTriangles(bool to_begin_or_end);
	void reverseOrderSelectedTriangle();

	// No undo on this one
	//void setVertexFree(unsigned v,bool o);
	bool isVertexFree(unsigned v)const;

	// When all faces attached to a vertex are deleted,the vertex is considered
	// an "orphan" and deleted (unless it is a "free" vertex,see m_free in the
	// vertex class).
	unsigned deleteOrphanedVertices(unsigned begin=0, unsigned end=~0);

	// A flattened triangle is a triangle with two or more corners that are
	// assigned to the same vertex (this usually happens when vertices are
	// welded together).
	void deleteFlattenedTriangles();

	bool isTriangleMarked(unsigned t)const;

	bool subdivideSelectedTriangles();
	//void unsubdivideTriangles(CAN'T BE UNDONE... IT'S REALLY FOR MU_SubdivideSelected use.) 
	void subdivideSelectedTriangles_undo(unsigned t1, unsigned t2, unsigned t3, unsigned t4);

	// If co-planer triangles have edges with points that are co-linear they
	// can be combined into a single triangle. The simplifySelectedMesh function
	// performs this operation to combine all faces that do not add detail to
	// the model.
	//
	// 2022: texture_map_tolerance says to consider groups and textures to within
	// this percent of a pixel vis-a-vis a group's texture's width and height. If
	// set to 0 then the old behavior of disregarding groups/textures applies but
	// texture coordinates are still preserved to the extend possible.
	//
	bool simplifySelectedMesh(float texture_map_tolerance=0.1666f);

	bool setTriangleVertices(unsigned triangleNum, unsigned vert1, unsigned vert2, unsigned vert3);
	bool getTriangleVertices(unsigned triangleNum, unsigned &vert1, unsigned &vert2, unsigned &vert3)const;
	void setTriangleMarked(unsigned triangleNum, bool marked);
	void clearMarkedTriangles();

	bool getVertexCoords(unsigned vertexNumber, double *coord)const;
	bool getVertexCoordsUnanimated(unsigned vertexNumber, double *coord)const;
	/*2019: Removing this to remove dependency on ViewRight/Left.
	//It is used only by implui/texturecoord.cc. It's not a big enough piece of Model to warrant
	//existing.
	//bool getVertexCoords2d(unsigned vertexNumber,ProjectionDirectionE dir, double *coord)const;
	*/

	int getTriangleVertex(unsigned triangleNumber, unsigned vertexIndex)const;

	void booleanOperation(BooleanOpE op, int_list &listA,int_list &listB);

	Model *copySelected(bool animated=false)const;

	bool duplicateSelected(bool animated=false, bool separate=false); //2020 (dupcmd)
	bool separateSelected(){ return duplicateSelected(false,true); } //2021

	// A BSP tree is calculated for triangles that have textures with an alpha
	// channel (transparency). It is used to determine in what order triangles
	// must be rendered to get the proper blending results (triangles that are
	// farther away from the camera are rendered first so that closer triangles
	// are drawn on top of them).
	void calculateBspTree(), invalidateBspTree();

	//2020: I've removed the trans/rot argument to focus on the task at hand
	//Please use applyMatrix.
	bool mergeModels(const Model *model, bool textures, AnimationMergeE mergeMode, bool emptyGroups);

	// These are helper functions for the boolean operations
	void removeInternalTriangles(int_list &listA, int_list &listB);
	void removeExternalTriangles(int_list &listA, int_list &listB);
	void removeBTriangles(int_list &listA, int_list &listB);

	// ------------------------------------------------------------------
	// Group and material functions
	// ------------------------------------------------------------------

	// TODO: Misnamed,should be getMaterialCount()
	inline int getTextureCount()const { return m_materials.size(); };

	int addGroup(const char *name);
	int addGroup(int cp, const char *name=0); //Copy Constructor

	// Textures and Color materials go into the same material list
	int addTexture(Texture *tex);
	int addColorMaterial(const char *name);

	bool setGroupTextureId(unsigned groupNumber, int textureId);

	void deleteGroup(unsigned group);
	void deleteTexture(unsigned texture);

	const char *getGroupName(unsigned groupNum)const;
	bool setGroupName(unsigned groupNum, const char *groupName);

	inline int getGroupCount()const { return m_groups.size(); };

	//REMOVE US?
	int getGroupByName(const char *groupName, bool ignoreCase=false)const;
	int getMaterialByName(const char *materialName, bool ignoreCase=false)const;

	Material::MaterialTypeE getMaterialType(unsigned materialIndex)const;

	//REMOVE ME
	//1 byte at at time???
	int getMaterialColor(unsigned materialIndex, unsigned c, unsigned v = 0)const
	{
		if(materialIndex<m_materials.size()&&c<4&&v<4)
		{
			return m_materials[materialIndex]->m_color[v][c];
		}
		else return 0;
	}

	int removeUnusedGroups();
	int mergeIdenticalGroups();
	int removeUnusedMaterials();
	int mergeIdenticalMaterials();

	// These implicitly change the material type.
	void setMaterialTexture(unsigned textureId,Texture *tex);
	void removeMaterialTexture(unsigned textureId);

	uint8_t getGroupSmooth(unsigned groupNum)const;
	bool setGroupSmooth(unsigned groupNum,uint8_t smooth);
	uint8_t getGroupAngle(unsigned groupNum)const;
	bool setGroupAngle(unsigned groupNum,uint8_t angle);

	bool setTextureName(unsigned textureId, const char *name);
	const char *getTextureName(unsigned textureId)const;

	const char *getTextureFilename(unsigned textureId)const;
	Texture *getTextureData(unsigned textureId);

	// Lighting accessors
	bool getTextureAmbient(unsigned textureId, float *ambient)const;
	bool getTextureDiffuse(unsigned textureId, float *diffuse)const;
	bool getTextureEmissive(unsigned textureId, float *emissive)const;
	bool getTextureSpecular(unsigned textureId, float *specular)const;
	bool getTextureShininess(unsigned textureId, float &shininess)const;

	bool setTextureAmbient(unsigned textureId, const float *ambient);
	bool setTextureDiffuse(unsigned textureId, const float *diffuse);
	bool setTextureEmissive(unsigned textureId, const float *emissive);
	bool setTextureSpecular(unsigned textureId, const float *specular);
	bool setTextureShininess(unsigned textureId, float shininess);

	// See the clamp property in the Material class.
	bool getTextureSClamp(unsigned textureId)const;
	bool getTextureTClamp(unsigned textureId)const;
	bool setTextureSClamp(unsigned textureId, bool clamp);
	bool setTextureTClamp(unsigned textureId, bool clamp);
	//2021: true enables "additive" blending.
	//NOTE: Other modes are pretty uncommon but if needed this
	//can be promoted to an enum type... bear in mind bool can
	//be saved as a "flag" in the MM3D format.
	bool getTextureBlendMode(unsigned textureId)const;
	bool setTextureBlendMode(unsigned textureId, bool accumulate);		

	void getUngroupedTriangles(int_list&)const;
	//int_list getGroupTriangles(unsigned groupNumber)const;
	const int_list &getGroupTriangles(unsigned groupNumber)const; //2020
	int getGroupTextureId(unsigned groupNumber)const;

	//FIX ME: THESE ARE EXTREMELY INEFFECIENT
	bool addTriangleToGroup(unsigned groupNum, unsigned triangleNum);
	bool removeTriangleFromGroup(unsigned groupNum, unsigned triangleNum);
	void setSelectedAsGroup(unsigned groupNum);
	void addSelectedToGroup(unsigned groupNum);
	int getTriangleGroup(unsigned triangleNumber)const;

	bool getTextureCoords(unsigned triangleNumber, unsigned vertexIndex, float &s, float &t)const;
	bool setTextureCoords(unsigned triangleNumber, unsigned vertexIndex, float s, float t);
	bool getTextureCoords(unsigned triangleNumber, float(*st)[3])const; //2022
	bool setTextureCoords(unsigned triangleNumber, float(*st)[3]);

	// ------------------------------------------------------------------
	// Skeletal structure and influence functions
	// ------------------------------------------------------------------
		
	//Please use setBoneJointOffset to assign relative coordinates.
	//I don't think passing them is a big gain but it's also a bit
	//confusing for one overload to be absolute and other relative.
	int addBoneJoint(const char *name, int parent=-1);
	//LEGACY: This is absolute translation (relative to the parent.)
	int addBoneJoint(const char *name, double x, double y, double z, int parent=-1);

	void deleteBoneJoint(unsigned joint);

	const char *getBoneJointName(unsigned joint)const;
	int getBoneJointParent(unsigned joint)const;

	//2021: This gets the joints local/relative coordinates. 
	//There's no other API for doing that besides the other
	//version that ignores animation. The Position versions
	//are a mix of global and local now but the plan is the
	//rotation and scale methods will be fully global later.
	bool getBoneJointOffset(unsigned j, double rel[3], double rot[3]=nullptr, double scale[3]=nullptr);
	bool getBoneJointOffsetUnanimated(unsigned j, double rel[3], double rot[3]=nullptr, double scale[3]=nullptr);
		
	//These are Position compatible APIs.
	bool getBoneJointCoords(unsigned jointNumber, double *abs)const;
	bool getBoneJointCoordsUnanimated(unsigned jointNumber, double *abs)const;
	bool getBoneJointRotation(unsigned jointNumber, double *coord)const;
	bool getBoneJointRotationUnanimated(unsigned jointNumber, double *coord)const;
	bool getBoneJointScale(unsigned jointNumber, double *coord)const;
	bool getBoneJointScaleUnanimated(unsigned jointNumber, double *coord)const;

	bool getBoneJointFinalMatrix(unsigned jointNumber,Matrix &m)const;
	bool getBoneJointAbsoluteMatrix(unsigned jointNumber,Matrix &m)const;
	bool getBoneJointRelativeMatrix(unsigned jointNumber,Matrix &m)const;
	bool getPointFinalMatrix(unsigned pointNumber,Matrix &m)const;
	bool getPointAbsoluteMatrix(unsigned pointNumber,Matrix &m)const;
		
	/*REMOVE ME
	//Only dupcmd uses this in a way that looks bad. 
	void getBoneJointVertices(int joint, int_list&)const;*/

	//REMOVE US?
	//These remove all existing influences and assign it 100%.
	bool setPositionBoneJoint(const Position &pos, int joint);
	bool setVertexBoneJoint(unsigned vertex, int joint);
	bool setPointBoneJoint(unsigned point, int joint);

	//These were "add" but it seems like most (all) setting operations really
	//ought to add and "autoSetPositionInfluences" is really adding. It seems
	//like there's too many APIs around influences, that maybe just one might
	//be enough for adding/setting?
	bool setPositionInfluence(const Position &pos, unsigned joint, InfluenceTypeE type, double weight);
	bool setVertexInfluence(unsigned vertex, unsigned joint, InfluenceTypeE type, double weight);
	bool setPointInfluence(unsigned point, unsigned joint, InfluenceTypeE type, double weight);

	bool removePositionInfluence(const Position &pos, unsigned joint);
	bool removeVertexInfluence(unsigned vertex, unsigned joint);
	bool removePointInfluence(unsigned point, unsigned joint);

	bool removeAllPositionInfluences(const Position &pos);
	bool removeAllVertexInfluences(unsigned vertex);
	bool removeAllPointInfluences(unsigned point);

	//REMOVE US
	bool getPositionInfluences(const Position &pos,infl_list &l)const;
	bool getVertexInfluences(unsigned vertex,infl_list &l)const;
	bool getPointInfluences(unsigned point,infl_list &l)const;		
	//2019: Trying to encourage better programming.
	infl_list &getVertexInfluences(unsigned vertex) //NEW
	{		
		return m_vertices[vertex]->m_influences;
	}
	infl_list &getPointInfluences(unsigned point) //NEW
	{
		return m_points[point]->m_influences;
	}
	infl_list *getPositionInfluences(const Position &pos) //NEW
	{
		switch(pos.type)
		{
		case PT_Vertex: return &getVertexInfluences(pos.index);			
		case PT_Point: return &getPointInfluences(pos.index);
		}
		return nullptr;
	}
	const infl_list &getVertexInfluences(unsigned vertex)const //NEW 
	{		
		return m_vertices[vertex]->m_influences;
	}
	const infl_list &getPointInfluences(unsigned point)const //NEW 
	{
		return m_points[point]->m_influences;
	}
	const infl_list *getPositionInfluences(const Position &pos)const //NEW
	{
		switch(pos.type)
		{
		case PT_Vertex: return &getVertexInfluences(pos.index);
		case PT_Point: return &getPointInfluences(pos.index);
		}
		return nullptr;
	}

	//NOTE: These appear to replace getVertexBoneJoint/getPointBoneJoint
	//so I'm removing those.
	int getPrimaryPositionInfluence(const Position &pos)const;
	int getPrimaryVertexInfluence(unsigned vertex)const;
	int getPrimaryPointInfluence(unsigned point)const;

	bool setPositionInfluenceType(const Position &pos, unsigned joint,InfluenceTypeE type);
	bool setVertexInfluenceType(unsigned vertex, unsigned joint,InfluenceTypeE type);
	bool setPointInfluenceType(unsigned point, unsigned joint,InfluenceTypeE type);

	bool setPositionInfluenceWeight(const Position &pos, unsigned joint, double weight);
	bool setVertexInfluenceWeight(unsigned vertex, unsigned joint, double weight);
	bool setPointInfluenceWeight(unsigned point, unsigned joint, double weight);

	bool autoSetPositionInfluences(const Position &pos, double sensitivity, bool selected);
	bool autoSetVertexInfluences(unsigned vertex, double sensitivity, bool selected);
	bool autoSetPointInfluences(unsigned point, double sensitivity, bool selected);
	bool autoSetCoordInfluences(double *coord, double sensitivity, bool selected, int_list &infList);

	bool setBoneJointName(unsigned joint, const char *name);
	bool setBoneJointParent(unsigned joint, int parent=-1, bool validate=true);

	//2021: This sets the joints local/relative coordinates. 
	//There's no other API for doing that besides the other
	//version that ignores animation. The Position versions
	//are a mix of global and local now but the plan is the
	//rotation and scale methods will be fully global later.
	bool setBoneJointOffset(unsigned j, const double rel[3], const double rot[3]=nullptr, const double scale[3]=nullptr);
	bool setBoneJointOffsetUnanimated(unsigned j, const double rel[3], const double rot[3]=nullptr, const double scale[3]=nullptr);

	//These are Position compatible APIs.
	bool setBoneJointCoords(unsigned j, const double *abs);			
	bool setBoneJointRotation(unsigned j, const double *rot);
	bool setBoneJointScale(unsigned point, const double *scale);

	double calculatePositionInfluenceWeight(const Position &pos, unsigned joint)const;
	double calculateVertexInfluenceWeight(unsigned vertex, unsigned joint)const;
	double calculatePointInfluenceWeight(unsigned point, unsigned joint)const;
	double calculateCoordInfluenceWeight(const double *coord, unsigned joint)const;
	//NOTE: All "calculate" Influence methods call this eventually, so
	//you can read it as "finalize" influence.
	void calculateRemainderWeight(const Position&);

	bool getBoneVector(unsigned joint, double *vec, const double *coord)const;

	// ------------------------------------------------------------------
	// Point functions
	// ------------------------------------------------------------------

	int addPoint(const char *name, double x, double y, double z, double xrot, double yrot, double zrot);

	void deletePoint(unsigned point);

	//REMOVE ME?
	int getPointByName(const char *name)const;

	const char *getPointName(unsigned point)const;
	bool setPointName(unsigned point, const char *name);

	bool getPointCoords(unsigned pointNumber, double *coord)const;
	bool getPointCoordsUnanimated(unsigned point, double *abs)const;
	bool getPointRotation(unsigned point, double *rot)const;
	bool getPointRotationUnanimated(unsigned point, double *rot)const;
	bool getPointScale(unsigned point, double *rot)const;
	bool getPointScaleUnanimated(unsigned point, double *rot)const;

	bool setPointRotation(unsigned point, const double *rot);
	bool setPointCoords(unsigned point, const double *abs);
	bool setPointScale(unsigned point, const double *scale);

	// ------------------------------------------------------------------
	// Texture projection functions
	// ------------------------------------------------------------------

	int addProjection(const char *name, int type, double x, double y, double z);
	void deleteProjection(unsigned proj);

	const char *getProjectionName(unsigned proj)const;		
	bool setProjectionName(unsigned proj, const char *name);
	bool setProjectionType(unsigned proj, int type);
	int  getProjectionType(unsigned proj)const;		
	bool setProjectionCoords(unsigned projNumber, const double *coord);
	bool getProjectionCoords(unsigned projNumber, double *coord)const;
	/*2020: This wasn't editor friendly.
	//NOTE: Needing to add Rotation to the Properties sidebar having
	//the old up/seam representation can't store rotation accurately.
	//TODO: It would be nice to be able to do what Create Projection
	//does with an existing projection. It can be easier than Rotate.
	//https://github.com/zturtleman/mm3d/issues/114
	//I think to bring these back (if desired) they need to set both
	//vectors at the same time and use the up vector as a hint so to
	//produce an othornormal affine matrix to take the rotation from.
	bool setProjectionUp(unsigned projNumber, const double *coord);
	bool setProjectionSeam(unsigned projNumber, const double *coord);
	bool getProjectionUp(unsigned projNumber, double *coord)const;
	bool getProjectionSeam(unsigned projNumber, double *coord)const;
	*/
	bool setProjectionRotation(unsigned projNumber, const double *rot);
	bool getProjectionRotation(unsigned projNumber, double *rot)const;				
	bool setProjectionScale(unsigned p, double scale);
	double getProjectionScale(unsigned p)const;
	bool setProjectionRange(unsigned projNumber,
	double xmin, double ymin, double xmax, double ymax);
	bool getProjectionRange(unsigned projNumber,
	double &xmin, double &ymin, double &xmax, double &ymax)const;

	#ifdef NDEBUG
//		#error Need int_list version of setTriangleProjection
	#endif
	void setTriangleProjection(unsigned triangleNum, int proj);
	int getTriangleProjection(unsigned triangleNum)const;

	void applyProjection(unsigned int proj);

	// ------------------------------------------------------------------
	// Undo/Redo functions
	// ------------------------------------------------------------------

	bool setUndoEnabled(bool o);
	bool getUndoEnabled()const{ return m_undoEnabled; } //NEW
	bool getUndoEnacted()const{ return m_undoEnacted; } //2022

	// Indicates that a user-specified operation is complete. A single
	// "operation" may span many function calls and different types of
	// manipulations.
	void operationComplete(const char *opname);
	
	// Clear undo list
	void clearUndo();

	bool canUndo()const;
	bool canRedo()const;
	void undo(int how=-1);
	void redo(){ undo(+1); }

	// If a manipulations have been performed, but not commited as a single
	// undo list, undo those operations (often used when the user clicks
	// a "Cancel" button to discard "unapplied" changes).
	void undoCurrent(){ undo(0); }

	// Allow intelligent handling of window close button
	bool canUndoCurrent()const{ return m_undoMgr->canUndoCurrent(); }

	const char *getUndoOpName()const;
	const char *getRedoOpName()const;

	// The limits at which undo operations are removed from memory.
	void setUndoSizeLimit(unsigned sizeLimit);
	void setUndoCountLimit(unsigned countLimit);

	// These functions are for undo and features such as this.
	// Don't use them unless you are me.
	void insertVertex(unsigned index,Vertex *vertex);
	void removeVertex(unsigned index);

	void insertTriangle(unsigned index,Triangle *triangle);
	void removeTriangle(unsigned index);
	void remapTrianglesIndices(const int_list&);

	void insertGroup(unsigned index,Group *group);
	void removeGroup(unsigned index);

	void insertBoneJoint(unsigned index,Joint *joint);
	void removeBoneJoint(unsigned index);
	void parentBoneJoint(unsigned index, int parent, Matrix &mutated); //2021
	void parentBoneJoint2(unsigned index, int parent);

	void insertInfluence(const Position &pos, unsigned index, const InfluenceT &influence);
	void removeInfluence(const Position &pos, unsigned index);

	void insertPoint(unsigned index,Point *point);
	void removePoint(unsigned index);

	void insertProjection(unsigned index,TextureProjection *proj);
	void removeProjection(unsigned index);

	void insertTexture(unsigned index,Material *material);
	void removeTexture(unsigned index);

	//INTERNAL: addAnimation subroutines
	//void insertFrameAnim(unsigned index,FrameAnim *anim);
	//void removeFrameAnim(unsigned index);
	//INTERNAL: addAnimation subroutines
	//void insertSkelAnim(unsigned anim,SkelAnim *fa);
	//void removeSkelAnim(unsigned anim);
	void insertAnimation(unsigned anim,Animation *fa);
	void removeAnimation(unsigned anim);

	//INTERNAL: setFrameCount subroutines
	void insertFrameAnimData(unsigned frame0, unsigned frames, FrameAnimData *data, const Animation *draw);
	void removeFrameAnimData(unsigned frame0, unsigned frames, FrameAnimData *data, bool release=false);

	// ------------------------------------------------------------------
	// Selection functions
	// ------------------------------------------------------------------

	void setSelectionMode(SelectionModeE m);
	inline SelectionModeE getSelectionMode(){ return m_selectionMode; };

	unsigned getSelectedVertexCount()const;
	unsigned getSelectedTriangleCount()const;
	unsigned getSelectedGroupCount()const;
	unsigned getSelectedBoneJointCount()const;
	unsigned getSelectedPointCount()const;
	unsigned getSelectedProjectionCount()const;

	bool setSelectedTriangles(const int_list &presorted); //2022
	void getSelectedTriangles(int_list &l)const;
	void getSelectedGroups(int_list &l)const;

	//2020: I've put PT_Vertex on the back of the
	//Position lists. See definition for thinking.
	void getSelectedPositions(pos_list &l)const;
	//2021: only copySelected uses this... I've 
	//changed it to to use m_joints2's ordering.
	void getSelectedBoneJoints(int_list &l)const;
	void getSelectedPoints(int_list &l)const;
	void getSelectedProjections(int_list &l)const;
	void getSelectedVertices(int_list &l)const;

	//2020: This is unlikely a permanent solution.
	//I'm not sure how else to implement this API
	//with existing semantics.
	template<class T> struct Get3
	{
		template<class Functor>
		Get3(const Functor &pred)
		{
			p = &pred;
			f = [](const void *p, const T t[3])
			{
				(*(const Functor*)p)(t);
			};
		}
		const void *p;
		void(*f)(const void*,const T[3]);
		void operator()(const T get[3]){ f(p,get); }
	};
	void getSelectedInterpolation(unsigned anim, unsigned frame, Get3<Interpolate2020E>);

	void selectAll(bool how=true);	
	bool selectAllVertices(bool how=true);
	bool selectAllTriangles(bool how=true);
	bool selectAllGroups(bool how=true); 
	bool selectAllBoneJoints(bool how=true);
	bool selectAllPoints(bool how=true);
	bool selectAllProjections(bool how=true);		
	bool selectAllPositions(PositionTypeE, bool how=true);
	void unselectAll(){ selectAll(false); }
	bool unselectAllVertices(){ return selectAllVertices(false); }
	bool unselectAllTriangles(){ return selectAllTriangles(false); }
	bool unselectAllGroups(){ return selectAllGroups(false); }
	bool unselectAllBoneJoints(){ return selectAllBoneJoints(false); }
	bool unselectAllPoints(){ return selectAllPoints(false); }
	bool unselectAllProjections(){ return selectAllProjections(false); }
	bool unselectAllPositions(PositionTypeE pt){ return selectAllPositions(pt,false); }

	// A selection test is an additional condition you can attach to whether
	// or not an object in the selection volume should be selected. For example,
	// this is used to add a test for which way a triangle is facing so that
	// triangles not facing the camera can be excluded from the selection.
	// You can implement a selection test for any property that you can check
	// on the primitive.
	struct SelectionTest
	{
		virtual ~SelectionTest(){}
		virtual bool shouldSelect(void *element) = 0;
	};

	bool selectInVolumeMatrix(const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test = nullptr);
	bool unselectInVolumeMatrix(const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test = nullptr);

	bool getBoundingRegion(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const;
	bool getSelectedBoundingRegion(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const;

	bool getBounds(Model::OperationScopeE os, double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const
	{
		return (this->*(os==OS_Selected?&Model::getSelectedBoundingRegion:&Model::getBoundingRegion))(x1,y1,z1,x2,y2,z2);
	}

	// CAUTION: No longer clears keyframes in ANIMMODE_SKELETON.
	void deleteSelected();

	//REMOVE ME? (SEEMS VERY WORK INTENSIVE FOR VERY LITTE GAIN)
	//SHOULD THIS BE LIMITED TO setSelectionMode?
	//(What about selectVerticesFromTriangles?)
	//
	// NOTE: If combining undo objects was done on the stack this
	// would be worse even in the hard cases, whereas when there
	// are only a few selections this is always worse depending
	// on how large the model is.
	// 
	// 2022: THIS INTRODUCES _OrderedSelection::Marker AS A HACK.
	// I INTEND TO REMOVE IT WITH begin/end (HERE) WHEN I HAVE A 
	// MOMENT TO STUDY IT.
	//
	// When changing the selection state of a lot of primitives, a difference
	// list is used to track undo information. These calls indicate when the
	// undo information should start being tracked and when it should be
	// completed.
	void beginSelectionDifference(); //OVERKILL!
	void endSelectionDifference();

	bool selectVertex(unsigned v, unsigned how=true);
	bool unselectVertex(unsigned v){ return selectVertex(v,0); }
	bool isVertexSelected(unsigned v)const;

	bool selectTriangle(unsigned t);
	//2019: This is too dangerous. Don't use it.
	//It calls selectVerticesFromTriangles every
	//time.
	//2020: I've used m_faces to optimize it.
	bool unselectTriangle(unsigned t, bool selectVerticesFromTriangles=true);
	bool isTriangleSelected(unsigned t)const;

	bool selectGroup(unsigned g);
	bool unselectGroup(unsigned g);
	bool isGroupSelected(unsigned g)const;

	bool selectBoneJoint(unsigned j, bool how=true);
	bool unselectBoneJoint(unsigned j){ return selectBoneJoint(j,0); }
	bool isBoneJointSelected(unsigned j)const;

	bool selectPoint(unsigned p, bool how=true);
	bool unselectPoint(unsigned p){ return selectPoint(p,0); }
	bool isPointSelected(unsigned p)const;

	bool selectProjection(unsigned p, bool how=true);
	bool unselectProjection(unsigned p){ return selectPoint(p,0); }
	bool isProjectionSelected(unsigned p)const;

	bool selectPosition(Position, bool how=true);		
	bool unselectPosition(Position p){ return selectPosition(p,false); }
	bool isPositionSelected(Position p)const;

	// The behavior of this function changes based on the selection mode.
	bool invertSelection();

	// Select all vertices that have the m_free property set and are not used
	// by any triangles (this can be used to clean up unused free vertices,
	// like a manual analog to the deleteOrphanedVertices() function).
	bool selectFreeVertices(bool how=true);

	bool selectUngroupedTriangles(bool how); //2022

	//FIX ME
	//https://github.com/zturtleman/mm3d/issues/63
	//#ifdef MM3D_EDIT 
	//TextureWidget API (texwidget.cc)
	void setSelectedUv(const int_list &uvList);
	void getSelectedUv(int_list &uvList)const;
	int_list &getSelectedUv(){ return m_selectedUv; } //2020
	void clearSelectedUv();
	//https://github.com/zturtleman/mm3d/issues/56
	//ModelViewport API (modelviewport.cc)
	ViewportUnits &getViewportUnits(){ return m_viewportUnits; }
	//#endif

	// ------------------------------------------------------------------
	// Hide functions
	// ------------------------------------------------------------------

	bool hideSelected(bool how=true);
	bool hideUnselected(){ return hideSelected(false); }
	bool unhideAll();
	void invertHidden(); //2022

	bool isVertexVisible(unsigned v)const;
	bool isTriangleVisible(unsigned t)const;
	//bool isGroupVisible(unsigned g)const; //UNUSED
	bool isBoneJointVisible(unsigned j)const;
	bool isPointVisible(unsigned p)const;

	//WARNING: THESE ARE UNSAFE FOR UNDO/CHANGE-BITS
	// Don't call these directly... use selection/hide selection
	void hideVertex(unsigned,int=true);
	//bool unhideVertex(unsigned);
	void hideTriangle(unsigned,int=true);
	//bool unhideTriangle(unsigned);
	void hideJoint(unsigned,int=true);
	//bool unhideJoint(unsigned);
	void hidePoint(unsigned,int=true);
	//bool unhidePoint(unsigned);

	// ------------------------------------------------------------------
	// Transform functions
	// ------------------------------------------------------------------

	bool moveVertex(unsigned v, double x, double y, double z);
	bool moveBoneJoint(unsigned j, double x, double y, double z);
	bool movePoint(unsigned p, double x, double y, double z);
	bool moveProjection(unsigned p, double x, double y, double z);
	bool movePosition(const Position &pos, double x, double y, double z);
	bool movePositionUnanimated(const Position &pos, double x, double y, double z);

	//TODO: NEED UNIFORM TRANSLATE/ROTATE MODES FOR JOINTS:
	//1) Free movement (current for translation w/o animation)
	//2) Move root of selection only (current for other modes)
	//3) Move all (was current *bug* for rotating w/ animation)
	//(3 can be pretty interesting, but might have applications)
	void translateSelected(const double vec[3]);
	void rotateSelected(const Matrix &m, const double point[3]);
	void interpolateSelected(Interpolant2020E,Interpolate2020E);

	// Applies arbitrary matrix to primitives (selected or all based on scope).
	// Some matrices cannot be undone (consider a matrix that scales a dimension to 0).
	// If the matrix cannot be undone,set undoable to false (a matrix is undoable if
	// the determinant is not equal to zero).
	//void applyMatrix(const Matrix &m,OperationScopeE scope, bool undoable);
	void applyMatrix(Matrix, OperationScopeE scope, bool undoable);

protected:

	// ==================================================================
	// Protected methods
	// ==================================================================
		
	// No undo on this one
	bool relocateBoneJoint(unsigned j, double x, double y, double z, bool downstream);				

	// ------------------------------------------------------------------
	// Texture context methods
	// ------------------------------------------------------------------

	DrawingContext *getDrawingContext(ContextT context);
	void deleteGlTextures(ContextT context);

	// If any group is using material "id",set the group to having
	// no texture (used when materials are deleted).
	void noTexture(unsigned id);

	// ------------------------------------------------------------------
	// Book-keeping for add/delete undo
	// ------------------------------------------------------------------

	//REMOVE US
	//https://github.com/zturtleman/mm3d/issues/92
	void adjustVertexIndices(unsigned index, int count);
	void adjustTriangleIndices(unsigned index, int count);
	void adjustProjectionIndices(unsigned index, int count);

	// ------------------------------------------------------------------
	// Hiding/visibility
	// ------------------------------------------------------------------

	/*??? //2022
	void beginHideDifference();
	void endHideDifference(); 
	// When primitives of one type are hidden, other primitives may need to
	// be hidden at the same time.
	void hideVerticesFromTriangles(); //UNUSED/UNSAFE
	void unhideVerticesFromTriangles(); //UNUSED/UNSAFE
	void hideTrianglesFromVertices(); //2021: this was modified //UNUSED/UNSAFE
	void unhideTrianglesFromVertices(); //UNUSED/UNSAFE*/

	// ------------------------------------------------------------------
	// Selection
	// ------------------------------------------------------------------

	bool selectVerticesInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);
	bool selectTrianglesInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,bool connected,SelectionTest *test = nullptr);
	bool selectGroupsInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);
	bool selectBoneJointsInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);
	bool selectPointsInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);
	bool selectProjectionsInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);

	friend class MU_Select;

	// When primitives of one type are selected, other primitives may need to
	// be selected at the same time.
	/*2022: "From" is misleading because there isn't a 1-to-1 relationship
	//for groups. I actually don't think this should be used in any scenario.
	//Otherwise it would be good to rename it.
	bool selectTrianglesFromGroups();*/
	bool _selectTrianglesFromGroups_marked(bool how);
	//void selectGroupsFromTriangles(bool all=true);
	void _selectGroupsFromTriangles_marked2(bool how);	
	void _selectVerticesFromTriangles();

	public:
	bool parentJointSelected(int joint)const;
	bool directParentJointSelected(int joint)const;
	protected:

	// ------------------------------------------------------------------
	// Undo
	// ------------------------------------------------------------------

	//This flag used to interleave things in the undo list by searching
	//into it, but this causes operations to be performed out-of-order
	//so I changed it to be optining into the "combine" function, but
	//I think now that it's too error prone to default or not really
	//necessary to think about, so it should just always combine if
	//combinable.
	//void sendUndo(Undo *undo, bool listCombine = false);
	void sendUndo(Undo *undo), appendUndo(Undo *undo);

	// ------------------------------------------------------------------
	// Meta
	// ------------------------------------------------------------------

	//REMOVE ME
	//This parameter is true for UI code that uses
	//this to refresh the display. It shouldn't be
	//necessary, but m_changeBits isn't always set.
	//https://github.com/zturtleman/mm3d/issues/90
	public:
	void updateObservers(bool remove_me=true);
	protected:

	// ------------------------------------------------------------------
	// Protected data
	// ------------------------------------------------------------------

	// See the various accessors for this data for details on what these
	// properties mean.

	friend class MainWin; //EXPOSE US
	friend class MU_InterpolateSelected;
	friend class MU_SetAnimFrameCount;
	std::string	m_filename;
	std::string	m_exportFile;
	std::string	m_filterSpecificError;
	static std::string	s_lastFilterError;

	std::list<std::string> m_loadErrors; //queue

	MetaDataList			 m_metaData;
	_VertexList m_vertices;
	_TriangleList m_triangles;
	_GroupList m_groups;
	_MaterialList m_materials;
	_JointList m_joints;
	_JointList2 m_joints2;
	_PointList m_points;
	_ProjectionList m_projections;
	//_FrameAnimList m_frameAnims;		
	//_SkelAnimList m_skelAnims;
	_AnimationList m_anims;

	//REMOVE US
	DrawingContextList m_drawingContexts;
	bool m_validContext; //2020

	bool m_validBspTree;

	BspTree m_bspTree;

	std::vector<FormatData*> m_formatData;
		
	//2019: Changing to int to break depenency on the
	//config system.
	//DrawJointModeE m_drawJoints;
	bool m_drawJoints,m_drawSelection;
	bool m_drawProjections;
		
	//bool m_forceAddOrDelete; //OBSOLETE

	bool m_validAnim; //2020
				
	bool m_validJoints;
	bool m_validAnimJoints; //2020

	bool m_validNormals;		
	bool m_validAnimNormals; //2020

	bool m_skeletalMode;
	bool m_skeletalMode2;

	AnimationModeE m_animationMode;
	unsigned m_currentFrame;
	unsigned m_currentAnim;
	double m_currentTime;
	double m_elapsedTime; //2022

	//MM3D_EDIT?
	//NOTE: These are ModelViewport::ViewOptionsE.
	//Maybe they should use draw flags instead?
	int m_canvasDrawMode,m_perspectiveDrawMode;
	//2019: These are draw flags for both types 
	//of views. E.g. DO_BADTEX.
	//https://github.com/zturtleman/mm3d/issues/56
	int m_drawOptions;

//FYI: THIS IS texturewidget.cc STATE. IT HAS
//NO RELATIONSHIP TO Model DATA.
#ifdef MM3D_EDIT
	std::vector<int> m_selectedUv;

	ObserverList m_observers;

	//BackgroundImage *m_background[6];
	std::vector<BackgroundImage*> m_background;

	std::vector<Utility*> m_utils; //2022

	bool			  m_saved;
	SelectionModeE m_selectionMode;
	bool			  m_selecting;

	// What has changed since the last time the observers were notified?
	// See ChangeBits
	int				m_changeBits;

	UndoManager *m_undoMgr;
	//UndoManager *m_animUndoMgr;
	bool m_undoEnabled;
	bool m_undoEnacted; //2022

	//https://github.com/zturtleman/mm3d/issues/56
	ViewportUnits m_viewportUnits;

#endif // MM3D_EDIT

	// Base position for skeletal animations (safe to ignore in MM3D).
	Matrix	m_localMatrix;

	// ModelFilter is defined as a friend so that classes derived from ModelFilter
	// can get direct access to the model primitive lists (yes,it's a messy hack).
	friend class ModelFilter;
};

extern void model_show_alloc_stats();
extern int model_free_primitives();

//errorobj.cc
extern const char *modelErrStr(Model::ModelErrorE,Model*m=nullptr);

//The MM3D code uses list for many
//things that it seems unsuited to.
typedef Model::pos_list pos_list;
typedef Model::infl_list infl_list;

#endif //__MODEL_H
