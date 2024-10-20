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


#ifndef __TOOL_H
#define __TOOL_H

#include "mm3dtypes.h"
#include "glmath.h"
#include "model.h"
#include "translate.h"

// The tool interface allows a Tool::Parent to notify a Tool that it is currently
// being used on the Model.
//
// The Tool receive mouses events such as a press,release,or drag (moving 
// mouse with button down).  The button state indicates which buttons
// are down,as well as which keyboard modifiers are in effect.
//
// Note that commands (non-interactive model operations)are much easier to
// implement.  There are also many similarities in how they are used by 
// Maverick Model 3D.  You should see the documentation in command.h before 
// reading this.
//
// Note that middle mouse button events are used by ModelViewport and
// never passed to the Tool.  The middle mouse button in the button
// state is provided in the unlikely event that this behavior changes.
//

class Tool
{
public:
	
	class Parent;

	//NOTE: This was a decommissioned submenu system.
	//It's repurposed for two reasons:
	//1) SelectTool uses the view+proj matrix.
	//2) Trying to shape the toolbar so that default
	//shortcuts line up with Querty keyboards.
	enum ToolType
	{
		TT_NullTool=0,
		TT_Separator,
		TT_Other, //TT_Manipulator,
		TT_Creator,
		TT_SelectTool,
		TT_ColorTool,

		//HACK: These are for synchronizing tools 
		//with the texture and animation systems.
		TT_MoveTool,
		TT_ScaleTool,
		TT_RotateTool,
	};

	const ToolType m_tooltype;	

	const int m_args;

	const char *const m_path;

	Parent *const parent;

	static bool tool_option_face_view; //EXPERIMENTAL

	Tool(ToolType, int count=1, const char *path=nullptr);
	virtual ~Tool();

	int getToolCount(){ return m_args; }
	
	// These functions are used in a manner similar to commands.  
	// See command.h for details.	
	const char *getPath(){ return m_path; }
	
	// Is this a place-holder for a menu separator?
	bool isSeparator(){ return !m_args; }

	bool isNullTool(){ return m_tooltype==TT_NullTool; }

	//TEMPORARY
	//This is just to use a projection matrix for with a
	//perspective view port.
	bool isSelectTool(){ return m_tooltype==TT_SelectTool; }

	//This enables vertex colors or influence visualization.
	bool isColorTool(){ return m_tooltype==TT_ColorTool; }

	//UNUSED
	// Does this tool create new primitives?
	//bool isCreation(){ return m_tooltype==TT_Creator; }
	bool isCreateTool(){ return m_tooltype==TT_Creator; }

	//UNUSED
	// Does this tool manipulate existing (selected primitives)?
	//bool isManipulation(){ return m_tooltype==TT_Manipulator; }
		
	// It is a good idea to override this if you implement
	// a tool as a plugin.
	virtual void release(){ delete this; }
	
	virtual const char *getName(int arg) = 0;

	// This returns the pixmap that appears in the toolbar
	virtual const char **getPixmap(int arg) = 0;

	// Replace keycfg.cc
	virtual const char *getKeymap(int arg){ return ""; }

	virtual void activated(int arg){}
	virtual void deactivated(int arg){}
	virtual void updateParam(void*){}
	virtual void modelChanged(int changeBits){}
	
	// NOTE: You will never receive a middle button event
	enum ButtonState
	{
		BS_Left       = 0x001,
		BS_Middle     = 0x002, //BS_Up
		BS_Right      = 0x004,
		BS_Shift      = 0x008,
		BS_Alt        = 0x010,
		BS_Ctrl	      = 0x020,

		/*UNSUED
		BS_CapsLock	  = 0x040,
		BS_NumLock	  = 0x080,
		BS_ScrollLock = 0x100,*/

		BS_Left_Middle_Right = BS_Left|BS_Middle|BS_Right, //NEW

			/* Qt/GLUT supplemental */

		//NEW: Arrow keys for TextureWidget and ModelViewport events.
		BS_Up = 0x002, BS_Down = 0x200, 
		//Accelerators (menus) will conflict here. (Is it worth it?)
		//NEW: Combine LEFT,RIGHT,UP,DOWN to form Home,End,PgUp/Down.
		BS_Special = 0x400,
	};
	// These functions indicate that a mouse event has occured while
	// the Tool is active.
	//
	// You will not get mouse events for the middle button.
	//
	virtual void mouseButtonDown() = 0;
	virtual void mouseButtonMove() = 0;
	virtual void mouseButtonUp(){}

	inline void mouseButtonDown(int buttonState, int x, int y);
	inline void mouseButtonMove(int buttonState, int x, int y);
	inline void mouseButtonUp(int buttonState, int x, int y);
	
	//2019: Replaces DecalManager.
	virtual void draw(bool focused){}

	virtual void sizeNib(int delta){} //2024

	static int s_allocated;

	struct ToolCoordT
	{
		double coords[3], w; //TESTING

		//TODO: What else uses this? 
		//atrfartool.cc
		//atrneartool.cc		
		double dist;

		//NEW: Help some highly unreadable code.
		operator unsigned(){ return pos.index; }

		Model::Position pos;

		void w_divide()
		{
			if(w==1) return;
			double l_w = 1/w;
			coords[0]*=l_w; coords[1]*=l_w; coords[2]*=l_w;
			//w = 1;
		}
		void w_multiply()
		{
			if(w==1) return;
			coords[0]*=w; coords[1]*=w; coords[2]*=w;
			//w = 1;
		}
		
		Vector &v(){ return *(Vector*)coords; }
		double &operator[](int i){ return coords[i]; }
		Vector *operator->(){ return (Vector*)coords; }
	};
	typedef std::vector<ToolCoordT> ToolCoordList;
	
	//NOTE: This was defined inside Parent, but I felt it too
	//much to write Tool::Parent::ViewPerspective.
	enum ViewE
	{
		//NEW: -1 down are user perspective.
		_FORCE_SIGNED_ = -1,
		ViewPerspective = 0,
		ViewFront,
		ViewBack,
		ViewLeft,
		ViewRight,
		ViewTop,
		ViewBottom,
		ViewOrtho,
		//NEW: Higher are user orthographic.

		//This is limited to going from ViewOrtho
		//to one of the 6 axial views.
		ViewOrthoDetect=100, 
	};

	//return is number of selected positions
	//minmax returns the center of the square minmax zone
	//in center and the min-max corners in minmax
	int getSelectionCenter(double center[3], double minmax[2][3]=nullptr);

protected:

	// These methods provide functionality that many tools use

	//FIX ME
	//
	// THESE NEED TO WORK WITH HOMOGENEOUS COORDINATES (i.e. 4D vector)
	//
	// These functions act like addVertex and addPoint except that they
	// work in the viewport space instead of the model space, use these
	// instead of addVertex/addPoint whenever possible
	ToolCoordT addPosition(Model::PositionTypeE,double,double,double, const char *name=nullptr, int boneId=-1);
	void movePosition(const Model::Position &pos, double x, double y, double z);
	void movePositionUnanimated(const Model::Position &pos, double x, double y, double z);
	void makeToolCoordList(ToolCoordList &list, const pos_list &positions);
	bool makeToolCoord(ToolCoordT&, Model::Position);
};

class Tool::Parent 
{
public:
	
	Parent() //2022
	{
		snap_select = snap_overlay = false;
	}

	Tool *const tool = nullptr, *prev_tool; 
	
	int tool_index;

	bool resetCurrentTool(bool force)
	{
		return setCurrentTool(tool,tool_index,force);
	}
	bool setCurrentTool(Tool *p, int index, bool force)
	{
		prev_tool = tool; //2024
		if(mouseIsPressed(force))
		return false;
		if(p&&p->parent) p->deactivated(tool_index);
		removeParams();
		if(tool)
		const_cast<Parent*&>(tool->parent) = nullptr;
		if(const_cast<Tool*&>(tool)=p)
		const_cast<Parent*&>(tool->parent) = this;
		tool_index = index;
		if(p&&p->parent) p->activated(index); return true;
	}

	// Get the model that the parent is viewing. This function 
	// should be called once for every event that requires a 
	// reference to the model.
	virtual Model *getModel() = 0;

	virtual ViewE getView() = 0;

	// Call this to force an update on the current model view
	virtual void updateView() = 0;

	// Call this to force an update on all model views
	virtual void updateAllViews() = 0;

	virtual bool mouseIsPressed(bool force_quit) = 0; //2024

	virtual bool getParentCoords(double coords[4], bool selected=false) = 0;	
	// The getParentXYValue function returns the mouse coordinates
	// in viewport space (as opposed to model space), X is left and
	// right, Y is up and down, and Z is depth (undefined) regardless
	// of the orientation of the viewport. Use this function instead
	// of get[XYZ]Value when possible.
	// 
	// The value of "selected" indicates if the snap should include selected
	// vertices or not (generally you would use "true" if you want to start
	// a manipulation operation on selected vertices, for update or all
	// other start operations you would set this value to false).
	//
	inline bool getParentXYZValue(double &xval, double &yval, double &zval, bool selected=false)
	{
		double c[4]; bool ret = getParentCoords(c,selected);
		
		xval = c[0]; yval = c[1]; zval = c[2]; return ret;
	}
	//
	// https://github.com/zturtleman/mm3d/issues/141#issuecomment-651962335
	// Note, MoveTool uses the new XYZ variant but it should be added to the
	// other tools in due time.
	//
	inline void getParentXYValue(double &xval, double &yval, bool selected=false)
	{
		double z; getParentXYZValue(xval,yval,z,selected);
	}	
	inline void getParentXYValue(int bs, int x, int y, double &xval, double &yval)
	{
		_bs = bs; _bx = x; _by = y; 
		
		double z; getParentXYZValue(xval,yval,z); 
	}
	//this is designed to not link in case "getParentXYZValue" is intended
	inline void getParentXYValue(double &xval, double &yval, double &zval);

	// The getRawParentXYValue function returns the mouse coordinates
	// in viewport space (as opposed to model space), X is left and
	// right, Y is up and down, and Z is depth (undefined) regardless
	// of the orientation of the viewport. Use this function instead
	// of get[XYZ]Value when possible.
	// 
	// This function ignores snap to vertex/grid settings.
	//
	virtual void getRawParentXYValue(double &xval, double &yval) = 0;

	inline void getRawParentXYValue(int x, int y, double &xval, double &yval)
	{
		_bx = x; _by = y; getRawParentXYValue(xval,yval); 
	}

	// The getParentViewMatrix function returns the Matrix
	// that is applied to the model to produce the parent's
	// viewport.
	virtual const Matrix &getParentViewMatrix()const = 0;
	virtual const Matrix &getParentViewInverseMatrix()const = 0;
	virtual const Matrix &getParentProjMatrix()const = 0;
	virtual const Matrix &getParentProjInverseMatrix()const = 0;	
	virtual double _getParentZoom()const{ return 1; } //TESTING

	//2020: These replace getParentViewMatrix with a better
	//behaved matrix, that currently can depend on the tool's
	//type. The old APIs are still the same as before but aren't
	//used by GetXYZ, etc. and really should not be used except in
	//very special cases.
	inline const Matrix &getParentBestMatrix(bool p=false)const
	{
		if(p||tool->isSelectTool()) //REMOVE ME
		return getParentProjMatrix(); 
		return getParentViewMatrix(); 
	}
	inline const Matrix &getParentBestInverseMatrix(bool p=false)const
	{
		if(p||tool->isSelectTool()) //REMOVE ME
		return getParentProjInverseMatrix(); 
		return getParentViewInverseMatrix(); 
	}

	// Get the 3d coordinate that corresponds to an x,y mouse event
	// value (in model space).
	//
	// As the views are 2d and the model is 3d, one of the
	// three coordinates will be undefined.
	// These return false if parent or val is nullptr,or the point
	// on the axis in question is undefined for the given view.
	// For example, if the x,y point was provided by the front or back
	// view, the z (depth)component of the coordinate is undefined.
	//
	// In the case of an undefined coordinate, the tool is responsible
	// for providing an appropriate value. For example,a sphere tool
	// would use the same radius for all three axis,and might center
	// the undefined coordinate on zero (0).
	virtual void getXYZ(double*,double*,double*) = 0;

	//REFACTOR
	bool getXValue(double *val) //UNUSED
	{
		double _y,_z; getXYZ(val,&_y,&_z);

		switch(int v=getView()) //???
		{
		default: if(v<ViewOrtho) return false;
		case ViewFront:
		case ViewTop:
		case ViewBottom:
		case ViewBack: case ViewOrtho: return true;
		}
	}
	bool getYValue(double *val) //UNUSED
	{
		double _x,_z; getXYZ(&_x,val,&_z);

		switch(int v=getView()) //???
		{
		default: if(v<ViewOrtho) return false;
		case ViewFront:
		case ViewBack:
		case ViewLeft:
		case ViewRight: case ViewOrtho: return true;
		}
	}
	bool getZValue(double *val) //UNUSED
	{
		double _x,_y; getXYZ(&_x,&_y,val);

		switch(int v=getView()) //???
		{
		default: if(v<ViewOrtho) return false;
		case ViewLeft:
		case ViewRight:
		case ViewTop:
		case ViewBottom: case ViewOrtho: return true;
		}
	}

	//2022: This is for the Hide command to figure
	//out which layer to assign to.
	virtual unsigned getPrimaryLayer() = 0;

	// The addDecal and removeDecal methods are not called directly
	// by tools. You must use the DecalManager::addDecalToParent
	// function instead.
	//
	// A decal is an object that is not part of a model,but drawn
	// over it to indicate some information to the user. The rotate
	// tool uses decals to display the center point of rotation.
	// There is no documentation on decals,but you can see the
	// RotateTool,RotatePoint,and DecalManager classes for an example 
	// of decal usage.
	//virtual void addDecal(Decal *decal) = 0;
	//virtual void removeDecal(Decal *decal) = 0;

	//EXPERIMENTAL/UNDOCUMENTED
	virtual void addBool(bool cfg, bool *p, const char *name) = 0;
	virtual void addInt(bool cfg, int *p, const char *name, int min=INT_MIN, int max=INT_MAX) = 0;
	virtual void addDouble(bool cfg, double *p, const char *name, double min=-DBL_MAX, double max=DBL_MAX) = 0;
	virtual void addEnum(bool cfg, int *p, const char *name, const char **enum_0_terminated) = 0;
	virtual void addButton(int bt, Tool *p, void(*f)(Tool *t, int bt), const char *name) = 0;
	virtual void groupParam(){}
	virtual void updateParams() = 0;
	virtual void removeParams() = 0;
	virtual void hideParam(void *p, int disable=-1) = 0;

	enum Command
	{
		cmd_open_color_window,
		cmd_open_color_window_select_joint,
	};
	virtual void command(Command, int arg1=0, int arg2=0) = 0; //addButton
	
	//EXPERIMENTAL
	//polytool.cc and selecttool.cc
	bool snap_select;
	bool snap_overlay;
	Model::Position snap_object;
	unsigned snap_vertex;

	int _bs,_bx,_by,_bs_lock = 0;

	int &getButtonX(){ return _bx; }
	int &getButtonY(){ return _by; }
	int &getButtons(){ return _bs; }

	int getButtonsLocked(int filter){ return _bs|(_bs_lock&filter); }
	int getButtonsLocked(){ return _bs|_bs_lock; }

	//EXPERIMENTAL
	bool snapSelect(Model::PositionTypeE type=Model::PT_ALL) 
	{
		snap_select = true;
		snap_object.type = type;
		snap_object.index = -1; //Check if implemented?
		auto &pos = snap_object;
		double _[3];
		getParentXYZValue(_[0],_[1],_[2],true);
		if(-1!=snap_object)
		{
			Model *model = getModel();
			bool how = !model->isPositionSelected(pos);
			if(how&&~getButtonsLocked()&BS_Shift)
			model->selectAllPositions(pos.type,false); //OVERKILL
			model->selectPosition(pos,how);
			return true;
		}
		return false;
	}

	//2024: implements colortool.cc
	virtual bool applyColor(double radius, double press, int vmode, int &iomode) = 0;
	virtual void setBackgroundColor(float press) = 0;
};
inline void Tool::mouseButtonDown(int buttonState, int x, int y)
{
	parent->_bs = buttonState; parent->_bx = x; parent->_by = y;
	mouseButtonDown();
}
inline void Tool::mouseButtonMove(int buttonState, int x, int y)
{
	parent->_bs = buttonState; parent->_bx = x; parent->_by = y;
	mouseButtonMove();
}
inline void Tool::mouseButtonUp(int buttonState, int x, int y)
{
	parent->_bs = buttonState; parent->_bx = x; parent->_by = y;
	mouseButtonUp();
}

#endif // __TOOL_H
