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

// cg_consolecmds.c -- text commands typed in at the local console, or
// executed by a key binding


#include "cg_local.h"



/*
=================
CG_SizeUp_f

Keybinding command
=================
*/
static void CG_SizeUp_f( void )
{
  trap_Cvar_Set( "cg_viewsize", va( "%i", MIN( cg_viewsize.integer + 10, 100 ) ) );
}


/*
=================
CG_SizeDown_f

Keybinding command
=================
*/
static void CG_SizeDown_f( void )
{
  trap_Cvar_Set( "cg_viewsize", va( "%i", MAX( cg_viewsize.integer - 10, 30 ) ) );
}


/*
=============
CG_Viewpos_f

Debugging command to print the current position
=============
*/
static void CG_Viewpos_f( void )
{
  CG_Printf( "(%i %i %i) : %i\n", (int)cg.refdef.vieworg[ 0 ],
    (int)cg.refdef.vieworg[ 1 ], (int)cg.refdef.vieworg[ 2 ],
    (int)cg.refdefViewAngles[ YAW ] );
}

qboolean CG_RequestScores( void )
{
  if( cg.scoresRequestTime + 2000 < cg.time )
  {
    // the scores are more than two seconds out of data,
    // so request new ones
    cg.scoresRequestTime = cg.time;
    trap_SendClientCommand( "score\n" );

    return qtrue;
  }
  else
    return qfalse;
}

extern menuDef_t *menuScoreboard;

static void CG_scrollScoresDown_f( void )
{
  if( menuScoreboard && cg.scoreBoardShowing )
  {
    Menu_ScrollFeeder( menuScoreboard, FEEDER_ALIENTEAM_LIST, qtrue );
    Menu_ScrollFeeder( menuScoreboard, FEEDER_HUMANTEAM_LIST, qtrue );
  }
}


static void CG_scrollScoresUp_f( void )
{
  if( menuScoreboard && cg.scoreBoardShowing )
  {
    Menu_ScrollFeeder( menuScoreboard, FEEDER_ALIENTEAM_LIST, qfalse );
    Menu_ScrollFeeder( menuScoreboard, FEEDER_HUMANTEAM_LIST, qfalse );
  }
}

static void CG_ScoresDown_f( void )
{
  if( !cg.showScores )
  {
    Menu_SetFeederSelection( menuScoreboard, FEEDER_ALIENTEAM_LIST, 0, NULL );
    Menu_SetFeederSelection( menuScoreboard, FEEDER_HUMANTEAM_LIST, 0, NULL );
    cg.showScores = qtrue;
  }
  else
  {
    cg.showScores = qfalse;
    cg.numScores = 0;
  }
}

static void CG_ScoresUp_f( void )
{
  if( cg.showScores )
  {
    cg.showScores = qfalse;
    cg.scoreFadeTime = cg.time;
  }
}

static void CG_Boost_f(void) {
  trap_SendConsoleCommand( "+button8; wait; -button8;\n" );
}

void CG_ClientList_f( void )
{
  clientInfo_t *ci;
  int i;
  int count = 0;

  for( i = 0; i < MAX_CLIENTS; i++ ) 
  {
    ci = &cgs.clientinfo[ i ];
    if( !ci->infoValid ) 
      continue;

    switch( ci->team ) 
    {
      case TEAM_ALIENS:
        Com_Printf( "%2d " S_COLOR_RED "A   " S_COLOR_WHITE "%s\n", i,
          ci->name );
        break;

      case TEAM_HUMANS:
        Com_Printf( "%2d " S_COLOR_CYAN "H   " S_COLOR_WHITE "%s\n", i,
          ci->name );
        break;

      default:
      case TEAM_NONE:
      case NUM_TEAMS:
        Com_Printf( "%2d S   %s\n", i, ci->name );
        break;
    }

    count++;
  }

  Com_Printf( "Listed %2d clients\n", count );
}

static void CG_VoiceMenu_f( void )
{
  char cmd[sizeof("voicemenu3")];

  trap_Argv(0, cmd, sizeof(cmd));

  switch (cmd[9]) {
    default:
    case '\0':
      trap_Cvar_Set("ui_voicemenu", "1");
      break;
    case '2':
      trap_Cvar_Set("ui_voicemenu", "2");
      break;
    case '3':
      trap_Cvar_Set("ui_voicemenu", "3");
      break;
  };

  trap_SendConsoleCommand("menu tremulous_voicecmd\n");
}

static void CG_UIMenu_f( void )
{
  trap_SendConsoleCommand( va( "menu \"%s\"\n", CG_Argv( 1 ) ) );
}

/*
=====================
CG_SpawnMenuTeam
=====================
*/
static void CG_SpawnMenuTeam( void )
{
  CG_Menu( MN_TEAM, 0 );
}

/*
=====================
CG_SpawnMenuClass
=====================
*/
static void CG_SpawnMenuClass( void )
{
  if( cg.predictedPlayerState.misc[ MISC_HEALTH ] > 0 &&
    ( cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_ALIENS ) ) //Are we alive and on aliens?
  {
    CG_Menu( MN_A_INFEST, 0 );
    return;
  }
  if( cg.predictedPlayerState.misc[ MISC_HEALTH ] <= 0 ) //Are we dead?
  {
    if( cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_ALIENS )
    {
      CG_Menu( MN_A_CLASS, 0 );
    }
    else if( cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_HUMANS )
    {
      CG_Menu( MN_H_SPAWN, 0 );
    }
  }
}

/*
=====================
CG_SpawnMenuBuild
=====================
*/
static void CG_SpawnMenuBuild( void )
{
  if( cg.snap->ps.weapon == WP_ABUILD || cg.snap->ps.weapon == WP_ABUILD2 )
  {
    CG_Menu( MN_A_BUILD, 0 );
  }
  else if( cg.snap->ps.weapon == WP_HBUILD )
  {
    CG_Menu( MN_H_BUILD, 0 );
  }
}

static consoleCommand_t commands[ ] =
{
  { "+scores", CG_ScoresDown_f },
  { "-scores", CG_ScoresUp_f },
  { "boost", CG_Boost_f },
  { "cgame_memory", BG_MemoryInfo },
  { "clientlist", CG_ClientList_f },
  { "destroyTestPS", CG_DestroyTestPS_f },
  { "destroyTestTS", CG_DestroyTestTS_f },
  { "nextframe", CG_TestModelNextFrame_f },
  { "nextskin", CG_TestModelNextSkin_f },
  { "prevframe", CG_TestModelPrevFrame_f },
  { "prevskin", CG_TestModelPrevSkin_f },
  { "scoresDown", CG_scrollScoresDown_f },
  { "scoresUp", CG_scrollScoresUp_f },
  { "sizedown", CG_SizeDown_f },
  { "sizeup", CG_SizeUp_f },
  { "testgun", CG_TestGun_f },
  { "testmodel", CG_TestModel_f },
  { "testPS", CG_TestPS_f },
  { "testTS", CG_TestTS_f },
  { "ui_menu", CG_UIMenu_f },
  { "viewpos", CG_Viewpos_f },
  { "voicemenu", CG_VoiceMenu_f },
  { "voicemenu2", CG_VoiceMenu_f },
  { "voicemenu3", CG_VoiceMenu_f },
  { "weapnext", CG_NextWeapon_f },
  { "weapon", CG_Weapon_f },
  { "weapprev", CG_PrevWeapon_f },
  { "menu_team", CG_SpawnMenuTeam},
  { "menu_class", CG_SpawnMenuClass },
  { "menu_build", CG_SpawnMenuBuild },
};

/*
=================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_ConsoleCommand( void )
{
  consoleCommand_t *cmd;

  cmd = bsearch( CG_Argv( 0 ), commands,
    ARRAY_LEN( commands ), sizeof( commands[ 0 ] ),
    cmdcmp );

  if( !cmd )
    return qfalse;

  cmd->function( );
  return qtrue;
}

/*
==================
CG_CompletePlayMap_f
==================
*/
void CG_CompletePlayMap_f( int argNum ) {
#ifndef MODULE_INTERFACE_11
  if( argNum == 2 )
    trap_Field_CompleteList( cgs.playMapPoolJson );
#endif
}

/*
==================
CG_CompleteCallVote_f
==================
*/
void CG_CompleteCallVote_f( int argNum ) {
#ifndef MODULE_INTERFACE_11
  switch( argNum )
  {
    case 2:
      trap_Field_CompleteList(
	"["
	"\"allowbuild\","
	"\"cancel\","
	"\"denybuild\","
	"\"draw\","
	"\"extend\","
	"\"kick\","
	"\"map\","
	"\"map_restart\","
	"\"mute\","
	"\"nextmap\","
	"\"poll\","
	"\"sudden_death\","
	"\"unmute\" ]" );
      break;
    case 3:
      if( !Q_stricmp( CG_Argv( 1 ), "map" ) ||
	  !Q_stricmp( CG_Argv( 1 ), "nextmap" ) )
	// TODO: this should be the complete list of server maps, not
	// just the playpool
	trap_Field_CompleteList( cgs.playMapPoolJson );
      break;
  }
#endif
}

static consoleCommandCompletions_t commandCompletions[ ] =
{
  { "callvote", CG_CompleteCallVote_f },
  { "playmap", CG_CompletePlayMap_f },
};

/*
=================
CG_Console_CompleteArgument

Try to complete the client command line argument given in
argNum. Returns true if a completion function is found in CGAME,
otherwise client tries another completion method.

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_Console_CompleteArgument( int argNum )
{
  consoleCommandCompletions_t *cmd;

  // Skip command prefix character
  cmd = bsearch( CG_Argv( 0 ) + 1, commandCompletions,
    ARRAY_LEN( commandCompletions ), sizeof( commandCompletions[ 0 ] ),
    cmdcmp );

  if( !cmd )
    return qfalse;

  cmd->function( argNum );
  return qtrue;
}


/*
=================
CG_InitConsoleCommands

Let the client system know about all of our commands
so it can perform tab completion
=================
*/
void CG_InitConsoleCommands( void )
{
  int   i;

  for( i = 0; i < ARRAY_LEN( commands ); i++ )
    trap_AddCommand( commands[ i ].cmd );

  //
  // the game server will interpret these commands, which will be automatically
  // forwarded to the server after they are not recognized locally
  //
  trap_AddCommand( "kill" );
  trap_AddCommand( "ui_messagemode" );
  trap_AddCommand( "ui_messagemode2" );
  trap_AddCommand( "ui_messagemode3" );
  trap_AddCommand( "ui_messagemode4" );
  trap_AddCommand( "say" );
  trap_AddCommand( "say_team" );
  trap_AddCommand( "vsay" );
  trap_AddCommand( "vsay_team" );
  trap_AddCommand( "vsay_local" );
  trap_AddCommand( "m" );
  trap_AddCommand( "mt" );
  trap_AddCommand( "give" );
  trap_AddCommand( "god" );
  trap_AddCommand( "notarget" );
  trap_AddCommand( "noclip" );
  trap_AddCommand( "team" );
  trap_AddCommand( "follow" );
  trap_AddCommand( "setviewpos" );
  trap_AddCommand( "callvote" );
  trap_AddCommand( "vote" );
  trap_AddCommand( "callteamvote" );
  trap_AddCommand( "teamvote" );
  trap_AddCommand( "class" );
  trap_AddCommand( "build" );
  trap_AddCommand( "buy" );
  trap_AddCommand( "sell" );
  trap_AddCommand( "reload" );
  trap_AddCommand( "itemact" );
  trap_AddCommand( "itemdeact" );
  trap_AddCommand( "itemtoggle" );
  trap_AddCommand( "destroy" );
  trap_AddCommand( "deconstruct" );
  trap_AddCommand( "ignore" );
  trap_AddCommand( "unignore" );
  
}
