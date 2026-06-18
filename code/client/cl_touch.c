#include "client.h"
#include "cl_touch.h"
#include "../ios/ios_layer.h"
#ifdef IOS
#include "../ios/ios_gamepad.h"
#endif

#define TOUCH_MAX_FINGERS 10
#define TOUCH_KEY_BASE 240

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
static touchMode_t touchMode = TOUCH_MODE_COMBAT;
static qboolean touchEditMode;
static int touchWidth = 1;
static int touchHeight = 1;
static float touchScale = 1.0f;
static qboolean touchFireDown;
static qboolean touchAltFireDown;
static qboolean touchJumpDown;
static qboolean touchCrouchDown;
static qboolean touchUseDown;
static qboolean touchOpenDown;
static qboolean touchMoveLeftDown;
static qboolean touchMoveRightDown;
static qboolean touchMoveForwardDown;
static qboolean touchMoveBackDown;
static qboolean touchTurnLeftDown;
static qboolean touchTurnRightDown;
static qboolean touchUIMouseDown;
static int touchUIPendingLeftUpFrames;
static int touchPendingFireUpFrames;
static int touchPendingAltFireUpFrames;
static long long touchUIPointerFinger = -1;
static long long touchUIRightFinger = -1;
static qboolean touchUIRightDown;
static float touchUILastPx = -1.0f;
static float touchUILastPy = -1.0f;
static float touchUICursorX = 320.0f;
static float touchUICursorY = 240.0f;
static qboolean touchUITapCandidate;
static float touchUIStartPx = -1.0f;
static float touchUIStartPy = -1.0f;

static cvar_t *in_touch;
static cvar_t *in_touchMoveSensitivity;
static cvar_t *in_touchSensitivity;
static cvar_t *in_touchUISensitivity;
static cvar_t *in_touchDeadzone;
static cvar_t *in_touchStickSize;
static cvar_t *in_touchBtnSize;
static cvar_t *in_touchMoveX;
static cvar_t *in_touchMoveY;
static cvar_t *in_touchLookX;
static cvar_t *in_touchLookY;
static cvar_t *in_touchJumpX;
static cvar_t *in_touchJumpY;
static cvar_t *in_touchCrouchX;
static cvar_t *in_touchCrouchY;
static cvar_t *in_touchUseX;
static cvar_t *in_touchUseY;
static cvar_t *in_touchOpenX;
static cvar_t *in_touchOpenY;
static cvar_t *in_touchWeaponsX;
static cvar_t *in_touchWeaponsY;
static cvar_t *in_touchMenuX;
static cvar_t *in_touchMenuY;
static cvar_t *in_touchConfigX;
static cvar_t *in_touchConfigY;
static cvar_t *in_touchFireX;
static cvar_t *in_touchFireY;
static cvar_t *in_touchAltFireX;
static cvar_t *in_touchAltFireY;
static cvar_t *in_touchAimMode;
static cvar_t *in_touchMoveMode;
static cvar_t *in_touchMoveHoriz;
static cvar_t *in_touchFireMode;
static cvar_t *in_touchDefaultsVersion;
static cvar_t *in_touchDebug;
static cvar_t *in_touchOpacity;

static void IN_TouchUIReset( void );

#define TOUCH_BTN_MIN 0.045f
#define TOUCH_BTN_MAX 0.140f

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

static qboolean Touch_PointNear( float x, float y, float cx, float cy, float radius )
{
	float dx = x - cx;
	float dy = y - cy;
	return dx * dx + dy * dy <= radius * radius;
}

static qboolean Touch_AimStickMode( void )
{
	return in_touchAimMode && in_touchAimMode->integer == 0;
}

static qboolean Touch_MoveStickMode( void )
{
	return in_touchMoveMode && in_touchMoveMode->integer == 0;
}

static qboolean Touch_MoveHorizTurn( void )
{
	return in_touchMoveHoriz && in_touchMoveHoriz->integer == 1;
}

static qboolean Touch_FireButtonsMode( void )
{
	return in_touchFireMode && in_touchFireMode->integer == 1;
}

static qboolean Touch_HideControlsForHardwareInput( void )
{
#ifdef IOS
	if( touchEditMode )
		return qfalse;
	return IOS_Layer_HasHardwareKeyboard() && IOS_Layer_HasHardwareMouse();
#else
	return qfalse;
#endif
}

#ifdef IOS
/*
 * External display + physical keyboard and mouse: the game and menus render on
 * the external screen, the player uses the hardware, and the device shows no
 * controls at all (an invisible trackpad that still moves the menu cursor).
 */
static qboolean Touch_ExternalKeyboardMouse( void )
{
	if( touchEditMode )
		return qfalse;
	return IOS_Layer_HasExternalScreen() &&
		IOS_Layer_HasHardwareKeyboard() &&
		IOS_Layer_HasHardwareMouse();
}
#endif

static qboolean Touch_IsButtonZone( touchZone_t zone )
{
	return zone == TOUCH_ZONE_JUMP ||
		zone == TOUCH_ZONE_CROUCH ||
		zone == TOUCH_ZONE_USE ||
		zone == TOUCH_ZONE_OPEN ||
		zone == TOUCH_ZONE_WEAPONS ||
		zone == TOUCH_ZONE_MENU ||
		zone == TOUCH_ZONE_CONFIG ||
		zone == TOUCH_ZONE_FIRE ||
		zone == TOUCH_ZONE_ALT_FIRE;
}

static float Touch_ControlBase( void )
{
	return touchWidth < touchHeight ? (float)touchWidth : (float)touchHeight;
}

static float Touch_ButtonRadius( void );
static float Touch_StickRadius( void );

static qboolean Touch_IsStickZone( touchZone_t zone )
{
	return zone == TOUCH_ZONE_MOVE || zone == TOUCH_ZONE_LOOK;
}

static qboolean Touch_ZoneCvars( touchZone_t zone, cvar_t **x, cvar_t **y )
{
	switch( zone )
	{
		case TOUCH_ZONE_JUMP: *x = in_touchJumpX; *y = in_touchJumpY; return qtrue;
		case TOUCH_ZONE_CROUCH: *x = in_touchCrouchX; *y = in_touchCrouchY; return qtrue;
		case TOUCH_ZONE_USE: *x = in_touchUseX; *y = in_touchUseY; return qtrue;
		case TOUCH_ZONE_OPEN: *x = in_touchOpenX; *y = in_touchOpenY; return qtrue;
		case TOUCH_ZONE_WEAPONS: *x = in_touchWeaponsX; *y = in_touchWeaponsY; return qtrue;
		case TOUCH_ZONE_MENU: *x = in_touchMenuX; *y = in_touchMenuY; return qtrue;
		case TOUCH_ZONE_CONFIG: *x = in_touchConfigX; *y = in_touchConfigY; return qtrue;
		case TOUCH_ZONE_FIRE: *x = in_touchFireX; *y = in_touchFireY; return qtrue;
		case TOUCH_ZONE_ALT_FIRE: *x = in_touchAltFireX; *y = in_touchAltFireY; return qtrue;
		case TOUCH_ZONE_MOVE: *x = in_touchMoveX; *y = in_touchMoveY; return qtrue;
		case TOUCH_ZONE_LOOK: *x = in_touchLookX; *y = in_touchLookY; return qtrue;
		default: return qfalse;
	}
}

static void Touch_SetZonePosition( touchZone_t zone, float x, float y )
{
	cvar_t *xCv = NULL;
	cvar_t *yCv = NULL;
	float safeLeft, safeTop, safeRight, safeBottom;
	float radius = Touch_IsStickZone( zone ) ? Touch_StickRadius() : Touch_ButtonRadius();
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

static float Touch_ButtonRadius( void )
{
	float button = in_touchBtnSize->value * Touch_ControlBase();
	if( button < 48.0f )
		button = 48.0f;
	return button;
}

static float Touch_StickRadius( void )
{
	float stick = in_touchStickSize->value * Touch_ControlBase();
	if( stick < 72.0f )
		stick = 72.0f;
	return stick;
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

static touchZone_t Touch_Classify( float x, float y )
{
	float stick = Touch_StickRadius();
	float button = Touch_ButtonRadius();
	qboolean hideControls = Touch_HideControlsForHardwareInput();

	if( !hideControls )
	{
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchMenuX ), Touch_EdgeY( in_touchMenuY ), button ) )
			return TOUCH_ZONE_MENU;
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchConfigX ), Touch_EdgeY( in_touchConfigY ), button ) )
			return TOUCH_ZONE_CONFIG;
	}
	if( touchEditMode && Touch_PointOnSizeSlider( x, y ) )
		return TOUCH_ZONE_SIZE_SLIDER;
	if( !hideControls )
	{
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchJumpX ), Touch_EdgeY( in_touchJumpY ), button ) )
			return TOUCH_ZONE_JUMP;
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchCrouchX ), Touch_EdgeY( in_touchCrouchY ), button ) )
			return TOUCH_ZONE_CROUCH;
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchUseX ), Touch_EdgeY( in_touchUseY ), button ) )
			return TOUCH_ZONE_USE;
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchOpenX ), Touch_EdgeY( in_touchOpenY ), button ) )
			return TOUCH_ZONE_OPEN;
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchWeaponsX ), Touch_EdgeY( in_touchWeaponsY ), button ) )
			return TOUCH_ZONE_WEAPONS;
		if( Touch_FireButtonsMode() )
		{
			if( Touch_PointNear( x, y, Touch_EdgeX( in_touchFireX ), Touch_EdgeY( in_touchFireY ), button ) )
				return TOUCH_ZONE_FIRE;
			if( Touch_PointNear( x, y, Touch_EdgeX( in_touchAltFireX ), Touch_EdgeY( in_touchAltFireY ), button ) )
				return TOUCH_ZONE_ALT_FIRE;
		}
	}
	if( touchEditMode )
	{
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchMoveX ), Touch_EdgeY( in_touchMoveY ), stick ) )
			return TOUCH_ZONE_MOVE;
		if( Touch_PointNear( x, y, Touch_EdgeX( in_touchLookX ), Touch_EdgeY( in_touchLookY ), stick ) )
			return TOUCH_ZONE_LOOK;
		return TOUCH_ZONE_NONE;
	}
	if( Touch_MoveStickMode() && Touch_PointNear( x, y, Touch_EdgeX( in_touchMoveX ), Touch_EdgeY( in_touchMoveY ), stick ) )
		return TOUCH_ZONE_MOVE;
	if( Touch_AimStickMode() && Touch_PointNear( x, y, Touch_EdgeX( in_touchLookX ), Touch_EdgeY( in_touchLookY ), stick ) )
		return TOUCH_ZONE_LOOK;
	if( !Touch_AimStickMode() && x >= touchWidth * 0.5f )
		return TOUCH_ZONE_LOOK;
	if( !Touch_MoveStickMode() && x < touchWidth * 0.5f )
		return TOUCH_ZONE_MOVE;
	return TOUCH_ZONE_NONE;
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
	if( zone == TOUCH_ZONE_FIRE )
		Touch_SetHeldKey( &touchFireDown, K_MOUSE1, qfalse );
	else if( zone == TOUCH_ZONE_ALT_FIRE )
		Touch_SetHeldKey( &touchAltFireDown, K_MOUSE2, qfalse );
	else if( zone == TOUCH_ZONE_JUMP )
		Touch_SetHeldCommand( &touchJumpDown, "+moveup", "-moveup", qfalse );
	else if( zone == TOUCH_ZONE_CROUCH )
		Touch_SetHeldCommand( &touchCrouchDown, "+movedown", "-movedown", qfalse );
	else if( zone == TOUCH_ZONE_USE )
		Touch_SetHeldCommand( &touchUseDown, "+use", "-use", qfalse );
	else if( zone == TOUCH_ZONE_OPEN )
		Touch_SetHeldCommand( &touchOpenDown, "+zoom", "-zoom", qfalse );
}

static void Touch_StopAllGameplay( void )
{
	Touch_SetHeldKey( &touchFireDown, K_MOUSE1, qfalse );
	Touch_SetHeldKey( &touchAltFireDown, K_MOUSE2, qfalse );
	Touch_SetHeldCommand( &touchJumpDown, "+moveup", "-moveup", qfalse );
	Touch_SetHeldCommand( &touchCrouchDown, "+movedown", "-movedown", qfalse );
	Touch_SetHeldCommand( &touchUseDown, "+use", "-use", qfalse );
	Touch_SetHeldCommand( &touchOpenDown, "+zoom", "-zoom", qfalse );
	Touch_SetHeldCommand( &touchMoveLeftDown, "+moveleft", "-moveleft", qfalse );
	Touch_SetHeldCommand( &touchMoveRightDown, "+moveright", "-moveright", qfalse );
	Touch_SetHeldCommand( &touchMoveForwardDown, "+forward", "-forward", qfalse );
	Touch_SetHeldCommand( &touchMoveBackDown, "+back", "-back", qfalse );
	Touch_SetHeldCommand( &touchTurnLeftDown, "+left", "-left", qfalse );
	Touch_SetHeldCommand( &touchTurnRightDown, "+right", "-right", qfalse );
	touchPendingFireUpFrames = 0;
	touchPendingAltFireUpFrames = 0;
}

static float Touch_TapThreshold( void );

static void Touch_ToggleEditMode( void )
{
	int i;

	touchEditMode = !touchEditMode;
	touchMode = touchEditMode ? TOUCH_MODE_UTILITY : TOUCH_MODE_COMBAT;
	Touch_StopAllGameplay();

	for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
	{
		if( fingers[i].active && fingers[i].zone != TOUCH_ZONE_CONFIG )
			fingers[i].active = qfalse;
	}

	if( !touchEditMode )
		Cbuf_AddText( "writeconfig\n" );
}

void IN_TouchEnterEditMode( void )
{
	if( !touchEditMode )
		Touch_ToggleEditMode();
}

qboolean IN_TouchEditModeActive( void )
{
	return touchEditMode;
}

static void Touch_OpenConfig( void )
{
	if( touchEditMode )
		Touch_ToggleEditMode();
	else
		IOS_Layer_OpenTouchSettings();
}

#ifdef IOS
static qboolean Touch_GamepadOverridesGameplay( void )
{
	if( !IOS_Gamepad_IsActive() )
		return qfalse;
	if( IN_TouchInUIMode() || clc.state != CA_ACTIVE )
		return qfalse;
	return qtrue;
}

static void Touch_ApplyGamepadMode( qboolean gamepadMode )
{
	static qboolean lastGamepadMode = qfalse;
	static qboolean lastOverlayGamepadMode = qfalse;
	int i;
	qboolean overlayGamepadMode = gamepadMode && !touchEditMode;

	if( overlayGamepadMode != lastOverlayGamepadMode )
	{
		lastOverlayGamepadMode = overlayGamepadMode;
		IOS_Layer_SetTouchGamepadMode( overlayGamepadMode );
	}

	if( gamepadMode == lastGamepadMode )
		return;

	lastGamepadMode = gamepadMode;

	if( !gamepadMode )
		return;

	Touch_StopAllGameplay();
	if( touchEditMode )
		return;

	touchEditMode = qfalse;
	touchMode = TOUCH_MODE_COMBAT;
	IN_TouchUIReset();
	for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
		fingers[i].active = qfalse;
}

static void Touch_HandleGamepadConfigFinger( long long fingerId, float x, float y, qboolean down, qboolean motion )
{
	touchFinger_t *finger;

	if( down )
	{
		finger = Touch_FindFinger( fingerId );
		if( !finger )
		{
			/* Hit-test the PAD config button directly. Touch_Classify drops the
			 * CONFIG zone when a hardware keyboard+mouse is present, which would
			 * otherwise make the gamepad PAD button untappable. */
			if( !Touch_PointNear( x, y, Touch_EdgeX( in_touchConfigX ),
				Touch_EdgeY( in_touchConfigY ), Touch_ButtonRadius() ) )
				return;

			finger = Touch_AllocFinger( fingerId );
			if( !finger )
				return;
			finger->startX = x;
			finger->startY = y;
			finger->x = x;
			finger->y = y;
			finger->zone = TOUCH_ZONE_CONFIG;
			finger->tapCandidate = qtrue;
			finger->editOffsetX = Touch_EdgeX( in_touchConfigX ) - x;
			finger->editOffsetY = Touch_EdgeY( in_touchConfigY ) - y;
		}
		else if( motion )
		{
			float tapDx = x - finger->startX;
			float tapDy = y - finger->startY;
			float tapThreshold = Touch_TapThreshold();

			if( tapDx * tapDx + tapDy * tapDy > tapThreshold * tapThreshold )
				finger->tapCandidate = qfalse;
			Touch_SetZonePosition( TOUCH_ZONE_CONFIG, x + finger->editOffsetX, y + finger->editOffsetY );
			finger->x = x;
			finger->y = y;
		}
		return;
	}

	finger = Touch_FindFinger( fingerId );
	if( !finger )
		return;

	if( finger->zone == TOUCH_ZONE_CONFIG && finger->tapCandidate )
		IOS_Gamepad_PresentSettings();
	finger->active = qfalse;
}
#endif

static void Touch_TapCommand( const char *command )
{
	Cbuf_AddText( command );
	Cbuf_AddText( "\n" );
}

static float Touch_TapThreshold( void )
{
	return 18.0f * ( touchScale > 0.0f ? touchScale : 1.0f );
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

static void Touch_TapAltFire( void )
{
	Touch_SetHeldKey( &touchAltFireDown, K_MOUSE2, qtrue );
	touchPendingAltFireUpFrames = 2;
}

static qboolean Touch_ConsumeSecondCombatTap( touchFinger_t *releasedFinger )
{
	int i;

	for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
	{
		if( &fingers[i] == releasedFinger )
			continue;
		if( fingers[i].active && fingers[i].tapCandidate )
		{
			fingers[i].tapCandidate = qfalse;
			return qtrue;
		}
	}

	return qfalse;
}

void IN_TouchApplyDefaults( void )
{
	Cvar_Set( "r_mode", "-2" );
	Cvar_Set( "r_fullscreen", "1" );
	Cvar_Set( "in_touchUISensitivity", "1.1" );
	Cvar_Set( "in_touchStickSize", "0.14" );
	Cvar_Set( "in_touchBtnSize", "0.075" );
	Cvar_Set( "in_touchJumpX", "-0.05" );
	Cvar_Set( "in_touchJumpY", "-0.30" );
	Cvar_Set( "in_touchCrouchX", "-0.05" );
	Cvar_Set( "in_touchCrouchY", "-0.05" );
	Cvar_Set( "in_touchUseX", "-0.35" );
	Cvar_Set( "in_touchUseY", "0.10" );
	Cvar_Set( "in_touchOpenX", "-0.25" );
	Cvar_Set( "in_touchOpenY", "0.10" );
	Cvar_Set( "in_touchWeaponsX", "-0.05" );
	Cvar_Set( "in_touchWeaponsY", "0.10" );
	Cvar_Set( "in_touchConfigX", "0.18" );
	Cvar_Set( "in_touchConfigY", "0.12" );
	Cvar_Set( "in_touchFireX", "-0.15" );
	Cvar_Set( "in_touchFireY", "-0.22" );
	Cvar_Set( "in_touchAltFireX", "-0.15" );
	Cvar_Set( "in_touchAltFireY", "-0.08" );
	Cvar_Set( "in_touchAimMode", "1" );
	Cvar_Set( "in_touchMoveMode", "0" );
	Cvar_Set( "in_touchMoveHoriz", "0" );
	Cvar_Set( "in_touchFireMode", "0" );
}

void IN_TouchInit( void )
{
	in_touch = Cvar_Get( "in_touch", "1", CVAR_ARCHIVE );
	in_touchMoveSensitivity = Cvar_Get( "in_touchMoveSensitivity", "1.0", CVAR_ARCHIVE );
	in_touchSensitivity = Cvar_Get( "in_touchSensitivity", "2.2", CVAR_ARCHIVE );
	in_touchUISensitivity = Cvar_Get( "in_touchUISensitivity", "1.6", CVAR_ARCHIVE );
	in_touchDeadzone = Cvar_Get( "in_touchDeadzone", "0.035", CVAR_ARCHIVE );
	in_touchStickSize = Cvar_Get( "in_touchStickSize", "0.14", CVAR_ARCHIVE );
	in_touchBtnSize = Cvar_Get( "in_touchBtnSize", "0.075", CVAR_ARCHIVE );
	in_touchMoveX = Cvar_Get( "in_touchMoveX", "0.13", CVAR_ARCHIVE );
	in_touchMoveY = Cvar_Get( "in_touchMoveY", "-0.18", CVAR_ARCHIVE );
	in_touchLookX = Cvar_Get( "in_touchLookX", "-0.32", CVAR_ARCHIVE );
	in_touchLookY = Cvar_Get( "in_touchLookY", "-0.18", CVAR_ARCHIVE );
	in_touchJumpX = Cvar_Get( "in_touchJumpX", "-0.24", CVAR_ARCHIVE );
	in_touchJumpY = Cvar_Get( "in_touchJumpY", "-0.11", CVAR_ARCHIVE );
	in_touchCrouchX = Cvar_Get( "in_touchCrouchX", "-0.36", CVAR_ARCHIVE );
	in_touchCrouchY = Cvar_Get( "in_touchCrouchY", "-0.11", CVAR_ARCHIVE );
	in_touchUseX = Cvar_Get( "in_touchUseX", "-0.19", CVAR_ARCHIVE );
	in_touchUseY = Cvar_Get( "in_touchUseY", "0.35", CVAR_ARCHIVE );
	in_touchOpenX = Cvar_Get( "in_touchOpenX", "-0.30", CVAR_ARCHIVE );
	in_touchOpenY = Cvar_Get( "in_touchOpenY", "0.33", CVAR_ARCHIVE );
	in_touchWeaponsX = Cvar_Get( "in_touchWeaponsX", "-0.08", CVAR_ARCHIVE );
	in_touchWeaponsY = Cvar_Get( "in_touchWeaponsY", "0.17", CVAR_ARCHIVE );
	in_touchMenuX = Cvar_Get( "in_touchMenuX", "0.08", CVAR_ARCHIVE );
	in_touchMenuY = Cvar_Get( "in_touchMenuY", "0.12", CVAR_ARCHIVE );
	in_touchConfigX = Cvar_Get( "in_touchConfigX", "0.18", CVAR_ARCHIVE );
	in_touchConfigY = Cvar_Get( "in_touchConfigY", "0.12", CVAR_ARCHIVE );
	in_touchFireX = Cvar_Get( "in_touchFireX", "-0.15", CVAR_ARCHIVE );
	in_touchFireY = Cvar_Get( "in_touchFireY", "-0.22", CVAR_ARCHIVE );
	in_touchAltFireX = Cvar_Get( "in_touchAltFireX", "-0.15", CVAR_ARCHIVE );
	in_touchAltFireY = Cvar_Get( "in_touchAltFireY", "-0.08", CVAR_ARCHIVE );
	in_touchAimMode = Cvar_Get( "in_touchAimMode", "1", CVAR_ARCHIVE );
	in_touchMoveMode = Cvar_Get( "in_touchMoveMode", "0", CVAR_ARCHIVE );
	in_touchMoveHoriz = Cvar_Get( "in_touchMoveHoriz", "0", CVAR_ARCHIVE );
	in_touchFireMode = Cvar_Get( "in_touchFireMode", "0", CVAR_ARCHIVE );
	in_touchDefaultsVersion = Cvar_Get( "in_touchDefaultsVersion", "0", CVAR_ARCHIVE );
	in_touchDebug = Cvar_Get( "in_touchDebug", "0", CVAR_ARCHIVE );
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
#ifdef IOS
	/* With an external display the GL drawable is the external screen, but
	 * touches happen on the device overlay. Size the touch space from the
	 * device layout (in pixels) rather than the external drawable. */
	if( IOS_Layer_HasExternalScreen() )
	{
		touchScale = layout.scale > 0.0f ? layout.scale : touchScale;
		touchWidth = screenWidth > 0 ? screenWidth : touchWidth;
		touchHeight = screenHeight > 0 ? screenHeight : touchHeight;
		return;
	}
#endif
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

	touchUICursorX += dx;
	touchUICursorY += dy;
	if( touchUICursorX < 0.0f )
		touchUICursorX = 0.0f;
	else if( touchUICursorX > 640.0f )
		touchUICursorX = 640.0f;
	if( touchUICursorY < 0.0f )
		touchUICursorY = 0.0f;
	else if( touchUICursorY > 480.0f )
		touchUICursorY = 480.0f;

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

#ifdef IOS
	Touch_ApplyGamepadMode( Touch_GamepadOverridesGameplay() );
	if( Touch_GamepadOverridesGameplay() && !touchEditMode )
	{
		Touch_HandleGamepadConfigFinger( fingerId, x, y, down, motion );
		return;
	}

	/* Hardware keyboard+mouse on an external display: gameplay is driven by the
	 * mouse and keyboard, so the device surface is inert in-game. In menus it
	 * still works as a relative trackpad for the cursor. */
	if( Touch_ExternalKeyboardMouse() && !IN_TouchInUIMode() )
		return;
#endif

	if( IN_TouchInUIMode() )
	{
		if( down )
		{
			if( touchUIPointerFinger < 0 || touchUIPointerFinger == fingerId )
			{
				touchUIPointerFinger = fingerId;
				IN_TouchUIMouse( x, y, qtrue, motion );
			}
			else
			{
				touchUITapCandidate = qfalse;
				if( touchUIRightFinger < 0 || touchUIRightFinger == fingerId )
				{
					touchUIRightFinger = fingerId;
					IN_TouchUIRightClick( qtrue );
				}
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
				else if( Touch_IsButtonZone( finger->zone ) || Touch_IsStickZone( finger->zone ) )
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
			if( !Touch_FireButtonsMode() )
				finger->tapCandidate = finger->zone == TOUCH_ZONE_LOOK || finger->zone == TOUCH_ZONE_MOVE;
			else
				finger->tapCandidate = qfalse;

			if( finger->zone == TOUCH_ZONE_FIRE )
				Touch_SetHeldKey( &touchFireDown, K_MOUSE1, qtrue );
			else if( finger->zone == TOUCH_ZONE_ALT_FIRE )
				Touch_SetHeldKey( &touchAltFireDown, K_MOUSE2, qtrue );
			else if( finger->zone == TOUCH_ZONE_JUMP )
				Touch_SetHeldCommand( &touchJumpDown, "+moveup", "-moveup", qtrue );
			else if( finger->zone == TOUCH_ZONE_CROUCH )
				Touch_SetHeldCommand( &touchCrouchDown, "+movedown", "-movedown", qtrue );
			else if( finger->zone == TOUCH_ZONE_USE )
				Touch_SetHeldCommand( &touchUseDown, "+use", "-use", qtrue );
			else if( finger->zone == TOUCH_ZONE_OPEN )
				Touch_SetHeldCommand( &touchOpenDown, "+zoom", "-zoom", qtrue );
			else if( finger->zone == TOUCH_ZONE_MENU )
				Cbuf_AddText( "togglemenu\n" );
			else if( finger->zone == TOUCH_ZONE_CONFIG )
				finger->tapCandidate = qtrue;
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
			if( Touch_MoveHorizTurn() && Touch_MoveStickMode() )
			{
				Touch_SetHeldCommand( &touchTurnRightDown, "+right", "-right", dx > dead );
				Touch_SetHeldCommand( &touchTurnLeftDown, "+left", "-left", dx < -dead );
			}
			else
			{
				Touch_SetHeldCommand( &touchMoveRightDown, "+moveright", "-moveright", dx > dead );
				Touch_SetHeldCommand( &touchMoveLeftDown, "+moveleft", "-moveleft", dx < -dead );
			}
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

		if( finger->zone == TOUCH_ZONE_WEAPONS )
			Touch_TapCommand( "weapnext" );
		else if( finger->zone == TOUCH_ZONE_CONFIG && finger->tapCandidate )
			Touch_OpenConfig();
		else if( finger->tapCandidate && !Touch_FireButtonsMode() )
		{
			if( Touch_ConsumeSecondCombatTap( finger ) )
				Touch_TapAltFire();
			else
				Touch_TapFire();
		}

		Touch_StopZone( finger->zone );
		if( finger->zone == TOUCH_ZONE_MOVE )
		{
			Touch_SetHeldCommand( &touchMoveRightDown, "+moveright", "-moveright", qfalse );
			Touch_SetHeldCommand( &touchMoveLeftDown, "+moveleft", "-moveleft", qfalse );
			Touch_SetHeldCommand( &touchMoveBackDown, "+back", "-back", qfalse );
			Touch_SetHeldCommand( &touchMoveForwardDown, "+forward", "-forward", qfalse );
			Touch_SetHeldCommand( &touchTurnRightDown, "+right", "-right", qfalse );
			Touch_SetHeldCommand( &touchTurnLeftDown, "+left", "-left", qfalse );
		}

		finger->active = qfalse;
		if( !touchEditMode )
			touchMode = TOUCH_MODE_COMBAT;
	}
}

void IN_TouchFrame( void )
{
	if( !in_touch || !in_touch->integer )
		return;
	if( touchPendingFireUpFrames > 0 && --touchPendingFireUpFrames == 0 )
		Touch_SetHeldKey( &touchFireDown, K_MOUSE1, qfalse );
	if( touchPendingAltFireUpFrames > 0 && --touchPendingAltFireUpFrames == 0 )
		Touch_SetHeldKey( &touchAltFireDown, K_MOUSE2, qfalse );
	if( touchUIPendingLeftUpFrames > 0 && --touchUIPendingLeftUpFrames == 0 )
		Com_QueueEvent( 0, SE_KEY, K_MOUSE1, qfalse, 0, NULL );
	if( !IN_TouchInUIMode() && ( touchUIPointerFinger >= 0 || touchUIRightFinger >= 0 || touchUIRightDown ) )
		IN_TouchUIReset();
#ifdef IOS
	if( !Touch_GamepadOverridesGameplay() )
	{
		int i;
		qboolean moveEngaged = qfalse;
		for( i = 0; i < TOUCH_MAX_FINGERS; i++ )
		{
			if( fingers[i].active && fingers[i].zone == TOUCH_ZONE_MOVE )
			{
				moveEngaged = qtrue;
				break;
			}
		}
		IOS_Gamepad_SetOnScreenMoveEngaged( moveEngaged );
	}
	else
	{
		IOS_Gamepad_SetOnScreenMoveEngaged( qfalse );
	}
#endif
	(void)in_touchMoveSensitivity;
	(void)in_touchLookX;
	(void)in_touchLookY;
	(void)in_touchDebug;
}

static float Touch_To640X( float x )
{
	return x * 640.0f / (float)( touchWidth > 0 ? touchWidth : 1 );
}

static float Touch_To480Y( float y )
{
	return y * 480.0f / (float)( touchHeight > 0 ? touchHeight : 1 );
}

static void Touch_DrawRectPx( float x, float y, float w, float h, const float *color )
{
	re.SetColor( color );
	re.DrawStretchPic( x, y, w, h, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );
}

static void Touch_DrawBorderPx( float cx, float cy, float radius, float thickness, const float *color )
{
	float x = cx - radius;
	float y = cy - radius;
	float size = radius * 2.0f;

	Touch_DrawRectPx( x, y, size, thickness, color );
	Touch_DrawRectPx( x, y + size - thickness, size, thickness, color );
	Touch_DrawRectPx( x, y, thickness, size, color );
	Touch_DrawRectPx( x + size - thickness, y, thickness, size, color );
}

static void Touch_DrawLabelPx( float cx, float cy, const char *label, const float *color )
{
	int x = (int)Touch_To640X( cx ) - (int)strlen( label ) * 4;
	int y = (int)Touch_To480Y( cy ) - 4;

	SCR_DrawSmallStringExt( x, y, label, (float *)color, qtrue, qtrue );
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

static float Touch_Opacity( void )
{
	float alpha = in_touchOpacity ? in_touchOpacity->value : 0.34f;

	if( alpha < 0.05f )
		alpha = 0.05f;
	if( alpha > 0.85f )
		alpha = 0.85f;
	return alpha;
}

static void Touch_DrawButton( float cx, float cy, float radius, const char *label, qboolean active, const float *rgb )
{
	vec4_t fill;
	vec4_t border;
	float alpha = Touch_Opacity();

	fill[0] = rgb[0];
	fill[1] = rgb[1];
	fill[2] = rgb[2];
	fill[3] = active ? alpha + 0.22f : alpha;
	if( fill[3] > 0.92f )
		fill[3] = 0.92f;

	border[0] = 1.0f;
	border[1] = 1.0f;
	border[2] = 1.0f;
	border[3] = active ? 0.72f : 0.42f;

	Touch_DrawRectPx( cx - radius * 0.58f, cy - radius * 0.58f, radius * 1.16f, radius * 1.16f, fill );
	Touch_DrawBorderPx( cx, cy, radius, radius * 0.10f, border );
	Touch_DrawLabelPx( cx, cy, label, border );
}

void IN_TouchDraw( void )
{
	float stick;
	float button;
	float mx, my, lx, ly, fx, fy, ax, ay;
	float jx, jy, cx, cy, ex, ey, ox, oy, wx, wy, ux, uy, gx, gy;
	float moveStickRadius = 0.0f;
	float lookStickRadius = 0.0f;
	float fireRadius = 0.0f;
	float altFireRadius = 0.0f;
	qboolean hideControls;

	if( !in_touch || !in_touch->integer )
	{
		IOS_Layer_SetTouchGamepadMode( qfalse );
		IOS_Layer_HideTouchControls( touchMode );
		return;
	}
	if( IN_TouchInUIMode() || clc.state != CA_ACTIVE )
	{
		IOS_Layer_SetTouchGamepadMode( qfalse );
		IOS_Layer_HideTouchControls( touchMode );
		return;
	}

#ifdef IOS
	Touch_ApplyGamepadMode( Touch_GamepadOverridesGameplay() );
#endif

#ifdef IOS
	if( Touch_GamepadOverridesGameplay() && !touchEditMode )
	{
		float button = in_touchBtnSize->value * Touch_ControlBase();
		float gx = Touch_EdgeX( in_touchConfigX );
		float gy = Touch_EdgeY( in_touchConfigY );

		if( button < 48.0f )
			button = 48.0f;

		IOS_Layer_SetTouchGamepadMode( qtrue );
		IOS_Layer_UpdateTouchControlsGamepadOnly( Touch_Opacity(), touchMode,
			gx, gy, button, Touch_ActiveFingerForZone( TOUCH_ZONE_CONFIG ) != NULL );
		return;
	}
	IOS_Layer_SetTouchGamepadMode( qfalse );

	/* Hardware keyboard+mouse on an external display: no on-screen controls
	 * (checked after the gamepad case so the PAD config button still shows
	 * when a gamepad is also connected). */
	if( Touch_ExternalKeyboardMouse() )
	{
		IOS_Layer_HideTouchControls( touchMode );
		return;
	}
#endif

	stick = in_touchStickSize->value * Touch_ControlBase();
	button = in_touchBtnSize->value * Touch_ControlBase();
	if( stick < 72.0f )
		stick = 72.0f;
	if( button < 48.0f )
		button = 48.0f;

	mx = Touch_EdgeX( in_touchMoveX );
	my = Touch_EdgeY( in_touchMoveY );
	lx = Touch_EdgeX( in_touchLookX );
	ly = Touch_EdgeY( in_touchLookY );
	fx = Touch_EdgeX( in_touchFireX );
	fy = Touch_EdgeY( in_touchFireY );
	ax = Touch_EdgeX( in_touchAltFireX );
	ay = Touch_EdgeY( in_touchAltFireY );

	if( touchEditMode || Touch_MoveStickMode() )
		moveStickRadius = stick;
	if( touchEditMode || Touch_AimStickMode() )
		lookStickRadius = stick;
	if( Touch_FireButtonsMode() )
	{
		fireRadius = button;
		altFireRadius = button;
	}
	hideControls = Touch_HideControlsForHardwareInput();
	if( hideControls )
	{
		fireRadius = 0.0f;
		altFireRadius = 0.0f;
		button = 0.0f;
	}
	jx = Touch_EdgeX( in_touchJumpX );
	jy = Touch_EdgeY( in_touchJumpY );
	cx = Touch_EdgeX( in_touchCrouchX );
	cy = Touch_EdgeY( in_touchCrouchY );
	ex = Touch_EdgeX( in_touchUseX );
	ey = Touch_EdgeY( in_touchUseY );
	ox = Touch_EdgeX( in_touchOpenX );
	oy = Touch_EdgeY( in_touchOpenY );
	wx = Touch_EdgeX( in_touchWeaponsX );
	wy = Touch_EdgeY( in_touchWeaponsY );
	ux = Touch_EdgeX( in_touchMenuX );
	uy = Touch_EdgeY( in_touchMenuY );
	gx = Touch_EdgeX( in_touchConfigX );
	gy = Touch_EdgeY( in_touchConfigY );

	IOS_Layer_UpdateTouchControls( qtrue, Touch_Opacity(), touchMode,
		mx, my, moveStickRadius, Touch_ActiveFingerForZone( TOUCH_ZONE_MOVE ) != NULL,
		lx, ly, lookStickRadius, Touch_ActiveFingerForZone( TOUCH_ZONE_LOOK ) != NULL,
		fx, fy, fireRadius, touchFireDown,
		ax, ay, altFireRadius, touchAltFireDown,
		jx, jy, button, touchJumpDown,
		cx, cy, button, touchCrouchDown,
		ex, ey, button, touchUseDown,
		ox, oy, button, touchOpenDown,
		wx, wy, button,
		ux, uy, button,
		gx, gy, button, Touch_ActiveFingerForZone( TOUCH_ZONE_CONFIG ) != NULL,
		touchEditMode, Touch_SliderX(), Touch_SliderY(), Touch_SliderW(),
		Touch_SizeSliderValue() );
}
