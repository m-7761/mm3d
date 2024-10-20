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

#ifdef HAVE_LUALIB

#ifndef __LUAIF_H
#define __LUAIF_H

#include "luascript.h"

class Model;
class MeshRectangle;

class LuaContext
{
	public:

		LuaContext(Model *model),~LuaContext();

		Model *m_scriptModel;
		Model *m_currentModel;
		std::vector<Model*> m_list; // Does not include m_scriptModel
		int_list m_createdVertices;
		int_list m_createdTriangles;
		std::vector<MeshRectangle*> m_createdRectangles;

	protected:
};

extern void luaif_registerfunctions(LuaScript *lua,LuaContext *context);

#endif // HAVE_LUALIB

#endif // __LUAIF_H
