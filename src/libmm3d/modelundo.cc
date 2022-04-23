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
	if(auto*undo=dynamic_cast<MU_TranslateSelected*>(u))
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
	if(auto*undo=dynamic_cast<MU_RotateSelected*>(u))
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
	
	for(int i:m_triangles) model->invertNormals(i); //SAME
}
void MU_InvertNormal::redo(Model *model)
{
	for(int i:m_triangles) model->invertNormals(i); //SAME
}
bool MU_InvertNormal::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_InvertNormal*>(u))
	{
		for(int i:undo->m_triangles) addTriangle(i);

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

	for(auto&ea:m_list)		
	model->setGroupTextureId(ea.groupNumber,ea.oldTexture);
}
void MU_SetTexture::redo(Model *model)
{
	for(auto&ea:m_list)		
	model->setGroupTextureId(ea.groupNumber,ea.newTexture);	
}
bool MU_SetTexture::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTexture*>(u))
	{
		for(auto&ea:undo->m_list)		
		setTexture(ea.groupNumber,ea.newTexture,ea.oldTexture);
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
	for(auto&ea:m_list)	
	if(ea.groupNumber==groupNumber)
	{
		ea.newTexture = newTexture; return;
	}

	SetTextureT st;
	st.groupNumber = groupNumber;
	st.newTexture  = newTexture;
	st.oldTexture  = oldTexture;
	m_list.push_back(st);
}

void MU_SetTextureCoords::undo(Model *model)
{
	for(auto&ea:m_list)	
	model->setTextureCoords(ea.triangle,ea.vertexIndex,ea.oldS,ea.oldT);
}
void MU_SetTextureCoords::redo(Model *model)
{
	for(auto&ea:m_list)	
	model->setTextureCoords(ea.triangle,ea.vertexIndex,ea.s,ea.t);
}
bool MU_SetTextureCoords::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTextureCoords*>(u))
	{
		for(auto&ea:undo->m_list)
		addTextureCoords(ea.triangle,ea.vertexIndex,ea.s,ea.t,ea.oldS,ea.oldT);
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

	for(auto&ea:m_list)	
	model->removeTriangleFromGroup(ea.groupNum,ea.triangleNum);
}
void MU_AddToGroup::redo(Model *model)
{
	for(auto&ea:m_list)	
	model->addTriangleToGroup(ea.groupNum,ea.triangleNum);
}
bool MU_AddToGroup::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_AddToGroup*>(u))
	{
		for(auto&ea:undo->m_list)
		addToGroup(ea.groupNum,ea.triangleNum);
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
	for(auto&ea:m_list)	
	if(ea.triangleNum==triangleNum)
	{
		ea.groupNum = groupNum; return;
	}

	AddToGroupT v; v.groupNum = groupNum; v.triangleNum = triangleNum;

	m_list.push_back(v);
}

void MU_RemoveFromGroup::undo(Model *model)
{
	//log_debug("undo remove from group\n"); //???

	for(auto&ea:m_list)	
	model->addTriangleToGroup(ea.groupNum,ea.triangleNum);
}
void MU_RemoveFromGroup::redo(Model *model)
{
	for(auto&ea:m_list)	
	model->removeTriangleFromGroup(ea.groupNum,ea.triangleNum);
}
bool MU_RemoveFromGroup::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_RemoveFromGroup*>(u))
	{
		for(auto&ea:undo->m_list)
		removeFromGroup(ea.groupNum,ea.triangleNum);
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
	for(auto&ea:m_list) if(ea.triangleNum==triangleNum)
	{
		ea.groupNum = groupNum; return;
	}*/

	m_list.push_back({groupNum,triangleNum});
}

void MU_SetLightProperties::undo(Model *model)
{
	//log_debug("undo set light properties\n"); //???

	for(auto&ea:m_list)	
	{
		if(ea.isSet[0]) model->setTextureAmbient(ea.textureNum,ea.oldLight[0]);		
		if(ea.isSet[1]) model->setTextureDiffuse(ea.textureNum,ea.oldLight[1]);		
		if(ea.isSet[2]) model->setTextureSpecular(ea.textureNum,ea.oldLight[2]);
		if(ea.isSet[3]) model->setTextureEmissive(ea.textureNum,ea.oldLight[3]);
	}
}
void MU_SetLightProperties::redo(Model *model)
{
	for(auto&ea:m_list)
	{
		if(ea.isSet[0]) model->setTextureAmbient(ea.textureNum,ea.newLight[0]);		
		if(ea.isSet[1]) model->setTextureDiffuse(ea.textureNum,ea.newLight[1]);		
		if(ea.isSet[2]) model->setTextureSpecular(ea.textureNum,ea.newLight[2]);		
		if(ea.isSet[3]) model->setTextureEmissive(ea.textureNum,ea.newLight[3]);
	}
}
bool MU_SetLightProperties::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetLightProperties*>(u))
	{
		for(auto&ea:undo->m_list)
		for(int t=0;t<LightTypeMax;t++)
		if(ea.isSet[t])
		setLightProperties(ea.textureNum,(LightTypeE)t,ea.newLight[t],ea.oldLight[t]);
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
	for(auto&ea:m_list)
	if(ea.textureNum==textureNum)
	{
		// Set old light if this is the first time we set this type
		if(!ea.isSet[type]) for(int n=0;n<4;n++)
		{
			ea.oldLight[type][n] = oldLight[n];
		}

		// Set new light for this type
		ea.isSet[type] = true;
		for(int n=0;n<4;n++)
		{
			ea.newLight[type][n] = newLight[n];
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
	
	for(auto&ea:m_list)
	model->setTextureShininess(ea.textureNum,ea.oldValue);
}
void MU_SetShininess::redo(Model *model)
{
	for(auto&ea:m_list)
	model->setTextureShininess(ea.textureNum,ea.newValue);
}
bool MU_SetShininess::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetShininess*>(u))
	{
		for(auto&ea:undo->m_list)
		setShininess(ea.textureNum,ea.newValue,ea.oldValue);
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
	for(auto&ea:m_list)
	if(ea.textureNum==textureNum)
	{
		ea.newValue = newValue; return;
	}

	// Add new ShininessT to list
	ShininessT shine;

	shine.textureNum = textureNum;
	shine.oldValue = oldValue;
	shine.newValue = newValue;

	m_list.push_back(shine);
}

void MU_SetTriangleVertices::undo(Model *model)
{
	//log_debug("undo set triangle vertices\n"); //???
	
	for(auto&ea:m_list)	
	model->setTriangleVertices(ea.triangleNum,
	ea.oldVertices[0],ea.oldVertices[1],ea.oldVertices[2]);	
}
void MU_SetTriangleVertices::redo(Model *model)
{
	for(auto&ea:m_list)
	model->setTriangleVertices(ea.triangleNum,
	ea.newVertices[0],ea.newVertices[1],ea.newVertices[2]);
}
bool MU_SetTriangleVertices::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTriangleVertices*>(u))
	{
		for(auto&ea:undo->m_list)
		setTriangleVertices(ea.triangleNum,
		ea.newVertices[0],ea.newVertices[1],ea.newVertices[2],
		ea.oldVertices[0],ea.oldVertices[1],ea.oldVertices[2]);
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
	for(auto&ea:m_list)
	if(ea.triangleNum==triangleNum)
	{
		ea.newVertices[0] = newV1;
		ea.newVertices[1] = newV2;
		ea.newVertices[2] = newV3; return;
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

	for(auto rit=m_list.rbegin(),ritt=m_list.rend();rit<ritt;rit++)
	{
		model->subdivideSelectedTriangles_undo(rit->a,rit->b,rit->c,rit->d);
	}
	for(auto rit=m_vlist.rbegin(),ritt=m_vlist.rend();rit<ritt;rit++)
	{
		model->deleteVertex(*rit);
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

void MU_ChangeSkeletalMode::undo(Model *model)
{
	model->setSkeletalModeEnabled(!m_how);
}
void MU_ChangeSkeletalMode::redo(Model *model)
{
	model->setSkeletalModeEnabled(m_how);
}
bool MU_ChangeSkeletalMode::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_ChangeSkeletalMode*>(u))
	{
		m_how = undo->m_how; return true;
	}
	return false;
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
	m_animNum = animNum; m_newCount = newCount; m_oldCount = oldCount; m_where = where;
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
	model->setAnimWrap(m_animNum,m_old);
}
void MU_SetAnimWrap::redo(Model *model)
{
	model->setAnimWrap(m_animNum,m_new);
}
bool MU_SetAnimWrap::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetAnimWrap*>(u))
	if(undo->m_animNum==m_animNum)
	{
		m_new = undo->m_new; return true;
	}
	return false;
}
void MU_SetAnimWrap::setAnimWrap(unsigned animNum, bool newWrap, bool oldWrap)
{
	m_animNum = animNum; m_new = newWrap; m_old = oldWrap;
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
	m_animNum = animNum; m_animFrame = frame; m_newTime = newTime; m_oldTime = oldTime;
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
void MU_SetObjectKeyframe::addKeyframe(Model::Position j, bool isNew, 
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
	mv.isNew = isNew;
	mv.x = x;
	mv.y = y;
	mv.z = z;
	mv.e = e;
	mv.oldx = oldx;
	mv.oldy = oldy;
	mv.oldz = oldz;
	mv.olde = olde;
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
	for(auto*ea:m_list) ea->release();
}
unsigned MU_DeleteObjectKeyframe::size()
{
	return sizeof(MU_DeleteObjectKeyframe)+m_list.size()*sizeof(Model::Keyframe);
}
void MU_DeleteObjectKeyframe::deleteKeyframe(Model::Keyframe *keyframe)
{
	m_list.push_back(keyframe);
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
		for(auto&ea:undo->m_vertices) addVertex(ea); return true;
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
	else // Not found, add new vertex information
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
	m_isAdd = isAdd; m_index = index; m_pos = pos; m_influence = influence;
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

void MU_SetTriangleProjection::undo(Model *model)
{
	//log_debug("undo set triangle projection\n"); //???
	
	for(auto&ea:m_list)
	model->setTriangleProjection(ea.triangle,ea.oldProj);
}
void MU_SetTriangleProjection::redo(Model *model)
{
	for(auto&ea:m_list)
	model->setTriangleProjection(ea.triangle,ea.newProj);
}
bool MU_SetTriangleProjection::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetTriangleProjection*>(u))
	{
		for(auto&ea:undo->m_list)
		setTriangleProjection(ea.triangle,ea.newProj,ea.oldProj);
		return true;
	}
	return false;
}
unsigned MU_SetTriangleProjection::size()
{
	return sizeof(MU_SetTriangleProjection)+m_list.size()*sizeof(SetProjectionT);
}
void MU_SetTriangleProjection::setTriangleProjection(unsigned triangle,
		int newProj, int oldProj)
{
	for(auto&ea:m_list) if(ea.triangle==triangle)
	{
		ea.newProj = newProj; return;
	}

	SetProjectionT sj;
	sj.triangle = triangle;
	sj.newProj  = newProj;
	sj.oldProj  = oldProj; m_list.push_back(sj);
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
void MU_SetProjectionRange::setProjectionRange(unsigned proj,
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

void MU_SetGroupSmooth::undo(Model *model)
{
	for(auto&ea:m_list) model->setGroupSmooth(ea.group,ea.oldSmooth);
}
void MU_SetGroupSmooth::redo(Model *model)
{
	for(auto&ea:m_list) model->setGroupSmooth(ea.group,ea.newSmooth);
}
bool MU_SetGroupSmooth::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetGroupSmooth*>(u))
	{
		for(auto&ea:undo->m_list)
		setGroupSmooth(ea.group,ea.newSmooth,ea.oldSmooth);
		return true;
	}
	return false;
}
unsigned MU_SetGroupSmooth::size()
{
	return sizeof(MU_SetGroupSmooth)+m_list.size()*sizeof(SetSmoothT);
}
void MU_SetGroupSmooth::setGroupSmooth(unsigned group,
		const uint8_t &newSmooth, const uint8_t &oldSmooth)
{
	for(auto&ea:m_list) if(ea.group==group)
	{
		ea.newSmooth = newSmooth; return;
	}

	SetSmoothT ss;
	ss.group		= group;
	ss.newSmooth  = newSmooth;
	ss.oldSmooth  = oldSmooth;
	m_list.push_back(ss);
}

void MU_SetGroupAngle::undo(Model *model)
{
	for(auto&ea:m_list) model->setGroupAngle(ea.group,ea.oldAngle);
}
void MU_SetGroupAngle::redo(Model *model)
{
	for(auto&ea:m_list) model->setGroupAngle(ea.group,ea.newAngle);	
}
bool MU_SetGroupAngle::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SetGroupAngle*>(u))
	{
		for(auto&ea:undo->m_list)
		setGroupAngle(ea.group,ea.newAngle,ea.oldAngle);
		return true;
	}
	return false;
}
unsigned MU_SetGroupAngle::size()
{
	return sizeof(MU_SetGroupAngle)+m_list.size()*sizeof(SetAngleT);
}
void MU_SetGroupAngle::setGroupAngle(unsigned group,
		const uint8_t &newAngle, const uint8_t &oldAngle)
{
	for(auto&ea:m_list) if(ea.group==group)
	{
		ea.newAngle = newAngle; return;
	}

	SetAngleT ss;
	ss.group	  = group;
	ss.newAngle  = newAngle;
	ss.oldAngle  = oldAngle;
	m_list.push_back(ss);
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
void MU_SetMaterialTexture::setMaterialTexture(unsigned material,
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
	for(auto&ea:m_list)
	{
		s+=ea.key.size()+ea.value.size();
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
void MU_InterpolateSelected::_do(Model *model, bool redoing)
{	
	model->m_changeBits|=Model::MoveGeometry;

	//NOTE: This became too complicated for keyframes so 
	//it's just an optimization for vertex animation data
	auto fa = model->m_anims[m_anim];
	auto fp = fa->m_frame0+m_frame;
	auto e = m_e; //optimizing
	auto &vl = model->getVertexList();
	for(auto&ea:m_eold)
	{
		auto vp = vl[ea.v];
		auto vf = vp->m_frames[fp];
		auto &cmp = vf->m_interp2020;

		if(redoing)
		{
			//HACK: Maybe this value should already be stored.
			if(ea.e<=Model::InterpolateCopy)
			{
				/*I think this can be better now.
				assert(m_anim==model->getCurrentAnimation()); //2021
				model->validateAnim();
				memcpy(vf->m_coord,vp->m_kfCoord,sizeof(vp->m_kfCoord));*/
				model->interpKeyframe(m_anim,m_frame,ea.v,vf->m_coord);
			}

			cmp = e;
		}
		else cmp = ea.e;
	}

	if(m_anim==model->getCurrentAnimation()) 
	{
		model->invalidateAnim(); //2022

		model->setCurrentAnimationFrame(m_frame);
	}
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

void MU_SwapStableStr::undo(Model *m)
{
	m->setChangeBits(m_changeBits);

	std::swap(*m_addr,m_swap);
}
void MU_SwapStableStr::redo(Model *m)
{
	undo(m);
}
bool MU_SwapStableStr::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SwapStableStr*>(u))
	if(undo&&undo->m_addr==m_addr)
	{
		m_swap = undo->m_swap; return true;
	}
	return false;
}
unsigned MU_SwapStableStr::size()
{
	return sizeof(*this)+m_swap.size();
}

template<class F>
void MU_SwapStableMem::_for_each(const F &f)
{
	union{ rec *r; char *p; };
	p = m_mem.data();
	char *d = p+m_mem.size();
	while(p<d){ f(*r); p+=r->rsz; }
}
void MU_SwapStableMem::undo(Model *m)
{
	m->setChangeBits(m_changeBits);	

	_for_each([](rec &ea)
	{
		//ea.swap();
		memcpy(ea.addr,ea.mem,ea.dsz); //Faster? Fatter?		
	});
}
void MU_SwapStableMem::redo(Model *m)
{
	m->setChangeBits(m_changeBits);	
	
	_for_each([](rec &ea)
	{
		//ea.swap();
		memcpy(ea.addr,ea.mem+ea.dsz,ea.dsz); //Faster? Fatter?		
	});
}
void MU_SwapStableMem::addMemory(void *addr, const void *p, const void *cp, size_t sz)
{
	//m_changeBits|=m->m_changeBits;

	_for_each([=,&p](rec &ea)
	{
		if(ea.addr==addr)
		{
			if(sz==ea.dsz)
			{
				memcpy(ea.mem+sz,cp,sz);
			}
			else assert(sz==ea.dsz);

			p = nullptr;
		}
	});

	if(p)
	{
		//size_t rsz = sizeof(rbase)+sz; //Swap?
		size_t rsz = sizeof(rbase)+sz*2;
		while(rsz%4) rsz++;

		if(rsz>=65536){ assert(0); return; }

		size_t at = m_mem.size();
		m_mem.resize(at+rsz);

		auto r = (rec*)&m_mem[at];
		r->addr = addr; 
		r->dsz = (unsigned short)sz;
		r->rsz = (unsigned short)rsz;
		//memcpy(r->mem,cp,sz); //Swap?
		memcpy(r->mem,p,sz);
		memcpy(r->mem+sz,cp,sz);
	}
}
bool MU_SwapStableMem::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_SwapStableMem*>(u))
	{
		addChange(undo->m_changeBits);

		undo->_for_each([this](rec &ea)
		{
			addMemory(ea.addr,ea.mem,ea.mem+ea.dsz,ea.dsz);			
		});
		
		return true;
	}
	return false;
}
size_t MU_SwapStableMem::size()
{
	return sizeof(*this)+m_mem.size();
}

void MU_MoveUtility::undo(Model *model)
{
	model->moveUtility(m_newIndex,m_oldIndex);
}
void MU_MoveUtility::redo(Model *model)
{
	model->moveUtility(m_oldIndex,m_newIndex);
}
bool MU_MoveUtility::combine(Undo *u)
{
	return false;
}
unsigned MU_MoveUtility::size()
{
	return sizeof(MU_MoveUtility);
}

void MU_AssocUtility::undo(Model *model)
{
	for(auto rit=m_list.rbegin();rit!=m_list.rend();rit++)
	//switch(rit->m_op)
	{
	//case Model::PartGroups: case -Model::PartGroups:

		((Model::Group*)rit->ptr)->_assoc_util(m_util,rit->m_op<0);
	//	break;
	}
}
void MU_AssocUtility::redo(Model *model)
{
	for(auto&ea:m_list) //switch(ea.m_op)
	{
	//case Model::PartGroups: case -Model::PartGroups:

		((Model::Group*)ea.ptr)->_assoc_util(m_util,ea.m_op>0);
	//	break;
	}
}
void MU_AssocUtility::addAssoc(Model::Group *g)
{
	m_list.push_back({Model::PartGroups,g});
}
void MU_AssocUtility::removeAssoc(Model::Group *g)
{
	m_list.push_back({-Model::PartGroups,g});
}
bool MU_AssocUtility::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_AssocUtility*>(u))
	{
		//NOTE: Not attempting to merge/annihilate.
		for(auto&ea:undo->m_list) m_list.push_back(ea);

		return true;
	}
	return false;
}
unsigned MU_AssocUtility::size()
{
	return sizeof(*this)+m_list.size()*sizeof(rec);
}

void MU_Add::undo(Model *model)
{
	void(Model::*f)(int,void*),(Model::*g)(int);
	if(_fg(f,g)) if(m_op>0)
	{
		for(auto rit=m_list.rbegin();rit!=m_list.rend();rit++)
		(model->*g)(rit->index);
	}
	else
	{
		for(auto rit=m_list.rbegin();rit!=m_list.rend();rit++)	
		(model->*f)(rit->index,rit->ptr);
	}	
}
void MU_Add::redo(Model *model)
{
	void(Model::*f)(int,void*),(Model::*g)(int);
	if(_fg(f,g)) if(m_op>0)
	{
		for(auto&ea:m_list) (model->*f)(ea.index,ea.ptr);
	}
	else for(auto&ea:m_list) (model->*g)(ea.index);
}
bool MU_Add::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_Add*>(u))
	{
		if(undo->m_op==m_op)
		{
			for(auto&ea:undo->m_list) m_list.push_back(ea);

			return true;
		}
	}
	return false;
}
void MU_Add::undoRelease()
{
	if(m_op<0) _release();
}
void MU_Add::redoRelease()
{
	if(m_op>0) _release();
}
bool MU_Add::_fg(void(Model::*&f)(int,void*), void(Model::*&g)(int))
{
	typedef void(Model::*F)(int,void*);
	typedef void(Model::*G)(int);

	switch(abs(m_op)) //TODO: Model itself could consolidate these?
	{
	default: assert(0); return false;
	case Model::PartVertices:
		f = (F)&Model::insertVertex; g = (G)&Model::removeVertex; break;
	case Model::PartFaces:
		f = (F)&Model::insertTriangle; g = (G)&Model::removeTriangle; break;
	case Model::PartGroups: 
		f = (F)&Model::insertGroup; g = (G)&Model::removeGroup; break;
	case Model::PartMaterials: 
		f = (F)&Model::insertTexture; g = (G)&Model::removeTexture; break;
	case Model::PartJoints: 
		f = (F)&Model::insertBoneJoint; g = (G)&Model::removeBoneJoint; break;
	case Model::PartProjections:
		f = (F)&Model::insertProjection; g = (G)&Model::removeProjection; break;
	case Model::PartUtilities:
		f = (F)&Model::_insertUtil; g = (G)&Model::_removeUtil; break;
	}
	return true;
}
void MU_Add::_release()
{
	switch(abs(m_op))
	{
	default: assert(0);
	case Model::PartVertices:
		for(auto&ea:m_list) ((Model::Vertex*)ea.ptr)->release(); break;
	case Model::PartFaces:
		for(auto&ea:m_list) ((Model::Triangle*)ea.ptr)->release(); break;
	case Model::PartGroups:
		for(auto&ea:m_list) ((Model::Group*)ea.ptr)->release(); break;
	case Model::PartMaterials:
		for(auto&ea:m_list) ((Model::Material*)ea.ptr)->release(); break;
	case Model::PartJoints: 
		for(auto&ea:m_list) ((Model::Joint*)ea.ptr)->release(); break;
	case Model::PartProjections:
		for(auto&ea:m_list) ((Model::TextureProjection*)ea.ptr)->release(); break;
	case Model::PartUtilities:
		for(auto&ea:m_list) ((Model::Utility*)ea.ptr)->release(); break;
	}
}
unsigned MU_Add::size()
{
	//Q: SHOULD THE SIZE OF INSERTION OPERATIONS BE INCLUDED IN THE TOTAL?
	size_t sz; switch(abs(m_op))
	{
	default: assert(0); sz = 0; break;
	case Model::PartVertices: sz = sizeof(Model::Vertex); break;
	case Model::PartFaces: sz = sizeof(Model::Triangle); break;
	case Model::PartGroups: sz = sizeof(Model::Group); break;
	case Model::PartMaterials: sz = sizeof(Model::Material); break;
	case Model::PartJoints: sz = sizeof(Model::Joint); break;
	case Model::PartProjections: sz = sizeof(Model::TextureProjection); break;
	case Model::PartUtilities:
		
		sz = 0; for(auto&ea:m_list) switch(((Model::Utility*)ea.ptr)->type)
		{
		default: sz+=sizeof(Model::Utility); break;

		case Model::UT_UvAnimation: sz+=sizeof(Model::UvAnimation); break;
		}
		return sizeof(MU_Add)+m_list.size()*sizeof(rec)+sz;
	}
	return sizeof(MU_Add)+m_list.size()*(sizeof(rec)+sz);
}

void MU_UvAnimKey::_delete_key(Key &k)
{
	auto iit = m_anim->keys.begin();
	auto itt = m_anim->keys.end();
	auto it = std::lower_bound(iit,itt,k);
	if(it!=itt) m_anim->delete_key(it-iit);
	else assert(it!=itt);
}
void MU_UvAnimKey::undo(Model *m)
{
	auto rit = m_ops.rbegin();
	auto ritt = m_ops.rend();
	for(;rit<ritt;rit++) switch(rit->op)
	{
	case +1: _delete_key(rit->key); break;
	case 0: rit++;
	case -1: m_anim->set_key(rit->key); break;
	}
}
void MU_UvAnimKey::redo(Model *m)
{
	auto it = m_ops.begin();
	auto itt = m_ops.end();
	for(;it<itt;it++) switch(it->op)
	{
	case -1: _delete_key(it->key); break;
	case 0: it++;
	case +1: m_anim->set_key(it->key); break;
	}
}
void MU_UvAnimKey::deleteKey(const Key &k)
{
	auto rit = m_ops.rbegin();
	auto ritt = m_ops.rend();
	for(;rit<ritt;rit++) if(rit->key.frame==k.frame)
	{
		assert(rit->op!=-1);

		if(rit->op==0) rit[1].op = -1;

		m_ops.erase(rit.base()); return;
	}
	m_ops.push_back({-1,k});
}
void MU_UvAnimKey::insertKey(const Key &k)
{
	auto rit = m_ops.rbegin();
	auto ritt = m_ops.rend();
	for(;rit<ritt;rit++) if(rit->key.frame==k.frame)
	{
		assert(rit->op==-1);

		rit->op = +1; rit->key = k; return;
	}
	m_ops.push_back({+1,k});
}
void MU_UvAnimKey::assignKey(const Key &k, const Key &cp)
{
	auto rit = m_ops.rbegin();
	auto ritt = m_ops.rend();
	for(;rit<ritt;rit++) if(rit->key.frame==k.frame)
	{
		assert(rit->op!=-1);

		rit->key = cp; return;
	}
	m_ops.push_back({0,k});
	m_ops.push_back({0,cp});
}
bool MU_UvAnimKey::combine(Undo *u)
{
	if(auto*undo=dynamic_cast<MU_UvAnimKey*>(u))
	{
		auto it = undo->m_ops.begin();
		auto itt = undo->m_ops.end();
		for(;it<itt;it++) switch(it->op)
		{
		case -1: deleteKey(it[0].key); break;
		case +1: insertKey(it[0].key); break;
		case  0: assignKey(it[0].key,it[1].key); it++; break;			
		}		
		return true;
	}
	return false;
}
size_t MU_UvAnimKey::size()
{
	return sizeof(*this)+sizeof(rec)*m_ops.size();
}
