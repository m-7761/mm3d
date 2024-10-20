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

#include <typeinfo>
#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h> //param_config_key
#endif

#include "win.h"
#include "viewwin.h"
#include "colorwin.h"
#include "model.h"
#include "log.h"
#include "msg.h"

extern bool viewwin_snap_overlay;

void ViewPanel::setModel()
{
	status.setModel(model);

	resetCurrentTool(true); //NEW

	m_model = model; //REMOVE ME

	if(viewsN&&model)
	{
		//HACK: Use value saved in model?
		auto add = model->getAddLayer();
		if(add>1) for(int i=viewsN;i-->0;)
		{
			views[i]->port.setLayer(add);
		}
		status.overlay.set(model->getOverlayLayers(),viewwin_snap_overlay);

		double x1,y1,z1,x2,y2,z2;
		if(model&&model->getBoundingRegion(&x1,&y1,&z1,&x2,&y2,&z2))
		frameArea(true,x1,y1,z1,x2,y2,z2);
	}
}

//NOTE: The reason to go to this length is to break
//the tools folder's dependency on the config system.
//It doesn't hurt either if the tools code is briefer.
utf8 ViewPanel::param_config_key(utf8 p)
{
	if(!p) return nullptr;

	//OPTIMIZING?
	static const char *name = nullptr;
	static std::string buf = "ui_";
	static size_t pos;

	const char *typeid_name = typeid(*tool).name();

	if(name!=typeid_name)
	{
		name = typeid_name;

		#ifdef __GNUC__
		int _; (void)_;
		typeid_name = abi::__cxa_demangle(typeid_name,0,0,&_);
		#endif
	
		//MSVC puts "class " etc. in front of class names.
		const char *p,*trim = typeid_name;
		while(p=strchr(trim,' ')) trim = p+1;

		buf.replace(3,buf.npos,trim).push_back('_');
		for(char&ea:buf) ea = tolower(ea);
		pos = buf.size();
		
		#ifdef __GNUC__
		std::free((void*)typeid_name);
		#endif
	}
	else buf.erase(pos);

	for(;*p;p++) switch(*p)
	{
	case ' ': case '-': continue;
	default: buf.push_back(tolower(*p));
	}

	return buf.c_str();
}

// Tool::Parent methods
void ViewPanel::addBool(bool cfg, bool *val, utf8 name)
{
	if(cfg) *val = config->get(param_config_key(name),*val);	

	params.new_boolean(val,name)->user = cfg?(void*)name:nullptr;
}
void ViewPanel::addInt(bool cfg, int *val, utf8 name, int min, int max)
{
	if(cfg) *val = config->get(param_config_key(name),*val);	

	params.new_spinbox(val,name,min,max)->user = cfg?(void*)name:nullptr;
}
void ViewPanel::addDouble(bool cfg, double *val, utf8 name, double min, double max)
{
	if(cfg) *val = config->get(param_config_key(name),*val);	

	params.new_spinbox(val,name,min,max)->user = cfg?(void*)name:nullptr;
}
void ViewPanel::addEnum(bool cfg, int *val, utf8 name, const char **items)
{
	if(cfg) *val = config->get(param_config_key(name),*val);	

	params.new_dropdown(val,name,items)->user = cfg?(void*)name:nullptr;
}
void ViewPanel::addButton(int bt, Tool *val, void(*f)(Tool*,int), utf8 name)
{
	params.new_button(bt,val,f,name)->user = nullptr;
}

void ViewPanel::groupParam()
{
	if(auto*c=params.nav.last_child())
	{
		//Checkbox right margin is just 2, so this doesn't buy much.
		//c->sspace(0,-c->space<2>());
		c->sspace(0,-8);
	}
}
void ViewPanel::updateParams(){ params.nav.sync_live(); }
void ViewPanel::removeParams(){ params.clear(); }
void ViewPanel::hideParam(void *cmp, int how)
{
	if(auto*c=params.nav.find_child([cmp](Win::control *ch)
	{
		return ch->live_ptr()==cmp;
	})) c->enable(how==0).set_hidden(how==1);
}

void ViewPanel::command(Command cmd, int arg1, int arg2)
{
	if(cmd==cmd_open_color_window)	
	{
		model.perform_menu_action(id_tool_colormixer);
	}
	else if(cmd==cmd_open_color_window_select_joint)
	{
		model.sync_colormixer_joint(arg1,arg2);
	}
	else assert(0);
}

extern MainWin* &viewwin(int=glutGetWindow());
extern void viewpanel_special_func(int kb, int x, int y)
{
	ViewPanel &vp = viewwin()->views;

	int cm = glutGetModifiers();

	switch(kb)
	{
	case -'s': //Ctrl+S? (viewpanel_keyboard_func)

		if(cm&GLUT_ACTIVE_CTRL)
		return vp.model.perform_menu_action(id_file_save_prompt);
		return;

	case -'z': //Not working?

		if(cm&GLUT_ACTIVE_SHIFT&&cm&GLUT_ACTIVE_CTRL)
		return vp.model.perform_menu_action(id_edit_redo);
		return;

	case -127: //Delete? (viewpanel_keyboard_func) 

		return vp.model.perform_menu_action(127);

	case GLUT_KEY_F1:

		//Make center-open consistent.
		//vp.model.perform_menu_action(id_help);
		extern void aboutwin(int); 
		aboutwin(id_help);		
		return;

	case GLUT_KEY_F2:

		if(vp.tool->isNullTool())
		{
			if(vp.timeline.range())
			vp.timeline.activate();
			else goto beep;
		}
		else if(cm&GLUT_ACTIVE_SHIFT)
		{
			auto *c = vp.params.nav.last_child();
			while(c&&!c->activate()) 
			c = c->next();
			if(!c) goto beep;
		}
		else if(auto*c=vp.params.nav.first_child())
		{
			while(c&&!c->activate())
			c = c->prev(); 
			if(!c) goto beep;
		}
		else beep:Win::event.beep();
		return;

	case GLUT_KEY_F3:
	{
		auto &p = vp.model.sidebar.prop_panel;
		if(!p.nav) p.nav.set().execute_callback(); 
		p.pos.x.activate();
		return;
	}
	case GLUT_KEY_F4:

		glutSetWindow((&vp.bar1)[vp.m_focus>=vp.viewsM].glut_window_id());
		vp.views[vp.m_focus]->view.activate();
		Win::event.spacebar_mouse_click();
		return;

	case GLUT_KEY_F6:

		#if defined(_DEBUG) && defined(WIDGETS_95_BUILD_EXAMPLES)
		Widgets95::xcv_example(6);
		#endif 
		return;

	case GLUT_KEY_F11:

		glutFullScreen(); //UNFINISHED
		break;
	}
	
	//2022: These are geared toward ModelViewport::keyPressEvent.
	bool broadcast = cm&GLUT_ACTIVE_SHIFT;
	//HACK: Some of these have menu items that don't require shift.
	if(kb==-'|') broadcast = true; 
	if(kb==-'~') broadcast = true; 
	//WARNING: A lot of this may be peculiar to certain US keyboards.
	if(broadcast) switch(-kb)
	{
	default: broadcast = false;

	case '+': break;
	case '_': kb = -'-'; break;
	case ')': kb = -'0'; break;

	case -GLUT_KEY_UP:
	case -GLUT_KEY_DOWN:
	case -GLUT_KEY_LEFT:
	case -GLUT_KEY_RIGHT: break;

	case '~': kb = -'`'; break;
	case '|': kb = -'\\'; break;
	}

	if(broadcast)
	{
		cm&=~GLUT_ACTIVE_SHIFT;
		for(int i=vp.viewsN;i-->0;) 
		vp.ports[i].keyPressEventUI(-kb,cm,x,y);
	}
	else vp.ports[vp.m_focus].keyPressEventUI(-kb,cm,x,y);	
}
static void viewpanel_keyboard_func(unsigned char kb, int x, int y)
{
	//_ctrl removes glutKeyboardFunc C^ codes so 13 becomes 's'.
	viewpanel_special_func(-Win::ui::_ctrl(kb),x,y);
}
static void viewpanel_entry_func(int e)
{
	ViewPanel &vp = viewwin()->views;
	vp.m_entry = e;
	vp.m_focus = vp.portsN_1-1; //dummy?

	if(vp.tool->isColorTool())
	{
		vp.updateAllViews(); //TODO? can see if same kind or not
	}
}
static void viewpanel_mouse_func(int bt, int st, int x, int y)
{
	int cm = glutGetModifiers();
	ViewPanel &vp = viewwin()->views;

	//Broadcast MO_Pan/Rotate?
	if(cm&GLUT_ACTIVE_SHIFT)
	if(bt==GLUT_MIDDLE_BUTTON
	||cm&GLUT_ACTIVE_CTRL||vp.tool->isNullTool())
	{
		cm|=GLUT_ACTIVE_CTRL;
		for(int i=vp.viewsN;i-->0;)
		if(st==GLUT_DOWN)
		vp.ports[i].mousePressEventUI(bt,cm,x,y);
		else 
		vp.ports[i].mouseReleaseEventUI(bt,cm,x,y);
		return;
	}
	
	if(st!=GLUT_DOWN)
	{
		vp.ports[vp.m_focus].mouseReleaseEventUI(bt,cm,x,y);
		return;
	}

	int xx,yy; 
	vp.getXY(xx=x,yy=y);
	for(int i=vp.viewsN;i-->0;) if(vp.ports[i].over(xx,yy))
	{
		if(i!=vp.m_focus) if(vp.tool->isColorTool())
		{
			vp.updateAllViews(); //TODO? can see if same kind or not

			glutSetCursor(GLUT_CURSOR_NONE);
		}
		else glutSetCursor(GLUT_CURSOR_LEFT_ARROW);

		if(vp.tool->isColorTool())
		{
			//YUCK: update for ColorTool cursor
			vp._bx = xx-vp.ports[i].m_viewportX;
			vp._by = yy-vp.ports[i].m_viewportY;
		}

		vp.m_entry = 1; //2024
		vp.m_focus = i; 
		vp.m_click = i; //2022
		if(st==GLUT_DOWN) vp.ports[i].mousePressEventUI(bt,cm,x,y);
	}
}
static void viewpanel_motion_func(int x, int y)
{	
	ViewPanel &vp = viewwin()->views;

	//Steal tab function/communicate input model.
	Win::event.deactivate();

	int cm = glutGetModifiers();
	int bt = vp.ports[vp.m_focus].m_activeButton;
	if(!bt)
	{
		//NOTE: MM3D had updated the status bar with the mouse's
		//coordinates, but that seems excessive.

		int xx,yy; 
		vp.getXY(xx=x,yy=y);
		for(int i=vp.viewsN;i-->0;) 
		if(vp.ports[i].over(xx,yy))
		{
			if(i!=vp.m_focus) if(vp.tool->isColorTool())
			{
				vp.updateAllViews(); //TODO? can see if same kind or not

				glutSetCursor(GLUT_CURSOR_NONE);
			}
			else glutSetCursor(GLUT_CURSOR_LEFT_ARROW);

			vp.m_entry = 1; //2024
			vp.m_focus = i; 

			if(vp.tool->isColorTool())
			{
				//YUCK: update for ColorTool cursor
				vp._bx = xx-vp.ports[i].m_viewportX;
				vp._by = yy-vp.ports[i].m_viewportY;

				vp.m_click = i; vp.updateView();
			}
		}

		//2024: Update x/y for drawTool?
		vp.ports[vp.m_focus].mouseMoveEventUI(cm,x,y);
	}
	else
	{
		//EXPERIMENTAL: Multi-panel?
		if(cm&GLUT_ACTIVE_SHIFT)
		if(bt==Tool::BS_Middle
		||cm&GLUT_ACTIVE_CTRL||vp.tool->isNullTool())
		{
			cm|=GLUT_ACTIVE_CTRL;
			for(int i=vp.viewsN;i-->0;) 
			vp.ports[i].mouseMoveEventUI(cm,x,y);
			return;
		}
		
		vp.ports[vp.m_click].mouseMoveEventUI(cm,x,y);	
	}
}
static void viewpanel_wheel_func(int wheel, int delta, int x, int y)
{
	if(wheel!=0) return;

	int cm = glutGetModifiers();

	ViewPanel &vp = viewwin()->views;

	//NOTE: Ctrl+Alt doesn't break mouse capture
	if(cm&GLUT_ACTIVE_ALT&&vp.tool->isColorTool())
	if(cm&GLUT_ACTIVE_CTRL) 
	{
		vp.updateView(); return vp.tool->sizeNib(delta);
	}

	for(int i=vp.viewsN;i-->0;) if(vp.ports[i].wheelEventUI(delta,cm,x,y))
	{
		vp.m_focus = i; return;
	}
}

void ViewPanel::back() //EXPERIMENTAL
{
	bool persp = ports[m_focus].getView()<=Tool::ViewPerspective;

	background_grid[persp] = !background_grid[persp];

	updateAllViews();
}

void ViewPanel::draw()
{
	//Sync with drawing.
	if(model._deferredModelChanged)
	model._drawingModelChanged();
	glutSetWindow(model.glut_window_id);

	//2022: model is nullptr in ViewPanel::ViewPanel
	if(0==ports[0].m_viewportWidth)
	{
		assert(m_model);
		Parent::initializeGL(m_model);

		//HACK: This is just about the only place I
		//could find to insert initialization logic.
		//The purpose here is to be able to use Tab.
		Win::event.deactivate();
	}

	//REMINDER: I spent like a day fussing with this
	//to layout the outlines between the views, only
	//to learn that the views need to be exactly the
	//same size to match projections up. Drawing the
	//outlines on top is actually the most stable for
	//resizing smoothly, and different sizes may not
	//be possible to match up precisely so the grids
	//line up.

	const int m = viewsM;
	const int n = viewsN;
	const int ww = shape[0];
	const int hh = shape[1];
	glScissor(0,0,ww,hh);
	glViewport(0,0,ww,hh);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	int h = m==n?hh:hh/2;
	int w = ww/m;

	//ASSUMING KEEPING BUTTONS
	//Unnecessary but helps to space
	//the buttons consistently.
	int x = ww-w*m;
	if(x>1) //3x2?
	{
		//This leaves a 2px gap on the right
		//view button. 4x1 would leave a 3px.
		x = 1;
	}
	int y = hh/2;
	int l = m!=n&&y*2!=hh;
	if(l) y++; 

	//2024: Prevent undo objects for frame?
	bool ue = model->setUndoEnabled(false);

	double swap; int swap2;

	if(playing1) //EXPERIMENTAL
	{
		swap = model->getCurrentAnimationFrameTime();
		swap2 = model->getChangeBits();

		if(!model->setCurrentAnimationTime(playing2,~playing1,Model::AT_invalidateAnim))
		{
			model.perform_menu_action(id_animate_stop);	

			assert(!playing1);
		}	
	}

	auto *cm = model._colormixer_win;
	int mode1 = model->getCanvasDrawMode();
	int mode2 = model->getPerspectiveDrawMode();
	for(int i=0;i<m;i++,x+=w)		
	for(int pass=1;pass<=2;pass++)
	{
		if(pass==1&&m==n) continue;

		int j = (pass==2?viewsN:viewsM)-1-i;
		
		int mod = 0, k = colors[j]; if(cm)
		{
			bool view = (&cm->v1)[k].tool;
			bool pair = (&cm->v1)[k].pair&&(&cm->v1)[k].pair.enabled();
			if(pair)
			{
				assert(!view);
				k = colors[(&cm->v1)[k].pair.name()[0]-'1'];
			}
			if(view||pair) goto permaview;

			if(tool->isColorTool()
			 ||tool->isNullTool()&&prev_tool->isColorTool())
			{			
				if(j==m_click||j==m_focus) permaview:
				{
					int vc = model->getViewportColorsMode(); 
					if(vc<3) mod = ModelViewport::ViewVColors;
					if(vc==3) mod = ModelViewport::ViewInflColors;
					model->setCanvasDrawMode(mode1|mod);
					model->setPerspectiveDrawMode(mode2|mod);
				}
			}
		}
		ports[j].draw(x,pass==1?y:l,w,h,k);
		model->setCanvasDrawMode(mode1);
		model->setPerspectiveDrawMode(mode2);
	}

	if(playing1)
	{
		swap2^=model->getChangeBits();
		model->unsetChangeBits(swap2);
		model->setCurrentAnimationFrameTime(swap,Model::AT_invalidateAnim);
	}

	model->setUndoEnabled(true);

	Widgets95::window::set_ortho_projection(ww,hh);	
	glScissor(0,0,ww,hh);
	glViewport(0,0,ww,hh);
	//130,200,200 //~1.3f
	//glColor3ub(100,150,150); 
	{
		float r = backColor[0];
		float g = backColor[1];
		float b = backColor[2];
		float f = 
		r>0.3||b>0.3||g>0.3?1/1.3f:1.5f;
		r*=f;
		g*=f;
		b*=f;
		glColor3f(r,g,b);
	}
	for(int i=0;i<n;i++)
	{
		ports[i].getGeometry(x,y,w,h);
	
		//ASSUMING KEEPING BUTTONS
		//Unnecessary but helps to space
		//the buttons consistently.
		y = hh-(y+h);
		if(x>=1){ x--; w++; }
		if(i%m) w--;

		if(x+w>ww-2) w = ww-x-1;
		if(y+h>hh-2) h = hh-y-1;

		glBegin(GL_LINE_LOOP);
		glVertex2i(x,y); glVertex2i(x+w,y);
		glVertex2i(x+w,y+h); glVertex2i(x,y+h);
		glEnd();
	}

	glutSwapBuffers();
}

ViewPanel::ViewPanel(MainWin &model)
:
m_model(), //REMOVE ME //???
model(model),shape(),
bar1(model),bar2(model),
params(bar1),timeline(bar1.exterior_row),playing1(),
status(bar2)
{
	timeline.id(id_bar).expand();
	timeline.style(Win::bar::shadow);
	timeline.spin(0.0).set_range();	
	extern int viewwin_tick(Win::si*,int,double&,int);
	timeline.set_tick_callback(viewwin_tick);

	//TODO: Maybe make glut::set_ obsolete?
	assert(glutGetWindow()==model.glut_window_id);
	auto display_func = [](){ viewwin()->views.draw(); };
	Widgets95::glut::set_glutDisplayFunc(display_func);
	Widgets95::glut::set_glutKeyboardFunc(viewpanel_keyboard_func);
	Widgets95::glut::set_glutSpecialFunc(viewpanel_special_func);
	Widgets95::glut::set_glutMouseFunc(viewpanel_mouse_func);
	Widgets95::glut::set_glutMotionFunc(viewpanel_motion_func);
	Widgets95::glut::set_glutPassiveMotionFunc(viewpanel_motion_func);
	Widgets95::glut::set_glutEntryFunc(viewpanel_entry_func);
	//EXPERIMENTAL
	glutext::glutMouseWheelFunc(viewpanel_wheel_func);

	//Parent::Parent doesn't do this in case timing is an issue.
	//model is nullptr here.
	//Parent::initializeGL(model);

	_makeViews(0);
}
ViewPanel::~ViewPanel()
{
	//Note: ~Parent does UI-independent destructor logic.

	assert(!m_model);

	_deleteViews(); //MEMORY LEAK
}
void ViewPanel::_deleteViews()
{
	for(int i=viewsN;i-->0;) delete views[i];
}
void ViewPanel::_makeViews(int n)
{  	
	_deleteViews(); 
	
	if(!n)
	{
		n = config->get("ui_viewport_count",4);
		if(n<1||n>/*9*/6) n = 4;
	}
	else config->set("ui_viewport_count",n);
		
	int mem = viewsN;
	if(n==2&&!views1x2) mem++;
	viewsN = n; 	
	views1x2 = n!=2?false:!config->get("ui_viewport_tall",true); 

	switch(n)
	{
	case 1: viewsM = 1; break;
	default:
	case 2: viewsM = views1x2?1:2; break; //m_tall
	case 4: viewsM = 2; break;
	case 6: viewsM = 3; break; //m_tall?2:3
	}
		
	//These are inserted in reverse order.
	ViewBar::ModelView* &ref = views[viewsM-1];
	ref = nullptr;
	for(int i=viewsM;i-->0;)	
	views[i] = new ViewBar::ModelView(bar1,ports[i],ref);
	for(int i=viewsN;i-->viewsM;)
	views[i] = new ViewBar::ModelView(bar2,ports[i],ref);
	for(int i=viewsN;i<portsN_1;i++)
	views[i] = nullptr;

	//Not required, but eliminates some padding.
	bar2.portside_row.set_hidden(viewsM==viewsN);

	//REMINDER: _defaultViews uses m_focus but also it needs to be
	//clipped to be less than viewsN. Assuming it will be assigned
	//almost instantly.
	_defaultViews(mem,true); m_focus = m_click = 0;

	//double x1,y1,z1,x2,y2,z2;
	//if(model&&model->getBoundingRegion(&x1,&y1,&z1,&x2,&y2,&z2))
	//frameArea(true,x1,y1,z1,x2,y2,z2);

	if(shape[0]) 
	{
		//Resize/reposition subwindows.
		glutSetWindow(model.glut_window_id);
		Widgets95::e::reshape();

		//Update viewports/locks.
		model.reshape();
	}

	if(model._colormixer_win)
	{
		model._colormixer_win->layoutChanged();
	}
}
void ViewPanel::_save(int a, int b)
{
	for(int i=0;a<=b;a++,i++)
	{
		ports[i].getViewState(memory[a]);

		//THIS CAN BE HELPFUL, BUT CAN ANNOY TOO.
		for(int i=0;i<memoryN;i++) 
		if(memory[i].direction==memory[a].direction)
		{
			if(i!=a&&memory[i]) 
			memory[i].copy_transformation(memory[a]);
		}
	}
}
bool ViewPanel::_recall(int a, int b)
{
	if(memory[a]) for(int i=0;a<=b;a++,i++)
	{
		ports[i].setViewState(memory[a]);
	}
	else return false; return true;
}
void ViewPanel::_defaultViews(int mem, bool save)
{
	int a,b,c; switch(save?mem:0) //reset?
	{
	default: a = -1; break;
	case 1: _save(a=0,b=0); break;
	case 2: _save(a=1,b=2); break;
	case 3: _save(a=3,b=4); break;
	case 4: _save(a=5,b=8); break; //2x2
	case 6: _save(a=9,b=14); break; //3x2
	}

	bool rc; switch(viewsN)
	{
	case 1: 

		//HACK: Grab the currently focused view when
		//switching to single view, as a convenience.
		if(a!=-1&&memory[a+m_focus])
		{
			auto &m = memory[a+m_focus];
			ports[0].setViewState(m);
			//c = 0; break;
			return; //2022
		}

		if(rc=_recall(c=0,0))
		break;
		views[0]->setView(Tool::ViewPerspective);
		colors[0] = 0;
		break;

	case 2:

		if(rc=views1x2?_recall(c=1,2):_recall(c=3,4))
		break;

		//NOTE: This layout will be more convenient if persp
		//mode is editable. ATM it feels better. And Tab can
		//be used to swap.
		views[0]->setView(Tool::ViewPerspective);		
		views[1]->setView(Tool::ViewOrtho);
		colors[0] = 0;
		colors[1] = 1;
		break;

	case 4:

		if(rc=_recall(c=5,8))
		break;

		//2019: Changing to Blender defaults, expecting users to 
		//be mostly familiar with Blender-> 
		//NOTE: This layout is more intuitive if ortho views are
		//synchronized where moving one moves others accordingly->
		views[0]->setView(Tool::ViewPerspective); //ViewLeft
		views[1]->setView(Tool::ViewTop); //ViewFront		
		views[2]->setView(Tool::ViewRight); //ViewPerspective
		views[3]->setView(Tool::ViewFront); //ViewTop
		colors[0] = 0;
		colors[1] = 1;
		colors[2] = 2;
		colors[3] = 3;
		break;

	case 6:

		if(rc=_recall(c=9,14))
		break;

		/*2019: Can't be implemented as multipane window->
		if(views1x2) //m_tall
		{
			views[0]->setView(ViewFront);
			views[1]->setView(ViewBack);
			views[2]->setView(ViewLeft);
			views[3]->setView(ViewRight);
			views[4]->setView(ViewTop);
			views[5]->setView(ViewPerspective);
		}
		else*/
		{
			//Retaining 2x2 layout, but with each getting a perspective view->
			views[0]->setView(Tool::ViewPerspective); //ViewFront
			views[1]->setView(Tool::ViewTop); //ViewBack
			views[2]->setView(Tool::ViewPerspective); //ViewTop			
			views[3]->setView(Tool::ViewRight); //ViewPerspective
			views[4]->setView(Tool::ViewFront); //ViewRight
			views[5]->setView(Tool::ViewPerspective); //ViewLeft
		}
		colors[0] = 0;
		colors[1] = 1;
		colors[2] = 4;
		colors[3] = 2;
		colors[4] = 3;
		colors[5] = 5;
		break;

	/*2019: Can't be implemented as multipane window->
	default: //Default???
		views[0]->setView(ViewFront);
		views[1]->setView(ViewBack);
		views[4]->setView(ViewRight);
		views[2]->setView(ViewTop);
		views[5]->setView(ViewBottom);
		views[3]->setView(ViewLeft);
		views[6]->setView(ViewOrtho);
		views[7]->setView(ViewOrtho);
		views[8]->setView(ViewPerspective);
		break;*/
	}

	//Keep transformation of the named views.
	//THIS CAN BE HELPFUL, BUT CAN ANNOY TOO.
	double x1,y1,z1,x2,y2,z2;
	bool framed = false;
	for(int i=0;i<viewsN;i++)
	{
		bool frame = true;
		int src = -1;
		if(a>=0) for(int j=a;j<=b;j++)	
		if(ports[i].getView()==memory[j].direction)
		{
			if(!memory[j]) continue;
			//if(memory[j].direction!=ViewOrtho)
			//continue;		
			double z = ports[i].getZoomLevel();
			ports[i].setViewState(memory[j]);
			frame = !memory[c+i];
			if(!frame)
			{
				ports[i].setZoomLevel(z);
				//views[i]->zoom.value.set_float_val(z); //???			
			}
			break;
		}	
		if(rc||!frame) continue;

		if(!framed&&model)
		framed = model->getBoundingRegion(&x1,&y1,&z1,&x2,&y2,&z2);
		if(framed) 
		ports[i].frameArea(true,x1,y1,z1,x2,y2,z2);
	}
}

//As implemented drawing individual viewports requires
//either render-target support/logic or a partial swap
//extension (GLX_MESA_copy_sub_buffer) so for now it's
//just simpler to draw them all.
void ViewPanel::updateAllViews()
{
	//for(int i=0;i<viewsN;i++) ports[i].updateGL();
	if(shape[0]) glutPostWindowRedisplay(model.glut_window_id);
}
void ViewPanel::updateView()
{
	//ports[m_focus].updateGL(); //This isn't possible.
	updateAllViews();	
}

void ViewPanel::viewChangeEvent(ModelViewport &mvp)
{
	auto &v = views[&mvp-ports];

	if(mvp.getView()!=(int)v->view||!v->view.selection())
	{
		v->view.select_id(mvp.getView());
	}

	if(mvp.getLayer()!=(int)v->layer) //2022
	{
		v->layer.select_id(mvp.getLayer());
		v->layer.mouse_over(); //HACK
	}

	//TODO: zoomLevelChangedEvent should be
	//merged into viewChangeEvent...
}
void ViewPanel::zoomLevelChangedEvent(ModelViewport &mvp)
{
	auto &v = views[&mvp-ports];

	if(mvp.getZoomLevel()!=(double)v->zoom.value)
	{
		v->zoom.value.set_float_val(mvp.getZoomLevel());
	}
}

void ViewPanel::rearrange(int how)
{	
	bool flip = how==2; assert(how==1||flip);

	//HACK: A bug sets the zoom to 32.0 (default) when
	//there's just 1 view ... or rather there others 
	//haven't been initialized.
	if(viewsN==1||viewsN==2&&how==(views1x2?1:2))
	{
		return Win::event.beep();
	}

	ModelViewport::ViewStateT swap[portsN];
	for(int i=0,j=viewsM;i<viewsM;i++,j++)
	{
		ports[flip?viewsM+i:viewsM-1-i].getViewState(swap[i]);
		ports[flip?j-viewsM:viewsN-1-i].getViewState(swap[j]);
	}
	for(int i=0;i<viewsN;i++) ports[i].setViewState(swap[i]); 
}

void ViewPanel::reset()
{
	memset(memory,0x00,sizeof(memory)); _defaultViews(viewsN,false); 
}

void ViewPanel::setBackgroundColor(float press)
{
	int bs = getButtons();

	float newColor[3]; if(bs&Tool::BS_Shift)
	{
		newColor[0] = 130/255.0f;
		newColor[1] = 200/255.0f;
		newColor[2] = 200/255.0f;
	}
	else
	{
		auto &vc = model->getViewportColors()[colors[m_click]];

		unsigned c = bs&Tool::BS_Left?0:bs&Tool::BS_Right?1:0;

		c = vc.colors[c];

		newColor[0] = (c&0xff)/255.0f;
		newColor[1] = (c>>8&0xff)/255.0f;
		newColor[2] = (c>>16&0xff)/255.0f;
	}

	for(int i=3;i-->0;) //lerp?
	backColor[i] = (newColor[i]-backColor[i])*press+backColor[i];
	
	int old = glutGetWindow();
	glutSetWindow(model.glut_window_id);
	glClearColor(backColor[0],backColor[1],backColor[2],1);
	glutSetWindow(old);
}

