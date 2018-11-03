/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2018 GrangerHub

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// g_weapon.c
// perform the server side effects of a weapon firing

#include "g_local.h"

static  vec3_t  forward, right, up;
static  vec3_t  muzzle;

/*
================
Blow_up
================
*/
void Blow_up( gentity_t *ent )
{
  // set directions
  AngleVectors( ent->client->ps.viewangles, forward, right, up );
  BG_CalcMuzzlePointFromPS( &ent->client->ps, forward, right, up, muzzle );

  launch_grenade2( ent, muzzle, forward );
}

/*
================
G_ForceWeaponChange
================
*/
void G_ForceWeaponChange( gentity_t *ent, weapon_t weapon )
{
  playerState_t *ps = &ent->client->ps;

  // stop a reload in progress
  if( ps->weaponstate == WEAPON_RELOADING )
  {
    ps->torsoAnim = ( ( ps->torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | TORSO_RAISE;
    ps->weaponTime = 250;
    ps->weaponstate = WEAPON_READY;
  }

  ps->persistant[ PERS_NEWWEAPON ] =  ( weapon == WP_HBUILD ) ? WP_HBUILD :
    ( ( weapon == WP_BLASTER ) ?
      WP_BLASTER : ( ps->stats[ STAT_WEAPON ] != WP_NONE ?
                           ps->stats[ STAT_WEAPON ] : WP_BLASTER ) );

  // force this here to prevent flamer effect from continuing
  ps->generic1 = WPM_NOTFIRING;

  // The PMove will do an animated drop, raise, and set the new weapon
  ps->pm_flags |= PMF_WEAPON_SWITCH;
}

/*
=================
G_CanGiveClientMaxAmmo
=================
*/
qboolean G_CanGiveClientMaxAmmo( gentity_t *ent, qboolean buyingEnergyAmmo,
                                 int *rounds, int *clips, int *price )
{
  weapon_t weapon = ent->client->ps.stats[ STAT_WEAPON ];
  int maxAmmo = BG_Weapon( weapon )->maxAmmo;
  int maxClips = BG_Weapon( weapon )->maxClips;
  int roundDiff;
  int clipDiff = BG_Weapon( weapon )->maxClips - ent->client->ps.clips;
  int roundPrice = BG_Weapon( weapon )->roundPrice;

  Com_Assert( rounds &&
              "G_CanGiveClientMaxAmmo: rounds is NULL" );
  Com_Assert( clips &&
              "G_CanGiveClientMaxAmmo: clips is NULL" );
  Com_Assert( price &&
              "G_CanGiveClientMaxAmmo: price is NULL" );

  *price = 0;
  *rounds = 0;
  *clips = 0;

  if( BG_Weapon( weapon )->infiniteAmmo )
    return qfalse;

  if( !BG_Weapon( weapon )->ammoPurchasable )
    return qfalse;

  if( buyingEnergyAmmo && !BG_Weapon( weapon )->usesEnergy )
    return qfalse;

  if( BG_WeaponIsFull( weapon, ent->client->ps.stats, ent->client->ps.ammo,
                       ent->client->ps.clips ) )
    return qfalse;

  // Apply battery pack modifier
  if( BG_Weapon( weapon )->usesEnergy &&
      ( BG_InventoryContainsUpgrade( UP_BATTPACK, ent->client->ps.stats ) ||
        BG_InventoryContainsUpgrade( UP_BATTLESUIT, ent->client->ps.stats ) ) )
  {
    maxAmmo *= BATTPACK_MODIFIER;
  }

  // Apply clip capacity modification
  if( !BG_Weapon( weapon )->usesEnergy &&
        !BG_Weapon( weapon )->infiniteAmmo &&
        BG_Weapon( weapon )->ammoPurchasable &&
        !BG_Weapon( weapon )->roundPrice &&
        BG_InventoryContainsUpgrade( UP_BATTLESUIT, ent->client->ps.stats ) )
      maxClips += maxClips;

  roundDiff = maxAmmo - ent->client->ps.ammo;

  // charge for ammo when applicable
  if(  !( IS_WARMUP && BG_Weapon( weapon )->warmupFree ) &&
       roundPrice && !BG_Weapon( weapon )->usesEnergy )
  {
    int clipPrice = roundPrice * maxClips;
    int numAffordableRounds, numAffordableClips;

    if( roundPrice > ent->client->pers.credit )
    {
      SV_GameSendServerCommand( ent-g_entities,
                              va( "print \"You can't afford to buy more ammo for this %s\n\"",
                                  BG_Weapon( weapon )->humanName ) );
      return qfalse;
    }

    // buy ammo rounds thatcan be afforded
    numAffordableRounds = ent->client->pers.credit / roundPrice;
    if( numAffordableRounds < roundDiff )
      *rounds = numAffordableRounds;
    else
      *rounds =  roundDiff;

    *price = ( *rounds ) * roundPrice;

    // buy ammo clips that can be afforded
    if( clipPrice && ( numAffordableClips = ent->client->pers.credit / clipPrice ) )
    {
      if( numAffordableClips < clipDiff )
        *clips = numAffordableClips;
      else
        *clips = clipDiff;

      *price += (  ( *clips ) * clipPrice );
    }
  } else
  {
    *price = 0;
    *rounds =  roundDiff;
    *clips = clipDiff;
  }

  return qtrue;
}

/*
=================
G_GiveClientMaxAmmo
=================
*/
void G_GiveClientMaxAmmo( gentity_t *ent, qboolean buyingEnergyAmmo )
{
  weapon_t weapon = ent->client->ps.stats[ STAT_WEAPON ];
  int rounds, clips, price;
  int maxAmmo = BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->maxAmmo;

  if( !G_CanGiveClientMaxAmmo( ent, buyingEnergyAmmo,
                               &rounds, &clips, &price ) )
    return;

  G_AddCreditToClient( ent->client, -(short)( price ), qfalse );

  ent->client->ps.ammo += rounds;

  // Apply battery pack modifier
  if( BG_Weapon( weapon )->usesEnergy &&
      ( BG_InventoryContainsUpgrade( UP_BATTPACK, ent->client->ps.stats ) ||
        BG_InventoryContainsUpgrade( UP_BATTLESUIT, ent->client->ps.stats ) ) ) {
    maxAmmo *= BATTPACK_MODIFIER;
  }

  if( ent->client->ps.ammo > maxAmmo )
    ent->client->ps.ammo = maxAmmo;

  ent->client->ps.clips += clips;
  if( ent->client->ps.clips > BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->maxClips )
    ent->client->ps.clips = BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->maxClips;

  G_ForceWeaponChange( ent, ent->client->ps.weapon );

  ent->client->ps.pm_flags |= PMF_WEAPON_FORCE_RELOAD;

  if( BG_Weapon( weapon )->usesEnergy )
    G_AddEvent( ent, EV_RPTUSE_SOUND, 0 );
}

/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout )
{
  vec3_t v, newv;
  float dot;

  VectorSubtract( impact, start, v );
  dot = DotProduct( v, dir );
  VectorMA( v, -2 * dot, dir, newv );

  VectorNormalize(newv);
  VectorMA(impact, 8192, newv, endout);
}

/*
================
G_WideTraceSolid

Trace a bounding box against entities and the world
================
*/
static void G_WideTraceSolid(trace_t *tr, gentity_t *ent, float range,
                             float width, float height, gentity_t **target) {
  vec3_t    mins, maxs;
  vec3_t    end;
  trace_t   tr2;

  VectorSet( mins, -width, -width, -height );
  VectorSet( maxs, width, width, height );

  *target = NULL;

  if(!ent->client) {
    return;
  }

  G_UnlaggedOn(ent->s.number, muzzle, range + width);

  VectorMA(muzzle, range, forward, end);

  // Trace against entities and the world
  SV_Trace(tr, muzzle, mins, maxs, end, ent->s.number, MASK_SHOT, TT_AABB);
  if(tr->entityNum != ENTITYNUM_NONE) {
    *target = &g_entities[ tr->entityNum ];
  }

  if(tr->startsolid) {
    //check for collision against the world
    SV_Trace( &tr2, muzzle, mins, maxs, muzzle, ent->s.number, MASK_SOLID, TT_AABB );
    if(tr2.entityNum != ENTITYNUM_NONE) {
      *target = &g_entities[ tr2.entityNum ];
    }
  }

  G_UnlaggedOff( );
}

/*
================
G_WideTraceSolidSeries

Uses a series of enlarging traces starting with a line trace.
================
*/
static void G_WideTraceSolidSeries(trace_t *tr, gentity_t *ent, float range,
                                    float width, float height, gentity_t **target) {
  int n;
  float widthAdjusted, heightAdjusted;

  for(n = 0; n < 5; ++n) {
    widthAdjusted = (width * (float)(n)) / 5.00f;
    heightAdjusted = (height * (float)(n)) / 5.00f;

    G_WideTraceSolid(tr, ent, range, widthAdjusted, heightAdjusted, target);
    if(
      tr->startsolid ||
      (*target != NULL && G_TakesDamage(*target))) {
      return;
    }
  }

  G_WideTraceSolid(tr, ent, range, width, height, target);
}

/*
======================
SnapVectorTowards
SnapVectorNormal

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating
into a wall.
======================
*/
void SnapVectorTowards( vec3_t v, vec3_t to )
{
  int   i;

  for( i = 0 ; i < 3 ; i++ )
  {
    if( v[ i ] >= 0 )
      v[ i ] = (int)( v[ i ] + ( to[ i ] <= v[ i ] ? 0 : 1 ) );
    else
      v[ i ] = (int)( v[ i ] + ( to[ i ] <= v[ i ] ? -1 : 0 ) );
  }
}

void SnapVectorNormal( vec3_t v, vec3_t normal )
{
  int i;

  for( i = 0 ; i < 3 ; i++ )
  {
    if( v[ i ] >= 0 )
      v[ i ] = (int)( v[ i ] + ( normal[ i ] <= 0 ? 0 : 1 ) );
    else
      v[ i ] = (int)( v[ i ] + ( normal[ i ] <= 0 ? -1 : 0 ) );
  }
}

/*
===============
BloodSpurt

Generates a blood spurt event for traces with accurate end points
===============
*/
static void BloodSpurt( gentity_t *attacker, gentity_t *victim, trace_t *tr )
{
  gentity_t *tent;

  if( !attacker->client )
    return;

  if( victim->health <= 0 )
    return;

  tent = G_TempEntity( tr->endpos, EV_MISSILE_HIT );
  tent->s.otherEntityNum = victim->s.number;
  tent->s.eventParm = DirToByte( tr->plane.normal );
  tent->s.weapon = attacker->s.weapon;
  tent->s.generic1 = attacker->s.generic1; // weaponMode
}

/*
===============
WideBloodSpurt

Calculates the position of a blood spurt for wide traces and generates an event
===============
*/
static void WideBloodSpurt( gentity_t *attacker, gentity_t *victim, trace_t *tr )
{
  gentity_t *tent;
  vec3_t normal, origin;
  float mag, radius;

  if( !attacker->client )
    return;

  if( victim->health <= 0 &&
      victim->s.eType == ET_BUILDABLE )
    return;

  if( tr )
    VectorSubtract( tr->endpos, victim->r.currentOrigin, normal );
  else
    VectorSubtract( attacker->client->ps.origin,
                    victim->r.currentOrigin, normal );

  // Normalize the horizontal components of the vector difference to the
  // "radius" of the bounding box
  mag = sqrt( normal[ 0 ] * normal[ 0 ] + normal[ 1 ] * normal[ 1 ] );
  radius = victim->r.maxs[ 0 ] * 1.21f;
  if( mag > radius )
  {
    normal[ 0 ] = normal[ 0 ] / mag * radius;
    normal[ 1 ] = normal[ 1 ] / mag * radius;
  }

  // Clamp origin to be within bounding box vertically
  if( normal[ 2 ] > victim->r.maxs[ 2 ] )
    normal[ 2 ] = victim->r.maxs[ 2 ];
  if( normal[ 2 ] < victim->r.mins[ 2 ] )
    normal[ 2 ] = victim->r.mins[ 2 ];

  VectorAdd( victim->r.currentOrigin, normal, origin );
  VectorNegate( normal, normal );
  VectorNormalize( normal );

  // Create the blood spurt effect entity
  tent = G_TempEntity( origin, EV_MISSILE_HIT );
  tent->s.eventParm = DirToByte( normal );
  tent->s.otherEntityNum = victim->s.number;
  tent->s.weapon = attacker->s.weapon;
  tent->s.generic1 = attacker->s.generic1; // weaponMode
}

/*
===============
meleeAttack
===============
*/

void meleeAttack( gentity_t *ent, float range, float width, float height,
                  int damage, meansOfDeath_t mod )
{
  trace_t   tr;
  gentity_t *traceEnt;

  G_WideTraceSolidSeries( &tr, ent, range, width, height, &traceEnt );
  if( traceEnt == NULL || !G_TakesDamage( traceEnt ) )
    return;

  WideBloodSpurt( ent, traceEnt, &tr );
  G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, DAMAGE_NO_KNOCKBACK, mod );
}

/*
======================================================================

MACHINEGUN

======================================================================
*/

void bulletFire( gentity_t *ent, float spread, int damage, int mod )
{
  trace_t   tr;
  vec3_t    end;
  float     t, u, r, x, y;
  gentity_t *tent;
  gentity_t *traceEnt;

  spread *= 16;

  t = 2 * M_PI * random( );
  u = random( ) + random( );
  r = u > 1? 2 - u : u;
  r *= spread;
  x = r * cos( t );
  y = r * sin( t );

  VectorMA( muzzle, 8192 * 16, forward, end );
  VectorMA( end, x, right, end );
  VectorMA( end, y, up, end );

  // don't use unlagged if this is not a client (e.g. turret)
  if( ent->client )
  {
    G_UnlaggedOn( ent->s.number, muzzle, 8192 * 16 );
    SV_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT, TT_AABB );
    G_UnlaggedOff( );
  }
  else
    SV_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT, TT_AABB );

  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  // snap the endpos to integers, but nudged towards the line
  SnapVectorTowards( tr.endpos, muzzle );

  // send bullet impact
  if( G_TakesDamage( traceEnt ) &&
      (traceEnt->s.eType == ET_PLAYER ||
       traceEnt->s.eType == ET_BUILDABLE ) )
  {
    tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
    tent->s.eventParm = traceEnt->s.number;
  }
  else
  {
    tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );
    tent->s.eventParm = DirToByte( tr.plane.normal );
  }
  tent->s.otherEntityNum = ent->s.number;

  if( G_TakesDamage( traceEnt ) )
  {
    G_Damage( traceEnt, ent, ent, forward, tr.endpos,
      damage, DAMAGE_NO_KNOCKBACK, mod );
  }
}

/*
======================================================================

SPLATTER

======================================================================
*/

typedef struct gSplatterData_s
{
  meansOfDeath_t mod;
  gentity_t      *inflicter;
  gentity_t      *attacker;
} gSplatterData_t;

/*
==============
CG_SplatterMarks
==============
*/
static void G_Splatter( splatterData_t *data ) {
  gSplatterData_t *gData = (gSplatterData_t *)data->user_data;
  const int modeIndex = data->weaponMode - 1;

  Com_Assert( modeIndex >= 0 &&
              modeIndex < 3 &&
              "G_Splatter: invalid weaponMode" );
  Com_Assert( data &&
              "G_Splatter: data is NULL" );
  Com_Assert( data->tr &&
              "G_Splatter: tr is NULL" );
  Com_Assert( gData &&
              "G_Splatter: gData is NULL" );

  if( !( data->tr->surfaceFlags & SURF_NOIMPACT ) ) {
    gentity_t *traceEnt = &g_entities[ data->tr->entityNum ];

    if( G_TakesDamage( traceEnt ) )
    {
      weapon_t weapon = data->weapon;
      vec3_t   kbDir;
      float    dmg_falloff = BG_Weapon( weapon )->splatter[modeIndex].impactDamageFalloff;
      float    dmg_cap = BG_Weapon( weapon )->splatter[modeIndex].impactDamageCap;
      int      damage = BG_Weapon( weapon )->splatter[modeIndex].impactDamage;

      if( dmg_falloff ) {
        float distance = Distance( data->origin, data->tr->endpos );

        damage = (int)ceil( ( (float)damage ) *
                            ( 1 - ( distance / dmg_falloff ) ) );
        damage = MIN( damage, dmg_cap);

        if( damage <= 0 ) {
          damage = 1;
        }
      }

      VectorSubtract( data->tr->endpos, data->origin, kbDir );
      G_Damage( traceEnt, gData->inflicter, gData->attacker, kbDir, data->tr->endpos,
                damage,
                0, gData->mod );
    }
  }
}

/*
==============
G_SplatterFire
==============
*/
void G_SplatterFire( gentity_t *inflicter, gentity_t *attacker,
                     vec3_t origin, vec3_t dir,
                     weapon_t weapon, weaponMode_t weaponMode, meansOfDeath_t mod ) {
  splatterData_t  data;
  gSplatterData_t gData;
  const int modeIndex = weaponMode - 1;
  gentity_t       *tent;

  Com_Assert( modeIndex >= 0 &&
              modeIndex < 3 &&
              "BG_SplatterPattern: invalid weaponMode" );

  if( BG_Weapon( weapon )->splatter[ modeIndex ].number <= 0 )
    return;

  Com_Assert( inflicter &&
              "G_SplatterFire: inflicter is NULL" );
  Com_Assert( attacker &&
              "G_SplatterFire: attacker is NULL" );
  Com_Assert( origin &&
              "G_SplatterFire: origin is NULL" );
  Com_Assert( dir &&
              "G_SplatterFire: dir is NULL" );

  // send splatter
  tent = G_TempEntity( origin, EV_SPLATTER );
  VectorNormalize2( dir, tent->s.origin2 );
  VectorCopy( tent->s.pos.trBase, data.origin );
  tent->s.eventParm = rand() / ( RAND_MAX / 0x100 + 1 );    // seed for spread pattern
  tent->s.otherEntityNum = inflicter->s.number;
  data.weapon = tent->s.angles2[0] = weapon;
  data.weaponMode = tent->s.angles2[1] = weaponMode;
  gData.inflicter = inflicter;
  gData.attacker = attacker;
  gData.mod = mod;
  data.user_data = &gData;

  BG_SplatterPattern( tent->s.origin2, tent->s.eventParm, tent->s.otherEntityNum,
                      &data, G_Splatter, G_TraceWrapper );
}

/*
======================================================================

SHOTGUN

======================================================================
*/
void shotgunFire( gentity_t *ent )
{
  G_UnlaggedOn( ent->s.number, muzzle, SHOTGUN_RANGE );
  G_SplatterFire( ent, ent,
                       muzzle, forward,
                       ent->s.weapon, ent->s.generic1, MOD_SHOTGUN );
  G_UnlaggedOff();
}

/*
======================================================================

MASS DRIVER

======================================================================
*/

void massDriverFire( gentity_t *ent )
{
  trace_t tr;
  vec3_t hitPoints[ MDRIVER_MAX_HITS ], hitNormals[ MDRIVER_MAX_HITS ],
         origin, originToEnd, muzzleToEnd, muzzleToOrigin, end;
  gentity_t *traceEnts[ MDRIVER_MAX_HITS ], *traceEnt, *tent;
  float length_offset;
  int i, hits = 0, skipent;

  // loop through all entities hit by a line trace
  G_UnlaggedOn( ent->s.number, muzzle, 8192.0f * 16.0f );
  VectorMA( muzzle, 8192 * 16, forward, end );
  VectorCopy( muzzle, tr.endpos );
  skipent = ent->s.number;
  for( i = 0; i < MDRIVER_MAX_HITS && skipent != ENTITYNUM_NONE; i++ )
  {
    SV_Trace( &tr, tr.endpos, NULL, NULL, end, skipent, MASK_SHOT, TT_AABB );
    if( tr.surfaceFlags & SURF_NOIMPACT )
      break;
    traceEnt = &g_entities[ tr.entityNum ];
    skipent = tr.entityNum;
    if( traceEnt->s.eType == ET_PLAYER )
    {
      // don't travel through teammates with FF off
      if( OnSameTeam( ent, traceEnt ) && !g_friendlyFire.integer ) //|| !g_friendlyFireHumans.integer ) )
        skipent = ENTITYNUM_NONE;
    }
    else if( traceEnt->s.eType == ET_BUILDABLE )
    {
      // don't travel through team buildables with FF off
      if( traceEnt->buildableTeam == ent->client->pers.teamSelection &&
          !g_friendlyBuildableFire.integer )
        skipent = ENTITYNUM_NONE;
    }
    else
      skipent = ENTITYNUM_NONE;

    // save the hit entity, position, and normal
    VectorCopy( tr.endpos, hitPoints[ hits ] );
    VectorCopy( tr.plane.normal, hitNormals[ hits ] );
    SnapVectorNormal( hitPoints[ hits ], tr.plane.normal );
    traceEnts[ hits++ ] = traceEnt;
  }

  // originate trail line from the gun tip, not the head!
  VectorCopy( muzzle, origin );
  VectorMA( origin, -6, up, origin );
  VectorMA( origin, 4, right, origin );
  VectorMA( origin, 24, forward, origin );

  // save the final position
  VectorCopy( tr.endpos, end );
  VectorSubtract( end, origin, originToEnd );
  VectorNormalize( originToEnd );

  // origin is further in front than muzzle, need to adjust length
  VectorSubtract( origin, muzzle, muzzleToOrigin );
  VectorSubtract( end, muzzle, muzzleToEnd );
  VectorNormalize( muzzleToEnd );
  length_offset = DotProduct( muzzleToEnd, muzzleToOrigin );


  // now that the trace is finished, we know where we stopped and can generate
  // visually correct impact locations
  for( i = 0; i < hits; i++ )
  {
    vec3_t muzzleToPos;
    float length;

    // restore saved values
    VectorCopy( hitPoints[ i ], tr.endpos );
    VectorCopy( hitNormals[ i ], tr.plane.normal );
    traceEnt = traceEnts[ i ];

    // compute the visually correct impact point
    VectorSubtract( tr.endpos, muzzle, muzzleToPos );
    length = VectorLength( muzzleToPos ) - length_offset;
    VectorMA( origin, length, originToEnd, tr.endpos );

    // send impact
    if( G_TakesDamage( traceEnt ) && traceEnt->client )
      BloodSpurt( ent, traceEnt, &tr );
    else if( i < hits - 1 )
    {
      tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
      tent->s.eventParm = DirToByte( tr.plane.normal );
      tent->s.weapon = ent->s.weapon;
      tent->s.generic1 = ent->s.generic1; // weaponMode
    }

    if( G_TakesDamage( traceEnt ) )
      G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                MDRIVER_DMG/(i+1), 0, MOD_MDRIVER );
  }

  // create an event entity for the trail, doubles as an impact event
  SnapVectorNormal( end, tr.plane.normal );
  tent = G_TempEntity( end, EV_MASS_DRIVER );
  tent->s.eventParm = DirToByte( tr.plane.normal );
  tent->s.weapon = ent->s.weapon;
  tent->s.generic1 = ent->s.generic1; // weaponMode
  VectorCopy( origin, tent->s.origin2 );

  G_UnlaggedOff( );
}


/*
======================================================================

LOCKBLOB

======================================================================
*/

void lockBlobLauncherFire( gentity_t *ent )
{
  fire_lockblob( ent, muzzle, forward );
}

/*
======================================================================

HIVE

======================================================================
*/

void hiveFire( gentity_t *ent )
{
  vec3_t origin;

  // Fire from the hive tip, not the center
  VectorMA( muzzle, ent->r.maxs[ 2 ], ent->s.origin2, origin );

  fire_hive( ent, origin, forward );
}

/*
======================================================================

BLASTER PISTOL

======================================================================
*/

void blasterFire( gentity_t *ent )
{
  fire_blaster( ent, muzzle, forward );
}

/*
======================================================================

PULSE RIFLE

======================================================================
*/

void pulseRifleFire( gentity_t *ent )
{
  fire_pulseRifle( ent, muzzle, forward );
}

/*
======================================================================

FLAME THROWER

======================================================================
*/

void flamerFire( gentity_t *ent )
{
  vec3_t origin;

  // Correct muzzle so that the missile does not start in the ceiling
  VectorMA( muzzle, -7.0f, up, origin );

  // Correct muzzle so that the missile fires from the player's hand
  VectorMA( origin, 4.5f, right, origin );

  fire_flamer( ent, origin, forward );
}

/*
======================================================================

GRENADE

======================================================================
*/

void throwGrenade( gentity_t *ent )
{
  launch_grenade( ent, muzzle, forward );
}

/*
======================================================================

FRAGMENTATION GRENADE

======================================================================
*/

void throwFragnade( gentity_t *ent )
{
  launch_fragnade( ent, muzzle, forward );
}

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void launcherFire( gentity_t *ent, qboolean impact )
{
  launch_grenade3( ent, muzzle, forward, impact );
}

/*
======================================================================

Lightning Gun

======================================================================
*/

/*
===============
lightningBallFire
===============
*/
void lightningBallFire( gentity_t *ent )
{
  gentity_t *missile;
  gclient_t *client = ent ->client;

  Com_Assert( client && "lightningBallFire: Can only be used by clients" );

  Com_Assert( client->firedBallLightningNum < LIGHTNING_BALL_BURST_ROUNDS &&
              "lightningBallFire: attempted to exceed the ball lightning burst limit" );

  missile = fire_lightningBall( ent, qfalse, muzzle, forward );

  //add to the client's ball lightning array
  G_Entity_id_set( &client->firedBallLightning[ client->firedBallLightningNum ],
                   missile );
  client->firedBallLightningNum++;

  // damage self if in contact with water
  if( ent->waterlevel )
    G_Damage( ent, ent, ent, NULL, NULL,
              LIGHTNING_BALL_DAMAGE, 0, MOD_LIGHTNING);
}

/*
===============
lightningEMPFire
===============
*/
void lightningEMPFire( gentity_t *ent )
{
  int i;
  gclient_t *client = ent->client;

  Com_Assert( client && "lightningEMPFire: Can only be used by clients" );

  for( i = 0; i < LIGHTNING_BALL_BURST_ROUNDS; i++ )
  {
    gentity_t *missile = G_Entity_id_get( &client->firedBallLightning[ i ] );

    if( missile &&
        missile->s.eType == ET_MISSILE &&
        !strcmp( missile->classname, "lightningBall" ) )
      missile->die( missile, ent, ent, missile->health, MOD_LIGHTNING_EMP );
  }

  client->firedBallLightningNum = 0;

  fire_lightningBall( ent, qtrue, muzzle, forward );
}

/*
===============
lightningBoltFire
===============
*/
void lightningBoltFire( gentity_t *ent )
{
	trace_t		*tr = &ent->client->pmext.impactTriggerTrace;
	gentity_t	*traceEnt, *tent;
  int       damage = ( LIGHTNING_BOLT_DPS * ent->client->ps.misc[ MISC_MISC ] ) / 1000;

  // damage self if in contact with water
  if( ent->waterlevel )
    G_Damage( ent, ent, ent, NULL, NULL, damage, 0, MOD_LIGHTNING);

  BG_ResetLightningBoltCharge( &ent->client->ps, &ent->client->pmext);

	if( tr->entityNum == ENTITYNUM_NONE )
		return;

	traceEnt = &g_entities[ tr->entityNum ];

  if( traceEnt->s.groundEntityNum == ENTITYNUM_NONE &&
      !( traceEnt->client && traceEnt->waterlevel ) )
    damage = (int)( damage * LIGHTNING_BOLT_NOGRND_DMG_MOD );

	if( G_TakesDamage( traceEnt ))
    G_Damage( traceEnt, ent, ent, forward, tr->endpos,
              damage, 0, MOD_LIGHTNING);

	if ( G_TakesDamage( traceEnt ) &&
       ( traceEnt->client ||
         traceEnt->s.eType == ET_BUILDABLE ) )
  {
		tent = G_TempEntity( tr->endpos, EV_MISSILE_HIT );
		tent->s.otherEntityNum = traceEnt->s.number;
		tent->s.eventParm = DirToByte( tr->plane.normal );
		tent->s.weapon = ent->s.weapon;
    tent->s.generic1 = ent->s.generic1;
	} else
  {
    tent = G_TempEntity( tr->endpos, EV_MISSILE_MISS );
    tent->s.eventParm = DirToByte( tr->plane.normal );
    tent->s.weapon = ent->s.weapon;
    tent->s.generic1 = ent->s.generic1; //weaponMode
  }
}

/*
======================================================================

LAS GUN

======================================================================
*/

/*
===============
lasGunFire
===============
*/
void lasGunFire( gentity_t *ent )
{
  trace_t   tr;
  vec3_t    end;
  gentity_t *tent;
  gentity_t *traceEnt;

  VectorMA( muzzle, 8192 * 16, forward, end );

  G_UnlaggedOn( ent->s.number, muzzle, 8192 * 16 );
  SV_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT, TT_AABB );
  G_UnlaggedOff( );

  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  // snap the endpos to integers, but nudged towards the line
  SnapVectorTowards( tr.endpos, muzzle );

  // send impact
  if( G_TakesDamage( traceEnt ) &&
      (traceEnt->s.eType == ET_BUILDABLE ||
       traceEnt->s.eType == ET_PLAYER ) )
  {
    BloodSpurt( ent, traceEnt, &tr );
  }
  else
  {
    tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
    tent->s.eventParm = DirToByte( tr.plane.normal );
    tent->s.weapon = ent->s.weapon;
    tent->s.generic1 = ent->s.generic1; //weaponMode
  }

  if( G_TakesDamage( traceEnt ) )
    G_Damage( traceEnt, ent, ent, forward, tr.endpos, LASGUN_DAMAGE, 0, MOD_LASGUN );
}

/*
======================================================================

PAIN SAW

======================================================================
*/

void painSawFire( gentity_t *ent) {
  trace_t   tr;
  vec3_t    temp;
  gentity_t *tent, *traceEnt;

  G_WideTraceSolid(
    &tr, ent, PAINSAW_RANGE, PAINSAW_WIDTH, PAINSAW_HEIGHT, &traceEnt);

  if(!traceEnt)
    return;

  // hack to line up particle system with weapon model
  tr.endpos[ 2 ] -= 5.0f;

  if(G_TakesDamage(traceEnt)) {
    // send blood impact
    if(traceEnt->s.eType == ET_PLAYER || traceEnt->s.eType == ET_BUILDABLE) {
        BloodSpurt(ent, traceEnt, &tr);
    } else {
      VectorCopy(tr.endpos, temp);
      tent = G_TempEntity(temp, EV_MISSILE_MISS);
      tent->s.eventParm = DirToByte(tr.plane.normal);
      tent->s.weapon = ent->s.weapon;
      tent->s.generic1 = ent->s.generic1; //weaponMode
    }

    G_Damage(
      traceEnt, ent, ent, forward, tr.endpos, PAINSAW_DAMAGE,
      DAMAGE_NO_KNOCKBACK, MOD_PAINSAW);
  }

  VectorCopy(tr.endpos, temp);
  tent = G_TempEntity(temp, EV_MISSILE_MISS);
  tent->s.eventParm = DirToByte(tr.plane.normal);
  tent->s.weapon = ent->s.weapon;
  tent->s.generic1 = ent->s.generic1; //weaponMode
}

/*
======================================================================

LUCIFER CANNON

======================================================================
*/

/*
===============
LCChargeFire
===============
*/
void LCChargeFire( gentity_t *ent, qboolean secondary )
{
  if( secondary && ent->client->ps.misc[ MISC_MISC ] <= 0 )
    fire_luciferCannon( ent, muzzle, forward, LCANNON_SECONDARY_DAMAGE,
                            LCANNON_SECONDARY_RADIUS, LCANNON_SECONDARY_SPEED );
  else
    fire_luciferCannon( ent, muzzle, forward,
                            ent->client->ps.misc[ MISC_MISC ] *
                            LCANNON_DAMAGE / LCANNON_CHARGE_TIME_MAX,
                            LCANNON_RADIUS,
                            BG_GetLCannonPrimaryFireSpeed( ent->client->ps.misc[ MISC_MISC ] ));

  ent->client->ps.misc[ MISC_MISC ] = 0;
}

/*
======================================================================

PORTAL GUN

======================================================================
*/

/*
===============
PGChargeFire
===============
*/
void PGChargeFire( gentity_t *ent, qboolean secondary )
{
  if( secondary )
    fire_portalGun( ent, muzzle, forward, PORTAL_BLUE, !( ent->client->buttons & BUTTON_WALKING ) );
  else
    fire_portalGun( ent, muzzle, forward, PORTAL_RED, !( ent->client->buttons & BUTTON_WALKING ) );

  ent->client->ps.weaponTime = PORTALGUN_REPEAT;
}

/*
======================================================================

TESLA GENERATOR

======================================================================
*/


void teslaFire( gentity_t *self )
{
  trace_t tr;
  vec3_t origin, target;
  gentity_t *tent;

  if( !self->enemy )
    return;

  // Move the muzzle from the entity origin up a bit to fire over turrets
  VectorMA( muzzle, self->r.maxs[ 2 ], self->s.origin2, origin );

  // Don't aim for the center, aim at the top of the bounding box
  VectorCopy( self->enemy->r.currentOrigin, target );
  target[ 2 ] += self->enemy->r.maxs[ 2 ];

  // Trace to the target entity
  SV_Trace( &tr, origin, NULL, NULL, target, self->s.number, MASK_SHOT, TT_AABB );

  if( tr.entityNum != self->enemy->s.number )
    return;

  // Client side firing effect
  self->s.eFlags |= EF_FIRING;

  // Deal damage
  if( G_TakesDamage( self->enemy ) )
  {
    vec3_t dir;

    VectorSubtract( target, origin, dir );
    G_Damage( self->enemy, self, self, dir, tr.endpos,
              TESLAGEN_DMG, DAMAGE_NO_KNOCKBACK, MOD_TESLAGEN );
  }

  // Send tesla zap trail
  tent = G_TempEntity( tr.endpos, EV_TESLATRAIL );
  tent->s.misc = self->s.number; // src
  tent->s.clientNum = self->enemy->s.number; // dest
}


/*
======================================================================

BUILD GUN

======================================================================
*/
/*
================
G_SetBuildableLinkState

Links or unlinks all the undamaged spawned buildable entities
================
*/
static void G_SetUndamagedSpawnedBuildableLinkState( qboolean link )
{
  int       i;
  gentity_t *ent;

  for ( i = 1, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if( ent->s.eType != ET_BUILDABLE )
      continue;

    if( ent->spawned && 
        ent->health < ent->s.constantLight )
      continue;

    if( link )
      SV_LinkEntity( ent );
    else
      SV_UnlinkEntity( ent );
  }
}

void CheckCkitRepair( gentity_t *ent )
{
  vec3_t      viewOrigin, forward, end;
  trace_t     tr;
  gentity_t   *traceEnt;

  if( ent->client->pmext.repairRepeatDelay > 0 ||
      ent->client->ps.misc[ MISC_MISC ] > 0 )
    return;

  BG_GetClientViewOrigin( &ent->client->ps, viewOrigin );
  AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
  VectorMA( viewOrigin,
            ( 2 * BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->buildDist ),
            forward, end );

  G_SetPlayersLinkState( qfalse, ent );
  G_SetUndamagedSpawnedBuildableLinkState( qfalse );
  SV_Trace( &tr, viewOrigin, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID, TT_AABB );
  traceEnt = &g_entities[ tr.entityNum ];
  G_SetUndamagedSpawnedBuildableLinkState( qtrue );
  G_SetPlayersLinkState( qtrue, ent );

  if( tr.fraction < 1.0f && traceEnt->health > 0 &&
      traceEnt->s.eType == ET_BUILDABLE && traceEnt->buildableTeam == TEAM_HUMANS )
  {
    gentity_t *tent = G_TempEntity( tr.endpos, EV_BUILD_FIRE );
    qboolean sendEffects = qfalse;

    if( traceEnt->spawned )
    {
      if( traceEnt->health < traceEnt->s.constantLight )
      {
        G_ChangeHealth( traceEnt, ent, HBUILD_HEALRATE, 0 );

        if( traceEnt->health >= traceEnt->s.constantLight )
        {
          G_AddEvent( ent, EV_BUILD_REPAIRED, 0 );
        } else
          G_AddEvent( ent, EV_BUILD_REPAIR, 0 );

        ent->client->pmext.repairRepeatDelay += BG_Weapon( ent->client->ps.weapon )->repeatRate1;
        sendEffects = qtrue;
      }

      tent->s.generic1 = WPM_SECONDARY;
    }

    if( sendEffects )
    {
      // send ckit build effects
      tent->s.weapon = ent->s.weapon;
      tent->s.otherEntityNum = ent->s.number;
      tent->s.otherEntityNum2 = ENTITYNUM_NONE;
      ent->client->ps.eFlags |= EF_FIRING;
      ent->client->buildFireTime = level.time + 250;
    }
  }
}

/*
===============
cancelBuildFire
===============
*/
void cancelBuildFire( gentity_t *ent )
{
  // Cancel ghost buildable
  if( ent->client->ps.stats[ STAT_BUILDABLE ] != BA_NONE )
  {
    ent->client->ps.stats[ STAT_BUILDABLE ] = BA_NONE;
    return;
  }

  if( ent->client->ps.weapon == WP_ABUILD )
    meleeAttack( ent, ABUILDER_CLAW_RANGE, ABUILDER_CLAW_WIDTH,
                 ABUILDER_CLAW_WIDTH, ABUILDER_CLAW_DMG, MOD_ABUILDER_CLAW );
  else if( ent->client->ps.weapon == WP_ABUILD2 )
    meleeAttack( ent, ABUILDER_CLAW_RANGE, ABUILDER_CLAW_WIDTH,
                 ABUILDER_CLAW_WIDTH, ABUILDER_UPG_CLAW_DMG, MOD_ABUILDER_CLAW );
}

/*
===============
buildFire
===============
*/
void buildFire( gentity_t *ent, dynMenu_t menu )
{
  buildable_t buildable = ( ent->client->ps.stats[ STAT_BUILDABLE ]
                            & ~SB_VALID_TOGGLEBIT );

  if( buildable > BA_NONE )
  {
    if( ent->client->ps.misc[ MISC_MISC ] > 0 &&
        ( ent->client->ps.weapon == WP_ABUILD ||
          ent->client->ps.weapon == WP_ABUILD2 ) )
    {
      G_AddEvent( ent, EV_BUILD_DELAY, ent->client->ps.clientNum );
      return;
    }

    if( G_BuildIfValid( ent, buildable ) )
    {
      // send ckit build effects
      if(
        (
          ent->client->ps.weapon == WP_HBUILD ||
          ent->client->ps.weapon == WP_HBUILD2) &&
        ent->client->built )
      {
        gentity_t *tent;

        tent = G_TempEntity( ent->client->built->r.currentOrigin, EV_BUILD_FIRE );
        tent->s.weapon = ent->s.weapon;
        tent->s.otherEntityNum = ent->s.number;
        tent->s.otherEntityNum2 = ent->client->built->s.number;
        tent->s.generic1 = ent->client->ps.generic1;
        ent->client->ps.eFlags |= EF_FIRING;
        ent->client->built = NULL;
        ent->client->buildFireTime = level.time + 250;
      }

      ent->client->ps.stats[ STAT_BUILDABLE ] = BA_NONE;
    }

    return;
  }

  G_TriggerMenu( ent->client->ps.clientNum, menu );
}

void slowBlobFire( gentity_t *ent )
{
  fire_slowBlob( ent, muzzle, forward );
}


/*
======================================================================

LEVEL0

======================================================================
*/

/*
===============
CheckVenomAttack
===============
*/
qboolean CheckVenomAttack( gentity_t *ent )
{
  trace_t   tr;
  gentity_t *traceEnt;
  int       damage = LEVEL0_BITE_DMG;

  if( ent->client->ps.weaponTime )
	return qfalse;

  // Calculate muzzle point
  AngleVectors( ent->client->ps.viewangles, forward, right, up );
  BG_CalcMuzzlePointFromPS( &ent->client->ps, forward, right, up, muzzle );

  G_WideTraceSolidSeries( &tr, ent, LEVEL0_BITE_RANGE, LEVEL0_BITE_WIDTH,
                          LEVEL0_BITE_WIDTH, &traceEnt );

  if( traceEnt == NULL )
    return qfalse;

  if( !G_TakesDamage( traceEnt ) )
    return qfalse;

  // Allow the attacking of all enemy buildables.
  if( traceEnt->s.eType == ET_BUILDABLE )
  {

    if( traceEnt->health <= 0 )
        return qfalse;

    if( traceEnt->buildableTeam == TEAM_ALIENS )
      return qfalse;

    //hackery
    damage *= 0.5f;
  }

  if( traceEnt->client )
  {
    if( traceEnt->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      return qfalse;
  }

  // send blood impact
  WideBloodSpurt( ent, traceEnt, &tr );

  G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, DAMAGE_NO_KNOCKBACK, MOD_LEVEL0_BITE );
  ent->client->ps.weaponTime += LEVEL0_BITE_REPEAT;
  return qtrue;
}

/*
======================================================================

LEVEL1

======================================================================
*/

/*
===============
CheckGrabAttack
===============
*/
void CheckGrabAttack( gentity_t *ent )
{
  trace_t     tr;
  vec3_t      end, dir;
  gentity_t   *traceEnt;
  const float range = ent->client->ps.weapon == WP_ALEVEL1 ?
                        LEVEL1_GRAB_RANGE : LEVEL1_GRAB_U_RANGE;

  if( ent->client->grabRepeatTime >= level.time )
    return;

  // set aiming directions
  AngleVectors( ent->client->ps.viewangles, forward, right, up );

  BG_CalcMuzzlePointFromPS( &ent->client->ps, forward, right, up, muzzle );

  VectorMA( muzzle, range, forward, end );

  G_UnlaggedOn( ent->s.number, muzzle, range );
  SV_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT, TT_AABB );
  G_UnlaggedOff();
  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  if( !G_TakesDamage( traceEnt ) || traceEnt->health <= 0 )
    return;

  if( traceEnt->client )
  {
    if( traceEnt->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      return;

    if( traceEnt->health <= 0 )
      return;

    if( !( traceEnt->client->ps.stats[ STAT_STATE ] & SS_GRABBED ) )
    {
      AngleVectors( traceEnt->client->ps.viewangles, dir, NULL, NULL );
      traceEnt->client->ps.stats[ STAT_VIEWLOCK ] = DirToByte( dir );

      //event for client side grab effect
      G_AddPredictableEvent( ent, EV_LEV1_GRAB, 0 );
    }

    traceEnt->client->ps.stats[ STAT_STATE ] |= SS_GRABBED;

    if( ent->client->ps.weapon == WP_ALEVEL1 )
    {
      traceEnt->client->grabExpiryTime = level.time + LEVEL1_GRAB_TIME;
      ent->client->grabRepeatTime = level.time + LEVEL1_GRAB_REPEAT;
    }
    else if( ent->client->ps.weapon == WP_ALEVEL1_UPG )
    {
      traceEnt->client->grabExpiryTime = level.time + LEVEL1_GRAB_U_TIME;
      ent->client->grabRepeatTime = level.time + LEVEL1_GRAB_U_REPEAT;
    }
  }else if( traceEnt->s.eType == ET_BUILDABLE &&
      traceEnt->s.modelindex == BA_H_MGTURRET )
  {
    if( !traceEnt->lev1Grabbed )
      G_AddPredictableEvent( ent, EV_LEV1_GRAB, 0 );

      traceEnt->lev1Grabbed = qtrue;
      if( ent->client->ps.weapon == WP_ALEVEL1 )
      {
        traceEnt->lev1GrabTime = level.time + LEVEL1_GRAB_TIME;
        ent->client->grabRepeatTime = level.time + LEVEL1_GRAB_REPEAT;
      }
      else if( ent->client->ps.weapon == WP_ALEVEL1_UPG )
      {
        traceEnt->lev1GrabTime = level.time + LEVEL1_GRAB_U_TIME;
        ent->client->grabRepeatTime = level.time + LEVEL1_GRAB_U_REPEAT;
      }
  }
}

/*
===============
poisonCloud
===============
*/
void poisonCloud( gentity_t *ent )
{
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { LEVEL1_PCLOUD_RANGE, LEVEL1_PCLOUD_RANGE, LEVEL1_PCLOUD_RANGE };
  vec3_t    mins, maxs;
  int       i, num;
  gentity_t *target;
  trace_t   tr;

  VectorAdd( ent->client->ps.origin, range, maxs );
  VectorSubtract( ent->client->ps.origin, range, mins );

  G_UnlaggedOn( ent->s.number, ent->client->ps.origin, LEVEL1_PCLOUD_RANGE );
  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
  {
    target = &g_entities[ entityList[ i ] ];

    if( target->client )
    {
      if( target->client->pers.teamSelection == TEAM_HUMANS )
      {
        SV_Trace( &tr, muzzle, NULL, NULL, target->r.currentOrigin,
                    target->s.number, CONTENTS_SOLID, TT_AABB );

        //can't see target from here
        if( tr.entityNum == ENTITYNUM_WORLD )
          continue;

        target->client->ps.eFlags |= EF_POISONCLOUDED;
        target->client->lastPoisonCloudedTime = level.time;
        target->client->lastPoisonCloudedClient = ent;

        if( !BG_InventoryContainsUpgrade( UP_HELMET, target->client->ps.stats ) &&
            !BG_InventoryContainsUpgrade( UP_BATTLESUIT, target->client->ps.stats ) &&
            ent->client->ps.stats[ STAT_STATE ] & SS_BOOSTED )
        {
          G_Damage( target, ent, ent, NULL, NULL,
                    LEVEL1_PCLOUD_DMG, 0, MOD_LEVEL1_PCLOUD );
        }

        SV_GameSendServerCommand( target->client->ps.clientNum,
                                "poisoncloud" );
      } else if( target->client->pers.teamSelection == TEAM_ALIENS &&
                 ent->client &&
                 !( ( ent->client->ps.stats[ STAT_STATE ] & SS_BOOSTED ) &&
                    target->client->boostedTime > ent->client->boostedTime ) &&
                 ( target->r.contents & MASK_SHOT ) &&
                 !( target->r.contents & CONTENTS_ASTRAL_NOCLIP ) )
      {
        target->client->ps.stats[ STAT_STATE ] |= SS_BOOSTED;
        target->client->boostedTime = ent->client->boostedTime;
      }
    }
  }
  G_UnlaggedOff( );
}


/*
======================================================================

LEVEL2 & SPITFIRE zaps

======================================================================
*/

bglist_t *lev2ZapList = NULL;
bglist_t *spitfireZapList = NULL;

/*
===============
G_DeleteZapData
===============
*/
void G_DeleteZapData( void *data )
{
  zap_t *zapData = (zap_t *)data;

  BG_Queue_Clear_Full( &zapData->targetQueue, BG_FreePassed );

  BG_Free( zapData );
}

/*
===============
G_FindLev2ZapChainTargets
===============
*/
static void G_FindLev2ZapChainTargets( zap_t *zap )
{
  gentity_t *ent =
            ((zapTarget_t *)BG_Queue_Peek_Head( &zap->targetQueue ))->targetEnt; // the source
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { LEVEL2_AREAZAP_CHAIN_RANGE,
                      LEVEL2_AREAZAP_CHAIN_RANGE,
                      LEVEL2_AREAZAP_CHAIN_RANGE };
  vec3_t    mins, maxs;
  int       i, num;
  gentity_t *enemy;
  trace_t   tr;
  float     distance;

  VectorAdd( ent->r.currentOrigin, range, maxs );
  VectorSubtract( ent->r.currentOrigin, range, mins );

  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );

  for( i = 0; i < num; i++ )
  {
    enemy = &g_entities[ entityList[ i ] ];
    // don't chain to self; noclippers can be listed, don't chain to them either
    if( enemy == ent || ( enemy->client && enemy->client->noclip ) )
      continue;

    distance = Distance( ent->r.currentOrigin, enemy->r.currentOrigin );

    if( ( ( enemy->client &&
            enemy->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ) ||
          ( enemy->s.eType == ET_BUILDABLE &&
            BG_Buildable( enemy->s.modelindex )->team == TEAM_HUMANS ) ||
          enemy->s.eType == ET_TELEPORTAL ) &&
        enemy->health > 0 && // only chain to living targets
        distance <= LEVEL2_AREAZAP_CHAIN_RANGE )
    {
      // world-LOS check: trace against the world, ignoring other BODY entities
      SV_Trace( &tr, ent->r.currentOrigin, NULL, NULL,
         enemy->r.currentOrigin, ent->s.number, CONTENTS_SOLID, TT_AABB );

      if( tr.entityNum == ENTITYNUM_NONE )
      {
        zapTarget_t *zapTarget;

        zapTarget = BG_Alloc( sizeof(zapTarget_t) );
        zapTarget->targetEnt = enemy;
        zapTarget->distance = distance;
        BG_Queue_Push_Tail( &zap->targetQueue, zapTarget );

        if( BG_Queue_Get_Length( &zap->targetQueue ) >=
                                                    LEVEL2_AREAZAP_MAX_TARGETS )
          return;
      }
    }
  }
}

/*
=============================
G_SpitfireZapTargetDistanceCompare

Comparison function used in a BG_Queue_Insert_Sorted()
=============================
*/
static int G_SpitfireZapTargetDistanceCompare( const void *a, const void *b,
                                              void *user_data )
{
  return ((zapTarget_t *)a)->distance - ((zapTarget_t *)b)->distance;
}

/*
=============================
G_FindSpitfireZapTarget
=============================
*/
static void G_FindSpitfireZapTarget( zap_t *zap )
{
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { SPITFIRE_ZAP_RANGE,
                      SPITFIRE_ZAP_RANGE,
                      SPITFIRE_ZAP_RANGE };
  vec3_t    mins, maxs;
  int       i, num;
  gentity_t *enemy;
  trace_t   tr;
  float     distance;

  VectorAdd( zap->creator->r.currentOrigin, range, maxs );
  VectorSubtract( zap->creator->r.currentOrigin, range, mins );

  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );

  for( i = 0; i < num; i++ )
  {
    enemy = &g_entities[ entityList[ i ] ];

    if( ( enemy->client &&
          enemy->client->ps.stats[ STAT_TEAM ] != zap->creator->client->ps.stats[ STAT_TEAM ] &&
          enemy->client->sess.spectatorState == SPECTATOR_NOT ) ||
        ( enemy->s.eType == ET_BUILDABLE &&
          BG_Buildable( enemy->s.modelindex )->team != zap->creator->client->ps.stats[ STAT_TEAM ] ) ||
        ( enemy->s.eType == ET_MISSILE ||
          enemy->s.eType == ET_TELEPORTAL ) )
    {
      zapTarget_t *zapTarget;
      float range = SPITFIRE_ZAP_RANGE;

      if( zap->creator && zap->creator->client &&
          zap->creator->client->ps.weapon == WP_ALEVEL2 )
        range = LEVEL2_EXPLODE_CHARGE_ZAP_RADIUS;

      // only target entities that can take damage
      if( !G_TakesDamage( enemy ) )
        continue;

      // only target living enemies
      if( enemy->health <= 0 )
        continue;

      // don't target noclippers
      if( enemy->client && enemy->client->noclip )
        continue;

      // only target enemies that are in a line of sight and within range
      SV_Trace( &tr, zap->creator->r.currentOrigin, NULL, NULL, enemy->r.currentOrigin,
                zap->creator->s.number, MASK_SOLID, TT_AABB );
      if( ( tr.fraction < 1.0f &&
            tr.entityNum != enemy->s.number ) ||
          (distance = Distance( zap->creator->r.currentOrigin, tr.endpos )) >
          SPITFIRE_ZAP_RANGE )
        continue;

      // add the zap targets, giving preference to the closest targets
      zapTarget = BG_Alloc( sizeof(zapTarget_t) );
      zapTarget->targetEnt = enemy;
      zapTarget->distance = distance;
      BG_Queue_Insert_Sorted( &zap->targetQueue, zapTarget,
                              G_SpitfireZapTargetDistanceCompare, NULL );

      // ellimenate the targets over the max targets that don't
      // have the distance preference
      while( BG_Queue_Get_Length( &zap->targetQueue ) > SPITFIRE_ZAP_MAX_TARGETS )
      {
        bglist_t *tailLink = BG_Queue_Peek_Tail_Link( &zap->targetQueue );

        if( tailLink )
        {
          BG_Free( tailLink->data );
          BG_Queue_Delete_Link( &zap->targetQueue, tailLink );
        }
      }
    }
  }
 
}

/*
===============
G_UpdateZapEffect
===============
*/
static void G_UpdateZapEffect( zap_t *zap )
{
  G_PackEntityNumbers( &zap->effectChannel->s, zap->creator->s.number,
                       &zap->targetQueue );

  VectorCopy( zap->creator->r.currentOrigin, zap->effectChannel->r.currentOrigin );
  SV_LinkEntity( zap->effectChannel );
}

/*
===============
G_DamageLev2ZapTarget
===============
*/
static void G_DamageLev2ZapTarget( void *data, void *user_data )
{
  zapTarget_t *target = (zapTarget_t *)data;
  zap_t *zap = (zap_t *)user_data;


  G_Damage( target->targetEnt, target->targetEnt, zap->creator, forward,
            target->targetEnt->r.currentOrigin, LEVEL2_AREAZAP_DMG *
            ( 1 - pow( ( target->distance / LEVEL2_AREAZAP_CHAIN_RANGE ),
                       LEVEL2_AREAZAP_CHAIN_FALLOFF ) ) + 1,
            DAMAGE_NO_KNOCKBACK | DAMAGE_NO_LOCDAMAGE, MOD_LEVEL2_ZAP );
}

/*
===============
G_DamageSpitfireZapTarget
===============
*/
static void G_DamageSpitfireZapTarget( void *data, void *user_data )
{
  zapTarget_t *target = (zapTarget_t *)data;
  zap_t *zap = (zap_t *)user_data;
  vec3_t dir;
  meansOfDeath_t mod = MOD_SPITFIRE_ZAP;
  int damage = SPITFIRE_ZAP_DMG;

  if( zap->creator && zap->creator->client &&
      zap->creator->client->ps.weapon == WP_ALEVEL2 )
  {
    mod = MOD_LEVEL2_ZAP;
    damage = LEVEL2_EXPLODE_CHARGE_ZAP_DMG;
  }

  // apply 3/4 damage to targets niether grounded nor that contact water
  if( target->targetEnt->s.groundEntityNum == ENTITYNUM_NONE &&
      !( target->targetEnt->client && target->targetEnt->client->pmext.ladder ) &&
      target->targetEnt->waterlevel < 1 )
    damage = ( damage * 3 ) / 4;

  VectorSubtract( target->targetEnt->r.currentOrigin,
                  zap->creator->r.currentOrigin, dir );
  G_Damage( target->targetEnt, zap->creator, zap->creator, dir,
            target->targetEnt->r.currentOrigin,
            damage / BG_Queue_Get_Length( &zap->targetQueue ),
            DAMAGE_NO_KNOCKBACK | DAMAGE_NO_LOCDAMAGE, mod );
}

/*
===============
G_CreateNewLev2Zap
===============
*/
static void G_CreateNewLev2Zap( gentity_t *creator, gentity_t *targetEnt )
{
  zap_t   *zap = BG_Alloc0( sizeof( zap_t ) );
  zapTarget_t *primaryZapTarget;

  // add the newly created zap to the lev2ZapList
  lev2ZapList = zap->zapLink = BG_List_Prepend( lev2ZapList, zap );

  // initialize the zap
  zap->timeToLive = LEVEL2_AREAZAP_TIME;
  zap->creator = creator;
  BG_Queue_Init( &zap->targetQueue );
  primaryZapTarget = BG_Alloc( sizeof(zapTarget_t) );
  primaryZapTarget->targetEnt = targetEnt;
  primaryZapTarget->distance  = 0;
  BG_Queue_Push_Head( &zap->targetQueue, primaryZapTarget );

  // the zap chains only through living entities
  if( targetEnt->health > 0 )
  {
    G_FindLev2ZapChainTargets( zap );

    G_Damage( targetEnt, zap->creator, zap->creator, forward,
              targetEnt->r.currentOrigin, LEVEL2_AREAZAP_DMG,
              DAMAGE_NO_KNOCKBACK | DAMAGE_NO_LOCDAMAGE,
              MOD_LEVEL2_ZAP );

    BG_List_Foreach( BG_Queue_Peek_Head_Link( &zap->targetQueue )->next,
                     G_DamageLev2ZapTarget, zap );
  }

  // create and initialize the zap's effectChannel
  zap->effectChannel = G_Spawn( );
  zap->effectChannel->s.eType = ET_LEV2_ZAP_CHAIN;
  zap->effectChannel->classname = "lev2zapchain";
  zap->effectChannel->zapLink = zap->zapLink;
  G_UpdateZapEffect( zap );
}

/*
===============
G_CreateNewSpitfireZap
===============
*/
static void G_CreateNewSpitfireZap( gentity_t *creator )
{
  zap_t   *zap = BG_Alloc0( sizeof( zap_t ) );

  // add the newly created zap to the spitfireZapList
  spitfireZapList = zap->zapLink = BG_List_Prepend( spitfireZapList, zap );

  // initialize the zap
  zap->timeToLive = SPITFIRE_ZAP_TIME;
  zap->creator = creator;
  BG_Queue_Init( &zap->targetQueue );

  // find spitefire zap targets
  G_FindSpitfireZapTarget( zap );

  BG_Queue_Foreach( &zap->targetQueue,
                    G_DamageSpitfireZapTarget, zap );

  // create and initialize the zap's effectChannel
  zap->effectChannel = G_Spawn( );
  zap->effectChannel->s.eType = ET_SPITFIRE_ZAP;
  zap->effectChannel->classname = "spitfirezap";
  zap->effectChannel->zapLink = zap->zapLink;
  G_UpdateZapEffect( zap );
}

/*
===============
G_UpdateLev2Zap
===============
*/
static void G_UpdateLev2Zap( void *data, void *user_data )
{
  zap_t *zap = (zap_t *)data;
  zapTarget_t *primaryTarget =
              (zapTarget_t *)BG_Queue_Peek_Head_Link( &zap->targetQueue )->data;
  bglist_t    *targetZapLink = BG_Queue_Peek_Head_Link( &zap->targetQueue )->next;
  int j;


  zap->timeToLive -= *(int *)user_data;

  // first, the disappearance of players is handled immediately in G_ClearPlayerZapEffects()

  // the deconstruction or gibbing of a directly targeted buildable destroys the whole zap effect
  if( zap->timeToLive <= 0 || !primaryTarget->targetEnt->inuse )
  {
    G_FreeEntity( zap->effectChannel );
    lev2ZapList = BG_List_Delete_Link( lev2ZapList, zap->zapLink );
    G_DeleteZapData( zap );
    return;
  }

  // the deconstruction or gibbing of chained buildables destroy the appropriate beams
  for( j = 1; targetZapLink; j++ )
  {
    bglist_t *next = targetZapLink->next;

    if( !((zapTarget_t *)targetZapLink->data)->targetEnt->inuse )
      {
        BG_Free( targetZapLink->data );
        BG_Queue_Delete_Link( &zap->targetQueue, targetZapLink );
      }

      targetZapLink = next;
  }

  G_UpdateZapEffect( zap );
}

/*
===============
G_UpdateSpitfireZap
===============
*/
static void G_UpdateSpitfireZap( void *data, void *user_data )
{
  zap_t *zap = (zap_t *)data;
  bglist_t    *targetZapLink = BG_Queue_Peek_Head_Link( &zap->targetQueue );
  int j;


  zap->timeToLive -= *(int *)user_data;

  if( zap->timeToLive <= 0 )
  {
    G_FreeEntity( zap->effectChannel );
    spitfireZapList = BG_List_Delete_Link( spitfireZapList, zap->zapLink );
    G_DeleteZapData( zap );
    return;
  }

  // first, the disappearance of players is handled immediately in G_ClearPlayerZapEffects()

  // the deconstruction or gibbing of chained buildables destroy the appropriate beams
  for( j = 1; targetZapLink; j++ )
  {
    bglist_t *next = targetZapLink->next;

    if( !((zapTarget_t *)targetZapLink->data)->targetEnt->inuse )
      {
        BG_Free( targetZapLink->data );
        BG_Queue_Delete_Link( &zap->targetQueue, targetZapLink );
      }

      targetZapLink = next;
  }

  G_UpdateZapEffect( zap );
}

/*
===============
G_UpdateZaps
===============
*/
void G_UpdateZaps( int msec )
{
  BG_List_Foreach( lev2ZapList, G_UpdateLev2Zap, &msec );
  BG_List_Foreach( spitfireZapList, G_UpdateSpitfireZap, &msec );
}

/*
===============
G_ClearPlayerLev2ZapEffects
===============
*/
void G_ClearPlayerLev2ZapEffects( void *data, void *user_data )
{
  zap_t *zap = (zap_t *)data;
  zapTarget_t *primaryTarget =
              (zapTarget_t *)BG_Queue_Peek_Head_Link( &zap->targetQueue )->data;
  bglist_t    *targetZapLink = BG_Queue_Peek_Head_Link( &zap->targetQueue )->next;
  gentity_t *player = (gentity_t *)user_data;
  int       j;

  // the disappearance of the creator or the first target destroys the whole zap effect
  if( zap->creator == player || primaryTarget->targetEnt == player )
  {
    G_FreeEntity( zap->effectChannel );
    lev2ZapList = BG_List_Delete_Link( lev2ZapList, zap->zapLink );
    G_DeleteZapData( zap );
    return;
  }
  // the disappearance of chained players destroy the appropriate beams
  for( j = 1; targetZapLink; j++ )
  {
    bglist_t *next = targetZapLink->next;

    if( ((zapTarget_t *)targetZapLink->data)->targetEnt == player )
      {
        BG_Free( targetZapLink->data );
        BG_Queue_Delete_Link( &zap->targetQueue, targetZapLink );
      }

      targetZapLink = next;
  }
}

/*
===============
G_ClearPlayerSpitfireZapEffects
===============
*/
void G_ClearPlayerSpitfireZapEffects( void *data, void *user_data )
{
  zap_t *zap = (zap_t *)data;
  bglist_t    *targetZapLink = BG_Queue_Peek_Head_Link( &zap->targetQueue );
  gentity_t *player = (gentity_t *)user_data;
  int       j;

  // the disappearance of the creator destroys the whole zap effect
  if( zap->creator == player )
  {
    G_FreeEntity( zap->effectChannel );
    spitfireZapList = BG_List_Delete_Link( spitfireZapList, zap->zapLink );
    G_DeleteZapData( zap );
    return;
  }
  // the disappearance of chained players destroy the appropriate beams
  for( j = 1; targetZapLink; j++ )
  {
    bglist_t *next = targetZapLink->next;

    if( ((zapTarget_t *)targetZapLink->data)->targetEnt == player )
      {
        BG_Free( targetZapLink->data );
        BG_Queue_Delete_Link( &zap->targetQueue, targetZapLink );
      }

      targetZapLink = next;
  }
}

/*
===============
G_ClearPlayerZapEffects

called from G_LeaveTeam() and TeleportPlayer()
===============
*/
void G_ClearPlayerZapEffects( gentity_t *player )
{
  BG_List_Foreach( lev2ZapList, G_ClearPlayerLev2ZapEffects, player );
  BG_List_Foreach( spitfireZapList, G_ClearPlayerSpitfireZapEffects, player );
}

/*
===============
areaLev2ZapFire
===============
*/
void areaLev2ZapFire( gentity_t *ent )
{
  trace_t   tr;
  gentity_t *traceEnt;

  G_WideTraceSolidSeries( &tr, ent, LEVEL2_AREAZAP_RANGE, LEVEL2_AREAZAP_WIDTH,
                          LEVEL2_AREAZAP_WIDTH, &traceEnt );

  if( traceEnt == NULL )
    return;

  if( ( traceEnt->client && traceEnt->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ) ||
      ( traceEnt->s.eType == ET_BUILDABLE &&
        BG_Buildable( traceEnt->s.modelindex )->team == TEAM_HUMANS ) ||
      traceEnt->s.eType == ET_TELEPORTAL )
  {
    G_CreateNewLev2Zap( ent, traceEnt );
  }
}

/*
===============
SpitfireZap
===============
*/
void SpitfireZap( gentity_t *self )
{
  G_CreateNewSpitfireZap( self );
}

/*
===============
MarauderExplosionZap
===============
*/
void MarauderExplosionZap( gentity_t *self )
{
  G_CreateNewSpitfireZap( self );
}

/*
======================================================================

LEVEL3

======================================================================
*/

/*
===============
CheckPounceAttack
===============
*/
qboolean CheckPounceAttack( gentity_t *ent, trace_t *trace,
                            const vec3_t impactVelocity )
{
  gentity_t *impactEnt = &g_entities[ trace->entityNum ];
  vec3_t    impactDir;
  vec3_t    impactEntVelocity;
  vec3_t    relativeVelocity;
  vec3_t    impactNormal;
  float     impactSpeed;
  float     speedmod;
  float     deflectionMod;
  float     maxJumpMag;
  float     payloadMod = 0.5f;
  int       maxDamage;
  int       repeat;
  int       maxPounceTime;
  int       damage;
  qboolean  usePayload = qfalse;

  if( trace->entityNum == ENTITYNUM_NONE )
    return qfalse;

  if( ent->client->pounceEntHitTime[ impactEnt->s.number ] >= level.time )
    return qfalse;

  switch ( ent->client->ps.weapon )
  {
    case WP_ALEVEL3:
      maxJumpMag = LEVEL3_POUNCE_JUMP_MAG;
      maxDamage = LEVEL3_POUNCE_DMG;
      repeat = LEVEL3_POUNCE_REPEAT;
      maxPounceTime = LEVEL3_POUNCE_TIME;
      break;

    case WP_ALEVEL3_UPG:
      maxJumpMag = LEVEL3_POUNCE_JUMP_MAG_UPG;
      maxDamage = LEVEL3_POUNCE_DMG;
      repeat = LEVEL3_POUNCE_REPEAT;
      maxPounceTime = LEVEL3_POUNCE_TIME_UPG;
      break;

    case WP_ASPITFIRE:
      maxJumpMag = SPITFIRE_POUNCE_JUMP_MAG;
      maxDamage = SPITFIRE_POUNCE_DMG;
      repeat = SPITFIRE_POUNCE_REPEAT;
      maxPounceTime = SPITFIRE_POUNCE_TIME;
      break;

    default:
      return qfalse;
  }

  if( !( ent->client->ps.pm_flags & PMF_CHARGE ) )
  {
    ent->client->pmext.pouncePayloadTime = -1;
    ent->client->pmext.pouncePayload = 0;
  }

  if( trace->entityNum == ENTITYNUM_WORLD )
  {
    ent->client->pmext.pouncePayloadTime = -1;
    ent->client->pmext.pouncePayload = 0;
    return qfalse;
  }

  VectorCopy( impactVelocity, impactDir );
  impactSpeed = VectorNormalize( impactDir );

  if( impactSpeed <= ( ent->client->ps.speed * 1.25 ) )
  {
    if( impactEnt->s.number == ent->client->ps.groundEntityNum )
      usePayload = qtrue;
    else
      return qfalse;
  }

  if( impactEnt->client ||
      impactEnt->s.eType == ET_BUILDABLE )
  {
    VectorSubtract( ent->r.currentOrigin, impactEnt->r.currentOrigin,
                    impactNormal);
    VectorNormalize( impactNormal );
  }
  else
    VectorCopy( trace->plane.normal, impactNormal );

  deflectionMod = -DotProduct( impactNormal, impactDir );

  // ensure that the impact entity is in the direction of travel
  if( deflectionMod <= 0 )
  {
    if( impactEnt->s.number == ent->client->ps.groundEntityNum )
      usePayload = qtrue;
    else
      return qfalse;
  }

  payloadMod *= ( (float)ent->client->pmext.pouncePayload ) /
                ( (float)maxPounceTime );

  if( !usePayload )
  {
    if( impactEnt->client )
      VectorCopy( impactEnt->client->ps.velocity, impactEntVelocity);
    else
      VectorCopy( impactEnt->s.pos.trDelta, impactEntVelocity);

    // determine relativeVelocity
    VectorSubtract( impactVelocity, impactEntVelocity, relativeVelocity );
    speedmod = VectorLength( relativeVelocity );
    speedmod = speedmod / maxJumpMag;

    damage = maxDamage * deflectionMod * speedmod;
  } else
  {
    damage = maxDamage * payloadMod;
  }

  // Send blood impact
  if( damage > 0 &&
      G_TakesDamage( impactEnt ) )
    WideBloodSpurt( ent, impactEnt, trace );
  else
    return qfalse;

  // Deal damage
  if( ent->client->ps.weapon == WP_ASPITFIRE)
  {
    G_Damage( impactEnt, ent, ent, forward, trace->endpos, damage,
	      DAMAGE_NO_LOCDAMAGE, MOD_SPITFIRE_POUNCE );
  }
  else
  {
    G_Damage( impactEnt, ent, ent, forward, trace->endpos, damage,
	      DAMAGE_NO_LOCDAMAGE, MOD_LEVEL3_POUNCE );
  }
  ent->client->pmext.pouncePayload = 0;
  ent->client->pmext.pouncePayloadTime = -1;
  ent->client->pounceEntHitTime[ impactEnt->s.number ] = level.time + repeat;
  return qtrue;
}

void bounceBallFire( gentity_t *ent )
{
  fire_bounceBall( ent, muzzle, forward );
}


/*
======================================================================

LEVEL4

======================================================================
*/

/*
===============
G_ChargeAttack
===============
*/
void G_ChargeAttack( gentity_t *ent, gentity_t *victim )
{
  int       damage;
  int       i;
  vec3_t    forward;

  if( ent->client->ps.misc[ MISC_MISC ] <= 0 ||
      !( ent->client->ps.stats[ STAT_STATE ] & SS_CHARGING ) ||
      ent->client->ps.weaponTime )
    return;

  VectorSubtract( victim->r.currentOrigin, ent->r.currentOrigin, forward );
  VectorNormalize( forward );

  if( !G_TakesDamage( victim ) )
    return;

  // For buildables, track the last MAX_TRAMPLE_BUILDABLES_TRACKED buildables
  //  hit, and do not do damage if the current buildable is in that list
  //  in order to prevent dancing over stuff to kill it very quickly
  if( !victim->client )
  {
    for( i = 0; i < MAX_TRAMPLE_BUILDABLES_TRACKED; i++ )
    {
      if( ent->client->trampleBuildablesHit[ i ] == victim - g_entities )
        return;
    }

    ent->client->trampleBuildablesHit[
      ent->client->trampleBuildablesHitPos++ % MAX_TRAMPLE_BUILDABLES_TRACKED ] =
      victim - g_entities;
  }

  WideBloodSpurt( ent, victim, NULL );

  damage = LEVEL4_TRAMPLE_DMG * ent->client->ps.misc[ MISC_MISC ] /
           LEVEL4_TRAMPLE_DURATION;

  G_Damage( victim, ent, ent, forward, victim->r.currentOrigin, damage,
            DAMAGE_NO_LOCDAMAGE, MOD_LEVEL4_TRAMPLE );

  ent->client->ps.weaponTime += LEVEL4_TRAMPLE_REPEAT;
}

/*
===============
G_CrushAttack

Should only be called if there was an impact between a tyrant and another player
===============
*/
void G_CrushAttack( gentity_t *ent, gentity_t *victim )
{
  vec3_t dir;
  float jump;
  int damage;

  if( !G_TakesDamage( victim ) ||
      ent->client->ps.origin[ 2 ] + ent->r.mins[ 2 ] <
      victim->r.currentOrigin[ 2 ] + victim->r.maxs[ 2 ] ||
      ( victim->client &&
        victim->client->ps.groundEntityNum == ENTITYNUM_NONE ) )
    return;

  // Deal velocity based damage to target
  jump = BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->jumpMagnitude;
  damage = ( ent->client->pmext.fallVelocity + jump ) *
           -LEVEL4_CRUSH_DAMAGE_PER_V;

  if( damage < 0 )
    damage = 0;

  // Players also get damaged periodically
  if( victim->client &&
      ent->client->lastCrushTime + LEVEL4_CRUSH_REPEAT < level.time )
  {
    ent->client->lastCrushTime = level.time;
    damage += LEVEL4_CRUSH_DAMAGE;
  }

  if( damage < 1 )
    return;

  // Crush the victim over a period of time
  VectorSubtract( victim->r.currentOrigin, ent->client->ps.origin, dir );
  G_Damage( victim, ent, ent, dir, victim->r.currentOrigin, damage,
            DAMAGE_NO_LOCDAMAGE, MOD_LEVEL4_CRUSH );
}

//======================================================================

/*
===============
FireWeapon3
===============
*/
void FireWeapon3( gentity_t *ent )
{
  if( ent->client )
  {
    // set aiming directions
    AngleVectors( ent->client->ps.viewangles, forward, right, up );
    BG_CalcMuzzlePointFromPS( &ent->client->ps, forward, right, up, muzzle );
  }
  else
  {
    AngleVectors( ent->s.angles2, forward, right, up );
    VectorCopy( ent->s.pos.trBase, muzzle );
  }

  // fire the specific weapon
  switch( ent->s.weapon )
  {
    case WP_ALEVEL3_UPG:
      bounceBallFire( ent );
      break;

    case WP_LIGHTNING:
      lightningEMPFire( ent );
      break;

    case WP_ABUILD:
    case WP_ABUILD2:
      slowBlobFire( ent );
      break;

    default:
      break;
  }
}

/*
===============
FireWeapon2
===============
*/
void FireWeapon2( gentity_t *ent )
{
  if( ent->client )
  {
    // set aiming directions
    AngleVectors( ent->client->ps.viewangles, forward, right, up );
    BG_CalcMuzzlePointFromPS( &ent->client->ps, forward, right, up, muzzle );
  }
  else
  {
    AngleVectors( ent->s.angles2, forward, right, up );
    VectorCopy( ent->s.pos.trBase, muzzle );
  }

  // fire the specific weapon
  switch( ent->s.weapon )
  {
    case WP_ALEVEL1_UPG:
      poisonCloud( ent );
      break;

    case WP_MACHINEGUN:
      bulletFire( ent, RIFLE_SPREAD2, RIFLE_DMG2, MOD_MACHINEGUN );
      break;

    case WP_SHOTGUN:
      shotgunFire( ent );
      break;

    case WP_CHAINGUN:
      bulletFire( ent, CHAINGUN_SPREAD2, CHAINGUN_DMG2, MOD_CHAINGUN );
      break;

    case WP_LUCIFER_CANNON:
      LCChargeFire( ent, qtrue );
      break;

    case WP_PORTAL_GUN:
      PGChargeFire( ent, qtrue );
      break;

    case WP_LAUNCHER:
      launcherFire( ent, qfalse );
      break;

    case WP_LIGHTNING:
      lightningBallFire( ent );
      break;

    case WP_ALEVEL2_UPG:
      areaLev2ZapFire( ent );
      break;

    case WP_ABUILD:
    case WP_ABUILD2:
    case WP_HBUILD:
    case WP_HBUILD2:
      cancelBuildFire( ent );
      break;
    default:
      break;
  }
}

/*
===============
FireWeapon
===============
*/
void FireWeapon( gentity_t *ent )
{
  if( ent->client )
  {
    // set aiming directions
    AngleVectors( ent->client->ps.viewangles, forward, right, up );
    BG_CalcMuzzlePointFromPS( &ent->client->ps, forward, right, up, muzzle );
  }
  else
  {
    AngleVectors( ent->turretAim, forward, right, up );
    VectorCopy( ent->s.pos.trBase, muzzle );
  }

  // fire the specific weapon
  switch( ent->s.weapon )
  {
    case WP_ALEVEL1:
      meleeAttack( ent, LEVEL1_CLAW_RANGE, LEVEL1_CLAW_WIDTH, LEVEL1_CLAW_WIDTH,
                   LEVEL1_CLAW_DMG, MOD_LEVEL1_CLAW );
      CheckGrabAttack( ent );
      break;
    case WP_ALEVEL1_UPG:
      meleeAttack( ent, LEVEL1_CLAW_U_RANGE, LEVEL1_CLAW_WIDTH, LEVEL1_CLAW_WIDTH,
                   LEVEL1_CLAW_DMG, MOD_LEVEL1_CLAW );
      CheckGrabAttack( ent );
      break;
    case WP_ALEVEL3:
      meleeAttack( ent, LEVEL3_CLAW_RANGE, LEVEL3_CLAW_WIDTH, LEVEL3_CLAW_WIDTH,
                   LEVEL3_CLAW_DMG, MOD_LEVEL3_CLAW );
      break;
    case WP_ALEVEL3_UPG:
      meleeAttack( ent, LEVEL3_CLAW_UPG_RANGE, LEVEL3_CLAW_WIDTH,
                   LEVEL3_CLAW_WIDTH, LEVEL3_CLAW_DMG, MOD_LEVEL3_CLAW );
      break;
    case WP_ALEVEL2:
      meleeAttack( ent, LEVEL2_CLAW_RANGE, LEVEL2_CLAW_WIDTH, LEVEL2_CLAW_WIDTH,
                   LEVEL2_CLAW_DMG, MOD_LEVEL2_CLAW );
      break;
    case WP_ALEVEL2_UPG:
      meleeAttack( ent, LEVEL2_CLAW_U_RANGE, LEVEL2_CLAW_WIDTH, LEVEL2_CLAW_WIDTH,
                   LEVEL2_CLAW_DMG, MOD_LEVEL2_CLAW );
      break;
    case WP_ASPITFIRE:
      SpitfireZap( ent );			   
      break;
    case WP_ALEVEL4:
      meleeAttack( ent, LEVEL4_CLAW_RANGE, LEVEL4_CLAW_WIDTH,
                   LEVEL4_CLAW_HEIGHT, LEVEL4_CLAW_DMG, MOD_LEVEL4_CLAW );
      break;

    case WP_BLASTER:
      blasterFire( ent );
      break;
    case WP_MACHINEGUN:
      bulletFire( ent, RIFLE_SPREAD, RIFLE_DMG, MOD_MACHINEGUN );
      break;
    case WP_SHOTGUN:
      shotgunFire( ent );
      break;
    case WP_CHAINGUN:
      bulletFire( ent, CHAINGUN_SPREAD, CHAINGUN_DMG, MOD_CHAINGUN );
      break;
    case WP_FLAMER:
      flamerFire( ent );
      break;
    case WP_PULSE_RIFLE:
      pulseRifleFire( ent );
      break;
    case WP_MASS_DRIVER:
      massDriverFire( ent );
      break;
    case WP_LUCIFER_CANNON:
      LCChargeFire( ent, qfalse );
      break;
    case WP_PORTAL_GUN:
      PGChargeFire( ent, qfalse );
      break;
    case WP_LAS_GUN:
      lasGunFire( ent );
      break;
    case WP_PAIN_SAW:
      painSawFire( ent );
      break;
    case WP_GRENADE:
      throwGrenade( ent );
      break;
    case WP_FRAGNADE:
      throwFragnade( ent );
      break;
    case WP_LAUNCHER:
      launcherFire(ent, qtrue);
      break;

    case WP_LIGHTNING:
      lightningBoltFire( ent );
      break;

    case WP_LOCKBLOB_LAUNCHER:
      lockBlobLauncherFire( ent );
      break;
    case WP_HIVE:
      hiveFire( ent );
      break;
    case WP_TESLAGEN:
      teslaFire( ent );
      break;
    case WP_MGTURRET:
      bulletFire( ent, MGTURRET_SPREAD, MGTURRET_DMG, MOD_MGTURRET );
      break;

    case WP_ABUILD:
    case WP_ABUILD2:
      buildFire( ent, MN_A_BUILD );
      break;
    case WP_HBUILD:
    case WP_HBUILD2:
      buildFire( ent, MN_H_BUILD );
      break;
    default:
      break;
  }
}
