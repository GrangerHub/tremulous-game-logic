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

// server's map pool cache
static playMapPool_t playMapPoolCache;

// current map queue
static playMapQueue_t playMapQueue;


/*
 * playmap utility functions
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
 * map pool utility functions
 */ 

/*
================
G_AddToPlayMapPool

Add a map to the current pool of maps available.
================
*/
playMapError_t G_AddToPlayMapPool( char *mapname )
{
  // TODO: code
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_FOUND );
}

/*
================
G_RemoveFromPlayMapPool

Remove a map from the current pool of maps available.
================
*/
playMapError_t G_RemoveFromPlayMapPool( char *mapname )
{
  // TODO: code
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_IN_POOL );
}

/*
================
G_SavePlayMapPool

Save map pool to configuration file
================
*/
qboolean G_SavePlayMapPool( void )
{
  // TODO: code
  return qfalse;
}

/*
================
G_ReloadPlayMapPool

Reload map pool from configuration file
================
*/
qboolean G_ReloadPlayMapPool( void )
{
  // TODO: code
  return qfalse;
}

/*
================
G_ClearPlayMapPool

Clear cached map pool
================
*/

qboolean G_ClearPlayMapPool( void )
{
  // TODO: code
  return qfalse;
}

void G_ParsePlayMapFlag(char *flags, playMapFlag_t **plusFlags, playMapFlag_t **minusFlags)
{
  int i;
  char *token;

  // Loop through
  for( i = 0; ; i++ )
  {
    token = COM_Parse( &flags );

    if( !token[0] )
      break;

    switch( token[0] )
    {
      case '+':
        if( strlen( token ) > 1 )
        {
          G_Printf( "G_AddToPlayMapQueue: PLUS flag '%s'\n", token + 1 );
          // TODO: Add to plusFlags
        }
        else
          G_Printf( "G_AddToPlayMapQueue: PLUS flag truncated (ignored)\n" );
        break;
      case '-':
        if( strlen( token ) > 1 )
        {
          G_Printf( "G_AddToPlayMapQueue: MINUS flag '%s'\n", token + 1 );
          // TODO: Add to minusFlags
        }
        else
          G_Printf( "G_AddToPlayMapQueue: MINUS flag truncated (ignored)\n" );
        break;
    }
  }
}

/*
================
G_InitPlayMapQueue

Initialize the playmap queue. Should only be run once.
================
*/
void G_InitPlayMapQueue( void )
{
  int i;

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
    playMapQueue.playMap[ i ].plusFlags  = NULL;
    playMapQueue.playMap[ i ].minusFlags = NULL;
  }
}

qboolean G_PlayMapQueueIsEmpty( void )
{
  return ( playMapQueue.tail == playMapQueue.head ) &&
    ( playMapQueue.playMap[ playMapQueue.tail ].mapname == NULL &&
      playMapQueue.playMap[ playMapQueue.tail ].layout == NULL &&
      playMapQueue.playMap[ playMapQueue.tail ].client == NULL );
}

qboolean G_PlayMapQueueIsFull( void )
{
  return ( playMapQueue.tail == playMapQueue.head ) &&
    ( playMapQueue.playMap[ playMapQueue.tail ].mapname != NULL &&
      playMapQueue.playMap[ playMapQueue.tail ].layout != NULL &&
      playMapQueue.playMap[ playMapQueue.tail ].client != NULL );
}

/*
================
G_AddToPlayMapQueue

Enqueue a player requested map in the playmap queue.
================
*/
playMapError_t G_AddToPlayMapQueue( char *mapname, char *layout, gclient_t *client, char *flags )
{

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
