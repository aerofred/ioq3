#include <SDL.h>

#include "../client/client.h"
#include "../client/cl_touch.h"
#include "sdl_input_mouse.h"

static int in_mouseEventTime = 0;
static int in_mouseLastX = -1;
static int in_mouseLastY = -1;
static qboolean in_mouseGrabbed = qfalse;

static int IN_Mouse_ButtonToKey( Uint8 button )
{
	switch( button )
	{
		case SDL_BUTTON_LEFT:   return K_MOUSE1;
		case SDL_BUTTON_MIDDLE: return K_MOUSE3;
		case SDL_BUTTON_RIGHT:  return K_MOUSE2;
		case SDL_BUTTON_X1:     return K_MOUSE4;
		case SDL_BUTTON_X2:     return K_MOUSE5;
		default:                return K_AUX1 + ( button - SDL_BUTTON_X2 + 1 ) % 16;
	}
}

static void IN_Mouse_QueueMotion( int dx, int dy )
{
	if( !dx && !dy )
		return;

	Com_QueueEvent( in_mouseEventTime, SE_MOUSE, dx, dy, 0, NULL );
}

static void IN_Mouse_ProcessMotion( SDL_Event *event )
{
	int dx, dy;

	dx = event->motion.xrel;
	dy = event->motion.yrel;

	if( dx || dy )
	{
		in_mouseLastX = event->motion.x;
		in_mouseLastY = event->motion.y;
		IN_Mouse_QueueMotion( dx, dy );
		return;
	}

	if( in_mouseLastX < 0 )
	{
		in_mouseLastX = event->motion.x;
		in_mouseLastY = event->motion.y;
		return;
	}

	dx = event->motion.x - in_mouseLastX;
	dy = event->motion.y - in_mouseLastY;
	in_mouseLastX = event->motion.x;
	in_mouseLastY = event->motion.y;
	IN_Mouse_QueueMotion( dx, dy );
}

void IN_Mouse_Init( void )
{
	in_mouseLastX = -1;
	in_mouseLastY = -1;
	in_mouseGrabbed = qfalse;
}

void IN_Mouse_Shutdown( void )
{
	if( in_mouseGrabbed )
	{
		SDL_SetRelativeMouseMode( SDL_FALSE );
		in_mouseGrabbed = qfalse;
	}
	in_mouseLastX = -1;
	in_mouseLastY = -1;
}

void IN_Mouse_SetEventTime( int eventTime )
{
	in_mouseEventTime = eventTime;
}

qboolean IN_Mouse_ProcessEvent( SDL_Event *event )
{
	int button;

	switch( event->type )
	{
		case SDL_MOUSEMOTION:
			IN_Mouse_ProcessMotion( event );
			return qtrue;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			button = IN_Mouse_ButtonToKey( event->button.button );
			Com_QueueEvent( in_mouseEventTime, SE_KEY, button,
				event->type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse, 0, NULL );
			return qtrue;

		case SDL_MOUSEWHEEL:
			if( event->wheel.y > 0 )
			{
				Com_QueueEvent( in_mouseEventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
				Com_QueueEvent( in_mouseEventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
			}
			else if( event->wheel.y < 0 )
			{
				Com_QueueEvent( in_mouseEventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
				Com_QueueEvent( in_mouseEventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
			}
			return qtrue;

		default:
			return qfalse;
	}
}

void IN_Mouse_UpdateGrab( void )
{
	qboolean wantGrab = !IN_TouchInUIMode();

	if( wantGrab == in_mouseGrabbed )
		return;

	SDL_SetRelativeMouseMode( wantGrab ? SDL_TRUE : SDL_FALSE );
	in_mouseGrabbed = wantGrab;

	if( !wantGrab )
	{
		in_mouseLastX = -1;
		in_mouseLastY = -1;
	}
}
