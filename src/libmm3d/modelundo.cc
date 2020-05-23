/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2004-2007 Kevin Worcester
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

#include "modelundo.h"
#include "model.h"

#include "log.h"

int ModelUndo::s_allocated = 0;

bool MU_TranslateSelected::combine(Undo *u)
{
	MU_TranslateSelected *undo = dynamic_cast<MU_TranslateSelected*>(u);

	if(undo)
	{
		//m_matrix = m_matrix *undo->m_matrix;
		for(int i=3;i-->0;) m_vec[i]+=undo->m_vec[i];
		return true;
	}
	return false;
}

void MU_TranslateSelected::undo(Model *model)
{
	log_debug("undo translate selected\n");

	//Matrix m = m_matrix.getInverse();

	/*
	// Invert our translation matrix
	for(int c = 0; c<3; c++)
	{
		m.set(3,c,-m_matrix.get(3,c));
	}
	*/

	//FIX ME: Translate via vector.
	//model->translateSelected(m);
	double v[3] = {-m_vec[0],-m_vec[1],-m_vec[2]};
	model->translateSelected(v);
}

void MU_TranslateSelected::redo(Model *model)
{
	//FIX ME: Translate via vector.
	//model->translateSelected(m_matrix);
	model->translateSelected(m_vec);
}

bool MU_RotateSelected::combine(Undo *u)
{
	MU_RotateSelected *undo = dynamic_cast<MU_RotateSelected*>(u);

	if(undo)
	{
		for(int t = 0; t<3; t++)
		{
			if(m_point[t]!=undo->m_point[t])
			{
				return false;
			}
		}

		m_matrix = m_matrix *undo->m_matrix;
		return true;
	}
	else
	{
		return false;
	}
}

void MU_RotateSelected::setMatrixPoint(const Matrix &rhs, double *point)
{
	m_matrix = rhs;
	for(int t = 0; t<3; t++)
	{
		m_point[t] = point[t];
	}
}

void MU_RotateSelected::undo(Model *model)
{
	log_debug("undo rotate selected\n");
	Matrix inv = m_matrix.getInverse();
	model->rotateSelected(inv,m_point);
}

void MU_RotateSelected::redo(Model *model)
{
	log_debug("undo rotate selected\n");
	model->rotateSelected(m_matrix,m_point);
}

bool MU_ApplyMatrix::combine(Undo *u)
{
	MU_ApplyMatrix *undo = dynamic_cast<MU_ApplyMatrix*>(u);

	if(undo)
	{
		m_matrix = m_matrix *undo->m_matrix;
		return true;
	}
	else
	{
		return false;
	}
}

void MU_ApplyMatrix::setMatrix(const Matrix &m,Model::OperationScopeE scope,bool animations)
{
	m_matrix = m;
	m_scope = scope;
	m_animations = animations;
}

void MU_ApplyMatrix::undo(Model *model)
{
	log_debug("undo apply matrix\n");
	Matrix m;

	m = m_matrix.getInverse();

	model->applyMatrix(m,m_scope,m_animations,true);
}

void MU_ApplyMatrix::redo(Model *model)
{
	model->applyMatrix(m_matrix,m_scope,m_animations,true);
}

void MU_SelectionMode::undo(Model *model)
{
	log_debug("undo selection mode %d\n",m_oldMode);

	model->setSelectionMode(m_oldMode);
}

void MU_SelectionMode::redo(Model *model)
{
	log_debug("redo selection mode %d\n",m_mode);

	model->setSelectionMode(m_mode);
}

bool MU_SelectionMode::combine(Undo *u)
{
	MU_SelectionMode *undo = dynamic_cast<MU_SelectionMode*>(u);
	if(undo)
	{
		m_mode = undo->m_mode;
		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SelectionMode::size()
{
	return sizeof(MU_SelectionMode);
}

void MU_Select::undo(Model *model)
{
	SelectionDifferenceList::iterator it;

	bool unselect = false; //2019

	// Invert selection from our list
	for(it = m_diff.begin(); it!=m_diff.end(); it++)
	{
		if(!it->oldSelected) switch (m_mode)
		{
		case Model::SelectVertices:
			model->unselectVertex(it->number);
			break;
		case Model::SelectTriangles:
			unselect = true;
			model->unselectTriangle(it->number);
			break;
		case Model::SelectGroups:
			model->unselectGroup(it->number);
			break;
		case Model::SelectJoints:
			model->unselectBoneJoint(it->number);
			break;
		case Model::SelectPoints:
			model->unselectPoint(it->number);
			break;
		case Model::SelectProjections:
			model->unselectProjection(it->number);
			break;
		case Model::SelectNone: break; //default?
		}		
		else switch (m_mode)
		{
		case Model::SelectVertices:
			model->selectVertex(it->number);
			break;
		case Model::SelectTriangles:
			model->selectTriangle(it->number);
			break;
		case Model::SelectGroups:
			model->selectGroup(it->number);
			break;
		case Model::SelectJoints:
			model->selectBoneJoint(it->number);
			break;
		case Model::SelectPoints:
			model->selectPoint(it->number);
			break;
		case Model::SelectProjections:
			model->selectProjection(it->number);
			break;
		case Model::SelectNone: break; //default?
		}
	}

	//2019: unselectTriangle did this per triangle.
	if(unselect) model->selectVerticesFromTriangles();
}

void MU_Select::redo(Model *model)
{
	SelectionDifferenceList::iterator it;

	bool unselect = false; //2019

	// Set selection from our list
	if(m_diff.size())
	{
		for(it = m_diff.begin(); it!=m_diff.end(); it++)
		{
			if(it->selected) switch (m_mode)
			{
			case Model::SelectVertices:
				model->selectVertex(it->number);
				break;
			case Model::SelectTriangles:
				model->selectTriangle(it->number);
				break;
			case Model::SelectGroups:
				model->selectGroup(it->number);
				break;
			case Model::SelectJoints:
				model->selectBoneJoint(it->number);
				break;
			case Model::SelectPoints:
				model->selectPoint(it->number);
				break;
			case Model::SelectProjections:
				model->selectProjection(it->number);
				break;
			case Model::SelectNone: break; //default?
			}
			else switch (m_mode)
			{
			case Model::SelectVertices:
				model->unselectVertex(it->number);
				break;
			case Model::SelectTriangles:
				unselect = true;
				model->unselectTriangle(it->number);
				break;
			case Model::SelectGroups:
				model->unselectGroup(it->number);
				break;
			case Model::SelectJoints:
				model->unselectBoneJoint(it->number);
				break;
			case Model::SelectPoints:
				model->unselectPoint(it->number);
				break;
			case Model::SelectProjections:
				model->unselectProjection(it->number);
				break;
			case Model::SelectNone: break; //default?
			}
		}
	}

	//2019: unselectTriangle did this per triangle.
	if(unselect) model->selectVerticesFromTriangles();
}

bool MU_Select::combine(Undo *u)
{
	//WHY WAS THIS DIABLED?
	//Restoring this. The selectVerticesFromTriangles
	//fix above can't work with uncombined lists.
	//https://github.com/zturtleman/mm3d/issues/93
	///*
	MU_Select *undo = dynamic_cast<MU_Select*>(u);
	if(undo&&getSelectionMode()==undo->getSelectionMode())
	{
		//2019: m_diff here was "sorted_list" but I'm removing that
		//since it will affect performance bad / can't think of any
		//reason to sort.

		SelectionDifferenceList::iterator it;
		for(it = undo->m_diff.begin(); it!=undo->m_diff.end(); it++)
		{
			setSelectionDifference(it->number,it->selected,it->oldSelected);
		}
		return true;
	}
	else
	//*/
	{
		return false;
	}
}

unsigned MU_Select::size()
{
	return sizeof(MU_Select)+m_diff.size()*sizeof(SelectionDifferenceT);
}

void MU_Select::setSelectionDifference(int number, bool selected, bool oldSelected)
{
	//Disabling so "combine" can be restored.
	//https://github.com/zturtleman/mm3d/issues/93
	/*WHY WAS THIS?
	unsigned index;
	SelectionDifferenceT diff;
	diff.number	= number;
	// Change selection state if it exists in our list
	if(m_diff.find_sorted(diff,index))
	{
		m_diff[index].selected = selected;
	}

	// add selection state to our list
	diff.selected = selected;
	diff.oldSelected = oldSelected;

	m_diff.insert_sorted(diff);
	*/
	m_diff.push_back({number,selected,oldSelected});
}

void MU_SetSelectedUv::undo(Model *model)
{
	model->setSelectedUv(m_oldUv);
}

void MU_SetSelectedUv::redo(Model *model)
{
	model->setSelectedUv(m_newUv);
}

bool MU_SetSelectedUv::combine(Undo *u)
{
	return false;
}

unsigned MU_SetSelectedUv::size()
{
	return sizeof(MU_SetSelectedUv)+m_oldUv.size()*sizeof(int)+m_newUv.size()*sizeof(int)+sizeof(m_oldUv)+sizeof(m_newUv);
}

void MU_SetSelectedUv::setSelectedUv(const std::vector<int> &newUv, const std::vector<int> &oldUv)
{
	m_newUv = newUv;
	m_oldUv = oldUv;
}

void MU_Hide::undo(Model *model)
{
	log_debug("undo hide\n");

	HideDifferenceList::iterator it;

	// Invert visible from our list
	for(it = m_diff.begin(); it!=m_diff.end(); it++)
	{
		if(it->visible)
		{
			switch (m_mode)
			{
				case Model::SelectVertices:
					model->hideVertex(it->number);
					break;
				case Model::SelectTriangles:
					model->hideTriangle(it->number);
					break;
				case Model::SelectJoints:
					model->hideJoint(it->number);
					break;
				case Model::SelectPoints:
					model->hidePoint(it->number);
					break;
				case Model::SelectNone:
				default:
					break;
			}
		}
		else
		{
			switch (m_mode)
			{
				case Model::SelectVertices:
					model->unhideVertex(it->number);
					break;
				case Model::SelectTriangles:
					model->unhideTriangle(it->number);
					break;
				case Model::SelectJoints:
					model->unhideJoint(it->number);
					break;
				case Model::SelectPoints:
					model->unhidePoint(it->number);
					break;
				case Model::SelectNone:
				default:
					break;
			}
		}
	}
}

void MU_Hide::redo(Model *model)
{
	HideDifferenceList::iterator it;

	// Set visible from our list
	for(it = m_diff.begin(); it!=m_diff.end(); it++)
	{
		if(it->visible)
		{
			switch (m_mode)
			{
				case Model::SelectVertices:
					model->unhideVertex(it->number);
					break;
				case Model::SelectTriangles:
					model->unhideTriangle(it->number);
					break;
				case Model::SelectJoints:
					model->unhideJoint(it->number);
					break;
				case Model::SelectPoints:
					model->unhidePoint(it->number);
					break;
				case Model::SelectNone:
				default:
					break;
			}
		}
		else
		{
			switch (m_mode)
			{
				case Model::SelectVertices:
					model->hideVertex(it->number);
					break;
				case Model::SelectTriangles:
					model->hideTriangle(it->number);
					break;
				case Model::SelectJoints:
					model->hideJoint(it->number);
					break;
				case Model::SelectPoints:
					model->hidePoint(it->number);
					break;
				case Model::SelectNone:
				default:
					break;
			}
		}
	}
}

bool MU_Hide::combine(Undo *u)
{
	MU_Hide *undo = dynamic_cast<MU_Hide*>(u);

	if(undo&&getSelectionMode()==undo->getSelectionMode())
	{
		HideDifferenceList::iterator it;
		for(it = undo->m_diff.begin(); it!=undo->m_diff.end(); it++)
		{
			setHideDifference(it->number,it->visible);
		}
		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_Hide::size()
{
	return sizeof(MU_Hide)+m_diff.size()*sizeof(HideDifferenceT);
}

void MU_Hide::setHideDifference(int number,bool visible)
{
	HideDifferenceList::iterator it;

	// Change selection state if it exists in our list
	for(it = m_diff.begin(); it!=m_diff.end(); it++)
	{
		if(it->number==number)
		{
			it->visible = visible;
			return;
		}
	}

	// add selection state to our list
	HideDifferenceT diff;
	diff.number  = number;
	diff.visible = visible;

	m_diff.push_back(diff);
}

void MU_InvertNormal::undo(Model *model)
{
	log_debug("undo invert normal\n");
	int_list::iterator it;
	for(it = m_triangles.begin(); it!=m_triangles.end(); it++)
	{
		model->invertNormals(*it);
	}
}

void MU_InvertNormal::redo(Model *model)
{
	int_list::reverse_iterator it;
	for(it = m_triangles.rbegin(); it!=m_triangles.rend(); it++)
	{
		model->invertNormals(*it);
	}
}

bool MU_InvertNormal::combine(Undo *u)
{
	MU_InvertNormal *undo = dynamic_cast<MU_InvertNormal*>(u);

	if(undo)
	{
		int_list::iterator it;
		for(it = undo->m_triangles.begin(); it!=undo->m_triangles.end(); it++)
		{
			addTriangle(*it);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_InvertNormal::size()
{
	return sizeof(MU_InvertNormal)+m_triangles.size()*sizeof(int);
}

void MU_InvertNormal::addTriangle(int triangle)
{
	int_list::iterator it;

	for(it = m_triangles.begin(); it!=m_triangles.end(); it++)
	{
		if(triangle==*it)
		{
			/*2020: WRONG RIGHT?
			//m_triangles.remove(triangle);
			m_triangles.erase(it);
			*/
			return;
		}
	}

	m_triangles.push_back(triangle);
}

void MU_MovePrimitive::undo(Model *model)
{
	log_debug("undo move vertex\n");

	MovePrimitiveList::iterator it;

	// Modify a vertex we already have
	for(it = m_objects.begin(); it!=m_objects.end(); it++)
	{
		switch(it->type)
		{
		case MT_Vertex:
			model->moveVertex(it->number,it->oldx,it->oldy,it->oldz);
			break;
		case MT_Joint:
			model->moveBoneJoint(it->number,it->oldx,it->oldy,it->oldz);
			break;
		case MT_Point:
			model->movePoint(it->number,it->oldx,it->oldy,it->oldz);
			break;
		case MT_Projection:
			model->moveProjection(it->number,it->oldx,it->oldy,it->oldz);
			break;
		default:
			log_error("Unknown type in move object undo\n");
			break;
		}
	}
}

void MU_MovePrimitive::redo(Model *model)
{
	MovePrimitiveList::iterator it;

	// Modify a vertex we already have
	for(it = m_objects.begin(); it!=m_objects.end(); it++)
	{
		switch(it->type)
		{
		case MT_Vertex:
			model->moveVertex(it->number,it->x,it->y,it->z);
			break;
		case MT_Joint:
			model->moveBoneJoint(it->number,it->x,it->y,it->z);
			break;
		case MT_Point:
			model->movePoint(it->number,it->x,it->y,it->z);
			break;
		case MT_Projection:
			model->moveProjection(it->number,it->x,it->y,it->z);
			break;
		default:
			log_error("Unknown type in move object redo\n");
			break;
		}
	}
}

bool MU_MovePrimitive::combine(Undo *u)
{
	MU_MovePrimitive *undo = dynamic_cast<MU_MovePrimitive*>(u);

	if(undo)
	{
		MovePrimitiveList::iterator it;

		for(it = undo->m_objects.begin(); it!=undo->m_objects.end(); it++)
		{
			addMovePrimitive(it->type,it->number,it->x,it->y,it->z,
					it->oldx,it->oldy,it->oldz);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_MovePrimitive::size()
{
	return sizeof(MU_MovePrimitive)+m_objects.size()*sizeof(MovePrimitiveT);
}

void MU_MovePrimitive::addMovePrimitive(MU_MovePrimitive::MoveTypeE type, int i, double x, double y, double z,
		double oldx, double oldy, double oldz)
{
	unsigned index;
	MovePrimitiveT mv;
	mv.number = i;
	mv.type = type;

	// Modify an object we already have
	if(m_objects.find_sorted(mv,index))
	{
		m_objects[index].x = x;
		m_objects[index].y = y;
		m_objects[index].z = z;
		return;
	}

	// Not found,add object information
	mv.x		 = x;
	mv.y		 = y;
	mv.z		 = z;
	mv.oldx	 = oldx;
	mv.oldy	 = oldy;
	mv.oldz	 = oldz;
	m_objects.insert_sorted(mv);
}

void MU_SetObjectXYZ::undo(Model *model)
{
	//log_debug("undo point translation\n"); //???

	_call_setter(model,vold);
}
void MU_SetObjectXYZ::redo(Model *model)
{
	//log_debug("redo point translation\n"); //???

	_call_setter(model,v);
}
void MU_SetObjectXYZ::_call_setter(Model *model, double *xyz)
{
	switch(vec)
	{
	case Pos: model->setPositionCoords(pos,xyz); break;
	case Rot: model->setPositionRotation(pos,xyz); break;
	case Scale: model->setPositionScale(pos,xyz); break;
	}
}
bool MU_SetObjectXYZ::combine(Undo *u)
{
	MU_SetObjectXYZ *undo = dynamic_cast<MU_SetObjectXYZ*>(u);

	if(undo&&undo->pos==pos&&undo->vec==vec)
	{
		memcpy(v,undo->v,sizeof(v));

		return true;
	}
	else
	{
		//log_debug("couldn't point translation\n"); //???
		return false;
	}
}
unsigned MU_SetObjectXYZ::size()
{
	return sizeof(MU_SetObjectXYZ);
}
void MU_SetObjectXYZ::setXYZ(Model *m, double object[3], const double xyz[3])
{
	auto o = m->getPositionObject(pos);
	assert(o);
	if(o->m_abs==object) vec = Pos;
	else if(o->m_rot==object) vec = Rot;
	else if(o->m_xyz==object) vec = Scale;	
	else assert(0);

	memcpy(v,xyz,sizeof(v)); memcpy(vold,object,sizeof(v));	

}

void MU_SetTexture::undo(Model *model)
{
	log_debug("undo set texture\n");
	SetTextureList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setGroupTextureId(it->groupNumber,it->oldTexture);
	}
}

void MU_SetTexture::redo(Model *model)
{
	SetTextureList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setGroupTextureId(it->groupNumber,it->newTexture);
	}
}

bool MU_SetTexture::combine(Undo *u)
{
	MU_SetTexture *undo = dynamic_cast<MU_SetTexture*>(u);

	if(undo)
	{
		SetTextureList::iterator it;

		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			setTexture(it->groupNumber,it->newTexture,it->oldTexture);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetTexture::size()
{
	return sizeof(MU_SetTexture)+m_list.size()*sizeof(SetTextureT);
}

void MU_SetTexture::setTexture(unsigned groupNumber, int newTexture, int oldTexture)
{
	SetTextureList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->groupNumber==groupNumber)
		{
			it->newTexture = newTexture;
			return;
		}
	}

	SetTextureT st;
	st.groupNumber = groupNumber;
	st.newTexture  = newTexture;
	st.oldTexture  = oldTexture;
	m_list.push_back(st);
}

void MU_AddVertex::undo(Model *model)
{
	log_debug("undo add vertex\n");

	AddVertexList::reverse_iterator it;
	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->removeVertex(it->index);
	}
}

void MU_AddVertex::redo(Model *model)
{
	AddVertexList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->insertVertex(it->index,it->vertex);
	}
}

bool MU_AddVertex::combine(Undo *u)
{
	MU_AddVertex *undo = dynamic_cast<MU_AddVertex*>(u);

	if(undo)
	{
		AddVertexList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			addVertex(it->index,it->vertex);
		}
		return true;
	}
	else
	{
		return false;
	}
}

void MU_AddVertex::redoRelease()
{
	AddVertexList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		it->vertex->release();
	}
}

unsigned MU_AddVertex::size()
{
	return sizeof(MU_AddVertex)+m_list.size()*(sizeof(AddVertexT)+sizeof(Model::Vertex));
}

void MU_AddVertex::addVertex(unsigned index,Model::Vertex *vertex)
{
	if(vertex)
	{
		AddVertexT v;
		v.index  = index;
		v.vertex = vertex;
		m_list.push_back(v);
	}
}

void MU_AddTriangle::undo(Model *model)
{
	log_debug("undo add triangle\n");

	AddTriangleList::reverse_iterator it;
	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->removeTriangle(it->index);
	}
}

void MU_AddTriangle::redo(Model *model)
{
	AddTriangleList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->insertTriangle(it->index,it->triangle);
	}
}

bool MU_AddTriangle::combine(Undo *u)
{
	MU_AddTriangle *undo = dynamic_cast<MU_AddTriangle*>(u);

	if(undo)
	{
		AddTriangleList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			addTriangle(it->index,it->triangle);
		}
		return true;
	}
	else
	{
		return false;
	}
}

void MU_AddTriangle::redoRelease()
{
	AddTriangleList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		it->triangle->release();
	}
}

unsigned MU_AddTriangle::size()
{
	return sizeof(MU_AddTriangle)+m_list.size()*(sizeof(AddTriangleT)+sizeof(Model::Triangle));
}

void MU_AddTriangle::addTriangle(unsigned index,Model::Triangle *triangle)
{
	if(triangle)
	{
		AddTriangleT v;
		v.index  = index;
		v.triangle = triangle;
		m_list.push_back(v);
	}
}

void MU_AddGroup::undo(Model *model)
{
	log_debug("undo add group\n");
	AddGroupList::reverse_iterator it;
	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->removeGroup(it->index);
	}
}

void MU_AddGroup::redo(Model *model)
{
	AddGroupList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->insertGroup(it->index,it->group);
	}
}

bool MU_AddGroup::combine(Undo *u)
{
	MU_AddGroup *undo = dynamic_cast<MU_AddGroup*>(u);

	if(undo)
	{
		AddGroupList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			addGroup(it->index,it->group);
		}
		return true;
	}
	else
	{
		return false;
	}
}

void MU_AddGroup::redoRelease()
{
	AddGroupList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		it->group->release();
	}
}

unsigned MU_AddGroup::size()
{
	return sizeof(MU_AddGroup)+m_list.size()*(sizeof(AddGroupT)+sizeof(Model::Group));
}

void MU_AddGroup::addGroup(unsigned index,Model::Group *group)
{
	if(group)
	{
		AddGroupT v;
		v.index  = index;
		v.group  = group;
		m_list.push_back(v);
	}
}

void MU_AddTexture::undo(Model *model)
{
	log_debug("undo add texture\n");
	AddTextureList::reverse_iterator it;
	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->removeTexture(it->index);
	}
}

void MU_AddTexture::redo(Model *model)
{
	AddTextureList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->insertTexture(it->index,it->texture);
	}
}

bool MU_AddTexture::combine(Undo *u)
{
	MU_AddTexture *undo = dynamic_cast<MU_AddTexture*>(u);

	if(undo)
	{
		AddTextureList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			addTexture(it->index,it->texture);
		}
		return true;
	}
	else
	{
		return false;
	}
}

void MU_AddTexture::redoRelease()
{
	AddTextureList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		it->texture->release();
	}
}

unsigned MU_AddTexture::size()
{
	return sizeof(MU_AddTexture)+m_list.size()*(sizeof(AddTextureT)+sizeof(Model::Material));
}

void MU_AddTexture::addTexture(unsigned index,Model::Material *texture)
{
	if(texture)
	{
		AddTextureT v;
		v.index  = index;
		v.texture  = texture;
		m_list.push_back(v);
	}
}

void MU_SetTextureCoords::undo(Model *model)
{
	STCList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setTextureCoords(it->triangle,it->vertexIndex,it->oldS,it->oldT);
	}
}

void MU_SetTextureCoords::redo(Model *model)
{
	STCList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setTextureCoords(it->triangle,it->vertexIndex,it->s,it->t);
	}
}

bool MU_SetTextureCoords::combine(Undo *u)
{
	MU_SetTextureCoords *undo = dynamic_cast<MU_SetTextureCoords*>(u);

	if(undo)
	{
		STCList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			addTextureCoords(it->triangle,it->vertexIndex,
					it->s,it->t,it->oldS,it->oldT);
		}
		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetTextureCoords::size()
{
	return sizeof(MU_SetTextureCoords)+m_list.size()*sizeof(SetTextureCoordsT);
}

void MU_SetTextureCoords::addTextureCoords(unsigned triangle, unsigned vertexIndex, float s, float t, float oldS, float oldT)
{
	unsigned index;
	SetTextureCoordsT stc;
	stc.triangle	 = triangle;
	stc.vertexIndex = vertexIndex;

	if(m_list.find_sorted(stc,index))
	{
		m_list[index].s = s;
		m_list[index].t = t;
		return;
	}

	stc.s			  = s;
	stc.t			  = t;
	stc.oldS		  = oldS;
	stc.oldT		  = oldT;

	m_list.insert_sorted(stc);
}

void MU_AddToGroup::undo(Model *model)
{
	log_debug("undo add to group\n");
	AddToGroupList::reverse_iterator it;
	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->removeTriangleFromGroup(it->groupNum,it->triangleNum);
	}
}

void MU_AddToGroup::redo(Model *model)
{
	AddToGroupList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->addTriangleToGroup(it->groupNum,it->triangleNum);
	}
}

bool MU_AddToGroup::combine(Undo *u)
{
	MU_AddToGroup *undo = dynamic_cast<MU_AddToGroup*>(u);

	if(undo)
	{
		AddToGroupList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			addToGroup(it->groupNum,it->triangleNum);
		}
		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_AddToGroup::size()
{
	return sizeof(MU_AddToGroup)+m_list.size()*sizeof(AddToGroupT);
}

void MU_AddToGroup::addToGroup(unsigned groupNum, unsigned triangleNum)
{
	AddToGroupList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->triangleNum==triangleNum)
		{
			it->groupNum = groupNum;
			return;
		}
	}

	AddToGroupT add;
	add.groupNum	 = groupNum;
	add.triangleNum = triangleNum;
	m_list.push_back(add);
}

void MU_RemoveFromGroup::undo(Model *model)
{
	log_debug("undo remove from group\n");
	RemoveFromGroupList::reverse_iterator it;
	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->addTriangleToGroup(it->groupNum,it->triangleNum);
	}
}

void MU_RemoveFromGroup::redo(Model *model)
{
	RemoveFromGroupList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->removeTriangleFromGroup(it->groupNum,it->triangleNum);
	}
}

bool MU_RemoveFromGroup::combine(Undo *u)
{
	MU_RemoveFromGroup *undo = dynamic_cast<MU_RemoveFromGroup*>(u);

	if(undo)
	{
		RemoveFromGroupList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			removeFromGroup(it->groupNum,it->triangleNum);
		}
		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_RemoveFromGroup::size()
{
	return sizeof(MU_RemoveFromGroup)+m_list.size()*sizeof(RemoveFromGroupT);
}

void MU_RemoveFromGroup::removeFromGroup(unsigned groupNum, unsigned triangleNum)
{
	/*
	RemoveFromGroupList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->triangleNum==triangleNum)
		{
			it->groupNum = groupNum;
			return;
		}
	}
	*/

	RemoveFromGroupT add;
	add.groupNum	 = groupNum;
	add.triangleNum = triangleNum;
	m_list.push_back(add);
}

void MU_DeleteTriangle::undo(Model *model)
{
	log_debug("undo delete triangle\n");
	DeleteTriangleList::reverse_iterator it;

	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->insertTriangle(it->triangleNum,it->triangle);
	}
}

void MU_DeleteTriangle::redo(Model *model)
{
	DeleteTriangleList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->removeTriangle(it->triangleNum);
	}
}

bool MU_DeleteTriangle::combine(Undo *u)
{
	MU_DeleteTriangle *undo = dynamic_cast<MU_DeleteTriangle*>(u);

	if(undo)
	{
		DeleteTriangleList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			deleteTriangle(it->triangleNum,it->triangle);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void MU_DeleteTriangle::undoRelease()
{
	DeleteTriangleList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		it->triangle->release();
	}
}

unsigned MU_DeleteTriangle::size()
{
	return sizeof(MU_DeleteTriangle)+m_list.size()*(sizeof(DeleteTriangleT)+sizeof(Model::Triangle));
}

void MU_DeleteTriangle::deleteTriangle(unsigned triangleNum,Model::Triangle *triangle)
{
	DeleteTriangleT del;

	del.triangleNum = triangleNum;
	del.triangle	 = triangle;

	m_list.push_back(del);
}

void MU_DeleteVertex::undo(Model *model)
{
	log_debug("undo delete vertex\n");
	DeleteVertexList::reverse_iterator it;

	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->insertVertex(it->vertexNum,it->vertex);
	}
}

void MU_DeleteVertex::redo(Model *model)
{
	DeleteVertexList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->removeVertex(it->vertexNum);
	}
}

bool MU_DeleteVertex::combine(Undo *u)
{
	MU_DeleteVertex *undo = dynamic_cast<MU_DeleteVertex*>(u);

	if(undo)
	{
		DeleteVertexList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			deleteVertex(it->vertexNum,it->vertex);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void MU_DeleteVertex::undoRelease()
{
	DeleteVertexList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		it->vertex->release();
	}
}

unsigned MU_DeleteVertex::size()
{
	return sizeof(MU_DeleteVertex)+m_list.size()*(sizeof(DeleteVertexT)+sizeof(Model::Vertex));
}

void MU_DeleteVertex::deleteVertex(unsigned vertexNum,Model::Vertex *vertex)
{
	DeleteVertexT del;

	del.vertexNum = vertexNum;
	del.vertex	 = vertex;

	m_list.push_back(del);
}

void MU_DeleteGroup::undo(Model *model)
{
	log_debug("undo delete group\n");
	DeleteGroupList::reverse_iterator it;

	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->insertGroup(it->groupNum,it->group);
	}
}

void MU_DeleteGroup::redo(Model *model)
{
	DeleteGroupList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->removeGroup(it->groupNum);
	}
}

bool MU_DeleteGroup::combine(Undo *u)
{
	MU_DeleteGroup *undo = dynamic_cast<MU_DeleteGroup*>(u);

	if(undo)
	{
		DeleteGroupList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			deleteGroup(it->groupNum,it->group);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void MU_DeleteGroup::undoRelease()
{
	DeleteGroupList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		it->group->release();
	}
}

unsigned MU_DeleteGroup::size()
{
	return sizeof(MU_DeleteGroup)+m_list.size()*(sizeof(DeleteGroupT)+sizeof(Model::Group));
}

void MU_DeleteGroup::deleteGroup(unsigned groupNum,Model::Group *group)
{
	DeleteGroupT del;

	del.groupNum = groupNum;
	del.group	 = group;

	m_list.push_back(del);
}

void MU_DeleteTexture::undo(Model *model)
{
	log_debug("undo delete texture\n");
	DeleteTextureList::reverse_iterator it;

	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->insertTexture(it->textureNum,it->texture);
	}
}

void MU_DeleteTexture::redo(Model *model)
{
	DeleteTextureList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->removeTexture(it->textureNum);
	}
}

bool MU_DeleteTexture::combine(Undo *u)
{
	MU_DeleteTexture *undo = dynamic_cast<MU_DeleteTexture*>(u);

	if(undo)
	{
		DeleteTextureList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			deleteTexture(it->textureNum,it->texture);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void MU_DeleteTexture::undoRelease()
{
	DeleteTextureList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		it->texture->release();
	}
}

unsigned MU_DeleteTexture::size()
{
	return sizeof(MU_DeleteTexture)+m_list.size()*(sizeof(DeleteTextureT)+sizeof(Model::Material));
}

void MU_DeleteTexture::deleteTexture(unsigned textureNum,Model::Material *texture)
{
	DeleteTextureT del;

	del.textureNum = textureNum;
	del.texture	 = texture;

	m_list.push_back(del);
}

void MU_SetLightProperties::undo(Model *model)
{
	log_debug("undo set light properties\n");
	LightPropertiesList::reverse_iterator it;

	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		if(it->isSet[0])
		{
			model->setTextureAmbient(it->textureNum,it->oldLight[0]);
		}
		if(it->isSet[1])
		{
			model->setTextureDiffuse(it->textureNum,it->oldLight[1]);
		}
		if(it->isSet[2])
		{
			model->setTextureSpecular(it->textureNum,it->oldLight[2]);
		}
		if(it->isSet[3])
		{
			model->setTextureEmissive(it->textureNum,it->oldLight[3]);
		}
	}
}

void MU_SetLightProperties::redo(Model *model)
{
	LightPropertiesList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->isSet[0])
		{
			model->setTextureAmbient(it->textureNum,it->newLight[0]);
		}
		if(it->isSet[1])
		{
			model->setTextureDiffuse(it->textureNum,it->newLight[1]);
		}
		if(it->isSet[2])
		{
			model->setTextureSpecular(it->textureNum,it->newLight[2]);
		}
		if(it->isSet[3])
		{
			model->setTextureEmissive(it->textureNum,it->newLight[3]);
		}
	}
}

bool MU_SetLightProperties::combine(Undo *u)
{
	MU_SetLightProperties *undo = dynamic_cast<MU_SetLightProperties*>(u);

	if(undo)
	{
		LightPropertiesList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			for(int t = 0; t<LightTypeMax; t++)
			{
				if(it->isSet[t])
				{
					setLightProperties(it->textureNum,(LightTypeE)t,it->newLight[t],it->oldLight[t]);
				}
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetLightProperties::size()
{
	return sizeof(MU_SetLightProperties)+m_list.size()*sizeof(LightPropertiesT);
}

void MU_SetLightProperties::setLightProperties(unsigned textureNum,LightTypeE type, const float *newLight, const float *oldLight)
{
	LightPropertiesList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->textureNum==textureNum)
		{
			// Set old light if this is the first time we set this type
			if(!it->isSet[type])
			{
				for(int n = 0; n<4; n++)
				{
					it->oldLight[type][n] = oldLight[n];
				}
			}

			// Set new light for this type
			it->isSet[type] = true;
			for(int n = 0; n<4; n++)
			{
				it->newLight[type][n] = newLight[n];
			}
			return;
		}
	}

	// Add new LightPropertiesT to list
	LightPropertiesT prop;

	for(int t = 0; t<LightTypeMax; t++)
	{
		prop.isSet[t] = false;
	}

	prop.textureNum = textureNum;
	prop.isSet[type] = true;
	for(int n = 0; n<4; n++)
	{
		prop.oldLight[type][n] = oldLight[n];
		prop.newLight[type][n] = newLight[n];
	}

	m_list.push_back(prop);
}

void MU_SetShininess::undo(Model *model)
{
	log_debug("undo set shininess\n");
	ShininessList::reverse_iterator it;

	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->setTextureShininess(it->textureNum,it->oldValue);
	}
}

void MU_SetShininess::redo(Model *model)
{
	ShininessList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setTextureShininess(it->textureNum,it->newValue);
	}
}

bool MU_SetShininess::combine(Undo *u)
{
	MU_SetShininess *undo = dynamic_cast<MU_SetShininess*>(u);

	if(undo)
	{
		ShininessList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			setShininess(it->textureNum,it->newValue,it->oldValue);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetShininess::size()
{
	return sizeof(MU_SetShininess)+m_list.size()*sizeof(ShininessT);
}

void MU_SetShininess::setShininess(unsigned textureNum, const float &newValue, const float &oldValue)
{
	ShininessList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->textureNum==textureNum)
		{
			it->newValue = newValue;
			return;
		}
	}

	// Add new ShininessT to list
	ShininessT shine;

	shine.textureNum = textureNum;
	shine.oldValue = oldValue;
	shine.newValue = newValue;

	m_list.push_back(shine);
}

void MU_SetTextureName::undo(Model *model)
{
	log_debug("undo set texture name\n");
	model->setTextureName(m_textureNum,m_oldName.c_str());
}

void MU_SetTextureName::redo(Model *model)
{
	model->setTextureName(m_textureNum,m_newName.c_str());
}

bool MU_SetTextureName::combine(Undo *u)
{
	MU_SetTextureName *undo = dynamic_cast<MU_SetTextureName*>(u);

	if(undo&&undo->m_textureNum==m_textureNum)
	{
		m_newName = undo->m_newName;
		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetTextureName::size()
{
	return sizeof(MU_SetTextureName);
}

void MU_SetTextureName::setTextureName(unsigned textureNum, const char *newName, const char *oldName)
{
	m_textureNum = textureNum;
	m_newName	 = newName;
	m_oldName	 = oldName;
}

void MU_SetTriangleVertices::undo(Model *model)
{
	log_debug("undo set triangle vertices\n");
	TriangleVerticesList::reverse_iterator it;

	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->setTriangleVertices(it->triangleNum,
				it->oldVertices[0],
				it->oldVertices[1],
				it->oldVertices[2]);
	}
}

void MU_SetTriangleVertices::redo(Model *model)
{
	TriangleVerticesList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setTriangleVertices(it->triangleNum,
				it->newVertices[0],
				it->newVertices[1],
				it->newVertices[2]);
	}
}

bool MU_SetTriangleVertices::combine(Undo *u)
{
	MU_SetTriangleVertices *undo = dynamic_cast<MU_SetTriangleVertices*>(u);

	if(undo)
	{
		TriangleVerticesList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			setTriangleVertices(it->triangleNum,
					it->newVertices[0],
					it->newVertices[1],
					it->newVertices[2],
					it->oldVertices[0],
					it->oldVertices[1],
					it->oldVertices[2]);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetTriangleVertices::size()
{
	return sizeof(MU_SetTriangleVertices)+m_list.size()*sizeof(TriangleVerticesT);
}

void MU_SetTriangleVertices::setTriangleVertices(unsigned triangleNum,
		unsigned newV1, unsigned newV2, unsigned newV3,
		unsigned oldV1, unsigned oldV2, unsigned oldV3)
{
	TriangleVerticesList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->triangleNum==triangleNum)
		{
			it->newVertices[0] = newV1;
			it->newVertices[1] = newV2;
			it->newVertices[2] = newV3;
			return;
		}
	}

	// Add new ShininessT to list
	TriangleVerticesT tv;

	tv.triangleNum = triangleNum;
	tv.oldVertices[0] = oldV1;
	tv.oldVertices[1] = oldV2;
	tv.oldVertices[2] = oldV3;
	tv.newVertices[0] = newV1;
	tv.newVertices[1] = newV2;
	tv.newVertices[2] = newV3;

	m_list.push_back(tv);
}

void MU_SubdivideSelected::undo(Model *model)
{
	log_debug("undo subdivide selected\n");
}

void MU_SubdivideSelected::redo(Model *model)
{
	model->subdivideSelectedTriangles();
}

bool MU_SubdivideSelected::combine(Undo *u)
{
	return false;
}

void MU_SubdivideTriangle::undo(Model *model)
{
	log_debug("undo subdivide\n");

	SubdivideTriangleList::reverse_iterator it;

	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		//model->unsubdivideTriangles(it->a,it->b,it->c,it->d);
		model->subdivideSelectedTriangles_undo(it->a,it->b,it->c,it->d);
	}

	std::vector<unsigned>::reverse_iterator iit;

	for(iit = m_vlist.rbegin(); iit!=m_vlist.rend(); iit++)
	{
		model->deleteVertex(*iit);
	}
}

bool MU_SubdivideTriangle::combine(Undo *u)
{
	if(auto undo=dynamic_cast<MU_SubdivideTriangle*>(u))
	{
		for(auto&ea:undo->m_list)
		subdivide(ea.a,ea.b,ea.c,ea.d);
		undo->m_list.clear();

		for(auto&ea:undo->m_vlist)
		addVertex(ea);
		undo->m_vlist.clear();

		return true;
	}
	else return false;
}

unsigned MU_SubdivideTriangle::size()
{
	return sizeof(MU_SubdivideTriangle)
	  +m_list.size()*sizeof(SubdivideTriangleT)
	  +m_vlist.size()*sizeof(int);
}

void MU_SubdivideTriangle::subdivide(unsigned a, unsigned b, unsigned c, unsigned d)
{
	SubdivideTriangleT st;

	st.a = a;
	st.b = b;
	st.c = c;
	st.d = d;

	m_list.push_back(st);
}

void MU_SubdivideTriangle::addVertex(unsigned v)
{
	m_vlist.push_back(v);
}

void MU_ChangeAnimState::undo(Model *model)
{
	log_debug("undo change anim state: old %d\n",m_oldMode);
	//if(m_oldMode) //???
	bool skel = m_oldMode==Model::ANIMMODE_SKELETAL;
	_sync_animation(model,skel,m_oldAnim,m_oldFrame); //REMOVE US
	//else model->setNoAnimation(); //???
}

void MU_ChangeAnimState::redo(Model *model)
{
	log_debug("redo change anim state: new %d\n",m_newMode);
	//if(m_newMode) //???
	bool skel = m_newMode==Model::ANIMMODE_SKELETAL;
	_sync_animation(model,skel,m_anim,0); //REMOVE US
	//else model->setNoAnimation(); //???
}

bool MU_ChangeAnimState::combine(Undo *u)
{
	return false;
}

unsigned MU_ChangeAnimState::size()
{
	return sizeof(MU_ChangeAnimState);
}

//void MU_ChangeAnimState::setState(Model::AnimationModeE newMode,Model::AnimationModeE oldMode, unsigned anim, unsigned frame)
void MU_ChangeAnimState::setState(Model::AnimationModeE oldMode, int oldAnim, int oldFrame, Model &cp)
{
	//m_newMode = newMode;
	m_newMode = cp.getAnimationMode();
	m_oldMode = oldMode;
	//m_anim = anim;
	m_anim = cp.getCurrentAnimation();
	m_oldAnim = oldAnim;
	//m_frame = frame;
	m_oldFrame = oldFrame;

	log_debug("ChangeAnimState undo info: old = %d,new = %d\n",m_oldMode,m_newMode);
}

void MU_SetAnimName::undo(Model *model)
{
	model->setAnimName(m_mode,m_animNum,m_oldName.c_str());
	if((model->getAnimationMode()!=Model::ANIMMODE_NONE&&model->getAnimationMode()!=m_mode)||(model->getCurrentAnimation()!=m_animNum))
	{
		model->setCurrentAnimation(m_mode,m_oldName.c_str());
	}
}

void MU_SetAnimName::redo(Model *model)
{
	model->setAnimName(m_mode,m_animNum,m_newName.c_str());
	if((model->getAnimationMode()!=Model::ANIMMODE_NONE&&model->getAnimationMode()!=m_mode)||(model->getCurrentAnimation()!=m_animNum))
	{
		model->setCurrentAnimation(m_mode,m_newName.c_str());
	}
}

bool MU_SetAnimName::combine(Undo *u)
{
	return false;
}

unsigned MU_SetAnimName::size()
{
	return sizeof(MU_SetAnimName);
}

void MU_SetAnimName::setName(Model::AnimationModeE mode, unsigned animNum, const char *newName, const char *oldName)
{
	m_mode		 = mode;
	m_animNum	 = animNum;
	m_newName	 = newName;
	m_oldName	 = oldName;
}

void MU_SetAnimFrameCount::undo(Model *model)
{
	model->setAnimFrameCount(m_mode,m_animNum,m_oldCount,m_where,&m_vertices);

	if(!m_timetable.empty())
	{
		Model::AnimBase2020 *ab;
		if(m_mode==Model::ANIMMODE_SKELETAL)
		ab = model->m_skelAnims[m_animNum];
		else ab = model->m_frameAnims[m_animNum];

		std::copy(m_timetable.begin(),m_timetable.end(),ab->m_timetable2020.begin()+m_where);
	}
}
void MU_SetAnimFrameCount::redo(Model *model)
{
	model->setAnimFrameCount(m_mode,m_animNum,m_newCount,m_where,&m_vertices);
}
bool MU_SetAnimFrameCount::combine(Undo *u)
{
	return false;
}
void MU_SetAnimFrameCount::undoRelease()
{	
	if(m_oldCount>m_newCount)
	{
		//log_debug("releasing animation in undo\n");

		for(auto*ea:m_vertices) ea->release();
	}
}
void MU_SetAnimFrameCount::redoRelease()
{	
	if(m_oldCount<m_newCount)
	{
		//log_debug("releasing animation in redo\n");

		for(auto*ea:m_vertices) ea->release();
	}	
}
unsigned MU_SetAnimFrameCount::size()
{
	size_t sz = m_timetable.size()*sizeof(double);
	sz+=m_vertices.size()*sizeof(*m_vertices.data());

	//FIX ME
	//2020: MU_AddAnimation::size was reporting this.
	//sz+=
	//I give up for now:
	//https://github.com/zturtleman/mm3d/issues/128

	return sz+sizeof(MU_SetAnimFrameCount);
}
void MU_SetAnimFrameCount::setAnimFrameCount(Model::AnimationModeE mode, unsigned animNum, unsigned newCount, unsigned oldCount, unsigned where)
{
	m_mode		 = mode;
	m_animNum	 = animNum;
	m_newCount	= newCount;
	m_oldCount	= oldCount;
	m_where      = where;
}

void MU_SetAnimFPS::undo(Model *model)
{
	model->setAnimFPS(m_mode,m_animNum,m_oldFPS);
	if(model->getAnimationMode()!=Model::ANIMMODE_NONE&&(model->getAnimationMode()!=m_mode||model->getCurrentAnimation()!=m_animNum))
	{
		model->setCurrentAnimation(m_mode,m_animNum);
	}
}

void MU_SetAnimFPS::redo(Model *model)
{
	model->setAnimFPS(m_mode,m_animNum,m_newFPS);
	if(model->getAnimationMode()!=Model::ANIMMODE_NONE&&(model->getAnimationMode()!=m_mode||model->getCurrentAnimation()!=m_animNum))
	{
		model->setCurrentAnimation(m_mode,m_animNum);
	}
}

bool MU_SetAnimFPS::combine(Undo *u)
{
	MU_SetAnimFPS *undo = dynamic_cast<MU_SetAnimFPS*>(u);

	if(undo&&undo->m_mode==m_mode&&undo->m_animNum==m_animNum)
	{
		m_newFPS = undo->m_newFPS;
		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetAnimFPS::size()
{
	return sizeof(MU_SetAnimFPS);
}

void MU_SetAnimFPS::setFPS(Model::AnimationModeE mode, unsigned animNum, double newFps, double oldFps)
{
	m_mode		 = mode;
	m_animNum	 = animNum;
	m_newFPS	  = newFps;
	m_oldFPS	  = oldFps;
}

void MU_SetAnimWrap::undo(Model *model)
{
	model->setAnimWrap(m_mode,m_animNum,m_oldLoop);

	//REMOVE ME
	if(model->getAnimationMode()!=Model::ANIMMODE_NONE&&(model->getAnimationMode()!=m_mode||model->getCurrentAnimation()!=m_animNum))
	{
		model->setCurrentAnimation(m_mode,m_animNum); //???
	}
}
void MU_SetAnimWrap::redo(Model *model)
{
	model->setAnimWrap(m_mode,m_animNum,m_newLoop);

	//REMOVE ME
	if(model->getAnimationMode()!=Model::ANIMMODE_NONE&&(model->getAnimationMode()!=m_mode||model->getCurrentAnimation()!=m_animNum))
	{
		model->setCurrentAnimation(m_mode,m_animNum); //???
	}
}
bool MU_SetAnimWrap::combine(Undo *u)
{
	MU_SetAnimWrap *undo = dynamic_cast<MU_SetAnimWrap*>(u);

	if(undo&&undo->m_mode==m_mode&&undo->m_animNum==m_animNum)
	{
		m_newLoop = undo->m_newLoop;
		return true;
	}
	else
	{
		return false;
	}
}
void MU_SetAnimWrap::setAnimWrap(Model::AnimationModeE mode, unsigned animNum,bool newLoop,bool oldLoop)
{
	m_mode		 = mode;
	m_animNum	 = animNum;
	m_newLoop	 = newLoop;
	m_oldLoop	 = oldLoop;
}

void MU_SetAnimTime::undo(Model *model)
{
	if(m_animFrame==INT_MAX)
	model->setAnimTimeFrame(m_mode,m_animNum,m_oldTime);
	else
	model->setAnimFrameTime(m_mode,m_animNum,m_animFrame,m_oldTime);

	/*REMOVE ME
	if(model->getAnimationMode()!=Model::ANIMMODE_NONE&&(model->getAnimationMode()!=m_mode||model->getCurrentAnimation()!=m_animNum))
	{
		model->setCurrentAnimation(m_mode,m_animNum); //???
	}*/
}
void MU_SetAnimTime::redo(Model *model)
{
	if(m_animFrame==INT_MAX)
	model->setAnimTimeFrame(m_mode,m_animNum,m_newTime);
	else
	model->setAnimFrameTime(m_mode,m_animNum,m_animFrame,m_newTime);

	/*REMOVE ME
	if(model->getAnimationMode()!=Model::ANIMMODE_NONE&&(model->getAnimationMode()!=m_mode||model->getCurrentAnimation()!=m_animNum))
	{
		model->setCurrentAnimation(m_mode,m_animNum); //???
	}*/
}
bool MU_SetAnimTime::combine(Undo *u)
{
	MU_SetAnimTime *undo = dynamic_cast<MU_SetAnimTime*>(u);

	if(undo&&undo->m_mode==m_mode&&undo->m_animNum==m_animNum&&m_animFrame==undo->m_animFrame)
	{
		m_newTime = undo->m_newTime;
		return true;
	}
	else
	{
		return false;
	}
}
void MU_SetAnimTime::setAnimFrameTime(Model::AnimationModeE mode, 
unsigned animNum, unsigned frame, double newTime, double oldTime)
{
	m_mode		 = mode;
	m_animNum	 = animNum; m_animFrame = frame;
	m_newTime	 = newTime;
	m_oldTime	 = oldTime;
}

void MU_SetObjectKeyframe::undo(Model *model, bool skel)
{
	SetKeyframeList::iterator it;

	for(it = m_keyframes.begin(); it!=m_keyframes.end(); it++)
	{
		Model::Position pos;
		pos.type = skel?Model::PT_Joint:Model::PT_Point; 
		pos.index = it->number;

		if(it->isNew)
		{
			log_debug("undoing new keyframe\n"); //???
			
			model->removeKeyframe(m_anim,m_frame,pos,m_isRotation,true);			
		}
		else
		{
			log_debug("undoing existing keyframe\n"); //???
			
			model->setKeyframe(m_anim,m_frame,pos,m_isRotation,it->oldx,it->oldy,it->oldz,it->olde);			
		}
	}

	_sync_animation(model,skel,m_anim,m_frame); //REMOVE US
}

void MU_SetObjectKeyframe::redo(Model *model, bool skel)
{
	SetKeyframeList::iterator it;

	for(it = m_keyframes.begin(); it!=m_keyframes.end(); it++)
	{
		Model::Position pos;
		pos.type = skel?Model::PT_Joint:Model::PT_Point; 
		pos.index = it->number;

		model->setKeyframe(m_anim,m_frame,pos,m_isRotation,it->x,it->y,it->z,it->e);		
	}

	_sync_animation(model,skel,m_anim,m_frame); //REMOVE US
}

bool MU_SetJointKeyframe::combine(Undo *u)
{
	return MU_SetObjectKeyframe::combine(dynamic_cast<MU_SetJointKeyframe*>(u));
}
bool MU_SetPointKeyframe::combine(Undo *u)
{
	return MU_SetObjectKeyframe::combine(dynamic_cast<MU_SetPointKeyframe*>(u));
}
bool MU_SetObjectKeyframe::combine(Undo *u)
{
	MU_SetObjectKeyframe *undo = dynamic_cast<MU_SetObjectKeyframe*>(u);

	if(undo&&undo->m_anim==m_anim&&undo->m_frame==m_frame&&m_isRotation==undo->m_isRotation)
	{
		SetKeyframeList::iterator it;

		for(it = undo->m_keyframes.begin(); it!=undo->m_keyframes.end(); it++)
		{
			addKeyframe(it->number,it->isNew,it->x,it->y,it->z,it->e,
					it->oldx,it->oldy,it->oldz,it->olde);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetObjectKeyframe::size()
{
	return sizeof(MU_SetObjectKeyframe)+m_keyframes.size()*sizeof(SetKeyFrameT);
}

void MU_SetObjectKeyframe::setAnimationData(unsigned anim, unsigned frame, Model::KeyType2020E isRotation)
{
	m_anim		 = anim;
	m_frame		= frame;
	m_isRotation = isRotation;
}

void MU_SetObjectKeyframe::addKeyframe(int j,bool isNew, 
	double x, double y, double z, Model::Interpolate2020E e,
		double oldx, double oldy, double oldz, Model::Interpolate2020E olde)
{
	unsigned index = 0;
	SetKeyFrameT mv;
	mv.number = j;

	// Modify a joint we already have
	if(m_keyframes.find_sorted(mv,index))
	{
		m_keyframes[index].x = x;
		m_keyframes[index].y = y;
		m_keyframes[index].z = z;
		return;
	}

	// Not found,add new joint information
	mv.isNew	= isNew;
	mv.x		 = x;
	mv.y		 = y;
	mv.z		 = z;
	mv.e		 = e;
	mv.oldx	 = oldx;
	mv.oldy	 = oldy;
	mv.oldz	 = oldz;
	mv.olde	 = olde;
	m_keyframes.insert_sorted(mv);
}

void MU_DeleteObjectKeyframe::undo(Model *model, bool skel)
{
	log_debug("undo delete keyframe\n");
	DeleteKeyframeList::reverse_iterator it;

	unsigned frame = 0;
	for(it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->insertKeyframe(m_anim,skel?Model::PT_Joint:Model::PT_Point,*it);
		
		frame = (*it)->m_frame;
	}

	_sync_animation(model,skel,m_anim,frame); //REMOVE US
}

void MU_DeleteObjectKeyframe::redo(Model *model, bool skel)
{
	DeleteKeyframeList::iterator it;

	unsigned frame = 0;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		Model::Position pos;
		pos.type = skel?Model::PT_Joint:Model::PT_Point; 
		pos.index = (*it)->m_objectIndex;
		model->removeKeyframe(m_anim,(*it)->m_frame,pos,(*it)->m_isRotation);

		frame = (*it)->m_frame;
	}

	_sync_animation(model,skel,m_anim,frame); //REMOVE US
}

bool MU_DeleteJointKeyframe::combine(Undo *u)
{
	return MU_DeleteObjectKeyframe::combine(dynamic_cast<MU_DeleteJointKeyframe*>(u));
}
bool MU_DeletePointKeyframe::combine(Undo *u)
{
	return MU_DeleteObjectKeyframe::combine(dynamic_cast<MU_DeletePointKeyframe*>(u));
}
bool MU_DeleteObjectKeyframe::combine(Undo *u)
{
	MU_DeleteObjectKeyframe *undo = dynamic_cast<MU_DeleteObjectKeyframe*>(u);

	if(undo&&m_anim==undo->m_anim)
	{
		DeleteKeyframeList::iterator it;
		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			deleteKeyframe(*it);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void MU_DeleteObjectKeyframe::undoRelease()
{
	DeleteKeyframeList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		(*it)->release();
	}
}

unsigned MU_DeleteObjectKeyframe::size()
{
	return sizeof(MU_DeleteObjectKeyframe)+m_list.size()*sizeof(Model::Keyframe);
}

void MU_DeleteObjectKeyframe::setAnimationData(unsigned anim)
{
	m_anim = anim;
}

void MU_DeleteObjectKeyframe::deleteKeyframe(Model::Keyframe *keyframe)
{
	m_list.push_back(keyframe);
}

void MU_SetObjectName::undo(Model *model)
{
	model->setPositionName(m_obj,m_oldName.c_str());
}

void MU_SetObjectName::redo(Model *model)
{
	model->setPositionName(m_obj,m_newName.c_str());
}

bool MU_SetObjectName::combine(Undo *u)
{
	return false;
}

unsigned MU_SetObjectName::size()
{
	return sizeof(MU_SetObjectName);
}

void MU_SetObjectName::setName(const char *newName, const char *oldName)
{
	m_newName = newName;
	m_oldName = oldName;
}

void MU_SetProjectionType::undo(Model *model)
{
	model->setProjectionType(m_projection,m_oldType);
}

void MU_SetProjectionType::redo(Model *model)
{
	model->setProjectionType(m_projection,m_newType);
}

bool MU_SetProjectionType::combine(Undo *u)
{
	return false;
}

unsigned MU_SetProjectionType::size()
{
	return sizeof(MU_SetProjectionType);
}

void MU_SetProjectionType::setType(unsigned projection, int newType, int oldType)
{
	m_projection = projection;
	m_newType	 = newType;
	m_oldType	 = oldType;
}

void MU_MoveFrameVertex::undo(Model *model)
{
	MoveFrameVertexList::iterator it;

	// Modify a vertex we already have
	for(it = m_vertices.begin(); it!=m_vertices.end(); it++)
	{
		model->setQuickFrameAnimVertexCoords(m_anim,m_frame,it->number,it->oldx,it->oldy,it->oldz,it->olde);
	}

	_sync_animation(model,false,m_anim,m_frame); //REMOVE US
}

void MU_MoveFrameVertex::redo(Model *model)
{
	MoveFrameVertexList::iterator it;

	// Modify a vertex we already have
	for(it = m_vertices.begin(); it!=m_vertices.end(); it++)
	{
		model->setQuickFrameAnimVertexCoords(m_anim,m_frame,it->number,it->x,it->y,it->z,it->e);
	}

	_sync_animation(model,false,m_anim,m_frame); //REMOVE US
}

bool MU_MoveFrameVertex::combine(Undo *u)
{
	MU_MoveFrameVertex *undo = dynamic_cast<MU_MoveFrameVertex*>(u);

	if(undo)
	{
		MoveFrameVertexList::iterator it;

		for(it = undo->m_vertices.begin(); it!=undo->m_vertices.end(); it++)
		{
			addVertex(*it);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_MoveFrameVertex::size()
{
	return sizeof(MU_MoveFrameVertex)+m_vertices.size()*sizeof(MoveFrameVertexT);
}

void MU_MoveFrameVertex::setAnimationData(unsigned anim, unsigned frame)
{
	m_anim = anim;
	m_frame = frame;
}

void MU_MoveFrameVertex::addVertex(int v, double x, double y, double z, 
		Model::Interpolate2020E e, Model::FrameAnimVertex *old, bool sort)
{
	MoveFrameVertexT mv;
	mv.number = v;
	mv.x = x;
	mv.y = y;
	mv.z = z;
	mv.e = e;
	mv.oldx	 = old->m_coord[0];
	mv.oldy	 = old->m_coord[1];
	mv.oldz	 = old->m_coord[2];	
	mv.olde = old->m_interp2020;

	if(sort) addVertex(mv); 
	else m_vertices.push_back(mv); //2020
}
void MU_MoveFrameVertex::addVertex(MoveFrameVertexT &mv)
{
	// Modify a vertex we already have
	unsigned index;
	if(m_vertices.find_sorted(mv,index))
	{
		m_vertices[index].x = mv.x;
		m_vertices[index].y = mv.y;
		m_vertices[index].z = mv.z;
		m_vertices[index].e = mv.e;
	}
	else // Not found,add new vertex information
	{
		m_vertices.insert_sorted(mv);
	}
}

/*
void MU_MoveFramePoint::undo(Model *model)
{
	MoveFramePointList::iterator it;

	for(it = m_points.begin(); it!=m_points.end(); it++)
	{
		//FIX ME: Scale?
		Point &o = it->oldp;
		model->setQuickFrameAnimPoint(m_anim,m_frame,it->number,
		o.p[0],o.p[1],o.p[2],o.r[0],o.r[1],o.r[2],o.e);
	}
	
	_sync_animation(model,false,m_anim,m_frame); //REMOVE US
}
void MU_MoveFramePoint::redo(Model *model)
{
	MoveFramePointList::iterator it;

	for(it = m_points.begin(); it!=m_points.end(); it++)
	{
		//FIX ME: Scale?
		Point &p = it->p;
		model->setQuickFrameAnimPoint(m_anim,m_frame,it->number,
		p.p[0],p.p[1],p.p[2],p.r[0],p.r[1],p.r[2],p.e);
	}

	_sync_animation(model,false,m_anim,m_frame); //REMOVE US
}
bool MU_MoveFramePoint::combine(Undo *u)
{
	MU_MoveFramePoint *undo = dynamic_cast<MU_MoveFramePoint*>(u);

	if(undo)
	{
		MoveFramePointList::iterator it;

		for(it = undo->m_points.begin(); it!=undo->m_points.end(); it++)
		{
			addPoint(it->number,it->p,it->oldp);
		}

		return true;
	}
	else
	{
		return false;
	}
}
unsigned MU_MoveFramePoint::size()
{
	return sizeof(MU_MoveFramePoint)+m_points.size()*sizeof(MoveFramePointT);
}
void MU_MoveFramePoint::setAnimationData(unsigned anim, unsigned frame)
{
	m_anim = anim;
	m_frame = frame;
}
void MU_MoveFramePoint::addPoint(int p, const Point &move, const Point &old)
{
	unsigned index;
	MoveFramePointT mv;
	mv.number = p;
	
	// Modify a point we already have
	if(m_points.find_sorted(mv,index))
	{
		m_points[index].p = move;
		return;
	}
	
	// Not found, add new point information
	mv.p = move;
	mv.oldp = old;
	m_points.insert_sorted(mv);
}*/

void MU_SetPositionInfluence::undo(Model *model)
{
	if(m_isAdd)
	{
		model->removeInfluence(m_pos,m_index);
	}
	else
	{
		model->insertInfluence(m_pos,m_index,m_influence);
	}
}

void MU_SetPositionInfluence::redo(Model *model)
{
	if(m_isAdd)
	{
		model->insertInfluence(m_pos,m_index,m_influence);
	}
	else
	{
		model->removeInfluence(m_pos,m_index);
	}
}

bool MU_SetPositionInfluence::combine(Undo *u)
{
	return false;
}

unsigned MU_SetPositionInfluence::size()
{
	return sizeof(MU_SetPositionInfluence);
}

void MU_SetPositionInfluence::setPositionInfluence(bool isAdd,
		const Model::Position &pos,
		unsigned index, const Model::InfluenceT &influence)
{
	m_isAdd = isAdd;
	m_index = index;
	m_pos = pos;
	m_influence = influence;
}

void MU_UpdatePositionInfluence::undo(Model *model)
{
	model->setPositionInfluenceType(
			m_pos,m_oldInf.m_boneId,m_oldInf.m_type);
	model->setPositionInfluenceWeight(
			m_pos,m_oldInf.m_boneId,m_oldInf.m_weight);
}

void MU_UpdatePositionInfluence::redo(Model *model)
{
	model->setPositionInfluenceType(
			m_pos,m_newInf.m_boneId,m_newInf.m_type);
	model->setPositionInfluenceWeight(
			m_pos,m_newInf.m_boneId,m_newInf.m_weight);
}

bool MU_UpdatePositionInfluence::combine(Undo *u)
{
	return false; //IMPLEMENT ME
}

unsigned MU_UpdatePositionInfluence::size()
{
	return sizeof(MU_UpdatePositionInfluence);
}

void MU_UpdatePositionInfluence::updatePositionInfluence(const Model::Position &pos,
		const Model::InfluenceT &newInf,
		const Model::InfluenceT &oldInf)
{
	m_pos = pos;
	m_newInf = newInf;
	m_oldInf = oldInf;
}

/*UNUSED
void MU_SetVertexBoneJoint::undo(Model *model)
{
	log_debug("undo set vertex bone joint\n");
	SetJointList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setVertexBoneJoint(it->vertex,it->oldBone);
	}
}
void MU_SetVertexBoneJoint::redo(Model *model)
{
	SetJointList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setVertexBoneJoint(it->vertex,it->newBone);
	}
}
bool MU_SetVertexBoneJoint::combine(Undo *u)
{
	MU_SetVertexBoneJoint *undo = dynamic_cast<MU_SetVertexBoneJoint*>(u);

	if(undo)
	{
		SetJointList::iterator it;

		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			setVertexBoneJoint(it->vertex,it->newBone,it->oldBone);
		}

		return true;
	}
	else
	{
		return false;
	}
}
unsigned MU_SetVertexBoneJoint::size()
{
	return sizeof(MU_SetVertexBoneJoint)+m_list.size()*sizeof(SetJointT);
}
void MU_SetVertexBoneJoint::setVertexBoneJoint(const unsigned &vertex,
		const int &newBone, const int &oldBone)
{
	SetJointList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->vertex==vertex)
		{
			it->newBone = newBone;
			return;
		}
	}

	SetJointT sj;
	sj.vertex	= vertex;
	sj.newBone  = newBone;
	sj.oldBone  = oldBone;
	m_list.push_back(sj);
}*/

/*UNUSED
void MU_SetPointBoneJoint::undo(Model *model)
{
	log_debug("undo set point bone joint\n");
	SetJointList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setPointBoneJoint(it->point,it->oldBone);
	}
}
void MU_SetPointBoneJoint::redo(Model *model)
{
	SetJointList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setPointBoneJoint(it->point,it->newBone);
	}
}
bool MU_SetPointBoneJoint::combine(Undo *u)
{
	MU_SetPointBoneJoint *undo = dynamic_cast<MU_SetPointBoneJoint*>(u);

	if(undo)
	{
		SetJointList::iterator it;

		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			setPointBoneJoint(it->point,it->newBone,it->oldBone);
		}

		return true;
	}
	else
	{
		return false;
	}
}
unsigned MU_SetPointBoneJoint::size()
{
	return sizeof(MU_SetPointBoneJoint)+m_list.size()*sizeof(SetJointT);
}
void MU_SetPointBoneJoint::setPointBoneJoint(const unsigned &point,
		const int &newBone, const int &oldBone)
{
	SetJointList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->point==point)
		{
			it->newBone = newBone;
			return;
		}
	}

	SetJointT sj;
	sj.point	 = point;
	sj.newBone  = newBone;
	sj.oldBone  = oldBone;
	m_list.push_back(sj);
}*/

void MU_SetTriangleProjection::undo(Model *model)
{
	log_debug("undo set triangle projection\n");
	SetProjectionList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setTriangleProjection(it->triangle,it->oldProj);
	}
}

void MU_SetTriangleProjection::redo(Model *model)
{
	SetProjectionList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setTriangleProjection(it->triangle,it->newProj);
	}
}

bool MU_SetTriangleProjection::combine(Undo *u)
{
	MU_SetTriangleProjection *undo = dynamic_cast<MU_SetTriangleProjection*>(u);

	if(undo)
	{
		SetProjectionList::iterator it;

		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			setTriangleProjection(it->triangle,it->newProj,it->oldProj);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetTriangleProjection::size()
{
	return sizeof(MU_SetTriangleProjection)+m_list.size()*sizeof(SetProjectionT);
}

void MU_SetTriangleProjection::setTriangleProjection(const unsigned &triangle,
		const int &newProj, const int &oldProj)
{
	SetProjectionList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->triangle==triangle)
		{
			it->newProj = newProj;
			return;
		}
	}

	SetProjectionT sj;
	sj.triangle = triangle;
	sj.newProj  = newProj;
	sj.oldProj  = oldProj;
	m_list.push_back(sj);
}

void MU_AddAnimation::undo(Model *model)
{
	if(m_skelAnim)
	{
		model->removeSkelAnim(m_anim);
		unsigned num = (m_anim<model->getAnimCount(Model::ANIMMODE_SKELETAL))? m_anim : m_anim-1;

		if(model->getAnimationMode()!=Model::ANIMMODE_NONE)
		{
			model->setCurrentAnimation(Model::ANIMMODE_SKELETAL,num);
		}
	}
	if(m_frameAnim)
	{
		model->removeFrameAnim(m_anim);
		unsigned num = (m_anim<model->getAnimCount(Model::ANIMMODE_FRAME))? m_anim : m_anim-1;

		if(model->getAnimationMode()!=Model::ANIMMODE_NONE)
		{
			model->setCurrentAnimation(Model::ANIMMODE_FRAME,num);
		}
	}
}

void MU_AddAnimation::redo(Model *model)
{
	if(m_skelAnim)
	{
		model->insertSkelAnim(m_anim,m_skelAnim);

		if(model->getAnimationMode()!=Model::ANIMMODE_NONE)
		{
			model->setCurrentAnimation(Model::ANIMMODE_SKELETAL,m_anim);
		}
	}
	if(m_frameAnim)
	{
		model->insertFrameAnim(m_anim,m_frameAnim);

		if(model->getAnimationMode()!=Model::ANIMMODE_NONE)
		{
			model->setCurrentAnimation(Model::ANIMMODE_FRAME,m_anim);
		}
	}
}

bool MU_AddAnimation::combine(Undo *u)
{
	return false;
}

void MU_AddAnimation::redoRelease()
{
	log_debug("releasing animation in redo\n");
	if(m_skelAnim)
	{
		m_skelAnim->release();
	}
	if(m_frameAnim)
	{
		m_frameAnim->release();
	}
}

unsigned MU_AddAnimation::size()
{
	unsigned frameAnimSize = (m_frameAnim)? sizeof(m_frameAnim): 0;
	unsigned frame = 0;
	unsigned skelAnimSize = (m_skelAnim)? sizeof(m_skelAnim): 0;

	//FIX ME
	//I give up for now:
	//https://github.com/zturtleman/mm3d/issues/128
	/*2020: There shouldn't be any frames and if there are it's because
	//they were added after this undo and so should not be included in 
	//its memory footprint.
	if(m_frameAnim)
	{
		unsigned count = m_frameAnim->_frame_count();
		for(frame = 0; frame<count; frame++)
		{
			Model::FrameAnimVertexList *vertexList = &m_frameAnim->m_frameData[frame]->m_frameVertices;
			frameAnimSize += sizeof(Model::FrameAnimVertexList)+vertexList->size()*sizeof(Model::FrameAnimVertex);
			Model::FrameAnimPointList *pointList = &m_frameAnim->m_frameData[frame]->m_framePoints;
			frameAnimSize += sizeof(Model::FrameAnimPointList)+pointList->size()*sizeof(Model::FrameAnimPoint);
		}
	}
	if(m_skelAnim)
	{
		unsigned jointCount = m_skelAnim->m_keyframes.size();
		for(unsigned j = 0; j<jointCount; j++)
		{
			Model::KeyframeList &list = m_skelAnim->m_keyframes[j];
			skelAnimSize += sizeof(list)+list.size()*sizeof(*list.data());
		}
		//skelAnimSize += jointCount *sizeof(Model::JointKeyframeList); //???
		skelAnimSize += jointCount *sizeof(Model::KeyframeList);
	}*/

	return sizeof(MU_AddAnimation)+frameAnimSize+skelAnimSize;
}

void MU_AddAnimation::addAnimation(const unsigned &anim,Model::SkelAnim *skelanim)
{
	m_anim		= anim;
	m_skelAnim  = skelanim;
	m_frameAnim = nullptr;
}

void MU_AddAnimation::addAnimation(const unsigned &anim,Model::FrameAnim *frameanim)
{
	m_anim		= anim;
	m_skelAnim  = nullptr;
	m_frameAnim = frameanim;
}

void MU_DeleteAnimation::undo(Model *model)
{
	if(m_skelAnim)
	{
		model->insertSkelAnim(m_anim,m_skelAnim);
		if(model->getAnimationMode()!=Model::ANIMMODE_NONE)
		{
			model->setCurrentAnimation(Model::ANIMMODE_SKELETAL,m_anim);
		}
	}
	if(auto fa=m_frameAnim)
	{
		model->insertFrameAnim(m_anim,fa);
		//2020: This restores FrameAnimVertex data to m_vertices.
		model->insertFrameAnimFrame(fa->m_frame0,fa->_frame_count(),&m_vertices);
		if(model->getAnimationMode()!=Model::ANIMMODE_NONE)
		{
			model->setCurrentAnimation(Model::ANIMMODE_FRAME,m_anim);
		}
	}
}

void MU_DeleteAnimation::redo(Model *model)
{
	if(m_skelAnim)
	{
		model->removeSkelAnim(m_anim);
		unsigned num = (m_anim<model->getAnimCount(Model::ANIMMODE_SKELETAL))? m_anim : m_anim-1;
		if(model->getAnimationMode()!=Model::ANIMMODE_NONE)
		{
			model->setCurrentAnimation(Model::ANIMMODE_SKELETAL,num);
		}
	}
	if(auto fa=m_frameAnim)
	{
		//2020: This removes FrameAnimVertex data from m_vertices.
		model->removeFrameAnimFrame(fa->m_frame0,fa->_frame_count(),nullptr);
		model->removeFrameAnim(m_anim);

		unsigned num = (m_anim<model->getAnimCount(Model::ANIMMODE_FRAME))? m_anim : m_anim-1;
		if(model->getAnimationMode()!=Model::ANIMMODE_NONE)
		{
			model->setCurrentAnimation(Model::ANIMMODE_FRAME,num);
		}
	}
}

bool MU_DeleteAnimation::combine(Undo *u)
{
	return false;
}

void MU_DeleteAnimation::undoRelease()
{
	//log_debug("releasing animation in undo\n");
	if(m_skelAnim)
	{
		m_skelAnim->release();
	}
	if(m_frameAnim)
	{
		m_frameAnim->release();
		
		for(auto*ea:m_vertices) ea->release();
	}
}

unsigned MU_DeleteAnimation::size()
{
	unsigned frameAnimSize = (m_frameAnim)? sizeof(m_frameAnim): 0;
	unsigned frame = 0;

	Model::AnimBase2020 *ab = m_skelAnim;
	if(m_frameAnim)
	{
		ab = m_frameAnim;

		unsigned count = m_frameAnim->_frame_count();
		/*
		for(frame = 0; frame<count; frame++)
		{
			Model::FrameAnimVertexList *vertexList = &m_frameAnim->m_frameData[frame]->m_frameVertices;
			frameAnimSize += sizeof(Model::FrameAnimVertexList)+vertexList->size()*sizeof(Model::FrameAnimVertex);
			Model::FrameAnimPointList *pointList = &m_frameAnim->m_frameData[frame]->m_framePoints;
			frameAnimSize += sizeof(Model::FrameAnimPointList)+pointList->size()*sizeof(Model::FrameAnimPoint);
		}*/
		frameAnimSize+=count*m_vertices.size()*sizeof(Model::FrameAnimVertex);
	}

	unsigned skelAnimSize = (m_skelAnim)? sizeof(m_skelAnim): 0;

	if(ab) //m_skelAnim
	{
		unsigned jointCount = ab->m_keyframes.size();
		for(unsigned j = 0; j<jointCount; j++)
		{
			Model::KeyframeList &list = ab->m_keyframes[j];
			skelAnimSize += sizeof(list)+list.size()*sizeof(*list.data());
		}
		//skelAnimSize += jointCount *sizeof(Model::JointKeyframeList); //???
		skelAnimSize += jointCount *sizeof(Model::KeyframeList);
	}

	return sizeof(MU_DeleteAnimation)+frameAnimSize+skelAnimSize;
}

void MU_DeleteAnimation::deleteAnimation(const unsigned &anim,Model::SkelAnim *skelanim)
{
	m_anim		= anim;
	m_skelAnim  = skelanim;
	m_frameAnim = nullptr;
}

void MU_DeleteAnimation::deleteAnimation(const unsigned &anim,Model::FrameAnim *frameanim)
{
	m_anim		= anim;
	m_skelAnim  = nullptr;
	m_frameAnim = frameanim;
}

void MU_SetJointParent::undo(Model *model)
{
	model->setBoneJointParent(m_joint,m_oldParent);
}

void MU_SetJointParent::redo(Model *model)
{
	model->setBoneJointParent(m_joint,m_newParent);
}

void MU_SetJointParent::setJointParent(unsigned joint, int newParent, int oldParent)
{
	m_joint = joint;
	m_newParent = newParent;
	m_oldParent = oldParent;
}

void MU_SetProjectionRange::undo(Model *model)
{
	model->setProjectionRange(m_proj,
			m_oldRange[0],m_oldRange[1],m_oldRange[2],m_oldRange[3]);
}

void MU_SetProjectionRange::redo(Model *model)
{
	model->setProjectionRange(m_proj,
			m_newRange[0],m_newRange[1],m_newRange[2],m_newRange[3]);
}

void MU_SetProjectionRange::setProjectionRange(const unsigned &proj,
		double newXMin, double newYMin, double newXMax, double newYMax,
		double oldXMin, double oldYMin, double oldXMax, double oldYMax)
{
	m_proj = proj;

	m_newRange[0] = newXMin;
	m_newRange[1] = newYMin;
	m_newRange[2] = newXMax;
	m_newRange[3] = newYMax;

	m_oldRange[0] = oldXMin;
	m_oldRange[1] = oldYMin;
	m_oldRange[2] = oldXMax;
	m_oldRange[3] = oldYMax;
}

void MU_AddBoneJoint::undo(Model *model)
{
	model->removeBoneJoint(m_jointNum);
}

void MU_AddBoneJoint::redo(Model *model)
{
	model->insertBoneJoint(m_jointNum,m_joint);
}

bool MU_AddBoneJoint::combine(Undo *u)
{
	return false;
}

void MU_AddBoneJoint::redoRelease()
{
	m_joint->release();
}

unsigned MU_AddBoneJoint::size()
{
	return sizeof(MU_AddBoneJoint)+sizeof(Model::Joint);
}

void MU_AddBoneJoint::addBoneJoint(unsigned jointNum,Model::Joint *joint)
{
	m_jointNum = jointNum;
	m_joint	 = joint;
}

void MU_AddPoint::undo(Model *model)
{
	model->removePoint(m_pointNum);
}

void MU_AddPoint::redo(Model *model)
{
	model->insertPoint(m_pointNum,m_point);
}

bool MU_AddPoint::combine(Undo *u)
{
	return false;
}

void MU_AddPoint::redoRelease()
{
	m_point->release();
}

unsigned MU_AddPoint::size()
{
	return sizeof(MU_AddPoint)+sizeof(Model::Point);
}

void MU_AddPoint::addPoint(unsigned pointNum,Model::Point *point)
{
	m_pointNum = pointNum;
	m_point	 = point;
}

void MU_AddProjection::undo(Model *model)
{
	model->removeProjection(m_projNum);
}

void MU_AddProjection::redo(Model *model)
{
	model->insertProjection(m_projNum,m_proj);
}

bool MU_AddProjection::combine(Undo *u)
{
	return false;
}

void MU_AddProjection::redoRelease()
{
	m_proj->release();
}

unsigned MU_AddProjection::size()
{
	return sizeof(MU_AddProjection)+sizeof(Model::TextureProjection);
}

void MU_AddProjection::addProjection(unsigned projNum,Model::TextureProjection *proj)
{
	m_projNum = projNum;
	m_proj	 = proj;
}

void MU_DeleteBoneJoint::undo(Model *model)
{
	log_debug("undo delete joint\n");
	model->insertBoneJoint(m_jointNum,m_joint);
}

void MU_DeleteBoneJoint::redo(Model *model)
{
	log_debug("redo delete joint\n");
	model->removeBoneJoint(m_jointNum);
}

bool MU_DeleteBoneJoint::combine(Undo *u)
{
	return false;
}

void MU_DeleteBoneJoint::undoRelease()
{
	m_joint->release();
}

unsigned MU_DeleteBoneJoint::size()
{
	return sizeof(MU_DeleteBoneJoint)+sizeof(Model::Joint);
}

void MU_DeleteBoneJoint::deleteBoneJoint(unsigned jointNum,Model::Joint *joint)
{
	m_jointNum = jointNum;
	m_joint	 = joint;
}

void MU_DeletePoint::undo(Model *model)
{
	log_debug("undo delete point\n");
	model->insertPoint(m_pointNum,m_point);
}

void MU_DeletePoint::redo(Model *model)
{
	log_debug("redo delete point\n");
	model->removePoint(m_pointNum);
}

bool MU_DeletePoint::combine(Undo *u)
{
	return false;
}

void MU_DeletePoint::undoRelease()
{
	m_point->release();
}

unsigned MU_DeletePoint::size()
{
	return sizeof(MU_DeletePoint)+sizeof(Model::Point);
}

void MU_DeletePoint::deletePoint(unsigned pointNum,Model::Point *point)
{
	m_pointNum = pointNum;
	m_point	 = point;
}

void MU_DeleteProjection::undo(Model *model)
{
	log_debug("undo delete proj\n");
	model->insertProjection(m_projNum,m_proj);
}

void MU_DeleteProjection::redo(Model *model)
{
	log_debug("redo delete proj\n");
	model->removeProjection(m_projNum);
}

bool MU_DeleteProjection::combine(Undo *u)
{
	return false;
}

void MU_DeleteProjection::undoRelease()
{
	m_proj->release();
}

unsigned MU_DeleteProjection::size()
{
	return sizeof(MU_DeleteProjection)+sizeof(Model::TextureProjection);
}

void MU_DeleteProjection::deleteProjection(unsigned projNum,Model::TextureProjection *proj)
{
	m_projNum = projNum;
	m_proj	 = proj;
}

void MU_SetGroupSmooth::undo(Model *model)
{
	SetSmoothList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setGroupSmooth(it->group,it->oldSmooth);
	}
}

void MU_SetGroupSmooth::redo(Model *model)
{
	SetSmoothList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setGroupSmooth(it->group,it->newSmooth);
	}
}

bool MU_SetGroupSmooth::combine(Undo *u)
{
	MU_SetGroupSmooth *undo = dynamic_cast<MU_SetGroupSmooth*>(u);

	if(undo)
	{
		SetSmoothList::iterator it;

		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			setGroupSmooth(it->group,it->newSmooth,it->oldSmooth);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetGroupSmooth::size()
{
	return sizeof(MU_SetGroupSmooth)+m_list.size()*sizeof(SetSmoothT);
}

void MU_SetGroupSmooth::setGroupSmooth(const unsigned &group,
		const uint8_t &newSmooth, const uint8_t &oldSmooth)
{
	SetSmoothList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->group==group)
		{
			it->newSmooth = newSmooth;
			return;
		}
	}

	SetSmoothT ss;
	ss.group		= group;
	ss.newSmooth  = newSmooth;
	ss.oldSmooth  = oldSmooth;
	m_list.push_back(ss);
}

void MU_SetGroupAngle::undo(Model *model)
{
	SetAngleList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setGroupAngle(it->group,it->oldAngle);
	}
}

void MU_SetGroupAngle::redo(Model *model)
{
	SetAngleList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setGroupAngle(it->group,it->newAngle);
	}
}

bool MU_SetGroupAngle::combine(Undo *u)
{
	MU_SetGroupAngle *undo = dynamic_cast<MU_SetGroupAngle*>(u);

	if(undo)
	{
		SetAngleList::iterator it;

		for(it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		{
			setGroupAngle(it->group,it->newAngle,it->oldAngle);
		}

		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetGroupAngle::size()
{
	return sizeof(MU_SetGroupAngle)+m_list.size()*sizeof(SetAngleT);
}

void MU_SetGroupAngle::setGroupAngle(const unsigned &group,
		const uint8_t &newAngle, const uint8_t &oldAngle)
{
	SetAngleList::iterator it;

	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->group==group)
		{
			it->newAngle = newAngle;
			return;
		}
	}

	SetAngleT ss;
	ss.group	  = group;
	ss.newAngle  = newAngle;
	ss.oldAngle  = oldAngle;
	m_list.push_back(ss);
}

void MU_SetGroupName::undo(Model *model)
{
	model->setGroupName(m_groupNum,m_oldName.c_str());
}

void MU_SetGroupName::redo(Model *model)
{
	model->setGroupName(m_groupNum,m_newName.c_str());
}

bool MU_SetGroupName::combine(Undo *u)
{
	MU_SetGroupName *undo = dynamic_cast<MU_SetGroupName*>(u);

	if(undo&&undo->m_groupNum==m_groupNum)
	{
		m_newName = undo->m_newName;
		return true;
	}
	else
	{
		return false;
	}
}

unsigned MU_SetGroupName::size()
{
	return sizeof(MU_SetGroupName);
}

void MU_SetGroupName::setGroupName(unsigned groupNum, const char *newName, const char *oldName)
{
	m_groupNum	= groupNum;
	m_newName	 = newName;
	m_oldName	 = oldName;
}

void MU_MoveAnimation::undo(Model *model)
{
	model->moveAnimation(m_mode,m_newIndex,m_oldIndex);
}

void MU_MoveAnimation::redo(Model *model)
{
	model->moveAnimation(m_mode,m_oldIndex,m_newIndex);
}

bool MU_MoveAnimation::combine(Undo *u)
{
	return false;
}

unsigned MU_MoveAnimation::size()
{
	return sizeof(MU_MoveAnimation);
}

void MU_MoveAnimation::moveAnimation(const Model::AnimationModeE &mode,
		const unsigned &oldIndex, const unsigned &newIndex)
{
	log_debug("moved animation from %d to %d\n",oldIndex,newIndex);

	m_mode	  = mode;
	m_oldIndex = oldIndex;
	m_newIndex = newIndex;
}

/*
void MU_SetFrameAnimVertexCount::undo(Model *model)
{
	model->setFrameAnimVertexCount(m_oldCount);
}

void MU_SetFrameAnimVertexCount::redo(Model *model)
{
	model->setFrameAnimVertexCount(m_newCount);
}

unsigned MU_SetFrameAnimVertexCount::size()
{
	return sizeof(MU_SetFrameAnimVertexCount);
}

void MU_SetFrameAnimVertexCount::setCount(const unsigned &newCount, const unsigned &oldCount)
{
	m_newCount = newCount;
	m_oldCount = oldCount;
}

void MU_SetFrameAnimPointCount::undo(Model *model)
{
	model->setFrameAnimPointCount(m_oldCount);
}

void MU_SetFrameAnimPointCount::redo(Model *model)
{
	model->setFrameAnimPointCount(m_newCount);
}

unsigned MU_SetFrameAnimPointCount::size()
{
	return sizeof(MU_SetFrameAnimPointCount);
}

void MU_SetFrameAnimPointCount::setCount(const unsigned &newCount, const unsigned &oldCount)
{
	m_newCount = newCount;
	m_oldCount = oldCount;
}*/

void MU_SetMaterialClamp::undo(Model *model)
{
	if(m_isS)
	{
		model->setTextureSClamp(m_material,m_oldClamp);
	}
	else
	{
		model->setTextureTClamp(m_material,m_oldClamp);
	}
}

void MU_SetMaterialClamp::redo(Model *model)
{
	if(m_isS)
	{
		model->setTextureSClamp(m_material,m_newClamp);
	}
	else
	{
		model->setTextureTClamp(m_material,m_newClamp);
	}
}

bool MU_SetMaterialClamp::combine(Undo *u)
{
	return false;
}

unsigned MU_SetMaterialClamp::size()
{
	return sizeof(MU_SetMaterialClamp);
}

void MU_SetMaterialClamp::setMaterialClamp(const unsigned &material, const bool &isS,
		const bool &newClamp, const bool &oldClamp)
{
	m_material = material;
	m_isS = isS;
	m_newClamp = newClamp;
	m_oldClamp = oldClamp;
}

void MU_SetMaterialTexture::undo(Model *model)
{
	if(m_oldTexture)
	{
		model->setMaterialTexture(m_material,m_oldTexture);
	}
	else
	{
		model->removeMaterialTexture(m_material);
	}
}

void MU_SetMaterialTexture::redo(Model *model)
{
	if(m_newTexture)
	{
		model->setMaterialTexture(m_material,m_newTexture);
	}
	else
	{
		model->removeMaterialTexture(m_material);
	}
}

bool MU_SetMaterialTexture::combine(Undo *u)
{
	return false;
}

unsigned MU_SetMaterialTexture::size()
{
	return sizeof(MU_SetMaterialTexture);
}

void MU_SetMaterialTexture::setMaterialTexture(const unsigned &material,
		Texture *newTexture,Texture *oldTexture)
{
	m_material = material;
	m_newTexture = newTexture;
	m_oldTexture = oldTexture;
}

void MU_AddMetaData::undo(Model *model)
{
	model->removeLastMetaData();
}

void MU_AddMetaData::redo(Model *model)
{
	model->addMetaData(m_key.c_str(),m_value.c_str());
}

bool MU_AddMetaData::combine(Undo *u)
{
	return false;
}

unsigned MU_AddMetaData::size()
{
	return sizeof(MU_AddMetaData)+m_key.size()+m_value.size();
}

void MU_AddMetaData::addMetaData(const std::string &key,
		const std::string &value)
{
	m_key	= key;
	m_value = value;
}

void MU_UpdateMetaData::undo(Model *model)
{
	model->updateMetaData(m_key.c_str(),m_oldValue.c_str());
}

void MU_UpdateMetaData::redo(Model *model)
{
	model->updateMetaData(m_key.c_str(),m_newValue.c_str());
}

bool MU_UpdateMetaData::combine(Undo *u)
{
	return false;
}

unsigned MU_UpdateMetaData::size()
{
	return sizeof(MU_UpdateMetaData)+m_key.size()+m_newValue.size()+m_oldValue.size();
}

void MU_UpdateMetaData::updateMetaData(const std::string &key,
		const std::string &newValue, const std::string &oldValue)
{
	m_key	= key;
	m_newValue = newValue;
	m_oldValue = oldValue;
}

void MU_ClearMetaData::undo(Model *model)
{
	Model::MetaDataList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->addMetaData(it->key.c_str(),it->value.c_str());
	}
}

void MU_ClearMetaData::redo(Model *model)
{
	model->clearMetaData();
}

bool MU_ClearMetaData::combine(Undo *u)
{
	return false;
}

unsigned MU_ClearMetaData::size()
{
	unsigned int s = sizeof(MU_ClearMetaData);
	s += m_list.size()*sizeof(Model::MetaData);
	Model::MetaDataList::iterator it;
	for(it = m_list.begin(); it!=m_list.end(); it++)
	{
		s += it->key.size();
		s += it->value.size();
	}
	return s;
}

void MU_ClearMetaData::clearMetaData(const Model::MetaDataList &list)
{
	m_list = list;
}


MU_InterpolateSelected::MU_InterpolateSelected
(Model::Interpolant2020E d, Model::Interpolate2020E e, bool s, unsigned a, unsigned f)
:m_d(d),m_e(e),m_skeletal(s),m_anim(a),m_frame(f)
{}
unsigned MU_InterpolateSelected::size()
{
	return sizeof(*this)+m_eold.size()*sizeof(Model::Interpolate2020E);
}
void MU_InterpolateSelected::_do(Model *m, bool undoing)
{
	auto it = m_eold.begin(), itt = m_eold.end();
	
	Model::Keyframe kf;
	kf.m_isRotation = Model::KeyType2020E(1<<m_d);
	kf.m_frame = m_frame;
	if(m_skeletal) 
	{	
		auto jt = m->m_skelAnims[m_anim]->m_keyframes.begin();
		for(auto*ea:m->m_joints) 
		{
			auto &c = *jt++;

			if(!ea->m_selected) continue;

			m->m_changeBits|=Model::MoveOther;

			unsigned index;
			if(c.find_sorted(&kf,index))
			c[index]->m_interp2020 = undoing?*it:m_e; 
			else assert(0);
			
			if(undoing) it++;
		}
	}
	else
	{
		auto fa = m->m_frameAnims[m_anim];
		auto fp = fa->m_frame0+m_frame;
		if(m_d==m->InterpolantCoords) 
		for(auto*ea:m->m_vertices) if(ea->m_selected)
		{
			m->m_changeBits|=Model::MoveGeometry;

			ea->m_frames[fp]->m_interp2020 = undoing?*it++:m_e; 
		}
		auto jt = fa->m_keyframes.begin();
		for(auto*ea:m->m_points) 
		{
			auto &c = *jt++;

			if(!ea->m_selected) continue;

			m->m_changeBits|=Model::MoveOther;

			unsigned index;
			if(c.find_sorted(&kf,index))
			c[index]->m_interp2020 = undoing?*it:m_e; 
			else assert(0);
			
			if(undoing) it++;
		}		
	}

	_sync_animation(m,m_skeletal,m_anim,m_frame); //REMOVE US
}
bool ModelUndo::_skel(Model::AnimationModeE e)
{
	return e==Model::ANIMMODE_SKELETAL; 
}
void ModelUndo::_sync_animation(Model *m, bool skel, unsigned anim, unsigned frame)
{
	//if(m->getAnimationMode()!=m->ANIMMODE_NONE) //???
	{
		m->setCurrentAnimation(skel?m->ANIMMODE_SKELETAL:m->ANIMMODE_FRAME,anim);
		m->setCurrentAnimationFrame(frame,m->AT_invalidateAnim);
	}
}