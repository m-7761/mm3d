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

#include "model.h"
#include "modelundo.h"
#include "toolbox.h"
#include "tool.h"
#include "glmath.h"

#include "log.h"
#include "modelstatus.h"
#include "texmgr.h"
#include "modelviewport.h"
#include "mm3dport.h"

#include "translate.h"

//https://github.com/zturtleman/mm3d/issues/99
//1.2?
//This seems unrelated to 1.2 used to add margins to
//the view. It must correspond to gluPerspective FOV.
static const double modelviewport_persp_factor = 2*1.2;

//static const double modelviewport_znear_factor = 0.1;
//static const double modelviewport_zfar_factor = 1000;

static const double modelviewport_ortho_factor = 5000/32;

void ModelViewport::getXY(int &x, int &y)
{
	parent->getXY(x,y); //circular dependency
}

ModelViewport::ModelViewport(Parent *parent)
	:
ScrollWidget(zoom_min,zoom_max),
parent(parent),
m_operation(MO_None),
m_view(Tool::ViewFront),
m_layer(1),
m_rotX(),m_rotY(),m_rotZ(),
m_width(),m_height(),
m_unitWidth(1),
m_background(),
m_backgroundTexture(),
m_rendering()
{
	m_interactive = true;

	/*BOGUS
	//https://github.com/zturtleman/mm3d/issues/97
	//m_nearOrtho = 0.001; //???
	//m_farOrtho = 1000000; //???
	//5000 seems not good for glPolygonOffset
	//should m_zoom be applied?
	m_nearOrtho = -5000;
	m_farOrtho = 5000;*/
	m_zoom = 32;
	m_nearOrtho = -modelviewport_ortho_factor;
	m_farOrtho = modelviewport_ortho_factor;
	
	//setAutoBufferSwap(false);

	//setFocusPolicy(Qt::WheelFocus);
	//setMinimumSize(220,180);
   
	//setAcceptDrops(true);
	//setMouseTracking(true); //QWidget

	//setCursor(Qt::ArrowCursor);
}

enum{ modelviewport_drawing_view=1 }; //TESTING

void ModelViewport::updateMatrix() //NEW
{
	if(m_view<Tool::ViewOrtho
	 &&m_view>Tool::ViewPerspective)
	{
		m_rotX = m_rotY = m_rotZ = 0; 		
		switch(m_view)
		{
		case Tool::ViewBack: m_rotY = 180; break;
		case Tool::ViewLeft: m_rotY = -90; break; //90
		case Tool::ViewRight: m_rotY = 90; break; //-90
		case Tool::ViewTop: m_rotX = -90; break;
		case Tool::ViewBottom: m_rotX = 90; break;
		}
	}

	m_viewMatrix.loadIdentity();

	if(m_view<Tool::ViewOrtho
	&&m_view>Tool::ViewPerspective) //NEW
	{
		m_viewMatrix.setRotationInDegrees(-m_rotX,-m_rotY,-m_rotZ);
	}
	else
	{
		//NOTE: Ortho views have a problem where the poles kind of
		//switch on one axis making it do in reverse before it can
		//straighten out. Maverick does this too. It's strange the
		//perspective isn't plagued by this
		//
		// UPDATE: I somehow solved this, by accident!! I suppose
		// this comment can be removed but I'd still like to know 
		// why it's solved. So I will leave it here a little more.
		//
		//https://github.com/zturtleman/mm3d/issues/144

		m_viewMatrix.setRotationInDegrees(m_rotX,m_rotY,m_rotZ);
	}

	double z = -m_scroll[2];
	//https://github.com/zturtleman/mm3d/issues/99
	//NEW: Misfit's persp mode hadn't originally allowed tool work.
	const double aspect = (double)m_viewportWidth/m_viewportHeight;
	if(1&&m_view<=Tool::ViewPerspective)
	{
		z-=m_zoom*modelviewport_persp_factor/std::min(1.0,aspect);
	}
	//YUCK: Need to keep ortho matrix even for perspective.
	//m_viewMatrix.setTranslation(-m_scroll[0],-m_scroll[1],z);
	//Ortho should be 0 right? Tool::addPosition is offsetting
	//according to m_scroll[2], but it didn't always do so?
	//https://github.com/zturtleman/mm3d/issues/141#issuecomment-651962335
	//m_viewMatrix.setTranslation(-m_scroll[0],-m_scroll[1],-m_scroll[2]);
		/*EXPERIMENTING
	//Trying to go back so "eye" can be extracted from view matrix.
	if(modelviewport_drawing_view)
	m_viewMatrix.setTranslation(-m_scroll[0],-m_scroll[1],z);
	else m_viewMatrix.setTranslation(-m_scroll[0],-m_scroll[1],0);
		*/
	m_viewMatrix.setTranslation(-m_scroll[0],-m_scroll[1],z);
	m_bestMatrix = m_viewMatrix; m_bestMatrix.getMatrix()[14] = 0;
	m_bestInverse = m_bestMatrix.getInverse();

	m_viewInverse = m_viewMatrix.getInverse();

	//NEW: It's probably incorrect to apply a perspective 
	//matrix, but it would produce better selection results.
	if(m_view<=Tool::ViewPerspective)
	{
		//YUCK: updateViewport gets these out of whack.
		double s,t; 
		aspect<1?t=(s=m_zoom)/aspect:s=(t=m_zoom)*aspect;
		m_width = s*2; m_height = t*2;

		//gluPerspective(45,aspect,m_zoom*0.1,m_zoom*1000);
		auto &vu = parent->getModel()->getViewportUnits();
	//	const double f = 1/tan(PI/8); //45/2
		const double f = 1/tan(PI/(180/(vu.fov/2)));
	//	const double znear = m_zoom*modelviewport_znear_factor;
	//	const double zfar = m_zoom*modelviewport_zfar_factor;
		const double znear = m_zoom*vu.znear;
		const double zfar = m_zoom*vu.zfar;
		double proj[4][4] = {};		
		proj[0][0] = f/aspect;
		proj[1][1] = f;
		//Reverse back-face sense from gluPerspective.
		//proj[2][2] = (zfar+znear)/(znear-zfar);
		proj[2][2] = (zfar+znear)/(zfar-znear);
		proj[2][3] = -1; 
		//Reverse back-face sense from gluPerspective.
		//proj[3][2] = 2*zfar*znear/(znear-zfar);
		proj[3][2] = 2*zfar*znear/(zfar-znear);
		
		//Scale to getRawParentXYValue coordinate system.
		proj[0][0]*=m_width/2;
		proj[1][1]*=m_height/2;

		//2019: This is equivalent to doing
		//glLoadMatrixd(this);
		//glMultMatrixd(lhs);
		//m_viewMatrix.postMultiply(*(Matrix*)proj);
	//	std::swap(z,m_viewMatrix.getMatrix()[14]); //OBSOLETE
		assert(z==m_viewMatrix.getMatrix()[14]);
		((Matrix*)proj)->postMultiply(m_viewMatrix);
	//	std::swap(z,m_viewMatrix.getMatrix()[14]); //OBSOLETE
		//memcpy(&m_viewMatrix,proj,sizeof(proj));
		memcpy(&m_projMatrix,proj,sizeof(proj));

		m_unprojMatrix = m_projMatrix.getInverse();
	}
	else
	{
		m_projMatrix = m_viewMatrix;
		m_unprojMatrix = m_viewInverse;
	}	

	parent->updateView();
}

static int modelviewport_opts(int drawMode)
{		
	int o = Model::DO_SMOOTHING;
	/*if(config->get("ui_render_bad_textures",false))
	o|=Model::DO_BADTEX;
	if(config->get("ui_render_backface_cull",false))
	o|=Model::DO_BACKFACECULL;*/
	switch(drawMode&7)
	{
	case ModelViewport::ViewFlat: o = 0; break; //Model::DO_NONE;
	case ModelViewport::ViewSmooth: o = Model::DO_SMOOTHING; break;
	case ModelViewport::ViewTexture: o|=Model::DO_TEXTURE; break;
	case ModelViewport::ViewAlpha: o|=Model::DO_ALPHA|Model::DO_TEXTURE; break;
	}
	if(drawMode>ModelViewport::ViewAlpha)
	{
		if(drawMode&ModelViewport::ViewInflColors)
		o|=Model::DO_INFLUENCE;
		if(drawMode&ModelViewport::ViewVColors)
		o|=Model::DO_VERTEXCOLOR;
	}
	return o;
}

void ModelViewport::draw(int x, int y, int w, int h, int vc)
{	
	if(1) //Parent::initializeGL(parent->getModel());
	{
		glEnable(GL_DEPTH_TEST);
	}

	Model *model = parent->getModel();

	//HACK: can this be automated?
	model->setPrimaryLayers(m_layer); //1 only?

	if(m_rendering)
	{
		assert(!w);
		x = m_viewportX; w = m_viewportWidth;
		y = m_viewportY; h = m_viewportHeight;
	}
	else
	{
		//NEW: Trying to have tools in perspective.
		//HACK: Adapt to new size... assuming user
		//interaction support is all that's needed.
		bool cmp = w!=m_viewportWidth||h!=m_viewportHeight;
		{
			m_viewportX = x; m_viewportWidth = w;
			m_viewportY = y; m_viewportHeight = h;
		}
		//NOTE: Ortho seems to shape invariant.
		if(cmp&&m_view<=Tool::ViewPerspective)
		{
			updateMatrix();
		}
	}

	if(x<0||y<0||w<=0||h<=0) return;
	
	glScissor(x,y,w,h); //NEW

	double aspect = (double)w/h;

	//void setViewportDraw()
	{
		glViewport(x,y,w,h);

		glMatrixMode(GL_PROJECTION); glLoadIdentity();
				
		double s,t;
		aspect<1?t=(s=m_zoom)/aspect:s=(t=m_zoom)*aspect;
		
		if(m_view>Tool::ViewPerspective)
		{
			//2020: m_zoom has already been factored into
			//m_near/farOrtho
			if(!modelviewport_drawing_view) //REMOVE ME
			glOrtho(m_scroll[0]-s,m_scroll[0]+s,
			m_scroll[1]-t,m_scroll[1]+t,m_nearOrtho,m_farOrtho);
			else
			glOrtho(-s,+s,-t,+t,m_nearOrtho,m_farOrtho);
		}
		else //gluPerspective(45,aspect,m_zoom*0.002,m_zoom*2000); //GLU
		{
			auto &vu = parent->getModel()->getViewportUnits();
			double zn = m_zoom*vu.znear;
			double zf = m_zoom*vu.zfar;
			//double zn = modelviewport_znear_factor;
			//double zf = modelviewport_zfar_factor;			
			gluPerspective(vu.fov,aspect,m_zoom*zn,m_zoom*zf); //GLU
		}

		s*=2; t*=2;
	//	if(m_width!=s||m_height!=t) //vu.grid???
		{
			m_width = s; m_height = t;
			m_unitWidth = getUnitWidth();
		}
				
		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	}

	if(m_rendering) //animexportwin? 
	{
		//??? Undoing old focus effect?
		//glClearColor(130.0/256.0,200.0/256.0,200.0/256.0,1);		

		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); //NEW
	}	

	if(modelviewport_drawing_view) //TESTING 
	{
		//NOTE: I tried using m_projMatrix by itself (above)
		//and it didn't work... maybe OpenGL needs both???
		glLoadMatrixd(m_viewMatrix.getMatrix());
	}
	else
	{
		double _viewPoint[4] = { 0,0,0,1 };
		if(m_view>=Tool::ViewOrtho
		 ||m_view<=Tool::ViewPerspective)
		{
			//glEnable(GL_LIGHT0); //???
		
			_viewPoint[0] = m_scroll[0]; 
			_viewPoint[1] = m_scroll[1]; 
			_viewPoint[2] = m_scroll[2]; 
			if(m_view<=Tool::ViewPerspective)
			{
				//https://github.com/zturtleman/mm3d/issues/99
				_viewPoint[2]+=m_zoom*modelviewport_persp_factor/std::min(1.0,aspect);

				glTranslated(-_viewPoint[0],-_viewPoint[1],-_viewPoint[2]);
			}
			else 
			{
				_viewPoint[2]+=m_zoom*2;

				//https://github.com/zturtleman/mm3d/issues/97
				//glTranslatef(0,0,-m_farOrtho/2);
				glTranslated(0,0,-(m_farOrtho+m_nearOrtho)/2);
			}		

			glRotated(m_rotZ,0,0,1);
			glRotated(m_rotY,0,1,0);
			glRotated(m_rotX,1,0,0);

			Matrix m; //???
			m.setRotationInDegrees(0,0,-m_rotZ);
			m.apply3(_viewPoint);
			m.setRotationInDegrees(0,-m_rotY,0);
			m.apply3(_viewPoint);
			m.setRotationInDegrees(-m_rotX,0,0);
			m.apply3(_viewPoint);
		}
		else
		{
			//glDisable(GL_LIGHT0); //???
			//glDisable(GL_LIGHT1); //???

			//https://github.com/zturtleman/mm3d/issues/97
			_viewPoint[0] = 0;
			_viewPoint[1] = 0;
			_viewPoint[2] = (m_farOrtho+m_nearOrtho)/2; //500000;

			glTranslated(-_viewPoint[0],-_viewPoint[1],-_viewPoint[2]);

			Matrix m; //???
			switch(m_view)
			{
			case Tool::ViewFront:
				// do nothing
				break;
			case Tool::ViewBack:
				glRotatef(180,0,1,0);
				m.setRotationInDegrees(0,-180,0);
				break;
			case Tool::ViewLeft:
				glRotatef(90,0,1,0); //-90
				m.setRotationInDegrees(0,-90,0); //90
				break;
			case Tool::ViewRight:
				glRotatef(-90,0,1,0); //90
				m.setRotationInDegrees(0,90,0); //-90
				break;
			case Tool::ViewTop:
				glRotatef(90,1,0,0);
				m.setRotationInDegrees(-90,0,0);
				break;
			case Tool::ViewBottom:
				glRotatef(-90,1,0,0);
				m.setRotationInDegrees(90,0,0);
				break;
			//default:
				//log_error("Unknown ViewDirection: %d\n",m_view);
				//swapBuffers();
				//return;
			}
			m.apply4(_viewPoint);
		}
	}

	glColor3f(0.7f,0.7f,0.7f); //???

	//2020: Intel can't do negative offset on lines because it
	//seems to sometimes flip the sign on the lines' depth, so
	//lines in the back jump in front of polygons in the front
	const bool poffset = true;

	int drawMode;
	bool drawSelections = true;	
	if(m_view<=Tool::ViewPerspective)
	{			
		drawMode = model->getPerspectiveDrawMode();
		if(drawMode!=ViewWireframe)
		{
			//drawSelections = config->get("ui_render_3d_selections",false);
			drawSelections = model->getDrawSelection();
		}
	}
	else
	{
		drawMode = model->getCanvasDrawMode();

		drawBackground(); // Draw background
		
		//glClear(GL_DEPTH_BUFFER_BIT); //???
	}

	double *bspEye = 0; int opt = 0;

	glEnable(GL_DEPTH_TEST);
	if(drawMode!=ViewWireframe)
	{
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		
		//ContextT was because every view was its own OpenGL context
		//model->draw(opt,static_cast<ContextT>(this),_viewPoint);
		//model->draw(modelviewport_opts(drawMode),nullptr,_viewPoint);
		opt = modelviewport_opts(drawMode);
		if(opt&Model::DO_ALPHA)
		{
			opt|=Model::DO_ALPHA_DEFER_BSP;
			bspEye = m_viewInverse.getVector(3);
		}
		if(opt&Model::DO_INFLUENCE) 
		{
			opt|=vc<<12; //DO_INFLUENCE_VIEWPORT
		}

		if(poffset)
		if(int off=(bspEye?1:0)+drawSelections)
		{
			//GL_CULL_FACE doesn't play nice with this on my system
			//with a light cube it shows the dark back faces along 
			//the exterior edges
			glEnable(GL_POLYGON_OFFSET_FILL);

			//NOTE: this is what works but I'm not sure it's what the
			//specification prescribes. I think OpenGL is just a mess
			//in terms of how it's been implemented historically. The
			//original Misift (or Maverick code used 1,0 as well)
			//
			//Somtimes there's slight z-fighting with the grid with 1
			//(with a flat polygon)
			//glPolygonOffset(1,0);
			//glPolygonOffset(1.5,0); //fails forward facing polygons
			glPolygonOffset(1,off);
		}

		//this is to make the grid consistently in front of polygons
		//in the six standard orther directions
		glDepthRange(0.00001,1);
		{	 			
			if(opt&Model::DO_TEXTURE)
			{
				//2022: by default this is now set to 0, but it's a
				//little dark in texture mode because it's setup to
				//ensure the wireframes remain visible in solid mode
				//and the lighting is same as before.
				float brighter[4] = {0.1f,0.1f,0.1f};
				glLightModelfv(GL_LIGHT_MODEL_AMBIENT,brighter);
			}
			model->draw(opt,nullptr,bspEye);
		}
		glDepthRange(0,1);

		if(poffset) glDisable(GL_POLYGON_OFFSET_FILL);
	}

	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);

	if(drawSelections)
	{
		if(!poffset)
		{
			//Intel uses FILL
			glEnable(GL_POLYGON_OFFSET_FILL);
			glEnable(GL_POLYGON_OFFSET_LINE);
			assert(0); //0 won't do, what would? 1,-1?
			glPolygonOffset(-1,0);
		}

		//Using glPolygonOffset seems to work on the grid
		//but I got very strange results using it on the 
		//lines on my system. I think using it on polygons
		//causes back-faces to bleed through along edges.
		//With lines (negative offset) long lines far back
		//in the model momentarily pop up in front of the
		//polygons. It doesn't make a lot of sense, so it 
		//could be a driver thing.

		model->drawLines(0.5f);
		model->drawVertices(!parent->tool->isNullTool());

		if(!poffset) glDisable(GL_POLYGON_OFFSET_FILL);
		if(!poffset) glDisable(GL_POLYGON_OFFSET_LINE);
	}

	if(!m_rendering) //animexportwin?
	{
		//2019: I'm moving this to after so it isn't
		//blended behind drawLines

		/*EXPERIMENTAL
		if(drawMode!=ViewWireframe)
		if(drawSelections
		&&(m_view>Tool::ViewPerspective
		||model->getPerspectiveDrawMode()<ViewTexture))*/
		if(drawMode!=ViewWireframe)
		if(parent->background_grid[m_view<=Tool::ViewPerspective])
		{
			//This is done implicitly by glDisable(GL_DEPTH_TEST)
			//glDepthMask(0);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			{
				drawGridLines(0.15f);
			}
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
			//glDepthMask(1);
		}
		
		//GL_LESS is intended to prefer wireframes
		//over the grid lines
		glDepthFunc(GL_LESS);
		//On my system there's often ugly artifacts
		//like the depth-buffer is uneven, so this
		//is designed to resolve wireframes snapped
		//to the grid. Note, it's probably rounded
		//up to the minimum nonzero value
		if(drawSelections)
		glDepthRange(0.00001,1);		
		{
			//NOTE: I had a z-fighting issue due to 
			//glPolygonOffset on my Intel system. I 
			//found out it goes away if the second
			//parameter is left to be 0... but this
			//is (actually) because 0 doesn't offset
			//forward facing polygons! Note, Misfit
			//(or Maverick) was using 0

			drawGridLines(1,!drawSelections);
		}
		glDepthRange(0,1);
		glDepthFunc(GL_LEQUAL);
	}

	//This should be X pixels.
	//float axis = (float)m_zoom/10;
	//This is how snapping is done in projection space.
	//It just has to be moved into real space. 25 is 2
	//arrow cursors tall on my system.
	float axis = (float)std::min(25.0/w*m_width,25.0/h*m_height);
	{
		//This seems to work but there's surely better
		//ways to compute it. Maybe not though if it's
		//to account for scrolling.
		Vector p; m_projMatrix.apply4(p);
		//If only one is done it shrinks away from the 
		//side view.
		p[0]+=axis*p[3];
		p[1]+=axis*p[3]; m_unprojMatrix.apply4(p);
		axis = (float)p.mag3();
	}
	
	if(bspEye&&!model->draw_bspTree().empty())
	{
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
	
		//REMINDER: This has to offset more than opaque
		//polygons since cuts made by bsptree.cc cannot
		//match exactly for GL_LEQUAL.
		if(poffset&&drawSelections)
		{
			glEnable(GL_POLYGON_OFFSET_FILL);

			glPolygonOffset(1,1);
		}

		//TESTING SOMETHING
	//	glDisable(GL_LIGHTING);
		//model->draw_bspTree(opt|Model::DO_WIREFRAME,nullptr,bspEye);
	//	glEnable(GL_LIGHTING);

		//there are some grid artifacts but this
		//doesn't seem to help
		glDepthRange(0.00001,1);
		{
			model->draw_bspTree(opt,nullptr,bspEye);
		}
		glDepthRange(0,1);

		if(poffset) glDisable(GL_POLYGON_OFFSET_FILL);

		glDisable(GL_LIGHTING);
	}
	if(opt&Model::DO_TEXTURE)
	{
		float oooo[4] = {};
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT,oooo);
	}

	if(drawSelections)
	{
		model->drawJoints(0.333333f,axis);
	}

	glDisable(GL_DEPTH_TEST);
	
	model->drawPoints(axis);
	model->drawProjections();	

	if(!m_rendering) //animexportwin?
	{
		//drawOrigin();
		{
			glBegin(GL_LINES);
			glColor3f(1,0,0);
			glVertex3f(0,0,0); glVertex3f(axis,0,0);
			glColor3f(0,1,0);
			glVertex3f(0,0,0); glVertex3f(0,axis,0);
			glColor3f(0,0,1);
			glVertex3f(0,0,0); glVertex3f(0,0,axis);
			glEnd();
		}
	}

	//TODO: Probably ought to break this up into two
	//stages, one in model space, one in screen space.
	//The SelectTool should draw in screen space, so
	//it can not use _getParentZoom.
//	if(m_view>Tool::ViewPerspective
//	 ||model->getDrawSelection()) //TESTING	//need to see colortool.cc cursor
	{
		parent->drawTool(this);

		//Don't trust drawTool?
		//glDisable(GL_DEPTH_TEST);
	}

	if(!m_rendering
	 &&!model->getViewportUnits().no_overlay_option) //2022
	{
		drawOverlay(parent->m_scrollTextures);
	}
	else //HACK: My client code is failing to draw a viewport outline.
	{
		//2022: drawOverlay had done this? Inappropriate? (FIX ME)

		//setViewportOverlay();
		{
			//glViewport(0,0,(GLint)m_viewportWidth,(GLint)m_viewportHeight);

			glMatrixMode(GL_PROJECTION); glLoadIdentity();

			glOrtho(0,w,0,h,m_nearOrtho,m_farOrtho); //-1,1);

			//glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

			glMatrixMode(GL_MODELVIEW); glLoadIdentity();
		}
	}

	//2019: Testing correctness of updateMatrix w/ projection.
	#if 0 && defined(_DEBUG)
	if(m_view<=Tool::ViewPerspective)
	{
		//TODO: Would rather do this to fill a selection buffer.

		glTranslated(w/2,h/2,0);
		glBegin(GL_POINTS);			
		double ww = w/m_width;
		double hh = h/m_height;
		for(int i=0,iN=model->getVertexCount();i<iN;i++)
		{
			//I think gluProject neglects to mention homogeneous
			//divide.
			//https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluProject.xml
			Vector v;
			model->getVertexCoords(i,v.getVector());
			m_projMatrix.apply(v);
			v[0]/=v[3]; v[1]/=v[3];
			glVertex2d(v[0]*ww,v[1]*hh);
		}
		glEnd();
		glLoadIdentity(); //HACK
	}
	#endif

	//REMOVE ME
	//ModelViewport needs to be separate 
	//from the UI.
	Parent::checkGlErrors(model);

	//swapBuffers();
}

void ModelViewport::drawGridLines(float a, bool offset3d)
{
	glColor4f(0.55f,0.55f,0.55f,a);

	Model *model = parent->getModel();
	Model::ViewportUnits &vu = model->getViewportUnits();

	if(m_view<=Tool::ViewPerspective
	 ||m_view>=Tool::ViewOrtho)
	{		
		if(!vu.xyz3d) return;

		//NOTE: Artifacts remain in perspective
		//views with lines & selection disabled.		
		double os = 0, dp[3] = {0,0,1};
		if(offset3d) m_viewInverse.apply3(dp);
		//float *eye = _viewPoint; //REMOVE ME
		double *eye = m_viewInverse.getVector(3);		
		auto osf = [&](double x, bool test)
		{
			//HARDWARE SENSITIVE?
			//I think I noticed a zoom level
			//that seemed to work well
			static const double oss = 0.005; //1/200

			//This breaks down under scrolling.
			//double f = m_zoom*(x>0?-oss:oss);
			double f = m_zoom*(test?-oss:oss);
			x = 1-fabs(x);

			//1-osr wasn't based on anything really
			//for some reason small viewports break
			//down (hardware sensitive) and similar
			//artifacts sometimes show up. It helps
			//to increase the right term's factor
			//os = x>0.99998f?0:f*osr+(1-osr)*f*sqrt(x);
			os = x>0.99998f?0:f*0.1+(1.5)*f*sqrt(x);
		};

		//double inc = config->get("ui_3dgrid_inc",4.0);
		double inc = vu.inc3d;
		//double max = config->get("ui_3dgrid_count",6)*inc;
		double max = vu.lines3d*inc;
		double x,y,z;
		
		glBegin(GL_LINES);

		//if(config->get("ui_3dgrid_xy",false))
		if(vu.xyz3d&4)
		{
			if(offset3d) osf(dp[2],eye[2]>0);

			for(x=-max;x<=max;x+=inc)
			{
				glVertex3d(x,-max,os); glVertex3d(x,+max,os);
			}
			for(y=-max;y<=max;y+=inc)
			{
				glVertex3d(-max,y,os); glVertex3d(+max,y,os);
			}
		}
		//if(config->get("ui_3dgrid_xz",true))
		if(vu.xyz3d&2)
		{
			if(offset3d) osf(dp[1],eye[1]>0);

			for(x=-max;x<=max;x+=inc)
			{
				glVertex3d(x,os,-max); glVertex3d(x,os,+max);
			}
			for(z=-max;z<=max;z+=inc)
			{
				glVertex3d(-max,os,z); glVertex3d(+max,os,z);
			}
		}
		//if(config->get("ui_3dgrid_yz",false))
		if(vu.xyz3d&1)
		{
			if(offset3d) osf(dp[0],eye[0]>0);

			for(y=-max;y<=max;y+=inc)
			{
				glVertex3d(os,y,-max); glVertex3d(os,y,+max);
			}
			for(z=-max;z<=max;z+=inc)
			{
				glVertex3d(os,-max,z); glVertex3d(os,+max,z);
			}
		}

		glEnd();
	}
	else if(double uw=m_unitWidth) //getUnitWidth()
	{
		//if(uw<=0) return; //May be 0. 
		assert(uw>0); 

		glBegin(GL_LINES);
		
		double xRangeMin = m_scroll[0];
		double xRangeMax = m_scroll[0];
		double yRangeMin = m_scroll[1];
		double yRangeMax = m_scroll[1];

		switch(m_view)
		{
		case Tool::ViewBack: 
		
		//2019: CHANGING SENSE.
		//case Tool::ViewLeft:
		case Tool::ViewRight:

			xRangeMin*=-1; xRangeMax*=-1; break;
			
		case Tool::ViewTop: 
			
			yRangeMin*=-1; yRangeMax*=-1; break;
		}

		xRangeMin-=m_width/2; yRangeMin-=m_height/2;
		xRangeMax+=m_width/2; yRangeMax+=m_height/2;
		
		double x,xStart = uw*(int)(xRangeMin/uw);
		double y,yStart = uw*(int)(yRangeMin/uw);
		
		//NEW: Fix sometimes visible rounding error?
		xStart-=uw; xRangeMax+=uw;
		yStart-=uw; yRangeMax+=uw;

		//TODO: Don't use double for this? Draw in 2D?
		switch(m_view)
		{
		case Tool::ViewFront: case Tool::ViewBack:
			
			for(x=xStart;x<xRangeMax;x+=uw)
			{
				glVertex3d(x,yRangeMin,0);
				glVertex3d(x,yRangeMax,0);
			}
			for(y=yStart;y<yRangeMax;y+=uw)
			{
				glVertex3d(xRangeMin,y,0);
				glVertex3d(xRangeMax,y,0);
			}
			break;

		case Tool::ViewLeft: case Tool::ViewRight:
			
			for(x=xStart;x<xRangeMax;x+=uw)
			{
				glVertex3d(0,yRangeMin,x);
				glVertex3d(0,yRangeMax,x);
			}
			for(y=yStart;y<yRangeMax;y+=uw)
			{
				glVertex3d(0,y,xRangeMin);
				glVertex3d(0,y,xRangeMax);
			}
			break;

		case Tool::ViewTop: case Tool::ViewBottom:
			
			for(x=xStart;x<xRangeMax;x+=uw)
			{
				glVertex3d(x,0,yRangeMin);
				glVertex3d(x,0,yRangeMax);
			}
			for(y=yStart;y<yRangeMax;y+=uw)
			{
				glVertex3d(xRangeMin,0,y);
				glVertex3d(xRangeMax,0,y);
			}
			break;

		//default: 
		//	log_error("Unhandled view direction: %d\n",m_view);
		//	break;
		}

		glEnd();
			
		/*REMOVE ME
		if(config->get("ui_render_text",false))
		{
			glColor3f(0.35f,0.35f,0.35f);
			glRasterPos3f(xRangeMin,yRangeMin,0);
			switch(m_view)
			{
			case Tool::ViewFront: case Tool::ViewBack:
			
				glRasterPos3f(xRangeMax,yRangeMin,0); break;

			case Tool::ViewLeft: case Tool::ViewRight:
			
				glRasterPos3f(0,yRangeMin,xRangeMin); break;

			case Tool::ViewTop: case Tool::ViewBottom:
			
				glRasterPos3f(xRangeMin,0,yRangeMin); break;
			}

			// Broken with Qt4+non-accelerated nVidia card (possibly other configs as
			// well). That is why it is disabled by default. You can set ui_render_text
			// to a non-zero value in your mm3drc file to enable text rendering.
			QString text; text.sprintf("%g",uw);
			renderText(2,this->height()-12,text,QFont("Sans",10));
		}*/
	} 
}
void ModelViewport::drawBackground()
{	
	if(!updateBackground()) return;

	Model *model = parent->getModel();
		
	//https://github.com/zturtleman/mm3d/issues/102	
	if(Texture::FORMAT_RGBA==m_background->m_format)
	glEnable(GL_BLEND);

	glDisable(GL_LIGHTING);
	glColor3f(1,1,1);

	int index = m_view-1;
		
	float w = (float)m_background->m_origWidth;
	float h = (float)m_background->m_origHeight;
	float dimMax = std::max(w,h);

	float cenX,cenY,cenZ;
	model->getBackgroundCenter(index,cenX,cenY,cenZ);

	glBindTexture(GL_TEXTURE_2D,m_backgroundTexture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glEnable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);

	//SIMPLIFY ME
	//SIMPLIFY ME
	//SIMPLIFY ME
	float minX,minY,minZ;
	float maxX,maxY,maxZ;
	float normX = 0, normY = 0, normZ = 0;
	float scale = (float)model->getBackgroundScale(index);
	//TODO: Assuming this wants background
	//to slice through 0,0.
	//https://github.com/zturtleman/mm3d/issues/97
	//float ortho = m_farOrtho/2-0.1f; //???
	//float ortho = -0.1f; //???
	float ortho = (float)m_farOrtho-0.01f; //???
	switch(m_view)
	{
	case Tool::ViewFront:
		minZ  =  maxZ = -ortho;
		minX  = -scale *(w/dimMax)+cenX;
		maxX  =  scale *(w/dimMax)+cenX;
		minY  = -scale *(h/dimMax)+cenY;
		maxY  =  scale *(h/dimMax)+cenY;
		normZ =  1;
		break;

	case Tool::ViewBack:
		minZ  =  maxZ = ortho;
		minX  =  scale *(w/dimMax)+cenX;
		maxX  = -scale *(w/dimMax)+cenX;
		minY  = -scale *(h/dimMax)+cenY;
		maxY  =  scale *(h/dimMax)+cenY;
		normZ = -1;
		break;

	//2019: CHANGING SENSE
	case Tool::ViewLeft: //case Tool::ViewRight:
		minX  =  maxX = ortho;
		minZ  = -scale *(w/dimMax)+cenZ;
		maxZ  =  scale *(w/dimMax)+cenZ;
		minY  = -scale *(h/dimMax)+cenY;
		maxY  =  scale *(h/dimMax)+cenY;
		normX =  1;
		break;
	//2019: CHANGING SENSE.
	case Tool::ViewRight: //case Tool::ViewLeft:
		minX  =  maxX = -ortho;
		minZ  =  scale *(w/dimMax)+cenZ;
		maxZ  = -scale *(w/dimMax)+cenZ;
		minY  = -scale *(h/dimMax)+cenY;
		maxY  =  scale *(h/dimMax)+cenY;
		normX = -1;
		break;

	case Tool::ViewTop:
		minY  =  maxY = -((float)m_farOrtho/2-0.1f);
		minX  = -scale *(w/dimMax)+cenX;
		maxX  =  scale *(w/dimMax)+cenX;
		minZ  =  scale *(h/dimMax)+cenZ;
		maxZ  = -scale *(h/dimMax)+cenZ;
		normY =  1;
		break;

	case Tool::ViewBottom:
		minY  =  maxY = (float)m_farOrtho/2-0.1f;
		minX  = -scale *(w/dimMax)+cenX;
		maxX  =  scale *(w/dimMax)+cenX;
		minZ  = -scale *(h/dimMax)+cenZ;
		maxZ  =  scale *(h/dimMax)+cenZ;
		normY = -1;
		break;
	}

	if(m_view==Tool::ViewLeft||m_view==Tool::ViewRight)
	{
		glNormal3f(normX,normY,normZ);
		glTexCoord2f(0,0);
		glVertex3f(minX, minY,minZ);
		glNormal3f(normX,normY,normZ);
		glTexCoord2f(0,1);
		glVertex3f(maxX, maxY,minZ);
		glNormal3f(normX,normY,normZ);
		glTexCoord2f(1,1);
		glVertex3f(maxX, maxY,maxZ);
		glNormal3f(normX,normY,normZ);
		glTexCoord2f(1,0);
		glVertex3f(minX, minY,maxZ);
	}
	else
	{
		glNormal3f(normX,normY,normZ);
		glTexCoord2f(0,0);
		glVertex3f(minX, minY,minZ);
		glNormal3f(normX,normY,normZ);
		glTexCoord2f(1,0);
		glVertex3f(maxX, minY,minZ);
		glNormal3f(normX,normY,normZ);
		glTexCoord2f(1,1);
		glVertex3f(maxX, maxY,maxZ);
		glNormal3f(normX,normY,normZ);
		glTexCoord2f(0,1);
		glVertex3f(minX, maxY,maxZ);
	}

	glEnd();

	//https://github.com/zturtleman/mm3d/issues/88
	glDisable(GL_TEXTURE_2D);

	//https://github.com/zturtleman/mm3d/issues/102
	glDisable(GL_BLEND);
}
  
double ModelViewport::getUnitWidth()
{
	Model *model = parent->getModel();
	if(!model) return 1; //2022
	Model::ViewportUnits &vu = model->getViewportUnits();

	if(m_view>=Tool::ViewOrtho
	 ||m_view<=Tool::ViewPerspective)
	{
		//return config->get("ui_3dgrid_inc",4.0);
		return vu.inc3d;
	}

	//zero divide? infinite loop?
	//0.00001 is taken from viewportsettings.cc
	//double unitWidth = config->get("ui_grid_inc",4.0);
	double unitWidth = std::max(0.00001,vu.inc); 

	double ratio;
	int scale_min,scale_max;

	// Note early return for fixed width ('case 2:')
	//switch(config->get("ui_grid_mode",0))
	switch(vu.grid)
	{
	default:
	case vu.BinaryGrid: //0  // Binary
		ratio = 2;
		scale_min = 4; scale_max = 16;
		break;
	case vu.DecimalGrid:  //1 // Decimal
		ratio = 10;
		scale_min = 2; scale_max = 60;
		break;
	case vu.FixedGrid:  //2 // Fixed
		return unitWidth;
		break;
	}

	if(m_width) //Prior to drawing?
	{
		double maxDimension = m_width>m_height?m_width:m_height;
		while(maxDimension/unitWidth>scale_max)
		{
			unitWidth*=ratio;
		}
		while(maxDimension/unitWidth<scale_min)
		{
			unitWidth/=ratio;
		}
	}
	return unitWidth;
}

bool ModelViewport::updateBackground()
{
	Model *model = parent->getModel();
	const char *file;
	if(file=model->getBackgroundImage(m_view-1))
	{
		bool ret = *file!='\0';
		bool diff = file!=m_backgroundFile; 
		if(diff) m_backgroundFile = file;	
		if(!diff||!ret) return ret;
	}
	else return false;
		
	if(!m_backgroundTexture) glGenTextures(1,&m_backgroundTexture);

	m_background = TextureManager::getInstance()->getTexture(file,false,false);
	if(!m_background)
	{
		//TRANSLATE("LowLevel","Could not load background %1"))
		model_status(model,StatusError,STATUSTIME_LONG,
		TRANSLATE("LowLevel","Could not load background %s"),file);
		m_background = TextureManager::getInstance()->getDefaultTexture(file);
	}
	glBindTexture(GL_TEXTURE_2D,m_backgroundTexture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP); //NEW
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP); //NEW

	GLuint format = m_background->m_format;
	format = format==Texture::FORMAT_RGBA?GL_RGBA:GL_RGB;
	
	//https://github.com/zturtleman/mm3d/issues/85
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);

	/*FIX ME //Mipmaps option?
	glTexImage2D(GL_TEXTURE_2D,0,format,
	m_background->m_width,m_background->m_height,0,
	format,GL_UNSIGNED_BYTE,m_background->m_data.data());*/
	gluBuild2DMipmaps(GL_TEXTURE_2D,format, //GLU
	m_background->m_width,m_background->m_height,							
	format,GL_UNSIGNED_BYTE,m_background->m_data.data()); return true;
}

void ModelViewport::updateViewport(int how)
{
	if(how=='z') //HACK: m_zoom changed?
	{
		//factoring in m_zoom improves glPolygonOffset
		m_farOrtho = m_zoom*modelviewport_ortho_factor; 
		m_nearOrtho = -m_farOrtho; 

		parent->zoomLevelChangedEvent(*this);
	
		//getUnitWidth is invalid if uninitialized
		//and this way "Press F1 for help" is seen.
		if(m_viewportWidth&&m_viewportHeight)
		{
			m_unitWidth = getUnitWidth();

			model_status(parent->getModel(),StatusNormal,STATUSTIME_NONE,"Unit: %g",m_unitWidth);
		}
	}

	updateMatrix();
}

void ModelViewport::wheelEvent(int wh, int bs, int x, int y)
{
	//bool rotate = (e->modifiers()&Qt::ControlModifier)!=0;
	if(bs&Tool::BS_Ctrl&&~bs&Tool::BS_Alt)
	return wh>0?rotateClockwise():rotateCounterClockwise();

	//This is for BS_Shift (zoom all viewports) but covers
	//any case out-of-bounds use case while accounting for
	//the cursor in the main viewport
	if(!over(x,y)) return wh>0?zoomIn():zoomOut();

	double xDiff,yDiff;
	getRawParentXYValue(x-m_viewportX,y-m_viewportY,xDiff,yDiff);
	zoom(wh>0,xDiff+m_scroll[0],yDiff+m_scroll[1]);
}
bool ModelViewport::_rmb_rotates_view = false; //2021
bool ModelViewport::mousePressEvent(int bt, int bs, int x, int y)
{	
	Model *model = parent->getModel();

	if(m_activeButton==bt) //DEBUGGING?
	{
		//NOTE: TextureWidget has m_buttons, which
		//I don't know, maybe ModelViewport should
		//too?
		
		//HACK: In case the button state is confused
		//let the user unstick buttons by pressing
		//the button. (Hopefully this only happens
		//when using the debugger "break" function.)
		m_activeButton = 0;

		model_status(model,StatusError,STATUSTIME_LONG,
		TRANSLATE("LowLevel","Warning: Unstuck confused mouse button state"));
	}

	//printf("press = %d\n",e->button());
	//if(m_activeButton!=Qt::NoButton)
	if(m_activeButton)
	{
		//e->ignore(); //???
		return true;
	}

	//e->accept(); //???
	m_activeButton = bt; //e->button();
	
	int w = m_viewportWidth; //this->width();
	int h = m_viewportHeight; //this->height();

	//bool rotate = (e->modifiers()&Qt::ControlModifier)!=0;
	bool rotate = (bs&Tool::BS_Ctrl)!=0;
	
	if(!model->getViewportUnits().no_overlay_option //2022
	&&pressOverlayButton(x,y,rotate))	
	{
		if(m_overlayButton==ScrollButtonPan)
		{
			m_scrollStartPosition = {x,y};
			m_operation = rotate?MO_Rotate:MO_Pan;
		}
		else 
		{
			//UNUSED
			//m_operation = rotate?MO_RotateButton:MO_PanButton; 
			m_operation = MO_None;
		}
	}
	else 
	{
		//m_operation = MO_None;
		if(m_operation!=MO_None) return true; //NEW

		//if(e->button()!=Qt::MidButton)
		if(bt!=Tool::BS_Middle)
		{
			bool persp = m_view<=Tool::ViewPerspective
			&&!parent->getModel()->getDrawSelection();
			bool def = persp||parent->tool->isNullTool();

			//if(def&&bt==Tool::BS_Left||bs&Tool::BS_Ctrl)
			if((def||bs&Tool::BS_Ctrl)&&~bs&Tool::BS_Alt)
			{
				bool pan = (bt&Tool::BS_Right)!=0;
				if(_rmb_rotates_view) pan = !pan;

				m_operation = pan?MO_Pan:MO_Rotate;
			}
			else
			{
				//HACK: can this be automated?
				model->setPrimaryLayers(m_layer); //1 only?

				m_operation = MO_Tool;
				Tool *tool = parent->tool;
				tool->mouseButtonDown(bt|bs,x-m_viewportX,y-m_viewportY);
			}
		}
		else m_operation = MO_Pan;

		if(m_operation!=MO_Tool)
		{
			m_scrollStartPosition = {x,y};

			//2022: This had been done in getParentXYZValue but never
			//appears in the status bar for reasons I'm uncertain of?
			//It doesn't make much sense there. I think it used to be
			//shown exclusively over the viewport but some version of
			//Qt made that impractical. Putting it here seems best to
			//be able to invoke it manally by clicking and to not act
			//like spam!
			if(m_operation==MO_Pan)
			if(m_view>Tool::ViewPerspective)
			{
				double pos[3]; pos[2] = 0;
				getRawParentXYValue(x-m_viewportX,y-m_viewportY,pos[0],pos[1]);
				//m_viewInverse.apply(pos);
				m_bestInverse.apply3x(pos);				
				
				double cmp = std::min(m_width,m_height);
				char p[4] = ".6f";
				if(cmp>=0.01) p[1]--; //5
				if(cmp>=0.10) p[1]--; //4
				if(cmp>=1.00) p[1]--; //3
				if(cmp>=10.0) p[2]--; //5
				for(int i=0;i<3;i++) 
				if(fabs(pos[i])<=0.00005) pos[i] = 0;
				char f[32];
				sprintf(f,"Unit: %%g (%%%s,%%%s,%%%s)",
				pos[0]?p:"g",pos[1]?p:"g",pos[2]?p:"g");
				model_status(parent->getModel(),StatusNormal,STATUSTIME_NONE,
				f,m_unitWidth,pos[0],pos[1],pos[2]);
			}
			else
			{
				//This isn't very helpful since the units are fixed and easy
				//to see visually. Zooming is a way to show this same string.
				//model_status(parent->getModel(),StatusNormal,STATUSTIME_NONE,
				//"Unit: %g",m_unitWidth); //getUnitWidth());
			}
		}
	}

	return m_operation!=MO_None;
}
void ModelViewport::mouseMoveEvent(int bs, int x, int y)
{
	//2024: Update these for drawTool.
	parent->getButtonX() = x-m_viewportX;
	parent->getButtonY() = y-m_viewportY;

	int w = m_viewportWidth;
	int h = m_viewportHeight;

	if(!m_activeButton) return; //NEW

	Model *model = parent->getModel();

	if(m_operation==MO_Rotate)
	{
		double xDiff = m_scrollStartPosition[0]-x;
		double yDiff = m_scrollStartPosition[1]-y;

		double rotY = xDiff/w*PI*2;
		double rotX = yDiff/h*PI*2;

		rotateViewport(rotX,-rotY);

		m_scrollStartPosition = {x,y};
	}
	else if(m_operation==MO_Pan)
	{
		//printf("adjusting translation\n");
		
		double xDiff = m_scrollStartPosition[0]-x;
		xDiff*=m_width/w;		
		double yDiff = m_scrollStartPosition[1]-y;
		yDiff*=m_height/h;
	
		m_scroll[0]+=xDiff;
		m_scroll[1]+=yDiff;

		m_scrollStartPosition = {x,y};

		updateMatrix();
	}
	else if(m_operation==MO_Tool)
	{			
		bs|=m_activeButton; //NEW

		//HACK: can this be automated?
		model->setPrimaryLayers(m_layer); //1 only?

		//printf("tool mouse event\n");
		Tool *tool = parent->tool;
		tool->mouseButtonMove(bs,x-m_viewportX,y-m_viewportY);
	}
}
void ModelViewport::mouseReleaseEvent(int bt, int bs, int x, int y)
{
	Model *model = parent->getModel();

	//printf("release = %d\n",e->button());
	//if(e->button()!=m_activeButton) return;
	if(bt!=m_activeButton) return;
	
	if(m_overlayButton==ScrollButtonMAX)
	{
		if(m_operation==MO_Tool)
		{
			//HACK: can this be automated?
			model->setPrimaryLayers(m_layer); //1 only?

			Tool *tool = parent->tool;
			tool->mouseButtonUp(bt|bs,x-m_viewportX,y-m_viewportY);
			model->operationComplete(TRANSLATE("Tool",tool->getName(parent->tool_index)));
		}
	}
	else
	{
		m_overlayButton = ScrollButtonMAX;

		cancelOverlayButtonTimer(); //HACK

		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("LowLevel","Use the middle mouse button to drag/pan the viewport"));
	}
	m_activeButton = 0; //Qt::NoButton;
	m_operation = MO_None;
}

bool ModelViewport::keyPressEvent(int bt, int bs, int x, int y)
{
	//bool ctrl = (e->modifiers()&Qt::ControlModifier)!=0;
	bool ctrl = (bs&Tool::BS_Ctrl)!=0;

	if(!m_activeButton) switch(bt)
	{
	//case Qt::Key_Equal: case Qt::Key_Plus:
	case '=': case '+':
	
		ctrl?rotateClockwise():zoomIn(); break;

	//case Qt::Key_Minus: case Qt::Key_Underscore:
	case '-': case '_':
	
		ctrl?rotateCounterClockwise():zoomOut(); break;
	
	//ODD ASSIGNMENT?
	//case Qt::Key_QuoteLeft: //???
	case '`':
	{
		Tool::ViewE newDir = Tool::ViewPerspective;
		if(m_view<=newDir) newDir = Tool::ViewOrthoDetect;

		//emit viewDirectionChanged(newDir);					
		//emit(this,viewDirectionChanged,newDir);
		viewChangeEvent(newDir);
		break;
	}
	//ODD ASSIGNMENT?
	//case Qt::Key_Backslash:
	case '\\':
	{
		Tool::ViewE newDir = Tool::ViewPerspective;
		switch(m_view)
		{
		case Tool::ViewFront: newDir = Tool::ViewBack; break;
		case Tool::ViewBack: newDir = Tool::ViewFront; break;
		case Tool::ViewRight: newDir = Tool::ViewLeft; break;
		case Tool::ViewLeft: newDir = Tool::ViewRight; break;
		case Tool::ViewTop: newDir = Tool::ViewBottom; break;
		case Tool::ViewBottom: newDir = Tool::ViewTop; break;
		case Tool::ViewPerspective: //newDir = Tool::ViewOrtho; break;
		case Tool::ViewOrtho: //newDir = Tool::ViewPerspective; break;

			//2020: It's more useful to think of this as 
			//inverting the view, especially when applying
			//it to all views at once.
			double x = m_scroll[0], y = m_scroll[1];
			rotateViewport(0,PI,0); //2022
			m_scroll[0] = x; m_scroll[1] = y;
			updateMatrix();
			return false;
		}

		//emit viewDirectionChanged(newDir);					
		//emit(this,viewDirectionChanged,newDir);
		viewChangeEvent(newDir);
		break;
	}

	case Tool::BS_Up: //Qt::Key_Up:
		ctrl?rotateUp():scrollUp();
		break;
	case Tool::BS_Down: //Qt::Key_Down
		ctrl?rotateDown():scrollDown();		
		break;
	case Tool::BS_Left: //Qt::Key_Left
		ctrl?rotateLeft():scrollLeft();
		break;
	case Tool::BS_Right: //Qt::Key_Right
		ctrl?rotateRight():scrollRight();
		break;

	case '0':	
		
		m_scroll[0] = m_scroll[1] = m_scroll[2] = 0;
		updateMatrix();
		break;

	case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':

		if(ctrl)
		{
			//log_debug("set viewport %d\n",bt-'1');

			ViewStateT viewState;
			getViewState(viewState);			

			//emit viewportSaveState(bt-'1',viewState);
			//emit(this,viewportSaveState,bt-'1',viewState);
			parent->viewportSaveStateEvent(bt-'1',viewState);
		}
		else
		{
			//log_debug("viewport recall %d\n",bt-'1');
			
			//emit viewportRecallState(bt-'1');
			//emit(this,viewportRecallState,bt-'1');
			parent->viewportRecallStateEvent(*this,bt-'1');
		}
		break;

	default: return false;
	/*default:
		
		QGLWidget::keyPressEvent(e);
		return;*/
	}

	//e->accept(); //???
	return true;
}
void ModelViewport::getViewState(ModelViewport::ViewStateT &viewState)
{				
	viewState.direction = m_view;
	viewState.layer = m_layer;
	viewState.zoom = m_zoom;
	viewState.rotation[0] = m_rotX;
	viewState.rotation[1] = m_rotY;
	viewState.rotation[2] = m_rotZ;
	viewState.translation[0] = m_scroll[0];
	viewState.translation[1] = m_scroll[1];
	viewState.translation[2] = m_scroll[2];
}

void ModelViewport::rotateViewport(double rotX, double rotY, double rotZ)
{
	if(m_view>Tool::ViewPerspective&&m_view<Tool::ViewOrtho)
	{
		//emit viewDirectionChanged(Tool::ViewOrtho);
		//emit(this,viewDirectionChanged,Tool::ViewOrtho);
		viewChangeEvent(Tool::ViewOrtho);
	}

	double rot[3] = { m_rotX*PIOVER180,m_rotY*PIOVER180,m_rotZ*PIOVER180 };

	//FIX ME: I'm positive this isn't necessary!

		Matrix mcur; 
		mcur.setRotation(rot);
		mcur.inverseRotateVector(m_scroll);

		/*if(0)
		{
			Matrix mcurinv = mcur.getInverse();

			Vector xvec(1,0,0,0);
			Vector yvec(0,1,0,0);
			Vector zvec(0,0,1,0);

			Matrix mx,my,mz; //???

			zvec = zvec*mcurinv;
			mz.setRotationOnAxis(zvec.getVector(),rotZ);
			yvec = yvec*mcurinv;
			my.setRotationOnAxis(yvec.getVector(),rotY);
			xvec = xvec*mcurinv;
			mx.setRotationOnAxis(xvec.getVector(),rotX);
			mcur = mx*mcur;
			mcur = my*mcur;
			mcur = mz*mcur;
		}
		else if(1) //OPTIMIZING
		{
			Matrix m = mcur.getInverse();
			Quaternion q,tmp;
			q.setRotationOnAxis(m.getVector(2),rotZ);
			tmp.setRotationOnAxis(m.getVector(1),rotY);
			q = tmp*q;
			tmp.setRotationOnAxis(m.getVector(0),rotX);
			q = tmp*q;
			m.setRotationQuaternion(q);
			mcur = m*mcur;			
		}
		else*/ //This feels like it's equivalent.*/
		{
			Quaternion q,tmp;
			q.setRotationOnAxis(mcur.getColumn(2).getVector(),rotZ);
			tmp.setRotationOnAxis(mcur.getColumn(1).getVector(),rotY);
			q = tmp*q;
			tmp.setRotationOnAxis(mcur.getColumn(0).getVector(),rotX);
			q = tmp*q;
			Matrix m; m.setRotationQuaternion(q);
			mcur = m*mcur;			
		}

		mcur.getRotation(rot);
		m_rotX = rot[0]/PIOVER180;
		m_rotY = rot[1]/PIOVER180;
		m_rotZ = rot[2]/PIOVER180;

	//m_rotY += xDiff *PIOVER180 *14.0; //???
	//m_rotX += yDiff *PIOVER180 *14.0; //???

	mcur.apply3(m_scroll);

	// And finally,update the view
	updateMatrix();
}

void ModelViewport::setViewState(const ViewStateT &viewState)
{
	m_view = viewState.direction;
	m_layer = viewState.layer;
	m_zoom = viewState.zoom;
	m_rotX = viewState.rotation[0];
	m_rotY = viewState.rotation[1];
	m_rotZ = viewState.rotation[2];
	m_scroll[0] = viewState.translation[0];
	m_scroll[1] = viewState.translation[1];
	m_scroll[2] = viewState.translation[2];

	/*update m_near/farOrtho and m_unitWidth?
	updateMatrix();
	parent->zoomLevelChangedEvent(*this); //NEW*/
	updateViewport('z');

	parent->viewChangeEvent(*this); //NEW
}
void ModelViewport::setLayer(int l)
{
	m_layer = l; parent->viewChangeEvent(*this); 
}

void ModelViewport::viewChangeEvent(Tool::ViewE dir)
{
	//log_debug("viewChangeEvent(%d)\n",dir);

	if(dir==m_view) return;
	
	bool basically_vertical = false;

	//2022: I feel like this isn't the best user experience.
	//However, m_scroll[2] may be important for maintaining
	//frameArea. If it's disabled it's possible to switch a
	//view between back and front (for example) and see the
	//model's other side without it shifting around beneath.
	//m_scroll[1] is set when changing to/fro topdown views.
	Vector scroll(m_scroll); if(1)
	{
		if(m_view<=Tool::ViewPerspective)
		{
			Matrix m;
			m.setRotationInDegrees(0,0,-m_rotZ);
			m.apply3(scroll);
			m.setRotationInDegrees(0,-m_rotY,0);
			m.apply3(scroll);
			m.setRotationInDegrees(-m_rotX,0,0);
			m.apply3(scroll);
		}
		else m_viewInverse.apply3(scroll);

		//log_debug("center point = %f,%f,%f\n",scroll[0],scroll[1],scroll[2]);

		bool toFree = false;
		bool isFree = dir<=Tool::ViewPerspective||dir>=Tool::ViewOrtho;

		switch(m_view)
		{
		case Tool::ViewFront: case Tool::ViewBack:
		case Tool::ViewRight: case Tool::ViewLeft: 
		case Tool::ViewTop: case Tool::ViewBottom: 
		
			toFree = isFree;
		}

		if(toFree) //???
		{
			//log_debug("inverting rotation\n");
			m_rotX = -m_rotX;
			m_rotY = -m_rotY;
			m_rotZ = -m_rotZ;
		}
		else if(!isFree)
		{
			m_rotX = m_rotY = m_rotZ = 0;

			switch(dir)
			{
			case Tool::ViewBack: m_rotY = 180; break;
			case Tool::ViewLeft: m_rotY = -90; break; //90
			case Tool::ViewRight: m_rotY = 90; break; //-90
			case Tool::ViewTop: m_rotX = -90; break;
			case Tool::ViewBottom: m_rotX = 90; break;
			}
		}

		Matrix m; if(isFree&&!toFree)
		{
			m.setRotationInDegrees(m_rotX,m_rotY,m_rotZ);
		}
		else m.setRotationInDegrees(-m_rotX,-m_rotY,-m_rotZ);

		if(toFree) m = m.getInverse();

		m.apply3(scroll);
		//log_debug("after point = %f,%f,%f\n",scroll[0],scroll[1],scroll[2]);

		Vector v; v[2] = 1; m.apply3(v);

		basically_vertical = fabs((float)v[1])>0.9f; //ARBITRARY

		//2022: This enables toggling in and out of "3D"
		//mode without losing track of the original "2D"
		//mode.
		if(dir==Tool::ViewOrthoDetect) //Tool::ViewOrtho
		{
			dir = Tool::ViewOrtho;

			float x = fabs((float)v[0])-0.9999f;
			float y = fabs((float)v[1])-0.9999f;
			float z = fabs((float)v[2])-0.9999f;

			if(x>=0)
			{
				dir = v[0]>0?Tool::ViewLeft:Tool::ViewRight;
			}
			else if(y>=0)
			{
				dir = v[1]>0?Tool::ViewBottom:Tool::ViewTop;
			}
			else if(z>=0)
			{
				dir = v[2]>0?Tool::ViewFront:Tool::ViewBack;
			}
		}
	}	
	//scroll.getVector3(m_scroll);
	m_scroll[2] = scroll[2];
	//Switch to a topdown view is an odd exception to
	//the desire to keep 0,0,0 pinned down since that
	//puts the birds eye view along the view's bottom.
	//if(!toFree&&!isFree)
	{
		Vector v; v[2] = 1; m_viewMatrix.apply3(v);

		bool a = basically_vertical;
	//	bool a = dir==Tool::ViewTop||dir==Tool::ViewBottom;
		bool b = fabs((float)v[1])>0.9f;
	//	bool b = m_view==Tool::ViewTop||m_view==Tool::ViewBottom;

		//TODO: May want to throw in some recenter logic?
		if(a!=b) m_scroll[1] = scroll[1];
	}
	
	m_view = dir;

	if(m_viewportWidth) //2022 //HACK?
	{
		m_unitWidth = getUnitWidth();
			
		updateMatrix();
	}

	parent->viewChangeEvent(*this); //NEW
}

bool ModelViewport::getParentCoords(int bs, int bx, int by, double o[4], bool selected)
{	
	bool ret = false; //2021: Must signal if z is set to deal with "selected" logic.
	o[2] = 0; //2020: Snapping to vertex Z component!
	o[3] = 1; //2023

	Model *model = parent->getModel();
	auto &vu = model->getViewportUnits();

	getRawParentXYValue(bx,by,o[0],o[1]);

	//This needs to be larger for selecting vertices
	//double maxDist = 4.1/m_viewportWidth*m_width;
	double maxDist = 6.1/m_viewportWidth*m_width;

	int snaps = vu.snap, mask = ~0; 
	 
	//EXPERIMENTAL
	//Ctrl+Alt snaps animations and vertices.
	//NOTE: Ctrl is taken by the view rotate
	//function. Other uses of Alt causes the
	//window menu to appear to be focused on.
	if(bs&Tool::BS_Ctrl&&bs&Tool::BS_Alt)
	{
		snaps = snaps?0:~0;
	}

	int lv; if(parent->snap_overlay) //2022
	{
		lv = model->getVisibleLayers();
	}
	else lv = model->getPrimaryLayers();

	//EXPERIMENTAL
	//I've modified polytool.cc and selecttool.cc 
	//to use this back door to get feedback from 
	//the snap system
	bool ss; if(ss=parent->snap_select)
	{
		parent->snap_select = false;
		parent->snap_vertex = -1;
		parent->snap_object.index = -1;
		auto m = parent->snap_object.type;
		if(m<Model::PT_MAX)
		{
			snaps = vu.VertexSnap; 
			
			if(m!=Model::PT_ALL) mask = 1<<m;
		}
	}

	//Perspective needs full getParentXYZW refactor to do better
	//than snapping to the ortho matrix (which still seems to 
	//have troubles even though I don't understand how.)
	if(ss||m_view>Tool::ViewPerspective) 
	if(snaps&vu.VertexSnap)	
	{
		// snap to vertex

		//I'm rewriting this to consider depth the most important
		//criteria. I'm not sure how to formulate a mixed metric?
		//double curDist = maxDist;
		double maxSquared = maxDist*maxDist;
		double curDepth = -DBL_MAX;
		int i,curIndex = -1;
		Model::PositionTypeE curType = Model::PT_MAX;
		//HACK: I'm trying to add a click model to selecttool.cc
		//(I got the idea from polytool.cc)
		//const Matrix &mat = m_viewMatrix;
		//2021: Did I miss this? Snap breaks down in perspective view???
		const Matrix &mat = ss?m_projMatrix:parent->getParentBestMatrix();
		Vector coord;
		double saveCoord[4];

		auto f = [&](Model::PositionTypeE pt)
		{
			coord[3] = 1; mat.apply4(coord);
			//TESTING
			//This lets a projection matrix be used to do the selection.
			//I guess it should be a permanent feature
			double w = coord[3]; if(1!=w) 
			{
				//HACK: Reject if behind Z plane
				if(w<=0) return;

				coord.scale(1/w);
			}

			//double dist = distance(coord[0],coord[1],xval,yval);
			double dist = pow(coord[0]-o[0],2)+pow(coord[1]-o[1],2);

			//NOTE: If all points fall on a plane, just take first
			//in vertex order
			//if(dist<curDist)
			if(dist<maxSquared&&coord[2]>curDepth) 
			{
				//curDist = dist;
				curDepth = coord[2];
				curIndex = i;
				curType = pt;
				for(int i=4;i-->0;) 
				{
					//TODO: return W?
					saveCoord[i] = coord[i]*w;
				}
			}
		};			
		if(mask&1<<Model::PT_Joint)
		{
			i = -1; for(auto*ea:model->getJointList())
			{
				i++; if(selected||!ea->m_selected)
				{
					if(ea->visible(lv))
					{
						coord.setAll(ea->m_absSource);

						f(Model::PT_Joint);
					}
				}
			}
		}
		if(mask&1<<Model::PT_Point)
		{
			i = -1; for(auto*ea:model->getPointList())
			{
				i++; if(selected||!ea->m_selected)
				{
					if(ea->visible(lv))
					{
						coord.setAll(ea->m_absSource);

						f(Model::PT_Point);
					}
				}
			}
		}			
		if(mask&1<<Model::PT_Projection)
		if(model->getDrawProjections()) //2022: visible?
		{
			i = -1;			
			for(auto*ea:model->getProjectionList())
			{
				i++; if(selected||!ea->m_selected)
				{
					//if(ea->visible(lv)) //getDrawProjections?
					{
						coord.setAll(ea->m_absSource);

						f(Model::PT_Projection);
					}
				}
			}
		}
		//There's a lot more vertices to process and they're
		//smaller so users may need to zoom in. The main 
		//reason for this is to be able to click on joints
		//in skeletal animation mode.
		if(curType==Model::PT_MAX)
		if(mask&1<<Model::PT_Vertex)
		{
			i = -1; for(auto*ea:model->getVertexList())
			{
				i++; if(selected||!ea->m_selected)
				{
					if(ea->visible(lv))
					{
						coord.setAll(ea->m_absSource);

						f(Model::PT_Vertex);
					}
				}
			}
			parent->snap_vertex = curIndex;
		}

		if(curIndex>=0)
		{
			o[0] = saveCoord[0]; o[1] = saveCoord[1];
			o[2] = saveCoord[2]; o[3] = saveCoord[3]; 
			
			ret = true; maxDist = 0; //???
		}

		//EXPERIMENTAL
		if(parent->snap_object.type==curType
		 ||parent->snap_object.type==Model::PT_ALL)		
		{
			parent->snap_object.type = curType;
			parent->snap_object.index = curIndex;
		}
	}	

	//if(config->get("ui_snap_grid",false))
	if(snaps&vu.UnitSnap)
	if(maxDist)
	if(m_view<Tool::ViewOrtho)
	if(m_view>Tool::ViewPerspective) //2020
	{
		// snap to grid

		double cmp[2],val[2]; for(int i=2;i-->0;)
		{
			double x = (i?o[1]:o[0])+m_scroll[i];
			double round = x<0?-0.5:0.5;
			int mult = (int)(x/m_unitWidth+round);
			val[i] = mult*m_unitWidth;
			cmp[i] = fabs(x-val[i]);
		}
		if(cmp[0]<maxDist) o[0] = val[0]-m_scroll[0];
		if(cmp[1]<maxDist) o[1] = val[1]-m_scroll[1];
	}

	return ret; //zval was set.
}
bool ModelViewport::applyColor(int v, int bs, int bx, int by, double radius, double press, int vmode, int &iomode)
{
	if(v<0||v>5){ assert(0); return false; }

	double radius2 = 1.0/32*parent->_getParentZoom();
	radius2 = radius2*radius2;

	Model *model = parent->getModel();

	auto &vc = model->getViewportColors()[v];

	int ret = 0, joint = -1;

	double o[2];
	getRawParentXYValue(bx,by,o[0],o[1]);

	//double maxDist = radius/m_viewportWidth*m_width;
	//double maxDist = radius;

	bool l = (bs&Tool::BS_Left)!=0;
	bool r = (bs&Tool::BS_Right)!=0;
	bool shift = (bs&Tool::BS_Shift)!=0;
	
	int lv = model->getPrimaryLayers();

	//I'm rewriting this to consider depth the most important
	//criteria. I'm not sure how to formulate a mixed metric?
	//double curDist = maxDist;
	double maxSquared = radius*radius;
	
	//HACK: I'm trying to add a click model to selecttool.cc
	//(I got the idea from polytool.cc)
	//const Matrix &mat = m_viewMatrix;
	//2021: Did I miss this? Snap breaks down in perspective view???
	const Matrix &mat = 1?m_projMatrix:parent->getParentBestMatrix();
	Vector coord;

	bool ue = model->getUndoEnabled();
	Model::Undo<MU_SwapStableMem> undo;

	double dist;

	auto press2 = [&]()->double
	{
		double p = sqrt(dist);
		p = (0.1+(1-p/radius)*0.9)*press;
		return p; //breakpoint
	};
	auto lerp = [&](float *cc, unsigned c)
	{
		if(ue&&!undo)
		{
			undo = Model::Undo<MU_SwapStableMem>(model);	
			undo->addChange(Model::RedrawAll);
		}

		float newColor[4] = 
		{c&0xff,c>>8&0xff,c>>16&0xff,c>>24&0xff};
		for(int i=4;i-->0;)
		newColor[i]*=1/255.0f;
		for(int i=4;i-->0;) //lerp?
		newColor[i] = (newColor[i]-cc[i])*press2()+cc[i];

		if(undo) undo->addMemory(cc,cc,newColor,sizeof(newColor));
		memcpy(cc,newColor,sizeof(newColor));
	};

	auto miss = [&]()->bool
	{
		coord[3] = 1; mat.apply4(coord);
		//TESTING
		//This lets a projection matrix be used to do the selection.
		//I guess it should be a permanent feature
		double w = coord[3]; if(1!=w) 
		{
			//HACK: Reject if behind Z plane
			if(w<=0) return true;

			coord.scale(1/w);
		}

		//dist = distance(coord[0],coord[1],xval,yval);
		dist = pow(coord[0]-o[0],2)+pow(coord[1]-o[1],2);

		if(dist>=maxSquared) return true;

		ret++; return false;
	};			
	int i;
	if(iomode&1||vmode==3) //mask&1<<Model::PT_Joint
	{
		i = -1; for(auto*ea:model->getJointList())
		{
			i++; //if(selected||!ea->m_selected)
			{
				if(!ea->visible(lv)) continue;
				
				coord.setAll(ea->m_absSource);

				if(miss()) continue;

				if(dist<radius2) //vmode==3
				{
					if(joint==-1) joint =  i;
					else joint = -2;
				}

				if(iomode&1) if(l||r)
				{
					DWORD c = vc.colors[l?0:r?1:0];

					if(shift) c = 0xff000000;

					lerp(ea->m_color,c);
				}
			}
		}
	}
	if(iomode&2) //mask&1<<Model::PT_Point
	{
		i = -1; for(auto*ea:model->getPointList())
		{
			i++; //if(selected||!ea->m_selected)
			{
				if(!ea->visible(lv)) continue;
				
				coord.setAll(ea->m_absSource);

				if(miss()) continue;

				if(l||r)
				{
					DWORD c = vc.colors[l?0:r?1:0];

					if(shift) c = 0xff008000;

					lerp(ea->m_color,c);
				}
			}
		}
	}			

	if(vmode==3) 
	iomode = -1;		
	if(vmode==3&&joint!=-1)
	{
		if(joint==-2)
		model_status(model,StatusError,STATUSTIME_LONG,
		TRANSLATE("Tool","Tried to assign more than 1 joint"));
		else model_status(model,StatusNotice,STATUSTIME_SHORT,
		TRANSLATE("Tool","Trying to assign joint to button..."));

		iomode = joint==-2?-1:joint; 
	}
	else if(vmode) //mask&1<<Model::PT_Vertex
	{
		auto input = l?0:r?1:0; //good enough?
		DWORD color = vc.colors[input];
		DWORD joint = vc.joints[input];

		double cmp[3];
		memcpy(cmp,parent->getParentViewInverseMatrix().getVector(2),sizeof(cmp));

		i = -1; for(auto*ea:model->getVertexList())
		{
			i++; //if(selected||!ea->m_selected)
			{
				if(!ea->visible(lv)) continue;
				
				coord.setAll(ea->m_absSource);

				if(miss()) continue;

				if(vmode==1||vmode==2) //polygon?
				{
					DWORD c = color;

					if(shift) c = 0xffffffff;

					for(auto&f:ea->m_faces)
					{
						Model::Triangle *t = f.first;

						if(vmode==2) //back-side test?
						{							
							if(!t->visible(lv)) continue;
							
							double dp = 0; 
							for(int i=0;i<3;i++) 
							dp+=cmp[i]*t->m_flatSource[i];
							if(dp<0.0) continue;
						}

						lerp(t->m_colors[f.second],c);
					}
				}
				else if(vmode==3&&~0u!=joint) //influence?
				{
					Model::Position p;
					p.type = Model::PT_Vertex;
					p.index = i;

					int found = -1;					
					auto &il = ea->m_influences;
					for(auto&ea:il)
					if(ea.m_boneId==joint)
					{
						found = &ea-il.data();
						if(ea.m_type!=Model::IT_Auto)
						model->setPositionInfluenceType(p,joint,Model::IT_Auto);
						break;
					}
					if(found==-1) //insert it?					
					if(!model->setPositionInfluence(p,joint,Model::IT_Auto,0.0))
					{
						assert(0); break;
					}
					double sum = 0;
					for(auto&ea:il)
					{
						if(ea.m_boneId==joint)						
						model->setPositionInfluenceWeight(p,joint,ea.m_weight+press2());						
						if(ea.m_type!=Model::IT_Remainder)
						sum+=ea.m_weight;
					}
					bool rem = false;
					if(sum>1)
					{
						double x = 1-(sum-1)/sum;
						sum = 0;
						for(auto&ea:il)
						if(ea.m_type!=Model::IT_Remainder)						
						{
							model->setPositionInfluenceWeight(p,ea.m_boneId,ea.m_weight*x);
							sum+=ea.m_weight;
						}
						assert(fabs(sum-1)<0.00001);
					}
				}
			}
		}
	}
	
	return ret!=0;
}

void ModelViewport::frameArea(bool lock, double x1, double y1, double z1, double x2, double y2, double z2)
{
	/*lock just seems to do better with deep models, and the
	//results are unpredictable when "lock" is disabled above
	//when drag-dropping models onto a single perspective view
	if(lock) //2019
	{
		//HACK: This is just avoiding locking if 
		//there is less than two 2D views. 

		lock = false;
		
		for(int i=0;i<parent->viewsN;i++)
		{
			ModelViewport &other = parent->ports[i];

			if(other.m_view<Tool::ViewOrtho
			 &&other.m_view>Tool::ViewPerspective)
			{
				//if(this!=&other)
				if(other.m_view!=m_view)
				{
					//Could do more analysis at this point.
					//Or mutual locking can a user managed 
					//deal.

					//Looks best at start up.
					//NOTE: At time of writing this, every
					//use case uses lock.
					lock = true;
				}
			}
		}
	}*/

	//CAUTION: This is the only way to set Z translation
	//for ortho views. The Rotate Tool sets its pivot on
	//this center position.
	m_scroll[0] = (x1+x2)/2;
	m_scroll[1] = (y1+y2)/2;
	m_scroll[2] = (z1+z2)/2;

	double width = std::max(fabs(x1-x2),0.0001667);
	double height = std::max(fabs(y1-y2),0.0001667);
	double depth = std::max(fabs(z1-z2),0.0001667);

	Vector bounds(width,height,depth); 
	
	bounds.scale(1.2);

	m_viewMatrix.apply3(m_scroll);
	m_viewMatrix.apply3(bounds);

	width = fabs(bounds[0]);
	height = fabs(bounds[1]);

	//NEW: Make zoom consistent for all viewports? Certainly
	//looks better?
	if(lock) width = std::max(width,fabs(bounds[2]));

	m_zoom = std::max(width,height)/2;
	
	/*update m_near/farOrtho and m_unitWidth?
	//QString zoomStr;
	//zoomStr.sprintf("%f",m_zoom);	
	//emit zoomLevelChanged(zoomStr);
	//emit(this,zoomLevelChanged,m_zoom);
	parent->zoomLevelChangedEvent(*this);
	updateMatrix(); //NEW*/
	updateViewport('z');
}

ModelViewport::Parent::Parent()
	:
	background_grid(),
	ports{this,this,this,this,this,this,this}, //C++11
	views1x2(),viewsM(),viewsN(),
	m_scrollTextures(),
	m_entry(),m_focus(),m_click(),colors()
{	
	ports[0].initOverlay(m_scrollTextures);

	//Let derived class call this in case timing is an issue.
	//initializeGL(getModel());
}

void ModelViewport::Parent::initializeGL(Model *m)
{
	//NOTE: ViewPanel calls initializeGL to initialize its OpenGL context.
	//Ideally ViewPanel is a thin UI class. This gets some code out of it. 

	glEnable(GL_SCISSOR_TEST); //NEW

	//TODO: Get from Model settings.
	int backColor[3] = {130,200,200};

	//glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR); //2020
	glLightModeli(0x81F8,0x81FA); //2020

	glShadeModel(GL_SMOOTH);
	glClearColor
	(backColor[0]/255.0f
	,backColor[1]/255.0f
	,backColor[2]/255.0f,1);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);

	{
		//2022: Well this makes no sense?
		//https://github.com/zturtleman/mm3d/issues/173
		//This may break the look of old MM3D files, but it's needed
		//because the old way was assigning default materials light
		//like values and these lights material like values.
		//REMINDER: White lines need to be visible over the polygons.
		/*GLfloat ambient[]  = { 0.8f, 0.8f, 0.8f, 1.0f };
		GLfloat diffuse[]  = { 0.9f, 0.9f, 0.9f, 1.0f };*/
		//This throws off everything unnecessarily, but is a little
		//dicey to set to 0 for debugging purposes.
		//glLightModeli(GL_LIGHT_MODEL_AMBIENT,0); //doesn't work
		float oooo[4] = {};
		//NOTE: I think in TEXTURE mode it might help to use this 
		//to brighten up things (in the draw method), so setting
		//this here may be redundant
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT,oooo);
		GLfloat ambient[]  = { 0.2f, 0.2f, 0.2f, 1.0f }; //(0.8+0.2)*0.2
		GLfloat diffuse[]  = { 0.72f, 0.72f, 0.72f, 1.0f }; //0.9*0.8
		GLfloat position[] = { 0.0f, 0.0f, 1.0f, 0.0f };		


		//glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE); //???
		glLightfv(GL_LIGHT0,GL_AMBIENT,ambient);
		glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);
		glLightfv(GL_LIGHT0,GL_POSITION,position);
		//glEnable(GL_LIGHT0);
	}

	{
		/*Same deal as above?
		//https://github.com/zturtleman/mm3d/issues/173
		GLfloat ambient[]  = { 0.8f, 0.4f, 0.4f, 1.0f };
		GLfloat diffuse[]  = { 0.9f, 0.5f, 0.5f, 1.0f };*/
		GLfloat ambient[]  = { 0.2f, 0.12f, 0.12f, 1.0f }; //(0.4+0.2)*0.2
		GLfloat diffuse[]  = { 0.72f, 0.4f, 0.4f, 1.0f }; //0.5*0.8 
		GLfloat position[] = { 0.0f, 0.0f, 1.0f, 0.0f };

		//glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE); //???
		glLightfv(GL_LIGHT1,GL_AMBIENT,ambient);
		glLightfv(GL_LIGHT1,GL_DIFFUSE,diffuse);
		glLightfv(GL_LIGHT1,GL_POSITION,position);
		//glEnable(GL_LIGHT1);
	}

	{
		GLint texSize = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE,&texSize);
		//log_debug("max texture size is %dx%d\n",texSize,texSize);
	}

	if(m) checkGlErrors(m);

	if(1) //#define MM3D_ENABLEALPHA
	{
		//NOTE: model_draw.cc controls GL_BLEND.

		//https://github.com/zturtleman/mm3d/issues/102
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		if(!glIsEnabled(GL_BLEND))
		{
			log_warning("alpha not supported\n");
			//glDisable(GL_BLEND); //???
			glGetError(); // clear errors
		}
		glDisable(GL_BLEND); //NEW
	}
}
void ModelViewport::Parent::checkGlErrors(Model *m)
{
	const char *e; switch(glGetError())
	{
	case 0: return;
	case GL_INVALID_VALUE:
	e = TRANSLATE_NOOP("LowLevel","OpenGL error = Invalid Value"); break;
	case GL_INVALID_ENUM: 
	e = TRANSLATE_NOOP("LowLevel","OpenGL error = Invalid Enum"); break;
	case GL_INVALID_OPERATION: 
	e = TRANSLATE_NOOP("LowLevel","OpenGL error = Invalid Operation"); break;
	case GL_STACK_OVERFLOW: 
	e = TRANSLATE_NOOP("LowLevel","OpenGL error = Stack Overflow"); break;
	case GL_STACK_UNDERFLOW:
	e = TRANSLATE_NOOP("LowLevel","OpenGL error = Stack Underflow"); break;
	case GL_OUT_OF_MEMORY: 
	e = TRANSLATE_NOOP("LowLevel","OpenGL error = Out Of Memory"); break;
	default: 
	e = TRANSLATE_NOOP("LowLevel","OpenGL error = Unknown"); break;
	}
	model_status(m,StatusNormal,STATUSTIME_NONE,e);
}

void ModelViewport::Parent::getXYZ(double *xx, double *yy, double *zz)
{
	ModelViewport &mvp = ports[m_click]; //m_focus
	Vector vec;
	getRawParentXYValue(vec[0],vec[1]);
	//2020: I'm not positive this is better, but it feels more correct?
	//mvp.m_bestInverse.apply(vec);
	getParentBestInverseMatrix().apply4(vec);
	*xx = vec[0]; *yy = vec[1]; *zz = vec[2];
}

void ModelViewport::Parent::viewportSaveStateEvent(int slot, const ModelViewport::ViewStateT &viewState)
{
	if(slot>=0&&slot<user_statesN) user_states[slot] = viewState;
}
void ModelViewport::Parent::viewportRecallStateEvent(ModelViewport &mvp, int slot)
{
	if(slot>=0&&slot<user_statesN&&user_states[slot])
	{
		int i = &mvp-ports;		
		ports[&mvp-ports].setViewState(user_states[slot]);
	}
}
