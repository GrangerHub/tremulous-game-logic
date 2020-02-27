/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2012-2018 ET:Legacy team <mail@etlegacy.com>
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

#include "server.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f( void ) {
	char		*cmd;
	char		*map;
	qboolean	killBots, cheat;
	char		expanded[MAX_QPATH];
	char		mapname[MAX_QPATH];
	int			a;
	int			i;

	map = Cmd_Argv(1);
	if ( !map ) {
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);
	if ( FS_ReadFile (expanded, NULL) == -1 ) {
		Com_Printf ("Can't find map %s\n", expanded);
		return;
	}

	cmd = Cmd_Argv(0);
	if ( !Q_stricmp( cmd, "devmap" ) ) {
		cheat = qtrue;
		killBots = qtrue;
	} else {
		cheat = qfalse;
		killBots = qfalse;
	}

	// stop any demos
	if (sv.demoState == DS_RECORDING) {
		SV_DemoStopRecord();
	}

	if (sv.demoState == DS_PLAYBACK) {
		SV_DemoStopPlayback();
	}

	// save the map name here cause on a map restart we reload the autogen.cfg
	// and thus nuke the arguments of the map command
	Q_strncpyz(mapname, map, sizeof(mapname));

	// consider only if a scrim is still in progress
	Cvar_SetSafe( 
		"g_restartingFlags",
		va("%i", (Cvar_VariableIntegerValue("g_restartingFlags") & RESTART_SCRIM)));

	// start up the map
	SV_SpawnServer( mapname, killBots );

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if ( cheat ) {
		Cvar_Set( "sv_cheats", "1" );
	} else {
		Cvar_Set( "sv_cheats", "0" );
	}

	// This forces the local master server IP address cache
	// to be updated on sending the next heartbeat
	for( a = 0; a < 3; ++a )
		for( i = 0; i < MAX_MASTER_SERVERS; i++ )
			sv_masters[ a ][ i ]->modified  = qtrue;

	Cvar_SetSafe( "g_restartingFlags", "0" );
}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
	int			i;
	client_t	*client;
	char		*denied;
	int			delay;

	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.serverId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( sv.restartTime ) {
		return;
	}

	if (Cmd_Argc() > 1 ) {
		delay = atoi( Cmd_Argv(1) );
	}
	else {
		delay = 0;
	}
	if( delay && !Cvar_VariableValue("g_doCountdown") ) {
		int    index = rand( ) % MAX_WARMUP_SOUNDS; // randomly select an end of warmup sound set

		sv.restartTime = sv.time + delay * 1000;
		SV_SetConfigstring( CS_COUNTDOWN, va( "%i %i", sv.restartTime, index ), qfalse );
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients change
	if ( sv_maxclients->modified ) {
		char	mapname[MAX_QPATH];

		Com_Printf( "variable change -- restarting.\n" );
		// restart the map the slow way
		Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );

		SV_SpawnServer( mapname, qfalse );
		Cvar_SetSafe( "g_restartingFlags", "0" );
		return;
	}

	// stop any demos
	if (sv.demoState == DS_RECORDING) {
		SV_DemoStopRecord();
	}

	if (sv.demoState == DS_PLAYBACK) {
		SV_DemoStopPlayback();
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid
	// TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// if a map_restart occurs while a client is changing maps, we need
	// to give them the correct time so that when they finish loading
	// they don't violate the backwards time check in cl_cgame.c
	for (i=0 ; i<sv_maxclients->integer ; i++) {
		if (svs.clients[i].state == CS_PRIMED) {
			svs.clients[i].oldServerTime = sv.restartTime;
		}
	}

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for (i = 0; i < 3; i++)
	{
		dll_G_RunFrame( sv.time );
		sv.time += 100;
		svs.time += 100;
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for (i=0 ; i<sv_maxclients->integer ; i++) {
		client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED) {
			continue;
		}

		// add the map_restart command
		SV_AddServerCommand( client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		denied = VM_ExplicitArgPtr( gvm, (intptr_t)dll_ClientConnect( i, qfalse ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( client, denied );
			Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i );
			continue;
		}

		if(client->state == CS_ACTIVE)
			SV_ClientEnterWorld(client, &client->lastUsercmd);
		else
		{
			// If we don't reset client->lastUsercmd and are restarting during map load,
			// the client will hang because we'll use the last Usercmd from the previous map,
			// which is wrong obviously.
			SV_ClientEnterWorld(client, NULL);
		}
	}

	// run another frame to allow things to look at all the players
	dll_G_RunFrame( sv.time );
	sv.time += 100;
	svs.time += 100;

	Cvar_SetSafe( "g_restartingFlags", "0" );

	// start recording a demo
	if(sv_autoDemo->integer) {
		SV_DemoAutoDemoRecord();
	}
}

//===============================================================

/**
 * @brief SV_Status_f
 */
static void SV_Status_f(void)
{
	int           i;
	client_t      *cl;
	playerState_t *ps;
	const char    *s;
	int           ping;
	unsigned int  maxNameLength;

	// make sure server is running
	if (!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	Com_Printf("cpu server utilization: %i %%\n"
	           "avg response time     : %i ms\n"
	           "server time           : %i\n"
	           "internal time         : %i\n"
	           "map                   : %s\n\n"
	           "num score ping name                                lastmsg address               qport rate  lastConnectTime\n"
	           "--- ----- ---- ----------------------------------- ------- --------------------- ----- ----- ---------------\n",
	           ( int ) svs.stats.cpu,
	           ( int ) svs.stats.avg,
	           svs.time,
	           Sys_Milliseconds(),
	           sv_mapname->string);

	for (i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++)
	{
		if(!cl->state && !cl->demoClient) {
			continue;
		}

		Com_Printf("%3i ", i);
		ps = SV_GameClientNum(i);
		Com_Printf("%5i ", ps->persistant[PERS_SCORE]);

		if(cl->demoClient) {
			// if it's a democlient, we show DEMO instead of the ping (which would be
			// 999 anyway - which is not the case in the scoreboard which show the
			// real ping that had the player because commands are replayed!)
			Com_Printf ("DEMO ");
		} else if(cl->state == CS_CONNECTED) {
			Com_Printf("CNCT ");
		} else if(cl->state == CS_ZOMBIE) {
			Com_Printf("ZMBI ");
		} else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf("%4i ", ping);
		}

		s = NET_AdrToString(cl->netchan.remoteAddress);

		// extend the name length by couting extra color characters to keep well formated output
		maxNameLength = sizeof(cl->name_ansi) + (strlen(cl->name_ansi) - Q_PrintStrlen(cl->name_ansi)) + 1;

		Com_Printf("%-*s %7i %-21s %5i %5i %i\n", maxNameLength, rc(cl->name_ansi), svs.time - cl->lastPacketTime, s, cl->netchan.qport, cl->rate, svs.time - cl->lastConnectTime);
	}

	Com_Printf("\n");
}

/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	Com_Printf ("Server info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
}


/*
===========
SV_Systeminfo_f

Examine the systeminfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	Com_Printf ("System info settings:\n");
	Info_Print ( Cvar_InfoString_Big( CVAR_SYSTEMINFO ) );
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}

/*
=================
SV_Demo_Record_f
=================
*/
static void SV_Demo_Record_f( void ) {
        // make sure server is running
        if (!com_sv_running->integer) {
                Com_Printf("Server is not running.\n");
                return;
        }

        if (Cmd_Argc() > 2) {
                Com_Printf("Usage: demo_record <demoname>\n");
                return;
        }

        if (sv.demoState != DS_NONE) {
                Com_Printf("A demo is already being recorded/played. Use demo_stop and retry.\n");
                return;
        }

        if (sv_maxclients->integer == MAX_CLIENTS) {
                Com_Printf("DEMO: ERROR: Too many client slots, reduce sv_maxclients and retry.\n");
                return;
        }

        if (Cmd_Argc() == 2)
                sprintf(sv.demoName, "svdemos/%s.%s%d", Cmd_Argv(1), SVDEMOEXT, PROTOCOL_VERSION);
        else {
                int     number;
                // scan for a free demo name
                for (number = 0 ; number >= 0 ; number++) {
                        Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%d.%s%d", number, SVDEMOEXT, PROTOCOL_VERSION);
                        if (!FS_FileExists(sv.demoName))
                                break;  // file doesn't exist
                }
                if (number < 0) {
                        Com_Printf("DEMO: ERROR: Couldn't generate a filename for the demo, try deleting some old ones.\n");
                        return;
                }
        }

        sv.demoFile = FS_FOpenFileWrite(sv.demoName);
        if (!sv.demoFile) {
                Com_Printf("DEMO: ERROR: Couldn't open %s for writing.\n", sv.demoName);
                return;
        }
        SV_DemoStartRecord();
}


/*
=================
SV_Demo_Play_f
=================
*/
static void SV_Demo_Play_f( void ) {
        char *arg;

        if (Cmd_Argc() != 2) {
                Com_Printf("Usage: demo_play <demoname>\n");
                return;
        }

        if (sv.demoState != DS_NONE && sv.demoState != DS_WAITINGPLAYBACK) {
                Com_Printf("A demo is already being recorded/played. Use demo_stop and retry.\n");
                return;
        }

        // check for an extension .svdm_?? (?? is protocol)
        arg = Cmd_Argv(1);
        if (!strcmp(arg + strlen(arg) - 6, va(".%s%d", SVDEMOEXT, PROTOCOL_VERSION)))
                Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%s", arg);
        else
                Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%s.%s%d", arg, SVDEMOEXT, PROTOCOL_VERSION);


        //FS_FileExists(sv.demoName);
	FS_FOpenFileRead(sv.demoName, &sv.demoFile, qtrue);
        if (!sv.demoFile) {
                Com_Printf("ERROR: Couldn't open %s for reading.\n", sv.demoName);
                return;
        }

        SV_DemoStartPlayback();
}


/*
=================
SV_Demo_Stop_f
=================
*/
static void SV_Demo_Stop_f( void ) {
        if (sv.demoState == DS_NONE) {
                Com_Printf("No demo is currently being recorded or played.\n");
                return;
        }

        // Close the demo file
        if (sv.demoState == DS_PLAYBACK || sv.demoState == DS_WAITINGPLAYBACK)
                SV_DemoStopPlayback();
        else
                SV_DemoStopRecord();
}


/*
====================
SV_CompleteDemoName
====================
*/
static void SV_CompleteDemoName( char *args, int argNum )
{
	if( argNum == 2 )
	{
		char demoExt[ 16 ];

		Com_sprintf( demoExt, sizeof( demoExt ), ".%s%d", SVDEMOEXT, PROTOCOL_VERSION );
		Field_CompleteFilename( "svdemos", demoExt, qtrue, qtrue );
	}
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "maps", "bsp", qtrue, qfalse );
	}
}

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("systeminfo", SV_Systeminfo_f);
	Cmd_AddCommand ("map_restart", SV_MapRestart_f);
	Cmd_AddCommand ("sectorlist", SV_SectorList_f);
	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
	Cmd_AddCommand ("devmap", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
	Cmd_AddCommand ("killserver", SV_KillServer_f);
	Cmd_AddCommand ("demo_record", SV_Demo_Record_f);
	Cmd_AddCommand ("demo_play", SV_Demo_Play_f);
	Cmd_SetCommandCompletionFunc( "demo_play", SV_CompleteDemoName );
	Cmd_AddCommand ("demo_stop", SV_Demo_Stop_f);
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("heartbeat");
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("systeminfo");
	Cmd_RemoveCommand ("map_restart");
	Cmd_RemoveCommand ("sectorlist");
#endif
}
