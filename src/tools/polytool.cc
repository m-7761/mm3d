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
#include "msg.h"
#include "log.h"
#include "modelstatus.h"

#include "pixmap/polytool.xpm"

struct PolyTool : Tool
{
	PolyTool():Tool(TT_Creator)
	{
		m_type = 0; m_snap = false; //config defaults
	}

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Tool","Create Polygon");
	}

	virtual const char **getPixmap(int){ return polytool_xpm; }

	virtual const char *getKeymap(int){ return "Z"; }

	virtual void activated(int)
	{
		const char *e[3+1] = 
		{
		TRANSLATE_NOOP("Param","Strip","Triangle strip option"),
		TRANSLATE_NOOP("Param","Fan","Triangle fan option"),
		TRANSLATE_NOOP("Param","Nearest","Triangle fan option"), //2020
		};
		parent->addEnum(true,&m_type,TRANSLATE_NOOP("Param","Poly Type"),e);
		parent->addBool(true,&m_snap,TRANSLATE_NOOP("Param","Use Snaps"));
	}

	void mouseButtonDown();		
	void mouseButtonMove();
	void mouseButtonUp();
		
		int m_type; bool m_snap; 

		ToolCoordT m_lastVertex;

		//FIX ME
		//TODO: Add this and lift getParentXYValue(false)
		//constraint. Refer to MoveTool.
		bool m_allowX,m_allowY; double m_x,m_y,m_z;

		bool m_selecting; //2020
};

extern Tool *polytool(){ return new PolyTool; }

void PolyTool::mouseButtonDown()
{
	Model *model = parent->getModel();

	int bt = parent->getButtons();
	
	m_selecting = false; 
	m_allowX = m_allowY = 0!=(bt&BS_Shift);
	if(m_allowX) 
	parent->getParentXYZValue(m_x,m_y,m_z,true);
	
	//EXPANDED BEHAVIOR (2020)
	//
	// Left-click reuses existing vertices, right-click
	// creates new vertices. Shift+left starts over and
	// Shift+right toggles the selection
	//
	parent->snap_select = true;
	parent->snap_object.type = m_snap?Model::PT_MAX:Model::PT_Vertex;
	unsigned &sv = parent->snap_vertex; 
	double pos[3];
	parent->getParentXYZValue(pos[0],pos[1],pos[2],bt&BS_Shift?true:false);
	if(sv==-1||bt==BS_Right&&~bt&BS_Shift) 
	{
		if(bt&BS_Left&&bt&BS_Shift) //starting over?
		{
			model->unselectAllVertices();
		}
		m_lastVertex = addPosition(Model::PT_Vertex,pos[0],pos[1],pos[2]);
	}
	else //2020: reusing vertex?
	{
		makeToolCoord(m_lastVertex,{Model::PT_Vertex,(unsigned)sv});

		if(bt&BS_Shift) //changing selection or dragging vertices?
		{
			if(bt&BS_Left) //starting over?
			{
				model->unselectAllVertices(); //2020
				model->selectVertex(m_lastVertex);		
				parent->updateAllViews();
			}
			else //selecting or dragging?
			{
				m_selecting = true; 
			}

			return; //!
		}
	}
	
	std::vector<int> selected;
	model->getSelectedVertices(selected);
	if(selected.size()>3) 
	{
		//2020: at least show this behavior?
		for(unsigned i=3;i<selected.size();i++)
		model->unselectVertex(selected[i]);

		selected.resize(3); //FIX ME
	}
	
	if(3==selected.size())
	{
		int v; if(2==m_type) //2020
		{
			ToolCoordT cmp;
			double max = -1; v = 1;
			for(unsigned i=0;i<selected.size();i++)
			if(makeToolCoord(cmp,{Model::PT_Vertex,(unsigned)selected[i]}))
			{
				double d = distance(cmp.coords,m_lastVertex.coords);
				if(d>max){ max = d; v = i; }
			}
			else assert(0);
		}
		else v = m_type?1:0;	
		model->unselectVertex(selected[v]);
		selected.erase(selected.begin()+v);
	}
	selected.push_back(m_lastVertex);

	int tri;
	if(3==selected.size())
	tri = model->addTriangle(selected[0],selected[1],selected[2]);	
	model->selectVertex(m_lastVertex);
	if(3==selected.size())
	{
		const Matrix &viewMatrix = 
		parent->getParentBestInverseMatrix();
		//viewMatrix.show(); //???

		Vector viewNorm(0,0,1);
		viewNorm.transform3(viewMatrix);

		double dNorm[3];
		//model->getNormal(tri,0,dNorm); //invalid (at vertex?)
		model->getFlatNormal(tri,dNorm); //2020

		//log_debug("view normal is %f %f %f\n",viewNorm[0],viewNorm[1],viewNorm[2]);
		//log_debug("triangle normal is %f %f %f\n",dNorm[0],dNorm[1],dNorm[2]);

		double d = dot3(viewNorm.getVector(),dNorm);
		//log_debug("dot product is %f\n",d);

		if(d<0) model->invertNormals(tri);
	}	

	parent->updateAllViews();	
}
void PolyTool::mouseButtonMove()
{
	m_selecting = false;

	//if(parent->getButtons()==BS_Left)
	{
		double pos[3];
		parent->getParentXYZValue(pos[0],pos[1],pos[2],false);

		if(m_allowX||m_allowY) //2020
		{
			if(m_allowX&&m_allowY)
			{
				double ax = fabs(pos[0]-m_x);
				double ay = fabs(pos[1]-m_y);

				if(ax>ay) m_allowY = false;
				if(ay>ax) m_allowX = false;
			}

			if(!m_allowX) pos[0] = m_x;
			if(!m_allowY) pos[1] = m_y;
		}

		movePositionUnanimated(m_lastVertex.pos,pos[0],pos[1],pos[2]);

		parent->updateAllViews();
	}
}
void PolyTool::mouseButtonUp()
{
	if(m_selecting)
	{
		Model *model = parent->getModel();

		if(model->isVertexSelected(m_lastVertex))
		model->unselectVertex(m_lastVertex);
		else model->selectVertex(m_lastVertex);
		
		parent->updateAllViews();
	}
}


