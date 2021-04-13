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
		loop(name_col,"Wrap",id_check),
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
	convert(main,"Convert To Frame Animation",id_convert),
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
		convert.expand();		
	
		//The columns should support the window.
		//But just to be safe.
		table.expand();

		loop.ctrl_tab_navigate();
		//name_col.expand(); //Doesn't work yet.
		name_col.lock(150,false);
		//NOTE: The end column auto-extends, so
		//it's better to have the short name as
		//the middle column.
		fps_col.span() = frames_col.span() = 0;

		active_callback = &AnimSetWin::submit;

		submit(id_init);
	}

	MainWin &model;
	int mode; //Model::AnimationModeE mode;

	dropdown type; listbox table; //MERGE US?
	listbar header;
	multiple::item name_col,fps_col,frames_col;
	boolean loop; 
	row nav1; button up,down;
	row nav2; button add,name,del;
	row nav3; button copy;
	row joint; button split,join;
	button merge, convert;
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
		assert(impl&it.impl_checkbox);
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
			conv = Model::AnimationModeE(a); basic_submit(id_ok);
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
	f1_ok_cancel_panel f1_ok_cancel;
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

		table^[&](li::multisel ea){ ea->check(loop); };
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
		default: //Spacebar????
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
			return;		
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
				model->moveAnimation(i,j); //NEW
				table.add_item(new_item(j)->select());
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
	case id_copy: case id_split: case id_join: case id_merge:
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
	int iN = 0; if(!table.empty())
	{
		unsort();
		table^[&](li::allitems ea){ ea->id() = iN++; };
	}
	else if(iN=model->getAnimationCount((Model::AnimationModeE)mode))
	{
		main_panel()->enable();

		//2020: It looks like I've implemented this in
		//mergeAnimations but since I didn't enable it
		//here I likely neglected to test the new code
		//merge.enable(mode==Model::ANIMMODE_SKELETAL);
		convert.enable(mode!=Model::ANIMMODE_FRAME);

		int ii = model->getAnimationIndex((Model::AnimationModeE)mode);
		
		for(int i=0;i<iN;i++) table.add_item(new_item(ii+i));

		//MM3D's table is not sortable. It needs more work
		//I think to be sortable. unsort must go obviously.
		if(!animsetwin_sort)
		{
			//header.disable(); loop.enable(); 
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