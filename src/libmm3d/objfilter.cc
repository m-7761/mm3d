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

#include "modelfilter.h"
#include "datadest.h"
#include "datasource.h"

//#include "objfilter.h"
class ObjFilter : public ModelFilter
{
public:

	// Note, filename interacts with FileFactory:
	// https://github.com/zturtleman/mm3d/issues/70
	// It would be best to pass DataDest/DataSource
	// by reference, except for multipart models is
	// an open problem.
	//
	virtual Model::ModelErrorE readFile(Model *model, const char *const filename);
	virtual Model::ModelErrorE writeFile(Model *model, const char *const filename, Options &o);

	virtual const char *getReadTypes(){ return "OBJ"; }
	virtual const char *getWriteTypes(){ return "OBJ"; }

	// Create a new options object that is specific to this filter
	virtual Options *getDefaultOptions(){ return new ObjOptions; };

	class ObjMaterial
	{
		public:
			ObjMaterial();

			std::string name;
			float		 diffuse[4];
			float		 ambient[4];
			float		 specular[4];
			float		 shininess;
			float		 alpha;
			std::string textureMap;
	};

	struct UvDataT
	{
		float u,v;
	};
	typedef std::vector<UvDataT> UvDataList;

	struct MaterialGroupT
	{
		unsigned material,group;
	};
	typedef std::vector<MaterialGroupT> MaterialGroupList;

protected:
	bool readLine(char *line);
	bool readVertex(char *line);
	bool readTextureCoord(char *line);
	bool readFace(char *line);
	bool readGroup(char *line);
	bool readLibrary(char *line);
	bool readMaterial(char *line);

	bool readMaterialLibrary(const char *filename);

	void addObjMaterial(ObjMaterial *mat);
	char *skipSpace(char *str);

	bool writeLine(const char *line,...);
	bool writeStripped(const char *line,...);
	bool writeHeader();
	bool writeMaterials();
	bool writeGroups();

	Model		 *m_model;
	ObjOptions  *m_options;
	DataSource  *m_src;
	DataDest	 *m_dst;
	int			  m_curGroup;
	int			  m_curMaterial;
	bool			 m_needGroup;
	int			  m_vertices;
	int			  m_faces;
	int			  m_groups;
	UvDataList	 m_uvList;
	MaterialGroupList m_mgList;

	std::string  m_groupName;

	std::string  m_modelPath;
	std::string  m_modelBaseName;
	std::string  m_modelFullName;
	std::string  m_materialFile;
	std::string  m_materialFullFile;
	std::vector<std::string>m_materialNames;
};

#include "../mm3dcore/version.h"
#include "model.h"
#include "texture.h"
#include "log.h"
#include "misc.h"
#include "filtermgr.h"
#include "mm3dport.h"

static void objfilter_replace(char *str,char this_char,char that_char)
{
	for(size_t t=strlen(str);t-->0;)
	{
		if(str[t]==this_char) str[t] = that_char;
	}
}

ObjFilter::ObjMaterial::ObjMaterial()
	: name(""),
	  shininess(0.0f),
	  alpha(1.0f),
	  textureMap("")
{
	for(int t = 0; t<3; t++)
	{
		diffuse[t]  = 0.8f;
		ambient[t]  = 0.2f;
		specular[t] = 0.0f;
	}
	diffuse[3]  = 1.0f;
	ambient[3]  = 1.0f;
	specular[3] = 1.0f;
}

Model::ModelErrorE ObjFilter::readFile(Model *model, const char *const filename)
{
	Model::ModelErrorE err = Model::ERROR_NONE;
	m_src = openInput(filename,err);
	SourceCloser fc(m_src);

	if(err!=Model::ERROR_NONE)
		return err;

	m_modelPath = "";
	m_modelBaseName = "";
	m_modelFullName = "";

	normalizePath(filename,m_modelFullName,m_modelPath,m_modelBaseName);

	model->setFilename(m_modelFullName.c_str());

	m_model = model;
	//log_debug("model has %d faces\n",model->getTriangleCount());

	m_vertices  =  0;
	m_faces	  =  0;
	m_groups	 =  0;
	m_curGroup  = -1;
	m_curMaterial = -1;
	m_needGroup = false;
	m_uvList.clear();
	m_mgList.clear();

	char line[1024];
	while(m_src->readLine(line,sizeof(line)))
	{
		line[sizeof(line)-1] = '\0';
		readLine(line);
	}

	//log_debug("read %d vertices,%d faces,%d groups\n",m_vertices,m_faces,m_groups);

	return Model::ERROR_NONE;
}

Model::ModelErrorE ObjFilter::writeFile(Model *model, const char *const filename, Options &o)
{
	Model::ModelErrorE err = Model::ERROR_NONE;
	m_dst = openOutput(filename,err);
	DestCloser fc(m_dst);

	if(err!=Model::ERROR_NONE)
		return err;

	m_model = model;

	m_materialNames.clear();

	m_modelPath = "";
	m_modelBaseName = "";
	m_modelFullName = "";

	normalizePath(filename,m_modelFullName,m_modelPath,m_modelBaseName);

	m_options = o.getOptions<ObjOptions>();
	writeHeader();
	writeMaterials();
	writeGroups();

	return Model::ERROR_NONE;
}

bool ObjFilter::writeLine(const char *line,...)
{
	va_list ap;
	va_start(ap,line);
	m_dst->writeVPrintf(line,ap);
	va_end(ap);
	m_dst->writePrintf("\r\n");
	return true;
}

enum objfilter_OFWS_StateE
{
	OFWS_Whitespace,
	OFWS_Token,
	OFWS_Number,
	OFWS_Decimal,
	OFWS_AfterDecimal
};

bool ObjFilter::writeStripped(const char *fmt,...)
{
	char line[512];
	char line2[512];

	va_list ap;
	va_start(ap,fmt);
	vsnprintf(line,sizeof(line),fmt,ap);
	objfilter_replace(line,',','.');
	va_end(ap);

	objfilter_OFWS_StateE state = OFWS_Whitespace;

	size_t s = 0;
	size_t d = 0;
	size_t len = strlen(line);

	while(s<len) switch(state)
	{
	case OFWS_Token:

		line2[d] = line[s];
		if(isspace(line2[d]))
		{
			state = OFWS_Whitespace;
		}
		s++; d++; break;

	case OFWS_Whitespace:

		line2[d] = line[s];
		if(!isspace(line2[d]))
		{
			if(isdigit(line2[d])||line2[d]=='-')
			{
				state = OFWS_Number;
			}
			else
			{
				state = OFWS_Token;
			}
		}
		s++; d++; break;

	case OFWS_Number:

		line2[d] = line[s];
		if(!isdigit(line2[d]))
		{
			if(line2[d]=='.')
			{
				state = OFWS_Decimal;
			}
			else if(isspace(line2[d]))
			{
				state = OFWS_Whitespace;
			}
			else
			{
				state = OFWS_Token;
			}
		}
		s++; d++; break;

	case OFWS_Decimal:

		line2[d] = line[s];
		if(isdigit(line2[d]))
		{
			state = OFWS_AfterDecimal;
		}
		else if(isspace(line2[d]))
		{
			state = OFWS_Whitespace;
		}
		else
		{
			state = OFWS_Token;
		}
		//INTENTIONAL???
		//https://github.com/zturtleman/mm3d/issues/152
		//s++; d++;
		s++; d++; break; //2021

	case OFWS_AfterDecimal:

		if(isdigit(line[s]))
		{
			if(line[s]=='0')
			{
				size_t bak = s;

				while(line[s]=='0')
				{
					s++;
				}

				if(isspace(line[s])||line[s]=='\0')
				{
					state = OFWS_Whitespace;
				}
				else
				{
					s = bak;
				}
			}
			line2[d] = line[s];
		}
		else
		{
			line2[d] = line[s];
			if(isspace(line2[d]))
			{
				state = OFWS_Whitespace;
			}
			else
			{
				state = OFWS_Token;
			}
		}
		s++; d++; break;
	}
	
	line2[d] = '\0';

	m_dst->writePrintf("%s\r\n",line2);

	return true;
}

bool ObjFilter::writeHeader()
{
	writeLine("# Wavefront OBJ file");
	writeLine("# Exported by MM3D %s",VERSION); //Maverick Model 3D

	time_t tval;
	struct tm tmval;
	char	tstr[32];

	time(&tval);
	PORT_localtime_r(&tval,&tmval);
	PORT_asctime_r(&tmval,tstr);

	unsigned tlen = strlen(tstr);
	while(isspace(tstr[tlen-1]))
	{
		tstr[tlen-1] = '\0';
		tlen--;
	}

	writeLine("# %s",tstr);

	writeLine("");

	return true;
}

bool ObjFilter::writeMaterials()
{
	if(m_model->getTextureCount()>0)
	{
		/*char *base = strdup(m_modelBaseName.c_str());
		int baseLen = strlen(base);
		for(int t = baseLen-1; t>=0; t--)
		{
			if(base[t]=='.')
			{
				base[t] = '\0';
				break;
			}
		}
		m_materialFile = std::string(base)+".mtl";
		free(base);*/
		m_materialFile = m_modelBaseName;
		auto ext = m_materialFile.rfind('.');
		if(~ext) m_materialFile.erase(ext); //C++
		m_materialFile.append(".mtl");

		m_materialFullFile = m_modelPath+std::string("/")+m_materialFile;

		Model::ModelErrorE err = Model::ERROR_NONE;
		DataDest *dst = openOutput(m_materialFullFile.c_str(),err);
		if(dst->errorOccurred())
		{
			dst->close();
			return false;
		}

		writeLine("mtllib %s",m_materialFile.c_str());
		writeLine("");

		DataDest *saveDst = m_dst;
		m_dst = dst;

		writeLine("# Material file for %s",m_modelBaseName.c_str());
		writeLine("");

		unsigned m = 0;
		unsigned mcount = m_model->getTextureCount();

		for(m = 0; m<mcount; m++)
		{
			std::string matName = m_model->getTextureName(m);
			unsigned matNameLen = matName.size();
			for(unsigned n = 0; n<matNameLen; n++)
			{
				if(isspace(matName[n]))
				{
					matName[n] = '_';
				}
			}

			m_materialNames.push_back(matName);

			writeLine("newmtl %s",matName.c_str());
			float shininess = 1.0f;
			m_model->getTextureShininess(m,shininess);
			writeLine("\tNs %d",(int)shininess);
			writeLine("\td 1");  // TODO adjust this if I ever support transparency
			writeLine("\tillum 2");

			float fval[4] = { 0.0f,0.0f,0.0f,0.0f };

			m_model->getTextureDiffuse(m,fval);
			writeStripped("\tKd %f %f %f",fval[0],fval[1],fval[2]);
			m_model->getTextureSpecular(m,fval);
			writeStripped("\tKs %f %f %f",fval[0],fval[1],fval[2]);
			m_model->getTextureAmbient(m,fval);
			writeStripped("\tKa %f %f %f",fval[0],fval[1],fval[2]);

			if(m_model->getMaterialType(m)==Model::MATTYPE_TEXTURE)
			{
				std::string filename = getRelativePath(m_modelPath.c_str(),m_model->getTextureFilename(m));
				//char *filecpy = strdup(filename.c_str());
				//replaceSlash(filecpy);
				auto filecpy = filename.c_str(); 
				//https://github.com/zturtleman/mm3d/issues/152
				//writeStripped("\tmap_Kd %s",
				writeLine( "\tmap_Kd %s",filecpy+(strncmp(filecpy,".\\",2)?0:2));
				//free(filecpy);
			}

			writeLine("");
		}

		m_dst = saveDst;

		dst->close();
	}
	return true;
}

bool ObjFilter::writeGroups()
{
	int_list tris;

	unsigned v = 0;
	unsigned vcount = m_model->getVertexCount();
	unsigned t = 0;
	unsigned tcount = m_model->getTriangleCount();
	unsigned g = 0;
	unsigned gcount = m_model->getGroupCount();

	char vertfmt[20];
	char texfmt[20];
	char normfmt[20];

	sprintf(vertfmt,"v %%.%df %%.%df %%.%df",
			m_options->m_places,m_options->m_places,m_options->m_places);

	sprintf(texfmt,"vt %%.%df %%.%df",
			m_options->m_texPlaces,m_options->m_texPlaces);

	sprintf(normfmt,"vn %%.%df %%.%df %%.%df",
			m_options->m_normalPlaces,m_options->m_normalPlaces,m_options->m_normalPlaces);

	writeLine("# %d Vertices",vcount);

	for(v = 0; v<vcount; v++)
	{
		double coords[3];
		m_model->getVertexCoords(v,coords);
		writeStripped(vertfmt,coords[0],coords[1],coords[2]);
	}
	writeLine("");

	writeLine("# %d Texture Coordinates",tcount *3);

	for(t = 0; t<tcount; t++)
	{
		float u,v;

		m_model->getTextureCoords(t,0,u,v);
		writeStripped(texfmt,u,v);
		m_model->getTextureCoords(t,1,u,v);
		writeStripped(texfmt,u,v);
		m_model->getTextureCoords(t,2,u,v);
		writeStripped(texfmt,u,v);
	}
	writeLine("");

	if(m_options->m_saveNormals)
	{
		writeLine("# %d Vertex Normals",tcount *3);

		for(t = 0; t<tcount; t++)
		{
			double norm[3];

			m_model->getNormal(t,0,norm);
			writeStripped(normfmt,norm[0],norm[1],norm[2]);
			m_model->getNormal(t,1,norm);
			writeStripped(normfmt,norm[0],norm[1],norm[2]);
			m_model->getNormal(t,2,norm);
			writeStripped(normfmt,norm[0],norm[1],norm[2]);
		}
		writeLine("");
	}

	m_model->getUngroupedTriangles(tris);

	if(!tris.empty())
	{
		writeLine("# %d Ungrouped triangles",tris.size());
		writeLine("");

		writeLine("o ungrouped");
		writeLine("g ungrouped");
		writeLine("");

		if(m_options->m_saveNormals)
		{
			int_list::iterator it;
			for(it = tris.begin(); it!=tris.end(); it++)
			{
				writeLine("f %d/%d/%d %d/%d/%d %d/%d/%d",
						m_model->getTriangleVertex(*it,0)+1,(*it)*3+1,(*it)*3+1,
						m_model->getTriangleVertex(*it,1)+1,(*it)*3+2,(*it)*3+2,
						m_model->getTriangleVertex(*it,2)+1,(*it)*3+3,(*it)*3+3);
			}
		}
		else
		{
			int_list::iterator it;
			for(it = tris.begin(); it!=tris.end(); it++)
			{
				writeLine("f %d/%d %d/%d %d/%d",
						m_model->getTriangleVertex(*it,0)+1,(*it)*3+1,
						m_model->getTriangleVertex(*it,1)+1,(*it)*3+2,
						m_model->getTriangleVertex(*it,2)+1,(*it)*3+3);
			}
		}
	}

	std::string _str;
	for(g = 0; g<gcount; g++)
	{
		tris = m_model->getGroupTriangles(g);

		if(tris.size())
		{
			/*char *grpStr = strdup(m_model->getGroupName(g));
			unsigned grpStrLen = strlen(grpStr);
			for(unsigned n = 0; n<grpStrLen; n++)
			{
				if(isspace(grpStr[n]))
				{
					grpStr[n] = '_';
				}
			}*/
			for(auto&ea:_str=m_model->getGroupName(g))
			if(isspace(ea)) ea = '_';
			auto grpStr = _str.c_str();

			writeLine("# %s,%d grouped triangles",grpStr,tris.size());
			writeLine("");

			int matId = m_model->getGroupTextureId(g);
			if(matId>=0)
			{
				writeLine("usemtl %s",m_materialNames[matId].c_str());
			}

			writeLine("o %s",grpStr);
			writeLine("g %s",grpStr);
			writeLine("");

			//free(grpStr);

			if(m_options->m_saveNormals)
			{
				int_list::iterator it;
				for(it = tris.begin(); it!=tris.end(); it++)
				{
					writeLine("f %d/%d/%d %d/%d/%d %d/%d/%d",
							m_model->getTriangleVertex(*it,0)+1,(*it)*3+1,(*it)*3+1,
							m_model->getTriangleVertex(*it,1)+1,(*it)*3+2,(*it)*3+2,
							m_model->getTriangleVertex(*it,2)+1,(*it)*3+3,(*it)*3+3);
				}
			}
			else
			{
				int_list::iterator it;
				for(it = tris.begin(); it!=tris.end(); it++)
				{
					writeLine("f %d/%d %d/%d %d/%d",
							m_model->getTriangleVertex(*it,0)+1,(*it)*3+1,
							m_model->getTriangleVertex(*it,1)+1,(*it)*3+2,
							m_model->getTriangleVertex(*it,2)+1,(*it)*3+3);
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------
// Protected Methods
//------------------------------------------------------------------

void ObjFilter::addObjMaterial(ObjMaterial *objmat)
{
	if(objmat->name.size()!=0)
	{
		Model::Material *mat = Model::Material::get();

		mat->m_type	  = Model::MATTYPE_BLANK;
		mat->m_name	  = objmat->name;
		mat->m_filename = objmat->textureMap;

		if(!mat->m_filename.empty())
		{
			mat->m_type = Model::MATTYPE_TEXTURE;
		}

		for(int t=0;t<4;t++)
		{
			mat->m_diffuse[t]  = objmat->diffuse[t];
			mat->m_ambient[t]  = objmat->ambient[t];
			mat->m_specular[t] = objmat->specular[t];
			mat->m_emissive[t] = 0.0f;
			//2021: https://github.com/zturtleman/mm3d/issues/157
			//OpenGL's range is 128 but MM3D's is 100
			mat->m_shininess = std::min(100.0f,objmat->shininess); //128
		}

		//FIX ME
		(*(Model::_MaterialList*)&m_model->getMaterialList()).push_back(mat);
	}
}

char *ObjFilter::skipSpace(char *str)
{
	if(str) while(isspace(str[0])) str++; return str;
}

bool ObjFilter::readLine(char *line)
{
	char *str = skipSpace(line);

	if(strncmp(str,"v ",2)==0)
	{
		m_vertices++;
		objfilter_replace(str,',','.');
		readVertex(str);
	}
	else if(strncmp(str,"vt ",3)==0)
	{
		objfilter_replace(str,',','.');
		readTextureCoord(str);
	}
	else if(strncmp(str,"f ",2)==0)
	{
		m_faces++;
		readFace(str);
	}
	else if(strncmp(str,"g ",2)==0)
	{
		m_groups++;
		readGroup(str);
	}
	else if(strncmp(str,"mtllib",6)==0)
	{
		readLibrary(str);
	}
	else if(strncmp(str,"usemtl",6)==0)
	{
		readMaterial(str);
	}
	return true;
}

bool ObjFilter::readVertex(char *line)
{
	line += 2;
	float x,y,z;
	if(sscanf(line,"%f %f %f",&x,&y,&z)==3)
	{
		m_model->addVertex(x,y,z);
		return true;
	}
	return false;
}

bool ObjFilter::readTextureCoord(char *line)
{
	line += 3;
	UvDataT uvd;
	uvd.u = 0.0f;
	uvd.v = 0.0f;

	if(sscanf(line,"%f %f",&uvd.u,&uvd.v)!=2)
	{
		log_warning("could not read 2 texture coordinates from %s\n",line);
	}

	m_uvList.push_back(uvd);

	return true;
}

bool ObjFilter::readFace(char *line)
{
	line += 2;
	int_list vlist;
	int_list vtlist;
	int len = 0;
	int v = 0;

	while(sscanf(line,"%d%n",&v,&len)>0)
	{
		if(v<0)
		{
			v = (int)m_model->getVertexCount()+v;
		}
		else
		{
			v = v-1;
		}

		int texidx  = -1;
		int normidx = -1;

		vlist.push_back(v);

		line = &line[len];

		// Face has texture coords or normals
		if(line[0]=='/')
		{
			line++;

			if(line[0]!='/')
			{
				// Read texture coord index
				texidx = atoi(line);
				if(texidx<0)
				{
					texidx = m_uvList.size()+texidx;
				}
				else
				{
					texidx = texidx-1;
				}
				vtlist.push_back(texidx);

				while(line[0]!='\0'&&line[0]!='/'&&!isspace(line[0]))
				{
					line++;
				}
			}

			if(line[0]=='/')
			{
				line++;
				// Read normal index (and ignore it)
				// NOTE adjust for negative indices
				normidx = atoi(line)-1;

				while(line[0]!='\0'&&line[0]!=' ')
				{
					line++;
				}
			}
		}

		line = skipSpace(line);
	}

	if(vlist.size()<3)
	{
		log_warning("face with less than 3 vertices\n");
		return false;
	}

	bool addTextureCoords = vtlist.size()==vlist.size();
	for(unsigned n=0;n+2<vlist.size();n++)
	{
		int tri = m_model->addTriangle(vlist[0],vlist[n+1],vlist[n+2]);

		if(m_needGroup&&m_curMaterial>=0)
		{
			int count = m_mgList.size();
			std::string name = m_groupName.c_str();
			//2021: Empty group names seem common, these show up as
			//an initial gap in the editor.
			if(count>0||name.empty()) //???
			{
				//I think this needs more thought
				//https://github.com/zturtleman/mm3d/issues/158
				char temp[32];
				snprintf(temp,32,"%d",count+1);
				name += temp;
			}
			MaterialGroupT mg;
			m_curGroup = m_model->addGroup(name.c_str());
			m_model->setGroupTextureId(m_curGroup,m_curMaterial);

			//log_debug("added group for %d %d\n",m_curGroup,m_curMaterial);

			mg.material = m_curMaterial;
			mg.group	 = m_curGroup;
			m_mgList.push_back(mg);

			m_needGroup = false;
		}

		if(m_curGroup>=0)
		{
			m_model->addTriangleToGroup(m_curGroup,tri);
		}

		if(addTextureCoords)
		{
			int i = vtlist[0];
			int j = vtlist[n+1], k = vtlist[n+2]; 
			float st[2][3] = 
			{{m_uvList[i].u,m_uvList[j].u,m_uvList[k].u}
			,{m_uvList[i].v,m_uvList[j].v,m_uvList[k].v}};
			m_model->setTextureCoords(tri,st);
		}
	}
	return true;
}

bool ObjFilter::readGroup(char *line)
{
	int lastGroup = m_curGroup;
	m_needGroup = false;

	line += 2;

	line = skipSpace(line);

	//char *temp = strdup(line);
	//char *ptr = temp;
	char *ptr = line;

	while(*ptr&&!isspace(*ptr))
	{
		ptr++;
	}
	//*ptr = '\0';
	m_groupName.assign(line,ptr-line);

	m_curGroup = -1;

	//if(temp[0]=='\0')
	if(m_groupName.empty())
	{
		m_curGroup = m_model->getGroupByName("default");
		if(m_curGroup<0)
		{
			m_curGroup = m_model->addGroup("default");
			//m_groupName = "default";
		}
		m_groupName = "default"; //2021
	}
	else
	{
		//m_groupName = temp;
		m_curGroup = m_model->getGroupByName(m_groupName.c_str()); //temp
		if(m_curGroup<0)
		{
			m_curGroup = m_model->addGroup(m_groupName.c_str()); //temp
		}
	}

	//log_debug("current group is %s\n",m_groupName.c_str());

	if(m_curGroup!=lastGroup)
	{
		//log_debug("%d!=%d,clearing mg list\n",m_curGroup,lastGroup);
		m_mgList.clear();
	}
	
	if(m_curGroup>=0&&m_curMaterial>=0)
	{
		m_model->setGroupTextureId(m_curGroup,m_curMaterial);

		MaterialGroupT mg;
		mg.material = m_curMaterial;
		mg.group	 = m_curGroup;
		m_mgList.push_back(mg);
	}

	//free(temp);

	return true;
}

bool ObjFilter::readLibrary(char *line)
{
	char filename[256];

	//I don't see where is it modified?
	//char *temp = strdup(line);
	//char *ptr = temp;
	char *ptr = line; do 
	{
		while(*ptr&&!isspace(*ptr))
		ptr++;
		while(*ptr&&isspace(*ptr))
		ptr++;

		if(*ptr)
		{
			unsigned len = 0;
			if(sscanf(ptr,"%[^ \t\r\n]%n",filename,(int*)&len)>0)
			{
				std::string fullFilename = m_modelPath+std::string("/")+filename;
				//log_debug("material library: %s\n",fullFilename.c_str());
				readMaterialLibrary(fullFilename.c_str());
				ptr+=len;
			}
		}

	}while(*ptr);

	//free(temp);

	return true;
}

bool ObjFilter::readMaterial(char *line)
{
	//char *temp = strdup(line);
	//char *ptr = temp;
	char *ptr = line;
	while(*ptr&&!isspace(*ptr))
	ptr++;
	while(*ptr&&isspace(*ptr))
	ptr++;
	if(*ptr)
	{
		char *filename = ptr;
		while(*ptr&&!isspace(*ptr))
		ptr++;
		char swap = *ptr;
		*ptr = '\0';
		m_curMaterial = m_model->getMaterialByName(filename);
		*ptr = swap;
		m_needGroup = true;

		unsigned count = m_mgList.size();
		for(unsigned t = 0; t<count; t++)
		{
			if(m_mgList[t].material==(unsigned)m_curMaterial)
			{
				m_curGroup = m_mgList[t].group;
				//log_debug("already have group for %d %d\n",m_curGroup,m_curMaterial);
				m_needGroup = false;
				break;
			}
		}
	}
	else m_curMaterial = m_model->getMaterialByName("default");

	//if(m_needGroup)
	//log_debug("need group for %d %d\n",m_curGroup,m_curMaterial);

	//free(temp);

	return true;
}

bool ObjFilter::readMaterialLibrary(const char *filename)
{
	Model::ModelErrorE err = Model::ERROR_NONE;
	DataSource *src = openInput(filename,err);
	if(src->errorOccurred())
	{
		src->close();
		return false;
	}

	char line[1024];
	char *ptr;

	ObjMaterial *objmat = nullptr;

	while(src->readLine(line,sizeof(line)))
	{
		ptr = line;
		while(ptr[0]&&isspace(ptr[0]))
		{
			ptr++;
		}

		if(strncmp("newmtl",ptr,6)==0)
		{
			if(objmat)
			{
				addObjMaterial(objmat);
				delete objmat;
			}
			objmat = new ObjMaterial();

			ptr += 7;
			while(ptr[0]&&isspace(ptr[0]))
			{
				ptr++;
			}

			char *name = ptr;
			while(ptr[0]&&!isspace(ptr[0]))
			{
				ptr++;
			}
			ptr[0] = '\0';

			objmat->name = name;
		}
		else if(strncmp("map_Kd",ptr,6)==0)
		{
			ptr += 7;
			while(ptr[0]&&isspace(ptr[0]))
			{
				ptr++;
			}

			char *filename = ptr;
			while(ptr[0]&&ptr[0]!='\r'&&ptr[0]!='\n')
			{
				ptr++;
			}
			ptr[0] = '\0';

			replaceBackslash(filename);
			const std::string relativePath = fixFileCase(m_modelPath.c_str(),filename);
			objmat->textureMap = getAbsolutePath(m_modelPath.c_str(),relativePath.c_str());
			//log_debug("  texture map is '%s'\n",objmat->textureMap.c_str());
		}
		else if(strncmp("Kd ",ptr,3)==0)
		{
			ptr += 3;
			if(objmat)
			{
				sscanf(ptr,"%f %f %f",
						&objmat->diffuse[0],
						&objmat->diffuse[1],
						&objmat->diffuse[2]);
			}
		}
		else if(strncmp("Ka ",ptr,3)==0)
		{
			ptr += 3;
			if(objmat)
			{
				sscanf(ptr,"%f %f %f",
						&objmat->ambient[0],
						&objmat->ambient[1],
						&objmat->ambient[2]);
			}
		}
		else if(strncmp("Ks ",ptr,3)==0)
		{
			ptr += 3;
			if(objmat)
			{
				sscanf(ptr,"%f %f %f",
						&objmat->specular[0],
						&objmat->specular[1],
						&objmat->specular[2]);
			}
		}
		else if(strncmp("Ns ",ptr,3)==0)
		{
			ptr += 3;
			objmat->shininess = (float)atof(ptr);
		}
		else if(strncmp("d ",ptr,2)==0||strncmp("Tr ",ptr,3)==0)
		{
			ptr += 2;
			objmat->alpha = (float)atof(ptr);
		}
	}

	if(objmat)
	{
		addObjMaterial(objmat);
		delete objmat;
	}

	return true;
}

extern ModelFilter *objfilter(ModelFilter::PromptF f)
{
	auto o = new ObjFilter; o->setOptionsPrompt(f); return o;
}
