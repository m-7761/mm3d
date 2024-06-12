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

#include "helpwin.h"

#include "version.h"
#include "sysconf.h"

struct AboutWin : Win
{	
	AboutWin(const char *title)
	    :
	Win(title),html(main,"",false),ok(main)
	{
		//Off-white border on bottom.
		html.style(~0x08000000).space<bottom>(3); 
		main->space(0,0,0,0,6);
		ok.calign().activate();
	}

	ui::html html; ok_button ok;
};

static void aboutwin_a(std::string &s, utf8 p, utf8 u)
{
	s.append("<a href='");
	s.append(p).append(u).append("'>");
	s.append(p).append(u).append("</a><br>");
}
static std::string aboutwin_http(utf8 u)
{
	std::string s; aboutwin_a(s,"http://",u); return s;
}
static std::string aboutwin_file(utf8 u)
{
	std::string s; aboutwin_a(s,"file://",u); return s;
}

extern void aboutwin(int id)
{
	if(id==id_about)
	{
		auto mp = aboutwin_http("github.com/mick-p1982/mm3d");
		aboutwin_a(mp,"http://","sourceforge.net/p/daedalus-3d");
		aboutwin_a(mp,"http://","sourceforge.net/p/widgets-95");

		auto zm = aboutwin_http("clover.moe/mm3d");
		auto kw = aboutwin_http("www.misfitcode.com/misfitmodel3d");

		auto cf = aboutwin_file(config->locate().c_str());
		auto kc = aboutwin_file(keycfg->locate().c_str());

		//The Help title is "MM3D Help - Contents".
		//auto w = new AboutWin(::tr("MM3D-About"));
		auto w = new AboutWin(::tr("About MM3D"));
		w->html.measure(320,170);
		w->html.set_text(w->html.text().format
		("<html>"
		"<head><title>MM3D-About</title></head> "
		"<body><center><br>"
		"<br>" //NEW
		"<h1>Multimedia 3D</h1><br>"
		"%s<br>"
		"Copyright &copy; 2019-2024 Mick Pearson<br><br>"
		"<h1>Maverick Model 3D</h1>"
		"<h2>" VERSION_STRING "</h2><br>"
		"%s<br>"
		"Copyright &copy; 2009-2019 Zack Middleton<br><br>"
		"<h1>Misfit Model 3D</h1>"		
		"%s<br>"
		"Copyright &copy; 2004-2008 Kevin Worcester<br><br>"
		"<h1>Configuration Files</h1><br>"
		"%s%s<br>"
		"<i>Tip: New versions of MM3D have new default hotkeys "
		"that require keycfg.ini to be purged of old defaults. "
		"Quit/restart to take effect."
		"</center></body></html>",mp.c_str(),
		zm.c_str(),kw.c_str(),cf.c_str(),kc.c_str()));
	}
	else if(id==id_license)
	{
		//(*new AboutWin(::tr("GNU General Public License")))
		//.html.set_url(std::string(getDocDirectory())+="/olh_license.html")
		//.measure(/*600*/500,400); //Auto-fit.
		auto *w = new HelpWin("license");
		w->html.measure(500,400); w->ok.activate();
	}
	else if(id==id_help) new HelpWin;
}