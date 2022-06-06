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

#include "glheaders.h"
#include "texwidget.h"

#include "tool.h" //ButtonState
#include "rotatepoint.h"
#include "glmath.h" //distance

#include "texture.h"
#include "model.h"
#include "log.h"
#include "modelstatus.h"

#include "pixmap/arrow.xpm"
#include "pixmap/crosshairrow.xpm"

//#define VP_ZOOMSCALE 0.75
static const double scrollwidget_zoom = 0.75;

static struct
{
	enum{ SCROLL_SIZE = 16 };

	int x, y;
	int texIndex;
	float s1,t1;
	float s2,t2;
	float s3,t3;
	float s4,t4;

}scrollwidget[ScrollWidget::ScrollButtonMAX] =
{
	{ -18,-18,1,  0,0,  1,0,  1,1, 0,1  }, //Pan
	{ -52,-18,0,  0,1,  0,0,  1,0, 1,1  }, //Left
	{ -35,-18,0,  0,0,  0,1,  1,1, 1,0  }, //Right
	{ -18,-35,0,  0,0,  1,0,  1,1, 0,1  }, //Up
	{ -18,-52,0,  0,1,  1,1,  1,0, 0,0  }, //Down
};

static std::pair<int,ScrollWidget*> 
scrollwidget_scroll,texwidget_3dcube;
void ScrollWidget::cancelOverlayButtonTimer() //HACK
{
	scrollwidget_scroll.first = 0;
}
static void scrollwidget_scroll_timer(int id)
{
	if(id==scrollwidget_scroll.first)
	scrollwidget_scroll.second->pressOverlayButton(id<0);
}
ScrollWidget::~ScrollWidget()
{
	scrollwidget_scroll.first = 0; //Timer business.
	texwidget_3dcube.first = 0;
}
ScrollWidget::ScrollWidget(double zmin, double zmax)
	:
	uid(),
m_activeButton(),
m_viewportX(),m_viewportY(),
m_viewportWidth(),m_viewportHeight(),
m_zoom(1),
m_zoom_min(zmin),m_zoom_max(zmax),
m_scroll(),
m_nearOrtho(-1),m_farOrtho(1),
m_interactive(),
m_uvSnap(),
m_overlayButton(ScrollButtonMAX)
{}

void ScrollWidget::scrollUp()
{
	m_scroll[1]+=m_zoom*0.10; updateViewport('p');
}
void ScrollWidget::scrollDown()
{
	m_scroll[1]-=m_zoom*0.10; updateViewport('p');
}
void ScrollWidget::scrollLeft()
{
	m_scroll[0]-=m_zoom*0.10; updateViewport('p');
}
void ScrollWidget::scrollRight()
{
	m_scroll[0]+=m_zoom*0.10; updateViewport('p');
}
void ScrollWidget::rotateUp()
{
	rotateViewport(-15*PIOVER180,0,0);
}
void ScrollWidget::rotateDown()
{
	rotateViewport(15*PIOVER180,0,0);
}
void ScrollWidget::rotateLeft()
{
	rotateViewport(0,-15*PIOVER180,0);
}
void ScrollWidget::rotateRight()
{
	rotateViewport(0,15*PIOVER180,0);
}
void ScrollWidget::rotateClockwise()
{
	rotateViewport(0,0,-15*PIOVER180);
}
void ScrollWidget::rotateCounterClockwise()
{
	rotateViewport(0,0,15*PIOVER180);
}
void ScrollWidget::zoomIn()
{
	setZoomLevel(m_zoom*scrollwidget_zoom);
}
void ScrollWidget::zoomOut()
{
	setZoomLevel(m_zoom/scrollwidget_zoom);
}
void ScrollWidget::setZoomLevel(double z)
{		
	//Does this include navigation? If so it
	//needs to block scrollUp, etc.
	//if(!m_interactive) return;

	z = std::min(std::max(z,m_zoom_min),m_zoom_max);

	if(m_zoom!=z) 
	{
		m_zoom = z;
		
		//QString zoomStr;
		//zoomStr.sprintf("%f",m_zoom);		
		//emit zoomLevelChanged(zoomStr);
		//emit(this,zoomLevelChanged,m_zoom);
		updateViewport('z');
	}
}
void ScrollWidget::zoom(bool in, double x, double y)
{
	double z = m_zoom;	
	if(in) z*=scrollwidget_zoom; else z/=scrollwidget_zoom;		
	z = std::min(std::max(z,m_zoom_min),m_zoom_max);
	if(m_zoom!=z) 
	{
		double zDiff = z/m_zoom;

		x-=m_scroll[0]; m_scroll[0]-=x*zDiff-x; 
		y-=m_scroll[1]; m_scroll[1]-=y*zDiff-y;
		
		m_zoom = z; updateViewport('z');
	}
}

void ScrollWidget::drawOverlay(GLuint m_scrollTextures[2])
{		
	//Need to defer this because of GLX crumminess
	//(it won't bind a GL context until onscreen.)
	if(!*m_scrollTextures) initOverlay(m_scrollTextures);

	int w = m_viewportWidth;
	int h = m_viewportHeight;

	//setViewportOverlay();
	{
		//glViewport(0,0,(GLint)m_viewportWidth,(GLint)m_viewportHeight);

		glMatrixMode(GL_PROJECTION); glLoadIdentity();

		glOrtho(0,w,0,h,m_nearOrtho,m_farOrtho); //-1,1);

		//glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	}

	glDisable(GL_LIGHTING);
	glColor3f(1,1,1);

	glEnable(GL_TEXTURE_2D);

	int sx = 0;
	int sy = 0;
	int size = scrollwidget->SCROLL_SIZE;

	for(int b=0;b<ScrollButtonMAX;b++)
	{
		auto *sbt = &scrollwidget[b];
		sx = sbt->x;
		sy = sbt->y;

		glBindTexture(GL_TEXTURE_2D,m_scrollTextures[sbt->texIndex]);

		glBegin(GL_QUADS);

		glTexCoord2f(sbt->s1,sbt->t1);
		glVertex3i(w+sx,h+sy,0);
		glTexCoord2f(sbt->s2,sbt->t2);
		glVertex3i(w+sx+size,h+sy,0);
		glTexCoord2f(sbt->s3,sbt->t3);
		glVertex3i(w+sx+size,h+sy+size,0);
		glTexCoord2f(sbt->s4,sbt->t4);
		glVertex3i(w+sx,h+sy+size,0);

		glEnd();
	}

	glDisable(GL_TEXTURE_2D);
}

void ScrollWidget::initOverlay(GLuint m_scrollTextures[2])
{	
	if(m_scrollTextures[0]){ assert(0); return; }

	const char **xpm[] = { arrow_xpm,crosshairrow_xpm };

	initTexturesUI(2,m_scrollTextures,(char***)xpm);
}

bool ScrollWidget::pressOverlayButton(int x, int y, bool rotate)
{	
	x-=m_viewportX; y-=m_viewportY; 

	int w = m_viewportWidth;
	int h = m_viewportHeight; 
	m_overlayButton = ScrollButtonMAX;
	for(int b=0;b<ScrollButtonMAX;b++)
	{
		int sx = scrollwidget[b].x;
		int sy = scrollwidget[b].y;
		if(x>=w+sx&&x<=w+sx+scrollwidget->SCROLL_SIZE
		 &&y>=h+sy&&y<=h+sy+scrollwidget->SCROLL_SIZE)
		{
			m_overlayButton = (ScrollButtonE)b;
			pressOverlayButton(rotate);
			break;
		}
	}
	return m_overlayButton!=ScrollButtonMAX;
}
void ScrollWidget::pressOverlayButton(bool rotate)
{
	switch(m_overlayButton)
	{
	case ScrollButtonPan: 

		default: return; //ScrollButtonMAX 

	case ScrollButtonLeft:
		if(rotate)
		rotateLeft(); else scrollLeft();
		break;
	case ScrollButtonRight:
		if(rotate) 
		rotateRight(); else scrollRight();
		break;
	case ScrollButtonUp:
		if(rotate) 
		rotateUp(); else scrollUp();
		break;
	case ScrollButtonDown:
		if(rotate)
		rotateDown(); else scrollDown();
		break;
	}
	
	//MAGIC: Communicate these to the timer.
	int id = 1+abs(scrollwidget_scroll.first); //C99
	if(rotate) id = -id;
	scrollwidget_scroll.first = id;
	scrollwidget_scroll.second = this;
	//TODO: UI layer should implement this.
	setTimerUI(300,scrollwidget_scroll_timer,id);
}

TextureWidget::TextureWidget(Parent *parent)
	: 
ScrollWidget(zoom_min,zoom_max),
//PAD_SIZE(6),
parent(parent),
m_scrollTextures(),
m_sClamp(),
m_tClamp(),
m_model(),
m_materialId(-1),
m_texture(),
m_glTexture(),
m_drawMode(DM_Edit),
m_drawVertices(true),
m_drawBorder(),
m_solidBackground(),
m_operation(MouseNone), //MouseSelect
m_scaleKeepAspect(),
m_scaleFromCenter(),
m_selecting(),
m_drawBounding(),
m_3d(),
m_buttons(),
m_xMin(0),
m_xMax(1),
m_yMin(0),
m_yMax(1),
m_xRotPoint(0.5),
m_yRotPoint(0.5),
m_linesColor(0xffffff),
m_selectionColor(0xff0000),
m_overrideWidth(),m_overrideHeight(),
m_width(),m_height() //2022
{
	m_scroll[0] = m_scroll[1] = 0.5;

	//setAutoBufferSwap(false); //Qt
}

void TextureWidget::setModel(Model *model)
{
	m_model = model;
	//m_texture = nullptr;
	//m_materialId = -1; //NEW
	setTexture(-1,nullptr);
	clearCoordinates();
	//glDisable(GL_TEXTURE_2D); //???
}
void TextureWidget::clearCoordinates()
{
	m_vertices.clear(); m_triangles.clear();
	
	m_operations_verts.clear();
}

void TextureWidget::updateViewport(int how)
{
	if(how=='z') //HACK
	{
		parent->zoomLevelChangedSignal();
	}

	m_xMin = m_scroll[0]-m_zoom/2;
	m_xMax = m_scroll[0]+m_zoom/2;
	m_yMin = m_scroll[1]-m_zoom/2;
	m_yMax = m_scroll[1]+m_zoom/2;

	parent->updateWidget(); //updateGL();
}

void TextureWidget::drawTriangles()
{
	auto vb = m_vertices.data();

	for(auto&ea:m_triangles)
	{
		bool wrapLeft = false;
		bool wrapRight = false;
		bool wrapTop = false;
		bool wrapBottom = false;

		for(int ea2:ea.vertex)
		{
			auto &v = vb[ea2];
			glVertex3d(v.s,v.t,-0.5);

			if(m_drawMode!=DM_Edit) //paintexturewin.cc
			{
				if(v.s<0) wrapLeft = true;
				if(v.s>1) wrapRight = true;
				if(v.t<0) wrapBottom = true;
				if(v.t>1) wrapTop = true;
			}
		}

		if(m_drawMode!=DM_Edit) //paintexturewin.cc
		{
			if(wrapLeft) for(int ea2:ea.vertex)
			glVertex3d(vb[ea2].s+1,vb[ea2].t,-0.5);

			if(wrapRight) for(int ea2:ea.vertex)
			glVertex3d(vb[ea2].s-1,vb[ea2].t,-0.5);

			if(wrapBottom) for(int ea2:ea.vertex)
			glVertex3d(vb[ea2].s,vb[ea2].t+1,-0.5);

			if(wrapTop) for(int ea2:ea.vertex)
			glVertex3d(vb[ea2].s,vb[ea2].t-1,-0.5);
		}
	}
}

static void texwidget_3dcube_timer(int i)
{
	if(i!=texwidget_3dcube.first) return;
	
	ScrollWidget *s = texwidget_3dcube.second;

	//TODO: UI layer should implement this.
	s->setTimerUI(30,texwidget_3dcube_timer,i);

	//texwidget_3dcube.second->updateGL();
	((TextureWidget*)s)->parent->updateWidget();
}
void TextureWidget::set3d(bool o)
{
	int i = ++texwidget_3dcube.first;
	
	m_3d = o; if(o)
	{
		texwidget_3dcube.second = this;
		texwidget_3dcube_timer(i); 
	}
	else parent->updateWidget(); //updateGL();
}

void TextureWidget::setTexture(int materialId, Texture *texture)
{
	if(!texture&&materialId>=0) //NEW
	{
		//Removing need for TextureFrame middle class.
		texture = m_model->getTextureData(materialId);
	}

	m_materialId = materialId; m_texture = texture;

	m_sClamp = m_model->getTextureSClamp(materialId);
	m_tClamp = m_model->getTextureTClamp(materialId);

	updateViewport();

	if(m_texture&&m_glTexture) initTexture();	

	//resizeGL(this->width(),this->height()); //???

	parent->updateWidget(); //updateGL();
}
void TextureWidget::initTexture()
{
	if(!m_glTexture)
	glGenTextures(1,&m_glTexture);

	glBindTexture(GL_TEXTURE_2D,m_glTexture);

	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,/*GL_NEAREST*/GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,/*GL_NEAREST*/GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,m_sClamp?GL_CLAMP:GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,m_tClamp?GL_CLAMP:GL_REPEAT);

	GLuint format = m_texture->m_format==Texture::FORMAT_RGBA?GL_RGBA:GL_RGB;

	//https://github.com/zturtleman/mm3d/issues/85
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);

	/*FIX ME //Mipmaps option?
	glTexImage2D(GL_TEXTURE_2D,0,format,
	m_texture->m_width,m_texture->m_height,0,
	format,GL_UNSIGNED_BYTE,m_texture->m_data.data());*/
	gluBuild2DMipmaps(GL_TEXTURE_2D,format, //GLU
	m_texture->m_width,m_texture->m_height,							
	format,GL_UNSIGNED_BYTE,m_texture->m_data.data());
}

void TextureWidget::uFlipCoordinates()
{
	for(auto&ea:m_vertices) if(ea.selected)
	{
		ea.s = 1-ea.s;
	}
	
	parent->updateWidget(); //updateGL();
}
void TextureWidget::vFlipCoordinates()
{
	for(auto&ea:m_vertices) if(ea.selected)
	{
		ea.t = 1-ea.t;
	}
	
	parent->updateWidget(); //updateGL();
}

void TextureWidget::rotateCoordinatesCcw()
{
	for(auto&ea:m_vertices) if(ea.selected)
	{
		double swap = ea.t;
		ea.t = ea.s; ea.s = 1-swap;
	}

	parent->updateWidget(); //updateGL();
}
void TextureWidget::rotateCoordinatesCw()
{
	for(auto&ea:m_vertices) if(ea.selected)
	{
		double swap = ea.t;
		ea.t = 1-ea.s; ea.s = swap;
	}

	parent->updateWidget(); //updateGL();
}

void TextureWidget::uFlattenCoordinates()
{
	double min = +DBL_MAX;
	double max = -DBL_MAX;
	for(auto&ea:m_vertices) if(ea.selected)
	{
		if(ea.s<min) min = ea.s;
		if(ea.s>max) max = ea.s;
	}
	min+=(max-min)*0.5;
	for(auto&ea:m_vertices) if(ea.selected)
	{
		ea.s = min;
	}	
	parent->updateWidget(); //updateGL();
}
void TextureWidget::vFlattenCoordinates()
{
	double min = +DBL_MAX;
	double max = -DBL_MAX;
	for(auto&ea:m_vertices) if(ea.selected)
	{
		if(ea.t<min) min = ea.t;
		if(ea.t>max) max = ea.t;
	}
	min+=(max-min)*0.5;
	for(auto&ea:m_vertices) if(ea.selected)
	{
		ea.t = min;
	}	
	parent->updateWidget(); //updateGL();
}

void TextureWidget::addVertex(double s, double t, bool select)
{
	TextureVertexT v = {s,t,select};
	m_vertices.push_back(v);
	//return (unsigned)m_vertices.size()-1;
}

void TextureWidget::addTriangle(int v1, int v2, int v3)
{
	//SILLINESS
	//size_t sz = m_vertices.size();
	//if((size_t)v1<sz&&(size_t)v2<sz&&(size_t)v3<sz) //???
	{	
		TextureTriangleT t = {{v1,v2,v3}};
		m_triangles.push_back(t);

	//	return (unsigned)m_triangles.size()-1;
	}
	//else assert(0); return -1; //???
}

void TextureWidget::snap(double &s, double &t, bool selected)
{		
	Model::ViewportUnits &vu = m_model->getViewportUnits();
 
	if(vu.snap&vu.UvSnap) 
	{		
		double ss = 6.1/m_width*m_zoom; 
		double tt = 6.1/m_height*m_zoom;

		if(1) //Square or elliptical?
		{
			//Existing code is square?			
			double x1 = s-ss, x2 = s+ss;
			double y1 = t-tt, y2 = t+tt;
			for(auto&ea:m_vertices) if(selected||!ea.selected)
			{		
				if(ea.s>x1&&ea.s<x2&&ea.t>y1&&ea.t<y2)
				{
					//Unclear how to get the nearest vertex?
					//Is it worth it? 
					//The mouse wheel can zoom in to get it?
					s = ea.s; t = ea.t; return;
				}
			}
		}
		else //Grabs the nearest vertex, but is it worth it?
		{	
			double d2 = ss*ss+tt*tt;
			bool snapped = false;
			for(auto&ea:m_vertices) if(selected||!ea.selected)
			{		
				double ds = ea.s-s, dt = ea.t-t; 
				if((ds=ds*ds+dt*dt)<d2)
				{
					snapped = true;

					d2 = ds; ss = ea.s; tt = ea.t;
				}
			}
			if(snapped)
			{
				s = ss; 
				t = tt; return;
			}
		}
	}
	if(vu.snap&vu.SubpixelSnap) 
	{
		double w = getUvWidth();
		double h = getUvHeight();

		//ARBITRARY 
		//5 feels good but I think it's good 
		//to have to zoom in some to make the
		//mode to change... really this needs
		//be to customizable, especially with
		//high DPI.
		enum{ box=6 }; //5 

		//Roughly estimate how many times
		//the snap box can fit in a pixel.
		double px = m_width/m_zoom/w;
		double py = m_height/m_zoom/h;
		int zoom = (int)std::min(px/box,py/box);
		zoom = std::min(zoom,vu.unitsUv);
		switch(zoom) //Force to power-of-two?
		{
		case 3: zoom = 2; break;
		case 0: zoom = 1;
		case 1: case 2: break;
		default: zoom = zoom>=8?8:4; break;
		}
		double units[2] = {1/w/zoom,1/h/zoom};

		//I don't understand this??? It just works.
		if(zoom==1) s-=units[0]*0.5f;
		if(zoom==1) t-=units[1]*0.5f;

		// snap to grid (getParentXYZValue)

		double cmp[2],val[2]; for(int i=2;i-->0;)
		{
			double x = i?t:s;
			double round = x<0?-0.5:0.5;
			int mult = (int)(x/units[i]+round);
			val[i] = mult*units[i];
			cmp[i] = fabs(x-val[i]);
		}
		if(zoom>1) //Subpixel snap?
		{		
			//Assuming for snapping to pixels that
			//in between coordinates are undesired.
			//If this isn't done the behavior when
			//in between snap points feels erratic
			//and if the space is small it appears
			//as if there's a snap point within it.

			/*if(cmp[0]<ss)*/ s = val[0];
			/*if(cmp[1]<tt)*/ t = val[1];
		}
		else //Whole pixel snap?
		{
			s = val[0]+vu.snapUv[0]*units[0];
			t = val[1]+vu.snapUv[1]*units[1];
		}
	}
}
bool TextureWidget::mousePressEvent(int bt, int bs, int x, int y)
{	
	if(!m_interactive) return false;

	if(!parent->mousePressSignal(bt)) return false;
	
	x-=m_x; y-=m_y; //NEW
	
	m_constrain = 0;
	m_constrainX = x; //m_lastXPos = x;
	m_constrainY = y; //m_lastYPos = y; 	

	if(m_buttons&bt) //DEBUGGING?
	{
		//HACK: In case the button state is confused
		//let the user unstick buttons by pressing
		//the button. (Hopefully this only happens
		//when using the debugger "break" function.)
		m_buttons&=~bt;

		if(bt==m_activeButton) m_activeButton = 0;
		
		model_status(m_model,StatusError,STATUSTIME_LONG,
		TRANSLATE("LowLevel","Warning: Unstuck confused mouse button state"));
	}

	if(!m_buttons) //NEW
	{
		assert(!m_activeButton);

		m_activeButton = bt;

		if(!m_model->getViewportUnits().no_overlay_option) //2022
		if(pressOverlayButton(x+m_x,y+m_y,false)) 
		{
			return true;
		}
	}

	m_buttons|=bt; //e->button();

	if(bt!=m_activeButton) return true; //NEW

	double s = getWindowSCoord(x);
	double t = getWindowTCoord(y);

	//if(e->button()&Qt::MidButton)
	if(bt==Tool::BS_Middle
	||bt&&bs&Tool::BS_Ctrl //2022
	||bt&&m_operation==MouseNone) //2022
	{
		return true; // We're panning
	}

	if(m_uvSnap) switch(m_operation) //2022
	{
	case MouseMove:
	case MouseScale:
	case MouseRotate: snap(s,t,true);
	}
	m_s = s; m_t = t;
	
	int bs_locked = bs|parent->_tool_bs_lock; //2022

	bool shift = 0!=(bs_locked&Tool::BS_Shift);

	//2022: Emulate Tool::Parent::snapSelect?
	m_selecting = bt==Tool::BS_Left;

	switch(m_operation)
	{
	case MouseSelect:

		//if(!(e->button()&Qt::RightButton
		//||e->modifiers()&Qt::ShiftModifier))
		if(!(bt==Tool::BS_Right||bs_locked&Tool::BS_Shift))
		{
			//clearSelected();
			for(auto&ea:m_vertices) ea.selected = false;
		}
		m_xSel1 = s; m_ySel1 = t;
		m_xSel2 = s; m_ySel2 = t; //NEW:
		//m_selecting = e->button()&Qt::RightButton?false:true;
		m_selecting = bt&Tool::BS_Right?false:true;
		m_drawBounding = true;
		break;

	case MouseScale:

		if(shift) m_constrain = ~3;
		startScale(s,t);
		break;

	case MouseMove:

		if(shift) m_constrain = ~3;
		break;

	case MouseRotate:

		if(bt==Tool::BS_Right)
		{
			m_xRotPoint = s; m_yRotPoint = t;
		}
		else
		{
			m_xRotStart = s-m_xRotPoint;
			m_yRotStart = t-m_yRotPoint;

			double angle = rotatepoint_diff_to_angle(m_xRotStart*m_aspect,m_yRotStart);
				
			//if(e->modifiers()&Qt::ShiftModifier)
			if(bs_locked&Tool::BS_Shift) angle = 
			rotatepoint_adjust_to_nearest(angle,bs&Tool::BS_Alt?5:15); //HACK

			m_startAngle = angle;

			m_operations_verts.clear();
			for(auto&ea:m_vertices) if(ea.selected)
			{
				int v = &ea-m_vertices.data();
				TransVertexT r = {v,(ea.s-m_xRotPoint)*m_aspect,ea.t-m_yRotPoint};
				m_operations_verts.push_back(r);
			}
		}
		parent->updateWidget(); //updateGL();
		break;
	
	case MouseRange:
	
		setCursorUI(getRangeDirection(s,t,true));

		if(m_dragAll) startSeam(); //2022

		break;
	}	

	return true; //I guess??
}
void TextureWidget::mouseMoveEvent(int bs, int x, int y)
{
	if(!m_interactive) return;

	x-=m_x; y-=m_y;

	int bt = m_activeButton; //NEW

	if(bt==Tool::BS_Middle
	||bt&&bs&Tool::BS_Ctrl //2022
	||bt&&m_operation==MouseNone) //2022
	{
		goto pan;
	}
	else if(m_overlayButton!=ScrollButtonMAX)
	{
		switch(m_overlayButton)
		{
		case ScrollButtonPan: pan:
		
			//For some reason (things are moving) using
			//m_s and m_t won't work.
			double ds = getWindowSDelta(x,m_constrainX);
			double dt = getWindowTDelta(y,m_constrainY);

			m_constrainX = x; m_constrainY = y;

			m_scroll[0]-=ds; m_xMin-=ds; m_xMax-=ds;
			m_scroll[1]-=dt; m_yMin-=dt; m_yMax-=dt;

			updateViewport('p'); break;
		}

		return;
	}
	else if(!bt) //!m_buttons
	{
		//This is peculiar to MouseRange alone.
		//updateCursorShape(x,y);
		if(m_operation==MouseRange) setRangeCursor(x,y);

		return;
	}

	//2022: Emulate Tool::Parent::snapSelect?
	if(m_operation!=MouseSelect) m_selecting = false;
	
	double s,t,ds,dt;
	s = getWindowSCoord(x); 
	t = getWindowTCoord(y);
	if(m_uvSnap) switch(m_operation)
	{
	case MouseMove:
	case MouseScale:
	case MouseRotate: snap(s,t,false);
	}
	if(m_constrain)
	{
		//TODO: STANDARDIZE AND REQUIRE MINIMUM PIXELS
		if(m_constrain==~3) 
		{
			int ax = std::abs(x-m_constrainX);
			int ay = std::abs(y-m_constrainY);
		
			//if(ax==ay) return; //NEW
			if(std::abs(ax-ay)<4) return; //2022

			m_constrain = ax>ay?~1:~2;
		}
		if(m_constrain&1) x = m_constrainX; else m_constrainX = x;
		if(m_constrain&2) y = m_constrainY; else m_constrainY = y;

		if(m_constrain&1) s = getWindowSCoord(x); 
		if(m_constrain&2) t = getWindowTCoord(y);
	}	
	ds = s-m_s; m_s = s; 
	dt = t-m_t; m_t = t;
	
	switch(m_operation)
	{
	case MouseSelect:
		
		updateSelectRegion(s,t);
		break;

	case MouseMove:
  
		moveSelectedVertices(ds,dt);
							
		//emit updateCoordinatesSignal();
		//emit(this,updateCoordinatesSignal);
		parent->updateCoordinatesSignal();
		break;

	case MouseRotate:
	{
		s-=m_xRotPoint; t-=m_yRotPoint;
				
		double angle = rotatepoint_diff_to_angle(s*m_aspect,t);
			
		if(Tool::BS_Shift&(bs|parent->_tool_bs_lock))
		angle = rotatepoint_adjust_to_nearest(angle,bs&Tool::BS_Alt?5:15);

		rotateSelectedVertices(angle-m_startAngle);

		//emit updateCoordinatesSignal();
		//emit(this,updateCoordinatesSignal);
		parent->updateCoordinatesSignal();
		break;
	}
	case MouseScale:

		scaleSelectedVertices(s,t);
							
		//emit updateCoordinatesSignal();
		//emit(this,updateCoordinatesSignal);
		parent->updateCoordinatesSignal();
		break;

	case MouseRange:
	
		//if(m_buttons&Qt::LeftButton)
		if(bt==Tool::BS_Left)
		{	
			if(m_dragLeft||m_dragAll)
			{
				m_xRangeMin+=ds;
				m_xRangeMax = std::max(m_xRangeMax,m_xRangeMin);
			}
			if(m_dragRight||m_dragAll)
			{
				m_xRangeMax+=ds;
				m_xRangeMin = std::min(m_xRangeMin,m_xRangeMax);
			}
			if(m_dragBottom||m_dragAll)
			{
				m_yRangeMin+=dt;
				m_yRangeMax = std::max(m_yRangeMax,m_yRangeMin);
			}
			if(m_dragTop||m_dragAll)
			{
				m_yRangeMax+=dt;
				m_yRangeMin = std::min(m_yRangeMin,m_yRangeMax);
			}

			if(m_dragAll||m_dragTop||m_dragBottom||m_dragLeft||m_dragRight)
			{
				//emit updateRangeSignal();
				//emit(this,updateRangeSignal);
				parent->updateRangeSignal();
			}
		}
		else if(m_dragAll) //???
		{
			double dx = ds*-2*PI, dy = dt*-2*PI;

			accumSeam(dx,dy); //2022

			//emit updateSeamSignal(ds*-2*PI,dt*-2*PI);
			//emit(this,updateSeamSignal,ds*-2*PI,dt*-2*PI);
			parent->updateSeamSignal(dx,dy);
		}

		parent->updateWidget();
		break;
	}
}
void TextureWidget::mouseReleaseEvent(int bt, int bs, int x, int y)
{
	if(!m_interactive) return;

	m_buttons&=~bt; //e->button();

	if(bt!=m_activeButton) return; //NEW
		
	m_activeButton = 0;

	if(m_overlayButton!=ScrollButtonMAX)
	{
		m_overlayButton = ScrollButtonMAX;

		cancelOverlayButtonTimer(); //HACK

		return;
	}		
	
	//if(e->button()&Qt::MidButton)
	if(bt==Tool::BS_Middle
	||bt&&bs&Tool::BS_Ctrl //2022
	||bt&&m_operation==MouseNone) //2022
	{
		return; // We're panning
	}

	x-=m_x; y-=m_y;

	//2022: Emulate Tool::Parent::snapSelect?
	if(m_selecting&&m_operation!=MouseSelect)
	{
		m_xSel1 = m_xSel2 = getWindowSCoord(x);
		m_ySel1 = m_ySel2 = getWindowTCoord(y);		
		selectDone(1|bs|parent->_tool_bs_lock);
	return parent->updateSelectionDoneSignal();
	}	
	else switch(m_operation)
	{
	case MouseSelect:
		
		m_drawBounding = false;

		selectDone();
						
		//emit updateSelectionDoneSignal();
		//emit(this,updateSelectionDoneSignal);
		parent->updateSelectionDoneSignal();
		break;

	case MouseMove:
								
		//emit updateCoordinatesSignal();
		//emit(this,updateCoordinatesSignal);
	//	parent->updateCoordinatesSignal(); //???
						
				case MouseRotate://2019
				case MouseScale: //2019
				/*2019: FALLING THROUGH

		//emit updateCoordinatesDoneSignal();
		//emit(this,updateCoordinatesDoneSignal);
		parent->updateCoordinatesDoneSignal();

		break;

	case MouseRotate:
						
		// Nothing to do here

		//emit updateCoordinatesDoneSignal();
		//emit(this,updateCoordinatesDoneSignal);
		parent->updateCoordinatesDoneSignal();

		break;

	case MouseScale:
						
		// Nothing to do here

				2019: FALLING THROUGH*/

		//emit updateCoordinatesDoneSignal();
		//emit(this,updateCoordinatesDoneSignal);
		parent->updateCoordinatesDoneSignal();
		break;

	case MouseRange:

		//if(m_buttons&Qt::LeftButton)
		if(bt==Tool::BS_Left)
		{
			if(m_dragAll||m_dragTop||m_dragBottom||m_dragLeft||m_dragRight)
			{
				//emit updateRangeDoneSignal();
				//emit(this,updateRangeDoneSignal);
				parent->updateRangeDoneSignal();
			}
		}
		else if(m_dragAll)
		{
			//emit updateSeamDoneSignal();
			//emit(this,updateSeamDoneSignal);
			parent->updateSeamDoneSignal(m_xSeamAccum,m_ySeamAccum);
		}
		break;
	}
}

void TextureWidget::wheelEvent(int wh, int bs, int x, int y)
{
	if(m_interactive) //Is navigation interaction?
	{
		//ModelViewport does this and it might make sense too
		//if an app had multiple texture views
		if(!over(x,y)) return wh>0?zoomIn():zoomOut();

		zoom(wh>0,getWindowSCoord(x-m_x),getWindowTCoord(y-m_y));
	}
}

bool TextureWidget::keyPressEvent(int bt, int bs, int, int)
{	
	if(m_interactive) switch(bt)
	{
	//case Qt::Key_Home:
	case Tool::BS_Left|Tool::BS_Special: //HOME
	
		//if(~e->modifiers()&Qt::ShiftModifier)
		if(~bs&Tool::BS_Shift)
		{
			m_scroll[0] = m_scroll[1] = 0.5;
			m_zoom = 1;
		}			
		else if(m_drawMode!=DM_Edit)
		{
			break;
		}
		else if(m_operation==MouseRange)
		{
			m_scroll[0] = (m_xRangeMax-m_xRangeMin)/2+m_xRangeMin;
			m_scroll[1] = (m_yRangeMax-m_yRangeMin)/2+m_yRangeMin;

			double xzoom = m_xRangeMax-m_xRangeMin;
			double yzoom = m_yRangeMax-m_yRangeMin;

			m_zoom = 1.10*std::max(xzoom,yzoom);
		}
		else
		{
			double xMin = +DBL_MAX, xMax = -DBL_MAX;
			double yMin = +DBL_MAX, yMax = -DBL_MAX;

			//for(size_t v=1;v<vcount;v++) //1???
			for(auto&ea:m_vertices) if(ea.selected)
			{
				xMin = std::min(xMin,ea.s);
				xMax = std::max(xMax,ea.s);
				yMin = std::min(yMin,ea.t);
				yMax = std::max(yMax,ea.t);
			}
			if(xMin!=DBL_MAX)
			{
				double xzoom = xMax-xMin;
				double yzoom = yMax-yMin;

				m_scroll[0] = xzoom/2+xMin;
				m_scroll[1] = yzoom/2+yMin;

				m_zoom = 1.10*std::max(xzoom,yzoom);
			}
		}
		updateViewport('z'); break;
	
	//case Qt::Key_Equal: case Qt::Key_Plus: 
	case '=': case '+': zoomIn(); break;

	//case Qt::Key_Minus: case Qt::Key_Underscore:
	case '-': case '_': zoomOut(); break;

	case '0':
		
		m_scroll[0] = m_scroll[1] = 0.5;
		updateViewport('p'); break;

	//case Qt::Key_Up:
	case Tool::BS_Up: scrollUp(); break;
	//case Qt::Key_Down:
	case Tool::BS_Down: scrollDown(); break;
	//case Qt::Key_Left:
	case Tool::BS_Left: scrollLeft(); break;
	//case Qt::Key_Right: 
	case Tool::BS_Right: scrollRight(); break;

	//default: QGLWidget::keyPressEvent(e); break;
	default: return false;
	}
	//else QGLWidget::keyPressEvent(e);
	return true;
}
 
void TextureWidget::moveSelectedVertices(double x, double y)
{
	for(auto&ea:m_vertices) if(ea.selected)
	{
		ea.s+=x; ea.t+=y;
	}
	
	parent->updateWidget(); //updateGL();
}

void TextureWidget::updateSelectRegion(double x, double y)
{
	m_xSel2 = x; m_ySel2 = y; 
	
	parent->updateWidget(); //updateGL();
}

void TextureWidget::selectDone(int snap_select)
{
	double x1 = m_xSel1, y1 = m_ySel1;
	double x2 = m_xSel2, y2 = m_ySel2;

	if(x1==x2) //2020
	{
		double x = 6.1/m_width*m_zoom;
		x2 = x1+x; x1-=x;
	}
	if(y1==y2) //2020
	{
		double y = 6.1/m_height*m_zoom;
		y2 = y1+y; y1-=y;
	}

	if(x1>x2) std::swap(x1,x2);
	if(y1>y2) std::swap(y1,y2);	

	if(snap_select)
	{
		//2022: Emulate Tool::Parent::snapSelect?

		bool how = true;
		for(auto&ea:m_vertices)
		if(ea.s>x1&&ea.s<x2&&ea.t>y1&&ea.t<y2)
		if(ea.selected)
		{
			how = false; break;
		}

		bool unselect = false;
		if(how&&snap_select&&~snap_select&Tool::BS_Shift)
		unselect = true;

		for(auto&ea:m_vertices)
		if(ea.s>x1&&ea.s<x2&&ea.t>y1&&ea.t<y2)
		{
			ea.selected = how;
		}
		else if(unselect) ea.selected = false;
	}
	else
	{
		bool how = m_selecting;
		for(auto&ea:m_vertices)
		if(ea.s>x1&&ea.s<x2&&ea.t>y1&&ea.t<y2)
		{
			ea.selected = how;
		}
	}
	
	parent->updateWidget(); //updateGL();
}

void TextureWidget::setRangeCursor(int x, int y)
{
	if(!m_interactive) return; //???
	
	int w = m_viewportWidth;
	int h = m_viewportHeight;

	int sx = 0;
	int sy = 0;
	int size = scrollwidget->SCROLL_SIZE;

	int bx = x;
	int by = /*h-*/y;

	ScrollButtonE button = ScrollButtonMAX;
	for(int b=0;b<ScrollButtonMAX;b++)
	{
		sx = scrollwidget[b].x;
		sy = scrollwidget[b].y;

		if(bx>=w+sx&&bx<=w+sx+size
		 &&by>=h+sy&&by<=h+sy+size)
		{
			button = (ScrollButtonE)b; 
			break;
		}
	}

	if(button==ScrollButtonMAX)		
	if(m_operation==MouseRange)
	{	
		double windowX = getWindowSCoord(x);
		double windowY = getWindowTCoord(y);						
		setCursorUI(getRangeDirection(windowX,windowY));
		return;
	}

	setCursorUI();
}

int TextureWidget::getRangeDirection(double windowX, double windowY, bool press)
{
	if(!m_interactive) return -1;

	bool dragAll = false;
	bool dragTop = false;
	bool dragBottom = false;
	bool dragLeft = false;
	bool dragRight = false;

	double prox = 6.1/m_width*m_zoom;
	double proy = 6.1/m_height*m_zoom; //2022

	if(windowX>=m_xRangeMin-prox
	 &&windowX<=m_xRangeMax+prox
	 &&windowY>=m_yRangeMin-proy
	 &&windowY<=m_yRangeMax+proy)
	{
		if(fabs(m_xRangeMin-windowX)<prox)
		{
			dragLeft = true;
		}
		if(fabs(m_xRangeMax-windowX)<prox)
		{
			dragRight = true;
		}
		if(fabs(m_yRangeMin-windowY)<proy)
		{
			dragBottom = true;
		}
		if(fabs(m_yRangeMax-windowY)<proy)
		{
			dragTop = true;
		}

		if(dragLeft&&dragRight)
		{
			// The min and max are very close together,don't drag
			// both at one time.
			if(windowX<m_xRangeMin)
			{
				dragRight = false;
			}
			else if(windowX>m_xRangeMax)
			{
				dragLeft = false;
			}
			else
			{
				// We're in-between,don't drag either (top/bottom still okay)
				dragLeft = false;
				dragRight = false;
			}
		}
		if(dragTop&&dragBottom)
		{
			// The min and max are very close together,don't drag
			// both at one time.
			if(windowY<m_yRangeMin)
			{
				dragTop = false;
			}
			else if(windowY>m_yRangeMax)
			{
				dragBottom = false;
			}
			else
			{
				// We're in-between,don't drag either (left/right still okay)
				dragTop = false;
				dragBottom = false;
			}
		}

		if(!dragTop&&!dragBottom&&!dragLeft&&!dragRight)
		{
			if(windowX>m_xRangeMin&&windowX<m_xRangeMax
			 &&windowY>m_yRangeMin&&windowY<m_yRangeMax)
			{
				dragAll = true;
			}
		}
	}	

	if(press)
	{
		m_dragAll = dragAll;
		m_dragTop = dragTop;
		m_dragBottom = dragBottom;
		m_dragLeft = dragLeft;
		m_dragRight = dragRight;
	}

	if(dragLeft)
	{
		if(dragTop) return 45;

		if(dragBottom) return 315;

		return 0;
	}
	if(dragRight)
	{
		if(dragTop) return 135;

		if(dragBottom) return 225;

		return 180;
	}

	if(dragTop) return 90;

	if(dragBottom) return 270;

	return dragAll?360:-1;
}

void TextureWidget::getCoordinates(int tri, float *s, float *t)
{
	//if(t&&s&&(size_t)tri<m_triangles.size()) //???
	for(int i:m_triangles[tri].vertex)
	{
		*s++ = (float)m_vertices[i].s; 
		*t++ = (float)m_vertices[i].t;
	}
}

void TextureWidget::saveSelectedUv()
{
	//HACK: Is it worth optimizing for undo/redo?
	int_list _selectedUv;
	auto *selectedUv = &m_model->getSelectedUv();
	if(m_model->getUndoEnabled())
	selectedUv = &_selectedUv;
	else selectedUv->clear();

	int i,iN = (unsigned)m_vertices.size();
	for(i=0;i<iN;i++) if(m_vertices[i].selected)
	{
		selectedUv->push_back(i);
	}
	m_model->setSelectedUv(*selectedUv);
}

void TextureWidget::restoreSelectedUv()
{		
	if(m_vertices.empty()) return; //hidden?

	//clearSelected();
	for(auto&ea:m_vertices) ea.selected = false;

	//int_list selectedUv;
	//m_model->getSelectedUv(selectedUv);
	auto &selectedUv = m_model->getSelectedUv();

	for(auto ea:selectedUv)
	{
		if((size_t)ea<m_vertices.size()) //???
		{
			m_vertices[ea].selected = true;
		}
		else //Shouldn't happen? (Still happening?)
		{
			//NOTE: To fix this I've added Model::SelectionUv and had
			//the UV window call clearCoordinates when face selection
			//occurs in case it's hidden. It's pretty dicey undo/redo
			//territory (Still happening?)

			assert(0); break; 
		}
	}
}

void TextureWidget::setRange(double xMin, double yMin, double xMax, double yMax)
{
	m_xRangeMin = xMin; m_yRangeMin = yMin; m_xRangeMax = xMax; m_yRangeMax = yMax;
}

void TextureWidget::getRange(double &xMin, double &yMin, double &xMax, double &yMax)
{
	xMin = m_xRangeMin; yMin = m_yRangeMin; xMax = m_xRangeMax; yMax = m_yRangeMax;
}

void TextureWidget::startScale(double x, double y)
{	
	double xMin = +DBL_MAX, xMax = -DBL_MAX;
	double yMin = +DBL_MAX, yMax = -DBL_MAX;

	m_operations_verts.clear();
	for(auto&ea:m_vertices) if(ea.selected)
	{
		xMin = std::min(xMin,ea.s);
		xMax = std::max(xMax,ea.s);
		yMin = std::min(yMin,ea.t);
		yMax = std::max(yMax,ea.t);

		TransVertexT s = {&ea-m_vertices.data(),ea.s,ea.t};
		m_operations_verts.push_back(s);
	}
	if(m_operations_verts.empty()) return; //NEW

	if(m_scaleFromCenter)
	{
		m_centerX = (xMax-xMin)/2+xMin;
		m_centerY = (yMax-yMin)/2+yMin;

		m_startLengthX = fabs(m_centerX-x);
		m_startLengthY = fabs(m_centerY-y);
	}
	else //From corner?
	{
		//DUPLICATES scaletool.cc.

		//NOTE: sqrt not required.
		double minmin = distance(x,y,xMin,yMin);
		double minmax = distance(x,y,xMin,yMax);
		double maxmin = distance(x,y,xMax,yMin);
		double maxmax = distance(x,y,xMax,yMax);

		//Can this be simplified?
		if(minmin>minmax)
		{
			if(minmin>maxmin)
			{
				if(minmin>maxmax)
				{
					m_farX = xMin; m_farY = yMin;
				}
				else
				{
					m_farX = xMax; m_farY = yMax;
				}
			}
			else same: // maxmin>minmin
			{
				if(maxmin>maxmax)
				{
					m_farX = xMax; m_farY = yMin;
				}
				else
				{
					m_farX = xMax; m_farY = yMax;
				}
			}
		}
		else // minmax>minmin
		{
			if(minmax>maxmin)
			{
				if(minmax>maxmax)
				{
					m_farX = xMin; m_farY = yMax;
				}
				else
				{
					m_farX = xMax; m_farY = yMax;
				}
			}
			else goto same; // maxmin>minmax			
		}

		m_startLengthX = fabs(x-m_farX);
		m_startLengthY = fabs(y-m_farY);
	}
}

void TextureWidget::rotateSelectedVertices(double angle)
{
	Matrix m;
	Vector rot(0,0,angle); m.setRotation(rot);
	
	double a = 1/m_aspect;
	for(auto&ea:m_operations_verts)
	{
		Vector vec(ea.x,ea.y,0);
		m.apply3(vec);
		m_vertices[ea.index].s = vec[0]*a+m_xRotPoint;
		m_vertices[ea.index].t = vec[1]+m_yRotPoint;
	}

	parent->updateWidget(); //updateGL();
}

void TextureWidget::scaleSelectedVertices(double x, double y)
{
	double xx = m_scaleFromCenter?m_centerX:m_farX;
	double yy = m_scaleFromCenter?m_centerY:m_farY;

	double s = m_startLengthX<0.00006?1:fabs(x-xx)/m_startLengthX;
	double t = m_startLengthY<0.00006?1:fabs(y-yy)/m_startLengthY;

	if(m_scaleKeepAspect) s = t = std::max(s,t);

	for(auto&ea:m_operations_verts) //lerp
	{
		m_vertices[ea.index].s = (ea.x-xx)*s+xx;
		m_vertices[ea.index].t = (ea.y-yy)*t+yy;
	}

	parent->updateWidget(); //updateGL();
}

void TextureWidget::draw(int x, int y, int w, int h)
{	 
	if(PAD_SIZE) //updateSize()
	{
		//enum{ PAD_SIZE=6 };

		x+=PAD_SIZE; w-=PAD_SIZE*2;
		y+=PAD_SIZE; h-=PAD_SIZE*2;
	}
	int ww = w, hh = h;

	//updateSize()
	{
		m_x = x; m_y = y;

		if(!m_3d&&m_texture)
		{
			ww = m_overrideWidth;
			hh = m_overrideHeight;
			if(!ww) ww = m_texture->m_origWidth;
			if(!hh) hh = m_texture->m_origHeight;

			double s = (double)w/ww;
			double t = (double)h/hh;
			if(s>t) ww = (int)(ww*t); else ww = w;
			if(s<t) hh = (int)(hh*s); else hh = h;
			
			m_x+=(w-ww)/2; m_y+=(h-hh)/2;

			if(!m_interactive) 
			{
				x = m_x; w = ww; 
				y = m_y; h = hh;
			}
		}

		m_viewportX = x; m_viewportWidth = w;
		m_viewportY = y; m_viewportHeight = h;

		if(w<=0||h<=0) return;
	}

		//NEW: OpenGL can't backup its states.
		//ModelViewport might need to do this
		//if it wasn't its own OpenGL context.
		const int attribs 
		=GL_COLOR_BUFFER_BIT
		|GL_CURRENT_BIT
		|GL_DEPTH_BUFFER_BIT
		|GL_ENABLE_BIT
		|GL_LIGHTING_BIT
		|GL_POLYGON_BIT
		|GL_SCISSOR_BIT
		|GL_TEXTURE_BIT
		|GL_TRANSFORM_BIT
		|GL_VIEWPORT_BIT;
		glPushAttrib(attribs);

	//initializeGL()
	{
		// general set-up
		//glEnable(GL_TEXTURE_2D);

		//glShadeModel(GL_SMOOTH); //???
		//glClearColor(0.80f,0.80f,0.80f,1);

		/*NEW: Assuming depth-buffer not required.
		glDepthFunc(GL_LEQUAL);
		glClearDepth(1);*/

		//REMINDER:
		
		// set up lighting
		//https://github.com/zturtleman/mm3d/issues/173
		// 
		// TODO: what about being accurate to the viewport
		// lighting?
		// 
		//I think compensating for GL_LIGHT_MODEL_AMBIENT?
		//GLfloat ambient[] = { 0.8f,0.8f,0.8f,1 };
		/*2022
		// 1,1,1,1 just doesn't work, I'm not sure this is
		// right to replicate the default lighting for the
		// 3D scene sinc thate will be uneven. Probably it
		// should synchronize with texwin.cc's RGBA groups.
		GLfloat llll[] = { 1,1,1,1 };*/
		GLfloat llll[] = { 0.5,0.5,0.5,0.5 };
		GLfloat position[] = { 0,0,3,0 };
		float oooo[4] = {};
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT,oooo);
		
		//glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR); //2020
		glLightModeli(0x81F8,0x81FA); //2020

		//glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE); //???
		glLightfv(GL_LIGHT0,GL_AMBIENT,llll);
		glLightfv(GL_LIGHT0,GL_DIFFUSE,llll);
		glLightfv(GL_LIGHT0,GL_SPECULAR,llll); //2022: I guess?
		glLightfv(GL_LIGHT0,GL_POSITION,position);

		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHTING);
		glEnable(GL_SCISSOR_TEST);
	}

	m_width = ww;
	m_height = hh;
	m_aspect = m_width/m_height; 
	
	double cx,cy,sMin,sMax,tMin,tMax;

	//setViewportDraw();
	{
		glScissor(x,y,w,h);
		glViewport(x,y,w,h); 

		glMatrixMode(GL_PROJECTION);		
		glPushMatrix();
		glLoadIdentity();

		//glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

		if(!m_3d)
		{
			//Changing this so excess is rendered.
			//glOrtho(m_xMin,m_xMax,m_yMin,m_yMax,-1,1);
			cx = m_xMax+m_xMin, cy = m_yMax+m_yMin;
			double cw = m_xMax-m_xMin, ch = m_yMax-m_yMin;
			cw*=(double)w/ww; ch*=(double)h/hh;
			cx/=2; cy/=2; cw/=2; ch/=2;
			sMin = cx-cw;
			sMax = cx+cw;
			tMin = cy-ch;
			tMax = cy+ch;			

			//NOTE: This half-pixel calculation was for the
			//m_drawBounding effect to use later. My instinct
			//is it should be applied to the projection too, in
			//which case a half might be wrong for m_drawBounding.
			//The small value is for my Intel chipset. It might not
			//be universally applicable:
			//https://community.khronos.org/t/gl-nearest-frayed-edges-rounding-in-simple-image-editor/104696/3
			
			//This should be roughly a half pixel.
			//The shrinkage eliminates the Intel bug described in
			//the link. Note, the half-pixel offset isn't required.
			//But I assume if Intel's chipset wants to be offsetted
			//it expects a half pixel.
			cx = cw/w*0.95; cy = ch/h*0.95; //0.9995 is too little.
			
			//NOTE: I don't think offsetting a half-pixel matters much
			//here, since it's basically arbitrary. It should match if
			//w/h are the texture's width/height.
			glOrtho(sMin-cx,sMax-cx,tMin+cy,tMax+cy,-1,1);

			//I can't get this to render m_drawBounding square more 
			//than 95% of the time. I think the problem is scrolling
			//is not pegged to screenspace units. I mean, the corners
			//ideally are L shaped. It can be achieved by using screen
			//coordinates instead of UV coordinates, offset accordingly.
			//cx*=2; cy*=2; 
		}
		else gluPerspective(45,m_aspect,0.01,30); //GLU

		glMatrixMode(GL_MODELVIEW);		
		glPushMatrix(); 
		glLoadIdentity();
	}

	//log_debug("paintInternal()\n");
	//log_debug("(%f,%f)%f\n",m_scroll[0],m_scroll[1],m_zoom);

	/*FIX ME (glClearColor(0.80f,0.80f,0.80f,1))
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);*/

	//glLoadIdentity();
	//glEnable(GL_LIGHTING);

	if(m_texture&&!m_solidBackground)
	{		
		//Need to defer this because of GLX crumminess
		//(it won't bind a GL context until onscreen.)
		if(!m_glTexture) initTexture();

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,m_glTexture);
	}
	else glDisable(GL_TEXTURE_2D);

	if(m_solidBackground)
	{
		glColor3f(0,0,0);

		float black[4] = {};
		glMaterialfv(GL_FRONT,GL_AMBIENT,black);
		glMaterialfv(GL_FRONT,GL_DIFFUSE,black);
		glMaterialfv(GL_FRONT,GL_SPECULAR,black);
		glMaterialfv(GL_FRONT,GL_EMISSION,black);
		glMaterialf(GL_FRONT,GL_SHININESS,black[0]);
	}
	else if(m_materialId>=0)
	{
		//I can't do anything to make it not white :(
		//glColor3f(1,1,1); //???
		glColor3f(0,0,0);

		float fval[4];
		m_model->getTextureAmbient(m_materialId,fval);
		glMaterialfv(GL_FRONT,GL_AMBIENT,fval);
		m_model->getTextureDiffuse(m_materialId,fval);
		glMaterialfv(GL_FRONT,GL_DIFFUSE,fval);
		m_model->getTextureSpecular(m_materialId,fval);
		glMaterialfv(GL_FRONT,GL_SPECULAR,fval);
		m_model->getTextureEmissive(m_materialId,fval);
		glMaterialfv(GL_FRONT,GL_EMISSION,fval);
		m_model->getTextureShininess(m_materialId,fval[0]);
		glMaterialf(GL_FRONT,GL_SHININESS,fval[0]);
	}
	else
	{
		float fval[4] = { 0.2f,0.2f,0.2f,1 };
		glMaterialfv(GL_FRONT,GL_AMBIENT,fval);
		fval[0] = fval[1] = fval[2] = 1;
		glMaterialfv(GL_FRONT,GL_DIFFUSE,fval);
		fval[0] = fval[1] = fval[2] = 0;
		glMaterialfv(GL_FRONT,GL_SPECULAR,fval);
		fval[0] = fval[1] = fval[2] = 0;
		glMaterialfv(GL_FRONT,GL_EMISSION,fval);
		glMaterialf(GL_FRONT,GL_SHININESS,0);

		if(!m_texture) glDisable(GL_LIGHTING);
		if(!m_texture) glColor3f(0,0,0); 
		else glColor3f(1,1,1); //???
	}

	if(m_3d&&m_materialId>=0)
	{	
		/*NEW: Don't assume depth-buffer. 
		//May need to enable back-face culling.
		glEnable(GL_DEPTH_TEST);*/
		glEnable(GL_CULL_FACE);
		//glEnable(GL_COLOR_MATERIAL);

		int ms = getElapsedTimeUI();
		float yRot = ms/4000.0f*360;
		float xRot = ms/8000.0f*360;

		glTranslatef(0,0,-5);
		glRotatef(yRot,0,1,0);
		glRotatef(xRot,1,0,0);

		glBegin(GL_QUADS);

		// Front
		//glColor3ub(255,0,0); //r
		glTexCoord2f(0,0); glNormal3f(0,0,1); glVertex3f(-1,-1,1);
		glTexCoord2f(1,0); glNormal3f(0,0,1); glVertex3f( 1,-1,1);
		glTexCoord2f(1,1); glNormal3f(0,0,1); glVertex3f( 1,+1,1);
		glTexCoord2f(0,1); glNormal3f(0,0,1); glVertex3f(-1,+1,1);

		// Back
		//glColor3ub(255,255,0); //y
		glTexCoord2f(0,0); glNormal3f(0,0,-1); glVertex3f( 1,-1,-1);
		glTexCoord2f(1,0); glNormal3f(0,0,-1); glVertex3f(-1,-1,-1);
		glTexCoord2f(1,1); glNormal3f(0,0,-1); glVertex3f(-1,+1,-1);
		glTexCoord2f(0,1); glNormal3f(0,0,-1); glVertex3f( 1,+1,-1);

		// Left
		//glColor3ub(0,255,0); //g
		glTexCoord2f(0,0); glNormal3f(1,0,0); glVertex3f(1,-1,+1);
		glTexCoord2f(1,0); glNormal3f(1,0,0); glVertex3f(1,-1,-1);
		glTexCoord2f(1,1); glNormal3f(1,0,0); glVertex3f(1,+1,-1);
		glTexCoord2f(0,1); glNormal3f(1,0,0); glVertex3f(1,+1,+1);

		// Right
		//glColor3ub(0,255,255); //c
		glTexCoord2f(0,0); glNormal3f(-1,0,0); glVertex3f(-1,-1,-1);
		glTexCoord2f(1,0); glNormal3f(-1,0,0); glVertex3f(-1,-1,+1);
		glTexCoord2f(1,1); glNormal3f(-1,0,0); glVertex3f(-1,+1,+1);
		glTexCoord2f(0,1); glNormal3f(-1,0,0); glVertex3f(-1,+1,-1);

		// Top
		//glColor3ub(0,0,255); //b*
		glTexCoord2f(0,1); glNormal3f(0,1,0); glVertex3f(-1,1,+1); //4
		glTexCoord2f(1,1); glNormal3f(0,1,0); glVertex3f(+1,1,+1); //3
		glTexCoord2f(1,0); glNormal3f(0,1,0); glVertex3f(+1,1,-1); //2
		glTexCoord2f(0,0); glNormal3f(0,1,0); glVertex3f(-1,1,-1); //1

		// Bottom
		//glColor3ub(255,0,255); //m*
		glTexCoord2f(0,1); glNormal3f(0,-1,0); glVertex3f(+1,-1,+1); //4
		glTexCoord2f(1,1); glNormal3f(0,-1,0); glVertex3f(-1,-1,+1); //3
		glTexCoord2f(1,0); glNormal3f(0,-1,0); glVertex3f(-1,-1,-1); //2
		glTexCoord2f(0,0); glNormal3f(0,-1,0); glVertex3f(+1,-1,-1); //1
																		
		glEnd();
		
		//glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glBegin(GL_QUADS);

		glTexCoord2d(sMin,tMin); glNormal3f(0,0,1);
		glVertex3d(sMin,tMin,0);
		glTexCoord2d(sMax,tMin); glNormal3f(0,0,1);
		glVertex3d(sMax,tMin,0);
		glTexCoord2d(sMax,tMax); glNormal3f(0,0,1);
		glVertex3d(sMax,tMax,0); 
		glTexCoord2d(sMin,tMax); glNormal3f(0,0,1);
		glVertex3d(sMin,tMax,0);

		glEnd();
	}

	//glLoadIdentity();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);

	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); 

	//I'm trying here to ensure the border is not drawn to only
	//2 of 4 sides when texturecoord.cc window is first opened.
	//I didn't check, but it's also possible it was obscuring
	//texture pixels on those edges.
	if(m_drawBorder)
	{
		//NOTE: This is to demonstrate the 0/1 edge of the UV space.
		//It should be repeated to form a grid.

		glColor3f(0.7f,0.7f,0.7f);
		glRectd(-cx,-cy,1+cx,1+cy);
	}

	if(!m_triangles.empty()) switch(m_drawMode)
	{
	case DM_Edit:

		//TESTING
		//CAUTION: I don't know what the blend mode is
		//at this point?
		//https://github.com/zturtleman/mm3d/issues/95
		//useLinesColor SETS ALPHA, SHARED EDGES BLEND
		//TOGETHER TO BE BRIGHTER. THIS IS UNDESIRABLE.
		glEnable(GL_BLEND);
		//break;

	case DM_Edges:

		if(m_operation==MouseRange)
		glColor3f(0.7f,0.7f,0.7f);
		else useLinesColor();

		glBegin(GL_TRIANGLES);
		drawTriangles();
		glEnd();

		//TESTING
		glDisable(GL_BLEND); 
		break;

	case DM_Filled:
	case DM_FilledEdges:

		glColor3f(0,0,0.8f);
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

		glBegin(GL_TRIANGLES);
		drawTriangles();
		glEnd();

		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	
		if(DM_FilledEdges!=m_drawMode)
		break;

		glColor3f(1,1,1);
		glBegin(GL_TRIANGLES);
		drawTriangles();
		glEnd();
		break;
	}
	if(m_drawVertices) 
	{
		glPointSize(3);
		glBegin(GL_POINTS);

		int c = -1;

		//2022: Provide visual feedback?
		if(m_operation==MouseNone&&m_drawMode==DM_Edit)
		{
			for(auto&ea:m_vertices) if(ea.selected)
			{
				if(!++c) useSelectionColor();
			
				glVertex3d(ea.s,ea.t,-0.5);
			}
		}
		else for(auto&ea:m_vertices)
		{
			int cc = ea.selected&&m_drawMode==DM_Edit;
			if(c!=cc)
			(c=cc)?useSelectionColor():useLinesColor();

			glVertex3d(ea.s,ea.t,-0.5);
		}

		glEnd();
	}

	if(m_drawBounding) //drawSelectBox()
	{
		glEnable(GL_COLOR_LOGIC_OP);

		//glColor3ub(255,255,255);
		glColor3ub(0x80,0x80,0x80);
		glLogicOp(GL_XOR);
		glRectd(m_xSel1,m_ySel1,m_xSel2,m_ySel2);
		glDisable(GL_COLOR_LOGIC_OP);
	}
	if(m_operation==MouseRange) //drawRangeBox()
	{		
		glColor3f(1,1,1);
		glRectd(m_xRangeMin-cx,m_yRangeMin-cy,m_xRangeMax+cx,m_yRangeMax+cy);
	}
	if(m_operation==MouseRotate) //drawRotationPoint()
	{	
		//glColor3f(0,1,0);
		glEnable(GL_COLOR_LOGIC_OP);
		glColor3ub(0x80,0x80,0x80);
		glLogicOp(GL_XOR);

		double yoff = m_zoom*0.04;
		double xoff = yoff/m_aspect;

		/*Doesn't quite work without a depth buffer.
		glTranslated(m_xRotPoint,m_yRotPoint,0);
		rotatepoint_draw_manip((float)xoff,(float)yoff);
		glTranslated(-m_xRotPoint,-m_yRotPoint,0);*/
		glBegin(GL_QUADS);
		glVertex2d(m_xRotPoint-xoff,m_yRotPoint);
		glVertex2d(m_xRotPoint,m_yRotPoint-yoff);
		glVertex2d(m_xRotPoint+xoff,m_yRotPoint);
		glVertex2d(m_xRotPoint,m_yRotPoint+yoff);
		glEnd();

		glDisable(GL_COLOR_LOGIC_OP);
	}

	if(m_interactive) 
	if(!m_model->getViewportUnits().no_overlay_option) //2022
	{
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

		drawOverlay(m_scrollTextures);
	}

	//swapBuffers();

	//glPushAttrib doesn't cover
	//these states.
	glMatrixMode(GL_PROJECTION); 
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW); 
	glPopMatrix();
	glPopAttrib();
}
void TextureWidget::useLinesColor()
{
	//TESTING
	//https://github.com/zturtleman/mm3d/issues/95
	int c = m_linesColor;
	glColor4ub(c>>16&0xff,c>>8&0xff,c>>0&0xff,0x80);
}
void TextureWidget::useSelectionColor()
{
	int c = m_selectionColor;
	glColor3ub(c>>16&0xff,c>>8&0xff,c>>0&0xff);
}
void TextureWidget::sizeOverride(int width, int height)
{
	//m_textureWidget->resize(0,0);
	m_overrideWidth = width; m_overrideHeight = height;
	//updateSize();
}

int TextureWidget::getUvWidth()
{
	return m_texture?m_texture->m_width:1; 
}
int TextureWidget::getUvHeight()
{
	return m_texture?m_texture->m_height:1; 
}
