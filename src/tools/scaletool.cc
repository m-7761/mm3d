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
#include "glmath.h" //distance

#include "model.h"
#include "log.h"
#include "modelstatus.h"
#include "pixmap/scaletool.xpm"

enum ScaleProportionE
{
	ST_ScaleFree=0,
	ST_ScaleProportion2D,
	ST_ScaleProportion3D,
};

enum ScalePointE
{
	ST_ScalePointCoords=0,
	ST_ScalePointFar,
};

struct ScaleTool : Tool
{
	ScaleTool():Tool(TT_ScaleTool)
	{
		m_proportion = m_point = 0; //config details		
		m_translate = true;		

		//It can be very annoying to accidently scale a bunch of points or joints
		//and have to individually reset every scale component of everyone to 1
		m_scale = false; //true; 
	}

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Tool","Scale");
	}

	virtual const char *getKeymap(int){ return "S"; }

	virtual const char **getPixmap(int){ return scaletool_xpm; }

	virtual void activated(int)
	{
		model_status(parent->getModel(),StatusNormal,STATUSTIME_NONE,
		TRANSLATE_NOOP("Tool","Tip: Hold shift to restrict scaling to one dimension"));

		const char *e[3+1] = 
		{
		TRANSLATE_NOOP("Param","Free","Free scaling option"),
		TRANSLATE_NOOP("Param","Keep Aspect 2D","2D scaling aspect option"),
		TRANSLATE_NOOP("Param","Keep Aspect 3D","3D scaling aspect option"),
		};
		parent->addEnum(true,&m_proportion,TRANSLATE_NOOP("Param","Proportion"),e);

		const char *f[2+1] = 
		{
		TRANSLATE("Param","Center","Scale from center"),
		TRANSLATE("Param","Far Corner","Scale from far corner"),
		};
		parent->addEnum(true,&m_point,TRANSLATE_NOOP("Param","Point"),f);

		parent->addBool(true,&m_translate,TRANSLATE_NOOP("Param","Move"));
		parent->groupParam();
		parent->addBool(true,&m_scale,TRANSLATE_NOOP("Param","Scale"));
	}

	virtual void mouseButtonDown();
	virtual void mouseButtonMove();
	virtual void mouseButtonUp();

		bool m_selecting,m_left;

		int m_proportion,m_point;		
		bool m_translate,m_scale;

		double m_xx,m_yy;
		bool m_allowX,m_allowY;		

		double m_pointX;
		double m_pointY;
		double m_pointZ;

		double m_startLength;
		double m_startLengthX;
		double m_startLengthY;

		//double m_projScale;
		//int_list m_objList;
		//std::vector<std::pair<int,double>> m_objList;
		struct scale:Model::Position{ double xyz[3]; };
		std::vector<scale> m_objList;

		ToolCoordList m_positionCoords;
};

extern Tool *scaletool(){ return new ScaleTool; }

void ScaleTool::mouseButtonDown()
{
	m_left = //2021: Reserving BS_Right
	m_selecting = BS_Left&parent->getButtons();
	if(!m_left) 	
	return model_status(parent->getModel(),StatusError,STATUSTIME_LONG,
	TRANSLATE("Tool","No function"));

	Model *model = parent->getModel();

	int bs = parent->getButtonsLocked();

	m_allowX = m_allowY = 0!=(bs&BS_Shift);
			
	pos_list posList;
	model->getSelectedPositions(posList);
	m_positionCoords.clear();
	makeToolCoordList(m_positionCoords,posList);

	double min[3] = {+DBL_MAX,+DBL_MAX,+DBL_MAX}; 
	double max[3] = {-DBL_MAX,-DBL_MAX,-DBL_MAX};

	m_objList.clear(); for(auto&ea:m_positionCoords)
	{
		for(int i=0;i<3;i++)
		{
			min[i] = std::min(min[i],ea.coords[i]);
			max[i] = std::max(max[i],ea.coords[i]);
		}
	}
	if(m_scale) for(auto&ea:m_positionCoords)
	{
		//if(ea.pos.type==Model::PT_Projection)
		if(ea.pos.type!=Model::PT_Vertex)
		{
			if(ea.pos.type==Model::PT_Joint
			&&model->parentJointSelected(ea.pos))
			{
				continue;
			}

			//log_debug("found projection %d\n",ea.pos.index);
			//m_objList.push_back(std::make_pair(ea.pos.index,model->getProjectionScale(ea)));
			scale s; //s = ea.pos;
			static_cast<Model::Position&>(s) = ea.pos; //???
			model->getPositionScale(ea.pos,s.xyz);
			m_objList.push_back(s);
		}
	}

	double pos[2];
	parent->getParentXYValue(pos[0],pos[1],true);
	m_xx = pos[0];
	m_yy = pos[1];
	if(m_point==ST_ScalePointFar)
	{
		//NOTE: This looks wrong, but seems to not matter.
		m_pointZ = min[2];

		//DUPLICATES texwidget.cc.

		//NOTE: sqrt not required.
		double minmin = distance(min[0],min[1],pos[0],pos[1]);
		double minmax = distance(min[0],max[1],pos[0],pos[1]);
		double maxmin = distance(max[0],min[1],pos[0],pos[1]);
		double maxmax = distance(max[0],max[1],pos[0],pos[1]);

		//Can this be simplified?
		if(minmin>minmax)
		{
			if(minmin>maxmin)
			{
				if(minmin>maxmax)
				{
					m_pointX = min[0]; m_pointY = min[1];
				}
				else
				{
					m_pointX = max[0]; m_pointY = max[1];
				}
			}
			else same: // maxmin>minmin
			{
				if(maxmin>maxmax)
				{
					m_pointX = max[0]; m_pointY = min[1];
				}
				else
				{
					m_pointX = max[0]; m_pointY = max[1];
				}
			}
		}
		else // minmax>minmin
		{
			if(minmax>maxmin)
			{
				if(minmax>maxmax)
				{
					m_pointX = min[0]; m_pointY = max[1];
				}
				else
				{
					m_pointX = max[0]; m_pointY = max[1];
				}
			}
			else goto same; // maxmin>minmax
		}
	}
	else
	{
		m_pointX = (max[0]-min[0])/2+min[0];
		m_pointY = (max[1]-min[1])/2+min[1];
		m_pointZ = (max[2]-min[2])/2+min[2];
	}

	m_startLengthX = fabs(m_pointX-pos[0]);
	m_startLengthY = fabs(m_pointY-pos[1]);
	m_startLength = magnitude(m_startLengthX,m_startLengthY);

	model_status(model,StatusNormal,STATUSTIME_SHORT,
	TRANSLATE("Tool","Scaling"));
}

void ScaleTool::mouseButtonMove()
{
	m_selecting = false;
	//2021: Reserving BS_Right
	if(!m_left) return;

	Model *model = parent->getModel();

	double pos[2];
	parent->getParentXYValue(pos[0],pos[1]);

	//TODO: STANDARDIZE AND REQUIRE MINIMUM PIXELS
	if(m_allowX||m_allowY)
	{
		if(m_allowX&&m_allowY)
		{
			double ax = fabs(pos[0]-m_xx);
			double ay = fabs(pos[1]-m_yy);

			if(ax>ay) m_allowY = false;
			if(ay>ax) m_allowX = false;
		}

		if(!m_allowX) pos[0] = m_xx;
		if(!m_allowY) pos[1] = m_yy;
	}

	const double &spX = m_pointX;
	const double &spY = m_pointY;
	const double &spZ = m_pointZ;

	const double lengthX = fabs(spX-pos[0]);
	const double lengthY = fabs(spY-pos[1]);
	const double length = magnitude(lengthX,lengthY);
	
	bool free = m_proportion==ST_ScaleFree;
	bool sp3d = m_proportion==ST_ScaleProportion3D;

	static const double min = 0.00006;
	static const double cmp = magnitude(min,min);

	double uniform,xper,yper; 
	uniform = m_startLength<=cmp?1:length/m_startLength;
	if(free)
	{
		xper = m_startLengthX<=min?1:lengthX/m_startLengthX; //???
		yper = m_startLengthY<=min?1:lengthY/m_startLengthY; //???
	}
	else xper = yper = uniform;

	if(m_translate)
	for(auto&ea:m_positionCoords)
	{
		double x = ea.coords[0]-spX;
		double y = ea.coords[1]-spY;
		double z = ea.coords[2]-spZ;

		x*=xper; y*=yper; if(sp3d) z*=xper;

		//2020: Don't generate keyframe/undo data?
		if(magnitude(x,y,z)>cmp)
		{
			movePosition(ea.pos,x+spX,y+spY,z+spZ);
		}
	}

	if(!m_objList.empty())
	{
		double s[3];
		if(!free) for(auto&ea:m_objList)
		{			
			for(int i=3;i-->0;)
			s[i] = ea.xyz[i]*uniform;
			model->setPositionScale(ea,s);
		}
		else if(xper!=1||yper!=1) //nonuniform
		{
			Matrix m;
			double v[3] = {xper,yper,sp3d?xper:1}; 
			parent->getParentBestInverseMatrix().apply3(v);
			for(auto&ea:v) 
			ea = fabs(ea); //apply3 can be negative.
			for(auto&ea:m_objList)
			if(ea.type!=Model::PT_Projection)
			{
				//NOTE: This isn't very user-friendly but it's how the 
				//scale tool works.
				for(int i=3;i-->0;)
				s[i] = v[i];
				double r[3];				
				if(ea.type==Model::PT_Joint)
				{
					//NOTE: Rotation shouldn't change while dragging so
					//this should be safe WRT validateSkel/validateAnim.

					//Won't work since scaling is included :(
					//model->m_joints[ea]->m_final.inverseRotateVector(s);
					model->getBoneJointFinalMatrix(ea,m);
					m.getScale(r);
					for(int i=3;i-->0;) r[i] = 1/r[i];
					m.scale(r);
					m.getRotation(r);
				}
				else 
				{
					model->getPositionRotation(ea,r);
					m.setRotation(r);
				}
				m.inverseRotateVector(s);
				for(int i=3;i-->0;)
				s[i] = fabs(s[i])*ea.xyz[i];
				model->setPositionScale(ea,s);
			}
			else model->setProjectionScale(ea,ea.xyz[0]*uniform);
		}			
	}

	parent->updateAllViews(); //Needed
}
void ScaleTool::mouseButtonUp()
{
	Model *model = parent->getModel();

	if(m_selecting)
	{
		parent->snapSelect(); //EXPERIMENTAL		
	}
	else if(m_left)	
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Scale complete"));
	}

	parent->updateAllViews(); //Needed
}