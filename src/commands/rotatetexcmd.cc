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
#include "command.h"

struct RotateTextureCommand : Command
{
	//GEOM_FACES_MENU/GEOM_GROUP_MENU
	RotateTextureCommand(const char *menu):Command(1,menu){}

	virtual const char *getName(int)
	{
		if(getPath()==GEOM_FACES_MENU)
		//TODO? Is this the best language?
		//return "Faces\nRotate Texture Coordinates"; 
		return "Rotate Texture Coordinates"; 
		//2022: Conflaing this language is misleading.
		//return "Groups\nRotate Texture Coordinates"; 
		return "Groups\nTurn Texture Coordinates CCW"; //UNUSED

		(void)TRANSLATE_NOOP("Command","Rotate Texture Coordinates");
	}

	virtual const char *getKeymap(int)
	{
		//REMINDER: Misfit assigns Unhide to Shift+U. This is
		//the current default also.
		//NOTE: I don't know if U is useful. Theoretically it
		//should be as convenient as flipping normals (N) but
		//I don't know how often UVs are legitimately off, if
		//ever, aside from UV editing.
		//NOTE: USING Ctrl+Shift+U SO THE MENUS ARE SAME SIZE.
		return getPath()==GEOM_FACES_MENU?"U":"Shift+Ctrl+U"; 
	}

	virtual bool activated(int, Model *model);
};

extern Command *rotatetexcmd(const char *menu)
{
	return new RotateTextureCommand(menu); 
}
  
bool RotateTextureCommand::activated(int arg, Model *model)
{
	int_list tris; model->getSelectedTriangles(tris);
	if(tris.empty())
	{
		model_status(model,StatusError,STATUSTIME_LONG,TRANSLATE("Command","Must select faces"));
		return false;
	}

	if(getPath()==GEOM_FACES_MENU)
	{
		for(int i:tris) //2->0, 1->2, 0->1
		{
			float st[2][3];
			model->getTextureCoords(i,0,st[0][1],st[1][1]);
			model->getTextureCoords(i,1,st[0][2],st[1][2]);
			model->getTextureCoords(i,2,st[0][0],st[1][0]);
			model->setTextureCoords(i,st);
		}
	}
	else if(getPath()==GEOM_GROUP_MENU) //UNUSED
	{
		//REMOVE ME?
		//TextureWidget::rotateCoordinatesCcw
		//provides the same functionality, or
		//in reverse with the other button. I
		//assume this was added for importing
		//some models that require fixing up?

		for(int i:tris)
		{
			float st[2][3]; for(int j=3;j-->0;)
			{
				float &s = st[0][j], &t = st[1][j];

				model->getTextureCoords(i,j,s,t);
				float swap = s;
				//TextureWidget::rotateCoordinatesCcw
				//uses 1-t and I think it's clear the
				//math is equivalent. It's good to be
				//clear these are identical functions.
				//s = 0.5f-(t-0.5f);
				s = 1-t; t = swap;
			}
			model->setTextureCoords(i,st);
		}	
	}
	else
	{
		assert(0); return false;
	}
	model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Texture coordinates rotated"));
	return true;
}

