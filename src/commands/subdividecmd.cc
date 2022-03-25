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

struct SubdivideCommand : Command
{
	SubdivideCommand():Command(1,GEOM_FACES_MENU){}

	virtual const char *getName(int)
	{
		//2022: I want to combine under one hotkey to use Ctrl+/.
		return TRANSLATE_NOOP("Command","Subdivide Faces or Edge");
	}

	//Make harder to do divide by accident not realizing it.
	//virtual const char *getKeymap(int){ return "D"; }
	virtual const char *getKeymap(int){ return "Ctrl+/"; }

	virtual bool activated(int,Model*);
};

extern Command *subdividecmd(){ return new SubdivideCommand; }

bool SubdivideCommand::activated(int, Model *model)
{
	if(model->subdivideSelectedTriangles())
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Subdivide complete"));
		return true;
	}
	
	auto &vl = model->getVertexList();
	auto &tl = model->getTriangleList();
	unsigned v1,v2,vN = 0;
	for(size_t v=vl.size();v-->0;) if(vl[v]->m_selected)
	{
		(vN?v1:v2) = v; if(vN++==2) break;
	}

	int split = 0;
	unsigned newVertex = ~0U; if(vN==2)
	{	
		// The triangle count will grow while we're iterating, but that's okay
		// because we know that the new triangles don't need to be split
		size_t tcount = tl.size();

		for(size_t tri=0;tri<tcount;tri++)
		{
			const int INVALID = ~0;
			int a = INVALID;
			int b = INVALID;
			int c = INVALID;

			auto *tp = tl[tri];

			unsigned tv[3]; for(int i=0;i<3;i++)
			{
				tv[i] = tp->m_vertexIndices[i];

				if(tv[i]==v1)
				a = i;
				else if(tv[i]==v2)
				b = i;
				else
				c = i;
			}

			// don't assume 'c' is set,triangle may have one vertex
			// assigned to two corners
			if(a!=INVALID&&b!=INVALID&&c!=INVALID)
			{
				if(split==0)
				{
					double coord1[3];
					double coord2[3];
					model->getVertexCoordsUnanimated(v1,coord1);
					model->getVertexCoordsUnanimated(v2,coord2);
					for(int i=3;i-->0;)
					{
						coord1[i] = (coord1[i]+coord2[i])*0.5;
					}

					newVertex = model->addVertex(v1,coord1);					
				}
															
				int vert[3];
				vert[a] = newVertex;
				vert[b] = tv[b];
				vert[c] = tv[c];
				int newTri = model->addTriangle(vert[0],vert[1],vert[2]);

				if(tp->m_group>=0)
				model->addTriangleToGroup(tp->m_group,newTri);

				vert[a] = tv[a];
				vert[b] = newVertex;
				vert[c] = tv[c];
				model->setTriangleVertices(tri,vert[0],vert[1],vert[2]);
								
				float st[2][3],dst[2][3]; 				
				model->getTextureCoords(tri,st); //2022									
				
				for(int i=2;i-->0;)
				{
					dst[i][a] = (st[i][a]+st[i][b])*0.5f;
					dst[i][b] = st[i][b];
					dst[i][c] = st[i][c];
				}
				model->setTextureCoords(newTri,dst);

				for(int i=2;i-->0;)
				{
					dst[i][a] = st[i][a];
					dst[i][b] = (st[i][a]+st[i][b])*0.5f;
					dst[i][c] = st[i][c];
				}
				model->setTextureCoords(tri,dst);

				split++;
			}
		}		
	}

	if(split>0)
	{
		model->unselectAllVertices(); model->selectVertex(newVertex);

		model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Edge Divide complete"));
		return true;
	}
	model_status(model,StatusError,STATUSTIME_LONG,
	TRANSLATE("Command","Select 1 or more faces or 2 adjacent vertices to divide"));
	return false;
}