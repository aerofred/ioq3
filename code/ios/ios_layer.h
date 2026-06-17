/*
===========================================================================
iOS overlay/layout hooks.
===========================================================================
*/

#ifndef IOS_LAYER_H
#define IOS_LAYER_H

#include "../qcommon/q_shared.h"

typedef struct iosLayout_s
{
	float width;
	float height;
	float safeLeft;
	float safeTop;
	float safeRight;
	float safeBottom;
	float scale;
} iosLayout_t;

void IOS_Layer_Init( void );
void IOS_Layer_Shutdown( void );
void IOS_Layer_Tick( void );
void IOS_Layer_SetActive( qboolean active );
qboolean IOS_Layer_IsActive( void );
qboolean IOS_Layer_HasHardwareKeyboard( void );
qboolean IOS_Layer_HasHardwareMouse( void );
qboolean IOS_Layer_HasExternalScreen( void );
void IOS_Layer_SyncScreen( int width, int height, float scale );
void IOS_Layer_GetLayout( iosLayout_t *layout );
void IOS_Layer_SetGameOverlayVisible( qboolean visible );
void IOS_Layer_UpdateTouchControls( qboolean visible, float opacity, int mode,
	float moveX, float moveY, float moveRadius, qboolean moveActive,
	float lookX, float lookY, float lookRadius, qboolean lookActive,
	float fireX, float fireY, float fireRadius, qboolean fireActive,
	float altFireX, float altFireY, float altFireRadius, qboolean altFireActive,
	float jumpX, float jumpY, float jumpRadius, qboolean jumpActive,
	float crouchX, float crouchY, float crouchRadius, qboolean crouchActive,
	float useX, float useY, float useRadius, qboolean useActive,
	float openX, float openY, float openRadius, qboolean openActive,
	float weaponX, float weaponY, float weaponRadius,
	float menuX, float menuY, float menuRadius,
	float configX, float configY, float configRadius, qboolean configActive,
	qboolean editMode, float sliderX, float sliderY, float sliderW,
	float sliderValue );
void IOS_Layer_HideTouchControls( int mode );
void IOS_Layer_UpdateTouchControlsGamepadOnly( float opacity, int mode,
	float configX, float configY, float configRadius, qboolean configActive );
void IOS_Layer_OpenTouchSettings( void );
void IOS_Layer_SetTouchGamepadMode( qboolean gamepadMode );
void IOS_Layer_AttachToWindow( void );

/* --- External-display input debugging (temporary instrumentation) --- */
extern volatile int ios_dbgTouchDown;
extern volatile int ios_dbgTouchMove;
extern volatile int ios_dbgTouchUp;
extern volatile int ios_dbgUIMouse;
extern volatile int ios_dbgUILastDx;
extern volatile int ios_dbgUILastDy;
extern volatile int ios_dbgGCMove;
extern volatile int ios_dbgGCBtn;
extern volatile int ios_dbgGCLastDx;
extern volatile int ios_dbgGCLastDy;
extern volatile int ios_dbgGCGate;
extern volatile int ios_dbgCatcher;
extern volatile int ios_dbgClcState;
extern volatile int ios_dbgInUI;
extern volatile int ios_dbgExtKBM;

#endif
