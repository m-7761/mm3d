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


#ifndef __SORTED_LIST_H
#define __SORTED_LIST_H

#include <vector>

template<typename T> 
class sorted_list : public std::vector<T>
{
public:
	
	size_t insert_sorted(const T &val);
	void insert_sorted(const T &val, unsigned index);
	bool find_sorted(const T &val, unsigned &index)const;
};

template<typename T> 
size_t sorted_list<T>::insert_sorted(const T &val)
{
	if(!empty()&&val<back())
	{
		/*for(auto it=begin();it!=end();it++)
		{
			if(val<*it) break;
		}*/
		auto it = std::lower_bound(begin(),end(),val);
		return insert(it,val)-begin();
	}
	else push_back(val); return size()-1;
}
template<typename T> 
void sorted_list<T>::insert_sorted(const T &val, unsigned index)
{
	#ifdef _DEBUG
	if(!empty())
	{
		auto *d = data();
		auto &bf = d[index-1], &af = d[index];
		assert(index==0||bf<val||!(val<bf));
		assert(index<=size());
		assert(index==size()||val<af||!(af<val));
	}
	else assert(index==0);	
	#endif

	insert(begin()+index,val);
}

template<typename T> 
bool sorted_list<T>::find_sorted(const T &val, unsigned &index)const
{
	//(*this)[mid] is slow in debug builds (STL oob-checks)
	const T *it,*d = data();

	if(1) 
	{
		//Note, this is the original code, and I guess
		//it has value because Microsoft's lower_bound 
		//seems to do oob-checks as-if it doesn't trust
		//its own production algorithm!!!

		int sz = size();
		int top = sz-1;
		int bot = 0;
		int mid = top/2;		
		while(bot<=top)
		{
		//	if(d[mid]==val)
			{
				//index = mid; return true;
		//		break;
			}

			if(d[mid]<val)
			{
				bot = mid+1;
			}
			else top = mid-1;

			mid = (top+bot)/2;
		}		
		//return false;
		it = d+mid;
		if(mid<sz&&*it<val) it++;
	}
	else //SLOW??? (CONFIRMED)
	{
		it = d+(std::lower_bound(begin(),end(),val)-begin());
	}

	index = it-d; //2022
	return index<size()&&val==*it;
}

template<typename T> 
class sorted_ptr_list : public std::vector<T>
{
public:

	size_t insert_sorted(const T &val);
	void insert_sorted(const T &val, unsigned index);
	bool find_sorted(const T &val, unsigned &index)const;

	static bool less(T a, T b){ return *a<*b; };
	void sort(){ std::sort(begin(),end(),less); }
};
 
template<typename T> 
size_t sorted_ptr_list<T>::insert_sorted(const T &val)
{
	if(!empty()&&*val<*back())
	{
		/*for(auto it=begin();it!=end();it++)
		{
			if(*val<**it) break;
		}*/
		auto it = std::lower_bound(begin(),end(),val,less);
		return insert(it,val)-begin();
	}
	else push_back(val); return size()-1;
}
template<typename T> 
void sorted_ptr_list<T>::insert_sorted(const T &val, unsigned index)
{
	#ifdef _DEBUG
	if(!empty())
	{
		auto *d = data();
		auto &bf = d[index-1], &af = d[index];
		assert(index==0||*bf<*val||!(*val<*bf));
		assert(index<=size());
		assert(index==size()||*val<*af||!(*af<*val));
	}
	else assert(index==0);	
	#endif

	insert(begin()+index,val);
}

template<typename T> 
bool sorted_ptr_list<T>::find_sorted(const T &val, unsigned &index)const
{
	//(*this)[mid] is slow in debug builds (STL oob-checks)
	const T *it,*d = data();

	if(1) 
	{
		//See sorted_list notes on this.
		//Note, KeyframeList is the only
		//user of sorted_ptr_list, so this
		//is no big deal.

		int sz = size();
		int top = sz-1; 
		int bot = 0;
		int mid = top/2;		
		while(bot<=top)
		{
		//	if(*d[mid]==*val)
			{
				//index = mid; return true;
		//		break;
			}

			if(*d[mid]<*val)
			{
				bot = mid+1; 
			}
			else top = mid-1;

			mid = (top+bot)/2; 
		}
		//return false;
		it = d+mid;
		if(mid<sz&&**it<*val) it++;
	}
	else //SLOW??? (CONFIRMED)
	{
		it = d+(std::lower_bound(begin(),end(),val,less)-begin());
	}

	index = it-d; //2022
	return index<size()&&*val==**it;	
}

#endif // __SORTED_LIST_H
