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

#include "model.h"
#include "log.h"

enum{ AT_Min=0, AT_Center, AT_Max }; //REMOVE ME

static void align(int dim, Model *m, int at, double val)
{
	double min[3],max[3];
	m->getSelectedBoundingRegion(min+0,min+1,min+2,max+0,max+1,max+2);

	//log_debug("selected bounding region is (%f,%f,%f)-(%f,%f,%f)\n",xmin,ymin,zmin,xmax,ymax,zmax);

	double delta; switch(at)
	{
	case AT_Center: delta = val-(max[dim]+min[dim])*0.5;
		break;
	case AT_Min: delta = val-min[dim]; break;
	case AT_Max: delta = val-max[dim]; break;
	default:  // Bzzt, thanks for playing
		log_error("bad align argument: %d\n",(int)at);
		return;
	}

	//log_debug("align difference is %f\n",delta);

	double coords[3];
	unsigned p,count = m->getVertexCount();
	for(p=0;p<count;p++)	
	if(m->isVertexSelected(p))
	{
		m->getVertexCoords(p,coords);
		coords[dim]+=delta;
		m->moveVertex(p,coords[0],coords[1],coords[2]);
	}	
	count = m->getBoneJointCount();
	for(p=0;p<count;p++)	
	if(m->isBoneJointSelected(p))
	{
		m->getBoneJointCoords(p,coords);
		coords[dim]+=delta;
		m->moveBoneJoint(p,coords[0],coords[1],coords[2]);
	}
	count = m->getPointCount();
	for(p=0;p<count;p++)	
	if(m->isPointSelected(p))
	{
		m->getPointCoords(p,coords);
		coords[dim]+=delta;
		m->movePoint(p,coords[0],coords[1],coords[2]);
	}
	
	count = m->getProjectionCount();
	for(p=0;p<count;p++)	
	if(m->isProjectionSelected(p))
	{
		m->getProjectionCoords(p,coords);
		coords[dim]+=delta;
		m->moveProjection(p,coords[0],coords[1],coords[2]);
	}	
}

extern void align_selected(int i, Model *m, int at, double val)
{
	switch(i) //REMOVE ME
	{
	case 0: case 'X': case 'x': return align(0,m,at,val);
	case 1: case 'Y': case 'y': return align(1,m,at,val);
	case 2: case 'Z': case 'z': return align(2,m,at,val);
	}
}