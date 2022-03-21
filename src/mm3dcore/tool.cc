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

#include "tool.h"
#include "log.h"

//int Tool::s_allocated = 0; //toolbox.cc

bool Tool::tool_option_face_view = true; //2022

Tool::Tool(ToolType tt, int count, const char *path)
	:
parent(),m_tooltype(tt),m_args(count),m_path(path)
{
	s_allocated++;
}
Tool::~Tool(){ s_allocated--; }

Tool::ToolCoordT Tool::addPosition
(Model::PositionTypeE type, 
double x, double y, double z, const char *name, int boneId)
{	
	Model *model = parent->getModel();

	const Matrix &m = parent->getParentBestInverseMatrix();
	double tranVec[4] = { x,y,z,1 };	
	m.apply(tranVec);

	Model::Position pos;
	pos.type  = type;
	pos.index = ~0U;
	switch(type)
	{
	case Model::PT_Vertex:
		pos.index = model->addVertex(tranVec[0],tranVec[1],tranVec[2]);
		break;
	case Model::PT_Joint:

		//FIX ME
		//Because rotateSelected works in the global frame assigning a
		//random rotation just confuses things since it's canceled out.
		//if(1)
		{
			pos.index = model->addBoneJoint(name,tranVec[0],tranVec[1],tranVec[2]/*,0,0,0*/,boneId);
			break;
		}
		//break; //FALLING THROUGH?

	case Model::PT_Point:
	{		
		double rotVec[3] = { /*xrot,yrot,zrot*/ };
		Matrix rot;
		rot.setRotation(rotVec);
		rot = rot *m;
		rot.getRotation(rotVec);
		//I don't know if this is helpful for joints since their Z-axis is
		//defined by their parent-child relationship.
	//	if(Model::PT_Joint==type)
	//	pos.index = model->addBoneJoint(name,tranVec[0],tranVec[1],tranVec[2],
	//				rotVec[0],rotVec[1],rotVec[2],boneId);
		if(Model::PT_Point==type)
		pos.index = model->addPoint(name,tranVec[0],tranVec[1],tranVec[2],
					rotVec[0],rotVec[1],rotVec[2]);
		break;
	}		
	case Model::PT_Projection:
		pos.index = model->addProjection(name,Model::TPT_Cylinder,tranVec[0],tranVec[1],tranVec[2]);
		break;
	default:
		log_error("don't know how to add a point of type %d\n",static_cast<int>(type));
		break;
	}

	ToolCoordT tc;

	tc.pos = pos;
	tc.coords[0] = x;
	tc.coords[1] = y;
	tc.coords[2] = z;

	return tc;
}

void Tool::movePosition
(const Model::Position &pos,double x, double y, double z)
{
	const Matrix &m = parent->getParentBestInverseMatrix();

	double tranVec[3] = { x,y,z };

	//log_debug("orig position is %f %f %f\n",tranVec[0],tranVec[1],tranVec[2]);

	m.apply3x(tranVec);

	//log_debug("tran position %f,%f,%f\n",tranVec[0],tranVec[1],tranVec[2]);

	parent->getModel()->movePosition(pos,tranVec[0],tranVec[1],tranVec[2]);
}
void Tool::movePositionUnanimated
(const Model::Position &pos,double x, double y, double z)
{
	const Matrix &m = parent->getParentBestInverseMatrix();

	double tranVec[3] = { x,y,z };

	//log_debug("orig position is %f %f %f\n",tranVec[0],tranVec[1],tranVec[2]);

	m.apply3x(tranVec);

	//log_debug("tran position %f,%f,%f\n",tranVec[0],tranVec[1],tranVec[2]);

	auto model = parent->getModel();
	model->movePositionUnanimated(pos,
	tranVec[0],tranVec[1],tranVec[2]);
}

void Tool::makeToolCoordList
(ToolCoordList &list, const pos_list &positions)
{
	Model *model = parent->getModel();
	const Matrix &mat = parent->getParentBestMatrix();

	ToolCoordT tc; for(auto&ea:positions)
	{
		tc.pos = ea;
		model->getPositionCoords(tc.pos,tc.coords);
		mat.apply3x(tc.coords);
		list.push_back(tc);
	}
}
bool Tool::makeToolCoord(ToolCoordT &tc, Model::Position pos)
{
	Model *model = parent->getModel();
	const Matrix &mat = parent->getParentBestMatrix();

	tc.pos = pos;
	if(!model->getPositionCoords(tc.pos,tc.coords))
	return false;

	mat.apply3(tc.coords);
	tc.coords[0] += mat.get(3,0);
	tc.coords[1] += mat.get(3,1);
	tc.coords[2] += mat.get(3,2); return true;
}

