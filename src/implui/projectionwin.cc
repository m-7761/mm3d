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

#include "projectionwin.h"
#include "viewwin.h"

#include "log.h"
#include "msg.h"
#include "modelstatus.h"

static void projection_reshape(Win::ui *ui, int x, int y)
{
	assert(!ui->hidden()); //DEBUGGING

	auto *w = (ProjectionWin*)ui;
	auto &r = w->_reshape;

	if(!ui->seen())
	{
		r[0] = x;
		r[1] = y; return;
	}
	else w->scene.lock(0,0);

	if(x<r[0]) x+=r[0]-x; 
	if(y<r[1]) y+=r[1]-y; 

	w->_main_panel.lock(x,y);
}

void ProjectionWin::init()
{	
	//TESTING: These are TextureCoordWin's 
	//dimensions ATM.
	//scene.lock(352,284);	
	scene.lock(false,284);
	reset.ralign();
	reshape_callback = projection_reshape;

	type.add_item(0,"Cylinder");
	type.add_item(1,"Sphere");
	type.add_item(2,"Plane");

	// TODO handle undo of select/unselect?
	// Can't do this until after constructor is done because of observer interface
	//setModel(model);
	texture.setModel(model);
	texture.setInteractive(true);
	texture.setMouseOperation(texture::MouseRange);
	texture.setDrawVertices(false);
	//texture.setMouseTracking(true); //QWidget //???
}
void ProjectionWin::submit(int id)
{
	int p = projection; switch(id)
	{	
	case id_item:
	{
		type.select_id(model->getProjectionType(p));

		double x,y,xx,yy;
		model->getProjectionRange(p,x,y,xx,yy);
		texture.setRange(x,y,xx,yy);

		goto refresh;
	}
	case id_subitem:
	
		model->setProjectionType(p,type);
		model->operationComplete(::tr("Set Projection Type","operation complete"));
		goto refresh;	
	
	case '+': texture.zoomIn(); break;
	case '-': texture.zoomOut(); break;
	case '=': texture.setZoomLevel(zoom.value); break;
		
	case id_name:
	{
		std::string name = model->getProjectionName(p);
		if(id_ok==EditBox(&name,::tr("Rename projection","window title"),
		::tr("Enter new point name:"),1,Model::MAX_NAME_LEN))		
		{
			model->setProjectionName(p,name.c_str());
			model->operationComplete(::tr("Rename Projection","operation complete"));
		}
		break;
	}
	case id_apply: //???
		
		model->applyProjection((int)projection);
		model->operationComplete(::tr("Apply Projection","operation complete"));
		break;

	case id_reset:
	
		model->setProjectionRange(p,0,0,1,1);
		model->operationComplete(::tr("Reset UV Coordinates","operation complete"));		break;

	case id_remove: p = -1; 
	case id_append:
	
		model->setTrianglesProjection(model.fselection,p);
		if(p!=-1) model->applyProjection(p);
		model->operationComplete(::tr("Set Triangle Projection","operation complete"));
	
		refresh: 
		refreshProjectionDisplay();
		break;

	case id_scene:

		texture.draw(scene.x(),scene.y(),scene.width(),scene.height());
		break;

	case id_close:

		return hide();
	}

	basic_submit(id);
}

void ProjectionWin::open()
{
	setModel();
	
	// If we are visible, setModel already did this
	if(hidden())
	{
		//REMINDER: GLX needs to show before
		//it can use OpenGL.
		show(); openModel();
	}
}
void ProjectionWin::setModel()
{				  
	texture.setModel(model);

	if(!hidden()) openModel();
}
void ProjectionWin::modelChanged(int changeBits)
{	
	int p = projection;
	int n = model->getProjectionCount();

	if(changeBits&Model::AddOther) //2022
	{
		// TODO need some way to re-select the projection we were looking at
		if(!hidden())
		if(n!=projection.find_line_count())
		{
			// A projection was added or deleted, we need to select a new
			// projection and re-initialize everything. 
			openModel(); return;
		}
		else if(n) //selection()-> crashes on 0.
		{
			// a change to projection itself, or a non-projection change, just 
			// re-initialize the projection display for the current projection			
			projection.selection()->set_text(model->getProjectionName(p));		
		}
	}
	if(changeBits&Model::MoveOther) //2022
	{
		type.select_id(model->getProjectionType(p));
	}
	//NOTE: This is a kludge but all range operations call
	//this anyway, so when projections are reassigned then
	//setTriangleProjection sets MoveTexture so this isn't
	//running on every single change!
	if(changeBits&Model::MoveTexture) //2022
	{
		addProjectionTriangles();
	}
}
void ProjectionWin::openModel()
{
	projection.select_id(-1).clear();

	int iN = model?model->getProjectionCount():0;	
	if(!enable(iN!=0).enabled())
	{
		ok.nav.enable(); return;
	}
	
	for(int i=0;i<iN;i++)
	projection.add_item(i,model->getProjectionName(i));

	int p = -1;
	for(int i=0;i<iN;i++)
	if(model->isProjectionSelected(i))
	{
		p = i; break;
	}
	if(p==-1)
	for(int i:model.fselection)
	{
		int pp = model->getTriangleProjection(i);
		if(pp<0) continue;
		p = pp; break;
	}
	projection.select_id(p==-1?0:p); submit(id_item);
}

void ProjectionWin::refreshProjectionDisplay()
{
	texture.clearCoordinates();

	if(!projection.empty())
	{			
		int iN = model->getTriangleCount();
		int proj = projection;
		for(int i=0;i<iN;i++)
		{
			int p = model->getTriangleProjection(i);
			int g = model->getTriangleGroup(i);
			int material = model->getGroupTextureId(g);
			if(p==proj&&g>=0&&material>=0)
			{
				texture.setTexture(material); 
				return addProjectionTriangles();
			}
		}
	}
	texture.setTexture(-1);

	//REFERENCE
	//DecalManager::getInstance()->modelUpdated(model); //???
}

void ProjectionWin::addProjectionTriangles()
{
	texture.clearCoordinates();

	int p = projection;

	double x,y,xx,yy;
	model->getProjectionRange(p,x,y,xx,yy);
	texture.setRange(x,y,xx,yy);

	float s,t;
	int iN = model->getTriangleCount();
	for(int i=0,v=0;i<iN;i++)	
	if(p==model->getTriangleProjection(i))
	{
		for(int j=0;j<3;j++)
		{				
			model->getTextureCoords(i,j,s,t);
			texture.addVertex(s,t);
		}
		texture.addTriangle(v,v+1,v+2); v+=3;
	}
	texture.updateWidget();

	//REFERENCE
	//DecalManager::getInstance()->modelUpdated(model); //???
}

void ProjectionWin::rangeChanged(bool done)
{
	int p = (int)projection; assert(p>=0);

	if(!done)
	{
		double x,y,xx,yy; texture.getRange(x,y,xx,yy);
		model->setProjectionRange(p,x,y,xx,yy);

		//HACK? This is also triggering modelChanged to
		//call addProjectionTriangles(). 
		//NOTE: Something like Tool::Parent::updateView
		//on texture views only would be an improvement.		
		model->updateObservers();
	}

	if(done)
	model->operationComplete(::tr("Move Projection Range","operation complete"));
}
void ProjectionWin::seamChanged(double xDiff, double yDiff, bool done)
{
	//NOTE: I've changed xDiff on Done to the total
	//for better or worse, but this still has to
	//run in real time for now.
	if(!done)
	{
		if(xDiff==0) return; (void)yDiff; //???

		int p = (int)projection; assert(p>=0);

		double rot[3],up[3] = { 0,1,0 };
		model->getProjectionRotation(p,rot);
		Matrix a,b;
		a.setRotation(rot);
		a.apply3(up);
		b.setRotationOnAxis(up,xDiff); (void)yDiff; //???
		(a*b).getRotation(rot);
		model->setProjectionRotation(p,rot);

		//HACK? This is also triggering modelChanged to
		//call addProjectionTriangles(). 
		//NOTE: Something like Tool::Parent::updateView
		//on texture views only would be an improvement.		
		model->updateObservers();
	}
	if(done)
	model->operationComplete(::tr("Rotate Projection Seam","operation complete"));
}