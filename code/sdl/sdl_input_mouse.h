#ifndef SDL_INPUT_MOUSE_H
#define SDL_INPUT_MOUSE_H

#include <SDL.h>
#include "../qcommon/q_shared.h"

void IN_Mouse_Init( void );
void IN_Mouse_Shutdown( void );
void IN_Mouse_SetEventTime( int eventTime );
qboolean IN_Mouse_ProcessEvent( SDL_Event *event );
void IN_Mouse_UpdateGrab( void );

#endif
