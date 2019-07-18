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

#include "g_local.h"

/*
================
G_TeamFromString

Return the team referenced by a string
================
*/
team_t G_TeamFromString( char *str )
{
  switch( tolower( *str ) )
  {
    case '0': case 's': return TEAM_NONE;
    case '1': case 'a': return TEAM_ALIENS;
    case '2': case 'h': return TEAM_HUMANS;
    default: return NUM_TEAMS;
  }
}

/*
================
G_TeamCommand

Broadcasts a command to only a specific team
================
*/
void G_TeamCommand( team_t team, char *cmd )
{
  int   i;

  for( i = 0 ; i < level.maxclients ; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_CONNECTED )
    {
      if( level.clients[ i ].pers.teamSelection == team ||
        ( level.clients[ i ].pers.teamSelection == TEAM_NONE &&
          G_admin_permission( &g_entities[ i ], ADMF_SPEC_ALLCHAT ) ) )
        SV_GameSendServerCommand( i, cmd );
    }
  }
}

/*
==============
OnSameTeam
==============
*/
qboolean OnSameTeam(const gentity_t *ent1, const gentity_t *ent2) {
  team_t ent1Team, ent2Team;

  if(!ent1 || !ent2) {
    return qtrue;
  }

  // entities are on the same team as themselves
  if(ent1 == ent2) {
    return qtrue;
  }

  // find the team of ent1
  if(ent1->client) {
    ent1Team = ent1->client->pers.teamSelection;
  } else {
    switch(ent1->s.eType) {
      case ET_BUILDABLE:
        ent1Team = BG_Buildable( ent1->s.modelindex )->team;
        break;

      default:
        return qfalse;
    }
  }

  // find the team of ent2
  if(ent2->client) {
    ent2Team = ent2->client->pers.teamSelection;
  } else {
    switch(ent2->s.eType) {
      case ET_BUILDABLE:
        ent2Team = BG_Buildable( ent2->s.modelindex )->team;
        break;

      default:
        return qfalse;
    }
  }

  return (ent1Team == ent2Team);
}

/*
==================
G_ClientListForTeam
==================
*/
static clientList_t G_ClientListForTeam( team_t team )
{
  int           i;
  clientList_t  clientList;

  Com_Memset( &clientList, 0, sizeof( clientList_t ) );

  for( i = 0; i < g_maxclients.integer; i++ )
  {
    gentity_t *ent = g_entities + i;
    if( ent->client->pers.connected != CON_CONNECTED )
      continue;

    if( ent->inuse && ( ent->client->ps.stats[ STAT_TEAM ] == team ) )
      Com_ClientListAdd( &clientList, ent->client->ps.clientNum );
  }

  return clientList;
}

/*
==================
G_UpdateTeamConfigStrings
==================
*/
void G_UpdateTeamConfigStrings( void )
{
  clientList_t alienTeam = G_ClientListForTeam( TEAM_ALIENS );
  clientList_t humanTeam = G_ClientListForTeam( TEAM_HUMANS );

  if( level.intermissiontime )
  {
    // No restrictions once the game has ended
    Com_Memset( &alienTeam, 0, sizeof( clientList_t ) );
    Com_Memset( &humanTeam, 0, sizeof( clientList_t ) );
  }

  SV_SetConfigstringRestrictions( CS_VOTE_TIME + TEAM_ALIENS,   &humanTeam );
  SV_SetConfigstringRestrictions( CS_VOTE_STRING + TEAM_ALIENS, &humanTeam );
  SV_SetConfigstringRestrictions( CS_VOTE_CAST + TEAM_ALIENS,    &humanTeam );
  SV_SetConfigstringRestrictions( CS_VOTE_ACTIVE + TEAM_ALIENS,     &humanTeam );

  SV_SetConfigstringRestrictions( CS_VOTE_TIME + TEAM_HUMANS,   &alienTeam );
  SV_SetConfigstringRestrictions( CS_VOTE_STRING + TEAM_HUMANS, &alienTeam );
  SV_SetConfigstringRestrictions( CS_VOTE_CAST + TEAM_HUMANS,    &alienTeam );
  SV_SetConfigstringRestrictions( CS_VOTE_ACTIVE + TEAM_HUMANS,     &alienTeam );

  SV_SetConfigstringRestrictions( CS_ALIEN_STAGES, &humanTeam );
  SV_SetConfigstringRestrictions( CS_HUMAN_STAGES, &alienTeam );
}

/*
==================
G_LeaveTeam
==================
*/
void G_LeaveTeam( gentity_t *self )
{
  team_t    team = self->client->pers.teamSelection;
  gentity_t *ent;
  int       i, u;

  self->r.svFlags &= ~SVF_CLIENTMASK_EXCLUSIVE;
  self->client->ps.persistant[ PERS_STATE ] &= ~PS_QUEUED;
  self->client->spawnReady = qfalse;
  BG_List_Remove_All(
    &level.spawn_queue[self->client->ps.stats[STAT_TEAM]], self->client);

  if( team == TEAM_NONE )
  {
    if( self->client->sess.spectatorState == SPECTATOR_FOLLOW )
      G_StopFollowing( self );
    return;
  }

  // stop any following clients
  G_StopFromFollowing( self );

  G_Vote( self, team, qfalse );
  self->suicideTime = 0;

  // reset player's ready flag to false
  self->client->sess.readyToPlay = qfalse;
  if( self->client->sess.readyToPlay ) {
    self->client->ps.stats[ STAT_FLAGS ] |= SFL_READY;
  } else {
    self->client->ps.stats[ STAT_FLAGS ] &= ~SFL_READY;
  }

  // reset any activation entities the player might be occupying
  if( self->client->ps.eFlags & EF_OCCUPYING )
    G_ResetOccupation( self->occupation.occupied, self );

  for( i = 0; i < level.num_entities; i++ )
  {
    ent = &g_entities[ i ];
    if( !ent->inuse )
      continue;

    if( ent->client && ent->client->pers.connected == CON_CONNECTED )
    {
      // cure poison
      if( ent->client->ps.stats[ STAT_STATE ] & SS_POISONED &&
          ent->client->lastPoisonClient == self )
        ent->client->ps.stats[ STAT_STATE ] &= ~SS_POISONED;
    }
    else if( ent->s.eType == ET_MISSILE && ent->r.ownerNum == self->s.number )
      G_FreeEntity( ent );

    ent->credits[ self->s.number ] = 0;
    for( u = 0; u < UP_NUM_UPGRADES; u++ )
      ent->creditsUpgrade[ u ][ self->s.number ] = 0;
  }

  if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, self->client->ps.stats) )
    G_RewardAttackers( self, UP_BATTLESUIT );
  G_RewardAttackers( self, UP_NONE );

  // cut all relevant zap beams
  G_ClearPlayerZapEffects( self );

  G_namelog_update_score( self->client );
}

/*
=================
G_ChangeTeam
=================
*/
void G_ChangeTeam( gentity_t *ent, team_t newTeam )
{
  team_t  oldTeam = ent->client->pers.teamSelection;

  if( oldTeam == newTeam )
    return;

  G_LeaveTeam( ent );
  if(IS_SCRIM && newTeam == TEAM_NONE) {
    G_Vote( ent, TEAM_NONE, qfalse );
  }
  ent->client->pers.teamChangeTime = level.time;
  ent->client->pers.teamSelection = newTeam;
  ent->client->pers.classSelection = PCL_NONE;
  ClientSpawn( ent, NULL, NULL, NULL, qtrue );

  if( oldTeam == TEAM_HUMANS && newTeam == TEAM_ALIENS )
  {
    // Convert from human to alien credits
    ent->client->pers.credit =
      (int)( ent->client->pers.credit *
             ALIEN_MAX_CREDITS / HUMAN_MAX_CREDITS + 0.5f );
  }
  else if( oldTeam == TEAM_ALIENS && newTeam == TEAM_HUMANS )
  {
    // Convert from alien to human credits
    ent->client->pers.credit =
      (int)( ent->client->pers.credit *
             HUMAN_MAX_CREDITS / ALIEN_MAX_CREDITS + 0.5f );
  }

  if(ent->client->pers.credit > SHRT_MAX) {
    ent->client->pers.credit = SHRT_MAX;
  }

  if( !g_cheats.integer )
  {
    if( ent->client->noclip )
    {
      ent->client->noclip = qfalse;
      ent->r.contents = ent->client->cliprcontents;
      G_BackupUnoccupyContents( ent );
    }
    ent->flags &= ~( FL_GODMODE | FL_NOTARGET );
  }

  //reset spectator lock
  ent->client->pers.namelog->specExpires = 0;

  // Copy credits to ps for the client
  ent->client->ps.persistant[ PERS_CREDIT ] = ent->client->pers.credit;

  ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );

  G_UpdateTeamConfigStrings( );

  G_LogPrintf( "ChangeTeam: %d %s: %s" S_COLOR_WHITE " switched teams\n",
    (int)( ent - g_entities ), BG_Team( newTeam )->name2, ent->client->pers.netname );

  if(IS_SCRIM) {
    G_Scrim_Restore_Player_Rank( ent->s.number );
  }

  G_namelog_update_score( ent->client );
  // refresh the teamoverlay
  ent->client->pers.teamInfo = 0;
  TeamplayInfoMessage( ent );
}

/*
===========
Team_GetLocation

Report a location for the player. Uses placed nearby target_location entities
============
*/
gentity_t *Team_GetLocation( gentity_t *ent )
{
  gentity_t   *eloc, *best;
  float       bestlen, len;

  best = NULL;
  bestlen = 3.0f * 8192.0f * 8192.0f;

  for( eloc = level.locationHead; eloc; eloc = eloc->nextTrain )
  {
    len = DistanceSquared( ent->r.currentOrigin, eloc->r.currentOrigin );

    if( len > bestlen )
      continue;

    if( !SV_inPVS( ent->r.currentOrigin, eloc->r.currentOrigin ) )
      continue;

    bestlen = len;
    best = eloc;
  }

  return best;
}


/*---------------------------------------------------------------------------*/

/*
==================
TeamplayInfoMessage

Format:
  clientNum location health weapon upgrade

==================
*/
void TeamplayInfoMessage( gentity_t *ent )
{
  char      entry[ 22 ],
            string[ ( MAX_CLIENTS - 1 ) * ( sizeof( entry ) - 1 ) + 1 ];
  int       i, j; 
  int       team, stringlength;
  gentity_t *player;
  gclient_t *cl;
  upgrade_t upgrade = UP_NONE;
  int       curWeaponClass = WP_NONE ; // sends weapon for humans, class for aliens
  char      *format;

  if( !g_allowTeamOverlay.integer )
     return;

  if( !ent->client->pers.teamInfo )
     return;

  if( ent->client->pers.teamSelection == TEAM_NONE )
  {
    if( ent->client->sess.spectatorState == SPECTATOR_FREE ||
        ent->client->sess.spectatorClient < 0 )
      return;
    team = g_entities[ ent->client->sess.spectatorClient ].client->
      pers.teamSelection;
  }
  else
    team = ent->client->pers.teamSelection;

  if( team == TEAM_ALIENS )
    format = " %i %i %i %i %i"; // aliens don't have upgrades
  else
    format = " %i %i %i %i %i %i";

  string[ 0 ] = '\0';
  stringlength = 0;

  for( i = 0; i < level.maxclients; i++)
  {
    player = g_entities + i ;
    cl = player->client;

    if( ent == player || !cl || team != cl->pers.teamSelection ||
        !player->inuse )
      continue;

    // only update if changed since last time
    if( cl->pers.infoChangeTime <= ent->client->pers.teamInfo  )
      continue;

    if( cl->sess.spectatorState != SPECTATOR_NOT )
    {
      curWeaponClass = WP_NONE;
      upgrade = UP_NONE;
    }
    else if ( cl->pers.teamSelection == TEAM_HUMANS )
    {
      curWeaponClass = cl->ps.weapon;

      if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, cl->ps.stats ) )
        upgrade = UP_BATTLESUIT;
      else if( BG_InventoryContainsUpgrade( UP_JETPACK, cl->ps.stats ) )
        upgrade = UP_JETPACK;
      else if( BG_InventoryContainsUpgrade( UP_BATTPACK, cl->ps.stats ) )
        upgrade = UP_BATTPACK;
      else if( BG_InventoryContainsUpgrade( UP_HELMET, cl->ps.stats ) )
        upgrade = UP_HELMET;
      else if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, cl->ps.stats ) )
        upgrade = UP_LIGHTARMOUR;
      else
        upgrade = UP_NONE;
    }
    else if( cl->pers.teamSelection == TEAM_ALIENS )
    {
      curWeaponClass = cl->ps.stats[ STAT_CLASS ];
      upgrade = UP_NONE;
    }

    Com_sprintf( entry, sizeof( entry ), format, i,
      cl->pers.location,
      BG_SU2HP( player->health ),
      cl->ps.persistant[ PERS_CREDIT ],
      curWeaponClass,
      upgrade );

    j = strlen( entry );

    // this should not happen if entry and string sizes are correct
    if( stringlength + j >= sizeof( string ) )
      break;

    strcpy( string + stringlength, entry );
    stringlength += j;
  }

  if( string[ 0 ] )
  {
    SV_GameSendServerCommand( ent - g_entities, va( "tinfo%s", string ) );
    ent->client->pers.teamInfo = level.time;
  }
}

void CheckTeamStatus( void )
{
  int i;
  gentity_t *loc, *ent;

  if( level.time - level.lastTeamLocationTime > TEAM_LOCATION_UPDATE_TIME )
  {
    level.lastTeamLocationTime = level.time;

    for( i = 0; i < g_maxclients.integer; i++ )
    {
      ent = g_entities + i;
      if( ent->client->pers.connected != CON_CONNECTED )
        continue;

      if( ent->inuse && ( ent->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ||
                          ent->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS ) )
      {

        loc = Team_GetLocation( ent );

        if( loc )
        {
          if( ent->client->pers.location != loc->s.generic1 )
          {
            ent->client->pers.infoChangeTime = level.time;
            ent->client->pers.location = loc->s.generic1;
          }
        }
        else if( ent->client->pers.location != 0 )
        {
          ent->client->pers.infoChangeTime = level.time;
          ent->client->pers.location = 0;
        }
      }
    }

    for( i = 0; i < g_maxclients.integer; i++ )
    {
      ent = g_entities + i;
      if( ent->client->pers.connected != CON_CONNECTED )
        continue;

      if( ent->inuse )
        TeamplayInfoMessage( ent );
    }
  }

  // Warn on imbalanced teams
  if( g_teamImbalanceWarnings.integer && !level.intermissiontime &&
      ( level.time - level.lastTeamImbalancedTime >
        ( g_teamImbalanceWarnings.integer * 1000 ) ) &&
      level.numTeamImbalanceWarnings < 3 && !level.restarted )
  {
    level.lastTeamImbalancedTime = level.time;
    if( level.numAlienSpawns > 0 && 
        level.numHumanClients - level.numAlienClients > 2 )
    {
      SV_GameSendServerCommand( -1, "print \"Teams are imbalanced. "
                                  "Humans have more players.\n\"");
      level.numTeamImbalanceWarnings++;
    }
    else if( level.numHumanSpawns > 0 && 
             level.numAlienClients - level.numHumanClients > 2 )
    {
      SV_GameSendServerCommand ( -1, "print \"Teams are imbalanced. "
                                   "Aliens have more players.\n\"");
      level.numTeamImbalanceWarnings++;
    }
    else
    {
      level.numTeamImbalanceWarnings = 0;
    }
  }
}
