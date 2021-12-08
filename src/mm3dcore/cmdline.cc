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

#include "copyright.h" //MM3D_PRODUCT

#include "pluginmgr.h"
#include "cmdline.h"
#include "cmdlinemgr.h"
#include "version.h"
//#include "modelutil.h"
#include "luascript.h"
#include "luaif.h"
#include "filtermgr.h"
#include "misc.h"
#include "msg.h"
#include "log.h"
#include "translate.h"
//#include "mlocale.h"
//#include "texturetest.h"
#include "texmgr.h"

bool cmdline_runcommand = false;
bool cmdline_runui = true;

static bool cmdline_doConvert = false;
static std::string cmdline_convertFormat = "";

static bool cmdline_doBatch = false;

static bool cmdline_doScripts = false;
static bool cmdline_doTextureTest = false;

typedef std::list<std::string> StringList;
static StringList cmdline_scripts;
static StringList cmdline_argList;

typedef std::vector<Model*> cmdline_ModelList;
static cmdline_ModelList cmdline_models;

static void cmdline_print_version()
{
	//printf("\nMaverick Model 3D, version %s\n\n",VERSION_STRING);
	puts("\n" MM3D_PRODUCT ", version " VERSION_STRING "\n\n");
}

static void cmdline_print_help(const char *progname)
{
	cmdline_print_version();

	printf("Usage:\n  %s [options] [model_file] ...\n\n",progname);

	printf("Options:\n");
	printf("  -h  --help				 Print command line help and exit\n");
	printf("  -v  --version			 Print version information and exit\n");
	printf("  --verbose			 Passed to platform, e.g. wxWidgets\n");
#ifdef HAVE_LUALIB
	printf("		--script [file]	 Run script [file] on models\n");
#endif // HAVE_LUALIB
	printf("		--convert [format] Save models to format [format]\n");
	printf("								 \n");
	printf("		--language [code]  Use language [code] instead of system default\n");
	printf("								 \n");
	printf("		--no-plugins		 Disable all plugins\n");
	printf("		--no-plugin [foo]  Disable plugin [foo]\n");
	printf("								 \n");
	printf("		--sysinfo			 Display system information (for bug reports)\n");
	printf("		--debug				Display debug messages in console\n");
	printf("		--warnings			Display warning messages in console\n");
	printf("		--errors			  Display error messages in console\n");
	printf("		--no-debug			Do not display debug messages in console\n");
	printf("		--no-warnings		Do not display warning messages in console\n");
	printf("		--no-errors		  Do not display error messages in console\n");
	printf("\n");

	exit(0);
}

static void cmdline_print_sysinfo()
{
	//printf("\nMaverick Model 3D,version %s\n\n",VERSION_STRING);
	cmdline_print_version();

#ifdef _WIN32
	// TODO: Get Windows version
#else
	FILE *fp = nullptr;
	char input[80];

	printf("uname output:\n");
	fp = popen("uname -a","r");
	if(fp)
	{
		while(fgets(input,sizeof(input),fp))
		{
			printf("%s",input);
		}
		fclose(fp);
	}
	else
	{
		printf("error: uname: %s\n",strerror(errno));
	}

	printf("\n/etc/issue:\n");
	fp = popen("cat /etc/issue","r");
	if(fp)
	{
		while(fgets(input,sizeof(input),fp))
		{
			printf("%s",input);
		}
		fclose(fp);
	}
	else
	{
		printf("error: cat /etc/issue: %s\n",strerror(errno));
	}

	printf("\n");
#endif
}

enum cmdline_Mm3dOptionsE 
{
	OptHelp,
	OptVersion,
	OptBatch,
	OptNoPlugins,
	OptNoPlugin,
	OptConvert,
	OptLanguage,
	OptScript,
	OptSysinfo,
	OptDebug,
	OptWarnings,
	OptErrors,
	OptNoDebug,
	OptNoWarnings,
	OptNoErrors,
	OptTestTextureCompare,

	OptVerbose, //NEW
	OptMAX
};

int init_cmdline(int &argc,char *argv[])
{
	CommandLineManager clm;

	clm.addOption(OptHelp,'h',"help");
	clm.addOption(OptVersion,'v',"version");
	clm.addOption(OptVerbose,0,"verbose"); //NEW
	clm.addOption(OptBatch,'b',"batch");

	clm.addOption(OptNoPlugins,0,"no-plugins");
	clm.addOption(OptNoPlugin,0,"no-plugin",nullptr,true);
	clm.addOption(OptConvert,0,"convert",nullptr,true);
	clm.addOption(OptLanguage,0,"language",nullptr,true);
	clm.addOption(OptScript,0,"script",nullptr,true);

	clm.addOption(OptSysinfo,0,"sysinfo");
	clm.addOption(OptDebug,0,"debug");
	clm.addOption(OptWarnings,0,"warnings");
	clm.addOption(OptErrors,0,"errors");
	clm.addOption(OptNoDebug,0,"no-debug");
	clm.addOption(OptNoWarnings,0,"no-warnings");
	clm.addOption(OptNoErrors,0,"no-errors");

	clm.addOption(OptTestTextureCompare,0,"testtexcompare");

	if(!clm.parse(argc,(const char **)argv))
	{
		const char *opt = argv[clm.errorArgument()];

		switch (clm.error())
		{
			case CommandLineManager::MissingArgument:
				fprintf(stderr,"Option '%s' requires an argument. "
									  "See --help for details.\n",opt);
				break;
			case CommandLineManager::UnknownOption:
				fprintf(stderr,"Unknown option '%s'. "
									  "See --help for details.\n",opt);
				break;
			case CommandLineManager::NoError:
				fprintf(stderr,"BUG: CommandLineManager::parse returned false but "
									  "error code was not set.\n");
				break;
			default:
				fprintf(stderr,"BUG: CommandLineManager::error returned an "
									  "unknown error code.\n");
				break;
		}
		exit(-1);
	}

	if(clm.isSpecified(OptHelp))
		cmdline_print_help(argv[0]);

	if(clm.isSpecified(OptVersion))
	{
		cmdline_print_version();
		exit(0);
	}

	if(clm.isSpecified(OptBatch))
	{
		cmdline_doBatch = true;
		cmdline_runcommand = true;
	}

	if(clm.isSpecified(OptNoPlugins))
		PluginManager::getInstance()->setInitializeAll(false);
	if(clm.isSpecified(OptNoPlugin))
		PluginManager::getInstance()->disable(clm.stringValue(OptNoPlugin));
	if(clm.isSpecified(OptConvert))
	{
		cmdline_convertFormat = clm.stringValue(OptConvert);

		cmdline_doConvert = true;

		cmdline_runcommand = true;
		cmdline_runui = false;
	}
	if(clm.isSpecified(OptLanguage))
		mlocale_set(clm.stringValue(OptLanguage));

	if(clm.isSpecified(OptScript))
	{
		cmdline_scripts.push_back(clm.stringValue(OptScript));

		cmdline_doScripts = true;

		cmdline_runcommand = true;
		cmdline_runui = true;
	}
		
	if(clm.isSpecified(OptSysinfo))
	{
		cmdline_print_sysinfo();
		exit(0);
	}
		
	bool verbose = //2021
		clm.isSpecified(OptVerbose);
	if(verbose) 
		log_enable_output(true);
	if(verbose)
		cmdline_print_sysinfo();

	if(clm.isSpecified(OptDebug))
		log_enable_debug(true);
	if(clm.isSpecified(OptWarnings))
		log_enable_warning(true);
	if(clm.isSpecified(OptErrors))
		log_enable_error(true);
	if(clm.isSpecified(OptNoDebug))
		log_enable_debug(false);
	if(clm.isSpecified(OptNoWarnings))
		log_enable_warning(false);
	if(clm.isSpecified(OptNoErrors))
		log_enable_error(false);

	if(clm.isSpecified(OptTestTextureCompare))
	{
		cmdline_doTextureTest = true;

		cmdline_runcommand = true;
		cmdline_runui = false;
	}

	int opts_done = clm.firstArgument();
	int offset = 1;

	for(int n = opts_done; n<argc; n++)
	{
		cmdline_argList.push_back(argv[n]);
		argv[offset] = argv[n];
		++offset;
	}

	argc = offset;

	return 0;
}

void shutdown_cmdline()
{
	cmdline_deleteOpenModels();
}

int cmdline_command()
{
	unsigned errors = 0;

	if(cmdline_doTextureTest)
	{
		std::string master = cmdline_argList.front();

		StringList::iterator it = cmdline_argList.begin();
		it++;
		for(; it!=cmdline_argList.end(); it++)
		{
			texture_test_compare(master.c_str(),it->c_str(),10);
		}
		return 0;
	}

	FilterManager *mgr = FilterManager::getInstance();

	StringList::iterator it = cmdline_argList.begin();
	for(; it!=cmdline_argList.end(); it++)
	{
		Model::ModelErrorE err = Model::ERROR_NONE;
		Model *m = new Model;
		if((err = mgr->readFile(m,it->c_str()))==Model::ERROR_NONE)
		{
			m->loadTextures(0); //??? FIX ME (Doesn't belong here.)
			cmdline_models.push_back(m);
		}
		else
		{
			//FIX ME
			//Should errors++; be here? (See below msg_error cases.)

			msg_error("%s: %s",it->c_str(),transll(Model::errorToString(err,m)));
			delete m;
		}
	}
	
	if(cmdline_argList.empty()&&cmdline_doScripts)
	{
		Model *m = new Model();
		cmdline_models.push_back(m);
	}

	for(unsigned n = 0; n<cmdline_models.size(); n++)
	{
		Model *m = cmdline_models[n];
		if(cmdline_doScripts)
		{
#ifdef HAVE_LUALIB
			LuaScript lua;
			LuaContext lc(m);
			luaif_registerfunctions(&lua,&lc);
			StringList::iterator it;
			for(it = cmdline_scripts.begin(); it!=cmdline_scripts.end(); it++)
			{
				if(lua.runFile((*it).c_str())!=0)
				{
					errors++;
				}
			}
#else
			fprintf(stderr,"scripts disabled at compile time\n");
#endif // HAVE_LUALIB

		}

		if(cmdline_doConvert)
		{
			const char *infile = m->getFilename();
			std::string outfile = replaceExtension(infile,cmdline_convertFormat.c_str());
			Model::ModelErrorE err = Model::ERROR_NONE;
			if((err = mgr->writeFile(m,outfile.c_str(),true,FilterManager::WO_ModelNoPrompt))!=Model::ERROR_NONE)
			{
				errors++;
				msg_error("%s: %s",outfile.c_str(),transll(Model::errorToString(err,m)));
			}
		}

		if(cmdline_doBatch)
		{
			cmdline_runui = false;
			std::string filename = m->getFilename();
			if(filename.size()==0)
			{
				filename = "unnamed.mm3d";
			}

			Model::ModelErrorE err = Model::ERROR_NONE;
			if((err = mgr->writeFile(m,filename.c_str(),true,FilterManager::WO_ModelNoPrompt))!=Model::ERROR_NONE)
			{
				errors++;
				msg_error("%s: %s",filename.c_str(),transll(Model::errorToString(err,m)));
			}
		}
	}

	return errors;
}

extern int cmdline_getOpenModelCount()
{
	return cmdline_models.size();
}

extern Model *cmdline_getOpenModel(int n)
{
	return cmdline_models[n];
}

extern void cmdline_clearOpenModelList()
{
	cmdline_models.clear();
}

extern void cmdline_deleteOpenModels()
{
	unsigned t = 0;
	unsigned count = cmdline_models.size();
	for(t = 0; t<count; t++)
	{
		delete cmdline_models[t];
	}
	cmdline_models.clear();
}

//TRANSPLANTED FROM (libmm3d) mlocal.h/cc
static std::string s_locale = "";
void mlocale_set(const char *l){ s_locale = l; }
const char *mlocale_get(){ return s_locale.c_str(); }

//TRANSPLANTED FROM (mm3dcore) texturetest.h/cc
extern void texture_test_compare(const char *f1, const char *f2, unsigned fuzzyValue)
{
	TextureManager *texmgr = TextureManager::getInstance();

	Texture *t1 = texmgr->getTexture(f1);
	if(t1==nullptr)
	{
		printf("could not open %s\n",f1);
		return;
	}
	Texture *t2 = texmgr->getTexture(f2);
	if(t2==nullptr)
	{
		printf("could not open %s\n",f2);
		return;
	}

	Texture::CompareResultT res;
	bool match = false;

	float fuzzyImage = 0.90f;
	bool pixelPerfect  = false;
	float fuzzyMatch	= 0.0f;
	int steps = 256 *3;
	for(int t = 0; t<=steps; t++)
	{
		float fuzzyValue = 1-(float)t/steps;
		match = Texture::compare(t1,t2,&res,t);
		if(res.comparable)
		{
			if(res.pixelCount==res.matchCount)
			{
				pixelPerfect = true;
			}

			if((float)res.fuzzyCount/res.pixelCount>=fuzzyImage)
			{
				fuzzyMatch = fuzzyValue;
				break;
			}
		}
		else break;
	}

	printf("%s and %s\n",t1->m_name.c_str(),t2->m_name.c_str());
	printf("  %c %.2f %.2f\n",match?'Y':'N',(float)res.matchCount/res.pixelCount,fuzzyMatch);
}
