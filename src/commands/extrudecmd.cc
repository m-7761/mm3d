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

#include "extrudecmd.h" //REMOVE ME
#include "command.h"
#include "model.h"
#include "log.h"

struct ExtrudeCommand : Command
{
	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Command","Extrude...");
	}

	//This is a historical shortcut. The tool is using X.
	//virtual const char *getKeymap(int){ return "Insert"; }
	virtual const char *getKeymap(int){ return "Shift+X"; }

	virtual bool activated(int, Model *model)
	{
		extern void extrudewin(Model*);
		extrudewin(model); return true;
	}
};

extern Command *extrudecmd(){ return new ExtrudeCommand; }

bool ExtrudeImpl::extrude(Model *model, double x, double y, double z, bool make_back_faces)
{
	m_sides.clear(); m_evMap.clear();
	
	int_list faces;
	model->getSelectedTriangles(faces);
	int_list vertices;
	model->getSelectedVertices(vertices);
	
	if(faces.empty()) return false;

	// Find edges (sides with count==1)
	for(unsigned tri:faces)
	{
		unsigned v[3];
		model->getTriangleVertices(tri,v[0],v[1],v[2]);

		for(int t=0;t<(3-1);t++)
		_addSide(v[t],v[t+1]);
		_addSide(v[0],v[2]);
	}

	const unsigned begin = model->getVertexCount(); //2020

	// make extruded vertices and create a map from old vertices
	// to new vertices
	for(unsigned vrt:vertices)
	{
		double coord[3];
		model->getVertexCoords(vrt,coord);
		coord[0]+=x;
		coord[1]+=y;
		coord[2]+=z;
		m_evMap[vrt] = model->addVertex(vrt,coord);

		//log_debug("added vertex %d for %d at %f,%f,%f\n",
		//m_evMap[vrt],vrt,coord[0],coord[1],coord[2]);
	}

	// Add faces for edges
	for(unsigned tri:faces)
	{
		unsigned v[3];
		model->getTriangleVertices(tri,v[0],v[1],v[2]);

		int g = model->getTriangleGroup(tri); //2022

		for(int t=0;t<(3-1);t++)
		if(_sideIsEdge(v[t],v[t+1]))
		{
			_makeFaces(model,g,v[t],v[t+1]);
		}
		if(_sideIsEdge(v[2],v[0]))
		{
			_makeFaces(model,g,v[2],v[0]);
		}
	}

	// Map selected faces onto extruded vertices
	for(unsigned tri:faces)
	{
		unsigned v[3];
		model->getTriangleVertices(tri,v[0],v[1],v[2]);
				
		if(make_back_faces)
		{
			int newTri = model->addTriangle(v[0],v[1],v[2]);
			model->invertNormals(newTri);

			int g = model->getTriangleGroup(tri);
			if(g>=0) model->addTriangleToGroup(g,newTri); //2022
		}

		//log_debug("face %d uses vertices %d,%d,%d\n",tri,v1[0],v[1],v[2]);

		for(int i=3;i-->0;) v[i] = m_evMap[v[i]];

		model->setTriangleVertices(tri,v[0],v[1],v[2]);

		//log_debug("moved face %d to vertices %d,%d,%d\n",tri,v1[0],v[1],v[2]);
	}

	// Update face selection
	for(auto it=m_evMap.begin();it!=m_evMap.end();it++)
	{
		model->unselectVertex(it->first);
		model->selectVertex(it->second);
	}

	model->deleteOrphanedVertices(begin); //???
	
	return true;
}
void ExtrudeImpl::_makeFaces(Model *model, int g, unsigned a, unsigned b)
{
	unsigned a2 = m_evMap[a], b2 = m_evMap[b];
	int t1 = model->addTriangle(b,b2,a2);	
	int t2 = model->addTriangle(a2,a,b);
	if(g>=0) model->addTriangleToGroup(g,t1);
	if(g>=0) model->addTriangleToGroup(g,t2);
}
void ExtrudeImpl::_addSide(unsigned a, unsigned b)
{
	if(b<a) std::swap(a,b);

	// Find existing side (if any)and increment count
	for(auto&ea:m_sides) if(ea.a==a&&ea.b==b)
	{
		//log_debug("side (%d,%d)= %d\n",a,b,it->count);
		ea.count++; return;
	}

	// Not found,add new side with a count of 1
	//log_debug("side (%d,%d)= %d\n",a,b,s.count);
	m_sides.push_back({a,b,1});
}
bool ExtrudeImpl::_sideIsEdge(unsigned a, unsigned b)
{
	if(b<a) std::swap(a,b); for(auto&ea:m_sides)
	{
		if(ea.a==a&&ea.b==b) return 1==ea.count;
	}
	return false;
}