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
		TRANSLATE_NOOP("Param","Nearest",""), //2020
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
		bool m_allowX,m_allowY; 
		
		double m_xx,m_yy;

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
	parent->getParentXYValue(m_xx,m_yy,true);
	
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
//	parent->getParentXYZValue(pos[0],pos[1],pos[2],bt&BS_Shift?true:false); //???
	parent->getParentXYZValue(pos[0],pos[1],pos[2],true);
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
	
	int_list sel;
	model->getSelectedVertices(sel);
	{
		//2022: this wasn't actually working
		//unless the vertices were brand new.
		auto &vl = model->getVertexList();
		if(sel.size()>1);
		std::sort(sel.begin(),sel.end(),[&](int a, int b)
		{
			return vl[a]->getOrderOfSelection()<vl[b]->getOrderOfSelection();
		});
	}
	if(sel.size()>3) 
	{
		//2020: at least show this behavior?
		for(unsigned i=sel.size()-3;i-->0;)
		model->unselectVertex(sel[i]);
		sel.erase(sel.begin(),sel.end()-3);
	}
	
	if(3==sel.size())
	{
		int v; if(2==m_type) //2020
		{
			ToolCoordT cmp;
			double max = -1; v = 1;
			for(unsigned i=0;i<sel.size();i++)
			if(makeToolCoord(cmp,{Model::PT_Vertex,(unsigned)sel[i]}))
			{
				double d = distance(cmp.coords,m_lastVertex.coords);
				if(d>max){ max = d; v = i; }
			}
			else assert(0);
		}
		else v = m_type?1:0;	
		model->unselectVertex(sel[v]);
		sel.erase(sel.begin()+v);
	}
	sel.push_back(m_lastVertex);

	if(3==sel.size())
	{
		auto *verts = (unsigned*)(sel.data()); //C++

		if(tool_option_face_view) //EXPERIMENTAL
		{
			Vector viewNorm(0,0,1),normal;
			viewNorm.transform3(parent->getParentBestInverseMatrix());
			model->calculateFlatNormal(verts,normal.getVector());
			if(viewNorm.dot3(normal)<0)
			std::swap(verts[1],verts[2]);
		}
		else if(!Model::Vertex::getOrderOfSelectionCCW()) //EXPERIMENTAL
		{
			std::reverse(verts,verts+3); //GOOD ENOUGH?
		}

		model->addTriangle(verts[0],verts[1],verts[2]);	
	}	

	model->selectVertex(m_lastVertex);

	parent->updateAllViews();	
}
void PolyTool::mouseButtonMove()
{
	m_selecting = false;

	//if(parent->getButtons()==BS_Left)
	if(parent->getView()>ViewPerspective)
	{
		double pos[3];
		parent->getParentXYZValue(pos[0],pos[1],pos[2],false);

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

		movePositionUnanimated(m_lastVertex.pos,pos[0],pos[1],pos[2]);

		parent->updateAllViews();
	}
	else model_status(parent->getModel(),StatusNotice,STATUSTIME_SHORT,
	TRANSLATE("Tool","Unable to Move in Perspective view"));
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


