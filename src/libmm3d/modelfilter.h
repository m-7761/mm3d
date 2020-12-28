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


#ifndef __MODELFILTER_H
#define __MODELFILTER_H

#include "filefactory.h"
#include "model.h"

//------------------------------------------------------------------
// About the ModelFilter class
//------------------------------------------------------------------
//
// The ModelFilter class is a base class for implementing filters to
// import and export models to various formats.  If you implement a
// ModelFilter,you need to register the filter with the FilterManager.
// You only need one instance of your filter.

class ModelFilter
{
public:

	// This class is used to provide model-specific options.  See
	// the ObjFilter class in objfilter.h and the ObjPrompt
	// class in objprompt.h for an example.
	//
	// For simple filters you can typically ignore anything related
	// to the Options class.
	class Options
	{
	public:

		Options();

		// It is a good idea to override this if you implement
		// filter options in a plugin.
		virtual void release(){ delete this; };

		static void stats(); //???

		virtual void setOptionsFromModel(Model*){} //NEW

		template<class T> T *getOptions()
		{	
			assert(dynamic_cast<T*>(this)); return (T*)this;
		}

	protected:

		virtual ~Options(); // Use release() instead

		static int s_allocated;
	};

	// To prompt a user for filter options,create a function
	// that matches this prototype and call setOptionsPrompt.
	//
	// The Model argument indicates the model that will be saved.
	//
	// The ModelFilter::Options argument contains the options that
	// should be set as default when the prompt is displayed.
	//
	// You must modify the value of the Options argument to match 
	// the options selected by the user.
	//
	// The return value is false if the prompt (save)was canceled,
	// and true otherwise.
	//
	// For simple filters you can typically ignore anything related
	// to the Options class.
	typedef bool PromptF(Model*,ModelFilter::Options*);

	ModelFilter();

	virtual ~ModelFilter(){}

	// It is a good idea to override this if you implement
	// a filter as a plugin.
	virtual void release(){ delete this; };

	// readFile reads the contents of 'file' and modifies 'model' to
	// match the description in 'file'.  This is the import function.
	//
	// The model argument will be an empty model instance.  If the file
	// cannot be loaded,return the appropriate ModelErrorE error code.
	// If the load succeeds,return Model::ERROR_NONE.
	virtual Model::ModelErrorE readFile(Model *model, const char *const file)= 0;

	// writeFile writes the contents of 'model' to the file 'file'.
	//
	// If the model cannot be written to disk,return the appropriate 
	// ModelErrorE error code.  If the write succeeds,return 
	// ModelErrorE::ERROR_NONE.
	//
	// For simple filters you can typically ignore anything related
	// to the Options class.  If you do not provide model specific options
	// with your filter the Options argument will always be nullptr.
	virtual Model::ModelErrorE writeFile(Model *model, const char *const file, Options &o) = 0;

	// This function returns an STL list of STL strings of file patterns
	// for which your model supports read operations.  Generally only one 
	// format type should be provided by a single filter.
	virtual const char *getReadTypes(){ return ""; }

	// This function returns an STL list of STL strings of file patterns
	// for which your model supports write operations.  Generally only one 
	// format type should be provided by a single filter.
	virtual const char *getWriteTypes(){ return ""; }

	virtual const char *getExportTypes(){ return getWriteTypes(); }

	// This function returns a dynamically allocated object derived
	// from ModelFilter::Options which holds filter-specific options that
	// the user can specify via a prompt at the time of save.
	//
	// To prompt a user for options,you most provide a prompt function 
	// using setOptionsPrompt()
	//
	// For simple filters you can typically ignore anything related
	// to the Options class.
	virtual Options *getDefaultOptions();

	// This function takes a pointer to a function which displays a prompt
	// to get filter options from the user.
	//
	// For simple filters you can typically ignore anything related
	// to the Options class.
	virtual void setOptionsPrompt(PromptF *f){ m_promptFunc = f; };

	// This function returns the pointer to the Options prompt.
	// This may be nullptr.
	//
	// For simple filters you can typically ignore anything related
	// to the Options class.
	virtual PromptF *getOptionsPrompt(){ return m_promptFunc; };

	// This function should return true if the file's extension matches 
	// a type supported by your filter,and your filter has read support.
	//
	// A nullptr argument means,do you support write operations for your
	// supported format[s]?
	bool canRead(const char *file=nullptr);

	// This function should return true if the file's extension matches 
	// a type supported by your filter,your filter has write support,and
	// over-writing a file of this type is not likely to be problematic
	// (for example,if the model has a skeleton and animations and your
	// write support does not include skeleton and animations,you don't
	// want to overwrite the original if the user accidently selects "Save").
	//
	// If write support is limited,you'll want to allow canExport().
	//
	// A nullptr argument means,do you support write operations for your
	// supported format[s]?
	bool canWrite(const char *file=nullptr);

	// This function should return true if the file's extension matches 
	// a type supported by your filter,and your filter has write support.
	// The canExport function may return true even if the canWrite function
	// returns false. If the canWrite function returns true,canExport
	// should also return true.
	//
	// A nullptr argument means,do you support write operations for your
	// supported format[s]?
	bool canExport(const char *file=nullptr);

	// This function should return true if the file's extension matches 
	// a type supported by your filter,regardless of whether your filter
	// is read-only,write-only,or read-write.
	bool isSupported(const char *file);

	// Overrides the default FileFactory with a custom version.
	// This can be used to change how files are read and written by
	// any file filters that use the ModelFilter's openInput and
	// openOutput functions.
	void setFactory(FileFactory *factory){ m_factory = factory; }

	// Default error should be Model::ERROR_FILE_OPEN,ERROR_FILE_READ,or
	// ERROR_FILE_WRITE depending on the context of operations being performed.
	static Model::ModelErrorE errnoToModelError(int err,Model::ModelErrorE defaultError);

protected:

	DataSource *openInput(const char *file, Model::ModelErrorE &err);
	DataDest *openOutput(const char *file, Model::ModelErrorE &err);

	PromptF *m_promptFunc;

	FileFactory	m_defaultFactory, *m_factory;
};

//2020: These are here to cut back on the huge number of 
//files so finding a file doesn't involve wading through
//hundreds of source code files!!

class Ms3dOptions : public ModelFilter::Options
{
public:

	int m_subVersion = 0;
	uint32_t m_vertexExtra = 0xffffffff;
	uint32_t m_vertexExtra2 = 0xffffffff;
	uint32_t m_jointColor = 0xffffffff;

	virtual void setOptionsFromModel(Model*); //ms3dfilter.cc
};

class SmdOptions : public ModelFilter::Options
{
public:

	bool m_saveMeshes = true;
	bool m_savePointsJoint = false;
	bool m_multipleVertexInfluences = false;
	std::vector<unsigned int> m_animations;

	virtual void setOptionsFromModel(Model*); //smdfilter.cc
};

class IqeOptions : public ModelFilter::Options
{
public:

	bool m_saveMeshes = true;
	bool m_savePointsJoint = true;
	bool m_savePointsAnim = true;
	bool m_saveSkeleton = true;
	bool m_saveAnimations = true;
	std::vector<unsigned int> m_animations;
};

// For a class derived from ModelFilter::Options,you'll
// want to add member variables for everything the user
// can control.  Whether or not you add accessors
// for the member variables is left to your discretion.
//
// Set the default values for the filter options in
// the constructor.
class ObjOptions : public ModelFilter::Options
{
public:

	bool m_saveNormals = true;
	int m_places = 6;
	int m_texPlaces = 6;
	int m_normalPlaces = 6;
};

#endif // __MODELFILTER_H
