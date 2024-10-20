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


#ifndef __MVIEWPORT_H
#define __MVIEWPORT_H

#include "texwidget.h" //ScrollWidget
#include "tool.h" //Tool::Parent

class ModelViewport : public ScrollWidget
{				
public:

	static constexpr double zoom_min = 0.08;
	static constexpr double zoom_max = 250000;

	class Parent;

	Parent *const parent;

	ModelViewport(Parent*);

	void draw(int frameX, int frameY, int width, int height, int vc=0);  //NEW	

	void render() //animexportwin
	{
		m_rendering = true; draw(0,0,0,0);
		m_rendering = false;
	}

	//Note: These Qt events are now made to take
	//Tool::ButtonState values to be independent.
	virtual void getXY(int &x, int &y);
	static bool _rmb_rotates_view; //2021: fix me?
	virtual bool mousePressEvent(int bt, int bs, int x, int y);
	virtual void mouseReleaseEvent(int bt, int bs, int x, int y);
	virtual void mouseMoveEvent(int bs, int x, int y);
	virtual void wheelEvent(int wh, int bs, int x, int y);
	virtual bool keyPressEvent(int bt, int bs, int x, int y);
		
	/*REFERENCE
	static int constructButtonState(QMouseEvent *e)
	{
		int button = 0;
		if(e->modifiers()&Qt::ShiftModifier)
		button|=Tool::BS_Shift;
		if(e->modifiers()&Qt::AltModifier)
		button|=Tool::BS_Alt;
		if(e->modifiers()&Qt::ControlModifier)
		button|=Tool::BS_Ctrl;
		switch(m_activeButton)
		{
		case Qt::LeftButton: return button|Tool::BS_Left;
		case Qt::MidButton: return button|Tool::BS_Middle;
		case Qt::RightButton: return button|Tool::BS_Right;
		}
		return button;
	}*/

public:

	//NOTE: These are Model::m_canvasDrawMode
	//and Model::m_perspectiveDrawMode.
	enum ViewOptionsE
	{
		ViewWireframe,
		ViewFlat,
		ViewSmooth,
		ViewTexture,
		ViewAlpha,	
	};
	enum ViewColorsModE
	{
		ViewVColors=16,
		ViewInflColors=32,		
	};

	enum MouseE
	{
		MO_None,
		MO_Tool,
		MO_Pan,
		MO_Rotate,
	};

	struct ViewStateT
	{
		Tool::ViewE direction;
		int layer;
		double zoom;
		double rotation[3];
		double translation[3];
		void copy_transformation(ViewStateT &cp)
		{
			memcpy(rotation,cp.rotation,3+3);
		}

		operator bool(){ return zoom!=0; }
		ViewStateT(){ memset(this,0x00,sizeof(*this)); }
	};

	void frameArea(bool lock, double x1, double y1, double z1, double x2, double y2, double z2);
	
	bool getParentCoords(int bs, int x, int y, double coords[4], bool selected);

	void getRawParentXYValue(int x, int y, double &xval, double &yval)
	{
		xval = x/(double)m_viewportWidth*m_width-m_width/2;
		yval = y/(double)m_viewportHeight*m_height-m_height/2;
	}

public:

	//RENAME ME
	void viewChangeEvent(Tool::ViewE dir);
	void setViewState(const ModelViewport::ViewStateT &viewState);
	void getViewState(ModelViewport::ViewStateT &viewState);

	Tool::ViewE getView(){ return m_view; }

	void setLayer(int layer);

	int getLayer(){ return m_layer; }

protected:
	
	friend class Parent;

	//ScrollWidget methods
	virtual void updateViewport(int=0); 
	virtual void rotateViewport(double x, double y, double z=0);

	void updateMatrix(); //NEW
	
	bool updateBackground();
	
	void drawGridLines(float a=1, bool offset3d=false);
	void drawBackground();
	void freeBackground() //UNUSED
	{
		//Removing ~ModelViewport since it's hard to coordinate
		//this with OpenGL context switching.
		glDeleteTextures(1,&m_backgroundTexture);
		m_backgroundTexture = 0;
	}

	double getUnitWidth();

	bool applyColor(int v, int bs, int x, int y, double r, double press, int vmode, int &iomode);

	MouseE m_operation;

	Tool::ViewE m_view;

	int m_layer; //2022

	//2019: m_projMatrix do perspective correct selection.
	//Ideally this matrix would be used by all tools, but
	//other tools must use the ortho matrix for right now.
	Matrix m_viewMatrix,m_viewInverse;
	Matrix m_projMatrix,m_unprojMatrix;
	//2020: These are the same as m_viewMatrix except the
	//scrolling along Z is zeroed out so they behave well
	//in free-rotation modes. I think in the future they
	//should be able to be arbitrary construction planes.
	//See getParentBestMatrix/getParentBestInverseMatrix.
	Matrix m_bestMatrix,m_bestInverse;

	double m_rotX;
	double m_rotY;
	double m_rotZ;
	double m_width;
	double m_height;
	double m_unitWidth;

	Texture *m_background;
	GLuint m_backgroundTexture;
	std::string m_backgroundFile;

	bool m_rendering; //animexportwin

	//REMOVE ME?
	std::array<int,2> m_scrollStartPosition; //QPoint
};

//WORK-IN-PROGRESS
class ModelViewport::Parent : public Tool::Parent
{
	//This class hoists a lot of code that does
	//not depend on how the UI is realized from
	//viewpanel.cc.

	friend class ModelViewport;

protected:

	Parent();

	bool background_grid[2];

	static void initializeGL(Model*),checkGlErrors(Model*);

	virtual bool mouseIsPressed(bool force)
	{
		for(int i=0;i<portsN;i++) if(ports[i].m_activeButton)
		{
			if(!force) return true;

			ports[i].mouseReleaseEvent(ports[i].m_activeButton,_bs,_bx,_by);
		}		
		return false;
	}

public:

	//NOTE: lock is new
	void frameArea(bool lock, double x1, double y1, double z1, double x2, double y2, double z2)
	{
		for(int i=0;i<viewsN;i++) ports[i].frameArea(lock,x1,y1,z1,x2,y2,z2);
	}

public: //slots:

	virtual void getXY(int&,int&) = 0;

	//TODO: MERGE THESE INTO viewStateChangeEvent
	//THAT COVERS EVERYTHING STORED IN ViewStateT.
	virtual void viewChangeEvent(ModelViewport&) = 0;
	virtual void zoomLevelChangedEvent(ModelViewport&) = 0;	

	void viewportSaveStateEvent(int,const ModelViewport::ViewStateT&);
	void viewportRecallStateEvent(ModelViewport&,int);

//protected:
		
	//NOTE: Misfit has 3x3 but for good
	//grief that's impractical
	enum{ portsN=3*2, portsN_1=portsN+1 };
	ModelViewport ports[portsN_1];

	bool views1x2;
	int viewsM;
	int viewsN;
	//ViewBar::ModelView *views[portsN_1];	

	GLuint m_scrollTextures[2]; //REMOVE ME	
	void freeOverlay() //UNUSED
	{
		//Removing ~Parent since it's hard to coordinate
		//this with OpenGL context switching.
		glDeleteTextures(1,m_scrollTextures);
		m_scrollTextures[0] = 0;
	}

	//2022: m_click is added for determining new face normals.
	//It means last clicked, so that moving the mouse up to the
	//system menu to choose a command doesn't switch mouse focus.
	int m_entry,m_focus,m_click; 
	
	//NOTE: This is used with Ctrl+1 through 9 to save/recall
	//states by viewportSaveStateEvent/viewportRecallStateEvent.
	enum{ user_statesN='9'-'1'+1 };
	ModelViewport::ViewStateT user_states[user_statesN];
		
		/*ModelViewport imports*/

	inline void drawTool(ModelViewport *p)
	{
		int swap = m_focus;
		m_focus = p-ports; tool->draw(swap==m_focus);
		m_focus = swap;
	}

	// Tool::Parent methods
	virtual Tool::ViewE getView()
	{
		return ports[m_click].m_view; //m_focus
	}
	virtual bool getParentCoords(double coords[4], bool selected)
	{
		return ports[m_click].getParentCoords(_bs,_bx,_by,coords,selected);
	}
	virtual void getRawParentXYValue(double &xval, double &yval)
	{
		ports[m_click].getRawParentXYValue(_bx,_by,xval,yval);
	}
	virtual const Matrix &getParentViewMatrix()const
	{
		return ports[m_click].m_viewMatrix; 
	}
	virtual const Matrix &getParentViewInverseMatrix()const
	{
		return ports[m_click].m_viewInverse; 
	}
	virtual const Matrix &getParentProjMatrix()const
	{
		return ports[m_click].m_projMatrix; 
	}
	virtual const Matrix &getParentProjInverseMatrix()const
	{
		return ports[m_click].m_unprojMatrix;
	}
	virtual double _getParentZoom()const //TESTING
	{
		return ports[m_click].m_zoom;
	} 
	virtual void getXYZ(double*,double*,double*);
		
	virtual unsigned getPrimaryLayer()
	{
		return ports[m_focus].m_layer; 
	}

	int colors[portsN_1];

	virtual bool applyColor(double r, double x, int vmode, int &iomode)
	{
		return ports[m_click].applyColor(colors[m_click],_bs,_bx,_by,r,x,vmode,iomode);
	}
};

#endif // __MVIEWPORT_H
