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
#include "modelstatus.h"
#include "undomgr.h"
#include "modelundo.h"
#endif // MM3D_EDIT

infl_list &Model::getVertexInfluences(unsigned vertex) //NEW
{		
	return m_vertices[vertex]->m_influences;
}
infl_list &Model::getPointInfluences(unsigned point) //NEW
{
	return m_points[point]->m_influences;
}
infl_list *Model::getPositionInfluences(const Position &pos) //NEW
{
	switch(pos.type)
	{
	case PT_Vertex: return &getVertexInfluences(pos.index);			
	case PT_Point: return &getPointInfluences(pos.index);
	}
	return nullptr;
}
const infl_list &Model::getVertexInfluences(unsigned vertex)const //NEW 
{		
	return m_vertices[vertex]->m_influences;
}
const infl_list &Model::getPointInfluences(unsigned point)const //NEW 
{
	return m_points[point]->m_influences;
}
const infl_list *Model::getPositionInfluences(const Position &pos)const //NEW
{
	switch(pos.type)
	{
	case PT_Vertex: return &getVertexInfluences(pos.index);
	case PT_Point: return &getPointInfluences(pos.index);
	}
	return nullptr;
}

bool Model::setPositionBoneJoint(const Position &pos, int joint)
{
	removeAllPositionInfluences(pos);
	return setPositionInfluence(pos,joint,IT_Custom,1);
}
bool Model::setVertexBoneJoint(unsigned vertex, int joint)
{
	return setPositionBoneJoint({PT_Vertex,vertex},joint);
}
bool Model::setPointBoneJoint(unsigned point, int joint)
{
	return setPositionBoneJoint({PT_Point,point},joint);
}

/*REFERENCE
void Model::getBoneJointVertices(int joint, int_list &rval)const
{
	rval.clear();

	if(joint>=-1&&joint<(signed)m_joints.size()) //???
	{
		//infl_list ilist;
		infl_list::const_iterator it;

		for(unsigned v = 0; v<m_vertices.size(); v++)
		{
			//getVertexInfluences(v,ilist);
			const infl_list &ilist = getVertexInfluences(v);

			for(it = ilist.begin(); it!=ilist.end(); it++)
			{
				if((*it).m_boneId==joint)
				{
					rval.push_back(v);
					break;
				}
			}
		}
	}
}*/

bool Model::setPositionInfluence(const Position &pos, unsigned joint, InfluenceTypeE type, double weight)
{	
	if(joint>=m_joints.size()) return false;

	auto *il = getPositionInfluences(pos); if(!il) return false;

	for(auto&ea:*il) if(ea.m_boneId==(int)joint)
	{
		m_changeBits |= MoveGeometry|SetInfluence; //AddOther

		InfluenceT oldInf = ea;

		ea.m_weight = weight;
		ea.m_type = type;

		InfluenceT newInf = ea;

		Undo<MU_UpdatePositionInfluence>(this,pos,newInf,oldInf);

		calculateRemainderWeight(pos);

		return true;
	}

	//if(il->size()<MAX_INFLUENCES) //2021
	{
		InfluenceT inf;
		inf.m_boneId = (int)joint;
		inf.m_weight = weight;
		inf.m_type = type;

		Undo<MU_SetPositionInfluence>(this,true,pos,il->size(),inf);

		insertInfluence(pos,il->size(),inf);

		return true;
	}

	return false;
}
bool Model::setVertexInfluence(unsigned vertex, unsigned joint, InfluenceTypeE type, double weight)
{
	return setPositionInfluence({PT_Vertex,vertex},joint,type,weight);
}
bool Model::setPointInfluence(unsigned point, unsigned joint, InfluenceTypeE type, double weight)
{
	return setPositionInfluence({PT_Point,point},joint,type,weight);
}

bool Model::removePositionInfluence(const Position &pos, unsigned joint)
{
	if(auto*il=getPositionInfluences(pos))
	{
		int index = 0; for(auto&ea:*il)
		{
			if(ea.m_boneId!=(int)joint)
			{
				index++; continue;
			}

			Undo<MU_SetPositionInfluence>(this,false,pos,index,ea);

			removeInfluence(pos,index);

			return true;
		}
	}
	return false;
}
bool Model::removeVertexInfluence(unsigned vertex, unsigned joint)
{
	return removePositionInfluence({PT_Vertex,vertex},joint);
}
bool Model::removePointInfluence(unsigned point, unsigned joint)
{
	return removePositionInfluence({PT_Point,point},joint);
}

bool Model::removeAllPositionInfluences(const Position &pos)
{
	if(auto*il=getPositionInfluences(pos)) if(!il->empty())
	{		
		while(!il->empty())
		removePositionInfluence(pos,il->back().m_boneId);
		return true;
	}
	return false;
}
bool Model::removeAllVertexInfluences(unsigned vertex)
{
	return removeAllPositionInfluences({PT_Vertex,vertex});
}
bool Model::removeAllPointInfluences(unsigned point)
{
	return removeAllPositionInfluences({PT_Point,point});	
}

bool Model::getPositionInfluences(const Position &pos, infl_list &l)const
{
	auto *il = getPositionInfluences(pos); if(!il) return false;

	if(il) l = *il; else l.clear(); //2020
	
	return !l.empty();
}
bool Model::getVertexInfluences(unsigned vertex, infl_list &l)const
{
	return getPositionInfluences({PT_Vertex,vertex},l);
}
bool Model::getPointInfluences(unsigned point, infl_list &l)const
{
	return getPositionInfluences({PT_Point,point},l);
}

int Model::getPrimaryPositionInfluence(const Position &pos)const
{
	//infl_list l;
	//getPositionInfluences(pos,l);
	auto *il = getPositionInfluences(pos);
	if(!il) return -1;

	Model::InfluenceT inf;
	inf.m_boneId = -1;
	inf.m_weight = -1.0;
	for(auto&ea:*il) if(inf<ea) inf = ea;

	return inf.m_boneId;
}
int Model::getPrimaryVertexInfluence(unsigned vertex)const
{
	return getPrimaryPositionInfluence({PT_Vertex,vertex});
}
int Model::getPrimaryPointInfluence(unsigned point)const
{
	return getPrimaryPositionInfluence({PT_Point,point});
}

bool Model::setPositionInfluenceType(const Position &pos, unsigned int joint, InfluenceTypeE type)
{
	if(joint>=m_joints.size()) return false;

	auto *il = getPositionInfluences(pos); if(!il) return false;
	
	m_changeBits |= MoveGeometry|SetInfluence; //AddOther

	for(auto&ea:*il) if(ea.m_boneId==(int)joint)
	{
		if(type==ea.m_type) return true; //2020
	
		InfluenceT oldInf = ea; ea.m_type = type;
		InfluenceT newInf = ea;
				
		Undo<MU_UpdatePositionInfluence>(this,pos,newInf,oldInf);

		calculateRemainderWeight(pos);

		return true;
	}

	return false;
}
bool Model::setVertexInfluenceType(unsigned vertex, unsigned int joint, InfluenceTypeE type)
{
	return setPositionInfluenceType({PT_Vertex,vertex},joint,type);
	
}
bool Model::setPointInfluenceType(unsigned point, unsigned int joint, InfluenceTypeE type)
{
	return setPositionInfluenceType({PT_Point,point},joint,type);
}

bool Model::setPositionInfluenceWeight(const Position &pos, unsigned int joint, double weight)
{
	if(joint>=m_joints.size()) return false;

	auto *il = getPositionInfluences(pos); if(!il) return false;

	for(auto&ea:*il) if(ea.m_boneId==(int)joint)
	{
		if(weight==ea.m_weight) return true; //2020

		m_changeBits |= MoveGeometry|SetInfluence; //AddOther

		InfluenceT oldInf = ea; ea.m_weight = weight;
		InfluenceT newInf = ea;

		Undo<MU_UpdatePositionInfluence>(this,pos,newInf,oldInf);

		calculateRemainderWeight(pos);

		return true;
	}

	return false;
}
bool Model::setVertexInfluenceWeight(unsigned vertex, unsigned int joint, double weight)
{
	return setPositionInfluenceWeight({PT_Vertex,vertex},joint,weight);
}
bool Model::setPointInfluenceWeight(unsigned point, unsigned int joint, double weight)
{
	return setPositionInfluenceWeight({PT_Point,point},joint,weight);
}

double Model::calculatePositionInfluenceWeight(const Position &pos, unsigned joint)const
{
	double coord[3] = { 0,0,0 };
	getPositionCoords(pos,coord);
	return calculateCoordInfluenceWeight(coord,joint);
}
double Model::calculateVertexInfluenceWeight(unsigned vertex, unsigned joint)const
{
	return calculatePositionInfluenceWeight({PT_Vertex,vertex},joint);
}
double Model::calculatePointInfluenceWeight(unsigned point, unsigned joint)const
{
	return calculatePositionInfluenceWeight({PT_Point,point},joint);
}
double Model::calculateCoordInfluenceWeight(const double *coord, unsigned joint)const
{
	if(joint>=m_joints.size()) return 0;

	int bcount = m_joints.size();

	int child = -1;
	double cdist = 0.0;
	for(int b = 0; b<bcount; b++)	
	if(getBoneJointParent(b)==(int)joint)
	{
		double ccoord[3];
		getBoneJointCoords(b,ccoord);
		double d = distance(ccoord,coord);

		if(child<0||d<cdist)
		{
			child = b;
			cdist = d;
		}
	}

	double bvec[3] = { 0,0,0 };
	double pvec[3] = { 0,0,0 };

	getBoneVector(joint,bvec,coord);

	double jcoord[3] = { 0,0,0 };
	getBoneJointCoords(joint,jcoord);

	pvec[0] = coord[0]-jcoord[0];
	pvec[1] = coord[1]-jcoord[1];
	pvec[2] = coord[2]-jcoord[2];

	normalize3(pvec);

	// get cos from point to bone vector
	double bcos = dot3(pvec,bvec);
	bcos = (bcos+1.0)/2.0;

	if(child<0)
	{
		return bcos; // no children
	}

	double cvec[3] = { 0,0,0 };
	getBoneVector(child,cvec,coord);

	// get cos from point to child vector
	double ccoord[3] = { 0,0,0 };
	getBoneJointCoords(child,ccoord);

	pvec[0] = coord[0]-ccoord[0];
	pvec[1] = coord[1]-ccoord[1];
	pvec[2] = coord[2]-ccoord[2];

	normalize3(pvec);
	double ccos = dot3(pvec,cvec);

	ccos = -ccos;
	ccos = (ccos+1.0)/2.0;

	return bcos *ccos;
}

bool Model::autoSetPositionInfluences(const Position &pos, double sensitivity, bool selected)
{
	if(!getPositionInfluences(pos)) return false; //2024

	double coord[3] = { 0,0,0 };
	getPositionCoords(pos,coord);
	int_list l;
	if(autoSetCoordInfluences(coord,sensitivity,selected,l))
	{
		removeAllPositionInfluences(pos);
		for(int i:l)
		{
			double w = calculatePositionInfluenceWeight(pos,i);
			setPositionInfluence(pos,i,Model::IT_Auto,w);
		}
		return true;
	}
	return false;
}
bool Model::autoSetVertexInfluences(unsigned vertex, double sensitivity, bool selected)
{
	return autoSetPositionInfluences({PT_Vertex,vertex},sensitivity,selected);
}
bool Model::autoSetPointInfluences(unsigned point, double sensitivity, bool selected)
{
	return autoSetPositionInfluences({PT_Point,point},sensitivity,selected);
}

bool Model::autoSetCoordInfluences(double *coord, double sensitivity, bool selected, int_list &infList)
{
	int bcount = m_joints.size(); if(bcount<=0) return false;

	int bestJoint = -1;
	int bestChild = -1;
	double bestChildDist = 0;
	double bestDist = 0;
	double bestDot  = 0;

	for(int joint=0;joint<bcount;joint++)	
	if(!selected||m_joints[joint]->m_selected)
	{
		int child = -1;
		double chdist = 0;
		for(int ch=0;ch<bcount;ch++)		
		if(getBoneJointParent(ch)==joint)
		{
			double ccoord[3];
			getBoneJointCoords(ch,ccoord);
			double d = distance(ccoord,coord);
			if(child<0||d<chdist)
			{
				child = ch; chdist = d;
			}
		}

		double bvec[3] = {};
		double pvec[3] = {};

		getBoneVector(joint,bvec,coord);

		double jcoord[3] = {};
		getBoneJointCoords(joint,jcoord);

		for(int i=3;i-->0;)
		pvec[i] = coord[i]-jcoord[i];
		normalize3(pvec);

		double dist = distance(coord,jcoord);

		// get cos from point to bone vector
		double bcos = dot3(pvec,bvec);		
		dist*= bcos>0?0.667:2; // *= bcos;

		if(bestJoint<0||dist<bestDist)
		{
			bestJoint = joint;
			bestDist  = dist;
			bestChild = child;
			bestChildDist = chdist;
			bestDot	= bcos;
		}
	}

	if(bestJoint>=0)
	{
		infList.push_back(bestJoint);

		if(bestChild>=0)		
		if(bestChildDist*(1-sensitivity)<bestDist*0.5)
	//	if(!selected||m_joints[bestChild]->m_selected) //2024 //TESTING
		{
			infList.push_back(bestChild);
		}
		
		int parent = getBoneJointParent(bestJoint);
		if(parent>0)
		if((bestDot-1)*sensitivity<-0.080)
	//	if(!selected||m_joints[parent]->m_selected) //2024 //TESTING
		{
			infList.push_back(parent);
		}
		return true;
	}
	return false;
}

void Model::calculateRemainderWeight(const Position &pos)
{
	auto *il = getPositionInfluences(pos); if(!il) return;

	//2021: Can't rememember why I split this off 
	//but this was its only use case.
	//_calculateRemainderWeight(*il);
	{
		int	 remainders = 0; double remaining = 1;

		for(auto&ea:*il) if(ea.m_type==IT_Remainder)
		{
			remainders++;
		}
		else remaining-=ea.m_weight;

		if(remainders>0&&remaining>0)
		{
			for(auto&ea:*il) if(ea.m_type==IT_Remainder)
			{
				ea.m_weight = remaining/(double)remainders;
			}
		}
	}

	if(inSkeletalMode())
	if(pos.type==PT_Vertex)
	{	
		m_changeBits |= MoveGeometry;

		if(m_validAnim)
		{
			m_vertices[pos]->_resample(*this,pos);

			invalidateAnimNormals(); //OVERKILL
		}
	}
	else if(pos.type==PT_Point)
	{
		m_changeBits |= MoveOther;

		if(m_validAnim)
		{
			m_points[pos]->_resample(*this,pos);
		}
	}
	else assert(0);
}
