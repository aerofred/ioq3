#include "ios_aim_assist.h"
#include "../client/client.h"

static cvar_t *in_aimAssist;
static float lookRemainder[IOS_AIM_SOURCE_COUNT][2];

void IOS_AimAssist_Init( void )
{
	in_aimAssist = Cvar_Get( "in_aimAssist", "0", CVAR_ARCHIVE );
	Com_Memset( lookRemainder, 0, sizeof( lookRemainder ) );
}

static const entityState_t *IOS_AimAssist_Entity( int index )
{
	return &cl.parseEntities[( cl.snap.parseEntitiesNum + index ) & ( MAX_PARSE_ENTITIES - 1 )];
}

/* The engine does not link bg_misc.c. Evaluate network trajectories at the
 * snapshot time so moving doors are traced at their current position. */
static void IOS_AimAssist_Position( const trajectory_t *tr, vec3_t position )
{
	float time = ( cl.snap.serverTime - tr->trTime ) * 0.001f;

	VectorCopy( tr->trBase, position );
	switch ( tr->trType ) {
		case TR_LINEAR_STOP:
			time = Com_Clamp( 0.0f, tr->trDuration * 0.001f, time );
			/* fall through */
		case TR_LINEAR:
			VectorMA( tr->trBase, time, tr->trDelta, position );
			break;
		case TR_SINE:
			if ( tr->trDuration > 0 ) {
				time = sinf( time * 1000.0f / tr->trDuration * M_PI * 2.0f );
				VectorMA( tr->trBase, time, tr->trDelta, position );
			}
			break;
		case TR_GRAVITY:
			VectorMA( tr->trBase, time, tr->trDelta, position );
			position[2] -= 0.5f * DEFAULT_GRAVITY * time * time;
			break;
		default:
			break;
	}
}

static qboolean IOS_AimAssist_Visible( const vec3_t eye, const vec3_t target )
{
	trace_t trace;
	vec3_t origin, angles;
	int i;
	const entityState_t *ent;

	CM_BoxTrace( &trace, eye, target, NULL, NULL, 0, MASK_SHOT, qfalse );
	if ( trace.startsolid || trace.allsolid || trace.fraction < 1.0f ) {
		return qfalse;
	}

	/* Doors and other moving brush models are not part of the world trace. */
	for ( i = 0; i < cl.snap.numEntities; i++ ) {
		ent = IOS_AimAssist_Entity( i );
		if ( ent->solid != SOLID_BMODEL || ent->modelindex <= 0 ||
			ent->modelindex >= CM_NumInlineModels() ) {
			continue;
		}
		IOS_AimAssist_Position( &ent->pos, origin );
		IOS_AimAssist_Position( &ent->apos, angles );
		CM_TransformedBoxTrace( &trace, eye, target, NULL, NULL,
			CM_InlineModel( ent->modelindex ), MASK_SHOT,
			origin, angles, qfalse );
		if ( trace.startsolid || trace.allsolid || trace.fraction < 1.0f ) {
			return qfalse;
		}
	}
	return qtrue;
}

static float IOS_AimAssist_Scale( void )
{
	const float coneDot = 0.9945219f; /* Six degrees around the crosshair. */
	float bestDot = coneDot;
	vec3_t eye, angles, forward, target, direction;
	const entityState_t *ent;
	const char *info;
	float distance, dot;
	int i, team, targetTeam;

	if ( !in_aimAssist || !in_aimAssist->integer || clc.state != CA_ACTIVE ||
		clc.demoplaying || !cl.snap.valid || Key_GetCatcher() ||
		cl.snap.ps.pm_type != PM_NORMAL || cl.snap.ps.stats[STAT_HEALTH] <= 0 ||
		( cl.snap.ps.pm_flags & PMF_FOLLOW ) ||
		cl.snap.ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return 1.0f;
	}

	VectorCopy( cl.snap.ps.origin, eye );
	eye[2] += cl.snap.ps.viewheight;
	for ( i = 0; i < 3; i++ ) {
		angles[i] = cl.viewangles[i] + SHORT2ANGLE( cl.snap.ps.delta_angles[i] );
	}
	AngleVectors( angles, forward, NULL, NULL );
	team = cl.snap.ps.persistant[PERS_TEAM];

	for ( i = 0; i < cl.snap.numEntities; i++ ) {
		ent = IOS_AimAssist_Entity( i );
		if ( ent->eType != ET_PLAYER || ent->number == cl.snap.ps.clientNum ||
			ent->clientNum < 0 || ent->clientNum >= MAX_CLIENTS ||
			( ent->eFlags & ( EF_DEAD | EF_NODRAW ) ) || ( ent->powerups & ( 1 << PW_INVIS ) ) ) {
			continue;
		}
		info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS + ent->clientNum];
		/* Unknown player info is not enough evidence to assist against them. */
		if ( !*info ) {
			continue;
		}
		targetTeam = atoi( Info_ValueForKey( info, "t" ) );
		if ( targetTeam == TEAM_SPECTATOR ||
			( ( team == TEAM_RED || team == TEAM_BLUE ) && targetTeam == team ) ) {
			continue;
		}

		/* Snapshot positions keep target and eye in the same time frame. Aim at
		 * the upper torso, accounting for the transmitted crouching bounds. */
		IOS_AimAssist_Position( &ent->pos, target );
		target[2] += ent->solid && ent->solid != SOLID_BMODEL ?
			( ( ( ent->solid >> 16 ) & 255 ) - 32 ) * 0.5f : 16.0f;
		VectorSubtract( target, eye, direction );
		distance = VectorNormalize( direction );
		if ( distance < 32.0f || distance > 2048.0f ) {
			continue;
		}
		dot = DotProduct( forward, direction );
		if ( dot > bestDot && IOS_AimAssist_Visible( eye, target ) ) {
			bestDot = dot;
		}
	}

	/* Blend smoothly from normal sensitivity at the edge to 45% at center.
	 * This is precision slowdown, with no snapping or automatic tracking. */
	return 1.0f - 0.55f * Com_Clamp( 0.0f, 1.0f, ( bestDot - coneDot ) / ( 1.0f - coneDot ) );
}

void IOS_AimAssist_QueueLook( float dx, float dy, iosAimSource_t source )
{
	float scale;
	int x, y;

	if ( source < 0 || source >= IOS_AIM_SOURCE_COUNT || ( dx == 0.0f && dy == 0.0f ) ) {
		return;
	}
	scale = IOS_AimAssist_Scale();
	if ( scale < 1.0f ) {
		dx = dx * scale + lookRemainder[source][0];
		dy = dy * scale + lookRemainder[source][1];
		x = (int)dx;
		y = (int)dy;
		/* Preserve small movements instead of creating a new stick deadzone. */
		lookRemainder[source][0] = dx - x;
		lookRemainder[source][1] = dy - y;
	} else {
		lookRemainder[source][0] = lookRemainder[source][1] = 0.0f;
		x = (int)dx;
		y = (int)dy;
	}
	if ( x || y ) {
		Com_QueueEvent( 0, SE_MOUSE, x, y, 0, NULL );
	}
}
