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
#include "texture.h"
#include "log.h"

static bool model_inner_recycle = true;

int Model::Vertex::s_allocated = 0;
int Model::Triangle::s_allocated = 0;
int Model::Group::s_allocated = 0;
int Model::Material::s_allocated = 0;
int Model::Keyframe::s_allocated = 0;
int Model::Joint::s_allocated = 0;
int Model::Point::s_allocated = 0;
int Model::TextureProjection::s_allocated = 0;
int Model::Animation::s_allocated = 0;
int Model::FrameAnimVertex::s_allocated = 0;
//int Model::FrameAnimPoint::s_allocated = 0;

//FIX THESE: list/pop_front for stack????
std::vector<Model::Vertex*> Model::Vertex::s_recycle;
std::vector<Model::Triangle*> Model::Triangle::s_recycle;
std::vector<Model::Group*> Model::Group::s_recycle;
std::vector<Model::Material*> Model::Material::s_recycle;
std::vector<Model::Keyframe*> Model::Keyframe::s_recycle;
std::vector<Model::Joint*> Model::Joint::s_recycle;
std::vector<Model::Point*> Model::Point::s_recycle;
std::vector<Model::Animation*> Model::Animation::s_recycle;
std::vector<Model::FrameAnimVertex*> Model::FrameAnimVertex::s_recycle;
//std::vector<Model::FrameAnimPoint*> Model::FrameAnimPoint::s_recycle;

const double EQ_TOLERANCE = 0.00001;

template<typename T>
bool floatCompareVector(T *lhs,T *rhs, size_t len, double tolerance = EQ_TOLERANCE)
{
	for(size_t index = 0; index<len; ++index)
		if(fabs(lhs[index]-rhs[index])>tolerance)
			return false;
	return true;
}

static bool influencesMatch(const infl_list &lhs,
		const infl_list &rhs, int propBits, double tolerance)
{
	//2020: Seems like silly business to me?
	//typedef std::unordered_map<int,Model::InfluenceT> InfluenceMap;
	//InfluenceMap lhsInfMap;

	infl_list::const_iterator it,jt;

	/*
	for(it = lhs.begin(); it!=lhs.end(); ++it)
	{
		lhsInfMap[it->m_boneId] = *it;
	}*/

	for(it = rhs.begin(); it!=rhs.end(); it++)
	{
		for(jt = lhs.begin(); jt!=lhs.end(); jt++)
		{
			if(it->m_boneId==jt->m_boneId) break;
		}

		//InfluenceMap::const_iterator lhs_it = lhsInfMap.find(it->m_boneId);
		//if(lhs_it==lhsInfMap.end())
		if(jt==lhs.end())
		{
			log_warning("match failed on missing lhs bone %d\n",it->m_boneId);
			return false;
		}

		// This doesn't matter if we only care about weights
		if(propBits&Model::PropInfluences)
		{
			//if(lhs_it->second.m_type!=it->m_type)
			if(jt->m_type!=it->m_type)
			{
				log_warning("match failed on influence type (%d vs %d)for bone %d\n",
						jt->m_type,it->m_type,it->m_boneId);
				return false;
			}
		}

		//if(fabs(lhs_it->second.m_weight-it->m_weight)>tolerance)
		if(fabs(jt->m_weight-it->m_weight)>tolerance)
		{
			log_warning("match failed on influence weight (%f vs %f)for bone %d\n",
					(float)jt->m_weight,(float)it->m_weight,it->m_boneId);
			return false;
		}

		//lhsInfMap.erase(it->m_boneId);
	}

	/*if(!lhsInfMap.empty())*/
	if(rhs.size()>lhs.size())
	{
		//log_warning("match failed on missing rhs bone %d\n",
		//		lhsInfMap.begin()->second.m_boneId);
		log_warning("match failed on missing rhs bone %d\n",lhs[rhs.size()].m_boneId);
		return false;
	}

	return true;
}

Model::Vertex::Vertex()
	 //:
	 //m_selected(false),
	 //m_visible(true),
	 //m_free(false),
	 //m_absSource(m_coord)
{
	s_allocated++;

	init(); //2020
}

Model::Vertex::~Vertex()
{
	s_allocated--;

	init(); //2020
}

void Model::Vertex::init()
{
	m_selected = false;

	//m_visible = true;
	Visible2022::init();

	//m_free = false;

	m_absSource = m_coord;

	releaseData(); //2020 //???
}

int Model::Vertex::flush()
{
	int c = 0;
	std::vector<Vertex*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it;
		it++;
		c++;
	}
	s_recycle.clear();
	return c;
}

void Model::Vertex::stats()
{
	log_debug("Vertex: %d/%d\n",s_recycle.size(),s_allocated);
}

Model::Vertex *Model::Vertex::get(unsigned layer, AnimationModeE am)
{
	Vertex *v; if(!s_recycle.empty())
	{
		v = s_recycle.back();
		s_recycle.pop_back();
		v->init();
	}
	else v = new Vertex();

	if(layer>1) v->hide(layer);
	else assert(layer!=0);

	v->_source(am); return v;
}

void Model::Vertex::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else delete this;
}
void Model::Vertex::releaseData() //2020
{
	m_influences.clear();
	m_faces.clear();
	for(auto j=m_frames.size();j-->0;)
	m_frames[j]->release();
	m_frames.clear();
}

bool Model::Vertex::propEqual(const Vertex &rhs, int propBits, double tolerance)const
{
	if((propBits &PropCoords)!=0)
	{
		if(!floatCompareVector(m_coord,rhs.m_coord,3,tolerance))
			return false;
	}

	if((propBits &PropSelection))
		if(m_selected!=rhs.m_selected)
			return false;

	if((propBits &PropVisibility))
		if(m_visible1!=rhs.m_visible1)
			return false;

	/*if((propBits &PropFree))
		if(m_free!=rhs.m_free)
			return false;*/

	if((propBits &(PropInfluences | PropWeights))!=0)
	{
		if(!influencesMatch(m_influences,rhs.m_influences,propBits,tolerance))
			return false;
	}

	if((propBits &(PropInfluences | PropWeights))!=0)
	{
		if(!influencesMatch(m_influences,rhs.m_influences,propBits,tolerance))
			return false;
	}

	if((propBits&PartFrameAnims)!=0)
	{
		size_t i = 0, iN = m_frames.size();
		if(iN==rhs.m_frames.size()) 
		for(;i<iN;i++) if(!m_frames[i]->propEqual(*rhs.m_frames[i],propBits,tolerance))
		return false; else return false;
	}

	return true;
}

Model::Triangle::Triangle()
	  //: 
	  //m_selected(false),
	  //m_visible(true),
	  //m_marked(false),
	  //m_projection(-1),
	  //m_group(-1) //2022
{
	init();

	s_allocated++;
}

Model::Triangle::~Triangle()
{
	s_allocated--;
}

void Model::Triangle::init()
{
	m_s[0] = 0;
	m_t[0] = 1;
	m_s[1] = 0;
	m_t[1] = 0;
	m_s[2] = 1;
	m_t[2] = 0;

	m_selected = false;
	m_marked = false;

	//m_visible  = true;
	Visible2022::init();

	m_projection = -1;
	m_group = -1; //2022

	m_flatSource = m_flatNormal;
	m_normalSource[0] = m_finalNormals[0];
	m_normalSource[1] = m_finalNormals[1];
	m_normalSource[2] = m_finalNormals[2];
	m_angleSource = m_vertAngles;
}

int Model::Triangle::flush()
{
	int c = 0;
	std::vector<Triangle*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it++; c++;
	}
	s_recycle.clear(); return c;
}

void Model::Triangle::stats()
{
	log_debug("Triangle: %d/%d\n",s_recycle.size(),s_allocated);
}

Model::Triangle *Model::Triangle::get(unsigned layer)
{
	Triangle *v;
	if(!s_recycle.empty())
	{
		v = s_recycle.back();
		s_recycle.pop_back();
		v->init();
	}
	else v = new Triangle();

	if(layer>1) v->hide(layer);
	else assert(layer!=0);

	return v;
}

void Model::Triangle::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else delete this;
}

bool Model::Triangle::propEqual(const Triangle &rhs, int propBits, double tolerance)const
{
	if((propBits &PropVertices)!=0)
	{
		if(m_vertexIndices[0]!=rhs.m_vertexIndices[0]
		 ||m_vertexIndices[1]!=rhs.m_vertexIndices[1]
		 ||m_vertexIndices[2]!=rhs.m_vertexIndices[2])
		{
			return false;
		}
	}

	if((propBits &PropProjections)!=0)
	if(m_projection!=rhs.m_projection)
	return false;

	if((propBits &PropGroups)!=0)
	if(m_group!=rhs.m_group)
	return false;

	if((propBits &PropTexCoords)!=0)
	for(int i = 0; i<3; ++i)
	{
		if(fabs(m_s[i]-rhs.m_s[i])>tolerance)
		return false;
		if(fabs(m_t[i]-rhs.m_t[i])>tolerance)
		return false;
	}

	if((propBits &PropSelection)!=0)
	if(m_selected!=rhs.m_selected)
	return false;

	if((propBits &PropVisibility)!=0)
	if(m_visible1!=rhs.m_visible1)
	return false;

	return true;
}

Model::Group::Group()
	: m_materialIndex(-1),
	  m_smooth(255),
	  m_angle(89),
	  m_selected(false)
//	  m_visible(true) //UNUSED
{
	s_allocated++;
}

Model::Group::~Group()
{
	s_allocated--;
}

void Model::Group::init()
{
	m_materialIndex = -1;
	m_smooth = 255;
	m_angle = 89;
	m_selected = false;
//	m_visible = true;
	m_name.clear(); //ABUSED (init?)
	m_triangleIndices.clear(); //ABUSED (init?)
	m_utils.clear(); //ABUSED
}

int Model::Group::flush()
{
	int c = 0;
	std::vector<Group*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it++; c++;
	}
	s_recycle.clear(); return c;
}

void Model::Group::stats()
{
	log_debug("Group: %d/%d\n",s_recycle.size(),s_allocated);
}

Model::Group *Model::Group::get()
{
	if(!s_recycle.empty())
	{
		Group *v = s_recycle.back();
		s_recycle.pop_back();
		v->init();
		return v;
	}
	else return new Group();
}

void Model::Group::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else delete this;
}

bool Model::Group::propEqual(const Group &rhs, int propBits, double tolerance)const
{
	if((propBits &PropNormals)!=0)
	{
		if(m_smooth!=rhs.m_smooth)
			return false;
		if(m_angle!=rhs.m_angle)
			return false;
	}

	if((propBits &PropMaterials)!=0)
		if(m_materialIndex!=rhs.m_materialIndex)
			return false;

	if((propBits &PropName)!=0)
		if(m_name!=rhs.m_name)
			return false;

	if((propBits &PropSelection)!=0)
		if(m_selected!=rhs.m_selected)
			return false;

//	if((propBits &PropVisibility)!=0)
//		if(m_visible1!=rhs.m_visible1)
//			return false;

	//Note, unless rhs and *this belong to different
	//models this makes no sense.
	if((propBits &PropTriangles)!=0)
		if(m_triangleIndices!=rhs.m_triangleIndices)
			return false;

	return true;
}

Model::Material::Material()
	: m_type(MATTYPE_TEXTURE),
	  m_sClamp(false),
	  m_tClamp(false),
	m_accumulate(),
	  m_texture(0),
	  m_textureData(nullptr)
{
	s_allocated++;
}

Model::Material::~Material()
{
	s_allocated--;
	// Do NOT free m_textureData.  TextureManager does that
}

void Model::Material::init()
{
	m_name.clear(); //ABUSED (init?)
	m_filename.clear(); //ABUSED (init?)
	m_alphaFilename.clear(); //ABUSED (init?)
	m_textureData	= nullptr;
	m_texture		 = 0;
	m_type			 = MATTYPE_TEXTURE;
	m_sClamp		  = false;
	m_tClamp		  = false;
	m_accumulate = false; //2021
}

int Model::Material::flush()
{
	int c = 0;
	std::vector<Material*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it;
		it++;
		c++;
	}
	s_recycle.clear();
	return c;
}

void Model::Material::stats()
{
	log_debug("Material: %d/%d\n",s_recycle.size(),s_allocated);
}

Model::Material *Model::Material::get()
{
	if(!s_recycle.empty())
	{
		Material *v = s_recycle.back();
		s_recycle.pop_back();
		v->init();
		return v;
	}
	return new Material();
}

void Model::Material::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else delete this;
}

bool Model::Material::propEqual(const Material &rhs, int propBits, double tolerance)const
{
	if(propBits&PropType&&m_type!=rhs.m_type)
			return false;

	if((propBits &PropLighting)!=0)
	{
		if(!floatCompareVector(m_ambient,rhs.m_ambient,4,tolerance))
			return false;
		if(!floatCompareVector(m_diffuse,rhs.m_diffuse,4,tolerance))
			return false;
		if(!floatCompareVector(m_specular,rhs.m_specular,4,tolerance))
			return false;
		if(!floatCompareVector(m_emissive,rhs.m_emissive,4,tolerance))
			return false;
		if(fabs(m_shininess-rhs.m_shininess)>tolerance)
			return false;
	}

	if(propBits&PropClamp)
	{
		if(m_sClamp!=rhs.m_sClamp||m_tClamp!=rhs.m_tClamp)
			return false;
	}

	// Color is unused

	if((propBits &PropPixels)!=0)
	{
		// If one is nullptr,the other must be also
		if((m_textureData==nullptr)!=(rhs.m_textureData==nullptr))
			return false;

		// Compare texture itself
		if(m_textureData)
		{
			Texture::CompareResultT res;
			if(!m_textureData->compare(rhs.m_textureData,&res,0))
				return false;

			// Name and filename should not be an issue (should match material)
		}
		else
		{
			// If no texture data,must not be a texture-mapped material
			if(m_type==Model::Material::MATTYPE_TEXTURE)
				return false;
		}
	}

	if(propBits&PropName&&m_name!=rhs.m_name)
			return false;

	if(propBits&PropPaths&&m_filename!=rhs.m_filename)
			return false;

	if(propBits&PropPaths&&m_alphaFilename!=rhs.m_alphaFilename)
			return false;

	return true;
}

Model::Keyframe::Keyframe()
{
	init(); //2020 (UNUSED)

	s_allocated++;
}

Model::Keyframe::~Keyframe()
{
	s_allocated--;
}

void Model::Keyframe::init()
{
	//NOP???

	m_interp2020 = InterpolateLerp; //NEW

	memset(m_selected,false,sizeof(m_selected)); //graph.cc
}

int Model::Keyframe::flush()
{
	int c = 0;
	std::vector<Keyframe*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it;
		it++;
		c++;
	}
	s_recycle.clear();
	return c;
}

void Model::Keyframe::stats()
{
	log_debug("Keyframe: %d/%d\n",s_recycle.size(),s_allocated);
}

Model::Keyframe *Model::Keyframe::get()
{
	if(!s_recycle.empty())
	{
		Keyframe *v = s_recycle.back();
		s_recycle.pop_back();
		v->init();
		return v;
	}
	return new Keyframe();
}

void Model::Keyframe::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else delete this;
}

bool Model::Keyframe::propEqual(const Keyframe &rhs, int propBits, double tolerance)const
{
	if(m_objectIndex!=rhs.m_objectIndex)
	return false;

	if((propBits &PropType)!=0)
	{
		if(m_isRotation!=rhs.m_isRotation)
		return false;

		if(m_interp2020!=rhs.m_interp2020) //2021
		return false;
	}

	if((propBits &PropTime)!=0)
	{
		if(m_frame!=rhs.m_frame)
		return false;
	//	if(fabs(m_time-rhs.m_time)>tolerance)
	//	return false;
	}

	if((m_isRotation==KeyRotate&&(propBits &PropRotation)!=0)
	 ||(m_isRotation==KeyTranslate&&(propBits &PropCoords)!=0)
	 ||(m_isRotation==KeyScale&&(propBits &PropScale)!=0)) //2021
	{
		if(!floatCompareVector(m_parameter,rhs.m_parameter,3,tolerance))
		return false;
	}

	return true;
}

void Model::Object2020::init(PositionTypeE t)
{
	m_name.clear();
	
	memset(m_abs,0x00,sizeof(m_abs));
	memset(m_rot,0x00,sizeof(m_rot));
	for(int i=3;i-->0;) m_xyz[i] = 1;
	
	//WARNING: Can't rely on this!!
	m_absSource = m_abs;
	m_rotSource = m_rot;
	m_xyzSource = m_xyz;

	m_selected = false;

	m_type = t;
}

Model::Joint::Joint()
{
	s_allocated++;
	init();
}

Model::Joint::~Joint()
{
	//init(); //???
	s_allocated--;
}

void Model::Joint::init()
{
	Object2020::init(PT_Joint); //2022
	
	m_parent = -1; //2022???

	//m_visible = true;
	Visible2022::init();

	m_bone = true;

	for(int i=0;i<3;i++)
	{
		m_rel[0] = m_kfRel[0] = m_kfRot[0] = 0;
		m_kfXyz[1] = 1;
	}

	//2022: I guess?
	m_absolute.loadIdentity();
	m_relative.loadIdentity();
	m_final.loadIdentity();

	_dirty_mask = ~0;
}

Model::Joint *Model::Joint::get(unsigned layer, AnimationModeE am)
{
	Joint *v; 
	if(!s_recycle.empty())
	{
		v = s_recycle.back();
		s_recycle.pop_back();
		v->init();
	}
	else v = new Joint();

	if(layer>1) v->hide(layer);
	else assert(layer!=0);

	v->_source(am); return v;
}

void Model::Joint::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else delete this;
}

int Model::Joint::flush()
{
	int c = 0;
	std::vector<Joint*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it;
		it++;
		c++;
	}
	s_recycle.clear();
	return c;
}

void Model::Joint::stats()
{
	log_debug("Joint: %d/%d\n",s_recycle.size(),s_allocated);
}

bool Model::Joint::propEqual(const Joint &rhs, int propBits, double tolerance)const
{
	if((propBits &PropRotation)!=0)
	{
		for(unsigned i = 0; i<3; i++)
		{
			if(fabs(m_rot[i]-rhs.m_rot[i])>tolerance)
				return false;
		}
	}
	if((propBits &PropCoords)!=0)
	{
		for(unsigned i = 0; i<3; i++)
		{
			if(fabs(m_rel[i]-rhs.m_rel[i])>tolerance)
				return false;
		}
	}

	if(m_parent!=rhs.m_parent)
		return false;

	if((propBits &PropName)!=0)
		if(m_name!=rhs.m_name)
			return false;

	if((propBits &PropSelection)!=0)
		if(m_selected!=rhs.m_selected)
			return false;

	if((propBits &PropVisibility)!=0)
		if(m_visible1!=rhs.m_visible1)
			return false;

	return true;
}

Model::Point::Point()
{
	s_allocated++;
	init();
}

Model::Point::~Point()
{
	//init(); //???
	s_allocated--;
}

void Model::Point::init()
{
	Object2020::init(PT_Point); //2022

	//m_visible = true;
	Visible2022::init();

	for(int i=0;i<3;i++)
	{
		m_kfAbs[0] = m_kfRot[0] = 0;
		m_kfXyz[1] = 1;
	}

	m_influences.clear();
}

Model::Point *Model::Point::get(unsigned layer, AnimationModeE am)
{
	Point *v;
	if(!s_recycle.empty())
	{
		v = s_recycle.back();
		s_recycle.pop_back();
		v->init();
	}
	else v = new Point();

	if(layer>1) v->hide(layer);
	else assert(layer!=0);

	v->_source(am); return v;
}

void Model::Point::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else delete this;
}

int Model::Point::flush()
{
	int c = 0;
	std::vector<Point*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it;
		it++;
		c++;
	}
	s_recycle.clear();
	return c;
}

void Model::Point::stats()
{
	log_debug("Point: %d/%d\n",s_recycle.size(),s_allocated);
}

bool Model::Point::propEqual(const Point &rhs, int propBits, double tolerance)const
{
	if((propBits &PropCoords)!=0)
		if(!floatCompareVector(m_abs,rhs.m_abs,3,tolerance))
			return false;

	if((propBits &PropRotation)!=0)
		if(!floatCompareVector(m_rot,rhs.m_rot,3,tolerance))
			return false;

	if((propBits &PropName)!=0)
		if(m_name!=rhs.m_name)
			return false;

	if((propBits &PropSelection)!=0)
		if(m_selected!=rhs.m_selected)
			return false;

	if((propBits &PropVisibility)!=0)
		if(m_visible1!=rhs.m_visible1)
			return false;
  	
	if((propBits &(PropInfluences | PropWeights))!=0)
	{
		if(!influencesMatch(m_influences,rhs.m_influences,propBits,tolerance))
			return false;
	}	

	return true;
}

Model::TextureProjection::TextureProjection()
{
	s_allocated++;
	init();
}

Model::TextureProjection::~TextureProjection()
{
	//init(); //???
	s_allocated--;
}

void Model::TextureProjection::init()
{
	Object2020::init(PT_Projection); //2022

	m_type = 0;

	m_range[0][0] = 0.0;
	m_range[0][1] = 0.0;
	m_range[1][0] = 1.0;
	m_range[1][1] = 1.0;
}

Model::TextureProjection *Model::TextureProjection::get()
{
	/*
	if(!s_recycle.empty())
	{
		TextureProjection *v = s_recycle.back();
		s_recycle.pop_back();
		v->init();
		return v;
	}
	else
	*/
	{
		return new TextureProjection();
	}
}

void Model::TextureProjection::release()
{
	/*
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else
	*/
	{
		delete this;
	}
}

int Model::TextureProjection::flush()
{
	int c = 0;
	/*
	std::vector<TextureProjection*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it;
		it++;
		c++;
	}
	s_recycle.clear();
	*/
	return c;
}

void Model::TextureProjection::stats()
{
	log_debug("TextureProjection: %d/%d\n",0,s_allocated);
}

bool Model::TextureProjection::propEqual(const TextureProjection &rhs, int propBits, double tolerance)const
{
	if((propBits &PropType)!=0)
		if(m_type!=rhs.m_type)
			return false;

	if((propBits &PropCoords)!=0)
		if(!floatCompareVector(m_abs,rhs.m_abs,3,tolerance))
			return false;

	if((propBits &PropRotation)!=0)
	{
		if(!floatCompareVector(m_xyz,rhs.m_xyz,3,tolerance))
			return false;
		if(!floatCompareVector(m_rot,rhs.m_rot,3,tolerance))
			return false;
	}

	if((propBits &PropDimensions)!=0)
	{
		if(!floatCompareVector(m_range[0],rhs.m_range[0],2,tolerance))
			return false;
		if(!floatCompareVector(m_range[1],rhs.m_range[1],2,tolerance))
			return false;
	}

	if((propBits &PropName)!=0)
		if(m_name!=rhs.m_name)
			return false;

	if((propBits &PropSelection)!=0)
		if(m_selected!=rhs.m_selected)
			return false;

	return true;
}

Model::Animation::Animation()
{
	s_allocated++; init();
}
Model::Animation::~Animation()
{
	s_allocated--; init();
}
void Model::Animation::init()
{
	//CAUTION: Several fields are assigned 
	//by addAnimation.

	_type = 0;
	//m_name = "skeletal"; //Note: m_name is std::string
	m_fps = 10; //10???
	m_frame2020 = -1; //0; //-1 is for loading old files
	m_wrap = false; //true;
	m_frame0 = ~0; //0

	releaseData(); //???

	m_timetable2020.clear(); //2020 //Assuming correct???
}
void Model::Animation::releaseData()
{
	for(auto&ea:m_keyframes)
	{
		for(auto*ea2:ea.second) ea2->release();

		ea.second.clear();
	}
	m_keyframes.clear();
}
Model::Animation *Model::Animation::get()
{
	if(!s_recycle.empty())
	{
		Animation *val = s_recycle.back();
		s_recycle.pop_back();
		return val;
	}
	else return new Animation;
}
void Model::Animation::release()
{
	if(model_inner_recycle)
	{
		init();
		s_recycle.push_back(this);
	}
	else delete this;
}
int Model::Animation::flush()
{
	int c = 0;
	auto it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it; it++; c++;
	}
	s_recycle.clear(); return c;
}
void Model::Animation::stats()
{
	log_debug("Animation: %d/%d\n",s_recycle.size(),s_allocated);
}
bool Model::Animation::propEqual(const Animation &rhs, int propBits, double tolerance)const
{
	if((propBits &PropName)!=0)
	{
		if(m_name!=rhs.m_name)
		{
			log_warning("match failed at anim name, lhs = '%s', rhs = '%s'\n",
					m_name.c_str(),rhs.m_name.c_str());
			return false;
		}
	}

	if((propBits &PropTime)!=0)
	{
		if(fabs(m_fps-rhs.m_fps)>tolerance)
		{
			log_warning("match failed at anim fps, lhs = %f, rhs = %f\n",
					(float)m_fps,(float)rhs.m_fps);
			return false;
		}
	}

	if((propBits &PropWrap)!=0)
	{
		if(m_wrap!=rhs.m_wrap)
		{
			log_warning("match failed at anim wrap, lhs = %d, rhs = %d\n",
					m_wrap,rhs.m_wrap);
			return false;
		}
	}

	if(_frame_count()!=rhs._frame_count())
	{
		if((propBits &PropDimensions)!=0)
		{
			log_warning("match failed at anim frame count, lhs = %d, rhs = %d\n",
					_frame_count(),rhs._frame_count());
			return false;
		}

		if((propBits &PropTimestamps)!=0) //2020
		{
			for(auto i=_frame_count();i-->0;)
			if(fabs(m_timetable2020[i]-rhs.m_timetable2020[i])>tolerance)
			{
				log_warning("match failed at anim fps, lhs = %f, rhs = %f\n",
						(float)m_timetable2020[i],(float)rhs.m_timetable2020[i]);
				return false;
			}
		}
	}

	if((propBits&PropKeyframes)!=0)
	{
		if(m_keyframes.size()!=rhs.m_keyframes.size())
		{
			log_warning("match failed at anim keyframe size, lhs = %d, rhs = %d\n",
					m_keyframes.size(),rhs.m_keyframes.size());
			return false;
		}

		auto lhs_it = m_keyframes.begin();
		auto rhs_it = rhs.m_keyframes.begin();
		for(;lhs_it!=m_keyframes.end();lhs_it++,rhs_it++)
		{
			//WARNING: m_keyframes is unordered_map
			//(Assuming same order where identical)

			//2021: This had been implied before.
			if(lhs_it->first!=rhs_it->first) return false; 

			auto &a = lhs_it->second, &b = rhs_it->second;

			if(a.size()!=b.size()) return false;

			for(auto l=a.begin(),r=b.begin();l!=a.end();l++,r++)			
			if(!(*l)->propEqual(**r,propBits,tolerance))
			{
				return false;
			}
		}
	}

	return true;
}

Model::FrameAnimVertex::FrameAnimVertex()
{
	s_allocated++;
	init();
}

Model::FrameAnimVertex::~FrameAnimVertex()
{
	s_allocated--;
}

void Model::FrameAnimVertex::init()
{
	for(unsigned t=3;t-->0;)
	{
		m_coord[t] = 0; //m_normal[t] = 0;
	}
	m_interp2020 = InterpolateNone; //NEW (2020)
}

Model::FrameAnimVertex *Model::FrameAnimVertex::get()
{
	if(s_recycle.empty())
	{
		return new FrameAnimVertex;
	}
	else
	{
		FrameAnimVertex *val = s_recycle.back();
		val->init();
		s_recycle.pop_back();
		return val;
	}
}

void Model::FrameAnimVertex::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else
	{
		delete this;
	}
}

int Model::FrameAnimVertex::flush()
{
	int c = 0;
	std::vector<FrameAnimVertex*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it;
		it++;
		c++;
	}
	s_recycle.clear();
	return c;
}

void Model::FrameAnimVertex::stats()
{
	log_debug("FrameAnimVertex: %d/%d\n",s_recycle.size(),s_allocated);
}

bool Model::FrameAnimVertex::propEqual(const FrameAnimVertex &rhs, int propBits, double tolerance)const
{	
	if((propBits &PropType)!=0) //???
	{
		if(m_interp2020!=rhs.m_interp2020) return false;
	}

	if((propBits &PropCoords)!=0) //???
	{
		if(!floatCompareVector(m_coord,rhs.m_coord,3,tolerance))
		{
			log_warning("frame anim vertex coord mismatch,lhs (%f,%f,%f)rhs (%f,%f,%f)\n",
					(float)m_coord[0],(float)m_coord[1],(float)m_coord[2],
					(float)rhs.m_coord[0],(float)rhs.m_coord[1],(float)rhs.m_coord[2]);
			return false;
		}
	}

	return true;
}

/*REFERENCE
Model::FrameAnimPoint::FrameAnimPoint()
{
	s_allocated++;
	init();
}
Model::FrameAnimPoint::~FrameAnimPoint()
{
	s_allocated--;
}
void Model::FrameAnimPoint::init()
{
	for(unsigned t = 0; t<3; t++)
	{
		m_abs[t] = 0; m_rot[t] = 0; m_xyz[t] = 0;

		m_interp2020[t] = InterpolateNone; //2020
	}
}
Model::FrameAnimPoint *Model::FrameAnimPoint::get()
{
	if(s_recycle.empty())
	{
		return new FrameAnimPoint;
	}
	else
	{
		FrameAnimPoint *val = s_recycle.back();
		val->init();
		s_recycle.pop_back();
		return val;
	}
}
void Model::FrameAnimPoint::release()
{
	if(model_inner_recycle)
	{
		s_recycle.push_back(this);
	}
	else
	{
		delete this;
	}
}
int Model::FrameAnimPoint::flush()
{
	int c = 0;
	std::vector<FrameAnimPoint*>::iterator it = s_recycle.begin();
	while(it!=s_recycle.end())
	{
		delete *it;
		it++;
		c++;
	}
	s_recycle.clear();
	return c;
}
void Model::FrameAnimPoint::stats()
{
	log_debug("FrameAnimPoint: %d/%d\n",s_recycle.size(),s_allocated);
}
bool Model::FrameAnimPoint::propEqual(const FrameAnimPoint &rhs, int propBits, double tolerance)const
{
	if((propBits &PropCoords)!=0)
	{
		if(!floatCompareVector(m_abs,rhs.m_abs,3,tolerance))
		{
			log_warning("frame anim point translation mismatch,lhs (%f,%f,%f)rhs (%f,%f,%f)tolerance (%f)\n",
					(float)m_abs[0],(float)m_abs[1],(float)m_abs[2],
					(float)rhs.m_abs[0],(float)rhs.m_abs[1],(float)rhs.m_abs[2],
					(float)tolerance);
			return false;
		}
	}

	if((propBits &PropRotation)!=0)
	{
		if(!floatCompareVector(m_rot,rhs.m_rot,3,tolerance))
		{
			log_warning("frame anim point rotation mismatch,lhs (%f,%f,%f)rhs (%f,%f,%f)\n",
					(float)m_rot[0],(float)m_rot[1],(float)m_rot[2],
					(float)rhs.m_rot[0],(float)rhs.m_rot[1],(float)rhs.m_rot[2]);
			return false;
		}
	}

	return true;
}*/

/*REFERENCE
bool Model::FrameAnimData::propEqual(const FrameAnimData &rhs, int propBits, double tolerance)const
{
	if((propBits &(PropCoords | PropRotation))!=0)
	{
		if(m_frameVertices.size()!=rhs.m_frameVertices.size())
		{
			log_warning("anim frame vertex size mismatch lhs %d,rhs %d\n",
					m_frameVertices.size(),rhs.m_frameVertices.size());
			return false;
		}

		if(m_framePoints.size()!=rhs.m_framePoints.size())
		{
			log_warning("anim frame point size mismatch lhs %d,rhs %d\n",
					m_framePoints.size(),rhs.m_framePoints.size());
			return false;
		}

		auto lv = m_frameVertices.begin();
		auto rv = rhs.m_frameVertices.begin();
		for(; lv!=m_frameVertices.end()&&rv!=rhs.m_frameVertices.end(); ++lv,++rv)
		{
			if(!(*lv)->propEqual(**rv,propBits,tolerance))
			{
				log_warning("anim frame vertex mismatch at %d\n",
						lv-m_frameVertices.begin());
				return false;
			}
		}

		auto lp = m_framePoints.begin();
		auto rp = rhs.m_framePoints.begin();
		for(; lp!=m_framePoints.end()&&rp!=rhs.m_framePoints.end(); ++lp,++rp)
		{
			if(!(*lp)->propEqual(**rp,propBits,tolerance))
			{
				log_warning("anim frame point mismatch at %d\n",
						lp-m_framePoints.begin());
				return false;
			}
		}
	}

	return true;
}*/

Model::FormatData::~FormatData()
{
	if(data)
	{
		delete[] data;
		data = nullptr;
	}
}

void Model::FormatData::serialize()
{
	// Implement this if you derive from Model::FormatData
	// The default implementation assumes that data never changes
	// after it is assigned, so it doesn't need to be re-serialized
}

#ifdef MM3D_EDIT

bool Model::BackgroundImage::propEqual(const BackgroundImage &rhs, int propBits, double tolerance)const
{
	if((propBits &PropPaths)!=0)
		if(m_name!=rhs.m_name)
			return false;

	if((propBits &PropScale)!=0)
		if(fabs(m_xyz[0]-rhs.m_xyz[0])>tolerance)
			return false;

	if((propBits &PropCoords)!=0)
		if(!floatCompareVector(m_abs,rhs.m_abs,3,tolerance))
			return false;

	return true;
}

#endif // MM3D_EDIT

