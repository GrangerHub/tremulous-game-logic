/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015 GrangerHub

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


/*
 * PLAYMAP POOL
 */

#define MAX_PLAYMAP_POOL_ENTRIES 128

// map pool for the playmap system
typedef struct playMapPool_s
{
  char *maps[ MAX_PLAYMAP_POOL_ENTRIES ];

  int  numMaps;
} playMapPool_t;


/*
 * PLAYMAP FLAGS
 */

// available flags for playmap (hint: these are just examples for a
// proof-of-concept demo. they are very likely to change)
typedef enum
{
  PLAYMAP_FLAG_NONE,

  PLAYMAP_FLAG_DPUNT, // Dretch Punt
  PLAYMAP_FLAG_FF,    // Friendly Fire
  PLAYMAP_FLAG_FBF,   // Friendly Buildable Fire
  PLAYMAP_FLAG_SD,    // Sudden Death
  PLAYMAP_FLAG_LGRAV, // Low Gravity
  PLAYMAP_FLAG_UBP,   // Unlimited BP

  PLAYMAP_NUM_FLAGS
} playMapFlag_t;


/*
 * PLAYMAP QUEUE
 */

#define PLAYMAP_QUEUE_ADD(x,y)  ( ( ( x ) + ( y ) ) % MAX_PLAYMAP_POOL_ENTRIES )
#define PLAYMAP_QUEUE_PLUS1(x)  PLAYMAP_QUEUE_ADD(x,1)
#define PLAYMAP_QUEUE_MINUS1(x) PLAYMAP_QUEUE_ADD(x,-1)
#define PLAYMAP_QUEUE_IS_EMPTY  ( G_GetPlayMapQueueLength() < 1 )
#define PLAYMAP_QUEUE_IS_FULL  ( G_GetPlayMapQueueLength() >= ( MAX_PLAYMAP_POOL_ENTRIES - 1 ) )

// individual playmap entry in the queue
typedef struct playMap_s
{
  char *mapname;
  char *layout;

  gclient_t *client;

  playMapFlag_t plusFlags[ PLAYMAP_NUM_FLAGS ];
  playMapFlag_t minusFlags[ PLAYMAP_NUM_FLAGS ];
} playMap_t;

// playmap queue/playlist
typedef struct playMapQueue_s
{
  playMap_t playMap[ MAX_PLAYMAP_POOL_ENTRIES ];

  int head, tail, numEntries;
} playMapQueue_t;


/*
 * PLAYMAP ERRORS
 */

// error codes
typedef enum playMapErrorCode_s
{
  // this item must always be the first in the enum list
  PLAYMAP_ERROR_NONE,

  // list of error codes (order does not matter)
  PLAYMAP_ERROR_CONFIG_UNREADABLE,
  PLAYMAP_ERROR_NO_CONFIG,
  PLAYMAP_ERROR_MAP_POOL_FULL,
  PLAYMAP_ERROR_MAP_ALREADY_IN_POOL,
  PLAYMAP_ERROR_MAP_NOT_FOUND,
  PLAYMAP_ERROR_LAYOUT_NOT_FOUND,
  PLAYMAP_ERROR_MAP_NOT_IN_POOL,
  PLAYMAP_ERROR_MAP_NOT_IN_QUEUE,
  PLAYMAP_ERROR_MAP_ALREADY_IN_QUEUE,
  PLAYMAP_ERROR_USER_ALREADY_IN_QUEUE,
  PLAYMAP_ERROR_MAP_QUEUE_EMPTY,
  PLAYMAP_ERROR_MAP_QUEUE_FULL,

  // an unknown error
  PLAYMAP_ERROR_UNKNOWN,

  // this item must always be the last in the enum list
  PLAYMAP_NUM_ERRORS,
} playMapErrorCode_t;

// error messages
typedef struct playMapError_s
{
  int errorCode;
  char *errorMessage;
} playMapError_t;


/*
 * PLAYMAP PROTOTYPES
 */

playMapError_t G_PlayMapErrorByCode( int errorCode );
playMapError_t G_AddToPlayMapPool( char *mapname );
playMapError_t G_RemoveFromPlayMapPool( char *mapname );
playMapError_t G_SavePlayMapPool( void );
playMapError_t G_ReloadPlayMapPool( void );
playMapError_t G_ClearPlayMapPool( void );
int G_FindInMapPool( char *mapname );
void G_InitPlayMapQueue( void );
int G_GetPlayMapQueueLength( void );
qboolean G_PlayMapQueueIsFull( void );
playMapFlag_t G_ParsePlayMapFlag(char *flag);
playMapError_t G_PlayMapEnqueue( char *mapname, char *layout, gclient_t *client, char *flags );
playMap_t *G_PopFromPlayMapQueue( void );
playMapError_t G_RemoveFromPlayMapQueue( int index );
int G_GetPlayMapQueueIndexByMapName( char *mapname );
int G_GetPlayMapQueueIndexByClient( gclient_t *client );
void G_PrintPlayMapQueue( gentity_t *ent );
