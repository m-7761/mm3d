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


#ifndef __TEXWIDGET_H
#define __TEXWIDGET_H

#include "mm3dtypes.h"
#include "glheaders.h" //GLuint

class ScrollWidget //REFACTOR
{
public:

	int uid; //Some user data for setCursorUI, etc?

	//NOTE: mouseMoveEventUI uses this to implement
	//grab/drag/capture semantics.
	int m_activeButton;

	int m_viewportX;
	int m_viewportY;
	int m_viewportWidth;
	int m_viewportHeight;

	bool m_interactive;	
	bool m_uvSnap;

	double m_scroll[3];

	double m_zoom;
	double m_zoom_min,m_zoom_max; //2022

	double m_farOrtho,m_nearOrtho; //???
	
	enum ScrollButtonE
	{
		ScrollButtonPan,
		ScrollButtonLeft,
		ScrollButtonRight,
		ScrollButtonUp,
		ScrollButtonDown,
		ScrollButtonMAX
	};
	ScrollButtonE m_overlayButton;
	
	ScrollWidget(double zmin, double zmax);
	~ScrollWidget();

	void getGeometry(int &x, int &y, int &w, int &h)
	{
		x = m_viewportX; w = m_viewportWidth;
		y = m_viewportY; h = m_viewportHeight;		
	}

	bool over(int x, int y)
	{
		x-=m_viewportX; y-=m_viewportY;
		return x>=0&&y>=0&&x<m_viewportWidth&&y<m_viewportHeight;
	}
	
	void scrollUp(),scrollDown();
	void scrollLeft(),scrollRight();
	void rotateUp(),rotateDown();
	void rotateLeft(),rotateRight();																																																																																																																																																																																																																																																																																																																																																																																																														  
	void rotateClockwise(),rotateCounterClockwise();
	virtual void updateViewport(int how=0) = 0;
	virtual void rotateViewport(double,double,double){}

	void zoomIn(),zoomOut(),zoom(bool, double x,double y);

	void setZoomLevel(double zoom);

	double getZoomLevel(){ return m_zoom; }

	void initOverlay(GLuint[2]); //MOVE OUTSIDE (GLUT/Qt)
	void drawOverlay(GLuint[2]);
	void pressOverlayButton(bool rotate);
	bool pressOverlayButton(int x, int y, bool rotate);

	static void cancelOverlayButtonTimer(); //HACK
	
	//The UI implementation must implement these.
	bool mousePressEventUI(int bt, int bs, int x, int y);
	bool mouseReleaseEventUI(int bt, int bs, int x, int y);
	bool mouseMoveEventUI(int bs, int x, int y);
	bool wheelEventUI(int wh, int bs, int x=-1, int y=-1);
	bool keyPressEventUI(int bt, int bs, int x, int y);
	//It might make sense to upgrade this for Qt.
	void setTimerUI(int ms, void(*)(int), int);
	static int getElapsedTimeUI();	
	static void initTexturesUI(int, unsigned int[], char**[]);
	enum
	{
		//2022: Here are some other cursor 
		//values that don't fit the degrees
		//scheme: 0,45,90,135,180,225,270,360
		UI_ArrowCursor=-1,
		UI_IBeamCursor=1,
		UI_UpDownCursor,
		UI_LeftRightCursor,
	};
	void setCursorUI(int=-1);

protected:

	//virtual makes these easier to hook up to a UI implementation.
	virtual void getXY(int&,int&) = 0;
	virtual bool mousePressEvent(int bt, int bs, int x, int y) = 0;
	virtual void mouseReleaseEvent(int bt, int bs, int x, int y) = 0;
	virtual void mouseMoveEvent(int bs, int x, int y) = 0;
	virtual void wheelEvent(int wh, int bs, int x, int y) = 0;
	virtual bool keyPressEvent(int bt, int bs, int x, int y) = 0;
};

class TextureWidget : public ScrollWidget
{
public:
	
	static constexpr double zoom_min = 0.01;
	static constexpr double zoom_max = 250000;
	
	int PAD_SIZE = 0; //6
	
	class Parent //Qt supplemental //REMOVE ME?
	{
	public:
		
		//2022: Bridging Tool::Parent::_bs_lock?
		int _tool_bs_lock = 0;

		virtual void updateWidget() = 0;

		virtual void getXY(int&,int&) = 0;

		virtual void zoomLevelChangedSignal(){}

		//NEW: I am using this to deactivate elements
		//that take arrow keys.
		virtual bool mousePressSignal(int){ return true; }

		//NOTE: The coordinates editor uses this.
		virtual void updateCoordinatesSignal(){}
		virtual void updateSelectionDoneSignal(){}
		virtual void updateCoordinatesDoneSignal(){}

		//NOTE: The projections editor uses this.
		virtual void updateRangeSignal(){}
		virtual void updateRangeDoneSignal(){}
		virtual void updateSeamSignal(double xDiff, double yDiff){}
		virtual void updateSeamDoneSignal(double xDiff, double yDiff){}

	}*const parent;
		
	void sizeOverride(int width, int height); //TextureFrame

	void draw(int frameX, int frameY, int width, int height);  //NEW	
		
	//ScrollWidget methods
	virtual void updateViewport(int remove_me=0);
	//Note: These Qt events are now made to take
	//Tool::ButtonState values to be independent.
	virtual void getXY(int &x, int &y){ parent->getXY(x,y); }
	virtual bool mousePressEvent(int bt, int bs, int x, int y);
	virtual void mouseReleaseEvent(int bt, int bs, int x, int y);
	virtual void mouseMoveEvent(int bs, int x, int y);
	virtual void wheelEvent(int wh, int bs, int x, int y);
	virtual bool keyPressEvent(int bt, int bs, int x, int y);
	
public:

	enum DrawModeE
	{
		DM_Edit,
		DM_Edges,
		DM_Filled,
		DM_FilledEdges,
		DM_MAX
	};

	enum MouseE
	{
		MouseNone, //pan
		MouseMove,
		MouseSelect,
		MouseScale,
		MouseRotate,
		MouseRange, //Requires setRange (projectionwin.cc)
	};

	TextureWidget(Parent*);

	void setModel(Model *model);
	void setTexture(int materialId=-1, Texture *texture=nullptr);
	void initTexture();
	void freeTexture() //UNUSED
	{
		//Removing ~TextureWidget since it's hard to coordinate
		//this with OpenGL context switching.
		glDeleteTextures(1,&m_glTexture);
		m_glTexture = 0;
	}
	void freeOverlay() //UNUSED
	{
		//Removing ~TextureWidget since it's hard to coordinate
		//this with OpenGL context switching.
		glDeleteTextures(1,m_scrollTextures); 
		m_scrollTextures[0] = 0;
	}

	void setInteractive(bool o){ m_interactive = o; }
	void set3d(bool o);

	void setUvSnap(bool o){ m_uvSnap = o; } //2022
	void snap(double &s, double &t, bool selected); 

	void setSClamp(bool o){ m_sClamp = o; setTexture(m_materialId,m_texture); }
	void setTClamp(bool o){ m_tClamp = o; setTexture(m_materialId,m_texture); }

	void setDrawMode(DrawModeE dm){ m_drawMode = dm; }
	void setDrawVertices(bool dv){ m_drawVertices = dv; }
	void setDrawUvBorder(bool db){ m_drawBorder = db; }
	void setMouseOperation(MouseE op){ m_operation = op; }

	void setScaleKeepAspect(bool o){ m_scaleKeepAspect = o; }
	void setScaleFromCenter(bool o){ m_scaleFromCenter = o; }
	void setSolidBackground(bool o){ m_solidBackground = o; }

	void uFlipCoordinates(),vFlipCoordinates();
	void rotateCoordinatesCcw(),rotateCoordinatesCw();
	void uFlattenCoordinates(),vFlattenCoordinates();

	void setLinesColor(int newColor){ m_linesColor = newColor; }
	void setSelectionColor(int newColor){ m_selectionColor = newColor; }

	void addVertex(double s, double t, bool select=true);
	void addTriangle(int v1, int v2, int v3);

	void clearCoordinates();
	void getCoordinates(int tri, float *s, float *t);

	void saveSelectedUv();
	void restoreSelectedUv();

	void setRange(double xMin, double yMin, double xMax, double yMax);
	void getRange(double &xMin, double &yMin, double &xMax, double &yMax);
		
	int getUvWidth(),getUvHeight();

protected:

	void moveSelectedVertices(double x, double y);
	void updateSelectRegion(double x, double y);
	void startScale(double x, double y);
	void rotateSelectedVertices(double angle);
	void scaleSelectedVertices(double x, double y);
	
	void startSeam(){ m_xSeamAccum = m_ySeamAccum = 0; } //EXPERIMENTAL
	void accumSeam(double x, double y){ m_xSeamAccum+=x; m_ySeamAccum+=y; }

	void drawTriangles();

	void selectDone(int snap_select=0);

	double getWindowSCoord(int x){ return x/m_width*m_zoom+m_xMin; }
	double getWindowTCoord(int y){ return y/m_height*m_zoom+m_yMin; }
	
	double getWindowSDelta(int x, int xx){ return (x-xx)/m_width*m_zoom; }
	double getWindowTDelta(int y, int yy){ return (y-yy)/m_height*m_zoom; }

	//These have to do with MouseRange so I'm renaming them accordingly.
	//void getDragDirections
	int getRangeDirection(double,double,bool=false);
	//void updateCursorShape
	void setRangeCursor(int x, int y);

	void useLinesColor();
	void useSelectionColor();

	GLuint m_scrollTextures[2];

	bool m_sClamp,m_tClamp;

	Model *m_model;

	int m_materialId;
	Texture *m_texture;
	GLuint m_glTexture;
	
	struct TextureVertexT
	{
		double s,t;
		bool selected;
	};
	struct TextureTriangleT
	{
		int vertex[3];
	};		
	struct TransVertexT
	{
		int index; double x,y;
	};
	std::vector<TextureVertexT> m_vertices;
	std::vector<TextureTriangleT> m_triangles;
	std::vector<TransVertexT> m_operations_verts;	

	DrawModeE m_drawMode;
	bool m_drawVertices;
	bool m_drawBorder;
	bool m_solidBackground;

	MouseE m_operation;

	bool m_scaleKeepAspect;
	bool m_scaleFromCenter;

	bool m_selecting;
	bool m_drawBounding;
	bool m_3d;

	int m_buttons;

	double m_xMin,m_xMax;
	double m_yMin,m_yMax;	

	//NEW: These let the entire area be used so that
	//the UI layouts are consistent.
	double m_width,m_height,m_aspect;
	int m_x,m_y;

	//2022: For snapping?
	double m_s,m_t;

	// For move and scale (and pan)
	int m_constrain;
	int m_constrainX,m_constrainY;

	// For select
	double m_xSel1;
	double m_ySel1;
	double m_xSel2;
	double m_ySel2;
		
	// For rotation
	double m_xRotPoint;
	double m_yRotPoint;
	double m_xRotStart;
	double m_yRotStart;
	double m_startAngle;

	// For projection range
	double m_xRangeMin;
	double m_yRangeMin;
	double m_xRangeMax;
	double m_yRangeMax;

	double m_xSeamAccum;
	double m_ySeamAccum;

	// For projection move/resize
	bool m_dragAll;
	bool m_dragTop;
	bool m_dragBottom;
	bool m_dragLeft;
	bool m_dragRight;
		
	double m_farX;
	double m_farY;
	double m_centerX;
	double m_centerY;
	double m_startLengthX;
	double m_startLengthY;

	int m_linesColor;
	int m_selectionColor;

	//TextureFrame::sizeOverride remnants.
	int m_overrideWidth, m_overrideHeight; 
};

#endif // __TEXWIDGET_H
