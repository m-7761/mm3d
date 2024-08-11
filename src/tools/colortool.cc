/*  2024 MIT License */

#include "mm3dtypes.h" //PCH

#include "tool.h"

#include "pixmap/colortool.xpm"

#include "model.h"
#include "msg.h"
#include "modelstatus.h"

struct ColorTool : Tool
{
	ColorTool():Tool(TT_ColorTool),m_nib(8)
	{
		m_jts = m_pts = true; m_bg = false; m_wts = 0; m_sat = 0.05;
	}

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Tool","Color");
	}

	virtual const char **getPixmap(int){ return colortool_xpm; }

	virtual const char *getKeymap(int){ return ";"; } //Shift+C

	virtual void activated(int)
	{
		Model *model = parent->getModel();
		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Tip: Hold Ctrl+Alt to zoom paint nib"));

		parent->addBool(true,&m_jts,TRANSLATE_NOOP("Param","Joints"));		
		parent->groupParam();
		parent->addBool(true,&m_pts,TRANSLATE_NOOP("Param","Points"));
		parent->groupParam();
		parent->addBool(false,&m_bg,TRANSLATE_NOOP("Param","Views"));
	//	parent->groupParam();

		const char *e[4+1] = 
		{
		TRANSLATE("Param","<None>",""),
		//TRANSLATE("Param","Facet",""), //centroid mode?
		TRANSLATE("Param","Vertex",""),
		TRANSLATE("Param","Polygon",""),		
		TRANSLATE("Param","Influences",""),		
		};
		m_wts = parent->getModel()->getViewportColorsMode();
		parent->addEnum(false,&m_wts,TRANSLATE_NOOP("Param","Colors"),e);
		parent->groupParam();
		parent->addButton(0,this,m_bts,TRANSLATE_NOOP("Param","..."));		
	//	parent->groupParam();	
		parent->addDouble(true,&m_sat,TRANSLATE_NOOP("Param","Blend"),0,1);
	}
	virtual void updateParam(void *p)
	{
		if(p==&m_wts) parent->getModel()->getViewportColorsMode() = m_wts;
	}
	
	virtual void mouseButtonDown();
	virtual void mouseButtonMove();
	virtual void mouseButtonUp();

	static void m_bts(Tool *me, int bt)
	{
		me->parent->command(Parent::cmd_open_color_window);
	}

	virtual void draw(bool focused)
	{
		if(!focused) return;

		double x,y;
		parent->getRawParentXYValue(x,y);

		const Matrix &inv = parent->getParentProjInverseMatrix();		
		double zoom = parent->_getParentZoom();

		glEnable(GL_COLOR_LOGIC_OP);
		glColor3ub(0x80,0x80,0x80);
		glLogicOp(GL_XOR);
		for(int pass=1;pass<=2;pass++)
		{
			int n = pass==1?32:16;
			double r = radius(pass);

			glLineWidth(pass==1?2.0f:1.0f);
			glBegin(GL_LINE_STRIP);
		
			for(int i=n;i-->-1;)
			{
				double c = cos(PI*2*i/n);
				double s = sin(PI*2*i/n);

				double p[4] = {x+r*c,y+r*s,0,1};			
				//BLACK MAGIC for avoiding near clip			
				if(zoom>1) 
				{
					for(int i=4;i-->0;) p[i]*=zoom; p[2] = -zoom;
				}
				inv.apply4(p); glVertex4dv(p);
			}
			glEnd();
			//glLineWidth(1.0f);
		}
		glDisable(GL_COLOR_LOGIC_OP);
	}

	virtual void sizeNib(int delta)
	{
		m_nib = std::max(1,std::min(25,m_nib+delta));
	}

	double radius(int pass=1)
	{
		double r = 1.0/(pass==1?m_nib:32);
		return r*parent->_getParentZoom();
	}

	bool m_jts,m_pts,m_bg; 
	
	int m_wts; 
	
	double m_sat;
	
	int m_nib; 
	
	bool hit, moved; int joint;
};

extern Tool *colortool(){ return new ColorTool; }

void ColorTool::mouseButtonDown()
{
	Model *model = parent->getModel();
	model_status(model,StatusNormal,STATUSTIME_SHORT,
	TRANSLATE("Tool","Tip: Hold Ctrl+Alt to zoom paint nib"));

	joint = -1;
	int io = m_jts|m_pts<<1; //1|2
	moved = false;
	if(hit=parent->applyColor(radius(),m_sat,m_wts,io))
	{
		//parent->updateAllViews();

		if(m_wts==3) joint = io;
	}
	else if(m_bg)
	{
		parent->setBackgroundColor(m_sat);
	}

	parent->updateAllViews(); //draw?
}
void ColorTool::mouseButtonMove()
{
	int io = m_jts|m_pts<<1; //1|2
	moved = true;
	if((!m_bg||hit)&&parent->applyColor(radius(),m_sat,m_wts,io))
	{
		//parent->updateAllViews();
	}
	else if(m_bg)
	{
		parent->setBackgroundColor(m_sat);
	}

	parent->updateAllViews(); //draw?
}
void ColorTool::mouseButtonUp()
{
	if(hit&&!moved&&joint!=-1)
	{
		int bts = parent->getButtons();

		int bt = 0;
		if(~bts&Tool::BS_Left)			
		if(bts&Tool::BS_Right) bt = 1;

		parent->command(Parent::cmd_open_color_window_select_joint,bt,joint);
	}
	else if(!hit&&!moved&&m_bg)
	{
		if('Y'==msg_info_prompt("Would you like to set the View background color?","Yn"))
		{
			parent->setBackgroundColor(1.0f);
		}
		else parent->setBackgroundColor(-m_sat);
	}

	parent->updateAllViews(); //draw?
}