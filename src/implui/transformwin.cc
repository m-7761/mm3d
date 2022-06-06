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
#include "win.h"

#include "transformwin.h"
#include "viewwin.h"

#include "model.h"
#include "glmath.h"
#include "log.h"

#include "msg.h"

void TransformWin::submit(control *c)
{	
	int id = c->id(); switch(id)
	{
	case id_init:
	
		tab.add_item("Translate")
		.add_item("Rotate").add_item("Scale") 
		.add_item("Matrix");
		scope.select_id(1)
		.add_item("Selected (including animations)")
		.add_item("Entire Model (including animations)");

		//Lock in largest tab.
		translate.nav.set_hidden();
		rotate.nav.set_hidden();
		scale.nav.set_hidden();
		tab.lock(true,true);
		
		//break;
	
	case id_tab:

		for(id=0,c=&translate.nav;id<4;c=c->next())
		{
			assert(c<=&matrix.nav);
			c->set_hidden(id++!=tab.int_val());
		}
		return; //id = id_tab; break;
		
	case id_ok: //id_apply //Enter? 

		if(1==(int)tab) //Try to solve Rotate?
		{
			auto &r = rotate;
			if(c!=r.rotate1&&c!=r.rotate2)
			{
				c = event.active_control;
				if(c>=r.x&&c<=r.z)				
				c = r.rotate1;				
				if(c>=r.ax&&c<=r.angle)				
				c = r.rotate2;				
			}
			if(c!=r.rotate1&&c!=r.rotate2)
			{
				bool r1 = (double)r.x||(double)r.y||(double)r.z;
				bool r2 = (double)r.ax||(double)r.ay||(double)r.az||(double)r.angle;
				if(r1&&!r2) c = r.rotate1;
				if(r2&&!r1) c = r.rotate2;

				if((r1&&r2)||(!r1||!r2))
				{
					event.beep(); return;
				}
			}
		}

		switch(tab)
		{
		case 0: translateEvent(); break;
		case 1: rotateEvent(c); break;
		case 2: scaleEvent(); break;
		case 3: matrixEvent(); break;
		}
		
		return; //break;
		
	//case id_ok: 
	case id_cancel:

		return hide();
	}	
	basic_submit(id);
}

void TransformWin::translateEvent()
{
	Matrix m;
	m.setTranslation(translate.x,translate.y,translate.z);
	applyMatrix(m,::tr("Matrix Translate"));
}
void TransformWin::rotateEvent(control *c)
{
	Matrix m; if(c==rotate.rotate1)
	{
		double vec[3] = { rotate.x,rotate.y,rotate.z };
		for(int i=0;i<3;i++) vec[i]*=PIOVER180; 
		m.setRotation(vec);
		applyMatrix(m,::tr("Matrix Rotate"));
	}
	else //rotate.rotate2
	{
		double vec[3] = { rotate.ax,rotate.ay,rotate.az };
		m.setRotationOnAxis(vec,rotate.angle.float_val()*PIOVER180);
		applyMatrix(m,::tr("Matrix Rotate On Axis"));
	}
}
void TransformWin::scaleEvent()
{
	Matrix m;
	m.set(0,0,scale.x);
	m.set(1,1,scale.y);
	m.set(2,2,scale.z);
	applyMatrix(m,::tr("Matrix Scale"));
}
void TransformWin::matrixEvent()
{
	Matrix m;
	for(int i=0;i<4;i++)
	for(int j=0;j<4;j++)
	m.set(i,j,matrix.m[i][j]);
	applyMatrix(m,::tr("Apply Matrix"));
}

bool TransformWin::matrixIsUndoable(const Matrix &m)
{
	return fabs(m.getDeterminant())>=0.0006;
}
bool TransformWin::warnNoUndo(bool undoable)
{
	if(undoable) return true;

	std::string q = ::tr("This transformation cannot be undone.");
	q+='\n'; q+=::tr("Are you sure you wish to continue?");
	
	return id_ok==WarningBox(::tr("Transform Cannot Be Undone","window title"),
	q.c_str(),		
	//::tr("Apply Transformation","button"),::tr("Cancel Transformation","button")
	id_ok|id_cancel);	
}
void TransformWin::applyMatrix(const Matrix &m, utf8 action)
{
	bool undoable = matrixIsUndoable(m);
	if(warnNoUndo(undoable))
	{
		auto e = scope&1?Model::OS_Global:Model::OS_Selected;
		model->applyMatrix(m,e,undoable);
		model->operationComplete(action);
	}
}
