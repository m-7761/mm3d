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

#include "menuconf.h"
#include "tool.h"


#include "log.h"
#include "modelstatus.h"

#include "pixmap/selectvertextool.xpm"
#include "pixmap/selectfacetool.xpm"
#include "pixmap/selectconnectedtool.xpm"
#include "pixmap/selectgrouptool.xpm"
#include "pixmap/selectbonetool.xpm"
#include "pixmap/selectpointtool.xpm"
#include "pixmap/selectprojtool.xpm"

class SelectTool : public Tool
{
public:

	SelectTool()
	:Tool(TT_SelectTool,7,TOOLS_SELECT_MENU)	
	{
		//m_boundingBox.tool = this;

		m_includeBackfacing = true; //config defaults
	}

	enum
	{
	Meshes=0, //CFVGB key order...
	Faces,
	Vertices,
	Groups,
	Joints,
	Points,
	Projections, //6
	};

	virtual const char *getName(int arg)
	{	
		switch(arg)
		{
		default: assert(0);		
		case 0: return TRANSLATE_NOOP("Tool","Select Connected Mesh");
		case 1: return TRANSLATE_NOOP("Tool","Select Faces");
		case 2: return TRANSLATE_NOOP("Tool","Select Vertices");		
		case 3: return TRANSLATE_NOOP("Tool","Select Groups");
		case 4:	return TRANSLATE_NOOP("Tool","Select Bone Joints");
		case 5: return TRANSLATE_NOOP("Tool","Select Points");
		case 6: return TRANSLATE_NOOP("Tool","Select Projections");
		}
	}

	virtual const char **getPixmap(int arg)
	{
		switch(arg)
		{
		default: assert(0);
		case 0: return selectconnectedtool_xpm;
		case 1: return selectfacetool_xpm; 		
		case 2: return selectvertextool_xpm; 
		case 3: return selectgrouptool_xpm;
		case 4:	return selectbonetool_xpm;
		case 5: return selectpointtool_xpm; 
		case 6: return selectprojtool_xpm;
		}		
	}

	virtual const char *getKeymap(int arg)
	{
		switch(arg)
		{
		default: assert(0);
		case 0: return "C";
		case 1: return "F";
		case 2: return "V";
		case 3: return "G";
		case 4: return "B";
		case 5: return "O"; //"T"
		case 6: return "P"; //NEW
		}
	}
	
	enum{ no_draw=-10000 };

	virtual void activated(int arg)
	{
		m_startX = no_draw;

		m_op = arg;
		if(m_op==Faces)
		parent->addBool(true,&m_includeBackfacing,
		TRANSLATE_NOOP("Param","Include Back-facing"));

		//NEW: InvertSelectionCommand depends on this
		//https://github.com/zturtleman/mm3d/issues/145
		set_selection_mode();
	}
		
	virtual void mouseButtonDown();
	virtual void mouseButtonMove()
	{	
		//Note: Doing this in mouseButtonDown can look
		//bad in some modes.
		if(!m_unselect&&~parent->getButtonsLocked()&BS_Shift)
		{
			parent->getModel()->unselectAll(); //OVERKILL
		}

		parent->getRawParentXYValue(m_x2,m_y2);
		parent->updateView();
	}
	virtual void mouseButtonUp();
	
		int m_op;

		bool m_includeBackfacing;
   
		bool m_unselect;		

		int m_startX,m_startY;

		double m_x1,m_y1,m_x2,m_y2;
		
		//struct Box : Decal
		//{	
		//	SelectTool *tool;

			virtual void draw(bool focused);	

		//}m_boundingBox;

	void set_selection_mode()
	{
		Model *model = parent->getModel();

		Model::SelectionModeE sm; switch(m_op)
		{
		case Vertices: sm = Model::SelectVertices; break;
		case Faces: sm = Model::SelectTriangles; break;
		case Meshes: sm = Model::SelectConnected; break;
		case Groups: sm = Model::SelectGroups; break;
		case Joints: 
		
			model->setDrawJoints(true);

			sm = Model::SelectJoints; break;
	
		case Points: sm = Model::SelectPoints; break;
		case Projections: sm = Model::SelectProjections; //break;
		
			if(!model->getDrawProjections()) //NEW
			model->setDrawProjections(true); break;
		}
		model->setSelectionMode(sm);
	}
};

extern Tool *selecttool(){ return new SelectTool; }

void SelectTool::mouseButtonDown()
{
	Model *model = parent->getModel();

		set_selection_mode();

	m_unselect = (parent->getButtons()&BS_Right)!=0;

	m_startX = parent->getButtonX(); 
	m_startY = parent->getButtonY();

	parent->getRawParentXYValue(m_x1,m_y1);
	m_x2 = m_x1;
	m_y2 = m_y1;
	
	parent->updateView();
	
	model_status(model,StatusNormal,STATUSTIME_SHORT,
	TRANSLATE("Tool","Starting selection"));
}
void SelectTool::mouseButtonUp()
{
	Model *model = parent->getModel();

	//EXPERIMENTAL
	if(m_x1==m_x2&&m_y1==m_y2) //2020
	{
		if(!m_unselect&&~parent->getButtonsLocked()&BS_Shift)
		{
			model->unselectAll(); //OVERKILL
		}
		Model::PositionTypeE e; switch(m_op)
		{
		case Vertices: e = Model::PT_Vertex; break;	
		case Joints: e = Model::PT_Joint; break;	
		case Points: e = Model::PT_Point; break;
	//	case Projections: e = Model::PT_Projection; break;
		default: e = Model::PT_MAX;
		}
		if(e!=Model::PT_MAX)
		{
			parent->snap_select = true;
			auto &pos = parent->snap_object;
			pos.type = e;
			double coords[2];
			parent->getParentXYValue(coords[0],coords[1],true);
			if(-1!=pos) 
			model->selectPosition(pos,!m_unselect);
			model_status(model,StatusNormal,STATUSTIME_SHORT,
			TRANSLATE("Tool","Selection complete"));
			return;
		}
	}

	//FIX ME (BROKEN)
	//https://github.com/zturtleman/mm3d/issues/62#issuecomment-521525933
	/*What is this? It's blocking RemoveDecal.
	if(m_unselect) //???
	{
		// We're waiting for the right button
		if(parent->getButtons()&BS_Left) return;
	}
	else
	{
		// We're waiting for the left button
		if(parent->getButtons()&BS_Right) return;
	}*/
		
	struct BackFaceTest : Model::SelectionTest
	{	
		double cmp[3];
		const Model::Vertex *const *vl;
		virtual bool shouldSelect(void *element)
		{			
			auto tri = (Model::Triangle*)element;			
			double dp = 0; if(vl) //Perspective accurate?
			{
				double *v = vl[tri->m_vertexIndices[0]]->m_absSource;
				for(int i=0;i<3;i++) dp+=(cmp[i]-v[i])*tri->m_flatSource[i];
			}
			else for(int i=0;i<3;i++) dp+=cmp[i]*tri->m_flatSource[i];			
			return dp>0;
		}

	}ntest;
	Model::SelectionTest *test = nullptr;	
	if(m_op==Faces&&!m_includeBackfacing)
	{	
		test = &ntest;
		if(parent->getView()<=ViewPerspective) //2020: new way
		{
			//NOTE: getParentBestInverseMatrix is a projected matrix.
			const double *eye = parent->getParentViewInverseMatrix().getVector(3);
			memcpy(ntest.cmp,eye,sizeof(ntest.cmp));

			ntest.vl = model->getVertexList().data();			
		}
		else //old way (fewer operations in orthographic modes)
		{
			ntest.vl = nullptr; //HACK: Use orthographic mode.
			ntest.cmp[0] = 0;
			ntest.cmp[1] = 0;
			ntest.cmp[2] = 1;
			parent->getParentViewInverseMatrix().apply3(ntest.cmp);
		}			
	}

	auto mf = m_unselect? //FIX ME
	&Model::unselectInVolumeMatrix:&Model::selectInVolumeMatrix;
	
	parent->getRawParentXYValue(m_x2,m_y2);
	(model->*mf)(parent->getParentBestMatrix(),m_x1,m_y1,m_x2,m_y2,test);

	m_startX = no_draw;
	
	model_status(model,StatusNormal,STATUSTIME_SHORT,
	TRANSLATE("Tool","Selection complete"));
}

void SelectTool::draw(bool focused)
{	
	if(!focused||m_startX==no_draw) return;

	//TESTING
	//-0.20/0.25 happen to be nearly pixel
	//perfect at 1x zoom. But it'd be best
	//to work in 2D.
	bool persp_test = true;
	double w = persp_test?0.25:1;  
	double z = persp_test?-0.20:0;
	double p[4][4] = 
	{
		{m_x1,m_y1,z,1}, {m_x2,m_y1,z,1},
		{m_x2,m_y2,z,1}, {m_x1,m_y2,z,1}
	};
	if(parent->getView()<=Tool::ViewPerspective) 
	{
		if(persp_test) 
		{
			//This shouldn't be done like this, but
			//I want to see if it works.
			w*=parent->_getParentZoom();

			//Homogeneous component?
			for(double *pt=*p;pt<p[3]+4;pt++) *pt*=w;
		}
		else if(0) return;
	}
	
	const Matrix &inv = parent->getParentBestInverseMatrix();
	for(int i=0;i<4;i++) inv.apply(p[i]);

	glEnable(GL_COLOR_LOGIC_OP);

	//glColor3ub(255,255,255);
	glColor3ub(0x80,0x80,0x80);
	glLogicOp(GL_XOR);	

	glBegin(GL_LINES);
	glVertex3dv(p[0]); glVertex3dv(p[1]);
	glVertex3dv(p[1]); glVertex3dv(p[2]);
	glVertex3dv(p[2]); glVertex3dv(p[3]);
	glVertex3dv(p[3]); glVertex3dv(p[0]);
	glEnd();

	//glLogicOp(GL_COPY); //???
	glDisable(GL_COLOR_LOGIC_OP);
}
