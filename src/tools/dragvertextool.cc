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

#include "tool.h"

#include "pixmap/dragvertextool.xpm"

#include "glmath.h"
#include "model.h"
#include "modelstatus.h"
#include "log.h"

struct DragVertexTool: public Tool
{
	DragVertexTool():Tool(TT_Other)
	{
		m_moveTexCoords = true; //config defaults
		m_proximity = 0;
	}

	virtual const char *getName(int)
	{
		return TRANSLATE_NOOP("Tool","Drag Vertex on Edge");
	}

	virtual const char *getKeymap(int){ return "Shift+T"; } //"D"

	virtual const char **getPixmap(int){ return dragvertextool_xpm; }

	virtual void activated(int)
	{
		parent->addBool(true,&m_moveTexCoords,
		TRANSLATE_NOOP("Param","Move Texture"));
		parent->addDouble(true,&m_proximity,
		TRANSLATE_NOOP("Param","Proximity"),0,8192*4);
		updateParam(&m_moveTexCoords);
	}
	virtual void updateParam(void *p)
	{
		if(p==&m_moveTexCoords)
		parent->hideParam(&m_proximity,!m_moveTexCoords);
	}

	virtual void mouseButtonDown();
	virtual void mouseButtonMove();
	virtual void mouseButtonUp();

		int m_vertId;
		Vector m_coords;

		bool m_selecting,m_left;

		struct Tri
		{
			int tri,src; 
			float st[2],dst[2];
			int tol[2];
			Tri():tri(-1){}
		};
		struct Vec //2022
		{
			Vector v; 
			double v2[2],mag2;

			int vrt;
			Tri tris[2];

			Vec(Vector &v):v(v){}
		};
		std::vector<Vec> m_vectors; //Vector

		std::vector<ToolCoordT> m_multi; //2022
				
		bool m_moveTexCoords;
		double m_proximity;
};

extern Tool *dragvertextool(){ return new DragVertexTool; }

void DragVertexTool::mouseButtonDown()
{
	m_vertId = -1;

	m_left = //2021: Reserving BS_Right
	m_selecting = BS_Left&parent->getButtons();
	if(!m_left) 
	return model_status(parent->getModel(),StatusError,STATUSTIME_LONG,
	TRANSLATE("Tool","No function"));

	Model *model = parent->getModel();
	
	int multi = 0;
	auto &vl = model->getVertexList();
	int vid = -1;
	unsigned ord = 0;
	for(size_t i=0,n=vl.size();i<n;i++)	
	if(auto cmp=vl[i]->m_selected._select_op)
	{
		multi++; if(cmp>ord)
		{
			ord = cmp; vid = (int)i;
		}
	}
	if(multi>1) //TODO: snap/closest?
	{
		//TODO: Scan for other Position types?
	}	

	if(vid==-1) return; //Wait for m_selecting?
	
	m_vectors.clear(); m_multi.clear();
	
	// model to viewport space
	const Matrix &mat = parent->getParentBestMatrix();
	m_coords.setAll(vl[vid]->m_absSource);
	if(multi>1)
	{
		m_multi.push_back({{Model::PT_Vertex,(unsigned)vid}}); 
		m_coords.getVector3(m_multi.back().coords);
	}
	mat.apply3x(m_coords); m_coords[3] = 0;

	auto &tl = model->getTriangleList();
	int iN = (int)tl.size();
	for(int a,b,i=0;i<iN;i++)
	{
		auto *tp = tl[i];
		auto *tv = tp->m_vertexIndices;

		for(a=3;a-->0;) if(tv[a]==vid) 
		break;
		if(a==-1) continue;
			
		for(b=3;b-->0;) if(tv[b]!=vid)
		{
			int vrt = tv[b];
			Vec *vp = nullptr;
			for(auto&ea:m_vectors) 
			if(ea.vrt==vrt)
			{
				vp = &ea; break;
			}
			if(!vp)
			{
				Vector v(vl[vrt]->m_absSource);
				mat.apply3x(v);
				v-=m_coords;
			
				m_vectors.push_back(v);
				vp = &m_vectors.back();

				vp->v2[0] = v[0]; 
				vp->v2[1] = v[1];
				vp->mag2 = 1/normalize2(vp->v2);
				vp->vrt = vrt;
			}
			int k = vp->tris[0].tri==-1?0:1;
			auto &tri = vp->tris[k];
			tri.tri = i;
			tri.src = a;
			tri.st[0] = tp->m_s[a];
			tri.st[1] = tp->m_t[a];
			tri.dst[0] = tp->m_s[b];
			tri.dst[1] = tp->m_t[b];
			int tid = model->getGroupTextureId(tp->m_group);			
			auto *tex = model->getTextureData(tid);
			tri.tol[0] = tex?tex->m_origWidth:1;
			tri.tol[1] = tex?tex->m_origHeight:1;
		}
	}

	if(multi>1)
	for(int i=0,n=(int)vl.size();i<n;i++)
	if(vl[i]->m_selected&&i!=vid)
	{
		m_multi.push_back({{Model::PT_Vertex,(unsigned)i}}); 
		auto &v = *(Vector*)vl[i]->m_absSource;
		v.getVector3(m_multi.back().coords);
	}
}
void DragVertexTool::mouseButtonMove()
{
	Model *model = parent->getModel();

	if(m_selecting)
	{
		m_selecting = false;		

		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Dragging selected vertex"));
	}
	//2021: Reserving BS_Right
	if(!m_left) return;

	double newPos[2];
	parent->getParentXYValue(newPos[0],newPos[1]);

	//log_debug("pos is (%f,%f,%f)\n",newPos[0],newPos[1],newPos[2]); //???

	newPos[0]-=m_coords[0];
	newPos[1]-=m_coords[1];

	//log_debug("pos vector is (%f,%f,%f)\n",newPos[0],newPos[1],newPos[2]); //???

	double pscale = normalize2(newPos);

	//log_debug("mouse has moved %f units\n",pscale); //???

	if(!m_vectors.empty())
	{
		double bestDp = 0; Vec *vp = 0;

		for(auto&ea:m_vectors)
		{
			double dp = dot2(ea.v2,newPos);

			//log_debug("  dot3 is %f (%f,%f,%f)\n",d,vtry[0],vtry[1],vtry[2]); //???

			if(fabs(dp)>fabs(bestDp))
			{
				vp = &ea; bestDp = dp;				
			}
		}
		if(!vp) //0 bestDp reduces UVs to 0,0.
		{
			vp = m_vectors.data(); //bestDp = 1;
		}
		if(bestDp==0) bestDp = 1; //Still happens? //SEEMS TO HELP? //EXPERIMENTAL

		Vector best = vp->v;

		//log_debug("best vector is (%f,%f,%f)\n",best[0],best[1],best[2]); //???

		double ratio = bestDp*pscale*vp->mag2;
		
		best.scale3(ratio);

		//log_debug("best scaled is (%f,%f,%f)\n",best[0],best[1],best[2]); //???

		best[0]+=m_coords[0];
		best[1]+=m_coords[1];
		best[2]+=m_coords[2];

		//log_debug("best sum is (%f,%f,%f)\n",best[0],best[1],best[2]); //???

		parent->getParentBestInverseMatrix().apply3x(best);

		//log_debug("best applied is (%f,%f,%f)\n",best[0],best[1],best[2]); //???

		int vid = m_vertId;
		model->moveVertex(vid,best[0],best[1],best[2]);

		//If moving multiple vertices the Uvs may be immobile.
		bool moveTCs = m_moveTexCoords;
		if(moveTCs&&!m_multi.empty())
		{
			//TODO: It'd be nice to adjust UVs for coinciding
			//vertices, or even connected but not immobilized.
			//This would be more practical if the undo system
			//used pointers down the road.
			auto cmp = (unsigned)vp->vrt;
			auto it = m_multi.begin(), itt = m_multi.end();
			for(it++;it<itt;it++) if(cmp==it->pos.index)
			{
				moveTCs = false; break;
			}
		}
		if(moveTCs) //2022
		{
			//I guess this is in pixels since I don't know what else
			//it could be that's square. So no texture is 1x1 pixels.
			double tol = m_proximity+0.00001;
			tol*=tol; //sqrt

			Tri *cmp1 = nullptr, *cmp2 = nullptr;

			double ds1,dt1,ds2,dt2,s1,t1,s2,t2;

			for(auto&ea:vp->tris) if(ea.tri!=-1)
			{
				//TODO: I reckon there's a way to arrive at different
				//paths through UV space for each mobile edge, however
				//this just reuses the same delta (or rather two since
				//an edge can have two sets of UVs for both triangles.)
				//
				// NOTE: I'm not convinced this is a bad strategy, so
				// it could be two models are offered after I can sit
				// down to figure out the other model.
				//
				(cmp1?ds2:ds1) = (ea.dst[0]-ea.st[0])*ratio;
				(cmp1?dt2:dt1) = (ea.dst[1]-ea.st[1])*ratio;
				(cmp1?cmp2:cmp1) = &ea;
			}
			//Must translate proximity tolerance into a flat UV space.
			if(cmp1) s1 = (double)cmp1->st[0]*cmp1->tol[0];
			if(cmp1) t1 = (double)cmp1->st[1]*cmp1->tol[1];
			if(cmp2) s2 = (double)cmp2->st[0]*cmp2->tol[0];
			if(cmp2) t2 = (double)cmp2->st[1]*cmp2->tol[1];

			for(auto&va:m_vectors) 
			for(auto&ea:va.tris) if(ea.tri!=-1)
			{
				double s = ea.st[0], t = ea.st[1];
								
				Tri *cmp = 0;
				if(cmp1==&ea) cmp = cmp1; else
				if(cmp2==&ea) cmp = cmp2; else
				{
					double ds = s1-s;
					double dt = t1-t;
					double d1 = ds*ds+dt*dt,d2;					
					if(d1<=tol) cmp = cmp1; if(cmp2)
					{
						ds = s2-s;
						dt = t2-t; 					
						d2 = ds*ds+dt*dt;
						if(d2<d1&&d2<=tol) cmp = cmp2;
					}
				}

				if(cmp2)
				if(cmp2==cmp){ s+=ds2; t+=dt2; }
				if(cmp1==cmp){ s+=ds1; t+=dt1; }
				model->setTextureCoords(ea.tri,ea.src,(float)s,(float)t);
			}
		}

		if(!m_multi.empty()) //2022
		{
			auto it = m_multi.begin(), itt = m_multi.end();

			best-=it->v(); for(it++;it<itt;it++)
			{
				Vector mv = it->v()+best;
				model->movePosition(it->pos,mv[0],mv[1],mv[2]);
			}
		}
	}

	parent->updateAllViews();
}
void DragVertexTool::mouseButtonUp()
{
	Model *model = parent->getModel();

	if(m_selecting)
	{
		parent->snapSelect(); //EXPERIMENTAL		

		parent->updateAllViews(); //Needed

		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Select complete"));
	}
	else if(m_left) if(-1==m_vertId)
	{
		 model_status(model,StatusError,STATUSTIME_LONG,
		TRANSLATE("Tool","Select 1 vertex"));
	}
	else
	{
		model_status(model,StatusNormal,STATUSTIME_SHORT,
		TRANSLATE("Tool","Drag complete"));
	}
}
