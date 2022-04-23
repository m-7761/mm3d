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
#include "glmath.h"
#include "log.h"
#include "msg.h"
#include "texmgr.h"
#include "texture.h"
#include "translate.h"

#include "modelstatus.h"
#include "modelundo.h"

// FIXME centralize this
const double EQ_TOLERANCE = 0.00001;

// FIXME centralize this
static bool matrixEquiv(const Matrix &lhs, const Matrix &rhs, double tolerance = EQ_TOLERANCE)
{
	Vector lright(2,0,0);
	Vector lup(0,2,0);
	Vector lfront(0,0,2);

	Vector rright(2,0,0);
	Vector rup(0,2,0);
	Vector rfront(0,0,2);

	lhs.apply4(lright);
	lhs.apply4(lup);
	lhs.apply4(lfront);

	rhs.apply4(rright);
	rhs.apply4(rup);
	rhs.apply4(rfront);

	if((lright-rright).mag3()>tolerance)
		return false;
	if((lup-rup).mag3()>tolerance)
		return false;
	if((lfront-rfront).mag3()>tolerance)
		return false;

	return true;
}

static bool jointsMatch(const Model *lhs, int lb, const Model *rhs, int rb)
{
	// Check for root joints
	if(lb<0&&rb<0)
		return true;  // both are root joints,match
	else if(lb<0)
		return false;  // only left is root,no match
	else if(rb<0)
		return false;  // only right is root,no match

	Matrix lMat;
	Matrix rMat;

	lhs->getBoneJointFinalMatrix(lb,lMat);
	rhs->getBoneJointFinalMatrix(rb,rMat);

	if(matrixEquiv(lMat,rMat,0.01))
	{
		// It's only a match if the parents match
		return jointsMatch(
				lhs,lhs->getBoneJointParent(lb),
				rhs,rhs->getBoneJointParent(rb));
	}

	return false;
}

typedef std::unordered_map<int,int> IntMap;

static bool influencesMatch(const infl_list &lhs,
		const infl_list &rhs, const IntMap jointMap)
{
	typedef std::unordered_map<int, double> WeightMap;
	WeightMap lhsInfMap;
	WeightMap rhsInfMap;

	infl_list::const_iterator it;

	double totalWeight = 0.0;
	for(it = lhs.begin(); it!=lhs.end(); ++it)
	{
		totalWeight += it->m_weight;
	}
	for(it = lhs.begin(); it!=lhs.end(); ++it)
	{
		if(it->m_weight>0.0)
			lhsInfMap[it->m_boneId] = it->m_weight/totalWeight;
	}

	totalWeight = 0.0;
	for(it = rhs.begin(); it!=rhs.end(); ++it)
	{
		totalWeight += it->m_weight;
	}
	for(it = rhs.begin(); it!=rhs.end(); ++it)
	{
		if(it->m_weight>0.0)
			rhsInfMap[it->m_boneId] = it->m_weight/totalWeight;
	}

	for(it = lhs.begin(); it!=lhs.end(); ++it)
	{
		IntMap::const_iterator boneIt = jointMap.find(it->m_boneId);
		if(boneIt==jointMap.end())
			return false;

		double lw = 0;
		WeightMap::const_iterator lwit = lhsInfMap.find(it->m_boneId);
		if(lwit!=lhsInfMap.end())
		{
			lw = lwit->second;
			lhsInfMap.erase(lwit->first);
		}

		double rw = 0;
		WeightMap::const_iterator rwit = rhsInfMap.find(boneIt->second);
		if(rwit!=rhsInfMap.end())
		{
			rw = rwit->second;
			rhsInfMap.erase(rwit->first);
		}

		if(fabs (rw-lw)>0.02f)
		{
			log_warning("%f %f\n",lw,rw);
			return false;
		}
	}

	WeightMap::const_iterator doomed;
	while(!lhsInfMap.empty())
	{
		doomed = lhsInfMap.begin();
		if(doomed->second>0.02f)
			return false;

		lhsInfMap.erase(doomed->first);
	}
	while(!rhsInfMap.empty())
	{
		doomed = rhsInfMap.begin();
		if(doomed->second>0.02f)
			return false;

		rhsInfMap.erase(doomed->first);
	}

	return true;
}

struct model_ops_TriMatchT
{
	int rtri;
	int indexOffset;
};
typedef std::unordered_map<int,model_ops_TriMatchT> model_ops_TriMatchMap;

bool Model::equivalent(const Model *model, double tolerance)const
{
		//CORRECT???
		//HACK? Need to update m_final matrix.
		//(jointsMatch calls getBoneJointFinalMatrix.)
		validateAnimSkel(); model->validateAnimSkel();

	int lhsTCount	 = (unsigned)m_triangles.size();
	int rhsTCount	 = (unsigned)model->m_triangles.size();
	int lhsGCount	 = (unsigned)m_groups.size();
	int rhsGCount	 = (unsigned)model->m_groups.size();
	int lhsBCount	 = (unsigned)m_joints.size();
	int rhsBCount	 = (unsigned)model->m_joints.size();
	int lhsPCount	 = (unsigned)m_points.size();
	int rhsPCount	 = (unsigned)model->m_points.size();

	if(lhsTCount!=rhsTCount)
	{
		log_warning("lhs triangle count %d does not match rhs %d\n",lhsTCount,rhsTCount);
		return false;
	}

	if(lhsPCount!=rhsPCount)
	{
		log_warning("lhs point count %d does not match rhs %d\n",lhsPCount,rhsPCount);
		return false;
	}

	if(lhsBCount!=rhsBCount)
	{
		log_warning("lhs bone joint count %d does not match rhs %d\n",lhsBCount,rhsBCount);
		return false;
	}

	int v = 0;
	int g = 0;
	int m = 0;
	int b = 0;
	int p = 0;

	// Mapping to a list of ints allows a single group on the lhs to be equal
	// several smaller groups on the rhs.
	typedef std::map<int,int_list> IntListMap;
	typedef std::vector<bool> BoolList;
	typedef int_list IntList;

	// Match skeletons

	BoolList jointMatched;
	jointMatched.resize(rhsBCount,false);

	// Save joint mapping for influence comparision.
	// Key = lhs joint,value = rhs joint
	IntMap jointMap;

	for(b = 0; b<lhsBCount; ++b)
	{
		bool match = false;
		for(int rb=0;!match&&rb<rhsBCount;rb++)		
		if(!jointMatched[rb]&&jointsMatch(this,b,model,rb))
		{
			jointMatched[rb] = true;
			jointMap[b] = rb;
			match = true;
		}

		if(!match)
		{
			std::string dstr;
			m_joints[b]->sprint(dstr);
			log_warning("no match for lhs joint %d:\n%s\n",b,dstr.c_str());
			return false;
		}
	}

	// Find groups that are equivalent.

	IntListMap groupMap;

	for(g = 0; g<lhsGCount; ++g)
	for(int r = 0; r<rhsGCount; ++r)		
	if(m_groups[g]->propEqual(*model->m_groups[r],Model::PropNormals))
	{
		bool match = false;

		m = getGroupTextureId(g);
		int rm = model->getGroupTextureId(r);
		if(m<0&&rm<0)
		{
			match = true;
		}
		if(m>=0&&rm>=0)
		{
			if(m_materials[m]->propEqual(*model->m_materials[rm],
							Model::PropType
						| Model::PropLighting
						| Model::PropClamp
						| Model::PropPixels
						| Model::PropDimensions,
						tolerance))
			{
				match = true;
			}
		}

		if(match)
			groupMap[g].push_back(r);
	}

	model_ops_TriMatchMap triMap;

	BoolList triMatched;
	triMatched.resize(rhsTCount,false);

	IntList gtris;
	IntList rgtris;

	// For each triangle in each group,find a match in a corresponding group.
	for(g = -1; g<lhsGCount; ++g)
	{
		if(g>=0)
		{
			gtris = getGroupTriangles(g);

			rgtris.clear();
			if(!gtris.empty())
			{
				// Add all triangles from all matching groups to rgtris
				IntListMap::const_iterator it = groupMap.find(g);

				if(it!=groupMap.end())
				{
					for(IntList::const_iterator lit = it->second.begin();
							lit!=it->second.end(); ++lit)
					{
						IntList temp = model->getGroupTriangles(*lit);
						rgtris.insert(rgtris.end(),temp.begin(),temp.end());
					}
				}
			}
		}
		else
		{
			getUngroupedTriangles(gtris);
			model->getUngroupedTriangles(rgtris);
		}

		// Don't fail on group sizes.  This causes a failure if the lhs has two
		// equal groups that are combined into one on the rhs.

		if(gtris.size()!=rgtris.size())
		{
			log_warning("lhs group %d triangles = %d,rhs = %d (not fatal)\n",
					g,gtris.size(),rgtris.size());
		}

		bool compareTexCoords = false;

		if(g>=0)
		{
			int m = getGroupTextureId(g);
			// Only compare texture coordinates if the coordinates affect the
			// material at that vertex (texture map,gradient,etc).
			if(m>=0)
			{
				compareTexCoords =
					(getMaterialType(g)!=Model::Material::MATTYPE_BLANK);
			}
		}

		for(IntList::const_iterator it = gtris.begin(); it!=gtris.end(); ++it)
		{
			bool match = false;
			for(IntList::const_iterator rit = rgtris.begin();
					!match&&rit!=rgtris.end(); ++rit)
			{
				if(triMatched[*rit])
					continue;

				for(int i = 0; !match&&i<3; i++)
				{
					match = true;
					int matchOffset = 0;
					for(int j = 0; match&&j<3; ++j)
					{
						double coords[3];
						double rcoords[3];

						v = m_triangles[*it]->m_vertexIndices[j];
						int rv = model->m_triangles[*rit]->m_vertexIndices[(j+i)%3];
						getVertexCoords(v,coords);
						model->getVertexCoords(rv,rcoords);

						if(!floatCompareVector(coords,rcoords,3,tolerance))
						{
							//log_warning("failed match coords for index %d\n",j);
							match = false;
						}

						// Check to see if vertex influences match
						if(match)
						{
							//infl_list ilhs;
							//infl_list irhs;
							//getVertexInfluences(v,ilhs);
							//model->getVertexInfluences(rv,irhs);
							const infl_list &ilhs = getVertexInfluences(v);
							const infl_list &irhs = model->getVertexInfluences(rv);

							match = influencesMatch(ilhs,irhs,jointMap);
							
							if(!match)
							{
								log_warning("  influence match fail\n");
								std::string dest;
								m_vertices[v]->sprint(dest);
								log_warning("	 lhs vertex: %s\n",dest.c_str());
								model->m_vertices[rv]->sprint(dest);
								log_warning("	 rhs vertex: %s\n",dest.c_str());
							}
						}

						if(match&&compareTexCoords)
						{
							float s = 0.0f;
							float t = 0.0f;
							float rs = 0.0f;
							float rt = 0.0f;

							getTextureCoords(*it,j,s,t);
							model->getTextureCoords(*rit,(j+i)%3,rs,rt);

							if(fabs(s-rs)>tolerance
								  ||fabs(t-rt)>tolerance)
							{
								match = false;
								log_warning(" texture coords match fail\n");
							}
						}

						if(match)
						{
							matchOffset = i;
						}
					}

					if(match)
					{
						triMatched[*rit] = true;

						model_ops_TriMatchT tm;
						tm.rtri = *rit;
						tm.indexOffset = matchOffset;
						triMap[*it] = tm;
					}
				}
			}

			if(!match)
			{
				std::string dstr;
				m_triangles[*it]->sprint(dstr);
				log_warning("no match for lhs triangle %d in group %d: %s\n",*it,g,dstr.c_str());
				return false;
			}
		}
	}

	// Find points that are equivalent.

	BoolList pointMatched;
	pointMatched.resize(rhsPCount,false);

	double trans[3] = { 0,0,0 };
	double rot[3] = { 0,0,0 };

	IntMap pointMap;

	for(p = 0; p<lhsPCount; ++p)
	{
		bool match = false;
		for(int rp = 0; !match&&rp<rhsPCount; ++rp)
		{
			if(!pointMatched[rp])
			{
				Matrix lMat,rMat;

				getPointRotationUnanimated(p,rot);
				getPointCoordsUnanimated(p,trans);
				lMat.setRotation(rot);
				lMat.setTranslation(trans);

				model->getPointRotationUnanimated(rp,rot);
				model->getPointCoordsUnanimated(rp,trans);
				rMat.setRotation(rot);
				rMat.setTranslation(trans);

				match = matrixEquiv(lMat,rMat);

				if(match)
				{
					//infl_list ilhs;
					//infl_list irhs;
					//getPointInfluences(p,ilhs);
					//model->getPointInfluences(rp,irhs);
					const infl_list &ilhs = getPointInfluences(p);
					const infl_list &irhs = model->getPointInfluences(rp);

					match = influencesMatch(ilhs,irhs,jointMap);

					if(match)
					{
						pointMatched[rp] = true;
						pointMap[p] = rp;
					}
				}
			}
		}

		if(!match)
		{
			std::string dstr;
			m_points[p]->sprint(dstr);
			log_warning("no match for lhs point %d:\n%s\n",p,dstr.c_str());
			return false;
		}
	}

	// Don't need to check texture contents,already did that in the group
	// and material check above.


	//////
	//
	// TODO? Does it matter to classify by restriction?
	//
	//
	// Compare skeletal animations. This assumes animations are in the
	// same order.
	Model::AnimationModeE mode = Model::ANIMMODE_JOINT;
	unsigned acount = getAnimationCount(mode);
	if(acount!=model->getAnimationCount(mode))
	{
		log_warning("animation count mismatch,lhs = %d,rhs = %d\n",
				acount,model->getAnimationCount(mode));
		return false;
	}	
	// Compare frame animations. This assumes animations are in the
	// same order.
	mode = Model::ANIMMODE_FRAME;
	acount = getAnimationCount(mode);
	if(acount!=model->getAnimationCount(mode))
	{
		log_warning("frame animation count mismatch,lhs = %d,rhs = %d\n",
				acount,model->getAnimationCount(mode));
		return false;
	}
	// Compare frame+skeletal. This assumes animations are in the
	// same order.
	mode = Model::ANIMMODE;
	acount = getAnimationCount(mode);
	if(acount!=model->getAnimationCount(mode))
	{
		log_warning("frame+skeletal animation count mismatch,lhs = %d,rhs = %d\n",
				acount,model->getAnimationCount(mode));
		return false;
	}

	Matrix lhs_mat, rhs_mat;
	acount = getAnimationCount();
	for(unsigned a=0;a<acount;a++)
	{
		if(strcmp(getAnimName(a),model->getAnimName(a)))
		{
			log_warning("anim name mismatch on %d,lhs = %s,rhs = %s\n",
					a,getAnimName(a),model->getAnimName(a));
			return false;
		}
		if(getAnimFrameCount(a)!=model->getAnimFrameCount(a))
		{
			log_warning("animation frame count mismatch on %d,lhs = %d,rhs = %d\n",
					a,getAnimFrameCount(a),model->getAnimFrameCount(a));
			return false;
		}
		if(fabs(getAnimFPS(a)-model->getAnimFPS(a))>tolerance)
		{
			//How to print?
			//log_warning("animation fps mismatch on %d,lhs = %d,rhs = %d\n",
			log_warning("animation fps mismatch on %d,lhs = %f,rhs = %f\n",
					a,model->getAnimFPS(a),model->getAnimFPS(a));
			return false;
		}

		//FIX ME //Why not propEqual?
		auto sa = m_anims[a], sb = model->m_anims[a]; //2020
		
		int fcount = getAnimFrameCount(a);
		int type = getAnimType(a);
		for(int f=0;f<fcount;f++)
		{
			if(type&ANIMMODE_JOINT)
			{			
				auto time = sa->m_timetable2020[f];
				if(fabs(time-sb->m_timetable2020[f])>tolerance)
				{
					log_warning("animation timestamp mismatch on %d,lhs = %f,rhs = %f\n",
							a,time,sb->m_timetable2020[f]);
					return false;
				}
				for(Position b{PT_Joint,0};b<(unsigned)lhsBCount;b++)
				{
					Position rb{PT_Joint,(unsigned)jointMap[b]};
				
					//NOTE: These omit model-> so would always fail.
					//I want to retire interpSkelAnimKeyframe usage.
					//https://github.com/zturtleman/mm3d/issues/123
					/*REFERENCE (2020)

					bool lhs_havekf;
					bool rhs_havekf;

					double lhs_param[3];
					double rhs_param[3];

					lhs_havekf = getKeyframe(a,f,b,KeyRotate,
							lhs_param[0],lhs_param[1],lhs_param[2]);
					rhs_havekf = getKeyframe(a,f,rb,KeyRotate,
							rhs_param[0],rhs_param[1],rhs_param[2]);

					if(lhs_havekf!=rhs_havekf)
					{
						if(!lhs_havekf)
						{
							interpSkelAnimKeyframe(a,f,true,b,KeyRotate,
									lhs_param[0],lhs_param[1],lhs_param[2]);
						}
						if(!rhs_havekf)
						{
							interpSkelAnimKeyframe(a,f,true,rb,KeyRotate,
									rhs_param[0],rhs_param[1],rhs_param[2]);
						}
					}

					if(lhs_havekf||rhs_havekf)
					{
						Matrix lhs_mat;
						Matrix rhs_mat;
						lhs_mat.setRotation(lhs_param);
						rhs_mat.setRotation(rhs_param);

						if(!matrixEquiv(lhs_mat,rhs_mat))
						{
							log_warning("rotation keyframe %d mismatch on anim %d for joint %d\n",
									f,a,b);
							return false;
						}
					}

					lhs_havekf = getKeyframe(a,f,b,KeyTranslate,
							lhs_param[0],lhs_param[1],lhs_param[2]);
					rhs_havekf = getKeyframe(a,f,rb,KeyTranslate,
							rhs_param[0],rhs_param[1],rhs_param[2]);

					if(lhs_havekf!=rhs_havekf)
					{
						if(!lhs_havekf)
						{
							interpSkelAnimKeyframe(a,f,true,b,KeyTranslate,
									lhs_param[0],lhs_param[1],lhs_param[2]);
						}
						if(!rhs_havekf)
						{
							interpSkelAnimKeyframe(a,f,true,rb,KeyTranslate,
									rhs_param[0],rhs_param[1],rhs_param[2]);
						}
					}

					if(lhs_havekf)
					{
						if(!floatCompareVector(lhs_param,rhs_param,3,tolerance))
						{
							log_warning("translation keyframe %d mismatch on anim %d for joint %d\n",
									f,a,b);
							return false;
						}
					}*/
					//HACK: I hope this is good enough?
					interpKeyframe(a,f,time,b,lhs_mat);
					model->interpKeyframe(a,f,time,rb,rhs_mat);
					if(!matrixEquiv(lhs_mat,rhs_mat/*,tolerance?*/))
					{
						log_warning("keyframe %d mismatch on anim %d for joint %d\n",f,a,b);
						return false;
					}
				}
			}		
			if(type&ANIMMODE_FRAME)
			{				
				model_ops_TriMatchMap::const_iterator it;
				for(it = triMap.begin(); it!=triMap.end(); ++it)
				{
					double coords[3];
					double rcoords[3];
					for(int i = 0; i<3; i++)
					{
						int lv = getTriangleVertex(it->first,i);
						int rv = model->getTriangleVertex(it->second.rtri,
								(i+it->second.indexOffset)%3);
						auto le = getFrameAnimVertexCoords(a,f,lv,
								coords[0],coords[1],coords[2]);
						auto re = model->getFrameAnimVertexCoords(a,f,rv,
								rcoords[0],rcoords[1],rcoords[2]);

						if(le!=re||le&&!floatCompareVector(coords,rcoords,3,tolerance))
						{
							log_warning("anim frame triangle %d mismatch on anim %d for frame %d\n",
									it->first,a,f);
							return false;
						}
					}
				}

				IntMap::const_iterator pit;
				for(pit = pointMap.begin(); pit!=pointMap.end(); ++pit)
				{
					double vec[3];
					double rvec[3];

					//getFrameAnimPointCoords(a,f,pit->first,
					getKeyframe(a,f,{PT_Point,(unsigned)pit->first},KeyTranslate,
							vec[0],vec[1],vec[2]);
					//model->getFrameAnimPointCoords(a,f,pit->second,
					model->getKeyframe(a,f,{PT_Point,(unsigned)pit->second},KeyTranslate,
							rvec[0],rvec[1],rvec[2]);

					if(!floatCompareVector(vec,rvec,3,tolerance))
					{
						log_warning("anim frame point %d coord mismatch on anim %d for frame %d\n",
								pit->first,a,f);
						return false;
					}

					//getFrameAnimPointRotation(a,f,pit->first,
					getKeyframe(a,f,{PT_Point,(unsigned)pit->first},KeyRotate,
							vec[0],vec[1],vec[2]);
					//model->getFrameAnimPointRotation(a,f,pit->second,
					model->getKeyframe(a,f,{PT_Point,(unsigned)pit->second},KeyRotate,
							rvec[0],rvec[1],rvec[2]);

					if(!floatCompareVector(vec,rvec,3,tolerance))
					{
						log_warning("anim frame point %d rot mismatch on anim %d for frame %d\n",
								pit->first,a,f);
						return false;
					}
				}			
			}
		}
	}

	return true;
}

bool Model::propEqual(const Model *model, int partBits, int propBits,
		double tolerance)const
{
	unsigned numVertices	 = m_vertices.size();
	unsigned numTriangles	= m_triangles.size();
	unsigned numGroups		= m_groups.size();
	unsigned numJoints		= m_joints.size();
	unsigned numPoints		= m_points.size();
	unsigned numTextures	 = m_materials.size();
	unsigned numProjections = m_projections.size();
	unsigned numFrameAnims  = 0;
	unsigned numSkelAnims	= 0;
	unsigned numRemAnims	= 0;
	for(auto*ea:m_anims) switch(ea->_type)
	{
	case 1: numSkelAnims++; continue;
	case 2: numFrameAnims++; continue;
	case 3: numRemAnims++; continue;
	}

	unsigned t = 0;
	unsigned v = 0;

	std::string dstr;

	if(partBits&(PartVertices|PartFrameAnims/*2020*/))
	{
		if(numVertices!=model->m_vertices.size())
		{
			log_warning("match failed at vertex count %d!=%d\n",
					numVertices,model->m_vertices.size());
			return false;
		}

		for(v = 0; v<numVertices; v++)
		{
			if(!m_vertices[v]->propEqual(*model->m_vertices[v],propBits,tolerance))
			{
				log_warning("match failed at vertex %d\n",v);
				m_vertices[v]->sprint(dstr);
				log_warning("lhs:\n%s\n",dstr.c_str());
				model->m_vertices[v]->sprint(dstr);
				log_warning("rhs:\n%s\n",dstr.c_str());
				return false;
			}
		}
	}

	if(partBits&PartFaces)
	{
		if(numTriangles!=model->m_triangles.size())
		{
			log_warning("match failed at triangle count %d!=%d\n",
					numTriangles,model->m_triangles.size());
			return false;
		}

		for(t = 0; t<numTriangles; t++)
		{
			if(!m_triangles[t]->propEqual(*model->m_triangles[t],propBits,tolerance))
			{
				log_warning("match failed at triangle %d\n",t);
				m_triangles[t]->sprint(dstr);
				log_warning("lhs:\n%s\n",dstr.c_str());
				model->m_triangles[t]->sprint(dstr);
				log_warning("rhs:\n%s\n",dstr.c_str());
				return false;
			}
		}
	}

	if(partBits&PartGroups)
	{
		if(numGroups!=(unsigned)model->getGroupCount())
		{
			log_warning("match failed at group count %d!=%d\n",
					numGroups,model->m_groups.size());
			return false;
		}

		for(unsigned int g = 0; g<numGroups; g++)
		{
			if(!m_groups[g]->propEqual(*model->m_groups[g],propBits,tolerance))
			{
				log_warning("match failed at group %d\n",g);
				m_groups[g]->sprint(dstr);
				log_warning("lhs:\n%s\n",dstr.c_str());
				model->m_groups[g]->sprint(dstr);
				log_warning("rhs:\n%s\n",dstr.c_str());
				return false;
			}
		}
	}

	if(partBits&PartJoints)
	{
		if(numJoints!=model->m_joints.size())
		{
			log_warning("match failed at joint count %d!=%d\n",
					numJoints,model->m_joints.size());
			return false;
		}

		for(unsigned j = 0; j<numJoints; j++)
		{
			if(!m_joints[j]->propEqual(*model->m_joints[j],propBits,tolerance))
			{
				log_warning("match failed at joint %d\n",j);
				m_joints[j]->sprint(dstr);
				log_warning("lhs:\n%s\n",dstr.c_str());
				model->m_joints[j]->sprint(dstr);
				log_warning("rhs:\n%s\n",dstr.c_str());
				return false;
			}
		}
	}

	if(partBits&PartPoints)
	{
		if(numPoints!=model->m_points.size())
		{
			log_warning("match failed at point count %d!=%d\n",
					numPoints,model->m_points.size());
			return false;
		}

		for(unsigned p = 0; p<numPoints; p++)
		{
			if(!m_points[p]->propEqual(*model->m_points[p],propBits,tolerance))
			{
				log_warning("match failed at point %d\n",p);
				m_points[p]->sprint(dstr);
				log_warning("lhs:\n%s\n",dstr.c_str());
				model->m_points[p]->sprint(dstr);
				log_warning("rhs:\n%s\n",dstr.c_str());
				return false;
			}
		}
	}

	if(partBits&(PartMaterials | PartTextures))
	{
		if(numTextures!=model->m_materials.size())
		{
			log_warning("match failed at material count %d!=%d\n",
					numTextures,model->m_materials.size());
			return false;
		}

		for(t = 0; t<numTextures; t++)
		{
			if(!m_materials[t]->propEqual(*model->m_materials[t],propBits,tolerance))
			{
				log_warning("match failed at material %d\n",t);
				m_materials[t]->sprint(dstr);
				log_warning("lhs:\n%s\n",dstr.c_str());
				model->m_materials[t]->sprint(dstr);
				log_warning("rhs:\n%s\n",dstr.c_str());

				return false;
			}
		}
	}

	if(partBits&PartProjections)
	{
		if(numProjections!=model->m_projections.size())
		{
			log_warning("match failed at projection count %d!=%d\n",
					numVertices,model->m_vertices.size());
			return false;
		}

		for(t = 0; t<numProjections; t++)
		{
			if(!m_projections[t]->propEqual(*model->m_projections[t],propBits,tolerance))
			{
				log_warning("match failed at projection %d\n",t);
				/*
				// FIXME
				m_projections[t]->sprint(dstr);
				log_warning("lhs:\n%s\n",dstr.c_str());
				model->m_projections[t]->sprint(dstr);
				log_warning("rhs:\n%s\n",dstr.c_str());
				*/
				return false;
			}
		}
	}

	if(partBits&PartSkelAnims)
	{
		auto cmp = model->getAnimationCount(ANIMMODE_JOINT);
		if(numSkelAnims!=cmp)
		{
			log_warning("match failed at skel anim count %d!=%d\n",
					numSkelAnims,cmp);
			return false;
		}

		for(t = 0; t<numSkelAnims; t++)
		{
			if(!m_anims[t]->propEqual(*model->m_anims[t],propBits,tolerance))
			{
				log_warning("match failed at skel animation %d\n",t);
				return false;
			}
		}
	}
	if(partBits&PartFrameAnims)
	{
		auto cmp = model->getAnimationCount(ANIMMODE_FRAME);
		if(numFrameAnims!=cmp)
		{
			log_warning("match failed at frame anim count %d!=%d\n",numFrameAnims,cmp);
			return false;
		}
		cmp = numSkelAnims;
		for(t = 0; t<numFrameAnims; t++)
		{
			if(!m_anims[cmp+t]->propEqual(*model->m_anims[cmp+t],propBits,tolerance))
			{
				log_warning("match failed at frame animation %d\n",t);
				return false;
			}
		}
	}
	if(partBits&PartAnims)
	{
		auto cmp = model->getAnimationCount(ANIMMODE);
		if(numFrameAnims!=cmp)
		{
			log_warning("match failed at skel+frame anim count %d!=%d\n",
					numFrameAnims,cmp);
			return false;
		}
		cmp = numSkelAnims+numFrameAnims;
		for(t = 0; t<numFrameAnims; t++)
		{
			if(!m_anims[cmp+t]->propEqual(*model->m_anims[cmp+t],propBits,tolerance))
			{
				log_warning("match failed at skel+frame animation %d\n",t);
				return false;
			}
		}
	}

	if(partBits&PartMeta)
	{
		if(getMetaDataCount()!=model->getMetaDataCount())
		{
			log_warning("match failed at meta data count %d!=%d\n",
					getMetaDataCount(),model->getMetaDataCount());
			return false;
		}

		unsigned int mcount = getMetaDataCount();
		for(unsigned int m = 0; m<mcount; ++m)
		{
			char key[1024];
			char value_lhs[1024];
			char value_rhs[1024];

			getMetaData(m,key,sizeof(key),value_lhs,sizeof(value_lhs));

			if(!model->getMetaData(m,key,sizeof(key),value_rhs,sizeof(value_rhs)))
			{
				log_warning("missing meta data key: '%s'\n",key);
				return false;
			}
			else
			{
				if(strcmp(value_lhs,value_rhs)!=0)
				{
					log_warning("meta data value mismatch for '%s'\n",key);
					log_warning("  '%s'!='%s'\n",value_lhs,value_rhs);
					return false;
				}
			}
		}
	}

	if(partBits&PartBackgrounds)
	{
		for(unsigned int b = 0; b<MAX_BACKGROUND_IMAGES; ++b)
		{
			if(!m_background[b]->propEqual(*model->m_background[b],propBits,tolerance))
			{
				log_warning("match failed at background image %d\n",t);
				/*
				// FIXME
				m_background[b]->sprint(dstr);
				log_warning("lhs:\n%s\n",dstr.c_str());
				model->m_background[b]->sprint(dstr);
				log_warning("rhs:\n%s\n",dstr.c_str());
				*/
				return false;
			}
		}
	}

	return true;
}

#ifdef MM3D_EDIT

struct SpatialSort //credit Assimp/SpatialSort.cpp
{	
	Vector planeNormal;

    SpatialSort(Model &m):planeNormal(0.8523f,0.34321f,0.5736f)
    {
		planeNormal.normalize();

		auto &pl = m.getVertexList();

		entries.reserve(pl.size());
		Entry e; e.index = 0;
		for(auto*p:pl)
		{
			for(int i=3;i-->0;)
			e[i] = p->m_coord[i];
			e[3] = e.dot3(planeNormal);
			entries.push_back(e); 
			e.index++;
		}
		std::sort(entries.begin(),entries.end());
	}

	struct Entry : Vector
	{
        int index; 

        inline bool operator<(const Entry &e)const
		{
			return m_val[3]<e.m_val[3]; //distance
		}
    };
	std::vector<Entry> entries;
	
	void find(double cmp[3], double radius, int_list &out)const
	{
		const double dist = dot3(cmp,planeNormal.getVector());
		const double minDist = dist-radius, maxDist = dist+radius;
		out.clear();
		if(entries.empty()||maxDist<entries.front()[3]||minDist>entries.back()[3]) 
		return;

		size_t i = entries.size()/2, binaryStepSize = entries.size()/4;
		for(;binaryStepSize>1;binaryStepSize/=2)
		if(entries[i][3]<minDist) 
		i+=binaryStepSize; else i-=binaryStepSize;		
		while(i>0&&entries[i][3]>minDist) 
		i--;
		while(i<(entries.size()-1)&&entries[i][3]<minDist) 
		i++;
		const double radius2 = radius*radius;
		for(;i<entries.size()&&entries[i][3]<maxDist;i++)
		{
			double diff[3];
			for(int j=3;j-->0;)
			diff[j] = entries[i][j]-cmp[j];
			if(squared_mag3(diff)<radius2)
			out.push_back(entries[i].index);
		}
	}
};

unsigned Model::_dup(Animation *a, bool keyframes)
{
	unsigned index = addAnimation((AnimationModeE)a->_type,a->m_name.c_str());
	setAnimFrameCount(index,a->_frame_count());

	Animation *b = m_anims[index];

	b->m_fps = a->m_fps;
	b->m_frame2020 = a->m_frame2020;
	b->m_timetable2020 = a->m_timetable2020;
	b->m_wrap = a->m_wrap;

	if(keyframes)
	{
		for(auto&ea:a->m_keyframes)
		for(auto*kf:ea.second)
		{
			setKeyframe(index,kf->m_frame,ea.first,kf->m_isRotation,
			kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
		}
	}

	return index; //return b;
}
bool Model::mergeAnimations(Model *model)
{
	if(model->m_anims.empty())
	{
		//msg_warning(TRANSLATE("LowLevel","Model contains no skeletal animations"));
		msg_warning(TRANSLATE("LowLevel","Model contains no animations"));
		return false;
	}

	unsigned sa_count = model->getAnimationCount(ANIMMODE_JOINT);

	if(sa_count)
	{
		unsigned j1 = getBoneJointCount();
		unsigned j2 = model->getBoneJointCount();

		if(j1!=j2) mismatchwarn:
		{
			msg_warning(TRANSLATE("LowLevel","Model skeletons do not match"));
			return false;
		}
		for(unsigned j = 0; j<j1; j++)
		if(m_joints[j]->m_parent!=model->m_joints[j]->m_parent)
		{
			goto mismatchwarn;
		}
	}

	if(sa_count) // Do skeletal add
	{
		for(unsigned i=0;i<sa_count;i++)
		{
			_dup(model->m_anims[i]);
		}
	}

	unsigned fa_count = model->m_anims.size()-sa_count;

	if(fa_count) //2020
	{
		std::vector<std::pair<Animation*,Animation*>> ab;

		for(unsigned i=0;i<fa_count;i++)
		{
			Animation *a = model->m_anims[sa_count+i];
			Animation *b = m_anims[_dup(a)];
			ab.push_back({a,b});
		}
		
		SpatialSort ss(*model);
		int_list match;

		//todo: want to configure this
		double r = 2*std::numeric_limits<float>::epsilon();

		//todo: offer 1-to-1 alternative
		for(auto*v:m_vertices)
		{
			ss.find(v->m_coord,r,match);

			if(match.empty()) continue;

			if(match.size()>1)
			{
				//todo: try to resolve conflicts
			}

			auto *w = model->m_vertices[match.front()];

			//I've changed MU_SetAnimFrameCount so that it should restore
			//these without having to generate separate undo data
			for(auto&ea:ab)
			{
				int ap = ea.first->m_frame0;
				if(0==~ap) continue;
				int bp = ea.second->_frame0(this);

				for(size_t i=ea.first->_frame_count();i-->0;)
				{
					*v->m_frames[bp++] = *w->m_frames[ap++];
				}
			}
		}
	}

	return true;
}

bool Model::mergeModels(const Model *model, bool textures, AnimationMergeE animations, bool emptyGroups)
{
	if(animations==AM_MERGE) //Do up front with error!
	{		
		bool merge = true;
		for(int i=1;i<=3;i++)// Do skeletal merge if possible
		{
			unsigned ac1 = getAnimationCount((AnimationModeE)i);
			unsigned ac2 = model->getAnimationCount((AnimationModeE)i);
			if(ac1&&ac2&&ac1!=ac2) merge = false;
		}
		if(!merge)
		{
			model_status(this,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel",
			"Reason can't merge animations: type and count mismatch"));
			return false;
		}	
	}

	std::unordered_map<int,int> groupMap;
	std::unordered_map<int,int> materialMap;
	std::unordered_set<int> materialsNeeded;

	unsigned vertbase = m_vertices.size();
	unsigned tribase = m_triangles.size();
	unsigned jointbase = m_joints.size();
	unsigned pointbase = m_points.size();
	unsigned projbase = m_projections.size();

	unselectAll();

	unsigned n,count;
	count = model->m_vertices.size();
	for(n=0;n<count;n++)
	{
		Vertex *v = model->m_vertices[n];		
		addVertex(v->m_coord[0],v->m_coord[1],v->m_coord[2]);
	}

	count = model->m_triangles.size();
	for(n=0;n<count;n++)
	{
		Triangle *tri = model->m_triangles[n];
		addTriangle(tri->m_vertexIndices[0]+vertbase,
		tri->m_vertexIndices[1]+vertbase,tri->m_vertexIndices[2]+vertbase);
	}

	//I guess groups are needed to be able to easily assign existing
	//materials and groups to them. The "clean up" function can be
	//used to remove them after (TODO: could use a Groups option)
	//if(textures)
	{
		count = model->m_groups.size();
		for(n=0;n<count;n++)
		{
			if(emptyGroups||!model->getGroupTriangles(n).empty())
			{
				const char *name = model->getGroupName(n);
				int g = addGroup(name);
				groupMap[n] = g;
				auto *gp = m_groups[g];
				auto *gq = model->m_groups[n];
				gp->m_smooth = gq->m_smooth;
				gp->m_angle = gq->m_angle; //2022
				int mat = gq->m_materialIndex;
				if(mat>=0) materialsNeeded.insert(mat);
			}
		}
	}

	count = model->m_joints.size(); 
	if(count>0) model->validateSkel();
	for(n=0;n<count;n++)
	{
		//Joint *joint = model->m_joints[n];
		Joint *joint = model->m_joints2[n].second;
			
		int parent = joint->m_parent;
		if(parent>=0) parent+=jointbase;

		//NOTE: Passing those rotations to addBoneJoint was
		//incorrect anyway since it modified them.
		double *tran = joint->m_abs;
		unsigned nj = addBoneJoint(joint->m_name.c_str(),tran[0],tran[1],tran[2],
		/*rot[0],rot[1],rot[2],*/parent);
		memcpy(m_joints[nj]->m_rot,joint->m_rot,sizeof(joint->m_rot));
		memcpy(m_joints[nj]->m_xyz,joint->m_xyz,sizeof(joint->m_xyz));
	}

	count = model->m_points.size(); 	
	for(n=0;n<count;n++)
	{
		Point *point = model->m_points[n];

		double *tran = point->m_abs, *rot = point->m_rot;
		int pnum = addPoint(point->m_name.c_str(),tran[0],tran[1],tran[2],rot[0],rot[1],rot[2]);

		memcpy(m_points[pnum]->m_xyz,point->m_xyz,sizeof(point->m_xyz));

		for(auto&ea:model->m_points[n]->m_influences)
		{
			setPointInfluence(pnum,ea.m_boneId+jointbase,ea.m_type,ea.m_weight);
		}
	}

	count = model->m_vertices.size();
	for(n=0;n<count;n++)
	{
		for(auto&ea:model->m_vertices[n]->m_influences)
		{
			setVertexInfluence(n+vertbase,ea.m_boneId+jointbase,ea.m_type,ea.m_weight);
		}
	}

	if(textures)
	{
		count = model->getTextureCount();
		for(n=0;n<count;n++)
		{
			if(materialsNeeded.find(n)!=materialsNeeded.end())
			{
				int newMat;
				if(model->getMaterialType(n)==Model::Material::MATTYPE_TEXTURE)
				{
					auto *texmgr = TextureManager::getInstance();
					auto *newtex = texmgr->getTexture(model->getTextureFilename(n));
					newMat = addTexture(newtex);
					setTextureName(newMat,model->getTextureName(n));
				}
				else newMat = addColorMaterial(model->getTextureName(n));
				materialMap[n] = newMat;

				float val[4], shin;
				model->getTextureAmbient(n,val);
				setTextureAmbient(newMat,val);
				model->getTextureDiffuse(n,val);
				setTextureDiffuse(newMat,val);
				model->getTextureEmissive(n,val);
				setTextureEmissive(newMat,val);
				model->getTextureSpecular(n,val);
				setTextureSpecular(newMat,val);
				model->getTextureShininess(n,shin);
				setTextureShininess(newMat,shin);
			}
		}

		count = model->m_groups.size();
		for(n=0;n<count;n++)		
		if(groupMap.find(n)!=groupMap.end())
		{
			int oldMat = model->getGroupTextureId(n);
			setGroupTextureId(groupMap[n],materialMap[oldMat]);
		}		
	}
	//if(textures) //2020: why rope this in?
	{
		count = model->getProjectionCount();
		for(n=0;n<count;n++)
		{
			double pos[3],rot[3],scale,range[2][2];
			int type = model->getProjectionType(n);

			model->getProjectionCoords(n,pos);
			model->getProjectionRotation(n,rot);
			scale = model->getProjectionScale(n);
			model->getProjectionRange(n,range[0][0],range[0][1],range[1][0],range[1][1]);

			addProjection(model->getProjectionName(n),type,pos[0],pos[1],pos[2]);
			setProjectionRotation(n+projbase,rot);
			setProjectionScale(n+projbase,scale);
			setProjectionRange(n+projbase,range[0][0],range[0][1],range[1][0],range[1][1]);
		}

		count = model->getTriangleCount();
		int tpcount = getProjectionCount();
		for(n=0;n<count;n++)
		{
			float st[2][3];			
			model->getTextureCoords(n,st);
			setTextureCoords(n+tribase,st);

			if(tpcount) //2020
			{
				int prj = model->getTriangleProjection(n);
				if(prj>=0&&(prj+(int)projbase)<tpcount)
				{
					setTriangleProjection(n+tribase,prj+projbase);
				}
			}

			//if(textures) //see above rationale
			{
				int grp = model->m_triangles[n]->m_group;
				if(grp>=0)
				{
					addTriangleToGroup(groupMap[grp],n+tribase);
				}
			}
		}
	}

	int_list frameMap;

	if(animations!=AM_NONE) 
	for(unsigned ac1,ac2,base2=0,i=1;i<=3;i++,base2+=ac2)
	{
		// Do merge if possible
		ac1 = getAnimationCount((AnimationModeE)i);
		ac2 = model->getAnimationCount((AnimationModeE)i);
		bool merge = animations==AM_MERGE&&ac1&&ac2&&ac1==ac2;

		unsigned base = getAnimationIndex((AnimationModeE)i);
		if(!merge) base+=ac1;

		for(unsigned a=0;a<ac2;a++)
		{	
			auto p = model->m_anims[base2+a];
			auto fc = p->_frame_count();			

			frameMap.clear(); if(merge)
			{
				auto q = m_anims[base+a]; 

				double r = 1;
				if(q->m_fps!=p->m_fps) r = q->m_fps/p->m_fps;

				if(r!=1||q->m_timetable2020!=p->m_timetable2020)
				{
					setAnimTimeFrame(base+a,
					std::max(q->_time_frame(),r*p->_time_frame()));

					for(unsigned j=0;j<fc;j++)
					frameMap.push_back(insertAnimFrame(base+a,r*p->m_timetable2020[j]));
				}
			}
			else 
			{
				unsigned num = _dup(p,false);
				assert(num==base+a);
			}

			for(auto&ea:p->m_keyframes)
			{
				auto pos = ea.first; pos.index+=jointbase;
				for(Keyframe*kf:ea.second)
				{
					auto fr = kf->m_frame;
					if(!frameMap.empty()) fr = frameMap[fr];

					setKeyframe(base+a,fr,pos,kf->m_isRotation,
					kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
				}
			}

			if(fc&&~p->m_frame0) //ANIMMODE_FRAME
			{
				auto fp = p->m_frame0;
				auto fd = m_anims[base+a]->_frame0(this);				
				//if(fc) //OUCH
				for(size_t v=model->m_vertices.size();v-->0;)
				{
					//Note, it's easier on the cache to do
					//the vertices in the outer loop

					auto p = &model->m_vertices[v]->m_frames[fp];
					auto d = &m_vertices[vertbase+v]->m_frames[fd];

					if(frameMap.empty())
					for(unsigned c=fc;c-->0;d++,p++) 
					{
						**d = **p;
					}
					else for(unsigned c=0;c<fc;c++,p++)
					{
						*d[frameMap[c]] = **p;
					}
				}			
			}
		}
	}
			
	n = getTriangleCount();
	while(n-->tribase) selectTriangle(n);
	//n=getVertexCount();
	//while(n-->vertbase) selectVertex(n);
	n = getBoneJointCount();
	while(n-->jointbase) selectBoneJoint(n);
	n = getPointCount();
	while(n-->pointbase) selectPoint(n);
	n = getProjectionCount();
	while(n-->projbase) selectProjection(n);

	calculateSkel(); calculateNormals();

	return true;
}

const int SE_POLY_MAX = 2;
struct model_ops_SimplifyEdgeT
{
	unsigned vFar;
	
	int polyCount;
	int poly[SE_POLY_MAX];

	double vecB[3]; //OPTIMIZING
};
struct model_ops_SimplifyVertT
{
	unsigned vrt; float s,t;
};
bool Model::simplifySelectedMesh(float subpixel)
{
	bool preserve = subpixel!=0;

	std::vector<model_ops_SimplifyEdgeT> edges;
	std::vector<model_ops_SimplifyVertT> uvmap;

	//NOTE: Results will depend on how any animation has been posed.
	validateNormals();

	const auto *vl = m_vertices.data();
	const auto *tl = m_triangles.data();

	//2022: This is smarter, as well as leveraging m_faces to avoid
	//extereme nonlinearity.
	for(auto i=m_triangles.size();i-->0;) tl[i]->m_user = (int)i;
	
	//NOTE: This was equiv3 in glmath.h, but 0.0001 doesn't seem
	//like a good epsilon in a header template.
	auto _eq_norms = [](double a[3], double b[3])
	{
		//Is 0.0001 too low? Especially on straight edges?
		return fabs(a[0]-b[0])<0.0001
		&&fabs(a[1]-b[1])<0.0001&&fabs(a[2]-b[2])<0.0001;
	};
		#if 1
		enum{ debug=0 };
		#else
		#ifdef NDEBUG
		#error Disable me (even in debug builds)
		#endif
		#ifdef _DEBUG
		enum{ debug=2 }; //1
		#else
		enum{ debug=0 };
		#endif
		#endif

	bool welded, valid;
	unsigned welds = ~0, any = 0;
	unsigned vcount = m_vertices.size();
	for(;welds;any+=welds) //2022
	for(unsigned v=welds=0;v<vcount;v+=!welded)
	{
		//log_debug("checking vertex %d\n",v);
		welded = false;	
				
		const auto *vp = vl[v];

		//2022: Assume selected triangles will select
		//their respective vertices!!
		if(!vp->m_selected) continue;

		
	// for each vertex
	//	find all edges
	//	if Va to V is same vector as V to Vb
	//	  make sure every *other* edge has exactly two co-planar faces
	//	  if so
	//		 move V to Va and weld
	//		 re-do loop


		// valid weld candidate until we learn otherwise
		valid = true;
		
		// build edge list
		edges.clear();
		for(auto&ea:vp->m_faces) 
		if(ea.first->m_selected&&ea.first->m_user!=-1)
		{
			// If triangle is using v as a vertex, add to edge list
			auto *tv = ea.first->m_vertexIndices;
			int idx[3];
			idx[0] = -1; if(v==tv[0])
			{
				idx[0] = 0; idx[1] = 1; idx[2] = 2;
			}
			else if(tv[1]==v)
			{
				idx[0] = 1; idx[1] = 0; idx[2] = 2;
			}
			else if(tv[2]==v)
			{
				idx[0] = 2; idx[1] = 0; idx[2] = 1;
			}
			if(idx[0]==-1) continue;
			
			//log_debug("  triangle %d uses vertex %d\n",t,v);
			// vert[idx[1]] and vert[idx[2]] are the opposite vertices
			for(int i=1;i<=2;i++)
			{
				unsigned vrt = tv[idx[i]];
				bool newEdge = true;
				for(auto&e:edges) 
				if(e.vFar==vrt) if(e.polyCount>=SE_POLY_MAX)
				{
					//NOTE: I think this is like a fin/spoke situation.
					
					// more than two faces on this edge
					// we can't weld at all, skip this vertex
					//log_debug("  too many polygons connected to edge to %d\n",ea.vFar);
					valid = false;
				}
				else
				{
					valid = true;

					e.poly[e.polyCount++] = ea.first->m_user;
				
					//log_debug("  adding polygon to edge for %d\n",e.vFar);
					newEdge = false; break;
				}

				if(valid&&newEdge)
				{
					//log_debug("  adding new edge for polygon for %d\n",idx[i]);
					model_ops_SimplifyEdgeT se;
					se.vFar = vrt;
					se.polyCount = 1;
					se.poly[0] = ea.first->m_user; 

					edges.push_back(se);
				}
			}
			if(!valid) break;
		}
		if(!valid) continue;

		//2022: Skip orphans on second (etc.) passes... or just in general.
		if(edges.empty()) continue;
		
		// use vectors from two edges to see if they are in a straight line
		//getVertexCoords(v,coords);
		Vector coords,delta,vecA;
		coords.setAll(vp->m_absSource);

		float uvtol[2];

		for(auto&b:edges) //vecB
		{
			//getVertexCoordsb.vFar,vecB);
			delta.setAll(vl[b.vFar]->m_absSource);
			(delta-=coords).normalize3();
			delta.getVector3(b.vecB);
		}
		for(auto&a:edges)
		{
			//getVertexCoords(a.vFar,vecA);
			delta.setAll(vl[a.vFar]->m_absSource);
			(vecA=coords-delta).normalize3();

			for(auto&b:edges) if(&a!=&b)
			{
				//getVertexCoords(b.vFar,vecB);
			//	delta.setAll(vl[b.vFar]->m_absSource);
			//	(vecB=delta-coords).normalize3();
				if(!_eq_norms(vecA.getVector(),b.vecB)) continue; 
			
				//2022: There needs to be split on either side of
				//the straight edge to not foil potential merging.
				int sides = 1;
				double sideplane[3];
				if(a.polyCount>1||b.polyCount>1)
				{
				//	sides = 2;
					auto &ab = a.polyCount>1?a:b;
					auto *t0 = tl[ab.poly[0]];
					auto *t1 = tl[ab.poly[1]];
					double avg[3], sum = 0;
					for(int i=3;i-->0;)
					sum+=avg[i]=(t0->m_flatSource[i]+t1->m_flatSource[i])*0.5;
					if(fabs(sum)>0.00001)
					{
						sides = 2; cross_product(sideplane,b.vecB,avg);
					}
				}				
				for(int side=sides;side-->0;)
				{
					int grp = -2; uvmap.clear(); //2022

					//2022: This is just a commonsense optimization, that
					//I'm not 100% clear won't negatively impact outcomes
					//by changing the order which eliminations will occur.
					enum{ prefilter_flipped_winding=0 };

					//log_debug("  found a straight line\n");
					bool canWeld = true;
					for(auto&e:edges) if((&e!=&a)&&(&e!=&b))
					{
						if(sides==2)
						{
							double s = dot3(sideplane,e.vecB);
							if((s<0)==(side==1)) continue;
						}

						// must have a face on each side of edge
						if(e.polyCount!=2)
						{
							//log_debug("	 not enough polygons connected to edge\n");
							canWeld = false; break;
						}

						auto *t0 = tl[e.poly[0]], *t1 = tl[e.poly[1]];

						if(preserve) //2022: Don't ignore appearances?
						{
							//Triangles on one side must share groups.

							if(grp==-2)
							{
								grp = t0->m_group;

								if(auto*t=getTextureData(getGroupTextureId(grp)))
								{
									uvtol[0] = subpixel/t->m_origWidth;
									uvtol[1] = subpixel/t->m_origHeight;
								}
								else uvtol[0] = uvtol[0] = 0.0001f; //ARBITARY
							}

							if(grp!=t0->m_group||grp!=t1->m_group)
							{
								canWeld = false; break; //group?
							}
						}
						//NOTE: UVs are maintained even when "ignore"
						//is set so that less fixup is required after.
						for(int poly=2;poly-->0;) 
						{
							auto *tp = poly==1?t1:t0;
							auto *tv = tp->m_vertexIndices;
							auto *st =&tp->m_s;
							for(int i=3;i-->0;)
							{
								bool found = false;

								unsigned vrt = tv[i];

								float s = st[0][i], t = st[1][i];

								for(auto&uv:uvmap) if(uv.vrt==vrt)
								{
									if(preserve)
									if(fabsf(s-uv.s)>uvtol[0]||fabsf(t-uv.t)>uvtol[1])
									{
										canWeld = false; i = 0; //OPTIMIZING?
									}
									found = true; break;
								}
								if(!found) uvmap.push_back({vrt,s,t});
							}						
							if(!canWeld) break;
						}

						if(!_eq_norms(t0->m_flatSource,t1->m_flatSource)) 
						{
							// faces must be in the same plane
					
							//log_debug("	 polygons on edge do not face the same direction\n");
							canWeld = false;
						}
						else for(int i=e.polyCount;i-->0;) // check inverted normals
						{
							//WHY??? ISN'T THE PREVIOUS TEST ABOUT NORMALS? WINDING?

							if(!prefilter_flipped_winding) break; //EXPERIMENTAL

							auto *tp = tl[e.poly[i]];
							auto *tv = tp->m_vertexIndices;

							bool flat = false;
							double *tcoords[3] = {};
							for(int j=3;j-->0;) if(tv[j]==v)
							{
								tcoords[j] = vl[a.vFar]->m_absSource; 
							}
							else if(tv[j]==a.vFar)
							{
								//NOTE: I guess this can happen if the incoming model
								//includes degenerate triangles. (They'll be removed
								//with the rest if so.)
								flat = true; 
							}
							if(!flat)
							{
								for(int j=3;j-->0;) if(!tcoords[j])
								{
									tcoords[j] = vl[tv[j]]->m_absSource;
								}
								double norm[3];
								calculate_normal(norm,tcoords[0],tcoords[1],tcoords[2]);

								//log_debug("-- %f,%f,%f  %f,%f,%f\n",norm[0],norm[1],norm[2],e.normal[i][0],e.normal[i][1],e.normal[i][2]);
								if(!_eq_norms(norm,tp->m_flatSource))
								{
								//	assert(0); //I want to know when this happens. (It happens.)

									//log_debug("normal gets inverted on collapse, skipping\n");
									canWeld = false; break;
								}
							}
						}
						if(!canWeld) break;
					}
					if(!canWeld) continue;

					if(preserve) //Avoid UV collapse?
					{
						//NOTE: This has to stay on sides.

						float s[3],t[3]; for(auto&ea:uvmap)
						{
							//NOTE: To get this far these
							//must all be equal.
							if(ea.vrt==a.vFar)
							{
								s[0] = ea.s; t[0] = ea.t;
							}
							else if(ea.vrt==b.vFar)
							{
								s[2] = ea.s; t[2] = ea.t;
							}
							else if(ea.vrt==v)
							{
								s[1] = ea.s; t[1] = ea.t;
							}
						}

						//Reducing this tolerance helps with little nicks
						//left over from boolean (CSG) operations. In the
						//test I did I couldn't see a difference visually.
					//	static const float err = 0.005f; //0.0001f;

						//Same as vecA==vecB in UV space.
						float stA[2] = { s[0]-s[1],t[0]-t[1] };
					//	normalize2(stA);
						float stB[2] = { s[1]-s[2],t[1]-t[2] };
					//	normalize2(stB);
						//dot2 is trying just to eliminate opposite directions
						//as if that is a test for mirrored UVs. It just seems
						//like requiring them to line up is eliminating things
						//that look fine to me. Hopefully this is happy medium
						//since you can still narrow the input selection where
						//dot2 goes too far.
					//	if(fabsf(stA[0]-stB[0])>err||fabsf(stA[1]-stB[1])>err)
						if(dot2(stA,stB)<0)
						{
							continue; //canWeld = false;
						}
					}
								// Yay! We can collapse v to vA

					//log_debug("*** vertex %d can be collapsed to %d\n",v,a.vFar);
					// move v to va on each edge, mark flattened triangles
					for(auto&e:edges)					
					{
						//2022: These were processed before, however I believe they
						//would not have been flattened, and with the sides test it
						//can't place these edges on one side or the other, so they
						//can't be processed here.
						if(&e==&a||&e==&b) continue;

						if(sides==2)
						{
							double s = dot3(sideplane,e.vecB);
							if((s<0)==(side==1)) continue;
						}

						for(int j,i=e.polyCount;i-->0;)
						{
							auto *tp = tl[j=e.poly[i]];
							auto *tv = tp->m_vertexIndices;

							//log_debug("finding %d in triangle %d\n",v,j);

							for(int vrt=a.vFar,k=3;k-->0;) if(tv[k]==v) 
							{
								unsigned tw[3] = { tv[0],tv[1],tv[2] }; tw[k] = vrt;

								bool flat = tw[0]==tw[1]||tw[0]==tw[2]||tw[1]==tw[2];

								if(!flat&&!prefilter_flipped_winding) //EXPERIMENTAL
								{
									double normal[3];
									calculateFlatNormal(tw,normal);
									if(0>=dot3(normal,tp->m_flatSource))									
									switch(k) //Must avoid vrt in uvmap below.
									{
									case 0: std::swap(tw[1],tw[2]); break;
									case 1: std::swap(tw[0],tw[2]); break;
									case 2: std::swap(tw[0],tw[1]); break;
									}									
								}

								setTriangleVertices(j,tw[0],tw[1],tw[2]); 

								//NOTE: Updating m_flatSource isn't necessary
								//as merging demands triangles to be coplanar.
								//if(tp->_flattened()) tp->m_user = -1;
								if(flat) tp->m_user = -1;
								
								for(auto&uv:uvmap) if(vrt==uv.vrt)
								{
									setTextureCoords(j,k,uv.s,uv.t);
								}
								
								welded = true; k = 0; //OPTIMIZING
								
								(debug?any:welds)++; //DEBUGGING?
							}
						}
					}
				}
				if(welded) break;
			}
			if(welded) break;
		} 		
		if(welded&&debug==2) break; //DEBUGGING?
	}

	//NOTE: This leaves some previously selected vertices visible, however
	//it seems a useful indication of where discontinuties arise, so maybe
	//it's alright to not correct it.
	if(any) deleteFlattenedTriangles(); 
	if(any) deleteOrphanedVertices(); 
	
	return any!=0;
}

#endif // MM3D_EDIT
