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
       PLAYMAP_ERROR_MAP_NONEXISTANT,         /* errorCode */
      "cannot find map."
  },
  {
    PLAYMAP_ERROR_POOL_CONFIG_UNREADABLE,    /* errorCode */
    "playmap pool config file specified by g_playMapPoolConfig is not readable."
  },
  {
    PLAYMAP_ERROR_NO_POOL_CONFIG,            /* errorCode */
    "g_playMapPoolConfig is not set. playmap pool configuration cannot be saved."
  },
  {
    PLAYMAP_ERROR_MAP_POOL_FULL,         /* errorCode */
    "the map pool is currently full"
  },
  {
    PLAYMAP_ERROR_MAP_ALREADY_IN_POOL,   /* errorCode */
    "that map you requested is already in the map pool"
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
    PLAYMAP_ERROR_NO_QUEUE_CONFIG,            /* errorCode */
    "g_playMapQueueConfig is not set. playmap queue configuration cannot be saved."
  },
  {
    PLAYMAP_ERROR_QUEUE_CONFIG_UNREADABLE,    /* errorCode */
    "playmap queue config file specified by g_playMapQueueConfig is not readable."
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

  // decrease numMaps by one count
  playMapPoolCache.numMaps--;

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_SavePlayMapPool

Save map pool to configuration file
================
*/
playMapError_t G_SavePlayMapPool( void )
{
  fileHandle_t f;
  int i;

  if( !g_playMapPoolConfig.string[ 0 ] )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_NO_POOL_CONFIG );

  if( trap_FS_FOpenFile( g_playMapPoolConfig.string, &f, FS_WRITE ) < 0 )
  {
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_POOL_CONFIG_UNREADABLE );
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

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_ReloadPlayMapPool

Reload map pool from configuration file
================
*/
playMapError_t G_ReloadPlayMapPool( void )
{
  playMapError_t playMapError;
  fileHandle_t f;
  int len;
  char *cnf, *cnf2, *mapname;

  if( !g_playMapPoolConfig.string[ 0 ] )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_NO_POOL_CONFIG );

  playMapError = G_ClearPlayMapPool();
  if( playMapError.errorCode != PLAYMAP_ERROR_NONE )
    return playMapError;

  // read playmap config file
  len = trap_FS_FOpenFile( g_playMapPoolConfig.string, &f, FS_READ );
  if( len < 0 )
  {
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_POOL_CONFIG_UNREADABLE );
  }

  cnf = BG_Alloc( len + 1 );
  cnf2 = cnf;
  trap_FS_Read( cnf, len, f );
  cnf[ len ] = '\0';
  trap_FS_FCloseFile( f );

  COM_BeginParseSession( g_playMapPoolConfig.string );
  while( 1 )
  {
    mapname = COM_Parse( &cnf );
    if( !*mapname )
      break;
    G_AddToPlayMapPool( mapname );
  }
  BG_Free( cnf2 );

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_ClearPlayMapPool

Clear cached map pool
================
*/

playMapError_t G_ClearPlayMapPool( void )
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

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
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
  playMapQueue.tail = playMapQueue.head = 0;
  playMapQueue.tail = PLAYMAP_QUEUE_MINUS1( playMapQueue.tail );
  playMapQueue.numEntries = 0;

  for( i = 0; i < MAX_PLAYMAP_POOL_ENTRIES; i++ )
  {
    // set all values/pointers to NULL
    playMapQueue.playMap[ i ].mapname = NULL;
    playMapQueue.playMap[ i ].layout  = NULL;
    playMapQueue.playMap[ i ].clientname  = NULL;

    for( j = 0; j < PLAYMAP_NUM_FLAGS; j++ )
    {
      playMapQueue.playMap[ i ].plusFlags[ j ]  = PLAYMAP_FLAG_NONE;
      playMapQueue.playMap[ i ].minusFlags[ j ] = PLAYMAP_FLAG_NONE;
    }
  }
}

/*
================
G_SavePlayMapQueue

Save the playmap queue into a config file. Should be run before
switching maps for the queue to be persistent across map changes.
================
*/
playMapError_t G_SavePlayMapQueue( void )
{
  fileHandle_t f;
  int i;

  if( !g_playMapQueueConfig.string[ 0 ] )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_NO_QUEUE_CONFIG );

  if( trap_FS_FOpenFile( g_playMapQueueConfig.string, &f, FS_WRITE ) < 0 )
  {
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_QUEUE_CONFIG_UNREADABLE );
  }

  for( i = 0; i < G_GetPlayMapQueueLength(); i++ )
  {
    playMap_t playMap =
      playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i) ];

    trap_FS_Write( playMap.mapname, strlen( playMap.mapname ), f );
    if( playMap.clientname )
    {
	 trap_FS_Write( " ", 1, f );
	 trap_FS_Write( playMap.clientname,
			strlen( playMap.clientname ), f );
	 if( playMap.layout )
	 {
	      trap_FS_Write( " ", 1, f );
	      trap_FS_Write( playMap.layout, strlen( playMap.layout ), f );
	 }
	 // TODO: also save flags here
    }
    trap_FS_Write( "\n", 1, f );
  }

  trap_FS_FCloseFile( f );

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_ReloadPlayMapQueue

Load the playmap queue from config file. Should be run after
switching maps for the queue to be persistent across map changes.
================
*/
playMapError_t G_ReloadPlayMapQueue( void )
{
  fileHandle_t f;
  int len;
  char *cnf, *cnf2, *mapname, empty = 0,
    *layout = &empty, *clientname = &empty, *flags = &empty;
  
  if( !g_playMapQueueConfig.string[ 0 ] )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_NO_QUEUE_CONFIG );

  // TODO: potential memory leak of old queue items
  G_InitPlayMapQueue();
  
  // read playmap config file
  len = trap_FS_FOpenFile( g_playMapQueueConfig.string, &f, FS_READ );
  if( len < 0 )
  {
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_QUEUE_CONFIG_UNREADABLE );
  }

  cnf = BG_Alloc( len + 1 );
  cnf2 = cnf;
  trap_FS_Read( cnf, len, f );
  cnf[ len ] = '\0';
  trap_FS_FCloseFile( f );

  trap_Print( va( "DEBUG: before reading maps file of length %d\n", len ) );
  COM_BeginParseSession( g_playMapQueueConfig.string );
  while( cnf < (cnf2 + len) ) // rows
  {
    while ( cnf < (cnf2 + len) ) // map arguments
      {
	trap_Print( va( "DEBUG: reading maps at char index %ld\n", cnf - cnf2 ) );
	// Must use G_CopyString to prevent overwrites by parser
	mapname = G_CopyString(COM_ParseExt( &cnf, qfalse ));
	if( *mapname )
	  {
	    trap_Print( va( "DEBUG: found map %s\n", mapname ) );

	    clientname = G_CopyString(COM_ParseExt( &cnf, qfalse ));
	    if( *clientname )
	      {
		layout = COM_ParseExt( &cnf, qfalse );
		if( *layout )
		  {
		    layout = G_CopyString(layout);
		    flags = COM_ParseExt( &cnf, qfalse );
		    if( *flags )
		      {
			flags = G_CopyString(flags);
			// TODO: parse flags here
			break;
		      }
		    else break;
		  }
		else break;
	      }
	    else break;
	  }
	else break;
      }	   
    if( !*mapname )
      break; // end of all maps
    
    trap_Print( va( "DEBUG: map=%s\n"
		    "       client=%s\n" 
		    "       layout=%s\n"
		    "       flags=%s\n",
		    mapname, clientname, layout, flags ) );

    G_PlayMapEnqueue( mapname, layout, clientname, flags );

    // G_PlayMapEnqueue also copies the strings, so release the
    // originals here
    if( *mapname ) BG_Free(mapname);
    if( *clientname ) BG_Free(clientname);
    if( *layout ) BG_Free(layout);
    if( *flags ) BG_Free(layout);

    trap_Print( "DEBUG: enqueued\n" );
  }
  BG_Free( cnf2 );

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_FindClientByName

Find client from given netname.
TODO: May need to be in g_utils.c. Also, no longer used here.
================
*/

gclient_t *G_FindClientByName(gentity_t *from, const char *netname)
{
  // Copied from g_utils.c:G_Find()
  if( !from )
    from = g_entities;
  else
    from++;

  for( ; from < &g_entities[ level.num_entities ]; from++ )
  {
    if( !from->inuse || !from->client
	|| !from->client->pers.netname )
      continue;

    if( !Q_stricmp( from->client->pers.netname, netname ) )
      return from->client;
  }

  return NULL;
}

/*
================
G_GetPlayMapQueueLength

Returns the length of the playmap queue.
================
*/

int G_GetPlayMapQueueLength( void )
{
  int length = playMapQueue.tail - playMapQueue.head + 1;

  while( length < 0 )
    length += MAX_PLAYMAP_POOL_ENTRIES;

  while( length >= MAX_PLAYMAP_POOL_ENTRIES )
    length -= MAX_PLAYMAP_POOL_ENTRIES;

  return length;
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
G_PlayMapEnqueue

Enqueue a player requested map in the playmap queue (add to the back of the
queue).
================
*/

playMapError_t G_PlayMapEnqueue( char *mapname, char *layout, char
    *clientname, char *flags )
{
  int       i;
  int       plusFlagIdx = 0, minusFlagIdx = 0;
  char      *token;
  playMap_t playMap;
  playMapFlag_t playMapFlag;

  if( ! G_MapExists( mapname ) )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NONEXISTANT );
  
  if( PLAYMAP_QUEUE_IS_FULL )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_QUEUE_FULL );

  // check if user already has a map queued
  if( G_GetPlayMapQueueIndexByClient( clientname ) >= 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_USER_ALREADY_IN_QUEUE );

  // check if map has already been queued by someone else
  if( G_GetPlayMapQueueIndexByMapName( mapname ) >= 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_ALREADY_IN_QUEUE );

  // copy mapname
  playMap.mapname = G_CopyString(mapname);

  // copy layout
  if( *layout )
    playMap.layout = G_CopyString(layout);
  else
    playMap.layout = NULL;

  // copy clientname
  if( *clientname )
    playMap.clientname = G_CopyString(clientname);
  else
    playMap.clientname = NULL;

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

  playMapQueue.tail = PLAYMAP_QUEUE_PLUS1( playMapQueue.tail );
  playMapQueue.playMap[ playMapQueue.tail ] = playMap;
  playMapQueue.numEntries++;

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_PopFromPlayMapQueue

Pop a map from the head of playmap queue (dequeue) and return to function
caller.
================
*/
playMap_t *G_PopFromPlayMapQueue( void )
{
  playMap_t *playMap;
  int i;

  // return null pointer if queue is empty
  if( PLAYMAP_QUEUE_IS_EMPTY )
  {
    return (playMap_t *) NULL;
  }

  playMap = BG_Alloc( sizeof( playMap_t ) );

  // copy values from playmap in the head of the queue
  playMap->mapname = playMapQueue.playMap[ playMapQueue.head ].mapname;
  playMap->layout = playMapQueue.playMap[ playMapQueue.head ].layout;
  playMap->clientname = playMapQueue.playMap[ playMapQueue.head ].clientname;
  for( i = 0; i < PLAYMAP_NUM_FLAGS; i++ )
  {
    playMap->plusFlags[ i ]
      = playMapQueue.playMap[ playMapQueue.head ].plusFlags[ i ];
    playMap->minusFlags[ i ]
      = playMapQueue.playMap[ playMapQueue.head ].minusFlags[ i ];
  }

  // reset the playmap in the head of the queue to null values
  playMapQueue.playMap[ playMapQueue.head ].mapname = NULL;
  playMapQueue.playMap[ playMapQueue.head ].layout = NULL;
  playMapQueue.playMap[ playMapQueue.head ].clientname = NULL;
  for( i = 0; i < PLAYMAP_NUM_FLAGS; i++ )
  {
    playMapQueue.playMap[ playMapQueue.head ].plusFlags[ i ] =
      PLAYMAP_FLAG_NONE;
    playMapQueue.playMap[ playMapQueue.head ].minusFlags[ i ] =
      PLAYMAP_FLAG_NONE;
  }

  // advance the head of the queue in the array (shorten queue by one)
  playMapQueue.head = PLAYMAP_QUEUE_PLUS1( playMapQueue.head );

  return playMap;
}

/*
================
G_RemoveFromPlayMapQueue

Remove a map from the playmap queue by its index.
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

  for( i = 0; i < G_GetPlayMapQueueLength(); i++ )
  {
    if( Q_stricmp( playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i)
					 ].mapname, mapname ) == 0 )
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
int G_GetPlayMapQueueIndexByClient( char *clientname )
{
  int i;

  for( i = 0; i < G_GetPlayMapQueueLength(); i++ )
  {
    if( Q_stricmp( playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i)
					 ].clientname, clientname ) == 0 )
      return i;
  }

  // client's map was not found in the queue
  return -1;
}

/*
================
G_PrintPlayMapQueue

Print the playmap queue on the console
================
*/
void G_PrintPlayMapQueue( gentity_t *ent )
{
  int i, len;

  ADMBP_begin(); // begin buffer
  
  if ( ( len = G_GetPlayMapQueueLength() ) )
    {
      ADMBP( "Maps that are currently in the queue:\n" );
    }
  else
    {
    ADMBP( va( "%s\n",
	       G_PlayMapErrorByCode(PLAYMAP_ERROR_MAP_QUEUE_EMPTY).errorMessage) );
    return;
    }

  for( i = 0; i < len; i++ )
  {
    playMap_t playMap =
      playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i) ];
    ADMBP( va( "^3%d.^7 ^5%s^7 (added by %s)\n", i + 1,
	       playMap.mapname, playMap.clientname ) );
  }

  ADMBP_end();
}

/*
===============
G_PlayMapActive

Test if playmap rotation is currently active
===============
*/
qboolean G_PlayMapActive( void )
{
  return ( g_playMapEnable.integer != PLAYMAP_INACTIVE &&
	   G_GetPlayMapQueueLength() > 0 );
}

/*
===============
G_NextPlayMap

Pop one playmap from queue, do checks and send commands to the server
to actually change the map.
===============
*/
void G_NextPlayMap( void )
{
  playMap_t *playMap = G_PopFromPlayMapQueue();
    
  // allow a manually defined g_nextLayout setting to override the maprotation
  if( !g_nextLayout.string[ 0 ] )
  {
    trap_Cvar_Set( "g_nextLayout", playMap->layout );
  }

  G_MapConfigs( playMap->mapname );

  trap_SendConsoleCommand( EXEC_APPEND, va( "map \"%s\"\n", playMap->mapname ) );

  // TODO: do the flags here
}
