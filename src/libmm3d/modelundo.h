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


#ifndef __MODELUNDO_H
#define __MODELUNDO_H

#include "mm3dtypes.h"
#include "undo.h"
#include "model.h"
#include "glmath.h"
#include "sorted_list.h"

class ModelUndo : public Undo
{
public:

	enum{ resume=true };
	bool resume2(){ return true; } //YUCK

	static int s_allocated;
	ModelUndo(){ s_allocated++; };
	virtual ~ModelUndo(){ s_allocated--; };
		
	virtual void undo(Model*) = 0;
	virtual void redo(Model*) = 0;
};

template<class MU>
struct Model::Undo //2022
{
	Model *model;
	MU *ptr; bool send;
	MU *operator->(){ return ptr; }
	operator MU*(){ return ptr; }
	template<class...PP>
	Undo(Model *m, PP&&...pp):model(m),ptr()
	{
		if(send=m->m_undoEnabled)		
		{
			if constexpr(MU::resume) //C++17 //SFINAE?			
			if(ptr=m->m_undoMgr->resume<MU>())
			if(ptr->resume2(std::forward<PP>(pp)...))
			{
				send = false; return;
			}
			ptr = new MU(std::forward<PP>(pp)...);
		}
	}
	~Undo()
	{
		//if(send) model->sendUndo(ptr);
		if(send) model->m_undoMgr->addUndo(ptr,MU::resume);
	}

	Undo():model(),ptr(),send(){}

	Undo &operator=(Undo&& mv)
	{
		model = mv.model; assert(!ptr&&mv.ptr);		
		
		ptr = mv.ptr; mv.ptr = nullptr; //for good measure
		
		send = mv.send; mv.send = false; return *this;
	}

	void cancel()
	{
		if(send) delete ptr;ptr = nullptr;
	}
};

class MU_TranslateSelected : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size(){ return sizeof(MU_TranslateSelected); };

	//void setMatrix(const Matrix &rhs){ m_matrix = rhs; }
	//Matrix getMatrix()const{ return m_matrix; };
	MU_TranslateSelected(const double vec[3])
	{
		memcpy(m_vec,vec,sizeof(m_vec));
	}
	bool resume2(const double vec[3]);

private:
	//Matrix m_matrix;
	double m_vec[3];
};

class MU_RotateSelected : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size(){ return sizeof(MU_RotateSelected); };

	MU_RotateSelected(const Matrix &rhs, const double point[3]);
	Matrix getMatrix()const { return m_matrix; };

	bool resume2(const Matrix &rhs, const double point[3]);

private:

	Matrix m_matrix;
	double m_point[3];
};

class MU_ApplyMatrix : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size(){ return sizeof(MU_ApplyMatrix); };

	MU_ApplyMatrix(const Matrix &mat, Model::OperationScopeE scope);
	Matrix getMatrix()const { return m_matrix; };

	bool resume2(const Matrix &mat, Model::OperationScopeE scope);

private:

	Matrix m_matrix;
	Model::OperationScopeE m_scope;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SelectionMode : public ModelUndo
{
public:

	enum{ resume=false };

	virtual bool nonEdit(){ return true; }

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	MU_SelectionMode(Model::SelectionModeE mode, Model::SelectionModeE oldMode)
	{ m_mode = mode; m_oldMode = oldMode; };

private:

	Model::SelectionModeE m_mode;
	Model::SelectionModeE m_oldMode;
};

class MU_Select : public ModelUndo
{
public:

	MU_Select(Model::SelectionModeE mode):m_mode(mode)
	{}
	MU_Select(Model::SelectionModeE mode, int num, unsigned sel, unsigned old)
	:m_mode(mode)
	{
		setSelectionDifference(num,sel,old);		
	}

	bool resume2(Model::SelectionModeE mode)
	{
		return m_mode==mode;
	}
	bool resume2(Model::SelectionModeE mode, int num, unsigned sel, unsigned old)
	{
		return m_mode==mode?setSelectionDifference(num,sel,old):false;
	}

	virtual bool nonEdit(){ return true; }

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	Model::SelectionModeE getSelectionMode()const { return m_mode; };

	//YUCK: begin/endSelectionDifference
	typedef Model::_OrderedSelection OS;
	void setSelectionDifference(int number, OS &s, OS::Marker &oldS);
	bool setSelectionDifference(int number, unsigned selected, unsigned oldSelected);
		
	void toggle(int number);

	unsigned diffCount(){ return m_diff.size(); };

private:
	typedef struct _SelectionDifference_t
	{
		int number;
		unsigned selected;
		unsigned oldSelected;
		bool operator<(const struct _SelectionDifference_t &rhs)const
		{
			return (this->number<rhs.number);
		}
		bool operator== (const struct _SelectionDifference_t &rhs)const
		{
			return (this->number==rhs.number);
		}

	}SelectionDifferenceT;

	//2019: setSelectionDifference used this. It seems nuts/prevents
	//combining. 
	//typedef sorted_list<SelectionDifferenceT> SelectionDifferenceList;
	typedef std::vector<SelectionDifferenceT> SelectionDifferenceList;

	Model::SelectionModeE m_mode;
	SelectionDifferenceList m_diff;		
};

class MU_SelectAnimFrame : public ModelUndo
{
public:

	MU_SelectAnimFrame(unsigned anim):m_anim(anim){}

	bool resume2(unsigned anim){ return anim==m_anim; }

	virtual bool nonEdit(){ return true; }

	void undo(Model*m);
	void redo(Model*m);
	int combine(Undo*);

	unsigned size();

	void setSelectionDifference(unsigned frame, bool how)
	{
		m_list.push_back({frame,how});
	}

private:

	struct rec
	{
		unsigned frame; bool how;
	};

	unsigned m_anim; std::vector<rec> m_list;
};

class MU_SetSelectedUv : public ModelUndo
{
public:

	virtual bool nonEdit(){ return true; }

	void undo(Model*);
	void redo(Model*);
	int combine(Undo*); //UNUSED?

	unsigned size();

	MU_SetSelectedUv(const int_list &newUv, const int_list &oldUv);

	bool resume2(const int_list &newUv, const int_list &oldUv) //UNUSED?
	{
		m_newUv = newUv; return true; (void)oldUv;
	}

private:
	int_list m_oldUv,m_newUv; //TODO: std::swap
};

//TODO: Can replace with MU_SwapStableMem?
class MU_Hide : public ModelUndo
{
public:

	virtual bool nonEdit(){ return true; }

	void undo(Model*m);
	void redo(Model*m);
	int combine(Undo *);

	unsigned size();

	void setHideDifference(Model::Visible2022 *obj, unsigned layer);

private:

	void hide(Model*,int);

	struct rec
	{
		Model::Visible2022 *obj; int layer,old;
	};
	typedef std::vector<rec> HideDifferenceList;

	HideDifferenceList m_list;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_InvertNormals : public ModelUndo
{
public:

	void undo(Model*);
	void redo(Model*);
	int combine(Undo*);

	unsigned size();

	void addTriangle(int triangle);

	void addTriangles(const int_list &triangles);

	MU_InvertNormals(Model *sanity_check)
	{
		m_sanity = sanity_check->getTriangleCount();
	}	
	bool resume2(Model *sanity_check)
	{
		assert(m_sanity==sanity_check->getTriangleCount());
		return true;
	}

private:

	int_list m_triangles;

	int m_sanity;
};

class MU_MoveUnanimated : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	void addPosition(const Model::Position &pos, double x, double y, double z,
			double oldx, double oldy, double oldz);

private:

	struct MovePrimitiveT : Model::Position
	{
		double x;
		double y;
		double z;
		double oldx;
		double oldy;
		double oldz;
	};

	//TODO: Use unsorted_map/Position.
	typedef sorted_list<MovePrimitiveT> MovePrimitiveList;

	MovePrimitiveList m_objects;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetObjectUnanimated : public ModelUndo
{
public:

	//This has turned into a real mutant with 
	//the addition of Model::Undo.

	MU_SetObjectUnanimated(const Model::Position &pos):pos(pos)
	{}
	MU_SetObjectUnanimated(const Model::Position &pos,
	Model *m, double object[3], const double xyz[3]):pos(pos)
	{
		setXYZ(m,object,xyz);
	}

	bool resume2(const Model::Position &pos2,
	Model *m, double object[3], const double xyz[3]);

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	enum Vec{ Abs=0,Rot,Scale,Rel };

	void setXYZ(Model*, double object[3], const double xyz[3]);

	void _call_setter(Model*,double*);

	int _get_vector(Model*,const Model::Position&,double*);

private:

	Model::Position pos;
	int vec;
	double v[3], vold[3];
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetTexture : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	bool resume2(unsigned groupNumber, int newTexture, int oldTexture);

	MU_SetTexture(unsigned groupNumber, int newTexture, int oldTexture)
	{
		resume2(groupNumber,newTexture,oldTexture); //setTexture
	}

private:
	typedef struct _SetTexture_t
	{
		unsigned groupNumber;
		int newTexture;
		int oldTexture;
	} SetTextureT;
	typedef std::vector<SetTextureT> SetTextureList;

	SetTextureList m_list;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetTextureCoords : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	void addTextureCoords(unsigned triangle, unsigned vertexIndex, float s, float t, float oldS, float oldT);

private:
	typedef struct _SetTextureCoords_t
	{
		unsigned triangle;
		unsigned vertexIndex;
		float t;
		float s;
		float oldT;
		float oldS;
		bool operator<(const struct _SetTextureCoords_t &rhs)const
		{
			return (this->triangle<rhs.triangle
					||(this->triangle==rhs.triangle&&this->vertexIndex<rhs.vertexIndex));
		};
		bool operator==(const struct _SetTextureCoords_t &rhs)const
		{
			return (this->triangle==rhs.triangle&&this->vertexIndex==rhs.vertexIndex);
		};
	} SetTextureCoordsT;

	typedef sorted_list<SetTextureCoordsT> STCList;

	STCList m_list;
};

class MU_AddToGroup : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	bool resume2(unsigned tri, int grp, int old);

	MU_AddToGroup(unsigned tri, int grp, int old)
	{
		resume2(tri,grp,old); //addToGroup
	}

private:
	typedef struct _AddToGroup_t
	{
		unsigned triangleNum;
		int groupNum,groupOld;		
	} AddToGroupT;
	typedef std::vector<AddToGroupT> AddToGroupList;

	AddToGroupList m_list;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetLightProperties : public ModelUndo
{
public:
	enum _LightType_e
	{
		LightAmbient  = 0,
		LightDiffuse,
		LightSpecular,
		LightEmissive,
		LightTypeMax  
	};
	typedef enum _LightType_e LightTypeE;

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	void setLightProperties(unsigned textureNum,LightTypeE type, const float *newLight, const float *oldLight);

private:
	typedef struct _LightProperties_t
	{
		unsigned textureNum;
		float newLight[LightTypeMax][4];
		float oldLight[LightTypeMax][4];
		bool  isSet[LightTypeMax];
	} LightPropertiesT;

	typedef std::vector<LightPropertiesT> LightPropertiesList;

	LightPropertiesList m_list;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetShininess : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	void setShininess(unsigned textureNum, const float &newValue, const float &oldValue);

private:
	typedef struct _Shininess_t
	{
		unsigned textureNum;
		float oldValue;
		float newValue;
	} ShininessT;

	typedef std::vector<ShininessT> ShininessList;

	std::vector<ShininessT> m_list;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetTriangleVertices : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	void setTriangleVertices(unsigned triangleNum,
			unsigned newV1, unsigned newV2, unsigned newV3,
			unsigned oldV1, unsigned oldV2, unsigned oldV3);

private:
	typedef struct _TriangleVertices_t
	{
		unsigned triangleNum;
		unsigned newVertices[3];
		unsigned oldVertices[3];
	} TriangleVerticesT;

	typedef std::vector<TriangleVerticesT> TriangleVerticesList;

	TriangleVerticesList m_list;
};

class MU_SubdivideSelected : public ModelUndo
{
public:

	enum{ resume=false };

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size(){ return sizeof(MU_SubdivideSelected); };

private:
};
class MU_SubdivideTriangle : public ModelUndo
{
public:

	enum{ resume=false };

	void undo(Model*);
	void redo(Model*){ /*NOP*/ }
	int combine(Undo *);

	unsigned size();

	void subdivide(unsigned a, unsigned b, unsigned c, unsigned d);
	void addVertex(unsigned v);

private:

	typedef struct _SubdivideTriangle_t
	{
		unsigned a;
		unsigned b;
		unsigned c;
		unsigned d;
	} SubdivideTriangleT;

	//FIX ME?
	typedef std::vector<SubdivideTriangleT> SubdivideTriangleList;
	SubdivideTriangleList m_list;
	std::vector<unsigned> m_vlist;
};

class MU_ChangeAnimState : public ModelUndo
{
public:

	enum{ resume=false };

	virtual bool nonEdit(){ return true; }

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	MU_ChangeAnimState(Model *newInfo, const Model::RestorePoint &old)
	:m_new(newInfo->makeRestorePoint()),m_old(old){}

protected:

	Model::RestorePoint m_new,m_old;
};

class MU_ChangeSkeletalMode : public ModelUndo
{
public:
		
	enum{ resume=false };

	MU_ChangeSkeletalMode(bool how):m_how(how){}

	virtual bool nonEdit(){ return true; }

	void undo(Model*),redo(Model*);

	int combine(Undo *); //???

	unsigned size(){ return sizeof(MU_ChangeSkeletalMode); }

private:

	bool m_how;
};

class MU_SetAnimFrameCount : public ModelUndo
{
public:

	enum{ resume=false };

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	void undoRelease();
	void redoRelease();

	unsigned size();

	MU_SetAnimFrameCount(unsigned animNum, unsigned newCount, unsigned oldCount, unsigned where);

	Model::FrameAnimData *removeVertexData(){ return &m_vertices; }

	void removeTimeTable(const std::vector<double> &tt)
	{
		if(m_oldCount>m_newCount) //Reminder: Dinkumware range checks +/- operators
		m_timetable.assign(tt.begin()+m_where,tt.begin()+(m_where+m_oldCount-m_newCount)); 
	}
	void removeSelection(const std::vector<char> &st)
	{
		if(!st.empty())
		if(m_oldCount>m_newCount) //Reminder: Dinkumware range checks +/- operators
		m_selection.assign(st.begin()+m_where,st.begin()+(m_where+m_oldCount-m_newCount)); 
	}

private:

	unsigned m_animNum;
	unsigned m_newCount;
	unsigned m_oldCount;		
	unsigned m_where; 

	std::vector<char> m_selection;
	std::vector<double> m_timetable;
	Model::FrameAnimData m_vertices;
};

class MU_MoveAnimFrame : public ModelUndo
{
public:

	enum{ resume=false };

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	MU_MoveAnimFrame(unsigned animNum, unsigned newFrame, unsigned oldFrame);

private:

	unsigned m_anim,m_new,m_old;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetAnimFPS : public ModelUndo
{
public:

	void undo(Model*),redo(Model*);

	int combine(Undo *);

	unsigned size();

	MU_SetAnimFPS(unsigned animNum, double newFps, double oldFps);

	bool resume2(unsigned animNum, double newFps, double oldFps)
	{
		if(animNum!=m_animNum) return false; (void)oldFps;

		m_newFPS = newFps; return true;
	}

private:

	unsigned m_animNum;
	double	m_newFPS;
	double	m_oldFPS;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetAnimWrap : public ModelUndo
{
public:
		
	void undo(Model*),redo(Model*);

	int combine(Undo *);

	unsigned size(){ return sizeof(MU_SetAnimWrap); }

	MU_SetAnimWrap(unsigned animNum, bool newWrap, bool oldWrap);

	bool resume2(unsigned animNum, bool newWrap, bool oldWrap)
	{
		if(animNum!=m_animNum) return false; (void)oldWrap;

		m_new = newWrap; return true;
	}

private:

	unsigned m_animNum;
	bool m_new, m_old;
};

//TODO: Can replace with MU_SwapStableMem?
//https://github.com/zturtleman/mm3d/issues/106
class MU_SetAnimTime : public ModelUndo
{
public:

	void undo(Model*),redo(Model*);

	int combine(Undo *);

	unsigned size(){ return sizeof(MU_SetAnimTime); }

	MU_SetAnimTime(unsigned animNum, double newTime, double oldTime);
	MU_SetAnimTime(unsigned animNum, unsigned frame, double newTime, double oldTime);

	bool resume2(unsigned animNum, double newTime, double oldTime)
	{
		if(animNum!=m_animNum) return false; (void)oldTime;

		m_newTime = newTime; return true;
	}
	bool resume2(unsigned animNum, unsigned frame, double newTime, double oldTime)
	{
		if(animNum!=m_animNum||frame!=m_animFrame) return false; (void)oldTime;

		m_newTime = newTime; return true;
	}

private:

	unsigned m_animNum;
	unsigned m_animFrame;
	double m_newTime;
	double m_oldTime;
};

class MU_SetObjectKeyframe : public ModelUndo
{
protected:

	void undo(Model*),redo(Model*);

	int combine(Undo *);

public:

	unsigned size();

	MU_SetObjectKeyframe(unsigned anim, unsigned frame)
	:m_anim(anim),m_frame(frame){}

	void addKeyframe(Model::Position j, 
	unsigned frame, Model::KeyType2020E isRotation, //2022
	bool isNew,
	double xyz[3], Model::Interpolate2020E e,
	double old[3], Model::Interpolate2020E olde);

	void addKeyframe(bool isNew, 
	Model::Keyframe *kf, double old[3], Model::Interpolate2020E olde)
	{
		addKeyframe(kf->m_objectIndex,
		kf->m_frame,kf->m_isRotation,isNew,kf->m_parameter,
		kf->m_interp2020,old,olde);
	}
		
	bool resume2(unsigned anim, unsigned frame)
	{
		return anim==m_anim; (void)frame;
	}

private:

	struct SetKeyFrameT : Model::Position
	{
		using Position::operator=; //C++

		bool operator<(const SetKeyFrameT &b) //2022
		{
			return memcmp(this,&b,(char*)&isNew-(char*)this)<0;
		}
		unsigned frame; //2022
		Model::KeyType2020E isRotation; //2022

		bool isNew;
		double xyz[3];
		double old[3];
		Model::Interpolate2020E e,olde;
	};
	typedef sorted_list<SetKeyFrameT> SetKeyframeList;

	SetKeyframeList m_list;
	unsigned m_anim;
	unsigned m_frame;
	//Model::KeyType2020E m_isRotation;
};

class MU_DeleteObjectKeyframe : public ModelUndo
{
protected:

	void undo(Model*),redo(Model*);

	int combine(Undo *);

public:

	void undoRelease();

	unsigned size();

	MU_DeleteObjectKeyframe(unsigned anim, unsigned frame)
	:m_anim(anim),m_frame(frame){}

	void deleteKeyframe(Model::Keyframe *keyframe);

	bool resume2(unsigned anim, unsigned frame)
	{
		return anim==m_anim&&frame==m_frame;
	}

private:
	typedef std::vector<Model::Keyframe*> DeleteKeyframeList;

	DeleteKeyframeList m_list;
	unsigned m_anim,m_frame;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetProjectionType : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	MU_SetProjectionType(unsigned projection, int newType, int oldType);

	bool resume2(unsigned projection, int newType, int oldType)
	{
		if(projection!=m_projection) return false; (void)oldType;

		m_newType = newType; return true;
	}

private:

	unsigned m_projection;
	int	m_newType;
	int	m_oldType;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_MoveFrameVertex : public ModelUndo
{
public:

	void undo(Model*),redo(Model*);

	int combine(Undo*);

	unsigned size();

	MU_MoveFrameVertex(unsigned anim, unsigned frame)
	:m_anim(anim),m_frame(frame){}

	void addVertex(int v, const double xyz[3],
	Model::Interpolate2020E e, Model::FrameAnimVertex *old, bool sort=true);

	bool resume2(unsigned anim, unsigned frame)
	{
		return anim==m_anim&&frame==m_frame;
	}

private:

	struct MoveFrameVertexT
	{
		int number;
		double xyz[3];
		double old[3];
		Model::Interpolate2020E e,olde;

		bool operator<(const MoveFrameVertexT &rhs)const
		{
			return number<rhs.number;
		};
		bool operator==(const MoveFrameVertexT &rhs)const
		{
			return number==rhs.number;
		};
	};
	typedef sorted_list<MoveFrameVertexT> MoveFrameVertexList;

	friend Model; //2020
	MoveFrameVertexList m_vertices;
	unsigned m_anim;
	unsigned m_frame;
		
	void addVertex(MoveFrameVertexT&); //2020
};

class MU_SetPositionInfluence : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	bool resume2(bool isAdd, const Model::Position &pos, unsigned index, const Model::InfluenceT &influence);

	MU_SetPositionInfluence(bool isAdd, const Model::Position &pos, unsigned index, const Model::InfluenceT &influence)
	{
		resume2(isAdd,pos,index,influence); //setPositionInfluence
	}

private:

	struct rec
	{
		bool m_isAdd;
		Model::Position m_pos;
		unsigned m_index;
		Model::InfluenceT m_influence;
	};
	std::vector<rec> m_list;
};

class MU_UpdatePositionInfluence : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	bool resume2(const Model::Position &pos,
			const Model::InfluenceT &newInf,
			const Model::InfluenceT &oldInf);

	MU_UpdatePositionInfluence(const Model::Position &pos,
			const Model::InfluenceT &newInf,
			const Model::InfluenceT &oldInf)
	{
		//updatePositionInfluence(pos,newInf,oldInf);
		resume2(pos,newInf,oldInf);
	}

private:

	struct rec
	{
		Model::Position m_pos;
		Model::InfluenceT m_newInf;
		Model::InfluenceT m_oldInf;
	};
	std::vector<rec> m_list;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetTriangleProjection : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	void setTriangleProjection(unsigned vertex, int newProj, int oldProj);

	MU_SetTriangleProjection()
	{}
	MU_SetTriangleProjection(unsigned vertex, int newProj, int oldProj)
	{
		setTriangleProjection(vertex,newProj,oldProj);
	}

private:
	typedef struct _SetProjection_t
	{
		unsigned triangle;
		int newProj;
		int oldProj;
	} SetProjectionT;
	typedef std::vector<SetProjectionT> SetProjectionList;

	SetProjectionList m_list;
};

class MU_AddAnimation : public ModelUndo
{
public:

	enum{ resume=false };

	MU_AddAnimation(unsigned a, Model::Animation *p)
	:m_anim(a),m_animp(p){}

	void undo(Model*),redo(Model*);

	int combine(Undo *);

	void redoRelease();

	unsigned size();

private:
		
	unsigned m_anim;
	Model::Animation *m_animp;
};

class MU_DeleteAnimation : public ModelUndo
{
public:

	enum{ resume=false };

	MU_DeleteAnimation(unsigned a, Model::Animation *p)
	:m_anim(a),m_animp(p){}

	void undo(Model*),redo(Model*);

	int combine(Undo *);

	void undoRelease();

	unsigned size();

	Model::FrameAnimData *removeVertexData(){ return &m_vertices; }

private:
		
	unsigned m_anim;
	Model::Animation *m_animp;

	Model::FrameAnimData m_vertices;
};
class MU_VoidFrameAnimation : public ModelUndo
{
//NOTE: this is needed because _frame_count is used to
//to tell how many frames are present, and m_frame0 is
//taken to mean there's frame data in Vertex::m_frames

public:

	enum{ resume=false };

	MU_VoidFrameAnimation(Model::Animation *p, unsigned frame0)
	:m_animp(p),m_frame0(frame0){}

	void undo(Model*),redo(Model*);

	int combine(Undo *);

	void undoRelease();

	unsigned size();

	Model::FrameAnimData *removeVertexData(){ return &m_vertices; }

private:
		
	Model::Animation *m_animp;
	unsigned m_frame0;

	Model::FrameAnimData m_vertices;
};

class MU_IndexAnimation : public ModelUndo
{
public:

	enum{ resume=false };

	MU_IndexAnimation(unsigned oldIndex, unsigned newIndex, int typeDiff=0)
	:m_oldIndex(oldIndex),m_newIndex(newIndex),m_typeDiff(typeDiff)
	{}

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

private:

	unsigned m_oldIndex,m_newIndex; int m_typeDiff;
};
class MU_Index : public ModelUndo
{
public:

	enum{ resume=false };

	MU_Index(Model::ComparePartsE type, unsigned oldIndex, unsigned newIndex)
	:m_oldIndex(oldIndex),m_newIndex(newIndex),m_type(type)
	{}

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

private:

	unsigned m_oldIndex,m_newIndex; Model::ComparePartsE m_type;
};

class MU_SetJointParent : public ModelUndo
{
public:

	enum{ resume=false };

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *){ return false; };

	unsigned size(){ return sizeof(MU_SetJointParent); };

	MU_SetJointParent(unsigned joint, int newParent, Matrix &redo, Model::Joint *old)
	:m_joint(joint),m_new(newParent)
	,m_old(old->m_parent),m_redo(redo),m_undo(old->m_relative)
	{}

protected:
	unsigned m_joint;
	int m_new,m_old;
	Matrix m_undo,m_redo; //2021
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetProjectionRange : public ModelUndo
{
public:

	void undo(Model*);
	void redo(Model*);
	int combine(Undo*);

	unsigned size(){ return sizeof(MU_SetProjectionRange); };

	MU_SetProjectionRange(unsigned proj, double (&old)[2][2])
	{
		m_proj = proj; memcpy(m_oldRange,old,sizeof(old));
	}
	bool resume2(unsigned proj, double (&old)[2][2])
	{
		return m_proj==proj;
	}

	void setProjectionRange(double XMin, double YMin, double XMax, double YMax);

protected:
	unsigned m_proj;
	double m_newRange[4];
	double m_oldRange[4];
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetGroupSmooth : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	bool resume2(unsigned group,
	const uint8_t &newSmooth, const uint8_t &oldSmooth);

	MU_SetGroupSmooth(unsigned group,
	const uint8_t &newSmooth, const uint8_t &oldSmooth)
	{
		resume2(group,newSmooth,oldSmooth); //setGroupSmooth
	}


private:
	typedef struct _SetSmooth_t
	{
		unsigned group;
		uint8_t newSmooth;
		uint8_t oldSmooth;
	} SetSmoothT;
	typedef std::vector<SetSmoothT> SetSmoothList;

	SetSmoothList m_list;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetGroupAngle : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	bool resume2(unsigned group,
	const uint8_t &newAngle, const uint8_t &oldAngle);

	MU_SetGroupAngle(unsigned group,
	const uint8_t &newAngle, const uint8_t &oldAngle)
	{
		resume2(group,newAngle,oldAngle); //setGroupAngle
	}

private:

	typedef struct _SetAngle_t
	{
		unsigned group;
		uint8_t newAngle;
		uint8_t oldAngle;
	} SetAngleT;
	typedef std::vector<SetAngleT> SetAngleList;

	SetAngleList m_list;
};

//TODO: Can replace with MU_SwapStableMem?
class MU_SetMaterialBool : public ModelUndo //MU_SetMaterialClamp
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	MU_SetMaterialBool(unsigned material, char how, bool newBool, bool oldBool);

	bool resume2(unsigned material, char how, bool newBool, bool oldBool=false)
	{
		if(material!=m_material||how!=m_how) return false;

		m_new = newBool; return true;
	}

private:
	unsigned m_material;
	//bool m_isS;
	char m_how;
	bool m_old;
	bool m_new;
};

class MU_SetMaterialTexture : public ModelUndo
{
public:

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	MU_SetMaterialTexture(unsigned material, Texture *newTex, Texture *old);

	bool resume2(unsigned material, Texture *newTex, Texture *old)
	{
		if(material!=m_material) return false; (void)old;

		m_newTexture = old; return true;
	}

private:
	unsigned m_material;
	Texture *m_oldTexture,*m_newTexture;
};

class MU_AddMetaData : public ModelUndo
{
public:

	enum{ resume=false };

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	void addMetaData(const std::string &key, const std::string &value);

	MU_AddMetaData(const std::string &key, const std::string &value);

private:

	std::string m_key,m_value;
};

class MU_UpdateMetaData : public ModelUndo
{
public:

	enum{ resume=false };

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	void updateMetaData(const std::string &key,
			const std::string &newValue, const std::string &oldValue);

	MU_UpdateMetaData(const std::string &key,
			const std::string &newValue, const std::string &oldValue);

private:
	std::string m_key;
	std::string m_newValue;
	std::string m_oldValue;
};

class MU_ClearMetaData : public ModelUndo
{
public:

	enum{ resume=false };

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

	MU_ClearMetaData(const Model::MetaDataList &list);

private:
	
	Model::MetaDataList m_list;
};

//TODO: Can replace with MU_SwapStableMem?
//NOTE: This became too complicated for keyframes so 
//it's just an optimization for vertex animation data
class MU_InterpolateSelected : public ModelUndo 
{
public:

	enum{ resume=false };

	MU_InterpolateSelected
	(Model::Interpolate2020E e, unsigned anim, unsigned frame)
	:m_e(e),m_anim(anim),m_frame(frame){}

	void undo(Model *m){ _do(m,false); }
	void redo(Model *m){ _do(m,true); }
	int combine(Undo*){ return false; }

	unsigned size();

	void addVertex(unsigned v, Model::Interpolate2020E e)
	{
		m_eold.push_back({v,e});
	}

private:

	struct InterpT
	{
		unsigned v;
		Model::Interpolate2020E e;
	};

	Model::Interpolate2020E m_e;
	unsigned m_anim;
	unsigned m_frame;
	std::vector<InterpT> m_eold;

	//NOTE: Had synchronized animation (AnimRP needed?)
	void _do(Model*,bool);
};

//TODO: Can replace with MU_SwapStableMem?
class MU_RemapTrianglesIndices : public ModelUndo 
{
public:

	void undo(Model*);
	void redo(Model*);
	int combine(Undo*){ return false; }

	unsigned size();

	int_list map;

	void swap();
};

class MU_SwapStableStr : public ModelUndo
{
public:

	void undo(Model*);
	void redo(Model*);
	int combine(Undo*);

	unsigned size();

	MU_SwapStableStr(unsigned cb, std::string &addr)
	:m_changeBits(cb),m_addr(&addr),m_swap(addr){}

	bool resume2(unsigned cb, std::string &addr)
	{
		if(&addr!=m_addr) return false;

		assert(cb==m_changeBits); 

		m_swap = addr; return true;
	}

private:

	unsigned m_changeBits;

	std::string	*m_addr,m_swap;
};

/*WARNING
* 
* This is a general purpose memory reassignment
* solution. It's powerful but it's pretty much 
* opaque to mistakes if it must be audited, so
* use it with care.
* 
* NOTE: Technically this could swap memory but
* it's questionable if it would do well or not.
*/
class MU_SwapStableMem : public ModelUndo
{
public:

	void undo(Model*);
	void redo(Model*);
	int combine(Undo*);

	unsigned size();

	virtual bool nonEdit(){ return !m_edit; }

	void setEdit(bool edit){ m_edit = edit; }

	//WARNING: If merge==false addresses must all be unique
	//because undo() isn't able to iterate in reverse order.
	MU_SwapStableMem(bool merge=true)
	:m_merge(merge),m_edit(true),m_changeBits(){}

	bool resume2(bool merge=true){ return merge==m_merge; }

	void addChange(unsigned c){ m_changeBits|=c; }
	
	template<class T>
	void addMemory(T *addr, const T *old, const T *cp)
	{
		addMemory(addr,old,cp,sizeof(T));
	}
	void addMemory(void*,const void*,const void*,size_t);
	
	template<class T>
	void setMemory(unsigned cb, const T *old, const T *cp)
	{
		addChange(cb); addMemory(const_cast<T*>(old),old,cp,sizeof(T));
	}
	void setMemory(unsigned cb, const void *old, const void *cp, size_t sz)
	{
		addChange(cb); addMemory(const_cast<void*>(old),old,cp,sz);
	}

private:

	struct rbase
	{
		void *addr;
		unsigned short rsz,dsz;
	};
	struct rec : rbase
	{
		//I tried to write a generalized swap routine
		//but found it likely to not perform well, so
		//I'm just stashing 2 copies in m_mem for now.
		//void swap();

		unsigned char mem[1];
	};

	bool m_merge, m_edit;

	unsigned m_changeBits;

	std::vector<char> m_mem;

	template<class F>
	void _for_each(const F&);
};

class MU_MoveUtility : public ModelUndo
{
public:

	enum{ resume=false };

	MU_MoveUtility(unsigned oldIndex, unsigned newIndex)
	:m_oldIndex(oldIndex),m_newIndex(newIndex)
	{}

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	unsigned size();

private:

	unsigned m_oldIndex,m_newIndex;
};

class MU_AssocUtility : public ModelUndo
{
public:
	
	MU_AssocUtility(Model::Utility *up)
	:m_util(up){}

	bool resume2(Model::Utility *up)
	{
		return m_util==up;
	}

	void undo(Model*);
	void redo(Model*);
	int combine(Undo*);

	unsigned size();

	void addAssoc(Model::Group *g);
	void removeAssoc(Model::Group *g);

private:

	struct rec
	{
		int m_op; void *ptr;
	};
	std::vector<rec> m_list;

	Model::Utility *m_util;
};

/* 2022
 * I wrote this class when adding Utilities to consolidate
 * several Undo objects in order to begin to get this file
 * down to size and not contribute to its growth.
*/
class MU_Add : public ModelUndo
{
public:

	enum{ resume_first_only=true };

	void undo(Model *);
	void redo(Model *);
	int combine(Undo *);

	void undoRelease();
	void redoRelease();

	unsigned size();

	template<class T>
	MU_Add(unsigned index, T *ptr)
	{
		m_op = _op(ptr); m_list.push_back({index,ptr});
	}
	MU_Add(Model::ComparePartsE e, bool how=true)
	{
		m_op = how?e:-e;
	}

	template<class T> void add(unsigned index, T *ptr)
	{
		assert(m_op==_op(ptr)); m_list.push_back({index,ptr});
	}

	template<class T>
	bool resume2(unsigned index, T *ptr)
	{
		if(m_op!=_op(ptr)) return false;
		
		m_list.push_back({index,ptr}); return true;
	}
	bool resume2(Model::ComparePartsE e)
	{
		return m_op==e;
	}

protected:

	struct rec
	{
		unsigned index; void *ptr;
	};
	std::vector<rec> m_list; int m_op;
	
	int _op(Model::Vertex*){ return Model::PartVertices; }
	int _op(Model::Triangle*){ return Model::PartFaces; }
	int _op(Model::Group*){ return Model::PartGroups; }
	int _op(Model::Material*){ return Model::PartMaterials; }
	int _op(Model::Joint*){ return Model::PartJoints; }
	int _op(Model::Point*){ return Model::PartPoints; }
	int _op(Model::TextureProjection*){ return Model::PartProjections; }
	int _op(Model::Utility*){ return Model::PartUtilities; }

	void _release();
	bool _fg(void(Model::*&f)(int,void*), void(Model::*&g)(int));
};
class MU_Delete : public MU_Add
{
public:

	template<class T>
	MU_Delete(unsigned index, T *ptr):MU_Add(index,ptr)
	{
		m_op = -m_op;
	}
	MU_Delete(Model::ComparePartsE e):MU_Add(e,false){}

	template<class T> void add(unsigned index, T *ptr)
	{
		assert(m_op==-_op(ptr)); m_list.push_back({index,ptr});
	}

	template<class T>
	bool resume2(unsigned index, T *ptr)
	{
		if(m_op!=-_op(ptr)) return false;
		
		m_list.push_back({index,ptr}); return true;
	}
	bool resume2(Model::ComparePartsE e)
	{
		return m_op==-e;
	}
};

class MU_UvAnimKey : public ModelUndo
{
public:

	void undo(Model*);
	void redo(Model*);
	int combine(Undo*);

	unsigned size();

	typedef Model::UvAnimation::Key Key;

	void deleteKey(const Key&);
	void insertKey(const Key&);
	void assignKey(const Key&,const Key&);

	MU_UvAnimKey(const Model::UvAnimation *anim):m_anim(anim)
	{}

	bool resume2(const Model::UvAnimation *anim)
	{
		return m_anim==anim;
	}

private:

	struct rec
	{
		int op; Key key;
	};
	std::vector<rec> m_ops;

	const Model::UvAnimation *m_anim;

	void _delete_key(Key&);
};

#endif //  __MODELUNDO_H
