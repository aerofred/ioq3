#include "sdl_input_ios_gamepad.h"

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "../sys/sys_local.h"
#include "../ios/ios_gamepad.h"
#include "../ios/ios_gamepad_look.h"

static SDL_Joystick *stick = NULL;
static SDL_GameController *gamepad = NULL;

static cvar_t *in_joystick = NULL;
static cvar_t *in_joystickThreshold = NULL;
static cvar_t *in_joystickNo = NULL;
static cvar_t *in_joystickUseAnalog = NULL;

static struct
{
	qboolean buttons[SDL_CONTROLLER_BUTTON_MAX + 1];
	int oldaaxes[MAX_JOYSTICK_AXIS];
} stick_state;

static int iosPadStickDir[4];
static Sint16 iosJoyAxisCenter[MAX_JOYSTICK_AXIS];

static qboolean IN_IosNameIsAccelerometer( const char *name )
{
	return ( name && Q_stristr( name, "accelerometer" ) != NULL );
}

static void IN_IosEnsureJoystickHints( void )
{
	SDL_SetHint( SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0" );
}

static int IN_IosPickJoystickIndex( int total )
{
	int i, best = -1, bestScore = -1;
	const char *name;

	for ( i = 0; i < total; i++ ) {
		name = SDL_JoystickNameForIndex( i );
		if ( IN_IosNameIsAccelerometer( name ) ) {
			continue;
		}

		if ( SDL_IsGameController( i ) ) {
			return i;
		}

		if ( name ) {
			if ( Q_stristr( name, "controller" ) || Q_stristr( name, "dualshock" ) ||
				Q_stristr( name, "wireless" ) || Q_stristr( name, "gamepad" ) ||
				Q_stristr( name, "gamesir" ) || Q_stristr( name, "xbox" ) ) {
				if ( bestScore < 50 ) {
					bestScore = 50;
					best = i;
				}
			} else if ( best < 0 ) {
				best = i;
				bestScore = 1;
			}
		} else if ( best < 0 ) {
			best = i;
			bestScore = 1;
		}
	}

	return best;
}

static void IN_IosReleaseDigitalStickKeys( void )
{
	static const int negKeys[4] = {
		K_PAD0_LEFTSTICK_LEFT, K_PAD0_LEFTSTICK_UP,
		K_PAD0_RIGHTSTICK_LEFT, K_PAD0_RIGHTSTICK_UP
	};
	static const int posKeys[4] = {
		K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN,
		K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN
	};
	int i;

	for ( i = 0; i < 4; i++ ) {
		if ( iosPadStickDir[i] < 0 ) {
			Com_QueueEvent( 0, SE_KEY, negKeys[i], qfalse, 0, NULL );
		}
		if ( iosPadStickDir[i] > 0 ) {
			Com_QueueEvent( 0, SE_KEY, posKeys[i], qfalse, 0, NULL );
		}
		iosPadStickDir[i] = 0;
	}
}

static void IN_IosCalibratePadAxes( void )
{
	int i, n;

	if ( !stick ) {
		return;
	}

	SDL_GameControllerUpdate();
	SDL_JoystickUpdate();

	Com_Memset( iosJoyAxisCenter, 0, sizeof( iosJoyAxisCenter ) );
	n = SDL_JoystickNumAxes( stick );
	if ( n > MAX_JOYSTICK_AXIS ) {
		n = MAX_JOYSTICK_AXIS;
	}
	for ( i = 0; i < n; i++ ) {
		iosJoyAxisCenter[i] = SDL_JoystickGetAxis( stick, i );
	}

	IN_IosReleaseDigitalStickKeys();
}

static Sint16 IN_IosGetPadAxisFromJoystick( int joyAxis )
{
	Sint16 raw;

	if ( !stick || joyAxis < 0 || joyAxis >= SDL_JoystickNumAxes( stick ) ) {
		return 0;
	}
	if ( joyAxis >= MAX_JOYSTICK_AXIS ) {
		return 0;
	}

	raw = SDL_JoystickGetAxis( stick, joyAxis );
	return raw - iosJoyAxisCenter[joyAxis];
}

static Sint16 IN_IosGetPadAxis( SDL_GameControllerAxis axis )
{
	Sint16 v = 0;
	int joyAxis = -1;

	if ( gamepad ) {
		v = SDL_GameControllerGetAxis( gamepad, axis );

		if ( stick ) {
			SDL_GameControllerButtonBind bind =
				SDL_GameControllerGetBindForAxis( gamepad, axis );

			if ( bind.bindType == SDL_CONTROLLER_BINDTYPE_AXIS ) {
				joyAxis = bind.value.axis;
			}
		}
	}

	if ( joyAxis < 0 ) {
		joyAxis = (int)axis;
	}

	if ( stick && joyAxis >= 0 ) {
		Sint16 jv = IN_IosGetPadAxisFromJoystick( joyAxis );

		if ( abs( jv ) > abs( v ) + 400 || ( v == 0 && jv != 0 ) ) {
			v = jv;
		}
	}

	return v;
}

static Sint16 IN_IosGetPadTrigger( SDL_GameControllerAxis axis )
{
	Sint16 v = 0;
	int joyAxis = -1;

	if ( gamepad ) {
		v = SDL_GameControllerGetAxis( gamepad, axis );

		if ( stick ) {
			SDL_GameControllerButtonBind bind =
				SDL_GameControllerGetBindForAxis( gamepad, axis );

			if ( bind.bindType == SDL_CONTROLLER_BINDTYPE_AXIS ) {
				joyAxis = bind.value.axis;
			}
		}
	}

	if ( joyAxis < 0 ) {
		joyAxis = (int)axis;
	}

	if ( stick && joyAxis >= 0 && joyAxis < SDL_JoystickNumAxes( stick ) ) {
		Sint16 jv = SDL_JoystickGetAxis( stick, joyAxis );

		if ( jv > v ) {
			v = jv;
		}
	}

	return v;
}

static void IN_IosPadAxisDigitalKeys( Sint16 raw, float threshold, int negKey, int posKey, int *dirState )
{
	int dir = 0;
	float norm = (float)raw / 32767.0f;

	if ( norm > threshold ) {
		dir = 1;
	} else if ( norm < -threshold ) {
		dir = -1;
	}

	if ( *dirState == dir ) {
		return;
	}

	if ( *dirState < 0 ) {
		Com_QueueEvent( 0, SE_KEY, negKey, qfalse, 0, NULL );
	}
	if ( *dirState > 0 ) {
		Com_QueueEvent( 0, SE_KEY, posKey, qfalse, 0, NULL );
	}
	if ( dir < 0 ) {
		Com_QueueEvent( 0, SE_KEY, negKey, qtrue, 0, NULL );
	}
	if ( dir > 0 ) {
		Com_QueueEvent( 0, SE_KEY, posKey, qtrue, 0, NULL );
	}

	*dirState = dir;
}

static void IN_IosReleaseRightStickDigitalKeys( void )
{
	static const int negKeys[2] = {
		K_PAD0_RIGHTSTICK_LEFT, K_PAD0_RIGHTSTICK_UP
	};
	static const int posKeys[2] = {
		K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN
	};
	int i;

	for ( i = 0; i < 2; i++ ) {
		if ( iosPadStickDir[i + 2] < 0 ) {
			Com_QueueEvent( 0, SE_KEY, negKeys[i], qfalse, 0, NULL );
		}
		if ( iosPadStickDir[i + 2] > 0 ) {
			Com_QueueEvent( 0, SE_KEY, posKeys[i], qfalse, 0, NULL );
		}
		iosPadStickDir[i + 2] = 0;
	}
}

static qboolean IN_IosPadDpadActive( void )
{
	return stick_state.buttons[SDL_CONTROLLER_BUTTON_DPAD_UP] ||
		stick_state.buttons[SDL_CONTROLLER_BUTTON_DPAD_DOWN] ||
		stick_state.buttons[SDL_CONTROLLER_BUTTON_DPAD_LEFT] ||
		stick_state.buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT];
}

static void IN_IosPadMoveDigitalSticks( void )
{
	float moveThresh = 0.18f;
	float rx;
	float ry;
	Sint16 rawRx;
	Sint16 rawRy;
	Sint16 rawLx;
	Sint16 rawLy;

	if ( in_joystickThreshold && in_joystickThreshold->value > 0.01f ) {
		moveThresh = in_joystickThreshold->value;
	}
	if ( moveThresh < 0.18f ) {
		moveThresh = 0.18f;
	}

	rawLx = IN_IosPadDpadActive() ? 0 : IN_IosGetPadAxis( SDL_CONTROLLER_AXIS_LEFTX );
	rawLy = IN_IosPadDpadActive() ? 0 : IN_IosGetPadAxis( SDL_CONTROLLER_AXIS_LEFTY );

	IN_IosPadAxisDigitalKeys(
		rawLx, moveThresh, K_PAD0_LEFTSTICK_LEFT, K_PAD0_LEFTSTICK_RIGHT, &iosPadStickDir[0] );
	IN_IosPadAxisDigitalKeys(
		rawLy, moveThresh, K_PAD0_LEFTSTICK_UP, K_PAD0_LEFTSTICK_DOWN, &iosPadStickDir[1] );

	IN_IosReleaseRightStickDigitalKeys();
	rawRx = IN_IosGetPadAxis( SDL_CONTROLLER_AXIS_RIGHTX );
	rawRy = IN_IosGetPadAxis( SDL_CONTROLLER_AXIS_RIGHTY );
	rx = (float)rawRx / 32767.0f;
	ry = (float)rawRy / 32767.0f;
	IOS_Gamepad_LookFromStick( rx, ry );

	{
		static int ltDown, rtDown;
		qboolean lt = ( IN_IosGetPadTrigger( SDL_CONTROLLER_AXIS_TRIGGERLEFT ) > 16384 );
		qboolean rt = ( IN_IosGetPadTrigger( SDL_CONTROLLER_AXIS_TRIGGERRIGHT ) > 16384 );

		if ( lt != ltDown ) {
			Com_QueueEvent( 0, SE_KEY, K_PAD0_LEFTTRIGGER, lt, 0, NULL );
			ltDown = lt;
		}
		if ( rt != rtDown ) {
			Com_QueueEvent( 0, SE_KEY, K_PAD0_RIGHTTRIGGER, rt, 0, NULL );
			rtDown = rt;
		}
	}
}

static void IN_PadMove( void )
{
	int i;

	if ( !gamepad && !stick ) {
		return;
	}

	if ( Sys_NativeGamepadActive() ) {
		return;
	}

	if ( gamepad ) {
		SDL_GameControllerUpdate();
	} else {
		SDL_JoystickUpdate();
	}

	for ( i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ ) {
		qboolean pressed = qfalse;

		if ( gamepad ) {
			pressed = SDL_GameControllerGetButton( gamepad, (SDL_GameControllerButton)i );
		} else if ( i < SDL_JoystickNumButtons( stick ) ) {
			pressed = ( SDL_JoystickGetButton( stick, i ) != 0 );
		}

		if ( pressed != stick_state.buttons[i] ) {
			Com_QueueEvent( 0, SE_KEY, K_PAD0_A + i, pressed, 0, NULL );
			stick_state.buttons[i] = pressed;
		}
	}

	if ( gamepad ) {
		IN_IosPadMoveDigitalSticks();
	}
}

static void IN_InitJoystick( void )
{
	int i, total;
	char buf[16384] = "";

	if ( gamepad ) {
		SDL_GameControllerClose( gamepad );
	}
	if ( stick ) {
		SDL_JoystickClose( stick );
	}

	stick = NULL;
	gamepad = NULL;
	Com_Memset( &stick_state, 0, sizeof( stick_state ) );

	if ( !in_joystick ) {
		in_joystick = Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH );
	}
	if ( !in_joystickThreshold ) {
		in_joystickThreshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );
	}

	IN_IosEnsureJoystickHints();

	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) ) {
		if ( SDL_Init( SDL_INIT_JOYSTICK ) != 0 ) {
			Com_DPrintf( "SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError() );
			return;
		}
	}

	if ( !SDL_WasInit( SDL_INIT_GAMECONTROLLER ) ) {
		if ( SDL_Init( SDL_INIT_GAMECONTROLLER ) != 0 ) {
			Com_DPrintf( "SDL_Init(SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError() );
			return;
		}
	}

	total = SDL_NumJoysticks();
	Com_DPrintf( "%d possible joysticks\n", total );

	for ( i = 0; i < total; i++ ) {
		Q_strcat( buf, sizeof( buf ), SDL_JoystickNameForIndex( i ) );
		Q_strcat( buf, sizeof( buf ), "\n" );
	}
	Cvar_Get( "in_availableJoysticks", buf, CVAR_ROM );

	if ( total > 0 && !in_joystick->integer ) {
		Cvar_Set( "in_joystick", "1" );
	}

	for ( i = 0; i < total; i++ ) {
		SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID( i );
		char *mapping = SDL_GameControllerMappingForGUID( guid );

		if ( mapping ) {
			SDL_GameControllerAddMapping( mapping );
			SDL_free( mapping );
		}
	}

	if ( !in_joystick->integer ) {
		Com_DPrintf( "Joystick is not active.\n" );
		return;
	}

	in_joystickNo = Cvar_Get( "in_joystickNo", "0", CVAR_ARCHIVE );
	{
		int pick = IN_IosPickJoystickIndex( total );
		char pickBuf[16];

		if ( pick < 0 ) {
			Com_DPrintf( "IN_InitJoystick: no usable joystick\n" );
			return;
		}

		if ( pick != in_joystickNo->integer ) {
			Com_sprintf( pickBuf, sizeof( pickBuf ), "%d", pick );
			Cvar_Set( "in_joystickNo", pickBuf );
			in_joystickNo = Cvar_Get( "in_joystickNo", pickBuf, CVAR_ARCHIVE );
		}
	}

	in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "0", CVAR_ARCHIVE );

	stick = SDL_JoystickOpen( in_joystickNo->integer );
	if ( !stick ) {
		Com_DPrintf( "No joystick opened: %s\n", SDL_GetError() );
		return;
	}

	if ( SDL_IsGameController( in_joystickNo->integer ) ) {
		gamepad = SDL_GameControllerOpen( in_joystickNo->integer );
	}

	if ( gamepad ) {
		Cvar_Set( "in_joystickUseAnalog", "0" );
		Cvar_Set( "j_side", "0.25" );
		Cvar_Set( "j_yaw_axis", "2" );
		Cvar_Set( "j_yaw", "0" );
		Cvar_Set( "j_pitch_axis", "3" );
		Cvar_Set( "j_pitch", "0" );
		in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "0", CVAR_ARCHIVE );
		IN_IosCalibratePadAxes();
	}

	Com_DPrintf( "Joystick opened: %s (gamepad=%s)\n",
		SDL_JoystickNameForIndex( in_joystickNo->integer ),
		gamepad ? "yes" : "no" );

	SDL_JoystickEventState( SDL_ENABLE );
	SDL_GameControllerEventState( SDL_ENABLE );
	IOS_Gamepad_SetNativePresent( qtrue );
}

static void IN_ShutdownJoystick( void )
{
	if ( gamepad ) {
		SDL_GameControllerClose( gamepad );
		gamepad = NULL;
	}
	if ( stick ) {
		SDL_JoystickClose( stick );
		stick = NULL;
	}
	IN_IosReleaseDigitalStickKeys();
	Com_Memset( &stick_state, 0, sizeof( stick_state ) );
	IOS_Gamepad_NotifyDeviceChange();
}

void IN_IosGamepadInit( void )
{
	in_joystick = Cvar_Get( "in_joystick", "1", CVAR_ARCHIVE|CVAR_LATCH );
	in_joystickThreshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );
	in_joystickNo = Cvar_Get( "in_joystickNo", "0", CVAR_ARCHIVE );
	in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "0", CVAR_ARCHIVE );
	IOS_Gamepad_LookInit();
}

void IN_IosGamepadShutdown( void )
{
	IN_ShutdownJoystick();
}

void IN_IosGamepadFrame( void )
{
	static int lastJoyProbe = 0;
	int now = Sys_Milliseconds();

	if ( !gamepad && !stick ) {
		if ( now - lastJoyProbe > 500 ) {
			lastJoyProbe = now;
			if ( SDL_NumJoysticks() > 0 ) {
				IN_InitJoystick();
			}
		}
		return;
	}

	IN_PadMove();
}

void IN_IosRefreshJoystick( qboolean openDevice )
{
	int total;

	IN_IosEnsureJoystickHints();

	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) ) {
		SDL_Init( SDL_INIT_JOYSTICK );
	}
	if ( !SDL_WasInit( SDL_INIT_GAMECONTROLLER ) ) {
		SDL_Init( SDL_INIT_GAMECONTROLLER );
	}

	total = SDL_NumJoysticks();
	Com_DPrintf( "IN_IosRefreshJoystick: count=%d open=%d\n", total, openDevice );

	if ( total > 0 && openDevice ) {
		if ( !com_fullyInitialized ) {
			Com_DPrintf( "IN_IosRefreshJoystick: defer open (engine not ready)\n" );
			return;
		}
		IN_InitJoystick();

		if ( gamepad || stick ) {
			int a;

			for ( a = 0; a < MAX_JOYSTICK_AXIS; a++ ) {
				CL_JoystickEvent( a, 0, Sys_Milliseconds() );
			}
			if ( gamepad ) {
				IN_IosCalibratePadAxes();
			}
		}
	}
}

void IN_IosCloseJoystick( void )
{
	int a;

	if ( gamepad ) {
		SDL_GameControllerClose( gamepad );
		gamepad = NULL;
	}
	if ( stick ) {
		SDL_JoystickClose( stick );
		stick = NULL;
	}

	Com_Memset( &stick_state, 0, sizeof( stick_state ) );
	IN_IosReleaseDigitalStickKeys();

	for ( a = 0; a < MAX_JOYSTICK_AXIS; a++ ) {
		CL_JoystickEvent( a, 0, Sys_Milliseconds() );
	}

	Cvar_Set( "j_yaw_axis", "0" );
	Cvar_Set( "j_yaw", "1" );
	IOS_Gamepad_NotifyDeviceChange();
}

int Sys_SDLGamepadOpened( void )
{
	if ( gamepad ) {
		return 2;
	}
	if ( stick ) {
		return 1;
	}
	return 0;
}

int Sys_SDLJoystickCount( void )
{
	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) ) {
		return 0;
	}
	return SDL_NumJoysticks();
}

void IN_IosDebugPadState( char *buf, int bufsize )
{
	int rawAxis0 = 0;

	if ( !buf || bufsize < 32 ) {
		return;
	}

	if ( gamepad ) {
		rawAxis0 = IN_IosGetPadAxis( SDL_CONTROLLER_AXIS_LEFTX );
	} else if ( stick && SDL_JoystickNumAxes( stick ) > 0 ) {
		rawAxis0 = SDL_JoystickGetAxis( stick, 0 );
	}

	Com_sprintf( buf, bufsize,
		"gamepad: stick=%s gc=%s catcher=%d state=%d axis0=%d",
		stick ? "yes" : "no",
		gamepad ? "yes" : "no",
		Key_GetCatcher(),
		clc.state,
		rawAxis0 );
}
