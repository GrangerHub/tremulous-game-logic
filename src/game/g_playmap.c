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

#include "g_local.h"


/*
 * external utilities
 */

void trap_FS_Read( void *buffer, int len, fileHandle_t f );
void trap_FS_Write( const void *buffer, int len, fileHandle_t f );


/*
 * globals
 */

// list of error codes and messages
static const playMapError_t playMapError[ ] =
{
  {
    PLAYMAP_ERROR_NONE,                 /* errorCode */
    ""
  },
  {
    PLAYMAP_ERROR_MAP_ALREADY_IN_POOL,   /* errorCode */
    "the map you requested was not found on the server"
  },
  {
    PLAYMAP_ERROR_MAP_NOT_FOUND,         /* errorCode */
    "the map you requested was not found on the server"
  },
  {
    PLAYMAP_ERROR_LAYOUT_NOT_FOUND,      /* errorCode */
    "the layout you requested for the map was not found on the server"
  },
  {
    PLAYMAP_ERROR_MAP_NOT_IN_POOL,       /* errorCode */
    "the map you requested is not currently in the map pool"
  },
  {
    PLAYMAP_ERROR_MAP_NOT_IN_QUEUE,      /* errorCode */
    "the map you specified is not on the playlist"
  },
  {
    PLAYMAP_ERROR_MAP_ALREADY_IN_QUEUE,  /* errorCode */
    "the map you requested is already on the playlist"
  },
  {
    PLAYMAP_ERROR_USER_ALREADY_IN_QUEUE, /* errorCode */
    "you already have a map queued on the playlist"
  },
  {
    PLAYMAP_ERROR_MAP_QUEUE_EMPTY,       /* errorCode */
    "the map queue is empty"
  },
  {
    PLAYMAP_ERROR_MAP_QUEUE_FULL,        /* errorCode */
    "the map queue is full"
  },
  {
    PLAYMAP_ERROR_UNKNOWN,               /* errorCode */
    "an unknown error has occured"
  }
};


/*
 * playmap error utility functions
 */

/*
================
G_PlayMapErrorByCode

Return a playmap error from its errorCode.
================
 */
playMapError_t G_PlayMapErrorByCode( int errorCode )
{
  int i;

  for( i = 0; i < PLAYMAP_NUM_ERRORS; i++ )
  {
    if( playMapError[ i ].errorCode == errorCode )
      return playMapError[ i ];
  }

  // return unknown error if the error code specified was not found
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_UNKNOWN );
}


/*
 * playmap pool utility functions
 */ 

// server's map pool cache
static playMapPool_t playMapPoolCache;

/*
================
G_AddToPlayMapPool

Add a map to the current pool of maps available.
================
*/
playMapError_t G_AddToPlayMapPool( char *mapname )
{
  if( G_FindInMapPool( mapname ) >= 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_ALREADY_IN_POOL );

  if( playMapPoolCache.numMaps >= MAX_PLAYMAP_POOL_ENTRIES )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_POOL_FULL );

  playMapPoolCache.maps[ playMapPoolCache.numMaps ] = G_CopyString( mapname );

  playMapPoolCache.numMaps++;

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_RemoveFromPlayMapPool

Remove a map from the current pool of maps available.
================
*/
playMapError_t G_RemoveFromPlayMapPool( char *mapname )
{
  int i, mapIndex;

  mapIndex = G_FindInMapPool( mapname );

  if( mapIndex < 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_IN_POOL );

  // remove the matching map pool entry
  BG_Free( playMapPoolCache.maps[ mapIndex ] );
  playMapPoolCache.maps[ mapIndex ] = NULL;

  for( i = mapIndex; i < playMapPoolCache.numMaps; i++ )
  {
    // safeguard against going beyond array bounds
    if( i >= MAX_PLAYMAP_POOL_ENTRIES )
      continue;

    // shift next entry in the map pool down by one (close the gap)
    playMapPoolCache.maps[ i ] = playMapPoolCache.maps[ i + 1 ];

    if( i == ( playMapPoolCache.numMaps - 1 ) )
      playMapPoolCache.maps[ i ] = NULL;
  }

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_SavePlayMapPool

Save map pool to configuration file
================
*/
void G_SavePlayMapPool( void )
{
  fileHandle_t f;
  int i;

  if( !g_playMapConfig.string[ 0 ] )
  {
    G_Printf( S_COLOR_YELLOW "WARNING: g_playMapConfig is not set. "
        " playmap pool configuration cannot be saved.\n" );
    return;
  }

  if( trap_FS_FOpenFile( g_playMapConfig.string, &f, FS_WRITE ) < 0 )
  {
    G_Printf( "G_SavePlayMapPool: could not open g_playMapConfig file \"%s\"\n",
        g_playMapConfig.string );
    return;
  }

  for( i = 0; i < playMapPoolCache.numMaps; i++ )
  {
    // Skip entry if it is somehow a null pointer (should not happen)
    if( !playMapPoolCache.maps[ i ] )
      continue;

    trap_FS_Write( playMapPoolCache.maps[ i ], strlen( playMapPoolCache.maps[ i ] ), f );
    trap_FS_Write( "\n", 1, f );
  }

  trap_FS_FCloseFile( f );
}

/*
================
G_ReloadPlayMapPool

Reload map pool from configuration file
================
*/
void G_ReloadPlayMapPool( void )
{
  fileHandle_t f;
  int len;
  char *cnf, *cnf2, *mapname;

  G_ClearPlayMapPool();

  // read playmap config file
  len = trap_FS_FOpenFile( g_playMapConfig.string, &f, FS_READ );
  if( len < 0 )
  {
    G_Printf( "^3G_ReloadPlayMapPool: ^7could not open playmap config file %s\n",
            g_playMapConfig.string );

    return;
  }
  cnf = BG_Alloc( len + 1 );
  cnf2 = cnf;
  trap_FS_Read( cnf, len, f );
  cnf[ len ] = '\0';
  trap_FS_FCloseFile( f );

  COM_BeginParseSession( g_playMapConfig.string );
  while( 1 )
  {
    mapname = COM_Parse( &cnf );
    if( !*mapname )
      break;

    G_AddToPlayMapPool( mapname );
  }
  BG_Free( cnf2 );
  G_Printf( "^3G_ReloadPlayMapPool: ^7loaded %d maps\n",
        playMapPoolCache.numMaps );
}

/*
================
G_ClearPlayMapPool

Clear cached map pool
================
*/

void G_ClearPlayMapPool( void )
{
  int i;

  for( i = 0; i < MAX_PLAYMAP_POOL_ENTRIES; i++ )
  {
    // skip entry if it is already a null pointer
    if( playMapPoolCache.maps[ i ] == NULL )
      continue;

    // free the space taken up by the slot
    BG_Free( playMapPoolCache.maps[ i ] );
    playMapPoolCache.maps[ i ] = NULL;
  }

  playMapPoolCache.numMaps = 0;
}

/*
================
G_FindInMapPool

Check if map is in playmap pool
================
*/

int G_FindInMapPool( char *mapname )
{
  int i;

  for( i = 0; i < playMapPoolCache.numMaps; i++ )
  {
    // Skip entry if it is somehow null (should not happen)
    if( playMapPoolCache.maps[ i ] == NULL )
      continue;

    // found a match. return the array index
    if( Q_stricmp(playMapPoolCache.maps[ i ], mapname) == 0 )
      return i;
  }

  // not found
  return -1;
}


/*
 * playmap queue utility functions
 */ 

// current map queue
static playMapQueue_t playMapQueue;

/*
================
G_InitPlayMapQueue

Initialize the playmap queue. Should only be run once.
================
*/
void G_InitPlayMapQueue( void )
{
  int i, j;

  // Reset everything
  playMapQueue.numEntries =
    playMapQueue.tail =
    playMapQueue.head = 0;

  for( i = 0; i < MAX_PLAYMAP_POOL_ENTRIES; i++ )
  {
    // set all values/pointers to NULL
    playMapQueue.playMap[ i ].mapname    = NULL;
    playMapQueue.playMap[ i ].layout     = NULL;
    playMapQueue.playMap[ i ].client     = NULL;
    for( j = 0; j < PLAYMAP_NUM_FLAGS; j++ )
    {
      playMapQueue.playMap[ i ].plusFlags[ j ] = PLAYMAP_FLAG_NONE;
      playMapQueue.playMap[ i ].minusFlags[ j ] = PLAYMAP_FLAG_NONE;
    }
  }
}

/*
================
G_PlayMapQueueIsEmpty

Returns true if the playmap queue is currently empty.
================
*/

qboolean G_PlayMapQueueIsEmpty( void )
{
  return ( playMapQueue.tail == playMapQueue.head ) &&
    ( playMapQueue.playMap[ playMapQueue.tail ].mapname == NULL &&
      playMapQueue.playMap[ playMapQueue.tail ].layout == NULL &&
      playMapQueue.playMap[ playMapQueue.tail ].client == NULL );
}

/*
================
G_PlayMapQueueIsFull

Returns true if the playmap queue is currently full.
================
*/

qboolean G_PlayMapQueueIsFull( void )
{
  return ( playMapQueue.tail == playMapQueue.head ) &&
    ( playMapQueue.playMap[ playMapQueue.tail ].mapname != NULL &&
      playMapQueue.playMap[ playMapQueue.tail ].layout != NULL &&
      playMapQueue.playMap[ playMapQueue.tail ].client != NULL );
}

/*
================
G_ParsePlayMapFlag

Parses a playmap flag string and returns the playMapFlag_t equivalent.
================
*/

playMapFlag_t G_ParsePlayMapFlag(char *flag)
{
  if( Q_stricmpn( flag, "dpunt", 5 ) == 0 )
    return PLAYMAP_FLAG_DPUNT;

  if( Q_stricmpn( flag, "ff", 2 ) == 0 )
    return PLAYMAP_FLAG_FF;

  if( Q_stricmpn( flag, "fbf", 3 ) == 0 )
    return PLAYMAP_FLAG_FBF;

  if( Q_stricmpn( flag, "sd", 2 ) == 0 )
    return PLAYMAP_FLAG_SD;

  if( Q_stricmpn( flag, "lgrav", 5 ) == 0 )
    return PLAYMAP_FLAG_LGRAV;

  if( Q_stricmpn( flag, "ubp", 3 ) == 0 )
    return PLAYMAP_FLAG_UBP;

  return PLAYMAP_FLAG_NONE;
}

/*
================
G_AddToPlayMapQueue

Enqueue a player requested map in the playmap queue.
================
*/

playMapError_t G_AddToPlayMapQueue( char *mapname, char *layout, gclient_t
    *client, char *flags )
{
  int       i;
  int       plusFlagIdx = 0, minusFlagIdx = 0;
  char      *token;
  playMap_t playMap;
  playMapFlag_t playMapFlag;

  if( G_PlayMapQueueIsFull() )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_QUEUE_FULL );

  // copy mapname
  playMap.mapname = G_CopyString( mapname );

  // copy layout
  playMap.layout = G_CopyString( layout );

  // copy client pointer
  playMap.client = client;

  // Loop through flags
  for( i = 0; ; i++ )
  {
    token = COM_Parse( &flags );

    if( !token[0] )
      break;

    switch( token[0] )
    {
      case '+':
        if( ( strlen( token ) > 1 ) )
        {
          playMapFlag = G_ParsePlayMapFlag( token + 1 );

          if ( playMapFlag != PLAYMAP_FLAG_NONE )
            playMap.plusFlags[ plusFlagIdx++ ] = playMapFlag;
        }
        break;
      case '-':
        if( ( strlen( token ) > 1 ) )
        {
          playMapFlag = G_ParsePlayMapFlag( token + 1 );

          if ( playMapFlag != PLAYMAP_FLAG_NONE )
            playMap.minusFlags[ minusFlagIdx++ ] = playMapFlag;
        }
        break;
    }
  }

  playMapQueue.playMap[ playMapQueue.tail ] = playMap;
  playMapQueue.tail = MAP_QUEUE_PLUS1( playMapQueue.tail );
  playMapQueue.numEntries++;

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_RemoveFromPlayMapQueue

Dequeue a player requested map from the playmap queue.
================
*/
playMapError_t G_RemoveFromPlayMapQueue( int index )
{
  // TODO: code

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_IN_QUEUE );
}

/*
================
G_GetPlayMapQueueIndexByMapName

Get the index of the map in the queue from the mapname.
================
*/
int G_GetPlayMapQueueIndexByMapName( char *mapname )
{
  int i;

  for( i = 0; i < sizeof( playMapQueue.playMap ); i++ )
  {
    if( sizeof( playMapQueue.playMap[ i ].mapname ) &&
        Q_stricmp( playMapQueue.playMap[ i ].mapname, mapname ) == 0 )
      return i;
  }

  // map was not found in the queue
  return -1;
}

/*
================
G_GetPlayMapQueueIndexByClient

Get the index of the map in the queue from the client that requested it.
================
*/
int G_GetPlayMapQueueIndexByClient( gclient_t *client )
{
  int i;

  for( i = 0; i < sizeof( playMapQueue.playMap ); i++ )
  {
    if( sizeof( playMapQueue.playMap[ i ] ) > 0 &&
        playMapQueue.playMap[ i ].client == client )
      return i;
  }

  // client's map was not found in the queue
  return -1;
}
