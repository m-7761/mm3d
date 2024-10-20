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


#ifndef __MODELSTATUS_H
#define __MODELSTATUS_H

#include "mm3dtypes.h"

enum StatusTypeE
{
	StatusNormal,
	StatusError,
	StatusNotice //2021: Like error but shouldn't use the term "error".
};

class StatusObject
{
public:

	StatusObject():m_model()
	{}
		
	// Status text
	virtual void setText(const char *str) = 0;
	virtual void addText(StatusTypeE type, int ms, const char *str) = 0;

	/*SIMPLIFYING (REMOVE ME)
	// Second (optional)argument is num selected
	virtual void setVertices(  unsigned v, unsigned sv = 0)= 0;
	virtual void setFaces(	  unsigned f, unsigned sf = 0)= 0;
	virtual void setGroups(	 unsigned g, unsigned sg = 0)= 0;
	virtual void setBoneJoints(unsigned b, unsigned sb = 0)= 0;
	virtual void setPoints(	 unsigned p, unsigned sp = 0)= 0;
	virtual void setTextures(  unsigned t, unsigned st = 0)= 0;
	*/
	virtual void setStats() = 0; 
	Model *getModel(){ return m_model; }
	Model *m_model;
};

const int STATUSTIME_NONE  = 0;
const int STATUSTIME_SHORT = 750;
const int STATUSTIME_LONG  = 6500;

//SIMPLIFY ME
typedef StatusObject *(*StatusObjectFunction)(Model*);
extern "C" void model_status(Model *model, StatusTypeE type, int ms, const char *fmt,...);
StatusObject *model_status_get_object(Model*);
void model_status_register_function(StatusObjectFunction func);

#endif // __MODELSTATUS_H
