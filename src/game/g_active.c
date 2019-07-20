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
===============
G_DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the player_state_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void P_DamageFeedback( gentity_t *player )
{
  gclient_t *client;
  float     count;
  vec3_t    angles;

  client = player->client;
  if( !PM_Alive( client->ps.pm_type ) )
    return;

  // total points of damage shot at the player this frame
  count = client->damage_blood + client->damage_armor;
  if( count == 0 )
    return;   // didn't take any damage

  if( BG_SU2HP( count ) )
    count = BG_SU2HP( count );
  else
    count = 1;

  if( count > 255 )
    count = 255;

  // send the information to the client

  // world damage (falling, slime, etc) uses a special code
  // to make the blend blob centered instead of positional
  if( client->damage_fromWorld )
  {
    client->ps.damagePitch = 255;
    client->ps.damageYaw = 255;

    client->damage_fromWorld = qfalse;
  }
  else
  {
    vectoangles( client->damage_from, angles );
    client->ps.damagePitch = angles[ PITCH ] / 360.0 * 256;
    client->ps.damageYaw = angles[ YAW ] / 360.0 * 256;
  }

  // play an apropriate pain sound
  if( ( level.time > player->pain_debounce_time ) && !( player->flags & FL_GODMODE ) )
  {
    player->pain_debounce_time = level.time + 700;
    G_AddEvent( player, EV_PAIN, BG_GetPainState( &client->ps ) );
    client->ps.damageEvent++;
  }


  client->ps.damageCount = count;

  //
  // clear totals
  //
  client->damage_blood = 0;
  client->damage_armor = 0;
  client->damage_knockback = 0;
}



/*
=============
P_WorldEffects

Check for lava / slime contents and drowning
=============
*/
void P_WorldEffects( gentity_t *ent )
{
  int       waterlevel;

  if( ent->client->noclip )
  {
    ent->client->airOutTime = level.time + 12000; // don't need air
    return;
  }

  waterlevel = ent->waterlevel;

  //
  // check for drowning
  //
  if( waterlevel == 3 )
  {
    // if out of air, start drowning
    if( ent->client->airOutTime < level.time)
    {
      // drown!
      ent->client->airOutTime += 1000;
      if( ent->health > 0 )
      {
        // take more damage the longer underwater
        ent->damage += BG_HP2SU( 2 );
        if( ent->damage > BG_HP2SU( 15 ) )
          ent->damage = BG_HP2SU( 15 );

        // play a gurp sound instead of a normal pain sound
        if( ent->health <= ent->damage )
          G_Sound( ent, CHAN_VOICE, G_SoundIndex( "*drown.wav" ) );
        else if( rand( ) < RAND_MAX / 2 + 1 )
          G_Sound( ent, CHAN_VOICE, G_SoundIndex( "sound/player/gurp1.wav" ) );
        else
          G_Sound( ent, CHAN_VOICE, G_SoundIndex( "sound/player/gurp2.wav" ) );

        // don't play a normal pain sound
        ent->pain_debounce_time = level.time + 200;

        G_Damage( ent, NULL, NULL, NULL, NULL,
          ent->damage, DAMAGE_NO_ARMOR, MOD_WATER );
      }
    }
  }
  else
  {
    ent->client->airOutTime = level.time + 12000;
    ent->damage = BG_HP2SU( 2 );
  }

  //
  // check for sizzle damage (move to pmove?)
  //
  if( waterlevel &&
      ( ent->watertype & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) )
  {
    if( ent->health > 0 &&
        ent->pain_debounce_time <= level.time  )
    {
      if( ent->watertype & CONTENTS_LAVA )
      {
        G_Damage( ent, NULL, NULL, NULL, NULL,
          BG_HP2SU( 30 ) * waterlevel, 0, MOD_LAVA );
      }

      if( ent->watertype & CONTENTS_SLIME )
      {
        G_Damage( ent, NULL, NULL, NULL, NULL,
          BG_HP2SU( 10 ) * waterlevel, 0, MOD_SLIME );
      }
    }
  }
}



/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound( gentity_t *ent )
{
  if( ent->waterlevel && ( ent->watertype & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) )
    ent->client->ps.loopSound = level.snd_fry;
  else
    ent->client->ps.loopSound = 0;
}



//==============================================================

/*
==============
ClientShove
==============
*/
static int GetClientMass( gentity_t *ent )
{
  int entMass = 100;

  if( ent->client->pers.teamSelection == TEAM_ALIENS )
    entMass = BG_Class( ent->client->pers.classSelection )->health;
  else if( ent->client->pers.teamSelection == TEAM_HUMANS )
  {
    if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, ent->client->ps.stats ) )
      entMass *= 2;
  }
  else
    return 0;
  return entMass;
}

static void ClientShove( gentity_t *ent, gentity_t *victim )
{
  vec3_t dir, push;
  float force;
  int entMass, vicMass;

  // Don't push if the entity is not trying to move
  if( !ent->client->pers.cmd.rightmove && !ent->client->pers.cmd.forwardmove &&
      !ent->client->pers.cmd.upmove )
    return;

  // Cannot push enemy players unless they are walking on the player
  if( !OnSameTeam( ent, victim ) &&
      victim->client->ps.groundEntityNum != ent - g_entities )
    return;

  // Shove force is scaled by relative mass
  entMass = GetClientMass( ent );
  vicMass = GetClientMass( victim );
  if( vicMass <= 0 || entMass <= 0 )
    return;
  force = g_shove.value * entMass / vicMass;
  if( force < 0 )
    force = 0;
  if( force > 150 )
    force = 150;

  // Give the victim some shove velocity
  VectorSubtract( victim->r.currentOrigin, ent->r.currentOrigin, dir );
  VectorNormalizeFast( dir );
  VectorScale( dir, force, push );
  VectorAdd( victim->client->ps.velocity, push, victim->client->ps.velocity );

  // Set the pmove timer so that the other client can't cancel
  // out the movement immediately
  if( !victim->client->ps.pm_time )
  {
    int time;

    time = force * 2 + 0.5f;
    if( time < 50 )
      time = 50;
    if( time > 200 )
      time = 200;
    victim->client->ps.pm_time = time;
    victim->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
  }
}

/*
==============
ClientImpacts
==============
*/
void ClientImpacts( pmove_t *pm, trace_t *trace,
                    const vec3_t impactVelocity )
{
  gentity_t *other = &g_entities[ trace->entityNum ];
  gentity_t *ent = &g_entities[ pm->ps->clientNum ];


  // see G_UnlaggedDetectCollisions(), this is the inverse of that.
  // if our movement is blocked by another player's real position,
  // don't use the unlagged position for them because they are
  // blocking or server-side Pmove() from reaching it
  if( other->client )
    G_DisableUnlaggedCalc(other);

  // tyrant impact attacks
  if( ent->client->ps.weapon == WP_ALEVEL4 )
  {
    G_ChargeAttack( ent, other );
    G_CrushAttack( ent, other );
  }

  // shove players
  if( ent->client && other->client )
    ClientShove( ent, other );

  // touch triggers
  if( other->touch ) {
    if(g_debugAMP.integer) {
      char *s = va(
        "touched by #%i (%s^7)",
        (int)(ent-g_entities),
        ent->client->pers.netname);
      G_LoggedActivation(other, ent, NULL, trace, s, LOG_ACT_TOUCH);
    }
    else {
      other->touch(other, ent, trace);
    }
  }
}

/*
============
G_TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void  G_TouchTriggers( gentity_t *ent )
{
  int       i, num;
  int       touch[MAX_GENTITIES];
  gentity_t *hit;
  trace_t   trace;
  vec3_t    mins, maxs;
  vec3_t    pmins, pmaxs;
  static    vec3_t range = { 10, 10, 10 };

  if( !ent->client )
    return;

  // noclipping clients don't activate triggers!
  if( ent->client->noclip )
    return;

  // dead clients don't activate triggers!
  if( ent->client->ps.misc[ MISC_HEALTH ] <= 0 )
    return;

  ent->activation.usable_map_trigger = ENTITYNUM_NONE;

  BG_ClassBoundingBox( ent->client->ps.stats[ STAT_CLASS ],
                       pmins, pmaxs, NULL, NULL, NULL );

  VectorAdd( ent->client->ps.origin, pmins, mins );
  VectorAdd( ent->client->ps.origin, pmaxs, maxs );

  VectorSubtract( mins, range, mins );
  VectorAdd( maxs, range, maxs );

  num = SV_AreaEntities( mins, maxs, touch, MAX_GENTITIES );

  // can't use ent->absmin, because that has a one unit pad
  VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
  VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );

  for( i = 0; i < num; i++ )
  {
    hit = &g_entities[ touch[ i ] ];

    if( !hit->touch && !ent->touch )
      continue;

    if( !( hit->r.contents & CONTENTS_TRIGGER ) )
      continue;

    // ignore most entities if a spectator
    if( ( ent->clipmask & CONTENTS_ASTRAL_NOCLIP ) &&
        hit->s.eType != ET_TELEPORT_TRIGGER )
      continue;

    if( !SV_EntityContact( mins, maxs, hit, TT_AABB ) )
      continue;

    memset( &trace, 0, sizeof( trace ) );

    if( hit->touch ) {
      if( g_debugAMP.integer )
      {
        char *s = va(
          "triggered by #%i (%s^7)",
          (int)(ent-g_entities),
          ent->client->pers.netname);
        G_LoggedActivation(hit, ent, NULL, &trace, s, LOG_ACT_TOUCH);
      }
      else
        hit->touch( hit, ent, &trace );
    }
  }
}

/*
=================
G_ClientUpdateSpawnQueue

Send spawn queue data to a client
=================
*/
static void G_ClientUpdateSpawnQueue(gclient_t *client) {
  client->pers.spawn_queue_pos =
    BG_List_Index(
      &level.spawn_queue[client->ps.stats[STAT_TEAM]], client);

  if(client->pers.spawn_queue_pos < 1) {
    client->ps.persistant[PERS_QUEUEPOS] = (client->pers.spawnTime - level.time) / 1000;
  	if( client->ps.persistant[PERS_QUEUEPOS] < 0 ) {
  		client->ps.persistant[PERS_QUEUEPOS] = 0;
    }
    client->ps.persistant[PERS_STATE] &= ~PS_QUEUED_NOT_FIRST;
  } else {
    client->ps.persistant[PERS_QUEUEPOS] = client->pers.spawn_queue_pos;
    client->ps.persistant[PERS_STATE] |= PS_QUEUED_NOT_FIRST;
  }

	if( client->ps.stats[STAT_TEAM] == TEAM_ALIENS) {
		client->ps.persistant[PERS_SPAWNS] = level.numAlienSpawns;
  } else if(client->ps.stats[STAT_TEAM] == TEAM_HUMANS) {
		client->ps.persistant[PERS_SPAWNS] = level.numHumanSpawns;
  }
}

/*
=================
SpectatorThink
=================
*/
void SpectatorThink( gentity_t *ent, usercmd_t *ucmd )
{
  pmove_t pm;
  gclient_t *client;
  int clientNum;
  qboolean attack1, following, queued;

  client = ent->client;

  client->oldbuttons = client->buttons;
  client->buttons = ucmd->buttons;

  attack1 = !client->pers.swapAttacks ?
                ( ( client->buttons & BUTTON_ATTACK ) &&
                  !( client->oldbuttons & BUTTON_ATTACK ) ) :
                ( ( client->buttons & BUTTON_ATTACK2 ) &&
                  !( client->oldbuttons & BUTTON_ATTACK2 ) );

  // We are in following mode only if we are following a non-spectating client
  following = client->sess.spectatorState == SPECTATOR_FOLLOW;
  if( following )
  {
    clientNum = client->sess.spectatorClient;
    if( clientNum < 0 || clientNum > level.maxclients ||
        !g_entities[ clientNum ].client ||
        g_entities[ clientNum ].client->sess.spectatorState != SPECTATOR_NOT )
      following = qfalse;
  }

  // Check to see if we are in the spawn queue
  
  if(BG_List_Find(&level.spawn_queue[client->ps.stats[STAT_TEAM]], client)) {
    queued = qtrue;
  } else {
    queued = client->spawnReady;
  }

  // Wants to get out of spawn queue
  if( attack1 && queued )
  {
    if( client->sess.spectatorState == SPECTATOR_FOLLOW ) {
      G_StopFollowing( ent );
    }
    BG_List_Remove_All(
      &level.spawn_queue[client->ps.stats[STAT_TEAM]], client);
    client->spawnReady = qfalse;
    client->pers.classSelection = PCL_NONE;
    client->pers.humanItemSelection = WP_NONE;
    client->ps.stats[ STAT_CLASS ] = PCL_NONE;
    client->ps.persistant[ PERS_STATE ] &= ~PS_QUEUED;
    queued = qfalse;
  }
  else if( attack1 )
  {
    // Wants to get into spawn queue
    if( client->sess.spectatorState == SPECTATOR_FOLLOW )
      G_StopFollowing( ent );
    if( client->pers.teamSelection == TEAM_NONE )
      G_TriggerMenu( client->ps.clientNum, MN_TEAM );
    else if( client->pers.teamSelection == TEAM_ALIENS )
      G_TriggerMenu( client->ps.clientNum, MN_A_CLASS );
    else if( client->pers.teamSelection == TEAM_HUMANS )
      G_TriggerMenu( client->ps.clientNum, MN_H_SPAWN );
  }

  // We are either not following anyone or following a spectator
  if( !following )
  {
    if( client->sess.spectatorState == SPECTATOR_LOCKED ||
        client->sess.spectatorState == SPECTATOR_FOLLOW )
      client->ps.pm_type = PM_FREEZE;
    else if( client->noclip )
      client->ps.pm_type = PM_NOCLIP;
    else
      client->ps.pm_type = PM_SPECTATOR;

    if( queued )
      client->ps.persistant[ PERS_STATE ] |= PS_QUEUED;

    client->ps.speed = client->pers.flySpeed;
    client->ps.stats[ STAT_STAMINA ] = 0;
    client->ps.misc[ MISC_MISC ] = 0;
    client->ps.stats[ STAT_BUILDABLE ] = BA_NONE;
    client->ps.stats[ STAT_CLASS ] = PCL_NONE;
    client->ps.weapon = WP_NONE;

    // Set up for pmove
    memset( &pm, 0, sizeof( pm ) );
    pm.ps = &client->ps;
    pm.pmext = &client->pmext;
    pm.cmd = *ucmd;
    pm.tracemask = ent->clipmask;
    pm.trace = G_TraceWrapper;
    pm.pointcontents = SV_PointContents;
    pm.unlagged_on = G_UnlaggedOn;
    pm.unlagged_off = G_UnlaggedOff;
    pm.tauntSpam = 0;
    pm.swapAttacks = client->pers.swapAttacks;
    pm.wallJumperMinFactor = client->pers.wallJumperMinFactor;
    pm.marauderMinJumpFactor = client->pers.marauderMinJumpFactor;

    // Perform a pmove
    Pmove( &pm );

    // Save results of pmove
    VectorCopy( client->ps.origin, ent->s.pos.trBase );
    VectorCopy( client->ps.origin, ent->r.currentOrigin );
    VectorCopy( client->ps.viewangles, ent->r.currentAngles );
    VectorCopy( client->ps.viewangles, ent->s.pos.trBase );

    G_TouchTriggers( ent );
    SV_UnlinkEntity( ent );

    // Set the queue position and spawn count for the client side
    G_ClientUpdateSpawnQueue( ent->client );
  }
}

/*
=================
ComparePreviousCmdAngles

Checks if a client's command angles changed
=================
*/
static void ComparePreviousCmdAngles( gclient_t *client )
{
  int i;

  if( client->pers.previousCmdAnglesTime < level.time )
  {
    if( ( client->pers.previousCmdAngles[0] != client->pers.cmd.angles[0] ) ||
        ( client->pers.previousCmdAngles[1] != client->pers.cmd.angles[1] ) ||
        ( client->pers.previousCmdAngles[2] != client->pers.cmd.angles[2] ) )
    {
      for( i = 0; i < 3; i++ )
        client->pers.previousCmdAngles[i] = client->pers.cmd.angles[i];

      client->pers.previousCmdAnglesTime = level.time;
      client->pers.cmdAnglesChanged = qtrue;
    } else
      client->pers.cmdAnglesChanged = qfalse;
  }

  return;
}

/*
=================
ClientInactivityTimer

Returns qfalse if the client is dropped
=================
*/
qboolean ClientInactivityTimer( gentity_t *ent )
{
  gclient_t *client = ent->client;

  if( ! g_inactivity.integer )
  {
    // give everyone some time, so if the operator sets g_inactivity during
    // gameplay, everyone isn't kicked
    client->inactivityTime = level.time + 60 * 1000;
    client->inactivityWarning = qfalse;
  }
  else if( client->pers.cmd.forwardmove ||
           client->pers.cmd.rightmove ||
           client->pers.cmd.upmove ||
           ( client->pers.cmd.buttons & BUTTON_ATTACK ) ||
           ( client->pers.cmd.buttons & BUTTON_ATTACK2 ) ||
           client->pers.cmdAnglesChanged ||
           client->spawnReady )
  {
    client->inactivityTime = level.time + g_inactivity.integer * 1000;
    client->inactivityWarning = qfalse;
  }
  else if( !client->pers.localClient && !IS_SCRIM )
  {
    if( level.time > client->inactivityTime - 10000 )
    {
      if( level.time > client->inactivityTime )
      {
        if( G_admin_permission( ent, ADMF_ACTIVITY ) )
        {
          client->inactivityTime = level.time + g_inactivity.integer * 1000;
          return qtrue;
        }
        {
          SV_GameSendServerCommand( -1,
                                  va( "print \"%s^7 moved from %s to spectators due to inactivity\n\"",
                                      client->pers.netname,
                                      BG_Team( client->pers.teamSelection )->name2 ) );
          G_LogPrintf( "Inactivity: %d", (int)(client - level.clients) );
          G_ChangeTeam( ent, TEAM_NONE );
        }

        return qfalse;
      }
      else if( !client->inactivityWarning )
      {
        client->inactivityWarning = qtrue;
        if( !G_admin_permission( ent, ADMF_ACTIVITY ) )
          SV_GameSendServerCommand( client - level.clients,
            va( "cp \"Ten seconds until inactivity spectate!\n\" %d",
            CP_INACTIVITY ) );
      }
    }
  }

  return qtrue;
}

/*
=================
VoterInactivityTimer

This is used for implied voting
=================
*/
void VoterInactivityTimer( gentity_t *ent )
{
  gclient_t *client = ent->client;

  if( client->pers.connected != CON_CONNECTED )
    client->pers.voterInactivityTime = level.time - 2000;
  else if( client->pers.cmd.forwardmove ||
      client->pers.cmd.rightmove ||
      client->pers.cmd.upmove ||
      ( client->pers.cmd.buttons & BUTTON_ATTACK ) ||
      ( client->pers.cmd.buttons & BUTTON_ATTACK2 ) ||
      client->pers.cmdAnglesChanged ||
      client->spawnReady )
  {
    client->pers.voterInactivityTime = level.time + ( VOTE_TIME );
  }

  return;
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
void ClientTimerActions( gentity_t *ent, int msec )
{
  gclient_t *client;
  usercmd_t *ucmd;
  int       aForward, aRight;
  const int maxHealth = BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->health;
  qboolean  walking = qfalse, stopped = qfalse,
            crouched = qfalse;
  int       i;

  ucmd = &ent->client->pers.cmd;

  aForward  = abs( ucmd->forwardmove );
  aRight    = abs( ucmd->rightmove );

  if( aForward == 0 && aRight == 0 )
    stopped = qtrue;
  else if( aForward <= 64 && aRight <= 64 )
    walking = qtrue;

  if( ucmd->upmove <= 0 && ent->client->ps.pm_flags & PMF_DUCKED )
    crouched = qtrue;

  client = ent->client;
  client->time100 += msec;
  client->time1000 += msec;
  client->time10000 += msec;

  while ( client->time100 >= 100 )
  {
    weapon_t weapon = BG_GetPlayerWeapon( &client->ps );

    client->time100 -= 100;

    // Restore or subtract stamina
    if( BG_ClassHasAbility( client->ps.stats[STAT_CLASS], SCA_STAMINA ) )
    {
      if( stopped || client->ps.pm_type == PM_JETPACK )
        client->ps.stats[ STAT_STAMINA ] += STAMINA_STOP_RESTORE;
      else if( ( client->ps.stats[ STAT_STATE ] & SS_SPEEDBOOST )  &&
                 g_humanStaminaMode.integer &&
               !( client->buttons & BUTTON_WALKING ) ) // walk overrides sprint
        client->ps.stats[ STAT_STAMINA ] -= STAMINA_SPRINT_TAKE;
      else if( walking || crouched )
        client->ps.stats[ STAT_STAMINA ] += STAMINA_WALK_RESTORE;
      else
        client->ps.stats[ STAT_STAMINA ] += STAMINA_RUN_RESTORE;

      // Check stamina limits
      if( client->ps.stats[ STAT_STAMINA ] > STAMINA_MAX )
      {
        client->medKitStaminaToRestore = 0;
        client->nextMedKitRestoreStaminaTime = -1;
        client->ps.stats[ STAT_STAMINA ] = STAMINA_MAX;
      }
      else
      {
        int      staminaDifference;

        if( client->ps.stats[ STAT_STAMINA ] < -STAMINA_MAX )
          client->ps.stats[ STAT_STAMINA ] = -STAMINA_MAX;

        //  restore stamina from medkit
        staminaDifference = STAMINA_MAX - client->ps.stats[ STAT_STAMINA ];

        if( client->medKitStaminaToRestore &&
            client->nextMedKitRestoreStaminaTime < level.time )
        {
          if( client->medKitStaminaToRestore > STAMINA_MEDISTAT_RESTORE )
          {
            if( staminaDifference > STAMINA_MEDISTAT_RESTORE )
            {
              client->ps.stats[ STAT_STAMINA ] += STAMINA_MEDISTAT_RESTORE;
              client->medKitStaminaToRestore -= STAMINA_MEDISTAT_RESTORE;
              if( client->medKitStaminaToRestore > 0 )
                client->nextMedKitRestoreStaminaTime = level.time + MEDISTAT_REPEAT;
              else
                client->medKitStaminaToRestore = 0;
            } else
            {
              client->medKitStaminaToRestore = 0;
              client->ps.stats[ STAT_STAMINA ] = STAMINA_MAX;
            }
          } else if( client->medKitStaminaToRestore > 0 )
          {
            if( staminaDifference > client->medKitStaminaToRestore )
            {
              client->ps.stats[ STAT_STAMINA ] += client->medKitStaminaToRestore;
              client->medKitStaminaToRestore = 0;
            } else
            {
              client->medKitStaminaToRestore = 0;
              client->ps.stats[ STAT_STAMINA ] = STAMINA_MAX;
            }
          } else
            client->medKitStaminaToRestore = 0;
        }
      }
    }

    if( weapon == WP_ABUILD || weapon == WP_ABUILD2 ||
        client->ps.stats[ STAT_WEAPON ] == WP_HBUILD )
    {
        // Update build timer
        if( client->ps.misc[ MISC_MISC ] > 0 )
          client->ps.misc[ MISC_MISC ] -= 100;

        if( client->ps.misc[ MISC_MISC ] < 0 )
          client->ps.misc[ MISC_MISC ] = 0;
    }

    {
      qboolean send_replacable_buildables_message = qfalse;

      switch( weapon )
      {
        case WP_ABUILD:
        case WP_ABUILD2:
        case WP_HBUILD:

          // Set validity bit on buildable
          if( ( client->ps.stats[ STAT_BUILDABLE ] & ~SB_VALID_TOGGLEBIT ) > BA_NONE )
          {
            int     dist = BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->buildDist;
            vec3_t  dummy, dummy2;
            int     dummy3;

            if( G_CanBuild( ent, client->ps.stats[ STAT_BUILDABLE ] & ~SB_VALID_TOGGLEBIT,
                            dist, dummy, dummy2, &dummy3 ) == IBE_NONE )
              client->ps.stats[ STAT_BUILDABLE ] |= SB_VALID_TOGGLEBIT;
            else
              client->ps.stats[ STAT_BUILDABLE ] &= ~SB_VALID_TOGGLEBIT;

            // Let the client know which buildables will be removed by building
            for( i = 0; i < MAX_REPLACABLE_BUILDABLES; i++ )
            {
              if( i < level.numBuildablesForRemoval ) {
                if(
                  client->pers.replacable_buildables[ i ] !=
                  level.markedBuildables[ i ]->s.number) {
                    client->pers.replacable_buildables[ i ] =
                      level.markedBuildables[ i ]->s.number;
                    send_replacable_buildables_message = qtrue;
                  }
              } else {
                if(client->pers.replacable_buildables[ i ]) {
                  client->pers.replacable_buildables[ i ] = 0;
                  send_replacable_buildables_message = qtrue;
                }
              }
            }
          } else {
            for(i = 0; i < MAX_REPLACABLE_BUILDABLES; i++) {
              if(client->pers.replacable_buildables[ i ]) {
                client->pers.replacable_buildables[ i ] = 0;
                send_replacable_buildables_message = qtrue;
              }
            }
          }
          break;

        default:
          for(i = 0; i < MAX_REPLACABLE_BUILDABLES; i++) {
            if(client->pers.replacable_buildables[i]) {
              client->pers.replacable_buildables[i] = 0;
              send_replacable_buildables_message = qtrue;
            }
          }
          break;
      }

      if(send_replacable_buildables_message) {
        G_ReplacableBuildablesMessage(ent);

        //send to any following clients
        for(i = 0; i < level.maxclients; i++) {
          gentity_t *follower = &g_entities[i];

          //don't resend
          if(follower == ent) {
            continue;
          }

          if(
            !follower->client ||
            follower->client->pers.connected == CON_DISCONNECTED) {
            continue;
          }

          if(
            follower->client->sess.spectatorState == SPECTATOR_FOLLOW &&
            follower->client->sess.spectatorClient == ent->s.number) {
            int j;

            //copy the array
            for(j = 0; j < MAX_REPLACABLE_BUILDABLES; j++) {
              follower->client->pers.replacable_buildables[j] =
                client->pers.replacable_buildables[j];
            }
            
            G_ReplacableBuildablesMessage(follower);
          }
        }
      }
    }

    if( ent->client->pers.teamSelection == TEAM_HUMANS &&
        ( client->ps.stats[ STAT_STATE ] & SS_HEALING_2X ) )
    {
      int remainingStartupTime = MEDKIT_STARTUP_TIME - ( level.time - client->lastMedKitTime );

      if( remainingStartupTime < 0 )
      {
        if( ent->health < maxHealth &&
            ent->client->medKitHealthToRestore &&
            ent->client->ps.pm_type != PM_DEAD )
        {
          ent->client->medKitHealthToRestore -= BG_HP2SU( 1 );
          if( ent->client->medKitHealthToRestore < 0 )
            ent->client->medKitHealthToRestore = 0;
          ent->health += BG_HP2SU( 1 );
          if( ent->health > maxHealth )
            ent->health = maxHealth;
          ent->client->ps.misc[ MISC_HEALTH ] = ent->health;
          client->pers.infoChangeTime = level.time;
        }
        else
          ent->client->ps.stats[ STAT_STATE ] &= ~SS_HEALING_2X;
      }
      else
      {
        if( ent->health < maxHealth &&
            ent->client->medKitHealthToRestore &&
            ent->client->ps.pm_type != PM_DEAD )
        {
          //partial increase
          if( level.time > client->medKitIncrementTime )
          {
            ent->client->medKitHealthToRestore -= BG_HP2SU( 1 );
            if( ent->client->medKitHealthToRestore < 0 )
              ent->client->medKitHealthToRestore = 0;
            ent->health += BG_HP2SU( 1 );
            if( ent->health > maxHealth )
              ent->health = maxHealth;
            ent->client->ps.misc[ MISC_HEALTH ] = ent->health;
            client->pers.infoChangeTime = level.time;

            client->medKitIncrementTime = level.time +
              ( remainingStartupTime / MEDKIT_STARTUP_SPEED );
          }
        }
        else
          ent->client->ps.stats[ STAT_STATE ] &= ~SS_HEALING_2X;
      }
    }

    // jet fuel
    if( BG_InventoryContainsUpgrade( UP_JETPACK, ent->client->ps.stats ) )
    {
      if( BG_UpgradeIsActive( UP_JETPACK, ent->client->ps.stats ) )
      {
        if( ent->client->ps.stats[ STAT_FUEL ] > 0 )
        {
          // use fuel
          if( ent->client->ps.persistant[PERS_JUMPTIME] > JETPACK_ACT_BOOST_TIME )
            ent->client->ps.stats[ STAT_FUEL ] -= JETPACK_FUEL_USAGE;
          else
            ent->client->ps.stats[ STAT_FUEL ] -= JETPACK_ACT_BOOST_FUEL_USE;
          if( ent->client->ps.stats[ STAT_FUEL ] <= 0 )
          {
            ent->client->ps.stats[ STAT_FUEL ] = 0;
            BG_DeactivateUpgrade( UP_JETPACK, client->ps.stats );
            BG_AddPredictableEventToPlayerstate( EV_JETPACK_DEACTIVATE, 0, &client->ps );
          }
        }
      } else if( ent->client->ps.stats[ STAT_FUEL ] < JETPACK_FUEL_FULL &&
                 G_Reactor( ) &&
                  !(ent->client->ps.pm_flags & PMF_FEATHER_FALL) )
      {
        // recharge fuel
        ent->client->ps.stats[ STAT_FUEL ] += JETPACK_FUEL_RECHARGE;
        if( ent->client->ps.stats[ STAT_FUEL ] > JETPACK_FUEL_FULL )
          ent->client->ps.stats[ STAT_FUEL ] = JETPACK_FUEL_FULL;
      }
    }
  }

  //Camera Shake
    ent->client->ps.stats[ STAT_SHAKE ] *= 0.77f;
    if( ent->client->ps.stats[ STAT_SHAKE ] < 0 )
      ent->client->ps.stats[ STAT_SHAKE ] = 0;


  while( client->time1000 >= 1000 )
  {
    client->time1000 -= 1000;

    //client is poison clouded
    if( client->ps.eFlags & EF_POISONCLOUDED ) {
      G_Damage(
        ent, client->lastPoisonCloudedClient, client->lastPoisonCloudedClient,
        NULL, NULL, LEVEL1_PCLOUD_DMG, 0, MOD_LEVEL1_PCLOUD );
    }

    //client is poisoned
    if( client->ps.stats[ STAT_STATE ] & SS_POISONED )
    {
      int damage = ALIEN_POISON_DMG;

      if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, client->ps.stats ) )
        damage -= BSUIT_POISON_PROTECTION;

      if( BG_InventoryContainsUpgrade( UP_HELMET, client->ps.stats ) )
        damage -= HELMET_POISON_PROTECTION;

      if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, client->ps.stats ) )
        damage -= LIGHTARMOUR_POISON_PROTECTION;

      G_Damage( ent, client->lastPoisonClient, client->lastPoisonClient, NULL,
        0, damage, 0, MOD_POISON );
    }

    // turn off life support when a team admits defeat
    if( client->ps.stats[ STAT_TEAM ] == level.surrenderTeam )
    {
      int dmg = maxHealth / 20;
      if ( BG_ClassHasAbility(client->ps.stats[STAT_CLASS], SCA_REGEN) )
          dmg = BG_Class(client->ps.stats[STAT_CLASS])->regenRate;
      G_Damage( ent, NULL, NULL, NULL, NULL, dmg, DAMAGE_NO_ARMOR, MOD_SUICIDE );
    }

    // lose some voice enthusiasm
    if( client->voiceEnthusiasm > 0.0f )
      client->voiceEnthusiasm -= VOICE_ENTHUSIASM_DECAY;
    else
      client->voiceEnthusiasm = 0.0f;

    client->pers.secondsAlive++;
    if( g_freeFundPeriod.integer > 0 &&
        !(IS_SCRIM && !level.scrim.timed_income ) &&
        client->pers.secondsAlive % g_freeFundPeriod.integer == 0 )
    {
      // Give clients some credit periodically
      // (if not in warmup)
      if( G_TimeTilSuddenDeath( ) > 0 )
      {
        if( client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
          G_AddCreditToClient( client, FREEKILL_ALIEN, qtrue );
        else if( client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
          G_AddCreditToClient( client, FREEKILL_HUMAN, qtrue );
      }
    }

    G_ClientUpdateSpawnQueue( ent->client );
  }

  while( client->time10000 >= 10000 )
  {
    client->time10000 -= 10000;

    if( ent->client->ps.weapon == WP_ABUILD ||
        ent->client->ps.weapon == WP_ABUILD2 )
    {
      AddScore( ent, ALIEN_BUILDER_SCOREINC );
    }
    else if( ent->client->ps.weapon == WP_HBUILD )
    {
      AddScore( ent, HUMAN_BUILDER_SCOREINC );
    }

    // Give score to basis that healed other aliens
    if( ent->client->pers.hasHealed )
    {
      if( client->ps.weapon == WP_ALEVEL1 )
        AddScore( ent, LEVEL1_REGEN_SCOREINC );
      else if( client->ps.weapon == WP_ALEVEL1_UPG )
        AddScore( ent, LEVEL1_UPG_REGEN_SCOREINC );

      ent->client->pers.hasHealed = qfalse;
    }
  }

  // Regenerate Adv. Dragoon barbs
  if( client->ps.weapon == WP_ALEVEL3_UPG )
  {
    if( client->ps.ammo < BG_Weapon( WP_ALEVEL3_UPG )->maxAmmo )
    {
      if( ent->timestamp + LEVEL3_BOUNCEBALL_REGEN < level.time )
      {
        client->ps.ammo++;
        ent->timestamp = level.time;
      }
    }
    else
      ent->timestamp = level.time;
   }
}

/*
====================
ClientIntermissionThink
====================
*/
void ClientIntermissionThink( gclient_t *client )
{
  client->ps.eFlags &= ~EF_FIRING;
  client->ps.eFlags &= ~EF_FIRING2;

  // the level will exit when everyone wants to or after timeouts

  // swap and latch button actions
  client->oldbuttons = client->buttons;
  client->buttons = client->pers.cmd.buttons;
  if( client->buttons & ( ( !client->pers.swapAttacks ? BUTTON_ATTACK : BUTTON_ATTACK2 ) | BUTTON_USE_HOLDABLE ) & ( client->oldbuttons ^ client->buttons ) )
    client->readyToExit = 1;
}


/*
================
ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
void ClientEvents( gentity_t *ent, int oldEventSequence )
{
  int       i;
  int       event;
  gclient_t *client;
  int       damage;
  vec3_t    dir;
  vec3_t    point, mins;
  float     fallDistance;
  class_t   class;

  client = ent->client;
  class = client->ps.stats[ STAT_CLASS ];

  if( oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS )
    oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;

  for( i = oldEventSequence; i < client->ps.eventSequence; i++ )
  {
    event = client->ps.events[ i & ( MAX_PS_EVENTS - 1 ) ];

    switch( event )
    {
      case EV_FALL_MEDIUM:
      case EV_FALL_FAR:
        if( ent->s.eType != ET_PLAYER )
          break;    // not in the player model

        fallDistance = ( (float)client->ps.stats[ STAT_FALLDIST ] - MIN_FALL_DISTANCE ) /
                         ( MAX_FALL_DISTANCE - MIN_FALL_DISTANCE );

        if( fallDistance < 0.0f )
          fallDistance = 0.0f;
        else if( fallDistance > 1.0f )
          fallDistance = 1.0f;

        damage = (int)( (float)BG_Class( class )->health *
                 BG_Class( class )->fallDamage * fallDistance );

        VectorSet( dir, 0, 0, 1 );
        BG_ClassBoundingBox( class, mins, NULL, NULL, NULL, NULL );
        mins[ 0 ] = mins[ 1 ] = 0.0f;
        VectorAdd( client->ps.origin, mins, point );

        ent->pain_debounce_time = level.time + 200; // no normal pain sound
        G_Damage( ent, NULL, NULL, dir, point, damage, DAMAGE_NO_LOCDAMAGE, MOD_FALLING );
        break;

      case EV_FIRE_WEAPON:
        FireWeapon( ent );
        break;

      case EV_FIRE_WEAPON2:
        FireWeapon2( ent );
        break;

      case EV_FIRE_WEAPON3:
        FireWeapon3( ent );
        break;

      case EV_NOAMMO:
        break;

      default:
        break;
    }
  }
}


/*
==============
SendPendingPredictableEvents
==============
*/
void SendPendingPredictableEvents( playerState_t *ps )
{
  gentity_t *t;
  int       event, seq;
  int       extEvent, number;

  // if there are still events pending
  if( ps->entityEventSequence < ps->eventSequence )
  {
    // create a temporary entity for this event which is sent to everyone
    // except the client who generated the event
    seq = ps->entityEventSequence & ( MAX_PS_EVENTS - 1 );
    event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
    // set external event to zero before calling BG_PlayerStateToEntityState
    extEvent = ps->externalEvent;
    ps->externalEvent = 0;
    // create temporary entity for event
    t = G_TempEntity( ps->origin, event );
    number = t->s.number;
    BG_PlayerStateToEntityState( ps, &t->s );
    t->s.number = number;
    t->s.eType = ET_EVENTS + event;
    t->s.eFlags |= EF_PLAYER_EVENT;
    t->s.otherEntityNum = ps->clientNum;
    // send to everyone except the client who generated the event
    t->r.svFlags |= SVF_NOTSINGLECLIENT;
    t->r.singleClient = ps->clientNum;
    // set back external event
    ps->externalEvent = extEvent;
  }
}

/*
=====================
G_CanActivateEntity

Determines if a given client can activate a given entity.
Primarily used by G_FindActivationEnt().
=====================
*/
qboolean G_CanActivateEntity( gclient_t *client, gentity_t *ent )
{
  if( !ent || ent->s.number == ENTITYNUM_NONE ||
      !ent->activation.activate || !client )
    return qfalse;

  // team check
  if( ( ent->activation.flags & ACTF_TEAM ) &&
      ent->buildableTeam != client->ps.stats[ STAT_TEAM ] )
    return qfalse;

  // entity spawned check
  if( ( ent->activation.flags & ACTF_SPAWNED ) &&
      !ent->spawned )
    return qfalse;

  // entity alive check
  if( ( ent->activation.flags & ACTF_ENT_ALIVE ) &&
      ent->health <= 0 )
    return qfalse;

  // player alive check
  if( ( ent->activation.flags & ACTF_PL_ALIVE ) &&
      !PM_Alive( client->ps.pm_type ) )
    return qfalse;

  // check if this entity must be stood on to activate
  if( ( ent->activation.flags & ACTF_GROUND ) &&
      client->ps.groundEntityNum != ent->s.number )
    return qfalse;

  // custom canActivate() check
  if( ent->activation.canActivate && !ent->activation.canActivate( ent, client ) )
    return qfalse;

  // entity line of sight check
  if( ( ent->activation.flags & ACTF_LINE_OF_SIGHT ) &&
      !G_Visible( &g_entities[ client->ps.clientNum ], ent, MASK_DEADSOLID ) )
    return qfalse;

  return qtrue;
}

/*
=====================
G_FindActivationEnt

Determines if there is a nearby entity the client can activate
=====================
*/
static void G_FindActivationEnt( gentity_t *ent )
{
  gclient_t *client;
  trace_t   trace;
  vec3_t    view, point;
  gentity_t *traceEnt, *groundEnt;
  int       entityList[ MAX_GENTITIES ];
  vec3_t    mins, maxs;
  int       i, num;

  if( !( client = ent->client ) )
    return;

  // check if the client is occupying an activation entity
  if ( client->ps.eFlags & EF_OCCUPYING )
    return;

  // reset PERS_ACT_ENT
  client->ps.persistant[ PERS_ACT_ENT ] = ENTITYNUM_NONE;
  ent->occupation.occupied = NULL;

  groundEnt = &g_entities[ client->ps.groundEntityNum ];

  // look for an activation entity infront of player
  AngleVectors( client->ps.viewangles, view, NULL, NULL );
  VectorMA( client->ps.origin, ACTIVATION_ENT_RANGE, view, point );
  SV_Trace( &trace, client->ps.origin, NULL, NULL, point, ent->s.number, MASK_SHOT, TT_AABB );

  traceEnt = &g_entities[ trace.entityNum ];

  if( ent->activation.usable_map_trigger != ENTITYNUM_NONE ) {
    client->ps.persistant[ PERS_ACT_ENT ] = ent->activation.usable_map_trigger;
    ent->occupation.occupied = &g_entities[ent->activation.usable_map_trigger];
  }
  else if( G_CanActivateEntity( client, traceEnt ) )
  {
    client->ps.persistant[ PERS_ACT_ENT ] = traceEnt->s.number;
    ent->occupation.occupied = traceEnt;
  }
  else if( ( groundEnt->activation.flags & ACTF_GROUND ) &&
           G_CanActivateEntity( client, groundEnt ) )
  {
    // ACTF_GROUND activation entities get preference after activation entities in front of the client
    client->ps.persistant[ PERS_ACT_ENT ] = client->ps.groundEntityNum;
    ent->occupation.occupied = groundEnt;
  }
  else
  {
    vec3_t    range = { ACTIVATION_ENT_RANGE, ACTIVATION_ENT_RANGE, ACTIVATION_ENT_RANGE };
    float activationEntDistance, bestDistance = ( 2 * Distance( vec3_origin, range) );

    // do a small area search

    VectorAdd( client->ps.origin, range, maxs );
    VectorSubtract( client->ps.origin, range, mins );

    num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
    for( i = 0; i < num; i++ )
    {
      traceEnt = &g_entities[ entityList[ i ] ];

      if( G_CanActivateEntity( client, traceEnt ) )
      {
        SV_Trace( &trace, client->ps.origin, NULL, NULL, traceEnt->r.currentOrigin,
          client->ps.clientNum, MASK_PLAYERSOLID, TT_AABB );

        if( trace.fraction < 1.0f && trace.entityNum == traceEnt->s.number )
          activationEntDistance = Distance( client->ps.origin, trace.endpos );
        else
          activationEntDistance = Distance( client->ps.origin, traceEnt->r.currentOrigin );

        if( activationEntDistance < bestDistance )
        {
          client->ps.persistant[ PERS_ACT_ENT ] = traceEnt->s.number;
          ent->occupation.occupied = traceEnt;
          bestDistance = activationEntDistance;
        }
      }
    }
  }
}

/*
=====================
G_OvrdActMenuMsg

This is used for activation menu messages that can be overriden by the
activation.menuMsgOvrd[] array.  See actMNOvrdIndex_t for information
regarding each index.
=====================
*/
void G_OvrdActMenuMsg( gentity_t *activator, actMNOvrdIndex_t index, dynMenu_t defaultMenu )
{
  if( activator->occupation.occupied->activation.menuMsgOvrd[ index ] )
    activator->activation.menuMsg = activator->occupation.occupied->activation.menuMsgOvrd[ index ];
  else
    activator->activation.menuMsg = defaultMenu;
}

/*
=====================
G_WillActivateEntity

Determines if a given client will activate the nearby found activation entity
=====================
*/
qboolean G_WillActivateEntity( gentity_t *actEnt, gentity_t *activator )
{
  // set the general menu message for activation failure.
  G_OvrdActMenuMsg( activator, ACTMN_ACT_FAILED, MN_ACT_FAILED );

  if ( ( actEnt->activation.flags & ACTF_POWERED ) &&
       !actEnt->powered )
  {
    // this entitity must be powered to use
    switch ( actEnt->buildableTeam )
    {
      case TEAM_HUMANS:
        G_OvrdActMenuMsg( activator, ACTMN_H_NOTPOWERED, MN_H_NOTPOWERED );
        break;
      case TEAM_ALIENS:
        G_OvrdActMenuMsg( activator, ACTMN_ACT_NOTCONTROLLED, MN_ACT_NOTCONTROLLED );
        break;
      default:
        G_OvrdActMenuMsg( activator, ACTMN_ACT_NOTPOWERED, MN_ACT_NOTPOWERED );
        break;
    }

    return qfalse;
  }

  if( actEnt->activation.flags & ACTF_OCCUPY )
  {
    if( ( actEnt->flags & FL_OCCUPIED ) &&
        actEnt->occupation.occupant != activator )
    {
      // only the occupant of this activation entity can activate it when occupied
      G_OvrdActMenuMsg( activator, ACTMN_ACT_OCCUPIED, MN_ACT_OCCUPIED );
      return qfalse;
    }

    actEnt->occupation.occupantFound = NULL;
    if( actEnt->occupation.findOccupant )
      actEnt->occupation.findOccupant( actEnt, activator );
    else
      actEnt->occupation.occupantFound = activator;

    if( !actEnt->occupation.occupantFound )
    {
      G_OvrdActMenuMsg( activator, ACTMN_ACT_NOOCCUPANTS, MN_ACT_NOOCCUPANTS );
      return qfalse;
    }

    if( ( ( actEnt->occupation.occupantFound->client &&
          ( actEnt->occupation.occupantFound->client->ps.eFlags &
            EF_OCCUPYING ) ) || ( !( actEnt->occupation.occupantFound->client ) &&
          ( actEnt->occupation.occupantFound->s.eFlags & EF_OCCUPYING ) ) ) &&
          !( ( actEnt->flags & FL_OCCUPIED ) &&
             ( actEnt->occupation.occupant ==
                                          actEnt->occupation.occupantFound ) ) )
    {
      // potential occupant is preoccupied with another activation entity
      G_OvrdActMenuMsg( activator, ACTMN_ACT_OCCUPYING, MN_ACT_OCCUPYING );
      return qfalse;
    }
  }

  if( actEnt->occupation.findOther )
  {
    actEnt->occupation.other = NULL;
    actEnt->occupation.findOther( actEnt, activator );
  } else
    actEnt->occupation.other = activator;

  if( actEnt->activation.willActivate )
    if( !actEnt->activation.willActivate( actEnt, activator ) )
    {
      actEnt->occupation.other = NULL;
      return qfalse;
    }

  return qtrue;
}

/*
=====================
_G_LoggedActivateEnt

A wrapper for optionally logging activation details
=====================
*/
static qboolean _G_LoggedActivateEnt(gentity_t *actEnt, gentity_t *activator) {
  if( g_debugAMP.integer ) {
    char *s;
    s = va(
      "activated by #%i (%s^7)", 
      (int)( activator-g_entities ),
      activator->client->pers.netname);
    return G_LoggedActivation(
      actEnt, activator, activator, NULL, s, LOG_ACT_ACTIVATE);
  }

  return actEnt->activation.activate( actEnt, activator);
}

/*
=====================
G_ActivateEntity

This is a general wrapper for activation.(*activate)()
=====================
*/
void G_ActivateEntity(gentity_t *actEnt, gentity_t *activator) {
  if(!activator->client) {
    return;
  }

  if(G_WillActivateEntity(actEnt, activator)) {
    if(
      _G_LoggedActivateEnt(actEnt, activator) &&
      (actEnt->activation.flags & ACTF_OCCUPY)) {
      //occupy the activation entity
      G_OccupyEnt(actEnt);
    }
  } else if(activator->activation.menuMsg) {
    G_TriggerMenu(
      activator->client->ps.clientNum,
      activator->activation.menuMsg);
  }
}

/*
=====================
G_ResetOccupation

This is called to reset occupation entities
=====================
*/
void G_ResetOccupation( gentity_t *occupied, gentity_t *occupant )
{
  if( occupied )
  {
    if( occupied->occupation.occupiedReset )
      occupied->occupation.occupiedReset( occupied );

    if( ( occupied->occupation.flags & OCCUPYF_RESET_OTHER ) &&
         occupied->occupation.other &&
         ( occupied->occupation.other->flags & FL_OCCUPIED ) )
    {
      if( occupied->occupation.other->occupation.other == occupied )
        occupied->occupation.other->occupation.other = NULL;

      if( occupant &&
          occupied->occupation.other->occupation.occupant == occupant )
        occupied->occupation.other->occupation.occupant = NULL;

      G_ResetOccupation( occupied->occupation.other,
                         occupied->occupation.other->occupation.occupant );
    }

    occupied->occupation.occupant = NULL;
    occupied->occupation.other = NULL;
    occupied->flags &= ~FL_OCCUPIED;
  }

  if( occupant )
  {
    if( occupant->occupation.occupied &&
        occupant->occupation.occupied->occupation.occupantReset )
      occupant->occupation.occupied->occupation.occupantReset( occupant );

    occupant->occupation.occupied = NULL;

    if( occupant->client )
      occupant->client->ps.eFlags &= ~EF_OCCUPYING;
    else
      occupant->s.eFlags &= ~EF_OCCUPYING;

    G_OccupantClip( occupant );
  }

  return;
}

/*
=====================
G_UnoccupyEnt

This is a general wrapper for (*unoccupy)().
This is called to allow a player to leave an
occupiable activation entity.  When force is set to
qtrue, the player will definitely unoccupy the entity.
=====================
*/
void G_UnoccupyEnt( gentity_t *occupied, gentity_t *occupant, gentity_t *activator, qboolean force )
{
  if( force )
  {
    if( occupied->occupation.unoccupy )
      occupied->occupation.unoccupy( occupied, occupant, activator, qtrue );

    G_ResetOccupation( occupied, occupant );
  } else if( occupied->occupation.unoccupy )
  {
    if( occupied->occupation.unoccupy( occupied, occupant, activator, qfalse ) )
      G_ResetOccupation( occupied, occupant );
    else
    {
      G_OvrdActMenuMsg( activator, ACTMN_ACT_NOEXIT, MN_ACT_NOEXIT );
      G_TriggerMenu( activator->client->ps.clientNum, activator->activation.menuMsg );
    }
  } else
    G_ResetOccupation( occupied, occupant );
}

/*
===============
G_OccupyEnt

Called to occupy an entity with found potential occupants.
===============
*/
void G_OccupyEnt( gentity_t *occupied )
{
  if( !occupied->occupation.occupantFound )
    return;

  occupied->occupation.occupant = occupied->occupation.occupantFound;
  occupied->occupation.occupantFound = NULL;
  occupied->occupation.occupant->occupation.occupied = occupied;

  occupied->flags |= FL_OCCUPIED;
  if( occupied->occupation.occupant->client )
  {
    occupied->occupation.occupant->client->ps.eFlags |= EF_OCCUPYING;
    occupied->occupation.occupant->client->ps.persistant[ PERS_ACT_ENT ] =
                                                             occupied->s.number;
  } else
    occupied->occupation.occupant->s.eFlags |= EF_OCCUPYING;

  G_OccupantClip( occupied->occupation.occupant );

  if( occupied->occupation.occupy )
    occupied->occupation.occupy( occupied );

  return;
}

/*
===============
G_SetClipmask

Used to set the clip mask of an entity taking into account
the temporary OccupantClip condition.
===============
*/
void G_SetClipmask( gentity_t *ent, int clipmask )
{
  if( ent->occupation.occupantFlags & OCCUPYF_CLIPMASK )
    ent->occupation.unoccupiedClipMask = clipmask;
  else
  {
    ent->clipmask = clipmask;
  }
}

/*
===============
G_SetContents

Used to set the contents of an entity taking into account
the temporary OccupantClip and noclip conditions.
===============
*/
void G_SetContents( gentity_t *ent, int contents ) {
  if( ent->occupation.occupantFlags & OCCUPYF_CONTENTS ) {
    ent->occupation.unoccupiedContents = contents;
  } else {
    if( ent->client && ent->client->noclip ) {
      ent->client->cliprcontents = contents;
    } else {
      ent->r.contents = contents;
    }
  }
}

/*
===============
G_BackupUnoccupyClipmask

Provides a backup of the entity's clip mask so that it can be restored after
unoccupying an occupiable entity that alters the clip mask of its occupants.
===============
*/
void G_BackupUnoccupyClipmask( gentity_t *ent )
{
  ent->occupation.unoccupiedClipMask = ent->clipmask;

  return;
}

/*
===============
G_BackupUnoccupyContents

Provides a backup of the entity's contents so that it can be restored after
unoccupying an occupiable entity that alters the contents of its occupants.
===============
*/
void G_BackupUnoccupyContents( gentity_t *ent )
{
  if( ent->client && ent->client->noclip )
    ent->occupation.unoccupiedContents = ent->client->cliprcontents;
  else
    ent->occupation.unoccupiedContents = ent->r.contents;

  return;
}

/*
===============
G_OccupantClip

Sets the clip mask and/or the contents of
activation entity occupants
===============
*/
void G_OccupantClip( gentity_t *occupant )
{
  if( occupant->occupation.occupied &&
      ( ( occupant->client &&
          ( occupant->client->ps.eFlags & EF_OCCUPYING ) ) ||
        ( !occupant->client && ( occupant->s.eFlags & EF_OCCUPYING ) ) ) )
  {
    if( ( occupant->occupation.occupied->occupation.flags & OCCUPYF_CLIPMASK ) &&
        !( occupant->occupation.occupantFlags & OCCUPYF_CLIPMASK ) )
    {
      G_BackupUnoccupyClipmask( occupant );
      occupant->clipmask = occupant->occupation.occupied->occupation.clipMask;
      occupant->occupation.occupantFlags |= OCCUPYF_CLIPMASK;
    }

    if( ( occupant->occupation.occupied->occupation.flags & OCCUPYF_CONTENTS ) &&
          !( occupant->occupation.occupantFlags & OCCUPYF_CONTENTS )  )
    {
      G_BackupUnoccupyContents( occupant );
      if( occupant->client && occupant->client->noclip )
        occupant->client->cliprcontents =
                             occupant->occupation.occupied->occupation.contents;
      else {
        occupant->r.contents =
                             occupant->occupation.occupied->occupation.contents;
      }

      occupant->occupation.occupantFlags |= OCCUPYF_CONTENTS;
    }
  } else
  {
    if( occupant->occupation.occupantFlags & OCCUPYF_CLIPMASK )
    {
      occupant->clipmask = occupant->occupation.unoccupiedClipMask;
      occupant->occupation.occupantFlags &= ~OCCUPYF_CLIPMASK;
    }

    if( occupant->occupation.occupantFlags & OCCUPYF_CONTENTS )
    {
      if( occupant->client && occupant->client->noclip )
        occupant->client->cliprcontents = occupant->occupation.unoccupiedContents;
      else
        occupant->r.contents = occupant->occupation.unoccupiedContents;

      occupant->occupation.occupantFlags &= ~OCCUPYF_CONTENTS;
    }
  }
}

/*
===============
G_OccupantThink

This runs every frame for each entity
===============
*/
void G_OccupantThink( gentity_t *occupant )
{
  gentity_t *occupied;

  occupied = occupant->occupation.occupied;

  if( ( occupant->client && ( occupant->client->ps.eFlags & EF_OCCUPYING ) ) ||
      ( !occupant->client && ( occupant->s.eFlags & EF_OCCUPYING ) ) )
  {
    if( occupied )
    {
      if( occupied->flags & FL_OCCUPIED )
      {
        if( occupied->occupation.occupyUntil &&
            occupied->occupation.occupyUntil( occupied, occupant ) )
          G_UnoccupyEnt( occupied, occupant, occupant, qtrue );
        else if( ( occupied->occupation.flags & OCCUPYF_ACTIVATE ) &&
                   occupant->client )
        {
          if( occupied->occupation.flags & OCCUPYF_UNTIL_INACTIVE )
          {
            if( !G_CanActivateEntity( occupant->client, occupied ) )
              G_UnoccupyEnt( occupied, occupant, occupant, qtrue );
            else if( !G_WillActivateEntity( occupied, occupant ) )
            {
              G_UnoccupyEnt( occupied, occupant, occupant, qtrue );
             if( occupant->activation.menuMsg )
               G_TriggerMenu( occupant->client->ps.clientNum,
                              occupant->activation.menuMsg );
            }
            else if(!_G_LoggedActivateEnt(occupied, occupant))
              G_UnoccupyEnt( occupied, occupant, occupant, qtrue );
          } else if( G_CanActivateEntity( occupant->client, occupied ) &&
                     G_WillActivateEntity( occupied, occupant ) )
          {
            if(_G_LoggedActivateEnt(occupied, occupant))
            {
              //occupy the activation entity
              G_OccupyEnt( occupied );
            }
          }
        }
      } else
      {
        // this entity isn't actually occupying
        G_ResetOccupation( occupied, occupant );
      }
    } else
    {
      // this entity isn't actually occupying
      G_ResetOccupation( NULL, occupant );
    }
  } else if( occupied )
  {
    // this entity shouldn't be occupying
    G_ResetOccupation( NULL, occupant );
  }

  //for non-client entities
  if( !( occupant->client ) )
  {
    // set the clip mask and/or the contents of entities that occupied an
    // activation entity
    G_OccupantClip( occupant );
  }
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
void ClientThink_real( gentity_t *ent )
{
  gclient_t *client = ent->client;
  pmove_t   pm;
  vec3_t    up = { 0.0f, 0.0f, 1.0f };
  const int maxHealth = BG_Class( client->ps.stats[ STAT_CLASS ] )->health;
  int       oldEventSequence;
  int       msec;
  int       unlaggedTime;
  usercmd_t *ucmd;
  int       i;

  ComparePreviousCmdAngles( client );

  VoterInactivityTimer( ent );

  // don't think if the client is not yet connected (and thus not yet spawned in)
  if( client->pers.connected != CON_CONNECTED )
    return;

  // mark the time, so the connection sprite can be removed
  ucmd = &ent->client->pers.cmd;

  // sanity check the command time to prevent speedup cheating
  if( ucmd->serverTime > level.time + 200 )
    ucmd->serverTime = level.time + 200;

  if( ucmd->serverTime < level.time - 1000 )
    ucmd->serverTime = level.time - 1000;

  msec = ucmd->serverTime - client->ps.commandTime;
  // following others may result in bad times, but we still want
  // to check for follow toggles
  if( msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW )
    return;

  if( msec > 200 )
    msec = 200;

  unlaggedTime = ucmd->serverTime;

  if( pmove_msec.integer < 8 )
    Cvar_SetSafe( "pmove_msec", "8" );
  else if( pmove_msec.integer > 33 )
    Cvar_SetSafe( "pmove_msec", "33" );

  if( pmove_fixed.integer || client->pers.pmoveFixed )
  {
    ucmd->serverTime = ( ( ucmd->serverTime + pmove_msec.integer - 1 ) / pmove_msec.integer ) * pmove_msec.integer;
    //if (ucmd->serverTime - client->ps.commandTime <= 0)
    //  return;
  }

  //
  // check for exiting intermission
  //
  if( level.intermissiontime )
  {
    ClientIntermissionThink( client );
    return;
  }

  // check for inactivity timer, but never drop the local client of a non-dedicated server
  if( ( client->pers.teamSelection != TEAM_NONE ) &&
      !ClientInactivityTimer( ent ) )
    return;

  if(client->pers.teamSelection != TEAM_NONE) {
    client->ps.persistant[ PERS_BP ] = G_GetBuildPoints( client->ps.origin,
      client->ps.stats[ STAT_TEAM ] );
    client->ps.persistant[ PERS_MARKEDBP ] = G_GetMarkedBuildPoints( &client->ps );

    if( client->ps.persistant[ PERS_BP ] < 0 )
      client->ps.persistant[ PERS_BP ] = 0;
  }

  // spectators don't do much
  if( client->sess.spectatorState != SPECTATOR_NOT )
  {
    if( client->sess.spectatorState == SPECTATOR_SCOREBOARD )
      return;

    SpectatorThink( ent, ucmd );
    return;
  }

  G_namelog_update_score( client );

  // check for inactivity timer, but never drop the local client of a non-dedicated server
  if( !ClientInactivityTimer( ent ) )
    return;

  // calculate where ent is currently seeing all the other active clients
  G_UnlaggedCalc( unlaggedTime, ent );

  if(
    client->ps.misc[ MISC_HEALTH ] > 0 &&
    (
      !G_TakesDamage( ent ) ||
      (g_damageProtection.integer && ent->dmgProtectionTime > level.time) ||
      ( ent->flags & FL_GODMODE ) ||
      !ent->r.contents ) )
    client->ps.eFlags |= EF_INVINCIBLE;
  else if( client->ps.eFlags & EF_INVINCIBLE )
  {
    if( client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      G_AddPredictableEvent( ent, EV_ALIEN_SPAWN_PROTECTION_ENDED,
                             DirToByte( up ) );
    client->ps.eFlags &= ~EF_INVINCIBLE;
  }

  if( client->noclip )
    client->ps.pm_type = PM_NOCLIP;
  else if( client->ps.misc[ MISC_HEALTH ] <= 0 )
  {
    client->ps.pm_type = PM_DEAD;

    // reset any activation entities the player might be occupying
    if( client->ps.eFlags & EF_OCCUPYING )
      G_ResetOccupation( ent->occupation.occupied, ent );
  }
  else if ( ( client->ps.eFlags & EF_OCCUPYING ) &&
            ent->occupation.occupied &&
            ( ent->occupation.occupied->occupation.flags &
              OCCUPYF_PM_TYPE ) )
    client->ps.pm_type = ent->occupation.occupied->occupation.pm_type;
  else if( BG_InventoryContainsUpgrade( UP_JETPACK, client->ps.stats ) &&
           BG_UpgradeIsActive( UP_JETPACK, client->ps.stats ) )
    client->ps.pm_type = PM_JETPACK;
  else if( client->ps.stats[ STAT_STATE ] & SS_BLOBLOCKED ||
           client->ps.stats[ STAT_STATE ] & SS_GRABBED )
    client->ps.pm_type = PM_GRABBED;
  else
    client->ps.pm_type = PM_NORMAL;

  if( ( client->ps.stats[ STAT_STATE ] & SS_GRABBED ) &&
      client->grabExpiryTime < level.time )
    client->ps.stats[ STAT_STATE ] &= ~SS_GRABBED;

  if( ( client->ps.stats[ STAT_STATE ] & SS_BLOBLOCKED ) &&
      client->lastLockTime + LOCKBLOB_LOCKTIME < level.time )
    client->ps.stats[ STAT_STATE ] &= ~SS_BLOBLOCKED;

  if( ( client->ps.stats[ STAT_STATE ] & SS_SLOWLOCKED ) &&
      client->lastSlowTime + ABUILDER_BLOB_TIME < level.time )
    client->ps.stats[ STAT_STATE ] &= ~SS_SLOWLOCKED;

  // Update boosted state flags
  client->ps.stats[ STAT_STATE ] &= ~SS_BOOSTEDWARNING;
  if( client->ps.stats[ STAT_STATE ] & SS_BOOSTED )
  {
    if( level.time - client->boostedTime >= BOOST_TIME )
      client->ps.stats[ STAT_STATE ] &= ~SS_BOOSTED;
    else if( level.time - client->boostedTime >= BOOST_WARN_TIME )
      client->ps.stats[ STAT_STATE ] |= SS_BOOSTEDWARNING;
  }

  // Check if poison cloud has worn off
  if( ( client->ps.eFlags & EF_POISONCLOUDED ) &&
      BG_PlayerPoisonCloudTime( &client->ps ) - level.time +
      client->lastPoisonCloudedTime <= 0 )
    client->ps.eFlags &= ~EF_POISONCLOUDED;

  if( client->ps.stats[ STAT_STATE ] & SS_POISONED &&
      client->lastPoisonTime + ALIEN_POISON_TIME < level.time )
    client->ps.stats[ STAT_STATE ] &= ~SS_POISONED;

  client->ps.gravity = g_gravity.value;

  if( BG_InventoryContainsUpgrade( UP_MEDKIT, client->ps.stats ) &&
      BG_UpgradeIsActive( UP_MEDKIT, client->ps.stats ) )
  {
    //if currently using a medkit or have no need for a medkit now
    if( client->ps.stats[ STAT_STATE ] & SS_HEALING_2X ||
        ( client->ps.misc[ MISC_HEALTH ] == maxHealth &&
          !( client->ps.stats[ STAT_STATE ] & SS_POISONED ) ) )
    {
      BG_DeactivateUpgrade( UP_MEDKIT, client->ps.stats );
    }
    else if( client->ps.misc[ MISC_HEALTH ] > 0 )
    {
      //remove anti toxin
      BG_DeactivateUpgrade( UP_MEDKIT, client->ps.stats );
      BG_RemoveUpgradeFromInventory( UP_MEDKIT, client->ps.stats );

      client->ps.stats[ STAT_STATE ] &= ~SS_POISONED;
      client->poisonImmunityTime = level.time + MEDKIT_POISON_IMMUNITY_TIME;

      // restore stamina
      if( ( client->medKitStaminaToRestore =
                            ( STAMINA_MAX - client->ps.stats[ STAT_STAMINA ] ) ) )
      {
        if( client->medKitStaminaToRestore > STAMINA_MEDISTAT_RESTORE )
        {
          client->ps.stats[ STAT_STAMINA ] += STAMINA_MEDISTAT_RESTORE;
          client->medKitStaminaToRestore -= STAMINA_MEDISTAT_RESTORE;
          if( client->medKitStaminaToRestore > 0 )
            client->nextMedKitRestoreStaminaTime = level.time + MEDISTAT_REPEAT;
          else
            client->medKitStaminaToRestore = 0;
        } else
          client->ps.stats[ STAT_STAMINA ] = STAMINA_MAX;
      }

      client->ps.stats[ STAT_STATE ] |= SS_HEALING_2X;
      client->lastMedKitTime = level.time;
      client->medKitHealthToRestore = maxHealth - client->ps.misc[ MISC_HEALTH ];
      client->medKitIncrementTime = level.time +
        ( MEDKIT_STARTUP_TIME / MEDKIT_STARTUP_SPEED );

      G_AddEvent( ent, EV_MEDKIT_USED, 0 );
    }
  }

  // Replenish alien health
  if( level.surrenderTeam != client->pers.teamSelection &&
      ent->nextRegenTime >= 0 && ent->nextRegenTime < level.time )
  {
    float regenRate =
        BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->regenRate;

    if( ent->health <= 0 || ent->nextRegenTime < 0 || regenRate == 0 )
      ent->nextRegenTime = -1; // no regen
    else
    {
      int       entityList[ MAX_GENTITIES ];
      int       num;
      int       count, interval;
      vec3_t    range, mins, maxs;
      float     modifier = 1.0f;

      VectorSet( range, REGEN_BOOST_RANGE, REGEN_BOOST_RANGE,
                 REGEN_BOOST_RANGE );
      VectorAdd( client->ps.origin, range, maxs );
      VectorSubtract( client->ps.origin, range, mins );

      num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
      for( i = 0; i < num; i++ )
      {
        gentity_t *boost = &g_entities[ entityList[ i ] ];

        if( Distance( client->ps.origin, boost->r.currentOrigin ) > REGEN_BOOST_RANGE )
          continue;

        if( modifier < BOOSTER_REGEN_MOD && boost->s.eType == ET_BUILDABLE &&
            boost->s.modelindex == BA_A_BOOSTER && boost->spawned &&
            boost->health > 0 && boost->powered )
        {
          modifier = BOOSTER_REGEN_MOD;
          continue;
        }

        if( boost->s.eType == ET_PLAYER && boost->client &&
            boost->client->pers.teamSelection ==
              ent->client->pers.teamSelection && boost->health > 0 )
        {
          class_t class = boost->client->ps.stats[ STAT_CLASS ];
          qboolean didBoost = qfalse;

          //don't give a heal boost to self
          if( boost->s.number == ent->s.number ) {
            continue;
          }

          if( class == PCL_ALIEN_LEVEL1 && modifier < LEVEL1_REGEN_MOD )
          {
            modifier = LEVEL1_REGEN_MOD;
            didBoost = qtrue;
          }
          else if( class == PCL_ALIEN_LEVEL1_UPG &&
                   modifier < LEVEL1_UPG_REGEN_MOD )
          {
            modifier = LEVEL1_UPG_REGEN_MOD;
            didBoost = qtrue;
          }

          if( didBoost && ent->health < maxHealth )
            boost->client->pers.hasHealed = qtrue;
        }
      }

      // Transmit heal rate to the client so it can be displayed on the HUD
      client->ps.stats[ STAT_STATE ] |= SS_HEALING_ACTIVE;
      client->ps.stats[ STAT_STATE ] &= ~( SS_HEALING_2X | SS_HEALING_3X );
      if( modifier == 1.0f && !G_FindCreep( ent ) )
      {
        client->ps.stats[ STAT_STATE ] &= ~SS_HEALING_ACTIVE;
        modifier *= ALIEN_REGEN_NOCREEP_MOD;
      }
      else if( modifier >= 3.0f )
        client->ps.stats[ STAT_STATE ] |= SS_HEALING_3X;
      else if( modifier >= 2.0f )
        client->ps.stats[ STAT_STATE ] |= SS_HEALING_2X;

      interval = 1000 / ( regenRate * modifier );
      if( !interval )
      {
        // interval is less than one millisecond
        regenRate = ( regenRate * modifier ) / 1000;
        count = ( level.time - ent->nextRegenTime - 1 ) * regenRate;
        ent->nextRegenTime = level.time + 1;
      } else
      {
        // if recovery interval is less than frametime, compensate
        count = 1 + ( level.time - ent->nextRegenTime ) / interval;
        ent->nextRegenTime += count * interval;
      }

      if( ent->health < maxHealth )
      {
        ent->health += count;
        client->ps.misc[ MISC_HEALTH ] = ent->health;
        client->pers.infoChangeTime = level.time;

        // if at max health, clear damage counters
        if( ent->health >= maxHealth )
        {
          ent->health = client->ps.misc[ MISC_HEALTH ] = maxHealth;
          for( i = 0; i < MAX_CLIENTS; i++ )
            ent->credits[ i ] = 0;
        }
      }
    }
  }

  if( BG_InventoryContainsUpgrade( UP_GRENADE, client->ps.stats ) &&
      BG_UpgradeIsActive( UP_GRENADE, client->ps.stats ) )
  {
    int lastWeapon = ent->s.weapon;

    //remove grenade
    BG_DeactivateUpgrade( UP_GRENADE, client->ps.stats );
    BG_RemoveUpgradeFromInventory( UP_GRENADE, client->ps.stats );

    //M-M-M-M-MONSTER HACK
    ent->s.weapon = WP_GRENADE;
    FireWeapon( ent );
    ent->s.weapon = lastWeapon;
  }

  if( BG_InventoryContainsUpgrade( UP_FRAGNADE, client->ps.stats ) &&
      BG_UpgradeIsActive( UP_FRAGNADE, client->ps.stats ) )
  {
    int lastWeapon = ent->s.weapon;

    //remove grenade
    BG_DeactivateUpgrade( UP_FRAGNADE, client->ps.stats );
    BG_RemoveUpgradeFromInventory( UP_FRAGNADE, client->ps.stats );

    //M-M-M-M-MONSTER HACK
    ent->s.weapon = WP_FRAGNADE;
    FireWeapon( ent );
    ent->s.weapon = lastWeapon;
  }

  if( BG_InventoryContainsUpgrade( UP_LASERMINE, client->ps.stats ) &&
      BG_UpgradeIsActive( UP_LASERMINE, client->ps.stats ) )
  {
    int lastWeapon = ent->s.weapon;

    //remove lasermine
    BG_DeactivateUpgrade( UP_LASERMINE, client->ps.stats );
    BG_RemoveUpgradeFromInventory( UP_LASERMINE, client->ps.stats );

    //M-M-M-M-MONSTER HACK
    ent->s.weapon = WP_LASERMINE;
    FireWeapon( ent );
    ent->s.weapon = lastWeapon;
  }

  // set speed
  if( client->ps.pm_type == PM_NOCLIP )
    client->ps.speed = client->pers.flySpeed;
  else
    client->ps.speed = g_speed.value *
        BG_Class( client->ps.stats[ STAT_CLASS ] )->speed;

  if( client->lastCreepSlowTime + CREEP_TIMEOUT < level.time )
    client->ps.stats[ STAT_STATE ] &= ~SS_CREEPSLOWED;

  //randomly disable the jet pack if damaged
  if( BG_InventoryContainsUpgrade( UP_JETPACK, client->ps.stats ) &&
      BG_UpgradeIsActive( UP_JETPACK, client->ps.stats ) )
  {
    if( ent->lastDamageTime + JETPACK_DISABLE_TIME > level.time )
    {
      if( random( ) > JETPACK_DISABLE_CHANCE )
        client->ps.pm_type = PM_NORMAL;
    }
  }

  // set the clip mask and/or the contents of clients that occupied an
  // activation entity
  G_OccupantClip( ent );

  // set up for pmove
  oldEventSequence = client->ps.eventSequence;

  memset( &pm, 0, sizeof( pm ) );

  if( ent->flags & FL_FORCE_GESTURE )
  {
    ent->flags &= ~FL_FORCE_GESTURE;
    ent->client->pers.cmd.buttons |= BUTTON_GESTURE;
  }

  // clear fall velocity before every pmove
  client->pmext.fallVelocity = 0.0f;

  pm.ps = &client->ps;
  pm.pmext = &client->pmext;
  pm.cmd = *ucmd;
  pm.tracemask = ent->clipmask;
  pm.trace = G_TraceWrapper;
  pm.pointcontents = SV_PointContents;
  pm.unlagged_on = G_UnlaggedOn;
  pm.unlagged_off = G_UnlaggedOff;
  pm.debugLevel = g_debugMove.integer;
  pm.noFootsteps = 0;

  pm.pmove_fixed = pmove_fixed.integer | client->pers.pmoveFixed;
  pm.pmove_msec = pmove_msec.integer;

  pm.humanStaminaMode = g_humanStaminaMode.integer;

  pm.playerAccelMode = g_playerAccelMode.integer;

  pm.tauntSpam = 0;

  pm.swapAttacks = client->pers.swapAttacks;
  pm.wallJumperMinFactor = client->pers.wallJumperMinFactor;
  pm.marauderMinJumpFactor = client->pers.marauderMinJumpFactor;

  VectorCopy( client->ps.origin, client->oldOrigin );

  // touch other objects
  pm.ClientImpacts = ClientImpacts;

  for( i = 0; i < PORTAL_NUM; i++ )
    pm.humanPortalCreateTime[ i ] = level.humanPortals.createTime[ i ];

  // moved from after Pmove -- potentially the cause of
  // future triggering bugs
  G_TouchTriggers( ent );

  // For firing lightning bolts early
  BG_CheckBoltImpactTrigger( &pm, G_TraceWrapper,
                             G_UnlaggedOn, G_UnlaggedOff );

  Pmove( &pm );

  G_UnlaggedDetectCollisions( ent );

  // save results of pmove
  if( ent->client->ps.eventSequence != oldEventSequence )
    ent->eventTime = level.time;

  VectorCopy( ent->client->ps.viewangles, ent->r.currentAngles );
  if( g_smoothClients.integer )
    BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps,
                                            &ent->s,
                                            ent->client->ps.commandTime );
  else
    BG_PlayerStateToEntityState( &ent->client->ps, &ent->s );

  switch( client->ps.weapon )
  {
    case WP_ALEVEL0:
      if( !CheckVenomAttack( ent ) )
      {
        client->ps.weaponstate = WEAPON_READY;
      }
      else
      {
        client->ps.generic1 = WPM_PRIMARY;
        G_AddEvent( ent, EV_FIRE_WEAPON, 0 );
      }
      break;

    case WP_ALEVEL1:
    case WP_ALEVEL1_UPG:
      CheckGrabAttack( ent );
      break;

    case WP_ALEVEL3:
    case WP_ALEVEL3_UPG:
      if( !CheckPounceAttack( ent ) )
      {
        client->ps.weaponstate = WEAPON_READY;
      }
      else
      {
        client->ps.generic1 = WPM_SECONDARY;
        G_AddEvent( ent, EV_FIRE_WEAPON2, 0 );
      }
      break;

    case WP_ALEVEL4:
      // If not currently in a trample, reset the trample bookkeeping data
      if( !( client->ps.pm_flags & PMF_CHARGE ) && client->trampleBuildablesHitPos )
      {
        ent->client->trampleBuildablesHitPos = 0;
        memset( ent->client->trampleBuildablesHit, 0, sizeof( ent->client->trampleBuildablesHit ) );
      }
      break;

    case WP_HBUILD:
      CheckCkitRepair( ent );
      break;

    default:
      break;
  }

  SendPendingPredictableEvents( &ent->client->ps );

  if( !( ent->client->ps.eFlags & EF_FIRING ) )
    client->fireHeld = qfalse;    // for grapple
  if( !( ent->client->ps.eFlags & EF_FIRING2 ) )
    client->fire2Held = qfalse;

  // use the snapped origin for linking so it matches client predicted versions
  VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );

  VectorCopy( pm.mins, ent->r.mins );
  VectorCopy( pm.maxs, ent->r.maxs );

  ent->waterlevel = pm.waterlevel;
  ent->watertype = pm.watertype;

  // execute client events
  ClientEvents( ent, oldEventSequence );

  // link entity now, after any personal teleporters have been used
  SV_LinkEntity( ent );

  // NOTE: now copy the exact origin over otherwise clients can be snapped into solid
  VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );
  VectorCopy( ent->client->ps.origin, ent->s.pos.trBase );

  // save results of triggers and client events
  if( ent->client->ps.eventSequence != oldEventSequence )
    ent->eventTime = level.time;

  // Don't think anymore if dead
  if( client->ps.misc[ MISC_HEALTH ] <= 0 )
    return;

  // swap and latch button actions
  client->oldbuttons = client->buttons;
  client->buttons = ucmd->buttons;
  client->latched_buttons |= client->buttons & ~client->oldbuttons;

  // interactions with activation entities
  G_FindActivationEnt( ent );
  if( ( client->buttons & BUTTON_USE_EVOLVE ) && !( client->oldbuttons & BUTTON_USE_EVOLVE ) &&
       client->ps.misc[ MISC_HEALTH ] > 0 )
  {
    gentity_t *actEnt;

    actEnt = ent->occupation.occupied;

    if( client->ps.eFlags & EF_OCCUPYING )
      G_UnoccupyEnt( actEnt, ent, ent, qfalse );
    else if( client->ps.persistant[ PERS_ACT_ENT ] != ENTITYNUM_NONE ) {
      if(client->ps.persistant[ PERS_ACT_ENT ] != ent->activation.usable_map_trigger) {
        G_ActivateEntity( actEnt, ent );
      }
    }
    else if( client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
    {
      if( BG_AlienCanEvolve( client->ps.stats[ STAT_CLASS ],
                             client->pers.credit, g_alienStage.integer,
                             IS_WARMUP, g_cheats.integer,
                             (client->ps.stats[STAT_FLAGS] & SFL_CLASS_FORCED)) )
      {
        //no nearby objects and alien - show class menu
        G_TriggerMenu( ent->client->ps.clientNum, MN_A_INFEST );
      }
      else
      {
        //flash frags
        G_AddEvent( ent, EV_ALIEN_EVOLVE_FAILED, 0 );
      }
    }
  }

  // perform once-a-second actions
  ClientTimerActions( ent, msec );

  if( ent->suicideTime > 0 && ent->suicideTime < level.time )
  {
    // reset any acitvation entities the player might be occupying
    if( client->ps.eFlags & EF_OCCUPYING )
      G_ResetOccupation( ent->occupation.occupied, ent );

    ent->client->ps.misc[ MISC_HEALTH ] = ent->health = 0;
    player_die( ent, ent, ent, 100000, MOD_SUICIDE );

    ent->suicideTime = 0;
  }
}

/*
==================
ClientThink

A new command has arrived from the client
==================
*/
Q_EXPORT void ClientThink( int clientNum )
{
  gentity_t *ent;

  ent = g_entities + clientNum;
  SV_GetUsercmd( clientNum, &ent->client->pers.cmd );

  // mark the time we got info, so we can display the
  // phone jack if they don't get any for a while
  ent->client->lastCmdTime = level.time;

  if( !g_synchronousClients.integer )
    ClientThink_real( ent );
}


void G_RunClient(gentity_t *ent)
{
  if(
    ent->client->pers.fight &&
    ((g_doWarmup.integer) ||
      (level.countdownTime > 0 &&
        level.countdownTime <= ( level.time + 1000)))) {
    G_AddPredictableEvent( ent, EV_FIGHT, 0 );

    ent->client->pers.fight = qfalse;
  }

  if( !g_synchronousClients.integer )
  {
    if( level.time - ent->client->lastCmdTime > 250 )
    {
      SV_GetUsercmd( ent->client->ps.clientNum, &ent->client->pers.cmd );
      ClientThink_real( ent );
    }
     return;
  }


  ent->client->pers.cmd.serverTime = level.time;
  ClientThink_real( ent );
}


/*
==================
SpectatorClientEndFrame

==================
*/
void SpectatorClientEndFrame( gentity_t *ent )
{
  gclient_t *cl;
  int       clientNum;
  int       score, ping;

  //hax to ensure that changes in the misc array are broadcasted
  ent->client->ps.stats[ STAT_FLAGS ] ^= SFL_REFRESH_MISC;

  // if we are doing a chase cam or a remote view, grab the latest info
  if( ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
  {
    clientNum = ent->client->sess.spectatorClient;
    if( clientNum >= 0 && clientNum < level.maxclients )
    {
      cl = &level.clients[ clientNum ];
      if( cl->pers.connected == CON_CONNECTED )
      {
        score = ent->client->ps.persistant[ PERS_SCORE ];
        ping = ent->client->ps.ping;

        // Copy
        ent->client->ps = cl->ps;

        // Restore
        ent->client->ps.persistant[ PERS_SCORE ] = score;
        ent->client->ps.ping = ping;

        ent->client->ps.pm_flags |= PMF_FOLLOW;
        ent->client->ps.persistant[ PERS_STATE ] &= ~PS_QUEUED;
      }
    }
  }
}

/*
==============
ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEndFrame,
while a slow client may have multiple ClientEndFrame between ClientThink.
==============
*/
void ClientEndFrame( gentity_t *ent )
{
  if( ent->client->sess.spectatorState != SPECTATOR_NOT )
  {
    SpectatorClientEndFrame( ent );
    return;
  }

  //
  // If the end of unit layout is displayed, don't give
  // the player any normal movement attributes
  //
  if( level.intermissiontime )
    return;

  // burn from lava, etc
  P_WorldEffects( ent );

  // apply all the damage taken this frame
  P_DamageFeedback( ent );

  // add the EF_CONNECTION flag if we haven't gotten commands recently
  if( level.time - ent->client->lastCmdTime > 1000 )
    ent->s.eFlags |= EF_CONNECTION;
  else
    ent->s.eFlags &= ~EF_CONNECTION;

  // respawn if dead
  if( ent->client->ps.misc[ MISC_HEALTH ] <= 0 && level.time >= ent->client->respawnTime )
    respawn( ent );

  G_SetClientSound( ent );

  // set the latest infor
  if( g_smoothClients.integer )
    BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s,
                                            ent->client->ps.commandTime );
  else
    BG_PlayerStateToEntityState( &ent->client->ps, &ent->s );

  SendPendingPredictableEvents( &ent->client->ps );
}
