/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2004-2007 Kevin Worcester
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not,write to the Free Software
 * Foundation,Inc.,59 Temple Place-Suite 330,Boston,MA 02111-1307,
 * USA.
 *
 * See the COPYING file for full license text.
 */

#include "mm3dtypes.h" //PCH

#include "menuconf.h" //TOOLS_CREATE_MENU

#include "tool.h"

#include "model.h"
#include "msg.h"
#include "log.h"
#include "modelstatus.h"
#include "rotatepoint.h"

#include "pixmap/projtool.xpm"

struct ProjTool : Tool
{
	ProjTool():Tool(TT_Creator)
	{
		m_type = 1; //config defaults
	}
		
	virtual const char *getName(int arg)
	{
		return TRANSLATE_NOOP("Tool","Create Projection");
	}

	virtual const char **getPixmap(int){ return projtool_xpm; }

	virtual const char *getKeymap(int){ return "Shift+F12"; }

	virtual void activated(int)
	{
		const char *e[3+1] = 
		{
		TRANSLATE_NOOP("Param","Cylinder","Cylinder projection type"),
		TRANSLATE_NOOP("Param","Sphere","Sphere projection type"),
		TRANSLATE_NOOP("Param","Plane","Plane projection type"),
		};
		parent->addEnum(true,&m_type,TRANSLATE_NOOP("Param","Poly Type"),e);
	}
	
	void mouseButtonDown();	
	void mouseButtonMove();
	
		int	 m_type;

		double m_orig[3];
		int m_x,m_y;
		bool m_allowX,m_allowY;		
		
		ToolCoordT m_proj;
};

extern Tool *projtool(){ return new ProjTool; }

void ProjTool::mouseButtonDown()
{
	m_x = parent->getButtonX(); m_allowX = true;
	m_y = parent->getButtonY(); m_allowY = true;

	Model *model = parent->getModel();

	model->setDrawProjections(true);

	// use parent view matrix to translate 2D coords into 3D coords
	double pos[4];
	parent->getParentXYValue(pos[0],pos[1],true);
	pos[2] = 0;
	pos[3] = 1;
	const Matrix &m = parent->getParentViewInverseMatrix();
	m.apply(pos);
	for(int i=0;i<3;i++) m_orig[i] = pos[i];

	// Find a unique name for the projection
	char name[64] = "Projection ";
	int nameN = sizeof("Projection ")-1;
	int num = 0;
	int iN = model->getProjectionCount();
	for(int i=0;i<iN;i++)
	{
		const char *cmp = model->getProjectionName(i);
		if(!memcmp(cmp,name,nameN))
		num = std::max(num,atoi(cmp+nameN));
	}
	sprintf(name+nameN,"%d",num+1);

	// Create projection
	m_proj.pos.type = Model::PT_Projection;
	m_proj.pos.index = model->addProjection
	(name,(Model::TextureProjectionTypeE)m_type,pos[0],pos[1],pos[2]);
	/*2020: This wasn't editor friendly.
	//https://github.com/zturtleman/mm3d/issues/114
	double upVec[3]	= { 0,1,0 };
	m.apply3(upVec);
	double seamVec[3] = { 0,0,-1/3.0 };
	m.apply3(seamVec);
	model->setProjectionUp(m_proj,upVec);
	model->setProjectionSeam(m_proj,seamVec);
	model->setProjectionScale(m_proj,1); //???
	*/
	double rot[3];
	parent->getParentViewInverseMatrix().getRotation(rot);
	model->setProjectionRotation(m_proj,rot); 

	// Assign selected faces to projection
	iN = model->getTriangleCount();
	for(int i=0;i<iN;i++)
	if(model->isTriangleSelected(i))
	model->setTriangleProjection(i,m_proj);
	model->applyProjection(m_proj);

	// Make new projection the only thing selected
	model->unselectAll();
	model->selectProjection(m_proj);

	parent->updateAllViews();

	model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Tool","Projection created"));
}
void ProjTool::mouseButtonMove()
{
	if(m_proj.pos.type!=Model::PT_Projection) return;

	Model *model = parent->getModel();

	/*2020: No, rotation semantics make most sense given 
	//how this works!
	//Should tools be responsible for this?
	if(parent->getButtons()&BS_Shift&&m_allowX&&m_allowY)
	{
		double ax = fabs(parent->getButtonX()-m_x);
		double ay = fabs(parent->getButtonY()-m_y);

		if(ax>ay) m_allowY = false;
		if(ay>ax) m_allowX = false;
	}

	//REMOVE ME
	if(!m_allowX) parent->getButtonX() = m_x;
	if(!m_allowY) parent->getButtonY() = m_y;
	*/

	// Convert 2D coords to 3D in parent's view space
	double pos[4];
	parent->getParentXYValue(pos[0],pos[1]);
	pos[2] = 0;
	pos[3] = 1;
	parent->getParentViewInverseMatrix().apply(pos);
	pos[0]-=m_orig[0];
	pos[1]-=m_orig[1];
	pos[2]-=m_orig[2];
	pos[3] = 1;

	/*2020: This wasn't editor friendly.
	//https://github.com/zturtleman/mm3d/issues/114
	//WORKS BECAUSE "seam" IS ORTHOGONAL
	// Set the up vector to wherever the mouse is	
	model->setProjectionUp(m_proj,pos);
	*/	
	int xDiff = parent->getButtonX()-m_x;
	int yDiff = parent->getButtonY()-m_y;
	double angle = rotatepoint_diff_to_angle(xDiff,yDiff);
	if(parent->getButtons()&BS_Shift) 
	angle = rotatepoint_adjust_to_nearest(angle,15); //NEW
	//NOTE: I'm not sure what convention makes since here, but this 
	//one matches the original convention of dragging the mouse down
	//making the (front/identity) projection upside down.
	Matrix m; m.setRotation({0,0,-PI/2+angle});	
	//HACK: Don't call applyProjection twice?
	(m*parent->getParentViewInverseMatrix()).getRotation
	(model->getPositionObject({Model::PT_Projection,m_proj})->m_rot);
	//REMINDER: Does applyProjection indirectly.
	model->setProjectionScale(m_proj,mag3(pos));
	
	parent->updateAllViews();
}
