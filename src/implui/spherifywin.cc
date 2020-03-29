/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2004-2007 Kevin Worcester
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not,write to the Free Software
 * Foundation,Inc.,59 Temple Place-Suite 330,Boston,MA 02111-1307,
 * USA.
 *
 * See the COPYING file for full license text.
 */

#include "mm3dtypes.h" //PCH

#include "viewwin.h"

#include "glmath.h"

#include "log.h"

struct SpherifyWin : Win
{
	void submit(int);

	SpherifyWin(MainWin &model)
		:
	Win("Spherify"),model(model),
	radius(params.radius_init(model)),
	value(main,"Value",-radius,radius,0.0),
	f1_ok_cancel(main)
	{
		if(!radius) value.nav.disable(); //NEW
		
		active_callback = &SpherifyWin::submit;
		
		submit(id_init);
	}

	MainWin &model;
	
	struct SpherifyPosition
	{
		double coords[3]; Model::Position pos;
	};
	struct Parameters : std::vector<SpherifyPosition>
	{
		double centerpoint[3], radius_init(MainWin&);
	};

	//HACK: Constructor needs these in this order.
	Parameters params; double radius;	

	slider_value value;
	f1_ok_cancel_panel f1_ok_cancel;
};

extern void spherifywin(Model *model) //spherifycmd.cc
{
	SpherifyWin(MainWin::cast(model)).return_on_close();
}

double SpherifyWin::Parameters::radius_init(MainWin &model)
{
	//WORRIED THIS IS IMPRECISE
	double init = model.selection.empty()?0.0:DBL_MAX;
	double cmin[3] = {init}, cmax[3] = {-init};
	
	for(auto&i:model.selection)
	{
		SpherifyPosition sv; sv.pos = i;
		model->getPositionCoords(i,sv.coords);
		push_back(sv);
		for(int j=0;j<3;j++)
		cmin[j] = std::min(cmin[j],sv.coords[j]);
		for(int j=0;j<3;j++)
		cmax[j] = std::max(cmax[j],sv.coords[j]);
	}
	for(int i=0;i<3;i++)
	{
		centerpoint[i] = (cmin[i]+cmax[i])/2;
	}

	double radius = 0;
	{
		for(auto&ea:*this)
		{
			radius = std::max(radius,distance
			(centerpoint[0],centerpoint[1],centerpoint[2],
			ea.coords[0],ea.coords[1],ea.coords[2]));
		}
	 
		log_debug("center is %f,%f,%f\n",centerpoint[0],centerpoint[1],centerpoint[2]);
		log_debug("radius is %f\n",radius);
	}
	return radius;
}

void SpherifyWin::submit(int id)
{
	switch(id)
	{
	case id_ok:

		model->operationComplete(::tr("Spherify","operation complete"));		
		break;

	case id_cancel:

		model->undoCurrent();
		break;
	
	case id_value:

		//log_debug("changed\n"); //???

		//double percent = (double)v/100.0;
		double t = value.slider.float_val()/radius;

		//FIX ME: This produces different results owing to 
		//floating point error. It also generates needless
		//undo records.

		Vector unitvec;
		Parameters::iterator it,itt;	
		for(it=params.begin(),itt=params.end();it<itt;it++)
		{
			for(int i=0;i<3;i++)
			unitvec[i] = it->coords[i]-params.centerpoint[i];
			unitvec.normalize3();

			double diff[3]; for(int i=0;i<3;i++) //lerp??
			{
				diff[i] = unitvec[i]*radius+params.centerpoint[i];
				diff[i] = (diff[i]-it->coords[i])*t+it->coords[i];
			}
			model->movePosition(it->pos,diff[0],diff[1],diff[2]);
		}

		//https://github.com/zturtleman/mm3d/issues/90
		//DecalManager::getInstance()->modelUpdated(model); //???
		model->updateObservers();
		break;	
	}

	basic_submit(id);
}



