#include <SDL.h>

#include "../client/client.h"
#include "../client/cl_touch.h"
#include "../ios/ios_layer.h"
#include "../ios/ios_gamepad.h"
#include "../sdl/sdl_input_ios_gamepad.h"
#include "../sdl/sdl_input_keyboard.h"
#include "../sdl/sdl_input_mouse.h"
#include "../sys/sys_local.h"

static qboolean inputInited = qfalse;
static void *in_windowData = NULL;

static void IN_IosRegisterCommands( void );

static void IN_ProcessEvent( SDL_Event *event )
{
	switch( event->type )
	{
		case SDL_FINGERDOWN:
			IN_TouchFinger( event->tfinger.fingerId, event->tfinger.x, event->tfinger.y, qtrue, qfalse );
			break;
		case SDL_FINGERMOTION:
			IN_TouchFinger( event->tfinger.fingerId, event->tfinger.x, event->tfinger.y, qtrue, qtrue );
			break;
		case SDL_FINGERUP:
			IN_TouchFinger( event->tfinger.fingerId, event->tfinger.x, event->tfinger.y, qfalse, qfalse );
			break;
		case SDL_CONTROLLERAXISMOTION:
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			/* Axes et boutons gérés par IN_PadMove (binds PAD0_*). */
			break;
		case SDL_JOYAXISMOTION:
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
		case SDL_JOYHATMOTION:
			/* Joystick brut : IN_PadMove poll quand la manette SDL est ouverte. */
			break;
		case SDL_CONTROLLERDEVICEADDED:
			IN_IosRefreshJoystick( qfalse );
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			IN_IosCloseJoystick();
			break;
		case SDL_APP_WILLENTERBACKGROUND:
		case SDL_APP_DIDENTERBACKGROUND:
			IOS_Layer_SetActive( qfalse );
			Cvar_Set( "com_minimized", "1" );
			Cvar_Set( "com_unfocused", "1" );
			Cvar_Set( "s_muted", "1" );
			IOS_Gamepad_PauseForOverlay();
			break;
		case SDL_APP_WILLENTERFOREGROUND:
		case SDL_APP_DIDENTERFOREGROUND:
			IOS_Layer_SetActive( qtrue );
			Cvar_Set( "com_minimized", "0" );
			Cvar_Set( "com_unfocused", "0" );
			Cvar_Set( "s_muted", "0" );
			break;
		case SDL_QUIT:
			Cbuf_AddText( "quit\n" );
			break;
	}
}

void IN_Frame( void )
{
	SDL_Event event;

	IN_Keyboard_SetEventTime( Sys_Milliseconds() );
	IN_Mouse_SetEventTime( Sys_Milliseconds() );

	while( SDL_PollEvent( &event ) )
	{
		if( IN_Keyboard_ProcessEvent( &event ) )
			continue;
		if( IN_Mouse_ProcessEvent( &event ) )
			continue;
		IN_ProcessEvent( &event );
	}

	IN_Keyboard_UpdateTextInput();
	IN_Mouse_UpdateGrab();
	IN_IosGamepadFrame();
	IN_TouchFrame();
}

void IN_InitKeyLockStates( void )
{
}

void IN_Init( void *windowData )
{
	if( inputInited )
		return;

	in_windowData = windowData;

	SDL_SetHint( SDL_HINT_TOUCH_MOUSE_EVENTS, "0" );
	SDL_SetHint( SDL_HINT_MOUSE_TOUCH_EVENTS, "0" );
	SDL_InitSubSystem( SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK );
	IN_Keyboard_Init();
	IN_Mouse_Init();
	IN_TouchInit();
	IN_IosGamepadInit();
	IN_IosRegisterCommands();
	inputInited = qtrue;
}

void IN_Shutdown( void )
{
	if( !inputInited )
		return;
	IN_Keyboard_Shutdown();
	IN_Mouse_Shutdown();
	IN_IosGamepadShutdown();
	IN_TouchShutdown();
	SDL_QuitSubSystem( SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK );
	in_windowData = NULL;
	inputInited = qfalse;
}

void IN_Restart( void )
{
	IN_Shutdown();
	IN_Init( in_windowData );
}

static void IOS_GamepadSettings_f( void )
{
	IOS_Gamepad_PresentSettings();
}

static void IN_IosRegisterCommands( void )
{
	Cmd_AddCommand( "gamepad_config", IOS_GamepadSettings_f );
}
