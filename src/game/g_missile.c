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

#include "g_local.h"

#define MISSILE_PRESTEP_TIME  50

/*
================
G_BounceMissile

================
*/
void G_BounceMissile( gentity_t *ent, trace_t *trace )
{
  vec3_t  velocity;
  float dot;
  int   hitTime;

  // reflect the velocity on the trace plane
  hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
  BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
  dot = DotProduct( velocity, trace->plane.normal );
  VectorMA( velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta );

  if( ent->flags & FL_BOUNCE_HALF )
  {
    VectorScale( ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta );
    // check for stop
    if( trace->plane.normal[ 2 ] > 0.2 && VectorLength( ent->s.pos.trDelta ) < 40 )
    {
      G_SetOrigin( ent, trace->endpos );
      return;
    }
  }

  VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin );
  VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
  ent->s.pos.trTime = level.time;
}


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile( gentity_t *ent )
{
  vec3_t    dir;
  vec3_t    origin;

  BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
  SnapVector( origin );
  G_SetOrigin( ent, origin );

  // we don't have a valid direction, so just point straight up
  dir[ 0 ] = dir[ 1 ] = 0;
  dir[ 2 ] = 1;

  ent->s.eType = ET_GENERAL;

  ent->s.eFlags &= ~EF_WARN_CHARGE;

  G_SplatterFire( ent, ent->parent, ent->s.pos.trBase, dir, ent->s.weapon,
                  ent->s.generic1, ent->splashMethodOfDeath );

  if( ent->s.weapon != WP_LOCKBLOB_LAUNCHER &&
      ent->s.weapon != WP_FLAMER )
    G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );

  ent->freeAfterEvent = qtrue;

  // splash damage
  if( ent->splashDamage )
    G_RadiusDamage( ent->r.currentOrigin, ent->r.mins,
                    ent->r.maxs, ent->parent, ent->splashDamage,
                    ent->splashRadius, ent, ent->splashMethodOfDeath, qtrue );

  SV_LinkEntity( ent );
}

/*
================
G_KickUpMissile

================
*/
void G_KickUpMissile( gentity_t *ent )
{
  vec3_t velocity, origin;

  BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
  BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
  velocity[2] = 350.0f;
  VectorCopy( velocity, ent->s.pos.trDelta);
  ent->s.pos.trType = TR_GRAVITY;
  VectorCopy( origin, ent->s.pos.trBase );
  ent->s.pos.trTime = level.time;
  ent->think = G_ExplodeMissile;
  ent->nextthink = level.time + 650;
}

void AHive_ReturnToHive( gentity_t *self );

/*
================
G_MissileImpact

================
*/
void G_MissileImpact( gentity_t *ent, trace_t *trace )
{
  gentity_t   *other, *attacker;
  qboolean    returnAfterDamage = qfalse;
  vec3_t      dir;

  other = &g_entities[ trace->entityNum ];
  attacker = &g_entities[ ent->r.ownerNum ];

  if(!strcmp(ent->classname,"lasermine")) {
    VectorCopy(trace->plane.normal, ent->s.origin2);
    vectoangles(trace->plane.normal, ent->r.currentAngles);
    VectorCopy(ent->r.currentAngles, ent->s.apos.trBase);
    G_SetOrigin( ent, trace->endpos );
    G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );
    return;
  }

  // check for bounce
  if( !G_TakesDamage( other ) &&
      ( ent->flags & ( FL_BOUNCE | FL_BOUNCE_HALF ) ) )
  {
    G_BounceMissile( ent, trace );

    //only play a sound if requested
    if( !( ent->s.eFlags & FL_NO_BOUNCE_SOUND ) )
      G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );

    return;
  }

  if( !strcmp( ent->classname, "grenade" ) )
  {
    //grenade doesn't explode on impact
    G_BounceMissile( ent, trace );

    //only play a sound if requested
    if( !( ent->s.eFlags & FL_NO_BOUNCE_SOUND ) )
      G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );

    return;
  }
  else if( !strcmp( ent->classname, "lockblob" ) )
  {
    if( other->client && other->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
    {
      other->client->ps.stats[ STAT_STATE ] |= SS_BLOBLOCKED;
      other->client->lastLockTime = level.time;
      AngleVectors( other->client->ps.viewangles, dir, NULL, NULL );
      other->client->ps.stats[ STAT_VIEWLOCK ] = DirToByte( dir );
    }
  }
  else if( !strcmp( ent->classname, "slowblob" ) )
  {
    if( other->client && other->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
    {
      other->client->ps.stats[ STAT_STATE ] |= SS_SLOWLOCKED;
      other->client->lastSlowTime = level.time;
      AngleVectors( other->client->ps.viewangles, dir, NULL, NULL );
      other->client->ps.stats[ STAT_VIEWLOCK ] = DirToByte( dir );
    }
  }
  else if( !strcmp( ent->classname, "hive" ) )
  {
    if( other->s.eType == ET_BUILDABLE && other->s.modelindex == BA_A_HIVE )
    {
      if( !ent->parent )
        Com_Printf( S_COLOR_YELLOW "WARNING: hive entity has no parent in G_MissileImpact\n" );
      else
        ent->parent->active = qfalse;

      G_FreeEntity( ent );
      return;
    }
    else
    {
      //prevent collision with the client when returning
      ent->r.ownerNum = other->s.number;

      ent->think = G_ExplodeMissile;
      ent->nextthink = level.time + FRAMETIME;

      //only damage humans
      if( other->client && other->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
        returnAfterDamage = qtrue;
      else
        return;
    }
  }
  else if ( !strcmp( ent->classname, "portalgun" ) && !other->client &&
            other->s.eType != ET_BUILDABLE && other->s.eType != ET_MOVER &&
            !( other->r.contents & CONTENTS_DOOR ) )
  {
    G_Portal_Create( ent->parent, trace->endpos, trace->plane.normal, ent->s.modelindex2 );
  }

  // impact damage
  if( G_TakesDamage( other ) )
  {
    // FIXME: wrong damage direction?
    if( ent->damage )
    {
      vec3_t  velocity;
      int     dflags;

      BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
      if( VectorLength( velocity ) == 0 )
        velocity[ 2 ] = 1;  // stepped on a grenade

      dflags = DAMAGE_NO_LOCDAMAGE;

      if(ent->noKnockback) {
        dflags |= DAMAGE_NO_KNOCKBACK;
      }

      G_Damage( other, ent, attacker, velocity, ent->r.currentOrigin, ent->damage,
        dflags, ent->methodOfDeath );
    }
  }

  if( returnAfterDamage )
    return;

  // is it cheaper in bandwidth to just remove this ent and create a new
  // one, rather than changing the missile into the explosion?

  if( ent->s.weapon == WP_LUCIFER_CANNON )
    G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
  else if( ent->s.weapon == WP_LAUNCHER )
    G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
  else if( G_TakesDamage( other ) &&
      ( other->s.eType == ET_PLAYER || other->s.eType == ET_BUILDABLE ) )
  {
    G_AddEvent( ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
    ent->s.otherEntityNum = other->s.number;
  }
  else if( trace->surfaceFlags & SURF_METALSTEPS )
    G_AddEvent( ent, EV_MISSILE_MISS_METAL, DirToByte( trace->plane.normal ) );
  else
    G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );

  ent->freeAfterEvent = qtrue;

  // change over to a normal entity right at the point of impact
  ent->s.eType = ET_GENERAL;

  SnapVectorTowards( trace->endpos, ent->s.pos.trBase );  // save net bandwidth

  G_SetOrigin( ent, trace->endpos );

  // splash damage (doesn't apply to person directly hit)
  if( ent->splashDamage ||
      !strcmp( ent->classname, "lightningEMP" ) )
    G_RadiusDamage( trace->endpos, NULL, NULL,
                    ent->parent, ent->splashDamage, ent->splashRadius,
                    other, ent->splashMethodOfDeath, qtrue );

  SV_LinkEntity( ent );
}

/*
================
G_CheckForMissileImpact

================
*/
static qboolean G_CheckForMissileImpact(gentity_t *ent, vec3_t origin, trace_t *tr) {
  int       passent;

  // ignore interactions with the missile owner
  passent = ent->r.ownerNum;

  // general trace to see if we hit anything at all
  SV_Trace(
    tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs,
    origin, passent, ent->clip_mask, TT_AABB);

  if(tr->startsolid || tr->allsolid) {
    tr->fraction = 0.0f;
    VectorCopy(ent->r.currentOrigin, tr->endpos);
  }

  if(tr->fraction < 1.0f) {
    if(!ent->pointAgainstWorld || tr->contents & CONTENTS_BODY) {
      // We hit an entity or we don't care
      return qtrue;
    } else {
      SV_Trace(
        tr, ent->r.currentOrigin, NULL, NULL, origin,
        passent, ent->clip_mask, TT_AABB);

      if(tr->fraction < 1.0f) {
        // Hit the world with point trace
        return qtrue;
      } else {
        if(tr->contents & CONTENTS_BODY) {
          // Hit an entity
          return qtrue;
        } else {
          SV_Trace(
            tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs,
            origin, passent, *Temp_Clip_Mask(CONTENTS_BODY, 0), TT_AABB);

          if(tr->fraction < 1.0f){
            return qtrue;
          }
        }
      }
    }
  }

  return qfalse;
}

/*
==============
 G_MissileLerpFraction
==============
*/
static float G_MissileLerpFraction(int time, int start_time, int stop_time) {
  float frameMsec;

  if(start_time == stop_time) {
    return 0.0f;
  } else {
    frameMsec = stop_time - start_time;
    return (float)(time - start_time) / (float)frameMsec;
  }
}

/*
================
G_RunMissile

================
*/
void G_RunMissile( gentity_t *ent )
{
  vec3_t    origin;
  trace_t   tr;
  qboolean  impact;
  int       contents;

  // get current position
  BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

  impact = G_CheckForMissileImpact(ent, origin, &tr);

  VectorCopy( tr.endpos, ent->r.currentOrigin );

  if( impact )
  {
    if( tr.surfaceFlags & SURF_NOIMPACT )
    {
      // Never explode or bounce on sky
      G_FreeEntity( ent );
      return;
    }

    G_MissileImpact( ent, &tr );

    if( ent->s.eType != ET_MISSILE )
      return;   // exploded
  }

  //check for scaling of the missile dimensions
  if(ent->scale_missile.enabled && ent->scale_missile.start_time < level.time) {
    vec3_t    backup_mins;
    vec3_t    backup_maxs;
    qboolean  restore_backup = qfalse;

      VectorCopy(ent->r.mins, backup_mins);
      VectorCopy(ent->r.maxs, backup_maxs);

    if(ent->scale_missile.stop_time < level.time) {
      VectorCopy(ent->scale_missile.end_mins, ent->r.mins);
      VectorCopy(ent->scale_missile.end_maxs, ent->r.maxs);

      //don't scale if the missile would impact as a result
      if(G_CheckForMissileImpact(ent, origin, &tr)) {
        restore_backup = qtrue;
      } else {
        //the scaling as finished
        ent->scale_missile.enabled = qfalse;
      }
    } else {
      float lerp = G_MissileLerpFraction(
        level.time,
        ent->scale_missile.start_time,
        ent->scale_missile.stop_time);

      //try scaling
      VectorLerp2(
        lerp, ent->scale_missile.start_mins,
        ent->scale_missile.end_mins,
        ent->r.mins);
      VectorLerp2(
        lerp, ent->scale_missile.start_maxs,
        ent->scale_missile.end_maxs,
        ent->r.maxs);

      //don't scale if the missile would impact as a result
      if(G_CheckForMissileImpact(ent, origin, &tr)) {
        restore_backup = qtrue;
      }
    }

    if(restore_backup) {
      VectorCopy(backup_mins, ent->r.mins);
      VectorCopy(backup_maxs, ent->r.maxs);
    }
  }

  contents = ent->r.contents;
  if(!contents) {
    G_SetContents( ent, CONTENTS_SOLID, qfalse ); //trick SV_LinkEntity into...
  }
  SV_LinkEntity( ent );
  if(!contents) {
    G_SetContents( ent, contents, qfalse ); //...encoding bbox information
  }

  // check think function after bouncing
  G_RunThink( ent );
}


//=============================================================================

/*
=================
fire_flamer

=================
*/
gentity_t *fire_flamer( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;
  vec3_t    pvel;

  VectorNormalize (dir);

  bolt = G_Spawn();
  bolt->classname = "flame";
  bolt->pointAgainstWorld = qfalse;
  bolt->nextthink = level.time + FLAMER_LIFETIME;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_FLAMER;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = FLAMER_DMG;
  bolt->splashDamage = FLAMER_SPLASHDAMAGE;
  bolt->splashRadius = FLAMER_RADIUS;
  bolt->methodOfDeath = MOD_FLAMER;
  bolt->splashMethodOfDeath = MOD_FLAMER_SPLASH;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -FLAMER_START_SIZE;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = FLAMER_START_SIZE;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame

  bolt->scale_missile.enabled = qtrue;
  bolt->scale_missile.start_time = bolt->s.pos.trTime;
  bolt->scale_missile.stop_time = bolt->scale_missile.start_time + FLAMER_EXPAND_TIME;
  VectorCopy(bolt->r.mins, bolt->scale_missile.start_mins);
  VectorCopy(bolt->r.maxs, bolt->scale_missile.start_maxs);
  bolt->scale_missile.end_mins[ 0 ] = bolt->scale_missile.end_mins[ 1 ] = bolt->scale_missile.end_mins[ 2 ] = -FLAMER_FULL_SIZE;
  bolt->scale_missile.end_maxs[ 0 ] = bolt->scale_missile.end_maxs[ 1 ] = bolt->scale_missile.end_maxs[ 2 ] = FLAMER_FULL_SIZE;

  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( self->client->ps.velocity, FLAMER_LAG, pvel );
  VectorMA( pvel, FLAMER_SPEED, dir, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================

/*
=================
fire_blaster

=================
*/
gentity_t *fire_blaster( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize (dir);

  bolt = G_Spawn();
  bolt->classname = "blaster";
  bolt->pointAgainstWorld = qtrue;
  bolt->nextthink = level.time + 10000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_BLASTER;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = BLASTER_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_BLASTER;
  bolt->splashMethodOfDeath = MOD_BLASTER;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -BLASTER_SIZE;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = BLASTER_SIZE;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, BLASTER_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================

/*
=================
fire_pulseRifle

=================
*/
gentity_t *fire_pulseRifle( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize (dir);

  bolt = G_Spawn();
  bolt->classname = "pulse";
  bolt->pointAgainstWorld = qtrue;
  bolt->nextthink = level.time + 10000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_PULSE_RIFLE;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = PRIFLE_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_PRIFLE;
  bolt->splashMethodOfDeath = MOD_PRIFLE;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -PRIFLE_SIZE;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = PRIFLE_SIZE;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, PRIFLE_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================

/*
=================
fire_luciferCannon

=================
*/
gentity_t *fire_luciferCannon( gentity_t *self, vec3_t start, vec3_t dir,
  int damage, int radius, int speed )
{
  gentity_t *bolt;
  float charge;

  VectorNormalize( dir );

  bolt = G_Spawn( );
  bolt->classname = "lcannon";
  bolt->pointAgainstWorld = qtrue;

  bolt->nextthink = level.time + 10000;

  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_LUCIFER_CANNON;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = damage;
  bolt->splashDamage = damage / 2;
  bolt->splashRadius = radius;
  bolt->methodOfDeath = MOD_LCANNON;
  bolt->splashMethodOfDeath = MOD_LCANNON_SPLASH;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;

  // Give the missile a small bounding box
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] =
    -LCANNON_SIZE;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] =
    -bolt->r.mins[ 0 ];

  // Pass the missile charge through
  charge = (float)( damage - LCANNON_SECONDARY_DAMAGE ) / LCANNON_DAMAGE;
  bolt->s.torsoAnim = charge * 255;
  if( bolt->s.torsoAnim < 0 )
    bolt->s.torsoAnim = 0;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, speed, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

/*
=================
launch_grenade

=================
*/
gentity_t *launch_grenade( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize( dir );

  bolt = G_Spawn( );
  bolt->classname = "grenade";
  bolt->pointAgainstWorld = qfalse;
  bolt->nextthink = level.time + 5000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_GRENADE;
  bolt->flags |= FL_BOUNCE_HALF;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = GRENADE_DAMAGE;
  bolt->splashDamage = GRENADE_DAMAGE;
  bolt->splashRadius = GRENADE_RANGE;
  bolt->methodOfDeath = MOD_GRENADE;
  bolt->splashMethodOfDeath = MOD_GRENADE;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -3.0f;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = 3.0f;
  bolt->s.time = level.time;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, GRENADE_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}
//=============================================================================

/*
=================
launch_grenade2

Used for /explode
=================
*/
gentity_t *launch_grenade2( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize( dir );

  bolt = G_Spawn( );
  bolt->classname = "grenade";
  bolt->pointAgainstWorld = qfalse;
  bolt->nextthink = level.time;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_GRENADE;
  bolt->flags |= FL_BOUNCE_HALF;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = GRENADE_DAMAGE;
  bolt->splashDamage = GRENADE_DAMAGE;
  bolt->splashRadius = 1.0f;
  bolt->methodOfDeath = MOD_GRENADE;
  bolt->splashMethodOfDeath = MOD_GRENADE;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -3.0f;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = 3.0f;
  bolt->s.time = level.time;

  bolt->s.pos.trType = TR_STATIONARY;
  bolt->s.pos.trTime = level.time;
  //bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, 0, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}
//=============================================================================

/*
=================
launch_grenade3

Used by the grenade launcher
=================
*/
gentity_t *launch_grenade3( gentity_t *self, vec3_t start, vec3_t dir,
                            qboolean impact )
{
  gentity_t *bolt;

  VectorNormalize(dir);

  bolt = G_Spawn();
  bolt->nextthink = level.time + 5000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_GRENADE;
  if( !impact )
  {
    bolt->classname = "grenade";
    bolt->flags |= FL_BOUNCE_HALF;
  } else
    bolt->classname = "grenade2";
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = LAUNCHER_DAMAGE;
  bolt->splashDamage = LAUNCHER_DAMAGE;
  bolt->splashRadius = LAUNCHER_RADIUS;
  bolt->methodOfDeath = MOD_GRENADE_LAUNCHER;
  bolt->splashMethodOfDeath = MOD_GRENADE_LAUNCHER;
  bolt->noKnockback = qfalse;
  G_SetClipmask(bolt, MASK_SHOT, 0);
  bolt->target_ent = NULL;
  bolt->r.mins[0] = bolt->r.mins[1] = bolt->r.mins[2] = -3.0f;
  bolt->r.maxs[0] = bolt->r.maxs[1] = bolt->r.maxs[2] = 3.0f;
  bolt->s.time = level.time;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME; // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, LAUNCHER_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta ); // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}
//=============================================================================

/*
=================
launch_fragnade

=================
*/
gentity_t *launch_fragnade( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize( dir );

  bolt = G_Spawn( );
  bolt->classname = "grenade";
  bolt->pointAgainstWorld = qfalse;
  bolt->nextthink = level.time + 1500;
  bolt->think = G_KickUpMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_FRAGNADE;
  bolt->flags |= FL_BOUNCE_HALF;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = FRAGNADE_BLAST_DAMAGE;
  bolt->splashDamage = FRAGNADE_BLAST_DAMAGE;
  bolt->splashRadius = FRAGNADE_BLAST_RANGE;
  bolt->methodOfDeath = MOD_FRAGNADE;
  bolt->splashMethodOfDeath = MOD_FRAGNADE;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -3.0f;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = 3.0f;
  bolt->s.time = level.time;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, GRENADE_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}
//=============================================================================

void lasermine_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{
  self->nextthink = level.time + LASERMINE_BOOM_TIME;
  self->think = G_ExplodeMissile;
}

/*
================
G_LaserMineThink
Explode if the laser beam is tripped
================
*/
void G_LaserMineThink(gentity_t *ent) {
  trace_t     previous_trace;
  vec3_t      end;
  bgentity_id unlinked_humans[MAX_CLIENTS];
  int         num_unnlinked_humans = 0;
  int         i;

  //check for self destruct
  if(ent->lasermine_set && ent->lasermine_self_destruct_time < level.time) {
    lasermine_die(ent, ent, ent, LASERMINE_HEALTH, MOD_LASERMINE);
    return;
  }

  ent->nextthink = level.time + LASERMINE_CHECK_FREQUENCY;

  //backup the trace
  previous_trace = ent->lasermine_trace;

  //ignore friendly human players
  for(i = 0; i < MAX_CLIENTS; i++) {
    gentity_t *ent = &g_entities[i];

    if(!ent->inuse) {
      continue;
    }

    if(!ent->r.linked) {
      continue;
    }

    if(!ent->client) {
      continue;
    }

    if(ent->client->pers.connected != CON_CONNECTED) {
      continue;
    }

    if(ent->client->pers.teamSelection != TEAM_HUMANS) {
      continue;
    }

    SV_UnlinkEntity(ent);
    BG_UEID_set(&unlinked_humans[i], i);
    num_unnlinked_humans++;
  }

  VectorMA(ent->r.currentOrigin, LASERMINE_TRIP_RANGE, ent->s.origin2, end);

  //perform a new trace
  SV_Trace(
    &ent->lasermine_trace, ent->r.currentOrigin, NULL, NULL, end, ent->s.number,
    *Temp_Clip_Mask(MASK_SHOT, 0), TT_AABB);

  //relink unlinked human players
  for(i = 0; i < num_unnlinked_humans; i++) {
    gentity_t *ent = &g_entities[BG_UEID_get_ent_num(&unlinked_humans[i])];
    SV_LinkEntity(ent);
  }

  if(!ent->lasermine_set) {
    //the laser mine is being set
    ent->lasermine_self_destruct_time = level.time + LASERMINE_SELF_DESTRUCT;
    ent->s.eFlags |= EF_WARN_CHARGE;
    ent->lasermine_set = qtrue;
    G_AddEvent( ent, EV_LASERMINE_ARMED, 0 );
    return;
  }

  //check if the laser beam has been tripped
  if(
    ent->lasermine_trace.entityNum != previous_trace.entityNum ||
    ent->lasermine_trace.startsolid != previous_trace.startsolid ||
    ent->lasermine_trace.allsolid != previous_trace.allsolid ||
    ent->lasermine_trace.fraction != previous_trace.fraction) {
    lasermine_die(ent, ent, ent, LASERMINE_HEALTH, MOD_LASERMINE);
    return;
  }
}

/*
=================
launch_lasermine

=================
*/
gentity_t *launch_lasermine( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize( dir );

  bolt = G_Spawn( );
  bolt->s.eFlags &= ~EF_WARN_CHARGE;
  bolt->lasermine_set = qfalse;
  bolt->classname = "lasermine";
  bolt->pointAgainstWorld = qfalse;
  bolt->nextthink = level.time + LASERMINE_INIT_TIME;
  bolt->think = G_LaserMineThink;
  bolt->die = lasermine_die;
  bolt->takedamage = qtrue;
  bolt->health = BG_HP2SU(LASERMINE_HEALTH);
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_LASERMINE;
  bolt->flags |= FL_BOUNCE_HALF;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = LASERMINE_DAMAGE;
  bolt->splashDamage = LASERMINE_DAMAGE;
  bolt->splashRadius = LASERMINE_RANGE;
  bolt->methodOfDeath = MOD_LASERMINE;
  bolt->splashMethodOfDeath = MOD_LASERMINE;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  G_SetContents( bolt, CONTENTS_BODY, qfalse );
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -3.0f;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = 3.0f;
  bolt->s.time = level.time;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, LASERMINE_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}
//=============================================================================

/*
=================
lightningBall_die
=================
*/
static void lightningBall_die( gentity_t *self, gentity_t *inflictor,
                               gentity_t *attacker, int damage, int mod )
{
  self->nextthink = level.time;
  //trigger the detonation of any other secondary lightning balls
  self->methodOfDeath = MOD_LIGHTNING_EMP;
  self->splashMethodOfDeath = MOD_LIGHTNING_EMP;
  self->splashRadius = LIGHTNING_BALL_RADIUS2;
}

/*
=================
fire_lightningBall

Used by the lightning gun's secondary fire
=================
*/
gentity_t *fire_lightningBall( gentity_t *self, qboolean EMP,
                               vec3_t start, vec3_t dir )
{
  gentity_t *bolt;
  int speed;

  VectorNormalize (dir);

  bolt = G_Spawn();

  if( EMP )
  {
    bolt->classname = "lightningEMP";
    bolt->s.generic1 = WPM_TERTIARY; //weaponMode
    bolt->nextthink = level.time;
    bolt->damage = LIGHTNING_EMP_DAMAGE;
    bolt->splashDamage = LIGHTNING_EMP_SPLASH_DMG;
    bolt->s.pos.trType = TR_LINEAR;
    bolt->methodOfDeath = MOD_LIGHTNING_EMP;
    bolt->splashMethodOfDeath = MOD_LIGHTNING_EMP;
    bolt->splashRadius = LIGHTNING_EMP_RADIUS;
    speed = LIGHTNING_EMP_SPEED;
  } else
  {
    bolt->classname = "lightningBall";
    bolt->s.generic1 = self->s.generic1; //weaponMode
    bolt->nextthink = level.time + LIGHTNING_BALL_LIFETIME;
    bolt->damage = LIGHTNING_BALL_DAMAGE;
    bolt->flags |= ( FL_BOUNCE_HALF | FL_NO_BOUNCE_SOUND );
    bolt->splashDamage = LIGHTNING_BALL_SPLASH_DMG;
    bolt->s.pos.trType = TR_HALF_GRAVITY;
    bolt->takedamage = qtrue;
    bolt->health = 1;
    bolt->die = lightningBall_die;
    bolt->methodOfDeath = MOD_LIGHTNING;
    bolt->splashMethodOfDeath = MOD_LIGHTNING;
    bolt->splashRadius = LIGHTNING_BALL_RADIUS1;
    speed = LIGHTNING_BALL_SPEED;
  }
  bolt->noKnockback = qfalse;
  bolt->pointAgainstWorld = qtrue;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_LIGHTNING;
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -LIGHTNING_BALL_SIZE;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = LIGHTNING_BALL_SIZE;

  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, speed, bolt->s.pos.trDelta );

  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}
//=============================================================================

/*
================
G_PlayerHasUnexplodedGrenades

Checks to see if a player still has unexploded grenades.
================
 */
qboolean G_PlayerHasUnexplodedGrenades( gentity_t *player )
{
  gentity_t *ent;

  if( !g_nadeSpamProtection.integer )
    return qfalse;

  // just in case this was called for a non-player
  if( !player->client )
    return qfalse;

  // loop through all entities
  for( ent = &g_entities[ MAX_CLIENTS ]; ent < &g_entities[ level.num_entities ]; ++ent )
  {
    // return true if an unexploded grenade belonging to this player is found
    if( ent->s.eType == ET_MISSILE && ent->s.weapon == WP_GRENADE &&
        ent->parent->client == player->client )
      return qtrue;
  }

  return qfalse;
}

//=============================================================================

/*
=================
fire_portalGun

=================
*/
gentity_t *fire_portalGun( gentity_t *self, vec3_t start, vec3_t dir,
                           portal_t portal, qboolean relativeVelocity )
{
  gentity_t *bolt;
  vec3_t pvel;

  VectorNormalize( dir );

  bolt = G_Spawn( );
  bolt->classname = "portalgun";
  bolt->pointAgainstWorld = qfalse;

  bolt->nextthink = level.time + 10000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_PORTAL_GUN;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->s.modelindex2 = portal;
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  G_SetClipmask(bolt, MASK_PLAYERSOLID, 0);
  bolt->noKnockback = qfalse;

  // Give the missile a small bounding box
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] =
    -PORTALGUN_SIZE;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] =
    -bolt->r.mins[ 0 ];

  bolt->s.pos.trType = TR_ACCEL;
  bolt->s.pos.trDuration = PORTALGUN_SPEED * 2;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );

  if( relativeVelocity )
  {
    VectorScale( self->client->ps.velocity, 1.0f, pvel );
    VectorMA( pvel, PORTALGUN_SPEED, dir, bolt->s.pos.trDelta );
  }
  else
    VectorScale( dir, PORTALGUN_SPEED, bolt->s.pos.trDelta );

  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================


/*
================
AHive_SearchAndDestroy

Adjust the trajectory to point towards the target
================
*/
void AHive_SearchAndDestroy(gentity_t *self) {
  vec3_t         dir;
  trace_t        tr;
  gentity_t      *ent;
  bboxPoint_t    targeted_bbox_point;
  bboxPointNum_t bbox_point_num;
  int            i;
  float          d, nearest;

  if(self->parent && !self->parent->inuse) {
    self->parent = NULL;
  }

  if(level.time > self->timestamp) {
    VectorCopy( self->r.currentOrigin, self->s.pos.trBase );
    self->s.pos.trType = TR_STATIONARY;
    self->s.pos.trTime = level.time;

    self->think = G_ExplodeMissile;
    self->nextthink = level.time + 50;
    if( self->parent )
      self->parent->active = qfalse; //allow the parent to start again
    return;
  }

  ent = self->target_ent;
  if(
    ent && ent->health > 0 && ent->client &&
    ent->client->ps.stats[STAT_TEAM] == TEAM_HUMANS) {
      qboolean isVisible = qfalse;
      //check for visibility
      for(
        bbox_point_num = 0; bbox_point_num < NUM_NONVERTEX_BBOX_POINTS;
        bbox_point_num++) {
        bboxPoint_t bboxPoint;

        bboxPoint.num = bbox_point_num;
        BG_EvaluateBBOXPoint( &bboxPoint,
                              ent->r.currentOrigin,
                              ent->r.mins, ent->r.maxs);

        SV_Trace(
          &tr, self->r.currentOrigin, self->r.mins, self->r.maxs,
          bboxPoint.point, self->r.ownerNum, self->clip_mask, TT_AABB);

        if(tr.entityNum != ENTITYNUM_WORLD) {
          isVisible = qtrue;
          targeted_bbox_point.num = bboxPoint.num;
          VectorCopy(bboxPoint.point, targeted_bbox_point.point);
          nearest =
            DistanceSquared(self->r.currentOrigin, targeted_bbox_point.point);
          break;
        }
      }

      if(!isVisible) {
        self->target_ent = NULL;
        nearest = 0; // silence warning
      }
  } else {
    self->target_ent = NULL;
    nearest = 0; // silence warning
  }

  //find the closest human
  for( i = 0; i < MAX_CLIENTS; i++ ) {
    ent = &g_entities[ i ];

    if(G_NoTarget(ent)) {
      continue;
    }

    if(
      ent->client &&
      ent->health > 0 &&
      ent->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
      (
        d = DistanceSquared( ent->r.currentOrigin, self->r.currentOrigin ),
        (self->target_ent == NULL || d < nearest))) {
      for(
        bbox_point_num = 0; bbox_point_num < NUM_NONVERTEX_BBOX_POINTS;
        bbox_point_num++) {
        bboxPoint_t bboxPoint;

        bboxPoint.num = bbox_point_num;
        BG_EvaluateBBOXPoint( &bboxPoint,
                              ent->r.currentOrigin,
                              ent->r.mins, ent->r.maxs);

        SV_Trace(
          &tr, self->r.currentOrigin, self->r.mins, self->r.maxs,
          bboxPoint.point, self->r.ownerNum, self->clip_mask, TT_AABB);

        if(tr.entityNum != ENTITYNUM_WORLD) {
          nearest = d;
          self->target_ent = ent;
          targeted_bbox_point.num = bboxPoint.num;
          VectorCopy(bboxPoint.point, targeted_bbox_point.point);
          break;
        }
      }
    }
  }

  if(self->target_ent == NULL) {
    VectorClear(dir);
  } else {
    VectorSubtract(targeted_bbox_point.point, self->r.currentOrigin, dir);
    VectorNormalize(dir);
  }

  //change direction towards the player
  VectorScale(dir, HIVE_SPEED, self->s.pos.trDelta);
  SnapVector(self->s.pos.trDelta);      // save net bandwidth
  VectorCopy(self->r.currentOrigin, self->s.pos.trBase);
  self->s.pos.trTime = level.time;

  self->nextthink = level.time + HIVE_DIR_CHANGE_PERIOD;
}

/*
=================
fire_hive
=================
*/
gentity_t *fire_hive( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "hive";
  bolt->pointAgainstWorld = qfalse;
  bolt->nextthink = level.time + HIVE_DIR_CHANGE_PERIOD;
  bolt->think = AHive_SearchAndDestroy;
  bolt->s.eType = ET_MISSILE;
  bolt->flags |= FL_BOUNCE | FL_NO_BOUNCE_SOUND;
  bolt->s.weapon = WP_HIVE;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = HIVE_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_SWARM;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = self->target_ent;
  bolt->timestamp = level.time + HIVE_LIFETIME;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, HIVE_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================

/*
=================
fire_lockblob
=================
*/
gentity_t *fire_lockblob( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "lockblob";
  bolt->pointAgainstWorld = qtrue;
  bolt->nextthink = level.time + 15000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_LOCKBLOB_LAUNCHER;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = 0;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_UNKNOWN; //doesn't do damage so will never kill
  bolt->noKnockback = qtrue;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, 500, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

/*
=================
fire_slowBlob
=================
*/
gentity_t *fire_slowBlob( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "slowblob";
  bolt->pointAgainstWorld = qtrue;
  bolt->nextthink = level.time + 15000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_ABUILD2;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = ABUILDER_BLOB_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_SLOWBLOB;
  bolt->splashMethodOfDeath = MOD_SLOWBLOB;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, ABUILDER_BLOB_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

/*
=================
fire_paraLockBlob
=================
*/
gentity_t *fire_paraLockBlob( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "lockblob";
  bolt->pointAgainstWorld = qtrue;
  bolt->nextthink = level.time + 15000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_LOCKBLOB_LAUNCHER;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = 0;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->noKnockback = qtrue;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, LOCKBLOB_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

/*
=================
fire_bounceBall
=================
*/
gentity_t *fire_bounceBall( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "bounceball";
  bolt->pointAgainstWorld = qtrue;
  bolt->nextthink = level.time + 3000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->s.weapon = WP_ALEVEL3_UPG;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = LEVEL3_BOUNCEBALL_DMG;
  if(LEVEL3_BOUNCEBALL_RADIUS) {
    bolt->splashDamage = LEVEL3_BOUNCEBALL_DMG;
  } else {
    bolt->splashDamage = 0;
  }
  bolt->splashRadius = LEVEL3_BOUNCEBALL_RADIUS;
  bolt->methodOfDeath = MOD_LEVEL3_BOUNCEBALL;
  bolt->splashMethodOfDeath = MOD_LEVEL3_BOUNCEBALL;
  bolt->noKnockback = qfalse;
  G_SetClipmask( bolt, MASK_SHOT, 0 );
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, LEVEL3_BOUNCEBALL_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}
