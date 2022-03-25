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
#include "modelstatus.h"
#include "command.h"

struct HideCommand : Command
{
	HideCommand():Command(4,
	TRANSLATE_NOOP("Command","Hide")){}

	virtual const char *getName(int arg)
	{
		switch(arg)
		{
		default: assert(0);
		case 0: return TRANSLATE_NOOP("Command","Hide Selected");
		case 1: return TRANSLATE_NOOP("Command","Hide Unselected");
		case 2: return TRANSLATE_NOOP("Command","Unhide");
		case 3: return TRANSLATE_NOOP("Command","Unhide All");
		
		}
	}

	virtual const char *getKeymap(int arg)
	{
		switch(arg)
		{
		default: assert(0);
		case 0: return "H";
		case 1: return "Shift+H";		
		case 2: return "Shift+Space";
		case 3: return "Ctrl+H";
		}
	}

	virtual bool activated(int,Model*);
};

extern Command *hidecmd(){ return new HideCommand; }

bool HideCommand::activated(int arg, Model *model)
{
	const char *msg; switch(arg)
	{
	case 0:
		msg = TRANSLATE("Command","Selection hidden");
		model->hideSelected(); break;
	case 1:
		msg = TRANSLATE("Command","Inversion hidden");		
		model->hideUnselected(); break;
	default: assert(0); 	
	case 2:
		msg = TRANSLATE("Command","Unhidden");
		model->invertHidden(); break;
	case 3:
		msg = TRANSLATE("Command","All unhidden");
		model->unhideAll(); break;
	}
	model_status(model,StatusNormal,STATUSTIME_SHORT,msg);
	return true;
}

