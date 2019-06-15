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
  entityState_t *es;
  playerState_t *ps;

  qboolean      *valid_entityState;
} bgentity_t;

static int        cgame_local_client_num; // this should be ENTITYNUM_NONE in the SGAME
static bgentity_t bg_entities[ MAX_GENTITIES ];


/*
============
BG_Init_Entities

Initialized in the sgame and cgame
============
*/
void BG_Init_Entities(void) {
  memset( bg_entities, 0, MAX_GENTITIES * sizeof( bg_entities[ 0 ] ) );
  cgame_local_client_num = ENTITYNUM_NONE;
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
BG_Locate_entityState

============
*/
void BG_Locate_entityState(
  entityState_t *es, int ent_num, qboolean *valid_check_variable) {
  Com_Assert(es);
  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);
  Com_Assert(valid_check_variable);

  bg_entities[ent_num].es = es;
  bg_entities[ent_num].valid_entityState = valid_check_variable;
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
BG_Locate_playerState

============
*/
void BG_Locate_playerState(playerState_t *ps, int ent_num) {
  Com_Assert(ps);
  Com_Assert(ent_num >= 0 && ent_num < MAX_CLIENTS);

  bg_entities[ent_num].ps = ps;

#ifdef CGAME
  //in the cgame this function should only be called for the local client's
  //playerState
  cgame_local_client_num = ent_num;
#endif
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

  if(!(*bg_entities[ueid->ent_num].valid_entityState)) {
    if(
      cgame_local_client_num != ENTITYNUM_NONE &&
      ueid->ent_num != cgame_local_client_num) {
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
