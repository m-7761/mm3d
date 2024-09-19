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

#include "log.h"
#include "msg.h"

//MM3D's table is not sortable. It needs more work
//I think to be sortable. unsort must go obviously.
enum{ animsetwin_sort=0 };

struct AnimSetWin : Win
{
	void submit(int);

	AnimSetWin(MainWin &model)
		:
	Win("Animation Sets"),model(model),
	type(main,"",id_item),
	table(main,"",id_subitem),
	header(table,animsetwin_sort,id_sort),
		name_col(header,"Name"),
		fps_col(header,"FPS"),
		frames_col(header,"Frames"),
		wrap(name_col,"Wrap",id_check),
	nav1(main),		
		up(nav1,"Up",id_up),
		down(nav1,"Down",id_down),
	nav2(main),
		add(nav2," New ",id_new), //YUCK 
		name(nav2,"Rename",id_name),
		del(nav2,"Delete",id_delete),
	nav3(main),
		copy(nav3,"Copy",id_copy),
		joint(nav3), //YUCK
		split(joint,"Split",id_split),
		join(joint,"Join",id_join),
		merge(nav3,"Merge",id_merge),
	nav4(main),
	convert(nav4,"Convert To Frame Animation",id_convert),
	clean(nav4,"Clean Up",id_remove),
	f1_ok_cancel(main)	
	{	
		(checkbox_item::cbcb&) //YUCK
		table.user_cb = cbcb;

		type.expand();		
		nav1.ralign();
		nav2.expand_all().proportion();
		//nav3.expand_all().proportion();
		nav3.expand();		
		joint.expand_all().proportion();
		merge.ralign();
		nav4.expand();
		convert.expand();
		clean.ralign();
	
		//The columns should support the window.
		//But just to be safe.
		table.expand();

		//TODO: Maybe resize as the list changes
		//up to some maximum.
		table.drop()+=table.font().height; //2022

		wrap.ctrl_tab_navigate();
		//name_col.expand(); //Doesn't work yet.
		name_col.lock(150,false);
		//NOTE: The end column auto-extends, so
		//it's better to have the short name as
		//the middle column.
		fps_col.span() = frames_col.span() = 0;

		main->pack(); clean.span(del.span());

		active_callback = &AnimSetWin::submit;

		submit(id_init);
	}

	MainWin &model;
	int mode; //Model::AnimationModeE mode;

	dropdown type; listbox table;
	listbar header;
	multiple::item name_col,fps_col,frames_col;
	boolean wrap; 
	row nav1; button up,down;
	row nav2; button add,name,del;
	row nav3; button copy;
	row joint; button split,join;
	button merge;
	row nav4;
	button convert,clean;
	f1_ok_cancel_panel f1_ok_cancel;

	void refresh();

	checkbox_item *new_item(int id)
	{
		auto i = new checkbox_item(id,nullptr);
		format_item(i); return i;
	}
	void format_item(li::item *i)
	{
		int id = i->id();
		utf8 n = model->getAnimName(id);		
		double fps = model->getAnimFPS(id);
		double frames = model->getAnimTimeFrame(id);
		i->check(model->getAnimWrap(id));
		format_item(i,n,fps,frames);
	}
	void format_item(li::item *i, const char *n, double fps, double frames)
	{
		i->text().format(&"%s\0%g\0%g",n,fps,frames);
	}

	static void cbcb(int impl, checkbox_item &it)
	{
		if(~impl&it.impl_checkbox) return;

		auto w = (AnimSetWin*)it.list().ui();
		w->model->setAnimWrap(it.id(),it.checkbox());
	}

	//HACK: Since the columns can be sorted it's
	//necessary to undo it. This should probably
	//throw up a prompt.
	bool unsort()
	{
		if(!animsetwin_sort) return true;

		if(header.text().empty()) return true;
				
			//Prompt?

		header.clear();
		header.sort_items([&](li::item *a, li::item *b)
		{
			return a->id()<b->id();
		});		

		return true;
	}
};

struct AnimEditWin : Win //EditWin
{
	AnimEditWin(AnimSetWin &win, int id, li::item *i)
		:
	Win(id==id_new?"New Animation":"Rename Animation"),
		win(win),conv(),
	type(main),
	name(main,"Name"), //"New name:"
	nav(main),
	fps(nav,"FPS"),frames(nav,"Frames"),
	ok_cancel(main)
	{
		type.reference(win.type);
		type.select_id(win.type);

		double c1 = 30, c2 = !1; if(i)
		{
			//It's a problem if merely "focused".
			//i->select();
			assert(i->multisel());
			utf8 row[3]; i->text().c_row(row);
			name.set_text(row[0]);
			c1 = strtod(row[1],nullptr);
			c2 = strtod(row[2],nullptr);
		}
		else if(id==id_name)
		{
			//Must be multi-selection, so don't
			//give impression all names will be
			//reassigned.
			name.disable();
		}
		fps.edit<double>(1,c1,120);
		frames.edit<double>(!1,c2,INT_MAX);

		//HACK: Cleared boxes mean don't change.
		//This is essential for multi-selection.
		if(!i) fps.text().clear();
		if(!i) frames.text().clear();

		type.expand();
		name.place(bottom).expand();
		fps.place(bottom);
		fps.compact(ok_cancel.cancel.span());
		frames.place(bottom);
		frames.compact(ok_cancel.ok.span());

		if(2==event.get_click()) switch(win.header)
		{
		case 0: name.activate(); break;
		case 1: fps.activate(); break;
		case 2: frames.activate(); break;
		}
		else name.activate();

		if(id!=id_new)
		active_callback = &AnimEditWin::confirm_conv;
	}

	AnimSetWin &win;
	Model::AnimationModeE conv;
	
	dropdown type;
	textbox name;
	row nav;
	textbox fps,frames;
	ok_cancel_panel ok_cancel;

	void confirm_conv(int id)
	{
		if(id!=id_ok) return basic_submit(id);
		
		int a = type, b = win.type; if(a!=b&&a!=3)
		{
			b = Model::KM_Joint; if(a==1) b = ~b;

			int n = 0; std::string msg;

			win.table^[&](li::multisel ea)
			{
				n = n?id_yes|id_no:id_ok;
			};				
			win.table^[&](li::multisel ea)
			{
				if(0==(id&(id_no|id_cancel)))
				if(win.model->hasKeyframeData(ea->id(),b))
				{
					msg.reserve(255);
					msg.assign(n==id_ok?"This animation":"At least 1 animation (");
					if(n!=id_ok) 
					msg.append(ea->text().c_str()).push_back(')'); //text is a row
					msg.append(" will lose data with type conversion.");
					if(n!=id_ok)
					msg.append("\nContinue scanning for data loss?");
					id = Win::InfoBox(::tr("Potential data loss!"),
					msg.c_str(),n|id_cancel,id==id_yes?id_yes:id_cancel);
				}
			};
			if(id&(id_yes|id_no))
			id = Win::InfoBox(::tr("Accept data loss?"),
			::tr("The converted animations will lose data incompatible with their new animation type.\n"
			"Is this acceptable?"),id_ok|id_cancel,id_cancel);
		}
		if(id!=id_cancel) 
		{
			conv = (Model::AnimationModeE)a; basic_submit(id_ok);
		}
	}
};

struct AnimConvertWin : Win
{
	void submit(int);

	AnimConvertWin(AnimSetWin &owner)
		:
	Win("Convert To Frame Animation"),owner(owner),
	//table(main,"Convert Skeletal to Frame:",id_item),
	table(main,"",id_item),
		header(table,id_sort),
	f1_ok_cancel(main)
	{
		active_callback = &AnimConvertWin::submit;

		submit(id_init);
	}

	AnimSetWin &owner;

	listbox table;
	listbar header;
	f1_ok_cancel_panel f1_ok_cancel; //"Convert"
};
void AnimConvertWin::submit(int id)
{
	switch(id)
	{
	case id_init:
 
		//Assuming table will be auto-sized.
		header.add_item("Skeletal Animation"); //200
		header.add_item("Frame Animation"); //200
		//header.add_item("Frame Count"); //30
		header.add_item("Samples Count"); //30
		owner.table^[&](li::multisel ea)
		{
			auto row = new li::item(ea->id());

			utf8 name = ea->text().c_str();
			row->text().format(&"%s\0%s\0%g",name,name,
			owner.model->getAnimTimeFrame(ea->id()));
			
			table.add_item(row);
		};
		f1_ok_cancel.ok_cancel.ok.name("Convert");
		break;

	case id_sort:

		header.sort_items([&](li::item *a, li::item *b)
		{
			utf8 row1[3]; a->text().c_row(row1);
			utf8 row2[3]; b->text().c_row(row2);
			switch(int col=header)
			{
			case 2: return atoi(row1[col])<atoi(row2[col]);
			default: return strcmp(row1[col],row2[col])<0;
			}
		});
		break;
	
	case id_item:
	
		switch(int col=header)
		{
		case 0: col = 1; default:

			textbox modal(table);
			if(2==col) modal.edit<int>(!1,INT_MAX);
			if(modal.move_into_place(table.outline(),col)) 
			modal.return_on_enter();
		}
		break;
	
	case id_ok:

		table^[&](li::allitems ea)
		{
			utf8 row[3]; ea->text().c_row(row);
			auto fix_me = Model::InterpolateLerp; //FIX ME
			owner.model->convertAnimToFrame(ea->id(),row[1],atoi(row[2]),fix_me);
		};
		break;
	}

	basic_submit(id);
}

struct AnimCleanupWin : Win
{
	//NOTE: This used for InterpolateStep comparison.
	static constexpr double eps = 0.00000001;

	AnimCleanupWin(MainWin &m, AnimSetWin *o=nullptr)
		:
	Win("Clean Up"),model(m),owner(o),
	a(main),
	b(main,"Removed identical keys"),
	c(main,"Removed unused frames"),
	ok_cancel(main),
	ops(main.inl,"Checklist"),
	sel(ops,"Select"),
	pos(ops,"Position"),
	rot(ops,"Rotation"),
	scl(ops,"Scale"),
	reset(ops,"Reset Checked Boxes",id_reset)
	{
		ok_cancel.ok.activate();

		int mode = owner?owner->mode:3;

		auto &n = model.nselection;
		if(mode&1&&n[Model::PT_Joint]
		 ||mode&2&&n[Model::PT_Point]
		 ||mode&2&&n[Model::PT_Vertex])
		{
			if(owner) sel.box.set();
		}
		else sel.box.disable();

		//The right panel was just 1px shy
		//of the OK button, just dumb luck.
		//The extra space is more centered.
		ok_cancel.nav.space<top>()+=1;

		reset.expand();
				
		active_callback = &AnimCleanupWin::submit;

		pos.tol.edit(eps,100.0); //0.0001 //1/10th millimeter
		rot.tol.edit(eps,0.0001); //0.00001 //0.0001 is too high for dot4.
		scl.tol.edit(eps,100.0); //0.0001 //???
		auto ser = config->get("ui_anim_clean","0.0001,0.00001,0.0001,1,1,1,3,3,1,1");
		{
			float f[3]; int d[7];
			sscanf(ser,"%f,%f,%f,%d,%d,%d,%d,%d,%d,%d",f+0,f+1,f+2,d+0,d+1,d+2,d+3,d+4,d+5,d+6);
			pos.tol.set_float_val(f[0]); pos.box.set_int_val(d[0]);
			rot.tol.set_float_val(f[1]); rot.box.set_int_val(d[1]);
			scl.tol.set_float_val(f[2]); scl.box.set_int_val(d[2]);
			a.k.select_id(d[3]); a.v.select_id(d[4]);
			b.set_int_val(d[5]); c.set_int_val(d[6]);
		}	
	}

	MainWin &model; AnimSetWin *owner;

	struct convert
	{
		panel nav;
		
		dropdown k,v;
		
		convert(node *main)
			:
		nav(main,"Convert"),		
		k(nav,"Match"),v(nav,"Motion")
		{
			//REMINDER: I didn't share
			//this list because having
			//<None><None> would imply
			//no-action, and require a
			//pop-up warning.
			k.add_item(0,"<All>");
			k.add_item(3,"Lerp");
			k.add_item(2,"Step");
			v.add_item(0,"<None>");
			v.add_item(3,"Lerp");
			v.add_item(2,"Step");			
			
			//This just communicates a
			//way to disable this step
			//so it doesn't have to be
			//left in a blank state to
			//avoid taking this action.
		//	k.select_id(3);
		//	v.select_id(3);
		}
	}a;
	boolean b,c;
	ok_cancel_panel ok_cancel;
	panel ops;
	struct sel //SINGLETON
	{
		row nav;
		titlebar tol;
		boolean box;
		sel(node *main, utf8 name)
		:nav(main),tol(nav,"Key tolerance"),box(nav,name)
		{
			nav.space<center>()-=1;
			box.space<top>()-=3;
		}

		operator bool(){ return box; }

	}sel;
	struct op
	{
		row nav;
		textbox tol;
		boolean box;
		op(node *main, utf8 name)
		:nav(main),tol(nav),box(nav,name)
		{
			box.space<top>()+=3;
			//box.set();
		}

		operator bool(){ return box; }

	}pos,rot,scl;
	button reset;

	struct ok
	{
		Model *m;
		bool sel;
		int mask;		
		unsigned anim;
		const Model::Animation *ap;
		size_t fc;
		
		//TODO: animwin.cc will need access to
		//piecewise version of these functions.
		//Maybe they should be Model functions.
		void interpolate(int,int);
		void remove_keys(double,double,double);
		void remove_frames();
	};
	void submit(int id)
	{
		if(id==id_reset)
		{
			if(pos) pos.tol.set_float_val(0.0001);
			if(rot) rot.tol.set_float_val(0.00001);
			if(scl) scl.tol.set_float_val(0.0001);
		}
		else if(id==id_ok)
		{
			bool aa = (int)a.k!=(int)a.v||!a.k;
			bool bb = b;
			bool cc = c;
			int mask = 0;
			{
				if(pos) mask|=Model::KeyTranslate;
				if(rot) mask|=Model::KeyRotate;
				if(scl) mask|=Model::KeyScale;
			}
			if(!mask)
			{
				//TODO: Warning??? return?
				aa = bb = false;
			}

			if(aa||bb||cc)
			{
				ok o = {model,sel,mask};

				if(owner) owner->table^[&](li::multisel ea)
				{
					o.anim = ea->id();
					o.ap = o.m->getAnimationList()[o.anim];
					o.fc = o.ap->_frame_count();
					if(aa) o.interpolate(a.k,a.v);
					if(bb) o.remove_keys(pos.tol,rot.tol,scl.tol);
					if(cc) o.remove_frames();
				};
				else for(auto*ap:model->getAnimationList()) 
				{
					o.ap = ap;
					o.fc = ap->_frame_count();
					if(aa) o.interpolate(a.k,a.v);
					if(bb) o.remove_keys(pos.tol,rot.tol,scl.tol);
					if(cc) o.remove_frames();
					o.anim++;
				}

				if(owner) o.m->updateObservers();
				else model->operationComplete("Animation Cleanup");
			}

			char buf[64]; //15*3+2*7+1
			sprintf(buf,"%.10f,%.10f,%.10f,%d,%d,%d,%d,%d,%d,%d",
			(double)pos.tol,(double)rot.tol,(double)scl.tol,
			(int)pos.box,(int)rot.box,(int)scl.box,
			(int)a.k,(int)a.v,(int)b,(int)c);
			config->set("ui_anim_clean",buf);
		}
		basic_submit(id);
	}
};

struct AnimMirrorWin : Win //EXPERIMENTAL
{
	AnimMirrorWin()
		:
	Win("Mirror"),
	nav(main),
	x(nav,"X"),y(nav,"Y"),z(nav,"Z"),
	type(main),
	ok_cancel(main)
	{
		type.row_pack();
		type.add_item(0,"Rotation");
		type.add_item(1,"Translation");
	}

	row nav;
	boolean x,y,z;
	multiple type;
	ok_cancel_panel ok_cancel;	
};

extern void animsetwin_clean(MainWin &m)
{
	AnimCleanupWin(m).return_on_close();
}

void AnimSetWin::submit(int id)
{
	switch(id)
	{
	case id_init:
	{	
		type.add_item(2,::tr("Frame Animation"));
		type.add_item(1,::tr("Skeletal Animation"));
		type.add_item(3,::tr("Complex Animation"));
		//I'd like to add this as a mode to combine
		//real animations, and add a hide flag when
		//animations aren't intended to be exported.
		//type.add_item(0,::tr("Composite Animation"));

		int n = model->getCurrentAnimation();
		auto m = model->getAnimType(n);		
		type.select_id(m?m:Model::ANIMMODE);
		submit(id_item); if(m)
		{
			n-=model->getAnimationIndex(m);
			table.outline(n);
			table.find_line_ptr()->select();
			table.show_line(n);
		}
		break;
	}
	case id_item:
	{
		if(mode!=(int)type) table.clear();

		mode = type; refresh(); 
		
		break;
	}
	case id_check:

		table^[&](li::multisel ea){ ea->check(wrap); };
		break;

	case id_sort:
	
		if(animsetwin_sort)
		header.sort_items([&](li::item *a, li::item *b)
		{
			utf8 row1[3]; a->text().c_row(row1);
			utf8 row2[3]; b->text().c_row(row2);
			switch(int col=header)
			{			
			case 0: return strcmp(row1[col],row2[col])<0;
			default: //compiler
			return strtod(row1[col],0)<strtod(row2[col],0);
			}
		});
		break;
	
	case id_subitem: 
	
		switch(event.get_click())
		{
		case 2: id = id_name; break; //Double-click?
		case 1:

			//This test implements Windows Explorer
			//select, wait, single-click, to rename 
			//logic.
			if(event.listbox_item_rename(true,true))
			{
				textbox modal(table); switch(header)
				{
				case 1: modal.edit<double>(1,120); break;
				case 2: modal.edit<double>(!1,INT_MAX); break;
				}

				if(modal.move_into_place())
				if(modal.return_on_enter()) switch(header)
				{
				case 0: model->setAnimName((int)table,modal); break;
				case 1: model->setAnimFPS((int)table,modal); break;
				case 2: model->setAnimTimeFrame((int)table,modal); break;
				}
			}
			//break;

		case 0: return; //2022
		}
		//break; //FALLING-THROUGH
	
	case id_name: case id_new:
	{
		int j = 0;
		li::item *it = nullptr;
		table^[&](li::multisel ea)
		{
			j++; it = ea;
		};		
		if(!j&&id==id_name)
		{
			//Should probably disable Delete/Rename then?
			return event.beep();
		}
		if(j>1) it = nullptr;
		AnimEditWin e(*this,id,it);		
		if(id_ok!=e.return_on_close())
		return;
		int converted = 0;
		utf8 c_str = e.name.c_str();
		j = -1; table^[&](li::multisel ea)
		{
			if(id==id_new)
			{
				if(j==-1) j = ea->id(); ea->unselect();
			}
			else //id_name
			{
				if(*c_str)
				model->setAnimName(ea->id(),c_str);
				if(!e.fps.text().empty())
				model->setAnimFPS(ea->id(),e.fps);
				if(!e.frames.text().empty())
				model->setAnimTimeFrame(ea->id(),e.frames);
				format_item(ea,c_str,e.fps,e.frames);
				if(e.conv)
				{
					int i = model->convertAnimToType(e.conv,ea->id()-converted);
					if(j==-1)
					j = i-model->getAnimationIndex(e.conv);
					converted++;
				}
			}
		};
		e.conv = Model::AnimationModeE((int)e.type);
		if(id==id_new)
		{
			int i = model->addAnimation(e.conv,c_str);
			if(i==-1) break;

			if(!e.fps.text().empty())
			model->setAnimFPS(i,e.fps);
			if(!e.frames.text().empty())
			model->setAnimTimeFrame(i,e.frames);
			if(j==-1) j = i;
			if(mode==e.conv) 
			{
				if(i!=j)
				model->moveAnimation(i,j); //NEW

				auto *jt = &new_item(j)->select();

				if(!it) table.add_item(jt);
				else table.insert_item(jt,it);

				refresh();
			}
			else converted = 1;
			j-=model->getAnimationIndex(e.conv);
		}
		if(j!=-1) //New or conversion?
		{
			if(converted)
			{
				mode = e.conv;
				type.select_id(mode);
				table.clear();
				refresh(); 
				int i = 0; table^[&](li::allitems ea)
				{
					if(i++>=j&&i<=j+converted) ea->select();
				};
			}
			table.outline(j);
		}		
		break;
	}		
	case id_up:

		if(!table.first_item()->multisel())
		{
			table^[&](li::multisel ea)
			{
				table.insert_item(ea,ea->prev());
				model->moveAnimation(ea->id(),ea->id()-1);
			};
			refresh();
		}
		else event.beep(); break;

	case id_down:
		
		if(!table.last_item()->multisel())
		{
			table^[&](li::reverse_multisel ea)
			{
				table.insert_item(ea,ea->next(),behind);
				model->moveAnimation(ea->id(),ea->id()+1);
			};
			refresh();
		}
		else event.beep(); break;

	case id_delete:
	case id_copy: case id_mirror:
	case id_split: case id_join: case id_merge:
	{
		unsort(); //YUCK

		li::item *i; //id_copy/id_merge

		bool beep = true;

		int a = -1, b = 0; table^[&](li::multisel ea)
		{
			beep = false; switch(id)
			{
			case id_delete:

				model->deleteAnimation(ea->id()-b);
				b++;
				table.erase(ea);
				break;

			case id_copy:

				unsort(); //YUCK

				model->copyAnimation(ea->id());
				b++;
				table.insert(ea->next(),new_item(ea->id()+b)); 
				break;

			case id_mirror: //EXPERIMENTAL
			{
				unsort(); //YUCK

				AnimMirrorWin w;
				if(id_ok!=w.return_on_close())
				return;

				if((int)w.type==0) model->mirrorRotation(ea->id(),w.x,w.y,w.z);
				if((int)w.type==1) model->mirrorTranslation(ea->id(),w.x,w.y,w.z);

				b++;
				table.insert(ea->next(),new_item(ea->id()+b)); 
				break;
			}
			case id_split:
								
				a = ea->id(); /*b = model->getAnimFrameCount(a); if(b<2)
				{
					InfoBox(::tr("Cannot Split","Cannot split animation window title"),
					::tr("Must have at least 2 frames to split","split animation"),id_ok);
				}
				else*/ //SIMPLIFY ME
				{	
					std::string name;
					name = ::tr("Split","'Split' refers to splitting an animation into two separate animations");
					name.push_back(' '); //MERGE US 
					name.append(model->getAnimName(a));
					name.push_back(' '); //MERGE US
					name.append(::tr("at frame number","the frame number where the second (split) animation begins"));

					//int split = b/2;
					double b = model->getAnimTimeFrame(a);
					double t = b/2;
					if(id_ok!=EditBox(&t,::tr("Split at frame","Split animation frame window title"),name.c_str(),/*2*/0,b))
					return;

					int split = model->insertAnimFrame(a,t);
					
					name = model->getAnimName(a);
					name.push_back(' ');
					name.append(::tr("split"));
					if((split=model->splitAnimation(a,name.c_str(),split))<0)
					return;
		
					format_item(ea);

					a++; assert(split==a);

					ea = ea->next(); //HACK
					{
						table.insert(ea,new_item(a));
					}							
					for(;ea;ea.item=ea->next()) //HACK: Increment the IDs.
					{
						ea->id() = ++a;
					}
				}
				break;
			
			case id_merge: case id_join: 
			
				if(a==-1)
				{
					a = ea->id(); i = ea; beep = true;
				}
				else
				{	
					if(id==id_join)				
					if(beep=!model->joinAnimations(a,ea->id()+b))
					return; //shouldn't occur
					if(id==id_merge)
					if(beep=!model->mergeAnimations(a,ea->id()+b))
					return; //shouldn't occur

					b--;

					format_item(i); //update Frames field?

					table.erase(ea); 
				}
				break;
			}
		};

		if(beep) event.beep();

		refresh(); break;
	}
	case id_convert:
	
		AnimConvertWin(*this).return_on_close(); 
		break;

	case id_remove:
	
		AnimCleanupWin(model,this).return_on_close(); 
		break;
	
	case id_ok:
			
		model->operationComplete(::tr("Animation changes","operation complete"));
		break;

	case id_cancel:

		model->undoCurrent(); break;
	}

	basic_submit(id);
}
void AnimSetWin::refresh()
{
	int ii = model->getAnimationIndex((Model::AnimationModeE)mode);

	int iN = 0; if(!table.empty())
	{
		unsort();
		table^[&](li::allitems ea){ ea->id() = ii+iN++; };
	}
	else if(iN=model->getAnimationCount((Model::AnimationModeE)mode))
	{
		main_panel()->enable();

		//2020: It looks like I've implemented this in
		//mergeAnimations but since I didn't enable it
		//here I likely neglected to test the new code
		//merge.enable(mode==Model::ANIMMODE_JOINT);
		convert.enable(mode!=Model::ANIMMODE_FRAME);
				
		for(int i=0;i<iN;i++) table.add_item(new_item(ii+i));

		//MM3D's table is not sortable. It needs more work
		//I think to be sortable. unsort must go obviously.
		if(!animsetwin_sort)
		{
			//header.disable(); wrap.enable(); 
		}
	}
	else
	{
		main_panel()->disable();
		type.enable();
		add.enable();
		f1_ok_cancel.nav.enable();
	}
}

extern void animsetwin(MainWin &m){ AnimSetWin(m).return_on_close(); }

void AnimCleanupWin::ok::interpolate(int key, int val)
{
	for(auto&kv:ap->m_keyframes)
	for(auto*kp:kv.second)
	if(mask&kp->m_isRotation)
	{
		auto e = kp->m_interp2020;
		if(e>=Model::InterpolateStep) if(e==key||!key)
		{
			m->setKeyframe(anim,kp,nullptr,(Model::Interpolate2020E)val);
		}
	}

	auto f0 = ap->m_frame0;
	if(~0!=f0&&mask&Model::KeyTranslate)
	{
		auto &vl = m->getVertexList();
		for(int v=(int)vl.size();v-->0;)
		{
			auto *pp = &vl.data()[v]->m_frames.data()[f0];
			for(auto f=fc;f-->0;)		
			{
				auto*c = pp[f]->m_coord;
				auto e = pp[f]->m_interp2020;
				if(e>=Model::InterpolateStep) if(e==key||!key)	
				{					
					m->setFrameAnimVertexCoords(anim,f,v,c,(Model::Interpolate2020E)val);
				}
			}
		}
	}
}
void AnimCleanupWin::ok::remove_keys(double pt, double rt, double st)
{
	rt = 1-rt; //0.9999 is default

	auto tt = ap->m_timetable2020.data();

	std::vector<char> l;
	for(auto&kv:ap->m_keyframes)
	if(!sel||m->isPositionSelected(kv.first))
	{
		size_t sz = kv.second.size();
		int n = (int)sz;
		l.assign(sz,true);

		char *lp = l.data();
		auto keys = kv.second.data();

		Quaternion q,qq,cq;
		int p[3] = {-1,-1,-1};
		int pp[3] = {-1,-1,-1};
		for(int i=0;i<n;i++)
		{
			auto *kp = keys[i];

			int j = kp->m_isRotation;

			if(0==(mask&j)) continue;

			j>>=1;

			if(pp[j]==-1)
			{
				pp[j] = i; 
				
				if(j==Model::InterpolantRotation)
				qq.setEulerAngles(kp->m_parameter);

				continue;
			}
			else if(p[j]==-1)
			{
				p[j] = i; 
				
				if(j==Model::InterpolantRotation)
				q.setEulerAngles(kp->m_parameter);

				continue;
			}

			bool identical = true;
			{
				auto &a = *keys[pp[j]];
				auto &b = *keys[p[j]];
				auto &c = *kp;

				double cmp[4];			
				double t = tt[a.m_frame];
				t = (tt[b.m_frame]-t)/(tt[c.m_frame]-t);

				//WARNING: I'm winging it with Step/Copy here :(
				auto e = b.m_interp2020;

				if(e==Model::InterpolateCopy)
				{
					identical = false; //Assume there's a reason?
				}
				else if(e==Model::InterpolateStep)
				{
					//This is just requiring all 3 to be identical?
					for(int k=3;k-->0;)
					if(fabs(a.m_parameter[k]-b.m_parameter[k])>eps
					 ||fabs(b.m_parameter[k]-c.m_parameter[k])>eps)
					{
						identical = false; break;
					}
				}
				else if(j==Model::InterpolantRotation)
				{
					cq.setEulerAngles(c.m_parameter);
					a.slerp(qq.getVector(),cq.getVector(),t,cmp);

					double *cmp2 = q.getVector();

					/*if(0) //REFERENCE? //TESTING
					{
						//I'm not so sure the "axis" part of the
						//quaternion can be measured in isolation
						//of the angle part. In my tests the axis
						//varied with the angle even though the
						//Euler angle only moved on one component.
						
						//TODO: What's an appropriate 
						//magnitude???
						constexpr double eps1 = 0.0005; //axis
						constexpr double eps2 = 0.005; //angle

						//Maybe the quaternions should compare a 
						//rotated vector?
						for(int k=3;k-->0;)
						if(fabs(cmp[k]-cmp2[k])>eps1)
						{
							identical = false; 
						}
						if(fabs(cmp[3]-cmp2[3])>eps2)
						{
							identical = false; 
						}
					}
					else //BLACK MAGIC?*/ //TESTING
					{
						//NOTE: In my tests 0.9999 is a minimum and
						//0.99999 seems too much (one decimal place
						//difference) so there isn't much wiggle if
						//this approach is chosen. My hunch is this
						//is iffy and isn't highly configurable for
						//end-users. What's desired I think is just
						//what looks imperceptible to the human eye.
						//But at what scale?
						if(dot4(cmp2,cmp)<rt) //0.9999
						{
							identical = false;					
						}
					}

					if(!identical) qq = q; q = cq;
				}
				else
				{
					a.lerp(a.m_parameter,c.m_parameter,t,cmp);

					double tol = j?st:pt;
					double *cmp2 = b.m_parameter;
					for(int k=3;k-->0;)
					if(fabs(cmp[k]-cmp2[k])>tol)
					{
						identical = false;
					}
				}
			}

			if(identical)
			{
				lp[p[j]] = false;
			}
			else pp[j] = p[j]; p[j] = i;
		}

		for(auto i=n;i-->0;) if(!lp[i])
		{
			if(!m->deleteKeyframe(anim,kv.second[i]))
			assert(0);
		}
	}

	int v = -1;
	auto f0 = ap->m_frame0;
	if(~0!=f0)
	if(mask&Model::KeyTranslate)
	for(auto*vp:m->getVertexList())
	{
		if(sel&&!vp->m_selected) continue;

		int n = (int)fc;
		l.assign(fc,true);
		char *lp = l.data();
		int p = -1, pp = -1;
		auto *keys = &vp->m_frames.data()[f0];
		for(auto i=0;i<n;i++)
		{
			auto *kp = keys[i];

			if(!kp->m_interp2020) continue;

			if(pp==-1)
			{
				pp = i; continue;
			}
			else if(p==-1)
			{
				p = i; continue;
			}

			bool identical = true;
			{
				auto &a = *keys[pp];
				auto &b = *keys[p];
				auto &c = *kp;
			
				//WARNING: I'm winging it with Step/Copy here :(
				auto e = b.m_interp2020;

				if(e==Model::InterpolateCopy)
				{
					identical = false; //Assume there's a reason?
				}
				else if(e==Model::InterpolateStep)
				{
					//This is just requiring all 3 to be identical?
					for(int k=3;k-->0;)
					if(fabs(a.m_coord[k]-b.m_coord[k])>eps
					 ||fabs(b.m_coord[k]-c.m_coord[k])>eps)
					{
						identical = false; break;
					}
				}
				else
				{			
					double cmp[4];			
					double t = tt[pp];
					t = (tt[p]-t)/(tt[i]-t);

					a.lerp(a.m_coord,c.m_coord,t,cmp);

					double *cmp2 = b.m_coord;

					for(int k=3;k-->0;)
					if(fabs(cmp[k]-cmp2[k])>pt) //eps
					{
						identical = false;
					}
				}
			}

			if(identical)
			{
				lp[p] = false;
			}
			else pp = p; p = i;			
		}

		v++; for(auto i=n;i-->0;) if(!lp[i])
		{
			m->deleteFrameAnimVertex(anim,i,v);
		}
	}
}
void AnimCleanupWin::ok::remove_frames()
{
	std::vector<char> l(fc,false);
	char *lp = l.data();

	for(auto&kv:ap->m_keyframes)
	for(auto*kp:kv.second)
	{
		//assert(kp->m_interp2020);

		lp[kp->m_frame] = true;
	}

	auto f0 = ap->m_frame0;
	if(~0!=f0)
	for(auto*vp:m->getVertexList())
	{
		auto *pp = &vp->m_frames.data()[f0];
		for(auto f=fc;f-->0;)
		{
			if(pp[f]->m_interp2020) lp[f] = true;
		}
	}

	for(auto f=fc;f-->0;) if(!lp[f])
	{
		m->deleteAnimFrame(anim,f);
	}
}
