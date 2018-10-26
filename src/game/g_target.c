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

//==========================================================

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8)
"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
*/
void Think_Target_Delay( gentity_t *ent )
{
  if( ent->activator && !ent->activator->inuse )
    ent->activator = NULL;
  G_UseTargets( ent, ent->activator );
}

void Use_Target_Delay( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
  ent->nextthink = level.time + ( ent->wait + ent->random * crandom( ) ) * 1000;
  ent->think = Think_Target_Delay;
  ent->activator = activator;
}

void SP_target_delay( gentity_t *ent )
{
  // check delay for backwards compatability
  if( !G_SpawnFloat( "delay", "0", &ent->wait ) )
    G_SpawnFloat( "wait", "1", &ent->wait );

  if( !ent->wait )
    ent->wait = 1;

  ent->use = Use_Target_Delay;
}


//==========================================================

/*QUAKED target_score (1 0 0) (-8 -8 -8) (8 8 8)
"count" number of points to add, default 1

The activator is given this many points.
*/
void Use_Target_Score( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
  if( !activator )
    return;

  AddScore( activator, ent->count );
}

void SP_target_score( gentity_t *ent )
{
  if( !ent->count )
    ent->count = 1;

  ent->use = Use_Target_Score;
}


//==========================================================

/*QUAKED target_print (1 0 0) (-8 -8 -8) (8 8 8) humanteam alienteam private
"message" text to print
If "private", only the activator gets the message.  If no checks, all clients get the message.
*/
void Use_Target_Print( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
  if( ent->spawnflags & 4 )
  {
    if( activator && activator->client &&
        G_Expired( activator, EXP_MAP_PRINT ) )
    {
      SV_GameSendServerCommand( activator-g_entities,
                              va( "cp \"%s\" %d",
                                  ent->message, CP_MAP ) );
      G_SetExpiration( activator, EXP_MAP_PRINT, 500 );
    }
    return;
  }

  if( !G_Expired( ent, EXP_MAP_PRINT ) )
    return;

  G_SetExpiration( ent, EXP_MAP_PRINT, 500 );

  if( ent->spawnflags & 3 )
  {
    if( ent->spawnflags & 1 )
      G_TeamCommand( TEAM_HUMANS, va( "cp \"%s\" %d", ent->message, CP_MAP ) );
    if( ent->spawnflags & 2 )
      G_TeamCommand( TEAM_ALIENS, va( "cp \"%s\" %d", ent->message, CP_MAP ) );

    return;
  }

  SV_GameSendServerCommand( -1, va("cp \"%s\" %d", ent->message, CP_MAP ) );
}

void SP_target_print( gentity_t *ent )
{
  ent->use = Use_Target_Print;
}


//==========================================================


/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off global activator
"noise"   wav file to play

A global sound will play full volume throughout the level.
Activator sounds will play on the player that activated the target.
Global and activator sounds can't be combined with looping.
Normal sounds play each time the target is used.
Looped sounds will be toggled by use functions.
Multiple identical looping sounds will just increase volume without any speed cost.
"wait" : Seconds between auto triggerings, 0 = don't auto trigger
"random"  wait variance, default is 0
*/
void Use_Target_Speaker( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
  if( ent->spawnflags & 3 )
  {  // looping sound toggles
    if( ent->s.loopSound ) {
       ent->s.loopSound = 0; // turn it off
      G_FreeEntity(ent);
    } else {
      ent->s.loopSound = ent->noise_index;  // start it
      SV_LinkEntity(ent);
    }
  }
  else
  {
    // normal sound
    if( ent->spawnflags & 8 && activator )
      G_AddEvent( activator, EV_GENERAL_SOUND, ent->noise_index );
    else if( ent->spawnflags & 4 )
      G_AddEvent( ent, EV_GLOBAL_SOUND, ent->noise_index );
    else
      G_AddEvent( ent, EV_GENERAL_SOUND, ent->noise_index );
  }
}

void SP_target_speaker( gentity_t *ent )
{
  char  buffer[ MAX_QPATH ];
  char  *s;

  G_SpawnFloat( "wait", "0", &ent->wait );
  G_SpawnFloat( "random", "0", &ent->random );

  if( !G_SpawnString( "noise", "NOSOUND", &s ) )
    Com_Error( ERR_DROP, "target_speaker without a noise key at %s", vtos( ent->r.currentOrigin ) );

  // force all client relative sounds to be "activator" speakers that
  // play on the entity that activates it
  if( s[ 0 ] == '*' )
    ent->spawnflags |= 8;

  if( !strstr( s, ".wav" ) )
    Com_sprintf( buffer, sizeof( buffer ), "%s.wav", s );
  else
    Q_strncpyz( buffer, s, sizeof( buffer ) );

  ent->noise_index = G_SoundIndex( buffer );

  // a repeating speaker can be done completely client side
  ent->s.eType = ET_SPEAKER;
  ent->s.eventParm = ent->noise_index;
  ent->s.frame = ent->wait * 10;
  ent->s.clientNum = ent->random * 10;


  // check for prestarted looping sound
  if( ent->spawnflags & 1 )
    ent->s.loopSound = ent->noise_index;

  ent->use = Use_Target_Speaker;

  if( ent->spawnflags & 4 )
    ent->r.svFlags |= SVF_BROADCAST;

  // must link the entity so we get areas and clusters so
  // the server can determine who to send updates to
  SV_LinkEntity( ent );
}

//==========================================================

void target_teleporter_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
  gentity_t *dest;

  if( !activator || !activator->client )
    return;

  dest =  G_PickTarget( self->target );

  if( !dest )
  {
    Com_Printf( "Couldn't find teleporter destination\n" );
    return;
  }

  TeleportPlayer( activator, dest->r.currentOrigin, dest->r.currentAngles, self->speed );
}

/*QUAKED target_teleporter (1 0 0) (-8 -8 -8) (8 8 8)
The activator will be teleported away.
*/
void SP_target_teleporter( gentity_t *self )
{
  if( !self->targetname )
    Com_Printf( "untargeted %s at %s\n", self->classname, vtos( self->r.currentOrigin ) );

  G_SpawnFloat( "speed", "400", &self->speed );

  self->use = target_teleporter_use;
}

//==========================================================


/*QUAKED target_relay (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM
This doesn't perform any actions except fire its targets.
The activator can be forced to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them
*/
void target_relay_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
  if( ( self->spawnflags & 1 ) && activator && activator->client &&
      activator->client->ps.stats[ STAT_TEAM ] != TEAM_HUMANS )
    return;

  if( ( self->spawnflags & 2 ) && activator && activator->client &&
      activator->client->ps.stats[ STAT_TEAM ] != TEAM_ALIENS )
    return;

  if(self->spawnflags & 4) {
    gentity_t *ent;

    ent = G_PickTarget(self->target);
    if(ent && ent->use) {
      if(g_debugAMP.integer) {
        G_LoggedActivation(ent, self, activator, NULL, "randomly relayed", LOG_ACT_USE);
      } else {
        ent->use(ent, self, activator);
      }
    }

    return;
  }

  G_UseTargets( self, activator );
}

void SP_target_relay( gentity_t *self )
{
  self->use = target_relay_use;
}


//==========================================================

/*QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8)
Kills the activator.
*/
void target_kill_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
  if( !activator )
    return;

  G_Damage( activator, NULL, NULL, NULL, NULL, 0, DAMAGE_INSTAGIB|DAMAGE_NO_PROTECTION, MOD_TELEFRAG );
}

void SP_target_kill( gentity_t *self )
{
  self->use = target_kill_use;
}

/*QUAKED target_position (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
*/
void SP_target_position( gentity_t *self )
{
  G_SetOrigin( self, self->r.currentOrigin );
}

/*QUAKED target_location (0 0.5 0) (-8 -8 -8) (8 8 8)
Set "message" to the name of this location.
Set "count" to 0-7 for color.
0:white 1:red 2:green 3:yellow 4:blue 5:cyan 6:magenta 7:white

Closest target_location in sight used for the location, if none
in site, closest in distance
*/
void SP_target_location( gentity_t *self )
{
  static int n = 0;
  char *message;
  self->s.eType = ET_LOCATION;
  self->r.svFlags = SVF_BROADCAST;
  SV_LinkEntity( self ); // make the server send them to the clients
  if( n == MAX_LOCATIONS )
  {
    Com_Printf( S_COLOR_YELLOW "too many target_locations\n" );
    return;
  }
  if( self->count )
  {
    if( self->count < 0 )
      self->count = 0;

    if( self->count > 7 )
      self->count = 7;

    message = va( "%c%c%s" S_COLOR_WHITE, Q_COLOR_ESCAPE, self->count + '0',
      self->message);
  }
  else
    message = self->message;
  SV_SetConfigstring( CS_LOCATIONS + n, message );
  self->nextTrain = level.locationHead;
  self->s.generic1 = n; // use for location marking
  level.locationHead = self;
  n++;

  G_SetOrigin( self, self->r.currentOrigin );
}


/*
===============
target_rumble_think
===============
*/
void target_rumble_think( gentity_t *self )
{
  int        i;
  gentity_t  *ent;

  if( self->last_move_time < level.time )
    self->last_move_time = level.time + 0.5;

  for( i = 0, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if( !ent->inuse )
      continue;

    if( !ent->client )
      continue;

    if( ent->client->ps.groundEntityNum == ENTITYNUM_NONE )
      continue;

    ent->client->ps.groundEntityNum = ENTITYNUM_NONE;
    ent->client->ps.velocity[ 0 ] += crandom( ) * 150;
    ent->client->ps.velocity[ 1 ] += crandom( ) * 150;
    ent->client->ps.velocity[ 2 ] = self->speed;
  }

  if( level.time < self->timestamp )
    self->nextthink = level.time + FRAMETIME;
}

/*
===============
target_rumble_use
===============
*/
void target_rumble_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
  self->timestamp = level.time + ( self->count * FRAMETIME );
  self->nextthink = level.time + FRAMETIME;
  self->activator = activator;
  self->last_move_time = 0;
}

/*
===============
SP_target_rumble
===============
*/
void SP_target_rumble( gentity_t *self )
{
  if( !self->targetname )
  {
    Com_Printf( S_COLOR_YELLOW "WARNING: untargeted %s at %s\n", self->classname,
                                                               vtos( self->r.currentOrigin ) );
  }

  if( !self->count )
    self->count = 10;

  if( !self->speed )
    self->speed = 100;

  self->think = target_rumble_think;
  self->use = target_rumble_use;
}

/*
===============
target_alien_win_use
===============
*/
void target_alien_win_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
  if( !level.uncondHumanWin )
    level.uncondAlienWin = qtrue;
}

/*
===============
SP_target_alien_win
===============
*/
void SP_target_alien_win( gentity_t *self )
{
  self->use = target_alien_win_use;
}

/*
===============
target_human_win_use
===============
*/
void target_human_win_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
  if( !level.uncondAlienWin )
    level.uncondHumanWin = qtrue;
}

/*
===============
SP_target_human_win
===============
*/
void SP_target_human_win( gentity_t *self )
{
  self->use = target_human_win_use;
}

/*
===============
target_hurt_use
===============
*/
void target_hurt_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
  // hurt the activator
  if( !activator || !activator->takedamage )
    return;

    // Don't damage players that are not alive
    if( activator->client &&
        ( activator->client->pers.teamSelection == TEAM_NONE ||
          activator->client->pers.classSelection == PCL_NONE ||
          activator->client->ps.persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT ||
          activator->health <= 0 ) )
    return;

  G_Damage( activator, self, self, NULL, NULL, self->damage, 0, MOD_TRIGGER_HURT );
}

/*
===============
SP_target_hurt
===============
*/
void SP_target_hurt( gentity_t *self )
{
  if( !self->targetname )
  {
    Com_Printf( S_COLOR_YELLOW "WARNING: untargeted %s at %s\n", self->classname,
                                                               vtos( self->r.currentOrigin ) );
  }

  if( !self->damage )
    self->damage = 5;

  self->damage = BG_HP2SU( self->damage );

  self->use = target_hurt_use;
}

/*
===============
bpctrl target
===============
*/
void Use_Target_bpctrl(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  if(!other->TargetGate) {
    return;
  }

  if(other->TargetGate && (other->TargetGate & RESET_BIT)) {
    if(other->TargetGate & TEAM_BIT) {
      g_humanBuildPoints.integer = 0;
    }
    if(!(other->TargetGate & TEAM_BIT)) {
      g_alienBuildPoints.integer = 0;
    }
  }

  if(other->TargetGate & TEAM_BIT) {
    if(other->TargetGate & SIGN_BIT) {
      g_humanBuildPoints.integer -= (other->TargetGate & VALUE_MASK);
    } else {
      g_humanBuildPoints.integer += (other->TargetGate & VALUE_MASK);
    }
  } else {
    if( other->TargetGate & SIGN_BIT ) {
      g_alienBuildPoints.integer -= (other->TargetGate & VALUE_MASK);
    }
    else {
      g_alienBuildPoints.integer += (other->TargetGate & VALUE_MASK);
    }
  }

  if( g_humanBuildPoints.integer < 0 ) {
    g_humanBuildPoints.integer = 0;
  } else if(g_humanBuildPoints.integer > 5000) {
    g_humanBuildPoints.integer = 5000;
  }


  if( g_alienBuildPoints.integer < 0 ) {
    g_alienBuildPoints.integer = 0;
  } else if( g_alienBuildPoints.integer > 5000 ) {
    g_alienBuildPoints.integer = 5000;
  }
}

void SP_target_bpctrl(gentity_t *ent) {
  ent->use = Use_Target_bpctrl;
}

/*
===============
Force Win Target
===============
*/
void Use_Target_force_win(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  if(ent->AMPintVAR[0] == 1) {
    level.uncondHumanWin = qtrue;
  } else {
    level.uncondAlienWin = qtrue;
  }
}

void SP_target_force_win(gentity_t *ent) {
  G_SpawnInt("team", "0", &ent->AMPintVAR[0]); // 0 - aliens  1 - humans

  ent->use = Use_Target_force_win;
}

/*
===============
Force player to class.
===============
*/
void Use_Target_force_class(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  vec3_t infestOrigin;
  int     clientNum;
  class_t class;

  class = BG_ClassByName( ent->inventory )->number;

  if( activator && activator->client &&
      activator->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS &&
      (G_RoomForClassChange(activator, class, infestOrigin)) &&
      (activator->client->ps.stats[STAT_CLASS] != class)) {
    if(class == PCL_NONE) {
      SV_GameSendServerCommand(-1 , "print \"Unknown class (target_force_class)\n\"");
      return;
    }
    clientNum = activator->client - level.clients;
    if( activator->client->ps.stats[ STAT_STATE ] & SS_WALLCLIMBING )
      activator->client->ps.stats[ STAT_STATE ] &= ~SS_WALLCLIMBING;

    activator->client->pers.evolveHealthFraction = (float)activator->client->ps.misc[ MISC_HEALTH ] /
                                                   (float)BG_Class( activator->client->pers.classSelection )->health;

    G_Evolve( activator, class, 0, infestOrigin, qtrue );

    ent->activator = activator;
    G_UseTargets( ent, ent->activator );
  }
  else
    return;
}

void SP_target_force_class( gentity_t *ent ) {
  ent->use = Use_Target_force_class;
}

/*
===============
Force player to use a weapon.
===============
*/
void Use_Target_force_weapon(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  int i;
  qboolean change = qfalse;

  if(
    activator && activator->client &&
    activator->client->ps.stats[STAT_TEAM] == TEAM_HUMANS) {
    if(other->TargetGate && (other->TargetGate & RESET_BIT)) {
      G_TakeItemAfterCheck(activator, "weapon", qtrue);
    }

    if(other->TargetGate && (other->TargetGate & SIGN_BIT)) {
      for(i = 0; ent->wTriggers[i] != WP_NONE; i++) {
        if(
          BG_InventoryContainsWeapon(
            ent->wTriggers[i], activator->client->ps.stats)) {
          G_TakeItemAfterCheck(activator, BG_Weapon(ent->wTriggers[i])->name, qtrue);
        }
      }

      for(i = 0; ent->uTriggers[ i ] != UP_NONE; i++) {
        if(
          BG_InventoryContainsUpgrade(
            ent->uTriggers[i], activator->client->ps.stats)) {
          G_TakeItemAfterCheck(activator, BG_Upgrade(ent->uTriggers[i])->name, qtrue);
        }
      }
    }

    if(!other->TargetGate || !(other->TargetGate & SIGN_BIT)) {
      for(i = 0; ent->wTriggers[i] != WP_NONE; i++) {
        if(
          BG_InventoryContainsWeapon(
            ent->wTriggers[i], activator->client->ps.stats)) {
          continue;
        } else {
          if(G_GiveItemAfterCheck(activator, BG_Weapon(ent->wTriggers[i])->name, qtrue)) {
            change = qtrue;
          }
        }
      }

      for(i = 0; ent->uTriggers[i] != UP_NONE; i++) {
        if(
          BG_InventoryContainsUpgrade(
            ent->uTriggers[i], activator->client->ps.stats)) {
              continue;
        }

        if(G_GiveItemAfterCheck(activator, BG_Upgrade(ent->uTriggers[i])->name, qtrue)) {
          change = qtrue;
        }
      }
    }
    if(change) {
      ent->activator = activator;
      G_UseTargets( ent, ent->activator );
    }
  }
}

void SP_target_force_weapon(gentity_t *ent) {
  char *buffer;

  G_SpawnString("inventory", "", &buffer);

  BG_ParseCSVEquipmentList(
    buffer, ent->wTriggers, WP_NUM_WEAPONS, ent->uTriggers, UP_NUM_UPGRADES);
  ent->use = Use_Target_force_weapon;
}

/*
===============
Give Funds Target
===============
*/
void Use_Target_fund( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
  if( activator && activator->client ) {
    if( activator->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ) {
      G_AddCreditToClient(activator->client, ent->AMPintVAR[0] , qtrue);
      ent->activator = activator;
      G_UseTargets( ent, ent->activator );
    }

    if( activator->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS ) {
      G_AddCreditToClient(activator->client, ent->AMPintVAR[1] , qtrue);
      ent->activator = activator;
      G_UseTargets( ent, ent->activator );
    }
  }
  else {
    return;
  }
}

void SP_target_fund( gentity_t *ent ) {
  G_SpawnInt( "hfund", "0", &ent->AMPintVAR[0] );
  G_SpawnInt( "afund", "0", &ent->AMPintVAR[1] );

  ent->use = Use_Target_fund;
}

/*
===============
If Target.
===============
*/
void Use_Target_if(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  int i;

  if( !strcmp(ent->Cvar_Val, "NULL") ) {
    return;
  }

  ent->activator = activator;

  i = Cvar_VariableIntegerValue(ent->Cvar_Val);

  // NOTE: changed to if/elseif since it makes more sense
  if(!strcmp(ent->sign, "==")) {
    if(i == ent->GateState) {
      G_UseTargets(ent, ent->activator);
    }

    return;
  } else if(!strcmp(ent->sign, "!=")) {
    if(i != ent->GateState) {
      G_UseTargets(ent, ent->activator);
    }

    return;
  } else if(!strcmp(ent->sign, ">=")) {
    if(i >= ent->GateState) {
      G_UseTargets(ent, ent->activator);
    }

    return;
  } else if(!strcmp(ent->sign, ">")) {
    if(i > ent->GateState) {
      G_UseTargets(ent, ent->activator);
    }

    return;
  } else if(!strcmp(ent->sign, "<=")) {
    if(i <= ent->GateState) {
      G_UseTargets(ent, ent->activator);
    }

    return;
  } else if(!strcmp(ent->sign, "<")) {
    if(i < ent->GateState) {
      G_UseTargets(ent, ent->activator);
    }

    return;
  }
}

void SP_target_if(gentity_t *ent) {
  G_SpawnInt("value", "0", &ent->GateState);

  ent->use = Use_Target_if;
}

/*
===============
StageControl Target
===============
*/
// NOTE: Do we need to uncomment this or get rid of this
//void Think_Target_stagecontrol( gentity_t *ent )
//{
//  G_UseTargets( ent, ent->activator );
//}

void Use_Target_stgctrl(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  ent->activator = activator;
  if(!other->TargetGate) {
    return;
  }

  if(other->TargetGate & RESET_BIT) {
    g_AMPStageLock.integer = 0;
    if(other->TargetGate & TEAM_BIT) {
      g_humanStage.integer = 0;
    }
    if(!(other->TargetGate & TEAM_BIT)) {
      g_alienStage.integer = 0;
    }
  }

  if(other->TargetGate & LOCKSTAGE_BIT) {
    g_AMPStageLock.integer = 1;
  } else {
    g_AMPStageLock.integer = 0;
  }

  if(other->TargetGate & TEAM_BIT) {
    if(other->TargetGate & SIGN_BIT) {
      g_humanStage.integer -= (other->TargetGate & VALUE_MASK);
    } else {
      g_humanStage.integer += (other->TargetGate & VALUE_MASK);
    }
  } else {
    if(other->TargetGate & SIGN_BIT) {
      g_alienStage.integer -= (other->TargetGate & VALUE_MASK);
    } else {
      g_alienStage.integer += (other->TargetGate & VALUE_MASK);
    }
  }

  if(g_humanStage.integer < 0) {
    g_humanStage.integer = 0;
  }
  if(g_humanStage.integer > g_humanMaxStage.integer) {
    g_humanStage.integer = g_humanMaxStage.integer;
  }

  if(g_alienStage.integer < 0) {
    g_alienStage.integer = 0;
  }
  if(g_alienStage.integer > g_alienMaxStage.integer) {
    g_alienStage.integer = g_alienMaxStage.integer;
  }
}

void SP_target_stgctrl(gentity_t *ent) {
  ent->use = Use_Target_stgctrl;
}

/*
===============
Counter Target. Counts up and triggers when overflows.
===============
*/
void Use_Target_count(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  ent->activator = activator;

  if(
    (other->TargetGate && (other->TargetGate & RESET_BIT)) ||
    (ent->Charge != 0 && ent->AMPintVAR[1] < level.time)) {
    ent->AMPintVAR[0] = 0;
    ent->AMPintVAR[1] = level.time + ent->Charge;
  }

  if(!(other->TargetGate & SIGN_BIT)) {
    if(++ent->AMPintVAR[0] >= ent->ResetValue) {
      ent->AMPintVAR[0] = 0;
      G_UseTargets( ent, ent->activator );
    }
  } else if( --ent->AMPintVAR[0] < 0 ) {
    ent->AMPintVAR[0] = 0;
  }
}

void SP_target_count( gentity_t *ent ) {
  G_SpawnInt( "maxval", "1", &ent->ResetValue );
  G_SpawnInt( "reset", "0", &ent->Charge );

  if(ent->ResetValue <= 0)
    ent->ResetValue = 1;

  ent->use = Use_Target_count;
}

/*
===============
AND Gate. max 8 inputs.
===============
*/
// NOTE: this too do we need to uncomment it
//void Think_Target_AND( gentity_t *ent )
//{
//  G_UseTargets( ent, ent->activator );
//}

void Use_Target_AND(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  int j;
  qboolean total;

  if(!other->TargetGate) {
    return;
  }

  total = ent->GateState == 0xFF;

  ent->activator = activator;
  j = ent->GateState;

  if(other->TargetGate & RESET_BIT) {
    ent->GateState = ent->ResetValue;
  } else {
    ent->GateState ^= ((other->TargetGate) & 0xFF);
  }

  if((total != (ent->GateState == 0xFF)) && !(ent->TrigOnlyRise && total)) {
    if(other->TargetGate & RESET_AFTER_USE) {
      ent->GateState = j;
    }

    G_UseTargets(ent, ent->activator);
  } else {
    if(other->TargetGate & RESET_AFTER_USE) {
      ent->GateState = j;
    }
  }
}

void SP_target_and(gentity_t *ent) {
  int i;
  G_SpawnInt("trigonlyrise", "0", &i);
  if(i == 0)
    ent->TrigOnlyRise = qfalse;
  else
    ent->TrigOnlyRise = qtrue;

  G_SpawnInt("resetval","252", &ent->ResetValue);
  ent->ResetValue &= 0xFF;
  ent->GateState = ent->ResetValue;
  ent->use = Use_Target_AND;
}

/*
===============
ORGate. max 8 inputs.
===============
*/
//void Think_Target_AND( gentity_t *ent )
//{
//  G_UseTargets( ent, ent->activator );
//}

void Use_Target_OR( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
  int j;
  qboolean total;

  if(!other->TargetGate) {
    return;
  }

  total = ent->GateState || 0;
  ent->activator = activator;

  j = ent->GateState;

  if(other->TargetGate && (other->TargetGate & RESET_BIT)) {
    ent->GateState = ent->ResetValue;
  } else {
    ent->GateState ^= ((other->TargetGate) & 0xFF);
  }

  if((total != (ent->GateState || 0)) && !(ent->TrigOnlyRise && total)) {
    if(other->TargetGate & RESET_AFTER_USE) {
      ent->GateState = j;
    }

    G_UseTargets(ent, ent->activator);
  } else {
    if(other->TargetGate & RESET_AFTER_USE) {
      ent->GateState = j;
    }
  }
}

void SP_target_or(gentity_t *ent) {
  int i;

  G_SpawnInt("trigonlyrise", "0", &i);
  if(i == 0) {
    ent->TrigOnlyRise = qfalse;
  } else {
    ent->TrigOnlyRise = qtrue;
  }

  G_SpawnInt("resetval","0", &ent->ResetValue);
  ent->ResetValue &= 0xFF;
  ent->GateState = ent->ResetValue;
  ent->use = Use_Target_OR;
}

/*
===============
XOR Gate. max 8 inputs.
===============
*/
//void Think_Target_XOR( gentity_t *ent )
//{
//  G_UseTargets( ent, ent->activator );
//}

void Use_Target_XOR(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  int i, j;
  qboolean total, test;

  if( !other->TargetGate ) {
    return;
  }

  i = ent->GateState & 0xFF;
  j = ent->GateState;

  if( i == 0 || (i & (i - 1)) ) {
    total = qfalse;
  } else {
    total = qtrue;
  }
    

  ent->activator = activator;

  if(other->TargetGate && (other->TargetGate & RESET_BIT)) {
    ent->GateState = ent->ResetValue;
  } else {
    ent->GateState ^= ((other->TargetGate) & 0xFF);
  }

  i = ent->GateState;
  if(i == 0 || (i & (i - 1))) {
    test = qfalse;
  } else {
    test = qtrue;
  }

  if((total != test) && !(ent->TrigOnlyRise && total)) {
    if(other->TargetGate & RESET_AFTER_USE) {
      ent->GateState = j;
    }

    G_UseTargets( ent, ent->activator );
  } else {
    if(other->TargetGate & RESET_AFTER_USE) {
      ent->GateState = j;
    }
  }
}

void SP_target_xor(gentity_t *ent) {
  int i;

  G_SpawnInt("trigonlyrise", "0", &i);

  if(i == 0) {
    ent->TrigOnlyRise = qfalse;
  } else {
    ent->TrigOnlyRise = qtrue;
  }

  G_SpawnInt("resetval","254", &ent->ResetValue);
  ent->ResetValue &= 0xFF;
  ent->GateState = ent->ResetValue;
  ent->use = Use_Target_XOR;
}

/*
===============
trigger_class_match
===============
*/
qboolean target_class_match(gentity_t *self, gentity_t *activator) {
  int i = 0;

  //if there is no class list every class triggers (stupid case)
  if(self->cTriggers[i] == PCL_NONE) {
    return qtrue;
  } else {
    //otherwise check against the list
    for(i = 0; self->cTriggers[i] != PCL_NONE; i++) {
      if(activator->client->ps.stats[STAT_CLASS] == self->cTriggers[i]) {
        return qtrue;
      }
    }
  }

  return qfalse;
}

/*
===============
trigger_class_trigger
===============
*/
void target_class_trigger( gentity_t *self, gentity_t *activator )
{
  //sanity check
  if(!activator->client) {
    return;
  }

  if(activator->client->ps.stats[ STAT_TEAM ] != TEAM_ALIENS) {
    return;
  }

  self->activator = activator;

  if(self->s.eFlags & EF_DEAD) {
    if(!target_class_match(self, activator)) {
      G_UseTargets(self, activator);
    }
  } else {
    if(target_class_match(self, activator)) {
      G_UseTargets(self, activator);
    }
  }
}

/*
===============
trigger_class_touch
===============
*/
void target_class_use(gentity_t *ent, gentity_t *other, gentity_t *activator) {
  //only triggered by clients
  if(!other->client)
    return;

  target_class_trigger(ent, other);
}


void SP_target_class(gentity_t *self) {
  char *buffer;

  G_SpawnString("classes", "", &buffer);

  BG_ParseCSVClassList(buffer, self->cTriggers, PCL_NUM_CLASSES);

  // NEGATE
  if(self->spawnflags & 2) {
    self->s.eFlags |= EF_DEAD;
  }

  self->use = target_class_use;
}

/*
===============
Target equipment
===============
*/
qboolean target_equipment_match(gentity_t *self, gentity_t *activator) {
  int i = 0;

  //if there is no equipment list all equipment triggers (stupid case)
  if(self->wTriggers[i] == WP_NONE && self->uTriggers[ i ] == UP_NONE) {
    return qtrue;
  } else {
    //otherwise check against the lists
    for(i = 0; self->wTriggers[i] != WP_NONE; i++) {
      if(
        BG_InventoryContainsWeapon(
          self->wTriggers[i], activator->client->ps.stats)) {
        return qtrue;
      }
    }

    for(i = 0; self->uTriggers[ i ] != UP_NONE; i++)
    {
      if(
        BG_InventoryContainsUpgrade(
          self->uTriggers[i], activator->client->ps.stats)) {
        return qtrue;
      }
    }
  }

  return qfalse;
}


void target_equipment_use(gentity_t *self, gentity_t *other, gentity_t *activator) {
  //sanity check
  if(!activator->client) {
    return;
  }

  if(activator->client->ps.stats[STAT_TEAM] != TEAM_HUMANS) {
    return;
  }

  self->activator = activator;

  if(self->s.eFlags & EF_DEAD) {
    if(!target_equipment_match( self, activator)) {
      G_UseTargets( self, activator );
    }
  } else {
    if(target_equipment_match( self, activator)) {
      G_UseTargets(self, activator);
    }
  }
}

void SP_target_equipment(gentity_t *self) {
  char *buffer;

  G_SpawnString("equipment", "", &buffer);

  BG_ParseCSVEquipmentList(buffer, self->wTriggers, WP_NUM_WEAPONS,
      self->uTriggers, UP_NUM_UPGRADES);

  // NEGATE
  if(self->spawnflags & 2) {
    self->s.eFlags |= EF_DEAD;
  }

  self->use = target_equipment_use;
}

void Use_target_power(gentity_t *self, gentity_t *other, gentity_t *activator) {
  self->powered = !self->powered;
}

void SP_target_power(gentity_t *self) {
  G_SpawnInt("radius", "100", &self->PowerRadius);

  if(self->spawnflags & 1) {
    self->MasterPower = qtrue;
  } else {
    self->MasterPower = qfalse;
  }

  if(!(self->spawnflags & 2)) {
    self->powered = qtrue;
  } else {
    self->powered = qfalse;
  }

  self->use = Use_target_power;
}

void Use_target_creep( gentity_t *self, gentity_t *other, gentity_t *activator ) {
  self->powered = !self->powered;
}

void SP_target_creep(gentity_t *self) {
  G_SpawnInt("radius", "100", &self->PowerRadius);

  if(self->spawnflags & 1) {
    self->MasterPower = qtrue;
  } else {
    self->MasterPower = qfalse;
  }

  if(!(self->spawnflags & 2)) {
    self->powered = qtrue;
  } else {
    self->powered = qfalse;
  }

  self->use = Use_target_creep;
}
