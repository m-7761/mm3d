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
#include "texture.h"

static void model_draw_defaultMaterial()
{
	//2022: Well this makes no sense?
	//https://github.com/zturtleman/mm3d/issues/173
	//float fval[4] = { 0.2f,0.2f,0.2f,1.0f };	
	float fval[4] = { 1,1,1,1 };
	glMaterialfv(GL_FRONT,GL_AMBIENT,fval);
	//fval[0] = fval[1] = fval[2] = 0.8f;
	glMaterialfv(GL_FRONT,GL_DIFFUSE,fval);
	fval[0] = fval[1] = fval[2] = 0.0f;
	glMaterialfv(GL_FRONT,GL_SPECULAR,fval);
	glMaterialfv(GL_FRONT,GL_EMISSION,fval);
	glMaterialf(GL_FRONT,GL_SHININESS,0.0f);
}
void Model::_drawMaterial(BspTree::Draw &d, int g)
{
	auto &gl = m_groups;

	if(~d.ops&DO_TEXTURE||g<0||gl[g]->m_materialIndex<0)
	{
		model_draw_defaultMaterial();
		glDisable(GL_TEXTURE_2D);
	//	glColor3f(0.9f,0.9f,0.9f); //??? //glColorMaterial?

		return;
	}	

	auto *grp = gl[g];
	
	int mi = grp->m_materialIndex;
							
	auto *mp = m_materials[mi];

	if(d.ops&DO_ALPHA)
	{
		//2021: these modes are supported by Assimp and needed by games
		int dst = mp->m_accumulate?GL_ONE:GL_ONE_MINUS_SRC_ALPHA;
		glBlendFunc(GL_SRC_ALPHA,dst);
	}

	glMaterialfv(GL_FRONT,GL_AMBIENT,mp->m_ambient);
	glMaterialfv(GL_FRONT,GL_DIFFUSE,mp->m_diffuse);
	glMaterialfv(GL_FRONT,GL_SPECULAR,mp->m_specular);
	glMaterialfv(GL_FRONT,GL_EMISSION,mp->m_emissive);
	glMaterialf(GL_FRONT,GL_SHININESS,mp->m_shininess);

	if(mp->m_type==Model::MATTYPE_TEXTURE
	&&(!mp->m_textureData->m_isBad||d.ops&DO_BADTEX))
	{
		if(d.context)
		glBindTexture(GL_TEXTURE_2D,d.context->m_matTextures[mi]);
		else
		glBindTexture(GL_TEXTURE_2D,mp->m_texture);

		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,mp->m_sClamp?GL_CLAMP:GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,mp->m_tClamp?GL_CLAMP:GL_REPEAT);

		if(d.ops&DO_TEXTURE_MATRIX&&m_animationMode) 
		{
			if(d.texture_matrix!=g)
			{
				bool tm = false;
				for(auto*up:grp->m_utils) 
				if(up->type==UT_UvAnimation)
				{
					auto *uv = (UvAnimation*)up;
					uv->_make_cur();
						
					if(!tm)
					{
						tm = true;

						d.texture_matrix = g;

						glMatrixMode(GL_TEXTURE);
						glLoadMatrixd(uv->_cur_texture_matrix.getMatrix());
					}
					else glMultMatrixd(uv->_cur_texture_matrix.getMatrix());
				}
				if(!tm&&d.texture_matrix!=-1)
				{
					d.texture_matrix = -1;

					glLoadIdentity(); 
					glMatrixMode(GL_MODELVIEW);
				}
			}
		}

		glEnable(GL_TEXTURE_2D);
	}
	else glDisable(GL_TEXTURE_2D);
}

static void model_draw_drawPointOrientation(bool selected, double scale, const Matrix &m)
{
	//Selected green is completely invisible on the teal background :(
	//float color = selected?0.9f:0.7f;
	float color = selected?1.0f:0.7f;

	Vector v1,v2;

	glBegin(GL_LINES);

	glColor3f(color,0.0f,0.0f);

	v1.setAll(0.0,0.0,0.0);
	v2.setAll(scale,0.0,scale);

	v1.transform(m);
	v2.transform(m);

	glVertex3dv(v1.getVector());
	glVertex3dv(v2.getVector());

	v1.setAll(scale,0.0,scale);
	v2.setAll(0.0,0.0,scale *2);

	v1.transform(m);
	v2.transform(m);

	glVertex3dv(v1.getVector());
	glVertex3dv(v2.getVector());
	 	 
	glColor3f(0.0f,color,0.0f);

	v1.setAll(0.0,0.0,0.0);
	v2.setAll(0.0,scale,scale);

	v1.transform(m);
	v2.transform(m);

	glVertex3dv(v1.getVector());
	glVertex3dv(v2.getVector());

	v1.transform(m);
	v2.transform(m);

	v1.setAll(0.0,scale,scale);
	v2.setAll(0.0,0.0,scale *2);

	v1.transform(m);
	v2.transform(m);

	glVertex3dv(v1.getVector());
	glVertex3dv(v2.getVector());

	glColor3f(0.0f,0.0f,color);

	v1.setAll(0.0,0.0,0.0);
	v2.setAll(0.0,0.0,scale *3);

	v1.transform(m);
	v2.transform(m);

	glVertex3dv(v1.getVector());
	glVertex3dv(v2.getVector());

	glEnd(); // GL_LINES
}

static const int CYL_VERT_COUNT = 8;
static const int CYL_SEAM_VERT  = 6; //2; //2020 sense

static double model_draw_cylinderVertices[CYL_VERT_COUNT][3] =
{
	{  0.30, 1.00, 0.00 },
	{  0.21, 1.00, 0.21 },
	{  0.00, 1.00, 0.30 },
	{ -0.21, 1.00, 0.21 },
	{ -0.30, 1.00, 0.00 },
	{ -0.21, 1.00,-0.21 },
	{  0.00, 1.00,-0.30 },
	{  0.21, 1.00,-0.21 },
};

static void model_draw_drawProjectionCylinder(const Matrix &m)
{	 
	 double topVerts[CYL_VERT_COUNT][3];
	 double botVerts[CYL_VERT_COUNT][3];

	 for(int v = 0; v<CYL_VERT_COUNT; v++)
	 {
		 topVerts[v][0] =  model_draw_cylinderVertices[v][0];
		 topVerts[v][1] =  model_draw_cylinderVertices[v][1];
		 topVerts[v][2] =  model_draw_cylinderVertices[v][2];

		 botVerts[v][0] =  model_draw_cylinderVertices[v][0];
		 botVerts[v][1] = -model_draw_cylinderVertices[v][1];
		 botVerts[v][2] =  model_draw_cylinderVertices[v][2];

		 m.apply3x(topVerts[v]);
		 m.apply3x(botVerts[v]);
	 }
	 	 
	 glBegin(GL_LINES);

	 for(int v = 0; v<CYL_VERT_COUNT; v++)
	 {
		 int v2 = (v+1)%CYL_VERT_COUNT;

		 glVertex3dv(topVerts[v]);
		 glVertex3dv(topVerts[v2]);

		 glVertex3dv(botVerts[v]);
		 glVertex3dv(botVerts[v2]);

		 if(v==CYL_SEAM_VERT)
		 {
			 glEnd();
			 glLineWidth(3);
			 glBegin(GL_LINES);
		 }
		 glVertex3dv(topVerts[v]);
		 glVertex3dv(botVerts[v]);
		 if(v==CYL_SEAM_VERT)
		 {
			 glEnd();
			 glLineWidth(1);
			 glBegin(GL_LINES);
		 }
	 }

	 glEnd(); // GL_LINES	 
	 glLineWidth(1);

	 glBegin(GL_POINTS);
	 glVertex3dv(topVerts[CYL_SEAM_VERT]);
	 glVertex3dv(botVerts[CYL_SEAM_VERT]);
	 glEnd();
}

static const int SPH_VERT_COUNT = 8;
static const int SPH_SEAM_VERT_TOP = 0; //2020 sense.
static const int SPH_SEAM_VERT_BOT = 4;

static double model_draw_sphereXVertices[SPH_VERT_COUNT][3] =
{
	{  0.00, 1.00, 0.00 },
	{  0.00, 0.71, 0.71 },
	{  0.00, 0.00, 1.00 },
	{  0.00,-0.71, 0.71 },
	{  0.00,-1.00, 0.00 },
	{  0.00,-0.71,-0.71 },
	{  0.00, 0.00,-1.00 },
	{  0.00, 0.71,-0.71 },
};

static double model_draw_sphereYVertices[SPH_VERT_COUNT][3] =
{
	{  1.00, 0.00, 0.00 },
	{  0.71, 0.00, 0.71 },
	{  0.00, 0.00, 1.00 },
	{ -0.71, 0.00, 0.71 },
	{ -1.00, 0.00, 0.00 },
	{ -0.71, 0.00,-0.71 },
	{  0.00, 0.00,-1.00 },
	{  0.71, 0.00,-0.71 },
};

static double model_draw_sphereZVertices[SPH_VERT_COUNT][3] =
{
	{  1.00, 0.00, 0.00 },
	{  0.71, 0.71, 0.00 },
	{  0.00, 1.00, 0.00 },
	{ -0.71, 0.71, 0.00 },
	{ -1.00, 0.00, 0.00 },
	{ -0.71,-0.71, 0.00 },
	{  0.00,-1.00, 0.00 },
	{  0.71,-0.71, 0.00 },
};

static void model_draw_drawProjectionSphere(const Matrix &m)
{	 
	 double xVerts[SPH_VERT_COUNT][3];
	 double yVerts[SPH_VERT_COUNT][3];
	 double zVerts[SPH_VERT_COUNT][3];

	 for(int v = 0; v<SPH_VERT_COUNT; v++)
	 {
		 xVerts[v][0] =  model_draw_sphereXVertices[v][0];
		 xVerts[v][1] =  model_draw_sphereXVertices[v][1];
		 xVerts[v][2] =  model_draw_sphereXVertices[v][2];

		 yVerts[v][0] =  model_draw_sphereYVertices[v][0];
		 yVerts[v][1] =  model_draw_sphereYVertices[v][1];
		 yVerts[v][2] =  model_draw_sphereYVertices[v][2];

		 zVerts[v][0] =  model_draw_sphereZVertices[v][0];
		 zVerts[v][1] =  model_draw_sphereZVertices[v][1];
		 zVerts[v][2] =  model_draw_sphereZVertices[v][2];

		 m.apply3x(xVerts[v]);
		 m.apply3x(yVerts[v]);
		 m.apply3x(zVerts[v]);
	 }
		
	 glBegin(GL_LINES);

	 bool thick = false;

	 for(int v = 0; v<SPH_VERT_COUNT; v++)
	 {
		 int v2 = (v+1)%SPH_VERT_COUNT;

		 //2020 sense
		 //if(v>=SPH_SEAM_VERT_TOP&&v<SPH_SEAM_VERT_BOT)
		 if(!(v>=SPH_SEAM_VERT_TOP&&v<SPH_SEAM_VERT_BOT))
		 {
			 glEnd();
			 glLineWidth(3);
			 glBegin(GL_LINES);
			 thick = true;
		 }
		 glVertex3dv(xVerts[v]);
		 glVertex3dv(xVerts[v2]);
		 if(thick)
		 {
			 glEnd();
			 glLineWidth(1);
			 glBegin(GL_LINES);
			 thick = false;
		 }

		 glVertex3dv(yVerts[v]);
		 glVertex3dv(yVerts[v2]);

		 glVertex3dv(zVerts[v]);
		 glVertex3dv(zVerts[v2]);
	 }

	 glEnd(); // GL_LINES	 

	 glBegin(GL_POINTS);
	 glVertex3dv(xVerts[SPH_SEAM_VERT_TOP]);
	 glVertex3dv(xVerts[SPH_SEAM_VERT_BOT]);
	 glEnd();
}

static const int PLN_VERT_COUNT = 4;

static double model_draw_planeVertices[PLN_VERT_COUNT][3] =
{
	{ -1.00,-1.00, 0.00 },
	{  1.00,-1.00, 0.00 },
	{  1.00, 1.00, 0.00 },
	{ -1.00, 1.00, 0.00 },
};

static void model_draw_drawProjectionPlane(const Matrix &m)
{	 
	 double verts[PLN_VERT_COUNT][3];
   
	 for(int v = 0; v<PLN_VERT_COUNT; v++)
	 {
		 verts[v][0] =  model_draw_planeVertices[v][0];
		 verts[v][1] =  model_draw_planeVertices[v][1];
		 verts[v][2] =  model_draw_planeVertices[v][2];

		 m.apply3x(verts[v]);
	 }

	 glLineWidth(3);
	 glBegin(GL_LINES);
	 for(int v = 0; v<PLN_VERT_COUNT; v++)
	 {
		 int v2 = (v+1)%PLN_VERT_COUNT;

		 glVertex3dv(verts[v]);
		 glVertex3dv(verts[v2]);
	 }
	 glEnd();
	 glLineWidth(1);
}

void Model::draw(unsigned drawOptions, ContextT context, double viewPoint[3])
{
	//https://github.com/zturtleman/mm3d/issues/56
	drawOptions|=m_drawOptions;

	if(0!=(drawOptions&DO_WIREFRAME))
	{
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	}
	else
	{
#ifdef MM3D_EDIT //???
		if(0!=(drawOptions&DO_BACKFACECULL))
		{
			glEnable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
		}
		else
		{
			glDisable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
		}
#else
		glPolygonMode(GL_FRONT,GL_FILL);
#endif
	}

	validateNormals();

	//FIX ME
	//If not using ContextT textures aren't loaded
	//automatically, so that m_textureData crashes
	//below.
	DrawingContext *drawContext = nullptr;
	if(context)
	{
		drawContext = getDrawingContext(context);

		if(!drawContext->m_valid)
		{
			loadTextures(context);
			//log_debug("loaded textures for %p\n",context);
		}
	}
	else if(!m_validContext) //2020
	{
		loadTextures();
	}

	//2021: BspTree::render requires viewPoint
	//(can viewPoint be nonzero?)
	//
	// TODO! if models just use additive blending
	// bspTree is overkill (two passes can do it)
	//
	bool alpha = drawOptions&DO_ALPHA&&viewPoint;
	if(alpha&&!m_validBspTree)
	{
		//TODO: this doesn't encode selection mode
		//color at all
		calculateBspTree();
	}

	for(unsigned t=m_triangles.size();t-->0;)
	{
		m_triangles[t]->m_marked = false;
	}

	glDisable(GL_BLEND);
	glEnable(GL_LIGHT0);
	glDisable(GL_LIGHT1);
	//glColor3f(0.9f,0.9f,0.9f); //??? //glColorMaterial?

	BspTree::Draw d = 
	{
		drawOptions&~DO_ALPHA,-1,drawContext,m_drawingLayers 
	};

	auto vl = getVertexList();
	bool infl = (drawOptions&DO_INFLUENCE)!=0;
	auto &infl_vp = m_viewportColors[(drawOptions&DO_INFLUENCE_VIEWPORT)>>12];
	float infl_wt[4] = {1,1,1,1}, infl_wh[4] = {1,1,1,1};
	auto wt = [&](int v)->float*
	{
		float sum = 0;
		auto &il = vl[v]->m_influences;
		for(auto&ea:il)		
		for(int i=4;i-->0;)
		if(ea.m_boneId==infl_vp.joints[i])
		{
			unsigned c = infl_vp.colors[i];
			float w = (float)ea.m_weight;

			if(sum==0)			
			infl_wt[0] = infl_wt[1] = infl_wt[2] = 0;
			sum+=w;
			infl_wt[0]+=(c&0xff)/255.0f*w;
			infl_wt[1]+=(c>>8&0xff)/255.0f*w;
			infl_wt[2]+=(c>>16&0xff)/255.0f*w;
		}
		if(sum==0) return infl_wh;
		else if(sum>1)
		{
			float x = 1-(sum-1)/sum;
			for(int i=3;i-->0;)
			infl_wt[i]*=x;
		}
		else for(int i=3;i-->0;)
		{ 
			infl_wt[i]+=(1-sum);
		}		
		return infl_wt;
	};

	if(drawOptions&(DO_VERTEXCOLOR|DO_INFLUENCE)) //2024
	{
		glEnable(GL_COLOR_MATERIAL);
	}

	//https://github.com/zturtleman/mm3d/issues/98
	//bool colorSelected = false;
	int colorSelected;	
	for(unsigned g=0;g<m_groups.size();g++)
	{
		Group *grp = m_groups[g];

		int mi = grp->m_materialIndex;

		if(alpha&&mi>=0&&m_materials[mi]->needsAlpha())
		{
			// Alpha blended groups are drawn by bspTree later
			for(unsigned i:grp->m_triangleIndices)
			{
				m_triangles[i]->m_marked = true;
			}
			continue;
		}
		else _drawMaterial(d,g);

		//colorSelected = false;
		colorSelected = -1;

		glBegin(GL_TRIANGLES);
		for(int i:grp->m_triangleIndices)
		{
			auto tp = m_triangles[i]; 
			
			tp->m_marked = true;

			if(!tp->visible(d.layers)) continue;
			
			if(tp->m_selected)
			{
				//if(colorSelected==false)
				if(colorSelected!=(int)true)
				{
					/*if(0==(drawOptions&DO_TEXTURE))
					{
						glColor3f(1,0,0); //??? //glColorMaterial?
					}*/
					glEnd();
					glDisable(GL_LIGHT0);
					glEnable(GL_LIGHT1);
					glBegin(GL_TRIANGLES);
					colorSelected = true;
				}
			}
			else
			{
				//if(colorSelected==true)
				if(colorSelected!=(int)false)
				{
					/*if(0==(drawOptions&DO_TEXTURE))
					{
						glColor3f(0.9f,0.9f,0.9f); //??? //glColorMaterial?
					}*/
					glEnd();
					glDisable(GL_LIGHT1);
					glEnable(GL_LIGHT0);
					glBegin(GL_TRIANGLES);
					colorSelected = false;
				}						
			}

			for(int v=0;v<3;v++)
			{
				if(drawOptions&(DO_VERTEXCOLOR|DO_INFLUENCE))
				{
					float *f = infl?wt(tp->m_vertexIndices[v]):tp->m_colors[v];
					glColor4fv(f);
				}
				glTexCoord2f(tp->m_s[v],tp->m_t[v]);
				if(drawOptions&DO_SMOOTHING)
				glNormal3dv(tp->m_normalSource[v]);
				else glNormal3dv(tp->m_flatSource);
				glVertex3dv(vl[tp->m_vertexIndices[v]]->m_absSource);
			}
		}
		glEnd();

		if(colorSelected==(int)true) glDisable(GL_LIGHT1); //2020
	}

	if(d.texture_matrix!=-1)
	{	
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
	}

	glDisable(GL_TEXTURE_2D);

	// Draw ungrouped triangles
	{
		colorSelected = -1; //NEW

		model_draw_defaultMaterial();

		glBegin(GL_TRIANGLES);
		for(auto*tp:m_triangles) if(!tp->m_marked)
		{
			tp->m_marked = true;

			if(!tp->visible(d.layers)) continue;
			
			if(tp->m_selected)
			{
				//if(colorSelected==false)
				if(colorSelected!=(int)true)
				{
				//	glColor3f(1,0,0); //??? //glColorMaterial?
					glEnd();
					glDisable(GL_LIGHT0);
					glEnable(GL_LIGHT1);
					glBegin(GL_TRIANGLES);
					colorSelected = true;
				}
			}
			else
			{
				//if(colorSelected==true)
				if(colorSelected!=(int)false)
				{
				//	glColor3f(0.9f,0.9f,0.9f); //??? //glColorMaterial?
					glEnd();
					glDisable(GL_LIGHT1);
					glEnable(GL_LIGHT0);
					glBegin(GL_TRIANGLES);
					colorSelected = false;
				}							
			}

			for(int v=0;v<3;v++)
			{
				if(drawOptions&(DO_VERTEXCOLOR|DO_INFLUENCE))
				{
					float *f = infl?wt(tp->m_vertexIndices[v]):tp->m_colors[v];
					glColor4fv(f);
				}
				glTexCoord2f(tp->m_s[v],tp->m_t[v]);
				if(drawOptions&DO_SMOOTHING)
				glNormal3dv(tp->m_normalSource[v]);
				else glNormal3dv(tp->m_flatSource);
				glVertex3dv(vl[tp->m_vertexIndices[v]]->m_absSource);
			}
		}
		glEnd();

		if(colorSelected==(int)true) glDisable(GL_LIGHT1); //2020
	}

	if(alpha&&~drawOptions&DO_ALPHA_DEFER_BSP)
	{
		// Draw depth-sorted alpha blended polys last
		//draw_bspTree(drawOptions,context,viewPoint);
		if(!m_bspTree.empty())
		{
			glDepthMask(0);
			glEnable(GL_BLEND);		
			m_bspTree.render(viewPoint,d); //drawContext		
			glDisable(GL_BLEND);
			glDepthMask(1);

			glDisable(GL_TEXTURE_2D);
		}
	}

	if(drawOptions&(DO_VERTEXCOLOR|DO_INFLUENCE)) //2024
	{
		glDisable(GL_COLOR_MATERIAL);
	}

	if(0!=(drawOptions&DO_WIREFRAME))
	{
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	}
}
void Model::draw_bspTree(unsigned drawOptions, ContextT context, double viewPoint[3])
{
	if(m_bspTree.empty()) return;

	//https://github.com/zturtleman/mm3d/issues/56
	drawOptions|=m_drawOptions;

	//TESTING SOMETHING
	int wireframe = drawOptions&DO_WIREFRAME;
	if(0!=(drawOptions&DO_WIREFRAME))
	{
		drawOptions&=~DO_TEXTURE;

		glColor3f(1,1,1); //a

		//NOTE: This will draw cuts in the BSP tree
		//which may not be desired?
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
		glEnable(GL_LINE_STIPPLE);
		glDisable(GL_TEXTURE_2D);
	}
	else
	{
#ifdef MM3D_EDIT //???
		if(0!=(drawOptions&DO_BACKFACECULL))
		{
			glEnable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
		}
		else
		{
			glDisable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
		}
#else
		glPolygonMode(GL_FRONT,GL_FILL);
#endif
	}

	DrawingContext *drawContext = nullptr;
	if(context)
	{
		drawContext = getDrawingContext(context);

		/*Model::draw should've done this already.
		if(!drawContext->m_valid)
		{
			loadTextures(context);
			//log_debug("loaded textures for %p\n",context);
		}*/
	}

	BspTree::Draw d = //2022
	{
		drawOptions,-1,drawContext,m_drawingLayers,this
	};

	//if(!m_bspTree.empty())
	{
		glDepthMask(0);
		glEnable(GL_BLEND);		
		m_bspTree.render(viewPoint,d); //drawContext
		glDisable(GL_BLEND);
		glDepthMask(1);

		glDisable(GL_TEXTURE_2D);
	}	

	if(d.texture_matrix!=-1)
	{	
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW_MATRIX);
	}

	if(wireframe)
	{
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
		glDisable(GL_LINE_STIPPLE);
	}
}

void Model::drawLines(float a)
{
	//TESTING
	//CAUTION: I don't know what the blend mode is
	//at this point?
	//https://github.com/zturtleman/mm3d/issues/95
	if(1!=a) glEnable(GL_BLEND);

	//The goal of this is to not double-blend over
	//lines. The reality is depth-buffer tests are
	//inaccurate, so results aren't very different.
	glDepthFunc(GL_LESS);

	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);

	//The 2-pass system prevents touching 
	//triangles from double-drawing their
	//alpha blended edges. It's a problem
	//if lone polygons are too dim to see.
	
		//REMINDER: I TRIED DIFFERENT WAYS OF 
		//DRAWING BACK-FACES DIFFERENTLY. AND
		//FELT THE EXPERIMENTS WERE NO BETTER.

		glColor4f(1,1,1,a); //white
		if(a) //Hide?
		_drawPolygons(0);

	glDisable(GL_BLEND);

	//CAUTION: These need a way to come out on top.
	//glPolygonOffset is unreliable, so the only
	//other technique is to do everything in 2D.
	//glDepthFunc(GL_LEQUAL);
	//
	//THIS DISABLES glDepthMask
	//glDisable(GL_DEPTH_TEST);
	//
	// NOTE: this can potentially put wrong values
	// into the depth buffer... the only alternative
	// is to draw all of the lines normally so the
	// depth is correct and then draw the selected
	// lines over them :(
	//
	glDepthFunc(GL_ALWAYS);

		//REMINDER: I TRIED DIFFERENT WAYS OF 
		//DRAWING BACK-FACES DIFFERENTLY. AND
		//FELT THE EXPERIMENTS WERE NO BETTER.

		glColor4f(1,0,0,1); //red
		_drawPolygons(1);

	//glLineWidth(1.0);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	//glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
}
void Model::drawVertices(float a)
{
	unsigned lv = m_drawingLayers; //2022

	//HACK: This should match the below
	//behavior. It's easier to break it
	//out.
	glPointSize(5);
	//glDepthFunc(GL_LEQUAL);
	//THIS DISABLES glDepthMask
	//glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_ALWAYS);
	glColor3ub(255,0,0);
	glBegin(GL_POINTS);
	for(auto*vp:m_vertices) if(vp->m_selected)
	{
		if(vp->visible(lv)) draw:
		{
			glVertex3dv(vp->m_absSource);
		}
		else for(auto&ea:vp->m_faces)
		{
			if(ea.first->visible(lv)) goto draw;
		}
	}
	glEnd();
	
	if(0==a) //return;
	{
		glDepthFunc(GL_LEQUAL);
		//glEnable(GL_DEPTH_TEST);
	
		return;
	}

	glDepthFunc(GL_LESS);

	if(1!=a) glEnable(GL_BLEND);

	glPointSize(3); 

	glColor4f(1,1,1,a); //white

	int cull = m_drawOptions&DO_BACKFACECULL;
	//YUCK: Degenerate triangles (in screen space)
	//don't draw vertices.
	if(cull)
	{
		//2019: Don't draw back-faces.
		//https://github.com/zturtleman/mm3d/issues/96
		glPolygonMode(GL_FRONT_AND_BACK,GL_POINT); 

			_drawPolygons(0/*,true*/);
	}

	//Draw "free" vertices, and vertices that
	//don't match their triangle's selections.
	//Or, typically all the selected vertices.
	glBegin(GL_POINTS);
	for(auto*vp:m_vertices) 
	if(!vp->m_selected&&vp->visible(lv))
	{
		/*2022 Can leverage m_faces for this.
		if(vp->m_marked||vp->m_marked2) continue;*/
		if(cull) for(auto&ea:vp->m_faces)
		{
			//NOTE: This is in case a vertex is
			//left behind in a layer without its
			//triangles. This scenario used to be
			//possible. It may be unlikely now.
			if(ea.first->visible(lv)) goto no_draw;
		}
		glVertex3dv(vp->m_absSource); no_draw:;
	}
	glEnd();

	glDepthFunc(GL_LEQUAL);
	//glEnable(GL_DEPTH_TEST);
		
	if(1!=a) glDisable(GL_BLEND);

	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); 
}
void Model::_drawPolygons(int pass/*,bool mark*/)
{	
	unsigned lv = m_drawingLayers; //2022

	validateAnim();

	int cull = m_drawOptions&DO_BACKFACECULL;
	
	//YUCK: Degenerate triangles (in screen space)
	//don't draw vertices.
	//if(!cull&&mark) return; //FIX ME

	glDisable(GL_TEXTURE_2D);

	//https://github.com/zturtleman/mm3d/issues/96
	(!pass&&cull?glEnable:glDisable)(GL_CULL_FACE);

	glBegin(GL_TRIANGLES);
	Vertex **v = m_vertices.data();
	for(size_t t=m_triangles.size();t-->0;)
	{
		Triangle *triangle = m_triangles[t];
				
		//triangle->m_marked = true; //???
		
		auto *vi = triangle->m_vertexIndices;
		
		if(triangle->m_selected)
		{
			if(pass==0) //continue;
			{
				/*2022: Can leverage m_faces for this.
				if(mark) for(int i=0;i<3;i++)
				{							
					v[vi[i]]->m_marked2 = true;
				}*/
				continue;
			}
		}
		else
		{
			//if(pass==1) continue;
			if(pass==0) 
			{
				/*2022: Can leverage m_faces for this.
				if(mark) for(int i=0;i<3;i++)
				{
					if(!v[vi[i]]->m_selected)
					v[vi[i]]->m_marked = true;
				}*/
			}
			else continue;
		}

		if(triangle->visible(lv))
		{			
			double *vertices[3];

			for(int i=0;i<3;i++)
			{		
				vertices[i] = v[vi[i]]->m_absSource;
			}

			glVertex3dv(vertices[0]); //glVertex3dv(vertices[1]);
			glVertex3dv(vertices[1]); //glVertex3dv(vertices[2]);
			glVertex3dv(vertices[2]); //glVertex3dv(vertices[0]);
		}		
	}
	glEnd();
}

#ifdef MM3D_EDIT

void Model::drawJoints(float a, float axis)
{
	bool draw = m_drawJoints;
	if(!draw&&!getSelectedBoneJointCount()) 
	return;

	unsigned lv = m_drawingLayers; //2022
	
	//NOTE: Both up vectors seem to work.
	//Using Y up is easier since the bone
	//shape identity matrix defaults to it.
	//const Vector face(0,1,0),up(0,0,1);
	const Vector up(0,1,0);
	
	validateAnimSkel();

	bool alpha = a!=1;
			
	//EXPERIMENTAL
	//NOTE: If this DepthRange technique isn't used
	//then the GL_POINTS primitives will need to be
	//drawn after GL_LINES as opposed to vice versa.
	//Make room for Tool::draw?
	//if(alpha) glDepthRange(0,0);
	if(alpha) glDepthRange(0.00002f,0.00002f);
	if(alpha) glDepthFunc(GL_LESS);	
	
	//Black seems to be best. Blue/purple is legacy
	//color.
	//https://github.com/zturtleman/mm3d/issues/118
	float l = (float)(alpha?0:1);

	//TODO: falsify in coloring mode
	bool skel = inJointAnimMode();
	bool skel2 = false;

	glPointSize(5); //3
	glBegin(GL_POINTS); 
	int marks = 0, sel = 0;
	for(Position j{PT_Joint,0};j<m_joints.size();j++)
	{
		auto *ea = m_joints[j];

		if(ea->m_selected)
		{
			skel2 = true;
			if(!sel++) glColor3f(1,0,1);
			glVertex3dv(ea->m_absSource);
			ea->m_marked = false;
		}
		else if(draw)
		{
			ea->m_marked = skel&&ea->visible(lv)
			&&hasKeyframe(m_currentAnim,m_currentFrame,j);
			marks+=ea->m_marked;
		}		
	}	
	if(draw)
	{
		if(marks)
		{
			glColor3f(0,1,0);
			for(auto*ea:m_joints)
			{
				if(ea->m_marked)
				glVertex3dv(ea->m_absSource);
			}
		}
		if(sel+marks!=(int)m_joints.size())
		{
			if(skel) glColor3f(0,0,l); //legacy
						
			for(auto*ea:m_joints)
			if(!ea->m_marked&&ea->visible(lv))
			{
				if(!skel)
				glColor4fv(ea->m_color);
				glVertex3dv(ea->m_absSource);
			}	
		}
	}
	glEnd();
	if(axis)
	{
		//TODO? Could decorate naked points
		//based on axis scale?

		if(sel) for(auto*ea:m_joints)
		{
			if(!ea->m_selected) continue;

			Matrix m = ea->getMatrix();

			double *pos = m.getVector(3); //m.scale(axis)
			{
				for(int i=0;i<3;i++) 
				for(int j=0;j<3;j++) 
				{
					(m.getVector(i)[j]*=axis)+=pos[j];
				}
			}

			//drawOrigin();
			{
				glBegin(GL_LINES);
				glColor3f(1,0,0);
				glVertex3dv(pos); glVertex3dv(m.getVector(0));
				glColor3f(0,1,0);
				glVertex3dv(pos); glVertex3dv(m.getVector(1));
				glColor3f(0,0,1);
				glVertex3dv(pos); glVertex3dv(m.getVector(2));
				glEnd();
			}
		}
	}
	
	if(alpha) glEnable(GL_BLEND);
	float aa = skel2&&skel?a/2:a;	

	glLineStipple(1,0xCCCC); //2020	
	//Needs to go in reverse to prioritize
	//the child joints in the depth buffer.
	//for(unsigned j=0 j<m_joints.size();j++)
	//for(unsigned j=m_joints.size();j-->0;)
	for(unsigned j2=m_joints2.size();j2-->0;)
	if(m_joints2[j2].second->m_parent>=0)
	{
		unsigned j = m_joints2[j2].first;

		Joint *joint  = m_joints[j];
		Joint *parent = m_joints[joint->m_parent];
		if(!joint->visible(lv)
		||!parent->visible(lv))
		continue;
		
		skel2 = skel&&parentJointSelected(j);
		
		if(!draw&&!skel2)
		if(!joint->m_selected
		||!parent->m_selected) continue;

		Vector pvec(parent->m_absSource);
		Vector jvec(joint->m_absSource); 

		if(skel2)
		{	
			glColor4f(l,0,l,a);
		}
		else glColor4f(0,0,l,aa);

		glEnable(GL_LINE_STIPPLE);
		glBegin(GL_LINES);
		glVertex3dv(pvec.getVector());
		glVertex3dv(jvec.getVector());
		glEnd();
		glDisable(GL_LINE_STIPPLE);

		//EXPERIMENTAL
		//This almost works except when the child
		//joint is translated. It's accurate even
		//though it looks odd.
		bool stable = skel&&true;

		//https://github.com/zturtleman/mm3d/issues/56
		//if(m_drawJoints==JOINTMODE_BONES)
		if(m_drawOptions&DO_BONES&&joint->m_bone)
		{				
			if(stable) pvec.setAll(parent->m_abs);
			if(stable) jvec.setAll(joint->m_abs);

			if(skel) if(skel2)
			{
				glColor4f(l,0,l,a);
			}
			else glColor4f(0,0,l,aa);
		
			//Vector face(0,1,0);
			Vector point = jvec-pvec;
			double length = distance(pvec,jvec);

			/*FIX ME
			//This rolls around in the Top view.
			//https://github.com/zturtleman/mm3d/issues/118
			Quaternion rot;
			rot.setRotationToPoint(face,point);	
			//Matrix m;
			//m.setRotationQuaternion(rot);
			*/
			//2020: I wasted some time trying to pass "up" to setRotationToPoint
			//but it wasn't working out as I expected. I got it to work with a 
			//different approach but knew all along that a Matrix is a better
			//fit here. setRotationToPoint should be avoided/removed. 
			Matrix rot;
			if(point[0]||point[2]) //Clashes with up?
			{
				memcpy(rot.getVector(1),&point,sizeof(point));
				normalize3(rot.getVector(1));
				cross_product(rot.getVector(2),up.getVector(),rot.getVector(1));		
				normalize3(rot.getVector(2));
				cross_product(rot.getVector(0),rot.getVector(1),rot.getVector(2));
				normalize3(rot.getVector(0));
			}
			else if(point[1]<0)
			{
				rot.getVector(1)[1] = -1;
			}
			Vector v[4] = 
			{{ 0.07, 0.10, 0.07},
			{ -0.07, 0.10, 0.07},
			{ -0.07, 0.10,-0.07},
			{  0.07, 0.10,-0.07}};
			for(int i=0;i<4;i++)
			{
				//v[i] = v[i]*length*m+pvec;
				v[i] = v[i]*length*rot+pvec;
			
				//2021: Stabilize the octohedron WRT bind pose.
				if(stable) v[i].transform(parent->getSkinMatrix());
			}
			if(stable) pvec.transform(parent->getSkinMatrix());
			if(stable) jvec.transform(parent->getSkinMatrix());

			glBegin(GL_LINES);
			glVertex3dv(pvec.getVector()); glVertex3dv(v[0].getVector());
			glVertex3dv(pvec.getVector()); glVertex3dv(v[1].getVector());
			glVertex3dv(pvec.getVector()); glVertex3dv(v[2].getVector());
			glVertex3dv(pvec.getVector()); glVertex3dv(v[3].getVector());
			glVertex3dv(v[0].getVector()); glVertex3dv(v[1].getVector());
			glVertex3dv(v[1].getVector()); glVertex3dv(v[2].getVector());
			glVertex3dv(v[2].getVector()); glVertex3dv(v[3].getVector());
			glVertex3dv(v[3].getVector()); glVertex3dv(v[0].getVector());
			glVertex3dv(jvec.getVector()); glVertex3dv(v[0].getVector());
			glVertex3dv(jvec.getVector()); glVertex3dv(v[1].getVector());
			glVertex3dv(jvec.getVector()); glVertex3dv(v[2].getVector());
			glVertex3dv(jvec.getVector()); glVertex3dv(v[3].getVector());
			glEnd();
		}
	}

	//EXPERIMENTAL
	glDisable(GL_BLEND); 	
	glDepthRange(0,1); //glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);	
}

void Model::drawPoints(float axis)
{
	validateAnim();

	unsigned lv = m_drawingLayers; //2022

	//float scale = 2; //???
	double scale = m_viewportUnits.ptsz3d;
	if(!scale) scale = 0.25;
	scale*=m_viewportUnits.inc3d;
	if(!scale) scale = 2;
	
	for(auto*ea:m_points) if(ea->visible(lv))
	{
		model_draw_drawPointOrientation(ea->m_selected,scale,ea->getMatrix());

		float ptsz = 5;
	
		if(ea->m_selected)
		{
			glColor3f(0.7f,1,0);
		}
		else
		{
			//glColor3f(0,0.5f,0);
			glColor4fv(ea->m_color);

			//Use old size if default colored? 
			if(!ea->m_color[0]&&ea->m_color[1]==0.5f&&!ea->m_color[2])
			{
				ptsz = 3;
			}
		}

		glPointSize(ptsz);

		glBegin(GL_POINTS);
		glVertex3dv(ea->m_absSource);
		glEnd();
	}	

	if(axis) for(auto*ea:m_points)
	{
		if(!ea->m_selected) continue;

		Matrix m = ea->getMatrix();

		double *pos = m.getVector(3); //m.scale(axis)
		{
			for(int i=0;i<3;i++) 
			for(int j=0;j<3;j++) 
			{
				(m.getVector(i)[j]*=axis)+=pos[j];
			}
		}

		//drawOrigin();
		{
			glBegin(GL_LINES);
			glColor3f(1,0,0);
			glVertex3dv(pos); glVertex3dv(m.getVector(0));
			glColor3f(0,1,0);
			glVertex3dv(pos); glVertex3dv(m.getVector(1));
			glColor3f(0,0,1);
			glVertex3dv(pos); glVertex3dv(m.getVector(2));
			glEnd();
		}
	}
}

void Model::drawProjections()
{
	if(!m_drawProjections) return; //m_animationMode

	glPointSize(3);	
	glLineStipple(1,0xf1f1);
	glEnable(GL_LINE_STIPPLE);
	for(unsigned p=0;p<m_projections.size();p++)
	{
		TextureProjection *proj = m_projections[p];

		Matrix m = proj->getMatrixUnanimated();

		glColor3f(0,proj->m_selected?0.75f:0.55f,0);

		switch (proj->m_type)
		{
		case Model::TPT_Sphere:
			model_draw_drawProjectionSphere(m);
			break;
		case Model::TPT_Cylinder:
			model_draw_drawProjectionCylinder(m);
			break;		
		case Model::TPT_Plane: 
			//default:
			model_draw_drawProjectionPlane(m);
			break;
		default: continue; //2020
		}

		if(proj->m_selected)
		glColor3f(0.7f,1,0);
		else
		glColor3f(0,0.5f,0);

		glBegin(GL_POINTS);
		glVertex3dv(proj->m_abs);
		glEnd();
	}
	glDisable(GL_LINE_STIPPLE);
}

#endif // MM3D_EDIT


