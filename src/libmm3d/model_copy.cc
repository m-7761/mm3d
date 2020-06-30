/*  MM3D Misfit/Maverick Model 3D
 *
 * Copyright (c)2004-2008 Kevin Worcester
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

//#include <ext/hash_map>
//using __gnu_cxx::hash_map;
//template<class S, class T>
//using hash_map = std::unordered_map<S,T>;
typedef std::unordered_map<int,int> model_copy_int_map;

#include "model.h"

#include "glmath.h"
#include "log.h"
#include "texmgr.h"

Model *Model::copySelected()const
{
	Model *m = new Model;

	int_list tri;
	getSelectedTriangles(tri);
	int_list joints;
	getSelectedBoneJoints(joints);
	int_list points;
	getSelectedPoints(points);
	int_list proj;
	getSelectedProjections(proj);

	//REMOVE ME
	//I think vector works here. hash_map
	//probably messes up the order anyway.
//	model_copy_int_map projMap; //UNUSED???
	model_copy_int_map vertMap;
	model_copy_int_map triMap;
	model_copy_int_map jointMap;
	model_copy_int_map pointMap;

	int_list::iterator lit;

	if(!proj.empty())
	{
		for(lit = proj.begin(); lit!=proj.end(); ++lit)
		{
			const char *name = getProjectionName(*lit);
			int type = getProjectionType(*lit);

			double pos[3];
			double rot[3];
			double scale;
			double range[2][2];

			getProjectionCoords(*lit,pos);
			getProjectionRotation(*lit,rot);
			scale = getProjectionScale(*lit);
			getProjectionRange(*lit,range[0][0],range[0][1],range[1][0],range[1][1]);

			int np = m->addProjection(name,type,pos[0],pos[1],pos[2]);
			m->setProjectionRotation(np,rot);
			m->setProjectionScale(np,scale);
			m->setProjectionRange(np,range[0][0],range[0][1],range[1][0],range[1][1]);

			m->selectProjection(np);

//			projMap[*lit] = np; //UNUSED???

			//FIX ME: Assign copied faces?!
		}
	}

	if(!tri.empty())
	{
		int_list vert;
		getSelectedVertices(vert);

		// Copy vertices
		log_debug("Copying %d vertices\n",vert.size());
		for(lit = vert.begin(); lit!=vert.end(); lit++)
		{
			double coords[3];
			getVertexCoords(*lit,coords);
			int nv = m->addVertex(coords[0],coords[1],coords[2]);
			m->selectVertex(nv);
			m->setVertexFree(nv,isVertexFree(*lit));

			vertMap[*lit] = nv;
		}

		// Copy faces
		log_debug("Copying %d faces\n",tri.size());
		for(lit = tri.begin(); lit!=tri.end(); lit++)
		{
			unsigned v[3];

			for(int t = 0; t<3; t++)
			{
				v[t] = getTriangleVertex(*lit,t);
			}
			int nt = m->addTriangle(vertMap[v[0]] ,vertMap[v[1]],vertMap[v[2]]);
			m->selectTriangle(nt);

			triMap[*lit] = nt;
		}

		// Copy texture coords
		log_debug("Copying %d face texture coordinates\n",tri.size());
		for(lit = tri.begin(); lit!=tri.end(); lit++)
		{
			float s;
			float t;

			for(unsigned i = 0; i<3; i++)
			{
				getTextureCoords((unsigned)*lit,i,s,t);
				m->setTextureCoords((unsigned)triMap[*lit],i,s,t);
			}
		}

		// TODO only copy textures used by groups?
		// Copy textures
		// It's easier here to just copy the groups and textures 
		// even if not needed,the user can delete the unecessary parts
		unsigned tcount = getTextureCount();
		log_debug("Copying %d textures\n",tcount);
		for(unsigned t = 0; t<tcount; t++)
		{
			switch (getMaterialType(t))
			{
				case Model::Material::MATTYPE_TEXTURE:
					{
						Texture *tex = TextureManager::getInstance()->getTexture(getTextureFilename(t));
						int num = m->addTexture(tex);
						m->setTextureName(num,getTextureName(t));
					}
					break;
				default:
					log_error("Unknown material type %d in duplicate\n",getMaterialType(t));
				case Model::Material::MATTYPE_BLANK:
					m->addColorMaterial(getTextureName(t));
					break;
			}

			float c[4];
			getTextureAmbient(t,c);
			m->setTextureAmbient(t,c);
			getTextureDiffuse(t,c);
			m->setTextureDiffuse(t,c);
			getTextureSpecular(t,c);
			m->setTextureSpecular(t,c);
			getTextureEmissive(t,c);
			m->setTextureEmissive(t,c);
			getTextureShininess(t,c[0]);
			m->setTextureShininess(t,c[0]);

			// TODO Material m_color (if ever used)

			m->setTextureSClamp(t,getTextureSClamp(t));
			m->setTextureTClamp(t,getTextureTClamp(t));
		}

		// TODO Only copy selected groups?
		// Copy groups
		// It's easier here to just copy the groups and textures 
		// even if not needed,the user can delete the unecessary parts
		unsigned gcount = getGroupCount();
		log_debug("Copying %d groups\n",gcount);
		for(unsigned g = 0; g<gcount; g++)
		{
			m->addGroup(getGroupName(g));
			m->setGroupSmooth(g,getGroupSmooth(g));
			m->setGroupAngle(g,getGroupAngle(g));
			m->setGroupTextureId(g,getGroupTextureId(g));
		}

		if(gcount>0)
		{
			// Set groups
			log_debug("Setting %d triangle groups\n",tri.size());
			for(lit = tri.begin(); lit!=tri.end(); lit++)
			{
				// This works,even if triangle group==-1
				int gid = getTriangleGroup(*lit);
				if(gid>=0)
				{
					m->addTriangleToGroup(gid,triMap[*lit]);
				}
			}
		}

	}

	if(!points.empty())
	{
		// Copy points
		log_debug("Copying %d points\n",points.size());
		for(lit = points.begin(); lit!=points.end(); lit++)
		{
			double coord[3],rot[3],xyz[3];
			getPointCoordsUnanimated(*lit,coord);
			getPointRotationUnanimated(*lit,rot);
			getPointScaleUnanimated(*lit,xyz);

			int np = m->addPoint(getPointName(*lit),
			coord[0],coord[1],coord[2],rot[0],rot[1],rot[2]);
			memcpy(m->m_points[np]->m_xyz,xyz,sizeof(xyz));
			pointMap[*lit] = np;

			m->selectPoint(np);
		}
	}

	if(!joints.empty())
	{
		// Copy joints
		log_debug("Copying %d joints\n",joints.size());
		for(lit = joints.begin(); lit!=joints.end(); lit++)
		{
			int parent = getBoneJointParent(*lit);

			// TODO this will not work if parent joint comes after child
			// joint.  That shouldn't happen... but...
			if(isBoneJointSelected(parent))
			{
				parent = jointMap[parent];
			}
			else
			{
				if(m->getBoneJointCount()>0)
					parent = 0;
				else
					parent = -1;
			}

			//TODO: Why not just copy the matrix by value?
			double coord[3],rot[3],xyz[3];
			getBoneJointCoordsUnanimated(*lit,coord);
			getBoneJointRotationUnanimated(*lit,rot); //2020
			getBoneJointScaleUnanimated(*lit,xyz);

			int nj = m->addBoneJoint(getBoneJointName(*lit),
			coord[0],coord[1],coord[2]/*,rot[0],rot[1],rot[2]*/,parent);
			jointMap[*lit] = nj;
			memcpy(m->m_joints[nj]->m_rot,rot,sizeof(rot));
			memcpy(m->m_joints[nj]->m_xyz,xyz,sizeof(xyz));
			m->selectBoneJoint(nj);
		}

		for(model_copy_int_map::iterator it = vertMap.begin();
				it!=vertMap.end(); ++it)
		{
			//infl_list il;
			//getVertexInfluences(it->first,il);
			const infl_list &il = getVertexInfluences(it->first);

			for(infl_list::const_iterator iit = il.begin(); iit!=il.end(); ++iit)
			{
				if(isBoneJointSelected(iit->m_boneId))
				{
					m->addVertexInfluence(it->second,jointMap[iit->m_boneId],
							iit->m_type,iit->m_weight);
				}
			}
		}

		for(model_copy_int_map::iterator it = pointMap.begin();
				it!=pointMap.end(); ++it)
		{
			//infl_list il;
			//getPointInfluences(it->first,il);
			const infl_list &il = getPointInfluences(it->first);

			for(infl_list::const_iterator iit = il.begin(); iit!=il.end(); ++iit)
			{
				if(isBoneJointSelected(iit->m_boneId))
				{
					m->addPointInfluence(it->second,jointMap[iit->m_boneId],
							iit->m_type,iit->m_weight);
				}
			}
		}
	}

	// TODO what about animations?

	m->calculateSkel(); m->calculateNormals();

	return m;
}

bool Model::duplicateSelected()
{
	//2020: this code comes from the body of dupcmd.cc
	
	//2020: I added this code to phase these containers out but most
	//of them are still required
	//std::unordered_map<int,int> vertMap,triMap,jointMap,pointMap;
	std::unordered_map<int,int> vertMap,triMap,jointMap;
	unsigned vertbase = m_vertices.size();
	unsigned tribase = m_triangles.size();
	unsigned jointbase = m_joints.size();
	unsigned pointbase = m_points.size();
	//projbase = m_projections.size(); //IMPLEMENT ME

	//if(!tri.empty())
	{
		//model_status(model,StatusNormal,STATUSTIME_SHORT,
		//TRANSLATE("Command","Selected primitives duplicated"));
		
		// Duplicated vertices
		//log_debug("Duplicating %d vertices\n",vert.size());
		for(unsigned v=0;v<vertbase;v++)
		{
			if(!m_vertices[v]->m_selected) continue;

			double coords[3];
			getVertexCoords(v,coords);
			int nv = addVertex(coords[0],coords[1],coords[2]);

			if(isVertexFree(v)) setVertexFree(nv,true);

			vertMap[v] = nv;
		}

		// Duplicate faces
		//log_debug("Duplicating %d faces\n",tri.size());
		for(unsigned it=0;it<tribase;it++)
		{
			if(!m_triangles[it]->m_selected) continue;

			unsigned v[3];
			getTriangleVertices(it,v[0],v[1],v[2]);
			int nt = addTriangle(vertMap[v[0]],vertMap[v[1]],vertMap[v[2]]);

			triMap[it] = nt;
		//}

		// Duplicate texture coords
		//log_debug("Duplicating %d face texture coordinates\n",tri.size());
		//for(lit = tri.begin(); lit!=tri.end(); lit++)
		//{	
			for(unsigned i=0;i<3;i++)
			{
				float s,t;
				getTextureCoords(it,i,s,t);
				setTextureCoords(nt,i,s,t); //triMap[it]
			}
		}

		if(getGroupCount())
		{
			// Set groups
			//log_debug("Setting %d triangle groups\n",tri.size());
			for(unsigned t=0;t<tribase;t++)
			{
				if(!m_triangles[t]->m_selected) continue;

				//FIX ME: getTriangleGroup is dumb!!
				//FIX ME: getTriangleGroup is dumb!!
				//FIX ME: getTriangleGroup is dumb!!
				//FIX ME: getTriangleGroup is dumb!!

				// This works,even if triangle group==-1
				int gid = getTriangleGroup(t);
				if(gid>=0)
				{
					addTriangleToGroup(gid,triMap[t]);
				}
			}
		}

	}

	//if(!joints.empty())
	{
		int_list vertlist;

		// Duplicated joints
		//log_debug("Duplicating %d joints\n",joints.size());
		for(unsigned j=0;j<jointbase;j++)
		{
			if(!m_joints[j]->m_selected) continue;

			int parent = getBoneJointParent(j);

			// TODO this will not work if parent joint comes after child
			// joint.  That shouldn't happen... but...
			if(isBoneJointSelected(parent))
			{
				parent = jointMap[parent];
			}

			// If joint is root joint,assign duplicated joint to be child
			// of original
			if(parent==-1)
			{
				parent = 0;
			}

			double coord[3],rot[3],xyz[3];
			//double rot[3] = { 0,0,0 }; //UNUSED
			getBoneJointCoords(j,coord);
			getBoneJointRotationUnanimated(j,rot); //2020
			getBoneJointScaleUnanimated(j,xyz);

			int nj = addBoneJoint(getBoneJointName(j),
			coord[0],coord[1],coord[2]/*,rot[0],rot[1],rot[2]*/,parent);
			memcpy(m_joints[nj]->m_rot,rot,sizeof(rot));
			memcpy(m_joints[nj]->m_xyz,xyz,sizeof(xyz));
			jointMap[j] = nj;

			//FIX ME
			// Assign duplicated vertices to duplicated bone joints			
			getBoneJointVertices(j,vertlist);
			int_list::iterator vit;
			for(vit = vertlist.begin(); vit!=vertlist.end(); vit++)
			{
				if(isVertexSelected(*vit))
				{
					//LOOKS HIGHLY PROBLEMATIC
					//LOOKS HIGHLY PROBLEMATIC
					//LOOKS HIGHLY PROBLEMATIC
					setVertexBoneJoint(vertMap[*vit],nj);
				}
			}
		}
	}

	//if(!points.empty())
	{
		// Duplicated points
		//log_debug("Duplicating %d points\n",points.size());
		for(unsigned p=0;p<pointbase;p++)
		{
			if(!m_points[p]->m_selected) continue;
		
			/*2020: addPoint ignored this parameter so
			//I removed it.
			int parent = getPrimaryPointInfluence(p);
			if(isBoneJointSelected(parent))
			{
				parent = jointMap[parent];
			}*/

			double coord[3],rot[3],xyz[3];
			getPointCoordsUnanimated(p,coord);
			getPointRotationUnanimated(p,rot);
			getPointScaleUnanimated(p,rot);

			int np = addPoint(getPointName(p),
			coord[0],coord[1],coord[2],rot[0],rot[1],rot[2]/*,parent*/);
			memcpy(m_points[np]->m_xyz,xyz,sizeof(xyz));
			//pointMap[p] = np;
		}
	}

	if(vertMap.empty()&&triMap.empty()
	&&jointMap.empty()&&pointbase==getPointCount())
	{
		return false;
	}

	unselectAll();
	
	//2020: I'm quickly copying this from mergeModels 
	//to remove old map containers
	int n = getTriangleCount();
	while(n-->tribase) selectTriangle(n);
	n = getVertexCount();
	while(n-->vertbase) selectVertex(n);
	n = getBoneJointCount();
	while(n-->jointbase) selectBoneJoint(n);
	n = getPointCount();
	while(n-->pointbase) selectPoint(n);
	//n=getProjectionCount();
	//while(n-->projbase) selectProjection(n); //IMPLEMENTE ME

	calculateSkel(); calculateNormals(); return true;
}