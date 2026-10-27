/* Standalone behavior checks with real engine math and mocked client/collision. */
#include "../../../code/client/client.h"
#include "../../../code/ios/ios_aim_assist.h"
#include <assert.h>

clientActive_t cl;
clientConnection_t clc;
static cvar_t option;
static int catcher, queuedX, queuedY, events;
static qboolean wall, door;
static vec3_t doorOrigin;

cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	assert( !strcmp( name, "in_aimAssist" ) );
	assert( !strcmp( value, "0" ) && ( flags & CVAR_ARCHIVE ) );
	return &option;
}
int Key_GetCatcher( void ) { return catcher; }
void Com_QueueEvent( int time, sysEventType_t type, int x, int y, int length, void *ptr ) {
	assert( type == SE_MOUSE && !length && !ptr );
	queuedX += x;
	queuedY += y;
	events++;
}
void QDECL Com_Error( int code, const char *fmt, ... ) { abort(); }
void QDECL Com_Printf( const char *fmt, ... ) {}
int CM_NumInlineModels( void ) { return 2; }
clipHandle_t CM_InlineModel( int index ) { return index; }
void CM_BoxTrace( trace_t *trace, const vec3_t start, const vec3_t end,
	vec3_t mins, vec3_t maxs, clipHandle_t model, int mask, int capsule ) {
	memset( trace, 0, sizeof( *trace ) );
	trace->fraction = wall ? 0.5f : 1.0f;
}
void CM_TransformedBoxTrace( trace_t *trace, const vec3_t start, const vec3_t end,
	vec3_t mins, vec3_t maxs, clipHandle_t model, int mask,
	const vec3_t origin, const vec3_t angles, int capsule ) {
	memset( trace, 0, sizeof( *trace ) );
	trace->fraction = door ? 0.5f : 1.0f;
	VectorCopy( origin, doorOrigin );
}

static void Reset( void ) {
	memset( &cl, 0, sizeof( cl ) );
	memset( &clc, 0, sizeof( clc ) );
	option.integer = 1;
	catcher = queuedX = queuedY = events = 0;
	wall = door = qfalse;
	IOS_AimAssist_Init();
	clc.state = CA_ACTIVE;
	cl.snap.valid = qtrue;
	cl.snap.ps.pm_type = PM_NORMAL;
	cl.snap.ps.stats[STAT_HEALTH] = 100;
	cl.snap.ps.viewheight = 26;
	cl.snap.numEntities = 1;
	cl.parseEntities[0].eType = ET_PLAYER;
	cl.parseEntities[0].number = cl.parseEntities[0].clientNum = 1;
	VectorSet( cl.parseEntities[0].pos.trBase, 500, 0, 10 );
	cl.gameState.stringOffsets[CS_PLAYERS + 1] = 1;
	strcpy( cl.gameState.stringData + 1, "\\n\\Enemy\\t\\0" );
}

static void ExpectNormal( void ) {
	IOS_AimAssist_QueueLook( 100, -100, IOS_AIM_TOUCH );
	assert( queuedX == 100 && queuedY == -100 );
}

int main( void ) {
	int i;

	Reset(); option.integer = 0; ExpectNormal();
	Reset();
	IOS_AimAssist_QueueLook( 100, -100, IOS_AIM_TOUCH );
	assert( queuedX == 45 && queuedY == -45 );
	Reset();
	IOS_AimAssist_QueueLook( 100, -100, IOS_AIM_GAMEPAD );
	assert( queuedX == 45 && queuedY == -45 );

	Reset(); cl.viewangles[YAW] = 7; ExpectNormal();
	Reset(); cl.viewangles[YAW] = 3;
	IOS_AimAssist_QueueLook( 100, 0, IOS_AIM_TOUCH );
	assert( queuedX > 45 && queuedX < 100 );
	Reset(); cl.viewangles[YAW] = -90;
	cl.snap.ps.delta_angles[YAW] = ANGLE2SHORT( 90 );
	IOS_AimAssist_QueueLook( 100, 0, IOS_AIM_TOUCH );
	assert( queuedX == 45 );
	Reset(); cl.parseEntities[0].pos.trBase[0] = 3000; ExpectNormal();
	Reset(); cl.parseEntities[0].eFlags = EF_DEAD; ExpectNormal();
	Reset(); cl.parseEntities[0].eFlags = EF_NODRAW; ExpectNormal();
	Reset(); cl.parseEntities[0].powerups = 1 << PW_INVIS; ExpectNormal();
	Reset(); cl.parseEntities[0].number = 0; ExpectNormal();
	Reset(); cl.gameState.stringData[1] = 0; ExpectNormal();
	Reset(); cl.snap.ps.persistant[PERS_TEAM] = TEAM_RED;
	strcpy( cl.gameState.stringData + 1, "\\n\\Ally\\t\\1" );
	ExpectNormal();
	Reset(); cl.snap.ps.persistant[PERS_TEAM] = TEAM_RED;
	strcpy( cl.gameState.stringData + 1, "\\n\\Enemy\\t\\2" );
	IOS_AimAssist_QueueLook( 100, 0, IOS_AIM_TOUCH );
	assert( queuedX == 45 );

	Reset(); wall = qtrue; ExpectNormal();
	Reset(); cl.snap.numEntities = 2;
	cl.parseEntities[1].solid = SOLID_BMODEL;
	cl.parseEntities[1].modelindex = 1;
	cl.snap.serverTime = 1500;
	cl.parseEntities[1].pos.trType = TR_LINEAR_STOP;
	cl.parseEntities[1].pos.trTime = 1000;
	cl.parseEntities[1].pos.trDuration = 1000;
	VectorSet( cl.parseEntities[1].pos.trDelta, 100, 0, 0 );
	door = qtrue; ExpectNormal();
	assert( doorOrigin[0] == 50 );
	Reset(); cl.snap.valid = qfalse; ExpectNormal();
	Reset(); clc.state = CA_DISCONNECTED; ExpectNormal();
	Reset(); clc.demoplaying = qtrue; ExpectNormal();
	Reset(); cl.snap.ps.stats[STAT_HEALTH] = 0; ExpectNormal();
	Reset(); cl.snap.ps.pm_type = PM_SPECTATOR; ExpectNormal();
	Reset(); cl.snap.ps.pm_flags = PMF_FOLLOW; ExpectNormal();
	Reset(); cl.snap.ps.persistant[PERS_TEAM] = TEAM_SPECTATOR; ExpectNormal();
	for ( i = 1; i <= KEYCATCH_CGAME; i <<= 1 ) {
		Reset(); catcher = i; ExpectNormal();
	}

	/* Tiny stick/touch deltas must accumulate, independently per device. */
	Reset();
	for ( i = 0; i < 100; i++ ) {
		IOS_AimAssist_QueueLook( 1, -1, IOS_AIM_GAMEPAD );
	}
	assert( queuedX >= 44 && queuedX <= 45 && queuedY == -queuedX );
	Reset();
	IOS_AimAssist_QueueLook( 1, 0, IOS_AIM_TOUCH );
	IOS_AimAssist_QueueLook( 1, 0, IOS_AIM_GAMEPAD );
	assert( events == 0 );
	IOS_AimAssist_QueueLook( 0, 0, IOS_AIM_TOUCH );
	assert( events == 0 );
	puts( "iOS aim assist checks passed" );
	return 0;
}
