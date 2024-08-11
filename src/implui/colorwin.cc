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

#include "msg.h"
#include "colorwin.h"
#include "viewwin.h"

void ColorWin::open()
{
	if(!hidden()) return;

	show(); 
	
	layoutChanged();
	jointsChanged();
}
void ColorWin::setModel()
{
	for(int i=0;i<6;i++)
	{		
		vp[i].primary.joint.user = nullptr;
		vp[i].secondary.joint.user = nullptr;
		vp[i].mouse4.joint.user = nullptr;
		vp[i].mouse5.joint.user = nullptr;
	}
	jointsChanged();

	for(int i=6;i-->0;) if(i!=cur)
	{
		submit(vp[i].tool);
	}
	submit(vp[cur].tool);
}
void ColorWin::setJoint(int bt, int jt)
{
	if(bt<0||bt>3){ assert(0); return; }

	auto &vc = model->getViewportColors()[cur];
	for(int i=4;i-->0;)	
	if(vc.joints[i]==jt)
	{
		model_status(model,StatusNotice,STATUSTIME_SHORT,
		TRANSLATE("Tool","Joint already assigned to a button"));
		return;
	}

	if((bt==0||bt==1)&&(&v1)[cur].lock
	 ||(bt==2||bt==3)&&(&v1)[cur].lock2) 
	{
		model_status(model,StatusNotice,STATUSTIME_SHORT,
		TRANSLATE("Tool","Can't assign to locked joint"));
		return;
	}

	switch(bt)
	{
	case 0: submit(&(&v1)[cur].primary.joint.select_id(jt)); break;
	case 1: submit(&(&v1)[cur].secondary.joint.select_id(jt)); break;
	case 2: submit(&(&v1)[cur].mouse4.joint.select_id(jt)); break;
	case 3: submit(&(&v1)[cur].mouse5.joint.select_id(jt)); break;
	}

	utf8 kind[4] = {"primary","secondary","mouse4","mouse5"};
	model_status(model,StatusNotice,STATUSTIME_SHORT,
	TRANSLATE("Tool","Assigned joint to %s color"),kind[bt]);
}
void ColorWin::jointsChanged()
{
	if(hidden()) return;

	auto &jl = model->getJointList();
	auto &ref = model.sidebar.prop_panel.infl.parent_joint;	
	for(int i=0;i<6;i++)
	{		
		dropdown *l[4] = 
		{
			&(&v1)[i].primary.joint,
			&(&v1)[i].secondary.joint,
			&(&v1)[i].mouse4.joint,
			&(&v1)[i].mouse5.joint,
		};
		for(auto*ea:l)
		{
			ea->clear();
			ea->reference(ref).select_id(-1);

			for(size_t j=jl.size();j-->0;)
			{
				if(ea->user==jl[j]) 
				ea->select_id((int)j);
			}
		}
	}
}
void ColorWin::layoutChanged()
{
	if(hidden()) return;

	int m = model.views.viewsM;
	int n = model.views.viewsN;
	
	for(int i=6;i-->0;)
	{
		if(i>=4)
		{
			vp[i].tool.set_parent(i==4?row1:row2);
			vp[i].pair.set_parent(i==4?row3:row4);
		}
		else if(i>=2)
		{
			vp[i].tool.set_parent(row2);
			vp[i].pair.set_parent(row4);
		}
		else 
		{
			vp[i].tool.set_parent(i<m?row1:row2);
			vp[i].pair.set_parent(i<m?row3:row4);
		}

		if(!vp[i].viewport.hidden())
		{
			assert(cur==i); cur = i;
		}
		vp[i].tool.set_hidden(i>=n);
		vp[i].pair.set_hidden(i>=n);
	}
	if(cur>=m*n)
	{
		vp[cur].viewport.set_hidden();
		vp[0].viewport.set_hidden(false);
		cur = 0;
	}

	activate_control(vp[cur].tool);

	submit(vp[cur].tool);

	model.views.updateAllViews();
}
void ColorWin::_rename_tabs()
{
	control *ch = _first_tab;
	for(int i=0;i<6;i++,ch=ch->next()) if(i!=cur)
	{
		char name[2] = {'1'+(char)i};
		ch->name(name); ch->span(10);
	}
	else
	{
		char name[] = {"Viewport 1"};
		name[9]+=cur; ch->name(name); 
	}
}
void ColorWin::submit(control *c)
{
	int id = c->id(); switch(id)
	{
	case id_init:
	{			
		for(int i=0;i<6;i++)
		{
			char name[2] = {'1'+(char)i};
			auto it = new multiple::item(tab,i?name:"Viewport 1",i);
			it->span(10);
			if(i==0) _first_tab = it;
		}

		v1.primary.color.add_item(0,"Custom")
		.add_item(0xffff0000,"Blue")
		.add_item(0xff00ff00,"Green")
		.add_item(0xffffff00,"Cyan")
		.add_item(0xff0000ff,"Red") 
		.add_item(0xffff00ff,"Magenta")
		.add_item(0xff00ffff,"Yellow")
		.add_item(-1,"<Custom>")
		.select_id(0xffff0000);
		v1.secondary.color.reference(v1.primary.color)
		.select_id(0xff0000ff);
		v1.mouse4.color.reference(v1.primary.color)
		.select_id(0xffffff00);
		v1.mouse5.color.reference(v1.primary.color)
		.select_id(0xff00ffff);
		for(int i=1;i<6;i++)
		{		
			vp[i].primary.color.reference(v1.primary.color)
			.select_id(0xffff0000);
			vp[i].secondary.color.reference(v1.primary.color)
			.select_id(0xff0000ff);
			vp[i].viewport.set_hidden();
			vp[i].mouse4.color.reference(v1.primary.color)
			.select_id(0xffffff00);
			vp[i].mouse5.color.reference(v1.primary.color)
			.select_id(0xff00ffff);
		}
		jointsChanged();
		
		for(int i=6;i-->0;)
		{
			vp[i].viewport.pack();

			vp[i].mouse4.color.space<left>() = 
			vp[i].mouse5.color.space<left>() =
			vp[i].primary.color.space<left>() = v1.secondary.color.space<left>();

			//HACK/TODO: initialize from getViewportColors
			submit(vp[i].primary.color);
			submit(vp[i].primary.joint);
			submit(vp[i].secondary.color);
			submit(vp[i].secondary.joint);
			submit(vp[i].mouse4.color);
			submit(vp[i].mouse4.joint);
			submit(vp[i].mouse5.color);
			submit(vp[i].mouse5.joint);			
		}
		layoutChanged(); return;
	}
	case id_tab:

		id = '1'+(int)tab; //break;

	case '1': case '2': case '3': case '4': case '5': case '6':
	{
		vp[cur].viewport.set_hidden(true);
		vp[cur=id-'1'].viewport.set_hidden(false);
				
		tab.set_int_val(id-'1'); _rename_tabs();

		vp[cur].pair.enable(!vp[cur].tool);

		for(int i=6;i-->0;)
		if(vp[i].pair.name()!=vp[cur].tool.name())
		{
			vp[i].pair.name(vp[cur].tool.name());
			if(vp[i].pair&&vp[i].pair.enabled())
			{
				submit(vp[i].primary.color);
				submit(vp[i].primary.joint);
				submit(vp[i].secondary.color);
				submit(vp[i].secondary.joint);
				submit(vp[i].mouse4.color);
				submit(vp[i].mouse4.joint);
				submit(vp[i].mouse5.color);
				submit(vp[i].mouse5.joint);			
			}
		}
		submit(vp[cur].primary.color);
		submit(vp[cur].primary.joint);
		submit(vp[cur].secondary.color);
		submit(vp[cur].secondary.joint);
		submit(vp[cur].mouse4.color);
		submit(vp[cur].mouse4.joint);
		submit(vp[cur].mouse5.color);
		submit(vp[cur].mouse5.joint);		
		return;
	}
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':

		submit(vp[id-'a'].primary.color);
		submit(vp[id-'a'].primary.joint);
		submit(vp[id-'a'].secondary.color);
		submit(vp[id-'a'].secondary.joint);
		submit(vp[id-'a'].mouse4.color);
		submit(vp[id-'a'].mouse4.joint);
		submit(vp[id-'a'].mouse5.color);
		submit(vp[id-'a'].mouse5.joint);
		return;

	case 'vc':
	{
		auto *p = (Infl*)c->parent();
		if(-1==(int)*c)
		{
			p->color_custom = glutext::glutColorPicker(p->color_custom);
			p->color.select_id(0);
		}		
		p->panel::color(p->custom_color()&0x80ffffff).redraw();

		auto &vc = model->getViewportColors()[p->vp];
		vc.colors[p->vc] = p->pair()->custom_color();

		return model.views.updateAllViews();
	}
	case 'jt':
	{
		auto *p = (Infl*)c->parent();
		int j = p->joint;
		auto &jl = model->getJointList();
		c->user = j==-1?nullptr:(void*)jl[j]; 
		auto &vc = model->getViewportColors()[p->vp];
		vc.joints[p->vc] = (int)p->pair()->joint;

		return model.views.updateAllViews();
	}
	case 'lock':

		if(c==vp[cur].lock2)
		{
			vp[cur].secondary.joint.enable(!*c);
			vp[cur].mouse5.joint.enable(!*c);
		}
		else
		{
			vp[cur].primary.joint.enable(!*c);
			vp[cur].mouse4.joint.enable(!*c);			
		}
		return;

	case 'swap':

		if(c==vp[cur].swap2)
		{
			vp[cur].secondary.swap(vp[cur].mouse5);
		}
		else vp[cur].primary.swap(vp[cur].mouse4);

		return;

	case id_close:

		return hide();
	}

	basic_submit(id);
}
void ColorWin::Infl::swap(Infl &b)
{
	int c = color, j = joint; 

	int cc = b.color, jj = b.joint;
	
	std::swap(joint.user,b.joint.user);
	std::swap(color_custom,b.color_custom);

	color.set_int_val(cc);
	joint.set_int_val(jj);

	b.color.set_int_val(c);
	b.joint.set_int_val(j);
		
	auto cw = (ColorWin*)ui();

	cw->submit(color); cw->submit(b.color);
	cw->submit(joint); cw->submit(b.joint);
}
ColorWin::Infl *ColorWin::Infl::pair()
{
	auto &cw = *(ColorWin*)ui();

	auto *vp = &cw.v1;
	for(int i=0;i<6;i++)
	if((char*)&vp[i]<(char*)this&&(char*)&vp[i+1]>(char*)this)
	{
		if(vp[i].pair&&vp[i].pair.enabled())
		{
			if(this==vp[i].primary) return &vp[cw.cur].primary;
			if(this==vp[i].secondary) return &vp[cw.cur].secondary;
			if(this==vp[i].mouse4) return &vp[cw.cur].mouse4;
			if(this==vp[i].mouse5) return &vp[cw.cur].mouse5;

			assert(0);
		}
		break;
	}

	return this;
}
