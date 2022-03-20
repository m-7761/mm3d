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
#include "win.h"

#include "texturecoord.h"

#include "viewwin.h"
#include "log.h"

//2022: I need a fast solution for mapping
//PlayStation textures to a unit dimension.
enum
{
	//REMINDER: These names go keycfg.ini!

	id_uv_view_init=id_view_init,
	
	//Fit
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
};
extern int viewwin_shifthold;
static int texturecoord_fit_uv = 1|2;
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
	done:tw->updateTextureCoordsDone();
	}
	else switch(id) //Tool?
	{
	case id_edit_undo:
	case id_edit_redo:

		tw->model.perform_menu_action(id);
		break;

	case id_tool_toggle: //DICEY

		id+=id_uv_model; //break; //Ready for subtraction below.

	case id_uv_model+0: 
	case id_uv_model+1:
	case id_uv_model+3:	

		glutSetWindow(tw->model.glut_window_id);				
		extern void viewwin_toolboxfunc(int);
		viewwin_toolboxfunc(id-id_uv_model); //tool_select_faces?
		break;

	case id_uv_tool_select: case id_uv_tool_move: 
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

		//tw->submit(tw->mouse); //Can't send id_item.		
		{
			tw->texture.setMouseOperation
			((Widget::MouseOperationE)tw->recall_tool[1]);

			if(tw->texture._tool_bs_lock!=tw->recall_lock[1])
			{
				tw->texture._tool_bs_lock = tw->recall_lock[1];
				tw->model.views.status._texshlock.indicate(tw->recall_lock[1]!=0);
			}

			tw->texture.updateWidget();
		}			
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
			ea->views.status._uvfitlock.name(id==1?"Fw":"Fh");
			ea->views.status._uvfitlock.indicate(id!=3);
		}
		break;
	
	case id_uv_cmd_select_all:

		tw->select_all(); break;

	case id_uv_cmd_select_faces_incl:
	case id_uv_cmd_select_faces_excl:

		tw->select_faces(id==id_uv_cmd_select_faces_excl); 
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
static int texturecoord_init_menu()
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
		int sm = glutCreateMenu(TextureCoordWin::_menubarfunc);		
		glutAddMenuEntry(O(false,uv_fit_u,"Fit Width","","Ctrl+W"));
		glutAddMenuEntry(O(false,uv_fit_v,"Fit Height","","Ctrl+H"));		
		glutAddMenuEntry(O(true,uv_fit_uv,"Regular Fit","",""));
		fit_menu = glutCreateMenu(TextureCoordWin::_menubarfunc);
		glutAddSubMenu(::tr("Confine",""),sm);
		glutAddMenuEntry();
		glutAddMenuEntry(E(uv_fit_0,"Modulo","","0"));
		glutAddMenuEntry(E(uv_fit_1,"Unscale","","1"));
		glutAddMenuEntry(E(uv_fit_2_x,"Upscale","","2"));
		glutAddMenuEntry(E(uv_fit_2,"Scale 1/2","","Ctrl+2"));
		glutAddMenuEntry(E(uv_fit_4,"Scale 1/4","","Ctrl+3"));
		glutAddMenuEntry(E(uv_fit_8,"Scale 1/8","","Ctrl+4"));
		glutAddMenuEntry(E(uv_fit_16,"Scale 1/16","","Ctrl+5"));
		glutAddMenuEntry(E(uv_fit_32,"Scale 1/32","","Ctrl+6"));
		glutAddMenuEntry(E(uv_fit_64,"Scale 1/64","","Ctrl+7"));
		glutAddMenuEntry(E(uv_fit_128,"Scale 1/128","","Ctrl+8"));
		glutAddMenuEntry(E(uv_fit_256,"Scale 1/256","","Ctrl+9"));
		
	}
	static int view_menu=0; if(!view_menu)
	{
		view_menu = glutCreateMenu(TextureCoordWin::_menubarfunc);
		//NOTE: viewwini.cc uses "Restore to Normal" but it
		//doesn't use Home (Home comes from Misfit Model 3D.)
		glutAddMenuEntry(E(uv_view_init,"Reset","","Home"));
	}
	static int tool_menu=0; if(!tool_menu) //2022
	{
		tool_menu = glutCreateMenu(TextureCoordWin::_menubarfunc);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_vertices","Select","Tool","V"),id_uv_tool_select);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_move","Move","Tool","T"),id_uv_tool_move);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_rotate","Rotate","Tool","R"),id_uv_tool_rotate);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_scale","Scale","Tool","S"),id_uv_tool_scale);
		glutAddMenuEntry();
		extern int viewwin_shlk_menu; //Not sure GTK will like this!
		glutAddSubMenu(::tr("Shift Lock",""),viewwin_shlk_menu);
		glutAddMenuEntry();
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_recall","Switch Tool","","Space"),id_tool_recall);
	}
	static int model_menu=0; if(!model_menu)
	{
		model_menu = glutCreateMenu(TextureCoordWin::_menubarfunc);
		glutAddMenuEntry(viewwin_menu_entry(o,"edit_undo","Undo","Undo shortcut","Ctrl+Z"),id_edit_undo);
		glutAddMenuEntry(viewwin_menu_entry(o,"edit_redo","Redo","Redo shortcut","Ctrl+Y"),id_edit_redo);
		glutAddMenuEntry();
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_connected_mesh","Select Connected Mesh","Tool","C"),id_uv_model+0);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_faces","Select Faces","Tool","F"),id_uv_model+1);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_groups","Select Groups","Tool","G"),id_uv_model+3);
		glutAddMenuEntry();
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_toggle","Toggle Tool","","Tab"),id_tool_toggle);
	}
	static int cmd_menu=0; if(!cmd_menu)
	{
		cmd_menu = glutCreateMenu(TextureCoordWin::_menubarfunc);
		glutAddMenuEntry(viewwin_menu_entry(o,"cmd_select_all","Select All UVs","Command","Ctrl+A"),id_uv_cmd_select_all+0);
		glutAddMenuEntry();
		glutAddMenuEntry(E(uv_cmd_select_faces_incl,"Select Faces from UVs","","M"));
		glutAddMenuEntry(E(uv_cmd_select_faces_excl,"Select Inscribed Faces","","N"));
	}

	int menubar = glutCreateMenu(TextureCoordWin::_menubarfunc);
	glutAddSubMenu(::tr("&Fit","menu bar"),fit_menu);
	glutAddSubMenu(::tr("&View","menu bar"),view_menu);
	glutAddSubMenu(::tr("&Tools","menu bar"),tool_menu);
	glutAddSubMenu(::tr("&Model","menu bar"),model_menu);
	glutAddSubMenu(::tr("&Geometry","menu bar"),cmd_menu);
	glutAttachMenu(glutext::GLUT_NON_BUTTON);

	#undef E //viewwin_menu_entry
	#undef O //viewwin_menu_radio
	#undef X //viewwin_menu_check

	return menubar;
}
TextureCoordWin::~TextureCoordWin()
{
	glutDestroyMenu(_menubar);
}

struct MapDirectionWin : Win
{
	MapDirectionWin(int *lv)
		:
	Win("Which direction?"),
	dir(main,"Set new texture coordinates from which direction?",lv),
	ok_cancel(main)
	{
		//MAGIC NUMBER
		dir.add_item("Front").add_item("Back");
		dir.add_item("Left").add_item("Right");
		dir.add_item("Top").add_item("Bottom");

		//2022: This isn't necessary, but since the auto selection
		//system is pretty good, it's suggestive that the selection
		//is made for the user... what wouldn't hurt though is more
		//options to choose from the view ports.
		if(*lv!=-1) ok_cancel.ok.activate();
	}

	multiple dir;
	ok_cancel_panel ok_cancel;
};

void TextureCoordWin::open()
{
	setModel();
	
	// If visible, setModel already did this
	if(hidden())
	{
		//REMINDER: GLX needs to show before
		//it can use OpenGL.
		show();
		
		if(trilist.empty()) openModel();
	}
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

//NOTE: Win::Widget::getXY could be overriden instead but
//it doesn't deal with "entry" (really "exit" here) logic.
static void texturecoord_entry_cb(Widgets95::ui *ui, int state)
{
	if(!state&&ui==Win::event.active_control_ui)
	Win::event.deactivate();
}
static bool texturecoord_motion_cb(Widgets95::ui *ui, int x, int y)
{
	//REMINDER: Win::Widget::getXY already steals focus to
	//the canvas, but this causes some problems with menus
	//(mainly around Space and Tab "accelerators" although
	//I've diabled _space_clicks in widgets95.h there's no
	//way yet to disable tab navigation.)
	if(ui==Win::event.active_control_ui)
	{
		//NOTE: x/y are used down below.
		auto &a = ((TextureCoordWin*)ui)->scene.area(); 	
		if((unsigned)(x-a[0])<(unsigned)a[2])
		if((unsigned)(y-a[1])<(unsigned)a[3])
		Win::event.deactivate();
	}
	//NOTE: this needs to run after
	extern bool win_widget_motion(Widgets95::ui*,int,int); //YUCK
	return win_widget_motion(ui,x,y);
}
void TextureCoordWin::init()
{
	//Tab key? (like viewpanel_motion_func)
	entry_callback = texturecoord_entry_cb;
	motion_callback = texturecoord_motion_cb; 

	_menubar = texturecoord_init_menu();

	m_ignoreChange = false; 

	for(int i=2;i-->0;recall_lock[i]=0) recall_tool[i] = !i;

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
	int w = config.get("ui_texcoord_lines_color",+id_white);
	int r = config.get("ui_texcoord_selection_color",+id_red);
	if(w&&w<8) w = id_white; if(r&&r<8) r = id_red; //back-compat
	white.set_int_val(w); red.set_int_val(r);

	// TODO handle undo of select/unselect?
	// Can't do this until after constructor is done because of observer interface
	//setModel(model);
	texture.setModel(model);
	texture.setInteractive(true);
	texture.setDrawUvBorder(true); 

	mouse.select_id(+Widget::MouseSelect)
	.add_item(Widget::MouseSelect,"Select")
	.add_item(Widget::MouseMove,"Move")
	.add_item(Widget::MouseRotate,"Rotate")
	.add_item(Widget::MouseScale,"Scale");	
		
	scale_sfc.set(config.get("ui_texcoord_scale_center",true));
	scale_kar.set(config.get("ui_texcoord_scale_aspect",false));

	map.add_item(3,"Triangle").add_item(4,"Quad");
	map.add_item(0,"Group"); //Group???
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
		//break;

	case id_item: 

		//Just to be safe?
		if(recall_tool[1]!=(int)mouse)
		{
			recall_lock[0] = recall_lock[1];
			recall_tool[0] = recall_tool[1];
			recall_tool[1] = mouse;
		}

		texture.setMouseOperation
		((Widget::MouseOperationE)recall_tool[1]);

		recall_lock[1] = viewwin_shifthold?texture._tool_bs_lock:0;
		if(texture._tool_bs_lock!=recall_lock[1])
		{
			texture._tool_bs_lock = recall_lock[1];
			model.views.status._texshlock.indicate(recall_lock[1]!=0);
		}

		texture.updateWidget();
		break;

	case id_subitem:

		mapReset(); break;

	case id_white: 

		config.set("ui_texcoord_lines_color",(int)white);
	    goto init2;

	case id_red:

		config.set("ui_texcoord_selection_color",(int)red);
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

			updateTextureCoordsDone();	
		}
		else if(c==scale_sfc)
		{
			config.set("ui_texcoord_scale_center",(bool)scale_sfc);
			goto init2;
		}
		else if(c==scale_kar)
		{
			config.set("ui_texcoord_scale_aspect",(bool)scale_kar);
			goto init2;
		}
		break;

	case id_scene:

		texture.draw(scene.x(),scene.y(),scene.width(),scene.height());
		break;

	case 'U': case 'V':
	
		texture.move(u.float_val()-centerpoint[0],v.float_val()-centerpoint[1]); 
		updateTextureCoordsDone();
		break;
	
	case id_ok: case id_close:

		event.close_ui_by_create_id(); //Help?

		//I guess this model saves users' work?
		hide();
		
		//DUPLICATE (FIX)
		//There seems to be a wxWidgets bug that is documented under hide() which
		//can be defeated by voiding the current GLUT window. TODO: this needs to
		//be removed once the bug is long fixed. Other windows are using this too.
		glutSetWindow(0);
		
		return;
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

void TextureCoordWin::mapReset()
{
	double min = 0, max = 1; //goto

	if(map)
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

	done: updateTextureCoordsDone();

		return texture.updateWidget();
	}
		
	if(trilist.empty()) //???
	return log_error("no triangles selected\n"); //???

	int direction;
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
	}
	if(id_ok!=MapDirectionWin(&direction).return_on_close())
	return;

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
	int v = 0; for(int ea:trilist)
	{
		for(int i=0;i<3;i++)
		{
			int vv = model->getTriangleVertex(ea,i);
			texturecoord_2d(model,vv,direction,coord);
			texture.addVertex((coord[0]-xMin)*x,(coord[1]-yMin)*y);	
		}
		texture.addTriangle(v,v+1,v+2); v+=3;
	}

	goto done;
}

void TextureCoordWin::updateTextureCoordsDone(bool done)
{
	//NOTE: Could set the coords, but it would be different from
	//how the Position sidebar behaves.
	if(done) setTextureCoordsEtc(true);
	if(done) operationComplete(::tr("Move texture coordinates"));
}
void TextureCoordWin::updateSelectionDone()
{
	if(model->getUndoEnabled())
	{
		texture.saveSelectedUv();
		m_ignoreChange = true;
		model->nonEditOpComplete(::tr("Select texture coordinates"));
		m_ignoreChange = false;
	}

	setTextureCoordsEtc(false); //NEW
}
void TextureCoordWin::operationComplete(const char *opname)
{
	m_ignoreChange = true; model->operationComplete(opname);
	m_ignoreChange = false;
}

void TextureCoordWin::setTextureCoordsEtc(bool setCoords)
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
