#ifndef IOS_GAMEPAD_H
#define IOS_GAMEPAD_H

#include "../qcommon/q_shared.h"

void IOS_Gamepad_PrepareAtLaunch( void );
void IOS_Gamepad_Start( void );
void IOS_Gamepad_Stop( void );
void IOS_Gamepad_PauseForOverlay( void );
void IOS_Gamepad_PresentSettings( void );
void IOS_Touch_PresentSettings( void );
void IOS_Gamepad_ApplyLaunchConfig( char *commandLine, int commandLineSize );
void IOS_Gamepad_SetOnScreenMoveEngaged( qboolean engaged );
qboolean IOS_Gamepad_IsConfigCaptureActive( void );
void IOS_Gamepad_SetConfigCaptureActive( qboolean active );
void IOS_Gamepad_ReloadKeyNumbers( void );
qboolean IOS_Gamepad_IsActive( void );
void IOS_Gamepad_SetNativePresent( qboolean present );
void IOS_Gamepad_NotifyDeviceChange( void );

#endif
