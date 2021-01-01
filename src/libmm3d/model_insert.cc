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
	invalidateAnim(); vertex->_source(m_animationMode); //OVERKILL

	m_changeBits |= AddGeometry;

	if(vertex->m_selected)
	{
		m_changeBits |= SelectionVertices; 
	}

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
	m_changeBits |= AddGeometry;

	invalidateNormals(); //OVERKILL

	if(index<m_vertices.size())
	{
		if(m_vertices[index]->m_selected)
		{
			m_changeBits |= SelectionVertices; 
		}

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

	invalidateAnim(); triangle->_source(m_animationMode); //OVERKILL

	m_changeBits |= AddGeometry;

	if(triangle->m_selected)
	{
		m_changeBits |= SelectionFaces; 
	}

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
	if(index<m_triangles.size())
	{		
		m_changeBits |= AddGeometry;

		if(m_triangles[index]->m_selected)
		{
			m_changeBits |= SelectionFaces; 
		}

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

void Model::insertGroup(unsigned index, Model::Group *group)
{
	if(index>m_groups.size())
	return log_error("insertGroup(%d)index out of range\n",index); 

	m_changeBits |= AddOther;

	if(group->m_selected)
	{
		m_changeBits |= SelectionGroups; 
	}

	m_groups.insert(m_groups.begin()+index,group);
}

void Model::removeGroup(unsigned index)
{
	m_changeBits |= AddOther;

	if(index<m_groups.size())
	{
		if(m_groups[index]->m_selected)
		{
			m_changeBits |= SelectionGroups; 
		}

		m_groups.erase(m_groups.begin()+index);
	}
	else log_error("removeGroup(%d)index out of range\n",index);
}

void Model::insertBoneJoint(unsigned index, Joint *joint)
{
	if(index>m_joints.size())
	return log_error("insertBoneJoint(%d)index out of range\n",index);

	invalidateAnim(); joint->_source(m_animationMode);
	
	m_changeBits |= AddOther;

	if(joint->m_selected)
	{
		m_changeBits |= SelectionJoints; 
	}
		
	if(index<m_joints.size())
	{
		// Adjust parent relationships
		for(unsigned j=0;j<m_joints.size();j++)
		{
			if(m_joints[j]->m_parent>=(signed)index)
			{
				m_joints[j]->m_parent++;
			}
			if(m_joints2[j].first>=index)
			{
				m_joints2[j].first++;
			}
		}

		// Adjust joint indices of keyframes after this joint
		for(auto*sa:m_anims) if(1&sa->_type)
		{
			for(auto&ea:sa->m_keyframes)			
			if(ea.first.type==PT_Joint)
			if(ea.first.index<=index)
			for(auto*kf:ea.second)
			{
				kf->m_objectIndex++;
			}
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

		if(joint->m_parent<0)
		{
			m_joints2.insert(m_joints2.begin(),{index,joint});
		}
		else for(auto it=m_joints2.begin();it<m_joints2.end();it++)		
		{
			if(it->first==joint->m_parent)
			{
				m_joints2.insert(it+1,{index,joint}); break;
			}
		}
	}
	else
	{
		m_joints.push_back(joint);
		m_joints2.push_back({index,joint});
	}

	invalidateSkel(); //m_validJoints = false;
}

void Model::removeBoneJoint(unsigned index)
{
	if(index>=m_joints.size())
	return log_error("removeBoneJoint(%d)index out of range\n",index);

	//log_debug("removeBoneJoint(%d)\n",index); //???

	m_changeBits |= AddOther;

	//DUPLICATES removePoint
	int anim = -1;
	for(auto*sa:m_anims) if(anim++,1&sa->_type) //ANIMMODE_SKELETAL
	{
		Position pos = {PT_Joint,index};

		auto it = sa->m_keyframes.find(pos);

		// Adjust skeletal animations
		if(it!=sa->m_keyframes.end())
		{
			// Delete joint keyframes
			while(!it->second.empty())
			{
				//log_debug("deleting keyframe %d for joint %d\n",index,joint); //???

				//Could use removeKeyframe/MU_DeleteObjectKeyframe here
				//but probably better to improve the undo system itself.
				deleteKeyframe(anim,it->second.back()->m_frame,pos);
			}
				
			// Remove joint from keyframe list
			{
				//log_debug("removed keyframe list for joint %d\n",index); //???
				
				//NOTE: This is no longer strictly required.
				sa->m_keyframes.erase(pos);
			}
		}

		// Adjust joint indices of keyframes after this joint
		{
			for(auto&ea:sa->m_keyframes)			
			if(ea.first.type==PT_Joint)
			if(ea.first.index<=index)
			for(auto*kf:ea.second)
			{
				kf->m_objectIndex--;
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
	
	//2020: let BS_RIGHT add a new root?
	if(index==0&&1!=m_joints.size())
	{
		for(int i=3;i-->0;) 
		m_joints[1]->m_rel[i]+=m_joints[0]->m_rel[i];
	}

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
		if(m_joints2[j].first>index)
		{
			m_joints2[j].first--;
		}
	}

	auto *cmp = m_joints[index];

	if(cmp->m_selected)
	{
		m_changeBits |= SelectionJoints; 
	}

	m_joints.erase(m_joints.begin()+index);

	for(auto it=m_joints2.begin();it<m_joints2.end();it++)		
	{
		if(cmp==it->second)
		{
			m_joints2.erase(it); break;
		}
	}

	invalidateSkel();
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
	if(l==nullptr) return;
			
	assert(index<=l->size());

	if(index>l->size()) return;

	l->insert(l->begin()+index,influence);

	calculateRemainderWeight(pos); //2020
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
	if(l==nullptr) return;
	
	assert(index<l->size());

	if(index>=l->size()) return;

	auto it = l->begin()+index;

	//unsigned bi = it->m_boneId;

	l->erase(it);

	calculateRemainderWeight(pos); //2020

	/*EXPERIMENTAL
	if(bi<m_joints.size()) //2020
	{
		m_joints[bi]->_infl--;

		assert(m_joints[bi]->_infl>=0);
	}
	else assert(0);*/
}

void Model::insertPoint(unsigned index, Model::Point *point)
{
	if(index>m_points.size())
	return log_error("insertPoint(%d)index out of range\n",index);

	invalidateAnim(); point->_source(m_animationMode);

	m_changeBits |= AddOther;

	if(point->m_selected)
	{
		m_changeBits |= SelectionPoints; 
	}

	m_points.insert(m_points.begin()+index,point);
}

void Model::removePoint(unsigned index)
{
	if(index>=m_points.size())
	return log_error("removePoint(%d)index out of range\n",index);

	//log_debug("removePoint(%d)\n",index);

	m_changeBits |= AddOther;
	
	//DUPLICATES removeBoneJoint
	int anim = -1;
	for(auto*fa:m_anims) if(anim++,2&fa->_type) //ANIMMODE_FRAME
	{
		Position pos = {PT_Point,index};

		auto it = fa->m_keyframes.find(pos);

		// Adjust skeletal animations
		if(it!=fa->m_keyframes.end())
		{
			// Delete joint keyframes
			while(!it->second.empty())
			{
				//log_debug("deleting keyframe %d for joint %d\n",index,joint); //???

				//Could use removeKeyframe/MU_DeleteObjectKeyframe here
				//but probably better to improve the undo system itself.
				deleteKeyframe(anim,it->second.back()->m_frame,pos);
			}
				
			// Remove joint from keyframe list
			{
				//log_debug("removed keyframe list for joint %d\n",index); //???
						
				//NOTE: This is no longer strictly required.
				fa->m_keyframes.erase(pos);
			}
		}

		// Adjust joint indices of keyframes after this joint
		{
			for(auto&ea:fa->m_keyframes)			
			if(ea.first.type==PT_Point)
			if(ea.first.index<=index)
			for(auto*kf:ea.second)
			{
				kf->m_objectIndex--;
			}
		}
	}

	if(m_points[index]->m_selected)
	{
		m_changeBits |= SelectionPoints; 
	}

	m_points.erase(m_points.begin()+index);	
}

void Model::insertProjection(unsigned index, Model::TextureProjection *proj)
{
	if(index>m_projections.size())
	return log_error("insertProjection(%d)index out of range\n",index);

	m_changeBits |= AddOther;

	if(proj->m_selected)
	{
		m_changeBits |= SelectionProjections; 
	}

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

	m_changeBits |= AddOther;

	if(proj<m_projections.size())
	{
		if(m_projections[proj]->m_selected)
		{
			m_changeBits |= SelectionProjections; 
		}

		m_projections.erase(m_projections.begin()+proj);
		adjustProjectionIndices(proj,-1);
	}
	else log_error("removeProjection(%d)index out of range\n",proj);
}

void Model::insertTexture(unsigned index, Model::Material *texture)
{
	if(index>m_materials.size())
	return log_error("insertTexture(%d)index out of range\n",index);

	m_changeBits |= AddOther;

	m_materials.insert(m_materials.begin()+index,texture);

	invalidateTextures(); //OVERKILL
}

void Model::removeTexture(unsigned index)
{
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

