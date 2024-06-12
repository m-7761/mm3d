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
#include "viewwin.h"
#include "model.h"
#include "modelstatus.h"

struct ViewportSettings : Win
{	
	void submit(int);

	ViewportSettings(MainWin &model)
		:
	//Win("Viewport Settings"),model(model),
	Win("Grid Settings"),model(model),
	vu(model->getViewportUnits()),
	ortho(main,vu),persp(main,vu),
	uvmap(main,vu),f1_ok_cancel(main),
	x_close(main,"",id_close)
	{
		x_close.set_hidden(); //win_close?
		f1_ok_cancel.ok_cancel.cancel.id(id_no);

		active_callback = &ViewportSettings::submit;

		submit(id_init);
	}

	MainWin &model;

	Model::ViewportUnits &vu, defaults;

	struct ortho_group
	{	
		ortho_group(node *main, Model::ViewportUnits &vu)
			:
		nav(main,"2D View"),
		unit(nav,"Grid Unit",&vu.inc),
		mult(nav,"",&vu.grid)
		{
			mult.add_item("Binary Zoom");
			mult.add_item("Decimal Zoom");
			mult.add_item("Fixed Zoom");
		}

		panel nav;
		spinbox unit; multiple mult;
	};
	struct persp_group
	{
		persp_group(node *main, Model::ViewportUnits &vu)
			:
		nav(main,"3D View"),
		unit(nav,"Grid Unit",&vu.inc3d),
		lines(nav,"Grid Lines",&vu.lines3d),
		points(nav,"Point Size",&vu.ptsz3d),
		xy(nav,"X/Y Plane",'xyz'),
		xz(nav,"X/Z Plane",'xyz'),
		yz(nav,"Y/Z Plane",'xyz'),
		col(nav),
		nav2(nav,""),
		fov(nav2,"FOV Angle",&vu.fov),
		cam(nav2,"Unzoomed:"),
		znear(nav2,"Near Plane",&vu.znear),
		zfar(nav2,"Far Plane",&vu.zfar),
		reset(nav2,"Default Settings",id_reset)
		{
			nav.ralign_all(); col.align(); 

			reset.expand();
		}

		panel nav;
		spinbox unit,lines;
		spinbox points;
		boolean xy,xz,yz;
		column col;
		panel nav2;
		spinbox fov;
		titlebar cam;
		spinbox znear,zfar;
		button reset;
	};
	struct uvmap_group
	{
		uvmap_group(node *main, Model::ViewportUnits &vu)
			:
		nav(main,"UV View"),
		zin(nav,"Zooming In:"),
		units(nav,"Maximum Units",'2'), //&vu.unitsUv
		zout(nav,"Zooming Out:"),
		u(nav,"U Snap",&vu.snapUv[0]),
		v(nav,"V Snap",&vu.snapUv[1])
		{
			//units.edit(1,2,8);

			//u.edit(0.0,0.0,1.0);
			//v.edit(0.0,0.0,1.0);
		}

		panel nav;
		titlebar zin;
		spinbox units;
		titlebar zout;
		spinbox u,v;
	};
	ortho_group ortho;
	persp_group persp;
	uvmap_group uvmap;
	f1_ok_cancel_panel f1_ok_cancel;	
	button x_close;
};
void ViewportSettings::submit(int i)
{
	switch(i)
	{
	case id_reset:
		
		vu.fov = 45;
		vu.znear = 0.1;
		vu.zfar = 1000;
		
		goto def; //break;

	case id_init: defaults = vu; def:

		ortho.unit.edit(0.00001,vu.inc,100000.0);
		ortho.mult.select_id(vu.grid);
		persp.unit.edit(0.00001,vu.inc3d,100000.0);
		persp.lines.edit(1,vu.lines3d,1000);
		persp.points.edit(0.01,vu.ptsz3d,1.0);
		persp.xy.set(vu.xyz3d&4);
		persp.xz.set(vu.xyz3d&2);
		persp.yz.set(vu.xyz3d&1);
		persp.fov.edit(25.0,vu.fov,65.0); //45
		persp.znear.spinner.set_speed(0.01); 
		persp.znear.edit(0.001,vu.znear,100.0); //0.001 fits
		persp.zfar.edit(100.0,vu.zfar,100000.0);
		uvmap.units.edit(1,vu.unitsUv?vu.unitsUv:2,8); 
		uvmap.u.edit(0.0,vu.snapUv[0],1.0);
		uvmap.v.edit(0.0,vu.snapUv[1],1.0);
		uvmap.u.sspace<left>({uvmap.v});
		//persp.lines.sspace<left>({persp.points});
		persp.points.compact(60);
		{
			int c = persp.points.box_span(0,false);
			persp.lines.compact(c);
			persp.unit.compact(c);		
			ortho.unit.compact(c);
		}
		break;

	case '2': //uvmap.units

		switch(i=uvmap.units) //Power-of-two?
		{
		case 0: case 1: 
		case 2: case 4: case 8: return;
		case 3: i = 4; break;
		default: i = 8; break;
		}
		uvmap.units.set_int_val(i);
		vu.unitsUv = i;
		model_status(model,StatusError,STATUSTIME_LONG,
		"Maximum Units rounded up to nearest power-of-two (%d)",i);
		break;

	case 'xyz':

		vu.xyz3d = 0;
		if(persp.xy) vu.xyz3d|=4;
		if(persp.xz) vu.xyz3d|=2;
		if(persp.yz) vu.xyz3d|=1;
		break;

	case id_ok:

		config->set("ui_grid_inc",(double)ortho.unit);
		config->set("ui_grid_mode",(int)ortho.mult);
		config->set("ui_3dgrid_inc",(float)persp.unit);
		config->set("ui_3dgrid_count",(int)persp.lines);
		config->set("ui_point_size",(double)persp.points);
		config->set("ui_3dgrid_xy",(bool)persp.xy);
		config->set("ui_3dgrid_xz",(bool)persp.xz);
		config->set("ui_3dgrid_yz",(bool)persp.yz);
		config->set("ui_3d_cam_fov",(double)persp.fov);
		config->set("ui_3d_cam_znear",(double)persp.znear);
		config->set("ui_3d_cam_zfar",(double)persp.zfar);
		config->set("uv_grid_subpixels",(int)uvmap.units);
		config->set("uv_grid_default_u",(double)uvmap.u);
		config->set("uv_grid_default_v",(double)uvmap.v);
	
		model.close_viewport_window();
		break;

	case id_close: //x_close

		extern bool viewwin_confirm_close(int,bool);
		if(!viewwin_confirm_close(glut_window_id(),false))
		return;
		//break;

	case id_cancel: case id_no:

		vu = defaults;

		model.close_viewport_window();
		break;

	case id_f1: return basic_submit(i);
	}
	model->updateObservers();
}
extern void viewportsettings(MainWin &model)
{
	//ViewportSettings(model).return_on_close(); 
	if(!model._vpsettings_win)
	model._vpsettings_win = new ViewportSettings(model);
	else 
	model.close_viewport_window(); //toggle?
}

