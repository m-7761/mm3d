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

#include "model.h"
#include "msg.h" //2019
#include "translate.h" //2019

#ifdef MM3D_EDIT
#include "modelstatus.h"
#include "modelundo.h"
#endif // MM3D_EDIT

#include "log.h"
#include "glmath.h"

Model::AnimBase2020 *Model::_anim(AnimationModeE m, unsigned index)const
{
	switch(m)
	{
	case ANIMMODE_SKELETAL: if(index<m_skelAnims.size()) return m_skelAnims[index]; break;
	case ANIMMODE_FRAME: if(index<m_frameAnims.size()) return m_frameAnims[index]; break;
	}
	return nullptr;
}
Model::AnimBase2020 *Model::_anim(unsigned anim, unsigned frame, Position pos, bool verbose)const
{
	AnimBase2020 *ab = nullptr;
	size_t objects;
	if(pos.type==PT_Joint //TEMPORARY API
	&&anim<m_skelAnims.size())
	{
		ab = m_skelAnims[anim]; objects = m_joints.size();
	}
	else if(pos.type==PT_Point //TEMPORARY API
	&&anim<m_frameAnims.size())
	{
		ab = m_frameAnims[anim]; objects = m_points.size();
	}	
	int c = ab?pos.type==PT_Joint?'j':'c':'?';
	if(!ab||frame>=ab->_frame_count()||pos>=objects)
	{
		assert(!verbose); //PROGRAMMER ERROR

		//REMOVE US
		if(verbose)
		log_error("anim keyframe out of range: anim %d, frame %d, %coint %d\n",anim,frame,c,pos.index); //???
		if(verbose&&ab)		
		log_error("max frame is %d, max %coint is %d\n",c,ab->_frame_count());

		ab = nullptr;
	}	
	else //YUCK
	{
		assert(ab->m_keyframes.size()==objects);

		ab->m_keyframes.resize(objects);	
	}
	return ab;
}

#ifdef MM3D_EDIT

unsigned Model::insertAnimFrame(AnimationModeE mode, unsigned anim, double time)
{
	auto ab = _anim(mode,anim); if(!ab) return 0; 

	auto frame = getAnimFrame(mode,anim,time);
	double cmp = ab->_frame_time(frame);
	unsigned count = ab->_frame_count();
	if(time>cmp //insert after?
	||frame==0&&cmp>0&&time!=cmp //insert before?
	||!count&&!cmp&&!frame) //e.g. zero-length pose?
	{
		//If 0 the new frame is somewhere before the
		//first frame and it has a nonzero timestamp.
		if(count) frame+=time>cmp;

		//TODO: Relying on setAnimFrameCount to fill
		//out the new vertices with the current ones.
		//It would be an improvement to do that here.
		//REMINDER: HOW MU_AddFrameAnimFrame ANOTHER //???
		//ModelUndo WON'T BE NEEDED TO FILL THE DATA.
		setAnimFrameCount(mode,anim,count+1,frame,nullptr);
		setAnimFrameTime(mode,anim,frame,time);		
	}
	else assert(time==cmp); return frame;
}

int Model::addAnimation(AnimationModeE m, const char *name)
{
	LOG_PROFILE();

	int num = -1;
	if(name) switch(m)
	{
	case ANIMMODE_SKELETAL:
	{
		num = m_skelAnims.size();
		auto sa = SkelAnim::get();
		sa->m_name = name;
				
		sa->m_keyframes.resize(m_joints.size()); //YUCK

		if(m_undoEnabled)
		{			
			auto undo = new MU_AddAnimation;
			undo->addAnimation(num,sa);
			sendUndo(undo);
		}

		insertSkelAnim(num,sa);
		break;
	}
	case ANIMMODE_FRAME:
	{
		num = m_frameAnims.size();
		auto fa = FrameAnim::get();
		fa->m_name = name;

		//TODO: Define FrameAnim::init or add m_frame0 to
		//AnimBase2020.
		if(!m_vertices.empty())		
		fa->m_frame0 = m_vertices.front()->m_frames.size();
		else fa->m_frame0 = 0;
		
		fa->m_keyframes.resize(m_points.size()); //YUCK

		if(m_undoEnabled)
		{
			auto undo = new MU_AddAnimation;
			undo->addAnimation(num,fa);
			sendUndo(undo);
		}

		insertFrameAnim(num,fa);
		break;
	}}
	return num;
}

void Model::deleteAnimation(AnimationModeE m, unsigned index)
{
	LOG_PROFILE();

	auto ab = _anim(m,index); if(!ab) return;
	
	auto undo = m_undoEnabled?new MU_DeleteAnimation:nullptr;

	if(m==ANIMMODE_SKELETAL)
	{
		if(undo) undo->deleteAnimation(index,(SkelAnim*)ab); //?
		
		removeSkelAnim(index);
	}
	else //ANIMMODE_FRAME
	{
		FrameAnim *fa;
		if(undo) undo->deleteAnimation(index,fa=(FrameAnim*)ab); //?

		auto fp = fa->m_frame0;
		auto dt = undo?undo->removeVertexData():nullptr;
		removeFrameAnimData(fp,fa->_frame_count(),dt);
		removeFrameAnim(index);
	}

	if(undo) sendUndo(undo);
}

bool Model::setAnimName(AnimationModeE m, unsigned anim, const char *name)
{
	if(auto*ab=_anim(m,anim))
	{
		if(m_undoEnabled)
		{
			auto undo = new MU_SetAnimName;
			undo->setName(m,anim,name,ab->m_name.c_str());
			sendUndo(undo);
		}

		ab->m_name = name; return true;
	}
	return false;
}

bool Model::setAnimFrameCount(AnimationModeE mode, unsigned anim, unsigned count)
{
	return setAnimFrameCount(mode,anim,count,~0u,nullptr);
}
bool Model::setAnimFrameCount(AnimationModeE m, unsigned anim, unsigned count, unsigned where, FrameAnimData *ins)
{
	unsigned old_count = getAnimFrameCount(m,anim);
	int diff = (int)count-(int)old_count;
	if(!diff) return true;

	//NOTE: where is for makeCurrentAnimationFrame.
	bool compat_mode = where==~0u; if(compat_mode)
	{
		where = diff>0?old_count:(int)old_count+diff;
	}
	else if(diff<0)
	{
		if(where+(unsigned)-diff>old_count)
		{
			assert(where+(unsigned)-diff<=old_count); 
		
			return false;
		}
	}
	else if(where+diff>count)
	{
		assert(where+diff<=count); return false;
	}

	AnimBase2020 *ab; Position j;
	if(m==ANIMMODE_SKELETAL&&anim<m_skelAnims.size())
	{
		ab = m_skelAnims[anim];		
		j = {PT_Joint,m_joints.size()};
	}
	else if(m==ANIMMODE_FRAME&&anim<m_frameAnims.size())
	{
		ab = m_frameAnims[anim];
		j = {PT_Point,m_points.size()};
	}
	else return false;

	auto undo = m_undoEnabled?new MU_SetAnimFrameCount:nullptr;

	if(undo) undo->setAnimFrameCount(m,anim,count,old_count,where);	
	if(undo) undo->removeTimeTable(ab->m_timetable2020);

	if(m==ANIMMODE_FRAME)
	{
		FrameAnim *fa = m_frameAnims[anim];
		auto fp = fa->m_frame0+where;

		auto dt = undo?undo->removeVertexData():nullptr;

		if(diff<0)
		{
			removeFrameAnimData(fp,-diff,dt);
		}
		else 
		{
			//NEW: Gathering the pointers so routines like copyAnimation
			//don't have to generate undo data for their copied vertices.
			assert(!ins||!dt);

			insertFrameAnimData(fp,diff,ins?ins:dt,fa);
		}
	}

	while(j-->0) //m_points/joints.size()
	{	
		KeyframeList &l = ab->m_keyframes[j];
		if(diff<0)
		{
			size_t i;
			for(i=0;i<l.size()&&l[i]->m_frame<where;)
			i++;
			unsigned whereto = where-diff;
			while(i<l.size()&&l[i]->m_frame<whereto)
			deleteKeyframe(anim,l[i]->m_frame,j);
			for(;i<l.size();i++)
			l[i]->m_frame+=diff;
		}
		else for(auto*ea:l) if(ea->m_frame>=where)
		{
			ea->m_frame+=diff;
		}
	}
	
	if(undo) sendUndo(undo);
			
	auto it = ab->m_timetable2020.begin()+where;

	if(compat_mode) //Compatibility mode.
	{
		ab->m_timetable2020.resize(count);
		for(auto i=old_count;i<count;i++)
		ab->m_timetable2020[i] = i;
	}
	else if(diff>0)
	{				
		ab->m_timetable2020.insert(it,diff,-1); //!!!
	}
	else ab->m_timetable2020.erase(it,it-diff);

	if(m_currentAnim==anim&&m_currentFrame>=count)
	{
		//setCurrentAnimationFrame(0);
		setCurrentAnimationFrame(count?count-1:0);
	}

	invalidateAnim(m,anim,m_currentFrame);
	
	return true;
}

bool Model::setAnimFPS(AnimationModeE m, unsigned anim, double fps)
{
	if(auto*ab=_anim(m,anim))
	{
		if(m_undoEnabled)
		{
			auto undo = new MU_SetAnimFPS;
			undo->setFPS(m,anim,fps,ab->m_fps);
			sendUndo(undo,true);
		}

		ab->m_fps = fps; return true;
	}
	return false;
}

bool Model::setAnimWrap(AnimationModeE m, unsigned anim, bool wrap)
{
	if(auto*ab=_anim(m,anim))
	{
		if(m_undoEnabled)
		{
			auto undo = new MU_SetAnimWrap;
			undo->setAnimWrap(m,anim,wrap,ab->m_wrap);
			sendUndo(undo,true);
		}

		ab->m_wrap = wrap; return true;
	}
	return false;
}

bool Model::setAnimTimeFrame(AnimationModeE m, unsigned anim, double time)
{	
	AnimBase2020 *ab = _anim(m,anim); if(!ab) return false;

	size_t frames = ab->m_timetable2020.size();
	if(frames&&time<ab->m_timetable2020.back())
	{
		model_status(this,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel","Cannot set timeframe before keyframes"));
		return false;
	}

	if(m_undoEnabled)
	{
		auto undo = new MU_SetAnimTime;
		undo->setAnimTimeFrame(m,anim,time,ab->m_frame2020);
		sendUndo(undo,true);
	}

	ab->m_frame2020 = time;

	if(time<m_currentTime&&m==m_animationMode&&anim==m_currentAnim)
	{
		//REMINDER: If this were to be an error case I'm skeptical
		//that "redo" operations could backfire.
		setCurrentAnimationTime(time);
	}
	else invalidateAnim(m,anim,(unsigned)frames-1);	
			
	return true;
}

bool Model::setAnimFrameTime(AnimationModeE m, unsigned anim, unsigned frame, double time)
{
	AnimBase2020 *ab = _anim(m,anim);

	if(!ab||frame>=ab->m_timetable2020.size()) return false;

	auto &cmp = ab->m_timetable2020[frame];
	if(cmp!=time) 
	{
		if(m_undoEnabled)
		{
			auto undo = new MU_SetAnimTime;
			undo->setAnimFrameTime(m,anim,frame,time,cmp);
			sendUndo(undo,true);
		}

		cmp = time;			
	
		invalidateAnim(m,anim,frame);
	}
	return true;
}

/*
void Model::setFrameAnimPointCount(unsigned pointCount)
{
	unsigned anim = 0;
	unsigned frame = 0;

	unsigned oldCount = 0;

	unsigned acount = m_frameAnims.size();
	for(anim = 0; anim<acount; anim++)
	{
		unsigned fcount = m_frameAnims[anim]->_frame_count();
		oldCount = m_frameAnims[anim]->m_frameData[0]->m_framePoints.size();
		for(frame = 0; frame<fcount; frame++)
		{
			FrameAnimPoint *fap = nullptr;
			unsigned pcount = m_frameAnims[anim]->m_frameData[frame]->m_framePoints.size();
			while(pointCount>pcount)
			{
				fap = FrameAnimPoint::get();
				m_frameAnims[anim]->m_frameData[frame]->m_framePoints.push_back(fap);
				pcount++;
			}
			while(pointCount<pcount)
			{
				m_frameAnims[anim]->m_frameData[frame]->m_framePoints.back()->release();
				m_frameAnims[anim]->m_frameData[frame]->m_framePoints.pop_back();
				pcount--;
			}
		}
	}

	MU_SetFrameAnimPointCount *undo = new MU_SetFrameAnimPointCount();
	undo->setCount(pointCount,oldCount);
	sendUndo(undo);
}
bool Model::setFrameAnimPointCoords(unsigned anim, unsigned frame, unsigned point,
		double x, double y, double z, Interpolate2020E interp2020)
{
	if(anim<m_frameAnims.size()
		  &&frame<m_frameAnims[anim]->_frame_count()
		  &&point<m_points.size())
	{
		m_changeBits|=MoveOther; //2020

		FrameAnimPoint *fap = nullptr;

		while(point>=m_frameAnims[anim]->m_frameData[frame]->m_framePoints.size())
		{
			fap = FrameAnimPoint::get();
			m_frameAnims[anim]->m_frameData[frame]->m_framePoints.push_back(fap);
		}

		fap = m_frameAnims[anim]->m_frameData[frame]->m_framePoints[point];

		//HACK: Let MU_MoveFramePoint pass setQuickFrameAnimPoint assert. 
		if(InterpolateKeep==interp2020)
		{
			//REMINDER: Assuming "keep" wants a valid interpolation mode.
			interp2020 = fap->m_interp2020[0]?fap->m_interp2020[0]:InterpolateLerp;
		}

		MU_MoveFramePoint::Point p(*fap), o = p;
		p.setPoint(x,y,z,interp2020);

		//TODO: Let sendUndo regect no-op changes.
		if(!memcmp(&p,&o,sizeof(o))) return true; //2020

		m_changeBits|=MoveOther; //2020

		auto undo = new MU_MoveFramePoint;
		undo->setAnimationData(anim,frame);
		//undo->addPoint(point,x,y,z,fap->m_abs[0],fap->m_abs[1],fap->m_abs[2]);
		undo->addPoint(point,p,o);
		sendUndo(undo,true); //2020

		fap->m_abs[0] = x;
		fap->m_abs[1] = y;
		fap->m_abs[2] = z;
		fap->m_interp2020[0] = interp2020;

		invalidateAnim(ANIMMODE_FRAME,anim,frame); //OVERKILL

		return true;
	}
	return false;
}
bool Model::setFrameAnimPointRotation(unsigned anim, unsigned frame, unsigned point,
		double x, double y, double z, Interpolate2020E interp2020)
{
	if(anim<m_frameAnims.size()
		  &&frame<m_frameAnims[anim]->_frame_count()
		  &&point<m_points.size())
	{
		FrameAnimPoint *fap = nullptr;

		while(point>=m_frameAnims[anim]->m_frameData[frame]->m_framePoints.size())
		{
			fap = FrameAnimPoint::get();
			m_frameAnims[anim]->m_frameData[frame]->m_framePoints.push_back(fap);
		}

		fap = m_frameAnims[anim]->m_frameData[frame]->m_framePoints[point];

		//HACK: Let MU_MoveFramePoint pass setQuickFrameAnimPoint assert. 
		if(InterpolateKeep==interp2020)
		{
			//REMINDER: Assuming "keep" wants a valid interpolation mode.
			interp2020 = fap->m_interp2020[1]?fap->m_interp2020[1]:InterpolateLerp;
		}

		MU_MoveFramePoint::Point p(*fap), o = p;
		p.setPointRotation(x,y,z,interp2020);

		//TODO: Let sendUndo regect no-op changes.
		if(!memcmp(&p,&o,sizeof(o))) return true; //2020

		m_changeBits|=MoveOther; //2020

		//MU_RotateFramePoint *undo = new MU_RotateFramePoint;
		auto undo = new MU_MoveFramePoint;
		undo->setAnimationData(anim,frame);		
		//undo->addPointRotation(point,x,y,z,fap->m_rot[0],fap->m_rot[1],fap->m_rot[2]);		
		undo->addPoint(point,p,o);
		sendUndo(undo,true);

		fap->m_rot[0] = x;
		fap->m_rot[1] = y;
		fap->m_rot[2] = z;
		fap->m_interp2020[1] = interp2020;

		invalidateAnim(ANIMMODE_FRAME,anim,frame); //OVERKILL

		return true;
	}
	return false;
}
bool Model::setFrameAnimPointScale(unsigned anim, unsigned frame, unsigned point,
		double x, double y, double z, Interpolate2020E interp2020)
{
	if(anim<m_frameAnims.size()
		  &&frame<m_frameAnims[anim]->_frame_count()
		  &&point<m_points.size())
	{
		FrameAnimPoint *fap = nullptr;

		while(point>=m_frameAnims[anim]->m_frameData[frame]->m_framePoints.size())
		{
			fap = FrameAnimPoint::get();
			m_frameAnims[anim]->m_frameData[frame]->m_framePoints.push_back(fap);
		}

		fap = m_frameAnims[anim]->m_frameData[frame]->m_framePoints[point];

		//HACK: Let MU_MoveFramePoint pass setQuickFrameAnimPoint assert. 
		if(InterpolateKeep==interp2020)
		{
			//REMINDER: Assuming "keep" wants a valid interpolation mode.
			interp2020 = fap->m_interp2020[2]?fap->m_interp2020[2]:InterpolateLerp;
		}

		MU_MoveFramePoint::Point p(*fap), o = p;
		p.setPointScale(x,y,z,interp2020);

		//TODO: Let sendUndo regect no-op changes.
		if(!memcmp(&p,&o,sizeof(o))) return true; //2020

		m_changeBits|=MoveOther; //2020

		//MU_RotateFramePoint *undo = new MU_RotateFramePoint;
		auto undo = new MU_MoveFramePoint;
		undo->setAnimationData(anim,frame);		
		//undo->addPointScale(point,x,y,z,fap->m_rot[0],fap->m_rot[1],fap->m_rot[2]);		
		undo->addPoint(point,p,o);
		sendUndo(undo,true);

		fap->m_xyz[0] = x;
		fap->m_xyz[1] = y;
		fap->m_xyz[2] = z;
		fap->m_interp2020[2] = interp2020;

		invalidateAnim(ANIMMODE_FRAME,anim,frame); //OVERKILL

		return true;
	}
	return false;
}*/

/*REFERENCE
bool Model::setQuickFrameAnimPoint(unsigned anim, unsigned frame, unsigned point,
				double x, double y, double z,
				double rx, double ry, double rz, Interpolate2020E interp2020[2])
{
	if(anim<m_frameAnims.size()
		  &&frame<m_frameAnims[anim]->_frame_count()
		  &&point<m_points.size())
	{
		if(point>=m_frameAnims[anim]->m_frameData[frame]->m_frameVertices.size())
		{
			//REMINDER: setAnimFrameCount covers this too.
			log_warning("resize the animation point list before calling setQuickFrameAnimVertexCoords\n");
			setFrameAnimPointCount(point);
		}

		FrameAnimPoint *fap = m_frameAnims[anim]->m_frameData[frame]->m_framePoints[point];

		fap->m_abs[0] = x;
		fap->m_abs[1] = y;
		fap->m_abs[2] = z;
		fap->m_rot[0] = rx;
		fap->m_rot[1] = ry;
		fap->m_rot[2] = rz;

		if(interp2020) for(int i=2;i-->0;)
		{
			assert(interp2020[i]!=InterpolateKeep);
			fap->m_interp2020[i] = interp2020[i];
		}
		else for(int i=2;i-->0;)
		{
			fap->m_interp2020[i] = InterpolateLerp;
		}

		return true;
	}
	else
	{
		return false;
	}
}*/

/*REFERENCE
Model::Interpolate2020E Model::hasFrameAnimPointCoords(unsigned anim, unsigned frame, unsigned point)const
{
	double _; auto ret = getFrameAnimPointCoords(anim,frame,point,_,_,_);
	return ret>0?ret:InterpolateNone; //YUCK
}
Model::Interpolate2020E Model::getFrameAnimPointCoords(unsigned anim, unsigned frame, unsigned point,
		double &x, double &y, double &z)const
{
	if(anim<m_frameAnims.size()
		  &&frame<m_frameAnims[anim]->_frame_count()
		  &&point<m_frameAnims[anim]->m_frameData[frame]->m_framePoints.size())
	{
		FrameAnimPoint *fap = m_frameAnims[anim]->m_frameData[frame]->m_framePoints[point];
		x = fap->m_abs[0];
		y = fap->m_abs[1];
		z = fap->m_abs[2];
		return fap->m_interp2020[0];
	}
	else if(point<m_points.size()) //REMOVE ME
	{
		x = m_points[point]->m_abs[0];
		y = m_points[point]->m_abs[1];
		z = m_points[point]->m_abs[2];

		//CAUTION: InterpolateVoid is only a nonzero return value for these getters.
		//return true;
		return InterpolateVoid; //YUCK
	}
	//return false;
	return InterpolateNone;
}
Model::Interpolate2020E Model::hasFrameAnimPointRotation(unsigned anim, unsigned frame, unsigned point)const
{
	double _; auto ret = getFrameAnimPointRotation(anim,frame,point,_,_,_);
	return ret>0?ret:InterpolateNone; //YUCK
}
Model::Interpolate2020E Model::getFrameAnimPointRotation(unsigned anim, unsigned frame, unsigned point,
		double &x, double &y, double &z)const
{
	if(anim<m_frameAnims.size()
		  &&frame<m_frameAnims[anim]->_frame_count()
		  &&point<m_frameAnims[anim]->m_frameData[frame]->m_framePoints.size())
	{
		FrameAnimPoint *fap = m_frameAnims[anim]->m_frameData[frame]->m_framePoints[point];
		x = fap->m_rot[0];
		y = fap->m_rot[1];
		z = fap->m_rot[2];
		return fap->m_interp2020[1];
	}
	else if(point<m_points.size()) //REMOVE ME
	{
		x = m_points[point]->m_rot[0];
		y = m_points[point]->m_rot[1];
		z = m_points[point]->m_rot[2];
		//CAUTION: InterpolateVoid is only a nonzero return value for these getters.
		//return true;
		return InterpolateVoid; //YUCK
	}
	//return false;
	return InterpolateNone;
}
Model::Interpolate2020E Model::hasFrameAnimPointScale(unsigned anim, unsigned frame, unsigned point)const
{
	double _; auto ret = getFrameAnimPointScale(anim,frame,point,_,_,_);
	return ret>0?ret:InterpolateNone; //YUCK
}
Model::Interpolate2020E Model::getFrameAnimPointScale(unsigned anim, unsigned frame, unsigned point,
		double &x, double &y, double &z)const
{
	if(anim<m_frameAnims.size()
		  &&frame<m_frameAnims[anim]->_frame_count()
		  &&point<m_frameAnims[anim]->m_frameData[frame]->m_framePoints.size())
	{
		FrameAnimPoint *fap = m_frameAnims[anim]->m_frameData[frame]->m_framePoints[point];
		x = fap->m_xyz[0];
		y = fap->m_xyz[1];
		z = fap->m_xyz[2];
		return fap->m_interp2020[2];
	}
	else if(point<m_points.size()) //REMOVE ME
	{
		x = m_points[point]->m_xyz[0];
		y = m_points[point]->m_xyz[1];
		z = m_points[point]->m_xyz[2];
		//CAUTION: InterpolateVoid is only a nonzero return value for these getters.
		//return true;
		return InterpolateVoid; //YUCK
	}
	//return false;
	return InterpolateNone;
}
void Model::setFrameAnimVertexCount(unsigned vertexCount)
{
	unsigned anim = 0;
	unsigned frame = 0;

	unsigned oldCount = 0;

	unsigned acount = m_frameAnims.size();
	for(anim = 0; anim<acount; anim++)
	{
		unsigned fcount = m_frameAnims[anim]->_frame_count();
		oldCount = m_frameAnims[anim]->m_frameData[0]->m_frameVertices.size();
		for(frame = 0; frame<fcount; frame++)
		{
			//2020
			auto *fd = m_frameAnims[anim]->m_frameData[frame];
			fd->m_frameVertices.reserve(vertexCount);

			FrameAnimVertex *fav = nullptr;
			unsigned vcount = fd->m_frameVertices.size();
			while(vertexCount>vcount)
			{
				fav = FrameAnimVertex::get();
				fd->m_frameVertices.push_back(fav);
				vcount++;
			}
			while(vertexCount<vcount)
			{
				fd->m_frameVertices.back()->release();
				fd->m_frameVertices.pop_back();
				vcount--;
			}
		}
	}

	MU_SetFrameAnimVertexCount *undo = new MU_SetFrameAnimVertexCount();
	undo->setCount(vertexCount,oldCount);
	sendUndo(undo);
}*/

bool Model::setFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
		double x, double y, double z, Interpolate2020E interp2020)
{
	if(anim>=m_frameAnims.size())
	return false;
	auto fa = m_frameAnims[anim];
	const auto fc = fa->_frame_count();
	if(frame>=fc||vertex>=m_vertices.size())
	return false;
		
	//https://github.com/zturtleman/mm3d/issues/90
	m_changeBits|=MoveGeometry;

	const auto fp = fa->m_frame0;
	
	auto list = &m_vertices[vertex]->m_frames[fp];

	FrameAnimVertex *fav = list[frame];

	//HACK: Supply default interpolation mode to any
	//neighboring keyframe
	if(InterpolateKeep==interp2020) if(!fav->m_interp2020)
	{
		for(auto i=frame;++i<fc;)
		if(interp2020=list[i]->m_interp2020) 
		break;		
		if(InterpolateKeep==interp2020)
		if(fa->m_wrap)
		{
			for(unsigned i=0;i<frame;i++)
			if(interp2020=list[i]->m_interp2020) 
			break;
		}
		else for(auto i=frame;i-->0;)
		if(interp2020=list[i]->m_interp2020) 
		break;
		if(InterpolateKeep==interp2020) interp2020 = InterpolateLerp;
	}
	else interp2020 = fav->m_interp2020;

	if(m_undoEnabled)
	{
		auto undo = new MU_MoveFrameVertex;
		undo->setAnimationData(anim,frame);
		undo->addVertex(vertex,x,y,z,interp2020,fav);
		sendUndo(undo,true);
	}

	fav->m_coord[0] = x;
	fav->m_coord[1] = y;
	fav->m_coord[2] = z;
		
	assert(InterpolateKeep!=interp2020);
	fav->m_interp2020 = interp2020;

	invalidateAnim(ANIMMODE_FRAME,anim,frame); //OVERKILL

	return true;
}

bool Model::setQuickFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
		double x, double y, double z, Interpolate2020E interp2020)
{		
	if(anim>=m_frameAnims.size())
	return false;
	auto fa = m_frameAnims[anim];
	const auto fc = fa->_frame_count();
	if(frame>=fc||vertex>=m_vertices.size())
	return false;

	const auto fp = fa->m_frame0;	
	auto list = &m_vertices[vertex]->m_frames[fp];
	FrameAnimVertex *fav = list[frame];

	fav->m_coord[0] = x;
	fav->m_coord[1] = y;
	fav->m_coord[2] = z;

	//invalidateNormals(); //OVERKILL

	assert(interp2020!=InterpolateKeep);
	fav->m_interp2020 = interp2020;

	return true;
}

int Model::copyAnimation(AnimationModeE mode, unsigned index, const char *newName)
{
	AnimBase2020 *ab = _anim(mode,index); if(!ab) return -1;
	//int num = addAnimation(mode,newName); if(num<0) return num;
	int num = getAnimCount(mode);
	AnimBase2020 *ab2 = _dup(mode,ab);
	if(newName) ab2->m_name = newName;

	if(auto fa=ANIMMODE_FRAME==mode?(FrameAnim*)ab:nullptr)
	{	
		//Note, setFrameAnimVertexCoords is unnecessary 
		//because this is a fresh animation
		auto fp = fa->m_frame0;
		auto fd = m_frameAnims[num]->m_frame0;
		auto fc = fa->_frame_count();
		for(auto*ea:m_vertices)
		{
			//Note, it's easier on the cache to do
			//the vertices in the outer loop

			auto p = &ea->m_frames[fp];
			auto d = &ea->m_frames[fd];

			for(unsigned c=fc;c-->0;d++,p++) **d = **p;
		}
	}		

	//2020: If splitAnimation does this then so should copy
	moveAnimation(mode,num,index+1); 
	
	return index+1; //return num;
}

int Model::splitAnimation(AnimationModeE mode, unsigned index, const char *newName, unsigned frame)
{
	AnimBase2020 *ab = _anim(mode,index); if(!ab) return -1;

	if(frame>ab->_frame_count()) return -1; //2020?

	int num = addAnimation(mode,newName); if(num<0) return num;

	AnimBase2020 *ab2;
	if(mode==ANIMMODE_SKELETAL) ab2 = m_skelAnims[num];
	else ab2 = m_frameAnims[num];

	int fc = ab->_frame_count()-frame;
	setAnimFrameCount(mode,num,fc);
	ab2->m_fps = ab->m_fps;
	ab2->m_wrap = ab->m_wrap;
	double ft = ab->m_timetable2020[frame];
	ab2->m_frame2020 = ab->m_frame2020-ft;
	while(fc-->0)
	ab2->m_timetable2020[fc] = ab->m_timetable2020[frame+fc]-ft;		
	
	auto fa = ANIMMODE_FRAME==mode?(FrameAnim*)ab:nullptr;
	for(Position j{fa?PT_Point:PT_Joint,0};j<ab->m_keyframes.size();j++)
	for(auto*kf:ab->m_keyframes[j])
	if(kf->m_frame>=frame)
	{
		setKeyframe(num,kf->m_frame-frame,j,kf->m_isRotation,
		kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
	}

	if(fa) 
	{	
		//Note, setFrameAnimVertexCoords is unnecessary 
		//because this is a fresh animation
		auto fp = fa->m_frame0+frame;
		auto fd = m_frameAnims[num]->m_frame0;
		auto fc = fa->_frame_count();
		for(auto*ea:m_vertices)
		{
			//Note, it's easier on the cache to do
			//the vertices in the outer loop

			auto p = &ea->m_frames[fp];
			auto d = &ea->m_frames[fd];

			for(unsigned c=fc;c-->frame;d++,p++) **d = **p;
		}	
	}	
	
	//NOTE: setAnimTimeFrame fails if the end frames aren't first removed
	//using setAnimFrameCount.
	setAnimFrameCount(mode,index,frame);
	setAnimTimeFrame(mode,index,ft);

	moveAnimation(mode,num,index+1); 
	
	return index+1; //return num;
}

bool Model::joinAnimations(AnimationModeE mode, unsigned anim1, unsigned anim2)
{
	log_debug("join %d anim %d+%d\n",mode,anim1,anim2);
	if(anim1==anim2) return true;

	auto aa = _anim(mode,anim1); //ab1
	auto ab = _anim(mode,anim2); //ab2
	if(!aa||!ab) return false;

	//2020: I'm not sure I've fully tested this code with every
	//possible permutation but it looks fine. If not please fix.
	
	//NOTE: I don't recommend inserting a dummy frame to keep a
	//keyframe from blending with the earlier animation's frame
	//because it's impossible to tease the results apart in the
	//final data set, whereas it's pretty trivial in the editor
	//to insert the dummy frame before or after with copy/paste.

	int fc1 = aa->_frame_count();
	int fc2 = ab->_frame_count();
	double tf = getAnimTimeFrame(mode,anim1);
	int spliced = 0; if(fc1&&fc2)
	{
		//If both animations have frames on the end and beginning
		//they are overlapped and must be merged together somehow.
		double t1 = getAnimFrameTime(mode,anim1,fc1-1);
		double t2 = getAnimFrameTime(mode,anim2,0);
		spliced = t1>=tf&&t2<=0;

		fc1-=spliced; //!
	}	
	int fc = fc1+fc2;
	setAnimFrameCount(mode,anim1,fc);

	//2020: Convert the frames into the correct time scales and 
	//add the durations and timestamps together.
	double r = 1;
	if(aa->m_fps!=ab->m_fps)
	r = aa->m_fps/ab->m_fps;
	setAnimTimeFrame(mode,anim1,tf+r*getAnimTimeFrame(mode,anim2));
	for(int i=spliced,f=fc1+spliced;f<fc;f++)	
	setAnimFrameTime(mode,anim1,f,tf+r*ab->m_timetable2020[i++]);	

	auto fa = ANIMMODE_FRAME==mode?(FrameAnim*)ab:nullptr;
	for(Position j{fa?PT_Point:PT_Joint,0};j<ab->m_keyframes.size();j++)
	for(auto*kf:ab->m_keyframes[j])	
	{
		//HACK: Don't overwrite an existing keyframe with Copy.
		auto e = kf->m_interp2020;
		if(e==InterpolateCopy) e = InterpolateKopy;

		setKeyframe(anim1,kf->m_frame+fc1,j,kf->m_isRotation,
		kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],e);
	}
	
	if(fa) 
	{	
		auto fp = ((FrameAnim*)ab)->m_frame0;
		auto fd = ((FrameAnim*)aa)->m_frame0+fc1;

		bool ue = m_undoEnabled;
		MU_MoveFrameVertex *undo;
		for(unsigned f=0;f<fc2;f++,fp++,fd++)
		{
			if(ue) undo = new MU_MoveFrameVertex;
			if(ue) undo->setAnimationData(anim1,fc1+f);

			int v = -1; for(auto*ea:m_vertices)
			{
				v++; //HACK: Increment always.

				auto p = ea->m_frames[fp], d = ea->m_frames[fd];				

				//Give priority to the second animation since
				//it's easier to implement for keyframes above.
				//If there's no data in that animation keep the
				//first animation's data since that's like above.
				if(!f&&spliced&&p->m_interp2020<=InterpolateCopy)
				{
					if(d->m_interp2020||!p->m_interp2020) continue;
				}

				if(ue) undo->addVertex(v,p->m_coord[0],p->m_coord[1],p->m_coord[2],p->m_interp2020,d,false);			
				*d = *p;
			}

			if(ue) sendUndo(undo,false);
		}
	}	

	deleteAnimation(mode,anim2); return true;
}

bool Model::mergeAnimations(AnimationModeE mode, unsigned anim1, unsigned anim2)
{
	log_debug("merge %d anim %d+%d\n",mode,anim1,anim2);
	if(anim1==anim2) return true;

	auto aa = _anim(mode,anim1); //ab1
	auto ab = _anim(mode,anim2); //ab2
	if(!aa||!ab) return false;

	unsigned fc1 = aa->_frame_count();
	unsigned fc2 = ab->_frame_count();

	/*2020: Doing this interleaved.
	if(fc1!=fc2)
	{
		//str = ::tr("Cannot merge animation %1 and %2,\n frame counts differ.")
		//.arg(model->getAnimName(mode,a)).arg(model->getAnimName(mode,b));
		msg_error(TRANSLATE("LowLevel",
		"Cannot merge animation %d and %d,\n frame counts differ."),anim1,anim2);
		return false;
	}*/

	int_list smap,tmap;
	std::vector<double> ts;
	
	double r = 1;
	if(aa->m_fps!=ab->m_fps)
	r = aa->m_fps/ab->m_fps;
	double s = -1, t = -1;
	for(int i=0,j=0;i<fc1||j<fc2;)
	{
		if(i<fc1) s = aa->m_timetable2020[i];
		if(j<fc2) t = ab->m_timetable2020[j]*r;

		double st; int ii = i, jj = j;

		if(s<t&&s>=0)
		{
			i++; st = s;
		}
		else if(t<s&&t>=0)
		{
			j++; st = t;
		}
		else if(s==t&&t>=0)
		{
			i++; j++; st = t;
		}
		else 
		{
			assert(0); break;
		}

		size_t fc = ts.size();

		if(!fc||st!=ts.back())
		{
			ts.push_back(st);
		}
		else fc--;

		if(ii!=i) smap.push_back(fc);
		if(jj!=j) tmap.push_back(fc);
	}

	//Do this before setAnimFrameTime to be safe.
	double tf1 = getAnimTimeFrame(mode,anim1);
	double tf2 = getAnimTimeFrame(mode,anim2);
	if(tf2>tf1) setAnimTimeFrame(mode,anim1,tf2);

	auto sz = (unsigned)ts.size();
	setAnimFrameCount(mode,anim1,sz);
	while(sz-->0)
	setAnimFrameTime(mode,anim1,sz,ts[sz]);

	//Remap anim1 keyframes to new interleaved times.
	std::vector<Keyframe*> kmap;
	auto fa = ANIMMODE_FRAME==mode?(FrameAnim*)ab:nullptr;
	for(Position j{fa?PT_Point:PT_Joint,0};j<aa->m_keyframes.size();j++)
	for(auto*kf:aa->m_keyframes[j])
	{
		int f = smap[kf->m_frame];

		//HACK: There isn't an undo system for just changing the frame.
		if(!m_undoEnabled)
		{
			kf->m_frame = f;
		}
		else if(kf->m_frame!=f)
		{
			kmap.push_back(kf);
			deleteKeyframe(anim1,kf->m_frame,j,kf->m_isRotation);
		}
	}
	for(auto*kf:kmap) //Use setKeyframe for undo support?
	{
		//FIX ME: This calls for a new ModelUndo object!!

		Position j{fa?PT_Point:PT_Joint,kf->m_objectIndex};
		setKeyframe(anim1,smap[kf->m_frame],j,kf->m_isRotation,
		kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
	}

	//Add anim2 keyframes to anim1
	for(Position j{fa?PT_Point:PT_Joint,0};j<ab->m_keyframes.size();j++)
	for(auto*kf:ab->m_keyframes[j])
	{
		//HACK: Don't overwrite an existing keyframe with Copy.
		auto e = kf->m_interp2020;
		if(e==InterpolateCopy) e = InterpolateKopy;

		setKeyframe(anim1,tmap[kf->m_frame],j,kf->m_isRotation,
		kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],e);
	}

	if(fa) 
	{	
		int fp,fp0 = fa->m_frame0;
		int fd,fd0 = ((FrameAnim*)aa)->m_frame0;
		
		bool ue = m_undoEnabled;
		MU_MoveFrameVertex *undo;

		//NOTE: Forward order is important to
		//not overwrite the source frame data.
		for(auto f=(unsigned)ts.size();f-->0;)
		{
			int fq = -1;
			for(auto&i:smap) if(i==f) 
			{
				fp = fd0+(&i-smap.data());
				fq = fp;
			}
			for(auto&i:tmap) if(i==f) 
			{
				fp = fp0+(&i-tmap.data());
			}
			if(fq==-1) fq = fp; 
			
			fd = fd0+f; if(fp==fd) continue;

			if(ue) undo = new MU_MoveFrameVertex;
			if(ue) undo->setAnimationData(anim1,f);

			int v = -1; for(auto*ea:m_vertices)
			{
				v++; //HACK: Increment always.

				auto p = ea->m_frames[fp], d = ea->m_frames[fd];

				//Give priority to non-Copy data, but prefer
				//Copy to None too.
				//Note, anim2 is given priority since that's
				//how joinAnimations works on the splice. It
				//would be best to offer a parameter perhaps.
				if(p->m_interp2020<=InterpolateCopy&&fq!=fp)
				{
					auto q = ea->m_frames[fq];

					if(q->m_interp2020>p->m_interp2020)
					{
						p = q;
					}
				}

				if(p==d) continue;

				if(ue) undo->addVertex(v,p->m_coord[0],p->m_coord[1],p->m_coord[2],p->m_interp2020,d,false);			

				*d = *p;
			}

			if(ue) sendUndo(undo,false);
		}
	}		

	deleteAnimation(mode,anim2); return true;
}

bool Model::moveAnimation(AnimationModeE mode, unsigned oldIndex, unsigned newIndex)
{
	if(oldIndex==newIndex) return true;

	switch (mode)
	{
	default: return false;

	case ANIMMODE_SKELETAL:

		if(oldIndex<m_skelAnims.size()&&newIndex<m_skelAnims.size())
		{
			SkelAnim *sa = m_skelAnims[oldIndex];
			m_skelAnims.erase(m_skelAnims.begin()+oldIndex);				
			m_skelAnims.insert(m_skelAnims.begin()+newIndex,sa);
		}
		else return false; break;

	case ANIMMODE_FRAME:

		if(oldIndex<m_frameAnims.size()&&newIndex<m_frameAnims.size())
		{
			FrameAnim *fa = m_frameAnims[oldIndex];
			m_frameAnims.erase(m_frameAnims.begin()+oldIndex);				
			m_frameAnims.insert(m_frameAnims.begin()+newIndex,fa);
		}
		else return false; break;
	}

	if(m_undoEnabled)
	{
		auto undo = new MU_MoveAnimation;
		undo->moveAnimation(mode,oldIndex,newIndex);
		sendUndo(undo);
	}
	return true;
}

template<int I> struct model_cmp_t //convertAnimToFrame
{
	double params[3*I]; int diff;

	double *compare(double(&cmp)[3*I], int &prev, int &curr)
	{
		curr = 0;
		for(int i=0;i<I;i++)
		if(memcmp(params+i*3,cmp+i*3,sizeof(*cmp)*3)) 
		curr|=1<<i;
		prev = curr&~diff; diff = curr; return params;
	}
};
int Model::convertAnimToFrame(AnimationModeE mode, unsigned anim, const char *newName, unsigned frameCount, Interpolate2020E how, Interpolate2020E how2)
{
	if(!how2) how2 = how; if(!how) return -1;

	int num = -1;	
	if(mode!=ANIMMODE_SKELETAL||anim>=m_skelAnims.size())
	return num;		
	if((num=addAnimation(ANIMMODE_FRAME,newName))<0)	
	return num;

	setAnimFrameCount(ANIMMODE_FRAME,num,frameCount);
	setAnimWrap(ANIMMODE_FRAME,num,getAnimWrap(ANIMMODE_SKELETAL,anim));

	if(!frameCount) //???
	{
		setAnimFPS(ANIMMODE_FRAME,num,10);
		return num;
	}

	//double time = (m_skelAnims[anim]->_frame_count() *(1.0/m_skelAnims[anim]->m_fps));
	double time = getAnimTimeFrame(mode,anim);
	double fps  = (double)frameCount/time;
	double spf  = time/(double)frameCount;

	log_debug("resampling %d frames at %.2f fps to %d frames at %.2f fps\n",
	m_skelAnims[anim]->_frame_count(),m_skelAnims[anim]->m_fps,frameCount,fps);

	setAnimFPS(ANIMMODE_FRAME,num,fps);

	setCurrentAnimation(ANIMMODE_SKELETAL,anim);

	unsigned vcount = m_vertices.size();
	std::vector<model_cmp_t<1>> cmp; cmp.resize(vcount);
	for(unsigned v=0;v<vcount;v++)
	{
		getVertexCoords(v,cmp[v].params);
		cmp[v].diff = ~0;
	}
	unsigned pcount = m_points.size();
	std::vector<model_cmp_t<3>> cmpt; cmpt.resize(pcount);
	for(unsigned p=0;p<pcount;p++)
	{
		auto params = cmpt[p].params;
		m_points[p]->getParams(params,params+3,params+6);
		cmpt[p].diff = ~0;
	}
	for(unsigned f=0;f<frameCount;f++)
	{
		setCurrentAnimationTime(spf*f);

		for(unsigned v=0;v<vcount;v++)
		{
			double coord[3];
			getVertexCoords(v,coord);
			int prev, curr;
			double *pcoord = cmp[v].compare(coord,prev,curr);
			if(prev&1) setFrameAnimVertexCoords(num,f-1,v,pcoord[0],pcoord[1],pcoord[2],how2);
			if(curr&1) setFrameAnimVertexCoords(num,f,v,coord[0],coord[1],coord[2],how);
			memcpy(pcoord,coord,sizeof(coord));
		}

		for(Position p{PT_Point,0};p<pcount;p++)
		{
			double params[3+3+3];
			m_points[p]->getParams(params,params+3,params+6);
			int prev, curr;
			double *pparams = cmpt[p].compare(params,prev,curr);
			if(prev&1) setKeyframe(num,f-1,p,KeyTranslate,pparams[0],pparams[1],pparams[2],how2);
			if(prev&2) setKeyframe(num,f-1,p,KeyRotate,pparams[3],pparams[4],pparams[5],how2);
			if(prev&4) setKeyframe(num,f-1,p,KeyScale,pparams[6],pparams[7],pparams[8],how2);
			if(curr&1) setKeyframe(num,f,p,KeyTranslate,params[0],params[1],params[2],how);
			if(curr&2) setKeyframe(num,f,p,KeyRotate,params[3],params[4],params[5],how);
			if(curr&4) setKeyframe(num,f,p,KeyScale,params[6],params[7],params[8],how);
			memcpy(pparams,params,sizeof(params));
		}
	}

	setNoAnimation(); //??? //FIX ME

	return num;
}

bool Model::deleteAnimFrame(AnimationModeE m, unsigned anim, unsigned frame)
{
	//TODO: It would be nice if eliminating all of the frame data automatically
	//deleted the frame but perhaps that shouldn't be implemented at this level.
	if(size_t count=getAnimFrameCount(m,anim))
	return setAnimFrameCount(m,anim,(unsigned)count-1,frame,nullptr); return false;
}

int Model::setKeyframe(unsigned anim, unsigned frame, Position pos, KeyType2020E isRotation,
		double x, double y, double z, Interpolate2020E interp2020)
{	
	AnimBase2020 *ab = _anim(anim,frame,pos); if(!ab) return -1;

	//NEW: this saves user code from branching on this case and is analogous
	//to vertex frame data
	if(!interp2020) deleteKeyframe(anim,frame,pos,isRotation);

	if(isRotation<=0||isRotation>KeyScale) return -1;

	m_changeBits|=MoveOther; //2020

	//log_debug("set %s of %d (%f,%f,%f)at frame %d\n",
	//		(isRotation ? "rotation" : "translation"),pos.index,x,y,z,frame);

	//int num = ab->m_keyframes[pos].size();
	auto kf = Keyframe::get();

	kf->m_frame		  = frame;
	kf->m_isRotation	= isRotation;

	bool	isNew = false;
	double old[3] = {};

	unsigned index = 0;
	auto &list = ab->m_keyframes[pos];
	if(list.find_sorted(kf,index))
	{
		isNew = false;

		kf->release();

		kf = list[index];

		memcpy(old,kf->m_parameter,sizeof(old));

		if(x==old[0]&&y==old[1]&&z==old[2])
		{
			if(InterpolateKeep==interp2020
			||kf->m_interp2020==interp2020)
			{
				return true; //2020
			}
		}
		if(InterpolateKopy==interp2020)
		{ 
			assert(kf->m_interp2020);

			return true;
		}
	}
	else
	{
		isNew = true;

		list.insert_sorted(kf);

		// Do lookup to return proper index
		list.find_sorted(kf,index);

		if(InterpolateKopy==interp2020)
		{ 
			interp2020 = InterpolateCopy;
		}
	}
	
	//HACK: Supply default interpolation mode to any
	//neighboring keyframe
	if(InterpolateKeep==interp2020) if(!kf->m_interp2020)
	{
		for(auto i=index;++i<list.size();)
		if(list[i]->m_isRotation==isRotation)		
		if(interp2020=list[i]->m_interp2020) 
		break;		
		if(InterpolateKeep==interp2020)
		if(ab->m_wrap)
		{
			for(unsigned i=0;i<index;i++) 
			if(list[i]->m_isRotation==isRotation)
			if(interp2020=list[i]->m_interp2020) 
			break;
		}
		else for(auto i=index;i-->0;)
		if(list[i]->m_isRotation==isRotation)
		if(interp2020=list[i]->m_interp2020) 
		break;
		if(InterpolateKeep==interp2020)
		interp2020 = InterpolateLerp;
	}
	else interp2020 = kf->m_interp2020;

	auto olde = kf->m_interp2020;
	kf->m_parameter[0] = x;
	kf->m_parameter[1] = y;
	kf->m_parameter[2] = z;
	kf->m_objectIndex	= pos;
//	kf->m_time			= ab->m_spf *frame;
	assert(InterpolateKeep!=interp2020);
	//if(InterpolateKeep!=interp2020)
	kf->m_interp2020 = interp2020;

	if(m_undoEnabled)
	{
		//TEMPORARY FIX
		MU_SetObjectKeyframe *undo;
		if(PT_Joint==pos.type) undo = new MU_SetJointKeyframe;
		if(PT_Point==pos.type) undo = new MU_SetPointKeyframe;

		undo->setAnimationData(anim,frame,isRotation);
		undo->addKeyframe(pos,isNew,x,y,z,interp2020,old[0],old[1],old[2],olde);
		sendUndo(undo);
	}

	//TEMPORARY FIX
	auto mode = pos.type==PT_Joint?ANIMMODE_SKELETAL:ANIMMODE_FRAME;

	invalidateAnim(mode,anim,frame); //OVERKILL

	return index;
}

bool Model::deleteKeyframe(unsigned anim, unsigned frame, Position pos, KeyType2020E isRotation)
{
	AnimBase2020 *ab = _anim(anim,frame,pos); if(!ab) return false;
	
	KeyframeList &list = ab->m_keyframes[pos];
	KeyframeList::iterator it = list.begin();
	for(;it!=list.end();it++)	
	if((*it)->m_frame==frame)
	if((*it)->m_isRotation&isRotation)
	{
		m_changeBits |= MoveOther; //2020

		//log_debug("deleting keyframe for anim %d frame %d joint %d\n",anim,frame,joint,isRotation ? "rotation" : "translation"); //???

		if(m_undoEnabled)
		{
			//TEMPORARY FIX
			MU_DeleteObjectKeyframe *undo;
			if(PT_Joint==pos.type) undo = new MU_DeleteJointKeyframe;
			if(PT_Point==pos.type) undo = new MU_DeletePointKeyframe;

			undo->setAnimationData(anim);
			undo->deleteKeyframe(*it);
			sendUndo(undo);
		}
	}
	//MEMORY LEAK
	//It seems like "false" should be true when not using the "undo" system?		
	return removeKeyframe(anim,frame,pos,isRotation,false);
}

bool Model::insertKeyframe(unsigned anim, PositionTypeE pt, Keyframe *keyframe)
{
	Position pos = {pt,keyframe->m_objectIndex};
	AnimBase2020 *ab = _anim(anim,keyframe->m_frame,pos); if(!ab) return false;

	//log_debug("inserted keyframe for anim %d frame %d joint %d\n",anim,keyframe->m_frame,keyframe->m_objectIndex); //???

	ab->m_keyframes[pos].insert_sorted(keyframe);

	//TEMPORARY FIX
	auto mode = pt==PT_Joint?ANIMMODE_SKELETAL:ANIMMODE_FRAME;

	invalidateAnim(mode,anim,keyframe->m_frame); //OVERKILL

	return true;
}

bool Model::removeKeyframe(unsigned anim, unsigned frame, Position pos, KeyType2020E isRotation, bool release)
{
	AnimBase2020 *ab = _anim(anim,frame,pos); if(!ab) return false;

	auto &list = ab->m_keyframes[pos];
	for(auto it=list.begin();it!=list.end();)
	if((*it)->m_frame==frame)
	{
		//if bad keys aren't removed (they shouldn't be in here) callers
		//(setAnimFrameCount) that expect removal infinite loop
		auto r = (*it)->m_isRotation;
		if(r<=0||r&isRotation)
		{	
			assert(r>0&&r<=KeyScale);

			Keyframe *kf = *it;
			it = list.erase(it);
			if(release) kf->release(); //MEMORY LEAK (when is this ever the case?)
		}
		else it++;
	}
	else it++;
	
	//TEMPORARY FIX
	auto mode = pos.type==PT_Joint?ANIMMODE_SKELETAL:ANIMMODE_FRAME;

	invalidateAnim(ANIMMODE_SKELETAL,anim,frame); //OVERKILL

	return true;
}

void Model::insertFrameAnim(unsigned index, FrameAnim *anim)
{
	LOG_PROFILE();

	if(index<=m_frameAnims.size())
	{
		//2019: MU_AddAnimation?
		m_changeBits|=Model::AddAnimation;

		/*unsigned count = 0;
		std::vector<FrameAnim*>::iterator it;
		for(it = m_frameAnims.begin(); it!=m_frameAnims.end(); it++)
		{
			if(count==index)
			{
				m_frameAnims.insert(it,anim);				
				break;
			}
			count++;
		}*/
		m_frameAnims.insert(m_frameAnims.begin()+index,anim);
	}
	else
	{
		log_error("index %d/%d out of range in insertFrameAnim\n",index,m_frameAnims.size());
	}
}

void Model::removeFrameAnim(unsigned index)
{
	LOG_PROFILE();

	if(index<m_frameAnims.size())
	{
		//2019: MU_AddAnimation?
		m_changeBits|=Model::AddAnimation;

		FrameAnim *fa = m_frameAnims[index];

		/*unsigned num = 0;
		std::vector<FrameAnim*>::iterator it = m_frameAnims.begin();
		while(num<index)
		{
			num++;
			it++;
		}
		m_frameAnims.erase(it);*/
		m_frameAnims.erase(m_frameAnims.begin()+index);

		if(m_animationMode==ANIMMODE_FRAME)
		if(m_frameAnims.empty())
		{
			m_currentAnim = 0; //infinite loop?
		}
		else while(m_currentAnim>=m_frameAnims.size())
		{
			m_currentAnim--;
		}
	}
	else //2019
	{
		log_error("index %d/%d out of range in removeFrameAnim\n",index,m_frameAnims.size());
	}
}

void Model::insertSkelAnim(unsigned index, SkelAnim *anim)
{
	LOG_PROFILE();

	if(index<=m_skelAnims.size())
	{
		//2019: MU_AddAnimation?
		m_changeBits|=Model::AddAnimation;

		/*unsigned count = 0;
		std::vector<SkelAnim*>::iterator it;
		for(it = m_skelAnims.begin(); it!=m_skelAnims.end(); it++)
		{
			if(count==index)
			{
				m_skelAnims.insert(it,anim);				
				break;
			}
			count++;
		}*/
		m_skelAnims.insert(m_skelAnims.begin()+index,anim);
	}
	else
	{
		log_error("index %d/%d out of range in insertSkelAnim\n",index,m_skelAnims.size());
	}
}

void Model::removeSkelAnim(unsigned index)
{
	LOG_PROFILE();

	if(index<m_skelAnims.size())
	{
		//2019: MU_AddAnimation?
		m_changeBits|=Model::AddAnimation;

		/*unsigned num = 0;
		std::vector<SkelAnim*>::iterator it = m_skelAnims.begin();
		while(num<index)
		{
			num++;
			it++;
		}
		m_skelAnims.erase(it);*/
		m_skelAnims.erase(m_skelAnims.begin()+index);

		if(m_animationMode==ANIMMODE_SKELETAL)
		if(m_skelAnims.empty())
		{
			m_currentAnim = 0; //infinite loop?
		}
		else while(m_currentAnim>=m_skelAnims.size())
		{
			m_currentAnim--;
		}
	}
	else //2019
	{
		log_error("index %d/%d out of range in removeSkelAnim\n",index,m_skelAnims.size());
	}
}

void Model::insertFrameAnimData(unsigned frame0, unsigned frames, FrameAnimData *data, FrameAnim *draw)
{
	if(!frames) return;

	FrameAnimVertex **dp,**dpp; if(data&&!data->empty()) 
	{
		dp = data->data(); assert(data->size()==m_vertices.size()*frames);
	}
	else 
	{
		dp = nullptr; if(data) data->reserve(m_vertices.size()*frames);
	}

	for(auto*ea:m_vertices)
	{
		auto it = ea->m_frames.begin()+frame0; if(dp)
		{
			dpp = dp+frames;

			ea->m_frames.insert(it,dp,dpp); dp = dpp;
		}
		else 
		{
			//https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55817
			//it = ea->m_frames.insert(it,frames,nullptr);
			ea->m_frames.insert(it,frames,nullptr);
			it = ea->m_frames.begin()+frame0;

			for(auto i=frames;i-->0;)
			{
				auto fav = FrameAnimVertex::get();

				if(data) data->push_back(fav);

				*it++ = fav; 
			}
		}		
	}

	for(auto*ea:m_frameAnims)
	{
		//Note, draw is required because it's ambiguous when adding to 
		//the back of an animation, since it could be the front of the
		//next animation.

		if(ea->m_frame0>=frame0&&ea!=draw) ea->m_frame0+=frames;
	}
}

void Model::removeFrameAnimData(unsigned frame0, unsigned frames, FrameAnimData *data)
{
	if(!frames) return;

	FrameAnimData::iterator jt; if(data)
	{
		assert(data->empty());

		data->resize(frames*m_vertices.size());

		jt = data->begin();
	}

	for(auto*ea:m_vertices)
	{
		auto it = ea->m_frames.begin()+frame0, itt = it+frames;
		if(data)
		{
			std::copy(it,itt,jt); jt+=frames;
		}
		ea->m_frames.erase(it,itt);		
	}	

	for(auto*ea:m_frameAnims)
	{
		//Note, I don't think this is ambiguous as with insertFrameAnimData
		//but it might be?

		if(ea->m_frame0>frame0) ea->m_frame0-=frames;
	}
}

#endif // MM3D_EDIT

bool Model::setCurrentAnimation(AnimationModeE m, const char *name)
{
	switch(m)
	{
	case ANIMMODE_SKELETAL:
		
		for(size_t t=0;t<m_skelAnims.size();t++)		
		if(m_skelAnims[t]->m_name==name)
		return setCurrentAnimation(m,(unsigned)t);		
		break;

	case ANIMMODE_FRAME:

		for(size_t t=0;t<m_frameAnims.size();t++)		
		if(m_frameAnims[t]->m_name==name)
		return setCurrentAnimation(m,(unsigned)t);		
		break;
	}
	return false;
}

bool Model::setCurrentAnimation(AnimationModeE m, unsigned index)
{
	//NOTE: NOT SETTING THE TIME IS IMPORTANT SO THIS IS ABLE
	//TO RETURN HERE.
	#ifdef MM3D_EDIT
	/*2019: Don't know why this worked like this, but changing
	//animation requires an undo too.
	bool needUndo = (m_animationMode)?false:true;*/
	if(m==m_animationMode&&index==m_currentAnim) //NEW
	return false; 
	const bool needUndo = true;
	#endif // MM3D_EDIT

	AnimationModeE oldMode = m_animationMode;
	int oldAnim = m_currentAnim; //2019
	int oldFrame = m_currentFrame; //2019

	m_animationMode = ANIMMODE_NONE; //???

	m_changeBits |= AnimationSet;
	if(m!=oldMode)
	{
		m_changeBits |= AnimationMode;
	}

	m_validBspTree = false;

	//log_debug("Changing animation from %d to %d\n",oldMode,m); //???

	size_t n = 0; switch(m)
	{
	case ANIMMODE_SKELETAL: n = m_skelAnims.size(); 		
		//if(index<n) //???
		//log_debug("current animation: skeletal '%s'\n",m_skelAnims[index]->m_name.c_str());
		break;
	case ANIMMODE_FRAME: n = m_frameAnims.size(); 
		//if(index<n) //???
		//log_debug("current animation: frame '%s'\n",m_frameAnims[index]->m_name.c_str());
		break;
	}

	m_currentAnim = index<n?index:0;
	m_animationMode = m;
	m_currentTime = 0;
	m_currentFrame = 0;

	#ifdef MM3D_EDIT
	if(needUndo&&m_undoEnabled)
	{
		MU_ChangeAnimState *undo = new MU_ChangeAnimState();
		//undo->setState(m_animationMode,ANIMMODE_NONE,m_currentAnim,m_currentFrame);
		undo->setState(oldMode,oldAnim,oldFrame,*this);
		sendUndo(undo);
	}
	//2019: Inappropriate/unexpected???
	//updateObservers(false);
	#endif // MM3D_EDIT

	//2020
	//HACK: setCurrentAnimationTime builds its normals from the
	//base model's flat normals.
	if(!oldMode&&m) validateNormals();
	
	if(index>=n) m = ANIMMODE_NONE;
	for(auto*ea:m_vertices) ea->_source(m);
	for(auto*ea:m_triangles) ea->_source(m);
	for(auto*ea:m_points) ea->_source(m);
	for(auto*ea:m_joints) ea->_source(m);

	//SelectTool is failing to find bones? Might want to do this
	//unconditionally, but setCurrentAnimationTime is currently
	//required to get animation working properly (leave the time
	//up to the user instead of setting it to 0 here.)
	if(!m) calculateSkel();

	return m!=ANIMMODE_NONE;
}
void Model::Vertex::_source(AnimationModeE m)
{
	m_absSource = m?m_kfCoord:m_coord;
}
void Model::Triangle::_source(AnimationModeE m)
{
	m_flatSource = m?m_kfFlatNormals:m_flatNormals;
	for(int i=3;i-->0;) 
	m_normalSource[i] = m?m_kfNormals[i]:m_finalNormals[i];
	m_angleSource = m?m_kfVertAngles:m_vertAngles;
}
void Model::Point::_source(AnimationModeE m)
{
	m_absSource = m?m_kfAbs:m_abs;
	m_rotSource = m?m_kfRot:m_rot;
	m_xyzSource = m?m_kfXyz:m_xyz;
}
void Model::Joint::_source(AnimationModeE m)
{
	bool mm = m==ANIMMODE_SKELETAL;
	m_absSource = mm?m_kfAbs():m_abs;
	m_rotSource = mm?m_kfRot:m_rot;
	m_xyzSource = mm?m_kfXyz:m_xyz;
}

bool Model::setCurrentAnimationFrame(unsigned frame, AnimationTimeE calc)
{	
	auto ab = _anim(m_animationMode,m_currentAnim);

	if(!ab||frame>=ab->_frame_count()) return false;

	double t = ab->m_timetable2020[frame];

	if(setCurrentAnimationFrameTime(ab->m_timetable2020[frame],calc))
	{
		assert(m_currentFrame==frame);

		return true;
	}
	else return false;
}

bool Model::setCurrentAnimationTime(double seconds, int loop, AnimationTimeE calc)
{
	double t = seconds*getAnimFPS(m_animationMode,m_currentAnim);
	double len = getAnimTimeFrame(m_animationMode,m_currentAnim);
	//while(t>=len)
	while((float)t>len) //Truncate somewhat.
	{
		switch(loop) //2019
		{
		case -1:

			if(!getAnimWrap(m_animationMode,m_currentAnim))
			{
			case 0: return false;
			}
				
		default: case 1: break;
		}
		t-=len;
	}	
	return setCurrentAnimationFrameTime(t,calc);
}

bool Model::setCurrentAnimationFrameTime(double time, AnimationTimeE calc)
{
	auto anim = m_currentAnim;
	auto am = m_animationMode; if(!am) return false;
	
	/*2020
	//This should be based on getAnimTimeFrame but
	//should time be limited?
	if(am==ANIMMODE_FRAME&&anim<m_frameAnims.size())
	{
		auto *fa = m_frameAnims[anim];

		//validateFrameNormals(anim); //???

		size_t totalFrames = fa->_frame_count();

		if(!totalFrames) time = 0; //???
	}
	else if(am==ANIMMODE_SKELETAL&&anim<m_skelAnims.size())
	{
		SkelAnim *sa = m_skelAnims[anim];

		if(!sa->_frame_count()) time = 0;
	}
	else time = 0;*/
	if(time>0) //Best practice?
	time = std::min(time,getAnimTimeFrame(am,anim));
	else time = 0;
	
	m_changeBits |= AnimationFrame;

	m_validBspTree = false;

	//m_currentFrame = (unsigned)(frameTime/spf);
	m_currentFrame = time?getAnimFrame(am,anim,time):0;
	m_currentTime = time;
	
	if(calc!=AT_invalidateAnim) 
	{
		invalidateAnimSkel(); //NEW
		calculateAnim();
	}
	else invalidateAnim();

	//HACK: When this is called more rapidly than the animation is
	//rendered it's a performance problem to calculate the normals.
	if(calc==AT_calculateNormals) 
	{
		calculateNormals(); 
	}
	else //Concerned about BSP pathway.
	{
		//TODO: Maybe only invalidate flat normals for BSP pathway.
		invalidateAnimNormals();
	}

	//2019: Inappropriate/unexpected???
	//updateObservers(false);
	return true;
}
void Model::invalidateSkel()
{	
	if(inSkeletalMode()) m_validAnimJoints = false;

	m_validJoints = false; 
}
void Model::invalidateAnim()
{
	m_validAnim = m_validAnimJoints = false;
}
void Model::invalidateAnim(AnimationModeE a, unsigned b, unsigned c)
{
	if(a==m_animationMode&&b==m_currentAnim)
	{	
		if(m_currentFrame-c<=1) invalidateAnim();
	}
}
void Model::validateSkel()const
{	
	if(!m_validJoints) 
	const_cast<Model*>(this)->calculateSkel();
}
void Model::validateAnim()const
{
	if(!m_validAnim)
	const_cast<Model*>(this)->calculateAnim();
}
void Model::validateAnimSkel()const
{
	validateSkel();
	if(!m_validAnimJoints)
	const_cast<Model*>(this)->calculateAnimSkel();
}

void Model::calculateSkel()
{	
	LOG_PROFILE();

	m_validJoints = true;

	//if(m_animationMode) return; //REMOVE ME?

	if(inSkeletalMode()) //2020
	{
		invalidateAnim(); //invalidateNormals?
	}

	//log_debug("validateSkel()\n"); //???

	/*REMOVE ME
	//Assuming addAnimation covers this for loaders
	for(auto*sa:m_skelAnims)
	sa->m_keyframes.resize(m_joints.size());*/

	for(unsigned j = 0; j<m_joints.size(); j++)
	{
		Joint *joint = m_joints[j];
		joint->m_relative.loadIdentity();
		joint->m_relative.setRotation(joint->m_rot);
		joint->m_relative.scale(joint->m_xyz);
		joint->m_relative.setTranslation(joint->m_rel);

		if(joint->m_parent>=0) // parented?
		{
			joint->m_absolute = joint->m_relative * m_joints[joint->m_parent]->m_absolute;
		}
		else joint->m_absolute = joint->m_relative;

		//TODO: Is it possible to build m_inv here with relative ease?
		//Can Euler be trivially inverted?
		joint->m_absolute_inverse = false; //2020

		//REMINDER: This matrix is going to be animated. Not sure why it's
		//set here.
		joint->m_final = joint->m_absolute;
//		log_debug("\n");
//		log_debug("Joint %d:\n",j);
//		joint->m_final.show();
//		log_debug("local rotation: %.2f %.2f %.2f\n",
//				joint->m_rot[0],joint->m_rot[1],joint->m_localRotation[2]);
//		log_debug("\n");

		//NEW: For object system. Will try to refactor this at some point.
		joint->m_absolute.getTranslation(joint->m_abs);
	}
}
void Model::calculateAnimSkel()
{
	m_validAnimJoints = true;

	auto anim = m_currentAnim;
	auto am = m_animationMode;

	invalidateAnimNormals();

	//const unsigned f = getAnimFrame(am,anim,t);
	const unsigned f = m_currentFrame;

	//double t = m_currentTime*getAnimFPS(am,anim);
	double t = m_currentTime;

	if(am==ANIMMODE_SKELETAL&&anim<m_skelAnims.size())
	{
		LOG_PROFILE(); //????

		validateSkel();

		SkelAnim *sa = m_skelAnims[anim];

		Matrix transform;
		auto jN = m_joints.size();
		for(Position j{PT_Joint,0};j<jN;j++)
		{
			double trans[3],rot[3],scale[3];
			//interpSkelAnimKeyframeTime(anim,frameTime,sa->m_wrap,j,transform);
			int ch = interpKeyframe(anim,f,t,j,trans,rot,scale);
			transform.loadIdentity();		
			if(ch&KeyRotate)
			transform.setRotation(rot); 
			if(ch&KeyScale)
			transform.scale(scale);
			if(ch&KeyTranslate)
			transform.setTranslation(trans);

			//FIX ME: What if a parent is later in the joint list?

			int jp = m_joints[j]->m_parent;			
			Matrix &b = m_joints[j]->m_relative;
			Matrix &c = jp>=0?m_joints[jp]->m_final:m_localMatrix;

			m_joints[j]->m_final = transform * b * c;

			//LOCAL or GLOBAL? (RELATIVE or ABSOLUTE?)
			//https://github.com/zturtleman/mm3d/issues/35
			//NEW: For object system. Will try to refactor this at some point.
			//transform.getRotation(m_joints[j]->m_kfRot);
			memcpy(m_joints[j]->m_kfRel,trans,sizeof(trans)); //relative?
			memcpy(m_joints[j]->m_kfRot,rot,sizeof(rot)); //relative?
			memcpy(m_joints[j]->m_kfXyz,scale,sizeof(scale)); //relative?
		}
	}
}
void Model::calculateAnim()
{
	m_validAnim = true;

	auto anim = m_currentAnim;
	auto am = m_animationMode;

	if(!am) return;

	invalidateAnimNormals();

	//const unsigned f = getAnimFrame(am,anim,t);
	const unsigned f = m_currentFrame;

	//double t = m_currentTime*getAnimFPS(am,anim);
	double t = m_currentTime;

	if(am==ANIMMODE_FRAME&&anim<m_frameAnims.size())
	{
		auto vN = m_vertices.size();
		for(unsigned v=0;v<vN;v++)
		{
			auto &vt = *m_vertices[v];
			interpKeyframe(anim,f,t,v,vt.m_kfCoord);
		}
		auto jN = m_points.size();
		for(Position j{PT_Point,0};j<jN;j++)
		{
			auto &pt = *m_points[j];
			interpKeyframe(anim,f,t,j,pt.m_kfAbs,pt.m_kfRot,pt.m_kfXyz);
		}
	}
	else if(am==ANIMMODE_SKELETAL&&anim<m_skelAnims.size())
	{
		//NOTE: This exists so when valid joint (m_final) data
		//is required but vertex data isn't the vertex data can
		//wait until it's required (e.g. draw)
		validateAnimSkel();

		for(unsigned v=m_vertices.size();v-->0;)
		{
			m_vertices[v]->_calc_influences(*this);
		}

		for(unsigned p=m_points.size();p-->0;)
		{
			m_points[p]->_calc_influences(*this);
		}
	}
}
void Model::Vertex::_calc_influences(Model &model)
{
	if(!m_influences.empty())
	{
		m_kfCoord[0] = 0.0;
		m_kfCoord[1] = 0.0;
		m_kfCoord[2] = 0.0;

		double total = 0.0;

		Vector vert;

		infl_list::iterator it;
		for(it = m_influences.begin(); it!=m_influences.end(); it++)
		{
			if(it->m_weight>0.00001)
			{
				auto *jp = model.m_joints[(*it).m_boneId];

				const Matrix &final = jp->m_final;
				const Matrix &abs = jp->m_absolute;

				vert.setAll(m_coord);
				vert[3] = 1.0;

				abs.inverseTranslateVector((double *)vert.getVector());
				abs.inverseRotateVector((double *)vert.getVector());

				vert.transform(final);

				vert.scale3((*it).m_weight);
				m_kfCoord[0] += vert[0];
				m_kfCoord[1] += vert[1];
				m_kfCoord[2] += vert[2];

				total += (*it).m_weight;
			}
		}

		if(total>0.00001)
		{
			m_kfCoord[0] /= total;
			m_kfCoord[1] /= total;
			m_kfCoord[2] /= total;
		}
		else goto uninfluenced; //zero divide
	}
	else uninfluenced:
	{
		m_kfCoord[0] = m_coord[0];
		m_kfCoord[1] = m_coord[1];
		m_kfCoord[2] = m_coord[2];
	}
}
void Model::Point::_calc_influences(Model &model)
{
	//ALGORITHM
	//The hell!
	if(!m_influences.empty())
	{
		m_kfAbs[0] = 0;
		m_kfAbs[1] = 0;
		m_kfAbs[2] = 0;

		double axisX[3] = { 0,0,0 };
		double axisY[3] = { 0,0,0 };
		double axisZ[3] = { 0,0,0 };

		Vector ax;
		Vector ay;
		Vector az;

		double total = 0.0;

		Vector vert;
		Vector rot;

		//FIX ME		
		Matrix m,mm = getMatrixUnanimated();
		for(auto&ea:m_influences)
		{
			auto *jp = model.m_joints[ea.m_boneId];

			const Matrix &final = jp->m_final;
			const Matrix &inv = jp->getAbsoluteInverse(); 

			m = mm * inv * final;

			vert[0] = m.get(3,0);
			vert[1] = m.get(3,1);
			vert[2] = m.get(3,2);

			for(int i = 0; i<3; i++)
			{
				ax[i] = m.get(0,i);
				ay[i] = m.get(1,i);
				az[i] = m.get(2,i);
			}

			vert.scale3(ea.m_weight);

			ax.scale3(ea.m_weight);
			ay.scale3(ea.m_weight);
			az.scale3(ea.m_weight);

			m_kfAbs[0] += vert[0];
			m_kfAbs[1] += vert[1];
			m_kfAbs[2] += vert[2];

			for(int j = 0; j<3; j++)
			{
				axisX[j] += ax[j];
				axisY[j] += ay[j];
				axisZ[j] += az[j];
			}

			total += ea.m_weight;
		}
		
		if(total>0.0)
		{
			total = 1/total; //2020

			m_kfAbs[0] *= total;
			m_kfAbs[1] *= total;
			m_kfAbs[2] *= total;
		}
		else //zero divide
		{
			/*????
			m_kfAbs[0] = 0.0;
			m_kfAbs[1] = 0.0;
			m_kfAbs[2] = 0.0;
			m_kfRot[0] = 0.0;
			m_kfRot[1] = 0.0;
			m_kfRot[2] = 0.0;
			*/
			goto uninfluenced2; //2020
		}

		//FIX ME
		m_kfXyz[0] = normalize3(axisX)*total;
		m_kfXyz[1] = normalize3(axisY)*total;
		m_kfXyz[2] = normalize3(axisZ)*total;
		for(int i = 0; i<3; i++)
		{
			m.set(0,i,axisX[i]);
			m.set(1,i,axisY[i]);
			m.set(2,i,axisZ[i]);
		}
		m.getRotation(m_kfRot);
	}
	else uninfluenced2:
	{
		for(int i=3;i-->0;)
		{
			m_kfAbs[i] = m_abs[i];
			m_kfRot[i] = m_rot[i];
			m_kfXyz[i] = m_xyz[i];
		}
	}
}

unsigned Model::getCurrentAnimation()const
{
	return m_currentAnim;
}

unsigned Model::getCurrentAnimationFrame()const
{
	return m_currentFrame;
}

double Model::getCurrentAnimationTime()const
{
	//return m_currentTime;
	return m_currentTime/getAnimFPS(m_animationMode,m_currentAnim);
}

double Model::getCurrentAnimationFrameTime()const //2020
{
	/*
	//Assuming animations selection states valid!
	switch(m_animationMode)
	{
	case ANIMMODE_SKELETAL:		
	return m_skelAnims[m_currentAnim]->m_timetable2020[m_currentFrame];
	case ANIMMODE_FRAME:
	return m_frameAnims[m_currentAnim]->m_frameData[m_currentFrame]->m_frame2020;
	}
	return 0;*/
	return m_currentTime;
}

void Model::setNoAnimation()
{
	auto oldMode = m_animationMode;
	if(oldMode==ANIMMODE_NONE) return; //2019
	
	m_changeBits |= AnimationSet;
	m_changeBits |= AnimationMode;
	m_validBspTree = false;
	
	m_animationMode = ANIMMODE_NONE; //2019
#ifdef MM3D_EDIT
	//if(m_animationMode) //2019
	{
		if(m_undoEnabled)
		{
			MU_ChangeAnimState *undo = new MU_ChangeAnimState();
			//undo->setState(ANIMMODE_NONE,m_animationMode,m_currentAnim,m_currentFrame);
			undo->setState(oldMode,m_currentAnim,m_currentFrame,*this);
			sendUndo(undo);
		}
	}
#endif // MM3D_EDIT
	
	for(auto*ea:m_vertices) ea->_source(ANIMMODE_NONE);
	for(auto*ea:m_triangles) ea->_source(ANIMMODE_NONE);
	for(auto*ea:m_points) ea->_source(ANIMMODE_NONE);
	for(auto*ea:m_joints) ea->_source(ANIMMODE_NONE);

	//SelectTool is failing to find bones?
	calculateSkel();

	//Not sure what's best in this case?
	//if(1) calculateNormals(); else m_validNormals = false;
	validateNormals();
}

unsigned Model::getAnimCount(AnimationModeE m)const
{
	switch(m)
	{
	case ANIMMODE_SKELETAL: return (unsigned)m_skelAnims.size();
	case ANIMMODE_FRAME: return (unsigned)m_frameAnims.size();
	default: return 0;
	}
}
const char *Model::getAnimName(AnimationModeE m, unsigned anim)const
{
	if(auto*ab=_anim(m,anim)) 
	return ab->m_name.c_str(); return nullptr;
}

unsigned Model::getAnimFrameCount(AnimationModeE m, unsigned anim)const
{
	if(auto*ab=_anim(m,anim))
	return ab->_frame_count(); return 0;
}

unsigned Model::getAnimFrame(AnimationModeE m, unsigned anim, double t)const
{
	if(auto*ab=_anim(m,anim)) 
	for(auto&tt=ab->m_timetable2020;!tt.empty();)
	{
		auto lb = std::upper_bound(tt.begin(),tt.end(),t);
		return unsigned(lb==tt.begin()?0:lb-tt.begin()-1);
	}
	return 0; //return -1; //insertAnimFrame expects zero?
}

double Model::getAnimFPS(AnimationModeE m, unsigned anim)const
{
	AnimBase2020 *ab = _anim(m,anim); return ab?ab->m_fps:0;
}

bool Model::getAnimWrap(AnimationModeE m, unsigned anim)const
{
	AnimBase2020 *ab = _anim(m,anim); return ab?ab->m_wrap:false; //true
}

double Model::getAnimTimeFrame(AnimationModeE m, unsigned anim)const
{
	AnimBase2020 *ab = _anim(m,anim); return ab?ab->_time_frame():0;
}
double Model::getAnimFrameTime(AnimationModeE m, unsigned anim, unsigned frame)const
{
	if(auto*ab=_anim(m,anim)) 
	return ab->_frame_time(frame); return 0;
}

Model::Interpolate2020E Model::hasFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex)const
{
	double _; auto ret = getFrameAnimVertexCoords(anim,frame,vertex,_,_,_);
	return ret>0?ret:InterpolateNone; //YUCK
}
Model::Interpolate2020E Model::getFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
		double &x, double &y, double &z)const
{
	if(vertex<m_vertices.size())
	if(anim<m_frameAnims.size()&&frame<m_frameAnims[anim]->_frame_count())
	{
		auto fp = m_frameAnims[anim]->m_frame0+frame;
		FrameAnimVertex *fav = m_vertices[vertex]->m_frames[fp];
		x = fav->m_coord[0];
		y = fav->m_coord[1];
		z = fav->m_coord[2];
		return fav->m_interp2020;
	}
	else //REMOVE ME
	{
		x = m_vertices[vertex]->m_coord[0];
		y = m_vertices[vertex]->m_coord[1];
		z = m_vertices[vertex]->m_coord[2];

		//CAUTION: InterpolateVoid is only a nonzero return value for these getters.
		//return true;
		return InterpolateVoid; //YUCK
	}
	//return false;
	return InterpolateNone; 
}

Model::Interpolate2020E Model::hasKeyframe(unsigned anim, unsigned frame,
		Position pos, KeyType2020E isRotation)const
{
	AnimBase2020 *ab = _anim(anim,frame,pos,false); if(!ab) return InterpolateNone;

	//Keyframe *kf = Keyframe::get(); //STUPID
	Keyframe kf;

	kf.m_frame		  = frame;
	kf.m_isRotation	= isRotation;

	unsigned index;
	Interpolate2020E found = InterpolateNone;
	auto &c = ab->m_keyframes[pos];
	switch(isRotation)
	{
	case KeyTranslate:
	case KeyRotate:
	case KeyScale:

		if(c.find_sorted(&kf,index))
		{
			found = c[index]->m_interp2020;
		}
		break;

	default:

		auto er = std::equal_range(c.begin(),c.end(),&kf,
		[](Keyframe *a, Keyframe *b)->bool{ return a->m_frame<b->m_frame; });
		while(er.first<er.second)
		if((*er.first++)->m_isRotation&isRotation)
		{
			found = std::max(found,(er.first[-1]->m_interp2020));
		}
		break;
	}

	//kf->release();

	return found;	
}

Model::Interpolate2020E Model::getKeyframe(unsigned anim, unsigned frame,
		Position pos, KeyType2020E isRotation,
		double &x, double &y, double &z)const
{
	AnimBase2020 *ab = _anim(anim,frame,pos,false); if(!ab) return InterpolateNone;

	//Keyframe *kf = Keyframe::get(); //STUPID
	Keyframe kf; 

	kf.m_frame		  = frame;
	kf.m_isRotation	= isRotation;

	unsigned index;
	Interpolate2020E found = InterpolateNone;
	if(ab->m_keyframes[pos].find_sorted(&kf,index))
	{
		auto &f = *ab->m_keyframes[pos][index];

//			log_debug("found keyframe anim %d,frame %d,joint %d,%s\n",
//					anim,frame,joint,isRotation ? "rotation" : "translation");
		x = f.m_parameter[0];
		y = f.m_parameter[1];
		z = f.m_parameter[2];

		//FIX ME: Needs InterpolateStep/InterpolateNone options.
		found = f.m_interp2020; 
	}
	else
	{
		//log_debug("could not find keyframe anim %d,frame %d,joint %d,%s\n",
		//		anim,frame,joint,isRotation ? "rotation" : "translation");
	}
	//kf->release();

	return found;
}

/*REFERENCE
bool Model::interpSkelAnimKeyframe(unsigned anim, unsigned frame,
		bool loop, unsigned j, KeyType2020E isRotation,
		double &x, double &y, double &z)const
{
	if(anim<m_skelAnims.size()
		  &&frame<m_skelAnims[anim]->_frame_count()
		  &&j<m_joints.size())
	{
		SkelAnim *sa = m_skelAnims[anim];
		double time = sa->m_timetable2020[frame];

		Matrix relativeFinal;
		bool rval = interpSkelAnimKeyframeTime(anim,time,loop,j,
				relativeFinal);

		switch(isRotation)
		{
		case KeyRotate:

			relativeFinal.getRotation(x,y,z); break;

		case KeyTranslate:

			relativeFinal.getTranslation(x,y,z); break;
		}

		return rval;
	}
	else
	{
		log_error("anim keyframe out of range: anim %d,frame %d,joint %d\n",anim,frame,j);
		return false;
	}
}
bool Model::interpSkelAnimKeyframeTime(unsigned anim, double frameTime,
		bool loop, unsigned j, Matrix &transform)const
{
	//2020: Historically frameTime unit is seconds!!
	//frameTime*=sa->m_fps;
	//double totalTime = sa->m_spf*sa->_frame_count();
	frameTime*=getAnimFPS(ANIMMODE_SKELETAL,anim);
	double totalTime = getAnimTimeFrame(ANIMMODE_SKELETAL,anim);
	while(frameTime>totalTime)
	{
		if(loop) frameTime-=totalTime;
		else return false;
	}
	unsigned f = getAnimFrame(ANIMMODE_SKELETAL,anim,frameTime);
	return interpKeyframe(anim,f,frameTime,j,transform); 
}*/

int Model::interpKeyframe(unsigned anim, unsigned frame,
	unsigned pos, double trans[3])const
{
	auto ab = _anim(ANIMMODE_FRAME,anim);
	return interpKeyframe(anim,frame,ab?ab->_frame_time(frame):0,pos,trans); 
}
int Model::interpKeyframe(unsigned anim, unsigned frame,
	Position pos, Matrix &relativeFinal)const
{
	auto ab = _anim(pos.type==PT_Joint?ANIMMODE_SKELETAL:ANIMMODE_FRAME,anim);
	return interpKeyframe(anim,frame,ab?ab->_frame_time(frame):0,pos,relativeFinal); 
}
int Model::interpKeyframe(unsigned anim, unsigned frame,
	Position pos, double trans[3], double rot[3], double scale[3])const
{
	auto ab = _anim(pos.type==PT_Joint?ANIMMODE_SKELETAL:ANIMMODE_FRAME,anim);
	return interpKeyframe(anim,frame,ab?ab->_frame_time(frame):0,pos,trans,rot,scale); 
}
int Model::interpKeyframe(unsigned anim, unsigned frame, double time,
		Position pos, Matrix &transform)const
{
	transform.loadIdentity();
	double trans[3], rot[3], scale[3];
	if(int ch=interpKeyframe(anim,frame,time,pos,trans,rot,scale))
	{		
		if(ch&KeyRotate)
		transform.setRotation(rot); 
		if(ch&KeyScale)
		transform.scale(scale);
		if(ch&KeyTranslate)
		transform.setTranslation(trans); 
		return ch;
	}
	return 0;
}
int Model::interpKeyframe(unsigned anim, unsigned frame, double time,
		Position pos, double trans[3], double rot[3], double scale[3])const
{
	int mask = 0;
	if(trans) mask|=KeyTranslate;
	if(rot) mask|=KeyRotate; 
	if(scale) mask|=KeyScale;
	mask = ~mask;
	
	int ret = 0; if(pos.type==PT_Vertex) 
	{
		ret = interpKeyframe(anim,frame,pos,trans);
	}
	else if(AnimBase2020*ab=_anim(anim,frame,pos,false))
	{
		auto &jk = ab->m_keyframes[pos]; 
		auto &tt = ab->m_timetable2020;
	   
		//TODO: Binary search? //FIX ME
		unsigned first[3] = {};
		unsigned key[3] = {}, stop[3] = {};
		unsigned last[3] = {};
		for(unsigned k=0;k<jk.size();)
		{
			auto cmp = jk[k++];

			if(mask&cmp->m_isRotation) continue;

			int i = cmp->m_isRotation>>1;

			//if(cmp->m_time<=time)
			if(cmp->m_frame<=frame)
			{
				if(!first[i]) first[i] = k; 

				// Less than current time
				// get latest keyframe for rotation and translation
				key[i] = k;
			}
			else
			{
				// Greater than current time
				// get earliest keyframe for rotation and translation
				if(!stop[i])
				{
					if(key[i]) 
					{						
						if(time<=tt[jk[key[i]-1]->m_frame])
						{
							stop[i] = key[i]; 
						}
						else stop[i] = k;
					}
					else stop[i] = key[i] = k;
				}

				last[i] = k;
			}
		}

		bool wrap = ab->m_wrap;
	
		for(int i=0;i<3;i++) if(auto k=key[i])
		{
			ret|=1<<i;

			if(!stop[i]) stop[i] = (wrap?first:key)[i]; 
	
			auto p = jk[k-1];
			auto d = jk[stop[i]-1];
			double t = 0, cmp = tt[p->m_frame];
			const double *dp,*pp = p->m_parameter;
			//RATIONALE: The mode comes from the later keyframe because
			//there are not modes associated with the base model's data.
			//If this is unconventional importers should add end frames.
			auto e = d->m_interp2020;

			bool lerp = e==InterpolateLerp;

			//TODO: setKeyframe might fill these out instead of looking
			//this up here. Unlike vertex-data there's no memory saving.
			if(InterpolateCopy==p->m_interp2020)
			{
				auto tf = p->m_isRotation;
				while(k-->0)
				{			
					if(tf==jk[k]->m_isRotation)			
					if(jk[k]->m_interp2020>InterpolateCopy)
					{
						pp = jk[k]->m_parameter; break;
					}
				}
				if(k==-1) if(wrap) 
				{
					for(k=last[i];k-->0;)				
					if(tf==jk[k]->m_isRotation)
					if(jk[k]->m_interp2020>InterpolateCopy)
					{
						pp = jk[k]->m_parameter; break;
					}
				}				

				if(pp==p->m_parameter)
				{
					pp = getPositionObject(pos)->getParamsUnanimated((Interpolant2020E)i);
				}

				if(!lerp) d = p;
			}		
			else if(InterpolateCopy==e)
			{
				d = p;
			}

			if(time!=cmp) if(time>cmp||wrap)
			{			
				if(lerp&&p!=d)
				{
					dp = d->m_parameter;
					double diff = tt[d->m_frame]-cmp;
					if(diff<0) diff+=ab->_time_frame();
					t = (time-cmp)/diff;
				}
			}
			else //Clamping?
			{
				//Note: There is no extrapolation after the final frame
				//since older MM3D files used step mode and fixed count.

				dp = pp;
				pp = getPositionObject(pos)->getParamsUnanimated((Interpolant2020E)i);

				t = lerp?time/cmp:0;
			}

			if(i!=InterpolantRotation)
			{
				double *x = i?scale:trans;			
				if(t)
				{
					Vector va(pp);
					//if(t) //interpolate?
					va+=(Vector(dp)-va)*t;
					va.getVector3(x);
				}
				else memcpy(x,pp,sizeof(*x)*3);
			}
			else if(t) //InterpolantRotation
			{
				Quaternion va; va.setEulerAngles(pp);
				Quaternion vb; vb.setEulerAngles(dp);

				#ifdef NDEBUG
				#error Should use slerp algorithm!
				https://github.com/zturtleman/mm3d/issues/125
				#endif				

				// Negate if necessary to get shortest rotation path for
				// interpolation
				if(va.dot4(vb)<-0.00001) 
				{
					//NOTE: It's odd to negate all four since that's the
					//same rotation but represented by different figures.
					//I couldn't find examples so I checked it to see it
					//works for the below math.
					for(int i=0;i<4;i++) vb[i] = -vb[i];
				}								
				va = va*(1-t)+vb*t; va.normalize();

				va.getEulerAngles(rot);
			}
			else memcpy(rot,pp,sizeof(*rot)*3);
		}
	}	
	mask|=ret;

	for(int i=1;i<=4;i<<=1) if(~mask&i)
	if(auto pt=pos.type!=PT_Joint?getPositionObject(pos):nullptr)
	{
		switch(i)
		{
		case 1: memcpy(trans,pt->m_abs,sizeof(*trans)*3); break;
		case 2: memcpy(rot,pt->m_rot,sizeof(*rot)*3); break;
		case 4: memcpy(scale,pt->m_xyz,sizeof(*scale)*3); break;
		}		
	}
	else switch(i)
	{
	case 1: memset(trans,0x00,sizeof(*trans)*3); break;
	case 2: memset(rot,0x00,sizeof(*rot)*3); break;
	case 4: for(int i=3;i-->0;) scale[i] = 1; break;
	}	

	return ret;	
}
int Model::interpKeyframe(unsigned anim, unsigned frame, double time,
		unsigned pos, double trans[3])const
{
	//REMINDER: I've quickly rewritten the regular keyframe interpolation
	//routine to apply the same logic to vertices to make sure they match
	//and provide a standalone version for user code to take advantage of.

	FrameAnim *ab = nullptr;
	if(anim<m_frameAnims.size()&&pos<m_vertices.size()&&trans)
	ab = m_frameAnims[anim];

	int ret = 0; if(ab&&frame<ab->_frame_count())
	{
		auto *jk = &m_vertices[pos]->m_frames[ab->m_frame0];	
		auto &tt = ab->m_timetable2020;

		//TODO: Binary search? //FIX ME
		unsigned first[1] = {};
		unsigned key[1] = {}, stop[1] = {};
		unsigned last[1] = {};
		unsigned frames = ab->_frame_count();
		for(unsigned kk,k=0;k<frames;)
		{
			auto cmp = jk[kk=k++];

			if(!cmp->m_interp2020) continue;

			const int i = 0; if(kk<=frame)
			{
				if(!first[i]) first[i] = k; 

				// Less than current time
				// get latest keyframe for rotation and translation
				key[i] = k;
			}
			else
			{
				// Greater than current time
				// get earliest keyframe for rotation and translation
				if(!stop[i])
				{
					if(key[i]) 
					{						
						if(time<=tt[key[i]-1])
						{
							stop[i] = key[i]; 
						}
						else stop[i] = k;
					}
					else stop[i] = key[i] = k;
				}

				last[i] = k;
			}
		}

		bool wrap = ab->m_wrap;
	
		for(int i=0;i<1;i++) if(auto k=key[i])
		{	
			ret|=1<<i;

			if(!stop[i]) stop[i] = (wrap?first:key)[i]; 
	
			unsigned frame2;
			auto p = jk[frame=k-1];
			auto d = jk[frame2=stop[i]-1];
			double t = 0, cmp = tt[frame];
			const double *dp,*pp = p->m_coord;
			//RATIONALE: The mode comes from the later keyframe because
			//there are not modes associated with the base model's data.
			//If this is unconventional importers should add end frames.
			auto e = d->m_interp2020;
			
			bool lerp = e==InterpolateLerp;

			//TODO: setKeyframe might fill these out instead of looking
			//this up here. Unlike vertex-data there's no memory saving.
			if(InterpolateCopy==p->m_interp2020)
			{
				while(k-->0)
				{	
					if(jk[k]->m_interp2020>InterpolateCopy)
					{
						pp = jk[k]->m_coord; break;
					}
				}
				if(k==-1) if(wrap) 
				{
					for(k=last[i];k-->0;)
					if(jk[k]->m_interp2020>InterpolateCopy)
					{
						pp = jk[k]->m_coord; break;
					}
				}

				if(pp==p->m_coord)
				{
					pp = m_vertices[pos]->m_coord;
				}

				if(!lerp) d = p;
			}		
			else if(InterpolateCopy==e)
			{
				d = p;
			}

			if(time!=cmp) if(time>cmp||wrap) 
			{			
				if(lerp&&p!=d)
				{
					dp = d->m_coord;
					double diff = tt[frame2]-cmp;
					if(diff<0) diff+=ab->_time_frame();
					t = (time-cmp)/diff;
				}
			}
			else //Clamping?
			{
				//Note: There is no extrapolation after the final frame
				//since older MM3D files used step mode and fixed count.

				dp = pp;				
				pp = m_vertices[pos]->m_coord;
				t = lerp?time/cmp:0;
			}
		
			if(t)
			{
				Vector va(pp);
				//if(t) //interpolate?
				va+=(Vector(dp)-va)*t;
				va.getVector3(trans);
			}
			else memcpy(trans,pp,sizeof(*trans)*3);
		}
	}

	if(!ret&&trans) if(pos<m_vertices.size())
	{
		memcpy(trans,m_vertices[pos]->m_coord,sizeof(*trans)*3);
	}
	else memset(trans,0x00,sizeof(*trans)*3);

	return ret;	
}
