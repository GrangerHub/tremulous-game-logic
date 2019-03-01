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

#define MAX_UNLAGGED_HISTORY_WHEEL_FRAMES 64

/*
--------------------------------------------------------------------------------
Custom Dynamic Memory Management for the unlagged system
*/
#define MAX_UNLAGGED_HISTORY_DATA_CHUNKS (MAX_GENTITIES * MAX_UNLAGGED_HISTORY_WHEEL_FRAMES)

//trajectories, need enough to save the history for both pos and apos
 allocator_protos( unlagged_trajectory )
 allocator( unlagged_trajectory, sizeof(trajectory_t), (MAX_UNLAGGED_HISTORY_DATA_CHUNKS * 2 ) )

/*
===============
G_Unlagged_Dimensions_Memory_Info

Displays info related to allocation for the unlagged system
===============
*/
void G_Unlagged_Memory_Info( void ) {
  memoryInfo_unlagged_trajectory(  );
}

/*
--------------------------------------------------------------------------------
*/

typedef struct unlagged_history_frame_s {
  trajectory_t       *pos;
  trajectory_t       *apos;
  vec3_t             origin;
  vec3_t             mins;
  vec3_t             maxs;
} unlagged_history_frame_t;

static struct frame_usage_s {
  qboolean frame[ENTITYNUM_MAX_NORMAL];
  qboolean dims[ENTITYNUM_MAX_NORMAL];
  qboolean origin[ENTITYNUM_MAX_NORMAL];
} frame_usage[MAX_UNLAGGED_HISTORY_WHEEL_FRAMES];

typedef struct unlagged_latest_hist_data_s {
  qboolean           used;
  trajectory_t       **latest_frame_traj_pointer;
  int                time;
  trajectory_t       *traj;
} unlagged_latest_hist_traj_t;


typedef struct unlagged_s {
  qboolean     used;
  vec3_t       angles;
  vec3_t       origin;
  vec3_t       mins;
  vec3_t       maxs;
  qboolean     use_angles;
  qboolean     use_origin;
  qboolean     use_dims;
} unlagged_t;

typedef struct unlagged_data_s {
  qboolean                     data_stored;
  qboolean                     use_origin;
  unlagged_history_frame_t     history_wheel[MAX_UNLAGGED_HISTORY_WHEEL_FRAMES];
  unlagged_latest_hist_traj_t  latest_saved_pos;
  unlagged_latest_hist_traj_t  latest_saved_apos;
  unlagged_t                   backup;
  unlagged_t                   calc;
} unlagged_data_t;

typedef struct rewind_ent_adjustment_s {
  qboolean use;
  vec3_t   move;
  vec3_t   amove;
} rewind_ent_adjustment_t;

static unlagged_data_t     unlagged_data[ENTITYNUM_MAX_NORMAL];
static unlagged_data_t     *unlagged_data_head;
static int                 history_wheel_times[MAX_UNLAGGED_HISTORY_WHEEL_FRAMES];
static int                 current_history_frame;
static bgqueue_t           dims_store_list = BG_QUEUE_INIT;
static bgqueue_t           origin_store_list = BG_QUEUE_INIT;
static bgqueue_t           pos_store_list = BG_QUEUE_INIT;
static bgqueue_t           apos_store_list = BG_QUEUE_INIT;
static rewind_ent_adjustment_t rewind_ents_adjustments[ENTITYNUM_MAX_NORMAL];

/*
==============
 G_Init_Unlagged
==============
*/
void G_Init_Unlagged(void) {
  initPool_unlagged_trajectory( __FILE__, __LINE__ );

  current_history_frame = 0;
  unlagged_data_head = NULL;
  memset(&history_wheel_times, 0, sizeof(history_wheel_times));
  memset(&unlagged_data, 0, sizeof(unlagged_data));
  memset(rewind_ents_adjustments, 0, sizeof(rewind_ents_adjustments));
  memset(frame_usage, 0, sizeof(frame_usage));
  BG_Queue_Clear(&dims_store_list);
  BG_Queue_Clear(&origin_store_list);
  BG_Queue_Clear(&pos_store_list);
  BG_Queue_Clear(&apos_store_list);
}

/*
==============
 G_Unlagged_Prepare_Store

 Called right before the entity think loop in G_RunFrame().
==============
*/
void G_Unlagged_Prepare_Store(void) { 
  if(!g_unlagged.integer) {
    return;
  }

  BG_Queue_Clear(&dims_store_list);
  BG_Queue_Clear(&origin_store_list);
  BG_Queue_Clear(&pos_store_list);
  BG_Queue_Clear(&apos_store_list);
}

/*
==============
 G_Unlagged_Link_To_Store_Data

 Called in the entity think loop in G_RunFrame() to link the entity for
 data storage of the given type(s)
==============
*/
void G_Unlagged_Link_To_Store_Data(
  gentity_t *ent,
  qboolean dims,
  qboolean pos,
  qboolean apos,
  qboolean origin) {
  Com_Assert(ent && "G_Unlagged_Link_To_Store_Data: ent is NULL")

  if(!g_unlagged.integer) {
    return;
  }

  if(!ent->inuse) {
    return;
  }

  if(!(ent->r.contents & MASK_SHOT)) {
    return;
  }

  if(dims) {
    BG_Queue_Push_Head(&dims_store_list, ent);
  }

  if(pos) {
    BG_Queue_Push_Head(&pos_store_list, ent);
  }

  if(apos) {
    BG_Queue_Push_Head(&apos_store_list, ent);
  }

  if(origin) {
    BG_Queue_Push_Head(&origin_store_list, ent);
  }
}

/*
==============
 G_Unlagged_Store_Dimensions
==============
*/
static void G_Unlagged_Store_Dimensions(void *data, void *user_data) {
  gentity_t                *ent = (gentity_t *)data;
  unlagged_history_frame_t *save;

  Com_Assert(ent && "G_Unlagged_Store_Dimensions: ent is NULL");

  save = &unlagged_data[ent->s.number].history_wheel[current_history_frame];

  VectorCopy(ent->r.mins, save->mins);
  VectorCopy(ent->r.maxs, save->maxs);
  frame_usage[current_history_frame].frame[ent->s.number] = qtrue;
  frame_usage[current_history_frame].dims[ent->s.number] = qtrue;
  unlagged_data[ent->s.number].data_stored = qtrue;
}

/*
==============
 G_Unlagged_Store_Origin
==============
*/
static void G_Unlagged_Store_Origin(void *data, void *user_data) {
  gentity_t                *ent = (gentity_t *)data;
  unlagged_history_frame_t *save;

  Com_Assert(ent && "G_Unlagged_Store_Origin: ent is NULL");

  save = &unlagged_data[ent->s.number].history_wheel[current_history_frame];

  VectorCopy(ent->r.currentOrigin, save->origin);
  frame_usage[current_history_frame].frame[ent->s.number] = qtrue;
  frame_usage[current_history_frame].origin[ent->s.number] = qtrue;
  unlagged_data[ent->s.number].data_stored = qtrue;
  unlagged_data[ent->s.number].use_origin = qtrue;
}

/*
==============
 G_SaveTrajectory

Saves a trajectory only if it different from the from the latest saved trajectory
==============
*/
static qboolean G_SaveTrajectory(
  gentity_t                   *ent,
  const trajectory_t          *current_traj,
  trajectory_t                **frame_traj,
  unlagged_latest_hist_traj_t *latest_saved_traj) {
  Com_Assert(ent && "G_SaveTrajectory: ent");
  Com_Assert(current_traj && "G_SaveTrajectory: current_traj");
  Com_Assert(frame_traj && "G_SaveTrajectory: frame_traj");
  Com_Assert(latest_saved_traj && "G_SaveTrajectory: latest_saved_traj");

  if(latest_saved_traj->traj) {
    trajectory_t *traj = latest_saved_traj->traj;

    //only save if the trajectory changed from the last valid saved trajectory
    if(
      traj->trTime != current_traj->trTime ||
      ent->client ||
      !latest_saved_traj->used) {
      if(*latest_saved_traj->latest_frame_traj_pointer != traj) {
        free_unlagged_trajectory(traj, __FILE__, __LINE__);
      }

      if(*frame_traj) {
        free_unlagged_trajectory(*frame_traj, __FILE__, __LINE__);
      }

      *frame_traj = alloc_unlagged_trajectory(__FILE__, __LINE__);
      latest_saved_traj->traj = *frame_traj;
      latest_saved_traj->latest_frame_traj_pointer = frame_traj;
      latest_saved_traj->used = qtrue;
      if(
        current_traj->trType == TR_STATIONARY ||
        current_traj->trType == TR_INTERPOLATE) {
        //no need to copy the full trajectory for TR_STATIONARY and TR_INTERPOLATE
        (*frame_traj)->trType = current_traj->trType;
        (*frame_traj)->trTime = current_traj->trTime;
        VectorCopy(current_traj->trBase, (*frame_traj)->trBase);
      } else {
        **frame_traj = *current_traj;
      }

      return qtrue;
    }
  } else {
    if(*frame_traj) {
      free_unlagged_trajectory(*frame_traj, __FILE__, __LINE__);
    }

    *frame_traj = alloc_unlagged_trajectory(__FILE__, __LINE__);
    latest_saved_traj->traj = *frame_traj;
    latest_saved_traj->latest_frame_traj_pointer = frame_traj;
    latest_saved_traj->used = qtrue;
    if(
      current_traj->trType == TR_STATIONARY ||
      current_traj->trType == TR_INTERPOLATE) {
      //no need to copy the full trajectory for TR_STATIONARY and TR_INTERPOLATE
      (*frame_traj)->trType = current_traj->trType;
      (*frame_traj)->trTime = level.time;
      VectorCopy(ent->r.currentOrigin, (*frame_traj)->trBase);
    } else {
      **frame_traj = *current_traj;
    }

    return qtrue;
  }

  return qfalse;
}

/*
==============
 G_Unlagged_Store_pos
==============
*/
static void G_Unlagged_Store_pos(void *data, void *user_data) {
  gentity_t *ent = (gentity_t *)data;
  unlagged_history_frame_t *save;
  unlagged_data_t          *data_for_ent;

  Com_Assert(ent && "G_Unlagged_Store_pos: ent is NULL");

  data_for_ent = &unlagged_data[ent->s.number];
  save = &data_for_ent->history_wheel[current_history_frame];

  if(G_SaveTrajectory(ent, &ent->s.pos, &save->pos, &data_for_ent->latest_saved_pos)) {
    frame_usage[current_history_frame].frame[ent->s.number] = qtrue;
    data_for_ent->data_stored = qtrue;
  } 
}

/*
==============
 G_Unlagged_Store_apos

 Called in the entity think loop in G_RunFrame() for entities that
 would have their pos trajectory changed
==============
*/
static void G_Unlagged_Store_apos(void *data, void *user_data) {
  gentity_t *ent = (gentity_t *)data;
  unlagged_history_frame_t *save;
  unlagged_data_t          *data_for_ent;

  Com_Assert(ent && "G_Unlagged_Store_apos: ent is NULL")

  data_for_ent = &unlagged_data[ent->s.number];
  save = &data_for_ent->history_wheel[current_history_frame];

  if(G_SaveTrajectory(ent, &ent->s.pos, &save->pos, &data_for_ent->latest_saved_pos)) {
    frame_usage[current_history_frame].frame[ent->s.number] = qtrue;
    data_for_ent->data_stored = qtrue;
  } 
}

/*
==============
G_UnlaggedStore

Called after the entity think loop in G_RunFrame(), and stores the dimension,
pos, and/or apos data for all entities that were appropriately linked for this
storage.
==============
*/
void G_UnlaggedStore( void ) {
  if(!g_unlagged.integer) {
    return;
  }

  current_history_frame++;

  if(current_history_frame >= MAX_UNLAGGED_HISTORY_WHEEL_FRAMES) {
    current_history_frame = 0;
  }

  history_wheel_times[current_history_frame] = level.time;

  memset(
    &frame_usage[current_history_frame],
    0,
    sizeof(frame_usage[current_history_frame]));

  BG_Queue_Foreach(&dims_store_list, G_Unlagged_Store_Dimensions, NULL);
  BG_Queue_Foreach(&origin_store_list, G_Unlagged_Store_Origin, NULL);
  BG_Queue_Foreach(&pos_store_list, G_Unlagged_Store_pos, NULL);
  BG_Queue_Foreach(&apos_store_list, G_Unlagged_Store_apos, NULL);
}

/*
==============
 G_UnlaggedClear

 Mark all history_wheel[] frames for this entitiy invalid.  Useful for
 preventing teleporting and death.
==============
*/
void G_UnlaggedClear(gentity_t *ent) {
  Com_Assert(ent && "G_UnlaggedClear: ent is NULL")

  if(unlagged_data[ent->s.number].data_stored) {
    int i;

    for(i = 0; i < MAX_UNLAGGED_HISTORY_WHEEL_FRAMES; i++) {
      frame_usage[i].frame[ent->s.number] = qfalse;
      frame_usage[i].origin[ent->s.number] = qfalse;
      frame_usage[i].dims[ent->s.number] = qfalse;
    }

    unlagged_data[ent->s.number].latest_saved_pos.used = qfalse;
    unlagged_data[ent->s.number].latest_saved_apos.used = qfalse;

    unlagged_data[ent->s.number].calc.used = qfalse;
    unlagged_data[ent->s.number].data_stored = qfalse;
    unlagged_data[ent->s.number].use_origin = qfalse;
  }
}

/*
==============
 G_UnlaggedLerpFraction
==============
*/
static float G_UnlaggedLerpFraction(int time, int start_time, int stop_time) {
  float frameMsec;

  if(start_time == stop_time) {
    return 0.0f;
  } else {
    frameMsec = stop_time - start_time;
    return (float)(time - start_time) / (float)frameMsec;
  }
}

/*
==============
 G_UnlaggedCalc

 Loops through all the unlagged_data for all entitties and calculates their
 predicted position for time then stores it in unlagged_data[].calc
==============
*/
void G_UnlaggedCalc(int time, gentity_t *rewindEnt) {
  int i = 0;
  gentity_t *ent;
  int startIndex;
  int stopIndex;
  float lerp;
  rewind_ent_adjustment_t *rewind_ent_adjustment;

  Com_Assert(rewindEnt && "G_UnlaggedCalc: rewindEnt is NULL");
  Com_Assert(rewindEnt->client && "G_UnlaggedCalc: rewindEnt->client is NULL");

  if(!g_unlagged.integer) {
    return;
  }

  // clear any calculated values from a previous run
  for(i = 0; i < ENTITYNUM_MAX_NORMAL; i++) {
    unlagged_data[i].calc.used = qfalse;
  }

  if(!rewindEnt->client->pers.useUnlagged) {
    return;
  }

  // client is on the current frame, no need for unlagged
  if(history_wheel_times[current_history_frame] <= time) {
    return;
  }

  startIndex = current_history_frame;
  for(i = 1; i < MAX_UNLAGGED_HISTORY_WHEEL_FRAMES; i++) {
    stopIndex = startIndex;

    if(--startIndex < 0) {
      startIndex = MAX_UNLAGGED_HISTORY_WHEEL_FRAMES - 1;
    }

    if(history_wheel_times[startIndex] <= time) {
      break;
    }
  }

  if(i == MAX_UNLAGGED_HISTORY_WHEEL_FRAMES) {
    // if we searched all markers and the oldest one still isn't old enough
    // just use the oldest marker with no lerping
    lerp = 0.0f;
  } else {
    // lerp between two markers
    lerp = G_UnlaggedLerpFraction(
      time,
      history_wheel_times[startIndex],
      history_wheel_times[stopIndex]);
  }

  for(i = 0; i < ENTITYNUM_MAX_NORMAL; i++) {
    unlagged_data_t *unlagged_data_for_ent = &unlagged_data[i];
    trajectory_t *start_pos = NULL;
    trajectory_t *stop_pos = NULL;
    trajectory_t *start_apos = NULL;
    trajectory_t *stop_apos = NULL;
    qboolean     start_index_frame_used = qfalse;
    qboolean     stop_index_frame_used = qfalse;

    ent = &g_entities[i];

    if(!unlagged_data_for_ent->data_stored) {
      continue;
    }

    if(!ent->r.linked || !(ent->r.contents & MASK_SHOT)) {
      continue;
    }

    if(ent->client) {
      if(ent->client->pers.connected != CON_CONNECTED || ent == rewindEnt) {
        continue;
      }
    }

    if(frame_usage[startIndex].frame[i]) {
      start_index_frame_used = qtrue;
    }

    if(frame_usage[stopIndex].frame[i]) {
      stop_index_frame_used = qtrue;
    }

    //for calculating the pos
    if(
      unlagged_data_for_ent->latest_saved_pos.used &&
      unlagged_data_for_ent->latest_saved_pos.traj ) {
      if(unlagged_data_for_ent->latest_saved_pos.traj->trTime < time) {
        //use the latest saved pos
        start_pos = unlagged_data_for_ent->latest_saved_pos.traj;
      } else if(start_index_frame_used) {
        int check_frame;

        //find the start pos
        check_frame = startIndex;
        while(check_frame != current_history_frame){
          if(
            frame_usage[check_frame].frame[ent->s.number] &&
            unlagged_data_for_ent->history_wheel[check_frame].pos) {
            start_pos = unlagged_data_for_ent->history_wheel[check_frame].pos;
            break;
          }

          //decrement
          check_frame--;
          if(check_frame < 0) {
            check_frame = MAX_UNLAGGED_HISTORY_WHEEL_FRAMES - 1;
          }
        }
      }

      //find the stop pos)
      stop_pos = unlagged_data_for_ent->history_wheel[stopIndex].pos;
      if(!stop_pos || !stop_index_frame_used) {
        stop_pos = start_pos;
      }
    }

    //for calculating the apos
    if(
      unlagged_data_for_ent->latest_saved_apos.used &&
      unlagged_data_for_ent->latest_saved_apos.traj) {
      if(unlagged_data_for_ent->latest_saved_apos.traj->trTime < time) {
        //use the latest saved apos
        start_apos = unlagged_data_for_ent->latest_saved_apos.traj;
      } else if(start_index_frame_used) {
        int check_frame;

        //find the start apos
        check_frame = startIndex;
        while(check_frame != current_history_frame){
          if(
            frame_usage[check_frame].frame[ent->s.number] &&
            unlagged_data_for_ent->history_wheel[check_frame].apos) {
            start_apos = unlagged_data_for_ent->history_wheel[check_frame].apos;
            break;
          }

          //decrement
          check_frame--;
          if(check_frame < 0) {
            check_frame = MAX_UNLAGGED_HISTORY_WHEEL_FRAMES - 1;
          }
        }

      }

      //find the stop apos
      stop_apos = unlagged_data_for_ent->history_wheel[stopIndex].apos;
      if(!stop_apos || !stop_index_frame_used) {
        stop_apos = start_apos;
      }
    }

    if(
      !start_index_frame_used &&
      !start_pos &&
      !start_apos) {
      continue;
    }

    //calculate the dimensions
    if(start_index_frame_used &&
      frame_usage[startIndex].dims[i]) {
      unlagged_data_for_ent->calc.use_dims = qtrue;

      if(stop_index_frame_used) {
        // between two unlagged markers
        VectorLerp2( lerp, unlagged_data_for_ent->history_wheel[ startIndex ].mins,
          unlagged_data_for_ent->history_wheel[ stopIndex ].mins,
          unlagged_data_for_ent->calc.mins );
        VectorLerp2( lerp, unlagged_data_for_ent->history_wheel[ startIndex ].maxs,
          unlagged_data_for_ent->history_wheel[ stopIndex ].maxs,
          unlagged_data_for_ent->calc.maxs );
      } else {
        VectorCopy(
          unlagged_data_for_ent->history_wheel[ startIndex ].mins,
          unlagged_data_for_ent->calc.mins);
        VectorCopy(
          unlagged_data_for_ent->history_wheel[ startIndex ].maxs,
          unlagged_data_for_ent->calc.maxs);
      }
    } else if(stop_index_frame_used && frame_usage[stopIndex].dims[i]) {
      unlagged_data_for_ent->calc.use_dims = qtrue;

      VectorCopy(
        unlagged_data_for_ent->history_wheel[ stopIndex ].mins,
        unlagged_data_for_ent->calc.mins);
      VectorCopy(
        unlagged_data_for_ent->history_wheel[ stopIndex ].maxs,
        unlagged_data_for_ent->calc.maxs);
    } else {
      unlagged_data_for_ent->calc.use_dims = qfalse;
    }

    //calculate the origin
    if(start_index_frame_used &&
      frame_usage[startIndex].origin[i]) {
      unlagged_data_for_ent->calc.use_origin = qtrue;

      if(stop_index_frame_used) {
        // between two unlagged markers
        VectorLerp2( lerp, unlagged_data_for_ent->history_wheel[ startIndex ].origin,
          unlagged_data_for_ent->history_wheel[ stopIndex ].origin,
          unlagged_data_for_ent->calc.origin );
      } else {
        VectorCopy(
          unlagged_data_for_ent->history_wheel[ startIndex ].origin,
          unlagged_data_for_ent->calc.origin);
      }
    } else if(stop_index_frame_used && frame_usage[stopIndex].origin[i]) {
      unlagged_data_for_ent->calc.use_origin = qtrue;

      VectorCopy(
        unlagged_data_for_ent->history_wheel[ stopIndex ].origin,
        unlagged_data_for_ent->calc.origin);
    } else {
      unlagged_data_for_ent->calc.use_origin = qfalse;
    }


    if(!unlagged_data_for_ent->calc.use_origin) {
      //calculate the pos
      if(start_pos) {
        unlagged_data_for_ent->calc.use_origin = qtrue;

        if(
          stop_pos &&
          (
            start_pos->trTime != stop_pos->trTime ||
            start_pos->trType != stop_pos->trType) &&
          stop_pos->trTime <= history_wheel_times[stopIndex]) {
          vec3_t start_origin, stop_origin;

          //lerp
          BG_EvaluateTrajectory(start_pos, history_wheel_times[startIndex], start_origin);
          BG_EvaluateTrajectory(stop_pos, history_wheel_times[stopIndex], stop_origin);
          VectorLerp2( lerp, start_origin, stop_origin, unlagged_data_for_ent->calc.origin );
        } else {
          int used_time = (time > start_pos->trTime) ? time : start_pos->trTime;

          BG_EvaluateTrajectory(start_pos, used_time, unlagged_data_for_ent->calc.origin);
        }
      } else {
        unlagged_data_for_ent->calc.use_origin = qfalse;
      }
    }

    //calculate the angles
    if(start_apos) {
      unlagged_data_for_ent->calc.use_angles = qtrue;

      if(
        stop_apos &&
        (
          start_apos->trTime != stop_apos->trTime ||
          start_apos->trType != stop_apos->trType) &&
        stop_apos->trTime <= history_wheel_times[stopIndex]) {
        vec3_t start_angles, stop_angles;

        //lerp
        BG_EvaluateTrajectory(start_apos, history_wheel_times[startIndex], start_angles);
        BG_EvaluateTrajectory(stop_apos, history_wheel_times[stopIndex], stop_angles);
        VectorLerp2( lerp, start_angles, stop_angles, unlagged_data_for_ent->calc.angles );
      } else {
        int used_time = (time > start_apos->trTime) ? time : start_apos->trTime;

        BG_EvaluateTrajectory(start_apos, used_time, unlagged_data_for_ent->calc.angles);
      }
    } else {
      unlagged_data_for_ent->calc.use_angles = qfalse;
    }

    if(
      unlagged_data_for_ent->calc.use_dims ||
      unlagged_data_for_ent->calc.use_origin ||
      unlagged_data_for_ent->calc.use_angles) {
      unlagged_data_for_ent->calc.used = qtrue;
    }
  }

  //account for any movers acted on the rewindEnt
  rewind_ent_adjustment = &rewind_ents_adjustments[rewindEnt->s.number];
  if(
    time == level.time ||
    (
      rewindEnt->s.eType != ET_PLAYER && rewindEnt->s.eType != ET_BUILDABLE &&
      rewindEnt->s.eType != ET_ITEM && rewindEnt->s.eType != ET_CORPSE)) {
    rewind_ent_adjustment->use = qfalse;
  } else {
    int pusher_num;

    if(rewindEnt->client) {
      pusher_num = rewindEnt->client->ps.otherEntityNum;
    } else {
      pusher_num = rewindEnt->s.otherEntityNum;
    }

    if(pusher_num == ENTITYNUM_NONE) {
      int foundation_num = G_Get_Foundation_Ent_Num(rewindEnt);

      if(
        foundation_num != ENTITYNUM_NONE &&
        g_entities[foundation_num].s.eType == ET_MOVER) {
        pusher_num = foundation_num;
      }
    }

    if(pusher_num == ENTITYNUM_NONE) {
      rewind_ent_adjustment->use = qfalse;
    } else {
      int       fromTime, toTime;
      qboolean  negate_moves;
      gentity_t *pusher;
      vec3_t    old_origin, origin;
      vec3_t    old_angles, angles;
      vec3_t    org, org2, move2;
      vec3_t    matrix[3], transpose[3];

      rewind_ent_adjustment->use = qtrue;

      pusher = &g_entities[pusher_num];

      if(time < level.time) {
        negate_moves = qtrue;
        fromTime = time;
        toTime = level.time;
      } else {
        negate_moves = qfalse;
        fromTime = level.time;
        toTime = time;
      }

      BG_EvaluateTrajectory(&pusher->s.pos, fromTime, old_origin);
      BG_EvaluateTrajectory(&pusher->s.apos, fromTime, old_angles);

      BG_EvaluateTrajectory(&pusher->s.pos, toTime, origin);
      BG_EvaluateTrajectory(&pusher->s.apos, toTime, angles);

      VectorSubtract(origin, old_origin, rewind_ent_adjustment->move);
      VectorSubtract(angles, old_angles, rewind_ent_adjustment->amove);

      // figure movement due to the pusher's amove
      BG_CreateRotationMatrix(rewind_ent_adjustment->amove, transpose);
      BG_TransposeMatrix(transpose, matrix);

      VectorSubtract(rewindEnt->client->ps.origin, old_origin, org);

      VectorCopy(org, org2);
      BG_RotatePoint(org2, matrix);
      VectorSubtract(org2, org, move2);
      // combine movements
      VectorAdd(
        rewind_ent_adjustment->move,
        move2,
        rewind_ent_adjustment->move);

      rewind_ent_adjustment->amove[ROLL] = 0.0f;
      rewind_ent_adjustment->amove[PITCH] = 0.0f;

      if(negate_moves) {
        VectorNegate(rewind_ent_adjustment->move, rewind_ent_adjustment->move);
        VectorNegate(rewind_ent_adjustment->amove, rewind_ent_adjustment->amove);
      }
    }
  }
}

/*
==============
 G_UnlaggedOff

 Reverses all changes made to any entities by G_UnlaggedOn()
==============
*/
void G_UnlaggedOff(void) {
  int i = 0;

  if(!g_unlagged.integer) {
    return;
  }

  for(i = 0; i < ENTITYNUM_MAX_NORMAL; i++) {
    gentity_t  *ent = &g_entities[ i ];
    unlagged_t *backup = &unlagged_data[i].backup;

    if(!backup->used) {
      continue;
    }

    if(backup->use_dims) {
      VectorCopy(backup->mins, ent->r.mins);
      VectorCopy(backup->maxs, ent->r.maxs);
    }

    if(backup->use_origin) {
      VectorCopy(backup->origin, ent->r.currentOrigin);
    }

    if(backup->use_angles) {
      VectorCopy(backup->angles, ent->r.currentAngles);
    }

    backup->used = qfalse;
    SV_LinkEntity(ent);
  }
}

/*
==============
 G_UnlaggedOn

 Called after G_UnlaggedCalc() to apply the calculated values to all active
 clients.  Once finished tracing, G_UnlaggedOff() must be called to restore
 the clients' position data

 As an optimization, all clients that have an unlagged position that is
 not touchable at "range" from "muzzle" will be ignored.  This is required
 to prevent a huge amount of SV_LinkEntity() calls per user cmd.
==============
*/

void G_UnlaggedOn(unlagged_attacker_data_t *attacker_data) {
  int                     i;
  gentity_t               *attacker;
  rewind_ent_adjustment_t *rewind_ent_adjustment;
  vec3_t                  backup_origin;
  vec3_t                  unlagged_point;

  Com_Assert(attacker_data && "G_UnlaggedOn: attacker_data is NULL");

  if(!g_unlagged.integer) {
    return;
  }

  attacker = &g_entities[attacker_data->ent_num];

  if(!attacker->client || !attacker->client->pers.useUnlagged) {
    return;
  }

  rewind_ent_adjustment = &rewind_ents_adjustments[attacker_data->ent_num];

  //evaluate the view vectors
  if(rewind_ent_adjustment->use) {
    vec3_t unlagged_view_angles;

    VectorAdd(
      attacker->client->ps.viewangles, rewind_ent_adjustment->amove,
      unlagged_view_angles);

    AngleVectors(
      unlagged_view_angles, attacker_data->forward_out,
      attacker_data->right_out, attacker_data->up_out);
  } else {
    AngleVectors(
      attacker->client->ps.viewangles,
      attacker_data->forward_out,
      attacker_data->right_out,
      attacker_data->up_out);
  }

  //evalutate the muzzle
  if(rewind_ent_adjustment->use) {

    VectorCopy(attacker->client->ps.origin, backup_origin);
    VectorAdd(
      attacker->client->ps.origin, rewind_ent_adjustment->move,
      attacker->client->ps.origin);
  }

  BG_CalcMuzzlePointFromPS(
    &attacker->client->ps,
    attacker_data->forward_out,
    attacker_data->right_out,
    attacker_data->up_out,
    attacker_data->muzzle_out);

  if(rewind_ent_adjustment->use) {
    VectorCopy(backup_origin, attacker->client->ps.origin);
  }

  //evaluate the current origin
  if(rewind_ent_adjustment->use) {
    VectorAdd(
      attacker->client->ps.origin, rewind_ent_adjustment->move,
      attacker_data->origin_out);
  } else {
    VectorCopy(attacker->client->ps.origin, attacker_data->origin_out);
    
  }

  switch (attacker_data->point_type) {
    case UNLGD_PNT_MUZZLE:
      VectorCopy(attacker_data->muzzle_out, unlagged_point);
      break;

    case UNLGD_PNT_ORIGIN:
      VectorCopy(attacker_data->origin_out, unlagged_point);
      break;

    case UNLGD_PNT_OLD_ORIGIN:
      VectorCopy(attacker->client->oldOrigin, unlagged_point);
      break;

    default:
      VectorCopy(attacker_data->muzzle_out, unlagged_point);
      break;
  }

  for(i = 0; i < ENTITYNUM_MAX_NORMAL; i++) {
    gentity_t  *ent = &g_entities[i];
    unlagged_t *calc = &unlagged_data[i].calc;
    unlagged_t *backup = &unlagged_data[i].backup;

    if(!calc->used) {
      continue;
    }

    if(backup->used) {
      continue;
    }

    if(!ent->r.linked || !(ent->r.contents & MASK_SHOT)) {
      continue;
    }

    if(!calc->used) {
      continue;
    }

    if(attacker_data->point_type != UNLGD_PNT_NONE){
      float r1 = Distance(calc->origin, calc->maxs);
      float r2 = Distance(calc->origin, calc->mins);
      float maxRadius = (r1 > r2) ? r1 : r2;
      float unlagged_range = attacker_data->range + maxRadius;

      if(
        Distance(unlagged_point, calc->origin) > unlagged_range &&
        Distance(unlagged_point, ent->r.currentOrigin) > unlagged_range) {
        continue;
      }
    }

    backup->used = qtrue;

    if(calc->use_dims) {
      //create a backup of the real dimensions
      VectorCopy(ent->r.mins, backup->mins);
      VectorCopy(ent->r.maxs, backup->maxs);
      backup->use_dims = qtrue;

      //change to the unlagged dimentions
      VectorCopy(calc->mins, ent->r.mins);
      VectorCopy(calc->maxs, ent->r.maxs);
    }

    if(calc->use_origin) {
      //create a backup of the real origin
      VectorCopy(ent->r.currentOrigin, backup->origin);
      backup->use_origin = qtrue;

      //move the entity to the unlagged origin
      VectorCopy(calc->origin, ent->r.currentOrigin);
    }

    if(calc->use_angles) {
      //create a backup of the real angles
      VectorCopy(ent->r.currentAngles, backup->angles);
      backup->use_angles = qtrue;

      //rotate the entity to the unlagged angles
      VectorCopy(calc->angles, ent->r.currentAngles);
    }

    SV_LinkEntity(ent);
  }
}

/*
==============
 G_UnlaggedDetectCollisions

 cgame prediction will predict a client's own position all the way up to
 the current time, but only updates other player's positions up to the
 postition sent in the most recent snapshot.

 This allows player X to essentially "move through" the position of player Y
 when player X's cmd is processed with Pmove() on the server.  This is because
 player Y was clipping player X's Pmove() on his client, but when the same
 cmd is processed with Pmove on the server it is not clipped.

 Long story short (too late): don't use unlagged positions for players who
 were blocking this player X's client-side Pmove().  This makes the assumption
 that if player X's movement was blocked in the client he's going to still
 be up against player Y when the Pmove() is run on the server with the
 same cmd.

 This does not apply to collisions with other entities with fully predictable
 trajectories.

 NOTE: this must be called after Pmove() and G_UnlaggedCalc()
==============
*/
void G_UnlaggedDetectCollisions(gentity_t *ent) {
  unlagged_t               *calc;
  trace_t                  tr;
  float                    r1, r2;
  float                    range;
  vec3_t                   *unlagged_origin, *unlagged_mins, *unlagged_maxs;
  unlagged_attacker_data_t attacker_data;

  Com_Assert(ent && "G_UnlaggedDetectCollisions: ent is NULL");

  if(!g_unlagged.integer) {
    return;
  }

  if(!ent->client->pers.useUnlagged) {
    return;
  }

  calc = &unlagged_data[ent->s.number].calc;

  if(!calc->used) {
    return;
  }

  // if the client isn't moving, this is not necessary
  if( VectorCompare(ent->client->oldOrigin, ent->client->ps.origin)) {
    return;
  }

  if(calc->use_origin) {
    unlagged_origin = &calc->origin;
  } else {
    unlagged_origin = &ent->r.currentOrigin;
  }

  if(calc->use_dims) {
    unlagged_mins = &calc->mins;
    unlagged_maxs = &calc->maxs;
  } else {
    unlagged_mins = &ent->r.mins;
    unlagged_maxs = &ent->r.maxs;
  }

  range = Distance(ent->client->oldOrigin, ent->client->ps.origin);

  // increase the range by the player's largest possible radius since it's
  // the players bounding box that collides, not their origin
  r1 = Distance(*unlagged_origin, *unlagged_mins);
  r2 = Distance(*unlagged_origin, *unlagged_maxs);
  range += (r1 > r2) ? r1 : r2;

  attacker_data.ent_num = ent->s.number;
  attacker_data.point_type = UNLGD_PNT_OLD_ORIGIN;
  attacker_data.range = range;
  G_UnlaggedOn(&attacker_data);

  SV_Trace(&tr, ent->client->oldOrigin, ent->r.mins, ent->r.maxs,
    ent->client->ps.origin, ent->s.number,  MASK_PLAYERSOLID, TT_AABB);

  if(tr.entityNum >= 0 && tr.entityNum < MAX_CLIENTS) {
    unlagged_data[ tr.entityNum ].calc.used = qfalse;
  }

  G_UnlaggedOff( );
}

/*
==============
 G_GetUnlaggedOrigin

 Outputs the unlagged origin for an entity when applicable, otherwsie outputs
 the entity's currentOrigin
==============
*/
void G_GetUnlaggedOrigin(gentity_t *ent, vec3_t origin) {
  Com_Assert(ent && "G_GetUnlaggedOrigin: ent");
  Com_Assert(origin && "G_GetUnlaggedOrigin: angles");

  if(
    g_unlagged.integer &&
    unlagged_data[ent->s.number].calc.used &&
    unlagged_data[ent->s.number].calc.use_origin) {
    VectorCopy( unlagged_data[ent->s.number].calc.origin, origin );
  } else {
    VectorCopy( ent->r.currentOrigin, origin );
  }
}

/*
==============
 G_GetUnlaggedAngles

 Outputs the unlagged angles for an entity when applicable, otherwsie outputs
 the entity's currentAngles
==============
*/
void G_GetUnlaggedAngles(gentity_t *ent, vec3_t angles) {
  Com_Assert(ent && "G_GetUnlaggedAngles: ent");
  Com_Assert(angles && "G_GetUnlaggedAngles: angles");

  if(
    g_unlagged.integer &&
    unlagged_data[ent->s.number].calc.used &&
    unlagged_data[ent->s.number].calc.use_angles) {
    VectorCopy( unlagged_data[ent->s.number].calc.angles, angles );
  } else {
    VectorCopy( ent->r.currentAngles, angles );
  }
}

/*
==============
 G_GetUnlaggedDimensions

 Outputs the unlagged dimentions for an entity when applicable, otherwsie outputs
 the entity's current mins/maxs
==============
*/
void G_GetUnlaggedDimensions(gentity_t *ent, vec3_t mins, vec3_t maxs) {
  Com_Assert(ent && "G_GetUnlaggedOrigin: ent");
  Com_Assert(mins && "G_GetUnlaggedOrigin: mins");
  Com_Assert(maxs && "G_GetUnlaggedOrigin: maxs");

  if(
    g_unlagged.integer &&
    unlagged_data[ent->s.number].calc.used &&
    unlagged_data[ent->s.number].calc.use_dims) {
    VectorCopy( unlagged_data[ent->s.number].calc.mins, mins );
    VectorCopy( unlagged_data[ent->s.number].calc.maxs, maxs );
  } else {
    VectorCopy( ent->r.mins, mins );
    VectorCopy( ent->r.maxs, maxs );
  }
}

/*
==============
 G_DisableUnlaggedCalc

 Disables the unlagged calculation for the given entity
==============
*/
void G_DisableUnlaggedCalc(gentity_t *ent) {
  Com_Assert(ent && "G_DisableUnlaggedCalc");

  unlagged_data[ent->s.number].calc.used = qfalse;
}
