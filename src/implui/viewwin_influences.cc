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

#include "viewwin.h"

#include "viewpanel.h"
#include "model.h"
#include "modelstatus.h"

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
	if(!model.nselection[Model::PT_Joint])
	return model_status(model,StatusError,STATUSTIME_LONG,
	::tr("You must have at least one bone joint selected."));

	//log_debug("assigning %d vertices and %d points to joints\n",vertList.size(),pointList.size());
	//QString str = ::tr("Assigning %1 vertices and %2 points to joints").arg(vertList.size()).arg(pointList.size());
	//model_status(model,StatusNormal,STATUSTIME_SHORT,"%s",(const char *)str);
	model_status(model,StatusNormal,STATUSTIME_SHORT,
	::tr("Assigning %d vertices and %d points to joints"),
	model.nselection[Model::PT_Vertex],model.nselection[Model::PT_Point]);

	for(auto j:model.selection) 
	{
		if(!j.type) break; //OPTIMIZING
		if(j.type==Model::PT_Joint)
		for(auto i:model.selection) 
		{
			extern int viewwin_joints100; if(!viewwin_joints100)
			{
				double w = model->calculatePositionInfluenceWeight(i,j);
				model->setPositionInfluence(i,j,Model::IT_Auto,w);
			}
			else model->setPositionInfluence(i,j,Model::IT_Custom,1);
		}
	}

	model->operationComplete(::tr("Assign Selected to Joint"));

	model_status(model,StatusNormal,STATUSTIME_SHORT,
	::tr("Assigned selected to selected bone joints")); //2022
}

static void viewwin_influences_jointAutoAssignSelected(MainWin &model)
{
	if(!model.nselection[Model::PT_Point]
	&&!model.nselection[Model::PT_Vertex])
	return model_status(model,StatusError,STATUSTIME_LONG,
	::tr("You must have at least one vertex or point selected."));
	
	int selected = 0!=model.nselection[Model::PT_Joint];

	double sensitivity = 50; 
	if(id_ok==AutoAssignJointWin(&selected,&sensitivity).return_on_close())
	{
		sensitivity/=100;			

		//log_debug("auto-assigning %p vertices and points to joints\n",model.selection.size());

		for(auto&i:model.selection)
		model->autoSetPositionInfluences(i,sensitivity,1&selected);

		model->operationComplete(::tr("Auto-Assign Selected to Bone Joints"));
	}
}

static void viewwin_influences_jointRemoveInfluencesFromSelected(MainWin &model)
{
	for(auto&i:model.selection) model->removeAllPositionInfluences(i);

	model->operationComplete(::tr("Removed All Influences from Selected"));

	model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
	::tr("Removed all influences from selected vertices and points"));
}

static void viewwin_influences_jointRemoveInfluenceJoint(MainWin &model)
{
	if(model.nselection[Model::PT_Point]
	||model.nselection[Model::PT_Vertex]) //2020
	{
		for(auto j:model.selection) 
		{
			if(!j.type) break; //OPTIMIZING
			if(j.type==Model::PT_Joint)
			for(auto i:model.selection) model->removePositionInfluence(i,j);
		}

		model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
		::tr("Removed influence of bone joint for selected vertices and points"));
	}
	else //OLD WAY
	{
		unsigned vcount = model->getVertexCount();
		unsigned pcount = model->getPointCount();

		for(auto j:model.selection) 
		{
			if(!j.type) break; //OPTIMIZING
			if(j.type==Model::PT_Joint)
			for(unsigned v=vcount;v-->0;) model->removeVertexInfluence(v,j);
			if(j.type==Model::PT_Joint)
			for(unsigned p=pcount;p-->0;) model->removePointInfluence(p,j);
		}

		model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
		::tr("Removed all influence of bone joint"));
	}

	model->operationComplete(::tr("Removed Joint from Influencing"));
}

static void viewwin_influences_jointMakeSingleInfluence(MainWin &model)
{
	int bcount = model->getBoneJointCount();

	if(model.nselection[Model::PT_Point]
	||model.nselection[Model::PT_Vertex]) //2020
	{
		for(auto i:model.selection) 
		{	
			int joint = model->getPrimaryPositionInfluence(i);

			if(joint>=0) for(int b=bcount;b-->0;)
			{
				if(b!=joint) model->removePositionInfluence(i,b);
			}
		}

		model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
		::tr("Converted selected vertices and points to single influence model (100)"));
	}
	else //OLD WAY (OPERATE OVER ENTIRE MODEL?)	
	{
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

		model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
		::tr("Converted all vertices and points to single influence model (100)"));
	}

	model->operationComplete(::tr("Convert To Single Influence"));
}

static void viewwin_influences_jointSelectInfluenceJoints(MainWin &model)
{
	//model->beginSelectionDifference(); //OVERKILL! //OVERKILL
	{
		for(auto&i:model.selection)
		if(auto*infl=model->getPositionInfluences(i))
		{
			//model->getPositionInfluences(*it,ilist);
			for(auto&ea:*infl)
			model->selectBoneJoint(ea.m_boneId);
		}
	}
	//model->endSelectionDifference();
	model->operationComplete(::tr("Select Joint Influences"));

	model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
	::tr("Selected influencing joints"));
}

static void viewwin_influences_jointSelectInfluencedVertices(MainWin &model)
{
	model->beginSelectionDifference(); //OVERKILL! //OVERKILL?
	{
		for(auto&j:model.selection) 
		{
			if(!j.type) break; //OPTIMIZING
			if(j.type==Model::PT_Joint)
			for(unsigned v=model->getVertexCount();v-->0;)
			{
				//model->getVertexInfluences(v,ilist);
				for(auto&ea:model->getVertexInfluences(v))
				{
					if(j==ea.m_boneId) model->selectVertex(v);
				}
			}
		}
	}
	model->endSelectionDifference();
	model->operationComplete(::tr("Select Influences Vertices"));

	model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
	::tr("Selected influenced vertices"));
}
static void viewwin_influences_jointSelectInfluencedPoints(MainWin &model)
{
	//2020: Assuming a small point count
	//model->beginSelectionDifference(); //OVERKILL!
	{
		for(auto&j:model.selection) 
		{
			if(!j.type) break; //OPTIMIZING
			if(j.type==Model::PT_Joint)
			for(unsigned p=model->getPointCount();p-->0;)
			{
				//model->getPointInfluences(p,ilist);
				for(auto&ea:model->getPointInfluences(p))
				{
					if(j==ea.m_boneId) model->selectPoint(p);
				}
			}
		}
	}
	//model->endSelectionDifference();
	model->operationComplete(::tr("Select Influenced Points"));

	model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
	::tr("Selected influenced points"));
}

static void viewwin_influences_jointSelectUnassignedVertices(MainWin &model)
{
	model->beginSelectionDifference(); //OVERKILL!
	model->unselectAllVertices();
	{
		for(unsigned v=model->getVertexCount();v-->0;)
		if(model->getVertexInfluences(v).empty())
		{
			model->selectVertex(v);
		}
	}
	model->endSelectionDifference();
	model->operationComplete(::tr("Select Unassigned Vertices"));

	model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
	::tr("Selected all vertices not assigned to bone joints"));
}
static void viewwin_influences_jointSelectUnassignedPoints(MainWin &model)
{
	model->unselectAllPoints();
	//2020: Assuming a small point count
	//model->beginSelectionDifference(); //OVERKILL! 
	{
		for(unsigned p=model->getPointCount();p-->0;)
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
	//model->endSelectionDifference();
	model->operationComplete(::tr("Select Unassigned Points"));

	model_status(model,StatusNormal,STATUSTIME_SHORT, //2022
	::tr("Selected all points not assigned to bone joints"));
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
		
		if(!model.nselection[Model::PT_Joint]) //Reuse hotkey?
		goto free_verts;
		viewwin_influences_jointSelectInfluencedVertices(model);
		break;

	case id_joint_select_points_of: 
		
		if(!model.nselection[Model::PT_Joint]) //Reuse hotkey?
		goto free_points;
		viewwin_influences_jointSelectInfluencedPoints(model);
		break;

	case id_joint_unnassigned_verts: free_verts:
		
		viewwin_influences_jointSelectUnassignedVertices(model);
		break;

	case id_joint_unnassigned_points: free_points:
		
		viewwin_influences_jointSelectUnassignedPoints(model);
		break;
	}
}

