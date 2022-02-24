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
	MoveTool():Tool(TT_Other){}

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
	}
	
	virtual void mouseButtonDown();
	virtual void mouseButtonMove();
	virtual void mouseButtonUp();

		bool m_selecting,m_left;

		double m_x,m_y,m_z;

		bool m_allowX,m_allowY; double m_xx,m_yy,m_zz;
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
	parent->getParentXYZValue(m_x,m_y,m_z,true);

	m_allowX = m_allowY = 0!=(parent->getButtonsLocked()&BS_Shift);

	m_xx = m_x; m_yy = m_y; m_zz = m_z; //translateSelected?	
	
	model_status(parent->getModel(),StatusNormal,STATUSTIME_SHORT,
	TRANSLATE("Tool","Moving selected primitives"));
}
void MoveTool::mouseButtonMove()
{	
	m_selecting = false;
	//2021: Reserving BS_Right
	if(!m_left) return;

	double pos[3];
	if(!parent->getParentXYZValue(pos[0],pos[1],pos[2],false))
	{
		//DICEY: m_z snapped to the selected vertex
		//in mouseButtonDown but now that vertex is 
		//ineligible.
		pos[2] = m_zz; //m_z
	}

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

	double v[3] = { pos[0]-m_x,pos[1]-m_y,pos[2]-m_z };

	m_x = pos[0]; m_y = pos[1]; m_z = pos[2];

	parent->getParentBestInverseMatrix().apply3(v);

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
	}
	else if(m_left)	
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Move complete"));
	}

	parent->updateAllViews(); //Needed
}		
