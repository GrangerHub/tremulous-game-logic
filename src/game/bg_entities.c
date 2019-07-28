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

  ueid->id = (int)bg_entities[ent_num].es->origin[0];
}

/*
============
BG_UEID_get_ent_num

If the UEID is still valid, this function returns the entity number it
points to, otherwise ENTITYNUM_NONE is returned.
============
*/
int BG_UEID_get_ent_num(bgentity_id *ueid) {
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

  if(ueid->id != ((int)bg_entities[ueid->ent_num].es->origin[0])){
    ueid->ent_num = ENTITYNUM_NONE;
    ueid->id = 0;
  }
  return ueid->ent_num;
}
