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

#include "g_local.h"

typedef enum mover_block_s
{
  MBLCK_NONE = 0, //not blocked
  MBLCK_INDIRECT, //entity has collided with movable entity class that is
                  //blocked either directly or indirectly.
  MBLCK_DIRECT //collided with an entity class this entity can’t move.
} mover_block_t;

typedef enum mover_push_s {
  MPUSH_NONE = 0,
  MPUSH_RIDE, //groud entity is either a mover or something moved by a mover
  MPUSH_RIDING_STACK_HIT, //hit by a stack that was not hit by a prime mover
  MPUSH_PRIME_MOVER_HIT, //hit by a prime mover or by a stack hit by a prime mover
} mover_push_t;

typedef struct mover_data_s {
  qboolean      checked_for_push;
  qboolean      check_for_direct_block;
  mover_block_t blocked; //block status for entity

  //origin/angles at the start of the move
  vec3_t        start_origin;
  vec3_t        start_angles;

  //mins/maxs at the potential destination
  vec3_t        dest_mins;
  vec3_t        dest_maxs;
} mover_data_t;

typedef struct pushes_s {
  mover_data_t mover_data[MAX_GENTITIES];
  //first index is the pusher, second is the pushee
  mover_push_t type[MAX_GENTITIES][MAX_GENTITIES]; 
} pushes_t;

static pushes_t pushes;

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

typedef struct
{
  gentity_id id;
  vec3_t  origin;
  vec3_t  angles;
  float deltayaw;
} pushed_t;

#define MAX_PUSHES (MAX_GENTITIES * 8)

pushed_t  pushed[ MAX_PUSHES ], *pushed_p;

/*
============
G_Set_Pusher_Num

for client side mover prediction, a mover that is pushing this entity,
that this entity isn't riding, has its entity number stored here.
============
*/
void G_Set_Pusher_Num(gentity_t *ent, int pusher_num ) {
  Com_Assert(ent && "G_Set_Pusher_Num: ent is NULL");

  // only push items and players
  if(
    ent->s.eType != ET_ITEM && ent->s.eType != ET_BUILDABLE &&
    ent->s.eType != ET_CORPSE && ent->s.eType != ET_PLAYER &&
    !ent->physicsObject ) {
    return;
  }

  if(ent->client) {
    ent->client->ps.otherEntityNum = pusher_num;
  }

  ent->s.otherEntityNum = pusher_num;
}

/*
============
G_TestEntityPosition

============
*/
gentity_t *G_TestEntityPosition(gentity_t *ent) {
  trace_t tr;

  if(ent->client) {
    SV_Trace(&tr, ent->client->ps.origin, ent->r.mins, ent->r.maxs,
      ent->client->ps.origin, ent->s.number, ent->clipmask, TT_AABB);
  } else {
    SV_Trace(&tr, ent->s.pos.trBase, ent->r.mins, ent->r.maxs,
      ent->s.pos.trBase, ent->s.number, ent->clipmask, TT_AABB);
  }

  if(tr.startsolid) {
    return &g_entities[tr.entityNum];
  }

  return NULL;
}

/*
============
G_TestEntAgainstOtherEnt

Tests to see if an entity collides with a another specified entityNum
============
*/
qboolean G_TestEntAgainstOtherEnt(gentity_t *self, int touchedNum) {
  trace_t tr;

  SV_ClipToEntity( &tr, self->r.currentOrigin, self->r.mins, self->r.maxs,
                   self->r.currentOrigin, touchedNum, self->clipmask, TT_AABB );


  if(tr.startsolid) {
    return qtrue;
  } else {
    return qfalse;
  }
}

/*
==================
G_Set_Push_Type

returns qtrue if the particular push needs to be evaluated
==================
*/
static qboolean G_Set_Push_Type(
  gentity_t *ent,
  gentity_t *pusher,
  mover_push_t push_type) {
  Com_Assert(ent && "G_Set_Push_Type: ent is NULL");
  Com_Assert(pusher && "G_Set_Push_Type: pusher is NULL")

  if(push_type <= pushes.type[pusher->s.number][ent->s.number]) {
    return qfalse;
  }

  pushes.type[pusher->s.number][ent->s.number] = push_type;
  return qtrue;
}

/*
==================
G_Ent_Is_Pushable_By_Pusher
==================
*/
static qboolean G_Ent_Is_Pushable_By_Pusher(gentity_t *ent, gentity_t *pusher) {
  Com_Assert(ent && "G_Ent_Is_Movable_By_Ent: ent is NULL");
  Com_Assert(pusher && "G_Ent_Is_Movable_By_Ent: pusher is NULL");

  if( ent->client && ent->health <= 0 ) {
    return qfalse;
  }

  if(
    ent->health <= 0 &&
    ent->r.contents == 0 &&
    (ent->s.eFlags & EF_NODRAW)) {
    //this ent has already gibbed
    return qfalse;
  }

  switch(pusher->s.eType) {
    case ET_PLAYER:
      if(ent->s.eType == ET_PLAYER){
        return qtrue;
      }
      break;

    case ET_BUILDABLE:
      switch (ent->s.eType) {
        case ET_PLAYER:
          return qtrue;
          break;

        case ET_BUILDABLE:
          if(BG_Buildable(pusher->s.modelindex)->stackable) {
            return qtrue;
          }
          break;

        default:
          break;
      }
      break;

    case ET_MOVER:
      if(ent->physicsObject) {
        return qtrue;
      } else {
        switch(ent->s.eType) {
          case ET_PLAYER:
          case ET_ITEM:
          case ET_BUILDABLE:
          case ET_CORPSE:
            return qtrue;

          default:
            break;
        }
      }
      break;

    default:
      break;
  }

  return qfalse;
}

typedef struct push_data_s
{
  gentity_t    *pusher;
  gentity_t    *carrier;
  vec3_t       start_origin;
  mover_push_t push_type;
  vec3_t       move;
  vec3_t       amove;
} push_data_t;

/*
==================
G_Push_Ent
==================
*/
static void  G_Push_Ent(void *data, void *user_data) {
  gentity_t   *check = (gentity_t *)data;
  push_data_t *push_data = (push_data_t *)user_data;
  vec3_t       matrix[ 3 ], transpose[ 3 ];
  vec3_t       org, org2, move2;

  Com_Assert(push_data && "G_Push_Ent: push_data is NULL");
  Com_Assert(check && "G_Push_Ent: check is NUll");
  Com_Assert(push_data->pusher && "G_Push_Ent: pusher is NULL");

  G_Entity_id_set(&pushed_p->id, check);
  VectorCopy(check->s.pos.trBase, pushed_p->origin);
  VectorCopy(check->s.apos.trBase, pushed_p->angles);

  if(check->client) {
    pushed_p->deltayaw = check->client->ps.delta_angles[ YAW ];
    VectorCopy( check->client->ps.origin, pushed_p->origin );
  }
  pushed_p++;

  // try moving the contacted entity
  // figure movement due to the pusher's amove
  BG_CreateRotationMatrix(push_data->amove, transpose);
  BG_TransposeMatrix(transpose, matrix);

  if(check->client) {
    VectorSubtract(check->client->ps.origin, push_data->pusher->r.currentOrigin, org);
  }
  else {
      VectorSubtract(check->s.pos.trBase, push_data->pusher->r.currentOrigin, org);
  }

  VectorCopy(org, org2);
  BG_RotatePoint(org2, matrix);
  VectorSubtract(org2, org, move2);
  // add movement
  VectorAdd(check->s.pos.trBase, push_data->move, check->s.pos.trBase);
  VectorAdd(check->s.pos.trBase, move2, check->s.pos.trBase);

  if(check->client) {
    VectorAdd( check->client->ps.origin, push_data->move, check->client->ps.origin );
    VectorAdd( check->client->ps.origin, move2, check->client->ps.origin );
    VectorCopy(check->client->ps.origin, check->r.currentOrigin);
    // make sure the client's view rotates when on a rotating mover
    check->client->ps.delta_angles[YAW] += ANGLE2SHORT(push_data->amove[YAW]);
  } else {
    VectorCopy(check->s.pos.trBase, check->r.currentOrigin);
    check->s.apos.trBase[YAW] += push_data->amove[YAW];
  }
  SV_LinkEntity(check);
}

/*
============
G_Pushable_Area_Ents_For_Move
============
*/
static qboolean G_Pushable_Area_Ents_For_Move(
  gentity_t   *ent,
  push_data_t *push_data,
  bgqueue_t   *ent_list) {
  qboolean    check_for_direct_block = qfalse;
  int         ent_array[MAX_GENTITIES];
  int         i, num;

  Com_Assert(ent && "G_Pushable_Area_Ents_For_Move: ent is NULL");
  Com_Assert(push_data && "G_Pushable_Area_Ents_For_Move: push_data is NULL");
  Com_Assert(ent_list && "G_Pushable_Area_Ents_For_Move: ent_list is NULL");

  // mins/maxs are the bounds at the destination
  VectorCopy(ent->r.absmin, pushes.mover_data[ent->s.number].dest_mins);
  VectorCopy(ent->r.absmax, pushes.mover_data[ent->s.number].dest_maxs);

  num = SV_AreaEntities(
          pushes.mover_data[ent->s.number].dest_mins,
          pushes.mover_data[ent->s.number].dest_maxs,
          ent_array,
          MAX_GENTITIES);
  for(i = 0; i < num; i++) {
    gentity_t *pushable = &g_entities[ent_array[i]];

    //don't push self
    if(pushable->s.number == ent->s.number) {
      continue;
    }

    //don't push back an ent that pushed this ent
    if(pushes.type[pushable->s.number][ent->s.number] > MPUSH_NONE) {
      continue;
    }

    //don't push pusher
    if(pushable->s.number == push_data->pusher->s.number) {
      continue;
    }

    //ignore dead clients
    if(pushable->client && pushable->health <= 0) {
      continue;
    }

    //ignore gibbed buildables
    if(
      pushable->health <= 0 &&
      pushable->r.contents == 0 &&
      (pushable->s.eFlags & EF_NODRAW)) {
      continue;
    }

    //don't check the world nor ENTITYNUM_NONE
    if(
      pushable->s.number == ENTITYNUM_WORLD ||
      pushable->s.number == ENTITYNUM_NONE) {
      continue;
    }

    if(!G_Ent_Is_Pushable_By_Pusher(pushable, ent)) {
      if(ent->s.eType != ET_MOVER) {
        check_for_direct_block = qtrue;
      }
      continue;
    }

    if(G_TestEntAgainstOtherEnt(pushable, ent->s.number)) {
      BG_Queue_Push_Head(ent_list, pushable);
    }
  }

  //check if there is collision against the world
  if(
    !check_for_direct_block &&
    ent->s.eType != ET_MOVER &&
    G_TestEntAgainstOtherEnt(ent, ENTITYNUM_WORLD)) {
    check_for_direct_block = qtrue;
  }

  return check_for_direct_block;
}

static void G_Find_Mover_Pushes(void *data, void *user_data );
/*
==================
G_Foreach_Find_Mover_Pushes
==================
*/
static void G_Foreach_Find_Mover_Pushes(void *data, void *user_data ) {
  push_data_t *push_data = (push_data_t *)user_data;
  gentity_t   *carrier;

  Com_Assert(push_data && "G_Find_Mover_Pushes: push_data is NULL");

  carrier = push_data->carrier;
  G_Find_Mover_Pushes(data, push_data);
  push_data->carrier = carrier;
}

/*
==================
G_Find_Mover_Pushes

Expands all potential movements to find all of the collisions
==================
*/
static void G_Find_Mover_Pushes(void *data, void *user_data ) {
  gentity_t   *check = (gentity_t *)data;
  push_data_t *push_data = (push_data_t *)user_data;
  push_data_t next_push_data;
  bgqueue_t   pushable_ents = BG_QUEUE_INIT;
  int         i;

  Com_Assert(push_data && "G_Find_Mover_Pushes: push_data is NULL");
  Com_Assert(check && "G_Find_Mover_Pushes: check is NUll");
  Com_Assert(push_data->pusher && "G_Find_Mover_Pushes: pusher is NULL");
  Com_Assert(push_data->carrier && "G_Find_Mover_Pushes: carrier is NULL");

  //mover displacement is handled in G_MoverPush() and evaluates move and amove
  if(check->s.eType != ET_MOVER) {
    // EF_MS_STOP will just stop when contacting another entity
    // instead of pushing it, but entities can still ride on top of it
    if(
      (push_data->pusher->s.eFlags & EF_MOVER_STOP) &&
      G_Get_Foundation_Ent_Num(check) != push_data->pusher->s.number) {
      G_Set_Push_Type(check, push_data->carrier, push_data->push_type);
      pushes.mover_data[check->s.number].check_for_direct_block = qtrue;
      return;
    }

    // too much pushing
    if(pushed_p > &pushed[ MAX_PUSHES ]) {
      return;
    }

    if(!G_Set_Push_Type(check, push_data->carrier, push_data->push_type)) {
      return;
    }
    pushes.mover_data[check->s.number].checked_for_push = qtrue;

    //move the entity
    G_Push_Ent(check, push_data);
  }

  //find all collided pushable entities
  if(
    G_Pushable_Area_Ents_For_Move(check, push_data, &pushable_ents) &&
    check->s.eType != ET_MOVER) {
    pushes.mover_data[check->s.number].check_for_direct_block = qtrue;
  }

  //check any and all collided pushable entities
  if(pushable_ents.length) {
    next_push_data.pusher = push_data->pusher;
    if(push_data->push_type == MPUSH_RIDE) {
      next_push_data.push_type =  MPUSH_RIDING_STACK_HIT;
    } else {
      next_push_data.push_type =  push_data->push_type;
    }
    next_push_data.carrier = check;
    VectorCopy(push_data->amove, next_push_data.amove);
    VectorCopy(push_data->move, next_push_data.move);

    BG_Queue_Foreach(&pushable_ents, G_Foreach_Find_Mover_Pushes, &next_push_data);
    BG_Queue_Clear(&pushable_ents);
  }

  //check riding entities that have not collided from the push
  for(i = 0; i < ENTITYNUM_MAX_NORMAL; i++) {
    gentity_t *rider = &g_entities[i];

    if(rider->s.groundEntityNum != check->s.number) {
      continue;
    }

    //don't check riders that have already been checked
    
    if(pushes.type[check->s.number][rider->s.number] > MPUSH_NONE) {
      continue;
    }

    //don't check self as a rider
    if(rider->s.number == check->s.number) {
      continue;
    }

    //entities that pushed check are not riders
    if(pushes.type[rider->s.number][check->s.number] > MPUSH_NONE) {
      continue;
    }

    //pushers don't ride
    if(rider->s.number == push_data->pusher->s.number) {
      continue;
    }

    if(!G_Ent_Is_Pushable_By_Pusher(rider, check)) {
      continue;
    }

    next_push_data.pusher = push_data->pusher;
    next_push_data.push_type = MPUSH_RIDE;
    next_push_data.carrier = check;
    VectorCopy(push_data->amove, next_push_data.amove);
    VectorCopy(push_data->move, next_push_data.move);
    G_Find_Mover_Pushes(rider, &next_push_data);
  }

  if(check->s.eType != ET_MOVER) {
    //move check back
    VectorCopy((pushed_p - 1)->origin, check->s.pos.trBase);
    VectorCopy((pushed_p - 1)->origin, check->r.currentOrigin);
    SV_LinkEntity(check);

    if(check->client) {
      check->client->ps.delta_angles[YAW] = ( pushed_p - 1 )->deltayaw;
      VectorCopy((pushed_p - 1)->origin, check->client->ps.origin);
    }

    VectorCopy((pushed_p - 1)->angles, check->s.apos.trBase);

    pushed_p--;
  }
}

/*
==================
G_All_Collisions_Are_Cleared

determines if no other entity tried pushing this entity
==================
*/
static qboolean G_All_Collisions_Are_Cleared(gentity_t *ent) {
  int i;

  Com_Assert(ent && "G_All_Collisions_Are_Cleared: ent is NULL");

  for(i = 0; i < ENTITYNUM_MAX_NORMAL; i++) {
    if(pushes.type[i][ent->s.number] != MPUSH_NONE) {
      return qfalse;
    }
  }

  return qtrue;
}

/*
==================
G_Clear_Riding_Stack_Collisions

Clears a riding stack
==================
*/
static void G_Clear_Riding_Stack_Collisions(gentity_t *ent) {
  int i;

  Com_Assert(ent && "G_Clear_Riding_Stack_Collisions: ent is NULL");

  if(!G_All_Collisions_Are_Cleared(ent)) {
    return;
  }

  pushes.mover_data[ent->s.number].checked_for_push = qfalse;
  pushes.mover_data[ent->s.number].check_for_direct_block = qfalse;

  for(i = 0; i < ENTITYNUM_MAX_NORMAL; i++) {
    gentity_t *stack_rider = &g_entities[i];

    if(
      pushes.type[ent->s.number][i] != MPUSH_RIDING_STACK_HIT ||
      pushes.type[ent->s.number][i] != MPUSH_RIDE) {
      continue;
    }

    //clear the collision
    pushes.type[ent->s.number][i] = MPUSH_NONE;
    pushes.type[ent->s.number][stack_rider->s.number] = MPUSH_NONE;
    G_Clear_Riding_Stack_Collisions(stack_rider);
  }
}

/*
==================
G_Clear_Rider_Collisions

Clears collisions with any direct riders, and then handles any resulting stacks of those riders
==================
*/
static void G_Clear_Rider_Collisions(gentity_t *ent) {
  int i;

  Com_Assert(ent && "G_Clear_Rider_Collisions: ent is NULL");

  for(i = 0; i < ENTITYNUM_MAX_NORMAL; i++) {
    gentity_t *rider = &g_entities[i];

    if(pushes.type[ent->s.number][i] != MPUSH_RIDE) {
      continue;
    }

    //clear the collision
    pushes.type[ent->s.number][i] = MPUSH_NONE;
    pushes.type[ent->s.number][rider->s.number] = MPUSH_NONE;
    G_Clear_Riding_Stack_Collisions(rider);
  }
}

/*
==================
G_Block_Stack

recursively block a pusher collision stack
==================
*/
static void G_Block_Stack(gentity_t *ent) {
  int i;
  Com_Assert(ent && "G_Block_Stack: ent is NULL");

  if(pushes.mover_data[ent->s.number].blocked == MBLCK_NONE) {
    pushes.mover_data[ent->s.number].blocked = MBLCK_INDIRECT;

    G_Clear_Rider_Collisions(ent);

    for(i = 0; i < MAX_GENTITIES; i++) {
      if(pushes.type[i][ent->s.number] != MPUSH_RIDING_STACK_HIT) {
        continue;
      }

      G_Block_Stack(&g_entities[i]);
    }
  }
}

/*
==================
G_Ent_Is_Directly_Blocked
==================
*/
static qboolean G_Entity_Is_Directly_Blocked(
  gentity_t    *ent,
  push_data_t  *push_data) {
  int           ent_array[MAX_GENTITIES];
  int           i, num;

  Com_Assert(ent && "G_Entity_Is_Directly_Blocked: ent is NULL");
  Com_Assert(push_data && "G_Entity_Is_Directly_Blocked: push_data is NULL");

  //check if there is collision against the world
  if(G_TestEntAgainstOtherEnt(ent, ENTITYNUM_WORLD)) {
    return qtrue;
  }

  num = SV_AreaEntities(
          pushes.mover_data[ent->s.number].dest_mins,
          pushes.mover_data[ent->s.number].dest_maxs,
          ent_array,
          MAX_GENTITIES);
  for(i = 0; i < num; i++) {
    gentity_t *block = &g_entities[ent_array[i]];

    //self doesn't block
    if(block->s.number == ent->s.number) {
      continue;
    }

    //not blocked by the pusher
    if(block->s.number == push_data->pusher->s.number) {
      continue;
    }

    //ignore dead clients
    if(block->client && block->health <= 0) {
      continue;
    }

    //ignore gibbed buildables
    if(
      block->health <= 0 &&
      block->r.contents == 0 &&
      (block->s.eFlags & EF_NODRAW)) {
      continue;
    }

    //don't check the world nor ENTITYNUM_NONE
    if(
      block->s.number == ENTITYNUM_WORLD ||
      block->s.number == ENTITYNUM_NONE) {
      continue;
    }

    if(G_TestEntAgainstOtherEnt(ent, block->s.number)) {
      return qtrue;
    }
  }

  //not directly blocked by anything
  return qfalse;
}

/*
==================
G_Find_Mover_Blockage

returns qtrue if an obstacle blocks the prime mover
==================
*/
static qboolean G_Find_Mover_Blockage(
  push_data_t *push_data,
  bgqueue_t   *obstacles) {
  gentity_t   *check;
  pushed_t    *saved_pushed = pushed_p;
  qboolean    block_prime_mover = qfalse;
  qboolean    instagib = qfalse;
  int         i;

  Com_Assert(push_data && "G_Find_Mover_Blockage: push_data is NULL");

  //temporarily expand all the pushes for evaluating blocking
  for(i = 0; i < MAX_GENTITIES; i++) {
    check = &g_entities[i];

    // too much pushing
    if(pushed_p > &pushed[ MAX_PUSHES ]) {
      return qtrue;
    }

    if(pushes.mover_data[check->s.number].checked_for_push) {
      G_Push_Ent(check, push_data);
    }
  }

  //confirm direct blocks
  for(i = 0; i < MAX_GENTITIES; i++) {
    check = &g_entities[i];

    if(!pushes.mover_data[check->s.number].check_for_direct_block) {
      continue;
    }

    if(G_Entity_Is_Directly_Blocked(check, push_data)) {
      pushes.mover_data[check->s.number].blocked = MBLCK_DIRECT;
    }

    pushes.mover_data[check->s.number].check_for_direct_block = qfalse;
  }

  //propogate from direct blocking through collisions
  for(i = 0; i < MAX_GENTITIES; i++) {
    int j;

    check = &g_entities[i];

    if(pushes.mover_data[check->s.number].blocked != MBLCK_DIRECT) {
      continue;
    }

    G_Clear_Rider_Collisions(check);

    for(j = 0; j < MAX_GENTITIES; j++) {
      if(pushes.type[j][check->s.number] < MPUSH_RIDING_STACK_HIT) {
        continue;
      }

      if(pushes.type[j][check->s.number] == MPUSH_PRIME_MOVER_HIT) {
        // bobbing entities are instant-kill and never get blocked
        if(
          push_data->pusher->s.pos.trType == TR_SINE ||
          push_data->pusher->s.apos.trType == TR_SINE ) {
          instagib = qtrue;
          continue;
        }

        BG_Queue_Push_Head(obstacles, check);
        block_prime_mover = qtrue;
        continue;
      }

      G_Block_Stack(&g_entities[j]);
    }

    if(instagib) {
      G_Damage(
        check,
        push_data->pusher, push_data->pusher,
        NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_CRUSH);
      continue;
    }
  }

  //collapse the pushes back
  if(pushed_p > saved_pushed) {
    for(pushed_p--; pushed_p >= saved_pushed; pushed_p--) {
      gentity_t *ent = G_Entity_id_get(&pushed_p->id);

      if(ent) {
        VectorCopy(pushed_p->origin, ent->r.currentOrigin);
        VectorCopy(pushed_p->origin, ent->s.pos.trBase);
        VectorCopy(pushed_p->angles, ent->s.apos.trBase);

        if( ent->client) {
          ent->client->ps.delta_angles[YAW] = pushed_p->deltayaw;
          VectorCopy(pushed_p->origin, ent->client->ps.origin);
        }

        SV_LinkEntity(ent);
      }

      if(pushed_p == saved_pushed) {
        //don't decrement below saved_pushed
        break;
      }
    }
  }

  return block_prime_mover;
}

/*
==================
G_Push_Unblocked_Ents
==================
*/
static void G_Push_Unblocked_Ents(push_data_t *push_data) {
  gentity_t   *check;
  int i;

  Com_Assert(push_data && "G_Push_Unblocked_Ents: push_data is NULL");

  for(i = 0; i < MAX_GENTITIES; i++) {
    check = &g_entities[i];

    if(!pushes.mover_data[check->s.number].checked_for_push) {
      continue;
    }

    //don't push blocked stacks that are riding
    if(pushes.mover_data[check->s.number].blocked != MBLCK_NONE) {
      if(
        check->s.groundEntityNum != ENTITYNUM_NONE &&
        (
          check->s.groundEntityNum == push_data->pusher->s.number ||
          pushes.type[check->s.groundEntityNum][check->s.number] == MPUSH_NONE ||
          pushes.mover_data[check->s.groundEntityNum].blocked != MBLCK_NONE)) {
        //blocked entities slide
        check->s.groundEntityNum = ENTITYNUM_NONE;
      }
      continue;
    }

    G_Push_Ent(check, push_data);
    G_Set_Pusher_Num(check, push_data->pusher->s.number);

    // may have pushed them off an edge
    if(
      check->s.groundEntityNum != ENTITYNUM_NONE &&
      check->s.groundEntityNum != push_data->pusher->s.number &&
      (
        !pushes.mover_data[check->s.groundEntityNum].checked_for_push ||
        pushes.mover_data[check->s.groundEntityNum].blocked != MBLCK_NONE)) {
      check->s.groundEntityNum = ENTITYNUM_NONE;
    }

  }
}

/*
==================
G_TryPushingEntities

Returns qfalse if the move is blocked
==================
*/
static qboolean G_TryPushingEntities(
  gentity_t    *check,
  gentity_t    *pusher,
  vec3_t       start_origin,
  vec3_t       start_angles,
  vec3_t       move,
  vec3_t       amove,
  bgqueue_t    *obstacles) {
  push_data_t  push_data;
  int          i;

  Com_Assert(check && "G_TryPushingEntities: check is NULL");
  Com_Assert(pusher && "G_TryPushingEntities: pusher is NULL");
  Com_Assert(obstacles && "G_TryPushingEntities: obstacles is NULL");

  //initialize this prime pusher of the move
  memset(&pushes, 0, sizeof(pushes_t));
  for( i = 0; i < MAX_GENTITIES; i++ ) {
    VectorCopy(g_entities[i].r.currentOrigin, pushes.mover_data[i].start_origin);
    VectorCopy(g_entities[i].r.currentAngles, pushes.mover_data[i].start_angles);
  }
  push_data.pusher = push_data.carrier = pusher;
  push_data.push_type = MPUSH_PRIME_MOVER_HIT;
  VectorCopy(start_origin, push_data.start_origin);
  VectorCopy(start_origin, pushes.mover_data[push_data.pusher->s.number].start_origin);
  VectorCopy(start_angles, pushes.mover_data[push_data.pusher->s.number].start_angles);
  VectorCopy(move, push_data.move);
  VectorCopy(amove, push_data.amove);

  //Recursively find all push collisions
  G_Find_Mover_Pushes(check, &push_data);

  //evaluate blocking
  if( G_Find_Mover_Blockage(&push_data, obstacles) ) {
    return qfalse;
  }

  //ready to follow through on all relevent pushing
  G_Push_Unblocked_Ents(&push_data);

  return qtrue;
}

/*
============
G_MoverPush

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
If qfalse is returned, *obstacle will be the blocking entity
============
*/
qboolean G_MoverPush(
  gentity_t *pusher,
  vec3_t    start_origin,
  vec3_t    start_angles,
  vec3_t    move,
  vec3_t    amove,
  bgqueue_t *obstacles) {
  pushed_t  *p;

  // move the prime pusher to its final position
  VectorAdd(pusher->r.currentOrigin, move, pusher->r.currentOrigin);
  VectorAdd(pusher->r.currentAngles, amove, pusher->r.currentAngles);
  SV_LinkEntity(pusher);

  //try pushing contacted entities and entity stacks
  if(
    !G_TryPushingEntities(
      pusher,
      pusher,
      start_origin,
      start_angles,
      move,
      amove,
      obstacles)) {
    // the entire move was blocked by an entity

    // move back any entities we already moved
    // go backwards, so if the same entity was pushed
    // twice, it goes back to the original position
    for(p = pushed_p - 1; p >= pushed; p--) {
      gentity_t *ent = G_Entity_id_get(&pushed_p->id);

      VectorCopy(p->origin, ent->s.pos.trBase);
      VectorCopy(p->angles, ent->s.apos.trBase);

      if(ent->client) {
       ent->client->ps.delta_angles[ YAW ] = p->deltayaw;
        VectorCopy(p->origin, ent->client->ps.origin);
      }

      SV_LinkEntity(ent);
    }

    return qfalse;
  }
  return qtrue;
}

void G_Foreach_Blocked(void *data, void *user_data) {
  gentity_t *block = (gentity_t *)data;
  gentity_t  *pusher = (gentity_t *)user_data;

  Com_Assert(block && "G_Foreach_Blocked: block is NULL");
  Com_Assert(pusher && "G_Foreach_Blocked: pusher is NULL");
  Com_Assert(pusher->blocked && "G_Foreach_Blocked: pusher->blocked is NULL");

  pusher->blocked(pusher, block);
}

/*
=================
G_MoverTeam
=================
*/
void G_MoverTeam(gentity_t *ent) {
  vec3_t    move, amove;
  gentity_t *part;
  vec3_t    origin, angles;
  bgqueue_t obstacles = BG_QUEUE_INIT;
  qboolean  move_blocked = qfalse;

  // make sure all team slaves can move before commiting
  // any moves or calling any think functions
  // if the move is blocked, all moved objects will be backed out
  pushed_p = pushed;
  for(part = ent; part; part = part->teamchain) {
    if(
      part->s.pos.trType == TR_STATIONARY &&
      part->s.apos.trType == TR_STATIONARY) {
      continue;
    }

    // get current position
    BG_EvaluateTrajectory(&part->s.pos, level.time, origin);
    BG_EvaluateTrajectory(&part->s.apos, level.time, angles);
    VectorSubtract(origin, part->r.currentOrigin, move);
    VectorSubtract(angles, part->r.currentAngles, amove);
    if(
      !G_MoverPush(
        part,
        part->r.currentOrigin,
        part->r.currentAngles,
        move,
        amove,
        &obstacles)) {
      move_blocked = qtrue;
    }
  }

  if(move_blocked) {
    // go back to the previous position
    for(part = ent; part; part = part->teamchain) {
      if(
        part->s.pos.trType == TR_STATIONARY &&
        part->s.apos.trType == TR_STATIONARY) {
        continue;
      }

      part->s.pos.trTime += level.time - level.previousTime;
      part->s.apos.trTime += level.time - level.previousTime;
      BG_EvaluateTrajectory( &part->s.pos, level.time, part->r.currentOrigin );
      BG_EvaluateTrajectory( &part->s.apos, level.time, part->r.currentAngles );
      SV_LinkEntity( part );
    }

    // if the pusher has a "blocked" function, call it
    if(ent->blocked) {
      BG_Queue_Foreach(&obstacles, G_Foreach_Blocked, ent);
    }

    BG_Queue_Clear(&obstacles);

    return;
  }

  // the move succeeded
  for(part = ent; part; part = part->teamchain) {
    // call the reached function if time is at or past end point
    if(part->s.pos.trType == TR_LINEAR_STOP) {
      if(level.time >= part->s.pos.trTime + part->s.pos.trDuration) {
        if(part->reached) {
          part->reached(part);
        }
      }
    }
    if(part->s.apos.trType == TR_LINEAR_STOP) {
      if(level.time >= part->s.apos.trTime + part->s.apos.trDuration) {
        if(part->reached) {
          part->reached(part);
        }
      }
    }
  }
}



/*
================
G_ResetPusherNum

================
*/
void G_ResetPusherNum(void) {
  int i;

  for(i = 0;i < MAX_GENTITIES; i++) {
    gentity_t *ent = &g_entities[i];

    if(!ent->inuse) {
      continue;
    }

    G_Set_Pusher_Num(ent, ENTITYNUM_NONE);
  }
}

/*
================
G_RunMover

================
*/
void G_RunMover( gentity_t *ent )
{
  // if not a team captain, don't do anything, because
  // the captain will handle everything
  if( ent->flags & FL_TEAMSLAVE )
    return;

  G_MoverTeam( ent );

  // check think function
  G_RunThink( ent );
}

/*
============================================================================

GENERAL MOVERS

Doors, plats, and buttons are all binary (two position) movers
Pos1 is "at rest", pos2 is "activated"
============================================================================
*/

/*
===============
SetMoverState
===============
*/
void SetMoverState(gentity_t *ent, moverState_t moverState, int time) {
  vec3_t delta;
  float  f;

  ent->moverState = moverState;

  ent->s.pos.trTime = time;
  ent->s.apos.trTime = time;

  switch(ent->moverMotionType) {
    case MM_LINEAR:
      switch(moverState) {
        case MS_POS1:
          VectorCopy( ent->pos1, ent->s.pos.trBase );
          ent->s.pos.trType = TR_STATIONARY;
          break;

        case MS_POS2:
          VectorCopy( ent->pos2, ent->s.pos.trBase );
          ent->s.pos.trType = TR_STATIONARY;
          break;

        case MS_1TO2:
          VectorCopy( ent->pos1, ent->s.pos.trBase );
          VectorSubtract( ent->pos2, ent->pos1, delta );
          f = 1000.0 / ent->s.pos.trDuration;
          VectorScale( delta, f, ent->s.pos.trDelta );
          ent->s.pos.trType = TR_LINEAR_STOP;
          break;

        case MS_2TO1:
          VectorCopy( ent->pos2, ent->s.pos.trBase );
          VectorSubtract( ent->pos1, ent->pos2, delta );
          f = 1000.0 / ent->s.pos.trDuration;
          VectorScale( delta, f, ent->s.pos.trDelta );
          ent->s.pos.trType = TR_LINEAR_STOP;
          break;
      }
      BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->r.currentOrigin );
      break;

    case MM_ROTATION:
      switch(moverState) {
        case MS_POS1:
          VectorCopy( ent->pos1, ent->s.apos.trBase );
          ent->s.apos.trType = TR_STATIONARY;
          break;

        case MS_POS2:
          VectorCopy( ent->pos2, ent->s.apos.trBase );
          ent->s.apos.trType = TR_STATIONARY;
          break;

        case MS_1TO2:
          VectorCopy( ent->pos1, ent->s.apos.trBase );
          VectorSubtract( ent->pos2, ent->pos1, delta );
          f = 1000.0 / ent->s.apos.trDuration;
          VectorScale( delta, f, ent->s.apos.trDelta );
          ent->s.apos.trType = TR_LINEAR_STOP;
          break;

        case MS_2TO1:
          VectorCopy( ent->pos2, ent->s.apos.trBase );
          VectorSubtract( ent->pos1, ent->pos2, delta );
          f = 1000.0 / ent->s.apos.trDuration;
          VectorScale( delta, f, ent->s.apos.trDelta );
          ent->s.apos.trType = TR_LINEAR_STOP;
          break;
      }
      BG_EvaluateTrajectory( &ent->s.apos, level.time, ent->r.currentAngles );
      break;

    case MM_MODEL:
      break;
  }

  SV_LinkEntity( ent );
}

/*
================
MatchTeam

All entities in a mover team will move from pos1 to pos2
================
*/
void MatchTeam(gentity_t *teamLeader, moverState_t moverState, int time) {
  gentity_t   *slave;

  for(slave = teamLeader; slave; slave = slave->teamchain) {
    SetMoverState(slave, moverState, time);
  }
}


/*
================
MasterOf
================
*/
gentity_t *MasterOf( gentity_t *ent )
{
  if( ent->teammaster )
    return ent->teammaster;
  else
    return ent;
}


/*
================
GetMoverTeamState

Returns a moverState value representing the phase (either one
 of pos1, 1to2, pos2, or 2to1) of a mover team as a whole.
================
*/
moverState_t GetMoverTeamState(gentity_t *ent) {
  qboolean pos1 = qfalse;

  for(ent = MasterOf( ent ); ent; ent = ent->teamchain) {
    if(ent->moverState == MS_POS1) {
      pos1 = qtrue;
    } else if(ent->moverState == MS_1TO2) {
      return MS_1TO2;
    } else if(ent->moverState == MS_2TO1) {
      return MS_2TO1;
    }
  }

  if(pos1) {
    return MS_POS1;
  } else {
    return MS_POS2;
  }
}


/*
================
ReturnToPos1orApos1

Used only by a master movers.
================
*/

void Use_BinaryMover( gentity_t *ent, gentity_t *other, gentity_t *activator );

void ReturnToPos1orApos1( gentity_t *ent )
{
  if( GetMoverTeamState( ent ) != MS_POS2 )
    return; // not every mover in the team has reached its endpoint yet

  Use_BinaryMover( ent, ent, ent->activator );
}


/*
================
Think_ClosedModelDoor
================
*/
void Think_ClosedModelDoor( gentity_t *ent )
{
  // play sound
  if( ent->soundPos1 )
    G_AddEvent( ent, EV_GENERAL_SOUND, ent->soundPos1 );

  // close areaportals
  if( ent->teammaster == ent || !ent->teammaster )
    SV_AdjustAreaPortalState( ent, qfalse );

  ent->moverState = MS_POS1;
}


/*
================
Think_CloseModelDoor
================
*/
void Think_CloseModelDoor( gentity_t *ent )
{
  int       entityList[ MAX_GENTITIES ];
  int       numEntities, i;
  gentity_t *clipBrush = ent->clipBrush;
  gentity_t *check;
  qboolean  canClose = qtrue;

  numEntities = SV_AreaEntities( clipBrush->r.absmin, clipBrush->r.absmax, entityList, MAX_GENTITIES );

  //set brush solid
  SV_LinkEntity( ent->clipBrush );

  //see if any solid entities are inside the door
  for( i = 0; i < numEntities; i++ )
  {
    check = &g_entities[ entityList[ i ] ];

    //only test items and players
    if( check->s.eType != ET_ITEM && check->s.eType != ET_BUILDABLE &&
        check->s.eType != ET_CORPSE && check->s.eType != ET_PLAYER &&
        !check->physicsObject )
      continue;

    //test is this entity collides with this door
    if( G_TestEntityPosition( check ) )
      canClose = qfalse;
  }

  //something is blocking this door
  if( !canClose )
  {
    //set brush non-solid
    SV_UnlinkEntity( ent->clipBrush );

    ent->nextthink = level.time + ent->wait;
    return;
  }

  //toggle door state
  ent->s.legsAnim = qfalse;

  // play sound
  if( ent->sound2to1 )
    G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound2to1 );

  ent->moverState = MS_2TO1;

  ent->think = Think_ClosedModelDoor;
  ent->nextthink = level.time + ent->speed;
}


/*
================
Think_OpenModelDoor
================
*/
void Think_OpenModelDoor( gentity_t *ent )
{
  //set brush non-solid
  SV_UnlinkEntity( ent->clipBrush );

  // stop the looping sound
  ent->s.loopSound = 0;

  // play sound
  if( ent->soundPos2 )
    G_AddEvent( ent, EV_GENERAL_SOUND, ent->soundPos2 );

  ent->moverState = MS_POS2;

  // return to pos1 after a delay
  ent->think = Think_CloseModelDoor;
  ent->nextthink = level.time + ent->wait;

  // fire targets
  if( !ent->activator )
    ent->activator = ent;

  G_UseTargets( ent, ent->activator );
}


/*
================
Reached_BinaryMover
================
*/
void Reached_BinaryMover( gentity_t *ent )
{
  gentity_t *master = MasterOf( ent );

  // stop the looping sound
  ent->s.loopSound = 0;

  if( ent->moverState == MS_1TO2 ) {
    // reached pos2
    SetMoverState( ent, MS_POS2, level.time );

    // play sound
    if( ent->soundPos2 )
      G_AddEvent( ent, EV_GENERAL_SOUND, ent->soundPos2 );

    // return to pos1 after a delay
    master->think = ReturnToPos1orApos1;
    master->nextthink = MAX( master->nextthink, level.time + ent->wait );

    // fire targets
    if( !ent->activator )
      ent->activator = ent;

    G_UseTargets( ent, ent->activator );
  }
  else if( ent->moverState == MS_2TO1 )
  {
    // reached pos1
    SetMoverState( ent, MS_POS1, level.time );

    // play sound
    if( ent->soundPos1 )
      G_AddEvent( ent, EV_GENERAL_SOUND, ent->soundPos1 );

    // close areaportals
    if( ent->teammaster == ent || !ent->teammaster )
      SV_AdjustAreaPortalState( ent, qfalse );
  }
  else
    Com_Error( ERR_DROP, "Reached_BinaryMover: bad moverState" );
}


/*
================
Use_BinaryMover
================
*/
void Use_BinaryMover(gentity_t *ent, gentity_t *other, gentity_t *activator)
{
  int   total;
  int   partial;
  gentity_t *master;
  moverState_t teamState;

  // if this is a non-client-usable door return
  if( ent->targetname && other && other->client )
    return;

  // only the master should be used
  if( ent->flags & FL_TEAMSLAVE )
  {
    Use_BinaryMover( ent->teammaster, other, activator );
    return;
  }

  ent->activator = activator;

  master = MasterOf( ent );
  teamState = GetMoverTeamState( ent );

  for( ent = master; ent; ent = ent->teamchain )
  {
    //ind
    switch (ent->moverMotionType) {
      case MM_LINEAR:
        if( ent->moverState == MS_POS1 )
        {
          // start moving 50 msec later, becase if this was player
          // triggered, level.time hasn't been advanced yet
          SetMoverState( ent, MS_1TO2, level.time + 50 );

          // starting sound
          if( ent->sound1to2 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound1to2 );

          // looping sound
          ent->s.loopSound = ent->soundLoop;

          // open areaportal
          if( ent->teammaster == ent || !ent->teammaster )
            SV_AdjustAreaPortalState( ent, qtrue );
        }
        else if( ent->moverState == MS_POS2 &&
                 !( teamState == MS_1TO2 || other == master ) )
        {
          // if all the way up, just delay before coming down
          master->think = ReturnToPos1orApos1;
          master->nextthink = MAX( master->nextthink, level.time + ent->wait );
        }
        else if( ent->moverState == MS_POS2 &&
                 ( teamState == MS_1TO2 || other == master ) )
        {
          // start moving 50 msec later, becase if this was player
          // triggered, level.time hasn't been advanced yet
          SetMoverState( ent, MS_2TO1, level.time + 50 );

          // starting sound
          if( ent->sound2to1 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound2to1 );

          // looping sound
          ent->s.loopSound = ent->soundLoop;

          // open areaportal
          if( ent->teammaster == ent || !ent->teammaster )
            SV_AdjustAreaPortalState( ent, qtrue );
        }
        else if( ent->moverState == MS_2TO1 )
        {
          // only partway down before reversing
          total = ent->s.pos.trDuration;
          partial = level.time - ent->s.pos.trTime;

          if( partial > total )
            partial = total;

          SetMoverState( ent, MS_1TO2, level.time - ( total - partial ) );

          if( ent->sound1to2 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound1to2 );
        }
        else if( ent->moverState == MS_1TO2 )
        {
          // only partway up before reversing
          total = ent->s.pos.trDuration;
          partial = level.time - ent->s.pos.trTime;

          if( partial > total )
            partial = total;

          SetMoverState( ent, MS_2TO1, level.time - ( total - partial ) );

          if( ent->sound2to1 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound2to1 );
        }
        break;

      case MM_ROTATION:
        if( ent->moverState == MS_POS1 )
        {
          // start moving 50 msec later, becase if this was player
          // triggered, level.time hasn't been advanced yet
          SetMoverState( ent, MS_1TO2, level.time + 50 );

          // starting sound
          if( ent->sound1to2 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound1to2 );

          // looping sound
          ent->s.loopSound = ent->soundLoop;

          // open areaportal
          if( ent->teammaster == ent || !ent->teammaster )
            SV_AdjustAreaPortalState( ent, qtrue );
        }
        else if( ent->moverState == MS_POS2 &&
                 !( teamState == MS_1TO2 || other == master ) )
        {
          // if all the way up, just delay before coming down
          master->think = ReturnToPos1orApos1;
          master->nextthink = MAX( master->nextthink, level.time + ent->wait );
        }
        else if( ent->moverState == MS_POS2 &&
                 ( teamState == MS_1TO2 || other == master ) )
        {
          // start moving 50 msec later, becase if this was player
          // triggered, level.time hasn't been advanced yet
          SetMoverState( ent, MS_2TO1, level.time + 50 );

          // starting sound
          if( ent->sound2to1 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound2to1 );

          // looping sound
          ent->s.loopSound = ent->soundLoop;

          // open areaportal
          if( ent->teammaster == ent || !ent->teammaster )
            SV_AdjustAreaPortalState( ent, qtrue );
        }
        else if( ent->moverState == MS_2TO1 )
        {
          // only partway down before reversing
          total = ent->s.apos.trDuration;
          partial = level.time - ent->s.apos.trTime;

          if( partial > total )
            partial = total;

          SetMoverState( ent, MS_1TO2, level.time - ( total - partial ) );

          if( ent->sound1to2 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound1to2 );
        }
        else if( ent->moverState == MS_1TO2 )
        {
          // only partway up before reversing
          total = ent->s.apos.trDuration;
          partial = level.time - ent->s.apos.trTime;

          if( partial > total )
            partial = total;

          SetMoverState( ent, MS_2TO1, level.time - ( total - partial ) );

          if( ent->sound2to1 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound2to1 );
        }
        break;

      case MM_MODEL:
        if( ent->moverState == MS_POS1 )
        {
          //toggle door state
          ent->s.legsAnim = qtrue;

          ent->think = Think_OpenModelDoor;
          ent->nextthink = level.time + ent->speed;

          // starting sound
          if( ent->sound1to2 )
            G_AddEvent( ent, EV_GENERAL_SOUND, ent->sound1to2 );

          // looping sound
          ent->s.loopSound = ent->soundLoop;

          // open areaportal
          if( ent->teammaster == ent || !ent->teammaster )
            SV_AdjustAreaPortalState( ent, qtrue );

          ent->moverState = MS_1TO2;
        }
        else if( ent->moverState == MS_POS2 )
        {
          // if all the way up, just delay before coming down
          ent->nextthink = level.time + ent->wait;
        }
        break;
    }
  //outd
  }
}



/*
================
InitMover

"pos1", "pos2", and "speed" should be set before calling,
so the movement delta can be calculated
================
*/
void InitMover( gentity_t *ent )
{
  vec3_t    move;
  float     distance;
  float     light;
  vec3_t    color;
  qboolean  lightSet, colorSet;
  char      *sound;
  char      *team;

  // if the "model2" key is set, use a seperate model
  // for drawing, but clip against the brushes
  if( ent->model2 )
    ent->s.modelindex2 = G_ModelIndex( ent->model2 );

  // if the "noise" key is set, use a constant looping sound when moving
  if( G_SpawnString( "noise", "", &sound ) )
    ent->soundLoop = G_SoundIndex( sound );

  // if the "color" or "light" keys are set, setup constantLight
  lightSet = G_SpawnFloat( "light", "100", &light );
  colorSet = G_SpawnVector( "color", "1 1 1", color );

  if( lightSet || colorSet )
  {
    int   r, g, b, i;

    r = color[ 0 ] * 255;
    if( r > 255 )
      r = 255;

    g = color[ 1 ] * 255;
    if( g > 255 )
      g = 255;

    b = color[ 2 ] * 255;
    if( b > 255 )
      b = 255;

    i = light / 4;
    if( i > 255 )
      i = 255;

    ent->s.constantLight = r | ( g << 8 ) | ( b << 16 ) | ( i << 24 );
  }


  ent->use = Use_BinaryMover;
  ent->reached = Reached_BinaryMover;

  if( G_SpawnString( "team", "", &team ) )
    ent->team = G_CopyString( team );

  ent->moverState = MS_POS1;
  ent->moverMotionType = MM_LINEAR;
  ent->s.eType = ET_MOVER;
  VectorCopy( ent->pos1, ent->r.currentOrigin );
  SV_LinkEntity( ent );

  ent->s.pos.trType = TR_STATIONARY;
  VectorCopy( ent->pos1, ent->s.pos.trBase );

  // calculate time to reach second position from speed
  VectorSubtract( ent->pos2, ent->pos1, move );
  distance = VectorLength( move );
  if( !ent->speed )
    ent->speed = 100;

  VectorScale( move, ent->speed, ent->s.pos.trDelta );
  ent->s.pos.trDuration = distance * 1000 / ent->speed;

  if( ent->s.pos.trDuration <= 0 )
    ent->s.pos.trDuration = 1;
}


/*
================
InitRotator

"pos1", "pos2", and "speed" should be set before calling,
so the movement delta can be calculated
================
*/
void InitRotator( gentity_t *ent )
{
  vec3_t    move;
  float     angle;
  float     light;
  vec3_t    color;
  qboolean  lightSet, colorSet;
  char      *sound;
  char      *team;

  // if the "model2" key is set, use a seperate model
  // for drawing, but clip against the brushes
  if( ent->model2 )
    ent->s.modelindex2 = G_ModelIndex( ent->model2 );

  // if the "noise" key is set, use a constant looping sound when moving
  if( G_SpawnString( "noise", "", &sound ) )
    ent->soundLoop = G_SoundIndex( sound );

  // if the "color" or "light" keys are set, setup constantLight
  lightSet = G_SpawnFloat( "light", "100", &light );
  colorSet = G_SpawnVector( "color", "1 1 1", color );

  if( lightSet || colorSet )
  {
    int   r, g, b, i;

    r = color[ 0 ] * 255;

    if( r > 255 )
      r = 255;

    g = color[ 1 ] * 255;

    if( g > 255 )
      g = 255;

    b = color[ 2 ] * 255;

    if( b > 255 )
      b = 255;

    i = light / 4;

    if( i > 255 )
      i = 255;

    ent->s.constantLight = r | ( g << 8 ) | ( b << 16 ) | ( i << 24 );
  }


  ent->use = Use_BinaryMover;
  ent->reached = Reached_BinaryMover;

  if( G_SpawnString( "team", "", &team ) )
    ent->team = G_CopyString( team );

  ent->moverState = MS_POS1;
  ent->moverMotionType = MM_ROTATION;
  ent->s.eType = ET_MOVER;
  VectorCopy( ent->pos1, ent->r.currentAngles );
  SV_LinkEntity( ent );

  ent->s.apos.trType = TR_STATIONARY;
  VectorCopy( ent->pos1, ent->s.apos.trBase );

  // calculate time to reach second position from speed
  VectorSubtract( ent->pos2, ent->pos1, move );
  angle = VectorLength( move );

  if( !ent->speed )
    ent->speed = 120;

  VectorScale( move, ent->speed, ent->s.apos.trDelta );
  ent->s.apos.trDuration = angle * 1000 / ent->speed;

  if( ent->s.apos.trDuration <= 0 )
    ent->s.apos.trDuration = 1;
}


/*
===============================================================================

DOOR

A use can be triggered either by a touch function, by being shot, or by being
targeted by another entity.

===============================================================================
*/

/*
================
Blocked_Door
================
*/
void Blocked_Door(gentity_t *ent, gentity_t *other) {
  // remove anything other than a client or buildable
  if(!other->client && other->s.eType != ET_BUILDABLE) {
    G_FreeEntity(other);
    return;
  }

  if(
    ent->damage &&
    (other->s.eType != ET_BUILDABLE || other->health > 0)) {
    G_Damage( other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH );
  }

  if(ent->spawnflags & 4) {
    return;   // crushers don't reverse
  }

  // reverse direction
  Use_BinaryMover(ent, ent, other);
}

/*
================
Touch_DoorTrigger
================
*/
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace )
{
  moverState_t teamState;

  //buildables don't trigger movers
  if( other->s.eType == ET_BUILDABLE )
    return;

  teamState = GetMoverTeamState( ent->parent );

  if( teamState != MS_1TO2 )
    Use_BinaryMover( ent->parent, ent, other );
}


void Think_MatchTeam( gentity_t *ent )
{
  if( ent->flags & FL_TEAMSLAVE )
    return;
  MatchTeam( ent, ent->moverState, level.time );
}


/*
======================
Think_SpawnNewDoorTrigger

All of the parts of a door have been spawned, so create
a trigger that encloses all of them
======================
*/
void Think_SpawnNewDoorTrigger( gentity_t *ent )
{
  gentity_t *other;
  vec3_t    mins, maxs;
  int       i, best;

  // find the bounds of everything on the team
  VectorCopy( ent->r.absmin, mins );
  VectorCopy( ent->r.absmax, maxs );

  for( other = ent->teamchain; other; other=other->teamchain )
  {
    AddPointToBounds( other->r.absmin, mins, maxs );
    AddPointToBounds( other->r.absmax, mins, maxs );
  }

  // find the thinnest axis, which will be the one we expand
  best = 0;
  for( i = 1; i < 3; i++ )
  {
    if( maxs[ i ] - mins[ i ] < maxs[ best ] - mins[ best ] )
      best = i;
  }

  maxs[ best ] += 60;
  mins[ best ] -= 60;

  // create a trigger with this size
  other = G_Spawn( );
  other->classname = "door_trigger";
  VectorCopy( mins, other->r.mins );
  VectorCopy( maxs, other->r.maxs );
  other->parent = ent;
  G_SetContents( other, CONTENTS_TRIGGER );
  other->touch = Touch_DoorTrigger;
  // remember the thinnest axis
  other->count = best;
  SV_LinkEntity( other );

  if( ent->moverState < MS_POS1 )
    Think_MatchTeam( ent );
}


/*QUAKED func_door (0 .5 .8) ? START_OPEN x CRUSHER
TOGGLE    wait in both the start and end states for a trigger event.
START_OPEN  the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER monsters will not trigger this door

"model2"  .md3 model to also draw
"angle"   determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"speed"   movement speed (100 default)
"wait"    wait before returning (3 default, -1 = never return)
"lip"   lip remaining at end of move (8 default)
"dmg"   damage to inflict when blocked (2 default)
"color"   constantLight color
"light"   constantLight radius
"health"  if set, the door must be shot open
*/
void SP_func_door( gentity_t *ent )
{
  vec3_t  abs_movedir;
  float   distance;
  vec3_t  size;
  float   lip;
  char    *s;
  int     health;

  G_SpawnString( "sound2to1", "sound/movers/doors/dr1_strt.wav", &s );
  ent->sound2to1 = G_SoundIndex( s );
  G_SpawnString( "sound1to2", "sound/movers/doors/dr1_strt.wav", &s );
  ent->sound1to2 = G_SoundIndex( s );

  G_SpawnString( "soundPos2", "sound/movers/doors/dr1_end.wav", &s );
  ent->soundPos2 = G_SoundIndex( s );
  G_SpawnString( "soundPos1", "sound/movers/doors/dr1_end.wav", &s );
  ent->soundPos1 = G_SoundIndex( s );

  ent->blocked = Blocked_Door;

  // default speed of 400
  if( !ent->speed )
    ent->speed = 400;

  // default wait of 2 seconds
  if( !ent->wait )
    ent->wait = 2;

  ent->wait *= 1000;

  // default lip of 8 units
  G_SpawnFloat( "lip", "8", &lip );

  // default damage of 2 points
  G_SpawnInt( "dmg", "2", &ent->damage );

  ent->damage = BG_HP2SU( ent->damage);

  // first position at start
  VectorCopy( ent->r.currentOrigin, ent->pos1 );

  G_SetMovedir( ent->r.currentAngles, ent->movedir );
  VectorClear( ent->s.apos.trBase );
  VectorClear( ent->r.currentOrigin );
  // calculate second position
  SV_SetBrushModel( ent, ent->model );
  abs_movedir[ 0 ] = fabs( ent->movedir[ 0 ] );
  abs_movedir[ 1 ] = fabs( ent->movedir[ 1 ] );
  abs_movedir[ 2 ] = fabs( ent->movedir[ 2 ] );
  VectorSubtract( ent->r.maxs, ent->r.mins, size );
  distance = DotProduct( abs_movedir, size ) - lip;
  VectorMA( ent->pos1, distance, ent->movedir, ent->pos2 );

  // if "start_open", reverse position 1 and 2
  if( ent->spawnflags & 1 )
  {
    vec3_t  temp;

    VectorCopy( ent->pos2, temp );
    VectorCopy( ent->r.currentOrigin, ent->pos2 );
    VectorCopy( temp, ent->pos1 );
  }

  InitMover( ent );

  ent->s.eFlags |= EF_ASTRAL_NOCLIP;

  ent->nextthink = level.time + FRAMETIME;

  G_SpawnInt( "health", "0", &health );
  if( health )
    ent->takedamage = qtrue;

  if( ent->targetname || health )
  {
    // non touch/shoot doors
    ent->think = Think_MatchTeam;
  }
  else
    ent->think = Think_SpawnNewDoorTrigger;
}

/*QUAKED func_door_rotating (0 .5 .8) START_OPEN CRUSHER REVERSE TOGGLE X_AXIS Y_AXIS
 * This is the rotating door... just as the name suggests it's a door that rotates
 * START_OPEN the door to moves to its destination when spawned, and operate in reverse.
 * REVERSE    if you want the door to open in the other direction, use this switch.
 * TOGGLE   wait in both the start and end states for a trigger event.
 * X_AXIS   open on the X-axis instead of the Z-axis
 * Y_AXIS   open on the Y-axis instead of the Z-axis
 *
 *   You need to have an origin brush as part of this entity.  The center of that brush will be
 *   the point around which it is rotated. It will rotate around the Z axis by default.  You can
 *   check either the X_AXIS or Y_AXIS box to change that.
 *
 *   "model2" .md3 model to also draw
 *   "distance" how many degrees the door will open
 *   "speed"    how fast the door will open (degrees/second)
 *   "color"    constantLight color
 *   "light"    constantLight radius
 *   */

void SP_func_door_rotating( gentity_t *ent )
{
  char    *s;
  int     health;

  G_SpawnString( "sound2to1", "sound/movers/doors/dr1_strt.wav", &s );
  ent->sound2to1 = G_SoundIndex( s );
  G_SpawnString( "sound1to2", "sound/movers/doors/dr1_strt.wav", &s );
  ent->sound1to2 = G_SoundIndex( s );

  G_SpawnString( "soundPos2", "sound/movers/doors/dr1_end.wav", &s );
  ent->soundPos2 = G_SoundIndex( s );
  G_SpawnString( "soundPos1", "sound/movers/doors/dr1_end.wav", &s );
  ent->soundPos1 = G_SoundIndex( s );

  ent->blocked = Blocked_Door;

  //default speed of 120
  if( !ent->speed )
    ent->speed = 120;

  // if speed is negative, positize it and add reverse flag
  if( ent->speed < 0 )
  {
    ent->speed *= -1;
    ent->spawnflags |= 8;
  }

  // default of 2 seconds
  if( !ent->wait )
    ent->wait = 2;

  ent->wait *= 1000;

  // set the axis of rotation
  VectorClear( ent->movedir );
  VectorClear( ent->s.apos.trBase );
  VectorClear( ent->r.currentAngles );

  if( ent->spawnflags & 32 )
    ent->movedir[ 2 ] = 1.0;
  else if( ent->spawnflags & 64 )
    ent->movedir[ 0 ] = 1.0;
  else
    ent->movedir[ 1 ] = 1.0;

  // reverse direction if necessary
  if( ent->spawnflags & 8 )
    VectorNegate ( ent->movedir, ent->movedir );

  // default distance of 90 degrees. This is something the mapper should not
  // leave out, so we'll tell him if he does.
  if( !ent->rotatorAngle )
  {
    Com_Printf( "%s at %s with no rotatorAngle set.\n",
              ent->classname, vtos( ent->r.currentOrigin ) );

    ent->rotatorAngle = 90.0;
  }

  VectorCopy( ent->r.currentAngles, ent->pos1 );
  SV_SetBrushModel( ent, ent->model );
  VectorMA( ent->pos1, ent->rotatorAngle, ent->movedir, ent->pos2 );

  // if "start_open", reverse position 1 and 2
  if( ent->spawnflags & 1 )
  {
    vec3_t  temp;

    VectorCopy( ent->pos2, temp );
    VectorCopy( ent->pos1, ent->pos2 );
    VectorCopy( temp, ent->pos1 );
    VectorNegate( ent->movedir, ent->movedir );
  }

  InitRotator( ent );

  ent->s.eFlags |= EF_ASTRAL_NOCLIP;

  ent->nextthink = level.time + FRAMETIME;

  ent->damage = BG_HP2SU( ent->damage );

  G_SpawnInt( "health", "0", &health );
  if( health )
    ent->takedamage = qtrue;

  if( ent->targetname || health )
  {
    // non touch/shoot doors
    ent->think = Think_MatchTeam;
  }
  else
    ent->think = Think_SpawnNewDoorTrigger;
}

/*QUAKED func_door_model (0 .5 .8) ? START_OPEN
TOGGLE    wait in both the start and end states for a trigger event.
START_OPEN  the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER monsters will not trigger this door

"model2"  .md3 model to also draw
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"speed"   movement speed (100 default)
"wait"    wait before returning (3 default, -1 = never return)
"color"   constantLight color
"light"   constantLight radius
"health"  if set, the door must be shot open
*/
void SP_func_door_model( gentity_t *ent )
{
  char      *s;
  float     light;
  vec3_t    color;
  qboolean  lightSet, colorSet;
  char      *sound;
  gentity_t *clipBrush;
  int       health;

  G_SpawnString( "sound2to1", "sound/movers/doors/dr1_strt.wav", &s );
  ent->sound2to1 = G_SoundIndex( s );
  G_SpawnString( "sound1to2", "sound/movers/doors/dr1_strt.wav", &s );
  ent->sound1to2 = G_SoundIndex( s );

  G_SpawnString( "soundPos2", "sound/movers/doors/dr1_end.wav", &s );
  ent->soundPos2 = G_SoundIndex( s );
  G_SpawnString( "soundPos1", "sound/movers/doors/dr1_end.wav", &s );
  ent->soundPos1 = G_SoundIndex( s );

  //default speed of 100ms
  if( !ent->speed )
    ent->speed = 200;

  //default wait of 2 seconds
  if( ent->wait <= 0 )
    ent->wait = 2;

  ent->wait *= 1000;

  //brush model
  clipBrush = ent->clipBrush = G_Spawn( );
  clipBrush->classname = "func_door_model_clip_brush";
  clipBrush->clipBrush = ent; // link back
  clipBrush->model = ent->model;
  SV_SetBrushModel( clipBrush, clipBrush->model );
  clipBrush->s.eType = ET_INVISIBLE;
  SV_LinkEntity( clipBrush );

  //copy the bounds back from the clipBrush so the
  //triggers can be made
  VectorCopy( clipBrush->r.absmin, ent->r.absmin );
  VectorCopy( clipBrush->r.absmax, ent->r.absmax );
  VectorCopy( clipBrush->r.mins, ent->r.mins );
  VectorCopy( clipBrush->r.maxs, ent->r.maxs );

  G_SpawnVector( "modelOrigin", "0 0 0", ent->r.currentOrigin );

  G_SpawnVector( "scale", "1 1 1", ent->s.origin2 );

  // if the "model2" key is set, use a seperate model
  // for drawing, but clip against the brushes
  if( !ent->model2 )
    Com_Printf( S_COLOR_YELLOW "WARNING: func_door_model %d spawned with no model2 key\n", ent->s.number );
  else
    ent->s.modelindex = G_ModelIndex( ent->model2 );

  // if the "noise" key is set, use a constant looping sound when moving
  if( G_SpawnString( "noise", "", &sound ) )
    ent->soundLoop = G_SoundIndex( sound );

  // if the "color" or "light" keys are set, setup constantLight
  lightSet = G_SpawnFloat( "light", "100", &light );
  colorSet = G_SpawnVector( "color", "1 1 1", color );

  if( lightSet || colorSet )
  {
    int   r, g, b, i;

    r = color[ 0 ] * 255;
    if( r > 255 )
      r = 255;

    g = color[ 1 ] * 255;
    if( g > 255 )
      g = 255;

    b = color[ 2 ] * 255;
    if( b > 255 )
      b = 255;

    i = light / 4;
    if( i > 255 )
      i = 255;

    ent->s.constantLight = r | ( g << 8 ) | ( b << 16 ) | ( i << 24 );
  }

  ent->use = Use_BinaryMover;

  ent->moverState = MS_POS1;
  ent->moverMotionType = MM_MODEL;
  ent->s.eType = ET_MODELDOOR;
  ent->s.pos.trType = TR_STATIONARY;
  ent->s.pos.trTime = level.time;
  ent->s.pos.trDuration = 0;
  VectorClear( ent->s.pos.trDelta );
  ent->s.apos.trType = TR_STATIONARY;
  ent->s.apos.trTime = level.time;
  ent->s.apos.trDuration = 0;
  VectorClear( ent->s.apos.trDelta );

  ent->s.misc   = (int)ent->animation[ 0 ];                  //first frame
  ent->s.weapon = abs( (int)ent->animation[ 1 ] );           //number of frames

  //must be at least one frame -- mapper has forgotten animation key
  if( ent->s.weapon == 0 )
    ent->s.weapon = 1;

  ent->s.torsoAnim = ent->s.weapon * ( 1000.0f / ent->speed );  //framerate

  SV_LinkEntity( ent );

  G_SpawnInt( "health", "0", &health );
  if( health )
    ent->takedamage = qtrue;

  if( !( ent->targetname || health ) )
  {
    ent->nextthink = level.time + FRAMETIME;
    ent->think = Think_SpawnNewDoorTrigger;
  }
}

/*
===============================================================================

PLAT

===============================================================================
*/

/*
==============
Touch_Plat

Don't allow to descend if a player is on it and is alive
===============
*/
void Touch_Plat( gentity_t *ent, gentity_t *other, trace_t *trace )
{
  // DONT_WAIT
  if( ent->spawnflags & 1 )
    return;

  if( !other->client || other->health <= 0 )
    return;

  // delay return-to-pos1 by one second
  if( ent->moverState == MS_POS2 )
    ent->nextthink = level.time + 1000;
}

/*
==============
Touch_PlatCenterTrigger

If the plat is at the bottom position, start it going up
===============
*/
void Touch_PlatCenterTrigger( gentity_t *ent, gentity_t *other, trace_t *trace )
{
  if( !other->client )
    return;

  if( ent->parent->moverState == MS_POS1 )
    Use_BinaryMover( ent->parent, ent, other );
}


/*
================
SpawnPlatTrigger

Spawn a trigger in the middle of the plat's low position
Elevator cars require that the trigger extend through the entire low position,
not just sit on top of it.
================
*/
void SpawnPlatTrigger( gentity_t *ent )
{
  gentity_t *trigger;
  vec3_t    tmin, tmax;

  // the middle trigger will be a thin trigger just
  // above the starting position
  trigger = G_Spawn( );
  trigger->classname = "plat_trigger";
  trigger->touch = Touch_PlatCenterTrigger;
  G_SetContents( trigger, CONTENTS_TRIGGER );
  trigger->parent = ent;

  tmin[ 0 ] = ent->pos1[ 0 ] + ent->r.mins[ 0 ] + 33;
  tmin[ 1 ] = ent->pos1[ 1 ] + ent->r.mins[ 1 ] + 33;
  tmin[ 2 ] = ent->pos1[ 2 ] + ent->r.mins[ 2 ];

  tmax[ 0 ] = ent->pos1[ 0 ] + ent->r.maxs[ 0 ] - 33;
  tmax[ 1 ] = ent->pos1[ 1 ] + ent->r.maxs[ 1 ] - 33;
  tmax[ 2 ] = ent->pos1[ 2 ] + ent->r.maxs[ 2 ] + 8;

  if( tmax[ 0 ] <= tmin[ 0 ] )
  {
    tmin[ 0 ] = ent->pos1[ 0 ] + ( ent->r.mins[ 0 ] + ent->r.maxs[ 0 ] ) * 0.5;
    tmax[ 0 ] = tmin[ 0 ] + 1;
  }

  if( tmax[ 1 ] <= tmin[ 1 ] )
  {
    tmin[ 1 ] = ent->pos1[ 1 ] + ( ent->r.mins[ 1 ] + ent->r.maxs[ 1 ] ) * 0.5;
    tmax[ 1 ] = tmin[ 1 ] + 1;
  }

  VectorCopy( tmin, trigger->r.mins );
  VectorCopy( tmax, trigger->r.maxs );

  SV_LinkEntity( trigger );
}


/*QUAKED func_plat (0 .5 .8) ?
Plats are always drawn in the extended position so they will light correctly.

"lip"   default 8, protrusion above rest position
"height"  total height of movement, defaults to model height
"speed"   overrides default 200.
"dmg"   overrides default 2
"model2"  .md3 model to also draw
"color"   constantLight color
"light"   constantLight radius
*/
void SP_func_plat( gentity_t *ent )
{
  float lip, height;
  char  *s;

  G_SpawnString( "sound2to1", "sound/movers/plats/pt1_strt.wav", &s );
  ent->sound2to1 = G_SoundIndex( s );
  G_SpawnString( "sound1to2", "sound/movers/plats/pt1_strt.wav", &s );
  ent->sound1to2 = G_SoundIndex( s );

  G_SpawnString( "soundPos2", "sound/movers/plats/pt1_end.wav", &s );
  ent->soundPos2 = G_SoundIndex( s );
  G_SpawnString( "soundPos1", "sound/movers/plats/pt1_end.wav", &s );
  ent->soundPos1 = G_SoundIndex( s );

  VectorClear( ent->s.apos.trBase );
  VectorClear( ent->r.currentAngles );

  G_SpawnFloat( "speed", "200", &ent->speed );
  G_SpawnInt( "dmg", "2", &ent->damage );
  G_SpawnFloat( "wait", "1", &ent->wait );
  G_SpawnFloat( "lip", "8", &lip );

  ent->damage = BG_HP2SU( ent->damage);

  ent->wait = 1000;

  // create second position
  SV_SetBrushModel( ent, ent->model );

  if( !G_SpawnFloat( "height", "0", &height ) )
    height = ( ent->r.maxs[ 2 ] - ent->r.mins[ 2 ] ) - lip;

  // pos1 is the rest (bottom) position, pos2 is the top
  VectorCopy( ent->r.currentOrigin, ent->pos2 );
  VectorCopy( ent->pos2, ent->pos1 );
  ent->pos1[ 2 ] -= height;

  InitMover( ent );

  // touch function keeps the plat from returning while
  // a live player is standing on it
  ent->touch = Touch_Plat;

  ent->blocked = Blocked_Door;

  ent->parent = ent;  // so it can be treated as a door

  // spawn the trigger if one hasn't been custom made
  if( !ent->targetname )
    SpawnPlatTrigger( ent );
}


/*
===============================================================================

BUTTON

===============================================================================
*/

/*
==============
Touch_Button

===============
*/
void Touch_Button( gentity_t *ent, gentity_t *other, trace_t *trace )
{
  if( !other->client )
    return;

  if( ent->moverState == MS_POS1 )
    Use_BinaryMover( ent, other, other );
}


/*QUAKED func_button (0 .5 .8) ?
When a button is touched, it moves some distance in the direction of its angle, triggers all of its targets, waits some time, then returns to its original position where it can be triggered again.

"model2"  .md3 model to also draw
"angle"   determines the opening direction
"target"  all entities with a matching targetname will be used
"speed"   override the default 40 speed
"wait"    override the default 1 second wait (-1 = never return)
"lip"   override the default 4 pixel lip remaining at end of move
"health"  if set, the button must be killed instead of touched
"color"   constantLight color
"light"   constantLight radius
*/
void SP_func_button( gentity_t *ent )
{
  vec3_t  abs_movedir;
  float   distance;
  vec3_t  size;
  float   lip;
  char    *s;

  G_SpawnString( "sound1to2", "sound/movers/switches/button1.wav", &s );
  ent->sound1to2 = G_SoundIndex( s );

  if( !ent->speed )
    ent->speed = 40;

  if( !ent->wait )
    ent->wait = 1;

  ent->wait *= 1000;

  // first position
  VectorCopy( ent->r.currentOrigin, ent->pos1 );

  G_SetMovedir( ent->r.currentAngles, ent->movedir );
  VectorClear( ent->s.apos.trBase );
  VectorClear( ent->r.currentAngles );
  // calculate second position
  SV_SetBrushModel( ent, ent->model );

  G_SpawnFloat( "lip", "4", &lip );

  abs_movedir[ 0 ] = fabs( ent->movedir[ 0 ] );
  abs_movedir[ 1 ] = fabs( ent->movedir[ 1 ] );
  abs_movedir[ 2 ] = fabs( ent->movedir[ 2 ] );
  VectorSubtract( ent->r.maxs, ent->r.mins, size );
  distance = abs_movedir[ 0 ] * size[ 0 ] + abs_movedir[ 1 ] * size[ 1 ] + abs_movedir[ 2 ] * size[ 2 ] - lip;
  VectorMA( ent->pos1, distance, ent->movedir, ent->pos2 );

  if( ent->health )
  {
    // shootable button
    ent->takedamage = qtrue;
  }
  else
  {
    // touchable button
    ent->touch = Touch_Button;
  }

  InitMover( ent );
}



/*
===============================================================================

TRAIN

===============================================================================
*/


#define TRAIN_START_OFF   1
#define TRAIN_BLOCK_STOPS 2

/*
===============
Think_BeginMoving

The wait time at a corner has completed, so start moving again
===============
*/
void Think_BeginMoving( gentity_t *ent )
{
  ent->s.pos.trTime = level.time;
  ent->s.pos.trType = TR_LINEAR_STOP;
}

/*
===============
Reached_Train
===============
*/
void Reached_Train( gentity_t *ent )
{
  gentity_t *next;
  float     speed;
  vec3_t    move;
  float     length;

  // copy the apropriate values
  next = ent->nextTrain;
  if( !next || !next->nextTrain )
    return;   // just stop

  // fire all other targets
  G_UseTargets( next, NULL );

  // set the new trajectory
  ent->nextTrain = next->nextTrain;
  VectorCopy( next->r.currentOrigin, ent->pos1 );
  VectorCopy( next->nextTrain->r.currentOrigin, ent->pos2 );

  // if the path_corner has a speed, use that
  if( next->speed )
  {
    speed = next->speed;
  }
  else
  {
    // otherwise use the train's speed
    speed = ent->speed;
  }

  if( speed < 1 )
    speed = 1;

  ent->lastSpeed = speed;

  // calculate duration
  VectorSubtract( ent->pos2, ent->pos1, move );
  length = VectorLength( move );

  ent->s.pos.trDuration = length * 1000 / speed;

  // Be sure to send to clients after any fast move case
  ent->r.svFlags &= ~SVF_NOCLIENT;

  // Fast move case
  if( ent->s.pos.trDuration < 1 )
  {
    // As trDuration is used later in a division, we need to avoid that case now
    ent->s.pos.trDuration = 1;

    // Don't send entity to clients so it becomes really invisible
    ent->r.svFlags |= SVF_NOCLIENT;
  }

  // looping sound
  ent->s.loopSound = next->soundLoop;

  // start it going
  SetMoverState( ent, MS_1TO2, level.time );

  if( ent->spawnflags & TRAIN_START_OFF )
  {
    ent->s.pos.trType = TR_STATIONARY;
    return;
  }

  // if there is a "wait" value on the target, don't start moving yet
  if( next->wait )
  {
    ent->nextthink = level.time + next->wait * 1000;
    ent->think = Think_BeginMoving;
    ent->s.pos.trType = TR_STATIONARY;
  }
}

/*
================
Start_Train
================
*/
void Start_Train( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
  vec3_t  move;

  //recalculate duration as the mover is highly
  //unlikely to be right on a path_corner
  VectorSubtract( ent->pos2, ent->pos1, move );
  ent->s.pos.trDuration = VectorLength( move ) * 1000 / ent->lastSpeed;
  SetMoverState( ent, MS_1TO2, level.time );

  ent->spawnflags &= ~TRAIN_START_OFF;
}

/*
================
Stop_Train
================
*/
void Stop_Train( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
  vec3_t  origin;

  //get current origin
  BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
  VectorCopy( origin, ent->pos1 );
  SetMoverState( ent, MS_POS1, level.time );

  ent->spawnflags |= TRAIN_START_OFF;
}

/*
================
Use_Train
================
*/
void Use_Train( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
  if( ent->spawnflags & TRAIN_START_OFF )
  {
    //train is currently not moving so start it
    Start_Train( ent, other, activator );
  }
  else
  {
    //train is moving so stop it
    Stop_Train( ent, other, activator );
  }
}

/*
===============
Think_SetupTrainTargets

Link all the corners together
===============
*/
void Think_SetupTrainTargets( gentity_t *ent )
{
  gentity_t *path, *next, *start;

  ent->nextTrain = G_Find( NULL, FOFS( targetname ), ent->target );

  if( !ent->nextTrain )
  {
    Com_Printf( "func_train at %s with an unfound target\n",
      vtos( ent->r.absmin ) );
    return;
  }

  start = NULL;
  for( path = ent->nextTrain; path != start; path = next )
  {
    if( !start )
      start = path;

    if( !path->target )
    {
      Com_Printf( "Train corner at %s without a target\n",
        vtos( path->r.currentOrigin ) );
      return;
    }

    // find a path_corner among the targets
    // there may also be other targets that get fired when the corner
    // is reached
    next = NULL;
    do
    {
      next = G_Find( next, FOFS( targetname ), path->target );

      if( !next )
      {
        Com_Printf( "Train corner at %s without a target path_corner\n",
          vtos( path->r.currentOrigin ) );
        return;
      }
    } while( strcmp( next->classname, "path_corner" ) );

    path->nextTrain = next;
  }

  // start the train moving from the first corner
  Reached_Train( ent );
}



/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8)
Train path corners.
Target: next path corner and other targets to fire
"speed" speed to move to the next corner
"wait" seconds to wait before behining move to next corner
*/
void SP_path_corner( gentity_t *self )
{
  if( !self->targetname )
  {
    Com_Printf( "path_corner with no targetname at %s\n", vtos( self->r.currentOrigin ) );
    G_FreeEntity( self );
    return;
  }
  // path corners don't need to be linked in
}

/*
================
Blocked_Train
================
*/
void Blocked_Train( gentity_t *self, gentity_t *other )
{
  if( self->spawnflags & TRAIN_BLOCK_STOPS )
    Stop_Train( self, other, other );
  else
  {
    if( !other->client )
    {
      //whatever is blocking the train isn't a client

      //KILL!!1!!!
      G_Damage( other, self, self, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_CRUSH );

      //buildables need to be handled differently since even when
      //dealth fatal amounts of damage they won't instantly become non-solid
      if( other->s.eType == ET_BUILDABLE && other->spawned )
      {
        vec3_t    dir;
        gentity_t *tent;

        if( other->buildableTeam == TEAM_ALIENS )
        {
          VectorCopy( other->s.origin2, dir );
          tent = G_TempEntity( other->r.currentOrigin, EV_ALIEN_BUILDABLE_EXPLOSION );
          tent->s.eventParm = DirToByte( dir );
        }
        else if( other->buildableTeam == TEAM_HUMANS )
        {
          VectorSet( dir, 0.0f, 0.0f, 1.0f );
          tent = G_TempEntity( other->r.currentOrigin, EV_HUMAN_BUILDABLE_EXPLOSION );
          tent->s.eventParm = DirToByte( dir );
        }
      }

      //if it's still around free it
      if( other )
        G_FreeEntity( other );

      return;
    }

    G_Damage( other, self, self, NULL, NULL, 0, DAMAGE_INSTAGIB, MOD_CRUSH );
  }
}


/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
A train is a mover that moves between path_corner target points.
Trains MUST HAVE AN ORIGIN BRUSH.
The train spawns at the first target it is pointing at.
"model2"  .md3 model to also draw
"speed"   default 100
"dmg"   default 2
"noise"   looping sound to play when the train is in motion
"target"  next path corner
"color"   constantLight color
"light"   constantLight radius
*/
void SP_func_train( gentity_t *self )
{
  VectorClear( self->s.apos.trBase );
  VectorClear( self->r.currentAngles );

  if( self->spawnflags & TRAIN_BLOCK_STOPS )
    self->damage = 0;
  else if( !self->damage )
    self->damage = BG_HP2SU( 2 );

  if( !self->speed )
    self->speed = 100;

  if( !self->target )
  {
    Com_Printf( "func_train without a target at %s\n", vtos( self->r.absmin ) );
    G_FreeEntity( self );
    return;
  }

  SV_SetBrushModel( self, self->model );
  InitMover( self );

  self->reached = Reached_Train;
  self->use = Use_Train;
  self->blocked = Blocked_Train;

  // start trains on the second frame, to make sure their targets have had
  // a chance to spawn
  self->nextthink = level.time + FRAMETIME;
  self->think = Think_SetupTrainTargets;
}

/*
===============================================================================

STATIC

===============================================================================
*/


/*QUAKED func_static (0 .5 .8) ?
A bmodel that just sits there, doing nothing.  Can be used for conditional walls and models.
"model2"  .md3 model to also draw
"color"   constantLight color
"light"   constantLight radius
*/
void SP_func_static( gentity_t *ent )
{
  vec3_t savedOrigin;
  SV_SetBrushModel( ent, ent->model );
  VectorCopy( ent->r.currentOrigin, savedOrigin );
  InitMover( ent );
  VectorCopy( savedOrigin, ent->r.currentOrigin );
  VectorCopy( savedOrigin, ent->s.pos.trBase );
}


/*
===============================================================================

ROTATING

===============================================================================
*/


/*QUAKED func_rotating (0 .5 .8) ? START_ON - X_AXIS Y_AXIS
You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"model2"  .md3 model to also draw
"speed"   determines how fast it moves; default value is 100.
"dmg"   damage to inflict when blocked (2 default)
"color"   constantLight color
"light"   constantLight radius
*/
void SP_func_rotating( gentity_t *ent )
{
  vec3_t savedOrigin;

  if( !ent->speed )
    ent->speed = 100;

  // set the axis of rotation
  ent->s.apos.trType = TR_LINEAR;

  if( ent->spawnflags & 4 )
    ent->s.apos.trDelta[ 2 ] = ent->speed;
  else if( ent->spawnflags & 8 )
    ent->s.apos.trDelta[ 0 ] = ent->speed;
  else
    ent->s.apos.trDelta[ 1 ] = ent->speed;

  if( !ent->damage )
    ent->damage = BG_HP2SU( 2 );

  SV_SetBrushModel( ent, ent->model );
  VectorCopy( ent->r.currentOrigin, savedOrigin );
  InitMover( ent );
  VectorCopy( savedOrigin, ent->r.currentOrigin );
  VectorCopy( savedOrigin, ent->s.pos.trBase );

  SV_LinkEntity( ent );
}


/*
===============================================================================

BOBBING

===============================================================================
*/


/*QUAKED func_bobbing (0 .5 .8) ? X_AXIS Y_AXIS
Normally bobs on the Z axis
"model2"  .md3 model to also draw
"height"  amplitude of bob (32 default)
"speed"   seconds to complete a bob cycle (4 default)
"phase"   the 0.0 to 1.0 offset in the cycle to start at
"dmg"   damage to inflict when blocked (2 default)
"color"   constantLight color
"light"   constantLight radius
*/
void SP_func_bobbing( gentity_t *ent )
{
  float   height;
  float   phase;
  vec3_t  savedOrigin;

  G_SpawnFloat( "speed", "4", &ent->speed );
  G_SpawnFloat( "height", "32", &height );
  G_SpawnInt( "dmg", "2", &ent->damage );
  G_SpawnFloat( "phase", "0", &phase );

  ent->damage = BG_HP2SU( ent->damage);

  SV_SetBrushModel( ent, ent->model );
  VectorCopy( ent->r.currentOrigin, savedOrigin );
  InitMover( ent );
  VectorCopy( savedOrigin, ent->r.currentOrigin );
  VectorCopy( savedOrigin, ent->s.pos.trBase );

  ent->s.pos.trDuration = ent->speed * 1000;
  ent->s.pos.trTime = ent->s.pos.trDuration * phase;
  ent->s.pos.trType = TR_SINE;

  // set the axis of bobbing
  if( ent->spawnflags & 1 )
    ent->s.pos.trDelta[ 0 ] = height;
  else if( ent->spawnflags & 2 )
    ent->s.pos.trDelta[ 1 ] = height;
  else
    ent->s.pos.trDelta[ 2 ] = height;
}

/*
===============================================================================

PENDULUM

===============================================================================
*/


/*QUAKED func_pendulum (0 .5 .8) ?
You need to have an origin brush as part of this entity.
Pendulums always swing north / south on unrotated models.  Add an angles field to the model to allow rotation in other directions.
Pendulum frequency is a physical constant based on the length of the beam and gravity.
"model2"  .md3 model to also draw
"speed"   the number of degrees each way the pendulum swings, (30 default)
"phase"   the 0.0 to 1.0 offset in the cycle to start at
"dmg"   damage to inflict when blocked (2 default)
"color"   constantLight color
"light"   constantLight radius
*/
void SP_func_pendulum( gentity_t *ent )
{
  float freq;
  float length;
  float phase;
  float speed;
  vec3_t savedOrigin;

  G_SpawnFloat( "speed", "30", &speed );
  G_SpawnInt( "dmg", "2", &ent->damage );
  G_SpawnFloat( "phase", "0", &phase );

  ent->damage = BG_HP2SU( ent->damage);

  SV_SetBrushModel( ent, ent->model );

  // find pendulum length
  length = fabs( ent->r.mins[ 2 ] );

  if( length < 8 )
    length = 8;

  freq = 1 / ( M_PI * 2 ) * sqrt( g_gravity.value / ( 3 * length ) );

  ent->s.pos.trDuration = ( 1000 / freq );

  VectorCopy( ent->r.currentOrigin, savedOrigin );
  InitMover( ent );
  VectorCopy( savedOrigin, ent->r.currentOrigin );
  VectorCopy( savedOrigin, ent->s.pos.trBase );

  ent->s.apos.trDuration = 1000 / freq;
  ent->s.apos.trTime = ent->s.apos.trDuration * phase;
  ent->s.apos.trType = TR_SINE;
  ent->s.apos.trDelta[ 2 ] = speed;
}
