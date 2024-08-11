/* 2024 MIT License */

#ifndef __INFLUENCEWIN_H__
#define __INFLUENCEWIN_H__

#include "mm3dtypes.h" //PCH
#include "win.h"
#include "model.h"
#include "texwidget.h"

struct ColorWin : Win
{
	void submit(control*);
	
	ColorWin(class MainWin &model)
		:
	Win("Color Material"),
	model(model),	
		void1(main),
	row1(main),row2(main),
		void2(main),
	close(main,"Close",id_close),	
		void3(main),
	row3(main),row4(main),
		void4(main),
	col1(main),
		tab(main,id_tab),
	v1(row1,row3,tab,"1"),
	v2(row1,row3,tab,"2"),
	v3(row1,row3,tab,"3"),
	v4(row2,row4,tab,"4"),
	v5(row2,row4,tab,"5"),
	v6(row2,row4,tab,"6")
	{
		cur = 0;

		row1.calign(); row2.calign();
		row3.calign(); row4.calign();

		active_callback = &ColorWin::submit;

		submit(main);
	}

	class MainWin &model;

	struct Infl : panel
	{
		unsigned vp,vc;

		dropdown color,joint;
		
		unsigned color_custom;
		unsigned custom_color()
		{
			unsigned c = (int)color;
			return c?c:color_custom|0xff000000;
		}

		Infl(panel &p, utf8 name, int vp, int vc)
			:
		vp(vp),vc(vc),
		panel(p),color(this,name,'vc'),joint(this,"",'jt')
		{
			color_custom = 0;
			
			expand(); 
			
			color.lock(150,false);
			joint.lock(150,false);
		}

		void swap(Infl &b);

		Infl *pair();
	};	
	struct View
	{
		unsigned vp;
		View(row &r, row &r2, node *main, utf8 name)
			:
		vp(name[0]-'1'),
		tool(r,name,name[0]),
		pair(r,"1",name[0]-'1'+'a'),
		viewport(main),
		primary(viewport,"Primary\t",vp,0),
		row1(viewport),
		lock(row1,"Lock",'lock'),swap(row1,"Swap",'swap'),
		mouse4(viewport,"Mouse 4\t",vp,2),
		col1(viewport),
		secondary(viewport,"Secondary\t",vp,1),
		row2(viewport),
		lock2(row2,"Lock",'lock'),swap2(row2,"Swap",'swap'),
		mouse5(viewport,"Mouse 5\t",vp,3)
		{
			tool.push().span(1);
			pair.push().span(1);

			row1.proportion();	
			lock.push();

			row2.proportion();
			lock2.push();
		}

		boolean tool;
		boolean pair;
		panel viewport;		
		Infl primary; 
		row row1;
		boolean lock;
		button swap;		
		Infl mouse4;		

		column col1;
		Infl secondary;
		row row2;
		boolean lock2;
		button swap2;
		Infl mouse5;
	};

	void layoutChanged();
	void jointsChanged();
	void open();

	void setModel();

	void setJoint(int,int);

	canvas void1;
	row row1,row2;
	canvas void2;
	button close;
	canvas void3;
	row row3,row4;
	canvas void4;
	column col1;
	ui::tab tab;
	View v1,v2,v3,v4,v5,v6;

	int cur;
	control *_first_tab;
	void _rename_tabs();
	View*const vp = &v1;
};

#endif //__TEXTURECOORD_H__
