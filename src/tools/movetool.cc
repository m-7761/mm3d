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

#include "pixmap/movetool.xpm"

#include "model.h"
#include "modelstatus.h"
#include "log.h"

struct MoveTool : Tool
{
	MoveTool():Tool(TT_MoveTool)
	{
		m_snap3d = false; //config defaults
	}

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Tool","Move");
	}

	//2019: T is beside R and not on the right-hand side of
	//the keyboard. T for Translate. M for Material.
	virtual const char *getKeymap(int){ return "T"; } //"M"

	virtual const char **getPixmap(int){ return movetool_xpm; }	

	virtual void activated(int)
	{
		model_status(parent->getModel(),StatusNormal,STATUSTIME_NONE,
		TRANSLATE("Tool","Tip: Hold shift to restrict movement to one dimension"));

		parent->addBool(true,&m_snap3d,TRANSLATE_NOOP("Param","Snap in 3D"));
	}
	
	virtual void mouseButtonDown();
	virtual void mouseButtonMove();
	virtual void mouseButtonUp();

		bool m_snap3d;

		bool m_selecting,m_left;

		double m_c[4]; //m_x/y/z

		bool m_allowX,m_allowY; double m_xx,m_yy,m_zz,m_ww;
};

extern Tool *movetool(){ return new MoveTool; }

void MoveTool::mouseButtonDown()
{
	m_left = //2021: Reserving BS_Right
	m_selecting = BS_Left&parent->getButtons();
	if(!m_left) 
	return model_status(parent->getModel(),StatusError,STATUSTIME_LONG,
	TRANSLATE("Tool","No function"));

	//EXPERIMENTAL
	//parent->getParentXYValue(m_x,m_y,true);
	parent->snap_select = true;
	parent->snap_object.type = Model::PT_MAX; //2023
	bool snap = parent->getParentCoords(m_c,true);	

	if(BS_Alt&parent->getButtons()?m_snap3d:!m_snap3d)
	{
		m_zz = m_c[2] = 0; m_ww = m_c[3] = 1;
	}
	else
	{
		if(parent->getView()<=Tool::ViewPerspective) //2023
		{
			double avg[4] = {0,0,0,1};
			getSelectionCenter(avg);		
			parent->getParentProjMatrix().apply4(avg);

			m_zz = avg[2]; if(!snap) m_c[2] = avg[2];
			m_ww = avg[3]; if(!snap) m_c[3] = avg[3];
		}
		else
		{
			m_zz = m_c[2]; m_ww = m_c[3];
		}
	}

	m_xx = m_c[0]; m_yy = m_c[1]; 

	parent->getParentBestInverseMatrix(true).apply4(m_c);

	m_allowX = m_allowY = 0!=(parent->getButtonsLocked()&BS_Shift);	
}
void MoveTool::mouseButtonMove()
{	
	if(m_selecting)
	{
		m_selecting = false;

		model_status(parent->getModel(),StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Moving"));
	}
	//2021: Reserving BS_Right
	if(!m_left) return;

	double pos[4];
	parent->snap_select = true; 
	parent->snap_object.type = Model::PT_MAX; //2023 //EXPERIMENTAL
	if(!parent->getParentCoords(pos,true))
	{
		//DICEY: m_z snapped to the selected vertex
		//in mouseButtonDown but now that vertex is 
		//ineligible.
		pos[2] = m_zz; pos[3] = m_ww; //m_z
	}
	else if(BS_Alt&parent->getButtons()?m_snap3d:!m_snap3d)
	{
		if(parent->getView()<=Tool::ViewPerspective) //2023
		{
			pos[2] = m_zz; pos[3] = m_ww; //HACK?
		}
		else
		{
			pos[2] = 0; pos[3] = 1;
		}
	}

	//TODO: STANDARDIZE AND REQUIRE MINIMUM PIXELS
	if(m_allowX||m_allowY)
	{
		if(m_allowX&&m_allowY)
		{
			double ax = fabs(pos[0]-m_xx); //m_x
			double ay = fabs(pos[1]-m_yy); //m_y

			if(ax>ay) m_allowY = false;
			if(ay>ax) m_allowX = false;
		}
		
		if(!m_allowX) pos[0] = m_xx; //m_x
		if(!m_allowY) pos[1] = m_yy; //m_y
	}

	parent->getParentBestInverseMatrix(true).apply4(pos); //apply3

	double v[4] = { pos[0]-m_c[0],pos[1]-m_c[1],pos[2]-m_c[2],pos[3]-m_c[3] };

	for(int i=4;i-->0;){ m_c[i] = pos[i]; }

	/*Matrix m; //???
	m.set(3,0,v[0]);
	m.set(3,1,v[1]);
	m.set(3,2,v[2]);*/

	//FIX ME: Translate via vector.
	//parent->getModel()->translateSelected(m);
	parent->getModel()->translateSelected(v);

	parent->updateAllViews(); //Needed
}
void MoveTool::mouseButtonUp()
{
	Model *model = parent->getModel();

	if(m_selecting)
	{
		parent->snapSelect(); //EXPERIMENTAL

		parent->updateAllViews(); //Needed

		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Select complete"));
	}
	else if(m_left)	
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Move complete"));
	}
}		
