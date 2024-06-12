/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2008 Kevin Worcester
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


#ifndef __ANIMWIN_H__
#define __ANIMWIN_H__

#include "mm3dtypes.h" //PCH
#include "win.h"

#include "graphs.h" //WIP

struct AnimWin : Win
{
	void init(),submit(int);
	
	void sync(),open(bool undo);

	void setModel(){ open(true); }

	void modelChanged(int); //2021

	void _sync_anim_menu();
	void _sync_tools(int,int);
	void _show_tool(int);
	
	enum /*Animation controls*/
	{		
		id_anim_fps=1000,
		id_anim_frames,
		id_stf,id_sfc,id_key,
	};

	AnimWin(class MainWin &model, int menu)
		:
	Win(::tr("<None>"),&graphic),
	menu(menu),
	model(model),impl(),
	nav(main),
	animation(nav,"",id_item),
	frame_nav(nav),
	frame(frame_nav,"",id_subitem), //"Frame"
	play(frame_nav,"",id_animate_play),
	loop(frame_nav,"",id_animate_loop),
		//MouseNone
		anim_nav(nav),
		fps(anim_nav,"FPS",id_anim_fps),
		wrap(anim_nav,"Wrap",id_check), //"Loop"
		frames(anim_nav,"Frame&s",id_anim_frames),
		del(anim_nav,"Delete",id_delete), //"X"
		close(anim_nav,"Close",id_close),
		//MouseSelect
		tool_nav(nav),
		select(tool_nav,"Include",id_select), //"Aim"
		select_frame(select,"Frame",1),
		select_vertex(select,"Vertex",2),
		select_complex(select,"Complex",0),
		//MouseMove
		move_snap(tool_nav,"Snap to Frame",id_stf),
		//MouseScale
		scale(tool_nav),
	//	scale_sfc(scale,"Scale from center",id_sfc),
	//	scale_kar(scale,"Keep aspect ratio",id_kar), //???
		scale_sfc(scale,"Point",id_sfc),
		x(tool_nav,"X",'X'),
		key(tool_nav,"",id_key),
		rx(key,"Rx",3),ry(key,"Ry",4),rz(key,"Rz",5),
		sx(key,"Sx",6),sy(key,"Sy",7),sz(key,"Sz",8),
		tx(key,"Tx",0),ty(key,"Ty",1),tz(key,"Tz",2),
		y(tool_nav,"",'Y'), //"Y"
	zoom(nav,graphic::zoom_min,graphic::zoom_max),
	timeline(main,"",id_bar),
	status(main,"",bi::sunken), //2022
	scene(status,id_scene), //2022
	graphic(scene,false)
	{
		hide();

		active_callback = &AnimWin::submit;

		init(); //submit(id_init);
	}
	virtual ~AnimWin();
	
	int menu,_reshape[4];

	class MainWin &model;

	struct Impl; Impl *impl;
	static void _menubarfunc(int);

	row nav;
	dropdown animation;
	row frame_nav;	
	spinbox frame;	
	button play,loop;
	row anim_nav;
		spinbox fps;
		boolean wrap;
		spinbox frames;
		button del,close;
	row tool_nav;
		multiple select;
		multiple::item select_frame;
		multiple::item select_vertex;
		multiple::item select_complex;
		boolean move_snap;
		row scale;
	//	boolean scale_sfc;
	//	boolean scale_kar; //???
		dropdown scale_sfc;
		spinbox x;
		multiple key;
		multiple::item rx,ry,rz,sx,sy,sz,tx,ty,tz;
		spinbox y;
	Win::zoom_set zoom;
	bar timeline;
	panel status;
	canvas scene; //2022
		
	struct graphic : Win::graphic //2022
	{
		using Win::graphic::graphic; //C++11

		typedef GraphicWidget GW;

		//TODO: Expose/rethink these.
		using GW::m_operation;

		AnimWin &win(){ return *(AnimWin*)c.ui(); }

		virtual int sizeLabel(const char *l)
		{
			return c.str_span(l);
		}
		virtual void getLabel(int &x, int &y)
		{
			x = x-c._x_abs; y = c.y(y)-c._y_abs-1;
		}
		virtual void drawLabel(int x, int y, const char *l, int bold)
		{
			getLabel(x,y); y-=label_height-1;

			c.draw_str(x,y,l); if(bold=='b') c.draw_str(x+1,y,l);
		}

		virtual void zoomLevelChangedSignal()
		{
			win().zoom.value.set_float_val(getZoomLevel());
		}
		virtual void updateCoordinatesSignal()
		{
			win().updateCoordsDone(false);
		}
		virtual void updateSelectionDoneSignal()
		{
			win().updateSelectionDone();
		}
		virtual void updateCoordinatesDoneSignal()
		{
			win().updateCoordsDone(true);
		}

		virtual bool mousePressSignal(int bt)
		{
			if(!win().frame.enabled())
			win().submit(id_help); //F1?
			return Win::graphic::mousePressSignal(bt);
		}

	}graphic;

	virtual ScrollWidget *widget()
	{
		return &graphic;
	}

protected:

	void _toggle_button(int);

	void updateSelectionDone();
	void updateCoordsDone(bool done=true);
	void updateXY(bool=true);
	void shrinkSelect();

	double centerpoint[2];
};

#endif  // __ANIMWIN_H_INC__
