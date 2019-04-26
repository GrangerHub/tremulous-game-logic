/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2019 GrangerHub

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, see <https://www.gnu.org/licenses/>

===========================================================================
*/

// bg_pmove.c -- both games player movement code
// takes a playerstate and a usercmd as input and returns a modifed playerstate

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"

pmove_t     *pm;
pml_t       pml;

// movement parameters
float pm_stopspeed = 100.0f;
float pm_duckScale = 0.25f;
float pm_swimScale = 0.50f;

float pm_accelerate = 10.0f;
float pm_wateraccelerate = 4.0f;
float pm_flyaccelerate = 4.0f;

float pm_friction = 6.0f;
float pm_waterfriction = 1.0f;
float pm_flightfriction = 6.0f;
float pm_spectatorfriction = 5.0f;

int   c_pmove = 0;

static vec3_t upNormal = { 0.0f, 0.0f, 1.0f };

#define MIX(x, y, factor) ((x) * (1 - (factor)) + (y) * (factor))

/*
===============
PM_AddEvent

===============
*/
void PM_AddEvent( int newEvent )
{
  BG_AddPredictableEventToPlayerstate( newEvent, 0, pm->ps );
}

/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt( trace_t *trace, const vec3_t impactVelocity )
{
  if( !pm->ClientImpacts )
    return;

  if( trace->entityNum == ENTITYNUM_WORLD )
    return;

  // see if it is already added
  if( pm->touchents[ trace->entityNum ] )
    return;

  // add it
  pm->touchents[ trace->entityNum ] = qtrue;
  pm->ClientImpacts( pm, trace, impactVelocity );
}

/*
===================
PM_UnlaggedOn
===================
*/
static void PM_UnlaggedOn(unlagged_attacker_data_t *data) {
  data->ent_num = pm->ps->clientNum;

  if(pm->unlagged_on) {
    pm->unlagged_on(data);
  } else {
    AngleVectors(
      pm->ps->viewangles,
      data->forward_out,
      data->right_out,
      data->up_out);
    BG_CalcMuzzlePointFromPS(
      pm->ps,
      data->forward_out,
      data->right_out,
      data->up_out,
      data->muzzle_out);
    VectorCopy(pm->ps->origin, data->origin_out);
  }
}

/*
===================
PM_UnlaggedOff
===================
*/
static void PM_UnlaggedOff(void) {
  if(pm->unlagged_off) {
    pm->unlagged_off( );
  }
}

/*
===================
PM_StartTorsoAnim
===================
*/
void PM_StartTorsoAnim( int anim )
{
  if( PM_Paralyzed( pm->ps->pm_type ) )
    return;

  pm->ps->torsoAnim = ( ( pm->ps->torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT )
    | anim;
}

/*
===================
PM_StartWeaponAnim
===================
*/
static void PM_StartWeaponAnim( int anim )
{
  if( PM_Paralyzed( pm->ps->pm_type ) )
    return;

  pm->ps->weaponAnim = ( ( pm->ps->weaponAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT )
    | anim;
}

/*
===================
PM_StartLegsAnim
===================
*/
static void PM_StartLegsAnim( int anim )
{
  if( PM_Paralyzed( pm->ps->pm_type ) )
    return;

  //legsTimer is clamped too tightly for nonsegmented models
  if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
  {
    if( pm->ps->legsTimer > 0 )
      return;   // a high priority animation is running
  }
  else
  {
    if( pm->ps->torsoTimer > 0 )
      return;   // a high priority animation is running
  }

  pm->ps->legsAnim = ( ( pm->ps->legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT )
    | anim;
}

/*
===================
PM_ContinueLegsAnim
===================
*/
static void PM_ContinueLegsAnim( int anim )
{
  if( ( pm->ps->legsAnim & ~ANIM_TOGGLEBIT ) == anim )
    return;

  //legsTimer is clamped too tightly for nonsegmented models
  if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
  {
    if( pm->ps->legsTimer > 0 )
      return;   // a high priority animation is running
  }
  else
  {
    if( pm->ps->torsoTimer > 0 )
      return;   // a high priority animation is running
  }

  PM_StartLegsAnim( anim );
}

/*
===================
PM_ContinueTorsoAnim
===================
*/
static void PM_ContinueTorsoAnim( int anim )
{
  if( ( pm->ps->torsoAnim & ~ANIM_TOGGLEBIT ) == anim )
    return;

  if( pm->ps->torsoTimer > 0 )
    return;   // a high priority animation is running

  PM_StartTorsoAnim( anim );
}

/*
===================
PM_ContinueWeaponAnim
===================
*/
static void PM_ContinueWeaponAnim( int anim )
{
  if( ( pm->ps->weaponAnim & ~ANIM_TOGGLEBIT ) == anim )
    return;

  PM_StartWeaponAnim( anim );
}

/*
===================
PM_ForceLegsAnim
===================
*/
static void PM_ForceLegsAnim( int anim )
{
  //legsTimer is clamped too tightly for nonsegmented models
  if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
    pm->ps->legsTimer = 0;
  else
    pm->ps->torsoTimer = 0;

  PM_StartLegsAnim( anim );
}


/*
==================
PM_Gravity

Sreturns the current gravity of the player
==================
*/
int PM_Gravity( playerState_t *ps ) {
  if(ps->pm_flags & PMF_FEATHER_FALL) {
    if(
      BG_InventoryContainsUpgrade(UP_JETPACK, ps->stats) &&
      !BG_UpgradeIsActive(UP_JETPACK, ps->stats) &&
      ps->groundEntityNum == ENTITYNUM_NONE &&
      ps->persistant[PERS_JUMPTIME] < JETPACK_DEACTIVATION_FALL_TIME) {
      return ps->gravity *
      (((float)ps->persistant[PERS_JUMPTIME]) /
        ((float)JETPACK_DEACTIVATION_FALL_TIME));
    } else {
      ps->pm_flags &= ~PMF_FEATHER_FALL;
      return ps->gravity;
    }
  } else {
    return ps->gravity;
  }
}


/*
==================
PM_ClipVelocity

Slide off of the impacting surface
==================
*/
void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out )
{
  float t = -DotProduct( in, normal );
  VectorMA( in, t, normal, out );
}


/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
static void PM_Friction( void )
{
  vec3_t  vec;
  float   *vel;
  float   speed, newspeed, control;
  float   drop;
  qboolean groundFriction = qfalse;

  vel = pm->ps->velocity;

  // make sure vertical velocity is NOT set to zero when wall climbing
  VectorCopy( vel, vec );
  if( pml.walking && !( pm->ps->stats[ STAT_STATE ] & SS_WALLCLIMBING ) )
    vec[ 2 ] = 0; // ignore slope movement

  speed = VectorLength( vec );

  if( speed < 0.1 )
    return;

  drop = 0;

  // apply ground friction
  if( pm->waterlevel <= 1 )
  {
    if( ( pml.walking || pml.ladder ) && !( pml.groundTrace.surfaceFlags & SURF_SLICK ) )
    {
      // if getting knocked back, no friction
      if( !( pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) )
      {
        float stopSpeed = BG_Class( pm->ps->stats[ STAT_CLASS ] )->stopSpeed;
        float friction = BG_Class( pm->ps->stats[ STAT_CLASS ] )->friction;

        control = speed < stopSpeed ? stopSpeed : speed;
        drop += control * friction * pml.frametime;
        groundFriction = qtrue;
      }
    }
  }

  // apply water friction even if just wading
  if( pm->waterlevel )
    drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;

  // apply flying friction
  if( pm->ps->pm_type == PM_JETPACK )
    drop += speed * pm_flightfriction * pml.frametime;

  if( pm->ps->pm_type == PM_SPECTATOR )
    drop += speed * pm_spectatorfriction * pml.frametime;

  // scale the velocity
  newspeed = speed - drop;
  if( newspeed < 0 )
    newspeed = 0;

  newspeed /= speed;

  vel[ 0 ] = vel[ 0 ] * newspeed;
  vel[ 1 ] = vel[ 1 ] * newspeed;
  // Don't reduce upward velocity while using a jetpack
  if( !( pm->ps->pm_type == PM_JETPACK &&
         vel[2] > 0 &&
         !groundFriction &&
         !pm->waterlevel ) )
    vel[ 2 ] = vel[ 2 ] * newspeed;
}


/*
==============
PM_IsDretchOrBasilisk

Checks if the player is a Dretch or a Basilisk.
==============
*/
static qboolean PM_IsDretchOrBasilisk( void )
{
  return pm->ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL0 ||
         pm->ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL1 ||
         pm->ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL1_UPG;
}


/*
==============
PM_IsMarauder

Checks if the player is a Marauder variant.
==============
*/
static qboolean PM_IsMarauder( void )
{
  return pm->ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL2 ||
         pm->ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL2_UPG;
}
/*
=============
PM_PushSolve

Finds out how much of the original velocity to keep so that pushing the player
the given amount does not exceed the maximum speed limit.

Solves the following equation:
length( push + velocity * x ) = length( velocity )
=============
*/
static float PM_PushSolve( vec3_t velocity, float speedSquared, vec3_t push )
{
  float a;
  float b;
  float c;
  float dSquared;
  float fraction;

  a = speedSquared;
  b = DotProduct( velocity, push ) * 2.0f;
  c = VectorLengthSquared( push ) - speedSquared;

  dSquared = b * b - 4.0f * a * c;

  if( dSquared < 0.0f )
    fraction = 0.0f;
  else
  {
    float d = sqrt( dSquared );
    float aTwice = 2.0f * a;
    fraction = MAX( 0.0f, (-b + d) / aTwice );
  }

  return fraction;
}


/*
==============
PM_Push

Adds a push to the given velocity and takes away exactly as much of the original
velocity as needed to not exceed the given maximum speed.
==============
*/
void PM_Push( vec3_t velocity, vec3_t push, float maxSpeed, vec3_t out )
{
  // Original speed (squared).
  float originalSpeedSquared;
  // Original speed.
  float originalSpeed;
  // Velocity immediately after the initial push.
  vec3_t pushedVelocity;
  // Speed after the initial push.
  float pushedSpeed;

  // Calculate the original speed.
  originalSpeedSquared = VectorLengthSquared( velocity );
  originalSpeed = sqrt( originalSpeedSquared );

  // Push the player.
  VectorAdd( velocity, push, pushedVelocity );
  pushedSpeed = VectorLength( pushedVelocity );

  // If the push accelerated the player past the maximum movement speed, limit
  // the speed to either the maximum movement speed or to the original speed if
  // the player was already going faster than that.
  if( pushedSpeed > originalSpeed && pushedSpeed > maxSpeed )
  {
    vec3_t maxedVelocity;
    float* targetVelocity;
    float targetFraction;

    if( originalSpeed < maxSpeed )
    {
      VectorScale( velocity, maxSpeed / originalSpeed, maxedVelocity );
      targetFraction = PM_PushSolve( maxedVelocity, maxSpeed * maxSpeed, push );
      targetVelocity = maxedVelocity;
    }
    else
    {
      targetFraction = PM_PushSolve( velocity, originalSpeedSquared, push );
      targetVelocity = velocity;
    }

    VectorScale( targetVelocity, targetFraction, out );
    VectorAdd( out, push, out );
  }
  else
  {
    VectorCopy( pushedVelocity, out );
  }
}


/*
==============
PM_Accelerate

Handles user intended acceleration. Note that this function may counteract the
gravity, use PM_AccelerateHorizontal to avoid that.
==============
*/
static void PM_Accelerate( vec3_t wishDir, float wishSpeed, float accel )
{
  if(pm->playerAccelMode == 1) {
#if 1
    // q2 style
    int     i;
    float   addspeed, accelspeed, currentspeed;

    currentspeed = DotProduct( pm->ps->velocity, wishDir );
    addspeed = wishSpeed - currentspeed;
    if( addspeed <= 0 )
      return;

    accelspeed = accel * pml.frametime * wishSpeed;
    if( accelspeed > addspeed )
      accelspeed = addspeed;

    for( i = 0; i < 3; i++ )
      pm->ps->velocity[ i ] += accelspeed * wishDir[ i ];
#else
    // proper way (avoids strafe jump maxspeed bug), but feels bad
    vec3_t    wishVelocity;
    vec3_t    pushDir;
    float     pushLen;
    float     canPush;

    VectorScale( wishdir, wishspeed, wishVelocity );
    VectorSubtract( wishVelocity, pm->ps->velocity, pushDir );
    pushLen = VectorNormalize( pushDir );

    canPush = accel * pml.frametime * wishspeed;
    if( canPush > pushLen )
      canPush = pushLen;

    VectorMA( pm->ps->velocity, canPush, pushDir, pm->ps->velocity );
#endif
  } else {
    // How much to push the player during this frame.
    float push = accel * pml.frametime * wishSpeed;
    // Original speed of the player.
    float originalSpeed = VectorLength( pm->ps->velocity );
    // Speed of the player immediately after being pushed.
    float pushedSpeed;

    // Push the player in the intended direction.
    VectorMA( pm->ps->velocity, push, wishDir, pm->ps->velocity );
    pushedSpeed = VectorLength( pm->ps->velocity );

    // If the push accelerated the player past the maximum movement speed, limit
    // the resulting speed to either the maximum movement speed or to the original
    // speed if the player was already going faster than that.
    if( pushedSpeed > originalSpeed && pushedSpeed > wishSpeed )
    {
      float speedLimit = MAX( wishSpeed, originalSpeed );
      VectorNormalize( pm->ps->velocity );
      VectorScale( pm->ps->velocity, speedLimit, pm->ps->velocity );
    }
  }
}


/*
===========
PM_AccelerateHorizontal

Handles user intended acceleration, only limiting the speed along the
horizontal plane.
=====
*/
static void PM_AccelerateHorizontal( vec3_t wishDir, float wishSpeed,
                                     float accel )
{
  if(pm->playerAccelMode == 1) {
    PM_Accelerate( wishDir, wishSpeed, accel );
  } else {
    float verticalSpeed = pm->ps->velocity[ 2 ];

    pm->ps->velocity[ 2 ] = 0.0f;
    PM_Accelerate( wishDir, wishSpeed, accel );
    pm->ps->velocity[ 2 ] = verticalSpeed;
  }
}


/*
==============
PM_RedirectMomentum

Handles momentum redirection in the air.
==============
*/
static void PM_RedirectMomentum( vec3_t wishdir, float wishspeed,
                                 float multiplier )
{
  vec3_t currentdir; //- Current direction (horizontal only).
  vec3_t redirected; //- Velocity of the player (horizontal only).
  float speed;       //- Horizontal speed of the player.
  float airaccel;    //- Air acceleration of the player class.
  float alignment;   //- How much the current direction aligns with the desired
                     //  direction (from 0.0f to 1.0f).
  float factor;      //- Acceleration factor.

  // No direction provided, do nothing.
  if( wishspeed < 0.1f )
    return;

  // Don't redirect momentum unless moving forward.
  if( pm->cmd.forwardmove <= 0 || abs( pm->cmd.rightmove ) >= 10 )
    return;

  airaccel = BG_Class( pm->ps->stats[ STAT_CLASS ] )->airAcceleration;

  currentdir[ 0 ] = pm->ps->velocity[ 0 ];
  currentdir[ 1 ] = pm->ps->velocity[ 1 ];
  currentdir[ 2 ] = 0;
  speed = VectorNormalize( currentdir );

  alignment = ( DotProduct( currentdir, wishdir ) + 1.0f ) / 2.0f;

  factor = multiplier * airaccel * alignment * alignment * pml.frametime;

  VectorMA( currentdir, factor, wishdir, redirected );
  VectorNormalize( redirected );
  VectorScale( redirected, speed, redirected );

  pm->ps->velocity[ 0 ] = redirected[ 0 ];
  pm->ps->velocity[ 1 ] = redirected[ 1 ];
}


/*
==============
PM_RudderDrag

Applies some drag as if the player had a rudder. If the rudder is perfectly
aligned with the current horizontal velocity, the speed is not affected,
otherwise apply some drag.
==============
*/
static void PM_RudderDrag( vec3_t wishDir, float minSpeed, float maxSpeed,
                           float maxDrag )
{
  // Current horizontal movement direction.
  vec3_t horizDir;
  // Current horizontal movement speed;
  float horizSpeed;
  // How well is the rudder aligned with the current direction.
  float misalignment;
  // How much of the maximum drag to apply.
  float dragFraction;
  // How much to push the player this frame.
  float pushLen;

  horizDir[ 0 ] = pm->ps->velocity[ 0 ];
  horizDir[ 1 ] = pm->ps->velocity[ 1 ];
  horizDir[ 2 ] = 0.0f;
  horizSpeed = VectorNormalize( horizDir );

  if( horizSpeed <= minSpeed )
    return;
  else if( horizSpeed >= maxSpeed )
    dragFraction = 1.0f;
  else
  {
    float overspeed = horizSpeed - minSpeed;
    float maxOverspeed = maxSpeed - minSpeed;
    dragFraction = overspeed / maxOverspeed;
  }

  misalignment = 1.0f - fabs( DotProduct( horizDir, wishDir ) );

  dragFraction *= sqrt( MAX( 0.0f, misalignment ) );

  pushLen = maxDrag * dragFraction * pml.frametime;

  VectorMA( pm->ps->velocity, -pushLen, horizDir, pm->ps->velocity );
}


/*
==============
PM_BoostBraking

Increase acceleration when changing the movement direction. Works only in the
horizontal plane and assumes wishDir is horizontal.
==============
*/
static void PM_BoostBraking( vec3_t wishDir, float fadeStartSpeed,
                             float maxSpeed, float maxBoost, float *accel )
{
  // Current horizontal direction.
  vec3_t horizDir;
  // Current horizontal speed.
  float horizSpeed;
  // How much do we want to change the direction, goes from 0.0f when going
  // perfectly along the movement direction to 1.0f when braking.
  float changeFactor;
  // How much of the maximum boost to apply.
  float boostFraction;

  horizDir[ 0 ] = pm->ps->velocity[ 0 ];
  horizDir[ 1 ] = pm->ps->velocity[ 1 ];
  horizDir[ 2 ] = 0.0f;
  horizSpeed = VectorNormalize( horizDir );

  if( horizSpeed <= fadeStartSpeed )
    boostFraction = 1.0f;
  else if( horizSpeed >= maxSpeed )
    return;
  else
  {
    float overspeed = horizSpeed - fadeStartSpeed;
    float maxOverspeed = maxSpeed - fadeStartSpeed;
    boostFraction = 1.0f - overspeed / maxOverspeed;
  }

  changeFactor = ( -DotProduct( wishDir, horizDir ) + 1.0f ) / 2.0f;

  *accel = *accel * ( 1.0f + changeFactor * boostFraction * maxBoost );
}


/*
==============
PM_IsWall

Checks if the given surface is a wall.
==============
*/
static qboolean PM_IsWall( trace_t *trace )
{
  return trace->fraction < 1.0f && trace->plane.normal[ 2 ] < MIN_WALK_NORMAL &&
         !( trace->contents & CONTENTS_BODY ) ;
}


/*
==============
WALLCOAST_GROUNDED
WALLCOAST_3D

Defined to increase readability.
==============
*/
typedef enum
{
  WALLCOAST_3D = 0,
  WALLCOAST_GROUNDED
} wallcoast_t;


/*
==============
PM_WallCoast

Modifies `wishDir` as to allow coasting along walls at full speed even when the
desired movement direction is not perfectly parallel with the wall. The
`groundNormal` parameter is only used for ambiguity resolution.
==============
*/
static void PM_WallCoast( vec3_t wishDir, wallcoast_t grounded )
{
  // How directly can the player need to look at the wall before slowing down.
  static float SLOWDOWN_THRESHOLD = 0.95f;
  // Pointer to the ground normal.
  float* groundNormal;
  // Look direction (possibly projected onto the ground).
  vec3_t myLookDir;
  // Desired movement direction (possibly projected onto the ground).
  vec3_t myWishDir;
  // Wall normal (possibly projected onto the ground).
  vec3_t myWallNormal;
  // The wall the player is running into.
  trace_t wall;        
  // Where to stop searching for a wall. 
  vec3_t searchEnd;   
  // How much does myWishDir align with myLookDir (0 to 1).
  float alignment;   
  // How much is the player is moving into the wall (0 to 1).
  float ramFactor;    
  // Resulting direction.
  vec3_t result;

  // Get the ground normal.
  if( pml.groundPlane )
    groundNormal = pml.groundTrace.plane.normal;
  else
    groundNormal = upNormal;

  // Project the desired direction onto the ground.
  if( grounded )
  {
    ProjectPointOnPlane( myWishDir, wishDir, groundNormal );
    VectorNormalize( myWishDir );
  }
  else
    VectorCopy( wishDir, myWishDir );
  
  // Find a wall the player is running into.
  VectorMA( pm->ps->origin, 0.25f, myWishDir, searchEnd );
  pm->trace( &wall, pm->ps->origin, pm->mins, pm->maxs,
             searchEnd, pm->ps->clientNum, pm->tracemask );

  // Nothing found or not a wall.
  if( !PM_IsWall( &wall ) )
    return;

  // Project the look direction and the wall normal onto the ground.
  if( grounded )
  {
    ProjectPointOnPlane( myWallNormal, wall.plane.normal, groundNormal );
    ProjectPointOnPlane( myLookDir, pml.forward, groundNormal );
    VectorNormalize( myLookDir );
    VectorNormalize( myWallNormal );
  }
  else
  {
    VectorCopy( wall.plane.normal, myWallNormal );
    VectorCopy( pml.forward, myLookDir );
  }

  // Check how much the look direction aligns with the desired direction.
  alignment = DotProduct( myLookDir, myWishDir );

  // Project the desired direction onto the wall.
  ProjectPointOnPlane( result, wishDir, myWallNormal );

  // If we are moving directly towards the wall, try to guess the intended
  // movement direction.
  if( VectorLength( result ) < 0.025f )
  {
    // The look direction and the desired movement direction are orthogonal.
    // We have no information to work with, give up.
    if( fabs( alignment ) < 0.025f )
      return;

    // Pick either the look direction or the opposite of it, whichever aligns
    // with the original desired direction best.
    if( alignment > 0.0f )
      VectorCopy( myLookDir, result );
    else
      VectorNegate( myLookDir, result );

    // Project the picked direction onto the wall. If this direction would still
    // lead the player directly into the wall (this happens when moving directly
    // forward or backward), give up.
    ProjectPointOnPlane( result, result, wall.plane.normal );
    if( VectorLength( result ) < 0.025f )
      return;
  }

  // Normalize the resulting direction.
  VectorNormalize( result );

  // Calculate how much the player is moving into the wall.
  ramFactor = MAX( 0.0f, -DotProduct( myWishDir, myWallNormal ) );

  // Make sure that the transition from one direction to the opposite is not too
  // sudden. When moving directly or almost directly into the wall, do not give
  // the player full speed. Since we only have direct control over the movement
  // direction and not the speed in this function, this is accomplished by
  // directing some of the velocity into the wall, allowing PM_ClipVelocity to
  // do the rest of the job.
  if( ramFactor > SLOWDOWN_THRESHOLD )
  {
    float diff;
    float maxDiff;
    float fraction;

    // Calculate how much of the speed to keep.
    diff = ramFactor - SLOWDOWN_THRESHOLD;
    maxDiff = 1.0f - SLOWDOWN_THRESHOLD;
    fraction = 1.0f - diff * ( 1.0f / maxDiff );

    // Always keep some speed unless going directly forward.
    if ( alignment <= 0.95f )
    {
      static float MIN_FRACTION = 0.4f;
      fraction = MIN_FRACTION + ( fraction * ( 1.0f - MIN_FRACTION ) );
    }

    // Modify the resulting direction.
    VectorScale( result, fraction, result );
    VectorMA( result, fraction - 1.0f, myWallNormal, result );
    VectorNormalize( result );
  }

  // Set the new desired direction.
  VectorCopy( result, wishDir );
}


/*
==============
PM_ScanForWall

Scans for a wall in the given direction.
==============
*/
/*static qboolean PM_ScanForWall( trace_t *wall, vec3_t searchDir )
{
  vec3_t searchEnd;
  VectorMA( pm->ps->origin, 0.25f, searchDir, searchEnd );
  pm->trace( wall, pm->ps->origin, pm->mins, pm->maxs,
             searchEnd, pm->ps->clientNum, pm->tracemask );
  return PM_IsWall( wall );
}*/


/*
==============
PM_ComputeWallSpeedFactor

Checks if the player is in contact with a wall and computes how much of the
potential speed increase to actually add to the maximum speed, putting the
result in pml.wallSpeedFactor.
==============
*/
/*static void PM_ComputeWallSpeedFactor( void )
{
  float*   groundNormal; //- Ground normal.
  vec3_t   searchDir;    //- Direction for searching for a wall.
  trace_t  wall;         //- Wall we are moving along.
  vec3_t   lookDir;      //- Look direction (horizontal only).
  vec3_t   normal;       //- Wall normal (horizontal only).
  float    alignment;    //- How much the look direction aligns with the wall
                         //  direction (from 0 to 1).

  // Find the wall we are moving along.
  if( pml.groundPlane )
    groundNormal = pml.groundTrace.plane.normal;
  else
    groundNormal = upNormal;

  ProjectPointOnPlane( searchDir, pml.right, groundNormal );
  VectorNormalize( searchDir );

  if( !PM_ScanForWall( &wall, searchDir ) )
  {
    VectorNegate( searchDir, searchDir );
    if( !PM_ScanForWall( &wall, searchDir ) )
      return;
  }

  // Project the look direction and the wall normal onto the ground.
  ProjectPointOnPlane( lookDir, pml.forward, groundNormal );
  ProjectPointOnPlane( normal, wall.plane.normal, groundNormal );
  VectorNormalize( lookDir );
  VectorNormalize( normal );

  // Calculate the alignment.
  alignment = 1.0f - fabs( DotProduct( lookDir, normal ) );

  // Use sqrt to make this easier to use. MAX is used just in case.
  pml.wallSpeedFactor = sqrt( MAX( 0.0f, alignment ) );
}*/


/*
============
PM_CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
static float PM_CmdScale( usercmd_t *cmd, qboolean zFlight )
{
  static float HUMAN_WALL_MODIFIER = 1.15f;
  static float ALIEN_WALL_MODIFIER = 1.2f;
  int         max;
  float       total;
  float       scale;
  float       modifier = 1.0f;

  if ( BG_ClassHasAbility(pm->ps->stats[STAT_CLASS], SCA_STAMINA)
    && pm->ps->pm_type == PM_NORMAL )
  {
    qboolean wasSprinting;
    qboolean sprint;
    wasSprinting = sprint = pm->ps->stats[ STAT_STATE ] & SS_SPEEDBOOST;

    if( pm->ps->persistant[ PERS_STATE ] & PS_SPRINTTOGGLE )
    {
      // walking or crouching toggles off sprint
      if( ( pm->ps->pm_flags & PMF_SPRINTHELD ) &&
          ( ( cmd->buttons & BUTTON_WALKING ) || ( pm->cmd.upmove < 0 ) ) )
      {
        sprint = qfalse;
        pm->ps->pm_flags &= ~PMF_SPRINTHELD;
      }
      else if( ( pm->ps->persistant[ PERS_STATE ] & PS_SPRINTTOGGLEONSTOP ) &&
               !pm->cmd.forwardmove && !pm->cmd.rightmove )
      {
        sprint = qfalse;
        pm->ps->pm_flags &= ~PMF_SPRINTHELD;
      }
      else if( ( cmd->buttons & BUTTON_SPRINT ) &&
               !( pm->ps->pm_flags & PMF_SPRINTHELD ) &&
               !( cmd->buttons & BUTTON_WALKING ) )
      {
        sprint = qtrue;
        pm->ps->pm_flags |= PMF_SPRINTHELD;
      }
    }
    else if ( pm->cmd.upmove >= 0 )
      sprint = cmd->buttons & BUTTON_SPRINT;

    if( sprint )
      pm->ps->stats[ STAT_STATE ] |= SS_SPEEDBOOST;
    else if( wasSprinting && !sprint )
      pm->ps->stats[ STAT_STATE ] &= ~SS_SPEEDBOOST;

    // Walk overrides sprint. We keep the state that we want to be sprinting
    //  (above), but don't apply the modifier, and in g_active we skip taking
    //  the stamina too.
    if( sprint && !( cmd->buttons & BUTTON_WALKING ) )
      modifier *= HUMAN_SPRINT_MODIFIER;
    else
      modifier *= HUMAN_JOG_MODIFIER;

    if( cmd->forwardmove < 0 )
    {
      //can't run backwards
      modifier *= HUMAN_BACK_MODIFIER;
    }
    else if( cmd->rightmove )
   {
     //can't move that fast sideways
     modifier *= HUMAN_SIDE_MODIFIER;
     modifier *= MIX( HUMAN_SIDE_MODIFIER, 1.0f, pml.wallSpeedFactor );
   }
 
  modifier *= MIX( 1.0f, HUMAN_WALL_MODIFIER, pml.wallSpeedFactor );

    if( !zFlight )
    {
      //must have have stamina to jump
      if( pm->ps->stats[ STAT_STAMINA ] < STAMINA_SLOW_LEVEL + STAMINA_JUMP_TAKE )
        cmd->upmove = 0;
    }

    //slow down once stamina depletes
    if( pm->ps->stats[ STAT_STAMINA ] <= STAMINA_SLOW_LEVEL )
      modifier *= (float)( pm->ps->stats[ STAT_STAMINA ] + STAMINA_MAX ) / (float)(STAMINA_SLOW_LEVEL + STAMINA_MAX);

    if( pm->ps->stats[ STAT_STATE ] & SS_CREEPSLOWED )
    {
      if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, pm->ps->stats ) ||
          BG_InventoryContainsUpgrade( UP_BATTLESUIT, pm->ps->stats ) )
        modifier *= CREEP_ARMOUR_MODIFIER;
      else
        modifier *= CREEP_MODIFIER;
    }
    if( pm->ps->eFlags & EF_POISONCLOUDED )
    {
      if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, pm->ps->stats ) ||
          BG_InventoryContainsUpgrade( UP_BATTLESUIT, pm->ps->stats ) )
        modifier *= PCLOUD_ARMOUR_MODIFIER;
      else
        modifier *= PCLOUD_MODIFIER;
    }
    else if( pm->ps->stats[ STAT_TEAM ] == TEAM_ALIENS )
    {
      modifier *= MIX( 1.0f, ALIEN_WALL_MODIFIER, pml.wallSpeedFactor );
    }
  }

  if( pm->ps->weapon == WP_ALEVEL4 && pm->ps->pm_flags & PMF_CHARGE )
    modifier *= 1.0f + ( pm->ps->misc[ MISC_MISC ] *
                         ( LEVEL4_TRAMPLE_SPEED - 1.0f ) /
                         LEVEL4_TRAMPLE_DURATION );

  //slow player if charging up for a pounce
  if( ( pm->ps->weapon == WP_ALEVEL3 || pm->ps->weapon == WP_ALEVEL3_UPG ) &&
      ( !pm->swapAttacks ?
        (cmd->buttons & BUTTON_ATTACK2) : (cmd->buttons & BUTTON_ATTACK) ) )
    modifier *= LEVEL3_POUNCE_SPEED_MOD;

  //slow the player if slow locked
  if( pm->ps->stats[ STAT_STATE ] & SS_SLOWLOCKED )
    modifier *= ABUILDER_BLOB_SPEED_MOD;

  if( pm->ps->pm_type == PM_GRABBED )
    modifier = 0.0f;

  max = abs( cmd->forwardmove );
  if( abs( cmd->rightmove ) > max )
    max = abs( cmd->rightmove );
  total = cmd->forwardmove * cmd->forwardmove + cmd->rightmove * cmd->rightmove;

  if( zFlight )
  {
    if( abs( cmd->upmove ) > max )
      max = abs( cmd->upmove );
    total += cmd->upmove * cmd->upmove;
  }

  if( !max )
    return 0;

  total = sqrt( total );

  scale = (float)pm->ps->speed * max / ( 127.0 * total ) * modifier;

  return scale;
}


/*
================
PM_SetMovementDir

Determine the rotation of the legs relative
to the facing dir
================
*/
static void PM_SetMovementDir( void )
{
  if( pm->cmd.forwardmove || pm->cmd.rightmove )
  {
    if( pm->cmd.rightmove == 0 && pm->cmd.forwardmove > 0 )
      pm->ps->movementDir = 0;
    else if( pm->cmd.rightmove < 0 && pm->cmd.forwardmove > 0 )
      pm->ps->movementDir = 1;
    else if( pm->cmd.rightmove < 0 && pm->cmd.forwardmove == 0 )
      pm->ps->movementDir = 2;
    else if( pm->cmd.rightmove < 0 && pm->cmd.forwardmove < 0 )
      pm->ps->movementDir = 3;
    else if( pm->cmd.rightmove == 0 && pm->cmd.forwardmove < 0 )
      pm->ps->movementDir = 4;
    else if( pm->cmd.rightmove > 0 && pm->cmd.forwardmove < 0 )
      pm->ps->movementDir = 5;
    else if( pm->cmd.rightmove > 0 && pm->cmd.forwardmove == 0 )
      pm->ps->movementDir = 6;
    else if( pm->cmd.rightmove > 0 && pm->cmd.forwardmove > 0 )
      pm->ps->movementDir = 7;
  }
  else
  {
    // if they aren't actively going directly sideways,
    // change the animation to the diagonal so they
    // don't stop too crooked
    if( pm->ps->movementDir == 2 )
      pm->ps->movementDir = 1;
    else if( pm->ps->movementDir == 6 )
      pm->ps->movementDir = 7;
  }
}


/*
=============
PM_CheckCharge
=============
*/
static void PM_CheckCharge( void )
{
  if( pm->ps->weapon != WP_ALEVEL4 )
    return;

  if( ( !pm->swapAttacks ?
        (pm->cmd.buttons & BUTTON_ATTACK2) : (pm->cmd.buttons & BUTTON_ATTACK) ) &&
      !( pm->ps->stats[ STAT_STATE ] & SS_CHARGING ) )
  {
    pm->ps->pm_flags &= ~PMF_CHARGE;
    return;
  }

  if( pm->ps->misc[ MISC_MISC ] > 0 )
    pm->ps->pm_flags |= PMF_CHARGE;
  else
    pm->ps->pm_flags &= ~PMF_CHARGE;
}

/*
=============
PM_CheckPounce
=============
*/
static qboolean PM_CheckPounce( void )
{
  int jumpMagnitude;

  if( pm->ps->weapon != WP_ALEVEL3 &&
      pm->ps->weapon != WP_ALEVEL3_UPG )
    return qfalse;

  // We were pouncing, but we've landed
  if( pm->ps->groundEntityNum != ENTITYNUM_NONE &&
      ( pm->ps->pm_flags & PMF_CHARGE ) )
  {
    pm->ps->pm_flags &= ~PMF_CHARGE;
    pm->ps->weaponTime += LEVEL3_POUNCE_REPEAT;
    return qfalse;
  }

  // We're building up for a pounce
  if( ( !pm->swapAttacks ?
        (pm->cmd.buttons & BUTTON_ATTACK2) : (pm->cmd.buttons & BUTTON_ATTACK) ) )
  {
    pm->ps->pm_flags &= ~PMF_CHARGE;
    return qfalse;
  }

  // Can't start a pounce
  if( ( pm->ps->pm_flags & PMF_CHARGE ) ||
      pm->ps->misc[ MISC_MISC ] < LEVEL3_POUNCE_TIME_MIN ||
      pm->ps->groundEntityNum == ENTITYNUM_NONE )
    return qfalse;

  // Give the player forward velocity and simulate a jump
  pml.groundPlane = qfalse;
  pml.walking = qfalse;
  pm->ps->pm_flags |= PMF_CHARGE;
  pm->ps->groundEntityNum = ENTITYNUM_NONE;
  if( pm->ps->weapon == WP_ALEVEL3 )
    jumpMagnitude = pm->ps->misc[ MISC_MISC ] *
                    LEVEL3_POUNCE_JUMP_MAG / LEVEL3_POUNCE_TIME;
  else
    jumpMagnitude = pm->ps->misc[ MISC_MISC ] *
                    LEVEL3_POUNCE_JUMP_MAG_UPG / LEVEL3_POUNCE_TIME_UPG;
  VectorMA( pm->ps->velocity, jumpMagnitude, pml.forward, pm->ps->velocity );
  PM_AddEvent( EV_JUMP );

  // Play jumping animation
  if( pm->cmd.forwardmove >= 0 )
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
      PM_ForceLegsAnim( LEGS_JUMP );
    else
      PM_ForceLegsAnim( NSPA_JUMP );

    pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
  }
  else
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
      PM_ForceLegsAnim( LEGS_JUMPB );
    else
      PM_ForceLegsAnim( NSPA_JUMPBACK );

    pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
  }

  //Reduce charged stamina
  if(BG_ClassHasAbility(pm->ps->stats[STAT_CLASS], SCA_CHARGE_STAMINA)) {
    pm->ps->misc[MISC_CHARGE_STAMINA] -= 
      (
        pm->ps->misc[MISC_MISC] *
        BG_Class(pm->ps->stats[STAT_CLASS] )->chargeStaminaUseRate) /
      BG_Class(pm->ps->stats[STAT_CLASS] )->chargeStaminaRestoreRate;
  
    if(
      pm->ps->misc[MISC_CHARGE_STAMINA] <
      (
        BG_Class(pm->ps->stats[STAT_CLASS] )->chargeStaminaMin /
        BG_Class(pm->ps->stats[STAT_CLASS] )->chargeStaminaRestoreRate)) {
      pm->ps->misc[MISC_CHARGE_STAMINA] =
        BG_Class(pm->ps->stats[STAT_CLASS] )->chargeStaminaMin /
        BG_Class(pm->ps->stats[STAT_CLASS] )->chargeStaminaRestoreRate;
    }
  }

  pm->pmext->pouncePayload = pm->ps->misc[ MISC_MISC ];
  pm->ps->misc[ MISC_MISC ] = 0;

  return qtrue;
}

#define WALLJUMP_TIME 350

/*
=============
PM_CheckWallJump
=============
*/
static qboolean PM_CheckWallJump( vec3_t wishDir, float wishSpeed )
{  
  trace_t wall;           //- Which wall we are jumping off.
  vec3_t searchEnd;       //- Point of contact with the wall.
  vec3_t moveDir;         //- Desired or actual movement direction.
  vec3_t horizWallNormal; //- Horizontal wall normal.

  float upLook;        //- How much the player is looking up (0 to 1).
  float intoLook;      //- How much the player is looking at the wall (0 to 1).
  float ramFactor;     //- How much the player is moving into the wall (0 to 1).
  float jumpMagnitude; //- Jump speed of the player's class.
  float upFactor;      //- How fast to jump up (0 to 1).
  float awayFactor;    //- How fast to jump away (0 to 1).
  float upSpeed;       //- How fast to jump up (actual speed).
  float awaySpeed;     //- How fast to jump away (actual speed).
  float maxSpeed;      //- Maximum speed after the walljump.

  // Required upLook and ramFactor for the walljump to activate.
  static float minUpLook = 0.1f;
  static float minRamFactor = 0.2f;

  // Not pressing jump.
  if( pm->cmd.upmove < 10 )
    return qfalse;

  // Don't walljump if the class does not have such an ability.
  if( !( BG_Class( pm->ps->stats[ STAT_CLASS ] )->abilities & SCA_WALLJUMPER ) )
    return qfalse;

  // Don't allow jump after respawning until all buttons are up.
  if( pm->ps->pm_flags & PMF_RESPAWNED )
    return qfalse;

  // If holding the jump key, wait until the timer which limits automatic
  // jumping runs out.
  if( ( pm->ps->pm_flags & PMF_JUMP_HELD ) &&
      ( pm->ps->pm_flags & PMF_TIME_LAND ) )
    return qfalse;

  // Not enough time has passed since the previous walljump.
  if( pm->ps->pm_flags & PMF_TIME_WALLJUMP )
    return qfalse;

  // Use the desired movement direction, or the look direction projected onto
  // the horizontal plane if the desired direction is not available.
  if( wishSpeed >= 0.1f )
    VectorCopy( wishDir, moveDir );
  else
  {
    moveDir[ 0 ] = pml.forward[ 0 ];
    moveDir[ 1 ] = pml.forward[ 1 ];
    moveDir[ 2 ] = 0.0f;
    VectorNormalize( moveDir );
  }

  // Find a wall to jump off of.
  VectorMA( pm->ps->origin, 0.25f, moveDir, searchEnd );
  pm->trace( &wall, pm->ps->origin, pm->mins, pm->maxs,
             searchEnd, pm->ps->clientNum, pm->tracemask );

  // Check if the found a wall.
  if( !PM_IsWall( &wall ) )
    return qfalse;

  // Check if the wall is suitable for walljumping.
  if( wall.surfaceFlags & ( SURF_SKY | SURF_SLICK ) )
    return qfalse;

    // Find out how much we look up.
    upLook = MAX( pm->wallJumperMinFactor, DotProduct( upNormal, pml.forward ) );

  // Check if we are looking up the wall.
  if( upLook < minUpLook )
    return qfalse;

  // Find out how much we are moving into the wall.
  horizWallNormal[ 0 ] = wall.plane.normal[ 0 ];
  horizWallNormal[ 1 ] = wall.plane.normal[ 1 ];
  horizWallNormal[ 2 ] = 0.0f;
  VectorNormalize( horizWallNormal );
  ramFactor = fabs( -DotProduct( horizWallNormal, moveDir ) );

  // Check if we are moving into the wall enough.
  if( ramFactor < minRamFactor )
    return qfalse;

  // Find out how much we look into the wall.
  intoLook = MAX( 0.0f, -DotProduct( horizWallNormal, pml.forward ) );

  // Set grapple point to wall normal.
  VectorCopy( wall.plane.normal, pm->ps->grapplePoint );

  // Prepare the jump.
  pml.groundPlane = qfalse;
  pml.walking = qfalse;
  pm->ps->pm_flags |= PMF_JUMP_HELD;
  pm->ps->groundEntityNum = ENTITYNUM_NONE;

  // Do not allow gaining too much speed from walljumping, but never slow the
  // player down either.
  maxSpeed = MAX( VectorLength( pm->ps->velocity ), LEVEL2_WALLJUMP_MAXSPEED );

  // Calculate the jump speed.
  jumpMagnitude = BG_Class( pm->ps->stats[ STAT_CLASS ] )->jumpMagnitude;
  upFactor = MIN( 1.0f, MAX( upLook, intoLook + 0.15f ) );
  awayFactor = MIN( 1.0f - upLook * upLook, intoLook );
  upSpeed = upFactor * 1.25f * jumpMagnitude;
  awaySpeed = intoLook * 0.75f * jumpMagnitude;

  // Store awayFactor for use in PM_AirMove for control reduction.
  pm->ps->misc[ MISC_MISC ] = awayFactor * 100.0f;

  // Set timer until the next jump.
  pm->ps->pm_flags |= PMF_TIME_WALLJUMP;
  pm->ps->pm_time = WALLJUMP_TIME;

  // Make sure that we do not have any velocity going into the wall (which may
  // happen if we've moved very close to the wall at a high speed, but still
  // haven't quite made a contact it). Doing this makes sure that the push away
  // from the wall is always fully applied.
  PM_ClipVelocity( pm->ps->velocity, wall.plane.normal, pm->ps->velocity );

  // Break the fall.
  if( pm->ps->velocity[ 2 ] < 0.0f )
    pm->ps->velocity[ 2 ] /= 2.0f;

  // Perform the actual jump.
  pm->ps->velocity[ 2 ] += upSpeed;
  VectorMA( pm->ps->velocity, awaySpeed, wall.plane.normal, pm->ps->velocity );

  // Limit the maximum speed.
  if( VectorLength( pm->ps->velocity ) > maxSpeed )
  {
    VectorNormalize( pm->ps->velocity );
    VectorScale( pm->ps->velocity, maxSpeed, pm->ps->velocity );
  }

  // Play the sound and the animations.
  PM_AddEvent( EV_JUMP );

  if( pm->cmd.forwardmove >= 0 )
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
      PM_ForceLegsAnim( LEGS_JUMP );
    else
      PM_ForceLegsAnim( NSPA_JUMP );

    pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
  }
  else
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
      PM_ForceLegsAnim( LEGS_JUMPB );
    else
      PM_ForceLegsAnim( NSPA_JUMPBACK );

    pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
  }

  return qtrue;
}

/*
==============
PM_GetDesiredVelocity

Gets the desired velocity along the given ground plane.
==============
*/
static void PM_GetDesiredVelocity( vec3_t wishvel, vec3_t normal )
{
  float fmove;    //- Desired forward speed.
  float smove;    //- Desired side speed.
  float scale;    //- Movement scale factor.
  vec3_t forward; //- Forward look direction.
  vec3_t right;   //- Side direction.
  int i;          //- Loop index.

  fmove = pm->cmd.forwardmove;
  smove = pm->cmd.rightmove;

  scale = PM_CmdScale( &pm->cmd, qfalse );

  VectorCopy( pml.forward, forward );
  VectorCopy( pml.right, right );

  // Align the directions with the ground plane.
  if( normal )
  {
    ProjectPointOnPlane( forward, forward, normal );
    ProjectPointOnPlane( right, right, normal );
    VectorNormalize( forward );
    VectorNormalize( right );
  }

  for( i = 0; i < 3; i++ )
    wishvel[ i ] = scale * (forward[ i ] * fmove + right[ i ] * smove);
}

/*
==============
PM_GetDesiredDirAndSpeed

Gets the desired speed and movement direction along the given ground plane.
==============
*/
static void PM_GetDesiredDirAndSpeed( vec3_t dir, float *speed, vec3_t normal )
{
  float tmpSpeed;
  PM_GetDesiredVelocity( dir, normal );
  tmpSpeed = VectorNormalize( dir );
  if( speed ) *speed = tmpSpeed;
}

/*
=============
PM_CheckJump
=============
*/
static qboolean PM_CheckJump( vec3_t customNormal )
{
  // Ground normal under the player.
  vec3_t normal;
  // Where the player wants to go.
  vec3_t wishDir;      
  // Jump speed of the current class.
  float jumpMagnitude;
  // Maximum movement speed that can be gained from jumping repeatedly.
  float maxHoppingSpeed;  
  // Minimum fraction of the jump speed we can add to the horizontal movement.
  float minKickFraction;
  // Maximum fraction of the jump speed we can add to the horizontal movement.
  float maxKickFraction;

  if( BG_Class( pm->ps->stats[ STAT_CLASS ] )->jumpMagnitude == 0.0f )
    return qfalse;

  //can't jump and pounce at the same time
  if( ( pm->ps->weapon == WP_ALEVEL3 ||
        pm->ps->weapon == WP_ALEVEL3_UPG ) &&
      pm->ps->misc[ MISC_MISC ] > 0 )
  {
    // disable bunny hop
    pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;
    return qfalse;
  }

  //can't jump and charge at the same time
  if( ( pm->ps->weapon == WP_ALEVEL4 ) &&
      pm->ps->misc[ MISC_MISC ] > 0 )
  {
    // disable bunny hop
    pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;
    return qfalse;
  }

  if( BG_ClassHasAbility(pm->ps->stats[STAT_CLASS], SCA_STAMINA) &&
      ( pm->ps->stats[ STAT_STAMINA ] < STAMINA_SLOW_LEVEL + STAMINA_JUMP_TAKE ) &&
      ( !BG_InventoryContainsUpgrade( UP_JETPACK, pm->ps->stats ) ||
        pm->ps->stats[ STAT_FUEL ] < JETPACK_ACT_BOOST_FUEL_USE ) )
  {
    // disable bunny hop
    pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;
    return qfalse;
  }


  //no bunny hopping off a dodge
  //SCA_DODGE? -vjr
  if( BG_ClassHasAbility(pm->ps->stats[STAT_CLASS], SCA_STAMINA) &&
      pm->ps->pm_time )
  {
    // disable bunny hop
    pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;
    return qfalse;
  }

  if( pm->ps->pm_flags & PMF_RESPAWNED )
    return qfalse;    // don't allow jump until all buttons are up

  if( pm->cmd.upmove < 10 )
    // not holding jump
    return qfalse;

  // can't jump whilst grabbed
  if( pm->ps->pm_type == PM_GRABBED )
  {
    // disable bunny hop
    pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;
    return qfalse;
  }

  if( !( pm->ps->pm_flags & PMF_JUMP_HELD ) )
  {
    if( !( pm->ps->pm_flags & PMF_JUMPING ) &&
        (pm->ps->pm_flags & PMF_HOPPED) )
      pm->ps->pm_flags |= PMF_BUNNY_HOPPING; // enable hunny hop
  } else if( pm->ps->persistant[PERS_JUMPTIME] < BUNNY_HOP_DELAY ||
             !(pm->ps->pm_flags & PMF_BUNNY_HOPPING) ||
             ( pm->ps->pm_flags & PMF_TIME_WALLJUMP ) ||
             ( pm->ps->pm_flags & PMF_TIME_LAND ) )
  {
    // don't bunnhy hop
    return qfalse;
  }

  // Set timer until the next automatic jump.
  pm->ps->pm_flags |= PMF_TIME_LAND;
  pm->ps->pm_time = 300;

  pml.groundPlane = qfalse;   // jumping away
  pml.walking = qfalse;
  pm->ps->pm_flags |= PMF_JUMP_HELD;
  pm->ps->persistant[PERS_JUMPTIME] = 0;
  pm->ps->pm_flags |= PMF_JUMPING;

  // take some stamina off
  if( BG_ClassHasAbility(pm->ps->stats[STAT_CLASS], SCA_STAMINA) &&
      pm->humanStaminaMode )
    pm->ps->stats[ STAT_STAMINA ] -= STAMINA_JUMP_TAKE;

  pm->ps->groundEntityNum = ENTITYNUM_NONE;

  if( customNormal )
    VectorCopy( customNormal, normal );
  else
    BG_GetClientNormal( pm->ps, normal );

  if( pm->ps->velocity[ 2 ] < 0 )
    pm->ps->velocity[ 2 ] = 0;

  jumpMagnitude = BG_Class( pm->ps->stats[ STAT_CLASS ] )->jumpMagnitude;

  if(PM_IsMarauder()) {
    float upFactor;
    float minFactor = MIN(1.0f, MAX(0.0f, pm->marauderMinJumpFactor));
    float upLook = MAX(0.1f, DotProduct(upNormal, pml.forward));

    upFactor = minFactor + ((1.0f - minFactor) * upLook);

    jumpMagnitude *= upFactor;
  }

  VectorMA( pm->ps->velocity, jumpMagnitude, normal, pm->ps->velocity );

  if( pm->playerAccelMode == 0 && pm->cmd.forwardmove > 0 )
  {
    // Add some ground speed as well, but limit the maximum speed. Returns from
    // subsequent jumps are diminishing, to prevent players from accelerating
    // too much too quickly.

    PM_GetDesiredVelocity( wishDir, normal );
    maxHoppingSpeed = VectorNormalize( wishDir );

    if( pm->ps->stats[ STAT_TEAM ] == TEAM_HUMANS )
    {
      maxHoppingSpeed *= 2.0f;
      minKickFraction = 0.3f;
      maxKickFraction = 0.4f;
    }
    else if( PM_IsDretchOrBasilisk( ) )
    {
      maxHoppingSpeed *= 1.75f;
      maxKickFraction = 0.5f;
      minKickFraction = 0.3f;
    }
    else if( PM_IsMarauder( ) )
    {
      maxHoppingSpeed *= 3.0f;
      maxKickFraction = 0.5f;
      minKickFraction = 0.2f;
    }
    else
    {
      maxHoppingSpeed *= 1.75f;
      maxKickFraction = 0.4f;
      minKickFraction = 0.2f;
    }

    if( maxHoppingSpeed >= 1.0f )
    {
      // How much we are already moving in the desired direction.
      float goodSpeed = DotProduct( pm->ps->velocity, wishDir );
      // How much of the maximum speed we can get from jumping we already have.
      float haveFraction = goodSpeed / maxHoppingSpeed;

      if( haveFraction <= 1.0f )
      {
        // Fraction of the jump speed to add to the horizontal velocity.
        float kickFraction;
        // Player velocity projected onto the ground.
        vec3_t groundVelocity;
        // Non-ground component of the player velocity.
        float nonGroundSpeed;
        // Push the player by this vector.
        vec3_t push;
    
        // Calculate how much we need to push the player.
        kickFraction = MIX( maxKickFraction, minKickFraction, haveFraction );
        VectorScale( wishDir, jumpMagnitude * kickFraction, push );

        // Separate the player speed into the ground and non-ground components.
        ProjectPointOnPlane( groundVelocity, pm->ps->velocity, normal );
        nonGroundSpeed = DotProduct( pm->ps->velocity, normal );

        if( VectorLength( groundVelocity ) )
        {
          // Push the player in the desired direction.
          PM_Push( groundVelocity, push, maxHoppingSpeed, pm->ps->velocity );
          VectorMA( pm->ps->velocity, nonGroundSpeed, normal, pm->ps->velocity );
        }
      }
    }
  }

  PM_AddEvent( EV_JUMP );

  if( pm->cmd.forwardmove >= 0 )
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
      PM_ForceLegsAnim( LEGS_JUMP );
    else
      PM_ForceLegsAnim( NSPA_JUMP );

    pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
  }
  else
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
      PM_ForceLegsAnim( LEGS_JUMPB );
    else
      PM_ForceLegsAnim( NSPA_JUMPBACK );

    pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
  }

  return qtrue;
}

/*
=============
PM_CheckWaterJump
=============
*/
static qboolean PM_CheckWaterJump( void )
{
  vec3_t  start, spot;
  vec3_t  flatforward;
  vec3_t  mins, maxs;
  trace_t trace;

  if( pm->ps->pm_time )
    return qfalse;

  // check for water jump
  if( pm->waterlevel != 2 )
    return qfalse;

  if( pm->cmd.upmove < 10 )
    // not holding jump
    return qfalse;

  VectorCopy( pm->mins, mins );
  VectorCopy( pm->maxs, maxs );

  //flatten the mins and maxs
  maxs[2] = mins[2] = 0;

  //inset the mins and maxs
  maxs[0] -= 1;
  mins[0] += 1;
  if( maxs[0] <= mins[0] )
    maxs[0] = mins[0] = ( pm->maxs[0] + pm->mins[0] ) / 2;
  maxs[1] -= 1;
  mins[1] += 1;
  if( maxs[1] <= mins[1] )
    maxs[1] = mins[1] = ( pm->maxs[1] + pm->mins[1] ) / 2;

  flatforward[ 0 ] = pml.forward[ 0 ];
  flatforward[ 1 ] = pml.forward[ 1 ];
  flatforward[ 2 ] = 0;
  VectorNormalize( flatforward );

  VectorCopy( pm->ps->origin, start );

  start[ 2 ] += 4;

  VectorMA( start, 2.0f, flatforward, spot );

  pm->trace( &trace, start, mins, maxs, spot, pm->ps->clientNum,
             MASK_PLAYERSOLID );

  if( trace.fraction >= 1.0f  )
    return qfalse;

  start[ 2 ] += 16;
  spot[ 2 ] += 16;

  pm->trace( &trace, start, mins, maxs, spot, pm->ps->clientNum,
             MASK_PLAYERSOLID );

  if( trace.fraction < 1.0f  )
    return qfalse;

  // jump out of water
  VectorScale( pml.forward, 200, pm->ps->velocity );
  pm->ps->velocity[ 2 ] = 350;

  pm->ps->pm_flags |= PMF_TIME_WATERJUMP;
  pm->ps->pm_time = 2000;

  return qtrue;
}


/*
==================
PM_CheckDodge

Checks the dodge key and starts a human dodge or sprint
==================
*/
static qboolean PM_CheckDodge( void )
{
  vec3_t right, forward, velocity = { 0.0f, 0.0f, 0.0f };
  float jump, sideModifier;
  int i;

  if( !BG_ClassHasAbility(pm->ps->stats[STAT_CLASS], SCA_STAMINA) )
    return qfalse;

  // Landed a dodge
  if( ( pm->ps->pm_flags & PMF_CHARGE ) &&
      pm->ps->groundEntityNum != ENTITYNUM_NONE )
  {
    pm->ps->pm_flags = ( pm->ps->pm_flags & ~PMF_CHARGE ) | PMF_TIME_LAND;
    pm->ps->pm_time = HUMAN_DODGE_TIMEOUT;
  }

  // Reasons why we can't start a dodge or sprint
  if( pm->ps->pm_type != PM_NORMAL || pm->ps->stats[ STAT_STAMINA ] < STAMINA_SLOW_LEVEL + STAMINA_DODGE_TAKE ||
      ( pm->ps->pm_flags & PMF_DUCKED ) )
    return qfalse;

  // Can't dodge forward
  if( pm->cmd.forwardmove > 0 )
    return qfalse;

  // Reasons why we can't start a dodge only
  if( pm->ps->pm_flags & ( PMF_TIME_LAND | PMF_CHARGE ) ||
      pm->ps->groundEntityNum == ENTITYNUM_NONE ||
      !( pm->cmd.buttons & BUTTON_DODGE ) )
    return qfalse;

  // Dodge direction specified with movement keys
  if( ( !pm->cmd.rightmove && !pm->cmd.forwardmove ) || pm->cmd.upmove )
    return qfalse;

  AngleVectors( pm->ps->viewangles, NULL, right, NULL );
  forward[ 0 ] = -right[ 1 ];
  forward[ 1 ] = right[ 0 ];
  forward[ 2 ] = 0.0f;

  // Dodge magnitude is based on the jump magnitude scaled by the modifiers
  jump = BG_Class( pm->ps->stats[ STAT_CLASS ] )->jumpMagnitude;
  if( pm->cmd.rightmove && pm->cmd.forwardmove )
    jump *= ( 0.5f * M_SQRT2 );

  // Weaken dodge if slowed
  if( ( pm->ps->stats[ STAT_STATE ] & SS_SLOWLOCKED )  ||
      ( pm->ps->stats[ STAT_STATE ] & SS_CREEPSLOWED ) ||
      ( pm->ps->eFlags & EF_POISONCLOUDED ) )
    sideModifier = HUMAN_DODGE_SLOWED_MODIFIER;
  else
    sideModifier = HUMAN_DODGE_SIDE_MODIFIER;

  // The dodge sets minimum velocity
  if( pm->cmd.rightmove )
  {
    if( pm->cmd.rightmove < 0 )
      VectorNegate( right, right );
    VectorMA( velocity, jump * sideModifier, right, velocity );
  }

  if( pm->cmd.forwardmove )
  {
    if( pm->cmd.forwardmove < 0 )
      VectorNegate( forward, forward );
    VectorMA( velocity, jump * sideModifier, forward, velocity );
  }

  velocity[ 2 ] = jump * HUMAN_DODGE_UP_MODIFIER;

  // Make sure client has minimum velocity
  for( i = 0; i < 3; i++ )
  {
    if( ( velocity[ i ] < 0.0f &&
          pm->ps->velocity[ i ] > velocity[ i ] ) ||
        ( velocity[ i ] > 0.0f &&
          pm->ps->velocity[ i ] < velocity[ i ] ) )
      pm->ps->velocity[ i ] = velocity[ i ];
  }

  // Jumped away
  pml.groundPlane = qfalse;
  pml.walking = qfalse;
  pm->ps->groundEntityNum = ENTITYNUM_NONE;
  pm->ps->pm_flags |= PMF_CHARGE;
  pm->ps->stats[ STAT_STAMINA ] -= STAMINA_DODGE_TAKE;
  pm->ps->legsAnim = ( ( pm->ps->legsAnim & ANIM_TOGGLEBIT ) ^
                       ANIM_TOGGLEBIT ) | LEGS_JUMP;
  PM_AddEvent( EV_JUMP );

  return qtrue;
}

//============================================================================


/*
===================
PM_WaterJumpMove

Flying out of the water
===================
*/
static void PM_WaterJumpMove( void )
{
  // waterjump has no control, but falls

  PM_StepSlideMove( qtrue, qfalse );

  pm->ps->velocity[ 2 ] -= PM_Gravity(pm->ps) * pml.frametime;
  if( pm->ps->velocity[ 2 ] < 0 )
  {
    // cancel as soon as we are falling down again
    pm->ps->pm_flags &= ~PMF_ALL_TIMES;
    pm->ps->pm_time = 0;
  }
}

/*
===================
PM_WaterMove

===================
*/
static void PM_WaterMove( void )
{
  int   i;
  vec3_t  wishvel;
  float wishspeed;
  vec3_t  wishdir;
  float scale;
  float vel;

  if( PM_CheckWaterJump( ) )
  {
    PM_WaterJumpMove();
    return;
  }
#if 0
  // jump = head for surface
  if ( pm->cmd.upmove >= 10 ) {
    if (pm->ps->velocity[2] > -300) {
      if ( pm->watertype == CONTENTS_WATER ) {
        pm->ps->velocity[2] = 100;
      } else if (pm->watertype == CONTENTS_SLIME) {
        pm->ps->velocity[2] = 80;
      } else {
        pm->ps->velocity[2] = 50;
      }
    }
  }
#endif
  PM_Friction( );

  scale = PM_CmdScale( &pm->cmd, qtrue );
  //
  // user intentions
  //

  for( i = 0; i < 3; i++ )
    wishvel[ i ] = scale * pml.forward[ i ] * pm->cmd.forwardmove + scale * pml.right[ i ] * pm->cmd.rightmove;

  wishvel[ 2 ] += scale * pm->cmd.upmove;

  VectorCopy( wishvel, wishdir );
  wishspeed = VectorNormalize( wishdir );

  if( wishspeed > pm->ps->speed * pm_swimScale )
    wishspeed = pm->ps->speed * pm_swimScale;

  PM_WallCoast( wishdir, WALLCOAST_3D );
  PM_Accelerate( wishdir, wishspeed, pm_wateraccelerate );

  // make sure we can go up slopes easily under water
  if( pml.groundPlane && DotProduct( pm->ps->velocity, pml.groundTrace.plane.normal ) < 0 )
  {
    vel = VectorLength( pm->ps->velocity );
    // slide along the ground plane
    PM_ClipVelocity( pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity );

    VectorNormalize( pm->ps->velocity );
    VectorScale( pm->ps->velocity, vel, pm->ps->velocity );
  }

  PM_SlideMove( qfalse );
}

/*
===================
PM_JetPackMove

Only with the jetpack
===================
*/
static void PM_JetPackMove( void )
{
  int     i;
  vec3_t  wishvel;
  float   wishspeed;
  vec3_t  wishdir;
  float   scale;

  //normal slowdown
  PM_Friction( );

  scale = PM_CmdScale( &pm->cmd, qfalse );

  // user intentions
  for( i = 0; i < 2; i++ )
    wishvel[ i ] = scale * pml.forward[ i ] * pm->cmd.forwardmove + scale * pml.right[ i ] * pm->cmd.rightmove;

  wishvel[2] = 0.0f;

  if( pm->cmd.upmove > 0.0f )
  {
    vec3_t thrustDir = { 0.0f, 0.0f, 1.0f };

    if( pm->ps->viewangles[ PITCH ] > 0.0f )
      VectorCopy( pml.up, thrustDir ) ;

    // give an initial vertical boost when first activating the jet
    if( pm->ps->persistant[PERS_JUMPTIME] <= JETPACK_ACT_BOOST_TIME )
      VectorMA( wishvel, JETPACK_ACT_BOOST_SPEED, thrustDir, wishvel );
    else
      VectorMA( wishvel, JETPACK_FLOAT_SPEED, thrustDir, wishvel );
  }
  else if( pm->cmd.upmove < 0.0f )
    wishvel[ 2 ] = -JETPACK_SINK_SPEED;
  else
    wishvel[ 2 ] = 0.0f;

  // apply the appropirate amount of gravity if the upward velocity is greater
  // than the max upward jet velocity.
  if( pm->ps->velocity[ 2 ] > wishvel[ 2 ] )
    pm->ps->velocity[ 2 ] -= MIN( PM_Gravity(pm->ps) * pml.frametime, pm->ps->velocity[ 2 ] - wishvel[ 2 ] );

  VectorCopy( wishvel, wishdir );
  wishspeed = VectorNormalize( wishdir );

  PM_Accelerate( wishdir, wishspeed, pm_flyaccelerate );

  PM_StepSlideMove( qfalse, qfalse );

  if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
    PM_ContinueLegsAnim( LEGS_LAND );
  else
    PM_ContinueLegsAnim( NSPA_LAND );

  // disable bunny hop
  pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;
}




/*
===================
PM_FlyMove

Only with the flight powerup
===================
*/
static void PM_FlyMove( void )
{
  int     i;
  vec3_t  wishvel;
  float   wishspeed;
  vec3_t  wishdir;
  float   scale;

  // normal slowdown
  PM_Friction( );

  scale = PM_CmdScale( &pm->cmd, qtrue );
  //
  // user intentions
  //
  if( !scale )
  {
    wishvel[ 0 ] = 0;
    wishvel[ 1 ] = 0;
    wishvel[ 2 ] = 0;
  }
  else
  {
    for( i = 0; i < 3; i++ )
      wishvel[ i ] = scale * pml.forward[ i ] * pm->cmd.forwardmove + scale * pml.right[ i ] * pm->cmd.rightmove;

    wishvel[ 2 ] += scale * pm->cmd.upmove;
  }

  VectorCopy( wishvel, wishdir );
  wishspeed = VectorNormalize( wishdir );

  PM_WallCoast( wishdir, WALLCOAST_3D );
  PM_Accelerate( wishdir, wishspeed, pm_flyaccelerate );

  PM_StepSlideMove( qfalse, qfalse );
}


/*
===================
PM_AirMove

===================
*/
static void PM_AirMove( void )
{
  if(pm->playerAccelMode == 0) {
    vec3_t wishdir;
    float wishspeed;
    float controlFactor;
    float accel = BG_Class( pm->ps->stats[ STAT_CLASS ] )->airAcceleration;

    // Set the movementDir so clients can rotate the legs for strafing.
    PM_SetMovementDir( );

    PM_GetDesiredDirAndSpeed( wishdir, &wishspeed, upNormal );
    PM_CheckWallJump( wishdir, wishspeed );
    PM_Friction( );

    // Reduce air control immediately after a wall jump to give the player some
    // time to actually move a small distance away from the wall even when moving
    // directly towards it.
    if( pm->ps->pm_flags & PMF_TIME_WALLJUMP )
    {
      float awayFactor = pm->ps->misc[ MISC_MISC ] / 100.0f;
      controlFactor = ( float ) pm->ps->pm_time / WALLJUMP_TIME;
      controlFactor = pow( controlFactor, awayFactor * 6.0f );
    }
    else
      controlFactor = 1.0f;

    PM_WallCoast( wishdir, WALLCOAST_GROUNDED );

    // Give marauders more air control. Let them redirect their momentum in the
    // air and increase their acceleration when they accelerate against their
    // current velocity. Some drag is also applied, to prevent them from keeping
    // all of their speed when turning.
    if( PM_IsMarauder( ) && wishspeed >= 0.1f )
    {
      PM_RedirectMomentum( wishdir, wishspeed, 0.75f * controlFactor );
      PM_RudderDrag( wishdir, wishspeed * 1.35f, wishspeed * 3.0f, 1500.0f );
      PM_BoostBraking( wishdir, wishspeed, wishspeed * 1.75f, 1.5f, &accel );
    }

    PM_AccelerateHorizontal( wishdir, wishspeed, accel * controlFactor );
  } else {
    int       i;
    vec3_t    wishvel;
    float     fmove, smove;
    vec3_t    wishdir;
    float     wishspeed;
    float     scale;
    usercmd_t cmd;

    PM_Friction( );

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    cmd = pm->cmd;
    scale = PM_CmdScale( &cmd, qfalse );

    // set the movementDir so clients can rotate the legs for strafing
    PM_SetMovementDir( );

    // project moves down to flat plane
    pml.forward[ 2 ] = 0;
    pml.right[ 2 ] = 0;
    VectorNormalize( pml.forward );
    VectorNormalize( pml.right );

    for( i = 0; i < 2; i++ )
      wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;

    wishvel[ 2 ] = 0;

    VectorCopy( wishvel, wishdir );
    wishspeed = VectorNormalize( wishdir );
    wishspeed *= scale;

    PM_CheckWallJump( wishdir, wishspeed );

    // not on ground, so little effect on velocity
    PM_AccelerateHorizontal( wishdir, wishspeed,
      BG_Class( pm->ps->stats[ STAT_CLASS ] )->airAcceleration );
  }

  // We may have a ground plane that is very steep, even though we don't have a
  // groundentity slide along the steep plane.
  if( pml.groundPlane )
  {
    float *groundNormal = pml.groundTrace.plane.normal;
    float previousZ = pm->ps->origin[ 2 ];

    PM_ClipVelocity( pm->ps->velocity, groundNormal, pm->ps->velocity );
    PM_StepSlideMove( qtrue, qfalse );

    // Check if we haven't moved down despite the gravity pulling on us. If this
    // is the case, we're probably stuck between two steep planes, allow jumping
    // as if on a level ground.
    if( abs( PM_Gravity(pm->ps) ) > 0 &&
        fabs( pm->ps->origin[ 2 ] - previousZ ) < 0.05f )
      PM_CheckJump( upNormal );
  }
  else
  {
    PM_StepSlideMove( qtrue, qfalse );
  }
}

/*
===================
PM_ClimbMove

===================
*/
static void PM_ClimbMove( void )
{
  int       i;
  vec3_t    wishvel;
  float     fmove, smove;
  vec3_t    wishdir;
  float     wishspeed;
  float     scale;
  usercmd_t cmd;
  float     accelerate;
  float     vel;

  if( pm->waterlevel > 2 && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 )
  {
    // begin swimming
    PM_WaterMove( );
    return;
  }


  if( PM_CheckJump( NULL ) || PM_CheckPounce( ) )
  {
    // jumped away
    if( pm->waterlevel > 1 )
      PM_WaterMove( );
    else
      PM_AirMove( );

    return;
  }

  PM_Friction( );

  fmove = pm->cmd.forwardmove;
  smove = pm->cmd.rightmove;

  cmd = pm->cmd;
  scale = PM_CmdScale( &cmd, qfalse );

  // set the movementDir so clients can rotate the legs for strafing
  PM_SetMovementDir( );

  // project the forward and right directions onto the ground plane
  PM_ClipVelocity( pml.forward, pml.groundTrace.plane.normal, pml.forward );
  PM_ClipVelocity( pml.right, pml.groundTrace.plane.normal, pml.right );
  //
  VectorNormalize( pml.forward );
  VectorNormalize( pml.right );

  for( i = 0; i < 3; i++ )
    wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;

  // when going up or down slopes the wish velocity should Not be zero
//  wishvel[2] = 0;

  VectorCopy( wishvel, wishdir );
  wishspeed = VectorNormalize( wishdir );
  wishspeed *= scale;

  // clamp the speed lower if ducking
  if( pm->ps->pm_flags & PMF_DUCKED )
  {
    if( wishspeed > pm->ps->speed * pm_duckScale )
      wishspeed = pm->ps->speed * pm_duckScale;
  }

  // clamp the speed lower if wading or walking on the bottom
  if( pm->waterlevel )
  {
    float waterScale;

    waterScale = pm->waterlevel / 3.0;
    waterScale = 1.0 - ( 1.0 - pm_swimScale ) * waterScale;
    if( wishspeed > pm->ps->speed * waterScale )
      wishspeed = pm->ps->speed * waterScale;
  }

  // when a player gets hit, they temporarily lose
  // full control, which allows them to be moved a bit
  if( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK )
    accelerate = BG_Class( pm->ps->stats[ STAT_CLASS ] )->airAcceleration;
  else
    accelerate = BG_Class( pm->ps->stats[ STAT_CLASS ] )->acceleration;

  PM_Accelerate( wishdir, wishspeed, accelerate );

  if( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK )
    pm->ps->velocity[ 2 ] -= PM_Gravity(pm->ps) * pml.frametime;

  vel = VectorLength( pm->ps->velocity );

  // slide along the ground plane
  PM_ClipVelocity( pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity );

  // don't decrease velocity when going up or down a slope
  VectorNormalize( pm->ps->velocity );
  VectorScale( pm->ps->velocity, vel, pm->ps->velocity );

  // don't do anything if standing still
  if( !pm->ps->velocity[ 0 ] && !pm->ps->velocity[ 1 ] && !pm->ps->velocity[ 2 ] )
    return;

  PM_StepSlideMove( qfalse, qfalse );
}


/*
===================
PM_WalkMove

===================
*/
static void PM_WalkMove( void )
{
  int       i;
  vec3_t    wishvel;
  float     fmove, smove;
  vec3_t    wishdir;
  float     wishspeed;
  float     scale;
  usercmd_t cmd;
  float     accelerate;

  if( pm->waterlevel > 2 && DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0 )
  {
    // begin swimming
    PM_WaterMove( );
    return;
  }

  if( PM_CheckJump( NULL ) || PM_CheckPounce( ) )
  {
    // jumped away
    if( pm->waterlevel > 1 )
      PM_WaterMove( );
    else
      PM_AirMove( );

    return;
  }

  //charging
  PM_CheckCharge( );

  PM_Friction( );

  fmove = pm->cmd.forwardmove;
  smove = pm->cmd.rightmove;

  cmd = pm->cmd;
  scale = PM_CmdScale( &cmd, qfalse );

  // set the movementDir so clients can rotate the legs for strafing
  PM_SetMovementDir( );

  // project moves down to flat plane
  pml.forward[ 2 ] = 0;
  pml.right[ 2 ] = 0;

  // project the forward and right directions onto the ground plane
  PM_ClipVelocity( pml.forward, pml.groundTrace.plane.normal, pml.forward );
  PM_ClipVelocity( pml.right, pml.groundTrace.plane.normal, pml.right );
  //
  VectorNormalize( pml.forward );
  VectorNormalize( pml.right );

  for( i = 0; i < 3; i++ )
    wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;

  // when going up or down slopes the wish velocity should Not be zero
//  wishvel[2] = 0;

  VectorCopy( wishvel, wishdir );
  wishspeed = VectorNormalize( wishdir );
  wishspeed *= scale;

  // clamp the speed lower if ducking
  if( pm->ps->pm_flags & PMF_DUCKED )
  {
    if( wishspeed > pm->ps->speed * pm_duckScale )
      wishspeed = pm->ps->speed * pm_duckScale;
  }

  // clamp the speed lower if wading or walking on the bottom
  if( pm->waterlevel )
  {
    float waterScale;

    waterScale = pm->waterlevel / 3.0;
    waterScale = 1.0 - ( 1.0 - pm_swimScale ) * waterScale;
    if( wishspeed > pm->ps->speed * waterScale )
      wishspeed = pm->ps->speed * waterScale;
  }

  // when a player gets hit, they temporarily lose
  // full control, which allows them to be moved a bit
  if( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK )
    accelerate = BG_Class( pm->ps->stats[ STAT_CLASS ] )->airAcceleration;
  else
    accelerate = BG_Class( pm->ps->stats[ STAT_CLASS ] )->acceleration;

  PM_WallCoast( wishdir, WALLCOAST_GROUNDED );
  PM_Accelerate( wishdir, wishspeed, accelerate );

  //Com_Printf("velocity = %1.1f %1.1f %1.1f\n", pm->ps->velocity[0], pm->ps->velocity[1], pm->ps->velocity[2]);
  //Com_Printf("velocity1 = %1.1f\n", VectorLength(pm->ps->velocity));

  if( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK )
    pm->ps->velocity[ 2 ] -= PM_Gravity(pm->ps) * pml.frametime;
  else
  {
    // don't reset the z velocity for slopes
//    pm->ps->velocity[2] = 0;
  }

  // slide along the ground plane
  PM_ClipVelocity( pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity );

  // don't do anything if standing still
  if( !pm->ps->velocity[ 0 ] && !pm->ps->velocity[ 1 ] )
    return;

  PM_StepSlideMove( qfalse, qfalse );

  //Com_Printf("velocity2 = %1.1f\n", VectorLength(pm->ps->velocity));

}


/*
===================
PM_LadderMove

Basically a rip of PM_WaterMove with a few changes
===================
*/
static void PM_LadderMove( void )
{
  int     i;
  vec3_t  wishvel;
  float   wishspeed;
  vec3_t  wishdir;
  float   scale;
  float   vel;

  PM_Friction( );

  scale = PM_CmdScale( &pm->cmd, qtrue );

  for( i = 0; i < 3; i++ )
    wishvel[ i ] = scale * pml.forward[ i ] * pm->cmd.forwardmove + scale * pml.right[ i ] * pm->cmd.rightmove;

  wishvel[ 2 ] += scale * pm->cmd.upmove;

  VectorCopy( wishvel, wishdir );
  wishspeed = VectorNormalize( wishdir );

  if( wishspeed > pm->ps->speed * pm_swimScale )
    wishspeed = pm->ps->speed * pm_swimScale;

  PM_Accelerate( wishdir, wishspeed, pm_accelerate );

  //slanty ladders
  if( pml.groundPlane && DotProduct( pm->ps->velocity, pml.groundTrace.plane.normal ) < 0.0f )
  {
    vel = VectorLength( pm->ps->velocity );

    // slide along the ground plane
    PM_ClipVelocity( pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity );

    VectorNormalize( pm->ps->velocity );
    VectorScale( pm->ps->velocity, vel, pm->ps->velocity );
  }

  PM_SlideMove( qfalse );
}

/*
=============
PM_CheckLadder

Check to see if the player is on a ladder or not
=============
*/
static void PM_CheckLadder( void )
{
  vec3_t   flatforward, end, mins, maxs;
  trace_t  trace, bboxTouchTrace;
  qboolean waterClimb = qfalse;

  //find the "flat forward" that is horizontal with the map
  VectorCopy( pml.forward, flatforward );
  flatforward[ 2 ] = 0.0f;
  VectorNormalize( flatforward );

  VectorMA( pm->ps->origin, 2.0f, flatforward, end );

  // inset the mins and maxs
  VectorCopy( pm->mins, mins);
  VectorCopy( pm->maxs, maxs);
  maxs[0] -= 1;
  mins[0] += 1;
  if( maxs[0] <= mins[0] )
    maxs[0] = mins[0] = ( pm->maxs[0] + pm->mins[0] ) / 2;
  maxs[1] -= 1;
  mins[1] += 1;
  if( maxs[1] <= mins[1] )
    maxs[1] = mins[1] = ( pm->maxs[1] + pm->mins[1] ) / 2;
  maxs[2] -= 1;
  mins[2] += 1;
  if( maxs[2] <= mins[2] )
    maxs[2] = mins[2] = ( pm->maxs[2] + pm->mins[2] ) / 2;

  // trace to check for touching a potential ladder surface
  pm->trace( &bboxTouchTrace, pm->ps->origin, mins, maxs, end,
             pm->ps->clientNum, MASK_PLAYERSOLID );

  if( bboxTouchTrace.fraction >= 1 &&
      !bboxTouchTrace.startsolid &&
      !bboxTouchTrace.allsolid )
  {
    // no solid surface in front of the player
    pml.ladder = qfalse;
    return;
  }

  if( pm->waterlevel >= 1 &&
      pm->waterlevel < 3 &&
      !(pm->ps->eFlags & EF_WALLCLIMB) &&
      !PM_PredictStepMove( ) )
  {
    vec3_t start;

    //for climbing out of water

    if( PM_CheckWaterJump( ) )
    {
      PM_WaterJumpMove();
      pml.ladder = qfalse;
      return;
    }

    VectorCopy( pm->ps->origin, start );

    start[2] += pm->ps->viewheight;

    //make the mins and maxs a bit larger than the next traces
    mins[2] = -0.5f;
    maxs[2] = 0.5f;
    mins[1] += -0.5f;
    maxs[1] += 0.5f;
    mins[0] += -0.5f;
    maxs[0] += 0.5f;

    VectorMA( start, 2.0f, flatforward, end );

    pm->trace( &trace, start, mins, maxs, end, pm->ps->clientNum,
               MASK_PLAYERSOLID );

    if( trace.fraction >= 1.0f &&
        !trace.startsolid &&
        !trace.allsolid )
    {
      vec3_t bboxBottom;
      vec3_t dryTraceStart, dryTraceEnd;
      float  waterLevelDistance;
      float  drySurfaceDistance;

      VectorCopy( trace.endpos, dryTraceStart );
      VectorCopy( dryTraceStart, dryTraceEnd );
      dryTraceEnd[2] += pm->mins[2] - pm->ps->viewheight;

      //flatten and shrink the mins and maxs to the origin
      maxs[2] = mins[2] = 0;
      mins[1] += 0.5f;
      maxs[1] += -0.5f;
      mins[0] += 0.5f;
      maxs[0] += -0.5f;

      //find how far below the viewheight the dry surface is
      pm->trace( &trace, dryTraceStart, mins, maxs, dryTraceEnd,
                 pm->ps->clientNum, MASK_PLAYERSOLID );

      if( trace.startsolid || trace.allsolid )
        drySurfaceDistance = 0;
      else
        drySurfaceDistance = trace.fraction * ( ( (float)pm->ps->viewheight ) - pm->mins[ 2 ] );

      //find 1 QU below the bottom of the player's bbox
      VectorCopy( pm->ps->origin, bboxBottom );
      bboxBottom[2] += ( pm->mins[2] - 1 );

      // find how far below the viewheight the water level is
      pm->trace( &trace, start, mins, maxs, bboxBottom, pm->ps->clientNum,
                 MASK_WATER );

      if( trace.fraction < 1.0f )
      {
        if( trace.startsolid || trace.allsolid )
          waterLevelDistance = 0.0f;
        else
          waterLevelDistance = trace.fraction * ( ( (float)pm->ps->viewheight ) - pm->mins[ 2 ] - 1 );

        if( STEPSIZE >= ( waterLevelDistance - drySurfaceDistance ) )
        {
          waterClimb = qtrue;
        } else if( !BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_CANUSELADDERS ) )
        {
          //class can't use ladders
          pml.ladder = qfalse;
          return;
        }
      } else if( !BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_CANUSELADDERS ) )
      {
        //class can't use ladders
        pml.ladder = qfalse;
        return;
      }
    } else if( !BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_CANUSELADDERS ) )
    {
      //class can't use ladders
      pml.ladder = qfalse;
      return;
    }
  }
  else if( !BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_CANUSELADDERS ) )
  {
    //class can't use ladders
    pml.ladder = qfalse;
    return;
  }

  if( ( bboxTouchTrace.surfaceFlags & SURF_LADDER ) ||
        ( waterClimb && !(bboxTouchTrace.surfaceFlags & (SURF_SLICK|SURF_SKY)) &&
          ( bboxTouchTrace.plane.normal[ 2 ] < MIN_WALK_NORMAL ) ) )
  {
    pml.ladder = qtrue;

    if( BG_InventoryContainsUpgrade( UP_JETPACK, pm->ps->stats ) &&
        BG_UpgradeIsActive( UP_JETPACK, pm->ps->stats ) )
    {
      BG_DeactivateUpgrade( UP_JETPACK, pm->ps->stats );
      pm->ps->pm_flags &= ~PMF_FEATHER_FALL;
      pm->ps->pm_type = PM_NORMAL;
    }
  }
  else
    pml.ladder = qfalse;
}


/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove( void )
{
  float forward;

  if( !pml.walking )
    return;

  // extra friction

  forward = VectorLength( pm->ps->velocity );
  forward -= 20;

  if( forward <= 0 )
    VectorClear( pm->ps->velocity );
  else
  {
    VectorNormalize( pm->ps->velocity );
    VectorScale( pm->ps->velocity, forward, pm->ps->velocity );
  }
}


/*
===============
PM_NoclipMove
===============
*/
static void PM_NoclipMove( void )
{
  float   speed, drop, friction, control, newspeed;
  int     i;
  vec3_t  wishvel;
  float   fmove, smove;
  vec3_t  wishdir;
  float   wishspeed;
  float   scale;

  // friction

  speed = VectorLength( pm->ps->velocity );

  if( speed < 1 )
  {
    VectorCopy( vec3_origin, pm->ps->velocity );
  }
  else
  {
    drop = 0;

    friction = pm_friction * 1.5; // extra friction
    control = speed < pm_stopspeed ? pm_stopspeed : speed;
    drop += control * friction * pml.frametime;

    // scale the velocity
    newspeed = speed - drop;

    if( newspeed < 0 )
      newspeed = 0;

    newspeed /= speed;

    VectorScale( pm->ps->velocity, newspeed, pm->ps->velocity );
  }

  // accelerate
  scale = PM_CmdScale( &pm->cmd, qtrue );

  fmove = pm->cmd.forwardmove;
  smove = pm->cmd.rightmove;

  for( i = 0; i < 3; i++ )
    wishvel[ i ] = pml.forward[ i ] * fmove + pml.right[ i ] * smove;

  wishvel[ 2 ] += pm->cmd.upmove;

  VectorCopy( wishvel, wishdir );
  wishspeed = VectorNormalize( wishdir );
  wishspeed *= scale;

  PM_Accelerate( wishdir, wishspeed, pm_accelerate );

  // move
  VectorMA( pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin );
}

//============================================================================

/*
================
PM_FootstepForSurface

Returns an event number apropriate for the groundsurface
================
*/
static int PM_FootstepForSurface( void )
{
  if( pm->ps->stats[ STAT_STATE ] & SS_CREEPSLOWED )
    return EV_FOOTSTEP_SQUELCH;

  if( pml.groundTrace.surfaceFlags & SURF_NOSTEPS )
    return 0;

  if( pml.groundTrace.surfaceFlags & SURF_METALSTEPS )
    return EV_FOOTSTEP_METAL;

  return EV_FOOTSTEP;
}


/*
=================
PM_CrashLand

Check for hard landings that generate sound events
=================
*/
static void PM_CrashLand( void )
{
  float   delta;
  float   dist;
  float   vel, acc;
  float   t;
  float   a, b, c, den;

  // decide which landing animation to use
  if( pm->ps->pm_flags & PMF_BACKWARDS_JUMP )
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
      PM_ForceLegsAnim( LEGS_LANDB );
    else
      PM_ForceLegsAnim( NSPA_LANDBACK );
  }
  else
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
      PM_ForceLegsAnim( LEGS_LAND );
    else
      PM_ForceLegsAnim( NSPA_LAND );
  }

  if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
    pm->ps->legsTimer = TIMER_LAND;
  else
    pm->ps->torsoTimer = TIMER_LAND;

  // calculate the exact velocity on landing
  dist = pm->ps->origin[ 2 ] - pml.previous_origin[ 2 ];
  vel = pml.previous_velocity[ 2 ];
  acc = -PM_Gravity(pm->ps);

  a = acc / 2;
  b = vel;
  c = -dist;

  den =  b * b - 4 * a * c;
  if( den < 0 )
    return;

  t = (-b - sqrt( den ) ) / ( 2 * a );

  delta = vel + t * acc;
  delta = delta*delta * 0.0001;

  // never take falling damage if completely underwater
  if( pm->waterlevel == 3 )
    return;

  // reduce falling damage if there is standing water
  if( pm->waterlevel == 2 )
    delta *= 0.25;

  if( pm->waterlevel == 1 )
    delta *= 0.5;

  if( delta < 1 )
    return;

  // create a local entity event to play the sound

  // SURF_NODAMAGE is used for bounce pads where you don't ever
  // want to take damage or play a crunch sound
  if( !( pml.groundTrace.surfaceFlags & SURF_NODAMAGE ) )
  {
    pm->ps->stats[ STAT_FALLDIST ] = delta;

    if( delta > AVG_FALL_DISTANCE )
    {
      if( PM_Alive( pm->ps->pm_type ) )
        PM_AddEvent( EV_FALL_FAR );
    }
    else if( delta > MIN_FALL_DISTANCE )
    {
      if( PM_Alive( pm->ps->pm_type ) )
        PM_AddEvent( EV_FALL_MEDIUM );
    }
    else
    {
      if( delta > 7 )
        PM_AddEvent( EV_FALL_SHORT );
      else
        PM_AddEvent( PM_FootstepForSurface( ) );
    }
  }

  // start footstep cycle over
  pm->ps->misc[MISC_BOB_CYCLE] = 0;
}


/*
=============
PM_CorrectAllSolid
=============
*/
static int PM_CorrectAllSolid(trace_t *trace) {
  int       i, j, k;
  vec3_t    point;

  if(pm->debugLevel) {
    Com_Printf("%i:allsolid\n", c_pmove);
  }

  // jitter around
  for(i = -1; i <= 1; i++) {
    for(j = -1; j <= 1; j++) {
      for(k = -1; k <= 1; k++) {
        VectorCopy( pm->ps->origin, point );
        point[0] += (float)i;
        point[1] += (float)j;
        point[2] += (float)k;
        pm->trace( trace, point, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );

        if( pm->debugLevel ) {
          Com_Printf("%i:trace->allsolid is %d\n", c_pmove, trace->allsolid);
        }

        if(!trace->allsolid) {
          point[0] = pm->ps->origin[0];
          point[1] = pm->ps->origin[1];
          point[2] = pm->ps->origin[2] - 0.25;

          pm->trace( trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );
          pml.groundTrace = *trace;
          return qtrue;
        }
      }
    }
  }

  pm->ps->groundEntityNum = ENTITYNUM_NONE;
  pml.groundPlane = qfalse;
  pml.walking = qfalse;

  return qfalse;
}


/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
static void PM_GroundTraceMissed( void )
{
  trace_t   trace;
  vec3_t    point;

  if( pm->ps->groundEntityNum != ENTITYNUM_NONE )
  {
    // we just transitioned into freefall
    if( pm->debugLevel )
      Com_Printf( "%i:lift\n", c_pmove );

    // if they aren't in a jumping animation and the ground is a ways away, force into it
    // if we didn't do the trace, the player would be backflipping down staircases
    VectorCopy( pm->ps->origin, point );
    point[ 2 ] -= 64.0f;

    pm->trace( &trace, pm->ps->origin, NULL, NULL, point, pm->ps->clientNum, pm->tracemask );
    if( trace.fraction == 1.0f )
    {
      if( pm->cmd.forwardmove >= 0 )
      {
        if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
          PM_ForceLegsAnim( LEGS_JUMP );
        else
          PM_ForceLegsAnim( NSPA_JUMP );

        pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
      }
      else
      {
        if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
          PM_ForceLegsAnim( LEGS_JUMPB );
        else
          PM_ForceLegsAnim( NSPA_JUMPBACK );

        pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
      }
    }
  }

  if( BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_TAKESFALLDAMAGE ) )
  {
    if( pm->ps->velocity[ 2 ] < FALLING_THRESHOLD && pml.previous_velocity[ 2 ] >= FALLING_THRESHOLD )
    {
      // disable bunny hop
      pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;

      PM_AddEvent( EV_FALLING );
    }
  }

  pm->ps->groundEntityNum = ENTITYNUM_NONE;
  pml.groundPlane = qfalse;
  pml.walking = qfalse;
}

/*
=============
PM_GroundClimbTrace
=============
*/
static void PM_GroundClimbTrace( void )
{
  vec3_t      surfNormal, movedir, lookdir, point;
  vec3_t      refNormal = { 0.0f, 0.0f, 1.0f };
  vec3_t      ceilingNormal = { 0.0f, 0.0f, -1.0f };
  vec3_t      toAngles, surfAngles;
  vec3_t      impactVelocity;
  trace_t     trace;
  int         i;
  const float eps = 0.000001f;

  //used for delta correction
  vec3_t    traceCROSSsurf, traceCROSSref, surfCROSSref;
  float     traceDOTsurf, traceDOTref, surfDOTref, rTtDOTrTsTt;
  float     traceANGsurf, traceANGref, surfANGref;
  vec3_t    horizontal = { 1.0f, 0.0f, 0.0f }; //arbituary vector perpendicular to refNormal
  vec3_t    refTOtrace, refTOsurfTOtrace, tempVec;
  int       rTtANGrTsTt;
  float     ldDOTtCs, d;
  vec3_t    abc;

  BG_GetClientNormal( pm->ps, surfNormal );

  //construct a vector which reflects the direction the player is looking wrt the surface normal
  ProjectPointOnPlane( movedir, pml.forward, surfNormal );
  VectorNormalize( movedir );

  VectorCopy( movedir, lookdir );

  if( pm->cmd.forwardmove < 0 )
    VectorNegate( movedir, movedir );

  //allow strafe transitions
  if( pm->cmd.rightmove )
  {
    VectorCopy( pml.right, movedir );

    if( pm->cmd.rightmove < 0 )
      VectorNegate( movedir, movedir );
  }

  for( i = 0; i <= 4; i++ )
  {
    switch ( i )
    {
      case 0:
        //we are going to step this frame so skip the transition test
        if( PM_PredictStepMove( ) )
          continue;

        //trace into direction we are moving
        VectorMA( pm->ps->origin, 0.25f, movedir, point );
        pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );
        break;

      case 1:
        //trace straight down anto "ground" surface
        //mask out CONTENTS_BODY to not hit other players and avoid the camera flipping out when
        // wallwalkers touch
        VectorMA( pm->ps->origin, -0.25f, surfNormal, point );
        pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask & ~CONTENTS_BODY );
        break;

      case 2:
        if( pml.groundPlane != qfalse && PM_PredictStepMove( ) )
        {
          //step down
          VectorMA( pm->ps->origin, -STEPSIZE, surfNormal, point );
          pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );
        }
        else
          continue;
        break;

      case 3:
        //trace "underneath" BBOX so we can traverse angles > 180deg
        if( pml.groundPlane != qfalse )
        {
          VectorMA( pm->ps->origin, -16.0f, surfNormal, point );
          VectorMA( point, -16.0f, movedir, point );
          pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );
        }
        else
          continue;
        break;

      case 4:
        //fall back so we don't have to modify PM_GroundTrace too much
        VectorCopy( pm->ps->origin, point );
        point[ 2 ] = pm->ps->origin[ 2 ] - 0.25f;
        pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );
        break;
    }

    //if we hit something
    if( trace.fraction < 1.0f && !( trace.surfaceFlags & ( SURF_SKY | SURF_SLICK ) ) &&
        !( trace.entityNum != ENTITYNUM_WORLD && i != 4 ) )
    {
      if( i == 2 || i == 3 )
      {
        if( i == 2 )
          PM_StepEvent( pm->ps->origin, trace.endpos, surfNormal );

        VectorCopy( trace.endpos, pm->ps->origin );
      }

      //calculate a bunch of stuff...
      CrossProduct( trace.plane.normal, surfNormal, traceCROSSsurf );
      VectorNormalize( traceCROSSsurf );

      CrossProduct( trace.plane.normal, refNormal, traceCROSSref );
      VectorNormalize( traceCROSSref );

      CrossProduct( surfNormal, refNormal, surfCROSSref );
      VectorNormalize( surfCROSSref );

      //calculate angle between surf and trace
      traceDOTsurf = DotProduct( trace.plane.normal, surfNormal );
      traceANGsurf = RAD2DEG( acos( traceDOTsurf ) );

      if( traceANGsurf > 180.0f )
        traceANGsurf -= 180.0f;

      //calculate angle between trace and ref
      traceDOTref = DotProduct( trace.plane.normal, refNormal );
      traceANGref = RAD2DEG( acos( traceDOTref ) );

      if( traceANGref > 180.0f )
        traceANGref -= 180.0f;

      //calculate angle between surf and ref
      surfDOTref = DotProduct( surfNormal, refNormal );
      surfANGref = RAD2DEG( acos( surfDOTref ) );

      if( surfANGref > 180.0f )
        surfANGref -= 180.0f;

      //if the trace result and old surface normal are different then we must have transided to a new
      //surface... do some stuff...
      if( !VectorCompareEpsilon( trace.plane.normal, surfNormal, eps ) )
      {
        //if the trace result or the old vector is not the floor or ceiling correct the YAW angle
        if( !VectorCompareEpsilon( trace.plane.normal, refNormal, eps ) && !VectorCompareEpsilon( surfNormal, refNormal, eps ) &&
            !VectorCompareEpsilon( trace.plane.normal, ceilingNormal, eps ) && !VectorCompareEpsilon( surfNormal, ceilingNormal, eps ) )
        {
          //behold the evil mindfuck from hell
          //it has fucked mind like nothing has fucked mind before

          //calculate reference rotated through to trace plane
          RotatePointAroundVector( refTOtrace, traceCROSSref, horizontal, -traceANGref );

          //calculate reference rotated through to surf plane then to trace plane
          RotatePointAroundVector( tempVec, surfCROSSref, horizontal, -surfANGref );
          RotatePointAroundVector( refTOsurfTOtrace, traceCROSSsurf, tempVec, -traceANGsurf );

          //calculate angle between refTOtrace and refTOsurfTOtrace
          rTtDOTrTsTt = DotProduct( refTOtrace, refTOsurfTOtrace );
          rTtANGrTsTt = ANGLE2SHORT( RAD2DEG( acos( rTtDOTrTsTt ) ) );

          if( rTtANGrTsTt > 32768 )
            rTtANGrTsTt -= 32768;

          CrossProduct( refTOtrace, refTOsurfTOtrace, tempVec );
          VectorNormalize( tempVec );
          if( DotProduct( trace.plane.normal, tempVec ) > 0.0f )
            rTtANGrTsTt = -rTtANGrTsTt;

          //phew! - correct the angle
          pm->ps->delta_angles[ YAW ] -= rTtANGrTsTt;
        }

        //construct a plane dividing the surf and trace normals
        CrossProduct( traceCROSSsurf, surfNormal, abc );
        VectorNormalize( abc );
        d = DotProduct( abc, pm->ps->origin );

        //construct a point representing where the player is looking
        VectorAdd( pm->ps->origin, lookdir, point );

        //check whether point is on one side of the plane, if so invert the correction angle
        if( ( abc[ 0 ] * point[ 0 ] + abc[ 1 ] * point[ 1 ] + abc[ 2 ] * point[ 2 ] - d ) > 0 )
          traceANGsurf = -traceANGsurf;

        //find the . product of the lookdir and traceCROSSsurf
        if( ( ldDOTtCs = DotProduct( lookdir, traceCROSSsurf ) ) < 0.0f )
        {
          VectorInverse( traceCROSSsurf );
          ldDOTtCs = DotProduct( lookdir, traceCROSSsurf );
        }

        //set the correction angle
        traceANGsurf *= 1.0f - ldDOTtCs;

        if( !( pm->ps->persistant[ PERS_STATE ] & PS_WALLCLIMBINGFOLLOW ) )
        {
          //correct the angle
          pm->ps->delta_angles[ PITCH ] -= ANGLE2SHORT( traceANGsurf );
        }

        //transition from wall to ceiling
        //normal for subsequent viewangle rotations
        if( VectorCompareEpsilon( trace.plane.normal, ceilingNormal, eps ) )
        {
          CrossProduct( surfNormal, trace.plane.normal, pm->ps->grapplePoint );
          VectorNormalize( pm->ps->grapplePoint );
          pm->ps->eFlags |= EF_WALLCLIMBCEILING;
        }

        //transition from ceiling to wall
        //we need to do some different angle correction here cos GPISROTVEC
        if( VectorCompareEpsilon( surfNormal, ceilingNormal, eps ) )
        {
          vectoangles( trace.plane.normal, toAngles );
          vectoangles( pm->ps->grapplePoint, surfAngles );

          pm->ps->delta_angles[ 1 ] -= ANGLE2SHORT( ( ( surfAngles[ 1 ] - toAngles[ 1 ] ) * 2 ) - 180.0f );
        }
      }

      pml.groundTrace = trace;

      //so everything knows where we're wallclimbing (ie client side)
      pm->ps->eFlags |= EF_WALLCLIMB;

      //if we're not stuck to the ceiling then set grapplePoint to be a surface normal
      if( !VectorCompareEpsilon( trace.plane.normal, ceilingNormal, eps ) )
      {
        //so we know what surface we're stuck to
        VectorCopy( trace.plane.normal, pm->ps->grapplePoint );
        pm->ps->eFlags &= ~EF_WALLCLIMBCEILING;
      }

      //IMPORTANT: break out of the for loop if we've hit something
      break;
    }
    else if( trace.allsolid )
    {
      // do something corrective if the trace starts in a solid...
      if( !PM_CorrectAllSolid( &trace ) )
        return;
    }
  }

  if( trace.fraction >= 1.0f || ( trace.surfaceFlags & SURF_SLICK ) )
  {
    // if the trace didn't hit anything, we are in free fall
    PM_GroundTraceMissed( );
    pml.groundPlane = qfalse;
    pml.walking = qfalse;
    pm->ps->eFlags &= ~EF_WALLCLIMB;

    //just transided from ceiling to floor... apply delta correction
    if( pm->ps->eFlags & EF_WALLCLIMBCEILING )
    {
      vec3_t  forward, rotated, angles;

      AngleVectors( pm->ps->viewangles, forward, NULL, NULL );

      RotatePointAroundVector( rotated, pm->ps->grapplePoint, forward, 180.0f );
      vectoangles( rotated, angles );

      pm->ps->delta_angles[ YAW ] -= ANGLE2SHORT( angles[ YAW ] - pm->ps->viewangles[ YAW ] );
    }

    pm->ps->eFlags &= ~EF_WALLCLIMBCEILING;

    //we get very bizarre effects if we don't do this :0
    VectorCopy( refNormal, pm->ps->grapplePoint );
    return;
  }

  pml.groundPlane = qtrue;
  pml.walking = qtrue;

  // hitting solid ground will end a waterjump
  if( pm->ps->pm_flags & PMF_TIME_WATERJUMP )
  {
    pm->ps->pm_flags &= ~PMF_TIME_WATERJUMP;
    pm->ps->pm_time = 0;
  }

  pm->ps->groundEntityNum = trace.entityNum;

  // don't reset the z velocity for slopes
//  pm->ps->velocity[2] = 0;

  VectorCopy( pm->ps->velocity, impactVelocity );
  PM_AddTouchEnt( &trace, impactVelocity );
}

/*
=============
PM_GroundTrace
=============
*/
static void PM_GroundTrace( void )
{
  vec3_t      point;
  vec3_t      refNormal = { 0.0f, 0.0f, 1.0f };
  vec3_t      impactVelocity;
  trace_t     trace;

  if( BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_WALLCLIMBER ) )
  {
    if( pm->ps->persistant[ PERS_STATE ] & PS_WALLCLIMBINGTOGGLE )
    {
      //toggle wall climbing if holding crouch
      if( pm->cmd.upmove < 0 && !( pm->ps->pm_flags & PMF_CROUCH_HELD ) )
      {
        if( !( pm->ps->stats[ STAT_STATE ] & SS_WALLCLIMBING ) )
          pm->ps->stats[ STAT_STATE ] |= SS_WALLCLIMBING;
        else if( pm->ps->stats[ STAT_STATE ] & SS_WALLCLIMBING )
          pm->ps->stats[ STAT_STATE ] &= ~SS_WALLCLIMBING;

        pm->ps->pm_flags |= PMF_CROUCH_HELD;
      }
      else if( pm->cmd.upmove >= 0 )
        pm->ps->pm_flags &= ~PMF_CROUCH_HELD;
    }
    else
    {
      if( pm->cmd.upmove < 0 )
        pm->ps->stats[ STAT_STATE ] |= SS_WALLCLIMBING;
      else if( pm->cmd.upmove >= 0 )
        pm->ps->stats[ STAT_STATE ] &= ~SS_WALLCLIMBING;
    }

    if( pm->ps->pm_type == PM_DEAD )
      pm->ps->stats[ STAT_STATE ] &= ~SS_WALLCLIMBING;

    if( pm->ps->stats[ STAT_STATE ] & SS_WALLCLIMBING )
    {
      PM_GroundClimbTrace( );
      return;
    }

    //just transided from ceiling to floor... apply delta correction
    if( pm->ps->eFlags & EF_WALLCLIMBCEILING )
    {
      vec3_t  forward, rotated, angles;

      AngleVectors( pm->ps->viewangles, forward, NULL, NULL );

      RotatePointAroundVector( rotated, pm->ps->grapplePoint, forward, 180.0f );
      vectoangles( rotated, angles );

      pm->ps->delta_angles[ YAW ] -= ANGLE2SHORT( angles[ YAW ] - pm->ps->viewangles[ YAW ] );
    }
  }

  pm->ps->stats[ STAT_STATE ] &= ~SS_WALLCLIMBING;
  pm->ps->eFlags &= ~( EF_WALLCLIMB | EF_WALLCLIMBCEILING );

  point[ 0 ] = pm->ps->origin[ 0 ];
  point[ 1 ] = pm->ps->origin[ 1 ];
  point[ 2 ] = pm->ps->origin[ 2 ] - 0.25f;

  pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );

  pml.groundTrace = trace;

  // do something corrective if the trace starts in a solid...
  if( trace.allsolid )
    if( !PM_CorrectAllSolid( &trace ) )
      return;

  //make sure that the surfNormal is reset to the ground
  VectorCopy( refNormal, pm->ps->grapplePoint );

  // if the trace didn't hit anything, we are in free fall
  if( trace.fraction == 1.0f )
  {
    qboolean  steppedDown = qfalse;

    // try to step down
    if( pml.groundPlane != qfalse && PM_PredictStepMove( ) )
    {
      //step down
      point[ 0 ] = pm->ps->origin[ 0 ];
      point[ 1 ] = pm->ps->origin[ 1 ];
      point[ 2 ] = pm->ps->origin[ 2 ] - STEPSIZE;
      pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask );

      //if we hit something
      if( trace.fraction < 1.0f )
      {
        PM_StepEvent( pm->ps->origin, trace.endpos, refNormal );
        VectorCopy( trace.endpos, pm->ps->origin );
        steppedDown = qtrue;
        pml.groundTrace = trace;
      }
    }

    if( !steppedDown )
    {
      PM_GroundTraceMissed( );
      pml.groundPlane = qfalse;
      pml.walking = qfalse;

      return;
    }
  }

  // check if getting thrown off the ground
  if( pm->ps->velocity[ 2 ] > 0.0f && DotProduct( pm->ps->velocity, trace.plane.normal ) > 10.0f )
  {
    if( pm->debugLevel )
      Com_Printf( "%i:kickoff\n", c_pmove );

    // go into jump animation
    if( pm->cmd.forwardmove >= 0 )
    {
      if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
        PM_ForceLegsAnim( LEGS_JUMP );
      else
        PM_ForceLegsAnim( NSPA_JUMP );

      pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
    }
    else
    {
      if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
        PM_ForceLegsAnim( LEGS_JUMPB );
      else
        PM_ForceLegsAnim( NSPA_JUMPBACK );

      pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
    }

    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qfalse;
    pml.walking = qfalse;
    return;
  }

  // slopes that are too steep will not be considered onground
  if( trace.plane.normal[ 2 ] < MIN_WALK_NORMAL )
  {
    if( pm->debugLevel )
      Com_Printf( "%i:steep\n", c_pmove );

    // FIXME: if they can't slide down the slope, let them
    // walk (sharp crevices)
    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qtrue;
    pml.walking = qfalse;
    return;
  }

  pml.groundPlane = qtrue;
  pml.walking = qtrue;

  // hitting solid ground will end a waterjump
  if( pm->ps->pm_flags & PMF_TIME_WATERJUMP )
  {
    pm->ps->pm_flags &= ~PMF_TIME_WATERJUMP;
    pm->ps->pm_time = 0;
  }

  if( pm->ps->groundEntityNum == ENTITYNUM_NONE )
  {
    // just hit the ground
    if( pm->debugLevel )
      Com_Printf( "%i:Land\n", c_pmove );

    // communicate the fall velocity to the server
    pm->pmext->fallVelocity = pml.previous_velocity[ 2 ];

    if( BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_TAKESFALLDAMAGE ) )
      PM_CrashLand( );
  }

  if( pm->ps->pm_flags & PMF_JUMPING )
  {
    pm->ps->pm_flags &= ~PMF_JUMPING;
    pm->ps->pm_flags |= PMF_HOPPED;
  }

  pm->ps->pm_flags &= ~PMF_FEATHER_FALL;

  pm->ps->groundEntityNum = trace.entityNum;

  // don't reset the z velocity for slopes
//  pm->ps->velocity[2] = 0;

  VectorCopy( pm->ps->velocity, impactVelocity );
  PM_AddTouchEnt( &trace, impactVelocity );
}


/*
=============
PM_SetWaterLevel  FIXME: avoid this twice?  certainly if not moving
=============
*/
static void PM_SetWaterLevel( void )
{
  vec3_t  point;
  float   mins_z = pm->mins[2];
  int     cont;
  int     sample1;
  int     sample2;

  //
  // get waterlevel, accounting for ducking
  //
  pm->waterlevel = 0;
  pm->watertype = 0;

  point[ 0 ] = pm->ps->origin[ 0 ];
  point[ 1 ] = pm->ps->origin[ 1 ];
  point[ 2 ] = pm->ps->origin[ 2 ] + mins_z + 1;
  cont = pm->pointcontents( point, pm->ps->clientNum );

  if( cont & MASK_WATER )
  {
    sample2 = pm->ps->viewheight - mins_z;
    sample1 = sample2 / 2;

    pm->watertype = cont;
    pm->waterlevel = 1;
    point[ 2 ] = pm->ps->origin[ 2 ] + mins_z + sample1;
    cont = pm->pointcontents( point, pm->ps->clientNum );

    if( cont & MASK_WATER )
    {
      // disable bunny hop
      pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;

      pm->waterlevel = 2;
      point[ 2 ] = pm->ps->origin[ 2 ] + mins_z + sample2;
      cont = pm->pointcontents( point, pm->ps->clientNum );

      if( cont & MASK_WATER )
        pm->waterlevel = 3;
    }
  }
}



/*
==============
PM_SetViewheight
==============
*/
static void PM_SetViewheight( void )
{
  pm->ps->viewheight = ( pm->ps->pm_flags & PMF_DUCKED )
      ? BG_ClassConfig( pm->ps->stats[ STAT_CLASS ] )->crouchViewheight
      : BG_ClassConfig( pm->ps->stats[ STAT_CLASS ] )->viewheight;
}

/*
==============
PM_CheckDuck

Sets mins and maxs, and calls PM_SetViewheight
==============
*/
static void PM_CheckDuck (void)
{
  trace_t trace;
  vec3_t PCmins, PCmaxs, PCcmaxs;

  BG_ClassBoundingBox( pm->ps->stats[ STAT_CLASS ], PCmins, PCmaxs, PCcmaxs, NULL, NULL );

  pm->mins[ 0 ] = PCmins[ 0 ];
  pm->mins[ 1 ] = PCmins[ 1 ];

  pm->maxs[ 0 ] = PCmaxs[ 0 ];
  pm->maxs[ 1 ] = PCmaxs[ 1 ];

  pm->mins[ 2 ] = PCmins[ 2 ];

  if( pm->ps->pm_type == PM_DEAD )
  {
    pm->maxs[ 2 ] = -8;
    pm->ps->viewheight = PCmins[ 2 ] + DEAD_VIEWHEIGHT;
    return;
  }

  // If the standing and crouching bboxes are the same the class can't crouch
  if( ( pm->cmd.upmove < 0 ) && !VectorCompare( PCmaxs, PCcmaxs ) &&
      pm->ps->pm_type != PM_JETPACK )
  {
    // duck
    pm->ps->pm_flags |= PMF_DUCKED;
  }
  else
  {
    // stand up if possible
    if( pm->ps->pm_flags & PMF_DUCKED )
    {
      // try to stand up
      pm->maxs[ 2 ] = PCmaxs[ 2 ];
      pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask );
      if( !trace.allsolid )
        pm->ps->pm_flags &= ~PMF_DUCKED;
    }
  }

  if( pm->ps->pm_flags & PMF_DUCKED )
    pm->maxs[ 2 ] = PCcmaxs[ 2 ];
  else
    pm->maxs[ 2 ] = PCmaxs[ 2 ];

  PM_SetViewheight( );
}



//===================================================================


/*
===============
PM_Footsteps
===============
*/
static void PM_Footsteps( void )
{
  float     bobmove;
  int       old;
  qboolean  footstep;

  //
  // calculate speed and cycle to be used for
  // all cyclic walking effects
  //
  if( BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_WALLCLIMBER ) && ( pml.groundPlane ) )
  {
    // FIXME: yes yes i know this is wrong
    pm->xyspeed = sqrt( pm->ps->velocity[ 0 ] * pm->ps->velocity[ 0 ]
                      + pm->ps->velocity[ 1 ] * pm->ps->velocity[ 1 ]
                      + pm->ps->velocity[ 2 ] * pm->ps->velocity[ 2 ] );
  }
  else
    pm->xyspeed = sqrt( pm->ps->velocity[ 0 ] * pm->ps->velocity[ 0 ]
      + pm->ps->velocity[ 1 ] * pm->ps->velocity[ 1 ] );

  if( pm->ps->groundEntityNum == ENTITYNUM_NONE )
  {
    // airborne leaves position in cycle intact, but doesn't advance
    if( pm->waterlevel > 1 )
    {
      if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
        PM_ContinueLegsAnim( LEGS_SWIM );
      else
        PM_ContinueLegsAnim( NSPA_SWIM );
    }

    return;
  }

  // if not trying to move
  if( !pm->cmd.forwardmove && !pm->cmd.rightmove )
  {
    if( pm->xyspeed < 5 )
    {
      pm->ps->bobCycle = 0; // start at beginning of cycle again
      if( pm->ps->pm_flags & PMF_DUCKED )
      {
        if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
          PM_ContinueLegsAnim( LEGS_IDLECR );
        else
          PM_ContinueLegsAnim( NSPA_STAND );
      }
      else
      {
        if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
          PM_ContinueLegsAnim( LEGS_IDLE );
        else
          PM_ContinueLegsAnim( NSPA_STAND );
      }
    }
    return;
  }


  footstep = qfalse;

  if( pm->ps->pm_flags & PMF_DUCKED )
  {
    bobmove = 0.5;  // ducked characters bob much faster

    if( pm->ps->pm_flags & PMF_BACKWARDS_RUN )
    {
      if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
        PM_ContinueLegsAnim( LEGS_BACKCR );
      else
      {
        if( pm->cmd.rightmove > 0 && !pm->cmd.forwardmove )
          PM_ContinueLegsAnim( NSPA_WALKRIGHT );
        else if( pm->cmd.rightmove < 0 && !pm->cmd.forwardmove )
          PM_ContinueLegsAnim( NSPA_WALKLEFT );
        else
          PM_ContinueLegsAnim( NSPA_WALKBACK );
      }
    }
    else
    {
      if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
        PM_ContinueLegsAnim( LEGS_WALKCR );
      else
      {
        if( pm->cmd.rightmove > 0 && !pm->cmd.forwardmove )
          PM_ContinueLegsAnim( NSPA_WALKRIGHT );
        else if( pm->cmd.rightmove < 0 && !pm->cmd.forwardmove )
          PM_ContinueLegsAnim( NSPA_WALKLEFT );
        else
          PM_ContinueLegsAnim( NSPA_WALK );
      }
    }

    // ducked characters never play footsteps
  }
  else
  {
    if( !( pm->cmd.buttons & BUTTON_WALKING ) )
    {
      bobmove = 0.4f; // faster speeds bob faster

      if( pm->ps->weapon == WP_ALEVEL4 && pm->ps->pm_flags & PMF_CHARGE )
        PM_ContinueLegsAnim( NSPA_CHARGE );
      else if( pm->ps->pm_flags & PMF_BACKWARDS_RUN )
      {
        if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
          PM_ContinueLegsAnim( LEGS_BACK );
        else
        {
          if( pm->cmd.rightmove > 0 && !pm->cmd.forwardmove )
            PM_ContinueLegsAnim( NSPA_RUNRIGHT );
          else if( pm->cmd.rightmove < 0 && !pm->cmd.forwardmove )
            PM_ContinueLegsAnim( NSPA_RUNLEFT );
          else
            PM_ContinueLegsAnim( NSPA_RUNBACK );
        }
      }
      else
      {
        if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
          PM_ContinueLegsAnim( LEGS_RUN );
        else
        {
          if( pm->cmd.rightmove > 0 && !pm->cmd.forwardmove )
            PM_ContinueLegsAnim( NSPA_RUNRIGHT );
          else if( pm->cmd.rightmove < 0 && !pm->cmd.forwardmove )
            PM_ContinueLegsAnim( NSPA_RUNLEFT );
          else
            PM_ContinueLegsAnim( NSPA_RUN );
        }
      }

      footstep = qtrue;
    }
    else
    {
      bobmove = 0.3f; // walking bobs slow
      if( pm->ps->pm_flags & PMF_BACKWARDS_RUN )
      {
        if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
          PM_ContinueLegsAnim( LEGS_BACKWALK );
        else
        {
          if( pm->cmd.rightmove > 0 && !pm->cmd.forwardmove )
            PM_ContinueLegsAnim( NSPA_WALKRIGHT );
          else if( pm->cmd.rightmove < 0 && !pm->cmd.forwardmove )
            PM_ContinueLegsAnim( NSPA_WALKLEFT );
          else
            PM_ContinueLegsAnim( NSPA_WALKBACK );
        }
      }
      else
      {
        if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
          PM_ContinueLegsAnim( LEGS_WALK );
        else
        {
          if( pm->cmd.rightmove > 0 && !pm->cmd.forwardmove )
            PM_ContinueLegsAnim( NSPA_WALKRIGHT );
          else if( pm->cmd.rightmove < 0 && !pm->cmd.forwardmove )
            PM_ContinueLegsAnim( NSPA_WALKLEFT );
          else
            PM_ContinueLegsAnim( NSPA_WALK );
        }
      }
    }
  }

  bobmove *= BG_Class( pm->ps->stats[ STAT_CLASS ] )->bobCycle;

  if( pm->ps->stats[ STAT_STATE ] & SS_SPEEDBOOST )
    bobmove *= HUMAN_SPRINT_MODIFIER;

  bobmove *= MIX( 1.0f, 1.15f, pml.wallSpeedFactor );

  // check for footstep / splash sounds
  old = pm->ps->misc[MISC_BOB_CYCLE];
  pm->ps->misc[MISC_BOB_CYCLE] = (int)( old + ( 16 * bobmove * pml.msec ) ) & 4095;

  // if we just crossed a cycle boundary, play an apropriate footstep event
  if( ( ( old + 1024 ) ^ ( pm->ps->misc[MISC_BOB_CYCLE] + 1024 ) ) & 2048 )
  {
    if( pm->waterlevel == 0 )
    {
      // on ground will only play sounds if running
      if( footstep && !pm->noFootsteps )
        PM_AddEvent( PM_FootstepForSurface( ) );
    }
    else if( pm->waterlevel == 1 )
    {
      // splashing
      PM_AddEvent( EV_FOOTSPLASH );
    }
    else if( pm->waterlevel == 2 )
    {
      // wading / swimming at surface
      PM_AddEvent( EV_SWIM );
    }
    else if( pm->waterlevel == 3 )
    {
      // no sound when completely underwater
    }
  }
}

/*
==============
PM_WaterEvents

Generate sound events for entering and leaving water
==============
*/
static void PM_WaterEvents( void )
{
  // FIXME?
  //
  // if just entered a water volume, play a sound
  //
  if( !pml.previous_waterlevel && pm->waterlevel )
    PM_AddEvent( EV_WATER_TOUCH );

  //
  // if just completely exited a water volume, play a sound
  //
  if( pml.previous_waterlevel && !pm->waterlevel )
    PM_AddEvent( EV_WATER_LEAVE );

  //
  // check for head just going under water
  //
  if( pml.previous_waterlevel != 3 && pm->waterlevel == 3 )
    PM_AddEvent( EV_WATER_UNDER );

  //
  // check for head just coming out of water
  //
  if( pml.previous_waterlevel == 3 && pm->waterlevel != 3 )
    PM_AddEvent( EV_WATER_CLEAR );
}


/*
===============
PM_BeginWeaponChange
===============
*/
static void PM_BeginWeaponChange( int weapon )
{
  int i;

  if( weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS )
    return;

  if( !BG_InventoryContainsWeapon( weapon, pm->ps->stats ) )
    return;

  if( pm->ps->weaponstate == WEAPON_DROPPING )
    return;

  if( pm->ps->weapon == WP_LIGHTNING &&
      ( pm->ps->stats[ STAT_STATE ] & SS_CHARGING ) )
    return;

  for( i = 0; i < 3; i++ )
  {
    if( pm->pmext->burstRoundsToFire[ i ] )
      return;
  }

  // cancel a reload
  pm->ps->pm_flags &= ~PMF_WEAPON_RELOAD;
  if( pm->ps->weaponstate == WEAPON_RELOADING )
    pm->ps->weaponTime = 0;

  //special case to prevent storing a charged up lcannon
  if( pm->ps->weapon == WP_LUCIFER_CANNON )
    pm->ps->misc[ MISC_MISC ] = 0;

  BG_ResetLightningBoltCharge( pm->ps, pm->pmext);

  pm->ps->weaponstate = WEAPON_DROPPING;
  pm->ps->weaponTime += 200;
  pm->ps->persistant[ PERS_NEWWEAPON ] = weapon;

  //reset build weapon
  pm->ps->stats[ STAT_BUILDABLE ] = BA_NONE;

  if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
  {
    PM_StartTorsoAnim( TORSO_DROP );
    PM_StartWeaponAnim( WANIM_DROP );
  }
}


/*
===============
PM_FinishWeaponChange
===============
*/
static void PM_FinishWeaponChange( void )
{
  int   weapon;

  PM_AddEvent( EV_CHANGE_WEAPON );
  weapon = pm->ps->persistant[ PERS_NEWWEAPON ];
  if( weapon < WP_NONE || weapon >= WP_NUM_WEAPONS )
    weapon = WP_NONE;

  if( !BG_InventoryContainsWeapon( weapon, pm->ps->stats ) )
    weapon = WP_NONE;

  pm->ps->misc[MISC_HELD_WEAPON] = weapon;
  pm->ps->weapon = weapon;
  pm->ps->weaponstate = WEAPON_RAISING;
  pm->ps->weaponTime += 250;

  if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
  {
    PM_StartTorsoAnim( TORSO_RAISE );
    PM_StartWeaponAnim( WANIM_RAISE );
  }
}


/*
==============
PM_TorsoAnimation

==============
*/
static void PM_TorsoAnimation( void )
{
  if( pm->ps->weaponstate == WEAPON_READY )
  {
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
    {
      if( pm->ps->weapon == WP_BLASTER )
        PM_ContinueTorsoAnim( TORSO_STAND2 );
      else
        PM_ContinueTorsoAnim( TORSO_STAND );
    }

    PM_ContinueWeaponAnim( WANIM_IDLE );
  }
}


/*
==============
PM_Weapon

Generates weapon events and modifes the weapon counter
==============
*/
static void PM_Weapon( void )
{
  int           addTime = 200; //default addTime - should never be used
  qboolean      attack1 = !pm->swapAttacks ? (pm->cmd.buttons & BUTTON_ATTACK) : (pm->cmd.buttons & BUTTON_ATTACK2);
  qboolean      attack2 = !pm->swapAttacks ? (pm->cmd.buttons & BUTTON_ATTACK2) : (pm->cmd.buttons & BUTTON_ATTACK);
  qboolean      attack3 = pm->cmd.buttons & BUTTON_USE_HOLDABLE;
  qboolean      outOfAmmo = qfalse;
  qboolean      byPassWeaponTime = qfalse;
  qboolean      burstClearedByEmp = qfalse;

  // Ignore weapons in some cases
  if( pm->ps->persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT )
    return;

  // Check for dead player
  if( pm->ps->misc[ MISC_HEALTH ] <= 0 )
  {
    pm->ps->weapon = WP_NONE;
    return;
  }

  //restore charge stamina
  if(BG_ClassHasAbility(pm->ps->stats[STAT_CLASS], SCA_CHARGE_STAMINA))  {
    if( pm->ps->misc[MISC_CHARGE_STAMINA] < 0) {
      pm->ps->misc[MISC_CHARGE_STAMINA] = 0;
    }

    if(
      pm->ps->misc[MISC_CHARGE_STAMINA] <
      (
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaMin /
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaRestoreRate)) {
      pm->ps->misc[MISC_CHARGE_STAMINA] =
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaMin /
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaRestoreRate;
    }

    if(
      pm->ps->misc[MISC_CHARGE_STAMINA] <
      (
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaMax /
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaRestoreRate)) {
      pm->ps->misc[MISC_CHARGE_STAMINA] += pml.msec;
    }

    if(
      pm->ps->misc[MISC_CHARGE_STAMINA] >
      (
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaMax /
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaRestoreRate)) {
      pm->ps->misc[MISC_CHARGE_STAMINA] =
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaMax /
        BG_Class(pm->ps->stats[STAT_CLASS])->chargeStaminaRestoreRate;
    }
  }

  // Charging for a pounce or canceling a pounce
  if( pm->ps->weapon == WP_ALEVEL3 || pm->ps->weapon == WP_ALEVEL3_UPG )
  {
    int max;

    if(BG_ClassHasAbility( pm->ps->stats[STAT_CLASS], SCA_CHARGE_STAMINA)) {
        max =
          pm->ps->misc[MISC_CHARGE_STAMINA] *
          BG_Class(  pm->ps->stats[STAT_CLASS] )->chargeStaminaRestoreRate;
    } else {
      switch( pm->ps->weapon )
      {
        case WP_ALEVEL3:
          max = LEVEL3_POUNCE_TIME;
          break;

        case WP_ALEVEL3_UPG:
          max = LEVEL3_POUNCE_TIME_UPG;
          break;

        default:
          max = LEVEL3_POUNCE_TIME_UPG;
          break;
      }
    }

    if( ( !pm->swapAttacks ?
          (pm->cmd.buttons & BUTTON_ATTACK2) : (pm->cmd.buttons & BUTTON_ATTACK) ) )
      pm->ps->misc[ MISC_MISC ] += pml.msec;
    else
      pm->ps->misc[ MISC_MISC ] -= pml.msec;

    if( pm->ps->misc[ MISC_MISC ] > max )
      pm->ps->misc[ MISC_MISC ] = max;
    else if( pm->ps->misc[ MISC_MISC ] < 0 )
      pm->ps->misc[ MISC_MISC ] = 0;
  }

  // Trample charge mechanics
  if( pm->ps->weapon == WP_ALEVEL4 )
  {
    // Charging up
    if( !( pm->ps->stats[ STAT_STATE ] & SS_CHARGING ) )
    {
      // Charge button held
      if( pm->ps->misc[ MISC_MISC ] < LEVEL4_TRAMPLE_CHARGE_TRIGGER &&
          ( ( !pm->swapAttacks ?
                (pm->cmd.buttons & BUTTON_ATTACK2) : (pm->cmd.buttons & BUTTON_ATTACK) ) ) )
      {
        pm->ps->stats[ STAT_STATE ] &= ~SS_CHARGING;
        if( pm->cmd.forwardmove > 0 )
        {
          int charge = pml.msec;
          vec3_t dir,vel;

          AngleVectors( pm->ps->viewangles, dir, NULL, NULL );
          VectorCopy( pm->ps->velocity, vel );
          vel[2] = 0;
          dir[2] = 0;
          VectorNormalize( vel );
          VectorNormalize( dir );

          charge *= DotProduct( dir, vel );

          pm->ps->misc[ MISC_MISC ] += charge;
        }
        else
          pm->ps->misc[ MISC_MISC ] = 0;
      }

      // Charge button released
      else if( !( pm->ps->stats[ STAT_STATE ] & SS_CHARGING ) )
      {
        if( pm->ps->misc[ MISC_MISC ] > LEVEL4_TRAMPLE_CHARGE_MIN )
        {
          if( pm->ps->misc[ MISC_MISC ] > LEVEL4_TRAMPLE_CHARGE_MAX )
            pm->ps->misc[ MISC_MISC ] = LEVEL4_TRAMPLE_CHARGE_MAX;
          pm->ps->misc[ MISC_MISC ] = pm->ps->misc[ MISC_MISC ] *
                                       LEVEL4_TRAMPLE_DURATION /
                                       LEVEL4_TRAMPLE_CHARGE_MAX;
          pm->ps->stats[ STAT_STATE ] |= SS_CHARGING;
          PM_AddEvent( EV_LEV4_TRAMPLE_START );
        }
        else
          pm->ps->misc[ MISC_MISC ] -= pml.msec;
      }
    }

    // Discharging
    else
    {
      if( pm->ps->misc[ MISC_MISC ] < LEVEL4_TRAMPLE_CHARGE_MIN )
        pm->ps->misc[ MISC_MISC ] = 0;
      else
        pm->ps->misc[ MISC_MISC ] -= pml.msec;

      // If the charger has stopped moving take a chunk of charge away
      if( VectorLength( pm->ps->velocity ) < 64.0f || pm->cmd.rightmove )
        pm->ps->misc[ MISC_MISC ] -= LEVEL4_TRAMPLE_STOP_PENALTY * pml.msec;
    }

    // Charge is over
    if( pm->ps->misc[ MISC_MISC ] <= 0 || pm->cmd.forwardmove <= 0 )
    {
      pm->ps->misc[ MISC_MISC ] = 0;
      pm->ps->stats[ STAT_STATE ] &= ~SS_CHARGING;
    }
  }

  // Charging up a Lucifer Cannon
  pm->ps->eFlags &= ~EF_WARN_CHARGE;

  // don't allow attack until all buttons are up
  if( pm->ps->pm_flags & PMF_RESPAWNED )
    return;

  if( pm->ps->weapon == WP_LUCIFER_CANNON )
  {
    // Charging up
    if( !pm->ps->weaponTime &&
        ( ( !pm->swapAttacks ?
              (pm->cmd.buttons & BUTTON_ATTACK) : (pm->cmd.buttons & BUTTON_ATTACK2) ) ) )
    {
      pm->ps->misc[ MISC_MISC ] += pml.msec;
      if( pm->ps->misc[ MISC_MISC ] >= LCANNON_CHARGE_TIME_MAX )
        pm->ps->misc[ MISC_MISC ] = LCANNON_CHARGE_TIME_MAX;
      if( pm->ps->misc[ MISC_MISC ] > pm->ps->ammo * LCANNON_CHARGE_TIME_MAX /
                                              LCANNON_CHARGE_AMMO )
        pm->ps->misc[ MISC_MISC ] = pm->ps->ammo * LCANNON_CHARGE_TIME_MAX /
                                            LCANNON_CHARGE_AMMO;
    }

    // Set overcharging flag so other players can hear the warning beep
    if( pm->ps->misc[ MISC_MISC ] > LCANNON_CHARGE_TIME_WARN )
      pm->ps->eFlags |= EF_WARN_CHARGE;
  }

  if( pm->ps->weapon == WP_LIGHTNING )
  {
    attack3 = qfalse;

    //hide continous beam
    if( pm->pmext->pulsatingBeamTime[ 0 ] <= 0 )
        pm->ps->pm_flags |= PMF_PAUSE_BEAM;

    if( pm->ps->stats[ STAT_STATE ] & SS_CHARGING )
    {
      //clear any remaining burst rounds for the EMP
      if( !attack2 && pm->pmext->burstRoundsToFire[ 1 ] )
      {
        pm->pmext->burstRoundsToFire[ 1 ] = 0;
        burstClearedByEmp = qtrue;
      }
    } else if( !pm->pmext->burstRoundsToFire[ 1 ] )
    {
      // Charging up
      if( ( ( !pm->swapAttacks ?
            (pm->cmd.buttons & BUTTON_ATTACK) : (pm->cmd.buttons & BUTTON_ATTACK2) ) ) )
      {
        pm->ps->misc[ MISC_MISC ] += pml.msec;
        if( pm->ps->misc[ MISC_MISC ] >= LIGHTNING_BOLT_CHARGE_TIME_MAX )
          pm->ps->misc[ MISC_MISC ] = LIGHTNING_BOLT_CHARGE_TIME_MAX;
        if( pm->ps->misc[ MISC_MISC ] > pm->ps->ammo * LIGHTNING_BOLT_CHARGE_TIME_MIN )
          pm->ps->misc[ MISC_MISC ] = pm->ps->ammo * LIGHTNING_BOLT_CHARGE_TIME_MIN;
      }
    }
  }

  // no bite during pounce
  if( ( pm->ps->weapon == WP_ALEVEL3 || pm->ps->weapon == WP_ALEVEL3_UPG )
      && ( ( ( !pm->swapAttacks ?
            (pm->cmd.buttons & BUTTON_ATTACK) : (pm->cmd.buttons & BUTTON_ATTACK2) ) ) )
      && ( pm->ps->pm_flags & PMF_CHARGE ) )
    return;

  // pump weapon delays (repeat times etc)
  if( pm->ps->weapon == WP_PORTAL_GUN  )
  {
    if( attack2 && pm->humanPortalCreateTime[ PORTAL_BLUE ] > 0 )
      pm->ps->weaponTime = pm->humanPortalCreateTime[ PORTAL_BLUE ];
    else if( attack1 && pm->humanPortalCreateTime[ PORTAL_RED ] > 0 )
      pm->ps->weaponTime = pm->humanPortalCreateTime[ PORTAL_RED ];
    else if( pm->ps->weaponTime > 0 )
      pm->ps->weaponTime -= pml.msec;
  }
  else if( pm->ps->weaponTime > 0 )
    pm->ps->weaponTime -= pml.msec;
  if( pm->ps->weaponTime < 0 )
    pm->ps->weaponTime = 0;
  if( pm->pmext->repairRepeatDelay > 0 )
    pm->pmext->repairRepeatDelay -= pml.msec;
  if( pm->pmext->repairRepeatDelay < 0 )
    pm->pmext->repairRepeatDelay = 0;

  // no slash during charge
  if( pm->ps->stats[ STAT_STATE ] & SS_CHARGING &&
      pm->ps->weapon == WP_ALEVEL4 )
    return;

  // allways allow upgrades to be usable
  if( pm->cmd.weapon >= WP_NUM_WEAPONS &&
      ( pm->cmd.buttons & BUTTON_USE_HOLDABLE ) &&
      !( pm->ps->pm_flags & PMF_USE_ITEM_HELD ) )
  {
    //if trying to toggle an upgrade, toggle it
    if( BG_InventoryContainsUpgrade( pm->cmd.weapon - WP_NUM_WEAPONS, pm->ps->stats ) ) //sanity check
    {
      if( BG_UpgradeIsActive( pm->cmd.weapon - WP_NUM_WEAPONS, pm->ps->stats ) )
        BG_DeactivateUpgrade( pm->cmd.weapon - WP_NUM_WEAPONS, pm->ps->stats );
      else
        BG_ActivateUpgrade( pm->cmd.weapon - WP_NUM_WEAPONS, pm->ps->stats );
    }

    pm->ps->pm_flags |= PMF_USE_ITEM_HELD;
  }
  // check for weapon change
  // can't change if weapon is firing, but can change
  // again if lowering or raising
  else if( BG_PlayerCanChangeWeapon( pm->ps, pm->pmext ) )
  {
    // must press use to switch weapons
    if( pm->cmd.buttons & BUTTON_USE_HOLDABLE )
    {
      if( !( pm->ps->pm_flags & PMF_USE_ITEM_HELD ) )
      {
        if( pm->cmd.weapon < WP_NUM_WEAPONS )
        {
          //if trying to select a weapon, select it
          if( pm->ps->weapon != pm->cmd.weapon )
            PM_BeginWeaponChange( pm->cmd.weapon );
        }

        pm->ps->pm_flags |= PMF_USE_ITEM_HELD;
      }
    }
    else
      pm->ps->pm_flags &= ~PMF_USE_ITEM_HELD;

    //something external thinks a weapon change is necessary
    if( pm->ps->pm_flags & PMF_WEAPON_SWITCH )
    {
      pm->ps->pm_flags &= ~PMF_WEAPON_SWITCH;
      if( pm->ps->weapon != WP_NONE )
      {
        // drop the current weapon
        PM_BeginWeaponChange( pm->ps->persistant[ PERS_NEWWEAPON ] );
      }
      else
      {
        // no current weapon, so just raise the new one
        PM_FinishWeaponChange( );
      }
    }
  }

  // check for out of ammo
  if( ( !pm->ps->ammo ||
        ( attack1 &&
          pm->ps->ammo < BG_Weapon( pm->ps->weapon )->ammoUsage1 ) || 
        ( ( BG_Weapon( pm->ps->weapon )->hasAltMode && attack2 ) &&
          pm->ps->ammo < BG_Weapon( pm->ps->weapon )->ammoUsage2 ) || 
        ( ( BG_Weapon( pm->ps->weapon )->hasThirdMode && attack3 ) &&
          pm->ps->ammo < BG_Weapon( pm->ps->weapon )->ammoUsage3 ) ) &&
      !pm->ps->clips && !BG_Weapon( pm->ps->weapon )->infiniteAmmo )
    outOfAmmo = qtrue;

  if( !outOfAmmo &&
      pm->ps->weaponstate != WEAPON_RELOADING &&
      pm->ps->weaponstate != WEAPON_DROPPING &&
      pm->ps->weaponstate != WEAPON_RAISING )
  {
    //special cases for bypassing the weaponTime
    switch( pm->ps->weapon )
    {
      case WP_LIGHTNING:
        if( ( ( attack1 ||
                pm->ps->misc[ MISC_MISC ] > 0 ) &&
              pm->pmext->burstRoundsToFire[ 1 ] <= 0 ) ||
            ( ( pm->ps->stats[ STAT_STATE ] & SS_CHARGING ) &&
              pm->pmext->burstRoundsToFire[ 1 ] <= 0 &&
              !attack2 ) )
          byPassWeaponTime = qtrue;
        break;

      default:
        byPassWeaponTime = qfalse;
        break;
    }
  }

  if( pm->ps->weaponTime > 0 &&
      !byPassWeaponTime )
    return;

  // change weapon if time
  if( pm->ps->weaponstate == WEAPON_DROPPING )
  {
    PM_FinishWeaponChange( );
    return;
  }

  // Set proper animation
  if( pm->ps->weaponstate == WEAPON_RAISING )
  {
    pm->ps->weaponstate = WEAPON_READY;
    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
    {
      if( pm->ps->weapon == WP_BLASTER )
        PM_ContinueTorsoAnim( TORSO_STAND2 );
      else
        PM_ContinueTorsoAnim( TORSO_STAND );
    }

    PM_ContinueWeaponAnim( WANIM_IDLE );

    return;
  }

  // check for out of ammo
  if( outOfAmmo )
  {
    int i;

    if( !( pm->ps->weapon == WP_LIGHTNING &&
        ( pm->ps->stats[ STAT_STATE ] & SS_CHARGING ) ) )
    {
      if( attack1 ||
          ( BG_Weapon( pm->ps->weapon )->hasAltMode && attack2 ) ||
          ( BG_Weapon( pm->ps->weapon )->hasThirdMode && attack3 ) )
      {
        PM_AddEvent( EV_NOAMMO );
        pm->ps->weaponTime += 500;
      }

      if( pm->ps->weaponstate == WEAPON_FIRING )
        pm->ps->weaponstate = WEAPON_READY;

      return;
    }

    //clear bursts
    for( i = 0; i < 3; i++ )
      pm->pmext->burstRoundsToFire[ i ] = 0;
  }

  //done reloading so give em some ammo
  if( pm->ps->weaponstate == WEAPON_RELOADING )
  {
    pm->ps->clips--;
    pm->ps->ammo = BG_Weapon( pm->ps->weapon )->maxAmmo;

    if( BG_Weapon( pm->ps->weapon )->usesEnergy &&
        BG_InventoryContainsUpgrade( UP_BATTPACK, pm->ps->stats ) )
      pm->ps->ammo *= BATTPACK_MODIFIER;

    //allow some time for the weapon to be raised
    pm->ps->weaponstate = WEAPON_RAISING;
    PM_StartTorsoAnim( TORSO_RAISE );
    pm->ps->weaponTime += 250;
    return;
  }

  // check for end of clip
  if( !BG_Weapon( pm->ps->weapon )->infiniteAmmo &&
      ( pm->ps->ammo <= 0 || ( pm->ps->pm_flags & PMF_WEAPON_RELOAD ) ||
      ( attack1 &&
        pm->ps->ammo < BG_Weapon( pm->ps->weapon )->ammoUsage1 ) || 
      ( ( BG_Weapon( pm->ps->weapon )->hasAltMode && attack2 ) &&
        pm->ps->ammo < BG_Weapon( pm->ps->weapon )->ammoUsage2 ) || 
      ( ( BG_Weapon( pm->ps->weapon )->hasThirdMode && attack2 ) &&
        pm->ps->ammo < BG_Weapon( pm->ps->weapon )->ammoUsage3 ) ) &&
      pm->ps->clips > 0 )
  {
    int i;

    if( pm->ps->weapon != WP_LIGHTNING ||
        !( pm->ps->stats[ STAT_STATE ] & SS_CHARGING ) )
    {
      pm->ps->pm_flags &= ~PMF_WEAPON_RELOAD;
      pm->ps->weaponstate = WEAPON_RELOADING;

      //drop the weapon
      PM_StartTorsoAnim( TORSO_DROP );
      PM_StartWeaponAnim( WANIM_RELOAD );

      pm->ps->weaponTime += BG_Weapon( pm->ps->weapon )->reloadTime;
      return;
    }

    //clear bursts
    for( i = 0; i < 3; i++ )
      pm->pmext->burstRoundsToFire[ i ] = 0;
  }

  if( !pm->pmext->burstRoundsToFire[ 2 ] &&
      !pm->pmext->burstRoundsToFire[ 1 ] &&
      !pm->pmext->burstRoundsToFire[ 0 ] )
  {
    //check if non-auto primary/secondary attacks are permited
    switch( pm->ps->weapon )
    {
      case WP_ALEVEL0:
        //venom is only autohit
        return;

      case WP_ALEVEL3:
      case WP_ALEVEL3_UPG:
        //pouncing has primary secondary AND autohit procedures
        // pounce is autohit
        if( !attack1 && !attack2 && !attack3 )
          return;
        break;

      case WP_LUCIFER_CANNON:
        attack3 = qfalse;

        // Can't fire secondary while primary is charging
        if( attack1 || pm->ps->misc[ MISC_MISC ] > 0 )
          attack2 = qfalse;

        if( ( attack1 || pm->ps->misc[ MISC_MISC ] == 0 ) && !attack2 )
        {
          pm->ps->weaponTime = 0;

          // Charging
          if( pm->ps->misc[ MISC_MISC ] < LCANNON_CHARGE_TIME_MAX )
          {
            pm->ps->weaponstate = WEAPON_READY;
            return;
          }
        }

        if( pm->ps->misc[ MISC_MISC ] > LCANNON_CHARGE_TIME_MIN )
        {
          // Fire primary attack
          attack1 = qtrue;
          attack2 = qfalse;
        }
        else if( pm->ps->misc[ MISC_MISC ] > 0 )
        {
          // Not enough charge
          pm->ps->misc[ MISC_MISC ] = 0;
          pm->ps->weaponTime = 0;
          pm->ps->weaponstate = WEAPON_READY;
          return;
        }
        else if( !attack2 )
        {
          // Idle
          pm->ps->weaponTime = 0;
          pm->ps->weaponstate = WEAPON_READY;
          return;
        }
        break;

      case WP_MASS_DRIVER:
        attack2 = attack3 = qfalse;
        // attack2 is handled on the client for zooming (cg_view.c)

        if( !attack1 )
        {
          pm->ps->weaponTime = 0;
          pm->ps->weaponstate = WEAPON_READY;
          return;
        }
        break;

      case WP_LAUNCHER:
        if (!attack1 && !attack2 )
        {
          pm->ps->weaponTime = 0;
          pm->ps->weaponstate = WEAPON_READY;
          return;
        }
        break;

      case WP_LIGHTNING:
        // Can't fire secondary while primary is charging
        if( attack1 || pm->ps->misc[ MISC_MISC ] > 0 )
          attack2 = qfalse;

        if( ( pm->ps->misc[ MISC_MISC ] == 0 ||
              ( attack1 && 
                !( pm->pmext->impactTriggerTraceChecked &&
                   ( pm->pmext->impactTriggerTrace.fraction < 1.0f ||
                     pm->pmext->impactTriggerTrace.allsolid ||
                     pm->pmext->impactTriggerTrace.startsolid ) ) &&
                pm->ps->misc[ MISC_MISC ] < LIGHTNING_BOLT_CHARGE_TIME_MAX &&
                pm->ps->misc[ MISC_MISC ] < pm->ps->ammo * LIGHTNING_BOLT_CHARGE_TIME_MIN ) ) &&
            !attack2 &&
            !(pm->ps->stats[ STAT_STATE ] & SS_CHARGING) )
        {
          // Charging
          if( pm->ps->misc[ MISC_MISC ] < LIGHTNING_BOLT_CHARGE_TIME_MAX )
          {
            pm->ps->weaponstate = WEAPON_READY;
            return;
          }
        }

        if( pm->ps->stats[ STAT_STATE ] & SS_CHARGING )
        {
          if( attack2 )
          {
            // holding the emp charge
            pm->ps->weaponstate = WEAPON_READY;
            return;
          }

          // Fire the EMP
          attack3 = qtrue;
          attack1 = attack2 = qfalse;
          if( burstClearedByEmp )
            pm->ps->weaponTime += BG_Weapon( pm->ps->weapon )->burstDelay2;
          pm->ps->stats[ STAT_STATE ] &= ~SS_CHARGING;
        }
        else if( pm->ps->misc[ MISC_MISC ] >= LIGHTNING_BOLT_CHARGE_TIME_MIN )
        {
          // Fire primary attack
          attack1 = qtrue;
          attack2 = qfalse;
          pm->pmext->pulsatingBeamTime[ 0 ] = LIGHTNING_BOLT_BEAM_DURATION;
          if( pm->ps->pm_flags & PMF_PAUSE_BEAM )
            pm->ps->pm_flags &= ~PMF_PAUSE_BEAM;
          pm->ps->stats[ STAT_MISC2 ] = pm->ps->misc[ MISC_MISC ];
        }
        else if( pm->ps->misc[ MISC_MISC ] > 0 )
        {
          // Not enough charge
          pm->ps->misc[ MISC_MISC ] = 0;
          pm->ps->weaponstate = WEAPON_READY;
          return;
        }
        else if( !attack2 )
        {
          // Idle
          pm->ps->weaponTime = 0;
          pm->ps->weaponstate = WEAPON_READY;
          return;
        } else if( !pm->pmext->burstRoundsToFire[ 1 ] )
        {
          pm->ps->pm_flags |= PMF_PAUSE_BEAM;

          //charge the EMP to prepare to be fired upon release
          pm->ps->stats[ STAT_STATE ] |= SS_CHARGING;
        }
        break;

      default:
        if( !attack1 && !attack2 && !attack3 )
        {
          pm->ps->weaponTime = 0;
          pm->ps->weaponstate = WEAPON_READY;
          return;
        }
        break;
    }
  }

  if( BG_Weapon( pm->ps->weapon )->hasThirdMode &&
      BG_Weapon( pm->ps->weapon )->burstDelay3 > 0 &&
      BG_Weapon( pm->ps->weapon )->burstRounds3 > 0 &&
      attack3 &&
      !pm->pmext->burstRoundsToFire[ 2 ] )
    pm->pmext->burstRoundsToFire[ 2 ] = BG_Weapon( pm->ps->weapon )->burstRounds3;
  else if( BG_Weapon( pm->ps->weapon )->hasAltMode &&
           BG_Weapon( pm->ps->weapon )->burstDelay2 > 0 &&
           BG_Weapon( pm->ps->weapon )->burstRounds2 > 0 &&
           attack2 &&
           !pm->pmext->burstRoundsToFire[ 1 ] )
    pm->pmext->burstRoundsToFire[ 1 ] = BG_Weapon( pm->ps->weapon )->burstRounds2;
  else if( BG_Weapon( pm->ps->weapon )->burstDelay1 > 0 &&
           BG_Weapon( pm->ps->weapon )->burstRounds1 > 0 &&
           attack1 &&
           !pm->pmext->burstRoundsToFire[ 0 ] )
    pm->pmext->burstRoundsToFire[ 0 ] = BG_Weapon( pm->ps->weapon )->burstRounds1;

  // fire events for burst weapons
  if( pm->pmext->burstRoundsToFire[ 2 ] > 0 )
  {
    pm->ps->generic1 = WPM_TERTIARY;
    PM_AddEvent( EV_FIRE_WEAPON3 );
    pm->pmext->burstRoundsToFire[ 2 ]--;
    if( pm->pmext->burstRoundsToFire[ 2 ] )
      addTime = BG_Weapon( pm->ps->weapon )->repeatRate3;
    else
      addTime = BG_Weapon( pm->ps->weapon )->burstDelay3;
  }
  else if( pm->pmext->burstRoundsToFire[ 1 ] > 0 )
  {
    pm->ps->generic1 = WPM_SECONDARY;
    PM_AddEvent( EV_FIRE_WEAPON2 );
    pm->pmext->burstRoundsToFire[ 1 ]--;
    if( pm->pmext->burstRoundsToFire[ 1 ] )
      addTime = BG_Weapon( pm->ps->weapon )->repeatRate2;
    else
      addTime = BG_Weapon( pm->ps->weapon )->burstDelay2;
  }
  else if( pm->pmext->burstRoundsToFire[ 0 ] > 0 )
  {
    pm->ps->generic1 = WPM_PRIMARY;
    PM_AddEvent( EV_FIRE_WEAPON );
    pm->pmext->burstRoundsToFire[ 0 ]--;
    if( pm->pmext->burstRoundsToFire[ 0 ] )
      addTime = BG_Weapon( pm->ps->weapon )->repeatRate1;
    else
      addTime = BG_Weapon( pm->ps->weapon )->burstDelay1;
  }
  else
  {
    // fire events for non auto weapons
    if( attack3 )
    {
      if( BG_Weapon( pm->ps->weapon )->hasThirdMode )
      {
        //hacky special case for slowblob
        if( pm->ps->weapon == WP_ALEVEL3_UPG && !pm->ps->ammo )
        {
          pm->ps->weaponTime += 200;
          return;
        }

        pm->ps->generic1 = WPM_TERTIARY;
        PM_AddEvent( EV_FIRE_WEAPON3 );
        addTime = BG_Weapon( pm->ps->weapon )->repeatRate3;
      }
      else
      {
        pm->ps->weaponTime = 0;
        pm->ps->weaponstate = WEAPON_READY;
        pm->ps->generic1 = WPM_NOTFIRING;
        return;
      }
    }
    else if( attack2 )
    {
      if( BG_Weapon( pm->ps->weapon )->hasAltMode )
      {
        pm->ps->generic1 = WPM_SECONDARY;
        PM_AddEvent( EV_FIRE_WEAPON2 );
        addTime = BG_Weapon( pm->ps->weapon )->repeatRate2;
      }
      else
      {
        pm->ps->weaponTime = 0;
        pm->ps->weaponstate = WEAPON_READY;
        pm->ps->generic1 = WPM_NOTFIRING;
        return;
      }
    }
    else if( attack1 )
    {
      pm->ps->generic1 = WPM_PRIMARY;
      PM_AddEvent( EV_FIRE_WEAPON );
      addTime = BG_Weapon( pm->ps->weapon )->repeatRate1;
    }

    // fire events for autohit weapons
    if( pm->autoWeaponHit[ pm->ps->weapon ] )
    {
      switch( pm->ps->weapon )
      {
        case WP_ALEVEL0:
          pm->ps->generic1 = WPM_PRIMARY;
          PM_AddEvent( EV_FIRE_WEAPON );
          addTime = BG_Weapon( pm->ps->weapon )->repeatRate1;
          break;

        case WP_ALEVEL3:
        case WP_ALEVEL3_UPG:
          pm->ps->generic1 = WPM_SECONDARY;
          PM_AddEvent( EV_FIRE_WEAPON2 );
          addTime = BG_Weapon( pm->ps->weapon )->repeatRate2;
          break;

        default:
          break;
      }
    }
  }

  if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
  {
    //FIXME: this should be an option in the client weapon.cfg
    switch( pm->ps->weapon )
    {
      case WP_FLAMER:
        if( pm->ps->weaponstate == WEAPON_READY )
        {
          PM_StartTorsoAnim( TORSO_ATTACK );
          PM_StartWeaponAnim( WANIM_ATTACK1 );
        }
        break;

      case WP_BLASTER:
        PM_StartTorsoAnim( TORSO_ATTACK2 );
        PM_StartWeaponAnim( WANIM_ATTACK1 );
        break;

      default:
        PM_StartTorsoAnim( TORSO_ATTACK );
        PM_StartWeaponAnim( WANIM_ATTACK1 );
        break;
    }
  }
  else
  {
    int num = rand( );

    //FIXME: it would be nice to have these hard coded policies in
    //       weapon.cfg
    switch( pm->ps->weapon )
    {
      case WP_ALEVEL1_UPG:
      case WP_ALEVEL1:
        if( attack1 )
        {
          num /= RAND_MAX / 6 + 1;
          PM_ForceLegsAnim( NSPA_ATTACK1 );
          PM_StartWeaponAnim( WANIM_ATTACK1 + num );
        }
        break;

      case WP_ALEVEL2_UPG:
        if( attack2 )
        {
          PM_ForceLegsAnim( NSPA_ATTACK2 );
          PM_StartWeaponAnim( WANIM_ATTACK7 );
        }
      case WP_ALEVEL2:
        if( attack1 )
        {
          num /= RAND_MAX / 6 + 1;
          PM_ForceLegsAnim( NSPA_ATTACK1 );
          PM_StartWeaponAnim( WANIM_ATTACK1 + num );
        }
        break;

      case WP_ALEVEL4:
        num /= RAND_MAX / 3 + 1;
        PM_ForceLegsAnim( NSPA_ATTACK1 + num );
        PM_StartWeaponAnim( WANIM_ATTACK1 + num );
        break;

      default:
        if( attack1 )
        {
          PM_ForceLegsAnim( NSPA_ATTACK1 );
          PM_StartWeaponAnim( WANIM_ATTACK1 );
        }
        else if( attack2 )
        {
          PM_ForceLegsAnim( NSPA_ATTACK2 );
          PM_StartWeaponAnim( WANIM_ATTACK2 );
        }
        else if( attack3 )
        {
          PM_ForceLegsAnim( NSPA_ATTACK3 );
          PM_StartWeaponAnim( WANIM_ATTACK3 );
        }
        break;
    }

    pm->ps->torsoTimer = TIMER_ATTACK;
  }

  pm->ps->weaponstate = WEAPON_FIRING;

  // take an ammo away if not infinite
  if( !BG_Weapon( pm->ps->weapon )->infiniteAmmo ||
      ( pm->ps->weapon == WP_ALEVEL3_UPG && attack3 ) )
  {
    // Special case for lcannon
    if( pm->ps->weapon == WP_LUCIFER_CANNON && attack1 && !attack2 )
      pm->ps->ammo -= ( pm->ps->misc[ MISC_MISC ] * LCANNON_CHARGE_AMMO +
                LCANNON_CHARGE_TIME_MAX - 1 ) / LCANNON_CHARGE_TIME_MAX;
    else if( pm->ps->weapon == WP_LIGHTNING && attack1 && !attack2 )
      pm->ps->ammo -= pm->ps->misc[ MISC_MISC ] / LIGHTNING_BOLT_CHARGE_TIME_MIN;
    else
      pm->ps->ammo -= BG_AmmoUsage( pm->ps );

    // Stay on the safe side
    if( pm->ps->ammo < 0 )
      pm->ps->ammo = 0;
  }

  //FIXME: predicted angles miss a problem??
  if( pm->ps->weapon == WP_CHAINGUN )
  {
    if( pm->ps->pm_flags & PMF_DUCKED ||
        BG_InventoryContainsUpgrade( UP_BATTLESUIT, pm->ps->stats ) )
    {
      pm->ps->delta_angles[ PITCH ] -= ANGLE2SHORT( ( ( random() * 0.5 ) - 0.125 ) * ( 30 / (float)addTime ) );
      pm->ps->delta_angles[ YAW ] -= ANGLE2SHORT( ( ( random() * 0.5 ) - 0.25 ) * ( 30.0 / (float)addTime ) );
    }
    else
    {
      pm->ps->delta_angles[ PITCH ] -= ANGLE2SHORT( ( ( random() * 8 ) - 2 ) * ( 30.0 / (float)addTime ) );
      pm->ps->delta_angles[ YAW ] -= ANGLE2SHORT( ( ( random() * 8 ) - 4 ) * ( 30.0 / (float)addTime ) );
    }
  }

  pm->ps->weaponTime += addTime;
}

/*
================
PM_Animate
================
*/
static void PM_Animate( void )
{
  if( PM_Paralyzed( pm->ps->pm_type ) )
    return;

  if( pm->cmd.buttons & BUTTON_GESTURE )
  {
    if( !pm->tauntSpam && pm->ps->tauntTimer > 0 )
        return;

    if( !( pm->ps->persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
    {
      if( pm->ps->torsoTimer == 0 )
      {
        PM_StartTorsoAnim( TORSO_GESTURE );
        pm->ps->torsoTimer = TIMER_GESTURE;
        if( !pm->tauntSpam )
          pm->ps->tauntTimer = TIMER_GESTURE;

        PM_AddEvent( EV_TAUNT );
      }
    }
    else
    {
      if( pm->ps->torsoTimer == 0 )
      {
        PM_ForceLegsAnim( NSPA_GESTURE );
        pm->ps->torsoTimer = TIMER_GESTURE;
        if( !pm->tauntSpam )
          pm->ps->tauntTimer = TIMER_GESTURE;

        PM_AddEvent( EV_TAUNT );
      }
    }
  }
}


/*
================
PM_DropTimers
================
*/
static void PM_DropTimers( void )
{
  int i;

  // drop misc timing counter
  if( pm->ps->pm_time )
  {
    if( pml.msec >= pm->ps->pm_time )
    {
      pm->ps->pm_flags &= ~PMF_ALL_TIMES;
      pm->ps->pm_time = 0;
    }
    else
      pm->ps->pm_time -= pml.msec;
  }

  // drop animation counter
  if( pm->ps->legsTimer > 0 )
  {
    pm->ps->legsTimer -= pml.msec;

    if( pm->ps->legsTimer < 0 )
      pm->ps->legsTimer = 0;
  }

  if( pm->ps->torsoTimer > 0 )
  {
    pm->ps->torsoTimer -= pml.msec;

    if( pm->ps->torsoTimer < 0 )
      pm->ps->torsoTimer = 0;
  }

  if( pm->ps->tauntTimer > 0 )
  {
    pm->ps->tauntTimer -= pml.msec;

    if( pm->ps->tauntTimer < 0 )
    {
      pm->ps->tauntTimer = 0;
    }
  }

  // the jump timer increases
  if( pm->ps->persistant[PERS_JUMPTIME] < 0 )
    pm->ps->persistant[PERS_JUMPTIME] = 0;
  else
    pm->ps->persistant[PERS_JUMPTIME] += pml.msec;

  // pulsating beam timers
  for( i = 0; i < 3; i++ )
  {
    if( pm->pmext->pulsatingBeamTime[ i ] )
    {
      if( pm->pmext->pulsatingBeamTime[ i ] > 0)
        pm->pmext->pulsatingBeamTime[ i ] -= pml.msec;

      if( pm->pmext->pulsatingBeamTime[ i ] < 0 )
        pm->pmext->pulsatingBeamTime[ i ] = 0;
    }
  }
}


/*
================
PM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated instead of a full move
================
*/
void PM_UpdateViewAngles( playerState_t *ps, const usercmd_t *cmd )
{
  short   temp[ 3 ];
  int     i;
  vec3_t  axis[ 3 ], rotaxis[ 3 ];
  vec3_t  tempang;

  if( ps->pm_type == PM_INTERMISSION )
    return;   // no view changes at all

  if( ps->pm_type != PM_SPECTATOR && ps->misc[ MISC_HEALTH ] <= 0 )
    return;   // no view changes at all

  // circularly clamp the angles with deltas
  for( i = 0; i < 3; i++ )
  {
    if( i == ROLL )
    {
      // Guard against speed hack
      temp[ i ] = ps->delta_angles[ i ];

#ifdef CGAME
      // Assert here so that if cmd->angles[ i ] becomes non-zero
      // for a legitimate reason we can tell where and why it's
      // being ignored
      assert( cmd->angles[ i ] == 0 );
#endif

    }
    else
      temp[ i ] = cmd->angles[ i ] + ps->delta_angles[ i ];

    if( i == PITCH )
    {
      // don't let the player look up or down more than 90 degrees
      if( temp[ i ] > 16000 )
      {
        ps->delta_angles[ i ] = 16000 - cmd->angles[ i ];
        temp[ i ] = 16000;
      }
      else if( temp[ i ] < -16000 )
      {
        ps->delta_angles[ i ] = -16000 - cmd->angles[ i ];
        temp[ i ] = -16000;
      }
    }
    tempang[ i ] = SHORT2ANGLE( temp[ i ] );
  }

  //convert viewangles -> axis
  AnglesToAxis( tempang, axis );

  if( !( ps->stats[ STAT_STATE ] & SS_WALLCLIMBING ) ||
      !BG_RotateAxis( ps->grapplePoint, axis, rotaxis, qfalse,
                      ps->eFlags & EF_WALLCLIMBCEILING ) )
    AxisCopy( axis, rotaxis );

  //convert the new axis back to angles
  AxisToAngles( rotaxis, tempang );

  //force angles to -180 <= x <= 180
  for( i = 0; i < 3; i++ )
  {
    while( tempang[ i ] > 180.0f )
      tempang[ i ] -= 360.0f;

    while( tempang[ i ] < -180.0f )
      tempang[ i ] += 360.0f;
  }

  PM_CalculateAngularVelocity( ps, cmd );

  //actually set the viewangles
  for( i = 0; i < 3; i++ )
    ps->viewangles[ i ] = tempang[ i ];

  //pull the view into the lock point
  if( ps->pm_type == PM_GRABBED && !BG_InventoryContainsUpgrade( UP_BATTLESUIT, ps->stats ) )
  {
    vec3_t  dir, angles;

    ByteToDir( ps->stats[ STAT_VIEWLOCK ], dir );
    vectoangles( dir, angles );

    for( i = 0; i < 3; i++ )
    {
      float diff = AngleSubtract( ps->viewangles[ i ], angles[ i ] );

      while( diff > 180.0f )
        diff -= 360.0f;
      while( diff < -180.0f )
        diff += 360.0f;

      if( diff < -90.0f )
        ps->delta_angles[ i ] += ANGLE2SHORT( fabs( diff ) - 90.0f );
      else if( diff > 90.0f )
        ps->delta_angles[ i ] -= ANGLE2SHORT( fabs( diff ) - 90.0f );

      if( diff < 0.0f )
        ps->delta_angles[ i ] += ANGLE2SHORT( fabs( diff ) * 0.05f );
      else if( diff > 0.0f )
        ps->delta_angles[ i ] -= ANGLE2SHORT( fabs( diff ) * 0.05f );
    }
  }
}

void PM_CalculateAngularVelocity( playerState_t *ps, const usercmd_t *cmd )
{
  short   temp[ 3 ];
  int     i;
  vec3_t  axis[ 3 ], rotaxis[ 3 ];
  vec3_t  tempang, cartNew, cartOld;

  if( ps->pm_type == PM_INTERMISSION )
    return;   // no view changes at all

  if( ps->pm_type != PM_SPECTATOR && ps->misc[ MISC_HEALTH ] <= 0 )
    return;   // no view changes at all

  // circularly clamp the angles with deltas
  for( i = 0; i < 3; i++ )
  {
    if( i == ROLL )
    {
      // Guard against speed hack
      temp[ i ] = ps->delta_angles[ i ];

#ifdef CGAME
      // Assert here so that if cmd->angles[ i ] becomes non-zero
      // for a legitimate reason we can tell where and why it's
      // being ignored
      assert( cmd->angles[ i ] == 0 );
#endif

    }
    else
      temp[ i ] = cmd->angles[ i ] + ps->delta_angles[ i ];

    if( i == PITCH )
    {
      // don't let the player look up or down more than 90 degrees
      if( temp[ i ] > 16000 )
      {
        temp[ i ] = 16000;
      }
      else if( temp[ i ] < -16000 )
      {
        temp[ i ] = -16000;
      }
    }
    tempang[ i ] = SHORT2ANGLE( temp[ i ] );
  }

  //convert viewangles -> axis
  AnglesToAxis( tempang, axis );

  if( !( ps->stats[ STAT_STATE ] & SS_WALLCLIMBING ) ||
      !BG_RotateAxis( ps->grapplePoint, axis, rotaxis, qfalse,
                      ps->eFlags & EF_WALLCLIMBCEILING ) )
    AxisCopy( axis, rotaxis );

  //convert the new axis back to angles
  AxisToAngles( rotaxis, tempang );

  //force angles to -180 <= x <= 180
  for( i = 0; i < 3; i++ )
  {
    while( tempang[ i ] > 180.0f )
      tempang[ i ] -= 360.0f;

    while( tempang[ i ] < -180.0f )
      tempang[ i ] += 360.0f;
  }

  // actually calculate the angular velocity

  if( pm->pmext->updateAnglesTime != pm->ps->commandTime )
  {
    VectorCopy( pm->pmext->previousUpdateAngles, pm->pmext->previousFrameAngles );
    pm->pmext->diffAnglesPeriod = (float)( pm->ps->commandTime - pm->pmext->updateAnglesTime );
  }

  if( !( VectorCompare( tempang, pm->pmext->previousUpdateAngles ) &&
         pm->pmext->updateAnglesTime == pm->ps->commandTime ) )
  {
    VectorCopy( tempang, pm->pmext->previousUpdateAngles );

    cartOld[0] = (float)sin( (double)( pm->pmext->previousFrameAngles[ PITCH  ] + 90 ) ) *
                 (float)cos( (double)( pm->pmext->previousFrameAngles[ YAW ] ) );

    cartOld[1] = (float)sin( (double)( pm->pmext->previousFrameAngles[ PITCH  ] + 90 ) ) *
                 (float)sin( (double)( pm->pmext->previousFrameAngles[ YAW ] ) );

    cartOld[2] = (float)cos( (double)( pm->pmext->previousFrameAngles[ PITCH  ] + 90 ) );

    cartNew[0] = (float)sin( (double)( tempang[ PITCH ] + 90 ) ) *
                 (float)cos( (double)( tempang[ YAW ] ) );

    cartNew[1] = (float)sin( (double)( tempang[ PITCH ] + 90 ) ) *
                 (float)sin( (double)( tempang[ YAW ] ) );

    cartNew[2] = (float)cos( (double)( tempang[ PITCH ] + 90 ) );

    VectorSubtract( cartNew,  cartOld, pm->pmext->angularVelocity );

    VectorScale( pm->pmext->angularVelocity, 1.000f/pm->pmext->diffAnglesPeriod,
                 pm->pmext->angularVelocity );
  }

  pm->pmext->updateAnglesTime = pm->ps->commandTime;
}

/*
================
PmoveSingle

================
*/

void PmoveSingle( pmove_t *pmove )
{
  pm = pmove;

  // this counter lets us debug movement problems with a journal
  // by setting a conditional breakpoint for the previous frame
  c_pmove++;

  // clear results
  pm->watertype = 0;
  pm->waterlevel = 0;

  if( pm->ps->misc[ MISC_HEALTH ] <= 0 )
    pm->tracemask &= ~CONTENTS_BODY;  // corpses can fly through bodies

  // make sure walking button is clear if they are running, to avoid
  // proxy no-footsteps cheats
  if( abs( pm->cmd.forwardmove ) > 64 || abs( pm->cmd.rightmove ) > 64 )
    pm->cmd.buttons &= ~BUTTON_WALKING;

  // set the firing flag for continuous beam weapons
  if( !(pm->ps->pm_flags & PMF_RESPAWNED) && pm->ps->pm_type != PM_INTERMISSION &&
      ( ( ( ( !pm->swapAttacks ?
            (pm->cmd.buttons & BUTTON_ATTACK) : (pm->cmd.buttons & BUTTON_ATTACK2) ) ) ) ||
        pm->pmext->pulsatingBeamTime[ 0 ] ) &&
      ( ( pm->ps->ammo > 0 || pm->ps->clips > 0 ) || BG_Weapon( pm->ps->weapon )->infiniteAmmo ) &&
      !( pm->ps->pm_flags & PMF_PAUSE_BEAM ) )
    pm->ps->eFlags |= EF_FIRING;
  else
    pm->ps->eFlags &= ~EF_FIRING;

  // set the firing flag for continuous beam weapons
  if( !(pm->ps->pm_flags & PMF_RESPAWNED) && pm->ps->pm_type != PM_INTERMISSION &&
      ( ( ( ( !pm->swapAttacks ?
            (pm->cmd.buttons & BUTTON_ATTACK2) : (pm->cmd.buttons & BUTTON_ATTACK) ) ) ) ||
        pm->pmext->pulsatingBeamTime[ 1 ] ) &&
      ( ( pm->ps->ammo > 0 || pm->ps->clips > 0 ) || BG_Weapon( pm->ps->weapon )->infiniteAmmo ) &&
      !( pm->ps->pm_flags & PMF_PAUSE_BEAM ) )
    pm->ps->eFlags |= EF_FIRING2;
  else
    pm->ps->eFlags &= ~EF_FIRING2;

  // set the firing flag for continuous beam weapons
  if( !(pm->ps->pm_flags & PMF_RESPAWNED) && pm->ps->pm_type != PM_INTERMISSION &&
      ( ( pm->cmd.buttons & BUTTON_USE_HOLDABLE ) ||
        pm->pmext->pulsatingBeamTime[ 2 ] ) &&
      ( ( pm->ps->ammo > 0 || pm->ps->clips > 0 ) || BG_Weapon( pm->ps->weapon )->infiniteAmmo ) &&
      !( pm->ps->pm_flags & PMF_PAUSE_BEAM ) )
    pm->ps->eFlags |= EF_FIRING3;
  else
    pm->ps->eFlags &= ~EF_FIRING3;


  // clear the respawned flag if attack and use are cleared
  if( pm->ps->misc[ MISC_HEALTH ] > 0 &&
      !( ( !pm->swapAttacks ?
            (pm->cmd.buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE )) :
            (pm->cmd.buttons & ( BUTTON_ATTACK2 | BUTTON_USE_HOLDABLE )) ) ) )
    pm->ps->pm_flags &= ~PMF_RESPAWNED;

  // if talk button is down, dissallow all other input
  // this is to prevent any possible intercept proxy from
  // adding fake talk balloons
  if( pmove->cmd.buttons & BUTTON_TALK )
  {
    pmove->cmd.buttons = BUTTON_TALK;
    pmove->cmd.forwardmove = 0;
    pmove->cmd.rightmove = 0;

    if( pmove->cmd.upmove > 0 )
      pmove->cmd.upmove = 0;
  }

  // clear all pmove local vars
  memset( &pml, 0, sizeof( pml ) );

  // determine the time
  pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;

  if( pml.msec < 1 )
    pml.msec = 1;
  else if( pml.msec > 200 )
    pml.msec = 200;

  pm->ps->commandTime = pmove->cmd.serverTime;

  // save old org in case we get stuck
  VectorCopy( pm->ps->origin, pml.previous_origin );

  // save old velocity for crashlanding
  VectorCopy( pm->ps->velocity, pml.previous_velocity );

  pml.frametime = pml.msec * 0.001;

  AngleVectors( pm->ps->viewangles, pml.forward, pml.right, pml.up );

  if( pm->cmd.upmove < 10 )
  {
    // not holding jump

    if( pm->ps->pm_flags & PMF_HOPPED )
    {
      // disable bunny hop
      pm->ps->pm_flags &= ~PMF_ALL_HOP_FLAGS;
    }

    pm->ps->pm_flags &= ~PMF_JUMP_HELD;

    // deactivate the jet
    if( BG_InventoryContainsUpgrade( UP_JETPACK, pm->ps->stats ) &&
        BG_UpgradeIsActive( UP_JETPACK, pm->ps->stats ) &&
        !( pm->cmd.buttons & BUTTON_WALKING ) )
    {
      BG_DeactivateUpgrade( UP_JETPACK, pm->ps->stats );
      if(pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        pm->ps->persistant[PERS_JUMPTIME] = 0;
        pm->ps->pm_flags |= PMF_FEATHER_FALL;
      }
      pm->ps->pm_type = PM_NORMAL;
    }
  }

  // decide if backpedaling animations should be used
  if( pm->cmd.forwardmove < 0 )
    pm->ps->pm_flags |= PMF_BACKWARDS_RUN;
  else if( pm->cmd.forwardmove > 0 || ( pm->cmd.forwardmove == 0 && pm->cmd.rightmove ) )
    pm->ps->pm_flags &= ~PMF_BACKWARDS_RUN;

  if( PM_Paralyzed( pm->ps->pm_type ) )
  {
    pm->cmd.forwardmove = 0;
    pm->cmd.rightmove = 0;
    pm->cmd.upmove = 0;
  }

  if( pm->ps->pm_type == PM_SPECTATOR )
  {
    // update the viewangles
    PM_UpdateViewAngles( pm->ps, &pm->cmd );
    PM_CheckDuck( );
    PM_FlyMove( );
    PM_DropTimers( );
    return;
  }

  if( pm->ps->pm_type == PM_NOCLIP )
  {
    PM_UpdateViewAngles( pm->ps, &pm->cmd );
    PM_NoclipMove( );
    PM_SetViewheight( );
    PM_Weapon( );
    PM_DropTimers( );
    return;
  }

  if( pm->ps->pm_type == PM_FREEZE)
    return;   // no movement at all

  if( pm->ps->pm_type == PM_INTERMISSION )
    return;   // no movement at all

  // set mins, maxs, and viewheight
  PM_CheckDuck( );

  // set watertype, and waterlevel
  PM_SetWaterLevel( );
  pml.previous_waterlevel = pmove->waterlevel;

  PM_CheckLadder( );

  // jet activation
  if( (pm->cmd.upmove >= 10 || ( pm->cmd.buttons & BUTTON_WALKING )) &&
      BG_InventoryContainsUpgrade( UP_JETPACK, pm->ps->stats ) &&
      !BG_UpgradeIsActive( UP_JETPACK, pm->ps->stats ) &&
      ( pm->ps->groundEntityNum == ENTITYNUM_NONE ||
        pm->ps->stats[ STAT_STATE ] & SS_GRABBED ) &&
      !(pm->ps->pm_flags & PMF_JUMP_HELD) &&
      pm->ps->stats[ STAT_FUEL ] > JETPACK_FUEL_MIN_START &&
      ( pm->ps->pm_type == PM_NORMAL || 
        ( pm->ps->pm_type == PM_GRABBED && 
          pm->ps->stats[ STAT_STATE ] & SS_GRABBED ) ) &&
      !pml.ladder )
  {
    BG_ActivateUpgrade( UP_JETPACK, pm->ps->stats );
    pm->ps->pm_type = PM_JETPACK;

    if( pm->ps->stats[ STAT_FUEL ] >= JETPACK_ACT_BOOST_FUEL_USE )
    {
      // give an upwards boost when activating the jet
      pm->ps->persistant[PERS_JUMPTIME] = 0;
      PM_AddEvent( EV_JETJUMP );
    }
  }

  // set groundentity
  PM_GroundTrace( );

  // update the viewangles
  PM_UpdateViewAngles( pm->ps, &pm->cmd );

  if( pm->ps->pm_type == PM_DEAD || pm->ps->pm_type == PM_GRABBED )
    PM_DeadMove( );

  //PM_ComputeWallSpeedFactor();

  PM_DropTimers( );
  PM_CheckDodge( );

  if( pm->ps->pm_type == PM_JETPACK )
    PM_JetPackMove( );
  else if( pm->ps->pm_flags & PMF_TIME_WATERJUMP )
    PM_WaterJumpMove( );
  else if( pml.ladder )
    PM_LadderMove( );
  else if( pm->waterlevel > 1 )
    PM_WaterMove( );
  else if( pml.walking )
  {
    if( BG_ClassHasAbility( pm->ps->stats[ STAT_CLASS ], SCA_WALLCLIMBER ) &&
        ( pm->ps->stats[ STAT_STATE ] & SS_WALLCLIMBING ) )
      PM_ClimbMove( ); // walking on any surface
    else
      PM_WalkMove( ); // walking on ground
  }
  else
    PM_AirMove( );

  PM_Animate( );

  // set groundentity, watertype, and waterlevel
  PM_GroundTrace( );

  // update the viewangles
  PM_UpdateViewAngles( pm->ps, &pm->cmd );

  PM_SetWaterLevel( );

  // weapons
  PM_Weapon( );

  // torso animation
  PM_TorsoAnimation( );

  // footstep events / legs animations
  PM_Footsteps( );

  // entering / leaving water splashes
  PM_WaterEvents( );

  // snap some parts of playerstate to save network bandwidth
  #ifdef Q3_VM
    trap_SnapVector( pm->ps->velocity );
  #else
    Q_SnapVector( pm->ps->velocity );
  #endif
}


/*
================
Pmove

Can be called by either the server or the client
================
*/
void Pmove( pmove_t *pmove )
{
  int finalTime;

  finalTime = pmove->cmd.serverTime;

  if( finalTime < pmove->ps->commandTime )
    return; // should not happen

  if( finalTime > pmove->ps->commandTime + 1000 )
    pmove->ps->commandTime = finalTime - 1000;

  pmove->ps->pmove_framecount = ( pmove->ps->pmove_framecount + 1 ) & ( ( 1 << PS_PMOVEFRAMECOUNTBITS ) - 1 );

  // chop the move up if it is too long, to prevent framerate
  // dependent behavior
  while( pmove->ps->commandTime != finalTime )
  {
    int   msec;

    msec = finalTime - pmove->ps->commandTime;

    if( pmove->pmove_fixed )
    {
      if( msec > pmove->pmove_msec )
        msec = pmove->pmove_msec;
    }
    else
    {
      if( msec > 66 )
        msec = 66;
    }

    pmove->cmd.serverTime = pmove->ps->commandTime + msec;
    PmoveSingle( pmove );
  }
}
