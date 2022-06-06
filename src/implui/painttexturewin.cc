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
#include "win.h"

#include "model.h"
#include "log.h"
#include "misc.h"
#include "msg.h"

#include "texture.h"
#include "texwidget.h"

#include "filedatadest.h"

struct PaintTextureWin : Win
{
	void submit(int);

	PaintTextureWin(Model *model)
		:
	//Win("Paint Texture"),
	Win("Plot Texture Coordinates"),
	model(model),
	shelf1(main),
	plot(shelf1,"Plot:\t"),
	faces(shelf1,"Faces"),
	edges(shelf1,"Edges"),
	vertices(shelf1,"Vertices"),
	shelf2(main),
	width(shelf2,"Size\t",'X'),width_v(width,'X'), //"Save Size:"
	height(shelf2,"x\t",'Y'),height_v(height,'Y'),
	save(shelf2,"Save...",id_browse), //"Save Texture..."
	ok(main),

	//2019: Putting image on bottom so if it's larger
	//than the screen the interface is not off screen.

	scene(main,id_scene),texture(scene)
	{
		ok.ok.name("Close").id(id_close);

		active_callback = &PaintTextureWin::submit;

		submit(id_init);
	}

	Model *model;

	row shelf1;
	titlebar plot;
	boolean faces,edges,vertices;
	row shelf2;
	textbox width,height;
	dropdown width_v,height_v;
	button save;
	f1_ok_panel ok;
	canvas scene;

	Win::texture texture;
};
void PaintTextureWin::submit(int id)
{
	if(id!=id_browse) switch(id)
	{
	case id_init:
	{
		texture.setModel(model);
		texture.setSolidBackground(true);

		scene.space(1,1,4,1);

		//HACK?
		//DON'T USE canvas'S SPECIAL ALIGNMENT MODE
		//SINCE IT EXPANDS. COULD USE lock TOO, BUT
		//CANVASES assert ON lock UNTIL IMPLEMENTED.
		scene.calign(); 

		//shelf1.proportion();
		shelf1.space(9); faces.space<right>()+=10;

		shelf2.expand(); save.ralign();	

		faces.set(); edges.set();
		
		for(int x=1024;x>=64;x/=2)
		{
			auto i = new li::item(x);
			i->text().format("%d",x); width_v.add_item(i);
		}
		height_v.reference(width_v);

		width.edit(64,512,8192).compact(62);
		height.edit(64,512,8192).compact(2+width.active_area<2>(-2));
		width.space<right>()+=1;
		height.space<left>()+=2;

		int_list l;		
		model->getSelectedTriangles(l);		

		int material = -1;

		//FIX ME (LOOKS BOGUS)
		//https://github.com/zturtleman/mm3d/issues/54
		for(int i:l)
		{
			int g = model->getTriangleGroup(i);
			int m = model->getGroupTextureId(g);
			if(m>=0)
			{
				texture.setTexture(m);
				material = m;
				//break;
			}
			break; //getTriangleGroup workaround.
		}
		if(material==-1)
		{
			log_error("no group selected\n");
		}

		//addTriangles();
		{
			//HACK: Currently unselecting both modes
			//uses DM_Edit which draws vertices red.
			texture.setSelectionColor(0x80ffffff);

			texture.clearCoordinates();

			float s,t; 
			int v = -3; for(int i:l)
			{
				v+=3; for(int j=0;j<3;j++)
				{
					model->getTextureCoords(i,j,s,t);
					texture.addVertex(s,t);					
				}
				texture.addTriangle(v,v+1,v+2);
			}
		}

		if(material<0)
		{
			texture.setTexture(-1);
		}
		else if(auto*tex=model->getTextureData(material))		
		{
			//int x = 64; while(x<tex->m_width) x*=2;
			//int y = 64; while(y<tex->m_height) y*=2;
			//width.select_id(x); height.select_id(y);
			width.set_int_val(tex->m_width);
			height.set_int_val(tex->m_height);
		}

		//break;
	}
	case 'X': case 'Y':
	{	
		int w = width, h = height;
		scene.offset_dims(&w,&h); 
		scene.lock(w,h);
		//break;
	}
	default:
	
		texture.setDrawMode((TextureWidget::DrawModeE)((int)edges+2*(int)faces));
		texture.setDrawVertices(vertices);
		texture.sizeOverride(width,height);
		texture.updateWidget();
		//break;

	case id_close: return basic_submit(id);

	case id_scene:

		texture.draw(scene.x(),scene.y(),scene.width(),scene.height());
		return;
	}

	const char *modelFile = model->getFilename();
	FileBox file = config.get("ui_model_dir");
	if(*modelFile) //???
	{
		std::string fullname,fullpath,basename; //REMOVE US
		normalizePath(modelFile,fullname,fullpath,basename);
		file = fullpath.c_str();
	}
		
	file.locate("Save PNG",::tr("File name for saved texture?"),true);
	if(file.empty()) 	
	{
		//log_debug("save frame buffer canceled\n");
		return;
	}

	int x,y,w,h; texture.getGeometry(x,y,w,h); 
	int il = glutext::glutCreateImageList();
	int*pb =*glutext::glutLoadImageList(il,w,h,false);

	file.push_back('\0');
	size_t buf = file.size();

	glPixelStorei(GL_PACK_ALIGNMENT,1);
	if(!pb) assert(pb);
	else glReadPixels(x,y,pb[0],pb[1],pb[2],pb[3],(void*&)pb[4]);

	if(!file.render(il,"PNG")
	||!FileDataDest(file.c_str()).writeBytes(&file[buf],file.size()-buf))
	{
		msg_error("%s\n%s",::tr("Could not write file: "),file.c_str());
	}

	glutext::glutDestroyImageList(il);
}
extern void painttexturewin(Model *model)
{
	if(!model->getSelectedTriangleCount())
	{
		msg_info(::tr("You must select faces first.\n"
		"Use the 'Select Faces' tool.",
		"Notice that user must have faces selected to open 'paint texture' window"));
	}
	else PaintTextureWin(model).return_on_close(); 
}