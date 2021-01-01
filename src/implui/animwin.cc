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
	shelf1(win.shelf1),shelf2(win.shelf2),
	sidebar(win.model.sidebar.anim_panel),
	new_animation(sidebar.new_animation),
	//sep_animation(sidebar.sep_animation),
	model(win.model),
	mode(),soft_mode(-1),anim(),soft_anim(~0),frame(),
	playing(win.model.playing),autoplay()
	{
		//See AnimPanel::refresh_list.
		//It's easier to let the animation window manage this since
		//it's opaque.
		int swap = sidebar.animation;
		sidebar.animation.clear();
		sidebar.animation.reference(shelf1.animation);				
		sidebar.refresh_list();
		sidebar.animation.select_id(swap);
		shelf1.animation.select_id(-1);
	}

	AnimWin &win;
	shelf1_group &shelf1;
	shelf2_group &shelf2;
	SideBar::AnimPanel &sidebar;
	const int &new_animation; 
	//const int &sep_animation;

	Model *model;
	Model::AnimationModeE mode;
	int soft_mode;
	bool soft_mode_bones;
	unsigned anim;
	unsigned soft_anim;
	unsigned frame;	
	double soft_frame;
	bool &playing,autoplay;
	int step_t0;
	int step_ms;
	
	void open2(bool undo);
	void refresh_item();
	void refresh_undo();

	void close();

	void play(int=0);
	void stop(){ play(id_animate_stop); }
	bool loop();
	bool step();

	void anim_selected(int, bool local=false);
	void anim_deleted(int);

	void frames_edited(double);	
	void set_frame(double);
 
	bool copy(bool selected);
	void paste(bool values);


		//UTILITIES//

		
	Model::AnimationModeE item_mode(int i)
	{
		if(i<0) return Model::ANIMMODE_NONE;
		/*
		if(i>=sep_animation&&i<new_animation)
		return Model::ANIMMODE_FRAME;
		return Model::ANIMMODE_SKELETAL;
		*/
		i = model->getAnimType(i);
		if(i==3)
		i = win.model.animation_mode;
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
		unsigned object; 
		
		KeyframeData data[3];
	};
	struct VertexFrameCopy : KeyframeData
	{
		unsigned vertex;
	};
	/*struct FramePointCopy
	{
		unsigned point; double x,y,z,rx,ry,rz;
	};*/	
	std::vector<KeyframeCopy> copy1;
	std::vector<VertexFrameCopy> copy2;
	std::vector<KeyframeCopy> copy3; //FramePointCopy
};

/*UNIMPLEMENTED
static int animwin_tick_interval(int val)
{
	//Worth it? There's a big difference in 5000 and 1000??

	if(val>=25000) return 5000; if(val>=10000) return 1000;
	if(val>=2500) return 500;   if(val>=1000) return 100;
	if(val>=250) return 50;     if(val>=100) return 10;
	if(val>=25) return 5;       return 1;
}*/

extern void animwin_enable_menu(int menu, int clipboard)
{
	extern void viewwin_status_func(int=0);
	viewwin_status_func();

	void *off,*on = 0; 
	if(menu<=0) menu = -menu;
	else on = glutext::GLUT_MENU_ENABLE;
	off = !on?glutext::GLUT_MENU_ENABLE:0;
		
	if(menu) glutSetMenu(menu);

	//glutext::glutMenuEnable(id_animate_play,on);
	if(clipboard) on = 0;
	glutext::glutMenuEnable(id_animate_copy,on); 
	glutext::glutMenuEnable(id_animate_copy_all,on); 
	glutext::glutMenuEnable(id_animate_paste,0); // Disabled until copy occurs
	glutext::glutMenuEnable(id_animate_paste_v,0); // Disabled until copy occurs
	glutext::glutMenuEnable(id_animate_delete,on); 
}

void AnimWin::open(bool undo)
{	
	//animwin_enable_menu(menu,win.model.clipboard_mode);

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
	animwin_enable_menu(mode?win.menu:-win.menu,win.model.clipboard_mode);

	shelf1.animation.select_id(anim_item());
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

	int was = shelf1.animation;

	if(item==was)
	{
		if(!local)
		{
			double cmp = sidebar.frame;
			if(cmp!=shelf2.timeline.float_val()) 
			set_frame(cmp);
			return;
		}		
	}
	else shelf1.animation.select_id(item);
	
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
		animwin_enable_menu(win.menu,win.model.clipboard_mode);
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

			//mode = type?Model::ANIMMODE_FRAME:Model::ANIMMODE_SKELETAL;
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
			shelf1.animation.select_id(anim_item());
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

void AnimWin::Impl::frames_edited(double n)
{	
	bool op = model->setAnimTimeFrame(anim,n);
	if(!op) n = model->getAnimTimeFrame(anim);
	if(!op) shelf1.frames.set_float_val(n);

	//assert(n==shelf1.frames.int_val());

	//int nn = std::max(0,n-1);
	shelf2.timeline.set_range(0,n); //nn
	win.model.views.timeline.set_range(0,n); //nn
	//sidebar.frame.limit(n?1:0,n);
	sidebar.frame.limit(0,n);
	
	//HACK: Truncate?
	if(sidebar.frame.float_val()>n) set_frame(n);

	if(op) model->operationComplete(::tr("Change Frame Count","operation complete"));
}

void AnimWin::Impl::set_frame(double i)
{
	bool ok = true; if(playing) 
	{
		if(&win==event.active_control_ui) //HACK
		{
			//YUCK: Put AnimWin back at the begginging so the 
			//slider is not out of reach.
			i = 0;
		}
		else
		{
			//New: Playing ends at the end now, which is useful
			//to show the length of the animation and puts the
			//slider closer to the Animation panel.
			i = model->getAnimTimeFrame(anim);
		}
	}
	else
	{
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

		//https://github.com/zturtleman/mm3d/issues/90
		//DecalManager::getInstance()->modelUpdated(model); //???
		model->updateObservers();
	}
	if(ok)
	{
		if((int)i==i)
		shelf2.timeline.name().format("%s\t%d",::tr("Frame: "),(int)i);
		else shelf2.timeline.name().format("\t%g",i);
	}
	else shelf2.timeline.name(::tr("Frame: \tn/a"));

	sidebar.frame.set_float_val(i);
	win.model.views.timeline.set_float_val(i);
	
	//shelf2.timeline.name().push_back('\t');

	//frame = i;
	frame = model->getAnimFrame(anim,i);
	soft_frame = i;
	shelf2.timeline.set_float_val(i); //NEW!
}

bool AnimWin::Impl::copy(bool selected)
{
	copy1.clear(); copy2.clear(); copy3.clear();

	double t = model->getCurrentAnimationFrameTime();

	if(mode&Model::ANIMMODE_SKELETAL)
	{	
		KeyframeCopy cp;

		size_t numJoints = model->getBoneJointCount();
		copy1.reserve(numJoints);

		for(Model::Position jt{Model::PT_Joint,0};jt<numJoints;jt++)
		if(!selected||model->isBoneJointSelected(jt))			
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

				cp.object = jt; copy1.push_back(cp);
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
		VertexFrameCopy cp;

		size_t numVertices = model->getVertexCount();
		copy2.reserve(numVertices);

		for(size_t v=0;v<numVertices;v++)		
		if(!selected||model->isVertexSelected(v))
		{
			//https://github.com/zturtleman/mm3d/issues/127
			//if(model->getFrameAnimVertexCoords(anim,frame,v,cp.x,cp.y,cp.z))
			{
				//cp.e = model->getFrameAnimVertexCoords(anim,frame,v,cp.x,cp.y,cp.z);
				cp.e = model->hasFrameAnimVertexCoords(anim,frame,v);
				model->getVertexCoords(v,&cp.x);

				cp.vertex = v; copy2.push_back(cp);
			}		
		}

		//FramePointCopy cpt;
		KeyframeCopy cpt; 

		size_t numPoints = model->getPointCount();
		copy3.reserve(numPoints);

		for(Model::Position pt{Model::PT_Point,0};pt<numPoints;pt++)
		if(!selected||model->isPointSelected(pt))
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

				cpt.object = pt; copy3.push_back(cpt);
			}
		}

		if(copy2.empty()&&copy3.empty())
		if(~mode&Model::ANIMMODE_SKELETAL) //2021
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
	if(!mode) return;

	int made = false;

	if(mode==Model::ANIMMODE_SKELETAL
	 ||mode&Model::ANIMMODE_SKELETAL&&!copy1.empty())
	{
		if(copy1.empty())
		{
			//msg_error(::tr("No skeletal animation data to paste"));
			model_status(model,StatusError,STATUSTIME_LONG,"No skeletal animation data to paste");		
			return;
		}

		if(!made++)
		frame = model->makeCurrentAnimationFrame();

		for(KeyframeCopy*p=copy1.data(),*d=p+copy1.size();p<d;p++)
		{
			Model::Position jt{Model::PT_Joint,p->object};
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
		if(copy2.empty()&&copy3.empty())
		{
			//msg_error(::tr("No frame animation data to paste"));
			model_status(model,StatusError,STATUSTIME_LONG,"No frame animation data to paste");		
			return;
		}

		if(!made++)
		frame = model->makeCurrentAnimationFrame();

		for(VertexFrameCopy*p=copy2.data(),*d=p+copy2.size();p<d;p++)
		{
			model->setFrameAnimVertexCoords(anim,frame,p->vertex,p->x,p->y,p->z,
			values&&p->e<Model::InterpolateStep?Model::InterpolateLerp:p->e);
		}

		/*for(FramePointCopy*p=copy3.data(),*d=p+copy3.size();p<d;p++)
		{
			model->setFrameAnimPointCoords(anim,frame,p->point,p->x,p->y,p->z);
			model->setFrameAnimPointRotation(anim,frame,p->point,p->rx,p->ry,p->rz);
		}*/
		for(KeyframeCopy*p=copy3.data(),*d=p+copy3.size();p<d;p++)
		{
			Model::Position pt{Model::PT_Point,p->object};
			for(int i=0;i<3;i++)
			{
				auto &cd = p->data[i];
				auto kt = Model::KeyType2020E(1<<i);
				model->setKeyframe(anim,frame,pt,kt,cd.x,cd.y,cd.z,
				values&&cd.e<Model::InterpolateStep?Model::InterpolateLerp:cd.e);
			}
		}	
	}
	if(copy1.empty()&&copy2.empty()&&copy3.empty())
	{
		model_status(model,StatusError,STATUSTIME_LONG,"No skeletal/frame animation data to paste");		
		return;
	}

	model->setCurrentAnimationFrame(frame,Model::AT_invalidateAnim);

	model->operationComplete(mode==Model::ANIMMODE_FRAME?
	::tr("Paste frame","paste frame animation position operation complete"):
	::tr("Paste keyframe","Paste keyframe animation data complete"));
}

static void animwin_step(int id) //TEMPORARY
{
	auto w = (AnimWin*)Widgets95::e::find_ui_by_window_id(id);
	if(w&&w->impl->step())
	glutTimerFunc(w->impl->step_ms,animwin_step,id);
}
void AnimWin::Impl::play(int id)
{
	bool stopping = id==id_animate_stop;

	if(playing&&id) stopping = true;

	if(!mode&&id==id_animate_play) 
	return model_status(model,StatusError,STATUSTIME_LONG,
	sidebar.new_animation?"Animation unset":"Animation absent");

	//shelf2.play.enable(stop); 
	//shelf2.stop.enable(!stop);
	shelf1.frames.enable(stopping);
	shelf2.timeline.enable(stopping);

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
		//(mode==Model::ANIMMODE_SKELETAL?"skeletal":"frame"),step);

		//HACK: Woops! Draw the first frame?
		Impl::step();
	}
	else
	{
		autoplay = false;

		model->setCurrentAnimationFrameTime(shelf2.timeline,Model::AT_invalidateAnim);
		model->updateObservers();

		playing = false;
	}

	//TODO: How to differentiate the pausing from stopping?
	int pic = pics[playing?pic_pause:pic_play];
	if(pic!=sidebar.play.picture())
	{
		sidebar.play.picture(pic).redraw();
		shelf2.play.picture(pic).redraw();
	}

	glutSetMenu(win.menu);
	void *x = playing?glutext::GLUT_MENU_CHECK:glutext::GLUT_MENU_UNCHECK;
	glutext::glutMenuEnable(id_animate_play,x);
}
bool AnimWin::Impl::loop()
{
	int pic = sidebar.loop.picture();
	pic = pic==pics[pic_stop]?pic_loop:pic_stop;
	sidebar.loop.picture(pics[pic]).redraw();
	shelf2.loop.picture(pics[pic]).redraw();
	glutSetMenu(win.menu);
	//REMINDER: Inverting these to work like play/pause just isn't as intuitive.
	void *x = pic==pic_loop?glutext::GLUT_MENU_CHECK:glutext::GLUT_MENU_UNCHECK;
	glutext::glutMenuEnable(id_animate_loop,x);
	return pic==pic_loop;
}
bool AnimWin::Impl::step()
{
	if(!playing) return false;

	int t = glutGet(GLUT_ELAPSED_TIME)-step_t0;

	//REMINDER: Inverting these to work like play/pause just isn't as intuitive.
	int loop = autoplay?0:sidebar.loop.picture()==pics[pic_loop];

	if(!model->setCurrentAnimationTime(t/1000.0,loop,Model::AT_invalidateAnim))
	{		
		stop();
	}

	model->updateObservers();

	return playing;
}

void AnimWin::Impl::refresh_item()
{
	log_debug("refresh anim window page\n"); //???

	bool loop = false;
	double fps = 0;
	double frames = 0;

	if(!new_animation
	||-1==shelf1.animation.int_val()) //NEW
	{	
		win.disable();
		shelf1.animation.enable();		

		//Leave pressable.
		shelf2.play.enable();
		shelf2.loop.enable();

		//hitting this on deleting last animation?
		//assert(!frame);
		//frame = 0;

		shelf1.fps.limit();
		shelf1.frames.limit();
	}
	else
	{
		shelf1.fps.limit(1,120); //???
		shelf1.frames.limit(0,INT_MAX);

		shelf1.del.enable();
		shelf1.fps.enable();
		shelf1.loop.enable();
		shelf1.frames.enable(!playing);
		//shelf2.play.enable(!playing);
		//shelf2.stop.enable(playing);
		shelf2.timeline.enable(!playing);

		fps = model->getAnimFPS(anim);
		loop = model->getAnimWrap(anim);
		frames = model->getAnimTimeFrame(anim);
	}

	shelf1.fps.set_float_val(fps);
	shelf1.loop.set(loop);
	shelf1.frames.set_float_val(frames);	
	shelf2.timeline.set_range(0,frames);
	sidebar.frame.limit(0,frames);
	win.model.views.timeline.set_range(0,frames);
	//IMPLEMENT ME? (QSlider API)
	//shelf2.timeline.setTickInterval(animwin_tick_interval(frames));

	//set_frame(frame)
	if(mode&&anim!=soft_anim)
	{
		soft_anim = anim;
		set_frame(model->getCurrentAnimationFrameTime());	
	}
	else set_frame(soft_frame);

	if(mode!=soft_mode) switch(soft_mode=mode)
	{
	case Model::ANIMMODE_NONE:

		if(soft_mode_bones) model->setDrawJoints(soft_mode_bones); break;

	default: model->setDrawJoints(mode&Model::ANIMMODE_SKELETAL); break;
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

	shelf1.animation.select_id(anim_item());
	sidebar.animation.select_id(anim_item());

	//FIX ME
	/*OVERKILL (Seeing if doing without has any effect.)
	//UNDOCUMENTED ISN'T THIS REDUNDANT???
	model->setCurrentAnimation(anim,mode);
	model->setCurrentAnimationFrame(frame,Model::AT_invalidateNormals); //???*/

	refresh_item();
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
	anim = 0;
	frame = 0;
	soft_frame = 0;
	shelf1.animation.select_id(-1);
	sidebar.animation.select_id(-1);
	refresh_item();
		
	//emit animWindowClosed();
	//emit(this,animWindowClosed);
	{	
		//model->setNoAnimation(); //Done above.

//		win.hide(); //Or close? Should remember animation/frame.
		
		//views.modelUpdatedEvent(); //Probably unnecessary.

		//editEnableEvent(); //UNUSED?

		animwin_enable_menu(-win.menu,win.model.clipboard_mode);	
	}
}

AnimWin::~AnimWin()
{
	log_debug("Destroying AnimWin()\n"); //???

	delete impl;	
}
void AnimWin::submit(int id)
{
	switch(id)
	{
	case id_init:
		
		log_debug("AnimWidget constructor\n"); //???
		
		shelf1.fps.edit(0.0);
		shelf1.frames.edit(0.0);
		shelf1.frames.limit(0,INT_MAX).compact();
		shelf2.play.span(60).picture(pics[pic_play]);
		shelf2.loop.span(60).picture(pics[pic_stop]);		
		shelf2.timeline.spin(0.0); //NEW (2020)
		//IMPLEMENT ME? (QSlider API)
		//Tickmarks can't currently be drawn on a regular slider.
		//shelf2.timeline.style(bar::sunken|bar::tickmark|behind);
		shelf2.timeline.style(bar::shadow);
		extern int viewwin_tick(Win::si*,int,double&,int);
		shelf2.timeline.set_tick_callback(viewwin_tick);
		
		//Line up Delete button with scrollbar.
		shelf2.nav.pack();
		shelf1.animation.lock(shelf2.timeline.active_area<0>()-12,false);

		//Make space equal to that above media buttons.
		//shelf2.timeline.space<top>(3).drop()+=2;
		//shelf2.timeline.drop(shelf2.play.drop());
		shelf2.timeline.space<top>(3).drop()+=7;
		shelf1.fps.space<top>(4);
		shelf1.loop.space<top>(5);
		shelf1.frames.space<top>(4);

		assert(!impl);
		impl = new Impl(*this);
		open(false);
		
		break;

	case id_animate_copy:
	case id_animate_copy_all: 

		glutSetMenu(menu);
		{
			void *l = 0; 
			if(impl->copy(id==id_animate_copy))		
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
		
		if(model->getCurrentAnimationFrameTime()
		==model->getAnimFrameTime(impl->anim,impl->frame))
		model->deleteAnimFrame(impl->anim,impl->frame);
		else 
		return model_status(model,StatusError,STATUSTIME_LONG,"The current time isn't an animation frame.");
		model->operationComplete(::tr("Clear frame","Remove animation data from frame,operation complete"));
		break;	

	case id_edit_undo: //REMOVE ME
		
		log_debug("anim undo request\n"); //???
		model->undo(); impl->refresh_undo();
		break;

	case id_edit_redo: //REMOVE ME
		
		log_debug("anim redo request\n"); //???
		model->redo(); impl->refresh_undo();
		break;
			
	case id_item:

		impl->anim_selected(shelf1.animation,true);
		break;

	case id_delete:

		impl->anim_deleted(shelf1.del);
		break;		
		
	case id_anim_fps:

		log_debug("changing FPS\n"); //???
		model->setAnimFPS(impl->anim,shelf1.fps);
		model->operationComplete(::tr("Set FPS","Frames per second,operation complete"));
		break;

	case id_check: //id_anim_loop

		//log_debug("toggling loop\n"); //???
		model->setAnimWrap(impl->anim,shelf1.loop);
		model->operationComplete(::tr("Set Wrap","Change whether animation wraps operation complete"));
		//WHAT'S THIS DOING HERE??? (DISABLING)
		//model->setCurrentAnimationFrame((int)shelf2.timeline,Model::AT_invalidateNormals);		
		break;
	
	case id_animate_mode:

		model.sidebar.anim_panel.nav.set(!impl->mode);
		model->setCurrentAnimation
		(impl->anim,Model::AnimationModeE(impl->mode?0:model.animation_mode));
		impl->open2(true);
		break;

	case id_animate_play: 
		if(!impl->mode) submit(id_animate_mode);
		//break;
	case id_animate_stop: //This is now pseudo.

		impl->play(id);
		break;		

	case id_animate_loop:
	
		if(impl->loop())
		if(!model.sidebar.anim_panel.loop)
		{
			//2021: Make Shift+Pause behavior 
			//analogous to Pause but keep the
			//graphical button as pure toggle.
			submit(id_animate_play);			
		}
		break;

	case id_anim_frames:

		impl->frames_edited(shelf1.frames);
		break;

	case id_bar:

		impl->set_frame(shelf2.timeline);
		break;

	case id_ok: case id_close:

		event.close_ui_by_create_id(); //Help?

		hide(); return;
	}
}
