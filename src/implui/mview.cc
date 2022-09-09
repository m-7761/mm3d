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

ViewBar::ViewBar(MainWin &model)
	:
Win(model.glut_window_id,
this==&model.views.bar1?top:bottom),
model(model)
{
	active_callback = &ViewBar::submit;
	
	if(top==subpos())
	{
		exterior_row.set_parent(this);
		portside_row.set_parent(this);
	}
	else
	{
		portside_row.set_parent(this);
		exterior_row.set_parent(this);		
	}
	exterior_row.expand();
	portside_row.space(0,1,2);
}

void ViewBar::submit(control *cc)
{		
	//Find the ModelView in particular as
	//a column in front of the containing
	//panel.

	if(model.views.params.nav==cc->parent())
	{
		model.views.tool->updateParam(cc->live_ptr());
		if(cc->user)
		config.set(model.views.param_config_key((utf8)cc->user),cc);
		return;
	}

	control *c = cc;
	while(c->parent()!=portside_row
		&&c->parent()!=exterior_row)
	{
		c = (control*)c->parent();				
	}
	if(ModelView*mv=c->prev<ModelView>())  
	{
		return mv->submit(cc->id());	
	}
	else if(c==cc) switch(cc->id())
	{
	case id_bar:
	//if(int n=cc->int_val()+1)
	//model.sidebar.anim_panel.frame.set_int_val(n).execute_callback();
	model.sidebar.anim_panel.frame.set_float_val(*cc).execute_callback();
	}
}
static std::vector<Tool::ViewE> mview_new;
void ViewBar::ModelView::init_view_dropdown()
{
	auto *ref = view.reference();
	auto &v = ref?*ref:view;
	v.clear();
	//I'm not crazy about how this looks, but Widgets95
	//will have to be modified to omit the \t part when
	//copying the value from the menu into its titlebar.
	//IT'S NOW IN THE MENU SYSTEM. 2 PLACES IS TOO MUCH.
	//v.add_item(Tool::ViewPerspective,"&Perspective\tPgUp"); 
	v.add_item(Tool::ViewPerspective,"&Perspective"); 
	for(auto ea:mview_new) if(ea<-1)
	{
		char n[] = "Persp (&0)"; n[8]+=-ea;
		v.add_item((int)ea,n);
	}	
	//&B is below T. F/R is left of K/L.
	//v.add_item(1,"&Front\tEnd").add_item(2,"Bac&k\tShift+End");
	//v.add_item(3,"&Left\tShift+PgDn").add_item(4,"&Right\tPgDn");
	//v.add_item(5,"&Top\tHome").add_item(6,"&Bottom\tShift+Home");
	//v.add_item(Tool::ViewOrtho,"&Orthographic\tShift+PgUp");
	v.add_item(1,"&Front").add_item(2,"Bac&k");
	v.add_item(3,"&Left").add_item(4,"&Right");
	v.add_item(5,"&Top").add_item(6,"&Bottom");
	v.add_item(Tool::ViewOrtho,"&Orthographic");
	for(auto ea:mview_new) if(ea>-1)
	{
		char n[] = "Ortho (&1)"; n[8]+=ea-Tool::ViewOrtho;
		v.add_item((int)ea,n);
	}v.add_item(-1,"<&New View>"); //EXPERIMENTAL

	if(!ref) return; //YUCK
	auto &vp = bar.model.views;
	for(int i=0;i<vp.viewsN;i++)
	{
		auto &vv = vp.views[i]->view;
		if(&vv==ref) continue;
		vv.clear();
		vv.reference(*ref).select_id(vv);
	}		
}
bool ViewBar::ModelView::Layer_multiple::item::mouse_up_handler(int x, int y, bool inside)
{
	auto *m = (Layer_multiple*)parent();
	if(event.curr_shift||event.curr_ctrl)
	if(inside&&id()==m->mview->port.getLayer())
	{
		//HACK: Make shift/ctrl work even if the radio is unchanged?
		parent()->execute_callback();		
	}
	return item::_mouse_up_handler(x,y,inside);	
}
bool ViewBar::ModelView::Layer_multiple::mouse_over(bool st, int x, int y)
{
	int val = int_val();
	for(auto*ch=first_child();ch;ch=ch->next())
	{
		ch->set_hidden(st?false:val!=ch->id());
	}
	return true;
};
void ViewBar::ModelView::submit(int id)
{	
	switch(id)
	{
	case id_init:
	
		if(!view.reference()) init_view_dropdown();
				
		layer.row_pack().space(0,-3,1,0,3);
		for(auto&l:layers)
		{
			layer.add_item(&l);
			int i = l.id();
			const char cc[5] = {'_','=','2','3','4'};
			l.name().push_back(cc[i]);
			l.style(l.style_tab|l.style_thin|l.style_hide);
			l.span(16).drop(18);

			l.set_hidden(i!=1); //mouse_over?		
		}
		layer.space<top>(top==ui()->subpos()?2:3); //HACK
		layer.select_id(1);

		if(top==ui()->subpos())
		{
			//nav.cspace_all<top>().space<bottom>(2); 
			view.space<top>(3);
			zoom.value.space<top>(4).space<right>(3);
		}
		else 
		{
			//nav.space<top>(1);
			view.space<top>(4); 
			zoom.value.space<top>(4).space<right>(3);
		}
		break;

	case id_item:
	
		switch(int e=view)
		{
		case -1:
		
			struct E : Win
			{
				E(int *lv):
				Win("Which projection?"),
				value(main,"",lv),
				persp(value,"Perspective"),
				ortho(value,"Orthographic"),
				ok_cancel(main)
				{
					int a = -1;
					int b = Tool::ViewOrtho;
					for(auto ea:mview_new)
					{
						a = std::min<int>(a,ea);
						b = std::max<int>(b,ea);
					}
					persp.id(a-1); ortho.id(b+1);
					a = a>-9; 
					b = b-Tool::ViewOrtho<8;					
					if(!a) persp.disable();
					if(!b) ortho.disable();
					if(!a&&!b) ok_cancel.ok.disable();
					else value.select_id(a?persp.id():ortho.id());
				}
				multiple value;
				multiple::item persp,ortho;
				ok_cancel_panel ok_cancel;
			};
			if(event.wheel_event
			||id_ok!=E(&e).return_on_close())
			{			
				view.select_id(port.getView());
				return;
			}
			mview_new.push_back((Tool::ViewE)e);
			MainWin::each([](MainWin &ea)
			{
				ea.views->init_view_dropdown(); 
			});
			//break;
		
		default: port.viewChangeEvent((Tool::ViewE)e); 
		}
		break;
	
	case '=': port.setZoomLevel(zoom.value); break;
	case '+': port.zoomIn(); break;
	case '-': port.zoomOut(); break;

	case id_assign:

		id = layer;

		if(event.curr_shift) //broadcast?
		{
			if(event.curr_ctrl&&!event.wheel_event) //assign?
			{
				if(id) //hidecmd.cc?
				{
					bar.model->hideSelected(true,id);

					model_status(bar.model,StatusNormal,STATUSTIME_SHORT,
					TRANSLATE("Command","Assigned to layer %d"),id);

					bar.model->operationComplete(TRANSLATE("Command","Transfer to Layer"));
				}
				else
				{
					model_status(bar.model,StatusError,STATUSTIME_LONG,
					TRANSLATE("Command","Can't assign hidden layer"));
				}
				
				layer.set_int_val(port.getLayer()); //reset?
			}
			else bar.model.perform_menu_action(id_view_layer_0+id);
		}
		else if(event.curr_ctrl&&!event.wheel_event) //overlay?
		{
			bar.model.perform_menu_action(id_view_overlay);

			layer.set_int_val(port.getLayer()); //reset?
		}
		else
		{		
			port.setLayer(id);

			//HACK: force focus?
			port.parent->m_focus = &port-port.parent->ports;
				
			port.parent->updateView(); 		
		
			model_status(bar.model,StatusNormal,STATUSTIME_SHORT,
			id?::tr("Layer %d"): ::tr("Hidden layer"),id);
		}
		return;
	}
}

