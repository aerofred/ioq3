#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../sys/sys_local.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

/* Merge without replacing anything the player has installed or edited. */
static void IOS_CopyMissingFiles( NSString *source, NSString *destination, BOOL packsOnly )
{
	NSFileManager *manager = NSFileManager.defaultManager;
	NSDirectoryEnumerator *files = [manager enumeratorAtPath:source];

	for( NSString *relativePath in files )
	{
		NSString *sourcePath = [source stringByAppendingPathComponent:relativePath];
		NSString *destinationPath = [destination stringByAppendingPathComponent:relativePath];
		BOOL isDirectory = NO;
		NSError *error = nil;

		if( ![manager fileExistsAtPath:sourcePath isDirectory:&isDirectory] || isDirectory )
			continue;
		if( packsOnly && [relativePath.pathExtension caseInsensitiveCompare:@"pk3"] != NSOrderedSame )
			continue;
		if( [manager fileExistsAtPath:destinationPath] )
			continue;

		if( ![manager createDirectoryAtPath:destinationPath.stringByDeletingLastPathComponent
			withIntermediateDirectories:YES attributes:nil error:&error] ||
			![manager copyItemAtPath:sourcePath toPath:destinationPath error:&error] )
		{
			NSLog(@"Could not install %@: %@", destinationPath, error);
			Sys_Error( "Could not install game data in Documents: %s", error.localizedDescription.UTF8String );
		}
	}
}

char *IOS_DefaultHomePath( const char *legacyPath )
{
	static char documentsPath[MAX_OSPATH];

	if( !documentsPath[0] )
	{
		@autoreleasepool
		{
			NSFileManager *manager = NSFileManager.defaultManager;
			NSError *error = nil;
			NSURL *documents = [manager URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask
				appropriateForURL:nil create:YES error:&error];

			if( !documents )
				Sys_Error( "Could not open Documents: %s", error.localizedDescription.UTF8String );

			/* Preserve configs, downloads and mods from earlier iOS installations.
			 * Keep the old directory as a backup; only Documents is searched now. */
			NSString *migrationMarker = [documents.path stringByAppendingPathComponent:@".ioq3-documents-migrated"];
			if( legacyPath && legacyPath[0] && ![manager fileExistsAtPath:migrationMarker] )
			{
				IOS_CopyMissingFiles( [NSString stringWithUTF8String:legacyPath], documents.path, NO );
				if( ![@"1\n" writeToFile:migrationMarker atomically:YES encoding:NSUTF8StringEncoding error:&error] )
					Sys_Error( "Could not record Documents migration: %s", error.localizedDescription.UTF8String );
			}

			/* The bundle remains the fallback for QVMs and default configuration.
			 * Writable packs take precedence through fs_home*path. Missing bundled
			 * packs are restored at launch, but replacements are never overwritten. */
			IOS_CopyMissingFiles( NSBundle.mainBundle.resourcePath, documents.path, YES );
			if( ![manager createDirectoryAtPath:[documents.path stringByAppendingPathComponent:@BASEGAME]
				withIntermediateDirectories:YES attributes:nil error:&error] )
				Sys_Error( "Could not create game directory: %s", error.localizedDescription.UTF8String );
			Q_strncpyz( documentsPath, documents.path.fileSystemRepresentation, sizeof(documentsPath) );
		}
	}

	return documentsPath;
}

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
