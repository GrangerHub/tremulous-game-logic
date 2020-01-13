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

// g_public.h -- game module information visible to server

#ifndef __G_PUBLIC_H
#define __G_PUBLIC_H

#define GAME_API_VERSION  9

// entity->svFlags
// the server does not know how to interpret most of the values
// in entityStates (level eType), so the game must explicitly flag
// special server behaviors
#define SVF_NOCLIENT            0x00000001  // don't send entity to clients, even if it has effects

#define SVF_CLIENTMASK_EXCLUSIVE          0x00000002  // send only to clients specified by these bitmasks:
                      // entityShared_t->singleClient: low-order bits (0..31)
                      // entityShared_t->hack.generic1: high-order bits (32..63)
#define SVF_CLIENTMASK_INCLUSIVE          0x00000004 // similar to SVF_CLIENTMASK_EXCLUSIVE, always send to
                      // clients specified by the same above bitmasks, but can still send to other clients.

#define SVF_BROADCAST           0x00000020  // send to all connected clients
#define SVF_PORTAL              0x00000040  // merge a second pvs at origin2 into snapshots

#define SVF_SINGLECLIENT    0x00000100  // only send to a single client (entityShared_t->singleClient)
#define SVF_NOSERVERINFO    0x00000200  // don't send CS_SERVERINFO updates to this client
                      // so that it can be updated for ping tools without
                      // lagging clients
#define SVF_CAPSULE        0x00000400  // use capsule for collision detection instead of bbox
#define SVF_NOTSINGLECLIENT    0x00000800  // send entity to everyone but one client
                      // (entityShared_t->singleClient)

//===============================================================


typedef struct {
  entityState_t hack;     // exists (as padding) to retain ABI compatibility
                          //  with GPP, but can be used for extension hacks

  qboolean  linked;       // qfalse if not in any good cluster
  int     linkcount;

  int     svFlags;        // SVF_NOCLIENT, SVF_BROADCAST, etc.
  int     singleClient;   // only send to this client when SVF_SINGLECLIENT is set

  qboolean  bmodel;       // if false, assume an explicit mins / maxs bounding box
                  // only set by SV_SetBrushModel
  vec3_t    mins, maxs;
  int     contents;     // CONTENTS_TRIGGER, CONTENTS_SOLID, CONTENTS_BODY, etc
                  // a non-solid entity should set to 0

  vec3_t    absmin, absmax;   // derived from mins/maxs and origin + rotation

  // currentOrigin will be used for all collision detection and world linking.
  // it will not necessarily be the same as the trajectory evaluation for the current
  // time, because each entity must be moved one at a time after time is advanced
  // to avoid simultanious collision issues
  vec3_t    currentOrigin;
  vec3_t    currentAngles;

  // when a trace call is made and passEntityNum != ENTITYNUM_NONE,
  // an ent will be excluded from testing if:
  // ent->s.number == passEntityNum (don't interact with self)
  // ent->r.ownerNum == passEntityNum (don't interact with your own missiles)
  // entity[ent->r.ownerNum].r.ownerNum == passEntityNum (don't interact with other missiles from owner)
  int     ownerNum;
} entityShared_t;



// the server looks at a sharedEntity, which is the start of the game's gentity_t structure
typedef struct {
  entityState_t s;        // communicated by server to clients
  entityShared_t  r;        // shared by both the server system and game
} sharedEntity_t;


#define ADDRLEN 16
/*
addr_ts are passed as "arg" to admin_search for IP address matching
admin_search prints (char *)arg, so the stringified address needs to be first
*/
typedef struct
{
  char str[ 44 ];
  enum
  {
    IPv4,
    IPv6
  } type;
  byte addr[ ADDRLEN ];
  int mask;
} addr_t;

// Database
typedef enum {
  DB_OPEN,
  DB_CLOSE,
  DB_EXEC,
  DB_MAPSTAT_ADD,
  DB_SEEN_ADD,
  DB_TIME_GET,
  DB_LAST_MAPS,
  DB_SEEN,
  DB_COUNT //Must be last enum
} dbArray_t;

#define DATABASE_DATA_MAX 4096

//playmap
#define MAX_PLAYMAP_QUEUE_ENTRIES 128
// individual playmap entry in the queue
typedef struct playMap_s
{
  char     mapName[MAX_QPATH+1];
  char     layout[MAX_QPATH+1];

  qboolean console;
  char     guid[ 33 ];

  int      flags;
  //playMapFlag_t plusFlags[ PLAYMAP_NUM_FLAGS ];
  //playMapFlag_t minusFlags[ PLAYMAP_NUM_FLAGS ];
} playMap_t;

//scrim
typedef struct scrim_team_member_s
{
  qboolean inuse;
  size_t   roster_id;
  qboolean standby;
  char     netname[ MAX_COLORFUL_NAME_LENGTH ];
  char     registered_name[ MAX_COLORFUL_NAME_LENGTH ];
  addr_t   ip;
  char     guid[ 33 ];
  qboolean guidless;
} scrim_team_member_t;

typedef struct scrim_team_roster_s
{
  scrim_team_member_t members[64];
} scrim_team_roster_t;

typedef struct pers_scrim_team_info_s
{
  char                name[MAX_COLORFUL_NAME_LENGTH];
  int                 current_team;
  qboolean            has_captain;
  char                captain_guid[ 33 ];
  int                 wins;
  int                 losses;
  int                 draws;
  scrim_team_roster_t roster;
} pers_scrim_team_info_t;

typedef struct pers_scrim_s
{
  int                    mode;
  int                    win_condition;
  qboolean               timed_income;
  int                    sudden_death_mode;
  int                    sudden_death_time;
  int                    time_limit;
  pers_scrim_team_info_t team[3];
  int                    previous_round_win;
  int                    rounds_completed;
  int                    max_rounds;
} pers_scrim_t;

#endif	// __G_PUBLIC_H
