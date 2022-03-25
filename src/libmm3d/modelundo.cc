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
		for(int i=3;i-->0;) 
		m_vec[i]+=undo->m_vec[i]; return true;
	}
	return false;
}

void MU_TranslateSelected::undo(Model *model)
{
	//log_debug("undo translate selected\n");

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
		for(int t=3;t-->0;)
		if(m_point[t]!=undo->m_point[t])
		return false;
		m_matrix = m_matrix *undo->m_matrix;
		return true;
	}
	return false;
}
void MU_RotateSelected::setMatrixPoint(const Matrix &rhs, const double point[3])
{
	m_matrix = rhs; for(int t=3;t-->0;) m_point[t] = point[t];
}
void MU_RotateSelected::undo(Model *model)
{
	//log_debug("undo rotate selected\n");
	Matrix inv = m_matrix.getInverse();
	model->rotateSelected(inv,m_point);
}
void MU_RotateSelected::redo(Model *model)
{
	//log_debug("undo rotate selected\n");
	model->rotateSelected(m_matrix,m_point);
}

bool MU_ApplyMatrix::combine(Undo *u)
{
	if(auto undo=dynamic_cast<MU_ApplyMatrix*>(u))
	if(undo->m_scope==m_scope)
	{
		m_matrix = m_matrix*undo->m_matrix; 
		return true;
	}
	return false;
}

void MU_ApplyMatrix::setMatrix(const Matrix &m, Model::OperationScopeE scope)
{
	m_matrix = m; m_scope = scope;
}

void MU_ApplyMatrix::undo(Model *model)
{
	//log_debug("undo apply matrix\n"); //???

	model->applyMatrix(m_matrix.getInverse(),m_scope,true);
}

void MU_ApplyMatrix::redo(Model *model)
{
	model->applyMatrix(m_matrix,m_scope,true);
}

void MU_SelectionMode::undo(Model *model)
{
	//log_debug("undo selection mode %d\n",m_oldMode); //???

	model->setSelectionMode(m_oldMode);
}

void MU_SelectionMode::redo(Model *model)
{
	//log_debug("redo selection mode %d\n",m_mode); //???

	model->setSelectionMode(m_mode);
}

bool MU_SelectionMode::combine(Undo *u)
{
	if(auto undo=dynamic_cast<MU_SelectionMode*>(u))
	{
		m_mode = undo->m_mode; return true;
	}
	return false;
}

unsigned MU_SelectionMode::size()
{
	return sizeof(MU_SelectionMode);
}

void MU_Select::undo(Model *model)
{
	bool conv,unselect = false; //2019

	//2020: I optimized unselectTriangle with the
	//new connectivity data that should do better
	if(m_mode==Model::SelectTriangles)	
	conv = m_diff.size()<(unsigned)model->getTriangleCount()/10;

	/*2020: should go backward in time!
	// Invert selection from our list	
	for(auto it=m_diff.begin();it!=m_diff.end();it++)*/
	for(auto it=m_diff.rbegin();it!=m_diff.rend();it++)
	{
		if(!it->oldSelected) switch(m_mode)
		{
		case Model::SelectVertices:
			model->unselectVertex(it->number);
			break;
		case Model::SelectTriangles:
			unselect = true;
			//model->unselectTriangle(it->number,false);
			model->unselectTriangle(it->number,conv);
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
		}		
		else switch(m_mode)
		{
		case Model::SelectVertices:
			model->selectVertex(it->number,it->oldSelected);
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
		}
	}

	//2019: unselectTriangle did this per triangle
	if(unselect&&!conv) model->_selectVerticesFromTriangles();
}
void MU_Select::redo(Model *model)
{
	bool conv,unselect = false; //2019

	//2020: I optimized unselectTriangle with the
	//new connectivity data that should do better
	if(m_mode==Model::SelectTriangles)
	conv = m_diff.size()<(unsigned)model->getTriangleCount()/10;

	// Set selection from our list
	for(auto&ea:m_diff) 
	if(ea.selected) switch(m_mode)
	{
	case Model::SelectVertices:
		model->selectVertex(ea.number,ea.selected);
		break;
	case Model::SelectTriangles:
		model->selectTriangle(ea.number);
		break;
	case Model::SelectGroups:
		model->selectGroup(ea.number);
		break;
	case Model::SelectJoints:
		model->selectBoneJoint(ea.number);
		break;
	case Model::SelectPoints:
		model->selectPoint(ea.number);
		break;
	case Model::SelectProjections:
		model->selectProjection(ea.number);
		break;
	}
	else switch(m_mode)
	{
	case Model::SelectVertices:
		model->unselectVertex(ea.number);
		break;
	case Model::SelectTriangles:
		unselect = true;
		//model->unselectTriangle(ea.number,false);
		model->unselectTriangle(ea.number,conv);
		break;
	case Model::SelectGroups:
		model->unselectGroup(ea.number);
		break;
	case Model::SelectJoints:
		model->unselectBoneJoint(ea.number);
		break;
	case Model::SelectPoints:
		model->unselectPoint(ea.number);
		break;
	case Model::SelectProjections:
		model->unselectProjection(ea.number);
		break;
	}	

	//2019: unselectTriangle did this per triangle
	if(unselect&&!conv) model->_selectVerticesFromTriangles();
}

bool MU_Select::combine(Undo *u)
{
	//WHY WAS THIS DISABLED?
	//Restoring this. The _selectVerticesFromTriangles
	//fix above can't work with uncombined lists
	//https://github.com/zturtleman/mm3d/issues/93
	///*
	MU_Select *undo = dynamic_cast<MU_Select*>(u);
	if(undo&&getSelectionMode()==undo->getSelectionMode())
	{
		//2019: m_diff here was "sorted_list" but I'm removing that
		//since it will affect performance bad / can't think of any
		//reason to sort

		for(auto &ea:undo->m_diff)
		setSelectionDifference(ea.number,ea.selected,ea.oldSelected);
		return true;
	}
	//*/ 
	return false;
}

unsigned MU_Select::size()
{
	return sizeof(MU_Select)+m_diff.size()*sizeof(SelectionDifferenceT);
}
void MU_Select::setSelectionDifference(int number, OS &s, OS::Marker &oldS)
{
	unsigned a = s._select_op, b = oldS._op;
	//HACK: I'm concerned here begin/endSelectionDifference may override
	//the selection order without changing the selection state. It would
	//be a programmer error if so.
	if(!a==!b) s._select_op = b; else setSelectionDifference(number,a,b);
}
void MU_Select::setSelectionDifference(int number, unsigned selected, unsigned oldSelected)
{
	if(!selected==!oldSelected) return; //2022

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

void MU_SetSelectedUv::setSelectedUv(const int_list &newUv, const int_list &oldUv)
{
	m_newUv = newUv; m_oldUv = oldUv;
}

void MU_Hide::hide(Model *model, int how)
{
	//log_debug("undo hide\n"); //???

	// Invert visible from our list
	for(auto&ea:m_diff) switch(ea.mode)
	{	
	case Model::SelectVertices:
		model->hideVertex(ea.number,how);
		break;
	case Model::SelectTriangles:
		model->hideTriangle(ea.number,how);
		break;
	case Model::SelectJoints:
		model->hideJoint(ea.number,how);
		break;
	case Model::SelectPoints:
		model->hidePoint(ea.number,how);
		break;
	}
}
bool MU_Hide::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_Hide*>(u))
	if(m_hide==undo->m_hide)
	{
		for(auto&ea:undo->m_diff)		
		setHideDifference(ea.mode,ea.number);
		return true;
	}
	return false;
}
unsigned MU_Hide::size()
{
	return sizeof(MU_Hide)+m_diff.size()*sizeof(HideDifferenceT);
}

void MU_InvertNormal::undo(Model *model)
{
	//log_debug("undo invert normal\n"); //???
	
	for(auto it = m_triangles.begin(); it!=m_triangles.end(); it++)
	model->invertNormals(*it);
}

void MU_InvertNormal::redo(Model *model)
{
	for(auto it = m_triangles.rbegin(); it!=m_triangles.rend(); it++)	
	model->invertNormals(*it);	
}

bool MU_InvertNormal::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_InvertNormal*>(u))
	{
		for(auto it = undo->m_triangles.begin(); it!=undo->m_triangles.end(); it++)		
		addTriangle(*it);
		return true;
	}
	return false;
}

unsigned MU_InvertNormal::size()
{
	return sizeof(MU_InvertNormal)+m_triangles.size()*sizeof(int);
}

void MU_InvertNormal::addTriangle(int triangle)
{
	for(auto it = m_triangles.begin(); it!=m_triangles.end(); it++)
	if(triangle==*it)
	{
		/*2020: WRONG RIGHT?
		//m_triangles.remove(triangle);
		m_triangles.erase(it);
		*/
		return;
	}
	m_triangles.push_back(triangle);
}

void MU_MoveUnanimated::undo(Model *model)
{
	//log_debug("undo move vertex\n"); //???
	
	for(auto&ea:m_objects)
	model->movePositionUnanimated(ea,ea.oldx,ea.oldy,ea.oldz);
}

void MU_MoveUnanimated::redo(Model *model)
{
	for(auto&ea:m_objects)
	model->movePositionUnanimated(ea,ea.x,ea.y,ea.z);
}
bool MU_MoveUnanimated::combine(Undo *u)
{
	if(MU_MoveUnanimated*undo=dynamic_cast<MU_MoveUnanimated*>(u))
	{
		for(auto&ea:undo->m_objects)
		addPosition(ea,ea.x,ea.y,ea.z,ea.oldx,ea.oldy,ea.oldz);
		return true;
	}
	return false;
}
unsigned MU_MoveUnanimated::size()
{
	return sizeof(MU_MoveUnanimated)+m_objects.size()*sizeof(MovePrimitiveT);
}
void MU_MoveUnanimated::addPosition(const Model::Position &pos, double x, double y, double z,
		double oldx, double oldy, double oldz)
{
	MovePrimitiveT mv; mv.type = pos.type; mv.index = pos.index; //C++

	// Modify an object we already have
	unsigned index;
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

void MU_SetObjectUnanimated::undo(Model *model)
{
	//log_debug("undo point translation\n"); //???

	_call_setter(model,vold);
}
void MU_SetObjectUnanimated::redo(Model *model)
{
	//log_debug("redo point translation\n"); //???

	_call_setter(model,v);
}
void MU_SetObjectUnanimated::_call_setter(Model *model, double *xyz)
{
	switch(vec)
	{
	case Abs: model->setPositionCoordsUnanimated(pos,xyz); break;
	case Rot: model->setPositionRotationUnanimated(pos,xyz); break;
	case Scale: model->setPositionScaleUnanimated(pos,xyz); break;
	case Rel: model->setBoneJointOffsetUnanimated(pos,xyz); break;
	}
}
bool MU_SetObjectUnanimated::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetObjectUnanimated*>(u))
	if(undo->pos==pos&&undo->vec==vec)
	{
		memcpy(v,undo->v,sizeof(v)); return true;
	}
	return false;
}
unsigned MU_SetObjectUnanimated::size()
{
	return sizeof(MU_SetObjectUnanimated);
}
void MU_SetObjectUnanimated::setXYZ(Model *m, double object[3], const double xyz[3])
{
	auto o = m->getPositionObject(pos);
	assert(o);
	if(o->m_abs==object) vec = Abs;
	else if(o->m_rot==object) vec = Rot;
	else if(o->m_xyz==object) vec = Scale;	
	else if(pos.type==Model::PT_Joint
	&&((Model::Joint*)o)->m_rel==object) vec = Rel;	
	else assert(0);

	memcpy(v,xyz,sizeof(v)); memcpy(vold,object,sizeof(v));	
}

void MU_SetTexture::undo(Model *model)
{
	//log_debug("undo set texture\n"); //???

	for(auto it = m_list.begin(); it!=m_list.end(); it++)	
	model->setGroupTextureId(it->groupNumber,it->oldTexture);
}

void MU_SetTexture::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)	
	model->setGroupTextureId(it->groupNumber,it->newTexture);	
}

bool MU_SetTexture::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTexture*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)		
		setTexture(it->groupNumber,it->newTexture,it->oldTexture);
		return true;
	}
	return false;
}

unsigned MU_SetTexture::size()
{
	return sizeof(MU_SetTexture)+m_list.size()*sizeof(SetTextureT);
}

void MU_SetTexture::setTexture(unsigned groupNumber, int newTexture, int oldTexture)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	if(it->groupNumber==groupNumber)
	{
		it->newTexture = newTexture;
		return;
	}

	SetTextureT st;
	st.groupNumber = groupNumber;
	st.newTexture  = newTexture;
	st.oldTexture  = oldTexture;
	m_list.push_back(st);
}

void MU_AddVertex::undo(Model *model)
{
	//log_debug("undo add vertex\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)
	model->removeVertex(it->index);
}

void MU_AddVertex::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)	
	model->insertVertex(it->index,it->vertex);
}

bool MU_AddVertex::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_AddVertex*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		addVertex(it->index,it->vertex);
		return true;
	}
	return false;
}

void MU_AddVertex::redoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	it->vertex->release();
}

unsigned MU_AddVertex::size()
{
	return sizeof(MU_AddVertex)+m_list.size()*(sizeof(AddVertexT)+sizeof(Model::Vertex));
}

void MU_AddVertex::addVertex(unsigned index,Model::Vertex *vertex)
{
	if(vertex)
	{
		AddVertexT v; v.index  = index; v.vertex = vertex;

		m_list.push_back(v);
	}
}

void MU_AddTriangle::undo(Model *model)
{
	//log_debug("undo add triangle\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)	
	model->removeTriangle(it->index);	
}

void MU_AddTriangle::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->insertTriangle(it->index,it->triangle);
}

bool MU_AddTriangle::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_AddTriangle*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		addTriangle(it->index,it->triangle);
		return true;
	}
	return false;
}

void MU_AddTriangle::redoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	it->triangle->release();
}

unsigned MU_AddTriangle::size()
{
	return sizeof(MU_AddTriangle)+m_list.size()*(sizeof(AddTriangleT)+sizeof(Model::Triangle));
}

void MU_AddTriangle::addTriangle(unsigned index,Model::Triangle *triangle)
{
	if(triangle)
	{
		AddTriangleT v; v.index  = index; v.triangle = triangle;

		m_list.push_back(v);
	}
}

void MU_AddGroup::undo(Model *model)
{
	///log_debug("undo add group\n"); //???
	
	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)	
	model->removeGroup(it->index);
}

void MU_AddGroup::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->insertGroup(it->index,it->group);
}

bool MU_AddGroup::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_AddGroup*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		addGroup(it->index,it->group);
		return true;
	}
	return false;
}

void MU_AddGroup::redoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	it->group->release();
}

unsigned MU_AddGroup::size()
{
	return sizeof(MU_AddGroup)+m_list.size()*(sizeof(AddGroupT)+sizeof(Model::Group));
}

void MU_AddGroup::addGroup(unsigned index,Model::Group *group)
{
	if(group)
	{
		AddGroupT v; v.index  = index; v.group  = group;

		m_list.push_back(v);
	}
}

void MU_AddTexture::undo(Model *model)
{
	//log_debug("undo add texture\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)	
	model->removeTexture(it->index);
}

void MU_AddTexture::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->insertTexture(it->index,it->texture);
}

bool MU_AddTexture::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_AddTexture*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		addTexture(it->index,it->texture);
		return true;
	}
	return false;
}

void MU_AddTexture::redoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	it->texture->release();
}

unsigned MU_AddTexture::size()
{
	return sizeof(MU_AddTexture)+m_list.size()*(sizeof(AddTextureT)+sizeof(Model::Material));
}

void MU_AddTexture::addTexture(unsigned index,Model::Material *texture)
{
	if(texture)
	{
		AddTextureT v; v.index  = index; v.texture  = texture;

		m_list.push_back(v);
	}
}

void MU_SetTextureCoords::undo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->setTextureCoords(it->triangle,it->vertexIndex,it->oldS,it->oldT);
}

void MU_SetTextureCoords::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->setTextureCoords(it->triangle,it->vertexIndex,it->s,it->t);
}

bool MU_SetTextureCoords::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTextureCoords*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		addTextureCoords(it->triangle,it->vertexIndex,it->s,it->t,it->oldS,it->oldT);
		return true;
	}
	return false;
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
	//log_debug("undo add to group\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)
	model->removeTriangleFromGroup(it->groupNum,it->triangleNum);
}

void MU_AddToGroup::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->addTriangleToGroup(it->groupNum,it->triangleNum);
}

bool MU_AddToGroup::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_AddToGroup*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		addToGroup(it->groupNum,it->triangleNum);
		return true;
	}
	return false;
}

unsigned MU_AddToGroup::size()
{
	return sizeof(MU_AddToGroup)+m_list.size()*sizeof(AddToGroupT);
}

void MU_AddToGroup::addToGroup(unsigned groupNum, unsigned triangleNum)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	if(it->triangleNum==triangleNum)
	{
		it->groupNum = groupNum; return;
	}

	AddToGroupT v; v.groupNum = groupNum; v.triangleNum = triangleNum;

	m_list.push_back(v);
}

void MU_RemoveFromGroup::undo(Model *model)
{
	//log_debug("undo remove from group\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)
	model->addTriangleToGroup(it->groupNum,it->triangleNum);
}

void MU_RemoveFromGroup::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)	
	model->removeTriangleFromGroup(it->groupNum,it->triangleNum);
}

bool MU_RemoveFromGroup::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_RemoveFromGroup*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		removeFromGroup(it->groupNum,it->triangleNum);
		return true;
	}
	return false;
}

unsigned MU_RemoveFromGroup::size()
{
	return sizeof(MU_RemoveFromGroup)+m_list.size()*sizeof(RemoveFromGroupT);
}

void MU_RemoveFromGroup::removeFromGroup(unsigned groupNum, unsigned triangleNum)
{
	/*???
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	if(it->triangleNum==triangleNum)
	{
		it->groupNum = groupNum; return;
	}
	*/

	RemoveFromGroupT v; v.groupNum = groupNum; v.triangleNum = triangleNum;

	m_list.push_back(v);
}

void MU_DeleteTriangle::undo(Model *model)
{
	//log_debug("undo delete triangle\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)	
	model->insertTriangle(it->triangleNum,it->triangle);
}

void MU_DeleteTriangle::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->removeTriangle(it->triangleNum);
}

bool MU_DeleteTriangle::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_DeleteTriangle*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		deleteTriangle(it->triangleNum,it->triangle);
		return true;
	}
	return false;
}

void MU_DeleteTriangle::undoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	it->triangle->release();
}

unsigned MU_DeleteTriangle::size()
{
	return sizeof(MU_DeleteTriangle)+m_list.size()*(sizeof(DeleteTriangleT)+sizeof(Model::Triangle));
}

void MU_DeleteTriangle::deleteTriangle(unsigned triangleNum,Model::Triangle *triangle)
{
	DeleteTriangleT v; v.triangleNum = triangleNum; v.triangle = triangle;

	m_list.push_back(v);
}

void MU_DeleteVertex::undo(Model *model)
{
	//log_debug("undo delete vertex\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)	
	model->insertVertex(it->vertexNum,it->vertex);
}

void MU_DeleteVertex::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->removeVertex(it->vertexNum);
}

bool MU_DeleteVertex::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_DeleteVertex*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		deleteVertex(it->vertexNum,it->vertex);
		return true;
	}
	return false;
}

void MU_DeleteVertex::undoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	it->vertex->release();
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
	//log_debug("undo delete group\n"); //???
	
	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)	
	model->insertGroup(it->groupNum,it->group);
}

void MU_DeleteGroup::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->removeGroup(it->groupNum);
}

bool MU_DeleteGroup::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_DeleteGroup*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		deleteGroup(it->groupNum,it->group);
		return true;
	}
	return false;
}

void MU_DeleteGroup::undoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	it->group->release();
}

unsigned MU_DeleteGroup::size()
{
	return sizeof(MU_DeleteGroup)+m_list.size()*(sizeof(DeleteGroupT)+sizeof(Model::Group));
}

void MU_DeleteGroup::deleteGroup(unsigned groupNum,Model::Group *group)
{
	DeleteGroupT v; v.groupNum = groupNum; v.group = group;

	m_list.push_back(v);
}

void MU_DeleteTexture::undo(Model *model)
{
	//log_debug("undo delete texture\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)	
	model->insertTexture(it->textureNum,it->texture);
}

void MU_DeleteTexture::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->removeTexture(it->textureNum);
}

bool MU_DeleteTexture::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_DeleteTexture*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		deleteTexture(it->textureNum,it->texture);
		return true;
	}
	return false;
}

void MU_DeleteTexture::undoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	it->texture->release();
}

unsigned MU_DeleteTexture::size()
{
	return sizeof(MU_DeleteTexture)+m_list.size()*(sizeof(DeleteTextureT)+sizeof(Model::Material));
}

void MU_DeleteTexture::deleteTexture(unsigned textureNum,Model::Material *texture)
{
	DeleteTextureT v; v.textureNum = textureNum; v.texture = texture;

	m_list.push_back(v);
}

void MU_SetLightProperties::undo(Model *model)
{
	//log_debug("undo set light properties\n"); //???

	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		if(it->isSet[0])
		model->setTextureAmbient(it->textureNum,it->oldLight[0]);		
		if(it->isSet[1])
		model->setTextureDiffuse(it->textureNum,it->oldLight[1]);		
		if(it->isSet[2])
		model->setTextureSpecular(it->textureNum,it->oldLight[2]);
		if(it->isSet[3])
		model->setTextureEmissive(it->textureNum,it->oldLight[3]);
	}
}

void MU_SetLightProperties::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	{
		if(it->isSet[0])		
		model->setTextureAmbient(it->textureNum,it->newLight[0]);		
		if(it->isSet[1])		
		model->setTextureDiffuse(it->textureNum,it->newLight[1]);		
		if(it->isSet[2])		
		model->setTextureSpecular(it->textureNum,it->newLight[2]);		
		if(it->isSet[3])		
		model->setTextureEmissive(it->textureNum,it->newLight[3]);
	}
}

bool MU_SetLightProperties::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetLightProperties*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		for(int t = 0; t<LightTypeMax; t++)
		if(it->isSet[t])
		setLightProperties(it->textureNum,(LightTypeE)t,it->newLight[t],it->oldLight[t]);
		return true;
	}
	return false;
}

unsigned MU_SetLightProperties::size()
{
	return sizeof(MU_SetLightProperties)+m_list.size()*sizeof(LightPropertiesT);
}

void MU_SetLightProperties::setLightProperties(unsigned textureNum,LightTypeE type, const float *newLight, const float *oldLight)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
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
	//log_debug("undo set shininess\n"); //???
	
	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)
	model->setTextureShininess(it->textureNum,it->oldValue);
}

void MU_SetShininess::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->setTextureShininess(it->textureNum,it->newValue);
}

bool MU_SetShininess::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetShininess*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		setShininess(it->textureNum,it->newValue,it->oldValue);
		return true;
	}
	return false;
}

unsigned MU_SetShininess::size()
{
	return sizeof(MU_SetShininess)+m_list.size()*sizeof(ShininessT);
}

void MU_SetShininess::setShininess(unsigned textureNum, const float &newValue, const float &oldValue)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	if(it->textureNum==textureNum)
	{
		it->newValue = newValue;
		return;
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
	//log_debug("undo set texture name\n"); //???

	model->setTextureName(m_textureNum,m_oldName.c_str());
}

void MU_SetTextureName::redo(Model *model)
{
	model->setTextureName(m_textureNum,m_newName.c_str());
}

bool MU_SetTextureName::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTextureName*>(u))
	if(undo&&undo->m_textureNum==m_textureNum)
	{
		m_newName = undo->m_newName;
		return true;
	}
	return false;
}

unsigned MU_SetTextureName::size()
{
	return sizeof(MU_SetTextureName);
}

void MU_SetTextureName::setTextureName(unsigned textureNum, const char *newName, const char *oldName)
{
	m_textureNum = textureNum; m_newName = newName; m_oldName = oldName;
}

void MU_SetTriangleVertices::undo(Model *model)
{
	//log_debug("undo set triangle vertices\n"); //???
	
	for(auto it = m_list.rbegin(); it!=m_list.rend(); it++)
	{
		model->setTriangleVertices(it->triangleNum,
				it->oldVertices[0],
				it->oldVertices[1],
				it->oldVertices[2]);
	}
}

void MU_SetTriangleVertices::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	{
		model->setTriangleVertices(it->triangleNum,
				it->newVertices[0],
				it->newVertices[1],
				it->newVertices[2]);
	}
}

bool MU_SetTriangleVertices::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTriangleVertices*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
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
	return false;
}

unsigned MU_SetTriangleVertices::size()
{
	return sizeof(MU_SetTriangleVertices)+m_list.size()*sizeof(TriangleVerticesT);
}

void MU_SetTriangleVertices::setTriangleVertices(unsigned triangleNum,
		unsigned newV1, unsigned newV2, unsigned newV3,
		unsigned oldV1, unsigned oldV2, unsigned oldV3)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)	
	if(it->triangleNum==triangleNum)
	{
		it->newVertices[0] = newV1;
		it->newVertices[1] = newV2;
		it->newVertices[2] = newV3;
		return;
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
	//NOTE: MU_SubdivideTriangle works with this
	//to do the actual undo work
	//log_debug("undo subdivide selected\n");
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
	//log_debug("undo subdivide\n"); //???

	for(auto it=m_list.rbegin(),itt=m_list.rend();it<itt;it++)
	{
		model->subdivideSelectedTriangles_undo(it->a,it->b,it->c,it->d);
	}
	for(auto it=m_vlist.rbegin(),itt=m_vlist.rend();it<itt;it++)
	{
		model->deleteVertex(*it);
	}
}
bool MU_SubdivideTriangle::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SubdivideTriangle*>(u))
	{
		for(auto&ea:undo->m_list)
		subdivide(ea.a,ea.b,ea.c,ea.d);
		undo->m_list.clear();

		for(unsigned i:undo->m_vlist)
		addVertex(i);
		undo->m_vlist.clear();

		return true;
	}
	return false;
}
unsigned MU_SubdivideTriangle::size()
{
	return sizeof(MU_SubdivideTriangle)
	+m_list.size()*sizeof(SubdivideTriangleT)+m_vlist.size()*sizeof(int);
}
void MU_SubdivideTriangle::subdivide(unsigned a, unsigned b, unsigned c, unsigned d)
{
	m_list.push_back({a,b,c,d});
}
void MU_SubdivideTriangle::addVertex(unsigned v)
{
	m_vlist.push_back(v);
}

void MU_ChangeAnimState::undo(Model *model)
{
	model->setCurrentAnimation(m_old);
}
void MU_ChangeAnimState::redo(Model *model)
{
	model->setCurrentAnimation(m_new);
}
bool MU_ChangeAnimState::combine(Undo *u)
{
	return false;
}
unsigned MU_ChangeAnimState::size()
{
	return sizeof(MU_ChangeAnimState);
}

void MU_SetAnimName::undo(Model *model)
{
	model->setAnimName(m_animNum,m_oldName.c_str());
}

void MU_SetAnimName::redo(Model *model)
{
	model->setAnimName(m_animNum,m_newName.c_str());
}

bool MU_SetAnimName::combine(Undo *u)
{
	return false;
}

unsigned MU_SetAnimName::size()
{
	return sizeof(MU_SetAnimName);
}

void MU_SetAnimName::setName(unsigned animNum, const char *newName, const char *oldName)
{
	m_animNum	 = animNum;
	m_newName	 = newName;
	m_oldName	 = oldName;
}

void MU_SetAnimFrameCount::undo(Model *model)
{
	model->setAnimFrameCount(m_animNum,m_oldCount,m_where,&m_vertices);

	if(!m_timetable.empty())
	{
		Model::Animation *ab = model->m_anims[m_animNum];

		std::copy(m_timetable.begin(),m_timetable.end(),ab->m_timetable2020.begin()+m_where);
	}
}
void MU_SetAnimFrameCount::redo(Model *model)
{
	model->setAnimFrameCount(m_animNum,m_newCount,m_where,&m_vertices);
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
void MU_SetAnimFrameCount::setAnimFrameCount(unsigned animNum, unsigned newCount, unsigned oldCount, unsigned where)
{
	m_animNum	 = animNum;
	m_newCount	= newCount;
	m_oldCount	= oldCount;
	m_where      = where;
}

void MU_SetAnimFPS::undo(Model *model)
{
	model->setAnimFPS(m_animNum,m_oldFPS);
}

void MU_SetAnimFPS::redo(Model *model)
{
	model->setAnimFPS(m_animNum,m_newFPS);
}

bool MU_SetAnimFPS::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetAnimFPS*>(u))
	if(undo->m_animNum==m_animNum)
	{
		m_newFPS = undo->m_newFPS; return true;
	}
	return false;
}

unsigned MU_SetAnimFPS::size()
{
	return sizeof(MU_SetAnimFPS);
}

void MU_SetAnimFPS::setFPS(unsigned animNum, double newFps, double oldFps)
{
	m_animNum = animNum; m_newFPS = newFps; m_oldFPS = oldFps;
}

void MU_SetAnimWrap::undo(Model *model)
{
	model->setAnimWrap(m_animNum,m_oldLoop);
}
void MU_SetAnimWrap::redo(Model *model)
{
	model->setAnimWrap(m_animNum,m_newLoop);
}
bool MU_SetAnimWrap::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetAnimWrap*>(u))
	if(undo->m_animNum==m_animNum)
	{
		m_newLoop = undo->m_newLoop; return true;
	}
	return false;
}
void MU_SetAnimWrap::setAnimWrap(unsigned animNum,bool newLoop,bool oldLoop)
{
	m_animNum	 = animNum;
	m_newLoop	 = newLoop;
	m_oldLoop	 = oldLoop;
}

void MU_SetAnimTime::undo(Model *model)
{
	if(m_animFrame==INT_MAX)
	model->setAnimTimeFrame(m_animNum,m_oldTime);
	else
	model->setAnimFrameTime(m_animNum,m_animFrame,m_oldTime);
}
void MU_SetAnimTime::redo(Model *model)
{
	if(m_animFrame==INT_MAX)
	model->setAnimTimeFrame(m_animNum,m_newTime);
	else
	model->setAnimFrameTime(m_animNum,m_animFrame,m_newTime);
}
bool MU_SetAnimTime::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetAnimTime*>(u))
	if(undo->m_animNum==m_animNum&&m_animFrame==undo->m_animFrame)
	{
		m_newTime = undo->m_newTime; return true;
	}
	return false;
}
void MU_SetAnimTime::setAnimFrameTime(unsigned animNum, unsigned frame, double newTime, double oldTime)
{
	m_animNum	 = animNum; m_animFrame = frame;
	m_newTime	 = newTime;
	m_oldTime	 = oldTime;
}

void MU_SetObjectKeyframe::undo(Model *model)
{
	for(auto&ea:m_keyframes) if(ea.isNew)
	{
		//log_debug("undoing new keyframe\n"); //???
			
		model->removeKeyframe(m_anim,m_frame,ea,m_isRotation,true);			
	}
	else
	{
		//log_debug("undoing existing keyframe\n"); //???
			
		model->setKeyframe(m_anim,m_frame,ea,m_isRotation,ea.oldx,ea.oldy,ea.oldz,ea.olde);			
	}

	if(m_anim==model->getCurrentAnimation()) 
	model->setCurrentAnimationFrame(m_frame);
}

void MU_SetObjectKeyframe::redo(Model *model)
{
	for(auto&ea:m_keyframes)
	model->setKeyframe(m_anim,m_frame,ea,m_isRotation,ea.x,ea.y,ea.z,ea.e);

	if(m_anim==model->getCurrentAnimation()) 
	model->setCurrentAnimationFrame(m_frame);
}
bool MU_SetObjectKeyframe::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetObjectKeyframe*>(u))
	if(undo->m_anim==m_anim&&undo->m_frame==m_frame&&m_isRotation==undo->m_isRotation)
	{
		for(auto&ea:undo->m_keyframes)
		addKeyframe(ea,ea.isNew,ea.x,ea.y,ea.z,ea.e,ea.oldx,ea.oldy,ea.oldz,ea.olde);
		return true;
	}
	return false;
}
unsigned MU_SetObjectKeyframe::size()
{
	return sizeof(MU_SetObjectKeyframe)+m_keyframes.size()*sizeof(SetKeyFrameT);
}
void MU_SetObjectKeyframe::addKeyframe(Model::Position j,bool isNew, 
	double x, double y, double z, Model::Interpolate2020E e,
		double oldx, double oldy, double oldz, Model::Interpolate2020E olde)
{
	unsigned index = 0;
	//SetKeyFrameT mv = j;
	SetKeyFrameT mv; mv = j; //C++

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

void MU_DeleteObjectKeyframe::undo(Model *model)
{
	for(auto*ea:m_list) model->insertKeyframe(m_anim,ea);

	if(m_anim==model->getCurrentAnimation()) 
	model->setCurrentAnimationFrame(m_frame);
}
void MU_DeleteObjectKeyframe::redo(Model *model)
{
	for(auto*ea:m_list) model->removeKeyframe(m_anim,ea);

	if(m_anim==model->getCurrentAnimation()) 
	model->setCurrentAnimationFrame(m_frame);
}
bool MU_DeleteObjectKeyframe::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_DeleteObjectKeyframe*>(u))
	if(m_anim==undo->m_anim&&m_frame==undo->m_frame)
	{
		for(auto*ea:undo->m_list) 
		deleteKeyframe(ea); return true;
	}
	return false;
}
void MU_DeleteObjectKeyframe::undoRelease()
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	(*it)->release();
}
unsigned MU_DeleteObjectKeyframe::size()
{
	return sizeof(MU_DeleteObjectKeyframe)+m_list.size()*sizeof(Model::Keyframe);
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
	m_newName = newName; m_oldName = oldName;
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
	m_projection = projection; m_newType = newType; m_oldType = oldType;
}

void MU_MoveFrameVertex::undo(Model *model)
{
	// Modify a vertex we already have
	for(auto&ea:m_vertices)
	model->setQuickFrameAnimVertexCoords(m_anim,m_frame,ea.number,ea.oldx,ea.oldy,ea.oldz,ea.olde);	
	model->setQuickFrameAnimVertexCoords_final(); //2022

	if(m_anim==model->getCurrentAnimation()) 
	model->setCurrentAnimationFrame(m_frame);
}
void MU_MoveFrameVertex::redo(Model *model)
{
	// Modify a vertex we already have
	for(auto&ea:m_vertices)
	model->setQuickFrameAnimVertexCoords(m_anim,m_frame,ea.number,ea.x,ea.y,ea.z,ea.e);
	model->setQuickFrameAnimVertexCoords_final(); //2022

	if(m_anim==model->getCurrentAnimation()) 
	model->setCurrentAnimationFrame(m_frame);
}
bool MU_MoveFrameVertex::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_MoveFrameVertex*>(u))
	if(undo->m_frame==m_frame&&undo->m_anim==m_anim)	
	{
		for(auto&ea:undo->m_vertices) 
		addVertex(ea); return true;
	}
	return false;
}
unsigned MU_MoveFrameVertex::size()
{
	return sizeof(MU_MoveFrameVertex)+m_vertices.size()*sizeof(MoveFrameVertexT);
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

void MU_SetPositionInfluence::undo(Model *model)
{
	if(m_isAdd)
	{
		model->removeInfluence(m_pos,m_index);
	}
	else model->insertInfluence(m_pos,m_index,m_influence);	
}

void MU_SetPositionInfluence::redo(Model *model)
{
	if(m_isAdd)
	{
		model->insertInfluence(m_pos,m_index,m_influence);
	}
	else model->removeInfluence(m_pos,m_index);
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
	model->setPositionInfluence(m_pos,m_oldInf.m_boneId,m_oldInf.m_type,m_oldInf.m_weight);
}

void MU_UpdatePositionInfluence::redo(Model *model)
{
	model->setPositionInfluence(m_pos,m_newInf.m_boneId,m_newInf.m_type,m_newInf.m_weight);
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
	m_pos = pos; m_newInf = newInf; m_oldInf = oldInf;
}

/*UNUSED
void MU_SetVertexBoneJoint::undo(Model *model)
{
	//log_debug("undo set vertex bone joint\n");
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
	//log_debug("undo set point bone joint\n");
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
	//log_debug("undo set triangle projection\n"); //???
	
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->setTriangleProjection(it->triangle,it->oldProj);
}

void MU_SetTriangleProjection::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->setTriangleProjection(it->triangle,it->newProj);
}

bool MU_SetTriangleProjection::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTriangleProjection*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		setTriangleProjection(it->triangle,it->newProj,it->oldProj);
		return true;
	}
	return false;
}

unsigned MU_SetTriangleProjection::size()
{
	return sizeof(MU_SetTriangleProjection)+m_list.size()*sizeof(SetProjectionT);
}

void MU_SetTriangleProjection::setTriangleProjection(const unsigned &triangle,
		const int &newProj, const int &oldProj)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	if(it->triangle==triangle)
	{
		it->newProj = newProj; return;
	}

	SetProjectionT sj;
	sj.triangle = triangle;
	sj.newProj  = newProj;
	sj.oldProj  = oldProj;
	m_list.push_back(sj);
}

void MU_AddAnimation::undo(Model *model)
{
	model->removeAnimation(m_anim);
}
void MU_AddAnimation::redo(Model *model)
{
	model->insertAnimation(m_anim,m_animp);
}
bool MU_AddAnimation::combine(Undo *u)
{
	return false;
}
void MU_AddAnimation::redoRelease()
{
	//log_debug("releasing animation in redo\n"); //???
	
	if(m_animp) m_animp->release();
}
unsigned MU_AddAnimation::size()
{
		//REMINDER: Consider MU_DeleteAnimation::size.

	return sizeof(*this); //2021

		/*2021: sizeof(void*) ???
//	unsigned frameAnimSize = (m_frameAnim)? sizeof(m_frameAnim): 0;
//	unsigned frame = 0;
//	unsigned skelAnimSize = (m_skelAnim)? sizeof(m_skelAnim): 0;*/
	
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

//	return sizeof(MU_AddAnimation)+frameAnimSize+skelAnimSize;
}

void MU_DeleteAnimation::undo(Model *model)
{
	model->insertAnimation(m_anim,m_animp);

	model->insertFrameAnimData(m_animp->m_frame0,m_animp->_frame_count(),&m_vertices,m_animp);
}
void MU_DeleteAnimation::redo(Model *model)
{
	model->removeFrameAnimData(m_animp->m_frame0,m_animp->_frame_count(),nullptr);

	model->removeAnimation(m_anim);
}
bool MU_DeleteAnimation::combine(Undo *u)
{
	return false;
}
void MU_DeleteAnimation::undoRelease()
{
	//log_debug("releasing animation in undo\n");

	if(m_animp) m_animp->release();
	
	for(auto*ea:m_vertices) ea->release();
}
unsigned MU_DeleteAnimation::size()
{
		//REMINDER: Consider MU_AddAnimation::size.

		/*2021: sizeof(void*) ???
	unsigned frameAnimSize = (m_frameAnim)? sizeof(m_frameAnim): 0;*/
	unsigned sz = sizeof(*this);
	//unsigned frame = 0;

	//Model::Animation *ab = m_skelAnim;
	Model::Animation *ab = m_animp;
	//if(m_frameAnim)
	if(ab->_type&2)
	{
		//ab = m_frameAnim;

		unsigned count = ab->_frame_count();
		/*
		for(frame = 0; frame<count; frame++)
		{
			Model::FrameAnimVertexList *vertexList = &m_frameAnim->m_frameData[frame]->m_frameVertices;
			sz += sizeof(Model::FrameAnimVertexList)+vertexList->size()*sizeof(Model::FrameAnimVertex);
			Model::FrameAnimPointList *pointList = &m_frameAnim->m_frameData[frame]->m_framePoints;
			sz += sizeof(Model::FrameAnimPointList)+pointList->size()*sizeof(Model::FrameAnimPoint);
		}*/
		sz+=count*m_vertices.size()*sizeof(Model::FrameAnimVertex);
	}

		/*2021: sizeof(void*) ???
	unsigned skelAnimSize = (m_skelAnim)? sizeof(m_skelAnim): 0;*/

	if(ab) //m_skelAnim
	{
		unsigned objectCount = ab->m_keyframes.size();
		for(auto&list:ab->m_keyframes)
		{	
			sz+=sizeof(list)+list.second.size()*sizeof(*list.second.data());
		}
		//skelAnimSize += jointCount *sizeof(Model::JointKeyframeList); //???
		sz+=objectCount*sizeof(Model::KeyframeList);
	}

	return sz; //return sizeof(MU_DeleteAnimation)+frameAnimSize+skelAnimSize;
}

void MU_VoidFrameAnimation::undo(Model *model)
{
	m_animp->m_frame0 = m_frame0;
	model->insertFrameAnimData(m_animp->m_frame0,m_animp->_frame_count(),&m_vertices,m_animp);
}
void MU_VoidFrameAnimation::redo(Model *model)
{
	m_animp->m_frame0 = ~0;
	model->removeFrameAnimData(m_animp->m_frame0,m_animp->_frame_count(),nullptr);
}
bool MU_VoidFrameAnimation::combine(Undo *u)
{
	return false;
}
void MU_VoidFrameAnimation::undoRelease()
{
	//log_debug("releasing animation in undo\n");

	for(auto*ea:m_vertices) ea->release();
}
unsigned MU_VoidFrameAnimation::size()
{
	unsigned sz = sizeof(*this);
	Model::Animation *ab = m_animp;
	unsigned count = ab->_frame_count();		
	sz+=count*m_vertices.size()*sizeof(Model::FrameAnimVertex);
	return sz;
}

void MU_SetJointParent::undo(Model *model)
{
	model->parentBoneJoint(m_joint,m_old,(Matrix)m_undo); //copy
}
void MU_SetJointParent::redo(Model *model)
{
	model->parentBoneJoint(m_joint,m_new,(Matrix)m_redo); //copy
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
	m_jointNum = jointNum; m_joint = joint;
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
	m_pointNum = pointNum; m_point = point;
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
	m_projNum = projNum; m_proj = proj;
}

void MU_DeleteBoneJoint::undo(Model *model)
{
	//log_debug("undo delete joint\n"); //???

	model->insertBoneJoint(m_jointNum,m_joint);
}

void MU_DeleteBoneJoint::redo(Model *model)
{
	//log_debug("redo delete joint\n"); //???

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
	m_jointNum = jointNum; m_joint = joint;
}

void MU_DeletePoint::undo(Model *model)
{
	//log_debug("undo delete point\n"); //???

	model->insertPoint(m_pointNum,m_point);
}

void MU_DeletePoint::redo(Model *model)
{
	//log_debug("redo delete point\n"); //???

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
	m_pointNum = pointNum; m_point = point;
}

void MU_DeleteProjection::undo(Model *model)
{
	//log_debug("undo delete proj\n"); //???

	model->insertProjection(m_projNum,m_proj);
}

void MU_DeleteProjection::redo(Model *model)
{
	//log_debug("redo delete proj\n"); //???

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
	m_projNum = projNum; m_proj = proj;
}

void MU_SetGroupSmooth::undo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->setGroupSmooth(it->group,it->oldSmooth);
}

void MU_SetGroupSmooth::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->setGroupSmooth(it->group,it->newSmooth);
}

bool MU_SetGroupSmooth::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetGroupSmooth*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		setGroupSmooth(it->group,it->newSmooth,it->oldSmooth);
		return true;
	}
	return false;
}

unsigned MU_SetGroupSmooth::size()
{
	return sizeof(MU_SetGroupSmooth)+m_list.size()*sizeof(SetSmoothT);
}

void MU_SetGroupSmooth::setGroupSmooth(const unsigned &group,
		const uint8_t &newSmooth, const uint8_t &oldSmooth)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	if(it->group==group)
	{
		it->newSmooth = newSmooth;
		return;
	}

	SetSmoothT ss;
	ss.group		= group;
	ss.newSmooth  = newSmooth;
	ss.oldSmooth  = oldSmooth;
	m_list.push_back(ss);
}

void MU_SetGroupAngle::undo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)	
	model->setGroupAngle(it->group,it->oldAngle);
}

void MU_SetGroupAngle::redo(Model *model)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	model->setGroupAngle(it->group,it->newAngle);	
}

bool MU_SetGroupAngle::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetGroupAngle*>(u))
	{
		for(auto it = undo->m_list.begin(); it!=undo->m_list.end(); it++)
		setGroupAngle(it->group,it->newAngle,it->oldAngle);
		return true;
	}
	return false;
}

unsigned MU_SetGroupAngle::size()
{
	return sizeof(MU_SetGroupAngle)+m_list.size()*sizeof(SetAngleT);
}

void MU_SetGroupAngle::setGroupAngle(const unsigned &group,
		const uint8_t &newAngle, const uint8_t &oldAngle)
{
	for(auto it = m_list.begin(); it!=m_list.end(); it++)
	if(it->group==group)
	{
		it->newAngle = newAngle;
		return;
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
	if(auto*undo=dynamic_cast<MU_SetGroupName*>(u))
	if(undo->m_groupNum==m_groupNum)
	{
		m_newName = undo->m_newName; return true;
	}
	return false;
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
	model->_moveAnimation(m_newIndex,m_oldIndex,-m_typeDiff);
}
void MU_MoveAnimation::redo(Model *model)
{
	model->_moveAnimation(m_oldIndex,m_newIndex,+m_typeDiff);
}
bool MU_MoveAnimation::combine(Undo *u)
{
	return false;
}
unsigned MU_MoveAnimation::size()
{
	return sizeof(MU_MoveAnimation);
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

void MU_SetMaterialBool::undo(Model *model)
{
	switch(m_how)
	{
	case 'S': model->setTextureSClamp(m_material,m_old); break;
	case 'T': model->setTextureTClamp(m_material,m_old); break;
	case 'A': model->setTextureBlendMode(m_material,m_old); break;
	default: assert(0);
	}
}
void MU_SetMaterialBool::redo(Model *model)
{
	switch(m_how)
	{
	case 'S': model->setTextureSClamp(m_material,m_new); break;
	case 'T': model->setTextureTClamp(m_material,m_new); break;
	case 'A': model->setTextureBlendMode(m_material,m_new); break;
	default: assert(0);
	}
}
bool MU_SetMaterialBool::combine(Undo *u)
{
	return false;
}
unsigned MU_SetMaterialBool::size()
{
	return sizeof(MU_SetMaterialBool);
}
MU_SetMaterialBool::MU_SetMaterialBool(unsigned material, char how, bool newBool, bool oldBool)
{
	m_material = material;
	m_how = how;
	m_new = newBool;
	m_old = oldBool;
}

void MU_SetMaterialTexture::undo(Model *model)
{
	if(m_oldTexture)
	{
		model->setMaterialTexture(m_material,m_oldTexture);
	}
	else model->removeMaterialTexture(m_material);
}

void MU_SetMaterialTexture::redo(Model *model)
{
	if(m_newTexture)
	{
		model->setMaterialTexture(m_material,m_newTexture);
	}
	else model->removeMaterialTexture(m_material);
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
	m_material = material; m_newTexture = newTexture; m_oldTexture = oldTexture;
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
	m_key = key; m_value = value;
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
	m_key = key; m_newValue = newValue; m_oldValue = oldValue;
}

void MU_ClearMetaData::undo(Model *model)
{
	for(auto&ea:m_list)
	model->addMetaData(ea.key.c_str(),ea.value.c_str());
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
		s+=it->key.size()+it->value.size();
	}
	return s;
}

void MU_ClearMetaData::clearMetaData(const Model::MetaDataList &list)
{
	m_list = list;
}

unsigned MU_InterpolateSelected::size()
{
	return sizeof(*this)+m_eold.size()*sizeof(Model::Interpolate2020E);
}
void MU_InterpolateSelected::_do(Model *model, bool undoing)
{	
	model->m_changeBits|=Model::MoveGeometry;

	//NOTE: This became too complicated for keyframes so 
	//it's just an optimization for vertex animation data
	auto fa = model->m_anims[m_anim];
	auto fp = fa->m_frame0+m_frame;
	auto it = m_eold.begin();
	auto e = m_e; //optimizing
	int v = -1;
	for(auto*ea:model->m_vertices) if(v++,ea->m_selected)
	{
		auto vf = ea->m_frames[fp];
		auto &cmp = vf->m_interp2020;

		if(cmp!=e)
		{
			if(!undoing)
			{
				//HACK: Maybe this value should already be stored.
				if(*it<=Model::InterpolateCopy)
				{
					/*I think this can be better now.
					assert(m_anim==model->getCurrentAnimation()); //2021
					model->validateAnim();
					memcpy(vf->m_coord,ea->m_kfCoord,sizeof(ea->m_kfCoord));*/
					model->interpKeyframe(m_anim,m_frame,v,vf->m_coord);
				}

				cmp = e;
			}
			else cmp = *it; it++;
		}
	}

	if(m_anim==model->getCurrentAnimation()) 
	model->setCurrentAnimationFrame(m_frame);
}

size_t MU_RemapTrianglesIndices::size()
{
	return map.size()*sizeof(int)+sizeof(*this);
}
void MU_RemapTrianglesIndices::redo(Model *model)
{
	swap(); model->remapTrianglesIndices(map);
}
void MU_RemapTrianglesIndices::undo(Model *model)
{
	swap(); model->remapTrianglesIndices(map);
}
void MU_RemapTrianglesIndices::swap()
{
	size_t i = 0, sz = map.size();

	int_list swap(sz);
	
	for(int j:map) swap[j] = i++; map.swap(swap);	
}