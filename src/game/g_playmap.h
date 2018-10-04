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

/*
 * PLAYMAP POOL
 */

#include "g_public.h"

#define MAX_PLAYMAP_POOL_ENTRIES 128
#define PLAYMAP_INACTIVE 0

#define MAX_PLAYMAP_MAPNAME 32
#define MAX_MAPLIST_MAPS 256
#define MAX_MAPLIST_ROWS 9

// Map types
#define PLAYMAP_MAPTYPE_NONE 	"NONE"
#define PLAYMAP_MAPTYPE_ATCS 	"ATCS"
#define PLAYMAP_MAPTYPE_MISSION	"MISSION"

// map pool entry for the playmap system
typedef struct playMapPoolEntry_s
{
  char *mapName;		/* Name of map */
  char *mapType;		/* Type of map like ATCS, NIVEUS, etc */
  
  int  minClients, maxClients; /* Min, max number of clients for map */
} playMapPoolEntry_t;

// map pool for the playmap system
typedef struct playMapPool_s
{
  playMapPoolEntry_t mapEntries[ MAX_PLAYMAP_POOL_ENTRIES ];

  int  numMaps;
} playMapPool_t;


/*
 * PLAYMAP FLAGS
 */

// available flags for playmap (hint: these are just examples for a
// proof-of-concept demo. they are very likely to change)
typedef enum
{
  PLAYMAP_FLAG_NONE = 0,

  PLAYMAP_FLAG_DPUNT, // Dretch Punt
  PLAYMAP_FLAG_FF,    // Friendly Fire
  PLAYMAP_FLAG_FBF,   // Friendly Buildable Fire
  PLAYMAP_FLAG_SD,    // Sudden Death
  PLAYMAP_FLAG_LGRAV, // Low Gravity
  PLAYMAP_FLAG_UBP,   // Unlimited BP
  PLAYMAP_FLAG_PORTAL,// Portal Gun
  PLAYMAP_FLAG_STACK,// Buildable stacking

  PLAYMAP_NUM_FLAGS
} playMapFlag_t;

#define PlaymapFlag_Set(X, FLAG) ((X) |= (1 << (FLAG - 1)))
#define PlaymapFlag_IsSet(X, FLAG) ((X) & (1 << (FLAG - 1)))
#define PlaymapFlag_Clear(X, FLAG) ((X) &= ~(1 << (FLAG - 1)))

// flag names
typedef struct playMapFlagDesc_s
{
  int 	   flag;		/* Flag bit */
  char 	   *flagName;		/* String to parse */
  qboolean defVal;		/* Whether flag is on by default */
  qboolean avail;		/* Whether flag is available for users. 
				   If not, default value is ignored as well. */
  char 	   *flagDesc;		/* Description string */
} playMapFlagDesc_t;

/*
 * PLAYMAP QUEUE
 */

#define PLAYMAP_QUEUE_ADD(x,y)  ( ( ( x ) + ( y ) ) % MAX_PLAYMAP_QUEUE_ENTRIES )
#define PLAYMAP_QUEUE_PLUS1(x)  PLAYMAP_QUEUE_ADD(x,1)
#define PLAYMAP_QUEUE_MINUS1(x) PLAYMAP_QUEUE_ADD(x,-1)
#define PLAYMAP_QUEUE_IS_EMPTY  ( G_GetPlayMapQueueLength() < 1 )
#define PLAYMAP_QUEUE_IS_FULL  ( G_GetPlayMapQueueLength() >= ( MAX_PLAYMAP_QUEUE_ENTRIES - 1 ) )

#define FREE_IF_NOT_NULL(x)  { if( ( x ) ) { BG_Free( x ); x = NULL; } }

// playmap queue/playlist
typedef struct playMapQueue_s
{
  playMap_t playMap[ MAX_PLAYMAP_QUEUE_ENTRIES ];

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
  PLAYMAP_ERROR_POOL_CONFIG_UNREADABLE,
  PLAYMAP_ERROR_NO_POOL_CONFIG,
  PLAYMAP_ERROR_MAP_POOL_FULL,
  PLAYMAP_ERROR_MAP_POOL_EMPTY,
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
playMapError_t G_AddToPlayMapPool( char *mapName, char *mapType, int minClients,
				   int maxClients, qboolean sortPool );
playMapError_t G_RemoveFromPlayMapPool( char *mapName );
playMapError_t G_SavePlayMapPool( void );
void PlayMapPoolMessage( int client );
void SendPlayMapPoolMessageToAllClients( void );
playMapError_t G_ReloadPlayMapPool( void );
playMapError_t G_ClearPlayMapPool( void );
int G_FindInMapPool( char *mapName );
void G_SortPlayMapPool( void );
int G_GetPlayMapPoolLength( void );
void G_PrintPlayMapPool( gentity_t *ent, int page, qboolean isJson );
void G_InitPlayMapQueue( void );
playMapError_t G_SavePlayMapQueue( void );
playMapError_t G_ReloadPlayMapQueue( void );
gclient_t *G_FindClientByName(gentity_t *from, const char *netname);
int G_GetPlayMapQueueLength( void );
qboolean G_PlayMapQueueIsFull( void );
playMapFlag_t G_ParsePlayMapFlag( gentity_t *ent, char *flag );
playMapError_t G_PlayMapEnqueue( char *mapName, char *layout, char *clientName, char *flags, gentity_t *ent );
qboolean G_PopFromPlayMapQueue( playMap_t *playMap );
playMapError_t G_RemoveFromPlayMapQueue( int index );
int G_GetPlayMapQueueIndexByMapName( char *mapName );
int G_GetPlayMapQueueIndexByClient( char *clientName );
void G_PrintPlayMapQueue( gentity_t *ent );
qboolean G_PlayMapActive( void );
void G_NextPlayMap( void );
int G_ParsePlayMapFlagTokens( gentity_t *ent, char *flags );
char *G_PlayMapFlags2String( int flags );
int G_DefaultPlayMapFlags(void);
void G_ExecutePlaymapFlags( int flagsValue );
