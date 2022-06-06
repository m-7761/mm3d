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

struct InvertNormalCommand : Command
{
	InvertNormalCommand():Command(2,GEOM_NORMALS_MENU){}

	virtual const char *getName(int arg)
	{
		switch(arg)
		{		
		case 0: return TRANSLATE_NOOP("Command","Make Faces Behind Faces"); 
		default: assert(0);
		case 1: return TRANSLATE_NOOP("Command","Invert Normals"); 
		}
	}

	virtual const char *getKeymap(int arg)
	{
		assert((unsigned)arg<2); return !arg?"Shift+N":"N"; 
	}

	virtual bool activated(int,Model*);
};

extern Command *invnormalcmd(){ return new InvertNormalCommand; }

bool InvertNormalCommand::activated(int arg, Model *model)
{
	int_list l;
	model->getSelectedTriangles(l);
	if(!arg) for(auto i:l) 
	{
		unsigned int v[3];
		model->getTriangleVertices(i,v[0],v[1],v[2]);
		int ii = model->addTriangle(v[2],v[1],v[0]);

		float st[2][3];
		model->getTextureCoords(i,st);
		{
			std::swap(st[0][0],st[0][2]);
			std::swap(st[1][0],st[1][2]);
		}
		model->setTextureCoords(ii,st);

		int g = model->getTriangleGroup(i);
		if(g!=-1) model->addTriangleToGroup(g,ii);
	}
	else model->invertNormals(l);

	model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Normals inverted"));

	return true;
}