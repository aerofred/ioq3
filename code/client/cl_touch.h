#ifndef CL_TOUCH_H
#define CL_TOUCH_H

#include "../qcommon/q_shared.h"

void IN_TouchInit( void );
void IN_TouchShutdown( void );
void IN_TouchFrame( void );
void IN_TouchDraw( void );
void IN_TouchSyncLayout( int width, int height, float scale );
void IN_TouchFinger( long long fingerId, float x, float y, qboolean down, qboolean motion );
void IN_TouchMouse( int x, int y, qboolean down, qboolean motion );
qboolean IN_TouchInUIMode( void );
qboolean IN_TouchConsoleActive( void );
void IN_TouchToggleConsole( void );
void IN_TouchApplyDefaults( void );

#endif
