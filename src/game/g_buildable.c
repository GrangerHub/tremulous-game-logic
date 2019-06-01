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

/*
================
G_SetBuildableAnim

Triggers an animation client side
================
*/
void G_SetBuildableAnim( gentity_t *ent, buildableAnimNumber_t anim, qboolean force )
{
  int localAnim = anim | ( ent->s.legsAnim & ANIM_TOGGLEBIT );

  if( force )
    localAnim |= ANIM_FORCEBIT;

  // don't flip the togglebit more than once per frame
  if( ent->animTime != level.time )
  {
    ent->animTime = level.time;
    localAnim ^= ANIM_TOGGLEBIT;
  }

  ent->s.legsAnim = localAnim;
}

/*
================
G_SetIdleBuildableAnim

Set the animation to use whilst no other animations are running
================
*/
void G_SetIdleBuildableAnim( gentity_t *ent, buildableAnimNumber_t anim )
{
  ent->s.torsoAnim = anim;
}

/*
================
G_SuffocateTrappedEntities

Suffocate any entities still trapped within a buildable if we failed to shove
them out from where the buildable was supposed to respawn.
================
*/
void G_SuffocateTrappedEntities( gentity_t *self )
{
  int       i, num;
  int       touch[ MAX_GENTITIES ];
  gentity_t *ent;
  vec3_t    mins, maxs;
  trace_t   tr;

  VectorAdd( self->r.currentOrigin, self->r.mins, mins );
  VectorAdd( self->r.currentOrigin, self->r.maxs, maxs );
  num = SV_AreaEntities( mins, maxs, touch, MAX_GENTITIES );

  for( i = 0; i < num; i++ )
  {
    ent = &g_entities[ touch[ i ] ];

    // skip players with notarget set
    if( G_NoTarget( ent ) )
      continue;

    // only target players
    if( !ent->client )
      continue;

    // avoid targetting self
    if( self == ent )
      continue;

    // dont waste time targetting the dead
    if( ent->health <= 0 )
      continue;

    // not needed for enemies stuck in active defense buildables
    if( ent->client->ps.stats[ STAT_TEAM ] != self->buildableTeam &&
        ( self->s.modelindex == BA_H_MGTURRET ||
          self->s.modelindex == BA_H_TESLAGEN ||
          self->s.modelindex == BA_A_ACIDTUBE ||
          self->s.modelindex == BA_A_HIVE ||
          self->s.modelindex == BA_A_OVERMIND ||
          self->s.modelindex == BA_H_REACTOR ) )
      continue;

    // only suffocate once every second at most
    if( ent->client->lastSuffocationTime &&
        ( level.time < ( ent->client->lastSuffocationTime + 1000 ) ) )
      continue;

    // check to see if entity is really trapped inside
    SV_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs,
                ent->r.currentOrigin, ent->s.number, ent->clipmask, TT_AABB );
    if( tr.startsolid )
    {
      G_Damage( ent, self, self, NULL, NULL,
          BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->health / 10, DAMAGE_NO_PROTECTION,
          MOD_SUFFOCATION );

      ent->client->lastSuffocationTime = level.time;

      G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );
    }
  }
}

/*
===============
G_CheckSpawnPoint

Check if a spawn at a specified point is valid
===============
*/
gentity_t *G_CheckSpawnPoint( int spawnNum, const vec3_t origin,
    const vec3_t normal, buildable_t spawn, vec3_t spawnOrigin )
{
  float   displacement;
  vec3_t  mins, maxs;
  vec3_t  cmins, cmaxs;
  vec3_t  localOrigin;
  trace_t tr;

  BG_BuildableBoundingBox( spawn, mins, maxs );

  if( spawn == BA_A_SPAWN )
  {
    VectorSet( cmins, -MAX_ALIEN_BBOX, -MAX_ALIEN_BBOX, -MAX_ALIEN_BBOX );
    VectorSet( cmaxs,  MAX_ALIEN_BBOX,  MAX_ALIEN_BBOX,  MAX_ALIEN_BBOX );

    displacement = ( maxs[ 2 ] + MAX_ALIEN_BBOX ) * M_ROOT3 + 1.0f;
    VectorMA( origin, displacement, normal, localOrigin );
  }
  else if( spawn == BA_H_SPAWN )
  {
    BG_ClassBoundingBox( PCL_HUMAN, cmins, cmaxs, NULL, NULL, NULL );

    VectorCopy( origin, localOrigin );
    localOrigin[ 2 ] += maxs[ 2 ] + fabs( cmins[ 2 ] ) + 1.0f;
  }
  else
    return NULL;

  SV_Trace( &tr, origin, NULL, NULL, localOrigin, spawnNum, MASK_SHOT, TT_AABB );

  if( tr.entityNum != ENTITYNUM_NONE )
    return &g_entities[ tr.entityNum ];

  SV_Trace( &tr, localOrigin, cmins, cmaxs, localOrigin, -1, MASK_PLAYERSOLID, TT_AABB );

  if( tr.entityNum != ENTITYNUM_NONE )
    return &g_entities[ tr.entityNum ];

  if( spawnOrigin != NULL )
    VectorCopy( localOrigin, spawnOrigin );

  return NULL;
}

/*
===============
G_CheckAreaForSpawnIntersection

Checks if a given area would intersect with a given buildable's spawn area
===============
*/
static qboolean G_CheckAreaForSpawnIntersection(
  const vec3_t origin, const vec3_t normal, buildable_t spawn,
  vec3_t area_mins, vec3_t area_maxs, vec3_t area_origin) {
  float   displacement;
  vec3_t  mins, maxs;
  vec3_t  cmins, cmaxs;
  vec3_t  localOrigin;
  trace_t tr;

  BG_BuildableBoundingBox( spawn, mins, maxs );

  if(spawn == BA_A_SPAWN) {
    VectorSet(cmins, -MAX_ALIEN_BBOX, -MAX_ALIEN_BBOX, -MAX_ALIEN_BBOX);
    VectorSet(cmaxs,  MAX_ALIEN_BBOX,  MAX_ALIEN_BBOX,  MAX_ALIEN_BBOX);

    displacement = (maxs[ 2 ] + MAX_ALIEN_BBOX) * M_ROOT3 + 1.0f;
    VectorMA(origin, displacement, normal, localOrigin);
  } else if(spawn == BA_H_SPAWN) {
    BG_ClassBoundingBox(PCL_HUMAN, cmins, cmaxs, NULL, NULL, NULL);

    VectorCopy( origin, localOrigin );
    localOrigin[2] += maxs[2] + fabs(cmins[2]) + 1.0f;
  } else {
    return qfalse;
  }

  SV_ClipToTestArea(
    &tr, origin, NULL, NULL, localOrigin, area_mins, area_maxs, area_origin,
    MASK_SHOT, MASK_SHOT, TT_AABB);

  if(tr.entityNum != ENTITYNUM_NONE) {
    return qtrue;
  }

  SV_ClipToTestArea(
    &tr, origin, cmins, cmaxs, localOrigin, area_mins, area_maxs, area_origin,
    MASK_PLAYERSOLID, MASK_PLAYERSOLID, TT_AABB);

  if(tr.entityNum != ENTITYNUM_NONE) {
    return qtrue;
  }

  return qfalse;
}

/*
===============
G_PuntBlocker

Move spawn blockers
===============
*/
static void G_PuntBlocker( gentity_t *self, gentity_t *blocker ) {
  vec3_t     nudge;
  gentity_t  *tempEnt = blocker;
  gentity_t  *blockers[ MAX_GENTITIES ], *ignoredEnts[ MAX_GENTITIES ];
  qboolean   damageSpawn = qfalse;
  int        i, numBlockers = 0, numIgnoredEnts = 0;

  if( !g_antiSpawnBlock.integer ) {
    return;
  }

  if( self->attemptSpawnTime < level.time ) {
    self->spawnBlockTime = 0;
    return;
  }

  while( tempEnt ) {
    if( tempEnt->client ) {
      blockers[ numBlockers ] = tempEnt;
      numBlockers++;
    } else {
      ignoredEnts[ numIgnoredEnts ] = tempEnt;
      numIgnoredEnts++;
    }

    SV_UnlinkEntity( tempEnt );

    tempEnt = G_CheckSpawnPoint( self->s.number,self->r.currentOrigin,
                                 self->s.origin2, self->s.modelindex, NULL );
  }

  for( i = 0; i < numBlockers; i++ ) {
    SV_LinkEntity( blockers[ i ] );
  }

  for( i = 0; i < numIgnoredEnts; i++ ) {
    SV_LinkEntity( ignoredEnts[ i ] );
  }

  if( numBlockers <= 0 )
    return;

  if( self ) {
    if( !self->spawnBlockTime ) {
      // begin timer
      self->spawnBlockTime = level.time;
      return;
    }
    else if( level.time - self->spawnBlockTime > 5000 ) {
      // still blocked, get rid of them
      for( i = 0; i < numBlockers; i++ ) {
        if( self->buildableTeam == blockers[i]->client->ps.stats[ STAT_TEAM ] )
          G_Damage( blockers[ i ], NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB,
                    MOD_TRIGGER_HURT );
      }

      self->spawnBlockTime = 0;
      return;
    }
  }

  for( i = 0; i < numBlockers; i++ ) {
    // punt blockers that are on the same team as the buildable
    if( self->buildableTeam == blockers[i]->client->ps.stats[ STAT_TEAM ] ) {
      nudge[ 0 ] = crandom() * 250.0f;
      nudge[ 1 ] = crandom() * 250.0f;
      nudge[ 2 ] = 100.0f;

      VectorAdd( blockers[ i ]->client->ps.velocity, nudge,
                 blockers[ i ]->client->ps.velocity );
      if( G_Expired( blocker, EXP_SPAWN_BLOCK ) ) {
        SV_GameSendServerCommand( blockers[ i ] - g_entities,
                                    va( "cp \"Don't spawn block!\" %d",
                                    CP_SPAWN_BLOCK ) );

        G_SetExpiration( blocker, EXP_SPAWN_BLOCK, 1000 );
      }
    } else {
      // damage the spawn
      damageSpawn = qtrue;
    }
  }

  if( damageSpawn &&
      self->noTriggerHurtDmgTime <= level.time ) {
    G_Damage( self, NULL, NULL, NULL, NULL, BG_HP2SU( 30 ), 0,
              MOD_TRIGGER_HURT );

    self->noTriggerHurtDmgTime = level.time + 1000;
  }
}

#define POWER_REFRESH_TIME  2000

/*
================
G_FindPower

attempt to find power for self, return qtrue if successful
================
*/
qboolean G_FindPower( gentity_t *self, qboolean searchUnspawned )
{
  int       i;
  gentity_t *ent;
  gentity_t *closestPower = NULL;
  int       distance = 0;
  int       minDistance = REPEATER_BASESIZE + 1;
  vec3_t    temp_v;

  if( self->buildableTeam != TEAM_HUMANS )
    return qfalse;

  // Reactor is always powered
  if( self->s.modelindex == BA_H_REACTOR )
  {
    self->parentNode = self;

    return qtrue;
  }

  // Handle repeaters
  if( self->s.modelindex == BA_H_REPEATER )
  {
    self->parentNode = G_Reactor( );

    return self->parentNode != NULL;
  }

  // Iterate through entities
  for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if(
      !strcmp(ent->classname, "target_power") && ent->powered &&
      (int)Distance(self->s.origin, ent->s.origin) <= ent->PowerRadius &&
      (ent->MasterPower || G_Reactor( ) != NULL)) {
      self->parentNode = ent;
      return qtrue;
    }

    if( ent->s.eType != ET_BUILDABLE )
      continue;

    // If entity is a power item calculate the distance to it
    if( ( ent->s.modelindex == BA_H_REACTOR || ent->s.modelindex == BA_H_REPEATER ) &&
        ( searchUnspawned || ent->spawned ) && ent->powered && ent->health > 0 )
    {
      VectorSubtract( self->r.currentOrigin, ent->r.currentOrigin, temp_v );
      distance = VectorLength( temp_v );

      // Always prefer a reactor if there is one in range
      if( ent->s.modelindex == BA_H_REACTOR && distance <= REACTOR_BASESIZE )
      {
        self->parentNode = ent;
       return qtrue;
      }
      else if( distance < minDistance )
      {
        closestPower = ent;
        minDistance = distance;
      }
    }
  }

  self->parentNode = closestPower;
  return self->parentNode != NULL;
}

/*
================
G_PowerEntityForPoint

Simple wrapper to G_FindPower to find the entity providing
power for the specified point
================
*/
gentity_t *G_PowerEntityForPoint( const vec3_t origin )
{
  gentity_t dummy;

  dummy.parentNode = NULL;
  dummy.buildableTeam = TEAM_HUMANS;
  dummy.s.modelindex = BA_NONE;
  VectorCopy( origin, dummy.r.currentOrigin );

  if( G_FindPower( &dummy, qfalse ) )
    return dummy.parentNode;
  else
    return NULL;
}

/*
================
G_PowerEntityForEntity

Simple wrapper to G_FindPower to find the entity providing
power for the specified entity
================
*/
gentity_t *G_PowerEntityForEntity( gentity_t *ent )
{
  if( G_FindPower( ent, qfalse ) )
    return ent->parentNode;
  return NULL;
}

/*
================
G_IsPowered

Check if a location has power, returning the entity type
that is providing it
================
*/
buildable_t G_IsPowered( vec3_t origin )
{
  gentity_t *ent = G_PowerEntityForPoint( origin );

  if( ent )
    return ent->s.modelindex;
  else
    return BA_NONE;
}


/*
==================
G_GetBuildPoints

Get the number of build points from a position
==================
*/
int G_GetBuildPoints(const vec3_t pos, team_t team) {
  if(!IS_WARMUP && G_TimeTilSuddenDeath( ) <= 0) {
    if(G_SD_Mode( ) == SDMODE_SELECTIVE) {
      if(team == TEAM_ALIENS) {
        return level.alienBuildPoints;
      } else if(team == TEAM_HUMANS) {
        return level.humanBuildPoints;
      }
    } else {
      return 0;
    }
  }
  else if(team == TEAM_ALIENS) {
    return level.alienBuildPoints;
  }
  else if( team == TEAM_HUMANS )
  {
    return level.humanBuildPoints;
  }

  return 0;
}

/*
===============
G_BuildablesIntersect

Test if two buildables intersect each other
===============
*/
static qboolean G_BuildablesIntersect( buildable_t a, vec3_t originA,
                                       buildable_t b, vec3_t originB )
{
  vec3_t minsA, maxsA;
  vec3_t minsB, maxsB;

  BG_BuildableBoundingBox( a, minsA, maxsA );
  VectorAdd( minsA, originA, minsA );
  VectorAdd( maxsA, originA, maxsA );

  BG_BuildableBoundingBox( b, minsB, maxsB );
  VectorAdd( minsB, originB, minsB );
  VectorAdd( maxsB, originB, maxsB );

  return BoundsIntersect( minsA, maxsA, minsB, maxsB );
}

/*
==================
G_GetMarkedBuildPoints

Get the number of marked build points from a position
==================
*/
int G_GetMarkedBuildPoints( playerState_t *ps )
{
  gentity_t *ent;
  team_t team = ps->stats[ STAT_TEAM ];
  buildable_t buildable = ( ps->stats[ STAT_BUILDABLE ] & ~SB_VALID_TOGGLEBIT );
  vec3_t            angles;
  vec3_t            origin;
  trace_t           tr1;
  int       i;
  int sum = 0;

  if( G_TimeTilSuddenDeath( ) <= 0 )
    return 0;

  if( !g_markDeconstruct.integer )
    return 0;

  G_SetPlayersLinkState( qfalse, &g_entities[ ps->clientNum ] );
  BG_PositionBuildableRelativeToPlayer( ps, qfalse, G_TraceWrapper, origin,
                                        angles, &tr1 );
  G_SetPlayersLinkState( qtrue, &g_entities[ ps->clientNum ] );

  for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if( ent->s.eType != ET_BUILDABLE )
      continue;

    if( g_markDeconstruct.integer == 3 )
    {
      if( buildable == BA_NONE ||
          !G_BuildablesIntersect( buildable, origin, ent->s.modelindex, ent->r.currentOrigin ) )
        continue;
    }

    if( !ent->inuse )
      continue;

    if( ent->health <= 0 )
      continue;

    if( ent->buildableTeam != team )
      continue;

    if( ent->deconstruct )
      sum += BG_Buildable( ent->s.modelindex )->buildPoints;
  }

  return sum;
}

/*
==================
G_InPowerZone

See if a buildable is inside of another power zone.
Return pointer to provider if so.
It's different from G_FindPower because FindPower for
providers will find themselves.
(This doesn't check if power zones overlap)
==================
*/
gentity_t *G_InPowerZone( gentity_t *self )
{
  int         i;
  gentity_t   *ent;
  int         distance;
  vec3_t      temp_v;

  for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if(
      !strcmp(ent->classname, "target_power") && ent->powered &&
      (int)Distance(self->s.origin, ent->s.origin) <= ent->PowerRadius &&
      (ent->MasterPower || G_Reactor() != NULL)) {
      return ent;
    }

    if( ent->s.eType != ET_BUILDABLE )
      continue;

    if( ent == self )
      continue;

    if( !ent->spawned )
      continue;

    if( ent->health <= 0 )
      continue;

    // if entity is a power item calculate the distance to it
    if( ( ent->s.modelindex == BA_H_REACTOR || ent->s.modelindex == BA_H_REPEATER ) &&
        ent->spawned && ent->powered )
    {
      VectorSubtract( self->r.currentOrigin, ent->r.currentOrigin, temp_v );
      distance = VectorLength( temp_v );

      if( ent->s.modelindex == BA_H_REACTOR && distance <= REACTOR_BASESIZE )
        return ent;
      else if( ent->s.modelindex == BA_H_REPEATER && distance <= REPEATER_BASESIZE )
        return ent;
    }
  }

  return NULL;
}

/*
================
G_FindDCC

attempt to find a controlling DCC for self, return number found
================
*/
int G_FindDCC( gentity_t *self )
{
  int       i;
  gentity_t *ent;
  int       distance = 0;
  vec3_t    temp_v;
  int       foundDCC = 0;

  if( self->buildableTeam != TEAM_HUMANS )
    return 0;

  //iterate through entities
  for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if( ent->s.eType != ET_BUILDABLE )
      continue;

    //if entity is a dcc calculate the distance to it
    if( ent->s.modelindex == BA_H_DCC && ent->spawned )
    {
      VectorSubtract( self->r.currentOrigin, ent->r.currentOrigin, temp_v );
      distance = VectorLength( temp_v );
      if( distance < DC_RANGE && ent->powered )
      {
        foundDCC++;
      }
    }
  }

  return foundDCC;
}

/*
================
G_IsDCCBuilt

See if any powered DCC exists
================
*/
qboolean G_IsDCCBuilt( void )
{
  int       i;
  gentity_t *ent;

  for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if( ent->s.eType != ET_BUILDABLE )
      continue;

    if( ent->s.modelindex != BA_H_DCC )
      continue;

    if( !ent->spawned )
      continue;

    if( ent->health <= 0 )
      continue;

    return qtrue;
  }

  return qfalse;
}

/*
================
G_Reactor
G_Overmind

Since there's only one of these and we quite often want to find them, cache the
results, but check them for validity each time

The code here will break if more than one reactor or overmind is allowed, even
if one of them is dead/unspawned
================
*/
gentity_t *G_Reactor( void )
{
  static gentity_t *rc;

  // If cache becomes invalid renew it
  if( !rc || rc->s.eType != ET_BUILDABLE || rc->s.modelindex != BA_H_REACTOR )
    rc = G_FindBuildable( BA_H_REACTOR );

  // If we found it and it's alive, return it
  if( rc && rc->spawned && rc->health > 0 )
    return rc;

  return NULL;
}

gentity_t *G_Overmind( void )
{
  static gentity_t *om;

  // If cache becomes invalid renew it
  if( !om || om->s.eType != ET_BUILDABLE || om->s.modelindex != BA_A_OVERMIND )
    om = G_FindBuildable( BA_A_OVERMIND );

  // If we found it and it's alive, return it
  if( om && om->spawned && om->health > 0 )
    return om;

  return NULL;
}

/*
================
G_FindCreep

attempt to find creep for self, return qtrue if successful
================
*/
qboolean G_FindCreep( gentity_t *self )
{
  int       i;
  gentity_t *ent;
  gentity_t *closestSpawn = NULL;
  int       distance = 0;
  int       minDistance = 10000;
  vec3_t    temp_v;

  //don't check for creep if flying through the air
  if( !self->client && self->s.groundEntityNum == ENTITYNUM_NONE )
    return qtrue;

  //if self does not have a parentNode or its parentNode is invalid, then find a new one
  if( self->client || self->parentNode == NULL || !self->parentNode->inuse ||
      self->parentNode->health <= 0 ||
      ( Distance( self->r.currentOrigin,
                  self->parentNode->r.currentOrigin ) > CREEP_BASESIZE ) )
  {
    for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
    {
      if(
        !strcmp(ent->classname, "target_creep") && ent->powered &&
        (int)Distance(self->s.origin, ent->s.origin) <= ent->PowerRadius &&
        (ent->MasterPower || G_Reactor() != NULL)) {
        if(!self->client) {
          self->parentNode = ent;
        }

        return qtrue;
      }

      if( ent->s.eType != ET_BUILDABLE )
        continue;

      if( ( ent->s.modelindex == BA_A_SPAWN ||
            ent->s.modelindex == BA_A_OVERMIND ) &&
          ent->spawned && ent->health > 0 )
      {
        VectorSubtract( self->r.currentOrigin, ent->r.currentOrigin, temp_v );
        distance = VectorLength( temp_v );
        if( distance < minDistance )
        {
          closestSpawn = ent;
          minDistance = distance;
        }
      }
    }

    if( minDistance <= CREEP_BASESIZE )
    {
      if( !self->client )
        self->parentNode = closestSpawn;
      return qtrue;
    }
    else
      return qfalse;
  }

  if( self->client )
    return qfalse;

  //if we haven't returned by now then we must already have a valid parent
  return qtrue;
}

/*
================
G_IsCreepHere

simple wrapper to G_FindCreep to check if a location has creep
================
*/
static qboolean G_IsCreepHere( vec3_t origin )
{
  gentity_t dummy;

  memset( &dummy, 0, sizeof( gentity_t ) );

  dummy.parentNode = NULL;
  dummy.s.modelindex = BA_NONE;
  VectorCopy( origin, dummy.r.currentOrigin );

  return G_FindCreep( &dummy );
}

/*
================
G_CreepSlow

Set any nearby humans' SS_CREEPSLOWED flag
================
*/
static void G_CreepSlow( gentity_t *self )
{
  int         entityList[ MAX_GENTITIES ];
  vec3_t      range;
  vec3_t      mins, maxs;
  int         i, num;
  gentity_t   *enemy;
  buildable_t buildable = self->s.modelindex;
  float       creepSize = (float)BG_Buildable( buildable )->creepSize;

  VectorSet( range, creepSize, creepSize, creepSize );

  VectorAdd( self->r.currentOrigin, range, maxs );
  VectorSubtract( self->r.currentOrigin, range, mins );

  //find humans
  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
  {
    enemy = &g_entities[ entityList[ i ] ];

   if( G_NoTarget( enemy ) )
     continue;

    if( enemy->client && enemy->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
        enemy->client->ps.groundEntityNum != ENTITYNUM_NONE )
    {
      enemy->client->ps.stats[ STAT_STATE ] |= SS_CREEPSLOWED;
      enemy->client->lastCreepSlowTime = level.time;
    }
  }
}

/*
================
nullDieFunction

hack to prevent compilers complaining about function pointer -> NULL conversion
================
*/
static void nullDieFunction( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{
}

//==================================================================================


/*
================
AGeneric_CreepRecede

Called when an alien buildable dies
================
*/
void AGeneric_CreepRecede( gentity_t *self )
{
  //if the creep just died begin the recession
  if( !( self->s.eFlags & EF_DEAD ) )
  {
    self->s.eFlags |= EF_DEAD;
    G_QueueBuildPoints( self );

    G_RewardAttackers( self );

    G_AddEvent( self, EV_BUILD_DESTROY, 0 );

    if( self->spawned )
      self->s.time = -level.time;
    else
      self->s.time = -( level.time -
          (int)( (float)CREEP_SCALEDOWN_TIME *
                 ( 1.0f - ( (float)( level.time - self->buildTime ) /
                            (float)BG_Buildable( self->s.modelindex )->buildTime ) ) ) );
  }

  //creep is still receeding
  if( ( self->timestamp + 10000 ) > level.time )
    self->nextthink = level.time + 500;
  else //creep has died
  {
    G_FreeEntity( self );
  }
}

/*
================
AGeneric_Blast

Called when an Alien buildable explodes after dead state
================
*/

void AGeneric_Blast( gentity_t *self )
{
  vec3_t dir;

  VectorCopy( self->s.origin2, dir );

  //do a bit of radius damage
  G_SelectiveRadiusDamage( self->s.pos.trBase, self->r.mins, self->r.maxs,
                           g_entities + self->killedBy, self->splashDamage,
                           self->splashRadius, self, self->splashMethodOfDeath,
                           TEAM_ALIENS, qtrue );

  //pretty events and item cleanup
  self->s.eFlags |= EF_NODRAW; //don't draw the model once it's destroyed
  G_AddEvent( self, EV_ALIEN_BUILDABLE_EXPLOSION, DirToByte( dir ) );
  self->timestamp = level.time;
  self->think = AGeneric_CreepRecede;
  self->nextthink = level.time + 500;

  G_SetContents( self, 0);    //stop collisions...
  SV_LinkEntity( self ); //...requires a relink
}

/*
================
AGeneric_Die

Called when an Alien buildable is killed and enters a brief dead state prior to
exploding.
================
*/
void AGeneric_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{
  G_SetBuildableAnim( self, BANIM_DESTROY1, qtrue );
  G_SetIdleBuildableAnim( self, BANIM_DESTROYED );

  self->die = nullDieFunction;
  self->killedBy = attacker - g_entities;
  self->think = AGeneric_Blast;
  self->s.eFlags &= ~EF_FIRING; //prevent any firing effects
  self->powered = qfalse;
  self->methodOfDeath = mod;

  if(
    self->spawned &&
    !(
      inflictor->s.eType == ET_MOVER &&
      (
        inflictor->s.pos.trType == TR_SINE ||
        inflictor->s.apos.trType == TR_SINE)))
    self->nextthink = level.time + 5000;
  else
    self->nextthink = level.time; //blast immediately

  G_RemoveBuildableFromStack( self->s.groundEntityNum, self->s.number );
  G_SetBuildableDropper( self->s.number, attacker->s.number );

  G_RemoveRangeMarkerFrom( self );
  G_LogDestruction( self, attacker, mod );
}

/*
================
AGeneric_CreepCheck

Tests for creep and kills the buildable if there is none
================
*/
void AGeneric_CreepCheck( gentity_t *self )
{
  gentity_t *spawn;

  spawn = self->parentNode;
  if( !G_FindCreep( self ) )
  {
    // don't use killedBy for spawns that were already freed (such as by deconning)
    if( spawn && spawn->inuse &&
        ( spawn->s.modelindex == BA_A_SPAWN ||
          spawn->s.modelindex == BA_A_OVERMIND ) &&
        ( Distance( self->r.currentOrigin,
                    spawn->r.currentOrigin ) <= CREEP_BASESIZE ) )
      G_Damage( self, NULL, g_entities + spawn->killedBy, NULL, NULL,
                0, DAMAGE_INSTAGIB, MOD_NOCREEP );
    else
      G_Damage( self, NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_NOCREEP );
    return;
  }
  G_CreepSlow( self );
}

/*
================
AGeneric_Think

A generic think function for Alien buildables
================
*/
void AGeneric_Think( gentity_t *self )
{
  self->powered = G_Overmind( ) != NULL;
  self->nextthink = level.time + BG_Buildable( self->s.modelindex )->nextthink;

  G_SuffocateTrappedEntities( self );

  AGeneric_CreepCheck( self );
}

/*
================
AGeneric_Pain

A generic pain function for Alien buildables
================
*/
void AGeneric_Pain( gentity_t *self, gentity_t *attacker, int damage )
{
  if( self->health <= 0 )
    return;

  // Alien buildables only have the first pain animation defined
  G_SetBuildableAnim( self, BANIM_PAIN1, qfalse );
}




//==================================================================================

/*
================
ASpawn_Think

think function for Alien Spawn
================
*/
void ASpawn_Think( gentity_t *self )
{
  gentity_t *ent;

  if( self->spawned )
  {
    //only suicide if at rest
    if( self->s.groundEntityNum != ENTITYNUM_NONE )
    {
      if( ( ent = G_CheckSpawnPoint( self->s.number, self->r.currentOrigin,
              self->s.origin2, BA_A_SPAWN, NULL ) ) != NULL )
      {
        // If the thing blocking the spawn is a buildable, kill it.
        // If it's part of the map, kill self.
        if( ent->s.eType == ET_BUILDABLE )
        {
          // don't queue the bp from this
          if( ent->builtBy && ent->builtBy->slot >= 0 )
            G_Damage( ent, NULL, g_entities + ent->builtBy->slot, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_SUICIDE );
          else
            G_Damage( ent, NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_SUICIDE );

          if( ent->health > 0 )
            G_SetBuildableAnim( self, BANIM_SPAWN1, qtrue );
        }
        else if( ent->s.number == ENTITYNUM_WORLD || ent->s.eType == ET_MOVER )
        {
          G_Damage( self, NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_SUICIDE );
          return;
        }

        G_PuntBlocker( self, ent );

        if( ent->s.eType == ET_CORPSE )
          G_FreeEntity( ent ); //quietly remove
      }
      else
        self->spawnBlockTime = 0;
    }
  }

  G_CreepSlow( self );

  self->nextthink = level.time + BG_Buildable( self->s.modelindex )->nextthink;

  G_SuffocateTrappedEntities( self );
}





//==================================================================================





#define OVERMIND_ATTACK_PERIOD 10000
#define OVERMIND_DYING_PERIOD  5000
#define OVERMIND_SPAWNS_PERIOD 30000

/*
================
AOvermind_Think

Think function for Alien Overmind
================
*/
void AOvermind_Think( gentity_t *self )
{
  int    i;

  if( self->spawned && ( self->health > 0 ) )
  {
    vec3_t    range = { OVERMIND_ATTACK_RANGE, OVERMIND_ATTACK_RANGE, OVERMIND_ATTACK_RANGE };
    vec3_t    mins, maxs;
    int       entityList[ MAX_GENTITIES ];
    int       num;
    gentity_t *enemy;

    VectorAdd( self->s.pos.trBase, range, maxs );
    VectorSubtract( self->s.pos.trBase, range, mins );
    //do some damage
    num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
    for( i = 0; i < num; i++ )
    {
      enemy = &g_entities[ entityList[ i ] ];

      if( enemy->client && enemy->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
          G_SelectiveRadiusDamage( self->s.pos.trBase,
                                   self->r.mins, self->r.maxs,
                                   self, self->splashDamage,
                                   self->splashRadius, self,
                                   MOD_OVERMIND, TEAM_ALIENS, qtrue ) )
      {
        self->timestamp = level.time;
        G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );
      }
    }

    // just in case an egg finishes building after we tell overmind to stfu
    if( level.numAlienSpawns > 0 )
      level.overmindMuted = qfalse;

    // shut up during intermission
    if( level.intermissiontime )
      level.overmindMuted = qtrue;

    //low on spawns
    if( !level.overmindMuted && level.numAlienSpawns <= 0 &&
        level.time > self->overmindSpawnsTimer )
    {
      qboolean haveBuilder = qfalse;
      gentity_t *builder;

      self->overmindSpawnsTimer = level.time + OVERMIND_SPAWNS_PERIOD;
      G_BroadcastEvent( EV_OVERMIND_SPAWNS, 0 );

      for( i = 0; i < level.numConnectedClients; i++ )
      {
        builder = &g_entities[ level.sortedClients[ i ] ];
        if( builder->health > 0 &&
          ( builder->client->pers.classSelection == PCL_ALIEN_BUILDER0 ||
            builder->client->pers.classSelection == PCL_ALIEN_BUILDER0_UPG ) )
        {
          haveBuilder = qtrue;
          break;
        }
      }
      // aliens now know they have no eggs, but they're screwed, so stfu
      if( !haveBuilder || G_TimeTilSuddenDeath( ) <= 0 )
        level.overmindMuted = qtrue;
    }

    //overmind dying
    if( self->health < ( OVERMIND_HEALTH / 10.0f ) && level.time > self->overmindDyingTimer )
    {
      self->overmindDyingTimer = level.time + OVERMIND_DYING_PERIOD;
      G_BroadcastEvent( EV_OVERMIND_DYING, 0 );
    }

    //overmind under attack
    if( self->health < self->lastHealth && level.time > self->overmindAttackTimer )
    {
      self->overmindAttackTimer = level.time + OVERMIND_ATTACK_PERIOD;
      G_BroadcastEvent( EV_OVERMIND_ATTACK, 0 );
    }

    self->lastHealth = self->health;
  }
  else
    self->overmindSpawnsTimer = level.time + OVERMIND_SPAWNS_PERIOD;

  G_CreepSlow( self );

  self->nextthink = level.time + BG_Buildable( self->s.modelindex )->nextthink;

  G_SuffocateTrappedEntities( self );
}





//==================================================================================





/*
================
ABarricade_Pain

Barricade pain animation depends on shrunk state
================
*/
void ABarricade_Pain( gentity_t *self, gentity_t *attacker, int damage )
{
  if( self->health <= 0 )
    return;

  if( !self->shrunkTime )
    G_SetBuildableAnim( self, BANIM_PAIN1, qfalse );
  else
    G_SetBuildableAnim( self, BANIM_PAIN2, qfalse );
}

/*
================
ABarricade_Shrink

Set shrink state for a barricade. When unshrinking, checks to make sure there
is enough room.
================
*/
void ABarricade_Shrink( gentity_t *self, qboolean shrink )
{
  if ( !self->spawned || self->health <= 0 )
    shrink = qtrue;
  if ( shrink && self->shrunkTime )
  {
    int anim;

    // We need to make sure that the animation has been set to shrunk mode
    // because we start out shrunk but with the construct animation when built
    self->shrunkTime = level.time;
    anim = self->s.torsoAnim & ~( ANIM_FORCEBIT | ANIM_TOGGLEBIT );
    if ( self->spawned && self->health > 0 && anim != BANIM_DESTROYED )
    {
      G_SetIdleBuildableAnim( self, BANIM_DESTROYED );
      G_SetBuildableAnim( self, BANIM_ATTACK1, qtrue );
    }
    return;
  }

  if ( !shrink && ( !self->shrunkTime ||
       level.time < self->shrunkTime + BARRICADE_SHRINKTIMEOUT ) )
    return;

  BG_BuildableBoundingBox( BA_A_BARRICADE, self->r.mins, self->r.maxs );

  if ( shrink )
  {
    self->r.maxs[ 2 ] = (int)( self->r.maxs[ 2 ] * BARRICADE_SHRINKPROP );
    self->shrunkTime = level.time;

    // shrink animation, the destroy animation is used
    if ( self->spawned && self->health > 0 )
    {
      G_SetBuildableAnim( self, BANIM_ATTACK1, qtrue );
      G_SetIdleBuildableAnim( self, BANIM_DESTROYED );
    }
  }
  else
  {
    trace_t tr;
    int anim;

    SV_Trace( &tr, self->r.currentOrigin, self->r.mins, self->r.maxs,
                self->r.currentOrigin, self->s.number, MASK_PLAYERSOLID, TT_AABB );
    if ( tr.startsolid || tr.fraction < 1.0f )
    {
      self->r.maxs[ 2 ] = (int)( self->r.maxs[ 2 ] * BARRICADE_SHRINKPROP );
      return;
    }
    self->shrunkTime = 0;

    // unshrink animation, IDLE2 has been hijacked for this
    anim = self->s.legsAnim & ~( ANIM_FORCEBIT | ANIM_TOGGLEBIT );
    if ( self->spawned && self->health > 0 &&
         anim != BANIM_CONSTRUCT1 && anim != BANIM_CONSTRUCT2 )
    {
      G_SetIdleBuildableAnim( self, BG_Buildable( BA_A_BARRICADE )->idleAnim );
      G_SetBuildableAnim( self, BANIM_ATTACK2, qtrue );
    }
  }

  // a change in size requires a relink
  if ( self->spawned )
    SV_LinkEntity( self );
}

/*
================
ABarricade_Die

Called when an alien barricade dies
================
*/
void ABarricade_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{
  AGeneric_Die( self, inflictor, attacker, damage, mod );
  ABarricade_Shrink( self, qtrue );
}

/*
================
ABarricade_Think

Think function for Alien Barricade
================
*/
void ABarricade_Think( gentity_t *self )
{
  AGeneric_Think( self );

  // Shrink if unpowered
  ABarricade_Shrink( self, !self->powered );
}

/*
================
ABarricade_Touch

Barricades shrink when they are come into contact with an Alien that can
pass through
================
*/

void ABarricade_Touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
  gclient_t *client = other->client;
  int client_z, min_z;

  g_trigger_success = qfalse;

  if( !client || client->pers.teamSelection != TEAM_ALIENS )
    return;

  // Client must be high enough to pass over. Note that STEPSIZE (18) is
  // hardcoded here because we don't include bg_local.h!
  client_z = other->r.currentOrigin[ 2 ] + other->r.mins[ 2 ];
  min_z = self->r.currentOrigin[ 2 ] - 18 +
          (int)( self->r.maxs[ 2 ] * BARRICADE_SHRINKPROP );
  if( client_z < min_z )
    return;
  ABarricade_Shrink( self, qtrue );
  g_trigger_success = qtrue;
}

//==================================================================================




/*
================
AAcidTube_Think

Think function for Alien Acid Tube
================
*/
void AAcidTube_Think( gentity_t *self )
{
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { ACIDTUBE_RANGE, ACIDTUBE_RANGE, ACIDTUBE_RANGE };
  vec3_t    mins, maxs;
  int       i, num;
  gentity_t *enemy;

  AGeneric_Think( self );

  VectorAdd( self->r.currentOrigin, range, maxs );
  VectorSubtract( self->r.currentOrigin, range, mins );

  // attack nearby humans
  if( self->spawned && self->health > 0 && self->powered )
  {
    num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
    for( i = 0; i < num; i++ )
    {
      enemy = &g_entities[ entityList[ i ] ];

      if( G_NoTarget( enemy ) )
        continue;

      if( enemy->client && enemy->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
      {
        if( !G_SelectiveRadiusDamage( self->s.pos.trBase,
                                      self->r.mins, self->r.maxs,
                                      self, ACIDTUBE_DAMAGE,
                                      ACIDTUBE_RANGE, self,
                                      MOD_ATUBE, TEAM_ALIENS, qfalse ) )
          return;

        // start the attack animation
        if( level.time >= self->timestamp + ACIDTUBE_REPEAT_ANIM )
        {
          self->timestamp = level.time;
          G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );
          G_AddEvent( self, EV_ALIEN_ACIDTUBE, DirToByte( self->s.origin2 ) );
        }

        self->nextthink = level.time + ACIDTUBE_REPEAT;
        return;
      }
    }
  }
}




//==================================================================================

/*
================
AHive_CheckTarget

Returns true and fires the hive missile if the target is valid
================
*/
static qboolean AHive_CheckTarget( gentity_t *self, gentity_t *enemy )
{
  trace_t trace;
  vec3_t tip_origin, dirToTarget;
  bboxPointNum_t bbox_point_num;

  // Check if this is a valid target
  if( enemy->health <= 0 || !enemy->client ||
      enemy->client->ps.stats[ STAT_TEAM ] != TEAM_HUMANS )
    return qfalse;

  if( G_NoTarget( enemy ) )
    return qfalse;

  // Check if the tip of the hive can see the target
  VectorMA( self->s.pos.trBase, self->r.maxs[ 2 ], self->s.origin2,
            tip_origin );

  for(
    bbox_point_num = 0; bbox_point_num < NUM_NONVERTEX_BBOX_POINTS;
    bbox_point_num++) {
    bboxPoint_t bboxPoint;

    bboxPoint.num = bbox_point_num;
    BG_EvaluateBBOXPoint( &bboxPoint,
                          enemy->r.currentOrigin,
                          enemy->r.mins, enemy->r.maxs);

    if(Distance(tip_origin, bboxPoint.point) > HIVE_SENSE_RANGE) {
      continue;
    }

    SV_Trace( &trace, tip_origin, NULL, NULL, bboxPoint.point,
                self->s.number, MASK_SHOT, TT_AABB );
    if( trace.fraction < 1.0f && trace.entityNum != enemy->s.number ) {
      continue;
    }

    self->active = qtrue;
    self->target_ent = enemy;
    self->timestamp = level.time + HIVE_REPEAT;

    BG_EvaluateBBOXPoint( &bboxPoint,
                          enemy->s.pos.trBase,
                          enemy->r.mins, enemy->r.maxs);

    VectorSubtract( bboxPoint.point, self->s.pos.trBase, dirToTarget );
    VectorNormalize( dirToTarget );
    vectoangles( dirToTarget, self->turretAim );

    // Fire at target
    FireWeapon( self );
    G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );
    return qtrue;
  }

  return qfalse;
}

/*
================
AHive_Think

Think function for Alien Hive
================
*/
void AHive_Think( gentity_t *self )
{
  int       start;

  AGeneric_Think( self );

  // Hive missile hasn't returned in HIVE_REPEAT seconds, forget about it
  if( self->timestamp < level.time )
    self->active = qfalse;

  // Find a target to attack
  if( self->spawned && !self->active && self->powered )
  {
    int i, num, entityList[ MAX_GENTITIES ];
    vec3_t mins, maxs,
           range = { HIVE_SENSE_RANGE, HIVE_SENSE_RANGE, HIVE_SENSE_RANGE };

    VectorAdd( self->r.currentOrigin, range, maxs );
    VectorSubtract( self->r.currentOrigin, range, mins );

    num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );

    if( num == 0 )
      return;

    start = rand( ) / ( RAND_MAX / num + 1 );
    for( i = start; i < num + start; i++ )
    {
      if( AHive_CheckTarget( self, g_entities + entityList[ i % num ] ) )
        return;
    }
  }
}

/*
================
AHive_Pain

pain function for Alien Hive
================
*/
void AHive_Pain( gentity_t *self, gentity_t *attacker, int damage )
{
  if( self->spawned && self->powered && !self->active )
    AHive_CheckTarget( self, attacker );

  G_SetBuildableAnim( self, BANIM_PAIN1, qfalse );
}


//==================================================================================


/*
================
ABooster_Touch

Called when an alien touches a booster
================
*/
void ABooster_Touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
  gclient_t *client = other->client;

  g_trigger_success = qfalse;
  
  if( !self->spawned || !self->powered || self->health <= 0 )
    return;

  if( !client )
    return;

  if( client->ps.stats[ STAT_TEAM ] != TEAM_ALIENS )
    return;

  client->ps.stats[ STAT_STATE ] |= SS_BOOSTED;
  client->boostedTime = level.time;
  g_trigger_success = qtrue;
}

/*
================
ABooster_Think

Boosts aliens that are in range and line of sight
================
*/
void ABooster_Think( gentity_t *self )
{
  int i, num;
  int entityList[ MAX_GENTITIES ];
  vec3_t range, mins, maxs;

  AGeneric_Think( self );

  if( !self->spawned || !self->powered || self->health <= 0 )
    return;

  VectorSet( range, REGEN_BOOST_RANGE, REGEN_BOOST_RANGE,
             REGEN_BOOST_RANGE );
  VectorAdd( self->r.currentOrigin, range, maxs );
  VectorSubtract( self->r.currentOrigin, range, mins );

  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
  {
    gentity_t *player = &g_entities[ entityList[ i ] ];
    gclient_t *client = player->client;

    if( !client )
      continue;

    if( client->ps.stats[ STAT_TEAM ] != TEAM_ALIENS )
      continue;

    if( Distance( client->ps.origin, self->r.currentOrigin ) > REGEN_BOOST_RANGE )
      continue;

    if( !G_Visible( self, player, (CONTENTS_SOLID|CONTENTS_PLAYERCLIP) ) )
      continue;

    client->ps.stats[ STAT_STATE ] |= SS_BOOSTED;
    client->boostedTime = level.time;
  }
}


//==================================================================================


/*
================
AHovel_Blocked

Is this hovel entrance blocked?
================
*/
qboolean AHovel_Blocked( gentity_t *hovel, gentity_t *player, qboolean provideExit )
{
  vec3_t    mins, maxs, bmins, bmaxs, target, position;
  trace_t   tr;
  int       i;

  BG_ClassBoundingBox( player->client->ps.stats[ STAT_CLASS ],
                       mins, maxs, NULL, NULL, NULL );
  BG_BuildableBoundingBox( BA_A_HOVEL, bmins, bmaxs );
  for( i = 0; i < 3; i++ )
  {
    maxs[i]++;
    mins[i]--;
  }
  VectorCopy( hovel->r.currentOrigin, position );
  position[2] += fabs( mins[2] ) + bmaxs[2] + 1.0f;
  VectorCopy( hovel->r.currentOrigin, target );
  target[2] += fabs( mins[2] ) + 1.0f;
  SV_UnlinkEntity( player );
  SV_Trace( &tr, position, mins, maxs, target, hovel->s.number, MASK_PLAYERSOLID,
            TT_AABB );
  SV_LinkEntity( player );
  if( tr.startsolid || tr.fraction < 1.0f )
    return qtrue;
  if( provideExit == qtrue ){
    vec3_t newAngles;
    G_SetOrigin( player, position );
    VectorCopy( position, player->client->ps.origin );
    VectorCopy( vec3_origin, player->client->ps.velocity );
    VectorCopy( hovel->r.currentAngles, newAngles );
    newAngles[ ROLL ] = 0;
    G_SetClientViewAngle( player, newAngles );
  }
  return qfalse;
}

/*
================
APropHovel_Blocked

Wrapper to test a hovel placement for validity
================
*/
static qboolean APropHovel_Blocked( vec3_t origin, vec3_t angles, vec3_t normal,
                                    gentity_t *player )
{
  gentity_t hovel;

  VectorCopy( origin, hovel.r.currentOrigin );
  VectorCopy( angles, hovel.r.currentAngles );
  hovel.s.number = ENTITYNUM_NONE;

  return AHovel_Blocked( &hovel, player, qfalse );
}
/*
=================
G_PositionHovelsBuilder

Postitions a hovel's ocupying granger
=================
*/
void G_PositionHovelsBuilder( gentity_t *self )
{
  vec3_t  hovelOrigin, hovelAngles, inverseNormal;
  vec3_t  mins, maxs;
  trace_t tr;

  if( !self->occupation.occupant )
    return;

  BG_ClassBoundingBox( self->occupation.occupant->client->ps.stats[ STAT_CLASS ],
                       mins, maxs, NULL, NULL, NULL );

  VectorCopy( self->r.currentOrigin, hovelOrigin );
  hovelOrigin[2] += 128.0f;

  SV_UnlinkEntity( self->occupation.occupant );
  SV_Trace( &tr, self->r.currentOrigin, mins, maxs, hovelOrigin, self->s.number,
            MASK_DEADSOLID, TT_CAPSULE );
  SV_LinkEntity( self->occupation.occupant );

  if( tr.fraction < 1.0f )
    hovelOrigin[2] = tr.endpos[2];

  VectorCopy( self->s.origin2, inverseNormal );
  VectorInverse( inverseNormal );
  vectoangles( inverseNormal, hovelAngles );
  hovelAngles[YAW] = self->r.currentAngles[YAW];

  VectorCopy( self->occupation.occupant->r.currentOrigin, self->occupation.occupant->client->hovelOrigin );

  G_SetOrigin( self->occupation.occupant, hovelOrigin );
  VectorCopy( hovelOrigin, self->occupation.occupant->client->ps.origin );
  G_SetClientViewAngle( self->occupation.occupant, hovelAngles );
}

/*
================
AHovel_Activate

Called when an alien activates a hovel
================
*/
qboolean AHovel_Activate( gentity_t *self, gentity_t *activator )
{
  G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );

  if( self->occupation.occupantFound->client )
  {
    //prevent lerping
    self->occupation.occupantFound->client->ps.eFlags ^= EF_TELEPORT_BIT;
    self->occupation.occupantFound->client->ps.eFlags |= EF_NODRAW;
      return qtrue;
  } else
    return qfalse;
}

/*
================
AHovel_CanActivate

custom restrictions on the search for a nearby hovel that can be activated
================
*/
qboolean AHovel_CanActivate( gentity_t *self, gclient_t *client )
{
  return BG_ClassHasAbility( client->ps.stats[STAT_CLASS], SCA_CANHOVEL );
}

/*
================
AHovel_WillActivate

custom restrictions called to determine if a hovel will be activated
================
*/
qboolean AHovel_WillActivate( gentity_t *actEnt, gentity_t *activator )
{
  if( actEnt->occupation.occupantFound->client &&
      actEnt->occupation.occupantFound->client->noclip )
  {
    //activator has noclip on
    activator->activation.menuMsg = MN_A_HOVEL_NOCLIP;
    return qfalse;
  }

  if( AHovel_Blocked( actEnt, actEnt->occupation.occupantFound, qfalse ) )
  {
    //you can get in, but you can't get out
    activator->activation.menuMsg = MN_A_HOVEL_BLOCKED;
    return qfalse;
  }

  return qtrue;
}

/*
================
AHovel_Occupy

called when a hovel is being entered
================
*/
void AHovel_Occupy( gentity_t *occupied )
{
  if( !occupied->occupation.occupant )
    return;

  G_PositionHovelsBuilder( occupied->occupation.occupant );

  if( occupied->occupation.occupant->client )
    occupied->occupation.occupant->client->ps.stats[ STAT_STATE ] |= SS_HOVELING;
}

/*
================
AHovel_Unoccupy

called to exit a hovel
================
*/
qboolean  AHovel_Unoccupy( gentity_t *occupied, gentity_t *occupant,
                           gentity_t *activator, qboolean force )
{
  if( force )
  {
    vec3_t    newOrigin;
    vec3_t    newAngles;

    VectorCopy( occupied->r.currentAngles, newAngles );
    newAngles[ ROLL ] = 0;

    VectorCopy( occupied->r.currentOrigin, newOrigin );
    VectorMA( newOrigin, 1.0f, occupied->s.origin2, newOrigin );

    //prevent lerping
    occupant->client->ps.eFlags ^= EF_TELEPORT_BIT;
    occupant->client->ps.eFlags &= ~EF_NODRAW;

    G_SetOrigin( occupant, newOrigin );
    VectorCopy( newOrigin, occupant->client->ps.origin );
    G_SetClientViewAngle( occupant, newAngles );

    return qtrue;
  }

  // only let the player out if there is room
  if ( !AHovel_Blocked( occupied, occupant, qtrue ) )
  {
    // prevent lerping
    occupant->client->ps.eFlags ^= EF_TELEPORT_BIT;
    occupant->client->ps.eFlags &= ~EF_NODRAW;
    return qtrue;
  }

  return qfalse;
}

/*
================
AHovel_OccupantReset

Custom reset for entities that left the hovel
================
*/
void AHovel_OccupantReset( gentity_t *occupant )
{
  if( occupant && occupant->client )
  {
    if( occupant->client )
      occupant->client->ps.stats[ STAT_STATE ] &= ~SS_HOVELING;
  }
}

/*
================
AHovel_OccupiedReset

called to exit a hovel
================
*/
void AHovel_OccupiedReset( gentity_t *occupied )
{
  if( occupied )
    G_SetBuildableAnim( occupied, BANIM_ATTACK2, qfalse );
}

/*
================
AHovel_Think

Think for alien hovel
================
*/
void AHovel_Think( gentity_t *self )
{
  if( self->spawned )
  {
    if( self->active )
      G_SetIdleBuildableAnim( self, BANIM_IDLE2 );
    else
      G_SetIdleBuildableAnim( self, BANIM_IDLE1 );
  }

  AGeneric_Think( self );
  
  // hovels are always powered
  self->powered = qtrue;

  G_CreepSlow( self );
}

/*
================
AHovel_Die

Die for alien hovel
================
*/
void AHovel_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{
  vec3_t  dir;

  VectorCopy( self->s.origin2, dir );

  G_LogDestruction( self, attacker, mod );

  if( mod != MOD_DECONSTRUCT )
  {
    //do a bit of radius damage
    G_SelectiveRadiusDamage( self->s.pos.trBase,
                             self->r.mins, self->r.maxs,
                             self, self->splashDamage,
                             self->splashRadius, self,
                             self->splashMethodOfDeath,
                             TEAM_ALIENS, qtrue );

    //pretty events and item cleanup
    self->s.eFlags |= EF_NODRAW; //don't draw the model once its destroyed
    G_AddEvent( self, EV_ALIEN_BUILDABLE_EXPLOSION, DirToByte( dir ) );
    self->die = nullDieFunction;
    self->s.eFlags &= ~EF_FIRING; //prevent any firing effects
    self->timestamp = level.time;
    self->think = AGeneric_CreepRecede;
    self->nextthink = level.time + 500; //wait .5 seconds before damaging others
    self->methodOfDeath = mod;
    self->powered = qfalse;
  }

  self->r.contents = 0;    //stop collisions...
  SV_LinkEntity( self ); //...requires a relink
}

//==================================================================================

#define TRAPPER_ACCURACY 10 // lower is better

/*
================
ATrapper_FireOnEnemy

Used by ATrapper_Think to fire at enemy
================
*/
static void ATrapper_FireOnEnemy( gentity_t *self, int firespeed )
{
  gentity_t *enemy = self->enemy;
  vec3_t    dirToTarget, bestDirToTarget;
  vec3_t    halfAcceleration, thirdJerk;
  float     distanceToTarget = BG_Buildable( self->s.modelindex )->turretRange;
  float     bestDistanceToTarget;
  int       i = 0;
  int       lowMsec = 0;
  int       highMsec = (int)( (
    ( ( distanceToTarget * LOCKBLOB_SPEED ) +
      ( distanceToTarget * BG_Class( enemy->client->ps.stats[ STAT_CLASS ] )->speed ) ) /
    ( LOCKBLOB_SPEED * LOCKBLOB_SPEED ) ) * 1000.0f );

  VectorSubtract( enemy->s.pos.trBase, self->s.pos.trBase, bestDirToTarget );
  bestDistanceToTarget = VectorLength( bestDirToTarget );

  VectorScale( enemy->acceleration, 1.0f / 2.0f, halfAcceleration );
  VectorScale( enemy->jerk, 1.0f / 3.0f, thirdJerk );

  // highMsec and lowMsec can only move toward
  // one another, so the loop must terminate
  while( highMsec - lowMsec > TRAPPER_ACCURACY )
  {
    int   partitionMsec = ( highMsec + lowMsec ) / 2;
    float time = (float)partitionMsec / 1000.0f;
    float projectileDistance = LOCKBLOB_SPEED * time;

    VectorMA( enemy->s.pos.trBase, time, enemy->s.pos.trDelta, dirToTarget );
    VectorMA( dirToTarget, time * time, halfAcceleration, dirToTarget );
    VectorMA( dirToTarget, time * time * time, thirdJerk, dirToTarget );
    VectorSubtract( dirToTarget, self->s.pos.trBase, dirToTarget );
    distanceToTarget = VectorLength( dirToTarget );

    if( projectileDistance < distanceToTarget )
      lowMsec = partitionMsec;
    else if( projectileDistance > distanceToTarget )
      highMsec = partitionMsec;
    else if( projectileDistance == distanceToTarget )
      break; // unlikely to happen

    if( Q_fabs( projectileDistance - distanceToTarget ) < Q_fabs( projectileDistance - bestDistanceToTarget ) )
    {
      VectorCopy( dirToTarget, bestDirToTarget );
      bestDistanceToTarget = VectorLength( bestDirToTarget );
    }

    if( i > 50 )
    {
      VectorCopy( bestDirToTarget, dirToTarget );
      break;
    }

    i++;
  }

  VectorNormalize( dirToTarget );
  vectoangles( dirToTarget, self->turretAim );

  //fire at target
  FireWeapon( self );
  G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );
  self->count = level.time + firespeed;
}

/*
================
ATrapper_CheckTarget

Used by ATrapper_Think to check enemies for validity
================
*/
static qboolean ATrapper_CheckTarget( gentity_t *self, gentity_t *target, int range )
{
  vec3_t    distance;
  trace_t   trace;

  if( !target ) // Do we have a target?
    return qfalse;
  if( !target->inuse ) // Does the target still exist?
    return qfalse;
  if( target == self ) // is the target us?
    return qfalse;
  if( !target->client ) // is the target a bot or player?
    return qfalse;
  if( G_NoTarget( target ) ) // is the target cheating?
    return qfalse;
  if( target->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS ) // one of us?
    return qfalse;
  if( target->client->sess.spectatorState != SPECTATOR_NOT ) // is the target alive?
    return qfalse;
  if( target->health <= 0 ) // is the target still alive?
    return qfalse;
  if( target->client->ps.stats[ STAT_STATE ] & SS_BLOBLOCKED ) // locked?
    return qfalse;

  VectorSubtract( target->r.currentOrigin, self->r.currentOrigin, distance );
  if( VectorLength( distance ) > range ) // is the target within range?
    return qfalse;

  //only allow a narrow field of "vision"
  VectorNormalize( distance ); //is now direction of target
  if( DotProduct( distance, self->s.origin2 ) < LOCKBLOB_DOT )
    return qfalse;

  SV_Trace( &trace, self->s.pos.trBase, NULL, NULL, target->s.pos.trBase, self->s.number, MASK_SHOT, TT_AABB );
  if ( trace.contents & CONTENTS_SOLID ) // can we see the target?
    return qfalse;

  return qtrue;
}

/*
================
ATrapper_FindEnemy

Used by ATrapper_Think to locate enemy gentities
================
*/
static void ATrapper_FindEnemy( gentity_t *ent, int range )
{
  gentity_t *target;
  int       i;
  int       start;

  // iterate through entities
  start = rand( ) / ( RAND_MAX / level.num_entities + 1 );
  for( i = start; i < level.num_entities + start; i++ )
  {
    target = g_entities + ( i % level.num_entities );
    //if target is not valid keep searching
    if( !ATrapper_CheckTarget( ent, target, range ) )
      continue;

    //we found a target
    ent->enemy = target;
    return;
  }

  //couldn't find a target
  ent->enemy = NULL;
}

/*
================
ATrapper_Think

think function for Alien Defense
================
*/
void ATrapper_Think( gentity_t *self )
{
  int range =     BG_Buildable( self->s.modelindex )->turretRange;
  int firespeed = BG_Buildable( self->s.modelindex )->turretFireSpeed;

  AGeneric_Think( self );

  if( self->spawned && self->powered )
  {
    //if the current target is not valid find a new one
    if( !ATrapper_CheckTarget( self, self->enemy, range ) )
      ATrapper_FindEnemy( self, range );

    //if a new target cannot be found don't do anything
    if( !self->enemy )
      return;

    //if we are pointing at our target and we can fire shoot it
    if( self->count < level.time )
      ATrapper_FireOnEnemy( self, firespeed );
  }
}




//==================================================================================

/*
================
G_IdlePowerState

Set buildable idle animation to match power state
================
*/
static void G_IdlePowerState( gentity_t *self )
{
  if( self->powered )
  {
    if( self->s.torsoAnim == BANIM_IDLE3 )
      G_SetIdleBuildableAnim( self, BG_Buildable( self->s.modelindex )->idleAnim );
  }
  else
  {
    if( self->s.torsoAnim != BANIM_IDLE3 )
      G_SetIdleBuildableAnim( self, BANIM_IDLE3 );
  }
}




//==================================================================================


/*
================
HSpawn_Disappear

Called when a human spawn is destroyed before it is spawned
think function
================
*/
void HSpawn_Disappear( gentity_t *self )
{
  self->timestamp = level.time;
  G_QueueBuildPoints( self );
  G_RewardAttackers( self );

  G_FreeEntity( self );
}


/*
================
HSpawn_blast

Called when a human spawn explodes
think function
================
*/
void HSpawn_Blast( gentity_t *self )
{
  vec3_t  dir;

  // we don't have a valid direction, so just point straight up
  dir[ 0 ] = dir[ 1 ] = 0;
  dir[ 2 ] = 1;

  self->timestamp = level.time;

  //do some radius damage
  G_RadiusDamage( self->s.pos.trBase, self->r.mins, self->r.maxs,
                  g_entities + self->killedBy, self->splashDamage,
                  self->splashRadius, self, self->splashMethodOfDeath, qtrue );

  // begin freeing build points
  G_QueueBuildPoints( self );
  G_RewardAttackers( self );
  // turn into an explosion
  self->s.eType = ET_EVENTS + EV_HUMAN_BUILDABLE_EXPLOSION;
  self->freeAfterEvent = qtrue;

  G_AddEvent( self, EV_HUMAN_BUILDABLE_EXPLOSION, DirToByte( dir ) );
}


/*
================
HSpawn_die

Called when a human spawn dies
================
*/
void HSpawn_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{
  G_SetBuildableAnim( self, BANIM_DESTROY1, qtrue );
  G_SetIdleBuildableAnim( self, BANIM_DESTROYED );

  self->die = nullDieFunction;
  self->killedBy = attacker - g_entities;
  self->powered = qfalse; //free up power
  self->s.eFlags &= ~EF_FIRING; //prevent any firing effects
  self->methodOfDeath = mod;

  if( self->spawned )
  {
    self->think = HSpawn_Blast;
    if(
      inflictor->s.eType == ET_MOVER &&
      (
        inflictor->s.pos.trType == TR_SINE ||
        inflictor->s.apos.trType == TR_SINE)) {
      self->nextthink = level.time; //blast immediately
    } else {
      self->nextthink = level.time + HUMAN_DETONATION_DELAY;
    }
  }
  else
  {
    self->think = HSpawn_Disappear;
    self->nextthink = level.time; //blast immediately
  }

  G_RemoveBuildableFromStack( self->s.groundEntityNum, self->s.number );
  G_SetBuildableDropper( self->s.number, attacker->s.number );

  G_RemoveRangeMarkerFrom( self );
  G_LogDestruction( self, attacker, mod );
}

/*
================
HSpawn_Think

Think for human spawn
================
*/
void HSpawn_Think( gentity_t *self )
{
  gentity_t *ent;

  // set parentNode
  self->powered = G_FindPower( self, qfalse );

  if( self->spawned )
  {
    //only suicide if at rest
    if( self->s.groundEntityNum != ENTITYNUM_NONE )
    {
      if( ( ent = G_CheckSpawnPoint( self->s.number, self->r.currentOrigin,
              self->s.origin2, self->s.modelindex, NULL ) ) != NULL )
      {
        // If the thing blocking the spawn is a buildable, kill it.
        // If it's part of the map, kill self.
        if( ent->s.eType == ET_BUILDABLE )
        {
          if( ent->health > 0 )
            G_SetBuildableAnim( self, BANIM_SPAWN1, qtrue );

          G_Damage( ent, NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_SUICIDE );
        }
        else if( ent->s.number == ENTITYNUM_WORLD || ent->s.eType == ET_MOVER )
        {
          G_Damage( self, NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_SUICIDE );
          return;
        }

        G_PuntBlocker( self, ent );

        if( ent->s.eType == ET_CORPSE )
          G_FreeEntity( ent ); //quietly remove
      }
      else
        self->spawnBlockTime = 0;
    }
  }

  self->nextthink = level.time + BG_Buildable( self->s.modelindex )->nextthink;

  G_SuffocateTrappedEntities( self );
}




//==================================================================================




/*
================
HRepeater_Die

Called when a repeater dies
================
*/
static void HRepeater_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{
  G_SetBuildableAnim( self, BANIM_DESTROY1, qtrue );
  G_SetIdleBuildableAnim( self, BANIM_DESTROYED );

  self->die = nullDieFunction;
  self->killedBy = attacker - g_entities;
  self->powered = qfalse; //free up power
  self->s.eFlags &= ~EF_FIRING; //prevent any firing effects
  self->methodOfDeath = mod;

  if( self->spawned )
  {
    self->think = HSpawn_Blast;
    if(
      inflictor->s.eType == ET_MOVER &&
      (
        inflictor->s.pos.trType == TR_SINE ||
        inflictor->s.apos.trType == TR_SINE)) {
      self->nextthink = level.time; //blast immediately
    } else {
      self->nextthink = level.time + HUMAN_DETONATION_DELAY;
    }
  }
  else
  {
    self->think = HSpawn_Disappear;
    self->nextthink = level.time; //blast immediately
  }

  G_RemoveBuildableFromStack( self->s.groundEntityNum, self->s.number );
  G_SetBuildableDropper( self->s.number, attacker->s.number );

  G_RemoveRangeMarkerFrom( self );
  G_LogDestruction( self, attacker, mod );

  if( self->usesBuildPointZone )
  {
    buildPointZone_t *zone = &level.buildPointZones[self->buildPointZone];

    zone->active = qfalse;
    self->usesBuildPointZone = qfalse;
  }
}

/*
================
HRepeater_Think

Think for human power repeater
================
*/
void HRepeater_Think( gentity_t *self )
{
  int               i;
  gentity_t         *powerEnt;
  buildPointZone_t  *zone;

  self->powered = G_FindPower( self, qfalse );

  powerEnt = G_InPowerZone( self );
  if( powerEnt != NULL )
  {
    // If the repeater is inside of another power zone then suicide
    // Attribute death to whoever built the reactor if that's a human,
    // which will ensure that it does not queue the BP
    if( powerEnt->builtBy && powerEnt->builtBy->slot >= 0 )
      G_Damage( self, NULL, g_entities + powerEnt->builtBy->slot, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_SUICIDE );
    else
      G_Damage( self, NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_SUICIDE );
    return;
  }

  G_IdlePowerState( self );

  // Initialise the zone once the repeater has spawned
  if( self->spawned && ( !self->usesBuildPointZone || !level.buildPointZones[ self->buildPointZone ].active ) )
  {
    // See if a free zone exists
    for( i = 0; i < g_humanRepeaterMaxZones.integer; i++ )
    {
      zone = &level.buildPointZones[ i ];

      if( !zone->active )
      {
        // Initialise the BP queue with no BP queued
        zone->active = qtrue;

        self->buildPointZone = zone - level.buildPointZones;
        self->usesBuildPointZone = qtrue;

        break;
      }
    }
  }

  self->nextthink = level.time + ( POWER_REFRESH_TIME / ( IS_WARMUP ? 2 : 1 ) );

  G_SuffocateTrappedEntities( self );
}

/*
================
HRepeater_Activate

Use for human power repeater
================
*/
qboolean HRepeater_Activate( gentity_t *self, gentity_t *activator )
{
  if( self->occupation.other && self->occupation.other->client )
    G_GiveClientMaxAmmo( self->occupation.other, qtrue );

  return qfalse;
}

/*
================
HRepeater_CanActivate

Custom canActivate for human power repeater
================
*/
qboolean HRepeater_CanActivate( gentity_t *self, gclient_t *client )
{
  if( !BG_Weapon( client->ps.weapon )->usesEnergy ||
      BG_Weapon( client->ps.weapon )->infiniteAmmo )
    return qfalse;
  else
    return qtrue;
}

/*
================
HReactor_Think

Think function for Human Reactor
================
*/
void HReactor_Think( gentity_t *self )
{
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { REACTOR_ATTACK_RANGE,
                      REACTOR_ATTACK_RANGE,
                      REACTOR_ATTACK_RANGE };
  vec3_t    dccrange = { REACTOR_ATTACK_DCC_RANGE,
                         REACTOR_ATTACK_DCC_RANGE,
                         REACTOR_ATTACK_DCC_RANGE };
  vec3_t    mins, maxs;
  int       i, num;
  gentity_t *enemy, *tent;

  if( self->dcc )
  {
    VectorAdd( self->r.currentOrigin, dccrange, maxs );
    VectorSubtract( self->r.currentOrigin, dccrange, mins );
  }
  else
  {
    VectorAdd( self->r.currentOrigin, range, maxs );
    VectorSubtract( self->r.currentOrigin, range, mins );
  }

  if( self->spawned && ( self->health > 0 ) )
  {
    qboolean fired = qfalse;

    // Creates a tesla trail for every target
    num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
    for( i = 0; i < num; i++ )
    {
      enemy = &g_entities[ entityList[ i ] ];
      if( ( !enemy->client ||
            enemy->client->ps.stats[ STAT_TEAM ] != TEAM_ALIENS ||
            (enemy->r.contents & CONTENTS_ASTRAL_NOCLIP))
          && enemy->s.eType != ET_TELEPORTAL )
        continue;
      if( G_NoTarget( enemy ) )
        continue;

      tent = G_TempEntity( enemy->s.pos.trBase, EV_TESLATRAIL );
      tent->s.misc = self->s.number; //src
      tent->s.clientNum = enemy->s.number; //dest
      VectorCopy( self->s.pos.trBase, tent->s.origin2 );
      fired = qtrue;
    }

    // Actual damage is done by radius
    if( fired )
    {
      self->timestamp = level.time;
      if( self->dcc )
        G_SelectiveRadiusDamage( self->s.pos.trBase,
                                 self->r.mins, self->r.maxs,
                                 self, REACTOR_ATTACK_DCC_DAMAGE,
                                 REACTOR_ATTACK_DCC_RANGE, self,
                                 MOD_REACTOR, TEAM_HUMANS, qtrue );
      else
        G_SelectiveRadiusDamage( self->s.pos.trBase,
                                 self->r.mins, self->r.maxs,
                                 self, REACTOR_ATTACK_DAMAGE,
                                 REACTOR_ATTACK_RANGE, self,
                                 MOD_REACTOR, TEAM_HUMANS, qtrue );
    }
  }

  if( self->dcc )
    self->nextthink = level.time + REACTOR_ATTACK_DCC_REPEAT;
  else
    self->nextthink = level.time + REACTOR_ATTACK_REPEAT;

  G_SuffocateTrappedEntities( self );
}

//==================================================================================



/*
================
HArmoury_Activate

Called when a human activates an Armoury
================
*/
qboolean HArmoury_Activate( gentity_t *self, gentity_t *activator )
{
  G_TriggerMenu( activator->client->ps.clientNum, MN_H_ARMOURY );

  return qfalse;
}

/*
================
HArmoury_Think

Think for armoury
================
*/
void HArmoury_Think( gentity_t *self )
{
  //make sure we have power
  self->nextthink = level.time + ( POWER_REFRESH_TIME / ( IS_WARMUP ? 2 : 1 ) );

  G_SuffocateTrappedEntities( self );

  self->powered = G_FindPower( self, qfalse );
}




//==================================================================================





/*
================
HDCC_Think

Think for dcc
================
*/
void HDCC_Think( gentity_t *self )
{
  //make sure we have power
  self->nextthink = level.time + ( POWER_REFRESH_TIME / ( IS_WARMUP ? 2 : 1 ) );

  G_SuffocateTrappedEntities( self );

  self->powered = G_FindPower( self, qfalse );
}




//==================================================================================




/*
================
HMedistat_Die

Die function for Human Medistation
================
*/
void HMedistat_Die( gentity_t *self, gentity_t *inflictor,
                    gentity_t *attacker, int damage, int mod )
{
  //clear target's healing flag
  if( self->enemy && self->enemy->client )
    self->enemy->client->ps.stats[ STAT_STATE ] &= ~SS_HEALING_ACTIVE;

  HSpawn_Die( self, inflictor, attacker, damage, mod );
}

/*
================
HMedistat_Think

think function for Human Medistation
================
*/
void HMedistat_Think( gentity_t *self )
{
  int       entityList[ MAX_GENTITIES ];
  vec3_t    mins, maxs;
  int       i, num;
  gentity_t *player;
  qboolean  occupied = qfalse;

  self->nextthink = level.time + BG_Buildable( self->s.modelindex )->nextthink;

  G_SuffocateTrappedEntities( self );

  self->powered = G_FindPower( self, qfalse );
  G_IdlePowerState( self );

  //clear target's healing flag
  if( self->enemy && self->enemy->client )
    self->enemy->client->ps.stats[ STAT_STATE ] &= ~SS_HEALING_ACTIVE;

  //make sure we have power
  if( !self->powered )
  {
    if( self->active )
    {
      self->active = qfalse;
      self->enemy = NULL;
    }

    self->nextthink = level.time + POWER_REFRESH_TIME;
    return;
  }

  if( self->spawned )
  {
    VectorAdd( self->r.currentOrigin, self->r.maxs, maxs );
    VectorAdd( self->r.currentOrigin, self->r.mins, mins );

    mins[ 2 ] += fabs( self->r.mins[ 2 ] ) + self->r.maxs[ 2 ];
    maxs[ 2 ] += 60; //player height

    //if active use the healing idle
    if( self->active )
      G_SetIdleBuildableAnim( self, BANIM_IDLE2 );

    //check if a previous occupier is still here
    num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
    for( i = 0; i < num; i++ )
    {
      player = &g_entities[ entityList[ i ] ];
      int maxHealth;

      if( G_NoTarget( player ) )
        continue; // notarget cancels even beneficial effects?

      if( !player->client )
        continue;

      maxHealth = BG_Class( player->client->ps.stats[ STAT_CLASS ] )->health;

      //remove poison from everyone, not just the healed player
      if(player->client->ps.stats[ STAT_STATE ] & SS_POISONED )
        player->client->ps.stats[ STAT_STATE ] &= ~SS_POISONED;

      //give a medkit to all fully healed human players
      if( player->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
          PM_Alive( player->client->ps.pm_type ) &&
          player->health >= maxHealth &&
          !BG_InventoryContainsUpgrade( UP_MEDKIT, player->client->ps.stats ) )
        BG_AddUpgradeToInventory( UP_MEDKIT, player->client->ps.stats );

      if( self->enemy == player &&
          player->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
          player->health < maxHealth &&
          PM_Alive( player->client->ps.pm_type ) )
      {
        occupied = qtrue;
        player->client->ps.stats[ STAT_STATE ] |= SS_HEALING_ACTIVE;
      }
    }

    if( !occupied )
    {
      self->enemy = NULL;

      //look for something to heal
      for( i = 0; i < num; i++ )
      {
        player = &g_entities[ entityList[ i ] ];

        if( G_NoTarget( player ) )
          continue; // notarget cancels even beneficial effects?

        if( !player->client )
          continue;

        if( player->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
        {
          const int maxHealth = BG_Class( player->client->ps.stats[ STAT_CLASS ] )->health;

          if( ( player->health < maxHealth ||
                player->client->ps.stats[ STAT_STAMINA ] < STAMINA_MAX ) &&
              PM_Alive( player->client->ps.pm_type ) )
          {
            self->enemy = player;

            //start the heal anim
            if( !self->active )
            {
              G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );
              self->active = qtrue;
              player->client->ps.stats[ STAT_STATE ] |= SS_HEALING_ACTIVE;
            }
          }

          if( player->health >= maxHealth &&
              !BG_InventoryContainsUpgrade( UP_MEDKIT, player->client->ps.stats ) )
            BG_AddUpgradeToInventory( UP_MEDKIT, player->client->ps.stats );
        }
      }
    }

    //nothing left to heal so go back to idling
    if( !self->enemy && self->active )
    {
      G_SetBuildableAnim( self, BANIM_CONSTRUCT2, qtrue );
      G_SetIdleBuildableAnim( self, BANIM_IDLE1 );

      self->active = qfalse;
    }
    else if( self->enemy && self->enemy->client ) //heal!
    {
      gentity_t *player2 = self->enemy;
      gclient_t *client = player2->client;
      const int maxHealth = BG_Class( client->ps.stats[ STAT_CLASS ] )->health;

      if( client->ps.stats[ STAT_STAMINA ] <  STAMINA_MAX )
        client->ps.stats[ STAT_STAMINA ] += STAMINA_MEDISTAT_RESTORE;

      if( client->ps.stats[ STAT_STAMINA ] > STAMINA_MAX )
        client->ps.stats[ STAT_STAMINA ] = STAMINA_MAX;

      if( player2->health < maxHealth )
      {
        player2->health += BG_HP2SU( 1 );
        if( player2->health > maxHealth )
          player2->health = maxHealth;
        client->ps.misc[ MISC_HEALTH ] = player2->health;
        client->pers.infoChangeTime = level.time;
      }

      //if they're completely healed and have full stamina, give them a medkit
      if( player2->health >= maxHealth &&
          player2->client->ps.stats[ STAT_STAMINA ] >= STAMINA_MAX )
      {
        if( !BG_InventoryContainsUpgrade( UP_MEDKIT, self->enemy->client->ps.stats ) )
          BG_AddUpgradeToInventory( UP_MEDKIT, self->enemy->client->ps.stats );
      }
    }
  }
}




//==================================================================================


typedef struct targetPoint_s
{
  bboxPoint_t bboxPoint;
  vec3_t      direction;
  float       distance;
} targetPoint_t;

static int HMGTurret_ComparTargetpoints( const void *a, const void *b )
{
  targetPoint_t *pointA = (targetPoint_t *)a;
  targetPoint_t *pointB = (targetPoint_t *)b;

  Com_Assert( pointA &&
              "HMGTurret_ComparTargetpoints: pointA is NULL" );
  Com_Assert( pointB &&
              "HMGTurret_ComparTargetpoints: pointB is NULL" );

  //the origin gets the highest precedence
  if( pointA->bboxPoint.num == BBXP_ORIGIN )
    return -1;
  else if( pointB->bboxPoint.num == BBXP_ORIGIN )
    return 1;

  //faces have higher precedence than verticies
  if( pointA->bboxPoint.num < BBXP_VERTEX1 &&
      pointB->bboxPoint.num >= BBXP_VERTEX1 )
    return -1;
  else if( pointB->bboxPoint.num < BBXP_VERTEX1 &&
            pointA->bboxPoint.num >= BBXP_VERTEX1 )
    return 1;

  return( pointA->distance - pointB->distance );
}

static qboolean HMGTurret_TargetPointIsVisible( gentity_t *self,
                                                gentity_t *target,
                                                targetPoint_t *targetPoint )
{
  trace_t       tr;
  vec3_t        end;

  Com_Assert( self &&
              "HMGTurret_TargetPointIsVisible: self is NULL" );
  Com_Assert( target &&
              "HMGTurret_TargetPointIsVisible: target is NULL" );
  Com_Assert( targetPoint &&
              "HMGTurret_TargetPointIsVisible: targetPoint is NULL" );

  //check that the point is within the angular range
  if( DotProduct( self->s.origin2, targetPoint->direction ) <
      -sin( DEG2RAD( MGTURRET_VERTICALCAP ) ) )
    return qfalse;

  VectorMA( self->s.pos.trBase, MGTURRET_RANGE, targetPoint->direction, end );
  SV_Trace( &tr, self->s.pos.trBase, NULL, NULL, end,
              self->s.number, MASK_SHOT, TT_AABB );

  return ( tr.entityNum == target - g_entities );

}

static qboolean HMGTurret_FindTrackPoint( gentity_t *self,
                                          gentity_t *target,
                                          bboxPoint_t *trackPoint )
{
  targetPoint_t targetPoints[ NUM_BBOX_POINTS ];
  vec3_t        forward;
  int           i;

  Com_Assert( self &&
              "HMGTurret_FindTrackPoint: self is NULL" );
  Com_Assert( target &&
              "HMGTurret_FindTrackPoint: target is NULL" );
  Com_Assert( trackPoint &&
              "HMGTurret_FindTrackPoint: trackPoint is NULL" );

  AngleVectors( self->s.angles2, forward, NULL, NULL );

  //evaluate the target points
  for( i = 0; i < NUM_BBOX_POINTS; i++ )
  {
    vec3_t        projection;
    targetPoint_t *targetPoint = &targetPoints[ i ];

    // find the point
    targetPoint->bboxPoint.num = i;
    BG_EvaluateBBOXPoint( &targetPoint->bboxPoint,
                          target->s.pos.trBase,
                          target->r.mins, target->r.maxs);

    // find the direction and projected distance to the point
    VectorSubtract( targetPoint->bboxPoint.point,
                    self->s.pos.trBase,
                    targetPoint->direction );
    ProjectPointOnPlane( projection,
                         targetPoint->direction,
                         forward );
    targetPoint->distance = VectorNormalize( projection );
    VectorNormalize( targetPoint->direction );

    if( !self->dcc && targetPoint->bboxPoint.num == BBXP_ORIGIN )
    {
      //only need to check the origin in this case
      break;
    }
  }

  // check if only the origin needs to be tracked
  if( !self->dcc )
  {
    targetPoint_t *targetPoint = &targetPoints[ BBXP_ORIGIN ];

    Com_Assert( targetPoint->bboxPoint.num == BBXP_ORIGIN &&
                "HMGTurret_FindTrackPoint: target point bboxPointNum mismatch with origin" );

    if( HMGTurret_TargetPointIsVisible( self, target, targetPoint ) )
    {
      VectorCopy( targetPoint->bboxPoint.point, trackPoint->point );
      trackPoint->num = targetPoint->bboxPoint.num;
      return qtrue;
    } else
      return qfalse;
  }

  //sort the target points based on priority and the projected distance
  qsort( targetPoints, NUM_BBOX_POINTS, sizeof( targetPoints[ 0 ] ),
         HMGTurret_ComparTargetpoints );

  // check the bbox points for visibility
  for( i = 0; i < NUM_BBOX_POINTS; i++ )
  {
    targetPoint_t *targetPoint = &targetPoints[ i ];

    //only check the origin the centers of the faces
    if( targetPoint->bboxPoint.num > BBXP_MIDFACE6 )
      continue;

    if( HMGTurret_TargetPointIsVisible( self, target, targetPoint ) )
    {
      VectorCopy( targetPoint->bboxPoint.point, trackPoint->point );
      trackPoint->num = targetPoint->bboxPoint.num;
      return qtrue;
    }
  }

  return qfalse;
}

/*
================
HMGTurret_NumOfTargeting

Checks the validity of the members of an entity's targeted queue, then
returns the length of the queue
================
*/
size_t HMGTurret_NumOfTargeting(gentity_t *ent) {
  bglink_t *list;

  Com_Assert(ent && "HMGTurret_CheckTargetedQueue: ent is NULL");

  list = ent->targeted.head;
  while(list) {
    bglink_t *next = list->next;
    gentity_t *targeted = (gentity_t *)list->data;

    if(
      !targeted->inuse ||
      targeted->s.eType != ET_BUILDABLE ||
      targeted->enemy != ent ||
      !targeted->powered) {
      BG_List_Delete_Link(list);
    }
    list = next;
  }

  return BG_List_Get_Length(&ent->targeted);
}

/*
================
HMGTurret_CheckTarget

Used by HMGTurret_Think to check enemies for validity
================
*/
static qboolean HMGTurret_CheckTarget( gentity_t *self, gentity_t *target,
                                       qboolean los_check, qboolean ignorePainted )
{
  bboxPoint_t trackPoint;

  Com_Assert( self &&
              "HMGTurret_CheckTarget: self is NULL" );

  if( !target || target->health <= 0 || !target->client ||
      target->client->pers.teamSelection != TEAM_ALIENS )
    return qfalse;

  //some turret has already selected this target
  if(self->dcc && !ignorePainted) {
    size_t num_of_targeting = HMGTurret_NumOfTargeting(target);

    if(
      BG_ClassAllowedInStage(PCL_ALIEN_LEVEL4, g_alienStage.integer, IS_WARMUP) &&
      (target->client && target->client->pers.classSelection != PCL_ALIEN_LEVEL4)) {
      //check to see if a rant can be shot instead
      return qfalse;
    }

    //check to see if another potential target is an agressor
    if(!G_IsRecentAgressor(target)) {
      return qfalse;
    }

    if(
      num_of_targeting > 0 &&
      !(BG_List_Find(&target->targeted, self) && num_of_targeting == 1)) {
      return qfalse;
    }
  }

  if( G_NoTarget( target ) )
    return qfalse;

  if( !los_check )
  {
    self->trackedEnemyPointNum = BBXP_ORIGIN;
    return qtrue;
  }

  // Accept target if we can line-trace to it
  if( HMGTurret_FindTrackPoint( self,  target, &trackPoint ) )
  {
    self->trackedEnemyPointNum = trackPoint.num;
    return qtrue;
  }

  return qfalse;
}


/*
================
HMGTurret_TrackEnemy

Used by HMGTurret_Think to track enemy location
================
*/
static qboolean HMGTurret_TrackEnemy( gentity_t *self )
{
  vec3_t  dirToTarget, dttAdjusted, angleToTarget, angularDiff, xNormal;
  vec3_t  refNormal = { 0.0f, 0.0f, 1.0f };
  float   temp, rotAngle;
  bboxPoint_t trackPoint;
  float   angularSpeed;

  Com_Assert( self &&
              "HMGTurret_TrackEnemy: self is NULL" );
  Com_Assert( self->enemy &&
              "HMGTurret_TrackEnemy: enemy is NULL" );

  if( !self->dcc )
    trackPoint.num = BBXP_ORIGIN;
  else
    trackPoint.num = self->trackedEnemyPointNum;
  BG_EvaluateBBOXPoint( &trackPoint, 
                        self->enemy->s.pos.trBase,
                        self->enemy->r.mins,
                        self->enemy->r.maxs );

  // Move slow if no dcc or grabbed
  if( G_IsDCCBuilt( ) && self->lev1Grabbed )
  {
    angularSpeed = ( MGTURRET_DCC_ANGULARSPEED - MGTURRET_ANGULARSPEED ) + MGTURRET_GRAB_ANGULARSPEED;
  }
  else if( self->lev1Grabbed )
  {
    angularSpeed = MGTURRET_GRAB_ANGULARSPEED;
  }
  else if( G_IsDCCBuilt( ) )
  {
    angularSpeed = MGTURRET_DCC_ANGULARSPEED;
  }
  else
  {
    angularSpeed = MGTURRET_ANGULARSPEED;
  }

  VectorSubtract( trackPoint.point, self->s.pos.trBase, dirToTarget );
  VectorNormalize( dirToTarget );

  CrossProduct( self->s.origin2, refNormal, xNormal );
  VectorNormalize( xNormal );
  rotAngle = RAD2DEG( acos( DotProduct( self->s.origin2, refNormal ) ) );
  RotatePointAroundVector( dttAdjusted, xNormal, dirToTarget, rotAngle );

  vectoangles( dttAdjusted, angleToTarget );

  angularDiff[ PITCH ] = AngleSubtract( self->s.angles2[ PITCH ], angleToTarget[ PITCH ] );
  angularDiff[ YAW ] = AngleSubtract( self->s.angles2[ YAW ], angleToTarget[ YAW ] );

  //if not pointing at our target then move accordingly
  if( angularDiff[ PITCH ] < 0 && angularDiff[ PITCH ] < ( -angularSpeed ) )
    self->s.angles2[ PITCH ] += angularSpeed;
  else if( angularDiff[ PITCH ] > 0 && angularDiff[ PITCH ] > angularSpeed )
    self->s.angles2[ PITCH ] -= angularSpeed;
  else
    self->s.angles2[ PITCH ] = angleToTarget[ PITCH ];

  //disallow vertical movement past a certain limit
  temp = fabs( self->s.angles2[ PITCH ] );
  if( temp > 180 )
    temp -= 360;

  if( temp < -MGTURRET_VERTICALCAP )
    self->s.angles2[ PITCH ] = (-360) + MGTURRET_VERTICALCAP;

  //if not pointing at our target then move accordingly
  if( angularDiff[ YAW ] < 0 && angularDiff[ YAW ] < ( -angularSpeed ) )
    self->s.angles2[ YAW ] += angularSpeed;
  else if( angularDiff[ YAW ] > 0 && angularDiff[ YAW ] > angularSpeed )
    self->s.angles2[ YAW ] -= angularSpeed;
  else
    self->s.angles2[ YAW ] = angleToTarget[ YAW ];

  AngleVectors( self->s.angles2, dttAdjusted, NULL, NULL );
  RotatePointAroundVector( dirToTarget, xNormal, dttAdjusted, -rotAngle );
  vectoangles( dirToTarget, self->turretAim );

  //fire if target is within accuracy
  if( ( fabs( angularDiff[ YAW ] ) - MGTURRET_ANGULARSPEED <=
           MGTURRET_ACCURACY_TO_FIRE ) &&
      ( fabs( angularDiff[ PITCH ] ) - MGTURRET_ANGULARSPEED <=
           MGTURRET_ACCURACY_TO_FIRE ) )
  {
    return qtrue;
  }
  else if( self->dcc && self->active )
  {
    vec3_t  forward;
    vec3_t  end;
    trace_t tr;

    // check to see if the enemy is still in the line of fire
    AngleVectors( self->s.angles2, forward, NULL, NULL );
    VectorNormalize( forward );
    VectorMA( self->s.pos.trBase, MGTURRET_RANGE, forward, end );
    SV_Trace( &tr, self->s.pos.trBase, NULL, NULL, end,
                self->s.number, MASK_SHOT, TT_AABB );

    return ( tr.entityNum == self->enemy - g_entities );
  }

  return qfalse;
}


/*
================
HMGTurret_FindEnemy

Used by HMGTurret_Think to locate enemy gentities
================
*/
static void HMGTurret_FindEnemy( gentity_t *self )
{
  int         entityList[ MAX_GENTITIES ];
  vec3_t      range;
  vec3_t      mins, maxs;
  int         i, num;
  gentity_t   *target;
  gentity_t   *prev_enemy;
  int         start;

  Com_Assert( self &&
              "HMGTurret_FindEnemy: self is NULL" );

  prev_enemy = self->enemy;
  self->enemy = NULL;

  // Look for targets in a box around the turret
  VectorSet( range, MGTURRET_RANGE, MGTURRET_RANGE, MGTURRET_RANGE );
  VectorAdd( self->r.currentOrigin, range, maxs );
  VectorSubtract( self->r.currentOrigin, range, mins );
  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );

  if( num == 0 )
    return;

  if( self->dcc ) {
    size_t   min_targeting = MAX_GENTITIES;
    class_t  targeted_class = PCL_NONE;
    qboolean targeted_agressor = qfalse;

    //factor in targets being tracked by multiple turrets

    //first check the previous enemy
    if( HMGTurret_CheckTarget( self, prev_enemy, qtrue, qtrue ) ) {
      self->enemy = prev_enemy;
      min_targeting = HMGTurret_NumOfTargeting(prev_enemy);
      targeted_agressor = G_IsRecentAgressor(prev_enemy);
      if(prev_enemy->client) {
        targeted_class = prev_enemy->client->pers.classSelection;
      } else {
        targeted_class = PCL_NONE;
      }
    }

    //check all other area entities
    start = rand( ) / ( RAND_MAX / num + 1 );
    for( i = start; i < num + start ; i++ ) {
      size_t         num_of_targeting;
      bboxPointNum_t trackedEnemyPointNumBackup;

      target = &g_entities[ entityList[ i % num ] ];

      if(target == prev_enemy) {
        //already checked
        continue;
      }

      if( targeted_class == PCL_ALIEN_LEVEL4) {
        if(
          target->client &&
          target->client->pers.classSelection != PCL_ALIEN_LEVEL4) {
          //give preference to shooting tyrants
          continue;
        }

        if(targeted_agressor && !G_IsRecentAgressor(target)) {
          //give agressor tyrants top priority
          continue;
        }
      }

      if(targeted_agressor && !G_IsRecentAgressor(target)) {
        //give agressor enemies higher priority
        continue;
      }

      num_of_targeting = HMGTurret_NumOfTargeting(target);

      if(
        !(
          targeted_class != PCL_ALIEN_LEVEL4 &&
          (
            target->client &&
            target->client->pers.classSelection == PCL_ALIEN_LEVEL4)) &&
        !(!targeted_agressor && G_IsRecentAgressor(target)) &&
        num_of_targeting >= min_targeting) {
        continue;
      }

      trackedEnemyPointNumBackup = self->trackedEnemyPointNum;
      if( !HMGTurret_CheckTarget( self, target, qtrue, qtrue ) ) {
        self->trackedEnemyPointNum = trackedEnemyPointNumBackup;
        continue;
      }

      self->enemy = target;
      min_targeting = num_of_targeting;
      if(target->client) {
        targeted_class = target->client->pers.classSelection;
      } else {
        targeted_class = PCL_NONE;
      }
    }
  } else {
    start = rand( ) / ( RAND_MAX / num + 1 );
    for( i = start; i < num + start ; i++ ) {
      target = &g_entities[ entityList[ i % num ] ];
      if( !HMGTurret_CheckTarget( self, target, qtrue, qtrue ) ) {
        continue;
      }

      self->enemy = target;
      return;
    }
  }
}

/*
================
HMGTurret_State

Raise or lower MG turret towards desired state
================
*/
enum {
  MGT_STATE_INACTIVE,
  MGT_STATE_DROP,
  MGT_STATE_RISE,
  MGT_STATE_ACTIVE
};

static qboolean HMGTurret_State( gentity_t *self, int state )
{
  float angle;

  Com_Assert( self &&
              "HMGTurret_State: self is NULL" );

  if( self->waterlevel == state )
    return qfalse;

  angle = AngleNormalize180( self->s.angles2[ PITCH ] );

  if( state == MGT_STATE_INACTIVE )
  {
    if( angle < MGTURRET_VERTICALCAP )
    {
      if( self->waterlevel != MGT_STATE_DROP )
      {
        self->speed = 0.25f;
        self->waterlevel = MGT_STATE_DROP;
      }
      else
        self->speed *= 1.25f;

      self->s.angles2[ PITCH ] =
        MIN( MGTURRET_VERTICALCAP, angle + self->speed );
      return qtrue;
    }
    else
      self->waterlevel = MGT_STATE_INACTIVE;
  }
  else if( state == MGT_STATE_ACTIVE )
  {
    if( !self->enemy && angle > 0.0f )
    {
      self->waterlevel = MGT_STATE_RISE;
      self->s.angles2[ PITCH ] =
        MAX( 0.0f, angle - MGTURRET_ANGULARSPEED * 0.5f );
    }
    else
      self->waterlevel = MGT_STATE_ACTIVE;
  }

  return qfalse;
}

/*
================
HMGTurret_Think

Think function for MG turret
================
*/
void HMGTurret_Think( gentity_t *self )
{
  gentity_t * prev_enemy;

  self->nextthink = level.time +
                    BG_Buildable( self->s.modelindex )->nextthink;

  G_SuffocateTrappedEntities( self );

  // Turn off client side muzzle flashes
  self->s.eFlags &= ~EF_FIRING;

  self->powered = G_FindPower( self, qfalse );
  G_IdlePowerState( self );

  // If not powered or spawned don't do anything
  if( !self->powered )
  {
    // if power loss drop turret
    if( self->spawned &&
        HMGTurret_State( self, MGT_STATE_INACTIVE ) )
      return;

    self->nextthink = level.time + POWER_REFRESH_TIME;
    return;
  }
  if( !self->spawned )
    return;
 
  prev_enemy = self->enemy;

   // If the current target is not valid find a new enemy
  if( !HMGTurret_CheckTarget( self, self->enemy, qtrue, qfalse ) )
  {
    if( self->enemy && BG_List_Find(&self->enemy->targeted, self) ) {
      BG_List_Remove_All(&self->enemy->targeted, self);
    }
    HMGTurret_FindEnemy( self );
  }

  if(self->enemy != prev_enemy) {
    self->active = qfalse;
    self->turretSpinupTime = -1;
  }
  
  // if newly powered raise turret
  HMGTurret_State( self, MGT_STATE_ACTIVE );
  if( !self->enemy )
    return;

  if(BG_List_Find(&self->enemy->targeted, self) == NULL) {
    BG_List_Push_Head(&self->enemy->targeted, self);
  }

  // Track until we can hit the target
  if( !HMGTurret_TrackEnemy( self ) )
  {
    self->active = qfalse;
    self->turretSpinupTime = -1;
    return;
  }

  // Update spin state
  if( !self->active && self->timestamp < level.time )
  {
    self->active = qtrue;

    if( G_IsDCCBuilt( ) )
    {
     self->turretSpinupTime = level.time + MGTURRET_DCC_SPINUP_TIME;
    }
    else
    {
      self->turretSpinupTime = level.time + MGTURRET_SPINUP_TIME;
    }

    G_AddEvent( self, EV_MGTURRET_SPINUP, 0 );
  }

  // Not firing or haven't spun up yet
  if( !self->active || self->turretSpinupTime > level.time )
    return;

  // Fire repeat delay
  if( self->timestamp > level.time )
    return;

  FireWeapon( self );
  self->s.eFlags |= EF_FIRING;
  self->timestamp = level.time + BG_Buildable( self->s.modelindex )->turretFireSpeed;
  G_AddEvent( self, EV_FIRE_WEAPON, 0 );
  G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );
}




//==================================================================================




/*
================
HTeslaGen_Think

Think function for Tesla Generator
================
*/
void HTeslaGen_Think( gentity_t *self )
{
  self->nextthink = level.time + BG_Buildable( self->s.modelindex )->nextthink;

  G_SuffocateTrappedEntities( self );

  self->powered = G_FindPower( self, qfalse );

  if( !G_IsDCCBuilt( ) )
    self->powered = qfalse;

  G_IdlePowerState( self );

  //if not powered don't do anything and check again for power next think
  if( !self->powered )
  {
    self->s.eFlags &= ~EF_FIRING;
    self->nextthink = level.time + POWER_REFRESH_TIME;
    return;
  }

  if( self->spawned && self->timestamp < level.time )
  {
    vec3_t origin, range, mins, maxs;
    int entityList[ MAX_GENTITIES ], i, num;

    // Communicates firing state to client
    self->s.eFlags &= ~EF_FIRING;

    // Move the muzzle from the entity origin up a bit to fire over turrets
    VectorMA( self->r.currentOrigin, self->r.maxs[ 2 ], self->s.origin2, origin );

    VectorSet( range, TESLAGEN_RANGE, TESLAGEN_RANGE, TESLAGEN_RANGE );
    VectorAdd( origin, range, maxs );
    VectorSubtract( origin, range, mins );

    // Attack nearby Aliens and portals
    num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
    for( i = 0; i < num; i++ )
    {
      self->enemy = &g_entities[ entityList[ i ] ];

      if( G_NoTarget( self->enemy ) )
        continue;

      if(self->enemy->r.contents & CONTENTS_ASTRAL_NOCLIP) {
        continue;
      }

      if( ( ( self->enemy->client &&
              self->enemy->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS ) ||
            self->enemy->s.eType == ET_TELEPORTAL ) &&
          self->enemy->health > 0 &&
          Distance( origin, self->enemy->s.pos.trBase ) <= TESLAGEN_RANGE )
        FireWeapon( self );
    }
    self->enemy = NULL;

    if( self->s.eFlags & EF_FIRING )
    {
      G_AddEvent( self, EV_FIRE_WEAPON, 0 );

      //doesn't really need an anim
      //G_SetBuildableAnim( self, BANIM_ATTACK1, qfalse );

      self->timestamp = level.time + TESLAGEN_REPEAT;
    }
  }
}




//==================================================================================




/*
============
G_QueueValue
============
*/

static int G_QueueValue( gentity_t *self )
{
  int       i;
  int       damageTotal = 0;
  int       queuePoints;
  double    queueFraction = 0;

  if(
    G_TimeTilSuddenDeath() <= 0 &&
    G_SD_Mode( ) == SDMODE_SELECTIVE) {

    //don't queue buildables that are not allowed to be replaced
    if(!level.sudden_death_replacable[self->s.modelindex]) {
      return 0;
    }

    //determine if there is another built buildable of this type
    if(!G_BuildableIsUnique(self)) {
      return 0;
    }
  }

  for( i = 0; i < level.maxclients; i++ )
  {
    gentity_t *player = g_entities + i;

    damageTotal += self->credits[ i ];

    if( self->buildableTeam != player->client->pers.teamSelection )
      queueFraction += (double) self->credits[ i ];
  }

  if( damageTotal > 0 )
    queueFraction = queueFraction / (double) damageTotal;
  else // all damage was done by nonclients, so queue everything
    queueFraction = 1.0;

  queuePoints = (int) ( queueFraction * (double) BG_Buildable( self->s.modelindex )->buildPoints );
  return queuePoints;
}

/*
============
G_QueueBuildPoints
============
*/
void G_QueueBuildPoints( gentity_t *self )
{
  int       queuePoints;

  // dont queue bp in warmup
  queuePoints = IS_WARMUP ? 0 : G_QueueValue( self );

  if( !queuePoints )
    return;

  switch( self->buildableTeam )
  {
    default:
    case TEAM_NONE:
      return;

    case TEAM_ALIENS:
      if( !level.alienBuildPointQueue )
        level.alienNextQueueTime = level.time + g_alienBuildQueueTime.integer;

      level.alienBuildPointQueue += queuePoints;
      break;

    case TEAM_HUMANS:
      if( !level.humanBuildPointQueue )
          level.humanNextQueueTime = level.time + g_humanBuildQueueTime.integer;

      level.humanBuildPointQueue += queuePoints;
      break;

  }
}

/*
============
G_NextQueueTime
============
*/
int G_NextQueueTime( int queuedBP, int totalBP, int queueBaseRate )
{
  float fractionQueued;

  if( totalBP == 0 )
    return 0;

  fractionQueued = queuedBP / (float)totalBP;
  return ( 1.0f - fractionQueued ) * queueBaseRate;
}

/*
============
G_BuildableTouchTriggers

Find all trigger entities that a buildable touches.
============
*/
void G_BuildableTouchTriggers( gentity_t *ent )
{
  int       i, num;
  int       touch[ MAX_GENTITIES ];
  gentity_t *hit;
  trace_t   trace;
  vec3_t    mins, maxs;
  vec3_t    bmins, bmaxs;
  static    vec3_t range = { 10, 10, 10 };

  // dead buildables don't activate triggers!
  if( ent->health <= 0 )
    return;

  BG_BuildableBoundingBox( ent->s.modelindex, bmins, bmaxs );

  VectorAdd( ent->r.currentOrigin, bmins, mins );
  VectorAdd( ent->r.currentOrigin, bmaxs, maxs );

  VectorSubtract( mins, range, mins );
  VectorAdd( maxs, range, maxs );

  num = SV_AreaEntities( mins, maxs, touch, MAX_GENTITIES );

  VectorAdd( ent->r.currentOrigin, bmins, mins );
  VectorAdd( ent->r.currentOrigin, bmaxs, maxs );

  for( i = 0; i < num; i++ )
  {
    hit = &g_entities[ touch[ i ] ];

    if( !hit->touch )
      continue;

    if( !( hit->r.contents & CONTENTS_TRIGGER ) )
      continue;

    //ignore buildables not yet spawned
    if( !ent->spawned )
      continue;

    if( !SV_EntityContact( mins, maxs, hit, TT_AABB ) )
      continue;

    memset( &trace, 0, sizeof( trace ) );

    if(hit->touch) {
      if(g_debugAMP.integer) {
        char *s = va("touched by #%i (a %s)", (int)( ent-g_entities),
                      BG_Buildable(ent->s.modelindex )->humanName);
        G_LoggedActivation(hit, ent, NULL, &trace, s, LOG_ACT_TOUCH);
      } else {
        hit->touch(hit, ent, &trace);
      }
    }
  }
}

/*
===============
G_FindBuildableInStack
===============
*/
qboolean G_FindBuildableInStack( int groundBuildableNum, int stackedBuildableNum, int *index )
{
  if( g_entities[ groundBuildableNum ].s.eType != ET_BUILDABLE ||
      g_entities[ stackedBuildableNum ].s.eType != ET_BUILDABLE )
    return qfalse;

  for( *index = 0; *index < g_entities[ groundBuildableNum ].numOfStackedBuildables; *index += 1 )
  {
    if( g_entities[ groundBuildableNum ].buildableStack[ *index ] == stackedBuildableNum )
      return qtrue;
  }

  return qfalse;
}

/*
===============
G_AddBuildableToStack
===============
*/
void G_AddBuildableToStack( int groundBuildableNum, int stackedBuildableNum )
{
  int i;
  int *numOfStackedBuildables;
  gentity_t *groundBuildable;

  groundBuildable = &g_entities[ groundBuildableNum ];

  if( groundBuildable->s.eType != ET_BUILDABLE ||
      g_entities[ stackedBuildableNum ].s.eType != ET_BUILDABLE )
    return;

  numOfStackedBuildables = &groundBuildable->numOfStackedBuildables;

  if( *numOfStackedBuildables >= MAX_GENTITIES )
  {
    *numOfStackedBuildables = MAX_GENTITIES;
    return;
  }

  if( *numOfStackedBuildables < 0 )
    *numOfStackedBuildables = 0;

  if( !G_FindBuildableInStack( groundBuildableNum, stackedBuildableNum, &i )  )
  {
    groundBuildable->buildableStack[ *numOfStackedBuildables ] = stackedBuildableNum;
    *numOfStackedBuildables += 1;
  }
}

/*
===============
G_RemoveBuildableFromStack
===============
*/
void G_RemoveBuildableFromStack( int groundBuildableNum, int stackedBuildableNum )
{
  int i;
  int *numOfStackedBuildables;
  gentity_t *groundBuildable;

  groundBuildable = &g_entities[ groundBuildableNum ];

  if( groundBuildable->s.eType != ET_BUILDABLE ||
      g_entities[ stackedBuildableNum ].s.eType != ET_BUILDABLE )
    return;

  numOfStackedBuildables = &groundBuildable->numOfStackedBuildables;

  if( *numOfStackedBuildables <= 0 )
  {
    *numOfStackedBuildables = 0;
    return;
  }

  if( *numOfStackedBuildables == 1 )
  {
    if( groundBuildable->buildableStack[ 0 ] == stackedBuildableNum )
      groundBuildable->buildableStack[ 0 ] = ENTITYNUM_NONE;

    return;
  }

  if( *numOfStackedBuildables > MAX_GENTITIES )
    *numOfStackedBuildables = MAX_GENTITIES;

  if( G_FindBuildableInStack( groundBuildableNum, stackedBuildableNum, &i ) )
  {
    groundBuildable->buildableStack[ i ] = groundBuildable->buildableStack[ *numOfStackedBuildables - 1 ];
    *numOfStackedBuildables -= 1;
  }
}

/*
===============
G_SetBuildableDropper
===============
*/
void G_SetBuildableDropper( int removedBuildableNum, int dropperNum )
{
  int i;
  int *numOfStackedBuildables;
  gentity_t *removedBuildable;

  removedBuildable = &g_entities[ removedBuildableNum ];

  if( removedBuildable->s.eType != ET_BUILDABLE )
    return;

  numOfStackedBuildables = &removedBuildable->numOfStackedBuildables;

  if( *numOfStackedBuildables <= 0 )
  {
    *numOfStackedBuildables = 0;
    return;
  }

  if( *numOfStackedBuildables > MAX_GENTITIES )
    *numOfStackedBuildables = MAX_GENTITIES;

  for( i = 0; i < *numOfStackedBuildables; i++ )
  {
    g_entities[ removedBuildable->buildableStack[ i ] ].dropperNum = dropperNum;
    G_SetBuildableDropper( removedBuildable->buildableStack[ i ], dropperNum );
  }
}

/*
===============
G_BuildableThink

General think function for buildables
===============
*/
void G_BuildableThink( gentity_t *ent, int msec )
{
  int maxHealth = BG_Buildable( ent->s.modelindex )->health;
  int regenRate = BG_Buildable( ent->s.modelindex )->regenRate;
  int buildTime = BG_Buildable( ent->s.modelindex )->buildTime;

  //toggle spawned flag for buildables
  if( !ent->spawned && ent->health > 0 && !level.pausedTime )
  {
    if( ent->buildTime + buildTime < level.time )
    {
      ent->spawned = qtrue;
      if( ent->s.modelindex == BA_A_OVERMIND )
      {
        G_TeamCommand( TEAM_ALIENS,
                       va( "cp \"The Overmind has awakened!\" %d",
                       CP_OVERMIND_AWAKEN ) );
      }
    }
  }

  // Timer actions
  ent->time1000 += msec;
  if( ent->time1000 >= 1000 )
  {
    gentity_t *groundEnt;

    groundEnt = &g_entities[ ent->s.groundEntityNum ];

    ent->time1000 -= 1000;

    if( ent->health > 0 && ent->health < maxHealth )
    {
      if( !ent->spawned )
        ent->health += (int)( ceil( (float)maxHealth / (float)( buildTime * 0.001f ) ) );
      else
      {
        if( ent->buildableTeam == TEAM_ALIENS && regenRate &&
          ( ent->lastDamageTime + ALIEN_REGEN_DAMAGE_TIME ) < level.time )
        {
          ent->health += regenRate;
        }
        else if( ent->buildableTeam == TEAM_HUMANS && ent->dcc &&
          ( ent->lastDamageTime + HUMAN_REGEN_DAMAGE_TIME ) < level.time )
        {
          ent->health += DC_HEALRATE; // * ent->dcc;  #Swirl: Only allow 1 DCC
        }
      }

      if( ent->health >= maxHealth )
      {
        int i;
        ent->health = maxHealth;
        for( i = 0; i < MAX_CLIENTS; i++ )
          ent->credits[ i ] = 0;
      }
    }

    if(  g_allowBuildableStacking.integer )
    {
      meansOfDeath_t meansOD;

      if( ent->dropperNum == ENTITYNUM_WORLD )
        meansOD = MOD_CRUSH;
      else
        meansOD = MOD_DROP;

      if( groundEnt->s.eType == ET_BUILDABLE &&
          !BG_Buildable( groundEnt->s.modelindex )->stackable )
      {
        if( groundEnt->s.modelindex != BA_H_SPAWN &&
            groundEnt->s.modelindex != BA_A_SPAWN )
          G_Damage( groundEnt, ent, &g_entities[ ent->dropperNum ], NULL, NULL,
              BG_Buildable( groundEnt->s.modelindex )->health / 5, DAMAGE_NO_PROTECTION,
              meansOD );
        else if(ent->builtBy || groundEnt->builtBy)
          ent->damageDroppedBuildable = qtrue;
      }
      else if( groundEnt->client )
        G_Damage( groundEnt, ent, &g_entities[ ent->dropperNum ], NULL, NULL,
            BG_HP2SU( 20 ), DAMAGE_NO_PROTECTION, meansOD );

      if( ent->damageDroppedBuildable )
        G_Damage( ent, ent, &g_entities[ ent->dropperNum ], NULL, NULL,
            BG_Buildable( ent->s.modelindex )->health / 5, DAMAGE_NO_PROTECTION,
            meansOD );
    }

    //
    // check for sizzle damage
    //
    if(ent->health > 0) {
      trace_t   tr;
      SV_Trace(
        &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs,
        ent->r.currentOrigin, ent->s.number,
        (CONTENTS_LAVA|CONTENTS_SLIME), TT_AABB);
      if(tr.fraction < 1.0f || tr.allsolid || tr.startsolid) {
        if( tr.contents & CONTENTS_LAVA ) {
          G_Damage( ent, ent, &g_entities[ ent->dropperNum ], NULL, NULL,
            BG_Buildable( ent->s.modelindex )->health / 5, 0, MOD_LAVA );
        }

        if( ent->watertype & CONTENTS_SLIME ) {
          G_Damage( ent, ent, &g_entities[ ent->dropperNum ], NULL, NULL,
            BG_Buildable( ent->s.modelindex )->health / 10, 0, MOD_SLIME );
        }
      }
    }
  }

  if( ent->lev1Grabbed && ent->lev1GrabTime + LEVEL1_GRAB_TIME < level.time )
    ent->lev1Grabbed = qfalse;

  if( ent->clientSpawnTime > 0 )
    ent->clientSpawnTime -= msec;

  if( ent->clientSpawnTime < 0 )
    ent->clientSpawnTime = 0;

  ent->dcc = ( ent->buildableTeam != TEAM_HUMANS ) ? 0 : G_FindDCC( ent );

  // Set health
  ent->s.misc = MAX( BG_SU2HP( ent->health ), 0 );

  // Set flags
  ent->s.eFlags &= ~( EF_B_POWERED | EF_B_SPAWNED | EF_B_MARKED );
  if( ent->occupation.occupant && ent->occupation.occupant->client)
    ent->occupation.occupant->client->ps.stats[ STAT_STATE ] &=
                                                           ~( SS_HOVEL_MARKED );
  
  if( ent->powered )
    ent->s.eFlags |= EF_B_POWERED;

  if( ent->spawned )
    ent->s.eFlags |= EF_B_SPAWNED;

  if( ent->deconstruct )
  {
    if( ent->occupation.occupant && ent->occupation.occupant->client)
      ent->occupation.occupant->client->ps.stats[ STAT_STATE ] |=
                                                                SS_HOVEL_MARKED;
      
    ent->s.eFlags |= EF_B_MARKED;
  }

  // Check if this buildable is touching any triggers
  G_BuildableTouchTriggers( ent );

  // Fall back on normal physics routines
  if( msec != 0 )
  {
    G_Physics( ent, msec );

    //Move a hovel's occupant with it
    if( ent->s.modelindex == BA_A_HOVEL )
      G_PositionHovelsBuilder( ent );
  }
}


/*
===============
G_BuildableRange

Check whether a point is within some range of a type of buildable
===============
*/
qboolean G_BuildableRange( vec3_t origin, float r, buildable_t buildable )
{
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range;
  vec3_t    mins, maxs;
  int       i, num;
  gentity_t *ent;

  VectorSet( range, r, r, r );
  VectorAdd( origin, range, maxs );
  VectorSubtract( origin, range, mins );

  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
  {
    ent = &g_entities[ entityList[ i ] ];

    if( ent->s.eType != ET_BUILDABLE )
      continue;

    if( ent->buildableTeam == TEAM_HUMANS && !ent->powered )
      continue;

    if( ent->s.modelindex == buildable && ent->spawned )
      return qtrue;
  }

  return qfalse;
}

/*
================
G_FindBuildable

Finds a buildable of the specified type
================
*/
gentity_t *G_FindBuildable( buildable_t buildable )
{
  int       i;
  gentity_t *ent;

  for( i = MAX_CLIENTS, ent = g_entities + i;
       i < level.num_entities; i++, ent++ )
  {
    if( ent->s.eType != ET_BUILDABLE )
      continue;

    if( ent->s.modelindex == buildable && !( ent->s.eFlags & EF_DEAD ) )
      return ent;
  }

  return NULL;
}

/*
================
G_BuildableIsUnique

Determines if a given buildable unique.
================
*/
qboolean G_BuildableIsUnique(gentity_t *buildable) {
  int       i;
  gentity_t *ent;

  Com_Assert(buildable && "G_BuildableIsUnique: ent is NULL");
  Com_Assert(buildable->s.eType == ET_BUILDABLE && "G_BuildableIsUnique: ent is NULL");

  for(
    i = MAX_CLIENTS, ent = g_entities + i;
    i < level.num_entities; i++, ent++ ) {
    if( ent->s.eType != ET_BUILDABLE ) {
      continue;
    }

    if(ent == buildable) {
      continue;
    }

    if(ent->s.modelindex == buildable->s.modelindex && !(ent->s.eFlags & EF_DEAD)) {
      return qfalse;
    }
  }

  return qtrue;
}

/*
===============
G_CompareBuildablesForRemoval

qsort comparison function for a buildable removal list
===============
*/
static buildable_t  cmpBuildable;
static vec3_t       cmpOrigin;
static int G_CompareBuildablesForRemoval( const void *a, const void *b )
{
  int       precedence[ ] =
  {
    BA_NONE,

    BA_A_BARRICADE,
    BA_A_ACIDTUBE,
    BA_A_TRAPPER,
    BA_A_HIVE,
    BA_A_BOOSTER,
    BA_A_HOVEL,
    BA_A_SPAWN,
    BA_A_OVERMIND,

    BA_H_MGTURRET,
    BA_H_TESLAGEN,
    BA_H_DCC,
    BA_H_MEDISTAT,
    BA_H_ARMOURY,
    BA_H_SPAWN,
    BA_H_REPEATER,
    BA_H_REACTOR
  };

  gentity_t *buildableA, *buildableB;
  int       i;
  int       aPrecedence = 0, bPrecedence = 0;
  qboolean  aMatches = qfalse, bMatches = qfalse;

  buildableA = *(gentity_t **)a;
  buildableB = *(gentity_t **)b;

  // Prefer the one that collides with the thing we're building
  aMatches = G_BuildablesIntersect( cmpBuildable, cmpOrigin,
      buildableA->s.modelindex, buildableA->r.currentOrigin );
  bMatches = G_BuildablesIntersect( cmpBuildable, cmpOrigin,
      buildableB->s.modelindex, buildableB->r.currentOrigin );
  if( aMatches && !bMatches )
    return -1;
  else if( !aMatches && bMatches )
    return 1;

  // If the only spawn is marked, prefer it last
  if( cmpBuildable == BA_A_SPAWN || cmpBuildable == BA_H_SPAWN )
  {
    if( ( buildableA->s.modelindex == BA_A_SPAWN && level.numAlienSpawns == 1 ) ||
        ( buildableA->s.modelindex == BA_H_SPAWN && level.numHumanSpawns == 1 ) )
      return 1;

    if( ( buildableB->s.modelindex == BA_A_SPAWN && level.numAlienSpawns == 1 ) ||
        ( buildableB->s.modelindex == BA_H_SPAWN && level.numHumanSpawns == 1 ) )
      return -1;
  }

  // If one matches the thing we're building, prefer it
  aMatches = ( buildableA->s.modelindex == cmpBuildable );
  bMatches = ( buildableB->s.modelindex == cmpBuildable );
  if( aMatches && !bMatches )
    return -1;
  else if( !aMatches && bMatches )
    return 1;

  // They're the same type
  if( buildableA->s.modelindex == buildableB->s.modelindex )
  {
    gentity_t *powerEntity = G_PowerEntityForPoint( cmpOrigin );

    // Prefer the entity that is providing power for this point
    aMatches = ( powerEntity == buildableA );
    bMatches = ( powerEntity == buildableB );
    if( aMatches && !bMatches )
      return -1;
    else if( !aMatches && bMatches )
      return 1;

    // Pick the one marked earliest
    return buildableA->deconstructTime - buildableB->deconstructTime;
  }

  // Resort to preference list
  for( i = 0; i < ARRAY_LEN( precedence ); i++ )
  {
    if( buildableA->s.modelindex == precedence[ i ] )
      aPrecedence = i;

    if( buildableB->s.modelindex == precedence[ i ] )
      bPrecedence = i;
  }

  return aPrecedence - bPrecedence;
}

/*
===============
G_ClearDeconMarks

Remove decon mark from all buildables
===============
*/
void G_ClearDeconMarks( void )
{
  int       i;
  gentity_t *ent;

  for( i = MAX_CLIENTS, ent = g_entities + i ; i < level.num_entities ; i++, ent++ )
  {
    if( !ent->inuse )
      continue;

    if( ent->s.eType != ET_BUILDABLE )
      continue;

    ent->deconstruct = qfalse;
  }
}

/*
===============
G_FreeMarkedBuildables

Free up build points for a team by deconstructing marked buildables
===============
*/
void G_FreeMarkedBuildables( gentity_t *deconner, char *readable, int rsize,
  char *nums, int nsize )
{
  int       i;
  int       bNum;
  int       listItems = 0;
  int       totalListItems = 0;
  gentity_t *ent;
  int       removalCounts[ BA_NUM_BUILDABLES ] = {0};

  if( readable && rsize )
    readable[ 0 ] = '\0';
  if( nums && nsize )
    nums[ 0 ] = '\0';

  if( !g_markDeconstruct.integer )
    return; // Not enabled, can't deconstruct anything

  for( i = 0; i < level.numBuildablesForRemoval; i++ )
  {
    ent = level.markedBuildables[ i ];
    bNum = BG_Buildable( ent->s.modelindex )->number;

    if( removalCounts[ bNum ] == 0 )
      totalListItems++;

    G_Damage( ent, NULL, deconner, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_REPLACE );

    removalCounts[ bNum ]++;

    if( nums )
      Q_strcat( nums, nsize, va( " %d", (int)( ent - g_entities ) ) );

    G_RemoveRangeMarkerFrom( ent );
    G_FreeEntity( ent );
  }

  if( !readable )
    return;

  for( i = 0; i < BA_NUM_BUILDABLES; i++ )
  {
    if( removalCounts[ i ] )
    {
      if( listItems )
      {
        if( listItems == ( totalListItems - 1 ) )
          Q_strcat( readable, rsize,  va( "%s and ",
            ( totalListItems > 2 ) ? "," : "" ) );
        else
          Q_strcat( readable, rsize, ", " );
      }
      Q_strcat( readable, rsize, va( "%s", BG_Buildable( i )->humanName ) );
      if( removalCounts[ i ] > 1 )
        Q_strcat( readable, rsize, va( " (%dx)", removalCounts[ i ] ) );
      listItems++;
    }
  }
}

/*
===============
G_SufficientBPAvailable

Determine if enough build points can be released for the buildable
and list the buildables that must be destroyed if this is the case
===============
*/
static itemBuildError_t G_SufficientBPAvailable( buildable_t     buildable,
                                                 vec3_t          origin,
                                                 vec3_t          normal )
{
  int               i;
  int               numBuildables = 0;
  int               numRequired = 0;
  int               pointsYielded = 0;
  gentity_t         *ent;
  team_t            team = BG_Buildable( buildable )->team;
  int               buildPoints = BG_Buildable( buildable )->buildPoints;
  int               remainingBP, remainingSpawns;
  qboolean          collision = qfalse;
  qboolean          collisionOccupiedHovel = qfalse;
  int               collisionCount = 0;
  qboolean          repeaterInRange = qfalse;
  int               repeaterInRangeCount = 0;
  itemBuildError_t  bpError;
  buildable_t       spawn;
  buildable_t       core;
  int               spawnCount = 0;
  qboolean          changed = qtrue;

  level.numBuildablesForRemoval = 0;

  if( team == TEAM_ALIENS )
  {
    remainingBP     = G_GetBuildPoints( origin, team );
    remainingSpawns = level.numAlienSpawns;
    bpError         = IBE_NOALIENBP;
    spawn           = BA_A_SPAWN;
    core            = BA_A_OVERMIND;
  }
  else if( team == TEAM_HUMANS )
  {
    if( buildable == BA_H_REACTOR || buildable == BA_H_REPEATER )
      remainingBP   = level.humanBuildPoints;
    else
      remainingBP   = G_GetBuildPoints( origin, team );

    remainingSpawns = level.numHumanSpawns;
    bpError         = IBE_NOHUMANBP;
    spawn           = BA_H_SPAWN;
    core            = BA_H_REACTOR;
  }
  else
  {
    Com_Error( ERR_FATAL, "team is %d", team );
    return IBE_NONE;
  }

  // Simple non-marking case
  if( !g_markDeconstruct.integer )
  {
    if( remainingBP - buildPoints < 0 )
      return bpError;

    // Check for buildable<->buildable collisions
    for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
    {
      if( ent->s.eType != ET_BUILDABLE )
        continue;

      if( !ent->inuse ) {
        continue;
      }

      if( G_BuildablesIntersect( buildable, origin, ent->s.modelindex, ent->r.currentOrigin ) )
        return IBE_NOROOM;
    }

    return IBE_NONE;
  }

  // Set buildPoints to the number extra that are required
  buildPoints -= remainingBP;

  // Build a list of buildable entities
  for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if( ent->s.eType != ET_BUILDABLE )
      continue;

    if( !ent->inuse ) {
      continue;
    }

    if( ent->health <= 0 &&
        ent->r.contents == 0 &&
        (ent->s.eFlags & EF_NODRAW) ) {
      //this buildable has already gibbed
      continue;
    }

    collision = G_BuildablesIntersect( buildable, origin, ent->s.modelindex, ent->r.currentOrigin );

    if( !collision && ( buildable == BA_A_SPAWN ||
                        buildable == BA_H_SPAWN ) )
    {
      if( ent->deconstruct )
        SV_LinkEntity( ent );

      collision = ( G_CheckSpawnPoint( ENTITYNUM_NONE, origin, normal, buildable, NULL ) == ent );

      if( ent->deconstruct )
        SV_UnlinkEntity( ent );
    }

    if(
      !collision && ent->s.eType == ET_BUILDABLE &&
      ( ent->s.modelindex == BA_A_SPAWN || ent->s.modelindex == BA_H_SPAWN ) )
    {
      vec3_t buildable_mins, buildable_maxs;
      BG_BuildableBoundingBox( buildable, buildable_mins, buildable_maxs );
      collision = 
        G_CheckAreaForSpawnIntersection(
          ent->r.currentOrigin, ent->s.origin2, ent->s.modelindex,
          buildable_mins, buildable_maxs, origin);
    }

    if( collision )
    {
      // Don't allow replacements at all
      if( g_markDeconstruct.integer == 1 )
        return IBE_NOROOM;

      // Only allow replacements of the same type
      if( g_markDeconstruct.integer == 2 && ent->s.modelindex != buildable )
        return IBE_NOROOM;

      // Any other setting means anything goes

      collisionCount++;
    } else if( g_markDeconstruct.integer == 3 )
      continue;

    // Check if this is a repeater and it's in range
    if( buildable == BA_H_REPEATER &&
        buildable == ent->s.modelindex &&
        Distance( ent->r.currentOrigin, origin ) < REPEATER_BASESIZE )
    {
      repeaterInRange = qtrue;
      repeaterInRangeCount++;
    }
    else
      repeaterInRange = qfalse;


    if( ent->health <= 0 )
      continue;

    if( ent->buildableTeam != team )
      continue;

    // Explicitly disallow replacement of the core buildable with anything
    // other than the core buildable
    if( ent->s.modelindex == core && buildable != core )
      continue;

    // Don't allow a power source to be replaced by a dependant
    if( team == TEAM_HUMANS &&
        G_PowerEntityForPoint( origin ) == ent &&
        buildable != BA_H_REPEATER &&
        buildable != core )
      continue;

    if( ent->deconstruct )
    {
      level.markedBuildables[ numBuildables++ ] = ent;

      // Buildables that are marked here will always end up at the front of the
      // removal list, so just incrementing numBuildablesForRemoval is sufficient
      if( collision || repeaterInRange )
      {
        // Collided with something, so we definitely have to remove it or
        // it's a repeater that intersects the new repeater's power area,
        // so it must be removed

        if( collision )
        {
          collisionCount--;
          if ( ent->s.modelindex == BA_A_HOVEL &&
             ( ent->flags & FL_OCCUPIED ) )
            collisionOccupiedHovel = qtrue;
        }

        if( repeaterInRange )
          repeaterInRangeCount--;

        pointsYielded += BG_Buildable( ent->s.modelindex )->buildPoints;
        level.numBuildablesForRemoval++;
      }
      else if( BG_Buildable( ent->s.modelindex )->uniqueTest &&
               ent->s.modelindex == buildable )
      {
        // If it's a unique buildable, it must be replaced by the same type
        pointsYielded += BG_Buildable( ent->s.modelindex )->buildPoints;
        level.numBuildablesForRemoval++;
      }
    }
  }

  numRequired = level.numBuildablesForRemoval;

  // We still need build points, but have no candidates for removal
  if( buildPoints > 0 && numBuildables == 0 )
    return bpError;

  // Collided with something we can't remove
  if( collisionCount > 0 )
    return IBE_NOROOM;

  // Collided with a marked hovel that is occupied
  if( collisionOccupiedHovel )
    return IBE_NOROOM_HOVELOCCUPIED;

  // There are one or more repeaters we can't remove
  if( repeaterInRangeCount > 0 )
    return IBE_RPTPOWERHERE;

  // Sort the list
  cmpBuildable = buildable;
  VectorCopy( origin, cmpOrigin );
  qsort( level.markedBuildables, numBuildables, sizeof( level.markedBuildables[ 0 ] ),
         G_CompareBuildablesForRemoval );

  // Determine if there are enough markees to yield the required BP
  for( ; pointsYielded < buildPoints && level.numBuildablesForRemoval < numBuildables;
       level.numBuildablesForRemoval++ )
  {
    ent = level.markedBuildables[ level.numBuildablesForRemoval ];
    pointsYielded += BG_Buildable( ent->s.modelindex )->buildPoints;
  }

  // Do another pass to see if we can meet quota with fewer buildables
  //  than we have now due to mismatches between priority and BP amounts
  //  by repeatedly testing if we can chop off the first thing that isn't
  //  required by rules of collision/uniqueness, which are always at the head
  while( changed && level.numBuildablesForRemoval > 1 &&
         level.numBuildablesForRemoval > numRequired )
  {
    int pointsUnYielded = 0;
    changed = qfalse;
    ent = level.markedBuildables[ numRequired ];
    pointsUnYielded = BG_Buildable( ent->s.modelindex )->buildPoints;

    if( pointsYielded - pointsUnYielded >= buildPoints )
    {
      pointsYielded -= pointsUnYielded;
      memmove( &level.markedBuildables[ numRequired ],
               &level.markedBuildables[ numRequired + 1 ],
               ( level.numBuildablesForRemoval - numRequired )
                 * sizeof( gentity_t * ) );
      level.numBuildablesForRemoval--;
      changed = qtrue;
    }
  }

  for( i = 0; i < level.numBuildablesForRemoval; i++ )
  {
    if( level.markedBuildables[ i ]->s.modelindex == spawn )
      spawnCount++;
  }

  // Make sure we're not removing the last spawn
  if( !g_cheats.integer && remainingSpawns > 0 && ( remainingSpawns - spawnCount ) < 1 )
    return IBE_LASTSPAWN;

  // Not enough points yielded
  if( pointsYielded < buildPoints )
    return bpError;
  else
    return IBE_NONE;
}

static void G_SetBuildableMarkedLinkState( qboolean link ) {
  int       i;
  gentity_t *ent;

  for ( i = 1, ent = g_entities + i; i < level.num_entities; i++, ent++ ) {
    if( ent->s.eType != ET_BUILDABLE ) {
      continue;
    }

    if( !ent->inuse ) {
      continue;
    }

    if( !( ent->deconstruct ) ) {
      continue;
    }

    if( link )
      SV_LinkEntity( ent );
    else
      SV_UnlinkEntity( ent );
  }
}

/*
=================================================
G_IsOvermindCreepHere

Checks to see if a point is within Overmind creep
=================================================
*/
static qboolean G_IsOvermindCreepHere( vec3_t point )
{
  gentity_t  *tempent;
  float      d;

  // TODO: This needs to be improved to account for multiple overminds
  tempent = G_Overmind( );

  if( tempent != NULL )
  {
    d = Distance( tempent->r.currentOrigin, point );
    if ( d <= CREEP_BASESIZE )
    {
      return qtrue;
    }
  }

  return qfalse;
}

/*
================
G_CanBuild

Checks to see if a buildable can be built
================
*/
itemBuildError_t G_CanBuild( gentity_t *ent, buildable_t buildable, int distance,
                             vec3_t origin, vec3_t normal, int *groundEntNum )
{
  vec3_t            angles;
  vec3_t            entity_origin;
  vec3_t            mins, maxs;
  trace_t           tr1, tr2, tr3;
  itemBuildError_t  reason = IBE_NONE, tempReason;
  gentity_t         *tempent, *powerBuildable;
  float             minNormal;
  qboolean          invert, intersectsBuildable = qfalse;
  int               contents;
  playerState_t     *ps = &ent->client->ps;

  G_SetBuildableMarkedLinkState( qfalse );

  BG_BuildableBoundingBox( buildable, mins, maxs );

  G_SetPlayersLinkState( qfalse, ent );
  BG_PositionBuildableRelativeToPlayer( ps, qfalse, G_TraceWrapper,
                                        entity_origin, angles, &tr1 );
  G_SetPlayersLinkState( qtrue, ent );

  SV_Trace( &tr2, entity_origin, mins, maxs, entity_origin, -1,
                MASK_PLAYERSOLID, TT_AABB );

  VectorCopy( entity_origin, origin );
  *groundEntNum = tr1.entityNum;
  VectorCopy( tr1.plane.normal, normal );
  minNormal = BG_Buildable( buildable )->minNormal;
  invert = BG_Buildable( buildable )->invertNormal;

  //can we build at this angle?
  if( !( normal[ 2 ] >= minNormal || ( invert && normal[ 2 ] <= -minNormal ) ) )
    reason = IBE_NORMAL;

  if(
    (
      tr1.entityNum != ENTITYNUM_WORLD &&
      (
        !g_allowBuildableStacking.integer ||
        (
          g_entities[ *groundEntNum ].s.eType == ET_BUILDABLE &&
          !BG_Buildable( g_entities[ *groundEntNum ].s.modelindex )->stackable ) ||
      g_entities[ *groundEntNum ].client )) ||
    (
      g_entities[ tr1.entityNum ].classname &&
      (
        !strcmp( g_entities[ tr1.entityNum ].classname, "func_destructable" ) ||
        !strcmp( g_entities[ tr1.entityNum ].classname, "func_spawn")))) {
    reason = IBE_NORMAL;
  }

  contents = SV_PointContents( entity_origin, -1 );

  if( ( tempReason = G_SufficientBPAvailable( buildable, origin, normal ) ) != IBE_NONE )
    reason = tempReason;

  if( ent->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
  {
    //alien criteria

    // Check there is an Overmind
    if( buildable != BA_A_OVERMIND )
    {
      if( !G_Overmind( ) )
        reason = IBE_NOOVERMIND;
    }

    //check there is creep near by for building on
    if( BG_Buildable( buildable )->creepTest )
    {
      if( !G_IsCreepHere( entity_origin ) )
        reason = IBE_NOCREEP;
    }


    if ( buildable == BA_A_HOVEL )
    {
      if ( APropHovel_Blocked( origin, angles, normal, ent ) )
          reason = IBE_HOVELEXIT;
    }

    // Check if the enemy isn't blocking your building during pre-game warmup
    if( IS_WARMUP && g_warmupBlockEnemyBuilding.integer &&
        ( G_IsPowered( entity_origin ) == BA_H_REACTOR ) )
    {
      gentity_t *blocking_ent = G_PowerEntityForPoint( entity_origin );

      if(
        blocking_ent && buildable != BA_A_OVERMIND &&
        (
          !G_IsOvermindCreepHere( entity_origin ) ||
          G_BBOXes_Visible(
            ENTITYNUM_NONE, entity_origin, mins, maxs,
            blocking_ent->s.number, blocking_ent->r.currentOrigin,
            blocking_ent->r.mins, blocking_ent->r.maxs, MASK_SHOT))) {
        reason = IBE_BLOCKEDBYENEMY;
      }
    }

    // Check permission to build here
    if( tr1.surfaceFlags & SURF_NOALIENBUILD || contents & CONTENTS_NOALIENBUILD )
      reason = IBE_PERMISSION;
  }
  else if( ent->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
  {
    //human criteria

    // Check for power
    if( G_IsPowered( entity_origin ) == BA_NONE )
    {
      //tell player to build a repeater to provide power
      if( buildable != BA_H_REACTOR && buildable != BA_H_REPEATER )
        reason = IBE_NOPOWERHERE;
    }

    // Check if the enemy isn't blocking your building during pre-game warmup
    if( IS_WARMUP && g_warmupBlockEnemyBuilding.integer &&
        G_IsOvermindCreepHere( entity_origin ) )
    {
      gentity_t *blocking_ent = G_Overmind();

      if(
        blocking_ent && buildable != BA_H_REACTOR &&
        (
          G_IsPowered( entity_origin ) != BA_H_REACTOR ||
          G_BBOXes_Visible(
            ENTITYNUM_NONE, entity_origin, mins, maxs,
            blocking_ent->s.number, blocking_ent->r.currentOrigin,
            blocking_ent->r.mins, blocking_ent->r.maxs, MASK_SHOT))) {
        reason = IBE_BLOCKEDBYENEMY;
      }
    }

    //this buildable requires a DCC
    if( BG_Buildable( buildable )->dccTest && !G_IsDCCBuilt( ) )
      reason = IBE_NODCC;

    //check that there is a parent reactor when building a repeater
    if( buildable == BA_H_REPEATER )
    {
      tempent = G_Reactor( );

      powerBuildable = G_PowerEntityForPoint( entity_origin );

      G_SetBuildableMarkedLinkState( qtrue );

      if( powerBuildable )
        intersectsBuildable = G_BuildablesIntersect( buildable, entity_origin, powerBuildable->s.modelindex,
                                                          powerBuildable->r.currentOrigin );

      G_SetBuildableMarkedLinkState( qfalse );

      if( tempent == NULL ) // No reactor
        reason = IBE_RPTNOREAC;
      else if( g_markDeconstruct.integer && g_markDeconstruct.integer != 3 &&
               powerBuildable && powerBuildable->s.modelindex == BA_H_REACTOR )
        reason = IBE_RPTPOWERHERE;
      else if( powerBuildable &&
               ( !g_markDeconstruct.integer || ( g_markDeconstruct.integer == 3 &&
                   !intersectsBuildable ) ) )
        reason = IBE_RPTPOWERHERE;
    }

    // Check permission to build here
    if( tr1.surfaceFlags & SURF_NOHUMANBUILD || contents & CONTENTS_NOHUMANBUILD )
      reason = IBE_PERMISSION;
  }

  // Check permission to build here
  if( tr1.surfaceFlags & SURF_NOBUILD || contents & CONTENTS_NOBUILD )
    reason = IBE_PERMISSION;

  // Can we only have one of these?
  if(
    BG_Buildable( buildable )->uniqueTest ||
    (
      !IS_WARMUP &&
      G_TimeTilSuddenDeath() <= 0 &&
      G_SD_Mode( ) == SDMODE_SELECTIVE &&
      level.sudden_death_replacable[buildable]))
  {
    tempent = G_FindBuildable( buildable );
    intersectsBuildable = qfalse;

    G_SetBuildableMarkedLinkState( qtrue );

      if( tempent )
        intersectsBuildable = G_BuildablesIntersect( buildable, entity_origin, tempent->s.modelindex,
                                                          tempent->r.currentOrigin );

      G_SetBuildableMarkedLinkState( qfalse );

    if( tempent && ( !tempent->deconstruct || ( g_markDeconstruct.integer == 3 &&
                                                !intersectsBuildable ) ) )
    {
      switch( buildable )
      {
        case BA_A_OVERMIND:
          reason = IBE_ONEOVERMIND;
          break;

        case BA_A_HOVEL:
          reason = IBE_ONEHOVEL;
          break;

        case BA_H_REACTOR:
          reason = IBE_ONEREACTOR;
          break;

	case BA_H_DCC:
          reason = IBE_ONEDCC;
          break;

        default:
          if(
            !IS_WARMUP &&
            G_TimeTilSuddenDeath() <= 0 &&
            G_SD_Mode( ) == SDMODE_SELECTIVE) {
            reason = IBE_SD_UNIQUE;
          } else {
            Com_Error( ERR_FATAL, "No reason for denying build of %d", buildable );
          }
          break;
      }
    }
  }

  if(
    !IS_WARMUP && G_TimeTilSuddenDeath() <= 0 &&
    G_SD_Mode( ) == SDMODE_SELECTIVE &&
    !level.sudden_death_replacable[buildable]) {
    reason = IBE_SD_IRREPLACEABLE;
  }

  G_SetBuildableMarkedLinkState( qtrue );

  //check there is enough room to spawn from (presuming this is a spawn)
  if( reason == IBE_NONE )
  {
    G_SetBuildableMarkedLinkState( qfalse );
    SV_UnlinkEntity( ent );
    if( G_CheckSpawnPoint( ENTITYNUM_NONE, origin, normal, buildable, NULL ) != NULL )
      reason = IBE_NORMAL;
    G_SetBuildableMarkedLinkState( qtrue );
    SV_LinkEntity( ent );
  }

  //this item does not fit here
  if( reason == IBE_NONE &&
      ( tr1.fraction >= 1.0f || tr2.startsolid ) )
    reason = IBE_NOROOM;

  //can't build in lava nor slime
  if(reason == IBE_NONE) {
    SV_Trace(
      &tr3, entity_origin, mins, maxs, entity_origin, ent->s.number,
      (CONTENTS_LAVA|CONTENTS_SLIME), TT_AABB);
    if(tr3.fraction < 1.0f || tr3.allsolid || tr3.startsolid) {
      reason = IBE_NOROOM;
    }
  }

  if( reason != IBE_NONE )
    level.numBuildablesForRemoval = 0;

  return reason;
}


/*
================
G_AddRangeMarkerForBuildable
================
*/
static void G_AddRangeMarkerForBuildable( gentity_t *self )
{
  if( BG_Buildable( self->s.modelindex )->rangeMarkerRange > 0.0f )
  {
    self->rangeMarker = qtrue;
    self->r.svFlags |= SVF_CLIENTMASK_INCLUSIVE;
  } else
    self->rangeMarker = qfalse;

}

/*
================
G_RemoveRangeMarkerFrom
================
*/
void G_RemoveRangeMarkerFrom( gentity_t *self )
{
  if( self->rangeMarker )
  {
    self->rangeMarker = qfalse;
    self->r.svFlags &= ~SVF_CLIENTMASK_INCLUSIVE;
  }
}

/*
================
G_Build

Spawns a buildable
================
*/
static gentity_t *G_Build( gentity_t *builder, buildable_t buildable,
  const vec3_t origin, const vec3_t normal, const vec3_t angles, int groundEntNum )
{
  gentity_t *built;
  char      readable[ MAX_STRING_CHARS ];
  char      buildnums[ MAX_STRING_CHARS ];
  buildLog_t *log;

  if( builder->client )
    log = G_BuildLogNew( builder, BF_CONSTRUCT );
  else
    log = NULL;

  // Free existing buildables
  G_FreeMarkedBuildables( builder, readable, sizeof( readable ),
    buildnums, sizeof( buildnums ) );

  if( builder->client )
  {
    // Spawn the buildable
    built = G_Spawn();
  }
  else
    built = builder;

  built->s.eType = ET_BUILDABLE;
  built->killedBy = ENTITYNUM_NONE;
  built->classname = BG_Buildable( buildable )->entityName;
  built->s.modelindex = buildable;
  built->buildableTeam = built->s.modelindex2 = BG_Buildable( buildable )->team;
  BG_BuildableBoundingBox( buildable, built->r.mins, built->r.maxs );

  built->health = 1;

  built->splashDamage = BG_Buildable( buildable )->splashDamage;
  built->splashRadius = BG_Buildable( buildable )->splashRadius;
  built->splashMethodOfDeath = BG_Buildable( buildable )->meansOfDeath;

  built->nextthink = BG_Buildable( buildable )->nextthink;

  built->activation.flags = BG_Buildable( buildable )->activationFlags;
  built->occupation.flags = BG_Buildable( buildable )->occupationFlags;
  built->occupation.pm_type = BG_Buildable( buildable )->activationPm_type;
  built->occupation.contents = BG_Buildable( buildable )->activationContents;
  built->occupation.clipMask = BG_Buildable( buildable )->activationClipMask;

  built->takedamage = qtrue;
  built->spawned = qfalse;
  built->buildTime = built->s.time = level.time;
  built->attemptSpawnTime = -1;

  // initialize buildable stacking variables
  built->damageDroppedBuildable = qfalse;
  built->dropperNum = builder->s.number;
  memset( built->buildableStack, ENTITYNUM_NONE, sizeof( built->buildableStack ) );
  built->numOfStackedBuildables = 0;
  G_AddBuildableToStack( groundEntNum, built->s.number );

  // build instantly in cheat mode
  if( builder->client && ( g_cheats.integer || IS_WARMUP ) )
  {
    built->health = BG_Buildable( buildable )->health;
    built->buildTime = built->s.time =
      level.time - BG_Buildable( buildable )->buildTime;
  }

  //things that vary for each buildable that aren't in the dbase
  switch( buildable )
  {
    case BA_A_SPAWN:
      built->die = AGeneric_Die;
      built->think = ASpawn_Think;
      built->pain = AGeneric_Pain;
      break;

    case BA_A_BARRICADE:
      built->die = ABarricade_Die;
      built->think = ABarricade_Think;
      built->pain = ABarricade_Pain;
      built->touch = ABarricade_Touch;
      built->shrunkTime = 0;
      ABarricade_Shrink( built, qtrue );
      break;

    case BA_A_BOOSTER:
      built->die = AGeneric_Die;
      built->think = ABooster_Think;
      built->pain = AGeneric_Pain;
      built->touch = ABooster_Touch;
      break;

    case BA_A_ACIDTUBE:
      built->die = AGeneric_Die;
      built->think = AAcidTube_Think;
      built->pain = AGeneric_Pain;
      break;

    case BA_A_HIVE:
      built->die = AGeneric_Die;
      built->think = AHive_Think;
      built->pain = AHive_Pain;
      break;

    case BA_A_HOVEL:
      built->die = AHovel_Die;
      built->activation.activate = AHovel_Activate;
      built->activation.canActivate = AHovel_CanActivate;
      built->activation.willActivate = AHovel_WillActivate;
      built->occupation.occupy = AHovel_Occupy;
      built->occupation.unoccupy = AHovel_Unoccupy;
      built->occupation.occupantReset = AHovel_OccupantReset;
      built->occupation.occupiedReset = AHovel_OccupiedReset;
      built->activation.menuMsgOvrd[ ACTMN_ACT_OCCUPIED ] = MN_A_HOVEL_OCCUPIED;
      built->activation.menuMsgOvrd[ ACTMN_ACT_NOEXIT ] = MN_A_HOVEL_BLOCKED;
      built->think = AHovel_Think;
      built->pain = AGeneric_Pain;
      break;

    case BA_A_TRAPPER:
      built->die = AGeneric_Die;
      built->think = ATrapper_Think;
      built->pain = AGeneric_Pain;
      break;

    case BA_A_OVERMIND:
      built->die = AGeneric_Die;
      built->think = AOvermind_Think;
      built->pain = AGeneric_Pain;
      break;

    case BA_H_SPAWN:
      built->die = HSpawn_Die;
      built->think = HSpawn_Think;
      break;

    case BA_H_MGTURRET:
      built->die = HSpawn_Die;
      built->think = HMGTurret_Think;
      break;

    case BA_H_TESLAGEN:
      built->die = HSpawn_Die;
      built->think = HTeslaGen_Think;
      break;

    case BA_H_ARMOURY:
      built->think = HArmoury_Think;
      built->die = HSpawn_Die;
      built->activation.activate = HArmoury_Activate;
      break;

    case BA_H_DCC:
      built->think = HDCC_Think;
      built->die = HSpawn_Die;
      break;

    case BA_H_MEDISTAT:
      built->think = HMedistat_Think;
      built->die = HMedistat_Die;
      break;

    case BA_H_REACTOR:
      built->think = HReactor_Think;
      built->die = HSpawn_Die;
      built->activation.activate = HRepeater_Activate;
      built->activation.canActivate = HRepeater_CanActivate;
      built->powered = built->active = qtrue;
      break;

    case BA_H_REPEATER:
      built->think = HRepeater_Think;
      built->die = HRepeater_Die;
      built->activation.activate = HRepeater_Activate;
      built->activation.canActivate = HRepeater_CanActivate;
      built->count = -1;
      break;

    default:
      //erk
      break;
  }

  G_SetContents( built, CONTENTS_BODY );
  G_SetClipmask( built, MASK_PLAYERSOLID );
  built->enemy = NULL;
  built->s.weapon = BG_Buildable( buildable )->turretProjType;

  if( builder->client )
    built->builtBy = builder->client->pers.namelog;
  else if( builder->builtBy )
    built->builtBy = builder->builtBy;
  else
    built->builtBy = NULL;

  G_SetOrigin( built, origin );

  // set turret angles
  VectorCopy( builder->s.angles2, built->s.angles2 );

  VectorCopy( angles, built->s.apos.trBase );
  VectorCopy( angles, built->r.currentAngles );
  built->s.apos.trBase[ PITCH ] = 0.0f;
  built->r.currentAngles[ PITCH ] = 0.0f;
  built->s.angles2[ YAW ] = angles[ YAW ];
  built->s.angles2[ PITCH ] = MGTURRET_VERTICALCAP;
  built->physicsBounce = BG_Buildable( buildable )->bounce;

  built->s.groundEntityNum = groundEntNum;
  if( groundEntNum == ENTITYNUM_NONE )
  {
    built->s.pos.trType = BG_Buildable( buildable )->traj;
    built->s.pos.trTime = level.time;
    // gently nudge the buildable onto the surface :)
    VectorScale( normal, -50.0f, built->s.pos.trDelta );
  }

  built->s.misc = MAX( BG_SU2HP( built->health ), 0 );

  if( BG_Buildable( buildable )->team == TEAM_ALIENS )
  {
    built->powered = qtrue;
    built->s.eFlags |= EF_B_POWERED;
  }
  else if( ( built->powered = G_FindPower( built, qfalse ) ) )
    built->s.eFlags |= EF_B_POWERED;

  built->s.eFlags &= ~EF_B_SPAWNED;

  VectorCopy( normal, built->s.origin2 );

  G_AddEvent( built, EV_BUILD_CONSTRUCT, 0 );

  G_SetIdleBuildableAnim( built, BG_Buildable( buildable )->idleAnim );

  if( built->builtBy )
    G_SetBuildableAnim( built, BANIM_CONSTRUCT1, qtrue );

  SV_LinkEntity( built );

  if( builder && builder->client )
  {
    G_TeamCommand( builder->client->ps.stats[ STAT_TEAM ],
      va( "print \"%s ^2built^7 by %s%s%s\n\"",
        BG_Buildable( built->s.modelindex )->humanName,
        builder->client->pers.netname,
        ( readable[ 0 ] ) ? "^7, ^3replacing^7 " : "",
        readable ) );
    G_LogPrintf( "Construct: %d %d %s%s: %s" S_COLOR_WHITE " is building "
      "%s%s%s\n",
      (int)( builder - g_entities ),
      (int)( built - g_entities ),
      BG_Buildable( built->s.modelindex )->name,
      buildnums,
      builder->client->pers.netname,
      BG_Buildable( built->s.modelindex )->humanName,
      readable[ 0 ] ? ", replacing " : "",
      readable );
  }

  if( log )
    G_BuildLogSet( log, built );

  G_AddRangeMarkerForBuildable( built );

  return built;
}

/*
=================
G_BuildIfValid
=================
*/
qboolean G_BuildIfValid( gentity_t *ent, buildable_t buildable )
{
  float         dist;
  vec3_t        origin, normal;
  int           groundEntNum;
  gentity_t     *built;

  dist = BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->buildDist;

  switch( G_CanBuild( ent, buildable, dist, origin, normal, &groundEntNum ) )
  {
    case IBE_NONE:
      built = G_Build( ent, buildable, origin, normal, ent->s.apos.trBase, groundEntNum );
      if( ent->client )
      {
        ent->client->built = built;
      }
      return qtrue;

    case IBE_NOALIENBP:
      G_TriggerMenu( ent->client->ps.clientNum, MN_A_NOBP );
      return qfalse;

    case IBE_NOOVERMIND:
      G_TriggerMenu( ent->client->ps.clientNum, MN_A_NOOVMND );
      return qfalse;

    case IBE_NOCREEP:
      G_TriggerMenu( ent->client->ps.clientNum, MN_A_NOCREEP );
      return qfalse;

    case IBE_ONEOVERMIND:
      G_TriggerMenu( ent->client->ps.clientNum, MN_A_ONEOVERMIND );
      return qfalse;

    case IBE_ONEHOVEL:
      G_TriggerMenu( ent->client->ps.clientNum, MN_A_ONEHOVEL );
      return qfalse;

    case IBE_HOVELEXIT:
      G_TriggerMenu( ent->client->ps.clientNum, MN_A_HOVEL_EXIT );
      return qfalse;

    case IBE_NORMAL:
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_NORMAL );
      return qfalse;

    case IBE_PERMISSION:
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_NORMAL );
      return qfalse;

    case IBE_ONEREACTOR:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_ONEREACTOR );
      return qfalse;

    case IBE_NOPOWERHERE:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOPOWERHERE );
      return qfalse;

    case IBE_NOROOM:
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_NOROOM );
      return qfalse;

    case IBE_NOROOM_HOVELOCCUPIED:
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_NOROOM_HOVELOCCUPIED );
      return qfalse;

    case IBE_NOHUMANBP:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOBP);
      return qfalse;

    case IBE_NODCC:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NODCC );
      return qfalse;

    case IBE_ONEDCC:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_ONEDCC );
      return qfalse;

    case IBE_RPTPOWERHERE:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_RPTPOWERHERE );
      return qfalse;

    case IBE_LASTSPAWN:
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_LASTSPAWN );
      return qfalse;

    case IBE_BLOCKEDBYENEMY:
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_BLOCKEDBYENEMY );
      return qfalse;

    case IBE_SD_UNIQUE:
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_SD_UNIQUE );
      return qfalse;

    case IBE_SD_IRREPLACEABLE:
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_SD_IRREPLACEABLE );
      return qfalse;

    default:
      break;
  }

  return qfalse;
}

/*
================
G_FinishSpawningBuildable

Traces down to find where an item should rest, instead of letting them
free fall from their spawn points
================
*/
static gentity_t *G_FinishSpawningBuildable( gentity_t *ent, qboolean force )
{
  trace_t     tr;
  vec3_t      normal, dest;
  gentity_t   *built;
  buildable_t buildable = ent->s.modelindex;

  if( ent->s.origin2[ 0 ] || ent->s.origin2[ 1 ] || ent->s.origin2[ 2 ] )
    VectorCopy( ent->s.origin2, normal );
  else if( BG_Buildable( buildable )->traj == TR_BUOYANCY )
    VectorSet( normal, 0.0f, 0.0f, -1.0f );
  else
    VectorSet( normal, 0.0f, 0.0f, 1.0f );

  built = G_Build( ent, buildable, ent->r.currentOrigin,
                   normal, ent->r.currentAngles, ENTITYNUM_NONE );

  built->takedamage = qtrue;
  built->spawned = qtrue; //map entities are already spawned
  built->health = BG_Buildable( buildable )->health;
  built->s.eFlags |= EF_B_SPAWNED;

  // drop towards normal surface
  VectorScale( built->s.origin2, -4096.0f, dest );
  VectorAdd( dest, built->r.currentOrigin, dest );

  SV_Trace( &tr, built->r.currentOrigin, built->r.mins, built->r.maxs, dest,
    built->s.number, built->clipmask, TT_AABB );

  if( tr.startsolid && !force )
  {
    Com_Printf( S_COLOR_YELLOW "G_FinishSpawningBuildable: %s startsolid at %s\n",
              built->classname, vtos( built->r.currentOrigin ) );
    G_RemoveRangeMarkerFrom( built );
    G_FreeEntity( built );
    return NULL;
  }

  //point items in the correct direction
  VectorCopy( tr.plane.normal, built->s.origin2 );

  // allow to ride movers
  built->s.groundEntityNum = tr.entityNum;

  G_SetOrigin( built, tr.endpos );

  SV_LinkEntity( built );
  return built;
}

/*
============
G_SpawnBuildableThink

Complete spawning a buildable using its placeholder
============
*/
static void G_SpawnBuildableThink( gentity_t *ent )
{
  G_FinishSpawningBuildable( ent, qfalse );
  G_BuildableThink( ent, 0 );
}

/*
============
G_SpawnBuildable

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void G_SpawnBuildable( gentity_t *ent, buildable_t buildable )
{
  ent->s.modelindex = buildable;

  // some movers spawn on the second frame, so delay item
  // spawns until the third frame so they can ride trains
  ent->nextthink = level.time + FRAMETIME * 2;
  ent->think = G_SpawnBuildableThink;
}

void G_ParseCSVBuildablePlusList( const char *string, int *buildables, int buildablesSize )
{
  char      buffer[ MAX_STRING_CHARS ];
  int       i = 0;
  char      *p, *q;
  qboolean  EOS = qfalse;

  Q_strncpyz( buffer, string, MAX_STRING_CHARS );

  p = q = buffer;

  while( *p != '\0' && i < buildablesSize - 1 )
  {
    //skip to first , or EOS
    while( *p != ',' && *p != '\0' )
      p++;

    if( *p == '\0' )
      EOS = qtrue;

    *p = '\0';

    //strip leading whitespace
    while( *q == ' ' )
      q++;

    if( !Q_stricmp( q, "alien" ) )
    {
      buildable_t b;
      for( b = BA_A_SPAWN; b <= BA_A_HIVE && i < buildablesSize - 1; ++b )
        buildables[ i++ ] = b;

      if( i < buildablesSize - 1 )
        buildables[ i++ ] = BA_NUM_BUILDABLES + TEAM_ALIENS;
    }
    else if( !Q_stricmp( q, "human" ) )
    {
      buildable_t b;
      for( b = BA_H_SPAWN; b <= BA_H_REPEATER && i < buildablesSize - 1; ++b )
        buildables[ i++ ] = b;

      if( i < buildablesSize - 1 )
        buildables[ i++ ] = BA_NUM_BUILDABLES + TEAM_HUMANS;
    }
    else if( !Q_stricmp( q, "ivo_spectator" ) )
      buildables[ i++ ] = BA_NUM_BUILDABLES + TEAM_NONE;
    else if( !Q_stricmp( q, "ivo_alien" ) )
      buildables[ i++ ] = BA_NUM_BUILDABLES + TEAM_ALIENS;
    else if( !Q_stricmp( q, "ivo_human" ) )
      buildables[ i++ ] = BA_NUM_BUILDABLES + TEAM_HUMANS;
    else
    {
      buildables[ i ] = BG_BuildableByName( q )->number;
      if( buildables[ i ] == BA_NONE )
        Com_Printf( S_COLOR_YELLOW "WARNING: unknown buildable or special identifier %s\n", q );
      else
        i++;
    }

    if( !EOS )
    {
      p++;
      q = p;
    }
    else
      break;
  }

  buildables[ i ] = BA_NONE;
}

/*
============
G_LayoutSave
============
*/
void G_LayoutSave( char *lstr )
{
  char *lstrPipePtr;
  qboolean bAllowed[ BA_NUM_BUILDABLES + NUM_TEAMS ];
  char map[ MAX_QPATH ];
  char fileName[ MAX_OSPATH ];
  fileHandle_t f;
  int len;
  int i;
  gentity_t *ent;
  char *s;

  Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
  if( !map[ 0 ] )
  {
    Com_Printf( "LayoutSave( ): no map is loaded\n" );
    return;
  }

  if( ( lstrPipePtr = strchr( lstr, '|' ) ) )
  {
    int bList[ BA_NUM_BUILDABLES + NUM_TEAMS ];
    *lstrPipePtr = '\0';
    G_ParseCSVBuildablePlusList( lstr, &bList[ 0 ], sizeof( bList ) / sizeof( bList[ 0 ] ) );
    memset( bAllowed, 0, sizeof( bAllowed ) );
    for( i = 0; bList[ i ] != BA_NONE; i++ )
      bAllowed[ bList[ i ] ] = qtrue;
    *lstrPipePtr = '|';
    lstr = lstrPipePtr + 1;
  }
  else
    bAllowed[ BA_NONE ] = qtrue; // allow all

  Com_sprintf( fileName, sizeof( fileName ), "layouts/%s/%s.dat", map, lstr );

  len = FS_FOpenFileByMode( fileName, &f, FS_WRITE );
  if( len < 0 )
  {
    Com_Printf( "layoutsave: could not open %s\n", fileName );
    return;
  }

  Com_Printf( "layoutsave: saving layout to %s\n", fileName );

  for( i = MAX_CLIENTS; i < level.num_entities; i++ )
  {
    const char *name;

    ent = &level.gentities[ i ];
    if( ent->s.eType == ET_BUILDABLE )
    {
      if( !bAllowed[ BA_NONE ] && !bAllowed[ ent->s.modelindex ] )
        continue;
      name = BG_Buildable( ent->s.modelindex )->name;
    }
    else if( ent->count == 1 && !strcmp( ent->classname, "info_player_intermission" ) )
    {
      if( !bAllowed[ BA_NONE ] && !bAllowed[ BA_NUM_BUILDABLES + TEAM_NONE ] )
        continue;
      name = "ivo_spectator";
    }
    else if( ent->count == 1 && !strcmp( ent->classname, "info_alien_intermission" ) )
    {
      if( !bAllowed[ BA_NONE ] && !bAllowed[ BA_NUM_BUILDABLES + TEAM_ALIENS ] )
        continue;
      name = "ivo_alien";
    }
    else if( ent->count == 1 && !strcmp( ent->classname, "info_human_intermission" ) )
    {
      if( !bAllowed[ BA_NONE ] && !bAllowed[ BA_NUM_BUILDABLES + TEAM_HUMANS ] )
        continue;
      name = "ivo_human";
    }
    else
      continue;

    s = va( "%s %f %f %f %f %f %f %f %f %f %f %f %f\n",
      name,
      ent->r.currentOrigin[ 0 ],
      ent->r.currentOrigin[ 1 ],
      ent->r.currentOrigin[ 2 ],
      ent->r.currentAngles[ 0 ],
      ent->r.currentAngles[ 1 ],
      ent->r.currentAngles[ 2 ],
      ent->s.origin2[ 0 ],
      ent->s.origin2[ 1 ],
      ent->s.origin2[ 2 ],
      ent->s.angles2[ 0 ],
      ent->s.angles2[ 1 ],
      ent->s.angles2[ 2 ] );
    FS_Write( s, strlen( s ), f );
  }
  FS_FCloseFile( f );
}

/*
============
G_LayoutList
============
*/
int G_LayoutList( const char *map, char *list, int len )
{
  // up to 128 single character layout names could fit in layouts
  char fileList[ ( MAX_CVAR_VALUE_STRING / 2 ) * 5 ] = {""};
  char layouts[ MAX_CVAR_VALUE_STRING ] = {""};
  int numFiles, i, fileLen = 0, listLen;
  int  count = 0;
  char *filePtr;

  Q_strcat( layouts, sizeof( layouts ), "*BUILTIN* " );
  numFiles = FS_GetFileList( va( "layouts/%s", map ), ".dat",
    fileList, sizeof( fileList ) );
  filePtr = fileList;
  for( i = 0; i < numFiles; i++, filePtr += fileLen + 1 )
  {
    fileLen = strlen( filePtr );
    listLen = strlen( layouts );
    if( fileLen < 5 )
      continue;

    // list is full, stop trying to add to it
    if( ( listLen + fileLen ) >= sizeof( layouts ) )
      break;

    Q_strcat( layouts,  sizeof( layouts ), filePtr );
    listLen = strlen( layouts );

    // strip extension and add space delimiter
    layouts[ listLen - 4 ] = ' ';
    layouts[ listLen - 3 ] = '\0';
    count++;
  }
  if( count != numFiles )
  {
    Com_Printf( S_COLOR_YELLOW "WARNING: layout list was truncated to %d "
      "layouts, but %d layout files exist in layouts/%s/.\n",
      count, numFiles, map );
  }
  Q_strncpyz( list, layouts, len );
  return count + 1;
}

/*
============
G_LayoutSelect

set level.layout based on g_nextLayout, g_layouts or g_layoutAuto
============
*/
void G_LayoutSelect( void )
{
  char layouts[ ( MAX_CVAR_VALUE_STRING - 1 ) * 9 + 1 ];
  char layouts2[ ( MAX_CVAR_VALUE_STRING - 1 ) * 9 + 1 ];
  char *l;
  char map[ MAX_QPATH ];
  char *s;
  int cnt = 0;
  int layoutNum;

  Q_strncpyz( layouts, g_nextLayout.string, sizeof( layouts ) );
  if( !layouts[ 0 ] )
  {
    for( layoutNum = 0; layoutNum < 9; ++layoutNum )
      Q_strcat( layouts, sizeof( layouts ), g_layouts[ layoutNum ].string );
  }
  Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );

  // one time use cvar
  Cvar_SetSafe( "g_nextLayout", "" );

  // pick an included layout at random if no list has been provided
  if( !layouts[ 0 ] && g_layoutAuto.integer )
  {
    G_LayoutList( map, layouts, sizeof( layouts ) );
  }

  if( !layouts[ 0 ] )
    return;

  Q_strncpyz( layouts2, layouts, sizeof( layouts2 ) );
  l = &layouts2[ 0 ];
  layouts[ 0 ] = '\0';
  while( 1 )
  {
    s = COM_ParseExt( &l, qfalse );
    if( !*s )
      break;

    if( strchr( s, '+' ) || strchr( s, '|' ) || G_LayoutExists( map, s ) )
    {
      Q_strcat( layouts, sizeof( layouts ), s );
      Q_strcat( layouts, sizeof( layouts ), " " );
      cnt++;
    }
    else
      Com_Printf( S_COLOR_YELLOW "WARNING: layout \"%s\" does not exist\n", s );
  }
  if( !cnt )
  {
      Com_Printf( S_COLOR_RED "ERROR: none of the specified layouts could be "
        "found, using map default\n" );
      return;
  }
  layoutNum = rand( ) / ( RAND_MAX / cnt + 1 ) + 1;
  cnt = 0;

  Q_strncpyz( layouts2, layouts, sizeof( layouts2 ) );
  l = &layouts2[ 0 ];
  while( 1 )
  {
    s = COM_ParseExt( &l, qfalse );
    if( !*s )
      break;

    Q_strncpyz( level.layout, s, sizeof( level.layout ) );
    cnt++;
    if( cnt >= layoutNum )
      break;
  }
  Com_Printf( "using layout \"%s\" from list (%s)\n", level.layout, layouts );
}

/*
============
G_LayoutBuildItem
============
*/
static void G_LayoutBuildItem( buildable_t buildable, vec3_t origin,
  vec3_t angles, vec3_t origin2, vec3_t angles2 )
{
  gentity_t *builder;

  builder = G_Spawn( );
  builder->classname = "builder";
  VectorCopy( origin, builder->r.currentOrigin );
  VectorCopy( angles, builder->r.currentAngles );
  VectorCopy( origin2, builder->s.origin2 );
  VectorCopy( angles2, builder->s.angles2 );
  G_SpawnBuildable( builder, buildable );
}

static void G_SpawnIntermissionViewOverride( char *cn, vec3_t origin, vec3_t angles )
{
  gentity_t *spot = G_Find( NULL, FOFS( classname ), cn );
  if( !spot )
  {
    spot = G_Spawn();
    spot->classname = cn;
  }
  spot->count = 1;

  VectorCopy( origin, spot->r.currentOrigin );
  VectorCopy( angles, spot->r.currentAngles );
}

/*
============
G_LayoutLoad
============
*/
void G_LayoutLoad( char *lstr )
{
  char *lstrPlusPtr, *lstrPipePtr;
  qboolean bAllowed[ BA_NUM_BUILDABLES + NUM_TEAMS ];
  fileHandle_t f;
  int len;
  char *layout, *layoutHead;
  char map[ MAX_QPATH ];
  char buildName[ MAX_TOKEN_CHARS ];
  int buildable;
  vec3_t origin = { 0.0f, 0.0f, 0.0f };
  vec3_t angles = { 0.0f, 0.0f, 0.0f };
  vec3_t origin2 = { 0.0f, 0.0f, 0.0f };
  vec3_t angles2 = { 0.0f, 0.0f, 0.0f };
  char line[ MAX_STRING_CHARS ];
  int i;

  if( !lstr[ 0 ] || !Q_stricmp( lstr, "*BUILTIN*" ) )
    return;

  loadAnotherLayout:
  lstrPlusPtr = strchr( lstr, '+' );
  if( lstrPlusPtr )
    *lstrPlusPtr = '\0';

  if( ( lstrPipePtr = strchr( lstr, '|' ) ) )
  {
    int bList[ BA_NUM_BUILDABLES + NUM_TEAMS ];
    *lstrPipePtr = '\0';
    G_ParseCSVBuildablePlusList( lstr, &bList[ 0 ], sizeof( bList ) / sizeof( bList[ 0 ] ) );
    memset( bAllowed, 0, sizeof( bAllowed ) );
    for( i = 0; bList[ i ] != BA_NONE; i++ )
      bAllowed[ bList[ i ] ] = qtrue;
    *lstrPipePtr = '|';
    lstr = lstrPipePtr + 1;
  }
  else
    bAllowed[ BA_NONE ] = qtrue; // allow all

  Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
  len = FS_FOpenFileByMode( va( "layouts/%s/%s.dat", map, lstr ),
    &f, FS_READ );
  if( len < 0 )
  {
    Com_Printf( "ERROR: layout %s could not be opened\n", lstr );
    return;
  }
  layoutHead = layout = BG_StackPoolAlloc( len + 1 );
  FS_Read2( layout, len, f );
  layout[ len ] = '\0';
  FS_FCloseFile( f );
  i = 0;
  while( *layout )
  {
    if( i >= sizeof( line ) - 1 )
    {
      Com_Printf( S_COLOR_RED "ERROR: line overflow in %s before \"%s\"\n",
       va( "layouts/%s/%s.dat", map, lstr ), line );
      break;
    }
    line[ i++ ] = *layout;
    line[ i ] = '\0';
    if( *layout == '\n' )
    {
      i = 0;
      sscanf( line, "%s %f %f %f %f %f %f %f %f %f %f %f %f\n",
        buildName,
        &origin[ 0 ], &origin[ 1 ], &origin[ 2 ],
        &angles[ 0 ], &angles[ 1 ], &angles[ 2 ],
        &origin2[ 0 ], &origin2[ 1 ], &origin2[ 2 ],
        &angles2[ 0 ], &angles2[ 1 ], &angles2[ 2 ] );

      buildable = BG_BuildableByName( buildName )->number;
      if( !Q_stricmp( buildName, "ivo_spectator" ) )
      {
        if( bAllowed[ BA_NONE ] || bAllowed[ BA_NUM_BUILDABLES + TEAM_NONE ] )
          G_SpawnIntermissionViewOverride( "info_player_intermission", origin, angles );
      }
      else if( !Q_stricmp( buildName, "ivo_alien" ) )
      {
        if( bAllowed[ BA_NONE ] || bAllowed[ BA_NUM_BUILDABLES + TEAM_ALIENS ] )
          G_SpawnIntermissionViewOverride( "info_alien_intermission", origin, angles );
      }
      else if( !Q_stricmp( buildName, "ivo_human" ) )
      {
        if( bAllowed[ BA_NONE ] || bAllowed[ BA_NUM_BUILDABLES + TEAM_HUMANS ] )
          G_SpawnIntermissionViewOverride( "info_human_intermission", origin, angles );
      }
      else if( buildable <= BA_NONE || buildable >= BA_NUM_BUILDABLES )
        Com_Printf( S_COLOR_YELLOW "WARNING: bad buildable name (%s) in layout."
          " skipping\n", buildName );
      else
      {
        if( bAllowed[ BA_NONE ] || bAllowed[ buildable ] )
          G_LayoutBuildItem( buildable, origin, angles, origin2, angles2 );
      }
    }
    layout++;
  }
  BG_StackPoolFree( layoutHead );

  if( lstrPlusPtr )
  {
    *lstrPlusPtr = '+';
    lstr = lstrPlusPtr + 1;
    goto loadAnotherLayout;
  }
}

/*
============
G_BaseSelfDestruct
============
*/
void G_BaseSelfDestruct( team_t team )
{
  int       i;
  gentity_t *ent;

  for( i = MAX_CLIENTS; i < level.num_entities; i++ )
  {
    ent = &level.gentities[ i ];
    if( ent->health <= 0 )
      continue;
    if( ent->s.eType != ET_BUILDABLE )
      continue;
    if( ent->buildableTeam != team )
      continue;
    G_Damage( ent, NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_SUICIDE );
  }
}

/*
============
build log
============
*/
buildLog_t *G_BuildLogNew( gentity_t *actor, buildFate_t fate )
{
  buildLog_t *log = &level.buildLog[ level.buildId++ % MAX_BUILDLOG ];

  if( level.numBuildLogs < MAX_BUILDLOG )
    level.numBuildLogs++;
  log->time = level.time;
  log->fate = fate;
  if( actor && actor->s.eType == ET_BUILDABLE )
  {
    log->actor = actor->builtBy;
    log->attackerBuildable = actor->s.modelindex;
  }
  else
    log->actor = actor && actor->client ? actor->client->pers.namelog : NULL;
  return log;
}

void G_BuildLogSet( buildLog_t *log, gentity_t *ent )
{
  log->modelindex = ent->s.modelindex;
  log->deconstruct = ent->deconstruct;
  log->deconstructTime = ent->deconstructTime;
  log->builtBy = ent->builtBy;
  VectorCopy( ent->r.currentOrigin, log->origin );
  VectorCopy( ent->r.currentAngles, log->angles );
  VectorCopy( ent->s.origin2, log->origin2 );
  VectorCopy( ent->s.angles2, log->angles2 );
  log->powerSource = ent->parentNode ? ent->parentNode->s.modelindex : BA_NONE;
  log->powerValue = G_QueueValue( ent );
  if( log->fate == BF_CONSTRUCT )
    ent->buildLog = log;
}

void G_BuildLogAuto( gentity_t *actor, gentity_t *buildable, buildFate_t fate )
{
  G_BuildLogSet( G_BuildLogNew( actor, fate ), buildable );
}

void G_BuildLogRevertThink( gentity_t *ent )
{
  gentity_t *built;
  vec3_t    mins, maxs;
  int       blockers[ MAX_GENTITIES ];
  int       num;
  int       victims = 0;
  int       i;

  if( ent->suicideTime > 0 )
  {
    BG_BuildableBoundingBox( ent->s.modelindex, mins, maxs );
    VectorAdd( ent->s.pos.trBase, mins, mins );
    VectorAdd( ent->s.pos.trBase, maxs, maxs );
    num = SV_AreaEntities( mins, maxs, blockers, MAX_GENTITIES );
    for( i = 0; i < num; i++ )
    {
      gentity_t *targ;
      vec3_t    push;

      targ = g_entities + blockers[ i ];
      if( targ->client )
      {
        float val = ( targ->client->ps.eFlags & EF_WALLCLIMB) ? 300.0 : 150.0;

        VectorSet( push, crandom() * val, crandom() * val, random() * val );
        VectorAdd( targ->client->ps.velocity, push, targ->client->ps.velocity );
        victims++;
      }
    }

    ent->suicideTime--;

    if( victims )
    {
      // still a blocker
      ent->nextthink = level.time + FRAMETIME;
      return;
    }
  }

  built = G_FinishSpawningBuildable( ent, qtrue );
  built->buildTime = built->s.time = 0;
  G_KillBox( built );

  G_LogPrintf( "revert: restore %d %s\n",
    (int)( built - g_entities ), BG_Buildable( built->s.modelindex )->name );

  G_BuildableThink( built, 0 );
}

void G_BuildLogRevert( int id )
{
  buildLog_t *log;
  gentity_t  *ent;
  int        i;
  vec3_t     dist;

  level.numBuildablesForRemoval = 0;

  level.numBuildLogs -= level.buildId - id;
  while( level.buildId > id )
  {
    log = &level.buildLog[ --level.buildId % MAX_BUILDLOG ];
    if( log->fate == BF_CONSTRUCT )
    {
      for( i = MAX_CLIENTS; i < level.num_entities; i++ )
      {
        ent = &g_entities[ i ];
        if( ( ( ent->s.eType == ET_BUILDABLE &&
                ent->health > 0 ) ||
              ( ent->s.eType == ET_GENERAL &&
                ent->think == G_BuildLogRevertThink ) ) &&
            ent->s.modelindex == log->modelindex )
        {
          VectorSubtract( ent->s.pos.trBase, log->origin, dist );
          if( VectorLengthSquared( dist ) <= 2.0f )
          {
            if( ent->s.eType == ET_BUILDABLE )
              G_LogPrintf( "revert: remove %d %s\n",
                           (int)( ent - g_entities ),
                           BG_Buildable( ent->s.modelindex )->name );
            G_RemoveRangeMarkerFrom( ent );
            if( ( ent->flags & FL_OCCUPIED ) && ent->occupation.occupant )
              G_UnoccupyEnt( ent, ent->occupation.occupant,
                             ent->occupation.occupant, qtrue );
            G_FreeEntity( ent );
            break;
          }
        }
      }
    }
    else
    {
      gentity_t  *builder = G_Spawn();
      builder->classname = "builder";

      VectorCopy( log->origin, builder->r.currentOrigin );
      VectorCopy( log->angles, builder->r.currentAngles );
      VectorCopy( log->origin2, builder->s.origin2 );
      VectorCopy( log->angles2, builder->s.angles2 );
      builder->s.modelindex = log->modelindex;
      builder->deconstruct = log->deconstruct;
      builder->deconstructTime = log->deconstructTime;
      builder->builtBy = log->builtBy;

      builder->think = G_BuildLogRevertThink;
      builder->nextthink = level.time + FRAMETIME;

      // Number of thinks before giving up and killing players in the way
      builder->suicideTime = 30;

      if( log->fate == BF_DESTROY || log->fate == BF_TEAMKILL )
      {
        int value = log->powerValue;

        if( BG_Buildable( log->modelindex )->team == TEAM_ALIENS )
        {
          if(
            G_TimeTilSuddenDeath() <= 0 &&
            G_SD_Mode( ) == SDMODE_SELECTIVE) {
            if(
              level.sudden_death_replacable[log->modelindex] &&
              !G_FindBuildable(log->modelindex)) {
              level.alienBuildPointQueue =
                MAX(
                  0,
                  level.alienBuildPointQueue -
                    value );
            }
          } else {
            level.alienBuildPointQueue =
              MAX( 0, level.alienBuildPointQueue - value );
          }
          
        }
        else
        {
          level.humanBuildPointQueue =
            MAX( 0, level.humanBuildPointQueue - value );
        }
      }
    }
  }
}

/*
================
G_UpdateBuildableRangeMarkers

Ensures that range markers for power sources can be seen through walls
================
*/
void G_UpdateBuildableRangeMarkers( void )
{
  gentity_t *e;
  for( e = &g_entities[ MAX_CLIENTS ]; e < &g_entities[ level.num_entities ]; ++e )
  {
    buildable_t bType;
    team_t bTeam;
    int i;

    if( e->s.eType != ET_BUILDABLE )
      continue;

    if( !e->rangeMarker )
      continue;

    bType = e->s.modelindex;
    bTeam = BG_Buildable( bType )->team;

    e->r.singleClient = 0;
    e->r.hack.generic1 = 0;

    for( i = 0; i < level.maxclients; ++i )
    {
      gclient_t *client;
      team_t team;
      qboolean weaponDisplays, wantsToSee;

      client = &level.clients[ i ];
      if( client->pers.connected != CON_CONNECTED )
        continue;

      team = client->pers.teamSelection;
      weaponDisplays = ( ( BG_GetPlayerWeapon( &client->ps ) == WP_HBUILD ) ||
            client->ps.weapon == WP_ABUILD || client->ps.weapon == WP_ABUILD2 );
      wantsToSee = ( client->pers.buildableRangeMarkerMask & ( 1 << bType ) );

      if( ( team == bTeam ) &&
          weaponDisplays && 
          wantsToSee &&
          (BG_Buildable( bType )->role & ROLE_PERVASIVE) )
      {
        if( i >= 32 )
          e->r.hack.generic1 |= 1 << ( i - 32 );
        else
          e->r.singleClient |= 1 << i;
      }
    }

    SV_LinkEntity( e );
  }
}
