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

#include "model.h"

#include "texturecoord.h" //Widget

struct GroupWin : Win
{
	void submit(int);
	
	enum{ id_smooth=1000,id_angle };

	GroupWin(Model *model)
		:
	Win("Groups"),
	model(model),
	group(main,"",id_item),
	nav1(main),
	add(nav1,"New",id_new),
	name(nav1,"Rename",id_name),
	del(nav1,"Delete",id_delete),
	sel(nav1,"Select",id_value), //2022
	faces1(main),
		//TODO: What about add to selection?
	select(faces1,"Select Faces In Group",id_select), 
	deselect(faces1,"Unselect Faces In Group",-id_select),
	smooth(main),angle(main),
	nav2(main),
	scene(nav2,id_scene),	
		col1(nav2),
	material(nav2,"Material",id_subitem),
	faces2(nav2,"Membership",bi::none), //"Faces"	
	set_faces(faces2,"Assign Faces",id_assign), //"Assign As Group"
	add_faces(faces2,"Add To Group",id_append), //"Add To Group"	
	f1_ok_cancel(main),
	texture(scene,false)
	{
		//texwin.cc's happens to be 100 x 100.
		scene.lock(100,100);

		smooth.space(3,2);

		faces1.expand_all();
		faces2.expand_all();
		nav2.expand();
		col1.expand().space<top>(1);
		material.expand().place(bottom);

		active_callback = &GroupWin::submit;
		
		submit(id_init);
	}
	
	Model *model;

	dropdown group;
	row nav1;
	button add,name,del,sel;
	row faces1;
	button select,deselect;
	bar smooth,angle; //spinbox?
	panel nav2;
	canvas scene;
	column col1;
	dropdown material;
	row faces2;
	button set_faces,add_faces;
	f1_ok_cancel_panel f1_ok_cancel;

	Widget texture;

	void group_selected();
	void new_group_or_name(int);	
};
void GroupWin::submit(int id)
{
	switch(id)
	{
	case id_init:
	{	
		//2022: Default to empty selection
		//for better sidebar support... but
		//might better default to first when
		//opened from menus?
		group.select_id(-1);
		//2022: Useful for selecting ungrouped
		//faces (did I remove this?) (no)
		group.add_item(-1,::tr("<None>"));

		group.expand();

		int iN = model->getGroupCount();
		for(int i=0;i<iN;i++)
		group.add_item(i,model->getGroupName(i));

		iN = model->getTextureCount();
		material.add_item(-1,::tr("<None>"));
		for(int i=0;i<iN;i++)	
		material.add_item(i,model->getTextureName(i));
		texture.setModel(model);
		
		bool sel = false;
		for(int i=0;i<model->getTriangleCount();i++)
		if(model->isTriangleSelected(i))
		{
			sel = true;
			if(int g=model->getTriangleGroup(i)+1)
			{
				group.select_id(g-1); break;
			}
		}
		//2022: select first if selection is empty?
		//(I.e. can't be originating from sidebar.)
		if(!sel&&!group.empty())
		{
			group.select_id(0);
		}

		smooth.set_range(0,255).id(id_smooth).expand();
		angle.set_range(0,180).id(id_angle).expand();
		{
			group_selected();		
		}		
		smooth.sspace<left>({angle}); //EXPERIMENTAL

		break;
	}	
	case id_scene:

		texture.draw(scene.x(),scene.y(),scene.width(),scene.height());
		break;
		
	case id_item:

		group_selected();
		break;

	case id_delete: case id_value:
	case id_select: case -id_select: //Deselect?
	case id_assign: case id_append:
	case id_subitem:
	{
		int g = group; switch(id)
		{
		case id_delete:

			model->deleteGroup(g);	
			group.delete_all().select_id(0);
			for(int i=0,iN=model->getGroupCount();i<iN;i++)	
			group.add_item(i,model->getGroupName(i));
			group_selected();
			break;

		case id_value: //HACK: new Select button?

			model->unselectAllTriangles(); //break;

		case id_select: 

			if(g==-1) ungroup: //2022
			{
				model->selectUngroupedTriangles(id!=-id_select); //id_scene?				
			}
			else
			{		
			//	model->unselectAllTriangles(); //TODO: Disable option?
				model->selectGroup(g);			
			}
			break;

		case -id_select: 
			
			if(g==-1) goto ungroup; //2022			
			else model->unselectGroup(g); break;		

		case id_assign: model->setSelectedAsGroup(g); break; 
		case id_append:
			
			if(g==-1) //2022
			{				
				//EXTREMELY INEFFECIENT
				int_list l;
				model->getSelectedTriangles(l);
				for(int i:l)
				{
					int gg = model->getTriangleGroup(i);
					if(gg>=0)
					model->removeTriangleFromGroup(gg,i);
				}
			}
			else model->addSelectedToGroup(g); break;

		case id_subitem:

			model->setGroupTextureId(g,material);
	
			texture.setTexture(material); break;			
		}		
		model->updateObservers();
		break;
	}
	case id_smooth:

		//Obscuring this adds phantom positions to this slider.
		//smooth.name().format("%s%03d\t",::tr("Smoothness: "),(int)((val/255.0)*100));
		smooth.name().format("%s\t%03d",::tr("Smoothness: "),(int)smooth);
		model->setGroupSmooth((int)group,0xFF&smooth);
		if(model->validateNormals())
		model->updateObservers();
		break;

	case id_angle:

		angle.name().format("%s\t%03d",::tr("Max Angle: "),(int)angle);
		model->setGroupAngle((int)group,0xFF&angle);
		if(model->validateNormals())
		model->updateObservers();
		break;

	case id_new: case id_name:

		new_group_or_name(id);
		break;

	case id_ok:		

		model->operationComplete(::tr("Group changes","operation complete"));		
		break;

	case id_cancel:

		model->undoCurrent();
		break;
	}

	basic_submit(id);
}
void GroupWin::group_selected()
{
	//2022: empty means the list is empty, whereas the selection
	//can be cleared if opened from the sidebar with <None> used.
	//if(group.empty())
	//if(!group.selection())
	if(-1==group.int_val()) //2022: adding express <None> option.
	{
		disable();
		sel.enable();
		add.enable();
		f1_ok_cancel.nav.enable();
		faces1.enable(); add_faces.enable(); //For <None> option.	

		//material.select_id(0); //2022
		if(!group.empty()) group.enable();

		material.select_id(-1);
	}
	else
	{
		if(!del.enabled()) enable(); //2022: <New> had done this.

		int g = group;
		smooth.set_int_val(model->getGroupSmooth(g)); 
		angle.set_int_val(model->getGroupAngle(g));	
		material.select_id(model->getGroupTextureId(g));
	}
	texture.setTexture(material);

	submit(id_smooth); submit(id_angle);
}
void GroupWin::new_group_or_name(int id)
{
	int g = group; std::string name;
	utf8 str = model->getGroupName(g);
	if(str) name = str;
	str = id==id_new?"New group":"Rename group";	
	if(id_ok==EditBox(&name,::tr(str,"window title"),
	::tr("Enter new group name:"),1,Model::MAX_GROUP_NAME_LEN))
	{
		if(id==id_name)
		{	
			model->setGroupName(g,name.c_str());
			group.selection()->set_text(name);	
		}
		else if(id==id_new)
		{
			g = model->addGroup(g,name.c_str());
			group.add_item(g,name);
			group.select_id(g);
			group_selected();
			submit(id_smooth);
			submit(id_angle);
		}
		else assert(0);
	}
}

extern void groupwin(Model *m){ GroupWin(m).return_on_close(); }
