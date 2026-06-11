#include "ios_gamepad_look.h"

#include "../client/client.h"
#include "../qcommon/qcommon.h"

#include <math.h>

static cvar_t *in_gamepadLookAccel = NULL;
static cvar_t *in_gamepadLookScale = NULL;
static cvar_t *in_gamepadLookInvert = NULL;

void IOS_Gamepad_LookInit( void )
{
	in_gamepadLookAccel = Cvar_Get( "in_gamepadLookAccel", "2.0", CVAR_ARCHIVE );
	in_gamepadLookScale = Cvar_Get( "in_gamepadLookScale", "12.0", CVAR_ARCHIVE );
	in_gamepadLookInvert = Cvar_Get( "in_gamepadLookInvert", "0", CVAR_ARCHIVE );
}

static float IOS_Gamepad_ApplyLookCurve( float value, float deadzone, float exponent )
{
	float absValue;
	float normalized;

	if ( exponent < 1.0f ) {
		exponent = 1.0f;
	}

	absValue = fabsf( value );
	if ( absValue < deadzone ) {
		return 0.0f;
	}

	normalized = ( absValue - deadzone ) / ( 1.0f - deadzone );
	if ( normalized > 1.0f ) {
		normalized = 1.0f;
	}

	normalized = powf( normalized, exponent );
	return ( value < 0.0f ) ? -normalized : normalized;
}

void IOS_Gamepad_LookFromStick( float stickX, float stickY )
{
	float deadzone;
	float accel;
	float sens;
	float scale;
	float ySign;
	float curvedX;
	float curvedY;
	int dx;
	int dy;

	if ( Key_GetCatcher() & ( KEYCATCH_CONSOLE | KEYCATCH_MESSAGE | KEYCATCH_UI | KEYCATCH_CGAME ) ) {
		return;
	}

	if ( clc.state != CA_ACTIVE ) {
		return;
	}

	deadzone = Cvar_VariableValue( "joy_threshold" );
	if ( deadzone < 0.05f ) {
		deadzone = 0.05f;
	}

	accel = in_gamepadLookAccel ? in_gamepadLookAccel->value : 2.0f;
	sens = Cvar_VariableValue( "sensitivity" );
	if ( sens < 1.0f ) {
		sens = 1.0f;
	}

	scale = in_gamepadLookScale ? in_gamepadLookScale->value : 12.0f;
	if ( scale < 1.0f ) {
		scale = 1.0f;
	}

	curvedX = IOS_Gamepad_ApplyLookCurve( stickX, deadzone, accel );
	curvedY = IOS_Gamepad_ApplyLookCurve( stickY, deadzone, accel );

	if ( curvedX == 0.0f && curvedY == 0.0f ) {
		return;
	}

	ySign = ( in_gamepadLookInvert && in_gamepadLookInvert->integer ) ? 1.0f : -1.0f;

	dx = (int)( curvedX * scale * sens );
	dy = (int)( ySign * curvedY * scale * sens );

	if ( dx == 0 && dy == 0 ) {
		return;
	}

	Com_QueueEvent( 0, SE_MOUSE, dx, dy, 0, NULL );
}
