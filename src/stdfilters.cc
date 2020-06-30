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
#include "filtermgr.h"
#include "log.h"

/*REMOVE US
#include "mm3dfilter.h"
#include "md2filter.h"
#include "md3filter.h"
//#include "lwofilter.h" //end-of-support?
//#include "cal3dfilter.h" //end-of-support?
//#include "cobfilter.h" //end-of-support?
//#include "dxffilter.h" //end-of-support?
#include "objfilter.h"
#include "ms3dfilter.h"
//#include "txtfilter.h" //end-of-support?
#include "iqefilter.h"
#include "smdfilter.h"
*/

extern void init_std_filters()
{
	log_debug("initializing standard filters\n");

	FilterManager *mgr = FilterManager::getInstance();

	typedef ModelFilter::PromptF prompt;
	typedef ModelFilter *filter(ModelFilter::PromptF);

	extern filter mm3dfilter;
	mgr->registerFilter(mm3dfilter(nullptr));
	
	extern filter ms3dfilter;
	extern prompt ms3dprompt;
	mgr->registerFilter(ms3dfilter(ms3dprompt));
	
	//extern filter textfilter;
	//mgr->registerFilter(textfilter(nullptr));

	extern filter objfilter;
	extern prompt objprompt;
	mgr->registerFilter(objfilter(objprompt));
	
	//extern filter lwofilter;
	//mgr->registerFilter(lwofilter(nullptr));

	extern filter md2filter;
	mgr->registerFilter(md2filter(nullptr));
	
	extern filter md3filter;
	mgr->registerFilter(md3filter(nullptr));
	
	//extern filter cal3dfilter;
	//extern prompt cal3dprompt;
	//mgr->registerFilter(cal3dfilter(cal3dprompt));
	
	//extern filter cobfilter;
	//mgr->registerFilter(cobfilter(nullptr));
	
	//extern filter dxffilter;
	//mgr->registerFilter(dxffilter(nullptr));

	extern filter iqefilter;
	extern prompt iqeprompt;
	mgr->registerFilter(iqefilter(iqeprompt));

	extern filter smdfilter;
	extern prompt smdprompt;
	mgr->registerFilter(smdfilter(smdprompt));
}

