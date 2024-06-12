/* MM3D (Multimedia 3D)
 *
 * This code is an original contribution not wishing to 
 * be subjected to any form of copyright based thoughts.
 * 
 */

#include "mm3dtypes.h" //PCH

#include "glheaders.h"
#include "graphs.h"

#include "tool.h" //ButtonState
#include "glmath.h" //distance

#include "model.h"
#include "modelundo.h" //MU_SelectAnimFrame
#include "log.h"
#include "modelstatus.h"

GraphicWidget::GraphicWidget(Parent *parent)
	: 
ScrollWidget(zoom_min,zoom_max),
//PAD_SIZE(6),
parent(parent),
m_scrollTextures(),
m_model(),
m_autoSize(),
m_divDragStart(),
m_operation(),m_selection(),
m_moveToFrame(),
//m_scaleKeepAspect(),
m_scalePoint(),
m_selecting(),
m_freezing(),
m_highlightColor(),
m_buttons(),
m_xMin(0),
m_xMax(1), //???
m_yMin(0),
m_yMax(1), //???
m_width(),m_height(),m_asymmetry()
{
	m_scroll[0] = m_scroll[1] = 0.5; //???
}

static std::pair<int,GraphicWidget*> 
graphs_div;
static void graphs_div_timer(int id)
{
	//printf("graphs_div_timer %d\n",id);
	if(graphs_div.first==id)
	if(graphs_div.second)
	graphs_div.second->setCursorUI(id?id:-1);
}
GraphicWidget::~GraphicWidget()
{
	graphs_div.second = 0; //Timer business.
}

Model::Animation *GraphicWidget::animation()
{
	auto &al = m_model->getAnimationList();
	if(al.empty()) return nullptr;
	auto anim = m_model->getCurrentAnimation();
	auto a = al[anim];
	return const_cast<Model::Animation*>(a);
}

void GraphicWidget::setModel(Model *model)
{
	if(m_model) m_model->removeObserver(this);

	m_model = model;

	m_viewportWidth = m_viewportHeight = 0;

	if(m_model) m_model->addObserver(this);

	modelChanged(Model::ChangeAll);
}
void GraphicWidget::modelChanged(int cb)
{
	int mode = m_model->getAnimationMode();

	bool clr = false;

	if(mode&1&&cb&Model::SelectionJoints
	 ||mode&2&&cb&Model::SelectionPoints)
	{
		clr = true;
	}
	if(cb&Model::AnimationSet) //SKETCHY
	{
		clr = true;

		//Should reset scroll but not zoom.
		GraphicWidget::keyPressEvent('0',0,0,0);
	}

	if(clr)
	{
		m_autoSize = 0; clearSelected(true);
	}
}

void GraphicWidget::updateViewport(int how)
{
	if(how=='z') //HACK
	{
		parent->zoomLevelChangedSignal();
	}

	m_xMin = m_scroll[0]-m_zoom/2;
	m_xMax = m_scroll[0]+m_zoom/2;
	m_yMin = m_scroll[1]-m_zoom/2;
	m_yMax = m_scroll[1]+m_zoom/2;

	parent->updateWidget(); //updateGL();
}

bool GraphicWidget::mousePressEvent(int bt, int bs, int x, int y)
{	
	if(!m_interactive) return false;

	if(!parent->mousePressSignal(bt)) return false;
	
	x-=m_x; y-=m_y; //NEW
	
	m_constrain = 0;
	m_constrainX = x; //m_lastXPos = x;
	m_constrainY = y; //m_lastYPos = y; 	

	if(m_buttons&bt) //DEBUGGING?
	{
		//HACK: In case the button state is confused
		//let the user unstick buttons by pressing
		//the button. (Hopefully this only happens
		//when using the debugger "break" function.)
		m_buttons&=~bt;

		if(bt==m_activeButton) m_activeButton = 0;

		model_status(m_model,StatusError,STATUSTIME_LONG,
		TRANSLATE("LowLevel","Warning: Unstuck confused mouse button state"));
	}

	if(!m_buttons) //NEW
	{
		assert(!m_activeButton);

		m_activeButton = bt;

		if(!m_model->getViewportUnits().no_overlay_option) //2022
		if(pressOverlayButton(x+m_x,y+m_y,false)) 
		{
			return true;
		}
	}

	m_buttons|=bt; //e->button();

	if(bt!=m_activeButton) return true; //NEW

	double s = getWindowSCoord(x);
	double t = getWindowTCoord(y);

	//if(e->button()&Qt::MidButton)
	if(bt==Tool::BS_Middle||bt&&bs&Tool::BS_Ctrl) //2022
	{
		return true; // We're panning
	}
	else if(graphs_div.first&&graphs_div.second==this)
	{
		auto i = m_div;
		Div *p = &m_divs[i];
		bool b = bt!=Tool::BS_Left;
		if(i==0||b&&p<&m_divs.back()) 
		p++;
		m_freezing = FreezeSize;
		m_divDragY = y;
		m_divDragStart = p->obj->m_graph_range;

		return true;
	}
	else if(bt&&m_operation==MouseNone)
	{
		return true; // We're panning
	}

	/*if(m_uvSnap) switch(m_operation) //2022
	{
	case MouseMove:
	case MouseScale: snap(s,t,true);
	}*/
	m_s = s; m_t = t;
	
	int bs_locked = bs|parent->_tool_bs_lock; //2022

	bool shift = 0!=(bs_locked&Tool::BS_Shift);

	//2022: Emulate Tool::Parent::snapSelect?
	m_selecting = bt==Tool::BS_Left;

	m_accum = 0;
	m_trans_frames.clear();

	switch(m_operation)
	{
	case MouseSelect:
		
		if(!(bt==Tool::BS_Right||bs_locked&Tool::BS_Shift))
		{
			clearSelected();			
		}
		m_xSel1 = s; m_ySel1 = t;
		m_xSel2 = s; m_ySel2 = t; //NEW:
		//m_selecting = e->button()&Qt::RightButton?false:true;
		m_selecting = bt&Tool::BS_Right?false:true;
		break;
	
	case MouseScale:

		if(shift) m_constrain = ~3;
		startScale(s,t);
		break;

	case MouseMove:

		if(shift) m_constrain = ~3;
		break;
	}	

	return true; //I guess??
}
void GraphicWidget::mouseMoveEvent(int bs, int x, int y)
{
	if(!m_interactive) return;

	x-=m_x; y-=m_y;

	int bt = m_activeButton; //NEW

	if(m_freezing==FreezeSize)
	return dragRange(x,y,bt);
	
	double s,t,ds,dt;
	s = getWindowSCoord(x); 
	t = getWindowTCoord(y);

	if(bt==Tool::BS_Middle
	||bt&&bs&Tool::BS_Ctrl //2022
	||bt&&m_operation==MouseNone) //2022
	{
		goto pan;
	}
	else if(m_overlayButton!=ScrollButtonMAX)
	{
		switch(m_overlayButton)
		{
		case ScrollButtonPan: pan:
		
			//For some reason (things are moving) using
			//m_s and m_t won't work.
			double ds = getWindowSDelta(x,m_constrainX);
			double dt = getWindowTDelta(y,m_constrainY);

			m_constrainX = x; m_constrainY = y;

			m_scroll[0]-=ds; m_xMin-=ds; m_xMax-=ds;
			m_scroll[1]-=dt; m_yMin-=dt; m_yMax-=dt;

			updateViewport('p'); break;
		}

		return;
	}
	else if(!bt) //!m_buttons
	{
		int cur = 0;
		if(m_divs.size()>1&&s>0&&s<1)
		{	
			for(auto&ea:m_divs)
			{
				double d = ea.div; if(d>t) continue;

				m_div = &ea-m_divs.data();

				double half = 0.5*m_divsPadding;

				double z = 0.5;
				if(m_zoom>1&&m_zoom<2) z*=m_zoom;

				//if(t-d<m_divsPadding) cur = UI_UpDownCursor;
				if(fabs(t-(d+half))<z*half) cur = UI_UpDownCursor;
				
				break;
			}
		}			

		auto&[f,s] = graphs_div;
		if(cur!=f||s&&s!=this)
		{
			f = cur; s = this;
			setTimerUI(200,graphs_div_timer,cur);
		}		
		
		return;
	}

	//2022: Emulate Tool::Parent::snapSelect?
	if(m_operation!=MouseSelect) m_selecting = false;
	
	/*if(m_uvSnap) switch(m_operation)
	{
	case MouseMove:
	case MouseScale: snap(s,t,false);
	}*/
	if(m_constrain)
	{
		//TODO: STANDARDIZE AND REQUIRE MINIMUM PIXELS
		if(m_constrain==~3) 
		{
			int ax = std::abs(x-m_constrainX);
			int ay = std::abs(y-m_constrainY);
		
			//if(ax==ay) return; //NEW
			if(std::abs(ax-ay)<4) return; //2022

			m_constrain = ax>ay?~1:~2;
		}
		if(m_constrain&1) x = m_constrainX; else m_constrainX = x;
		if(m_constrain&2) y = m_constrainY; else m_constrainY = y;

		if(m_constrain&1) s = getWindowSCoord(x); 
		if(m_constrain&2) t = getWindowTCoord(y);
	}	
	ds = s-m_s; m_s = s; 
	dt = t-m_t; m_t = t;
	
	switch(m_operation)
	{
	case MouseSelect:
		
		updateSelectRegion(s,t);
		break;

	case MouseMove:
  
		moveSelectedVertices(ds,dt);
		parent->updateCoordinatesSignal();
		break;

	case MouseScale:

		scaleSelectedVertices(s,t,ds,dt);							
		parent->updateCoordinatesSignal();
		break;
	}
}
void GraphicWidget::mouseReleaseEvent(int bt, int bs, int x, int y)
{
	if(!m_interactive) return;

	m_buttons&=~bt; //e->button();

	if(bt!=m_activeButton) return; //NEW
		
	m_activeButton = 0;

	if(auto e=m_freezing)
	{
		m_freezing = FreezeNone;

		if(e==FreezeSize)
		{
			parent->updateWidget();

			return; //!
		}
	}

	if(m_overlayButton!=ScrollButtonMAX)
	{
		m_overlayButton = ScrollButtonMAX;

		cancelOverlayButtonTimer(); //HACK

		return; //!
	}		
	
	if(bt==Tool::BS_Middle
	||bt&&bs&Tool::BS_Ctrl //2022
	||bt&&m_operation==MouseNone) //2022
	{
		return; // We're panning
	}

	x-=m_x; y-=m_y;

	//2022: Emulate Tool::Parent::snapSelect?
	if(m_selecting&&m_operation!=MouseSelect)
	{
		m_xSel1 = m_xSel2 = getWindowSCoord(x);
		m_ySel1 = m_ySel2 = getWindowTCoord(y);		
		selectDone(1|bs|parent->_tool_bs_lock);
	return parent->updateSelectionDoneSignal();
	}	
	else switch(m_operation)
	{
	case MouseSelect:
		
		selectDone();
						
		parent->updateSelectionDoneSignal();
		break;

	case MouseMove:
	case MouseScale: 
		
		parent->updateCoordinatesDoneSignal();
		break;
	}
}

void GraphicWidget::wheelEvent(int wh, int bs, int x, int y)
{
	if(m_interactive) //Is navigation interaction?
	{
		//ModelViewport does this and it might make sense too
		//if an app had multiple texture views
		if(!over(x,y)) return wh>0?zoomIn():zoomOut();

		zoom(wh>0,getWindowSCoord(x-m_x),getWindowTCoord(y-m_y));
	}
}

bool GraphicWidget::keyPressEvent(int bt, int bs, int, int)
{	
	if(m_interactive) switch(bt)
	{
	//case Qt::Key_Home:
	case Tool::BS_Left|Tool::BS_Special: //HOME
	
	//	if(~bs&Tool::BS_Shift)
		{
			m_scroll[0] = m_scroll[1] = 0.5;
			m_scroll[0]+=m_asymmetry;
			m_zoom = 1;
		}		
		updateViewport('z'); break;
	
	//case Qt::Key_Equal: case Qt::Key_Plus: 
	case '=': case '+': zoomIn(); break;

	//case Qt::Key_Minus: case Qt::Key_Underscore:
	case '-': case '_': zoomOut(); break;

	case '0':
		
		m_scroll[0] = m_scroll[1] = 0.5;
		m_scroll[0]+=m_asymmetry;
		updateViewport('p'); break;

	//case Qt::Key_Up:
	case Tool::BS_Up: scrollUp(); break;
	//case Qt::Key_Down:
	case Tool::BS_Down: scrollDown(); break;
	//case Qt::Key_Left:
	case Tool::BS_Left: scrollLeft(); break;
	//case Qt::Key_Right: 
	case Tool::BS_Right: scrollRight(); break;

	//default: QGLWidget::keyPressEvent(e); break;
	default: return false;
	}
	//else QGLWidget::keyPressEvent(e);
	return true;
}

void GraphicWidget::updateSelectRegion(double x, double y)
{
	m_xSel2 = x; m_ySel2 = y; 
	
	parent->updateWidget(); //updateGL();
}

void GraphicWidget::clearSelected(bool graph)
{	
	auto *a = animation(); if(!a) return;

	auto &st = a->m_selected_frames;
	if(!st.empty()&&!graph)
	{
		Model::Undo<MU_SelectAnimFrame> undo;
		bool sel = false;
		for(auto&ea:st) if(ea)
		{
			if(!sel)
			{	
				sel = true;

				m_model->setChangeBits(Model::AnimationSelection);

				auto anim = m_model->getCurrentAnimation();
				undo = Model::Undo<MU_SelectAnimFrame>(m_model,anim);
			}

			ea = false;

			if(undo) undo->setSelectionDifference(&ea-st.data(),false);
		}
		st.clear();
	}
	//Vertices?
	{
		Model::Undo<MU_SwapStableMem> undo;
		bool sel = false;
		for(auto&ea:a->m_keyframes)
		for(auto*kp:ea.second)
		{
			auto &s = kp->m_selected;

			auto cmp = s; if(graph)
			{
				auto *o = m_model->getPositionObject(kp->m_objectIndex);				
				if(s==o->m_selected)
				continue;
				s = o->m_selected;
			}
			else if(s) s.x = s.y = s.z = false; 
			if(cmp==s) continue;

			if(!sel)
			{	
				sel = true;

				m_model->setChangeBits(Model::AnimationSelection);

				if(undo=Model::Undo<MU_SwapStableMem>(m_model,false))
				{
					undo->setEdit(false);
					undo->addChange(Model::AnimationSelection);
				}
			}
			if(undo) if(!graph) undo->addMemory(&s,&cmp,&s);
			else undo->addMemory(&s.graphed,&cmp.graphed,&s.graphed);
		}
	}
}
void GraphicWidget::selectDone(int snap_select)
{
	auto *a = animation(); if(!a) return;

	double x1 = m_xSel1, y1 = m_ySel1;
	double x2 = m_xSel2, y2 = m_ySel2;

	double epsx = 6.1/m_width*m_zoom;

	bool snapx; if(snapx=x1==x2) //2020
	{
		x2 = x1+epsx; x1-=epsx;
	}
	if(y1==y2) //2020
	{
		double y = 6.1/m_height*m_zoom;
		y2 = y1+y; y1-=y;
	}

	if(x1>x2) std::swap(x1,x2);
	if(y1>y2) std::swap(y1,y2);	

	int lh = parent->label_height;
	auto &tt = a->m_timetable2020;
	auto &st = a->m_selected_frames;
	double l_tf = a->_time_frame();
	if(l_tf) l_tf = 1/l_tf; //zero divide

	double t = getWindowTCoord(m_viewportHeight-lh);

	const bool self = m_selection&SelectFrame;
	const bool selv = m_selection&SelectVertex;

	bool unselect = false;
	if(snap_select&&~snap_select&Tool::BS_Shift)
	unselect = true;
	bool how = snap_select?true:m_selecting;

	bool sel2 = false;

	for(int pass=snap_select?2:1;pass-->0;)
	{
		bool sel = false;
		
		bool did_frame = self||y2>=t||m_divs.size()<=1;
		if(did_frame&&!selv) did_frame:
		{	
			//NOTE: I think this is the first time an undo
			//object has been created outside of Model but
			//I think this should be the way in the future
			//more and more because Model.h is a behemoth!
			Model::Undo<MU_SelectAnimFrame> undo;	

			int i = -1; for(auto f:tt)
			{
				i++; f*=l_tf;

				if(f>x1&&f<x2)
				{
					if(pass==1)
					{
						if(!st.empty()&&st[i])
						{
							how = false;						
							unselect = false;
							goto pass;
						}
						continue;
					}

					if(how)
					{
						if(!st.empty()) 
						{
							if(st[i]) continue;
						}
						else st.resize(tt.size());	
					}
					else if(st.empty()||!st[i])
					{
						continue;
					}
				}
				else if(unselect&&pass==0)
				{
					if(st.empty()||!st[i]) continue;
				}
				else continue;

				st[i] = !st[i]; 
				
				//YUCK: only select 1 when knotted
				if(snapx&&i&&tt[i]*l_tf-tt[i-1]*l_tf<epsx&&st[i]&&st[i-1])
				{
					st[i] = false;
				}

				if(!sel)
				{
					sel = sel2 = true;

					m_model->setChangeBits(Model::AnimationSelection);

					auto anim = m_model->getCurrentAnimation();
					undo = Model::Undo<MU_SelectAnimFrame>(m_model,anim);
				}
				if(undo) undo->setSelectionDifference(i,st[i]);
			}
		}
		else if(!self) //Control point selection?
		{
			Model::Undo<MU_SwapStableMem> undo;

			auto iit = m_graph_splines.begin();
			auto itt = m_graph_splines.end();			
			//for(int i=0;iit<itt;i++)
			for(int i=9998;iit<itt;i--)
			{	
				auto jt = iit, jtt = jt+1;
				while(jtt<itt&&jtt->s>0.0) jtt++;

				if(jt+1!=jtt) for(;jt<jtt;jt++)				
				{
					if(!jt->key) continue; //terminator

					bool &s = (&jt->key->m_selected.x)[i%3];

					if(jt->s>x1&&jt->s<x2
					 &&jt->t>y1&&jt->t<y2)
					{
						if(pass==1)
						{
							if(s)
							{
								how = false;						
								unselect = false;
								goto pass;
							}
							continue;
						}

						if(how==s) continue;
					}
					else if(unselect&&pass==0)
					{
						if(!s) continue;
					}
					else continue;

					bool old = s; s = !s;
				
					if(!sel)
					{
						sel = sel2 = true;

						m_model->setChangeBits(Model::AnimationSelection);

						if(undo=Model::Undo<MU_SwapStableMem>(m_model,false))
						{
							undo->setEdit(false);
							undo->addChange(Model::AnimationSelection);
						}
					}
					if(undo) undo->addMemory(&s,&old,&s);
				}
				else jt++; iit = jt;
			}		
		}
		if(!sel&&!did_frame&&!selv)
		{
			did_frame = true; goto did_frame; pass:;
		}
	}

	m_selecting = false; //2024

	parent->updateWidget(); //updateGL();
}

void GraphicWidget::moveSelectedVertices(double x, double y)
{
	auto *a = animation(); if(!a) return;

	auto anim = m_model->getCurrentAnimation();

	auto &st = a->m_selected_frames;
	if(x&&!st.empty())
	{		
		auto &tt = a->m_timetable2020;	
		double tf = a->_time_frame(); 

		x = m_accum+=x*tf;

		if(m_trans_frames.empty())
		for(auto i=tt.size();i-->0;) if(st[i])
		{
			m_trans_frames.push_back({i,tt[i]});
		}
		for(auto&ea:m_trans_frames)
		{
			double time = ea.time+x;

			if(m_moveToFrame) time = round(time);

			time = std::min(tf,std::max(0.0,time));

			//UNFORTUNATELY THIS IS COMPLICATED
			auto i = ea.index;
			auto j = m_model->moveAnimFrame(anim,i,time);
			if(j==i) continue;

			//THIS REPRODUCES THE INTERNAL LOGIC
			//OF moveAnimFrame. 
			auto min = std::min(i,j);
			auto max = std::max(i,j);
			for(auto&ea2:m_trans_frames)			
			if(ea2.index>=min&&ea2.index<=max)
			{
				int cmp = (int)ea.index-(int)i;
				if(cmp>0) ea.index--;
				else if(cmp<0) ea.index++;
				else ea.index = j;
			}
		}
	}
	if(y)
	{	
		int hc = m_highlightColor;

		for(auto&ea:a->m_keyframes)
		{
			auto o = m_model->getPositionObject(ea.first);
			auto &d = m_divs[o->m_graph_div];

			double scl = y/(d.size*0.5);

			for(auto*kp:ea.second) if(kp->m_selected)		
			{
				double p[3];
				memcpy(p,kp->m_parameter,sizeof(p));

				bool set = false;
				for(int dim=3;dim-->0;)		
				if(kp->m_selected[dim])
				{				
					unsigned i = dim+(kp->m_isRotation>>1)*3;

					if(hc&&hc-1!=i) continue;

					if(i<9)
					{
						set = true; 

						p[dim]+=scl*d.scale_factors[i];
					}
					else assert(0);
				}
				if(set) 
				{
					m_model->setKeyframe(anim,kp,p);
				}
			}
		}
	}

	parent->updateWidget(); //updateGL();
}
void GraphicWidget::moveSelectedFrames(double time)
{
	auto *a = animation(); if(!a) return;

	//ASSUMING NOT MID OPERATION!
	m_accum = 0; m_trans_frames.clear();

	double x = a->_time_frame();
	if(x) x = time/x; //zero divide

	moveSelectedVertices(x,0);

	parent->updateWidget(); //updateGL();
}

void GraphicWidget::startScale(double x, double y)
{	
	auto *a = animation(); if(!a) return;

	double xMin = +DBL_MAX, xMax = -DBL_MAX;
	double yMin = +DBL_MAX, yMax = -DBL_MAX;
		
	//frames
	{		
		auto &tt = a->m_timetable2020;
		auto &st = a->m_selected_frames;
		double l_tf = a->_time_frame();
		if(l_tf) l_tf = 1/l_tf;  //zero divide

		m_trans_frames.clear(); if(!st.empty())
		{
			for(size_t i=0;i<tt.size();i++) if(st[i])
			{
				m_trans_frames.push_back({i,tt[i]*l_tf});

				double t = m_trans_frames.back().time;
				xMin = std::min(xMin,t);
				xMax = std::max(xMax,t);			
			}
		}
	}
	//paramaters
	{
		int mid = 0;
		for(size_t i=m_divs.size();i-->1;)		
		{
			auto &d = m_divs[i];

			if(y<=m_divs[i-1].div&&y>=d.div)
			{
				mid = (int)i; break;
			}
		}

		m_scale_params.clear();

		int hc = m_highlightColor;

		for(auto&ea:a->m_keyframes)
		{
			auto o = m_model->getPositionObject(ea.first);
			auto &d = m_divs[o->m_graph_div];

			double scl = d.size*0.5;

			for(auto*kp:ea.second) if(kp->m_selected)	
			{		
				double p[3];
				memcpy(p,kp->m_parameter,sizeof(p));

				for(int dim=3;dim-->0;)		
				if(kp->m_selected[dim])
				{				
					unsigned i = dim+(kp->m_isRotation>>1)*3;

					if(hc&&hc-1!=i) continue;

					if(i<9) //DUPLICATE
					{
						double val = kp->m_parameter[dim];
						val/=scl*d.scale_factors[i];

						ScaleParamT sp;
						sp.div = (unsigned short)o->m_graph_div;
						sp.index = (unsigned short)i;
						sp.y = val;						
						m_scale_params.push_back(sp);

						if(mid==o->m_graph_div)
						{
							yMin = std::min(yMin,val);
							yMax = std::max(yMax,val);
						}
					}
					else assert(0);
				}
			}
		}
		//NOTE: Can't be sorted without rewriting scaleSelectedVertices.
		assert(std::is_sorted(m_scale_params.begin(),m_scale_params.end()));
		auto *p = m_scale_params.data();
		auto *d = m_scale_params.size()+p;
		while(p!=d)
		{
			auto *pp = p;

			double yMax = p->y, yMin = p->y; //SHADOWING

			int cmp = (int)*p;

			for(;p!=d&&*p==cmp;p++)
			{
				yMin = std::min(yMin,p->y);
				yMax = std::max(yMax,p->y);
			}

			double centerY,startLengthY;

			if(ScalePtOrigin==m_scalePoint)
			{
				centerY = 0.0;

				startLengthY = y; //fabs
			}
			else if(ScalePtCenter==m_scalePoint)
			{
				centerY = (yMax+yMin)/2;

				startLengthY = y-centerY; //fabs
			}
			else if(ScalePtFarCorner==m_scalePoint)
			{
				double minmin = fabs(y-yMin), minmax = fabs(y-yMax); //SAME
				double maxmin = fabs(y-yMin), maxmax = fabs(y-yMax); //SAME
				
				//Can this be simplified?
				if(minmin>minmax)
				{
					if(minmin>maxmin)
					{
						centerY = minmin>maxmax?yMin:yMax;
					}
					else same: // maxmin>minmin
					{
						centerY = maxmin>maxmax?yMin:yMax;
					}
				}
				else // minmax>minmin
				{
					if(minmax>maxmin)
					{
						centerY = yMax;
					}
					else goto same; // maxmin>minmax			
				}
								
				startLengthY = y-centerY; //fabs
			}
			startLengthY = fabs(startLengthY);
			for(p=pp;p!=d&&*p==cmp;p++)
			{
				p->center = centerY;
				p->start = startLengthY;
			}
		}
	}

	if(xMax==-DBL_MAX) xMax = xMin = 0.0;
	if(yMax==-DBL_MAX) yMax = yMin = 0.0;

	if(ScalePtOrigin==m_scalePoint)
	{
		m_centerX = 0.0;
		m_centerY = 0.0;

		m_startLengthX = x; //fabs
		m_startLengthY = y; //fabs
	}
	else if(ScalePtCenter==m_scalePoint)
	{
		m_centerX = (xMax+xMin)/2;
		m_centerY = (yMax+yMin)/2;

		m_startLengthX = x-m_centerX; //fabs
		m_startLengthY = y-m_centerY; //fabs
	}
	else if(ScalePtFarCorner==m_scalePoint)
	{
		//DUPLICATES scaletool.cc.

		//NOTE: sqrt not required.
		double minmin = distance(x,y,xMin,yMin);
		double minmax = distance(x,y,xMin,yMax);
		double maxmin = distance(x,y,xMax,yMin);
		double maxmax = distance(x,y,xMax,yMax);

		//Can this be simplified?
		if(minmin>minmax)
		{
			if(minmin>maxmin)
			{
				if(minmin>maxmax)
				{
					m_centerX = xMin; m_centerY = yMin;
				}
				else
				{
					m_centerX = xMax; m_centerY = yMax;
				}
			}
			else same2: // maxmin>minmin
			{
				if(maxmin>maxmax)
				{
					m_centerX = xMax; m_centerY = yMin;
				}
				else
				{
					m_centerX = xMax; m_centerY = yMax;
				}
			}
		}
		else // minmax>minmin
		{
			if(minmax>maxmin)
			{
				if(minmax>maxmax)
				{
					m_centerX = xMin; m_centerY = yMax;
				}
				else
				{
					m_centerX = xMax; m_centerY = yMax;
				}
			}
			else goto same2; // maxmin>minmax			
		}

		m_startLengthX = x-m_centerX; //fabs
		m_startLengthY = y-m_centerY; //fabs
	}
	m_startLengthX = fabs(m_startLengthX);
	m_startLengthY = fabs(m_startLengthY);
}
void GraphicWidget::scaleSelectedVertices(double xx, double yy, double dx, double dy)
{
	auto *a = animation(); if(!a) return;

	auto anim = m_model->getCurrentAnimation();

//	if(m_scaleKeepAspect) s = t = std::max(s,t);

	if(dx&&!m_trans_frames.empty())
	{
		double x = m_centerX;

		double s = m_startLengthX<0.00006?1:fabs(x-xx)/m_startLengthX;

		auto &tt = a->m_timetable2020;
		auto &st = a->m_selected_frames;
		double tf = a->_time_frame(); 
		
		bool rev = xx<x?dx>0:dx<0;
		int len = (int)m_trans_frames.size();

		//moveAnimFrame will get things out of order
		//without this logic
		int i,mid = len-1;
		for(i=0;i<len;i++)
		if(m_trans_frames[i].time>x)
		{
			mid = i; break;
		}
		for(int pass=1;pass<=2;pass++)
		{
			int dir = pass==1?1:-1;
			int beg = pass==1?0:len-1;
			int end = pass==1?mid-1:mid;

			if(rev) dir = -dir;
			if(rev) std::swap(beg,end);
							
			for(i=beg;i!=end+dir;i+=dir) 
			{
				auto &ea = m_trans_frames[i];

				double tx = ea.time;

				tx = (tx-x)*s+x; //lerp

				//tx = std::max(0.0,std::min(1.0,tx));
				if(pass==1) tx = std::max(0.0,std::min(x,tx));
				if(pass==2) tx = std::max(x,std::min(1.0,tx));

				double swap = tt[ea.index];
				int id = m_model->moveAnimFrame(anim,ea.index,tf*tx);

				if(id!=ea.index)
				for(int j=len;j-->0;) if(id==m_trans_frames[j].index)
				{
					id = m_model->moveAnimFrame(anim,id,swap);
					assert(id==ea.index);
					parent->updateWidget(); 
					return;
				}
				ea.index = id;
			}
		}
	}
	if(dy)
	{	
		double y = m_centerY, rel_y = fabs(y-yy);
		double t = m_startLengthY<0.00006?1:rel_y/m_startLengthY;

		int k = 0;

		int hc = m_highlightColor;

		for(auto&ea:a->m_keyframes)
		for(auto*kp:ea.second) if(kp->m_selected)		
		{
			double p[3];
			memcpy(p,kp->m_parameter,sizeof(p));

			bool set = false;
			for(int dim=3;dim-->0;)		
			if(kp->m_selected[dim])
			{				
				unsigned i = dim+(kp->m_isRotation>>1)*3;

				if(hc&&hc-1!=i) continue;

				if(i<9) //DUPLICATE
				{
					set = true; 

					auto &sp = m_scale_params[k++];						
					double val = sp.y;
					double t = sp.start<0.00006?1:rel_y/sp.start;					
					val = (val-sp.center)*t+sp.center; //lerp

					auto &d = m_divs[sp.div];
					p[dim] = val*d.size*0.5*d.scale_factors[i];
				}
				else assert(0);
			}
			if(set) m_model->setKeyframe(anim,kp,p);
		}
	}

	parent->updateWidget(); //updateGL();
}

bool GraphicWidget::animation_delete(bool protect)
{
	auto *a = animation(); if(!a) return false;

	auto anim = m_model->getCurrentAnimation();

	auto &st = a->m_selected_frames;
	double l_tf = a->_time_frame();
	if(l_tf) l_tf = 1/l_tf;  //zero divide

	int did = 0;

	if(!st.empty()) //frames
	{	
		for(size_t i=0;i<st.size();) if(st[i])
		{
			did|=1; m_model->deleteAnimFrame(anim,i);	
		}
		else i++;
	}
	else //paramaters
	{
		std::vector<Model::Keyframe*> v;

		for(auto&ea:a->m_keyframes)
		for(auto*kp:ea.second)
		for(int dim=3;dim-->0;)		
		if(kp->m_selected[dim])
		{	
			did|=2; v.push_back(kp); break;
		}

		for(auto*kp:v)
		m_model->deleteKeyframe(anim,kp);
	}
	if(!did&&!protect)
	{
		int frame = m_model->getCurrentAnimationFrame();
		if(m_model->getCurrentAnimationFrameTime()
		==m_model->getAnimFrameTime(anim,frame)
		&&m_model->deleteAnimFrame(anim,frame))
		{
			did|=4;
		}
		else model_status(m_model,StatusError,STATUSTIME_LONG,
		"The current time isn't an animation frame");
	}

	if(did) parent->updateWidget(); return did!=0;
}

void GraphicWidget::dragRange(int x, int y, int bt)
{
	auto i = m_div;
	Div *p = &m_divs[i];
	bool b; if(i==0)
	{
		b = true; p++;
	}
	else if(p<&m_divs.back())
	{
		b = bt!=Tool::BS_Left;

		if(b) p++; 
	}
	else b = false;

	double dt = getWindowTDelta(y,m_divDragY);
	if(!b) dt = -dt;

	//NOTE: m_divs isn't updated while dragging
	//to keep things simple.
	double sz = p->size;
	double scl = m_divDragStart;
	float &ref_size = p->obj->m_graph_range;

	sz = scl+dt/sz*scl;

	assert(m_autoSize);
	double as = m_autoSize?1/m_autoSize:1;

	//NOTE: At a minimum this must be greater
	//than zero.
	//1.0 is nice because it can be minimized
	//to the start size by dragging to a stop.
	//double sz2 = std::min(4.0,std::max(sz,1.0)); //ARBITRARY
	double sz2 = std::min(4.0,std::max(sz,as)); //ARBITRARY

	ref_size = (float)sz2; 

	if(b) //Pan?
	{	
		double dt2 = getWindowTDelta(y,m_constrainY);

		m_constrainY = y;

		if(sz2!=sz)		
		if(double zd=sz-ref_size) //zero-divide?
		dt2*=(sz2-ref_size)/zd; 
		else dt2 = 0;

		m_scroll[1]-=dt2; m_yMin-=dt2; m_yMax-=dt2;

		updateViewport('p'); 
	}
	else parent->updateWidget();
}

void GraphicWidget::scrollAnimationFrameToPixel(int x)
{
	if(!m_viewportWidth) return;

	Model *m = m_model;
	int mode = m?m->getAnimationMode():0;
	if(!mode) return;

	auto anim = m->getCurrentAnimation();

	double tf = m->getAnimTimeFrame(anim);
	double ft = m->getCurrentAnimationFrameTime();

	//NOTE: -1 is a tweak to match how draw is
	//organized in order to line up the miters
	//in lines with the 2px wide frame markers.
	double s = getWindowSCoord(x-m_x+1)-ft/tf;

	m_scroll[0]-=s; updateViewport('p');
}

typedef GraphicWidget::SplinePT graphs_spline_pt;
typedef std::vector<graphs_spline_pt> graphs_spline_t;
static void graphs_make_2d
(Model::Animation *a, double (*src)[3],
graphs_spline_t &spline, const Model::KeyframeList &kl)
{
	auto &tt = a->m_timetable2020;
	double l_tf = a->_time_frame();
	if(l_tf) l_tf = 1/l_tf; //zero divide 

	//Reversed seems to make more sense to me
	//because the blending favors later lines.
	//for(int i=0;i<9;i++) //TODO: graph time
	for(int i=9;i-->0;)
	{	
		int j = i; //Reverse? Rotation first?
		if(j<3) j+=3; 
		else if(j<6) j-=3;
		int k = 1<<j/3, l = i%3;
		size_t nz = 2, sz = spline.size();
		for(auto*kp:kl) if(k==kp->m_isRotation)
		{					
			double time = l_tf*tt[kp->m_frame];

			assert(time>=0&&time<=1); //Extensions?

			if(nz==2)
			{
				if(time>0)
				{			
					nz = 1;
					//HACK: The extra marks these as extensions.
					spline.push_back({0.0,src[k/2][l]}); //0
				}
				else nz = 0;		
			}
			else if(time==0) //HACK
			{
				//Knots on 0.0 appear like extra 0-terminators.
				time = 0.0000001;
			}

			spline.push_back({time,kp->m_parameter[l]});
			spline.back().key = kp;
		}
		bool keep = false;
		for(size_t i=nz+sz;i<spline.size();i++)
		{
			if(fabs(spline[i].t)>0.0000001)
			{
				keep = true; break;
			}
		}
		if(!keep) spline.resize(sz);
		for(nz+=sz;nz<spline.size();nz++)
		{
			if(spline[nz].t!=0.0) break;
		}
		if(nz>=spline.size()) 
		{
			spline.resize(sz);
			spline.push_back({0,0});
		}
		else if(spline.back().t<1.0)
		{
			//HACK: The extra marks these as extensions.
			spline.push_back({1.0,spline.back().t}); //1
		}
	}
}
static void graphs_normalize(double scale_factors[9], graphs_spline_t &spline, size_t b, size_t e, double tx, double ty)
{
	auto iit = spline.begin()+b;
	auto itt = spline.begin()+e;
	for(int ii=9-1;iit<itt;ii--)
	{	
		int i = ii; //Reverse? Rotation first?
		if(i<3) i+=3; 
		else if(i<6) i-=3;

		auto jt = iit, jtt = jt+1;
		while(jtt<itt&&jtt->s>0.0) jtt++;

		if(jt+1!=jtt)
		{
			auto jjt = jt;

			double max = 0;
			for(jt=jjt;jt<jtt;jt++)
			max = std::max(max,fabs(jt->t));			

			if(max<0.0000003) //0.0000001 is zero
			max = 0.0;
			
			if(scale_factors) scale_factors[i] = max;

			if(max) max = ty/max; //zero divide

			for(jt=jjt;jt<jtt;jt++)
			{
				jt->t = tx+max*jt->t;
			}			
		}
		else jt++; iit = jt;
	}
}
static unsigned char graphs_colors[9][4] =
{
	//Rotation is given primary colors
	//since skeletal animations mostly
	//rely on rotation and translation
	//should stand out. 
	//Translation is drawn first since
	//if there's overlap it will be in
	//background. It's usually constant
	//for posing prior to an animation.
	{255,0,0}, //red
	{0,225,0}, //green
	{0,0,255}, //blue
	{255,0,255}, //magenta
	{255,255,0}, //yellow
	{0,255,255}, //cyan
	//I have no clue what colors for this?
	{255,192,203}, //pink
	{255,128,0}, //orange
	//{128,64,0}, //brown
	{98,0,219}, //dark indigo
};
int GraphicWidget::highlightColor(int i)
{
	int o = m_highlightColor; if(i>=0&&i<=9&&i!=o) 
	{
		m_highlightColor = i; parent->updateWidget();
	}
	return o;
}
static void graphs_draw_2d(graphs_spline_t &spline, int how, int hc, float zoom)
{
	float lw = how==1?25.0f*std::min(1.0f,zoom):how==0?3.0f:1.5f;
	glLineWidth(sqrtf((lw)/zoom)); 
	
	float hide = std::max(1.0f,sqrtf(zoom)); //Less if zoomed out.
	glEnable(GL_BLEND);

	for(int pass=1;pass<=2;pass++)
	{
		if(0==hc)
		{
			if(pass==2) return;

			if(how==0) glColor4ub(0,0,0,160);
			if(how==255) glColor3ub(255,255,255);
		}

		auto iit = spline.begin(), itt = spline.end();			
		//for(int i=0;iit<itt;i++)
		for(int i=9998;iit<itt;i--)
		{	
			auto jt = iit, jtt = jt+1;
			while(jtt<itt&&jtt->s>0.0) jtt++;

			if(jt+1!=jtt)
			{
				jt++; jtt--; //InterpolateStep?

				int i9 = i%9, bypass = 0;

				if(how==1)
				{
					auto &c = graphs_colors[i9];
					c[3] = 128;
					if(hc) if(hc-1!=i9)
					{
						c[3]-=72*hide; bypass = 2;
					}
					else 
					{
						bypass = 1; //c[3]+=64;
					}
					if(bypass!=pass) glColor4ubv(c);
				}
				else if(hc)
				{
					unsigned char c[4];
					for(int k=3;k-->0;)
					c[k] = (unsigned char)how;
					c[3] = how==0?160:255;
					if(hc-1!=i9)
					{
						c[3]-=128; bypass = 2;
					}
					else bypass = 1;
					if(bypass!=pass) glColor4ubv(c);
				}

				if(bypass==pass)
				{
					while(jt<=jtt) jt++; iit = jt;
						
					continue;
				}
		
				//Strips may improve GL_LINE_SMOOTH miters.
				glBegin(GL_LINE_STRIP);
				for(;jt<=jtt;jt++)
				{
					glVertex2i(jt[-1].x,jt[-1].y);

					int e = jt->key?jt->key->m_interp2020:0;				
					if(e==Model::InterpolateStep)
					{
						glVertex2i(jt[0].x,jt[-1].y);

						glEnd(); glBegin(GL_LINE_STRIP);
					}				
				}
				glVertex2i(jt[-1].x,jt[-1].y);
				glEnd();
			}
			else jt++; iit = jt;
		}			
	}

	glLineWidth(1); glDisable(GL_BLEND);
}
static void graphs_draw_vs(graphs_spline_t &spline, int how, int hc, double scl, float zoom)
{
	glLineWidth(2/zoom); glEnable(GL_BLEND); scl*=zoom;
	
	glBegin(GL_LINES);		
	for(int pass=1;pass<=2;pass++)
	{
		if(pass==2&&!hc) continue;

		auto iit = spline.begin(), itt = spline.end();

		for(int i=9998;iit<itt;i--)
		{	
			int i9 = i%9, bypass = 0;

			int sc = 255; if(hc)
			{
				if(hc-1!=i9)
				{
					bypass = 2;
				}
				else
				{
					//bypass = 1;
						
					switch(i9)
					{
				//	case 1:
				//	case 2:
					case 0: sc-=40; //red
					}
				}
			}

			auto jt = iit, jtt = jt+1;
			while(jtt<itt&&jtt->s>0.0) jtt++;
				
			if(jt+1!=jtt)
			{
				auto &c = graphs_colors[i9];
					c[3] = 4;							
				if(bypass!=pass) glColor4ubv(c);

				for(;jt<jtt;jt++)
				{	
					if(!jt->key) continue; //terminator
					
					int sel = jt->key->m_selected[i%3];

					if(how!=sel) continue;

					if(bypass!=pass) if(pass!=2||sel)
					{
						double vs = jt->s;
						double vt = jt->t;
						double vh = sel?pass==1?!hc?1.2:1.4:0.8:1.0;
						vh*=scl;
						glVertex2d(vs,vt+vh);
							//58 was good with lots of key frames
							//but is hard to see with very few. I
							//guess an option would be helpful.
							//c[3] = 58;
							c[3] = sel?sc:!hc?85:65;
						glColor4ubv(c);
					//	if(sel) glLineWidth(4); //Doesn't work.
						glVertex2d(vs,vt);
						glVertex2d(vs,vt);
					//	if(sel) glLineWidth(2); //(Nvidia)
							c[3] = 4;
						glColor4ubv(c); 
						glVertex2d(vs,vt-vh);
					}					
				}
			}
			else jt++; iit = jt;
		}
	}
	glEnd();

	glLineWidth(1); glDisable(GL_BLEND);
}

void GraphicWidget::Parent::_color3ub(unsigned char c)
{
	c = palette[c/32]; glColor3ub(c,c,c);
}
void GraphicWidget::draw(int x, int y, int w, int h)
{		
	Model *m = m_model; if(!m) return;

	int mode = m->getAnimationMode();
	int anim = m->getCurrentAnimation();

	int hc = m_highlightColor;

	if(hc) if(hc-1<3) hc+=3; else if(hc-1<6) hc-=3;

	if(PAD_SIZE) //updateSize()
	{
		//enum{ PAD_SIZE=6 };

		x+=PAD_SIZE; w-=PAD_SIZE*2;
		y+=PAD_SIZE; h-=PAD_SIZE*2;
	}
	int ww = w, hh = h;
	
	//KIND OF A MESS
	//
	// For the record, the goal here is to map
	// 0-1 to the current frame marker extents.
	// That puts the text labels a little over
	// to the side since the frame is centered.
	// The first label is 0 and so is skinnier.

	int n = (int)m->getAnimTimeFrame(anim);
	int lx = parent->sizeLabel("0")/2; //HACK

	//Offset half a digit on the left for the
	//centered (half) "0" and a leading space.
	char buf[32];
	ww-=lx+lx+lx*sprintf(buf,"%d",n)+lx; //HACK	

	//updateSize()
	{
		if(h!=m_viewportHeight) m_autoSize = 0;

		m_x = x; m_y = y;
	 	
		m_viewportX = x; m_viewportWidth = w;
		m_viewportY = y; m_viewportHeight = h;

		m_x+=(w-ww)/2; m_y+=(h-hh)/2;

		if(w<=0||h<=0) return;
	}
	
	m_width = ww; m_height = hh;

	//This formula is a roundabout way of finding
	//the right margin and then subtract the left
	//margin (last term) to find their difference.
	int asym = w-ww-lx*2-lx*2-1;

	//I don't know? Must somehow account
	//for the asymmetry in these margins.
	{
		double cmp = asym/(double)ww;

		if(double da=cmp-m_asymmetry)
		{
			m_asymmetry = cmp;

			//NOTE: m_zoom shouldn't factor here.
			m_scroll[0]+=da; updateViewport('p');
		}
	}

		if(!mode) return;
	
	//192 is the background color.
	//User is responsible for filling it.
	auto bg = parent->palette[192/32];
	//TODO: This may need a setting if anyone
	//can't see it cleary.
	auto lt = bg-12;

		//NEW: OpenGL can't backup its states.
		//ModelViewport might need to do this
		//if it wasn't its own OpenGL context.
		const int attribs1
		=GL_CURRENT_BIT //?
		|GL_ENABLE_BIT
		|GL_LIGHTING_BIT		
		|GL_SCISSOR_BIT
		|GL_TEXTURE_BIT;
		glPushAttrib(attribs1); //labels

	//initializeGL()
	{
		glScissor(x,y,w,h);	
		glDisable(GL_LIGHTING);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_SCISSOR_TEST);
	}
		const int attribs2 
		=GL_CURRENT_BIT //?
		|GL_ENABLE_BIT
		|GL_DEPTH_BUFFER_BIT
		|GL_POLYGON_BIT
	//	|GL_TRANSFORM_BIT
		|GL_VIEWPORT_BIT;
		glPushAttrib(attribs2); //splines
	
	double l_z = 1/m_zoom;
	double cx,cy,px,py,sMin,sMax,tMin,tMax;
	{
		cx = m_xMax+m_xMin; cy = m_yMax+m_yMin;
	//	double cx = m_xMax+m_xMin, cy = m_yMax+m_yMin;
		double cw = m_xMax-m_xMin, ch = m_yMax-m_yMin;
		cw*=(double)w/ww; ch*=(double)h/hh;
		cx/=2; cy/=2; cw/=2; ch/=2;
		sMin = cx-cw;
		sMax = cx+cw;
		tMin = cy-ch;
		tMax = cy+ch;

		//cx = cw/w; cy = ch/h;
		px = 2*cw/w;
		py = 2*ch/h;

	//	cx = cw/w*0.95; cy = ch/h*0.95;
		cx = cw/w; cy = ch/h;
	}

	auto ortho = [=](bool on)
	{
		if(on) //setViewportDraw(); //DUPLICATE
		{
			//glScissor(x,y,w,h);		
			glViewport(x,y,w,h); 

			glMatrixMode(GL_PROJECTION);		
			glPushMatrix();
			glLoadIdentity();

			//HACK: I'm adding cx/cy just because 
			//Nvidia drivers are drawing the selection
			//rectangle with fully disconnected lines
			//(I thought Nvidia OpenGL was good?)
			//NOTE: Nvida works fine with glRectd but
			//AMD Adrenalin draws nothing with glRectd
			//NOTE: texwidget.cc does the same thing for a
			//different reason
			//glOrtho(sMin,sMax,tMin,tMax,-1,1);
			glOrtho(sMin-cx,sMax-cx,tMin+cy,tMax+cy,-1,1);

			glMatrixMode(GL_MODELVIEW);		
			glPushMatrix(); 
			glLoadIdentity();
		}
		else
		{
			glMatrixMode(GL_PROJECTION); 
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW); 
			glPopMatrix();
		}
	};
	ortho(true);	

	auto *a = animation();
	auto &tt = a->m_timetable2020;
	double l_tf = a->_time_frame();
	int nn = n+(n!=l_tf);
	if(l_tf) l_tf = 1/l_tf; //zero divide

	double epsx = 2.1/m_width*m_zoom/l_tf; //2024
	float zoom = sqrtf(std::min<float>(1,m_zoom));

	//margins and frames?
	{
		parent->_color3ub(96);

		double ml = px*lx*2;
		double mr = ml+px*asym;
		mr+=px; //1 extra bold font pixel?
		mr+=px; //FUDGE
		glBegin(GL_LINES);
		glVertex2d(0-ml,tMin); glVertex2d(0-ml,tMax);
		glVertex2d(1+mr,tMin); glVertex2d(1+mr,tMax);				
		glEnd();
				
		//glColor3ub(180,180,180);
		glColor3ub(lt,lt,lt);

		glLineWidth(2/zoom);
		glBegin(GL_LINES);
		auto *tpp = tt.data();
		auto *tp = tpp, *td = tp+tt.size(); while(tp!=td) 
		{
			bool knot = tp!=tpp&&*tp-tp[-1]<epsx;

			if(knot)
			{
				glLineWidth(3/zoom); glColor3ub(lt-50,lt-50,lt-50);
			}

			double f = *tp++*l_tf; 
						
			glVertex2d(f,tMin); glVertex2d(f,tMax);
			
			//HACK: Making double wide to fill centered
			//bold digits. It also makes them a little
			//visible behind the frame marker.
			/*Need to use glLineWidth to match vertices.
			glVertex2d(f+px,tMin); glVertex2d(f+px,tMax);*/

			if(knot)
			{
				glLineWidth(2/zoom); glColor3ub(lt,lt,lt);
			}
		}
		glEnd();
	//	glLineWidth(1);
	}
				
	int lh = parent->label_height;
	auto bl = parent->palette[0]; //black
	enum{ ba=64 };

	glEnable(GL_BLEND);

	auto &sf = a->m_selected_frames;

	if(!sf.empty()) //selection?
	{		
		glColor4ub(255,0,0,64);

		int n = (int)tt.size()-1;

		for(int j,i=0;i<n;i++) if(sf[i]&&sf[i+1])
		{
			for(j=i+1;j+1<=n&&sf[j+1];) j++;

			//BLACK MAGIC
			//glRectd rasterizes different from GL_LINES???
			//glRectd(l_tf*tt[i]-px,tMin,l_tf*tt[j]+px,tMax);
			glRectd(l_tf*tt[i]-px*1.5,tMin,l_tf*tt[j]+px*1.5,tMax);

			i = j;
		}

		glLineWidth(1/zoom);

		glColor4ub(255,0,0,128);

		glBegin(GL_LINES);
		int knot,prev = 0;
		auto *tp = tt.data()-1; for(int sel:sf) 
		{
			tp++; if(sel)			
			{
				if(knot=prev&&*tp-tp[-1]<epsx)
				{
					glLineWidth(5/zoom); glColor4ub(222,0,0,255);
				}

				double f = *tp*l_tf;

				//The current frame marker is on the left
				//side of the double line.
				glVertex2d(f,tMin); glVertex2d(f,tMax);

				if(knot)
				{
					glLineWidth(1/zoom); glColor4ub(255,0,0,128);
				}
			}
			prev = sel;
		}		
		glEnd();
	}
	glLineWidth(1);

	//graphs_spline_t spline;
	auto &spline = m_graph_splines; spline.clear(); 
	
	double t = (h-lh)/m_height;
	double dt = (h-lh*4)/m_height-t;
		
	//This has to increase with zoom since it's too
	//noticeable when zooming is a little bit wrong.
	//enum{ epad=4 };
	//double pad = cy*-epad*2;
	double pad = dt*0.15, epad = -pad*m_height*l_z;
	
	if(!m_freezing)
	{
		m_divsPadding = -2*pad;
		m_divs.clear();
		m_divs.push_back(nullptr);
		m_divs.back().div = t+pad;
		m_divs.back().size = t;
	}
	unsigned char divs = 1;

	auto &jl = m->getJointList();
	auto &pl = m->getJointList();
	if(m_autoSize==0)
	{
		//NOTE: This can't change when dragging
		//a divider to resize or it cancels out.
		assert(!m_freezing);

		for(auto&ea:jl) ea->m_graph_div = 0;
		for(auto&ea:pl) ea->m_graph_div = 0;

		double scl = 0; int sel = 0;
		if(mode&1) for(auto*jp:jl) if(jp->m_selected)
		{
			sel++; scl+=jp->m_graph_range;
		}
		if(mode&2) for(auto*jp:pl) if(jp->m_selected)
		{
			sel++; scl+=jp->m_graph_range;
		}
		scl =-std::min(t+pad*2*sel,300/(m_height/sel))/dt/scl;
		scl = std::max(1.0,scl);	

		m_autoSize = scl;
	}
	double scl = m_autoSize;

	double tx,ty = 0;
	auto graph_keys = [&](Model::Position j, auto &jl) //C++14
	{
		for(;j<jl.size();j++) if(jl[j]->m_selected)
		{
			tx = t+dt*jl[j]->m_graph_range*scl;
			ty = (t-tx)*0.5; 
			
			glBegin(GL_LINES);
			glColor4ub(bl,bl,bl,ba);
			double r = getWindowTCoordRoundedToPixel(t); //YUCK
			glVertex2d(0,r); glVertex2d(1,r);

			tx+=pad; t = tx+pad; tx+=ty;

			glColor4ub(bl,bl,bl,128);
			double rx = getWindowTCoordRoundedToPixel(tx); //YUCK
			glVertex2d(0,rx); glVertex2d(1,rx);
			glEnd();			

			auto drag = m_freezing;
			if(!drag) m_divs.push_back(jl[j]);			
			auto &div = m_divs[jl[j]->m_graph_div=divs++];
			if(!drag) div.div = t+pad;
			if(!drag) div.size = ty*2;

			auto it = a->m_keyframes.find(j);

			if(it==a->m_keyframes.end()) return;

			size_t sz = spline.size();
			auto *sf = drag?nullptr:div.scale_factors;
			graphs_make_2d(a,jl[j]->_sources(),spline,it->second);			
			graphs_normalize(sf,spline,sz,spline.size(),tx,ty);
		}
	};	
	if(mode&1) graph_keys({Model::PT_Joint,0},jl);
	if(mode&2) graph_keys({Model::PT_Point,0},pl);	
	if(ty)
	{
		glBegin(GL_LINES);
		glColor4ub(bl,bl,bl,ba);
		double r = getWindowTCoordRoundedToPixel(t); //YUCK
		glVertex2d(0,r); glVertex2d(1,r);
		glEnd();
	}
		
	//client/pixel space (labels and lines)
	{
		float star = dt*sqrtf(1);

		auto iit = spline.begin(), itt = spline.end();
		while(iit<itt)
		{	
			auto jt = iit, jtt = jt+1;
			while(jtt<itt&&jtt->s>0.0) jtt++;
				
			if(jt+1!=jtt) for(;jt<jtt;jt++)
			{						
				//HACK: GL_LINE_SMOOTH needs to be drawn
				//in pixel space.
				int vx = m_x+(int)round(getWindowX(jt->s));
				int vy = m_y+(int)round(getWindowY(jt->t));
				parent->getLabel(vx,vy);

				jt->x = vx; jt->y = vy;
			}
			else jt++; iit = jt;
		}
		graphs_draw_vs(spline,0,hc,star,zoom);

		//glPushAttrib doesn't cover
		//these states.
		ortho(false);
		glPopAttrib(); //attribs2

		//NOTE: GL_LINE_SMOOTH needs to be in
		//pixel space for a consistent result.

		glHint(GL_LINE_SMOOTH_HINT,GL_NICEST);
		glEnable(GL_LINE_SMOOTH);

		//base layer?
		{
			graphs_draw_2d(spline,0,hc,m_zoom); //zoom
			graphs_draw_2d(spline,255,hc,m_zoom);		
		}

		//labels?
		{
			glColor3ub(bl,bl,bl);

			//HACK: This looks better perceptually when
			//zoomed out and keeps the label closer to
			//the 0 line than the top bar with zoom_max.
		//	double compact = std::min(2.0,(m_zoom-1)*2);
			int compact = std::min(2,(int)((m_zoom-0.5)*2));

			//ll was kept labels from overlapping with 
			//the tick marks, but I think it's readable
			//as long as the tick marks are bold enough.
		//	double ll = y+h-lh*2+6; //ARBITRARY
			double ll = y+h;
			double l = y+getWindowY(1-lh/m_height);
			int mx = 1+std::max(x,(int)getWindowX(0)+m_x+1);
			for(auto&ea:m_divs) if(auto*jp=ea.obj)
			{	
				float sz;
				if(jp->m_type==Model::PT_Joint)				
				sz = ((Model::Joint*)jp)->m_graph_range;
				else
				sz = ((Model::Point*)jp)->m_graph_range;				
				double ln = lh*3*l_z*sz*scl+epad*2;
				l-=ln; ln = l+ln*0.5-compact;
				if(ln<ll&&ln>-lh)				
				parent->drawLabel(mx,(int)round(ln),jp->m_name.c_str(),0);
			}
		}

		//color layer?
		{
			graphs_draw_2d(spline,1,hc,m_zoom); //zoom
			glPushAttrib(attribs2);
			ortho(true);
			graphs_draw_vs(spline,1,hc,star,zoom);
			glPopAttrib(); //attribs2
			ortho(false);
		}

		glLineWidth(1);
		glDisable(GL_LINE_SMOOTH);

		//tick marks?	
		{	
			parent->_color3ub(32);
		
			int ln = y+h-lh;
			int l0x = lx*10;
			double w_z = w*l_z-(w-ww);				
			double dx = w_z*l_tf;
			int tick = dx?lh*2/(int)dx:0; //zero divide
			if(tick>1)
			if(tick>2)
			if(tick>5)
			if(tick>10)
			if(tick>15)
			if(tick>20)
			if(tick>25)
			if(tick>50)
			tick = 100;
			else tick = 50;
			else tick = 25;
			else tick = 20;
			else tick = 15;
			else tick = 10;
			else tick = 5;
			else tick = 2;
			else tick = 1;
			dx = w_z/(n/(double)tick);				
			if(nn%tick) nn+=tick;
			for(int i=0;i<=nn;i+=tick)
			{
				int d; if(i>0)
				{
					if(d=i%tick)
					d = i+tick-i%tick;
					else d = i;
				}
				else d = 0;

				int mx = m_x+(int)round(getWindowX(d*l_tf));

				if(mx+l0x>x) if(mx<x+w+l0x)
				{
					if(i>n) parent->_color3ub(128);

					int sz = lx*2*sprintf(buf,"%d",d); //HACK
					//int sz = parent->sizeLabel(buf);
					parent->drawLabel(mx-sz/2-1,ln-1,buf,'b');
				}
				else break;
			}
		}

		glPushAttrib(attribs2);
		ortho(true);
	}
	
	//frame marker?
	{	
		glEnable(GL_BLEND);

		//parent->_color3ub(128);
		glColor4ub(bl,bl,bl,128);

		auto f = l_tf*m->getCurrentAnimationFrameTime();

		glBegin(GL_LINES);
		glVertex2d(f-px,tMin); glVertex2d(f-px,tMax);
		glEnd();

		glDisable(GL_BLEND);
	}

	if(m_selecting) //drawSelectBox()
	{
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); 

		glEnable(GL_COLOR_LOGIC_OP);

		//glColor3ub(255,255,255);
		glColor3ub(0x80,0x80,0x80);
		glLogicOp(GL_XOR);
		{
			//2022: doesn't work with new AMD Adrenalin
			//glRectd(m_xSel1,m_ySel1,m_xSel2,m_ySel2);
			glBegin(GL_QUADS);
			glVertex2d(m_xSel1,m_ySel1);
			glVertex2d(m_xSel1,m_ySel2);
			glVertex2d(m_xSel2,m_ySel2);
			glVertex2d(m_xSel2,m_ySel1);
			glEnd();
		}
		glDisable(GL_COLOR_LOGIC_OP);
	}

	if(m_interactive)
	if(m_viewportHeight>=54)
	if(!m_model->getViewportUnits().no_overlay_option) //2022
	{
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

		drawOverlay(m_scrollTextures);
	}

	//glPushAttrib doesn't cover
	//these states.
	ortho(false);
	glPopAttrib(); //attribs2
	glPopAttrib(); //attribs1
}