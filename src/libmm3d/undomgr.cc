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

#include "time.h"

#include "undomgr.h"

#include "log.h"
#include "undo.h"

void UndoList::setOpName(const char *name)
{
	m_name = name; m_time = time(0);
}
bool UndoList::isEdit()const
{
	for(Undo*ea:*this)
	if(!ea->nonEdit()) return true; return false;
}

UndoManager::UndoManager()
	: m_currentUndo(nullptr),
	  m_currentList(nullptr),
	  //m_listCombine(true), //RETIRED
	  m_sizeLimit(0),
	  m_countLimit(0),
	  m_saveLevel(0)
{
}

UndoManager::~UndoManager()
{
	clear();
}

void UndoManager::setSaved()
{
	m_saveLevel = 0;
}

bool UndoManager::isSaved()const
{
	return (m_saveLevel==0)?true:false;
}

bool UndoManager::isEdited()const
{
	if(!isSaved())
	return true;	
	if(!canUndo()&&!canRedo())
	return false;
	for(auto&ea:m_atomic)
	{
		if(ea->isEdit()) return true;
	}
	for(auto&ea:m_atomicRedo)
	{
		if(ea->isEdit()) return true;
	}
	return false;
}

void UndoManager::clear()
{
	operationComplete("<Doomed operation>"); //???
	
	for(UndoList*ea:m_atomic)
	{
		for(Undo*ea2:*ea)
		{
			ea2->undoRelease(); //NOTE: Not redoRelease?
			
			ea2->release();
		}
		delete ea;
	}
	m_atomic.clear();

	clearRedo();
}
void UndoManager::clearRedo()
{
	for(UndoList*ea:m_atomicRedo)
	{
		for(Undo*ea2:*ea)
		{
			ea2->redoRelease(); //NOTE: Not undoRelease?
			
			ea2->release();
		}
		delete ea;
	}
	m_atomicRedo.clear();
}
//void UndoManager::addUndo(Undo *u, bool listCombine)
void UndoManager::addUndo(Undo *u, bool combine)
{
	//MAYBE THIS SHOULD BE DONE ON OPERATION-COMPLETE?
	clearRedo();
	
	if(m_currentUndo)
	{
		if(combine&&combineWithList(u))
		{
			// Combined, release new one
			u->release(); return;
		}
	}
	
	//2022: For some reason the original code waited
	//until there is two undos to form the list, but
	//that makes things ver complicated.
	pushUndoToList(u); m_currentUndo = u;
}

void UndoManager::operationComplete(const char *opname, int compound_ms)
{
	// if we have anything to undo
	if(m_currentUndo)
	{
		//Ensure this for Model::appendUndo.
		if(!opname||!*opname)
		{
			assert(opname&&*opname); //_operationCompleteInternal?

			//For some reason operationComplete() is 
			//called by undo() and redo(). I think if
			//it gets here, perhaps appendUndo is best.
			//assert(*opname);
			//opname = "<Unnamed Undo>";
			opname = "<Unnamed operation>";
		}

		//log_debug("operation complete: %s\n",opname);

		//This old code waited to form the list for some reason???
		//pushUndoToList(m_currentUndo);
		m_currentList->setOpName(opname);

		bool keep = true;
		if(compound_ms&&!m_atomic.empty()) //2023		
		{
			auto *a = m_atomic.back();
			auto *b = m_currentList;

			if(a->m_name==b->m_name)
			{
				double ms = 1000*difftime(b->m_time,a->m_time);
				if(ms<=(double)compound_ms)
				{
					keep = false;

					m_currentList = a; //HACK#1 (combineWithList)

					size_t i = 0, sz = b->size();
					for(;i<sz;i++)
					if(!combineWithList((*b)[i]))
					break;						
					a->insert(a->end(),b->begin()+i,b->end());

					a->m_time = b->m_time; //!

					b->clear(); delete b;

					m_currentList = nullptr; //HACK#2
				}
			}
		}
		if(keep)
		{
			m_atomic.push_back(m_currentList);

			//NEW: Don't pester users for things like
			//playing animations.
			if(m_currentList->isEdit()) m_saveLevel++; 
		}
			
		checkSize(); //???

		showStatistics(); //???
	}
	else
	{
		//log_debug("nothing to undo\n");
	}
	m_currentList = nullptr;
	m_currentUndo = nullptr;
	//m_listCombine = true; //RETIRED

//	checkSize(); //???

//	showStatistics(); //???
}

UndoList *UndoManager::undo()
{
	//LOG_PROFILE(); //???

	operationComplete("<Partial operation>"); //???

	if(!m_atomic.empty())
	{
		m_atomicRedo.push_back(m_atomic.back());
		m_atomic.pop_back();

		showStatistics();

		//log_debug("Undo: %s\n",m_atomicRedo.back()->getOpName());

		if(m_atomicRedo.back()->isEdit()) //2021
		m_saveLevel--;

		return m_atomicRedo.back();
	}
	else return nullptr;
}

UndoList *UndoManager::redo()
{
	//LOG_PROFILE(); //???

	operationComplete("<Partial operation>"); //???

	if(!m_atomicRedo.empty())
	{
		m_atomic.push_back(m_atomicRedo.back());
		m_atomicRedo.pop_back();

		showStatistics();

		//log_debug("Redo: %s\n",m_atomic.back()->getOpName());

		m_saveLevel++;

		return m_atomic.back();
	}
	else return nullptr;
}

const char *UndoManager::getUndoOpName()const
{
	if(m_currentUndo) //???
	{
		return "";
	}
	else if(!m_atomic.empty())
	{
		auto *o = m_atomic.back()->getOpName(); //isEdit?
		return o;
	}
	else return "";
}

const char *UndoManager::getRedoOpName()const
{
	if(!m_atomicRedo.empty())
	{
		auto *o = m_atomicRedo.back()->getOpName(); //isEdit?
		return o;
	}
	else return "";
}

UndoList *UndoManager::undoCurrent()
{
	//LOG_PROFILE(); //???

	if(m_currentUndo)
	{
		operationComplete("<Partial operation>"); //???

		if(!m_atomic.empty())
		{
			m_atomicRedo.push_back(m_atomic.back());
			m_atomic.pop_back();

			//log_debug("Undo: %s\n",m_atomicRedo.back()->getOpName());
			return m_atomicRedo.back();
		}
	}
	return nullptr;
}

bool UndoManager::combineWithList(Undo *u)
{
	/*2022: In some cases it's too dangerous not to combine
	//so I have to reinstate this, however the only thing
	//keeping undo operations in order is operationComplete
	//calls.
	if(m_currentUndo->combine(u))
	{
		return true;
	}*/
	//else if(m_listCombine)
	{
		//FIX ME!
		//It seems like this can get out of sequence? Why not just 
		//combine to back of the list?
		//https://github.com/zturtleman/mm3d/issues/138
		if(m_currentList)
		if(u->combine()!=Undo::CC_Unimplemented) 
		{
			//NOTE: This hadn't used reverse iterators, but even if
			//there's no reason for doing so it should be faster to
			//find the matching undo. Note, for those that don't do
			//combine this is just a waste loop.
			auto rit = m_currentList->rbegin();
			auto rtt = m_currentList->rend();
			for(;rit<rtt;rit++)
			{
				//2022: This is a new system...
				//if((*rit)->combine(u)) 
				//return true;
				switch((*rit)->combine(u))
				{
				case Undo::CC_Success: return true;
				case Undo::CC_Stop: return false;
				case Undo::CC_Continue: continue;
				}
			}
		}
	}
	return false;
}

void UndoManager::pushUndoToList(Undo *u)
{
	if(!m_currentList)
	{
		m_currentList = new UndoList;
	}
	m_currentList->push_back(u);
}

void UndoManager::showStatistics()const
{
	int undoItems = 0;
	int redoItems = 0;
	unsigned undoSize = 0;
	unsigned redoSize = 0;

	AtomicList::const_iterator it;
	UndoList::const_iterator uit;

	//log_debug("Undo:\n");
	for(it = m_atomic.begin(); it!=m_atomic.end(); it++)
	{
		//log_debug("  %s\n",(*it)->getOpName());
		undoItems += (*it)->size();
		for(uit = (*it)->begin(); uit!=(*it)->end(); uit++)
		{
			undoSize += (*uit)->size();
		}
	}
	//log_debug("\n");

	//log_debug("Redo:\n");
	for(it = m_atomicRedo.begin(); it!=m_atomicRedo.end(); it++)
	{
		//log_debug("  %s\n",(*it)->getOpName());
		redoItems += (*it)->size();
		for(uit = (*it)->begin(); uit!=(*it)->end(); uit++)
		{
			redoSize += (*uit)->size();
		}
	}
	//log_debug("\n");

	//log_debug("--------------- Undo statistics ---------------\n");
	//log_debug(" undo:  %7d size,%5d items,%5d lists\n",undoSize,undoItems,m_atomic.size());
	//log_debug(" redo:  %7d size,%5d items,%5d lists\n",redoSize,redoItems,m_atomicRedo.size());
	//log_debug(" total: %7d size,%5d items,%5d lists\n",undoSize+redoSize,undoItems+redoItems,m_atomic.size()+m_atomicRedo.size());
	//log_debug("-----------------------------------------------\n");
}

size_t UndoManager::getSize() //NEW
{
	size_t sum = 0;
	for(auto*ea:m_atomic) for(auto*ea2:*ea)
	sum+=ea2->size(); return sum;
}
void UndoManager::checkSize()
{	
	size_t count1 = 0, count2 = 0;

	if(m_sizeLimit) //2019: Ignoring MAX_UNDO_LIST_SIZE
	{
		size_t size = getSize();

		if(size>m_sizeLimit)
		{
			//log_debug("Undo list size is %d,freeing a list\n",size);

			auto it = m_atomic.begin();
			for(auto itt=m_atomic.end();it<itt;it++)
			{
				size_t sum = 0;
				for(Undo*ea:**it) sum+=ea->size();

				size-=sum;
				
				if(size>m_sizeLimit) 
				{
					count1++;
				}
				else break;
			}
		}
	}

	if(m_countLimit)
	if(m_atomic.size()>m_countLimit)
	{
		count2 = m_atomic.size()-m_countLimit;
	}

	size_t count = std::max(count1,count2);

	auto itt = m_atomic.begin()+count;
	for(auto it=itt-count;it<itt;it++)
	{
		for(Undo*ea:**it)
		{
			ea->undoRelease(); ea->release();
		}
		delete *it;
	}
	m_atomic.erase(itt-count,itt);
}
