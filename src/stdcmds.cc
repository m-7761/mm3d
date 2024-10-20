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

//#include "config.h"
#include "cmdmgr.h"
#include "log.h"
#include "cmdmgr.h"

#include "menuconf.h"

extern void init_std_cmds(CommandManager *cmdMgr)
{
	//log_debug("initializing standard commands\n");

	#define _(x) \
	extern Command *x##cmd(); cmdMgr->addCommand(x##cmd());
	#define __() cmdMgr->addSeparator();

	_(copy)__()_(delete)_(dup)__()_(invert)__()

	// Vertices submenu
	
	_(weld)_(snap)__()_(selectfree)

	// Faces submenu
	
	_(makeface)_(subdivide)_(edgeturn)_(rotatetex)
	__()_(invnormal)_(faceout) //INSANE //OVERKILL

	// Meshes submenu

	_(simplify)_(cap)_(spherify)

	// Other

	__()_(hide)_(flatten)_(flip)_(align)_(extrude)

	#undef _
	#undef __
}

