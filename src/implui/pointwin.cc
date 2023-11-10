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

struct PointWin : Win //pointwin.htm
{
	void submit(int);

	PointWin(MainWin &model)
		:
	Win("Reorder"),model(model),
	type(main,"",id_item),
	table(main,"",id_subitem),
	nav1(main),		
		up(nav1,"Up",id_up),
		down(nav1,"Down",id_down),
	nav2(main),
		add(nav2,"New ",id_new), 
		name(nav2,"Rename",id_name),
		del(nav2,"Delete",id_delete),
	//nav3(main),
	//	dup(nav3,"Copy",id_copy),
	//	name(nav2,"Select",id_select),
	f1_ok_cancel(main)
	{
		(multisel_item::mscb&) //YUCK
		table.user_cb = mscb;

		type.expand();
		nav1.ralign();
		nav2.expand_all().proportion();
		////nav3.expand_all().proportion();
		//nav3.expand();
		
		//The columns should support the window.
		//But just to be safe.
		//table.expand();
		table.lock(330,10*table.font().height);

		active_callback = &PointWin::submit;

		submit(id_init);
	}

	MainWin &model;

	dropdown type; listbox table;
 	//listbar header;
	row nav1; button up,down;
	row nav2; button add,name,del;
	//row nav3; button dup,select;
	f1_ok_cancel_panel f1_ok_cancel;

	template<class O>
	void list(int i, const O *o, bool sel=false)
	{
		auto *it = new multisel_item(i,o->m_name.c_str());

		table.add_item(it); if(sel) it->select();
	}
	void rename(int i, const char *str)
	{
		if(*str) switch(type)
		{
		case Model::PartPoints: model->setPointName(i,str); break;
		case Model::PartJoints: model->setBoneJointName(i,str); break;
		case Model::PartGroups: model->setGroupName(i,str); break;
		case Model::PartMaterials: model->setTextureName(i,str); break;
		case Model::PartAnims: model->setAnimName(i,str); break;
		}
	}
	bool move(int i, int j)
	{
		bool mv = false; switch(type)
		{
		case Model::PartPoints: mv = model->indexPoint(i,j); break;
		case Model::PartJoints: mv = model->indexJoint(i,j); break;
		case Model::PartGroups: mv = model->indexGroup(i,j); break;
		case Model::PartMaterials: mv = model->indexMaterial(i,j); break;
		case Model::PartProjections: mv = model->indexProjection(i,j); break;
	//	case Model::PartAnims: mv = model->moveAnimation(i,j); break;
		}
		return mv;
	}
	bool select(int i, bool sel)
	{
		bool mv = false; switch(type)
		{
		case Model::PartPoints: mv = model->selectPoint(i,sel); break;
		case Model::PartJoints: mv = model->selectBoneJoint(i,sel); break;
		}
		return mv;		
	}
	static void mscb(int impl, multisel_item &it)
	{
		if(~impl&it.impl_multisel) return;

		auto w = (PointWin*)it.list().ui();

		w->select(it.id(),it.multisel());

		w->model->updateObservers();
	}
};

void PointWin::submit(int id)
{
	switch(id)
	{
	case id_init:
	
		type.add_item(Model::PartPoints,::tr("Points"));
		type.add_item(Model::PartJoints,::tr("Bone Joints"));		
		type.add_item(Model::PartGroups,::tr("Groups"));
		type.add_item(Model::PartMaterials,::tr("Materials"));
		type.add_item(Model::PartProjections,::tr("Projections"));
	//	type.add_item(Model::PartAnims,::tr("Animations")); //UNFINISHED

		switch(model->getSelectionMode())
		{
		default:
		case Model::SelectPoints: type.select_id(Model::PartPoints); break;
		case Model::SelectJoints: type.select_id(Model::PartJoints); break;
		case Model::SelectProjections: type.select_id(Model::PartProjections); break;
		case Model::SelectGroups: type.select_id(Model::PartGroups); break;
		}
		//type.execute_callback(); break; //FALL-THRU
	
	case id_item: table.clear();
	
		id = 0; switch(type)
		{
		case Model::PartPoints:
		for(auto*o:model->getPointList()) list(id++,o,o->m_selected); break;
		case Model::PartJoints:
		for(auto*o:model->getJointList()) list(id++,o,o->m_selected); break;
		case Model::PartGroups:
		for(auto*g:model->getGroupList()) list(id++,g); break;
		case Model::PartMaterials:
		for(auto*m:model->getMaterialList()) list(id++,m); break;
		case Model::PartAnims:
		for(auto*a:model->getAnimationList()) list(id++,a); break;
		}
		nav1.enable((int)type!=Model::PartAnims);
		add.enable((int)type!=Model::PartAnims);		
		return;
	
	case id_subitem: 
	
		switch(event.get_click())
		{
		case 2: id = id_name; break; //Double-click?
		case 1:

			//This test implements Windows Explorer
			//select, wait, single-click, to rename 
			//logic.
			if(event.listbox_item_rename(true,true))
			{
				textbox modal(table); 

				if(modal.move_into_place())
				if(modal.return_on_enter()) 
				{
					table^[&](li::multisel ea)
					{
						rename(ea->id(),modal);
					};
				}
			}
			//break;

		case 0: return; //2022
		}
		//break; //FALL-THRU
	
	case id_name: case id_new:
	{
		int j = 0;
		li::item *it = nullptr;
		table^[&](li::multisel ea)
		{
			j++; it = ea;
		};		
		if(!j&&id==id_name)
		{
			//Should probably disable Delete/Rename then?
			return event.beep();
		}
		if(j>1) it = nullptr;
		std::string name;
		if(id_ok!=EditBox(&name,"Rename"))
		return;
		int converted = 0;
		utf8 c_str = name.c_str();
		j = -1; table^[&](li::multisel ea)
		{
			if(id==id_new)
			{
				if(j==-1) j = ea->id(); ea->unselect();
			}
			else rename(ea->id(),name.c_str()); //id_name
		};
		if(id==id_new)
		{
			int i = -1;
			switch(type)
			{
			case Model::PartPoints: i = model->addPoint(c_str); model->selectPoint(i); break;
			case Model::PartJoints: i = model->addBoneJoint(c_str); model->selectBoneJoint(i); break;
			case Model::PartGroups: i = model->addGroup(c_str); break;
			case Model::PartMaterials: i = model->addMaterial(c_str); break;
		//	case Model::PartAnims: i = model->addAnimation(am,c_str); break;
			}
			if(i==-1) break;

			if(j==-1) j = i;

			bool mv = move(i,j);

			if(mv) i = j;
			if(mv) switch(type)
			{
			case Model::PartPoints: list(id++,model->getPointList()[i],true); break;
			case Model::PartJoints: list(id++,model->getJointList()[i],true); break;
			case Model::PartGroups:	list(id++,model->getGroupList()[i],true); break;
			case Model::PartMaterials: list(id++,model->getMaterialList()[i],true); break;
			//case Model::PartAnims: list(id++,model->getAnimationList()[i],true); break;
			}
		}
		if(j!=-1) table.outline(j);

		return;
	}
	case id_up:

		if(!table.first_item()->multisel())
		{
			table^[&](li::multisel ea)
			{
				table.insert_item(ea,ea->prev());
				move(ea->id(),ea->id()-1);
			};
			submit(id_item); //refresh
		}
		else event.beep(); break;

	case id_down:
		
		if(!table.last_item()->multisel())
		{
			table^[&](li::reverse_multisel ea)
			{
				table.insert_item(ea,ea->next(),behind);
				move(ea->id(),ea->id()+1);
			};
			submit(id_item); //refresh
		}
		else event.beep(); break;

	case id_delete:

		id = 0; table^[&](li::multisel ea)
		{
			switch(type)
			{
			case Model::PartPoints: model->deletePoint(ea->id()-id); break;
			case Model::PartJoints: model->deleteBoneJoint(ea->id()-id); break;
			case Model::PartGroups: model->deleteGroup(ea->id()-id); break;
			case Model::PartMaterials: model->deleteTexture(ea->id()-id); break;
			case Model::PartAnims: model->deleteAnimation(ea->id()-id); break;
			}
			id++; table.erase(ea);
		};
		return;
	
	case id_ok:

		model->operationComplete(::tr("Reorder","operation complete"));
		break;
	
	case id_cancel:

		model->undoCurrent();
		break;
	}

	basic_submit(id);
}

extern void pointwin(MainWin &m){ PointWin(m).return_on_close(); }

