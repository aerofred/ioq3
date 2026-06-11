#ifndef CL_TOUCH_H
#define CL_TOUCH_H

#include "../qcommon/q_shared.h"

typedef enum
{
	TOUCH_MODE_COMBAT,
	TOUCH_MODE_RADIAL,
	TOUCH_MODE_UTILITY,
	TOUCH_MODE_CURSOR
} touchMode_t;

typedef enum
{
	TOUCH_ZONE_NONE,
	TOUCH_ZONE_MOVE,
	TOUCH_ZONE_LOOK,
	TOUCH_ZONE_FIRE,
	TOUCH_ZONE_ALT_FIRE,
	TOUCH_ZONE_JUMP,
	TOUCH_ZONE_CROUCH,
	TOUCH_ZONE_USE,
	TOUCH_ZONE_RELOAD,
	TOUCH_ZONE_OPEN,
	TOUCH_ZONE_BUY,
	TOUCH_ZONE_WEAPONS,
	TOUCH_ZONE_MENU,
	TOUCH_ZONE_CONFIG,
	TOUCH_ZONE_SIZE_SLIDER
} touchZone_t;

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
void IN_TouchEnterEditMode( void );
qboolean IN_TouchEditModeActive( void );

#endif
