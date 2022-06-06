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

#include "modelstatus.h"
#include "modelundo.h"

#include "log.h"
#include "glmath.h"

Model::Animation *Model::_anim(unsigned index, AnimationModeE m)const
{
	if(index>=m_anims.size()) return nullptr;

	auto p = m_anims[index]; return m&&0==(m&p->_type)?nullptr:p;
}
Model::Animation *Model::_anim(unsigned anim, unsigned frame, Position pos, bool verbose)const
{
	Animation *ab = anim<m_anims.size()?m_anims[anim]:nullptr;
	
	size_t objects = 0; switch(pos.type) //OBSOLETE?
	{
	case PT_Joint: objects = m_joints.size(); break;
	case PT_Point: objects = m_points.size(); break;
	}		
	if(!ab||frame>=ab->_frame_count()||pos>=objects)
	{
		assert(!verbose); //PROGRAMMER ERROR

		//REMOVE US
		int c = ab?pos.type==PT_Joint?'j':'c':'?';
		if(verbose)
		log_error("anim keyframe out of range: anim %d, frame %d, %coint %d\n",anim,frame,c,pos.index); //???
		if(verbose&&ab)		
		log_error("max frame is %d, max %coint is %d\n",c,ab->_frame_count());

		ab = nullptr;
	}
	else //2021: This isn't out-of-bounds but should it be reported too?
	{
		int compile[PT_Joint==1&&2==PT_Point&&PT_MAX==4]; (void)compile;

		if(~ab->_type&pos.type) return nullptr; 
	}
	return ab;
} 
//2021: I set something like this up in 2020 for translateSelected
//and rotateSelected, but I can't remember why. Obviously it's not
//a valid state since setCurrentAnimation is the only way to enter
//animation mode.
bool Model::_anim_check(bool report)
{
	if(m_currentAnim>=m_anims.size())
	{
		assert(0);

		if(report) model_status(this,
		StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel","Programmer error: No animation"));
	}
	else return true; return false;
}

#ifdef MM3D_EDIT

unsigned Model::insertAnimFrame(unsigned anim, double time)
{
	auto ab = _anim(anim); if(!ab) return 0; 

	auto frame = getAnimFrame(anim,time);
	double cmp = ab->_frame_time(frame);
	unsigned count = ab->_frame_count();
	if(time>cmp //insert after?
	||frame==0&&cmp>0&&time!=cmp //insert before?
	||!count&&!cmp&&!frame) //e.g. zero-length pose?
	{
		//If 0 the new frame is somewhere before the
		//first frame and it has a nonzero timestamp.
		if(count) 
		{			
			//2021: I'm worried about m_fps conversion.
			if(fabs(time-cmp)<=0.000005) return frame;

			frame+=time>cmp;
		}

		//TODO: Relying on setAnimFrameCount to fill
		//out the new vertices with the current ones.
		//It would be an improvement to do that here.
		//REMINDER: HOW MU_AddFrameAnimFrame ANOTHER //???
		//ModelUndo WON'T BE NEEDED TO FILL THE DATA.
		setAnimFrameCount(anim,count+1,frame,nullptr);
		setAnimFrameTime(anim,frame,time);		
	}
	else assert(time==cmp); return frame;
}

int Model::addAnimation(AnimationModeE m, const char *name)
{
	//LOG_PROFILE(); //???

	if(!name||!m||(unsigned)m>=ANIMMODE_Max) return -1;

	//2021: preventing surprise (MM3D)
	//doesn't allow setting names to a
	//blank but filters can.
	//https://github.com/zturtleman/mm3d/issues/158
	if(!name[0]) name = "_";

	//2021: Enforce partitions.
	auto num = (unsigned)m_anims.size();
	while(num&&m_anims[num-1]->_type>m) num--;

	auto p = Animation::get();
		
	p->_type = m; p->m_name = name;

	/*DEFERRING FOR TYPE 3 ANIMATION (_anim_valloc)
	//NOTE: There's a problem here since intially the 
	//animation has no frames, so code that iterates
	//starting at m_frame0 has to check for this case
	//since it'd be too misleading to insert a dummy 
	if(2&p->_type&&!m_vertices.empty())		
	p->m_frame0 = m_vertices.front()->m_frames.size();
	*/

	Undo<MU_AddAnimation>(this,num,p);
	
	insertAnimation(num,p); return num;
}
void Model::_anim_valloc(const Animation *p)
{
	if(0==~p->m_frame0&&2&p->_type&&!m_vertices.empty())		
	{
		auto &fp = const_cast<unsigned&>(p->m_frame0);
		fp = m_vertices.front()->m_frames.size();
		insertFrameAnimData(fp,p->_frame_count(),nullptr,p);
	}
}

void Model::deleteAnimation(unsigned index)
{
	//LOG_PROFILE(); //???

	auto ab = _anim(index); if(!ab) return;
	
	Undo<MU_DeleteAnimation> undo(this,index,ab);

	//DUPLICATES convertAnimToType deletion of vertex data.
	if(!undo) ab->release(); //2022

	auto fp = ab->m_frame0; if(~fp) //ANIMMODE_FRAME
	{		
		auto dt = undo?undo->removeVertexData():nullptr;

		removeFrameAnimData(fp,ab->_frame_count(),dt,!dt); //release?
	}
	
	removeAnimation(index);
}

bool Model::setAnimName(unsigned anim, const char *name)
{
	if(name&&name[0]) if(auto*ab=_anim(anim))
	{
		if(ab->m_name!=name)
		{
			m_changeBits|=AddAnimation; //2020

			Undo<MU_SwapStableStr>(this,AddAnimation,ab->m_name);

			ab->m_name = name; 
		}
		return true;
	}
	return false;
}

bool Model::setAnimFrameCount(unsigned anim, unsigned count)
{
	return setAnimFrameCount(anim,count,~0u,nullptr);
}
bool Model::setAnimFrameCount(unsigned anim, unsigned count, unsigned where, FrameAnimData *ins)
{
	unsigned old_count = getAnimFrameCount(anim);
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

	Animation *ab = m_anims[anim];

	m_changeBits|=AnimationProperty;

	Undo<MU_SetAnimFrameCount> undo(this,anim,count,old_count,where);

	if(undo) undo->removeTimeTable(ab->m_timetable2020);
	if(undo) undo->removeSelection(ab->m_selected_frames);

	if(~ab->m_frame0) //ANIMMODE_FRAME
	{
		auto fp = ab->m_frame0+where;
		auto dt = undo?undo->removeVertexData():nullptr;

		if(diff>=0)
		{
			//NEW: Gathering the pointers so routines like copyAnimation
			//don't have to generate undo data for their copied vertices.
			assert(!ins||!dt);

			insertFrameAnimData(fp,diff,ins?ins:dt,ab);
		}
		else removeFrameAnimData(fp,-diff,dt,!dt); //release?
	}

	for(auto&ea:ab->m_keyframes)
	{	
		KeyframeList &l = ea.second; if(diff<0)
		{
			size_t i;
			for(i=0;i<l.size()&&l[i]->m_frame<where;)
			i++;
			unsigned whereto = where-diff;
			while(i<l.size()&&l[i]->m_frame<whereto)
			{
				//Could use removeKeyframe/MU_DeleteObjectKeyframe here
				//but probably better to improve the undo system itself.
				deleteKeyframe(anim,l[i]->m_frame,ea.first);
			}
			for(;i<l.size();i++) l[i]->m_frame+=diff;
		}
		else for(auto*ea:l) if(ea->m_frame>=where)
		{
			ea->m_frame+=diff;
		}
	}
	
	auto it = ab->m_timetable2020.begin()+where;

	if(compat_mode) //Compatibility mode.
	{
		ab->m_timetable2020.resize(count);
		for(auto i=old_count;i<count;i++)
		ab->m_timetable2020[i] = i;

		//2022: This is to initialize this for the legacy
		//import "filters" to do away with _time_frame().
		//Currently setAnimFrameTime_undo is required by
		//mm3dfilter.cc in order to bypass time checks.
		if(ab->m_frame2020<0) ab->m_frame2020 = count;
	}
	else if(diff>0)
	{				
		ab->m_timetable2020.insert(it,(size_t)diff,-1); //!!!
	}
	else ab->m_timetable2020.erase(it,it-diff);

	if(!ab->m_selected_frames.empty())
	{
		m_changeBits|=AnimationSelection;

		auto jt = ab->m_selected_frames.begin()+where;

		if(compat_mode)
		{
			ab->m_selected_frames.resize(count);
		}
		else if(diff>0)
		{				
			ab->m_selected_frames.insert(jt,(size_t)diff,0);
		}
		else ab->m_selected_frames.erase(jt,jt-diff);
	}

	if(m_currentAnim==anim&&m_currentFrame>=count)
	{
		//setCurrentAnimationFrame(0);
		setCurrentAnimationFrame(count?count-1:0);
	}

	invalidateAnim(anim,m_currentFrame);
	
	return true;
}

bool Model::setAnimFPS(unsigned anim, double fps)
{
	if(auto*ab=_anim(anim))
	{		
		if(fps<setAnimFPS_minimum) //1
		{
			//2022: There's no code limiting FPS
			//to a range that would prevent many
			//uses of m_fps from running aground.								
			assert(fps>=setAnimFPS_minimum);
				
			model_status(this,StatusError,STATUSTIME_LONG,
			TRANSLATE("LowLevel","Programmer error: FPS below minimum (%d)"),setAnimFPS_minimum);

			//return false;
			fps = setAnimFPS_minimum; 
		}

		if(ab->m_fps!=fps) 
		{
			m_changeBits|=AnimationProperty;

			Undo<MU_SetAnimFPS>(this,anim,fps,ab->m_fps);

			ab->m_fps = fps; 

			invalidateAnim(anim,m_currentFrame);
		}
		return true;
	}
	return false;
}

bool Model::setAnimWrap(unsigned anim, bool wrap)
{
	if(auto*ab=_anim(anim))
	{
		if(ab->m_wrap!=wrap)
		{
			m_changeBits|=AnimationProperty;

			Undo<MU_SetAnimWrap>(this,anim,wrap,ab->m_wrap);

			ab->m_wrap = wrap;

			invalidateAnim(anim,m_currentFrame);
		}
		return true;
	}
	return false;
}

bool Model::setAnimTimeFrame(unsigned anim, double time)
{	
	Animation *ab = _anim(anim); if(!ab) return false;

	if(ab->m_frame2020==time) return true;

	size_t frames = ab->m_timetable2020.size();
	if(frames&&time<ab->m_timetable2020.back())
	{
		model_status(this,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel","Cannot set timeframe before keyframes"));
		return false;
	}

	m_changeBits|=AnimationProperty;

	Undo<MU_SetAnimTime>(this,anim,time,ab->_time_frame());

	ab->m_frame2020 = time;

	if(anim==m_currentAnim)
	{
		if(time<m_currentTime)
		{
			//REMINDER: If this were to be an error case I'm skeptical
			//that "redo" operations could backfire.
			setCurrentAnimationFrameTime(time);
		}
		else invalidateAnim(anim,(unsigned)frames-1);	
	}
			
	return true;
}

bool Model::setAnimFrameTime(unsigned anim, unsigned frame, double time)
{
	if(time<0){ assert(0); return false; } //2022

	auto ab = _anim(anim); if(!ab) return false;

	auto &tt = ab->m_timetable2020;

	if(time==tt[frame]) return true;

	//if(ab->m_frame2020!=-1)
	if(frame>=ab->m_frame2020){ assert(0); return false; }  //2022

	if(frame>0) //2022
	{
		assert(time>=tt[frame-1]); //return false;
	}
	if(frame+1<tt.size())
	{
		assert(time<=tt[frame+1]); //return false;		
	}

	Undo<MU_SetAnimTime>(this,anim,frame,time,time);

	setAnimFrameTime_undo(anim,frame,time); return true;
}
void Model::setAnimFrameTime_undo(unsigned anim, unsigned frame, double time)
{
	m_changeBits|=AnimationProperty;

	auto ab = m_anims[anim];

	ab->m_timetable2020[frame] = time;			
	
	invalidateAnim(anim,frame);
}
unsigned Model::moveAnimFrame(unsigned anim, unsigned frame, double time)
{
	if(time<0){ assert(0); return frame; } //2022

	auto ab = _anim(anim); if(!ab) return frame;

	auto &tt = ab->m_timetable2020;

	if(time==tt[frame]) return frame;

	//if(ab->m_frame2020!=-1)
	if(frame>=ab->m_frame2020){ assert(0); return frame; } //2022

	unsigned i = frame;

	while(i&&time<tt[i-1]) i--;

	while(i+1<tt.size()&&time>tt[i+1]) i++;

	if(i!=frame)
	{
		Undo<MU_MoveAnimFrame>(this,anim,i,frame);

		moveAnimFrame_undo(anim,frame,i);
	}
	
	Undo<MU_SetAnimTime>(this,anim,i,time,tt[frame]); //HACK?

	invalidateAnim(anim,frame);
	setAnimFrameTime_undo(anim,i,time); return i;
}
void Model::moveAnimFrame_undo(unsigned anim, unsigned frame, unsigned i)
{
	auto ab = m_anims[anim];

	auto min = std::min(frame,i);
	auto max = std::max(frame,i);
	auto mov = max-min;
	bool dir = i<frame;

	auto tt = ab->m_timetable2020.data();
	auto swap = tt[frame];
	auto d = tt+min, s = d;
	(dir?d:s)+=1;
	memmove(d,s,mov*sizeof(swap));
	tt[i] = swap;

	if(!ab->m_selected_frames.empty())
	{
		//m_changeBits|=AnimationSelection; //?

		auto st = ab->m_selected_frames.data();
		auto swap = st[frame];
		auto d = st+min, s = d;
		(dir?d:s)+=1;
		memmove(d,s,mov*sizeof(swap));
		st[i] = swap;
	}

	if(~0u!=ab->m_frame0)	
	{
		auto frame0 = ab->m_frame0;		
		auto min0 = frame0+min;
		auto i0 = frame0+i;
		frame0+=frame;
		for(auto*vp:m_vertices)
		{
			auto fd = vp->m_frames.data();
			auto swap = fd[frame0];
			auto d = fd+min0, s = d;
			(dir?d:s)+=1;
			memmove(d,s,mov*sizeof(swap));
			fd[i0] = swap;
		}
	}

	for(auto&ea:ab->m_keyframes)
	{
		bool sort = false;

		for(auto*kp:ea.second)
		{
			auto f = kp->m_frame;

			if(f<min||f>max) continue;

			sort = true;

			int cmp = (int)f-(int)frame;

			if(cmp>0)
			{
				kp->m_frame--; assert(!dir);
			}
			else if(cmp<0)
			{
				kp->m_frame++; assert(dir);
			}
			else kp->m_frame = i;
		}

		//NOTE: This is a sorted_ptr_list<Keyframe*> container.
		//if(sort) std::sort(ea.second.begin(),ea.second.end());
		if(sort) ea.second.sort();
	}
}

bool Model::setFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
		const double xyz[3], Interpolate2020E interp2020)
{
	auto fa = _anim(anim,ANIMMODE_FRAME); if(!fa) return false;

	const auto fc = fa->_frame_count();
	if(frame>=fc||vertex>=m_vertices.size()) return false;
		
	//https://github.com/zturtleman/mm3d/issues/90
	m_changeBits|=MoveGeometry;

	const auto fp = fa->_frame0(this);	
	
	auto list = &m_vertices[vertex]->m_frames[fp];

	FrameAnimVertex *fav = list[frame];

	//HACK: Supply default interpolation mode to any
	//neighboring keyframe
	if(InterpolateKeep==interp2020) 
	if(fav->m_interp2020<=InterpolateCopy)
	{
		for(auto i=frame;++i<fc;)
		if(interp2020=list[i]->m_interp2020) 
		break;
		
		if(fa->m_wrap)
		{
			for(unsigned i=0;i<frame;i++)
			if(interp2020=list[i]->m_interp2020) 
			break;
		}
		else for(auto i=frame;i-->0;)
		if(interp2020=list[i]->m_interp2020) 
		break;
		interp2020 = InterpolateLerp;
	}
	else interp2020 = fav->m_interp2020;

	Undo<MU_MoveFrameVertex> undo(this,anim,frame);		
	if(undo) undo->addVertex(vertex,xyz?xyz:fav->m_coord,interp2020,fav);

	if(xyz)
	{
		fav->m_coord[0] = xyz[0];
		fav->m_coord[1] = xyz[1];
		fav->m_coord[2] = xyz[2];		
	}

	assert(InterpolateKeep!=interp2020);
	fav->m_interp2020 = interp2020;

	invalidateAnim(anim,frame); //OVERKILL

	return true;
}

void Model::setQuickFrameAnimVertexCoords(unsigned anim, unsigned frame, unsigned vertex,
		double x, double y, double z, Interpolate2020E interp2020)
{
	//auto fa = _anim(anim,ANIMMODE_FRAME); //2022
	auto fa = m_anims[anim];
	
	//if(!fa) return false; //2022: Quick?
	//const auto fc = fa->_frame_count();
	//if(frame>=fc||vertex>=m_vertices.size()) return false;
	assert(fa&&frame<fa->_frame_count()&&vertex<m_vertices.size());

	//2022: Can it skip _frame0?
	//assert(0!=~fa->m_frame0);
	const auto fp = fa->_frame0(this);	
	auto list = &m_vertices[vertex]->m_frames[fp];
	FrameAnimVertex *fav = list[frame];

	fav->m_coord[0] = x;
	fav->m_coord[1] = y;
	fav->m_coord[2] = z;

	//invalidateNormals(); //OVERKILL

	assert(interp2020!=InterpolateKeep);
	fav->m_interp2020 = interp2020;

	//return true;
}

int Model::copyAnimation(unsigned index, const char *newName)
{
	Animation *ab = _anim(index); if(!ab) return -1;
	//int num = addAnimation(mode,newName); if(num<0) return num;
	int num = _dup(ab);
	Animation *ab2 = m_anims[num];
	if(newName) ab2->m_name = newName;

	auto fc = ab->_frame_count();
	if(fc!=0&&~ab->m_frame0) //ANIMMODE_FRAME
	{	
		_anim_valloc(ab2);

		//Note, setFrameAnimVertexCoords is unnecessary 
		//because this is a fresh animation
		auto fp = ab->m_frame0;
		auto fd = ab2->m_frame0;		
	//	auto fc = ab->_frame_count();
		for(auto*ea:m_vertices)
		{
			//Note, it's easier on the cache to do
			//the vertices in the outer loop

			auto p = &ea->m_frames[fp];
			auto d = &ea->m_frames[fd];

			for(unsigned c=fc;c-->0;d++,p++) **d = **p;
		}
	}		

	//2020: If splitAnimation does this then so should copy.
	moveAnimation(num,index+1); 
	
	return index+1; //return num;
}

int Model::splitAnimation(unsigned index, const char *newName, unsigned frame)
{
	Animation *ab = _anim(index); if(!ab) return -1;

	if(frame>ab->_frame_count()) return -1; //2020?

	int num = addAnimation((AnimationModeE)ab->_type,newName);
	if(num<0) return num;

	Animation *ab2 = m_anims[num];

	int fc = ab->_frame_count()-frame;
	setAnimFrameCount(num,fc);
	ab2->m_fps = ab->m_fps;
	ab2->m_wrap = ab->m_wrap;
	double ft = ab->m_timetable2020[frame];
	ab2->m_frame2020 = ab->_time_frame()-ft;
	while(fc-->0)
	ab2->m_timetable2020[fc] = ab->m_timetable2020[frame+fc]-ft;		
		
	for(auto&ea:ab->m_keyframes)
	for(auto*kf:ea.second)
	if(kf->m_frame>=frame)
	{
		setKeyframe(num,kf->m_frame-frame,ea.first,kf->m_isRotation,
		kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
	}

	if(~ab->m_frame0) //ANIMMODE_FRAME 
	{	
		_anim_valloc(m_anims[num]);

		//Note, setFrameAnimVertexCoords is unnecessary 
		//because this is a fresh animation
		auto fp = ab->m_frame0+frame;
		auto fd = m_anims[num]->m_frame0;
		auto fc = ab->_frame_count();
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
	setAnimFrameCount(index,frame+1);
	setAnimTimeFrame(index,ft);

	moveAnimation(num,index+1); 
	
	return index+1; //return num;
}

bool Model::joinAnimations(unsigned anim1, unsigned anim2)
{
	if(anim1==anim2) return true;

	auto aa = _anim(anim1); //ab1
	auto ab = _anim(anim2); //ab2
	if(!aa||!ab||aa->_type!=ab->_type) return false;

	//2020: I'm not sure I've fully tested this code with every
	//possible permutation but it looks fine. If not please fix.
	
	//NOTE: I don't recommend inserting a dummy frame to keep a
	//keyframe from blending with the earlier animation's frame
	//because it's impossible to tease the results apart in the
	//final data set, whereas it's pretty trivial in the editor
	//to insert the dummy frame before or after with copy/paste.

	int fc1 = aa->_frame_count();
	int fc2 = ab->_frame_count();
	double tf = getAnimTimeFrame(anim1);
	int spliced = 0; if(fc1&&fc2)
	{
		//If both animations have frames on the end and beginning
		//they are overlapped and must be merged together somehow.
		double t1 = getAnimFrameTime(anim1,fc1-1);
		double t2 = getAnimFrameTime(anim2,0);
		spliced = t1>=tf&&t2<=0;

		fc1-=spliced; //!
	}	
	int fc = fc1+fc2;
	setAnimFrameCount(anim1,fc);

	//2020: Convert the frames into the correct time scales and 
	//add the durations and timestamps together.
	double r = 1;
	if(aa->m_fps!=ab->m_fps)
	r = aa->m_fps/ab->m_fps;
	setAnimTimeFrame(anim1,tf+r*getAnimTimeFrame(anim2));
	for(int i=spliced,f=fc1+spliced;f<fc;f++)	
	setAnimFrameTime(anim1,f,tf+r*ab->m_timetable2020[i++]);	

	for(auto&ea:ab->m_keyframes)
	for(auto*kf:ea.second)	
	{
		//HACK: Don't overwrite an existing keyframe with Copy.
		auto e = kf->m_interp2020;
		if(e==InterpolateCopy) e = InterpolateKopy;

		setKeyframe(anim1,kf->m_frame+fc1,ea.first,kf->m_isRotation,
		kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],e);
	}
	
	if(~ab->m_frame0) //ANIMMODE_FRAME 
	{	
		_anim_valloc(aa);

		auto fp = ab->m_frame0;
		auto fd = aa->m_frame0+fc1;

		for(int f=0;f<fc2;f++,fp++,fd++)
		{
			Undo<MU_MoveFrameVertex> undo(this,anim1,fc1+f);

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

				if(undo) undo->addVertex(v,p->m_coord,p->m_interp2020,d,false);			

				*d = *p;
			}
		}
	}	

	deleteAnimation(anim2); return true;
}

bool Model::mergeAnimations(unsigned anim1, unsigned anim2)
{
	if(anim1==anim2) return true;

	auto aa = _anim(anim1); //ab1
	auto ab = _anim(anim2); //ab2
	if(!aa||!ab||aa->_type!=ab->_type) return false;

	int fc1 = (unsigned)aa->_frame_count();
	int fc2 = (unsigned)ab->_frame_count();

	/*2020: Doing this interleaved.
	if(fc1!=fc2)
	{
		//str = ::tr("Cannot merge animation %1 and %2,\n frame counts differ.")
		//.arg(model->getAnimName(a)).arg(model->getAnimName(b));
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

		//FIX? i is looping forever???
		if(i!=fc1&&s<t&&s>=0)
		{
			i++; st = s;
		}
		else if(j!=fc2&&t<s&&t>=0)
		{
			j++; st = t;
		}
		else if(s==t&&t>=0)
		{
			if(i!=fc1) i++; 
			if(j!=fc2) j++; st = t;
		}
		else if(i==fc1) //NEW: stuck at end?
		{
			j++; st = t; 
		}
		else if(j==fc2) //NEW: stuck at end?
		{
			i++; st = s;
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
	double tf1 = getAnimTimeFrame(anim1);
	double tf2 = getAnimTimeFrame(anim2);
	if(tf2>tf1) setAnimTimeFrame(anim1,tf2);

	auto sz = (unsigned)ts.size();
	setAnimFrameCount(anim1,sz);
	while(sz-->0)
	setAnimFrameTime(anim1,sz,ts[sz]);

	//Remap anim1 keyframes to new interleaved times.
	std::vector<Keyframe*> kmap;
	for(auto&ea:aa->m_keyframes)
	for(auto*kf:ea.second)
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
			deleteKeyframe(anim1,kf->m_frame,ea.first,kf->m_isRotation);
		}
	}
	for(auto*kf:kmap) //Use setKeyframe for undo support?
	{
		//FIX ME: This calls for a new ModelUndo object!!

		setKeyframe(anim1,smap[kf->m_frame],kf->m_objectIndex,kf->m_isRotation,
		kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
	}

	//Add anim2 keyframes to anim1
	for(auto&ea:ab->m_keyframes)
	for(auto*kf:ea.second)	
	{
		//HACK: Don't overwrite an existing keyframe with Copy.
		auto e = kf->m_interp2020;
		if(e==InterpolateCopy) e = InterpolateKopy;

		setKeyframe(anim1,tmap[kf->m_frame],ea.first,kf->m_isRotation,
		kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],e);
	}

	//WARNING: I think maybe this is untested since I
	//didn't enable the Merge button in the UI window
	if(~ab->m_frame0) //ANIMMODE_FRAME 
	{	
		_anim_valloc(aa);

		int fp,fp0 = ab->m_frame0;
		int fd,fd0 = aa->m_frame0;
		
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

			Undo<MU_MoveFrameVertex> undo(this,anim1,f);

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

				if(undo) undo->addVertex(v,p->m_coord,p->m_interp2020,d,false);			

				*d = *p;
			}
		}
	}		

	deleteAnimation(anim2); return true;
}

bool Model::moveAnimation(unsigned oldIndex, unsigned newIndex)
{
	if(oldIndex==newIndex) return true;

	if(oldIndex<m_anims.size()&&newIndex<m_anims.size())
	{
		auto p = m_anims[oldIndex];

		m_anims.erase(m_anims.begin()+oldIndex);			

		//2021: Enforce partitions.
		if(newIndex!=m_anims.size()
		&&m_anims[newIndex]->_type<p->_type
		||newIndex
		&&m_anims[newIndex-1]->_type>p->_type)
		{
			newIndex = oldIndex;
		}
		
		m_anims.insert(m_anims.begin()+newIndex,p);

		if(newIndex==oldIndex) return false;
	}
	else return false;
			
	m_changeBits|=AddAnimation; //2020
		
	if(m_currentAnim==oldIndex)
	{
		m_currentAnim = newIndex;

		m_changeBits|=AnimationSet;
	}

	Undo<MU_MoveAnimation>(this,oldIndex,newIndex);

	return true;
}
void Model::_moveAnimation(unsigned oldIndex, unsigned newIndex, int typeDiff)
{
	if(oldIndex<m_anims.size()&&newIndex<m_anims.size())
	{
		auto p = m_anims[oldIndex]; p->_type+=typeDiff;
		if(oldIndex!=newIndex)
		{
			m_anims.erase(m_anims.begin()+oldIndex);			
			m_anims.insert(m_anims.begin()+newIndex,p);
		}
	}
	else assert(0);
			
	m_changeBits|=AddAnimation; //2020
		
	if(m_currentAnim==oldIndex)
	{
		m_currentAnim = newIndex;

		m_changeBits|=AnimationSet;
	}
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
int Model::convertAnimToFrame(unsigned anim, const char *newName, unsigned frameCount, Interpolate2020E how, Interpolate2020E how2)
{
	if(!how2) how2 = how;
	auto ab = _anim(anim); 
	int num = -1; if(ab&&2==ab->_type) if(how)
	{
		num = addAnimation(ANIMMODE_FRAME,newName);
	}
	if(num<0) return num;

	setAnimFrameCount(num,frameCount);
	setAnimWrap(num,ab->m_wrap);

	if(!frameCount) //???
	{
		setAnimFPS(num,10); return num;
	}

	//double time = (ab->_frame_count()*(1/ab->m_fps));
	double time = ab->_time_frame();
	double fps  = (double)frameCount/time;
	double spf  = time/(double)frameCount;

	//log_debug("resampling %d frames at %.2f fps to %d frames at %.2f fps\n",ab->_frame_count(),ab->m_fps,frameCount,fps);

	setAnimFPS(num,fps);

	//2021: Below is a hack (did I program it?) so
	//at least the state should be restored...
	auto swap = makeRestorePoint();

	setCurrentAnimation(anim); //??? //FIX ME
	{
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
				if(prev&1) setFrameAnimVertexCoords(num,f-1,v,pcoord,how2);
				if(curr&1) setFrameAnimVertexCoords(num,f,v,coord,how);
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
	}
	setCurrentAnimation(swap); return num;
}
int Model::convertAnimToType(AnimationModeE e, unsigned anim)
{
	//WARNING: This is destructive. Users need to use
	//hasKeyframeData to determine if acceptable loss.

	if(!e||e>3) return -1;

	Animation *ab = _anim(anim); if(!ab) return -1;

	int td = e-ab->_type; if(!td) return anim;

	if(e!=ANIMMODE&&ab->_frame_count())
	{
		for(auto&ea:ab->m_keyframes)
		{
			if(e==ANIMMODE_JOINT)
			{
				if(ea.first.type==PT_Joint) continue;
			}
			else if(ea.first.type!=PT_Joint) continue;

			while(!ea.second.empty())
			{
				//Could use removeKeyframe/MU_DeleteObjectKeyframe here
				//but probably better to improve the undo system itself.
				deleteKeyframe(anim,ea.second.back()->m_frame,ea.first);
			}
		}

		if(~e&2&&~ab->m_frame0) //ANIMMODE_FRAME
		{
			//DUPLICATES deleteAnimation except just the vertex data parts.

			Undo<MU_VoidFrameAnimation> undo(this,ab,ab->m_frame0);
			
			auto fp = ab->m_frame0; 
			auto dt = undo?undo->removeVertexData():nullptr;
			removeFrameAnimData(fp,ab->_frame_count(),dt,!dt); //release?

			ab->m_frame0 = ~0; //2022: Adding dedicated MU_VoidFrameAnimation undo for this.
		}
	}

	auto to = getAnimationIndex(e)+getAnimationCount(e);

	if(to>anim) to--; //Complicated :(

	_moveAnimation(anim,to,td);

	Undo<MU_MoveAnimation>(this,anim,to,td);
	
	m_changeBits|=AddAnimation;
	
	//HACK: It's really not ideal to do this on the user's behalf.
	//But it's not ideal to lose state either.
	if(anim==m_currentAnim&&inAnimationMode())
	{
		model_status(this,StatusNotice, //StatusError
		STATUSTIME_LONG,TRANSLATE("LowLevel","Animation mode set to current animation's new type"));

		setAnimationMode(e);
	}

	return to;
}

bool Model::deleteAnimFrame(unsigned anim, unsigned frame)
{
	//TODO: It would be nice if eliminating all of the frame data automatically
	//deleted the frame but perhaps that shouldn't be implemented at this level.
	if(size_t count=getAnimFrameCount(anim))
	return setAnimFrameCount(anim,(unsigned)count-1,frame,nullptr); return false;
}

int Model::setKeyframe(unsigned anim, const Keyframe *cp, const double xyz[3], Interpolate2020E e)
{
	if(!xyz) xyz = cp->m_parameter;
	return setKeyframe(anim,cp->m_frame,cp->m_objectIndex,cp->m_isRotation,xyz[0],xyz[1],xyz[2],e);
}
int Model::setKeyframe(unsigned anim, unsigned frame, Position pos, KeyType2020E isRotation,
		double x, double y, double z, Interpolate2020E interp2020)
{	
	Animation *ab = _anim(anim,frame,pos); if(!ab) return -1;

	//NEW: this saves user code from branching on this case and is analogous
	//to vertex frame data
	if(!interp2020) 
	{
		deleteKeyframe(anim,frame,pos,isRotation); return -1;
	}

	if(isRotation<=0||isRotation>KeyScale) return -1;

	//m_changeBits|=MoveOther; //2020
	m_changeBits|=AnimationProperty; //2022

	//log_debug("set %s of %d (%f,%f,%f)at frame %d\n",(isRotation ? "rotation" : "translation"),pos.index,x,y,z,frame);

	//int num = ab->m_keyframes[pos].size();
	auto kf = Keyframe::get();
	kf->m_frame = frame;
	kf->m_isRotation = isRotation;

	bool isNew = false;
	double old[3] = {};

	unsigned index = 0;
	auto &list = ab->m_keyframes[pos];
	if(list.find_sorted(kf,index))
	{
		isNew = false;

		kf->release(); //???

		kf = list[index];

		memcpy(old,kf->m_parameter,sizeof(old));

		for(int j=4;j-->0;)
		if(kf->m_selected[j])
		m_changeBits|=AnimationSelection;

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

		//list.insert_sorted(kf);
		list.insert_sorted(kf,index);

		// Do lookup to return proper index
		//list.find_sorted(kf,index);

		if(InterpolateKopy==interp2020)
		{ 
			interp2020 = InterpolateCopy;
		}
	}
	
	//HACK: Supply default interpolation mode to any
	//neighboring keyframe
	if(InterpolateKeep==interp2020) 
	if(kf->m_interp2020<=InterpolateCopy)
	{
		for(auto i=index;++i<list.size();)
		if(list[i]->m_isRotation==isRotation)		
		if(interp2020=list[i]->m_interp2020) 
		break;		
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

	//Why is this???
	//undo->setAnimationData(anim,frame,isRotation);
	Undo<MU_SetObjectKeyframe> undo(this,anim,frame);
	if(undo)
	undo->addKeyframe(isNew,kf,old,olde);

	_reset_InterpolateCopy(anim,KeyNONE,index,list); //2022
	
	invalidateAnim(anim,frame); //OVERKILL

	return index;
}
void Model::_reset_InterpolateCopy(unsigned anim, KeyType2020E how, unsigned index, KeyframeList &list)
{	
	bool wrap = m_anims[anim]->m_wrap;

	if(how) if(how==KeyAny) 
	{
		//I thought merge/join/splitAnimation might
		//need this, but they are using setKeyframe.
		//I only found applyMatrix sets m_parameter.

		KeyType2020E cmp;
		int mask = KeyTranslate|KeyRotate|KeyScale;
		if(index==0||index==~0||wrap)				
		for(size_t i=0;i<list.size();i++,mask&=~cmp) 
		if(mask&(cmp=list[i]->m_isRotation))
		_reset_InterpolateCopy(anim,KeyNONE,(unsigned)i,list);
		mask = KeyTranslate|KeyRotate|KeyScale;	
		if(index>=list.size()||wrap)
		for(size_t i=list.size();i-->0;mask&=~cmp) 
		if(mask&(cmp=list[i]->m_isRotation))
		_reset_InterpolateCopy(anim,KeyNONE,(unsigned)i,list);

		return; //!!
	}
	else //Removing?
	{
		bool found = false;
		for(;index<list.size();index++) 
		if(how==list[index]->m_isRotation)
		{
			found = true; break;
		}
		if(!found&&wrap)
		for(index=0;index<list.size();index++)
		if(how==list[index]->m_isRotation)
		{
			found = true; break;
		}
		if(!found) return;
	}
	
	auto *kf = list[index];
	auto isRotation = kf->m_isRotation;
	if(kf->m_interp2020==InterpolateCopy)
	{
		double *x[3] = {};
		x[isRotation>>1] = kf->m_parameter;
		interpKeyframe(anim,kf->m_frame,kf->m_objectIndex,x[0],x[1],x[2]);
	}
	bool last = true;
	for(unsigned i=index+1;i<list.size();i++)
	{
		auto *kf2 = list[i];

		if(isRotation!=kf2->m_isRotation) continue;

		last = false;

		if(InterpolateCopy!=kf2->m_interp2020) break;

		memcpy(kf2->m_parameter,kf->m_parameter,sizeof(kf->m_parameter));
	}
	if(last&&wrap) for(unsigned i=0;i<index;i++)
	{
		auto *kf2 = list[i];

		if(isRotation!=kf2->m_isRotation) continue;

		if(InterpolateCopy!=kf2->m_interp2020) break;

		memcpy(kf2->m_parameter,kf->m_parameter,sizeof(kf->m_parameter));
	}
}

bool Model::deleteKeyframe(unsigned anim, const Keyframe *cp)
{
	return deleteKeyframe(anim,cp->m_frame,cp->m_objectIndex,cp->m_isRotation);
}
bool Model::deleteKeyframe(unsigned anim, unsigned frame, Position pos, KeyType2020E isRotation)
{
	Animation *ab = _anim(anim,frame,pos); if(!ab) return false;
	
	auto it = ab->m_keyframes.find(pos);
	if(it==ab->m_keyframes.end()) return false; //2021

	Undo<MU_DeleteObjectKeyframe> undo(this,anim,m_currentFrame);

	//REMINDER: vector deletions should tend to go in reverse.
	if(undo)
	{
		auto &list = it->second;
		for(size_t i=list.size();i-->0;) 
		{
			auto *kf = list[i];			
			if(kf->m_frame==frame&&kf->m_isRotation&isRotation)
			{
				undo->deleteKeyframe(kf);
			}
		}
	}
	return removeKeyframe(anim,frame,pos,isRotation,!m_undoEnabled);
}

bool Model::insertKeyframe(unsigned anim, Keyframe *keyframe)
{
	auto pos = keyframe->m_objectIndex;
	Animation *ab = _anim(anim,keyframe->m_frame,pos); if(!ab) return false;

	//log_debug("inserted keyframe for anim %d frame %d joint %d\n",anim,keyframe->m_frame,keyframe->m_objectIndex); //???

	//m_changeBits |= MoveOther; //2020
	m_changeBits|=AnimationProperty; //2022

	for(int j=4;j-->0;)
	if(keyframe->m_selected[j])
	m_changeBits|=AnimationSelection;

	auto &list = ab->m_keyframes[pos];

	size_t index = list.insert_sorted(keyframe);

	_reset_InterpolateCopy(anim,KeyNONE,(unsigned)index,list); //2022

	invalidateAnim(anim,keyframe->m_frame); //OVERKILL

	return true;
}
bool Model::removeKeyframe(unsigned anim, Keyframe *kf)
{
	//This would work too. I think the other removeKeyframe is often
	//misued but has the benefit of matching more than one key.
	//model->removeKeyframe(m_anim,kf->m_frame,kf->m_objectIndex,kf->m_isRotation,false);

	Animation *ab = _anim(anim); assert(ab); if(!ab) return false;

	//REMINDER: vector deletions should tend to go in reverse.
	auto &list = ab->m_keyframes[kf->m_objectIndex];
	for(size_t i=list.size();i-->0;) if(list[i]==kf)
	{
		//m_changeBits |= MoveOther; //2020
		m_changeBits|=AnimationProperty; //2022

		list.erase(list.begin()+i);

		_reset_InterpolateCopy(anim,kf->m_isRotation,(unsigned)i,list); 

		invalidateAnim(anim,kf->m_frame); //OVERKILL

		return true;
	}
	return false;
}
bool Model::removeKeyframe(unsigned anim, unsigned frame, Position pos, KeyType2020E isRotation, bool release)
{
	Animation *ab = _anim(anim,frame,pos); if(!ab) return false;

	//REMINDER: vector deletions should tend to go in reverse.
	auto &list = ab->m_keyframes[pos];
	size_t cmp = list.size();
	for(size_t i=cmp;i-->0;)
	{
		Keyframe *kf = list[i];

		if(kf->m_frame==frame)
		{
			//if bad keys aren't removed (they shouldn't be in here) callers
			//(setAnimFrameCount) that expect removal infinite loop
			auto r = kf->m_isRotation;
			if(r<=0||r&isRotation)
			{	
				//m_changeBits |= MoveOther; //2020
				m_changeBits|=AnimationProperty; //2022

				for(int j=4;j-->0;)
				if(kf->m_selected[j])
				m_changeBits|=AnimationSelection;

				assert(r>0&&r<=KeyScale);

				list.erase(list.begin()+i);

				_reset_InterpolateCopy(anim,kf->m_isRotation,(unsigned)i,list); 

				if(release) //MU_SetObjectKeyframe::undo? Undo disabled?
				{
					//MEMORY LEAK!
					//It seems like "false" should be true without an "undo" system?		
					kf->release(); 
				}
			}
		}
	}
	if(cmp==list.size()) return false;

	invalidateAnim(anim,frame); //OVERKILL

	return true;
}

void Model::insertAnimation(unsigned index, Animation *anim)
{
	//LOG_PROFILE(); //???

	if(index<=m_anims.size())
	{
		//2019: MU_AddAnimation?
		m_changeBits|=AddAnimation;
		
		if(m_currentAnim>=index) //2021
		{
			//Without this test loading models
			//makes the index to run up to the 
			//animation total.
			if(!m_anims.empty())
			{
				m_changeBits |= AnimationSet;

				m_currentAnim++;
			}
		}

		m_anims.insert(m_anims.begin()+index,anim);
	}
	else
	{
		log_error("index %d/%d out of range in insertAnimation\n",index,m_anims.size());
	}
}

void Model::removeAnimation(unsigned index)
{
	//LOG_PROFILE(); //???

	if(index<m_anims.size())
	{		
		//2019: MU_AddAnimation?
		m_changeBits|=AddAnimation;

		if(index==m_currentAnim) //2021 
		{
			if(index&&index==m_anims.size()-1)
			{
				m_currentAnim--;
			}

			//Adequate? I don't see a way to retain the animation mode.
			setNoAnimation();
		}
		else if(m_currentAnim>index) //2021
		{
			m_changeBits |= AnimationSet;

			m_currentAnim--;
		}

		m_anims.erase(m_anims.begin()+index);
	}
	else //2019
	{
		log_error("index %d/%d out of range in removeAnimation\n",index,m_anims.size());
	}
}

void Model::insertFrameAnimData(unsigned frame0, unsigned frames, FrameAnimData *data, const Animation *draw)
{
	if(!frames||0==~frame0) return;

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

	for(auto*ea:m_anims) if(~ea->m_frame0) //ANIMMODE_FRAME
	{
		//Note, draw is required because it's ambiguous when adding to 
		//the back of an animation, since it could be the front of the
		//next animation.

		if(ea->m_frame0>=frame0&&ea!=draw) ea->m_frame0+=frames;
	}
}

void Model::removeFrameAnimData(unsigned frame0, unsigned frames, FrameAnimData *data, bool release)
{
	if(!frames||0==~frame0) return;

	FrameAnimData::iterator jt; if(data)
	{
		assert(data->empty()&&!release);

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
		else if(release) //2022 //no undo?
		{
			for(auto jt=it;jt<itt;jt++) (*jt)->release();
		}

		ea->m_frames.erase(it,itt);		
	}	

	for(auto*ea:m_anims) if(~ea->m_frame0) //ANIMMODE_FRAME
	{
		//Note, I don't think this is ambiguous as with insertFrameAnimData
		//but it might be?

		if(ea->m_frame0>frame0) ea->m_frame0-=frames;
	}
}

#endif // MM3D_EDIT

bool Model::setSkeletalModeEnabled(bool how) //2022
{
	if(m_skeletalMode2!=how)
	{
		Undo<MU_ChangeSkeletalMode>(this,how);

		m_skeletalMode2 = how;

		if(m_skeletalMode!=how&&m_animationMode&1)
		{
			m_changeBits |= AnimationMode;

			m_skeletalMode = how; invalidateAnim();
		}
		return true;
	}
	return false;
}
bool Model::setCurrentAnimation(const RestorePoint &rp)
{
	if(rp!=makeRestorePoint())
	{
		setCurrentAnimation(rp.anim,rp.mode);
		//I think this is required even if 0.
		//So setCurrentAnimation doesn't set
		//time to 0 before the time is known.
		//if(rp.time)
		setCurrentAnimationFrameTime(rp.time);
	}
	return m_animationMode!=ANIMMODE_NONE;
}
bool Model::setCurrentAnimation(unsigned anim, AnimationModeE m)
{
	//NOTE: anim is allowed to be nonezro with ANIMMODE_NONE
	//since it can be switched to. But -1 is only valid with
	//ANIMMODE_NONE.

	if(anim==(unsigned)-1&&!m) anim = 0; //2022

	bool ret = !(!m&&anim); //2022

	//2021: calculateAnim is asserting/aborting on having the
	//animation mode nonzero with no animations.
	if(m&&m_anims.empty())
	{
		ret = false;

		m = ANIMMODE_NONE; //I can't remember if this is best? 
	}

	//NOTE: It's important to not generate redundant undo
	//objects since they may be used to determine if users
	//should be prompted about losing work.
	if(anim>=m_anims.size())
	{
		if(m||anim) //2022
		{
			ret = false; m = ANIMMODE_NONE;
		}

		anim = 0;
	}
	if(m&&ret)
	if(auto*p=anim<m_anims.size()?m_anims[anim]:nullptr)
	{
		if(p->_type!=3)
		{
			//Note: This strategy lets the caller blindly pass
			//in a mode mask that only applies to mode 3 types.
			m = AnimationModeE(p->_type);
		}
		else m = AnimationModeE(m&p->_type);
	}
	if(anim==m_currentAnim&&m==m_animationMode) 
	{
		return ret; //2021
	}

	//AnimationModeE oldMode = m_animationMode;
	//int oldAnim = m_currentAnim; //2019
	//int oldFrame = m_currentFrame; //2019
	auto old = makeRestorePoint();

	//log_debug("Changing animation from %d to %d\n",old.mode,m); //???

	//2021: Letting the animation and time be remembered so
	//animation can be toggled on and off.
	if(old.anim!=anim)
	{
		//NOTE: setCurrentAnimationFrameTime is not called since
		//the caller may have a different time than zero in mind.
		m_currentAnim = anim;
		m_currentTime = 0;
		m_currentFrame = 0;

		m_changeBits |= AnimationSet;
	}
	else if(!old.mode&&anim==0&&m) //2022
	{
		//old.anim is 0 by default :(
		m_changeBits |= AnimationSet; //NOTE: Not overriding time.
	}
	if(m!=old.mode)
	{
		m_changeBits |= AnimationMode;
				
		//2022: Note a change to <None> pseudo animation and also
		//change away from an animation.
		/*Actually the inAnimationMode system relies on this not
		//being sent so the previous animation can be restored.
		if(!m) m_changeBits |= AnimationSet;*/
	}

	//2022: I don't trust these to be maintained.
	Animation *a = nullptr;
	if(anim<m_anims.size())
	a = m_anims[anim];
	if(!a||a->_time_frame()<m_currentTime
	||a->m_timetable2020.size()<=m_currentFrame)
	{
		m_currentTime = 0; m_currentFrame = 0;
	}
	
	if(old.mode||m) invalidateBspTree();

	//2020: setCurrentAnimationTime builds its normals from the
	//base model's flat normals.
	if(!old.mode&&m) 
	{
		//Note: This is called below too (after _source is set)
		//but with different criteria.
		{
			validateNormals();
		}
	}	
	m_animationMode = m;
	m_skeletalMode = m&1&&m_skeletalMode2;
	
	Undo<MU_ChangeAnimState>(this,this,old);

		for(auto*ea:m_vertices) ea->_source(m);
		for(auto*ea:m_triangles) ea->_source(m);
		for(auto*ea:m_points) ea->_source(m);
		for(auto*ea:m_joints) ea->_source(m);

	if(!m&&old.mode)
	{
		//SelectTool is failing to find bones? Might want to do this
		//unconditionally, but setCurrentAnimationTime is currently
		//required to get animation working properly (leave the time
		//up to the user instead of setting it to 0 here.)
		calculateSkel();

		//2021: setNoAnimation had done this. I'm not positive
		//it's correct or required.
		{
			//Not sure what's best in this case?
			//if(1) calculateNormals(); else m_validNormals = false;
			validateNormals();
		}
	}
	else invalidateAnim();

	//return m!=ANIMMODE_NONE;
	return ret;
}
void Model::Vertex::_source(AnimationModeE m)
{
	m_absSource = m?m_kfCoord:m_coord;
}
void Model::Triangle::_source(AnimationModeE m)
{
	m_flatSource = m?m_kfFlatNormal:m_flatNormal;
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
	bool mm = 0!=(m&ANIMMODE_JOINT);
	m_absSource = mm?m_kfAbs():m_abs;
	m_rotSource = mm?m_kfRot:m_rot;
	m_xyzSource = mm?m_kfXyz:m_xyz;
}

bool Model::setCurrentAnimationFrame(unsigned frame, AnimationTimeE calc)
{	
	auto ab = _anim(m_currentAnim);

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
	double t = seconds*getAnimFPS(m_currentAnim);
	double len = getAnimTimeFrame(m_currentAnim);
	if((float)t>=len) //Truncate somewhat.
	{
		m_elapsedTime = t; //2022

		t = fmod(t,len); //Note, len may be zero.

		switch(loop) //2019
		{
		case -1:

			if(!getAnimWrap(m_currentAnim))
			{
			case 0: return false;
			}
				
		default: case 1: break;
		}
	}	
	return setCurrentAnimationFrameTime(t,calc);
}

bool Model::setCurrentAnimationElapsedFrameTime(double time, AnimationTimeE calc)
{
	auto anim = m_currentAnim;
	auto am = m_animationMode; if(!am) return false;

	double len = getAnimTimeFrame(anim);

	m_elapsedTime = time; //2022

	time = fmod(time,len); //Note, len may be zero.

	return setCurrentAnimationFrameTime(time,calc);
}
bool Model::setCurrentAnimationFrameTime(double time, AnimationTimeE calc)
{
	auto anim = m_currentAnim;
	auto am = m_animationMode; if(!am) return false;

	double len = getAnimTimeFrame(anim);
	
	/*2020
	//This should be based on getAnimTimeFrame but
	//should time be limited?
	if(anim<m_anims.size())
	{
		if(!m_anims[anim]->_frame_count()) time = 0;
	}
	else time = 0;*/
	if(time>0) //Best practice?
	time = std::min(time,len);
	else time = 0;
	
	unsigned f = time?getAnimFrame(anim,time):0;

	//2020: Stop animating still images?
	//(Don't emit AnimationFrame change) 
	if(f==m_currentFrame&&time==m_currentTime)
	{
		if(calc&AT_calculateNormals)
		validateAnim();
		if(calc==AT_calculateNormals)
		validateNormals();

		if(time!=fmod(m_elapsedTime,len)) //2022
		m_elapsedTime = time;

		return len?true:false; 
	}
	else assert(len);
	
	m_changeBits |= AnimationFrame; 

	invalidateBspTree();

	//m_currentFrame = (unsigned)(frameTime/spf);
	m_currentFrame = f;
	m_currentTime = time;

	if(time!=fmod(m_elapsedTime,len)) //2022
	{
		m_elapsedTime = time;
	}
	
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
	//2021: movePositionUnanimated may be in animation
	//mode, plus there's no longer a clean split
	//between animation off or on.
	//if(inJointAnimMode()) 
	m_validAnimJoints = false;

	m_validJoints = false; 
}
void Model::invalidateAnim()
{
	m_validAnim = m_validAnimJoints = false;
}
void Model::invalidateAnim(unsigned b, unsigned c)
{
	if(b==m_currentAnim&&m_currentFrame-c<=1)
	{	
		invalidateAnim();
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
	//LOG_PROFILE(); //???

	m_validJoints = true;

	if(inJointAnimMode()) //2020
	{
		invalidateAnim(); //invalidateNormals?
	}

	//log_debug("validateSkel()\n"); //???

	//2021: Must do in order even if parentage
	//is reordered.
	//for(unsigned j=0;j<m_joints.size();j++)
	for(auto&ea:m_joints2)
	{		
		//unsigned j = ea.first;
		//Joint *jt = m_joints[j];
		Joint *jt = ea.second;

		jt->m_relative.loadIdentity();
		jt->m_relative.setRotation(jt->m_rot);
		jt->m_relative.scale(jt->m_xyz);
		jt->m_relative.setTranslation(jt->m_rel);

		if(jt->m_parent>=0) // parented?
		{
			jt->m_absolute = jt->m_relative * m_joints[jt->m_parent]->m_absolute;
		}
		else jt->m_absolute = jt->m_relative;

		//TODO: Is it possible to build m_inv here with relative ease?
		//Can Euler be trivially inverted?
		jt->_dirty_mask = ~0; //2020

		//REMINDER: This matrix is going to be animated. Not sure why it's
		//set here.
		jt->m_final = jt->m_absolute;

//		//log_debug("\n");
//		//log_debug("Joint %d:\n",j);
//		jt->m_final.show();
//		//log_debug("local rotation: %.2f %.2f %.2f\n",jt->m_rot[0],jt->m_rot[1],jt->m_localRotation[2]);
//		//log_debug("\n");

		//NEW: For object system. Will try to refactor this at some point.
		jt->m_absolute.getTranslation(jt->m_abs);
	}
}
void Model::calculateAnimSkel()
{
	m_validAnimJoints = true;

	auto anim = m_currentAnim;

	invalidateAnimNormals();

	//const unsigned f = getAnimFrame(am,anim,t);
	const unsigned f = m_currentFrame;

	//double t = m_currentTime*getAnimFPS(am,anim);
	double t = m_currentTime;

	if(inJointAnimMode())
	{
		//LOG_PROFILE(); //???

		validateSkel();

		auto sa = m_anims[anim]; assert(sa->_type&1);

		Matrix transform;

		//2021: Must do in order even if parentage
		//is reordered.
		//auto jN = m_joints.size();
		//for(Position j{PT_Joint,0};j<jN;j++)
		for(auto&ea:m_joints2)
		{
			Position j{PT_Joint,ea.first};

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

			auto jt = ea.second;
			int jp = jt->m_parent;			
			Matrix &b = jt->m_relative;
			Matrix &c = jp>=0?m_joints[jp]->m_final:m_localMatrix;

			jt->_dirty_mask|=~1; //2021

			jt->m_final = transform * b * c;

			//LOCAL or GLOBAL? (RELATIVE or ABSOLUTE?)
			//https://github.com/zturtleman/mm3d/issues/35
			//NEW: For object system. Will try to refactor this at some point.
			//transform.getRotation(jt->m_kfRot);
			memcpy(jt->m_kfRel,trans,sizeof(trans)); //relative?
			memcpy(jt->m_kfRot,rot,sizeof(rot)); //relative?
			memcpy(jt->m_kfXyz,scale,sizeof(scale)); //relative?
		}
	}
}
void Model::calculateAnim()
{
	m_validAnim = true;

	if(!m_animationMode||!_anim_check()) return;

	invalidateAnimNormals();

	//NOTE: This exists so when valid joint (m_final) data
	//is required but vertex data isn't the vertex data can
	//wait until it's required (e.g. draw).
	if(inJointAnimMode()) validateAnimSkel();

	//Could avoid if _source was not the animation vector
	//or if they were initialized by setCurrentAnimation.
	//if(inFrameAnimMode()||inSkeletalMode())
	{
		for(unsigned v=m_vertices.size();v-->0;)
		{
			m_vertices[v]->_resample(*this,v);
		}

		for(unsigned p=m_points.size();p-->0;)
		{
			m_points[p]->_resample(*this,p);
		}
	}
}
void Model::Vertex::_resample(Model &model, unsigned v)
{		
	Vector source; 
	if(model.inFrameAnimMode())
	model.interpKeyframe(model.m_currentAnim,
	model.m_currentFrame,model.m_currentTime,v,source.getVector());
	else source.setAll(m_coord);

	if(!m_influences.empty()&&model.inSkeletalMode())	
	model._skel_xform_abs(1,m_influences,source);	
	for(int i=3;i-->0;) m_kfCoord[i] = source[i];
}
void Model::Point::_resample(Model &model, unsigned pt)
{
	int am; if(2&(am=model.m_animationMode))
	model.interpKeyframe(model.m_currentAnim,
	model.m_currentFrame,model.m_currentTime,{PT_Point,pt},m_kfAbs,m_kfRot,m_kfXyz);

	if(!m_influences.empty()&&model.inSkeletalMode())
	{
		//NOTE: Historically this was done with matrices.
		//Componentwise would probably be an improvement.
		Matrix mm = 2&am?getMatrix():getMatrixUnanimated();
		if(model._skel_xform_mat(1,m_influences,mm))
		{
			for(int i=3;i-->0;)
			{
				//FIX ME
				//I HAVE A FEELING THIS ISN'T ORTHONORMAL
				//(AND MAY INTRODUCE A SCALING COMPONENT)
				//ESPECIALLY IF BLENDING WEIGHTS
				m_kfAbs[i] = mm.getVector(3)[i];
				m_kfXyz[i] = normalize3(mm.getVector(i));		
				if(fabs(1-m_kfXyz[i])<=0.000005)
				{
					m_kfXyz[i] = 1;
				}
			}
			mm.getRotation(m_kfRot);
		}
		else goto uninfluenced;
	}
	else uninfluenced:
	{
		if(0==(2&am)) for(int i=3;i-->0;)
		{
			m_kfAbs[i] = m_abs[i]; m_kfRot[i] = m_rot[i];
			m_kfXyz[i] = m_xyz[i];
		}
	}
}
bool Model::_skel_xform_abs(int inv, infl_list &l, Vector &io)
{
	auto mf = &Joint::getSkinMatrix;

	if(inv==-1)
	{
		mf = &Joint::getSkinverseMatrix;

		//REMOVE ME
		//Unfortunately there's not a single solution
		//in this case and the problem appears pretty
		//intractible?
		if(l.size()>1) //return false;
		{
			//TODO: Is there a reversible algorithm??
			//Maybe just move on highest/even weight?

			return false;
		}
	}

	double sum[3] = {}, total = 0;
	for(auto&ea:l) //if(ea.m_weight>0.00001) //???
	{
		Vector vert = io;
		vert.transform((m_joints[ea.m_boneId]->*mf)());
		for(int i=3;i-->0;)
		sum[i]+= ea.m_weight*vert[i];
		total += ea.m_weight;
	}
	if(total) //zero divide?
	{
		total = 1/total;
		for(int i=3;i-->0;) sum[i]*=total;

		io.setAll(sum); return true;
	}
	return false;
}
bool Model::_skel_xform_rot(int inv, infl_list &l, Matrix &io)
{
	auto mf = &Joint::getSkinMatrix;

	if(inv==-1)
	{
		mf = &Joint::getSkinverseMatrix;

		//REMOVE ME
		//Unfortunately there's not a single solution
		//in this case and the problem appears pretty
		//intractible?
		if(l.size()>1) return false;
	}

	//NOTE: Historically this was done with matrices.
	//Componentwise would probably be an improvement.
	bool nonzero = false;
	double axis[3][3] = {}, total = 0; 
	Matrix m;
	for(auto&ea:l) if(ea.m_weight>0.00001)
	{
		m = io * (m_joints[ea.m_boneId]->*mf)();
		double wt = ea.m_weight;
		for(int i=3;i-->0;)
		for(int j=3;j-->0;)
		axis[j][i]+=wt*m.get(j,i); //FIX ME //OVERKILL
		nonzero = true;
	}
	if(nonzero) for(int i=3;i-->0;)
	{
		normalize3(axis[i]);
		for(int j=3;j-->0;)
		io.set(j,i,axis[j][i]); //FIX ME //OVERKILL
	}
	return nonzero;
}
bool Model::_skel_xform_mat(int inv, infl_list &l, Matrix &io)
{
	auto mf = &Joint::getSkinMatrix;

	if(inv==-1)
	{
		mf = &Joint::getSkinverseMatrix;

		//REMOVE ME
		//Unfortunately there's not a single solution
		//in this case and the problem appears pretty
		//intractible?
		if(l.size()>1) return false;
	}

	//NOTE: Historically this was done with matrices.
	//Componentwise would probably be an improvement.
	bool nonzero = false;
	double axis[3+1][3] = {}, total = 0; 
	Matrix m;
	for(auto&ea:l) //if(ea.m_weight>0.00001) //???
	{
		m = io * (m_joints[ea.m_boneId]->*mf)();
		double wt = ea.m_weight;
		for(int i=3;i-->0;)
		for(int j=4;j-->0;)
		axis[j][i]+=wt*m.get(j,i); //FIX ME //OVERKILL
		total+=wt;
	}
	if(total)
	{		
		//NOTE! Not renormalizing (Caller is responsible, I'm not
		//even sure, it's required... doing this with matrices is
		//unorthodox.)
		total = 1/total;
		for(int i=3;i-->0;)
		for(int j=4;j-->0;)
		io.set(j,i,axis[j][i]*total); //FIX ME //OVERKILL
		return true;
	}
	return false;
}

double Model::getCurrentAnimationTime()const
{
	//return m_currentTime;
	double fps = getAnimFPS(m_currentAnim);
	//2021: I'm a little paranoid of divide by zero.
	return fps<=0?0:m_currentTime/fps;
}

Model::AnimationModeE Model::getAnimType(unsigned anim)const
{
	if(auto ab=_anim(anim)) 
	return (AnimationModeE)ab->_type;
	return ANIMMODE_NONE;
}
unsigned Model::getAnimationIndex(AnimationModeE m)const
{
	unsigned o = 0; for(auto*ea:m_anims)
	{
		if(ea->_type>=m) break; o++;
	}
	return o;
}
int Model::getAnim(AnimationModeE type, unsigned subindex)const
{
	unsigned o = 0; for(auto*ea:m_anims)
	{
		if(ea->_type==type&&!subindex--) return o; o++;
	}
	return -1;
}
unsigned Model::getAnimationCount(AnimationModeE m)const
{
	unsigned o = 0; for(auto*ea:m_anims) 
	{
		if(ea->_type==m) o++; //TODO: Keep totals?
	}
	return o;
}
const char *Model::getAnimName(unsigned anim)const
{
	if(auto*ab=_anim(anim)) 
	return ab->m_name.c_str(); return nullptr;
}

unsigned Model::getAnimFrameCount(unsigned anim)const
{
	if(auto*ab=_anim(anim)) return ab->_frame_count(); return 0;
}

unsigned Model::getAnimFrame(unsigned anim, double t)const
{
	if(auto*ab=_anim(anim)) 
	for(auto&tt=ab->m_timetable2020;!tt.empty();)
	{
		auto lb = std::upper_bound(tt.begin(),tt.end(),t);
		return unsigned(lb==tt.begin()?0:lb-tt.begin()-1);
	}
	return 0; //return -1; //insertAnimFrame expects zero?
}

double Model::getAnimFPS(unsigned anim)const
{
	Animation *ab = _anim(anim); return ab?ab->m_fps:0;
}

bool Model::getAnimWrap(unsigned anim)const
{
	Animation *ab = _anim(anim); return ab?ab->m_wrap:false; //true
}

double Model::getAnimTimeFrame(unsigned anim)const
{
	Animation *ab = _anim(anim); return ab?ab->_time_frame():0;
}
double Model::getAnimFrameTime(unsigned anim, unsigned frame)const
{
	if(auto*ab=_anim(anim)) 
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
	auto *fa = _anim(anim,ANIMMODE_FRAME);
	auto fp = fa?fa->m_frame0:~0;

	if(vertex<m_vertices.size())
	if(~fp&&frame<fa->_frame_count())
	{	
		FrameAnimVertex *fav = m_vertices[vertex]->m_frames[fp+frame];
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

Model::KeyMask2021E Model::hasKeyframeData(unsigned anim, int incl)const
{
	//REMINDER: This supports convertAnimToType.

	Animation *ab = _anim(anim); if(!ab) return KM_None;

	unsigned mask = 0; 
	if(int inc2=incl&~KM_Vertex)	
	for(auto&ea:ab->m_keyframes) if(!ea.second.empty())
	{
		mask|=1<<ea.first.type;

		if(mask==inc2) break; //Early out in simple case?
	}
	if(incl&KM_Vertex) if(~ab->m_frame0) //ANIMMODE_FRAME
	{
		auto fc = ab->_frame_count(); 
		auto fp = ab->m_frame0;
		auto fd = fp+fc;
		if(fc) //OUCH
		for(auto*ea:m_vertices) 		
		for(auto f=fp;f<fd;f++)
		if(ea->m_frames[f]->m_interp2020)
		return KeyMask2021E(incl&(mask|KM_Vertex));
	}
	return KeyMask2021E(incl&mask);
}

Model::Interpolate2020E Model::hasKeyframe(unsigned anim, unsigned frame,
		Position pos, KeyType2020E isRotation)const
{
	Animation *ab = _anim(anim,frame,pos,false); if(!ab) return InterpolateNone;

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
	Animation *ab = _anim(anim,frame,pos,false); if(!ab) return InterpolateNone;

	//Keyframe *kf = Keyframe::get(); //STUPID
	Keyframe kf; 

	kf.m_frame		  = frame;
	kf.m_isRotation	= isRotation;

	auto it = ab->m_keyframes.find(pos); //2021

	unsigned index;
	Interpolate2020E found = InterpolateNone;
	if(it!=ab->m_keyframes.end()
	 &&it->second.find_sorted(&kf,index))
	{
		auto &f = *ab->m_keyframes[pos][index];

		//log_debug("found keyframe anim %d,frame %d,joint %d,%s\n",anim,frame,joint,isRotation?"rotation":"translation");
		x = f.m_parameter[0];
		y = f.m_parameter[1];
		z = f.m_parameter[2];

		//FIX ME: Needs InterpolateStep/InterpolateNone options.
		found = f.m_interp2020; 
	}
	else
	{
		//log_debug("could not find keyframe anim %d,frame %d,joint %d,%s\n",anim,frame,joint,isRotation?"rotation":"translation");
	}
	//kf->release();

	return found;
}

void Model::Keyframe::lerp(const double x[3], const double y[3], double t, double z[3])
{
	for(int i=3;i-->0;) z[i] = t*(y[i]-x[i])+x[i]; //DUPLICATE 
}
void Model::FrameAnimVertex::lerp(const double x[3], const double y[3], double t, double z[3])
{
	for(int i=3;i-->0;) z[i] = t*(y[i]-x[i])+x[i]; //DUPLICATE
}
void Model::Keyframe::slerp(const double qx[4], const double qy[4], double t, double qz[4])
{
	auto &x = *(const Quaternion*)qx;
	auto &y = *(const Quaternion*)qy;

	#ifdef NDEBUG
//				#error Should use slerp algorithm! (maybe?)
	//https://github.com/zturtleman/mm3d/issues/125
	#endif				

	// Negate if necessary to get shortest rotation path for
	// interpolation
	auto &z = *(Quaternion*)qz; if(x.dot4(y)<-0.00001) 
	{
		//NOTE: It's odd to negate all four since that's the
		//same rotation but represented by different figures.
		//I couldn't find examples so I checked it to see it
		//works for the below math.
		//for(int i=0;i<4;i++) y[i] = -y[i];
		for(int i=0;i<4;i++) qz[i] = -qy[i];

		z = x*(1-t)+z*t; //Here z is -y.
	}
	else z = x*(1-t)+y*t; z.normalize();
}
int Model::interpKeyframe(unsigned anim, unsigned frame,
	unsigned pos, double trans[3])const
{
	auto ab = _anim(anim,ANIMMODE_FRAME);
	return interpKeyframe(anim,frame,ab?ab->_frame_time(frame):0,pos,trans); 
}
int Model::interpKeyframe(unsigned anim, unsigned frame,
	Position pos, Matrix &relativeFinal)const
{
	auto ab = _anim(anim);
	return interpKeyframe(anim,frame,ab?ab->_frame_time(frame):0,pos,relativeFinal); 
}
int Model::interpKeyframe(unsigned anim, unsigned frame,
	Position pos, double trans[3], double rot[3], double scale[3])const
{
	auto ab = _anim(anim);
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
	else if(Animation*ab=_anim(anim,frame,pos,false))
	{
		auto &tt = ab->m_timetable2020;
		//auto &jk = ab->m_keyframes[pos]; 
		Keyframe **jk; size_t jk_size;
		auto it = ab->m_keyframes.find(pos);
		if(it!=ab->m_keyframes.end())
		{
			jk = it->second.data();
			jk_size = it->second.size();
		}
		else jk_size = 0;
	   
		//TODO: Binary search? //FIX ME
		unsigned first[3] = {};
		unsigned key[3] = {}, stop[3] = {};
		unsigned last[3] = {};
		for(unsigned k=0;k<jk_size;)
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
				if(t) //dp?
				{
					/*REFERENCE
					{
						Vector va(pp);
						//if(t) //interpolate?
						va+=(Vector(dp)-va)*t;
						va.getVector3(x);
					}*/
					Keyframe::lerp(pp,dp,t,x);
				}
				else memcpy(x,pp,sizeof(*x)*3);				
			}
			else if(t) //dp? //InterpolantRotation
			{
				Quaternion va; va.setEulerAngles(pp);
				Quaternion vb; vb.setEulerAngles(dp);
								
				/*REFERENCE
				{				
					#ifdef NDEBUG
	//				#error Should use slerp algorithm!
					//https://github.com/zturtleman/mm3d/issues/125
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
				}*/
				double *x = va.getVector();
				Keyframe::slerp(x,vb.getVector(),t,x);

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

	Animation *ab = nullptr;
	if(anim<m_anims.size()&&pos<m_vertices.size()&&trans)
	ab = m_anims[anim];
	if(ab&&0==~ab->m_frame0) //ANIMMODE_FRAME
	ab = nullptr;

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
