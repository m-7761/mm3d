/* MM3D (Multimedia 3D)
 *
 * This code is an original contribution not wishing to 
 * be subjected to any form of copyright based thoughts.
 * 
 */

#ifndef __MM3DCORE__GRAPHS_H__
#define __MM3DCORE__GRAPHS_H__

#include "texwidget.h" //ScrollWidget

#include "model.h" //Div?

  //2022: WHAT'S THIS? 
  //
  // This class is inspired by TextureWidget and
  // less so ModelViewport and is originally for
  // displaying and editing animation parameters.
  // These require curve plots and some labeling.

class GraphicWidget : public ScrollWidget 
	,
private ModelObserver //Model::Observer
{
	virtual void modelChanged(int); //Observer

public:

	static constexpr double zoom_min = 0.02;
	static constexpr double zoom_max = 2.0;

	int PAD_SIZE = 0; //6
	
	class Parent //Qt supplemental //REMOVE ME?
	{
	public:

		unsigned char palette[8] = {0,32,64,96,128,160,192,224};
		void _color3ub(unsigned char i);

		//2022: Bridging Tool::Parent::_bs_lock?
		int _tool_bs_lock = 0;

		int label_height = 20; //ARBITRARY

		//HACK: getLabel is like getXY for converting to the label
		//coordinate system. This is really used to draw the lines
		//with GL_LINE_SMOOTH since it needs to be in raster space.
		virtual void getLabel(int &x, int &y) = 0;
		virtual int sizeLabel(const char*) = 0;		
		virtual void drawLabel(int x, int y, const char*, int bold) = 0;

		virtual void updateWidget() = 0;

		virtual void getXY(int&,int&) = 0;

		virtual void zoomLevelChangedSignal(){}

		virtual void updateCoordinatesSignal(){}
		virtual void updateSelectionDoneSignal(){}
		virtual void updateCoordinatesDoneSignal(){}

		//NEW: I am using this to deactivate elements
		//that take arrow keys.
		virtual bool mousePressSignal(int){ return true; }

	}*const parent;

	//Line up the view so the current frame is at
	//x.
	//x is in the same space as draw's x argument.
	void scrollAnimationFrameToPixel(int x);

	void draw(int x, int y, int width, int height);	
			
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

	enum MouseE
	{
		MouseNone, //pan
		MouseMove,
		MouseSelect,
		MouseScale,
	};
	enum SelectE
	{
		Select=0,
		SelectFrame=1,
		SelectVertex=2,
	};

	GraphicWidget(Parent*),~GraphicWidget();

	void setModel(Model *model);
	void freeOverlay() //UNUSED
	{
		//Removing ~TextureWidget since it's hard to coordinate
		//this with OpenGL context switching.
		glDeleteTextures(1,m_scrollTextures); 
		m_scrollTextures[0] = 0;
	}

	void setMouseOperation(MouseE op){ m_operation = op; }
	void setMouseSelection(SelectE op){ m_selection = op; }

	void setMoveToFrame(bool o){ m_moveToFrame = o; }
	void setScaleFromCenter(bool o){ m_scaleFromCenter = o; }

	//WARNING: This can't be used if the mouse is dragging.
	void moveSelectedFrames(double time);

	Model::Animation *animation();
		
	struct SplinePT
	{
		double s,t; int x,y;

		const Model::Keyframe *key;
	};
	struct TransFrameT
	{
		unsigned index; double time;
	};

protected:
	
	void moveSelectedVertices(double x, double y);
	void updateSelectRegion(double x, double y);
	void startScale(double x, double y);
	void scaleSelectedVertices(double x, double y);
	void selectDone(int snap_select=0);
	void clearSelected();
	void dragRange(int x, int y, int bt);

	double getWindowSCoord(int x){ return x/m_width*m_zoom+m_xMin; }
	double getWindowTCoord(int y){ return y/m_height*m_zoom+m_yMin; }

	double getWindowX(double s){ return (s-m_xMin)/m_zoom*m_width; }
	double getWindowY(double t){ return (t-m_yMin)/m_zoom*m_height; }
	
	double getWindowSDelta(int x, int xx){ return (x-xx)/m_width*m_zoom; }
	double getWindowTDelta(int y, int yy){ return (y-yy)/m_height*m_zoom; }

	//These are for lining up with GL_LINE_SMOOTH.
	double getWindowSCoordRoundedToPixel(double t)
	{
		return getWindowSCoord((int)round(getWindowX(t))); //YUCK 
	}
	double getWindowTCoordRoundedToPixel(double t)
	{
		return getWindowTCoord((int)round(getWindowY(t))); //YUCK 
	}

	enum{ m_interactive=true };

	GLuint m_scrollTextures[2];

	Model *m_model;

	/*UNFINISHED
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
	std::vector<TransVertexT> m_operations_verts;*/

	std::vector<SplinePT> m_graph_splines;	
	std::vector<TransFrameT> m_trans_frames;
		
	struct Div
	{
		Model::Position pos;

		const Model::Object2020 *obj;

		double div;

		Div(double d):div(d),obj(){}

		Model::KeyframeGraph &ref()
		{
			if(pos.type==Model::PT_Joint)
			return ((Model::Joint*)obj)->_reference;
			return ((Model::Point*)obj)->_reference;
		}
	};

	double m_autoSize;
	std::vector<Div> m_divs;
	double m_divsPadding;
	size_t m_div;	
	double m_divDragSize;
	int m_divDragY;

	MouseE m_operation;
	SelectE m_selection;

	bool m_moveToFrame;

	bool m_scaleKeepAspect; //???
	bool m_scaleFromCenter;

	bool m_selecting;
	bool m_drawBounding;

	int m_buttons;
	
	double m_xMin,m_xMax;
	double m_yMin,m_yMax;	

	//NEW: These let the entire area be used so that
	//the UI layouts are consistent.
	double m_width,m_height,m_asymmetry;
	int m_x,m_y;

	//2022: For snapping?
	double m_s,m_t;

	double m_accum;

	// For move and scale (and pan)
	int m_constrain;
	int m_constrainX,m_constrainY;
	
	// For select
	double m_xSel1;
	double m_ySel1;
	double m_xSel2;
	double m_ySel2;

	double m_farX;
	double m_farY;
	double m_centerX;
	double m_centerY;
	double m_startLengthX;
	double m_startLengthY;
};

#endif // __MM3DCORE__GRAPHS_H__
