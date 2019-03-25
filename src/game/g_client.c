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

// g_client.c -- client functions that don't happen every frame

static vec3_t playerMins = {-15, -15, -24};
static vec3_t playerMaxs = {15, 15, 32};

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for deathmatch games.
The first time a player enters the game, they will be at an 'initial' spot.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_deathmatch( gentity_t *ent )
{
  int   i;

  G_SpawnInt( "nobots", "0", &i);

  if( i )
    ent->flags |= FL_NO_BOTS;

  G_SpawnInt( "nohumans", "0", &i );
  if( i )
    ent->flags |= FL_NO_HUMANS;
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
equivelant to info_player_deathmatch
*/
void SP_info_player_start( gentity_t *ent )
{
  ent->classname = "info_player_deathmatch";
  SP_info_player_deathmatch( ent );
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
*/
void SP_info_player_intermission( gentity_t *ent )
{
}

/*QUAKED info_alien_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
*/
void SP_info_alien_intermission( gentity_t *ent )
{
}

/*QUAKED info_human_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
*/
void SP_info_human_intermission( gentity_t *ent )
{
}

/*
===============
G_AddCreditToClient
===============
*/
void G_AddCreditToClient( gclient_t *client, short credit, qboolean cap )
{
  int capAmount;

  if( !client )
    return;

  if( !credit )
    return;

  if( cap && credit > 0 )
  {
    capAmount = client->pers.teamSelection == TEAM_ALIENS ?
                ALIEN_MAX_CREDITS : HUMAN_MAX_CREDITS;
    if( client->pers.credit < capAmount )
    {
      client->pers.credit += credit;
      if( client->pers.credit > capAmount )
      {
        if( g_overflowFunds.integer )
          G_DonateCredits( client, client->pers.credit - capAmount, qfalse );
        client->pers.credit = capAmount;
      }
    } else if( g_overflowFunds.integer )
      G_DonateCredits( client, credit, qfalse );
  }
  else
    client->pers.credit += credit;

  if( client->pers.credit < 0 )
    client->pers.credit = 0;
  else if(client->pers.credit > SHRT_MAX) {
    client->pers.credit = SHRT_MAX;
  }

  // Copy to ps so the client can access it
  client->ps.persistant[ PERS_CREDIT ] = client->pers.credit;
}


/*
=======================================================================

  G_SelectSpawnPoint

=======================================================================
*/

/*
================
SpotWouldTelefrag

================
*/
qboolean SpotWouldTelefrag( gentity_t *spot )
{
  int       i, num;
  int       touch[ MAX_GENTITIES ];
  gentity_t *hit;
  vec3_t    mins, maxs;

  VectorAdd( spot->r.currentOrigin, playerMins, mins );
  VectorAdd( spot->r.currentOrigin, playerMaxs, maxs );
  num = SV_AreaEntities( mins, maxs, touch, MAX_GENTITIES );

  for( i = 0; i < num; i++ )
  {
    hit = &g_entities[ touch[ i ] ];
    if ( hit->client && hit->health > 0 )
      return qtrue;
  }

  return qfalse;
}


/*
===========
G_SelectRandomFurthestSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
static gentity_t *G_SelectRandomFurthestSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles )
{
  gentity_t *spot;
  vec3_t    delta;
  float     dist;
  float     list_dist[ 64 ];
  gentity_t *list_spot[ 64 ];
  int       numSpots, rnd, i, j;

  numSpots = 0;
  spot = NULL;

  while( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != NULL )
  {
    if( SpotWouldTelefrag( spot ) )
      continue;

    VectorSubtract( spot->r.currentOrigin, avoidPoint, delta );
    dist = VectorLength( delta );

    for( i = 0; i < numSpots; i++ )
    {
      if( dist > list_dist[ i ] )
      {
        if( numSpots >= 64 )
          numSpots = 64 - 1;

        for( j = numSpots; j > i; j-- )
        {
          list_dist[ j ] = list_dist[ j - 1 ];
          list_spot[ j ] = list_spot[ j - 1 ];
        }

        list_dist[ i ] = dist;
        list_spot[ i ] = spot;
        numSpots++;

        if( numSpots > 64 )
          numSpots = 64;

        break;
      }
    }

    if( i >= numSpots && numSpots < 64 )
    {
      list_dist[ numSpots ] = dist;
      list_spot[ numSpots ] = spot;
      numSpots++;
    }
  }

  if( !numSpots )
  {
    spot = G_Find( NULL, FOFS( classname ), "info_player_deathmatch" );

    if( !spot )
      Com_Error( ERR_DROP, "Couldn't find a spawn point" );

    VectorCopy( spot->r.currentOrigin, origin );
    origin[ 2 ] += 9;
    VectorCopy( spot->r.currentAngles, angles );
    return spot;
  }

  // select a random spot from the spawn points furthest away
  rnd = random( ) * ( numSpots / 2 );

  VectorCopy( list_spot[ rnd ]->r.currentOrigin, origin );
  origin[ 2 ] += 9;
  VectorCopy( list_spot[ rnd ]->r.currentAngles, angles );

  return list_spot[ rnd ];
}


/*
================
G_SelectSpawnBuildable

find the nearest buildable of the right type that is
spawned/healthy/unblocked etc.
================
*/
static gentity_t *G_SelectSpawnBuildable( vec3_t preference, buildable_t buildable )
{
  gentity_t *search, *spot, *tempBlocker, *blocker;

  search = spot = blocker = NULL;

  while( ( search = G_Find( search, FOFS( classname ),
    BG_Buildable( buildable )->entityName ) ) != NULL )
  {
    if( !search->spawned )
      continue;

    if( search->health <= 0 )
      continue;

    if( search->s.groundEntityNum == ENTITYNUM_NONE )
      continue;

    if( search->clientSpawnTime > 0 && !IS_WARMUP )
      continue;

    if( ( tempBlocker = G_CheckSpawnPoint( search->s.number, search->r.currentOrigin,
          search->s.origin2, buildable, NULL ) ) != NULL && !tempBlocker->client )
      continue;

    if( !spot || DistanceSquared( preference, search->r.currentOrigin ) <
                 DistanceSquared( preference, spot->r.currentOrigin ) )
    {
      spot = search;
      blocker = tempBlocker;
    }
  }

  if( blocker && blocker->client )
  {
    spot->attemptSpawnTime = level.time + 1000;
    spot = NULL;
  } else if( spot )
    spot->attemptSpawnTime = -1;

  return spot;
}

/*
===========
G_SelectSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t *G_SelectSpawnPoint( vec3_t avoidPoint, vec3_t origin, vec3_t angles )
{
  return G_SelectRandomFurthestSpawnPoint( avoidPoint, origin, angles );
}


/*
===========
G_SelectTremulousSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t *G_SelectTremulousSpawnPoint( team_t team, vec3_t preference, vec3_t origin, vec3_t angles )
{
  gentity_t *spot = NULL;

  if( team == TEAM_ALIENS )
  {
    if( level.numAlienSpawns <= 0 )
      return NULL;

    spot = G_SelectSpawnBuildable( preference, BA_A_SPAWN );
  }
  else if( team == TEAM_HUMANS )
  {
    if( level.numHumanSpawns <= 0 )
      return NULL;

    spot = G_SelectSpawnBuildable( preference, BA_H_SPAWN );
  }

  //no available spots
  if( !spot )
    return NULL;

  if( team == TEAM_ALIENS )
    G_CheckSpawnPoint( spot->s.number, spot->r.currentOrigin, spot->s.origin2, BA_A_SPAWN, origin );
  else if( team == TEAM_HUMANS )
    G_CheckSpawnPoint( spot->s.number, spot->r.currentOrigin, spot->s.origin2, BA_H_SPAWN, origin );

  VectorCopy( spot->r.currentAngles, angles );
  angles[ ROLL ] = 0;

  return spot;

}


/*
===========
G_SelectSpectatorSpawnPoint

============
*/
static gentity_t *G_SelectSpectatorSpawnPoint( vec3_t origin, vec3_t angles )
{
  FindIntermissionPoint( );

  VectorCopy( level.intermission_origin, origin );
  VectorCopy( level.intermission_angle, angles );

  return NULL;
}


/*
===========
G_SelectAlienLockSpawnPoint

Try to find a spawn point for alien intermission otherwise
use normal intermission spawn.
============
*/
gentity_t *G_SelectAlienLockSpawnPoint( vec3_t origin, vec3_t angles )
{
  gentity_t *spot;

  spot = NULL;
  spot = G_Find( spot, FOFS( classname ), "info_alien_intermission" );

  if( !spot )
    return G_SelectSpectatorSpawnPoint( origin, angles );

  VectorCopy( spot->r.currentOrigin, origin );
  VectorCopy( spot->r.currentAngles, angles );

  return spot;
}


/*
===========
G_SelectHumanLockSpawnPoint

Try to find a spawn point for human intermission otherwise
use normal intermission spawn.
============
*/
gentity_t *G_SelectHumanLockSpawnPoint( vec3_t origin, vec3_t angles )
{
  gentity_t *spot;

  spot = NULL;
  spot = G_Find( spot, FOFS( classname ), "info_human_intermission" );

  if( !spot )
    return G_SelectSpectatorSpawnPoint( origin, angles );

  VectorCopy( spot->r.currentOrigin, origin );
  VectorCopy( spot->r.currentAngles, angles );

  return spot;
}


/*
=======================================================================

BODYQUE

=======================================================================
*/


/*
=============
BodySink

After sitting around for five seconds, fall into the ground and dissapear
=============
*/
static void BodySink( gentity_t *ent )
{
  //run on first BodySink call
  if( !ent->active )
  {
    ent->active = qtrue;

    //sinking bodies can't be infested
    ent->killedBy = ent->s.misc = MAX_CLIENTS;
    ent->timestamp = level.time;
  }

  if( level.time - ent->timestamp > 6500 )
  {
    G_FreeEntity( ent );
    return;
  }

  ent->nextthink = level.time + 100;
  ent->s.pos.trBase[ 2 ] -= 1;
}


/*
=============
SpawnCorpse

A player is respawning, so make an entity that looks
just like the existing corpse to leave behind.
=============
*/
static void SpawnCorpse( gentity_t *ent )
{
  gentity_t   *body, *tent;
  int         contents;
  vec3_t      origin, mins;

  VectorCopy( ent->r.currentOrigin, origin );

  SV_UnlinkEntity( ent );

  // if client is in a nodrop area, don't leave the body
  contents = SV_PointContents( origin, -1 );
  if( contents & CONTENTS_NODROP )
    return;

  body = G_Spawn( );

  VectorCopy( ent->s.apos.trBase, body->s.apos.trBase );
  VectorCopy( ent->s.apos.trBase, body->r.currentAngles );
  body->s.eFlags = EF_DEAD;
  body->s.otherEntityNum2 = ent->client->ps.stats[ STAT_FLAGS ] & SFL_GIBBED;
  body->s.eType = ET_CORPSE;
  body->timestamp = level.time;
  body->s.event = 0;
  if( ent->client && ent->client->noclip )
    G_SetContents( body, ent->client->cliprcontents );
  else
    G_SetContents( body, ent->r.contents );
  G_SetClipmask( body, MASK_DEADSOLID );
  body->s.clientNum = ent->client->ps.stats[ STAT_CLASS ];
  body->nonSegModel = ent->client->ps.persistant[ PERS_STATE ] & PS_NONSEGMODEL;

  // FIXIT-P: Looks like dead code
  if( ent->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
    body->classname = "humanCorpse";
  else
    body->classname = "alienCorpse";

  body->s.misc     = MAX_CLIENTS; // FIXIT-P: This doesn't seemto have any use.

  body->think      = BodySink;    // Ok, so body sinks
  body->nextthink  = level.time + 20000; // after 20seconds

  G_ChangeHealth( body, ent, ent->health,
                  (HLTHF_SET_TO_CHANGE|
                   HLTHF_EVOLVE_INCREASE|
                   HLTHF_NO_DECAY|
                   HLTHF_IGNORE_ENEMY_DMG) );

  body->takedamage = ent->takedamage;
  body->die = ent->die;

  body->s.legsAnim = ent->s.legsAnim;

  if( !body->nonSegModel )
  {
    switch( body->s.legsAnim & ~ANIM_TOGGLEBIT )
    {
      case BOTH_DEATH1:
      case BOTH_DEAD1:
        body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD1;
        break;

      case BOTH_DEATH2:
      case BOTH_DEAD2:
        body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD2;
        break;

      case BOTH_DEATH3:
      case BOTH_DEAD3:
      default:
        body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD3;
        break;
    }
  }
  else
  {
    switch( body->s.legsAnim & ~ANIM_TOGGLEBIT )
    {
      case NSPA_DEATH1:
      case NSPA_DEAD1:
        body->s.legsAnim = NSPA_DEAD1;
        break;

      case NSPA_DEATH2:
      case NSPA_DEAD2:
        body->s.legsAnim = NSPA_DEAD2;
        break;

      case NSPA_DEATH3:
      case NSPA_DEAD3:
      default:
        body->s.legsAnim = NSPA_DEAD3;
        break;
    }
  }

  //change body dimensions
  BG_ClassBoundingBox( ent->client->ps.stats[ STAT_CLASS ], mins, NULL, NULL,
          body->r.mins, body->r.maxs );

  //drop down to match the *model* origins of ent and body
  origin[2] += mins[ 2 ] - body->r.mins[ 2 ];

  G_SetOrigin( body, origin );

  body->s.pos.trType = TR_GRAVITY;
  body->s.pos.trTime = level.time;
  VectorCopy( ent->client->ps.velocity, body->s.pos.trDelta );

  // special case for the spitfire's wings to fly off
  if( body->s.clientNum == PCL_ALIEN_SPITFIRE )
  {
    vec3_t up;
    AngleVectors( body->r.currentAngles, NULL, NULL, up);
    tent = G_TempEntity( body->r.currentOrigin, EV_GIB_SPITFIRE_WINGS );
    VectorCopy( up, tent->s.origin2 );
  }

  SV_LinkEntity( body );
}

//======================================================================


/*
==================
G_SetClientViewAngle

==================
*/
void G_SetClientViewAngle( gentity_t *ent, const vec3_t angle )
{
  int     i;

  // set the delta angle
  for( i = 0; i < 3; i++ )
  {
    int   cmdAngle;

    cmdAngle = ANGLE2SHORT( angle[ i ] );
    ent->client->ps.delta_angles[ i ] = cmdAngle - ent->client->pers.cmd.angles[ i ];
  }

  VectorCopy( angle, ent->s.apos.trBase );
  VectorCopy( angle, ent->r.currentAngles );
  VectorCopy( angle, ent->client->ps.viewangles );
}

/*
================
respawn
================
*/
void respawn( gentity_t *ent )
{
  int i;

  if( ent->client->pmext.explosionMod <= 0.000f )
    SpawnCorpse( ent );
  else
    ent->client->pmext.explosionMod = 0.000f;

  // Clients can't respawn - they must go through the class cmd
  ent->client->pers.classSelection = PCL_NONE;
  ClientSpawn( ent, NULL, NULL, NULL, qfalse );

  // stop any following clients that don't have sticky spec on
  for( i = 0; i < level.maxclients; i++ )
  {
    if( level.clients[ i ].sess.spectatorState == SPECTATOR_FOLLOW &&
        level.clients[ i ].sess.spectatorClient == ent - g_entities )
    {
      if( !( level.clients[ i ].pers.stickySpec ) )
      {
        if( !G_FollowNewClient( &g_entities[ i ], 1 ) )
          G_StopFollowing( &g_entities[ i ] );
      }
      else
        G_FollowLockView( &g_entities[ i ] );
    }
  }
}

static qboolean G_IsEmoticon( const char *s, qboolean *escaped )
{
  int i, j;
  const char *p = s;
  char emoticon[ MAX_EMOTICON_NAME_LEN ] = {""};
  qboolean escape = qfalse;

  if( *p != '[' )
    return qfalse;
  p++;
  if( *p == '[' )
  {
    escape = qtrue;
    p++;
  }
  i = 0;
  while( *p && i < ( MAX_EMOTICON_NAME_LEN - 1 ) )
  {
    if( *p == ']' )
    {
      for( j = 0; j < level.emoticonCount; j++ )
      {
        if( !Q_stricmp( emoticon, level.emoticons[ j ].name ) )
        {
          *escaped = escape;
          return qtrue;
        }
      }
      return qfalse;
    }
    emoticon[ i++ ] = *p;
    emoticon[ i ] = '\0';
    p++;
  }
  return qfalse;
}

/*
===========
G_IsNewbieName
============
*/
qboolean G_IsNewbieName( const char *name )
{
  char testName[ MAX_NAME_LENGTH ];

  G_DecolorString( (char *)name, testName, sizeof( testName ) );

  if( !Q_stricmp( testName, "UnnamedPlayer" ) )
    return qtrue;

  if( g_newbieNameNumbering.integer &&
      g_newbieNamePrefix.string[ 0 ] &&
      !Q_stricmpn( testName, g_newbieNamePrefix.string, strlen( g_newbieNamePrefix.string ) ) )
    return qtrue;

  return qfalse;
}

/*
===========
G_ClientNewbieName
============
*/
static const char *G_ClientNewbieName( gclient_t *client )
{
  static int  nextNumber = 1;
  static char name[ MAX_NAME_LENGTH ];
  int         number;

  if( !g_newbieNameNumbering.integer ||
      !g_newbieNamePrefix.string[ 0 ] ||
      !client )
   return "UnnamedPlayer";

  if( client->pers.namelog->newbieNumber )
  {
    number = client->pers.namelog->newbieNumber;
  }
  else if( g_newbieNameNumbering.integer > 1 )
  {
    number = g_newbieNameNumbering.integer;
    Cvar_SetSafe( "g_newbieNameNumbering", va( "%d", number + 1 ) );
  }
  else
  {
    number = nextNumber++;
  }

  client->pers.namelog->newbieNumber = number;
  Com_sprintf( name, sizeof( name ), "%s%d",
               g_newbieNamePrefix.string, number );
  return name;
}

/*
===========
G_ClientCleanName
============
*/
static void G_ClientCleanName( const char *in, char *out, int outSize,
                               gclient_t *client )
{
  int   len, colorlessLen;
  char  *p;
  int   spaces;
  qboolean escaped;
  qboolean invalid = qfalse;

  //save room for trailing null byte
  outSize--;

  len = 0;
  colorlessLen = 0;
  p = out;
  *p = 0;
  spaces = 0;

  for( ; *in; in++ )
  {
    // don't allow leading spaces
    if( colorlessLen == 0 && *in == ' ' )
      continue;

    // don't allow nonprinting characters or (dead) console keys
    if( *in < ' ' || *in > '}' || *in == '`' )
      continue;

    // check colors
    if( Q_IsColorString( in ) )
    {
      in++;

      // make sure room in dest for both chars
      if( len > outSize - 2 )
        break;

      *out++ = Q_COLOR_ESCAPE;

      *out++ = *in;

      len += 2;
      continue;
    }
    else if( !g_emoticonsAllowedInNames.integer && G_IsEmoticon( in, &escaped ) )
    {
      // make sure room in dest for both chars
      if( len > outSize - 2 )
        break;

      *out++ = '[';
      *out++ = '[';
      len += 2;
      if( escaped )
        in++;
      continue;
    }

    // don't allow too many consecutive spaces
    if( *in == ' ' )
    {
      spaces++;
      if( spaces > 3 )
        continue;
    }
    else
      spaces = 0;

    if( len > outSize - 1 )
      break;

    *out++ = *in;
    colorlessLen++;
    len++;
  }

  *out = 0;

  // don't allow names beginning with "[skipnotify]" because it messes up /ignore-related code
  if( !Q_stricmpn( p, "[skipnotify]", 12 ) )
    invalid = qtrue;

  // don't allow comment-beginning strings because it messes up various parsers
  if( strstr( p, "//" ) || strstr( p, "/*" ) )
    invalid = qtrue;

  // don't allow empty names
  if( *p == 0 || colorlessLen == 0 )
    invalid = qtrue;

  // if something made the name bad, put them back to UnnamedPlayer
  if( invalid )
    Q_strncpyz( p, G_ClientNewbieName( client ), outSize );
}


/*
======================
G_NonSegModel

Reads an animation.cfg to check for nonsegmentation
======================
*/
static qboolean G_NonSegModel( const char *filename )
{
  char          *text_p;
  int           len;
  char          *token;
  char          text[ 20000 ];
  fileHandle_t  f;

  // load the file
  len = FS_FOpenFileByMode( filename, &f, FS_READ );
  if( !f )
  {
    Com_Printf( "File not found: %s\n", filename );
    return qfalse;
  }

  if( len < 0 )
    return qfalse;

  if( len == 0 || len >= sizeof( text ) - 1 )
  {
    FS_FCloseFile( f );
    Com_Printf( "File %s is %s\n", filename, len == 0 ? "empty" : "too long" );
    return qfalse;
  }

  FS_Read2( text, len, f );
  text[ len ] = 0;
  FS_FCloseFile( f );

  // parse the text
  text_p = text;

  // read optional parameters
  while( 1 )
  {
    token = COM_Parse( &text_p );

    //EOF
    if( !token[ 0 ] )
      break;

    if( !Q_stricmp( token, "nonsegmented" ) )
      return qtrue;
  }

  return qfalse;
}

/*
===========
ClientUserInfoChanged

Called from ClientConnect when the player first connects and
directly by the server system when the player updates a userinfo variable.

The game can override any of the settings and call SV_SetUserinfo
if desired.
============
*/
Q_EXPORT char *ClientUserinfoChanged( int clientNum, qboolean forceName )
{
  gentity_t *ent;
  int       value;
  char      *s, *s2;
  char      model[ MAX_QPATH ];
  char      buffer[ MAX_QPATH ];
  char      filename[ MAX_QPATH ];
  char      oldname[ MAX_NAME_LENGTH ];
  char      newname[ MAX_NAME_LENGTH ];
  char      err[ MAX_STRING_CHARS ];
  qboolean  revertName = qfalse;
  gclient_t *client;
  char      userinfo[ MAX_INFO_STRING ];

  ent = g_entities + clientNum;
  client = ent->client;

  SV_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

  // check for malformed or illegal info strings
  if( !Info_Validate(userinfo) )
  {
    SV_GameSendServerCommand( ent - g_entities,
        "disconnect \"illegal or malformed userinfo\n\"" );
    SV_GameDropClient( ent - g_entities,
        "dropped: illegal or malformed userinfo");
    return "Illegal or malformed userinfo";
  }
  // If their userinfo overflowed, tremded is in the process of disconnecting them.
  // If we send our own disconnect, it won't work, so just return to prevent crashes later
  //  in this function. This check must come after the Info_Validate call.
  else if( !userinfo[ 0 ] )
    return "Empty (overflowed) userinfo";

  // stickyspec toggle
  s = Info_ValueForKey( userinfo, "cg_stickySpec" );
  client->pers.stickySpec = atoi( s ) != 0;

  // set name
  Q_strncpyz( oldname, client->pers.netname, sizeof( oldname ) );
  s = Info_ValueForKey( userinfo, "name" );
  G_ClientCleanName( s, newname, sizeof( newname ), client );

  if( strcmp( oldname, newname ) )
  {
    if( !forceName && client->pers.namelog->nameChangeTime &&
      level.time - client->pers.namelog->nameChangeTime <=
      g_minNameChangePeriod.value * 1000 )
    {
      SV_GameSendServerCommand( ent - g_entities, va(
        "print \"Name change spam protection (g_minNameChangePeriod = %d)\n\"",
         g_minNameChangePeriod.integer ) );
      revertName = qtrue;
    }
    else if( !forceName && g_maxNameChanges.integer > 0 &&
      client->pers.namelog->nameChanges >= g_maxNameChanges.integer  )
    {
      SV_GameSendServerCommand( ent - g_entities, va(
        "print \"Maximum name changes reached (g_maxNameChanges = %d)\n\"",
         g_maxNameChanges.integer ) );
      revertName = qtrue;
    }
    else if( !forceName && client->pers.namelog->muted )
    {
      SV_GameSendServerCommand( ent - g_entities,
        "print \"You cannot change your name while you are muted\n\"" );
      revertName = qtrue;
    }
    else if( !G_admin_name_check( ent, newname, err, sizeof( err ) ) )
    {
      SV_GameSendServerCommand( ent - g_entities, va( "print \"%s\n\"", err ) );
      revertName = qtrue;
    }

    if( revertName )
    {
      Q_strncpyz( client->pers.netname, *oldname ? oldname : G_ClientNewbieName( client ),
        sizeof( client->pers.netname ) );
      Info_SetValueForKey( userinfo, "name", oldname );
      SV_SetUserinfo( clientNum, userinfo );
    }
    else
    {
      if( G_IsNewbieName( newname ) )
        Q_strncpyz( newname, G_ClientNewbieName( client ), sizeof( newname ) );

      G_CensorString( client->pers.netname, newname,
        sizeof( client->pers.netname ), ent );
      if( !forceName && client->pers.connected == CON_CONNECTED )
      {
        client->pers.namelog->nameChangeTime = level.time;
        client->pers.namelog->nameChanges++;
      }
      if( *oldname )
      {
        G_LogPrintf( "ClientRename: %i [%s] (%s) \"%s^7\" -> \"%s^7\" \"%c%s%c^7\"\n",
                   clientNum, client->pers.ip.str, client->pers.guid,
                   oldname, client->pers.netname,
                   DECOLOR_OFF, client->pers.netname, DECOLOR_ON );
      }
    }

    if(IS_SCRIM) {
      G_Scrim_Player_Netname_Updated(client);
    }

    G_namelog_update_name( client );
  }

  s = NULL;
  if ( client->pers.teamSelection == TEAM_HUMANS )
  {
    qboolean found;

    s = Info_ValueForKey(userinfo, "model");
    found = G_IsValidPlayerModel(s);

    if ( !found )
      s = NULL;

    if (s)
    {
      s2 = Info_ValueForKey(userinfo, "skin");
      s2 = GetSkin(s, s2);
    }
  }

  if( client->pers.classSelection == PCL_NONE )
  {
    //This looks hacky and frankly it is. The clientInfo string needs to hold different
    //model details to that of the spawning class or the info change will not be
    //registered and an axis appears instead of the player model. There is zero chance
    //the player can spawn with the battlesuit, hence this choice.
    Com_sprintf( buffer, MAX_QPATH, "%s/%s",  BG_ClassConfig( PCL_HUMAN_BSUIT )->modelName,
                                              BG_ClassConfig( PCL_HUMAN_BSUIT )->skinName );
  }
  else
  {
    if ( !(client->pers.classSelection == PCL_HUMAN_BSUIT) && s )
    {
        Com_sprintf( buffer, MAX_QPATH, "%s/%s", s, s2 );
    }
    else
    {
        Com_sprintf( buffer, MAX_QPATH, "%s/%s",  BG_ClassConfig( client->pers.classSelection )->modelName,
                                                  BG_ClassConfig( client->pers.classSelection )->skinName );
    }

    //model segmentation
    Com_sprintf( filename, sizeof( filename ), "models/players/%s/animation.cfg",
                 BG_ClassConfig( client->pers.classSelection )->modelName );

    if( G_NonSegModel( filename ) )
      client->ps.persistant[ PERS_STATE ] |= PS_NONSEGMODEL;
    else
      client->ps.persistant[ PERS_STATE ] &= ~PS_NONSEGMODEL;
  }
  Q_strncpyz( model, buffer, sizeof( model ) );

  // wallwalk follow
  s = Info_ValueForKey( userinfo, "cg_wwFollow" );

  if( atoi( s ) )
    client->ps.persistant[ PERS_STATE ] |= PS_WALLCLIMBINGFOLLOW;
  else
    client->ps.persistant[ PERS_STATE ] &= ~PS_WALLCLIMBINGFOLLOW;

  // wallwalk toggle
  s = Info_ValueForKey( userinfo, "cg_wwToggle" );

  if( atoi( s ) )
    client->ps.persistant[ PERS_STATE ] |= PS_WALLCLIMBINGTOGGLE;
  else
    client->ps.persistant[ PERS_STATE ] &= ~PS_WALLCLIMBINGTOGGLE;

  // always sprint
  s = Info_ValueForKey(userinfo, "cg_sprintToggle");
  value = atoi(s);

  if(value) {
    client->ps.persistant[ PERS_STATE ] |= PS_SPRINTTOGGLE;
    if(value == 2) {
      client->ps.persistant[ PERS_STATE ] |= PS_SPRINTTOGGLEONSTOP;
    } else {
      client->ps.persistant[ PERS_STATE ] &= ~PS_SPRINTTOGGLEONSTOP;
    }
  } else {
    client->ps.persistant[ PERS_STATE ] &= ~PS_SPRINTTOGGLE;
    client->ps.persistant[ PERS_STATE ] &= ~PS_SPRINTTOGGLEONSTOP;
  }

  // fly speed
  s = Info_ValueForKey( userinfo, "cg_flySpeed" );

  if( *s )
    client->pers.flySpeed = atoi( s );
  else
    client->pers.flySpeed = BG_Class( PCL_NONE )->speed;

  client->pers.buildableRangeMarkerMask =
    atoi( Info_ValueForKey( userinfo, "cg_buildableRangeMarkerMask" ) );

  // swap attacks
  client->pers.swapAttacks = atoi( Info_ValueForKey( userinfo, "cg_swapAttacks" ) );

  // wall jumper min factor
  client->pers.wallJumperMinFactor = atof( Info_ValueForKey( userinfo, "cg_wallJumperMinFactor" ) );
  if( client->pers.wallJumperMinFactor > 1.0f ) {
    client->pers.wallJumperMinFactor = 1.0f;
  }
  else if( client->pers.wallJumperMinFactor < 0.0f ) {
    client->pers.wallJumperMinFactor = 0.0f;
  }

  //marauder min jump factor
  client->pers.marauderMinJumpFactor = atof( Info_ValueForKey( userinfo, "cg_marauderMinJumpFactor" ) );
  if( client->pers.marauderMinJumpFactor > 1.0f ) {
    client->pers.marauderMinJumpFactor = 1.0f;
  }
  else if( client->pers.marauderMinJumpFactor < 0.0f ) {
    client->pers.marauderMinJumpFactor = 0.0f;
  }

  // teamInfo
  s = Info_ValueForKey( userinfo, "teamoverlay" );

  if( atoi( s ) != 0 )
  {
    // teamoverlay was enabled so we need an update
    if( client->pers.teamInfo == 0 )
      client->pers.teamInfo = 1;
  }
  else
    client->pers.teamInfo = 0;

  s = Info_ValueForKey( userinfo, "cg_unlagged" );
  if( !s[0] || atoi( s ) != 0 )
    client->pers.useUnlagged = qtrue;
  else
    client->pers.useUnlagged = qfalse;

  Q_strncpyz( client->pers.voice, Info_ValueForKey( userinfo, "voice" ),
    sizeof( client->pers.voice ) );

  // send over a subset of the userinfo keys so other clients can
  // print scoreboards, display models, and play custom sounds

  Com_sprintf( userinfo, sizeof( userinfo ),
    "n\\%s\\t\\%i\\rank\\%i\\model\\%s\\ig\\%16s\\v\\%s\\restart\\%i",
    client->pers.netname, client->pers.teamSelection, client->sess.rank, model,
    Com_ClientListString( &client->sess.ignoreList ),
    client->pers.voice, g_restartingFlags.integer );

  SV_SetConfigstring( CS_PLAYERS + clientNum, userinfo );

  /*G_LogPrintf( "ClientUserinfoChanged: %i %s\n", clientNum, userinfo );*/

  return NULL;
}


/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournement restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournement
restarts.
============
*/
Q_EXPORT char *ClientConnect( int clientNum, qboolean firstTime )
{
  char      *value;
  char      *userInfoError;
  gclient_t *client;
  char      userinfo[ MAX_INFO_STRING ];
  gentity_t *ent;
  char      reason[ MAX_STRING_CHARS ] = {""};
  int       i;

  ent = &g_entities[ clientNum ];
  client = &level.clients[ clientNum ];

  // ignore if client already connected
  if( client->pers.connected != CON_DISCONNECTED )
    return NULL;

  ent->client = client;
  memset( client, 0, sizeof( *client ) );

  SV_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

  value = Info_ValueForKey( userinfo, "cl_guid" );
  Q_strncpyz( client->pers.guid, value, sizeof( client->pers.guid ) );

  value = Info_ValueForKey( userinfo, "ip" );
  // check for local client
  if( !strcmp( value, "localhost" ) )
    client->pers.localClient = qtrue;
  G_AddressParse( value, &client->pers.ip );

  client->pers.admin = G_admin_admin( client->pers.guid );

  client->pers.alternateProtocol = Cvar_VariableIntegerValue( va( "sv_clAltProto%i", clientNum ) );

  if( client->pers.alternateProtocol == 2 && client->pers.guid[ 0 ] == '\0' )
  {
    size_t len = strlen( client->pers.ip.str );
    if( len == 0 )
      len = 1;
    for( i = 0; i < sizeof( client->pers.guid ) - 1; ++i )
    {
      int j = client->pers.ip.str[ i % len ] + rand() / ( RAND_MAX / 16 + 1 );
      client->pers.guid[ i ] = "0123456789ABCDEF"[ j % 16 ];
    }
    client->pers.guid[ sizeof( client->pers.guid ) - 1 ] = '\0';
    client->pers.guidless = qtrue;
  }

  // check for admin ban
  if( G_admin_ban_check( ent, reason, sizeof( reason ) ) )
  {
    return va( "%s", reason );
  }

  // check for a password
  value = Info_ValueForKey( userinfo, "password" );

  if( g_password.string[ 0 ] && Q_stricmp( g_password.string, "none" ) &&
      strcmp( g_password.string, value ) != 0 )
    return "Invalid password";

  // add guid to session so we don't have to keep parsing userinfo everywhere
  for( i = 0; i < sizeof( client->pers.guid ) - 1 &&
              isxdigit( client->pers.guid[ i ] ); i++ );

  if( i < sizeof( client->pers.guid ) - 1 )
    return "Invalid GUID";

  // do autoghost now so that there won't be any name conflicts later on
  if(g_autoGhost.integer && client->pers.guidless) {
    for(i = 0; i < MAX_CLIENTS; i++) {
      if(
        i != ent - g_entities && g_entities[i].client &&
        g_entities[i].client->pers.connected != CON_DISCONNECTED &&
        !g_entities[i].client->pers.guidless &&
        !g_entities[i].client->pers.localClient &&
        !Q_stricmp( g_entities[i].client->pers.guid, client->pers.guid ) )
      {
        SV_GameSendServerCommand( i, "disconnect \"You may not be connected to this server multiple times\"" );
        SV_GameDropClient( i, "disconnected" );
      }
    }
  }

  client->pers.connected = CON_CONNECTING;

  // read or initialize the session data
  if( firstTime || level.newSession )
    G_InitSessionData( client, userinfo );

  G_ReadSessionData( client );

  // get and distribute relevent paramters
  G_namelog_connect( client );
  userInfoError = ClientUserinfoChanged( clientNum, qfalse );
  if( userInfoError != NULL )
    return userInfoError;

  G_LogPrintf( "ClientConnect: %i [%s] (%s) \"%s^7\" \"%c%s%c^7\"\n",
               clientNum, client->pers.ip.str, client->pers.guid,
               client->pers.netname,
               DECOLOR_OFF, client->pers.netname, DECOLOR_ON );

  // don't do the "xxx connected" messages if they were caried over from previous level
  if( firstTime )
    SV_GameSendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " connected\n\"",
                                    client->pers.netname ) );

  if( client->pers.admin )
    G_admin_authlog( ent );

  // count current clients and rank for scoreboard
  CalculateRanks(qtrue);

  if(IS_SCRIM) {
    int roster_id;
    scrim_team_t scrim_team = G_Scrim_Find_Player_In_Rosters(client, &roster_id);

    if(scrim_team != SCRIM_TEAM_NONE) {
      team_t playing_team = level.scrim.team[scrim_team].current_team;

      if(playing_team != TEAM_NONE) {
        G_ChangeTeam(ent, playing_team);
        client->sess.restartTeam = TEAM_NONE;
        if(!IS_WARMUP) {
          client->sess.readyToPlay = qfalse;
        }
      } else {
        // Spectators aren't suppose to have ready states
        client->sess.readyToPlay = qfalse;
      }
    } else {
      // Spectators aren't suppose to have ready states
      client->sess.readyToPlay = qfalse;
    }
  } else {
    // if this is after !restart keepteams or !restart switchteams, apply said selection
    if(client->sess.restartTeam != TEAM_NONE) {
      G_ChangeTeam(ent, client->sess.restartTeam);
      client->sess.restartTeam = TEAM_NONE;
    } else {
      // Spectators aren't suppose to have ready states
      client->sess.readyToPlay = qfalse;
    }
  }
  

  client->pers.firstConnection = firstTime;

  return NULL;
}

/*
===========
ClientBegin

Called when a client has finished connecting, and is ready
to be placed into the level. This will happen on every
level load and level restart, but doesn't happen on respawns.
============
*/
Q_EXPORT void ClientBegin( int clientNum )
{
  gentity_t *ent;
  gclient_t *client;
  int       flags;

  ent = g_entities + clientNum;

  client = level.clients + clientNum;

  // ignore if client already entered the game
  if( client->pers.connected != CON_CONNECTING )
    return;

  if( ent->r.linked ) {
    SV_UnlinkEntity( ent );
  }

  G_InitGentity( ent );
  ent->touch = 0;
  ent->pain = 0;
  ent->client = client;

  client->pers.connected = CON_CONNECTED;
  client->pers.enterTime = level.time;

  // save eflags around this, because changing teams will
  // cause this to happen with a valid entity, and we
  // want to make sure the teleport bit is set right
  // so the viewpoint doesn't interpolate through the
  // world to the new position
  flags = client->ps.eFlags;
  memset( &client->ps, 0, sizeof( client->ps ) );
  memset( &client->pmext, 0, sizeof( client->pmext ) );
  client->ps.eFlags = flags;

  // communicate the client's ready status
  if( client->sess.readyToPlay ) {
    client->ps.stats[ STAT_FLAGS ] |= SFL_READY;
  } else {
    client->ps.stats[ STAT_FLAGS ] &= ~SFL_READY;
  }

  // locate ent at a spawn point
  ClientSpawn( ent, NULL, NULL, NULL, qtrue );

  if( !( g_restartingFlags.integer &
         ( RESTART_WARMUP_RESET | RESTART_WARMUP_END | RESTART_SCRIM ) ) ) {
   SV_GameSendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " entered the game\n\"", client->pers.netname ) );
 }
    
  if( client->pers.firstConnection )
  {
    gentity_t *tent;

    tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_IN );
    tent->r.svFlags = SVF_BROADCAST; // send to everyone

    if(IS_SCRIM) {
      G_admin_scrim_status(ent);
    }
  }

  G_namelog_restore( client );

  G_LogPrintf( "ClientBegin: %i\n", clientNum );

  // count current clients and rank for scoreboard
  CalculateRanks(qtrue);

  // Update player ready states if in warmup
  if( IS_WARMUP )
    G_LevelReady();

  // send the client a list of commands that can be used
  G_ListCommands( ent );

  // Send playmap pool
  PlayMapPoolMessage( clientNum );
}

/*
===========
ClientSpawn

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState
============
*/
void ClientSpawn( gentity_t *ent, gentity_t *spawn, const vec3_t origin, const vec3_t angles, qboolean check_exit_rules )
{
  int                 index;
  vec3_t              spawn_origin, spawn_angles;
  gclient_t           *client;
  int                 i;
  clientPersistant_t  saved;
  clientSession_t     savedSess;
  qboolean            savedNoclip, savedCliprcontents;
  int                 persistant[ MAX_PERSISTANT ];
  int                 miscMisc2;
  int                 flags;
  int                 savedPing;
  int                 teamLocal;
  int                 eventSequence;
  char                userinfo[ MAX_INFO_STRING ];
  vec3_t              up = { 0.0f, 0.0f, 1.0f };
  int                 maxAmmo, maxClips;
  weapon_t            weapon;

  index = ent - g_entities;
  client = ent->client;

  teamLocal = client->pers.teamSelection;

  //if client is dead and following teammate, stop following before spawning
  if( client->sess.spectatorClient != -1 )
  {
    client->sess.spectatorClient = -1;
    client->sess.spectatorState = SPECTATOR_FREE;
  }

  // only start client if chosen a class and joined a team
  if( client->pers.classSelection == PCL_NONE && teamLocal == TEAM_NONE )
    client->sess.spectatorState = SPECTATOR_FREE;
  else if( client->pers.classSelection == PCL_NONE )
    client->sess.spectatorState = SPECTATOR_LOCKED;

  // if client is dead and following teammate, stop following before spawning
  if( ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
    G_StopFollowing( ent );

  // find a spawn point
  // do it before setting health back up, so farthest
  // ranging doesn't count this client
  if( client->sess.spectatorState != SPECTATOR_NOT )
  {
    if( teamLocal == TEAM_ALIENS )
      spawn = G_SelectAlienLockSpawnPoint( spawn_origin, spawn_angles );
    else if( teamLocal == TEAM_HUMANS )
      spawn = G_SelectHumanLockSpawnPoint( spawn_origin, spawn_angles );
    else
      spawn = G_SelectSpectatorSpawnPoint( spawn_origin, spawn_angles );
  }
  else
  {
    if( origin == NULL || angles == NULL )
    {
      Com_Error( ERR_DROP, "ClientSpawn: origin or angles is NULL" );
      return;
    }

    VectorCopy( origin, spawn_origin );
    VectorCopy( angles, spawn_angles );

    if( spawn != NULL && spawn != ent )
    {
      //start spawn animation on spawnPoint
      G_SetBuildableAnim( spawn, BANIM_SPAWN1, qtrue );

      client->pers.lastSpawnedTime = level.time;

      if( spawn->buildableTeam == TEAM_ALIENS )
      {
        spawn->clientSpawnTime = ALIEN_SPAWN_REPEAT_TIME;
        ent->targetProtectionTime = level.time + ALIEN_SPAWN_PROTECTION_TIME;
      }
      else if( spawn->buildableTeam == TEAM_HUMANS )
      {
        spawn->clientSpawnTime = HUMAN_SPAWN_REPEAT_TIME;
        ent->targetProtectionTime = level.time + HUMAN_SPAWN_PROTECTION_TIME;
      }

      ent->dmgProtectionTime = level.time + client->pers.damageProtectionDuration;

      // reset the barbs
      ent->client->pers.barbs = BG_Weapon( WP_ALEVEL3_UPG )->maxAmmo;
      ent->client->pers.barbRegenTime = level.time;
    }
  }

  // toggle the teleport bit so the client knows to not lerp
  flags = ( ent->client->ps.eFlags & EF_TELEPORT_BIT ) ^ EF_TELEPORT_BIT;

  G_UnlaggedClear( ent );

  // clear everything but the persistant data

  saved = client->pers;
  savedSess = client->sess;
  savedPing = client->ps.ping;
  savedNoclip = client->noclip;
  savedCliprcontents = client->cliprcontents;

  for( i = 0; i < MAX_PERSISTANT; i++ )
    persistant[ i ] = client->ps.persistant[ i ];

  if( client->pers.teamSelection == TEAM_ALIENS )
    miscMisc2 = client->ps.misc[ MISC_MISC2 ];

  eventSequence = client->ps.eventSequence;
  memset( client, 0, sizeof( *client ) );

  client->pers = saved;
  client->sess = savedSess;
  client->ps.ping = savedPing;
  client->noclip = savedNoclip;
  client->cliprcontents = savedCliprcontents;
  client->lastkilled_client = -1;
  client->lasthurt_client = -1;

  for( i = 0; i < MAX_PERSISTANT; i++ )
    client->ps.persistant[ i ] = persistant[ i ];

  //Set the seed for predicted psuedorandomness in bg_pmove.c.
  //Do not touch this value outside of PM_PSRandom(), and
  //do not use PM_PSRandom() outside of bg_pmove.c.
  client->ps.misc[ MISC_SEED ] = rand() / ( RAND_MAX / 0x100 + 1 );

  client->ps.eventSequence = eventSequence;

  // increment the spawncount so the client will detect the respawn
  client->ps.persistant[ PERS_SPAWN_COUNT ]++;
  client->ps.persistant[ PERS_SPECSTATE ] = client->sess.spectatorState;

  client->airOutTime = level.time + 12000;

  SV_GetUserinfo( index, userinfo, sizeof( userinfo ) );
  client->ps.eFlags = flags;

  //Com_Printf( "ent->client->pers->pclass = %i\n", ent->client->pers.classSelection );

  if( spawn && ent != spawn )
    G_Entity_id_init(ent);

  ent->s.groundEntityNum = ENTITYNUM_NONE;
  ent->client = &level.clients[ index ];
  ent->takedamage = qtrue;
  ent->classname = "player";
  G_SetContents( ent, CONTENTS_BODY);
  if( client->pers.teamSelection == TEAM_NONE ) {
    G_SetClipmask( ent, MASK_ASTRALSOLID );
  } else {
    G_SetClipmask( ent, MASK_PLAYERSOLID );
  }
  ent->die = player_die;
  ent->waterlevel = 0;
  ent->watertype = 0;
  ent->flags &= FL_GODMODE | FL_NOTARGET;
  ent->noTelefrag = qfalse;

  ent->client->pmext.explosionMod = 0.000f;

  // calculate each client's acceleration
  ent->evaluateAcceleration = qtrue;

  client->ps.misc[ MISC_MISC ] = 0;

  client->ps.eFlags = flags;
  client->ps.clientNum = index;

  BG_ClassBoundingBox( ent->client->pers.classSelection, ent->r.mins, ent->r.maxs, NULL, NULL, NULL );

  // clear entity values
  if( ent->client->pers.classSelection == PCL_HUMAN )
  {
    BG_AddUpgradeToInventory( UP_MEDKIT, client->ps.stats );
    weapon = client->pers.humanItemSelection;
  }
  else if( client->sess.spectatorState == SPECTATOR_NOT )
    weapon = BG_Class( ent->client->pers.classSelection )->startWeapon;
  else
    weapon = WP_NONE;

  maxAmmo = BG_Weapon( weapon )->maxAmmo;
  maxClips = BG_Weapon( weapon )->maxClips;
  client->ps.stats[ STAT_WEAPON ] = weapon;
  client->ps.ammo = maxAmmo;
  client->ps.clips = maxClips;

  // We just spawned, not changing weapons
  client->ps.persistant[ PERS_NEWWEAPON ] = 0;

  ent->client->ps.stats[ STAT_CLASS ] = ent->client->pers.classSelection;
  ent->client->ps.stats[ STAT_TEAM ] = ent->client->pers.teamSelection;
  if( client->sess.readyToPlay ) {
    client->ps.stats[ STAT_FLAGS ] |= SFL_READY;
  } else {
    client->ps.stats[ STAT_FLAGS ] &= ~SFL_READY;
  }


  ent->client->ps.stats[ STAT_BUILDABLE ] = BA_NONE;
  ent->client->ps.stats[ STAT_STATE ] = 0;
  VectorSet( ent->client->ps.grapplePoint, 0.0f, 0.0f, 1.0f );

  //reset bonus value
  ent->bonusValue = 0;

  client->ps.misc[ MISC_MAX_HEALTH ] =
        BG_Class( client->ps.stats[ STAT_CLASS ] )->health;
  G_ChangeHealth( ent, ent, client->ps.misc[ MISC_MAX_HEALTH ],
                  (HLTHF_SET_TO_CHANGE|
                   HLTHF_EVOLVE_INCREASE) );

  ent->healthReserve = 0;

  //if evolving scale health
  if( ent == spawn )
  {
    const int oldHealth = ent->health;

    // restore evolve cool down
    if( client->pers.teamSelection == TEAM_ALIENS )
      client->ps.misc[ MISC_MISC2 ] = miscMisc2;

    if( !BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->maxHealthDecayRate )
    {
        G_ChangeHealth( ent, ent,
                        ( ent->health * client->pers.evolveHealthFraction ),
                        (HLTHF_SET_TO_CHANGE|
                         HLTHF_EVOLVE_INCREASE|
                         HLTHF_NO_DECAY|
                         HLTHF_IGNORE_ENEMY_DMG) );
    }
    else
    {
      client->ps.misc[ MISC_MAX_HEALTH ] *= client->pers.evolveMaxHealthFraction;
        G_ChangeHealth( ent, ent,
                        client->ps.misc[ MISC_MAX_HEALTH ],
                        (HLTHF_SET_TO_CHANGE|
                         HLTHF_EVOLVE_INCREASE|
                         HLTHF_NO_DECAY|
                         HLTHF_IGNORE_ENEMY_DMG) );
    }

    // ensure that evolving/devolving with low health doesn't kill
    if( ent->health < 1 && oldHealth > 0 )
      G_ChangeHealth( ent, ent, 1,
                      (HLTHF_SET_TO_CHANGE|
                       HLTHF_EVOLVE_INCREASE|
                       HLTHF_NO_DECAY|
                       HLTHF_IGNORE_ENEMY_DMG) );
  }

  if( BG_ClassHasAbility( client->ps.stats[STAT_CLASS], SCA_STAMINA ) )
    client->ps.stats[ STAT_STAMINA ] = STAMINA_MAX;
  else
  {
    client->ps.stats[ STAT_STAMINA ] =
                     BG_Class( client->ps.stats[STAT_CLASS] )->chargeStaminaMax;
  }

  G_SetOrigin( ent, spawn_origin );
  VectorCopy( spawn_origin, client->ps.origin );

#define UP_VEL  150.0f
#define F_VEL   50.0f

  //give aliens some spawn velocity
  if( client->sess.spectatorState == SPECTATOR_NOT &&
      ( client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS ||
        BG_Class( client->pers.classSelection )->team == TEAM_ALIENS ) )
  {
    if( spawn == NULL )
    {
      G_AddPredictableEvent( ent, EV_PLAYER_RESPAWN, 0 );
    }
    else if( ent == spawn )
    {
      //evolution particle system
      G_AddPredictableEvent( ent, EV_ALIEN_EVOLVE, DirToByte( up ) );
    }
    else
    {
      spawn_angles[ YAW ] += 180.0f;
      AngleNormalize360( spawn_angles[ YAW ] );

      if( spawn->s.origin2[ 2 ] > 0.0f )
      {
        vec3_t  forward, dir;

        AngleVectors( spawn_angles, forward, NULL, NULL );
        VectorScale( forward, F_VEL, forward );
        VectorAdd( spawn->s.origin2, forward, dir );
        VectorNormalize( dir );

        VectorScale( dir, UP_VEL, client->ps.velocity );
      }

      G_AddPredictableEvent( ent, EV_PLAYER_RESPAWN, 0 );
    }
  }
  else if( client->sess.spectatorState == SPECTATOR_NOT &&
           client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
  {
    if( spawn != NULL )
    {
      spawn_angles[ YAW ] += 180.0f;
      AngleNormalize360( spawn_angles[ YAW ] );
    }
  }

  // the respawned flag will be cleared after the attack and jump keys come up
  client->ps.pm_flags |= PMF_RESPAWNED;

  SV_GetUsercmd( client - level.clients, &ent->client->pers.cmd );
  G_SetClientViewAngle( ent, spawn_angles );

  if( client->sess.spectatorState == SPECTATOR_NOT ) {
    SV_LinkEntity( ent );

    // force the base weapon up
    if( client->pers.teamSelection == TEAM_HUMANS ) {
      G_ForceWeaponChange( ent, weapon );
    }

    client->ps.weaponstate = WEAPON_READY;
  }

  // don't allow full run speed for a bit
  client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
  client->ps.pm_time = 100;

  client->respawnTime = level.time;
  ent->nextRegenTime = level.time;
  ent->nextEvoTime = level.time;

  client->inactivityTime = level.time + g_inactivity.integer * 1000;
  client->latched_buttons = 0;

  // set default animations
  client->ps.torsoAnim = TORSO_STAND;
  client->ps.legsAnim = LEGS_IDLE;

  if( level.intermissiontime )
    MoveClientToIntermission( ent );
  else
  {
    // fire the targets of the spawn point
    if( spawn != NULL && spawn != ent )
      G_UseTargets( spawn, ent );

    client->ps.misc[MISC_HELD_WEAPON] = client->ps.stats[ STAT_WEAPON ];
    client->ps.weapon = client->ps.stats[ STAT_WEAPON ];
  }

  // run a client frame to drop exactly to the floor,
  // initialize animations and other things
  client->ps.commandTime = level.time - 100;
  ent->client->pers.cmd.serverTime = level.time;
  ClientThink( ent-g_entities );

  VectorCopy( ent->client->ps.viewangles, ent->r.currentAngles );
  VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );
  // positively link the client, even if the command times are weird
  if( client->sess.spectatorState == SPECTATOR_NOT )
  {
    BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );
    SV_LinkEntity( ent );
  }

  // must do this here so the number of active clients is calculated
  CalculateRanks(check_exit_rules);

  // run the presend to set anything else
  ClientEndFrame( ent );

  // clear entity state values
  BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );

  client->pers.infoChangeTime = level.time;
}


/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.

This should NOT be called directly by any game logic,
call SV_GameDropClient(), which will call this and do
server system housekeeping.
============
*/
Q_EXPORT void ClientDisconnect( int clientNum )
{
  gentity_t *ent;
  gentity_t *tent;
  int       i;

  ent = g_entities + clientNum;

  if( !ent->client || ent->client->pers.connected == CON_DISCONNECTED )
    return;

  G_namelog_update_score( ent->client );
  G_LeaveTeam( ent );
  G_namelog_disconnect( ent->client );
  G_Vote( ent, TEAM_NONE, qfalse );

  // stop any following clients
  for( i = 0; i < level.maxclients; i++ )
  {
    // remove any /ignore settings for this clientNum
    Com_ClientListRemove( &level.clients[ i ].sess.ignoreList, clientNum );
  }

  // send effect if they were completely connected
  if( ent->client->pers.connected == CON_CONNECTED &&
      ent->client->sess.spectatorState == SPECTATOR_NOT )
  {
    tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
    tent->s.clientNum = ent->s.clientNum;
  }

  G_LogPrintf( "ClientDisconnect: %i [%s] (%s) \"%s^7\"\n", clientNum,
   ent->client->pers.ip.str, ent->client->pers.guid, ent->client->pers.netname );

  {
    char cleanname[ MAX_NAME_LENGTH ];
    G_SanitiseString( ent->client->pers.netname, cleanname, sizeof( cleanname ) );
    sl_query( DB_SEEN_ADD, cleanname, NULL );
    if( ent->client->pers.admin ) {
      G_SanitiseString( ent->client->pers.admin->name, cleanname, sizeof( cleanname ) );
      sl_query( DB_SEEN_ADD, cleanname, NULL );
    }
  }

  SV_UnlinkEntity( ent );
  ent->id = 0;
  ent->inuse = qfalse;
  ent->classname = "disconnected";
  ent->client->pers.connected = CON_DISCONNECTED;
  ent->client->sess.spectatorState =
      ent->client->ps.persistant[ PERS_SPECSTATE ] = SPECTATOR_NOT;

  SV_SetConfigstring( CS_PLAYERS + clientNum, "");

  CalculateRanks(qtrue);

  // Update player ready states if in warmup
  if( IS_WARMUP )
    G_LevelReady();
}
