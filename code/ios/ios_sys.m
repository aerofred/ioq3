#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../sys/sys_local.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title )
{
	const char *resolvedTitle = title ? title : "ioquake3";
	const char *resolvedMessage = message ? message : "";

	NSLog(@"%s: %s", resolvedTitle, resolvedMessage);

	switch(type)
	{
		case DT_YES_NO:
			return DR_NO;

		case DT_OK_CANCEL:
			return DR_CANCEL;

		case DT_ERROR:
		case DT_INFO:
		case DT_WARNING:
		default:
			return DR_OK;
	}
}

char *Sys_StripAppBundle(char *dir)
{
	static char cwd[MAX_OSPATH];
	char *appDir;

	if(!dir || !dir[0])
	{
		return dir;
	}

	Q_strncpyz(cwd, dir, sizeof(cwd));
	appDir = strstr(cwd, ".app/");

	if(appDir)
	{
		appDir[4] = '\0';
		return cwd;
	}

	appDir = strstr(cwd, ".app");

	if(appDir && appDir[4] == '\0')
	{
		return cwd;
	}

	return dir;
}
