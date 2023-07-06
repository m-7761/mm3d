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

#include "viewwin.h"

//NOTE: As far as Utilities go, there's intentional
//overlap with the Meta (User) UI so that they feel
//consistent and mutually benefit from developments.

struct UtilEditWin : Win
{
	void submit(int);

	UtilEditWin(li::item *kv, int *lv, int focus)
		:
	Win("New utility"),kv(kv),
	name(main,"Name"),
	utils(main,"Utility",lv),
	ok_cancel(main)
	{
		name.expand(); utils.expand();

		add_util(Model::UT_NONE);
		add_util(Model::UT_UvAnimation);

		name.set_text(kv->c_str()); utils.select_id(*lv);

		active_callback = &UtilEditWin::submit;

		submit(id_init);

		if(focus) utils.activate();
	}

	li::item *kv;

	textbox name;
	dropdown utils; 
	ok_cancel_panel ok_cancel;
	
	static utf8 type_str(Model::UtilityTypeE e)
	{
		switch(e)
		{
		default: assert(0);
		case Model::UT_NONE: return "<None>";
		case Model::UT_UvAnimation: return "UV Animation";
		}
	}
	void add_util(Model::UtilityTypeE e)
	{
		utils.add_item(e,type_str(e));
	}
};
void UtilEditWin::submit(int id)
{
	if(id==id_ok)	
	{
		kv->text().format(&"%s\0%s",name.text().c_str(),
		type_str((Model::UtilityTypeE)utils.int_val()));
	}
	basic_submit(id);
}

struct MetaEditWin : Win
{
	void submit(int);

	MetaEditWin(li::item *kv, int focus)
		:
	//Win("Edit Meta Data"),kv(kv),
	Win("New data"),kv(kv),
	key(main,"Name"),
	val(main,"Value"),
	ok_cancel(main)
	{
		key.expand(); val.expand();

		utf8 row[2]; kv->text().c_row(row);
		key.set_text(row[0]); val.set_text(row[1]);		

		active_callback = &MetaEditWin::submit;

		submit(id_init);

		if(focus) val.activate();
	}

	li::item *kv;

	textbox key,val; 
	ok_cancel_panel ok_cancel;
};
void MetaEditWin::submit(int id)
{
	if(id==id_ok)		
	kv->text().format(&"%s\0%s",
	key.text().c_str(),val.text().c_str());
	basic_submit(id);
}

struct UtilWin : Win
{
	void submit(control*c);

	struct UvAnimation; //C++
	struct Plugin : Model::Observer
	{
		Plugin(MainWin &m, int u):model(m),
		u(u),type(m->getUtilityList()[u]->type),
		main(nullptr,panel::none)
		{}
		virtual ~Plugin(){ /*NOP*/ }

		MainWin &model; int u;

		const Model::UtilityTypeE type;

		panel main;
				
	//	virtual void modelChanged(int)=0;
		virtual void reset()=0;
		virtual bool submit(control*)=0;
	};
	static Plugin *close_all(Model *m, bool unplug=false)
	{
		for(auto*o:m->getObserverList())		
		if(auto*p=dynamic_cast<Plugin*>(o))		
		if(m==p->model)
		{
			m->removeObserver(o);
			auto w = (UtilWin*)p->main.ui();
			(unplug?w->p:p) = nullptr;
			if(unplug) p->main.set_parent();
			w->ui::close(); 
			close_all(m); return p;
		}
		return nullptr;
	}

	UtilWin(MainWin &m, Plugin *p) 
		:
	Win(UtilEditWin::type_str(p->type)),
		p(p),
	nav(main),
	name(nav,"",id_name),
	close(nav,"Close",id_close)
	{
		p->model->addObserver(p);

		nav.expand(); close.ralign();

		p->main.set_parent(main,nav);

		auto &ul = p->model->getUtilityList();

		for(int i=0,iN=(int)ul.size();i<iN;i++)
		{
			if(p->type==ul[i]->type)
			name.add_item(i,ul[i]->name);
		}
		name.select_id(p->u);

		active_callback = &UtilWin::submit;
	}
	~UtilWin()
	{
		if(p) p->model->removeObserver(p);

		delete p; 
	}

	Plugin *p;

	row nav;
	dropdown name; button close;
};
void UtilWin::submit(control *c)
{
	int id = c->id();

	switch(id)
	{
	case id_name:

		p->u = name;

		return p->reset();
	
	case id_close: return ui::close();
	
	default: 
		
		//In this case there's no Cancel button so every change
		//must be an undo sequence point.
		if(p->submit(c))
		p->model->operationComplete(UtilEditWin::type_str(p->type));
		break;
	}

	basic_submit(id);
}

extern UtilWin::Plugin *uvanimutil(MainWin &m, int u);

struct MetaWin : Win
{
	void both_init();
	void both_submit(int);
	void meta_submit(int);
	void util_submit(control*);

	MetaWin(MainWin &m, bool u)
		:
	//Win("Model Meta Data"),
	Win(u?"Utilities":"User Data"),
		model(m),u(u),
	open(main,"",0,id_open), //HACK	
	insert(open,"New",id_new),
	append(open,"Add",id_append),
	del(open,"Delete",id_delete),
	table(main,"",id_item),
	header(table,id_sort),
		k_col(header,"Name"),
		v_col(header,u?"Utility":"Value"),
	f1_ok_cancel(main),
	utils(nullptr)
	{
		k_col.span(140);

		//HACK: Give tabs like appereance.
		//open.lock(header.span()) would be
		//a better way but it's not working.
		insert.span()-=2;
		append.span()-=2; del.span()-=2;

		if(!u) table.lock(420,false);
		else table.expand();

		both_init();

		//2022: Send callbacks in response 
		//to keyboard/wheel input? Otherwise
		//the side panel is kind of confusing.
		if(u) table.set_click(false);

		//HACK: Closing out the detached 
		//window to avoid conflicts. There
		//can only be one with this approach.
		if(u) if(auto*p=UtilWin::close_all(model,true))
		{
			table.show_line(p->u).outline(p->u);
			utils = new Utils(*this);
			utils->plugin(p);			
		}
		else toggle_utils();
	}
	~MetaWin(){ delete utils; }

	MainWin &model;

	const bool u; //2022: Utilities?

	row open; //HACK
	button insert,append,del;
	listbox table;
	listbar header;	
	multiple::item k_col,v_col;
	f1_ok_cancel_panel f1_ok_cancel;

	struct Utils
	{
		enum{ id_detach=1000 };

		Utils(MetaWin &mw):p(),
		c(mw.main),
		nav(mw.main),
		detach(nav,"Detach",id_detach)
		{
			nav.expand(bottom);
			nav.cspace<center>();
		}
		~Utils(){ unplug(); }

		UtilWin::Plugin *p;

		column c;
		panel nav;
		button detach;

		void unplug()
		{
			if(p&&p->main.parent()==c.parent()) 
			delete p; p = nullptr;
		}
		void plugin(UtilWin::Plugin *pp)
		{
			if(p==pp) return; unplug(); p = pp; 

			if(p) p->main.set_parent(c.parent(),c,behind);
			nav.set_hidden(!p);

			//HACK: Hiding c seems not to work.
			c.parent()->space(p?Widgets95::ui_edgespacing:0);
		}

	}*utils;

	void append_item(utf8 k, utf8 v, int id=-1)
	{
		auto *i = new li::item(id); 
		i->text().format(&"%s\0%s",k,v); 
		table.add_item(i);
	}
	void insert_item(utf8 k, utf8 v, int id=-1)
	{
		auto *i = new li::item(id); 
		i->text().format(&"%s\0%s",k,v); 
		table.insert_item(i,table.find_line_ptr());
	}
	void refresh_utils()
	{
		int id = 0; table^[&](li::allitems i)
		{
			if(-1!=i->id()) i->id() = id++; 
		};
	}
	void toggle_utils()
	{
		if(auto*i=table.find_line_ptr())
		{	
			int u = i->id();
			auto *up = u>=0?model->getUtilityList()[u]:nullptr;
			if(int lv=up?up->type:0)
			{
				if(!utils) utils = new Utils(*this);

				auto *p = utils->p;

				if(p&&p->type==lv)
				{
					if(p->u!=u)
					{
						p->u = u; p->reset(); //OPTIMIZING
					}
					return;
				}
				else switch(up->type)
				{
				case Model::UT_UvAnimation:

					utils->plugin(uvanimutil(model,u)); 
					return;
				}
			}
		}
		if(utils) utils->plugin(nullptr);
	}
};

extern void metawin(MainWin &m, bool u){ MetaWin(m,u).return_on_close(); }

void MetaWin::both_init()
{
	if(u)
	active_callback = &MetaWin::util_submit;
	else
	active_callback = &MetaWin::meta_submit;

	int i=0,iN;
	if(u) for(auto*ea:model->getUtilityList())
	{
		append_item(ea->name.c_str(),UtilEditWin::type_str(ea->type),i++);
	}
	else for(iN=model->getMetaDataCount();i<iN;i++)
	{	
		Model::MetaData &md = model->getMetaData(i);
		append_item(md.key.c_str(),md.value.c_str(),i);
	}

	table.drop(14*table.font().height);
}
void MetaWin::both_submit(int id)
{
	switch(id)
	{
	case id_new: 
				
		insert_item(::tr("Name"),::tr(u?"<None>":"Value"));		
		active_callback(open); //HACK: id_open
		break;

	case id_append:

		//new_item(::tr("Name","meta value key name"),::tr("Value","meta value 'value'"));
		append_item(::tr("Name"),::tr(u?"<None>":"Value"));
		table.outline(table.find_line_count()-1);
		active_callback(open); //HACK: id_open
		break;

	case id_sort:
	
		header.sort_items([&](li::item *a, li::item *b)
		{
			utf8 row1[2]; a->text().c_row(row1);
			utf8 row2[2]; b->text().c_row(row2);
			//https://gcc.gnu.org/bugzilla/show_bug.cgi?id=92338
			return strcmp(row1[(int)header],row2[(int)header])<0;
		});
		break;

	case id_ok:

		//YUCK: Make deferred <None> utilities to be consistent?
		{
			//NOTE: id_new could be changed to avoid <None> or
			//to not make the entry until confirmed, but is it
			//better to see a blank or random default? Or what
			//if OK was disabled? I don't know???
			int i = -1; table^[&](li::allitems it)
			{
				if(i++,-1==it->id())
				model->moveUtility(model->addUtility(Model::UT_NONE,it->c_str()),i);
			};
		}

		model->operationComplete(::tr(u?"Utility changes":"User data","operation complete"));
		break;

	case id_cancel:

		model->undoCurrent();
		break;
	}

	basic_submit(id);
}
void MetaWin::meta_submit(int id)
{
	switch(id)
	{	
	case id_delete:

		table.delete_item(table.find_line_ptr());
		break;

	case id_item: case id_open:
	
		switch(id==id_open?2:event.get_click())
		{
		case 2: //Double-click? 
			
			MetaEditWin(table.find_line_ptr(),header).return_on_close();
			break;

		case 1:

			if(event.listbox_item_rename())
			{
				textbox modal(table);
				modal.move_into_place(); modal.return_on_enter();
			}
			break;
		}		
		break;
	
	case id_ok:

		model->clearMetaData();
		table^[&](li::allitems ea)
		{
			utf8 row[2]; ea->text().c_row(row);
			model->addMetaData(row[0],row[1]);
		};			
		break;
	}

	both_submit(id);
}
extern bool viewwin_confirm_close(int,bool);
void MetaWin::util_submit(control *c)
{
	int id = c->id();

	if((void*)c<this||(void*)c>=this+1)
	{
		assert(utils&&utils->p);

		if(id==Utils::id_detach) 
		{
			//This is reimplementing glutCloseFunc.
			if(viewwin_confirm_close(glut_window_id(),true))
			{
				auto *p = utils->p; p->main.set_parent();
				both_submit(id_close);
				//HACK: Need to change the foreground
				//window since UtilWin (Win) requires
				//a parent to remove its Windows icon.
				glutSetWindow(model.glut_window_id);
				glutPopWindow();
				auto *uw = new UtilWin(model,p);
			}
			return;
		}
		if(utils->p->submit(c))
		{
			//Here UtilWin does operationComplete.
			//Whereas MetaWin has a Cancel button.
		}
		return;
	}
	else switch(id)
	{
	case id_delete:
	{
		if(auto*i=table.find_line_ptr()) //Disable?
		{
			if(i->id()>=0) model->deleteUtility(i->id());
			table.delete_item(i);
			refresh_utils(); toggle_utils();
		}
		break;
	}
	case id_item: case id_open:
	{
		auto *i = table.find_line_ptr();
		int u = i->id();
		auto *up = u>=0?model->getUtilityList()[u]:nullptr;
		int lv = up?up->type:0;
		int lw = lv;
		switch(id==id_open?2:event.get_click())
		{
		case 2: //Double-click? 
			
			dblclk: 
			if(id_ok!=UtilEditWin(i,&lv,header).return_on_close())
			return;
			break;

		case 1:

			if(event.listbox_item_rename())
			{
				if(header) goto dblclk;

				textbox modal(table);
				modal.move_into_place(); modal.return_on_enter();
				break;
			}
			//break;

		case 0: i = nullptr; //Change selection?

			break;
		}
		auto type = (Model::UtilityTypeE)lv;
		if(i)
		{
			if(!lw&&lv) 
			{
				i->id() = u = table.find_line();
				model->moveUtility(model->addUtility(type,i->c_str()),u);
				up = model->getUtilityList()[u];
				refresh_utils();
			}
			else if(lw!=lv) if(!lv) //UT_NONE?
			{
				model->deleteUtility(u);
				i->id() = u = -1;				
				refresh_utils();
			}
			else model->convertUtilityToType(type,u); //Data?

			if(u!=-1) model->setUtilityName(u,i->c_str());
		}
		toggle_utils(); break;
	}}

	both_submit(id);
}

//TODO: THIS NEEDS ITS OWN uvanimutil.cc FILE.

struct UvFrameWin : Win
{
	typedef Model::UvAnimation::Key Key;

	UvFrameWin(Key &lv)
		:
	Win("New keyframe"),
	frame(main,"Frame",&lv.frame,id_item),
	r(main,"Rotate",&lv.r,'r'),
	rz(main,"CCW",&lv.rz),
	s(main.inl,"Scale",&lv.s,'s'),
	sx(main,"U",&lv.sx),
	sy(main,"V",&lv.sy),
	ok_cancel(main),
	t(main.inl,"Translate",&lv.t,'t'),
	tx(main,"U",&lv.tx),
	ty(main,"V",&lv.ty)
	{
		frame.spinner.set_speed(); //increment

		active_callback = &UvFrameWin::submit;

		submit('r'); submit('s'); submit('t');
	}

	struct Motion
	{
		multiple val;
		multiple::item none,lerp,step;

		Motion(node *p, utf8 name,
		Model::Interpolate2020E *lv, int id)
		:val(p,name,(int*)lv,id),
		none(&val,"None",(int)Model::InterpolateNone),
		lerp(&val,"Lerp",(int)Model::InterpolateLerp),
		step(&val,"Step",(int)Model::InterpolateStep)
		{
			val.row_pack();			
		}

		operator bool(){ return (bool)val; }
	};

	spinbox frame;
	Motion r; textbox rz;
	Motion s; textbox sx,sy;
	ok_cancel_panel ok_cancel;
	Motion t; textbox tx,ty;	

	void submit(int id)
	{
		switch(id)
		{
		case 'r': rz.enable(r); return;
		case 's': sx.enable(s); sy.enable(s); return;
		case 't': tx.enable(t); ty.enable(t); return;
		}
		basic_submit(id);
	}
};

struct UtilWin::UvAnimation : UtilWin::Plugin
{
	virtual bool submit(control*c);

	static void cbcb(int impl, checkbox_item &it)
	{
		if(~impl&it.impl_checkbox) return;

		union{ Plugin *p; char *os; };
		os = (char*)&it.list()-offsetof(UvAnimation,table);
		p->model->addGroupToUtility(p->u,it.id(),it.checkbox());
	}

	UvAnimation(MainWin &m, int u):Plugin(m,u),
	tab(main,id_tab),
	table(tab,"",id_item),
	header(table,false),
		name_col(header,"Name"),
		keys_col(header,"Keys"),
		x(name_col,"Animate",id_check_all),
	nav1(tab),
	val1(nav1,"U Unit",'u'),	
	nav2(nav1),
	val2(nav2,"V Unit",'v'),
	wrap(nav2,"Wrap",id_check),
		col1(nav1),	
	nav3(nav1),
	add(nav3,"Add",id_new),	
	del(nav3,"Delete",id_delete)
	{
		(checkbox_item::cbcb&) //YUCK
		table.user_cb = cbcb;

		tab.add_item("Groups").add_item("Frames");

		table.expand().drop(10*table.font().height);
		
		nav1.expand();

		val1.spinner->set_speed();
		val2.spinner->set_speed();
		val1.sspace<left>({val2});

		//HACK: Size table WRT the wider layout.
		tab.select_id(1); reset();
		tab.select_id(0); table.lock(true,true);
		groups_tab();
	}
	virtual void modelChanged(int ch)
	{
		if(ch&Model::AddOther&&!tab) 
		{
			groups_tab(ch); return;
		}		
		if(model->getUndoEnacted())
		if(ch&Model::AnimationProperty) //Units?
		{
			tab?frames_tab(ch):groups_tab(ch);
			return;
		}
	}
	virtual void reset()
	{		
		auto &ul = model->getUtilityList();
		up = (const Model::UvAnimation*)ul[u];
		submit(tab);
	}

	const Model::UvAnimation *up;

	ui::tab tab;	
	listbox table;
	listbar header;
	multiple::item name_col,keys_col;
	boolean x; 
	panel nav1;
	spinbox val1; //U/Frame&s
	row nav2;
	spinbox val2; //V/FPS
	boolean wrap;
	column col1;
	row nav3;
	button add,del;	

	void groups_tab(int ch=0)
	{
		table.clear();
		x.set_hidden(false);
		name_col.set_name("Name");
		keys_col.set_hidden(true);
		int swap = table.outline();
		int g = 0; for(auto*gp:model->getGroupList())
		{
			auto i = new checkbox_item(g++,gp->m_name.c_str());

			for(auto*cmp:gp->m_utils) if(cmp==up) i->check();

			table.add_item(i);
		}
		if(ch) //Undo, etc? 
		table.outline(std::min(swap,(int)table.size()-1));

		val1.set_name("U Unit");
		val1.edit(1.0,up->unit,8192.0);		
		val2.set_name("V Unit");
		val2.edit(1.0,up->vnit,8192.0);
		wrap.set_hidden(true);

		add.set_hidden(true); del.set_hidden(true);
	}
	void frames_tab(int ch=0)
	{
		table.clear();
		x.set_hidden(true);
		name_col.set_name("Frame").span(50);
		keys_col.set_hidden(false);
		int swap = table.outline();
		for(int i=0,n=(int)up->keys.size();i<n;i++)
		{
			auto it = new li::item(i);
			format_item(it->text(),up->keys[i]);
			table.add_item(it);
		}
		if(ch) //Undo, etc?  
		table.outline(std::min(swap,(int)table.size()-1));

		val1.set_name("Frame&s");
		val1.edit<double>(0,up->frames,INT_MAX);
		val2.set_name("FPS");
		val2.edit<double>(Model::setAnimFPS_minimum,up->fps,120); //1
		wrap.set_hidden(false);

		add.set_hidden(false); del.set_hidden(false);
	}
	void format_item(std::string &t, const UvFrameWin::Key &k)
	{
		char buf[64];
		sprintf(buf,"%g",k.frame);
		t.assign(buf).push_back('\0');
		sprintf(buf,k.r?"%.5g / ":"- / ",k.rz/PIOVER180);
		t.append(buf);
		sprintf(buf,k.s?"%.5g, %.5g / ":"-,- / ",k.sx,k.sy);
		t.append(buf);
		sprintf(buf,k.t?"%.5g, %.5g":"-,-",k.tx,k.ty);
		t.append(buf);
	}
};
bool UtilWin::UvAnimation::submit(control *c)
{
	switch(int id=c->id())
	{
	case id_tab:

		tab?frames_tab():groups_tab();
		break;

	case 'u': case 'v':

		if(tab)
		{
			if(id=='u') up->set_frames(*c);
			if(id=='v') up->set_fps(*c);
		}
		else up->set_units(val1,val2);
		break;

	case id_check: 

		up->set_wrap(wrap);
		break;
		
	case id_check_all: 

		table^[&](li::allitems i){ i->check(*c); };
		break;

	case id_item:

		switch(event.get_click())
		{
		case 2: //Double-click? 
			
			if(!tab) assoc:
			{
				auto it = table.find_line_ptr();
				it->check(!it->checkbox());
				return true;
			}
			else goto open; break;

		case 1:

			if(event.listbox_item_rename())
			{
				if(!tab) goto assoc;

				/*I'm just not sure I want to deal with sorting
				//by the frame time.
				if(header) goto open;
				textbox modal(table);
				modal.move_into_place(); modal.return_on_enter();*/
				if(tab) goto open; 
			}
			break;
		}
		return false;

	case id_new: open:
	{
		UvFrameWin::Key lv;
		size_t ln = table.find_line();
		if(ln<up->keys.size())
		{
			lv = up->keys[ln];
		}
		else assert(up->keys.empty());
		lv.rz/=PIOVER180;
		lv.tx*=up->unit;
		lv.ty*=up->vnit;
		if(id_cancel!=UvFrameWin(lv).return_on_close())
		{
			lv.rz*=PIOVER180;
			lv.tx/=up->unit;
			lv.ty/=up->vnit;			
			size_t cmp = up->keys.size();
			int k = up->set_key(lv);
			li::item *it = table.find_line_ptr(k);
			if(cmp<up->keys.size())
			{
				auto *ins = it; it = new li::item;
				table.insert_item(it,ins);
			}
			format_item(it->text(),lv);
			table.redraw();
			val1.set_float_val(up->frames);
		}
		else return false; break;
	}
	case id_delete:
	{
		if(up->delete_key(table.find_line()))
		table.delete_item(table.find_line_ptr());
		else return false; break;
	}
	default: return false;
	}
	return true; //operationComplete?
}

extern UtilWin::Plugin *uvanimutil(MainWin &m, int u)
{
	return new UtilWin::UvAnimation(m,u); 
}


