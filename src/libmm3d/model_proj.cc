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
#include "texture.h"
#include "texmgr.h"

#ifdef MM3D_EDIT
#include "modelundo.h"
#include "modelstatus.h"
#endif // MM3D_EDIT

int Model::getProjectionType(unsigned proj)const
{
	if(proj<m_projections.size())
	return m_projections[proj]->m_type;	
	return -1;
}

bool Model::setProjectionType(unsigned proj, int type)
{
	if(proj>=m_projections.size()) return false;
	
	if(type!=m_projections[proj]->m_type) //2020
	{
		//2020: applyProjection sets MoveGeometry if
		//triangles belong to the projection. The UI
		//needs a notification.
		m_changeBits|=MoveOther; 

		Undo<MU_SetProjectionType>(this,proj,type,m_projections[proj]->m_type);
		
		m_projections[proj]->m_type = type;

		applyProjection(proj);
	}
	return true;
}

void Model::setTriangleProjection(unsigned triangleNum, int proj)
{
	if(proj<(int)m_projections.size()&&triangleNum<m_triangles.size())
	{
		m_changeBits |= MoveTexture; //2022

		// negative is valid (means "none")
		if(Undo<MU_SetTriangleProjection>undo=this)
		undo->setTriangleProjection(triangleNum,proj,m_triangles[triangleNum]->m_projection);
		
		m_triangles[triangleNum]->m_projection = proj;
	}
	else assert(0);
}
void Model::setTrianglesProjection(const int_list &l, int proj)
{
	if(l.empty()) return;

	if(proj>=(int)m_projections.size()){ assert(0); return; }

	Undo<MU_SetTriangleProjection> undo(this);

	m_changeBits |= MoveTexture; //2022

	auto &tl = m_triangles;
	int sz = (int)tl.size(); for(int i:l) if(i<sz)
	{
		if(undo) undo->setTriangleProjection(i,proj,tl[i]->m_projection);
		
		tl[i]->m_projection = proj;
	}
	else assert(0);
}

int Model::getTriangleProjection(unsigned triangleNum)const
{
	if(triangleNum<m_triangles.size())
	{
		return m_triangles[triangleNum]->m_projection;
	}
	return -1;
}

bool Model::getProjectionRange(unsigned proj,
		double &xmin, double &ymin, double &xmax, double &ymax)const
{
	if(proj<m_projections.size())
	{
		xmin = m_projections[proj]->m_range[0][0];
		ymin = m_projections[proj]->m_range[0][1];
		xmax = m_projections[proj]->m_range[1][0];
		ymax = m_projections[proj]->m_range[1][1];
		return true;
	}
	return false;
}

bool Model::setProjectionRange(unsigned proj,
		double xmin, double ymin, double xmax, double ymax)
{
	if(proj<m_projections.size())
	{
		m_changeBits|=MoveOther; //2022

		TextureProjection *ptr = m_projections[proj];

		Undo<MU_SetProjectionRange> undo(this,proj,ptr->m_range);

		if(undo) undo->setProjectionRange(xmin,ymin,xmax,ymax);

		ptr->m_range[0][0] = xmin;
		ptr->m_range[0][1] = ymin;
		ptr->m_range[1][0] = xmax;
		ptr->m_range[1][1] = ymax;

		applyProjection(proj); return true;
	}
	return false;
}

bool Model::moveProjection(unsigned p, double x, double y, double z)
{
	if(p<m_projections.size())
	{
		m_changeBits|=MoveOther; //2020

		double old[3];

		old[0] = m_projections[p]->m_abs[0];
		old[1] = m_projections[p]->m_abs[1];
		old[2] = m_projections[p]->m_abs[2];

		m_projections[p]->m_abs[0] = x;
		m_projections[p]->m_abs[1] = y;
		m_projections[p]->m_abs[2] = z;

		applyProjection(p);

		Undo<MU_MoveUnanimated> undo(this);
		if(undo) 
		undo->addPosition({PT_Projection,p},x,y,z,old[0],old[1],old[2]);

		return true;
	}
	return false;
}

struct model_proj_TriangleTexCoordsT 
{
	bool set;

	double vc[3];

	double uac; // cos of angle from up vector to vertex
	double sac; // cos of angle from seam vector to vertex
	double pac; // cos of angle from plane vector to vertex

	double u,v;
};
void Model::applyProjection(unsigned int pnum)
{	
	if(pnum>=m_projections.size()) return; //???

	validateAnim(); //2021

	//BOTTLENECK
	//I was testing this because it's super slow in debug builds.
	//I realized that counterintuitively generating individual
	//undo objects could be faster on subsequent frames because
	//the "combine" operation will avoid std::insert. I tried to
	//optimize sorted_list::find_sorted to little avail.
	m_changeBits |= MoveTexture; 
	invalidateBspTree();	
	Undo<MU_SetTextureCoords> undo(this);
		
	TextureProjection *proj = m_projections[pnum];	
	auto type = proj->m_type;

	Matrix m; 
	m.setRotation(proj->m_rot);	
	double *upNormal = m.getVector(1);
	double *seamNormal = m.getVector(2);
	double *planeNormal = m.getVector(0);
	double *orig = proj->m_abs;
	double up[3];
	double upMag[3];
	for(int i=0;i<3;i++)
	{
		//NOTE: This is intended to move the seam on the back.
		//planeNormal should be the cross-product of upNormal
		//and seamNormal. I tried a few different angles with
		//just negating it, which seems to be a cross-product.
		seamNormal[i] = -seamNormal[i];
		planeNormal[i] = -planeNormal[i];
		upMag[i] = proj->m_xyz[i];
		up[i] = upNormal[i]*upMag[i];		
	}		

	//log_debug("up	 = %f,%f,%f\n",up[0],	up[1],	up[2]);
	//log_debug("seam  = %f,%f,%f\n",seam[0], seam[1], seam[2]);
	//log_debug("plane = %f,%f,%f\n",planeNormal[0],planeNormal[1],planeNormal[2]);

	unsigned tcount = m_triangles.size();
	auto *tl = m_triangles.data();
	for(unsigned ti=0;ti<tcount;ti++)	
	if(pnum==tl[ti]->m_projection)
	{
		model_proj_TriangleTexCoordsT tc[3];
		double center[3] = {0,0,0};

		int setCount = 0;
		double setAverage = 0.0;

		for(int i = 0; i<3; i++)
		{
			tc[i].set = false;
			double vc[3],vcn[3];

			getVertexCoords(tl[ti]->m_vertexIndices[i],tc[i].vc);

			center[0] += tc[i].vc[0];
			center[1] += tc[i].vc[1];
			center[2] += tc[i].vc[2];

			vc[0] = tc[i].vc[0]-orig[0];
			vc[1] = tc[i].vc[1]-orig[1];
			vc[2] = tc[i].vc[2]-orig[2];
			
			// uac *mag(vc)= distance along up vec to intersection point
			// distance = vertical texture coord

			vcn[0] = vc[0];
			vcn[1] = vc[1];
			vcn[2] = vc[2];
			normalize3(vcn);
			tc[i].uac = dot3(vcn,upNormal);
			tc[i].sac = dot3(vcn,seamNormal); //TPT_Cylinder/Sphere only.
			tc[i].pac = dot3(vcn,planeNormal);
			
			switch(type)
			{
			case TPT_Cylinder: //applyCylinderProjection(proj);
			{		
			// In this projection scheme vertices near the base of the
			// cylinder have a V of 0. Near the top is a V of 1. Above
			// or below the cylinder result in<0 or>1 respectively
			//
			// Vertices near the seam vector have a U of 0 (or 1)and
			// vertices in the opposite direction from the seam vector
			// have a U of 0.5. Perpendicular to the seam vector
			// is 0.25 or 0.75 (see the "pac" calculations below for
			// details).

				double d = tc[i].uac *mag3(vc)/upMag[i];
				tc[i].v = (d/2.0)+0.5;

				// get intersection point so we can determine where we are
				// releative to the seam
				vcn[0] = tc[i].vc[0]-up[0] *d;
				vcn[1] = tc[i].vc[1]-up[1] *d;
				vcn[2] = tc[i].vc[2]-up[2] *d;

				break;
			}
			case TPT_Sphere: //applySphereProjection(proj)
			{
				double d = tc[i].uac *mag3(vc);
				tc[i].v = 1.0-(acos(tc[i].uac)/PI);

				// get intersection point so we can determine where we are
				// releative to the seam
				vcn[0] = tc[i].vc[0]-upNormal[0] *d;
				vcn[1] = tc[i].vc[1]-upNormal[1] *d;
				vcn[2] = tc[i].vc[2]-upNormal[2] *d;

				break;
			}
			case TPT_Plane: //applyPlaneProjection(proj);
			{
				double d;

				d = tc[i].uac *mag3(vc)/upMag[i];
				tc[i].v = (d/2.0)+0.5;

				d = tc[i].pac *mag3(vc)/upMag[i];
				tc[i].u = (d/2.0)+0.5;

				continue; //!
			}}

			vcn[0]-=orig[0];
			vcn[1]-=orig[1];
			vcn[2]-=orig[2];
			//if(fabs(mag3(vcn))>0.0001) //fabs?
			if(squared_mag3(vcn)>0.0001*0.0001)
			{
				normalize3(vcn);

				// The acos gives us an indicaton of how far off from
				// the seam vector we are,but not in which direction. 
				// We use the sign of the plane's pac for that.
				double sac = acos(dot3(vcn,seamNormal));
				sac = sac/(2 *PI);
				if(tc[i].pac<=0.0)
				sac = 1.0-sac;
				tc[i].u = sac;

				tc[i].set = true;
				setAverage += tc[i].u;
				setCount++;
			}
		}

		// See if we cross the seam.
		//
		// If pac is the same sign for each vertex it means we 
		// don't cross the seam. If any are different,compare
		// the vector to the center of the plane to the seam
		// vector (dot product greater than 0 means it crosses).

		if(type!=TPT_Plane)
		if(!(tc[0].pac>0.0&&tc[1].pac>0.0&&tc[2].pac>0.0)
		 &&!(tc[0].pac<0.0&&tc[1].pac<0.0&&tc[2].pac<0.0))
		{
			// finish average
			center[0] /= 3.0;
			center[1] /= 3.0;
			center[2] /= 3.0;

			// convert to unit vector for dot product
			center[0] = center[0]-orig[0];
			center[1] = center[1]-orig[1];
			center[2] = center[2]-orig[2];
			normalize3(center);

			if(dot3(seamNormal,center)>0.0)
			{
				setAverage = 0.0; // need to recalculate this

				// the "side" variable tracks which whether the wrap
				// occurs on the 0.0 side (-1)or the 1.0 side (+1)
				// We pick a side based on which vertex is farthest
				// from the edge (that vert will be in the normal range).
				int side = 0;
				double dist = 0.0;

				int n;
				for(n = 0; n<3; n++)
				{
					if(tc[n].set)
					{
						double d = 0.0;
						int	 s = 0;
						if(tc[n].u<0.5)
						{
							d = fabs(tc[n].u);
							s = -1;
						}
						else
						{
							d = fabs(1.0-tc[n].u);
							s =  1;
						}

						if(d>dist)
						{
							side = s;
							dist = d;
						}
					}
				}

				// do wrap
				//
				// if side is positive,anything closer to 0 than 1
				// needs to be increased by 1.0
				//
				// if side is negative,anything closer to 1 than 0
				// needs to be decreased by 1.0
				for(int n = 0; n<3; n++)
				{
					if(tc[n].set)
					{
						if(tc[n].u<0.5)
						{
							if(side>0)
							{
								tc[n].u += 1.0;
							}
						}
						else
						{
							if(side<0)
							{
								tc[n].u -= 1.0;
							}
						}

						setAverage += tc[n].u;
					}
				}
			}
		}

		if(type!=TPT_Plane)
		if(setCount<3)
		{
			setAverage = (setCount>0)
				? setAverage/(double)setCount 
				: 0.5;

			for(int i = 0; i<3; i++)
			{
				if(!tc[i].set)
				{
					tc[i].u = setAverage;
				}
			}
		}
		
		// Update range and set texture coords
		double xDiff = proj->m_range[1][0]-proj->m_range[0][0];
		double yDiff = proj->m_range[1][1]-proj->m_range[0][1];
		for(int i = 0; i<3; i++)
		{
			double u = tc[i].u*xDiff+proj->m_range[0][0];
			double v = tc[i].v*yDiff+proj->m_range[0][1];
			float ss = (float)u, tt = (float)v;
			//setTextureCoords(ti,i,ss,tt);
			{
				float &s = tl[ti]->m_s[i], &t = tl[ti]->m_t[i];
				if(undo) undo->addTextureCoords(ti,i,ss,tt,s,t);
				s = ss, t = tt;
			}
		}
	}
}

