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


#ifndef __TEXTURECOORD_H__
#define __TEXTURECOORD_H__

#include "mm3dtypes.h" //PCH
#include "win.h"
#include "model.h"
#include "texwidget.h"

struct TextureCoordWin : Win
{
	void init(),submit(control*);
	
	~TextureCoordWin();
	TextureCoordWin(class MainWin &model)
		:
	Win("Texture Coordinates",&texture),
	model(model),
	viewbar(main),
	white(viewbar,"",id_white),
	red(viewbar,"",id_red),
	zoom(viewbar,texture::zoom_min,texture::zoom_max),
	scene(main,id_scene),
	toolbar3(main), //F3
	ccw(toolbar3,"Turn CCW"), //Rotate CCW
	cw(toolbar3,"Turn CW"), //Rotate CW
	uflip(toolbar3,"U Flip"), //H Flip
	vflip(toolbar3,"V Flip"),				
	toolbar2(main), //F2
	l80(toolbar2,"180 Turn"),
		//Snap is just shorter than "Flatten".	
	usnap(toolbar2,"U Snap"),
	vsnap(toolbar2,"V Snap"),
	snap(toolbar2,"Snap"),
		f1(main),  //F1
	mouse(main.inl,"Mouse Tool",id_item),	
	scale_sfc(main,"Scale from center"),
	scale_kar(main,"Keep aspect ratio"),	
		void1(main),
	pos(main,"Position",bi::none), //"Position"
	u(pos,"U",'U'),
	v(pos,"V",'V'),
	dimensions(pos,"Dimension"),
		void2(main),
	map(main,"Map Scheme",id_subitem),	
	map_keep(main,"Keep",id_reset), //EXPERIMENTAL
	map_group(main,"Group"),
	map_reset(main,"Reset Coordinates",id_subitem),
	map_remap(main,"Choose Projection",id_subitem+1), //PLACEHOLDER
		close(main,"Close",id_close),
	texture(scene),current_direction(-1)
	{
		map_group.set_hidden();
		map_keep.set_hidden();

		viewbar.expand();
		pos.expand();
		u.edit(0.0).expand(); 
		v.edit(0.0).expand();
		u.compact().sspace<left>({v.compact()},false);
		dimensions.expand().place(bottom);
		map_remap.expand();
		map_reset.expand();

		active_callback = &TextureCoordWin::submit;

		init(),submit(main);
	}

	class MainWin &model;

	int _menubar,_viewmenu;
	void _init_menu_toolbar();
	static void _menubarfunc(int);
	void _sync_tools(int,int);

	int _reshape[4];

	enum{ id_white=0xffffff, id_red=0xff0000 };

	row viewbar;	
	dropdown white,red;
	Win::zoom_set zoom;
	canvas scene;
	row toolbar3;
	button ccw,cw,uflip,vflip;
	row toolbar2;
	f1_titlebar f1;
	button l80,usnap,vsnap,snap; //2022
	multiple mouse;
	boolean scale_sfc,scale_kar;
	canvas void1;
	panel pos;
	spinbox u,v;
	textbox dimensions;
	canvas void2;
	multiple map;
	boolean map_keep; //2022
	boolean map_group; //2022
	button map_reset;
	button map_remap; //2022
	button close;

	struct texture : Win::texture
	{
		using Win::texture::texture; //C++11

		typedef TextureWidget TW;

		//TODO: Expose/rethink these.
		using TW::m_operation;
		using TW::m_vertices;
		using TW::m_triangles;	
		using TW::m_materialId;
		void move(double x, double y)
		{
			moveSelectedVertices
			(x/getUvWidth(),y/getUvHeight());
		}

		TextureCoordWin &win()
		{
			return *(TextureCoordWin*)c.ui();
		}		
		virtual void zoomLevelChangedSignal()
		{
			win().zoom.value.set_float_val(getZoomLevel());
		}
		virtual void updateCoordinatesSignal()
		{
			win().moveTextureCoordsDone(false);
		}
		virtual void updateSelectionDoneSignal()
		{
			win().updateSelectionDone();
		}
		virtual void updateCoordinatesDoneSignal()
		{
			win().moveTextureCoordsDone(true);
		}
		   
	}texture;

	virtual ScrollWidget *widget() //setInteractive?
	{
		return &texture; 
	}

	int current_direction; //2022

	void open(); //???
	
	void setModel();
	void modelChanged(int changeBits);

	bool select_all();
	bool select_faces(bool exclusive);

protected:		
		 
	void openModel();

	void mapReset(int id);
		
	void operationCompound(const char*);
	void updateSelectionDone();
	void moveTextureCoordsDone(bool done);
	void setTextureCoordsDone();

	void setTextureCoordsEtc(bool,bool); //NEW

	void toggle_toolbar(int);
	void toggle_indicators(bool);
	void toggle_tool(bool);

	bool m_ignoreChange;
	
	int_list trilist; //NEW: Reuse buffer.

	double centerpoint[2]; //UV textboxes.
	
	int recall_tool[2];
	int recall_lock[2];
};

#endif //__TEXTURECOORD_H__
