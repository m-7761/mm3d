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
#include "log.h"
#include "mm3dport.h"

#ifdef MM3D_EDIT
#include "modelundo.h"
#endif // MM3D_EDIT

#ifdef MM3D_EDIT

int Model::addGroup(const char *name)
{
	return addGroup(-1,name);
}
int Model::addGroup(int cp, const char *name)
{
	//LOG_PROFILE(); //???

	Group *p; if(cp>=0) //2022
	{
		if((unsigned)cp>=m_groups.size()) 
		{
			assert(0); return -1;
		}
		else p = m_groups[cp];

		if(!name) name = p->m_name.c_str();
	}
	else p = nullptr;

	m_changeBits |= AddOther; 
	
	//2021: preventing surprise (MM3D)
	//doesn't allow setting names to a
	//blank but filters can.
	//https://github.com/zturtleman/mm3d/issues/158
	if(!name[0]) name = "_";
	
	int num = m_groups.size();

	Group *group = Group::get();
	group->m_name = name; if(p)
	{
		group->m_materialIndex = p->m_materialIndex;
		group->m_smooth = p->m_smooth;
		group->m_angle = p->m_angle;
	}
	m_groups.push_back(group);

	Undo<MU_Add>(this,num,group);

	return num;
}

void Model::deleteGroup(unsigned groupNum)
{
	//LOG_PROFILE(); //???

	Undo<MU_Delete>(this,groupNum,m_groups[groupNum]);	

	removeGroup(groupNum);
}

bool Model::setGroupSmooth(unsigned groupNum, uint8_t smooth)
{
	if(groupNum>=m_groups.size()) return false;
	
	if(smooth!=m_groups[groupNum]->m_smooth) //2020
	{
		Undo<MU_SetGroupSmooth>(this,
		groupNum,smooth,m_groups[groupNum]->m_smooth);

		m_groups[groupNum]->m_smooth = smooth;
		
		invalidateNormals(); //OVERKILL
	}
	return true;
}

bool Model::setGroupAngle(unsigned groupNum, uint8_t angle)
{
	if(groupNum>=m_groups.size()) return false;
	
	if(angle!=m_groups[groupNum]->m_angle) //2020
	{
		Undo<MU_SetGroupAngle>(this,
		groupNum,angle,m_groups[groupNum]->m_angle);

		m_groups[groupNum]->m_angle = angle;
		
		invalidateNormals(); //OVERKILL
	}
	return true;
}

bool Model::setGroupName(unsigned groupNum, const char *name)
{
	if(groupNum>=0&&groupNum<m_groups.size()&&name&&name[0])
	{
		auto *gp = m_groups[groupNum];

		if(gp->m_name!=name)
		{
			m_changeBits|=AddOther; //2020

			Undo<MU_SwapStableStr>(this,AddOther,gp->m_name);

			gp->m_name = name;
		}
		return true;
	}
	return false;
}

void Model::setSelectedAsGroup(unsigned groupNum)
{
	//LOG_PROFILE(); //???

	if(groupNum<m_groups.size())
	{
		Group *gp = m_groups[groupNum];
		while(!gp->m_triangleIndices.empty()) //OVERKILL
		{
			//removeTriangleFromGroup(groupNum,gp->m_triangleIndices.front());
			removeTriangleFromGroup(groupNum,gp->m_triangleIndices.back());
		}

		// Put selected triangles into group groupNum
		for(unsigned t=0;t<m_triangles.size();t++)
		{
			auto *tp = m_triangles[t];
			if(tp->m_selected)
			{
				if(tp->m_group>=0)
				removeTriangleFromGroup(tp->m_group,t);
				addTriangleToGroup(groupNum,t);
			}
		}
	}
}

void Model::addSelectedToGroup(unsigned groupNum)
{
	//LOG_PROFILE(); //???

	if(groupNum<m_groups.size())	
	for(unsigned t=0;t<m_triangles.size();t++)		
	{
		auto *tp = m_triangles[t];
		if(tp->m_selected)
		{
			if(tp->m_group>=0)
			removeTriangleFromGroup(tp->m_group,t);
			addTriangleToGroup(groupNum,t);
		}	
	}
}

bool Model::addTriangleToGroup(unsigned groupNum, unsigned triangleNum)
{
	//LOG_PROFILE(); //???

	if(groupNum<m_groups.size()&&triangleNum<m_triangles.size())
	{
		auto *tp = m_triangles[triangleNum]; //2022
		int og = tp->m_group;
		if(og==-1) tp->m_group = groupNum;
		else return false;
		
		auto &c = m_groups[groupNum]->m_triangleIndices;
		if(!c.empty()&&(unsigned)c.back()>triangleNum)
		{
			//c.insert(triangleNum);
			auto it = std::lower_bound(c.begin(),c.end(),(int)triangleNum);
			if(it==c.end()||*it!=triangleNum)
			c.insert(it,triangleNum);
		}
		else c.push_back(triangleNum);

		Undo<MU_AddToGroup>(this,triangleNum,groupNum,og);

		m_changeBits |= SetGroup; //SetTexture?

		invalidateNormals();
		invalidateBspTree(); return true;
	}
	else
	{
		log_error("addTriangleToGroup(%d,%d) argument out of range\n",
				groupNum,triangleNum);
	}

	return false;
}

bool Model::ungroupTriangle(unsigned triangleNum)
{
	if(triangleNum>=m_triangles.size()) 
	return false;
	auto *tp = m_triangles[triangleNum];
	if(tp->m_group>=0)
	return removeTriangleFromGroup(tp->m_group,triangleNum);
	return true;
}
bool Model::removeTriangleFromGroup(unsigned groupNum, unsigned triangleNum)
{
	if(groupNum<m_groups.size()&&triangleNum<m_triangles.size())
	{	
		auto *tp = m_triangles[triangleNum]; //2022
		int og = tp->m_group;
		if(og!=groupNum) return false;

		auto &c = m_groups[groupNum]->m_triangleIndices;
		if(c.empty()){ assert(0); return false; }

		tp->m_group = -1;
				
		auto it = c.end()-1;
		if(*it!=triangleNum)
		{
			//it = c.find(triangleNum);
			it = std::find(c.begin(),c.end(),triangleNum);
		}
		if(it!=c.end())
		{
			c.erase(it);

			Undo<MU_AddToGroup>(this,triangleNum,-1,og);
		}

		m_changeBits |= SetGroup; //SetTexture?

		invalidateNormals();
		invalidateBspTree(); return true;
	}
	else
	{
		log_error("addTriangleToGroup(%d,%d)argument out of range\n",
				groupNum,triangleNum);
	}

	return false;
}

#endif // MM3D_EDIT

void Model::getUngroupedTriangles(int_list &l)const
{
	l.clear(); int i = 0;

	for(auto*tp:m_triangles)
	{
		if(-1==tp->m_group) l.push_back(i); i++;
	}
}

//int_list Model::getGroupTriangles(unsigned groupNumber)const
const int_list &Model::getGroupTriangles(unsigned groupNumber)const
{
	/*int_list triangles;
	if(groupNumber<m_groups.size())	
	for(auto i:m_groups[groupNumber]->m_triangleIndices)
	{
		triangles.push_back(i);
	}
	return triangles;*/
	if(groupNumber<m_groups.size())
	{
		return m_groups[groupNumber]->m_triangleIndices;
	}
	static const int_list e; return e;
}

int Model::getTriangleGroup(unsigned triangleNumber)const
{
	/*2022: A little better...
	for(unsigned g=0;g<m_groups.size();g++)
	{
		auto &c = m_groups[g]->m_triangleIndices;
		if(c.end()!=std::find(c.begin(),c.end(),triangleNumber))
		return g;
	}*/
	if(triangleNumber<m_triangles.size())
	{
		return m_triangles[triangleNumber]->m_group;
	}
	return -1; // Triangle is not in a group
}

const char *Model::getGroupName(unsigned groupNum)const
{
	if(groupNum>=0&&groupNum<m_groups.size())
	return m_groups[groupNum]->m_name.c_str();
	return nullptr;
}

int Model::getGroupByName(const char *const groupName,bool ignoreCase)const
{
	auto compare = ignoreCase?PORT_strcasecmp:strcmp;

	int groupNumber = -1;

	for(unsigned g=0;g<m_groups.size();g++)	
	if(!compare(groupName,m_groups[g]->m_name.c_str()))
	{
		groupNumber = g; break;
	}	

	return groupNumber;
}

uint8_t Model::getGroupSmooth(unsigned groupNum)const
{
	return groupNum<m_groups.size()?m_groups[groupNum]->m_smooth:0;
}

uint8_t Model::getGroupAngle(unsigned groupNum)const
{
	return groupNum<m_groups.size()?m_groups[groupNum]->m_angle:180;
}

int Model::removeUnusedGroups()
{
	int removed = 0;
	for(int g=m_groups.size();g-->0;)	
	if(m_groups[g]->m_triangleIndices.empty())
	{
		removed++; deleteGroup(g);
	}
	return removed;
}

int Model::mergeIdenticalGroups()
{
	int merged = 0;
	std::unordered_set<int> toRemove;
	int groupCount = m_groups.size();
	for(int g=0;g<groupCount;g++)	
	if(toRemove.find(g)==toRemove.end())		
	for(int g2=g+1;g2<groupCount;g2++)
	{
		Group *grp2 = m_groups[g2];
	//	if(m_groups[g]->propEqual(*grp2,~Model::PropTriangles))
		if(m_groups[g]->propEqual(*grp2,~Model::PropTriangles|Model::PropAllSuitable))
		{
			for(int i:grp2->m_triangleIndices)
			{
				addTriangleToGroup(g,i);
			}
			toRemove.insert(g2);

			merged++;
		}
	}

	for(int g=groupCount;g-->0;)	
	if(toRemove.find(g)!=toRemove.end())
	{
		deleteGroup(g);
	}

	return merged;
}

int Model::removeUnusedMaterials()
{
	int removed = 0;
	std::unordered_set<int> inUse;
	for(int g=m_groups.size();g-->0;)
	{
		int mat = m_groups[g]->m_materialIndex;
		if(mat>=0)
		{
			inUse.insert(mat);
		}
	}
	for(int m=m_materials.size();m-->0;)
	{
		if(inUse.find(m)==inUse.end())
		{
			removed++;

			deleteTexture(m);
		}
	}
	return removed;
}

int Model::mergeIdenticalMaterials()
{
	int merged = 0;

	std::unordered_set<int> toRemove;
	int matCount = m_materials.size();
	for(int m=0;m<matCount;m++)	
	if(toRemove.find(m)==toRemove.end())
	for(int m2=m+1;m2<matCount;m2++)	
	if(m_materials[m]->propEqual(*m_materials[m2]))
	{
		for(unsigned g=0;g<m_groups.size();g++)
		if(m2==m_groups[g]->m_materialIndex)
		{
			setGroupTextureId(g,m);
		}

		toRemove.insert(m2);

		merged++;
	}

	for(int m=matCount;m-->0;)	
	if(toRemove.find(m)!=toRemove.end())
	{
		deleteTexture(m);
	}

	return merged;
}

