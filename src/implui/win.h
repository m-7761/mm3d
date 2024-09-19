
#ifndef __WIN_H__
#define __WIN_H__

#undef interface
#include <widgets95.h>
using namespace Widgets95::glute;
namespace glutext = Widgets95::glutext;

typedef const char *utf8;

/* Qt supplemental */
static const char *tr(utf8 str,...){ return str; }

extern Widgets95::configure *config,*keycfg;

enum
{
	pic_icon=0,
	pic_play,pic_pause,
	pic_stop,pic_loop,
	pic_zoomin,pic_zoomout,
	pic_N
};
extern int pics[pic_N]; //ui.cc

enum
{
	/*standard buttons*/
	id_init=0,id_ok,id_cancel,id_apply,id_yes,id_close, //5

	id_open, //6

	id_no=8, //ok/cancel/yes/no occupy bits 1 though 4. //8

	id_f1,

	/*printable ASCII range lives here*/

	/*common functions*/
	id_delete=127,id_new,id_name,id_value,id_source,id_browse,

	/*animation operations*/
	id_up,id_down,id_copy,id_split,id_join,id_merge,id_mirror,id_convert,

	/*group operations*/
	id_select,id_assign,id_append,id_remove,id_reset,
		
	id_item,id_subitem,_id_subitems_=id_subitem+9,

	id_sort,id_check,id_check_all,id_bar,id_tab,

	/*TextureWidget*/
	id_scene,

  //NOTE: THE REST DOUBLE AS keycfg KEYS//

	/*File menu*/
	id_file_new,
	id_file_open,
	id_file_resume,
	id_file_close,
	id_file_save_prompt, //Ctrl+S
	id_file_save,
	id_file_save_as,
	id_file_export,
	id_file_export_selection,
	id_file_run_script,
	id_file_plugins,
	id_file_prefs, //wxOSX needs to be Preferences.
	id_file_prefs_rmb,
	id_file_quit, //wxOSX needs to be Quit.

	//EXPERIMENTAL
	id_normal_order,
	id_normal_click,
	id_normal_ccw,

	/*View->Snap menu*/
	id_snap_grid,id_snap_vert,

	/*View->Render Options menu*/
	id_rops_draw_bones,
	id_rops_hide_joints,
	id_rops_hide_projections,	
	id_rops_hide_lines,
	id_rops_hide_backs,
	id_rops_draw_badtex,
	id_rops_draw_buttons,

	/*View menu*/
	id_frame_all,
	id_frame_selection,	
	id_frame_lock,
	id_ortho_wireframe,
	id_ortho_flat,
	id_ortho_smooth,
	id_ortho_texture,
	id_ortho_blend,
	id_persp_wireframe,
	id_persp_flat,
	id_persp_smooth,
	id_persp_texture,
	id_persp_blend,
	id_view_1,
	id_view_2,
	id_view_1x2,id_view_2x1,
	id_view_2x2,id_view_3x2,
	id_view_swap,
	id_view_flip,
	id_view_settings,
	id_view_init,
	id_view_persp, //0-7
	id_view_front,id_view_back,
	id_view_left,id_view_right,
	id_view_top,id_view_bottom,
	id_view_ortho,
	id_view_invert,
	id_view_invert_all,
	id_view_project,
	id_view_project_all,	
	id_view_layer_0,
	id_view_layer_1,
	id_view_layer_2,
	id_view_layer_3,
	id_view_layer_4,
	id_view_layer_lower,
	id_view_layer_raise,
	id_view_layer_lower_all,
	id_view_layer_raise_all,
	id_view_overlay,
	id_view_overlay_1,
	id_view_overlay_2,
	id_view_overlay_3,
	id_view_overlay_4,
	id_view_overlay_snap,

	/*Tool menu*/
	id_tool_none,
	id_tool_toggle,
	id_tool_recall,
	id_tool_back,
	id_tool_shift_lock, //EXPERIMENTAL
	id_tool_shift_hold,
	id_tool_colormixer,

	/*Model menu*/
	id_edit_undo,
	id_edit_redo,
	id_edit_utildata,
	id_edit_userdata,
	id_transform,
	id_background_settings,
	id_merge_models,
	id_merge_animations,
	id_clean_animations, //EXPERIMENTAL

	/*Materials menu*/
	id_group_settings,
	id_material_settings,
	id_material_cleanup,
	id_refresh_textures,
	id_projection_settings,
	id_uv_editor,
	id_uv_render,

	/*Influences menu*/
	id_joint_settings,
	id_joint_attach_verts,
	id_joint_100,
	id_joint_break_all,
	id_joint_color_weights,
	id_joint_weight_verts,
	id_joint_remove_bones,
	id_joint_remove_selection,
	id_joint_simplify,
	//These scream for a type mask system!!
	id_joint_select_bones_of,
	id_joint_select_verts_of,
	id_joint_select_points_of,
	id_joint_unnassigned_verts,
	id_joint_unnassigned_points,
	id_joint_draw_bone,
	id_joint_lock_bone,

	/*Animation menu*/	
	id_animate_settings,
	id_animate_render,
	id_animate_paste,
	id_animate_paste_v,
	id_animate_copy,
	id_animate_insert,
	id_animate_delete,
	//Additional
	id_animate_window,
	id_animate_play,
	id_animate_stop,
	id_animate_loop,
	id_animate_mode,
	id_animate_snap,
	id_clipboard_mode, //clipboard_mode
	id_animate_mode_1,
	id_animate_mode_2,
	id_animate_mode_3,
	//media controls
	id_animate_rew,
	id_animate_ffwd,
	id_animate_back,
	id_animate_fwd,
	id_animate_top,
	id_animate_end,
	id_animate_auto,
	id_animate_prev,
	id_animate_next,
	
	/*Help menu*/
	id_help,  //wxOSX needs to be Help.
	id_about,  //wxOSX needs to be About.
	id_config,
	id_keycfg,
	id_license,
	id_mouse_4,id_mouse_5,id_mouse_6,
	id_unscale,
	id_reorder,

	id_toolparams,
	id_properties,
	id_viewselect,
	id_fullscreen,
};

class ScrollWidget;
class GraphicWidget;
class TextureWidget;

struct Win : Widgets95::ui
{	
	struct : panel
	{
		virtual void _delete(){}

	}_main_panel;

	virtual void _delete() //override
	{
		if(!modal()&&!subpos()) delete this;
	}

	template<class T> struct Widget;

	typedef Widget<GraphicWidget> graphic; 
	typedef Widget<TextureWidget> texture;

	virtual ScrollWidget *widget() //interface
	{
		return nullptr;
	}

	Win(utf8,ScrollWidget*_=nullptr);
	Win(int,int subpos); //subwindow
	void _common_init(bool);
	
	static class Widgets95::e &event;

	static bool basic_special(ui*,int,int);
	static bool basic_keyboard(ui*,int,int);

	void basic_submit(int);

	struct ok_button : button
	{
		ok_button(node *p):button(p,"OK",id_ok){ ralign(); }
	};
	struct cancel_button : button
	{
		cancel_button(node*p):button(p,"Cancel",id_cancel){}
	};
	struct ok_cancel_panel
	{
		row nav;

		ok_button ok; cancel_button cancel;

		ok_cancel_panel(node *p):nav(p),ok(nav),cancel(nav)
		{
			nav.ralign();
		}
	};
	struct f1_titlebar : titlebar
	{	
		f1_titlebar(node *p); //win.cpp

		static utf8 msg(){ return "Press F1 for help"; }

		virtual bool activate(int how)
		{
			if(how==ACTIVATE_MOUSE) basic_special(ui(),1,0);
			return false;
		}
	};
	struct f1_ok_panel
	{
		row nav;

		f1_titlebar f1; ok_button ok;

		f1_ok_panel(node *p):nav(p),f1(nav),ok(nav)
		{
			nav.expand();
		}
	};
	struct f1_ok_cancel_panel
	{
		row nav;

		f1_titlebar f1; ok_cancel_panel ok_cancel;

		f1_ok_cancel_panel(node *p):nav(p),f1(nav),ok_cancel(nav)
		{
			nav.expand();
		}
	};

	struct zoom_set
	{
		zoom_set(node *frame, double min, double max)
			:
		nav(frame),
		value(nav,"",'='),
		in(nav,"",'+'),out(nav,"",'-')
		{
			nav.ralign().space(1);	
			in.picture(pics[pic_zoomin]);
			out.picture(pics[pic_zoomout]);
			value.edit(min,1.0,max);
			//value.spinner.set_speed(-0.00001);
			value.spinner.set_speed(-2.5/(max-min));
		}

		row nav;
		spinbox value;
		button in,out; //":/images/zoomin.xpm"
	};

	struct slider_value 
	{
		row nav; bar slider;
		
		textbox value; //spinbox value; //FIX ME

		void expand() //TESTING
		{
			value.ralign(); nav.expand(); slider.expand();
		}

		void set(int val)
		{
			slider.set_int_val(val); value.set_int_val(val);
		}
		void set(double val)
		{
			slider.set_float_val(val); value.set_float_val(val);
		}

		template<class T>
		void init(utf8 name, double min, double max, T val)
		{
			slider.spin(val).set_range(min,max); 
			value.edit(val).limit(min,max).name(name);
		}
		template<class T>
		slider_value(node *frame, utf8 name, double min, double max, T val, int id=id_value)
			:
		nav(frame),slider(nav,"",bar::horizontal,id,slider_cb),value(nav,"",id,value_cb)
		{
			init(name,min,max,val); slider.style(bar::sunken).space<top>(2);
		}

		static void slider_cb(bar *c)
		{
			((textbox*)c->next())->set_val((double)*c); c->ui()->active_callback(c);
		}
		static void value_cb(textbox *c)
		{
			((bar*)c->prev())->set_val((double)*c); c->ui()->active_callback(c);
		}
	};

	struct multisel_item : li::item
	{
		int m;

		multisel_item(int id, utf8 text):item(id,text),m()
		{}

		typedef void (*mscb)(int,multisel_item&);

		virtual int impl(int s)
		{
			int cmp = m; //2023

			if(~s&impl_multisel) switch(s)
			{
			case impl_features: return impl_multisel;
			}
			else switch(s&3)
			{
			case impl_clr: m&=~impl_multisel; break;
			case impl_set: m|=impl_multisel; break;
			case impl_xor: m^=impl_multisel; break;
			}

			if(cmp!=m)
			if(parent())
			if(mscb f=(mscb)list().user_cb)
			f(s,*this);

			return m;
		}
	};
	struct checkbox_item : multisel_item
	{
		using multisel_item::multisel_item; //C++11

		typedef void (*cbcb)(int,checkbox_item&);

		virtual int impl(int s)
		{
			if(~s&impl_checkbox) switch(s)
			{
			case impl_features:
				
				return impl_multisel|impl_checkbox;
			}
			else 
			{
				int cmp = m; switch(s&3)
				{
				case impl_clr: m&=~impl_checkbox; break;
				case impl_set: m|=impl_checkbox; break;
				case impl_xor: m^=impl_checkbox; break;
				}
				if(cmp!=m)
				if(parent())
				if(cbcb f=(cbcb)list().user_cb)
				f(s,*this);
			//	else assert((cbcb)f);
			}
			return multisel_item::impl(s);
		}
	};
	
	/* QMessageBox supplemental */
	static int InfoBox(utf8,utf8,int=id_ok,int=0);
	static int ErrorBox(utf8,utf8,int=id_ok,int=0);
	static int WarningBox(utf8,utf8,int=id_ok,int=0);	

	/* QFileDialog supplemental */
	typedef Widgets95::string FileBox;

	template<class T> /* QInputDialog supplemental */
	static int EditBox(T*,utf8,
	//GCC warning wants defaults in this declaration.
	utf8=nullptr,double=0,double=0,cb=cb());
};

struct EditWin : Win /* QInputDialog supplemental */
{
	void submit(control*);

	template<class T>
	//FIX ME: How to extend EditWin requires
	//more consideration. This is a big mess.
	//I changed how it works for AnimEditWin.
	EditWin(T *lv, utf8 t, utf8 l, double l1, double l2, cb v, bool init)
		:
	Win(t),swap(),valid(v),
	msg(main,"",false),
	nav(main),	
	edit(nav,"",lv),
	ok_cancel(main)
	{
		msg.set_text(l);

		if(l1!=l2) edit.limit(l1,l2);

		active_callback = &EditWin::submit; 

		if(init) submit(main); //id_init
	}

	void *swap; cb valid;

	wordproc msg;
	panel nav;	
	spinbox edit;
	ok_cancel_panel ok_cancel;
};
template<class T> /* QInputDialog supplemental */
inline int Win::EditBox(T *value, utf8 title, utf8 label,
double limit1, double limit2, cb valid)
{			
	//NOTE: This used to be much more, however I ended up
	//gutting it for AnimEditWin.
	return EditWin(value,title,label,limit1,limit2,valid,true)
	.return_on_close();
}

template<class T>
struct Win::Widget : T,T::Parent
{
	canvas &c;
	virtual void updateWidget(){ c.redraw(); }	
	Widget(canvas &c, bool border=true)
	:T(this),c(c)
	{
		uid = c.ui()->glut_window_id();

		assert(c.id()==id_scene);

		if(border) //REMOVE ME?
		{
			assert(!T::PAD_SIZE);
		//	int &p = T::PAD_SIZE;
		//	c.space(p,0,p,p,p); p = 0;
		//	c.space(-1,-1);
			c.space(1,0,1);
		//	p = 0;
			c.style(~0xf00);
		}
	}

	virtual void getXY(int &x, int &y)
	{
		x = c.x(x); y = c.y(y);

		//2022: TexcoordWin is deactivating
		//itself so Tab can be forwarded to
		//the main window via its new menus.
		if(!event.active_control_ui) return;

		//HACK: Steal focus like viewports?
		if(c!=event.get_active())
		if((unsigned)(x-c.x())<(unsigned)c.width())
		if((unsigned)(y-c.y())<(unsigned)c.height())
		{
			if(!event.curr_button_down) //Scrolling?
			c.activate();
		}
	}

	virtual bool mousePressSignal(int)
	{
		//2022: see getXY comment...
		if(!event.active_control_ui) return true;

		//Need to focus it so other controls 
		//don't consume arrow keys.
		if(c!=event.get_active()) c.activate(); return true;
	}
};

#endif //__WIN_H__
