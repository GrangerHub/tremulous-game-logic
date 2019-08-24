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

// cg_predict.c -- this file generates cg.predictedPlayerState by either
// interpolating between snapshots from the server or locally predicting
// ahead the client's movement.
// It also handles local physics interaction, like fragments bouncing off walls


#include "cg_local.h"

static  pmove_t   cg_pmove;

static  int     cg_numSolidEntities;
static  centity_t *cg_solidEntities[MAX_ENTITIES_IN_SNAPSHOT];
static  int     cg_numTriggerEntities;
static  centity_t *cg_triggerEntities[MAX_ENTITIES_IN_SNAPSHOT];

/*
====================
CG_BuildSolidList

When a new cg.snap has been set, this function builds a sublist
of the entities that are actually solid, to make for more
efficient collision detection
====================
*/
void CG_BuildSolidList( void )
{
  int           i;
  centity_t     *cent;
  snapshot_t    *snap;
  entityState_t *ent;

  cg_numSolidEntities = 0;
  cg_numTriggerEntities = 0;

  for(i = 0; i < MAX_GENTITIES; i++) {
    const int *contents_pointer = (int *)(&cg_entities[i].currentState.origin[1]);

    cg_entities[i].linked = qfalse;
    cg_entities[i].is_in_solid_list = qfalse;
    cg_entities[i].contents = *contents_pointer;
  }

  if( cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport )
    snap = cg.nextSnap;
  else
    snap = cg.snap;

  cg.predictedPlayerEntity.linked = qtrue;
  cg.predictedPlayerEntity.is_in_solid_list = qtrue;
  cg.predictedPlayerEntity.contents = snap->ps.misc[MISC_CONTENTS];
  cg_entities[cg.clientNum].linked = qtrue;
  cg_entities[cg.clientNum].is_in_solid_list = qtrue;
  cg_entities[cg.clientNum].contents = snap->ps.misc[MISC_CONTENTS];

  for( i = 0; i < snap->numEntities; i++ )
  {
    cent = &cg_entities[ snap->entities[ i ].number ];
    ent = &cent->currentState;

    if( ent->eType == ET_ITEM || ent->eType == ET_PUSH_TRIGGER || ent->eType == ET_TELEPORT_TRIGGER )
    {
      cg_triggerEntities[ cg_numTriggerEntities ] = cent;
      cg_numTriggerEntities++;
      continue;
    }

    if( cent->nextState.solid && ent->eType != ET_MISSILE )
    {
      cg_solidEntities[ cg_numSolidEntities ] = cent;
      cg_numSolidEntities++;
      cent->linked = qtrue;
      cent->is_in_solid_list = qtrue;
      continue;
    }
  }
}

/*
====================
CG_Link_Solid_Entity
====================
*/
void CG_Link_Solid_Entity(int ent_num) {
  centity_t *cent;

  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  if(ent_num == cg.clientNum) {
    cent = &cg.predictedPlayerEntity;

    if(cent->is_in_solid_list) {
      cent->linked = qtrue;
    }
  }

  cent = &cg_entities[ent_num];

  if(cent->is_in_solid_list) {
    cent->linked = qtrue;
  }
}

/*
====================
CG_Unlink_Solid_Entity
====================
*/
void CG_Unlink_Solid_Entity(int ent_num) {
  centity_t *cent;

  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  if(ent_num == cg.clientNum) {
    cent = &cg.predictedPlayerEntity;
  
    if(!cent->is_in_solid_list || !cent->linked) {
      return;
    }

    cent->linked = qfalse;
  }

  cent = &cg_entities[ent_num];

  if(!cent->is_in_solid_list || !cent->linked) {
    return;
  }

  cent->linked = qfalse;
}

/*
====================
CG_Link_Buildables

====================
*/
void CG_Link_Buildables(void) {
  int i;

  for(i = 0; i < cg_numSolidEntities; i++) {
    centity_t *ent = cg_solidEntities[i];

    if(ent->currentState.eType != ET_BUILDABLE) {
      continue;
    }

    CG_Link_Solid_Entity(ent->currentState.number);
  }
}

/*
====================
CG_Unlink_Buildables

====================
*/
void CG_Unlink_Buildables(void) {
  int i;

  for(i = 0; i < cg_numSolidEntities; i++) {
    centity_t *ent = cg_solidEntities[i];

    if(ent->currentState.eType != ET_BUILDABLE) {
      continue;
    }

    CG_Unlink_Solid_Entity(ent->currentState.number);
  }
}

/*
====================
CG_Link_Marked_Buildables

====================
*/
void CG_Link_Marked_Buildables(void) {
  int i;

  for(i = 0; i < cg_numSolidEntities; i++) {
    centity_t *ent = cg_solidEntities[i];

    if(ent->currentState.eType != ET_BUILDABLE) {
      continue;
    }

    if(!(ent->currentState.eFlags & EF_B_MARKED)) {
      continue;
    }

    CG_Link_Solid_Entity(ent->currentState.number);
  }
}

/*
====================
CG_Unlink_Marked_Buildables

====================
*/
void CG_Unlink_Marked_Buildables(void) {
  int i;

  for(i = 0; i < cg_numSolidEntities; i++) {
    centity_t *ent = cg_solidEntities[i];

    if(ent->currentState.eType != ET_BUILDABLE) {
      continue;
    }

    if(!(ent->currentState.eFlags & EF_B_MARKED)) {
      continue;
    }

    CG_Unlink_Solid_Entity(ent->currentState.number);
  }
}

/*
====================
CG_Link_Players

====================
*/
void CG_Link_Players(void) {
  int i;

  for(i = 0; i < cg_numSolidEntities; i++) {
    centity_t *ent = cg_solidEntities[i];

    if(ent->currentState.number >= MAX_CLIENTS) {
      continue;
    }

    CG_Link_Solid_Entity(ent->currentState.number);
  }
}

/*
====================
CG_Unlink_Players

====================
*/
void CG_Unlink_Players(void) {
  int i;

  for(i = 0; i < cg_numSolidEntities; i++) {
    centity_t *ent = cg_solidEntities[i];

    if(ent->currentState.number >= MAX_CLIENTS) {
      continue;
    }

    CG_Unlink_Solid_Entity(ent->currentState.number);
  }
}

/*
====================
CG_Update_Collision_Data_For_Entity

====================
*/
void CG_Update_Collision_Data_For_Entity(
  int ent_num, int time, const vec3_t origin, const playerState_t *ps) {
  centity_t     *cent;
  entityState_t *ent;

  Com_Assert(ent_num >= 0 && ent_num < ENTITYNUM_MAX_NORMAL);

  if(ent_num == cg.clientNum) {
    if(ps == NULL) {
      return;
    }
    cent = &cg.predictedPlayerEntity;
  } else {
    cent = &cg_entities[ent_num];
  }

  if(!cent->is_in_solid_list) {
    return;
  }

  ent = &cent->currentState;

  if(ent->eFlags & EF_BMODEL) {
    vec3_t previous_origin, offset, centered_maxs;
    int    n;
    int    previous_origin_time = *((int *)(&ent->origin[2]));

    VectorCopy(cent->lerpAngles, cent->collision_angles);

    centered_maxs[0] = (ent->solid & 255);
    centered_maxs[1] = ((ent->solid >> 8) & 255);
    centered_maxs[2] = ((ent->solid >> 16) & 255);

    //for area checking assume the bmodel is huge in a given axis if the encoding is maxed.
    for(n = 0; n < 3; n++) {
      if(centered_maxs[n] >= 255) {
        centered_maxs[n] *= 100;
      }
    }

    BG_EvaluateTrajectory( &cent->currentState.pos, time, cent->collision_origin );
    BG_EvaluateTrajectory(&cent->currentState.pos, previous_origin_time, previous_origin);
    VectorSubtract(cent->collision_origin, previous_origin, offset);
    VectorAdd(cent->currentState.angles, offset, offset);

    VectorAdd(offset, centered_maxs, cent->absmax);
    VectorSubtract(offset, centered_maxs, cent->absmin);
    VectorSubtract(cent->absmax, cent->collision_origin, cent->maxs);
    VectorSubtract(cent->absmin, cent->collision_origin, cent->mins);
  } else {
    int x, zd, zu;
    int pusher_num = CG_Get_Pusher_Num(cent->currentState.number);

    VectorCopy(vec3_origin, cent->collision_angles);

    // encoded bbox
    x = ( ent->solid & 255 );
    zd = ( ( ent->solid >> 8 ) & 255 );
    zu = ( ( ent->solid >> 16 ) & 255 ) - 32;

    cent->mins[ 0 ] = cent->mins[ 1 ] = -x;
    cent->maxs[ 0 ] = cent->maxs[ 1 ] = x;
    cent->mins[ 2 ] = -zd;
    cent->maxs[ 2 ] = zu;

    if(ent_num == cg.clientNum) {
      if( ps->pm_type == PM_DEAD ) {
        BG_ClassBoundingBox(
          (ent->misc >> 8) & 0xFF, NULL, NULL, NULL, cent->mins, cent->maxs);
      } else if(ps->pm_flags & PMF_DUCKED) {
        BG_ClassBoundingBox(
          (ent->misc >> 8) & 0xFF, cent->mins, NULL, cent->maxs, NULL, NULL);
      } else {
        BG_ClassBoundingBox(
          (ent->misc >> 8) & 0xFF, cent->mins, cent->maxs, NULL, NULL, NULL);
      }
    }

    if(
      pusher_num != ENTITYNUM_NONE &&
      cg_entities[pusher_num].currentState.eType == ET_MOVER ){
      //this entity is being moved in the same mover stack as the local player
      BG_EvaluateTrajectory(
        &cent->currentState.pos, time, cent->collision_origin);
      CG_AdjustPositionForMover(
                    cent->collision_origin,
                    CG_Get_Pusher_Num(cent->currentState.number),
                    cg.snap->serverTime, time,
                    cent->collision_origin);
    } else {
      Com_Assert(origin);
      VectorCopy(origin, cent->collision_origin);
    }

    VectorAdd (cent->collision_origin, cent->mins, cent->absmin);	
    VectorAdd (cent->collision_origin, cent->maxs, cent->absmax);

    if(!(ent->eFlags & EF_BMODEL)) {
      // because movement is clipped an epsilon away from an actual edge,
    	// we must fully check even when bounding boxes don't quite touch
      cent->absmin[0] -= 1;
      cent->absmin[1] -= 1;
      cent->absmin[2] -= 1;
      cent->absmax[0] += 1;
      cent->absmax[1] += 1;
      cent->absmax[2] += 1;
    }
  }
}


/*
====================
CG_Area_Entities

====================
*/
int CG_Area_Entities(
  const vec3_t mins, const vec3_t maxs, const content_mask_t *content_mask,
  int *entityList, int maxcount) {
  int           i;
  int           count = 0;
  entityState_t *ent;
  centity_t     *cent;

  for(i = 0; i < (cg_numSolidEntities + 1); i++) {
    int ent_num;

    if( i < cg_numSolidEntities ) {
      cent = cg_solidEntities[ i ];
      ent_num = cent->currentState.number;
    } else {
      cent = &cg.predictedPlayerEntity;
      ent_num = cg.clientNum;
    }

    ent = &cent->currentState;

    if(content_mask) {
      if(cent->contents & content_mask->exclude) {
        continue;
      }

      if(!(cent->contents & content_mask->include)) {
        continue;
      }
    }

    if(
      !Com_BBOX_Intersects_Area(
        cent->absmin, cent->absmax, mins, maxs)) {
      continue;
    }

		if ( count == maxcount ) {
      Com_Printf ("CG_Area_Entities: MAXCOUNT\n");
      return count;
    }

    entityList[count] = ent_num;
    count++;
	}

  return count;
}

/*
====================
CG_Clip_To_Entity

====================
*/
void CG_Clip_To_Entity(
  trace_t *trace, const vec3_t start, vec3_t mins, vec3_t maxs,
  const vec3_t end, int entityNum, const content_mask_t content_mask,
  traceType_t collisionType ) {
  centity_t	    *cent;
  entityState_t *ent;
  clipHandle_t	cmodel;
  vec3_t        move_mins, move_maxs;
  int           k;

	Com_Memset(trace, 0, sizeof(trace_t));

  if(!mins) {
    mins = vec3_origin;
  }

  if(!maxs) {
    maxs = vec3_origin;
  }

  if(entityNum == ENTITYNUM_WORLD) {
    switch(collisionType) {
      case TT_AABB:
        trap_CM_BoxTrace(trace, start, end, mins, maxs, 0, content_mask.include);
        break;

      case TT_CAPSULE:
        trap_CM_CapsuleTrace(
          trace, start, end, mins, maxs, 0, content_mask.include);
        break;

      case TT_BISPHERE:
        trap_CM_BiSphereTrace(trace, start, end, mins[ 0 ], maxs[ 0 ], 0, content_mask.include);
      case TT_NONE:
      case TT_NUM_TRACE_TYPES:
        Com_Assert(qfalse && "CG_Clip_To_Entity: trace type not supported");
        break;
    }
		trace->entityNum = trace->fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
		return;
	}

  if(entityNum == cg.clientNum) {
    cent = &cg.predictedPlayerEntity;
  } else {
    cent = &cg_entities[entityNum];
  }

  if(!cent->is_in_solid_list || !cent->linked) {
    trace->startsolid = qfalse;
    trace->allsolid = qfalse;
    trace->fraction = 1.0f;
    trace->lateralFraction = 1.0f;
    trace->entityNum = ENTITYNUM_NONE;
    return;
  }

  ent = &cent->currentState;

  if(!(cent->contents & content_mask.include)) {
    return;
  }

  if(cent->contents & content_mask.exclude) {
    return;
  }

  for ( k=0 ; k<3 ; k++ ) {
    if ( end[k] > start[k] ) {
      move_mins[k] = start[k] + mins[k] - 1;
      move_maxs[k] = end[k] + maxs[k] + 1;
    } else {
      move_mins[k] = end[k] + mins[k] - 1;
      move_maxs[k] = start[k] + maxs[k] + 1;
    }
  }

  if(
    !Com_BBOX_Intersects_Area(
      cent->absmin, cent->absmax, move_mins, move_maxs)) {
    return;
  }

  if(ent->eFlags & EF_BMODEL) {
    cmodel = trap_CM_InlineModel( ent->modelindex );
  } else {
    cmodel = trap_CM_TempBoxModel( cent->mins, cent->maxs );
  }

  switch(collisionType) {
    case TT_AABB:
      trap_CM_TransformedBoxTrace(
        trace, start, end, mins, maxs, cmodel,  content_mask.include,
        cent->collision_origin, cent->collision_angles);
      break;

    case TT_CAPSULE:
      trap_CM_TransformedCapsuleTrace(
        trace, start, end, mins, maxs, cmodel,  content_mask.include,
        cent->collision_origin, cent->collision_angles);
      break;

    case TT_BISPHERE:
      trap_CM_TransformedBiSphereTrace(
        trace, start, end, mins[ 0 ], maxs[ 0 ], cmodel,
        content_mask.include, cent->collision_origin);
      break;

    default:
      Com_Assert(qfalse && "CG_Clip_To_Entity: trace type not supported");
  }
}

/*
====================
CG_Clip_To_Test_Area

====================
*/
void CG_Clip_To_Test_Area(
  trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs,
  const vec3_t end, const vec3_t test_mins, const vec3_t test_maxs,
  const vec3_t test_origin, int test_contents, const content_mask_t content_mask,
  traceType_t collisionType) {
  const vec3_t angles = {0.0f, 0.0f, 0.0f};
  vec3_t       test_abs_mins, test_abs_maxs, boxmaxs, boxmins;
  clipHandle_t clipHandle;
  int          i;

  if(!mins) {
    mins = vec3_origin;
  }
  if(!maxs) {
    maxs = vec3_origin;
  }

  Com_Memset(results, 0, sizeof(trace_t));
  results->entityNum = ENTITYNUM_NONE;
  results->fraction = 1.0;
  VectorCopy(end, results->endpos);

  // if it doesn't have any brushes of a type we
  // are looking for, ignore it
  if(!(content_mask.include & test_contents)) {
    return;
  }

  // ignore entities that have brushes of a type that we want to exclude
  if(content_mask.exclude & test_contents) {
    return;
  }

  // create the bounding box of the entire move
  // we can limit it to the part of the move not
  // already clipped off by the world, which can be
  // a significant savings for line of sight and shot traces
  for(i=0 ; i<3 ; i++) {
    if ( end[i] > start[i] ) {
      boxmins[i] = start[i] + mins[i] - 1;
      boxmaxs[i] = end[i] + maxs[i] + 1;
    } else {
      boxmins[i] = end[i] + mins[i] - 1;
      boxmaxs[i] = start[i] + maxs[i] + 1;
    }
  }

  VectorAdd(test_origin, test_mins, test_abs_mins);
  VectorAdd(test_origin, test_maxs, test_abs_maxs);

  if(
    !Com_BBOX_Intersects_Area(
      test_abs_mins, test_abs_maxs, boxmins, boxmaxs)) {
    return;
	}

  clipHandle = trap_CM_TempBoxModel(test_mins, test_abs_maxs);

  switch(collisionType) {
    case TT_AABB:
      trap_CM_TransformedBoxTrace(
        results, start, end, mins, maxs, clipHandle,  content_mask.include,
        test_origin, angles);
      break;

    case TT_CAPSULE:
      trap_CM_TransformedCapsuleTrace(
        results, start, end, mins, maxs, clipHandle,  content_mask.include,
        test_origin, angles);
      break;

    case TT_BISPHERE:
      trap_CM_TransformedBiSphereTrace(
        results, start, end, mins[ 0 ], maxs[ 0 ], clipHandle,
        content_mask.include, test_origin);
      break;

    default:
      Com_Assert(qfalse && "CG_Clip_To_Test_Area: trace type not supported");
  }
}

/*
====================
CG_ClipMoveToEntities

====================
*/
static void CG_ClipMoveToEntities(
  const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
  int skipNumber, content_mask_t content_mask, trace_t *tr,
  traceType_t collisionType) {
  int           i, j, k;
  trace_t       trace;
  entityState_t *ent;
  clipHandle_t  cmodel;
  vec3_t        move_mins, move_maxs;
  centity_t     *cent;

  //SUPAR HACK
  //this causes a trace to collide with the local player
  if(skipNumber == MAGIC_TRACE_HACK) {
    j = cg_numSolidEntities + 1;
  } else {
    j = cg_numSolidEntities;
  }

  if(!mins) {
    mins = vec3_origin;
  }

  if(!maxs) {
    maxs = vec3_origin;
  }

  // create the bounding box of the entire move
  // we can limit it to the part of the move not
  // already clipped off by the world, which can be
  // a significant savings for line of sight and shot traces
  for ( k=0 ; k<3 ; k++ ) {
    if ( tr->endpos[k] > start[k] ) {
      move_mins[k] = start[k] + mins[k] - 1;
      move_maxs[k] = tr->endpos[k] + maxs[k] + 1;
    } else {
      move_mins[k] = tr->endpos[k] + mins[k] - 1;
      move_maxs[k] = start[k] + maxs[k] + 1;
    }
  }

  for(i = 0; i < j; i++) {
    int ent_num;

    if(i < cg_numSolidEntities) {
      cent = cg_solidEntities[i];
      ent_num = cent->currentState.number;
    } else {
      cent = &cg.predictedPlayerEntity;
      ent_num = cg.clientNum;
    }

    if(!cent->linked) {
      continue;
    }

    ent = &cent->currentState;
    if(ent->number == skipNumber) {
      continue;
    }

    if(!(cent->contents & content_mask.include)) {
      continue;
    }

    if(cent->contents & content_mask.exclude) {
      continue;
    }

    CG_Update_Collision_Data_For_Entity(
      ent_num, cg.physicsTime, cent->lerpOrigin,
      &cg.predictedPlayerState);

    if(
      !Com_BBOX_Intersects_Area(
        cent->absmin, cent->absmax, move_mins, move_maxs)) {
      continue;
    }

    if(ent->eFlags & EF_BMODEL) {
      cmodel = trap_CM_InlineModel( ent->modelindex );
    } else {
      cmodel = trap_CM_TempBoxModel( cent->mins, cent->maxs );
    }

    switch(collisionType) {
      case TT_AABB:
        trap_CM_TransformedBoxTrace(
          &trace, start, end, mins, maxs, cmodel,  content_mask.include,
          cent->collision_origin, cent->collision_angles);
        break;

      case TT_CAPSULE:
        trap_CM_TransformedCapsuleTrace(
          &trace, start, end, mins, maxs, cmodel,  content_mask.include,
          cent->collision_origin, cent->collision_angles);
        break;

      case TT_BISPHERE:
        trap_CM_TransformedBiSphereTrace(
          &trace, start, end, mins[ 0 ], maxs[ 0 ], cmodel,
          content_mask.include, cent->collision_origin);
        break;

      default:
        Com_Assert(qfalse && "CG_ClipMoveToEntities: trace type not supported");
    }

    if ( trace.allsolid ) {
      tr->allsolid = qtrue;
      trace.entityNum = ent->number;
    } else if ( trace.startsolid ) {
      tr->startsolid = qtrue;
      trace.entityNum = ent->number;
    }

    if(trace.fraction < tr->fraction) {
      qboolean	oldStart;

      // make sure we keep a startsolid from a previous trace
      oldStart = tr->startsolid;

      trace.entityNum = ent->number;
      trace.contents |= cent->contents;
      if(tr->lateralFraction < trace.lateralFraction) {
        float oldLateralFraction = tr->lateralFraction;
        *tr = trace;
        tr->lateralFraction = oldLateralFraction;
      } else {
        *tr = trace;
      }
      tr->startsolid |= oldStart;

      //adjust the overall move bbox
      for( k=0 ; k<3 ; k++ ) {
        if( tr->endpos[k] > start[k] ) {
          move_mins[k] = start[k] + mins[k] - 1;
          move_maxs[k] = tr->endpos[k] + maxs[k] + 1;
        } else {
          move_mins[k] = tr->endpos[k] + mins[k] - 1;
          move_maxs[k] = start[k] + maxs[k] + 1;
        }
      }
    } else if( trace.startsolid ) {
      tr->startsolid = qtrue;
    }

    if(tr->allsolid) {
      return;
    }
  }
}

/*
================
CG_Trace
================
*/
void  CG_Trace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
                int skipNumber, content_mask_t content_mask )
{
  trace_t t;

  trap_CM_BoxTrace( &t, start, end, mins, maxs, 0, content_mask.include );
  t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
  if ( t.fraction == 0 ) {
    *result = t;
    return;		// blocked immediately by the world
  }
  // check all other solid models
  CG_ClipMoveToEntities( start, mins, maxs, end, skipNumber, content_mask, &t, TT_AABB );

  *result = t;
}

/*
================
CG_CapTrace
================
*/
void  CG_CapTrace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
                   int skipNumber, content_mask_t content_mask )
{
  trace_t t;

  trap_CM_CapsuleTrace( &t, start, end, mins, maxs, 0, content_mask.include );
  t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
  if ( t.fraction == 0 ) {
    *result = t;
    return;		// blocked immediately by the world
  }
  // check all other solid models
  CG_ClipMoveToEntities( start, mins, maxs, end, skipNumber, content_mask, &t, TT_CAPSULE );

  *result = t;
}

/*
================
CG_BiSphereTrace
================
*/
void CG_BiSphereTrace( trace_t *result, const vec3_t start, const vec3_t end,
    const float startRadius, const float endRadius, int skipNumber, content_mask_t content_mask )
{
  trace_t t;
  vec3_t  mins, maxs;

  mins[ 0 ] = startRadius;
  maxs[ 0 ] = endRadius;

  trap_CM_BiSphereTrace( &t, start, end, startRadius, endRadius, 0, content_mask.include );
  t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
  // check all other solid models
  CG_ClipMoveToEntities( start, mins, maxs, end, skipNumber, content_mask, &t, TT_BISPHERE );

  *result = t;
}

/*
================
CG_Trace_Wrapper
================
*/
void CG_Trace_Wrapper(
	trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs,
	const vec3_t end, int passEntityNum, const content_mask_t content_mask,
	traceType_t type) {
  trace_t t;

  switch(type) {
    case TT_AABB:
      CG_Trace(results, start, mins, maxs, end, passEntityNum, content_mask);
      break;

    case TT_CAPSULE:
      CG_CapTrace(results, start, mins, maxs, end, passEntityNum, content_mask);
      break;

    case TT_BISPHERE:
      trap_CM_BiSphereTrace(
        &t, start, end, mins[ 0 ], maxs[ 0 ], 0, content_mask.include);
      t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
      // check all other solid models
      CG_ClipMoveToEntities(
        start, mins, maxs, end, passEntityNum, content_mask, &t, TT_BISPHERE);

      *results = t;
      break;

    case TT_NONE:
    case TT_NUM_TRACE_TYPES:
      Com_Assert(qfalse && "CG_Trace_Wrapper: trace type not supported");
      break;
  }
}

/*
================
CG_PointContents
================
*/
int   CG_PointContents( const vec3_t point, int passEntityNum )
{
  int           i, num;
  int           touch[MAX_GENTITIES];
  entityState_t *ent;
  centity_t     *cent;
  clipHandle_t  cmodel;
  int           contents;

  contents = trap_CM_PointContents (point, 0);

  num = CG_Area_Entities( point, point, NULL, touch, MAX_GENTITIES );

  for( i = 0; i < num; i++ )
  {
    trace_t trace;

    if(touch[i] == passEntityNum) {
      continue;
    }
    cent = &cg_entities[touch[i]];

    ent = &cent->currentState;

    if(!(ent->eFlags & EF_BMODEL)) {
      trace_t trace;

      CG_Clip_To_Entity(
        &trace, point, NULL, NULL, point, touch[i], *Temp_Clip_Mask(-1, 0),
        TT_AABB);

      if(trace.fraction < 1.0f || trace.startsolid || trace.allsolid) {
        contents |= cent->contents;
      }
      continue;
    }

    cmodel = trap_CM_InlineModel( ent->modelindex );

    if(!cmodel) {
      continue;
    }

    contents |=
      trap_CM_TransformedPointContents(
        point, cmodel, cent->collision_origin, cent->collision_angles);

    if((cent->contents & CONTENTS_DOOR)) {
      CG_Clip_To_Entity(
        &trace, point, NULL, NULL, point, touch[i],
        *Temp_Clip_Mask(-1, 0), TT_AABB);

      if(trace.fraction < 1 || trace.allsolid || trace.startsolid) {
        contents |= (cent->contents & CONTENTS_DOOR);
      }
    }
  }

  return contents;
}

/*
========================
CG_Reactor_Is_Up
========================
*/
static qboolean CG_Reactor_Is_Up(void) {
  int i;

  for(i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
    centity_t     *cent = &cg_entities[i];
    entityState_t *ent = &cent->currentState;

    if(!cent->valid) {
      continue;
    }

    if(ent->eType != ET_BUILDABLE) {
      continue;
    }

    if(ent->modelindex != BA_H_REACTOR) {
      continue;
    }

    if(ent->eFlags & EF_B_SPAWNED) {
      return qtrue;
    }
  }

  return qfalse;
}


/*
========================
CG_InterpolatePlayerState

Generates cg.predictedPlayerState by interpolating between
cg.snap->player_state and cg.nextFrame->player_state
========================
*/
static void CG_InterpolatePlayerState( qboolean grabAngles )
{
  float         f;
  int           i;
  playerState_t *out;
  snapshot_t    *prev, *next;

  out = &cg.predictedPlayerState;
  prev = cg.snap;
  next = cg.nextSnap;

  *out = cg.snap->ps;

  // if we are still allowing local input, short circuit the view angles
  if( grabAngles )
  {
    usercmd_t cmd;
    int     cmdNum;

    cmdNum = trap_GetCurrentCmdNumber( );
    trap_GetUserCmd( cmdNum, &cmd );

    PM_UpdateViewAngles( out, &cmd );
  }

  // if the next frame is a teleport, we can't lerp to it
  if( cg.nextFrameTeleport )
    return;

  if( !next || next->serverTime <= prev->serverTime )
    return;

  f = (float)( cg.time - prev->serverTime ) / ( next->serverTime - prev->serverTime );

  i = next->ps.misc[MISC_BOB_CYCLE];
  if( i < prev->ps.misc[MISC_BOB_CYCLE] )
    i += 4096;   // handle wraparound

  out->misc[MISC_BOB_CYCLE] =
    prev->ps.misc[MISC_BOB_CYCLE] + f * (i - prev->ps.misc[MISC_BOB_CYCLE]);

  for( i = 0; i < 3; i++ )
  {
    out->origin[ i ] = prev->ps.origin[ i ] + f * ( next->ps.origin[ i ] - prev->ps.origin[ i ] );

    if( !grabAngles )
      out->viewangles[ i ] = LerpAngle( prev->ps.viewangles[ i ], next->ps.viewangles[ i ], f );

    out->velocity[ i ] = prev->ps.velocity[ i ] +
      f * (next->ps.velocity[ i ] - prev->ps.velocity[ i ] );
  }
}


/*
=========================
CG_TouchTriggerPrediction

Predict push triggers and items
=========================
*/
static void CG_TouchTriggerPrediction( void )
{
  int           i;
  trace_t       trace;
  entityState_t *ent;
  clipHandle_t  cmodel;
  centity_t     *cent;
  qboolean      spectator;

  // dead clients don't activate triggers
  if( cg.predictedPlayerState.misc[ MISC_HEALTH ] <= 0 )
    return;

  spectator = ( cg.predictedPlayerState.pm_type == PM_SPECTATOR );

  if( cg.predictedPlayerState.pm_type != PM_NORMAL && !spectator )
    return;

  for( i = 0; i < cg_numTriggerEntities; i++ )
  {
    cent = cg_triggerEntities[ i ];
    ent = &cent->currentState;

    if(!(ent->eFlags & EF_BMODEL))
      continue;

    cmodel = trap_CM_InlineModel( ent->modelindex );
    if( !cmodel )
      continue;

    trap_CM_BoxTrace( &trace, cg.predictedPlayerState.origin, cg.predictedPlayerState.origin,
                      cg_pmove.mins, cg_pmove.maxs, cmodel, -1 );

    if( !trace.startsolid )
      continue;

    if( ent->eType == ET_TELEPORT_TRIGGER )
      cg.hyperspace = qtrue;
  }
}

static int CG_IsUnacceptableError( playerState_t *ps, playerState_t *pps )
{
  vec3_t delta;
  int i;

  if( pps->pm_type != ps->pm_type ||
    pps->pm_flags != ps->pm_flags ||
    pps->pm_time != ps->pm_time )
  {
    return 1;
  }

  VectorSubtract( pps->origin, ps->origin, delta );
  if( VectorLengthSquared( delta ) > 0.1f * 0.1f )
  {
    if( cg_showmiss.integer )
    {
      CG_Printf( "origin delta: %.2f  ", VectorLength( delta ) );
    }
    return 2;
  }

  VectorSubtract( pps->velocity, ps->velocity, delta );
  if( VectorLengthSquared( delta ) > 0.1f * 0.1f )
  {
    if( cg_showmiss.integer )
    {
      CG_Printf( "velocity delta: %.2f  ", VectorLength( delta ) );
    }
    return 3;
  }

  if( pps->weaponTime != ps->weaponTime ||
    pps->gravity != ps->gravity ||
    pps->speed != ps->speed ||
    pps->delta_angles[ 0 ] != ps->delta_angles[ 0 ] ||
    pps->delta_angles[ 1 ] != ps->delta_angles[ 1 ] ||
    pps->delta_angles[ 2 ] != ps->delta_angles[ 2 ] || 
    pps->groundEntityNum != ps->groundEntityNum )
  {
    return 4;
  }

  if( pps->legsTimer != ps->legsTimer ||
    pps->legsAnim != ps->legsAnim ||
    pps->torsoTimer != ps->torsoTimer ||
    pps->torsoAnim != ps->torsoAnim ||
    pps->movementDir != ps->movementDir )
  {
    return 5;
  }

  VectorSubtract( pps->grapplePoint, ps->grapplePoint, delta );
  if( VectorLengthSquared( delta ) > 0.1f * 0.1f )
    return 6;

  if( pps->eFlags != ps->eFlags )
    return 7;

  if( pps->eventSequence != ps->eventSequence )
    return 8;

  for( i = 0; i < MAX_PS_EVENTS; i++ )
  {
    if ( pps->events[ i ] != ps->events[ i ] ||
      pps->eventParms[ i ] != ps->eventParms[ i ] )
    {
      return 9;
    }
  }

  if( pps->externalEvent != ps->externalEvent ||
    pps->externalEventParm != ps->externalEventParm ||
    pps->externalEventTime != ps->externalEventTime )
  {
    return 10;
  }

  if( pps->clientNum != ps->clientNum ||
    pps->weapon != ps->weapon ||
    pps->weaponstate != ps->weaponstate )
  {
    return 11;
  }

  if( fabs( AngleDelta( ps->viewangles[ 0 ], pps->viewangles[ 0 ] ) ) > 1.0f ||
    fabs( AngleDelta( ps->viewangles[ 1 ], pps->viewangles[ 1 ] ) ) > 1.0f ||
    fabs( AngleDelta( ps->viewangles[ 2 ], pps->viewangles[ 2 ] ) ) > 1.0f )
  {
    return 12;
  }

  if( pps->viewheight != ps->viewheight )
    return 13;

  if( pps->damageEvent != ps->damageEvent ||
    pps->damageYaw != ps->damageYaw ||
    pps->damagePitch != ps->damagePitch ||
    pps->damageCount != ps->damageCount )
  {
    return 14;
  }

  for( i = 0; i < MAX_STATS; i++ )
  {
    if( pps->stats[ i ] != ps->stats[ i ] ) {
      return 15;
    }
  }

  for( i = 0; i < MAX_PERSISTANT; i++ )
  {
    if( pps->persistant[ i ] != ps->persistant[ i ] )
      return 16;
  }

  for( i = 0; i < MAX_MISC; i++ )
  {
    if( pps->misc[ i ] != ps->misc[ i ] )
      return 17;
  }

  if( pps->otherEntityNum != ps->otherEntityNum )
  {
    return 18;
  }

  if( pps->generic1 != ps->generic1 ||
    pps->loopSound != ps->loopSound )
  {
    return 19;
  }

  return 0;
}


/*
=================
CG_PredictPlayerState

Generates cg.predictedPlayerState for the current cg.time
cg.predictedPlayerState is guaranteed to be valid after exiting.

For demo playback, this will be an interpolation between two valid
playerState_t.

For normal gameplay, it will be the result of predicted usercmd_t on
top of the most recent playerState_t received from the server.

Each new snapshot will usually have one or more new usercmd over the last,
but we simulate all unacknowledged commands each time, not just the new ones.
This means that on an internet connection, quite a few pmoves may be issued
each frame.

OPTIMIZE: don't re-simulate unless the newly arrived snapshot playerState_t
differs from the predicted one.  Would require saving all intermediate
playerState_t during prediction.

We detect prediction errors and allow them to be decayed off over several frames
to ease the jerk.
=================
*/
void CG_PredictPlayerState( void )
{
  int           cmdNum, current, i;
  playerState_t oldPlayerState;
  usercmd_t     oldestCmd;
  usercmd_t     latestCmd;
  int           stateIndex = 0, predictCmd = 0;
  float         delta_yaw;

  cg.hyperspace = qfalse; // will be set if touching a trigger_teleport

  // if this is the first frame we must guarantee
  // predictedPlayerState is valid even if there is some
  // other error condition
  if( !cg.validPPS )
  {
    cg.validPPS = qtrue;
    cg.predictedPlayerState = cg.snap->ps;
  }


  // demo playback just copies the moves
  if( cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW) )
  {
    CG_InterpolatePlayerState( qfalse );
    return;
  }

  // non-predicting local movement will grab the latest angles
  if( cg_nopredict.integer || cg_synchronousClients.integer )
  {
    CG_InterpolatePlayerState( qtrue );
    return;
  }

  // prepare for pmove
  cg_pmove.ps = &cg.predictedPlayerState;
  cg_pmove.pmext = &cg.pmext;
  cg_pmove.debugLevel = cg_debugMove.integer;
  cg_pmove.swapAttacks = cg_swapAttacks.integer;
  cg_pmove.wallJumperMinFactor = cg_wallJumperMinFactor.value;
  cg_pmove.marauderMinJumpFactor = cg_marauderMinJumpFactor.value;
  cg_pmove.pm_reactor = CG_Reactor_Is_Up;

  if( cg_pmove.ps->pm_type == PM_DEAD ) {
    cg_pmove.trace_mask = *Temp_Clip_Mask(MASK_DEADSOLID, 0);
  } else {
    centity_t *act_ent = &cg_entities[cg.snap->ps.persistant[ PERS_ACT_ENT ]];

    if(
      act_ent->currentState.number != ENTITYNUM_NONE &&
      (cg.snap->ps.eFlags & EF_OCCUPYING) &&
      act_ent->currentState.eType == ET_BUILDABLE) {
      cg_pmove.trace_mask = BG_Buildable(act_ent->currentState.modelindex)->activationClipMask;
    } else {
      cg_pmove.trace_mask = *Temp_Clip_Mask(MASK_PLAYERSOLID, 0);
    }
  }

  if( cg.snap->ps.persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT )
    cg_pmove.trace_mask = *Temp_Clip_Mask(MASK_DEADSOLID, CONTENTS_DOOR); // spectators can fly through bodies

  cg_pmove.noFootsteps = 0;

  // save the state before the pmove so we can detect transitions
  oldPlayerState = cg.predictedPlayerState;

  current = trap_GetCurrentCmdNumber( );

  // if we don't have the commands right after the snapshot, we
  // can't accurately predict a current position, so just freeze at
  // the last good position we had
  cmdNum = current - CMD_BACKUP + 1;
  trap_GetUserCmd( cmdNum, &oldestCmd );

  if( oldestCmd.serverTime > cg.snap->ps.commandTime &&
      oldestCmd.serverTime < cg.time )
  { // special check for map_restart
    if( cg_showmiss.integer )
      CG_Printf( "exceeded PACKET_BACKUP on commands\n" );

    return;
  }

  // get the latest command so we can know which commands are from previous map_restarts
  trap_GetUserCmd( current, &latestCmd );

  // get the most recent information we have, even if
  // the server time is beyond our current cg.time,
  // because predicted player positions are going to
  // be ahead of everything else anyway
  if( cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport )
  {
    cg.predictedPlayerState = cg.nextSnap->ps;
    cg.physicsTime = cg.nextSnap->serverTime;
  }
  else
  {
    cg.predictedPlayerState = cg.snap->ps;
    cg.physicsTime = cg.snap->serverTime;
  }

  if( pmove_msec.integer < 8 )
    trap_Cvar_Set( "pmove_msec", "8" );
  else if( pmove_msec.integer > 33 )
    trap_Cvar_Set( "pmove_msec", "33" );

  cg_pmove.pmove_fixed = pmove_fixed.integer;// | cg_pmove_fixed.integer;
  cg_pmove.pmove_msec = pmove_msec.integer;
  
  cg_pmove.humanStaminaMode = cgs.humanStaminaMode;

  cg_pmove.playerAccelMode = cgs.playerAccelMode;

  // Like the comments described above, a player's state is entirely
  // re-predicted from the last valid snapshot every client frame, which
  // can be really, really, really slow.  Every old command has to be
  // run again.  For every client frame that is *not* directly after a
  // snapshot, this is unnecessary, since we have no new information.
  // For those, we'll play back the predictions from the last frame and
  // predict only the newest commands.  Essentially, we'll be doing
  // an incremental predict instead of a full predict.
  //
  // If we have a new snapshot, we can compare its player state's command
  // time to the command times in the queue to find a match.  If we find
  // a matching state, and the predicted version has not deviated, we can
  // use the predicted state as a base - and also do an incremental predict.
  //
  // With this method, we get incremental predicts on every client frame
  // except a frame following a new snapshot in which there was a prediction
  // error.  This yeilds anywhere from a 15% to 40% performance increase,
  // depending on how much of a bottleneck the CPU is.
  if( cg_optimizePrediction.integer )
  {
    if( cg.nextFrameTeleport || cg.thisFrameTeleport )
    {
      // do a full predict
      cg.lastPredictedCommand = 0;
      cg.stateTail = cg.stateHead;
      predictCmd = current - CMD_BACKUP + 1;
    }
    // cg.physicsTime is the current snapshot's serverTime if it's the same
    // as the last one
    else if( cg.physicsTime == cg.lastServerTime )
    {
      // we have no new information, so do an incremental predict
      predictCmd = cg.lastPredictedCommand + 1;
    }
    else
    {
      // we have a new snapshot
      int errorcode;
      qboolean error = qtrue;

      // loop through the saved states queue
      for( i = cg.stateHead; i != cg.stateTail;
        i = ( i + 1 ) % NUM_SAVED_STATES )
      {
        // if we find a predicted state whose commandTime matches the snapshot
        // player state's commandTime
        if( cg.savedPmoveStates[ i ].commandTime !=
          cg.predictedPlayerState.commandTime )
        {
          continue;
        }
        // make sure the state differences are acceptable
        errorcode = CG_IsUnacceptableError( &cg.predictedPlayerState,
          &cg.savedPmoveStates[ i ] );

        if( errorcode )
        {
          if( cg_showmiss.integer )
            CG_Printf("errorcode %d at %d\n", errorcode, cg.time);
          break;
        }

        // this one is almost exact, so we'll copy it in as the starting point
        *cg_pmove.ps = cg.savedPmoveStates[ i ];
        // advance the head
        cg.stateHead = ( i + 1 ) % NUM_SAVED_STATES;

        // set the next command to predict
        predictCmd = cg.lastPredictedCommand + 1;

        // a saved state matched, so flag it
        error = qfalse;
        break;
      }

      // if no saved states matched
      if( error )
      {
        // do a full predict
        cg.lastPredictedCommand = 0;
        cg.stateTail = cg.stateHead;
        predictCmd = current - CMD_BACKUP + 1;
      }
    }

    // keep track of the server time of the last snapshot so we
    // know when we're starting from a new one in future calls
    cg.lastServerTime = cg.physicsTime;
    stateIndex = cg.stateHead;
  }

  for( cmdNum = current - CMD_BACKUP + 1; cmdNum <= current; cmdNum++ )
  {
    // get the command
    trap_GetUserCmd( cmdNum, &cg_pmove.cmd );

    if( cg_pmove.pmove_fixed )
      PM_UpdateViewAngles( cg_pmove.ps, &cg_pmove.cmd );

    // don't do anything if the time is before the snapshot player time
    if( cg_pmove.cmd.serverTime <= cg.predictedPlayerState.commandTime )
      continue;

    // don't do anything if the command was from a previous map_restart
    if( cg_pmove.cmd.serverTime > latestCmd.serverTime )
      continue;

    // check for a prediction error from last frame
    // on a lan, this will often be the exact value
    // from the snapshot, but on a wan we will have
    // to predict several commands to get to the point
    // we want to compare
    if( cg.predictedPlayerState.commandTime == oldPlayerState.commandTime )
    {
      vec3_t  delta;
      float   len;

      if( cg.thisFrameTeleport )
      {
        // a teleport will not cause an error decay
        VectorClear( cg.predictedError );

        if( cg_showmiss.integer )
          CG_Printf( "PredictionTeleport\n" );

        cg.thisFrameTeleport = qfalse;
      }
      else
      {
        vec3_t  adjusted;

        delta_yaw = CG_AdjustPositionForMover(
                      cg.predictedPlayerState.origin,
                      CG_Get_Pusher_Num(cg.predictedPlayerState.clientNum),
                      cg.physicsTime, cg.oldTime, adjusted);

        if( cg_showmiss.integer )
        {
          if( !VectorCompare( oldPlayerState.origin, adjusted ) )
            CG_Printf("prediction error\n");
        }

        oldPlayerState.viewangles[YAW] += delta_yaw;

        VectorSubtract( oldPlayerState.origin, adjusted, delta );
        len = VectorLength( delta );

        if( len > 0.1 )
        {
          if( cg_showmiss.integer )
            CG_Printf( "Prediction miss: %f\n", len );

          if( cg_errorDecay.integer )
          {
            int   t;
            float f;

            t = cg.time - cg.predictedErrorTime;
            f = ( cg_errorDecay.value - t ) / cg_errorDecay.value;

            if( f < 0 )
              f = 0;

            if( f > 0 && cg_showmiss.integer )
              CG_Printf( "Double prediction decay: %f\n", f );

            VectorScale( cg.predictedError, f, cg.predictedError );
          }
          else
            VectorClear( cg.predictedError );

          VectorAdd( delta, cg.predictedError, cg.predictedError );
          cg.predictedErrorTime = cg.oldTime;
        }
      }
    }

    // don't predict gauntlet firing, which is only supposed to happen
    // when it actually inflicts damage
    for( i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++ )
      cg_pmove.autoWeaponHit[ i ] = qfalse;

    if( cg_pmove.pmove_fixed )
      cg_pmove.cmd.serverTime = ( ( cg_pmove.cmd.serverTime + pmove_msec.integer - 1 ) /
                                  pmove_msec.integer ) * pmove_msec.integer;

    cg_pmove.tauntSpam = cg_tauntSpam.integer;

    for( i = 0; i < PORTAL_NUM; i++ )
      cg_pmove.humanPortalCreateTime[ i ] = cgs.humanPortalCreateTime[ i ];

    if( !cg_optimizePrediction.integer )
    {
      // For firing lightning bolts early
      BG_CheckBoltImpactTrigger(&cg_pmove);
      Pmove( &cg_pmove );
    }
    else if(
      cmdNum >= predictCmd ||
      (stateIndex + 1) % NUM_SAVED_STATES == cg.stateHead)
    {
      // For firing lightning bolts early
      BG_CheckBoltImpactTrigger(&cg_pmove);
      Pmove( &cg_pmove );
      // record the last predicted command
      cg.lastPredictedCommand = cmdNum;

      // if we haven't run out of space in the saved states queue
      if( ( stateIndex + 1 ) % NUM_SAVED_STATES != cg.stateHead )
      {
        // save the state for the false case ( of cmdNum >= predictCmd )
        // in later calls to this function
        cg.savedPmoveStates[ stateIndex ] = *cg_pmove.ps;
        stateIndex = ( stateIndex + 1 ) % NUM_SAVED_STATES;
        cg.stateTail = stateIndex;
      }
    }
    else
    {
      *cg_pmove.ps = cg.savedPmoveStates[ stateIndex ];
      stateIndex = ( stateIndex + 1 ) % NUM_SAVED_STATES;
    }

    // for ckit firing effects
    if( cg.predictedPlayerEntity.buildFireTime >= cg.time )
    {
      cg.predictedPlayerState.generic1 = cg.predictedPlayerEntity.buildFireMode;
      switch( cg.predictedPlayerEntity.buildFireMode )
      {
        case WPM_PRIMARY:
          cg.predictedPlayerState.eFlags |= EF_FIRING;
          break;

        case WPM_SECONDARY:
          cg.predictedPlayerState.eFlags |= EF_FIRING2;
          break;

        case WPM_TERTIARY:
          cg.predictedPlayerState.eFlags |= EF_FIRING3;
          break;

        default:
          break;
      }
    }

    // add push trigger movement effects
    CG_TouchTriggerPrediction( );

    // check for predictable events that changed from previous predictions
    //CG_CheckChangedPredictableEvents(&cg.predictedPlayerState);
  }

  // adjust for the movement of the groundentity
  delta_yaw = CG_AdjustPositionForMover(
                cg.predictedPlayerState.origin,
                CG_Get_Pusher_Num(cg.predictedPlayerState.clientNum),
                cg.physicsTime, cg.time, cg.predictedPlayerState.origin);
  cg.predictedPlayerState.viewangles[YAW] += delta_yaw;


  // fire events and other transition triggered things
  CG_TransitionPlayerState( &cg.predictedPlayerState, &oldPlayerState );


}
