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

#include "menuconf.h"

#include "model.h"
#include "log.h"
#include "modelstatus.h"
#include "cmdmgr.h"
#include "tool.h"

struct CapCommand : Command
{
	CapCommand():Command(1,GEOM_MESHES_MENU){}

	virtual const char *getName(int)
	{ 
		return TRANSLATE_NOOP( "Command","Cap Holes"); 
	}

	virtual const char *getKeymap(int){ return "Ctrl+Enter"; }

	double *m_view;
	virtual bool activated(int,Model*);

	typedef std::vector<Model::Triangle*> tri_list;
	void getConnected(Model *model, int vert, int_list &conList, tri_list &triList);
	int createMissingTriangle(Model *model, unsigned v, int_list &conList, tri_list &triList);
	Model::Triangle *findSingleton(Model *model, unsigned v1, unsigned v2, tri_list &triList);
};

extern Command *capcmd(){ return new CapCommand; }

bool CapCommand::activated(int arg, Model *model)
{
	assert(!m_view);

	double view[4];
	//HACK: polytool.cc uses the view port to flip the face instead
	//of winding... I intend to add an option to control this later.
	if(Tool*tool=CommandManager::getInstance()->getViewSurrogate(model))
	{
		m_view = view; //0,0,1
		view[0] = view[1] = 0; view[2] = 1;
		((Vector*)m_view)->transform3(tool->parent->getParentBestInverseMatrix());		
	}

	int added = 0;

	//2022: This is needed to use m_faces with setting up m_user.
	model->validateNormals(); 

	// Algorithm:
	//
	// Selected edges
	// For each selected vertex, check every vertex connected to it
	//
	//	For each connected edge that does not belong to two triangles,
	//	add a triangle using a third vertex from another triangle
	//	that uses this vertex and another selected vertex
	
	tri_list triList;  // triangles using target vertex
	int_list conList;  // vertices connected to target vertex
	int_list vertList;
	auto &tl = model->getTriangleList();
	model->getSelectedVertices(vertList);
	for(int v:vertList)
	{
		getConnected(model,v,conList,triList);

		// If I'm connected to more than two vertices, the
		// triangle and vertex count should match, otherwise
		// there is a gap/hole.

		auto vcount = conList.size();
		auto tcount = triList.size();  
		if(vcount>2&&vcount!=tcount)
		for(int tri;tcount!=vcount;)
		if((tri=createMissingTriangle(model,v,conList,triList))>=0)
		{
			added++; tcount++; 
					
			triList.push_back(const_cast<Model::Triangle*>(tl[tri]));
		}
		else break;
	}

	m_view = nullptr;

	if(added!=0)
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Cap Holes complete"));
		return true;
	}
	model_status(model,StatusError,STATUSTIME_LONG,TRANSLATE("Command","Could not find gap in selected region"));
	return false;
}

void CapCommand::getConnected(Model *model, int v, int_list &conList, tri_list &triList)
{
	conList.clear(); triList.clear();

	//2022: Optimizing with m_faces I'm uncertain if all of this 
	//infrastructure (containers/subroutines) are still required.
	for(auto&ea:model->getVertexList()[v]->m_faces)
	{
		//NOTE: I'm not sure only considering selected faces here
		//is correct or ideal. The algorithm depends entirely on
		//counting vertices and faces (I think I've seen it fail)
		//so it's necessary perhaps to collect all connections.

		auto *verts = ea.first->m_vertexIndices;

		for(int i=0;i<3;i++) if(v==(int)verts[i])
		{
			for(int j=0;j<3;j++) if(v!=verts[j])
			{
				auto e = conList.end();
				if(e==std::find(conList.begin(),e,(int)verts[j]))
				{	
					conList.push_back(verts[j]);
				}
			}
			triList.push_back(ea.first); break;
		}
	}
}

Model::Triangle *CapCommand::findSingleton
(Model *model, unsigned v1, unsigned v2, tri_list &triList)
{
	Model::Triangle *tri;
	int triCount = 0; for(auto*tp:triList)
	{
		auto*verts = tp->m_vertexIndices;
		bool have1 = false;
		bool have2 = false;
		for(int j=3;j-->0;)
		{
			if(verts[j]==v1) have1 = true;
			if(verts[j]==v2) have2 = true;
		}
		if(have1&&have2)
		{
			triCount++; tri = tp;
		}
	}
	//2022: This wasn't done prior, however I don't see any
	//way it would spoil the algorithm, and there's no other
	//way to keep the new created faces from being processed
	//and if that's the rule then the outer loop should then
	//startover from the beginning when faces are added to be
	//consistent, which would be prohibitive.		
	//return triCount==1?tri:nullptr;
	return triCount==1&&tri->m_selected?tri:nullptr;
}

int CapCommand::createMissingTriangle(Model *model, unsigned v, int_list &conList, tri_list &triList)
{
	auto &vl = model->getVertexList();
	auto &tl = model->getTriangleList();
	auto itt = conList.end();
	for(auto it=conList.begin();it<itt;it++)
	if(vl[*it]->m_selected)
	if(findSingleton(model,v,*it,triList))	
	for(auto jt=it+1;jt!=itt;jt++)		
	if(vl[*jt]->m_selected)
	if(auto*unused=findSingleton(model,v,*jt,triList))
	{
		unsigned verts[3] = { v,(unsigned)*it,(unsigned)*jt };

		//2022: Try to standardize with makefacecmd.cc.
		double normal[3],sum[3] = {}; 		
		model->calculateFlatNormal(verts,normal);

		double d; if(m_view) //dot3?
		{
			//NOTE: Dragging a mouse to get to the menu is prone
			//to messing up the ability to use the viewport here.
			d = dot3(normal,m_view); if(0==d) goto sum;
		}
		else sum:
		{
			for(int i=3;i-->0;)
			for(auto&ea:vl[verts[i]]->m_faces)
			if(ea.first->m_selected)
			{
				for(int j=3;j-->0;) sum[j]+=ea.first->m_flatSource[j];
			}
			d = dot3(normal,m_view?m_view:sum);
		}
		if(d<0) std::swap(verts[1],verts[2]);

		int tri = model->addTriangle(v,verts[1],verts[2]);

		//FIX ME: This seems too random perhaps, but at
		//least tri is selected.
		int grp = unused->m_group; if(grp>=0) //ARBITRARY
		{
			//NOTE: Could try to match to texture ID?
			auto *tp = model->getTriangleList()[tri];
			for(int i=3;i-->0;)
			for(auto&ea:vl[verts[i]]->m_faces)
			if(grp==ea.first->m_group)
			{
				//NOTE: CAN WRITE DIRECTLY INTO m_s/m_t SINCE 
				//THERE'S NO UNDO HISTORY ON THE NEW TRIANGLE.
				const_cast<float&>(tp->m_s[i]) = ea.first->m_s[ea.second];
				const_cast<float&>(tp->m_t[i]) = ea.first->m_t[ea.second];

				//Priority to selection.
				if(ea.first->m_selected) break;
			}
			model->addTriangleToGroup(grp,tri);
		}

		return tri;
	}	
	return -1;
}
