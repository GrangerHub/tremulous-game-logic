/*
===========================================================================
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2018 GrangerHub

This file is part of Tremulous.

This shrubbot implementation is the original work of Tony J. White.

Contains contributions from Wesley van Beelen, Chris Bajumpaa, Josh Menke,
and Travis Maurer.

The functionality of this code mimics the behaviour of the currently
inactive project shrubet (http://www.etstats.com/shrubet/index.php?ver=2)
by Ryan Mannion.   However, shrubet was a closed-source project and
none of its code has been copied, only its functionality.

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

#include "g_local.h"

// big ugly global buffer for use with buffered printing of long outputs
static char g_bfb[ 32000 ];

// note: list ordered alphabetically
g_admin_cmd_t g_admin_cmds[ ] =
  {
    {"addlayout", G_admin_addlayout, qfalse, "addlayout",
      "place layout elements into the game. the elements are specified by a "
      "union of filtered layouts. the syntax is demonstrated by an example: "
      "^5reactor,telenode|westside+alien|sewers^7 will place only the "
      "reactor and telenodes from the westside layout, and also all alien "
      "layout elements from the sewers layout",
      "[^3layoutelements^7]"
    },

    {"adjustban", G_admin_adjustban, qfalse, "ban",
      "change the IP address mask, duration or reason of a ban.  mask is "
      "prefixed with '/'.  duration is specified as numbers followed by units "
      " 'w' (weeks), 'd' (days), 'h' (hours) or 'm' (minutes), or seconds if "
      " no unit is specified.  if the duration is preceded by a + or -, the "
      "ban duration will be extended or shortened by the specified amount",
      "[^3ban#^7] (^5/mask^7) (^5duration^7) (^5reason^7)"
    },

    {"adminhelp", G_admin_adminhelp, qtrue, "adminhelp",
      "display admin commands available to you or help on a specific command",
      "(^5command^7)"
    },

    {"admintest", G_admin_admintest, qfalse, "admintest",
      "display your current admin level",
      ""
    },

    {"allowbuild", G_admin_denybuild, qfalse, "denybuild",
      "restore a player's ability to build",
      "[^3name^6|^3slot#^7]"
    },

    {"allready", G_admin_allready, qfalse, "allready",
      "makes everyone ready in intermission or developer mode pre-game warmup",
      ""
    },

    {"ban", G_admin_ban, qfalse, "ban",
      "ban a player by IP and GUID with an optional expiration time and reason."
      " duration is specified as numbers followed by units 'w' (weeks), 'd' "
      "(days), 'h' (hours) or 'm' (minutes), or seconds if no units are "
      "specified",
      "[^3name^6|^3slot#^6|^3IP(/mask)^7] (^5duration^7) (^5reason^7)"
    },

    {"builder", G_admin_builder, qtrue, "builder",
      "show who built a structure",
      ""
    },

    {"buildlog", G_admin_buildlog, qfalse, "buildlog",
      "show buildable log",
      "(^5name^6|^5slot#^7) (^5id^7)"
    },

    {"cancelvote", G_admin_endvote, qfalse, "cancelvote",
      "cancel a vote taking place",
      "(^5a^6|^5h^7)"
    },

    {"changemap", G_admin_changemap, qfalse, "changemap",
      "load a map (and optionally force layout)",
      "[^3mapname^7] (^5layout^7)"
    },

    {"cpa", G_admin_cp, qfalse, "cpa",
      "display a brief Center Print Announcement message to players, optionally specifying a team to send to",
      "(^5-A|H|S^7) [^3message^7]"
    },

    {"denybuild", G_admin_denybuild, qfalse, "denybuild",
      "take away a player's ability to build",
      "[^3name^6|^3slot#^7]"
    },

    {"explode", G_admin_explode, qfalse, "explode",
      "Blow up a player",
      "[^3name^6|^3slot#^7] (^5reason^7)"
    },

    {"flag", G_admin_flag, qtrue, "flag",
      "add an admin flag to a player, prefix flag with '-' to disallow the flag. "
      "console can use this command on admin levels by prefacing a '*' to the admin level value.",
      "[^3name^6|^3slot#^6|^3*adminlevel^7] (^5+^6|^5-^7)[^3flag^7]"
    },

    {"forcespec", G_admin_forcespec, qfalse, "forcespec",
      "temporarily force a player to stay on the spectator team",
      "[^3name^6|^3slot#^6|^3namelog#]"},

    {"gamedir", G_admin_gamedir, qtrue, "gamedir",
      "Filter files on the server",
      "[^3dir^7] [^3extension^7] [^3filter^7]"
    },

    {"kick", G_admin_kick, qfalse, "kick",
      "kick a player with an optional reason",
      "[^3name^6|^3slot#^7] (^5reason^7)"
    },

    {"layoutsave", G_admin_layoutsave, qfalse, "layoutsave",
      "save a map layout",
      "[^3layout^7]"
    },

    {"listadmins", G_admin_listadmins, qtrue, "listadmins",
      "display a list of all server admins and their levels",
      "(^5name^7) (^5start admin#^7)"
    },

    {"listflags", G_admin_listflags, qtrue, "flag",
     "list all flags understood by this server",
     "[^3name^6|^3slot#^7]"
    },

    {"listlayouts", G_admin_listlayouts, qtrue, "listlayouts",
      "display a list of all available layouts for a map",
      "(^5mapname^7)"
    },

    {"listplayers", G_admin_listplayers, qtrue, "listplayers",
      "display a list of players, their client numbers and their levels",
      ""
    },

    {"lock", G_admin_lock, qfalse, "lock",
      "lock a team to prevent anyone from joining it",
      "[^3a^6|^3h^7]"
    },

    {"maplog", G_admin_maplog, qfalse, "maplog",
      "retrieve the map log",
      "[^3offset(optional)^7]"
    },

    {"mute", G_admin_mute, qfalse, "mute",
      "mute a player",
      "[^3name^6|^3slot#^7]"
    },

    {"namelog", G_admin_namelog, qtrue, "namelog",
      "display a list of names used by recently connected players",
      "(^5name^6|^5IP(/mask)^7) (^5start namelog#^7)"
    },

    {"nextmap", G_admin_nextmap, qfalse, "nextmap",
      "go to the next map in the cycle",
      ""
    },

    {"passvote", G_admin_endvote, qfalse, "passvote",
      "pass a vote currently taking place",
      "(^5a^6|^5h^7)"
    },

    {"pause", G_admin_pause, qfalse, "pause",
      "Pause (or unpause) the game.",
      ""
    },

    {"playmap", G_admin_playmap, qtrue, "playmap",
     "List and add to playmap queue.",
     "^3mapname [layout] [flags]^7\n(Press TAB to complete map names!)"
    },

    {"playpool", G_admin_playpool, qtrue, "playpool",
      "Manage the playmap pool.",
      "[^3add (mapname)^6|^3remove (mapname)^6|^3clear^6|^3"
     "list (pagenum)^6|^3reload^6|^3save^7]"
    },

    {"putteam", G_admin_putteam, qfalse, "putteam",
      "move a player to a specified team",
      "[^3name^6|^3slot#^6|^3namelog#] [^3h^6|^3a^6|^3s^7]"
    },

    {"readconfig", G_admin_readconfig, qfalse, "readconfig",
      "reloads the admin config file and refreshes permission flags",
      ""
    },

    {"register", G_admin_register, qfalse, "register",
      "register your name to protect it from being used by others. "
      "use 'register 0' to remove name protection.",
      "(^5level^7)"
    },

    {"rename", G_admin_rename, qfalse, "rename",
      "rename a player",
      "[^3name^6|^3slot#^7] [^3new name^7]"
    },

    {"restart", G_admin_restart, qfalse, "restart",
      "restart the current map (optionally using named layout or keeping/switching teams)",
      "(^5layout^7) (^5keepteams^6|^5switchteams^6|^5keepteamslock^6|^5switchteamslock^7)"
    },

    {"revert", G_admin_revert, qfalse, "revert",
      "revert buildables to a given time",
      "[^3id^7]"
    },

    {"scrim", G_admin_scrim, qfalse, "scrim",
      "sets up and manages scrims, to check the current scrim settings execute with no arguments",
      "[^3on^6|^3off^7^6|^3restore_defaults^6|^3start^6|^3pause^6|^3win^6|^3timed_income^6|^3map^6|^3putteam^6|^3sd_mode^6|^3sd_time^6|^3time_limit^6|^3team_name^6|^3team_captain^6|^3max_rounds^6|^3roster^6|^3remove^7]"
    },

    {"seen", G_admin_seen, qfalse, "seen",
      "search for when the last time a player was on",
      "[^3name^7] [^3offset(optional)^7]"
    },

    {"setdevmode", G_admin_setdevmode, qfalse, "setdevmode",
      "switch developer mode on or off",
      "[^3on^6|^3off^7]"
    },

    {"setivo", G_admin_setivo, qfalse, "setivo",
      "set an intermission view override",
      "[^3s^6|^3a^6|^3h^7]"
    },

    {"setlevel", G_admin_setlevel, qfalse, "setlevel",
      "sets the admin level of a player",
      "[^3name^6|^3slot#^6|^3admin#^6] [^3level^7]"
    },

    {"setnextmap", G_admin_setnextmap, qfalse, "setnextmap",
      "set the next map (and, optionally, a forced layout)",
      "[^3mapname^7] (^5layout^7)"
    },

    {"showbans", G_admin_showbans, qtrue, "showbans",
      "display a (partial) list of active bans",
      "(^5name^6|^5IP(/mask)^7) (^5start at ban#^7)"
    },

    {"slap", G_admin_slap, qtrue, "slap",
      "Do damage to a player, and send them flying",
      "[^3name^6|^3slot^7]"
    },

    {"spec999", G_admin_spec999, qfalse, "spec999",
      "move 999 pingers to the spectator team",
      ""},

    {"time", G_admin_time, qtrue, "time",
      "show the current local server time",
      ""},

    {"transform", G_admin_transform, qfalse, "magic",
      "change a human player to a different player model",
      "[^3name^6|^3slot#^7] [^3player model^7]"
    },

    {"unban", G_admin_unban, qfalse, "ban",
      "unbans a player specified by the slot as seen in showbans",
      "[^3ban#^7]"
    },

    {"unlock", G_admin_lock, qfalse, "lock",
      "unlock a locked team",
      "[^3a^6|^3h^7]"
    },

    {"unmute", G_admin_mute, qfalse, "mute",
      "unmute a muted player",
      "[^3name^6|^3slot#^7]"
    },

    {
     "warn", G_admin_warn, qfalse, "warn",
      "warn a player to correct their current activity",
      "[^3name^6|^3slot#^7] [^3reason^7]"
    }
  };

static size_t adminNumCmds = ARRAY_LEN( g_admin_cmds );

static int admin_level_maxname = 0;
g_admin_level_t *g_admin_levels = NULL;
g_admin_admin_t *g_admin_admins = NULL;
g_admin_ban_t *g_admin_bans = NULL;
g_admin_command_t *g_admin_commands = NULL;

void G_admin_register_cmds( void )
{
  int i;

  for( i = 0; i < adminNumCmds; i++ )
    Cmd_AddCommand( g_admin_cmds[ i ].keyword, NULL );
}

void G_admin_unregister_cmds( void )
{
  int i;

  for( i = 0; i < adminNumCmds; i++ )
    Cmd_RemoveCommand( g_admin_cmds[ i ].keyword );
}

void G_admin_cmdlist( gentity_t *ent )
{
  int   i;
  char  out[ MAX_STRING_CHARS ] = "";
  int   len, outlen;

  outlen = 0;

  for( i = 0; i < adminNumCmds; i++ )
  {
    if( !G_admin_permission( ent, g_admin_cmds[ i ].flag ) )
      continue;

    len = strlen( g_admin_cmds[ i ].keyword ) + 1;
    if( len + outlen >= sizeof( out ) - 1 )
    {
      SV_GameSendServerCommand( ent - g_entities, va( "cmds%s\n", out ) );
      outlen = 0;
    }

    strcpy( out + outlen, va( " %s", g_admin_cmds[ i ].keyword ) );
    outlen += len;
  }

  SV_GameSendServerCommand( ent - g_entities, va( "cmds%s\n", out ) );
}

// match a certain flag within these flags
static qboolean admin_permission( char *flags, const char *flag, qboolean *perm )
{
  char *token, *token_p = flags;
  qboolean allflags = qfalse;
  qboolean p = qfalse;
  *perm = qfalse;
  while( *( token = COM_Parse( &token_p ) ) )
  {
    *perm = qtrue;
    if( *token == '-' || *token == '+' )
      *perm = *token++ == '+';
    if( !strcmp( token, flag ) )
      return qtrue;
    if( !strcmp( token, ADMF_ALLFLAGS ) )
    {
      allflags = qtrue;
      p = *perm;
    }
  }
  if( allflags )
    *perm = p;
  return allflags;
}

g_admin_cmd_t *G_admin_cmd( const char *cmd )
{
  return bsearch( cmd, g_admin_cmds, adminNumCmds, sizeof( g_admin_cmd_t ),
    cmdcmp );
}

g_admin_level_t *G_admin_level( const int l )
{
  g_admin_level_t *level;

  for( level = g_admin_levels; level; level = level->next )
  {
    if( level->level == l )
      return level;
  }

  return NULL;
}

g_admin_admin_t *G_admin_admin( const char *guid )
{
  g_admin_admin_t *admin;

  for( admin = g_admin_admins; admin; admin = admin->next )
  {
    if( !Q_stricmp( admin->guid, guid ) )
      return admin;
  }

  return NULL;
}

g_admin_command_t *G_admin_command( const char *cmd )
{
  g_admin_command_t *c;

  for( c = g_admin_commands; c; c = c->next )
  {
    if( !Q_stricmp( c->command, cmd ) )
      return c;
  }

  return NULL;
}

qboolean G_admin_permission( gentity_t *ent, const char *flag )
{
  qboolean perm;
  g_admin_admin_t *a;
  g_admin_level_t *l;

  // console always wins
  if( !ent )
    return qtrue;

  if( ( a = ent->client->pers.admin ) )
  {
    if( admin_permission( a->flags, flag, &perm ) )
      return perm;

    l = G_admin_level( a->level );
  }
  else
    l = G_admin_level( 0 );

  if( l )
    return admin_permission( l->flags, flag, &perm ) && perm;

  return qfalse;
}

qboolean G_admin_name_check( gentity_t *ent, char *name, char *err, int len )
{
  int i;
  gclient_t *client;
  char testName[ MAX_NAME_LENGTH ] = {""};
  char name2[ MAX_NAME_LENGTH ] = {""};
  g_admin_admin_t *admin;
  int alphaCount = 0;

  G_SanitiseString( name, name2, sizeof( name2 ) );

  if( !strcmp( name2, "unnamedplayer" ) )
    return qtrue;

  if( !strcmp( name2, "console" ) )
  {
    if( err && len > 0 )
      Q_strncpyz( err, "The name 'console' is not allowed.", len );
    return qfalse;
  }

  G_DecolorString( name, testName, sizeof( testName ) );
  if( isdigit( testName[ 0 ] ) )
  {
    if( err && len > 0 )
      Q_strncpyz( err, "Names cannot begin with numbers", len );
    return qfalse;
  }

  for( i = 0; testName[ i ]; i++)
  {
    if( isalpha( testName[ i ] ) )
     alphaCount++;
  }

  if( alphaCount == 0 )
  {
    if( err && len > 0 )
      Q_strncpyz( err, "Names must contain letters", len );
    return qfalse;
  }

  for( i = 0; i < level.maxclients; i++ )
  {
    client = &level.clients[ i ];
    if( client->pers.connected == CON_DISCONNECTED )
      continue;

    // can rename ones self to the same name using different colors
    if( i == ( ent - g_entities ) )
      continue;

    G_SanitiseString( client->pers.netname, testName, sizeof( testName ) );
    if( !strcmp( name2, testName ) )
    {
      if( err && len > 0 )
        Com_sprintf( err, len, "The name '%s^7' is already in use", name );
      return qfalse;
    }
  }

  for( admin = g_admin_admins; admin; admin = admin->next )
  {
    if( admin->level < 1 )
      continue;
    G_SanitiseString( admin->name, testName, sizeof( testName ) );
    if( !strcmp( name2, testName ) && ent->client->pers.admin != admin )
    {
      if( err && len > 0 )
        Com_sprintf( err, len, "The name '%s^7' belongs to an admin, "
                     "please use another name", name );
      return qfalse;
    }
  }

  if(IS_SCRIM) {
    scrim_team_t scrim_team;
    int          roster_index;

    //check if the name is in use in the scrim roster
    for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
      if(scrim_team == SCRIM_TEAM_NONE) {
        continue;
      }
      for(roster_index = 0; roster_index < 64; roster_index++) {
        if(!level.scrim_team_rosters[scrim_team].members[roster_index].inuse) {
          continue;
        }

        if(level.scrim_team_rosters[scrim_team].members[roster_index].guidless) {
          if(ent->client->pers.guidless && G_AddressCompare(
            &ent->client->pers.ip,
            &level.scrim_team_rosters[scrim_team].members[roster_index].ip)) {
            continue;
          }
        } else if(!ent->client->pers.guidless) {
          if(
            !Q_stricmp(
              ent->client->pers.guid,
              level.scrim_team_rosters[scrim_team].members[roster_index].guid)) {
            continue;
          }
        }

        G_SanitiseString(
          level.scrim_team_rosters[scrim_team].members[roster_index].netname,
          testName,
          sizeof(testName));

          if(!strcmp( name2, testName )){
            if( err && len > 0 ) {
              Com_sprintf( err, len, "The name '%s^7' is in use in the scrim roster, "
                           "please use another name", name );
            }
            return qfalse;
          }
      }
    }
  }

  return qtrue;
}

static qboolean admin_higher_admin( g_admin_admin_t *a, g_admin_admin_t *b )
{
  qboolean perm;

  if( !b )
    return qtrue;

  if( admin_permission( b->flags, ADMF_IMMUTABLE, &perm ) )
    return !perm;

  return b->level <= ( a ? a->level : 0 );
}

static qboolean admin_higher_guid( char *admin_guid, char *victim_guid )
{
  return admin_higher_admin( G_admin_admin( admin_guid ),
    G_admin_admin( victim_guid ) );
}

static qboolean admin_higher( gentity_t *admin, gentity_t *victim )
{

  // console always wins
  if( !admin )
    return qtrue;

  return admin_higher_admin( admin->client->pers.admin,
    victim->client->pers.admin );
}

static void admin_writeconfig_string( char *s, fileHandle_t f )
{
  if( s[ 0 ] )
    FS_Write( s, strlen( s ), f );
  FS_Write( "\n", 1, f );
}

static void admin_writeconfig_int( int v, fileHandle_t f )
{
  char buf[ 32 ];

  Com_sprintf( buf, sizeof( buf ), "%d\n", v );
  FS_Write( buf, strlen( buf ), f );
}

static void admin_writeconfig( void )
{
  fileHandle_t f;
  int t;
  g_admin_admin_t *a;
  g_admin_level_t *l;
  g_admin_ban_t *b;
  g_admin_command_t *c;

  if( !g_admin.string[ 0 ] )
  {
    Com_Printf( S_COLOR_YELLOW "WARNING: g_admin is not set. "
      " configuration will not be saved to a file.\n" );
    return;
  }
  t = Com_RealTime( NULL );
  if( FS_FOpenFileByMode( g_admin.string, &f, FS_WRITE ) < 0 )
  {
    Com_Printf( "admin_writeconfig: could not open g_admin file \"%s\"\n",
              g_admin.string );
    return;
  }
  for( l = g_admin_levels; l; l = l->next )
  {
    FS_Write( "[level]\n", 8, f );
    FS_Write( "level   = ", 10, f );
    admin_writeconfig_int( l->level, f );
    FS_Write( "name    = ", 10, f );
    admin_writeconfig_string( l->name, f );
    FS_Write( "flags   = ", 10, f );
    admin_writeconfig_string( l->flags, f );
    FS_Write( "\n", 1, f );
  }
  for( a = g_admin_admins; a; a = a->next )
  {
    // don't write level 0 users
    if( a->level == 0 )
      continue;

    FS_Write( "[admin]\n", 8, f );
    FS_Write( "name    = ", 10, f );
    admin_writeconfig_string( a->name, f );
    FS_Write( "guid    = ", 10, f );
    admin_writeconfig_string( a->guid, f );
    FS_Write( "level   = ", 10, f );
    admin_writeconfig_int( a->level, f );
    FS_Write( "flags   = ", 10, f );
    admin_writeconfig_string( a->flags, f );
    FS_Write( "\n", 1, f );
  }
  for( b = g_admin_bans; b; b = b->next )
  {
    // don't write expired bans
    // if expires is 0, then it's a perm ban
    if( b->expires != 0 && b->expires <= t )
      continue;

    FS_Write( "[ban]\n", 6, f );
    FS_Write( "name    = ", 10, f );
    admin_writeconfig_string( b->name, f );
    FS_Write( "guid    = ", 10, f );
    admin_writeconfig_string( b->guid, f );
    FS_Write( "ip      = ", 10, f );
    admin_writeconfig_string( b->ip.str, f );
    FS_Write( "reason  = ", 10, f );
    admin_writeconfig_string( b->reason, f );
    FS_Write( "made    = ", 10, f );
    admin_writeconfig_string( b->made, f );
    FS_Write( "expires = ", 10, f );
    admin_writeconfig_int( b->expires, f );
    FS_Write( "banner  = ", 10, f );
    admin_writeconfig_string( b->banner, f );
    FS_Write( "\n", 1, f );
  }
  for( c = g_admin_commands; c; c = c->next )
  {
    FS_Write( "[command]\n", 10, f );
    FS_Write( "command = ", 10, f );
    admin_writeconfig_string( c->command, f );
    FS_Write( "exec    = ", 10, f );
    admin_writeconfig_string( c->exec, f );
    FS_Write( "desc    = ", 10, f );
    admin_writeconfig_string( c->desc, f );
    FS_Write( "flag    = ", 10, f );
    admin_writeconfig_string( c->flag, f );
    FS_Write( "\n", 1, f );
  }
  FS_FCloseFile( f );
}

static void admin_readconfig_string( char **cnf, char *s, int size )
{
  char *t;

  //COM_MatchToken(cnf, "=");
  s[ 0 ] = '\0';
  t = COM_ParseExt( cnf, qfalse );
  if( strcmp( t, "=" ) )
  {
    COM_ParseWarning( "expected '=' before \"%s\"", t );
    Q_strncpyz( s, t, size );
  }
  while( 1 )
  {
    t = COM_ParseExt( cnf, qfalse );
    if( !*t )
      break;
    if( strlen( t ) + strlen( s ) >= size )
      break;
    if( *s )
      Q_strcat( s, size, " " );
    Q_strcat( s, size, t );
  }
}

static void admin_readconfig_int( char **cnf, int *v )
{
  char *t;

  //COM_MatchToken(cnf, "=");
  t = COM_ParseExt( cnf, qfalse );
  if( !strcmp( t, "=" ) )
  {
    t = COM_ParseExt( cnf, qfalse );
  }
  else
  {
    COM_ParseWarning( "expected '=' before \"%s\"", t );
  }
  *v = atoi( t );
}

// if we can't parse any levels from readconfig, set up default
// ones to make new installs easier for admins
static void admin_default_levels( void )
{
  g_admin_level_t *l;
  int             level = 0;

  l = g_admin_levels = BG_Alloc0( sizeof( g_admin_level_t ) );
  l->level = level++;
  Q_strncpyz( l->name, "^4Unknown Player", sizeof( l->name ) );
  Q_strncpyz( l->flags,
    "listplayers admintest adminhelp time register",
    sizeof( l->flags ) );

  l = l->next = BG_Alloc0( sizeof( g_admin_level_t ) );
  l->level = level++;
  Q_strncpyz( l->name, "^5Server Regular", sizeof( l->name ) );
  Q_strncpyz( l->flags,
    "listplayers admintest adminhelp time",
    sizeof( l->flags ) );

  l = l->next = BG_Alloc0( sizeof( g_admin_level_t ) );
  l->level = level++;
  Q_strncpyz( l->name, "^6Team Manager", sizeof( l->name ) );
  Q_strncpyz( l->flags,
    "listplayers admintest adminhelp time putteam spec999 forcepec",
    sizeof( l->flags ) );

  l = l->next = BG_Alloc0( sizeof( g_admin_level_t ) );
  l->level = level++;
  Q_strncpyz( l->name, "^2Junior Admin", sizeof( l->name ) );
  Q_strncpyz( l->flags,
    "listplayers admintest adminhelp time putteam spec999 forcepec kick mute ADMINCHAT",
    sizeof( l->flags ) );

  l = l->next = BG_Alloc0( sizeof( g_admin_level_t ) );
  l->level = level++;
  Q_strncpyz( l->name, "^3Senior Admin", sizeof( l->name ) );
  Q_strncpyz( l->flags,
    "listplayers admintest adminhelp time register rename putteam spec999 forcepec kick mute showbans ban "
    "namelog ADMINCHAT",
    sizeof( l->flags ) );

  l = l->next = BG_Alloc0( sizeof( g_admin_level_t ) );
  l->level = level++;
  Q_strncpyz( l->name, "^1Server Operator", sizeof( l->name ) );
  Q_strncpyz( l->flags,
    "ALLFLAGS -IMMUTABLE -INCOGNITO",
    sizeof( l->flags ) );
  admin_level_maxname = 15;
}

void G_admin_authlog( gentity_t *ent )
{
  char            aflags[ MAX_ADMIN_FLAGS * 2 ];
  g_admin_level_t *level;
  int             levelNum = 0;

  if( !ent )
    return;

  if( ent->client->pers.admin )
    levelNum = ent->client->pers.admin->level;

  level = G_admin_level( levelNum );

  Com_sprintf( aflags, sizeof( aflags ), "%s %s",
               ent->client->pers.admin->flags,
               ( level ) ? level->flags : "" );

  G_LogPrintf( "AdminAuth: %i \"%s" S_COLOR_WHITE "\" \"%s" S_COLOR_WHITE
               "\" [%d] (%s): %s\n",
               (int)( ent - g_entities ), ent->client->pers.netname,
               ent->client->pers.admin->name, ent->client->pers.admin->level,
               ent->client->pers.guid, aflags );
}

static char adminLog[ MAX_STRING_CHARS ];
static int  adminLogLen;
static void admin_log_start( gentity_t *admin, const char *cmd )
{
  const char *name = admin ? admin->client->pers.netname : "console";

  adminLogLen = Q_snprintf( adminLog, sizeof( adminLog ),
    "%d \"%s" S_COLOR_WHITE "\" \"%s" S_COLOR_WHITE "\" [%d] (%s): %s",
    admin ? admin->s.clientNum : -1,
    name,
    admin && admin->client->pers.admin ? admin->client->pers.admin->name : name,
    admin && admin->client->pers.admin ? admin->client->pers.admin->level : 0,
    admin ? admin->client->pers.guid : "",
    cmd );
}

static void admin_log( const char *str )
{
  if( adminLog[ 0 ] )
    adminLogLen += Q_snprintf( adminLog + adminLogLen,
      sizeof( adminLog ) - adminLogLen, ": %s", str );
}

static void admin_log_abort( void )
{
  adminLog[ 0 ] = '\0';
  adminLogLen = 0;
}

static void admin_log_end( const qboolean ok )
{
  if( adminLog[ 0 ] )
    G_LogPrintf( "AdminExec: %s: %s\n", ok ? "ok" : "fail", adminLog );
  admin_log_abort( );
}

struct llist
{
  struct llist *next;
};
static int admin_search( gentity_t *ent,
  const char *cmd,
  const char *noun,
  qboolean ( *match )( void *, const void * ),
  void ( *out )( void *, char * ),
  const void *list,
  const void *arg, /* this will be used as char* later */
  int start,
  const int offset,
  const int limit )
{
  int i;
  int count = 0;
  int found = 0;
  int total;
  int next = 0, end = 0;
  char str[ MAX_STRING_CHARS ];
  struct llist *l = (struct llist *)list;

  for( total = 0; l; total++, l = l->next ) ;
  if( start < 0 )
    start += total;
  else
    start -= offset;
  if( start < 0 || start > total )
    start = 0;

  ADMBP_begin();
  for( i = 0, l = (struct llist *)list; l; i++, l = l->next )
  {
    if( match( l, arg ) )
    {
      if( i >= start && ( limit < 1 || count < limit ) )
      {
        out( l, str );
        ADMBP( va( "%-3d %s\n", i + offset, str ) );
        count++;
        end = i;
      }
      else if( count == limit )
      {
        if( next == 0 )
          next = i;
      }

      found++;
    }
  }

  if( limit > 0 )
  {
    ADMBP( va( "^3%s: ^7showing %d of %d %s %d-%d%s%s.",
      cmd, count, found, noun, start + offset, end + offset,
      *(char *)arg ? " matching " : "", (char *)arg ) );
    if( next )
      ADMBP( va( "  use '%s%s%s %d' to see more", cmd,
        *(char *)arg ? " " : "",
        (char *)arg,
        next + offset ) );
  }
  ADMBP( "\n" );
  ADMBP_end();
  return next + offset;
}

static qboolean admin_match( void *admin, const void *match )
{
  char n1[ MAX_NAME_LENGTH ], n2[ MAX_NAME_LENGTH ];
  G_SanitiseString( (char *)match, n2, sizeof( n2 ) );
  if( !n2[ 0 ] )
    return qtrue;
  G_SanitiseString( ( (g_admin_admin_t *)admin )->name, n1, sizeof( n1 ) );
  return strstr( n1, n2 ) ? qtrue : qfalse;
}
static void admin_out( void *admin, char *str )
{
  g_admin_admin_t *a = (g_admin_admin_t *)admin;
  g_admin_level_t *l = G_admin_level( a->level );
  int lncol = 0, i;
  for( i = 0; l && l->name[ i ]; i++ )
  {
    if( Q_IsColorString( l->name + i ) )
      lncol += 2;
  }
  Com_sprintf( str, MAX_STRING_CHARS, "%-6d %*s^7 %s",
    a->level, admin_level_maxname + lncol - 1, l ? l->name : "(null)",
    a->name );
}
static int admin_listadmins( gentity_t *ent, int start, char *search )
{
  return admin_search( ent, "listadmins", "admins", admin_match, admin_out,
    g_admin_admins, search, start, MAX_CLIENTS, MAX_ADMIN_LISTITEMS );
}

#define MAX_DURATION_LENGTH 13
void G_admin_duration( int secs, char *duration, int dursize )
{
  // sizeof("12.5 minutes") == 13
  if( secs > ( 60 * 60 * 24 * 365 * 50 ) || secs < 0 )
    Q_strncpyz( duration, "PERMANENT", dursize );
  else if( secs >= ( 60 * 60 * 24 * 365 ) )
    Com_sprintf( duration, dursize, "%1.1f years",
      ( secs / ( 60 * 60 * 24 * 365.0f ) ) );
  else if( secs >= ( 60 * 60 * 24 * 90 ) )
    Com_sprintf( duration, dursize, "%1.1f weeks",
      ( secs / ( 60 * 60 * 24 * 7.0f ) ) );
  else if( secs >= ( 60 * 60 * 24 ) )
    Com_sprintf( duration, dursize, "%1.1f days",
      ( secs / ( 60 * 60 * 24.0f ) ) );
  else if( secs >= ( 60 * 60 ) )
    Com_sprintf( duration, dursize, "%1.1f hours",
      ( secs / ( 60 * 60.0f ) ) );
  else if( secs >= 60 )
    Com_sprintf( duration, dursize, "%1.1f minutes",
      ( secs / 60.0f ) );
  else
    Com_sprintf( duration, dursize, "%i seconds", secs );
}

static void G_admin_ban_message(
  gentity_t     *ent,
  g_admin_ban_t *ban,
  char          *creason,
  int           clen,
  char          *areason,
  int           alen )
{
  if( creason )
  {
    char duration[ MAX_DURATION_LENGTH ];
    G_admin_duration( ban->expires - Com_RealTime( NULL ), duration,
      sizeof( duration ) );
    // part of this might get cut off on the connect screen
    Com_sprintf( creason, clen,
      "You have been banned by %s" S_COLOR_WHITE " duration: %s"
      " reason: %s",
      ban->banner,
      duration,
      ban->reason );
  }

  if( areason && ent )
  {
    // we just want the ban number
    int n = 1;
    g_admin_ban_t *b = g_admin_bans;
    for( ; b && b != ban; b = b->next, n++ )
      ;
    Com_sprintf( areason, alen,
      S_COLOR_YELLOW "Banned player %s" S_COLOR_YELLOW
      " tried to connect from %s (ban #%d)",
      ent->client->pers.netname[ 0 ] ? ent->client->pers.netname : ban->name,
      ent->client->pers.ip.str,
      n );
  }
}

static qboolean G_admin_ban_matches( g_admin_ban_t *ban, gentity_t *ent )
{
  return !Q_stricmp( ban->guid, ent->client->pers.guid ) ||
         ( !G_admin_permission( ent, ADMF_IMMUNITY ) &&
           G_AddressCompare( &ban->ip, &ent->client->pers.ip ) );
}

static g_admin_ban_t *G_admin_match_ban( gentity_t *ent )
{
  int t;
  g_admin_ban_t *ban;

  t = Com_RealTime( NULL );
  if( ent->client->pers.localClient )
    return NULL;

  for( ban = g_admin_bans; ban; ban = ban->next )
  {
    // 0 is for perm ban
    if( ban->expires != 0 && ban->expires <= t )
      continue;

    if( G_admin_ban_matches( ban, ent ) )
      return ban;
  }

  return NULL;
}

qboolean G_admin_ban_check( gentity_t *ent, char *reason, int rlen )
{
  g_admin_ban_t *ban;
  char warningMessage[ MAX_STRING_CHARS ];

  if( ent->client->pers.localClient )
    return qfalse;

  if( ( ban = G_admin_match_ban( ent ) ) )
  {
    G_admin_ban_message( ent, ban, reason, rlen,
      warningMessage, sizeof( warningMessage ) );

    // don't spam admins
    if( ban->warnCount++ < 5 )
      G_AdminMessage( NULL, warningMessage );
    // and don't fill the console
    else if( ban->warnCount < 10 )
      Com_Printf( "%s%s\n", warningMessage,
        ban->warnCount + 1 == 10 ?
          S_COLOR_WHITE " - future messages for this ban will be suppressed" :
          "" );
    return qtrue;
  }

  return qfalse;
}

qboolean G_admin_cmd_check( gentity_t *ent )
{
  char command[ MAX_ADMIN_CMD_LEN ];
  g_admin_cmd_t *admincmd;
  g_admin_command_t *c;
  qboolean success;

  command[ 0 ] = '\0';
  Cmd_ArgvBuffer( 0, command, sizeof( command ) );
  if( !command[ 0 ] )
    return qfalse;

  Q_strlwr( command );
  admin_log_start( ent, command );

  if( ( c = G_admin_command( command ) ) )
  {
    admin_log( ConcatArgsPrintable( 1 ) );
    if( ( success = G_admin_permission( ent, c->flag ) ) )
    {
      if( G_FloodLimited( ent ) )
        return qtrue;
      Cbuf_ExecuteText( EXEC_APPEND, c->exec );
    }
    else
    {
      ADMP( va( "^3%s: ^7permission denied\n", c->command ) );
    }
    admin_log_end( success );
    return qtrue;
  }

  if( ( admincmd = G_admin_cmd( command ) ) )
  {
    if( ( success = G_admin_permission( ent, admincmd->flag ) ) )
    {
      if( G_FloodLimited( ent ) )
        return qtrue;
      if( admincmd->silent )
        admin_log_abort( );
      if( !( success = admincmd->handler( ent ) ) )
        admin_log( ConcatArgsPrintable( 1 ) );
    }
    else
    {
      ADMP( va( "^3%s: ^7permission denied\n", admincmd->keyword ) );
      admin_log( ConcatArgsPrintable( 1 ) );
    }
    admin_log_end( success );
    return qtrue;
  }
  return qfalse;
}

static void llsort( struct llist **head, int compar( const void *, const void * ) )
{
  struct llist *a, *b, *t, *l;
  int i, c = 1, ns, as, bs;

  if( !*head )
    return;

  do
  {
    a = *head, l = *head = NULL;
    for( ns = 0; a; ns++, a = b )
    {
      b = a;
      for( i = as = 0; i < c; i++ )
      {
        as++;
        if( !( b = b->next ) )
          break;
      }
      for( bs = c; ( b && bs ) || as; l = t )
      {
        if( as && ( !bs || !b || compar( a, b ) <= 0 ) )
          t = a, a = a->next, as--;
        else
          t = b, b = b->next, bs--;
        if( l )
          l->next = t;
        else
          *head = t;
      }
    }
    l->next = NULL;
    c *= 2;
  } while( ns > 1 );
}

static int cmplevel( const void *a, const void *b )
{
  return ((g_admin_level_t *)b)->level - ((g_admin_level_t *)a)->level;
}

qboolean G_admin_readconfig( gentity_t *ent )
{
  g_admin_level_t *l = NULL;
  g_admin_admin_t *a = NULL;
  g_admin_ban_t *b = NULL;
  g_admin_command_t *c = NULL;
  int lc = 0, ac = 0, bc = 0, cc = 0;
  fileHandle_t f;
  int len;
  char *cnf, *cnf2;
  char *t;
  qboolean level_open, admin_open, ban_open, command_open;
  int i;
  char ip[ 44 ];

  G_admin_cleanup();

  if( !g_admin.string[ 0 ] )
  {
    ADMP( "^3readconfig: g_admin is not set, not loading configuration "
      "from a file\n" );
    return qfalse;
  }

  len = FS_FOpenFileByMode( g_admin.string, &f, FS_READ );
  if( len < 0 )
  {
    Com_Printf( "^3readconfig: ^7could not open admin config file %s\n",
            g_admin.string );
    admin_default_levels();
    return qfalse;
  }
  cnf = BG_StackPoolAlloc( len + 1 );
  cnf2 = cnf;
  FS_Read2( cnf, len, f );
  cnf[ len ] = '\0';
  FS_FCloseFile( f );

  admin_level_maxname = 0;

  level_open = admin_open = ban_open = command_open = qfalse;
  COM_BeginParseSession( g_admin.string );
  while( 1 )
  {
    t = COM_Parse( &cnf );
    if( !*t )
      break;

    if( !Q_stricmp( t, "[level]" ) )
    {
      if( l )
        l = l->next = BG_Alloc0( sizeof( g_admin_level_t ) );
      else
        l = g_admin_levels = BG_Alloc0( sizeof( g_admin_level_t ) );
      level_open = qtrue;
      admin_open = ban_open = command_open = qfalse;
      lc++;
    }
    else if( !Q_stricmp( t, "[admin]" ) )
    {
      if( a )
        a = a->next = BG_Alloc0( sizeof( g_admin_admin_t ) );
      else
        a = g_admin_admins = BG_Alloc0( sizeof( g_admin_admin_t ) );
      admin_open = qtrue;
      level_open = ban_open = command_open = qfalse;
      ac++;
    }
    else if( !Q_stricmp( t, "[ban]" ) )
    {
      if( b )
        b = b->next = BG_Alloc0( sizeof( g_admin_ban_t ) );
      else
        b = g_admin_bans = BG_Alloc0( sizeof( g_admin_ban_t ) );
      ban_open = qtrue;
      level_open = admin_open = command_open = qfalse;
      bc++;
    }
    else if( !Q_stricmp( t, "[command]" ) )
    {
      if( c )
        c = c->next = BG_Alloc0( sizeof( g_admin_command_t ) );
      else
        c = g_admin_commands = BG_Alloc0( sizeof( g_admin_command_t ) );
      command_open = qtrue;
      level_open = admin_open = ban_open = qfalse;
      cc++;
    }
    else if( level_open )
    {
      if( !Q_stricmp( t, "level" ) )
      {
        admin_readconfig_int( &cnf, &l->level );
      }
      else if( !Q_stricmp( t, "name" ) )
      {
        admin_readconfig_string( &cnf, l->name, sizeof( l->name ) );
        // max printable name length for formatting
        len = Q_PrintStrlen( l->name );
        if( len > admin_level_maxname )
          admin_level_maxname = len;
      }
      else if( !Q_stricmp( t, "flags" ) )
      {
        admin_readconfig_string( &cnf, l->flags, sizeof( l->flags ) );
      }
      else
      {
        COM_ParseError( "[level] unrecognized token \"%s\"", t );
      }
    }
    else if( admin_open )
    {
      if( !Q_stricmp( t, "name" ) )
      {
        admin_readconfig_string( &cnf, a->name, sizeof( a->name ) );
      }
      else if( !Q_stricmp( t, "guid" ) )
      {
        admin_readconfig_string( &cnf, a->guid, sizeof( a->guid ) );
      }
      else if( !Q_stricmp( t, "level" ) )
      {
        admin_readconfig_int( &cnf, &a->level );
      }
      else if( !Q_stricmp( t, "flags" ) )
      {
        admin_readconfig_string( &cnf, a->flags, sizeof( a->flags ) );
      }
      else
      {
        COM_ParseError( "[admin] unrecognized token \"%s\"", t );
      }

    }
    else if( ban_open )
    {
      if( !Q_stricmp( t, "name" ) )
      {
        admin_readconfig_string( &cnf, b->name, sizeof( b->name ) );
      }
      else if( !Q_stricmp( t, "guid" ) )
      {
        admin_readconfig_string( &cnf, b->guid, sizeof( b->guid ) );
      }
      else if( !Q_stricmp( t, "ip" ) )
      {
        admin_readconfig_string( &cnf, ip, sizeof( ip ) );
        G_AddressParse( ip, &b->ip );
      }
      else if( !Q_stricmp( t, "reason" ) )
      {
        admin_readconfig_string( &cnf, b->reason, sizeof( b->reason ) );
      }
      else if( !Q_stricmp( t, "made" ) )
      {
        admin_readconfig_string( &cnf, b->made, sizeof( b->made ) );
      }
      else if( !Q_stricmp( t, "expires" ) )
      {
        admin_readconfig_int( &cnf, &b->expires );
      }
      else if( !Q_stricmp( t, "banner" ) )
      {
        admin_readconfig_string( &cnf, b->banner, sizeof( b->banner ) );
      }
      else
      {
        COM_ParseError( "[ban] unrecognized token \"%s\"", t );
      }
    }
    else if( command_open )
    {
      if( !Q_stricmp( t, "command" ) )
      {
        admin_readconfig_string( &cnf, c->command, sizeof( c->command ) );
      }
      else if( !Q_stricmp( t, "exec" ) )
      {
        admin_readconfig_string( &cnf, c->exec, sizeof( c->exec ) );
      }
      else if( !Q_stricmp( t, "desc" ) )
      {
        admin_readconfig_string( &cnf, c->desc, sizeof( c->desc ) );
      }
      else if( !Q_stricmp( t, "flag" ) )
      {
        admin_readconfig_string( &cnf, c->flag, sizeof( c->flag ) );
      }
      else
      {
        COM_ParseError( "[command] unrecognized token \"%s\"", t );
      }
    }
    else
    {
      COM_ParseError( "unexpected token \"%s\"", t );
    }
  }
  BG_StackPoolFree( cnf2 );
  ADMP( va( "^3readconfig: ^7loaded %d levels, %d admins, %d bans, %d commands\n",
          lc, ac, bc, cc ) );
  if( lc == 0 )
    admin_default_levels();
  else
  {
    llsort( (struct llist **)&g_admin_levels, cmplevel );
    llsort( (struct llist **)&g_admin_admins, cmplevel );
  }

  // restore admin mapping
  for( i = 0; i < level.maxclients; i++ )
  {
    if( level.clients[ i ].pers.connected != CON_DISCONNECTED )
    {
      level.clients[ i ].pers.admin =
        G_admin_admin( level.clients[ i ].pers.guid );
      if( level.clients[ i ].pers.admin )
        G_admin_authlog( &g_entities[ i ] );
      G_admin_cmdlist( &g_entities[ i ] );
    }
  }

  if(IS_SCRIM) {
    G_Scrim_Player_Refresh_Registered_Names( );
  }

  return qtrue;
}

qboolean G_admin_time( gentity_t *ent )
{
  qtime_t qt;

  Com_RealTime( &qt );
  ADMP( va( "^3time: ^7local time is %02i:%02i:%02i\n",
    qt.tm_hour, qt.tm_min, qt.tm_sec ) );
  return qtrue;
}

// this should be in one of the headers, but it is only used here for now
namelog_t *G_NamelogFromString( gentity_t *ent, char *s );

/*
for consistency, we should be able to target a disconnected player with setlevel
but we can't use namelog and remain consistent, so the solution would be to make
everyone a real level 0 admin so they can be targeted until the next level
but that seems kind of stupid
*/
qboolean G_admin_setlevel( gentity_t *ent )
{
  char name[ MAX_NAME_LENGTH ] = {""};
  char lstr[ 12 ]; // 11 is max strlen() for 32-bit (signed) int
  char testname[ MAX_NAME_LENGTH ] = {""};
  int i;
  gentity_t *vic = NULL;
  g_admin_admin_t *a = NULL;
  g_admin_level_t *l = NULL;
  int na;

  if( Cmd_Argc() < 3 )
  {
    ADMP( "^3setlevel: ^7usage: setlevel [name|slot#] [level]\n" );
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, testname, sizeof( testname ) );
  Cmd_ArgvBuffer( 2, lstr, sizeof( lstr ) );

  if( !( l = G_admin_level( atoi( lstr ) ) ) )
  {
    ADMP( "^3setlevel: ^7level is not defined\n" );
    return qfalse;
  }

  if( ent && l->level >
    ( ent->client->pers.admin ? ent->client->pers.admin->level : 0 ) )
  {
    ADMP( "^3setlevel: ^7you may not use setlevel to set a level higher "
      "than your current level\n" );
    return qfalse;
  }

  for( na = 0, a = g_admin_admins; a; na++, a = a->next );

  for( i = 0; testname[ i ] && isdigit( testname[ i ] ); i++ );
  if( !testname[ i ] )
  {
    int id = atoi( testname );
    if( id < MAX_CLIENTS )
    {
      vic = &g_entities[ id ];
      if( !vic || !vic->client || vic->client->pers.connected == CON_DISCONNECTED )
      {
        ADMP( va( "^3setlevel: ^7no player connected in slot %d\n", id ) );
        return qfalse;
      }
    }
    else if( id < na + MAX_CLIENTS )
      for( i = 0, a = g_admin_admins; i < id - MAX_CLIENTS; i++, a = a->next );
    else
    {
      ADMP( va( "^3setlevel: ^7%s not in range 1-%d\n",
                testname, na + MAX_CLIENTS - 1 ) );
      return qfalse;
    }
  }
  else
    G_SanitiseString( testname, name, sizeof( name ) );

  if( vic )
    a = vic->client->pers.admin;
  else if( !a )
  {
    g_admin_admin_t *wa;
    int             matches = 0;

    for( wa = g_admin_admins; wa && matches < 2; wa = wa->next )
    {
      G_SanitiseString( wa->name, testname, sizeof( testname ) );
      if( strstr( testname, name ) )
      {
        a = wa;
        matches++;
      }
    }

    for( i = 0; i < level.maxclients && matches < 2; i++ )
    {
      if( level.clients[ i ].pers.connected == CON_DISCONNECTED )
        continue;

      if( matches && level.clients[ i ].pers.admin &&
          level.clients[ i ].pers.admin == a )
      {
        vic = &g_entities[ i ];
        continue;
      }

      G_SanitiseString( level.clients[ i ].pers.netname, testname,
        sizeof( testname ) );
      if( strstr( testname, name ) )
      {
        vic = &g_entities[ i ];
        a = vic->client->pers.admin;
        matches++;
      }
    }

    if( matches == 0 )
    {
      ADMP( "^3setlevel:^7 no match.  use listplayers or listadmins to "
        "find an appropriate number to use instead of name.\n" );
      return qfalse;
    }
    if( matches > 1 )
    {
      ADMP( "^3setlevel:^7 more than one match.  Use the admin number "
        "instead:\n" );
      admin_listadmins( ent, 0, name );
      return qfalse;
    }
  }

  if( ent && !admin_higher_admin( ent->client->pers.admin, a ) )
  {
    ADMP( "^3setlevel: ^7sorry, but your intended victim has a higher"
        " admin level than you\n" );
    return qfalse;
  }

  if( vic && vic->client->pers.guidless )
  {
    ADMP( va( "^3setlevel: ^7%s^7 has no GUID\n", vic->client->pers.netname ) );
    return qfalse;
  }

  if( !a && vic )
  {
    for( a = g_admin_admins; a && a->next; a = a->next );
    if( a )
      a = a->next = BG_Alloc0( sizeof( g_admin_admin_t ) );
    else
      a = g_admin_admins = BG_Alloc0( sizeof( g_admin_admin_t ) );
    vic->client->pers.admin = a;
    Q_strncpyz( a->guid, vic->client->pers.guid, sizeof( a->guid ) );
  }

  a->level = l->level;
  if( vic )
    Q_strncpyz( a->name, vic->client->pers.netname, sizeof( a->name ) );

  if(IS_SCRIM) {
    if(a->level) {
      G_Scrim_Player_Set_Registered_Name(a->guid, a->name);
    } else {
      G_Scrim_Player_Set_Registered_Name(a->guid, NULL);
    }
  }

  admin_log( va( "%d (%s) \"%s" S_COLOR_WHITE "\"", a->level, a->guid,
    a->name ) );

  AP( va(
    "print \"^3setlevel: ^7%s^7 was given level %d admin rights by %s\n\"",
    a->name, a->level, ( ent ) ? ent->client->pers.netname : "console" ) );

  admin_writeconfig();
  if( vic )
  {
    G_admin_authlog( vic );
    G_admin_cmdlist( vic );
  }
  return qtrue;
}

qboolean G_admin_register( gentity_t *ent )
{
  char arg[ 16 ];
  char playerName [ MAX_NAME_LENGTH ];
  int  oldLevel;
  int  newLevel = 1;

  if( !ent )
  {
    ADMP( "^3admintest: ^7you are on the console.\n" );
    return qfalse;
  }

  if( ent->client->pers.guidless )
  {
    ADMP( "^3register: ^7You need a newer client to register. You can download the latest client from www.grangerhub.com\n" );
    return qfalse;
  }

  G_DecolorString( (char *)ent->client->pers.netname, playerName, sizeof( playerName ) );

  if( ent->client->pers.admin )
    oldLevel = ent->client->pers.admin->level;
  else
    oldLevel = 0;

  if( Cmd_Argc( ) > 1 )
  {
    Cmd_ArgvBuffer( 1, arg, sizeof( arg ) );
    newLevel = atoi( arg );

    if( newLevel < 0 || newLevel > 1 )
    {
      ADMP( "^3register: ^7level can only be 0 or 1\n" );
      return qfalse;
    }

    if( newLevel == 0 && oldLevel != 1 )
    {
      ADMP( "^3register: ^7you may only remove name protection when level 1. "
            "find an admin with setlevel\n");
      return qfalse;
    }
  }

  if( newLevel != 0 && G_IsNewbieName( playerName ) )
  {
    ADMP( va( "^3register: ^7You cannot register names similar to '%s^7' or 'UnnamedPlayer'\n",
              g_newbieNamePrefix.string ) );
    return qfalse;
  }

  if( oldLevel < 0 || oldLevel > 1 )
    newLevel = oldLevel;

  Cbuf_ExecuteText( EXEC_APPEND,
                           va( "setlevel %d %d;", (int)(ent - g_entities), newLevel ) );

  AP( va( "print \"^3register: ^7%s^3 is now a %s nickname. Commands unlocked. ^2Congratulations!\n\"",
          ( newLevel == 0 && ent->client->pers.admin ) ?
            ent->client->pers.admin->name : ent->client->pers.netname,
          newLevel == 0 ? "free" : "protected" ) );

  return qtrue;
}

static void admin_create_ban( gentity_t *ent,
  char *netname,
  char *guid,
  addr_t *ip,
  int seconds,
  char *reason )
{
  g_admin_ban_t *b = NULL;
  qtime_t       qt;
  int           t;
  int           i;
  char          *name;
  char          disconnect[ MAX_STRING_CHARS ];

  t = Com_RealTime( &qt );

  for( b = g_admin_bans; b; b = b->next )
  {
    if( !b->next )
      break;
  }

  if( b )
  {
    if( !b->next )
      b = b->next = BG_Alloc0( sizeof( g_admin_ban_t ) );
  }
  else
    b = g_admin_bans = BG_Alloc0( sizeof( g_admin_ban_t ) );

  Q_strncpyz( b->name, netname, sizeof( b->name ) );
  Q_strncpyz( b->guid, guid, sizeof( b->guid ) );
  memcpy( &b->ip, ip, sizeof( b->ip ) );

  Com_sprintf( b->made, sizeof( b->made ), "%04i-%02i-%02i %02i:%02i:%02i",
    qt.tm_year+1900, qt.tm_mon+1, qt.tm_mday,
    qt.tm_hour, qt.tm_min, qt.tm_sec );

  if( ent && ent->client->pers.admin )
    name = ent->client->pers.admin->name;
  else if( ent )
    name = ent->client->pers.netname;
  else
    name = "console";

  Q_strncpyz( b->banner, name, sizeof( b->banner ) );
  if( !seconds )
    b->expires = 0;
  else
    b->expires = t + seconds;
  if( !*reason )
    Q_strncpyz( b->reason, "banned by admin", sizeof( b->reason ) );
  else
    Q_strncpyz( b->reason, reason, sizeof( b->reason ) );

  G_admin_ban_message( NULL, b, disconnect, sizeof( disconnect ), NULL, 0 );

  for( i = 0; i < level.maxclients; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_DISCONNECTED )
      continue;
    if( G_admin_ban_matches( b, &g_entities[ i ] ) )
    {
      SV_GameSendServerCommand( i, va( "disconnect \"%s\"", disconnect ) );

      SV_GameDropClient( i, va( "has been kicked by %s^7. reason: %s",
        b->banner, b->reason ) );
    }
  }
}

int G_admin_parse_time( const char *time )
{
  int seconds = 0, num = 0;
  if( !*time )
    return -1;
  while( *time )
  {
    if( !isdigit( *time ) )
      return -1;
    while( isdigit( *time ) )
      num = num * 10 + *time++ - '0';

    if( !*time )
      break;
    switch( *time++ )
    {
      case 'w': num *= 7;
      case 'd': num *= 24;
      case 'h': num *= 60;
      case 'm': num *= 60;
      case 's': break;
      default:  return -1;
    }
    seconds += num;
    num = 0;
  }
  if( num )
    seconds += num;
  return seconds;
}

qboolean G_admin_setdevmode( gentity_t *ent )
{
  char str[ 5 ];

  if( Cmd_Argc() != 2 )
  {
    ADMP( "^3setdevmode: ^7usage: setdevmode [on|off]\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, str, sizeof( str ) );
  if( !Q_stricmp( str, "on" ) )
  {
    if( g_cheats.integer )
    {
      ADMP( "^3setdevmode: ^7developer mode is already on\n" );
      return qfalse;
    }

    if(IS_SCRIM) {
      ADMP( "^3setdevmode: ^7scrim mode must be off to turn on dev mode\n" );
      return qfalse;
    }

    Cvar_SetSafe( "sv_cheats", "1" );
    Cvar_Update( &g_cheats );
    AP( va( "print \"^3setdevmode: ^7%s ^7has switched developer mode on\n\"",
            ent ? ent->client->pers.netname : "console" ) );
    return qtrue;
  }
  else if( !Q_stricmp( str, "off" ) )
  {
    int i;
    gclient_t *cl;
    gentity_t *tent;

    if( !g_cheats.integer )
    {
      ADMP( "^3setdevmode: ^7developer mode is already off\n" );
      return qfalse;
    }

    // cycle through each client and change their ready flag
    for( i = 0; i < g_maxclients.integer; i++ )
    {
      cl = level.clients + i;
      tent = &g_entities[ cl->ps.clientNum ];

      if( cl->pers.connected != CON_CONNECTED )
        continue;

      if( cl->pers.teamSelection == TEAM_NONE )
        continue;

      //disable noclip
      if( cl->noclip )
      {
        tent->r.contents = cl->cliprcontents;

        cl->noclip = !cl->noclip;

        if( tent->r.linked )
          SV_LinkEntity( tent );

        SV_GameSendServerCommand( tent - g_entities, va( "print \"noclip OFF\n\"" ) );
      }

      //dissable god mode
      if( tent->flags & FL_GODMODE )
      {
        tent->flags ^= FL_GODMODE;
        SV_GameSendServerCommand( tent - g_entities, va( "print \"godmode OFF\n\"" ) );
      }
    }

    Cvar_SetSafe( "sv_cheats", "0" );
    Cvar_Update( &g_cheats );
    AP( va( "print \"^3setdevmode: ^7%s ^7has switched developer mode off\n\"",
            ent ? ent->client->pers.netname : "console" ) );
    return qtrue;
  }
  else
  {
    ADMP( "^3setdevmode: ^7usage: setdevmode [on|off]\n" );
    return qfalse;
  }
}

qboolean G_admin_kick( gentity_t *ent )
{
  int pid;
  char name[ MAX_NAME_LENGTH ], *reason, err[ MAX_STRING_CHARS ];
  int minargc;
  gentity_t *vic;

  minargc = 3;
  if( G_admin_permission( ent, ADMF_UNACCOUNTABLE ) )
    minargc = 2;

  if( Cmd_Argc() < minargc )
  {
    ADMP( "^3kick: ^7usage: kick [name] [reason]\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  reason = ConcatArgs( 2 );
  if( ( pid = G_ClientNumberFromString( name, err, sizeof( err ) ) ) == -1 )
  {
    ADMP( va( "^3kick: ^7%s", err ) );
    return qfalse;
  }
  vic = &g_entities[ pid ];
  if( !admin_higher( ent, vic ) )
  {
    ADMP( "^3kick: ^7sorry, but your intended victim has a higher admin"
        " level than you\n" );
    return qfalse;
  }
  if( vic->client->pers.localClient )
  {
    ADMP( "^3kick: ^7disconnecting the host would end the game\n" );
    return qfalse;
  }
  admin_log( va( "%d (%s) \"%s" S_COLOR_WHITE "\": \"%s" S_COLOR_WHITE "\"",
    pid,
    vic->client->pers.guid,
    vic->client->pers.netname,
    reason ) );
  if(IS_SCRIM) {
    G_Scrim_Remove_Player_From_Rosters(vic->client->pers.namelog, qtrue);
  }
  admin_create_ban( ent,
    vic->client->pers.netname,
    vic->client->pers.guidless ? "" : vic->client->pers.guid,
    &vic->client->pers.ip,
    MAX( 1, G_admin_parse_time( g_adminTempBan.string ) ),
    ( *reason ) ? reason : "kicked by admin" );
  admin_writeconfig();

  return qtrue;
}

qboolean G_admin_setivo( gentity_t* ent )
{
  char arg[ 3 ];
  const char *cn;
  gentity_t *spot;

  if( !ent )
  {
    ADMP( "^3setivo: ^7the console can't position intermission view overrides\n" );
    return qfalse;
  }

  if( Cmd_Argc() != 2 )
  {
    ADMP( "^3setivo: ^7usage: setivo {s|a|h}\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, arg, sizeof( arg ) );
  if( !Q_stricmp( arg, "s" ) )
    cn = "info_player_intermission";
  else if( !Q_stricmp( arg, "a" ) )
    cn = "info_alien_intermission";
  else if( !Q_stricmp( arg, "h" ) )
    cn = "info_human_intermission";
  else
  {
    ADMP( "^3setivo: ^7the argument must be either s, a or h\n" );
    return qfalse;
  }

  spot = G_Find( NULL, FOFS( classname ), cn );
  if( !spot )
  {
    spot = G_Spawn();
    spot->classname = (char *)cn;
  }
  spot->count = 1;

  BG_GetClientViewOrigin( &ent->client->ps, spot->r.currentOrigin );
  VectorCopy( ent->client->ps.viewangles, spot->r.currentAngles );

  ADMP( "^3setivo: ^7intermission view override positioned\n" );
  return qtrue;
}

qboolean G_admin_ban( gentity_t *ent )
{
  int seconds;
  char search[ MAX_NAME_LENGTH ];
  char secs[ MAX_TOKEN_CHARS ];
  char *reason;
  char duration[ MAX_DURATION_LENGTH ];
  int i;
  addr_t ip;
  qboolean ipmatch = qfalse;
  namelog_t *match = NULL;
  qboolean cidr = qfalse;

  if( Cmd_Argc() < 2 )
  {
    ADMP( "^3ban: ^7usage: ban [name|slot|IP(/mask)] [duration] [reason]\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, search, sizeof( search ) );
  Cmd_ArgvBuffer( 2, secs, sizeof( secs ) );

  seconds = G_admin_parse_time( secs );
  if( seconds <= 0 )
  {
    seconds = 0;
    reason = ConcatArgs( 2 );
  }
  else
  {
    reason = ConcatArgs( 3 );
  }
  if( !*reason && !G_admin_permission( ent, ADMF_UNACCOUNTABLE ) )
  {
    ADMP( "^3ban: ^7you must specify a reason\n" );
    return qfalse;
  }
  if( !G_admin_permission( ent, ADMF_CAN_PERM_BAN ) )
  {
    int maximum = MAX( 1, G_admin_parse_time( g_adminMaxBan.string ) );
    if( seconds == 0 || seconds > maximum )
    {
      ADMP( "^3ban: ^7you may not issue permanent bans\n" );
      seconds = maximum;
    }
  }

  if( G_AddressParse( search, &ip ) )
  {
    int max = ip.type == IPv4 ? 32 : 128;
    int min;

    cidr = G_admin_permission( ent, ADMF_CAN_IP_BAN );
    if( cidr )
      min = ent ? max / 2 : 1;
    else
      min = max;

    if( ip.mask < min || ip.mask > max )
    {
      ADMP( va( "^3ban: ^7invalid netmask (%d is not in the range %d-%d)\n",
        ip.mask, min, max ) );
      return qfalse;
    }
    ipmatch = qtrue;

    for( match = level.namelogs; match; match = match->next )
    {
      // skip players in the namelog who have already been banned
      if( match->banned )
        continue;

      for( i = 0; i < MAX_NAMELOG_ADDRS && match->ip[ i ].str[ 0 ]; i++ )
      {
        if( G_AddressCompare( &ip, &match->ip[ i ] ) )
          break;
      }
      if( i < MAX_NAMELOG_ADDRS && match->ip[ i ].str[ 0 ] )
        break;
    }

    if( !match )
    {
      if( cidr )
      {
        ADMP( "^3ban: ^7no player found by that IP address; banning anyway\n" );
      }
      else
      {
        ADMP( "^3ban: ^7no player found by that IP address\n" );
        return qfalse;
      }
    }
  }
  else if( !( match = G_NamelogFromString( ent, search ) ) || match->banned )
  {
    ADMP( "^3ban: ^7no match\n" );
    return qfalse;
  }

  if( ent && match && !admin_higher_guid( ent->client->pers.guid, match->guid ) )
  {

    ADMP( "^3ban: ^7sorry, but your intended victim has a higher admin"
      " level than you\n" );
    return qfalse;
  }
  if( match && match->slot > -1 && level.clients[ match->slot ].pers.localClient )
  {
    ADMP( "^3ban: ^7disconnecting the host would end the game\n" );
    return qfalse;
  }

  if(IS_SCRIM && match) {
    G_Scrim_Remove_Player_From_Rosters(match, qtrue);
  }

  G_admin_duration( ( seconds ) ? seconds : -1,
    duration, sizeof( duration ) );

  AP( va( "print \"^3ban:^7 %s^7 has been banned by %s^7 "
    "duration: %s, reason: %s\n\"",
    match ? match->name[ match->nameOffset ] : "an IP address",
    ( ent ) ? ent->client->pers.netname : "console",
    duration,
    ( *reason ) ? reason : "banned by admin" ) );

  admin_log( va( "%d (%s) \"%s" S_COLOR_WHITE "\": \"%s" S_COLOR_WHITE "\"",
    seconds,
    match ? match->guid : "",
    match ? match->name[ match->nameOffset ] : "IP ban",
    reason ) );
  if( ipmatch )
  {
    if( match )
    {
      admin_create_ban( ent,
        match->slot == -1 ?
          match->name[ match->nameOffset ] :
          level.clients[ match->slot ].pers.netname,
        match->guidless ? "" : match->guid,
        &ip,
        seconds, reason );
    }
    else
    {
      admin_create_ban( ent, "IP ban", "", &ip, seconds, reason );
    }
    admin_log( va( "[%s]", ip.str ) );
  }
  else
  {
    // ban all IP addresses used by this player
    for( i = 0; i < MAX_NAMELOG_ADDRS && match->ip[ i ].str[ 0 ]; i++ )
    {
      admin_create_ban( ent,
        match->slot == -1 ?
          match->name[ match->nameOffset ] :
          level.clients[ match->slot ].pers.netname,
        match->guidless ? "" : match->guid,
        &match->ip[ i ],
        seconds, reason );
      admin_log( va( "[%s]", match->ip[ i ].str ) );
    }
  }

  if( match )
    match->banned = qtrue;

  if( !g_admin.string[ 0 ] )
    ADMP( "^3ban: ^7WARNING g_admin not set, not saving ban to a file\n" );
  else
    admin_writeconfig();

  return qtrue;
}

qboolean G_admin_unban( gentity_t *ent )
{
  int bnum;
  int time = Com_RealTime( NULL );
  char bs[ 5 ];
  int i;
  g_admin_ban_t *ban;

  if( Cmd_Argc() < 2 )
  {
    ADMP( "^3unban: ^7usage: unban [ban#]\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, bs, sizeof( bs ) );
  bnum = atoi( bs );
  for( ban = g_admin_bans, i = 1; ban && i < bnum; ban = ban->next, i++ )
    ;
  if( i != bnum || !ban )
  {
    ADMP( "^3unban: ^7invalid ban#\n" );
    return qfalse;
  }
  if( ban->expires > 0 && ban->expires - time <= 0 )
  {
    ADMP( "^3unban: ^7ban already expired\n" );
    return qfalse;
  }
  if( !G_admin_permission( ent, ADMF_CAN_PERM_BAN ) &&
    ( ban->expires == 0 || ( ban->expires - time > MAX( 1,
      G_admin_parse_time( g_adminMaxBan.string ) ) ) ) )
  {
    ADMP( "^3unban: ^7you cannot remove permanent bans\n" );
    return qfalse;
  }
  admin_log( va( "%d (%s) \"%s" S_COLOR_WHITE "\": \"%s" S_COLOR_WHITE "\": [%s]",
    ban->expires ? ban->expires - time : 0, ban->guid, ban->name, ban->reason,
    ban->ip.str ) );
  AP( va( "print \"^3unban: ^7ban #%d for %s^7 has been removed by %s\n\"",
          bnum,
          ban->name,
          ( ent ) ? ent->client->pers.netname : "console" ) );
  ban->expires = time;
  admin_writeconfig();
  return qtrue;
}

qboolean G_admin_addlayout( gentity_t *ent )
{
  char layout[ MAX_QPATH ];

  if( Cmd_Argc( ) != 2 )
  {
    ADMP( "^3addlayout: ^7usage: addlayout <layoutelements>\n" );
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, layout, sizeof( layout ) );

  G_LayoutLoad( layout );

  AP( va( "print \"^3addlayout: ^7some layout elements have been placed by %s\n\"",
          ent ? ent->client->pers.netname : "console" ) );
  return qtrue;
}

qboolean G_admin_layoutsave( gentity_t *ent )
{
  char str[ MAX_QPATH ];
  char str2[ MAX_QPATH - 4 ];
  char *s;
  int i = 0;
  qboolean pipeEncountered = qfalse;

  if( Cmd_Argc( ) != 2 )
  {
    ADMP( "^3layoutsave: ^7usage: layoutsave <name>\n" );
    return qfalse;
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
    ADMP( "^3layoutsave: ^7invalid layout name\n" );
    return qfalse;
  }

  Cbuf_ExecuteText( EXEC_APPEND, va( "layoutsave %s", str2 ) );
  AP( va( "print \"^3layoutsave: ^7layout has been saved as '^2%s^7' by '%s'\n", str2,
          ent ? ent->client->pers.netname : "console" ) );
  return qtrue;
}

qboolean G_admin_adjustban( gentity_t *ent )
{
  int bnum;
  int length, maximum;
  int expires;
  int time = Com_RealTime( NULL );
  char duration[ MAX_DURATION_LENGTH ] = {""};
  char *reason;
  char bs[ 5 ];
  char secs[ MAX_TOKEN_CHARS ];
  char mode = '\0';
  g_admin_ban_t *ban;
  int mask = 0;
  int i;
  int skiparg = 0;

  if( Cmd_Argc() < 3 )
  {
    ADMP( "^3adjustban: ^7usage: adjustban [ban#] [/mask] [duration] [reason]"
      "\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, bs, sizeof( bs ) );
  bnum = atoi( bs );
  for( ban = g_admin_bans, i = 1; ban && i < bnum; ban = ban->next, i++ );
  if( i != bnum || !ban )
  {
    ADMP( "^3adjustban: ^7invalid ban#\n" );
    return qfalse;
  }
  maximum = MAX( 1, G_admin_parse_time( g_adminMaxBan.string ) );
  if( !G_admin_permission( ent, ADMF_CAN_PERM_BAN ) &&
    ( ban->expires == 0 || ban->expires - time > maximum ) )
  {
    ADMP( "^3adjustban: ^7you cannot modify permanent bans\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 2, secs, sizeof( secs ) );
  if( secs[ 0 ] == '/' )
  {
    int max = ban->ip.type == IPv6 ? 128 : 32;
    int min;

    if( G_admin_permission( ent, ADMF_CAN_IP_BAN ) )
      min = ent ? max / 2 : 1;
    else
      min = max;

    mask = atoi( secs + 1 );
    if( mask < min || mask > max )
    {
      ADMP( va( "^3adjustban: ^7invalid netmask (%d is not in range %d-%d)\n",
        mask, min, max ) );
      return qfalse;
    }
    Cmd_ArgvBuffer( 3 + skiparg++, secs, sizeof( secs ) );
  }
  if( secs[ 0 ] == '+' || secs[ 0 ] == '-' )
    mode = secs[ 0 ];
  length = G_admin_parse_time( &secs[ mode ? 1 : 0 ] );
  if( length < 0 )
    skiparg--;
  else
  {
    if( length )
    {
      if( ban->expires == 0 && mode )
      {
        ADMP( "^3adjustban: ^7new duration must be explicit\n" );
        return qfalse;
      }
      if( mode == '+' )
        expires = ban->expires + length;
      else if( mode == '-' )
        expires = ban->expires - length;
      else
        expires = time + length;
      if( expires <= time )
      {
        ADMP( "^3adjustban: ^7ban duration must be positive\n" );
        return qfalse;
      }
    }
    else
      length = expires = 0;
    if( !G_admin_permission( ent, ADMF_CAN_PERM_BAN ) &&
      ( length == 0 || length > maximum ) )
    {
      ADMP( "^3adjustban: ^7you may not issue permanent bans\n" );
      expires = time + maximum;
    }

    ban->expires = expires;
    G_admin_duration( ( expires ) ? expires - time : -1, duration,
      sizeof( duration ) );
  }
  if( mask )
  {
    char *p = strchr( ban->ip.str, '/' );
    if( !p )
      p = ban->ip.str + strlen( ban->ip.str );
    if( mask == ( ban->ip.type == IPv6 ? 128 : 32 ) )
      *p = '\0';
    else
      Com_sprintf( p, sizeof( ban->ip.str ) - ( p - ban->ip.str ), "/%d", mask );
    ban->ip.mask = mask;
  }
  reason = ConcatArgs( 3 + skiparg );
  if( *reason )
    Q_strncpyz( ban->reason, reason, sizeof( ban->reason ) );
  admin_log( va( "%d (%s) \"%s" S_COLOR_WHITE "\": \"%s" S_COLOR_WHITE "\": [%s]",
    ban->expires ? ban->expires - time : 0, ban->guid, ban->name, ban->reason,
    ban->ip.str ) );
  AP( va( "print \"^3adjustban: ^7ban #%d for %s^7 has been updated by %s^7 "
    "%s%s%s%s%s%s\n\"",
    bnum,
    ban->name,
    ( ent ) ? ent->client->pers.netname : "console",
    ( mask ) ?
      va( "netmask: /%d%s", mask,
        ( length >= 0 || *reason ) ? ", " : "" ) : "",
    ( length >= 0 ) ? "duration: " : "",
    duration,
    ( length >= 0 && *reason ) ? ", " : "",
    ( *reason ) ? "reason: " : "",
    reason ) );
  if( ent )
    Q_strncpyz( ban->banner, ent->client->pers.netname, sizeof( ban->banner ) );
  admin_writeconfig();
  return qtrue;
}

qboolean G_admin_putteam(gentity_t *ent) {
  char      name[MAX_NAME_LENGTH], team[sizeof("spectators")],
            err[MAX_STRING_CHARS];
  namelog_t *vic;
  team_t    old_team;
  team_t    teamnum = TEAM_NONE;
  char      secs[MAX_TOKEN_CHARS];
  int       seconds = 0;
  qboolean  useDuration = qfalse;

  Cmd_ArgvBuffer(1, name, sizeof(name));
  Cmd_ArgvBuffer(2, team, sizeof(team));

  if(Cmd_Argc() < 3) {
    ADMP("^3putteam: ^7usage: putteam [^3name^6|^3slot#^6|^3namelog#] [h|a|s]\n");
    return qfalse;
  }

  if(!(vic = G_NamelogFromString(ent, name))) {
    ADMP(va("^3putteam: ^7no player match\n"));
    return qfalse;
  }

  if(vic->slot >= 0 && vic->slot < MAX_CLIENTS) {
    old_team = level.clients[vic->slot].pers.teamSelection;
  } else {
    old_team = vic->team;
  }

  if( ent && !admin_higher_admin( ent->client->pers.admin,
      G_admin_admin( vic->guid ) ) )
  {
    ADMP( "^3putteam: ^7sorry, but your intended victim has a higher "
        " admin level than you\n" );
    return qfalse;
  }
  teamnum = G_TeamFromString( team );
  if( teamnum == NUM_TEAMS )
  {
    ADMP( va( "^3putteam: ^7unknown team %s\n", team ) );
    return qfalse;
  }
  //duration code
  if(Cmd_Argc() > 3) {
    //can only lock players in spectator
    if(teamnum != TEAM_NONE) {
      ADMP( "^3putteam: ^7You can only lock a player into the spectators team\n" );
      return qfalse;
    }
    Cmd_ArgvBuffer( 3, secs, sizeof( secs ) );
    seconds = G_admin_parse_time( secs );
    useDuration = qtrue;
  }
  if(
    old_team == teamnum &&
    !(useDuration && teamnum == TEAM_NONE))
  {
    ADMP( 
      va(
        "^3putteam: ^7%s ^7is already on the %s team\n",
        vic->name[vic->nameOffset], BG_Team(teamnum)->name2));
    return qfalse;
  }

  if(IS_SCRIM) {
    if(teamnum == TEAM_NONE) {
      G_Scrim_Remove_Player_From_Rosters(vic, qfalse);
    } else {
      if(
        !G_Scrim_Add_Namelog_To_Roster(
          vic,
          BG_Scrim_Team_From_Playing_Team(&level.scrim, teamnum), err, sizeof(err))) {
        ADMP(va("^3scrim: ^7%s", err));
        return qfalse;
      }
    }
  }

  admin_log(
    va(
      "%d (%s) \"%s" S_COLOR_WHITE "\"",
      (vic->slot < 0) ? vic->id : vic->slot,
      vic->guid,
      vic->name[vic->nameOffset]));

  if(vic->slot < 0 || vic->slot >= MAX_CLIENTS) {
    vic->team = teamnum;
  } else {
    if(teamnum != old_team) {
      G_ChangeTeam( &g_entities[vic->slot], teamnum );
    }
  }

  if(useDuration && seconds > 0) {
    vic->specExpires = level.time + ( seconds * 1000 );
  } else {
    vic->specExpires = 0;
  }

  if(useDuration && seconds > 0) {
    AP( va( "print \"^3putteam: ^7%s^7 forced %s^7 on to the %s team for %d seconds\n\"",
            ( ent ) ? ent->client->pers.netname : "console",
            vic->name[vic->nameOffset], BG_Team( teamnum )->name2, seconds ) );
  } else {
    AP( va( "print \"^3putteam: ^7%s^7 put %s^7 on to the %s team\n\"",
            ( ent ) ? ent->client->pers.netname : "console",
            vic->name[vic->nameOffset], BG_Team( teamnum )->name2 ) );
  }
  return qtrue;
}

qboolean G_admin_changemap( gentity_t *ent )
{
  char map[ MAX_QPATH ];
  char layout[ MAX_QPATH ] = { "" };

  if( Cmd_Argc( ) < 2 )
  {
    ADMP( "^3changemap: ^7usage: changemap [map] (layout)\n" );
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, map, sizeof( map ) );

  if( !G_MapExists( map ) )
  {
    ADMP( va( "^3changemap: ^7invalid map name '%s'\n", map ) );
    return qfalse;
  }

  if( Cmd_Argc( ) > 2 )
  {
    Cmd_ArgvBuffer( 2, layout, sizeof( layout ) );
    if( G_LayoutExists( map, layout ) )
    {
      Cvar_SetSafe( "g_nextLayout", layout );
    }
    else
    {
      ADMP( va( "^3changemap: ^7invalid layout name '%s'\n", layout ) );
      return qfalse;
    }
  }
  admin_log( map );
  admin_log( layout );

  G_MapConfigs( map );
  Cbuf_ExecuteText( EXEC_APPEND, va( "%smap \"%s\"\n",
                             ( g_cheats.integer ? "dev" : "" ), map ) );
  level.restarted = qtrue;
  AP( va( "print \"^3changemap: ^7map '%s' started by %s^7 %s\n\"", map,
          ( ent ) ? ent->client->pers.netname : "console",
          ( layout[ 0 ] ) ? va( "(forcing layout '%s')", layout ) : "" ) );
  Cvar_SetSafe( "g_warmup", "1" );
  SV_SetConfigstring( CS_WARMUP, va( "%d", IS_WARMUP ) );

  return qtrue;
}

qboolean G_admin_cp( gentity_t *ent )
{
  char     message[ MAX_STRING_CHARS ];
  char     arg[ 8 ];
  int      team = -1;
  int      i;
  qboolean admin;

  if( Cmd_Argc( ) < 2 )
  {
    ADMP( "^3cpa: ^7usage: cpa (-AHS) [message]\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, arg, sizeof( arg ) );
  if( arg[ 0 ] == '-' )
  {
    switch( arg[ 1 ] )
    {
      case 'a': case 'A':
        team = TEAM_ALIENS;
        break;
      case 'h': case 'H':
        team = TEAM_HUMANS;
        break;
      case 's': case 'S':
        team = TEAM_NONE;
        break;
      default:
        ADMP( "^3cpa: ^7team not recognized as -a -h or -s\n" );
        return qfalse;
    }
    if( Cmd_Argc( ) < 2 )
    {
      ADMP( "^3cpa: ^7no message\n" );
      return qfalse;
    }
    G_DecolorString( ConcatArgs( 2 ), message, sizeof( message ) );
  }
  else
    G_DecolorString( ConcatArgs( 1 ), message, sizeof( message ) );

  for( i = 0; i < level.maxclients; i++ )
  {
    if( level.clients[ i ].pers.connected != CON_CONNECTED )
      continue;

    admin = qfalse;
    if( team < 0 || level.clients[ i ].pers.teamSelection == team ||
        ( admin = G_admin_permission( &g_entities[ i ], ADMF_ADMINCHAT ) ) )
    {
      if( team < 0 || level.clients[ i ].pers.teamSelection == team )
        SV_GameSendServerCommand( i,
                                va( "cp \"^%s%s\" %d",
                                ( team < 0 ) ? "2" : "5", message,
                                CP_ADMIN_CP ) );

      SV_GameSendServerCommand( i,
                              va( "print \"%s^3cpa: ^7%s%s^7%s%s: %c%s\n\"",
                                  ( admin ) ? "[ADMIN] " : "",
                                  ( team >= 0 ) ? "(" : "",
                                  ( ent ) ? ent->client->pers.admin->name : "console",
                                  ( team >= 0 ) ? ")" : "",
                                  ( admin ) ? va( " to %s", ( team >= 0 ) ? va( "%ss", BG_Team( team )->name2 ) : "everyone" ) : "",
                                  INDENT_MARKER,
                                  message ) );
    }
  }

  G_LogPrintf( "print \"[ADMIN]^3cpa: ^7%s%s^7%s%s: %c%s\n\"",
      ( team >= 0 ) ? "(" : "",
      ( ent ) ? ent->client->pers.admin->name : "console",
      ( team >= 0 ) ? ")" : "",
      va( " to %s", ( team >= 0 ) ? va( "%ss", BG_Team( team )->name2 ) : "everyone" ),
      INDENT_MARKER,
      message );

  return qtrue;
}

qboolean G_admin_warn( gentity_t *ent )
{
  char      reason[ 64 ];
  int       pid;
  char      name[ MAX_NAME_LENGTH ], err[ MAX_STRING_CHARS ];
  gentity_t *vic;

  if( Cmd_Argc() < 3 )
  {
    ADMP( va( "^3warn: ^7usage: warn [name|slot#] [reason]\n" ) );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  if( ( pid = G_ClientNumberFromString( name, err, sizeof( err ) ) ) == -1 )
  {
    ADMP( va( "^3warn: ^7%s\n", err ) );
    return qfalse;
  }
  if( !admin_higher( ent, &g_entities[ pid ] ) )
  {
    ADMP( "^3warn: ^7sorry, but your intended victim has a higher admin level than you\n" );
    return qfalse;
  }
  vic = &g_entities[ pid ];

  G_DecolorString( ConcatArgs( 2 ), reason, sizeof( reason ) );
  CPx( pid, va( "cp \"^1You have been warned by an administrator:\n^7%s\" %d",
                reason, CP_ADMIN ) );
  AP( va( "print \"^3warn: ^7%s^7 has been warned: '%s' by %s\n\"",
          vic->client->pers.netname,
          reason,
          ( ent ) ? ent->client->pers.netname : "console" ) );

  return qtrue;
}

qboolean G_admin_mute( gentity_t *ent )
{
  char name[ MAX_NAME_LENGTH ];
  char command[ MAX_ADMIN_CMD_LEN ];
  namelog_t *vic;

  Cmd_ArgvBuffer( 0, command, sizeof( command ) );
  if( Cmd_Argc() < 2 )
  {
    ADMP( va( "^3%s: ^7usage: %s [name|slot#]\n", command, command ) );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  if( !( vic = G_NamelogFromString( ent, name ) ) )
  {
    ADMP( va( "^3%s: ^7no match\n", command ) );
    return qfalse;
  }
  if( ent && !admin_higher_admin( ent->client->pers.admin,
      G_admin_admin( vic->guid ) ) )
  {
    ADMP( va( "^3%s: ^7sorry, but your intended victim has a higher admin"
        " level than you\n", command ) );
    return qfalse;
  }
  if( vic->muted )
  {
    if( !Q_stricmp( command, "mute" ) )
    {
      ADMP( "^3mute: ^7player is already muted\n" );
      return qfalse;
    }
    vic->muted = qfalse;
    if( vic->slot > -1 )
      CPx( vic->slot, va( "cp \"^1You have been unmuted\" %d", CP_ADMIN ) );
    AP( va( "print \"^3unmute: ^7%s^7 has been unmuted by %s\n\"",
            vic->name[ vic->nameOffset ],
            ( ent ) ? ent->client->pers.netname : "console" ) );
  }
  else
  {
    if( !Q_stricmp( command, "unmute" ) )
    {
      ADMP( "^3unmute: ^7player is not currently muted\n" );
      return qfalse;
    }
    vic->muted = qtrue;
    if( vic->slot > -1 )
      CPx( vic->slot, va( "cp \"^1You've been muted\" %d", CP_ADMIN ) );
    AP( va( "print \"^3mute: ^7%s^7 has been muted by ^7%s\n\"",
            vic->name[ vic->nameOffset ],
            ( ent ) ? ent->client->pers.netname : "console" ) );
  }
  admin_log( va( "%d (%s) \"%s" S_COLOR_WHITE "\"", vic->slot, vic->guid,
    vic->name[ vic->nameOffset ] ) );
  return qtrue;
}

qboolean G_admin_denybuild( gentity_t *ent )
{
  char name[ MAX_NAME_LENGTH ];
  char command[ MAX_ADMIN_CMD_LEN ];
  namelog_t *vic;

  Cmd_ArgvBuffer( 0, command, sizeof( command ) );
  if( Cmd_Argc() < 2 )
  {
    ADMP( va( "^3%s: ^7usage: %s [name|slot#]\n", command, command ) );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  if( !( vic = G_NamelogFromString( ent, name ) ) )
  {
    ADMP( va( "^3%s: ^7no match\n", command ) );
    return qfalse;
  }
  if( ent && !admin_higher_admin( ent->client->pers.admin,
      G_admin_admin( vic->guid ) ) )
  {
    ADMP( va( "^3%s: ^7sorry, but your intended victim has a higher admin"
              " level than you\n", command ) );
    return qfalse;
  }
  if( vic->denyBuild )
  {
    if( !Q_stricmp( command, "denybuild" ) )
    {
      ADMP( "^3denybuild: ^7player already has no building rights\n" );
      return qfalse;
    }
    vic->denyBuild = qfalse;
    if( vic->slot > -1 )
      CPx( vic->slot,
           va( "cp \"^1You've regained your building rights\" %d",
               CP_ADMIN ) );
    AP( va(
      "print \"^3allowbuild: ^7building rights for ^7%s^7 restored by %s\n\"",
      vic->name[ vic->nameOffset ],
      ( ent ) ? ent->client->pers.netname : "console" ) );
  }
  else
  {
    if( !Q_stricmp( command, "allowbuild" ) )
    {
      ADMP( "^3allowbuild: ^7player already has building rights\n" );
      return qfalse;
    }
    vic->denyBuild = qtrue;
    if( vic->slot > -1 )
    {
      level.clients[ vic->slot ].ps.stats[ STAT_BUILDABLE ] = BA_NONE;
      CPx( vic->slot,
           va( "cp \"^1You've lost your building rights\" %d",
               CP_ADMIN ) );
    }
    AP( va(
      "print \"^3denybuild: ^7building rights for ^7%s^7 revoked by ^7%s\n\"",
      vic->name[ vic->nameOffset ],
      ( ent ) ? ent->client->pers.netname : "console" ) );
  }
  admin_log( va( "%d (%s) \"%s" S_COLOR_WHITE "\"", vic->slot, vic->guid,
    vic->name[ vic->nameOffset ] ) );
  return qtrue;
}

qboolean G_admin_explode( gentity_t *ent )
{

  int pid;
  char name[ MAX_NAME_LENGTH ], *reason, err[ MAX_STRING_CHARS ];
  gentity_t *vic;

  if( Cmd_Argc() < 2 )
  {
    ADMP( "^3explode: ^7usage: explode [name|slot#]\n" );
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  reason = ConcatArgs( 2 );
  if( ( pid = G_ClientNumberFromString( name, err, sizeof( err ) ) ) == -1 )
  {
    ADMP( va( "^3explode: ^7%s\n", err ) );
    return qfalse;
  }

  vic = &g_entities[ pid ];
  if( !admin_higher( ent, vic ) )
  {
    ADMP( "^3explode: ^7sorry, but your intended victim has a higher admin"
        " level than you\n" );
    return qfalse;
  }

  if( vic->client->pers.teamSelection == TEAM_NONE
   || vic->client->pers.classSelection == PCL_NONE
   || vic->health <= 0 )
  {
    ADMP( "^3explode: ^7they must be living to use this command\n" );
    return qfalse;
  }

  if( vic->flags & FL_GODMODE )
    vic->flags ^= FL_GODMODE;

  Blow_up(vic);

  SV_GameSendServerCommand( vic-g_entities,
			                    va( "cp \"^1Boom!!!\n^7%s\n\" %d",
                              reason, CP_ADMIN ) );

  AP( va( "print \"^3explode: ^7%s^7 has been exploded by %s^7 with the reason: ^7%s\n\"",
          vic->client->pers.netname,
          ( ent ) ? ent->client->pers.netname : "console",
          ( *reason ) ? reason : "No reason specified" ) );

  return qtrue;
}

qboolean G_admin_listadmins( gentity_t *ent )
{
  int i;
  char search[ MAX_NAME_LENGTH ] = {""};
  char s[ MAX_NAME_LENGTH ] = {""};
  int start = MAX_CLIENTS;

  if( Cmd_Argc() == 3 )
  {
    Cmd_ArgvBuffer( 2, s, sizeof( s ) );
    start = atoi( s );
  }
  if( Cmd_Argc() > 1 )
  {
    Cmd_ArgvBuffer( 1, s, sizeof( s ) );
    i = 0;
    if( Cmd_Argc() == 2 )
    {
      i = s[ 0 ] == '-';
      for( ; isdigit( s[ i ] ); i++ );
    }
    if( i && !s[ i ] )
      start = atoi( s );
    else
      G_SanitiseString( s, search, sizeof( search ) );
  }

  admin_listadmins( ent, start, search );
  return qtrue;
}

qboolean G_admin_listlayouts( gentity_t *ent )
{
  char list[ MAX_CVAR_VALUE_STRING ];
  char map[ MAX_QPATH ];
  int count = 0;
  char *s;
  char layout[ MAX_QPATH ] = { "" };
  int i = 0;

  if( Cmd_Argc( ) == 2 )
    Cmd_ArgvBuffer( 1, map, sizeof( map ) );
  else
    Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );

  count = G_LayoutList( map, list, sizeof( list ) );
  ADMBP_begin( );
  ADMBP( va( "^3listlayouts:^7 %d layouts found for '%s':\n", count, map ) );
  s = &list[ 0 ];
  while( *s )
  {
    if( *s == ' ' )
    {
      ADMBP( va ( " %s\n", layout ) );
      layout[ 0 ] = '\0';
      i = 0;
    }
    else if( i < sizeof( layout ) - 2 )
    {
      layout[ i++ ] = *s;
      layout[ i ] = '\0';
    }
    s++;
  }
  if( layout[ 0 ] )
    ADMBP( va ( " %s\n", layout ) );
  ADMBP_end( );
  return qtrue;
}

qboolean G_admin_listplayers( gentity_t *ent )
{
  int             i, j;
  gclient_t       *p;
  char            c, t; // color and team letter
  char            *registeredname;
  char            lname[ MAX_NAME_LENGTH ];
  char            muted, denied;
  int             colorlen;
  char            namecleaned[ MAX_NAME_LENGTH ];
  char            name2cleaned[ MAX_NAME_LENGTH ];
  g_admin_level_t *l;
  g_admin_level_t *d = G_admin_level( 0 );
  qboolean        hint;
  qboolean        canset = G_admin_permission( ent, "setlevel" );

  ADMBP_begin();
  ADMBP( va( "^3listplayers: ^7%d players connected:\n",
    level.numConnectedClients ) );
  for( i = 0; i < level.maxclients; i++ )
  {
    p = &level.clients[ i ];
    if( p->pers.connected == CON_DISCONNECTED )
      continue;
    if( p->pers.connected == CON_CONNECTING )
    {
      t = 'C';
      c = COLOR_YELLOW;
    }
    else
    {
      t = toupper( *( BG_Team( p->pers.teamSelection )->name2 ) );
      if( p->pers.teamSelection == TEAM_HUMANS )
        c = COLOR_CYAN;
      else if( p->pers.teamSelection == TEAM_ALIENS )
        c = COLOR_RED;
      else
        c = COLOR_WHITE;
    }

    muted = p->pers.namelog->muted ? 'M' : ' ';
    denied = p->pers.namelog->denyBuild ? 'B' : ' ';

    l = d;
    registeredname = NULL;
    hint = canset;
    if( p->pers.admin )
    {
      if( hint )
        hint = admin_higher( ent, &g_entities[ i ] );
      if( hint || !G_admin_permission( &g_entities[ i ], ADMF_INCOGNITO ) )
      {
        l = G_admin_level( p->pers.admin->level );
        G_SanitiseString( p->pers.netname, namecleaned,
                          sizeof( namecleaned ) );
        G_SanitiseString( p->pers.admin->name,
                          name2cleaned, sizeof( name2cleaned ) );
        if( Q_stricmp( namecleaned, name2cleaned ) )
          registeredname = p->pers.admin->name;
      }
    }

    if( l )
      Q_strncpyz( lname, l->name, sizeof( lname ) );

    for( colorlen = j = 0; lname[ j ]; j++ )
    {
      if( Q_IsColorString( &lname[ j ] ) )
        colorlen += 2;
    }

    ADMBP( va( "%2i ^%c%c %s %s^7 %*s^7 ^1%c%c^7 %s^7 %s%s%s\n",
              i,
              c,
              t,
              p->pers.guidless ? "^1---" : va( "^7%-2i^2%c", l ? l->level : 0, hint ? '*' : ' ' ),
              p->pers.alternateProtocol == 2 ? "^11" : p->pers.alternateProtocol == 1 ? "^3G" : " ",
              admin_level_maxname + colorlen,
              lname,
              muted,
              denied,
              p->pers.netname,
              ( registeredname ) ? "(a.k.a. " : "",
              ( registeredname ) ? registeredname : "",
              ( registeredname ) ? S_COLOR_WHITE ")" : "" ) );

  }
  ADMBP_end();
  return qtrue;
}

static qboolean ban_matchip( void *ban, const void *ip )
{
  return G_AddressCompare( &((g_admin_ban_t *)ban)->ip, (addr_t *)ip ) ||
    G_AddressCompare( (addr_t *)ip, &((g_admin_ban_t *)ban)->ip );
}
static qboolean ban_matchname( void *ban, const void *name )
{
  char match[ MAX_NAME_LENGTH ];

  G_SanitiseString( ( (g_admin_ban_t *)ban )->name, match, sizeof( match ) );
  return strstr( match, (const char *)name ) != NULL;
}
static void ban_out( void *ban, char *str )
{
  int i;
  int colorlen1 = 0;
  char duration[ MAX_DURATION_LENGTH ];
  char *d_color = S_COLOR_WHITE;
  char date[ 11 ];
  g_admin_ban_t *b = ( g_admin_ban_t * )ban;
  int t = Com_RealTime( NULL );
  char *made = b->made;

  for( i = 0; b->name[ i ]; i++ )
  {
    if( Q_IsColorString( &b->name[ i ] ) )
      colorlen1 += 2;
  }

  // only print out the the date part of made
  date[ 0 ] = '\0';
  for( i = 0; *made && *made != ' ' && i < sizeof( date ) - 1; i++ )
    date[ i ] = *made++;
  date[ i ] = 0;

  if( !b->expires || b->expires - t > 0 )
    G_admin_duration( b->expires ? b->expires - t : - 1,
                      duration, sizeof( duration ) );
  else
  {
    Q_strncpyz( duration, "expired", sizeof( duration ) );
    d_color = S_COLOR_CYAN;
  }

  Com_sprintf( str, MAX_STRING_CHARS, "%-*s %s%-15s " S_COLOR_WHITE "%-8s %s"
    "\n     \\__ %s%-*s " S_COLOR_WHITE "%s",
    MAX_NAME_LENGTH + colorlen1 - 1, b->name,
    ( strchr( b->ip.str, '/' ) ) ? S_COLOR_RED : S_COLOR_WHITE,
    b->ip.str,
    date,
    b->banner,
    d_color,
    MAX_DURATION_LENGTH - 1,
    duration,
    b->reason );
}
qboolean G_admin_showbans( gentity_t *ent )
{
  int i;
  int start = 1;
  char filter[ MAX_NAME_LENGTH ] = {""};
  char name_match[ MAX_NAME_LENGTH ] = {""};
  qboolean ipmatch = qfalse;
  addr_t ip;

  if( Cmd_Argc() == 3 )
  {
    Cmd_ArgvBuffer( 2, filter, sizeof( filter ) );
    start = atoi( filter );
  }
  if( Cmd_Argc() > 1 )
  {
    Cmd_ArgvBuffer( 1, filter, sizeof( filter ) );
    i = 0;
    if( Cmd_Argc() == 2 )
      for( i = filter[ 0 ] == '-'; isdigit( filter[ i ] ); i++ );
    if( !filter[ i ] )
      start = atoi( filter );
    else if( !( ipmatch = G_AddressParse( filter, &ip ) ) )
      G_SanitiseString( filter, name_match, sizeof( name_match ) );
  }

  admin_search( ent, "showbans", "bans",
    ipmatch ? ban_matchip : ban_matchname,
    ban_out, g_admin_bans,
    ipmatch ? (void * )&ip : (void *)name_match,
    start, 1, MAX_ADMIN_SHOWBANS );
  return qtrue;
}

qboolean G_admin_adminhelp( gentity_t *ent )
{
  g_admin_command_t *c;
  if( Cmd_Argc() < 2 )
  {
    int i;
    int count = 0;

    ADMBP_begin();
    for( i = 0; i < adminNumCmds; i++ )
    {
      if( G_admin_permission( ent, g_admin_cmds[ i ].flag ) )
      {
        ADMBP( va( "^3%-12s", g_admin_cmds[ i ].keyword ) );
        count++;
        // show 6 commands per line
        if( count % 6 == 0 )
          ADMBP( "\n" );
      }
    }
    for( c = g_admin_commands; c; c = c->next )
    {
      if( !G_admin_permission( ent, c->flag ) )
        continue;
      ADMBP( va( "^3%-12s", c->command ) );
      count++;
      // show 6 commands per line
      if( count % 6 == 0 )
        ADMBP( "\n" );
    }
    if( count % 6 )
      ADMBP( "\n" );
    ADMBP( va( "^3adminhelp: ^7%i available commands\n", count ) );
    ADMBP( "run adminhelp [^3command^7] for adminhelp with a specific command.\n" );
    ADMBP_end();

    return qtrue;
  }
  else
  {
    // adminhelp param
    char param[ MAX_ADMIN_CMD_LEN ];
    g_admin_cmd_t *admincmd;
    qboolean denied = qfalse;

    Cmd_ArgvBuffer( 1, param, sizeof( param ) );
    ADMBP_begin();
    if( ( c = G_admin_command( param ) ) )
    {
      if( G_admin_permission( ent, c->flag ) )
      {
        ADMBP( va( "^3adminhelp: ^7help for '%s':\n", c->command ) );
        ADMBP( va( " ^3Description: ^7%s\n", c->desc ) );
        ADMBP( va( " ^3Syntax: ^7%s\n", c->command ) );
        ADMBP( va( " ^3Flag: ^7'%s'\n", c->flag ) );
        ADMBP_end( );
        return qtrue;
      }
      denied = qtrue;
    }
    if( ( admincmd = G_admin_cmd( param ) ) )
    {
      if( G_admin_permission( ent, admincmd->flag ) )
      {
        ADMBP( va( "^3adminhelp: ^7help for '%s':\n", admincmd->keyword ) );
        ADMBP( va( " ^3Description: ^7%s\n", admincmd->function ) );
        ADMBP( va( " ^3Syntax: ^7%s %s\n", admincmd->keyword,
                 admincmd->syntax ) );
        ADMBP( va( " ^3Flag: ^7'%s'\n", admincmd->flag ) );
        ADMBP_end();
        return qtrue;
      }
      denied = qtrue;
    }
    ADMBP( va( "^3adminhelp: ^7%s '%s'\n",
      denied ? "you do not have permission to use" : "no help found for",
      param ) );
    ADMBP_end( );
    return qfalse;
  }
}

qboolean G_admin_admintest( gentity_t *ent )
{
  g_admin_level_t *l;

  if( !ent )
  {
    ADMP( "^3admintest: ^7you are on the console.\n" );
    return qtrue;
  }

  l = G_admin_level( ent->client->pers.admin ? ent->client->pers.admin->level : 0 );

  if( G_admin_permission( ent, ADMF_INCOGNITO ) )
    AP( va( "print \"^3admintest: ^7%s^7 is a level %d admin %s%s^7%s\n\"",
            ent->client->pers.netname,
            l ? l->level : 0,
            l ? "(" : "",
            l ? l->name : "",
            l ? ")" : "" ) );
  else
    ADMP( va( "^3admintest: ^7%s^7 is a level %d admin %s%s^7%s\n\"",
            ent->client->pers.netname,
            l ? l->level : 0,
            l ? "(" : "",
            l ? l->name : "",
            l ? ")" : "" ) );

  return qtrue;
}

qboolean G_admin_allready( gentity_t *ent )
{
  int i = 0;
  gclient_t *cl;

  // if game is in both warmup and developer mode, while not being in intermission,
  // /allready will set all players' readyToPlay flag to true
  if ( g_warmup.integer && g_cheats.integer && !level.intermissiontime )
  {
    if(IS_SCRIM) {
      if(level.scrim.mode == SCRIM_MODE_SETUP) {
        ADMP( "^3allready: ^7can't end warmup while a scrim is still being setup. see: scrim mode\n" );
        return qfalse;
      }

      if(level.scrim.mode == SCRIM_MODE_PAUSED) {
        ADMP( "^3allready: ^7can't end warmup while a scrim is paused. see: scrim mode\n" );
        return qfalse;
      }
    }
    // cycle through each client and change their ready flag
    for( i = 0; i < g_maxclients.integer; i++ )
    {
      cl = level.clients + i;

      if( cl->pers.connected != CON_CONNECTED )
        continue;

      if( cl->pers.teamSelection == TEAM_NONE )
        continue;

      //change the client's ready status
      cl->sess.readyToPlay = qtrue;
      cl->ps.stats[ STAT_FLAGS ] |= SFL_READY;
    }

    AP( va( "print \"^3allready: ^7%s ^7decided to end pre-game warmup early\n\"",
            ent ? ent->client->pers.netname : "console" ) );

    G_LevelRestart( qtrue );

    return qtrue;
  }

  if( !level.intermissiontime )
  {
    ADMP( "^3allready: ^7this command is only valid during intermission or developer mode pre-game warmup\n" );
    return qfalse;
  }

  for( i = 0; i < g_maxclients.integer; i++ )
  {
    cl = level.clients + i;
    if( cl->pers.connected != CON_CONNECTED )
      continue;

    if( cl->pers.teamSelection == TEAM_NONE )
      continue;

    cl->readyToExit = qtrue;
  }
  AP( va( "print \"^3allready:^7 %s^7 says everyone is READY now\n\"",
     ( ent ) ? ent->client->pers.netname : "console" ) );
  return qtrue;
}

qboolean G_admin_endvote( gentity_t *ent )
{
  char teamName[ sizeof( "spectators" ) ] = {"s"};
  char command[ MAX_ADMIN_CMD_LEN ];
  team_t team;
  qboolean cancel;
  char *msg;

  Cmd_ArgvBuffer( 0, command, sizeof( command ) );
  cancel = !Q_stricmp( command, "cancelvote" );
  if( Cmd_Argc() == 2 )
    Cmd_ArgvBuffer( 1, teamName, sizeof( teamName ) );
  team = G_TeamFromString( teamName );
  if( team == NUM_TEAMS )
  {
    ADMP( va( "^3%s: ^7invalid team '%s'\n", command, teamName ) );
    return qfalse;
  }
  msg = va( "print \"^3%s: ^7%s^7 decided that everyone voted %s\n\"",
    command, ( ent ) ? ent->client->pers.netname : "console",
    cancel ? "No" : "Yes" );
  if( !level.voteTime[ team ] )
  {
    ADMP( va( "^3%s: ^7no vote in progress\n", command ) );
    return qfalse;
  }
  admin_log( BG_Team( team )->name2 );
  if( team == TEAM_NONE )
    AP( msg );
  else
    G_TeamCommand( team, msg );

  G_EndVote( team, cancel );

  return qtrue;
}

qboolean G_admin_spec999( gentity_t *ent )
{
  int i;
  gentity_t *vic;

  for( i = 0; i < level.maxclients; i++ )
  {
    vic = &g_entities[ i ];
    if( !vic->client )
      continue;
    if( vic->client->pers.connected != CON_CONNECTED )
      continue;
    if( vic->client->pers.teamSelection == TEAM_NONE )
      continue;
    if( vic->client->ps.ping == 999 )
    {
      G_ChangeTeam( vic, TEAM_NONE );
      AP( va( "print \"^3spec999: ^7%s^7 moved %s^7 to spectators\n\"",
        ( ent ) ? ent->client->pers.netname : "console",
        vic->client->pers.netname ) );
    }
  }
  return qtrue;
}

qboolean G_admin_forcespec( gentity_t *ent )
{
  const int seconds = G_admin_parse_time( g_adminTempSpec.string );;
  team_t    old_team;
  char      name[MAX_NAME_LENGTH];
  namelog_t *vic;

  Cmd_ArgvBuffer(1, name, sizeof(name));

  if(Cmd_Argc() < 2) {
    ADMP("^3forcepec: ^7usage: forcepec \n");
    return qfalse;
  }

  if(!(vic = G_NamelogFromString(ent, name))) {
    ADMP(va("^3forcepec: ^7no player match\n"));
    return qfalse;
  }

  if(vic->slot >= 0 && vic->slot < MAX_CLIENTS) {
    old_team = level.clients[vic->slot].pers.teamSelection;
  } else {
    old_team = vic->team;
  }

  if( ent && !admin_higher_admin( ent->client->pers.admin,
      G_admin_admin( vic->guid ) ) )
  {
    ADMP( "^3forcepec: ^7sorry, but your intended victim has a higher "
        " admin level than you\n" );
    return qfalse;
  }

  if(old_team == TEAM_NONE) {
    if(seconds > 0) {
      if(vic->specExpires > level.time) {
        SV_GameSendServerCommand( ent-g_entities,
          va(
             "print \"^3forcespec: ^7%s is already forced to be on spectators\n\"",
             vic->name[vic->nameOffset] ) );
        return qfalse;
      }
    } else {
      SV_GameSendServerCommand( ent-g_entities,
        va(
           "print \"^3forcespec: ^7%s is already on spectators\n\"",
           vic->name[vic->nameOffset] ) );
      return qfalse;
    }
  }

  if(IS_SCRIM) {
    G_Scrim_Remove_Player_From_Rosters(vic, qfalse);
  }

  admin_log(
    va(
      "%d (%s) \"%s" S_COLOR_WHITE "\"",
      (vic->slot < 0) ? vic->id : vic->slot,
      vic->guid,
      vic->name[vic->nameOffset]));

  if(vic->slot < 0 || vic->slot >= MAX_CLIENTS) {
    vic->team = TEAM_NONE;
  } else {
    if(old_team != TEAM_NONE) {
      G_ChangeTeam( &g_entities[vic->slot], TEAM_NONE );
    }
  }

  if(seconds > 0) {
    vic->specExpires = level.time + ( seconds * 1000 );
  }

  if(seconds > 0) {
    AP( va( "print \"^3forcespec: ^7%s^7 forced %s^7 on to the %s team for %d seconds\n\"",
            ( ent ) ? ent->client->pers.netname : "console",
            vic->name[vic->nameOffset], BG_Team( TEAM_NONE )->name2, seconds ) );
  } else {
    AP( va( "print \"^3forcespec: ^7%s^7 put %s^7 on to the %s team\n\"",
            ( ent ) ? ent->client->pers.netname : "console",
            vic->name[vic->nameOffset], BG_Team( TEAM_NONE )->name2 ) );
  }

  return qtrue;
}

qboolean G_admin_rename( gentity_t *ent )
{
  int pid;
  char name[ MAX_NAME_LENGTH ];
  char newname[ MAX_NAME_LENGTH ];
  char err[ MAX_STRING_CHARS ];
  char userinfo[ MAX_INFO_STRING ];
  gentity_t *victim = NULL;

  if( Cmd_Argc() < 3 )
  {
    ADMP( "^3rename: ^7usage: rename [name] [newname]\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  Q_strncpyz( newname, ConcatArgs( 2 ), sizeof( newname ) );
  if( ( pid = G_ClientNumberFromString( name, err, sizeof( err ) ) ) == -1 )
  {
    ADMP( va( "^3rename: ^7%s", err ) );
    return qfalse;
  }
  victim = &g_entities[ pid ];
  if( !admin_higher( ent, victim ) )
  {
    ADMP( "^3rename: ^7sorry, but your intended victim has a higher admin"
        " level than you\n" );
    return qfalse;
  }
  if( !G_admin_name_check( victim, newname, err, sizeof( err ) ) )
  {
    ADMP( va( "^3rename: ^7%s\n", err ) );
    return qfalse;
  }
  if( victim->client->pers.connected != CON_CONNECTED )
  {
    ADMP( "^3rename: ^7sorry, but your intended victim is still connecting\n" );
    return qfalse;
  }
  admin_log( va( "%d (%s) \"%s" S_COLOR_WHITE "\"", pid,
    victim->client->pers.guid, victim->client->pers.netname ) );
  admin_log( newname );
  SV_GetUserinfo( pid, userinfo, sizeof( userinfo ) );
  AP( va( "print \"^3rename: ^7%s^7 has been renamed to %s^7 by %s\n\"",
          victim->client->pers.netname,
          newname,
          ( ent ) ? ent->client->pers.netname : "console" ) );
  Info_SetValueForKey( userinfo, "name", newname );
  SV_SetUserinfo( pid, userinfo );
  ClientUserinfoChanged( pid, qtrue );
  return qtrue;
}

qboolean G_admin_transform( gentity_t *ent )
{
  int pid;
  char name[ MAX_NAME_LENGTH ];
  char modelname[ MAX_NAME_LENGTH ];
  char skin[ MAX_NAME_LENGTH ];
  char err[ MAX_STRING_CHARS ];
  char userinfo[ MAX_INFO_STRING ];
  gentity_t *victim = NULL;
  qboolean found;

  if (Cmd_Argc() < 3)
  {
    ADMP("^3transform: ^7usage: transform [name|slot#] [model] <skin>\n");
    return qfalse;
  }

  Cmd_ArgvBuffer(1, name, sizeof(name));
  Cmd_ArgvBuffer(2, modelname, sizeof(modelname));

  strcpy(skin, "default");
  if (Cmd_Argc() >= 4)
  {
    Cmd_ArgvBuffer(1, skin, sizeof(skin));
  }

  pid = G_ClientNumberFromString(name, err, sizeof(err));
  if (pid == -1)
  {
    ADMP(va("^3transform: ^7%s", err));
    return qfalse;
  }

  victim = &g_entities[ pid ];
  if (victim->client->pers.connected != CON_CONNECTED)
  {
    ADMP("^3transform: ^7sorry, but your intended victim is still connecting\n");
    return qfalse;
  }

  found = G_IsValidPlayerModel(modelname);

  if (!found)
  {
    ADMP(va("^3transform: ^7no matching model %s\n", modelname));
    return qfalse;
  }

  SV_GetUserinfo(pid, userinfo, sizeof(userinfo));
  AP( va("print \"^3transform: ^7%s^7 has been changed into %s^7 by %s\n\"",
         victim->client->pers.netname, modelname,
         (ent ? ent->client->pers.netname : "console")) );

  Info_SetValueForKey( userinfo, "model", modelname );
  Info_SetValueForKey( userinfo, "skin", GetSkin(modelname, skin));
  Info_SetValueForKey( userinfo, "voice", modelname );
  SV_SetUserinfo( pid, userinfo );
  ClientUserinfoChanged( pid, qtrue );

  return qtrue;
}

qboolean G_admin_restart( gentity_t *ent )
{
  char      layout[ MAX_CVAR_VALUE_STRING ] = { "" };
  char      teampref[ MAX_STRING_CHARS ] = { "" };
  char      map[ MAX_CVAR_VALUE_STRING ];
  int       i;
  gclient_t *cl;

  if( Cmd_Argc( ) > 1 )
  {
    char map[ MAX_QPATH ];

    Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
    Cmd_ArgvBuffer( 1, layout, sizeof( layout ) );

    // Figure out which argument is which
    if( Q_stricmp( layout, "keepteams" ) &&
        Q_stricmp( layout, "keepteamslock" ) &&
        Q_stricmp( layout, "switchteams" ) &&
        Q_stricmp( layout, "switchteamslock" ) )
    {
      if( G_LayoutExists( map, layout ) )
      {
        Cvar_SetSafe( "g_nextLayout", layout );
      }
      else
      {
        ADMP( va( "^3restart: ^7layout '%s' does not exist\n", layout ) );
        return qfalse;
      }
    }
    else
    {
      layout[ 0 ] = '\0';
      Cmd_ArgvBuffer( 1, teampref, sizeof( teampref ) );
    }
  }

  if( Cmd_Argc( ) > 2 )
    Cmd_ArgvBuffer( 2, teampref, sizeof( teampref ) );

  admin_log( layout );
  admin_log( teampref );

  if( !Q_stricmpn( teampref, "keepteams", 9 ) )
  {
    for( i = 0; i < g_maxclients.integer; i++ )
    {
      cl = level.clients + i;
      if( cl->pers.connected != CON_CONNECTED )
        continue;

      if( cl->pers.teamSelection == TEAM_NONE )
        continue;

      cl->sess.restartTeam = cl->pers.teamSelection;
    }
  }
  else if( !Q_stricmpn( teampref, "switchteams", 11 ) )
  {
    if(IS_SCRIM) {
      level.scrim.swap_teams = qtrue;
    }

    for( i = 0; i < g_maxclients.integer; i++ )
    {
      cl = level.clients + i;

      if( cl->pers.connected != CON_CONNECTED )
        continue;

      if( cl->pers.teamSelection == TEAM_HUMANS )
        cl->sess.restartTeam = TEAM_ALIENS;
      else if(cl->pers.teamSelection == TEAM_ALIENS )
	    cl->sess.restartTeam = TEAM_HUMANS;
    }
  }

  if( !Q_stricmp( teampref, "switchteamslock" ) ||
      !Q_stricmp( teampref, "keepteamslock" ) )
    Cvar_SetSafe( "g_lockTeamsAtStart", "1" );

  Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
  G_MapConfigs( map );
  Cbuf_ExecuteText( EXEC_APPEND, "map_restart\n" );

  AP( va( "print \"^3restart: ^7map restarted by %s %s %s\n\"",
          ( ent ) ? ent->client->pers.netname : "console",
          ( layout[ 0 ] ) ? va( "^7(forcing layout '%s^7')", layout ) : "",
          ( teampref[ 0 ] ) ? va( "^7(with teams option: '%s^7')", teampref ) : "" ) );
  return qtrue;
}

qboolean G_admin_nextmap( gentity_t *ent )
{
  int index = rand( ) % MAX_INTERMISSION_SOUND_SETS;

  if( level.exited )
    return qfalse;
  AP( va( "print \"^3nextmap: ^7%s^7 decided to load the next map\n\"",
    ( ent ) ? ent->client->pers.netname : "console" ) );
  level.lastWin = TEAM_NONE;
  Q_strncpyz(
    level.winner_configstring,
    va("%i %i %i", MATCHOUTCOME_EVAC, index, TEAM_NONE),
    sizeof(level.winner_configstring));
  LogExit( 
    va(
      "nextmap was run by %s",
      ( ent ) ? ent->client->pers.netname : "console"));
  Cvar_SetSafe( "g_warmup", "1" );
  SV_SetConfigstring( CS_WARMUP, va( "%d", IS_WARMUP ) );

  return qtrue;
}

qboolean G_admin_setnextmap( gentity_t *ent )
{
  int argc = Cmd_Argc();
  char map[ MAX_QPATH ];
  char layout[ MAX_QPATH ];

  if( argc < 2 || argc > 3 )
  {
    ADMP( "^3setnextmap: ^7usage: setnextmap [map] (layout)\n" );
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, map, sizeof( map ) );

  if( !G_MapExists( map ) )
  {
    ADMP( va( "^3setnextmap: ^7map '%s' does not exist\n", map ) );
    return qfalse;
  }

  if( argc > 2 )
  {
    Cmd_ArgvBuffer( 2, layout, sizeof( layout ) );

    if( !G_LayoutExists( map, layout ) )
    {
      ADMP( va( "^3setnextmap: ^7layout '%s' does not exist for map '%s'\n", layout, map ) );
      return qfalse;
    }

    Cvar_SetSafe( "g_nextLayout", layout );
  }
  else
    Cvar_SetSafe( "g_nextLayout", "" );

  Cvar_SetSafe( "g_nextMap", map );

  AP( va( "print \"^3setnextmap: ^7%s^7 has set the next map to '%s'%s\n\"",
          ( ent ) ? ent->client->pers.netname : "console", map,
          argc > 2 ? va( " with layout '%s'", layout ) : "" ) );
  return qtrue;
}

static qboolean namelog_matchip( void *namelog, const void *ip )
{
  int i;
  namelog_t *n = (namelog_t *)namelog;
  for( i = 0; i < MAX_NAMELOG_ADDRS && n->ip[ i ].str[ 0 ]; i++ )
  {
    if( G_AddressCompare( &n->ip[ i ], (addr_t *)ip ) ||
        G_AddressCompare( (addr_t *)ip, &n->ip[ i ] ) )
      return qtrue;
  }
  return qfalse;
}
static qboolean namelog_matchname( void *namelog, const void *name )
{
  char match[ MAX_NAME_LENGTH ];
  int i;
  namelog_t *n = (namelog_t *)namelog;
  for( i = 0; i < MAX_NAMELOG_NAMES && n->name[ i ][ 0 ]; i++ )
  {
    G_SanitiseString( n->name[ i ], match, sizeof( match ) );
    if( strstr( match, (const char *)name ) )
      return qtrue;
  }
  return qfalse;
}
static void namelog_out( void *namelog, char *str )
{
  namelog_t *n = (namelog_t *)namelog;
  char *p = str;
  int l, l2 = MAX_STRING_CHARS, i;
  const char *scolor;

  if( n->slot > -1 )
  {
    scolor = S_COLOR_YELLOW;
    l = Q_snprintf( p, l2, "%s%-2d", scolor, n->slot );
    p += l;
    l2 -= l;
  }
  else
  {
    *p++ = '-';
    *p++ = ' ';
    *p = '\0';
    l2 -= 2;
    scolor = S_COLOR_WHITE;
  }

  for( i = 0; i < MAX_NAMELOG_ADDRS && n->ip[ i ].str[ 0 ]; i++ )
  {
    l = Q_snprintf( p, l2, " %s", n->ip[ i ].str );
    p += l;
    l2 -= l;
  }

  for( i = 0; i < MAX_NAMELOG_NAMES && n->name[ i ][ 0 ]; i++ )
  {
    l = Q_snprintf( p, l2, " '" S_COLOR_WHITE "%s%s'%s", n->name[ i ], scolor,
                    i == n->nameOffset ? "*" : "" );
    p += l;
    l2 -= l;
  }
}
qboolean G_admin_namelog( gentity_t *ent )
{
  char search[ MAX_NAME_LENGTH ] = {""};
  char s2[ MAX_NAME_LENGTH ] = {""};
  addr_t ip;
  qboolean ipmatch = qfalse;
  int start = MAX_CLIENTS, i;

  if( Cmd_Argc() == 3 )
  {
    Cmd_ArgvBuffer( 2, search, sizeof( search ) );
    start = atoi( search );
  }
  if( Cmd_Argc() > 1 )
  {
    Cmd_ArgvBuffer( 1, search, sizeof( search ) );
    i = 0;
    if( Cmd_Argc() == 2 )
      for( i = search[ 0 ] == '-'; isdigit( search[ i ] ); i++ );
    if( !search[ i ] )
      start = atoi( search );
    else if( !( ipmatch = G_AddressParse( search, &ip ) ) )
      G_SanitiseString( search, s2, sizeof( s2 ) );
  }

  admin_search( ent, "namelog", "recent players",
    ipmatch ? namelog_matchip : namelog_matchname, namelog_out, level.namelogs,
    ipmatch ? (void *)&ip : s2, start, MAX_CLIENTS, MAX_ADMIN_LISTITEMS );
  return qtrue;
}

/*
==================
G_NamelogFromString

This is similar to G_ClientNumberFromString but for namelog instead
Returns NULL if not exactly 1 match
==================
*/
namelog_t *G_NamelogFromString( gentity_t *ent, char *s )
{
  namelog_t *p, *m = NULL;
  int       i, found = 0;
  char      n2[ MAX_NAME_LENGTH ] = {""};
  char      s2[ MAX_NAME_LENGTH ] = {""};

  if( !s[ 0 ] )
    return NULL;

  // if a number is provided, it is a clientnum or namelog id
  for( i = 0; s[ i ] && isdigit( s[ i ] ); i++ );
  if( !s[ i ] )
  {
    i = atoi( s );

    if( i >= 0 && i < level.maxclients )
    {
      if( level.clients[ i ].pers.connected != CON_DISCONNECTED )
        return level.clients[ i ].pers.namelog;
    }
    else if( i >= MAX_CLIENTS )
    {
      for( p = level.namelogs; p; p = p->next )
      {
        if( p->id == i )
          break;
      }
      if( p )
        return p;
    }

    return NULL;
  }

  // check for a name match
  G_SanitiseString( s, s2, sizeof( s2 ) );

  for( p = level.namelogs; p; p = p->next )
  {
    for( i = 0; i < MAX_NAMELOG_NAMES && p->name[ i ][ 0 ]; i++ )
    {
      G_SanitiseString( p->name[ i ], n2, sizeof( n2 ) );

      // if this is an exact match to a current player
      if( i == p->nameOffset && p->slot > -1 && !strcmp( s2, n2 ) )
        return p;

      if( strstr( n2, s2 ) )
        m = p;
    }

    if( m == p )
      found++;
  }

  if( found == 1 )
    return m;

  if( found > 1 )
    admin_search( ent, "namelog", "recent players", namelog_matchname,
      namelog_out, level.namelogs, s2, 0, MAX_CLIENTS, -1 );

  return NULL;
}

qboolean G_admin_lock( gentity_t *ent )
{
  char command[ MAX_ADMIN_CMD_LEN ];
  char teamName[ sizeof( "aliens" ) ];
  team_t team;
  qboolean lock, fail = qfalse;

  Cmd_ArgvBuffer( 0, command, sizeof( command ) );
  if( Cmd_Argc() < 2 )
  {
    ADMP( va( "^3%s: ^7usage: %s [a|h]\n", command, command ) );
    return qfalse;
  }
  lock = !Q_stricmp( command, "lock" );
  Cmd_ArgvBuffer( 1, teamName, sizeof( teamName ) );
  team = G_TeamFromString( teamName );

  if( team == TEAM_ALIENS )
  {
    if( level.alienTeamLocked == lock )
      fail = qtrue;
    else
      level.alienTeamLocked = lock;
  }
  else if( team == TEAM_HUMANS )
  {
    if( level.humanTeamLocked == lock )
      fail = qtrue;
    else
      level.humanTeamLocked = lock;
  }
  else
  {
    ADMP( va( "^3%s: ^7invalid team: '%s'\n", command, teamName ) );
    return qfalse;
  }

  if( fail )
  {
    ADMP( va( "^3%s: ^7the %s team is %s locked\n",
      command, BG_Team( team )->name2, lock ? "already" : "not currently" ) );

    return qfalse;
  }

  admin_log( BG_Team( team )->name2 );
  AP( va( "print \"^3%s: ^7the %s team has been %slocked by %s\n\"",
    command, BG_Team( team )->name2, lock ? "" : "un",
    ent ? ent->client->pers.netname : "console" ) );

  return qtrue;
}

qboolean G_admin_builder( gentity_t *ent )
{
  vec3_t     forward, right, up;
  vec3_t     start, end, dist;
  trace_t    tr;
  gentity_t  *traceEnt;
  buildLog_t *log;
  int        i;
  qboolean   buildlog;
  char       logid[ 20 ] = {""};

  if( !ent )
  {
    ADMP( "^3builder: ^7console can't aim.\n" );
    return qfalse;
  }

  buildlog = G_admin_permission( ent, "buildlog" );

  AngleVectors( ent->client->ps.viewangles, forward, right, up );
  if( ent->client->pers.teamSelection != TEAM_NONE &&
      ent->client->sess.spectatorState == SPECTATOR_NOT )
    BG_CalcMuzzlePointFromPS( &ent->client->ps, forward, right, up, start );
  else
    VectorCopy( ent->client->ps.origin, start );
  VectorMA( start, 1000, forward, end );

  SV_Trace( &tr, start, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID, TT_AABB );
  traceEnt = &g_entities[ tr.entityNum ];
  if( tr.fraction < 1.0f && ( traceEnt->s.eType == ET_BUILDABLE ) )
  {
    if( !buildlog &&
        ent->client->pers.teamSelection != TEAM_NONE &&
        ent->client->pers.teamSelection != traceEnt->buildableTeam )
    {
      ADMP( "^3builder: ^7structure not owned by your team\n" );
      return qfalse;
    }

    if( buildlog )
    {
      for( i = 0 ; buildlog && i < level.numBuildLogs; i++ )
      {
        log = &level.buildLog[ ( level.buildId - i - 1 ) % MAX_BUILDLOG ];
        if( log->fate != BF_CONSTRUCT || traceEnt->s.modelindex != log->modelindex )
          continue;

        VectorSubtract( traceEnt->s.pos.trBase, log->origin, dist );
        if( VectorLengthSquared( dist ) < 2.0f )
          Com_sprintf( logid, sizeof( logid ), ", buildlog #%d",
                       MAX_CLIENTS + level.buildId - i - 1 );
      }
    }

    ADMP( va( "^3builder: ^7%s%s%s^7%s\n",
      BG_Buildable( traceEnt->s.modelindex )->humanName,
      traceEnt->builtBy ? " built by " : "",
      traceEnt->builtBy ?
        traceEnt->builtBy->name[ traceEnt->builtBy->nameOffset ] :
        "",
      buildlog ? ( logid[ 0 ] ? logid : ", not in buildlog" ) : "" ) );
  }
  else
    ADMP( "^3builder: ^7no structure found under crosshair\n" );

  return qtrue;
}

qboolean G_admin_pause( gentity_t *ent )
{
  if( !level.pausedTime )
  {
    AP( va( "print \"^3pause: ^7%s^7 paused the game.\n\"",
          ( ent ) ? ent->client->pers.netname : "console" ) );
    level.pausedTime = 1;
    SV_GameSendServerCommand( -1,
                            va( "cp \"The game has been paused. Please wait.\" %d",
                                CP_PAUSE ) );
  }
  else
  {
    // Prevent accidental pause->unpause race conditions by two admins
    if( level.pausedTime < 1000 )
    {
      ADMP( "^3pause: ^7Unpausing so soon assumed accidental and ignored.\n" );
      return qfalse;
    }

    AP( va( "print \"^3pause: ^7%s^7 unpaused the game (Paused for %d sec) \n\"",
          ( ent ) ? ent->client->pers.netname : "console",
          (int) ( (float) level.pausedTime / 1000.0f ) ) );
    SV_GameSendServerCommand( -1,
                            va( "cp \"The game has been unpaused!\" %d",
                                CP_PAUSE ) );

    level.pausedTime = 0;
  }

  return qtrue;
}

qboolean G_admin_playmap( gentity_t *ent )
{
  char   cmd[ MAX_TOKEN_CHARS ],
         map[ MAX_TOKEN_CHARS ], layout[ MAX_TOKEN_CHARS ],
         extra[ MAX_TOKEN_CHARS ];
  char   *flags;
  playMapError_t playMapError;
  g_admin_cmd_t *admincmd;

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );

  if( Cmd_Argc( ) < 2 )
  {
    // TODO: [layout [flags]] announce them once they're implemented

    // overwrite with current map
    Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );

    ADMP( va( S_COLOR_YELLOW "playmap" S_COLOR_WHITE ": Current map is "
	      S_COLOR_CYAN "%s" S_COLOR_WHITE ".\n", map ) );
    G_PrintPlayMapQueue( ent );
    ADMP( "\n" );

    G_PrintPlayMapPool( ent, -1, qfalse );

    // Get command structure
    admincmd = G_admin_cmd( "playmap" );
    ADMP( va( S_COLOR_YELLOW "\nusage: " S_COLOR_WHITE "%s %s\n",
	      admincmd->keyword, admincmd->syntax ) );

    return qtrue;
  }

  // Else if argc > 1
  Cmd_ArgvBuffer( 1, map, sizeof( map ) );
  Cmd_ArgvBuffer( 2, layout, sizeof( layout ) );
  Cmd_ArgvBuffer( 3, extra, sizeof( extra ) );
  if( *layout == '+' || *layout == '-' )
  {
    flags = ConcatArgs( 2 );
    *layout = '\0';
  } else
    flags = ConcatArgs( 3 );

  if( g_debugPlayMap.integer > 0 )
    SV_GameSendServerCommand( ent-g_entities,
			    va( "print \"DEBUG: cmd=%s\n"
				"       map=%s\n"
				"       layout=%s\n"
				"       flags=%s\n\"",
				cmd, map, layout, flags ) );

  playMapError = G_PlayMapEnqueue( map, layout,
				   ent ? qfalse : qtrue,
           (!ent || ent->client->pers.guidless) ? "\0" : ent->client->pers.guid,
				   flags, ent );
  if (playMapError.errorCode == PLAYMAP_ERROR_NONE)
  {
    SV_GameSendServerCommand( -1,
			    va( "print \"%s" S_COLOR_WHITE
				" added map " S_COLOR_CYAN "%s" S_COLOR_WHITE
				" to playlist\n\"",
				ent ? ent->client->pers.netname : "console", map ) );
  } else{
    ADMP( va( S_COLOR_YELLOW "playmap" S_COLOR_WHITE ": %s\n", playMapError.errorMessage ) );
    admincmd = G_admin_cmd( "playmap" );
    ADMP( va( S_COLOR_YELLOW "\nusage: " S_COLOR_WHITE "%s %s\n",
	      admincmd->keyword, admincmd->syntax ) );
  }

  return qtrue;
}

qboolean G_admin_playpool( gentity_t *ent )
{
  char           cmd[ MAX_TOKEN_CHARS ],
    		 map[ MAX_TOKEN_CHARS ],
    		 mapType[ MAX_TOKEN_CHARS ];
  int 		 page;
  playMapError_t playMapError;
  g_admin_cmd_t *admincmd;

  // Get command structure
  admincmd = G_admin_cmd( "playpool" );

  if( Cmd_Argc( ) < 2 )
  {
    G_PrintPlayMapPool( ent, -1, qfalse );
    ADMP( "\n" );

    ADMP( va( S_COLOR_YELLOW "usage: " S_COLOR_WHITE "%s %s\n",
	      admincmd->keyword, admincmd->syntax ) );
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, cmd, sizeof( cmd ) );

  if( !Q_stricmp( cmd, "add" ) )
  {
    if( Cmd_Argc( ) < 3 )
    {
      ADMP( S_COLOR_YELLOW "playpool: " S_COLOR_WHITE
	    "usage: playpool add (^5mapname^7) [^5maptype^7]\n" );
      return qfalse;
    }

    Cmd_ArgvBuffer( 2, map, sizeof( map ) );
    if( !G_MapExists( map ) )
    {
      ADMP( va( "^3playpool: ^7invalid map name '%s'\n", map ) );
      return qfalse;
    }

    if( Cmd_Argc( ) > 3 )
      Cmd_ArgvBuffer( 3, mapType, sizeof( mapType ) );

    playMapError =
      G_AddToPlayMapPool( map, ( Cmd_Argc( ) > 3 ) ? mapType : PLAYMAP_MAPTYPE_NONE,
			  0, 0, qtrue );
    if( playMapError.errorCode != PLAYMAP_ERROR_NONE )
    {
      ADMP( va( "^3playpool: ^7%s.\n", playMapError.errorMessage ) );
      return qfalse;
    }

    ADMP( va( "^3playpool: ^7added map '%s' to playmap pool.\n", map ) );
    return qtrue;
  }

  if( !Q_stricmp( cmd, "remove" ) )
  {
    if( Cmd_Argc( ) < 3 )
    {
      ADMP( "^3playpool: ^7usage: playpool remove (^5mapname^7)\n" );
      return qfalse;
    }

    Cmd_ArgvBuffer( 2, map, sizeof( map ) );
    playMapError = G_RemoveFromPlayMapPool( map );
    if( playMapError.errorCode != PLAYMAP_ERROR_NONE )
    {
      ADMP( va( "^3playpool: ^7%s.\n", playMapError.errorMessage ) );
      return qfalse;
    }

    ADMP( va( "^3playpool: ^7removed map '%s' from playmap pool.\n", map ) );
    return qtrue;
  }

  if( !Q_stricmp( cmd, "clear" ) )
  {
    playMapError = G_ClearPlayMapPool();
    if( playMapError.errorCode != PLAYMAP_ERROR_NONE )
    {
      ADMP( va( "^3playpool: ^7%s.\n", playMapError.errorMessage ) );
      return qfalse;
    }

    ADMP( va( "^3playpool: ^7cleared playmap pool.\n" ) );
    return qtrue;
  }

  if( !Q_stricmp( cmd, "list" ) )
  {
    if( Cmd_Argc( ) > 2 )
    {
      Cmd_ArgvBuffer( 2, map, sizeof( map ) );
      page = atoi( map ) - 1;
    } else page = 0;

    G_PrintPlayMapPool( ent, page, qfalse );
    ADMP( "\n" );

    return qtrue;
  }

  if( !Q_stricmp( cmd, "save" ) )
  {
    playMapError = G_SavePlayMapPool();
    if( playMapError.errorCode != PLAYMAP_ERROR_NONE )
    {
      ADMP( va( "^3playpool: ^7%s.\n", playMapError.errorMessage ) );
      return qfalse;
    }

    ADMP( va( "^3playpool: ^7saved playmap pool to '%s'.\n", g_playMapPoolConfig.string ) );
    return qtrue;
  }

  if( !Q_stricmp( cmd, "reload" ) )
  {
    playMapError = G_ReloadPlayMapPool();
    if( playMapError.errorCode != PLAYMAP_ERROR_NONE )
    {
      ADMP( va( "^3playpool: ^7%s.\n", playMapError.errorMessage ) );
      return qfalse;
    }

    ADMP( va( "^3playpool: ^7reloaded playmap pool from '%s'.\n", g_playMapPoolConfig.string ) );
    return qtrue;
  }

  ADMP( va( S_COLOR_YELLOW "usage: " S_COLOR_WHITE "%s %s\n",
	    admincmd->keyword, admincmd->syntax ) );
  return qfalse;
}

static char *fates[] =
{
  "^2built^7",
  "^3deconstructed^7",
  "^7replaced^7",
  "^3destroyed^7",
  "^1TEAMKILLED^7",
  "^7unpowered^7",
  "removed"
};

qboolean G_admin_slap( gentity_t *ent )
{
  int pid;
  char name[ MAX_NAME_LENGTH ], *reason, err[ MAX_STRING_CHARS ];
  int minargc;
  gentity_t *vic;
  vec3_t dir;
  minargc = 3;
  if( G_admin_permission( ent, ADMF_UNACCOUNTABLE ) )
    minargc = 2;

  if( Cmd_Argc() < minargc )
  {
    ADMP( "^3slap: ^7usage: slap [^3name^6|^3slot^7] (^5reason)\n" );
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  reason = ConcatArgs( 2 );
  if( ( pid = G_ClientNumberFromString( name, err, sizeof( err ) ) ) == -1 )
  {
    ADMP( va( "^3slap: ^7%s", err ) );
    return qfalse;
  }
  vic = &g_entities[ pid ];
  if( !admin_higher( ent, vic ) )
  {
    ADMP( "^3slap: ^7sorry, but your intended victim has a higher admin"
        " level than you\n" );
    return qfalse;
  }

  if( vic->client->pers.teamSelection == TEAM_NONE ||
      vic->client->pers.classSelection == PCL_NONE )
  {
    ADMP( "^3slap: ^7can't slap spectators\n" );
    return qfalse;
  }

  if(vic->health <= 0) {
    ADMP( 
      va(
        "^3slap: ^7%s^7 has passed on from too much abuse, no use in beating a dead hourse.\n",
        vic->client->pers.netname));
    return qfalse;
  }

  // knockback in a random direction
  dir[0] = crandom();
  dir[1] = crandom();
  dir[2] = random();
  G_Knockback( vic, dir, 100 );

  SV_GameSendServerCommand( vic-g_entities,
                          va( "cp \"%s^1 is not amused!\n^7%s\n\" %d",
                              ent ? ent->client->pers.netname : "console",
                              reason, CP_ADMIN ) );
  AP( va( "print \"^3slap: ^7%s^7 has been slapped by %s^7 with the reason: ^7%s\n\"",
          vic->client->pers.netname,
          ( ent ) ? ent->client->pers.netname : "console",
          ( *reason ) ? reason : "No reason specified" ) );

  G_Damage(
    vic,
    NULL,
    NULL,
    dir,
    vic->r.currentOrigin,
    BG_HP2SU(24),
    DAMAGE_NO_KNOCKBACK|DAMAGE_NO_PROTECTION|DAMAGE_GODLESS|DAMAGE_NO_LOCDAMAGE|DAMAGE_NO_ARMOR,
    MOD_SLAP);

  return qtrue;
}

qboolean G_admin_buildlog( gentity_t *ent )
{
  char       search[ MAX_NAME_LENGTH ] = {""};
  char       s[ MAX_NAME_LENGTH ] = {""};
  char       n[ MAX_NAME_LENGTH ];
  char       stamp[ 8 ];
  int        id = -1;
  int        printed = 0;
  int        time;
  int        start = MAX_CLIENTS + level.buildId - level.numBuildLogs;
  int        i = 0, j;
  buildLog_t *log;

  if( !level.buildId )
  {
    ADMP( "^3buildlog: ^7log is empty\n" );
    return qtrue;
  }

  if( Cmd_Argc() == 3 )
  {
    Cmd_ArgvBuffer( 2, search, sizeof( search ) );
    start = atoi( search );
  }
  if( Cmd_Argc() > 1 )
  {
    Cmd_ArgvBuffer( 1, search, sizeof( search ) );
    for( i = search[ 0 ] == '-'; isdigit( search[ i ] ); i++ );
    if( i && !search[ i ] )
    {
      id = atoi( search );
      if( Cmd_Argc() == 2 && ( id < 0 || id >= MAX_CLIENTS ) )
      {
        start = id;
        id = -1;
      }
      else if( id < 0 || id >= MAX_CLIENTS ||
        level.clients[ id ].pers.connected != CON_CONNECTED )
      {
        ADMP( "^3buildlog: ^7invalid client id\n" );
        return qfalse;
      }
    }
    else
      G_SanitiseString( search, s, sizeof( s ) );
  }
  else
    start = MAX( -MAX_ADMIN_LISTITEMS, -level.buildId );

  if( start < 0 )
    start = MAX( level.buildId - level.numBuildLogs, start + level.buildId );
  else
    start -= MAX_CLIENTS;
  if( start < level.buildId - level.numBuildLogs || start >= level.buildId )
  {
    ADMP( "^3buildlog: ^7invalid build ID\n" );
    return qfalse;
  }

  if( ent && ent->client->pers.teamSelection != TEAM_NONE )
    SV_GameSendServerCommand( -1,
      va( "print \"^3buildlog: ^7%s^7 requested a log of recent building activity\n\"",
           ent->client->pers.netname  ) );

  ADMBP_begin();
  for( i = start; i < level.buildId && printed < MAX_ADMIN_LISTITEMS; i++ )
  {
    log = &level.buildLog[ i % MAX_BUILDLOG ];
    if( id >= 0 && id < MAX_CLIENTS )
    {
      if( log->actor != level.clients[ id ].pers.namelog )
        continue;
    }
    else if( s[ 0 ] )
    {
      if( !log->actor )
        continue;
      for( j = 0; j < MAX_NAMELOG_NAMES && log->actor->name[ j ][ 0 ]; j++ )
      {
        G_SanitiseString( log->actor->name[ j ], n, sizeof( n ) );
        if( strstr( n, s ) )
          break;
      }
      if( j >= MAX_NAMELOG_NAMES || !log->actor->name[ j ][ 0 ] )
        continue;
    }
    printed++;
    time = ( log->time - level.startTime ) / 1000;
    Com_sprintf( stamp, sizeof( stamp ), "%3d:%02d", time / 60, time % 60 );
    ADMBP( va( "^2%c^7%-3d %s %s^7%s%s%s %s%s%s%s\n",
      log->actor && log->fate != BF_REPLACE && log->fate != BF_UNPOWER ?
        '*' : ' ',
      i + MAX_CLIENTS,
      log->actor && ( log->fate == BF_REPLACE || log->fate == BF_UNPOWER ) ?
        "    \\_" : stamp,
      BG_Buildable( log->modelindex )->humanName,
      log->builtBy && log->fate != BF_CONSTRUCT ?
        " (built by " :
        "",
      log->builtBy && log->fate != BF_CONSTRUCT ?
        log->builtBy->name[ log->builtBy->nameOffset ] :
        "",
      log->builtBy && log->fate != BF_CONSTRUCT ?
        "^7)" :
        "",
      fates[ log->fate ],
      log->attackerBuildable ?
               va( " by some %s%s", log->actor ? "" : "default ",
                              BG_Buildable( log->attackerBuildable )->humanName ) : "",
      log->actor ? ( log->attackerBuildable ? " built by " : " by " ) : "",
      log->actor ?
        log->actor->name[ log->actor->nameOffset ] : "") );
  }
  ADMBP( va( "^3buildlog: ^7showing %d build logs %d - %d of %d - %d.  %s\n",
    printed, start + MAX_CLIENTS, i + MAX_CLIENTS - 1,
    level.buildId + MAX_CLIENTS - level.numBuildLogs,
    level.buildId + MAX_CLIENTS - 1,
    i < level.buildId ? va( "run 'buildlog %s%s%d' to see more",
      search, search[ 0 ] ? " " : "", i + MAX_CLIENTS ) : "" ) );
  ADMBP_end();
  return qtrue;
}

qboolean G_admin_revert( gentity_t *ent )
{
  char       arg[ MAX_TOKEN_CHARS ];
  char       time[ MAX_DURATION_LENGTH ];
  int        id;
  buildLog_t *log;

  if( Cmd_Argc() != 2 )
  {
    ADMP( "^3revert: ^7usage: revert [id]\n" );
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, arg, sizeof( arg ) );
  id = atoi( arg ) - MAX_CLIENTS;
  if( id < level.buildId - level.numBuildLogs || id >= level.buildId )
  {
    ADMP( "^3revert: ^7invalid id\n" );
    return qfalse;
  }

  log = &level.buildLog[ id % MAX_BUILDLOG ];
  if( !log->actor || log->fate == BF_REPLACE || log->fate == BF_UNPOWER )
  {
    // fixme: then why list them with an id # in build log ? - rez
    ADMP( "^3revert: ^7you can only revert direct player actions, "
      "indicated by ^2* ^7in buildlog\n" );
    return qfalse;
  }

  G_admin_duration( ( level.time - log->time ) / 1000, time,
    sizeof( time ) );
  admin_log( arg );
  AP( va( "print \"^3revert: ^7%s^7 reverted %d %s over the past %s\n\"",
    ent ? ent->client->pers.netname : "console",
    level.buildId - id,
    ( level.buildId - id ) > 1 ? "changes" : "change",
    time ) );
  G_BuildLogRevert( id );
  return qtrue;
}

// TODO: Move this to header
typedef struct {
  char *flag;
  char *description;
} AdminFlagListEntry_t;

static AdminFlagListEntry_t adminFlagList[] =
{
  { ADMF_ACTIVITY,             "inactivity rules do not apply" },
  { ADMF_ADMINCHAT,            "can see and use admin chat" },
  { ADMF_CAN_PERM_BAN,         "can permanently ban players" },
  { ADMF_CAN_IP_BAN,           "can ban players with CIDR notation" },
  { ADMF_FORCETEAMCHANGE,      "team balance rules do not apply" },
  { ADMF_INCOGNITO,            "does not show as admin in !listplayers" },
  { ADMF_IMMUNITY,             "cannot be vote kicked or muted" },
  { ADMF_IMMUTABLE,            "admin commands cannot be used on them" },
  { ADMF_NOCENSORFLOOD,        "no flood protection" },
  { ADMF_NO_VOTE_LIMIT,        "vote limitations do not apply" },
  { ADMF_ADMINCHAT,            "can see and send /a admin messages" },
  { ADMF_SPEC_ALLCHAT,         "can see team chat as spectator" },
  { ADMF_UNACCOUNTABLE,        "does not need to specify reason for kick/ban" },
};

static int adminNumFlags = sizeof(adminFlagList) / sizeof(adminFlagList[ 0 ]);

qboolean G_admin_listflags( gentity_t * ent )
{
  int i, j;
  int count = 0;

  ADMBP_begin();

  ADMBP( "^3\nAbility flags:^7\n\n" );

  for( i = 0; i < adminNumFlags; i++ )
  {
    //ADMBP( va( "  %s%-20s ^7%s\n",
    ADMBP( va( "  %-20s ^7%s\n",
      adminFlagList[ i ].flag,
      adminFlagList[ i ].description ) );
  }

  ADMBP( "\n^3Command flags:^7\n\n" );

  for( i = 0; i < adminNumCmds; i++ )
  {
    ADMBP( va( "  ^5%-20s^7", g_admin_cmds[ i ].flag ) );
    for( j = 0; j < adminNumCmds; j++ )
    {
      if( !strcmp( g_admin_cmds[ j ].flag, g_admin_cmds[ i ].flag ) )
        ADMBP( va( " %s", g_admin_cmds[ j ].keyword ) );
    }
    ADMBP( "\n" );
    count++;
  }

  ADMBP( "^3\nPlayer Models:^7\n\n" );
  for ( i = 0; i < level.playerModelCount; i++ )
  {
      ADMBP( va( " ^2MODEL%-20s^7 %s\n", level.playerModel[i], level.playerModel[i] ) );
  }
  ADMBP( "\n" );

  ADMBP( va( "^3listflags: ^7listed %d abilities and %d command flags and %d\n",
    adminNumFlags, count, level.playerModelCount ) );

  ADMBP_end();

  return qtrue;
}

static int SortFlags( const void *pa, const void *pb )
{
  char *a = (char *)pa;
  char *b = (char *)pb;

  if( *a == '-' || *a == '+' )
    a++;
  if( *b == '-' || *b == '+' )
    b++;
  return strcmp(a, b);
}


#define MAX_USER_FLAGS 200
const char *G_admin_user_flag( char *oldflags, char *flag, qboolean add, qboolean clear,
                               char *newflags, int size )
{
  char *token, *token_p;
  char *key;
  char head_flags[ MAX_USER_FLAGS ][ MAX_ADMIN_FLAG_LEN ];
  char tail_flags[ MAX_USER_FLAGS ][ MAX_ADMIN_FLAG_LEN ];
  char allflag[ MAX_ADMIN_FLAG_LEN ];
  char newflag[ MAX_ADMIN_FLAG_LEN ];
  int head_count = 0;
  int tail_count = 0;
  qboolean flagset = qfalse;
  int i;

  if( !flag[ 0 ] )
  {
    return "invalid admin flag";
  }

  allflag[ 0 ] = '\0';
  token_p = oldflags;
  while( *( token = COM_Parse( &token_p ) ) )
  {
    key = token;
    if( *key == '-' || *key == '+' )
      key++;

    if( !strcmp( key, flag ) )
    {
      if( flagset )
        continue;
      flagset = qtrue;
      if( clear )
      {
        // clearing ALLFlAGS will result in clearing any following flags
        if( !strcmp( key, ADMF_ALLFLAGS ) )
          break;
        else
          continue;
      }
      Com_sprintf( newflag, sizeof( newflag ), "%s%s",
        ( add ? "+" : "-" ), key );
    }
    else
    {
      Q_strncpyz( newflag, token, sizeof( newflag ) );
    }

    if( !strcmp( key, ADMF_ALLFLAGS ) )
    {
      if( !allflag[ 0 ] )
        Q_strncpyz( allflag, newflag, sizeof( allflag ) );
      continue;
    }

    if( !allflag[ 0 ] )
    {
      if( head_count < MAX_USER_FLAGS )
      {
        Q_strncpyz( head_flags[ head_count ], newflag,
                    sizeof( head_flags[ head_count ] ) );
        head_count++;
      }
    }
    else
    {
      if( tail_count < MAX_USER_FLAGS )
      {
        Q_strncpyz( tail_flags[ tail_count ], newflag,
                    sizeof( tail_flags[ tail_count ] ) );
        tail_count++;
      }
    }
  }

  if( !flagset && !clear )
  {
    if( !strcmp( flag, ADMF_ALLFLAGS ) )
    {
      Com_sprintf( allflag, sizeof( allflag ), "%s%s",
        ( add ) ? "" : "-", ADMF_ALLFLAGS );
    }
    else if( !allflag[ 0 ] )
    {
      if( head_count < MAX_USER_FLAGS )
      {
        Com_sprintf( head_flags[ head_count ], sizeof( head_flags[ head_count ] ),
          "%s%s", ( add ) ? "" : "-", flag );
        head_count++;
      }
    }
    else
    {
      if( tail_count < MAX_USER_FLAGS )
      {
        Com_sprintf( tail_flags[ tail_count ], sizeof( tail_flags[ tail_count ] ),
          "%s%s", ( add ) ? "+" : "-", flag );
        tail_count++;
      }
    }
  }

  qsort( head_flags, head_count, sizeof( head_flags[ 0 ] ), SortFlags );
  qsort( tail_flags, tail_count, sizeof( tail_flags[ 0 ] ), SortFlags );

  // rebuild flags
  newflags[ 0 ] = '\0';
  for( i = 0; i < head_count; i++ )
  {
    Q_strcat( newflags, size,
              va( "%s%s", ( i ) ? " ": "", head_flags[ i ] ) );
  }
  if( allflag[ 0 ] )
  {
    Q_strcat( newflags, size,
      va( "%s%s", ( newflags[ 0 ] ) ? " ": "", allflag ) );

    for( i = 0; i < tail_count; i++ )
    {
      Q_strcat( newflags, size,
                va( " %s", tail_flags[ i ] ) );
    }
  }

  return NULL;
}

qboolean G_admin_flag( gentity_t *ent )
{
  char cmd[ MAX_NAME_LENGTH ] = {""};
  char testname[ MAX_NAME_LENGTH ] = {""};
  char name[ MAX_NAME_LENGTH ] = {""};
  char flag[ MAX_ADMIN_FLAG_LEN ] = {""};
  char *flagptr = NULL;
  const char *result;
  qboolean add = qtrue;
  qboolean clear = qfalse;
  g_admin_admin_t *a = NULL;
  g_admin_level_t *l = NULL;
  g_admin_level_t *d = G_admin_level( 0 );

  gentity_t *vic = NULL;
  int i, na;

  Cmd_ArgvBuffer( 0, cmd, sizeof(cmd) );
  if( Cmd_Argc() < 2 )
  {
    ADMP(va("^7usage: %s [name|slot|*level] [^3+^6|^3-^7][flag]\n", cmd));
    return qfalse;
  }

  Cmd_ArgvBuffer( 1, testname, sizeof(testname) );
  Cmd_ArgvBuffer( 2, flag, sizeof(flag) );

  if( testname[ 0 ] == '*' )
  {
    int id;
    if( ent )
    {
      ADMP(va("^3%s: ^7only console can change admin level flags\n", cmd));
      return qfalse;
    }

    id = atoi( testname + 1 );
    if( !(l = G_admin_level(id)) )
    {
      ADMP(va("^3%s: ^7level is not defined\n", cmd));
      return qfalse;
    }
  }
  else
  {
    for( na = 0, a = g_admin_admins; a; na++, a = a->next );

    for( i = 0; testname[ i ] && isdigit( testname[ i ] ); i++ );
    if( !testname[ i ] )
    {
      int id = atoi( testname );
      if( id < MAX_CLIENTS )
      {
        vic = &g_entities[ id ];
        if( !vic || !vic->client || vic->client->pers.connected == CON_DISCONNECTED )
        {
          ADMP( va("^3%s: ^7no player connected in slot %d\n", cmd, id) );
          return qfalse;
        }
      }
      else if( id < na + MAX_CLIENTS )
      {
        for( i = 0, a = g_admin_admins; i < id - MAX_CLIENTS; i++, a = a->next );
      }
      else
      {
        ADMP( va("^3%s: ^7%s not in range 1-%d\n", cmd, testname, na + MAX_CLIENTS - 1) );
        return qfalse;
      }
    }
    else
    {
      G_SanitiseString( testname, name, sizeof( name ) );
    }

    if( vic )
    {
      a = vic->client->pers.admin;
    }
    else if( !a )
    {
      g_admin_admin_t *wa;
      int             matches = 0;

      for( wa = g_admin_admins; wa && matches < 2; wa = wa->next )
      {
        G_SanitiseString( wa->name, testname, sizeof( testname ) );
        if( strstr( testname, name ) )
        {
          a = wa;
          matches++;
        }
      }

      for( i = 0; i < level.maxclients && matches < 2; i++ )
      {
        if( level.clients[ i ].pers.connected == CON_DISCONNECTED )
          continue;

        if( matches && level.clients[ i ].pers.admin &&
            level.clients[ i ].pers.admin == a )
        {
          vic = &g_entities[ i ];
          continue;
        }

        G_SanitiseString( level.clients[ i ].pers.netname, testname,
          sizeof( testname ) );
        if( strstr( testname, name ) )
        {
          vic = &g_entities[ i ];
          a = vic->client->pers.admin;
          matches++;
        }
      }

      if( matches == 0 )
      {
        ADMP( va("^3%s:^7 no match.  use listplayers or listadmins to "
          "find an appropriate number to use instead of name.\n", cmd) );
        return qfalse;
      }
      if( matches > 1 )
      {
        ADMP( va("^3%s:^7 more than one match.  Use the admin number "
          "instead:\n", cmd) );
        admin_listadmins( ent, 0, name );
        return qfalse;
      }
    }

    if( ent && !admin_higher_admin( ent->client->pers.admin, a ) )
    {
      ADMP( va("^3%s: ^7sorry, but your intended victim has a higher"
          " admin level than you\n", cmd) );
      return qfalse;
    }
  }

  if( Cmd_Argc() < 3 )
  {
    if( a )
    {
      g_admin_level_t *tmp = G_admin_level(a->level);
      if( tmp )
      {
        ADMP( va("^3%s:^7 flags for %s ^7%s are '^3%s%s^2%s'\n",
                  cmd,
                  tmp->name,
                  testname,
                  tmp->flags,
                  (*a->flags ? " " : ""),
                  a->flags) );
      }
      else
      {
        // XXX: Probably dead code path.
        ADMP( va("^3%s:^7 flags for player ^3%s^7 are '^3%s^7'\n",
                  cmd, testname, a->flags) );
      }
    }
    else if ( !l )
    {
       ADMP( va("^3%s:^7 flags for %s^7 %s are '^3%s^7'\n",
                  cmd, d->name, testname, d->flags) );
    }
    else
    {
      ADMP( va("^3%s:^7 flags for %s^7 (level %d) are '^3%s^7'\n",
                  cmd, l->name, l->level, l->flags) );
    }
    return qtrue;
  }

  Cmd_ArgvBuffer( 2, flag, sizeof(flag) );
  if( flag[ 0 ] == '-' || flag[ 0 ] == '+' )
  {
    add = ( flag[ 0 ] == '+' );
    flagptr = flag+1;
  } else
  {
    ADMP(va("^3%s:^7 use '+ to add a flag, or '-' to remove a flag\n", cmd));
    ADMP(va("^7usage: %s [name|slot|*level] [^3+^6|^3-^7][flag]\n", cmd));
    return qfalse;
  }

  if( ent && !Q_stricmp( ent->client->pers.guid, a->guid ) )
  {
    ADMP(va("^3%s:^7 you may not change your own flags (use rcon)\n", cmd));
    return qfalse;
  }

  if( flagptr[ 0 ] != '.' && !G_admin_permission( ent, flagptr ) )
  {
    ADMP( va("^3%s:^7 you can only change flags that you also have\n", cmd));
    return qfalse;
  }

  if( !Q_stricmp( cmd, "unflag" ) )
  {
    clear = qtrue;
  }

  if( a && a->level )
  {
    result = G_admin_user_flag( a->flags, flagptr, add, clear,
                                a->flags, sizeof(a->flags) );
  }
  else if (l)
  {
    //FIXME: Does not work on admin levels?
    result = G_admin_user_flag( l->flags, flagptr, add, clear,
                                l->flags, sizeof(l->flags) );
  }
  else
  {
    // cannot for non-registered players
    ADMP( va( "^3flag:^7 you can not set flags to %s's^7'", d->name) );
    return qfalse;
  }

  if( result )
  {
    ADMP( va( "^3flag: ^7an error occured setting flag '^3%s^7', %s\n",
      flagptr, result ) );
    return qfalse;
  }

  if( !Q_stricmp( cmd, "flag" ) )
  {
    ADMP( va( "^3%s: ^7%s^7 was %s admin flag '%s' by %s\n",
      cmd, testname,
      ( add ) ? "given" : "denied",
      flagptr,
      ( ent ) ? ent->client->pers.netname : "console" ) );
  }
  else
  {
    ADMP( va( "^3%s: ^7admin flag '%s' for %s^7 cleared by %s\n",
      cmd, flagptr, testname,
      ( ent ) ? ent->client->pers.netname : "console" ) );
  }

  if( !g_admin.string[ 0 ] )
    ADMP("^3flag: ^7WARNING g_admin not set, not saving admin record to a file\n");
  else
    admin_writeconfig();

  return qtrue;
}

qboolean G_admin_gamedir( gentity_t *ent )
{
    char dir[ MAX_QPATH ];
    char ext[ 24 ]; // arbitrary number, smaller or larger might be approriate.
    char filter[ MAX_QPATH ];
    int page = 0;

    if ( Cmd_Argc() < 4 )
    {
        ADMP("usage: ^3gamedir^7 \"<dir>\" \"<extension>\" \"<filter>\"\n");
        return qfalse;
    }

    Cmd_ArgvBuffer( 1, dir, sizeof(dir) );
    Cmd_ArgvBuffer( 2, ext, sizeof(ext) );
    Cmd_ArgvBuffer( 3, filter, sizeof(filter) );

    if ( Cmd_Argc() >= 5 )
    {
      char lp[ 8 ];
      Cmd_ArgvBuffer( 4, lp, sizeof( lp ) );
      page = atoi( lp );

      if( page < 0 )
        page = 0;
    }

    {
      char fileList[ 16*1024 ] = {""};
      char *filePtr;
      int  nFiles;
      int  fileLen = 0;
      int  i, j = 0;

      ADMBP_begin();

      nFiles = FS_GetFilteredFiles(dir, ext, filter,
              fileList, sizeof(fileList));
      filePtr = fileList;
      for( i = 0; i < nFiles; i++, filePtr += fileLen + 1 )
      {
        fileLen = strlen( filePtr );

        if ( i < page )
          continue;

        ADMBP( va( "%d - %s\n", i, filePtr ) );
        if ( j++ > 32 )
          break;
      }

      ADMBP( va( "^3gamedir^7: %d-%d of %d matches listed.\n", page, page+32, nFiles) );
      ADMBP_end();
    }

    return qtrue;
}


/*
================
 G_admin_print

 This function facilitates the ADMP define.  ADMP() is similar to CP except
 that it prints the message to the server console if ent is not defined.
================
*/
void G_admin_print( gentity_t *ent, char *m )
{
  if( ent )
    SV_GameSendServerCommand( ent - level.gentities, va( "print \"%s\"", m ) );
  else
  {
    char m2[ MAX_STRING_CHARS ];
    if( !Cvar_VariableIntegerValue( "com_ansiColor" ) )
    {
      G_DecolorString( m, m2, sizeof( m2 ) );
      Com_Printf( m2 );
    }
    else
      Com_Printf( m );
  }
}

void G_admin_buffer_begin( void )
{
  g_bfb[ 0 ] = '\0';
}

void G_admin_buffer_end( gentity_t *ent )
{
  ADMP( g_bfb );
}

void G_admin_buffer_print( gentity_t *ent, char *m )
{

  // 1022 - strlen("print 64 \"\"") - 1
#define MAX_CMDBUF 1009

  // Loop until m is consumed
  while( strlen( m ) + strlen( g_bfb ) >= MAX_CMDBUF )
  {
    ADMP( g_bfb );
    g_bfb[ 0 ] = '\0';
    Q_strcat( g_bfb, MAX_CMDBUF, m );
    m += MIN( strlen( m ), MAX_CMDBUF );
  }
  Q_strcat( g_bfb, sizeof( g_bfb ), m );
}


void G_admin_cleanup( void )
{
  g_admin_level_t *l;
  g_admin_admin_t *a;
  g_admin_ban_t *b;
  g_admin_command_t *c;
  void *n;

  for( l = g_admin_levels; l; l = n )
  {
    n = l->next;
    BG_Free( l );
  }
  g_admin_levels = NULL;
  for( a = g_admin_admins; a; a = n )
  {
    n = a->next;
    BG_Free( a );
  }
  g_admin_admins = NULL;
  for( b = g_admin_bans; b; b = n )
  {
    n = b->next;
    BG_Free( b );
  }
  g_admin_bans = NULL;
  for( c = g_admin_commands; c; c = n )
  {
    n = c->next;
    BG_Free( c );
  }
  g_admin_commands = NULL;
}

void G_admin_scrim_status(gentity_t *ent ) {
  char *string;
  int i;

  ADMBP_begin();
  ADMBP("^3======================================================^7\n");
  ADMBP( va(
    "^3Scrim Status: ^7%s ^7vs. ^7%s^7\n",
    level.scrim.team[SCRIM_TEAM_1].name, level.scrim.team[SCRIM_TEAM_2].name) );
    ADMBP("^7------------------------------------------------------\n");

  if(level.scrim.scrim_completed) {
    if(level.scrim.scrim_forfeiter != SCRIM_TEAM_NONE) {
      ADMBP(
        va(
          "    ^7%s^7 forfeits the scrim.\n",
          level.scrim.team[level.scrim.scrim_forfeiter].name));
    }
    switch (level.scrim.scrim_winner) {
      case SCRIM_TEAM_1:
        string =
          va(
            "^5T^7h^6e ^7S^1c^7r^4i^7m ^5i^7s ^6w^7o^1n ^7b^4y ^7%s^7",
            level.scrim.team[SCRIM_TEAM_1].name);
        break;

      case SCRIM_TEAM_2:
        string = 
          va(
            "^7%s ^5W^7i^6n^7s ^1T^7h^4e ^7S^5c^7r^6i^7m",
            level.scrim.team[SCRIM_TEAM_2].name);
        break;

      default:
        string = "^1T^7h^4e ^7S^5c^7r^6i^7m ^1I^7s ^4A ^7D^5r^7a^6w";
        break;
    }

    ADMBP(va("    %s^7\n", string));
    ADMBP(
      "^7*^5*^7*^6*^7*^1*^7*^4*^7*^5*^7*^6*^7*^1*^7*^4*^7*^5*^7*^6*^7*^1*^7*^4*^7*^5*^7*^6*^7*^1*^7*^4*^7*^5*^7*^6*^7*^1*^7*^4*^7*^5*^7*^6*^7*^1*\n");
  }

  switch (level.scrim.win_condition) {
    case SCRIM_WIN_CONDITION_DEFAULT:
      string = "Default, the first team to win 2 rounds in\n                    a row wins the scrim, round draws are ignored";
      break;

    case SCRIM_WIN_CONDITION_SHORT:
      string = "Short, out of exactly 2 rounds, the team\n                    that wins at least 1 round and loses no\n                    round wins the scrim";
  }

  ADMBP(va("    ^5Win Condistion: ^7%s^7\n", string));

  switch (level.scrim.mode) {
    case SCRIM_MODE_OFF:
      string = "Off";
      break;

    case SCRIM_MODE_SETUP:
      string = "Setup";
      break;

    case SCRIM_MODE_PAUSED:
      string = "Paused";
      break;

    case SCRIM_MODE_STARTED:
      string = "Started";
      break;
  }

  ADMBP(va("    ^5Scrim Mode: ^7%s^7\n",string));

  switch (level.scrim.sudden_death_mode) {
    case SDMODE_BP:
      string = "Build Point";
      break;

    case SDMODE_NO_BUILD:
      string = "No Build";
      break;

    case SDMODE_SELECTIVE:
      string = "Selective";
      break;

    case SDMODE_NO_DECON:
      string = "No Deconstruct";
      break;
  }

  ADMBP(va("    ^5Timed Income: ^7%s\n", level.scrim.timed_income ? "on" : "off"));

  ADMBP(va("    ^5Sudden Death Mode: ^7%s^7\n", string));

  ADMBP(va("    ^5Sudden Death Time: ^7%i^7\n", level.scrim.sudden_death_time));

  ADMBP(va("    ^5Time Limit: ^7%i^7\n", level.scrim.time_limit));

  ADMBP(va("    ^5Max Rounds: ^7%i^7\n", level.scrim.max_rounds));

  ADMBP(va("    ^5Rounds Completed: ^7%i^7\n", (level.scrim.rounds_completed)));

  if(level.scrim.rounds_completed > 0) {
    switch (level.scrim.previous_round_win) {
      case SCRIM_TEAM_1:
        string = va("won by team ^7%s", level.scrim.team[SCRIM_TEAM_1].name);
        break;

      case SCRIM_TEAM_2:
        string = va("won by team: ^7%s", level.scrim.team[SCRIM_TEAM_2].name);
        break;

      default:
        string = "a draw";
        break;
    }

    ADMBP(va("    ^5Previous round was %s^7\n", string));
  }

  for(i = 1; i < NUM_SCRIM_TEAMS; i++) {
    ADMBP(
        va(
        "    ^5This round's team selection for ^7%s^5 is:^7 %s^7\n",
        level.scrim.team[i].name,
        BG_Team(level.scrim.team[i].current_team)->humanName));

    ADMBP(
      va(
        "    ^5Stats for team ^7%s^5: ^7%i rounds won, %i rounds lost, %i rounds tied\n",
        level.scrim.team[i].name,
        level.scrim.team[i].wins,
        level.scrim.team[i].losses,
        level.scrim.team[i].draws));
  }

  ADMBP("^3======================================================^7\n");

  ADMBP_end();
}

qboolean admin_scrim_team_name_check(scrim_team_t team, char *name, char *err, int len) {
  int             i;
  char            testName[ MAX_NAME_LENGTH ] = {""};
  char            name2[ MAX_NAME_LENGTH ] = {""};

  G_SanitiseString(name, name2, sizeof(name2));

  if(!name[0] || !name[1]) {
    if(err && len > 0) {
      Q_strncpyz(err, "Scrim team names must contain at least 2 characters.", len);
    }
    return qfalse;
  }

  if(!strcmp(name2, "console")) {
    if(err && len > 0)
      Q_strncpyz(err, "The scrim team name 'console' is not allowed.", len);
    return qfalse;
  }

  G_DecolorString(name, testName, sizeof(testName));
  if(isdigit(testName[0])) {
    if(err && len > 0)
      Q_strncpyz( err, "Scrim team names cannot begin with numbers", len );
    return qfalse;
  }

  for(i = 0; i < NUM_SCRIM_TEAMS; i++) {
    if(i == SCRIM_TEAM_NONE) {
      continue;
    }

    // can rename ones self to the same name using different colors
    if(i == team) {
      continue;
    }

    G_SanitiseString(level.scrim.team[i].name, testName, sizeof(testName));
    if(!strcmp(name2, testName)) {
      if(err && len > 0) {
        Com_sprintf(err, len, "The scrim team name '%s^7' is already in use by another team", name);
      }
      return qfalse;
    }
  }

  return qtrue;
}

qboolean G_admin_scrim_roster(gentity_t *ent) {
  char                *registeredname;
  char                rank_name[MAX_NAME_LENGTH];
  const int           rank_name_length = Q_PrintStrlen(BG_Rank(RANK_CAPTAIN)->name);
  char                namecleaned[MAX_NAME_LENGTH];
  char                name2cleaned[MAX_NAME_LENGTH];
  scrim_team_t        scrim_team;
  int                 roster_index;
  scrim_team_member_t *member;

  ADMBP_begin( );
  ADMBP(va("^3roster:\n"));

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(scrim_team == SCRIM_TEAM_NONE) {
      continue;
    }

    ADMBP(va("\n  ^5Scrim Team: ^7%s^7\n", level.scrim.team[scrim_team].name));

    for(roster_index = 0; roster_index < 64; roster_index++) {
      member = &level.scrim_team_rosters[scrim_team].members[roster_index];

      if(!member->inuse) {
        continue;
      }

      if(
        !member->guidless &&
        level.scrim.team[scrim_team].has_captain &&
        !Q_stricmp(level.scrim.team[scrim_team].captain_guid, member->guid)) {
        Q_strncpyz(rank_name, BG_Rank(RANK_CAPTAIN)->name, sizeof(rank_name));
      } else {
        rank_name[0] = '\0';
      }

      registeredname = NULL;
      if(member->registered_name[0]) {
        G_SanitiseString(member->netname, namecleaned, sizeof(namecleaned));
        G_SanitiseString(
          member->registered_name, name2cleaned, sizeof(name2cleaned));
        if(Q_stricmp(namecleaned, name2cleaned)) {
          registeredname = member->registered_name;
        }
      }
      ADMBP(
        va(
          "    %3lu %*s^7 %s^7 %s%s%s\n",
          member->roster_id,
          rank_name_length,
          rank_name,
          member->netname,
          (registeredname) ? "(a.k.a. " : "",
          (registeredname) ? registeredname : "",
          (registeredname) ? S_COLOR_WHITE ")" : ""));
    }
  }

  ADMBP_end();
  return qtrue;
}

qboolean G_admin_scrim(gentity_t *ent) {
  g_admin_cmd_t *admincmd;
  char cmdStr[ MAX_ADMIN_CMD_LEN ] = "";
  char subcmd[MAX_TOKEN_CHARS] = "";
  char arg1[ MAX_TOKEN_CHARS ] = "";

  if(!g_allowScrims.integer) {
    ADMP("^3scrim: ^7g_allowScrims is set to 0 on this server\n" );
    return qfalse;
  }

  Cmd_ArgvBuffer(0, cmdStr, sizeof(cmdStr));
  admincmd = G_admin_cmd(cmdStr);

  if(Cmd_Argc() < 2) {
    if(IS_SCRIM) {
      G_admin_scrim_status(ent);
    }
    ADMP(
      va(
        "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
        admincmd->keyword, admincmd->keyword, admincmd->syntax));

    return qfalse;
  }

  Cmd_ArgvBuffer(1, subcmd, sizeof(subcmd));
  if(!Q_stricmp(subcmd, "on")) {
    int client_num;
    if( IS_SCRIM ) {
      ADMP("^3scrim: ^7scrim mode is already on\n");
      return qfalse;
    }

    if(!IS_WARMUP) {
      ADMP("^3scrim: ^7scrim mode can only be turned on during warmup\n");
      return qfalse;
    }

    if(g_cheats.integer) {
      ADMP("^3scrim: ^7dev mode must be off to turn on scrim mode\n");
      return qfalse;
    }

    //add the players on the current playing teams to the roster appropriately
    for(client_num = 0; client_num < MAX_CLIENTS; client_num++) {
      gclient_t *client = &level.clients[client_num];

      if(client->pers.connected == CON_DISCONNECTED) {
        continue;
      }

      if(client->pers.teamSelection == TEAM_NONE) {
        continue;
      }

      G_Scrim_Add_Player_To_Roster(
        client,
        BG_Scrim_Team_From_Playing_Team(
          &level.scrim, client->pers.teamSelection), NULL, 0);
    }

    level.scrim.mode = SCRIM_MODE_SETUP;
    G_Scrim_Send_Status( );
    G_admin_scrim_status(ent);
    AP(
      va(
        "print \"^3scrim: ^7%s ^7has switched scrim mode on for setup\n\"",
        ent ? ent->client->pers.netname : "console"));
    ADMP("^3scrim: ^7once the scrim starts, the scrim setup settings can't be changed\n");
    ADMP("^3scrim: ^7to start the scrim execute: scrim start\n");
    return qtrue;
  } else if(!Q_stricmp(subcmd, "off")) {
    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7scrim mode is already off\n");
      return qfalse;
    }

    if(!IS_WARMUP) {
      ADMP("^3scrim: ^7scrim mode can only be turned off during warmup\n");
      return qfalse;
    }

    G_Scrim_Reset_Settings();
    level.scrim.mode = SCRIM_MODE_OFF;
    G_Scrim_Send_Status( );
    AP( va( "print \"^3scrim: ^7%s ^7has switched scrim mode off\n\"",
            ent ? ent->client->pers.netname : "console" ) );
    return qtrue;
  } else if(!Q_stricmp(subcmd, "restore_defaults")) {
    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    G_Scrim_Reset_Settings();
    level.scrim.mode = SCRIM_MODE_SETUP;
    G_Scrim_Send_Status( );
    G_admin_scrim_status(ent);
    AP( va( "print \"^3scrim: ^7%s ^7has restored scrim defaults\n\"",
            ent ? ent->client->pers.netname : "console" ) );
    return qtrue;
  } else if(!Q_stricmp(subcmd, "start")) {
    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode == SCRIM_MODE_STARTED) {
      ADMP("^3scrim: ^7the scrim has already started\n");
      return qfalse;
    }

    if(level.scrim.mode == SCRIM_MODE_SETUP) {
      level.scrim.mode = SCRIM_MODE_STARTED;
      G_LevelRestart(qfalse);
      G_Scrim_Broadcast_Status( );
      AP( va( "print \"^3scrim: ^7%s ^7has started the scrim\n\"",
              ent ? ent->client->pers.netname : "console" ) );
    } else {
      level.scrim.mode = SCRIM_MODE_STARTED;
      AP( va( "print \"^3scrim: ^7%s ^7has resumed the scrim\n\"",
              ent ? ent->client->pers.netname : "console" ) );
    }

    G_Scrim_Send_Status( );

    return qtrue;
  } else if(!Q_stricmp(arg1, "pause")) {
    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode == SCRIM_MODE_PAUSED) {
      level.scrim.mode = SCRIM_MODE_STARTED;
      G_Scrim_Send_Status( );
      AP( va( "print \"^3scrim: ^7%s ^7has resumed the scrim\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      return qtrue;
    }

    if(level.scrim.mode != SCRIM_MODE_STARTED) {
      ADMP("^3scrim: ^7only scrims that have started can be paused\n");
      return qfalse;
    }

    if(!IS_WARMUP) {
      ADMP("^3scrim: ^7must be warmup to pause scrims\n");
      return qfalse;
    }

    level.scrim.mode = SCRIM_MODE_PAUSED;
    G_Scrim_Send_Status( );
    AP( va( "print \"^3scrim: ^7%s ^7has paused the scrim\n\"",
            ent ? ent->client->pers.netname : "console" ) );
    AP( va( "print \"^3scrim: ^7to resume the scrim execute: scrim start\n\"" ) );
    return qtrue;
  } else if(!Q_stricmp(subcmd, "win")) {
    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    if(Cmd_Argc() != 3) {
      ADMP("^3scrim: ^7scrim win is used to set the win condition for the scrim\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "win [^3default^6|^3short^7]"));
      return qfalse;
    }

    Cmd_ArgvBuffer( 2, arg1, sizeof( arg1 ) );

    if(!Q_stricmp(arg1, "default")) {
      if(level.scrim.win_condition == SCRIM_WIN_CONDITION_DEFAULT) {
        ADMP("^3scrim: ^7the win condition is already set to default\n");
        return qfalse;
      }

      AP( va( "print \"^3scrim: ^7%s ^7has set the win condition to default, the first team to win 2 rounds in a row wins the scrim, round draws are ignored\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      level.scrim.max_rounds = 5;
      level.scrim.win_condition = SCRIM_WIN_CONDITION_DEFAULT;
      G_Scrim_Send_Status( );
      return qtrue;
    } else if(!Q_stricmp(arg1, "short")) {
      if(level.scrim.win_condition == SCRIM_WIN_CONDITION_SHORT) {
        ADMP("^3scrim: ^7the win condition is already set to short\n");
        return qfalse;
      }

      AP( va( "print \"^3scrim: ^7%s ^7has set the win condition to short, out of exactly 2 rounds, the team that wins at least 1 round and loses no round wins the scrim\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      level.scrim.max_rounds = 2;
      level.scrim.win_condition = SCRIM_WIN_CONDITION_SHORT;
      G_Scrim_Send_Status( );
      return qtrue;
    } else {
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "win [^3extensive^6|^3short^7]"));
      return qfalse;
    }
  } else if(!Q_stricmp(subcmd, "timed_income")) {
    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    if(!g_freeFundPeriod.integer) {
      ADMP("^3scrim: ^7g_freeFundPeriod is set to 0 on this server, not allowing timed income to change for scrims\n");
      return qfalse;
    }

    if(Cmd_Argc() != 3) {
      ADMP("^3scrim: ^7scrim timed_income is used to set the win condition for the scrim\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "timed_income [^3on^6|^3off^7]"));
      return qfalse;
    }

    Cmd_ArgvBuffer( 2, arg1, sizeof( arg1 ) );

    if(!Q_stricmp(arg1, "on")) {
      if(level.scrim.timed_income) {
        ADMP("^3scrim: ^7timed income is already on\n");
        return qfalse;
      }

      AP( va( "print \"^3scrim: ^7%s ^7has turned on timed income for the scrim\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      level.scrim.timed_income = qtrue;
      G_Scrim_Send_Status( );
      return qtrue;
    } else if(!Q_stricmp(arg1, "off")) {
      if(!level.scrim.timed_income) {
        ADMP("^3scrim: ^7timed income is already off\n");
        return qfalse;
      }

      AP( va( "print \"^3scrim: ^7%s ^7has turned off timed income for the scrim\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      level.scrim.timed_income = qfalse;
      G_Scrim_Send_Status( );
      return qtrue;
    } else {
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "timed_income [^3on^6|^3off^7]"));
      return qfalse;
    }
  } else if(!Q_stricmp(subcmd, "map")) {
    char map[ MAX_QPATH ];
    char layout[ MAX_QPATH ] = { "" };

    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    if(Cmd_Argc( ) < 3) {
      ADMP("^3scrim: ^7scrim map is used to set the map and optionally set the layout\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "map [^3map name^7] (^5layout name^7)"));
      return qfalse;
    }

    Cmd_ArgvBuffer(2, map, sizeof(map));

    if(!G_MapExists(map)) {
      ADMP(va("^3scrim: ^7invalid map name '%s'\n", map));
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "map [^3map name^7] (^5layout name^7)"));
      return qfalse;
    }

    if(Cmd_Argc() > 3) {
      Cmd_ArgvBuffer(3, layout, sizeof(layout));
      if( G_LayoutExists(map, layout)) {
        Cvar_SetSafe("g_nextLayout", layout);
      }
      else {
        ADMP(va("^scrim: ^7invalid layout name '%s'\n", layout));
        ADMP(
          va(
            "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
            admincmd->keyword, admincmd->keyword, "map [^3map name^7] (^5layout name^7)"));
        return qfalse;
      }
    }
    admin_log(map);
    admin_log(layout);

    G_MapConfigs(map);
    Cbuf_ExecuteText(EXEC_APPEND, va( "%smap \"%s\"\n",
                               (g_cheats.integer ? "dev" : "" ), map));
    level.restarted = qtrue;
    AP(va("print \"^3scrim: ^7map '%s' started by %s^7 %s\n\"", map,
            (ent) ? ent->client->pers.netname : "console",
            (layout[ 0 ]) ? va( "(forcing layout '%s')", layout) : ""));
    Cvar_SetSafe("g_warmup", "1");
    SV_SetConfigstring(CS_WARMUP, va( "%d", IS_WARMUP));
    G_Scrim_Send_Status( );

    return qtrue;
  } else if(!Q_stricmp(subcmd, "putteam")) {
    int pid;
    char name[ MAX_NAME_LENGTH ], team[ sizeof( "spectators" ) ],
         err[ MAX_STRING_CHARS ];
    gentity_t *vic;
    team_t teamnum = TEAM_NONE;

    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(Cmd_Argc( ) < 4) {
      ADMP("^3scrim: ^7scrim putteam is used to move a player to a specific team\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "putteam [^3name^6|^3slot#^7] [^3h^6|^3a^6|^3s^7]"));
      return qfalse;
    }

    Cmd_ArgvBuffer(2, name, sizeof(name));
    Cmd_ArgvBuffer(3, team, sizeof(team));

    if((pid = G_ClientNumberFromString(name, err, sizeof(err))) == -1) {
      ADMP(va("^3scrim: ^7%s", err));
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "putteam [^3name^6|^3slot#^7] [^3h^6|^3a^6|^3s^7]"));
      return qfalse;
    }

    vic = &g_entities[pid];
    teamnum = G_TeamFromString(team);

    if(teamnum == NUM_TEAMS) {
      ADMP(va("^3scrim: ^7unknown team %s\n", team));
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "putteam [^3name^6|^3slot#^7] [^3h^6|^3a^6|^3s^7]"));
      return qfalse;
    }

    if(vic->client->pers.teamSelection == teamnum) {
      ADMP( 
        va(
          "^3scrim: ^7%s^7 is already on the %s team\n",
          vic->client->pers.netname, BG_Team(teamnum)->name2));
      return qfalse;
    }

    if(teamnum == TEAM_NONE) {
      G_Scrim_Remove_Player_From_Rosters(vic->client->pers.namelog, qfalse);
    } else {
      if(
        !G_Scrim_Add_Player_To_Roster(
          vic->client,
          BG_Scrim_Team_From_Playing_Team(&level.scrim, teamnum), err, sizeof(err))) {
        ADMP(va("^3scrim: ^7%s", err));
        ADMP(
          va(
            "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
            admincmd->keyword, admincmd->keyword, "putteam [^3name^6|^3slot#^7] [^3h^6|^3a^6|^3s^7]"));
        return qfalse;
      }
    }

    admin_log(
      va(
        "%d (%s) \"%s" S_COLOR_WHITE "\"", pid, vic->client->pers.guid,
        vic->client->pers.netname));
    admin_log(
      va(
        "scrim team: %d",
        BG_Scrim_Team_From_Playing_Team(&level.scrim, teamnum)));

    if(teamnum != TEAM_NONE || vic->client->pers.teamSelection != TEAM_NONE) {
      G_ChangeTeam( vic, teamnum );
    }

    G_Scrim_Send_Status( );
    AP(
      va(
        "print \"^3scrim: ^7%s^7 put %s^7 on to the %s team\n\"",
        (ent) ? ent->client->pers.netname : "console",
        vic->client->pers.netname, BG_Team(teamnum)->name2));
    return qtrue;
  } else if(!Q_stricmp(subcmd, "sd_mode")) {
    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    if(Cmd_Argc( ) != 3) {
      ADMP("^3scrim: ^7scrim sd_mode is used to set the sudden death mode for the scrim, selective is the default mode\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "sd_mode [^3bp^6|^3nobuild^6|^3selective^6|^3nodecon^7]"));
      return qfalse;
    }

    Cmd_ArgvBuffer(2, arg1, sizeof(arg1));

    if(!Q_stricmp(arg1, "bp")) {
      if(level.scrim.sudden_death_mode == SDMODE_BP) {
        ADMP("^3scrim: ^7sudden death mode is already set to bp\n");
        return qfalse;
      }

      level.scrim.sudden_death_mode = SDMODE_BP;
      G_Scrim_Send_Status( );
      AP( va( "print \"^3scrim: ^7%s ^7has set the sudden death mode to bp\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      return qtrue;
    } else if(!Q_stricmp(arg1, "nobuild")) {
      if(level.scrim.sudden_death_mode == SDMODE_NO_BUILD) {
        ADMP("^3scrim: ^sudden death mode is already set to nobuild\n");
        return qfalse;
      }

      level.scrim.sudden_death_mode = SDMODE_NO_BUILD;
      G_Scrim_Send_Status( );
      AP( va( "print \"^3scrim: ^7%s ^7has set the sudden death mode to nobuild\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      return qtrue;
    } else if(!Q_stricmp(arg1, "selective")) {
      if(level.scrim.sudden_death_mode == SDMODE_SELECTIVE) {
        ADMP("^3scrim: ^sudden death mode is already set to selective\n");
        return qfalse;
      }

      level.scrim.sudden_death_mode = SDMODE_SELECTIVE;
      G_Scrim_Send_Status( );
      AP( va( "print \"^3scrim: ^7%s ^7has set the sudden death mode to selective\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      return qtrue;
    } else if(!Q_stricmp(arg1, "nodecon")) {
      if(level.scrim.sudden_death_mode == SDMODE_NO_DECON) {
        ADMP("^3scrim: ^sudden death mode is already set to nodecon\n");
        return qfalse;
      }

      level.scrim.sudden_death_mode = SDMODE_NO_DECON;
      G_Scrim_Send_Status( );
      AP( va( "print \"^3scrim: ^7%s ^7has set the sudden death mode to nodecon\n\"",
              ent ? ent->client->pers.netname : "console" ) );
      return qtrue;
    } else {
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "sd_mode [^3bp^6|^3nobuild^6|^3selective^6|^3nodecon^7]"));
      return qfalse;
    }
  } else if(!Q_stricmp(subcmd, "sd_time")) {
    int sdtime;

    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    if(Cmd_Argc( ) != 3) {
      ADMP("^3scrim: ^7scrim sd_time is used to set the sudden death time in minutes for the scrim, setting to 0 disables sudden death for the scrim\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "sd_time [^3time in minutes^7]"));
      return qfalse;
    }

    Cmd_ArgvBuffer(2, arg1, sizeof(arg1));

    sdtime = atoi(arg1);
    if(sdtime < 0) {
      sdtime = 0;
    }

    if(sdtime == level.scrim.sudden_death_time) {
      ADMP(va("^3scrim: ^7the sudden death time is already set to %i\n", level.scrim.sudden_death_time));
      return qfalse;
    }

    level.scrim.sudden_death_time = sdtime;
    G_Scrim_Send_Status( );
    if(level.scrim.sudden_death_time) {
      AP(
        va(
          "print \"^3scrim: ^7%s ^7has set the sudden death time to %i minute%s for the scrim\n\"",
          ent ? ent->client->pers.netname : "console",
          level.scrim.sudden_death_time,
          level.scrim.sudden_death_time > 1 ? "s" : ""));
    } else {
      AP(
        va(
          "print \"^3scrim: ^7%s ^7has disabled sudden death for the scrim\n\"",
          ent ? ent->client->pers.netname : "console"));
    }

    return qtrue;
  } else if(!Q_stricmp(subcmd, "time_limit")) {
    int time_limit;

    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    if(Cmd_Argc( ) != 3) {
      ADMP("^3scrim: ^7scrim time_limit is used to set the time limit in minutes for the scrim, setting to 0 disables the time limit for the scrim\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "time_limit [^3time in minutes^7]"));
      return qfalse;
    }

    Cmd_ArgvBuffer(2, arg1, sizeof(arg1));

    time_limit = atoi(arg1);
    if(time_limit < 0) {
      time_limit = 0;
    }

    if(time_limit == level.scrim.time_limit) {
      ADMP(va("^3scrim: ^7the time limit is already set to %i\n", level.scrim.time_limit));
      return qfalse;
    }

    level.scrim.time_limit = time_limit;
    G_Scrim_Send_Status( );
    if(level.scrim.time_limit) {
      AP(
        va(
          "print \"^3scrim: ^7%s ^7has set the time limit to %i minute%s for the scrim\n\"",
          ent ? ent->client->pers.netname : "console",
          level.scrim.time_limit,
          level.scrim.time_limit > 1 ? "s" : ""));
    } else {
      AP(
        va(
          "print \"^3scrim: ^7%s ^7has disabled the time limit for the scrim\n\"",
          ent ? ent->client->pers.netname : "console"));
    }

    return qtrue;
  } else if(!Q_stricmp(subcmd, "team_name")) {
    char newname[ MAX_NAME_LENGTH ];
    char err[ MAX_STRING_CHARS ];
    team_t       playing_team;
    scrim_team_t scrim_team;

    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    if(Cmd_Argc( ) < 4) {
      ADMP("^3scrim: ^7scrim team_name is used to set the scrim team name for the scrim team currently on the given playing team.\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "team_name [^3a^6|^3h^7] [^3name of team^7]"));
      return qfalse;
    }

    Cmd_ArgvBuffer(2, arg1, sizeof(arg1));
    Q_strncpyz(newname, ConcatArgs(3), sizeof(newname));

    playing_team = G_TeamFromString(arg1);
    scrim_team = BG_Scrim_Team_From_Playing_Team(&level.scrim, playing_team);
    if(scrim_team == SCRIM_TEAM_NONE) {
      ADMP("^3scrim: ^7invalid playing team\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "team_name [^3a^6|^3h^7] [^3name of team^7]"));
      return qfalse;
    }

    if(!admin_scrim_team_name_check( scrim_team, newname, err, sizeof(err))) {
      ADMP(va("^3scrim: ^7%s\n",err));
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "team_name [^3a^6|^3h^7] [^3name of team^7]"));

      return qfalse;
    }

    AP( va( "print \"^3scrim: ^7scrim team %s^7 has been renamed to %s^7 by %s\n\"",
            level.scrim.team[scrim_team].name,
            newname,
            ( ent ) ? ent->client->pers.netname : "console" ) );
    Q_strncpyz(
      level.scrim.team[scrim_team].name, newname,
      sizeof( level.scrim.team[scrim_team].name ) );
    G_Scrim_Send_Status( );
    return qtrue;
  } else if(!Q_stricmp(subcmd, "team_captain")) {
    char                captain_name[ MAX_NAME_LENGTH ];
    char                err[ MAX_STRING_CHARS ];
    scrim_team_t        scrim_team;
    scrim_team_member_t *member;
    qboolean            removed_captain;

    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(Cmd_Argc( ) < 3) {
      ADMP("^3scrim: ^7scrim team_captain is used to set the scrim team captain for the team of the player, to designate a player with some team management abilities. Executing this subcommand on an existing team captain will remove their captain rank.\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "team_captain [^3roster id^6|^3player name^7]"));
      return qfalse;
    }

    Cmd_ArgvBuffer(2, arg1, sizeof(arg1));
    Q_strncpyz(captain_name, ConcatArgs(2), sizeof(captain_name));

    if(!(member = G_Scrim_Roster_Member_From_String(captain_name, &scrim_team, err, sizeof(err)))) {
      ADMP(va("^3scrim: ^7%s", err));
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "team_captain [^3roster id^6|^3player name^7]"));
      return qfalse;
    }

    if(scrim_team == SCRIM_TEAM_NONE || scrim_team < 0 || scrim_team >= NUM_SCRIM_TEAMS) {
      ADMP("^3scrim: ^7team captains must be on a valid scrim team\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "team_captain [^3name of team captain^7]"));
      return qfalse;
    }

    if(!G_Scrim_Set_Team_Captain(scrim_team, member->roster_id, &removed_captain, err, sizeof(err))) {
      ADMP(va("^3scrim: ^7%s", err));
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "team_captain [^3name of team captain^7]"));
      return qfalse;
    }

    if(removed_captain) {
      AP(
        va(
          "print \"^sscrim: ^7%s^7 has demoted %s^7 from team captain for scrim team %s\n\"",
          (ent) ? ent->client->pers.netname : "console",
          member->netname, level.scrim.team[scrim_team].name));
    } else {
      AP(
        va(
          "print \"^sscrim: ^7%s^7 has promoted %s^7 to team captain for scrim team %s\n\"",
          (ent) ? ent->client->pers.netname : "console",
          member->netname, level.scrim.team[scrim_team].name));
    }
    return qtrue;
  } else if(!Q_stricmp(subcmd, "max_rounds")) {
    int max_rounds;

    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(level.scrim.mode != SCRIM_MODE_SETUP) {
      ADMP("^3scrim: ^7this subcommand can only be used during scrim setup\n");
      return qfalse;
    }

    if(Cmd_Argc( ) != 3) {
      ADMP("^3scrim: ^7scrim max_rounds is used to set the maximum rounds for the scrim.\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "time_limit [^3time in minutes^7]"));
      return qfalse;
    }

    if(level.scrim.win_condition == SCRIM_WIN_CONDITION_SHORT) {
      ADMP("^3scrim: ^7the max rounds are fixed at 2 for the short win condition\n");
      return qfalse;
    }

    Cmd_ArgvBuffer(2, arg1, sizeof(arg1));

    max_rounds = atoi(arg1);
    if(max_rounds < 2) {
      ADMP("^3scrim: ^7the max rounds must be at least 2\n");
      return qfalse;
    }

    if(max_rounds == level.scrim.max_rounds) {
      ADMP(va("^3scrim: ^7the max rounds is already set to %i\n", level.scrim.max_rounds));
      return qfalse;
    }

    level.scrim.max_rounds = max_rounds;
    AP(
      va(
        "print \"^3scrim: ^7%s ^7has set the max rounds to %i for the scrim\n\"",
        ent ? ent->client->pers.netname : "console",
        level.scrim.max_rounds));
    G_Scrim_Send_Status( );
    return qtrue;
  } else if(!Q_stricmp(subcmd, "roster")) {
    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    G_admin_scrim_roster(ent);
    return qtrue;
  } else if(!Q_stricmp(subcmd, "remove")) {
    scrim_team_t        scrim_team;
    scrim_team_member_t *member;
    char name[ MAX_NAME_LENGTH ], err[ MAX_STRING_CHARS ], netname[ MAX_NAME_LENGTH ];

    if(!IS_SCRIM) {
      ADMP("^3scrim: ^7this subcommand requires scrim mode to be on\n");
      return qfalse;
    }

    if(Cmd_Argc( ) < 3) {
      ADMP("^3scrim: ^7this command is used to remove a player from the roster, even if the player is disconnected. To look up the roster IDs execute: scrim roster\n");
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "remove [^3roster id^6|^3player name^7]"));
      return qfalse;
    }

    Q_strncpyz(name, ConcatArgs(2), sizeof(name));

    if(!(member = G_Scrim_Roster_Member_From_String(name, &scrim_team, err, sizeof(err)))) {
      ADMP(va("^3scrim: ^7%s", err));
      ADMP(
        va(
          "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
          admincmd->keyword, admincmd->keyword, "remove [^3roster id^6|^3player name^7]"));
      return qfalse;
    }

    Q_strncpyz(netname, member->netname, sizeof(netname));

    G_Scrim_Remove_Player_From_Rosters_By_ID(member->roster_id);

    G_Scrim_Send_Status( );
    AP(
      va(
        "print \"^3scrim: ^7%s^7 removed %s^7 from the roster\n\"",
        (ent) ? ent->client->pers.netname : "console",
        netname));
    return qtrue;
  } else {
    ADMP(
      va(
        "\n" S_COLOR_YELLOW "%s: " S_COLOR_WHITE "usage: %s %s\n",
        admincmd->keyword, admincmd->keyword, admincmd->syntax));
    return qfalse;
  }
}

qboolean G_admin_seen( gentity_t *ent )
{
  int        offset = 0;
  char       offsetstr[ 10 ];
  char       name[ MAX_NAME_LENGTH ];

  if( Cmd_Argc() < 2 )
  {
    ADMP( "^3seen: ^7usage: seen [name] [offset(optional]\n" );
    return qfalse;
  }
  Cmd_ArgvBuffer( 1, name, sizeof( name ) );
  if( Cmd_Argc() == 3 )
  {
    Cmd_ArgvBuffer( 2, offsetstr, sizeof( offsetstr ) );
    offset = atoi( offsetstr );
  }
  pack_start( level.database_data, DATABASE_DATA_MAX );
  pack_int( ( ent ? ent - g_entities : -1 ) );
  pack_text2( va( "%%%s%%", name ) );
  pack_int( 10 );
  pack_int( offset );
  if( sl_query( DB_SEEN, level.database_data, NULL ) != 0 )
  {
    ADMP( "^3seen: ^7Query failed\n" );
    return qfalse;
  }
  ADMP( "^3seen: ^7Done\n" );
  return qtrue;
}

qboolean G_admin_maplog( gentity_t *ent )
{
  int        offset = 0;
  char       offsetstr[ 10 ];

  if( Cmd_Argc() == 2 )
  {
    Cmd_ArgvBuffer( 1, offsetstr, sizeof( offsetstr ) );
    offset = atoi( offsetstr );
  }

  pack_start( level.database_data, DATABASE_DATA_MAX );
  pack_int( ( ent ? ent - g_entities : -1 ) );
  pack_int( -1 );
  pack_int( 10 );
  pack_int( offset );
  if( sl_query( DB_LAST_MAPS, level.database_data, NULL ) != 0 )
  {
    ADMP( "^3maplog: ^7Query failed\n" );
    return qfalse;
  }
  ADMP( "^3maplog: ^7Done\n" );
  return qtrue;
}
