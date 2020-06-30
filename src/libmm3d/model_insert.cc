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
#include "log.h"

#ifdef MM3D_EDIT

void Model::insertVertex(unsigned index, Model::Vertex *vertex)
{
	//if(m_animationMode) return; //REMOVE ME
	invalidateAnim(); vertex->_source(m_animationMode); //OVERKILL

	m_changeBits |= AddGeometry;

	invalidateNormals(); //OVERKILL

	if(index==m_vertices.size())
	{
		m_vertices.push_back(vertex);
	}
	else if(index<m_vertices.size())
	{
		//unsigned count = 0;
		//std::vector<Vertex*>::iterator it;
		//for(it = m_vertices.begin(); it!=m_vertices.end(); it++)
		{
			//if(count==index)
			{
				//m_vertices.insert(it,vertex);
				m_vertices.insert(m_vertices.begin()+index,vertex);
				adjustVertexIndices(index,+1);
				//break;
			}
			//count++;
		}
	}
	else
	{
		log_error("insertVertex(%d)index out of range\n",index);
	}
}

void Model::removeVertex(unsigned index)
{
	//if(m_animationMode) return; //REMOVE ME

	m_changeBits |= AddGeometry;

	invalidateNormals(); //OVERKILL

	if(index<m_vertices.size())
	{
		//unsigned count = 0;
		//std::vector<Vertex*>::iterator it;
		//for(it = m_vertices.begin(); it!=m_vertices.end(); it++)
		{
			//if(count==index)
			{
				//m_vertices.erase(it);
				m_vertices.erase(m_vertices.begin()+index);
				adjustVertexIndices(index,-1);
				//break;
			}
			//count++;
		}
	}
	else
	{
		log_error("removeVertex(%d)index out of range\n",index);
	}
}

void Model::insertTriangle(unsigned index, Model::Triangle *triangle)
{
	if(index>m_triangles.size())
	return log_error("insertTriangle(%d)index out of range\n",index);

	//if(m_animationMode) return; //REMOVE ME
	invalidateAnim(); triangle->_source(m_animationMode); //OVERKILL

	m_changeBits |= AddGeometry;

	invalidateNormals(); //OVERKILL

	//2020: Keep connectivity to help calculateNormals
	auto &vi = triangle->m_vertexIndices;
	for(int i=3;i-->0;)
	m_vertices[vi[i]]->m_faces.push_back({triangle,i});

	if(index<m_triangles.size())
	{
		//FIX US
		//https://github.com/zturtleman/mm3d/issues/92
		m_triangles.insert(m_triangles.begin()+index,triangle);
		adjustTriangleIndices(index,+1);
	}
	else m_triangles.push_back(triangle);
}

void Model::removeTriangle(unsigned index)
{
	//if(m_animationMode) return; //REMOVE ME

	if(index<m_triangles.size())
	{		
		m_changeBits |= AddGeometry;

		invalidateNormals(); //OVERKILL

		//2020: Keep connectivity to help calculateNormals
		auto triangle = m_triangles[index];
		auto &vi = triangle->m_vertexIndices;
		for(int i=3;i-->0;)
		m_vertices[vi[i]]->_erase_face(triangle,i);

		//FIX US
		//https://github.com/zturtleman/mm3d/issues/92
		m_triangles.erase(m_triangles.begin()+index);
		adjustTriangleIndices(index,-1);
	}
	else log_error("removeTriangle(%d)index out of range\n",index);
}

void Model::insertGroup(unsigned index,Model::Group *group)
{
	if(index>m_groups.size())
	return log_error("insertGroup(%d)index out of range\n",index); 

	//if(m_animationMode) return; //REMOVE ME

	m_changeBits |= AddOther;

	m_groups.insert(m_groups.begin()+index,group);
}

void Model::removeGroup(unsigned index)
{
	//if(m_animationMode) return; //REMOVE ME

	m_changeBits |= AddOther;

	if(index<m_groups.size())
	{
		m_groups.erase(m_groups.begin()+index);
	}
	else log_error("removeGroup(%d)index out of range\n",index);
}

void Model::insertBoneJoint(unsigned index, Joint *joint)
{
	if(index>m_joints.size())
	return log_error("insertBoneJoint(%d)index out of range\n",index);

	//if(m_animationMode) return; //REMOVE ME?
	invalidateAnim(); joint->_source(m_animationMode);
	
	m_changeBits |= AddOther;
		
	if(index<m_joints.size())
	{
		// Adjust parent relationships
		for(unsigned j=0;j<m_joints.size();j++)
		if(m_joints[j]->m_parent>=(signed)index)
		{
			m_joints[j]->m_parent++;
		}

		// Adjust joint indices of keyframes after this joint
		for(auto*sa:m_skelAnims)		
		for(unsigned j=index;j<sa->m_keyframes.size();j++)
		for(unsigned k=0;k<sa->m_keyframes[j].size();k++)
		{
			sa->m_keyframes[j][k]->m_objectIndex++;
		}
	
		// Adjust vertex assignments
		for(auto*ea:m_vertices) for(auto&i:ea->m_influences)			
		{
			if(i.m_boneId>=(signed)index) i.m_boneId++;
		}

		// Adjust point assignments
		for(auto*ea:m_points) for(auto&i:ea->m_influences)
		{
			if(i.m_boneId>=(signed)index) i.m_boneId++;
		}

		m_joints.insert(m_joints.begin()+index,joint);
	}
	else m_joints.push_back(joint);

	// Insert joint into keyframe list
	for(auto*sa:m_skelAnims)
	{
		//log_debug("inserted keyframe list for joint %d\n",j); //???
		sa->m_keyframes.insert(sa->m_keyframes.begin()+index,KeyframeList());
	}

	invalidateSkel(); //m_validJoints = false;
}

void Model::removeBoneJoint(unsigned index)
{
	//if(m_animationMode) return; //REMOVE ME

	if(index>=m_joints.size())
	return log_error("removeBoneJoint(%d)index out of range\n",index);

	//log_debug("removeBoneJoint(%d)\n",index); //???

	m_changeBits |= AddOther;

	// Adjust parent relationships
	for(size_t j=m_joints.size();j-->0;)
	{
		if(m_joints[j]->m_parent==(signed)index)
		{
			m_joints[j]->m_parent = m_joints[index]->m_parent;
		}
		else if(m_joints[j]->m_parent>(signed)index)
		{
			m_joints[j]->m_parent--;
		}
	}

	//DUPLICATES removePoint
	for(unsigned anim=0;anim<m_skelAnims.size();anim++) 
	{
		auto sa = m_skelAnims[anim];

		// Adjust skeletal animations
		if(index<sa->m_keyframes.size())
		{
			// Delete joint keyframes
			for(auto&kf=sa->m_keyframes[index];!kf.empty();)
			{
				//log_debug("deleting keyframe %d for joint %d\n",index,joint); //???

				deleteKeyframe(anim,kf.back()->m_frame,{PT_Joint,index});
			}
				
			// Remove joint from keyframe list
			{
				//log_debug("removed keyframe list for joint %d\n",index); //???
						
				sa->m_keyframes.erase(sa->m_keyframes.begin()+index);
			}
		}
		else assert(index<sa->m_keyframes.size()); //2020

		// Adjust joint indices of keyframes after this joint
		for(unsigned j=index;j<sa->m_keyframes.size();j++)
		{
			for(size_t i=sa->m_keyframes[j].size();i-->0;)
			{
				sa->m_keyframes[j][i]->m_objectIndex--;
			}
		}
	}

	// Adjust vertex assignments
	for(auto*ea:m_vertices) for(auto&i:ea->m_influences)			
	{
		if(i.m_boneId>(signed)index) i.m_boneId--;
	}

	// Adjust point assignments
	for(auto*ea:m_points) for(auto&i:ea->m_influences)
	{
		if(i.m_boneId>(signed)index) i.m_boneId--;
	}
				
	m_joints.erase(m_joints.begin()+index);

	invalidateSkel(); //m_validJoints = false;
}

void Model::insertInfluence(const Position &pos, unsigned index, const InfluenceT &influence)
{
	m_changeBits |= AddOther;

	infl_list *l = nullptr;
	if(pos.type==PT_Vertex)
	{
		if(pos.index<m_vertices.size())
		{
			l = &m_vertices[pos.index]->m_influences;
		}
	}
	else if(pos.type==PT_Point)
	{
		if(pos.index<m_points.size())
		{
			l = &m_points[pos.index]->m_influences;
		}
	}

	if(l==nullptr)
	{
		return;
	}

	infl_list::iterator it = l->begin();

	while(index>0)
	{
		index--;
		it++;
	}

	if(it==l->end())
	{
		l->push_back(influence);
	}
	else
	{
		l->insert(it,influence);
	}
}

void Model::removeInfluence(const Position &pos, unsigned index)
{
	m_changeBits |= AddOther;

	infl_list *l = nullptr;
	if(pos.type==PT_Vertex)
	{
		if(pos.index<m_vertices.size())
		{
			l = &m_vertices[pos.index]->m_influences;
		}
	}
	else if(pos.type==PT_Point)
	{
		if(pos.index<m_points.size())
		{
			l = &m_points[pos.index]->m_influences;
		}
	}

	if(l==nullptr)
	{
		return;
	}

	infl_list::iterator it = l->begin();

	while(index>0)
	{
		index--;
		it++;
	}

	if(it!=l->end())
	{
		l->erase(it);
	}
}

void Model::insertPoint(unsigned index, Model::Point *point)
{
	if(index>m_points.size())
	return log_error("insertPoint(%d)index out of range\n",index);

	//if(m_animationMode) return; //REMOVE ME
	invalidateAnim(); point->_source(m_animationMode);

	m_changeBits |= AddOther;

	m_points.insert(m_points.begin()+index,point);

	// Insert point into keyframe list
	for(auto*fa:m_frameAnims)
	{
		//log_debug("inserted keyframe list for point %d\n",j); //???
		fa->m_keyframes.insert(fa->m_keyframes.begin()+index,KeyframeList());
	}
}

void Model::removePoint(unsigned index)
{
	if(index>=m_points.size())
	return log_error("removePoint(%d)index out of range\n",index);

	//log_debug("removePoint(%d)\n",index);

	//if(m_animationMode) return; //REMOVE ME

	m_changeBits |= AddOther;
	
	//DUPLICATES removeJoint
	for(unsigned anim=0;anim<m_frameAnims.size();anim++) 
	{
		auto fa = m_frameAnims[anim];

		// Adjust skeletal animations
		if(index<fa->m_keyframes.size())
		{
			// Delete joint keyframes
			for(auto&kf=fa->m_keyframes[index];!kf.empty();)
			{
				//log_debug("deleting keyframe %d for joint %d\n",index,joint); //???

				deleteKeyframe(anim,kf.back()->m_frame,{PT_Point,index});
			}
				
			// Remove joint from keyframe list
			{
				//log_debug("removed keyframe list for joint %d\n",index); //???
						
				fa->m_keyframes.erase(fa->m_keyframes.begin()+index);
			}
		}
		else assert(index<fa->m_keyframes.size()); //2020

		// Adjust joint indices of keyframes after this joint
		for(unsigned j=index;j<fa->m_keyframes.size();j++)
		{
			for(size_t i=fa->m_keyframes[j].size();i-->0;)
			{
				fa->m_keyframes[j][i]->m_objectIndex--;
			}
		}
	}

	m_points.erase(m_points.begin()+index);	
}

void Model::insertProjection(unsigned index, Model::TextureProjection *proj)
{
	if(index>m_projections.size())
	return log_error("insertProjection(%d)index out of range\n",index);

	//if(m_animationMode) return; //REMOVE ME

	m_changeBits |= AddOther;

	if(index<m_projections.size())
	{
		m_projections.insert(m_projections.begin()+index,proj);

		adjustProjectionIndices(index,+1);
	}
	else m_projections.push_back(proj);
}

void Model::removeProjection(unsigned proj)
{
	log_debug("removeProjection(%d)\n",proj);

	//if(m_animationMode) return; //REMOVE ME

	m_changeBits |= AddOther;

	if(proj<m_projections.size())
	{
		m_projections.erase(m_projections.begin()+proj);
		adjustProjectionIndices(proj,-1);
	}
	else log_error("removeProjection(%d)index out of range\n",proj);
}

void Model::insertTexture(unsigned index, Model::Material *texture)
{
	if(index>m_materials.size())
	return log_error("insertTexture(%d)index out of range\n",index);

	//if(m_animationMode) return; //REMOVE ME

	m_changeBits |= AddOther;

	m_materials.insert(m_materials.begin()+index,texture);

	invalidateTextures(); //OVERKILL
}

void Model::removeTexture(unsigned index)
{
	//if(m_animationMode) return; //REMOVE ME

	m_changeBits |= AddOther;

	if(index<m_materials.size())
	{
		m_materials.erase(m_materials.begin()+index);
	}
	else log_error("removeTexture(%d)index out of range\n",index);
}

void Model::adjustVertexIndices(unsigned index, int amount)
{
	for(auto t=m_triangles.size();t-->0;)	
	for(auto&i:m_triangles[t]->m_vertexIndices)
	{
		if(i>=index) i+=amount;
	}
}

void Model::adjustTriangleIndices(unsigned index, int amount)
{
	for(unsigned g = 0; g<m_groups.size(); g++)
	{
		//Group *grp = m_groups[g];
		auto &grp = m_groups[g]->m_triangleIndices;
		
		/*REMOVE ME
		//https://github.com/zturtleman/mm3d/issues/92
		std::unordered_set<int> newSet; //!!!

		for(auto i:grp)
		{
			if((unsigned)i>=index)
			newSet.insert(i+amount);
			else
			newSet.insert(i);
		}
		grp.swap(newSet);
		*/		
		for(size_t i=grp.size();i-->0;)
		{
			if(grp[i]>=index) grp[i]+=amount; else break;
		}
	}
}

void Model::adjustProjectionIndices(unsigned index, int amount)
{
	for(unsigned t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_projection>=(int)index)
		{
			m_triangles[t]->m_projection += amount;
		}
	}
}

#endif // MM3D_EDIT

