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

#include "sidebar.h"

#include "model.h"
#include "log.h"
#include "modelstatus.h"

#include "viewwin.h"
#include "modelstatus.h"

//REMOVE US
//This is replacing old functionality.
//I think at most it's an optimization
//as is. I think Qt was the source of 
//the problem. But possibly Model too?
static int sidebar_updating = 0;
//REMOVE US
struct sidebar_updater //RAII
{
	sidebar_updater(){ sidebar_updating++; }

	~sidebar_updater(){ sidebar_updating--; }
};

SideBar::SideBar(class MainWin &model)
	:
Win(model.glut_window_id,subpos_right),
//NOTE: Compilers squeal if "this" is passed
//in the initializer's list.
anim_panel(model.sidebar,model),
prop_panel(model.sidebar,model),
bool_panel(model.sidebar,model)
{
	prop_panel.infl.grow_igv();
	prop_panel.infl.igv[0]->joint.lock(false,false);

	clip(true,false,true);

	dropdown *ll[] = //HACK
	{
	&anim_panel.animation,
	&prop_panel.faces.menu,
	&prop_panel.group.group.menu,
	&prop_panel.group.material.menu,
	&prop_panel.group.projection.menu,
	&prop_panel.infl.igv[0]->joint,
	};
	//for(size_t i=std::size(ll);i-->0;)
	for(size_t i=sizeof(ll)/sizeof(*ll);i-->0;)
	{
		ll[i]->lock(ll[i]->span(),false);
	}		

	anim_panel.nav.set(config->get("ui_anim_sidebar",true));
	bool_panel.nav.set(config->get("ui_bool_sidebar",true));
	prop_panel.nav.set(config->get("ui_prop_sidebar",true));

	prop_panel.infl.nav.set(config->get("ui_infl_sidebar",true));

	active_callback = &SideBar::submit;

	//Do this after faces menu is initalized
	//so its options won't change its size.
	prop_panel.init();
}
void SideBar::submit(control *c)
{
	void *v = c; 
	if(v>=&anim_panel&&v<&anim_panel+1)
	{
		if(c!=anim_panel.nav) anim_panel.submit(c);
		else config->set("ui_anim_sidebar",(bool)*c);
	}
	else if(v>=&bool_panel&&v<&bool_panel+1)
	{
		int i = *c;
		if(c!=bool_panel.nav) bool_panel.submit(c);
		else config->set("ui_bool_sidebar",(bool)*c);
	}
	else if(v>=&prop_panel&&v<&prop_panel+1)
	{
		if(c!=prop_panel.nav) prop_panel.submit(c);
		else if(config->set("ui_prop_sidebar",(bool)*c))
		{
			prop_panel.modelChanged(~0); //OPTIMIZATION
		}
		if(c==prop_panel.infl.nav)
		if(config->set("ui_infl_sidebar",(bool)*c))
		{
			prop_panel.modelChanged(~0); //OPTIMIZATION
		}
	}
	else if(!prop_panel.infl.nav.hidden())
	{
		assert(prop_panel.infl.nav);

		size_t i = (size_t)(c->id()-'0');
		if(i<prop_panel.infl.igv.size())
		{
			auto *cmp = prop_panel.infl.igv[i];
			if(v>=cmp&&v<cmp+1)
			{
				sidebar_updater raii; //REMOVE ME
				
				prop_panel.infl.submit(c);
			}
		}
	}
}

void SideBar::AnimPanel::submit(control *c)
{
	switch(int id=c->id())
	{		
	case id_init:
			
		extern int viewwin_tick(Win::si*,int,double&,int);
		frame.spinner.set_tick_callback(viewwin_tick);
  
		animation.expand();
		frame_nav.expand().space(1);
		frame.edit(0,0,0).compact().expand();
		frame.edit(0.0); //BLACK MAGIC?
		window.drop(frame.drop()).span(0).ralign();

		uv_window.drop(frame.drop()).span(0).ralign();
		uv_nav.cspace<center>();
		uv_nav.expand().space(1);		
		uv_animate.expand().place(right);
		uv_animate.set(config->get("uv_animations",true));

		//refresh_list(); //Won't do.

		break;

	case 'u':

		config->set("uv_animations",(bool)uv_animate);		
		model->setDrawOption(Model::DO_TEXTURE_MATRIX,uv_animate);
		break;

	case 'v':

		model.perform_menu_action(id_edit_utildata);
		break;
	
	case id_animate_play: 
	case id_animate_loop:
	case id_animate_settings:

		model.perform_menu_action(id);
		break;		

	case id_item:
	case id_subitem:

		model.open_animation_system();
		break;
	}
}
void SideBar::AnimPanel::refresh_list()
{
	bool uv = false;
	for(auto*up:model->getUtilityList())
	if(up->type==Model::UT_UvAnimation) 
	uv = true;
	uv_nav.set_hidden(!uv);

	//See AnimWin::Impl::Impl.
	//It's easier to let the animation window manage this since
	//it's opaque.
	dropdown *r = animation.reference();
	if(!r) r = &animation;

	//sep_animation = model->getAnimationCount(Model::ANIMMODE_JOINT);
	//new_animation = model->getAnimationCount(Model::ANIMMODE_FRAME);
	new_animation = model->getAnimationCount();
	//new_animation+=sep_animation;

	enum{ z=2 }; //TODO: Let i18n/l10n disable this.

	r->clear();
	r->add_item(-1,::tr("<None>"));
	auto ins = r->first_item(); //<None>
	auto sep = Model::ANIMMODE_NONE,cmp=sep; 
	for(int i=0;i<new_animation;i++,sep=cmp)
	{
		//HACK: cmp==z is because I've moved Frame
		//to be in front of Skeletal in the UI just
		//because the layout looks nicer.
		cmp = model->getAnimType(i); 
		if(cmp!=sep&&sep)
		{
			auto it = new li::item;
			if(cmp==z) r->insert_item(it,ins,li::behind);
			else r->add_item(it);
		}
		auto it = new li::item(i,model->getAnimName(i));
		if(cmp==z) r->insert_item(it,ins,li::behind);
		if(cmp==z) ins = it;
		else r->add_item(it);
	}
	r->add_item(new_animation,::tr("<New Animation>"));

	auto m = model->getAnimationMode();
	int a = model->getCurrentAnimation();
	//if(m==Model::ANIMMODE_FRAME) a+=sep_animation;
	
	if(r!=&animation)
	{
		animation.clear();
		animation.reference(*r);
	}
	animation.select_id(m?a:-1);

	//WARNING: I believe this is only used to update
	//the window menubar list.
	model.sync_animation_window();
}

void SideBar::BoolPanel::submit(control *c)
{
	if(c==op)
	{
		config->set("ui_bool_op",op.int_val());

		utf8 str = nullptr; switch(op)
		{
		case op_union:
		//B replaces "Selected" to shorten the button.
		str = ::tr("Fuse With B","boolean operation");
		break;
		case op_union2:
		str = ::tr("Union With B","boolean operation");
		break;
		case op_subtract:
		str = ::tr("Subtract B","boolean operation");
		break;
		case op_intersect:
		str = ::tr("Intersect With B","boolean operation");
		break;
		}
		button_b.set_name(str);
	}
	else if(c==button_a)
	{
		int_list tris;
		model->getSelectedTriangles(tris);
		if(!tris.empty())
		{
			button_b.enable();
			//status.text().format("Object: %d Faces",(int)tris.size());
			//status.set_text(status);
			model->clearMarkedTriangles();			
			for(int_list::iterator it=tris.begin();it!=tris.end();it++)
			{
				model->setTriangleMarked(*it,true);
			}
		}
		else button_b.disable();
	}
	else if(c==button_b)
	{	
		int i,iN = model->getTriangleCount();

		int_list al,bl; for(i=0;i<iN;i++)
		{
			if(model->isTriangleSelected(i))
			{
				bl.push_back(i);
			}
			else if(model->isTriangleMarked(i))
			{
				al.push_back(i);
			}
		}
		utf8 str; if(!al.empty()&&!bl.empty())
		{			
			Model::BooleanOpE e = Model::BO_Union; 
			switch(op)
			{
			case op_union2: e = Model::BO_UnionRemove;
			case op_union:
			str = ::tr("Union","boolean operation");
			break;
			case op_subtract: e = Model::BO_Subtraction; 
			str = ::tr("Subtraction","boolean operation");
			break;
			case op_intersect: e = Model::BO_Intersection; 
			str = ::tr("Intersection","boolean operation");
			break;
			}
			model->booleanOperation(e,al,bl);
			model->operationComplete(str);

			button_b.disable();
		}
		else 
		{
			if(!bl.empty()) 
			str = ::tr("Object A triangles are still selected");
			else 
			str = ::tr("You must have at least once face selected");

			model_status(model,StatusError,STATUSTIME_LONG,str);
		}
	}
}
void SideBar::BoolPanel::init()
{
	//Maximizing use of space.
	//op.style(bi::etched).expand();
	//op.align();
	op.calign().space(2,0,0);
	op.add_item(0,"Fuse");
	op.add_item(1,"Union");
	col.set_parent(op);
	op.add_item(2,"Subtract"); //Subtraction
	op.add_item(3,"Intersect"); //Intersection

	button_a.expand();
	button_b.expand();

	//init(); submit(op.select_id(1));

	submit(op.select_id(config->get("ui_bool_op",1)));

	//status.set_name(::tr("Select faces to set",
	//"Select faces to set as 'A' Object in boolean operation"));
	button_b.disable();
}

void SideBar::PropPanel::init()
{
	pos.dimensions.disable();

	for(int i=0;i<3;i++)
	{
		interp.menu[i].set_parent(interp.nav).id('t').expand();
		interp.menu[i].add_item(Model::InterpolateNone,"<None>");
	}
	interp.menu[0].add_item(Model::InterpolateCopy,"<Position>");
	interp.menu[0].add_item(Model::InterpolateLerp,"Lerp Position");
	interp.menu[0].add_item(Model::InterpolateStep,"Step Position");		
	interp.menu[1].add_item(Model::InterpolateCopy,"<Rotation>");
	interp.menu[1].add_item(Model::InterpolateLerp,"Lerp Rotation");
	interp.menu[1].add_item(Model::InterpolateStep,"Step Rotation");
	interp.menu[2].add_item(Model::InterpolateCopy,"<Scale>");
	interp.menu[2].add_item(Model::InterpolateLerp,"Lerp Scale");
	interp.menu[2].add_item(Model::InterpolateStep,"Step Scale");
	proj.type.menu.add_item(0,"Cylinder");
	proj.type.menu.add_item(1,"Sphere");
	proj.type.menu.add_item(2,"Plane");

	faces.menu.add_item(0,"");
	faces.menu.add_item(1,"Move to Top of Display List");
	faces.menu.add_item(2,"Move to End");
	faces.menu.add_item(3,"Transpose with Reverse List");
	//IN A PINCH (2022)
	//there's no other way to peel off overlapping triangles?
	//NOTE: it seems necessary to use the Hide command after
	//isolating the face or else it pulls in its Vertices.
	//Using (3) (Transpose) can isolate the last face.
	faces.menu.add_item(4,"Keep First Face");
}
void SideBar::PropPanel::submit(control *c)
{
	int id = c->id();

	//2022: don't set sidebar_updating for dialogs
	//so they don't block change notice processing
	switch(id)
	{
	case id_group_settings: 
	case id_material_settings: 
	case id_projection_settings:

		return model.perform_menu_action(id);
	}

	sidebar_updater raii; //REMOVE ME

	switch(id)
	{
	case id_name:

		name.change(); break;

	case 'X': case 'Y': case 'Z':

		if(auto cc=c->parent())
		{
			if(cc==&pos.nav) pos.change();
			if(cc==&rot.nav) rot.change();
			if(cc==&scale.nav) scale.change(0,c);
			if(cc==&proj.nav) proj.change();
		}
		break;

	case 't': 

		interp.change(0,c); break;

	case -id_projection_settings:

		if(c>proj.props_base::nav)
		{
			proj.change(); break;
		}
		//break;

	case -id_group_settings: 
	case -id_material_settings:
	
		group.submit(-id); break;

	/*case '0': case '1': case '2': case '3':
	
		infl.submit(c); break;*/

	case id_up: //2021

		for(unsigned i:model.selection)
		model->setBoneJointParent(i,infl.parent_joint);
		model->operationComplete(::tr("Reparent","operation complete"));
		break;

	case '#': //2021

		faces.change(); break;
	}
}

	///// PROPERTIES //// PROPERTIES //// PROPERTIES ////

void SideBar::PropPanel::modelChanged()
{
	modelChanged(Model::ChangeAll);
}
void SideBar::PropPanel::modelChanged(int changeBits)
{	
	//THIS COMMENT IS NO LONGER TRUE
	//NOTE: This can happen now because AnimationFrame
	//is suppressed during animation playback. Because
	//changes are deferred it's not possible to filter
	//it here.
	if(!changeBits) 
	{
		return; //FIX ME
	}
	
	if(sidebar_updating) return; //REMOVE ME

	if(!nav) return; //OPTIMIZATION

	//log_debug("modelChanged()\n"); //???
			
	auto mode = model->getAnimationMode();

	Model::Position ss = {};
	size_t sz = model.selection.size();
	if(1==sz) ss = model.selection[0];

	bool show = ss.type!=Model::PT_Vertex;
	if(mode&&ss.type==Model::PT_Projection)
	show = false;
	name.nav.set_hidden(!show);
	if(show) name.change(changeBits);

	// Position should always be visible
	pos.change(changeBits);

	// Only allow joints in None and Skel	
	if(show) switch(ss.type)
	{
	case Model::PT_Point:
		show = mode!=Model::ANIMMODE_JOINT;
		break;
	case Model::PT_Joint:
		show = mode!=Model::ANIMMODE_FRAME;
		break;
	case Model::PT_Projection:
		show = mode==Model::ANIMMODE_NONE;
		break;
	}
	rot.nav.set_hidden(!show);
	if(show) rot.change(changeBits);

	if(ss.type==Model::PT_Projection)
	show = false;
	scale.nav.set_hidden(!show);
	if(show) scale.change(changeBits);

	show = mode?sz!=0:false;
	if(mode==Model::ANIMMODE_JOINT)
	for(auto&i:model.selection) 
	if(i.type!=Model::PT_Joint)
	show = false;
	if(mode==Model::ANIMMODE_FRAME)
	for(auto&i:model.selection) 
	if(i.type!=Model::PT_Vertex&&i.type!=Model::PT_Point)
	show = false;
	interp.nav.set_hidden(!show);
	if(show) interp.change(changeBits);

	show = !model.fselection.empty();
	if(show||changeBits&Model::AddOther)
	{
		group.change(changeBits);
	}		
	group.nav.set_hidden(!show);

	auto &sn = model.nselection;

	show = !mode&&sn[Model::PT_Projection];
	proj.nav.set_hidden(!show);
	if(show) proj.change(changeBits);
	
	// Only show influences if there are influences to show
	if(model->getBoneJointCount())
	show = sn[Model::PT_Vertex]||sn[Model::PT_Point];
	else show = false;
	infl.nav.set_hidden(!show);
	bool show2 = sz&&sz==sn[Model::PT_Joint];
	infl.parent_joint.set_hidden(!show2);
	if(show||changeBits&Model::AddOther)
	{
		infl.change(changeBits);	
	}
	if(show2) //2021
	{
		//Note, this is for joints, not influences.
		int cmp = -2;
		for(int i:model.selection)
		{
			int p = model->getJointList()[i]->m_parent;
			if(p!=cmp)
			cmp = cmp==-2?p:-3;
		}
		infl.parent_joint.select_id(cmp);
	}

	if(changeBits&Model::SelectionFaces)
	{
		show = !model.fselection.empty();	
		faces.nav.set_hidden(!show);
		faces.change(changeBits);
	}
}

void SideBar::PropPanel::name_props::change(int changeBits)
{
	if(!changeBits)
	{
		model->setPositionName(model.selection[0],name);
		model->operationComplete(::tr("Rename","operation complete"));
	}
	else if(utf8 n=model->getPositionName(model.selection[0]))
	{
		if(n!=name.text()) name.set_text(n);
	}
}

void SideBar::PropPanel::pos_props::change(int changeBits)
{	
	if(changeBits) // Update coordinates in text fields	
	{
		//FIX ME
		//2019: Assuming something like this should be done.
		//But I'm not sure what is the correct flags to use.
		int watchlist = 
		Model::SelectionVertices| //SelectionChange?
		Model::SelectionJoints|
		Model::SelectionPoints|
		Model::SelectionProjections|
		Model::MoveGeometry|
		Model::MoveOther|
		Model::AnimationMode|
		Model::AnimationFrame;
		if(0==(changeBits&watchlist))
		return;

		//WORRIED THIS IS IMPRECISE
		double init = model.selection.empty()?0.0:DBL_MAX;
		double cmin[3] = {+init,+init,+init};
		double cmax[3] = {-init,-init,-init};
	
		for(auto&i:model.selection)
		{
			if(i.type==Model::PT_Projection
			&&model->inAnimationMode())
			continue; 

			double coords[3];
			model->getPositionCoords(i,coords);
			for(int i=0;i<3;i++)
			cmin[i] = std::min(cmin[i],coords[i]);
			for(int i=0;i<3;i++)
			cmax[i] = std::max(cmax[i],coords[i]);
		}
		for(int i=0;i<3;i++)
		{
			centerpoint[i] = (cmin[i]+cmax[i])/2;
		}

		x.set_float_val(centerpoint[0]);
		y.set_float_val(centerpoint[1]);
		z.set_float_val(centerpoint[2]);
	
		bool hide = 1>=model.selection.size();
		dimensions.set_hidden(hide);
		if(hide) return;

		//TODO: Add tooltip... make editable?
		//dimensions.text().format("%g,%g,%g",cmax[0]-cmin[0],cmax[1]-cmin[1],cmax[2]-cmin[2]);
		dimensions.text().clear();
		for(int i=0;;i++)
		{
			enum{ width=5 };
			char buf[32]; 
			int len = snprintf(buf,sizeof(buf),"%.6f",fabs(cmax[i]-cmin[i]));
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
			if(i==2) break;
			dimensions.text().push_back(',');
			dimensions.text().push_back(' ');
		}
		dimensions.set_text(dimensions);
	}
	else // Change model based on text field input
	{
		//YUCK: Historically the Position field is
		//always visible just so Properties doesn't
		//appear to be empty.
		if(model.selection.empty()) 
		{
			model_status(model,StatusError,STATUSTIME_LONG,"Empty selection");
			return;
		}

		//WORRIED THIS IS IMPRECISE
		double trans[3],coords[3] = {x,y,z};		
		for(int i=0;i<3;i++)
		{
			trans[i] = coords[i]-centerpoint[i];
			centerpoint[i] = coords[i];
		}
		model->translateSelected(trans);
		model->operationCompound(::tr("Set Position","operation complete"));
	}
}

void SideBar::PropPanel::rot_props::change(int changeBits)
{
	double rad[3] = {};

	if(changeBits) // Update coordinates in text fields
	{	
		//FIX ME
		//2019: Assuming something like this should be done.
		//But I'm not sure what is the correct flags to use.
		int watchlist = 
		Model::SelectionJoints|
		Model::SelectionPoints|
		Model::SelectionProjections|
		Model::MoveOther|
		Model::AnimationMode|
		Model::AnimationFrame;		
		if(0==(changeBits&watchlist))
		return;
	}
	else // Change model based on text field input
	{	
		rad[0] = x; rad[1] = y; rad[2] = z;

		for(int i=0;i<3;i++) rad[i]*=PIOVER180;
	}

	if(changeBits)
	model->getPositionRotation(model.selection[0],rad);
	else
	model->setPositionRotation(model.selection[0],rad);

	if(changeBits)
	{
		for(int i=0;i<3;i++) rad[i]/=PIOVER180;

		x.set_float_val(rad[0]);
		y.set_float_val(rad[1]);
		z.set_float_val(rad[2]);
	}
	else model->operationCompound(::tr("Set Rotation","operation complete"));
}

void SideBar::PropPanel::scale_props::change(int changeBits, control *c)
{
	double rad[3] = {};

	if(changeBits) // Update coordinates in text fields
	{	
		//FIX ME
		//2019: Assuming something like this should be done.
		//But I'm not sure what is the correct flags to use.
		int watchlist = 
		Model::SelectionJoints|
		Model::SelectionPoints|
		Model::SelectionProjections|
		Model::MoveOther|
		Model::AnimationMode|
		Model::AnimationFrame;		
		if(0==(changeBits&watchlist))
		return;
	}
	else // Change model based on text field input
	{	
		//NOTE: To do this I had to change spinner::do_click to 
		//use Ctrl+Alt instead of Shift to change the increment.
		if(event.curr_shift&&!event.tab_activate_event)
		{
			unsigned i = (spinbox*)c-&x; assert(i<3);
			model->getPositionScale(model.selection[0],rad);
			double diff = c->float_val()-rad[i];
			for(i=3;i-->0;) rad[i]+=diff;
		}
		else
		{
			rad[0] = x; rad[1] = y; rad[2] = z;
		}
	}

	if(changeBits)
	model->getPositionScale(model.selection[0],rad);
	else
	model->setPositionScale(model.selection[0],rad);

	if(changeBits)
	{
		x.set_float_val(rad[0]);
		y.set_float_val(rad[1]);
		z.set_float_val(rad[2]);
	}
	else model->operationCompound(::tr("Set Scale","operation complete"));
}

void SideBar::PropPanel::interp_props::change(int changeBits, control *c)
{	
	if(changeBits) // Update coordinates in text fields
	{
		int sel = 0, am = model->getAnimationMode();
		if(am&1) sel|=Model::SelectionJoints;
		if(am&2) sel|=Model::SelectionVertices|Model::SelectionPoints;

		if(0==(changeBits&
		(Model::AnimationProperty //2022: Now for keyframes.
	//	|Model::MoveGeometry //Was for???
	//	|Model::MoveOther //Was for keyframes.
		|Model::AnimationMode
		|Model::AnimationFrame|sel))) 
		{
			return;
		}

		int anim = model->getCurrentAnimation();
		int frame = model->getCurrentAnimationFrame();		
		
		const auto keep = Model::InterpolateKeep;
		Model::Interpolate2020E cmp[3] = {keep,keep,keep};
		int mask = 0;
		model->getSelectedInterpolation
		(anim,frame,[&](const Model::Interpolate2020E e[3])
		{
			int i = e[1]==Model::InterpolateVoid?1:3;
			mask|=i;
			while(i-->0)
			if(cmp[i]!=Model::InterpolateVoid)
			{
				if(cmp[i]!=e[i])
				if(cmp[i]==keep)
				cmp[i] = e[i]; 
				else 
				cmp[i] = Model::InterpolateVoid;
			}
		});
		menu[1].set_hidden(mask==1);
		menu[2].set_hidden(mask==1);

		if(!mask //No frames? Not keyframe?
		||model->getCurrentAnimationFrameTime()
		!=model->getAnimFrameTime(anim,frame))
		for(int i=3;i-->0;) cmp[i] = Model::InterpolateNone;
		for(int i=3;i-->0;) menu[i].select_id(cmp[i]);	
	}
	else // Change model based on text field input
	{
		unsigned i = (dropdown*)c-menu; assert(i<3);
		model->interpolateSelected
		((Model::Interpolant2020E)i,(Model::Interpolate2020E)menu[i].int_val());
		model->operationComplete(::tr("Set Interpolation","operation complete"));
	}
}

void SideBar::PropPanel::group_props::change(int changeBits)
{
	if(!changeBits)
	{
		return; //group_props::submit?
	}

	if(changeBits&Model::AddOther) 
	{
		int iN = model->getGroupCount();
		group.menu.clear();
		group.menu.add_item(-1,::tr("<None>"));
		for(int i=0;i<iN;i++)
		group.menu.add_item(i,model->getGroupName(i));	
		group.menu.add_item(iN,::tr("<New Group>"));
		group.menu.add_item(iN+1,::tr("<New Material>"));
	
		iN = model->getTextureCount();
		material.menu.clear();
		material.menu.add_item(-1,::tr("<None>"));
		for(int i=0;i<iN;i++)
		material.menu.add_item(i,model->getTextureName(i));
	
		iN = model->getProjectionCount();
		projection.menu.clear();
		projection.menu.add_item(-1,::tr("<None>"));
		for(int i=0;i<iN;i++)	
		projection.menu.add_item(i,model->getProjectionName(i));
		//projection.nav.enable(iN!=0); 
		projection.nav.set_hidden(iN==0); 
	}

	//2022: AddOther is need when the index changes
	//as a result of removal, etc.
	if(changeBits&(Model::SelectionFaces|Model::AddOther))
	{
		int grp = -2, prj  = -2;
		for(int i:model.fselection)
		{
			if(grp!=-3)
			if(grp==-2) //2020: treat -1 as distinct entity
			{
				grp = model->getTriangleGroup(i);				
			}
			else if(grp!=model->getTriangleGroup(i))
			{
				grp = -3; //2020: show nothing? (enable setting)
			}
			if(prj!=-3)
			if(prj==-2) //2020: treat -1 as distinct entity
			{
				prj = model->getTriangleProjection(i);
			}
			else if(prj!=model->getTriangleProjection(i))
			{
				prj = -3; //2020: show nothing? (enable setting)
			}
		}

		group.menu.select_id(grp);		
		material.nav.enable(grp>=0);
		material.menu.select_id(grp==-3?-2:model->getGroupTextureId(grp));
		projection.menu.select_id(prj);
	}
}
void SideBar::PropPanel::group_props::submit(int id)
{
	if(id==id_material_settings)
	{	 		
		int g = group.menu;
		int m = material.menu; if(g>=0)
		{
			model->setGroupTextureId(g,m);
			model->operationComplete(::tr("Set Material","operation complete"));
		}
	}
	else if(id==id_projection_settings)
	{
		int p = projection.menu;
		model->setTrianglesProjection(model.fselection,p);
		if(projection.window.enable(p>=0).enabled())
		model->applyProjection(p);
		model->operationComplete(::tr("Set Projection","operation complete"));
	}
	else if(id==id_group_settings) 
	{
		int g = group.menu;		
		int iN = model->getGroupCount();
		if(g==-1)
		{
			//EXTREMELY INEFFECIENT
			material.nav.disable();
			material.menu.select_id(-1); //2022: Still showing old name/text?
			for(int i:model.fselection)
			{
				g = model->getTriangleGroup(i);
				if(g>=0)
				model->removeTriangleFromGroup(g,i);
			}
			model->operationComplete(::tr("Unset Group","operation complete"));
		}
		else if(g<iN) new_group:
		{
			model->addSelectedToGroup(g);
			model->operationComplete(::tr("Set Group","operation complete"));

			material.menu.select_id(model->getGroupTextureId(g));
			material.nav.enable();
		}
		else if(!event.wheel_event)
		{	
			int was = -1;
			if(auto*e=event.get_edited())
			was = *e; else assert(0);
			std::string groupName;
			if(auto*str=model->getGroupName(was)) groupName = str;
			else was = -1;
			if(id_ok==Win::EditBox(&groupName,
			::tr("New Group","Name of new group window title"),
			::tr("Enter new group name:"),1,Model::MAX_NAME_LEN))
			{
				model->addGroup(was,groupName.c_str());
				group.menu.selection()->set_text(groupName);
				group.menu.last_item()->set_text(::tr("<New Group>"));
				group.menu.add_item(iN+1,::tr("<New Material>"));
				if(g==iN+1) //2022
				{
					int i = model->addMaterial(groupName.c_str());
					model->setGroupTextureId(iN,i);
					material.menu.add_item(i,groupName);
					material.menu.select_id(i);

					g--;
				}
				goto new_group;
			}
		}
		else group.menu.select_id(iN-1); 
	}
	else assert(0);
}

void SideBar::PropPanel::proj_props::change(int changeBits)
{
	if(changeBits)
	{
		int watchlist =
		Model::SelectionProjections|
		Model::MoveOther|
		Model::AnimationMode;		
		if(0==(changeBits&watchlist))
		return;
	}

	for(auto&i:model.selection)
	if(i.type==Model::PT_Projection)
	{
		if(changeBits) // Update projection fields
		{
			type.menu.select_id(model->getProjectionType(i));
			break;
		}
		else model->setProjectionType(i,type.menu);
	}

	if(changeBits)
	{
		bool hide = 1!=model.selection.size();
		if(hide!=scale.hidden())
		{
			if(hide) nav.space<top>(0);
			nav.set_name(hide?"":"Scale");
			type.nav.set_name(hide?"Projection Type":"Type");
			scale.set_hidden(hide);
		}
		if(!hide)
		{
			scale.set_float_val(model->getProjectionScale(model.selection[0]));
		}
		else if(!model.sidebar.prop_panel.group.nav.hidden())
		{
			hide = false;
		}
		type.nav.set_name(hide?"Projection":"Type");
	}
	else 
	{
		model->setProjectionScale(model.selection[0],scale);			
		model->operationComplete(::tr("Set Projection Type","operation complete"));
	}
}

static void sidebar_update_weight_v(Widgets95::li &v, bool enable, int type, int weight)
{
	Widgets95::li::item *it[4] = {v.first_item()};
	for(int i=1;i<4;i++) it[i] = it[i-1]->next();

	int i = !type?2:type-1;
	it[0]->id(1).set_text(::tr("Auto","bone joint influence"));
	it[1]->id(2).set_text(::tr("Remainder","bone joint influence"));
	it[2]->id(0).set_text(::tr("Custom","bone joint influence"));
	it[3]->id(100).set_text(::tr("Assign 100","bone joint influence")); //2020
	
	if(enable&&type>=0)
	{
		assert(type<=2);

		utf8 str = "%d"; switch(type)
		{
		case 1: str = ::tr("Auto: %d"); //::tr("Auto: %1").arg(weight);
				break;
		case 2: str = ::tr("Rem: %d"); //::tr("Rem: %1").arg(weight);
				break;
		}
		auto &ittt = it[i]->text();
		ittt.format(str,weight);
		it[i]->set_text(ittt);
	}
	v.set_int_val(type); //v.select_id(type); 

	//HACK: editing textbox out from under user?
	//(suppress callback)
	Win::event.set_edited(); 

	if(enable!=v.enabled()) v.parent()->enable(enable);
}
void SideBar::PropPanel::infl_props::change(int changeBits)
{
	int watchlist = 
	Model::SelectionVertices|Model::SelectionPoints
	//2021: Until now weights were shown during regular
	//editing, but it seems like too much information for
	//regular animation work and too much processing burden
	//when selecting a large number of vertices
	//|Model::AnimationMode
	|Model::SetInfluence; //AddOther; 
	if(0==(changeBits&(Model::AddOther|watchlist)))
	{
		return; // Only change if group or selection change	
	}

	int bonesN = model->getBoneJointCount();

	//Why was this? I've changed model_influence.cc to
	//recalculate positions as the weights are changed.
	//bool enable = !model->inAnimationMode();
	//bool enable = true;

	if(changeBits&Model::SetInfluence) //AddOther
	{
		//2021: This is actually for Joints instead of
		//influences.
		parent_joint.clear();
		parent_joint.add_item(-1,::tr("<None>"));
		for(int i=0;i<bonesN;i++)
		parent_joint.add_item(i,model->getBoneJointName(i));
			
		for(size_t i=0;i<igv.size();i++) //MAX_INFLUENCES
		{
			igv[i]->joint.clear();
			igv[i]->joint.reference(parent_joint);
		}
	}

	if(!nav||nav.hidden()) return;
		
	// Update influence fields		
	JointCount def = {};
	jcl.clear(); jcl.resize(bonesN,def);
	
	// for now just do a sum on which bone joints are used the most
	// TODO: may want to weight by influence later
	for(auto&i:model.selection)
	if(auto*infl=model->getPositionInfluences(i))
	for(auto&ea:*infl)
	{
		int bone = ea.m_boneId;
		jcl[bone].count++;
		jcl[bone].weight+=(int)lround(ea.m_weight*100);
		jcl[bone].typeCount[ea.m_type]++;
	}

	for(int bone=0;bone<bonesN;bone++)
	{
		int type = 100;
		//2020: Why do this? It makes the UI confusing.
		//for(int t=0;type!=0&&t<Model::IT_MAX;t++)		
		for(int t=0;t<Model::IT_MAX;t++)		
		if(jcl[bone].typeCount[t]>0)
		{
			type = type==100?t:-1;
		}

		if(type!=100)
		{
			jcl[bone].typeIndex = type;
			jcl[bone].weight = (int)lround(jcl[bone].weight/(double)jcl[bone].count);
		}
		else jcl[bone].weight = 100;
	}

	for(size_t i=0;;i++) //MAX_INFLUENCES
	{
		if(i==igv.size()) grow_igv();

		int maxVal = 0, bone = -1;

		for(int b=0;b<bonesN;b++)		
		if(!jcl[b].inList&&jcl[b].count>maxVal)
		{
			bone = b; maxVal = jcl[b].count;
		}

		// No more influences; done.
		if(maxVal<=0) 
		{
			size_t ii = i; //NEW

			for(;i<igv.size();i++) //MAX_INFLUENCES
			{
				igv[i]->joint.select_id(-1);
				igv[i]->v.select_id(-1);
				igv[i]->prev_joint = -1;

				igv[i]->joint.set_hidden();
				igv[i]->weight.set_hidden();
			}
			//if(ii<Model::MAX_INFLUENCES)
			igv[ii]->joint.set_hidden(false);

			break; 
		}

		jcl[bone].inList = true;

		//REMOVE ME (will crash)
		//if(!enable) //NEW 
		//igv[i]->joint.add_item(bone,model->getBoneJointName(bone));
		igv[i]->joint.select_id(bone);
		igv[i]->prev_joint = bone;

		//NEW: It looks weird for 'weight' to be disabled but 'joint'
		//to be enabled.
		igv[i]->joint.set_hidden(false);
		igv[i]->weight.set_hidden(false);

		//log_debug("bone i %d is bone ID %d\n",i,bone); //???

		sidebar_update_weight_v(igv[i]->v,true,jcl[bone].typeIndex,jcl[bone].weight);
	}
}
void SideBar::PropPanel::infl_props::submit(control *c)
{
	size_t index = size_t(c->id()-'0'); 
	
	assert(index<igv.size());

	index_group &group = *igv[index];

	int j = group.joint; bool updateRemainders = false;

	pos_list &l = model.selection;

	if(c==group.joint) /* Joint selected? */
	{
		bool enable = j>=0; int jj = group.prev_joint;

		if(enable)
		for(size_t i=0;i<igv.size();i++) if(i!=index) //MAX_INFLUENCES
		{
			//int compile[4==Model::MAX_INFLUENCES]; (void)compile;

			if(igv[i]->prev_joint==j)
			{
				// trying to assign new joint when we already have that joint
				// in one of the edit boxes. Change the selection back. This
				// will cause some nasty bugs and probably confuse the user.
				// Change the selection back.	
				(void)group.joint.select_id(jj);
				return event.beep();
			}
		}
		group.prev_joint = j;

		if(jj>=0)
		{
			for(auto&i:l)
			model->removePositionInfluence(i,jj);
			jcl[jj].count = 0;
		}

		bool hide = j<0;
		bool compacted = false;
		//doing this even when hiding simplifies
		//the following code greatly
		if(index+1==(int)igv.size()) 
		{
			grow_igv();
			igv.back()->joint.select_id(-1);
			igv.back()->weight.set_hidden();
		}
		//NEW: Hide joints removed from the back
		//of the list, but don't attempt to move
		//joints around because that's too messy.
		if(!hide||igv[index+1]->weight.hidden())
		{	
			igv[index]->weight.set_hidden(hide);			
			igv[index+1]->joint.set_hidden(hide);
		}
		else if(hide) //2021: Remove from middle?
		{
			assert(!enable);

			compacted = true;

			change(Model::SelectionJoints); //HACK
		}

		if(enable)
		{
			jcl[j].count = l.size();

			extern int viewwin_joints100; if(viewwin_joints100)
			{
				for(auto&i:l)
				model->setPositionInfluence(i,j,Model::IT_Custom,1);
				jcl[j].weight = 100;
				jcl[j].typeIndex = Model::IT_Custom;
			}
			else
			{
				double weight = 0; for(auto&i:l)
				{
					double w = model->calculatePositionInfluenceWeight(i,j);

					//log_debug("influence = %f\n",w); //???

					model->setPositionInfluence(i,j,Model::IT_Auto,w);

					weight+=(int)lround(w*100);
				}
				assert(!l.empty());
				if(!l.empty()) //zero divide???
				jcl[j].weight = (int)lround(weight/l.size());
				jcl[j].typeIndex = Model::IT_Auto;
			}
		}		
		model->operationComplete(::tr("Change Joint Assignment","operation complete"));

		if(!compacted)
		sidebar_update_weight_v(group.v,enable,enable?jcl[j].typeIndex:-1,enable?jcl[j].weight:0);
	}
	else if(c==group.weight) /* Weight text edited? */
	{
		if(isdigit(group.weight.text()[0]))
		{
			double weight = std::max(0,std::min<int>(100,group.weight))/100.0;

			//log_debug("setting joint %d weight to %f\n",j,weight); //???

			for(auto&i:l) model->setPositionInfluence(i,j,Model::IT_Custom,weight);

			model->operationComplete(::tr("Change Influence Weight","operation complete"));
		}
		else model_status(model,StatusError,STATUSTIME_LONG,"Please input a plain number");
	}
	else if(c==group.v) /* Weight mode selected? */
	{
		auto type = (Model::InfluenceTypeE)group.v.int_val();
		
		if((int)type==100)
		{
			type = Model::IT_Custom; jcl[j].weight = 100; //NEW
		}

		//log_debug("setting joint %d type to %d\n",j,(int)type); //???

		double weight = type==Model::IT_Auto?0:jcl[j].weight/100.0;
		double w = weight;

		for(auto&i:l)
		{
			if(type==Model::IT_Auto)
			{
				w = model->calculatePositionInfluenceWeight(i,j);
				weight+=(int)lround(w*100);
			}
			model->setPositionInfluence(i,j,type,w);
		}
		model->operationComplete(::tr("Change Influence Type","operation complete"));

		if(type==Model::IT_Auto)
		{
			jcl[j].weight = (int)lround(weight/jcl[j].count);
		}

		jcl[j].typeIndex = type;

		sidebar_update_weight_v(group.v,true,jcl[j].typeIndex,jcl[j].weight);
	}

	for(size_t i=0;i<igv.size();i++) //MAX_INFLUENCES
	if(igv[i]->v.int_val()==Model::IT_Remainder)
	if(j>=0&&j<(int)model->getBoneJointCount()) //???
	{
		//log_debug("getting remainder weight for joint %d\n",j); //???
		
		int count = 0; double weight = 0;
		for(auto&i:l) if(auto*infl=model->getPositionInfluences(i))
		{
			for(auto&ea:*infl)
			if(ea.m_type==Model::IT_Remainder&&j==ea.m_boneId)
			{
				weight+=(int)lround(ea.m_weight*100);
				count++;
			}
		}

		int w = count?(int)lround(weight/(double)count):0;
		
		//log_debug("updating box %d with remaining weight %d\n",i,w); //???

		assert(group.v.enabled());
		sidebar_update_weight_v(group.v,true,group.v,w);
	}
}

void SideBar::PropPanel::faces_props::change(int changeBits)
{	
	auto &fs = model.fselection;

	if(changeBits) // Update coordinates in text fields	
	{
		reverse:
		char buf[64];
		int i = 0; for(int f:fs)
		{
			i+=snprintf(buf+i,sizeof(buf)-i,"%d,",f);
			if((unsigned)i>=sizeof(buf))
			{
				i = sizeof(buf)-2;
				while(i&&buf[i]!=','||i>sizeof(buf)-4) 
				i--;
				for(int j=3;j-->0;) 
				buf[i++] = '.';
				break;
			}
		}
		if(i&&buf[i-1]==',') 
		i--;
		buf[i] = '\0';
		menu.first_item()->set_text(buf);
	}
	else if(int id=menu)
	{
		menu.select_id(0);

		if(fs.empty()) return; //paranoia

		if(id<=2)
		{
			model->prioritizeSelectedTriangles(id==1);
			model->operationComplete(::tr("Prioritize Selected Faces","operation complete"));
		}		
		else if(id==3)
		{
			model->reverseOrderSelectedTriangle();
			model->operationComplete(::tr("Reverse Order Selected Faces","operation complete"));		
			//NOTE: This is pure cosmetic feedback.
			std::reverse(fs.begin(),fs.end());
			goto reverse;
		}
		else if(id==4)
		{
			std::swap(fs.front(),fs.back());
			fs.pop_back();
			for(int i:fs) model->unselectTriangle(i);
			model->operationComplete(::tr("Keep First Face in Selection","operation complete"));
		}
	}
}