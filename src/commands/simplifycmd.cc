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

struct SimplifyMeshCommand : Command
{
	SimplifyMeshCommand():Command(2,GEOM_MESHES_MENU){}

	virtual const char *getName(int arg)
	{
		//Simplification seems to suggest removing details.
		//return TRANSLATE_NOOP("Command","Simplify Mesh"); 
		return TRANSLATE_NOOP("Command",arg?"Minimize Bare Mesh":"Minimize"); 
	}

	virtual const char *getKeymap(int arg){ return arg?"Shift+Ctrl+M":"Shift+M"; }	

	virtual bool activated(int, Model *model);
};

extern Command *simplifycmd(){ return new SimplifyMeshCommand; }

bool SimplifyMeshCommand::activated(int arg, Model *model)
{
	bool ret = model->simplifySelectedMesh(arg==1); 
	//2022: Input feedback helps to be sure keys are processed.
	model_status(model,StatusNormal,STATUSTIME_SHORT,
	TRANSLATE("Command",ret?"Redundant edges eliminated"
	:"0 redundancy (elminating 1 vertex must convert 2 edges into 1 straight edge)"));
	return ret;
}

