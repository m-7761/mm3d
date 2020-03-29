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

#include "menuconf.h"

#include "model.h"
#include "log.h"
#include "modelstatus.h"

#include "command.h"

struct CapCommand : Command
{
	CapCommand():Command(1,GEOM_MESHES_MENU){}

	virtual const char *getName(int)
	{ 
		return TRANSLATE_NOOP( "Command","Cap Holes"); 
	}

	virtual bool activated(int, Model *model);

	void getConnected(
			Model *model, int vert,
			int_list &conList,
			int_list &triList);
	void addToList(int_list &l, int ignore, int val);
	int createMissingTriangle(Model *model, unsigned int v,
			int_list &conList,int_list &triList);
	int triangleCount(Model *model,
			unsigned int v1, unsigned int v2,
			int_list &triList, int &tri);
};

extern Command *capcmd(){ return new CapCommand; }

bool CapCommand::activated(int arg, Model *model)
{
	int added = 0;

	// Algorithm:
	//
	// Selected edges
	// For each selected vertex,check every vertex connected to it
	//
	//	For each connected edge that does not belong to two triangles,
	//	add a triangle using a third vertex from another triangle
	//	that uses this vertex and another selected vertex

	int_list vertList;
	int_list triList;  // triangles using target vertex
	int_list conList;  // vertices connected to target vertex

	model->getSelectedVertices(vertList);

	int_list::iterator it;
	for(it = vertList.begin(); it!=vertList.end(); it++)
	{
		getConnected(model,*it,conList,triList);

		unsigned int vcount = conList.size();
		unsigned int tcount = triList.size();  
		if(vcount>2&&vcount!=tcount)
		{
			// If I'm connected to more than two vertices,the
			// triangle and vertex count should match,otherwise
			// there is a gap/hole.

			int newTri = 0;
			while(tcount!=vcount&&newTri>=0)
			{
				newTri = createMissingTriangle(model,*it,conList,triList);
				if(newTri>=0)
				{
					added++;
					tcount++;
					triList.push_back(newTri);
				}
			}
		}
	}

	if(added!=0)
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Cap Holes complete"));
		return true;
	}
	else
	{
		model_status(model,StatusError,STATUSTIME_LONG,TRANSLATE("Command","Could not find gap in selected region"));
	}
	return false;
}

void CapCommand::getConnected(Model *model, int v,
		int_list &conList,
		int_list &triList)
{
	conList.clear();
	triList.clear();

	unsigned int tcount = model->getTriangleCount();
	for(unsigned int tri = 0; tri<tcount; tri++)
	{
		unsigned int verts[3];
		model->getTriangleVertices(tri,verts[0],verts[1],verts[2]);

		for(int i = 0; i<3; i++)
		{
			if((int)verts[i]==v)
			{
				triList.push_back(tri);

				addToList(conList,v,verts[0]);
				addToList(conList,v,verts[1]);
				addToList(conList,v,verts[2]);

				break;
			}
		}
	}
}

void CapCommand::addToList(int_list &l, int ignore, int val)
{
	if(ignore==val)
	{
		return;
	}

	int_list::iterator it;
	for(it = l.begin(); it!=l.end(); it++)
	{
		if(*it==val)
		{
			return;
		}
	}
	l.push_back(val);
}


//float Model::cosToPoint(unsigned t, double *point)const
static float capcmd_cos2point(double *normal, double *vertex, double *compare)
{
	double vec[3];
	for(int i=3;i-->0;) vec[i] = compare[i]-vertex[i];

	// normalized vector from plane to point
	normalize3(vec);

	double f = dot3(normal,vec);
	
	//log_debug("  dot3(%f,%f,%f  %f,%f,%f)\n",normal[0],normal[1],normal[2],vec[0],vec[1],vec[2]); //???
	//log_debug("  behind triangle dot check is %f\n",f); //???
		
	return (float)f; //truncate?
}

int CapCommand::createMissingTriangle(Model *model, unsigned int v,
		int_list &conList,int_list &triList)
{
	int_list::iterator cit1,cit2;
	int tri = 0;

	for(cit1 = conList.begin(); cit1!=conList.end(); cit1++)
	if(model->isVertexSelected(*cit1)
	&&1==triangleCount(model,v,*cit1,triList,tri))
	{
		cit2 = cit1;
		cit2++;

		// Find third triangle vertex for normal test below
		int tri1Vert = 0; 
		unsigned int verts[3];
		model->getTriangleVertices(tri,verts[0],verts[1],verts[2]);

		for(int i = 0; i<3; i++)
		if(verts[i]!=v&&verts[i]!=(unsigned int)*cit1)
		{
			tri1Vert = verts[i];
		}

		for(; cit2!=conList.end(); cit2++)		
		if(model->isVertexSelected(*cit2)
		&&1==triangleCount(model,v,*cit2,triList,tri))
		{
			int newTri = model->addTriangle(v,*cit1,*cit2);

			double coord1[3],norm1[3];
			double coord2[3],norm2[3];

			model->getFlatNormal(tri,norm1);
			model->getFlatNormal(newTri,norm2);
			model->getVertexCoords(*cit1,coord2);
			model->getVertexCoords(tri1Vert,coord1);

			// this is why we needed the third vertex above,
			// so we can check to see if we need to invert
			// the new triangle (are the triangles behind
			// each other or in front of each other?)
			float f = capcmd_cos2point(norm1,coord1,coord2);
			if(fabs(f)<0.0001f)
			{
				// Points are nearly co-planar,
				// normals should face the same way
				if(dot3(norm1,norm2)<0.0f)
				{
					model->invertNormals(newTri);
				}
			}
			else
			{
				bool cmp = f>0;
				if(cmp!=capcmd_cos2point(norm2,coord2,coord1)>0)
				{
					model->invertNormals(newTri);
				}
			}

			return newTri;
		}
	}	
	return -1;
}

int CapCommand::triangleCount(Model *model,
unsigned int v1, unsigned int v2,int_list &triList, int &tri)
{
	int_list::iterator it;

	unsigned int verts[3];
	int triCount = 0;

	for(it = triList.begin(); it!=triList.end(); it++)
	{
		model->getTriangleVertices(*it,verts[0],verts[1],verts[2]);

		bool have1 = false;
		bool have2 = false;

		for(int i = 0; i<3; i++)
		{
			if(verts[i]==v1)
				have1 = true;
			else if(verts[i]==v2)
				have2 = true;
		}

		if(have1&&have2)
		{
			triCount++;
			tri = *it;
		}
	}
	return triCount;
}
