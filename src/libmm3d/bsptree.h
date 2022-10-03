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

#ifndef __BSPTREE_H
#define __BSPTREE_H

#include "drawcontext.h"

class BspTree
{
public:

	struct Draw
	{
		unsigned ops;
	
		int texture_matrix;
	
		class DrawingContext *context;

		class Model *bsp;

		unsigned layers;
	};
	struct Node;
	struct Poly;

	BspTree():m_root(){};
	~BspTree(){ clear(); };

	void render(double *point, Draw&);

	void addTriangle(Model*m,unsigned);
	void clear();
	bool empty(){ return !m_root; }

	static int flush();
	static void stats();

protected:

	Node *m_root;

	static std::vector<Node*> s_recycle;

	static int s_allocated;
};

#endif // __BSPTREE_H

