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

#include "texmgr.h"
#include "texture.h"
#include "log.h"

#ifdef MM3D_EDIT
#include "modelstatus.h"
#include "undomgr.h"
#include "modelundo.h"
#endif // MM3D_EDIT

#include "translate.h"

#include "mm3dport.h"

#ifdef MM3D_EDIT

struct SplitEdgesT
{
	unsigned a;
	unsigned b;
	unsigned vNew;
	bool operator<(const struct SplitEdgesT &rhs)const
	{
		return (this->a<rhs.a 
			  ||(this->a==rhs.a&&this->b<rhs.b));
	};
	bool operator== (const struct SplitEdgesT &rhs)const
	{
		return (this->a==rhs.a&&this->b==rhs.b);
	};
};

#endif // MM3D_EDIT

/*REFERENCE
// used to calculate smoothed normals in calculateNormals
struct NormAccum
{
	NormAccum(){ memset(norm,0,sizeof(norm)); }
	//float norm[3];
	double norm[3];
};
// used to calculate smoothed normals in calculateNormals
struct NormAngleAccum : NormAccum
{
	//float angle; //UNUSED (https://github.com/zturtleman/mm3d/issues/109)
//	double angle;
};*/

std::string Model::s_lastFilterError = "No error";
static int model_allocated = 0;

const double TOLERANCE = 0.00005;
const double ATOLERANCE = 0.00000001;

// returns:
//	 1 = in front
//	 0 = on plane
//	-1 = in back
static int model_pointInPlane(double *coord, double *triCoord, double *triNorm)
{
	double btoa[3];

	for(int j = 0; j<3; j++)
	{
		btoa[j] = coord[j]-triCoord[j];
	}
	normalize3(btoa);

	double c = dot3(btoa,triNorm);

	if(c<-ATOLERANCE)
		return -1;
	else if(c>ATOLERANCE)
		return 1;
	else
		return 0;
}

// NOTE: Assumes coord is in plane of p1,p2,p3
//
// returns:
//	 1 = inside triangle
//	 0 = on triangle edge
//	-1 = outside triangle
static int model_pointInTriangle(double *coord,
		double *p1,
		double *p2,
		double *p3)
{
	int vside[3];

	int i;

	double avg[3];
	double normal[3];

	for(i = 0; i<3; i++)
	{
		avg[i] = (p1[i]+p2[i]+p3[i])/3.0;
	}

	calculate_normal(normal,
			p1,p2,p3);

	for(i = 0; i<3; i++)
	{
		avg[i] += normal[i];
	}

	calculate_normal(normal,
			p1,p2,avg);
	vside[0] = model_pointInPlane(coord,avg,normal);
	calculate_normal(normal,
			p2,p3,avg);
	vside[1] = model_pointInPlane(coord,avg,normal);
	calculate_normal(normal,
			p3,p1,avg);
	vside[2] = model_pointInPlane(coord,avg,normal);

	if(vside[0]<0&&vside[1]<0&&vside[2]<0)
	{
		return 1;
	}
	if(vside[0]>0&&vside[1]>0&&vside[2]>0)
	{
		return 1;
	}
	if(vside[0]==0&&vside[1]==vside[2])
	{
		return 0;
	}
	if(vside[1]==0&&vside[0]==vside[2])
	{
		return 0;
	}
	if(vside[2]==0&&vside[0]==vside[1])
	{
		return 0;
	}
	if(vside[0]==0&&vside[1]==0)
	{
		return 0;
	}
	if(vside[0]==0&&vside[2]==0)
	{
		return 0;
	}
	if(vside[1]==0&&vside[2]==0)
	{
		return 0;
	}
	return -1;
}

// returns:
//	 ipoint = coordinates of edge line and plane
//	 int	 = 0 is no intersection,+1 is toward p2 from p1,-1 is away from p2 from p1
static int model_findEdgePlaneIntersection(double *ipoint, double *p1, double *p2,
		double *triCoord, double *triNorm)
{
	double edgeVec[3];
	edgeVec[0] = p2[0]-p1[0];
	edgeVec[1] = p2[1]-p1[1];
	edgeVec[2] = p2[2]-p1[2];

	/*
	log_debug("finding intersection with plane with line from %f,%f,%f to %f,%f,%f\n",
			p1[0],p1[1],p1[2],
			p2[0],p2[1],p2[2]);
	*/

	double a =	dot3(edgeVec,triNorm);
	double d =-dot3(triCoord,triNorm);

	// prevent divide by zero
	if(fabs(a)<TOLERANCE)
	{
		// edge is parallel to plane,the value of ipoint is undefined
		return 0;
	}

	// distance along edgeVec p1 to plane
	double dist = -(d+dot3(p1,triNorm))/a;

	// dist is%of velocity vector,scale to get impact point
	edgeVec[0] *= dist;
	edgeVec[1] *= dist;
	edgeVec[2] *= dist;

	ipoint[0] = p1[0]+edgeVec[0];
	ipoint[1] = p1[1]+edgeVec[1];
	ipoint[2] = p1[2]+edgeVec[2];

	// Distance is irrelevant,we just want to know where 
	// the intersection is
	if(dist>=0.0)
	{
		return 1;
	}
	else
	{
		return -1;
	}
}

Model::Model()
	: m_filename(""),
	m_validContext(false),
	  m_validBspTree(false),
	  m_canvasDrawMode(0),
	  m_perspectiveDrawMode(3),
	  m_drawOptions(DO_BONES), //2019
	  m_drawJoints(/*JOINTMODE_BONES*/true),
	  m_drawSelection(),
	  m_drawProjections(true),
	  m_validNormals(false),
	  m_validAnim(false),
	  m_validAnimNormals(false),
	  m_validJoints(false),
	m_validAnimJoints(false),
	  //m_forceAddOrDelete(false),
	  m_animationMode(ANIMMODE_NONE),
	  m_currentFrame(0),
	  m_currentAnim(0),
	  m_currentTime(0.0)
#ifdef MM3D_EDIT
	,
	  m_saved(true),
	  m_selectionMode(SelectVertices),
	  m_selecting(false),
	  m_changeBits(ChangeAll),
	  m_undoEnabled(false)
#endif // MM3D_EDIT
{
	model_allocated++;

	m_localMatrix.loadIdentity();

#ifdef MM3D_EDIT
	//NOTE: The Object system wants std::vector
	m_background.resize(MAX_BACKGROUND_IMAGES);
	for(auto&ea:m_background)
	ea = new BackgroundImage();

	m_undoMgr	  = new UndoManager();
	//m_animUndoMgr = new UndoManager();
#endif // MM3D_EDIT

}

Model::~Model()
{
	m_bspTree.clear();
	m_selectedUv.clear();

	log_debug("deleting model\n");
	DrawingContextList::iterator it;
	for(it = m_drawingContexts.begin(); it!=m_drawingContexts.end(); it++)
	{
		deleteGlTextures((*it)->m_context);
	}
	m_drawingContexts.clear();

	while(!m_vertices.empty())
	{
		m_vertices.back()->release();
		m_vertices.pop_back();
	}

	while(!m_triangles.empty())
	{
		m_triangles.back()->release();
		m_triangles.pop_back();
	}

	while(!m_groups.empty())
	{
		m_groups.back()->release();
		m_groups.pop_back();
	}

	while(!m_materials.empty())
	{
		m_materials.back()->release();
		m_materials.pop_back();
	}

	while(!m_skelAnims.empty())
	{
		m_skelAnims.back()->release();
		m_skelAnims.pop_back();
	}

	while(!m_joints.empty())
	{
		m_joints.back()->release();
		m_joints.pop_back();
	}

	while(!m_points.empty())
	{
		m_points.back()->release();
		m_points.pop_back();
	}

	while(!m_projections.empty())
	{
		m_projections.back()->release();
		m_projections.pop_back();
	}

	while(!m_frameAnims.empty())
	{
		m_frameAnims.back()->release();
		m_frameAnims.pop_back();
	}

#ifdef MM3D_EDIT
	for(unsigned t = 0; t<6; t++)
	{
		delete m_background[t];
		m_background[t] = nullptr;
	}

	delete m_undoMgr;
	//delete m_animUndoMgr;
#endif // MM3D_EDIT

	model_allocated--;
}

const char *Model::errorToString(Model::ModelErrorE e,Model *model)
{
	switch (e)
	{
		case ERROR_NONE:
			return TRANSLATE_NOOP("LowLevel","Success");
		case ERROR_CANCEL:
			return TRANSLATE_NOOP("LowLevel","Canceled");
		case ERROR_UNKNOWN_TYPE:
			return TRANSLATE_NOOP("LowLevel","Unrecognized file extension (unknown type)");
		case ERROR_UNSUPPORTED_OPERATION:
			return TRANSLATE_NOOP("LowLevel","Operation not supported for this file type");
		case ERROR_BAD_ARGUMENT:
			return TRANSLATE_NOOP("LowLevel","Invalid argument (internal error,probably null pointer argument)");
		case ERROR_NO_FILE:
			return TRANSLATE_NOOP("LowLevel","File does not exist");
		case ERROR_NO_ACCESS:
			return TRANSLATE_NOOP("LowLevel","Permission denied");
		case ERROR_FILE_OPEN:
			return TRANSLATE_NOOP("LowLevel","Could not open file");
		case ERROR_FILE_READ:
			return TRANSLATE_NOOP("LowLevel","Could not read from file");
		case ERROR_BAD_MAGIC:
			return TRANSLATE_NOOP("LowLevel","File is the wrong type or corrupted");
		case ERROR_UNSUPPORTED_VERSION:
			return TRANSLATE_NOOP("LowLevel","Unsupported version");
		case ERROR_BAD_DATA:
			return TRANSLATE_NOOP("LowLevel","File contains invalid data");
		case ERROR_UNEXPECTED_EOF:
			return TRANSLATE_NOOP("LowLevel","Unexpected end of file");
		case ERROR_EXPORT_ONLY:
			return TRANSLATE_NOOP("LowLevel","Write not supported,try \"Export...\"");
		case ERROR_FILTER_SPECIFIC:
			if(model)
			{
				return model->getFilterSpecificError();
			}
			else
			{
				return getLastFilterSpecificError();
			}
		case ERROR_UNKNOWN:
			return TRANSLATE_NOOP("LowLevel","Unknown error" );
	}

	return TRANSLATE_NOOP("LowLevel","Invalid error code");
}

bool Model::operationFailed(Model::ModelErrorE err)
{
	return (err!=ERROR_NONE&&err!=ERROR_CANCEL);
}

void Model::pushError(const std::string &err)
{
	m_loadErrors.push_back(err);
}

std::string Model::popError()
{
	std::string rval = "";
	if(!m_loadErrors.empty())
	{
		rval = m_loadErrors.front();
		m_loadErrors.pop_front();
	}
	return rval;
}

#ifdef MM3D_EDIT

void Model::updateObservers(bool remove_me)
{
	//2019: Prevent recursion.
	static int recursive = 0;

	//https://github.com/zturtleman/mm3d/issues/90
	//I'm letting 0 indicate simply to refresh the view.
	//assert(m_changeBits);
	if(!m_changeBits)
	{
		if(recursive||!remove_me) return;
	}

	recursive++;
	int change = m_changeBits; 
	m_changeBits = 0; //2019
	for(auto*ea:m_observers) ea->modelChanged(change);
	//m_changeBits = 0;
	recursive--;
}

void Model::addObserver(Model::Observer *o)
{
	m_observers.push_back(o);
}

void Model::removeObserver(Model::Observer *o)
{
	ObserverList::iterator it;
	for(it = m_observers.begin(); it!=m_observers.end(); it++)
	{
		if(*it==o)
		{
			m_observers.erase(it);
			return;
		}
	}
}

void Model::setUndoSizeLimit(unsigned sizeLimit)
{
	m_undoMgr->setSizeLimit(sizeLimit);
}

void Model::setUndoCountLimit(unsigned countLimit)
{
	m_undoMgr->setCountLimit(countLimit);
}

bool Model::isVertexVisible(unsigned v)const
{
	LOG_PROFILE();

	if(v<m_vertices.size())
	{
		return m_vertices[v]->m_visible;
	}
	else
	{
		return false;
	}
}

bool Model::isTriangleVisible(unsigned v)const
{
	LOG_PROFILE();

	if(v<m_triangles.size())
	{
		return m_triangles[v]->m_visible;
	}
	else
	{
		return false;
	}
}

/*UNUSED
bool Model::isGroupVisible(unsigned v)const
{
	LOG_PROFILE();

	if(v<m_groups.size())
	{
		return m_groups[v]->m_visible;
	}
	else
	{
		return false;
	}
}*/

bool Model::isBoneJointVisible(unsigned j)const
{
	LOG_PROFILE();

	if(j<m_joints.size())
	{
		return m_joints[j]->m_visible;
	}
	else
	{
		return false;
	}
}

bool Model::isPointVisible(unsigned p)const
{
	LOG_PROFILE();

	if(p<m_points.size())
	{
		return m_points[p]->m_visible;
	}
	else
	{
		return false;
	}
}

int Model::addVertex(double x, double y, double z)
{
	//if(m_animationMode) return -1; //REMOVE ME

	//if(displayFrameAnimPrimitiveError()) return -1; //REMOVE ME

	int num = m_vertices.size();

	m_changeBits |= AddGeometry;

	Vertex *vertex = Vertex::get();

	vertex->m_coord[0] = x;
	vertex->m_coord[1] = y;
	vertex->m_coord[2] = z;
	vertex->m_free = false;
	m_vertices.push_back(vertex);

	if(auto fp=m_vertices.front()->m_frames.size())
	{
		vertex->m_frames.assign(fp,nullptr);
		while(fp-->0) vertex->m_frames[fp] = FrameAnimVertex::get();
	}

	if(m_undoEnabled)
	{
		MU_AddVertex *undo = new MU_AddVertex();
		undo->addVertex(num,vertex);
		sendUndo(undo);
	}

	return num;
}

void Model::setVertexFree(unsigned v,bool o)
{
	if(v<m_vertices.size())
	{
		m_vertices[v]->m_free = o;
	}
}

bool Model::isVertexFree(unsigned v)const
{
	if(v<m_vertices.size())
	{
		return m_vertices[v]->m_free;
	}
	return false;
}

int Model::addTriangle(unsigned v1, unsigned v2, unsigned v3)
{
	//if(m_animationMode) return -1; //REMOVE ME

	//if(displayFrameAnimPrimitiveError()) return -1; //REMOVE ME

	//m_changeBits |= AddGeometry;

	auto &vs = m_vertices;
	unsigned vsz = m_vertices.size();
	if(v1<vsz&&v2<vsz&&v3<vsz)
	{	
		auto num = (unsigned)m_triangles.size();
		//log_debug("adding triangle %d for vertices %d,%d,%d\n",num,v1,v2,v3);

		Triangle *triangle = Triangle::get();
		auto &vi = triangle->m_vertexIndices;
		vi[0] = v1;
		vi[1] = v2;
		vi[2] = v3;
		//m_triangles.push_back(triangle);
		insertTriangle(num,triangle);
		
		if(m_undoEnabled)
		{
			MU_AddTriangle *undo = new MU_AddTriangle();
			undo->addTriangle(num,triangle);
			sendUndo(undo);
		}

		//invalidateNormals(); //OVERKILL

		return num;
	}
	return -1;
}

int Model::addBoneJoint(const char *name, double x, double y, double z,
		/*double xrot, double yrot, double zrot,*/ int parent)
{
	if(!name||(unsigned)parent+1>m_joints.size())
	{
		return -1;
	}

	m_changeBits |= AddOther;

	int num = m_joints.size();

	Joint *joint = Joint::get();

	joint->m_parent = parent;
	joint->m_name	= name;

	double trans[3] = { x,y,z };
	//double rot[3]	= { xrot,yrot,zrot };

	log_debug("New joint at %f,%f,%f\n",x,y,z);

	joint->m_absolute.loadIdentity();
	//joint->m_absolute.setRotation(rot);
	joint->m_absolute.setTranslation(trans);
	joint->m_absolute.getTranslation(joint->m_abs);

	//https://github.com/zturtleman/mm3d/issues/35	
	//2020: I believe this serves no purpose since the rotation
	//values are arbitrary. It changes rot[1] to be -0 in the
	//identity case. These rotations are canceled out by the bind
	//pose. They just add to confusion. tool.cc always makes a 
	//joint with 0 initially.
	if(parent!=-1)
	{
		//NOTE: I think that this isn't even correct to begin with
		//and only works because the rotation doesn't actually matter
		//since it's all relative.

		m_joints[parent]->m_absolute.inverseTranslateVector(trans);
		m_joints[parent]->m_absolute.inverseRotateVector(trans);
		/*
		Matrix &minv = m_joints[parent]->getAbsoluteInverse();
		Matrix mr;
		mr.setRotation(rot);
		mr = mr * minv;
		mr.getRotation(rot);
		minv.getRotation(rot);
		*/
	}

	for(int t = 0; t<3; t++)
	{
		joint->m_rel[t] = trans[t];
	//	joint->m_rot[t] = rot[t]; 
	}

	//2020: let BS_RIGHT add a new root?
	//m_joints.push_back(joint);
	num = parent==-1?0:m_joints.size();
	 
	insertBoneJoint(num,joint);
	
	if(m_undoEnabled)
	{
		auto undo = new MU_AddBoneJoint;
		undo->addBoneJoint(num,joint);
		sendUndo(undo);
	}

	return num;
}

int Model::addPoint(const char *name, double x, double y, double z,
		double xrot, double yrot, double zrot)
{
	if(!name) return -1; //m_animationMode

	//if(displayFrameAnimPrimitiveError()) return -1; //REMOVE ME

	m_changeBits |= AddOther;

	int num = m_points.size();

	log_debug("New point at %f,%f,%f\n",x,y,z);

	Point *point = Point::get();

	point->m_name	= name;

	point->m_abs[0] = x;
	point->m_abs[1] = y;
	point->m_abs[2] = z;
	point->m_rot[0]	= xrot;
	point->m_rot[1]	= yrot;
	point->m_rot[2]	= zrot;

	num = m_points.size();
	insertPoint(num,point);

	if(m_undoEnabled)
	{
		auto undo = new MU_AddPoint;
		undo->addPoint(num,point);
		sendUndo(undo);
	}

	//validateSkel(); //???

	return num;
}

int Model::addProjection(const char *name, int type, double x, double y, double z)
{
	//if(m_animationMode) return -1; //REMOVE ME

	if(name==nullptr) return -1;

	m_changeBits |= AddOther;

	int num = m_projections.size();

	log_debug("New projection at %f,%f,%f\n",x,y,z);

	TextureProjection *proj = TextureProjection::get();

	proj->m_name = name;
	proj->m_type = type;

	proj->m_abs[0] = x;
	proj->m_abs[1] = y;
	proj->m_abs[2] = z;

	proj->m_range[0][0] =  0.0;  // min x
	proj->m_range[0][1] =  0.0;  // min y
	proj->m_range[1][0] =  1.0;  // max x
	proj->m_range[1][1] =  1.0;  // max y

	insertProjection(num,proj);

	if(m_undoEnabled)
	{
		auto undo = new MU_AddProjection;
		undo->addProjection(num,proj);
		sendUndo(undo);
	}

	return num;
}

bool Model::setTriangleVertices(unsigned triangleNum, unsigned vert1, unsigned vert2, unsigned vert3)
{
	//if(m_animationMode) return false; //REMOVE ME

	auto &vs = m_vertices;
	unsigned vsz = m_vertices.size();
	if(triangleNum<m_triangles.size()&&vert1<vsz&&vert2<vsz&&vert3<vsz)
	{
		auto tri = m_triangles[triangleNum];
		auto &vi = tri->m_vertexIndices;

		if(m_undoEnabled)
		{
			auto undo = new MU_SetTriangleVertices;
			undo->setTriangleVertices(triangleNum,vert1,vert2,vert3,vi[0],vi[1],vi[2]);
			sendUndo(undo);
		}

		//2020: Keep connectivity to help calculateNormals
		unsigned v[3] = {vert1,vert2,vert3};
		for(int i=3;i-->0;)
		{
			vs[vi[i]]->_erase_face(tri,i);
			vs[v[i]]->m_faces.push_back({tri,i});
			vi[i] = v[i];
		}

		invalidateNormals(); //OVERKILL

		return true;
	}
	return false;
}

bool Model::getTriangleVertices(unsigned triangleNum, unsigned &vert1, unsigned &vert2, unsigned &vert3)const
{
	if(triangleNum<m_triangles.size())
	{
		vert1 = m_triangles[triangleNum]->m_vertexIndices[0];
		vert2 = m_triangles[triangleNum]->m_vertexIndices[1];
		vert3 = m_triangles[triangleNum]->m_vertexIndices[2];

		return true;
	}
	else
	{
		return false;
	}
}

void Model::deleteVertex(unsigned vertexNum)
{
	LOG_PROFILE();
	
	//if(m_animationMode) return; //REMOVE ME

	//if(displayFrameAnimPrimitiveError()) return; //REMOVE ME

	if(vertexNum>=m_vertices.size()) return;

	m_changeBits |= AddGeometry; //2020

	if(m_undoEnabled)
	{
		auto undo = new MU_DeleteVertex;
		undo->deleteVertex(vertexNum,m_vertices[vertexNum]);
		sendUndo(undo);
	}

	removeVertex(vertexNum);
}

void Model::deleteTriangle(unsigned triangleNum)
{
	LOG_PROFILE();

	//if(m_animationMode) return; //REMOVE ME

	//if(displayFrameAnimPrimitiveError()) return; //REMOVE ME

	if(triangleNum>=m_triangles.size()) return;

	// remove it from any groups
	for(unsigned g = 0; g<m_groups.size(); g++)
	{
		removeTriangleFromGroup(g,triangleNum);
	}

	//m_changeBits |= AddGeometry

	if(m_undoEnabled)
	{
		// Delete triangle
		auto undo = new MU_DeleteTriangle;
		undo->deleteTriangle(triangleNum,m_triangles[triangleNum]);
		sendUndo(undo);
	}

	removeTriangle(triangleNum);
}

void Model::deleteBoneJoint(unsigned joint)
{
	if(joint>=m_joints.size()) return;
		
	unsigned count = m_joints.size();

	// Break out early if this is a root joint and it has a child
	if(m_joints[joint]->m_parent<0)		
	for(unsigned j=0;j<count;j++)
	if(j!=joint&&m_joints[j]->m_parent==(int)joint)
	{
		model_status(this,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel","Cannot delete root joint"));
		return;
	}

	m_changeBits |= AddOther; //2020

	for(unsigned v = 0; v<m_vertices.size(); v++)
	{
		removeVertexInfluence(v,joint);
	}

	for(unsigned p = 0; p<m_points.size(); p++)
	{
		removePointInfluence(p,joint);
	}

	/*NONSENSE
	https://github.com/zturtleman/mm3d/issues/132
	int parent = joint; do
	{
		parent = m_joints[parent]->m_parent;
	}
	while(parent>=0&&m_joints[parent]->m_selected); //???
	*/
	int parent = m_joints[joint]->m_parent; //Assuming???
		
	for(unsigned j = 0; j<count; j++)
	{
		if(m_joints[j]->m_parent==(int)joint)
		{
			setBoneJointParent(j,m_joints[joint]->m_parent);

			auto &mm = m_joints[j]->m_absolute;				

			//https://github.com/zturtleman/mm3d/issues/132
			if(parent>=0)
			mm = mm * m_joints[parent]->getAbsoluteInverse();

			mm.getRotation(m_joints[j]->m_rot);
			mm.getTranslation(m_joints[j]->m_rel);
		}
	}

	Joint *ptr = m_joints[joint];
	removeBoneJoint(joint);
		
	//invalidateSkel(); //m_validJoints = false; //???

	if(m_undoEnabled)
	{
		auto undo = new MU_DeleteBoneJoint;
		undo->deleteBoneJoint(joint,ptr);
		sendUndo(undo);
	}

	log_debug("parent was %d\n",parent);

	//2020: Insert into animation?
	//validateSkel();
	invalidateSkel();
	
}

void Model::deletePoint(unsigned point)
{
	//if(m_animationMode) return; //REMOVE ME

	//if(displayFrameAnimPrimitiveError()) return; //REMOVE ME
	
	m_changeBits |= AddOther; //2020

	if(point<m_points.size())
	{
		Point *ptr = m_points[point];
		removePoint(point);

		if(m_undoEnabled)
		{
			auto undo = new MU_DeletePoint;
			undo->deletePoint(point,ptr);
			sendUndo(undo);
		}

		//validateSkel(); //???
	}
}

void Model::deleteProjection(unsigned proj)
{
	//if(m_animationMode) return; //REMOVE ME

	if(proj<m_projections.size())
	{
		m_changeBits |= AddOther; //2020

		TextureProjection *ptr = m_projections[proj];
		removeProjection(proj);

		if(m_undoEnabled)
		{
			auto undo = new MU_DeleteProjection;
			undo->deleteProjection(proj,ptr);
			sendUndo(undo);
		}
	}
}

void Model::deleteOrphanedVertices()
{
	LOG_PROFILE();
	
	//if(m_animationMode) return; //REMOVE ME

	for(unsigned v = 0; v<m_vertices.size(); v++)
	{
		m_vertices[v]->m_marked = false;
	}

	for(unsigned t = 0; t<m_triangles.size(); t++)
	{
		for(unsigned v = 0; v<3; v++)
		{
			m_vertices[m_triangles[t]->m_vertexIndices[v]]->m_marked = true;
		}
	}

	for(int v = m_vertices.size()-1; v>=0; v--)
	{
		if(!m_vertices[v]->m_marked 
			  &&!m_vertices[v]->m_free)
		{
			deleteVertex(v);
		}
	}
}

void Model::deleteFlattenedTriangles()
{
	LOG_PROFILE();

	// Delete any triangles that have two or more vertex indices that point
	// at the same vertex (could happen as a result of welding vertices
	
	//if(m_animationMode) return; //REMOVE ME

	int count = m_triangles.size();
	for(int t = count-1; t>=0; t--)
	{
		if(m_triangles[t]->m_vertexIndices[0]==m_triangles[t]->m_vertexIndices[1]
		 ||m_triangles[t]->m_vertexIndices[0]==m_triangles[t]->m_vertexIndices[2]
		 ||m_triangles[t]->m_vertexIndices[1]==m_triangles[t]->m_vertexIndices[2])
		{
			deleteTriangle(t);
		}
	}
}

void Model::deleteSelected()
{
	LOG_PROFILE();

	/*FIX ME
	//NOTE: What about ANIMMODE_FRAME?
	//2020: interpolateSelected can be used instead
	//(It's probably better if deletecmd functions)
	if(m_animationMode)
	{
		if(inSkeletalMode())
		if(m_currentAnim<m_skelAnims.size()
		&&m_currentFrame<m_skelAnims[m_currentAnim]->_frame_count())
		{
			for(Position j{PT_Joint,m_joints.size()};j-->0;) 
			if(m_joints[j]->m_selected)
			deleteKeyframe(m_currentAnim,m_currentFrame,j);
		}

		return; //!
	}*/

	for(auto v=m_vertices.size();v-->0;)
	{
		m_vertices[v]->m_marked = false;
	}

	for(auto t=m_triangles.size();t-->0;)
	{
		if(m_triangles[t]->m_selected)
		{
			m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_marked = true;
			m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_marked = true;
			m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_marked = true;
			deleteTriangle(t);
		}
	}

	for(int t = m_triangles.size()-1; t>=0; t--)
	{
		if(m_triangles[t]->m_visible)
		{
			for(int v = 0; v<3; v++)
			{
				if(
						m_vertices[m_triangles[t]->m_vertexIndices[v]]->m_selected 
					  &&!m_vertices[m_triangles[t]->m_vertexIndices[v]]->m_marked)
				{
					deleteTriangle(t);
					break;
				}
			}
		}
	}

	for(int v = m_vertices.size()-1; v>=0; v--)
	{
		if(m_vertices[v]->m_selected&&!m_vertices[v]->m_marked)
		{
			deleteVertex(v);
		}
	}

	deleteOrphanedVertices();
	bool doJointSetup = false;

	for(int j = m_joints.size()-1; j>=0; j--)
	{
		if(m_joints[j]->m_selected)
		{
			deleteBoneJoint(j);
			doJointSetup = true;
		}
	}

	for(int p = m_points.size()-1; p>=0; p--)
	{
		if(m_points[p]->m_selected)
		{
			deletePoint(p);
		}
	}

	for(int r = m_projections.size()-1; r>=0; r--)
	{
		if(m_projections[r]->m_selected)
		{
			deleteProjection(r);
		}
	}

	// Some selected vertices may not be deleted if their parent
	// triangle was deleted
	unselectAll();
}

bool Model::movePosition(const Position &pos, double x, double y, double z)
{
	switch (pos.type)
	{
	case PT_Vertex:
		return moveVertex(pos.index,x,y,z);

	case PT_Joint:
		return moveBoneJoint(pos.index,x,y,z);

	case PT_Point:
		return movePoint(pos.index,x,y,z);

	case PT_Projection:
		return moveProjection(pos.index,x,y,z);

	default:
		log_error("do not know how to move position of type %d\n",pos.type);
		break;
	}
	return false;
}

bool Model::moveVertex(unsigned v, double x, double y, double z)
{
	switch (m_animationMode)
	{
	case ANIMMODE_NONE:
		if(v<m_vertices.size())
		{
			double old[3];

			old[0] = m_vertices[v]->m_coord[0];
			old[1] = m_vertices[v]->m_coord[1];
			old[2] = m_vertices[v]->m_coord[2];

			m_vertices[v]->m_coord[0] = x;
			m_vertices[v]->m_coord[1] = y;
			m_vertices[v]->m_coord[2] = z;

			invalidateNormals(); //OVERKILL

			if(m_undoEnabled)
			{
				auto undo = new MU_MovePrimitive;
				undo->addMovePrimitive(MU_MovePrimitive::MT_Vertex,v,x,y,z,
						old[0],old[1],old[2]);
				sendUndo(undo,true);
			}

			return true;
		}
		break;
	case ANIMMODE_FRAME:
		if(v<m_vertices.size()&&m_currentAnim<m_frameAnims.size())
		{
			setFrameAnimVertexCoords(m_currentAnim,m_currentFrame,v,x,y,z);

			return true;
		}
		break;
	default:
		break;
	}
	return false;
}

bool Model::moveBoneJoint(unsigned j, double x, double y, double z)
{
	if(j>=m_joints.size()) return false;

	bool rval; if(!m_animationMode)
	{
		double *cmp = m_joints[j]->m_abs;
		if(x==cmp[0]&&y==cmp[1]&&z==cmp[2]) return true; //2020

		if(m_undoEnabled)
		{
			auto undo = new MU_MovePrimitive;
			undo->addMovePrimitive(MU_MovePrimitive::MT_Joint,j,x,y,z,
					m_joints[j]->m_abs[0],//m_absolute.get(3,0),
					m_joints[j]->m_abs[1],//m_absolute.get(3,1),
					m_joints[j]->m_abs[2]);//m_absolute.get(3,2));
			sendUndo(undo,true);
		}

		rval = relocateBoneJoint(j,x,y,z,false);
	}
	else if(inSkeletalMode()) //2020
	{	
		//HACK? Need to update m_final matrix.
		validateAnimSkel();

		auto p = m_joints[j]; 
		double coord[3] = {x,y,z};
		if(!memcmp(coord,p->m_kfAbs(),sizeof(coord)))
		{
			return true; //2020
		}

		//DUPLICATES translateSelected LOGIC.		
		if(p->m_parent>=0)
		{
			Joint *pp = m_joints[p->m_parent];
			pp->m_final.inverseTranslateVector(coord);
			pp->m_final.inverseRotateVector(coord);
			p->m_relative.inverseTranslateVector(coord);
			p->m_relative.inverseRotateVector(coord);				
		}
		else //NECESSARY?
		{
			p->m_absolute.inverseTranslateVector(coord);
			p->m_absolute.inverseRotateVector(coord);
		}
		makeCurrentAnimationFrame();
		rval = -1!=setKeyframe
		(m_currentAnim,m_currentFrame,{PT_Joint,j},KeyTranslate,coord[0],coord[1],coord[2]);
	}
	else return false;

	if(rval) invalidateSkel(); return rval;
}

bool Model::movePoint(unsigned p, double x, double y, double z)
{
	if(!m_animationMode&&p<m_points.size())
	{
		m_changeBits|=MoveOther; //2020

		if(m_undoEnabled)
		{
			auto undo = new MU_MovePrimitive;
			undo->addMovePrimitive(MU_MovePrimitive::MT_Point,p,x,y,z,
					m_points[p]->m_abs[0],
					m_points[p]->m_abs[1],
					m_points[p]->m_abs[2]);
			sendUndo(undo,true);
		}

		m_points[p]->m_abs[0] = x;
		m_points[p]->m_abs[1] = y;
		m_points[p]->m_abs[2] = z;

		return true;
	}
	else if(m_animationMode==ANIMMODE_FRAME)
	{
		makeCurrentAnimationFrame();
		//return setFrameAnimPointCoords(m_currentAnim,m_currentFrame,p,x,y,z);
		return -1!=setKeyframe(m_currentAnim,m_currentFrame,{PT_Point,p},KeyTranslate,x,y,z);
	}
	else
	{
		log_debug("point %d does not exist\n",p);
		return false;
	}
}

bool Model::relocateBoneJoint(unsigned j, double x, double y, double z, bool downstream)
{
	if(j>=m_joints.size()) return false; //???
	
	m_changeBits|=MoveOther; //2020

	double old[3];
	double diff[3];
	double tran[3];

	old[0] = m_joints[j]->m_absolute.get(3,0);
	old[1] = m_joints[j]->m_absolute.get(3,1);
	old[2] = m_joints[j]->m_absolute.get(3,2);

	tran[0] = diff[0] = (x-old[0]);
	tran[1] = diff[1] = (y-old[1]);
	tran[2] = diff[2] = (z-old[2]);

	if(m_joints[j]->m_parent>=0)
	{
		m_joints[m_joints[j]->m_parent]->m_absolute.inverseRotateVector(tran);
	}

	m_joints[j]->m_rel[0] += tran[0];
	m_joints[j]->m_rel[1] += tran[1];
	m_joints[j]->m_rel[2] += tran[2];

	if(!downstream)
	for(unsigned t = 0; t<m_joints.size(); t++)
	{
		if(m_joints[t]->m_parent==(signed)j)
		{
			tran[0] = diff[0];
			tran[1] = diff[1];
			tran[2] = diff[2];

			m_joints[m_joints[t]->m_parent]->m_absolute.inverseRotateVector(tran);

			m_joints[t]->m_rel[0] -= tran[0];
			m_joints[t]->m_rel[1] -= tran[1];
			m_joints[t]->m_rel[2] -= tran[2];
		}
	}

	invalidateSkel();

	return true;
}

void Model::beginHideDifference()
{
	LOG_PROFILE();

	if(m_undoEnabled)
	{
		unsigned t;
		for(t = 0; t<m_vertices.size(); t++)
		{
			m_vertices[t]->m_marked = m_vertices[t]->m_visible;
		}
		for(t = 0; t<m_triangles.size(); t++)
		{
			m_triangles[t]->m_marked = m_triangles[t]->m_visible;
		}
	}
}

void Model::endHideDifference()
{
	LOG_PROFILE();

	if(m_undoEnabled) switch (m_selectionMode)
	{
	case SelectVertices:
	{
		auto undo = new MU_Hide(SelectVertices);
		for(unsigned t = 0; t<m_vertices.size(); t++)		
		if(m_vertices[t]->m_visible!=m_vertices[t]->m_marked)
		{
			undo->setHideDifference(t,m_vertices[t]->m_visible);
		}
		sendUndo(undo);	break;
	}
	case SelectTriangles:
	case SelectGroups:
	{
		auto undo = new MU_Hide(SelectTriangles);
		for(unsigned t = 0; t<m_triangles.size(); t++)
		if(m_triangles[t]->m_visible!=m_triangles[t]->m_marked)
		{
			undo->setHideDifference(t,m_triangles[t]->m_visible);
		}
		sendUndo(undo); break;
	}}
}

void Model::interpolateSelected(Model::Interpolant2020E d, Model::Interpolate2020E e)
{
	assert(e>=0&&e<=InterpolateLerp); //InterpolateLerp

	auto am = m_animationMode;

	if(!am||e<0||d<0||e>InterpolateLerp||d>InterpolantScale) return;

	bool skel = am==ANIMMODE_SKELETAL;
		
	//TODO: Remove the frame if no keys remain. This can be
	//done manually with the clear operation until a counter
	//is added/managed.
	auto anim = m_currentAnim, frame = m_currentFrame;
	if(!e) //Programmer error? 
	{
		if(m_currentTime!=getAnimFrameTime(am,anim,frame))
		return;
	}
	else frame = makeCurrentAnimationFrame(); 
	
	Keyframe kf;
	kf.m_isRotation = KeyType2020E(1<<d);
	kf.m_frame = frame;

	if(e) validateAnim(); //setKeyframe/setFrameAnimVertexCoords/InterpolateCopy

	m_changeBits|=MoveGeometry;

	Position j{skel?PT_Joint:PT_Point,-1};
	auto &vec = *(std::vector<Object2020*>*)(skel?(void*)&m_joints:&m_points);
	for(auto*ea:vec)
	{
		j++; if(ea->m_selected)
		{		
			const double *coord = ea->getParams(d);
			setKeyframe(anim,frame,j,kf.m_isRotation,coord[0],coord[1],coord[2],e);
		}
	}

	if(m_animationMode==ANIMMODE_FRAME)
	{	
		const bool ue = m_undoEnabled;
		MU_InterpolateSelected *undo = nullptr;

		auto fa = m_frameAnims[m_currentAnim];
		unsigned i=0, fp = fa->m_frame0+frame;
		for(auto*ea:m_vertices) 
		{
			i++; if(ea->m_selected)
			{
				auto vf = ea->m_frames[fp];
				auto &cmp = vf->m_interp2020;
				if(cmp!=e) 
				{
					if(ue&&!undo)
					undo = new MU_InterpolateSelected(e,anim,frame);
					if(ue) undo->addVertex(cmp); 
				
					//HACK: Maybe this value should already be stored.
					if(cmp<=InterpolateCopy)					
					memcpy(vf->m_coord,ea->m_kfCoord,sizeof(ea->m_kfCoord));

					cmp = e;
				}
			}
		}

		sendUndo(undo);
	}

	invalidateAnim();
}

//FIX ME 
//Call sites all use vectors. What does it 
//even mean to "translate" with a matrix??
//void translateSelected(const Matrix &m);
void Model::translateSelected(const double vec[3]) 
{
	LOG_PROFILE();

	if(!vec[0]&&!vec[1]&&!vec[2]) return; //2020

//	std::vector<unsigned> newJointList; //???
	
	makeCurrentAnimationFrame(); //2020

	auto ca = m_currentAnim, cf = m_currentFrame;

	if(inSkeletalMode())
	{
		for(Position j{PT_Joint,0};j<m_joints.size();j++)
		if(m_joints[j]->m_selected&&!parentJointSelected(j))
		{
			//HACK? Need to update m_final matrix.
			validateAnimSkel();

			//DUPLICATES NEW moveBoneJoint LOGIC.
			auto p = m_joints[j];
			double coord[3]; for(int i=3;i-->0;) 
			{
				coord[i] = vec[i]+p->m_kfAbs()[i];
			}
			if(p->m_parent>=0)
			{
				Joint *pp = m_joints[p->m_parent];
				pp->m_final.inverseTranslateVector(coord);
				pp->m_final.inverseRotateVector(coord);
				p->m_relative.inverseTranslateVector(coord);
				p->m_relative.inverseRotateVector(coord);				
			}
			else //NECESSARY?
			{
				p->m_absolute.inverseTranslateVector(coord);
				p->m_absolute.inverseRotateVector(coord);
			}
			setKeyframe(ca,cf,j,KeyTranslate,coord[0],coord[1],coord[2]);
		}
	}
	else if(m_animationMode==ANIMMODE_FRAME) 
	{
		if(m_frameAnims.empty()) return; //2020

		for(unsigned v=0;v<m_vertices.size();v++)
		if(m_vertices[v]->m_selected)
		{
			double coord[3];
			interpKeyframe(ca,cf,v,coord);
			for(int i=3;i-->0;) coord[i]+=vec[i];
			setFrameAnimVertexCoords(ca,cf,v,coord[0],coord[1],coord[2]);
		}

		for(Position j{PT_Point,0};j<m_points.size();j++)
		if(m_points[j]->m_selected)
		{
			double coord[3];
			interpKeyframe(ca,cf,j,coord,nullptr,nullptr);
			for(int i=3;i-->0;) coord[i]+=vec[i];
			setKeyframe(ca,cf,j,KeyTranslate,coord[0],coord[1],coord[2]);
		}
	}
	else
	{
		bool verts = false;
		for(unsigned v=0;v<m_vertices.size();v++)		
		if(m_vertices[v]->m_selected)
		{
			m_vertices[v]->m_coord[0]+=vec[0];
			m_vertices[v]->m_coord[1]+=vec[1];
			m_vertices[v]->m_coord[2]+=vec[2];

			verts = true;
		}
		if(verts)
		{
			m_changeBits |= MoveGeometry;

			invalidateNormals(); //OVERKILL
		}

		for(unsigned j=0;j<m_joints.size();j++) 
		{
			if(m_joints[j]->m_selected)
			{
				invalidateSkel(); //2020

				m_changeBits |= MoveOther; //2020

				double tran[3] = {vec[0],vec[1],vec[2]};
				//m.getTranslation(tran);
				//REMINDER: inverseRotateVector should scale
				if(int p=m_joints[j]->m_parent>=0)
				m_joints[m_joints[j]->m_parent]->m_absolute.inverseRotateVector(tran);
				for(int i=3;i-->0;)
				m_joints[j]->m_rel[i]+=tran[i];

				for(size_t t=0;t<m_joints.size();t++)									
				if(j==(unsigned)m_joints[t]->m_parent)
				{
					//REMINDER: inverseRotateVector should scale
					double tran2[3] = {vec[0],vec[1],vec[2]};
					m_joints[j]->m_absolute.inverseRotateVector(tran2);

					for(;t<m_joints.size();t++)
					if(m_joints[t]->m_parent==(signed)j)
					for(int i=3;i-->0;)
					m_joints[t]->m_rel[i]-=tran2[i]; 
						
					break;
				}
			}
		}

		for(unsigned p = 0; p<m_points.size(); p++)
		{
			if(m_points[p]->m_selected)
			{
				m_changeBits |= MoveOther; //2020

				for(int i=3;i-->0;)
				m_points[p]->m_abs[i]+=vec[i];
			}
		}

		for(unsigned p = 0; p<m_projections.size(); p++)
		{
			if(m_projections[p]->m_selected)
			{
				m_changeBits |= MoveOther; //2020

				for(int i=3;i-->0;)
				m_projections[p]->m_abs[i]+=vec[i];

				applyProjection(p);
			}
		}
		
		if(m_undoEnabled)
		{
			auto undo = new MU_TranslateSelected;
			//undo->setMatrix(m);
			undo->setVector(vec);
			sendUndo(undo,true);
		}
	}
}

void Model::rotateSelected(const Matrix &m, double *point)
{
	LOG_PROFILE();

	makeCurrentAnimationFrame(); //2020

	auto ca = m_currentAnim, cf = m_currentFrame;

	if(inSkeletalMode())
	{
		unsigned j = 0;
		for(j = 0; j<m_joints.size(); j++)
		{
			if(m_joints[j]->m_selected)
			{
				//HACK? Need to update m_final matrix.
				validateAnimSkel();

				Matrix absInv; if(m_joints[j]->m_parent>=0)
				{
					Joint *parent = m_joints[m_joints[j]->m_parent];
					absInv = m_joints[j]->m_relative * parent->m_final;
					absInv = absInv.getInverse();
				}
				else absInv = m_joints[j]->getAbsoluteInverse();

				Matrix cur = m_joints[j]->m_final * m * absInv;

				double rot[3]; cur.getRotation(rot);

				setKeyframe(ca,cf,{PT_Joint,j},KeyRotate,rot[0],rot[1],rot[2]);
			//	setCurrentAnimationFrame(m_currentFrame);

				// setKeyframe handles undo

				// TODO: should I really allow multiple joints here?
				//break;
			}
		}
	}
	else if(m_animationMode==ANIMMODE_FRAME)
	{
		for(unsigned v = 0; v<m_vertices.size(); v++)
		{
			if(m_vertices[v]->m_selected)
			{
				double coord[3];
				interpKeyframe(ca,cf,v,coord);

				coord[0] -= point[0];
				coord[1] -= point[1];
				coord[2] -= point[2];

				Vector vec(coord);
				vec.transform(m);

				coord[0] = vec[0];
				coord[1] = vec[1];
				coord[2] = vec[2];

				coord[0] += point[0];
				coord[1] += point[1];
				coord[2] += point[2];

				setFrameAnimVertexCoords(ca,cf,v,coord[0],coord[1],coord[2]);
			}
		}

		for(Position p{PT_Point,0};p<m_points.size();p++)
		{
			if(m_points[p]->m_selected)
			{
				double coord[3],rot[3];
				interpKeyframe(ca,cf,p,coord,rot,nullptr);

				Matrix pm;
				pm.setTranslation(
						coord[0]-point[0],
						coord[1]-point[1],
						coord[2]-point[2]);
				pm.setRotation(rot);

				pm = pm *m;

				pm.getTranslation(coord);
				pm.getRotation(rot);

				coord[0] += point[0];
				coord[1] += point[1];
				coord[2] += point[2];

				setKeyframe(ca,cf,p,KeyTranslate,coord[0],coord[1],coord[2]);
				setKeyframe(ca,cf,p,KeyRotate,rot[0],rot[1],rot[2]);
			}
		}
	}
	else
	{
		for(unsigned v = 0; v<m_vertices.size(); v++)
		{
			if(m_vertices[v]->m_selected)
			{
				m_changeBits|=MoveGeometry; //2020

				m_vertices[v]->m_coord[0] -= point[0];
				m_vertices[v]->m_coord[1] -= point[1];
				m_vertices[v]->m_coord[2] -= point[2];

				Vector vec(m_vertices[v]->m_coord);

				vec.transform(m);
				const double *val = vec.getVector();

				m_vertices[v]->m_coord[0] = val[0];
				m_vertices[v]->m_coord[1] = val[1];
				m_vertices[v]->m_coord[2] = val[2];

				m_vertices[v]->m_coord[0] += point[0];
				m_vertices[v]->m_coord[1] += point[1];
				m_vertices[v]->m_coord[2] += point[2];
			}
		}
		
		//NOTE: qm is just in case m is not affine.
		Matrix pm,qm;

		for(unsigned j = 0; j<m_joints.size(); j++)
		{
			// NOTE: This code assumes that if a bone joint is rotated,
			// all children are rotated with it. That may not be what
			// the user expects. To prevent this I would have to find
			// unselected joints whose direct parent was selected
			// and invert the operation on those joints.
			if(m_joints[j]->m_selected&&!parentJointSelected(j))
			{
				//HACK? Need to update m_final matrix.
				validateAnimSkel();

				m_changeBits|=MoveOther; //2020

				Joint *joint = m_joints[j];

				Matrix &hack = joint->m_absolute;
				double swap[3];
				hack.getTranslation(swap);
				{
					for(int i=3;i-->0;) hack.getVector(3)[i]-=point[i];
					qm = hack * m;
					for(int i=3;i-->0;) qm.getVector(3)[i]+=point[i];
				}
				hack.setTranslation(swap);

				if(joint->m_parent>=0)
				qm = qm * m_joints[joint->m_parent]->getAbsoluteInverse();

				//WARNING! Assuming m isn't a scale matrix.				
				qm.normalizeRotation();
				qm.getRotation(joint->m_rot);
				qm.getTranslation(joint->m_rel);

				invalidateSkel(); //2020
			}
		}

		auto f = [&](Object2020 &obj)
		{
			pm.setTranslation(
					obj.m_abs[0]-point[0],
					obj.m_abs[1]-point[1],
					obj.m_abs[2]-point[2]);
			pm.setRotation(obj.m_rot);

			qm = pm * m;

			qm.getTranslation(obj.m_abs);
			qm.getRotation(obj.m_rot);

			obj.m_abs[0] += point[0];
			obj.m_abs[1] += point[1];
			obj.m_abs[2] += point[2];
		};

		for(unsigned p = 0; p<m_points.size(); p++)
		{
			if(m_points[p]->m_selected)
			{
				m_changeBits|=MoveOther; //2020

				f(*m_points[p]);				
			}
		}

		for(unsigned r = 0; r<m_projections.size(); r++)
		{
			if(m_projections[r]->m_selected)
			{
				m_changeBits|=MoveOther; //2020

				f(*m_projections[r]);				

				applyProjection(r);
			}
		}

		invalidateNormals(); //OVERKILL

		if(m_undoEnabled)
		{
			auto undo = new MU_RotateSelected;
			undo->setMatrixPoint(m,point);
			sendUndo(undo,true);
		}
	}
}

//2020: Manipulating m to implement scale
//void Model::applyMatrix(const Matrix &m, OperationScopeE scope, bool animations, bool undoable)
void Model::applyMatrix(Matrix m, OperationScopeE scope, bool animations, bool undoable)
{
	LOG_PROFILE();
	
	//NOTE: I think "undoable" is in case the matrix
	//isn't invertible. 
	if(m_undoEnabled)
	{
		if(undoable) 
		{
			//CAUTION: m IS MODIFIED BELOW
			auto undo = new MU_ApplyMatrix;
			undo->setMatrix(m,scope,animations);
			sendUndo(undo,true);
		}
		else clearUndo(); //YIKES!!!
	}
	
	makeCurrentAnimationFrame(); //2020

	bool global = scope==OS_Global;

	unsigned vcount = m_vertices.size();
	unsigned bcount = m_joints.size();
	unsigned pcount = m_points.size();
	unsigned rcount = m_projections.size();
	unsigned facount = m_frameAnims.size();
	
	for(unsigned v = 0; v<vcount; v++)
	{
		if(global||m_vertices[v]->m_selected)
		{			
			m_changeBits |= MoveGeometry;

			m.apply3x(m_vertices[v]->m_coord);

			for(auto*ea:m_vertices[v]->m_frames) 
			{
				m.apply3x(ea->m_coord);
			}
		}
	}

	Matrix pm,qm;
	auto g = [&](Joint &j)
	{
		pm = j.m_absolute * m;
		if(j.m_parent>=0)
		pm = pm * m_joints[j.m_parent]->getAbsoluteInverse();			

		double unscale[3];
		pm.getScale(unscale);
		for(int i=3;i-->0;)
		{
			j.m_xyz[i] = unscale[i];
			unscale[i] = 1/unscale[i];
		}
		pm.scale(unscale);		
		pm.getRotation(j.m_rot);
		pm.getTranslation(j.m_rel);
	};
	
	//2020: I've reimplemented this from scratch
	//since it didn't really make any sense before
	//https://github.com/zturtleman/mm3d/issues/131
	if(global?!m_joints.empty():getSelectedBoneJointCount())
	{
		//HACK: m_marked and calculateSkel are to
		//avoid recomputing inverse matrices. The
		//better way would be to store an inverse
		//matrix in joints
		for(auto*ea:m_joints) ea->m_marked = false;
		{
			validateSkel();
	
			if(!global)
			{
				if(!m_joints.empty())
				{	
					g(*m_joints[0]);
				}
			}
			else for(unsigned b=bcount;b-->0;)
			{	
				if(m_joints[b]->m_selected&&!parentJointSelected(b))
				{
					g(*m_joints[b]);
				}		
			}
		}
		for(auto*ea:m_joints) ea->m_marked = false;				
		//invalidateSkel(); //USE ME
		calculateSkel(); //NOT ME
	}

	//FIX ME
	//What about scale?
	double scale[3],unscale[3]; //2020
	m.getScale(scale);
	for(int i=3;i-->0;) unscale[i] = 1/scale[i];
	m.scale(unscale);	

	auto f = [&](Object2020 &obj)
	{
		m_changeBits |= MoveOther;

		//FIX ME
		//What about scale? setMatrix? Projections?
		//Matrix pm = obj.getMatrix();
		pm.setRotation(obj.m_rot);
		pm.setTranslation(obj.m_abs);

		//NOTE: qm is because m may be a nonaffine
		//matrix... though no idea what that means
		//since the matrix is decomposed as affine.
		qm = pm * m;

		//pm.normalizeRotation(); //2020
		qm.getRotation(obj.m_rot);
		qm.getTranslation(obj.m_abs);

		//FIX ME
		//What about scale? //Correct?
		for(int i=3;i-->0;)
		obj.m_xyz[i]*=scale[i]; 
	};	

	for(unsigned p=pcount;p-->0;)
	if(global||m_points[p]->m_selected)
	{
		f(*m_points[p]);

		for(auto*fa:m_frameAnims)
		for(auto*kf:fa->m_keyframes[p])
		{
			switch(kf->m_isRotation)
			{
			case KeyTranslate:
				m.translateVector(kf->m_parameter); 
				break;
			case KeyRotate:
				m.rotateVector(kf->m_parameter); 
				break;
			//FIX ME
			//What about scale? //Correct?
			case KeyScale:
				for(int i=3;i-->0;)
				kf->m_parameter[i]*=scale[i]; 
				break;
			}
		}
	}

	//YUCK: Projections can't have nonuniform
	//scale... what to do then?
	scale[0]+=scale[1];
	scale[0]+=scale[2];
	scale[0]/=3;
	scale[1] = scale[0];
	scale[2] = scale[0];
	for(unsigned r=rcount;r-->0;)
	{
		if(global||m_projections[r]->m_selected)
		{
			f(*m_projections[r]);

			applyProjection(r); //2020
		}
	}

	// Skeletal animations are handled by bone joint

	invalidateNormals(); //OVERKILL
	invalidateAnim(); 
}

void Model::subdivideSelectedTriangles()
{
	LOG_PROFILE();

	//if(m_animationMode) return; //REMOVE ME

	//if(displayFrameAnimPrimitiveError()) return; //REMOVE ME

	m_changeBits|=SelectionVertices|SelectionFaces; //2020

	sorted_list<SplitEdgesT> seList;

	//NOTICE: m_undoEnabled = false!
	auto undo = m_undoEnabled?new MU_SubdivideTriangle:nullptr;
	if(undo) sendUndo(new MU_SubdivideSelected);
	if(undo) m_undoEnabled = false;

	unsigned vertexStart = m_vertices.size();
	unsigned tlen = m_triangles.size();
	unsigned tnew = tlen;
	
	for(unsigned t = 0; t<tlen; t++)
	{
		if(m_triangles[t]->m_selected)
		{
			unsigned verts[3];

			for(unsigned v = 0; v<3; v++)
			{
				unsigned a = m_triangles[t]->m_vertexIndices[v];
				unsigned b = m_triangles[t]->m_vertexIndices[(v+1)%3];

				unsigned index;

				if(b<a)
				{
					unsigned c = a;
					a = b;
					b = c;
				}

				SplitEdgesT e{a,b};
				if(!seList.find_sorted(e,index))
				{
					double pa[3],pb[3];
					getVertexCoords(a,pa);
					getVertexCoords(b,pb);

					int vNew = m_vertices.size();
					addVertex((pa[0]+pb[0])/2,(pa[1]+pb[1])/2,(pa[2]+pb[2])/2);
					m_vertices.back()->m_selected = true;
					e.vNew = vNew;
					seList.insert_sorted(e);
					verts[v] = vNew;
				}
				else verts[v] = seList[index].vNew;
			}

			addTriangle(m_triangles[t]->m_vertexIndices[1],verts[1],verts[0]);			
			addTriangle(m_triangles[t]->m_vertexIndices[2],verts[2],verts[1]);			
			addTriangle(verts[0],verts[1],verts[2]);			

			setTriangleVertices(t,m_triangles[t]->m_vertexIndices[0],verts[0],verts[2]);

			if(undo) undo->subdivide(t,tnew,tnew+1,tnew+2);

			for(int i=3;i-->0;) m_triangles[tnew++]->m_selected = true;
		}
	}
	if(undo)
	{
		m_undoEnabled = true;

		for(unsigned i=vertexStart;i<m_vertices.size();i++)
		undo->addVertex(i);
		sendUndo(undo);
	}	

	invalidateNormals(); //OVERKILL
}

//void Model::unsubdivideTriangles(unsigned t1, unsigned t2, unsigned t3, unsigned t4)
void Model::subdivideSelectedTriangles_undo(unsigned t1, unsigned t2, unsigned t3, unsigned t4)
{
	LOG_PROFILE();

	//if(m_animationMode) return; //REMOVE ME

	//if(displayFrameAnimPrimitiveError()) return; //REMOVE ME

	//This should implement a MU object or shouldn't be a regular API method.
	assert(!m_undoEnabled);

	bool swap = false; 
	std::swap(swap,m_undoEnabled);
	{
		setTriangleVertices(t1,
		m_triangles[t1]->m_vertexIndices[0],
		m_triangles[t2]->m_vertexIndices[0],
		m_triangles[t3]->m_vertexIndices[0]);

		deleteTriangle(t4);
		deleteTriangle(t3);
		deleteTriangle(t2);
	}
	std::swap(swap,m_undoEnabled);

	invalidateNormals(); //OVERKILL
}

void Model::operationComplete(const char *opname)
{
	validateSkel();
	validateAnim();
	endSelectionDifference();

	m_undoMgr->operationComplete(opname);
	
	//https://github.com/zturtleman/mm3d/issues/90
	//Treating operationComplete as-if calling updateObservers from outside.
	//updateObservers(false);
	updateObservers(!false);
}

bool Model::setUndoEnabled(bool o)
{
	bool old = m_undoEnabled;
	m_undoEnabled = o;
	return old;
}

void Model::clearUndo()
{
	m_undoMgr->clear();
}

bool Model::canUndo()const
{
	//if(m_animationMode)
	//{
	//	return m_animUndoMgr->canUndo();
	//}
	//else
	{
		return m_undoMgr->canUndo();
	}
}

bool Model::canRedo()const
{
	//if(m_animationMode)
	//{
	//	return m_animUndoMgr->canRedo();
	//}
	//else
	{
		return m_undoMgr->canRedo();
	}
}

const char *Model::getUndoOpName()const
{
	//if(m_animationMode)
	//{
	//	return m_animUndoMgr->getUndoOpName();
	//}
	//else
	{
		return m_undoMgr->getUndoOpName();
	}
}

const char *Model::getRedoOpName()const
{
	//if(m_animationMode)
	//{
	//	return m_animUndoMgr->getRedoOpName();
	//}
	//else
	{
		return m_undoMgr->getRedoOpName();
	}
}

void Model::undo()
{
	LOG_PROFILE();

	//UndoList *list = (m_animationMode)? m_animUndoMgr->undo(): m_undoMgr->undo();
	UndoList *list = m_undoMgr->undo();

	if(list)
	{
		log_debug("got atomic undo list\n");
		setUndoEnabled(false);

		// process back to front
		UndoList::reverse_iterator it;

		for(it = list->rbegin(); it!=list->rend(); it++)
		{
			ModelUndo *undo = static_cast<ModelUndo*>(*it);
			undo->undo(this);
		}

		validateSkel();

		updateObservers(false);

		setUndoEnabled(true);
	}

	m_selecting = false; //??? //endSelectionDifference?
}

void Model::redo()
{
	LOG_PROFILE();

	//UndoList *list = (m_animationMode)? m_animUndoMgr->redo(): m_undoMgr->redo();
	UndoList *list = m_undoMgr->redo();

	if(list)
	{
		log_debug("got atomic redo list\n");
		setUndoEnabled(false);

		// process front to back
		UndoList::iterator it;
		for(it = list->begin(); it!=list->end(); it++)
		{
			ModelUndo *undo = static_cast<ModelUndo*>((*it));
			undo->redo(this);
		}

		validateSkel();
		
		updateObservers(false);

		setUndoEnabled(true);
	}

	m_selecting = false; //??? //endSelectionDifference?
}

void Model::undoCurrent()
{
	LOG_PROFILE();

	//UndoList *list = (m_animationMode)? m_animUndoMgr->undoCurrent(): m_undoMgr->undoCurrent();
	UndoList *list = m_undoMgr->undoCurrent();

	if(list)
	{
		log_debug("got atomic undo list\n");
		setUndoEnabled(false);

		// process back to front
		UndoList::reverse_iterator it;
		for(it = list->rbegin(); it!=list->rend(); it++)
		{
			ModelUndo *undo = static_cast<ModelUndo*>((*it));
			undo->undo(this);
		}

		validateSkel();

		setUndoEnabled(true);

		//https://github.com/zturtleman/mm3d/issues/90
		//Treating operationComplete as-if calling updateObservers from outside.
		//updateObservers(false);
		updateObservers(!false);
	}

	m_selecting = false; //??? //endSelectionDifference?
}

bool Model::hideVertex(unsigned v)
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	if(v<m_vertices.size())
	{
		m_vertices[v]->m_visible = false;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::hideTriangle(unsigned t)
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	if(t<m_triangles.size())
	{
		m_triangles[t]->m_visible = false;

		return true;
	}
	else
	{
		return false;
	}
}

bool Model::hideJoint(unsigned j)
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	if(j<m_joints.size())
	{
		m_joints[j]->m_visible = false;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::hidePoint(unsigned p)
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	if(p<m_points.size())
	{
		m_points[p]->m_visible = false;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::unhideVertex(unsigned v)
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	if(v<m_vertices.size())
	{
		m_vertices[v]->m_visible = true;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::unhideTriangle(unsigned t)
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	if(t<m_triangles.size())
	{
		m_triangles[t]->m_visible = true;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::unhideJoint(unsigned j)
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	if(j<m_joints.size())
	{
		m_joints[j]->m_visible = true;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::unhidePoint(unsigned p)
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	if(p<m_points.size())
	{
		m_points[p]->m_visible = true;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::hideSelected()
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	unsigned t = 0;
	unsigned v = 0;

	// Need to track whether we are hiding a vertex and any triangles attached to it,
	// or hiding a triangle,and only vertices if they are orphaned
	for(v = 0; v<m_vertices.size(); v++)
	{
		if(m_vertices[v]->m_selected)
		{
			m_vertices[v]->m_marked = true;
		}
		else
		{
			m_vertices[v]->m_marked = false;
		}
	}

	// Hide selected triangles
	for(t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_selected)
		{
			m_triangles[t]->m_visible = false;
			m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_marked  = false;
			m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_marked  = false;
			m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_marked  = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectTriangles);
				undo->setHideDifference(t,m_triangles[t]->m_visible);
				sendUndo(undo);
			}
		}
	}

	// Hide triangles with a lone vertex that is selected
	for(t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_visible &&
			  (	m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_marked 
				||m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_marked 
				||m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_marked))
		{
			m_triangles[t]->m_visible = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectTriangles);
				undo->setHideDifference(t,m_triangles[t]->m_visible);
				sendUndo(undo);
			}
		}
	}

	// Find orphaned vertices
	for(v = 0; v<m_vertices.size(); v++)
	{
		m_vertices[v]->m_marked = true;
	}
	for(t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_visible)
		{
			// Triangle is visible,vertices must be too
			m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_marked = false;
			m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_marked = false;
			m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_marked = false;
		}
	}

	// Hide selected vertices
	for(v = 0; v<m_vertices.size(); v++)
	{
		if(m_vertices[v]->m_visible&&m_vertices[v]->m_marked)
		{
			m_vertices[v]->m_visible = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectVertices);
				undo->setHideDifference(v,m_vertices[v]->m_visible);
				sendUndo(undo);
			}
		}
	}

	for(unsigned j = 0; j<m_joints.size(); j++)
	{
		if(m_joints[j]->m_selected)
		{
			m_joints[j]->m_visible = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectJoints);
				undo->setHideDifference(j,m_joints[j]->m_visible);
				sendUndo(undo);
			}
		}
	}

	for(unsigned p = 0; p<m_points.size(); p++)
	{
		if(m_points[p]->m_selected)
		{
			m_points[p]->m_visible = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectPoints);
				undo->setHideDifference(p,m_points[p]->m_visible);
				sendUndo(undo);
			}
		}
	}

	unselectAll();

	return true;
}

bool Model::hideUnselected()
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	unsigned t = 0;
	unsigned v = 0;

	// Need to track whether we are hiding a vertex and any triangles attached to it,
	// or hiding a triangle,and only vertices if they are orphaned
	// We could be doing both at the same
	for(v = 0; v<m_vertices.size(); v++)
	{
		if(m_vertices[v]->m_selected)
		{
			m_vertices[v]->m_marked = false;
		}
		else
		{
			m_vertices[v]->m_marked = true;
		}
	}

	// Hide triangles with any unselected vertices
	for(t = 0; t<m_triangles.size(); t++)
	{
		if(!m_triangles[t]->m_selected)
		{
			log_debug("triangle %d is unselected,hiding\n",t);
			m_triangles[t]->m_visible = false;
			m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_marked  = false;
			m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_marked  = false;
			m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_marked  = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectTriangles);
				undo->setHideDifference(t,m_triangles[t]->m_visible);
				sendUndo(undo);
			}
		}
	}

	// Find orphaned vertices
	for(v = 0; v<m_vertices.size(); v++)
	{
		m_vertices[v]->m_marked = true;
	}
	for(t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_visible)
		{
			// Triangle is visible,vertices must be too
			m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_marked = false;
			m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_marked = false;
			m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_marked = false;
		}
	}

	// Hide selected vertices
	for(v = 0; v<m_vertices.size(); v++)
	{
		if(m_vertices[v]->m_visible&&m_vertices[v]->m_marked)
		{
			log_debug("vertex %d is visible and marked,hiding\n",v);
			m_vertices[v]->m_visible = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectVertices);
				undo->setHideDifference(v,m_vertices[v]->m_visible);
				sendUndo(undo);
			}
		}
	}

	for(unsigned j = 0; j<m_joints.size(); j++)
	{
		if(m_joints[j]->m_visible&&!m_joints[j]->m_selected)
		{
			m_joints[j]->m_visible = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectJoints);
				undo->setHideDifference(j,m_joints[j]->m_visible);
				sendUndo(undo);
			}
		}
	}

	for(unsigned p = 0; p<m_points.size(); p++)
	{
		if(m_points[p]->m_visible&&!m_points[p]->m_selected)
		{
			m_points[p]->m_visible = false;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectPoints);
				undo->setHideDifference(p,m_points[p]->m_visible);
				sendUndo(undo);
			}
		}
	}

	unselectAll();

	return true;
}

bool Model::unhideAll()
{
	LOG_PROFILE();

	//if(m_animationMode) return false; //REMOVE ME

	for(unsigned v = 0; v<m_vertices.size(); v++)
	{
		if(!m_vertices[v]->m_visible)
		{
			m_vertices[v]->m_visible = true;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectVertices);
				undo->setHideDifference(v,true);
				sendUndo(undo);
			}
		}
	}
	for(unsigned t = 0; t<m_triangles.size(); t++)
	{
		if(!m_triangles[t]->m_visible)
		{
			m_triangles[t]->m_visible = true;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectTriangles);
				undo->setHideDifference(t,true);
				sendUndo(undo);
			}
		}
	}
	for(unsigned j = 0; j<m_joints.size(); j++)
	{
		if(!m_joints[j]->m_visible)
		{
			m_joints[j]->m_visible = true;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectJoints);
				undo->setHideDifference(j,true);
				sendUndo(undo);
			}
		}
	}
	for(unsigned p = 0; p<m_points.size(); p++)
	{
		if(!m_points[p]->m_visible)
		{
			m_points[p]->m_visible = true;

			if(m_undoEnabled)
			{
				auto undo = new MU_Hide(SelectPoints);
				undo->setHideDifference(p,true);
				sendUndo(undo);
			}
		}
	}

	return true;
}

void Model::hideTrianglesFromVertices()
{
	LOG_PROFILE();

	// Hide triangles with at least one vertex hidden
	for(unsigned t = 0; t<m_triangles.size(); t++)
	{
		if(	!m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_visible
			  ||!m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_visible
			  ||!m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_visible
		  )
		{
			m_triangles[t]->m_visible = false;
		}
	}
}

void Model::unhideTrianglesFromVertices()
{
	LOG_PROFILE();

	// Hide triangles with at least one vertex hidden
	for(unsigned t = 0; t<m_triangles.size(); t++)
	{
		if(	m_vertices[m_triangles[t]->m_vertexIndices[0]]->m_visible
			  &&m_vertices[m_triangles[t]->m_vertexIndices[1]]->m_visible
			  &&m_vertices[m_triangles[t]->m_vertexIndices[2]]->m_visible
		  )
		{
			m_triangles[t]->m_visible = true;
		}
	}
}

void Model::hideVerticesFromTriangles()
{
	LOG_PROFILE();

	// Hide vertices with all triangles hidden
	unsigned t;

	for(t = 0; t<m_vertices.size(); t++)
	{
		m_vertices[t]->m_visible = false;
	}

	for(t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_visible)
		{
			for(int v = 0; v<3; v++)
			{
				m_vertices[m_triangles[t]->m_vertexIndices[v]]->m_visible = true;
			}
		}
	}
}

void Model::unhideVerticesFromTriangles()
{
	LOG_PROFILE();

	// Unhide vertices with at least one triangle visible
	for(unsigned t = 0; t<m_triangles.size(); t++)
	{
		if(m_triangles[t]->m_visible)
		{
			for(int v = 0; v<3; v++)
			{
				m_vertices[m_triangles[t]->m_vertexIndices[v]]->m_visible = true;
			}
		}
	}
}

bool Model::getBoundingRegion(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const
{
	//REMOVE ME: Duplicate of getSelectedBoundingRegion

	if(!x1||!y1||!z1||!x2||!y2||!z2) return false; //???

	validateAnim(); //2020

	int visible = 0;
	bool havePoint = false; //REMOVE ME
	*x1 = *y1 = *z1 = *x2 = *y2 = *z2 = 0.0;

	for(unsigned v = 0; v<m_vertices.size(); v++)		
	{
		if(!m_vertices[v]->m_visible) continue;

		visible++;

		if(!havePoint) //???
		{
			havePoint = true;

			*x1 = *x2 = m_vertices[v]->m_absSource[0];
			*y1 = *y2 = m_vertices[v]->m_absSource[1];
			*z1 = *z2 = m_vertices[v]->m_absSource[2];
		}
		else //???
		{
			if(m_vertices[v]->m_absSource[0]<*x1)
			{
				*x1 = m_vertices[v]->m_absSource[0];
			}
			if(m_vertices[v]->m_absSource[0]>*x2)
			{
				*x2 = m_vertices[v]->m_absSource[0];
			}
			if(m_vertices[v]->m_absSource[1]<*y1)
			{
				*y1 = m_vertices[v]->m_absSource[1];
			}
			if(m_vertices[v]->m_absSource[1]>*y2)
			{
				*y2 = m_vertices[v]->m_absSource[1];
			}
			if(m_vertices[v]->m_absSource[2]<*z1)
			{
				*z1 = m_vertices[v]->m_absSource[2];
			}
			if(m_vertices[v]->m_absSource[2]>*z2)
			{
				*z2 = m_vertices[v]->m_absSource[2];
			}
		}
	}

	for(unsigned j = 0; j<m_joints.size(); j++)
	{
		double coord[3];
		m_joints[j]->m_final.getTranslation(coord);

		if(havePoint)
		{
			if(coord[0]<*x1)
			{
				*x1 = coord[0];
			}
			if(coord[0]>*x2)
			{
				*x2 = coord[0];
			}
			if(coord[1]<*y1)
			{
				*y1 = coord[1];
			}
			if(coord[1]>*y2)
			{
				*y2 = coord[1];
			}
			if(coord[2]<*z1)
			{
				*z1 = coord[2];
			}
			if(coord[2]>*z2)
			{
				*z2 = coord[2];
			}
		}
		else
		{
			*x1 = *x2 = coord[0];
			*y1 = *y2 = coord[1];
			*z1 = *z2 = coord[2];
			havePoint = true;
		}

		visible++;
	}

	for(unsigned p = 0; p<m_points.size(); p++)
	{
		double coord[3];
		coord[0] = m_points[p]->m_absSource[0];
		coord[1] = m_points[p]->m_absSource[1];
		coord[2] = m_points[p]->m_absSource[2];

		if(havePoint)
		{
			if(coord[0]<*x1)
			{
				*x1 = coord[0];
			}
			if(coord[0]>*x2)
			{
				*x2 = coord[0];
			}
			if(coord[1]<*y1)
			{
				*y1 = coord[1];
			}
			if(coord[1]>*y2)
			{
				*y2 = coord[1];
			}
			if(coord[2]<*z1)
			{
				*z1 = coord[2];
			}
			if(coord[2]>*z2)
			{
				*z2 = coord[2];
			}
		}
		else
		{
			*x1 = *x2 = coord[0];
			*y1 = *y2 = coord[1];
			*z1 = *z2 = coord[2];
			havePoint = true;
		}

		visible++;
	}

	for(unsigned p = 0; p<m_projections.size(); p++)
	{
		double coord[3];
		//double scale = mag3(m_projections[p]->m_upVec);
		double scale = m_projections[p]->m_xyz[0];
		double *pos = m_projections[p]->m_abs;

		coord[0] = pos[0]+scale;
		coord[1] = pos[1]+scale;
		coord[2] = pos[2]+scale;

		if(havePoint)
		{
			if(coord[0]<*x1)
			{
				*x1 = coord[0];
			}
			if(coord[0]>*x2)
			{
				*x2 = coord[0];
			}
			if(coord[1]<*y1)
			{
				*y1 = coord[1];
			}
			if(coord[1]>*y2)
			{
				*y2 = coord[1];
			}
			if(coord[2]<*z1)
			{
				*z1 = coord[2];
			}
			if(coord[2]>*z2)
			{
				*z2 = coord[2];
			}
		}
		else
		{
			*x1 = *x2 = coord[0];
			*y1 = *y2 = coord[1];
			*z1 = *z2 = coord[2];
			havePoint = true;
		}

		coord[0] = pos[0]-scale;
		coord[1] = pos[1]-scale;
		coord[2] = pos[2]-scale;

		if(coord[0]<*x1)
		{
			*x1 = coord[0];
		}
		if(coord[0]>*x2)
		{
			*x2 = coord[0];
		}
		if(coord[1]<*y1)
		{
			*y1 = coord[1];
		}
		if(coord[1]>*y2)
		{
			*y2 = coord[1];
		}
		if(coord[2]<*z1)
		{
			*z1 = coord[2];
		}
		if(coord[2]>*z2)
		{
			*z2 = coord[2];
		}

		visible++;
	}

	return visible!=0;
}

void Model::invertNormals(unsigned triangleNum)
{
	LOG_PROFILE();

	if(triangleNum<m_triangles.size())
	{
		auto *tn = m_triangles[triangleNum];

		bool swap = false; 
		std::swap(swap,m_undoEnabled);
		{
			//NOTE: This will change the high 2 bits of Vertex::m_faces
			//std::swap(tn->m_vertexIndices[0],tn->m_vertexIndices[2]);
			setTriangleVertices(triangleNum,tn->m_vertexIndices[2],tn->m_vertexIndices[1],tn->m_vertexIndices[0]);
			std::swap(tn->m_s[0],tn->m_s[2]);
			std::swap(tn->m_t[0],tn->m_t[2]);
		}
		if(swap)
		{
			m_undoEnabled = true;

			MU_InvertNormal *undo = new MU_InvertNormal();
			undo->addTriangle(triangleNum);
			sendUndo(undo);
		}

		invalidateNormals(); //OVERKILL
	}
}

bool Model::triangleFacesIn(unsigned triangleNum)
{
	validateNormals();

	unsigned int tcount = m_triangles.size();
	if(triangleNum<tcount)
	{
		int inFront = 0;
		int inBack  = 0;
		int sideInFront  = 0;
		int sideInBack	= 0;

		double p1[3] = {};
		double p2[3] = {};

		Triangle *tri = m_triangles[triangleNum];
		for(int i = 0; i<3; i++)
		{
			for(int j = 0; j<3; j++)
			{
				p1[i] += m_vertices[tri->m_vertexIndices[j]]->m_coord[i];
			}
			p1[i] /= 3.0;
		}

		double norm[3] = {};
		getFlatNormal(triangleNum,norm);

		p2[0] = norm[0]+p1[0];
		p2[1] = norm[1]+p1[1];
		p2[2] = norm[2]+p1[2];

		for(unsigned int t = 0; t<tcount; t++)
		{
			double tpoint[3][3];
			double tnorm[3];
			getVertexCoords(m_triangles[t]->m_vertexIndices[0],tpoint[0]);
			getFlatNormal(t,norm);

			tnorm[0] = norm[0];
			tnorm[1] = norm[1];
			tnorm[2] = norm[2];

			if(t!=triangleNum)
			{
				double ipoint[3] = { 0,0,0 };
				int val = model_findEdgePlaneIntersection(ipoint,p1,p2,tpoint[0],tnorm);

				if(val!=0)
				{
					getVertexCoords(m_triangles[t]->m_vertexIndices[1],tpoint[1]);
					getVertexCoords(m_triangles[t]->m_vertexIndices[2],tpoint[2]);

					int inTri = model_pointInTriangle(ipoint,tpoint[0],tpoint[1],tpoint[2]);

					if(inTri>=0)
					{
						if(val>0)
						{
							if(inTri==0)
							{
								sideInFront++;
							}
							else
							{
								inFront++;
							}
						}
						else
						{
							if(inTri==0)
							{
								sideInBack++;
							}
							else
							{
								inBack++;
							}
						}
					}
				}
			}
		}

		if((sideInFront &1)==0)
		{
			inFront += sideInFront/2;
		}
		if((sideInBack &1)==0)
		{
			inBack += sideInBack/2;
		}

		//log_debug("front = %d	 back = %d\n",inFront,inBack);

		return ((inFront &1)!=0)&&((inBack &1)==0);
	}

	return false;
}

void Model::sendUndo(Undo *undo, bool listCombine)
{
	if(!undo) return; //2020

	if(m_undoEnabled)
	{
		//if(m_animationMode)
		//{
		//	m_animUndoMgr->addUndo(undo);
		//}
		//else
		{
			m_undoMgr->addUndo(undo,listCombine);
		}
	}
	else //YUCK (ANTI-PATTERN)
	{
		undo->undoRelease();
		undo->release();
	}
}
void Model::appendUndo(Undo *undo)
{
	//NOTE: This is a fix for setSelectedUv.
	//It does no good to undo the UV selection
	//before the operation that did the selecting.

	if(m_undoEnabled&&*m_undoMgr->getUndoOpName())
	{
		auto &st = m_undoMgr->getUndoStack();
		if(!st.empty()) st.back()->push_back(undo);
		return;
	}
	
	sendUndo(undo,false);
}

/*REFERENCE
// Show an error because the user tried to add or remove primitives while
// the model has frame animations.
bool Model::displayFrameAnimPrimitiveError()
{
	//https://github.com/zturtleman/mm3d/issues/87

	if(1) return false; //I think this is solved.

	//REMINDER: Extend frame vectors if necessary.
	if(m_frameAnims.size()>0&&!m_forceAddOrDelete)
	model_status(this,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel","Cannot add or delete because you have frame animations.  Try \"Merge...\" instead."));
	else return false; return true;
}
void Model::forceAddOrDelete(bool o)
{
	if(!o&&m_forceAddOrDelete) clearUndo();

	m_forceAddOrDelete = o;
}*/

#endif // MM3D_EDIT

int Model::addFormatData(FormatData *fd)
{
	m_changeBits |= AddOther;

	int n = -1;
	if(fd)
	{
		n = m_formatData.size();
		m_formatData.push_back(fd);
	}

	return n;
}

bool Model::deleteFormatData(unsigned index)
{
	std::vector<FormatData*>::iterator it;

	for(it = m_formatData.begin(); it!=m_formatData.end(); it++)
	{
		while(index&&it!=m_formatData.end())
		{
			it++;
			index--;
		}

		if(it!=m_formatData.end())
		{
			delete (*it);
			m_formatData.erase(it);
			return true;
		}
	}

	return false;
}

unsigned Model::getFormatDataCount()const
{
	return m_formatData.size();
}

Model::FormatData *Model::getFormatData(unsigned index)const
{
	if(index<m_formatData.size())
	{
		return m_formatData[index];
	}
	return nullptr;
}

Model::FormatData *Model::getFormatDataByFormat(const char *format, unsigned index)const
{
	unsigned count = m_formatData.size();
	for(unsigned n = 0; n<count; n++)
	{
		FormatData *fd = m_formatData[n];

		if(PORT_strcasecmp(fd->format.c_str(),format)==0&&fd->index==index)
		{
			return fd;
		}
	}

	return nullptr;
}

bool Model::setBoneJointParent(unsigned joint, int parent)
{
	if(joint<m_joints.size()&&parent>=-1)
	{
		if(m_undoEnabled)
		{
			auto undo = new MU_SetJointParent;
			undo->setJointParent(joint,parent,m_joints[joint]->m_parent);
			sendUndo(undo);
		}

		m_joints[joint]->m_parent = parent;

		invalidateSkel();

		return true;
	}
	else return false;
}

int Model::getBoneJointParent(unsigned j)const
{
	return j<m_joints.size()?m_joints[j]->m_parent:-1;
}

int Model::getPointByName(const char *name)const
{
	for(unsigned int point = 0; point<m_points.size(); point++)
	{
		if(strcmp(name,m_points[point]->m_name.c_str())==0)
		{
			return point;
		}
	}
	return -1;
}

bool Model::getVertexCoords(unsigned vertexNumber, double *coord)const
{
	if(coord&&vertexNumber<m_vertices.size())
	{
		for(int t = 0; t<3; t++)
		{
			coord[t] = m_vertices[vertexNumber]->m_absSource[t];
		}
		return true;
	}
	return false;
}

bool Model::getVertexCoordsUnanimated(unsigned vertexNumber, double *coord)const
{
	if(coord&&vertexNumber<m_vertices.size())
	{
		for(int t = 0; t<3; t++)
		{
			coord[t] = m_vertices[vertexNumber]->m_coord[t];
		}
		return true;
	}
	return false;
}

/*2019: Removing to implui/texturecoord.cc to eliminate ProjectionDirectionE::ViewRight/Left.
bool Model::getVertexCoords2d(unsigned vertexNumber,ProjectionDirectionE dir, double *coord)const
{
	if(coord&&vertexNumber>=0&&(unsigned)vertexNumber<m_vertices.size())
	{
		Vertex *vert = m_vertices[vertexNumber];
		switch (dir)
		{
			case ViewFront:
				coord[0] =  vert->m_coord[0];
				coord[1] =  vert->m_coord[1];
				break;
			case ViewBack:
				coord[0] = -vert->m_coord[0];
				coord[1] =  vert->m_coord[1];
				break;
			case ViewLeft:
				coord[0] = -vert->m_coord[2];
				coord[1] =  vert->m_coord[1];
				break;
			case ViewRight:
				coord[0] =  vert->m_coord[2];
				coord[1] =  vert->m_coord[1];
				break;
			case ViewTop:
				coord[0] =  vert->m_coord[0];
				coord[1] = -vert->m_coord[2];
				break;
			case ViewBottom:
				coord[0] =  vert->m_coord[0];
				coord[1] =  vert->m_coord[2];
				break;

			// Not an orthogonal view
			default:
				return false;
				break;
		}
		return true;
	}
	else
	{
		return false;
	}
}*/

bool Model::getBoneJointFinalMatrix(unsigned jointNumber,Matrix &m)const
{
	if(jointNumber<m_joints.size())
	{
		//scaletool is using this. Better to just copy I think.
		//HACK? Need to update m_final matrix.
		//validateAnimSkel();

		m = m_joints[jointNumber]->m_final;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::getBoneJointAbsoluteMatrix(unsigned jointNumber,Matrix &m)const
{
	if(jointNumber<m_joints.size())
	{
		m = m_joints[jointNumber]->m_absolute;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::getBoneJointRelativeMatrix(unsigned jointNumber,Matrix &m)const
{
	if(jointNumber<m_joints.size())
	{
		m = m_joints[jointNumber]->m_relative;
		return true;
	}
	else
	{
		return false;
	}
}

bool Model::getPointFinalMatrix(unsigned pointNumber, Matrix &m)const
{
	if(pointNumber<m_points.size())
	{
		Matrix mat;
		Point *p = m_points[pointNumber];

		mat.setTranslation(p->m_absSource);
		mat.setRotation(p->m_rotSource);

		m = mat * m_localMatrix;

		return true;
	}
	return false;
}

bool Model::getPointAbsoluteMatrix(unsigned pointNumber,Matrix &m)const
{
	if(pointNumber<m_points.size())
	{
		Matrix mat;
		Point *p = m_points[pointNumber];

		mat.setTranslation(p->m_abs);
		mat.setRotation(p->m_rot);

		m = mat *m_localMatrix;

		return true;
	}
	return false;
}

int Model::getTriangleVertex(unsigned triangleNumber, unsigned vertexIndex)const
{
	if(triangleNumber<m_triangles.size()&&vertexIndex<3)
	{
		return m_triangles[triangleNumber]->m_vertexIndices[vertexIndex];
	}
	else
	{
		return -1;
	}
}

bool Model::getNormal(unsigned triangleNum, unsigned vertexIndex, double *normal)const
{
	if(triangleNum<m_triangles.size()&&vertexIndex<3)
	{
		for(int t = 0; t<3; t++)
		{
			normal[t] = m_triangles[triangleNum]->m_normalSource[vertexIndex][t];
		}
		return true;
	}
	return false;
}
bool Model::getNormalUnanimated(unsigned triangleNum, unsigned vertexIndex, double *normal)const
{
	if(triangleNum<m_triangles.size()&&vertexIndex<3)
	{
		for(int t = 0; t<3; t++)
		{
			//normal[t] = m_triangles[triangleNum]->m_vertexNormals[vertexIndex][t];
			normal[t] = m_triangles[triangleNum]->m_finalNormals[vertexIndex][t];
		}
		return true;
	}
	return false;
}

bool Model::getFlatNormal(unsigned t, double *normal)const
{
	if(t>=m_triangles.size()) return false;
	
	auto *tri = m_triangles[t];

	//TODO: SHOULD PROBABLY validateNormals AND NOT CALCULATE
	//TO BE CONSISTENT WITH getNormal.
	//https://github.com/zturtleman/mm3d/issues/116
	if(m_animationMode?m_validAnimNormals:m_validNormals)
	{
		//2020: Don't calculate if valid.
		for(int i=0;i<3;i++) 
		normal[i] = tri->m_flatSource[i];
		return true;
	}	

	double *v0 = m_vertices[tri->m_vertexIndices[0]]->m_absSource;
	double *v1 = m_vertices[tri->m_vertexIndices[1]]->m_absSource;
	double *v2 = m_vertices[tri->m_vertexIndices[2]]->m_absSource;

	calculate_normal(normal,v0,v1,v2); return true;
}
bool Model::getFlatNormalUnanimated(unsigned t, double *normal)const
{
	if(t>=m_triangles.size()) return false;
	
	auto *tri = m_triangles[t];

	//TODO: SHOULD PROBABLY validateNormals AND NOT CALCULATE
	//TO BE CONSISTENT WITH getNormal.
	//https://github.com/zturtleman/mm3d/issues/116
	if(m_validNormals)
	{
		//2020: Don't calculate if valid.
		for(int i=0;i<3;i++)
		normal[i] = tri->m_flatNormals[i];
		return true;
	}

	double *v0 = m_vertices[tri->m_vertexIndices[0]]->m_coord;
	double *v1 = m_vertices[tri->m_vertexIndices[1]]->m_coord;
	double *v2 = m_vertices[tri->m_vertexIndices[2]]->m_coord;

	calculate_normal(normal,v0,v1,v2); return true;
}

bool Model::getBoneVector(unsigned joint, double *vec, const double *coord)const
{
	if(joint>=m_joints.size())
	{
		return false;
	}

	double jcoord[3] = { 0,0,0 };
	getBoneJointCoords(joint,jcoord);

	double cdist = 0.0;
	int child = -1;
	int bcount = m_joints.size();

	// find best child bone joint vector based on the min distance between
	// the target coordinate and the child bone joint coordinate
	for(int b = 0; b<bcount; b++)
	{
		if(getBoneJointParent(b)==(int)joint)
		{
			double ccoord[3];
			getBoneJointCoords(b,ccoord);
			double d = distance(ccoord,coord);

			if(child<0||d<cdist)
			{
				child = b;
				cdist = d;
				vec[0] = ccoord[0]-jcoord[0];
				vec[1] = ccoord[1]-jcoord[1];
				vec[2] = ccoord[2]-jcoord[2];
			}
		}
	}

	if(child<0)
	{
		int parent = getBoneJointParent(joint);
		if(parent<0)
		{
			// no children,no parent
			// return max influence
			return false;
		}

		getBoneJointCoords(parent,vec);

		cdist = distance(vec,coord);

		// We're using the parent instead of the child,so invert the direction
		vec[0] = jcoord[0]-vec[0];
		vec[1] = jcoord[1]-vec[1];
		vec[2] = jcoord[2]-vec[2];
	}

	normalize3(vec);
	return true;
}

void Model::Vertex::_erase_face(Triangle *f, unsigned s)
{
	m_faces.erase(std::find(m_faces.begin(),m_faces.end(),std::make_pair(f,s)));
}
void Model::calculateNormals()
{
	//CAUTION: I've changed this to fill out the animation "source" normals
	//when selected.

	LOG_PROFILE();

	//INSANITY
	//Note: I've used static to avoid reallocations but it's not threadsafe
	//static std::vector<std::vector<NormAngleAccum>> acl_normmap;	
	//acl_normmap.resize(std::max(acl_normmap.size(),m_vertices.size()));
	//for(size_t i=m_vertices.size();i-->0;) acl_normmap[i].clear();

	for(Triangle*tri:m_triangles) // accumulate normals
	{
		tri->m_marked = false;

		//FIX ME
		//I think the area calculation should be factored into the 
		//weights but historically that wasn't the case. For right
		//this moment this code is too complicated.
		//https://github.com/zturtleman/mm3d/issues/109
		/*
		float x1 = m_vertices[tri->m_vertexIndices[0]]->m_absSource[0];
		float y1 = m_vertices[tri->m_vertexIndices[0]]->m_absSource[1];
		float z1 = m_vertices[tri->m_vertexIndices[0]]->m_absSource[2];
		float x2 = m_vertices[tri->m_vertexIndices[1]]->m_absSource[0];
		float y2 = m_vertices[tri->m_vertexIndices[1]]->m_absSource[1];
		float z2 = m_vertices[tri->m_vertexIndices[1]]->m_absSource[2];
		float x3 = m_vertices[tri->m_vertexIndices[2]]->m_absSource[0];
		float y3 = m_vertices[tri->m_vertexIndices[2]]->m_absSource[1];
		float z3 = m_vertices[tri->m_vertexIndices[2]]->m_absSource[2];

		//Newell's Method for triangles?
		//https://github.com/zturtleman/mm3d/issues/115
		float A = y1 *(z2-z3)+y2 *(z3-z1)+y3 *(z1-z2);
		float B = z1 *(x2-x3)+z2 *(x3-x1)+z3 *(x1-x2);
		float C = x1 *(y2-y3)+x2 *(y3-y1)+x3 *(y1-y2);

		// Get flat normal
		float len = sqrt((A *A)+(B *B)+(C *C));

		A = A/len;
		B = B/len;
		C = C/len;

		tri->m_flatSource[0] = A;
		tri->m_flatSource[1] = B;
		tri->m_flatSource[2] = C;

		// Accumulate for smooth normal,weighted by face angle
		for(int vert = 0; vert<3; vert++)
		{
			unsigned index = tri->m_vertexIndices[vert];
			std::vector<NormAngleAccum>&acl = acl_normmap[index];

		/*UNUSED //https://github.com/zturtleman/mm3d/issues/109
			float ax = 0.0f;
			float ay = 0.0f;
			float az = 0.0f;
			float bx = 0.0f;
			float by = 0.0f;
			float bz = 0.0f;

			switch (vert)
			{
			case 0:
				ax = x2-x1;
				ay = y2-y1;
				az = z2-z1;
				bx = x3-x1;
				by = y3-y1;
				bz = z3-z1;
				break;
			case 1:
				ax = x1-x2;
				ay = y1-y2;
				az = z1-z2;
				bx = x3-x2;
				by = y3-y2;
				bz = z3-z2;
				break;
			case 2:
				ax = x1-x3;
				ay = y1-y3;
				az = z1-z3;
				bx = x2-x3;
				by = y2-y3;
				bz = z2-z3;
				break;
			}

			float ad = sqrt(ax*ax+ay*ay+az*az);
			float bd = sqrt(bx*bx+by*by+bz*bz);
	

			NormAngleAccum aacc;
			aacc.norm[0] = A;
			aacc.norm[1] = B;
			aacc.norm[2] = C;
			aacc.angle	= fabs(acos((ax*bx+ay*by+az*bz)/(ad *bd))); //UNUSED

			acl.push_back(aacc);
		}
		NormAngleAccum aacc;*/		
		double *v0 = m_vertices[tri->m_vertexIndices[0]]->m_absSource;
		double *v1 = m_vertices[tri->m_vertexIndices[1]]->m_absSource;
		double *v2 = m_vertices[tri->m_vertexIndices[2]]->m_absSource;
		/*REFERENCE
		calculate_normal(aacc.norm,v0,v1,v2);
		for(int i=0;i<3;i++)
		{
			tri->m_flatSource[i] = aacc.norm[i];
			acl_normmap[tri->m_vertexIndices[i]].push_back(aacc);
		}*/
		calculate_normal(tri->m_flatSource,v0,v1,v2);

		//New angle code (assuming degenerate values don't matter)
		double a[3][3],dp[3] = {};
		normalize3(delta3(a[0],v1,v0));
		normalize3(delta3(a[1],v2,v1));
		normalize3(delta3(a[2],v0,v2));
		for(int i=3;i-->0;)
		{
			dp[0]+=a[0][i]*-a[2][i];
			dp[1]+=a[1][i]*-a[0][i];
			dp[2]+=a[2][i]*-a[1][i];
		}
		for(int i=3;i-->0;) 
		tri->m_angleSource[i] = acos(dp[i]);
	}

	// Apply accumulated normals to triangles

	for(unsigned g = 0; g<m_groups.size(); g++)
	{
		Group *grp = m_groups[g];

		float maxAngle = grp->m_angle;
		if(maxAngle<0.50f)
		{
			maxAngle = 0.50f;
		}
		maxAngle *= PIOVER180;

		for(int i:grp->m_triangleIndices)
		{
			Triangle *tri = m_triangles[i];
			tri->m_marked = true;
			for(int vert=3;vert-->0;)
			{
				unsigned v = tri->m_vertexIndices[vert];
								
				//std::vector<NormAngleAccum> &acl = acl_normmap[v];
				auto &acl = m_vertices[v]->m_faces;

				float A = 0;
				float B = 0;
				float C = 0; for(auto&ea:acl)
				{
					//float dotprod = dot3(tri->m_flatSource,ea.norm);
					//auto tri2 = m_triangles[ea&0x3fffffff];
					auto tri2 = ea.first;
					auto ea_norm = tri2->m_flatSource;
					float dotprod = dot3(tri->m_flatSource,ea_norm);

					// Don't allow it to go over 1.0f
					float angle = 0.0f;
					if(dotprod<0.99999f)
					{
						angle = fabs(acos(dotprod));
					}

					//float w = tri2->m_angleSource[ea>>30];
					float w = tri2->m_angleSource[ea.second];

					if(angle<=maxAngle)
					{
						A += ea_norm[0]*w;
						B += ea_norm[1]*w;
						C += ea_norm[2]*w;
					}
				}

				float len = magnitude(A,B,C);

				if(len>=0.0001f)
				{
					tri->m_normalSource[vert][0] = A/len;
					tri->m_normalSource[vert][1] = B/len;
					tri->m_normalSource[vert][2] = C/len;
				}
				else for(int i=3;i-->0;)
				{
					tri->m_normalSource[vert][i] = tri->m_flatSource[i];
				}				
			}
		}
	}

	for(Triangle*tri:m_triangles) if(!tri->m_marked)
	{
		for(int vert=3;vert-->0;)
		{
			unsigned v = tri->m_vertexIndices[vert];
			
			//std::vector<NormAngleAccum> &acl = acl_normmap[v];
			auto &acl = m_vertices[v]->m_faces;

			float A = 0;
			float B = 0;
			float C = 0; for(auto&ea:acl)
			{
				//float dotprod = dot3(tri->m_flatSource,ea.norm);
				//auto tri2 = m_triangles[ea&0x3fffffff];
				auto tri2 = ea.first;
				auto ea_norm = tri2->m_flatSource;
				float dotprod = dot3(tri->m_flatSource,ea_norm);

				// Don't allow it to go over 1.0f
				float angle = 0.0f;
				if(dotprod<0.99999f)
				{
					angle = fabs(acos(dotprod));
				}

				//float w = tri2->m_angleSource[ea>>30];
				float w = tri2->m_angleSource[ea.second];

				if(angle<=45.0f *PIOVER180)
				{
					A += ea_norm[0]*w;
					B += ea_norm[1]*w;
					C += ea_norm[2]*w;
				}
			}

			float len = magnitude(A,B,C);

			if(len>=0.0001f)
			{
				tri->m_normalSource[vert][0] = A/len;
				tri->m_normalSource[vert][1] = B/len;
				tri->m_normalSource[vert][2] = C/len;
			}
			else for(int i=3;i-->0;)
			{
				tri->m_normalSource[vert][i] = tri->m_flatSource[i];
			}			
		}
	}
	else tri->m_marked = false; //???

	for(Group*grp:m_groups)
	{
		double percent = grp->m_smooth/255.0;
		for(int i:grp->m_triangleIndices)
		{
			Triangle *tri = m_triangles[i];

			for(int v=3;v-->0;) if(grp->m_smooth>0)
			{
				for(int i=3;i-->0;)
				tri->m_normalSource[v][i] = tri->m_flatSource[i]
				+(tri->m_normalSource[v][i]-tri->m_flatSource[i])*percent;
				normalize3(tri->m_normalSource[v]);
			}
			else for(int i=3;i-->0;)
			{
				tri->m_normalSource[v][i] = tri->m_flatSource[i];
			}
		}
	}

	(m_animationMode?m_validAnimNormals:m_validNormals) = true;

	m_validBspTree = false;
}

void Model::invalidateNormals()
{
	//NOTE: This is a HACK as near as I can tell.
	//It's true that when normals are invalidated
	//geometry has changed.
	m_changeBits |= MoveGeometry;

	if(m_animationMode) m_validAnimNormals = false;
	
	m_validNormals = false;
	m_validBspTree = false;
}
void Model::invalidateAnimNormals() //UNUSED (USE ME)
{
	//m_changeBits |= MoveGeometry; //???

	m_validAnimNormals = false; //!!
	m_validBspTree = false;
}

bool Model::validateNormals()const
{
	validateAnim();

	if(m_animationMode?m_validAnimNormals:m_validNormals)
	return false;

	const_cast<Model*>(this)->calculateNormals(); return true;
}

void Model::calculateBspTree()
{
	log_debug("calculating BSP tree\n");
	m_bspTree.clear();

	for(unsigned m = 0; m<m_groups.size(); m++)
	{
		Group *grp = m_groups[m];
		if(grp->m_materialIndex>=0)
		{
			int index = grp->m_materialIndex;

			if(m_materials[index]->m_type==Model::Material::MATTYPE_TEXTURE
				  &&m_materials[index]->m_textureData->m_format==Texture::FORMAT_RGBA)
			{
				for(int ti:grp->m_triangleIndices)
				{
					Triangle *triangle = m_triangles[ti];
					triangle->m_marked = true;

					BspTree::Poly *poly = BspTree::Poly::get();
					
					for(int i=0;i<3;i++)
					{
						poly->coord[0][i] = m_vertices[triangle->m_vertexIndices[0]]->m_absSource[i];
						poly->coord[1][i] = m_vertices[triangle->m_vertexIndices[1]]->m_absSource[i];
						poly->coord[2][i] = m_vertices[triangle->m_vertexIndices[2]]->m_absSource[i];

						poly->drawNormals[0][i] = triangle->m_normalSource[0][i];
						poly->drawNormals[1][i] = triangle->m_normalSource[1][i];
						poly->drawNormals[2][i] = triangle->m_normalSource[2][i];

						poly->norm[i] = triangle->m_flatSource[i];
					}

					for(int i = 0; i<3; i++)
					{
						poly->s[i] = triangle->m_s[i];
						poly->t[i] = triangle->m_t[i];
					}
					poly->texture = index;
					poly->material = static_cast<void*>(m_materials[index]);
					poly->triangle = static_cast<void*>(triangle);
					poly->calculateD();
					m_bspTree.addPoly(poly);
				}
			}
		}
	}

	m_validBspTree = true;
}

void Model::invalidateBspTree()
{
	m_validBspTree = false;
}

bool Model::isTriangleMarked(unsigned int t)const
{
	if(t<m_triangles.size())
	{
		return m_triangles[t]->m_userMarked;
	}
	return false;
}

void Model::setTriangleMarked(unsigned t,bool marked)
{
	if(t<m_triangles.size())
	{
		m_triangles[t]->m_userMarked = marked;
	}
}

void Model::clearMarkedTriangles()
{
	unsigned tcount = m_triangles.size();
	for(unsigned t = 0; t<tcount; t++)
	{
		m_triangles[t]->m_userMarked = false;
	}
}

void model_show_alloc_stats()
{
	log_debug("\n");
	log_debug("primitive allocation stats (recycler/total)\n");

	log_debug("Model: none/%d\n",model_allocated);
	Model::Vertex::stats();
	Model::Triangle::stats();
	Model::Group::stats();
	Model::Material::stats();
	Model::Keyframe::stats();
	Model::Joint::stats();
	Model::Point::stats();
	Model::TextureProjection::stats();
	Model::SkelAnim::stats();
	Model::FrameAnim::stats();
	Model::FrameAnimVertex::stats();
//	Model::FrameAnimPoint::stats();
	BspTree::Poly::stats();
	BspTree::Node::stats();
	log_debug("Textures: none/%d\n",Texture::s_allocated);
	log_debug("GlTextures: none/%d\n",Model::s_glTextures);
#ifdef MM3D_EDIT
	log_debug("ModelUndo: none/%d\n",ModelUndo::s_allocated);
#endif // MM3D_EDIT
	log_debug("\n");
}

int model_free_primitives()
{
	int c = 0;

	log_debug("purging primitive recycling lists\n");

	c += Model::Vertex::flush();
	c += Model::Triangle::flush();
	c += Model::Group::flush();
	c += Model::Material::flush();
	c += Model::Joint::flush();
	c += Model::Point::flush();
	c += Model::Keyframe::flush();
	c += Model::SkelAnim::flush();
	c += BspTree::Poly::flush();
	c += BspTree::Node::flush();
	c += Model::FrameAnim::flush();
	c += Model::FrameAnimVertex::flush();
//	c += Model::FrameAnimPoint::flush();

	return c;
}

//errorObj.cc
const char *modelErrStr(Model::ModelErrorE err,Model *model)
{
	switch (err)
	{
		case Model::ERROR_NONE:
			return transll("Success");
		case Model::ERROR_CANCEL:
			return transll("Canceled");
		case Model::ERROR_UNKNOWN_TYPE:
			return transll("Unrecognized file extension (unknown type)");
		case Model::ERROR_UNSUPPORTED_OPERATION:
			return transll("Operation not supported for this file type");
		case Model::ERROR_BAD_ARGUMENT:
			return transll("Invalid argument (internal error)");
		case Model::ERROR_NO_FILE:
			return transll("File does not exist");
		case Model::ERROR_NO_ACCESS:
			return transll("Permission denied");
		case Model::ERROR_FILE_OPEN:
			return transll("Could not open file");
		case Model::ERROR_FILE_READ:
			return transll("Could not read from file");
		case Model::ERROR_BAD_MAGIC:
			return transll("File is the wrong type or corrupted");
		case Model::ERROR_UNSUPPORTED_VERSION:
			return transll("Unsupported version");
		case Model::ERROR_BAD_DATA:
			return transll("File contains invalid data");
		case Model::ERROR_UNEXPECTED_EOF:
			return transll("Unexpected end of file");
		case Model::ERROR_EXPORT_ONLY:
			return transll("Write not supported,try \"Export...\"");
		case Model::ERROR_FILTER_SPECIFIC:
			if(model)
			{
				return model->getFilterSpecificError();
			}
			else
			{
				return Model::getLastFilterSpecificError();
			}
		case Model::ERROR_UNKNOWN:
			return transll("Unknown error");
		default:
			break;
	}
	return "FIXME: Untranslated model error";
}

	////Object2020////Object2020////Object2020////Object2020/////

	/*BACKGROUND
	https://github.com/zturtleman/mm3d/issues/114

	This refactor simplifies a lot of logic and removes countless
	modelundo.cc objects from the mix.

	The pass-through APIs are legacy compatibility functions. The
	various APIs have been ported as-is for the time being. There
	is little consistency between the various types of "Position"
	classes. I've removed some euphemistic method names, in favor
	of the "Unanimated" construction that seems better suited for
	conveying intent. Note, only preexisting APIs are implemented.
	*/

Model::Object2020 *Model::getPositionObject(const Position &pos)
{
	void *v; switch(pos.type)
	{
	case PT_Joint: v = &m_joints; break;
	case PT_Point: v = &m_points; break;
	case PT_Projection: v = &m_projections; break;
	case _OT_Background_: v = &m_background; break;
	default: return nullptr;
	}
	auto &vec = *(std::vector<Object2020*>*)v;
	return pos.index<vec.size()?vec[pos]:nullptr;
}

Matrix Model::Object2020::getMatrix()const
{
	//REMINDER: setRotation doesn't preserve scale.
	Matrix m; m.setRotation(m_rotSource); m.scale(m_xyzSource); 
	m.setTranslation(m_absSource); return m;
}
Matrix Model::Object2020::getMatrixUnanimated()const
{
	//REMINDER: setRotation doesn't preserve scale.
	Matrix m; m.setRotation(m_rot); m.scale(m_xyz);
	m.setTranslation(m_abs); return m;
}

bool Model::setPositionCoords(const Position &pos, const double *coord)
{
	Object2020 *obj = getPositionObject(pos); if(!obj) return false;

	if(pos.type==PT_Point&&m_animationMode==ANIMMODE_FRAME
	 ||pos.type==PT_Joint&&m_animationMode==ANIMMODE_SKELETAL)
	{
		makeCurrentAnimationFrame();
		return -1!=setKeyframe
		(m_currentAnim,m_currentFrame,pos,KeyTranslate,coord[0],coord[1],coord[2]);
	}

	//TODO: Let sendUndo reject no-op changes.
	if(!memcmp(obj->m_abs,coord,sizeof(*coord)*3)) return true; //2020

	m_changeBits|=MoveOther; //2020

	if(m_undoEnabled)
	{
		auto undo = new MU_SetObjectXYZ(pos);
		undo->setXYZ(this,obj->m_abs,coord); 
		sendUndo(undo,true);
	}
	memcpy(obj->m_abs,coord,sizeof(*coord)*3); 
	
	switch(pos.type)
	{
	case PT_Joint: invalidateSkel(); break;
	case PT_Projection: applyProjection(pos); break;
	}
		
	return true;
}
bool Model::getPositionCoords(const Position &pos, double *coord)const
{
	if(pos.type==PT_Vertex) return getVertexCoords(pos,coord);
	auto *obj = getPositionObject(pos); if(!obj) return false;	
	memcpy(coord,obj->m_absSource,sizeof(*coord)*3); return true;
}
bool Model::getPositionCoordsUnanimated(const Position &pos, double *coord)const
{
	if(pos.type==PT_Vertex) return getVertexCoordsUnanimated(pos,coord);
	auto *obj = getPositionObject(pos); if(!obj) return false;	
	memcpy(coord,obj->m_abs,sizeof(*coord)*3); return true;
}

bool Model::setPositionRotation(const Position &pos, const double *rot)
{
	Object2020 *obj = getPositionObject(pos);
	if(!obj) return false;

	//HACK: Compatibility fix.
	if(pos.type==PT_Point&&m_animationMode==ANIMMODE_FRAME
	 ||pos.type==PT_Joint&&m_animationMode==ANIMMODE_SKELETAL)
	{
		makeCurrentAnimationFrame();
		return -1!=setKeyframe
		(m_currentAnim,m_currentFrame,pos,KeyRotate,rot[0],rot[1],rot[2]);
	}

	//TODO: Let sendUndo reject no-op changes.
	if(!memcmp(obj->m_rot,rot,sizeof(*rot)*3)) return true; //2020

	m_changeBits|=MoveOther; //2020

	if(m_undoEnabled)
	{			
		auto undo = new MU_SetObjectXYZ(pos);
		undo->setXYZ(this,obj->m_rot,rot); 
		sendUndo(undo,true);
	}
	memcpy(obj->m_rot,rot,sizeof(*rot)*3);

	switch(pos.type)
	{
	case PT_Joint: invalidateSkel(); break;
	case PT_Projection: applyProjection(pos); break;
	}
	
	return true;
}
bool Model::getPositionRotation(const Position &pos, double *rot)const
{
	auto *obj = getPositionObject(pos); if(!obj) return false;	
	memcpy(rot,obj->m_rotSource,sizeof(*rot)*3); return true;
}
bool Model::getPositionRotationUnanimated(const Position &pos, double *rot)const
{
	auto *obj = getPositionObject(pos); if(!obj) return false;	
	memcpy(rot,obj->m_rot,sizeof(*rot)*3); return true;
}

bool Model::setPositionScale(const Position &pos, const double *scale)
{
	Object2020 *obj = getPositionObject(pos); if(!obj) return false;

	//HACK: Compatibility fix.
	if(pos.type==PT_Point&&m_animationMode==ANIMMODE_FRAME
	 ||pos.type==PT_Joint&&m_animationMode==ANIMMODE_SKELETAL)
	{
		makeCurrentAnimationFrame();
		return -1!=setKeyframe
		(m_currentAnim,m_currentFrame,pos,KeyScale,scale[0],scale[1],scale[2]);
	}

	//TODO: Let sendUndo reject no-op changes.
	if(!memcmp(obj->m_xyz,scale,sizeof(*scale)*3)) return true; //2020

	m_changeBits|=MoveOther; //2020

	if(m_undoEnabled)
	{
		auto undo = new MU_SetObjectXYZ(pos);
		undo->setXYZ(this,obj->m_xyz,scale); 
		sendUndo(undo,true);
	}
	memcpy(obj->m_xyz,scale,sizeof(*scale)*3); 
	
	switch(pos.type)
	{
	case PT_Joint: invalidateSkel(); break;
	case PT_Projection: applyProjection(pos); break;
	}

	return true;
}
bool Model::getPositionScale(const Position &pos, double *scale)const
{
	auto *obj = getPositionObject(pos); if(!obj) return false;	
	memcpy(scale,obj->m_xyzSource,sizeof(*scale)*3); return true;
}
bool Model::getPositionScaleUnanimated(const Position &pos, double *scale)const
{
	auto *obj = getPositionObject(pos); if(!obj) return false;	
	memcpy(scale,obj->m_xyz,sizeof(*scale)*3); return true;
}

bool Model::setPositionName(const Position &pos, const char *name)
{
	auto *obj = getPositionObject(pos);
	if(obj&&name&&name[0])
	{
		if(m_undoEnabled)
		{			
			auto undo = new MU_SetObjectName(pos);
			undo->setName(name,obj->m_name.c_str());
			sendUndo(undo);
		}

		obj->m_name = name;
		return true;
	}
	else return false;
}
const char *Model::getPositionName(const Position &pos)const
{
	auto *obj = getPositionObject(pos);
	return obj?obj->m_name.c_str():nullptr;
}

bool Model::setPointCoords(unsigned point, const double *trans)
{
	return setPositionCoords({PT_Point,point},trans);
}
bool Model::setProjectionCoords(unsigned proj, const double *coord)
{
	return setPositionCoords({PT_Projection,proj},coord);
}
bool Model::setBackgroundCenter(unsigned index, float x, float y, float z)
{
	double coord[3] = {x,y,z};
	return setPositionCoords({_OT_Background_,index},coord);
}
bool Model::getBoneJointCoords(unsigned joint, double *coord)const
{
	return getPositionCoords({PT_Joint,joint},coord);
}
bool Model::getBoneJointCoordsUnanimated(unsigned joint, double *coord)const
{
	return getPositionCoordsUnanimated({PT_Joint,joint},coord);
}
bool Model::getBoneJointRotation(unsigned joint, double *coord)const
{
	return getPositionRotation({PT_Joint,joint},coord);
}
bool Model::getBoneJointRotationUnanimated(unsigned joint, double *coord)const
{
	return getPositionRotationUnanimated({PT_Joint,joint},coord);
}
bool Model::getBoneJointScale(unsigned joint, double *coord)const
{
	return getPositionScale({PT_Joint,joint},coord);
}
bool Model::getBoneJointScaleUnanimated(unsigned joint, double *coord)const
{
	return getPositionScaleUnanimated({PT_Joint,joint},coord);
}
bool Model::getPointCoords(unsigned point, double *coord)const
{
	return getPositionCoords({PT_Point,point},coord);
}
bool Model::getPointCoordsUnanimated(unsigned point, double *coord)const
{
	return getPositionCoordsUnanimated({PT_Point,point},coord);
}
bool Model::getProjectionCoords(unsigned proj, double *coord)const
{
	return getPositionCoords({PT_Projection,proj},coord);
}
bool Model::getBackgroundCenter(unsigned index, float &x, float &y, float &z)const
{
	double coord[3];
	if(!getPositionCoords({_OT_Background_,index},coord)) return false;
	x = (float)coord[0]; 
	y = (float)coord[1]; 
	z = (float)coord[2]; return true;
}

bool Model::setBoneJointRotation(unsigned joint, const double *rot)
{
	return setPositionRotation({PT_Joint,joint},rot);
}
bool Model::setPointRotation(unsigned point, const double *rot)
{
	return setPositionRotation({PT_Point,point},rot);
}
bool Model::setProjectionRotation(unsigned proj, const double *rot)
{
	return setPositionRotation({PT_Projection,proj},rot);
}
bool Model::getPointRotation(unsigned point, double *rot)const
{
	return getPositionRotation({PT_Point,point},rot);
}
bool Model::getPointRotationUnanimated(unsigned point, double *rot)const
{
	return getPositionRotationUnanimated({PT_Point,point},rot);
}
bool Model::getPointScale(unsigned point, double *rot)const
{
	return getPositionScale({PT_Point,point},rot);
}
bool Model::getPointScaleUnanimated(unsigned point, double *rot)const
{
	return getPositionScaleUnanimated({PT_Point,point},rot);
}
bool Model::getProjectionRotation(unsigned proj, double *rot)const
{
	return getPositionRotation({PT_Projection,proj},rot);
}

bool Model::setProjectionScale(unsigned proj, double scale)
{
	double xyz[3] = {scale,scale,scale};
	return setPositionScale({PT_Projection,proj},xyz);
}
bool Model::setBackgroundScale(unsigned index, float scale)
{
	double xyz[3] = {scale,scale,scale};
	return setPositionScale({_OT_Background_,index},xyz);
}
double Model::getProjectionScale(unsigned proj)const
{
	double xyz[3];
	if(getPositionScale({PT_Projection,proj},xyz)) 
	return (float)xyz[0]; return 0; //???	
}
float Model::getBackgroundScale(unsigned index)const
{
	double xyz[3];
	if(getPositionScale({_OT_Background_,index},xyz)) 
	return (float)xyz[0]; return 0; //???	
}

bool Model::setBoneJointName(unsigned joint, const char *name)
{
	return setPositionName({PT_Joint,joint},name);
}
bool Model::setPointName(unsigned point, const char *name)
{
	return setPositionName({PT_Point,point},name);
}
bool Model::setProjectionName(unsigned proj, const char *name)
{
	return setPositionName({PT_Projection,proj},name);
}
bool Model::setBackgroundImage(unsigned index, const char *str)
{
	return setPositionName({_OT_Background_,index},str);
}
const char *Model::getBoneJointName(unsigned joint)const
{
	return getPositionName({PT_Joint,joint});
}
const char *Model::getPointName(unsigned point)const
{
	return getPositionName({PT_Point,point});
}
const char *Model::getProjectionName(unsigned proj)const
{
	return getPositionName({PT_Projection,proj});
}
const char *Model::getBackgroundImage(unsigned index)const
{
	return getPositionName({_OT_Background_,index});
}

static const double model_identity[2][3] = {{0,0,0},{1,1,1}};
void Model::Object2020::getParams(double abs[3], double rot[3], double xyz[3])const
{
	//joints?
	//if(abs) memcpy(abs,m_absSource,sizeof(*abs)*3);
	if(abs) memcpy(abs,getParams(InterpolantCoords),sizeof(*abs)*3);
	if(rot) memcpy(rot,m_rotSource,sizeof(*rot)*3);
	if(xyz) memcpy(xyz,m_xyzSource,sizeof(*xyz)*3);
}				
const double *Model::Object2020::getParams(Interpolant2020E d)const
{
	auto *j = m_type==PT_Joint?(Joint*)this:nullptr;

	if(j&&m_abs==m_absSource) return model_identity[d==InterpolantScale]; 

	switch(d)
	{
	case InterpolantCoords: return j?j->m_kfRel:m_absSource;
	case InterpolantRotation: return m_rotSource;
	case InterpolantScale: return m_xyzSource;
	default: return nullptr;
	}
}
const double *Model::Object2020::getParamsUnanimated(Interpolant2020E d)const
{
	if(m_type==PT_Joint) return model_identity[d==InterpolantScale]; 

	switch(d)
	{
	case InterpolantCoords: return m_abs;
	case InterpolantRotation: return m_rot;
	case InterpolantScale: return m_xyz;
	default: return nullptr;
	}
}