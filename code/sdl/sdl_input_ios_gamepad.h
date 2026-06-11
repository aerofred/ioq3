#ifndef SDL_INPUT_IOS_GAMEPAD_H
#define SDL_INPUT_IOS_GAMEPAD_H

#include "../qcommon/q_shared.h"

void IN_IosGamepadInit( void );
void IN_IosGamepadShutdown( void );
void IN_IosGamepadFrame( void );
void IN_IosRefreshJoystick( qboolean openDevice );
void IN_IosCloseJoystick( void );
int Sys_SDLGamepadOpened( void );
int Sys_SDLJoystickCount( void );
void IN_IosDebugPadState( char *buf, int bufsize );

#endif
