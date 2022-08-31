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


#ifndef __UNDOMGR_H
#define __UNDOMGR_H

#include "mm3dtypes.h"

#include "undo.h" //template?

class UndoList : public std::vector<Undo*>
{
public:
		
	void setOpName(const char *name){ m_name = name; }

	const char *getOpName()const{ return m_name.c_str(); };

	bool isEdit()const;

protected:

	std::string m_name;
};

typedef std::vector<UndoList*> AtomicList;

class UndoManager
{
public:

	UndoManager(),~UndoManager();

	void clear();

	void setSaved();
	bool isSaved()const;
	bool isEdited()const;

	//See Model::sendUndo note.
	//void addUndo(Undo *u,bool listCombine = false);
	void addUndo(Undo *u, bool combine=true);
	void operationComplete(const char *opname);

	// Items should be applied in reverse order (back to front)
	UndoList *undo();

	// Items should be applied in forward order (front to back)
	UndoList *redo();

	// True if there is an undo list available
	bool canUndo()const { return !m_atomic.empty(); };

	// True if there is a redo list available
	bool canRedo()const { return !m_atomicRedo.empty(); }; 

	const char *getUndoOpName()const;
	const char *getRedoOpName()const;

	//Exposing in order to implement Model::appendUndo.
	AtomicList &getUndoStack(){ return m_atomic; }

	//2022: This is helpful for sorted_list based undo
	//object performance.
	template<class U> U *resume()
	{
		if(auto*l=m_currentList)
		for(auto rit=l->rbegin();rit<l->rend();rit++)
		{
			if(auto*u=dynamic_cast<U*>(*rit))
			return u;
			if(U::resume_first_only
			||Undo::CC_Stop==(*rit)->combine())
			return nullptr;
		}
		return nullptr;
	}

	// Only returns undo list if there is one being built
	// Items should be applied in reverse order (back to front)
	UndoList *undoCurrent();

	// Allow intelligent handling of window close button
	bool canUndoCurrent()const{ return m_currentUndo!=nullptr; }

	void showStatistics()const;

	void setSizeLimit(unsigned sizeLimit) { m_sizeLimit  = sizeLimit; };
	void setCountLimit(unsigned countLimit){ m_countLimit = countLimit; };

	size_t getSize(); //NEW

protected:

	enum //REMOVE ME (20mb IS WAY TOO SMALL?)
	{
		MAX_UNDO_LIST_SIZE = 20000000  // 20mb
	};

	bool combineWithList(Undo *u);
	void pushUndoToList(Undo *u);
	void clearRedo();
	void checkSize();

	Undo		 *m_currentUndo;
	UndoList	*m_currentList;
	AtomicList	m_atomic;
	AtomicList	m_atomicRedo;
	//bool			m_listCombine; //RETIRED

	unsigned	  m_sizeLimit;
	unsigned	  m_countLimit;
	int			 m_saveLevel;
};

#endif // __UNDOMGR_H
