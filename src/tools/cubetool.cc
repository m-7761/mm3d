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

#include "pixmap/cubetool.xpm"
#include "menuconf.h" //TOOLS_CREATE_MENU

#include "tool.h"
#include "model.h"
#include "weld.h"
#include "modelstatus.h"

struct CubeTool : Tool
{
	CubeTool():Tool(TT_Creator,1,TOOLS_CREATE_MENU),m_tracking(),m_created()
	{
		m_isCube = false; m_segments = 1; //config defaults
	}
		
	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Tool","Create Cube"); 
	}

	virtual const char **getPixmap(int){ return cubetool_xpm; }
 
	virtual const char *getKeymap(int){ return "Shift+F6"; }

	virtual void activated(int)
	{
		parent->addBool(true,&m_isCube,TRANSLATE_NOOP("Param","Cube"));
		parent->addInt(true,&m_segments,TRANSLATE_NOOP("Param","Segments"),1,32); //25
	}

	virtual void mouseButtonDown();
	virtual void mouseButtonMove();
	virtual void mouseButtonUp();

		bool m_isCube; 
		int m_segments;

		bool m_tracking;
		bool m_invertedNormals;

		ToolCoordList m_vertices;
		int_list m_triangles;

		double m_x1,m_y1,m_z1;

		void create();

		bool m_created;

	void updateVertexCoords
	(double x1, double y1, double z1, double x2, double y2, double z2);
};

extern Tool *cubetool(){ return new CubeTool; }

static void cubetool_cubify(bool isCube, double &coord, double &diff_d, double &diff_s1, double &diff_s2)
{
	if(isCube)	
	{
		bool cond = diff_s1<0&&diff_s2>0||diff_s1>0&&diff_s2<0;

		if(fabs(diff_s1)>fabs(diff_s2))
		{
			diff_s2 = cond?-diff_s1:diff_s1;
		}
		else diff_s1 = cond?-diff_s2:diff_s2;
	}

	if(!diff_d||isCube) 
	{
		coord = fabs(diff_s1/2); diff_d = -fabs(diff_s1); //???
	}	
}

void CubeTool::mouseButtonDown()
{
	if(m_tracking) return; //???

	m_created = false;
	m_tracking = true;
	m_invertedNormals = false;

	parent->getParentXYZValue(m_x1,m_y1,m_z1,true);

	Model *model = parent->getModel();

	model->unselectAll();
}
void CubeTool::create()
{
	m_created = true;

	Model *model = parent->getModel();

	double x1,x2,y1,y2,z;

	int xindex = 0;
	int yindex = 1;
	int zindex = 2;

	for(unsigned side=0;side<6;side++)
	{
		if(side&1)
		{
			x1 = 1; x2 = 0;
			y1 = 1; y2 = 0; z  = 1;
		}
		else
		{
			x1 = 0; x2 = 1;
			y1 = 1; y2 = 0; z  = 0;
		}

		switch(side)
		{
		case 0: case 1:
			xindex = 0; yindex = 1; zindex = 2;
			break;
		case 2: case 3:
			xindex = 2; yindex = 1; zindex = 0;
			break;
		case 4: case 5:
			xindex = 0; yindex = 2; zindex = 1;
			break;
		}

		double coord[3];
		for(int y=0;y<=m_segments;y++)
		{
			for(int x=0;x<=m_segments;x++)
			{
				coord[xindex] = x1+(x2-x1)*(double)x/(double)m_segments;
				coord[yindex] = y1+(y2-y1)*(double)y/(double)m_segments;
				coord[zindex] = z;
				ToolCoordT tc = addPosition(Model::PT_Vertex,coord[0],coord[1],coord[2]);
						
				//log_debug("adding vertex %d at %f,%f,%f\n",tc.pos.index,tc.coords[0],tc.coords[1],tc.coords[2]);

				m_vertices.push_back(tc);
			}

			if(y>0)
			{
				int row1 = m_vertices.size()-(m_segments+1)*2;
				int row2 = m_vertices.size()-(m_segments+1);
				for(int x=0;x<m_segments;x++)
				{
					//log_debug("%d,%d,%d,%d\n",row1+x,row1+x+1,row2+x,row2+x+1);

					int a = m_vertices[row2+x], b = m_vertices[row1+x];
					if(side>=2) std::swap(a,b); //invertNormal?
					int t1 = model->addTriangle(a,m_vertices[row1+x+1],b);
					a = m_vertices[row2+x], b = m_vertices[row1+x+1];
					if(side>=2) std::swap(a,b); //invertNormal?
					int t2 = model->addTriangle(a,m_vertices[row2+x+1],b);
					m_triangles.push_back(t1);
					m_triangles.push_back(t2);

					//if(side>=2) model->invertNormal(t1);
					//if(side>=2) model->invertNormal(t2);
				}
			}
		}
	}

	for(auto&ea:m_vertices)
	{
		model->selectVertex(ea.pos.index);
	}

	//updateVertexCoords(m_x1,m_y1,m_z1,m_x1,m_y1,m_z1);

	parent->updateAllViews();

	model_status(model,StatusNormal,STATUSTIME_SHORT,
	TRANSLATE("Tool","Cube created"));
}

void CubeTool::mouseButtonUp()
{
	if(!m_tracking||!m_created) return; //???

	m_tracking = false;

	weldSelectedVertices(parent->getModel());
	parent->updateAllViews();

	m_vertices.clear(); m_triangles.clear();
}

void CubeTool::mouseButtonMove()
{
	if(!m_tracking) return; //???
	
	double x2,y2,z2;
	parent->getParentXYZValue(x2,y2,z2);

	if(x2==m_x1||y2==m_y1)
	{
		return;
	}
	else if(!m_created) create(); 

	updateVertexCoords(m_x1,m_y1,m_z1,x2,y2,z2);
	parent->updateAllViews();
}

void CubeTool::updateVertexCoords
(double x1, double y1, double z1, double x2, double y2, double z2)
{
	Model *model = parent->getModel();
	bool invert = false;

	double xdiff = x2-x1;
	double ydiff = y2-y1;
	double zdiff = z2-z1;

	cubetool_cubify(m_isCube,z1,zdiff,xdiff,ydiff);

	if(y1<y2) invert = !invert;
	if(x2>x1) invert = !invert;
//	if(z2>z1) invert = !invert;

	for(auto it=m_vertices.begin();it!=m_vertices.end();it++)
	{
		movePositionUnanimated(it->pos,
				it->coords[0]*xdiff+x1,
				it->coords[1]*ydiff+y1,
				it->coords[2]*zdiff+z1);
	}

	if(m_invertedNormals!=invert)
	{
		m_invertedNormals = invert;
		
		//It's not necessary to undo this and it
		//costs a lot to remove double entries.
		bool ue = model->setUndoEnabled(false);
		{
			model->invertNormals(m_triangles);
		}
		model->setUndoEnabled(ue);
	}
}
