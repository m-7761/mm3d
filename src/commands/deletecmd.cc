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

#include "model.h"
#include "msg.h"
#include "modelstatus.h"

#include "command.h"

struct DeleteCommand : Command
{	
	DeleteCommand():Command(1,
	TRANSLATE_NOOP("Command","Edit")){}

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Command","Delete"); 
	}

	//NOTE: This locates the delete function on the lefthand
	//side of the keyboard, but special care is taken so Del
	//works also.
	//NOTE: Duplicate is Ctrl+D. These two may be grouped in
	//the Geometry menu. Other D functions are Subdivide and
	//Edge Divide.
	virtual const char *getKeymap(int){ return "Ctrl+X"; }	

	virtual bool activated(int, Model *model);
};

extern Command *deletecmd(){ return new DeleteCommand; }

bool DeleteCommand::activated(int arg,Model *model)
{
	/*2021: Is this nececessary? Could print a message inside deleteSelected?
	//if(!model->inAnimationMode())
	//if(model->getAnimationCount(Model::ANIMMODE_SKELETAL)
	if(model->getAnimationCount()!=model->getAnimCount(Model::ANIMMODE_FRAME)
	 &&model->getSelectedBoneJointCount())
	{
		if('Y'!=msg_warning_prompt
		//(TRANSLATE("Command","Deleting joints may destroy skeletal animations\nDo you wish to continue?"),"yN"))
		(TRANSLATE("Command","Deleting joints may affect skeletal animations\nDo you wish to continue?"),"yN"))
		return false;
	}*/

	//model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Primitives deleted"));
	model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Deleted"));
	model->deleteSelected();
	return true;
}

