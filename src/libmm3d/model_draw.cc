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
			log_debug("loaded textures for %p\n",context);
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

	//https://github.com/zturtleman/mm3d/issues/98
	//bool colorSelected = false;
	int colorSelected;
	for(unsigned m=0;m<m_groups.size();m++)
	{
		Group *grp = m_groups[m];

		if(drawOptions&DO_TEXTURE)
		{
			//glColor3f(1,1,1); //??? //glColorMaterial?

			if(grp->m_materialIndex>=0)
			{
				int index = grp->m_materialIndex;

				if(alpha&&m_materials[index]->needsAlpha())
				{
					// Alpha blended groups are drawn by bspTree later
					for(unsigned triIndex:grp->m_triangleIndices)
					{
						Triangle *triangle = m_triangles[triIndex];
						triangle->m_marked = true;
					}
					continue;
				}

				glMaterialfv(GL_FRONT,GL_AMBIENT,m_materials[index]->m_ambient);
				glMaterialfv(GL_FRONT,GL_DIFFUSE,m_materials[index]->m_diffuse);
				glMaterialfv(GL_FRONT,GL_SPECULAR,m_materials[index]->m_specular);
				glMaterialfv(GL_FRONT,GL_EMISSION,m_materials[index]->m_emissive);
				glMaterialf(GL_FRONT,GL_SHININESS,m_materials[index]->m_shininess);

				if(m_materials[index]->m_type==Model::Material::MATTYPE_TEXTURE
				&&(!m_materials[index]->m_textureData->m_isBad||drawOptions&DO_BADTEX))
				{
					if(drawContext)					
					glBindTexture(GL_TEXTURE_2D,drawContext->m_matTextures[grp->m_materialIndex]);
					else
					glBindTexture(GL_TEXTURE_2D,m_materials[grp->m_materialIndex]->m_texture);

					glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,
					(m_materials[grp->m_materialIndex]->m_sClamp ? GL_CLAMP : GL_REPEAT));
					glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,
					(m_materials[grp->m_materialIndex]->m_tClamp ? GL_CLAMP : GL_REPEAT));

					glEnable(GL_TEXTURE_2D);
				}
				else glDisable(GL_TEXTURE_2D);
			}
			else goto defmat;
		}
		else defmat:
		{
			model_draw_defaultMaterial();
			glDisable(GL_TEXTURE_2D);
		//	glColor3f(0.9f,0.9f,0.9f); //??? //glColorMaterial?
		}

		//colorSelected = false;
		colorSelected = -1;

		glBegin(GL_TRIANGLES);
		for(int i:grp->m_triangleIndices)
		{
			Triangle *triangle = m_triangles[i];
			triangle->m_marked = true;

			if(triangle->m_visible)
			{
				if(triangle->m_selected)
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
					Vertex *vertex = (m_vertices[triangle->m_vertexIndices[v]]);

					glTexCoord2f(triangle->m_s[v],triangle->m_t[v]);
					if(0!=(drawOptions&DO_SMOOTHING))
					{
						glNormal3dv(triangle->m_normalSource[v]);
					}
					else
					{
						glNormal3dv(triangle->m_flatSource);
					}
					glVertex3dv(vertex->m_absSource);
				}
			}
		}
		glEnd();

		if(colorSelected==(int)true) glDisable(GL_LIGHT1); //2020
	}

	glDisable(GL_TEXTURE_2D);

	// Draw ungrouped triangles
	{
		colorSelected = -1; //NEW

		model_draw_defaultMaterial();

		glBegin(GL_TRIANGLES);
		for(auto*triangle:m_triangles) if(!triangle->m_marked)
		{
			triangle->m_marked = true;

			if(triangle->m_visible)
			{
				if(triangle->m_selected)
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
					Vertex *vertex = m_vertices[triangle->m_vertexIndices[v]];

					if(0!=(drawOptions&DO_SMOOTHING))
					{
						glNormal3dv(triangle->m_normalSource[v]);
					}
					else
					{
						glNormal3dv(triangle->m_flatSource);
					}
							
					glVertex3dv(vertex->m_absSource);
				}				
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
			m_bspTree.render(viewPoint,drawContext);		
			glDisable(GL_BLEND);
			glDepthMask(1);

			glDisable(GL_TEXTURE_2D);
		}
	}
}
void Model::draw_bspTree(unsigned drawOptions, ContextT context, double viewPoint[3])
{
	if(m_bspTree.empty()) return;

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

	DrawingContext *drawContext = nullptr;
	if(context)
	{
		drawContext = getDrawingContext(context);

		/*Model::draw should've done this already.
		if(!drawContext->m_valid)
		{
			loadTextures(context);
			log_debug("loaded textures for %p\n",context);
		}*/
	}

	//if(!m_bspTree.empty())
	{
		glDepthMask(0);
		glEnable(GL_BLEND);		
		m_bspTree.render(viewPoint,drawContext);		
		glDisable(GL_BLEND);
		glDepthMask(1);

		glDisable(GL_TEXTURE_2D);
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
	//REMINDER: The reason for a (alpha) is
	//primarily to hide unselected vertices.

	if(!a) //Hide unselected?
	{
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
		Vertex **v = m_vertices.data();
		for(size_t i=m_vertices.size();i-->0;)	
		if(v[i]->m_selected&&v[i]->m_visible)
		{
			glVertex3dv(v[i]->m_absSource);
		}
		glEnd();
		glDepthFunc(GL_LEQUAL);
		//glEnable(GL_DEPTH_TEST);
		return;
	}

	if(1!=a) glEnable(GL_BLEND);

	Vertex **v = m_vertices.data();
	for(size_t i=m_vertices.size();i-->0;)
	{
		v[i]->m_marked = false;
		v[i]->m_marked2 = false;
	}

	//2019: Don't draw back-faces.
	//https://github.com/zturtleman/mm3d/issues/96
	glPolygonMode(GL_FRONT_AND_BACK,GL_POINT); 
	glPointSize(3); 
	glColor4f(1,1,1,a); //white

		_drawPolygons(0,true);
	
	glDisable(GL_BLEND);

	//CAUTION: These need a way to come out on top.
	//glPoloygonOffset is unreliable, so the only
	//other technique is to do everything in 2D.
	//THIS DISABLES glDepthMask
	//glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_ALWAYS);
	glPointSize(5); //4	
	glColor3ub(255,0,0); //red

		_drawPolygons(1,true);

	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); 

	//Draw "free" vertices, and vertices that
	//don't match their triangle's selections.
	//Or, typically all the selected vertices.
	bool colorSelected = true;
	glBegin(GL_POINTS);
	for(size_t i=m_vertices.size();i-->0;)	
	if(!v[i]->m_marked&&!v[i]->m_marked2&&v[i]->m_visible)
	{
		if(v[i]->m_selected)
		{
			if(colorSelected==false)
			{
				glEnd();
				glPointSize(5);
				//glDepthFunc(GL_LEQUAL);
				//glDisable(GL_DEPTH_TEST);
				glDepthFunc(GL_ALWAYS);
				glColor3ub(255,0,0);
				glBegin(GL_POINTS);
				colorSelected = true;
			}
		}
		else
		{		
			//NOTE: It would be better to do this before
			//the second pass above, but I don't think it
			//will be a problem, since it should be "free"
			//vertices.
			if(colorSelected==true)
			{
				glEnd();
				glPointSize(3);
				glDepthFunc(GL_LESS);
				//glEnable(GL_DEPTH_TEST);
				glColor3ub(255,255,255);
				glBegin(GL_POINTS);
				colorSelected = false;
			}
		}		
		glVertex3dv(v[i]->m_absSource);
	}
	glEnd();
	glDepthFunc(GL_LEQUAL);
	//glEnable(GL_DEPTH_TEST);
}
void Model::_drawPolygons(int pass, bool mark)
{	
	validateAnim();

	int cull = m_drawOptions&DO_BACKFACECULL;
	
	//YUCK: Degenerate triangles (in screen space)
	//don't draw vertices.
	if(!cull&&mark) return; //FIX ME

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
				if(mark) for(int i=0;i<3;i++)
				{							
					v[vi[i]]->m_marked2 = true;
				}
				continue;
			}
		}
		else
		{
			//if(pass==1) continue;
			if(pass==0) 
			{
				if(mark) for(int i=0;i<3;i++)
				{
					if(!v[vi[i]]->m_selected)
					v[vi[i]]->m_marked = true;
				}
			}
			else continue;
		}

		double *vertices[3];

		for(int i=0;i<3;i++)
		{		
			vertices[i] = v[vi[i]]->m_absSource;
		}

		if(triangle->m_visible)
		{
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

	bool skel = inSkeletalMode();
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
			ea->m_marked = skel&&ea->m_visible
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
			glColor3f(0,0,l);
			for(auto*ea:m_joints)
			if(!ea->m_marked&&ea->m_visible)
			{
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
		if(!joint->m_visible
		||!parent->m_visible) continue;

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

void Model::drawPoints()
{
	validateAnim();

	//float scale = 2; //???
	double scale = m_viewportUnits.ptsz3d;
	if(!scale) scale = 0.25;
	scale*=m_viewportUnits.inc3d;
	if(!scale) scale = 2;
	
	glPointSize(3);
	for(unsigned p = 0; p<m_points.size(); p++)
	{
		if(m_points[p]->m_visible)
		{
			Point *pt = m_points[p];

			model_draw_drawPointOrientation(pt->m_selected,scale,pt->getMatrix());

			if(pt->m_selected)
			{
				glColor3f(0.7f,1,0);
			}
			else
			{
				glColor3f(0,0.5f,0);
			}

			glBegin(GL_POINTS);
			glVertex3dv(pt->m_absSource);
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


