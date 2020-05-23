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

#include "model.h"
#include "log.h"
#include "msg.h"
#include "modelstatus.h"
#include "command.h"

struct DuplicateCommand : Command
{
	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Command","Duplicate"); 
	}

	//NOTE: Delete is Ctrl+Shift+D. Ctrl+F is avoided to 
	//not accidentally hit D. Ctrl+S will save the model.
	virtual const char *getKeymap(int){ return "Ctrl+D"; }

	virtual bool activated(int, Model *model);
};

extern Command *dupcmd(){ return new DuplicateCommand; }

bool DuplicateCommand::activated(int arg, Model *model)
{
	if(!model->duplicateSelected())
	{
		model_status(model,StatusError,STATUSTIME_LONG,TRANSLATE("Command","You must have at least 1 face,joint,or point selected to Duplicate"));
		return false;
	}
	else
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Duplicate complete"));
		return true;
	}
}

