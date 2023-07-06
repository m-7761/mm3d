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

#include "texturecoord.h"

#include "viewwin.h"
#include "log.h"

enum
{
	//REMINDER: These names go in keycfg.ini!

	id_uv_view_init=id_view_init,
	
	//Fit
	//2022: I need a fast solution for mapping
	//PlayStation textures to a unit dimension.
	id_uv_fit_0=0x100000, //0
	id_uv_fit_1=0x100001, //1
	id_uv_fit_2=0x100002, //2
	id_uv_fit_4=0x100004, //3
	id_uv_fit_8=0x100008, //4
	id_uv_fit_16=0x100010, //5
	id_uv_fit_32=0x100020, //6
	id_uv_fit_64=0x100040, //7
	id_uv_fit_128=0x100080, //8
	id_uv_fit_256=0x100100, //9
	id_uv_fit_2_x=0x120000,
	id_uv_fit_u, id_uv_fit_v,
	id_uv_fit_uv=id_uv_fit_u|id_uv_fit_v, //1|2

	//Tool
	id_uv_tool_select=TextureWidget::MouseSelect,
	id_uv_tool_move=TextureWidget::MouseMove,
	id_uv_tool_rotate=TextureWidget::MouseRotate,
	id_uv_tool_scale=TextureWidget::MouseScale,
	//these should start at 0, however that
	//clashes with the
	//above enum values
	id_uv_model=1000, //ARBITRARY

	id_uv_cmd_select_all=2000,
	id_uv_cmd_select_faces_incl,
	id_uv_cmd_select_faces_excl,
	id_uv_cmd_flip_u,
	id_uv_cmd_flip_v,
	id_uv_cmd_flip_uv,
	id_uv_cmd_snap_u,
	id_uv_cmd_snap_v,
	id_uv_cmd_snap_uv,
	id_uv_cmd_turn_ccw,
	id_uv_cmd_turn_cw,
	id_uv_cmd_turn_180,

	id_uv_tools_f1,
	id_uv_tools_f2,
	id_uv_tools_f3,
};
extern int viewwin_shifthold;
static int texturecoord_fit_uv = 1|2;
extern void win_help(const char*,Win::ui*);
void TextureCoordWin::_menubarfunc(int id)
{
	TextureCoordWin *tw;
	MainWin* &viewwin(int=glutGetWindow());
	if(auto*w=viewwin())
	tw = w->_texturecoord_win;
	else{ assert(0); return; } assert(tw);

	if(id>=id_uv_fit_0&&id<=id_uv_fit_2_x)
	{
		int smask = texturecoord_fit_uv&1;
		int tmask = texturecoord_fit_uv&2;

		double s,t;
		if(id==id_uv_fit_1||id==id_uv_fit_0)
		{
			double ss = DBL_MAX, tt = ss;
			s = t = -DBL_MIN;
			for(auto&ea:tw->texture.m_vertices)
			if(ea.selected)
			{
				s = std::max(s,ea.s);
				t = std::max(t,ea.t);
				ss = std::min(ss,ea.s);
				tt = std::min(tt,ea.t);
			}
			if(id==id_uv_fit_0) //modulo
			{
				ss = (int)ss;
				tt = (int)tt;
				for(auto&ea:tw->texture.m_vertices)
				if(ea.selected)
				{
					if(smask) ea.s-=ss; 
					if(tmask) ea.t-=tt;		
				}
			}
			else if(id==id_uv_fit_1) //unscale
			{
				auto cmp = 1/(std::max(s-ss,t-tt));
				if(fabs(cmp-1)<0.0000001)				
				{
					//This might be too esoteric if the
					//the origin isn't at 0,0, but what
					//the idea is to make two inputs to
					//stretch the texture out the image
					//in both dimensions. What's a good
					//word for that?
					s = 1/(s-ss); t = 1/(t-tt);
				}
				else s = t = cmp;
				for(auto&ea:tw->texture.m_vertices)
				if(ea.selected)
				{
					//NOTE: Basing off ss/tt to prevent
					//positive scaling from getting out
					//of hand.
					if(smask) ea.s = s*(ea.s-ss)+ss;
					if(tmask) ea.t = t*(ea.t-tt)+tt;
				}
			}
			else assert(0); goto done;
		}
		else if(id==id_uv_fit_2_x)
		{
			s = t = 2;
		}
		else s = t = 1.0/(id&0x1FF);
		for(auto&ea:tw->texture.m_vertices)
		if(ea.selected)
		{
			if(smask) ea.s*=s;
			if(tmask) ea.t*=t;		
		}
	done:tw->setTextureCoordsDone();
	}
	else switch(id) //Tool?
	{
	case id_help:

		//tw->f1.execute_callback();
		win_help(typeid(TextureCoordWin).name(),tw);
		break;

	case id_uv_editor:

		tw->submit(tw->close); //HACK
		break;

	case id_uv_tools_f1:
	case id_uv_tools_f2:
	case id_uv_tools_f3:

		tw->toggle_toolbar(id-id_uv_tools_f1+1);
		break;

	case id_edit_undo:
	case id_edit_redo:

		tw->model.perform_menu_action(id);
		break;

	case id_tool_toggle: //DICEY
	case id_tool_none:

		id+=id_uv_model; //break; //Ready for subtraction below.

	case id_uv_model+0: //tool_select_connected?
	case id_uv_model+1: //tool_select_faces?
	case id_uv_model+3: //tool_select_groups?	

		glutSetWindow(tw->model.glut_window_id);				
		extern void viewwin_toolboxfunc(int);
		viewwin_toolboxfunc(id-id_uv_model);
		break;

	case id_snap_grid: case id_snap_vert:	
	{
		bool x = 0!=glutGet(glutext::GLUT_MENU_CHECKED);

		config->set(id==id_snap_grid?"uv_snap_grid":"uv_snap_vertex",x);
		
		Model::ViewportUnits &vu = tw->model->getViewportUnits();

		int y = id==id_snap_grid?vu.SubpixelSnap:vu.UvSnap;

		if(x) vu.snap|=y; else vu.snap&=~y; 

		if(id==id_snap_vert) tw->model.views.status._uv_snap.indicate(x);
		if(id==id_snap_grid) tw->model.views.status._sp_snap.indicate(x);

		tw->texture.setUvSnap(0!=(vu.snap&(vu.SubpixelSnap|vu.UvSnap)));
		break;	
	}
	case id_view_settings:		
		
		extern void viewportsettings(MainWin &model);
		viewportsettings(tw->model); 
		break;

	case id_uv_tool_select: 
	
		tw->model._sync_sel2 = -1; //break; //HACK

	case id_uv_tool_move: 
	case id_uv_tool_rotate: case id_uv_tool_scale:

		tw->submit(tw->mouse.select_id(id)); break;

	case id_uv_view_init:

		tw->texture.keyPressEvent(Tool::BS_Left|Tool::BS_Special,0,0,0); //Home
		break;

	case id_tool_recall: //CLEAN ME UP

		tw->recall_lock[1] = tw->texture._tool_bs_lock;

		std::swap(tw->recall_tool[0],tw->recall_tool[1]);
		std::swap(tw->recall_lock[0],tw->recall_lock[1]);

		tw->mouse.select_id(tw->recall_tool[1]);

		tw->toggle_tool(false);
		
		break;

	case id_uv_fit_u:
	case id_uv_fit_v:
	case id_uv_fit_uv: //u|v
	
		id&=3;
		if(id!=3&&id==texturecoord_fit_uv)
		{
			id = 3;
			glutext::glutMenuEnable(id_uv_fit_uv,glutext::GLUT_MENU_CHECK);
		}
		texturecoord_fit_uv = id;
		extern std::vector<MainWin*> viewwin_list;
		for(auto&ea:viewwin_list)
		{
			ea->views.status._uvfitlock.name(id==1?"UFw":"UFh");
			ea->views.status._uvfitlock.indicate(id!=3);
		}
		break;
	
	case id_uv_cmd_select_all:

		tw->select_all(); break;

	case id_uv_cmd_select_faces_incl:
	case id_uv_cmd_select_faces_excl:

		tw->select_faces(id==id_uv_cmd_select_faces_excl); 
		break;

	case id_uv_cmd_flip_u:
	case id_uv_cmd_flip_v:
	case id_uv_cmd_flip_uv:

		id-=id_uv_cmd_flip_u;
		if(id!=1) tw->texture.uFlipCoordinates();
		if(id!=0) tw->texture.vFlipCoordinates();
		tw->setTextureCoordsDone(); 
		break;

	case id_uv_cmd_snap_u:
	case id_uv_cmd_snap_v:
	case id_uv_cmd_snap_uv:

		id-=id_uv_cmd_snap_u;
		if(id!=1) tw->texture.uFlattenCoordinates();
		if(id!=0) tw->texture.vFlattenCoordinates();
		tw->setTextureCoordsDone(); 
		break;

	case id_uv_cmd_turn_ccw:
	case id_uv_cmd_turn_cw:
	case id_uv_cmd_turn_180:

		id-=id_uv_cmd_turn_ccw;
		if(id!=1) tw->texture.rotateCoordinatesCcw();
		if(id==2) tw->texture.rotateCoordinatesCcw();
		if(id==1) tw->texture.rotateCoordinatesCw();			
		tw->setTextureCoordsDone(); 
		break;

	}
}
bool TextureCoordWin::select_all()
{
	if(texture.m_vertices.empty())
	return false;
	bool sel = true;
	for(auto&ea:texture.m_vertices) //invert?
	if(ea.selected){ sel = false; break; }
	for(auto&ea:texture.m_vertices)
	ea.selected = sel;
	updateSelectionDone(); return true;
}
bool TextureCoordWin::select_faces(bool excl)
{
	int_list l;	
	int v = 0; for(auto i:trilist)
	{
		int sel = 0;
		for(int j=0;j<3;j++,v++)
		if(texture.m_vertices[v].selected)
		sel|=1<<j;
		if(excl?sel==7:sel) l.push_back(i);
	}
	if(l!=trilist)
	{
		model->setSelectedTriangles(l);
		model->operationComplete("UVs to Faces");		
	}
	else return false; return true;
}
void TextureCoordWin::toggle_toolbar(int f)
{
	control *c; switch(f)
	{
	default: assert(0); return;
	case 3: c = &toolbar3;break; 
	case 2: c = &toolbar2; break;	
	case 1: c = &f1; break;
	}
	bool x = !c->hidden();
	c->set_hidden(x); 
	if(f==1) close.set_hidden(x);
	int bts = (int)toolbar3.hidden()<<2|(int)toolbar2.hidden()<<1;
	map_remap.set_hidden(bts!=0);
	bts|=(int)f1.hidden();		 	
	config->set("uv_buttons_mask",bts);	

	if(f==1) close.id(bts&1?id_ok:id_close); //Allow Esc for None tool??

	if(seen()) //DICEY
	{
		int dy = _main_panel._h;
		scene.lock(scene._w,scene._h);
		_main_panel.lock(0,false);
		_main_panel.pack();
		_main_panel.lock(0,true);
		scene.lock(scene._w,false);		
		dy = _main_panel._h-dy;
		_reshape[1]+=dy; _reshape[3]+=dy;	
	}
}
void TextureCoordWin::toggle_tool(bool unlock)
{
	texture.setMouseOperation
	((texture::MouseE)recall_tool[1]);

	if(unlock)
	recall_lock[1] = viewwin_shifthold?texture._tool_bs_lock:0;

	if(texture._tool_bs_lock!=recall_lock[1])
	{
		texture._tool_bs_lock = recall_lock[1];
		model.views.status._texshlock.indicate(recall_lock[1]!=0);
	}

	int tt; switch(mouse)
	{	
	case texture::MouseNone:
		default: assert(0); tt = Tool::TT_NullTool; break;
	case texture::MouseMove: tt = Tool::TT_MoveTool; break;
	case texture::MouseSelect: tt = Tool::TT_SelectTool; break;
	case texture::MouseScale: tt = Tool::TT_ScaleTool; break;
	case texture::MouseRotate: tt = Tool::TT_RotateTool; break;		
	}

	model._sync_tools(tt,2);

	texture.updateWidget();	
}
void TextureCoordWin::_init_menu_toolbar()
{	
	std::string o; //radio	
	#define E(id,...) viewwin_menu_entry(o,#id,__VA_ARGS__),id_##id
	#define O(on,id,...) viewwin_menu_radio(o,on,#id,__VA_ARGS__),id_##id
	#define X(on,id,...) viewwin_menu_check(o,on,#id,__VA_ARGS__),id_##id
	extern utf8 viewwin_menu_entry(std::string &s, utf8 key, utf8 n, utf8 t="", utf8 def="", bool clr=true);
	extern utf8 viewwin_menu_radio(std::string &o, bool O, utf8 key, utf8 n, utf8 t="", utf8 def="");
	extern utf8 viewwin_menu_check(std::string &o, bool X, utf8 key, utf8 n, utf8 t="", utf8 def="");

	static int fit_menu=0; if(!fit_menu)
	{
		int sm = glutCreateMenu(_menubarfunc);		
		glutAddMenuEntry(O(false,uv_fit_u,"Fit Width","","Ctrl+W"));
		glutAddMenuEntry(O(false,uv_fit_v,"Fit Height","","Ctrl+H"));		
		glutAddMenuEntry(O(true,uv_fit_uv,"Regular Fit","",""));
		fit_menu = glutCreateMenu(_menubarfunc);
		glutAddSubMenu(::tr("Confine",""),sm);
		glutAddMenuEntry();
		glutAddMenuEntry(E(uv_fit_0,"Modulo","","0"));
		glutAddMenuEntry(E(uv_fit_1,"Unscale","","1"));
		glutAddMenuEntry(E(uv_fit_2_x,"Upscale","","2"));
		glutAddMenuEntry(E(uv_fit_2,"Scale 1/2","","Ctrl+2"));
		//REMOVE US?
		//I don't know how useful this is. Even PlayStation
		//uses 255 instead of 256, so these are more or less
		//off by 1.
		glutAddMenuEntry(E(uv_fit_4,"Scale 1/4","","Ctrl+3"));
		glutAddMenuEntry(E(uv_fit_8,"Scale 1/8","","Ctrl+4"));
		glutAddMenuEntry(E(uv_fit_16,"Scale 1/16","","Ctrl+5"));
		glutAddMenuEntry(E(uv_fit_32,"Scale 1/32","","Ctrl+6"));
		glutAddMenuEntry(E(uv_fit_64,"Scale 1/64","","Ctrl+7"));
		glutAddMenuEntry(E(uv_fit_128,"Scale 1/128","","Ctrl+8"));
		glutAddMenuEntry(E(uv_fit_256,"Scale 1/256","","Ctrl+9"));
		
	}
	static int conf_menu=0; if(!conf_menu)
	{
		conf_menu = glutCreateMenu(_menubarfunc);
		//NOTE: viewwini.cc uses "Restore to Normal" but it
		//doesn't use Home (Home comes from Misfit Model 3D.)
		glutAddMenuEntry(E(uv_view_init,"Reset","","Home"));
	}

	_viewmenu = glutCreateMenu(_menubarfunc);
	glutAddSubMenu("Reconfigure",conf_menu);
	glutAddMenuEntry();
	bool r = config->get("uv_snap_vertex",false);
	bool s = config->get("uv_snap_grid",false);
	glutAddMenuEntry(X(r,snap_vert,"Snap to Vertex","View|Snap to Vertex","Shift+V"));
	glutAddMenuEntry(X(s,snap_grid,"Snap to Grid","View|Snap to Grid","Shift+G"));	
	texture.setUvSnap(r||s);
	model.views.status._uv_snap.indicate(r);
	model.views.status._sp_snap.indicate(s);
	glutAddMenuEntry(E(view_settings,"Grid Settings...","View|Grid Settings","Shift+Ctrl+G"));

	static int tool_menu=0; if(!tool_menu) //2022
	{
		tool_menu = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_vertices","Select","Tool","V"),id_uv_tool_select);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_move","Move","Tool","T"),id_uv_tool_move);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_rotate","Rotate","Tool","R"),id_uv_tool_rotate);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_scale","Scale","Tool","S"),id_uv_tool_scale);
		glutAddMenuEntry();
		//extern int viewwin_shlk_menu; //Not sure GTK will like this!
		//glutAddSubMenu(::tr("Shift Lock",""),viewwin_shlk_menu);
		{
			extern void viewwin_menubarfunc(int);

			//DUPLICATE (viewin.cc)
			int shlk = glutCreateMenu(viewwin_menubarfunc);
		//	r = config->get("ui_tool_shift_hold",false);
		//	if(r) model.views.status._shifthold.indicate(true);
			bool r = model.views.status._shifthold;
			glutAddMenuEntry(E(tool_shift_lock,"Sticky Shift Key (Emulation)","","L"));
			glutAddMenuEntry(X(!r,tool_shift_hold,"Unlock when Selecting Tool","","Shift+L"));
			glutSetMenu(tool_menu);
			glutAddSubMenu(::tr("Shift Lock",""),shlk);
		}		
		glutAddMenuEntry();
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_recall","Switch Tool","","Space"),id_tool_recall);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_none","None","Tool","Esc"),id_tool_none);
	}
	static int model_menu=0; if(!model_menu)
	{
		model_menu = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(viewwin_menu_entry(o,"edit_undo","Undo","Undo shortcut","Ctrl+Z"),id_edit_undo);
		glutAddMenuEntry(viewwin_menu_entry(o,"edit_redo","Redo","Redo shortcut","Ctrl+Y"),id_edit_redo);
		glutAddMenuEntry();
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_connected","Select Connected","Tool","C"),id_uv_model+0);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_faces","Select Faces","Tool","F"),id_uv_model+1);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_groups","Select Groups","Tool","G"),id_uv_model+3);
		glutAddMenuEntry();
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_toggle","Toggle Tool","","Tab"),id_tool_toggle);
	}
	static int cmd_menu=0; if(!cmd_menu)
	{
		int edit = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(viewwin_menu_entry(o,"cmd_select_all","Select All UVs","Command","Ctrl+A"),id_uv_cmd_select_all+0);
		int faces = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(E(uv_cmd_select_faces_incl,"Select Faces from UVs","","Ctrl+F"));
		glutAddMenuEntry(E(uv_cmd_select_faces_excl,"Select Inscribed Faces","","Shift+Ctrl+F"));		
		//Note, trying to match flipcmd.cc and flattencmd.cc
		//Snap is just shorter than "Flatten" in the button layout.
		int snap = glutCreateMenu(_menubarfunc);		
		glutAddMenuEntry(E(uv_cmd_snap_u,"Snap on U","","F5"));
		glutAddMenuEntry(E(uv_cmd_snap_v,"Snap on V","","F6"));
		glutAddMenuEntry(E(uv_cmd_snap_uv,"Snap Both","","F7"));
		int flip = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(E(uv_cmd_flip_u,"Flip on U","","Alt+F5"));
		glutAddMenuEntry(E(uv_cmd_flip_v,"Flip on V","","Alt+F6"));
		glutAddMenuEntry(E(uv_cmd_flip_uv,"Flip Both","","Alt+F7"));		
		int turn = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(E(uv_cmd_turn_ccw,"Turn CCW","","F8"));
		glutAddMenuEntry(E(uv_cmd_turn_cw,"Turn CW","","F9"));
		glutAddMenuEntry(E(uv_cmd_turn_180,"Turn 180","","F10"));

		cmd_menu = glutCreateMenu(_menubarfunc);
		glutAddSubMenu(TRANSLATE_NOOP("Command","Edit"),edit);
		glutAddMenuEntry();
		glutAddSubMenu(TRANSLATE_NOOP("Command","Faces"),faces);
		glutAddMenuEntry();
		glutAddSubMenu(TRANSLATE_NOOP("Command","Snap"),snap);
		glutAddSubMenu(TRANSLATE_NOOP("Command","Flip"),flip);		
		glutAddSubMenu(TRANSLATE_NOOP("Command","Turn"),turn);
	}
	static int f1_menu=0; if(!f1_menu)
	{
		int buttons = glutCreateMenu(_menubarfunc);		
		glutAddMenuEntry(E(uv_tools_f3,"Toggle Row 3","","Shift+F3"));		
		glutAddMenuEntry(E(uv_tools_f2,"Toggle Row 2","","Shift+F2"));
		glutAddMenuEntry(E(uv_tools_f1,"Toggle Row 1","","Shift+F1"));
		f1_menu = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(E(help,"Contents","Help|Contents","F1"));
		glutAddMenuEntry();
		glutAddSubMenu("Buttons",buttons);
		glutAddMenuEntry();
		glutAddMenuEntry(E(uv_editor,"Close","","M"));
	}
	int bts = config->get("uv_buttons_mask",0);
	if(bts&1) toggle_toolbar(1);
	if(bts&2) toggle_toolbar(2);
	if(bts&4) toggle_toolbar(3);

	_menubar = glutCreateMenu(_menubarfunc);
	glutAddSubMenu(::tr("Fit","menu bar"),fit_menu);
	glutAddSubMenu(::tr("View","menu bar"),_viewmenu);
	glutAddSubMenu(::tr("Tools","menu bar"),tool_menu);
	glutAddSubMenu(::tr("Model","menu bar"),model_menu);
	glutAddSubMenu(::tr("Geometry","menu bar"),cmd_menu);
	glutAddSubMenu(::tr("Help","menu bar"),f1_menu);
	glutAttachMenu(glutext::GLUT_NON_BUTTON);

	#undef E //viewwin_menu_entry
	#undef O //viewwin_menu_radio
	#undef X //viewwin_menu_check
}
TextureCoordWin::~TextureCoordWin()
{
	for(int*m=&_menubar;m<=&_viewmenu;m++)
	glutDestroyMenu(_menubar);
}

struct MapDirectionWin : Win
{
	static utf8 dir_str(unsigned dir) //MAGIC NUMBER
	{
		const char *dirs[] =
		{"Front","Back","Left","Right","Top","Bottom"};
		assert(dir<6);
		return dirs[dir];
	}

	MapDirectionWin(int *lv, bool *grp)
		:
	Win("Set new texture coordinates from which direction?"),
	dir(main,"",lv),	
	group(main,"Group",grp),
	ok_cancel(main)
	{
		main_panel()->row_pack().cspace<top>();

		for(int i=0;i<6;i++) dir.add_item(dir_str(i));

		//2022: This isn't necessary, but since the auto selection
		//system is pretty good, it's suggestive that the selection
		//is made for the user... what wouldn't hurt though is more
		//options to choose from the view ports.
		if(*lv!=-1) ok_cancel.ok.activate();
	}

	multiple dir;
	boolean group;
	ok_cancel_panel ok_cancel;	
};

void TextureCoordWin::open()
{
	setModel();
	
	// If visible, setModel already did this
	if(hidden()||!seen())
	{
		int tt = model._sync_tool;
		if(tt==-1) tt = Tool::TT_SelectTool;
		_sync_tools(model._sync_tool,model._sync_sel2);
	
		//REMINDER: GLX needs to show before
		//it can use OpenGL.
		show();
		
		if(trilist.empty()) openModel();
	}
	toggle_indicators(true); //2022
}
void TextureCoordWin::toggle_indicators(bool how) 
{
	//The goal here is just to remove clutter from the main
	//window's status bar when not in use. I think probably
	//TextureCoordWin should get its own statusbar later on.
	auto &st = model.views.status;
	auto &vp = model->getViewportUnits();
	st._texshlock.indicate(how?texture._tool_bs_lock!=0:false);
	st._uvfitlock.indicate(how?texturecoord_fit_uv!=3:false);
	st._uv_snap.indicate(how?0!=(vp.snap&vp.UvSnap):false);
	st._sp_snap.indicate(how?0!=(vp.snap&vp.SubpixelSnap):false);
}
void TextureCoordWin::setModel()
{
	//Note, setModel clears the texture and coords.
	texture.setModel(model); trilist.clear();

	if(!hidden()) openModel();
}
void TextureCoordWin::modelChanged(int changeBits)
{
	if(m_ignoreChange) return;

	if(hidden()) //Invalidate at first opportunity?
	{
		if(changeBits&Model::SelectionFaces)
		{
			trilist.clear();
			texture.clearCoordinates();
		}
		if(changeBits&(Model::SetTexture|Model::SetGroup))
		{
			texture.setTexture(-1);
		}
		return;
	}

	//Note, this is mainly to avoid calling
	//glutSetWindow/updateWidget many times.
	int clr = changeBits&Model::SelectionFaces;
	int grp = changeBits&Model::SetGroup;
	int set = changeBits&Model::SetTexture;
	int mov = changeBits&Model::MoveTexture;
	int sel = changeBits&Model::SelectionUv;
	int upd = set|mov|sel; if(clr|=grp)
	{
		//TODO: Is it desirable to retain the
		//UV selection while selecting faces?
		//Would the new UVs be selected then?
	}
	else
	{
		if(sel) texture.restoreSelectedUv();
		if(mov)
		{
			int v = 0; for(auto i:trilist)
			{
				float s,t; for(int j=0;j<3;j++)
				{
					model->getTextureCoords(i,j,s,t);
					texture.m_vertices[v].s = s;
					texture.m_vertices[v].t = t; v++;
				}
			}
		}
	}
	
	if(clr) 
	{
		int cmp = texture.m_materialId;
		openModel();
		if(upd=set&&cmp==texture.m_materialId)
		texture.setTexture(texture.m_materialId);
	}
	else
	{
		if(upd) glutSetWindow(glut_window_id());
		if(set) texture.setTexture(texture.m_materialId);
	}	
	if(upd) texture.updateWidget();
}
void TextureCoordWin::openModel()
{
	  ////POINT-OF-NO-RETURN////
	
	assert(!hidden());

	//REMINDER: saveSelectedUv/restoreSelectedUv
	//must be called so that they don't disagree.

	glutSetWindow(glut_window_id());

	model->getSelectedTriangles(trilist);
	int m = texture.m_materialId; for(int i:trilist)
	{
		m = model->getGroupTextureId(model->getTriangleGroup(i));
		if(m>=0) break;
	}
	if(m!=texture.m_materialId) texture.setTexture(m);
		
	//openModelCoordinates();
	{	
		texture.clearCoordinates();
		{
			int v = 0; float s,t; for(auto i:trilist)	
			{	
				for(int j=0;j<3;j++)
				{
					model->getTextureCoords(i,j,s,t);
					texture.addVertex(s,t);
				}		
				texture.addTriangle(v,v+1,v+2); v+=3;
			}
		}		
	}

	//NEW: Call operationComplete so setSelectedUv isn't open-ended
	//and do everything else that was once done here as side-effect.
	updateSelectionDone();
}

static void texturecoord_reshape(Win::ui *ui, int x, int y)
{
	assert(!ui->hidden()); //DEBUGGING

	auto *w = (TextureCoordWin*)ui;
	auto &r = w->_reshape;

	if(!ui->seen())
	{
		r[0] = x-w->scene.span(); 
		r[1] = y-w->scene.drop();
		r[2] = x;
		r[3] = y; return;
	}

	if(x<r[2]) x+=r[2]-x; 
	if(y<r[3]) y+=r[3]-y; 

	//NOTE: Can't really lock scene.y because of
	//the option trays on the bottom make it very
	//complicated/runaway.
	w->scene.lock(x-r[0],0);
	w->_main_panel.lock(0,y);
}

void TextureCoordWin::_sync_tools(int tt, int arg)
{
	int t = (int)mouse; switch(t)
	{
	case texture::MouseNone: t = Tool::TT_NullTool; break;
	case texture::MouseMove: t = Tool::TT_MoveTool; break;
	case texture::MouseSelect: t = Tool::TT_SelectTool; break;
	case texture::MouseScale: t = Tool::TT_ScaleTool; break;
	case texture::MouseRotate: t = Tool::TT_RotateTool; break;
	default: return;
	}
	if(t==tt) return;

	switch(tt)
	{
	case Tool::TT_NullTool:  t = texture::MouseNone; break;
	case Tool::TT_SelectTool: t = texture::MouseSelect; break;
	case Tool::TT_MoveTool: t = texture::MouseMove; break;
	case Tool::TT_RotateTool: t = texture::MouseRotate; break;
	case Tool::TT_ScaleTool: t = texture::MouseScale; break;
	}

	if(mouse.select_id(t)) submit(mouse);
}
void TextureCoordWin::init()
{
	toolbar3.calign();
	toolbar2.calign();
	reshape_callback = texturecoord_reshape; 

	//This is used to let Tab hotkeys go to menus.
	//It depends on Win::Widget activating itself.
	scene.ctrl_tab_navigate();

	close.ralign();

	_init_menu_toolbar(); //_menubar //_viewmenu

	m_ignoreChange = false; 

	for(int i=2;i-->0;recall_lock[i]=0) recall_tool[i] = !i+1;

	dimensions.disable();

	white.add_item(0,"Black")
    .add_item(0x0000ff,"Blue")
    .add_item(0x00ff00,"Green")
    .add_item(0x00ffff,"Cyan")
    .add_item(0xff0000,"Red") //id_red
    .add_item(0xff00ff,"Magenta")
    .add_item(0xffff00,"Yellow")
    .add_item(id_white,"White"); //0xffffff
	red.reference(white);
	int w = config->get("ui_texcoord_lines_color",+id_white);
	int r = config->get("ui_texcoord_selection_color",+id_red);
	if(w&&w<8) w = id_white; if(r&&r<8) r = id_red; //back-compat
	white.set_int_val(w); red.set_int_val(r);

	// TODO handle undo of select/unselect?
	// Can't do this until after constructor is done because of observer interface
	//setModel(model);
	texture.setModel(model);
	texture.setInteractive(true);
	texture.setDrawUvBorder(true); 

	mouse.select_id(texture.m_operation)
	.add_item(texture::MouseSelect,"Select")
	.add_item(texture::MouseMove,"Move")
	.add_item(texture::MouseRotate,"Rotate")
	.add_item(texture::MouseScale,"Scale");	
		
	scale_sfc.set(config->get("ui_texcoord_scale_center",true));
	scale_kar.set(config->get("ui_texcoord_scale_aspect",false));

	map.add_item(3,"Triangle");
	map.add_item(4,"Quad");	
	map.add_item(0,"View"); //"Group"	
	map_keep.set_parent(map.inl);
	map_group.set_parent(map);
	map.cspace<top>().space(0);
}
void TextureCoordWin::submit(control *c)
{
	int id = c->id(); switch(id)
	{
	case id_init: 
	
		//HACK: one pixel increment
		u.spinner.set_speed(); 
		v.spinner.set_speed();
		init2:
		texture.setLinesColor((int)white);
		texture.setSelectionColor((int)red);
		texture.setScaleFromCenter(scale_sfc);
		texture.setScaleKeepAspect(scale_kar);
		break;

	case id_item: 

		assert((int)mouse);

		if(recall_tool[1]!=(int)mouse)
		{
			recall_lock[0] = recall_lock[1];
			recall_tool[0] = recall_tool[1];
			recall_tool[1] = mouse;
		}

		toggle_tool(true);

		break;

	case id_reset:

		map_keep.set_hidden();
		map_group.set_hidden();
		current_direction = -1;
		map.find(0)->set_name("View"); 		
		break;

	case id_subitem:
	case id_subitem+1:

		mapReset(id); break;

	case id_white: 

		config->set("ui_texcoord_lines_color",(int)white);
	    goto init2;

	case id_red:

		config->set("ui_texcoord_selection_color",(int)red);
	    goto init2;

	case '+': texture.zoomIn(); break;
	case '-': texture.zoomOut(); break;
	case '=': texture.setZoomLevel(zoom.value); break;

	default:

		if(c>=ccw&&c<=vflip)
		{
			if(c==ccw) texture.rotateCoordinatesCcw();
			if(c==cw) texture.rotateCoordinatesCw();
			if(c==uflip) texture.uFlipCoordinates();
			if(c==vflip) texture.vFlipCoordinates();

			setTextureCoordsDone();	
		}
		if(c>=l80&&c<=snap)
		{
			if(c==l80) texture.rotateCoordinatesCcw();
			if(c==l80) texture.rotateCoordinatesCcw();
			if(c==snap) texture.uFlattenCoordinates();
			if(c==snap) texture.vFlattenCoordinates();
			if(c==usnap) texture.uFlattenCoordinates();
			if(c==vsnap) texture.vFlattenCoordinates();

			setTextureCoordsDone();	
		}
		else if(c==scale_sfc)
		{
			config->set("ui_texcoord_scale_center",(bool)scale_sfc);
			goto init2;
		}
		else if(c==scale_kar)
		{
			config->set("ui_texcoord_scale_aspect",(bool)scale_kar);
			goto init2;
		}
		break;

	case id_scene:

		texture.draw(scene.x(),scene.y(),scene.width(),scene.height());
		break;

	case 'U': case 'V':
	
		texture.move(u.float_val()-centerpoint[0],v.float_val()-centerpoint[1]); 
		setTextureCoordsDone();
		break;
	
		//HACK: This prevents pressing Esc so
		//the None tool can use it by default.
		//Note: id_close is preventing Return.
	case id_ok:
	case id_close:

		toggle_indicators(false); //2022

		return hide();
	}

	basic_submit(id);
}

//Transplanting this Model API to here.
//bool Model::getVertexCoords2d(unsigned vertexNumber,ProjectionDirectionE dir, double *coord)const
static void texturecoord_2d(Model *m, int v, int dir, double coord[2])
{
	//if(coord&&vertexNumber>=0&&(unsigned)vertexNumber<m_vertices.size())
	{
		//Mode::Vertex *vert = m_vertices[vertexNumber];

		double c3d[3]; m->getVertexCoords(v,c3d);
		switch(dir)
		{		
		case 0: //case ViewFront:
			coord[0] =  c3d[0]; coord[1] =  c3d[1];			
			break;
		case 1: //case ViewBack:
			coord[0] = -c3d[0]; coord[1] =  c3d[1];
			break;
		case 2: //case ViewLeft:
			coord[0] = -c3d[2]; coord[1] =  c3d[1];
			break;
		case 3: //case ViewRight:
			coord[0] =  c3d[2]; coord[1] =  c3d[1];
			break;
		case 4: //case ViewTop:
			coord[0] =  c3d[0]; coord[1] = -c3d[2];
			break;
		case 5: //case ViewBottom:
			coord[0] =  c3d[0]; coord[1] =  c3d[2];
			break;
		}
	}
}

void TextureCoordWin::mapReset(int id)
{
	double min = 0, max = 1; //goto

	if(map&&id!=id_subitem+1)
	{
		texture.clearCoordinates();

		texture.addVertex(min,max);
	
		if(3==map.int_val())
		{
		texture.addVertex(min,min);
		texture.addVertex(max,min);
		texture.addTriangle(0,1,2);
		}
		else //4
		{
		texture.addVertex(max,max);
		texture.addVertex(min,min);
		texture.addVertex(max,min);
		texture.addTriangle(3,1,0);
		texture.addTriangle(0,2,3);
		}

	done: setTextureCoordsDone();

		return texture.updateWidget();
	}
		
	if(trilist.empty()) //???
	return log_error("no triangles selected\n"); //???

	int direction = -1;
	if(!map_remap.hidden()) //2022
	{
		direction = current_direction;
	}
	if(direction==-1||id==id_subitem+1)
	{
		double normal[3],total[3] = {};
		for(int ea:trilist)
		for(int i=0;i<3;i++)
		{
			model->getNormal(ea,i,normal);
			for(int i=0;i<3;i++)
			total[i]+=normal[i];
		}

		int index = 0;
		for(int i=1;i<3;i++)		
		if(fabs(total[i])>fabs(total[index]))
		index = i;
		switch(index)
		{
		case 0: direction = total[index]>=0.0?3:2; break;
		case 1: direction = total[index]>=0.0?4:5; break;
		case 2: direction = total[index]>=0.0?0:1; break;
		default:direction = -1;
		//	log_error("bad normal index: %d\n",index); //???
		}
	
		bool group = true;
		if(id==id_subitem+1||current_direction!=direction)
		if(id_ok!=MapDirectionWin(&direction,&group).return_on_close())
		return;

		if(current_direction!=direction) //2022
		{
			current_direction = direction;

			map.find(0)->name() = MapDirectionWin::dir_str(direction);

			map_keep.set(); map_keep.set_hidden(false);
		}
		map_group.set(group); map_group.set_hidden(false);

		if(id==id_subitem+1) //Choosing?
		{
			map.select_id(0); return; 
		}
	}

	//log_debug("mapGroup(%d)\n",direction); //???

	texture.clearCoordinates();

	double coord[2];
	double xMin = +DBL_MAX, x = -DBL_MAX;
	double yMin = +DBL_MAX, y = -DBL_MAX;
	for(int ea:trilist) for(int i=0;i<3;i++)
	{
		int v = model->getTriangleVertex(ea,i);
		texturecoord_2d(model,v,direction,coord);
		xMin = std::min(xMin,coord[0]); x = std::max(x,coord[0]);
		yMin = std::min(yMin,coord[1]); y = std::max(y,coord[1]);
	}		
	//log_debug("Bounds = (%f,%f)-(%f,%f)\n",xMin,yMin,xMax,yMax); //???

	x = 1/(x-xMin); y = 1/(y-yMin);

	bool ungroup = !map_group;

	int v = 0; for(int ea:trilist)
	{
		for(int i=0;i<3;i++)
		{
			int vv = model->getTriangleVertex(ea,i);
			texturecoord_2d(model,vv,direction,coord);
			texture.addVertex((coord[0]-xMin)*x,(coord[1]-yMin)*y);	
		}

		if(ungroup) //2022
		{
			auto *p = &texture.m_vertices[v];
		
			coord[0] = coord[1] = DBL_MAX;
			for(int i=3;i-->0;)
			{
				if(p[i].s<coord[0]) coord[0] = p[i].s;
				if(p[i].t<coord[1]) coord[1] = p[i].t;
			}
			for(int i=3;i-->0;)
			{
				p[i].s-=coord[0]; p[i].t-=coord[1];
			}
		}

		texture.addTriangle(v,v+1,v+2); v+=3;
	}
	
	goto done;
}

void TextureCoordWin::setTextureCoordsDone()
{
	setTextureCoordsEtc(true,true);
	model->updateObservers();
	operationComplete(::tr("Set texture coordinates"));
}
void TextureCoordWin::moveTextureCoordsDone(bool done)
{
	//NOTE: Could set the coords, but it would be different from
	//how the Position sidebar behaves.
	/*2022: projectionwin.cc is now updating in real-time since
	//it relies on Model::applyProjection and change-notices are
	//simpler that way.
	if(done) setTextureCoordsEtc(true);*/
	setTextureCoordsEtc(!done,done);
	if(done) operationComplete(::tr("Move texture coordinates"));
}
void TextureCoordWin::updateSelectionDone()
{
	if(model->getUndoEnabled())
	{
		texture.saveSelectedUv();
		m_ignoreChange = true;
		model->operationComplete(::tr("Select texture coordinates"));
		m_ignoreChange = false;
	}

	setTextureCoordsEtc(false,true); //NEW
}
void TextureCoordWin::operationComplete(const char *opname)
{
	m_ignoreChange = true; model->operationComplete(opname);
	m_ignoreChange = false;
}

void TextureCoordWin::setTextureCoordsEtc(bool setCoords, bool setUI)
{
	dimensions.redraw().text().clear();
	
	//int_list trilist;
	//model->getSelectedTriangles(trilist);

	float s[3],t[3];
		
	//NEW: Gathering stats based on sidebar.cc.
	float cmin[2] = {+FLT_MAX,+FLT_MAX};
	float cmax[2] = {-FLT_MAX,-FLT_MAX};
	
	int m = (unsigned)trilist.size();
	int n = map?(unsigned)texture.m_triangles.size():0;
	auto *tv = texture.m_vertices.data();
	for(int i=0;i<m;i++)
	{
		int tri = n?i%n:i;
		int *tt = texture.m_triangles[tri].vertex;
		int sel = 0;
		for(int j=0;j<3;j++) if(tv[tt[j]].selected)
		sel|=1<<j;
		if(!sel) continue;
		texture.getCoordinates(tri,s,t);
		int ea = trilist[i];
		for(int j=0;j<3;j++) if(sel&1<<j)
		{
			if(setCoords)
			model->setTextureCoords(ea,j,s[j],t[j]);

			cmin[0] = std::min(cmin[0],s[j]);
			cmin[1] = std::min(cmin[1],t[j]);
			cmax[0] = std::max(cmax[0],s[j]);
			cmax[1] = std::max(cmax[1],t[j]);
		}
	}	
	
	if(cmin[0]==FLT_MAX)
	{
		if(setCoords&&trilist.empty())
		log_error("no group selected\n"); 

		u.text().clear();
		v.text().clear();

		return;
	}

	if(setCoords) //2022
	{
		//NOTE: Something like Tool::Parent::updateView
		//on texture views only would be an improvement.
		model->updateObservers();

		//TODO: This is to match the sidebar's
		//behavior, which is not not update the
		//text boxes in real-time.
		//return;
	}
	if(!setUI) return;

	for(int i=0;i<2;i++)
	{
		centerpoint[i] = (cmin[i]+cmax[i])/2;
	}
	u.set_float_val(centerpoint[0]*=texture.getUvWidth());
	v.set_float_val(centerpoint[1]*=texture.getUvHeight());
	
	//dimensions.text().format("%g,%g,%g",cmax[0]-cmin[0],cmax[1]-cmin[1],cmax[2]-cmin[2]);
	//dimensions.text().clear();
	for(int i=0;;i++)
	{
		enum{ width=5 };
		char buf[32]; 
		double dim = fabs(cmax[i]-cmin[i]);
		dim*=i?texture.getUvHeight():texture.getUvWidth();
		int len = snprintf(buf,sizeof(buf),"%.6f",dim); 
		if(char*dp=strrchr(buf,'.'))
		{
			len = std::max<int>(width,dp-buf);

			if(buf[len]=='0')
			while(buf+len>dp&&buf[len-1]=='0')
			{
				len--;
			}
			if(dp==buf+len-1) len--;
		}
		dimensions.text().append(buf,len);
		if(i==1) break;
		dimensions.text().push_back(',');
		dimensions.text().push_back(' ');		
	}
	dimensions.set_text(dimensions);
}
