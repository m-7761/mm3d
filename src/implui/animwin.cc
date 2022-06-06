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

#include "animwin.h"
#include "viewwin.h"

#include "model.h"
#include "log.h"
#include "msg.h"

struct NewAnim : Win
{		
	void submit(int id)
	{
		if(id!=id_ok||!name.text().empty())
		basic_submit(id);
	}		  

	NewAnim(std::string *lv, int *lv2)
		:
	Win("New Animation"),
	name(main,"Name",lv,id_name),
	type(main,"Animation Mode",lv2),
	aligner(type),
	choices1_2(aligner),
	frame(choices1_2,2,"Frame"),
	skel(choices1_2,1,"Skeletal"),
	both(aligner,3,"Indeterminate (Both)"),
	ok_cancel(main)
	{		
		name.expand(); 
		type.style(bi::etched).expand();
		aligner.calign();
		choices1_2.expand().space(0);
		skel.ralign();

		active_callback = &NewAnim::submit;
	}

	textbox name;
	multiple type;
	panel aligner;
	row choices1_2;
	multiple::item frame,skel,both;
	ok_cancel_panel ok_cancel;
};

struct AnimWin::Impl
{	
	Impl(AnimWin &win)
		:
	win(win),
	sidebar(win.model.sidebar.anim_panel),
	new_animation(sidebar.new_animation),
	//sep_animation(sidebar.sep_animation),
	model(win.model),
	mode(),soft_mode(-1),anim(),soft_anim(~0),frame(),
	playing(),autoplay()
	{
		//See AnimPanel::refresh_list.
		//It's easier to let the animation window manage this since
		//it's opaque.
		int swap = sidebar.animation;
		sidebar.animation.clear();
		sidebar.animation.reference(win.animation);				
		sidebar.refresh_list();
		sidebar.animation.select_id(swap);
		win.animation.select_id(-1);
		
		_init_menu_toolbar(); //2022		
	}
	~Impl()
	{
		for(int*m=&_menubar;m<=&_anim_menu;m++)
		glutDestroyMenu(*m);
	}

	AnimWin &win;
	SideBar::AnimPanel &sidebar;
	const int &new_animation; 
	//const int &sep_animation;

	int _menubar;
	int _tool_menu;
	int _anim_menu;
	void _init_menu_toolbar();

	void draw_tool_text();

	Model *model;
	Model::AnimationModeE mode;
	int soft_mode;
	bool soft_mode_bones;
	unsigned anim;
	unsigned soft_anim;
	unsigned frame;	
	double soft_frame;
	bool playing,autoplay;
	int step_t0;
	int step_ms;
	
	void open2(bool undo);
	void refresh_item(bool undo=false);
	void refresh_undo();

	void close();

	void play(int=0);
	void stop(){ play(id_animate_stop); }
	bool step();

	void anim_selected(int, bool local=false);
	void anim_deleted(int);

	void frames_edited(double);	
	void set_frame(double);
 
	bool copy();
	void paste(bool values);


		//UTILITIES//

		
	Model::AnimationModeE item_mode(int i)
	{
		if(i<0) return Model::ANIMMODE_NONE;
		/*
		if(i>=sep_animation&&i<new_animation)
		return Model::ANIMMODE_FRAME;
		return Model::ANIMMODE_JOINT;
		*/
		i = model->getAnimType(i);
		int cmp =  win.model.animation_mode;
		if(i==3||cmp==7&&(i&1)) i = cmp;
		return (Model::AnimationModeE)i;
	}
	unsigned item_anim(int i)
	{
		//if(i>=sep_animation) i-=sep_animation;
		return i;
	}
	int anim_item()
	{
		if(!mode) return -1;
		//if(mode==Model::ANIMMODE_FRAME)
		//return (unsigned)sep_animation+anim; 
		return anim;
	}	

		//COPY/PASTE//

	struct KeyframeData
	{
		Model::Interpolate2020E e;
		//Model::KeyType2020E isRotation; //???
		double x,y,z;
	};

	struct KeyframeCopy
	{			
		KeyframeData data[3];

		unsigned object;
		const void *object2; //2022
	};
	struct VertexFrameCopy : KeyframeData
	{
		unsigned vertex;
		const void *vertex2; //2022
	};
	/*struct FramePointCopy
	{
		unsigned point; double x,y,z,rx,ry,rz;
	};*/	
	std::vector<KeyframeCopy> copy1;
	std::vector<VertexFrameCopy> copy2;
	std::vector<KeyframeCopy> copy3; //FramePointCopy
};

enum
{
	//REMINDER: These names go in keycfg.ini!

	id_aw_view_init=id_view_init,
	id_aw_view_snap=id_view_persp, //init+1

	//Tool
	id_aw_tool_move=GraphicWidget::MouseMove,
	id_aw_tool_scale=GraphicWidget::MouseScale,
	id_aw_tool_select=GraphicWidget::MouseSelect,
	//these should start at 0, however that
	//clashes with the
	//above enum values
	id_aw_model=1000, //ARBITRARY
	id_aw_anims=2000, //how many?

	id_aw_hide_delete=3000,
	id_aw_hide_cancel,
};
static bool animwin_auto_scroll;
extern void win_help(const char*, Win::ui *w);
void AnimWin::_menubarfunc(int id)
{
	AnimWin *aw;
	MainWin* &viewwin(int=glutGetWindow());
	if(auto*w=viewwin())
	aw = w->_animation_win;
	else{ assert(0); return; } assert(aw);

	if((unsigned)(id-id_aw_anims)<100)
	{
		aw->impl->anim_selected(id-id_aw_anims-1);
	}
	else switch(id) //Tool?
	{
	case id_help:

		aw->submit(id_help);
		break;

	case id_animate_window:

		aw->submit(id_ok);
		break;
	
	case id_edit_undo:
	case id_edit_redo:

		aw->model.perform_menu_action(id);
		break;

	case id_tool_toggle: //DICEY
	case id_tool_none: //Esc???
	
		id+=id_aw_model; //break; //Ready for subtraction below.

	case id_aw_model+4: //tool_select_bone_joints?
	case id_aw_model+5: //tool_select_points?

		glutSetWindow(aw->model.glut_window_id);				
		extern void viewwin_toolboxfunc(int);
		viewwin_toolboxfunc(id-id_aw_model);
		break;

	case id_aw_tool_select: 

		aw->model._sync_sel2 = -1; //HACK		
		aw->select.set_int_val(0);
		aw->graphic.setMouseSelection(graphic::Select);
		aw->shrinkSelect();
		//break;

	case id_aw_tool_move:	
	case id_aw_tool_scale: //case id_aw_tool_select: 

		aw->_show_tool(id);
		aw->graphic.setMouseOperation((graphic::MouseE)id);	
		
		switch(id)
		{
		case id_aw_tool_select: id = Tool::TT_SelectTool; break;
		case id_aw_tool_move: id = Tool::TT_MoveTool; break;
		case id_aw_tool_scale: id = Tool::TT_ScaleTool; break;
		}
		
		aw->scene.redraw();
		aw->model._sync_tools(id,0); //Note: 0 is tool_connected.
		return; //break;

	case id_aw_view_init:

		aw->graphic.keyPressEvent(Tool::BS_Left|Tool::BS_Special,0,0,0); //Home
		break;

	case id_aw_view_snap:

		animwin_auto_scroll = 0!=glutGet(glutext::GLUT_MENU_CHECKED);
		config.set("aw_view_snap",animwin_auto_scroll);
		break;

	case id_aw_hide_delete:
	case id_aw_hide_cancel:

		aw->_toggle_button(id-id_aw_hide_delete);
		break;
	}
}
void AnimWin::_toggle_button(int id)
{
	//It's helpful to show the buttons and nav needs
	//to be adjusted to trigger reshape, although it 
	//may not matter if a different tool is selected.
	if(graphic.m_operation)
	{
		_show_tool(0);
		graphic.m_operation = graphic::MouseNone;
		glutSetMenu(impl->_tool_menu);
		glutext::glutMenuEnable(id_tool_none,glutext::GLUT_MENU_CHECK);
	}

	auto &bt = (id?close:del);
	bt.set_hidden(!bt.hidden());
	int bts = del.hidden();
	bts|=close.hidden()<<1;			
	config.set("aw_buttons_mask",bts);
	if(id==1)
	close.id(bts&2?id_ok:id_close);

	if(seen()) //DICEY
	{
		int dx = anim_nav._w;
		anim_nav.pack();
		dx = anim_nav._w-dx;				
		_reshape[2]+=dx;
		nav.lock(nav._w+dx,0);				
	}
}
void AnimWin::Impl::_init_menu_toolbar()
{	
	std::string o; //radio	
	#define E(id,...) viewwin_menu_entry(o,#id,__VA_ARGS__),id_##id
	#define O(on,id,...) viewwin_menu_radio(o,on,#id,__VA_ARGS__),id_##id
	#define X(on,id,...) viewwin_menu_check(o,on,#id,__VA_ARGS__),id_##id
	extern utf8 viewwin_menu_entry(std::string &s, utf8 key, utf8 n, utf8 t="", utf8 def="", bool clr=true);
	extern utf8 viewwin_menu_radio(std::string &o, bool O, utf8 key, utf8 n, utf8 t="", utf8 def="");
	extern utf8 viewwin_menu_check(std::string &o, bool X, utf8 key, utf8 n, utf8 t="", utf8 def="");
	
	static int frame_menu=0; if(!frame_menu)
	{
		frame_menu = glutCreateMenu(_menubarfunc);
		//NEED SOMETHING HERE
	}
	static int view_menu=0; if(!view_menu)
	{
		view_menu = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(E(aw_view_init,"Reset","","Home"));
		glutAddMenuEntry();
		bool x = config.get("aw_view_snap",true);
		animwin_auto_scroll = x;
		glutAddMenuEntry(X(x,aw_view_snap,"Auto-Scroll","","F12")); 
	}	
		_tool_menu = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(viewwin_menu_radio(o,false,"tool_select_connected","Select","Tool","C"),id_aw_tool_select);
		glutAddMenuEntry(viewwin_menu_radio(o,false,"tool_move","Move","Tool","T"),id_aw_tool_move);
		glutAddMenuEntry(viewwin_menu_radio(o,false,"tool_scale","Scale","Tool","S"),id_aw_tool_scale);
		glutAddMenuEntry(O(true,tool_none,"None","","Esc"));	
	
	static int model_menu=0; if(!model_menu)
	{
		model_menu = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(viewwin_menu_entry(o,"edit_undo","Undo","Undo shortcut","Ctrl+Z"),id_edit_undo);
		glutAddMenuEntry(viewwin_menu_entry(o,"edit_redo","Redo","Redo shortcut","Ctrl+Y"),id_edit_redo);
		glutAddMenuEntry();
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_bone_joints","Select Bone Joints","Tool","B"),id_aw_model+4);
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_select_points","Select Points","Tool","O"),id_aw_model+5);
		glutAddMenuEntry();
		glutAddMenuEntry(viewwin_menu_entry(o,"tool_toggle","Toggle Tool","","Tab"),id_tool_toggle);
	}
	
		_anim_menu = glutCreateMenu(_menubarfunc);

	static int f1_menu=0; if(!f1_menu)
	{
		int buttons = glutCreateMenu(_menubarfunc);		
		glutAddMenuEntry(E(aw_hide_delete,"Toggle Delete","","Shift+Delete"));
		glutAddMenuEntry(E(aw_hide_cancel,"Toggle Esc","","Shift+Esc"));
		f1_menu = glutCreateMenu(_menubarfunc);
		glutAddMenuEntry(E(help,"Contents","Help|Contents","F1"));
		glutAddMenuEntry();
		glutAddSubMenu("Buttons",buttons);
		glutAddMenuEntry();
		glutAddMenuEntry(E(animate_window,"Close","","A"));
	}
	int bts = config.get("aw_buttons_mask",0);
	if(bts&1) win.del.set_hidden();
	if(bts&2) win.close.set_hidden().id(id_ok);

	_menubar = glutCreateMenu(AnimWin::_menubarfunc);
	glutAddSubMenu(::tr("Frame","menu bar"),frame_menu);
	glutAddSubMenu(::tr("View","menu bar"),view_menu);
	glutAddSubMenu(::tr("Tools","menu bar"),_tool_menu);
	glutAddSubMenu(::tr("Model","menu bar"),model_menu);
	glutAddSubMenu(::tr("Animation","menu bar"),_anim_menu);
	glutAddSubMenu(::tr("Help","menu bar"),f1_menu);
	glutAttachMenu(glutext::GLUT_NON_BUTTON);

	#undef E //viewwin_menu_entry
	#undef O //viewwin_menu_radio
	#undef X //viewwin_menu_check
}

extern void animwin_enable_menu(int menu, int ins)
{
	extern void viewwin_status_func(int=0);
	viewwin_status_func();

	void *off,*on = 0; 
	if(menu<=0) menu = -menu;
	else on = glutext::GLUT_MENU_ENABLE;
	off = !on?glutext::GLUT_MENU_ENABLE:0;
		
	if(menu) glutSetMenu(menu);

	if(!ins) on = 0;
	glutext::glutMenuEnable(id_animate_copy,on);
	glutext::glutMenuEnable(id_animate_paste,0); // Disabled until copy occurs
	glutext::glutMenuEnable(id_animate_paste_v,0); // Disabled until copy occurs
	glutext::glutMenuEnable(id_animate_delete,on); 
}

void AnimWin::open(bool undo)
{	
	//animwin_enable_menu(menu,win.model.animate_insert);

	impl->open2(undo);

	//HACK: MainWin::open_animation_window opens up
	//the window manually, so it can operate in the 
	//background to support the sidebar and toolbar.
	//It's like this because animwin.cc has so much
	//code.
	//show();
}
void AnimWin::Impl::open2(bool undo)
{	
	bool swapping = false; //YUCK

	if(!mode)
	{
		soft_mode_bones = model->getDrawJoints();
	}

	if(model!=win.model)
	{
		swapping = true; //NEW

		model = win.model;

		win.graphic.setModel(model); //2022

		soft_anim = ~0;
	}
	else if(mode&&!undo) //Sidebar?
	{			
		anim_selected(sidebar.animation);
		return;
	}

	if(undo&&!swapping)
	{
		mode = model->getAnimationMode();
		anim = model->getCurrentAnimation();
		frame = model->getCurrentAnimationFrame();
		soft_frame = model->getCurrentAnimationFrameTime();
	}
	else
	{
		int id = sidebar.animation;
		if(id==new_animation)
		{
			//RECURSIVE
			return anim_selected(id);
		}
		mode = item_mode(id);
		anim = item_anim(id);		

		//2021: Recall frame if toggling between
		//<None> and the same animation manually.
		//frame = 0;
		if(anim==model->getCurrentAnimation())
		{
			frame = model->getCurrentAnimationFrame();
			soft_frame = model->getCurrentAnimationFrameTime();
		}
		else soft_frame = frame = 0;

		model->setCurrentAnimation(anim,mode);

		//The first frame isn't necessarily the start of the animation.
		//model->setCurrentAnimationFrame(frame,Model::AT_invalidateNormals);
		if(undo) //autoplay?
		model->setCurrentAnimationFrameTime(0,Model::AT_invalidateNormals);
	}

	//Not disabling on model swapping???
	//if(mode) animwin_enable_menu(win.menu);
	animwin_enable_menu(mode?win.menu:-win.menu,win.model.animate_insert);

	win.animation.select_id(anim_item());
	sidebar.animation.select_id(anim_item());
	
	if(!undo&&mode)
	{
		model->operationComplete(::tr("Start animation mode","operation complete"));

		if(anim!=soft_anim)
		{
			autoplay = true; play(); //NEW			
		}
	}

	//Reminder: set_frame checks if playing.
	refresh_item();
}

void AnimWin::Impl::anim_deleted(int item)
{
	if(id_ok!=WarningBox
	(::tr("Delete Animation?","window title"),
	 ::tr("Are you sure you want to delete this animation?"),id_ok|id_cancel))
	return;

	if(playing) stop(); //NEW
		
	//FIX ME
	//NOTE: removeSkelAnim/deleteAnimation select the
	//previous animation always. Seems like bad policy.
	//item = std::min(0,item-1);
	if(item&&item>=new_animation-1)
	{
		item--;
	}

	auto swap = anim;
	mode = item_mode(item);
	anim = item_anim(item);
	model->deleteAnimation(swap);
	model->setCurrentAnimation(anim,mode);
	open2(true);

	model->operationComplete(::tr("Delete Animation","Delete animation,operation complete"));
}
void AnimWin::Impl::anim_selected(int item, bool local)
{
	//log_debug("anim name selected: %d\n",item); //???

	int was = win.animation;

	if(item==was)
	{
		if(!local)
		{
			double cmp = sidebar.frame;
			if(cmp!=win.timeline.float_val()) 
			set_frame(cmp);
			return;
		}		
	}
	else win.animation.select_id(item);
	
	if(item!=sidebar.animation.int_val())
	{
		if(item<new_animation)
		sidebar.animation.select_id(item);
	}

	if(item<0)
	{
		assert(item==-1);

		if(mode) close();

		return;
	}
	else if(was<0&&item>=0)
	{
		animwin_enable_menu(win.menu,win.model.animate_insert);
	}

	if(item>=new_animation)
	{
		std::string name;
		int type = config.get("ui_new_anim_type",3);
		if(!type||type>3) type = 3;
		if(!event.wheel_event
		&&id_ok==NewAnim(&name,&type).return_on_close())
		{
			stop();

			config.set("ui_new_anim_type",type);

			//mode = type?Model::ANIMMODE_FRAME:Model::ANIMMODE_JOINT;
			anim = model->addAnimation((Model::AnimationModeE)type,name.c_str());
			mode = item_mode(anim);
			model->setCurrentAnimation(anim,mode);

			//REMOVE ME
			//The default is -1 (Animation::_time_frame)
			model->setAnimTimeFrame(anim,0);

			set_frame(0);			
			model->operationComplete(::tr("New Animation","operation complete"));			
			open2(false);
		}
		else
		{
			win.animation.select_id(anim_item());
			sidebar.animation.select_id(anim_item());
		}
	}
	else
	{
		mode = item_mode(item);
		anim = item_anim(item);
	
		model->setCurrentAnimation(anim,mode);

		//FIX ME
		//NEW: Note setCurrentAnimation seems to ignore this unless the animation
		//mode was previously ANIMMODE_NONE?!
		//if(!undo) 
		model->operationComplete(::tr("Set current animation","operation complete"));
	
		frame = 0;

		if(0) //Play the animation one time.
		{
			// Re-initialize time interval based on new anim's FPS.
			if(playing) play();
		}
		else //if(!undo)
		{
			autoplay = !playing; //NEW

			play(); //2019
		}

		//Reminder: set_frame checks if playing.
		refresh_item(); 
	}
}

void AnimWin::modelChanged(int changeBits)
{
	if(changeBits&(Model::AnimationMode|Model::AnimationSet))
	impl->refresh_undo();
	if(changeBits&Model::AnimationProperty&&impl->mode)
	{
		impl->frames_edited(model->getAnimTimeFrame(impl->anim));

		//2022: I'm not sure this shouldn't just call on
		//refresh_undo() as well?
		fps.set_float_val(model->getAnimFPS(impl->anim));
		wrap.set(model->getAnimWrap(impl->anim));
	}

	//if(!status.hidden())
	{
		int y = 0;
		if(impl->mode&1) y+=model.nselection[Model::PT_Joint];
		if(impl->mode&2) y+=model.nselection[Model::PT_Point];
		if(y<=2) y*=125;
		else y = 250+50*std::min(4,y-2);
		//y+=impl->mode?35:25; //I swear 35 seems too little?
		y+=impl->mode?37:25; //I swear 40 seems like too much?
		int yy = status.drop();
		int extra = yy-status.int_val(); //Add manual resizing?
		if(y+extra>720)
		y = 720-extra;
		status.int_val() = y;
		y+=extra;		
		if(y!=status.drop()) status.lock(0,y);

		//Assuming change?
		//NOTE: I've changed key frames edits to AnimationProperty. 
		status.redraw(); 
	}
		
	//2022: Seeing if the dropdown menu can be removed now 
	//that there's a big honking menubar :(
	if(changeBits&(Model::AnimationSet
	|Model::AnimationMode|Model::AddAnimation))
	{
		utf8 nm = nullptr;
		if(impl->mode) nm = 
		model->getAnimName(impl->anim);
		glutSetWindow(glut_window_id());
		glutSetWindowTitle(nm?nm:"<None>");
	}

	if(changeBits&Model::AnimationSelection)
	{
		updateXY();
	}
}

void AnimWin::Impl::frames_edited(double n)
{
	double nn = model->getAnimTimeFrame(anim);
	if(nn!=n&&model->setAnimTimeFrame(anim,n))
	model->operationComplete(::tr("Change Frame Count","operation complete"));
	else n = nn;

	win.frames.set_float_val(n);	
	win.timeline.set_range(0,n);
	win.model.views.timeline.set_range(0,n);
	sidebar.frame.limit(0,n);
	win.frame.limit(0,n);
	win.x.limit(0,n);
	
	//HACK: Truncate?
	if(sidebar.frame.float_val()>n) set_frame(n);
}

void AnimWin::Impl::set_frame(double i)
{
	bool ok = true;
	
	//REMINDER: These are coming faster than the views
	//refresh rate. Guessing it's just the mouse speed.

	//if(model->setCurrentAnimationFrame(i,Model::Model::AT_invalidateAnim))
	if(ok=model->setCurrentAnimationFrameTime(i,Model::Model::AT_invalidateAnim))
	{
		//2021: I can't recall why I did this but it seems like it
		//can probably be removed. The API does it but only for BSP
		//insurance.

		//REMOVE ME?
		//HACK: The slider can be manually dragged faster than the
		//animation can be displayed right now.
		model->invalidateAnimNormals();
	}
	else i = 0; 

	//setCurrentAnimationFrameTime doesn't call this because
	//normally operationComplete does that.
	//https://github.com/zturtleman/mm3d/issues/90
	//DecalManager::getInstance()->modelUpdated(model); //???
	model->updateObservers();

	/*if(ok) //REFRENCE
	{
		if((int)i==i)
		win.timeline.name().format("%s\t%d",::tr("Frame: "),(int)i);
		else win.timeline.name().format("\t%g",i);
	}
	else win.timeline.name(::tr("Frame: \tn/a"));*/
	if(ok) win.frame.set_float_val(i);

	sidebar.frame.set_float_val(i);
	win.model.views.timeline.set_float_val(i);

	//win.timeline.name().push_back('\t');

	//frame = i;
	frame = model->getAnimFrame(anim,i);
	soft_frame = i;
	win.timeline.set_float_val(i); //NEW!

	if(animwin_auto_scroll&&!autoplay)
	if(!win.hidden()&&!win.timeline.empty()) //Synchronize?
	{
		//-1 is since the canvas is inside a 1px border.
		int x = win.scene.x()-1+win.timeline.midpoint();

		win.graphic.scrollAnimationFrameToPixel(x);	
	}
}

bool AnimWin::Impl::copy()
{
	copy1.clear(); copy2.clear(); copy3.clear();

	double t = model->getCurrentAnimationFrameTime();

	if(mode&Model::ANIMMODE_JOINT)
	{	
		//2022: This matches copycmd.cc semantics and 
		//the old system (Ctrl+Shift+C) is confused by
		//making paste to filter according to selection.
		size_t sel = win.model.nselection[Model::PT_Joint];

		auto &jl = model->getJointList();
		copy1.reserve(sel?sel:jl.size());

		Model::Position jt{Model::PT_Joint,jl.size()};

		KeyframeCopy cp;
		while(jt-->0) if(!sel||jl[jt]->m_selected)
		{
			//https://github.com/zturtleman/mm3d/issues/127
			//if(cp.e=model->getKeyframe(anim,frame,jt,rot,cp.x,cp.y,cp.z)) //???
			{
				auto &cd = cp.data; for(int i=3;i-->0;)
				{
					auto kt = Model::KeyType2020E(1<<i);
					//cd.e = model->getKeyframe(anim,frame,jt,kt,cd.x,cd.y,cd.z);
					cd[i].e = model->hasKeyframe(anim,frame,jt,kt);
				}
				model->interpKeyframe(anim,frame,t,jt,&cd[0].x,&cd[1].x,&cd[2].x);

				cp.object = jt;
				cp.object2 = jl[jt]; copy1.push_back(cp);
			}
		}

		if(copy1.empty())
		if(~mode&Model::ANIMMODE_FRAME) //2021
		{
			//msg_error(::tr("No skeletal animation data to copy"));
			model_status(model,StatusError,STATUSTIME_LONG,"No skeletal animation data to copy");		
			return false;
		}
	}
	if(mode&Model::ANIMMODE_FRAME)
	{
		size_t sel = win.model.nselection[Model::PT_Vertex];		

		auto &vl = model->getVertexList();
		size_t v = vl.size();
		copy2.reserve(sel?sel:v);

		VertexFrameCopy cp;
		while(v-->0) if(!sel||vl[v]->m_selected)
		{
			//https://github.com/zturtleman/mm3d/issues/127
			//if(model->getFrameAnimVertexCoords(anim,frame,v,cp.x,cp.y,cp.z))
			{
				//cp.e = model->getFrameAnimVertexCoords(anim,frame,v,cp.x,cp.y,cp.z);
				cp.e = model->hasFrameAnimVertexCoords(anim,frame,v);
				model->getVertexCoords(v,&cp.x);

				cp.vertex = v;
				cp.vertex2 = vl[v]; copy2.push_back(cp);
			}		
		}

		sel = win.model.nselection[Model::PT_Point];

		auto &pl = model->getJointList();
		copy3.reserve(sel?sel:pl.size());

		Model::Position pt{Model::PT_Point,pl.size()};

		//FramePointCopy cpt;
		KeyframeCopy cpt; 
		while(pt-->0) if(!sel||pl[pt]->m_selected)
		{
			/*
			if(model->getFrameAnimPointCoords(anim,frame,pt,cpt.x,cpt.y,cpt.z)
			 &&model->getFrameAnimPointRotation(anim,frame,pt,cpt.rx,cpt.ry,cpt.rz))
			{
				cpt.point = pt; copy3.push_back(cpt);
			}*/
			//https://github.com/zturtleman/mm3d/issues/127
			//if(cp.e=model->getKeyframe(anim,frame,pt,rot,cp.x,cp.y,cp.z)) //???
			{
				auto &cd = cpt.data; for(int i=3;i-->0;)
				{
					auto kt = Model::KeyType2020E(1<<i);
					//cd.e = model->getKeyframe(anim,frame,pt,kt,cd.x,cd.y,cd.z);
					cd[i].e = model->hasKeyframe(anim,frame,pt,kt);
				}			
				model->interpKeyframe(anim,frame,t,pt,&cd[0].x,&cd[1].x,&cd[2].x);

				cpt.object = pt;
				cpt.object2 = pl[pt]; copy3.push_back(cpt);
			}
		}

		if(copy2.empty()&&copy3.empty())
		if(~mode&Model::ANIMMODE_JOINT) //2021
		{
			//msg_error(::tr("No frame animation data to copy"));
			model_status(model,StatusError,STATUSTIME_LONG,"No frame animation data to copy");		
			return false;
		}
		else if(copy1.empty())
		{
			model_status(model,StatusError,STATUSTIME_LONG,"No sketetal/frame animation data to copy");		
			return false;
		}
	}

	return true;
}
void AnimWin::Impl::paste(bool values)
{
	int made = 0, attempts = 0;
	if(mode==Model::ANIMMODE_JOINT
	||mode&Model::ANIMMODE_JOINT&&!copy1.empty())
	{
		if(copy1.empty()) //msg_error(::tr("No skeletal animation data to paste"));
		return model_status(model,StatusError,STATUSTIME_LONG,"No skeletal animation data to paste");		

		size_t sel = win.model.nselection[Model::PT_Joint];

		auto &jl = model->getJointList();
		for(KeyframeCopy*p=copy1.data(),*d=p+copy1.size();p<d;p++)
		{
			Model::Position jt{Model::PT_Joint,p->object};
			
			if(sel&&!jl[jt]->m_selected) continue;

			attempts++; if(jl[jt]!=p->object2) continue;			
			if(!made++) frame = model->makeCurrentAnimationFrame();

			for(int i=0;i<3;i++)			
			{
				auto &cd = p->data[i];
				auto kt = Model::KeyType2020E(1<<i);
				model->setKeyframe(anim,frame,jt,kt,cd.x,cd.y,cd.z,
				values&&cd.e<Model::InterpolateStep?Model::InterpolateLerp:cd.e);
			}
		}
	}
	if(mode==Model::ANIMMODE_FRAME
	||mode&Model::ANIMMODE_FRAME&&!(copy2.empty()&&copy3.empty()))
	{
		if(copy2.empty()&&copy3.empty()) //msg_error(::tr("No frame animation data to paste"));
		return model_status(model,StatusError,STATUSTIME_LONG,"No frame animation data to paste");		

		size_t sel = win.model.nselection[Model::PT_Vertex];

		auto &vl = model->getVertexList();
		for(VertexFrameCopy*p=copy2.data(),*d=p+copy2.size();p<d;p++)
		{
			auto *vp = vl[p->vertex];			
			if(sel&&!vp->m_selected) continue; //2022

			attempts++; if(vp!=p->vertex2) continue;			
			if(!made++) frame = model->makeCurrentAnimationFrame();

			model->setFrameAnimVertexCoords(anim,frame,p->vertex,&p->x,
			values&&p->e<Model::InterpolateStep?Model::InterpolateLerp:p->e);
		}

		sel = win.model.nselection[Model::PT_Point];

		auto &pl = model->getPointList();
		for(KeyframeCopy*p=copy3.data(),*d=p+copy3.size();p<d;p++)
		{
			Model::Position pt{Model::PT_Point,p->object};
			
			if(sel&&!pl[pt]->m_selected) continue;

			attempts++; if(pl[pt]!=p->object2) continue;
			if(!made++) frame = model->makeCurrentAnimationFrame();

			for(int i=0;i<3;i++)			
			{
				auto &cd = p->data[i];
				auto kt = Model::KeyType2020E(1<<i);
				model->setKeyframe(anim,frame,pt,kt,cd.x,cd.y,cd.z,
				values&&cd.e<Model::InterpolateStep?Model::InterpolateLerp:cd.e);
			}
		}	
	}
	int errors = attempts-made;
	if(attempts==0) //TODO: Report selection+clipboard disjunction?
	return model_status(model,StatusError,STATUSTIME_LONG,"Selection and clipboard don't overlap");
	if(errors!=0) model_status(model,StatusError,STATUSTIME_LONG,
	"%d/%d couldn't paste because of model edits after the clipboard was filled",errors,attempts);
	if(errors==attempts)
	return model->undoCurrent(); //makeCurrentAnimationFrame?

	model->setCurrentAnimationFrame(frame,Model::AT_invalidateAnim);

	model->operationComplete(mode==Model::ANIMMODE_FRAME?
	::tr("Paste frame","paste frame animation position operation complete"):
	::tr("Paste keyframe","Paste keyframe animation data complete"));
}

static void animwin_step(int id) //TEMPORARY
{
	//NOTE: model was 0 on hitting close button while playing.
	auto w = (AnimWin*)Widgets95::e::find_ui_by_window_id(id);
	if(w&&w->model&&w->impl->step())
	glutTimerFunc(w->impl->step_ms,animwin_step,id);
}
void AnimWin::Impl::play(int id)
{
	bool stopping = id==id_animate_stop;

	if(playing&&id) stopping = true;

	if(!mode&&id==id_animate_play) 
	return model_status(model,StatusError,STATUSTIME_LONG,
	sidebar.new_animation?"Animation unset":"Animation absent");

	//MainWin::modelChanged uses playing to 
	//filter AnimationFrame.
	//if(playing=!stop)
	if(!stopping)
	{
		playing = true;

		//TODO? Add 1/2 speed, etc. buttons.
		double step;
		step = 1/model->getAnimFPS(anim);
		/*Min or max?
		const double shortInterval = 1.0 / 20.0;
	    if ( m_timeInterval > shortInterval )
		 m_timeInterval = shortInterval;
		*/
		step = std::max(1/60.0,step); //min?

		step_t0 = glutGet(GLUT_ELAPSED_TIME);
		step_ms = (int)(step*1000);	

		//TEMPORARY
		glutTimerFunc(step_ms,animwin_step,win.glut_window_id());

		//log_debug("starting %s animation,update every %.03f seconds\n",
		//(mode==Model::ANIMMODE_JOINT?"skeletal":"frame"),step);

		//HACK: Woops! Draw the first frame?
		Impl::step();
	}
	else
	{
		playing = autoplay = false;
		
		win.model.views.playing1 = 0;

		model->setCurrentAnimationFrameTime(win.timeline,Model::AT_invalidateAnim);
		model->updateObservers();
	}

	//TODO: How to differentiate the pausing from stopping?
	int pic = pics[playing?pic_pause:pic_play];
	if(pic!=sidebar.play.picture())
	{
		sidebar.play.picture(pic).redraw();
		win.play.picture(pic).redraw();
	}

	glutSetMenu(win.menu);
	void *x = playing?glutext::GLUT_MENU_CHECK:glutext::GLUT_MENU_UNCHECK;
	glutext::glutMenuEnable(id_animate_play,x);
}
bool AnimWin::Impl::step()
{
	if(!playing) return false;

	int t = glutGet(GLUT_ELAPSED_TIME)-step_t0;

	//REMINDER: Inverting these to work like play/pause just isn't as intuitive.
	int loop = autoplay?0:sidebar.loop.picture()==pics[pic_loop];

	if(1) //EXPERIMENTAL
	{
		//Attempting to decouple m_currentFrame from playback.
		win.model.views.playing1 = ~loop;
		win.model.views.playing2 = t/1000.0;
	}
	else if(!model->setCurrentAnimationTime(t/1000.0,loop,Model::AT_invalidateAnim))
	{		
		stop();
	}

	model->updateObservers();

	return playing;
}

void AnimWin::Impl::refresh_item(bool undo)
{
	//log_debug("refresh anim window page\n"); //???

	bool wrap = false;
	bool op = win.graphic.m_operation!=0;
	double fps = 0;
	double frames = 0;
	if(!new_animation
	||-1==win.animation.int_val()) //NEW
	{	
		op = false;

		//NOTE: Don't disable the play/loop buttons.
		win.frame.disable();
		win.anim_nav.disable();
		win.zoom.nav.disable();
		win.fps.limit();
		win.close.enable();
	}
	else
	{
		win.nav.enable();
		win.fps.limit(Model::setAnimFPS_minimum,120); //1

		fps = model->getAnimFPS(anim);
		wrap = model->getAnimWrap(anim);
		frames = model->getAnimTimeFrame(anim);
	}
	win.anim_nav.set_hidden(op);
	win.tool_nav.set_hidden(!op);
	win.updateXY();
	win.fps.set_float_val(fps);
	win.wrap.set(wrap);
	win.frames.set_float_val(frames);	
	win.timeline.set_range(0,frames);
	sidebar.frame.limit(0,frames);
	win.frame.limit(0,frames);
	win.model.views.timeline.set_range(0,frames);
	win.x.limit(0,frames);

	//set_frame(frame)
	double f; if(!undo&&mode&&anim!=soft_anim)
	{
		soft_anim = anim;

		//HACK: First frame logic on animation change?		
		if(playing)
		{
			//Counter-argument? Maybe ending on the final
			//articulation should trump other concerns?
			//Or maybe the Animator window shouldn't play
			//the animation automatically?
			
			//Rationale: When setting the animation it's
			//useful to see its frame count, so this sets
			//it to the end. Whereas on the Animator view
			//the count is already displayed and it's set
			//up is on the left side, so it's better to be
			//at the beginning to be closer to the slider.
			//(which is true in both cases.)
			/*This makes less sense with the newer layout.
			if(&win==event.active_control_ui)
			f = 0;
			else*/
			f = model->getAnimTimeFrame(anim);
		}
		else f = model->getCurrentAnimationFrameTime();
	}
	else f = soft_frame; set_frame(f);

	if(mode!=soft_mode) switch(soft_mode=mode)
	{
	case Model::ANIMMODE_NONE:

		if(soft_mode_bones) model->setDrawJoints(soft_mode_bones); break;

	default: model->setDrawJoints(mode&Model::ANIMMODE_JOINT); break;
	}		
}

void AnimWin::Impl::refresh_undo()
{
	if(!model->inAnimationMode()) //???
	{
		return close();
	}

	anim = model->getCurrentAnimation();
	mode = item_mode(anim);
	frame = model->getCurrentAnimationFrame();
	soft_frame = model->getCurrentAnimationFrameTime();

	win.animation.select_id(anim_item());
	sidebar.animation.select_id(anim_item());

	//FIX ME
	/*OVERKILL (Seeing if doing without has any effect.)
	//UNDOCUMENTED ISN'T THIS REDUNDANT???
	model->setCurrentAnimation(anim,mode);
	model->setCurrentAnimationFrame(frame,Model::AT_invalidateNormals); //???*/

	refresh_item(true);
}

void AnimWin::Impl::close()
{
	if(model->inAnimationMode())
	{
		model->setNoAnimation();
		model->operationComplete(::tr("End animation mode","operation complete"));
	}
		
	//NEW: Keeping open.
	//NOTE: If modelChanged is implemented this is uncalled for.
	mode = Model::ANIMMODE_NONE;
	//2022: These need to be remembered for toggling animations
	//on and off.
	//anim = 0;
	//frame = 0;
	//soft_frame = 0; //This too?
	win.animation.select_id(-1);
	sidebar.animation.select_id(-1);
	refresh_item();

	//emit animWindowClosed();
	//emit(this,animWindowClosed);
	{	
		//model->setNoAnimation(); //Done above.

//		win.hide(); //Or close? Should remember animation/frame.
		
		//views.modelUpdatedEvent(); //Probably unnecessary.

		//editEnableEvent(); //UNUSED?

		animwin_enable_menu(-win.menu,win.model.animate_insert);	
	}
}

static void animwin_reshape(Win::ui *ui, int x, int y)
{
	assert(!ui->hidden()); //DEBUGGING

	auto *w = (AnimWin*)ui;
	auto &r = w->_reshape;

	if(!ui->seen())
	{
		r[0] = x-w->nav.span(); 
		r[1] = y-w->status.drop();
		r[2] = x;
		r[3] = r[1]+25; //y
		return;
	}

	if(x<r[2]) x+=r[2]-x; 
	if(y<r[3]) y+=r[3]-y; 

	w->nav.lock(x-r[0],0);
	w->status.lock(0,y-r[1]);
}

void AnimWin::Impl::draw_tool_text()
{
	utf8 str; switch(win.graphic.m_operation)
	{
	default: return;
	case graphic::MouseMove: str = "Move"; break;
	case graphic::MouseSelect: str = "Select"; break;
	case graphic::MouseScale: str = "Scale"; break;
	}
	glColor3ub(32,32,32); //bold?
	int x = win.scene.width()-win.scene.str_span(str)-8;
	int y = win.scene.height()-win.scene.font().height-1;
	win.scene.draw_str(x,y,str);
	win.scene.draw_str(x-1,y,str); //bold?
}
void AnimWin::_show_tool(int id)
{
	int cur = graphic.m_operation;
	switch(cur)
	{
	case graphic::MouseMove: move_snap.set_hidden(true); break;
	case graphic::MouseScale: scale.set_hidden(true); break;
	case graphic::MouseSelect: select.set_hidden(true); break;
	
	}	
	switch(id)
	{
	case graphic::MouseMove: move_snap.set_hidden(false); break;
	case graphic::MouseScale: scale.set_hidden(false); break;
	case graphic::MouseSelect: select.set_hidden(false); break;
	}

	//I don't know what to do with move/scale
	//for the time being.
	if(id&&-1!=key.int_val()) 
	{
		key.set_hidden(id!=graphic::MouseSelect);
		y.set_hidden(id!=graphic::MouseSelect);
	}

	anim_nav.set_hidden(id!=0);
	tool_nav.set_hidden(id==0);	
}
void AnimWin::_sync_tools(int tt, int arg)
{
	int t = (int)graphic.m_operation;
	switch(t)
	{
	case graphic::MouseNone: t = Tool::TT_NullTool; break;
	case graphic::MouseMove: t = Tool::TT_MoveTool; break;
	case graphic::MouseSelect: t = Tool::TT_SelectTool; break;
	case graphic::MouseScale: t = Tool::TT_ScaleTool; break;
	default: return;
	}
	if(t==tt) //return;
	{
		if(tt==Tool::TT_SelectTool&&arg!=(int)select)
		goto select;
		return;
	}

	switch(tt)
	{
	default: return;
	case Tool::TT_NullTool:  t = graphic::MouseNone; break; 
	case Tool::TT_SelectTool: t = graphic::MouseSelect; break;
	case Tool::TT_MoveTool: t = graphic::MouseMove; break;
	case Tool::TT_ScaleTool: t = graphic::MouseScale; break;
	}

	_show_tool(t);
	graphic.setMouseOperation((graphic::MouseE)t);

	if(!tt) t = id_tool_none;
	glutSetMenu(impl->_tool_menu);
	glutext::glutMenuEnable(t,glutext::GLUT_MENU_CHECK);

	if(tt==Tool::TT_SelectTool) select:
	{
		arg = arg>=1&&arg<=2?arg:0;

		select.set_int_val(arg);
		graphic.setMouseSelection((graphic::SelectE)arg);
		shrinkSelect();
	}
	else scene.redraw();
}
void AnimWin::_sync_anim_menu()
{
	assert(!hidden());

	std::string s;
	glutSetMenu(impl->_anim_menu); 
	int seps = 0;
	int iN = glutGet(GLUT_MENU_NUM_ITEMS);	
	int i,n = impl->new_animation;
	li::item *it = animation.first_item();
	for(i=0;i<=n;i++,it=it->next())
	{
		auto *str = it->c_str();
		if(!*str)
		{
			seps++; it = it->next();

			if(i+seps>=iN) glutAddMenuEntry();
			else glutChangeToMenuEntry(i+1,"",-1);
		}
		if(i<=10)
		{
			s = str; s.append("\t0").back()+=i; str = s.c_str();
		}
		int id = id_aw_anims+it->id()+1;
		if(i+seps>=iN) glutAddMenuEntry(str,id);
		else glutChangeToMenuEntry(1+seps+i,str,id);
	}
	while(i+seps<iN) glutRemoveMenuItem(iN--);
}
void AnimWin::init()
{	
	anim_nav.calign();
	tool_nav.calign();
	reshape_callback = animwin_reshape;

	tool_nav.set_hidden(true);
	//tool_nav.style(~0xa0000); //shadow top/bottom?
	tool_nav.style(~0x50000); //left/right?
	
	//This is used to let Tab hotkeys go to menus.
	//It depends on Win::Widget activating itself.
	scene.ctrl_tab_navigate();

	main->space(3,1,3,2,3); //2022		
	nav.cspace_all<center>();
	nav.space<bottom>(-1);
	frame_nav.space(0,1,0);
	anim_nav.space(3,4,3);
	tool_nav.space(3,4,3);
	select.cspace<top>();
	//HACK: -1 happens to lineup the label and the items.
	select.row_pack().place(mi::right).space(0,0,-5,-1,0);
	//1 is compensating for cspace and -4 is closing the
	//gap to the X box to meet the Select tool's spacing.
	scale_sfc.space<bottom>(1).space<right>()-=4;
	move_snap.space<bottom>(1).space<right>()-=4;
	
	fps.edit<double>(0,0).compact();		
	frames.edit<double>(0,INT_MAX).compact();		
	play.picture(pics[pic_play]);
	loop.picture(pics[pic_stop]);
	timeline.spin(0.0);
	//IMPLEMENT ME? (QSlider API)
	//Tickmarks can't currently be drawn on a regular slider.
	//timeline.style(bar::sunken|bar::tickmark|behind);
	timeline.style(bar::shadow);
	extern int viewwin_tick(Win::si*,int,double&,int);
	timeline.set_tick_callback(viewwin_tick);
	frame.spinner.set_tick_callback(viewwin_tick);
	frames.spinner.set_speed();
	fps.spinner.set_speed();
	
	#ifdef NDEBUG
//		#error should probably remove animation
	#endif
	//This has to be fixed one way or other.
	/*Trying to remove this by using the menu bar instead since
	//it's not clearly labeled if the window titlebar no longer
	//says Animation.
	animation.lock(m->sidebar.anim_panel.animation.span(),0);*/
	animation.set_hidden();

	//YUCK: Negative margins increase height.
	//(I don't know why negative is required.)
	//timeline.drop(play.drop()).expand();
	timeline.space<bottom>(-4).expand().drop()+=4; //25
	frame.edit(0.0,0.0,0.0).compact();
	frame.drop()++;
	del.span(30); close.span(30);
		 
	//HACK: This is to make the canvas flush
	//with the panel border.
	scene.space(-4,0,-4,-2,-4);
	status.expand();

	graphic.setModel(model);
	graphic.label_height = scene.font().height-3; //FUDGE

	//HACK: initially show F1 help message?
	status.int_val() = 25; status.lock(0,25);

	x.edit(0.0,0.0,0.0).compact();
	x.space<bottom>(4); //cspace
	x.space<left>()+=3;
	x.spinner.set_speed();		
	x.set_hidden(); 
	y.edit(0.0).compact();
	y.space<bottom>(4); //cspace
	//Don't need this without "Y" the label.
	//y.space<left>()+=3;
	//NOTE: If compact() is used this will have to overcome
	//a 7 pixel margin that update_area erases.
	y.span() = 79; //1 more digit?
	y.update_area(); //TESTING
	y.set_hidden(); 

	select.set_hidden();
	move_snap.set_hidden();
	scale.set_hidden();		
	key.int_val() = -1;
	key.set_hidden();
	key.row_pack().space(2,-5,-3);
	int bt = rx.style_tab|rx.style_thin|rx.style_hide;
	for(auto*p=&rx;p<=&tz;p++) p->style(bt).span(10);	

	//This is so the "shadow" bars around tool_nav run the
	//full height of the crossbar.
	int nh = nav.pack().drop();
	tool_nav.lock(0,nh); key.lock(0,nh).space(-1,-1);

	assert(!impl);
	impl = new Impl(*this);

	nav.lock(true,true);

	return open(false);
}
AnimWin::~AnimWin()
{
	//log_debug("Destroying AnimWin()\n"); //???

	delete impl;	
}
void AnimWin::submit(int id)
{
	MainWin *w = &model;
	Model *m = model.operator->();

	switch(id)
	{
	case id_help: //id_f1

		//aw->f1.execute_callback();
		win_help(typeid(AnimWin).name(),this);
		break;

	case id_scene:
	
		if(!impl->mode) 
		{
			scene.draw_active_name(0,3,f1_titlebar::msg());
			//mousePressSignal won't be called when clicking F1 if
			//not initialized (this isn't necessary but I want to
			//document this and avoid glTranslated for right now.)
			if(graphic.m_viewportWidth) break;
		}
		//INVESTIGATE ME
		//GL_LINE_SMOOTH has some very slight ghosting going on on my
		//system... it's probably to do with the Widgets 95 projection
		//setup... which has a lot of fudging for different use cases.
		//
		//FYI this is identical to Xming_x_server_y in set_ortho_projection
		//It's now disabled unless Xming is the server, which I'm not trying
		//to include here.
		//glTranslated(0,0.1,0);
		graphic.draw(scene.x(),scene.y(),scene.width(),scene.height());
		impl->draw_tool_text();
		//glTranslated(0,-0.1,0);

		break;

	case 'X': 
	
		graphic.moveSelectedFrames(x.float_val()-centerpoint[0]); 
		updateCoordsDone();
		break;

	case 'Y':
	{
		int k = key;
		double c = y; switch(k)
		{
		case 3: case 4: case 5: c*=PIOVER180; break;
		}		
		//graphic doesn't know about the key filter.
		//graphic.moveSelectedVertex(c-centerpoint[1]); 
		{
			c-=centerpoint[1];
			auto *a = m->getAnimationList()[impl->anim];

			int dim = k%3;
			int e = 1<<k/3;
			for(auto&ea:a->m_keyframes)
			for(auto*kp:ea.second)
			if(kp->m_selected[dim])
			if(e==kp->m_isRotation)
			{
				double p[3];
				memcpy(p,kp->m_parameter,sizeof(p));
				p[dim]+=c;
				m->setKeyframe(impl->anim,kp,p);
			}
		}		
		updateCoordsDone();
		break;
	}
	case id_key:

		updateXY(false);
		break;

	case id_select:

		graphic.setMouseSelection((graphic::SelectE)select.int_val());
		model._sync_tools(Tool::TT_SelectTool,select);
		shrinkSelect();
		break;

	case id_stf:

		graphic.setMoveToFrame(move_snap);
		break;

	case id_sfc:

		graphic.setScaleFromCenter(scale_sfc);
		break;

	case '+': graphic.zoomIn(); break;
	case '-': graphic.zoomOut(); break;
	case '=': graphic.setZoomLevel(zoom.value); break;
	
	case id_animate_copy:

		glutSetMenu(menu);
		{
			void *l = 0; 
			if(impl->copy())		
			l = glutext::GLUT_MENU_ENABLE;
			glutext::glutMenuEnable(id_animate_paste,l);
			glutext::glutMenuEnable(id_animate_paste_v,l);
		}
		break;

	case id_animate_paste:
	case id_animate_paste_v:

		impl->paste(id==id_animate_paste_v);
		break;

	case id_animate_delete: //clearFrame
		
		if(m->getCurrentAnimationFrameTime()
		==m->getAnimFrameTime(impl->anim,impl->frame))
		m->deleteAnimFrame(impl->anim,impl->frame);
		else 
		return model_status(m,StatusError,STATUSTIME_LONG,"The current time isn't an animation frame.");
		m->operationComplete(::tr("Clear frame","Remove animation data from frame,operation complete"));
		break;	

	case id_item:

		impl->anim_selected(animation,true);
		break;

	case id_subitem:

		impl->set_frame(frame);
		break;

	case id_delete:

		if(!del.hidden())
		impl->anim_deleted(animation);
		break;		
		
	case id_anim_fps:

		//log_debug("changing FPS\n"); //???
		if(m->setAnimFPS(impl->anim,fps))
		m->operationComplete(::tr("Set FPS","Frames per second,operation complete"));
		break;

	case id_check: //id_anim_loop

		//log_debug("toggling loop\n"); //???
		m->setAnimWrap(impl->anim,wrap);
		m->operationComplete(::tr("Set Wrap","Change whether animation wraps operation complete"));
		break;
	
	case id_animate_mode:

		if(!impl->mode&&impl->anim==(unsigned)-1)
		{
			auto am = Model::AnimationModeE(w->animation_mode);
			impl->anim = am==Model::ANIMMODE?0:m->getAnimationIndex(am);
		}
		if(m->setCurrentAnimation
		(impl->anim,Model::AnimationModeE(impl->mode?0:w->animation_mode)))
		{
			m->operationComplete(::tr("Animator Mode"));

			if(impl->mode&&hidden())
			w->sidebar.anim_panel.nav.set();
		}
		else event.beep();
		impl->open2(true);
		break;

	case id_animate_play: 
		if(!impl->mode) //NEW: Play first animation?
		{
			if(!m->getAnimationCount())
			return event.beep();
			w->sidebar.anim_panel.nav.set();
			impl->soft_anim = ~0;			
			//HACK: This just happens to defeat the
			//logic that only does one round of play
			//on select so the loop button is honored.
			impl->playing = true;
			return impl->anim_selected(0);
		}
		//break;
	case id_animate_stop: //This is now pseudo.

		impl->play(id);
		break;

	case id_anim_frames:

		impl->frames_edited(frames);
		break;

	case id_bar:

		impl->set_frame(timeline);
		break;

		//HACK: This prevents pressing Esc so
		//the None tool can use it by default.
		//Note: id_close is preventing Return.
	case id_ok:
	case id_close:

		return hide(); 
	}
}

void AnimWin::updateSelectionDone()
{
	model->operationComplete(::tr("Animator Selection"));
}
void AnimWin::updateCoordsDone(bool done)
{
	if(done) model->operationComplete(::tr("Animator Position"));
}
void AnimWin::updateXY(bool update_keys)
{
	if(!impl->mode) return; //HAPPENS

	auto *a = model->getAnimationList()[impl->anim];
	auto &tt = a->m_timetable2020;
	auto &st = a->m_selected_frames;	
	double min[2] = {+DBL_MAX,+DBL_MAX};
	double max[2] = {-DBL_MAX,-DBL_MAX};		
	if(!st.empty())			
	for(auto i=tt.size();i-->0;) if(st[i])
	{
		min[0] = std::min(tt[i],min[0]);
		max[0] = std::max(tt[i],max[0]);
	}
	//for(int i=0;i<2;i++)
	//centerpoint[i] = (min[i]+max[i])*0.5;

	bool hid = min[0]==DBL_MAX;
	if(!hid)
	x.set_float_val(centerpoint[0]=(min[0]+max[0])*0.5);
	x.set_hidden(hid);	

	//if(hid) //THIS IS A MESS
	{
		int k = key;

		constexpr bool cmp[4] = {};

		if(update_keys)
		{
			int mask = 0x1ff;
			for(auto&ea:a->m_keyframes)
			for(auto*kp:ea.second)
			if(memcmp(kp->m_selected,cmp,sizeof(cmp)))
			{
				int e = kp->m_isRotation>>1;
				for(int i=3;i-->0;)
				if(kp->m_selected[i]) mask&=~(1<<(e*3+i));
			}
			rx.set_hidden(mask&(1<<3));
			ry.set_hidden(mask&(1<<4));
			rz.set_hidden(mask&(1<<5));
			sx.set_hidden(mask&(1<<6));
			sy.set_hidden(mask&(1<<7));
			sz.set_hidden(mask&(1<<8));
			tx.set_hidden(mask&(1<<0));
			ty.set_hidden(mask&(1<<1));
			tz.set_hidden(mask&(1<<2));

			if(mask==0x1ff) //key.clear();
			{
				key.select_id(-1); k = -1;
			}

			if(k==-1||0!=(mask&(1<<k))) 
			for(int i=0;mask;mask>>=1,i++) //!!
			{
				if(0==(mask&1))
				{
					key.select_id(i); k = i;
					break;
				}
			}
		}
		if(k!=-1) //!key.empty()
		{		
			int dim = k%3;
			int e = 1<<k/3;
			for(auto&ea:a->m_keyframes)
			for(auto*kp:ea.second)
			if(memcmp(kp->m_selected,cmp,sizeof(cmp)))		
			if(e==kp->m_isRotation)
			if(kp->m_selected[dim])
			{
				min[1] = std::min(kp->m_parameter[dim],min[1]);
				max[1] = std::max(kp->m_parameter[dim],max[1]);
			}
		
			double c = (min[1]+max[1])*0.5;

			centerpoint[1] = c;

			if(e==Model::KeyRotate) c/=PIOVER180;

			y.set_float_val(c);
		}

		//NOTE: _show_tool reproduces this logic.
		hid = k==-1||graphic.m_operation!=graphic::MouseSelect;
		key.set_hidden(hid); y.set_hidden(hid);
	}

	shrinkSelect();
}
void AnimWin::shrinkSelect()
{	
	bool hide = !x.hidden()||-1!=key.int_val();

	int s = select;
	select_frame.set_hidden(hide?s!=1:false);
	select_vertex.set_hidden(hide?s!=2:false);
	select_complex.set_hidden(hide?s!=0:false);
}