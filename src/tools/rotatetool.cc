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

#include "model.h"
#include "pixmap/rotatetool.xpm"
#include "glmath.h"

#include "rotatepoint.h"
#include "log.h"
#include "modelstatus.h"

struct RotateTool : Tool
{
	RotateTool():Tool(TT_Other)
	{
		m_mode = 0; 

		m_move = m_rotate = m_link = true;
	}

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Tool","Rotate");
	}

	virtual const char *getKeymap(int){ return "R"; }

	virtual const char **getPixmap(int){ return rotatetool_xpm; }

	void activated2(); 

	virtual void activated(int)
	{
		activated2();

		/*WIP
		const char *f[7+1] = 
		{
		TRANSLATE_NOOP("Param","Rotate"),
		TRANSLATE_NOOP("Param","X Axis"),
		TRANSLATE_NOOP("Param","Y Axis"),
		TRANSLATE_NOOP("Param","Z Axis"),
		TRANSLATE_NOOP("Param","X Roll"),
		TRANSLATE_NOOP("Param","Y Roll"),
		TRANSLATE_NOOP("Param","Z Roll"),
		};
		parent->addEnum(true,&m_mode,TRANSLATE_NOOP("Param","Mode"),f);

		parent->addBool(true,&m_move,TRANSLATE_NOOP("Param","Move"));
		parent->groupParam();
		parent->addBool(true,&m_rotate,TRANSLATE_NOOP("Param","Rotate"));
		parent->groupParam();
		parent->addBool(true,&m_link,TRANSLATE_NOOP("Param","Link"));*/

		parent->addDouble(false,&m_rotatePoint.x,TRANSLATE_NOOP("Param","X"));
		parent->groupParam();
		parent->addDouble(false,&m_rotatePoint.y,TRANSLATE_NOOP("Param","Y"));
		parent->groupParam();
		parent->addDouble(false,&m_rotatePoint.z,TRANSLATE_NOOP("Param","Z"));

		modelChanged(Model::SelectionJoints);
	}	
	virtual void modelChanged(int changeBits)
	{
		//NOTE: The only way to do this currently is to
		//do something like Ctrl+A or turn on animation.
		if(changeBits&(Model::SelectionJoints|Model::AnimationChange))
		{
			m_skel_mode = false;

			auto model = parent->getModel();
			bool hide = true; 
			for(auto&ea:model->getFlatJointList())			
			if(ea.second->m_selected)
			{
				if(m_skel_mode=model->inJointAnimMode())
				{
					model->getBoneJointCoords(ea.first,m_rotatePoint);
					parent->updateParams();
				}
				hide = false; break;
			}
			/*WIP
			parent->hideParam(&m_mode,hide);
			parent->hideParam(&m_rotate,hide);
			parent->hideParam(&m_move,hide);
			parent->hideParam(&m_link,hide);*/
		}
	}
	virtual void updateParam(void*)
	{
		//TODO: Update parent?
		//DecalManager::getInstance()->modelUpdated(parent->getModel());
		parent->updateAllViews();
	}
	virtual void deactivated()
	{
		parent->updateAllViews();
	}
	
	int mouse2();

	virtual void mouseButtonDown();
	virtual void mouseButtonMove();
	virtual void mouseButtonUp();

	virtual void draw(bool){ m_rotatePoint.draw(); }
		
		double m_mouse2_angle;

		bool m_selecting;
		
		bool m_skel_mode;

		int m_mode; 
		
		bool m_move,m_rotate,m_link;

		struct RotatePoint
		{	
			void draw()
			{
				//HACK: The crosshairs draw on top of each
				//other. Note, assuming the previous value.
				glEnable(GL_DEPTH_TEST);
				glDepthRange(0,0);
				glDepthFunc(GL_LESS);
				{
					//glColor3f(0,1,0);
					glEnable(GL_COLOR_LOGIC_OP);
					glColor3ub(0x80,0x80,0x80);
					glLogicOp(GL_XOR);
					glTranslated(x,y,z);
					rotatepoint_draw_manip((float)scale); //0.25f
					glTranslated(-x,-y,-z);
					glDisable(GL_COLOR_LOGIC_OP);
				}
				glDepthRange(0,1);
				glDepthFunc(GL_LEQUAL);
			}

			double x,y,z,w, scale;

			operator double*(){ return &x; }	

		}m_rotatePoint;
	
	void getRotateCoords();
};

extern Tool *rotatetool(){ return new RotateTool; }

void RotateTool::activated2()
{
	Model *model = parent->getModel();
		
	int am = model->getAnimationMode();
	if(am&Model::ANIMMODE_JOINT)
	{
		int iN = model->getBoneJointCount();
		for(int i=0;i<iN;i++)		
		if(model->isBoneJointSelected(i))
		{
			model->getBoneJointCoords(i,m_rotatePoint);
			goto empty2;
		}
		if(am!=Model::ANIMMODE_JOINT) //2021
		goto empty3;
		
	empty1: m_rotatePoint.x = m_rotatePoint.y = m_rotatePoint.z = 0;
	empty2:	m_rotatePoint.scale = model->getViewportUnits().inc3d/4; //0.25
	}
	else empty3:
	{
		//FIX ME
		//Min/max/scale the pivot accordingly.
		//https://github.com/zturtleman/mm3d/issues/89
		pos_list l; model->getSelectedPositions(l);
		if(l.empty()) goto empty1; //NEW
		double min[3] = {+DBL_MAX,+DBL_MAX,+DBL_MAX}; 
		double max[3] = {-DBL_MAX,-DBL_MAX,-DBL_MAX}; 
		for(auto&ea:l) 
		{
			double coords[3];
			model->getPositionCoords(ea,coords);
			for(int i=0;i<3;i++)
			{
				min[i] = std::min(min[i],coords[i]);
				max[i] = std::max(max[i],coords[i]);
			}
		}
		for(int i=0;i<3;i++)
		m_rotatePoint[i] = (min[i]+max[i])/2;
		double dist = distance(min,max);
		if(!dist) goto empty2; //NEW
		m_rotatePoint.scale = dist/6;
	}
	m_rotatePoint.w = 1;		

	//FIX ME
	//Should show decal in all views.
//	DecalManager::getInstance()->addDecalToModel(&m_rotatePoint,model);
	parent->updateAllViews(); //NEW
		
	model_status(model,StatusNormal,STATUSTIME_NONE,
	TRANSLATE("Tool","Tip: Hold shift to rotate in 15 degree increments"));
}

int RotateTool::mouse2()
{
	Model *model = parent->getModel();
	
	double pos[2];
	parent->getParentXYValue(pos[0],pos[1],true);

	int ret = 0; if(parent->getButtons()&BS_Left)
	{
		ret = 1;

		double coords[4];
		memcpy(coords,m_rotatePoint,sizeof(coords));
		coords[3] = 1; //???
		parent->getParentBestMatrix().apply4(coords);

		double xDiff = pos[0]-coords[0];
		double yDiff = pos[1]-coords[1];
		double angle = rotatepoint_diff_to_angle(xDiff,yDiff);
		if(parent->getButtonsLocked()&BS_Shift) angle = 
		rotatepoint_adjust_to_nearest(angle,parent->getButtons()&Tool::BS_Alt?5:15); //HACK

		m_mouse2_angle = angle;
	}
	else if(!m_skel_mode&&parent->getButtons()&BS_Right)
	{
		ret = 2;

		//FIX ME
		//ModelViewport::frameArea determines Z for ortho
		//views. RotateTool::activated places Z elsewhere.
		m_rotatePoint.x = pos[0];
		m_rotatePoint.y = pos[1];
		m_rotatePoint.z = 0;
		m_rotatePoint.w = 1; //???
		parent->getParentBestInverseMatrix().apply4(m_rotatePoint);
		parent->updateParams();
	}

	parent->updateAllViews(); //Needed
	
	return ret;
}
void RotateTool::mouseButtonDown()
{
	m_selecting = BS_Left&parent->getButtons();

	Model *model = parent->getModel();
	 
	if(int mode=mouse2())
	{
		const char *msg;
		if(mode==1) msg = TRANSLATE("Tool","Rotating");	
		if(mode!=1) msg = TRANSLATE("Tool","Setting rotation point");
		model_status(model,StatusNormal,STATUSTIME_SHORT,msg);
	}
}
void RotateTool::mouseButtonMove()
{
	m_selecting = false;

	double angle = m_mouse2_angle; if(1==mouse2())
	{
		double vec[3] = { 0,0,1 };
		parent->getParentBestInverseMatrix().apply3(vec);
		Matrix m;
		m.setRotationOnAxis(vec,m_mouse2_angle-angle);

		parent->getModel()->rotateSelected(m,m_rotatePoint);
	}
}
void RotateTool::mouseButtonUp()
{
	Model *model = parent->getModel();

	if(m_selecting)
	{
		parent->snapSelect(); //EXPERIMENTAL		
	}
	else if(parent->getButtons()&BS_Left)	
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Rotate complete"));
	}
	
	parent->updateAllViews(); //Needed
}

