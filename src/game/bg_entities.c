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

#include "../qcommon/q_shared.h"
#include "bg_public.h"

typedef struct bgentity_s
{
  entityState_t  *es;
  playerState_t  *ps;

  const qboolean *valid_entityState;
  const qboolean *linked;

  const team_t   *client_team;
} bgentity_t;


static int        cgame_local_client_num; // this should be ENTITYNUM_NONE in the SGAME
static bgentity_t bg_entities[ MAX_GENTITIES ];


/*
============
BG_Init_Entities

Initialized in the sgame and cgame
============
*/
void BG_Init_Entities(const int cgame_client_num) {
  memset( bg_entities, 0, MAX_GENTITIES * sizeof( bg_entities[ 0 ] ) );
  cgame_local_client_num = cgame_client_num;
}

/*
============
BG_entityState_From_Ent_Num

============
*/
entityState_t *BG_entityState_From_Ent_Num(int ent_num) {
  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  if(
    bg_entities[ent_num].valid_entityState &&
    *bg_entities[ent_num].valid_entityState) {
    return bg_entities[ent_num].es;
  } else {
    return NULL;
  }
}

/*
============
BG_Locate_Entity_Data

============
*/
void BG_Locate_Entity_Data(
  int ent_num, entityState_t *es, playerState_t *ps,
  const qboolean *valid_check_var, const qboolean *linked_var,
  const team_t *client_team_var) {
  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);
  Com_Assert(es);
  Com_Assert(valid_check_var);

  bg_entities[ent_num].ps = ps;
  bg_entities[ent_num].es = es;
  bg_entities[ent_num].valid_entityState = valid_check_var;
  bg_entities[ent_num].linked = valid_check_var;
  bg_entities[ent_num].client_team = client_team_var;
}

/*
============
BG_playerState_From_Ent_Num

============
*/
playerState_t *BG_playerState_From_Ent_Num(int ent_num) {
  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  if(ent_num < MAX_CLIENTS) {
    return bg_entities[ent_num].ps;
  } else {
    return NULL;
  }
}

/*
============
BG_Entity_Is_Valid

============
*/
qboolean BG_Entity_Is_Valid(int ent_num) {
  bgentity_t *ent;

  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  ent = &bg_entities[ent_num];
  if(
    cgame_local_client_num != ENTITYNUM_NONE &&
    cgame_local_client_num == ent_num) {
    return qtrue;
  }

  if(!(ent->valid_entityState) || !(*(ent->valid_entityState))) {
    return qfalse;
  }

  return qtrue;
}

/*
============
BG_Entity_Is_Linked

============
*/
qboolean BG_Entity_Is_Linked(int ent_num) {
  bgentity_t *ent;

  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  if(!BG_Entity_Is_Valid(ent_num)) {
    return qfalse;
  }

  ent = &bg_entities[ent_num];

  if(ent->linked && !(*(ent->linked))) {
    return qfalse;
  }

  return qtrue;
}



/*
================================================================================
Unique Entity IDs

Unique Entity IDs (UEID) distinguish different entities that have reused the
same entity slot, and is more reliable than entity pointers and slot numbers.
UEID numbers are initialized in the game module, and stored in origin[0] of the
entityState and in MISC_ID in the playerState so that it can be communicated to
the cgame.  Don't access origin[0] directly, instead use the UEID functions.
================================================================================
*/

/*
============
BG_UEID_set

Sets a UEID to point to a particular entity, but clears the UEID if attempted to
point to ENTITYNUM_NONE or an invalid entity.
============
*/
void BG_UEID_set(bgentity_id *ueid, int ent_num) {
  int *id_pointer;
  Com_Assert(ueid);
  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  if(ent_num == ENTITYNUM_NONE) {
    ueid->ent_num = ENTITYNUM_NONE;
    ueid->id = 0;
    return;
  }

  //clear the ueid if attempting to point to an invalid entity
  if(
    (
      cgame_local_client_num == ENTITYNUM_NONE ||
      ent_num != cgame_local_client_num) &&
    !(*bg_entities[ent_num].valid_entityState)) {
      ueid->ent_num = ENTITYNUM_NONE;
      ueid->id = 0;
      return;
  }

  ueid->ent_num = ent_num;
  if(ent_num < MAX_CLIENTS) {
    if(
      cgame_local_client_num == ent_num ||
      cgame_local_client_num == ENTITYNUM_NONE) {
      ueid->id = bg_entities[ent_num].ps->misc[MISC_ID];
      return;
    }
  }

  id_pointer = (int *)(&bg_entities[ent_num].es->origin[0]);
  ueid->id = *id_pointer;
}

/*
============
BG_UEID_get_ent_num

If the UEID is still valid, this function returns the entity number it
points to, otherwise ENTITYNUM_NONE is returned.
============
*/
int BG_UEID_get_ent_num(bgentity_id *ueid) {
  int *id_pointer;

  Com_Assert(ueid);

  if(ueid->ent_num == ENTITYNUM_NONE){
    ueid->id = 0;
    return ENTITYNUM_NONE;
  }

  if(ueid->id == 0) {
    ueid->ent_num = ENTITYNUM_NONE;
    return ENTITYNUM_NONE;
  }

  if(!BG_Entity_Is_Valid(ueid->ent_num)) {
    if(cgame_local_client_num != ENTITYNUM_NONE) {
      //if the entity isn't valid CGAME side, return ENTITYNUM_NONE but
      //don't reset the UEID
      return ENTITYNUM_NONE;
    }

    ueid->ent_num = ENTITYNUM_NONE;
    ueid->id = 0;
    return ENTITYNUM_NONE;
  }

  if(ueid->ent_num < MAX_CLIENTS) {
    if(
      cgame_local_client_num == ueid->ent_num ||
      cgame_local_client_num == ENTITYNUM_NONE) {
      Com_Assert(bg_entities[ueid->ent_num].ps);

      if(ueid->id != bg_entities[ueid->ent_num].ps->misc[MISC_ID]) {
        ueid->ent_num = ENTITYNUM_NONE;
        ueid->id = 0;
        return ENTITYNUM_NONE;
      }
    }
  }

  id_pointer = (int *)(&bg_entities[ueid->ent_num].es->origin[0]);
  if(ueid->id != *id_pointer){
    ueid->ent_num = ENTITYNUM_NONE;
    ueid->id = 0;
  }
  return ueid->ent_num;
}

/*
================================================================================
BGAME Collision Functions

Collision related functions that can be used in bgame files for
prediction purposes
================================================================================
*/

static bg_collision_funcs_t bg_collision_funcs;

/*
============
BG_Init_Collision_Functions
============
*/
void BG_Init_Collision_Functions(bg_collision_funcs_t funcs) {
  bg_collision_funcs = funcs;
}

/*
============
BG_Trace
============
*/
void BG_Trace(
  trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs,
  const vec3_t end, int passEntityNum, const content_mask_t content_mask,
  traceType_t type) {
  Com_Assert(bg_collision_funcs.trace);

  bg_collision_funcs.trace(
    results, start, mins, maxs, end, passEntityNum, content_mask, type);
}

/*
============
BG_Area_Entities
============
*/
void BG_Area_Entities(
  const vec3_t mins, const vec3_t maxs, const content_mask_t *content_mask,
  int *entityList, int maxcount) {
  Com_Assert(bg_collision_funcs.area_entities);

  bg_collision_funcs.area_entities(
    mins, maxs, content_mask, entityList, maxcount);
}

/*
============
BG_Clip_To_Entity
============
*/
void BG_Clip_To_Entity(
    trace_t *trace, const vec3_t start, vec3_t mins, vec3_t maxs,
    const vec3_t end, int entityNum, const content_mask_t content_mask,
    traceType_t collisionType) {
  Com_Assert(bg_collision_funcs.clip_to_entity);

  bg_collision_funcs.clip_to_entity(
    trace, start, mins, maxs, end, entityNum, content_mask, collisionType);
}

/*
============
BG_Clip_To_Test_Area
============
*/
void BG_Clip_To_Test_Area(
  trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs,
  const vec3_t end, const vec3_t test_mins, const vec3_t test_maxs,
  const vec3_t test_origin, int test_contents, const content_mask_t content_mask,
  traceType_t collisionType) {
  Com_Assert(bg_collision_funcs.clip_to_test_area);

  bg_collision_funcs.clip_to_test_area(
    results, start, mins, maxs, end, test_mins, test_maxs, test_origin,
    test_contents, content_mask, collisionType);
}

/*
============
BG_PointContents
============
*/
int BG_PointContents(const vec3_t p, int passEntityNum) {
  Com_Assert(bg_collision_funcs.pointcontents);

  return bg_collision_funcs.pointcontents(p, passEntityNum);
}

/*
===================
BG_UnlaggedOn
===================
*/
void BG_UnlaggedOn(unlagged_attacker_data_t *data) {
  if(bg_collision_funcs.unlagged_on) {
    bg_collision_funcs.unlagged_on(data);
  } else {
    playerState_t *ps;
    Com_Assert(cgame_local_client_num == data->ent_num);

    ps = BG_playerState_From_Ent_Num(data->ent_num);

    BG_CalcMuzzlePointFromPS(
      ps,
      data->forward_out,
      data->right_out,
      data->up_out,
      data->muzzle_out);
    VectorCopy(ps->origin, data->origin_out);
  }
}

/*
===================
BG_UnlaggedOff
===================
*/
void BG_UnlaggedOff(void) {
  if(bg_collision_funcs.unlagged_off) {
    bg_collision_funcs.unlagged_off( );
  }
}

/*
================================================================================
Splatter Patterns
================================================================================
*/

//TODO: redo all of the angles, vectors, and math code, and look into quaternions!

typedef struct splatterPatternData_s
{
  const splatterAttributes_t *splatter;
  int                        mode_index;
  int                        seed;
  int                        ammo_used;
  const int                  *fragment_num;
  weapon_t                   weapon;

  float (*distribution)( struct splatterPatternData_s *data, angleIndex_t angle_index );
  void (*pattern)( struct splatterPatternData_s *data, vec3_t out );
} splatterPatternData_t;

/*
==============
BG_SplatterRandom
==============
*/
static float BG_SplatterRandom( splatterPatternData_t *data, angleIndex_t angle_index ) {
  Com_Assert( data );

  return Q_random( &data->seed );
}

/*
==============
BG_SplatterRandom
==============
*/
static int BG_SplatterPatternNumber(splatterPatternData_t *data) {
  int ammo_usage;

  switch(data->mode_index) {
    case 0:
      ammo_usage = BG_Weapon(data->weapon)->ammoUsage1;
      break;

    case 1:
      ammo_usage = BG_Weapon(data->weapon)->ammoUsage2;
      break;

    case 2:
      ammo_usage = BG_Weapon(data->weapon)->ammoUsage1;
      break;

    default:
      ammo_usage = 1;
      break;
  }

  if(
    BG_Weapon(data->weapon)->allowPartialAmmoUsage &&
    data->ammo_used < ammo_usage) {
    return (data->splatter->number * data->ammo_used) / ammo_usage;
  } else {
    return data->splatter->number;
  }
}

/*
==============
BG_SplatterUniform
==============
*/
static float BG_SplatterUniform( splatterPatternData_t *data, angleIndex_t angle_index ) {
  const int yaw_layers =
    BG_SplatterPatternNumber(data) / data->splatter->pitchLayers;

  Com_Assert( data );
  Com_Assert( data->fragment_num );

  if( angle_index == YAW ) {
    const int yaw_layer_num = *data->fragment_num % yaw_layers;
    float yaw_position = ( ( data->seed & 0xffff ) / (float)0x10000 ); // start the overall pattern at a random YAW

    yaw_position += ( (float)yaw_layer_num ) / ( (float)( yaw_layers ) );
    return yaw_position - ( (int)yaw_position );
  }

  //at this point angle_index must be PITCH
  Com_Assert( angle_index == PITCH );

  {
    const int pitch_layer_num = *data->fragment_num / yaw_layers;

    return ( (float)pitch_layer_num ) / ( (float)data->splatter->pitchLayers - 1 );
  }
}

/*
==============
BG_SplatterUniformAlternating
==============
*/
static float BG_SplatterUniformAlternating( splatterPatternData_t *data, angleIndex_t angle_index ) {
  const int yaw_layers =
    BG_SplatterPatternNumber(data) / data->splatter->pitchLayers;

  Com_Assert( data );
  Com_Assert( data->fragment_num );

  if( angle_index == YAW ) {
    const int pitch_layer_num = *data->fragment_num / yaw_layers;
    const int yaw_layer_num = *data->fragment_num % yaw_layers;
    float yaw_position = ( ( data->seed & 0xffff ) / (float)0x10000 ); // start the overall pattern at a random YAW

    //alternate by a half yaw position between pitch layers
    yaw_position += 0.5 * ( ( (float)pitch_layer_num ) / ( (float)yaw_layers ) );

    yaw_position += ( (float)yaw_layer_num ) / ( (float)( yaw_layers - 1 ) );
    return yaw_position - ( (int)yaw_position );
  }

  //at this point angle_index must be PITCH
  Com_Assert( angle_index == PITCH );

  {
    const int pitch_layer_num = *data->fragment_num / yaw_layers;

    return ( (float)pitch_layer_num ) / ( (float)( data->splatter->pitchLayers - 1 ) );
  }
}

/*
==============
BG_SplatterSphericalCone
==============
*/
static void BG_SplatterSphericalCone( splatterPatternData_t *data, vec3_t out ) {
  vec3_t splatter_angles;

  Com_Assert( data );
  Com_Assert( data->distribution );
  Com_Assert( out );

  splatter_angles[PITCH] =  data->distribution( data, PITCH ) * data->splatter->spread;
  AngleNormalize180( splatter_angles[PITCH] );
  splatter_angles[PITCH] -= 90; //the spread angle is in relation to pointing straight up
  splatter_angles[YAW] = data->distribution( data, YAW ) * 360;
  AngleNormalize360( splatter_angles[YAW] );
  splatter_angles[ROLL] = 0;

  AngleVectors( splatter_angles, out, NULL, NULL );
  VectorNormalize( out );
}

/*
==============
BG_SplatterMirroredInverseSphericalCone
==============
*/
static void BG_SplatterMirroredInverseSphericalCone( splatterPatternData_t *data, vec3_t out ) {
  vec3_t splatter_angles;

  Com_Assert( data );
  Com_Assert( data->distribution );
  Com_Assert( out );

  splatter_angles[PITCH] =  data->distribution( data, PITCH ) * data->splatter->spread;
  AngleNormalize180( splatter_angles[PITCH] );
  splatter_angles[PITCH] -= ( data->splatter->spread * 0.5f ); //the spread angle is centered at the horizontal
  splatter_angles[YAW] = data->distribution( data, YAW ) * 360;
  AngleNormalize360( splatter_angles[YAW] );
  splatter_angles[ROLL] = 0;

  AngleVectors( splatter_angles, out, NULL, NULL );
  VectorNormalize( out );
}

/*
==============
BG_SplatterPattern

For the shotgun, frag nade, acidtubes, flamer, etc...
==============
*/
void BG_SplatterPattern(
  vec3_t origin2, int seed, int passEntNum, splatterData_t *data,
  void (*func)( splatterData_t *data)) {
  int i;
  const int modeIndex = data->weaponMode - 1;
  weapon_t weapon = data->weapon;
  vec3_t    origin, forward, cross;
  const vec3_t    up_absolute = { 0.0f, 0.0f, 1.0f };
  float  rotation_angle, cross_length, dot;
  trace_t   tr;
  splatterPatternData_t splatterData;

  Com_Assert( modeIndex >= 0 &&
              modeIndex < 3 );

  memset( &splatterData, 0, sizeof( splatterData ) );
  splatterData.splatter = &BG_Weapon( weapon )->splatter[modeIndex];
  splatterData.weapon = weapon;
  splatterData.mode_index = modeIndex;
  splatterData.ammo_used = data->ammo_used;

  Com_Assert( func );
  Com_Assert( splatterData.splatter );
  Com_Assert( splatterData.splatter->spread >= 0 &&
              splatterData.splatter->spread <= 180 );
  Com_Assert( ( splatterData.distribution == SPLATD_RANDOM ||
                splatterData.splatter->pitchLayers > 0 ) );
  Com_Assert( splatterData.distribution == SPLATD_RANDOM ||
              splatterData.splatter->pitchLayers <
              BG_SplatterPatternNumber(&splatterData) );
  Com_Assert( splatterData.distribution == SPLATD_RANDOM ||
              !( BG_SplatterPatternNumber(&splatterData) % splatterData.splatter->pitchLayers ) );

  splatterData.seed = seed;

  VectorCopy( data->origin, origin );

  //select the pattern type
  switch ( splatterData.splatter->pattern ) {
    case SPLATP_SPHERICAL_CONE:
    splatterData.pattern = BG_SplatterSphericalCone;
      break;

    case SPLATP_MIRRORED_INVERSE_SPHERICAL_CONE:
      splatterData.pattern = BG_SplatterMirroredInverseSphericalCone;
      break;
  }

  Com_Assert( splatterData.pattern );

  switch( splatterData.splatter->distribution ) {
    case SPLATD_RANDOM:
      splatterData.distribution = BG_SplatterRandom;
      break;

    case SPLATD_UNIFORM:
      splatterData.distribution = BG_SplatterUniform;
      break;

    case SPLATD_UNIFORM_ALTERNATING:
      splatterData.distribution = BG_SplatterUniformAlternating;
      break;
  }

  Com_Assert( splatterData.distribution );

  //prepare for rotation to the facing direction
  VectorCopy( origin2, forward );
  CrossProduct( up_absolute, forward, cross );
  cross_length = VectorLength( cross );
  dot = DotProduct( up_absolute, forward );

  if( cross_length > 0 ) {
    VectorNormalize( cross );
    rotation_angle = RAD2DEG( atan2( cross_length, dot) );
  } else if( dot > 0.0f ) {
    rotation_angle = 0;
  } else {
    rotation_angle = 180;
  }

  // generate the pattern
  for( i = 0; i < BG_SplatterPatternNumber(&splatterData); i++ ) {
    vec3_t dir, temp, end;

    splatterData.fragment_num = &i;

    //get the next pattern vector
    splatterData.pattern( &splatterData, temp );

    //rotate toward the facing direction
    if( cross_length > 0 ) {
      RotatePointAroundVector( dir, cross, temp, rotation_angle );
    } else if( dot > 0.0f ){
      VectorCopy( temp, dir );
    } else {
      VectorScale( temp, -1.0f, dir );
    }

    VectorMA( origin, splatterData.splatter->range, dir, end );

    //apply the impact
    BG_Trace(
      &tr, origin, NULL, NULL, end, passEntNum, *Temp_Clip_Mask(MASK_SHOT, 0),
      TT_AABB);
    data->tr = &tr;
    func( data );
  }
}

/*
================================================================================
Lightning Gun Prediction
================================================================================
*/

/*
===============
BG_LightningBoltRange

Finds the current lightning bolt range of a charged lightning gun
===============
*/
int BG_LightningBoltRange( const entityState_t *es,
                           const playerState_t *ps,
                           qboolean currentRange )
{
  if( ps )
  {
    int charge;

    Com_Assert( ps->weapon == WP_LIGHTNING );

    if( currentRange )
      charge = ps->misc[ MISC_MISC ];
    else
      charge = ps->stats[ STAT_MISC2 ];

    return ( charge * LIGHTNING_BOLT_RANGE_MAX ) / LIGHTNING_BOLT_CHARGE_TIME_MAX;
  }
  
  Com_Assert( es );
  Com_Assert( es->weapon == WP_LIGHTNING );
  return ( es->constantLight * LIGHTNING_BOLT_RANGE_MAX ) / LIGHTNING_BOLT_CHARGE_TIME_MAX;
}

/*
===============
BG_CheckBoltImpactTrigger

For firing lightning bolts early
===============
*/
void BG_CheckBoltImpactTrigger(pmove_t *pm) {
  unlagged_attacker_data_t attacker_data;

  if(
    pm->ps->weapon == WP_LIGHTNING &&
    pm->ps->misc[ MISC_MISC ] > LIGHTNING_BOLT_CHARGE_TIME_MIN &&
    pm->ps->misc[ MISC_MISC ] - pm->ps->stats[ STAT_MISC3 ] > 50 ) {
    vec3_t end;

    attacker_data.ent_num = pm->ps->clientNum;
    attacker_data.point_type = UNLGD_PNT_MUZZLE;
    attacker_data.range = BG_LightningBoltRange(NULL, pm->ps, qtrue);
    BG_UnlaggedOn(&attacker_data);

    VectorMA(attacker_data.muzzle_out, BG_LightningBoltRange(NULL, pm->ps, qtrue),
              attacker_data.forward_out, end );

    BG_Trace(
      &pm->pmext->impactTriggerTrace, attacker_data.muzzle_out, NULL, NULL, end,
      pm->ps->clientNum, *Temp_Clip_Mask(MASK_SHOT, 0), TT_AABB );

    BG_UnlaggedOff();

    pm->ps->stats[ STAT_MISC3 ] = pm->ps->misc[ MISC_MISC ];
    pm->pmext->impactTriggerTraceChecked = qtrue;
  } else
    pm->pmext->impactTriggerTraceChecked = qfalse;
}

/*
===============
BG_ResetLightningBoltCharge

Resets the charging of lightning bolts
===============
*/
void BG_ResetLightningBoltCharge( playerState_t *ps, pmoveExt_t *pmext )
{
  if( ps->weapon != WP_LIGHTNING )
    return;

  ps->stats[ STAT_MISC3 ] = 0;
  ps->misc[ MISC_MISC ] = 0;
  pmext->impactTriggerTraceChecked = qfalse;
}

/*
================================================================================
Buildable Prediction
================================================================================
*/



/*
===============
BG_FindValidSpot
===============
*/
qboolean BG_FindValidSpot(
  trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end,
  int passEntityNum, const content_mask_t content_mask, float incDist, int limit) {
  vec3_t start2, increment;

  VectorCopy(start, start2);

  VectorSubtract(end, start2, increment);
  VectorNormalize(increment);
  VectorScale(increment, incDist, increment);

  do {
    BG_Trace(tr, start2, mins, maxs, end, passEntityNum, content_mask, TT_AABB);
    VectorAdd(tr->endpos, increment, start2);
    if(!tr->allsolid) {
      return qtrue;
    }
    limit--;
  } while (tr->fraction < 1.0f && limit >= 0);
  return qfalse;
}

/*
===============
BG_PositionBuildableRelativeToPlayer

Find a place to build a buildable
===============
*/
//TODO: Partial move of canbuild to this function to allow quicker updates for the red shader.
void BG_PositionBuildableRelativeToPlayer(
  const playerState_t *ps, const qboolean builder_adjacent_placement,
  vec3_t outOrigin, vec3_t outAngles, trace_t *tr) {
  vec3_t forward, targetOrigin;
  vec3_t playerNormal;
  vec3_t mins, maxs;
  float  buildDist;
  const  buildable_t buildable = ps->stats[STAT_BUILDABLE] & ~SB_VALID_TOGGLEBIT;
 
  BG_BuildableBoundingBox(buildable, mins, maxs);

  BG_GetClientNormal(ps, playerNormal);

  VectorCopy(ps->viewangles, outAngles);

  if(builder_adjacent_placement) {
    vec3_t right;

    AngleVectors(outAngles, NULL, right, NULL);
    CrossProduct(playerNormal, right, forward);
  } else {
    AngleVectors(outAngles, forward, NULL, NULL);
  }

  {
    vec3_t viewOrigin, startOrigin;
    vec3_t mins2, maxs2;
    vec3_t builderBottom;
    trace_t builder_bottom_trace;
    const float minNormal = BG_Buildable(buildable)->minNormal;
    const qboolean invertNormal = BG_Buildable(buildable)->invertNormal;
    qboolean validAngle;
    const int conditional_pass_ent_num =
      builder_adjacent_placement ? MAGIC_TRACE_HACK : ps->clientNum;
    float heightOffset = 0.0f;
    float builderDia;
    vec3_t builderMins, builderMaxs;
    const qboolean preciseBuild = 
      (ps->stats[STAT_STATE] & SS_PRECISE_BUILD) ||
      BG_Class(ps->stats[STAT_CLASS])->buildPreciseForce;
    qboolean onBuilderPlane = qfalse;

    BG_ClassBoundingBox(
      ps->stats[STAT_CLASS], builderMins, builderMaxs, NULL, NULL, NULL);
    builderDia = RadiusFromBounds(builderMins, builderMaxs);
    builderDia *= 2.0f;

    if(builder_adjacent_placement) {
      float projected_maxs, projected_mins;
  
      projected_maxs =
        fabs(maxs[0] * forward[0]) +
        fabs(maxs[1] * forward[1]) +
        fabs(maxs[2] * forward[2]);
      projected_mins =
        fabs(mins[0] * forward[0]) +
        fabs(mins[1] * forward[1]) +
        fabs(mins[2] * forward[2]);
      buildDist = max(projected_maxs, projected_mins) + builderDia + 5.0f;
    } else if(preciseBuild) {
      buildDist = BG_Class(ps->stats[STAT_CLASS])->buildDistPrecise;
    } else {
      buildDist = BG_Class(ps->stats[STAT_CLASS])->buildDist;
    }

    BG_GetClientViewOrigin(ps, viewOrigin);

    maxs2[ 0 ] = maxs2[ 1 ] = 14;
    maxs2[ 2 ] = ps->stats[ STAT_TEAM ] == TEAM_HUMANS ? 7 :maxs2[ 0 ];
    mins2[ 0 ] = mins2[ 1 ] = -maxs2[ 0 ];
    mins2[ 2 ] = 0;

    //trace the bottom of the builder
    VectorMA(viewOrigin, -builderDia, playerNormal, builderBottom);
    BG_Trace(
      &builder_bottom_trace, viewOrigin, mins2, maxs2, builderBottom,
      ps->clientNum, *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
    VectorCopy(builder_bottom_trace.endpos, builderBottom);
    VectorMA(builderBottom, 0.5f, playerNormal, builderBottom);

    if(builder_adjacent_placement) {
      VectorCopy(builderBottom, startOrigin);
    } else {
      VectorCopy(viewOrigin, startOrigin);
    }

    VectorMA( startOrigin, buildDist, forward, targetOrigin );

    {
      {//Do a small bbox trace to find the true targetOrigin.
        vec3_t targetNormal;

        BG_Trace(
          tr, startOrigin, mins2, maxs2, targetOrigin, ps->clientNum,
          *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
        if( tr->startsolid || tr->allsolid ) {
          VectorCopy( viewOrigin, outOrigin );
          tr->plane.normal[ 2 ] = 0.0f;
          tr->entityNum = ENTITYNUM_NONE;
          return;
        }
        VectorCopy( tr->endpos, targetOrigin );

        //check if tracing should be view the view or from the base of the builder
        VectorCopy(tr->plane.normal, targetNormal);
        if(
          ps->groundEntityNum != ENTITYNUM_NONE &&
          (
            builder_adjacent_placement ||
            VectorCompare(playerNormal, targetNormal))) {

          if(builder_adjacent_placement) {
            vec3_t end;
            trace_t tr2;

            VectorCopy(playerNormal, targetNormal);

            //move the target origin away from any collided surface
            VectorSubtract(startOrigin, targetOrigin, end);
            VectorNormalize(end);
            VectorMA(targetOrigin, 1.0f, end, end);
            BG_Trace(
              &tr2, startOrigin, mins2, maxs2, end, MAGIC_TRACE_HACK,
              *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
            if(!tr2.allsolid && tr2.fraction >= 1.0f) {
              VectorCopy(tr2.endpos, startOrigin);
            } else {
              VectorCopy(viewOrigin, outOrigin);
              tr->plane.normal[2] = 0.0f;
              tr->entityNum = ENTITYNUM_NONE;
              return;
            }

            //nudge the target origin back down to undo the offset
            VectorMA(targetOrigin, -0.5f, targetNormal, end);
            BG_Trace(
              &tr2, targetOrigin, mins2, maxs2, end, ps->clientNum,
              *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
            if(!tr2.startsolid && !tr2.allsolid) {
              VectorCopy(tr2.endpos, targetOrigin);
            }
          }

          if(!builder_bottom_trace.startsolid && !builder_bottom_trace.allsolid) {
            vec3_t end;
            trace_t tr2;

            //check that there is a clear trace from the builderBottom to the target
            VectorMA(targetOrigin, 0.5, targetNormal, end);
            BG_Trace(
              &tr2, builderBottom, mins2, maxs2, end, ps->clientNum,
              *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
            if(!tr2.startsolid && !tr2.allsolid && tr2.fraction >= 1.0f) {
              //undo the offset
              VectorMA(builderBottom, -0.5f, playerNormal, builderBottom);
              //use the builderBottom
              VectorCopy(builderBottom, startOrigin);
              onBuilderPlane = qtrue;
            }
          }
        }
      }

      {//Center height and find the smallest axis.  This assumes that all x and y axis are the same magnitude.
        heightOffset = -(maxs[2] + mins[2]) / 2.0f;

        maxs[2] += heightOffset;
        mins[2] += heightOffset;
      }

      if(
        (minNormal > 0.0f && !invertNormal) ||
        preciseBuild ||
        onBuilderPlane ||
        builder_adjacent_placement) {
        //Raise origins by 1+maxs[2].
        VectorMA(startOrigin, maxs[2] + 1.0f, playerNormal, startOrigin);
        VectorMA(targetOrigin, maxs[2] + 1.0f, playerNormal, targetOrigin);
      }

      if(builder_adjacent_placement) {
        vec3_t  temp;
        trace_t tr2;

        //swap the startOrigin with the targetOrigin to position back against the builder
        VectorCopy(startOrigin, temp);
        VectorCopy(targetOrigin, startOrigin);
        VectorCopy(temp, targetOrigin);

        //position the target origin to collide back against the builder.
        BG_Trace(
          &tr2, startOrigin, mins2, maxs2, targetOrigin, MAGIC_TRACE_HACK,
          *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
        if(!tr2.startsolid && !tr2.allsolid) {
          VectorCopy(tr2.endpos, targetOrigin);

          //allow for a gap between the builder and the builder
          VectorSubtract(startOrigin, targetOrigin, temp);
          VectorNormalize(temp);
          VectorMA(targetOrigin, 5.0f, temp, temp);
          BG_Trace(
            &tr2, targetOrigin, mins2, maxs2, temp, ps->clientNum,
            *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
          if(!tr2.startsolid && !tr2.allsolid && tr2.fraction >= 1.0f) {
            VectorCopy(tr2.endpos, targetOrigin);
          } else {
            VectorCopy( viewOrigin, outOrigin );
            tr->plane.normal[ 2 ] = 0.0f;
            tr->entityNum = ENTITYNUM_NONE;
            return;
          }
        } else {
          VectorCopy( viewOrigin, outOrigin );
          tr->plane.normal[ 2 ] = 0.0f;
          tr->entityNum = ENTITYNUM_NONE;
          return;
        }
      }

      {//Do traces from behind the player to the target to find a valid spot.
        trace_t tr2;

        if(
          !BG_FindValidSpot(
            tr, startOrigin, mins, maxs, targetOrigin,
            conditional_pass_ent_num, *Temp_Clip_Mask(MASK_PLAYERSOLID, 0),
            5.0f, 5)) {
          VectorCopy(viewOrigin, outOrigin);
          tr->plane.normal[2] = 0.0f;
          tr->entityNum = ENTITYNUM_NONE;
          return;
        }

        //Check that the spot is not on the opposite side of a thin wall
        BG_Trace(
          &tr2, startOrigin, NULL, NULL, tr->endpos, ps->clientNum,
          *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
        if(tr2.fraction < 1.0f || tr2.startsolid || tr2.allsolid) {
          VectorCopy(viewOrigin, outOrigin);
          tr->plane.normal[2] = 0.0f;
          tr->entityNum = ENTITYNUM_NONE;
          return;
        }
      }
    }

    validAngle =
      tr->plane.normal[2] >=
        minNormal || (invertNormal && tr->plane.normal[2] <= -minNormal);

    //Down trace if precision building, builder adjacent placement, no hit, or surface is too steep.
    if(
      preciseBuild ||
      builder_adjacent_placement ||
      tr->fraction >= 1.0f ||
      !validAngle ) {//TODO: These should be utility functions like "if(traceHit(&tr))"
      if(tr->fraction < 1.0f) {
        //Bring endpos away from surface it has hit.
        VectorAdd(tr->endpos, tr->plane.normal, tr->endpos);
      }

      {
        vec3_t startOrigin;

        VectorMA(tr->endpos, -buildDist / 2.0f, playerNormal, targetOrigin);

        VectorCopy(tr->endpos, startOrigin);

        BG_Trace(
          tr, startOrigin, mins, maxs, targetOrigin, ps->clientNum,
          *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
      }
    }

    if(!builder_adjacent_placement) {
      trace_t tr2;

      //check if this position would collide with the builder
      BG_Trace(
        &tr2, tr->endpos, mins, maxs, tr->endpos, MAGIC_TRACE_HACK,
        *Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);

      if((tr2.startsolid || tr2.allsolid) && tr2.entityNum == ps->clientNum) {
        //attempt to position buildable adjacent to the builder
        BG_PositionBuildableRelativeToPlayer(
          ps, qtrue, outOrigin, outAngles, tr);
        return;
      }
    }

    tr->endpos[2] += heightOffset;
  }
  VectorCopy(tr->endpos, outOrigin);
}
