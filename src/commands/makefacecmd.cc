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
#include "msg.h"
#include "modelstatus.h"
#include "cmdmgr.h"
#include "log.h"
#include "cmdmgr.h"
#include "tool.h"

struct MakeFaceCommand : Command
{
	MakeFaceCommand():Command(1,GEOM_FACES_MENU){} //GEOM_VERTICES_MENU

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Command","Make Face from Vertices");
	}

	virtual const char *getKeymap(int){ return "Enter"; } //EXPERIMENTAL

	virtual bool activated(int,Model*);
};

extern Command *makefacecmd(){ return new MakeFaceCommand; }

bool MakeFaceCommand::activated(int, Model *model)
{	
	//HACK: polytool.cc uses the view port to flip the face instead
	//of winding... I intend to add an option to control this later.
	auto view = CommandManager::getInstance()->getViewSurrogate(model);

	int vi = 3;
	auto &vl = model->getVertexList();
	double normal[3],sum[4] = {};
	std::pair<unsigned,int> g3[3]; 
	std::pair<unsigned,unsigned> v3[3];
	for(auto v=vl.size();v-->0;)
	{
		auto *vp = vl[v]; 
		
		if(vp->m_selected) if(vi-->0)
		{			 
			v3[vi].first = vp->getOrderOfSelection();			
			v3[vi].second = v;

			g3[vi].first = v;
			for(auto&ea:vp->m_faces)
			g3[vi].second = ea.first->m_group; //ARBITRARY
		}
		else break;	
	}
	if(view) //Use view port like polytool.cc?
	{
		sum[2] = 1;
		((Vector*)sum)->transform3(view->parent->getParentBestInverseMatrix());
	}
	if(vi==0)
	{		
		std::sort(v3,v3+3);

		unsigned verts[3] = {v3[0].second,v3[1].second,v3[2].second};

		if(view||v3[0].first==v3[1].first||v3[1].first==v3[2].first)
		{
			//2022: Try to standardize with capcmd.cc.		
			model->calculateFlatNormal(verts,normal);

			//NOTE: Dragging a mouse to get to the menu is prone
			//to messing up the ability to use the viewport here.
			double d = dot3(normal,sum);
			if(d==0) for(int v=3;v-->0;) //view is 0 or a bad fit?
			{			
				for(auto&ea:vl[verts[v]]->m_faces)
				{
					g3[vi].second = ea.first->m_group; //ARBITRARY

					model->calculateFlatNormal(ea.first->m_vertexIndices,normal);
					for(int i=3;i-->0;) sum[i]+=normal[i];
				}
				d = dot3(normal,sum);
			}		
			if(d<0)
			{
				std::swap(verts[1],verts[2]);
			}
		}
		else if(!Model::Vertex::getOrderOfSelectionCCW())
		{
			std::reverse(verts,verts+3);
		}
		int tri = model->addTriangle(verts[0],verts[1],verts[2]);

		//2022: Guess group w/ texcoords?
		int grp = -1; for(int i=3;i-->0;)
		{
			//HACK? To be less random, at least this way the
			//first/oldest selected vertex nominates a group.
			if(v3[0].second==g3[i].first) grp = g3[i].second;
		}
		if(grp==-1) for(int i=0;i<3;i++)
		{
			if((grp=g3[i].second)!=-1) break; //Any will do?
		}
		if(grp!=-1)
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
				break;
			}
			model->addTriangleToGroup(grp,tri); 
		}
		
		//2019: If winding is back-face it will be invisible, and may need to be flipped.
		//2022: Rely on neighbors/getOrderOfSelection for now?
		//model->selectTriangle(tri); 

		model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Face created"));
		return true;
	}	
	model_status(model,StatusError,STATUSTIME_LONG,TRANSLATE("Command","Must select exactly 3 vertices"));
	return false;
}

