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

#ifdef __APPLE__
#include <errno.h>
#include <sys/syslimits.h>
#include <mach-o/dyld.h> // _NSGetExecutablePath
#endif

#include "sysconf.h"
#include "mm3dconfig.h"
#include "mm3dport.h"
#include "log.h"
#include "version.h"

#ifdef _WIN32
static std::wstring s_mm3dHomeDir;
static std::wstring s_docDir;
static std::wstring s_i18nDir;
static std::wstring s_pluginDir;
static std::wstring s_sharedPluginDir;
static std::wstring s_configFile;
#else
static std::string s_mm3dHomeDir;
static std::string s_docDir;
static std::string s_i18nDir;
static std::string s_pluginDir;
static std::string s_sharedPluginDir;
static std::string s_configFile;
#endif

#ifdef _WIN32

// Gets the path to the current process's exe file
static std::wstring getExecutablePath()
{
	std::wstring rval = L"";
	wchar_t execpath[MAX_PATH];
	wchar_t execpath2[MAX_PATH];
	DWORD length;

	length = GetModuleFileNameW(nullptr,execpath2,sizeof(execpath));

	//2021: Visual Studio generated paths fail FindFirstFile
	//https://github.com/zturtleman/mm3d/issues/163
	if(PathCanonicalizeW(execpath,execpath2))
	length = wcslen(execpath);

	if(length>=sizeof(execpath))
	{
		//log_debug("getExecutablePath: Execuable path too long\n");
	}
	else if(length==0)
	{
		//log_debug("getExecutablePath: GetModuleFileNameA() failed: 0x%x\n",GetLastError());
	}
	else if(wchar_t*ptr=wcsrchr(execpath,'\\'))
	{
		*ptr = '\0'; rval = execpath;
	}

	return rval;
}

#elif defined(__APPLE__)

static std::string getExecutablePath()
{
	char actualpath[PATH_MAX];
	char tmppath[PATH_MAX];
	char *ptr;
	uint32_t size;

	size = sizeof(tmppath);

	if(0!=_NSGetExecutablePath(tmppath,&size))
	{
		//log_debug("getExecutablePath: _NSGetExecutablePath()failed: %s\n",strerror(errno));
		actualpath[0] = '.';
		actualpath[1] = '\0';
	}
	else if(!realpath(tmppath,actualpath))
	{
		//log_debug("getExecutablePath: realpath(%s)failed: %s\n",tmppath,strerror(errno));
		actualpath[0] = '.';
		actualpath[1] = '\0';
	}

	// Remove executable name and trailing directory separator
	if(ptr=strrchr(actualpath,'/')) *ptr = '\0';

	return actualpath;
}

static std::string getAppBundlePath()
{
	std::string path = getExecutablePath();
	std::string endsWith = ".app/Contents/MacOS";

	// Check if executable is not in an app bundle
	if(path.size()<endsWith.size()
	||0!=path.compare(path.size()-endsWith.size(),endsWith.size(),endsWith))
	return "";
	return path.substr(0,path.size()-endsWith.size()+strlen(".app"));
}

#endif // __APPLE__

void init_sysconf()
{
	/*char *majorMinor = strdup(VERSION);  
	int off = strlen(majorMinor)-1;
	while(off>0&&majorMinor[off]!='.')
	off--;
	majorMinor[off] = '\0';*/
	std::string majorMinor = VERSION; 
	majorMinor.erase(majorMinor.rfind('.'));

#ifdef _WIN32

	std::wstring path = getExecutablePath();

	if(path.size()==0)
	{
		// Fall back to current directory

		//UNTESTED?
		//char cwd[PATH_MAX]; //???
		//char cwd[MAX_PATH];

		wchar_t cwd[MAX_PATH];

		if(GetCurrentDirectoryW(MAX_PATH,cwd)>0)
		path = cwd;
		else path = L".";
	}

	wchar_t appdata[MAX_PATH];
	if(SHGetFolderPathW(nullptr,CSIDL_APPDATA|CSIDL_FLAG_CREATE,nullptr,0,appdata)==S_OK)
	{
		s_mm3dHomeDir  = appdata;
		//s_mm3dHomeDir += HOME_MM3D;
		for(const char&ea:HOME_MM3D)
		s_mm3dHomeDir += (wchar_t)ea;
	}
	else
	{
		s_mm3dHomeDir  = path;
		s_mm3dHomeDir += L"\\userhome";
	}

	for(const char&ea:HOME_PLUGINS)
	s_pluginDir+=(wchar_t)ea;
	s_pluginDir  += L"\\";
	for(const char&ea:majorMinor)
	s_pluginDir  += (wchar_t)ea;
	
	s_docDir = path;
	for(const char&ea:DOC_ROOT)
	s_docDir += ea;
	s_i18nDir = path;
	for(const char&ea:I18N_ROOT)
	s_i18nDir += ea;
	s_sharedPluginDir = path;
	for(const char&ea:SHARED_PLUGINS)
	s_sharedPluginDir+=ea;
	s_sharedPluginDir += L"\\";
	for(const char&ea:majorMinor)
	s_sharedPluginDir += ea;

#elif defined __APPLE__

	s_mm3dHomeDir = getenv("HOME");
	s_mm3dHomeDir += HOME_MM3D;
	s_pluginDir = s_mm3dHomeDir+HOME_PLUGINS;
	s_pluginDir += "/";
	s_pluginDir += majorMinor;

	std::string appPath = getAppBundlePath();

	if(appPath.size()>0){
		s_docDir = appPath+"/Contents/SharedSupport/mm3d/doc/html";
		s_i18nDir = appPath+"/Contents/SharedSupport/mm3d/i18n";
		s_sharedPluginDir = appPath+"/Contents/PlugIns/mm3d";
		s_sharedPluginDir += "/";
		s_sharedPluginDir += majorMinor;
	}
	else 
	{
		s_docDir = DOC_ROOT;
		s_i18nDir = I18N_ROOT;
		s_sharedPluginDir = SHARED_PLUGINS;
		s_sharedPluginDir += "/";
		s_sharedPluginDir += majorMinor;
	}

#else

	s_mm3dHomeDir = getenv("HOME");
	s_mm3dHomeDir += HOME_MM3D;
	s_docDir	 = DOC_ROOT;
	s_i18nDir	= I18N_ROOT;
	s_pluginDir = s_mm3dHomeDir+HOME_PLUGINS;
	s_pluginDir += "/";
	s_pluginDir += majorMinor;
	s_sharedPluginDir  = SHARED_PLUGINS;
	s_sharedPluginDir += "/";
	s_sharedPluginDir += majorMinor;

#endif

	#ifdef _WIN32 
	{
		s_configFile = s_mm3dHomeDir;
		for(const char&ea:HOME_RC)
		s_configFile+=(wchar_t)ea;
	}
	#else 
	{
		s_configFile = s_mm3dHomeDir+HOME_RC;
	}
	#endif

	/*
	//log_debug("mm3d home is %s\n",s_mm3dHomeDir.c_str());
	//log_debug("doc dir is %s\n",s_docDir.c_str());
	//log_debug("plugin dir is %s\n",s_pluginDir.c_str());
	//log_debug("shared plugin dir is %s\n",s_sharedPluginDir.c_str());
	//log_debug("config file is %s\n",s_configFile.c_str());
	*/

	//free(majorMinor);
}

#ifdef _WIN32
const std::string getConfigFile()
{
	int len = WideCharToMultiByte(CP_UTF8,0,s_configFile.c_str(),-1,0,0,0,0)-1;
	std::string u(len,'\0');
	WideCharToMultiByte(CP_UTF8,0,s_configFile.c_str(),-1,&u[0],len,0,0);
	return u;
}
#else
const std::string &getConfigFile()
{
	return s_configFile;
}
#endif

#ifdef _WIN32
const std::string getMm3dHomeDirectory()
{
	int len = WideCharToMultiByte(CP_UTF8,0,s_mm3dHomeDir.c_str(),-1,0,0,0,0)-1;
	std::string u(len,'\0');
	WideCharToMultiByte(CP_UTF8,0,s_mm3dHomeDir.c_str(),-1,&u[0],len,0,0);
	return u;
}
#else
const std::string &getMm3dHomeDirectory()
{
	return s_mm3dHomeDir;
}
#endif

#ifdef _WIN32
const std::string getDocDirectory()
{
	int len = WideCharToMultiByte(CP_UTF8,0,s_docDir.c_str(),-1,0,0,0,0)-1;
	std::string u(len,'\0');
	WideCharToMultiByte(CP_UTF8,0,s_docDir.c_str(),-1,&u[0],len,0,0);
	return u;
}
#else
const std::string &getDocDirectory()
{
	return s_docDir;
}
#endif

#ifdef _WIN32
const std::string getI18nDirectory()
{
	int len = WideCharToMultiByte(CP_UTF8,0,s_i18nDir.c_str(),-1,0,0,0,0)-1;
	std::string u(len,'\0');
	WideCharToMultiByte(CP_UTF8,0,s_i18nDir.c_str(),-1,&u[0],len,0,0);
	return u;
}
#else
const std::string &getI18nDirectory()
{
	return s_i18nDir;
}
#endif

#ifdef _WIN32
const std::string getPluginDirectory()
{
	int len = WideCharToMultiByte(CP_UTF8,0,s_pluginDir.c_str(),-1,0,0,0,0)-1;
	std::string u(len,'\0');
	WideCharToMultiByte(CP_UTF8,0,s_pluginDir.c_str(),-1,&u[0],len,0,0);
	return u;
}
#else
const std::string &getPluginDirectory()
{
	return s_pluginDir;
}
#endif

#ifdef _WIN32
const std::string getSharedPluginDirectory()
{
	int len = WideCharToMultiByte(CP_UTF8,0,s_sharedPluginDir.c_str(),-1,0,0,0,0)-1;
	std::string u(len,'\0');
	WideCharToMultiByte(CP_UTF8,0,s_sharedPluginDir.c_str(),-1,&u[0],len,0,0);
	return u;
}
#else
const std::string &getSharedPluginDirectory()
{
	return s_sharedPluginDir;
}
#endif

