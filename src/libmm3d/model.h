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
	public:
		// ChangeBits is used to tell Model::Observer objects what has changed
		// about the model.
		enum ChangeBits
		{
			//SelectionChange      = 0x00000001, // General selection change
			SelectionChange      = 0x000000FF, // General selection change
			SelectionVertices    = 0x00000002, // Vertices selection changed
			SelectionFaces       = 0x00000004, // Faces selection changed
			SelectionGroups      = 0x00000008, // Groups selection changed
			SelectionJoints	     = 0x00000010, // Joints selection changed
			SelectionPoints      = 0x00000020, // Points selection changed
			SelectionProjections = 0x00000040, //2019
			AddGeometry          = 0x00000100, // Added or removed objects
			AddAnimation		 = 0x00000200, // Added or removed animations
			AddOther			 = 0x00000400, // Added or removed something else
			MoveGeometry		 = 0x00000800, // Model shape changed
			MoveOther			 = 0x00001000, // Something non-geometric was moved
			AnimationMode        = 0x00002000, // Changed animation mode
			AnimationSet		 = 0x00004000, // Changes to animation sets
			AnimationFrame       = 0x00008000, // Changed current animation frame
			ShowJoints           = 0x00010000, // Joints forced visible
			ShowProjections      = 0x00020000, // Projections forced visible
			ChangeAll			 = 0xFFFFFFFF	// All of the above
		};

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
			PartVertices	 = 0x0001, // 
			PartFaces		 = 0x0002, // 
			PartGroups		= 0x0004, // 
			PartMaterials	= 0x0008, // 
			PartTextures	 = 0x0010, // 
			PartJoints		= 0x0020, // 
			PartPoints		= 0x0040, // 
			PartProjections = 0x0080, // 
			PartBackgrounds = 0x0100, // 
			PartMeta		  = 0x0200, // 
			PartSkelAnims	= 0x0400, // 
			PartFrameAnims  = 0x0800, // 
			PartFormatData  = 0x1000, // 
			PartFilePaths	= 0x2000, // 
			PartAll			= 0xFFFF, // 

			// These are combinations of parts above,for convenience
			PartGeometry	 = PartVertices | PartFaces | PartGroups, // 
			PartTextureMap  = PartFaces | PartGroups | PartMaterials | PartTextures | PartProjections, // 
			PartAnimations  = PartSkelAnims | PartFrameAnims, // 
		};

		enum PartPropertiesE
		{
			PropName		  = 0x000001, // 
			PropType		  = 0x000002, // 
			PropSelection	= 0x000004, // 
			PropVisibility  = 0x000008, // 
			PropFree		  = 0x000010, // 
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
			PropAll			= 0xFFFFFF, // 

			// These are combinations of parts above,for convenience
			PropFlags		 = PropSelection | PropVisibility | PropFree,
		};

		enum //REMOVE ME 
		{
			MAX_INFLUENCES = 4
		};

		enum PositionTypeE
		{
		  PT_Vertex,
		  PT_Joint,
		  PT_Point,
		  PT_Projection,

		  //For the undo system.
		  _OT_Background_, 
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

			operator unsigned&(){ return index; } //2020
			operator unsigned()const{ return index; } //2020
		};
		typedef std::vector<Position> pos_list;

		enum AnimationModeE
		{
			ANIMMODE_NONE = 0,
			ANIMMODE_SKELETAL,
			ANIMMODE_FRAME,
			//UNIMPLEMENTED
			//ANIMMODE_FRAMERELATIVE, // Not implemented
			ANIMMODE_MAX
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

				bool propEqual(const FrameAnimVertex &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
				bool operator==(const FrameAnimVertex &rhs)const
					{ return propEqual(rhs); }

			protected:
				FrameAnimVertex(),~FrameAnimVertex();
				void init();

				static std::vector<FrameAnimVertex*> s_recycle;
				static int s_allocated;
		};

		typedef std::vector<FrameAnimVertex*> FrameAnimVertexList;
		
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
				double m_flatNormals[3];		  // Normal for this triangle
				double m_vertAngles[3];			// Angle of vertices
				double m_kfFlatNormals[3];		// Flat normal, rotated relative to the animating bone joints
				double m_kfNormals[3][3];		 // Final normals, rotated relative to the animating bone joints
				double m_kfVertAngles[3];
				//Can these be one pointer?
				double *m_flatSource;			 // Either m_flatNormals or m_kfFlatNormals
				double *m_normalSource[3];	  // Either m_finalNormals or m_kfNormals
				double *m_angleSource;			 // Either m_vertAngles or m_kfVertAngles
				bool  m_selected;
				bool  m_visible;
				mutable bool m_marked;
				mutable bool m_marked2;
				mutable bool m_userMarked;

				mutable int m_user; //2020: associate an index with a pointer

				int	m_projection;  // Index of texture projection (-1 for none)

				bool propEqual(const Triangle &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
				bool operator==(const Triangle &rhs)const
					{ return propEqual(rhs); }

				void _source(AnimationModeE);

			protected:
				Triangle(),~Triangle();
				void init();

				static std::vector<Triangle*> s_recycle;
				static int s_allocated;
		};

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
				static Vertex *get();
				void release();
				void releaseData(); //2020 ???
				void sprint(std::string &dest);

				double m_coord[3];	  // Absolute vertex location
				double m_kfCoord[3];	// Animated position
				double *m_absSource;  // Points to m_coord or m_kfCoord for drawing
				bool	m_selected;
				bool	m_visible;
				mutable bool m_marked;
				mutable bool m_marked2;

				// If m_free is false,the vertex will be implicitly deleted when
				// all faces using it are deleted
				bool	m_free;
				
				// List of bone joints that move the vertex in skeletal animations.
				infl_list m_influences;

				void _calc_influences(Model&); //2020

				//Each of these is a triangle and the index held by this vertex in
				//that triangle is encoded in the two bits
				//NOTE: Hoping "small string optimization" is in play for vertices
				//that don't touch many triangles, but the terminator wastes space
				//In that case 4 probably allocates memory
				//https://github.com/zturtleman/mm3d/issues/109
				//Can't be stored as indices.
				//std::basic_string<unsigned> m_faces;
				std::vector<std::pair<Triangle*,unsigned>> m_faces;
				void _erase_face(Triangle*,unsigned);

				//This change enables editing a model having vertex animation data
				//https://github.com/zturtleman/mm3d/issues/87
				FrameAnimVertexList m_frames;

				bool propEqual(const Vertex &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
				bool operator==(const Vertex &rhs)const
					{ return propEqual(rhs); }

				void _source(AnimationModeE);

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
				int			m_materialIndex;	 // Material index (-1 for none)

				//FIX ME (TEST ME)
				//Draw code is order-independent. 
				//std::set<int> m_triangleIndices;  // List of triangles in this group
				//std::unordered_set<int> m_triangleIndices;  // List of triangles in this group
				int_list m_triangleIndices;

				// Percentage of blending between flat normals and smooth normals
				// 0 = 0%,255 = 100%
				uint8_t	  m_smooth;

				// Maximum angle around which triangle normals will be blended
				// (ie,if m_angle = 90,triangles with an edge that forms an
				// angle greater than 90 will not be blended).
				uint8_t	  m_angle;

				bool m_selected;
			//	bool m_visible; //UNUSED
				mutable bool m_marked;

				bool propEqual(const Group &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
				bool operator==(const Group &rhs)const
					{ return propEqual(rhs); }

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
					MATTYPE_MAX		=  4	 // For convenience
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
				float			m_ambient[4];
				float			m_diffuse[4];
				float			m_specular[4];
				float			m_emissive[4];

				// Lighting value 0 to 100.0
				float			m_shininess;

				uint8_t		 m_color[4][4];  // Unused


				// The clamp properties determine if the texture map wraps when crossing
				// the 0.0 or 1.0 boundary (false)or if the pixels along the edge are
				// used for coordinates outside the 0.0-1.0 limits (true).
				bool			 m_sClamp;  // horizontal wrap/clamp
				bool			 m_tClamp;  // vertical wrap/clamp

				// Open GL texture index (not actually used in model editing
				// viewports since each viewport needs its own texture index)
				GLuint		  m_texture;

				std::string	m_filename;		 // Absolute path to texture file (for MATTYPE_TEXTURE)
				std::string	m_alphaFilename;  // Unused
				Texture	  *m_textureData;	 // Texture data (for MATTYPE_TEXTURE)

				bool propEqual(const Material &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
				bool operator==(const Material &rhs)const
					{ return propEqual(rhs); }

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

				//int m_objectIndex; //???
				unsigned m_objectIndex; // Joint that this keyframe affects
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
				bool propEqual(const Keyframe &rhs, int propBits = PropAll, double tolerance = 0.00001)const;

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

			Object2020():m_abs(),
			m_absSource(m_abs),m_rot(),
			m_rotSource(m_rot),m_xyz{1,1,1}, //C++11
			m_xyzSource(m_xyz)
			{}

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

			const double *getParams(Interpolant2020E)const;
			const double *getParamsUnanimated(Interpolant2020E)const;
			void getParams(double abs[3], double rot[3], double xyz[3])const;
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
			static Joint *get();
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

			bool m_selected;
			bool m_visible;
			mutable bool m_marked;

			//2020 (drawJoints)
			//TODO: Needs Undo objects. Remove it mutable status.
			mutable bool m_bone;

			//TODO: If Euler angles are easily inverted it makes
			//more sense to just calculate m_inv in validateSkel.
			bool m_absolute_inverse;
			Matrix m_inv;
			Matrix &getAbsoluteInverse() //2020
			{
				if(!m_absolute_inverse)
				{
					m_absolute_inverse = true;
					m_inv = m_absolute.getInverse();
				}
				return m_inv;
			}

			bool propEqual(const Joint &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
			bool operator==(const Joint &rhs)const
				{ return propEqual(rhs); }

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
			static Point *get();
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

			bool m_selected;
			bool m_visible;
			mutable bool m_marked;

			// List of bone joints that move the point in skeletal animations.
			infl_list m_influences;

			void _calc_influences(Model&); //2020

			bool propEqual(const Point &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
			bool operator==(const Point &rhs)const
				{ return propEqual(rhs); }

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

			bool m_selected;
			mutable bool m_marked;

			bool propEqual(const TextureProjection &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
			bool operator==(const TextureProjection &rhs)const
				{ return propEqual(rhs); }

		protected:

			TextureProjection(),~TextureProjection();
			void init();

			static int s_allocated;
		};

		// TODO: Probably should use a map for the KeyframeList
		typedef sorted_ptr_list<Keyframe*> KeyframeList;		
		typedef std::vector<KeyframeList> ObjectKeyframeList;

		class AnimBase2020 //RENAME ME
		{
		public:

			std::string m_name;
			ObjectKeyframeList m_keyframes;
			double m_fps;  // Frames per second
			bool m_wrap; // Whether or not the animation uses wraparound keyframe interpotion
			//UNUSED
			//unsigned m_frameCount;	 // Number of frames in the animation				
			//bool	  m_validNormals;  // Whether or not the normals have been calculated for the current animation frame

			//https://github.com/zturtleman/mm3d/issues/106
			//If 0 m_timetable2020.size is used.
			double m_frame2020;
			std::vector<double> m_timetable2020;
			
			size_t _frame_count()const
			{
				return m_timetable2020.size(); 
			}
			double _time_frame()const
			{
				return m_frame2020?m_frame2020:_frame_count();
			}
			double _frame_time(unsigned frame)const
			{
				return frame<_frame_count()?m_timetable2020[frame]:0;
			}			
						
			bool propEqual(const AnimBase2020 &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
			bool operator==(const AnimBase2020 &rhs)const
				{ return propEqual(rhs); }

			void init(),releaseData();
		};
		//TEMPORARY FIX
		AnimBase2020 *_anim(AnimationModeE,unsigned)const;
		AnimBase2020 *_anim(unsigned,unsigned,Position,bool=true)const;

		// Describes a skeletal animation.
		class SkelAnim : public AnimBase2020
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
		};
		
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

			bool propEqual(const FrameAnimData &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
			bool operator==(const FrameAnimData &rhs)const
			{ return propEqual(rhs); }
		};
		typedef std::vector<FrameAnimData*> FrameAnimDataList;*/

		// Describes a frame animation (also known as "Mesh Deformation Animation").
		// This object contains a list of vertex positions for each vertex for every
		// frame (and also every point for every frame).
		class FrameAnim : public AnimBase2020
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
		};

		// Working storage for an animated vertex.
		class KeyframeVertex
		{
			public:
				double m_coord[3];
				double m_normal[3];
				double m_rotatedNormal[3];
		};

		// Working storage for an animated triangle.
		class KeyframeTriangle
		{
			public:
				double m_normals[3][3];
		};

		// Reference background images for canvas viewports.
		class BackgroundImage : public Object2020
		{
			public:
				BackgroundImage(){ m_xyz[0] = 30; /*???*/ }

				//std::string m_filename;
				//float m_scale;      // 1.0 means 1 GL unit from the center to the edges of the image
				//float m_center[3];  // Point in the viewport where the image is centered

				void sprint(std::string &dest);

				bool propEqual(const BackgroundImage &rhs, int propBits = PropAll, double tolerance = 0.00001)const;
				bool operator==(const BackgroundImage &rhs)const
					{ return propEqual(rhs); }
		};

		//https://github.com/zturtleman/mm3d/issues/56
		struct ViewportUnits
		{
			enum{ VertexSnap=1,UnitSnap=2 };

			int snap;

			enum{ BinaryGrid=0,DecimalGrid,FixedGrid };

			int grid;

			int lines3d;

			double inc,inc3d,ptsz3d;

			int xyz3d; //1|2|4

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
				FormatData()
					: offsetType(),
					  index(),
					  len(),
					  data(){ };
				virtual ~FormatData();

				uint16_t		offsetType;  // 0 = none,is valid
				std::string	format;		// Should not be empty
				uint32_t		index;		 // for formats with multiple data sets
				uint32_t		len;			// length of data in 'data'
				uint8_t	  *data;		  // pointer to data

				virtual void serialize();
		};

		// Arbitrary key/value string pairs. This is used to provide a simple interface
		// for model attributes that are not manipulated by commands/tools. Often
		// user-editable format-specific data is stored as MetaData key/value pairs
		// (for example texture paths in MD2 and MD3 models).
		class MetaData
		{
			public:
				std::string key;
				std::string value;
		};
		typedef std::vector<MetaData> MetaDataList;

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

		enum
		{
			MAX_GROUP_NAME_LEN = 32,
			MAX_BACKGROUND_IMAGES = 6
		};

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
		};

		//REMOVE ME
		/*https://github.com/zturtleman/mm3d/issues/56
		enum DrawJointModeE
		{
			JOINTMODE_NONE = 0,
			JOINTMODE_LINES,
			JOINTMODE_BONES,
			JOINTMODE_MAX
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
				virtual ~Observer(){} //???
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
		bool propEqual(const Model *model, int partBits = PartAll, int propBits = PropAll,
				double tolerance = 0.00001)const;

		//model_print.cc
		void sprint(std::string &dest); //???

		// ------------------------------------------------------------------
		// "Meta" data,model information that is not rendered in a viewport.
		// ------------------------------------------------------------------

		// Indicates if the model has changed since the last time it was saved.
		void setSaved(){ m_undoMgr->setSaved(); }
		bool getSaved()const{ return m_undoMgr->isSaved(); }

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

		// Background image accessors. See the BackgroundImage class.
		bool setBackgroundImage(unsigned index, const char *str);
		bool setBackgroundScale(unsigned index, float scale);
		bool setBackgroundCenter(unsigned index, float x, float y, float z);

		const char *getBackgroundImage(unsigned index)const;
		float getBackgroundScale(unsigned index)const;
		bool getBackgroundCenter(unsigned index, float &x, float &y, float &z)const;

		// These are used to store status messages when the model does not have a status
		// bar. When a model is assigned to a viewport window,the messages will be
		// displayed in the status bar.
		bool hasErrors()const { return !m_loadErrors.empty(); }
		void pushError(const std::string &err);
		std::string popError();

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
		void draw(unsigned drawOptions = DO_TEXTURE,ContextT context = nullptr, float *viewPoint = nullptr);
		void drawLines(float alpha=1);
		void drawVertices(float alpha=1);
		void _drawPolygons(int,bool mark=false); //2019
		void drawPoints();
		void drawProjections();
		void drawJoints(float alpha=1);

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
		bool getDrawProjections()const { return m_drawProjections; };

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

		bool setPositionCoords(const Position &pos, const double coords[3]);
		bool setPositionRotation(const Position &pos, const double rot[3]);
		bool setPositionScale(const Position &pos, const double scale[3]);
		bool setPositionName(const Position &pos, const char *name);		

		const char *getPositionName(const Position &pos)const;
		bool getPositionCoords(const Position &pos, double coords[3])const;
		bool getPositionCoordsUnanimated(const Position &pos, double coords[3])const;
		bool getPositionRotation(const Position &pos, double rot[3])const;
		bool getPositionRotationUnanimated(const Position &pos, double rot[3])const;
		bool getPositionScale(const Position &pos, double scale[3])const;
		bool getPositionScaleUnanimated(const Position &pos, double scale[3])const;		
		bool getPositionParams(const Position &pos, Interpolant2020E, double params[3])const;
		bool getPositionParamsUnanimated(const Position &pos, Interpolant2020E, double params[3])const;
		
		// ------------------------------------------------------------------
		// Animation functions
		// ------------------------------------------------------------------

		bool setCurrentAnimation(AnimationModeE m, const char *name);
		bool setCurrentAnimation(AnimationModeE m, unsigned index);
		inline bool setCurrentAnimation(AnimationModeE m, int index)
		{
			return setCurrentAnimation(m,(unsigned)index);
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
		bool setCurrentAnimationTime(double seconds, int loop=-1, AnimationTimeE=AT_calculateNormals);

		//2020: Try to make a new frame at the current time.
		//The returned value is the new current frame which
		//may or may not be a different value since usually
		//the value will be 1 more than the current but not
		//always when inserting to the front.
		unsigned makeCurrentAnimationFrame()
		{
			auto cmp = insertAnimFrame(m_animationMode,m_currentAnim,m_currentTime);
			if(cmp!=m_currentFrame) setCurrentAnimationFrame(cmp,AT_invalidateAnim);
			return cmp;
		}

		unsigned getCurrentAnimation()const;
		unsigned getCurrentAnimationFrame()const;
		//2020: Internally the time is stored in frame units so it's possible to compare
		//the value to the frames' timestamps.
		double getCurrentAnimationFrameTime()const;
		double getCurrentAnimationTime()const;

		// Stop animation mode; Go back to standard pose editing.
		void setNoAnimation();

		AnimationModeE getAnimationMode()const{ return m_animationMode; };
		bool inSkeletalMode()const{ return m_animationMode==ANIMMODE_SKELETAL; };

		// Common animation properties
		int addAnimation(AnimationModeE mode, const char *name);
		void deleteAnimation(AnimationModeE mode, unsigned index);

		unsigned getAnimCount(AnimationModeE m)const;

		const char *getAnimName(AnimationModeE mode, unsigned anim)const;
		bool setAnimName(AnimationModeE mode, unsigned anim, const char *name);

		double getAnimFPS(AnimationModeE mode, unsigned anim)const;
		bool setAnimFPS(AnimationModeE mode, unsigned anim, double fps);
		
		bool getAnimWrap(AnimationModeE mode, unsigned anim)const;
		bool setAnimWrap(AnimationModeE mode, unsigned anim, bool wrap);

		//https://github.com/zturtleman/mm3d/issues/106
		//2020: Get m_frame2020 for sparse animation data. Units are in frames.
		double getAnimTimeFrame(AnimationModeE mode, unsigned anim)const;
		bool setAnimTimeFrame(AnimationModeE mode, unsigned anim, double time);
		//NOTE: Units are frames. Caller is responsible for uniqueness/sorting.
		double getAnimFrameTime(AnimationModeE mode, unsigned anim, unsigned frame)const;
		bool setAnimFrameTime(AnimationModeE mode, unsigned anim, unsigned frame, double time);	

		//2020: WHAT VERB IS NORMATIVE IN THIS CASE?
		//Efficiently get frame/convenience function. Units are in frames.
		unsigned getAnimFrame(AnimationModeE mode, unsigned anim, double time)const;

		unsigned getAnimFrameCount(AnimationModeE mode, unsigned anim)const;
		
		typedef std::vector<FrameAnimVertex*> FrameAnimData;
		bool setAnimFrameCount(AnimationModeE mode, unsigned anim, unsigned count);
		bool setAnimFrameCount(AnimationModeE mode, unsigned anim, unsigned count, unsigned where, FrameAnimData*);

		//2020: Formerly clearAnimFrame.
		bool deleteAnimFrame(AnimationModeE mode, unsigned anim, unsigned frame);

		//2020: Same deal as makeCurrentAnimationFrame
		unsigned insertAnimFrame(AnimationModeE mode, unsigned anim, double time);

		// Frame animation geometry
		//void setFrameAnimVertexCount(unsigned vertexCount);
		//void setFrameAnimPointCount(unsigned pointCount);

		//TODO: interpFrameAnimVertexCoords?
		bool setFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
				double x, double y, double z, Interpolate2020E interp=InterpolateKeep);
		//TODO: Remove ability to return InterpolateVoid.
		//CAUTION: xyz HOLD JUNK IF RETURNED VALUE IS 0!!
		Interpolate2020E getFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
				double &x, double &y, double &z)const;
		//2020: These won't return InterpolateVoid.
		Interpolate2020E hasFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex)const;

		//REMOVED		
		//https://github.com/zturtleman/mm3d/issues/109
		//Instead of this use getNormal with setCurrentAnimationFrame.
		//bool getFrameAnimVertexNormal(unsigned anim, unsigned frame, unsigned vertex,
		//		double &x, double &y, double &z)const;

		// Not undo-able
		//NOTE: Historically InterpolateStep is a more correct default but my sense is
		//users would prefer importers to defaul to InterpolateLerp.
		bool setQuickFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
				double x, double y, double z, Interpolate2020E interp=InterpolateLerp);
		/*2020 Not undo-able
		bool setQuickFrameAnimPoint(unsigned anim, unsigned frame, unsigned point,
				double x, double y, double z,
				double rx, double ry, double rz, Interpolate2020E interp[2]=nullptr);*/

		/*REFERENCE
		bool setFrameAnimPointCoords(unsigned anim, unsigned frame, unsigned point,
				double x, double y, double z, Interpolate2020E interp=InterpolateKeep);
		//TODO: Remove ability to return InterpolateVoid.
		Interpolate2020E getFrameAnimPointCoords(unsigned anim, unsigned frame, unsigned point,
				double &x, double &y, double &z)const;
		//2020: These won't return InterpolateVoid.
		Interpolate2020E hasFrameAnimPointCoords(unsigned anim, unsigned frame, unsigned point)const;

		bool setFrameAnimPointRotation(unsigned anim, unsigned frame, unsigned point,
				double x, double y, double z, Interpolate2020E interp=InterpolateKeep);
		//TODO: Remove ability to return InterpolateVoid.
		Interpolate2020E getFrameAnimPointRotation(unsigned anim, unsigned frame, unsigned point,
				double &x, double &y, double &z)const;
		//2020: Won't return InterpolateVoid.
		Interpolate2020E hasFrameAnimPointRotation(unsigned anim, unsigned frame, unsigned point)const;

		bool setFrameAnimPointScale(unsigned anim, unsigned frame, unsigned point,
				double x, double y, double z, Interpolate2020E interp=InterpolateKeep);
		//TODO: Remove ability to return InterpolateVoid.
		Interpolate2020E getFrameAnimPointScale(unsigned anim, unsigned frame, unsigned point,
				double &x, double &y, double &z)const;
		//2020: Won't return InterpolateVoid.
		Interpolate2020E hasFrameAnimPointScale(unsigned anim, unsigned frame, unsigned point)const;
		*/

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

		//MEMORY LEAK (removeKeyframe leaks if not using undo system.)
		bool deleteKeyframe(unsigned anim, unsigned frame, Position joint, KeyType2020E isRotation=KeyAny);

		/*REFERENCE
		// Interpolate what a keyframe for this joint would be at the specified frame.
		bool interpSkelAnimKeyframe(unsigned anim, unsigned frame,
				bool loop, unsigned joint, KeyType2020E isRotation,
				double &x, double &y, double &z)const;
		// Interpolate what a keyframe for this joint would be at the specified time.
		bool interpSkelAnimKeyframeTime(unsigned anim, double frameTime,
				bool loop, unsigned joint, Matrix &relativeFinal)const;
		*/
		//2020: Avoids time/frame resolution.		
		int interpKeyframe(unsigned anim, unsigned frame, Position, Matrix &relativeFinal)const;
		int interpKeyframe(unsigned anim, unsigned frame, double frameTime, Position, Matrix &relativeFinal)const;
		int interpKeyframe(unsigned anim, unsigned frame, Position, double trans[3], double rot[3], double scale[3])const;
		int interpKeyframe(unsigned anim, unsigned frame, double frameTime, Position, double trans[3], double rot[3], double scale[3])const;
		int interpKeyframe(unsigned anim, unsigned frame, unsigned vertex, double trans[3])const;
		int interpKeyframe(unsigned anim, unsigned frame, double frameTime, unsigned vertex, double trans[3])const;

		//2020: This doesn't copy FrameAnimVertex data.
		AnimBase2020 *_dup(AnimationModeE mode, AnimBase2020 *copy, bool keyframes=true);

		// Animation set operations
		int copyAnimation(AnimationModeE mode, unsigned anim, const char *newName=nullptr);
		int splitAnimation(AnimationModeE mode, unsigned anim, const char *newName, unsigned frame);
		bool joinAnimations(AnimationModeE mode, unsigned anim1, unsigned anim2);
		bool mergeAnimations(AnimationModeE mode, unsigned anim1, unsigned anim2);
		//2020: "how" is what interploation mode to use, "how2" is what to use for frames that retain
		//the same value, i.e. InterpolateCopy. The default behavior is to use "how" for this purpose.
		int convertAnimToFrame(AnimationModeE mode, unsigned anim1, const char *newName, unsigned frameCount, Interpolate2020E how, Interpolate2020E how2=InterpolateNone);

		bool moveAnimation(AnimationModeE mode, unsigned oldIndex, unsigned newIndex);

		// For undo,don't call these directly
		bool insertKeyframe(unsigned anim, PositionTypeE, Keyframe *keyframe);
		bool removeKeyframe(unsigned anim, unsigned frame, Position, KeyType2020E isRotation,bool release = false);

		// Merge all animations from model into this model.
		// For skeletal,skeletons must match
		bool mergeAnimations(Model *model);

		/*https://github.com/zturtleman/mm3d/issues/87
		// When a model has frame animations,adding or removing primitives is disabled
		// (because of the large amount of undo information that would have to be stored).
		// Use this function to force an add or remove (you must make sure that you adjust
		// the frame animations appropriately as well as preventing an undo on the
		// operation you are performing).
		//
		// In other words,setting this to true is probably a really bad idea unless
		// you know what you're doing.
		void forceAddOrDelete(bool o);

		bool canAddOrDelete()const{ return m_frameAnims.empty()||m_forceAddOrDelete; };
		
		// Show an error because the user tried to add or remove primitives while
		// the model has frame animations.
		//bool displayFrameAnimPrimitiveError();
		*/

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
		bool getNormalUnanimated(unsigned triangleNum, unsigned vertexIndex, double *normal)const;
		bool getNormal(unsigned triangleNum, unsigned vertexIndex, double *normal)const;
		bool getFlatNormalUnanimated(unsigned triangleNum, double *normal)const;
		bool getFlatNormal(unsigned triangleNum, double *normal)const;

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

		void invalidateAnim();
		void invalidateAnim(AnimationModeE,unsigned,unsigned);
		//NEW: Defer animation calculations same as normals calculations.
		void validateAnim()const,calculateAnim();

		void invertNormals(unsigned triangleNum);
		bool triangleFacesIn(unsigned triangleNum);

		// ------------------------------------------------------------------
		// Geometry functions
		// ------------------------------------------------------------------

		inline int getVertexCount()	const { return m_vertices.size(); }
		inline int getTriangleCount() const { return m_triangles.size(); }
		inline int getBoneJointCount()const { return m_joints.size(); }
		inline int getPointCount()	 const { return m_points.size(); }
		inline int getProjectionCount()const { return m_projections.size(); }

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
		typedef std::vector<Point*> _PointList;
		typedef const std::vector<const Point*> PointList;
		PointList &getPointList(){ return *(PointList*)&m_points; };
		typedef std::vector<TextureProjection*> _ProjectionList;
		typedef const std::vector<const TextureProjection*> ProjectionList;
		ProjectionList &getProjectionList(){ return *(ProjectionList*)&m_projections; };
		typedef std::vector<SkelAnim*> _SkelAnimList;
		typedef const std::vector<const SkelAnim*> SkelAnimList;
		SkelAnimList &getSkelList(){ return *(SkelAnimList*)&m_skelAnims; };
		typedef std::vector<FrameAnim*> _FrameAnimList;
		typedef const std::vector<const FrameAnim*> FrameAnimList;
		FrameAnimList &getFrameList(){ return *(FrameAnimList*)&m_frameAnims; };

		//bool getPositionCoords(const Position &pos, double *coord)const;

		int addVertex(double x, double y, double z);
		int addTriangle(unsigned vert1, unsigned vert2, unsigned vert3);

		void deleteVertex(unsigned vertex);
		void deleteTriangle(unsigned triangle);

		// No undo on this one
		void setVertexFree(unsigned v,bool o);
		bool isVertexFree(unsigned v)const;

		// When all faces attached to a vertex are deleted,the vertex is considered
		// an "orphan" and deleted (unless it is a "free" vertex,see m_free in the
		// vertex class).
		void deleteOrphanedVertices();

		// A flattened triangle is a triangle with two or more corners that are
		// assigned to the same vertex (this usually happens when vertices are
		// welded together).
		void deleteFlattenedTriangles();

		bool isTriangleMarked(unsigned t)const;

		void subdivideSelectedTriangles();
		//void unsubdivideTriangles(CAN'T BE UNDONE... IT'S REALLY FOR MU_SubdivideSelected use.) 
		void subdivideSelectedTriangles_undo(unsigned t1, unsigned t2, unsigned t3, unsigned t4);

		// If co-planer triangles have edges with points that are co-linear they
		// can be combined into a single triangle. The simplifySelectedMesh function
		// performs this operation to combine all faces that do not add detail to
		// the model.
		void simplifySelectedMesh();

		bool setTriangleVertices(unsigned triangleNum, unsigned vert1, unsigned vert2, unsigned vert3);
		bool getTriangleVertices(unsigned triangleNum, unsigned &vert1, unsigned &vert2, unsigned &vert3)const;
		void setTriangleMarked(unsigned triangleNum,bool marked);
		void clearMarkedTriangles();

		bool getVertexCoordsUnanimated(unsigned vertexNumber, double *coord)const;
		bool getVertexCoords(unsigned vertexNumber, double *coord)const;
		/*2019: Removing this to remove dependency on ViewRight/Left.
		//It is used only by implui/texturecoord.cc. It's not a big enough piece of Model to warrant
		//existing.
		//bool getVertexCoords2d(unsigned vertexNumber,ProjectionDirectionE dir, double *coord)const;
		*/

		int getTriangleVertex(unsigned triangleNumber, unsigned vertexIndex)const;

		void booleanOperation(BooleanOpE op,
				int_list &listA,int_list &listB);

		Model *copySelected()const;

		bool duplicateSelected(); //2020 (dupcmd)

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

		// Textures and Color materials go into the same material list
		int addTexture(Texture *tex);
		int addColorMaterial(const char *name);

		bool setGroupTextureId(unsigned groupNumber, int textureId);

		void deleteGroup(unsigned group);
		void deleteTexture(unsigned texture);

		const char *getGroupName(unsigned groupNum)const;
		bool setGroupName(unsigned groupNum, const char *groupName);

		inline int getGroupCount()const { return m_groups.size(); };
		int getGroupByName(const char *groupName,bool ignoreCase = false)const;
		int getMaterialByName(const char *materialName,bool ignoreCase = false)const;
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

		void setTextureName(unsigned textureId, const char *name);
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
		bool setTextureSClamp(unsigned textureId,bool clamp);
		bool setTextureTClamp(unsigned textureId,bool clamp);

		int_list getUngroupedTriangles()const;
		//int_list getGroupTriangles(unsigned groupNumber)const;
		const int_list &getGroupTriangles(unsigned groupNumber)const; //2020
		int getGroupTextureId(unsigned groupNumber)const;

		int getTriangleGroup(unsigned triangleNumber)const;

		void addTriangleToGroup(unsigned groupNum, unsigned triangleNum);
		void removeTriangleFromGroup(unsigned groupNum, unsigned triangleNum);

		void setSelectedAsGroup(unsigned groupNum);
		void addSelectedToGroup(unsigned groupNum);

		bool getTextureCoords(unsigned triangleNumber, unsigned vertexIndex, float &s, float &t)const;
		bool setTextureCoords(unsigned triangleNumber, unsigned vertexIndex, float s, float t);

		// ------------------------------------------------------------------
		// Skeletal structure and influence functions
		// ------------------------------------------------------------------
		
		//NOTE: Internally translation is all that matters. Rotation is
		//just a number. As such it's best to keep it at 0,0,0 unless a
		//tool changes the value for book-keeping purposes.
		int addBoneJoint(const char *name, double x, double y, double z,
				//2020: I think this is unnecessary. The loaders (filters)
				//aren't using addBoneJoint.
				/*double xrot, double yrot, double zrot,*/
				int parent = -1);

		void deleteBoneJoint(unsigned joint);

		const char *getBoneJointName(unsigned joint)const;
		int getBoneJointParent(unsigned joint)const;
		bool getBoneJointCoords(unsigned jointNumber, double *coord)const;
		bool getBoneJointCoordsUnanimated(unsigned jointNumber, double *coord)const;
		bool getBoneJointRotation(unsigned jointNumber, double *coord)const;
		bool getBoneJointRotationUnanimated(unsigned jointNumber, double *coord)const;
		bool getBoneJointScale(unsigned jointNumber, double *coord)const;
		bool getBoneJointScaleUnanimated(unsigned jointNumber, double *coord)const;

		bool getBoneJointFinalMatrix(unsigned jointNumber,Matrix &m)const;
		bool getBoneJointAbsoluteMatrix(unsigned jointNumber,Matrix &m)const;
		bool getBoneJointRelativeMatrix(unsigned jointNumber,Matrix &m)const;
		bool getPointFinalMatrix(unsigned pointNumber,Matrix &m)const;
		bool getPointAbsoluteMatrix(unsigned pointNumber,Matrix &m)const;
		
		//REMOVE ME
		//Only dupcmd uses this in a way that looks bad. 
		void getBoneJointVertices(int joint, int_list&)const;

		//REMOVE US
		//I think these date back to a time when primitives were stuck to one joint apiece.
		bool setPositionBoneJoint(const Position &pos, int joint);
		bool setVertexBoneJoint(unsigned vertex, int joint);
		bool setPointBoneJoint(unsigned point, int joint);

		bool addPositionInfluence(const Position &pos, unsigned joint,InfluenceTypeE type, double weight);
		bool addVertexInfluence(unsigned vertex, unsigned joint,InfluenceTypeE type, double weight);
		bool addPointInfluence(unsigned point, unsigned joint,InfluenceTypeE type, double weight);

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

		bool autoSetPositionInfluences(const Position &pos, double sensitivity,bool selected);
		bool autoSetVertexInfluences(unsigned vertex, double sensitivity,bool selected);
		bool autoSetPointInfluences(unsigned point, double sensitivity,bool selected);
		bool autoSetCoordInfluences(double *coord, double sensitivity,bool selected,int_list &infList);

		bool setBoneJointName(unsigned joint, const char *name);
		bool setBoneJointParent(unsigned joint, int parent = -1);
		bool setBoneJointRotation(unsigned j, const double *rot);
		//UNUSED: Had taken relative coordinates. Don't want that.
		//bool setBoneJointTranslation(unsigned j, const double *trans);

		double calculatePositionInfluenceWeight(const Position &pos, unsigned joint)const;
		double calculateVertexInfluenceWeight(unsigned vertex, unsigned joint)const;
		double calculatePointInfluenceWeight(unsigned point, unsigned joint)const;
		double calculateCoordInfluenceWeight(const double *coord, unsigned joint)const;

		//RENAME/REMOVE ME
		//2020: I'm hijacking this API in order to keep the geometry up-to-date.
		static void _calculateRemainderWeight(infl_list &list);
		void calculateRemainderWeight(const Position&);

		bool getBoneVector(unsigned joint, double *vec, const double *coord)const;

		// No undo on this one
		bool relocateBoneJoint(unsigned j, double x, double y, double z);
				
		// ------------------------------------------------------------------
		// Point functions
		// ------------------------------------------------------------------

		int addPoint(const char *name, double x, double y, double z, double xrot, double yrot, double zrot);

		void deletePoint(unsigned point);

		int getPointByName(const char *name)const;

		const char *getPointName(unsigned point)const;
		bool setPointName(unsigned point, const char *name);

		bool getPointCoords(unsigned pointNumber, double *coord)const;
		bool getPointCoordsUnanimated(unsigned point, double *trans)const;
		bool getPointRotation(unsigned point, double *rot)const;
		bool getPointRotationUnanimated(unsigned point, double *rot)const;
		bool getPointScale(unsigned point, double *rot)const;
		bool getPointScaleUnanimated(unsigned point, double *rot)const;

		bool setPointRotation(unsigned point, const double *rot);
		bool setPointCoords(unsigned point, const double *trans);

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

		void setTriangleProjection(unsigned triangleNum, int proj);
		int getTriangleProjection(unsigned triangleNum)const;

		void applyProjection(unsigned int proj);

		// ------------------------------------------------------------------
		// Undo/Redo functions
		// ------------------------------------------------------------------

		bool setUndoEnabled(bool o);
		bool getUndoEnabled(){ return m_undoEnabled; } //NEW

		// Indicates that a user-specified operation is complete. A single
		// "operation" may span many function calls and different types of
		// manipulations.
		void operationComplete(const char *opname = nullptr);

		// Clear undo list
		void clearUndo();

		bool canUndo()const;
		bool canRedo()const;
		void undo();
		void redo();

		// If a manipulations have been performed,but not commited as a single
		// undo list,undo those operations (often used when the user clicks
		// a "Cancel" button to discard "unapplied" changes).
		void undoCurrent();

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

		void insertGroup(unsigned index,Group *group);
		void removeGroup(unsigned index);

		void insertBoneJoint(unsigned index,Joint *joint);
		void removeBoneJoint(unsigned index);

		void insertInfluence(const Position &pos, unsigned index, const InfluenceT &influence);
		void removeInfluence(const Position &pos, unsigned index);

		void insertPoint(unsigned index,Point *point);
		void removePoint(unsigned index);

		void insertProjection(unsigned index,TextureProjection *proj);
		void removeProjection(unsigned index);

		void insertTexture(unsigned index,Material *material);
		void removeTexture(unsigned index);

		//INTERNAL: addAnimation subroutines
		void insertFrameAnim(unsigned index,FrameAnim *anim);
		void removeFrameAnim(unsigned index);

		//INTERNAL: addAnimation subroutines
		void insertSkelAnim(unsigned anim,SkelAnim *fa);
		void removeSkelAnim(unsigned anim);

		//INTERNAL: setFrameCount subroutines
		void insertFrameAnimData(unsigned frame0, unsigned frames, FrameAnimData *data, FrameAnim *draw);
		void removeFrameAnimData(unsigned frame0, unsigned frames, FrameAnimData *data);

		// ------------------------------------------------------------------
		// Selection functions
		// ------------------------------------------------------------------

		void setSelectionMode(SelectionModeE m);
		inline SelectionModeE getSelectionMode(){ return m_selectionMode; };

		unsigned getSelectedVertexCount()const;
		unsigned getSelectedTriangleCount()const;
		unsigned getSelectedBoneJointCount()const;
		unsigned getSelectedPointCount()const;
		unsigned getSelectedProjectionCount()const;

		void getSelectedTriangles(int_list &l)const;
		void getSelectedGroups(int_list &l)const;

		//2020: I've put PT_Vertex on the back of the
		//Position lists. See definition for thinking.
		void getSelectedPositions(pos_list &l)const;
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
		void getSelectedInterpolation(AnimationModeE am, unsigned anim, unsigned frame, Get3<Interpolate2020E>);

		void unselectAll();				
		void unselectAllVertices();
		void unselectAllTriangles();
		void unselectAllGroups(); 
		bool unselectAllBoneJoints();
		bool unselectAllPoints();
		bool unselectAllProjections();

		// A selection test is an additional condition you can attach to whether
		// or not an object in the selection volume should be selected. For example,
		// this is used to add a test for which way a triangle is facing so that
		// triangles not facing the camera can be excluded from the selection.
		// You can implement a selection test for any property that you can check
		// on the primitive.
		class SelectionTest
		{
			public:
				virtual ~SelectionTest(){};
				virtual bool shouldSelect(void *element)= 0;
		};

		bool selectInVolumeMatrix(const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test = nullptr);
		bool unselectInVolumeMatrix(const Matrix &viewMat, double x1, double y1, double x2, double y2,SelectionTest *test = nullptr);

		bool getBoundingRegion(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const;
		bool getSelectedBoundingRegion(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const;

		bool getBounds(Model::OperationScopeE os, double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const
		{
			return (this->*(os==OS_Selected?&Model::getSelectedBoundingRegion:&Model::getBoundingRegion))(x1,y1,z1,x2,y2,z2);
		}

		// CAUTION: No longer clears keyframes in ANIMMODE_SKELETON
		void deleteSelected();

		// When changing the selection state of a lot of primitives,a difference
		// list is used to track undo information. These calls indicate when the
		// undo information should start being tracked and when it should be
		// completed.
		void beginSelectionDifference();
		void endSelectionDifference();

		bool selectVertex(unsigned v);
		bool unselectVertex(unsigned v);
		bool isVertexSelected(unsigned v)const;

		bool selectTriangle(unsigned t);
		//2019: This is too dangerous. Don't use it.
		//It calls selectVerticesFromTriangles every
		//time.
		bool unselectTriangle(unsigned t, bool selectVerticesFromTriangles=false);
		bool isTriangleSelected(unsigned t)const;

		bool selectGroup(unsigned g);
		bool unselectGroup(unsigned g);
		bool isGroupSelected(unsigned g)const;

		bool selectBoneJoint(unsigned j);
		bool unselectBoneJoint(unsigned j);
		bool isBoneJointSelected(unsigned j)const;

		bool selectPoint(unsigned p);
		bool unselectPoint(unsigned p);
		bool isPointSelected(unsigned p)const;

		bool selectProjection(unsigned p);
		bool unselectProjection(unsigned p);
		bool isProjectionSelected(unsigned p)const;

		// The behavior of this function changes based on the selection mode.
		bool invertSelection();

		// Select all vertices that have the m_free property set and are not used
		// by any triangles (this can be used to clean up unused free vertices,
		// like a manual analog to the deleteOrphanedVertices()function).
		void selectFreeVertices();

		//FIX ME
		//https://github.com/zturtleman/mm3d/issues/63
		//#ifdef MM3D_EDIT 
		//TextureWidget API (texwidget.cc)
		void setSelectedUv(const std::vector<int> &uvList);
		void getSelectedUv(std::vector<int> &uvList)const;
		void clearSelectedUv();
		//https://github.com/zturtleman/mm3d/issues/56
		//ModelViewport API (modelviewport.cc)
		ViewportUnits &getViewportUnits(){ return m_viewportUnits; }
		//#endif

		// ------------------------------------------------------------------
		// Hide functions
		// ------------------------------------------------------------------

		bool hideSelected();
		bool hideUnselected();
		bool unhideAll();

		bool isVertexVisible(unsigned v)const;
		bool isTriangleVisible(unsigned t)const;
		//bool isGroupVisible(unsigned g)const; //UNUSED
		bool isBoneJointVisible(unsigned j)const;
		bool isPointVisible(unsigned p)const;

		// Don't call these directly... use selection/hide selection
		bool hideVertex(unsigned);
		bool unhideVertex(unsigned);
		bool hideTriangle(unsigned);
		bool unhideTriangle(unsigned);
		bool hideJoint(unsigned);
		bool unhideJoint(unsigned);
		bool hidePoint(unsigned);
		bool unhidePoint(unsigned);

		// ------------------------------------------------------------------
		// Transform functions
		// ------------------------------------------------------------------

		//NOTICE: These all use MU_MovePrimitive whereas setPositionCenter
		//and setPointTranslation, etc. use MU_SetObjectXYZ (formerly there
		//was an undo object of this type for every combination of types and
		//parameters.) MU_MovePrimitive keeps a mapped array of reassignments.
		//FIX ME? MU_TranslateSelected, etc. exists via translateSelected, etc!
		bool movePosition(const Position &pos, double x, double y, double z);
		bool moveVertex(unsigned v, double x, double y, double z);
		bool moveBoneJoint(unsigned j, double x, double y, double z);
		bool movePoint(unsigned p, double x, double y, double z);
		bool moveProjection(unsigned p, double x, double y, double z);

		//FIX ME 
		//Call sites all use vectors. What does it 
		//even mean to "translate" with a matrix??
		//void translateSelected(const Matrix &m);
		void translateSelected(const double vec[3]);
		void rotateSelected(const Matrix &m, double *point);
		void interpolateSelected(Interpolant2020E,Interpolate2020E);

		// Applies arbitrary matrix to primitives (selected or all based on scope).
		// Some matrices cannot be undone (consider a matrix that scales a dimension to 0).
		// If the matrix cannot be undone,set undoable to false (a matrix is undoable if
		// the determinant is not equal to zero).
		//void applyMatrix(const Matrix &m,OperationScopeE scope,bool animations,bool undoable);
		void applyMatrix(Matrix, OperationScopeE scope, bool animations, bool undoable);

	protected:

		// ==================================================================
		// Protected methods
		// ==================================================================

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

		void beginHideDifference();
		void endHideDifference();

		// When primitives of one type are hidden,other primitives may need to
		// be hidden at the same time.
		void hideVerticesFromTriangles();
		void unhideVerticesFromTriangles();

		void hideTrianglesFromVertices();
		void unhideTrianglesFromVertices();

		// ------------------------------------------------------------------
		// Selection
		// ------------------------------------------------------------------

		bool selectVerticesInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);
		bool selectTrianglesInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,bool connected,SelectionTest *test = nullptr);
		bool selectGroupsInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);
		bool selectBoneJointsInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);
		bool selectPointsInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);
		bool selectProjectionsInVolumeMatrix(bool select, const Matrix &viewMat, double a1, double b1, double a2, double b2,SelectionTest *test = nullptr);

		// When primitives of one type are selected,other primitives may need to
		// be selected at the same time.
		void selectTrianglesFromGroups();
		public: //This needs to be manually callable, so unselectTriangle in loop
			//is less pathological.
		void selectVerticesFromTriangles();
		protected:
		void selectTrianglesFromVertices(bool all = true);
		void selectGroupsFromTriangles(bool all = true);

		public:
		bool parentJointSelected(int joint)const;
		bool directParentJointSelected(int joint)const;
		protected:

		// ------------------------------------------------------------------
		// Undo
		// ------------------------------------------------------------------

		void sendUndo(Undo *undo,bool listCombine = false);
		//Fix for setSelectedUv.
		//There is a sequencing problem. This adds the undo
		//onto the back of the previous operation unless an
		//open operation exists.
		//https://github.com/zturtleman/mm3d/issues/90
		void appendUndo(Undo *undo);

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
		_PointList m_points;
		_ProjectionList m_projections;
		_FrameAnimList m_frameAnims;		
		_SkelAnimList m_skelAnims;

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

		AnimationModeE m_animationMode;
		unsigned m_currentFrame;
		unsigned m_currentAnim;
		double m_currentTime;

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

		bool			  m_saved;
		SelectionModeE m_selectionMode;
		bool			  m_selecting;

		// What has changed since the last time the observers were notified?
		// See ChangeBits
		int				m_changeBits;

		UndoManager *m_undoMgr;
		UndoManager *m_animUndoMgr;
		bool m_undoEnabled;

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
