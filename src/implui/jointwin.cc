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

#include "viewwin.h"

#include "log.h"
#include "msg.h"
#include "modelstatus.h"

//ISSUES
//https://github.com/zturtleman/mm3d/issues/49

struct JointWin : Win
{
	void submit(control*);

	JointWin(MainWin &model, int i)
		:
	Win("Joints"),model(model),loo(i!=0),
	joint(main,"",id_item),
	nav(main),
	name(nav,"Rename",id_name),
	del(nav,"Delete",id_delete),
	sel(main),f1_ok_cancel(main)
	{
		//TEMPORARY FIX
		//This is to avoid nagging on close
		//and also to restore the selection.
		glutSetWindow(glut_window_id());
		glutext::glutCloseFunc(auto_cancel);

		joint.expand();
		nav.expand_all().proportion();
		sel.nav.expand_all();
		
		active_callback = &JointWin::submit;

		submit(main);
	}

	MainWin &model; bool loo; //100

	void confirm_close()
	{
		extern void win_close();
		glutext::glutCloseFunc(win_close);
	}
	static void auto_cancel()
	{
		auto w = (JointWin*)event.find_ui_by_window_id();
		w->submit(w->f1_ok_cancel.ok_cancel.cancel);
	}

	struct selection_group
	{
		selection_group(node *main)
			:
		nav(main,"Selection"),
		infl(nav,"Select Joint Vertices"),
		uninfl(nav,"Select Unassigned Vertices"),
		assign(nav,"Assign Selected To Joint"),
		append(nav,"Add Selected To Joint")
		{}

		panel nav;
		button infl,uninfl,assign,append;
	};

	dropdown joint;
	row nav;
	button name,del;
	selection_group sel;
	f1_ok_cancel_panel f1_ok_cancel;
};

void JointWin::submit(control *c)
{
	//ISSUES
	//https://github.com/zturtleman/mm3d/issues/49

	int id = c->id(); switch(id)
	{
	case id_name: //"Rename"

		if(id_ok==EditBox(&joint.selection()->text(),
		::tr("Rename joint","window title"),
		::tr("Enter new joint name:"),1,Model::MAX_NAME_LEN))
		{
			confirm_close(); //HACK

			model->setBoneJointName((int)joint,joint.selection()->c_str());
		}
		break;

	case id_delete: //"Delete"

		confirm_close(); //HACK

		model->deleteBoneJoint((int)joint); 

		joint.clear(); //break; //NEW

	case id_init:
	
		//2021: Prevent X confirmation prompt on empty
		//selection.
		joint.add_item(-1,"<None>").select_id(-1);

		for(int i=0;i<model->getBoneJointCount();i++)
		{
			joint.add_item(i,model->getBoneJointName(i));
		}

		//TODO: A multi-select listbox might be better
		for(auto&i:model.selection)
		if(i.type==Model::PT_Joint)
		{		
			joint.select_id(i); //break;

			if(1!=model.nselection[Model::PT_Joint])
			{
				//Note: This caused the X button to
				//request to ask for a confirmation.
				goto id_item; 
			}
		}
		else if(!i.type) break; //nice?

		goto enable;

	case id_item: id_item: 

		//Note: This is just a visual aid.
		model->unselectAllBoneJoints();
		model->selectBoneJoint((int)joint);
		enable: if(-1==(int)joint)
		{
			disable(); 
			joint.enable();
			sel.uninfl.enable();
			f1_ok_cancel.nav.enable();
		}
		else enable(); model->updateObservers();

		break;
	
	default: //"Selection"

		switch(int bt=(button*)c-&sel.infl)
		{
		default: assert(0); return; //2022

		case 0: //"Select Joint Vertices"
		case 1: //"Select Unassigned Vertices"
		{
			model->unselectAllVertices();

			infl_list::const_iterator it;
			for(int j=joint,i=0,iN=model->getVertexCount();i<iN;i++)
			{
				const infl_list &l = model->getVertexInfluences(i);				
				if(bt==0) for(it=l.begin();it!=l.end();it++)
				{
					if(it->m_boneId==j)
					{
						select: model->selectVertex(i); break;
					}
				}
				else if(l.empty()) goto select;
			}
			for(int j=joint,i=0,iN=model->getPointCount();i<iN;i++)
			{
				const infl_list &l = model->getPointInfluences(i);
				if(bt==0) for(it=l.begin();it!=l.end();it++)
				{
					if(it->m_boneId==j)
					{
						select2: model->selectPoint(i); break;
					}
				}
				else if(l.empty()) goto select2;
			}			
			break;
		}
		case 2: //"Assign Selected to Joint"
		case 3: //"Add Selected to Joint"
		{
			int j = joint;
			//log_debug("%sing %d objects to joint %d\n",bt==2?"ass":"add",model.selection.size(),j); //???
			for(auto&i:model.selection)			
			if(bt==2||bt==3&&loo)
			{
				model->setPositionBoneJoint(i,j);
			}
			else //2020 (standard way of auto-assignment elsewhere)
			{
				double w = model->calculatePositionInfluenceWeight(i,j);
				model->setPositionInfluence(i,j,Model::IT_Auto,w);
			}
			break;
		}}

		confirm_close(); //HACK

		model->updateObservers();
		break;

	case id_ok:

		//log_debug("Joint changes complete\n");
		model->operationComplete(::tr("Joint changes","operation complete"));
		break;

	case id_cancel:

		//log_debug("Joint changes canceled\n");
		model->undoCurrent();
		break;
	}

	basic_submit(id);
}

extern void jointwin(MainWin &m, int loo){ JointWin(m,loo).return_on_close(); }
