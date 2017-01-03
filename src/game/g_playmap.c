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
 * TODO: Move this into server code so pool and queue aren't reset at
 * map changes.
 */

#include "g_local.h"

/*
 * external utilities
 */

int  trap_FS_Read( void *buffer, int len, fileHandle_t f );
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
    PLAYMAP_ERROR_MAP_POOL_EMPTY,         /* errorCode */
    "no maps in pool"
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
    "you already have a map queued on the playlist. Your previous entry is being deleted."
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

// list of playmap flags and info
static const playMapFlagDesc_t playMapFlagList[ ] =
{ // flag, flagName, defVal, avail, flagDesc
  { PLAYMAP_FLAG_NONE,  "", 	 qfalse, qfalse, "No flags" },
  { PLAYMAP_FLAG_DPUNT, "dpunt", qfalse, qfalse, "Dretch Punt" },
  { PLAYMAP_FLAG_FF, 	"ff", 	 qtrue,  qfalse, "Friendly Fire" },
  { PLAYMAP_FLAG_FBF, 	"fbf", 	 qtrue,  qfalse, "Friendly Buildable Fire" },
  { PLAYMAP_FLAG_SD, 	"sd", 	 qtrue,  qfalse, "Sudden Death" },
  { PLAYMAP_FLAG_LGRAV, "lgrav", qfalse, qfalse, "Low Gravity" },
  { PLAYMAP_FLAG_UBP, 	"ubp",	 qfalse, qfalse, "Unlimited BP" },
  { PLAYMAP_FLAG_PORTAL,"portal",qfalse, qfalse, "Portal Gun" },
  { PLAYMAP_FLAG_STACK, "stack", qfalse,  qtrue, "Buildable stacking" }
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
playMapError_t G_AddToPlayMapPool( char *mapName, char *mapType, int minClients,
                                   int maxClients, qboolean sortPool )
{
  playMapPoolEntry_t playPool;

  if( G_FindInMapPool( mapName ) >= 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_ALREADY_IN_POOL );

  if( playMapPoolCache.numMaps >= MAX_PLAYMAP_POOL_ENTRIES )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_POOL_FULL );

  // Initialize entry
  playPool.mapName = G_CopyString( mapName );
  playPool.mapType = G_CopyString( mapType );
  playPool.minClients = minClients;
  playPool.maxClients = maxClients;

  playMapPoolCache.mapEntries[ playMapPoolCache.numMaps ] = playPool;
  playMapPoolCache.numMaps++;

  // Sort all entries if requested
  if ( sortPool )
    G_SortPlayMapPool();

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_RemoveFromPlayMapPool

Remove a map from the current pool of maps available.
================
*/
playMapError_t G_RemoveFromPlayMapPool( char *mapName )
{
  int i, mapIndex;

  mapIndex = G_FindInMapPool( mapName );

  if( mapIndex < 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_IN_POOL );

  // remove the matching map pool entry
  BG_Free( playMapPoolCache.mapEntries[ mapIndex ].mapName );
  playMapPoolCache.mapEntries[ mapIndex ].mapName = NULL;

  for( i = mapIndex; i < playMapPoolCache.numMaps; i++ )
  {
    // safeguard against going beyond array bounds
    if( i >= MAX_PLAYMAP_POOL_ENTRIES )
      continue;

    // shift next entry in the map pool down by one (close the gap)
    playMapPoolCache.mapEntries[ i ] = playMapPoolCache.mapEntries[ i + 1 ];

    if( i == ( playMapPoolCache.numMaps - 1 ) )
      playMapPoolCache.mapEntries[ i ].mapName = NULL;
  }

  // decrease numMaps by one count
  playMapPoolCache.numMaps--;

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_SortPlayMapPool

Sort map pool alphabetically.
================
*/
static int SortPlaypoolEntries( const void *a, const void *b )
{
  // Can't link strcasecmp for some reason, so doing it manually -CG
  char mapNameA[ MAX_PLAYMAP_MAPNAME ], mapNameB[ MAX_PLAYMAP_MAPNAME ];
  Q_strncpyz( mapNameA, (*(playMapPoolEntry_t *)a).mapName, sizeof( mapNameA ) );
  Q_strncpyz( mapNameB, (*(playMapPoolEntry_t *)b).mapName, sizeof( mapNameB ) );
  return strcmp( Q_strlwr( mapNameA ), Q_strlwr( mapNameB ) );
}

void G_SortPlayMapPool( void )
{
  qsort( playMapPoolCache.mapEntries, playMapPoolCache.numMaps,
         sizeof( playMapPoolEntry_t ), SortPlaypoolEntries );
}

/*
================
G_SavePlayMapPool

Save map pool to configuration file after sorting.
================
*/
playMapError_t G_SavePlayMapPool( void )
{
  fileHandle_t  f;
  int           i;
  char          row[ 1024 ];
  playMapPoolEntry_t* entry;

  if( !g_playMapPoolConfig.string[ 0 ] )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_NO_POOL_CONFIG );

  G_SortPlayMapPool();

  if( trap_FS_FOpenFile( g_playMapPoolConfig.string, &f, FS_WRITE ) < 0 )
  {
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_POOL_CONFIG_UNREADABLE );
  }

  for( i = 0; i < playMapPoolCache.numMaps; i++ )
  {
    entry = &playMapPoolCache.mapEntries[ i ];

    // Croak if it is somehow a null pointer (should not happen)
    assert( entry->mapName );

    Com_sprintf( row, 1024, "%s %s %d %d\n", entry->mapName,
             entry->mapType ? entry->mapType : PLAYMAP_MAPTYPE_NONE,
             entry->minClients, entry->maxClients );
    trap_FS_Write( row, strlen( row ), f );
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
  int len, minClients, maxClients;
  char *cnf, *cnf2, mapName[ MAX_PLAYMAP_MAPNAME ], mapType[ MAX_PLAYMAP_MAPNAME ];

  if( !g_playMapPoolConfig.string[ 0 ] )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_NO_POOL_CONFIG );

  playMapError = G_ClearPlayMapPool();
  if( playMapError.errorCode != PLAYMAP_ERROR_NONE )
    return playMapError;

  // read playmap pool config file
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
  while( cnf < ( cnf2 + len )  )
  {
    Q_strncpyz( mapName, COM_ParseExt( &cnf, qtrue ), sizeof( mapName ) );
    if( !*mapName ) break;
    else if ( !G_MapExists( mapName ) )
    {
      COM_ParseWarning( "map '%s' not found skipping rest of line.", mapName );
      SkipRestOfLine( &cnf );
      continue;
    }

    Q_strncpyz( mapType, COM_ParseExt( &cnf, qfalse ), sizeof( mapType ) );
    if( !*mapType )
    {
      COM_ParseWarning("mapType not found on play pool file");

      if( g_debugPlayMap.integer > 0 )
        trap_Print( va( "PLAYMAP: adding to playmap pool map only: %s\n",
                        mapName ) );
      G_AddToPlayMapPool( mapName, PLAYMAP_MAPTYPE_NONE, 0, 0, qfalse );
    }
    else
    {
      minClients = atoi( COM_ParseExt( &cnf, qfalse ) );
      maxClients = atoi( COM_ParseExt( &cnf, qfalse ) );

      if( g_debugPlayMap.integer > 0 )
        trap_Print( va( "PLAYMAP: adding to playmap pool: %s %s %d %d\n",
                        mapName, mapType, minClients, maxClients ) );

      G_AddToPlayMapPool( mapName, mapType, minClients, maxClients, qfalse );
    }
  }
  BG_Free( cnf2 );

  // Sort once after adding all
  G_SortPlayMapPool();

  trap_Print( va( "Loaded %d maps into the playmap pool from config file %s\n",
                  G_GetPlayMapPoolLength( ), g_playMapPoolConfig.string ) );

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
    FREE_IF_NOT_NULL( playMapPoolCache.mapEntries[ i ].mapName );
    FREE_IF_NOT_NULL( playMapPoolCache.mapEntries[ i ].mapType );
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

int G_FindInMapPool( char *mapName )
{
  int i;

  for( i = 0; i < playMapPoolCache.numMaps; i++ )
  {
    // Croak if entry is somehow null (should not happen)
    assert( playMapPoolCache.mapEntries[ i ].mapName );

    // found a match. return the array index
    if( Q_stricmp(playMapPoolCache.mapEntries[ i ].mapName, mapName) == 0 )
      return i;
  }

  // not found
  return -1;
}

/*
================
G_GetPlayMapPoolLength

Returns the length of the play pool.
================
*/

int G_GetPlayMapPoolLength( void )
{
  return playMapPoolCache.numMaps;
}

/*
================
G_PrintPlayMapPool

Print the playmap pool on the console
================
*/
void G_PrintPlayMapPool( gentity_t *ent, int page )
{
  int i, j, len, rows, pages, row, start;

  if( ( len = playMapPoolCache.numMaps ) )
    {
      ADMBP_begin(); // begin buffer
      ADMBP( va(S_COLOR_CYAN "%d" S_COLOR_WHITE " maps available in pool",
		G_GetPlayMapPoolLength( )));
    }
  else
    {
    ADMP( va( "%s\n",
               G_PlayMapErrorByCode(PLAYMAP_ERROR_MAP_POOL_EMPTY).errorMessage) );
    return;
    }

  rows = ( len + 2 ) / 3;
  pages = MAX( 1, ( rows + MAX_MAPLIST_ROWS - 1 ) / MAX_MAPLIST_ROWS );

  // Enforce bounds
  page = min(page, pages - 1);
  page = max(page, 0);

  if( pages > 1 )
    ADMBP( va( " (page %d out of %d)", page + 1, pages ) );
  ADMBP( ":\n" );

  start = page * MAX_MAPLIST_ROWS * 3;
  if( len < start + ( 3 * MAX_MAPLIST_ROWS ) )
    rows = ( len - start + 2 ) / 3;
  else
    rows = MAX_MAPLIST_ROWS;

  for( row = 0; row < rows; row++ )
  {
    for( i = start + row, j = 0; i < len && j < 3; i += rows, j++ )
    {
      ADMBP( va( S_COLOR_CYAN " %-20s" S_COLOR_WHITE,
                 playMapPoolCache.mapEntries[ i ].mapName ) );
      // Limit per row (TODO: make one per row and write type and min/max next to it)
    }
    ADMBP( "\n" );
  }
  ADMBP_end();

}

/*
 * playmap queue utility functions
 */

// current map queue
static playMapQueue_t playMapQueue;

/*
================
G_InitPlayMapQueue

Initialize the playmap queue.
================
*/
void G_InitPlayMapQueue( void )
{
  int i;

  // Reset everything
  playMapQueue.tail = playMapQueue.head = 0;
  playMapQueue.tail = PLAYMAP_QUEUE_MINUS1( playMapQueue.tail );
  playMapQueue.numEntries = 0;

  for( i = 0; i < MAX_PLAYMAP_QUEUE_ENTRIES; i++ )
  {
    // set all values/pointers to NULL
    playMapQueue.playMap[ i ].mapName = NULL;
    playMapQueue.playMap[ i ].layout  = NULL;
    playMapQueue.playMap[ i ].clientName = NULL;
    playMapQueue.playMap[ i ].flags  = 0;
  }
}

/*
================
G_ClearPlayMapQueue

Free the memory allocated for the playmap queue elements.
================
*/
playMapError_t G_ClearPlayMapQueue( void )
{
  int i;

  for( i = 0; i < G_GetPlayMapQueueLength(); i++ )
  {
    playMap_t playMap =
      playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i) ];

    FREE_IF_NOT_NULL( playMap.mapName );
    FREE_IF_NOT_NULL( playMap.clientName );
    FREE_IF_NOT_NULL( playMap.layout );
  }

  // All contents gone, now must initialize it
  G_InitPlayMapQueue();

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_ValidatePlayMapQueue

Check playmap queue contents to validate them. E.g., entries from
disconnected clients will be deleted.
================
*/
playMapError_t G_ValidatePlayMapQueue( void )
{
  int i;
  playMap_t playMap;

  for( i = 0; i < G_GetPlayMapQueueLength(); i++ )
    {
      if( g_debugPlayMap.integer > 0 )
        trap_Print( va( "PLAYMAP: checking entry #%d out of %d in queue\n",
                        i + 1, G_GetPlayMapQueueLength() ) );
      playMap =
        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i ) ];

      // Check if client is still connected
      if ( ! G_FindClientByName( NULL, playMap.clientName ) )
        {
          trap_SendServerCommand( -1,
                                  va( "print \"Removing playlist entry #%d for "
                                      S_COLOR_CYAN "%s" S_COLOR_WHITE
                                      " because player %s" S_COLOR_WHITE
                                      " has left\n\"",
                                      i + 1, playMap.mapName, playMap.clientName ) );

          G_RemoveFromPlayMapQueue( i );
          i--; // because queue elements are now shifted
        }
    }

  if( g_debugPlayMap.integer > 0 )
    trap_Print( "PLAYMAP: leaving G_ValidatePlayMapQueue\n" );
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
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
  char *flagString;

  // Clean stale entries
  G_ValidatePlayMapQueue();

  if( !g_playMapQueueConfig.string[ 0 ] )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_NO_QUEUE_CONFIG );

  if( trap_FS_FOpenFile( g_playMapQueueConfig.string, &f, FS_WRITE ) < 0 )
  {
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_QUEUE_CONFIG_UNREADABLE );
  }

  if( g_debugPlayMap.integer > 0 )
    trap_Print( va( "PLAYMAP: saving %d queue entries\n", G_GetPlayMapQueueLength() ) );
  for( i = 0; i < G_GetPlayMapQueueLength(); i++ )
  {
    playMap_t playMap =
      playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i) ];

    trap_FS_Write( playMap.mapName, strlen( playMap.mapName ), f );
    if( playMap.clientName )
    {
         trap_FS_Write( " ", 1, f );
         trap_FS_Write( playMap.clientName,
                        strlen( playMap.clientName ), f );
         if( playMap.layout )
         {
              trap_FS_Write( " ", 1, f );
              trap_FS_Write( playMap.layout, strlen( playMap.layout ), f );
         }
	 flagString = G_PlayMapFlags2String( playMap.flags );
	 if( *flagString )
	 {
	   trap_FS_Write( " ", 1, f );
	   trap_FS_Write( flagString, strlen( flagString ), f );
	 }
	 BG_Free( flagString );
    }
    trap_FS_Write( "\n", 1, f );
  }

  trap_FS_FCloseFile( f );

  // Now clear the memory and initialize it
  G_ClearPlayMapQueue();

  if( g_debugPlayMap.integer > 0 )
    trap_Print( "PLAYMAP: leaving G_SavePlayMapQueue\n" );
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
    qboolean hasNewLines = qfalse;
    char *cnf, *cnf2, *mapName, empty = 0,
         *layout = &empty, *clientName = &empty, *flags = &empty;

    if( g_debugPlayMap.integer > 0 )
        trap_Print( "PLAYMAP: entering G_ReloadPlayMapQueue\n" );

    if( !g_playMapQueueConfig.string[ 0 ] )
        return G_PlayMapErrorByCode( PLAYMAP_ERROR_NO_QUEUE_CONFIG );

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

    if( g_debugPlayMap.integer > 0 )
        trap_Print( va( "PLAYMAP: before reading maps file of length %d\n", len ) );
    COM_BeginParseSession( g_playMapQueueConfig.string );
    while( cnf && cnf < (cnf2 + len) ) // rows
    {
        while ( cnf && cnf < (cnf2 + len) ) // map arguments
        {
	    // Initialize
	    mapName = NULL;
	    layout = NULL;
	    clientName = NULL;
	    flags = NULL;

            if( g_debugPlayMap.integer > 0 )
                trap_Print( va( "PLAYMAP: reading maps at char index %ld: '%s'\n",
				cnf - cnf2, cnf ) );
            mapName = COM_ParseExt( &cnf, qfalse );
            if( !*mapName )
	    {
	      mapName = NULL;
	      break;
	    }

	    // Must use G_CopyString to prevent overwrites by parser
    	    mapName = G_CopyString( mapName );
	    
            if( g_debugPlayMap.integer > 0 )
                trap_Print( va( "PLAYMAP: found map %s\n", mapName ) );

            clientName = G_CopyString(COM_ParseExt( &cnf, qfalse ));
            if( !*clientName )
	    {
	      clientName = NULL;
	      break;
	    }

	    // Check whether we have layout or flags
	    cnf = SkipWhitespace( cnf, &hasNewLines );

	    if( ! cnf || ! *cnf || *cnf == '\n')
	      break;
	    
	    if( !( *cnf == '+' )&&!( *cnf == '-' ) )
	    {
	      layout = COM_ParseExt( &cnf, qfalse );
	      if( !*layout )
	      {
		layout = NULL;
                break;
	      }
	      layout = G_CopyString(layout);
	    } else
	      layout = NULL;

	    cnf = SkipWhitespace( cnf, &hasNewLines );

	    if( !hasNewLines )
            {
	      flags = cnf;	/* Leave it at the end of layouts */
	      SkipRestOfLine( &cnf );
	    }
            break;
        }
        if( !mapName || !*mapName )
            break; // end of all maps

        if( g_debugPlayMap.integer > 0 )
            trap_Print( va( "PLAYMAP: map=%s\n"
                        "       client=%s\n"
                        "       layout=%s\n"
                        "       flags=%s\n",
                        mapName, clientName, layout, flags ) );

        G_PlayMapEnqueue( mapName, layout, clientName, flags, NULL );

        // G_PlayMapEnqueue also copies the strings, so release the
        // originals here
        if( mapName && *mapName )
            BG_Free(mapName);

        if( clientName && *clientName )
            BG_Free(clientName);

        if( layout && *layout )
            BG_Free(layout);

        if( g_debugPlayMap.integer > 0 )
            trap_Print( "PLAYMAP: enqueued\n" );
    }
    BG_Free( cnf2 );

    if( g_debugPlayMap.integer > 0 )
        trap_Print( "PLAYMAP: leaving G_ReloadPlayMapQueue\n" );

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
    if( !from->inuse || !from->client )
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
  return (playMapQueue.tail - playMapQueue.head + 1)
    % MAX_PLAYMAP_QUEUE_ENTRIES;
}

/*
================
G_PlayMapEnqueue

Enqueue a player requested map in the playmap queue (add to the back of the
queue).
================
*/

playMapError_t G_PlayMapEnqueue( char *mapName, char *layout,
                                 char *clientName, char *flags, gentity_t *ent )
{
  int       	i;
  playMap_t 	playMap;

  if( G_FindInMapPool( mapName ) < 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_IN_POOL );

  if( PLAYMAP_QUEUE_IS_FULL )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_QUEUE_FULL );

  // check if user already has a map queued
  if( ( i = G_GetPlayMapQueueIndexByClient( clientName ) ) >= 0 )
    {
      ADMP( va( "%s\n",
                G_PlayMapErrorByCode( PLAYMAP_ERROR_USER_ALREADY_IN_QUEUE ).errorMessage) );
      G_RemoveFromPlayMapQueue( i );
    }

  // check if map has already been queued by someone else
  if( G_GetPlayMapQueueIndexByMapName( mapName ) >= 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_ALREADY_IN_QUEUE );

  // copy mapName
  playMap.mapName = G_CopyString(mapName);

  // copy layout
  if( layout && *layout )
    playMap.layout = G_CopyString(layout);
  else
    playMap.layout = NULL;

  // copy clientName
  if( clientName && *clientName )
    playMap.clientName = G_CopyString(clientName);
  else
    playMap.clientName = NULL;

  playMap.flags = G_ParsePlayMapFlagTokens( ent, flags );

  playMapQueue.tail = PLAYMAP_QUEUE_PLUS1( playMapQueue.tail );
  playMapQueue.playMap[ playMapQueue.tail ] = playMap;
  playMapQueue.numEntries++;

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_ParsePlayMapFlag

Parses a playmap flag string and returns the playMapFlag_t equivalent.
================
*/

playMapFlag_t G_ParsePlayMapFlag(gentity_t *ent, char *flag)
{
  int flagNum;

  for( flagNum = PLAYMAP_FLAG_NONE + 1;
       flagNum < PLAYMAP_NUM_FLAGS; flagNum++ )
  {
    if( Q_stricmp_exact( flag, playMapFlagList[ flagNum ].flagName ) == 0 )
    {
      // Check if flag is available
      if( ! playMapFlagList[ flagNum ].avail )
      {
	ADMP( va( "Playmap flag " S_COLOR_CYAN "%s" S_COLOR_WHITE
		  " is disabled.\n", flag) );
	break;			// Out of for
      }
      // Confirm flag sequence and number match 
      assert( flagNum == playMapFlagList[ flagNum ].flag);
      return flagNum;
    }
  }

  return PLAYMAP_FLAG_NONE;
}

/*
================
G_DefaultPlayMapFlags

Returns binary value of default flags.
================
*/

int G_DefaultPlayMapFlags(void)
{
  int flagNum,
      flags = PLAYMAP_FLAG_NONE;

  for( flagNum = PLAYMAP_FLAG_NONE + 1;
       flagNum < PLAYMAP_NUM_FLAGS; flagNum++ )
    if( playMapFlagList[ flagNum ].defVal )
      PlaymapFlag_Set( flags, flagNum );

  return flags;
}

/*
================
G_ParsePlayMapFlagTokens

Tokenize string to find flags and set or clear bits on a given default
flag configuration. Defaults come from playMapFlagList.
================
*/
int G_ParsePlayMapFlagTokens( gentity_t *ent, char *flags )
{
  int 		i, flagsValue;
  char  	*token;
  playMapFlag_t playMapFlag;

  // Get defaults
  flagsValue = G_DefaultPlayMapFlags();

  if( !flags || !*flags )
    return flagsValue;
  
  // Loop through flags
  for( i = 0; ; i++ )
  {
    token = COM_ParseExt( &flags, qfalse );
    if( g_debugPlayMap.integer > 0 )
      trap_Print( va( "PLAYMAP: parsing flag #%d: %s.\n",
		      i + 1, token ));

    if( !token[0] )
      break;

    switch( token[0] )
    {
      case '+':
        if( ( strlen( token ) > 1 ) )
        {
          playMapFlag = G_ParsePlayMapFlag( ent, token + 1 );

          if ( playMapFlag != PLAYMAP_FLAG_NONE )
            PlaymapFlag_Set(flagsValue, playMapFlag);

	  if( g_debugPlayMap.integer > 0 )
	    trap_Print( va( "PLAYMAP: setting flag %s.\n",
			    playMapFlagList[ playMapFlag ].flagDesc ) );
        }
        break;
      case '-':
        if( ( strlen( token ) > 1 ) )
        {
          playMapFlag = G_ParsePlayMapFlag( ent, token + 1 );

          if ( playMapFlag != PLAYMAP_FLAG_NONE )
            PlaymapFlag_Clear(flagsValue, playMapFlag);

	  if( g_debugPlayMap.integer > 0 )
	    trap_Print( va( "PLAYMAP: clearing flag %s.\n",
			    playMapFlagList[ playMapFlag ].flagDesc ) );
        }
        break;
    }
  }

  return flagsValue;
}

/*
================
G_PlayMapFlags2String

Convert flag bits into string representation. Compares to
default flags here to show only differences.
TODO: Write to caller's buffer instead of allocating new one.
================
*/
char *G_PlayMapFlags2String( int flags )
{
  int 	 flagNum;
  char 	*flagString;
  char 	 token[ MAX_TOKEN_CHARS ];

  flagString = BG_Alloc( MAX_STRING_CHARS );
  *flagString = '\0';
  
  // Loop through flags
  for( flagNum = PLAYMAP_FLAG_NONE + 1; flagNum < PLAYMAP_NUM_FLAGS; flagNum++ )
  {
    if( ! playMapFlagList[ flagNum ].defVal &&
	PlaymapFlag_IsSet( flags, flagNum ) )
    {
      Com_sprintf( token, MAX_TOKEN_CHARS, "+%s ",
		   playMapFlagList[ flagNum ].flagName );
      Q_strcat( flagString, MAX_STRING_CHARS, token );
    } else if( playMapFlagList[ flagNum ].defVal &&
	       ! PlaymapFlag_IsSet( flags, flagNum ) )
    {
      Com_sprintf( token, MAX_TOKEN_CHARS, "-%s ",
		   playMapFlagList[ flagNum ].flagName );
      Q_strcat( flagString, MAX_STRING_CHARS, token );
    }
  }

  return flagString;
}

/*
================
G_ExecutePlaymapFlags

Set cvars, etc. to execute actions associated with the selected or
cleared flags.
================
*/
void G_ExecutePlaymapFlags( int flagsValue )
{
  int flagNum;

  for( flagNum = PLAYMAP_FLAG_NONE + 1;
       flagNum < PLAYMAP_NUM_FLAGS; flagNum++ )
  {
    // Add special handling of each flag here
    switch( flagNum )
    {
      case PLAYMAP_FLAG_STACK:
	g_allowBuildableStacking.integer =
	  PlaymapFlag_IsSet( flagsValue, PLAYMAP_FLAG_STACK ) != 0;
	if( g_debugPlayMap.integer > 0 )
	  trap_Print( va( "PLAYMAP: setting stackable flag to %d.\n",
			  PlaymapFlag_IsSet( flagsValue, PLAYMAP_FLAG_STACK ) != 0 ) );
	break;	
    }
    if( g_debugPlayMap.integer > 0 )
      trap_Print( va( "PLAYMAP: executing flag #%d with value %d.\n",
		      flagNum, PlaymapFlag_IsSet( flagsValue, flagNum ) != 0 ) );
  }
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

  if( PLAYMAP_QUEUE_IS_EMPTY )
    return NULL;

  playMap = BG_Alloc( sizeof(*playMap) );

  // copy values from playmap in the head of the queue
  playMap->mapName = playMapQueue.playMap[ playMapQueue.head ].mapName;
  playMap->layout = playMapQueue.playMap[ playMapQueue.head ].layout;
  playMap->clientName = playMapQueue.playMap[ playMapQueue.head ].clientName;
  playMap->flags = playMapQueue.playMap[ playMapQueue.head ].flags;

  // reset the playmap in the head of the queue to null values
  playMapQueue.playMap[ playMapQueue.head ].mapName = NULL;
  playMapQueue.playMap[ playMapQueue.head ].layout = NULL;
  playMapQueue.playMap[ playMapQueue.head ].clientName = NULL;
  playMapQueue.playMap[ playMapQueue.head ].flags = 0;

  // advance the head of the queue in the array (shorten queue by one)
  playMapQueue.head = PLAYMAP_QUEUE_PLUS1( playMapQueue.head );

  // housekeeping, but this is redundant
  playMapQueue.numEntries--;

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
  int i, len;

  playMap_t playMap =
    playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, index ) ];

  if( g_debugPlayMap.integer > 0 )
    trap_Print( va( "PLAYMAP: about to remove map %d out of %d in queue. "
                    "Head: %d, Tail: %d\n", index + 1, G_GetPlayMapQueueLength(),
                    playMapQueue.head, playMapQueue.tail ) );

  // Clear up memory for deleted information
  FREE_IF_NOT_NULL( playMap.mapName );
  FREE_IF_NOT_NULL( playMap.clientName );
  FREE_IF_NOT_NULL( playMap.layout );

  len = G_GetPlayMapQueueLength();
  for( i = index; i < ( len - 1 ); i++ )
    {
      if( g_debugPlayMap.integer > 0 )
        trap_Print( va( "PLAYMAP: copying map '%s' from #%d onto map '%s' in #%d.\n",
                        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i + 1) ].mapName,
                        PLAYMAP_QUEUE_ADD( playMapQueue.head, i + 1),
                        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i) ].mapName,
                        PLAYMAP_QUEUE_ADD( playMapQueue.head, i ) ) );

      // Copy next one over
      playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i ) ] =
        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i + 1) ];

      if( g_debugPlayMap.integer > 0 )
        trap_Print( va( "PLAYMAP: after copying map is '%s'.\n",
                        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i)
                                              ].mapName ) );
    }

  // If this wasn't the last one
  if ( index < ( len - 1 ) )
    {
      if( g_debugPlayMap.integer > 0 )
        trap_Print( va( "PLAYMAP: erasing map '%s' from #%d.\n",
                        playMapQueue.playMap[ playMapQueue.tail ].mapName,
                        playMapQueue.tail ) );

      // Clear pointers in the last one
      // (they are copied and in use, so don't free them!)
      playMapQueue.playMap[ playMapQueue.tail ].mapName = NULL;
      playMapQueue.playMap[ playMapQueue.tail ].clientName = NULL;
      playMapQueue.playMap[ playMapQueue.tail ].layout = NULL;
    }

  // Regress the tail marker
  playMapQueue.tail = PLAYMAP_QUEUE_MINUS1( playMapQueue.tail );
  playMapQueue.numEntries--;

  if( g_debugPlayMap.integer > 0 )
    trap_Print( va( "PLAYMAP: after removing, index points to map %s."
                    " Head: %d, Tail: %d\n",
                    playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, index )
                                          ].mapName,
                    playMapQueue.head, playMapQueue.tail ) );

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_GetPlayMapQueueIndexByMapName

Get the index of the map in the queue from the mapName.
================
*/
int G_GetPlayMapQueueIndexByMapName( char *mapName )
{
  int i;

  for( i = 0; i < G_GetPlayMapQueueLength(); i++ )
  {
    if( Q_stricmp( playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i)
                                         ].mapName, mapName ) == 0 )
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
int G_GetPlayMapQueueIndexByClient( char *clientName )
{
  int i;

  for( i = 0; i < G_GetPlayMapQueueLength(); i++ )
  {
    if( Q_stricmp( playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i)
                                         ].clientName, clientName ) == 0 )
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
  int 	    i, len;
  playMap_t *playMap;
  char 	    *flagString;

  ADMBP_begin(); // begin buffer

  if ( ( len = G_GetPlayMapQueueLength() ) )
  {
    ADMBP( va( "Playmap queue has " S_COLOR_CYAN "%d" S_COLOR_WHITE " maps:\n"
	       "    Map        Player     Flags\n", G_GetPlayMapQueueLength( ) ) );
  }
  else
  {
    ADMBP( va( "%s\n",
               G_PlayMapErrorByCode(PLAYMAP_ERROR_MAP_QUEUE_EMPTY).errorMessage) );
    ADMBP_end();
    return;
    }

  for( i = 0; i < len; i++ )
  {
    playMap = &playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i) ];
    flagString = G_PlayMapFlags2String( playMap->flags );
    ADMBP( va( S_COLOR_YELLOW "%2d." S_COLOR_WHITE " "
               S_COLOR_CYAN "%-10s" S_COLOR_WHITE
               " %-10s " S_COLOR_RED "%s" S_COLOR_WHITE "\n", i + 1,
               playMap->mapName, playMap->clientName, flagString ) );
    BG_Free( flagString );
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
  return ( g_playMapEnable.integer != PLAYMAP_INACTIVE && G_GetPlayMapQueueLength() > 0 );
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

  G_MapConfigs( playMap->mapName );

  trap_SendConsoleCommand( EXEC_APPEND, va( "map \"%s\"\n", playMap->mapName ) );

  // too early to execute the flags 
  level.playmapFlags = playMap->flags;
  //G_ExecutePlaymapFlags( playMap->flags );
}

