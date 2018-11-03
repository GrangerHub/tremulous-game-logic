/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2018 GrangerHub

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

damageRegion_t  g_damageRegions[ PCL_NUM_CLASSES ][ MAX_DAMAGE_REGIONS ];
int             g_numDamageRegions[ PCL_NUM_CLASSES ];

damageRegion_t  g_armourRegions[ UP_NUM_UPGRADES ][ MAX_DAMAGE_REGIONS ];
int             g_numArmourRegions[ UP_NUM_UPGRADES ];

/*
============
G_TakesDamage

Determines if an entity can be damaged
============
*/
qboolean G_TakesDamage( gentity_t *ent )
{
   return ent->takedamage;
}

/*
============
AddScore

Adds score to the client
============
*/
void AddScore( gentity_t *ent, int score )
{
  if( !ent->client )
    return;

  if( IS_WARMUP )
    return;

  // make alien and human scores equivalent
  if ( ent->client->pers.teamSelection == TEAM_ALIENS )
  {
    score = rint( ((float)score) / 2.0f );
  }

  // scale values down to fit the scoreboard better
  score = rint( ((float)score) / 50.0f );

  ent->client->ps.persistant[ PERS_SCORE ] += score;
  CalculateRanks( );
}

/*
==================
LookAtKiller
==================
*/
void LookAtKiller( gentity_t *self, gentity_t *inflictor, gentity_t *attacker )
{

  if ( attacker && attacker != self )
    self->client->ps.stats[ STAT_VIEWLOCK ] = attacker - g_entities;
  else if( inflictor && inflictor != self )
    self->client->ps.stats[ STAT_VIEWLOCK ] = inflictor - g_entities;
  else
    self->client->ps.stats[ STAT_VIEWLOCK ] = self - g_entities;
}

// these are just for logging, the client prints its own messages
char *modNames[ ] =
{
  "MOD_UNKNOWN",
  "MOD_SHOTGUN",
  "MOD_BLASTER",
  "MOD_PAINSAW",
  "MOD_MACHINEGUN",
  "MOD_CHAINGUN",
  "MOD_PRIFLE",
  "MOD_MDRIVER",
  "MOD_LASGUN",
  "MOD_LCANNON",
  "MOD_LCANNON_SPLASH",
  "MOD_FLAMER",
  "MOD_FLAMER_SPLASH",
  "MOD_GRENADE",
  "MOD_FRAGNADE",
  "MOD_GRENADE_LAUNCHER",
  "MOD_LIGHTNING",
  "MOD_LIGHTNING_EMP",
  "MOD_WATER",
  "MOD_SLIME",
  "MOD_LAVA",
  "MOD_CRUSH",
  "MOD_DROP",
  "MOD_TELEFRAG",
  "MOD_FALLING",
  "MOD_SUICIDE",
  "MOD_TARGET_LASER",
  "MOD_TRIGGER_HURT",
  "MOD_SUFFOCATION",
  "MOD_SELFDESTRUCT",

  "MOD_ABUILDER_CLAW",
  "MOD_LEVEL0_BITE",
  "MOD_LEVEL1_CLAW",
  "MOD_LEVEL1_PCLOUD",
  "MOD_LEVEL3_CLAW",
  "MOD_LEVEL3_POUNCE",
  "MOD_LEVEL3_BOUNCEBALL",
  "MOD_LEVEL2_CLAW",
  "MOD_LEVEL2_EXPLOSION",
  "MOD_LEVEL2_ZAP",
  "MOD_LEVEL4_CLAW",
  "MOD_LEVEL4_TRAMPLE",
  "MOD_LEVEL4_CRUSH",
  "MOD_SPITFIRE_POUNCE",
  "MOD_SPITFIRE_ZAP",

  "MOD_SLOWBLOB",
  "MOD_POISON",
  "MOD_SWARM",

  "MOD_HSPAWN",
  "MOD_TESLAGEN",
  "MOD_MGTURRET",
  "MOD_REACTOR",
  "MOD_MEDISTAT",

  "MOD_ASPAWN",
  "MOD_ATUBE",
  "MOD_ZUNGE",
  "MOD_OVERMIND",
  "MOD_DECONSTRUCT",
  "MOD_REPLACE",
  "MOD_NOCREEP",
  "MOD_SLAP"
};

/*
==================
G_IncreaseBonusValue

Safely increase the bonusValue
==================
*/
void  G_IncreaseBonusValue( int *bonusValue, int diff )
{
  if( diff <= 0 )
    return;

  Com_Assert( bonusValue &&
              "G_IncreaseBonusValue: NULL bonusValue" );

  if( *bonusValue == INT_MAX )
    return;

  if( INT_MAX - diff > *bonusValue )
    *bonusValue += diff;
  else
    *bonusValue = INT_MAX;

  if( *bonusValue < 0 )
    *bonusValue = 0;
}

/*
==================
G_IncreaseDamageCredits

Safely increase the damage credits
==================
*/
static void  G_IncreaseDamageCredits( int *credits, int diff )
{
  if( diff <= 0 )
    return;

  Com_Assert( credits &&
              "G_IncreaseDamageCredits: NULL credits" );

  if( *credits == INT_MAX )
    return;

  if( INT_MAX - diff > *credits )
    *credits += diff;
  else
    *credits = INT_MAX;

  if( *credits < 0 )
    *credits = 0;
}

typedef struct rewardBuildableData_s
{
  gentity_t *self;
  gentity_t *enemyPlayer;
  qboolean  damagedByEnemyPlayer;
  int       *alienCredits;
  int       *humanCredits;
  int       (*numTeamPlayers)[ NUM_TEAMS ];
  float     totalDamage;
  int       maxHealth;
  float     value;
} rewardBuildableData_t;

/*
==================
G_RewardAttackers

give credits from damage by defense buildables and empty the array
==================
*/
static void RewardBuildableAttackers( void *data, void *user_data )
{
  credits_t             *credits = (credits_t *)data;
  rewardBuildableData_t *rewardBuildableData = (rewardBuildableData_t *)user_data;
  gentity_t             *buildable = G_Entity_id_get( &credits->id );
  gentity_t             *self = rewardBuildableData->self;
  gentity_t             *player = rewardBuildableData->enemyPlayer;
  qboolean              damagedByEnemyPlayer = rewardBuildableData->damagedByEnemyPlayer;
  team_t                buildableTeam;
  int                   *alienCredits = rewardBuildableData->alienCredits;
  int                   *humanCredits = rewardBuildableData->humanCredits;
  float                 totalDamage = rewardBuildableData->totalDamage;
  int                   maxHealth = rewardBuildableData->maxHealth;
  float                 value = rewardBuildableData->value;
  int                   (*numTeamPlayers)[ NUM_TEAMS ];
  float                 damageFraction = ( (float)credits->credits ) / ( (float)totalDamage );
  int                   dBValue = value * damageFraction;
  int                   buildablesTeamDistributionEarnings;
  int                   j;

  numTeamPlayers = rewardBuildableData->numTeamPlayers;

  Com_Assert( buildable &&
              "RewardBuildableAttackers: buildable is NULL" );
  Com_Assert( buildable->s.number >= 0 &&
              buildable->s.number < MAX_GENTITIES &&
              "RewardBuildableAttackers: buildable number is invalid" );
  Com_Assert( buildable == &g_entities[ buildable->s.number ] &&
              "RewardBuildableAttackers: buildable is not in the gentities[] array" );
  Com_Assert( buildable->inuse &&
              "RewardBuildableAttackers: buildable is not in use" );
  Com_Assert( buildable->s.eType == ET_BUILDABLE &&
              "RewardBuildableAttackers: buildable is not really a buildable" );
  Com_Assert( self &&
              "RewardBuildableAttackers: self is NULL" );
  Com_Assert( self->s.number >= 0 &&
              self->s.number < MAX_GENTITIES &&
              "RewardBuildableAttackers: self number is invalid" );
  Com_Assert( self == &g_entities[ self->s.number ] &&
              "RewardBuildableAttackers: self is not in the gentities[] array" );
  Com_Assert( self->inuse &&
              "RewardBuildableAttackers: self is not in use" );
  Com_Assert( alienCredits &&
              "RewardBuildableAttackers: alienCredits is NULL" );
  Com_Assert( alienCredits >= 0 &&
              "RewardBuildableAttackers: alienCredits is negative" );
  Com_Assert( humanCredits &&
              "RewardBuildableAttackers: humanCredits is NULL" );
  Com_Assert( humanCredits >= 0 &&
              "RewardBuildableAttackers: humanCredits is negative" );
  Com_Assert( numTeamPlayers &&
              "RewardBuildableAttackers: numTeamPlayers is NULL" );
  Com_Assert( totalDamage >= 0 &&
              "RewardBuildableAttackers: totalDamage is negative" );
  Com_Assert( maxHealth >= 0 &&
              "RewardBuildableAttackers: maxHealth is negative" );
  Com_Assert( value >= 0 &&
              "RewardBuildableAttackers: value is negative" );
  Com_Assert( ( !damagedByEnemyPlayer ||
                player) &&
              "RewardBuildableAttackers: player is NULL" );
  Com_Assert( ( !damagedByEnemyPlayer ||
                ( player->s.number >= 0 &&
                  player->s.number < MAX_CLIENTS ) ) &&
              "RewardBuildableAttackers: player number is invalid" );
  Com_Assert( ( !damagedByEnemyPlayer ||
                ( player == &g_entities[ player->s.number ] ) ) &&
              "RewardBuildableAttackers: player is not in the gentities[] array" );
  Com_Assert( ( !damagedByEnemyPlayer ||
                player->inuse ) &&
              "RewardBuildableAttackers: player is not in use" );

  buildableTeam = buildable->buildableTeam;

  Com_Assert( buildableTeam >= 0 &&
              buildableTeam < NUM_TEAMS &&
              "RewardBuildableAttackers: buildableTeam is invalid" );

  if( totalDamage < maxHealth )
    dBValue = (int)( ( (float)dBValue ) * ( ( totalDamage ) / ( (float)maxHealth ) ) );

  if( (*numTeamPlayers)[ buildableTeam ] )
    buildablesTeamDistributionEarnings = (int)( ( (float)dBValue ) * ( 2.0f / ( 5.0f * (float)(*numTeamPlayers)[ buildableTeam ] ) ) );
  else
    buildablesTeamDistributionEarnings = 0;

  G_IncreaseBonusValue( &buildable->bonusValue, ( ( (float)dBValue * 0.80f ) ) );

  // add to the builder's score
  if( buildable->builtBy &&
      buildable->builtBy->slot >= 0 &&
      g_entities[ buildable->builtBy->slot ].client->pers.connected == CON_CONNECTED &&
      buildableTeam == g_entities[ buildable->builtBy->slot ].client->ps.stats[ STAT_TEAM ] )
    AddScore( &g_entities[ buildable->builtBy->slot ], dBValue );

  for( j = 0; j < level.maxclients; j++ )
  {
    if( level.clients[ j ].pers.connected == CON_CONNECTED &&
        level.clients[ j ].pers.teamSelection != TEAM_NONE )
    {
      //distribute the team specific earnings
      if( level.clients[ j ].pers.teamSelection == buildableTeam )
        G_AddCreditToClient( &level.clients[ j ], buildablesTeamDistributionEarnings, qtrue );
    }
  }

  // The value from killed players that have been damaged by another player counts towards stage advancement
  if( ( !IS_WARMUP ) && ( self->client ) &&
      damagedByEnemyPlayer )
  {
    // add to stage counters
    if( player->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      G_IncreaseDamageCredits( alienCredits, dBValue);
    else if( player->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
      G_IncreaseDamageCredits( humanCredits, dBValue);
  }

  credits->credits = 0;
}

/*
==================
G_RewardAttackers

Function to distribute rewards to entities that killed this one.
Returns the total damage dealt.
==================
*/
float G_RewardAttackers( gentity_t *self, upgrade_t destroyedUp )
{
  float                 value, totalDamage = 0;
  int                   i, maxHealth = 0;
  int                   alienCredits = 0, humanCredits = 0;
  int                   numTeamPlayers[ NUM_TEAMS ];
  int                   ( *credits )[ MAX_CLIENTS ];
  credits_t             ( *creditsDeffenses )[ MAX_GENTITIES ];
  team_t                team;
  gentity_t             *enemyPlayer = NULL;
  qboolean              damagedByEnemyPlayer = qfalse;
  const qboolean        isBuildable = ( self->s.eType == ET_BUILDABLE );
  bgqueue_t             enemyBuildables = BG_QUEUE_INIT;
  rewardBuildableData_t rewardBuildableData;

  if( destroyedUp != UP_NONE )
  {
    Com_Assert( self->client && "Only clients are suppose to have breakable upgrades" );
    credits = self->creditsUpgrade[ destroyedUp ];
    creditsDeffenses = self->creditsUpgradeDeffenses[ destroyedUp ];
  } else
  {
    credits = self->credits;
    creditsDeffenses = self->creditsDeffenses;
  }

  // Total up all the damage done by non-teammates
  for( i = 0; i < level.maxclients; i++ )
  {
    gentity_t *player = g_entities + i;

    // no damage from this player
    if( !( *credits )[ i ] )
      continue;

    // don't count players that are no longer playing
    if( player->client->pers.connected != CON_CONNECTED ||
        player->client->ps.stats[ STAT_TEAM ] == TEAM_NONE )
    {
      ( *credits )[ i ] = 0;
      continue;
    }

    if( !OnSameTeam( self, player ) )
    {
      totalDamage += (float)( *credits )[ i ];
      damagedByEnemyPlayer = qtrue;
      enemyPlayer = player;
    }
  }

  // add damage done by enemy buildables to the total and
  // add those buildables to the enemy buildable list
  for( i = MAX_CLIENTS; i < level.num_entities; i++ )
  {
    gentity_t *buildable = &g_entities[ i ];

    if( !( *creditsDeffenses )[ i ].credits )
      continue;

    // no longer the same entity that damaged this entity
    if( G_Entity_id_get( &( *creditsDeffenses )[ i ].id ) != buildable )
    {
      ( *creditsDeffenses )[ i ].credits = 0;
      continue;
    }

    if( buildable->s.eType != ET_BUILDABLE ||
        ( self->client &&
          self->client->ps.stats[ STAT_TEAM ] == buildable->buildableTeam ) || 
        buildable->buildableTeam == self->buildableTeam )
    {
      continue;
    }

    BG_Queue_Push_Head( &enemyBuildables, &( *creditsDeffenses )[ i ] );

    totalDamage += ( *creditsDeffenses )[ i ].credits;
  }

  if( totalDamage <= 0.0f )
    return 0.0f;

  // Only give credits for killing players and buildables
  if( self->client )
  {
    if( destroyedUp == UP_NONE )
    {
      value = BG_GetValueOfPlayer( &self->client->ps );

      // value while evolving
      if( self->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS &&
          self->client->ps.eFlags & EF_EVOLVING )
      {
        //disperse income for death during evolving based on the current progress
        value *= 1.0f - ( ( (float)self->client->ps.stats[ STAT_MISC3 ] ) /
                          ( (float)self->client->ps.stats[ STAT_MISC2 ] ) );
      }
    }
    else
      value = BG_Upgrade( destroyedUp )->price;

    team = self->client->pers.teamSelection;

    if( destroyedUp == UP_BATTLESUIT )
      maxHealth = BSUIT_MAX_ARMOR;
    else
      maxHealth = BG_Class( self->client->ps.stats[ STAT_CLASS ] )->health;
  }
  else if( isBuildable )
  {
    value = BG_Buildable( self->s.modelindex )->value;

    // only give partial credits for a buildable not yet completed
    if( !self->spawned )
    {
      if( self->buildableTeam == TEAM_HUMANS )
        value = ( value * ( BG_Buildable( self->s.modelindex )->buildTime - 
                            self->buildProgress ) ) /
                 BG_Buildable( self->s.modelindex )->buildTime;
      else if( self->buildableTeam == TEAM_ALIENS )
        value = ( value * ( level.time - self->buildTime ) ) /
                 BG_Buildable( self->s.modelindex )->buildTime;
    }

    team = self->buildableTeam;
    maxHealth = BG_Buildable( self->s.modelindex )->health;
    // account for repaired damage
    maxHealth += (int)( (float)( maxHealth - self->s.constantLight ) *
                        BG_Buildable( self->s.modelindex )->maxHealthDecayRate );
  }
  else
    return totalDamage;

  if( destroyedUp == UP_NONE )
  {
    value += self->bonusValue;
    if( value > INT_MAX )
      value = INT_MAX;
    else if( value < 0 )
      value = 0;
  }

  numTeamPlayers[ TEAM_ALIENS ] = level.numAlienClients;
  numTeamPlayers[ TEAM_HUMANS ] = level.numHumanClients;

  // Give credits and empty the array
  for( i = 0; i < level.maxclients; i++ )
  {
    int stageValue = (int)( (float)value * ( ( (float)( *credits )[ i ] ) / ( totalDamage ) ) );
    int j;
    team_t playersTeam;
    gentity_t *player = g_entities + i;
    playersTeam = player->client->pers.teamSelection;

    if( playersTeam != team )
    {
      int          playersTeamDistributionEarnings;
      int          playersPersonalEarnings;
      int bonusValue;

      if( totalDamage < maxHealth )
        stageValue *= totalDamage / maxHealth;

      if( !( *credits )[ i ] || player->client->ps.stats[ STAT_TEAM ] == team )
        continue;

      playersTeamDistributionEarnings = ( stageValue * (  isBuildable ? 4 : 3 ) ) / ( 10 * numTeamPlayers[ playersTeam ] );
      bonusValue = ( stageValue * (  isBuildable ? 2 : 3 ) ) / ( 10 );

      // any remainders goes to the player that did the damage;
      playersPersonalEarnings = stageValue;
      playersPersonalEarnings -= playersTeamDistributionEarnings * numTeamPlayers[ playersTeam ];
      playersPersonalEarnings -= bonusValue;


      AddScore( player, stageValue );

      G_IncreaseBonusValue( &player->bonusValue, bonusValue );

      for( j = 0; j < level.maxclients; j++ )
      {
        if( level.clients[ j ].pers.connected == CON_CONNECTED &&
            level.clients[ j ].pers.teamSelection != TEAM_NONE )
        {
          //distribute the team specific earnings
          if( level.clients[ j ].pers.teamSelection == playersTeam )
            G_AddCreditToClient( &level.clients[ j ], playersTeamDistributionEarnings, qtrue );
        }
      }

      // any remaining earnings goes to the player that did the damage
      G_AddCreditToClient( &level.clients[ i ], playersPersonalEarnings, qtrue );

      // Only the value from killed players counts towards stage advancement
      if( ( !IS_WARMUP ) && ( self->client ) && !g_AMPStageLock.integer )
      {
        // add to stage counters
        if( player->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
          G_IncreaseDamageCredits( &alienCredits, stageValue );
        else if( player->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
          G_IncreaseDamageCredits( &humanCredits, stageValue );
      }
    }
    ( *credits )[ i ] = 0;
  }

  rewardBuildableData.self = self;
  rewardBuildableData.enemyPlayer = enemyPlayer;
  rewardBuildableData.damagedByEnemyPlayer = damagedByEnemyPlayer;
  rewardBuildableData.alienCredits = &alienCredits;
  rewardBuildableData.humanCredits= &humanCredits;
  rewardBuildableData.numTeamPlayers = numTeamPlayers;
  rewardBuildableData.totalDamage = totalDamage;
  rewardBuildableData.maxHealth = maxHealth;
  rewardBuildableData.value = value;

  BG_Queue_Foreach( &enemyBuildables, RewardBuildableAttackers,
                    &rewardBuildableData );
  BG_Queue_Clear( &enemyBuildables );


  if( alienCredits )
  {
    Cvar_SetSafe( "g_alienCredits",
      va( "%d", g_alienCredits.integer + alienCredits ) );
    Cvar_Update( &g_alienCredits );
  }
  if( humanCredits )
  {
    Cvar_SetSafe( "g_humanCredits",
      va( "%d", g_humanCredits.integer + humanCredits ) );
    Cvar_Update( &g_humanCredits );
  }

  return totalDamage;
}

/*
==================
GibEntity
==================
*/
void GibEntity( gentity_t *self )
{
  vec3_t dir;
  gentity_t *tent;

  VectorCopy( self->s.origin2, dir );

  //send gibbing event
  tent = G_TempEntity( self->r.currentOrigin, EV_GIB_PLAYER );
  tent->s.eventParm = DirToByte( dir );
  tent->s.pos.trType = TR_GRAVITY;
  tent->s.pos.trTime = level.time;
  if( self->client )
    VectorCopy( self->client->ps.velocity, tent->s.pos.trDelta );
  else
    VectorCopy( self->s.pos.trDelta, tent->s.pos.trDelta );

  self->takedamage = qfalse;
  G_SetContents( self, CONTENTS_CORPSE );

  //set the gibbed flag
  if( self->client )
  {
    self->client->ps.stats[ STAT_FLAGS ] |= SFL_GIBBED;
  } else
  {
    self->s.otherEntityNum2 |= SFL_GIBBED;
  }

  self->die = NULL;
}

/*
==================
body_die
==================
*/
void body_die( gentity_t *self, gentity_t *unused, gentity_t *unused2, int damage, int meansOfDeath )
{
  if ( self->health > GIB_HEALTH )
    return;

  GibEntity( self );
}


/*
==================
player_die
==================
*/
void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
  gentity_t *ent;
  int       savedClientFlags;
  int       anim;
  int       killer;
  int       i;
  char      *killerName, *obit;

  if( self->client->ps.pm_type == PM_DEAD &&
      meansOfDeath != MOD_SELFDESTRUCT )
    return;

  if( level.intermissiontime )
    return;

  self->id = 0;

  self->client->ps.pm_type = PM_DEAD;
  self->suicideTime = 0;

  self->healthReserve = 0;

  if( attacker )
  {
    killer = attacker->s.number;

    if( attacker->client )
      killerName = attacker->client->pers.netname;
    else
      killerName = "<world>";
  }
  else
  {
    killer = ENTITYNUM_WORLD;
    killerName = "<world>";
  }

  if( meansOfDeath < 0 || meansOfDeath >= ARRAY_LEN( modNames ) )
    // fall back on the number
    obit = va( "%d", meansOfDeath );
  else
    obit = modNames[ meansOfDeath ];

  G_LogPrintf( "Die: %d %d %s: %s" S_COLOR_WHITE " killed %s\n",
    killer,
    (int)( self - g_entities ),
    obit,
    killerName,
    self->client->pers.netname );

  // deactivate all upgrades
  for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
    BG_DeactivateUpgrade( i, self->client->ps.stats );

  // broadcast the death event to everyone
  ent = G_TempEntity( self->r.currentOrigin, EV_OBITUARY );
  ent->s.eventParm = meansOfDeath;
  ent->s.otherEntityNum = self->s.number;
  ent->s.otherEntityNum2 = killer;
  ent->r.svFlags = SVF_BROADCAST; // send to everyone

  self->enemy = attacker;
  self->client->ps.persistant[ PERS_KILLED ]++;

  if( attacker && attacker->client )
  {
    attacker->client->lastkilled_client = self->s.number;

    if( ( attacker == self || OnSameTeam( self, attacker ) ) && meansOfDeath != MOD_HSPAWN )
    {
      //punish team kills and suicides
      if( attacker->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      {
        G_AddCreditToClient( attacker->client, -ALIEN_TK_SUICIDE_PENALTY, qtrue );
        AddScore( attacker, -ALIEN_TK_SUICIDE_PENALTY );
      }
      else if( attacker->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
      {
        G_AddCreditToClient( attacker->client, -HUMAN_TK_SUICIDE_PENALTY, qtrue );
        AddScore( attacker, -HUMAN_TK_SUICIDE_PENALTY );
      }
    }
  }
  else if( attacker && attacker->s.eType != ET_BUILDABLE )
  {
    if( self->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      AddScore( self, -ALIEN_TK_SUICIDE_PENALTY );
    else if( self->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
      AddScore( self, -HUMAN_TK_SUICIDE_PENALTY );
  }

  // give credits for killing this player
  if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, self->client->ps.stats) )
    G_RewardAttackers( self, UP_BATTLESUIT );
  G_RewardAttackers( self, UP_NONE );

  ScoreboardMessage( self );    // show scores

  // send updated scores to any clients that are following this one,
  // or they would get stale scoreboards
  for( i = 0 ; i < level.maxclients ; i++ )
  {
    gclient_t *client;

    client = &level.clients[ i ];
    if( client->pers.connected != CON_CONNECTED )
      continue;

    if( client->sess.spectatorState == SPECTATOR_NOT )
      continue;

    if( client->sess.spectatorClient == self->s.number )
      ScoreboardMessage( g_entities + i );
  }

  VectorCopy( self->r.currentOrigin, self->client->pers.lastDeathLocation );

  self->s.weapon = WP_NONE;

  self->client->ps.viewangles[ PITCH ] = 0; // zomg
  self->client->ps.viewangles[ YAW ] = self->s.apos.trBase[ YAW ];
  self->client->ps.viewangles[ ROLL ] = 0;
  LookAtKiller( self, inflictor, attacker );

  self->s.loopSound = 0;

  self->r.maxs[ 2 ] = -8;

  // don't allow respawn until the death anim is done
  // g_forcerespawn may force spawning at some later time
  self->client->respawnTime = level.time + 1700;

  if( BG_ExplodeMarauder( &self->client->ps,
                          &self->client->pmext ) )
  {
    float  explosionMod = self->client->pmext.explosionMod;

    if( self->client->ps.eFlags & EF_EVOLVING )
      explosionMod *= BG_EvolveScale( &self->client->ps );

    // do some splash damage
    G_SelectiveRadiusDamage( self->client->ps.origin,
                             self->r.mins, self->r.maxs,
                             self,
                             ( (int)( explosionMod * ( (float)LEVEL2_EXPLODE_CHARGE_SPLASH_DMG ) ) ),
                             ( (int)( explosionMod * LEVEL2_EXPLODE_CHARGE_SPLASH_RADIUS ) ),
                             self, MOD_LEVEL2_EXPLOSION,
                             TEAM_ALIENS, qtrue );

    // create the explosion zap
    MarauderExplosionZap( self );

    //cleanup the player entity
    self->takedamage = qfalse;
    G_SetContents( self, 0 );
    self->die = NULL;
    return;
  }
  else if( self->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
  {
    if ( self->health <= GIB_HEALTH )
    {
      GibEntity( self );
    } else
    {
      self->takedamage = qtrue; // can still be gibbed
      G_SetContents( self, CONTENTS_CORPSE );
      self->die = body_die;
    }
  } else
  {
    self->takedamage = qfalse;
    G_SetContents( self, 0 );
    self->die = NULL;
  }

  // save client flags
  savedClientFlags = self->client->ps.stats[ STAT_FLAGS ];

  // clear misc
  memset( self->client->ps.misc, 0, sizeof( self->client->ps.misc ) );

  self->client->ps.stats[ STAT_FLAGS ] = savedClientFlags;

  {
    // normal death
    static int i;

    if( !( self->client->ps.persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
    {
      switch( i )
      {
        case 0:
          anim = BOTH_DEATH1;
          break;
        case 1:
          anim = BOTH_DEATH2;
          break;
        case 2:
        default:
          anim = BOTH_DEATH3;
          break;
      }
    }
    else
    {
      switch( i )
      {
        case 0:
          anim = NSPA_DEATH1;
          break;
        case 1:
          anim = NSPA_DEATH2;
          break;
        case 2:
        default:
          anim = NSPA_DEATH3;
          break;
      }
    }

    self->client->ps.legsAnim =
      ( ( self->client->ps.legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;

    if( !( self->client->ps.persistant[ PERS_STATE ] & PS_NONSEGMODEL ) )
    {
      self->client->ps.torsoAnim =
        ( ( self->client->ps.torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;
    }

    // use own entityid if killed by non-client to prevent uint8_t overflow
    G_AddEvent( self, EV_DEATH1 + i,
      ( killer < MAX_CLIENTS ) ? killer : self - g_entities );

    // globally cycle through the different death animations
    i = ( i + 1 ) % 3;
  }

  SV_LinkEntity( self );

  self->client->pers.infoChangeTime = level.time;
}

/*
===============
G_ParseDmgScript
===============
*/
static int G_ParseDmgScript( damageRegion_t *regions, char *buf )
{
  char  *token;
  float angleSpan, heightSpan;
  int   count;

  for( count = 0; ; count++ )
  {
    token = COM_Parse( &buf );
    if( !token[ 0 ] )
      break;

    if( strcmp( token, "{" ) )
    {
      COM_ParseError( "Missing {" );
      break;
    }

    if( count >= MAX_DAMAGE_REGIONS )
    {
      COM_ParseError( "Max damage regions exceeded" );
      break;
    }

    // defaults
    regions[ count ].name[ 0 ] = '\0';
    regions[ count ].minHeight = 0.0f;
    regions[ count ].maxHeight = 1.0f;
    regions[ count ].minAngle = 0.0f;
    regions[ count ].maxAngle = 360.0f;
    regions[ count ].modifier = 1.0f;
    regions[ count ].crouch = qfalse;

    while( 1 )
    {
      token = COM_ParseExt( &buf, qtrue );

      if( !token[ 0 ] )
      {
        COM_ParseError( "Unexpected end of file" );
        break;
      }

      if( !Q_stricmp( token, "}" ) )
      {
        break;
      }
      else if( !strcmp( token, "name" ) )
      {
        token = COM_ParseExt( &buf, qfalse );
        if( token[ 0 ] )
          Q_strncpyz( regions[ count ].name, token,
                      sizeof( regions[ count ].name ) );
      }
      else if( !strcmp( token, "minHeight" ) )
      {
        token = COM_ParseExt( &buf, qfalse );
        if( !token[ 0 ] )
          strcpy( token, "0" );
        regions[ count ].minHeight = atof( token );
      }
      else if( !strcmp( token, "maxHeight" ) )
      {
        token = COM_ParseExt( &buf, qfalse );
        if( !token[ 0 ] )
          strcpy( token, "100" );
        regions[ count ].maxHeight = atof( token );
      }
      else if( !strcmp( token, "minAngle" ) )
      {
        token = COM_ParseExt( &buf, qfalse );
        if( !token[ 0 ] )
          strcpy( token, "0" );
        regions[ count ].minAngle = atoi( token );
      }
      else if( !strcmp( token, "maxAngle" ) )
      {
        token = COM_ParseExt( &buf, qfalse );
        if( !token[ 0 ] )
          strcpy( token, "360" );
        regions[ count ].maxAngle = atoi( token );
      }
      else if( !strcmp( token, "modifier" ) )
      {
        token = COM_ParseExt( &buf, qfalse );
        if( !token[ 0 ] )
          strcpy( token, "1.0" );
        regions[ count ].modifier = atof( token );
      }
      else if( !strcmp( token, "crouch" ) )
      {
        regions[ count ].crouch = qtrue;
      }
      else
      {
        COM_ParseWarning("Unknown token \"%s\"", token);
      }
    }

    // Angle portion covered
    angleSpan = regions[ count ].maxAngle - regions[ count ].minAngle;
    if( angleSpan < 0.0f )
      angleSpan += 360.0f;
    angleSpan /= 360.0f;

    // Height portion covered
    heightSpan = regions[ count ].maxHeight - regions[ count ].minHeight;
    if( heightSpan < 0.0f )
      heightSpan = -heightSpan;
    if( heightSpan > 1.0f )
      heightSpan = 1.0f;

    regions[ count ].area = angleSpan * heightSpan;
    if( !regions[ count ].area )
      regions[ count ].area = 0.00001f;
  }

  return count;
}

/*
============
GetRegionDamageModifier
============
*/
static float GetRegionDamageModifier( gentity_t *targ, int class, int piece )
{
  damageRegion_t *regions, *overlap;
  float modifier = 0.0f, areaSum = 0.0f;
  int j, i;
  qboolean crouch;

  crouch = targ->client->ps.pm_flags & PMF_DUCKED;
  overlap = &g_damageRegions[ class ][ piece ];

  if( g_debugDamage.integer > 2 )
    Com_Printf( "GetRegionDamageModifier():\n"
              ".   bodyRegion = [%d %d %f %f] (%s)\n"
              ".   modifier = %f\n",
              overlap->minAngle, overlap->maxAngle,
              overlap->minHeight, overlap->maxHeight,
              overlap->name, overlap->modifier );

              
  // Battlesuits use armor points
  if( !BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) )
  {
    // Find the armour layer modifier, assuming that none of the armour regions
    // overlap and that any areas that are not covered have a modifier of 1.0
    for( j = UP_NONE + 1; j < UP_NUM_UPGRADES; j++ )
    {
      if( !BG_InventoryContainsUpgrade( j, targ->client->ps.stats ) ||
          !g_numArmourRegions[ j ] )
        continue;

      regions = g_armourRegions[ j ];

      for( i = 0; i < g_numArmourRegions[ j ]; i++ )
      {
        float overlapMaxA, regionMinA, regionMaxA, angleSpan, heightSpan, area;

        if( regions[ i ].crouch != crouch )
          continue;

        // Convert overlap angle to 0 to max
        overlapMaxA = overlap->maxAngle - overlap->minAngle;
        if( overlapMaxA < 0.0f )
          overlapMaxA += 360.0f;

        // Convert region angles to match overlap
        regionMinA = regions[ i ].minAngle - overlap->minAngle;
        if( regionMinA < 0.0f )
          regionMinA += 360.0f;
        regionMaxA = regions[ i ].maxAngle - overlap->minAngle;
        if( regionMaxA < 0.0f )
          regionMaxA += 360.0f;

        // Overlapping Angle portion
        if( regionMinA <= regionMaxA )
        {
          angleSpan = 0.0f;
          if( regionMinA < overlapMaxA )
          {
            if( regionMaxA > overlapMaxA )
              regionMaxA = overlapMaxA;
            angleSpan = regionMaxA - regionMinA;
          }
        }
        else
        {
          if( regionMaxA > overlapMaxA )
            regionMaxA = overlapMaxA;
          angleSpan = regionMaxA;
          if( regionMinA < overlapMaxA )
            angleSpan += overlapMaxA - regionMinA;
        }
        angleSpan /= 360.0f;

        // Overlapping height portion
        heightSpan = MIN( overlap->maxHeight, regions[ i ].maxHeight ) -
                     MAX( overlap->minHeight, regions[ i ].minHeight );
        if( heightSpan < 0.0f )
          heightSpan = 0.0f;
        if( heightSpan > 1.0f )
          heightSpan = 1.0f;

        if( g_debugDamage.integer > 2 )
          Com_Printf( ".   armourRegion = [%d %d %f %f] (%s)\n"
                    ".   .   modifier = %f\n"
                    ".   .   angleSpan = %f\n"
                    ".   .   heightSpan = %f\n",
                    regions[ i ].minAngle, regions[ i ].maxAngle,
                    regions[ i ].minHeight, regions[ i ].maxHeight,
                    regions[ i ].name, regions[ i ].modifier,
                    angleSpan, heightSpan );

        areaSum += area = angleSpan * heightSpan;
        modifier += regions[ i ].modifier * area;
      }
    }
  }

  if( g_debugDamage.integer > 2 )
    Com_Printf( ".   areaSum = %f\n"
              ".   armourModifier = %f\n", areaSum, modifier );

  return overlap->modifier * ( overlap->area + modifier - areaSum );
}

/*
============
GetNonLocDamageModifier
============
*/
static float GetNonLocDamageModifier( gentity_t *targ, int class )
{
  float modifier = 0.0f, area = 0.0f, scale = 0.0f;
  int i;
  qboolean crouch;

  // For every body region, use stretch-armor formula to apply armour modifier
  // for any overlapping area that armour shares with the body region
  crouch = targ->client->ps.pm_flags & PMF_DUCKED;
  for( i = 0; i < g_numDamageRegions[ class ]; i++ )
  {
    damageRegion_t *region;

    region = &g_damageRegions[ class ][ i ];

    if( region->crouch != crouch )
      continue;

    modifier += GetRegionDamageModifier( targ, class, i );

    scale += region->modifier * region->area;
    area += region->area;

  }

  modifier = !scale ? 1.0f : 1.0f + ( modifier / scale - 1.0f ) * area;

  if( g_debugDamage.integer > 1 )
    Com_Printf( "GetNonLocDamageModifier() modifier:%f, area:%f, scale:%f\n",
              modifier, area, scale );

  return modifier;
}

/*
============
GetPointDamageModifier

Returns the damage region given an angle and a height proportion
============
*/
static float GetPointDamageModifier( gentity_t *targ, damageRegion_t *regions,
                                     int len, float angle, float height )
{
  float modifier = 1.0f;
  int i;

  for( i = 0; i < len; i++ )
  {
    if( regions[ i ].crouch != ( targ->client->ps.pm_flags & PMF_DUCKED ) )
      continue;

    // Angle must be within range
    if( ( regions[ i ].minAngle <= regions[ i ].maxAngle &&
          ( angle < regions[ i ].minAngle ||
            angle > regions[ i ].maxAngle ) ) ||
        ( regions[ i ].minAngle > regions[ i ].maxAngle &&
          angle > regions[ i ].maxAngle && angle < regions[ i ].minAngle ) )
      continue;

    // Height must be within range
    if( height < regions[ i ].minHeight || height > regions[ i ].maxHeight )
      continue;

    modifier *= regions[ i ].modifier;
  }

  if( g_debugDamage.integer )
    Com_Printf( "GetDamageRegionModifier(angle = %f, height = %f): %f\n",
              angle, height, modifier );

  return modifier;
}

/*
============
G_CalcDamageModifier
============
*/
static float G_CalcDamageModifier( vec3_t point, gentity_t *targ, gentity_t *attacker, int class, int dflags )
{
  vec3_t targOrigin, bulletPath, bulletAngle, pMINUSfloor, floor, normal;
  vec3_t mins, maxs;
  float  clientHeight, hitRelative, hitRatio, modifier;
  int    hitRotation, i;

  if( point == NULL )
    return 1.0f;

  // Battlesuits use armor points
  if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) )
    return 1.0f;

  // Don't need to calculate angles and height for non-locational damage
  if( dflags & DAMAGE_NO_LOCDAMAGE )
    return GetNonLocDamageModifier( targ, class );

  // Get the point location relative to the floor under the target
  G_GetUnlaggedOrigin(targ, targOrigin);

  G_GetUnlaggedDimensions(targ, mins, maxs);

  BG_GetClientNormal( &targ->client->ps, normal );
  VectorMA( targOrigin, mins[ 2 ], normal, floor );
  VectorSubtract( point, floor, pMINUSfloor );

  // Get the proportion of the target height where the hit landed
  clientHeight = maxs[ 2 ] - mins[ 2 ];

  if( !clientHeight )
    clientHeight = 1.0f;

  hitRelative = DotProduct( normal, pMINUSfloor ) / VectorLength( normal );

  if( hitRelative < 0.0f )
    hitRelative = 0.0f;

  if( hitRelative > clientHeight )
    hitRelative = clientHeight;

  hitRatio = hitRelative / clientHeight;

  // Get the yaw of the attack relative to the target's view yaw
  VectorSubtract( point, targOrigin, bulletPath );
  vectoangles( bulletPath, bulletAngle );

  hitRotation = AngleNormalize360( targ->client->ps.viewangles[ YAW ] -
                                   bulletAngle[ YAW ] );

  // Get modifiers from the target's damage regions
  modifier = GetPointDamageModifier( targ, g_damageRegions[ class ],
                                     g_numDamageRegions[ class ],
                                     hitRotation, hitRatio );

 // Battlesuits use armor points
  if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) )
    return modifier;

  for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
  {
    if( BG_InventoryContainsUpgrade( i, targ->client->ps.stats ) )
    {
      modifier *= GetPointDamageModifier( targ, g_armourRegions[ i ],
                                          g_numArmourRegions[ i ],
                                          hitRotation, hitRatio );
    }
  }

  return modifier;
}


/*
============
G_InitDamageLocations
============
*/
void G_InitDamageLocations( void )
{
  char          *modelName;
  char          filename[ MAX_QPATH ];
  int           i;
  int           len;
  fileHandle_t  fileHandle;
  char          buffer[ MAX_DAMAGE_REGION_TEXT ];

  for( i = PCL_NONE + 1; i < PCL_NUM_CLASSES; i++ )
  {
    modelName = BG_ClassConfig( i )->modelName;
    Com_sprintf( filename, sizeof( filename ),
                 "models/players/%s/locdamage.cfg", modelName );

    len = FS_FOpenFileByMode( filename, &fileHandle, FS_READ );
    if ( !fileHandle )
    {
      Com_Printf( S_COLOR_RED "file not found: %s\n", filename );
      continue;
    }

    if( len >= MAX_DAMAGE_REGION_TEXT )
    {
      Com_Printf( S_COLOR_RED "file too large: %s is %i, max allowed is %i",
                filename, len, MAX_DAMAGE_REGION_TEXT );
      FS_FCloseFile( fileHandle );
      continue;
    }

    COM_BeginParseSession( filename );

    FS_Read2( buffer, len, fileHandle );
    buffer[len] = 0;
    FS_FCloseFile( fileHandle );

    g_numDamageRegions[ i ] = G_ParseDmgScript( g_damageRegions[ i ], buffer );
  }

  for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
  {
    modelName = BG_Upgrade( i )->name;
    Com_sprintf( filename, sizeof( filename ), "armour/%s.armour", modelName );

    len = FS_FOpenFileByMode( filename, &fileHandle, FS_READ );

    //no file - no parsage
    if ( !fileHandle )
      continue;

    if( len >= MAX_DAMAGE_REGION_TEXT )
    {
      Com_Printf( S_COLOR_RED "file too large: %s is %i, max allowed is %i",
                filename, len, MAX_DAMAGE_REGION_TEXT );
      FS_FCloseFile( fileHandle );
      continue;
    }

    COM_BeginParseSession( filename );

    FS_Read2( buffer, len, fileHandle );
    buffer[len] = 0;
    FS_FCloseFile( fileHandle );

    g_numArmourRegions[ i ] = G_ParseDmgScript( g_armourRegions[ i ], buffer );
  }
}


/*
============
T_Damage

targ    entity that is being damaged
inflictor entity that is causing the damage
attacker  entity that caused the inflictor to damage targ
  example: targ=monster, inflictor=rocket, attacker=player

dir     direction of the attack for knockback
point   point at which the damage is being inflicted, used for headshots
damage    amount of damage being inflicted
knockback force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be NULL for environmental effects

dflags    these flags are used to control how T_Damage works
  DAMAGE_RADIUS     damage was indirect (from a nearby explosion)
  DAMAGE_NO_ARMOR     armor does not protect from this damage
  DAMAGE_NO_KNOCKBACK   do not affect velocity, just view angles
  DAMAGE_NO_PROTECTION  kills everything except godmode
============
*/

/*
============
G_ChangeHealth

Increases or decreases the target entity's health.
returns the applied change in health
============
*/
int G_ChangeHealth( gentity_t *targ, gentity_t *changer,
                     int change, int healthFlags )
{
  int   *maxHealth = NULL;
  int   initialMaxHealth = 0;
  int   minHealth = 0;
  float maxHealthDecayRate = 0.0f;
  int   targReserveChange = 0;
  int   changerReserveChange = 0;
  int   oldHealth = targ->health;

  Com_Assert( targ &&
              "G_ChangeHealth: NULL targ" );
  Com_Assert( changer &&
              "G_ChangeHealth: NULL inflictor" );

  if( targ->client )
  {
    const class_t class = targ->client->ps.stats[ STAT_CLASS ];

    maxHealth = &targ->client->ps.misc[ MISC_MAX_HEALTH ];
    initialMaxHealth = BG_Class( class )->health;
    minHealth = BG_Class( class )->minHealth;
    maxHealthDecayRate = BG_Class( class )->maxHealthDecayRate;
  } else if( targ->s.eType == ET_BUILDABLE )
  {
    const buildable_t buildable = targ->s.modelindex;

    maxHealth = &targ->s.constantLight;
    initialMaxHealth = BG_Buildable( buildable )->health;
    minHealth = BG_Buildable( buildable )->minHealth;
    maxHealthDecayRate = BG_Buildable( buildable )->maxHealthDecayRate;
  }

  if( healthFlags & HLTHF_IGNORE_MAX )
    maxHealth = NULL;

  // set the health equal to the change, but let the function
  // continue for sanity checks
  if( healthFlags & HLTHF_SET_TO_CHANGE )
  {
    targ->health = change;
    change = 0;
  }

  if( change < 0 )
  {
    // track total damage done by enemies
    if( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) &&
        !OnSameTeam( changer, targ ) &&
        maxHealth )
    {
      if( *maxHealth <
          ( targ->damageFromEnemies - change ) )
      {
        targ->damageFromEnemies = *maxHealth;
      } else
      {
        targ->damageFromEnemies -= change;
      }
    }

    // decrease the health
    if( ( INT_MIN - change ) > targ->health )
      targ->health = INT_MIN;
    else
      targ->health += change;

    // send the health to the clients
    if( targ->client )
    {
      targ->client->ps.misc[ MISC_HEALTH ] = targ->health;
      targ->client->pers.infoChangeTime = level.time;
    }

    return targ->health - oldHealth;
  }

  // attempting to increase the target entity's
  // health, and/or perform sanity checks on its health

  //prevent integer overflow
  if( targ->health == INT_MAX )
    return targ->health - oldHealth;
  else if( targ->health > INT_MAX )
  {
    targ->health = INT_MAX;
    change = 0;
  } else if( ( INT_MAX - change ) < targ->health )
    change = INT_MAX - targ->health;

  // check for evolving
  if( !(healthFlags & HLTHF_EVOLVE_INCREASE) &&
      targ->client &&
      targ->client->pers.teamSelection == TEAM_ALIENS &&
      (targ->s.eFlags & EF_EVOLVING) )
    return targ->health - oldHealth;

  if( (healthFlags & HLTHF_REQ_TARG_RESERVE) &&
      targ->healthReserve <= 0 )
    change = 0;
  else if( (healthFlags & HLTHF_REQ_CHANGER_RESERVE) &&
      changer->healthReserve <= 0 )
    change = 0;

  // check if the initial max health can be exceeded
  if( (healthFlags & HLTHF_INITIAL_MAX_CAP) &&
      change > ( initialMaxHealth - targ->health ) )
  {
    if( ( change + targ->health ) > initialMaxHealth )
      change = initialMaxHealth - targ->health;

    if( change < 0 )
      change = 0;
  }

  // don't let the change heal past the max health
  if( maxHealth &&
      change > ( *maxHealth - targ->health ) )
  {
    change = ( *maxHealth - targ->health );

    if( change < 0 )
      change = 0;
  }

  // check the target's health reserve
  if( (healthFlags & HLTHF_USE_TARG_RESERVE) &&
       change &&
      targ->healthReserve > 0 &&
      ( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) &&
        targ->damageFromEnemies > 0 ) )
  {
    targReserveChange = change;

    // only reduce the reserve from healing enemy damage
    if( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) &&
        targReserveChange > targ->damageFromEnemies )
      targReserveChange = targ->damageFromEnemies;

    // reduce the reserve
    targ->healthReserve -= targReserveChange;
    if( targ->healthReserve < 0 )
      targ->healthReserve = 0;

    if( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) )
    {
      // reduce the damageFromEnemies total counter
      targ->damageFromEnemies -= targReserveChange;
      if( targ->damageFromEnemies < 0 )
        targ->damageFromEnemies = 0;
    }

    change -= targReserveChange;
  }

  // check the changer's health reserve
  if( (healthFlags & HLTHF_USE_CHANGER_RESERVE) &&
      change &&
      targ->health < initialMaxHealth &&
      targ->health < *maxHealth &&
      changer->healthReserve > 0 &&
      ( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) &&
        targ->damageFromEnemies > 0 ) )
  {
    changerReserveChange = change;

    // only decrease the reserve from healing enemy damage
    if( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) &&
        changerReserveChange > targ->damageFromEnemies )
      changerReserveChange = targ->damageFromEnemies;

    // reduce the reserve
    changer->healthReserve -= changerReserveChange;
    if( changer->healthReserve < 0 )
      changer->healthReserve = 0;

    if( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) )
    {
      // reduce the damageFromEnemies total counter
      targ->damageFromEnemies -= changerReserveChange;
      if( targ->damageFromEnemies < 0 )
        targ->damageFromEnemies = 0;
    }

    change -= changerReserveChange;
  }

  // check if healing is allowed without using the reserves
  if( healthFlags & HLTHF_RESERVES_CAP )
    change = 0;

  //check for max health decay
  if( maxHealth &&
      !(healthFlags & HLTHF_NO_DECAY) &&
      change &&
      maxHealthDecayRate &&
      ( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) &&
        targ->damageFromEnemies > 0 ) &&
      minHealth < *maxHealth )
  {
    int decayableChange = ( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) &&
                            ( change > targ->damageFromEnemies ) ) ?
                          targ->damageFromEnemies : change;
    int maxHealthDecay = (int)( maxHealthDecayRate *
                                ( (float)decayableChange ) );

    // don't decay below the currently regained health
    if( maxHealthDecay > ( *maxHealth -
                           ( targ->health + targReserveChange +
                             changerReserveChange ) ) )
      maxHealthDecay = 0;

    if( maxHealthDecay )
    {
      *maxHealth -= maxHealthDecay;

      if( *maxHealth < minHealth )
        *maxHealth = minHealth;
    }
  }

  // increase the health
  targ->health += ( change + targReserveChange + changerReserveChange );

  if( !(healthFlags & HLTHF_IGNORE_ENEMY_DMG) )
  {
    // decrease the damage from enemies
    targ->damageFromEnemies -= change;
    if( targ->damageFromEnemies < 0 )
      targ->damageFromEnemies = 0;
  }

  if( maxHealth &&
      targ->health > *maxHealth )
  {
    // don't exceed the maxHealth;
    targ->health = *maxHealth;
    targ->damageFromEnemies = 0;
  }

  // send the health to the clients
  if( targ->client )
  {
    targ->client->ps.misc[ MISC_HEALTH ] = targ->health;
    targ->client->pers.infoChangeTime = level.time;
  }

  return targ->health - oldHealth;
}

/*
============
G_MODHitDetection

Determines if a means of death would result in hit indication
============
*/
static qboolean G_MODHitIndication( meansOfDeath_t mod )
{
  switch( mod )
  {
    case MOD_DROP:
    case MOD_FALLING:
    case MOD_TRIGGER_HURT:
    case MOD_LEVEL1_PCLOUD:
    case MOD_POISON:
    case MOD_HSPAWN:
    case MOD_ASPAWN:
    case MOD_DECONSTRUCT:
    case MOD_REPLACE:
    case MOD_NOCREEP:
      return qfalse;
    default:
      return qtrue;
  }
}

/*
============
G_MODSpawnProtected

Determines if a means of death doesn't apply damage when spawn protected
============
*/
static qboolean G_MODSpawnProtected( meansOfDeath_t mod )
{
  switch ( mod ) 
  {
    case MOD_WATER:
    case MOD_SLIME:
    case MOD_LAVA:
    case MOD_CRUSH:
    case MOD_DROP:
    case MOD_TELEFRAG:
    case MOD_FALLING:
    case MOD_SUICIDE:
    case MOD_TARGET_LASER:
    case MOD_TRIGGER_HURT:
    case MOD_SUFFOCATION:
    case MOD_SELFDESTRUCT:
      return qfalse;
    default:
      return qtrue;
  }
}

// team is the team that is immune to this damage
void G_SelectiveDamage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
         vec3_t dir, vec3_t point, int damage, int dflags, int mod, int team )
{
  if( targ->client && ( team != targ->client->ps.stats[ STAT_TEAM ] ) )
    G_Damage( targ, inflictor, attacker, dir, point, damage, dflags, mod );
}

void G_Damage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
         vec3_t dir, vec3_t point, int damage, int dflags, int mod )
{
  gclient_t *client;
  int       take;
  int       modDamge = 100;
  int       asave = 0;
  int       knockback;
  int       k;
  weapon_t  weapon = WP_NONE;
  upgrade_t upgrade = UP_NONE;
  class_t   class = PCL_NONE;

  // Can't deal damage sometimes
  if( !targ->takedamage ||
      level.intermissionQueued )
    return;

  if( inflictor ) {
    upgrade = BG_UpgradeByName( inflictor->classname )->number;
    weapon = BG_WeaponByName( inflictor->classname )->number;
  } else {
    inflictor = &g_entities[ ENTITYNUM_WORLD ];

    if( attacker && attacker->client ) {
      if( attacker->client->pers.teamSelection == TEAM_HUMANS ) {
        weapon = attacker->client->ps.weapon;
      }
      if( attacker->client->pers.teamSelection == TEAM_ALIENS ) {
        class = attacker->client->ps.stats[ STAT_CLASS ];
      }
    }
  }

  if( !attacker )
    attacker = &g_entities[ ENTITYNUM_WORLD ];

  // end damage and target protection early
  attacker->dmgProtectionTime = 0;
  attacker->targetProtectionTime = 0;

  if(
    g_friendlyFireLastSpawnProtection.integer &&
    targ->s.eType == ET_BUILDABLE) {
    if(BG_Buildable(targ->s.modelindex)->role & ROLE_SPAWN) {
      //don't allow teammates to deal damage to their last spawn
      switch (targ->buildableTeam) {
        case TEAM_ALIENS:
          if(
            level.numAlienSpawns <= 1 &&
            attacker->client &&
            attacker->client->pers.teamSelection == TEAM_ALIENS) {
            return;
          }
          break;

        case TEAM_HUMANS:
          if(
            level.numHumanSpawns <= 1 &&
            attacker->client &&
            attacker->client->pers.teamSelection == TEAM_HUMANS) {
            return;
          }
          break;

        default:
          break;
      }
    }
  }

  if((dflags & DAMAGE_INSTAGIB)) {
    if(targ->s.eType == ET_PLAYER || targ->s.eType == ET_CORPSE) {
      if( targ->health > GIB_HEALTH ) {
        damage = targ->health - GIB_HEALTH;
      }
      else {
        damage = 1;
      }
    } else if( targ->health > 0 ) {
      damage = targ->health;
    }
    else {
      damage = 1;
    }

    if(targ->client && targ->client->ps.misc[ MISC_ARMOR ] > 0) {
      damage += targ->client->ps.misc[ MISC_ARMOR ];
    }
  } else if( 
    g_damageProtection.integer &&
    targ->dmgProtectionTime > level.time &&
    attacker && attacker->s.number != ENTITYNUM_WORLD &&
    G_MODSpawnProtected( mod )) {
    damage = (int)(0.3f * (float)damage);
    if( damage < 1 ) {
      damage = 1;
    }
  }

  // shootable doors / buttons don't actually have any health
  if(targ->s.eType == ET_MOVER) {
    for( k = 0; targ->wTriggers[ k ]; ++k ) {
      if( targ->wTriggers[ k ] == weapon ) {
        return;
      }
    }

    for( k = 0; targ->uTriggers[ k ]; ++k ) {
      if( targ->uTriggers[ k ] == upgrade ) {
        return;
      }
    }

    for( k = 0; targ->cTriggers[ k ]; ++k ) {
      if( targ->cTriggers[ k ] == class ) {
        return;
      }
    }

    targ->health -= damage;
    if( targ->health > 0 ) {
      return;
    }

    if(
      targ->use && targ->moverState == MS_POS1 &&
      targ->moverMotionType != MM_MODEL) {
      if(g_debugAMP.integer) {
        char *s;
        if(attacker) {
          s = va("damaged by #%i (%s^7)", (int)(attacker-g_entities),
                  attacker < g_entities + level.maxclients ? attacker->client->pers.netname : attacker->classname );
        } else {
          s = "damaged";
        }
        G_LoggedActivation(targ, inflictor, attacker, NULL, s, LOG_ACT_USE);
      } else {
        targ->use(targ, inflictor, attacker);
      }
    }

    // FIXME
    targ->health = 1; // 1 hp hack to allow dretches to bite movers after their first use
    return;
  } else if(
    targ->s.eType == ET_MISSILE &&
    !strcmp( targ->classname, "lightningBall" ) )
  {
    //special case for detonating lightning balls
    if( mod == MOD_LIGHTNING_EMP )
      targ->die( targ, inflictor, attacker, targ->health, mod );

    return;
  } else if( mod == MOD_LIGHTNING_EMP && !damage )
    return;

  client = targ->client;
  if( client && client->noclip )
    return;

  if( !dir )
    dflags |= DAMAGE_NO_KNOCKBACK;
  else
    VectorNormalize( dir );

  knockback = BG_SU2HP( damage );

  // Some splash damage is decreased for various armor without reducing knockback
  if( targ->client && targ->client->pers.teamSelection == TEAM_HUMANS )
  {
    if( mod == MOD_LCANNON_SPLASH )
    {
      // luci splash does less damage to non-naked humans, but still deals the
      // same knockback
      if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) )
        damage *= LCANNON_SPLASH_BATTLESUIT;
      else
      {
        if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, targ->client->ps.stats ) )
          damage *= LCANNON_SPLASH_LIGHTARMOUR;
        if( BG_InventoryContainsUpgrade( UP_HELMET, targ->client->ps.stats ) )
          damage *= LCANNON_SPLASH_HELMET;
      }
    } else if( mod == MOD_GRENADE || mod == MOD_GRENADE_LAUNCHER )
    {
      if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) )
        damage *= GRENADE_SPLASH_BATTLESUIT;
    }
  }

  // Hack to ensure that old maps with trigger hurt meant to instantly kill, does so to the new bsuits.
  if( mod == MOD_TRIGGER_HURT &&
      targ->client &&
      BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) &&
    damage >= BG_HP2SU( 400 ) &&
    damage <  BG_HP2SU( 10000 ) )
    damage = BG_HP2SU( 10000 );

  if( inflictor->s.weapon != WP_NONE )
  {
    knockback = (int)( (float)knockback *
      BG_Weapon( inflictor->s.weapon )->knockbackScale );
  }

  if( targ->client )
  {
    knockback = (int)( (float)knockback *
      BG_Class( targ->client->ps.stats[ STAT_CLASS ] )->knockbackScale );
  }

  // Too much knockback from falling really far makes you "bounce" and
  //  looks silly. However, none at all also looks bad. Cap it.
  if( mod == MOD_FALLING && knockback > 50 )
    knockback = 50;

  if( knockback > 200 )
    knockback = 200;

  if( targ->flags & FL_NO_KNOCKBACK )
    knockback = 0;

  if( dflags & DAMAGE_NO_KNOCKBACK )
    knockback = 0;

  // figure momentum add, even if the damage won't be taken
  if( knockback && targ->client )
  {
    vec3_t  kvel;
    float   mass;

    mass = 200;

    VectorScale( dir, g_knockback.value * (float)knockback / mass, kvel );
    VectorAdd( targ->client->ps.velocity, kvel, targ->client->ps.velocity );

    // set the timer so that the other client can't cancel
    // out the movement immediately
    if( !targ->client->ps.pm_time )
    {
      int   t;

      t = knockback * 2;
      if( t < 50 )
        t = 50;

      if( t > 200 )
        t = 200;

      targ->client->ps.pm_time = t;
      targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
    }
  }

  // check for godmode
  if( (targ->flags & FL_GODMODE) && !(dflags & DAMAGE_GODLESS) )
    return;

  // don't do friendly fire on movement attacks
  if( ( mod == MOD_LEVEL4_TRAMPLE || mod == MOD_LEVEL3_POUNCE || 
	mod == MOD_SPITFIRE_POUNCE || mod == MOD_LEVEL4_CRUSH ) &&
      targ->s.eType == ET_BUILDABLE && targ->buildableTeam == TEAM_ALIENS )
  {
    return;
  }

  // check for completely getting out of the damage
  if( !( dflags & DAMAGE_NO_PROTECTION ) )
  {

    // if TF_NO_FRIENDLY_FIRE is set, don't do damage to the target
    // if the attacker was on the same team
    if( targ->client && targ != attacker &&
        OnSameTeam( targ, attacker ) )
    {
      // don't do friendly fire on movement attacks
      if( mod == MOD_LEVEL4_TRAMPLE || mod == MOD_LEVEL3_POUNCE ||
	       mod == MOD_SPITFIRE_POUNCE || mod == MOD_LEVEL4_CRUSH )

        return;

      // if dretchpunt is enabled and this is a dretch, do dretchpunt instead of damage
      if( g_dretchPunt.integer &&
          targ->client->ps.stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL0 )
      {
        vec3_t dir, push;

        VectorSubtract( targ->r.currentOrigin, attacker->r.currentOrigin, dir );
        VectorNormalizeFast( dir );
        VectorScale( dir, ( BG_SU2HP( damage ) * 10.0f ), push );
        push[2] = 64.0f;
        VectorAdd( targ->client->ps.velocity, push, targ->client->ps.velocity );
        return;
      }

      // don't do friendly fire on clients that can't regen
      if( targ->client->ps.stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL4 &&
            mod != MOD_HSPAWN )
      {
        return;
      }

      // check if friendly fire has been disabled or if Warmup is in progress
      if( ( !g_friendlyFire.integer &&
            mod != MOD_HSPAWN &&
            mod != MOD_ASPAWN ) ||
          IS_WARMUP )
      {
        return;
      }
      else
        modDamge = g_friendlyFire.integer;
    }

    // Battlesuit protects against all human weapons since it can't regen
    if( targ->client &&
        BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) )
    {
      switch ( mod )
      {
        case MOD_SHOTGUN:
        case MOD_BLASTER:
        case MOD_PAINSAW:
        case MOD_MACHINEGUN:
        case MOD_CHAINGUN:
        case MOD_PRIFLE:
        case MOD_MDRIVER:
        case MOD_LASGUN:
        case MOD_LCANNON:
        case MOD_FLAMER:
        case MOD_FLAMER_SPLASH:
        case MOD_LIGHTNING:
        case MOD_TESLAGEN:
        case MOD_MGTURRET:
        case MOD_REACTOR:
          return;
      }
    }

    if( targ->s.eType == ET_BUILDABLE && attacker->client &&
        mod != MOD_DECONSTRUCT && mod != MOD_SUICIDE &&
        mod != MOD_REPLACE && mod != MOD_NOCREEP )
    {
      if( targ->buildableTeam == attacker->client->pers.teamSelection )
      {
        if( ( !g_friendlyBuildableFire.integer ||
              ( IS_WARMUP && !g_warmupFriendlyBuildableFire.integer ) ) &&
            ( mod != MOD_TRIGGER_HURT ) )
          return;
        else
          modDamge = g_friendlyBuildableFire.integer;
      }

      // base is under attack warning if DCC'd
      if( targ->buildableTeam == TEAM_HUMANS && G_FindDCC( targ ) &&
          level.time > level.humanBaseAttackTimer )
      {
        level.humanBaseAttackTimer = level.time + DC_ATTACK_PERIOD;
        G_BroadcastEvent( EV_DCC_ATTACK, 0 );
      }
    }
  }

  // add to the attacker's hit counter
  if( attacker->client && targ != attacker && targ->health > 0
      && targ->s.eType != ET_MISSILE
      && targ->s.eType != ET_GENERAL
      && G_MODHitIndication( mod ) )
  {
    if( OnSameTeam( targ, attacker ) )
      attacker->client->ps.persistant[ PERS_HITS ]--;
    else
      attacker->client->ps.persistant[ PERS_HITS ]++;
  }

  // give scaled down damage during evolving
  if( attacker->client &&
      attacker->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS &&
      ( attacker->client->ps.eFlags & EF_EVOLVING ) )
  {
    modDamge = (int)( ( (float)modDamge ) * BG_EvolveScale( &attacker->client->ps ) );
  }

  // receive scaled up damage during evolving
  if( targ->client &&
      targ->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS &&
      ( targ->client->ps.eFlags & EF_EVOLVING ) )
  {
    modDamge = (int)( ( (float)modDamge ) / BG_EvolveScale( &targ->client->ps ) );
  }

  if( targ->s.eType == ET_BUILDABLE &&
      ( BG_Buildable( targ->s.modelindex )->ballisticDmgMod != 1.0 ) ) {
    switch( mod ) {
      case MOD_SHOTGUN:
      case MOD_MACHINEGUN:
      case MOD_CHAINGUN:
      case MOD_FRAGNADE:
      case MOD_LEVEL3_BOUNCEBALL:
        modDamge = (int)( ( (float)modDamge ) * BG_Buildable( targ->s.modelindex )->ballisticDmgMod );
        break;

      default:
        break;
    }
  }

  if( modDamge != 100 &&
      !(dflags & DAMAGE_INSTAGIB) )
  {
    if( damage < ( INT_MAX / modDamge ) )
      take = ( modDamge * damage ) / 100;
    else
      take = (int)( ((float)damage) * (((float)modDamge) / 100.0f) );
  } else
    take = damage;

  // add to the damage inflicted on a player this frame
  // the total will be turned into screen blends and view angle kicks
  // at the end of the frame
  if( client )
  {
    if( attacker )
      client->ps.persistant[ PERS_ATTACKER ] = attacker->s.number;
    else
      client->ps.persistant[ PERS_ATTACKER ] = ENTITYNUM_WORLD;

    client->damage_armor += asave;
    client->damage_blood += take;
    client->damage_knockback += knockback;

    if( dir )
    {
      VectorCopy ( dir, client->damage_from );
      client->damage_fromWorld = qfalse;
    }
    else
    {
      VectorCopy ( targ->r.currentOrigin, client->damage_from );
      client->damage_fromWorld = qtrue;
    }

    // set the last client who damaged the target
    targ->client->lasthurt_client = attacker->s.number;
    targ->client->lasthurt_mod = mod;
    
    if( !(dflags & DAMAGE_INSTAGIB) )
    {
      float damageMod = G_CalcDamageModifier( point, targ, attacker,
                                                 client->ps.stats[ STAT_CLASS ],
                                                 dflags );

      if( modDamge >= 1.0 )
      {
        if( take < ( ( INT_MAX - 1 ) / damageMod ) )
          take = (int)( ( take * damageMod ) + 0.5f );
      }
    }

    //if boosted poison every attack
    if( attacker->client && attacker->client->ps.stats[ STAT_STATE ] & SS_BOOSTED )
    {
      if( targ->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
          !BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) &&
          mod != MOD_LEVEL2_ZAP && mod != MOD_POISON &&
          mod != MOD_HSPAWN &&
          mod != MOD_SPITFIRE_ZAP &&
          mod != MOD_SPITFIRE_POUNCE &&
          mod != MOD_LEVEL3_POUNCE &&
          mod != MOD_LEVEL4_TRAMPLE &&
          mod != MOD_ASPAWN && targ->client->poisonImmunityTime < level.time )
      {
        targ->client->ps.stats[ STAT_STATE ] |= SS_POISONED;
        targ->client->lastPoisonTime = level.time;
        targ->client->lastPoisonClient = attacker;
      }
    }
  }

  if( take < 1 )
    take = 1;

  if( g_debugDamage.integer )
  {
    Com_Printf( "%i: client:%i health:%i damage:%i armor:%i\n", level.time, targ->s.number,
      targ->health, take, asave );
  }

  // do the damage
  if( take || (dflags & DAMAGE_INSTAGIB) )
  {
    int deathHealth;

    // Battlesuit absorbs the damage
    if( targ->client &&
        !(dflags & DAMAGE_NO_ARMOR) &&
        BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) &&
        mod != MOD_POISON &&
        mod != MOD_WATER )
    {
      // add to the attackers "account" on the target
      if( attacker != targ )
      {
        if( attacker->client )
          targ->creditsUpgrade[ UP_BATTLESUIT ][ attacker->client->ps.clientNum ] += take;
        else if( attacker->s.eType == ET_BUILDABLE )
        {
          if( G_Entity_id_get( &targ->creditsUpgradeDeffenses[ UP_BATTLESUIT ][ attacker->s.number ].id ) == attacker )
          {
            G_IncreaseDamageCredits( &targ->creditsUpgradeDeffenses[ UP_BATTLESUIT ][ attacker->s.number ].credits,
                                     take );
          } else
          {
            G_Entity_id_set( &targ->creditsUpgradeDeffenses[ UP_BATTLESUIT ][ attacker->s.number ].id, attacker );
            G_IncreaseDamageCredits( &targ->creditsUpgradeDeffenses[ UP_BATTLESUIT ][ attacker->s.number ].credits,
                                     take );
          }
        }
      }

      targ->client->ps.misc[ MISC_ARMOR ] -= take;
      take = 0;
      if( targ->client->ps.misc[ MISC_ARMOR ] <= 0 ||
          (dflags & DAMAGE_INSTAGIB) )
      {
        gentity_t *tent;
        vec3_t newOrigin;
        const weapon_t weapon = targ->client->ps.stats[ STAT_WEAPON ];

        // have damage that is greater than the remaining armor transfer to the main helth
        take -= targ->client->ps.misc[ MISC_ARMOR ];

        if(dflags & DAMAGE_INSTAGIB)
        {
          targ->client->ps.misc[ MISC_ARMOR ] = 0;
          if( take <= 0)
          {
            if( targ->health > GIB_HEALTH )
              take = targ->health - GIB_HEALTH;
            else
              take = 1;
          }
        }

        // Break the BSuit off into pieces
        G_RoomForClassChange( targ, PCL_HUMAN, newOrigin );
        VectorCopy( newOrigin, targ->client->ps.origin );
        targ->client->ps.stats[ STAT_CLASS ] = PCL_HUMAN;
        targ->client->pers.classSelection = PCL_HUMAN;
        targ->client->ps.eFlags ^= EF_TELEPORT_BIT;
        BG_RemoveUpgradeFromInventory( UP_BATTLESUIT,
                                       targ->client->ps.stats );
        tent = G_TempEntity( targ->client->ps.origin, EV_GIB_BSUIT );
        BG_GetClientNormal( &targ->client->ps, tent->s.origin2 );
        targ->client->ps.misc[ MISC_ARMOR ] = 0;
        targ->client->ps.stats[ STAT_FLAGS ] &= ~SFL_ARMOR_GENERATE;
        targ->client->lastArmorGenTime = 0;
        targ->client->armorToGen = 0;
        targ->client->armorGenIncrementTime = 0;

        // Give income for destroying the battlesuit
        G_RewardAttackers( targ, UP_BATTLESUIT );

        //adjust ammo and clips
        if( BG_Weapon( weapon )->usesEnergy &&
            targ->client->ps.ammo > BG_Weapon( weapon )->maxAmmo )
          G_GiveClientMaxAmmo( targ, qtrue );

        if( !BG_Weapon( targ->client->ps.weapon )->usesEnergy &&
            !BG_Weapon( targ->client->ps.weapon )->infiniteAmmo &&
            BG_Weapon( targ->client->ps.weapon )->ammoPurchasable &&
            !BG_Weapon( targ->client->ps.weapon )->roundPrice &&
            targ->client->ps.clips > BG_Weapon( targ->client->ps.weapon )->maxClips )
          targ->client->ps.clips = BG_Weapon( targ->client->ps.weapon )->maxClips;

        //update ClientInfo
        ClientUserinfoChanged( targ->client->ps.clientNum, qfalse );
        targ->client->pers.infoChangeTime = level.time;

        // Apply any remaining damage after the bsuit breaks off
        if( take )
          G_Damage( targ, inflictor, attacker, dir, point, damage, dflags, mod );
        else
        {
          targ->lastDamageTime = level.time;
          targ->client->lastMedKitTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;
          targ->client->lastBioKitTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;
          targ->client->lastArmorGenTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;

          if( targ->pain )
            targ->pain( targ, attacker, take );
        }

        return;
      } else
      {
        targ->lastDamageTime = level.time;
        targ->client->lastMedKitTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;
        targ->client->lastBioKitTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;
        targ->client->lastArmorGenTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;

        //update ClientInfo
        ClientUserinfoChanged( targ->client->ps.clientNum, qfalse );
        targ->client->pers.infoChangeTime = level.time;

        if( targ->pain )
          targ->pain( targ, attacker, take );

        return;
      }
    }

    // add to the attackers "account" on the target
    if( attacker != targ )
    {
      if( attacker->client )
      {
        G_IncreaseDamageCredits( &targ->credits[ attacker->client->ps.clientNum ],
                                 take );
      }
      else if( attacker->s.eType == ET_BUILDABLE )
      {
        if( G_Entity_id_get( &targ->creditsDeffenses[ attacker->s.number ].id ) == attacker )
        {
          G_IncreaseDamageCredits( &targ->creditsDeffenses[ attacker->s.number ].credits,
                                   take );
        } else
        {
          G_Entity_id_set( &targ->creditsDeffenses[ attacker->s.number ].id, attacker );
          G_IncreaseDamageCredits( &targ->creditsDeffenses[ attacker->s.number ].credits,
                                   take );
        }
      }
    }

    if( !(dflags & DAMAGE_INSTAGIB) )
    {
      const int currentHealthBeforeDamage = targ->health;

      G_ChangeHealth( targ, attacker, ( - take ), 0 );

      //adjust health scaling for evolving
      if( targ->client &&
          targ->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS &&
          (targ->client->ps.eFlags & EF_EVOLVING) &&
          targ->client->ps.stats[ STAT_MISC2 ] )
      {
        int projectedHealth = currentHealthBeforeDamage +
                              targ->client->pers.evolveHealthRegen;
        const float damageScaleMod = ( (float)projectedHealth ) /
                                     ( (float)BG_Class( targ->client->ps.stats[ STAT_CLASS ] )->health );

        projectedHealth -= ( (int)( damageScaleMod * ( (float)take ) ) );

        targ->client->pers.evolveHealthRegen = projectedHealth - targ->health;
      }
    }
    else
    {
      if( targ->s.eType == ET_PLAYER ||
          targ->s.eType == ET_CORPSE )
        G_ChangeHealth( targ, attacker, GIB_HEALTH,
                        (HLTHF_SET_TO_CHANGE|
                         HLTHF_EVOLVE_INCREASE) );
      else
      G_ChangeHealth( targ, attacker, 0,
                      (HLTHF_SET_TO_CHANGE|
                       HLTHF_EVOLVE_INCREASE) );
    }

    targ->lastDamageTime = level.time;
    if( targ->client ) {
      targ->client->lastMedKitTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;
      targ->client->lastBioKitTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;
      targ->client->lastArmorGenTime += HUMAN_DAMAGE_HEAL_DELAY_TIME;
      targ->client->ps.misc[ MISC_LAST_DAMAGE_TIME ] = level.time;
    }
    if( !targ->client ||
        !( targ->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS &&
           targ->client->ps.eFlags & EF_EVOLVING &&
           targ->client->pers.evolveHealthRegen &&
           targ->client->ps.stats[ STAT_MISC3 ] > 0 ) )
      targ->nextRegenTime = level.time + ALIEN_REGEN_DAMAGE_TIME;
    targ->nextHPReserveRegenTime = level.time + ALIEN_REGEN_DAMAGE_TIME;

    if ( targ->s.eType == ET_CORPSE ) {
      deathHealth = GIB_HEALTH;
    } else {
      deathHealth = 0;
    }

    if( ( targ->health <= deathHealth ||
          (dflags & DAMAGE_INSTAGIB) ) &&
        targ != G_Entity_id_get( &targ->idAtLastDeath ) )
    {
      if( client )
        targ->flags |= FL_NO_KNOCKBACK;

      if( targ->health < BG_HP2SU( -999 ) )
      {
        G_ChangeHealth( targ, attacker, BG_HP2SU( -999 ),
                        (HLTHF_SET_TO_CHANGE|
                         HLTHF_EVOLVE_INCREASE) );
      }

      if( targ->s.eType == ET_BUILDABLE &&
          !targ->spawned )
          level.numUnspawnedBuildables[ targ->buildableTeam ]--;

      targ->enemy = attacker;
      G_Entity_id_set( &targ->idAtLastDeath, targ );
      targ->die( targ, inflictor, attacker, take, mod );
      if(
        targ->s.eType == ET_BUILDABLE &&
        BG_Buildable(targ->s.modelindex)->role & ROLE_SPAWN) {
        G_CountSpawns( );
      }
      if( ( targ->activation.flags & ACTF_OCCUPY ) &&
          ( targ->flags & FL_OCCUPIED ) &&
          targ->occupation.occupant && targ->occupation.occupant->client )
        G_UnoccupyEnt( targ, targ->occupation.occupant, targ->occupation.occupant, qtrue );

      if(
        targ->s.eType == ET_BUILDABLE &&
        targ->nextthink == level.time) {
        G_BuildableThink( targ, 0 );
        G_RunThink(targ);
      }
      return;
    }
    else if( targ->pain )
      targ->pain( targ, attacker, take );
  }
}


/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
qboolean CanDamage( gentity_t *targ, vec3_t origin,
                    vec3_t mins, vec3_t maxs )
{
  vec3_t         dest;
  trace_t        tr;
  vec3_t         midpoint;
  bboxPoint_t    sourcePoint;

  Com_Assert( targ &&
              "CanDamage: targ is NULL" );
  Com_Assert( origin &&
              "CanDamage: origin is NULL" );

  // use the midpoint of the bounds instead of the origin, because
  // bmodels may have their origin is 0,0,0
  VectorAdd( targ->r.absmin, targ->r.absmax, midpoint );
  VectorScale( midpoint, 0.5, midpoint );

  sourcePoint.num = 0;

  do
  {
    //check from the different source points
    BG_EvaluateBBOXPoint( &sourcePoint, origin,
                     mins, maxs );

    VectorCopy( midpoint, dest );
    SV_Trace( &tr, sourcePoint.point, NULL, NULL, dest, ENTITYNUM_NONE,
      MASK_SOLID, TT_AABB );
    if( tr.fraction == 1.0  || tr.entityNum == targ->s.number )
      return qtrue;

    // this should probably check in the plane of projection,
    // rather than in world coordinate, and also include Z
    VectorCopy( midpoint, dest );
    dest[ 0 ] += 15.0;
    dest[ 1 ] += 15.0;
    SV_Trace( &tr, sourcePoint.point, NULL, NULL, dest, ENTITYNUM_NONE,
      MASK_SOLID, TT_AABB );
    if( tr.fraction == 1.0 )
      return qtrue;

    VectorCopy( midpoint, dest );
    dest[ 0 ] += 15.0;
    dest[ 1 ] -= 15.0;
    SV_Trace( &tr, sourcePoint.point, NULL, NULL, dest, ENTITYNUM_NONE,
      MASK_SOLID, TT_AABB );
    if( tr.fraction == 1.0 )
      return qtrue;

    VectorCopy( midpoint, dest );
    dest[ 0 ] -= 15.0;
    dest[ 1 ] += 15.0;
    SV_Trace( &tr, sourcePoint.point, NULL, NULL, dest, ENTITYNUM_NONE,
      MASK_SOLID, TT_AABB );
    if( tr.fraction == 1.0 )
      return qtrue;

    VectorCopy( midpoint, dest );
    dest[ 0 ] -= 15.0;
    dest[ 1 ] -= 15.0;
    SV_Trace( &tr, sourcePoint.point, NULL, NULL, dest, ENTITYNUM_NONE,
      MASK_SOLID, TT_AABB );
    if( tr.fraction == 1.0 )
      return qtrue;

    // check only the origin if mins or maxs are NULL
    if( !mins || !maxs )
      break;

    sourcePoint.num++;
  } while( sourcePoint.num < NUM_BBOX_POINTS );

  return qfalse;
}

/*
============
G_SelectiveRadiusDamage
============
*/
qboolean G_SelectiveRadiusDamage( vec3_t origin, vec3_t originMins, vec3_t originMaxs,
                                  gentity_t *attacker, float damage,
                                  float radius, gentity_t *ignore, int mod, int team,
                                  qboolean knockback )
{
  float     points, dist;
  gentity_t *ent;
  int       entityList[ MAX_GENTITIES ];
  int       numListedEntities;
  vec3_t    mins, maxs;
  vec3_t    v;
  vec3_t    dir;
  int       i, e;
  qboolean  hitClient = qfalse;

  if( radius < 1 )
    radius = 1;

  for( i = 0; i < 3; i++ )
  {
    mins[ i ] = origin[ i ] - radius;
    maxs[ i ] = origin[ i ] + radius;
  }

  numListedEntities = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );

  for( e = 0; e < numListedEntities; e++ )
  {
    ent = &g_entities[ entityList[ e ] ];

    if( ent == ignore )
      continue;

    if(!ent->takedamage)
    continue;

    if( G_NoTarget( ent ) )
      continue;

    if(ent->r.contents & CONTENTS_ASTRAL_NOCLIP) {
      continue;
    }

    // find the distance from the edge of the bounding box
    for( i = 0 ; i < 3 ; i++ )
    {
      if( origin[ i ] < ent->r.absmin[ i ] )
        v[ i ] = ent->r.absmin[ i ] - origin[ i ];
      else if( origin[ i ] > ent->r.absmax[ i ] )
        v[ i ] = origin[ i ] - ent->r.absmax[ i ];
      else
        v[ i ] = 0;
    }

    dist = VectorLength( v );
    if( dist >= radius )
      continue;

    points = damage * ( 1.0 - dist / radius );
    // don't let the damage be less than half of one health point unit
    points = MAX( points, ( BG_HP2SU( 1 ) / 2 ) );
    // ensure that damage is always done when in range
    points = MAX( points, 1 );

    if( ( ( ent->client && ent->client->ps.stats[ STAT_TEAM ] != team ) ||
          ( ent->s.eType == ET_BUILDABLE && ent->buildableTeam != team ) ||
          ( mod == MOD_REACTOR && ent->s.eType == ET_TELEPORTAL ) ) &&
        CanDamage( ent, origin, originMins, originMaxs ) )
    {
      int dflags = (DAMAGE_RADIUS|DAMAGE_NO_LOCDAMAGE);
  
      if( !knockback )
        dflags |= DAMAGE_NO_KNOCKBACK;

      VectorSubtract( ent->r.currentOrigin, origin, dir );
      // push the center of mass higher than the origin so players
      // get knocked into the air more
      dir[ 2 ] += 24;
      hitClient = qtrue;
      G_Damage( ent, NULL, attacker, dir, origin,
                (int)points ? (int)points : 1,
                dflags, mod );
    }
  }

  return hitClient;
}

/*
============
G_Knockback
============
*/
void G_Knockback( gentity_t *targ, vec3_t dir, int knockback )
{
  if( knockback && targ->client )
  {
    vec3_t  kvel;
    float   mass;

    mass = 200;

    // Halve knockback for bsuits
    if( targ->client &&
        targ->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
        BG_InventoryContainsUpgrade( UP_BATTLESUIT, targ->client->ps.stats ) )
      mass += 400;

    // Halve knockback for crouching players
    if(targ->client->ps.pm_flags&PMF_DUCKED) knockback /= 2;

    VectorScale( dir, g_knockback.value * (float)knockback / mass, kvel );
    VectorAdd( targ->client->ps.velocity, kvel, targ->client->ps.velocity );

    // set the timer so that the other client can't cancel
    // out the movement immediately
    if( !targ->client->ps.pm_time )
    {
      int   t;
      t = knockback * 2;
      if( t < 50 )
        t = 50;

      if( t > 200 )
        t = 200;
      targ->client->ps.pm_time = t;
      targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
    }
  }
}

/*
============
G_RadiusDamage
============
*/
qboolean G_RadiusDamage( vec3_t origin, vec3_t originMins, vec3_t originMaxs,
                         gentity_t *attacker, float damage,
                         float radius, gentity_t *ignore, int mod, qboolean knockback )
{
  float     points, dist, shake;
  gentity_t *ent;
  int       entityList[ MAX_GENTITIES ];
  int       numListedEntities;
  vec3_t    mins, maxs;
  vec3_t    v;
  vec3_t    dir;
  int       i, e;
  qboolean  hitClient = qfalse;

  if( radius < 1 )
    radius = 1;

  for( i = 0; i < 3; i++ )
  {
    mins[ i ] = origin[ i ] - radius;
    maxs[ i ] = origin[ i ] + radius;
  }

  numListedEntities = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );

  for( e = 0; e < numListedEntities; e++ )
  {
    ent = &g_entities[ entityList[ e ] ];

    if( ent == ignore )
      continue;

      if(!ent->takedamage)
      continue;

    if( ent->client && ( ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING ) )
      continue;

    // find the distance from the edge of the bounding box
    for( i = 0; i < 3; i++ )
    {
      if( origin[ i ] < ent->r.absmin[ i ] )
        v[ i ] = ent->r.absmin[ i ] - origin[ i ];
      else if( origin[ i ] > ent->r.absmax[ i ] )
        v[ i ] = origin[ i ] - ent->r.absmax[ i ];
      else
        v[ i ] = 0;
    }

    dist = VectorLength( v );
    if( dist >= radius )
      continue;

    if( ( mod == MOD_LIGHTNING_EMP ||
          mod == MOD_LIGHTNING ) &&
        attacker->s.number == ent->s.number )
      continue;

    points = damage * ( 1.0 - dist / radius );
    // don't let the damage be less than half of one health point unit
    points = MAX( points, ( BG_HP2SU( 1 ) / 2 ) );
    // ensure that damage is always done when in range
    points = MAX( points, 1 );

    if( CanDamage( ent, origin, originMins, originMaxs ) )
    {
      int dflags = (DAMAGE_RADIUS|DAMAGE_NO_LOCDAMAGE);
  
      if( !knockback )
        dflags |= DAMAGE_NO_KNOCKBACK;

      VectorSubtract( ent->r.currentOrigin, origin, dir );
      // push the center of mass higher than the origin so players
      // get knocked into the air more
      dir[ 2 ] += 24;
      hitClient = qtrue;
      G_Damage( ent, NULL, attacker, dir, origin,
                (int)points ? (int)points : 1,
                dflags, mod );
    }
  }

  for( i = 0; i < 3; i++ )
  {
    mins[ i ] = origin[ i ] - radius * 2;
    maxs[ i ] = origin[ i ] + radius * 2;
  }

  numListedEntities = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );

  for( e = 0; e < numListedEntities; e++ )
  {
    ent = g_entities + entityList[ e ];

    if( ent == ignore )
      continue;

    if( !ent->client )
      continue;

      if( !ent->takedamage )
      continue;

    shake = damage * 10 / Distance( origin, ent->r.currentOrigin );
    ent->client->ps.stats[ STAT_SHAKE ] += BG_SU2HP( (int) shake );
  }

  return hitClient;
}

/*
================
G_LogDestruction

Log deconstruct/destroy events
================
*/
void G_LogDestruction( gentity_t *self, gentity_t *actor, int mod )
{
  buildFate_t fate;

  switch( mod )
  {
    case MOD_DECONSTRUCT:
      fate = BF_DECONSTRUCT;
      break;
    case MOD_REPLACE:
      fate = BF_REPLACE;
      break;
    case MOD_NOCREEP:
      fate = ( actor->client ) ? BF_UNPOWER : BF_AUTO;
      break;
    default:
      if( actor->client )
      {
        if( actor->client->pers.teamSelection ==
            BG_Buildable( self->s.modelindex )->team )
        {
          fate = BF_TEAMKILL;
        }
        else
          fate = BF_DESTROY;
      }
      else if( actor->s.eType == ET_BUILDABLE )
      {
        if( actor->buildableTeam == 
            BG_Buildable( self->s.modelindex )->team )
        {
          fate = BF_TEAMKILL;
        }
        else
          fate = BF_DESTROY;
      }
      else
        fate = BF_AUTO;
      break;
  }
  G_BuildLogAuto( actor, self, fate );

  // don't log when marked structures are removed
  if( mod == MOD_REPLACE )
    return;

  G_LogPrintf( S_COLOR_YELLOW "Deconstruct: %d %d %s %s: %s %s by %s%s\n",
    (int)( actor - g_entities ),
    (int)( self - g_entities ),
    BG_Buildable( self->s.modelindex )->name,
    modNames[ mod ],
    BG_Buildable( self->s.modelindex )->humanName,
    mod == MOD_DECONSTRUCT ? "deconstructed" : "destroyed",
    actor->client ?
        actor->client->pers.netname :
        ( actor->s.eType == ET_BUILDABLE ?
            va( "some %s%s", actor->builtBy ? "" : "default ",
                BG_Buildable( actor->s.modelindex )->humanName ) :
            "<world>" ),
    ( actor->s.eType == ET_BUILDABLE && actor->builtBy ) ?
                    va( " built by %s", actor->builtBy->name[ actor->builtBy->nameOffset ] ) : "" );

  // No-power deaths for humans come after some minutes and it's confusing
  //  when the messages appear attributed to the deconner. Just don't print them.
  if( mod == MOD_NOCREEP && actor->client &&
      actor->client->pers.teamSelection == TEAM_HUMANS )
    return;

  if( actor->client && actor->client->pers.teamSelection ==
    BG_Buildable( self->s.modelindex )->team )
  {
    G_TeamCommand( actor->client->ps.stats[ STAT_TEAM ],
      va( "print \"%s ^3%s^7 by %s\n\"",
        BG_Buildable( self->s.modelindex )->humanName,
        mod == MOD_DECONSTRUCT ? "DECONSTRUCTED" : "DESTROYED",
        actor->client->pers.netname ) );
  }

  // warn if a teammate builder might be feeding friendly buildables to enemy buildables
  if( actor->s.eType == ET_BUILDABLE &&
      self->buildLog &&
      ( !actor->buildLog ||
        actor->buildLog->time < self->buildLog->time ) )
  {
    G_TeamCommand( self->buildableTeam,
      va( "print \"%s (built^7 by %s^7) was ^3DESTROYED^7 by some %s\n\"",
        BG_Buildable( self->s.modelindex )->humanName,
        self->builtBy->name[ self->builtBy->nameOffset ],
        BG_Buildable( actor->s.modelindex )->humanName ) );
  }
}
