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

#ifdef MM3D_EDIT
#include "modelundo.h"
#endif // MM3D_EDIT

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

	lhs.apply(lright);
	lhs.apply(lup);
	lhs.apply(lfront);

	rhs.apply(rright);
	rhs.apply(rup);
	rhs.apply(rfront);

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

		float lw = 0;
		WeightMap::const_iterator lwit = lhsInfMap.find(it->m_boneId);
		if(lwit!=lhsInfMap.end())
		{
			lw = lwit->second;
			lhsInfMap.erase(lwit->first);
		}

		float rw = 0;
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

	int lhsTCount	 = m_triangles.size();
	int rhsTCount	 = model->m_triangles.size();
	int lhsGCount	 = m_groups.size();
	int rhsGCount	 = model->m_groups.size();
	int lhsBCount	 = m_joints.size();
	int rhsBCount	 = model->m_joints.size();
	int lhsPCount	 = m_points.size();
	int rhsPCount	 = model->m_points.size();

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
		for(int rb = 0; !match&&rb<rhsBCount; ++rb)
		{
			if(!jointMatched[rb])
			{
				if(jointsMatch(this,b,model,rb))
				{
					jointMatched[rb] = true;
					jointMap[b] = rb;
					match = true;
				}
			}
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
	{
		for(int r = 0; r<rhsGCount; ++r)
		{
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
		}
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
			gtris = getUngroupedTriangles();
			rgtris = model->getUngroupedTriangles();
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

	// Compare skeletal animations. This assumes animations are in the
	// same order.
	Model::AnimationModeE mode = Model::ANIMMODE_SKELETAL;
	int acount = getAnimCount(mode);
	if(acount!=(int)model->getAnimCount(mode))
	{
		log_warning("animation count mismatch,lhs = %d,rhs = %d\n",
				acount,model->getAnimCount(mode));
		return false;
	}

	for(int a = 0; a<acount; ++a)
	{
		if(std::string(getAnimName(mode,a))
			  !=std::string(model->getAnimName(mode,a)))
		{
			log_warning("anim name mismatch on %d,lhs = %s,rhs = %s\n",
					a,getAnimName(mode,a),model->getAnimName(mode,a));
			return false;
		}
		if(getAnimFrameCount(mode,a)!=model->getAnimFrameCount(mode,a))
		{
			log_warning("animation frame count mismatch on %d,lhs = %d,rhs = %d\n",
					a,getAnimFrameCount(mode,a),model->getAnimFrameCount(mode,a));
			return false;
		}
		if(fabs(getAnimFPS(mode,a)-model->getAnimFPS(mode,a))
			 >tolerance)
		{
			//How to print?
			//log_warning("animation fps mismatch on %d,lhs = %d,rhs = %d\n",
			log_warning("animation fps mismatch on %d,lhs = %f,rhs = %f\n",
					a,model->getAnimFPS(mode,a),model->getAnimFPS(mode,a));
			return false;
		}

		int fcount = getAnimFrameCount(mode,a);
		
		//FIX ME //Why not propEqual?
		auto sa = m_skelAnims[a], sb = model->m_skelAnims[a]; //2020
		Matrix lhs_mat, rhs_mat;

		for(int f = 0; f<fcount; ++f)
		{
			auto time = sa->m_timetable2020[f];
			if(fabs(time-sb->m_timetable2020[f])>tolerance)
			{
				log_warning("animation timestamp mismatch on %d,lhs = %f,rhs = %f\n",
						a,time,sb->m_timetable2020[f]);
				return false;
			}
			for(Position b{PT_Joint,0};b<lhsBCount;b++)
			{
				Position rb{PT_Joint,jointMap[b]};
				
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
					log_warning("keyframe %d mismatch on anim %d for joint %d\n",
							f,a,b);
					return false;
				}
			}
		}
	}

	// Compare frame animations. This assumes animations are in the
	// same order.
	mode = Model::ANIMMODE_FRAME;
	acount = getAnimCount(mode);
	if(acount!=(int)model->getAnimCount(mode))
	{
		log_warning("frame animation count mismatch,lhs = %d,rhs = %d\n",
				acount,model->getAnimCount(mode));
		return false;
	}

	for(int a = 0; a<acount; ++a)
	{
		if(std::string(getAnimName(mode,a))
			  !=std::string(model->getAnimName(mode,a)))
		{
			log_warning("anim name mismatch on %d,lhs = %s,rhs = %s\n",
					a,getAnimName(mode,a),model->getAnimName(mode,a));
			return false;
		}
		if(getAnimFrameCount(mode,a)!=model->getAnimFrameCount(mode,a))
		{
			log_warning("animation frame count mismatch on %d,lhs = %d,rhs = %d\n",
					a,getAnimFrameCount(mode,a),model->getAnimFrameCount(mode,a));
			return false;
		}
		if(fabs(getAnimFPS(mode,a)-model->getAnimFPS(mode,a))
			 >tolerance)
		{
			log_warning("animation fps mismatch on %d,lhs = %d,rhs = %d\n",
					a,model->getAnimFPS(mode,a),model->getAnimFPS(mode,a));
			return false;
		}

		int fcount = getAnimFrameCount(mode,a);

		for(int f = 0; f<fcount; ++f)
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
	unsigned numFrameAnims  = m_frameAnims.size();
	unsigned numSkelAnims	= m_skelAnims.size();

	unsigned t = 0;
	unsigned v = 0;

	std::string dstr;

	if(partBits &(PartVertices|PartFrameAnims/*2020*/))
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

	if(partBits &PartFaces)
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

	if(partBits &PartGroups)
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

	if(partBits &PartJoints)
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

	if(partBits &PartPoints)
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

	if(partBits &(PartMaterials | PartTextures))
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

	if(partBits &PartProjections)
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

	if(partBits &PartSkelAnims)
	{
		if(numSkelAnims!=model->m_skelAnims.size())
		{
			log_warning("match failed at skel anim count %d!=%d\n",
					numSkelAnims,model->m_skelAnims.size());
			return false;
		}

		for(t = 0; t<numSkelAnims; t++)
		{
			if(!m_skelAnims[t]->propEqual(*model->m_skelAnims[t],propBits,tolerance))
			{
				log_warning("match failed at skel animation %d\n",t);
				return false;
			}
		}
	}

	if(partBits &PartFrameAnims)
	{
		if(numFrameAnims!=model->m_frameAnims.size())
		{
			log_warning("match failed at frameAnim count %d!=%d\n",
					numFrameAnims,model->m_frameAnims.size());
			return false;
		}

		for(t = 0; t<numFrameAnims; t++)
		{
			if(!m_frameAnims[t]->propEqual(*model->m_frameAnims[t],propBits,tolerance))
			{
				log_warning("match failed at frame animation %d\n",t);
				return false;
			}
		}
	}

	if(partBits &PartMeta)
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

	if(partBits &PartBackgrounds)
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
	
	void find(double cmp[3], double radius, std::vector<int> &out)const
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

Model::AnimBase2020 *Model::_dup(AnimationModeE mode, AnimBase2020 *a, bool keyframes)
{
	unsigned index = addAnimation(mode,a->m_name.c_str());
	setAnimFrameCount(mode,index,a->_frame_count());

	AnimBase2020 *b;
	if(mode==ANIMMODE_SKELETAL) b = m_skelAnims[index];
	else b = m_frameAnims[index];

	b->m_fps = a->m_fps;
	b->m_frame2020 = a->m_frame2020;
	b->m_timetable2020 = a->m_timetable2020;
	b->m_wrap = a->m_wrap;

	if(keyframes)
	{
		Position j{mode==ANIMMODE_SKELETAL?PT_Joint:PT_Point,0};

		for(;j<a->m_keyframes.size();j++)			
		for(auto*kf:a->m_keyframes[j])
		{
			setKeyframe(index,kf->m_frame,j,kf->m_isRotation,
			kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
		}
	}

	return b;
}
bool Model::mergeAnimations(Model *model)
{
	//if(m_animationMode) return false; //REMOVE ME

	unsigned count = model->getAnimCount(ANIMMODE_SKELETAL);

	if(!count&&!model->getAnimCount(ANIMMODE_FRAME))
	{
		//msg_warning(TRANSLATE("LowLevel","Model contains no skeletal animations"));
		msg_warning(TRANSLATE("LowLevel","Model contains no animations"));
		return false;
	}

	if(count)
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

	//	bool canAdd = canAddOrDelete();
	//	forceAddOrDelete(true);

	auto f = [&](AnimBase2020 *a, AnimationModeE mode)
	{
		unsigned index = addAnimation(mode,a->m_name.c_str());
		setAnimFrameCount(mode,index,a->_frame_count());

		AnimBase2020 *b;
		if(mode==ANIMMODE_SKELETAL) b = m_skelAnims[index];
		else b = m_frameAnims[index];

		b->m_fps = a->m_fps;
		b->m_frame2020 = a->m_frame2020;
		b->m_timetable2020 = a->m_timetable2020;

		auto pt = mode==ANIMMODE_SKELETAL?PT_Joint:PT_Point;

		//TODO: compare points/joints by position?
		for(unsigned j=0;j<a->m_keyframes.size();j++)			
		for(unsigned k=0;k<a->m_keyframes[j].size();k++)
		{
			Keyframe *kf = a->m_keyframes[j][k];
			setKeyframe(index,kf->m_frame,{pt,j},kf->m_isRotation,
			kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2]);
		}

		return b;
	};

	if(count) // Do skeletal add
	{
		for(unsigned n=0;n<count;n++)
		{
			_dup(ANIMMODE_SKELETAL,model->m_skelAnims[n]);
		}
	}

	count = model->getAnimCount(ANIMMODE_FRAME);

	if(!count&&!model->getAnimCount(ANIMMODE_SKELETAL))
	{
		//msg_warning(TRANSLATE("LowLevel","Model contains no skeletal animations"));
		msg_warning(TRANSLATE("LowLevel","Model contains no animations"));
		return false;
	}

	if(count) //2020
	{
		std::vector<std::pair<FrameAnim*,FrameAnim*>> ab;

		for(unsigned n=0;n<count;n++)
		{
			FrameAnim *a = model->m_frameAnims[n];
			AnimBase2020 *b = _dup(ANIMMODE_FRAME,a);
			ab.push_back({a,(FrameAnim*)b});
		}
		
		SpatialSort ss(*model);
		std::vector<int> match;

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
				int bp = ea.second->m_frame0;

				for(size_t i=ea.first->_frame_count();i-->0;)
				{
					*v->m_frames[bp++] = *w->m_frames[ap++];
				}
			}
		}
	}

//	invalidateNormals(); //???

//	forceAddOrDelete(canAdd&&m_frameAnims.empty());

	return true;
}

bool Model::mergeModels(const Model *model, bool textures, AnimationMergeE animations, bool emptyGroups)
{
//	if(m_animationMode) return false; //REMOVE ME

//	bool canAdd = canAddOrDelete();
//	forceAddOrDelete(true);

	std::unordered_map<int,int> groupMap;
	std::unordered_map<int,int> materialMap;
	std::unordered_set<int> materialsNeeded;

	unsigned vertbase = m_vertices.size();
	unsigned tribase = m_triangles.size();
	unsigned grpbase = m_groups.size();
	unsigned jointbase = m_joints.size();
	unsigned pointbase = m_points.size();
	unsigned projbase = m_projections.size();
	unsigned matbase = m_materials.size();

	unselectAll();

	unsigned n,count;
	count = model->m_vertices.size();
	for(n = 0; n<count; n++)
	{
		Vertex *v = model->m_vertices[n];		
		addVertex(v->m_coord[0],v->m_coord[1],v->m_coord[2]);
	}

	count = model->m_triangles.size();
	for(n = 0; n<count; n++)
	{
		Triangle *tri = model->m_triangles[n];
		addTriangle(tri->m_vertexIndices[0]+vertbase,
		tri->m_vertexIndices[1]+vertbase,tri->m_vertexIndices[2]+vertbase);
	}

	count = model->m_groups.size();
	for(n = 0; n<count; n++)
	{
		if(emptyGroups||!model->getGroupTriangles(n).empty())
		{
			const char *name = model->getGroupName(n);
			groupMap[n] = addGroup(name);
			uint8_t val = model->getGroupSmooth(n);
			setGroupSmooth(groupMap[n],val);
			int mat = model->getGroupTextureId(n);
			if(mat>=0) materialsNeeded.insert(mat);
		}
	}

	count = model->m_joints.size(); 
	if(count>0) model->validateSkel();
	for(n = 0; n<count; n++)
	{
		Joint *joint = model->m_joints[n];
			
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
	for(n = 0; n<count; n++)
	{
		Point *point = model->m_points[n];

		double *tran = point->m_abs, *rot = point->m_rot;
		int pnum = addPoint(point->m_name.c_str(),tran[0],tran[1],tran[2],rot[0],rot[1],rot[2]);

		memcpy(m_points[pnum]->m_xyz,point->m_xyz,3*sizeof(point->m_xyz));

		for(auto&ea:model->m_points[n]->m_influences)
		{
			addPointInfluence(pnum,ea.m_boneId+jointbase,ea.m_type,ea.m_weight);
		}
	}

	count = model->m_vertices.size();
	for(n = 0; n<count; n++)
	{
		for(auto&ea:model->m_vertices[n]->m_influences)
		{
			addVertexInfluence(n+vertbase,ea.m_boneId+jointbase,ea.m_type,ea.m_weight);
		}
	}

	if(textures)
	{
		TextureManager *texmgr = TextureManager::getInstance();

		count = model->getTextureCount();
		for(n = 0; n<count; n++)
		{
			if(materialsNeeded.find(n)!=materialsNeeded.end())
			{
				int newMat = 0;
				if(model->getMaterialType(n)==Model::Material::MATTYPE_TEXTURE)
				{
					const char *filename = model->getTextureFilename(n);
					Texture *newtex = texmgr->getTexture(filename);

					newMat = addTexture(newtex);

					const char *name = model->getTextureName(n);
					setTextureName(newMat,name);
				}
				else
				{
					const char *name = model->getTextureName(n);
					newMat = addColorMaterial(name);
				}
				materialMap[n] = newMat;

				float val[4] = { 0.0,0.0,0.0,0.0 };
				float shin = 0.0;

				model->getTextureAmbient( n,val);
				setTextureAmbient( newMat,val);
				model->getTextureDiffuse( n,val);
				setTextureDiffuse( newMat,val);
				model->getTextureEmissive(n,val);
				setTextureEmissive(newMat,val);
				model->getTextureSpecular(n,val);
				setTextureSpecular(newMat,val);

				model->getTextureShininess(n,shin);
				setTextureShininess(newMat,shin);
			}
		}

		count = model->m_groups.size();
		for(n = 0; n<count; n++)
		{
			if(groupMap.find(n)!=groupMap.end())
			{
				int oldMat = model->getGroupTextureId(n);
				setGroupTextureId(groupMap[n],materialMap[oldMat]);
			}
		}

		count = model->getProjectionCount();
		for(n = 0; n<count; n++)
		{
			const char *name = model->getProjectionName(n);
			int type = model->getProjectionType(n);

			double pos[3];
			double rot[3];
			double scale;
			double range[2][2];

			model->getProjectionCoords(n,pos);
			model->getProjectionRotation(n,rot);
			scale = model->getProjectionScale(n);
			model->getProjectionRange(n,range[0][0],range[0][1],range[1][0],range[1][1]);

			addProjection(name,type,pos[0],pos[1],pos[2]);
			setProjectionRotation(n+projbase,rot);
			setProjectionScale(n+projbase,scale);
			setProjectionRange(n+projbase,range[0][0],range[0][1],range[1][0],range[1][1]);
		}

		int tpcount = getProjectionCount();

		count = model->getTriangleCount();
		float s = 0.0;
		float t = 0.0;
		for(n = 0; n<count; n++)
		{
			for(unsigned i = 0; i<3; i++)
			{
				model->getTextureCoords(n,i,s,t);
				setTextureCoords(n+tribase,i,s,t);
			}

			int grp = model->getTriangleGroup(n);
			if(grp>=0)
			{
				addTriangleToGroup(groupMap[grp],n+tribase);
			}

			int prj = model->getTriangleProjection(n);
			if(prj>=0&&(prj+(int)projbase)<tpcount)
			{
				setTriangleProjection(n+tribase,prj+projbase);
			}
		}
	}

	if(animations!=AM_NONE)
	{
		// Do frame merge if possible
		unsigned ac1 = getAnimCount(ANIMMODE_FRAME);
		unsigned ac2 = model->getAnimCount(ANIMMODE_FRAME);

		bool merge = animations==AM_MERGE&&ac1==ac2;		
		// Have to check frame counts too
		for(unsigned a=0;merge&&a<ac1;a++)
		{
			unsigned fc1 = getAnimFrameCount(ANIMMODE_FRAME,a);
			unsigned fc2 = model->getAnimFrameCount(ANIMMODE_FRAME,a);

			if(fc1!=fc2) merge = false;
		}

		// Do frame add otherwise
		unsigned base = merge?0:ac1;
		for(unsigned a=0;a<ac2;a++)
		{			
			FrameAnim *fa = model->m_frameAnims[a];
			
			if(!merge) _dup(ANIMMODE_FRAME,fa,false);								

			for(unsigned j=0;j<fa->m_keyframes.size();j++)
			for(Keyframe *kf:fa->m_keyframes[j])
			{
				setKeyframe(base+a,kf->m_frame,{Model::PT_Point,j+pointbase},
				kf->m_isRotation,kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
			}

			//Note, setFrameAnimVertexCoords is unnecessary 
			//because this is a fresh animation
			auto fp = fa->m_frame0;
			auto fd = m_frameAnims[base+a]->m_frame0;
			auto fc = fa->_frame_count();
			for(size_t v=model->m_vertices.size();v-->0;)
			{
				//Note, it's easier on the cache to do
				//the vertices in the outer loop

				auto p = &model->m_vertices[v]->m_frames[fp];
				auto d = &m_vertices[vertbase+v]->m_frames[fd];

				for(unsigned c=fc;c-->0;d++,p++) **d = **p;
			}			
		}

		// Do skeletal merge if possible
		ac1 = getAnimCount(ANIMMODE_SKELETAL);
		ac2 = model->getAnimCount(ANIMMODE_SKELETAL);

		merge = ac1==ac2&&animations==AM_MERGE;
		// Have to check frame counts too
		for(unsigned a=0;merge&&a<ac1;a++)
		{
			unsigned fc1 = getAnimFrameCount(ANIMMODE_SKELETAL,a);
			unsigned fc2 = model->getAnimFrameCount(ANIMMODE_SKELETAL,a);

			if(fc1!=fc2) merge = false;
		}

		base = merge?0:ac1;
		for(unsigned a=0;a<ac2;a++)
		{	
			SkelAnim *sa = model->m_skelAnims[a];

			if(!merge) _dup(ANIMMODE_SKELETAL,sa,false);

			for(unsigned j=0;j<sa->m_keyframes.size();j++)
			for(Keyframe *kf:sa->m_keyframes[j])
			{
				setKeyframe(base+a,kf->m_frame,{Model::PT_Joint,j+jointbase},
				kf->m_isRotation,kf->m_parameter[0],kf->m_parameter[1],kf->m_parameter[2],kf->m_interp2020);
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

//	forceAddOrDelete(canAdd&&m_frameAnims.empty());

	return true;
}

const int SE_POLY_MAX = 2;
struct model_ops_SimplifyEdgeT
{
	unsigned int vFar;
	
	int polyCount;
	int poly[SE_POLY_MAX];
	double normal[SE_POLY_MAX][3]; //float
};
typedef std::list<model_ops_SimplifyEdgeT> model_ops_SimplifyEdgeList;

void Model::simplifySelectedMesh()
{
	validateNormals(); //2020

	// for each vertex
	//	find all edges
	//	if Va to V is same vector as V to Vb
	//	  make sure every *other* edge has exactly two co-planar faces
	//	  if so
	//		 move V to Va and weld
	//		 re-do loop at 

	unsigned int vcount = m_vertices.size();
	unsigned int v = 0;

	for(v = 0; v<vcount; v++)
	{
		m_vertices[v]->m_marked = false;
	}

	int tcount = m_triangles.size();
	int t = 0;

	for(t = 0; t<tcount; t++)
	{
		m_triangles[t]->m_marked = false;
	}

	model_ops_SimplifyEdgeList edges;
	model_ops_SimplifyEdgeList::iterator it;
	model_ops_SimplifyEdgeList::iterator itA;
	model_ops_SimplifyEdgeList::iterator itB;
	unsigned int verts[3];
	int idx[3];

	double coords[3];
	double tcoords[3][3];
	double vecA[3];
	double vecB[3];

	bool welded = false;
	bool valid  = true;

	v = 0;
	while(v<vcount)
	{
		log_debug("checking vertex %d\n",v);
		welded = false;

		//if(!m_vertices[v]->m_marked)
		{
			valid = true; // valid weld candidate until we learn otherwise
			edges.clear();

			// build edge list
			for(t = 0; valid&&t<tcount; t++)
			{
				// unflattened triangles only
				if(m_triangles[t]->m_selected)//&&!m_triangles[t]->m_marked)
				{
					getTriangleVertices(t,verts[0],verts[1],verts[2]);
					idx[0] = -1;

					if(verts[0]==v)
					{
						idx[0] = 0;
						idx[1] = 1;
						idx[2] = 2;
					}
					if(verts[1]==v)
					{
						idx[0] = 1;
						idx[1] = 0;
						idx[2] = 2;
					}
					if(verts[2]==v)
					{
						idx[0] = 2;
						idx[1] = 0;
						idx[2] = 1;
					}

					// If triangle is using v as a vertex, add to edge list
					if(idx[0]>=0)
					{
						log_debug("  triangle %d uses vertex %d\n",t,v);
						// vert[idx[1]] and vert[idx[2]] are the opposite vertices
						for(int i = 1; i<=2; i++)
						{
							bool newEdge = true;
							for(it = edges.begin(); valid&&it!=edges.end(); it++)
							{
								if((*it).vFar==verts[idx[i]])
								{
									if((*it).polyCount<SE_POLY_MAX)
									{
										(*it).poly[(*it).polyCount] = t;
										getFlatNormalUnanimated(t,(*it).normal[(*it).polyCount]);

										(*it).polyCount++;
										newEdge = false;

										log_debug("  adding polygon to edge for %d\n",
												(*it).vFar);
										break;
									}
									else
									{
										// more than two faces on this edge
										// we can't weld at all,skip this vertex
										log_debug("  too many polygons connected to edge to %d\n",
												(*it).vFar);
										valid = false;
									}
								}
							}

							if(valid&&newEdge)
							{
								log_debug("  adding new edge for polygon for %d\n",
										idx[i]);
								model_ops_SimplifyEdgeT se;
								se.vFar = verts[idx[i]];
								se.polyCount = 1;
								se.poly[0] = t;
								getFlatNormalUnanimated(t,se.normal[0]);

								edges.push_back(se);
							}
						}
					}
				}
			}

			if(valid)
			{
				// use vectors from two edges to see if they are in a straight line
				getVertexCoords(v,coords);

				for(itA = edges.begin(); valid&&!welded&&itA!=edges.end(); itA++)
				{
					getVertexCoords((*itA).vFar,vecA);
					vecA[0] = coords[0]-vecA[0];
					vecA[1] = coords[1]-vecA[1];
					vecA[2] = coords[2]-vecA[2];
					normalize3(vecA);

					for(itB = edges.begin(); valid&&!welded&&itB!=edges.end(); itB++)
					{
						if(itA!=itB)
						{
							bool canWeld = true;

							getVertexCoords((*itB).vFar,vecB);
							vecB[0] -= coords[0];
							vecB[1] -= coords[1];
							vecB[2] -= coords[2];
							normalize3(vecB);

							if(equiv3(vecA,vecB))
							{
								log_debug("  found a straight line\n");
								for(it = edges.begin(); it!=edges.end(); it++)
								{
									if(it!=itA&&it!=itB)
									{
										// must have a face on each side of edge
										if((*it).polyCount!=2)
										{
											log_debug("	 not enough polygons connected to edge\n");
											canWeld = false;
										}

										// faces must be in the same plane
										if(canWeld&&!equiv3((*it).normal[0],(*it).normal[1]))
										{
											log_debug("	 polygons on edge do not face the same direction\n");
											canWeld = false;
										}

										// check inverted normals
										for(int i = 0; i<(*it).polyCount; i++)
										{
											getTriangleVertices((*it).poly[i],
													verts[0],verts[1],verts[2]);

											bool flat = false;

											for(int n = 0; n<3; n++)
											{
												if(verts[n]==v)
												{
													verts[n] = (*itA).vFar;
												}
												else if(verts[n]==(*itA).vFar)
												{
													flat = true;
												}
											}

											if(!flat)
											{
												getVertexCoords(verts[0],tcoords[0]);
												getVertexCoords(verts[1],tcoords[1]);
												getVertexCoords(verts[2],tcoords[2]);

												double norm[3];
												calculate_normal(norm,tcoords[0],tcoords[1],tcoords[2]);

												log_debug("-- %f,%f,%f  %f,%f,%f\n",
														norm[0],norm[1],norm[2],
														(*it).normal[i][0],(*it).normal[i][1],(*it).normal[i][2]);
												if(!equiv3(norm,(*it).normal[i]))
												{
													log_debug("normal gets inverted on collapse,skipping\n");
													canWeld = false;
												}
											}
										}
									}
								}

								if(canWeld)
								{
									// Yay!We can collapse v to va (itA)

									log_debug("*** vertex %d can be collapsed to %d\n",
											v,(*itA).vFar);
									for(it = edges.begin(); it!=edges.end(); it++)
									{
										// move v to va on each edge,mark flattened triangles
										for(int i = 0; i<(*it).polyCount; i++)
										{
											getTriangleVertices((*it).poly[i],
													verts[0],verts[1],verts[2]);

											log_debug("finding %d in triangle %d\n",v,(*it).poly[i]);
											for(int n = 0; n<3; n++)
											{
												if(verts[n]==v)
												{
													log_debug(" vertex %d\n",n);
													verts[n] = (*itA).vFar;
													// don't break,we want to check for va also
												}
												else if(verts[n]==(*itA).vFar)
												{
													log_debug(" triangle %d is flattened\n",(*it).poly[i]);
													// v and va are now the same
													// mark the triangle as flattened
													m_triangles[(*it).poly[i]]->m_marked = true;
												}
											}

											setTriangleVertices((*it).poly[i],
													verts[0],verts[1],verts[2]);
										}
									}

									welded = true;

									// v is now an orphan,next vertex
									v++;
									v = 0; // TODO let's start completely over for now
									deleteFlattenedTriangles();
									deleteOrphanedVertices();
									vcount = m_vertices.size();
									tcount = m_triangles.size();

									if((*itA).vFar<v)
									{
										// The edges connected to va have changed
										// we must back up to re-check that vertex
										v = (*itA).vFar;
									}
								}
							}
						}
					}
				}
			}
		}

		if(!welded)
		{
			v++;
		}
	}

}

#endif // MM3D_EDIT
