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


#ifndef __CMDLINE_H
#define __CMDLINE_H

//TRANSPLANTED FROM mlocal.h/cc
const char *mlocale_get();
void mlocale_set(const char*);

int init_cmdline(int &argc,char *argv[]);
void shutdown_cmdline();

extern bool cmdline_runcommand;
extern bool cmdline_runui;
extern int cmdline_command();

extern bool cmdline_resume; //2022
extern int cmdline_getOpenModelCount();
extern class Model *cmdline_getOpenModel(int n);
extern void cmdline_clearOpenModelList();  // Clear list without freeing
extern void cmdline_deleteOpenModels();	 // Free models in list,then clear

//TRANSPLANTED FROM (mm3dcore) texturetest.h/cc
extern void texture_test_compare(const char *f1, const char *f2, unsigned fuzzyValue);

#endif // __CMDLINE_H
