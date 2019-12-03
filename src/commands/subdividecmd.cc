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

struct SubdivideCommand : Command
{
	SubdivideCommand():Command(1,GEOM_FACES_MENU){}

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Command","Subdivide Faces");
	}

	virtual const char *getKeymap(int){ return "D"; }

	virtual bool activated(int,Model*);
};

extern Command *subdividecmd(){ return new SubdivideCommand; }

bool SubdivideCommand::activated(int arg,Model *model)
{
	if(model->getSelectedTriangleCount()>=1)
	{
		model->subdivideSelectedTriangles();
		model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Subdivide complete"));
		return true;
	}
	model_status(model,StatusError,STATUSTIME_LONG,TRANSLATE("Command","You must have at least 1 face selected to Subdivide Faces"));
	return false;
}