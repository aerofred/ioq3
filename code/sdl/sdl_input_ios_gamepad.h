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

/*
 * Translate gamepad D-Pad / A / B into menu navigation keys (arrows / Enter /
 * Escape) while an external display is connected and a UI catcher is active.
 * Shared by the SDL and native GameController paths. Pass the current button
 * states each frame; edge transitions are detected internally. When menu
 * navigation is not applicable it releases any keys it injected.
 */
void IN_IosGamepadMenuNav( qboolean up, qboolean down, qboolean left,
	qboolean right, qboolean accept, qboolean cancel );

#endif
