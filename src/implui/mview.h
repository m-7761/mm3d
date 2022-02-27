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


#ifndef __MVIEW_H__
#define __MVIEW_H__

#include "mm3dtypes.h" //PCH
#include "win.h"

#include "modelstatus.h"
#include "modelviewport.h"

// There is one or two ViewBar subwindows that are
// system drawing surfaces. Each column in the top
// level is a ModelView object. ViewPanel contains
// these 2 view bars.

struct ViewBar : Win
{
	struct ModelView;
	struct StatusBar;
	struct ParamsBar;

	void submit(control*);

	ViewBar(class MainWin&);

	MainWin &model;

	//NEW: The exterior sides are kind of a hack
	//to shoehorn in a parameters bar and status
	//bar without adding two more OpenGL windows.
	row exterior_row;
	row portside_row;
};	 	
struct ViewBar::ParamsBar
{
	row nav;

	ParamsBar(ViewBar &bar):nav(bar.exterior_row)
	{
		nav.space<left>(6);
		nav.style(~0xa0000); //shadow top/bottom.
	}

	control *new_boolean(bool *lv, utf8 name)
	{
		//HACK: Make appear to be lower to line up with
		//text boxes.
		//NOTE: The focus rectangles get cutoff if this
		//isn't done.
		auto *c = new boolean(nav,name,lv);
		return &c->space<top>(3).sspace(1,2);
	}
	template<class T>
	control *new_spinbox(T *lv, utf8 name, T min, T max)
	{
		auto *c = new spinbox(nav,name,lv);
		return &c->limit(min,max).compact().sspace(0,8);
	}
	control *new_dropdown(int *lv, utf8 name, const char **e)
	{
		auto *c = new dropdown(nav,name,lv);
		int i; for(i=0;*e;i++,e++) c->add_item(i,*e);
		*lv = std::min(i-1,std::max(0,*lv));
		return &c->compact().sspace(0,8);
	}
	void clear()
	{
		for(node*q,*p=nav.first_child();p;)
		{
			q = p; p = p->next(); q->_delete();
		}
		nav.repack(); //Not automatic.
	}
};
struct ViewBar::ModelView 
	
	//TODO: Would rather inherit from row.

	: column //REMOVE ME
{
	void submit(int);
	
	ModelView(ViewBar &bar, ModelViewport &port, ModelView *ref)
		:
	column(bar.portside_row),
	port(port),
	nav(bar.portside_row),
	view(nav,"",id_item),zoom(nav)
	{
		nav.expand().space(2);
		
		zoom.nav.ralign();

		if(ref) view.reference(ref->view);

		submit(id_init); //HACK
	}

	ModelViewport &port;

	row nav;
	dropdown view;
	Win::zoom_set zoom;

	void setView(Tool::ViewE dir)
	{
		port.viewChangeEvent(dir); view.select_id(+dir);
	}

	void init_view_dropdown();
};
struct ViewBar::StatusBar : StatusObject
{
	StatusBar(ViewBar &bar)
		:
	model(bar.model),
	m_queueDisplay(),
	nav(bar.exterior_row,bi::sunken),
	text(nav,""),
	flags(nav),	
	_vert_snap(flags,"Vs",id_snap_vert),
	_grid_snap(flags,"Gs",id_snap_grid),
	_uvfitlock(flags,"Fw",id_uv_editor), //?
	_interlock(flags,"Ex",id_frame_lock),
	_shiftlock(flags,"Sh",id_tool_shift_lock),
	_texshlock(flags,"Uv",id_tool_shift_lock),
	_shifthold(flags,"Lk",id_tool_shift_hold),
	_100(flags,"Wt",id_joint_100),
	_clipboard(flags,"Ins",id_animate_insert),	
	_keys_snap(flags,"Scr",id_animate_snap),
	_sanim_mode(flags,"Sam",id_animate_mode_1),
	_fanim_mode(flags,"Fam",id_animate_mode_2),
	stats(nav,"")
	{
		nav.expand();
		//There's extra space when there's just
		//one flag that I don't understand.
		//(Using 2 letter codes cancels it out.)
		//nav.space(1);
		nav.space(20);
		text.expand();
		text.title(true); //EXPERIMENTAL
		stats.ralign(); _curstats[0][0] = -1;
		flags.ralign().space(0,/*underscoring*/0,0);
	}
	~StatusBar();

	MainWin &model;

	row nav;
	textbox text;
	row flags;
	struct Flag:titlebar
	{
		Flag(row &r, utf8 c, int i):titlebar(r,c)
		{
			id(i); //if(hiding) 
			{
				set_hidden();

				//if(underscoring) _name.insert(0,1,'&');
			}
		}

		bool underscore(bool _)
		{
			int_val() = _; /*if(!hiding) //underscoring?
			{
				if(_==('&'==_name[0])) return;
				if(_) _name.insert(0,1,'&');
				else _name.erase(0,1); repack();
			}
			else*/ set_hidden(!_); return _;
		}

		//Note, these must be dissimilar where
	}	//unrelated.
	_vert_snap, //Vs
	_grid_snap, //Gs
	_uvfitlock, //Fw/Fh
	_interlock, //Ex (invertd)
	_shiftlock, //Sh
	_texshlock, //Uv 
	_shifthold, //Lk (inverted)
	_100, //1 //Wt (inverted)
	_clipboard, //Ins
	_keys_snap, //Scr (inverted)
	_sanim_mode, //Sam
	_fanim_mode; //Fam
	titlebar stats;

	// StatusObject methods
	virtual void setText(utf8 str);
	virtual void addText(StatusTypeE type, int ms, utf8 str);
	virtual void setStats();
	
	void setModel(Model*);
	
	void timer_expired();

	struct TextQueueItemT
	{
		StatusTypeE type;
		unsigned ms;
		std::string str;
	};
	std::list<TextQueueItemT> m_queue;
	bool m_queueDisplay;

	int _curstats[6][2]; //OPTIMIZING
};

#endif // __MVIEW_H__
