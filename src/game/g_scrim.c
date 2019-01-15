/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2018 GrangerHubs

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
============
G_Scrim_Restore_Player_Rank

============
*/
void G_Scrim_Restore_Player_Rank(int client_num) {
  scrim_team_t scrim_team;
  gclient_t    *client;
  rank_t       old_rank;

  Com_Assert(
    client_num >= 0 && client_num < MAX_CLIENTS &&
    "G_Scrim_Restore_Player_Rank: client_num is invalid");

  client = &level.clients[client_num];

  old_rank = client->sess.rank;
  client->sess.rank = RANK_NONE;

  if(client->pers.connected == CON_DISCONNECTED) {
    return;
  }

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(!level.scrim.team[scrim_team].has_captain) {
      continue;
    }

    if(level.scrim.team[scrim_team].current_team != client->pers.teamSelection) {
      continue;
    }

    if(client->pers.guidless) {
      continue;
    }

    if(Q_stricmp(level.scrim.team[scrim_team].captain_guid, client->pers.guid )) {
      continue;
    }

    client->sess.rank = RANK_CAPTAIN;
    level.scrim.team[scrim_team].captain_num = client_num;

    if(old_rank != client->sess.rank) {
      //update ClientInfo
      ClientUserinfoChanged( client->ps.clientNum, qfalse );
      client->pers.infoChangeTime = level.time;
      CalculateRanks(qfalse);
    }

    return;
  }

  if(old_rank != client->sess.rank) {
    //update ClientInfo
    ClientUserinfoChanged( client->ps.clientNum, qfalse );
    client->pers.infoChangeTime = level.time;
    CalculateRanks(qfalse);
  }
}

/*
============
G_Scrim_Restore_Team_Captains

============
*/
static void G_Scrim_Restore_Team_Captains(void) {
  int client_num;
  scrim_team_t scrim_team;

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    level.scrim.team[scrim_team].captain_num = -1;
  }

  for(client_num = 0; client_num < MAX_CLIENTS; client_num++) {
    G_Scrim_Restore_Player_Rank(client_num);
  }
}



/*
============
G_Scrim_Set_Team_Captains

============
*/
qboolean G_Scrim_Set_Team_Captain(
  scrim_team_t scrim_team, const int roster_id, qboolean *removed_captain,
  char *err, int err_len) {
  scrim_team_t        scrim_team_temp;
  scrim_team_member_t *member = NULL;
  int                 i;

  //find the team member
  for(i = 0; i < 64; i++) {
    if(level.scrim_team_rosters[scrim_team].members[i].roster_id == roster_id) {
      member = &level.scrim_team_rosters[scrim_team].members[i];
      break;
    }
  }

  if(!member) {
    if(err && err_len > 0) {
      Q_strncpyz(
        err, 
        va(
          "player isn't on the team roster of scrim team %s^7",
          level.scrim.team[scrim_team].name), err_len );
    }
    return qfalse;
  }

  if(member->guidless) {
    if(err && err_len > 0) {
      Q_strncpyz(
        err, va("%s^7 doesn't have a GUID", member->netname), err_len );
    }
    return qfalse;
  }

  if(
    level.scrim.team[scrim_team].has_captain &&
    !Q_stricmp(level.scrim.team[scrim_team].captain_guid, member->guid )) {
    //the player is currently the captain, so remove the player's rank
    level.scrim.team[scrim_team].has_captain = qfalse;
    level.scrim.team[scrim_team].captain_num = -1;
    memset(
      level.scrim.team[scrim_team].captain_guid,
      0,
      sizeof(level.scrim.team[scrim_team].captain_guid));
    G_Scrim_Restore_Team_Captains( );

    for(i = 0; i < MAX_CLIENTS; i++) {
      gclient_t *client = &level.clients[i];

      if(client->pers.connected != CON_CONNECTED) {
        continue;
      }

      if(client->pers.guidless) {
        continue;
      }

      if(Q_stricmp(client->pers.guid, member->guid)) {
        continue;
      }
      ClientUserinfoChanged( i, qfalse );
    }

    G_Scrim_Send_Status( );
    *removed_captain = qtrue;
    return qtrue;
  }

  //remove the player as team captain of all other teams where applicable
  for(scrim_team_temp = 0; scrim_team_temp < NUM_SCRIM_TEAMS; scrim_team_temp++) {
    if(scrim_team_temp == scrim_team) {
      continue;
    }

    if(!level.scrim.team[scrim_team_temp].has_captain) {
      continue;
    }

    //check for matching GUID
    if(Q_stricmp(level.scrim.team[scrim_team_temp].captain_guid, member->guid )) {
      continue;
    }

    level.scrim.team[scrim_team_temp].has_captain = qfalse;
    level.scrim.team[scrim_team_temp].captain_num = -1;
    memset(
      level.scrim.team[scrim_team_temp].captain_guid,
      0,
      sizeof(level.scrim.team[scrim_team_temp].captain_guid));
  }

  //set the team captain
  level.scrim.team[scrim_team].has_captain = qtrue;
  strcpy(level.scrim.team[scrim_team].captain_guid, member->guid);
  G_Scrim_Restore_Team_Captains( );

  for(i = 0; i < MAX_CLIENTS; i++) {
    gclient_t *client = &level.clients[i];

    if(client->pers.connected != CON_CONNECTED) {
      continue;
    }

    if(client->pers.guidless) {
      continue;
    }

    if(Q_stricmp(client->pers.guid, member->guid)) {
      continue;
    }
    ClientUserinfoChanged( i, qfalse );
  }

  G_Scrim_Send_Status( );
  *removed_captain = qfalse;
  return qtrue;
}

/*
============
G_Scrim_Load

============
*/
void G_Scrim_Load(void) {
  pers_scrim_t pers_scrim;
  scrim_team_t scrim_team;

  SV_Scrim_Load(&pers_scrim);

  memset(&level.scrim, 0, sizeof(level.scrim));

  level.scrim.mode = pers_scrim.mode;
  level.scrim.win_condition = pers_scrim.win_condition;
  level.scrim.timed_income = pers_scrim.timed_income;
  level.scrim.sudden_death_mode = pers_scrim.sudden_death_mode;
  level.scrim.sudden_death_time = pers_scrim.sudden_death_time;
  level.scrim.time_limit = pers_scrim.time_limit;
  level.scrim.previous_round_win = pers_scrim.previous_round_win;
  level.scrim.rounds_completed = pers_scrim.rounds_completed;
  level.scrim.max_rounds = pers_scrim.max_rounds;

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    Q_strncpyz(
      level.scrim.team[scrim_team].name,
      pers_scrim.team[scrim_team].name,
      sizeof(level.scrim.team[scrim_team].name));

    Q_strncpyz(
      level.scrim.team[scrim_team].captain_guid,
      pers_scrim.team[scrim_team].captain_guid,
      sizeof(level.scrim.team[scrim_team].captain_guid));

    level.scrim.team[scrim_team].current_team =
      pers_scrim.team[scrim_team].current_team;
    level.scrim.team[scrim_team].has_captain =
        pers_scrim.team[scrim_team].has_captain;
    level.scrim.team[scrim_team].wins = pers_scrim.team[scrim_team].wins;
    level.scrim.team[scrim_team].losses = pers_scrim.team[scrim_team].losses;
    level.scrim.team[scrim_team].draws = pers_scrim.team[scrim_team].draws;

    level.scrim_team_rosters[scrim_team] = pers_scrim.team[scrim_team].roster;

    level.forfeit_when_team_is_empty[scrim_team] = level.time + 125000;
  }

  G_Scrim_Restore_Team_Captains();

  G_Scrim_Send_Status( );
}

/*
============
G_Scrim_Reset_Settings

============
*/
void G_Scrim_Reset_Settings(void) {
  SV_Scrim_Init();
  G_Scrim_Load();
}

/*
============
G_Scrim_Save

Saves the scrim information to cvars for map restarts
============
*/
void G_Scrim_Save(void) {
  pers_scrim_t pers_scrim;
  scrim_team_t scrim_team;

  pers_scrim.mode = level.scrim.mode;
  pers_scrim.win_condition = level.scrim.win_condition;
  pers_scrim.timed_income = level.scrim.timed_income;
  pers_scrim.sudden_death_mode = level.scrim.sudden_death_mode;
  pers_scrim.sudden_death_time = level.scrim.sudden_death_time;
  pers_scrim.time_limit = level.scrim.time_limit;
  pers_scrim.previous_round_win = level.scrim.previous_round_win;
  pers_scrim.rounds_completed = level.scrim.rounds_completed;
  pers_scrim.max_rounds = level.scrim.max_rounds;

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    Q_strncpyz(
      pers_scrim.team[scrim_team].name,
      level.scrim.team[scrim_team].name,
      sizeof(pers_scrim.team[scrim_team].name));

    Q_strncpyz(
      pers_scrim.team[scrim_team].captain_guid,
      level.scrim.team[scrim_team].captain_guid,
      sizeof(pers_scrim.team[scrim_team].captain_guid));

    pers_scrim.team[scrim_team].current_team = 
      level.scrim.team[scrim_team].current_team;
    pers_scrim.team[scrim_team].has_captain = 
      level.scrim.team[scrim_team].has_captain;
    pers_scrim.team[scrim_team].wins = level.scrim.team[scrim_team].wins;
    pers_scrim.team[scrim_team].losses = level.scrim.team[scrim_team].losses;
    pers_scrim.team[scrim_team].draws = level.scrim.team[scrim_team].draws;

    pers_scrim.team[scrim_team].roster = level.scrim_team_rosters[scrim_team];
  }

  SV_Scrim_Save(&pers_scrim);
}

/*
============
G_Scrim_Send_Status

============
*/
void G_Scrim_Send_Status(void) {
  scrim_team_t scrim_team;

  SV_SetConfigstring(
    CS_SCRIM,
    va(
      "%i %i %i %i %i %i %i %i %i %i %i %i %i",
      g_allowScrims.integer,
      level.scrim.mode,
      level.scrim.win_condition,
      level.scrim.scrim_completed ? 1 : 0,
      level.scrim.scrim_winner,
      level.scrim.scrim_forfeiter,
      level.scrim.timed_income,
      level.scrim.sudden_death_mode,
      level.scrim.sudden_death_time,
      level.scrim.time_limit,
      level.scrim.previous_round_win,
      level.scrim.rounds_completed,
      level.scrim.max_rounds));

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    SV_SetConfigstring(
      CS_SCRIM_TEAM_NAME + scrim_team, level.scrim.team[scrim_team].name);

    SV_SetConfigstring(
      CS_SCRIM_TEAM + scrim_team,
      va(
        "%i %i %i %i %i %i",
        level.scrim.team[scrim_team].current_team,
        level.scrim.team[scrim_team].has_captain ? 1 : 0,
        level.scrim.team[scrim_team].captain_num,
        level.scrim.team[scrim_team].wins,
        level.scrim.team[scrim_team].losses,
        level.scrim.team[scrim_team].draws));
  }
}

/*
============
G_Scrim_Broadcast_Status

============
*/
void G_Scrim_Broadcast_Status(void) {
  int slot;

  //broadcast to all connected clients
  for(slot = 0; slot < level.maxclients; slot++) {
    gentity_t *ent = &g_entities[slot];
    
    if(!ent->client) {
      continue;
    }

    if(ent->client->pers.connected != CON_CONNECTED) {
      continue;
    }

    G_admin_scrim_status(ent);
  }

  //send to the server console
  G_admin_scrim_status(NULL);
}

/*
============
G_Scrim_Check_Restart

============
*/
void G_Scrim_Check_Win_Conditions(void) {
  if(g_allowScrims.integer && (level.scrim.mode == SCRIM_MODE_STARTED)) {
    scrim_team_t previous_round_win = level.scrim.previous_round_win;
    qboolean round_drawn = qfalse;

    level.scrim.rounds_completed++;

    if(level.scrim.scrim_forfeiter == SCRIM_TEAM_1) {
      level.scrim.team[SCRIM_TEAM_2].wins++;
      level.scrim.team[SCRIM_TEAM_1].losses++;
      level.scrim.scrim_completed = qtrue;
      level.scrim.scrim_winner = SCRIM_TEAM_2;
    } else if(level.scrim.scrim_forfeiter == SCRIM_TEAM_2) {
      level.scrim.team[SCRIM_TEAM_1].wins++;
      level.scrim.team[SCRIM_TEAM_2].losses++;
      level.scrim.scrim_completed = qtrue;
      level.scrim.scrim_winner = SCRIM_TEAM_1;
    } else if(level.scrim.team[SCRIM_TEAM_1].current_team == level.lastWin) {
      level.scrim.team[SCRIM_TEAM_1].wins++;
      level.scrim.team[SCRIM_TEAM_2].losses++;

      if(level.scrim.rounds_completed > 0) {
        switch(level.scrim.win_condition) {
          case SCRIM_WIN_CONDITION_DEFAULT:
            if(level.scrim.previous_round_win == SCRIM_TEAM_1) {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_1;
            } else if(level.scrim.rounds_completed >= level.scrim.max_rounds) {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_NONE;
            }
            break;

          case SCRIM_WIN_CONDITION_SHORT:
            if(level.scrim.previous_round_win != SCRIM_TEAM_2) {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_1;
            } else {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_NONE;
            }
            break;
        }
      }

      previous_round_win = SCRIM_TEAM_1;
    } else if(level.scrim.team[SCRIM_TEAM_2].current_team == level.lastWin) {
      level.scrim.team[SCRIM_TEAM_2].wins++;
      level.scrim.team[SCRIM_TEAM_1].losses++;

      if(level.scrim.rounds_completed > 0) {
        switch(level.scrim.win_condition) {
          case SCRIM_WIN_CONDITION_DEFAULT:
            if(level.scrim.previous_round_win == SCRIM_TEAM_2) {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_2;
            } else if(level.scrim.rounds_completed >= level.scrim.max_rounds) {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_NONE;
            }
            break;

          case SCRIM_WIN_CONDITION_SHORT:
            if(level.scrim.previous_round_win != SCRIM_TEAM_1) {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_2;
            } else {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_NONE;
            }
            break;
        }
      }

      previous_round_win = SCRIM_TEAM_2;
    } else {
      level.scrim.team[SCRIM_TEAM_2].draws++;
      level.scrim.team[SCRIM_TEAM_1].draws++;
      round_drawn = qtrue;

      if(level.scrim.rounds_completed > 0) {
        switch(level.scrim.win_condition) {
          case SCRIM_WIN_CONDITION_DEFAULT:
            if(level.scrim.rounds_completed >= level.scrim.max_rounds) {
              level.scrim.scrim_completed = qtrue;
              level.scrim.scrim_winner = SCRIM_TEAM_NONE;
            }
            break;

          case SCRIM_WIN_CONDITION_SHORT:
            level.scrim.scrim_completed = qtrue;
            level.scrim.scrim_winner = level.scrim.previous_round_win;
            break;
        }
      }
    }

    G_Scrim_Send_Status( );
    G_Scrim_Broadcast_Status( );

    level.scrim.previous_round_win = previous_round_win;

    //if the scrim isn't complete, prepare for a restart
    if(!level.scrim.scrim_completed) {
      char      map[ MAX_CVAR_VALUE_STRING ];
      Cvar_SetSafe( "g_restartingFlags",
        va( "%i", ( g_restartingFlags.integer | RESTART_SCRIM ) ) );
      Cvar_Update( &g_restartingFlags );

      //only extensive scrim matches with a draw keep the same teams
      if(
        level.scrim.win_condition != SCRIM_WIN_CONDITION_DEFAULT ||
        !round_drawn) {
        level.scrim.swap_teams = qtrue;
      }

      Cvar_SetSafe( "g_lockTeamsAtStart", "1" );
      Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
      Cvar_SetSafe( "g_nextMap", map);
    }
  }
}

/*
============
G_Scrim_Check_Pause

============
*/
void G_Scrim_Check_Pause(void) {
  static int pause_time = -1;

  if(level.scrim.mode != SCRIM_MODE_TIMEOUT) {
    pause_time = -1;
    return;
  } else if(pause_time < 0) {
    pause_time = level.time;
  }

  if(level.time > (pause_time + (g_scrimMaxPauseTime.integer * 60 * 1000))) {
    AP(va("print \"^3scrim: ^7console has automatically resumed the scrim\n\""));
    level.scrim.mode = SCRIM_MODE_STARTED;
    pause_time = -1;
  }
}

/*
-------------------------------------------------------------------------------
Rosters
*/

/*
============
G_Scrim_Remove_Player_From_Rosters

============
*/
void G_Scrim_Remove_Player_From_Rosters(namelog_t *namelog, qboolean force_ip) {
  scrim_team_t scrim_team;
  int          roster_index;

  if(namelog->guidless || force_ip) {
    for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
      for(roster_index = 0; roster_index < 64; roster_index++) {
        int ip_index;
        qboolean ip_match = qfalse;

        if(!level.scrim_team_rosters[scrim_team].members[roster_index].inuse) {
          continue;
        }

        if(
          !level.scrim_team_rosters[scrim_team].members[roster_index].guidless &&
          !force_ip) {
          continue;
        }

        for(ip_index = 0; ip_index < MAX_NAMELOG_ADDRS; ip_index++) {
          if(G_AddressCompare(
            &namelog->ip[ip_index],
            &level.scrim_team_rosters[scrim_team].members[roster_index].ip)) {
            ip_match = qtrue;
            break;
          }
        }

        if(
          !ip_match) {
          if(
            force_ip &&
            !namelog->guidless &&
            !level.scrim_team_rosters[scrim_team].members[roster_index].guidless) {
            if(
              Q_stricmp(
                namelog->guid,
                &level.scrim_team_rosters[scrim_team].members[roster_index].guid)) {
              continue;
            }
          } else {
            continue;
          }
        }

        memset(
          &level.scrim_team_rosters[scrim_team].members[roster_index],
          0,
          sizeof(level.scrim_team_rosters[scrim_team].members[roster_index]));

        level.scrim_team_rosters[scrim_team].members[roster_index].inuse = qfalse;

        //remove from the playing team
        if(
          IS_SCRIM &&
          namelog->team != TEAM_NONE &&
          BG_Scrim_Team_From_Playing_Team(&level.scrim, namelog->team) == scrim_team) {
          if(namelog->slot > -1) {
            G_ChangeTeam(&g_entities[namelog->slot], TEAM_NONE);
          } else {
            namelog->team = TEAM_NONE;
          }
        }
      }
    }
  } else {
    for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
      for(roster_index = 0; roster_index < 64; roster_index++) {
        if(!level.scrim_team_rosters[scrim_team].members[roster_index].inuse) {
          continue;
        }

        if(
          Q_stricmp(
            namelog->guid,
            &level.scrim_team_rosters[scrim_team].members[roster_index].guid)) {
          continue;
        }

        memset(
          &level.scrim_team_rosters[scrim_team].members[roster_index],
          0,
          sizeof(level.scrim_team_rosters[scrim_team].members[roster_index]));

        level.scrim_team_rosters[scrim_team].members[roster_index].inuse = qfalse;

        //remove from the playing team
        if(
          IS_SCRIM &&
          namelog->team != TEAM_NONE &&
          BG_Scrim_Team_From_Playing_Team(&level.scrim, namelog->team) == scrim_team) {
          if(namelog->slot > -1) {
            G_ChangeTeam(&g_entities[namelog->slot], TEAM_NONE);
          } else {
            namelog->team = TEAM_NONE;
          }
        }
      }
    }
  }
}

/*
============
G_Scrim_Remove_Player_From_Rosters_By_ID

============
*/
void G_Scrim_Remove_Player_From_Rosters_By_ID(size_t roster_id) {
  scrim_team_t scrim_team;
  int          roster_index;
  int          clientNum;

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    for(roster_index = 0; roster_index < 64; roster_index++) {
      if(!level.scrim_team_rosters[scrim_team].members[roster_index].inuse) {
        continue;
      }

      if(roster_id != level.scrim_team_rosters[scrim_team].members[roster_index].roster_id) {
        continue;
      }

      //remove from the playing team
      for(clientNum = 0; clientNum < MAX_CLIENTS; clientNum++) {
        gclient_t *client = &level.clients[clientNum];
        if(client->pers.connected == CON_DISCONNECTED) {
          continue;
        }

        if(
          level.scrim_team_rosters[scrim_team].members[roster_index].guidless &&
          client->pers.namelog->guidless) {
          int ip_index;
          qboolean ip_match = qfalse;

          for(ip_index = 0; ip_index < MAX_NAMELOG_ADDRS; ip_index++) {
            if(G_AddressCompare(
              &client->pers.namelog->ip[ip_index],
              &level.scrim_team_rosters[scrim_team].members[roster_index].ip)) {
              ip_match = qtrue;
              break;
            }
          }

          if(
            !ip_match) {
            continue;
          }
        } else if(
          Q_stricmp(
            client->pers.guid,
            &level.scrim_team_rosters[scrim_team].members[roster_index].guid)) {
          continue;
        }

        G_ChangeTeam(&g_entities[clientNum], TEAM_NONE);
      }

      memset(
        &level.scrim_team_rosters[scrim_team].members[roster_index],
        0,
        sizeof(level.scrim_team_rosters[scrim_team].members[roster_index]));

      level.scrim_team_rosters[scrim_team].members[roster_index].inuse = qfalse;

      return;
    }
  }
}

/*
============
G_Scrim_Roster_Member_Is_Client

============
*/
static qboolean G_Scrim_Roster_Member_Is_Client(
  gclient_t *client, const scrim_team_member_t member) {
  Com_Assert(client && "G_Scrim_Roster_Member_Is_Client: client is NULL");

  if(client->pers.connected == CON_DISCONNECTED) {
    return qfalse;
  }

  if(!member.inuse) {
    return qfalse;
  }

  if(client->pers.guidless) {
    if(!member.guidless) {
      return qfalse;
    }

    if(
      !G_AddressCompare(&client->pers.ip, &member.ip)) {
      return qfalse;
    }
  } else {
    if(member.guidless) {
      return qfalse;
    }

    if(
      Q_stricmp(client->pers.guid, member.guid)) {
      return qfalse;
    }
  }

  return qtrue;
}

/*
============
G_Scrim_Roster_Member_Matches_Namelog

============
*/
static qboolean G_Scrim_Roster_Member_Matches_Namelog(
  const namelog_t *namelog, const scrim_team_member_t member) {
  Com_Assert(namelog && "G_Scrim_Roster_Member_Is_Client: namelog is NULL");

  if(!member.inuse) {
    return qfalse;
  }

  if(namelog->guidless) {
    int ip_index;
    qboolean address_matched = qfalse;

    if(!member.guidless) {
      return qfalse;
    }

    for(ip_index = 0; ip_index < MAX_NAMELOG_ADDRS; ip_index++) {
      if(G_AddressCompare(&namelog->ip[ip_index], &member.ip)) {
        address_matched = qtrue;
        break;
      }
    }

    if(!address_matched) {
      return qfalse;
    }
  } else {
    if(member.guidless) {
      return qfalse;
    }

    if(
      Q_stricmp(namelog->guid, member.guid)) {
      return qfalse;
    }
  }

  return qtrue;
}



/*
============
G_Scrim_Find_Player_In_Rosters

============
*/
scrim_team_t G_Scrim_Find_Player_In_Rosters(gclient_t *client, int *roster_index) {
  scrim_team_t scrim_team;

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(scrim_team == SCRIM_TEAM_NONE) {
      continue;
    }

    for(*roster_index = 0; *roster_index < 64; (*roster_index)++) {
      if(
        G_Scrim_Roster_Member_Is_Client(
          client, level.scrim_team_rosters[scrim_team].members[*roster_index])) {
        return scrim_team;
      }
    }
  }

  *roster_index = -1;

  return SCRIM_TEAM_NONE;
}

/*
============
G_Scrim_Find_Namelog_In_Rosters

============
*/
scrim_team_t G_Scrim_Find_Namelog_In_Rosters(namelog_t *namelog, int *roster_index) {
  scrim_team_t scrim_team;

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(scrim_team == SCRIM_TEAM_NONE) {
      continue;
    }

    for(*roster_index = 0; *roster_index < 64; (*roster_index)++) {
      if(
        G_Scrim_Roster_Member_Matches_Namelog(
          namelog, level.scrim_team_rosters[scrim_team].members[*roster_index])) {
        return scrim_team;
      }
    }
  }

  *roster_index = -1;

  return SCRIM_TEAM_NONE;
}

/*
============
G_Scrim_Add_Player_To_Roster

============
*/
qboolean G_Scrim_Add_Player_To_Roster(
  gclient_t *client, scrim_team_t scrim_team, char *err, int err_len) {
  scrim_team_member_t *free_roster_slot = NULL;
  int          roster_index;
  scrim_team_t prev_team;
  size_t       prev_id;
  int          prev_index;

  Com_Assert(client && "G_Scrim_Add_Player_To_Roster: client is NULL");

  if(client->pers.connected == CON_DISCONNECTED) {
    if(err && err_len > 0) {
      Q_strncpyz(
        err, "player is not currently connected", err_len );
    }
    return qfalse;
  }

  if(scrim_team == SCRIM_TEAM_NONE || scrim_team == NUM_SCRIM_TEAMS) {
    if(err && err_len > 0) {
      Q_strncpyz(
        err, "not a valid scrim team roster", err_len );
    }
    return qfalse;
  }

  //check if the player is already in the roster, and if not, check if there is a free slot
  for(roster_index = 63; roster_index >= 0; roster_index--) {
    if(
      G_Scrim_Roster_Member_Is_Client(
        client, level.scrim_team_rosters[scrim_team].members[roster_index])) {
      if(err && err_len > 0) {
        Q_strncpyz(
          err,
          va(
            "player is already on the roster for scrim team %s",
            level.scrim.team[scrim_team].name),
            err_len );
      }
      return qtrue;
    }

    if(!level.scrim_team_rosters[scrim_team].members[roster_index].inuse) {
      free_roster_slot = &level.scrim_team_rosters[scrim_team].members[roster_index];
    }
  }

  if(!free_roster_slot) {
    if(err && err_len > 0) {
      Q_strncpyz(
        err,
        va(
          "roster for scrim team %s is full",
          level.scrim.team[scrim_team].name),
          err_len );
    }
    return qfalse;
  }

  //check for an existing roster id number
  prev_team = G_Scrim_Find_Player_In_Rosters(client, &prev_index);
  prev_id = level.scrim_team_rosters[prev_team].members[prev_index].roster_id;

  //remove the player from any other roster
  G_Scrim_Remove_Player_From_Rosters(client->pers.namelog, qfalse);

  //add the player to the roster
  free_roster_slot->inuse = qtrue;
  if(prev_team == SCRIM_TEAM_NONE) {
    free_roster_slot->roster_id = SV_Scrim_Get_New_Roster_ID();
  } else {
    free_roster_slot->roster_id = prev_id;
  }
  free_roster_slot->standby = qfalse;
  Q_strncpyz(
    free_roster_slot->netname, client->pers.netname, sizeof(free_roster_slot->netname));
  if(client->pers.admin) {
    Q_strncpyz(
      free_roster_slot->registered_name, client->pers.admin->name, sizeof(free_roster_slot->registered_name));
  } else {
    free_roster_slot->registered_name[0] = '\0';
  }
  free_roster_slot->guidless = client->pers.guidless;
  free_roster_slot->ip = client->pers.ip;
  Q_strncpyz(
    free_roster_slot->guid, client->pers.guid, sizeof(free_roster_slot->guid));
  return qtrue;
}

/*
============
G_Scrim_Add_Namelog_To_Roster

============
*/
qboolean G_Scrim_Add_Namelog_To_Roster(
  namelog_t *namelog, scrim_team_t scrim_team, char *err, int err_len) {
  scrim_team_member_t *free_roster_slot = NULL;
  g_admin_admin_t     *admin;
  int                 roster_index;
  scrim_team_t        prev_team;
  size_t              prev_id;
  int                 prev_index;

  Com_Assert(namelog && "G_Scrim_Add_Player_To_Roster: namelog is NULL");

  admin = G_admin_admin(namelog->guid);

  if(scrim_team == SCRIM_TEAM_NONE || scrim_team == NUM_SCRIM_TEAMS) {
    if(err && err_len > 0) {
      Q_strncpyz(
        err, "not a valid scrim team roster", err_len );
    }
    return qfalse;
  }

  //check if the player is already in the roster, and if not, check if there is a free slot
  for(roster_index = 63; roster_index >= 0; roster_index--) {
    if(
      G_Scrim_Roster_Member_Matches_Namelog(
        namelog, level.scrim_team_rosters[scrim_team].members[roster_index])) {
      if(err && err_len > 0) {
        Q_strncpyz(
          err,
          va(
            "player is already on the roster for scrim team %s",
            level.scrim.team[scrim_team].name),
            err_len );
      }
      return qtrue;
    }

    if(!level.scrim_team_rosters[scrim_team].members[roster_index].inuse) {
      free_roster_slot = &level.scrim_team_rosters[scrim_team].members[roster_index];
    }
  }

  if(!free_roster_slot) {
    if(err && err_len > 0) {
      Q_strncpyz(
        err,
        va(
          "roster for scrim team %s is full",
          level.scrim.team[scrim_team].name),
          err_len );
    }
    return qfalse;
  }

  //check for an existing roster id number
  prev_team = G_Scrim_Find_Namelog_In_Rosters(namelog, &prev_index);
  prev_id = level.scrim_team_rosters[prev_team].members[prev_index].roster_id;

  //remove the player from any other roster
  G_Scrim_Remove_Player_From_Rosters(namelog, qfalse);

  //add the player to the roster
  free_roster_slot->inuse = qtrue;
  if(prev_team == SCRIM_TEAM_NONE) {
    free_roster_slot->roster_id = SV_Scrim_Get_New_Roster_ID();
  } else {
    free_roster_slot->roster_id = prev_id;
  }
  free_roster_slot->standby = qfalse;
  Q_strncpyz(
    free_roster_slot->netname, namelog->name[namelog->nameOffset], sizeof(free_roster_slot->netname));
  if(admin) {
    Q_strncpyz(
      free_roster_slot->registered_name, admin->name, sizeof(free_roster_slot->registered_name));
  } else {
    free_roster_slot->registered_name[0] = '\0';
  }
  free_roster_slot->guidless = namelog->guidless;
  free_roster_slot->ip = namelog->slot < 0 ? namelog->ip[0] : level.clients[namelog->slot].pers.ip;
  Q_strncpyz(
    free_roster_slot->guid, namelog->guid, sizeof(free_roster_slot->guid));
  return qtrue;
}

/*
==================
G_Scrim_Roster_Member_From_String

Returns a scrim team member for either a roster id number or name string
Returns NULL and optionally sets err if invalid or not exactly 1 match
err will have a trailing \n if set
==================
*/
scrim_team_member_t *G_Scrim_Roster_Member_From_String(
  char *s, scrim_team_t *scrim_team, char *err, int len) {
  scrim_team_t         temp_scrim_team;
  int                  roster_index;
  int                  i, found = 0;
  scrim_team_member_t  *m = NULL;
  char                 s2[MAX_NAME_LENGTH];
  char                 n2[MAX_NAME_LENGTH];
  char                 *p = err;
  int                  l, l2 = len;

  if(!s[0]) {
    if(p) {
      Q_strncpyz(p, "no player name or roster id# provided\n", len);
    }

    return NULL;
  }

  // numeric values are just roster ids
  for(i = 0; s[i] && isdigit(s[i]); i++);
  if( !s[ i ] ) {
    i = atoi( s );

    if(i < 0 || i > SV_Scrim_Get_Last_Roster_ID()) {
      if( p ) {
        Q_strncpyz( p, "invalid roster id\n", len );
      }
      return NULL;
    }

    for(*scrim_team = 0; *scrim_team < NUM_SCRIM_TEAMS; (*scrim_team)++) {
      if(*scrim_team == SCRIM_TEAM_NONE) {
        continue;
      }
      for(roster_index = 0; roster_index < 64; roster_index++) {
        if(!level.scrim_team_rosters[*scrim_team].members[roster_index].inuse) {
          continue;
        }

        if(level.scrim_team_rosters[*scrim_team].members[roster_index].roster_id == i) {
          return &level.scrim_team_rosters[*scrim_team].members[roster_index];
        }
      }
    }

    if( p ) {
      Q_strncpyz( p, "no player in the rosters with that roster id\n", len );
    }
    return NULL;
  }

  G_SanitiseString( s, s2, sizeof( s2 ) );
  if( !s2[ 0 ] )
  {
    if( p ) {
      Q_strncpyz( p, "no player name provided\n", len );
    }

    return NULL;
  }

  if( p )
  {
    Q_strncpyz( p, "more than one player name matches. "
                "be more specific or use the slot #:\n", l2 );
    l = strlen( p );
    p += l;
    l2 -= l;
  }

  // check for a name match
  for(temp_scrim_team = 0; temp_scrim_team < NUM_SCRIM_TEAMS; temp_scrim_team++) {
    if(temp_scrim_team == SCRIM_TEAM_NONE) {
      continue;
    }
    for(roster_index = 0; roster_index < 64; roster_index++) {
      if(!level.scrim_team_rosters[temp_scrim_team].members[roster_index].inuse) {
        continue;
      }

      G_SanitiseString(
        level.scrim_team_rosters[temp_scrim_team].members[roster_index].netname,
        n2, sizeof(n2));

      if(!strcmp(n2, s2)) {
        return &level.scrim_team_rosters[temp_scrim_team].members[roster_index];
      }

      if( strstr( n2, s2 ) )
      {
        if( p )
        {
          l = Q_snprintf( p, l2, "%-2lu - %s^7\n",
            level.scrim_team_rosters[temp_scrim_team].members[roster_index].roster_id,
            level.scrim_team_rosters[temp_scrim_team].members[roster_index].netname);
          p += l;
          l2 -= l;
        }

        found++;
        m = &level.scrim_team_rosters[temp_scrim_team].members[roster_index];
        *scrim_team = temp_scrim_team;
      }
    }
  }

  if( found == 1 )
    return m;

  if( found == 0 && err )
    Q_strncpyz( err, "no connected player by that name or roster id\n", len );

  return NULL;
}

/*
============
G_Scrim_Player_Netname_Updated

============
*/
void G_Scrim_Player_Netname_Updated(gclient_t *client) {
  scrim_team_t scrim_team;
  scrim_team_member_t *scrim_member = NULL;
  int roster_index;

  Com_Assert(client && "G_Scrim_Player_Netname_Updated: client is NULL");

  if(client->pers.connected == CON_DISCONNECTED) {
    return;
  }

  scrim_team = G_Scrim_Find_Player_In_Rosters(client, &roster_index);

  if(scrim_team == SCRIM_TEAM_NONE) {
    return;
  }

  scrim_member = &level.scrim_team_rosters[scrim_team].members[roster_index];

  Q_strncpyz(
    scrim_member->netname, client->pers.netname, sizeof(scrim_member->netname));
}

/*
============
G_Scrim_Player_Set_Registered_Name

============
*/
void G_Scrim_Player_Set_Registered_Name(const char *guid, const char *name) {
  scrim_team_t scrim_team;
  int roster_index;

  if(!guid) {
    return;
  }

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(scrim_team == SCRIM_TEAM_NONE) {
      continue;
    }

    for(roster_index = 0; roster_index < 64; (roster_index)++) {
      if(!level.scrim_team_rosters[scrim_team].members[roster_index].inuse) {
        continue;
      }

      if(level.scrim_team_rosters[scrim_team].members[roster_index].guidless) {
        continue;
      }

      if(Q_stricmp(level.scrim_team_rosters[scrim_team].members[roster_index].guid, guid)) {
        continue;
      }

      if(!name || name[0] == '\0') {
        level.scrim_team_rosters[scrim_team].members[roster_index].registered_name[0] = '\0';
      } else {
        Q_strncpyz(
          level.scrim_team_rosters[scrim_team].members[roster_index].registered_name,
          name,
          sizeof(level.scrim_team_rosters[scrim_team].members[roster_index].registered_name));
      }
    }
  }
}

/*
============
G_Scrim_Player_Refresh_Registered_Names

============
*/
void G_Scrim_Player_Refresh_Registered_Names( void ) {
  g_admin_admin_t *admin = NULL;
  scrim_team_t    scrim_team;
  int             roster_index;

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(scrim_team == SCRIM_TEAM_NONE) {
      continue;
    }

    for(roster_index = 0; roster_index < 64; (roster_index)++) {
      if(!level.scrim_team_rosters[scrim_team].members[roster_index].inuse) {
        continue;
      }

      if(level.scrim_team_rosters[scrim_team].members[roster_index].guidless) {
        continue;
      }

      admin = G_admin_admin( level.scrim_team_rosters[scrim_team].members[roster_index].guid );

      if(!admin || !admin->level) {
        level.scrim_team_rosters[scrim_team].members[roster_index].registered_name[0] = '\0';
      } else {
        Q_strncpyz(
          level.scrim_team_rosters[scrim_team].members[roster_index].registered_name,
          admin->name,
          sizeof(level.scrim_team_rosters[scrim_team].members[roster_index].registered_name));
      }
    }
  }
}
