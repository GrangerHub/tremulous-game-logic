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

  ps->persistant[ PERS_NEWWEAPON ] = ( weapon == WP_BLASTER ) ?
    WP_BLASTER : ( ps->stats[ STAT_WEAPON ] != WP_NONE ?
        ps->stats[ STAT_WEAPON ] : WP_BLASTER );

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
      BG_InventoryContainsUpgrade( UP_BATTPACK, ent->client->ps.stats ) )
  {
    maxAmmo *= BATTPACK_MODIFIER;
  }

  roundDiff = maxAmmo - ent->client->ps.ammo;

  // charge for ammo when applicable
  if(  !( IS_WARMUP && BG_Weapon( weapon )->warmupFree ) &&
       roundPrice && !BG_Weapon( weapon )->usesEnergy )
  {
    int clipPrice = roundPrice * BG_Weapon( weapon )->maxClips;
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
      BG_InventoryContainsUpgrade( UP_BATTPACK, ent->client->ps.stats ) ) {
    maxAmmo *= BATTPACK_MODIFIER;
  }

  if( ent->client->ps.ammo > maxAmmo )
    ent->client->ps.ammo = maxAmmo;

  ent->client->ps.clips += clips;
  if( ent->client->ps.clips > BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->maxClips )
    ent->client->ps.clips = BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->maxClips;

  G_ForceWeaponChange( ent, ent->client->ps.weapon );

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
                             float width, float height, const float height_offset,
                             gentity_t **target) {
  vec3_t                   mins, maxs;
  vec3_t                   end;
  float                    viewheight_backup;
  unlagged_attacker_data_t unlagged_attacker;
  trace_t                  tr2;

  VectorSet( mins, -width, -width, -height );
  VectorSet( maxs, width, width, height );

  *target = NULL;

  if(!ent->client) {
    return;
  }

  if(height_offset) {
    viewheight_backup = ent->client->ps.viewheight;
    ent->client->ps.viewheight += height_offset;
  }

  unlagged_attacker.ent_num = ent->s.number;
  unlagged_attacker.point_type = UNLGD_PNT_MUZZLE;
  unlagged_attacker.range = range + width;
  G_UnlaggedOn(&unlagged_attacker);

  if(height_offset) {
    ent->client->ps.viewheight = viewheight_backup;
  }

  VectorMA(unlagged_attacker.muzzle_out, range, unlagged_attacker.forward_out, end);

  // Trace against entities and the world
  SV_Trace(
    tr, unlagged_attacker.muzzle_out, mins, maxs, end, ent->s.number,
    MASK_SHOT, TT_AABB);
  if(tr->entityNum != ENTITYNUM_NONE) {
    *target = &g_entities[ tr->entityNum ];
  }

  if(tr->startsolid) {
    //check for collision against the world
    SV_Trace(
      &tr2, unlagged_attacker.muzzle_out, mins, maxs, unlagged_attacker.muzzle_out,
      ent->s.number,
      MASK_SOLID, TT_AABB );
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

    G_WideTraceSolid(tr, ent, range, widthAdjusted, heightAdjusted, 0, target);
    if(
      tr->startsolid ||
      (*target != NULL && G_TakesDamage(*target))) {
      return;
    }
  }

  G_WideTraceSolid(tr, ent, range, width, height, 0, target);
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
                  float height_offset, int damage, meansOfDeath_t mod )
{
  trace_t   tr;
  gentity_t *traceEnt;

  G_WideTraceSolid( &tr, ent, range, width, height, height_offset, &traceEnt );
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
  trace_t                  tr;
  vec3_t                   end, muzzle_temp, forward_temp;
  float                    t, u, r, x, y;
  gentity_t                *tent;
  gentity_t                *traceEnt;
  unlagged_attacker_data_t unlagged_attacker;

  spread *= 16;

  t = 2 * M_PI * random( );
  u = random( ) + random( );
  r = u > 1? 2 - u : u;
  r *= spread;
  x = r * cos( t );
  y = r * sin( t );

  // don't use unlagged if this is not a client (e.g. turret)
  if( ent->client ) {
    unlagged_attacker.ent_num = ent->s.number;
    unlagged_attacker.point_type = UNLGD_PNT_MUZZLE;
    unlagged_attacker.range = 8192 * 16;
    G_UnlaggedOn(&unlagged_attacker);
    VectorMA(
      unlagged_attacker.muzzle_out, 8192 * 16,
      unlagged_attacker.forward_out, end );
    VectorMA(end, x, unlagged_attacker.right_out, end);
    VectorMA(end, y, unlagged_attacker.up_out, end);
    SV_Trace(
      &tr, unlagged_attacker.muzzle_out, NULL, NULL, end, ent->s.number,
      MASK_SHOT, TT_AABB );
    G_UnlaggedOff( );

    VectorCopy(unlagged_attacker.muzzle_out, muzzle_temp);
    VectorCopy(unlagged_attacker.forward_out, forward_temp);
  }
  else {
    VectorMA( muzzle, 8192 * 16, forward, end );
    VectorMA( end, x, right, end );
    VectorMA( end, y, up, end );

    SV_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT, TT_AABB );

    VectorCopy(muzzle, muzzle_temp);
    VectorCopy(forward, forward_temp);
  }

  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  // snap the endpos to integers, but nudged towards the line
  SnapVectorTowards( tr.endpos, muzzle_temp );

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
    G_Damage( traceEnt, ent, ent, forward_temp, tr.endpos,
      damage, 0, mod );
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
  unlagged_attacker_data_t unlagged_attacker;

  unlagged_attacker.ent_num = ent->s.number;
  unlagged_attacker.point_type = UNLGD_PNT_MUZZLE;
  unlagged_attacker.range = SHOTGUN_RANGE;
  G_UnlaggedOn(&unlagged_attacker);
  G_SplatterFire(
    ent, ent, unlagged_attacker.muzzle_out, unlagged_attacker.forward_out,
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
  trace_t                  tr;
  vec3_t                   end;
  gentity_t                *tent;
  gentity_t                *traceEnt;
  unlagged_attacker_data_t unlagged_attacker;

  unlagged_attacker.ent_num = ent->s.number;
  unlagged_attacker.point_type = UNLGD_PNT_MUZZLE;
  unlagged_attacker. range = 8192.0f * 16.0f;
  G_UnlaggedOn(&unlagged_attacker);
  VectorMA(
    unlagged_attacker.muzzle_out, 8192.0f * 16.0f,
    unlagged_attacker.forward_out, end);
  SV_Trace(
    &tr, unlagged_attacker.muzzle_out, NULL, NULL, end, ent->s.number,
    MASK_SHOT, TT_AABB);
  G_UnlaggedOff( );

  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  // snap the endpos to integers, but nudged towards the line
  SnapVectorTowards( tr.endpos, unlagged_attacker.muzzle_out );

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
  {
    G_Damage( traceEnt, ent, ent, unlagged_attacker.forward_out, tr.endpos,
      MDRIVER_DMG, 0, MOD_MDRIVER );
  }
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
  trace_t                  tr;
  vec3_t                   end;
  gentity_t                *tent;
  gentity_t                *traceEnt;
  unlagged_attacker_data_t unlagged_attacker;

  unlagged_attacker.ent_num = ent->s.number;
  unlagged_attacker.point_type = UNLGD_PNT_MUZZLE;
  unlagged_attacker.range = 8192 * 16;
  G_UnlaggedOn(&unlagged_attacker);
  VectorMA(
    unlagged_attacker.muzzle_out, 8192 * 16,
    unlagged_attacker.forward_out, end);
  SV_Trace(
    &tr, unlagged_attacker.muzzle_out, NULL, NULL, end, ent->s.number,
    MASK_SHOT, TT_AABB);
  G_UnlaggedOff( );

  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  // snap the endpos to integers, but nudged towards the line
  SnapVectorTowards( tr.endpos, unlagged_attacker.muzzle_out );

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
    G_Damage(
      traceEnt, ent, ent, unlagged_attacker.forward_out, tr.endpos,
      LASGUN_DAMAGE, 0, MOD_LASGUN );
}

/*
======================================================================

PAIN SAW

======================================================================
*/

void painSawFire( gentity_t *ent )
{
  trace_t   tr;
  vec3_t    temp;
  gentity_t *tent, *traceEnt;

  G_WideTraceSolid( &tr, ent, PAINSAW_RANGE, PAINSAW_WIDTH, PAINSAW_HEIGHT,
               0, &traceEnt );
  if( !traceEnt || !G_TakesDamage( traceEnt ) )
    return;

  // hack to line up particle system with weapon model
  tr.endpos[ 2 ] -= 5.0f;

  // send blood impact
  if( traceEnt->s.eType == ET_PLAYER || traceEnt->s.eType == ET_BUILDABLE )
  {
      BloodSpurt( ent, traceEnt, &tr );
  }
  else
  {
    VectorCopy( tr.endpos, temp );
    tent = G_TempEntity( temp, EV_MISSILE_MISS );
    tent->s.eventParm = DirToByte( tr.plane.normal );
    tent->s.weapon = ent->s.weapon;
    tent->s.generic1 = ent->s.generic1; //weaponMode
  }

  G_Damage( traceEnt, ent, ent, forward, tr.endpos, PAINSAW_DAMAGE, DAMAGE_NO_KNOCKBACK, MOD_PAINSAW );
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
  if( self->enemy->s.eType != ET_TELEPORTAL )
    SV_Trace( &tr, origin, NULL, NULL, target, self->s.number, MASK_SHOT, TT_AABB );
  else
    SV_Trace( &tr, origin, NULL, NULL, target, self->s.number,
                (MASK_SHOT|CONTENTS_TELEPORTER), TT_AABB );
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
              TESLAGEN_DMG, 0, MOD_TESLAGEN );
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
void CheckCkitRepair( gentity_t *ent )
{
  vec3_t      viewOrigin, forward2, end;
  trace_t     tr;
  gentity_t   *traceEnt;
  int         bHealth;

  if( ent->client->pmext.repairRepeatDelay > 0 ||
      ent->client->ps.misc[ MISC_MISC ] > 0 )
    return;

  BG_GetClientViewOrigin( &ent->client->ps, viewOrigin );
  AngleVectors( ent->client->ps.viewangles, forward2, NULL, NULL );
  VectorMA( viewOrigin, 100, forward2, end );

  G_SetPlayersLinkState( qfalse, ent );
  SV_Trace( &tr, viewOrigin, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID, TT_AABB );
  traceEnt = &g_entities[ tr.entityNum ];
  G_SetPlayersLinkState( qtrue, ent );

  if( tr.fraction < 1.0f && traceEnt->spawned && traceEnt->health > 0 &&
      traceEnt->s.eType == ET_BUILDABLE && traceEnt->buildableTeam == TEAM_HUMANS )
  {
    bHealth = BG_Buildable( traceEnt->s.modelindex )->health;
    if( traceEnt->health < bHealth )
    {
      gentity_t *tent;
      traceEnt->health += HBUILD_HEALRATE;
      if( traceEnt->health >= bHealth )
      {
        traceEnt->health = bHealth;
        G_AddEvent( ent, EV_BUILD_REPAIRED, 0 );
      }
      else
        G_AddEvent( ent, EV_BUILD_REPAIR, 0 );

      // send ckit build effects
      tent = G_TempEntity( tr.endpos, EV_BUILD_FIRE );
      tent->s.weapon = ent->s.weapon;
      tent->s.otherEntityNum = ent->s.number;
      tent->s.otherEntityNum2 = ENTITYNUM_NONE;
      tent->s.generic1 = WPM_SECONDARY;
      ent->client->ps.eFlags |= EF_FIRING;
      ent->client->buildFireTime = level.time + 250;

      ent->client->pmext.repairRepeatDelay += BG_Weapon( ent->client->ps.weapon )->repeatRate1;
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

  if( ent->client->ps.weapon == WP_ABUILD ||
      ent->client->ps.weapon == WP_ABUILD2 )
    meleeAttack( ent, ABUILDER_CLAW_RANGE, ABUILDER_CLAW_WIDTH,
                 ABUILDER_CLAW_WIDTH, 0, ABUILDER_CLAW_DMG, MOD_ABUILDER_CLAW );
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
    if( ent->client->ps.misc[ MISC_MISC ] > 0 )
    {
      G_AddEvent( ent, EV_BUILD_DELAY, ent->client->ps.clientNum );
      return;
    }

    if( G_BuildIfValid( ent, buildable ) )
    {
      if( !g_cheats.integer && !IS_WARMUP )
      {
        ent->client->ps.misc[ MISC_MISC ] +=
          BG_Buildable( buildable )->buildTime;
      }

      // send ckit build effects
      if( ent->client->ps.weapon == WP_HBUILD &&
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

      // end damage protection early
      ent->dmgProtectionTime = 0;

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

  // only allow bites to work against buildings as they are constructing
  if( traceEnt->s.eType == ET_BUILDABLE )
  {
    if( traceEnt->health <= 0 )
        return qfalse;

    if( ( traceEnt->spawned ) &&
        ( traceEnt->s.modelindex != BA_H_MGTURRET ) &&
        ( traceEnt->s.modelindex != BA_H_TESLAGEN ) )
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
  trace_t                  tr;
  vec3_t                   end, dir;
  gentity_t                *traceEnt;
  float                    range = LEVEL1_GRAB_RANGE;
  unlagged_attacker_data_t unlagged_attacker;

  if(ent->client->ps.weapon == WP_ALEVEL1_UPG) {
    range = LEVEL1_GRAB_U_RANGE;
  }

  unlagged_attacker.ent_num = ent->s.number;
  unlagged_attacker.point_type = UNLGD_PNT_MUZZLE;
  unlagged_attacker.range = range;
  G_UnlaggedOn(&unlagged_attacker);
  VectorMA(
    unlagged_attacker.muzzle_out, range, unlagged_attacker.forward_out, end);
  SV_Trace(
    &tr, unlagged_attacker.muzzle_out, NULL, NULL, end, ent->s.number,
    MASK_SHOT, TT_AABB );
  G_UnlaggedOff( );

  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  if( !G_TakesDamage( traceEnt ) )
    return;

  if( traceEnt->client )
  {
    if( traceEnt->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      return;

    if( traceEnt->client->ps.misc[ MISC_HEALTH ] <= 0 )
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
      traceEnt->client->grabExpiryTime = level.time + LEVEL1_GRAB_TIME;
    else if( ent->client->ps.weapon == WP_ALEVEL1_UPG )
      traceEnt->client->grabExpiryTime = level.time + LEVEL1_GRAB_U_TIME;
  } else if( traceEnt->s.eType == ET_BUILDABLE &&
      traceEnt->s.modelindex == BA_H_MGTURRET )
  {
    if( !traceEnt->lev1Grabbed ) {
      G_AddPredictableEvent( ent, EV_LEV1_GRAB, 0 );
    }

    traceEnt->lev1Grabbed = qtrue;
    traceEnt->lev1GrabTime = level.time;
  }
}

/*
===============
poisonCloud
===============
*/
void poisonCloud( gentity_t *ent )
{
  int                      entityList[ MAX_GENTITIES ];
  int                      i, num;
  gentity_t                *humanPlayer;
  trace_t                  tr;
  unlagged_attacker_data_t unlagged_attacker;
  vec3_t                   mins, maxs;
  vec3_t                   range =
    { LEVEL1_PCLOUD_RANGE, LEVEL1_PCLOUD_RANGE, LEVEL1_PCLOUD_RANGE };

  unlagged_attacker.ent_num = ent->s.number;
  unlagged_attacker.point_type = UNLGD_PNT_ORIGIN;
  unlagged_attacker.range = LEVEL1_PCLOUD_RANGE;
  G_UnlaggedOn(&unlagged_attacker);
  VectorAdd( unlagged_attacker.origin_out, range, maxs );
  VectorSubtract( unlagged_attacker.origin_out, range, mins );
  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
  {
    humanPlayer = &g_entities[ entityList[ i ] ];

    if( humanPlayer->client &&
        humanPlayer->client->pers.teamSelection == TEAM_HUMANS &&
        !BG_InventoryContainsUpgrade( UP_HELMET, humanPlayer->client->ps.stats ) &&
        !BG_InventoryContainsUpgrade( UP_BATTLESUIT, humanPlayer->client->ps.stats ))
    {
      SV_Trace(
        &tr, unlagged_attacker.muzzle_out, NULL, NULL,
        humanPlayer->r.currentOrigin, humanPlayer->s.number,
        CONTENTS_SOLID, TT_AABB );

      //can't see target from here
      if( tr.entityNum == ENTITYNUM_WORLD )
        continue;

      humanPlayer->client->ps.eFlags |= EF_POISONCLOUDED;
      humanPlayer->client->lastPoisonCloudedTime = level.time;
      humanPlayer->client->lastPoisonCloudedClient = ent;

      SV_GameSendServerCommand( humanPlayer->client->ps.clientNum,
                              "poisoncloud" );
    }
  }
  G_UnlaggedOff( );
}


/*
======================================================================

LEVEL2

======================================================================
*/

bglist_t *lev2ZapList = NULL;

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
G_CreateNewZap
===============
*/
static void G_CreateNewZap( gentity_t *creator, gentity_t *targetEnt )
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
G_UpdateZaps
===============
*/
void G_UpdateZaps( int msec )
{
  BG_List_Foreach( lev2ZapList, G_UpdateLev2Zap, &msec );
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
G_ClearPlayerZapEffects

called from G_LeaveTeam() and TeleportPlayer()
===============
*/
void G_ClearPlayerZapEffects( gentity_t *player )
{
  BG_List_Foreach( lev2ZapList, G_ClearPlayerLev2ZapEffects, player );
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
    G_CreateNewZap( ent, traceEnt );
  }
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
qboolean CheckPounceAttack( gentity_t *ent )
{
  trace_t tr;
  gentity_t *traceEnt;
  int damage, timeMax, pounceRange, payload;

  if( ent->client->pmext.pouncePayload <= 0 )
    return qfalse;

  // In case the goon lands on his target, he gets one shot after landing
  payload = ent->client->pmext.pouncePayload;
  if( !( ent->client->ps.pm_flags & PMF_CHARGE ) )
    ent->client->pmext.pouncePayload = 0;

  traceEnt = & g_entities[ent->client->ps.groundEntityNum];

  if(ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
    gentity_t *groundEnt = & g_entities[ent->client->ps.groundEntityNum];

    if(G_TakesDamage(groundEnt)) {
      //landed on something that can take damage, so use that as the target
      traceEnt = groundEnt;
    } else {
      //landed on something that can't be damaged, so disarm the pounce
      traceEnt = NULL;
    }
  } else {
    //check for mid air pounce collision

    // Calculate muzzle point
    AngleVectors( ent->client->ps.viewangles, forward, right, up );
    BG_CalcMuzzlePointFromPS( &ent->client->ps, forward, right, up, muzzle );

    // Trace from muzzle to see what we hit
    pounceRange = ent->client->ps.weapon == WP_ALEVEL3 ? LEVEL3_POUNCE_RANGE :
                                                         LEVEL3_POUNCE_UPG_RANGE;
    G_WideTraceSolidSeries( &tr, ent, pounceRange, LEVEL3_POUNCE_WIDTH,
                            LEVEL3_POUNCE_WIDTH, &traceEnt);
  }

  if( traceEnt == NULL )
    return qfalse;

  // Send blood impact
  if( G_TakesDamage( traceEnt ) )
    WideBloodSpurt( ent, traceEnt, &tr );

  if( !G_TakesDamage( traceEnt ) )
    return qfalse;

  // Deal damage
  timeMax = ent->client->ps.weapon == WP_ALEVEL3 ? LEVEL3_POUNCE_TIME :
                                                   LEVEL3_POUNCE_TIME_UPG;
  damage = payload * LEVEL3_POUNCE_DMG / timeMax;
  ent->client->pmext.pouncePayload = 0;
  if(traceEnt->client) {
    damage =  damage / 2;
  }
  G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage,
            DAMAGE_NO_LOCDAMAGE, MOD_LEVEL3_POUNCE );

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
  vec3_t    forward2;

  if( ent->client->ps.misc[ MISC_MISC ] <= 0 ||
      !( ent->client->ps.stats[ STAT_STATE ] & SS_CHARGING ) ||
      ent->client->ps.weaponTime )
    return;

  VectorSubtract( victim->r.currentOrigin, ent->r.currentOrigin, forward2 );
  VectorNormalize( forward2 );

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

  G_Damage( victim, ent, ent, forward2, victim->r.currentOrigin, damage,
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
                   0, LEVEL1_CLAW_DMG, MOD_LEVEL1_CLAW );
      break;
    case WP_ALEVEL1_UPG:
      meleeAttack( ent, LEVEL1_CLAW_U_RANGE, LEVEL1_CLAW_WIDTH, LEVEL1_CLAW_WIDTH,
                   0, LEVEL1_CLAW_DMG, MOD_LEVEL1_CLAW );
      break;
    case WP_ALEVEL3:
      meleeAttack( ent, LEVEL3_CLAW_RANGE, LEVEL3_CLAW_WIDTH, LEVEL3_CLAW_WIDTH,
                   0, LEVEL3_CLAW_DMG, MOD_LEVEL3_CLAW );
      break;
    case WP_ALEVEL3_UPG:
      meleeAttack( ent, LEVEL3_CLAW_UPG_RANGE, LEVEL3_CLAW_WIDTH,
                   LEVEL3_CLAW_WIDTH, 0, LEVEL3_CLAW_DMG, MOD_LEVEL3_CLAW );
      break;
    case WP_ALEVEL2:
      meleeAttack( ent, LEVEL2_CLAW_RANGE, LEVEL2_CLAW_WIDTH, LEVEL2_CLAW_WIDTH,
                   0, LEVEL2_CLAW_DMG, MOD_LEVEL2_CLAW );
      break;
    case WP_ALEVEL2_UPG:
      meleeAttack( ent, LEVEL2_CLAW_U_RANGE, LEVEL2_CLAW_WIDTH, LEVEL2_CLAW_WIDTH,
                   0, LEVEL2_CLAW_DMG, MOD_LEVEL2_CLAW );
      break;
    case WP_ALEVEL4:
      meleeAttack( ent, LEVEL4_CLAW_RANGE, LEVEL4_CLAW_WIDTH,
                   LEVEL4_CLAW_HEIGHT, LEVEL4_CLAW_OFFSET, LEVEL4_CLAW_DMG,
                   MOD_LEVEL4_CLAW );
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
      buildFire( ent, MN_H_BUILD );
      break;
    default:
      break;
  }
}
