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
#include "weld.h"

const double TOLERANCE = 0.00005;
const double ATOLERANCE = 0.00000001;

static bool model_bool_coord_equiv(double ac[3], double bc[3])
{
	return distance(ac[0],ac[1],ac[2],bc[0],bc[1],bc[2])<TOLERANCE;
}

struct model_bool_UnionTriangle
{
	int tri;
	double norm[3];
	double denorm;
	unsigned int v[3];
	double coord[3][3];
	double center[3];
	double tpoint[3];
	double tnorm[3][3];
};
//typedef std::list<model_bool_UnionTriangle> model_bool_UnionTriangleList;
typedef std::vector<model_bool_UnionTriangle> model_bool_UnionTriangleList;

enum IntersectionCheckE
{
	IC_TryAgain, // Swap A and B and try again
	IC_No,		 // No intersection found
	IC_Yes,		// Intersection found
};

// This function returns the angle on pivot from point p1 to p2
static double model_bool_angleToPoint(double pivot[3], double p1[3], double p2[3])
{
	double vec1[3] = { p1[0]-pivot[0],p1[1]-pivot[1],p1[2]-pivot[2] };
	double vec2[3] = { p2[0]-pivot[0],p2[1]-pivot[1],p2[2]-pivot[2] };

	normalize3(vec1); normalize3(vec2);

	double d = dot3(vec1,vec2); return d>(1.0-ATOLERANCE)?0.0:acos(d);
}

// returns:
//	 1 = in front
//	 0 = on plane
//	-1 = in back
static int model_bool_pointInPlane(double coord[3], double triCoord[3], double triNorm[3])
{
	double btoa[3] = { coord[0]-triCoord[0],coord[1]-triCoord[1],coord[2]-triCoord[2] };

	normalize3(btoa);

	double c = dot3(btoa,triNorm); return c<-ATOLERANCE?-1:c>ATOLERANCE?1:0;
}

// returns:
//	 1 = inside triangle
//	 0 = on triangle edge
//	-1 = outside triangle
static int model_bool_pointInTriangle(double coord[3], model_bool_UnionTriangle &tri)
{
	int vside[3]; for(int i=0;i<3;i++)
	{
		vside[i] = model_bool_pointInPlane(coord,tri.tpoint,tri.tnorm[i]);
	}
	if(vside[0]<0&&vside[1]<0&&vside[2]<0) return 1;
	if(vside[0]>0&&vside[1]>0&&vside[2]>0) return 1;
	if(vside[0]==0&&vside[1]==vside[2]) return 0;
	if(vside[1]==0&&vside[0]==vside[2]) return 0;
	if(vside[2]==0&&vside[0]==vside[1]) return 0;
	if(vside[0]==0&&vside[1]==0) return 0;
	if(vside[0]==0&&vside[2]==0) return 0;
	if(vside[1]==0&&vside[2]==0) return 0; return -1;
}

//2022: I'm pulling this out of findEdgePlaneIntersection
//to reuse to interpolate UV coordinates
static double model_bool_findEdgePlaneDistance
(double p1[3], double p2_p1[3], double triCoord[3], double triNorm[3])
{
	double a =	dot3(p2_p1,triNorm), d = -dot3(triCoord,triNorm);

	// prevent divide by zero
	// edge is parallel to plane, the value of ipoint is undefined
	if(fabs(a)<TOLERANCE) return 0;

	// distance along edgeVec (p2_p1) to plane
	return -(d+dot3(p1,triNorm))/a;
}

// returns:
//	 ipoint = coordinates of edge line and plane
//	 bool	= whether or not ipoint is between p1 and p2
static bool model_bool_findEdgePlaneIntersection
(double ipoint[3], double p1[3], double p2[3], double triCoord[3], double triNorm[3], bool checkRange=true)
{
	double edgeVec[3] = { p2[0]-p1[0],p2[1]-p1[1],p2[2]-p1[2] };

	//log_debug("finding intersection with plane with line from %f,%f,%f to %f,%f,%f\n",p1[0],p1[1],p1[2],p2[0],p2[1],p2[2]);

	//WARNING: This isn't "dist" because edgeVec isn't normalized
	//That means TOLERANCE below isn't an absolute distance either
	//double dist = model_bool_findEdgePlaneDistance(p1,edgeVec,triCoord,triNorm);
	double t = model_bool_findEdgePlaneDistance(p1,edgeVec,triCoord,triNorm);

	// edge is parallel to plane, the value of ipoint is undefined
	if(0==t) return false;

	// t is%of velocity vector, scale to get impact point
	for(int i=3;i-->0;)
	{
		ipoint[i] = p1[i]+t*edgeVec[i];
	}

	if(checkRange)
	{
		// Make sure intersection point is between p1 and p2
		return t>=(0.0-TOLERANCE)&&t<=(1.0+TOLERANCE);
	}
	else
	{
		// Distance is irrelevant,we just want to know where 
		// the intersection is (as long as it's not behind the triangle)
		return t>=(0.0-TOLERANCE);
	}
}

static void model_bool_initUnionTriangle(Model *model, model_bool_UnionTriangle &ut, int triIndex)
{
	ut.tri = triIndex;
	model->getTriangleVertices(triIndex,ut.v[0],ut.v[1],ut.v[2]);

	for(int i=3;i-->0;) model->getVertexCoords(ut.v[i],ut.coord[i]);	

	ut.denorm = calculate_normal(ut.norm,ut.coord[0],ut.coord[1],ut.coord[2]);

	// Find center point and tent point (for inside-triangle test)
	for(int i=0;i<3;i++)
	{
		ut.center[i] = (ut.coord[0][i]+ut.coord[1][i]+ut.coord[2][i])/3.0;
		ut.tpoint[i] = ut.center[i]+ut.norm[i];
	}

	// Find normals for tent triangles (for inside-triangle test)
	calculate_normal(ut.tnorm[0],ut.tpoint,ut.coord[0],ut.coord[1]);
	calculate_normal(ut.tnorm[1],ut.tpoint,ut.coord[1],ut.coord[2]);
	calculate_normal(ut.tnorm[2],ut.tpoint,ut.coord[2],ut.coord[0]);
}

static bool model_bool_isValidTriangle(Model *model, int v1, int v2, int v3)
{
	double coord[3][3];
	model->getVertexCoords(v1,coord[0]);
	model->getVertexCoords(v2,coord[1]);
	model->getVertexCoords(v3,coord[2]);

	if(model_bool_coord_equiv(coord[0],coord[1])) return false;
	if(model_bool_coord_equiv(coord[0],coord[2])) return false;
	if(model_bool_coord_equiv(coord[1],coord[2])) return false;

	double aval1 = fabs(model_bool_angleToPoint(coord[0],coord[1],coord[2]));
	double aval2 = fabs(model_bool_angleToPoint(coord[1],coord[0],coord[2]));
	double aval3 = fabs(model_bool_angleToPoint(coord[2],coord[0],coord[1]));

	if(aval1<TOLERANCE&&aval2<TOLERANCE) return false;
	if(aval1<TOLERANCE&&aval3<TOLERANCE) return false;
	if(aval2<TOLERANCE&&aval3<TOLERANCE) return false; return true;
}

struct model_bool_NewTriangle{ int v[3]; };
typedef std::vector<model_bool_NewTriangle> model_bool_NewTriangleList;

static void model_bool_cutTriangle(Model *model,
	model_bool_UnionTriangleList &utl,
	model_bool_UnionTriangle &ut,
//	model_bool_UnionTriangle &utCut, //UNUSED
	double p1[3], double p2[3])
{
	//log_debug("cutting triangle %d\n",ut.tri);

	/*UNUSED
	int onVertex = 0;
	for(int n=0;n<3;n++)	
	if(model_bool_coord_equiv(ut.coord[n],p1)
	 ||model_bool_coord_equiv(ut.coord[n],p2))
	{
		onVertex++;
	}
	//log_debug("onVertex = %d\n",onVertex);*/

	double a1 = fabs(model_bool_angleToPoint(ut.coord[0],p1,ut.coord[1]));
	double a2 = fabs(model_bool_angleToPoint(ut.coord[0],p2,ut.coord[1]));

	if(a1==0&&a2==0)
	{
		// If we're a perfectly straight line from v0 to v1 then
		// our intersection line is on our edge. No need to cut.
		a1 = a2; return;
	}

	if(model_bool_coord_equiv(ut.coord[0],p1)) a1 = 0.0;
	if(model_bool_coord_equiv(ut.coord[0],p2)) a2 = 0.0;

	auto nc1 = p1,nc2 = p2; if(a1>=a2) std::swap(nc1,nc2);

	int v1 = model->addVertex(ut.v[0],nc1);
	int v2 = model->addVertex(ut.v[1],nc2);
	unsigned int v3[3] = { ut.v[0],ut.v[1],ut.v[2] }; //2022

	model_bool_NewTriangle nt;
	model_bool_NewTriangleList ntl;

	nt.v[0] = v3[0];
	nt.v[1] = v1;
	nt.v[2] = v2;
	ntl.push_back(nt);
	
	nt.v[0] = v3[0];
	nt.v[1] = v3[1];
	nt.v[2] = v1;
	ntl.push_back(nt);
	
	nt.v[0] = v3[0];
	nt.v[1] = v2;
	nt.v[2] = v3[2];
	ntl.push_back(nt);

	double newnorm[3];
	calculate_normal(newnorm,ut.coord[2],nc2,nc1);	
	if(dot3(ut.norm,newnorm)>0.0)
	{
		nt.v[0] = v3[2];
		nt.v[1] = v2;
		nt.v[2] = v1;
		ntl.push_back(nt);

		nt.v[0] = v3[2];
		nt.v[1] = v1;
		nt.v[2] = v3[1];
		ntl.push_back(nt);
	}
	else
	{
		nt.v[0] = v3[1];
		nt.v[1] = v2;
		nt.v[2] = v1;
		ntl.push_back(nt);

		// reversed v[1] and v[2] (appears to behave correctly now)
		nt.v[0] = v3[2];
		nt.v[1] = v2;
		nt.v[2] = v3[1];
		ntl.push_back(nt);
	}

	//2022: This is new fix up code for texture mapping.
	//I couldn't find a way to make it work without the
	//normal vector not being normalized, so the 
	//https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/barycentric-coordinates
	float st3[2][3];
	model->getTextureCoords(ut.tri,st3);
	float st1[2],st2[2];
	{
		auto &v0 = *(Vector*)ut.coord[0];
		auto &v1 = *(Vector*)ut.coord[1]; 
		auto &v2 = *(Vector*)ut.coord[2];

		Vector edge1 = v2-v1, edge2 = v0-v2;
				
		//auto &N = *(Vector*)ut.norm;
		double denorm = ut.denorm;
		double N[3]; //Un-normalize N?
		for(int i=3;i-->0;) N[i] = ut.norm[i]*denorm;
		//Revert saved length to N.dot(N) and prefer 
		//reciprocal for optimal division down below.
		denorm = 1/(denorm*denorm);

		for(int i=2;i-->0;)
		{	
			auto &P = *(Vector*)(i?nc2:nc1);
			
			//NOTE: normalize3 could be skipped if N wasn't
			//itself normalized... in which case u/v need to
			//be divided by N.dot3(N).
			double u = denorm*dot3(N,edge1.cross3(P-v1).getVector());
			double v = denorm*dot3(N,edge2.cross3(P-v2).getVector());
			double w = 1-u-v;

			float *st = i?st2:st1; for(int j=2;j-->0;)
			{
				st[j] = (float)(u*st3[j][0]+v*st3[j][1]+w*st3[j][2]);
			}			
		}
	}

	bool setUt = true;

	int group = model->getTriangleGroup(ut.tri);

	// now add the remaining triangles to the list
	for(auto&ea:ntl)	
	if(model_bool_isValidTriangle(model,ea.v[0],ea.v[1],ea.v[2]))
	{
		int tri; if(setUt)
		{
			tri = ut.tri; setUt = false;

			model->setTriangleVertices(tri,ea.v[0],ea.v[1],ea.v[2]);			
			model_bool_initUnionTriangle(model,ut,tri);
		}
		else
		{
			tri = model->addTriangle(ea.v[0],ea.v[1],ea.v[2]);
			if(group>=0) model->addTriangleToGroup(group,tri);

			model_bool_UnionTriangle nut;
			model_bool_initUnionTriangle(model,nut,tri);
			utl.push_back(nut);
		}

		float st[2][3]; for(int i=3;i-->0;) //2022
		{
			float &s = st[0][i], &t = st[1][i]; 
			int cmp = ea.v[i];
			if(cmp==v1){ s = st1[0], t = st1[1]; }
			if(cmp==v2){ s = st2[0], t = st2[1]; }
			if(cmp==v3[0]){ s = st3[0][0], t = st3[1][0]; }
			if(cmp==v3[1]){ s = st3[0][1], t = st3[1][1]; }
			if(cmp==v3[2]){ s = st3[0][2], t = st3[1][2]; }
		}
		model->setTextureCoords(tri,st);
	}

	if(setUt) log_error("No valid triangles in any splits\n");
}

static IntersectionCheckE model_bool_findIntersection(Model *model,
model_bool_UnionTriangleList &la, model_bool_UnionTriangleList &lb,
model_bool_UnionTriangle &a ,model_bool_UnionTriangle &b)
{
	//log_debug("checking %d against %d\n",a.tri,b.tri);

	//for(int i=0;i<3;i++)
	//log_debug("  point %d: %f,%f,%f\n",a.tri,a.coord[i][0],a.coord[i][1],a.coord[i][2]);
	//for(int i=0;i<3;i++) 
	//log_debug("  point %d: %f,%f,%f\n",b.tri,b.coord[i][0],b.coord[i][1],b.coord[i][2]);
	
	int sharedVertices = 0;

	// If the triangles share a vertex or if the vertices
	// are at the same point in space,they are considered "shared"
	for(int i=0;i<3;i++)
	for(int j=0;j<3;j++)
	if(a.v[i]==b.v[j]||model_bool_coord_equiv(a.coord[i],b.coord[j]))
	{
		sharedVertices++;
	}
	// triangles completely overlap each other
	// no splits required
	if(sharedVertices==3) return IC_No;
			
	int coPlanar = 0;
	int vside[3]; // on which side of B is each A vertex?

	bool inFront = false, inBack = false;

	// Calculate which side of B each A vertex is on
	for(int i=0;i<3;i++)
	{
		double btoa[3];
		for(int j=0;j<3;j++)
		btoa[j] = a.coord[i][j]-b.coord[i][j];
		normalize3(btoa);

		double c = dot3(btoa,b.norm);
		if(c<-TOLERANCE)
		{
			inBack = true; vside[i] = -1;
		}
		else if(c>TOLERANCE)
		{
			inFront = true; vside[i] = 1;
		}
		else
		{
			coPlanar++; vside[i] = 0;
		}
	}

	//log_debug("  shared=%d coPlanar=%d front=%s back=%s\n",sharedVertices,coPlanar,(inFront?"yes":"no"),(inBack?"yes":"no"));

	// triangles share an edge but are not co-planar
	// no splits required
	if(sharedVertices==2&&(inFront||inBack))
	{		
		return IC_No;
	}
	// Are A and B co-planar?
	if(coPlanar==3)
	{
		// co-planar faces should only matter
		// if other non-coplanar faces intersect along the same edge
		// (for face removal purposes)

		// NOTE: My testing seems to confirm this,but there may be
		// cases where this is not true. If co-planar faces are not
		// getting split correctly this case may be the cause.

		// They are on the same plane but do not overlap
		// (but B may be inside A, swap and try again)
		//return IC_TryAgain;

		//TODO
		//https://github.com/zturtleman/mm3d/issues/177
		//I believe this should be implemented to enable
		//stenciling style editing.
		//If so I think it requires 3 cuts instead of 2
		//that should cut out the triangle from inside of
		//the outer triangle.

		return IC_No;
	}
	// Does A go through B's plane?
	// (consider co-planar to go "through")
	if(coPlanar==2||(inFront&&inBack))
	{
		// Yes, does A go through B itself?

		int oddSide = 0; // default to 0, correct below
		int far1 = 1;
		int far2 = 2;
		if(vside[1]!=0&&vside[1]!=vside[0]&&vside[1]!=vside[2])
		{
			oddSide = 1; far1 = 0; far2 = 2;
		}
		else if(vside[2]!=0&&vside[2]!=vside[0]&&vside[2]!=vside[1])
		{
			oddSide = 2; far1 = 0; far2 = 1;
		}

		double ipoint1[3],ipoint2[3];
		bool hit1 = model_bool_findEdgePlaneIntersection
		(ipoint1,a.coord[oddSide],a.coord[far1],b.coord[0],b.norm);
		bool hit2 = model_bool_findEdgePlaneIntersection
		(ipoint2,a.coord[oddSide],a.coord[far2],b.coord[0],b.norm);

		// hit1 and hit2 indicate that the edge crosses the plane
		// the follow up is to see if the intersection is within the triangle
		// On the triangle's edge counts as "inside"
		if(hit1)
		{
			hit1 = (model_bool_pointInTriangle(ipoint1,b)>=0);
			//log_debug("  intersects plane at %f,%f,%f (%s)\n",ipoint1[0],ipoint1[1],ipoint1[2],(hit1 ? "yes" : "no"));
		}
		if(hit2)
		{
			hit2 = (model_bool_pointInTriangle(ipoint2,b)>=0);
			//log_debug("  intersects plane at %f,%f,%f (%s)\n",ipoint2[0],ipoint2[1],ipoint2[2],(hit2 ? "yes" : "no"));
		}

		if(!hit1&&!hit2)
		{
			// No, A edges go through B's plane, but not B itself,
			// but B edges may go through A, swap and try again
			return IC_TryAgain;
		}
		
		if(!hit1||!hit2)
		{	
			//log_debug("  one hit\n");
			
			// If the only intersection was on the plane's edge,
			// don't count it as an intersection			
			if(0==model_bool_pointInTriangle(hit1?ipoint1:ipoint2,b))
			{
				// One intersection,on edge,bail
				return IC_TryAgain;
			}			

			// find point where edge leaves B and change ipoint to that point
			double epoint[3];			
			for(int i=0;i<3;i++)
			if(model_bool_findEdgePlaneIntersection
			(epoint,ipoint1,ipoint2,b.tpoint,b.tnorm[i]))
			break;
			memcpy(hit2?ipoint1:ipoint2,epoint,sizeof(epoint));
		}

		//NOTE: a can't be passed to the second call because the
		//std::vector's memory is invalidated (it was std::list).
		model_bool_cutTriangle(model,la,a,/*b,*/ipoint1,ipoint2);
		model_bool_cutTriangle(model,lb,b,/*a,*/ipoint1,ipoint2);

		return IC_Yes;
	}

	return IC_No; // No, A is entirely on one side of B's plane
}

static void model_bool_build_UnionTriangleList
(Model *model, model_bool_UnionTriangleList &buildList, int_list &sourceList)
{
	buildList.clear();

	// Creation union triangle lists for intersection tests
	for(int i:sourceList)
	{
		model_bool_UnionTriangle ut;
		model_bool_initUnionTriangle(model,ut,i);
		buildList.push_back(ut);
	}
}

static void model_bool_findNearTriangles
(Model *model, model_bool_UnionTriangle &ut, model_bool_UnionTriangleList &lb, int &coplanar, int &front)
{
	coplanar = -1, front = -1;

	double dist = 0, ipoint[3];

	auto *save_it = 0?lb.data():0;

	for(auto&b:lb)	
	if(model_bool_findEdgePlaneIntersection(ipoint,ut.center,ut.tpoint,b.center,b.norm,false))
	{
		//log_debug("found line/plane intersection\n");

		if(model_bool_pointInTriangle(ipoint,b)>=0)
		{
			//log_debug("  intersection is in far triangle\n");

			if(model_bool_pointInPlane(ipoint,ut.center,ut.norm)==0)
			{
				//log_debug("	 intersection is in self (co planar)\n");

				coplanar = b.tri;
			}
			else
			{
				double d = mag3(ipoint);
				if(front<0||d<dist)
				{
					//log_debug("	 found new nearest front triangle\n");
					front = b.tri;
					dist = d;
					save_it = &b;
				}
			}
		}
	}

	if(front>=0)
	{
		if(model_bool_pointInPlane(ut.center,save_it->center,save_it->norm)>0)
		{
			//log_debug("	 front triangle faces us,we're outside\n");

			// our triangle is in front of an outward facing triangle,
			// so it is not inside another shape
			front = -1;
		}
	}
}

static void model_bool_removeInternalTriangles
(Model *model, model_bool_UnionTriangleList &la, model_bool_UnionTriangleList &lb)
{
	int_list removeList,removeList2;

	int coplanar,front; for(auto&a:la)
	{
		model_bool_findNearTriangles(model,a,lb,coplanar,front);

		if(front>=0)
		{
			//log_debug("A triangle %d is on the inside\n",a.tri);
			removeList.push_back(a.tri);
		}
	}

	for(auto&b:lb)
	{
		model_bool_findNearTriangles(model,b,la,coplanar,front);

		if(front>=0||coplanar>=0)
		{
			//log_debug("B triangle %d is on the inside\n",b.tri);
			removeList2.push_back(b.tri);
		}
	}

	// Must delete in reverse order because deleting a triangle invalidates
	// any index that comes after it.
	for(int a,b;!removeList.empty()||!removeList2.empty();)
	{
		a = b = -1; //does a/b mean the same here???

		if(!removeList.empty()) a = removeList.back();
		if(!removeList2.empty()) b = removeList2.back();
		
		if(a>b)
		{
			model->deleteTriangle(a); removeList.pop_back();
		}
		else
		{
			model->deleteTriangle(b); removeList2.pop_back();
		}
	}

	model->deleteOrphanedVertices();
}

static void model_bool_removeExternalTriangles
(Model *model, model_bool_UnionTriangleList &la, model_bool_UnionTriangleList &lb)
{
	int_list removeList,removeList2;

	int coplanar,front; for(auto&a:la)
	{
		model_bool_findNearTriangles(model,a,lb,coplanar,front);

		if(front<0)
		{
			//log_debug("A triangle %d is on the inside\n",a.tri);
			removeList.push_back(a.tri);
		}
	}
	for(auto&b:lb)
	{
		model_bool_findNearTriangles(model,b,la,coplanar,front);

		if(front<0||coplanar>=0)
		{
			//log_debug("B triangle %d is on the inside\n",b.tri);
			removeList2.push_back(b.tri);
		}
	}

	// Must delete in reverse order because deleting a triangle invalidates
	// any index that comes after it.
	for(int a,b;!removeList.empty()||!removeList2.empty();)
	{
		a = b = -1; //does a/b mean the same here???

		if(!removeList.empty()) a = removeList.back();
		if(!removeList2.empty()) b = removeList2.back();
		
		if(a>b)
		{
			model->deleteTriangle(a); removeList.pop_back();
		}
		else
		{
			model->deleteTriangle(b); removeList2.pop_back();
		}
	}

	model->deleteOrphanedVertices();
}

static void model_bool_removeSubtractionTriangles
(Model *model, model_bool_UnionTriangleList &la, model_bool_UnionTriangleList &lb)
{
	int_list invertList,removeList,removeList2;

	int coplanar,front; for(auto&b:lb)
	{
		model_bool_findNearTriangles(model,b,la,coplanar,front);

		if(coplanar>=0)
		{
			removeList.push_back(b.tri);
		}
		else if(front>=0)
		{
			//log_debug("B triangle %d is on the inside\n",b.tri);
			invertList.push_back(b.tri);
		}
		else
		{
			//log_debug("B triangle %d is on the outside\n",b.tri);
			removeList.push_back(b.tri);
		}
	}
	for(auto&a:la)
	{
		model_bool_findNearTriangles(model,a,lb,coplanar,front);

		if(coplanar>=0&&front<0||coplanar<0&&front>=0)
		{
			//log_debug("A triangle %d is on the inside\n",a.tri);
			removeList2.push_back(a.tri);
		}
	}
	
	model->invertNormals(invertList);

	// Must delete in reverse order because deleting a triangle invalidates
	// any index that comes after it.
	for(int a,b;!removeList.empty()||!removeList2.empty();)
	{
		a = b = -1; //does a/b mean the same here???

		if(!removeList.empty()) a = removeList.back();
		if(!removeList2.empty()) b = removeList2.back();
		
		if(a>b)
		{
			model->deleteTriangle(a); removeList.pop_back();
		}
		else
		{
			model->deleteTriangle(b); removeList2.pop_back();
		}
	}

	model->deleteOrphanedVertices();
}

void Model::booleanOperation(Model::BooleanOpE op, int_list &listA, int_list &listB)
{
	model_bool_UnionTriangleList la,lb;

	model_bool_build_UnionTriangleList(this,la,listA);
	model_bool_build_UnionTriangleList(this,lb,listB);

	//NOTE: These lists are growing.
	for(size_t i=0;i<la.size();i++)
	for(size_t j=0;j<lb.size();j++)
	{
		auto &a = la[i], &b = lb[j];
		if(IC_TryAgain==model_bool_findIntersection(this,la,lb,a,b))
		{
			model_bool_findIntersection(this,lb,la,b,a);
		}	
	}

	for(auto&a:la) selectTriangle(a.tri);
	for(auto&b:lb) selectTriangle(b.tri);

	weldSelectedVertices(this);

	switch(op)
	{
	case BO_UnionRemove:
		model_bool_removeInternalTriangles(this,la,lb);
		break;
	case BO_Subtraction:
		model_bool_removeSubtractionTriangles(this,la,lb);
		break;
	case BO_Intersection:
		model_bool_removeExternalTriangles(this,la,lb);
		break;
	default:
		log_error("Uknown boolean op: %d\n",op);
	case BO_Union:
		break;
	}
}

