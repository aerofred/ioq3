#ifndef SDL_INPUT_KEYBOARD_H
#define SDL_INPUT_KEYBOARD_H

#include <SDL.h>
#include "../qcommon/q_shared.h"

void IN_Keyboard_Init( void );
void IN_Keyboard_Shutdown( void );
void IN_Keyboard_SetEventTime( int eventTime );
qboolean IN_Keyboard_ProcessEvent( SDL_Event *event );
void IN_Keyboard_UpdateTextInput( void );

#endif
