/* MM3D (Multimedia 3D)
 *
 * This code is an original contribution not wishing to 
 * be subjected to any form of copyright based thoughts.
 * 
 */

#include "mm3dtypes.h" //PCH

#include "model.h"

#include "modelundo.h"
#include "modelstatus.h"
#include "log.h"

//2022: Utilities are somewhat like metadata but
//with some actual/plugin support infrastructure.

int Model::addUtility(Model::UtilityTypeE type, const char *name)
{
	if(!name) return false; 
	
	if(!name[0]) name = "_";

	auto up = Utility::get(type);

	//All setting methods assumes this is set/nonzero.
	up->model = this;
	
	up->name = name;

	auto index = m_utils.size();

	Undo<MU_Add>(this,index,up);

	_insertUtil(index,up); return (int)index;
}
void Model::_insertUtil(unsigned index, Utility *up)
{	
	if(up->type==UT_UvAnimation)
	m_changeBits |= AddAnimation;
	m_changeBits |= AddUtility;

	m_utils.insert(m_utils.begin()+index,up);
}
bool Model::deleteUtility(unsigned index)
{
	if(index>=m_utils.size()) return false;

	auto up = m_utils[index];

	if(m_undoEnabled)
	{
		Undo<MU_Delete>(this,index,up);

		Undo<MU_AssocUtility> undo; 
		{		
			if(up->assoc&PartGroups)
			{
				for(auto*gp:m_groups) //removeGroupFromUtility?
				{
					auto &l = gp->m_utils;

					for(auto&ea:l) if(up==ea)
					{
						if(!undo)
						undo = Undo<MU_AssocUtility>(this,up);
						undo->removeAssoc(gp);

						l.erase(l.begin()+(&ea-l.data()));

						break;
					}
				}
			}
			else assert(0==up->assoc);
		}
	}
	else up->release();

	_removeUtil(index); return true;
}
void Model::_removeUtil(unsigned index)
{		
	if(m_utils[index]->type==UT_UvAnimation)
	m_changeBits |= AddAnimation;
	m_changeBits |= AddUtility;

	m_utils.erase(m_utils.begin()+index);
}
bool Model::setUtilityName(unsigned index, const char *name)
{
	if(!name||index>=m_utils.size()) return false;

	if(!name[0]) name = "_";

	auto *up = m_utils[index];

	if(up->name!=name)
	{
		//if(up->type==UT_UvAnimation)
		//m_changeBits |= AddAnimation; //See moveUtility comment.
		m_changeBits |= AddUtility;

		Undo<MU_SwapStableStr>(this,AddUtility,up->name);

		up->name = name;
	}
	
	return true;
}
bool Model::convertUtilityToType(Model::UtilityTypeE type, unsigned index)
{
	if(index>=m_utils.size()) return false;

	auto it = m_utils.begin()+index; 
	auto uup = *it;
	
	if(uup->type==type) return true;

	auto up = Utility::get(type);

	//All setting methods assumes this is set/nonzero.
	up->model = this;
		
	up->name = uup->name;

	if(up->assoc==uup->assoc)
	{
		Undo<MU_Delete> undo(this,index,uup);

		if(!undo) uup->release();

		_removeUtil(index);
	}
	else deleteUtility(index);

	_insertUtil(index,up); return true;
}
bool Model::moveUtility(unsigned oldIndex, unsigned newIndex)
{
	if(oldIndex==newIndex)
	return true;
	if(oldIndex>=m_utils.size()||newIndex>=m_utils.size())
	return false;
	
	//NOTE: AddAnimation is more for toggling animation controls
	//than lists of names. To be correct here this would need to
	//scan all displaced names.
	//if(up->type==UT_UvAnimation)
	//m_changeBits |= AddAnimation; //I don't know? Displacement?
	m_changeBits |= AddUtility;

	auto up = m_utils[oldIndex];

	m_utils.erase(m_utils.begin()+oldIndex);
	m_utils.insert(m_utils.begin()+newIndex,up);
	
	Undo<MU_MoveUtility>(this,oldIndex,newIndex);

	return true;
}
bool Model::addGroupToUtility(unsigned index, unsigned group, bool how)
{
	if(index>=m_utils.size()||group>=m_groups.size()) return false;

	auto up = m_utils[index]; if(~up->assoc&PartGroups) return false;
	auto gp = m_groups[group];
	
	if(!gp->_assoc_util(up,how)) return false;	
	
	if(m_undoEnabled)
	{
		Undo<MU_AssocUtility> undo(this,up);
		if(how) undo->addAssoc(gp);
		else undo->removeAssoc(gp);
	}

	return true;
}
bool Model::Group::_assoc_util(Utility *up, bool how)
{
	auto &l = m_utils;

	int j = -1; for(auto&ea:l)
	{
		if(ea==up) j = &ea-l.data();
	}
	if(how)
	{
		if(j!=-1) return false;	

		l.push_back(up);
	}
	else if(j!=-1)
	{
		l.erase(l.begin()+j);
	}
	else return false; return true;
}

/////////////////// Utility ///////////////////

Model::Utility *Model::Utility::get(UtilityTypeE e)
{
	switch(e)
	{
	default: assert(0);
	case UT_NONE: return new Utility(UT_NONE,0);
	case UT_UvAnimation: return new UvAnimation;
	}
}
void Model::Utility::release()
{
	//NOTE: This is forgoing the usual recycle pattern
	//since dynamic_cast would be needed and Utilities
	//aren't high volume.
	delete this; 
}
void Model::Utility::_set(const void *p, const void *cp, size_t sz)const
{
	if(Undo<MU_SwapStableMem>undo=model)
	undo->setMemory(model->getChangeBits(),p,cp,sz);
	memcpy(const_cast<void*>(p),cp,sz);
}

////////////////// UvAnimation //////////////////

void Model::UvAnimation::_dirty()const
{
	model->m_changeBits|=AnimationProperty;

	const_cast<double&>(_cur_time) = -1;
}
void Model::UvAnimation::_make_cur()const
{
	double et = model->m_elapsedTime;

	if(_cur_time==et) return;

	const_cast<double&>(_cur_time) = et;

	et/=model->getAnimFPS(model->m_currentAnim);
	et*=fps;	

	_interp_keys(_cur_key,et);
	_refresh_texture_matrix();
}
void Model::UvAnimation::_interp_keys(Key &k, double time)const
{	
	time = fmod(time,frames);

	new(&k) Key; k.frame = time;

	if(keys.empty()) return;
		
	//NOTE: I'm sure I've probably messed up
	//some subtle things here... I should've
	//copied from interpKeyframe but instead
	//I just spitballed to see what a binary
	//search might be like.

	auto iit = keys.begin();
	auto itt = keys.end();
	auto lb = std::lower_bound(iit,itt,_cur_key);
	if(lb==itt) lb--;

	for(int i=0;i<3;i++)
	{
		auto jt = lb;
		while(jt>iit&&!(&jt->r)[i])
		jt--;
		auto jtt = lb;
		if(jtt->frame<time) 
		jtt++;
		while(jtt<itt&&!(&jtt->r)[i])
		jtt++;
		if(jtt==itt&&itt[-1].frame>=time)
		jtt = itt-1;

		if(wrap)
		if(time<jt->frame||jtt==itt||time>jtt->frame)
		{
			jtt = jt; jt = itt-1;
				
			while(jt>iit&&!(&jt->r)[i]) jt--;
			while(jtt<itt&&!(&jtt->r)[i]) jtt++;
		}

		bool i0 = (&jt->r)[i]&&jt->frame<=time;
		
		//This is feedback to the caller to let them
		//know which keys are set.
		auto &ret = (&k.r)[i];

		double t = 0; if(jtt!=itt)
		{
			ret = (&jtt->r)[i];

			//QUOTE: interpKeyframe says the later key
			//supplies the interpolation mode owing to
			//the mode being undefined when the source
			//comes from the initial values.
			if((&jtt->r)[i]==InterpolateLerp)
			if(!i0||jt!=jtt)
			{
				double t0 = i0?jt->frame:0;
				double t1 = (&jtt->r)[i]?jtt->frame:t0;
				if(t0>t1)
				{
					t0-=frames; assert(wrap&&t1>t0);
				}
				if(t0!=t1) //Zero-divide?
				{			
					t = (time-t0)/(t1-t0); assert(t>=0&&t<=1);
				}
			}
		}
		if(!ret) ret = (&jt->r)[i];
		if(!ret) continue;

		double Key::*mp; switch(i)
		{
		case 0: mp = &Key::rz; break;
		case 1: mp = &Key::sx; break;
		case 2: mp = &Key::tx; break;
		}

		//Iterators don't have PtoM operators.
		const Key &a = *jt, &b = *jtt;

		for(int j=i?2:1;j-->0;)
		{
			double x = i0?(&(a.*mp))[j]:i==1;
			if(jtt!=itt)
			x+=((&(b.*mp))[j]-x)*t;			
			assert(ret||x==int(i==1)); //C4805 nonsense
			(&(k.*mp))[j] = x;
		}
	}
}
void Model::UvAnimation::_refresh_texture_matrix()const
{
	Key &k = _cur_key;
	double *m = _cur_texture_matrix.getMatrix();
	double cy = cos(k.rz);
	double sy = sin(k.rz);

	m[0] = cy*k.sx; m[1] = sy; //m[2] = m[3] = 0;

	m[4] = -sy*k.sy; m[5] = cy*k.sy; //m[6] = m[7] = 0;

	//m[8] = m[9] = 0; m[10] = 1; m[11] = 0;

	m[12] = k.tx; m[13] = k.ty; //m[14] = 0; m[15] = 1;
}
bool Model::UvAnimation::set_fps(double v)const
{
	_dirty(); return _set_if(v>=0,&fps,v); 
}
bool Model::UvAnimation::set_frames(double v)const
{
	_dirty(); return _set_if(v>=0,&frames,v); 
}
bool Model::UvAnimation::set_units(double u, double v)const
{
	model->m_changeBits|=AnimationProperty; //_dirty();

	return _set_if(u>=1&&v>=1,&_pack<2>(&unit),{u,v});
}
void Model::UvAnimation::set_wrap(bool v)const
{
	_dirty(); _set(&wrap,v); 
}
int Model::UvAnimation::set_key(const Key &v)const
{
	if(v.frame<0){ assert(0); return -1; }

	_dirty(); 

	Undo<MU_UvAnimKey> undo(model,this);

	auto it = std::lower_bound(keys.begin(),keys.end(),v);
	if(it!=keys.end()&&it->frame==v.frame)
	{
		if(undo) undo->assignKey(*it,v);

		_set_cast(*it) = v; 
	}
	else
	{
		if(undo) undo->insertKey(v);

		it = _set_cast(keys).insert(it,v);
	}

	if(v.frame>frames) set_frames(v.frame);

	return it-keys.begin();
}
bool Model::UvAnimation::delete_key(unsigned i)const
{
	if(i>=keys.size()) return false;

	_dirty();

	Undo<MU_UvAnimKey> undo(model,this);
		
	if(undo) undo->deleteKey(keys[i]);

	_set_cast(keys).erase(keys.begin()+i);

	return true; 
}