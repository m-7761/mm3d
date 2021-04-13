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

#include "command.h"

struct EdgeTurnCommand : Command
{
	EdgeTurnCommand():Command(1,GEOM_FACES_MENU){}
	
	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Command","Edge Turn"); 
	}

	//NOTE: Shift is to avoid proximity to Ctrl+D (duplicate).
	virtual const char *getKeymap(int){ return "Shift+Ctrl+F"; }

	virtual bool activated(int,Model*);
		
	bool canTurnEdge(Model *model, const Model::Triangle *tri1, const Model::Triangle *tri2,
	unsigned int &edge_v1, unsigned int &edge_v2, unsigned int &tri1_v, unsigned int &tri2_v);
	
	void getTriangleVertices(Model *model, const Model::Triangle *,
	unsigned int edge_v1, unsigned int edge_v2, unsigned int tri_v,
	int &a, int &b, int &c);
};

extern Command *edgeturncmd(){ return new EdgeTurnCommand; }

bool EdgeTurnCommand::activated(int arg, Model *model)
{
	int_list triList;
	model->getSelectedTriangles(triList);

	// Only turns one edge, deal with it //???
	unsigned int edge_v1,edge_v2, tri1_v,tri2_v;
	
	//TODO: Maybe model needs its own edgeTurn API and 
	//corresponding undo object?
	struct st
	{
		const Model::Triangle *t1,*t2; 
		
		int s1,s2,d1,d2; unsigned v1,v2;
	};
	std::vector<st> vst;

	auto &vl = model->getVertexList();
	auto &tl = model->getTriangleList();

	for(int i:triList) 
	{	
		tl[i]->m_user = i;
		tl[i]->m_userMarked = false;
	}
	for(int i:triList) 
	{	
		auto *tri = tl[i];
		auto &cmp1 = tri->m_vertexIndices;

		//2020: I rewrote this to take advantage of new connectivity
		//data as an exercise. It took more time than I had expected
		if(!tri->m_userMarked)
		for(auto j:cmp1) for(auto&ea:vl[j]->m_faces)
		if(ea.first->m_selected&&ea.first!=tri&&!ea.first->m_userMarked)
		if(canTurnEdge(model,tri,ea.first,edge_v1,edge_v2,tri1_v,tri2_v))
		{		
			auto *tri2 = ea.first;
			auto &cmp2 = tri2->m_vertexIndices;

			tri->m_userMarked = tri2->m_userMarked = true; //blacklist

			//log_debug("turning edge on triangles %d and %d\n",*it1,*it2);
			//log_debug("vertices %d,%d,%d %d,%d,%d\n",edge_v1,edge_v2,tri1_v,edge_v1,edge_v2,tri2_v);

			int t1a=0,t1b=0,t1c=0,t2a=0,t2b=0,t2c=0;
			getTriangleVertices(model,tri,edge_v1,edge_v2,tri1_v,t1a,t1b,t1c);
			getTriangleVertices(model,tri2,edge_v1,edge_v2,tri2_v,t2a,t2b,t2c);

			//log_debug("indices %d,%d,%d %d,%d,%d\n",t1a,t1b,t1c,t2a,t2b,t2c);

			// For triangle 1:
			unsigned verts1[3] = {tri1_v,tri2_v,edge_v2};
			unsigned verts2[3] = {tri1_v,tri2_v,edge_v1};

			//2020: Do texture coords and only change 1 vertex each?
			//(There's probably a smarter way to do this but I don't
			//want to give it much thought or disturb existing code)			
			int draw1,diff1 = -1;
			int draw2,diff2 = -1;
			for(int i=3;i-->0;)
			{
				bool f1 = false;
				bool f2 = false;
				for(int j=3;j-->0;)
				{
					if(cmp1[j]==verts1[i])
					f1 = true;
					if(cmp2[j]==verts2[i])
					f2 = true;
				}
				if(!f1&&diff1==-1)
				{
					int x = verts1[i];
					for(int i=3;i-->0;)
					{
						f1 = false;
						for(int j=3;j-->0;)
						if(cmp1[i]==verts1[j])
						f1 = true;
						if(!f1)
						{
							for(int j=3;j-->0;)
							if(cmp2[j]==x)
							draw2 = j;
							diff1 = i;
							memcpy(verts1,cmp1,sizeof(cmp1));
							verts1[i] = x;
							i = 0;
						}
					}
				}
				if(!f2&&diff2==-1) //SUBROUTINE?: DUPLICATES ABOVE LOGIC
				{
					int x = verts2[i];
					for(int i=3;i-->0;)
					{
						f2 = false;
						for(int j=3;j-->0;)
						if(cmp2[i]==verts2[j])
						f2 = true;
						if(!f2)
						{
							for(int j=3;j-->0;)
							if(cmp1[j]==x)
							draw1 = j;
							diff2 = i;
							memcpy(verts2,cmp2,sizeof(cmp2));
							verts2[i] = x;
							i = 0;
						}
					}
				}
			}
			/*2020: The changes are defered now for better or worse. Maybe model
			//needs its own edge-turn API and corresponding undo object?
			float s1,s2,t1,t2;
			model->getTextureCoords(*it1,draw1,s2,t2);
			model->getTextureCoords(*it2,draw2,s1,t1);
			model->setTextureCoords(*it1,diff1,s1,t1);
			model->setTextureCoords(*it2,diff2,s2,t2);
			model->setTriangleVertices(*it1,verts1[0],verts1[1],verts1[2]);
			model->setTriangleVertices(*it2,verts2[0],verts2[1],verts2[2]);*/
			st e = {tri,tri2,draw1,draw2,diff1,diff2,verts1[diff1],verts2[diff2]};
			vst.push_back(e);
			goto break_break;
		}
		break_break:; //C++
	}
	for(auto&ea:vst) //first pass (undo)
	{
		ea.t1->m_userMarked = ea.t2->m_userMarked = false;

		int v1[3],v2[3]; for(int i=3;i-->0;)
		{
			v1[i] = ea.t1->m_vertexIndices[i]; 
			v2[i] = ea.t2->m_vertexIndices[i];
		}
		v1[ea.d1] = ea.v1; v2[ea.d2] = ea.v2;
		model->setTriangleVertices(ea.t1->m_user,v1[0],v1[1],v1[2]);
		model->setTriangleVertices(ea.t2->m_user,v2[0],v2[1],v2[2]);
	}
	for(auto&ea:vst) //second pass (undo)
	{
		float s1 = ea.t2->m_s[ea.s2];
		float t1 = ea.t2->m_t[ea.s2];
		float s2 = ea.t1->m_s[ea.s1];
		float t2 = ea.t1->m_t[ea.s1];		
		model->setTextureCoords(ea.t1->m_user,ea.d1,s1,t1);
		model->setTextureCoords(ea.t2->m_user,ea.d2,s2,t2);
	}
	if(vst.empty())
	model_status(model,StatusError,STATUSTIME_LONG,TRANSLATE("Command","You must have at least 2 adjacent faces to Edge Turn"));
	else
	model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Edge Turn complete"));
	return !vst.empty();
}

// edge_v1 and edge_v2 are the model vertices of the common edge
// tri1_v and tri2_v are the opposite vertices of the respective triangles
bool EdgeTurnCommand::canTurnEdge(Model *model, 
const Model::Triangle *tri1, const Model::Triangle *tri2,
unsigned int &edge_v1, unsigned int &edge_v2, unsigned int &tri1_v, unsigned int &tri2_v)
{
	auto *verts1 = tri1->m_vertexIndices;
	auto *verts2 = tri2->m_vertexIndices;

	const unsigned int invalid = (unsigned)~0;

	edge_v1 = invalid;
	edge_v2 = invalid;
	tri1_v  = invalid;
	tri2_v  = invalid;

	for(int i1 = 0; i1<3; i1++)
	{
		unsigned int v1 = verts1[i1];
		if(v1!=edge_v1&&v1!=edge_v2)
		for(int i2 = 0; i2<3; i2++)
		{
			unsigned int v2 = verts2[i2];
			if(v2!=edge_v1&&v2!=edge_v2)			
			if(v1==v2)
			{
				if(edge_v1==invalid)
					edge_v1 = v1;
				else if(edge_v2==invalid)
					edge_v2 = v1;
			}
		}
	}

	if(edge_v1!=invalid&&edge_v2!=invalid)
	{
		int i;
		for(i = 0; i<3; i++)		
		if(verts1[i]!=edge_v1&&verts1[i]!=edge_v2)
		{
			tri1_v = verts1[i];
			break;
		}
		for(i = 0; i<3; i++)
		if(verts2[i]!=edge_v1&&verts2[i]!=edge_v2)
		{
			tri2_v = verts2[i];
			break;
		}
		return true;
	}
	else return false;
}

// Triangle index a is edge_v1
// Triangle index b is edge_v2
// Triangle index c is tri_v,the third triangle vertex
//
// Note that a,b,and c are the indices into the list of three triangle vertices,
// not the index of the model vertices
void EdgeTurnCommand::getTriangleVertices(Model *model,
const Model::Triangle *tri,
unsigned int edge_v1, unsigned int edge_v2, unsigned int tri_v,
int &a, int &b, int &c)
{
	auto &v = tri->m_vertexIndices;

	for(int i = 0; i<3; i++)
	{
		if(v[i]==edge_v1)
			a = i;
		else if(v[i]==edge_v2)
			b = i;
		else if(v[i]==tri_v)
			c = i;
	}
}
