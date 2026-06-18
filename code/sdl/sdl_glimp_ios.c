/*
===========================================================================
SDL2 + OpenGL ES 1.1 (fixed-function) video driver for iOS
Based on Quake3-iOS / ioquake3 GLES path
===========================================================================
*/
#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../renderercommon/tr_common.h"
#include "../qcommon/qcommon.h"
#include "../ios/ios_layer.h"
#include "../client/cl_touch.h"

#ifdef USE_GLES_FIXED
#include "../renderercommon/qgl_es1.h"
void GLimp_AssignES1DesktopStubs( void );
#endif

SDL_Window *SDL_window = NULL;
static SDL_GLContext glContext = NULL;
static int glimp_builtDisplayIndex = 0;
static qboolean glimp_everInited = qfalse;

/*
 * Target display index for the GL surface: when an external display is
 * connected (more than one SDL video display) the game renders there; the
 * device screen is reserved for the touch-control overlay.
 */
int GLimp_DesiredDisplayIndex( void )
{
	return SDL_GetNumVideoDisplays() > 1 ? 1 : 0;
}

/*
 * True when the GL window was built for a different display than the one we
 * now want (external screen connected/disconnected). The caller issues a
 * vid_restart, which destroys and recreates the window on the right screen.
 */
qboolean GLimp_DisplayRelocationNeeded( void )
{
	if ( !SDL_window )
		return qfalse;
	return GLimp_DesiredDisplayIndex() != glimp_builtDisplayIndex ? qtrue : qfalse;
}

/*
 * True when the GL surface is currently rendering on a non-primary (external)
 * display. SDL routes mouse events to its on-screen view, so while the window
 * lives on the external screen we read the physical mouse ourselves instead.
 */
qboolean GLimp_RenderingOnExternalDisplay( void )
{
	return ( SDL_window && glimp_builtDisplayIndex != 0 ) ? qtrue : qfalse;
}

cvar_t *r_allowSoftwareGL;
cvar_t *r_allowResize;
cvar_t *r_centerWindow;
cvar_t *r_sdlDriver;

int qglMajorVersion, qglMinorVersion;
int qglesMajorVersion, qglesMinorVersion;

void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);
void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);

#define GLE(ret, name, ...) name##proc * qgl##name;
QGL_1_1_PROCS;
QGL_1_1_FIXED_FUNCTION_PROCS;
QGL_DESKTOP_1_1_PROCS;
QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
QGL_ES_1_1_PROCS;
QGL_ES_1_1_FIXED_FUNCTION_PROCS;
QGL_1_3_PROCS;
QGL_1_5_PROCS;
QGL_2_0_PROCS;
QGL_3_0_PROCS;
QGL_ARB_occlusion_query_PROCS;
QGL_ARB_framebuffer_object_PROCS;
QGL_ARB_vertex_array_object_PROCS;
QGL_EXT_direct_state_access_PROCS;
#undef GLE

/* iOS headers can prevent the GLE macro from emitting qglColor4ub storage. */
Color4ubproc *qglColor4ub;

typedef enum {
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_UNKNOWN
} rserr_t;

static void APIENTRY GLimp_GLES_ClearDepth( GLclampd depth ) {
	qglClearDepthf( depth );
}

static void APIENTRY GLimp_GLES_ClipPlane( GLenum plane, const GLdouble *equation ) {
	GLfloat values[4];
	values[0] = equation[0];
	values[1] = equation[1];
	values[2] = equation[2];
	values[3] = equation[3];
	qglClipPlanef( plane, values );
}

static void APIENTRY GLimp_GLES_Color3f( GLfloat red, GLfloat green, GLfloat blue ) {
	qglColor4f( red, green, blue, 1.0f );
}

static void APIENTRY GLimp_GLES_Color4ubv( const GLubyte *v ) {
	if ( qglColor4ub ) {
		qglColor4ub( v[0], v[1], v[2], v[3] );
	} else {
		qglColor4f( v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f, v[3] / 255.0f );
	}
}

static void APIENTRY GLimp_GLES_DepthRange( GLclampd near_val, GLclampd far_val ) {
	qglDepthRangef( near_val, far_val );
}

static void APIENTRY GLimp_GLES_DrawBuffer( GLenum mode ) {
	(void)mode;
}

static void APIENTRY GLimp_GLES_Frustum( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val ) {
	qglFrustumf( left, right, bottom, top, near_val, far_val );
}

static void APIENTRY GLimp_GLES_Ortho( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val ) {
	qglOrthof( left, right, bottom, top, near_val, far_val );
}

static void APIENTRY GLimp_GLES_PolygonMode( GLenum face, GLenum mode ) {
	(void)face;
	(void)mode;
}

static void GLimp_ClearProcAddresses( void ) {
#define GLE( ret, name, ... ) qgl##name = NULL;

	qglMajorVersion = 0;
	qglMinorVersion = 0;
	qglesMajorVersion = 0;
	qglesMinorVersion = 0;

	QGL_1_1_PROCS;
	QGL_1_1_FIXED_FUNCTION_PROCS;
	QGL_DESKTOP_1_1_PROCS;
	QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
	QGL_ES_1_1_PROCS;
	QGL_ES_1_1_FIXED_FUNCTION_PROCS;
	QGL_1_3_PROCS;
	QGL_1_5_PROCS;
	QGL_2_0_PROCS;
	QGL_3_0_PROCS;
	QGL_ARB_occlusion_query_PROCS;
	QGL_ARB_framebuffer_object_PROCS;
	QGL_ARB_vertex_array_object_PROCS;
	QGL_EXT_direct_state_access_PROCS;

	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	qglMultiTexCoord2fARB = NULL;
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;

#undef GLE
}

static qboolean GLimp_GetProcAddresses( void ) {
	qboolean success = qtrue;
	const char *version;

#ifdef __SDL_NOGETPROCADDR__
#define GLE( ret, name, ... ) qgl##name = gl##name;
#else
#define GLE( ret, name, ... ) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name); \
	if ( qgl##name == NULL ) { \
		ri.Printf( PRINT_ALL, "ERROR: Missing OpenGL function %s\n", "gl" #name ); \
		success = qfalse; \
	}
#endif

	GLE(const GLubyte *, GetString, GLenum name)

	if ( !qglGetString ) {
		ri.Error( ERR_FATAL, "glGetString is NULL" );
	}

	version = (const char *)qglGetString( GL_VERSION );
	if ( !version ) {
		ri.Error( ERR_FATAL, "GL_VERSION is NULL" );
	}

	if ( Q_stricmpn( "OpenGL ES", version, 9 ) == 0 ) {
		char profile[6];
		sscanf( version, "OpenGL %5s %d.%d", profile, &qglesMajorVersion, &qglesMinorVersion );
		if ( Q_stricmp( profile, "ES-CL" ) == 0 ) {
			qglesMajorVersion = 0;
			qglesMinorVersion = 0;
		}
	} else {
		sscanf( version, "%d.%d", &qglMajorVersion, &qglMinorVersion );
	}

	if ( qglesMajorVersion == 1 && qglesMinorVersion >= 1 ) {
		QGL_1_1_PROCS;
		QGL_1_1_FIXED_FUNCTION_PROCS;
		QGL_ES_1_1_PROCS;
		QGL_ES_1_1_FIXED_FUNCTION_PROCS;

		if ( !qglColor4ub ) {
			qglColor4ub = (Color4ubproc *)SDL_GL_GetProcAddress( "glColor4ub" );
		}
		if ( !qglColor4ub ) {
			qglColor4ub = (Color4ubproc *)glColor4ub;
		}

		qglActiveTextureARB = (void (APIENTRY *)(GLenum)) SDL_GL_GetProcAddress( "glActiveTexture" );
		qglClientActiveTextureARB = (void (APIENTRY *)(GLenum)) SDL_GL_GetProcAddress( "glClientActiveTexture" );
		if ( !qglActiveTextureARB ) {
			qglActiveTextureARB = (void (APIENTRY *)(GLenum)) glActiveTexture;
		}
		if ( !qglClientActiveTextureARB ) {
			qglClientActiveTextureARB = (void (APIENTRY *)(GLenum)) glClientActiveTexture;
		}
		glConfig.numTextureUnits = 2;

		qglClearDepth = GLimp_GLES_ClearDepth;
		qglClipPlane = GLimp_GLES_ClipPlane;
		qglColor3f = GLimp_GLES_Color3f;
		qglColor4ubv = GLimp_GLES_Color4ubv;
		qglDepthRange = GLimp_GLES_DepthRange;
		qglDrawBuffer = GLimp_GLES_DrawBuffer;
		qglFrustum = GLimp_GLES_Frustum;
		qglOrtho = GLimp_GLES_Ortho;
		qglPolygonMode = GLimp_GLES_PolygonMode;
	} else {
		ri.Error( ERR_FATAL, "Unsupported OpenGL Version (%s), OpenGL ES 1.1 is required", version );
	}

#undef GLE
	return success;
}

static void GLimp_SyncIOSLayer( void )
{
	int pointsW = 0;
	int pointsH = 0;
	int drawableW = 0;
	int drawableH = 0;
	float scale = 1.0f;

	if ( !SDL_window ) {
		return;
	}

	SDL_GetWindowSize( SDL_window, &pointsW, &pointsH );
	SDL_GL_GetDrawableSize( SDL_window, &drawableW, &drawableH );
	if ( pointsW > 0 ) {
	 scale = (float)drawableW / (float)pointsW;
	}

	if ( drawableW > 0 && drawableH > 0 ) {
		Cvar_Set( "r_customwidth", va( "%d", drawableW ) );
		Cvar_Set( "r_customheight", va( "%d", drawableH ) );
	}

	IOS_Layer_SyncScreen( drawableW, drawableH, scale );
	IN_TouchSyncLayout( drawableW, drawableH, scale );
}

static rserr_t GLimp_SetMode( int mode, qboolean fullscreen, qboolean noborder )
{
	int width, height;
	int modeWidth, modeHeight;
	int windowWidth, windowHeight;
	int displayIndex;
	Uint32 flags;
	const char *glstring;
	SDL_DisplayMode desktopMode;

	(void)noborder;
	ri.Printf( PRINT_ALL, "Initializing OpenGL ES 1.1 display (SDL2)\n" );

	/* Always create the very first window on the built-in display so the device
	 * scene becomes the app's primary (input-receiving) scene. If an external
	 * display is present, the hotplug check then relocates the GL surface to it
	 * via vid_restart, which is the path that keeps device input working. */
	displayIndex = glimp_everInited ? GLimp_DesiredDisplayIndex() : 0;
	glimp_everInited = qtrue;

	modeWidth = 0;
	modeHeight = 0;

	if ( mode == -2 )
	{
		if ( SDL_window )
		{
			SDL_GL_GetDrawableSize( SDL_window, &width, &height );
		}
		else if ( SDL_GetDesktopDisplayMode( 0, &desktopMode ) == 0 )
		{
			width = desktopMode.w;
			height = desktopMode.h;
		}
		else
		{
			width = 1024;
			height = 768;
		}
		displayAspect = (float)width / (float)height;
	}
	else if ( R_GetModeInfo( &modeWidth, &modeHeight, &displayAspect, mode ) )
	{
		width = modeWidth;
		height = modeHeight;
	}
	else
	{
		width = 1024;
		height = 768;
		displayAspect = 4.0f / 3.0f;
	}

	/* UIKit plein écran : surface native, ratio selon r_mode */
	fullscreen = qtrue;
	glConfig.isFullscreen = qtrue;

	if ( SDL_GetDesktopDisplayMode( displayIndex, &desktopMode ) == 0 )
	{
		windowWidth = desktopMode.w;
		windowHeight = desktopMode.h;
	}
	else
	{
		windowWidth = width;
		windowHeight = height;
	}

	flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI |
		SDL_WINDOW_FULLSCREEN_DESKTOP;

	if ( glContext )
	{
		GLimp_ClearProcAddresses();
		SDL_GL_DeleteContext( glContext );
		glContext = NULL;
	}

	if ( SDL_window )
	{
		SDL_SetWindowFullscreen( SDL_window, SDL_WINDOW_FULLSCREEN_DESKTOP );
		SDL_GL_GetDrawableSize( SDL_window, &width, &height );
		glConfig.vidWidth = width;
		glConfig.vidHeight = height;
		glConfig.windowAspect = (float)width / (float)height;
	}
	else
	{
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 1 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );

		SDL_window = SDL_CreateWindow( CLIENT_WINDOW_TITLE,
			SDL_WINDOWPOS_CENTERED_DISPLAY( displayIndex ),
			SDL_WINDOWPOS_CENTERED_DISPLAY( displayIndex ), windowWidth, windowHeight, flags );
		if ( !SDL_window )
		{
			ri.Printf( PRINT_ALL, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
			return RSERR_INVALID_MODE;
		}

		glimp_builtDisplayIndex = displayIndex;
		ri.Printf( PRINT_ALL, "GL surface on display %d of %d\n",
			displayIndex, SDL_GetNumVideoDisplays() );

		glContext = SDL_GL_CreateContext( SDL_window );
		if ( !glContext )
		{
			ri.Printf( PRINT_ALL, "SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
			return RSERR_INVALID_MODE;
		}

		SDL_GL_SetSwapInterval( 1 );

		SDL_GL_GetDrawableSize( SDL_window, &width, &height );
		glConfig.vidWidth = width;
		glConfig.vidHeight = height;
		glConfig.windowAspect = (float)width / (float)height;
	}

	SDL_GL_MakeCurrent( SDL_window, glContext );

	if ( !GLimp_GetProcAddresses() )
		ri.Error( ERR_FATAL, "GLimp_GetProcAddresses failed" );

	glConfig.colorBits = 24;
	glConfig.depthBits = 24;
	glConfig.stencilBits = 8;

	glstring = (const char *)qglGetString( GL_RENDERER );
	ri.Printf( PRINT_ALL, "GL_RENDERER: %s\n", glstring ? glstring : "unknown" );
	ri.Printf( PRINT_ALL, "GL_VERSION: %s\n", (char *)qglGetString( GL_VERSION ) );

	GLimp_SyncIOSLayer();

	return RSERR_OK;
}

static void GLimp_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	glConfig.textureCompression = TC_NONE;
	glConfig.textureEnvAddAvailable = qfalse;

	if ( QGLES_VERSION_ATLEAST( 1, 0 ) || SDL_GL_ExtensionSupported( "GL_EXT_texture_env_add" ) )
	{
		if ( r_ext_texture_env_add->integer )
			glConfig.textureEnvAddAvailable = qtrue;
	}

	if ( QGLES_VERSION_ATLEAST( 1, 0 ) || SDL_GL_ExtensionSupported( "GL_ARB_multitexture" ) )
	{
		if ( QGLES_VERSION_ATLEAST( 1, 0 ) )
		{
			if ( !qglActiveTextureARB ) {
				qglActiveTextureARB = (void (APIENTRY *)(GLenum)) SDL_GL_GetProcAddress( "glActiveTexture" );
			}
			if ( !qglClientActiveTextureARB ) {
				qglClientActiveTextureARB = (void (APIENTRY *)(GLenum)) SDL_GL_GetProcAddress( "glClientActiveTexture" );
			}
		}
		else if ( r_ext_multitexture->value )
		{
			qglMultiTexCoord2fARB = (void (APIENTRY *)(GLenum, GLfloat, GLfloat)) SDL_GL_GetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = (void (APIENTRY *)(GLenum)) SDL_GL_GetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = (void (APIENTRY *)(GLenum)) SDL_GL_GetProcAddress( "glClientActiveTextureARB" );
		}

		if ( qglActiveTextureARB )
		{
			GLint glint = 0;
			qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glint );
			glConfig.numTextureUnits = (int)glint;
			if ( glConfig.numTextureUnits < 2 )
			{
				glConfig.numTextureUnits = 2;
			}
		}
	}

	if ( glConfig.numTextureUnits < 1 )
		glConfig.numTextureUnits = 2;

	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;

	textureFilterAnisotropic = qfalse;
	if ( SDL_GL_ExtensionSupported( "GL_EXT_texture_filter_anisotropic" ) )
	{
		if ( r_ext_texture_filter_anisotropic->integer )
		{
			qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&maxAnisotropy );
			if ( maxAnisotropy > 0 )
				textureFilterAnisotropic = qtrue;
		}
	}

	Q_strncpyz( glConfig.extensions_string,
		"GL_ARB_multitexture GL_EXT_texture_env_add",
		sizeof( glConfig.extensions_string ) );

#ifdef USE_GLES_FIXED
	GLimp_AssignES1DesktopStubs();
#endif
}

void GLimp_Init( qboolean fixedFunction )
{
	(void)fixedFunction;
	r_allowSoftwareGL = ri.Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	r_sdlDriver = ri.Cvar_Get( "r_sdlDriver", "ios", CVAR_ROM );
	r_allowResize = ri.Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_centerWindow = ri.Cvar_Get( "r_centerWindow", "0", CVAR_ARCHIVE | CVAR_LATCH );

	SDL_SetHint( "SDL_VIDEO_HIGHDPI_DISABLED", "0" );
	SDL_SetHint( "SDL_IOS_HIDE_HOME_INDICATOR", "2" );

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS ) < 0 )
			ri.Error( ERR_FATAL, "SDL_Init failed: %s", SDL_GetError() );
		ri.Cvar_Set( "r_sdlDriver", "SDL2" );
	}

	if ( GLimp_SetMode( r_mode->integer, r_fullscreen->integer, r_noborder->integer ) != RSERR_OK )
		ri.Error( ERR_FATAL, "GLimp_Init: could not set video mode" );

	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;

	/* Comme Quake3-iOS : détecter le gamma matériel via SDL */
	glConfig.deviceSupportsGamma = !Cvar_VariableIntegerValue( "r_ignorehwgamma" ) &&
		SDL_window && SDL_SetWindowBrightness( SDL_window, 1.0f ) >= 0;

	Q_strncpyz( glConfig.vendor_string, (char *)qglGetString( GL_VENDOR ),
		sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (char *)qglGetString( GL_RENDERER ),
		sizeof( glConfig.renderer_string ) );
	Q_strncpyz( glConfig.version_string, (char *)qglGetString( GL_VERSION ),
		sizeof( glConfig.version_string ) );

	GLimp_InitExtensions();

#ifdef USE_GLES_FIXED
	GLimp_AssignES1DesktopStubs();
#endif

	/* Ancien contournement iOS (1.35) : revenir aux valeurs moteur par défaut */
	if ( Cvar_VariableValue( "r_gamma" ) == 1.35f )
		Cvar_Set( "r_gamma", "1" );
	if ( Cvar_VariableValue( "r_intensity" ) == 1.35f )
		Cvar_Set( "r_intensity", "1" );

	IOS_Layer_Init();
	ri.IN_Init( SDL_window );
	GLimp_SyncIOSLayer();
}

void GLimp_GetWindowSize( int *width, int *height )
{
	iosLayout_t layout;

	if ( width ) {
		*width = 0;
	}
	if ( height ) {
		*height = 0;
	}

	IOS_Layer_GetLayout( &layout );
	if ( layout.width > 0 && layout.height > 0 ) {
		if ( width ) {
			*width = (int)layout.width;
		}
		if ( height ) {
			*height = (int)layout.height;
		}
		return;
	}

	if ( SDL_window ) {
		SDL_GetWindowSize( SDL_window, width, height );
	}
}

void GLimp_Shutdown( void )
{
	IOS_Layer_Shutdown();
	if ( glContext )
	{
		GLimp_ClearProcAddresses();
		SDL_GL_DeleteContext( glContext );
		glContext = NULL;
	}
	if ( SDL_window )
	{
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}
}

void GLimp_EndFrame( void )
{
	if ( SDL_window && glContext )
	{
		int w, h;

		SDL_GL_GetDrawableSize( SDL_window, &w, &h );
		if ( w > 0 && h > 0 )
		{
			glConfig.vidWidth = w;
			glConfig.vidHeight = h;
			glConfig.windowAspect = (float)w / (float)h;
		}

		GLimp_SyncIOSLayer();

		SDL_GL_SwapWindow( SDL_window );
	}
}

void GLimp_Minimize( void ) {}

void GLimp_LogComment( char *comment )
{
	(void)comment;
}

void GLimp_FrontEndSleep( void ) {}
int GLimp_SpawnWorkerThread( void (*function)( void ) ) { (void)function; return 0; }
void GLimp_WakeBackEnd( void *data ) { (void)data; }
void GLimp_WakeBackEndPost( void ) {}
void GLimp_DeactivateContext( void )
{
	if ( SDL_window && glContext )
		SDL_GL_MakeCurrent( SDL_window, NULL );
}
void GLimp_ActivateContext( void )
{
	if ( SDL_window && glContext )
		SDL_GL_MakeCurrent( SDL_window, glContext );
}
void GLimp_DestroyContext( void )
{
	GLimp_Shutdown();
}
