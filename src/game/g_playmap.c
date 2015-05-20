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

void trap_FS_Read( void *buffer, int len, fileHandle_t f );
void trap_FS_Write( const void *buffer, int len, fileHandle_t f );

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
    PLAYMAP_ERROR_UNKNOWN,               /* errorCode */
    "an unknown error has occured"
  }
};

// server's map pool cache
playMapPool_t playMapPoolCache;

// current map queue
playMapQueue_t playMapQueue;


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
================
G_AddToMapPool

Add a map to the current pool of maps available.
================
*/
playMapError_t G_AddToMapPool( char *mapname )
{
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_FOUND );
}

/*
================
G_RemoveFromMapPool

Remove a map from the current pool of maps available.
================
*/
playMapError_t G_RemoveFromMapPool( char *mapname )
{
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_IN_POOL );
}

/*
================
G_AddToMapQueue

Add a user requested map to the playmap queue.
================
*/
playMapError_t G_AddToMapQueue( char *mapname, char *layout, gclient_t *client )
{
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_ALREADY_IN_QUEUE );
}

/*
================
G_AddToMapQueue

Add a user requested map to the playmap queue.
================
*/
playMapError_t G_RemoveFromMapQueue( int index )
{
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_IN_QUEUE );
}

/*
================
G_GetQueueIndexByMapName

Get the index of the map in the queue from the mapname.
================
*/
int G_GetQueueIndexByMapName( char *mapname )
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
G_GetQueueIndexByClient

Get the index of the map in the queue from the mapname.
================
*/
int G_GetQueueIndexByClient( gclient_t *client )
{
  int i;

  for( i = 0; i < sizeof( playMapQueue.playMap ); i++ )
  {
    if( sizeof( playMapQueue.playMap[ i ] ) > 0 &&
        playMapQueue.playMap[ i ].client == client )
      return i;
  }

  // map was not found in the queue
  return -1;
}
