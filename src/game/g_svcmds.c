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

// this file holds commands that can be executed by the server console, but not remote clients

#include "g_local.h"

/*
===================
Svcmd_EntityList_f
===================
*/
void  Svcmd_EntityList_f( void )
{
  int       e;
  gentity_t *check;

  check = g_entities;

  for( e = 0; e < level.num_entities; e++, check++ )
  {
    if( !check->inuse )
      continue;

    Com_Printf( "%3i:", e );

    switch( check->s.eType )
    {
      case ET_GENERAL:
        Com_Printf( "ET_GENERAL          " );
        break;
      case ET_PLAYER:
        Com_Printf( "ET_PLAYER           " );
        break;
      case ET_ITEM:
        Com_Printf( "ET_ITEM             " );
        break;
      case ET_BUILDABLE:
        Com_Printf( "ET_BUILDABLE        " );
        break;
      case ET_LOCATION:
        Com_Printf( "ET_LOCATION         " );
        break;
      case ET_MISSILE:
        Com_Printf( "ET_MISSILE          " );
        break;
      case ET_MOVER:
        Com_Printf( "ET_MOVER            " );
        break;
      case ET_BEAM:
        Com_Printf( "ET_BEAM             " );
        break;
      case ET_PORTAL:
        Com_Printf( "ET_PORTAL           " );
        break;
      case ET_SPEAKER:
        Com_Printf( "ET_SPEAKER          " );
        break;
      case ET_PUSH_TRIGGER:
        Com_Printf( "ET_PUSH_TRIGGER     " );
        break;
      case ET_TELEPORT_TRIGGER:
        Com_Printf( "ET_TELEPORT_TRIGGER " );
        break;
      case ET_INVISIBLE:
        Com_Printf( "ET_INVISIBLE        " );
        break;
      case ET_GRAPPLE:
        Com_Printf( "ET_GRAPPLE          " );
        break;
      case ET_CORPSE:
        Com_Printf( "ET_CORPSE           " );
        break;
      case ET_PARTICLE_SYSTEM:
        Com_Printf( "ET_PARTICLE_SYSTEM  " );
        break;
      case ET_ANIMMAPOBJ:
        Com_Printf( "ET_ANIMMAPOBJ       " );
        break;
      case ET_MODELDOOR:
        Com_Printf( "ET_MODELDOOR        " );
        break;
      case ET_LIGHTFLARE:
        Com_Printf( "ET_LIGHTFLARE       " );
        break;
      case ET_LEV2_ZAP_CHAIN:
        Com_Printf( "ET_LEV2_ZAP_CHAIN   " );
        break;
      default:
        Com_Printf( "%-3i                 ", check->s.eType );
        break;
    }

    if( check->classname )
      Com_Printf( "%s", check->classname );

    Com_Printf( "\n" );
  }
}

static gclient_t *ClientForString( char *s )
{
  int  idnum;
  char err[ MAX_STRING_CHARS ];

  idnum = G_ClientNumberFromString( s, err, sizeof( err ) );
  if( idnum == -1 )
  {
    Com_Printf( "%s", err );
    return NULL;
  }

  return &level.clients[ idnum ];
}

static void Svcmd_Status_f( void )
{
  int       i;
  gclient_t *cl;
  char      userinfo[ MAX_INFO_STRING ];

  Com_Printf( "slot score ping address               rate     name\n" );
  Com_Printf( "---- ----- ---- -------               ----     ----\n" );
  for( i = 0, cl = level.clients; i < level.maxclients; i++, cl++ )
  {
    if( cl->pers.connected == CON_DISCONNECTED )
      continue;

    Com_Printf( "%-4d ", i );
    Com_Printf( "%-5d ", cl->ps.persistant[ PERS_SCORE ] );

    if( cl->pers.connected == CON_CONNECTING )
      Com_Printf( "CNCT " );
    else
      Com_Printf( "%-4d ", cl->ps.ping );

    SV_GetUserinfo( i, userinfo, sizeof( userinfo ) );
    Com_Printf( "%-21s ", Info_ValueForKey( userinfo, "ip" ) );
    Com_Printf( "%-8d ", atoi( Info_ValueForKey( userinfo, "rate" ) ) );
    Com_Printf( "%s\n", cl->pers.netname ); // Info_ValueForKey( userinfo, "name" )
  }
}

/*
===================
Svcmd_ForceTeam_f

forceteam <player> <team>
===================
*/
static void Svcmd_ForceTeam_f( void )
{
  gclient_t *cl;
  char      str[ MAX_TOKEN_CHARS ];
  team_t    team;

  if( Cmd_Argc( ) != 3 )
  {
    Com_Printf( "usage: forceteam <player> <team>\n" );
    return;
  }

  Cmd_ArgvBuffer( 1, str, sizeof( str ) );
  cl = ClientForString( str );

  if( !cl )
    return;

  Cmd_ArgvBuffer( 2, str, sizeof( str ) );
  team = G_TeamFromString( str );
  if( team == NUM_TEAMS )
  {
    Com_Printf( "forceteam: invalid team \"%s\"\n", str );
    return;
  }
  G_ChangeTeam( &g_entities[ cl - level.clients ], team );
}

/*
===================
Svcmd_LayoutSave_f

layoutsave <name>
===================
*/
static void Svcmd_LayoutSave_f( void )
{
  char str[ MAX_QPATH ];
  char str2[ MAX_QPATH - 4 ];
  char *s;
  int i = 0;
  qboolean pipeEncountered = qfalse;

  if( Cmd_Argc( ) != 2 )
  {
    Com_Printf( "usage: layoutsave <name>\n" );
    return;
  }
  Cmd_ArgvBuffer( 1, str, sizeof( str ) );

  // sanitize name
  str2[ 0 ] = '\0';
  s = &str[ 0 ];
  while( *s && i < sizeof( str2 ) - 1 )
  {
    if( isalnum( *s ) || *s == '-' || *s == '_' ||
        ( ( *s == '|' || *s == ',' ) && !pipeEncountered ) )
    {
      str2[ i++ ] = *s;
      str2[ i ] = '\0';
      if( *s == '|' )
        pipeEncountered = qtrue;
    }
    s++;
  }

  if( !str2[ 0 ] )
  {
    Com_Printf( "layoutsave: invalid name \"%s\"\n", str );
    return;
  }

  G_LayoutSave( str2 );
}

char  *ConcatArgs( int start );

/*
===================
Svcmd_LayoutLoad_f

layoutload [<name> [<name2> [<name3 [...]]]]

This is just a silly alias for doing:
 set g_nextLayout "name name2 name3"
 map_restart
===================
*/
static void Svcmd_LayoutLoad_f( void )
{
  char layouts[ MAX_CVAR_VALUE_STRING ];
  char map[ MAX_CVAR_VALUE_STRING ];
  char *s;

  if( Cmd_Argc( ) < 2 )
  {
    Com_Printf( "usage: layoutload <name> ...\n" );
    return;
  }

  s = ConcatArgs( 1 );
  Q_strncpyz( layouts, s, sizeof( layouts ) );
  Cvar_SetSafe( "g_nextLayout", layouts );
  Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
  G_MapConfigs( map );
  Cbuf_ExecuteText( EXEC_APPEND, "map_restart\n" );
  level.restarted = qtrue;
}

static void Svcmd_AdmitDefeat_f( void )
{
  int  team;
  char teamNum[ 2 ];

  if( Cmd_Argc( ) != 2 )
  {
    Com_Printf("admitdefeat: must provide a team\n");
    return;
  }
  Cmd_ArgvBuffer( 1, teamNum, sizeof( teamNum ) );
  team = G_TeamFromString( teamNum );
  if( team == TEAM_ALIENS )
  {
    G_TeamCommand( TEAM_ALIENS,
                   va( "cp \"Hivemind Link Broken\" %d",
                       CP_LIFE_SUPPORT ) );
    SV_GameSendServerCommand( -1, "print \"Alien team has admitted defeat\n\"" );
  }
  else if( team == TEAM_HUMANS )
  {
    G_TeamCommand( TEAM_HUMANS,
                   va( "cp \"Life Support Terminated\" %d",
                   CP_LIFE_SUPPORT ) );
    SV_GameSendServerCommand( -1, "print \"Human team has admitted defeat\n\"" );
  }
  else
  {
    Com_Printf("admitdefeat: invalid team\n");
    return;
  }
  level.surrenderTeam = team;
  G_BaseSelfDestruct( team );
}

static void Svcmd_Forfeit_f(void) {
  scrim_team_t scrim_team;
  char teamNum[2];

  if(!IS_SCRIM) {
    Com_Printf("forfeit: scrim mode is off\n");
    return;
  }

  if(Cmd_Argc( ) != 2) {
    Com_Printf("forfeit: must provide a scrim team number\n");
    return;
  }

  Cmd_ArgvBuffer( 1, teamNum, sizeof( teamNum ) );
  scrim_team = (scrim_team_t)atoi(teamNum);

  if(
    scrim_team == SCRIM_TEAM_NONE ||
    scrim_team >= NUM_SCRIM_TEAMS ||
    scrim_team < 0 ||
    level.scrim.team[scrim_team].current_team == TEAM_NONE) {
    Com_Printf("forfeit: invalid scrim team number\n");
    return;
  }

  SV_GameSendServerCommand(
    -1, 
    va(
      "print \"%s forfeits the scrim\n\"", level.scrim.team[scrim_team].name));
  level.scrim.scrim_forfeiter = scrim_team;
}

static void Svcmd_TeamWin_f( void )
{
  // this is largely made redundant by admitdefeat <team>
  char cmd[ 6 ];
  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );

  switch( G_TeamFromString( cmd ) )
  {
    case TEAM_ALIENS:
      G_BaseSelfDestruct( TEAM_HUMANS );
      break;

    case TEAM_HUMANS:
      G_BaseSelfDestruct( TEAM_ALIENS );
      break;

    default:
      return;
  }
}

static void Svcmd_Evacuation_f( void )
{
  int index = rand( ) % MAX_INTERMISSION_SOUND_SETS;

  if( level.exited )
    return;
  SV_GameSendServerCommand( -1, "print \"Evacuation ordered\n\"" );
  level.lastWin = TEAM_NONE;
  Q_strncpyz(
        level.winner_configstring,
        va("%i %i %i", MATCHOUTCOME_EVAC, index, TEAM_NONE),
        sizeof(level.winner_configstring));
  LogExit("Evacuation.");
}

static void Svcmd_MapRotation_f( void )
{
  char rotationName[ MAX_QPATH ];

  if( Cmd_Argc( ) != 2 )
  {
    Com_Printf( "usage: maprotation <name>\n" );
    return;
  }

  G_ClearRotationStack( );

  Cmd_ArgvBuffer( 1, rotationName, sizeof( rotationName ) );
  if( !G_StartMapRotation( rotationName, qfalse, qtrue, qfalse, 0 ) )
    Com_Printf( "maprotation: invalid map rotation \"%s\"\n", rotationName );
}

static void Svcmd_TeamMessage_f( void )
{
  char   teamNum[ 2 ];
  team_t team;

  if( Cmd_Argc( ) < 3 )
  {
    Com_Printf( "usage: say_team <team> <message>\n" );
    return;
  }

  Cmd_ArgvBuffer( 1, teamNum, sizeof( teamNum ) );
  team = G_TeamFromString( teamNum );

  if( team == NUM_TEAMS )
  {
    Com_Printf( "say_team: invalid team \"%s\"\n", teamNum );
    return;
  }

  G_TeamCommand( team, va( "chat -1 %d \"%s\"", SAY_TEAM, ConcatArgs( 2 ) ) );
  G_LogPrintf( "SayTeam: -1 \"console\": %s\n", ConcatArgs( 2 ) );
}

static void Svcmd_EjectClient_f( void )
{
  char *reason, name[ MAX_STRING_CHARS ];

  if( Cmd_Argc( ) < 2 )
  {
    Com_Printf( "usage: eject <player|-1> <reason>\n" );
    return;
  }

  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  reason = ConcatArgs( 2 );

  if( atoi( name ) == -1 )
  {
    int i;
    for( i = 0; i < level.maxclients; i++ )
    {
      if( level.clients[ i ].pers.connected == CON_DISCONNECTED )
        continue;
      if( level.clients[ i ].pers.localClient )
        continue;
      SV_GameDropClient( i, reason );
    }
  }
  else
  {
    gclient_t *cl = ClientForString( name );
    if( !cl )
      return;
    if( cl->pers.localClient )
    {
      Com_Printf( "eject: cannot eject local clients\n" );
      return;
    }
    SV_GameDropClient( cl-level.clients, reason );
  }
}

static void Svcmd_DumpUser_f( void )
{
  char name[ MAX_STRING_CHARS ], userinfo[ MAX_INFO_STRING ];
  char key[ BIG_INFO_KEY ], value[ BIG_INFO_VALUE ];
  const char *info;
  gclient_t *cl;

  if( Cmd_Argc( ) != 2 )
  {
    Com_Printf( "usage: dumpuser <player>\n" );
    return;
  }

  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  cl = ClientForString( name );
  if( !cl )
    return;

  SV_GetUserinfo( cl-level.clients, userinfo, sizeof( userinfo ) );
  info = &userinfo[ 0 ];
  Com_Printf( "userinfo\n--------\n" );
  //Info_Print( userinfo );
  while( 1 )
  {
    Info_NextPair( &info, key, value );
    if( !*info )
      return;

    Com_Printf( "%-20s%s\n", key, value );
  }
}

static void Svcmd_Pr_f( void )
{
  char targ[ 4 ];
  int cl;

  if( Cmd_Argc( ) < 3 )
  {
    Com_Printf( "usage: <clientnum|-1> <message>\n" );
    return;
  }

  Cmd_ArgvBuffer( 1, targ, sizeof( targ ) );
  cl = atoi( targ );

  if( cl >= MAX_CLIENTS || cl < -1 )
  {
    Com_Printf( "invalid clientnum %d\n", cl );
    return;
  }

  SV_GameSendServerCommand( cl, va( "print \"%s\n\"", ConcatArgs( 2 ) ) );
}

// dumb wrapper for "a", "m", "chat", and "say"
static void Svcmd_MessageWrapper( void )
{
  char cmd[ 5 ];
  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );

  if( !Q_stricmp( cmd, "a" ) )
    Cmd_AdminMessage_f( NULL );
  else if( !Q_stricmp( cmd, "m" ) )
    Cmd_PrivateMessage_f( NULL );
  else if( !Q_stricmp( cmd, "say" ) )
    G_Say( NULL, SAY_ALL, ConcatArgs( 1 ) );
  else if( !Q_stricmp( cmd, "chat" ) )
    G_Say( NULL, SAY_RAW, ConcatArgs( 1 ) );
}

static void Svcmd_ListMapsWrapper( void )
{
  Cmd_ListMaps_f( NULL );
}

static void Svcmd_SuddenDeath_f( void )
{
  char secs[ 5 ];
  int  offset;
  Cmd_ArgvBuffer( 1, secs, sizeof( secs ) );
  offset = atoi( secs );

  level.suddenDeathBeginTime = level.time - level.startTime + offset * 1000;
  SV_GameSendServerCommand( -1,
    va( "cp \"Sudden Death will begin in %d second%s\" %d",
        offset, offset == 1 ? "" : "s", CP_SUDDEN_DEATH ) );
}

static void Svcmd_Extend_f( void )
{
  char mins[ 16 ] = {""};
  int  offset;
  Cmd_ArgvBuffer( 1, mins, sizeof( mins ) );
  offset = atoi( mins );

  level.extendTimeLimit += offset;
  SV_GameSendServerCommand( -1,
    va( "cp \"The time limit has been ^2extended^7 by %d minute%s\" %d",
        offset, offset == 1 ? "" : "s", CP_TIME_LIMIT ) );
}

static void Svcmd_G_AdvanceMapRotation_f( void )
{
  G_AdvanceMapRotation( 0 );
}

static void Svcmd_G_MemoryInfo( void ) {
  BG_MemoryInfo( );

  //local custom allocators
  G_Unlagged_Memory_Info( );
}

struct svcmd
{
  char     *cmd;
  qboolean dedicated;
  void     ( *function )( void );
} svcmds[ ] = {
  { "a", qtrue, Svcmd_MessageWrapper },
  { "admitDefeat", qfalse, Svcmd_AdmitDefeat_f },
  { "advanceMapRotation", qfalse, Svcmd_G_AdvanceMapRotation_f },
  { "alienWin", qfalse, Svcmd_TeamWin_f },
  { "chat", qtrue, Svcmd_MessageWrapper },
  { "dumpuser", qfalse, Svcmd_DumpUser_f },
  { "eject", qfalse, Svcmd_EjectClient_f },
  { "entityList", qfalse, Svcmd_EntityList_f },
  { "evacuation", qfalse, Svcmd_Evacuation_f },
  { "extend", qfalse, Svcmd_Extend_f },
  { "forceTeam", qfalse, Svcmd_ForceTeam_f },
  { "forfeit", qfalse, Svcmd_Forfeit_f },
  { "game_memory", qfalse, Svcmd_G_MemoryInfo },
  { "humanWin", qfalse, Svcmd_TeamWin_f },
  { "layoutLoad", qfalse, Svcmd_LayoutLoad_f },
  { "layoutSave", qfalse, Svcmd_LayoutSave_f },
  { "listmaps", qtrue, Svcmd_ListMapsWrapper },
  { "loadcensors", qfalse, G_LoadCensors },
  { "m", qtrue, Svcmd_MessageWrapper },
  { "mapRotation", qfalse, Svcmd_MapRotation_f },
  { "pr", qfalse, Svcmd_Pr_f },
  { "say", qtrue, Svcmd_MessageWrapper },
  { "say_team", qtrue, Svcmd_TeamMessage_f },
  { "status", qfalse, Svcmd_Status_f },
  { "stopMapRotation", qfalse, G_StopMapRotation },
  { "suddendeath", qfalse, Svcmd_SuddenDeath_f }
};

/*
=================
ConsoleCommand

=================
*/
Q_EXPORT qboolean  ConsoleCommand( void )
{
  char cmd[ MAX_TOKEN_CHARS ];
  struct svcmd *command;

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );

  command = bsearch( cmd, svcmds, ARRAY_LEN( svcmds ),
    sizeof( struct svcmd ), cmdcmp );

  if( !command )
  {
    // see if this is an admin command
    if( G_admin_cmd_check( NULL ) )
      return qtrue;

    if( g_dedicated.integer )
      Com_Printf( "unknown command: %s\n", cmd );

    return qfalse;
  }

  if( command->dedicated && !g_dedicated.integer )
    return qfalse;

  command->function( );
  return qtrue;
}

void G_RegisterCommands( void )
{
  int i;

  for( i = 0; i < ARRAY_LEN( svcmds ); i++ )
  {
    if( svcmds[ i ].dedicated && !g_dedicated.integer )
      continue;
    Cmd_AddCommand( svcmds[ i ].cmd, NULL );
  }

  G_admin_register_cmds( );
}

void G_UnregisterCommands( void )
{
  int i;

  for( i = 0; i < ARRAY_LEN( svcmds ); i++ )
  {
    if( svcmds[ i ].dedicated && !g_dedicated.integer )
      continue;
    Cmd_RemoveCommand( svcmds[ i ].cmd );
  }

  G_admin_unregister_cmds( );
}
