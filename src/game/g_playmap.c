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

/*
 * TODO: Move this into server code so pool and queue aren't reset at
 * map changes.
 */

#include "g_local.h"

/*
 * external utilities
 */

int  FS_Read2( void *buffer, int len, fileHandle_t f );
void FS_Write( const void *buffer, int len, fileHandle_t f );

/*
 * globals
 */

// playmap pool buffer
static char playmap_pool_str[ MAX_PLAYMAP_POOL_CHARS ];

// Indicates we're in the middle of a broadcast and the ADMBP holds
// the contents of the playmap pool
static qboolean PlayMapPoolMessageBroadcast = qfalse;

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
    PLAYMAP_ERROR_USER_GUIDLESS,         /* errorCode */
    "your client is missing a GUID, you can download an updated client with a GUID from ^5www.grangerhub.com^7"
  },
  {
    PLAYMAP_ERROR_UNKNOWN,               /* errorCode */
    "an unknown error has occured"
  }
};

// list of playmap flags and info
// See g_playmap.h for field definitions.
// Warning: make sure flag field is in same order as the enum.
static const playMapFlagDesc_t playMapFlagList[ ] =
  { // Fields: flag, flagName, defVal, avail, flagDesc
  { PLAYMAP_FLAG_NONE,  "",    qfalse, qfalse, "No flags" },
  { PLAYMAP_FLAG_DPUNT, "dpunt", qfalse, qfalse, "Dretch Punt" },
  { PLAYMAP_FLAG_FF,   "ff",    qtrue,  qfalse, "Friendly Fire" },
  { PLAYMAP_FLAG_FBF,   "fbf",    qtrue,  qfalse, "Friendly Buildable Fire" },
  { PLAYMAP_FLAG_SD,   "sd",    qtrue,  qfalse, "Sudden Death" },
  { PLAYMAP_FLAG_LGRAV, "lgrav", qfalse, qfalse, "Low Gravity" },
  { PLAYMAP_FLAG_UBP,   "ubp",   qfalse, qfalse, "Unlimited BP" },
  { PLAYMAP_FLAG_PORTAL,"portal",qfalse, qfalse, "Portal Gun" },
  { PLAYMAP_FLAG_STACK, "stack", qtrue,  qfalse, "Buildable stacking" }
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

  // Notify all clients if we're not in broadcast mode during reload
  // because they will be notified after all are loaded
  if( ! PlayMapPoolMessageBroadcast )
    SendPlayMapPoolMessageToAllClients();

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

  // Notify all clients
  SendPlayMapPoolMessageToAllClients();

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

  if( FS_FOpenFileByMode( g_playMapPoolConfig.string, &f, FS_WRITE ) < 0 )
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
    FS_Write( row, strlen( row ), f );
  }

  FS_FCloseFile( f );

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
==================
PlayMapPoolMessage

==================
*/
void PlayMapPoolMessage( int client )
{
  if ( !PlayMapPoolMessageBroadcast )
    G_PrintPlayMapPool( g_entities + client, -1, qtrue );
  SV_GameSendServerCommand( client, va( "playpool_json %s", playmap_pool_str ) );
}

/*
========================
SendPlayMapPoolMessageToAllClients

Do this at the beginning of game and everytime the pool contents change.
========================
*/
void SendPlayMapPoolMessageToAllClients( void )
{
  int   i, num_clients;
  PlayMapPoolMessageBroadcast = qtrue;
  G_PrintPlayMapPool( NULL, -1, qtrue );

  for( i = 0, num_clients = 3; i < level.maxclients; i++ )
    if( level.clients[ i ].pers.connected == CON_CONNECTED )
    {
      PlayMapPoolMessage( i );
      num_clients++;
    }

  if( g_debugPlayMap.integer > 0 )
    Com_Printf( "PLAYMAP: broadcasted playmap pool to %d "
        "clients with message %s.\n",
        num_clients, playmap_pool_str );

  PlayMapPoolMessageBroadcast = qfalse;
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
  len = FS_FOpenFileByMode( g_playMapPoolConfig.string, &f, FS_READ );
  if( len < 0 )
  {
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_POOL_CONFIG_UNREADABLE );
  }

  cnf = BG_StackPoolAlloc( len + 1 );
  cnf2 = cnf;
  FS_Read2( cnf, len, f );
  cnf[ len ] = '\0';
  FS_FCloseFile( f );

  // enable broadcast flag to prevent sending info to clients for each add
  PlayMapPoolMessageBroadcast = qtrue;

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
        Com_Printf( "PLAYMAP: adding to playmap pool map only: %s\n",
                        mapName );
      G_AddToPlayMapPool( mapName, PLAYMAP_MAPTYPE_NONE, 0, 0, qfalse );
    }
    else
    {
      minClients = atoi( COM_ParseExt( &cnf, qfalse ) );
      maxClients = atoi( COM_ParseExt( &cnf, qfalse ) );

      if( g_debugPlayMap.integer > 0 )
        Com_Printf( "PLAYMAP: adding to playmap pool: %s %s %d %d\n",
                        mapName, mapType, minClients, maxClients );

      G_AddToPlayMapPool( mapName, mapType, minClients, maxClients, qfalse );
    }
  }
  BG_StackPoolFree( cnf2 );

  // Sort once after adding all
  G_SortPlayMapPool();

  Com_Printf( "Loaded %d maps into the playmap pool from config file %s\n",
                  G_GetPlayMapPoolLength( ), g_playMapPoolConfig.string );

  // Notify all clients
  SendPlayMapPoolMessageToAllClients();

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

Print the playmap pool on the console. If JSON is requested, prepare
with ADMBP, but don't print it so it's still accessible.
================
*/
void G_PrintPlayMapPool( gentity_t *ent, int page, qboolean isJson )
{
  int i, j, len, rows, pages, row, start, num_cols;

  if( !isJson )
  {
    ADMBP_begin();
    // TODO: this should be calculated from console width
    num_cols = 3;
  }
  else
  {
    playmap_pool_str[ 0 ] = '\0';
    num_cols = 1;
  }

  if( ( len = playMapPoolCache.numMaps ) )
    {
      if( !isJson ) // discard verbose messages in JSON
  ADMBP( va( S_COLOR_CYAN "%d" S_COLOR_WHITE " maps available in pool",
       G_GetPlayMapPoolLength( ) ) );
      else
  Q_strcat( playmap_pool_str, MAX_PLAYMAP_POOL_CHARS, "[" );
    }
  else
    {
      if ( !isJson )
  ADMBP( va( "%s\n",
       G_PlayMapErrorByCode(PLAYMAP_ERROR_MAP_POOL_EMPTY).errorMessage));
      else
      { // create an empty array in buffer
  Q_strcat( playmap_pool_str, MAX_PLAYMAP_POOL_CHARS, "[]" );
      }
    return;
    }

  rows = ( len + ( num_cols - 1 ) ) / num_cols;

  if( page < 0 )
    pages = 1;      //disabled
  else
    pages = MAX( 1, ( rows + MAX_MAPLIST_ROWS - 1 ) / MAX_MAPLIST_ROWS );

  // Enforce bounds
  page = min(page, pages - 1);
  page = max(page, 0);

  if ( !isJson )
  {
    if( pages > 1 )
      ADMBP( va( " (page %d out of %d)", page + 1, pages ) );
    ADMBP( ":\n" );
  }
  start = page * MAX_MAPLIST_ROWS * num_cols;
  if( len < start + ( num_cols * MAX_MAPLIST_ROWS ) || pages == 1)
    rows = ( len - start + ( num_cols - 1 ) ) / num_cols;
  else
    rows = MAX_MAPLIST_ROWS;

  for( row = 0; row < rows; row++ )
  {
    for( i = start + row, j = 0; i < len && j < num_cols; i += rows, j++ )
    {
      if ( !isJson )
  ADMBP( va( S_COLOR_CYAN " %-20s" S_COLOR_WHITE,
       playMapPoolCache.mapEntries[ i ].mapName ) );
      else
  Q_strcat( playmap_pool_str, MAX_PLAYMAP_POOL_CHARS,
      va( "%s,",
          playMapPoolCache.mapEntries[ i ].mapName ) );
      // Limit per row (TODO: make one per row and write type and min/max next to it)
    }
    if ( !isJson )
      ADMBP( "\n" );
  }
  // don't end for JSON because it prints out
  if ( !isJson )
    ADMBP_end();
  else
    Q_strcat( playmap_pool_str, MAX_PLAYMAP_POOL_CHARS, "]" );
}

/*
 * playmap queue utility functions
 */

// current map queue
static playMapQueue_t playMapQueue;

qboolean G_Get_Net_Name_From_GUID(const char *guid, char *name, size_t size_of_name) {
  namelog_t *n;

  //search through the namelogs
  for(n = level.namelogs; n; n = n->next) {
    if(!Q_stricmp(guid, n->guid)) {
      Q_strncpyz(name, n->name[n->nameOffset], size_of_name);
      return qtrue;
    }
  }

  return qfalse;
}

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
    playMapQueue.playMap[ i ].mapName[0] = '\0';
    playMapQueue.playMap[ i ].layout[0] = '\0';
    playMapQueue.playMap[ i ].console = qfalse;
    playMapQueue.playMap[ i ].guid[0] = '\0';
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

    playMap.mapName[0] = '\0';
    playMapQueue.playMap[ i ].console = qfalse;
    playMapQueue.playMap[ i ].guid[0] = '\0';
    playMap.layout[0] = '\0';
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
        Com_Printf( "PLAYMAP: checking entry #%d out of %d in queue\n",
                        i + 1, G_GetPlayMapQueueLength() );
      playMap =
        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i ) ];

      // Check if client is still connected
      if ( !playMap.console &&
     !G_FindClientByGUID( playMap.guid ) )
        {
          char name[MAX_NAME_LENGTH + 1];

          SV_GameSendServerCommand( -1,
                                  va( "print \"Removing playlist entry #%d for "
                                      S_COLOR_CYAN "%s" S_COLOR_WHITE
                                      " because player %s" S_COLOR_WHITE
                                      " has left\n\"",
                                      i + 1, playMap.mapName, 
                                      (G_Get_Net_Name_From_GUID(playMap.guid, name, sizeof(name)) ? name : "")));

          G_RemoveFromPlayMapQueue( i );
          i--; // because queue elements are now shifted
        }
    }

  if( g_debugPlayMap.integer > 0 )
    Com_Printf( "PLAYMAP: leaving G_ValidatePlayMapQueue\n" );
  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_SavePlayMapQueue

Save the playmap queue into a config file. Should be run before
switching maps for the queue to be persistent across map changes.
================
*/
playMapError_t G_SavePlayMapQueue( void ) {
  int i;

  // Clean stale entries
  G_ValidatePlayMapQueue();

  if( g_debugPlayMap.integer > 0 )
    Com_Printf( "PLAYMAP: saving %d queue entries\n", G_GetPlayMapQueueLength() );

  SV_PlayMap_Clear_Saved_Queue(G_DefaultPlayMapFlags( ));

  for( i = 0; i < G_GetPlayMapQueueLength(); i++ ) {
    SV_PlayMap_Save_Queue_Entry(
      playMapQueue.playMap[ PLAYMAP_QUEUE_ADD(playMapQueue.head, i) ],
      i);
  }

  // Now clear the memory and initialize it
  G_ClearPlayMapQueue();

  if( g_debugPlayMap.integer > 0 )
    Com_Printf( "PLAYMAP: leaving G_SavePlayMapQueue\n" );
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
  int i;

  if( g_debugPlayMap.integer > 0 )
    Com_Printf( "PLAYMAP: entering G_ReloadPlayMapQueue\n" );

  G_InitPlayMapQueue();

  for( i = 0; i < MAX_PLAYMAP_QUEUE_ENTRIES; i++ ) {
    playMap_t savedEntry = SV_PlayMap_Get_Queue_Entry(i);
    char *flagString = G_PlayMapFlags2String(savedEntry.flags);

    if( g_debugPlayMap.integer > 0 ) {
      Com_Printf( va( "PLAYMAP: map=%s\n"
                      "       console=%s\n"
                      "       guid=%s\n"
                      "       layout=%s\n"
                      "       flags=%s\n",
                      savedEntry.mapName,
                      savedEntry.console ? "qtrue" : "qfalse",
                      savedEntry.guid,
                      savedEntry.layout,
                      flagString ) );
    }

    G_PlayMapEnqueue(
      savedEntry.mapName,
      savedEntry.layout,
      savedEntry.console,
      savedEntry.guid,
      flagString, NULL );

    FREE_IF_NOT_NULL(flagString);
  }

  if( g_debugPlayMap.integer > 0 )
    Com_Printf( "PLAYMAP: leaving G_ReloadPlayMapQueue\n" );

  return G_PlayMapErrorByCode( PLAYMAP_ERROR_NONE );
}

/*
================
G_FindClientByGUID

Find client from given netname.
TODO: May need to be in g_utils.c. Also, no longer used here.
================
*/

gclient_t *G_FindClientByGUID(const char *guid) {
  gclient_t *client;
  int i;

  Com_Assert(guid && "G_FindClientByGUID: guid is NULL");

  for(i = 0; i < level.maxclients; i++ ) {
    client = &level.clients[i];

    if( client->pers.connected == CON_DISCONNECTED )
      continue;

    if( !Q_stricmp( client->pers.guid, guid ) )
      return client;
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
                                 qboolean console, const char *guid,
                                 char *flags, gentity_t *ent )
{
  int         i;
  playMap_t   playMap;

  if(!console) {
    if(!guid || guid[0] == '\0') {
      return G_PlayMapErrorByCode( PLAYMAP_ERROR_USER_GUIDLESS );
    }
  }

  if( G_FindInMapPool( mapName ) < 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_NOT_IN_POOL );

  if( PLAYMAP_QUEUE_IS_FULL )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_QUEUE_FULL );

  // check if map has already been queued by someone else
  if( G_GetPlayMapQueueIndexByMapName( mapName ) >= 0 )
    return G_PlayMapErrorByCode( PLAYMAP_ERROR_MAP_ALREADY_IN_QUEUE );

  // check if user already has a map queued
  if( ( i = G_GetPlayMapQueueIndexByClient( guid, console ) ) >= 0 )
    {
      ADMP( va( "%s\n",
                G_PlayMapErrorByCode( PLAYMAP_ERROR_USER_ALREADY_IN_QUEUE ).errorMessage) );
      G_RemoveFromPlayMapQueue( i );
    }

  // copy mapName
  Q_strncpyz(playMap.mapName, mapName, sizeof(playMap.mapName));

  // copy layout
  if( layout && *layout ) {
    Q_strncpyz(playMap.layout, layout, sizeof(playMap.layout));
  }
  else {
    playMap.layout[0] = '\0';
  }

  // copy client info
  playMap.console = console;
  if( !console && *guid ) {
    Q_strncpyz(playMap.guid, guid, sizeof(playMap.guid));
  }
  else {
    playMap.guid[0] = '\0';
  }

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
        break;      // Out of for
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
  int     i, flagsValue;
  char    *token;
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
      Com_Printf( "PLAYMAP: parsing flag #%d: %s.\n", i + 1, token );

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
            Com_Printf( "PLAYMAP: setting flag %s.\n",
              playMapFlagList[ playMapFlag ].flagDesc );
        }
        break;
      case '-':
        if( ( strlen( token ) > 1 ) )
        {
          playMapFlag = G_ParsePlayMapFlag( ent, token + 1 );

          if ( playMapFlag != PLAYMAP_FLAG_NONE )
            PlaymapFlag_Clear(flagsValue, playMapFlag);

          if( g_debugPlayMap.integer > 0 )
            Com_Printf( "PLAYMAP: clearing flag %s.\n",
              playMapFlagList[ playMapFlag ].flagDesc );
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
  int    flagNum;
  char   *flagString;
  char    token[ MAX_TOKEN_CHARS ];

  flagString = BG_Alloc0( MAX_STRING_CHARS );
  *flagString = '\0';

  // Loop through flags
  for( flagNum = PLAYMAP_FLAG_NONE + 1; flagNum < PLAYMAP_NUM_FLAGS; flagNum++ )
  {
    if( !playMapFlagList[ flagNum ].defVal &&
        PlaymapFlag_IsSet( flags, flagNum ) )
    {
      Com_sprintf( token, MAX_TOKEN_CHARS, "+%s ",
        playMapFlagList[ flagNum ].flagName );
      Q_strcat( flagString, MAX_STRING_CHARS, token );
    } else if( playMapFlagList[ flagNum ].defVal &&
              !PlaymapFlag_IsSet( flags, flagNum ) )
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
    // Only process available flags even for its default value
    if ( ! playMapFlagList[flagNum].avail )
      continue;

    // Add special handling of each flag here
    switch( flagNum )
    {
      case PLAYMAP_FLAG_STACK:
        Cvar_SetSafe( "g_allowBuildableStacking",
          ( PlaymapFlag_IsSet( flagsValue, PLAYMAP_FLAG_STACK ) != 0 )
           ? "1" : "0");
        if( g_debugPlayMap.integer > 0 )
          Com_Printf( "PLAYMAP: setting stackable flag to %d.\n",
            PlaymapFlag_IsSet( flagsValue, PLAYMAP_FLAG_STACK ) != 0 );
        break;
    }
    if( g_debugPlayMap.integer > 0 )
      Com_Printf( "PLAYMAP: executing flag #%d with value %d.\n",
        flagNum, PlaymapFlag_IsSet( flagsValue, flagNum ) != 0 );
  }
}

/*
================
G_PopFromPlayMapQueue

Pop a map from the head of playmap queue (dequeue) and return to function
caller.
================
*/
qboolean G_PopFromPlayMapQueue( playMap_t *playMap )
{
  if( PLAYMAP_QUEUE_IS_EMPTY )
    return qfalse;

  // copy values from playmap in the head of the queue
  Q_strncpyz(
    playMap->mapName,
    playMapQueue.playMap[ playMapQueue.head ].mapName,
    sizeof(playMap->mapName));
  Q_strncpyz(
    playMap->layout,
    playMapQueue.playMap[ playMapQueue.head ].layout,
    sizeof(playMap->layout));
  Q_strncpyz(
    playMap->guid,
    playMapQueue.playMap[ playMapQueue.head ].guid,
    sizeof(playMap->guid));
  playMap->flags = playMapQueue.playMap[ playMapQueue.head ].flags;
  playMap->console = playMapQueue.playMap[ playMapQueue.head ].console;

  // reset the playmap in the head of the queue to null values
  playMapQueue.playMap[ playMapQueue.head ].mapName[0] = '\0';
  playMapQueue.playMap[ playMapQueue.head ].layout[0] = '\0';
  playMapQueue.playMap[ playMapQueue.head ].guid[0] = '\0';
  playMapQueue.playMap[ playMapQueue.head ].console = qfalse;
  playMapQueue.playMap[ playMapQueue.head ].flags = G_DefaultPlayMapFlags( );

  // advance the head of the queue in the array (shorten queue by one)
  playMapQueue.head = PLAYMAP_QUEUE_PLUS1( playMapQueue.head );

  // housekeeping, but this is redundant
  playMapQueue.numEntries--;

  return qtrue;
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

  if( g_debugPlayMap.integer > 0 )
    Com_Printf( "PLAYMAP: about to remove map %d out of %d in queue. "
                "Head: %d, Tail: %d\n", index + 1, G_GetPlayMapQueueLength(),
                playMapQueue.head, playMapQueue.tail );

  len = G_GetPlayMapQueueLength();
  for( i = index; i < ( len - 1 ); i++ )
  {
    if( g_debugPlayMap.integer > 0 )
      Com_Printf( "PLAYMAP: copying map '%s' from #%d onto map '%s' in #%d.\n",
        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i + 1) ].mapName,
        PLAYMAP_QUEUE_ADD( playMapQueue.head, i + 1),
        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i) ].mapName,
        PLAYMAP_QUEUE_ADD( playMapQueue.head, i ) );

    // Copy next one over
    playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i ) ] =
      playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i + 1) ];

    if( g_debugPlayMap.integer > 0 )
      Com_Printf( "PLAYMAP: after copying map is '%s'.\n",
        playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, i) ].mapName );
  }

  // If this wasn't the last one
  if ( index < ( len - 1 ) )
  {
    if( g_debugPlayMap.integer > 0 )
      Com_Printf( "PLAYMAP: erasing map '%s' from #%d.\n",
                  playMapQueue.playMap[ playMapQueue.tail ].mapName,
                  playMapQueue.tail );

    // Clear strings in the last one
    playMapQueue.playMap[ playMapQueue.tail ].mapName[0] = '\0';
    playMapQueue.playMap[ playMapQueue.tail ].guid[0] = '\0';
    playMapQueue.playMap[ playMapQueue.tail ].console = 0;
    playMapQueue.playMap[ playMapQueue.tail ].layout[0] = '\0';
  }

  // Regress the tail marker
  playMapQueue.tail = PLAYMAP_QUEUE_MINUS1( playMapQueue.tail );
  playMapQueue.numEntries--;

  if( g_debugPlayMap.integer > 0 )
    Com_Printf( "PLAYMAP: after removing, index points to map %s."
                    " Head: %d, Tail: %d\n",
      playMapQueue.playMap[ PLAYMAP_QUEUE_ADD( playMapQueue.head, index ) ].mapName,
      playMapQueue.head, playMapQueue.tail );

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
int G_GetPlayMapQueueIndexByClient(const char *guid, qboolean console) {
  int i;

  for(i = 0; i < G_GetPlayMapQueueLength(); i++) {
    if(console) {
      if(playMapQueue.playMap[PLAYMAP_QUEUE_ADD(playMapQueue.head, i)].console) {
        return i;
      }

      continue;
    }

    if(Q_stricmp(playMapQueue.playMap[PLAYMAP_QUEUE_ADD(playMapQueue.head, i)
                                         ].guid, guid) == 0 ) {
      return i;
    }
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
  int       i, len;
  playMap_t *playMap;
  char      *flagString;
  char      name[MAX_NAME_LENGTH + 1];

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
               playMap->mapName,
               !playMap->console ? (G_Get_Net_Name_From_GUID(playMap->guid, name, sizeof(name)) ? name : "") : "console",
               flagString ) );
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
qboolean G_PlayMapActive( void ) {
  if(g_playMapEnable.integer == PLAYMAP_INACTIVE) {
    return qfalse;
  }

  G_ValidatePlayMapQueue( );

  return (G_GetPlayMapQueueLength() > 0);
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
  playMap_t playMap;

  if(!G_PopFromPlayMapQueue(&playMap)) {
    return;
  }

  // allow a manually defined g_nextLayout setting to override the maprotation
  if( !g_nextLayout.string[ 0 ] )
  {
    Cvar_SetSafe( "g_nextLayout", playMap.layout );
  }

  G_MapConfigs( playMap.mapName );

  Cbuf_ExecuteText( EXEC_APPEND, va( "map \"%s\"\n", playMap.mapName ) );

  // too early to execute the flags
  level.playmapFlags = playMap.flags;
  //G_ExecutePlaymapFlags( playMap->flags );
}
