#include "client.h"
#include "cl_touch.h"
#include "../ios/ios_layer.h"

#define TOUCH_MAX_FINGERS 10
#define TOUCH_BTN_MIN 0.050f
#define TOUCH_BTN_MAX 0.150f

typedef enum
{
	TOUCH_ZONE_NONE,
	TOUCH_ZONE_MOVE,
	TOUCH_ZONE_LOOK,
	TOUCH_ZONE_JUMP,
	TOUCH_ZONE_CROUCH,
	TOUCH_ZONE_USE,
	TOUCH_ZONE_WEAPON,
	TOUCH_ZONE_MENU,
	TOUCH_ZONE_CONFIG,
	TOUCH_ZONE_SIZE_SLIDER
} touchZone_t;

typedef struct
{
	qboolean active;
	long long id;
	float x;
	float y;
	float startX;
	float startY;
	float editOffsetX;
	float editOffsetY;
	qboolean tapCandidate;
	touchZone_t zone;
} touchFinger_t;

static touchFinger_t fingers[TOUCH_MAX_FINGERS];
static qboolean touchEditMode;
static int touchWidth = 1;
static int touchHeight = 1;
static float touchScale = 1.0f;
static qboolean touchFireDown;
static qboolean touchJumpDown;
static qboolean touchCrouchDown;
static qboolean touchUseDown;
static qboolean touchMoveLeftDown;
static qboolean touchMoveRightDown;
static qboolean touchMoveForwardDown;
static qboolean touchMoveBackDown;
static qboolean touchUIMouseDown;
static int touchUIPendingLeftUpFrames;
static int touchPendingFireUpFrames;
static long long touchUIPointerFinger = -1;
static long long touchUIRightFinger = -1;
static qboolean touchUIRightDown;
static float touchUILastPx = -1.0f;
static float touchUILastPy = -1.0f;
static float touchUIStartPx = -1.0f;
static float touchUIStartPy = -1.0f;
static qboolean touchUITapCandidate;

static cvar_t *in_touch;
static cvar_t *in_touchSensitivity;
static cvar_t *in_touchUISensitivity;
static cvar_t *in_touchDeadzone;
static cvar_t *in_touchStickSize;
static cvar_t *in_touchBtnSize;
static cvar_t *in_touchMoveX;
static cvar_t *in_touchMoveY;
static cvar_t *in_touchJumpX;
static cvar_t *in_touchJumpY;
static cvar_t *in_touchCrouchX;
static cvar_t *in_touchCrouchY;
static cvar_t *in_touchUseX;
static cvar_t *in_touchUseY;
static cvar_t *in_touchWeaponX;
static cvar_t *in_touchWeaponY;
static cvar_t *in_touchMenuX;
static cvar_t *in_touchMenuY;
static cvar_t *in_touchConfigX;
static cvar_t *in_touchConfigY;
static cvar_t *in_touchDefaultsVersion;
static cvar_t *in_touchOpacity;

static void IN_TouchUIReset( void );

static void Touch_GetSafePixels( float *left, float *top, float *right, float *bottom )
{
	iosLayout_t layout;
	float scale;

	IOS_Layer_GetLayout( &layout );
	scale = layout.scale > 0.0f ? layout.scale : touchScale;
	if( scale <= 0.0f )
		scale = 1.0f;

	if( left )
		*left = layout.safeLeft * scale;
	if( top )
		*top = layout.safeTop * scale;
	if( right )
		*right = layout.safeRight * scale;
	if( bottom )
		*bottom = layout.safeBottom * scale;
}

static void Touch_Key( int key, qboolean down )
{
	Com_QueueEvent( 0, SE_KEY, key, down, 0, NULL );
}

static touchFinger_t *Touch_FindFinger( long long id )
{
	int i;

	for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
	{
		if( fingers[i].active && fingers[i].id == id )
			return &fingers[i];
	}

	return NULL;
}

static touchFinger_t *Touch_AllocFinger( long long id )
{
	int i;

	for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
	{
		if( !fingers[i].active )
		{
			Com_Memset( &fingers[i], 0, sizeof( fingers[i] ) );
			fingers[i].active = qtrue;
			fingers[i].id = id;
			return &fingers[i];
		}
	}

	return NULL;
}

static qboolean Touch_PointNear( float x, float y, float cx, float cy, float radius )
{
	float dx = x - cx;
	float dy = y - cy;
	return dx * dx + dy * dy <= radius * radius;
}

static float Touch_ControlBase( void )
{
	return touchWidth < touchHeight ? (float)touchWidth : (float)touchHeight;
}

static float Touch_ButtonRadius( void )
{
	float button = in_touchBtnSize->value * Touch_ControlBase();

	if( button < 48.0f )
		button = 48.0f;

	return button;
}

static float Touch_EdgeX( cvar_t *cv )
{
	float v = cv->value;
	float safeLeft, safeRight;

	Touch_GetSafePixels( &safeLeft, NULL, &safeRight, NULL );
	if( v < 0.0f )
		return touchWidth - safeRight + v * touchWidth;
	return safeLeft + v * touchWidth;
}

static float Touch_EdgeY( cvar_t *cv )
{
	float v = cv->value;
	float safeTop, safeBottom;

	Touch_GetSafePixels( NULL, &safeTop, NULL, &safeBottom );
	if( v < 0.0f )
		return touchHeight - safeBottom + v * touchHeight;
	return safeTop + v * touchHeight;
}

static qboolean Touch_ZoneCvars( touchZone_t zone, cvar_t **x, cvar_t **y )
{
	switch( zone )
	{
		case TOUCH_ZONE_JUMP: *x = in_touchJumpX; *y = in_touchJumpY; return qtrue;
		case TOUCH_ZONE_CROUCH: *x = in_touchCrouchX; *y = in_touchCrouchY; return qtrue;
		case TOUCH_ZONE_USE: *x = in_touchUseX; *y = in_touchUseY; return qtrue;
		case TOUCH_ZONE_WEAPON: *x = in_touchWeaponX; *y = in_touchWeaponY; return qtrue;
		case TOUCH_ZONE_MENU: *x = in_touchMenuX; *y = in_touchMenuY; return qtrue;
		case TOUCH_ZONE_CONFIG: *x = in_touchConfigX; *y = in_touchConfigY; return qtrue;
		default: return qfalse;
	}
}

static void Touch_SetHeldCommand( qboolean *state, const char *downCommand, const char *upCommand, qboolean down )
{
	if( *state == down )
		return;
	*state = down;
	if( down ? ( downCommand && downCommand[0] ) : ( upCommand && upCommand[0] ) )
	{
		Cbuf_AddText( down ? downCommand : upCommand );
		Cbuf_AddText( "\n" );
	}
}

static void Touch_SetHeldKey( qboolean *state, int key, qboolean down )
{
	if( *state == down )
		return;
	*state = down;
	Touch_Key( key, down );
}

static void Touch_StopZone( touchZone_t zone )
{
	if( zone == TOUCH_ZONE_JUMP )
		Touch_SetHeldCommand( &touchJumpDown, "+moveup", "-moveup", qfalse );
	else if( zone == TOUCH_ZONE_CROUCH )
		Touch_SetHeldCommand( &touchCrouchDown, "+movedown", "-movedown", qfalse );
	else if( zone == TOUCH_ZONE_USE )
		Touch_SetHeldCommand( &touchUseDown, "+button2", "-button2", qfalse );
}

static void Touch_StopAllGameplay( void )
{
	Touch_SetHeldKey( &touchFireDown, K_MOUSE1, qfalse );
	Touch_SetHeldCommand( &touchJumpDown, "+moveup", "-moveup", qfalse );
	Touch_SetHeldCommand( &touchCrouchDown, "+movedown", "-movedown", qfalse );
	Touch_SetHeldCommand( &touchUseDown, "+button2", "-button2", qfalse );
	Touch_SetHeldCommand( &touchMoveLeftDown, "+moveleft", "-moveleft", qfalse );
	Touch_SetHeldCommand( &touchMoveRightDown, "+moveright", "-moveright", qfalse );
	Touch_SetHeldCommand( &touchMoveForwardDown, "+forward", "-forward", qfalse );
	Touch_SetHeldCommand( &touchMoveBackDown, "+back", "-back", qfalse );
	touchPendingFireUpFrames = 0;
}

static void Touch_TapCommand( const char *command )
{
	Cbuf_AddText( command );
	Cbuf_AddText( "\n" );
}

static float Touch_TapThreshold( void )
{
	return 18.0f * ( touchScale > 0.0f ? touchScale : 1.0f );
}

static float Touch_SliderX( void )
{
	return touchWidth * 0.24f;
}

static float Touch_SliderY( void )
{
	float safeBottom;

	Touch_GetSafePixels( NULL, NULL, NULL, &safeBottom );
	return touchHeight - safeBottom - Touch_ButtonRadius() * 1.15f;
}

static float Touch_SliderW( void )
{
	return touchWidth * 0.52f;
}

static float Touch_SizeSliderValue( void )
{
	float v = ( in_touchBtnSize->value - TOUCH_BTN_MIN ) / ( TOUCH_BTN_MAX - TOUCH_BTN_MIN );

	if( v < 0.0f )
		v = 0.0f;
	if( v > 1.0f )
		v = 1.0f;

	return v;
}

static void Touch_SetSizeFromSliderX( float x )
{
	float sx = Touch_SliderX();
	float sw = Touch_SliderW();
	float t = sw > 0.0f ? ( x - sx ) / sw : 0.0f;
	float size;

	if( t < 0.0f )
		t = 0.0f;
	if( t > 1.0f )
		t = 1.0f;

	size = TOUCH_BTN_MIN + t * ( TOUCH_BTN_MAX - TOUCH_BTN_MIN );
	Cvar_Set( "in_touchBtnSize", va( "%.4f", size ) );
}

static qboolean Touch_PointOnSizeSlider( float x, float y )
{
	float sx = Touch_SliderX();
	float sy = Touch_SliderY();
	float sw = Touch_SliderW();
	float hit = Touch_ButtonRadius() * 0.55f;

	return x >= sx - hit && x <= sx + sw + hit && y >= sy - hit && y <= sy + hit;
}

static void Touch_SetZonePosition( touchZone_t zone, float x, float y )
{
	cvar_t *xCv = NULL;
	cvar_t *yCv = NULL;
	float safeLeft, safeTop, safeRight, safeBottom;
	float radius = Touch_ButtonRadius();
	float newX, newY;

	if( !Touch_ZoneCvars( zone, &xCv, &yCv ) || !xCv || !yCv )
		return;

	Touch_GetSafePixels( &safeLeft, &safeTop, &safeRight, &safeBottom );

	if( x < safeLeft + radius )
		x = safeLeft + radius;
	if( x > touchWidth - safeRight - radius )
		x = touchWidth - safeRight - radius;
	if( y < safeTop + radius )
		y = safeTop + radius;
	if( y > touchHeight - safeBottom - radius )
		y = touchHeight - safeBottom - radius;

	if( xCv->value < 0.0f )
		newX = ( x - ( touchWidth - safeRight ) ) / (float)touchWidth;
	else
		newX = ( x - safeLeft ) / (float)touchWidth;

	if( yCv->value < 0.0f )
		newY = ( y - ( touchHeight - safeBottom ) ) / (float)touchHeight;
	else
		newY = ( y - safeTop ) / (float)touchHeight;

	Cvar_Set( xCv->name, va( "%.4f", newX ) );
	Cvar_Set( yCv->name, va( "%.4f", newY ) );
}

static touchZone_t Touch_Classify( float x, float y )
{
	float stick = in_touchStickSize->value * Touch_ControlBase();
	float button = Touch_ButtonRadius();

	if( Touch_PointNear( x, y, Touch_EdgeX( in_touchConfigX ), Touch_EdgeY( in_touchConfigY ), button * 0.78f ) )
		return TOUCH_ZONE_CONFIG;
	if( touchEditMode && Touch_PointOnSizeSlider( x, y ) )
		return TOUCH_ZONE_SIZE_SLIDER;
	if( Touch_PointNear( x, y, Touch_EdgeX( in_touchJumpX ), Touch_EdgeY( in_touchJumpY ), button ) )
		return TOUCH_ZONE_JUMP;
	if( Touch_PointNear( x, y, Touch_EdgeX( in_touchCrouchX ), Touch_EdgeY( in_touchCrouchY ), button ) )
		return TOUCH_ZONE_CROUCH;
	if( Touch_PointNear( x, y, Touch_EdgeX( in_touchUseX ), Touch_EdgeY( in_touchUseY ), button ) )
		return TOUCH_ZONE_USE;
	if( Touch_PointNear( x, y, Touch_EdgeX( in_touchWeaponX ), Touch_EdgeY( in_touchWeaponY ), button ) )
		return TOUCH_ZONE_WEAPON;
	if( Touch_PointNear( x, y, Touch_EdgeX( in_touchMenuX ), Touch_EdgeY( in_touchMenuY ), button ) )
		return TOUCH_ZONE_MENU;
	if( touchEditMode )
		return TOUCH_ZONE_NONE;
	if( x >= touchWidth * 0.5f )
		return TOUCH_ZONE_LOOK;
	if( Touch_PointNear( x, y, Touch_EdgeX( in_touchMoveX ), Touch_EdgeY( in_touchMoveY ), stick ) )
		return TOUCH_ZONE_MOVE;
	if( x < touchWidth * 0.5f )
		return TOUCH_ZONE_MOVE;
	return TOUCH_ZONE_NONE;
}

static void Touch_TapFire( void )
{
	if( !touchFireDown )
	{
		touchFireDown = qtrue;
		Touch_Key( K_MOUSE1, qtrue );
	}
	touchPendingFireUpFrames = 2;
}

static void Touch_ToggleEditMode( void )
{
	int i;

	touchEditMode = !touchEditMode;
	Touch_StopAllGameplay();

	for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
	{
		if( fingers[i].active && fingers[i].zone != TOUCH_ZONE_CONFIG )
			fingers[i].active = qfalse;
	}

	if( !touchEditMode )
		Cbuf_AddText( "writeconfig\n" );
}

void IN_TouchApplyDefaults( void )
{
	Cvar_Set( "r_mode", "-2" );
	Cvar_Set( "r_fullscreen", "1" );
	Cvar_Set( "in_touchStickSize", "0.14" );
	Cvar_Set( "in_touchBtnSize", "0.075" );
	Cvar_Set( "in_touchJumpX", "-0.06" );
	Cvar_Set( "in_touchJumpY", "-0.30" );
	Cvar_Set( "in_touchCrouchX", "-0.06" );
	Cvar_Set( "in_touchCrouchY", "-0.08" );
	Cvar_Set( "in_touchUseX", "-0.26" );
	Cvar_Set( "in_touchUseY", "0.11" );
	Cvar_Set( "in_touchWeaponX", "-0.16" );
	Cvar_Set( "in_touchWeaponY", "0.11" );
	Cvar_Set( "in_touchMenuX", "0.08" );
	Cvar_Set( "in_touchMenuY", "0.12" );
	Cvar_Set( "in_touchConfigX", "0.18" );
	Cvar_Set( "in_touchConfigY", "0.12" );
}

void IN_TouchInit( void )
{
	in_touch = Cvar_Get( "in_touch", "1", CVAR_ARCHIVE );
	in_touchSensitivity = Cvar_Get( "in_touchSensitivity", "2.2", CVAR_ARCHIVE );
	in_touchUISensitivity = Cvar_Get( "in_touchUISensitivity", "1.6", CVAR_ARCHIVE );
	in_touchDeadzone = Cvar_Get( "in_touchDeadzone", "0.035", CVAR_ARCHIVE );
	in_touchStickSize = Cvar_Get( "in_touchStickSize", "0.14", CVAR_ARCHIVE );
	in_touchBtnSize = Cvar_Get( "in_touchBtnSize", "0.075", CVAR_ARCHIVE );
	in_touchMoveX = Cvar_Get( "in_touchMoveX", "0.13", CVAR_ARCHIVE );
	in_touchMoveY = Cvar_Get( "in_touchMoveY", "-0.18", CVAR_ARCHIVE );
	in_touchJumpX = Cvar_Get( "in_touchJumpX", "-0.22", CVAR_ARCHIVE );
	in_touchJumpY = Cvar_Get( "in_touchJumpY", "-0.11", CVAR_ARCHIVE );
	in_touchCrouchX = Cvar_Get( "in_touchCrouchX", "-0.34", CVAR_ARCHIVE );
	in_touchCrouchY = Cvar_Get( "in_touchCrouchY", "-0.11", CVAR_ARCHIVE );
	in_touchUseX = Cvar_Get( "in_touchUseX", "-0.18", CVAR_ARCHIVE );
	in_touchUseY = Cvar_Get( "in_touchUseY", "0.34", CVAR_ARCHIVE );
	in_touchWeaponX = Cvar_Get( "in_touchWeaponX", "-0.06", CVAR_ARCHIVE );
	in_touchWeaponY = Cvar_Get( "in_touchWeaponY", "0.18", CVAR_ARCHIVE );
	in_touchMenuX = Cvar_Get( "in_touchMenuX", "0.08", CVAR_ARCHIVE );
	in_touchMenuY = Cvar_Get( "in_touchMenuY", "0.12", CVAR_ARCHIVE );
	in_touchConfigX = Cvar_Get( "in_touchConfigX", "0.18", CVAR_ARCHIVE );
	in_touchConfigY = Cvar_Get( "in_touchConfigY", "0.12", CVAR_ARCHIVE );
	in_touchDefaultsVersion = Cvar_Get( "in_touchDefaultsVersion", "0", CVAR_ARCHIVE );
	in_touchOpacity = Cvar_Get( "in_touchOpacity", "0.34", CVAR_ARCHIVE );

	if( in_touchDefaultsVersion->integer < 1 )
	{
		IN_TouchApplyDefaults();
		Cvar_Set( "in_touchDefaultsVersion", "1" );
	}
}

void IN_TouchShutdown( void )
{
	int i;

	for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
	{
		if( fingers[i].active )
			Touch_StopZone( fingers[i].zone );
	}

	Com_Memset( fingers, 0, sizeof( fingers ) );
	Touch_StopAllGameplay();
	touchEditMode = qfalse;
	IN_TouchUIReset();
}

void IN_TouchSyncLayout( int width, int height, float scale )
{
	iosLayout_t layout;
	int screenWidth;
	int screenHeight;

	touchWidth = width > 0 ? width : 1;
	touchHeight = height > 0 ? height : 1;
	touchScale = scale > 0 ? scale : 1.0f;
	IOS_Layer_SyncScreen( width, height, touchScale );
	IOS_Layer_GetLayout( &layout );
	screenWidth = (int)( layout.width * layout.scale );
	screenHeight = (int)( layout.height * layout.scale );
	if( screenWidth > touchWidth )
		touchWidth = screenWidth;
	if( screenHeight > touchHeight )
		touchHeight = screenHeight;
}

qboolean IN_TouchInUIMode( void )
{
	if( Key_GetCatcher() & ( KEYCATCH_UI | KEYCATCH_CGAME ) )
		return qtrue;
	if( clc.state == CA_DISCONNECTED )
		return qtrue;
	return qfalse;
}

static void IN_TouchUIMouse( float x, float y, qboolean down, qboolean move )
{
	float tapDx, tapDy;
	float tapThreshold;
	int dx, dy;

	if( !down )
	{
		if( touchUIMouseDown )
		{
			touchUIMouseDown = qfalse;
			Com_QueueEvent( 0, SE_KEY, K_MOUSE1, qfalse, 0, NULL );
		}
		else if( touchUITapCandidate )
		{
			Com_QueueEvent( 0, SE_KEY, K_MOUSE1, qtrue, 0, NULL );
			touchUIPendingLeftUpFrames = 2;
		}
		touchUITapCandidate = qfalse;
		touchUIStartPx = -1.0f;
		touchUIStartPy = -1.0f;
		touchUILastPx = -1.0f;
		touchUILastPy = -1.0f;
		return;
	}

	if( touchUILastPx < 0.0f )
	{
		touchUIStartPx = x;
		touchUIStartPy = y;
		touchUILastPx = x;
		touchUILastPy = y;
		touchUITapCandidate = qtrue;
		return;
	}
	else if( move )
	{
		tapDx = x - touchUIStartPx;
		tapDy = y - touchUIStartPy;
		tapThreshold = 18.0f * ( touchScale > 0.0f ? touchScale : 1.0f );
		if( tapDx * tapDx + tapDy * tapDy > tapThreshold * tapThreshold )
			touchUITapCandidate = qfalse;
	}

	dx = (int)( ( x - touchUILastPx ) * in_touchUISensitivity->value );
	dy = (int)( ( y - touchUILastPy ) * in_touchUISensitivity->value );
	touchUILastPx = x;
	touchUILastPy = y;

	if( dx == 0 && dy == 0 )
		return;

	Com_QueueEvent( 0, SE_MOUSE, dx, dy, 0, NULL );
}

static void IN_TouchUIRightClick( qboolean down )
{
	if( touchUIRightDown == down )
		return;
	touchUIRightDown = down;
	Com_QueueEvent( 0, SE_KEY, K_MOUSE2, down, 0, NULL );
}

static void IN_TouchUIReset( void )
{
	if( touchUIPendingLeftUpFrames > 0 )
	{
		touchUIPendingLeftUpFrames = 0;
		Com_QueueEvent( 0, SE_KEY, K_MOUSE1, qfalse, 0, NULL );
	}
	if( touchUIMouseDown )
	{
		touchUIMouseDown = qfalse;
		Com_QueueEvent( 0, SE_KEY, K_MOUSE1, qfalse, 0, NULL );
	}
	IN_TouchUIRightClick( qfalse );
	touchUIPointerFinger = -1;
	touchUIRightFinger = -1;
	touchUITapCandidate = qfalse;
	touchUIStartPx = -1.0f;
	touchUIStartPy = -1.0f;
	touchUILastPx = -1.0f;
	touchUILastPy = -1.0f;
}

qboolean IN_TouchConsoleActive( void )
{
	return ( Key_GetCatcher() & KEYCATCH_CONSOLE ) != 0;
}

void IN_TouchToggleConsole( void )
{
	Touch_Key( K_CONSOLE, qtrue );
	Touch_Key( K_CONSOLE, qfalse );
}

void IN_TouchMouse( int x, int y, qboolean down, qboolean motion )
{
	if( motion )
		Com_QueueEvent( 0, SE_MOUSE, x, y, 0, NULL );
	if( down != qfalse )
		Touch_Key( K_MOUSE1, qtrue );
	else if( !motion )
		Touch_Key( K_MOUSE1, qfalse );
}

void IN_TouchFinger( long long fingerId, float nx, float ny, qboolean down, qboolean motion )
{
	touchFinger_t *finger;
	float x = nx * touchWidth;
	float y = ny * touchHeight;

	if( !in_touch || !in_touch->integer )
		return;

	if( IN_TouchInUIMode() )
	{
		if( down )
		{
			if( touchUIPointerFinger < 0 || touchUIPointerFinger == fingerId )
			{
				touchUIPointerFinger = fingerId;
				IN_TouchUIMouse( x, y, qtrue, motion );
			}
			else if( touchUIRightFinger < 0 || touchUIRightFinger == fingerId )
			{
				touchUITapCandidate = qfalse;
				touchUIRightFinger = fingerId;
				IN_TouchUIRightClick( qtrue );
			}
		}
		else if( touchUIRightFinger == fingerId )
		{
			IN_TouchUIRightClick( qfalse );
			touchUIRightFinger = -1;
		}
		else if( touchUIPointerFinger == fingerId )
		{
			IN_TouchUIMouse( x, y, qfalse, motion );
			touchUIPointerFinger = -1;
		}
		return;
	}

	if( touchEditMode )
	{
		if( down )
		{
			finger = Touch_FindFinger( fingerId );
			if( !finger )
			{
				cvar_t *xCv = NULL;
				cvar_t *yCv = NULL;

				finger = Touch_AllocFinger( fingerId );
				if( !finger )
					return;

				finger->startX = x;
				finger->startY = y;
				finger->zone = Touch_Classify( x, y );
				finger->tapCandidate = finger->zone == TOUCH_ZONE_CONFIG;

				if( finger->zone == TOUCH_ZONE_SIZE_SLIDER )
					Touch_SetSizeFromSliderX( x );
				else if( Touch_ZoneCvars( finger->zone, &xCv, &yCv ) )
				{
					finger->editOffsetX = Touch_EdgeX( xCv ) - x;
					finger->editOffsetY = Touch_EdgeY( yCv ) - y;
				}
			}

			if( !finger )
				return;

			if( motion )
			{
				float tapDx = x - finger->startX;
				float tapDy = y - finger->startY;
				float tapThreshold = Touch_TapThreshold();

				if( tapDx * tapDx + tapDy * tapDy > tapThreshold * tapThreshold )
					finger->tapCandidate = qfalse;

				if( finger->zone == TOUCH_ZONE_SIZE_SLIDER )
					Touch_SetSizeFromSliderX( x );
				else if( finger->zone != TOUCH_ZONE_NONE && finger->zone != TOUCH_ZONE_CONFIG )
					Touch_SetZonePosition( finger->zone, x + finger->editOffsetX, y + finger->editOffsetY );
			}

			finger->x = x;
			finger->y = y;
		}
		else
		{
			finger = Touch_FindFinger( fingerId );
			if( !finger )
				return;
			if( finger->zone == TOUCH_ZONE_CONFIG && finger->tapCandidate )
				Touch_ToggleEditMode();
			finger->active = qfalse;
		}
		return;
	}

	if( down )
	{
		finger = Touch_FindFinger( fingerId );
		if( !finger )
		{
			finger = Touch_AllocFinger( fingerId );
			if( !finger )
				return;

			finger->startX = x;
			finger->startY = y;
			finger->zone = Touch_Classify( x, y );
			finger->tapCandidate = finger->zone == TOUCH_ZONE_LOOK || finger->zone == TOUCH_ZONE_MOVE;

			if( finger->zone == TOUCH_ZONE_JUMP )
				Touch_SetHeldCommand( &touchJumpDown, "+moveup", "-moveup", qtrue );
			else if( finger->zone == TOUCH_ZONE_CROUCH )
				Touch_SetHeldCommand( &touchCrouchDown, "+movedown", "-movedown", qtrue );
			else if( finger->zone == TOUCH_ZONE_USE )
				Touch_SetHeldCommand( &touchUseDown, "+button2", "-button2", qtrue );
			else if( finger->zone == TOUCH_ZONE_MENU )
				Cbuf_AddText( "togglemenu\n" );
		}

		if( !finger )
			return;

		if( motion && finger->zone == TOUCH_ZONE_LOOK )
		{
			float dx = ( x - finger->x ) * in_touchSensitivity->value;
			float dy = ( y - finger->y ) * in_touchSensitivity->value;
			float tapDx = x - finger->startX;
			float tapDy = y - finger->startY;
			float tapThreshold = Touch_TapThreshold();

			if( tapDx * tapDx + tapDy * tapDy > tapThreshold * tapThreshold )
				finger->tapCandidate = qfalse;

			Com_QueueEvent( 0, SE_MOUSE, (int)dx, (int)dy, 0, NULL );
		}
		else if( motion && finger->zone == TOUCH_ZONE_MOVE )
		{
			float dx = x - finger->startX;
			float dy = y - finger->startY;
			float dead = in_touchDeadzone->value * touchWidth;
			float tapThreshold = Touch_TapThreshold();

			if( dx * dx + dy * dy > tapThreshold * tapThreshold )
				finger->tapCandidate = qfalse;

			Touch_SetHeldCommand( &touchMoveRightDown, "+moveright", "-moveright", dx > dead );
			Touch_SetHeldCommand( &touchMoveLeftDown, "+moveleft", "-moveleft", dx < -dead );
			Touch_SetHeldCommand( &touchMoveBackDown, "+back", "-back", dy > dead );
			Touch_SetHeldCommand( &touchMoveForwardDown, "+forward", "-forward", dy < -dead );
		}

		finger->x = x;
		finger->y = y;
	}
	else
	{
		finger = Touch_FindFinger( fingerId );
		if( !finger )
			return;

		if( finger->zone == TOUCH_ZONE_WEAPON )
			Touch_TapCommand( "weapnext" );
		else if( finger->zone == TOUCH_ZONE_CONFIG && finger->tapCandidate )
			Touch_ToggleEditMode();
		else if( finger->tapCandidate )
			Touch_TapFire();

		Touch_StopZone( finger->zone );
		if( finger->zone == TOUCH_ZONE_MOVE )
		{
			Touch_SetHeldCommand( &touchMoveRightDown, "+moveright", "-moveright", qfalse );
			Touch_SetHeldCommand( &touchMoveLeftDown, "+moveleft", "-moveleft", qfalse );
			Touch_SetHeldCommand( &touchMoveBackDown, "+back", "-back", qfalse );
			Touch_SetHeldCommand( &touchMoveForwardDown, "+forward", "-forward", qfalse );
		}

		finger->active = qfalse;
	}
}

void IN_TouchFrame( void )
{
	if( !in_touch || !in_touch->integer )
		return;

	if( touchPendingFireUpFrames > 0 && --touchPendingFireUpFrames == 0 )
		Touch_SetHeldKey( &touchFireDown, K_MOUSE1, qfalse );
	if( touchUIPendingLeftUpFrames > 0 && --touchUIPendingLeftUpFrames == 0 )
		Com_QueueEvent( 0, SE_KEY, K_MOUSE1, qfalse, 0, NULL );
	if( !IN_TouchInUIMode() && ( touchUIPointerFinger >= 0 || touchUIRightFinger >= 0 || touchUIRightDown ) )
		IN_TouchUIReset();

	IOS_Layer_Tick();
}

static touchFinger_t *Touch_ActiveFingerForZone( touchZone_t zone )
{
	int i;

	for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
	{
		if( fingers[i].active && fingers[i].zone == zone )
			return &fingers[i];
	}

	return NULL;
}

void IN_TouchDraw( void )
{
	float stick;
	float button;
	float mx, my, jx, jy, cx, cy, ux, uy, wx, wy, menux, menuy, gx, gy;

	if( !in_touch || !in_touch->integer || IN_TouchInUIMode() || clc.state != CA_ACTIVE )
	{
		IOS_Layer_UpdateTouchControls( qfalse, 0.0f,
			0, 0, 0,
			0, 0, 0, qfalse,
			0, 0, 0, qfalse,
			0, 0, 0, qfalse,
			0, 0, 0,
			0, 0, 0,
			0, 0, 0, qfalse,
			qfalse, 0, 0, 0, 0 );
		return;
	}

	stick = in_touchStickSize->value * Touch_ControlBase();
	button = in_touchBtnSize->value * Touch_ControlBase();
	if( stick < 72.0f )
		stick = 72.0f;
	if( button < 48.0f )
		button = 48.0f;

	mx = Touch_EdgeX( in_touchMoveX );
	my = Touch_EdgeY( in_touchMoveY );
	jx = Touch_EdgeX( in_touchJumpX );
	jy = Touch_EdgeY( in_touchJumpY );
	cx = Touch_EdgeX( in_touchCrouchX );
	cy = Touch_EdgeY( in_touchCrouchY );
	ux = Touch_EdgeX( in_touchUseX );
	uy = Touch_EdgeY( in_touchUseY );
	wx = Touch_EdgeX( in_touchWeaponX );
	wy = Touch_EdgeY( in_touchWeaponY );
	menux = Touch_EdgeX( in_touchMenuX );
	menuy = Touch_EdgeY( in_touchMenuY );
	gx = Touch_EdgeX( in_touchConfigX );
	gy = Touch_EdgeY( in_touchConfigY );

	IOS_Layer_UpdateTouchControls( qtrue, in_touchOpacity->value,
		mx, my, stick,
		jx, jy, button, touchJumpDown,
		cx, cy, button, touchCrouchDown,
		ux, uy, button, touchUseDown,
		wx, wy, button,
		menux, menuy, button * 0.78f,
		gx, gy, button * 0.78f, Touch_ActiveFingerForZone( TOUCH_ZONE_CONFIG ) != NULL,
		touchEditMode, Touch_SliderX(), Touch_SliderY(), Touch_SliderW(), Touch_SizeSliderValue() );
}
