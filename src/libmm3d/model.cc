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
	for(int j=3;j-->0;)	
	btoa[j] = coord[j]-triCoord[j];	
	normalize3(btoa);

	double c = dot3(btoa,triNorm);
	return c<-ATOLERANCE?-1:c>ATOLERANCE?1:0;
}

// NOTE: Assumes coord is in plane of p1,p2,p3
//
// returns:
//	 1 = inside triangle
//	 0 = on triangle edge
//	-1 = outside triangle
static int model_pointInTriangle(double *coord, double *p1, double *p2, double *p3)
{
	int vside[3];
	double avg[3],normal[3];
	for(int i=3;i-->0;)
	avg[i] = (p1[i]+p2[i]+p3[i])/3;	
	calculate_normal(normal,p1,p2,p3);
	for(int i=3;i-->0;) avg[i]+=normal[i];

	calculate_normal(normal,p1,p2,avg);
	vside[0] = model_pointInPlane(coord,avg,normal);
	calculate_normal(normal,p2,p3,avg);
	vside[1] = model_pointInPlane(coord,avg,normal);
	calculate_normal(normal,p3,p1,avg);
	vside[2] = model_pointInPlane(coord,avg,normal);

	if(vside[0]<0&&vside[1]<0&&vside[2]<0)
	return 1;
	if(vside[0]>0&&vside[1]>0&&vside[2]>0)
	return 1;
	if(vside[0]==0&&vside[1]==vside[2])
	return 0;
	if(vside[1]==0&&vside[0]==vside[2])
	return 0;
	if(vside[2]==0&&vside[0]==vside[1])
	return 0;
	if(vside[0]==0&&vside[1]==0)
	return 0;
	if(vside[0]==0&&vside[2]==0)
	return 0;
	if(vside[1]==0&&vside[2]==0)
	return 0;
	return -1;
}

// returns:
//	 ipoint = coordinates of edge line and plane
//	 int	 = 0 is no intersection,+1 is toward p2 from p1,-1 is away from p2 from p1
static int model_findEdgePlaneIntersection(double *ipoint, double *p1, double *p2,
		double *triCoord, double *triNorm)
{
	double edgeVec[3] = { p2[0]-p1[0],p2[1]-p1[1],p2[2]-p1[2] };

	//log_debug("finding intersection with plane with line from %f,%f,%f to %f,%f,%f\n",p1[0],p1[1],p1[2],p2[0],p2[1],p2[2]);

	double a = dot3(edgeVec,triNorm), d = -dot3(triCoord,triNorm);

	// prevent divide by zero
	if(fabs(a)<TOLERANCE)
	{
		// edge is parallel to plane,the value of ipoint is undefined
		return 0;
	}

	// distance along edgeVec p1 to plane
	double dist = -(d+dot3(p1,triNorm))/a;

	// dist is %of velocity vector, scale to get impact point	
	ipoint[0] = p1[0]+dist*edgeVec[0];
	ipoint[1] = p1[1]+dist*edgeVec[1];
	ipoint[2] = p1[2]+dist*edgeVec[2];

	// Distance is irrelevant,we just want to know where 
	// the intersection is
	return dist>=0.0?1:-1;
}

Model::Model()
	: m_filename(""),
	m_validContext(false),
	  m_validBspTree(false),
	  m_drawJoints(/*JOINTMODE_BONES*/true),
	  m_drawSelection(),
	  m_drawProjections(true),
	  m_validNormals(false),
	  m_validAnim(false),
	  m_validAnimNormals(false),
	  m_validJoints(false),
	m_validAnimJoints(false),
	  //m_forceAddOrDelete(false),
		m_skeletalMode(false),
		m_skeletalMode2(true),
	  m_animationMode(ANIMMODE_NONE),
	  m_currentFrame(0),
	  m_currentAnim(0),
	  m_currentTime(0.0),
	m_elapsedTime(0.0), //2022	
	  m_canvasDrawMode(0),
	  m_perspectiveDrawMode(3),
	  m_drawOptions(DO_BONES), //2019
		m_addLayer(1), //2022
		m_addedLayers(0),
		m_primaryLayers(2),
		m_overlayLayers(0),
		m_drawingLayers(2)
#ifdef MM3D_EDIT
	,
	  m_saved(true),
	//https://github.com/zturtleman/mm3d/issues/145
	//m_selectionMode(SelectVertices),
	  m_selectionMode(SelectTriangles),
	  m_selecting(false),
	  //This is held onto until something calls 
	  //updateObservers, usually the first edit.
	  //This just causes double changes on open.
	  //m_changeBits(ChangeAll),
	m_changeBits(0),
	  m_undoEnabled(false),
	  m_undoEnacted(false),
	m_viewportColorsMode(0),
	m_viewportColors(6)

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

	//log_debug("deleting model\n");
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

	while(!m_anims.empty())
	{
		m_anims.back()->release();
		m_anims.pop_back();
	}

	while(!m_utils.empty())
	{
		m_utils.back()->release();
		m_utils.pop_back();
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
			return TRANSLATE_NOOP("LowLevel","Invalid argument (internal error, probably null-pointer argument)");
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
			return model->getFilterSpecificError();			
			return getLastFilterSpecificError();			
		case ERROR_UNKNOWN:
			return TRANSLATE_NOOP("LowLevel","Unknown error" );
	}

	return TRANSLATE_NOOP("LowLevel","Invalid error code");
}

bool Model::operationFailed(Model::ModelErrorE err)
{
	return err!=ERROR_NONE&&err!=ERROR_CANCEL;
}

void Model::pushError(const std::string &err)
{
	m_loadErrors.push_back(err);
}

bool Model::popError(std::string &rval)
{
	if(!m_loadErrors.empty())
	{
		rval.swap(m_loadErrors.front());
		m_loadErrors.pop_front();
		return true;
	}
	rval.clear(); return false;
}

#ifdef MM3D_EDIT //???

void Model::updateObservers()
{
	//2019: Prevent recursion.
	static int recursive = 0;

	//https://github.com/zturtleman/mm3d/issues/90
	//I'm letting 0 indicate simply to refresh the view.
	//assert(m_changeBits);
	//if(!m_changeBits)
	{
		if(recursive) return;
	}

	recursive++;
	int change = m_changeBits; 
	m_changeBits = 0; //2019
	for(auto*ea:m_observers) ea->modelChanged(change);
	//m_changeBits = 0;
	recursive--;

	//2022: consolidate inner changes (in response)
	//NOTE: this can't detect changes that don't set
	//m_changeBits
	if(m_changeBits&&!recursive)
	{
		updateObservers();
	}
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

static bool model_inv_infls_msg(Model *m, int n) //2021
{
	//NOTE: Even if this could be solved in an acceptable way
	//it's not likely to be reversible without saving the old
	//coordinates so that undo operations can restore them :(

	if(m->getUndoEnabled())
	model_status(m,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel",
	"%d multiply influenced vertices not transformed"),n);
	return false;
}

int Model::addVertex(double x, double y, double z)
{
	int num = m_vertices.size();

	m_changeBits |= AddGeometry;

	Vertex *vp = Vertex::get(m_addLayer,m_animationMode);

	vp->m_coord[0] = x;
	vp->m_coord[1] = y;
	vp->m_coord[2] = z;
	memcpy(vp->m_kfCoord,vp->m_coord,3*sizeof(double));
	//vp->m_free = false;
	m_vertices.push_back(vp);

	if(auto fp=m_vertices.front()->m_frames.size())
	{
		vp->m_frames.assign(fp,nullptr);
		while(fp-->0) vp->m_frames[fp] = FrameAnimVertex::get();
	}

	Undo<MU_Add>(this,num,vp); return num;
}
int Model::addVertex(int copy, const double pos[3])
{
	size_t num = m_vertices.size();

	if((unsigned)copy>=num){ assert(0); return -1; }

	auto *cp = m_vertices[copy];

	m_changeBits |= AddGeometry;

	Vertex *vp = Vertex::get(m_addLayer,m_animationMode);	
	m_vertices.push_back(vp);

	if(auto fp=m_vertices.front()->m_frames.size())
	{
		vp->m_frames.assign(fp,nullptr);
		while(fp-->0)
		{
			auto &f = vp->m_frames[fp];
			auto &g = cp->m_frames[fp];
			f = FrameAnimVertex::get();			
			f->m_interp2020 = g->m_interp2020;
			memcpy(f->m_coord,g->m_coord,3*sizeof(double));
		}
	}
	vp->m_influences = cp->m_influences;

	if(!pos)
	{
		memcpy(vp->m_coord,cp->m_coord,3*sizeof(double));
		memcpy(vp->m_kfCoord,cp->m_kfCoord,3*sizeof(double));
	}
	else //TODO: Should this insert frames?
	{
		//memcpy(vp->m_coord,pos,3*sizeof(double));
		memcpy(vp->m_kfCoord,pos,3*sizeof(double));

		Vector os(pos); if(inSkeletalMode()) 
		{
			validateAnim(); //I guess?

			if(!_skel_xform_abs(-1,vp->m_influences,os))
			{
				model_inv_infls_msg(this,1);
			}
		}
		memcpy(vp->m_coord,os.getVector(),3*sizeof(double));

		if(!vp->m_frames.empty())
		{
			for(int i=3;i-->0;) os[i]-=cp->m_coord[i];

			for(auto&ea:vp->m_frames) if(ea->m_interp2020)
			{
				for(int i=3;i-->0;) ea->m_coord[i]+=os[i];
			}
		}
	}

	Undo<MU_Add>(this,num,vp); return num;
}

/*REFERENCE
void Model::setVertexFree(unsigned v,bool o)
{
	if(v<m_vertices.size()) m_vertices[v]->m_free = o;
}*/
bool Model::isVertexFree(unsigned v)const
{
	if(v<m_vertices.size())
	{
		//return m_vertices[v]->m_free;
		return m_vertices[v]->m_faces.empty();
	}
	return false;
}

int Model::addTriangle(unsigned v1, unsigned v2, unsigned v3)
{
	//m_changeBits |= AddGeometry;

	auto &vs = m_vertices;
	unsigned vsz = m_vertices.size();
	if(v1<vsz&&v2<vsz&&v3<vsz)
	{	
		auto num = (unsigned)m_triangles.size();
		//log_debug("adding triangle %d for vertices %d,%d,%d\n",num,v1,v2,v3);

		Triangle *triangle = Triangle::get(m_addLayer);
		auto &vi = triangle->m_vertexIndices;
		vi[0] = v1;
		vi[1] = v2;
		vi[2] = v3;
		//m_triangles.push_back(triangle);
		insertTriangle(num,triangle);
				
		Undo<MU_Add>(this,num,triangle);

		return num;
	}
	return -1;
}

int Model::addBoneJoint(const char *name, int parent)
{
	//TODO? Disallow ""?
	//if(!name||parent>=(int)m_joints.size())
	if(!name||(unsigned)parent+1>m_joints.size())
	{
		return -1;
	}

	//2021: preventing surprise (MM3D)
	//doesn't allow setting names to a
	//blank but filters can.
	//https://github.com/zturtleman/mm3d/issues/158
	if(!name[0]) name = "_";

	m_changeBits |= AddOther;

	int num = m_joints.size();

	Joint *joint = Joint::get(m_addLayer,m_animationMode);

	joint->m_parent = parent;
	joint->m_name	= name;

	insertBoneJoint(num,joint);
	
	if(m_undoEnabled) sendUndo(new MU_Add(num,joint));

	return num;
}
int Model::addBoneJoint(const char *name, double x, double y, double z, int parent)
{
	//Need to do this before the joint is added.
	if(parent!=-1) validateSkel();

	if(-1==addBoneJoint(name,parent))
	{
		assert(0); //copySelected got -1, I see no reason why???
		return -1;
	}

	Joint *joint = m_joints.back();

	//log_debug("New joint at %f,%f,%f\n",x,y,z); //???

	joint->m_rel[0] = x;
	joint->m_rel[1] = y;
	joint->m_rel[2] = z;
	if(parent!=-1)
	m_joints[parent]->getAbsoluteInverse().apply3x(joint->m_rel);

	return (unsigned)m_joints.size()-1;
}

int Model::addPoint(const char *name, double x, double y, double z,
		double xrot, double yrot, double zrot)
{
	if(!name) return -1; //TODO? Disallow ""?

	//2021: preventing surprise (MM3D)
	//doesn't allow setting names to a
	//blank but filters can.
	//https://github.com/zturtleman/mm3d/issues/158
	if(!name[0]) name = "_";

	int num = m_points.size();

	//log_debug("New point at %f,%f,%f\n",x,y,z); //???

	Point *point = Point::get(m_addLayer,m_animationMode);

	point->m_name	= name;

	point->m_abs[0] = x;
	point->m_abs[1] = y;
	point->m_abs[2] = z;
	point->m_rot[0]	= xrot;
	point->m_rot[1]	= yrot;
	point->m_rot[2]	= zrot;

	num = m_points.size();
	insertPoint(num,point);

	if(m_undoEnabled) sendUndo(new MU_Add(num,point));

	//validateSkel(); //???

	return num;
}

int Model::addProjection(const char *name, int type, double x, double y, double z)
{
	if(name==nullptr) return -1;

	//2021: preventing surprise (MM3D)
	//doesn't allow setting names to a
	//blank but filters can.
	//https://github.com/zturtleman/mm3d/issues/158
	if(!name[0]) name = "_";

	m_changeBits |= AddOther;

	int num = m_projections.size();

	//log_debug("New projection at %f,%f,%f\n",x,y,z);

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

	Undo<MU_Add>(this,num,proj); return num;
}

bool Model::setTriangleVertices(unsigned t, unsigned v1, unsigned v2, unsigned v3)
{		
	if(t>=m_triangles.size()) return false;

	auto *tp = m_triangles[t];	
	auto *tv = tp->m_vertexIndices;

	if(tv[0]==v1&&tv[1]==v2&&tv[2]==v3) return true; //2022

	auto &vl = m_vertices;
	unsigned vsz = vl.size();
	if(v1>=vsz||v2>=vsz||v3>=vsz) return false;	

	if(Undo<MU_SetTriangleVertices>undo=this)
	undo->setTriangleVertices(t,v1,v2,v3,tv[0],tv[1],tv[2]);

	//2020: Keep connectivity to help calculateNormals
	unsigned v[3] = {v1,v2,v3};
	for(int i=3;i-->0;)
	{
		vl[tv[i]]->_erase_face(tp,i);
		vl[v[i]]->m_faces.push_back({tp,i});
		tv[i] = v[i];
	}

	m_changeBits |= AddGeometry;

	invalidateNormals(); //OVERKILL

	return true;
}

bool Model::getTriangleVertices(unsigned t, unsigned &v1, unsigned &v2, unsigned &v3)const
{
	if(t>=m_triangles.size()) return false;

	auto *tp = m_triangles[t];	
	v1 = tp->m_vertexIndices[0];
	v2 = tp->m_vertexIndices[1];
	v3 = tp->m_vertexIndices[2]; return true;
}

void Model::deleteVertex(unsigned vertexNum)
{
	//LOG_PROFILE(); //???

	if(vertexNum>=m_vertices.size()) return;

	m_changeBits |= AddGeometry; //2020

	Undo<MU_Delete>(this,vertexNum,m_vertices[vertexNum]);

	removeVertex(vertexNum);
}

void Model::deleteTriangle(unsigned t)
{
	//LOG_PROFILE(); //???

	if(t>=m_triangles.size()) return;

	auto *tp = m_triangles[t];

	if(tp->m_group>=0)
	{
		//2022: Passing false is an optimization to
		//avoid interleaving undo objects since that
		//prevents MU_Delete from resuming/combining.
		removeTriangleFromGroup(tp->m_group,t,false);
	}

	//m_changeBits |= AddGeometry

	Undo<MU_Delete>(this,t,tp);

	removeTriangle(t);
}
void Model::deleteTriangles(const int_list &l, bool asc)
{
	auto *tl = m_triangles.data();
	int sz = (int)m_triangles.size();

	for(auto&i:l) if(i<sz)
	{
		//2022: Passing false is an optimization to
		//avoid interleaving undo objects since that
		//prevents MU_Delete from resuming/combining.
		auto *tp = tl[i];	
		if(tp->m_group>=0)
		removeTriangleFromGroup(tp->m_group,i,false);
	}
	else assert(0);

	//m_changeBits |= AddGeometry

	Undo<MU_Delete> undo(this,PartFaces);

	//TODO: It would be nice to do this in 
	//contiguous blocks to limit memmove
	//like patterns. It might help to sort
	//the list, but it's const right now.

	int cmp = sz; if(asc)
	{
		auto rit = l.rbegin();
		auto ritt = l.rend();
		for(;rit<ritt;rit++)
		{
			int i = *rit; if(i<cmp)
			{
				cmp = i;

				if(undo) undo->add(i,tl[i]);

				removeTriangle(i);
			}
			else assert(0);
		}
	}
	else for(int i:l) if(i<cmp)
	{
		cmp = i;

		if(undo) undo->add(i,tl[i]);

		removeTriangle(i);
	}
	else assert(0);
}

void Model::deleteBoneJoint(unsigned joint)
{
	if(joint>=m_joints.size()) return;
		
	unsigned count = m_joints.size();

	m_changeBits |= AddOther; //2020

	for(unsigned v=0;v<m_vertices.size();v++)	
	removeVertexInfluence(v,joint);
	for(unsigned p=0;p<m_points.size();p++)
	removePointInfluence(p,joint);

	int new_parent = m_joints[joint]->m_parent;
		
	validateSkel(); //Need m_absolute to be correct below.

	for(unsigned j=0;j<count;j++)	
	if(m_joints[j]->m_parent==(int)joint)
	{
		//NOTE: This triggers validateSkel on the way out.
		//(It calls invalidateSkel.)
		setBoneJointParent(j,new_parent,false);
	}	

	Undo<MU_Delete>(this,joint,m_joints[joint]);

	removeBoneJoint(joint);

	//log_debug("new parent was %d\n",new_parent);

	//May as well keep it up-to-date since the only real
	//benefit to not doing so is if multiple joints are
	//deleted, but in that case validateSkel is called
	//above either way.
	//invalidateSkel();
	validateSkel();
}

void Model::deletePoint(unsigned point)
{
	m_changeBits |= AddOther; //2020

	if(point<m_points.size())
	{		
		Undo<MU_Delete>(this,point,m_points[point]);

		removePoint(point);
	}
}

void Model::deleteProjection(unsigned proj)
{
	if(proj<m_projections.size())
	{
		m_changeBits |= AddOther; //2020

		Undo<MU_Delete>(this,proj,m_projections[proj]);

		removeProjection(proj);
	}
}

unsigned Model::deleteOrphanedVertices(unsigned begin, unsigned i)
{
	unsigned ret = 0;
	for(i=std::min(i,(unsigned)m_vertices.size());i-->begin;)
	if(m_vertices[i]->m_faces.empty()) 
	{
		deleteVertex(i); ret++;
	}
	return ret; //2020
}

void Model::deleteFlattenedTriangles()
{
	//LOG_PROFILE(); //???

	// Delete any triangles that have two or more vertex indices that point
	// at the same vertex (could happen as a result of welding vertices

	for(int t=m_triangles.size();t-->0;)
	{
		if(m_triangles[t]->_flattened()) deleteTriangle(t);	
	}
}

void Model::deleteSelected()
{
	//LOG_PROFILE(); //???

	auto lv = m_primaryLayers; //???

	//THIS CAN BE SIMPLIFIED
	for(auto&vp:m_vertices) vp->m_marked = false;

	//2022: deleteTriangle is interleaved undo.
	int_list tris; 

	for(auto t=m_triangles.size();t-->0;) if(m_triangles[t]->m_selected)
	{
		for(int i:m_triangles[t]->m_vertexIndices)
		{
			m_vertices[i]->m_marked = true;
		}
		//deleteTriangle(t);		
	}
	for(auto t=m_triangles.size();t-->0;) 	
	if(m_triangles[t]->m_selected) //2022
	{
		tris.push_back(t);
	}
	else //if(m_triangles[t]->m_visible) //2022
	{
		//NOTE: I'm not sure it's appropriate to
		//test "visible" here, but it might be to
		//mask deletions

		if(m_triangles[t]->visible(lv))
		{
			for(int i:m_triangles[t]->m_vertexIndices)
			if(m_vertices[i]->m_selected&&!m_vertices[i]->m_marked)
			{
				//deleteTriangle(t); break;
				tris.push_back(t); break;
			}
		}
	}
	deleteTriangles(tris,false);

	for(auto v=m_vertices.size();v-->0;)	
	if(m_vertices[v]->m_selected&&!m_vertices[v]->m_marked)
	{
		deleteVertex(v); //Or dissolveVertex?
	}	
	deleteOrphanedVertices(); //???

	for(auto j=m_joints.size();j-->0;) if(m_joints[j]->m_selected)
	{
		deleteBoneJoint(j);
	}
	for(auto p=m_points.size();p-->0;) if(m_points[p]->m_selected)
	{
		deletePoint(p);
	}
	for(auto p=m_projections.size();p-->0;) if(m_projections[p]->m_selected)
	{
		deleteProjection(p);
	}

	// Some selected vertices may not be deleted if their parent
	// triangle was deleted
	unselectAllVertices(); //NOTE: REALLY SOME SHOULD BE HIDDEN
}

bool Model::movePosition(const Position &pos, double x, double y, double z)
{
	switch(pos.type)
	{
	case PT_Vertex: return moveVertex(pos.index,x,y,z);
	case PT_Joint: return moveBoneJoint(pos.index,x,y,z);
	case PT_Point: return movePoint(pos.index,x,y,z);
	case PT_Projection: return moveProjection(pos.index,x,y,z);
	default: log_error("do not know how to move position of type %d\n",pos.type);
	}
	return false;
}
bool Model::movePositionUnanimated(const Position &pos, double x, double y, double z)
{
	auto swap = m_animationMode;
	if(swap) m_animationMode = ANIMMODE_NONE;
	bool ret = movePosition(pos,x,y,z);
	if(swap) m_animationMode = swap; return ret;
}

bool Model::moveVertex(unsigned index, double x, double y, double z)
{
	if(index>=m_vertices.size()) return false;

	auto v = m_vertices[index]; if(inSkeletalMode()) 
	{
		Vector tmp(x,y,z);
		if(_skel_xform_abs(-1,v->m_influences,tmp))
		{
			x = tmp[0]; y = tmp[1]; z = tmp[2];
		}
		else return model_inv_infls_msg(this,1); //:(
	}

	if(inFrameAnimMode())
	{
		makeCurrentAnimationFrame();		

		double xyz[3] = {x,y,z};
		return setFrameAnimVertexCoords(m_currentAnim,m_currentFrame,index,xyz);
	}
	else invalidateAnim();

	double *swap = v->m_coord; //FIX ME
	if(Undo<MU_MoveUnanimated> undo=this)
	undo->addPosition({PT_Vertex,index},x,y,z,swap[0],swap[1],swap[2]);
	swap[0] = x; swap[1] = y; swap[2] = z;

	m_changeBits |= MoveGeometry;

	invalidateNormals(); //OVERKILL

	return true;
} 
bool Model::movePoint(unsigned index, double x, double y, double z)
{
	if(index>=m_points.size()) return false;

	auto *p = m_points[index];
	
	if(inSkeletalMode())
	{
		Vector tmp(x,y,z);
		if(_skel_xform_abs(-1,p->m_influences,tmp))
		{
			x = tmp[0]; y = tmp[1]; z = tmp[2];
		}
		else return model_inv_infls_msg(this,1); //:(
	}

	if(inFrameAnimMode())
	{
		makeCurrentAnimationFrame();

		return -1!=setKeyframe
		(m_currentAnim,m_currentFrame,{PT_Point,index},KeyTranslate,x,y,z);
	}
	else invalidateAnim();

	m_changeBits|=MoveOther; //2020

	if(Undo<MU_MoveUnanimated>undo=this)
	undo->addPosition({PT_Point,index},x,y,z,p->m_abs[0],p->m_abs[1],p->m_abs[2]);
	
	p->m_abs[0] = x; p->m_abs[1] = y; p->m_abs[2] = z; return true;
}
bool Model::moveBoneJoint(unsigned j, double x, double y, double z)
{
	if(j>=m_joints.size()) return false;

	auto *p = m_joints[j];

	if(inJointAnimMode()) //2020
	{	
		//HACK? Need to update m_final matrix.
		validateAnimSkel();

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
		return -1!=setKeyframe
		(m_currentAnim,m_currentFrame,{PT_Joint,j},KeyTranslate,coord[0],coord[1],coord[2]);
	}
	
	validateSkel();

	double *cmp = p->m_abs;
	if(x==cmp[0]&&y==cmp[1]&&z==cmp[2]) return true; //2020

	if(Undo<MU_MoveUnanimated>undo=this)
	undo->addPosition({PT_Joint,j},x,y,z,p->m_abs[0],p->m_abs[1],p->m_abs[2]);

	return relocateBoneJoint(j,x,y,z,false);
}
bool Model::relocateBoneJoint(unsigned j, double x, double y, double z, bool downstream)
{
	if(j>=m_joints.size()) return false;
	
	m_changeBits|=MoveOther; //2020

	double old[3],diff[3],tran[3];

	auto *p = m_joints[j];

	old[0] = p->m_absolute.get(3,0);
	old[1] = p->m_absolute.get(3,1);
	old[2] = p->m_absolute.get(3,2);

	tran[0] = diff[0] = (x-old[0]);
	tran[1] = diff[1] = (y-old[1]);
	tran[2] = diff[2] = (z-old[2]);

	if(p->m_parent>=0)
	{
		m_joints[p->m_parent]->m_absolute.inverseRotateVector(tran);
	}

	p->m_rel[0] += tran[0];
	p->m_rel[1] += tran[1];
	p->m_rel[2] += tran[2];

	if(!downstream)
	for(unsigned t=0;t<m_joints.size();t++) if((int)j==m_joints[t]->m_parent)
	{
		tran[0] = diff[0];
		tran[1] = diff[1];
		tran[2] = diff[2];

		m_joints[m_joints[t]->m_parent]->m_absolute.inverseRotateVector(tran);

		m_joints[t]->m_rel[0] -= tran[0];
		m_joints[t]->m_rel[1] -= tran[1];
		m_joints[t]->m_rel[2] -= tran[2];
	}

	invalidateSkel(); return true;
}

void Model::interpolateSelected(Model::Interpolant2020E d, Model::Interpolate2020E e)
{
	assert(e>=0&&e<=InterpolateLerp); //InterpolateLerp

	auto am = m_animationMode;

	if(!am||e<0||d<0||e>InterpolateLerp||d>InterpolantScale) return;

	if(!_anim_check(true)) return;

	//TODO: Remove the frame if no keys remain. This can be
	//done manually with the clear operation until a counter
	//is added/managed.
	auto ca = m_currentAnim, cf = m_currentFrame;
	if(!e) //Programmer error? 
	{
		if(m_currentTime!=getAnimFrameTime(ca,cf))
		return;
	}
	else cf = makeCurrentAnimationFrame(); 
	
	Keyframe kf;
	kf.m_isRotation = KeyType2020E(1<<d);
	kf.m_frame = cf;

	if(e) validateAnim(); //setKeyframe/setFrameAnimVertexCoords/InterpolateCopy

	for(int pass=1;pass<=2;pass++) if(pass&m_anims[ca]->_type)
	{
		Position j{pass==1?PT_Joint:PT_Point,(unsigned)-1};
		auto &vec = *(std::vector<Object2020*>*)(pass==1?(void*)&m_joints:&m_points);
		for(auto*ea:vec)
		{
			j++; if(ea->m_selected)
			{		
				const double *coord = ea->getParams(d);
				setKeyframe(ca,cf,j,kf.m_isRotation,coord[0],coord[1],coord[2],e);
			}
		}
	}

	if(inFrameAnimMode())
	{
		Undo<MU_InterpolateSelected> undo(this,e,cf,ca);

		auto fa = m_anims[m_currentAnim];
		unsigned fp = fa->m_frame0;
		
		bool verts = false;

		int v = -1; if(e||~fp) for(auto*ea:m_vertices) 
		{
			v++; if(ea->m_selected)
			{
				//Have to reset fp?
				//if(0==~fp) _anim_valloc(fa);
				if(0==~fp) fp = fa->_frame0(this);

				auto vf = ea->m_frames[fp+cf];
				auto &cmp = vf->m_interp2020;
				if(cmp!=e) 
				{
					verts = true;

					if(undo) undo->addVertex(v,cmp); 
				
					//HACK: Maybe this value should already be stored.
					if(cmp<=InterpolateCopy)					
					memcpy(vf->m_coord,ea->m_kfCoord,sizeof(ea->m_kfCoord));

					cmp = e;
				}
			}
		}

		if(verts) m_changeBits|=MoveGeometry;
	}

	invalidateAnim();
}

void Model::translateSelected(const double vec[3]) 
{
	//TODO: NEED UNIFORM TRANSLATE/ROTATE MODES FOR JOINTS:
	//1) Free movement (current for translation w/o animation)
	//2) Move root of selection only (current for other modes)
	//3) Move all (was current *bug* for rotating w/ animation)
	//(3 can be pretty interesting, but might have applications)

	//LOG_PROFILE(); //???

	if(!vec[0]&&!vec[1]&&!vec[2]) return; //2020

	int multi = 0; //2021
	bool skel = inSkeletalMode();
	bool skam = inJointAnimMode(); //ANIMMODE_BONELESS?
	bool fram = inFrameAnimMode();
	
	bool sel = false; if(inAnimationMode())
	{
		if(!_anim_check(true)) return; //???

		auto ca = m_currentAnim, cf = m_currentFrame;

		if(skam)
		{
			//for(Position j{PT_Joint,0};j<m_joints.size();j++)
			for(auto&ea:m_joints2)
			if(m_joints[ea.first]->m_selected)
			if(!parentJointSelected(ea.first)) //2021
			{
				Position j = {PT_Joint,ea.first};
				Joint *jt = ea.second;

				if(!sel) 
				{
					sel = true;

					cf = makeCurrentAnimationFrame(); //2020
				}

				//HACK? Need to update m_final matrix.
				validateAnimSkel();

				//DUPLICATES NEW moveBoneJoint LOGIC.
				double coord[3]; for(int i=3;i-->0;) 
				{
					coord[i] = vec[i]+jt->m_kfAbs()[i];
				}
				if(jt->m_parent>=0)
				{
					Joint *pp = m_joints[jt->m_parent];
					pp->m_final.inverseTranslateVector(coord);
					pp->m_final.inverseRotateVector(coord);
					jt->m_relative.inverseTranslateVector(coord);
					jt->m_relative.inverseRotateVector(coord);				
				}
				else //NECESSARY?
				{
					jt->m_absolute.inverseTranslateVector(coord);
					jt->m_absolute.inverseRotateVector(coord);
				}
				setKeyframe(ca,cf,j,KeyTranslate,coord[0],coord[1],coord[2]);
			}
			//2021: Not allowing to (double) rotate joints and other data at
			//same time.
			if(sel) return;
		}
		
		if(fram)
		{
			for(unsigned v=0;v<m_vertices.size();v++)
			if(m_vertices[v]->m_selected)
			{
				if(!sel)
				{
					sel = true;

					cf = makeCurrentAnimationFrame(); //2020

					if(skel) validateAnim(); //Need real m_kfCoord? _resample?
				}

				double coord[3+1];
				auto vt = m_vertices[v];
				if(skel&&!vt->m_influences.empty())
				{
					//setKeyframe would make this recompute the animation for
					//every vertex.
					//validateAnim(); //Need real m_kfCoord? _resample?					
					for(int i=3;i-->0;)
					coord[i] = vt->m_kfCoord[i]+vec[i];
					coord[3] = 1;
					if(!_skel_xform_abs(-1,vt->m_influences,*(Vector*)coord))
					{
						multi++; continue;
					}
				}
				else
				{			
					interpKeyframe(ca,cf,v,coord);
					for(int i=3;i-->0;) coord[i]+=vec[i];
				}
				setFrameAnimVertexCoords(ca,cf,v,coord);
			}

			for(Position j{PT_Point,0};j<m_points.size();j++)
			if(m_points[j]->m_selected)
			{
				if(!sel)
				{
					sel = true;

					cf = makeCurrentAnimationFrame(); //2020

					if(skel) validateAnim(); //Need real m_kfAbs? _resample?
				}

				double coord[3+1];
				auto pt = m_points[j.index];
				if(skel&&!pt->m_influences.empty())
				{
					//setKeyframe would make this recompute the animation for
					//every point.
					//validateAnim(); //Need real m_kfAbs? _resample?
					for(int i=3;i-->0;)
					coord[i] = pt->m_kfAbs[i]+vec[i];
					coord[3] = 1;
					if(!_skel_xform_abs(-1,pt->m_influences,*(Vector*)coord))
					{
						multi++; continue;
					}
				}
				else
				{
					interpKeyframe(ca,cf,j,coord,nullptr,nullptr);
					for(int i=3;i-->0;) coord[i]+=vec[i];
				}
				setKeyframe(ca,cf,j,KeyTranslate,coord[0],coord[1],coord[2]);
			}
		}	
	}
	//else //Enabling modeling in animation mode.
	{
		if(!fram)
		{
			bool verts = false;

			for(auto*v:m_vertices) if(v->m_selected)
			{
				if(!sel)
				{
					sel = true;

					if(skel) validateAnim(); //Need real m_kfCoord? _resample?
				}

				if(skel&&!v->m_influences.empty())
				{
					double coord[3+1];
					for(int i=3;i-->0;)
					coord[i] = v->m_kfCoord[i]+vec[i];
					coord[3] = 1;
					if(!_skel_xform_abs(-1,v->m_influences,*(Vector*)coord))
					{
						multi++; continue;
					}
					for(int i=3;i-->0;) v->m_coord[i] = coord[i];
				}
				else for(int i=3;i-->0;) v->m_coord[i]+=vec[i];

				verts = true;
			}
			if(verts)
			{
				m_changeBits |= MoveGeometry;

				invalidateNormals(); //OVERKILL
			}
		}

		if(!skam)
		{
			//for(unsigned j=0;j<m_joints.size();j++) 		
			for(auto&ea:m_joints2)
			if(m_joints[ea.first]->m_selected)
			{
				sel = true;

				invalidateSkel(); //2020

				m_changeBits |= MoveOther; //2020

				unsigned j = ea.first;
				Joint *jt = ea.second;

				double tran[3] = {vec[0],vec[1],vec[2]};
				//REMINDER: inverseRotateVector should scale.
				if(int p=jt->m_parent>=0)
				m_joints[jt->m_parent]->m_absolute.inverseRotateVector(tran);
				for(int i=3;i-->0;)
				jt->m_rel[i]+=tran[i];

				for(size_t t=0;t<m_joints.size();t++)									
				if(j==(unsigned)m_joints[t]->m_parent)
				{
					//REMINDER: inverseRotateVector should scale.
					double tran2[3] = {vec[0],vec[1],vec[2]};
					jt->m_absolute.inverseRotateVector(tran2);

					for(;t<m_joints.size();t++)
					if(m_joints[t]->m_parent==(signed)j)
					for(int i=3;i-->0;)
					m_joints[t]->m_rel[i]-=tran2[i]; 
						
					break;
				}
			}		
		}

		if(!fram)
		{
			for(auto*p:m_points) if(p->m_selected)
			{
				if(!sel)
				{
					sel = true;

					if(skel) validateAnim(); //Need real m_kfAbs? _resample?
				}

				if(skel&&!p->m_influences.empty())
				{
					double coord[3+1];
					for(int i=3;i-->0;)
					coord[i] = p->m_kfAbs[i]+vec[i];
					coord[3] = 1;
					if(!_skel_xform_abs(-1,p->m_influences,*(Vector*)coord))
					{
						multi++; continue;
					}
					for(int i=3;i-->0;) p->m_abs[i] = coord[i];
				}
				else for(int i=3;i-->0;) p->m_abs[i]+=vec[i];

				m_changeBits |= MoveOther; //2020
			}
		}

			if(skam&&sel) invalidateAnim();


		for(unsigned p=0;p<m_projections.size();p++)
		{
			if(m_projections[p]->m_selected)
			{
				sel = true;

				m_changeBits |= MoveOther; //2020

				for(int i=3;i-->0;)
				m_projections[p]->m_abs[i]+=vec[i];

				applyProjection(p);
			}
		}
		
		if(sel)
		{
			Undo<MU_TranslateSelected>(this,vec);
		}
	}

	if(multi) model_inv_infls_msg(this,multi);

	if(!sel) model_status(this,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel","No selection"));
}
void Model::rotateSelected(const Matrix &m, const double point[3])
{
	//TODO: NEED UNIFORM TRANSLATE/ROTATE MODES FOR JOINTS:
	//1) Free movement (current for translation w/o animation)
	//2) Move root of selection only (current for other modes)
	//3) Move all (was current *bug* for rotating w/ animation)
	//(3 can be pretty interesting, but might have applications)

	//LOG_PROFILE(); //???

	int multi = 0 ; //2021
	bool skel = inSkeletalMode();
	bool skam = inJointAnimMode(); //ANIMMODE_BONELESS?
	bool fram = inFrameAnimMode();

	bool sel = false; if(inAnimationMode())
	{
		if(!_anim_check(true)) return; //???

		auto ca = m_currentAnim, cf = m_currentFrame;

		if(skam)
		{
			//for(Position j{PT_Joint,0};j<m_joints.size();j++)
			for(auto&ea:m_joints2)
			if(m_joints[ea.first]->m_selected)
			if(!parentJointSelected(ea.first)) //2021
			{
				Position j = {PT_Joint,ea.first};
				Joint *jt = ea.second;

				if(!sel) 
				{
					sel = true;

					cf = makeCurrentAnimationFrame(); //2020

					//HACK? Need to update m_final matrix.
					validateAnimSkel();
				}

				Matrix absInv; if(jt->m_parent>=0)
				{
					Joint *parent = m_joints[jt->m_parent];
					absInv = jt->m_relative * parent->m_final;
					absInv = absInv.getInverse();
				}
				else absInv = jt->getAbsoluteInverse();

				Matrix cur = jt->m_final * m * absInv;

				double rot[3]; cur.getRotation(rot);

				setKeyframe(ca,cf,j,KeyRotate,rot[0],rot[1],rot[2]);
			//	setCurrentAnimationFrame(m_currentFrame);

				// setKeyframe handles undo

				// TODO: should I really allow multiple joints here?
				//break;
			}
			//2021: Not allowing to (double) rotate joints and other data at
			//same time.
			if(sel) return;
		}

		if(fram)
		{
			for(unsigned v=0;v<m_vertices.size();v++)
			if(m_vertices[v]->m_selected)
			{
				if(!sel) 
				{
					sel = true;

					cf = makeCurrentAnimationFrame(); //2020

					if(skel) validateAnim(); //Need real m_kfCoord? _resample?
				}

				Vector coord;				
				auto vt = m_vertices[v];
				bool infl = skel&&!vt->m_influences.empty(); if(infl)
				{
					//setKeyframe would make this recompute the animation for
					//every vertex.
					//validateAnim(); //Need real m_kfCoord? _resample?					
					for(int i=3;i-->0;) coord[i] = vt->m_kfCoord[i];
				}
				else interpKeyframe(ca,cf,v,coord.getVector());
				for(int i=3;i-->0;) coord[i]-=point[i];
				coord.transform(m);
				for(int i=3;i-->0;) coord[i]+=point[i];
				if(infl) 
				{
					if(!_skel_xform_abs(-1,vt->m_influences,coord))
					{
						multi++; continue;
					}					
				}
				else setFrameAnimVertexCoords(ca,cf,v,coord.getVector());
			}

			for(Position p{PT_Point,0};p<m_points.size();p++)
			if(m_points[p]->m_selected)
			{
				if(!sel) 
				{
					sel = true;

					cf = makeCurrentAnimationFrame(); //2020

					if(skel) validateAnim(); //Need real m_kfAbs? _resample?
				}

				Matrix pm;
				double rot[3],*coord = pm.getVector(3);
				auto pt = m_points[p];
				bool infl = skel&&!pt->m_influences.empty(); if(infl)
				{
					//setKeyframe would make this recompute the animation for
					//every point.
					//validateAnim(); //Need real m_kfAbs? _resample?
					for(int i=3;i-->0;) coord[i] = pt->m_kfAbs[i];
				}
				else interpKeyframe(ca,cf,p,coord,rot,nullptr);
				for(int i=3;i-->0;) coord[i]-=point[i];				
				//pm.setTranslation(coord[0],coord[1],coord[2]);
				pm.setRotation(infl?pt->m_kfRot:rot);
				pm = pm * m;
				//pm.getTranslation(coord);
				for(int i=3;i-->0;) coord[i]+=point[i];				
				if(infl)				
				if(!_skel_xform_abs(-1,pt->m_influences,*(Vector*)coord))
				{
					multi++; continue;
				}
				else _skel_xform_rot(-1,pt->m_influences,pm);
				pm.getRotation(rot);
				setKeyframe(ca,cf,p,KeyTranslate,coord[0],coord[1],coord[2]);
				setKeyframe(ca,cf,p,KeyRotate,rot[0],rot[1],rot[2]);
			}
		}
	}
	//else //Enabling modeling in animation mode.
	{
		if(!fram)
		{
			bool verts = false;
			for(auto*v:m_vertices) if(v->m_selected)
			{
				if(!sel)
				{
					sel = true;

					if(skel) validateAnim(); //Need real m_kfCoord? _resample?
				} 

				bool infl = skel&&!v->m_influences.empty();
				Vector coord(infl?v->m_kfCoord:v->m_coord);
				for(int i=3;i-->0;) coord[i]-=point[i];
				coord.transform(m);
				for(int i=3;i-->0;) coord[i]+=point[i];
				if(infl&&!_skel_xform_abs(-1,v->m_influences,coord))
				{
					multi++; continue;
				}
				for(int i=3;i-->0;)
				v->m_coord[i] = coord[i];

				verts = true;
			}
			if(verts)
			{
				m_changeBits |= MoveGeometry;

				invalidateNormals(); //OVERKILL
			}
		}
		
		//NOTE: qm is just in case m is not affine.
		Matrix pm,qm;

		if(!skam)
		{
			bool inval = false;

			// NOTE: This code assumes that if a bone joint is rotated,
			// all children are rotated with it. That may not be what
			// the user expects. To prevent this I would have to find
			// unselected joints whose direct parent was selected
			// and invert the operation on those joints.
			//for(unsigned j=0;j<m_joints.size();j++)
			for(auto&ea:m_joints2)
			if(m_joints[ea.first]->m_selected)
			if(!parentJointSelected(ea.first))
			{
				sel = inval = true;

				validateSkel();

				m_changeBits|=MoveOther; //2020

				//Joint *jt = m_joints[j];
				Joint *jt = ea.second;

				Matrix &hack = jt->m_absolute;
				double swap[3];
				hack.getTranslation(swap);
				{
					for(int i=3;i-->0;) hack.getVector(3)[i]-=point[i];
					qm = hack * m;
					for(int i=3;i-->0;) qm.getVector(3)[i]+=point[i];
				}
				hack.setTranslation(swap);

				if(jt->m_parent>=0)
				qm = qm * m_joints[jt->m_parent]->getAbsoluteInverse();

				//WARNING! Assuming m isn't a scale matrix.				
				qm.normalizeRotation();
				qm.getRotation(jt->m_rot);
				qm.getTranslation(jt->m_rel);
			}
			if(inval) invalidateSkel();
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

		if(!fram)
		{
			for(auto*p:m_points) if(p->m_selected)
			{
				if(!sel)
				{
					sel = true;

					if(skel) validateAnim(); //Need real m_kfAbs? _resample?
				}

				if(skel&&!p->m_influences.empty())
				{
					Matrix pm;
					double *coord = pm.getVector(3);
					for(int i=3;i-->0;) 
					coord[i] = p->m_kfAbs[i]-point[i];				
					//pm.setTranslation(coord[0],coord[1],coord[2]);
					pm.setRotation(p->m_kfRot);
					pm = pm * m;
					//pm.getTranslation(coord);
					for(int i=3;i-->0;) coord[i]+=point[i];					
					if(!_skel_xform_abs(-1,p->m_influences,*(Vector*)coord))
					{
						multi++; continue;
					}
					_skel_xform_rot(-1,p->m_influences,pm);
					pm.getRotation(p->m_rot);
					for(int i=3;i-->0;) p->m_abs[i] = coord[i];
				}
				else f(*p);

				m_changeBits|=MoveOther; //2020
			}
		}

			if(skam&&sel) invalidateAnim();


		for(unsigned p=0;p<m_projections.size();p++)
		{
			if(m_projections[p]->m_selected)
			{
				sel = true;

				m_changeBits|=MoveOther; //2020

				f(*m_projections[p]);				

				applyProjection(p);
			}
		}

		if(sel)
		{
			Undo<MU_RotateSelected>(this,m,point);
		}
	}

	if(multi) model_inv_infls_msg(this,multi);

	if(!sel) model_status(this,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel","No selection"));
}

//2020: Manipulating m to implement scale
//void Model::applyMatrix(const Matrix &m, OperationScopeE scope, bool undoable)
void Model::applyMatrix(Matrix m, OperationScopeE scope, bool undoable)
{
	//LOG_PROFILE(); //???
	
	//NOTE: I think "undoable" is in case the matrix
	//isn't invertible. 
	if(m_undoEnabled)
	{
		if(undoable) 
		{
			//CAUTION: m IS MODIFIED BELOW
			Undo<MU_ApplyMatrix>(this,m,scope);
		}
		else clearUndo(); //YIKES!!!
	}

	bool global = scope==OS_Global;

	unsigned vcount = m_vertices.size();
	unsigned bcount = m_joints.size();
	unsigned pcount = m_points.size();
	unsigned rcount = m_projections.size();
	
	bool verts = false;
	for(unsigned v=0;v<vcount;v++)
	if(global||m_vertices[v]->m_selected)
	{			
		verts = true;

		m.apply3x(m_vertices[v]->m_coord);

		for(auto*ea:m_vertices[v]->m_frames) 
		{
			m.apply3x(ea->m_coord);
		}
	}
	if(verts) 
	{
		m_changeBits |= MoveGeometry;

		invalidateNormals(); //OVERKILL
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
		m_changeBits |= MoveOther;

		validateSkel();

		if(!global) for(unsigned b=bcount;b-->0;)
		{	
			if(m_joints[b]->m_selected&&!parentJointSelected(b))
			{
				g(*m_joints[b]);
			}		
		}
		else g(*m_joints[0]);
		
		invalidateSkel(); 
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
		{
			//TODO: Make optional (can be annoying)
			obj.m_xyz[i]*=scale[i]; 
			//NEW: Correct???
			obj.m_abs[i]*=scale[i]; 
		}
	};	

	double q4[4]; bool have_q4 = false;
	for(Position p={PT_Point,pcount};p-->0;)
	if(global||m_points[p]->m_selected)
	{
		f(*m_points[p]);

		for(size_t anim=m_anims.size();anim-->0;)
		{
			auto *fa = m_anims[anim];
			auto it = fa->m_keyframes.find(p);
			if(it==fa->m_keyframes.end()) continue;
			else
			for(auto*&kf:it->second)
			if(kf->m_interp2020!=InterpolateCopy) //2022
			switch(kf->m_isRotation)
			{
			case KeyTranslate:
				//no, the points need to rotate, etc.
				//m.translateVector(kf->m_parameter);
				m.apply3x(kf->m_parameter);
				break;
			case KeyRotate:			
			{
				auto &q = *(Quaternion*)q4;
				if(!have_q4)
				{
					have_q4 = true;
					m.getRotationQuaternion(q);
				}				
				Quaternion q2;
				q2.setEulerAngles(kf->m_parameter);
				q2 = q*q2;
				q2.getEulerAngles(kf->m_parameter);				
			}
			break;
			//FIX ME
			//What about scale? //Correct?
			case KeyScale:
				for(int i=3;i-->0;)
				kf->m_parameter[i]*=scale[i]; 
				break;
			}
			else //InterpolateCopy?
			{
				auto i = (unsigned)(&kf-it->second.data());
				_reset_InterpolateCopy(anim,KeyNONE,i,it->second); 
			}
			if(m_anims[anim]->m_wrap) //DICEY
			{
				_reset_InterpolateCopy(anim,KeyAny,0,it->second); 
			}
		}
	}

	//NOTE: applyProjection may validateAnim
	invalidateAnim();
	//YUCK: Projections can't have nonuniform
	//scale... what to do then?
	scale[0]+=scale[1];
	scale[0]+=scale[2];
	scale[0]/=3;
	scale[1] = scale[0];
	scale[2] = scale[0];
	for(unsigned r=rcount;r-->0;)	
	if(global||m_projections[r]->m_selected)
	{
		f(*m_projections[r]);

		applyProjection(r); //2020
	}	
}

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
bool Model::subdivideSelectedTriangles()
{
	//LOG_PROFILE(); //???

	sorted_list<SplitEdgesT> seList;

	Undo<MU_SubdivideSelected>(this);
	Undo<MU_SubdivideTriangle> undo(this);
	
	m_undoEnabled = false; //!!!

	m_changeBits|=SelectionVertices|SelectionFaces; //2020

	unsigned vertexStart = m_vertices.size();
	
	unsigned tlen = m_triangles.size();
	unsigned tnew = tlen;	
	for(unsigned t=0;t<tlen;t++)	
	{
		auto *tp = m_triangles[t];

		if(!tp->m_selected) continue;
				
		unsigned verts[3];
		for(unsigned v=0;v<3;v++)
		{
			unsigned a = tp->m_vertexIndices[v];
			unsigned b = tp->m_vertexIndices[(v+1)%3];
			if(b<a) std::swap(a,b);

			unsigned index;
			SplitEdgesT e{a,b};
			if(!seList.find_sorted(e,index))
			{
				double pa[3],pb[3];
				getVertexCoords(a,pa);
				getVertexCoords(b,pb);
				for(int i=3;i-->0;)
				pa[i] = (pa[i]+pb[i])*0.5f;

				int vNew = m_vertices.size();
				addVertex(a,pa);
				m_vertices.back()->m_selected = true;
				e.vNew = vNew;
				seList.insert_sorted(e,index);
				verts[v] = vNew;
			}
			else verts[v] = seList[index].vNew;
		}

		addTriangle(tp->m_vertexIndices[1],verts[1],verts[0]);			
		addTriangle(tp->m_vertexIndices[2],verts[2],verts[1]);			
		addTriangle(verts[0],verts[1],verts[2]);			

		setTriangleVertices(t,tp->m_vertexIndices[0],verts[0],verts[2]);

		if(undo) undo->subdivide(t,tnew,tnew+1,tnew+2);

		int g = tp->m_group; for(int i=3;i-->0;)
		{
			m_triangles[tnew+i]->m_selected = true;

			if(g>=0) addTriangleToGroup(g,tnew+i); //2022
		}
		
		float dst[2][3], (*st)[3] = &tp->m_s; //2022
		{
			float tc[2][3]; for(int i=2;i-->0;) 
			{
				tc[i][0] = (st[i][0]+st[i][1])*0.5f;
				tc[i][1] = (st[i][1]+st[i][2])*0.5f;
				tc[i][2] = (st[i][2]+st[i][0])*0.5f;
			}			
			dst[0][0] = st[0][1]; dst[0][1] = tc[0][1]; dst[0][2] = tc[0][0]; //s
			dst[1][0] = st[1][1]; dst[1][1] = tc[1][1]; dst[1][2] = tc[1][0]; //t
			setTextureCoords(tnew+0,dst);
			dst[0][0] = st[0][2]; dst[0][1] = tc[0][2]; dst[0][2] = tc[0][1]; //s
			dst[1][0] = st[1][2]; dst[1][1] = tc[1][2]; dst[1][2] = tc[1][1]; //t
			setTextureCoords(tnew+1,dst);
			dst[0][0] = tc[0][0]; dst[0][1] = tc[0][1]; dst[0][2] = tc[0][2]; //s
			dst[1][0] = tc[1][0]; dst[1][1] = tc[1][1]; dst[1][2] = tc[1][2]; //t
			setTextureCoords(tnew+2,dst);
			dst[0][0] = st[0][0]; dst[0][1] = tc[0][0]; dst[0][2] = tc[0][2]; //s
			dst[1][0] = st[1][0]; dst[1][1] = tc[1][0]; dst[1][2] = tc[1][2]; //t
			setTextureCoords(t,dst); //st
		}

		tnew+=3;
	}
	
	if(undo)
	{
		m_undoEnabled = true; //!!!
		for(unsigned i=vertexStart;i<m_vertices.size();i++)
		undo->addVertex(i);		
	}	

	if(vertexStart==m_vertices.size()) return false;

	invalidateNormals(); //OVERKILL

	return true;
}

//void Model::unsubdivideTriangles(unsigned t1, unsigned t2, unsigned t3, unsigned t4)
void Model::subdivideSelectedTriangles_undo(unsigned t1, unsigned t2, unsigned t3, unsigned t4)
{
	//LOG_PROFILE(); //???

	//This should implement a MU object or shouldn't be a regular API method.
	assert(!m_undoEnabled);

//	bool swap = false; 
//	std::swap(swap,m_undoEnabled);
	{
		Triangle *t[3] = {m_triangles[t1],m_triangles[t2],m_triangles[t3]};

		setTriangleVertices(t1,
		t[0]->m_vertexIndices[0],t[1]->m_vertexIndices[0],t[2]->m_vertexIndices[0]);
		setTextureCoords(t1,1,t[1]->m_s[0],t[1]->m_t[0]);
		setTextureCoords(t1,2,t[2]->m_s[0],t[2]->m_t[0]);

		deleteTriangle(t4);
		deleteTriangle(t3);
		deleteTriangle(t2);
	}
//	std::swap(swap,m_undoEnabled);

	invalidateNormals(); //OVERKILL
}

void Model::prioritizeSelectedTriangles(bool first)
{
	auto *tris = m_triangles.data();

	unsigned i,sel,sz = getTriangleCount();

	int_list map(sz);

	for(auto pass=sel=0;pass<=1;pass++,first=!first)
	{
		for(i=0;i<sz;i++)
		if(first==tris[i]->m_selected) map[i] = sel++;
	}	
	
	remapTrianglesIndices(map);

	if(Undo<MU_RemapTrianglesIndices>undo=this)
	{
		undo->map.swap(map); //???
	}
}
void Model::reverseOrderSelectedTriangle()
{
	auto *tris = m_triangles.data();

	int sz = getTriangleCount();

	int_list map(sz);

	for(int i=0,sel=sz;i<sz;i++)
	if(tris[i]->m_selected) 
	{
		while(!tris[--sel]->m_selected)
		;
		map[i] = sel;
	}
	else map[i] = i;
	
	remapTrianglesIndices(map);

	if(Undo<MU_RemapTrianglesIndices>undo=this)
	{
		undo->map.swap(map); //???
	}
}

unsigned Model::_op = !0;
bool Model::_op_ccw = !0;
void Model::operationComplete(const char *opname, int compound_ms)
{
	Model::_op++;

	validateSkel();
	validateAnim();
	endSelectionDifference();

	m_undoMgr->operationComplete(opname,compound_ms);
	
	updateObservers();
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

void Model::undo(int how)
{
	//LOG_PROFILE(); //???

	assert(m_undoEnabled); //2022

	UndoList *list; switch(how)
	{
	default: assert(0); return;
	case -1: list = m_undoMgr->undo(); break;
	case +1: list = m_undoMgr->redo(); break;
	case  0: list = m_undoMgr->undoCurrent(); break;
	}	
	if(list==nullptr) return;

	//log_debug("got atomic undo list\n"); //???
	
	//2022: m_undoEnacted helps to distinguish between
	//a scenario where undo facilities are disabled, in
	//which case everything seems to come from an undo.
	//NOTE: There's a larger problem where non "modal" 
	//"dialogs" have to use "ignoreChange" states when
	//making changes. This can help with that in theory
	//... especially if dialogs can know if they're the
	//active window or not.
	//setUndoEnabled(false);
	m_undoEnabled = false; m_undoEnacted = true; 

	if(how==1) //redo 
	{
		// process front to back	
		for(auto it=list->begin();it!=list->end();it++)
		{
			ModelUndo *redo = static_cast<ModelUndo*>(*it);
			redo->redo(this);
		}
	}
	else //undo/undoCurrent()
	{
		// process back to front
		for(auto rit=list->rbegin();rit!=list->rend();rit++)
		{
			ModelUndo *undo = static_cast<ModelUndo*>(*rit);
			undo->undo(this);
		}		
	}

	validateSkel();

	m_selecting = false; //??? //endSelectionDifference?

	if(how!=0) //undo/redo //Continued below...
	{
		updateObservers();
	}

	//setUndoEnabled(true);
	m_undoEnabled = true; m_undoEnacted = false;

	if(how==0) //undoCurrent? Continued???
	{
		//2022: In consolidating these three APIs to add m_undoEnacted
		//I'm looking at this and I'm not so confident the undoCurrent
		//path should differ from undo/redo in this way? I believe the
		//point of this is to make "Cancel" button behavior match "OK"
		//button behavior, i.e. operationComplete().

		//https://github.com/zturtleman/mm3d/issues/90
		//Treating operationComplete as-if calling updateObservers from outside.
		updateObservers();
	}
}

void Model::hideVertex(unsigned i, unsigned layer) //UNSAFE???
{
	if(i<m_vertices.size()) 
	{
		//bool &v = m_vertices[i]->m_visible;
		//v = how==-1?!v:!how;
		m_vertices[i]->hide(layer);
	}
	else assert(0);
}
void Model::hideTriangle(unsigned i, unsigned layer) //UNSAFE???
{
	if(i<m_triangles.size())
	{
		//bool &v = m_triangles[i]->m_visible;
		//v = how==-1?!v:!how;
		m_triangles[i]->hide(layer);
	}
	else assert(0);
}
void Model::hideJoint(unsigned i, unsigned layer) //UNSAFE???
{
	if(i<m_joints.size())
	{
		//bool &v = m_joints[i]->m_visible;
		//v = how==-1?!v:!how;
		m_joints[i]->hide(layer);
	}
	else assert(0);
}
void Model::hidePoint(unsigned i, unsigned layer) //UNSAFE???
{
	if(i<m_points.size()) 
	{
		//bool &v = m_points[i]->m_visible;
		//v = how==-1?!v:!how;
		m_points[i]->hide(layer);
	}
	else assert(0);
}

bool Model::hideSelected(bool how, unsigned layer)
{
	//LOG_PROFILE(); //???

	//If assigning to a layer the elements won't
	//already be on that layer, so it's no good
	//to screen them out.
	//
	// NOTE: This API allows for more variations
	// than this will support. Only any existing
	// patterns in use by editors are considered.
	//
	auto lv = layer||1==m_primaryLayers?~1:m_primaryLayers&~1;

	if(layer&&how) lv|=1; //Use hidden selection.

	Undo<MU_Hide> undo(this);

	// Need to track whether we are hiding a vertex and any triangles attached to it,
	// or hiding a triangle,and only vertices if they are orphaned
	for(auto*vp:m_vertices) vp->m_marked = how==vp->m_selected;

	// Hide selected triangles
	for(auto*tp:m_triangles)	
	if(how==tp->m_selected&&tp->visible(lv))
	{
		for(int i:tp->m_vertexIndices)
		{
			m_vertices[i]->m_marked = false;
		}

		if(tp->hide_difference(layer))
		{
			if(undo) undo->setHideDifference(tp,layer);

			tp->hide(layer);
		}
	}

	// Hide triangles with a lone vertex that is selected
	for(auto*tp:m_triangles)
	if(tp->hide_difference(layer))
	if(tp->visible(lv))
	{
		for(int i:tp->m_vertexIndices)
		if(m_vertices[i]->m_marked)
		{
			if(undo) undo->setHideDifference(tp,layer);

			tp->hide(layer);

			break; //!
		}
	}

	auto lv2 = 1<<layer; //DICEY

	for(auto*vp:m_vertices)
	if(vp->hide_difference(layer)&&vp->visible(lv))
	{
		if(vp->m_faces.empty()) //Orphan?
		{
			if(how!=vp->m_selected) continue;
		}
		else // Triangle is visible, vertices must be too
		{
			bool mark = false;
			for(auto&ea:vp->m_faces) 
			{
				auto &f = *ea.first;

				//This won't work any longer with this
				//layers change. Instead, try to reject
				//any faces not qualified to be modified.
				//if(f.m_visible)
				//may be only vertices are selected...
				//in which case this hides the triangles
				//but not the vertices (i.e. opposite of
				//of what's implied)
			//	if(f.m_visible2!=lv2||how!=f.m_selected)
				if(f.m_visible2!=lv2)
				{
					mark = true; break;	
				}
			}
			if(mark) continue;
		}

		if(undo) undo->setHideDifference(vp,layer);
			
		vp->hide(layer);
	}

	for(auto*jp:m_joints)	
	if(jp->hide_difference(layer))
	if(how==jp->m_selected&&jp->visible(lv))
	{
		if(undo) undo->setHideDifference(jp,layer);

		jp->hide(layer);
	}

	for(auto*pp:m_points)
	if(pp->hide_difference(layer))
	if(how==pp->m_selected&&pp->visible(lv))
	{
		if(undo) undo->setHideDifference(pp,layer);
			
		pp->hide(layer);
	}

	//NOTE: This actually does selection undo logic, ensuring
	//hidden elements aren't seen as selected.
	if(how&&0==(1<<layer&m_primaryLayers)) unselectAll();

	m_changeBits|=RedrawAll; //2022
	
	return true;
}

bool Model::unhideAll()
{
	//LOG_PROFILE(); //???

	auto lv = m_primaryLayers;

	if(lv==1) lv = ~0; else assert(~lv&1);

	Undo<MU_Hide> undo(this);

	for(auto*p:m_vertices) if(p->hidden(lv))
	{
		if(undo) undo->setHideDifference(p,p->m_layer);

		p->unhide();
	}
	for(auto*p:m_triangles) if(p->hidden(lv))
	{
		if(undo) undo->setHideDifference(p,p->m_layer);

		p->unhide();
	}
	for(auto*p:m_joints) if(p->hidden(lv))
	{
		if(undo) undo->setHideDifference(p,p->m_layer);

		p->unhide();
	}	
	for(auto*p:m_points) if(p->hidden(lv))
	{
		if(undo) undo->setHideDifference(p,p->m_layer);

		p->unhide();
	}

	m_changeBits|=RedrawAll; //2022

	return true;
}

void Model::invertHidden()
{
	//LOG_PROFILE(); //???

	auto lv = m_primaryLayers;

	if(lv==1) lv = ~0; else assert(~lv&1);

	//first pass (undo)
	{
		int i = -1; for(auto*p:m_triangles)
		{
			i++; if(p->m_selected&&p->visible(lv))
			{
				unselectTriangle(i);
			}
		}
		i = -1; for(auto*p:m_vertices)
		{
			i++; if(p->m_selected&&p->visible(lv))
			{
				unselectVertex(i);
			}
		}		
		i = -1; for(auto*p:m_joints)
		{
			i++; if(p->m_selected&&p->visible(lv))
			{
				unselectBoneJoint(i);
			}
		}	
		i = -1; for(auto*p:m_points)
		{
			i++; if(p->m_selected&&p->visible(lv))
			{
				unselectPoint(i);
			}
		}
	}
	//second pass (undo)
	{
		Undo<MU_Hide> undo(this);

		for(auto*p:m_vertices) if(lv&(1<<p->m_layer))
		{
			auto l = p->m_visible1?p->m_layer:0;
		
			if(undo) undo->setHideDifference(p,l);

			p->hide(l);
		}
		for(auto*p:m_triangles) if(lv&(1<<p->m_layer))
		{
			auto l = p->m_visible1?p->m_layer:0;

			if(undo) undo->setHideDifference(p,l);

			p->hide(l);
		}
		for(auto*p:m_joints) if(lv&(1<<p->m_layer))
		{
			auto l = p->m_visible1?p->m_layer:0;

			if(undo) undo->setHideDifference(p,l);

			p->hide(l);
		}	
		for(auto*p:m_points) if(lv&(1<<p->m_layer))
		{
			auto l = p->m_visible1?p->m_layer:0;
						
			if(undo) undo->setHideDifference(p,l);

			p->hide(l);
		}
	}

	m_changeBits|=RedrawAll; //2022
}

bool Model::getBoundingRegion(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2)const
{
	//REMOVE ME: Duplicate of getSelectedBoundingRegion

	if(!x1||!y1||!z1||!x2||!y2||!z2) return false; //???

	validateAnim(); //2020

	int visible = 0;
	bool havePoint = false; //REMOVE ME
	*x1 = *y1 = *z1 = *x2 = *y2 = *z2 = 0.0;

	auto lv = m_primaryLayers;

	for(auto*vp:m_vertices)
	{
		if(!vp->visible(lv)) continue;

		visible++;

		if(!havePoint) //???
		{
			havePoint = true;

			*x1 = *x2 = vp->m_absSource[0];
			*y1 = *y2 = vp->m_absSource[1];
			*z1 = *z2 = vp->m_absSource[2];
		}
		else //???
		{
			if(vp->m_absSource[0]<*x1)
			{
				*x1 = vp->m_absSource[0];
			}
			if(vp->m_absSource[0]>*x2)
			{
				*x2 = vp->m_absSource[0];
			}
			if(vp->m_absSource[1]<*y1)
			{
				*y1 = vp->m_absSource[1];
			}
			if(vp->m_absSource[1]>*y2)
			{
				*y2 = vp->m_absSource[1];
			}
			if(vp->m_absSource[2]<*z1)
			{
				*z1 = vp->m_absSource[2];
			}
			if(vp->m_absSource[2]>*z2)
			{
				*z2 = vp->m_absSource[2];
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

void Model::invertNormal(unsigned triangleNum)
{
	//LOG_PROFILE(); //???

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

			Undo<MU_InvertNormals> undo(this,this);			
			if(undo) undo->addTriangle(triangleNum);
		}

		invalidateNormals(); //OVERKILL
	}
}
void Model::invertNormals(const int_list &l)
{
	//LOG_PROFILE(); //???

	if(l.empty()) return;

	Undo<MU_InvertNormals> undo(this,this);
	if(undo) undo->addTriangles(l);

	invalidateNormals(); //OVERKILL

	auto *tl = m_triangles.data();

	bool swap = false; 
	std::swap(swap,m_undoEnabled);

	for(auto i:l) if(i<(int)m_triangles.size())
	{
		auto *tn = tl[i];

		//NOTE: This will change the high 2 bits of Vertex::m_faces
		//std::swap(tn->m_vertexIndices[0],tn->m_vertexIndices[2]);
		setTriangleVertices(i,tn->m_vertexIndices[2],tn->m_vertexIndices[1],tn->m_vertexIndices[0]);
		std::swap(tn->m_s[0],tn->m_s[2]);
		std::swap(tn->m_t[0],tn->m_t[2]);
	}
	else assert(0);

	if(swap) m_undoEnabled = true;
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

//void Model::sendUndo(Undo *undo, bool listCombine)
void Model::sendUndo(::Undo *undo)
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
			m_undoMgr->addUndo(undo/*,listCombine*/);
		}
	}
	else //YUCK (ANTI-PATTERN)
	{
		undo->undoRelease();
		undo->release();
	}
}
void Model::appendUndo(::Undo *undo)
{
	if(!undo) return; //2020

	//NOTE: This is a fix for setSelectedUv.
	//It does no good to undo the UV selection
	//before the operation that did the selecting.

	if(m_undoEnabled
	&&!m_undoMgr->canUndoCurrent()
	&&*m_undoMgr->getUndoOpName())
	{
		auto &st = m_undoMgr->getUndoStack();
		if(!st.empty()&&!st.back()->empty()
		&&typeid(*st.back()->back())!=typeid(*undo)) 
		{
			st.back()->push_back(undo);
			return;
		}
	}

	sendUndo(undo/*,false*/);
}

#endif // MM3D_EDIT //???

int Model::addFormatData(FormatData *fd)
{
	//2021: I think it's safe to not notify
	//of this change since this data isn't
	//meaningful to MM3D
	//NOTE: deleteFormatData would need to
	//notify if restored
	//m_changeBits |= AddOther; //Misfit

	int n = -1; if(fd)
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
	for(unsigned n=0;n<count;n++)
	{
		FormatData *fd = m_formatData[n];
		if(PORT_strcasecmp(fd->format.c_str(),format)==0&&fd->index==index)
		return fd;
	}
	return nullptr;
}

bool Model::setBoneJointParent(unsigned joint, int parent, bool validate)
{
	if(joint>=m_joints.size()) return false;
	
	if(parent==m_joints[joint]->m_parent) return true;

	if(joint==parent)
	{
		model_status(this,StatusError,STATUSTIME_LONG,TRANSLATE("LowLevel","Same joint"));
		return false;
	}

	if(validate) validateSkel();

	//2021: It's not necessary to reuse m_absolute 
	//here and it's misleading some since the goal
	//here is to compute a new m_relative.
	auto &m = m_joints[joint]->m_absolute;

	//https://github.com/zturtleman/mm3d/issues/132
	if(parent>=0)
	m = m * m_joints[parent]->getAbsoluteInverse();

	parentBoneJoint(joint,parent,m);

	if(validate) validateSkel(); return true;
}
void Model::parentBoneJoint(unsigned index, int parent, Matrix &m)
{
	auto jt = m_joints[index];
	
	for(int cmp,i=parent;i>=0;i=cmp) //2021: cyclic?
	{
		cmp = m_joints[i]->m_parent; if(index==cmp)
		{
			setBoneJointParent(i,m_joints[index]->m_parent,false);
			break;
		}
	}

	//WARNING! m MODIFIED BELOW!
	Undo<MU_SetJointParent>(this,index,parent,m,jt);

	//readFile calls this if it can't create the joints in
	//parental order
	parentBoneJoint2(index,parent);

	//TODO: Matrix::decompose or something?
	bool scale = false;
	m.getScale(jt->m_xyz); //2021
	{
		for(int i=3;i-->0;)	
		if(fabs(1-jt->m_xyz[i])>0.000005)
		{
			scale = true;
		}
		else jt->m_xyz[i] = 1;

		if(scale) m.normalizeRotation(); //MODIFYING!
	}
	m.getRotation(jt->m_rot);
	m.getTranslation(jt->m_rel); invalidateSkel(); 
}
void Model::parentBoneJoint2(unsigned index, int parent)
{
	auto jt = m_joints[index];

	m_changeBits |= MoveOther|AddOther; //???

	jt->m_parent = parent; 

	for(auto it=m_joints2.begin();it<m_joints2.end();it++)		
	{
		if(it->second==jt)
		{
			m_joints2.erase(it); break;
		}
	}
	if(parent<0)
	{
		m_joints2.insert(m_joints2.begin(),{index,jt});
	}
	else for(auto it=m_joints2.begin();it<m_joints2.end();it++)		
	{
		if(it->first==parent)
		{
			m_joints2.insert(it+1,{index,jt}); break;
		}
	}

	//Recursively move any descendents that were leap-frogged
	//over to be after this parent
	restart: for(auto&ea:m_joints2)
	{
		if(ea.first==index) break;

		if(ea.second->m_parent==index)
		{
			parentBoneJoint2(ea.first,index); 
			
			goto restart; //iterator is invalidated
		}
	}
}

int Model::getBoneJointParent(unsigned j)const
{
	return j<m_joints.size()?m_joints[j]->m_parent:-1;
}

int Model::getPointByName(const char *name)const
{
	for(unsigned int point=0;point<m_points.size();point++)	
	if(0==strcmp(name,m_points[point]->m_name.c_str()))
	return point; return -1;
}

bool Model::getVertexCoords(unsigned vertexNumber, double *coord)const
{
	if(coord&&vertexNumber<m_vertices.size())
	{
		for(int t=3;t-->0;)
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
		for(int t=3;t-->0;)
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

		m = m_joints[jointNumber]->m_final; return true;
	}
	return false;
}

bool Model::getBoneJointAbsoluteMatrix(unsigned jointNumber, Matrix &m)const
{
	if(jointNumber<m_joints.size())
	{
		m = m_joints[jointNumber]->m_absolute; return true;
	}
	return false;
}

bool Model::getBoneJointRelativeMatrix(unsigned jointNumber, Matrix &m)const
{
	if(jointNumber<m_joints.size())
	{
		m = m_joints[jointNumber]->m_relative; return true;
	}
	return false;
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
	return m_triangles[triangleNumber]->m_vertexIndices[vertexIndex];
	return -1;
}

bool Model::getNormal(unsigned triangleNum, unsigned vertexIndex, double *normal)const
{
	if(triangleNum<m_triangles.size()&&vertexIndex<3)
	{
		for(int t=0;t<3;t++)
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
		for(int t=0;t<3;t++)
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
	//UPDATE: Nope, getNormal no longer validates, so this is
	//useful for getting a flat-normal after addTriangle
	//(polytool.cc must do this to avoid validation)
	//(Should be updating normals piecemeal instead)
	//https://github.com/zturtleman/mm3d/issues/116
	if(m_animationMode?m_validAnimNormals:m_validNormals)
	{
		//2020: Don't calculate if valid.
		memcpy(normal,tri->m_flatSource,3*sizeof(double));
		return true;
	}	

	calculateFlatNormal(tri->m_vertexIndices,normal); 
	
	return true;
}
double Model::calculateFlatNormal(unsigned vertices[3], double normal[3])const
{
	double *v0 = m_vertices[vertices[0]]->m_absSource;
	double *v1 = m_vertices[vertices[1]]->m_absSource;
	double *v2 = m_vertices[vertices[2]]->m_absSource;

	return calculate_normal(normal,v0,v1,v2);
}
bool Model::getFlatNormalUnanimated(unsigned t, double *normal)const
{
	if(t>=m_triangles.size()) return false;
	
	auto *tri = m_triangles[t];

	//SEE getFlatNormal COMMENT
	//https://github.com/zturtleman/mm3d/issues/116
	if(m_validNormals)
	{
		//2020: Don't calculate if valid.
		memcpy(normal,tri->m_flatNormal,3*sizeof(double));
		return true;
	}

	double *v0 = m_vertices[tri->m_vertexIndices[0]]->m_coord;
	double *v1 = m_vertices[tri->m_vertexIndices[1]]->m_coord;
	double *v2 = m_vertices[tri->m_vertexIndices[2]]->m_coord;

	calculateFlatNormalUnanimated(tri->m_vertexIndices,normal);

	return true;
}
double Model::calculateFlatNormalUnanimated(unsigned vertices[3], double normal[3])const
{
	double *v0 = m_vertices[vertices[0]]->m_coord;
	double *v1 = m_vertices[vertices[1]]->m_coord;
	double *v2 = m_vertices[vertices[2]]->m_coord;

	return calculate_normal(normal,v0,v1,v2);
}

bool Model::getBoneVector(unsigned joint, double *vec, const double *coord)const
{
	if(joint>=m_joints.size()) return false;

	double jcoord[3] = { 0,0,0 };
	getBoneJointCoords(joint,jcoord);

	double cdist = 0.0;
	int child = -1;
	size_t bcount = m_joints.size();

	// find best child bone joint vector based on the min distance between
	// the target coordinate and the child bone joint coordinate
	for(size_t b=0;b<bcount;b++)	
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
	

	if(child<0)
	{
		int parent = getBoneJointParent(joint);
		if(parent<0)
		{
			// no children, no parent
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

	normalize3(vec); return true;
}

void Model::Vertex::_erase_face(Triangle *f, unsigned s)
{
	auto it = std::find(m_faces.begin(),m_faces.end(),std::make_pair(f,s));
	if(it!=m_faces.end()) m_faces.erase(it); else assert(0);
}
void Model::calculateNormals()
{
	//CAUTION: I've changed this to fill out the animation "source" normals
	//when selected.

	//LOG_PROFILE(); //???

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
		for(int i=3;i-->0;) if(1)
		{
			tri->m_angleSource[i] = acos(dp[i]);
		}
		else tri->m_angleSource[i] = 1; //TESTING
	}

	// Apply accumulated normals to triangles

	for(unsigned g=0;g<m_groups.size();g++)
	{
		Group *grp = m_groups[g];

		double maxAngle = 
		std::max(0.5*PIOVER180,grp->m_angle*PIOVER180);

		for(int i:grp->m_triangleIndices)
		{
			Triangle *tri = m_triangles[i];
			tri->m_marked = true;
			for(int vert=3;vert-->0;)
			{
				unsigned v = tri->m_vertexIndices[vert];
								
				auto &acl = m_vertices[v]->m_faces;

				double A = 0;
				double B = 0;
				double C = 0; for(auto&ea:acl)
				{
					auto tri2 = ea.first;
					auto ea_norm = tri2->m_flatSource;
					double dotprod = dot3(tri->m_flatSource,ea_norm);

					// Don't allow it to go over 1
					double angle = 0;
					if(dotprod<0.99999)
					{
						angle = fabs(acos(dotprod));
					}

					double w = tri2->m_angleSource[ea.second];

					if(angle<=maxAngle)
					{
						A += ea_norm[0]*w;
						B += ea_norm[1]*w;
						C += ea_norm[2]*w;
					}
				}

				double len = magnitude(A,B,C);

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
		double maxAngle = 45*PIOVER180; //???

		for(int vert=3;vert-->0;)
		{
			unsigned v = tri->m_vertexIndices[vert];
			
			//std::vector<NormAngleAccum> &acl = acl_normmap[v];
			auto &acl = m_vertices[v]->m_faces;

			double A = 0;
			double B = 0;
			double C = 0; for(auto&ea:acl)
			{
				auto tri2 = ea.first;
				auto ea_norm = tri2->m_flatSource;
				double dotprod = dot3(tri->m_flatSource,ea_norm);

				// Don't allow it to go over 1
				double angle = 0;
				if(dotprod<0.99999)
				{
					angle = fabs(acos(dotprod));
				}

				double w = tri2->m_angleSource[ea.second];

				if(angle<=maxAngle)
				{
					A += ea_norm[0]*w;
					B += ea_norm[1]*w;
					C += ea_norm[2]*w;
				}
			}

			double len = magnitude(A,B,C);

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
		double percent = grp->m_smooth*0.0039215686274509; //1/255
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

	invalidateBspTree();
}

void Model::invalidateNormals()
{
	m_changeBits |= MoveNormals;

	//2021: movePositionUnanimated may be in animation
	//mode, plus there's no longer a clean split
	//between animation off or on.
	//if(m_animationMode) 
	m_validAnimNormals = false;
	
	m_validNormals = false;

	//NOTE: calculateNormals calls invalidateBspTree?
	invalidateBspTree();
}
void Model::invalidateAnimNormals()
{
	m_changeBits |= MoveNormals;

	m_validAnimNormals = false;

	//NOTE: calculateNormals calls invalidateBspTree?
	invalidateBspTree();
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
	//log_debug("calculating BSP tree\n");
	m_bspTree.clear();

	for(auto*gp:m_groups)
	{
		int index = gp->m_materialIndex;
		if(index>=0&&m_materials[index]->needsAlpha())
		m_bspTree.addTriangles(this,gp->m_triangleIndices);
	}
	m_bspTree.partition(); //2022

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

void Model::setTriangleMarked(unsigned t, bool marked)
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
	Model::Animation::stats();
	Model::FrameAnimVertex::stats();
//	Model::FrameAnimPoint::stats();
	Model::BspTree::stats();
//	Model::BspTree::stats2();
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

	//log_debug("purging primitive recycling lists\n");

	c += Model::Vertex::flush();
	c += Model::Triangle::flush();
	c += Model::Group::flush();
	c += Model::Material::flush();
	c += Model::Joint::flush();
	c += Model::Point::flush();
	c += Model::Keyframe::flush();
	//c += Model::SkelAnim::flush();
	c += Model::Animation::flush();
	c += Model::BspTree::flush(); //nodes
	c += Model::BspTree::flush2(); //triangles
	//c += Model::FrameAnim::flush();
	c += Model::FrameAnimVertex::flush();
//	c += Model::FrameAnimPoint::flush();

	return c;
}

//errorObj.cc
const char *modelErrStr(Model::ModelErrorE err,Model *model)
{
	switch(err)
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
		if(model)return model->getFilterSpecificError();
		else return Model::getLastFilterSpecificError();
	case Model::ERROR_UNKNOWN:
		return transll("Unknown error");
	}
	assert(0); //2022
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
	//REFACTOR ME ME
	//This shouldn't be necessary but currently
	//joint m_rot/m_xyz is in local space.
	if(m_type==PT_Joint)
	{
		auto mp = 
		m_abs!=m_absSource?&Joint::m_final:&Joint::m_absolute;
		return ((Joint*)this)->*mp;
	}

	//REMINDER: setRotation doesn't preserve scale.
	Matrix m; m.setRotation(m_rotSource); m.scale(m_xyzSource); 
	m.setTranslation(m_absSource); return m;
}
Matrix Model::Object2020::getMatrixUnanimated()const
{
	//REFACTOR ME ME
	//This shouldn't be necessary but currently
	//joint m_rot/m_xyz is in local space.
	if(m_type==PT_Joint) return ((Joint*)this)->m_absolute;

	//REMINDER: setRotation doesn't preserve scale.
	Matrix m; m.setRotation(m_rot); m.scale(m_xyz);
	m.setTranslation(m_abs); return m;
}

bool Model::setPositionCoords(const Position &pos, const double abs[3])
{
	//1) Vertex isn't an "object".
	//2) Joint uses m_rel (and computes m_abs as side effect.)
	//3) Point animation is just less code this way.
	if(pos.type<=PT_Projection)
	{
		//NOTE: I guess setPositionCoords doesn't guarantee that 
		//getPositionCoords will get back the save data, otherwise
		//it'd be necessary to call validate/calculateAnim right now.
		return movePosition(pos,abs[0],abs[1],abs[2]);
	}	
	return setPositionCoordsUnanimated(pos,abs);
}
bool Model::setPositionCoordsUnanimated(const Position &pos, const double abs[3])
{
	if(pos.type<=PT_Projection) //HACK
	{
		//FYI: See setPositionCoords notes for why this is.
		return movePositionUnanimated(pos,abs[0],abs[1],abs[2]);
	}

	//HACK: This is just _OT_Background_ at this point but  
	//I want don't want MU_SetObjectUnanimated to be using
	//setPositionCoords.
	
	Object2020 *obj = getPositionObject(pos); if(!obj) return false;

	//TODO: Let sendUndo reject no-op changes.
	if(!memcmp(obj->m_abs,abs,sizeof(*abs)*3)) return true; //2020

	m_changeBits|=MoveOther; //2020

	Undo<MU_SetObjectUnanimated>(this,pos,this,obj->m_abs,abs); 

	memcpy(obj->m_abs,abs,sizeof(*abs)*3); return true;
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

bool Model::setPositionRotation(const Position &pos, const double rot[3])
{
	if(pos.type==PT_Point&&inFrameAnimMode()
	 ||pos.type==PT_Joint&&inJointAnimMode())
	{
		Object2020 *obj = getPositionObject(pos);
		if(!obj) return false;

		makeCurrentAnimationFrame();

		return -1!=setKeyframe
		(m_currentAnim,m_currentFrame,pos,KeyRotate,rot[0],rot[1],rot[2]);
	}
	return setPositionRotationUnanimated(pos,rot);
}
bool Model::setPositionRotationUnanimated(const Position &pos, const double rot[3])
{
	Object2020 *obj = getPositionObject(pos);
	if(!obj) return false;

	//TODO: Let sendUndo reject no-op changes.
	if(!memcmp(obj->m_rot,rot,sizeof(*rot)*3)) return true; //2020

	m_changeBits|=MoveOther; //2020

	Undo<MU_SetObjectUnanimated>(this,pos,this,obj->m_rot,rot); 

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
	//HACK: Compatibility fix.
	if(pos.type==PT_Point&&inFrameAnimMode()
	 ||pos.type==PT_Joint&&inJointAnimMode())
	{
		Object2020 *obj = getPositionObject(pos); 
		if(!obj) return false;

		makeCurrentAnimationFrame();

		return -1!=setKeyframe
		(m_currentAnim,m_currentFrame,pos,KeyScale,scale[0],scale[1],scale[2]);
	}
	return setPositionScaleUnanimated(pos,scale); 
}
bool Model::setPositionScaleUnanimated(const Position &pos, const double scale[3])
{
	Object2020 *obj = getPositionObject(pos); if(!obj) return false;
			
	//TODO: Let sendUndo reject no-op changes.
	if(!memcmp(obj->m_xyz,scale,sizeof(*scale)*3)) return true; //2020

	m_changeBits|=MoveOther; //2020

	Undo<MU_SetObjectUnanimated>(this,pos,this,obj->m_xyz,scale); 

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
		if(name!=obj->m_name)
		{
			m_changeBits|=AddOther; //2020

			Undo<MU_SwapStableStr>(this,AddOther,obj->m_name);

			obj->m_name = name;
		}
		return true;
	}
	return false;
}
const char *Model::getPositionName(const Position &pos)const
{
	auto *obj = getPositionObject(pos);
	return obj?obj->m_name.c_str():nullptr;
}

bool Model::setBoneJointOffsetUnanimated(unsigned j, const double rel[3], const double rot[3], const double xyz[3])
{
	if(j>=m_joints.size()) return false;

	Position pos = {PT_Joint,j};

	if(rel)
	{
		auto *obj = m_joints[j];

		//TODO: Let sendUndo reject no-op changes.
		if(!memcmp(obj->m_rel,rel,sizeof(*rel)*3)) return true; //2020

		m_changeBits|=MoveOther; //2020

		Undo<MU_SetObjectUnanimated>(this,pos,this,obj->m_rel,rel); 

		memcpy(obj->m_rel,rel,sizeof(*rel)*3);

		invalidateSkel();
	}
	//REFACTOR ME
	{
		//THIS WORKS RIGHT NOW BECAUSE THESE ARE IN LOCAL COORDINATES
		//BUT EVENTUALLY IT WILL NEED TO BE TREATED LIKE REL ABOVE IF
		//THE API IS SWITCHED TO GLOBAL COORDINATES.

		if(rot) setPositionRotationUnanimated(pos,rot);
		if(xyz) setPositionScaleUnanimated(pos,xyz);
	}
	return true; //Assuming all success?
}
bool Model::setBoneJointOffset(unsigned j, const double rel[3], const double rot[3], const double xyz[3])
{
	if(j>=m_joints.size()) return false;

	Position pos = {PT_Joint,j};

	if(rel)
	{
		if(inJointAnimMode())
		{
			makeCurrentAnimationFrame();

			setKeyframe(m_currentAnim,m_currentFrame,pos,KeyTranslate,rel[0],rel[1],rel[2]);
		}
		else setBoneJointOffsetUnanimated(j,rel);
	}
	//REFACTOR ME
	{
		//THIS WORKS RIGHT NOW BECAUSE THESE ARE IN LOCAL COORDINATES
		//BUT EVENTUALLY IT WILL NEED TO BE TREATED LIKE REL ABOVE IF
		//THE API IS SWITCHED TO GLOBAL COORDINATES.

		if(rot) setPositionRotation(pos,rot);
		if(xyz) setPositionScale(pos,xyz);
	}
	return true; //Assuming all success?
}
bool Model::setBoneJointCoords(unsigned joint, const double *abs)
{
	return setPositionCoords({PT_Joint,joint},abs);
}
bool Model::setPointCoords(unsigned point, const double *abs)
{
	return setPositionCoords({PT_Point,point},abs);
}
bool Model::setProjectionCoords(unsigned proj, const double *coord)
{
	return setPositionCoords({PT_Projection,proj},coord);
}
bool Model::setBackgroundCoords(unsigned index, const double abs[3])
{
	return setPositionCoords({_OT_Background_,index},abs);
}
bool Model::setBackgroundCenter(unsigned index, double x, double y, double z)
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
bool Model::getBoneJointOffset(unsigned j, double rel[3], double rot[3], double xyz[3])
{
	if(j>=m_joints.size()) return false;

	if(!inJointAnimMode()) return getBoneJointOffsetUnanimated(j,rel,rot,xyz);

	if(rel) memcpy(rel,m_joints[j]->m_kfRel,sizeof(*rel)*3);
	if(rot) memcpy(rot,m_joints[j]->m_kfRot,sizeof(*rot)*3);
	if(xyz) memcpy(xyz,m_joints[j]->m_kfXyz,sizeof(*xyz)*3);
	
	return true;
}
bool Model::getBoneJointOffsetUnanimated(unsigned j, double rel[3], double rot[3], double xyz[3])
{
	if(j>=m_joints.size()) return false;

	if(rel) memcpy(rel,m_joints[j]->m_rel,sizeof(*rel)*3);
	if(rot) memcpy(rot,m_joints[j]->m_rot,sizeof(*rot)*3);
	if(xyz) memcpy(xyz,m_joints[j]->m_xyz,sizeof(*xyz)*3);
	
	return true;
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
bool Model::getBackgroundCoords(unsigned index, double coord[3])const
{
	return getPositionCoords({_OT_Background_,index},coord);
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

bool Model::setBoneJointScale(unsigned joint, const double *scale)
{
	return setPositionScale({PT_Joint,joint},scale);
}
bool Model::setPointScale(unsigned point, const double *scale)
{
	return setPositionScale({PT_Point,point},scale);
}
bool Model::setProjectionScale(unsigned proj, double scale)
{
	double xyz[3] = {scale,scale,scale};
	return setPositionScale({PT_Projection,proj},xyz);
}
bool Model::setBackgroundScale(unsigned index, double scale)
{
	double xyz[3] = {scale,scale,scale};
	return setPositionScale({_OT_Background_,index},xyz);
}
double Model::getProjectionScale(unsigned proj)const
{
	double xyz[3];
	if(getPositionScale({PT_Projection,proj},xyz)) 
	return xyz[0]; return 0; //???	
}
double Model::getBackgroundScale(unsigned index)const
{
	double xyz[3];
	if(getPositionScale({_OT_Background_,index},xyz)) 
	return xyz[0]; return 0; //???	
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

bool Model::indexPoint(unsigned oldIndex, unsigned newIndex)
{
	if(oldIndex<m_points.size()&&newIndex<m_points.size())
	{
		if(newIndex==oldIndex) return true;

		auto p = m_points[oldIndex];

		m_points.erase(m_points.begin()+oldIndex);			
		m_points.insert(m_points.begin()+newIndex,p);
	}
	else return false;
			
	m_changeBits|=AddOther; //2020
			
	Undo<MU_Index>(this,PartPoints,oldIndex,newIndex);

	return true;
}
bool Model::indexJoint(unsigned oldIndex, unsigned newIndex)
{
	if(oldIndex<m_joints.size()&&newIndex<m_joints.size())
	{
		if(newIndex==oldIndex) return true;

		auto p = m_joints[oldIndex];

		m_joints.erase(m_joints.begin()+oldIndex);			
		m_joints.insert(m_joints.begin()+newIndex,p);

		size_t sz = m_joints.size();
		std::vector<unsigned> v; v.reserve(sz);
		for(size_t i=0;i<sz;i++) 
		v.push_back(i);
		v.erase(v.begin()+oldIndex);
		v.insert(v.begin()+newIndex,oldIndex); 
		{
			std::vector<unsigned> w; w.reserve(sz*2);
			for(size_t i=sz;i-->0;)
			{
				auto *ea = m_joints[i];
				if(-1!=ea->m_parent)
				{
					w.push_back(i);
					w.push_back(v[ea->m_parent]);

					//ea->m_parent = -1;
					setBoneJointParent((unsigned)i,-1,false); 
				}
			}
			int j = 0; for(auto&ea:m_joints2)
			{
				ea.first = j++; 
				ea.second = m_joints[ea.first];
			}
			for(size_t i=0;i<w.size();i+=2)
			{
				//ea->m_parent = v[ea->m_parent];
				setBoneJointParent(w[i],w[i+1],false); 
			}					
		}
		for(auto*ea:m_points)
		for(auto&eb:ea->m_influences)
		{
			eb.m_boneId = v[eb.m_boneId];
		}
		for(auto*ea:m_vertices)
		for(auto&eb:ea->m_influences)
		{
			eb.m_boneId = v[eb.m_boneId];
		}
		for(auto*a:m_anims)
		{
			typedef ObjectKeyframeList::value_type w_t; //2023
			std::vector<w_t> w;

			for(auto&ea:a->m_keyframes)
			if(v[ea.first]!=ea.first)
			{
				auto i = v[ea.first];
				for(auto&eb:ea.second)			
				eb->m_objectIndex.index = i;
				w.push_back(w_t(ea.first,std::move(ea.second)));
			}
			for(auto&ea:w) 
			{
				a->m_keyframes.erase(ea.first);
			}
			for(auto&ea:w) 
			{
				auto i = v[ea.first];
				const_cast<Position&>(ea.first).index = i;
				w_t e(ea.first,std::move(ea.second));
				a->m_keyframes.insert(e);
			}
		}

		calculateSkel();
	}
	else return false;

	m_changeBits|=AddOther; //2020
			
	Undo<MU_Index>(this,PartJoints,oldIndex,newIndex);

	return true;
}
bool Model::indexGroup(unsigned oldIndex, unsigned newIndex)
{
	if(oldIndex<m_groups.size()&&newIndex<m_groups.size())
	{
		if(newIndex==oldIndex) return true;

		auto p = m_groups[oldIndex];

		m_groups.erase(m_groups.begin()+oldIndex);			
		m_groups.insert(m_groups.begin()+newIndex,p);
	}
	else return false;
			
	m_changeBits|=AddOther; //2020
			
	Undo<MU_Index>(this,PartGroups,oldIndex,newIndex);

	return true;
}
bool Model::indexMaterial(unsigned oldIndex, unsigned newIndex)
{
	if(oldIndex<m_materials.size()&&newIndex<m_materials.size())
	{
		if(newIndex==oldIndex) return true;

		auto p = m_materials[oldIndex];

		m_materials.erase(m_materials.begin()+oldIndex);			
		m_materials.insert(m_materials.begin()+newIndex,p);

		std::vector<size_t> v(m_materials.size());
		for(size_t i=v.size();i-->0;) v[i] = i;
		v.erase(v.begin()+oldIndex);
		v.insert(v.begin()+newIndex,oldIndex);

		for(auto*ea:m_groups)
		{
			ea->m_materialIndex = v[ea->m_materialIndex];
		}
	}
	else return false;
			
	m_changeBits|=AddOther; //2020
			
	Undo<MU_Index>(this,PartTextures,oldIndex,newIndex);

	return true;
}
bool Model::indexProjection(unsigned oldIndex, unsigned newIndex)
{
	if(oldIndex<m_points.size()&&newIndex<m_points.size())
	{
		if(newIndex==oldIndex) return true;

		auto p = m_projections[oldIndex];

		m_projections.erase(m_projections.begin()+oldIndex);			
		m_projections.insert(m_projections.begin()+newIndex,p);
	}
	else return false;
			
	m_changeBits|=AddOther; //2020
			
	Undo<MU_Index>(this,PartProjections,oldIndex,newIndex);

	return true;
}
