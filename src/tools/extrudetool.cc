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
#include "extrudecmd.h"

#include "pixmap/extrudetool.xpm"

#include "glmath.h"
#include "model.h"
#include "modelstatus.h"
#include "log.h"

struct ExtrudeTool : Tool
{
	ExtrudeTool():Tool(TT_Creator){} //TT_Manipulator

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Tool","Extrude Faces"); //"Extrude"
	}

	virtual const char *getKeymap(int){ return "X"; }

	virtual const char **getPixmap(int){ return extrudetool_xpm; }

	virtual void activated(int)
	{
		model_status(parent->getModel(),StatusNormal,STATUSTIME_NONE,
		TRANSLATE("Tool","Tip: Hold shift to restrict movement to one dimension"));
	}

	virtual void mouseButtonDown();
	virtual void mouseButtonMove();

	//REMOVE ME
	virtual void mouseButtonUp()
	{
		model_status(parent->getModel(),StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Extrude complete"));
		parent->updateAllViews();
	}
		double m_x,m_y,m_xx,m_yy;

		bool m_allowX,m_allowY;
	
		ExtrudeImpl m_impl;
};

extern Tool *extrudetool(){ return new ExtrudeTool; }

void ExtrudeTool::mouseButtonDown()
{
	Model *model = parent->getModel();

	int bs = parent->getButtonsLocked();

	m_allowX = m_allowY = 0!=(bs&BS_Shift);

	parent->getParentXYValue(m_xx,m_yy,true);

	m_x = m_xx; m_y = m_yy;
		
	// TODO widget for back-facing
	if(!m_impl.extrude(model,0,0,0,false))
	{
		model_status(model,StatusError,STATUSTIME_LONG,
		TRANSLATE("Tool","Must have faces selected to extrude"));
	} 
	else 
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Extruding selected faces"));
	}
}
void ExtrudeTool::mouseButtonMove()
{
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

	double v[4] = { pos[0]-m_x,pos[1]-m_y,0,1 };

	m_x = pos[0]; m_y = pos[1];

	parent->getParentBestInverseMatrix().apply3(v);

	/*Matrix m; //???
	m.set(3,0,v[0]);
	m.set(3,1,v[1]);
	m.set(3,2,v[2]);*/

	//FIX ME: Translate via vector.
	//parent->getModel()->translateSelected(m);
	parent->getModel()->translateSelected(v);
	parent->updateAllViews();
}



