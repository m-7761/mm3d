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
#include "log.h"
#include "msg.h"
#include "modelstatus.h"
#include "command.h"

struct InvertSelectionCommand : Command
{
	InvertSelectionCommand():Command(2){}

	virtual const char *getName(int arg)
	{
		switch(arg)
		{
		default: assert(0);
		case 0: return TRANSLATE_NOOP("Command","Select All"); 
		case 1: return TRANSLATE_NOOP("Command","Invert Selection"); 
		}
	}

	virtual const char *getKeymap(int arg)
	{ 
		switch(arg)
		{
		default: assert(0);
		case 0: return "Ctrl+A";
		case 1: return "Shift+Ctrl+A"; 
		}
	}

	virtual bool activated(int,Model*);
};

extern Command *invertcmd(){ return new InvertSelectionCommand; }

bool InvertSelectionCommand::activated(int arg, Model *model)
{
	int n = 0; //OVERKILL

	//HACK: There isn't a Model::selectAll API
	if(!arg) switch(model->getSelectionMode()) //2020
	{
	case Model::SelectVertices:

		if(n=model->getSelectedVertexCount())
		model->unselectAllVertices(); break;

	default:
	case Model::SelectTriangles:
	case Model::SelectConnected:

		if(n=model->getSelectedTriangleCount())
		{
			model->unselectAllTriangles();
			model->unselectAllVertices(); //YUCK
		}
		model->unselectAllGroups(); break;

	case Model::SelectGroups:

		if(n=model->getSelectedGroupCount())
		{
			model->unselectAllGroups();
			model->unselectAllVertices(); //YUCK
		}
		model->unselectAllTriangles(); break;
	
	case Model::SelectJoints:

		if(n=model->getSelectedBoneJointCount())
		model->unselectAllBoneJoints(); break;

	case Model::SelectPoints:

		if(n=model->getSelectedPointCount())
		model->unselectAllPoints(); break;

	case Model::SelectProjections:

		if(n=model->getSelectedBoneJointCount())
		model->unselectAllProjections(); break;
	}

	if(!n) model->invertSelection();

	model_status(model,StatusNormal,STATUSTIME_SHORT,TRANSLATE("Command","Selection inverted"));

	return true;
}
