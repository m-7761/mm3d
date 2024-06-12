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

#include "mm3dport.h" //PATH_MAX

#include "pluginmgr.h"
#include "viewwin.h"
#include "viewpanel.h"
#include "model.h"
#include "tool.h"
#include "toolbox.h"
#include "cmdmgr.h"
#include "log.h"

#include "msg.h"
#include "modelstatus.h"
#include "filtermgr.h"
#include "misc.h"
#include "animwin.h"
#include "misc.h"
#include "version.h"
#include "texmgr.h"
#include "sysconf.h"
#include "projectionwin.h"
#include "transformwin.h"
#include "texturecoord.h"

#include "luascript.h"
#include "luaif.h"

extern void viewwin_menubarfunc(int);
static void viewwin_mrumenufunc(int);
static void viewwin_geomenufunc(int);
extern void viewwin_toolboxfunc(int);

static int //REMOVE US?
viewwin_mruf_menu=0,viewwin_mrus_menu=0,
viewwin_file_menu=0,viewwin_view_menu=0,
viewwin_tool_menu=0,viewwin_modl_menu=0,
viewwin_geom_menu=0,viewwin_edit_menu=0,
viewwin_mats_menu=0,viewwin_infl_menu=0,
viewwin_help_menu=0,viewwin_face_menu=0,
viewwin_deletecmd=0,viewwin_interlock=1,
viewwin_jointlock=0,
viewwin_toolbar = 0;
int //extern
viewwin_joints100=1,
viewwin_shifthold=0,viewwin_shlk_menu=0;

bool viewwin_snap_overlay = false; //extern

//TODO: Need an elaborate API for this I guess
static bool &viewwin_rmb = ModelViewport::_rmb_rotates_view;
static bool &viewwin_face_view = Tool::tool_option_face_view;
static bool &viewwin_face_ccw = Model::Vertex::getOrderOfSelectionCCW();

std::vector<MainWin*> viewwin_list(0); //extern

static utf8 viewwin_title = "Untitled"; //"Maverick Model 3D"

extern MainWin* &viewwin(int id=glutGetWindow())
{
	MainWin **o = nullptr;
	for(auto&ea:viewwin_list) if(ea->glut_window_id==id)
	o = &ea; //return ea;
	if(!o) //HACK: Assume id is a subwindow?
	{
		int w = glutGetWindow();
		glutSetWindow(id);
		int c = glutGet(glutext::GLUT_WINDOW_CREATOR);
		glutSetWindow(w);
		if(c>0) return viewwin(c);
	}
	if(!o) //assert(o); //ui_drop2?
	{
		assert(!viewwin_list.empty());

		o = &viewwin_list[0]; 
	}
	return *o;
}

static int viewwin_status_active = 0;
extern void viewwin_status_func(int st=0)
{
	if(viewwin_list.empty()) return; //Quit?

	MainWin *w = viewwin();

	if(st==glutext::GLUT_ACTIVE)
	{
		viewwin_status_active = w->glut_window_id;
	}	
	else if(st) return; //Inactive?

	if(viewwin_status_active!=w->glut_window_id)
	{
		return; //Mouse wheel event?
	}

	glutSetMenu(viewwin_edit_menu);
	
	int ins = w->clipboard_mode;
	if(ins&&!w->model->inAnimationMode())
	{
		ins = false;
	}
	void *l = ins?0:glutext::GLUT_MENU_ENABLE;
	glutext::glutMenuEnable(0,l); //Copy Animation Data
	glutext::glutMenuEnable(1,l); //Copy
	glutext::glutMenuEnable(2,l); //Paste
	glutext::glutMenuEnable(viewwin_deletecmd,l); //Delete
}

void MainWin::modelChanged(int changeBits) // Model::Observer method
{
	if(_window_title_asterisk==model->getSaved())
	_rewrite_window_title();
	
	//REMOVE ME
	//This is to replace DecalManager functionality
	//until it can be demonstrated Model::m_changeBits
	//is set by every change.
	//https://github.com/zturtleman/mm3d/issues/90
	if(!changeBits)
	{	
		if(_texturecoord_win) //What's this for again?
		{
			//NOTE: Texture coords aren't animated ATM.
			//model->validateAnim();

	//		_texturecoord_win->modelChanged(0); //???
		}

		//_deferredModelChanged makes this unnecessary.
		//return;
	}

	toolbox.getCurrentTool()->modelChanged(changeBits);
		
	/*REFERENCE
	//TODO: May want to implement this in Model somehow.
	//This is to keep the Properties panel from updating
	//mid playback. I'm not sure how or if Maverick Model
	//3D avoids that.
	if(playing&&!views.playing1) changeBits&=~Model::AnimationFrame;*/

	//REMINDER: Can't defer these because of m_ignoreChange pattern
	if(_projection_win) _projection_win->modelChanged(changeBits);
	if(_texturecoord_win) _texturecoord_win->modelChanged(changeBits);
	//if(_transform_win) = _transform_win->modelChanged(changeBits);

	if(changeBits&Model::SelectionChange)
	{
		model->getSelectedPositions(selection);
		auto &sn = nselection;
		memset(sn,0x00,sizeof(sn));
		for(auto&i:selection) sn[i.type]++;
		//Assuming getSelectedPositions puts vertices on back!
		assert(!sn[Model::PT_Vertex]||!selection.back().type);
	}
	if(changeBits&Model::SelectionFaces)
	{
		model->getSelectedTriangles(fselection);
	}
	if(changeBits&Model::ShowJoints)
	{
		glutSetMenu(_rops_menu);
		int id = model->getDrawJoints();
		glutext::glutMenuEnable(id_rops_hide_joints,
		id?glutext::GLUT_MENU_UNCHECK:glutext::GLUT_MENU_CHECK);	
		if(!id&&model->unselectAllBoneJoints())
		model->operationComplete(::tr("Hide bone joints"));
	}
	if(changeBits&Model::ShowProjections)
	{
		glutSetMenu(_rops_menu);
		int id = model->getDrawProjections();
		glutext::glutMenuEnable(id_rops_hide_projections,
		id?glutext::GLUT_MENU_UNCHECK:glutext::GLUT_MENU_CHECK);
		if(!id&&model->unselectAllProjections())
		model->operationComplete(::tr("Hide projections"));
	}
	if(changeBits&Model::AnimationMode)
	{
		glutSetMenu(_anim_menu);		
		int id = model->inAnimationMode();
		glutext::glutMenuEnable(id_animate_mode,
		id?glutext::GLUT_MENU_CHECK:glutext::GLUT_MENU_UNCHECK);
	}

	if(changeBits&(Model::AnimationChange
	|Model::SelectionJoints|Model::SelectionPoints)) //REMOVE ME?
	{
		if(_animation_win) _animation_win->modelChanged(changeBits); //2021
	}
	
	//Don't want to do this faster than the viewports are displayed.
	//Plus the animation coordinates should be validated to be safe.
	_deferredModelChanged|=changeBits;

	//Do on redraw so animation data isn't calculated unnecessarily.
	views.modelUpdatedEvent();
}
void MainWin::_drawingModelChanged()
{
	int changeBits = _deferredModelChanged;
	_deferredModelChanged = 0;
	model->validateAnim();

	//ViewPanel::draw calls _drawingModelChanged.
	//There's no sense in calling updateAllViews.
	//views.modelUpdatedEvent(); 
	{
		views.status.setStats(); //This is somewhat optimized.

		//updateAllViews(); //Overkill?
	}
	sidebar.modelChanged(changeBits);

	/*Can't defer these because of m_ignoreChange pattern???
	if(_projection_win) _projection_win->modelChanged(changeBits);
	if(_texturecoord_win) _texturecoord_win->modelChanged(changeBits);
	//if(_transform_win) = _transform_win->modelChanged(changeBits);
	*/
}

static void viewwin_mru(int id, char *add=nullptr)
{
	glutSetMenu(id); 
	
	id = id==viewwin_mruf_menu?0:100;

	utf8 cfg = "script_mru"+(id==100?0:7);

	std::string &mru = config->get(cfg);

	int lines = std::count(mru.begin(),mru.end(),'\n');

	if(!mru.empty()&&mru.back()!='\n') 
	{
		lines++; mru.push_back('\n');
	}

	if(add)
	{
		assert(*add); //Untitled?

		size_t n,len = strlen(add);

		//Seems to somehow trigger heap corruption
		//check for std::string
		//add[len] = '\n';
		char buf[PATH_MAX];
		if(len>sizeof(buf)-1) return; //FIX ME

		memcpy(buf,add,len);
		#ifdef _WIN32
		for(char*p=buf+len;p-->buf;)
		{
			if(*p=='\\') *p = '/';
		}
		#endif
		buf[len] = '\n';
		if(n=mru.find(buf,0,len+1))
		{	
			if(n!=mru.npos) mru.erase(n,len+1);
			else lines++;
			mru.insert(mru.begin(),buf,buf+len+1);
		}		

		if(!n) return; //ILLUSTRATING
	}
	for(;lines>10;lines--)
	{
		size_t s = mru.rfind('\n',mru.size()-2);
		if(s!=mru.npos) //FIX ME (hand edited??)
		mru.erase(s+1);
	}

	size_t r,s = 0;
	int i,iN = glutGet(GLUT_MENU_NUM_ITEMS);
	for(i=0;i<lines;i++,mru[s++]='\n',id++)
	{
		s = mru.find('\n',r=s); 
		if(s==mru.npos) break; //FIX ME (hand edited??)

		mru[s] = '\0';
		if(i>=iN) glutAddMenuEntry(mru.c_str()+r,id);
		else glutChangeToMenuEntry(1+i,mru.c_str()+r,id);
	}
	while(iN>i) glutRemoveMenuItem(iN--);

	if(add) config->set(cfg,mru);
}
extern void viewwin_mru_drop(char *add)
{
	viewwin_mru(viewwin_mruf_menu,add);
}

static bool viewwin_close_func_quit = false;
static void viewwin_close_func()
{		
	MainWin *w = viewwin();
	if(!viewwin_close_func_quit)
	if(!w->save_work_prompt()) return;
		
	//REMOVE ME
	//viewpanel_display_func is sometimes entered on closing
	//after removal from viewwin_list.
	//glutHideWindow();
	Widgets95::glut::set_glutDisplayFunc();
	//viewpanel_motion_func, etc?
	Widgets95::glut::set_glutKeyboardFunc();
	Widgets95::glut::set_glutSpecialFunc();
	Widgets95::glut::set_glutMouseFunc();
	Widgets95::glut::set_glutMotionFunc();
	Widgets95::glut::set_glutPassiveMotionFunc();	
	/*REMINDER: glutDetachMenu causes reshape with wrong
	//size saved to the config file. This just keeps the
	//wrong size from ever being recorded.
	Widgets95::glut::set_glutReshapeFunc(nullptr);
	//Not automatic currently. The menus can't be deleted
	//without first detaching.
	glutDetachMenu(glutext::GLUT_NON_BUTTON);*/

	//viewwin_list.remove(w); //C++
	std::swap(viewwin(w->glut_window_id),viewwin_list.back());
	viewwin_list.pop_back();

	//close_ui_by_create_id should cover these as well.
	//if(w->_animation_win) w->_animation_win->close();
	//if(w->_transform_win) w->_transform_win->close();
	//if(w->_projection_win) w->_projection_win->close();
	//if(w->_texturecoord_win) w->_texturecoord_win->close();
	Widgets95::e::close_ui_by_create_id(w->glut_window_id); //F1
	Widgets95::e::close_ui_by_parent_id(w->glut_window_id);

	//HACK: Wait for close_ui_by_parent_id to finish up in
	//the idle stage.
	glutext::glutModalLoop();
	
	//~MainWin can't do this since Model expects a 
	//GLX context and GLX expects onscreen windows.
	//REMINDER: ~UtilWin needs to call model->removeObserver.
	delete w->_swap_models(nullptr);
	delete w;
}
bool MainWin::quit()
{
	bool doSave = false;
	
	#ifndef CODE_DEBUG	
	for(auto ea:viewwin_list)
	if(!ea->model->getSaved())
	{
		 char response = msg_warning_prompt
		 (::tr("Some models are unsaved.  Save before exiting?"));
		 if(response=='C') return false; 
		 if(response=='Y') doSave = true;
	}
	#endif // CODE_DEBUG

	for(auto ea:viewwin_list)
	if(doSave&&!ea->model->getSaved())
	{
		if(!ea->save_work()) return false;
	}	

	/*Crashes wxWidgets.
	//NOTE: close won't take immediate effect.
	for(auto ea:viewwin_list)
	Widgets95::e::close_ui_by_parent_id(ea->glut_window_id);
	viewwin_list.clear();*/
	viewwin_close_func_quit = true;
	while(!viewwin_list.empty())
	{
		glutSetWindow(viewwin_list.back()->glut_window_id);
		viewwin_close_func();
	}

	return true;
}

static utf8 viewwin_key_sequence(utf8 pf, utf8 name, utf8 def="")
{
	char buf[64*2]; 	
	int i,j,iN = snprintf(buf,sizeof(buf),"%s_%s",pf,name);	
	for(i=0,j=0;i<iN;i++) switch(buf[i])
	{
	case '.': break; 
	case '\n': //RotateTextureCommand
	case ' ': buf[i] = '_'; 		
	default: buf[j++] = tolower(buf[i]);
	}
	buf[j] = '\0'; 
	//return keycfg->get(buf,*def?TRANSLATE("KeyConfig",def,name):def);
	return keycfg->get(buf,def);
}
static void viewwin_toolbar_title(std::string &s, Tool *tool, int i)
{
	//NOTE: This is really a title; not a tooltip.
	utf8 name = tool->getName(i);
	utf8 ks = viewwin_key_sequence("tool",name,tool->getKeymap(i));
	s.append(TRANSLATE("Tool",name));
	if(*ks) s.append(" (").append(ks).push_back(')');
//	if(*ks) s.append("\t").append(ks);
}
static void viewwin_synthetic_hotkey(std::string &s, Tool *tool, int i)
{
	utf8 ks = viewwin_key_sequence("tool",tool->getName(i),tool->getKeymap(i));
	if(*ks) s.append(1,'\t').append(ks);
}
static void viewwin_synthetic_hotkey(std::string &s, Command *cmd, int i)
{
	utf8 ks = viewwin_key_sequence("cmd",cmd->getName(i),cmd->getKeymap(i));
	if(*ks) s.append(1,'\t').append(ks);
}
extern utf8 viewwin_menu_entry(std::string &s, utf8 key, utf8 n, utf8 t="", utf8 def="", bool clr=true)
{
	//utf8 ks = keycfg->get(key,*def?TRANSLATE("KeyConfig",def,t):def);
	utf8 ks = keycfg->get(key,def);
	if(clr) s.clear(); s+=::tr(n,t);
	if(*ks) s.append(1,'\t').append(ks); return s.c_str();
}
extern utf8 viewwin_menu_radio(std::string &o, bool O, utf8 key, utf8 n, utf8 t="", utf8 def="")
{
	o = O?'O':'o'; o.push_back('|'); return viewwin_menu_entry(o,key,n,t,def,false);
}
extern utf8 viewwin_menu_check(std::string &o, bool X, utf8 key, utf8 n, utf8 t="", utf8 def="")
{
	o = X?'X':'x'; o.push_back('|'); return viewwin_menu_entry(o,key,n,t,def,false);
}
void MainWin::_init_menu_toolbar() //2019
{
	std::string o; //radio	
	#define E(id,...) viewwin_menu_entry(o,#id,__VA_ARGS__),id_##id
	#define O(on,id,...) viewwin_menu_radio(o,on,#id,__VA_ARGS__),id_##id
	#define X(on,id,...) viewwin_menu_check(o,on,#id,__VA_ARGS__),id_##id

	if(!viewwin_mruf_menu) //static menu(s)
	{
		//UNFINISHED
		/* Most Recently Used */		
		viewwin_mruf_menu = glutCreateMenu(viewwin_mrumenufunc);
		viewwin_mru(viewwin_mruf_menu);

		#ifdef HAVE_LUALIB //UNUSED			
		viewwin_mrus_menu = glutCreateMenu(viewwin_mrumenufunc);		
		viewwin_mru(viewwin_mrus_menu);
		#endif

		if(0) //EXPERIMENTAL
		{
			//normal_order mode isn't quite workable, but this at
			//least teaches that the view direction is significant.
			//I don't know if it should be kept or not.

			viewwin_face_menu = glutCreateMenu(viewwin_menubarfunc);

			bool r = config->get("ui_face_view",true);
			bool s = config->get("ui_prefs_face_ccw",true);
			viewwin_face_view = r;
			views.status._face_view.name(s?"Ccw":"Cw");
			views.status._face_view.indicate(!r);
			glutAddMenuEntry(O(r,normal_click,"View Direction (Mouse Click)","","Windows_menu+Enter")); 
			glutAddMenuEntry(O(!r,normal_order,"Use Select Order or a Heuristic","",""));
			glutAddMenuEntry(X(s,normal_ccw,"Use OpenGL Select Order (CCW)","",""));
		}

		viewwin_file_menu = glutCreateMenu(viewwin_menubarfunc);

		//wxWidets/src/common/accelcmn.cpp has a list of obscure
		//(ugly) accelerator strings that I wish Widgets95 could
		//workaround, but in the meantime there's no other means.
	glutAddMenuEntry(E(file_new,"New...","File|New","Alt+Up"));
	glutAddMenuEntry(E(file_open,"Open...","File|Open","Alt+Down"));		
	glutAddSubMenu(::tr("Recent Models","File|Recent Models"),viewwin_mruf_menu);
	glutAddMenuEntry(E(file_close,"Close","File|Close","Alt+End"));
	glutAddMenuEntry();
	glutAddMenuEntry(E(file_save,"Save (Ctrl+S)","File|Save","Shift+Alt+S"));
	glutAddMenuEntry(E(file_save_as,"Save As...","File|Save As","Alt+F12"));
	glutAddMenuEntry(E(file_export,"Export...","File|Export","Alt+F11"));
	glutAddMenuEntry(E(file_export_selection,"Export Selected...","File|Export Selected","Shift+F11"));
	glutAddMenuEntry();		
	#ifdef HAVE_LUALIB //UNUSED
	glutAddMenuEntry(E(file_run_script,"Run Script...","File|Run Script"));
	glutAddSubMenu(::tr("Recent Scripts","File|Recent Script"),viewwin_mrus_menu);
	#endif
	glutAddMenuEntry(E(file_plugins,"Plugins...","File|Plugins"));
	//Will wxWidgets detection ignore the ellipsis?
	glutAddMenuEntry(E(file_prefs,"Preferences...","")); //wxOSX requires this.	
	glutext::glutMenuEnable(id_file_prefs,0); //UNIMPLEMENTED	
	if(viewwin_face_menu) //EXPERIMENTAL
	glutAddSubMenu(::tr("New Face Normal",""),viewwin_face_menu);
	//I'm making this true so it appears as a checkbox on Windows.
	viewwin_rmb = config->get("ui_prefs_rmb",false);
	glutAddMenuEntry(X(!viewwin_rmb,file_prefs_rmb,"LMB Rotates View","",""));
	glutAddMenuEntry();
	//Unlike Scroll_lock removing the underscore doesn't work.
	//Capitalization does however.
	glutAddMenuEntry(E(file_resume,"Resume","","Windows_Menu")); //Windows_menu
	glutAddMenuEntry();
	//REMINDER: Alt+F4 closes a window according to the system menu.
	glutAddMenuEntry(E(file_quit,"Quit","File|Quit","Shift+Alt+Q"));
	}	

	if(!viewwin_view_menu) //static menu (NEW)
	{
		int layers = glutCreateMenu(viewwin_menubarfunc);
		glutAddMenuEntry(E(view_layer_0,"Hiding","","Shift+Esc"));
		glutAddMenuEntry();
		glutAddMenuEntry(E(view_layer_1,"Layer 1","","Shift+F1"));
		glutAddMenuEntry(E(view_layer_2,"Layer 2","","Shift+F2"));
		glutAddMenuEntry(E(view_layer_3,"Layer 3","","Shift+F3"));
		glutAddMenuEntry(E(view_layer_4,"Layer 4","","Shift+F4"));
		glutAddMenuEntry();
		glutAddMenuEntry(E(view_layer_lower,"Lower","","Alt+Left"));
		glutAddMenuEntry(E(view_layer_lower_all,"Lower All","","Shift+Alt+Left"));
		glutAddMenuEntry(E(view_layer_raise,"Raise","","Alt+Right"));
		glutAddMenuEntry(E(view_layer_raise_all,"Raise All","","Shift+Alt+Right"));

		int overlays = glutCreateMenu(viewwin_menubarfunc);
		glutAddMenuEntry(E(view_overlay,"Viewing","","Ctrl+O"));		
		glutAddMenuEntry();
		glutAddMenuEntry(E(view_overlay_1,"Overlay 1","","Ctrl+F1"));
		glutAddMenuEntry(E(view_overlay_2,"Overlay 2","","Ctrl+F2"));
		glutAddMenuEntry(E(view_overlay_3,"Overlay 3","","Ctrl+F3"));
		glutAddMenuEntry(E(view_overlay_4,"Overlay 4","","Ctrl+F4"));
		glutAddMenuEntry();
		viewwin_snap_overlay = config->get("ui_snap_overlay",true);
		glutAddMenuEntry(X(viewwin_snap_overlay,view_overlay_snap,"Snap to Overlay","","Shift+O"));		
		
		//_view_menu is pretty long compared to the others. The
		//purpose of this submenu is to collect the static menu
		//items.
		viewwin_view_menu = glutCreateMenu(viewwin_menubarfunc);

		//views.status._interlock.indicate(true); //inverting sense
		//glutAddMenuEntry(E(frame_all,"Frame All","View|Frame","Home"));
		glutAddMenuEntry(X(true,frame_lock,"Interlock","","Shift+Ctrl+E"));
		glutAddMenuEntry(E(frame_all,"Enhance","View|Frame","Shift+E"));
		glutAddMenuEntry(E(frame_selection,"Enhance Selection","View|Frame","E"));		
		glutAddMenuEntry();
		glutAddSubMenu("Switch Layer",layers);
		//glutAddMenuEntry(E(view_overlay,"Toggle Layer","","Ctrl+O"));
		glutAddSubMenu("Toggle Overlay",overlays);
		glutAddMenuEntry();
		glutAddMenuEntry(E(fullscreen,"Full Screen Mode","","F11"));
		glutAddMenuEntry(E(viewselect,"Open View Menu","","F4"));
		glutAddMenuEntry();
		glutAddMenuEntry(E(view_persp,"Perspective","","PgUp")); 
		glutAddMenuEntry(E(view_ortho,"Orthographic","","Shift+PgUp"));			
		glutAddMenuEntry(E(view_right,"Right","","PgDn"));
		glutAddMenuEntry(E(view_left,"Left","","Shift+PgDn"));
		glutAddMenuEntry(E(view_top,"Top","","Home"));
		glutAddMenuEntry(E(view_bottom,"Bottom","","Shift+Home"));	
		glutAddMenuEntry(E(view_front,"Front","","End"));
		glutAddMenuEntry(E(view_back,"Back","","Shift+End"));
		glutAddMenuEntry();
		glutAddMenuEntry(E(view_invert,"Invert View","","\\"));
		glutAddMenuEntry(E(view_invert_all,"Invert All Views","","Shift+\\"));
		glutAddMenuEntry(E(view_project_all,"Project All Views","","Shift+`"));
		//view_persp does this now	
		//glutAddMenuEntry(E(view_project,"Project View","","`")); 
		glutAddMenuEntry(E(view_init,"Restore to Normal","","`")); //Shift+Esc
		glutAddMenuEntry();
		glutAddMenuEntry(E(view_swap,"Change Sides","View|Viewports","Ctrl+Tab"));
		glutAddMenuEntry(E(view_flip,"Bottom on Top","View|Viewports","Ctrl+Q"));	
	}
		bool r,s,t,u,v;
				
		//* SUB MENU */ //View->Render Options		
		_rops_menu = glutCreateMenu(viewwin_menubarfunc);	
		{	
			r = config->get("ui_render_bones",true);
			glutAddMenuEntry(X(r,rops_draw_bones,"Draw Joint Bones","View|Draw Joint Bones"));
			glutAddMenuEntry();
			//r = !config->get("ui_render_joints",true);
			glutAddMenuEntry(X(false,rops_hide_joints,"Hide Bone Joints","View|Hide Joints","Shift+B"));			
			glutAddMenuEntry(X(false,rops_hide_projections,"Hide Texture Projections","View|Hide Texture Projections","Shift+P"));
			r = !config->get("ui_render_3d_selections",false);
			glutAddMenuEntry(X(r,rops_hide_lines,"Hide Lines from 3D Views","View|Hide 3D Lines","Shift+W"));		
			r = !config->get("ui_render_backfaces",false);
			glutAddMenuEntry(X(r,rops_hide_backs,"Hide Back-facing Triangles","View|Hide Back-facing Triangles","Shift+F")); 
			glutAddMenuEntry();		
			r = config->get("ui_render_bad_textures",true);			
			glutAddMenuEntry(X(r,rops_draw_badtex,"Use Red X Error Texture","View|Use Blank Error Texture"));			
			r = !config->get("ui_no_overlay_option",false);
			glutAddMenuEntry(X(r,rops_draw_buttons,"Use Corner Button Sets","",""));
		}

		_view_menu = glutCreateMenu(viewwin_menubarfunc);	
	
	glutAddSubMenu("Reconfigure",viewwin_view_menu); //NEW
	glutAddMenuEntry();
	//2019: Snap To had been a Tool menu item.
	//It seems to fit better in the View menu.
	//Plus, it has to be unique to the window.
	/* SUB MENU */ //Tools->Snap To		
	//_snap_menu = glutCreateMenu(viewwin_menubarfunc);	
	{	
		r = config->get("ui_snap_vertex",false);
		s = config->get("ui_snap_grid",false);
		//glutAddMenuEntry(X(r,snap_vert,"Vertex","View|Snap to Vertex","Shift+V"));
		glutAddMenuEntry(X(r,snap_vert,"Snap to Vertex","View|Snap to Vertex","Shift+V"));
		//glutAddMenuEntry(X(s,snap_grid,"Grid","View|Snap to Grid","Shift+G"));
		glutAddMenuEntry(X(s,snap_grid,"Snap to Grid","View|Snap to Grid","Shift+G"));
		glutAddMenuEntry(E(view_settings,"Grid Settings...","View|Grid Settings","Shift+Ctrl+G"));

		views.status._vert_snap.indicate(r);
		views.status._grid_snap.indicate(s);
	}		
	//glutAddSubMenu(::tr("Snap To"),_snap_menu);
	glutAddMenuEntry();	
	glutAddSubMenu(::tr("Render Options","View|Render Options"),_rops_menu);	
	glutAddMenuEntry();
		int conf = config->get("ui_ortho_drawing",0);
		if(conf<0||conf>4) conf = 0;
	glutAddMenuEntry(O(conf==0,ortho_wireframe,"2D Wireframe","View|Canvas","W"));		
	//2021: 2 for 2D, 4 for 2D+2D UV Map (mnemonic system)
	glutAddMenuEntry(O(conf==1,ortho_flat,"2D Flat","View|Canvas","Alt+2"));
	glutAddMenuEntry(O(conf==2,ortho_smooth,"2D Smooth","View|Canvas","Shift+Alt+2"));
	glutAddMenuEntry(O(conf==3,ortho_texture,"2D Texture","View|Canvas","Alt+4"));
	glutAddMenuEntry(O(conf==4,ortho_blend,"2D Alpha Blend","View|Canvas","Shift+Alt+4"));
	glutAddMenuEntry();
		conf = config->get("ui_persp_drawing",3);
		if(conf<0||conf>4) conf = 3;
	//2021: 3 for 3D, 5 for 3D+2D UV Map (mnemonic system)
	glutAddMenuEntry(O(conf==0,persp_wireframe,"3D Wireframe","View|3D","Alt+1"));
	glutAddMenuEntry(O(conf==1,persp_flat,"3D Flat","View|3D","Alt+3"));
	glutAddMenuEntry(O(conf==2,persp_smooth,"3D Smooth","View|3D","Shift+Alt+3"));
	glutAddMenuEntry(O(conf==3,persp_texture,"3D Texture","View|3D","Alt+5"));
	glutAddMenuEntry(O(conf==4,persp_blend,"3D Alpha Blend","View|3D","Shift+Alt+5"));
	glutAddMenuEntry();		
		r = s = t = u = v = false;
		switch(config->get("ui_viewport_count",0))
		{
		case 1: r = true; break;
		case 2: (config->get("ui_viewport_tall",0)?t:s) = true; break;
		case 4: default: u = true; break;
		case 6: v = true;
		}			
		if(r) _curr_view = id_view_1;
		if(s) _curr_view = id_view_1x2;
		if(t) _curr_view = id_view_2x1;
		if(u) _curr_view = id_view_2x2;
		if(v) _curr_view = id_view_3x2;
		_prev_view = u?id_view_1:id_view_2x2;
	glutAddMenuEntry(O(r,view_1,"1 View","View|Viewports","Shift+1"));
	glutAddMenuEntry(O(0,view_2,"2 Views","View|Viewports","Shift+2"));
	glutAddMenuEntry(O(s,view_1x2,"2 Views (Wide)","View|Viewports","Shift+3"));
	glutAddMenuEntry(O(t,view_2x1,"2 Views (Tall)","View|Viewports","Shift+4"));
	glutAddMenuEntry(O(u,view_2x2,"2x2 Views","View|Viewports","Q"));	
	glutAddMenuEntry(O(v,view_3x2,"3x2 Views","View|Viewports","Shift+Q"));		
			
	//REMOVE ME
	//NOTE: The selected tool holds a state, but the 
	//menu itself is not expected to change.
	toolbox.registerAllTools(); if(!viewwin_toolbar)
	{
		viewwin_toolbar = glutCreateMenu(viewwin_toolboxfunc);
	
			//FINISH ME
			//https://github.com/zturtleman/mm3d/issues/57

		int sm,id;

		//None (Esc) is first because Esc is top-left on keyboard.
		toolbox.setCurrentTool();
		Tool *tool = toolbox.getCurrentTool();
		int pixmap = glutext::glutCreateImageList((char**)tool->getPixmap(0));
		viewwin_toolbar_title(o="o|",tool,0);
		glutAddMenuEntry(o.c_str(),id_tool_none);
		glutext::glutMenuEnableImage(id_tool_none,pixmap);
		for(int pass=1;pass<=2;pass++)
		{
			int creators = id = 0;
			tool = toolbox.getFirstTool();
			for(;tool;tool=toolbox.getNextTool())	
			if(!tool->isSeparator())
			{
				int iN = tool->getToolCount();

				if(tool->isCreateTool()) creators++;

				//This is designed to put select-tools in the middle of 
				//the toolbar because their shortcuts are in the middle
				//of the keyboard.
				if((pass==1)==(creators<=3&&!tool->isSelectTool()))
				for(int i=0;i<iN;i++)
				{
					viewwin_toolbar_title(o="o|",tool,i);
					glutAddMenuEntry(o.c_str(),id+i);
					glutext::glutUpdateImageList(pixmap,(char**)tool->getPixmap(i));
					glutext::glutMenuEnableImage(id+i,pixmap);
				}

				id+=iN;
			}
		}
		glutext::glutDestroyImageList(pixmap);
	
		viewwin_tool_menu = glutCreateMenu(viewwin_toolboxfunc);

		sm = id = 0; 

		bool sep = false;
		utf8 path = nullptr;

		int select_menu = 0;
		tool = toolbox.getFirstTool();
		for(;tool;tool=toolbox.getNextTool())	
		if(!tool->isSeparator())
		{
			utf8 p = tool->getPath(); 
			if(p!=path&&sm)
			{
				glutSetMenu(viewwin_tool_menu);
				glutAddSubMenu(::tr(path,"Tool"),sm);
			}
			if(sep)
			{
				sep = false; glutAddMenuEntry();				
			}
			if(p!=path)
			{
				path = p; 
				sm = p?glutCreateMenu(viewwin_toolboxfunc):0;

				if(!select_menu) select_menu = sm;
			}

			int iN = tool->getToolCount(); 
			for(int i=0;i<iN;i++)
			{
				o = TRANSLATE("Tool",tool->getName(i));
				viewwin_synthetic_hotkey(o,tool,i);
				glutAddMenuEntry(o.c_str(),id+i);
			}
			id+=iN;
		}
		else sep = true;
		if(sm) glutSetMenu(viewwin_tool_menu);
		if(sm) glutAddSubMenu(::tr(path,"Tool"),sm);

		glutAddMenuEntry(E(tool_back,"Show Grids in Back","","Back"));
		glutAddMenuEntry(E(toolparams,"Tooling Parameters","","F2"));
		glutAddMenuEntry();
		viewwin_shlk_menu = glutCreateMenu(viewwin_menubarfunc);
		r = config->get("ui_tool_shift_hold",false);
		if(r) views.status._shifthold.indicate(true);
		glutAddMenuEntry(E(tool_shift_lock,"Sticky Shift Key (Emulation)","","L"));
		glutAddMenuEntry(X(!r,tool_shift_hold,"Unlock when Selecting Tool","","Shift+L"));
		glutSetMenu(viewwin_tool_menu);
		glutAddSubMenu(::tr("Shift Lock",""),viewwin_shlk_menu);
		glutAddMenuEntry();
		glutAddMenuEntry(E(tool_none,"None","","Esc"));		
		glutAddMenuEntry(E(tool_toggle,"Toggle Tool","","Tab"));
		glutAddMenuEntry(E(tool_recall,"Switch Tool","","Space"));
		glutSetMenu(select_menu);
		glutAddMenuEntry();
		glutAddMenuEntry(E(properties,"Selection Properties","","F3"));
	}

	if(!viewwin_modl_menu) //static menu(s)
	{
		viewwin_modl_menu = glutCreateMenu(viewwin_menubarfunc);	

	//Note, Shift+Ctrl+Z is hardwired as Redo since although
	//Ctrl+Y is standard these days, Shift+Ctrl+Z has a long
	//history is an is more efficient.
	glutAddMenuEntry(E(edit_undo,"Undo","Undo shortcut","Ctrl+Z"));
	glutAddMenuEntry(E(edit_redo,"Redo","Redo shortcut","Ctrl+Y"));
	glutAddMenuEntry();
	//SLOT(transformWindowEvent()),g_keyConfig.getKey("viewwin_model_transform"));
	glutAddMenuEntry(E(transform,"Transform Model...","Model|Transform Model","Ctrl+T"));
	glutAddMenuEntry(E(edit_utildata,"Edit Model Utilities...","","U"));
	//glutAddMenuEntry(E(edit_metadata,"Edit Model Meta Data...","Model|Edit Model Meta Data","Shift+Ctrl+T"));
	glutAddMenuEntry(E(edit_userdata,"Edit Model User Data...","","Ctrl+U"));
	glutAddMenuEntry(E(background_settings,"Set Background Image...","Model|Set Background Image","Alt+Back"));
	//SEEMS UNNECESSARY
	//glutAddMenuEntry(::tr("Boolean Operation...","Model|Boolean Operation"),id_modl_boolop);
	glutAddMenuEntry();
	glutAddMenuEntry(E(merge_models,"Merge...","Model|Merge","Ctrl+Insert"));
	//glutAddMenuEntry(E(merge_animations,"Import Animations...","Model|Import Animations","Ctrl+Alt+A"));
	glutAddMenuEntry(E(merge_animations,"Merge Animations...","","Alt+Insert"));
	//NOTE: "Clarify" just happens to be the same width as "Merge" on Windows.
	//The window is named "Clean UP" and is the same as the animsetwin.cc one.
	glutAddMenuEntry(E(clean_animations,"Clarify Animations...","","Alt+Delete")); //2022

		viewwin_geom_menu = glutCreateMenu(viewwin_geomenufunc);

			//FINISH ME
			//https://github.com/zturtleman/mm3d/issues/57

		bool sep = false;
		int sm = 0, id = 0;
		utf8 path = nullptr;
		auto cmgr = CommandManager::getInstance();
		for(Command*cmd=cmgr->getFirstCommand();cmd;cmd=cmgr->getNextCommand())
		if(!cmd->isSeparator())
		{
			utf8 p = cmd->getPath(); 
			if(p!=path)
			{					
				if(!viewwin_edit_menu) viewwin_edit_menu = sm;

				if(sm) glutSetMenu(viewwin_geom_menu);
				if(sm) glutAddSubMenu(::tr(path,"Command"),sm);
			}
			if(sep)
			{
				sep = false; glutAddMenuEntry();				
			}
			if(p!=path)
			{
				path = p; 
				sm = p?glutCreateMenu(viewwin_geomenufunc):0;
			}
			
			int iN = cmd->getCommandCount(); 
			for(int i=0;i<iN;i++)
			{
				utf8 n = cmd->getName(i);
				
				//REMOVE ME
				//wxWidgets only allows one shortcut per command.
				if(!memcmp(n,"Dele",4)) viewwin_deletecmd = id+i; 

				o = TRANSLATE("Command",n);
				viewwin_synthetic_hotkey(o,cmd,i);
				glutAddMenuEntry(o.c_str(),id+i);
			}
			id+=iN;
		}
		else sep = true;

		if(sm) glutSetMenu(viewwin_geom_menu);
		if(sm) glutAddSubMenu(::tr(path,"Command"),sm);

		viewwin_mats_menu = glutCreateMenu(viewwin_menubarfunc);	

	glutAddMenuEntry(E(group_settings,"Edit Groups...","Materials|Edit Groups","Ctrl+G"));	
	glutAddMenuEntry(E(material_settings,"Edit Materials...","Materials|Edit Materials","Ctrl+M"));
	glutAddMenuEntry(E(material_cleanup,"Clean Up...","Materials|Clean Up","Ctrl+Delete"));
	glutAddMenuEntry();
	glutAddMenuEntry(E(uv_editor,"Edit Texture Coordinates...","Materials|Edit Texture Coordinates","M")); //Ctrl+E
	glutAddMenuEntry(E(projection_settings,"Edit Projections...","Materials|Edit Projection","Ctrl+P"));	
	glutAddMenuEntry();
	glutAddMenuEntry(E(refresh_textures,"Reload Textures","Materials|Reload Textures","Ctrl+R"));
	glutAddMenuEntry(E(uv_render,"Plot Texture Coordinates...","Materials|Paint Texture","Shift+Ctrl+P"));
				
		viewwin_infl_menu = glutCreateMenu(viewwin_menubarfunc);	

	glutAddMenuEntry(E(joint_settings,"Edit Joints...","Joints|Edit Joints","J")); 
		r = config->get("ui_joint_100",true);
	glutAddMenuEntry(X(r,joint_100,"Assign 100","","I"));
		views.status._100.indicate(!r); //inverting sense
		
	glutAddMenuEntry(E(joint_attach_verts,"Assign Selected to Joint","Joints|Assign Selected to Joint","Ctrl+B"));
	glutAddMenuEntry(E(joint_weight_verts,"Auto-Assign Selected...","Joints|Auto-Assign Selected","Shift+Ctrl+B"));
	glutAddMenuEntry();	

		int sel = glutCreateMenu(viewwin_menubarfunc);
	glutAddMenuEntry(E(joint_select_bones_of,"Select Joint Influences","Joints|Select Joint Influences","Shift+J")); 
	glutAddMenuEntry();
	glutAddMenuEntry(E(joint_select_verts_of,"Select Influenced Vertices","Joints|Select Influenced Vertices","Shift+Alt+V")); 	
	glutAddMenuEntry(E(joint_select_points_of,"Select Influenced Points","Joints|Select Influenced Points","Shift+Alt+O")); 
	glutAddMenuEntry();
	glutAddMenuEntry(E(joint_unnassigned_verts,"Select Unassigned Vertices","Joints|Select Unassigned Vertices","Shift+Ctrl+F")); 
	glutAddMenuEntry(E(joint_unnassigned_points,"Select Unassigned Points","Joints|Select Unassigned Points","Shift+Alt+F")); 
		glutSetMenu(viewwin_infl_menu);
		glutAddSubMenu(::tr("Select","Tool"),sel);

	glutAddMenuEntry();	
	glutAddMenuEntry(E(joint_simplify,"Convert Multi-Influenced to Single","Joints|Convert Multiple Influences to Single","Shift+I"));		
	glutAddMenuEntry(E(joint_remove_bones,"Removed All Influences from Selected","Joints|Remove All Influences from Selected","Shift+Alt+I")); 
	glutAddMenuEntry(E(joint_remove_selection,"Removed Selected Joint from Influenced","Joints|Remove Selected Joint from Influencing","Shift+Alt+J")); 
	glutAddMenuEntry();	
	glutAddMenuEntry(E(joint_draw_bone,"Apply Alternative Appearance to Bones","","Shift+Alt+B")); 
	//IMPLEMENT ME
	//REMINDER: animation mode works differently (should they be standardized?)
	//glutAddMenuEntry(E(joint_lock_bone,"Articulate Bone Independent of Parent","","Shift+I")); 
	}		

	_anim_menu2 = glutCreateMenu(viewwin_menubarfunc);
	{
		r = config->get("ui_snap_keyframe",true);
		views.status._keys_snap.indicate(!r); //inverting sense
		//Fix me: Need to use wxTRANSLATE somehow to translate Scroll_lock?
		//https://forums.wxwidgets.org/viewtopic.php?f=1&t=46722
		//wxMSW (accelcmn.cpp)
		//2022: "Scroll_Lock" and "Scroll Lock" seem to work now.
		//"Windows Menu" doesn't, so it must not be a generic fix.
		//I guess keeping the underscore is best for consistency
		//and representing a single logical key. ScrLk won't work.
		glutAddMenuEntry(X(r,animate_snap,"Snap to Keyframe","","Scroll_Lock")); //Scroll_lock

		glutAddMenuEntry();
		//Ctrl+Scroll_lock is system interpreted as Break like Ctrl+Pause.
		glutAddMenuEntry(O(false,animate_mode_2,"Frame Animation Mode","","Alt+Scroll_Lock"));
		glutAddMenuEntry(O(false,animate_mode_1,"Skeletal Animation Mode","","Shift+Scroll_Lock"));
		glutAddMenuEntry(O(true,animate_mode_3,"Unlocked (Complex Mode)","","Shift+Alt+Scroll_Lock"));
		glutAddMenuEntry();
		glutAddMenuEntry(X(true,animate_bind,"Bind Influences to Skeleton","","Ctrl+Alt+Scroll_Lock"));
	}
		_anim_menu = glutCreateMenu(viewwin_menubarfunc);	

	glutAddSubMenu(::tr("Scroll Lock"),_anim_menu2);	
	glutAddMenuEntry();
		r = config->get("ui_clipboard_mode",false);
		const_cast<int&>(clipboard_mode) = r;
		views.status._clipboard.indicate(r);
	glutAddMenuEntry(E(animate_insert,"Insert Key Frame","Insert")); //2024
	glutAddMenuEntry(X(r,clipboard_mode,"Clipboard Mode","","Shift+Insert"));
	glutAddMenuEntry(E(animate_copy,"Copy Frame","","Ctrl+C"));
	glutAddMenuEntry(E(animate_paste,"Paste Key Frames","","Ctrl+V"));
	glutAddMenuEntry(E(animate_paste_v,"Paste Inter Frames","","Shift+Ctrl+V"));
	glutAddMenuEntry(E(animate_delete,"Delete Frame","","Ctrl+X")); //2022	
	glutAddMenuEntry();	
	glutAddMenuEntry(X(false,animate_play,"Play Animation","","Pause"));
	//REMINDER: wxWidgets doesn't support Ctrl+Pause. Windows generates a Cancel code in response.
	glutAddMenuEntry(X(false,animate_loop,"Play Repeatedly","","Shift+Pause"));		
	glutAddMenuEntry(E(animate_render,"Record Animation...","Animation|Save Animation Images","Alt+Pause"));
	glutAddMenuEntry();
	glutAddMenuEntry(X(false,animate_mode,"Animator Mode","","Shift+Tab"));
	glutAddMenuEntry(E(animate_window,"Animator Window...","","A"));	
	glutAddMenuEntry(E(animate_settings,"Animation Sets...","","Shift+A"));
		extern void animwin_enable_menu(int,int);
		animwin_enable_menu(0,0); //2019

	if(!viewwin_help_menu) //static menu
	{
		//TODO: According to wxOSX Apple moves Help and About menu items
		//to be in the first menu or a special menu of some kind. In that
		//case this menu may duplicate tha functionality or may degenerate 
		//to Help->License, which could be crummy.

		viewwin_help_menu = glutCreateMenu(viewwin_menubarfunc);	

	glutAddMenuEntry(E(help,"Contents","Help|Contents","F1"));	
	glutAddMenuEntry(::tr("License","Help|License"),id_license);		
	glutAddMenuEntry(::tr("About","Help|About"),id_about);
	glutAddMenuEntry();
	glutAddMenuEntry(E(reorder,"Reorder","",","));
	glutAddMenuEntry(E(unscale,"Unscale","","Shift+Alt+1"));
	}
		
	_menubar = glutCreateMenu(viewwin_menubarfunc);
	glutAddSubMenu(::tr("File","menu bar"),viewwin_file_menu);
	glutAddSubMenu(::tr("View","menu bar"),_view_menu);
	glutAddSubMenu(::tr("Tools","menu bar"),viewwin_tool_menu);
	glutAddSubMenu(::tr("Model","menu bar"),viewwin_modl_menu);
	glutAddSubMenu(::tr("Geometry","menu bar"),viewwin_geom_menu);
	glutAddSubMenu(::tr("Materials","menu bar"),viewwin_mats_menu);
	glutAddSubMenu(::tr("Influences","menu bar"),viewwin_infl_menu);
	glutAddSubMenu(::tr("Animation","menu bar"),_anim_menu);	
	glutAddSubMenu(::tr("Help","menu bar"),viewwin_help_menu);
	glutAddSubMenu("",viewwin_toolbar);
	glutAttachMenu(glutext::GLUT_NON_BUTTON);

	#undef E //viewwin_menu_entry
	#undef O //viewwin_menu_radio
	#undef X //viewwin_menu_check
}

static void viewwin_reshape_func(int x, int y)
{
	//reshape is still called after viewwin_close_func
	//and there's no automation for naked GLUT windows.
	//if(!viewwin_list.empty()) //Exit?
	int id = glutGetWindow();
	for(auto&ea:viewwin_list) if(ea->glut_window_id==id)
	viewwin()->reshape(x,y);
}
bool MainWin::reshape(int x, int y)
{
	glutSetWindow(glut_window_id);

	int xx,wx = glutGet(GLUT_WINDOW_WIDTH);
	int yy,wy = glutGet(GLUT_WINDOW_HEIGHT);

	if(x==10000){ x = wx; y = wy; } //HACK

	//FIX ME
	//262 is hardcoded. Want to calculate.
	//NOTE: Depends on text in view menus.
	int m = views.viewsM;
	int n = views.viewsN/m;
	int sbw = sidebar.width();
	enum{ extra=1 }; //PERFECTLY BALANCED
	int mx = std::max(extra+262*m+sbw,x); //520
	int my = std::max(520/2*n,y); //520
		
	if(mx!=x||my!=y) goto wrong_shape;
	
	mx = glutGet(GLUT_SCREEN_WIDTH);
	my = glutGet(GLUT_SCREEN_HEIGHT);

	xx = std::min(mx,x);
	yy = std::min(my,y);

	if(xx!=x||yy!=y)
	{
		mx = xx; my = yy; 
		goto wrong_shape;
	}	
	else if(wx!=x||wy!=y) 
	{
		mx = x; my = y; 
	
		wrong_shape:

		glutReshapeWindow(mx,my);

		//glutPostRedisplay();

		return false;
	}

	if(!sidebar.seen()) //Center?
	{
		//NOTE: ViewPanel::draw has some additional
		//initialization logic that didn't fit here.

		glutPositionWindow((mx-x)/2,(my-y)/2);
	}
	else if(!glutGet(glutext::GLUT_WINDOW_MANAGED))
	{
		config->set("ui_viewwin_width",x);
		config->set("ui_viewwin_height",y);
	}

	//Inform the central drawing area of its size.
	Widgets95::e::set_viewport_area
	(nullptr,nullptr,(int*)views.shape+0,(int*)views.shape+1);
	int c = (views.shape[0]-extra)/m;
	int c0 = (views.shape[0]-extra)-c*m+c;
	for(int i=0;i<m-1;i++,c0=c)
	{
		//TODO: I think locking the row should do.
		views.views[i]->distance(c0,true).repack();
		if(m!=views.viewsN)
		views.views[i+m]->distance(c0,true).repack();
	}
	views.bar1.portside_row.lock(views.shape[0],false);
	views.bar2.portside_row.lock(views.shape[0],false);
	views.bar1.clip(views.shape[0],false,true);
	views.bar2.clip(x,false,true);
	//NOTE: Some X servers won't be able to handle this
	//clipping over the bottom bar. It will need to cut
	//off higher up.
	//FIX ME: !true is because sometimes the z-order
	//mysteriously changes. See SideBar::SideBar too.
	sidebar.clip(sbw,y-views.status.nav.drop()-2,true);

	glutPostRedisplay(); return true;
}

static int viewwin_init()
{
	//The width needs to be established to get correct number of rows in 
	//toolbar so the height setting is correct.
	glutInitWindowSize 
	(config->get("ui_viewwin_width",640),config->get("ui_viewwin_height",520));
	glutInitDisplayMode(GLUT_RGB|GLUT_DOUBLE|GLUT_DEPTH);
	return glutCreateWindow(viewwin_title);
}
MainWin::MainWin(Model *model):
_deferredModelChanged(Model::ChangeAll),
model(/*model*/),		
glut_window_id(viewwin_init()),
clipboard_mode(),
animation_mode(3),
animation_bind(1),
//NOTE: Compilers (MSVC) may not like "this".
//Makes parent/child relationships headaches.
views(*this),sidebar(*this),
nselection(),fselection(),
_animation_win(),
_transform_win(),
_projection_win(),
_texturecoord_win(),
_vpsettings_win(),
_sync_tool(0),_sync_sel2(-1),
_prev_tool(3),_curr_tool(1),
_prev_shift(),_curr_shift(),_none_shift(),
_prev_ortho(3),_prev_persp(0),
_prev_view(),_curr_view()
{
	assert(config&&keycfg);

	if(!model) model = new Model;
	
	model->setUndoEnabled(false);

	viewwin_list.push_back(this);

	glutext::glutSetWindowIcon(pics[pic_icon]);
	

		_swap_models(model);


	views.setCurrentTool(toolbox.getCurrentTool(),0,false);
	//views.status.setText(::tr("Press F1 for help using any window"));
	views.status.setText(::tr(Win::f1_titlebar::msg()));
		

		_init_menu_toolbar(); //LONG SUBROUTINE		

		views.snap_overlay = viewwin_snap_overlay;


	model->clearUndo(); //???

	glutSetWindow(glut_window_id);
	glutext::glutCloseFunc(viewwin_close_func); 
	Widgets95::glut::set_glutReshapeFunc(viewwin_reshape_func);
	glutWindowStatusFunc(viewwin_status_func);
	glutPopWindow(); //Make spacebar operational.
		
	viewwin_toolboxfunc(id_tool_none);	

	//#ifdef _WIN32
	//The toolbar is 1px too close for comfort.
	//Making room for shadow rules.
	views.bar1.main->space<3>(2);
	views.timeline.space<3>(1);
	sidebar.main->space<3>(3);
	//#endif

	views.timeline.drop(views.bar1.exterior_row.lock
	(false,sidebar.anim_panel.media_nav.drop()+1).drop()+1);
	views.params.nav.space<Win::top>(2);
	views.params.nav.lock(false,views.bar1.exterior_row.drop());
	//reshape(config->get("ui_viewwin_width",0),config->get("ui_viewwin_height",0));
	reshape(glutGet(GLUT_INIT_WINDOW_WIDTH),glutGet(GLUT_INIT_WINDOW_HEIGHT));
}
MainWin::~MainWin()
{
	//NOTE: viewwin_close_func implements teardown logic
	//prior to the C++ destructor stage.

	//log_debug("deleting view window\n");

	/*Not using ContextT since lists/contexts are shared.
	//views.freeTextures();
	//log_debug("freeing texture for viewport\n");
	model->removeContext(static_cast<ContextT>(this));*/

	//log_debug("deleting view window %08X, model %08X\n",this,model);

	if(viewwin_list.empty())
	{
		#ifdef _DEBUG
		#ifdef _WIN32
		//wxFileConfig and Model::Background sometimes crash
		//deallocating std::string.
		config->flush(); keycfg->flush();
		TerminateProcess(GetModuleHandle(nullptr),0); //Fast. 
		#endif	
		#endif
	}

	//GLX can't have its OpenGL context at this stage.
	//delete _swap_models(nullptr);
	assert(!model);

	//glutHideWindow();
	for(int*m=&_view_menu;m<=&_menubar;m++)
	glutDestroyMenu(*m);

	//Could do this... Win::_delete is currently letting
	//nonmodal top-level windows be deleted. That includes
	//the help window, etc.
	//delete _animation_win;
	//delete _transform_win;
	//delete _projection_win;
	//delete _texturecoord_win;
	//delete _vp_settings_win;
}

Model *MainWin::_swap_models(Model *swap)
{
	selection.clear(); //best?

	if(swap==model) //NEW
	{
		assert(model!=swap); return nullptr; 
	} 

	if(model) model->removeObserver(this);

	if(swap) swap->addObserver(this);

	//FYI: If !swap this is what ~MainWin used
	//to do. viewwin_close_func calls this now.
	std::swap(swap,const_cast<Model*&>(model));	
	 
	//NOTE: must initialize the viewport settings
	//so ModelViewport::getUnitWidth isn't divide
	//by zero and just to ensure it's initialized
	//with meaningful values (what are defaults?)
	//YUCK
	//https://github.com/zturtleman/mm3d/issues/56
	if(model)
	if(!swap) 
	{
		int cmp = config->get("ui_ortho_drawing",0);
		if(cmp==_prev_ortho) _prev_ortho = 0;
		model->setCanvasDrawMode((ModelViewport::ViewOptionsE)cmp);
		cmp = config->get("ui_persp_drawing",3);
		if(cmp==_prev_persp) _prev_persp = 3;
		model->setPerspectiveDrawMode((ModelViewport::ViewOptionsE)cmp);

		if(config->get("ui_render_bad_textures",true))
		model->setDrawOption(Model::DO_BADTEX,true);
		if(!config->get("ui_render_backfaces",false))
		model->setDrawOption(Model::DO_BACKFACECULL,true);
		if(config->get("ui_render_3d_selections",false))
		model->setDrawSelection(true);	
		if(!config->get("ui_render_bones",true))
		model->setDrawOption(Model::DO_BONES,false);
		if(config->get("uv_animations",true))
		model->setDrawOption(Model::DO_TEXTURE_MATRIX,true);

		Model::ViewportUnits &vu = model->getViewportUnits();
		vu.inc = config->get("ui_grid_inc",1.0); //4.0?
		vu.grid = config->get("ui_grid_mode",0);
		vu.inc3d = config->get("ui_3dgrid_inc",1.0); //4.0?
		vu.ptsz3d = config->get("ui_point_size",0.1); //0.25
		vu.lines3d = config->get("ui_3dgrid_count",6);
		vu.xyz3d = 0; //NEW		
		if(config->get("ui_3dgrid_xy",false)) vu.xyz3d|=4;
		if(config->get("ui_3dgrid_xz",true)) vu.xyz3d|=2;
		if(config->get("ui_3dgrid_yz",false)) vu.xyz3d|=1;
		vu.fov = config->get("ui_3d_cam_fov",45.0);
		vu.znear = config->get("ui_3d_cam_znear",0.1);
		vu.zfar = config->get("ui_3d_cam_zfar",1000);
		if(config->get("ui_snap_grid",false)) vu.snap|=vu.UnitSnap;
		if(config->get("ui_snap_vertex",false)) vu.snap|=vu.VertexSnap;
		if(config->get("uv_snap_grid",false)) vu.snap|=vu.SubpixelSnap;
		if(config->get("uv_snap_vertex",false)) vu.snap|=vu.UvSnap;
		vu.unitsUv = config->get("uv_grid_subpixels",2);
		vu.snapUv[0] = config->get("uv_grid_default_u",0.0);
		vu.snapUv[1] = config->get("uv_grid_default_v",0.0);
		vu.no_overlay_option = config->get("ui_no_overlay_option",false);
	}
	else
	{
		model->setDrawOption(swap->getDrawOptions(),true);
		model->setDrawSelection(swap->getDrawSelection());
		model->getViewportUnits() = swap->getViewportUnits();

		model->setCanvasDrawMode(swap->getCanvasDrawMode());
		model->setPerspectiveDrawMode(swap->getPerspectiveDrawMode());
	}

	views.setModel();
	if(!model) return swap; //Closing?

	//Close doesn't enable this.
	model->setUndoEnabled(true);
		
	//FIX ME
	//NEW: Since "ContextT" is not in play the textures 
	//aren't loaded automatically by model.
	glutSetWindow(glut_window_id);
	//REMINDER: _projection_win/_texturecoord_win needs
	//loadTextures for setModel.
	model->loadTextures();

	sidebar.setModel();
	if(_animation_win)
	_animation_win->setModel();
	if(_projection_win)
	_projection_win->setModel();
	if(_texturecoord_win)
	_texturecoord_win->setModel();
	if(_vpsettings_win)
	{
		//HACK: restore model->getViewportUnits() reference?
		close_viewport_window();
	//	perform_menu_action(id_view_settings);
	}

	_rewrite_window_title();

		frame(); //NEW
	
	std::string e;
	while(model->popError(e))
	model_status(model,StatusError,STATUSTIME_LONG,"%s",e.c_str());

	if(int st=model->getAddedLayers()&(4|8|16))
	{
		//HACK: get noise out of the way to look better
		if(e.empty()) views.status.clear();

		int lc = 0;
		char l[3][2] = {}; for(int i=2;i<=4;i++)
		{
			if(st&(1<<i)){ lc++; l[i-2][0] = '0'+i; }
		}

		const char *f = ::tr("%d secondary layer has data (%s%s%s)");
		if(lc>1) f = ::tr("%d secondary layers have data (%s%s%s)");
		model_status(model,StatusNotice,STATUSTIME_SHORT,f,lc,l[0],l[1],l[2]);
	}

	//This is needed to jumpstart.
	modelChanged(Model::ChangeAll); return swap;
}

bool MainWin::save_work_prompt()
{
	#ifndef CODE_DEBUG
	if(!model->getSaved())
	{
		int ret = Win::InfoBox(::tr("Save first?"),
		::tr("Model has been modified\n"
		"Do you want to save before closing?"),
		id_yes|id_no|id_cancel,id_cancel);
		if(ret==id_yes&&!save_work())
		{
			ret = id_cancel;
		}
		return ret!=id_cancel;
	}		
	#endif
	return true;
}
bool MainWin::save_work()
{
	const char *filename = model->getFilename();
	if(!*filename)
	return MainWin::save(model,false); //save-as
	
	Model::ModelErrorE err =
	FilterManager::getInstance()->writeFile(model,filename,false);
	if(err==Model::ERROR_NONE)
	{
		model->setSaved();
		_rewrite_window_title();
		viewwin_mru(viewwin_mruf_menu,(char*)filename);
		return true;
	}
	if(Model::operationFailed(err))
	{
		msg_error("%s:\n%s",filename,modelErrStr(err,model));
	}
	return false;
}
bool MainWin::save(Model *model, bool expdir)
{
	std::string verb = "Save: ";
	verb+=::tr(expdir?"All Exportable Formats":"All Writable Formats");
	verb+=" (";
	verb+=FilterManager::getInstance()->getAllWriteTypes(expdir);
	verb+=");; "; //Qt
	verb+=::tr("All Files(*)");

	const char *title;
	const char *modelFile;
	std::string file; if(expdir)
	{
		//title = "Export model";
		title = "Export";
		modelFile = model->getFilename();
		file = config->get("ui_export_dir");
	}
	else
	{
		//title = "Save model file as";
		title = "Save As";
		modelFile = model->getExportFile();
		file = config->get("ui_model_dir");
	}
	if(*modelFile) //???
	{
		std::string fullname,fullpath,basename; //REMOVE US
		normalizePath(modelFile,fullname,fullpath,basename);
		file = fullpath;
	}
	if(file.empty()) file = "."; 
	file = Win::FileBox(file,verb,::tr(title));
	if(!file.empty())
	{	
		if(file.find('.')==file.npos)
		{
			file.append(".mm3d");
		}

		Model::ModelErrorE err =
		FilterManager::getInstance()->writeFile(model,file.c_str(),expdir);

		if(err==Model::ERROR_NONE)
		{
			utf8 cfg; if(expdir)
			{
				cfg = "ui_export_dir";
				model->setExportFile(file.c_str());
			}
			else
			{
				cfg = "ui_model_dir";
				model->setSaved();
				model->setFilename(file.c_str());
			}
			
			viewwin_mru(viewwin_mruf_menu,(char*)file.c_str());

			config->set(cfg,file,file.rfind('/'));			

			return true;
		}
		else if(Model::operationFailed(err))
		{
			msg_error(modelErrStr(err,model));
		}
	}
	return false;
}

void MainWin::merge(Model *model, bool animations_only_non_interactive)
{
	std::string verb = "Open: ";
	verb+=::tr("All Supported Formats","model formats");
	verb+=" (";
	verb+=FilterManager::getInstance()->getAllReadTypes();
	verb+=");; "; //Qt
	verb+=::tr("All Files(*)");

	std::string file = config->get("ui_model_dir");
	if(file.empty()) file = ".";
	file = Win::FileBox(file,verb,::tr("Open")); //"Open model file"
	if(!file.empty())
	{
		Model::ModelErrorE err;
		Model *merge = new Model;
		if((err=FilterManager::getInstance()->readFile(merge,file.c_str()))==Model::ERROR_NONE)
		{
			model_show_alloc_stats();

			if(!animations_only_non_interactive)
			{
				extern void mergewin(Model*,Model*);
				mergewin(model,merge);
			}
			else if(model->mergeAnimations(merge))
			{
				model->operationComplete(::tr("Merge animations","operation complete"));
			}

			viewwin_mru(viewwin_mruf_menu,(char*)file.c_str());

			config->set("ui_model_dir",file,file.rfind('/'));
		}
		else if(Model::operationFailed(err))
		{
			msg_error("%s:\n%s",file.c_str(),modelErrStr(err,merge));
		}
		delete merge;
	}
}

void MainWin::run_script(const char *file)
{
	#ifdef HAVE_LUALIB

	if(!file) return; //???
	
	LuaScript lua;
	LuaContext lc(model);
	luaif_registerfunctions(&lua,&lc);

	std::string scriptfile = file;

	std::string fullname,fullpath,basename; //REMOVE US
	normalizePath(scriptfile.c_str(),fullname,fullpath,basename); //???

	//log_debug("running script %s\n",basename.c_str());
	int rval = lua.runFile(scriptfile.c_str());

	viewwin_mru(viewwin_mrus_menu,(char*)file.c_str());

	if(rval==0) //UNFINISHED
	{
		//log_debug("script complete,exited normally\n");
		//QString str = ::tr("Script %1 complete").arg(basename.c_str());
		//model_status(model,StatusNormal,STATUSTIME_SHORT,"%s",(const char *)str);
		model_status(model,StatusNormal,STATUSTIME_SHORT,::tr("Script %s complete"),basename.c_str());

	}
	else
	{
		log_error("script complete,exited with error code %d\n",rval);
		//QString str = ::tr("Script %1 error %2").arg(basename.c_str()).arg(lua.error());
		//model_status(model,StatusError,STATUSTIME_LONG,"%s",(const char *)str);
		model_status(model,StatusError,STATUSTIME_LONG,::tr("Script %1 error %2"),basename.c_str(),lua.error());
	}

	model->setNoAnimation();
	model->operationComplete(basename.c_str());

	//views.modelUpdatedEvent();

	#endif // HAVE_LUALIB
}

void MainWin::frame(int scope)
{
	auto os = (Model::OperationScopeE)scope;

	//HACK: This is really viewport dependent.
	model->setPrimaryLayers((int)views->layer);
	
	//TODO: Need easy test for no selection.
	double x1,y1,z1,x2,y2,z2;	
	if(model->getBounds(os,&x1,&y1,&z1,&x2,&y2,&z2))
	{
		//NEW: true locks 2d viewports.
		views.frameArea(1==viewwin_interlock,x1,y1,z1,x2,y2,z2);
	}
	else //NEW: If nothing is selected, frame model. 
	{	
		if(os==Model::OS_Selected)
		{
			frame(Model::OS_Global);
		}
		else //NEW: Center grid axis?
		{
			auto &va = model->getViewportUnits();
			double x = va.lines3d*va.inc3d;
			views.frameArea(1==viewwin_interlock,-x,-x,-x,x,x,x);
		}
	}
}

void MainWin::open_texture_window()
{
	if(_texturecoord_win) //NEW: Toggle?
	{
		if(!_texturecoord_win->hidden())
		{
			//TODO? Probably can just hide?
			//return _texturecoord_win->hide();
			glutSetWindow(_texturecoord_win->glut_window_id());
			return _texturecoord_win->submit(_texturecoord_win->close);
		}
	}

	if(!model->getSelectedTriangleCount())
	{
		//2019: Seems obtrusive?
		//msg_info(::tr("You must select faces first.\nUse the 'Select Faces' tool.","Notice that user must have faces selected to open 'edit texture coordinates' window"));
		//return;
		model_status(model,StatusError,STATUSTIME_LONG,TRANSLATE("Command","Select faces to edit"));
	}
	if(!_texturecoord_win)
	{
		glutSetWindow(glut_window_id); //2024
		glutPopWindow();
		_texturecoord_win = new TextureCoordWin(*this);
	}
	_texturecoord_win->open();
}
void MainWin::open_projection_window()
{
	if(_projection_win) //NEW: Toggle?
	{
		if(!_projection_win->hidden())
		{
			//TODO? Probably can just hide?
			//return _projection_win->hide();
			glutSetWindow(_projection_win->glut_window_id());
			return _projection_win->submit(id_ok);
		}
	}

	if(!_projection_win)
	{
		glutSetWindow(glut_window_id); //2024
		glutPopWindow();
		_projection_win = new ProjectionWin(*this);
	}
	_projection_win->open();
}
void MainWin::open_transform_window()
{
	if(_transform_win) //NEW: Toggle?
	{
		if(!_transform_win->hidden())
		{
			//TODO? Probably can just hide?
			//return _transform_win->hide();
			glutSetWindow(_transform_win->glut_window_id());
			return _transform_win->submit(_transform_win->ok.ok);
		}
	}

	if(!_transform_win)
	{
		glutSetWindow(glut_window_id); //2024
		glutPopWindow();
		_transform_win = new TransformWin(*this);	
	}
	_transform_win->open();
}
void MainWin::open_animation_system()
{
	if(!_animation_win)
	{
		extern bool viewwin_menu_origin; //YUCK
		viewwin_menu_origin = true;

		glutSetWindow(glut_window_id); //2024
		glutPopWindow();
		_animation_win = new AnimWin(*this,_anim_menu);
				
		int tt = _sync_tool;
		if(tt==-1) tt = Tool::TT_SelectTool;
		_animation_win->_sync_tools(_sync_tool,_sync_sel2);
	}
	_animation_win->open(false);
}
void MainWin::sync_animation_window()
{
	if(_animation_win)	
	if(!_animation_win->hidden())
	_animation_win->_sync_anim_menu();
}
void MainWin::open_animation_window()
{
	if(_animation_win) //NEW: Toggle?
	{
		if(!_animation_win->hidden())
		{
			//TODO? Probably can just hide?
			//return _animation_win->hide();
			glutSetWindow(_animation_win->glut_window_id());
			return _animation_win->submit(id_ok);
		}
	}

	open_animation_system();
	
	int tt = _sync_tool;
	if(tt==-1) tt = Tool::TT_SelectTool;
	_animation_win->_sync_tools(_sync_tool,_sync_sel2);

	_animation_win->show();
	_animation_win->_sync_anim_menu();	
}
void MainWin::close_viewport_window()
{
	if(_vpsettings_win)
	((Win*)_vpsettings_win)->close(); _vpsettings_win = nullptr;
}

extern void viewwin_undo(int id, bool undo)
{
	MainWin *w = viewwin(id); 
	if(undo) w->undo(); else w->redo();
}
void MainWin::undo()
{
	//log_debug("undo request\n");

	if(model->canUndo())
	{
		//INVESTIGATE ME
		//This string doesn't persist after the operation... might be a bug?
		std::string buf = model->getUndoOpName();

		model->undo();
		
		model_status(model,StatusNormal,STATUSTIME_SHORT,::tr("Undo %s"),buf.c_str()); //"Undo %1"		
	}
	else model_status(model,StatusNormal,STATUSTIME_SHORT,::tr("Nothing to undo"));
}
void MainWin::redo()
{
	//log_debug("redo request\n");

	if(model->canRedo())
	{
		//INVESTIGATE ME
		//This string doesn't persist after the operation... might be a bug?
		std::string buf = model->getRedoOpName();

		model->redo();
		
		model_status(model,StatusNormal,STATUSTIME_SHORT,::tr("Redo %s"),buf.c_str()); //"Redo %1"
	}
	else model_status(model,StatusNormal,STATUSTIME_SHORT,::tr("Nothing to redo"));
}

void MainWin::open(Model *model, MainWin *window)
{
	if(window)
	{
		delete window->_swap_models(model);
	}
	else window = new MainWin(model);
}
bool MainWin::open(const char *file2, MainWin *window)
{
	std::string buf; if(!file2) //openModelDialog
	{		
		std::string &file = buf; //semi-shadowing

		std::string verb = "Open: ";
		verb+=::tr("All Supported Formats","model formats");
		verb+=" (";
		verb+=FilterManager::getInstance()->getAllReadTypes();
		verb+=");; "; //Qt
		verb+=::tr("All Files(*)");

		file = config->get("ui_model_dir");
		if(file.empty()) file = ".";
		file = Win::FileBox(file,verb,::tr("Open model file"));		
		if(!file.empty())
		config->set("ui_model_dir",file,file.rfind('/'));
		if(file.empty()) return false;
	}
	utf8 file = file2?file2:buf.c_str();
	if(window&&!window->save_work_prompt()) 
	{
		return false;
	}

	bool opened = false;

	//log_debug(" file: %s\n",file); //???

	Model *model = new Model;
	auto err = Model::ERROR_NONE;
	if(*file)
	err = FilterManager::getInstance()->readFile(model,file);
	if(err==Model::ERROR_NONE)
	{
		opened = true;

		if(*file) model_show_alloc_stats();

		assert(model->getSaved());

		open(model,window);
	
		if(*file) viewwin_mru(viewwin_mruf_menu,(char*)file);
	}
	else
	{
		if(Model::operationFailed(err))
		{
			msg_error("%s:\n%s",file,modelErrStr(err,model));
		}
		delete model;
	}

	return opened;
}

void MainWin::_rewrite_window_title()
{
	//HACK: Ensure contiguous storage.
	model->m_filename.push_back('*');

	utf8 path = model->getFilename();

	//TODO: Isolate URL path component.
	utf8 name = strrchr(path,'/');

	name = name?name+1:path;

	bool untitled = '*'==*name;
	bool asterisk = !model->getSaved();

	//NOTE: I've not put the software's name in the title. It could be put
	//back. Or it could be be an option.
	if(untitled&&!asterisk)
	{
		name = viewwin_title;
	}
	else if(untitled)
	{
		name = ::tr("[unnamed]","For filename in title bar (if not set)");
		if(!strcmp(name,"[unnamed]"))
		name = "Untitled";
	}
	else if(!asterisk)
	{
		model->m_filename.back() = '\0';
	}

	glutSetWindow(glut_window_id);
	glutSetWindowTitle(name);

	_window_title_asterisk = asterisk;

	model->m_filename.pop_back(); //*
}

extern bool viewwin_menu_origin = false;
struct viewin_menu
{
	viewin_menu(){ viewwin_menu_origin = true; }
	~viewin_menu(){ viewwin_menu_origin = false; }
};
void viewwin_menubarfunc(int id) //extern
{
	viewwin()->perform_menu_action(id);
}
void MainWin::perform_menu_action(int id)
{		
	//* marked cases are unsafe to call
	//without glutSetWindow.

	MainWin *w = this; Model *m = model;

	switch(id) //these center prompts on the mouse
	{		
	case id_file_save_prompt:

		//Note: id_no is included so the behavior is identical to closing
		if(id_yes==Win::InfoBox(::tr("Save over model?"),
		m->getSaved()?
		::tr("The model is unmodified\n""Overwrite file on disk anyway?"):
		::tr("Model has been modified\n""Overwrite file on disk?"),
		id_yes|id_no|id_cancel,id_cancel))
		if(w->save_work())
		{
			//2022: show full path and follow suit of hotkey combo
			model_status(m,StatusNormal,STATUSTIME_SHORT,::tr("Saved %s"),model->getFilename());			
		}
		
		return;
	}

	viewin_menu raii; //changes the pop-up behavior

	switch(id) //these put prompts beside the mouse
	{
	case 127: //Delete?

		//REMOVE ME
		//Note: The default key combo is Ctrl+Shift+D.
		if(clipboard_mode&&m->inAnimationMode())
		{
			id = id_animate_delete; goto delete2; 
		}
		return viewwin_geomenufunc(viewwin_deletecmd);

	case id_viewselect:
		extern void viewpanel_special_func(int,int,int);
		return viewpanel_special_func(GLUT_KEY_F4,0,0);
	case id_view_invert:
		return viewpanel_special_func(-'\\',0,0);
	case id_view_invert_all:
		return viewpanel_special_func(-'|',0,0);
	case id_view_project:
		return viewpanel_special_func(-'`',0,0);
	case id_view_project_all:
		return viewpanel_special_func(-'~',0,0);
	case id_fullscreen:
		return viewpanel_special_func(GLUT_KEY_F11,0,0);
		//Really viewwin_toolboxfunc catches these.
	case id_toolparams: assert(0);
		return viewpanel_special_func(GLUT_KEY_F2,0,0);
	case id_properties: assert(0);
		return viewpanel_special_func(GLUT_KEY_F3,0,0);

	case id_view_layer_0:
	case id_view_layer_1:
	case id_view_layer_2:
	case id_view_layer_3:
	case id_view_layer_4:

		id-=id_view_layer_0;
		for(int i=w->views.viewsN;i-->0;)
		w->views.ports[i].setLayer(id);
		layer_next:
		model_status(model,StatusNormal,STATUSTIME_SHORT,
		id?::tr("Layer %d"): ::tr("Hidden layer"),id);
		break;

	case id_view_layer_lower_all:
	case id_view_layer_raise_all:

		id = id==id_view_layer_lower_all?-1:1;
		id+=w->views->layer.int_val();
		if(id<0) id = 4;
		if(id>4) id = 0;
		return perform_menu_action(id_view_layer_0+id);

	case id_view_layer_lower:
	case id_view_layer_raise:

		id = id==id_view_layer_lower?-1:1;
		id+=w->views->layer.int_val();
		if(id<0) id = 4;
		if(id>4) id = 0;
		w->views->port.setLayer(id);
		goto layer_next;

	case id_view_overlay:

		if(int l=w->views->layer) id+=l;		
		else return model_status(m,StatusError,STATUSTIME_LONG,::tr("Hide layer can't overlay"));
		//break;

	case id_view_overlay_1:
	case id_view_overlay_2:
	case id_view_overlay_3:
	case id_view_overlay_4:	

		if(id-=id_view_overlay_1-1)
		{
			int l = 1<<id;
			int ll = m->getOverlayLayers();
			ll&l?ll&=~l:ll|=l;
			m->setOverlayLayers(ll);

			model_status(model,StatusNormal,STATUSTIME_SHORT,
			ll&l?::tr("Layer %d overlay set"): ::tr("Layer %d overlay unset"),id);

			w->views.status.overlay.set(ll,viewwin_snap_overlay);
		}
		else assert(0); break;

	case id_view_overlay_snap:

		viewwin_snap_overlay = 0!=glutGet(glutext::GLUT_MENU_CHECKED);

		for(auto&ea:viewwin_list)
		{
			ea->views.snap_overlay = viewwin_snap_overlay;
			ea->views.status.overlay.set(ea->model->getOverlayLayers(),viewwin_snap_overlay);
		}

		return;

		/*File menu*/
	
	case id_file_new:
		
		new MainWin; return;

	case id_file_open: 

		MainWin::open(nullptr,w); return;

	case id_file_resume: 

		viewwin_mrumenufunc(0); return;

	case id_file_close:

		//2019: Changing behavior.
		
		if(1) //Open blank model?
		{
			MainWin::open("",this);
			glutPostRedisplay();
		}
		else //Close window?
		{
			viewwin_close_func(); //*
		}
		return;

	case id_file_save: 
		
		if(w->save_work()) //2022: indicate hotkey combo
		{
			//HACK: -1 is suppressing beep sound (file name sould be loud enough)
			model_status(m,StatusNotice,STATUSTIME_LONG-1,::tr("Saved %s"),model->getFilename());
		}		
		return;
 
	case id_file_save_as: case id_file_export:
		
		if(MainWin::save(m,id_file_export==id))
		if(id_file_save_as==id)
		w->_rewrite_window_title();
		return;

	case id_file_export_selection: 

		if(!w->selection.empty())		
		if(Model*tmp=m->copySelected())
		{
			MainWin::save(tmp,true);
			
			delete tmp; return;
		}
		model_status(m,StatusError,STATUSTIME_LONG,
		//::tr("You must have at least 1 face, joint, or point selected to Export Selected"));
		::tr("Select faces, joints, points, or projections to export"));
		return;

	//case id_file_run_script:
	#ifdef HAVE_LUALIB
	{
		case id_file_run_script:
		std::string file = config->get("ui_script_dir");
		if(file.empty()) file = ".";	
		file = Win::FileBox(file,
		"Lua scripts (*.lua)" ";; " "All Files(*)", //::tr("All Files(*)")
		::tr("Open model file"));
		if(!file.empty())
		{			
			w->run_script(file);
			config->set("ui_script_dir",file,file.rfind('/'));
		}
		return;
	}
	#endif //HAVE_LUALIB

	case id_file_plugins: 
		
		extern void pluginwin(); 
		pluginwin(); return;

	case id_file_prefs_rmb:

		config->set("ui_prefs_rmb",viewwin_rmb=!viewwin_rmb);
		return;

	case id_normal_order: //EXPERIMENTAL
	case id_normal_click:	

		id-=id_normal_order;
		if(viewwin_face_view==(id!=0)) //Toggle?
		{
			id = !id;		
			glutext::glutMenuEnable(id_normal_order+id,glutext::GLUT_MENU_CHECK);
		}
		viewwin_face_view = 0!=id;
		config->set("ui_face_view",id);
		for(auto&ea:viewwin_list)
		ea->views.status._face_view.indicate(!id);
		return;

	case id_normal_ccw:

		id = 0!=glutGet(glutext::GLUT_MENU_CHECKED);
		viewwin_face_ccw = 0!=id;
		config->set("ui_prefs_face_ccw",id);
		for(auto&ea:viewwin_list)
		ea->views.status._face_view.set_name(id?"Ccw":"Cw");
		return;

	case id_file_quit: 
		
		//TODO: Confirm close all windows if there are more than one.
		if(MainWin::quit()) 
		{
			//qApp->quit();
		}
		return;

	/*View->Render Options menu*/

	case id_rops_draw_bones:
	
		id = Model::DO_BONES&m->getDrawOptions()?0:1;
		config->set("ui_render_bones",id);
		m->setDrawOption(Model::DO_BONES,id==1);
		break;

	case id_rops_hide_joints:

		id = !m->getDrawJoints();

		//if(m->getSelectedBoneJointCount())
		//if('Y'==msg_info_prompt(::tr("Cannot hide with selected joints.  Unselect joints now?"),"yN"))		
		{
			if(!id&&m->unselectAllBoneJoints())
			m->operationComplete(::tr("Hide bone joints"));
		}
		//else return;

		//2022: May as well not save since it's ignored.
		//config->set("ui_render_joints",id);
		m->setDrawJoints(0!=id,Model::ShowJoints);
		break;

	case id_rops_hide_projections:

		id = !m->getDrawProjections();
		//if(m->getSelectedProjectionCount())	
		//if('Y'==msg_info_prompt(::tr("Cannot hide with selected projections.  Unselect projections now?"),"yN"))
		{
			if(!id&&m->unselectAllProjections())
			m->operationComplete(::tr("Hide projections"));
		}
		//else return; 
		
		//2022: May as well not save since it's ignored.
		//config->set("ui_render_projections",id);
		m->setDrawProjections(0!=id,Model::ShowProjections);
		break;
		
	case id_rops_hide_lines: 

		id = !m->getDrawSelection();
		config->set("ui_render_3d_selections",id);
		m->setDrawSelection(0!=id);
		break;

	case id_rops_hide_backs:
		
		id = Model::DO_BACKFACECULL&m->getDrawOptions()?0:1;
		config->set("ui_render_backfaces",!id);
		m->setDrawOption(Model::DO_BACKFACECULL,0!=id);
		break;

	case id_rops_draw_badtex: 
		
		id = Model::DO_BADTEX&m->getDrawOptions()?0:1;
		config->set("ui_render_bad_textures",0!=id);
		m->setDrawOption(Model::DO_BADTEX,0!=id);
		break;

	case id_rops_draw_buttons:

		id = !m->getViewportUnits().no_overlay_option;
		m->getViewportUnits().no_overlay_option = id!=0;
		if(_texturecoord_win&&!_texturecoord_win->hidden())
		{
			_texturecoord_win->main_panel()->redraw();
		}
		if(_projection_win&&!_projection_win->hidden())
		{
			_projection_win->main_panel()->redraw();
		}
		if(_animation_win&&!_animation_win->hidden())
		{
			_animation_win->main_panel()->redraw();
		}
		config->set("ui_no_overlay_option",id!=0);
		break;

	/*View menu*/
	case id_frame_all:
	case id_frame_selection:
		
		w->frame(id==id_frame_all);
		return;

	case id_frame_lock:
	
		viewwin_interlock = glutGet(glutext::GLUT_MENU_CHECKED);
		for(auto&ea:viewwin_list) //inverting sense
		ea->views.status._interlock.indicate(0==viewwin_interlock);
		return;
	
	case id_tool_shift_hold: //2022
	{
		id = glutGet(glutext::GLUT_MENU_CHECKED);
		config->set("ui_tool_shift_hold",id==0);
		viewwin_shifthold = id==0;
		w->views.status._shifthold.indicate(id==0);

		auto *tc = w->_texturecoord_win; //YUCK
		//texturecoord_entry_cb/etc. is purposely
		//deactivating to use Tab with Toggle Tool.
		//if(tc&&tc==Win::event.active_control_ui) 
		if(tc&&tc->glut_window_id()==glutGetWindow())
		{
			assert(!tc->hidden());
			auto &f = w->views.status._texshlock;
			if(id==f.int_val()) //inverted
			tc->texture._tool_bs_lock = f.indicate(id==0)?Tool::BS_Shift:0;
		}
		else 
		{
			auto &f = w->views.status._shiftlock;
			if(id==f.int_val()) //inverted
			w->views._bs_lock = f.indicate(id==0)?Tool::BS_Shift:0;
		}
		return;
	}
	case id_tool_shift_lock: //2022
	{
		auto *tc = w->_texturecoord_win; //YUCK
		//texturecoord_entry_cb/etc. is purposely
		//deactivating to use Tab with Toggle Tool.
		//if(tc&&tc==Win::event.active_control_ui)
		if(tc&&tc->glut_window_id()==glutGetWindow())
		{
			assert(!tc->hidden());
			auto &f = w->views.status._texshlock;
			tc->texture._tool_bs_lock = f.indicate(!f)?Tool::BS_Shift:0;
		}
		else
		{
			auto &f = w->views.status._shiftlock;
			w->views._bs_lock = f.indicate(!f)?Tool::BS_Shift:0;
		}
		return;
	}
	//case id_view_props: //REMOVING
	//	
	//	w->sidebar.prop_panel.nav.open(); return;
	//
	case id_ortho_wireframe:
	case id_ortho_flat:
	case id_ortho_smooth:
	case id_ortho_texture:
	case id_ortho_blend:
		
		id-=id_ortho_wireframe; //Toggle?
		{
			int curr = m->getCanvasDrawMode();
			if(id==curr) std::swap(id,_prev_ortho);
			else _prev_ortho = curr;
			glutext::glutMenuEnable(id_ortho_wireframe+id,glutext::GLUT_MENU_CHECK);
		}		
		config->set("ui_ortho_drawing",id);
		m->setCanvasDrawMode((ModelViewport::ViewOptionsE)id);
		break;
	
	case id_persp_wireframe: //id_view_prsp1
	case id_persp_flat:
	case id_persp_smooth:
	case id_persp_texture:
	case id_persp_blend:

		id-=id_persp_wireframe; //Toggle?
		{
			int curr = m->getPerspectiveDrawMode();
			if(id==curr) std::swap(id,_prev_persp);			
			else _prev_persp = curr;
			glutext::glutMenuEnable(id_persp_wireframe+id,glutext::GLUT_MENU_CHECK);
		}		
		config->set("ui_persp_drawing",id);
		m->setPerspectiveDrawMode((ModelViewport::ViewOptionsE)id);
		break;

		//_view lets Q toggle modes.
	case id_view_1:   _view(id,&ViewPanel::view1);   break;
	case id_view_2:   _view(id,&ViewPanel::view2);   break;
	case id_view_1x2: _view(id,&ViewPanel::view1x2); break;
	case id_view_2x1: _view(id,&ViewPanel::view2x1); break;
	case id_view_2x2: _view(id,&ViewPanel::view2x2); break;
	case id_view_3x2: _view(id,&ViewPanel::view3x2); break;

	case id_view_swap: w->views.rearrange(1); break;
	case id_view_flip: w->views.rearrange(2); break;

	case id_view_settings:
		
		extern void viewportsettings(MainWin&); 
		viewportsettings(*w); break;

	case id_view_init: views.reset(); break;

	case id_view_persp: case id_view_ortho:
	case id_view_right: case id_view_left:
	case id_view_front: case id_view_back:
	case id_view_top: case id_view_bottom:

		w->views.modelview((Tool::ViewE)(id-id_view_persp));
		break;

	case id_snap_grid: case id_snap_vert:
	{
		bool x = 0!=glutGet(glutext::GLUT_MENU_CHECKED);

		config->set(id==id_snap_grid?"ui_snap_grid":"ui_snap_vertex",x);
		
		Model::ViewportUnits &vu = m->getViewportUnits();

		int y = id==id_snap_grid?vu.UnitSnap:vu.VertexSnap;

		if(x) vu.snap|=y; else vu.snap&=~y; 

		if(id==id_snap_vert) w->views.status._vert_snap.indicate(x);
		if(id==id_snap_grid) w->views.status._grid_snap.indicate(x);

		return;
	}
	/*Model menu*/
	case id_edit_undo: w->undo(); break;
	case id_edit_redo: w->redo(); break;
	case id_edit_userdata:
	case id_edit_utildata:
		
		extern void metawin(MainWin&,bool);
		metawin(*w,id==id_edit_utildata); return;

	case id_transform: 
		
		w->open_transform_window(); return;

	//case id_modl_boolop: //REMOVING 
	//	
	//	w->sidebar.bool_panel.nav.open(); return;

	case id_background_settings: 
		
		extern void backgroundwin(Model*);
		backgroundwin(m); return;

	case id_merge_models:
	case id_merge_animations:

		MainWin::merge(m,id==id_merge_animations); 
		return;

	case id_clean_animations:

		extern void animsetwin_clean(MainWin&);
		animsetwin_clean(*w);
		return;

	/*Materials menu*/
	case id_group_settings:

		extern void groupwin(Model*);
		groupwin(m); return;

	case id_material_settings: 

		extern void texwin(Model*);
		texwin(m); return;

	case id_material_cleanup: 
		
		extern void groupclean(Model*);
		groupclean(m); return;

	case id_refresh_textures: 
	
		//NOTE: It does timestamp comparison, but I think
		//it should be limited to the current main window.
		if(TextureManager::getInstance()->reloadTextures())		
		for(auto ea:viewwin_list)
		{
			ea->model->invalidateTextures(); //OVERKILL
			ea->views.modelUpdatedEvent();
		}
		return;

	case id_projection_settings: 
		
		w->open_projection_window(); return;
	
	case id_uv_editor:

		w->open_texture_window(); return;

	case id_uv_render:
		
		extern void painttexturewin(Model*);
		painttexturewin(m); return;
	
	case id_reorder:

		extern void pointwin(MainWin&);
		pointwin(*w);
		return;

	/*Influences menu*/
	case id_joint_settings: 
		
		extern void jointwin(MainWin&,int);
		jointwin(*w,viewwin_joints100); 
		return;
		
	case id_joint_100:
		
		viewwin_joints100 = glutGet(glutext::GLUT_MENU_CHECKED);
		config->set("ui_joint_100",viewwin_joints100);
		for(auto&ea:viewwin_list) //inverting sense
		ea->views.status._100.indicate(viewwin_joints100==0);
		return;
		
	case id_joint_attach_verts:
	case id_joint_weight_verts:
	case id_joint_remove_bones:
	case id_joint_remove_selection:
	case id_joint_simplify:
	case id_joint_select_bones_of:
	case id_joint_select_verts_of:
	case id_joint_select_points_of:
	case id_joint_unnassigned_verts:
	case id_joint_unnassigned_points:

		extern void viewwin_influences(MainWin&,int);
		viewwin_influences(*w,id); return;

	case id_joint_draw_bone:

		for(auto&ea:selection) if(ea.type==m->PT_Joint)
		{
			bool &b = m->getJointList()[ea.index]->m_bone;
			b = !b;
			#ifdef NDEBUG
//			#error need undo/redo for id_joint_draw_bone 
			#endif
			//m->setSaved(false); //Doesn't do anything?
		}
		model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
		::tr("Changed appearance of selected bone joints"));
		break;

	case id_animate_settings: 
		
		extern void animsetwin(MainWin&);
		animsetwin(*this); return;

	case id_animate_render:
		
		extern void animexportwin(Model*,ViewPanel*);
		animexportwin(m,&w->views); return;
		
	case id_animate_window:
		
		open_animation_window(); return;

	case id_animate_copy:
	case id_animate_paste:
	case id_animate_paste_v:
	case id_animate_delete: delete2:	
	case id_animate_insert:

	case id_animate_mode:
	case id_animate_play:
	case id_animate_stop:
	//case id_animate_loop:
	//case id_animate_snap:
	//case id_animate_bind:
	//case id_animate_mode_1:
	//case id_animate_mode_2:
	//case id_animate_mode_3:
	//case id_animate_insert:
		
		if(!_animation_win)
		open_animation_system();
		_animation_win->submit(id);
		return;
	
	case id_animate_loop:

		id = sidebar.anim_panel.loop.picture();
		id = id==pics[pic_stop]?pic_loop:pic_stop;
		sidebar.anim_panel.loop.picture(pics[id]).redraw();
		if(auto*aw=w->_animation_win)
		aw->loop.picture(pics[id]).redraw();
		glutSetMenu(w->_anim_menu);
		glutext::glutMenuEnable(id_animate_loop,
		id==pic_loop?glutext::GLUT_MENU_CHECK:glutext::GLUT_MENU_UNCHECK);
		return;

	case id_animate_snap:
	
		id = 0!=glutGet(glutext::GLUT_MENU_CHECKED);
		config->set("ui_snap_keyframe",id);
		//inverting sense		
		w->views.status._keys_snap.indicate(!id);		
		return;

	case id_animate_bind:

		id = glutGet(glutext::GLUT_MENU_CHECKED);
		const_cast<int&>(w->animation_bind) = id;
		w->views.status._anim_bind.indicate(id==0);
		if(m->setSkeletalModeEnabled(id!=0))
		m->operationComplete(::tr("Set skeletal mode","operation complete"));
		return;

	case id_animate_mode_1: case id_animate_mode_2: case id_animate_mode_3:	
	
		id = id-id_animate_mode_1+1; 
		if(id!=w->animation_mode) 
		const_cast<int&>(w->animation_mode) = id;
		else return;
		if(w->views.status._anim_mode.indicate(3!=id))
		w->views.status._anim_mode.set_name(1==id?"Sam":"Fam");
		if(auto cmp=m->getAnimationMode())
		{
			m->setAnimationMode((Model::AnimationModeE)id);
			if(cmp!=m->getAnimationMode())
			m->operationComplete(::tr("Start animation mode","operation complete"));
		}
		return;
	
	case id_clipboard_mode:

		const_cast<int&>(w->clipboard_mode) = glutGet(glutext::GLUT_MENU_CHECKED);
		config->set("ui_clipboard_mode",w->clipboard_mode);
		//I'm assuming this isn't a global state, but it's really hard to follow!!
		w->views.status._clipboard.indicate(w->clipboard_mode!=0);
		extern void animwin_enable_menu(int,int);
		animwin_enable_menu(m->inAnimationMode()?w->_anim_menu:-w->_anim_menu,w->clipboard_mode);		
		//animwin_enable_menu calls this immediately (guess best not to do twice?)
		//viewwin_status_func();		
		return;
	
	/*Help menu*/
	case id_help: 
	case id_about:
	case id_license:

		extern void aboutwin(int); 
		aboutwin(id); return;

	case id_unscale:
	{
		int n = 0;
		double lll[3] = {1,1,1};
		for(auto i:w->selection) 		
		if(i.type!=Model::PT_Vertex) 
		if(m->setPositionScale(i,lll)) n++;		
		if(n) m->operationComplete("Unscale");
		else model_status(m,StatusError,STATUSTIME_LONG,
		::tr("Select at least 1 joint, point, or projection to return scale to 1,1,1"));
		return;
	}}

	views.modelUpdatedEvent(); //HACK //???
}
void MainWin::_view(int i, void (ViewPanel::*mf)())
{
	//95% of this code deal with 2x modes.
	//It's a simple concept otherwise.
	bool two = 2==views.viewsN;
	if(i==_curr_view||two&&i==id_view_2) //Toggle?
	{
		glutext::glutMenuEnable(_prev_view,glutext::GLUT_MENU_CHECK);
		perform_menu_action(_prev_view);			
	}
	else
	{
		(views.*mf)();
		if(i==id_view_2)
		i = views.views1x2?id_view_1x2:id_view_2x1;		
		if(!two||2!=views.viewsN)
		_prev_view = _curr_view;
		_curr_view = i;
	}	
}

void MainWin::_sync_tools(int tt, int arg)
{
	if(_sync_tool==tt)
	if(tt!=Tool::TT_SelectTool||_sync_sel2!=-1)
	return;

	glutSetMenu(viewwin_toolbar);
	glutSetWindow(glut_window_id);

	if(tt==Tool::TT_NullTool)
	return viewwin_toolboxfunc(id_tool_none);

	int id = 0;
	Tool *tool = toolbox.getFirstTool();
	for(;tool;tool=toolbox.getNextTool())
	{
		if(tt==tool->m_tooltype)
		{
			int sel2 = _sync_sel2;
			{
				if(tt==Tool::TT_SelectTool)
				id+=sel2!=-1?sel2:arg;
				viewwin_toolboxfunc(id);
			}
			_sync_sel2 = sel2; return;
		}

		id+=tool->getToolCount();
	}
}
void viewwin_toolboxfunc(int id) //extern
{	
	int iid = id;

	MainWin *w = viewwin();
	bool def = id_tool_none==id;
	bool cmp = w->toolbox.getCurrentTool()->isNullTool();
	Tool *tool = nullptr; 

	int shift_lock = 0; //EXPERIMENTAL
	int shift_curr = w->views._bs_lock;
	if(cmp) w->_none_shift = shift_curr;
	if(def) shift_lock = w->_none_shift;
	else if(viewwin_shifthold)
	{
		//NOTE: The exception to manual shift locking
		//is the recall/toggle and the "default" tool.
		//The reason for treating it different is its
		//use of Shift is so different to be annoying.
		shift_lock = cmp?w->_curr_shift:shift_curr;
	}

	switch(id)
	{
	case id_tool_recall:

		shift_lock = w->_prev_shift;

		iid = id = w->_prev_tool; break;

	case id_tool_toggle:
	
		shift_lock = cmp?w->_curr_shift:w->_none_shift;

		//TRICKY: this is missed below, so it's cleaner
		//to do it here now than test for id_tool_toggle 
		if(!cmp) w->_curr_shift = shift_curr;

		def = !cmp;
		iid = id = cmp?w->_curr_tool:id_tool_none; 
		if(cmp) break;

	case id_tool_none:

		w->toolbox.setCurrentTool();
		tool = w->toolbox.getCurrentTool();
		w->views.timeline.redraw();
		id = 0; break;

	case id_tool_back:

		return w->views.back();

	case id_toolparams:
		extern void viewpanel_special_func(int,int,int);
		return viewpanel_special_func(GLUT_KEY_F2,0,0);
	case id_properties:
		return viewpanel_special_func(GLUT_KEY_F3,0,0);
	}
	if(!tool)
	{
		tool = w->toolbox.getFirstTool();
		for(;tool;tool=w->toolbox.getNextTool())
		{
			int iN = tool->getToolCount(); 
			if(id>=iN||tool->isSeparator())
			id-=iN; else break;
		}
	}
	if(!tool){ assert(0); return; } //???
			
	//REMOVE ME
	//w->toolbox.getCurrentTool()->deactivated();
	w->toolbox.setCurrentTool(tool);
	
	//tool->activated(id);
	w->views.setCurrentTool(tool,id,true); //NEW

	//2022: Make the texture and animation
	//windows match the tools to hopefully
	//reduce cognitive demand.
	w->_sync_tool = -1;
	w->_sync_sel2 = -1;
	switch(int tt=tool->m_tooltype)
	{
	case Tool::TT_SelectTool:

		//if(id!=2) break; //V? Vertices?
		if(id>=4) break; //Joints, etc?

		w->_sync_sel2 = id;

	case Tool::TT_MoveTool:
	case Tool::TT_RotateTool:
	case Tool::TT_ScaleTool:
	case Tool::TT_NullTool:

		w->_sync_tool = tt; 

		//NOTE: This will call glutSetMenu.
		if(auto*aw=w->_animation_win)
		if(!aw->hidden()) aw->_sync_tools(tt,id);		
		if(auto*tw=w->_texturecoord_win)
		if(!tw->hidden()) tw->_sync_tools(tt,id);
	}
	glutSetWindow(w->glut_window_id); //_sync_tools
	
	if(!def&&iid!=w->_curr_tool)
	{
		w->_prev_tool = w->_curr_tool;
		w->_curr_tool = iid;		

		w->_prev_shift = shift_curr;
	}
	if(viewwin_shifthold)
	{
		if(!def&&!cmp) w->_prev_shift = shift_curr;
	}
	w->_curr_shift = !def?shift_lock:shift_curr;
	if(w->views._bs_lock!=shift_lock)
	{
		w->views._bs_lock = shift_lock;
		w->views.status._shiftlock.indicate(shift_lock!=0);
	}

	//texturecoord.cc?
	//if(viewwin_toolbar!=glutGetMenu()) //Window menu?
	{
		//NOTE: wxWidets doesn't do this. I can't figure out
		//a sane way to do it without assuming that the menu
		//item IDs are the same for the toolbar's. Even then
		//it's hard to say items definitely belong to a menu.
		glutSetMenu(viewwin_toolbar);
		glutext::glutMenuEnable(iid,glutext::GLUT_MENU_CHECK);
	}

	bool empty = !w->views.params.nav.first_child();
	w->views.params.nav.set_hidden(empty||def);
	w->views.timeline.set_hidden(!def);

	int wx = glutGet(GLUT_WINDOW_X);
	int x = glutGet(glutext::GLUT_X)-wx;
	int div = w->views.shape[0]/7;
	enum{ l=Win::left,c=Win::center,r=Win::right };
	w->views.params.nav.align(x>div*2?x>div*5?r:c:l);

	//TESTING
	//May want to do regardless??
	//Trying to offer visual cues by hiding vertices?
	if(def!=cmp) glutPostWindowRedisplay(w->glut_window_id);
}

static void viewwin_geomenufunc(int id)
{
	viewin_menu raii;

	auto cmgr = CommandManager::getInstance();
	for(Command*cmd=cmgr->getFirstCommand();cmd;cmd=cmgr->getNextCommand())
	{
		int iN = cmd->getCommandCount();
		if(id<iN&&!cmd->isSeparator())
		{
			MainWin *w = viewwin();
			Model *model = w->model;

			//HACK: can this be automated?
			model->setPrimaryLayers(w->views.getPrimaryLayer()); //1 only?

			//hidecmd.cc needs this
			//if(viewwin_face_view)
			cmgr->setViewSurrogate(w->toolbox.getCurrentTool()); //2022
			if(cmd->activated(id,model))
			model->operationComplete(TRANSLATE("Command",cmd->getName(id)));
			cmgr->setViewSurrogate();
			return;
		}
		id-=iN;
	}
}

static void viewwin_mrumenufunc(int id)
{
	MainWin *w = viewwin();
	
	utf8 cmp = "script_mru";
	utf8 cfg = cmp+(id==100?0:7);
	if(id>=100) id-=100;
	
	std::string &mru = config->get(cfg);

	size_t pos = 0; while(id-->0)
	{
		pos = mru.find('\n',pos+1); 
	}
	if(~pos)
	{
		if(pos) pos++;
	}
	else{ assert(~pos); return; }
	
	size_t pos2 = mru.find('\n',pos);
	if(~pos2) mru[pos2] = '\0';

	char *str = const_cast<char*>(mru.c_str()+pos);

	#ifdef _WIN32
	for(char*p=str;*p;p++) if(*p=='\\') *p = '/';
	#endif

	//viewwin_mru consumes this string.
	std::string buf(str);

	if(cfg==cmp)
	{
		w->run_script(buf.c_str());
	}
	else MainWin::open(buf.c_str(),w);
}

extern int viewwin_tick(Win::si *c, int i, double &t, int e)
{
	int cm = glutGetModifiers();
	bool shift = false;

	MainWin *w; switch(c->id())
	{
	case id_bar:
	
		if(c->ui()->subpos())
		w = &((ViewBar*)c->ui())->model; 
		else
		w = &((AnimWin*)c->ui())->model; 
		break;

	case id_subitem:

		if(c->ui()->subpos())
		w = &((SideBar*)c->ui())->anim_panel.model; 
		else
		w = &((AnimWin*)c->ui())->model; 
		break;
	}

	Model *m = *w;
		
	if(e) //Not drawing?
	{
		//HACK: Constrain to integer time?
		shift = (cm&GLUT_ACTIVE_SHIFT)!=0;
		if(w->views._bs_lock&Tool::BS_Shift)
		shift = 1;
		if(!shift)
		{
			//bool snap = viewwin_framesnap!=0; 
			//inverting sense
			bool snap = !(int)w->views.status._keys_snap;
			if((cm&GLUT_ACTIVE_CTRL)!=0) 
			snap = !snap;
			if(!snap) return -1;
		}
	}

	if(!m->inAnimationMode()) return -1; //Drawing?

	int a = m->getCurrentAnimation();

	if(shift)
	{
		if(i==-1)
		{
			i = (int)t;
		}
		if(i<=(int)m->getAnimTimeFrame(a))
		{
			t = i;
		}
		else return -1; 
	}
	else
	{
		if(i==-1) i = m->getAnimFrame(a,t);

		int fc = m->getAnimFrameCount(a);

		if(i<fc) 
		{
			t = m->getAnimFrameTime(a,i);
		}
		else if(e)
		{
			//Note, bar::do_click probes +1.
			if(i!=fc)
			{
				t = 0; //New animation?
			}
		}
		else return -1; 
	}
	
	return i;
}

extern bool viewwin_confirm_close(int id, bool modal_undo)
{
	auto ui = Widgets95::e::find_ui_by_window_id(id);

	if(ui&&(!modal_undo||ui->modal()))
	for(auto*ea:viewwin_list)
	{
		if(ea->glut_window_id!=ui->glut_create_id())
		continue;

		if(modal_undo&&!ea->model->canUndoCurrent())
		return true;

		auto c = ui->main->find((int)id_ok);
		if(!c) return true;

		switch(Win::InfoBox(::tr("Keep changes?"),
		::tr("Model has been modified\n"
		"Do you want to keep the model as modified?"),
		id_yes|id_no|id_cancel,id_cancel))
		{
		case id_cancel: return false;
		case id_yes: ui->active_callback(c); break;
		}
		return true;
	}
	return true;
}
