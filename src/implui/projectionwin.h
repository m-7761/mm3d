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


#ifndef __PROJECTIONWIN_H__
#define __PROJECTIONWIN_H__

#include "mm3dtypes.h" //PCH
#include "win.h"
#include "model.h"
#include "texwidget.h"

struct ProjectionWin : Win
{
	void init(),submit(int);

	ProjectionWin(class MainWin &model)
		:
	Win("Projections",&texture),
	model(model),
	nav1(main),
	projection(nav1,"",id_item),
	name(nav1,"Rename",id_name), 	
	nav2(main),
	add(nav2,"Add Faces",id_append),
	remove(nav2,"Remove Faces",id_remove),
	zoom(nav2,texture::zoom_min,texture::zoom_max),
	scene(main,id_scene),	
	nav3(main),
	type(nav3,"",id_subitem), //"Type:"	
	reset(nav3,"Reset UV Range",id_reset),
	apply(nav3,"Apply Projection",id_apply),
	ok(main),
	texture(scene)
	{
		//2022: I guess this curtails scenarios
		//where the OK button is expected to do
		//something but only closes out instead.
		ok.ok.name(::tr("Close")).id(id_close);

		nav1.expand();
		projection.expand();
		name.ralign();
		nav2.expand();
		nav3.expand();
		apply.ralign();

		active_callback = &ProjectionWin::submit;

		init(),submit(id_init);
	}

	class MainWin &model;

	int _reshape[2];
	
	row nav1;
	dropdown projection; button name;
	row nav2;
	button add,remove;	
	Win::zoom_set zoom;
	canvas scene;
	row nav3;	
	dropdown type;	
	button reset,apply;
	f1_ok_panel ok;

	struct texture : Win::texture
	{
		using Win::texture::texture; //C++11

		ProjectionWin &win()
		{
			return *(ProjectionWin*)c.ui();
		}		
		virtual void zoomLevelChangedSignal()
		{
			win().zoom.value.set_float_val(getZoomLevel());
		}
		virtual void updateRangeSignal()
		{
			win().rangeChanged(false);
		}
		virtual void updateRangeDoneSignal()
		{
			win().rangeChanged(true);
		}
		virtual void updateSeamSignal(double x, double y)
		{
			win().seamChanged(x,y,false);
		}
		virtual void updateSeamDoneSignal(double x, double y)
		{
			win().seamChanged(x,y,true);
		}

	}texture;

	virtual ScrollWidget *widget() //setInteractive?
	{
		return &texture; 
	}

	void open(); //???

	void setModel();
	void modelChanged(int changeBits);
	
protected:

	void openModel();

	void refreshProjectionDisplay();
	void addProjectionTriangles();

	void rangeChanged(bool done);
	void seamChanged(double xDiff, double yDiff, bool done);
};

#endif //__PROJECTIONWIN_H__
