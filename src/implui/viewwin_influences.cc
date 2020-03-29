/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2004-2007 Kevin Worcester
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not,write to the Free Software
 * Foundation,Inc.,59 Temple Place-Suite 330,Boston,MA 02111-1307,
 * USA.
 *
 * See the COPYING file for full license text.
 */

#include "mm3dtypes.h" //PCH

#include "viewwin.h"

#include "viewpanel.h"
#include "model.h"
#include "modelstatus.h"
//#include "jointwin.h"
//#include "autoassignjointwin.h"
#include "log.h"

struct AutoAssignJointWin : Win
{
	AutoAssignJointWin(int *lv, double *lv2)
		:
	Win("Auto-Assign Bone Joints"),
	selected(main,"Only assign to selected joints",lv),
		sep1(main),
	nav(main),
	single(nav,"Single"),
	sensitivity(nav,"",bar::horizontal,lv2),
	multi(nav,"Multiple"),
		sep2(main),
	f1_ok_cancel(main)
	{
		if(!*lv) selected.disable();

		nav.expand();
		single.space<top>(1); 
		sensitivity.set_range(0,100).expand(); 
		multi.space<top>(1).ralign();
	}

	boolean selected;
	titlebar sep1;
	row nav;	
	titlebar single; 
	bar sensitivity;
	titlebar multi;
	titlebar sep2;
	f1_ok_cancel_panel f1_ok_cancel;
};

static void viewwin_influences_jointAssignSelectedToJoint(MainWin &model)
{
	int_list jointList,vertList,pointList;
	model->getSelectedBoneJoints(jointList);
	if(jointList.empty())
	return model_status(model,StatusError,STATUSTIME_LONG,
	::tr("You must have at least one bone joint selected."));
	model->getSelectedVertices(vertList);
	model->getSelectedPoints(pointList);

	log_debug("assigning %d vertices and %d points to joints\n",vertList.size(),pointList.size());
	//QString str = ::tr("Assigning %1 vertices and %2 points to joints").arg(vertList.size()).arg(pointList.size());
	//model_status(model,StatusNormal,STATUSTIME_SHORT,"%s",(const char *)str);
	utf8 str = ::tr("Assigning %d vertices and %d points to joints");
	model_status(model,StatusNormal,STATUSTIME_SHORT,str,vertList.size(),pointList.size());

	for(auto j:jointList) for(auto i:vertList)
	{
		double w = model->calculateVertexInfluenceWeight(i,j);
		model->addVertexInfluence(i,j,Model::IT_Auto,w);
	}

	for(auto j:jointList) for(auto i:pointList)
	{
		double w = model->calculatePointInfluenceWeight(i,j);
		model->addPointInfluence(i,j,Model::IT_Auto,w);
	}

	model->operationComplete(::tr("Assign Selected to Joint"));
}

static void viewwin_influences_jointAutoAssignSelected(MainWin &model)
{
	if(model.selection.empty())
	{
		return model_status(model,StatusError,STATUSTIME_LONG,
		::tr("You must have at least one vertex or point selected."));
	}	 
	int selected = 0!=model->getSelectedBoneJointCount();		
	double sensitivity = 50; 
	if(id_ok==AutoAssignJointWin(&selected,&sensitivity).return_on_close())
	{
		sensitivity/=100;			

		log_debug("auto-assigning %p vertices and points to joints\n",model.selection.size());

		for(auto&i:model.selection)
		model->autoSetPositionInfluences(i,sensitivity,1&selected);

		model->operationComplete(::tr("Auto-Assign Selected to Bone Joints"));
	}
}

static void viewwin_influences_jointRemoveInfluencesFromSelected(MainWin &model)
{
	for(auto&i:model.selection) model->removeAllPositionInfluences(i);

	model->operationComplete(::tr("Remove All Influences from Selected"));
}

static void viewwin_influences_jointRemoveInfluenceJoint(MainWin &model)
{
	unsigned vcount = model->getVertexCount();
	unsigned pcount = model->getPointCount();

	int_list jointList;
	model->getSelectedBoneJoints(jointList);
	for(auto i:jointList)
	{
		for(unsigned v=vcount;v-->0;) model->removeVertexInfluence(v,i);
		for(unsigned p=pcount;p-->0;) model->removePointInfluence(p,i);
	}

	model->operationComplete(::tr("Remove Joint from Influencing"));
}

static void viewwin_influences_jointMakeSingleInfluence(MainWin &model)
{
	//FIX ME
	//Should be selection based.

	int bcount = model->getBoneJointCount();

	for(unsigned v=model->getVertexCount();v-->0;)
	{
		int joint = model->getPrimaryVertexInfluence(v);
		
		if(joint>=0) for(int b=bcount;b-->0;)
		{
			if(b!=joint) model->removeVertexInfluence(v,b);
		}
	}
	for(unsigned p=model->getPointCount();p-->0;)
	{
		int joint = model->getPrimaryPointInfluence(p);
		
		if(joint>=0) for(int b=bcount;b-->0;)
		{
			if(b!=joint) model->removePointInfluence(p,b);
		}
	}

	model->operationComplete(::tr("Convert To Single Influence"));
}

static void viewwin_influences_jointSelectUnassignedVertices(MainWin &model)
{
	model->unselectAllVertices();
	model->beginSelectionDifference();
	{
		for(unsigned v=0,vN=model->getVertexCount();v<vN;v++)
		{
			//infl_list l;
			//model->getVertexInfluences(v,l);
			//if(l.empty())
			if(model->getVertexInfluences(v).empty())
			{
				model->selectVertex(v);
			}
		}
	}
	model->endSelectionDifference();
	model->operationComplete(::tr("Select Unassigned Vertices"));
}

static void viewwin_influences_jointSelectUnassignedPoints(MainWin &model)
{
	model->unselectAllVertices();
	model->beginSelectionDifference();
	{
		for(unsigned p=0,pN=model->getPointCount();p<pN;p++)
		{
			//infl_list l;
			//model->getPointInfluences(p,l);
			//if(l.empty())
			if(model->getPointInfluences(p).empty())
			{
				model->selectPoint(p);
			}
		}
	}
	model->endSelectionDifference();
	model->operationComplete(::tr("Select Unassigned Points"));
}

static void viewwin_influences_jointSelectInfluenceJoints(MainWin &model)
{
	model->beginSelectionDifference();
	{
		for(auto&i:model.selection)
		if(auto*infl=model->getPositionInfluences(i))
		{
			//model->getPositionInfluences(*it,ilist);
			for(auto&ea:*infl)
			model->selectBoneJoint(ea.m_boneId);
		}
	}
	model->endSelectionDifference();
	model->operationComplete(::tr("Select Joint Influences"));
}

static void viewwin_influences_jointSelectInfluencedVertices(MainWin &model)
{
	model->beginSelectionDifference();
	{
		int_list jointList;
		model->getSelectedBoneJoints(jointList);
		for(auto i:jointList)
		for(unsigned v=0,vN=model->getVertexCount();v<vN;v++)
		{
			//model->getVertexInfluences(v,ilist);
			for(auto&ea:model->getVertexInfluences(v))
			{
				if(i==ea.m_boneId) model->selectVertex(v);
			}
		}
	}
	model->endSelectionDifference();
	model->operationComplete(::tr("Select Influences Vertices"));
}

static void viewwin_influences_jointSelectInfluencedPoints(MainWin &model)
{
	model->beginSelectionDifference();
	{
		int_list jointList;
		model->getSelectedBoneJoints(jointList);
		for(auto i:jointList)
		for(unsigned p=0,pN=model->getPointCount();p<pN;p++)
		{
			//model->getPointInfluences(p,ilist);
			for(auto&ea:model->getPointInfluences(p))
			{
				if(i==ea.m_boneId) model->selectPoint(p);
			}
		}
	}
	model->endSelectionDifference();
	model->operationComplete(::tr("Select Influenced Points"));
}

extern void viewwin_influences(MainWin &model, int id)
{
	switch(id)
	{
	case id_joint_attach_verts: 
		
		viewwin_influences_jointAssignSelectedToJoint(model);
		break;

	case id_joint_weight_verts: 
		
		viewwin_influences_jointAutoAssignSelected(model);
		break;

	case id_joint_remove_bones:

		viewwin_influences_jointRemoveInfluencesFromSelected(model);
		break;

	case id_joint_remove_selection: 
		
		viewwin_influences_jointRemoveInfluenceJoint(model);
		break;

	case id_joint_simplify: 
		
		viewwin_influences_jointMakeSingleInfluence(model);
		break;

	case id_joint_select_bones_of:
		
		viewwin_influences_jointSelectInfluenceJoints(model);
		break;

	case id_joint_select_verts_of: 
		
		viewwin_influences_jointSelectInfluencedVertices(model);
		break;

	case id_joint_select_points_of: 
		
		viewwin_influences_jointSelectInfluencedPoints(model);
		break;

	case id_joint_unnassigned_verts:
		
		viewwin_influences_jointSelectUnassignedVertices(model);
		break;

	case id_joint_unnassigned_points:
		
		viewwin_influences_jointSelectUnassignedPoints(model);
		break;
	}
}

