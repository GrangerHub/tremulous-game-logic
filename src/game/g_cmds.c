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
==================
G_SanitiseString

Remove color codes and non-alphanumeric characters from a string
==================
*/
void G_SanitiseString( char *in, char *out, int len )
{
  len--;

  while( *in && len > 0 )
  {
    if( Q_IsColorString( in ) )
    {
      in += 2;    // skip color code
      continue;
    }

    if( isalnum( *in ) )
    {
        *out++ = tolower( *in );
        len--;
    }
    in++;
  }
  *out = 0;
}

/*
==================
G_SingleLineString

Remove semicolons and newlines from a string
==================
*/
void G_SingleLineString( char *in, char *out, int len )
{
  len--;

  while( *in && len > 0 )
  {
    if( isprint( *in ) && *in != ';' )
    {
      *out++ = *in;
      len--;
    }
    in++;
  }
  *out = '\0';
}

/*
==================
G_ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 and optionally sets err if invalid or not exactly 1 match
err will have a trailing \n if set
==================
*/
int G_ClientNumberFromString( char *s, char *err, int len )
{
  gclient_t *cl;
  int       i, found = 0, m = -1;
  char      s2[ MAX_NAME_LENGTH ];
  char      n2[ MAX_NAME_LENGTH ];
  char      *p = err;
  int       l, l2 = len;

  if( !s[ 0 ] )
  {
    if( p )
      Q_strncpyz( p, "no player name or slot # provided\n", len );

    return -1;
  }

  // numeric values are just slot numbers
  for( i = 0; s[ i ] && isdigit( s[ i ] ); i++ );
  if( !s[ i ] )
  {
    i = atoi( s );

    if( i < 0 || i >= level.maxclients )
      return -1;

    cl = &level.clients[ i ];

    if( cl->pers.connected == CON_DISCONNECTED )
    {
      if( p )
        Q_strncpyz( p, "no player connected in that slot #\n", len );

      return -1;
    }

    return i;
  }

  G_SanitiseString( s, s2, sizeof( s2 ) );
  if( !s2[ 0 ] )
  {
    if( p )
      Q_strncpyz( p, "no player name provided\n", len );

    return -1;
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
  for( i = 0, cl = level.clients; i < level.maxclients; i++, cl++ )
  {
    if( cl->pers.connected == CON_DISCONNECTED )
      continue;

    G_SanitiseString( cl->pers.netname, n2, sizeof( n2 ) );

    if( !strcmp( n2, s2 ) )
      return i;

    if( strstr( n2, s2 ) )
    {
      if( p )
      {
        l = Q_snprintf( p, l2, "%-2d - %s^7\n", i, cl->pers.netname );
        p += l;
        l2 -= l;
      }

      found++;
      m = i;
    }
  }

  if( found == 1 )
    return m;

  if( found == 0 && err )
    Q_strncpyz( err, "no connected player by that name or slot #\n", len );

  return -1;
}

/*
==================
G_ClientNumbersFromString

Sets plist to an array of integers that represent client numbers that have
names that are a partial match for s.

Returns number of matching clientids up to max.
==================
*/
int G_ClientNumbersFromString( char *s, int *plist, int max )
{
  gclient_t *p;
  int i, found = 0;
  char *endptr;
  char n2[ MAX_NAME_LENGTH ] = {""};
  char s2[ MAX_NAME_LENGTH ] = {""};

  if( max == 0 )
    return 0;

  if( !s[ 0 ] )
    return 0;

  // if a number is provided, it is a clientnum
  i = strtol( s, &endptr, 10 );
  if( *endptr == '\0' )
  {
    if( i >= 0 && i < level.maxclients )
    {
      p = &level.clients[ i ];
      if( p->pers.connected != CON_DISCONNECTED )
      {
        *plist = i;
        return 1;
      }
    }
    // we must assume that if only a number is provided, it is a clientNum
    return 0;
  }

  // now look for name matches
  G_SanitiseString( s, s2, sizeof( s2 ) );
  if( !s2[ 0 ] )
    return 0;
  for( i = 0; i < level.maxclients && found < max; i++ )
  {
    p = &level.clients[ i ];
    if( p->pers.connected == CON_DISCONNECTED )
    {
      continue;
    }
    G_SanitiseString( p->pers.netname, n2, sizeof( n2 ) );
    if( strstr( n2, s2 ) )
    {
      *plist++ = i;
      found++;
    }
  }
  return found;
}

/*
==================
ScoreboardMessage

==================
*/
void ScoreboardMessage( gentity_t *ent )
{
  char      entry[ 1024 ];
  char      string[ 1400 ];
  int       stringlength;
  int       i, j;
  gclient_t *cl;
  int       numSorted;
  weapon_t  weapon = WP_NONE;
  upgrade_t upgrade = UP_NONE;

  // send the latest information on all clients
  string[ 0 ] = 0;
  stringlength = 0;

  numSorted = level.numConnectedClients;

  for( i = 0; i < numSorted; i++ )
  {
    int   ping;

    cl = &level.clients[ level.sortedClients[ i ] ];

    if( cl->pers.connected == CON_CONNECTING )
      ping = -1;
    else
      ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

    if( cl->sess.spectatorState == SPECTATOR_NOT &&
        ( ent->client->pers.teamSelection == TEAM_NONE ||
          cl->pers.teamSelection == ent->client->pers.teamSelection ) )
    {
      weapon = cl->ps.weapon;

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
    else
    {
      weapon = WP_NONE;
      upgrade = UP_NONE;
    }

    Com_sprintf( entry, sizeof( entry ),
      " %d %d %d %d %d %d", level.sortedClients[ i ], cl->ps.persistant[ PERS_SCORE ],
      ping, ( level.time - cl->pers.enterTime ) / 60000, weapon, upgrade );

    j = strlen( entry );

    if( stringlength + j >= sizeof( string ) )
      break;

    strcpy( string + stringlength, entry );
    stringlength += j;
  }

  SV_GameSendServerCommand( ent-g_entities, va( "scores %i %i%s",
    level.alienKills, level.humanKills, string ) );
}


/*
==================
ConcatArgs
==================
*/
char *ConcatArgs( int start )
{
  int         i, c, tlen;
  static char line[ MAX_STRING_CHARS ];
  int         len;
  char        arg[ MAX_STRING_CHARS ];

  len = 0;
  c = Cmd_Argc( );

  for( i = start; i < c; i++ )
  {
    Cmd_ArgvBuffer( i, arg, sizeof( arg ) );
    tlen = strlen( arg );

    if( len + tlen >= MAX_STRING_CHARS - 1 )
      break;

    memcpy( line + len, arg, tlen );
    len += tlen;

    if( len == MAX_STRING_CHARS - 1 )
      break;

    if( i != c - 1 )
    {
      line[ len ] = ' ';
      len++;
    }
  }

  line[ len ] = 0;

  return line;
}

/*
==================
ConcatArgsPrintable
Duplicate of concatargs but enquotes things that need to be
Used to log command arguments in a way that preserves user intended tokenizing
==================
*/
char *ConcatArgsPrintable( int start )
{
  int         i, c, tlen;
  static char line[ MAX_STRING_CHARS ];
  int         len;
  char        arg[ MAX_STRING_CHARS + 2 ];
  char        *printArg;

  len = 0;
  c = Cmd_Argc( );

  for( i = start; i < c; i++ )
  {
    printArg = arg;
    Cmd_ArgvBuffer( i, arg, sizeof( arg ) );
    if( strchr( arg, ' ' ) )
      printArg = va( "\"%s\"", arg );
    tlen = strlen( printArg );

    if( len + tlen >= MAX_STRING_CHARS - 1 )
      break;

    memcpy( line + len, printArg, tlen );
    len += tlen;

    if( len == MAX_STRING_CHARS - 1 )
      break;

    if( i != c - 1 )
    {
      line[ len ] = ' ';
      len++;
    }
  }

  line[ len ] = 0;

  return line;
}

// Change class- this allows you to be any alien class on TEAM_HUMAN and the
// otherway round.
static qboolean Give_Class( gentity_t *ent, char *s ) {
  int newClass = BG_ClassByName( s )->number;

  if( newClass == PCL_NONE ) {
    ADMP( va( "^3give: ^7class %s not found\n", s ) );
    return qfalse;
  }

  ent->client->ps.stats[STAT_FLAGS] &= ~SFL_CLASS_FORCED;

  if( !G_EvolveAfterCheck( ent, newClass, qtrue ) ) {
    ADMP( va( "^3give: ^7failed to give class %s\n", s ) );
    return qfalse;
  }

  return qtrue;
}

static qboolean Give_Gun( gentity_t *ent, char *s ) {
  int w = BG_WeaponByName( s )->number;

  if ( w == WP_NONE ) {
    ADMP( va( "^3give: ^7weapon %s not found\n", s ) );
    return qfalse;
  }

  G_GiveItem( ent, s, 0, BG_Weapon( w )->usesEnergy, qfalse );
  return qtrue;
}

static qboolean Give_Upgrade( gentity_t *ent, char *s ) {
    int u = BG_UpgradeByName( s )->number;

    if( u == UP_NONE ) {
      ADMP( va( "^3give: ^7upgrade %s not found\n", s ) );
      return qfalse;
    }

    G_GiveItem( ent, s, 0, BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->usesEnergy, qfalse );

    return qtrue;
}

static void Give_Health( gentity_t *ent ) {
  ent->health = BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->health;
  BG_AddUpgradeToInventory( UP_MEDKIT, ent->client->ps.stats );
}

static void Give_Funds( gentity_t *ent, char *s ) {
  float credits;

  if( Cmd_Argc( ) < 3 ) {
    credits = 30000.0f;
  } else {
    credits = atof( s + 6 ) *
      ( ent->client->pers.teamSelection ==
        TEAM_ALIENS ? ALIEN_CREDITS_PER_KILL : 1.0f );

    // clamp credits manually, as G_AddCreditToClient() expects a short int
    if( credits > SHRT_MAX ) {
      credits = 30000.0f;
    } else if( credits < SHRT_MIN ) {
      credits = -30000.0f;
    }
  }

  G_AddCreditToClient( ent->client, (short)credits, qtrue );
}

static void Give_Stamina( gentity_t *ent ) {
  if(BG_ClassHasAbility(ent->client->ps.stats[STAT_CLASS], SCA_STAMINA)) {
    ent->client->ps.stats[ STAT_STAMINA ] = STAMINA_MAX;
  }

  if(BG_ClassHasAbility(ent->client->ps.stats[STAT_CLASS], SCA_CHARGE_STAMINA)) {
    ent->client->ps.misc[MISC_CHARGE_STAMINA] =
      BG_Class(ent->client->ps.stats[STAT_CLASS])->chargeStaminaMax /
      BG_Class(ent->client->ps.stats[STAT_CLASS])->chargeStaminaRestoreRate;
  }
}

static void Give_Ammo( gentity_t *ent ) {
  gclient_t *client = ent->client;

  if( client->ps.weapon != WP_ALEVEL3_UPG &&
      BG_Weapon( client->ps.weapon )->infiniteAmmo ) {
    return;
  }

  client->ps.ammo = BG_Weapon( client->ps.weapon )->maxAmmo;
  client->ps.clips = BG_Weapon( client->ps.weapon )->maxClips;

  if( BG_Weapon( client->ps.weapon )->usesEnergy &&
      BG_InventoryContainsUpgrade( UP_BATTPACK, client->ps.stats ) ) {
    client->ps.ammo = (int)( (float)client->ps.ammo * BATTPACK_MODIFIER );
  }
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f( gentity_t *ent ) {
  char      *name;
  char      classes[ MAX_STRING_CHARS ];
  char      weapons[ MAX_STRING_CHARS ];
  char      upgrades[ MAX_STRING_CHARS ];
  class_t   classNum;
  weapon_t  weaponNum;
  upgrade_t upgradeNum;

  memset( classes, 0, sizeof(classes) );
  memset( weapons, 0, sizeof(weapons) );
  memset( upgrades, 0, sizeof(upgrades) );

  for( classNum = (PCL_NONE + 1); classNum < PCL_NUM_CLASSES; classNum++ ) {
    Q_strcat( classes, sizeof(classes), va( "  ^3%s\n", BG_Class( classNum )->name ) );
  }

  for( weaponNum = (WP_NONE + 1); weaponNum < WP_NUM_WEAPONS; weaponNum++ ) {
    Q_strcat( weapons, sizeof(weapons), va( "  ^5%s\n", BG_Weapon( weaponNum )->name ) );
  }

  for( upgradeNum = (UP_NONE + 1); upgradeNum < UP_NUM_UPGRADES; upgradeNum++ ) {
    Q_strcat( upgrades, sizeof(upgrades), va( "  ^2%s\n", BG_Upgrade( upgradeNum )->name ) );
  }

  if( Cmd_Argc( ) < 2 ) {
    // FIXME I am hideous :#(
    ADMP( "^3give: ^7usage: give [what]\n\nNormal\n\n"
          "  health\n  funds <amount>\n  stamina\n  poison\n  gas\n  ammo\n"
          "  class <class name>\n  weapon <weapon name>\n  upgrade <upgrade name>\n" );

    ADMP( va( "\n^3Classes\n\n%s", classes ) );
    ADMP( va( "\n^5Weapons\n\n%s", weapons ) );
    ADMP( va( "\n^2Upgrades\n\n%s^7", upgrades ) );
    return;
  }

  name = ConcatArgs( 1 );
  if( Q_stricmp( name, "all" ) == 0 ) {
    Give_Health( ent );
    Give_Funds( ent, name );
    Give_Stamina( ent );
    Give_Ammo( ent );
  } else if( Q_stricmp( name, "health" ) == 0 ) {
    Give_Health( ent );
  } else if( Q_stricmpn( name, "funds", 5 ) == 0 ) {
    Give_Funds( ent, name );
  } else if( Q_stricmp( name, "stamina" ) == 0 ) {
    Give_Stamina( ent );
  } else if( Q_stricmp( name, "ammo" ) == 0 ) {
    Give_Ammo( ent );
  } else if( Q_stricmp( name, "poison" ) == 0 ) {
    if( ent->client->pers.teamSelection == TEAM_HUMANS ) {
      ent->client->ps.stats[ STAT_STATE ] |= SS_POISONED;
      ent->client->lastPoisonTime = level.time;
      ent->client->lastPoisonClient = ent;
    } else {
      ent->client->ps.stats[ STAT_STATE ] |= SS_BOOSTED;
      ent->client->boostedTime = level.time;
    }
  } else if( Q_stricmp( name, "gas" ) == 0 ) {
    ent->client->ps.eFlags |= EF_POISONCLOUDED;
    ent->client->lastPoisonCloudedTime = level.time;
    SV_GameSendServerCommand( ent->client->ps.clientNum, "poisoncloud" );
  } else if( Q_stricmpn( name, "class", 5 ) == 0 ) {
    char *className;

    if( Cmd_Argc( ) < 3 ) {
        ADMP( "^3give: ^7usage: give class <class name>\n\n" );
        ADMP( va( "\n^3Classes\n\n%s", classes ) );
        return;
    }

    className = name + 6;

    if( !Give_Class( ent, className ) ) {
      ADMP( "^3give: ^7usage: give class <class name>\n\n" );
      ADMP( va( "\n^3Classes\n\n%s", classes ) );
    }
  } else if( Q_stricmpn( name, "weapon", 6 ) == 0 ) {
    char *weaponName;

    if( Cmd_Argc( ) < 3 ) {
        ADMP( "^3give: ^7usage: give weapon <weapon name>\n\n" );
        ADMP( va( "\n^5Weapons\n\n%s", weapons ) );
        return;
    }

    weaponName = name + 7;

    if( !Give_Gun( ent, weaponName ) ) {
      ADMP( "^3give: ^7usage: give weapon <weapon name>\n\n" );
      ADMP( va( "\n^5Weapons\n\n%s", weapons ) );
    }
  } else if( Q_stricmpn( name, "upgrade", 7 ) == 0 ) {
    char *upgradeName;

    if( Cmd_Argc( ) < 3 ) {
        ADMP( "^3give: ^7usage: give upgrade <upgrade name>\n\n" );
        ADMP( va( "\n^2Upgrades\n\n%s^7", upgrades ) );
        return;
    }

    upgradeName = name + 8;

    if( !Give_Upgrade( ent, upgradeName ) ) {
      ADMP( "^3give: ^7usage: give upgrade <upgrade name>\n\n" );
      ADMP( va( "\n^2Upgrades\n\n%s^7", upgrades ) );
    }

    return;
  } else {
    //for shorthand use
    Give_Class( ent, name );
    Give_Gun( ent, name );
    Give_Upgrade( ent, name );
  }
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f( gentity_t *ent )
{
  char  *msg;

  ent->flags ^= FL_GODMODE;

  if( !( ent->flags & FL_GODMODE ) )
    msg = "godmode OFF\n";
  else
    msg = "godmode ON\n";

  SV_GameSendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent )
{
  char  *msg;

  ent->flags ^= FL_NOTARGET;

  if( !G_NoTarget( ent ) )
    msg = "notarget OFF\n";
  else
    msg = "notarget ON\n";

  SV_GameSendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent )
{
  char  *msg;
  if ( ent->client->ps.eFlags & EF_OCCUPYING )
  {
    msg = "noclip is disabled while occupying an activation entity";
  } else
  {
    if( ent->client->noclip )
    {
      msg = "noclip OFF\n";
      ent->r.contents = ent->client->cliprcontents;
    }
    else
    {
      msg = "noclip ON\n";
      ent->client->cliprcontents = ent->r.contents;
      ent->r.contents = 0;
    }

    ent->client->noclip = !ent->client->noclip;

    if( ent->r.linked )
      SV_LinkEntity( ent );
  }


  SV_GameSendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent )
{
  if( g_cheats.integer )
  {
    // reset any activation entities the player might be occupying
    if( ent->client->ps.eFlags & EF_OCCUPYING )
      G_ResetOccupation( ent->occupation.occupied, ent );

    ent->client->ps.misc[ MISC_HEALTH ] = ent->health = 0;
    player_die( ent, ent, ent, 100000, MOD_SUICIDE );
  }
  else
  {
    if( ent->suicideTime == 0 )
    {
      SV_GameSendServerCommand( ent-g_entities, va( "print \"You will suicide in %d seconds\n\"", IS_WARMUP ? 5 : 20 ) );
      ent->suicideTime = level.time + ( IS_WARMUP ? 5000 : 20000 );
    }
    else if( ent->suicideTime > level.time )
    {
      SV_GameSendServerCommand( ent-g_entities, "print \"Suicide cancelled\n\"" );
      ent->suicideTime = 0;
    }
  }
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent )
{
  team_t    team;
  team_t    oldteam = ent->client->pers.teamSelection;
  char      s[ MAX_TOKEN_CHARS ];
  qboolean  force = G_admin_permission( ent, ADMF_FORCETEAMCHANGE );
  int       aliens = level.numAlienClients;
  int       humans = level.numHumanClients;

  if(IS_SCRIM) {
    SV_GameSendServerCommand(
      ent-g_entities,
      va("print \"Scrim mode is ^1on^7, joining teams has been disabled\n\""));
    return;
  }

  if( oldteam == TEAM_ALIENS )
    aliens--;
  else if( oldteam == TEAM_HUMANS )
    humans--;

  // stop team join spam
  if( ent->client->pers.teamChangeTime &&
      level.time - ent->client->pers.teamChangeTime < 1000 )
    return;

  // stop switching teams for gameplay exploit reasons by enforcing a long
  // wait before they can come back
  if( !force && !g_cheats.integer && ent->client->pers.secondsAlive &&
      level.time - ent->client->pers.teamChangeTime < 30000 )
  {
    SV_GameSendServerCommand( ent-g_entities,
      va( "print \"You must wait another %d seconds before changing teams again\n\"",
        (int) ( ( 30000 - ( level.time - ent->client->pers.teamChangeTime ) ) / 1000.f ) ) );
    return;
  }

  //check for forced spectator
  if(ent->client->pers.namelog->specExpires > level.time) {
    SV_GameSendServerCommand(
      ent-g_entities,
      va(
        "print \"You can't join a team yet. Expires in %d seconds.\n\"",
        (ent->client->pers.namelog->specExpires - level.time) / 1000));
    return;
  }

  Cmd_ArgvBuffer( 1, s, sizeof( s ) );

  if( !s[ 0 ] )
  {
    SV_GameSendServerCommand( ent-g_entities, va( "print \"team: %s\n\"",
      BG_Team( oldteam )->name2 ) );
    return;
  }

  if( !Q_stricmp( s, "auto" ) )
  {
    if( level.humanTeamLocked && level.alienTeamLocked )
      team = TEAM_NONE;
    else if( level.humanTeamLocked || humans > aliens )
      team = TEAM_ALIENS;

    else if( level.alienTeamLocked || aliens > humans )
      team = TEAM_HUMANS;
    else
      team = TEAM_ALIENS + rand( ) / ( RAND_MAX / 2 + 1 );
  }
  else switch( G_TeamFromString( s ) )
  {
    case TEAM_NONE:
      team = TEAM_NONE;
      break;

    case TEAM_ALIENS:
      if( level.alienTeamLocked )
      {
        G_TriggerMenu( ent - g_entities, MN_A_TEAMLOCKED );
        return;
      }
      else if( level.humanTeamLocked )
        force = qtrue;

      if( !force && g_teamForceBalance.integer && aliens > humans )
      {
        G_TriggerMenu( ent - g_entities, MN_A_TEAMFULL );
        return;
      }

      team = TEAM_ALIENS;
      break;

    case TEAM_HUMANS:
      if( level.humanTeamLocked )
      {
        G_TriggerMenu( ent - g_entities, MN_H_TEAMLOCKED );
        return;
      }
      else if( level.alienTeamLocked )
        force = qtrue;

      if( !force && g_teamForceBalance.integer && humans > aliens )
      {
        G_TriggerMenu( ent - g_entities, MN_H_TEAMFULL );
        return;
      }

      team = TEAM_HUMANS;
      break;

    default:
      SV_GameSendServerCommand( ent-g_entities,
        va( "print \"Unknown team: %s\n\"", s ) );
      return;
  }

  // stop team join spam
  if( oldteam == team )
    return;

  if( team != TEAM_NONE && g_maxGameClients.integer &&
    level.numPlayingClients >= g_maxGameClients.integer )
  {
    G_TriggerMenu( ent - g_entities, MN_PLAYERLIMIT );
    return;
  }

  // Apply the change
  G_ChangeTeam( ent, team );

  // Update player ready states if in warmup
  if( IS_WARMUP )
    G_LevelReady();
}

/*
=================
Cmd_SpecMe_f
=================
*/
void Cmd_SpecMe_f( gentity_t *ent )
{
  qboolean  force = G_admin_permission( ent, ADMF_FORCETEAMCHANGE );

  // stop team join spam
  if( ent->client->pers.teamChangeTime &&
      level.time - ent->client->pers.teamChangeTime < 1000 )
    return;

  // stop switching teams for gameplay exploit reasons by enforcing a long
  // wait before they can come back
  if( !force && !g_cheats.integer && ent->client->pers.secondsAlive &&
      level.time - ent->client->pers.teamChangeTime < 30000 )
  {
    SV_GameSendServerCommand( ent-g_entities,
      va( "print \"You must wait another %d seconds before changing teams again\n\"",
        (int) ( ( 30000 - ( level.time - ent->client->pers.teamChangeTime ) ) / 1000.f ) ) );
    return;
  }

  // Apply the change
  G_ChangeTeam( ent, TEAM_NONE );

  // Update player ready states if in warmup
  if( IS_WARMUP )
    G_LevelReady();
}

/*
==================
G_CensorString
==================
*/
static char censors[ 20000 ];
static int numcensors;

void G_LoadCensors( void )
{
  char *text_p, *token;
  char text[ 20000 ];
  char *term;
  int  len;
  fileHandle_t f;

  numcensors = 0;

  if( !g_censorship.string[ 0 ] )
    return;

  len = FS_FOpenFileByMode( g_censorship.string, &f, FS_READ );
  if( len < 0 )
  {
    Com_Printf( S_COLOR_RED "ERROR: Censors file %s doesn't exist\n",
      g_censorship.string );
    return;
  }
  if( len == 0 || len >= sizeof( text ) - 1 )
  {
    FS_FCloseFile( f );
    Com_Printf( S_COLOR_RED "ERROR: Censors file %s is %s\n",
      g_censorship.string, len == 0 ? "empty" : "too long" );
    return;
  }
  FS_Read2( text, len, f );
  FS_FCloseFile( f );
  text[ len ] = 0;

  term = censors;

  text_p = text;
  while( 1 )
  {
    token = COM_Parse( &text_p );
    if( !*token || sizeof( censors ) - ( term - censors ) < 4 )
      break;
    Q_strncpyz( term, token, sizeof( censors ) - ( term - censors ) );
    Q_strlwr( term );
    term += strlen( term ) + 1;
    if( sizeof( censors ) - ( term - censors ) == 0 )
      break;
    token = COM_ParseExt( &text_p, qfalse );
    Q_strncpyz( term, token, sizeof( censors ) - ( term - censors ) );
    term += strlen( term ) + 1;
    numcensors++;
  }
  Com_Printf( "Parsed %d string replacements\n", numcensors );
}

void G_CensorString( char *out, const char *in, int len, gentity_t *ent )
{
  const char *s, *m;
  int  i;

  if( !numcensors || G_admin_permission( ent, ADMF_NOCENSORFLOOD) )
  {
    Q_strncpyz( out, in, len );
    return;
  }

  len--;
  while( *in )
  {
    if( Q_IsColorString( in ) )
    {
      if( len < 2 )
        break;
      *out++ = *in++;
      *out++ = *in++;
      len -= 2;
      continue;
    }
    if( !isalnum( *in ) )
    {
      if( len < 1 )
        break;
      *out++ = *in++;
      len--;
      continue;
    }
    m = censors;
    for( i = 0; i < numcensors; i++, m++ )
    {
      s = in;
      while( *s && *m )
      {
        if( Q_IsColorString( s ) )
        {
          s += 2;
          continue;
        }
        if( !isalnum( *s ) )
        {
          s++;
          continue;
        }
        if( tolower( *s ) != *m )
          break;
        s++;
        m++;
      }
      // match
      if( !*m )
      {
        in = s;
        m++;
        while( *m )
        {
          if( len < 1 )
            break;
          *out++ = *m++;
          len--;
        }
        break;
      }
      else
      {
        while( *m )
          m++;
        m++;
        while( *m )
          m++;
      }
    }
    if( len < 1 )
      break;
    // no match
    if( i == numcensors )
    {
      *out++ = *in++;
      len--;
    }
  }
  *out = 0;
}

/*
==================
G_Say
==================
*/
static qboolean G_SayTo( gentity_t *ent, gentity_t *other, saymode_t mode, const char *message )
{
  if( !other )
    return qfalse;

  if( !other->inuse )
    return qfalse;

  if( !other->client )
    return qfalse;

  if( other->client->pers.connected != CON_CONNECTED )
    return qfalse;

  if( Com_ClientListContains( &other->client->sess.ignoreList, (int)( ent - g_entities ) ) )
    return qfalse;

  if( ( ent && !OnSameTeam( ent, other ) ) &&
      ( mode == SAY_TEAM || mode == SAY_AREA || mode == SAY_TPRIVMSG ) )
  {
    if( other->client->pers.teamSelection != TEAM_NONE )
      return qfalse;

    // specs with ADMF_SPEC_ALLCHAT flag can see team chat
    if( !G_admin_permission( other, ADMF_SPEC_ALLCHAT ) && mode != SAY_TPRIVMSG )
      return qfalse;
  }

  SV_GameSendServerCommand( other-g_entities, va( "chat %d %d \"%s\"",
    (int)( ent ? ent-g_entities : -1 ),
    mode,
    message ) );

  return qtrue;
}

void G_Say( gentity_t *ent, saymode_t mode, const char *chatText )
{
  int         j;
  gentity_t   *other;
  // don't let text be too long for malicious reasons
  char        text[ MAX_SAY_TEXT ];
  char         *tmsgcolor = S_COLOR_YELLOW;

  // check if blocked by g_specChat 0
  if( ( !g_specChat.integer ) && ( mode != SAY_TEAM ) &&
      ( ent ) && ( ent->client->pers.teamSelection == TEAM_NONE ) &&
      ( !G_admin_permission( ent, ADMF_NOCENSORFLOOD ) ) )
  {
    SV_GameSendServerCommand( ent-g_entities, "print \"say: Global chatting for "
      "spectators has been disabled. You may only use team chat.\n\"" );
    mode = SAY_TEAM;
  }

  if( ( IS_SCRIM ) && ( mode != SAY_TEAM ) &&
      ( ent ) &&
      ( ( ent->client->pers.teamSelection == TEAM_NONE ) ||
        ( level.scrim.mode == SCRIM_MODE_STARTED &&
          !IS_WARMUP && !level.intermissiontime ) ) &&
      ( !G_admin_permission( ent, ADMF_NOCENSORFLOOD ) ) && 
      ( !G_admin_permission( ent, "scrim" ) ) )
  {
    SV_GameSendServerCommand( ent-g_entities,
      "print \"say: A scrim is in progress, global chatting "
      "has been disabled. You may only use team chat.\n\"" );
    mode = SAY_TEAM;
  }

  switch( mode )
  {
    case SAY_ALL:
      G_LogPrintf( "Say: %d \"%s" S_COLOR_WHITE "\": " S_COLOR_GREEN "%s\n",
        (int)( ( ent ) ? ent - g_entities : -1 ),
        ( ent ) ? ent->client->pers.netname : "console", chatText );
      break;
    case SAY_TEAM:
      // console say_team is handled in g_svscmds, not here
      if( !( ent->client->pers.teamSelection == TEAM_NONE ) )
        tmsgcolor = S_COLOR_CYAN;
      if( !ent || !ent->client )
        Com_Error( ERR_FATAL, "SAY_TEAM by non-client entity" );
      G_LogPrintf( "SayTeam: %d \"%s" S_COLOR_WHITE "\": %s%s\n",
        (int)( ent - g_entities ), ent->client->pers.netname, tmsgcolor, chatText );
      break;
    case SAY_RAW:
      if( ent )
        Com_Error( ERR_FATAL, "SAY_RAW by client entity" );
      G_LogPrintf( "Chat: -1 \"console\": %s\n", chatText );
    default:
      break;
  }

  G_CensorString( text, chatText, sizeof( text ), ent );

  // send it to all the apropriate clients
  for( j = 0; j < level.maxclients; j++ )
  {
    other = &g_entities[ j ];
    G_SayTo( ent, other, mode, text );
  }
}

/*
==================
Cmd_SayArea_f
==================
*/
static void Cmd_SayArea_f( gentity_t *ent )
{
  int    entityList[ MAX_GENTITIES ];
  int    num, i;
  vec3_t range = { 1000.0f, 1000.0f, 1000.0f };
  vec3_t mins, maxs;
  char   *msg;

  if( Cmd_Argc( ) < 2 )
  {
    ADMP( "usage: say_area [message]\n" );
    return;
  }

  msg = ConcatArgs( 1 );

  for(i = 0; i < 3; i++ )
    range[ i ] = g_sayAreaRange.value;

  G_LogPrintf( "SayArea: %d \"%s" S_COLOR_WHITE "\": " S_COLOR_BLUE "%s\n",
    (int)( ent - g_entities ), ent->client->pers.netname, msg );

  VectorAdd( ent->r.currentOrigin, range, maxs );
  VectorSubtract( ent->r.currentOrigin, range, mins );

  num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
    G_SayTo( ent, &g_entities[ entityList[ i ] ], SAY_AREA, msg );

  //Send to ADMF_SPEC_ALLCHAT candidates
  for( i = 0; i < level.maxclients; i++ )
  {
    if( g_entities[ i ].client->pers.teamSelection == TEAM_NONE &&
        G_admin_permission( &g_entities[ i ], ADMF_SPEC_ALLCHAT ) )
    {
      G_SayTo( ent, &g_entities[ i ], SAY_AREA, msg );
    }
  }
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent )
{
  char    *p;
  char    cmd[ MAX_TOKEN_CHARS ];
  saymode_t mode = SAY_ALL;

  if( Cmd_Argc( ) < 2 )
    return;

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );
  if( Q_stricmp( cmd, "say_team" ) == 0 )
    mode = SAY_TEAM;

  p = ConcatArgs( 1 );

  G_Say( ent, mode, p );
}

/*
==================
Cmd_VSay_f
==================
*/
void Cmd_VSay_f( gentity_t *ent )
{
  char            arg[MAX_TOKEN_CHARS];
  char            text[ MAX_TOKEN_CHARS ];
  voiceChannel_t  vchan;
  voice_t         *voice;
  voiceCmd_t      *cmd;
  voiceTrack_t    *track;
  int             cmdNum = 0;
  int             trackNum = 0;
  char            voiceName[ MAX_VOICE_NAME_LEN ] = {"default"};
  char            voiceCmd[ MAX_VOICE_CMD_LEN ] = {""};
  char            vsay[ 12 ] = {""};
  weapon_t        weapon;

  if( !ent || !ent->client )
    Com_Error( ERR_FATAL, "Cmd_VSay_f() called by non-client entity" );

  Cmd_ArgvBuffer( 0, arg, sizeof( arg ) );
  if( Cmd_Argc( ) < 2 )
  {
    SV_GameSendServerCommand( ent-g_entities, va(
      "print \"usage: %s command [text] \n\"", arg ) );
    return;
  }
  if( !level.voices )
  {
    SV_GameSendServerCommand( ent-g_entities, va(
      "print \"%s: voice system is not installed on this server\n\"", arg ) );
    return;
  }
  if( !g_voiceChats.integer )
  {
    SV_GameSendServerCommand( ent-g_entities, va(
      "print \"%s: voice system administratively disabled on this server\n\"",
      arg ) );
    return;
  }
  if( !Q_stricmp( arg, "vsay" ) )
    vchan = VOICE_CHAN_ALL;
  else if( !Q_stricmp( arg, "vsay_team" ) )
    vchan = VOICE_CHAN_TEAM;
  else if( !Q_stricmp( arg, "vsay_local" ) )
    vchan = VOICE_CHAN_LOCAL;
  else
    return;
  Q_strncpyz( vsay, arg, sizeof( vsay ) );

  if( ent->client->pers.voice[ 0 ] )
    Q_strncpyz( voiceName, ent->client->pers.voice, sizeof( voiceName ) );
  voice = BG_VoiceByName( level.voices, voiceName );
  if( !voice )
  {
    SV_GameSendServerCommand( ent-g_entities, va(
      "print \"%s: voice '%s' not found\n\"", vsay, voiceName ) );
    return;
  }

  Cmd_ArgvBuffer( 1, voiceCmd, sizeof( voiceCmd ) ) ;
  cmd = BG_VoiceCmdFind( voice->cmds, voiceCmd, &cmdNum );
  if( !cmd )
  {
    SV_GameSendServerCommand( ent-g_entities, va(
     "print \"%s: command '%s' not found in voice '%s'\n\"",
      vsay, voiceCmd, voiceName ) );
    return;
  }

  // filter non-spec humans by their primary weapon as well
  weapon = WP_NONE;
  if( ent->client->sess.spectatorState == SPECTATOR_NOT )
  {
    weapon = ent->client->ps.stats[ STAT_WEAPON ];
  }

  track = BG_VoiceTrackFind( cmd->tracks, ent->client->pers.teamSelection,
    ent->client->pers.classSelection, weapon, (int)ent->client->voiceEnthusiasm,
    &trackNum );
  if( !track )
  {
    SV_GameSendServerCommand( ent-g_entities, va(
      "print \"%s: no available track for command '%s', team %d, "
      "class %d, weapon %d, and enthusiasm %d in voice '%s'\n\"",
      vsay, voiceCmd, ent->client->pers.teamSelection,
      ent->client->pers.classSelection, weapon,
      (int)ent->client->voiceEnthusiasm, voiceName ) );
    return;
  }

  if( !Q_stricmp( ent->client->lastVoiceCmd, cmd->cmd ) )
    ent->client->voiceEnthusiasm++;

  Q_strncpyz( ent->client->lastVoiceCmd, cmd->cmd,
    sizeof( ent->client->lastVoiceCmd ) );

  // optional user supplied text
  Cmd_ArgvBuffer( 2, arg, sizeof( arg ) );
  G_CensorString( text, arg, sizeof( text ), ent );

  switch( vchan )
  {
    case VOICE_CHAN_ALL:
    case VOICE_CHAN_LOCAL:
      SV_GameSendServerCommand( -1, va(
        "voice %d %d %d %d \"%s\"\n",
        (int)( ent-g_entities ), vchan, cmdNum, trackNum, text ) );
      break;
    case VOICE_CHAN_TEAM:
      G_TeamCommand( ent->client->pers.teamSelection, va(
        "voice %d %d %d %d \"%s\"\n",
        (int)( ent-g_entities ), vchan, cmdNum, trackNum, text ) );
      break;
    default:
      break;
  }
}

/*
=================
Cmd_TeamStatus_f
=================
*/
void Cmd_TeamStatus_f( gentity_t *ent )
{
  char multiple[ 12 ];
  int builders = 0;
  int arm = 0, mediboost = 0;
  int omrccount = 0, omrchealth = 0;
  qboolean omrcbuild = qfalse;
  gentity_t *tmp;
  team_t team;
  int i;

  if( !g_teamStatus.integer )
  {
    SV_GameSendServerCommand( ent - g_entities,
      "print \"teamstatus is disabled.\n\"" );
    return;
  }

  team = ent->client->pers.teamSelection;

  if( level.lastTeamStatus[ team ] &&
      ( level.time - level.lastTeamStatus[ team ] ) < g_teamStatus.integer * 1000 )
  {
    ADMP( va("You may only check your team's status once every %i seconds.\n",
          g_teamStatus.integer  ));
    return;
  }

  level.lastTeamStatus[ team ] = level.time;

  tmp = &g_entities[ 0 ];
  for ( i = 0; i < level.num_entities; i++, tmp++ )
  {
    if( i < MAX_CLIENTS )
    {
      if( tmp->client &&
          tmp->client->pers.connected == CON_CONNECTED &&
          tmp->client->pers.teamSelection == ent->client->pers.teamSelection &&
          tmp->health > 0 &&
          ( tmp->client->ps.stats[ STAT_CLASS ] == PCL_ALIEN_BUILDER0 ||
            tmp->client->ps.stats[ STAT_CLASS ] == PCL_ALIEN_BUILDER0_UPG ||
            BG_InventoryContainsWeapon( WP_HBUILD, tmp->client->ps.stats ) ) )
        builders++;
      continue;
    }

    if( tmp->s.eType == ET_BUILDABLE )
    {
      if( tmp->buildableTeam != ent->client->pers.teamSelection ||
          tmp->health <= 0 )
        continue;

      switch( tmp->s.modelindex )
      {
        case BA_H_REACTOR:
        case BA_A_OVERMIND:
          omrccount++;
          if( tmp->health > omrchealth )
            omrchealth = tmp->health;
          if( !omrcbuild )
            omrcbuild = tmp->spawned;
          break;
        case BA_H_ARMOURY:
          arm++;
          break;
        case BA_H_MEDISTAT:
        case BA_A_BOOSTER:
          mediboost++;
          break;
        default:
          break;
      }
    }
  }

  if( omrccount > 1 )
    Com_sprintf( multiple, sizeof( multiple ), "^7[x%d]", omrccount );
  else
    multiple[ 0 ] = '\0';

  switch( ent->client->pers.teamSelection )
  {
    case TEAM_ALIENS:
      G_Say( ent, SAY_TEAM,
        va( "^3OM: %s(%d)%s ^3Eggs: ^5%d ^3Builders: ^5%d ^3Boosters: ^5%d^7" ,
        ( !omrccount ) ? "^1Down" : ( omrcbuild ) ? "^2Up" : "^5Building",
        omrchealth * 100 / OVERMIND_HEALTH,
        multiple,
        level.numAlienSpawns,
        builders,
        mediboost ) );
      break;
    case TEAM_HUMANS:
      G_Say( ent, SAY_TEAM,
        va( "^3RC: %s(%d)%s ^3Telenodes: ^5%d ^3Builders: ^5%d ^3Armouries: ^5%d ^3Medistations: ^5%d^7" ,
        ( !omrccount ) ? "^1Down" : ( omrcbuild ) ? "^2Up" : "^5Building",
        omrchealth * 100 / REACTOR_HEALTH,
        multiple,
        level.numHumanSpawns,
        builders,
        arm, mediboost ) );
      break;
    default:
      break;
  }
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent )
{
  if( !ent->client )
    return;
  SV_GameSendServerCommand( ent - g_entities,
                          va( "print \"origin: %f %f %f\n\"",
                              ent->r.currentOrigin[ 0 ], ent->r.currentOrigin[ 1 ],
                              ent->r.currentOrigin[ 2 ] ) );
}

/*
==================
Cmd_CallVote_f
==================
*/
void Cmd_CallVote_f( gentity_t *ent )
{
  char   cmd[ MAX_TOKEN_CHARS ],
         vote[ MAX_TOKEN_CHARS ],
         arg[ MAX_TOKEN_CHARS ],
         extra[ MAX_TOKEN_CHARS ];
  char   name[ MAX_NAME_LENGTH ] = "";
  char   caller[ MAX_NAME_LENGTH ] = "";
  char   reason[ MAX_TOKEN_CHARS ];
  char   *creason;
  char   *poll;
  char   cVoteDisplayString[ MAX_STRING_CHARS ];
  int    clientNum = -1;
  int    id = -1;
  team_t team;
  qboolean voteYes = qtrue;

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );
  Cmd_ArgvBuffer( 1, vote, sizeof( vote ) );
  Cmd_ArgvBuffer( 2, arg, sizeof( arg ) );
  Cmd_ArgvBuffer( 3, extra, sizeof( extra ) );
  creason = ConcatArgs( 3 );
  G_DecolorString( creason, reason, sizeof( reason ) );
  poll = ConcatArgs( 2 );

  if( !Q_stricmp( cmd, "callteamvote" ) )
    team = ent->client->pers.teamSelection;
  else
    team = TEAM_NONE;

  if( !g_allowVote.integer )
  {
    SV_GameSendServerCommand( ent-g_entities,
      va( "print \"%s: voting not allowed here\n\"", cmd ) );
    return;
  }

  if(IS_SCRIM) {
    const scrim_team_t scrim_team =
      BG_Scrim_Team_From_Playing_Team(
        &level.scrim, ent->client->pers.teamSelection);
    const qboolean iscaptain =
      (
        scrim_team != SCRIM_TEAM_NONE &&
        !ent->client->pers.guidless &&
        level.scrim.team[scrim_team].has_captain &&
        !Q_stricmp(
          level.scrim.team[scrim_team].captain_guid, ent->client->pers.guid));

    if(level.scrim.mode == SCRIM_MODE_SETUP) {
      if(G_admin_permission( ent, "scrim" )) {
        if(team == TEAM_NONE) {
          if(
            Q_stricmp( vote, "cancel" ) &&
            Q_stricmp( vote, "map" ) &&
            Q_stricmp( vote, "poll" ) &&
            Q_stricmp( vote, "map" ) &&
            Q_stricmp( vote, "kick" ) &&
            Q_stricmp( vote, "mute" ) &&
            Q_stricmp( vote, "unmute" )) {
            SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
            SV_GameSendServerCommand( ent-g_entities, "print \"Valid vote commands "
                "are: map, kick, poll, mute, "
                "unmute, and cancel\n" );
            return;
          }
        } else {
          if(
            Q_stricmp( vote, "cancel" ) &&
            Q_stricmp( vote, "poll" ) &&
            Q_stricmp( vote, "denybuild" ) &&
            Q_stricmp( vote, "allowbuild" ) &&
            Q_stricmp( vote, "spec" )) {
            SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
            SV_GameSendServerCommand( ent-g_entities, "print \"Valid team vote commands "
                "are: poll, denybuild, allowbuild, spec, and cancel\n" );
            return;
          }
        }
      } else if(iscaptain) {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: you don't have permission to call votes while a scrim is being setup^7\n\"", cmd ) );
        return;
      } else {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: you don't have permission to call votes while a scrim is ^1on^7\n\"", cmd ) );
        return;
      }
    } else {
      if(G_admin_permission( ent, "scrim" )) {
        if(team == TEAM_NONE) {
          if(
            Q_stricmp( vote, "cancel" ) &&
            Q_stricmp( vote, "draw" ) &&
            Q_stricmp( vote, "kick" ) &&
            Q_stricmp( vote, "spec" ) &&
            Q_stricmp( vote, "poll" ) &&
            Q_stricmp( vote, "mute" ) &&
            Q_stricmp( vote, "unmute" )) {
            SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
            SV_GameSendServerCommand( ent-g_entities, "print \"Valid vote commands "
                "are: draw, spec, kick, poll, mute, "
                "unmute, and cancel\n" );
            return;
          }
        } else {
          if(
            Q_stricmp( vote, "cancel" ) &&
            Q_stricmp( vote, "poll" ) &&
            Q_stricmp( vote, "admitdefeat" ) &&
            Q_stricmp( vote, "forfeit" ) &&
            Q_stricmp( vote, "spec" ) &&
            Q_stricmp( vote, "denybuild" ) &&
            Q_stricmp( vote, "allowbuild" )) {
            SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
            SV_GameSendServerCommand( ent-g_entities, "print \"Valid team vote commands "
                "are: spec, poll, denybuild, allowbuild, admitdefeat, forfeit, and cancel\n" );
            return;
          }
        }
      } else if(iscaptain) {
        if(team == TEAM_NONE) {
          if(
            Q_stricmp( vote, "cancel" ) &&
            Q_stricmp( vote, "draw" ) &&
            Q_stricmp( vote, "spec" ) &&
            Q_stricmp( vote, "kick" )) {
            SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
            SV_GameSendServerCommand( ent-g_entities, "print \"Valid vote commands "
                "are: spec, kick, draw, and cancel\n" );
            return;
          }
        } else {
          if(
            Q_stricmp( vote, "cancel" ) &&
            Q_stricmp( vote, "poll" ) &&
            Q_stricmp( vote, "admitdefeat" ) &&
            Q_stricmp( vote, "forfeit" ) &&
            Q_stricmp( vote, "denybuild" ) &&
            Q_stricmp( vote, "allowbuild" ) &&
            Q_stricmp( vote, "spec" )) {
            SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
            SV_GameSendServerCommand( ent-g_entities, "print \"Valid team vote commands "
                "are: spec, poll, denybuild, allowbuild, admitdefeat, forfeit and cancel\n" );
            return;
          }
        }
      } else {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: you don't have permission to call votes while a scrim is ^1on^7\n\"", cmd ) );
        return;
      }
    }
  }

  if( level.voteTime[ team ] )
  {
    if( !Q_stricmp( vote, "cancel" ) )
    {
      if( ent->client != level.voteCaller[ team ] )
      {
        // tell the player to go fly a kite if it was someone else's vote they
        // are trying to cancel
        SV_GameSendServerCommand( ent-g_entities,
            va( "print \"%s: you cannot cancel a vote that you did not "
                "call\n\"", cmd ) );
        return;
      }

      // tell all players that the vote has been called off
      SV_GameSendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " decided to "
            "call off the " S_COLOR_YELLOW "%s" S_COLOR_WHITE " vote\n\"",
            ent->client->pers.netname, cVoteDisplayString ) );

      // force all voting clients to be counted as No votes
      G_EndVote( team, qtrue );
      return;
    }
    else
    {
      SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: a vote is already in progress\n\"", cmd ) );
      return;
    }
  }

  // protect against the dreaded exploit of '\n'-interpretation inside quotes
  if( strchr( arg, '\n' ) || strchr( arg, '\r' ) ||
      strchr( extra, '\n' ) || strchr( extra, '\r' ) ||
      strchr( creason, '\n' ) || strchr( creason, '\r' ) ||
      strchr( poll, '\n' ) || strchr( poll, '\r' ) )
  {
    SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
    return;
  }

  if( level.voteExecuteTime[ team ] )
    G_ExecuteVote( team );

  level.voteDelay[ team ] = 0;
  level.voteThreshold[ team ] = 50;

  if( g_voteLimit.integer > 0 &&
    ent->client->pers.namelog->voteCount >= g_voteLimit.integer &&
    !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) )
  {
    SV_GameSendServerCommand( ent-g_entities, va(
      "print \"%s: you have already called the maximum number of votes (%d)\n\"",
      cmd, g_voteLimit.integer ) );
    return;
  }

  // kick, spec, mute, unmute, denybuild, allowbuild
  if( !Q_stricmp( vote, "kick" ) ||
      !Q_stricmp( vote, "spec") ||
      !Q_stricmp( vote, "mute" ) || !Q_stricmp( vote, "unmute" ) ||
      !Q_stricmp( vote, "denybuild" ) || !Q_stricmp( vote, "allowbuild" ) )
  {
    char err[ MAX_STRING_CHARS ];

    if( !arg[ 0 ] )
    {
      SV_GameSendServerCommand( ent-g_entities,
        va( "print \"%s: no target\n\"", cmd ) );
      return;
    }

    // with a little extra work only players from the right team are considered
    clientNum = G_ClientNumberFromString( arg, err, sizeof( err ) );

    if( clientNum == -1 )
    {
      ADMP( va( "%s: %s", cmd, err ) );
      return;
    }

    if(
      ent &&
      ent->s.number == clientNum &&
      Q_stricmp( vote, "unmute" ) && Q_stricmp( vote, "allowbuild" )) {
      SV_GameSendServerCommand( ent-g_entities,
        va( "print \"%s: you can't call that vote on yourself\n\"", cmd ) );
      return;
    }

    G_DecolorString( level.clients[ clientNum ].pers.netname, name, sizeof( name ) );
    id = level.clients[ clientNum ].pers.namelog->id;

    if( !Q_stricmp( vote, "kick" ) || !Q_stricmp( vote, "spec") ||
        !Q_stricmp( vote, "mute" ) || !Q_stricmp( vote, "denybuild" ) )
    {
      if( G_admin_permission( g_entities + clientNum, ADMF_IMMUNITY ) )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: player is immune\n\"", cmd ) );

        G_AdminMessage( NULL, va( S_COLOR_WHITE "%s" S_COLOR_YELLOW " attempted %s %s"
                                " on immune player " S_COLOR_WHITE "%s" S_COLOR_YELLOW
                                " for: %s",
                                ent->client->pers.netname, cmd, vote,
                                g_entities[ clientNum ].client->pers.netname,
                                reason[ 0 ] ? reason : "no reason" ) );
        return;
      }

      if( team != TEAM_NONE &&
          ( ent->client->pers.teamSelection !=
            level.clients[ clientNum ].pers.teamSelection ) )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: player is not on your team\n\"", cmd ) );
        return;
      }

      if( !reason[ 0 ] && !G_admin_permission( ent, ADMF_UNACCOUNTABLE ) )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: You must provide a reason\n\"", cmd ) );
        return;
      }
    }
  }

  if( !Q_stricmp( vote, "spec" ) )
  {
    if(
      level.clients[clientNum].pers.teamSelection == TEAM_NONE &&
      level.clients[clientNum].pers.namelog->specExpires > level.time) {
      SV_GameSendServerCommand( ent-g_entities,
        va(
           "print \"%s: %s is already forced to be on spectators\n\"",
           level.clients[clientNum].pers.netname, cmd ) );
      return;
    }

    Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
      "putteam %i s %s",id, g_adminTempSpec.string );
    Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                 "^4[^3Force Spec^4] ^5player '%s' (Needs > %d%% of %s)", name, level.voteThreshold[ team ],
                 g_impliedVoting.integer ? "active players" : "total votes" );

    if( reason[ 0 ] )
    {
      Q_strcat( level.voteDisplayString[ team ],
        sizeof( level.voteDisplayString[ team ] ), va( "%cfor '%s'", STRING_DELIMITER, reason ) );
    }

    level.voteType[ team ] = SPEC_VOTE;
  }
  else if( !Q_stricmp( vote, "poll" ) )
  {
    if( strlen( poll ) == 0 )
    {
      SV_GameSendServerCommand( ent-g_entities,
        va( "print \"%s: please specify a poll question\n\"", cmd ) );
      return;
    }

    // use an exec string that allows log parsers to see the vote
    Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
                 "echo poll \"^7%s^7\"", poll );
    Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                 "^4[^7Poll^4]^5%c%s^5", STRING_DELIMITER, poll );
    voteYes = qfalse;
    level.voteType[ team ] = POLL_VOTE;
  }
  else if( team == TEAM_NONE )
  {
    if( !Q_stricmp( vote, "kick" ) )
    {
      if( level.clients[ clientNum ].pers.localClient )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: player is immune\n\"", cmd ) );
        return;
      }

      Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
        "ban %s \"1s%s\" vote kick (%s)", level.clients[ clientNum ].pers.ip.str,
        g_adminTempBan.string, reason );
      Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                   "^4[^3Kick^4] ^5player '%s' (Needs > %d%% of %s)", name, level.voteThreshold[ team ],
                   g_impliedVoting.integer ? "active players" : "total votes" );

      if( reason[ 0 ] )
      {
        Q_strcat( level.voteDisplayString[ team ],
          sizeof( level.voteDisplayString[ team ] ), va( "%cfor '%s'", STRING_DELIMITER, reason ) );
      }

      level.voteType[ team ] = KICK_VOTE;
    } else if( !Q_stricmp( vote, "mute" ) )
    {
      if( level.clients[ clientNum ].pers.namelog->muted )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: player is already muted\n\"", cmd ) );
        return;
      }

      Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
        "mute %d", id );
      Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                   "^4[^3Mute^4]^5 player '%s' (Needs > %d%% of %s)", name, level.voteThreshold[ team ],
                   g_impliedVoting.integer ? "active players" : "total votes" );

      if( reason[ 0 ] )
      {
        Q_strcat( level.voteDisplayString[ team ],
          sizeof( level.voteDisplayString[ team ] ), va( "%cfor '%s'", STRING_DELIMITER, reason ) );
      }

      level.voteType[ team ] = MUTE_VOTE;
    }
    else if( !Q_stricmp( vote, "unmute" ) )
    {
      if( !level.clients[ clientNum ].pers.namelog->muted )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: player is not currently muted\n\"", cmd ) );
        return;
      }

      Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
        "unmute %d", id );
      Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                   "^4[^3Unmute^4] ^5player '%s' (Needs > %d%% of %s)", name, level.voteThreshold[ team ],
                   g_impliedVoting.integer ? "active players" : "total votes" );

      level.voteType[ team ] = UNMUTE_VOTE;
    }
    else if( !Q_stricmp( vote, "map_restart" ) )
    {
      if( arg[ 0 ] )
      {
        char map[ MAX_QPATH ];
        Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );

        if( !G_LayoutExists( map, arg ) )
        {
          SV_GameSendServerCommand( ent-g_entities,
            va( "print \"%s: layout '%s' does not exist for map '%s'\n\"",
              cmd, arg, map ) );
          return;
        }


      }

      Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
        "^4[^1Restart the current map^4]^5%s", arg[ 0 ] ? va( " with layout '%s'", arg ) : "" );
      Q_strcat( level.voteDisplayString[ team ], sizeof( level.voteDisplayString ),
                va( "^5 (Needs > %d%% of %s)", level.voteThreshold[ team ],
                g_impliedVoting.integer ? "active players" : "total votes" ) );

      Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
        "set g_nextMap \"\" ; set g_nextLayout \"%s\" ; set g_warmup \"1\" ;  %s", arg, vote );
      // map_restart comes with a default delay

      level.voteType[ team ] = MAP_RESTART_VOTE;
    }
    else if( !Q_stricmp( vote, "map" ) )
    {
      if( !G_MapExists( arg ) )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: 'maps/%s.bsp' could not be found on the server\n\"",
            cmd, arg ) );
        return;
      }

      if( extra[ 0 ] && !G_LayoutExists( arg, extra ) )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: layout '%s' does not exist for map '%s'\n\"",
            cmd, extra, arg ) );
        return;
      }

      Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
        "set g_nextMap \"\" ; set g_nextLayout \"%s\" ; set g_warmup \"1\"; %s \"%s\"", extra, vote, arg );
      Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
        "^4[^1Change to map^4] ^5 '%s'%s", arg, extra[ 0 ] ? va( " with layout '%s'", extra ) : "" );
      Q_strcat( level.voteDisplayString[ team ], sizeof( level.voteDisplayString ),
                va( "^5 (Needs > %d%% of %s)", level.voteThreshold[ team ],
                g_impliedVoting.integer ? "active players" : "total votes" ) );

      level.voteDelay[ team ] = 3000;

      level.voteType[ team ] = MAP_VOTE;
    }
    else if( !Q_stricmp( vote, "nextmap" ) )
    {
      if( G_MapExists( g_nextMap.string ) &&
          ( !g_nextLayout.string[ 0 ] || G_LayoutExists( g_nextMap.string, g_nextLayout.string ) ) )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: the next map is already set to '%s'%s\n\"",
            cmd, g_nextMap.string,
            g_nextLayout.string[ 0 ] ? va( " with layout '%s'", g_nextLayout.string ) : "" ) );
        return;
      }

      if( !G_MapExists( arg ) )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: 'maps/%s.bsp' could not be found on the server\n\"",
            cmd, arg ) );
        return;
      }

      if( extra[ 0 ] && !G_LayoutExists( arg, extra ) )
      {
        SV_GameSendServerCommand( ent-g_entities,
          va( "print \"%s: layout '%s' does not exist for map '%s'\n\"",
            cmd, extra, arg ) );
        return;
      }

      Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
        "set g_nextMap \"%s\" ; set g_nextLayout \"%s\"", arg, extra );
      Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
        "^4[^2Set the next map^4] ^5to '%s'%s", arg, extra[ 0 ] ? va( " with layout '%s'", extra ) : "" );
      Q_strcat( level.voteDisplayString[ team ], sizeof( level.voteDisplayString ),
                va( "^5 (Needs > %d%% of %s)", level.voteThreshold[ team ],
                g_impliedVoting.integer ? "active players" : "total votes" ) );

      level.voteType[ team ] = NEXTMAP_VOTE;
    }
    else if( !Q_stricmp( vote, "draw" ) )
    {
      strcpy( level.voteString[ team ], "evacuation" );
      strcpy( level.voteDisplayString[ team ], va( "^4[^1End match in a draw^4]^5" ) );
      Q_strcat( level.voteDisplayString[ team ], sizeof( level.voteDisplayString ),
                va( "^5 (Needs > %d%% of %s)", level.voteThreshold[ team ],
                g_impliedVoting.integer ? "active players" : "total votes" ) );
      level.voteDelay[ team ] = 3000;

      level.voteType[ team ] = DRAW_VOTE;
    }
    else if( !Q_stricmp( vote, "sudden_death" ) )
    {
      if(!g_suddenDeathVotePercent.integer)
      {
        SV_GameSendServerCommand( ent-g_entities,
              "print \"callvote: Sudden Death votes have been disabled\n\"" );
        return;
      }
      if(IS_WARMUP) {
        SV_GameSendServerCommand( ent-g_entities,
              "print \"callvote: Sudden Death votes can't be called during warmup\n\"" );
        return;
      }
      if( G_TimeTilSuddenDeath( ) <= 0 )
      {
        SV_GameSendServerCommand( ent - g_entities,
              va( "print \"callvote: Sudden Death has already begun\n\"") );
        return;
      }
      if(
        level.suddenDeathBeginTime > 0 &&
        G_TimeTilSuddenDeath() <=
          g_suddenDeathVoteDelay.integer * 1000 + VOTE_TIME)
      {
        SV_GameSendServerCommand( ent - g_entities,
              va( "print \"callvote: Sudden Death is imminent\n\"") );
        return;
      }
      level.voteThreshold[ team ] = g_suddenDeathVotePercent.integer;
      Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
        "suddendeath %d", g_suddenDeathVoteDelay.integer );
      Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                   "^4[^1Begin sudden death^4] ^5in %d seconds (Needs > %d%% of %s)",
                   g_suddenDeathVoteDelay.integer, g_suddenDeathVotePercent.integer,
                   g_impliedVoting.integer ? "active players" : "total votes" );

      level.voteType[ team ] = SUDDEN_DEATH_VOTE;
    }
    else if( !Q_stricmp( vote, "extend" ) )
    {
      if( !g_extendVotesPercent.integer )
      {
        SV_GameSendServerCommand( ent-g_entities, "print \"Extend votes have been disabled\n\"" );
        return;
      }
      if( g_extendVotesCount.integer
          && level.extendVoteCount >= g_extendVotesCount.integer )
      {
        SV_GameSendServerCommand( ent-g_entities,
                                va( "print \"callvote: Maximum number of %d extend votes has been reached\n\"",
                                    g_extendVotesCount.integer ) );
        return;
      }
      if( !G_Time_Limit() ) {
        SV_GameSendServerCommand( ent-g_entities,
                                "print \"This match has no timelimit so extend votes wont work\n\"" );
        return;
      }
      if( level.time - level.startTime <
          ( G_Time_Limit() - g_extendVotesTime.integer / 2 ) * 60000 )
      {
        SV_GameSendServerCommand( ent-g_entities,
                                va( "print \"callvote: Extend votes only allowed with less than %d minutes remaining\n\"",
                                    g_extendVotesTime.integer / 2 ) );
        return;
      }
      level.extendVoteCount++;
      level.voteThreshold[ team ] = g_extendVotesPercent.integer;
      Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
                   "extend %d", g_extendVotesTime.integer );
      Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                   "^4[^1Extend^4] ^5the timelimit by %d minutes (Needs > %d%% of %s)",
                   g_extendVotesTime.integer, g_extendVotesPercent.integer,
                   g_impliedVoting.integer ? "active players" : "total votes" );
    }
    else
    {
      SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
      SV_GameSendServerCommand( ent-g_entities, "print \"Valid vote commands "
          "are: map, nextmap, map_restart, draw, sudden_death, extend, spec, kick, poll, mute, "
          "unmute, and cancel\n" );
      return;
    }
  }
  else if( !Q_stricmp( vote, "denybuild" ) )
  {
    if( level.clients[ clientNum ].pers.namelog->denyBuild )
    {
      SV_GameSendServerCommand( ent-g_entities,
        va( "print \"%s: player already lost building rights\n\"", cmd ) );
      return;
    }

    Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
      "denybuild %d", id );
    Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
              "^4[^3Take away building rights^4] ^5from '%s' (Needs > %d%% of %s)", name, level.voteThreshold[ team ],
              g_impliedVoting.integer ? "active teammates" : "total votes" );
    if( reason[ 0 ] )
    {
      Q_strcat( level.voteDisplayString[ team ],
        sizeof( level.voteDisplayString[ team ] ), va( "%cfor '%s'", STRING_DELIMITER, reason ) );
    }
    level.voteType[ team ] = DENYBUILD_VOTE;
  }
  else if( !Q_stricmp( vote, "allowbuild" ) )
  {
    if( !level.clients[ clientNum ].pers.namelog->denyBuild )
    {
      SV_GameSendServerCommand( ent-g_entities,
        va( "print \"%s: player already has building rights\n\"", cmd ) );
      return;
    }

    Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
      "allowbuild %d", id );
    Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
              "^4[^3Give building rights^4] ^5to '%s' (Needs > %d%% of %s)", name, level.voteThreshold[ team ],
              g_impliedVoting.integer ? "active teammates" : "total votes" );

    level.voteType[ team ] = ALLOWBUILD_VOTE;
  }
  else if( !Q_stricmp( vote, "admitdefeat" ) )
  {
    if( IS_WARMUP )
    {
      SV_GameSendServerCommand( ent-g_entities,
        va( "print \"%s: admitdefeat cannot be called during warmup\n\"", cmd ) );
      return;
    }

    Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
      "admitdefeat %d", team );
    Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                 "^4[^1Admit Defeat^4] ^5 (Needs > %d%% of %s)", level.voteThreshold[ team ],
                 g_impliedVoting.integer ? "active teammates" : "total votes" );
    level.voteDelay[ team ] = 3000;

    level.voteType[ team ] = ADMITDEFEAT_VOTE;
  } else if( IS_SCRIM && !Q_stricmp(vote, "forfeit")) {
    const scrim_team_t scrim_team = BG_Scrim_Team_From_Playing_Team(&level.scrim, team);

    if(
      scrim_team == SCRIM_TEAM_NONE ||
      level.scrim.team[scrim_team].current_team == TEAM_NONE) {
      SV_GameSendServerCommand( ent-g_entities,
        va( "print \"%s: you are not on a valid team to call a forfeit vote\n\"", cmd ) );
      return;
    }

    Com_sprintf( level.voteString[ team ], sizeof( level.voteString[ team ] ),
      "forfeit %d", scrim_team );
    Com_sprintf( level.voteDisplayString[ team ], sizeof( level.voteDisplayString[ team ] ),
                 "^4[^1Forfeit Scrim^4] ^5 (Needs > %d%% of %s)", level.voteThreshold[ team ],
                 g_impliedVoting.integer ? "active teammates" : "total votes" );
  }
  else
  {
    SV_GameSendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
    SV_GameSendServerCommand( ent-g_entities, "print \"Valid team vote commands "
        "are: spec, poll, denybuild, allowbuild, admitdefeat and cancel\n" );
    return;
  }

  G_LogPrintf( "%s: %d \"%s" S_COLOR_WHITE "\": %s\n",
    team == TEAM_NONE ? "CallVote" : "CallTeamVote",
    (int)( ent - g_entities ), ent->client->pers.netname, level.voteString[ team ] );

  Q_cleanDelimitedString( cVoteDisplayString, level.voteDisplayString[ team ] );

  if( team == TEAM_NONE )
  {
    SV_GameSendServerCommand( -1, va( "print \"%s" S_COLOR_CYAN
      " called a vote: %s\n\"", ent->client->pers.netname,
      cVoteDisplayString ) );
  }
  else
  {
    int i;

    for( i = 0 ; i < level.maxclients ; i++ )
    {
      if( level.clients[ i ].pers.connected == CON_CONNECTED )
      {
        if( level.clients[ i ].pers.teamSelection == team ||
            ( level.clients[ i ].pers.teamSelection == TEAM_NONE &&
            G_admin_permission( &g_entities[ i ], ADMF_SPEC_ALLCHAT ) ) )
        {
          SV_GameSendServerCommand( i, va( "print \"%s" S_COLOR_CYAN
            " called a team vote: %s\n\"", ent->client->pers.netname,
            cVoteDisplayString ) );
        }
        else if( G_admin_permission( &g_entities[ i ], ADMF_ADMINCHAT ) )
        {
          SV_GameSendServerCommand( i, va( "chat -1 %d \"" S_COLOR_YELLOW "%s"
            S_COLOR_YELLOW " called a team vote (%ss): %s\"", SAY_ADMINS,
            ent->client->pers.netname, BG_Team( team )->name2,
            cVoteDisplayString ) );
        }
      }
    }
  }

  G_DecolorString( ent->client->pers.netname, caller, sizeof( caller ) );

  level.voteTime[ team ] = level.time;
  SV_SetConfigstring( CS_VOTE_TIME + team,
    va( "%d", level.voteTime[ team ] ) );
  SV_SetConfigstring( CS_VOTE_STRING + team,
    level.voteDisplayString[ team ] );
  SV_SetConfigstring( CS_VOTE_CALLER + team,
    caller );

  // issue a special message about mute and the virtues of /ignore
  if( !Q_stricmp( vote, "mute" ) )
    SV_GameSendServerCommand( ent-g_entities,
        va( "print \"" S_COLOR_RED "WARNING: " S_COLOR_WHITE
          "Muting other players will severely affect their ability to "
          "communicate with other players. If this is a personal dispute, "
          "consider using /ignore on the person. In order to cancel your "
          "vote, please use " S_COLOR_YELLOW "/%s cancel\n\"", cmd ) );

  // record the client that called this vote
  level.voteCaller[ team ] = ent->client;

  ent->client->pers.namelog->voteCount++;
  ent->client->pers.vote |= 1 << team;

  //during scrims, only players on teams can vote
  if(IS_SCRIM && ent->client->pers.teamSelection == TEAM_NONE) {
    voteYes = qfalse;
  }

  if ( voteYes == qtrue )
    G_Vote( ent, team, qtrue );   //Caller calls vote YES as default but not in all cases.
}

/*
==================
Cmd_Ready_f
==================
*/
void Cmd_Ready_f( gentity_t *ent )
{
  char      cmd[ MAX_TOKEN_CHARS ];
  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );

  // do not allow /ready command if not in warmup
  if( !IS_WARMUP )
  {
    SV_GameSendServerCommand( ent-g_entities,
      va( "print \"%s: game is no longer in warmup\n\"", cmd ) );
    return;
  }else if( g_doWarmupCountdown.integer && level.countdownTime )
  {
    SV_GameSendServerCommand( ent-g_entities,
      va( "print \"%s: ready or not, warmup is ending\n\"", cmd ) );
    return;
  }

  // update client readiness
  ent->client->sess.readyToPlay = !ent->client->sess.readyToPlay;
  if( ent->client->sess.readyToPlay ) {
    ent->client->ps.stats[ STAT_FLAGS ] |= SFL_READY;
  } else {
    ent->client->ps.stats[ STAT_FLAGS ] &= ~SFL_READY;
  }

  // let people see when player changes their ready status
  SV_GameSendServerCommand( -1, va( "print \"^7Warmup: %s %sready^7.\n",
                                  ent->client->pers.netname,
                                  ( ent->client->sess.readyToPlay ? "^2is " : "^1is no longer " ) ) );

  // Check if conditions are met to start the game
  G_LevelReady();
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent )
{
  char cmd[ MAX_TOKEN_CHARS ], vote[ MAX_TOKEN_CHARS ];
  team_t team = ent->client->pers.teamSelection;

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );
  if( Q_stricmp( cmd, "teamvote" ) )
    team = TEAM_NONE;

  if( IS_SCRIM && ent->client->pers.teamSelection == TEAM_NONE ) {
    SV_GameSendServerCommand( ent-g_entities,
      va( "print \"%s: You must be on a team to vote while a scrim is ^1on^7\n\"", cmd ) );
    return;
  }

  if( !level.voteTime[ team ] )
  {
    SV_GameSendServerCommand( ent-g_entities,
      va( "print \"%s: no vote in progress\n\"", cmd ) );
    return;
  }

  if( ent->client->pers.voted & ( 1 << team ) )
  {
    SV_GameSendServerCommand( ent-g_entities,
      va( "print \"%s: vote already cast\n\"", cmd ) );
    return;
  }

  SV_GameSendServerCommand( ent-g_entities,
    va( "print \"%s: vote cast\n\"", cmd ) );

  Cmd_ArgvBuffer( 1, vote, sizeof( vote ) );
  if( vote[ 0 ] == 'y' )
    ent->client->pers.vote |= 1 << team;
  else
    ent->client->pers.vote &= ~( 1 << team );
  G_Vote( ent, team, qtrue );
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent )
{
  vec3_t  origin, angles;
  char    buffer[ MAX_TOKEN_CHARS ];
  int     i;

  if( Cmd_Argc( ) < 4 )
  {
    SV_GameSendServerCommand( ent-g_entities, "print \"usage: setviewpos <x> <y> <z> [<yaw> [<pitch>]]\n\"" );
    return;
  }

  for( i = 0; i < 3; i++ )
  {
    Cmd_ArgvBuffer( i + 1, buffer, sizeof( buffer ) );
    origin[ i ] = atof( buffer );
  }
  origin[ 2 ] -= ent->client->ps.viewheight;

  VectorCopy( ent->client->ps.viewangles, angles );
  angles[ ROLL ] = 0;

  if( Cmd_Argc() >= 5 ) {
    Cmd_ArgvBuffer( 4, buffer, sizeof( buffer ) );
    angles[ YAW ] = atof( buffer );
    if( Cmd_Argc() >= 6 ) {
      Cmd_ArgvBuffer( 5, buffer, sizeof( buffer ) );
      angles[ PITCH ] = atof( buffer );
    }
  }

  TeleportPlayer( ent, origin, angles, 0.0f );
}

#define AS_OVER_RT3         ((ALIENSENSE_RANGE*0.5f)/M_ROOT3)

qboolean G_RoomForClassChange( gentity_t *ent, class_t class,
                               vec3_t newOrigin )
{
  vec3_t    fromMins, fromMaxs;
  vec3_t    toMins, toMaxs;
  vec3_t    temp;
  trace_t   tr;
  float     nudgeHeight;
  float     maxHorizGrowth;
  class_t   oldClass = ent->client->ps.stats[ STAT_CLASS ];

  BG_ClassBoundingBox( oldClass, fromMins, fromMaxs, NULL, NULL, NULL );
  BG_ClassBoundingBox( class, toMins, toMaxs, NULL, NULL, NULL );

  VectorCopy( ent->client->ps.origin, newOrigin );

  // find max x/y diff
  maxHorizGrowth = toMaxs[ 0 ] - fromMaxs[ 0 ];
  if( toMaxs[ 1 ] - fromMaxs[ 1 ] > maxHorizGrowth )
    maxHorizGrowth = toMaxs[ 1 ] - fromMaxs[ 1 ];
  if( toMins[ 0 ] - fromMins[ 0 ] > -maxHorizGrowth )
    maxHorizGrowth = -( toMins[ 0 ] - fromMins[ 0 ] );
  if( toMins[ 1 ] - fromMins[ 1 ] > -maxHorizGrowth )
    maxHorizGrowth = -( toMins[ 1 ] - fromMins[ 1 ] );

  if( maxHorizGrowth > 0.0f )
  {
    // test by moving the player up the max required on a 60 degree slope
    nudgeHeight = maxHorizGrowth * 2.0f;
  }
  else
  {
    // player is shrinking, so there's no need to nudge them upwards
    nudgeHeight = 0.0f;
  }

  // find what the new origin would be on a level surface
  newOrigin[ 2 ] -= toMins[ 2 ] - fromMins[ 2 ];

  if( ent->client->noclip )
    return qtrue;

  //compute a place up in the air to start the real trace
  VectorCopy( newOrigin, temp );
  temp[ 2 ] += nudgeHeight;
  SV_Trace( &tr, newOrigin, toMins, toMaxs, temp, ent->s.number,
    MASK_PLAYERSOLID, TT_AABB );

  //trace down to the ground so that we can evolve on slopes
  VectorCopy( newOrigin, temp );
  temp[ 2 ] += ( nudgeHeight * tr.fraction );
  SV_Trace( &tr, temp, toMins, toMaxs, newOrigin, ent->s.number,
    MASK_PLAYERSOLID, TT_AABB );
  VectorCopy( tr.endpos, newOrigin );

  //make REALLY sure
  SV_Trace( &tr, newOrigin, toMins, toMaxs, newOrigin,
    ent->s.number, MASK_PLAYERSOLID, TT_AABB );

  //check there is room to evolve
  return ( !tr.startsolid && tr.fraction == 1.0f );
}

/*
=================
G_CheckEvolve

Checks to see if evolving to a given
class is allowed for a given entity.
Returns the cost of evolving if evolving is allowed and
        evaluates infestOrigin.
Returns a negative int is evolving is not allowed.
=================
*/
int G_CheckEvolve( gentity_t *ent, class_t newClass,
                  vec3_t infestOrigin, qboolean give )
{
  int       clientNum;
  int       i;
  int       cost = -1;
  class_t   currentClass;
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { AS_OVER_RT3, AS_OVER_RT3, AS_OVER_RT3 };
  vec3_t    mins, maxs;
  int       num;
  gentity_t *other;

  Com_Assert( ent->client &&
              "G_CheckEvolve: Attempted to check a non-client entity for evolving" );

  clientNum = ent->client - level.clients;

  currentClass = ent->client->pers.classSelection;

  if( ent->client->sess.spectatorState != SPECTATOR_NOT )
    return -1;

  if( ent->health <= 0 )
    return -1;

  if( ent->client->pers.teamSelection == TEAM_ALIENS ||
      ( give && BG_Class( newClass )->team == TEAM_ALIENS ) )
  {
    if( newClass <= PCL_NONE ||
        newClass >= PCL_NUM_CLASSES )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_A_UNKNOWNCLASS );
      return -1;
    }

    //if we are not currently spectating, we are attempting evolution
    if( ent->client->pers.classSelection != PCL_NONE )
    {
      //check that we have an overmind
      if( !G_Overmind( ) && !give )
      {
        G_TriggerMenu( clientNum, MN_A_NOOVMND_EVOLVE );
        return -1;
      }

      //check there are no humans nearby
      VectorAdd( ent->client->ps.origin, range, maxs );
      VectorSubtract( ent->client->ps.origin, range, mins );

      num = SV_AreaEntities( mins, maxs, entityList, MAX_GENTITIES );
      for( i = 0; i < num; i++ )
      {
        other = &g_entities[ entityList[ i ] ];

        if( ( ( other->client && other->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ) ||
              ( other->s.eType == ET_BUILDABLE && other->buildableTeam == TEAM_HUMANS &&
                other->powered ) ) && !give )
        {
          G_TriggerMenu( clientNum, MN_A_TOOCLOSE );
          return -1;
        }
      }

      if ( ent->client->ps.eFlags & EF_OCCUPYING )
      {
        G_TriggerMenu( clientNum, MN_A_NOEROOM );
        return -1;
      }

      if( ent->client->sess.spectatorState == SPECTATOR_NOT &&
          !give &&
          ( currentClass == PCL_ALIEN_BUILDER0 ||
            currentClass == PCL_ALIEN_BUILDER0_UPG ) &&
          ent->client->ps.misc[ MISC_MISC ] > 0 )
      {
        G_TriggerMenu( ent->client->ps.clientNum, MN_A_EVOLVEBUILDTIMER );
        return -1;
      }

      if( give )
        cost = 0;
      else
      {
        cost = BG_ClassCanEvolveFromTo(
          currentClass, newClass, ent->client->pers.credit, g_alienStage.integer,
          0, IS_WARMUP, g_cheats.integer,
          (ent->client->ps.stats[STAT_FLAGS] & SFL_CLASS_FORCED));
      }

      if( G_RoomForClassChange( ent, newClass, infestOrigin ) )
      {
        if( cost < 0 )
        {
          G_TriggerMenuArgs( clientNum, MN_A_CANTEVOLVE, newClass );
          return -1;
        }
      }
      else
      {
        vec3_t playerNormal;

        BG_GetClientNormal( &ent->client->ps, playerNormal );
        //check that wallwalking isn't interfering
        if( ent->client->ps.eFlags & EF_WALLCLIMB &&
            ( playerNormal[ 2 ] != 1.0f ) )
        {
          G_TriggerMenu( clientNum, MN_A_EVOLVEWALLWALK );
          return -1;
        }
        else
        {
          G_TriggerMenu( clientNum, MN_A_NOEROOM );
          return -1;
        }
      }
    }
  }
  else if( ent->client->pers.teamSelection == TEAM_HUMANS ||
      ( give && BG_Class( newClass )->team == TEAM_HUMANS ) )
  {
    if( give )
      cost = 0;
    else
    {
      G_TriggerMenu( clientNum, MN_H_DEADTOCLASS );
      return -1;
    }
  }

  return cost;
}

/*
=================
G_Evolve

Evolves a given client entity to a given new class,
given also the cost and infestOrigin.
=================
*/
void G_Evolve( gentity_t *ent, class_t newClass,
               int cost, vec3_t infestOrigin, qboolean force )
{
  class_t   currentClass;
  int       clientNum;
  vec3_t    oldVel;
  int       oldBoostTime = -1;

  Com_Assert( ent->client &&
              "G_Evolve: Attempted to evolve a non-client entity" );

  clientNum = ent->client - level.clients;

  currentClass = ent->client->pers.classSelection;

  //disable wallwalking
  if( ent->client->ps.eFlags & EF_WALLCLIMB )
  {
    vec3_t newAngles;

    ent->client->ps.eFlags &= ~EF_WALLCLIMB;
    VectorCopy( ent->client->ps.viewangles, newAngles );
    newAngles[ PITCH ] = 0;
    newAngles[ ROLL ] = 0;
    G_SetClientViewAngle( ent, newAngles );
  }

  ent->client->pers.evolveHealthFraction = (float)ent->client->ps.misc[ MISC_HEALTH ] /
    (float)BG_Class( currentClass )->health;

  if( ent->client->pers.evolveHealthFraction < 0.0f )
    ent->client->pers.evolveHealthFraction = 0.0f;
  else if( ent->client->pers.evolveHealthFraction > 1.0f )
    ent->client->pers.evolveHealthFraction = 1.0f;

  ent->client->pers.evolveChargeStaminaFraction =
                  (float)ent->client->ps.misc[MISC_CHARGE_STAMINA] /
                  (float)
                    (
                      BG_Class( currentClass )->chargeStaminaMax /
                      BG_Class( currentClass )->chargeStaminaRestoreRate);

  if( ent->client->pers.evolveChargeStaminaFraction < 0.0f )
    ent->client->pers.evolveChargeStaminaFraction = 0.0f;
  else if( ent->client->pers.evolveChargeStaminaFraction > 1.0f )
    ent->client->pers.evolveChargeStaminaFraction = 1.0f;

  //remove credit
  G_AddCreditToClient( ent->client, -cost, qtrue );
  ent->client->pers.classSelection = newClass;
  ClientUserinfoChanged( clientNum, qfalse );
  VectorCopy( infestOrigin, ent->s.pos.trBase );
  VectorCopy( ent->client->ps.velocity, oldVel );

  if( ent->client->ps.stats[ STAT_STATE ] & SS_BOOSTED )
    oldBoostTime = ent->client->boostedTime;

  ClientSpawn( ent, ent, ent->s.pos.trBase, ent->s.apos.trBase, qtrue );

  VectorCopy( oldVel, ent->client->ps.velocity );
  if( oldBoostTime > 0 )
  {
    ent->client->boostedTime = oldBoostTime;
    ent->client->ps.stats[ STAT_STATE ] |= SS_BOOSTED;
  }

  if(force) {
    ent->client->ps.stats[STAT_FLAGS] |= SFL_CLASS_FORCED;
  }
}

/*
=================
G_EvolveAfterCheck

Executes G_CheckEvolve(), and uses its results for
G_Evolve()
=================
*/
qboolean G_EvolveAfterCheck( gentity_t *ent, class_t newClass, qboolean give )
{
  int cost;
  vec3_t    infestOrigin;

  cost = G_CheckEvolve( ent, newClass, infestOrigin, give );

  if( cost >= 0 ) {
    G_Evolve( ent, newClass, cost, infestOrigin, qfalse );
    return qtrue;
  } else {
    return qfalse;
  }
}

/*
=================
Cmd_Class_f
=================
*/
void Cmd_Class_f( gentity_t *ent )
{
  char      s[ MAX_TOKEN_CHARS ];
  int       clientNum;
  class_t   newClass;

  clientNum = ent->client - level.clients;
  Cmd_ArgvBuffer( 1, s, sizeof( s ) );
  newClass = BG_ClassByName( s )->number;

  if( ent->client->sess.spectatorState != SPECTATOR_NOT )
  {
    if( ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
      G_StopFollowing( ent );
    if( ent->client->pers.teamSelection == TEAM_ALIENS )
    {
      if( newClass != PCL_ALIEN_BUILDER0 &&
          newClass != PCL_ALIEN_BUILDER0_UPG &&
          newClass != PCL_ALIEN_LEVEL0 )
      {
        G_TriggerMenuArgs( ent->client->ps.clientNum, MN_A_CLASSNOTSPAWN, newClass );
        return;
      }

      if( !BG_ClassIsAllowed( newClass, g_cheats.integer ) )
      {
        G_TriggerMenuArgs( ent->client->ps.clientNum, MN_A_CLASSNOTALLOWED, newClass );
        return;
      }

      if( !BG_ClassAllowedInStage( newClass, g_alienStage.integer, IS_WARMUP ) )
      {
        G_TriggerMenuArgs( ent->client->ps.clientNum, MN_A_CLASSNOTATSTAGE, newClass );
        return;
      }

      // spawn from an egg
      if( G_PushSpawnQueue( &level.alienSpawnQueue, clientNum ) )
      {
        ent->client->pers.classSelection = newClass;
        ent->client->ps.stats[ STAT_CLASS ] = newClass;
      }
    }
    else if( ent->client->pers.teamSelection == TEAM_HUMANS )
    {
      //set the item to spawn with
      if( !Q_stricmp( s, BG_Weapon( WP_MACHINEGUN )->name ) &&
          BG_WeaponIsAllowed( WP_MACHINEGUN, g_cheats.integer ) )
      {
        ent->client->pers.humanItemSelection = WP_MACHINEGUN;
      }
      else if( !Q_stricmp( s, BG_Weapon( WP_HBUILD )->name ) &&
               BG_WeaponIsAllowed( WP_HBUILD, g_cheats.integer ) )
      {
        ent->client->pers.humanItemSelection = WP_HBUILD;
      }
      else
      {
        G_TriggerMenu( ent->client->ps.clientNum, MN_H_UNKNOWNSPAWNITEM );
        return;
      }
      // spawn from a telenode
      if( G_PushSpawnQueue( &level.humanSpawnQueue, clientNum ) )
      {
        ent->client->pers.classSelection = PCL_HUMAN;
        ent->client->ps.stats[ STAT_CLASS ] = PCL_HUMAN;
      }
    }
    return;
  }

  if(G_EvolveAfterCheck( ent, newClass, qfalse )) {
    // end damage protection early
    ent->dmgProtectionTime = 0;
  }
}


/*
=================
Cmd_Destroy_f
=================
*/
void Cmd_Destroy_f( gentity_t *ent )
{
  vec3_t      viewOrigin, forward, end;
  trace_t     tr;
  gentity_t   *traceEnt;
  char        cmd[ 12 ];
  qboolean    deconstruct = qtrue;
  qboolean    lastSpawn = qfalse;

  if( ent->client->pers.namelog->denyBuild )
  {
    G_TriggerMenu( ent->client->ps.clientNum, MN_B_REVOKED );
    return;
  }

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );
  if( Q_stricmp( cmd, "destroy" ) == 0 )
    deconstruct = qfalse;

  BG_GetClientViewOrigin( &ent->client->ps, viewOrigin );
  AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
  VectorMA( viewOrigin, 100, forward, end );

  SV_Trace( &tr, viewOrigin, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID,
    TT_AABB);
  traceEnt = &g_entities[ tr.entityNum ];

  if( tr.fraction < 1.0f &&
      ( traceEnt->s.eType == ET_BUILDABLE ) &&
      ( traceEnt->buildableTeam == ent->client->pers.teamSelection ) &&
      ( ( ent->client->ps.weapon >= WP_ABUILD ) &&
        ( ent->client->ps.weapon <= WP_HBUILD ) ) )
  {
    // Always let the builder prevent the explosion
    if( traceEnt->health <= 0 )
    {
      G_QueueBuildPoints( traceEnt );
      G_RewardAttackers( traceEnt );
      G_FreeEntity( traceEnt );
      return;
    }

    // Prevent destruction of the last spawn
    if( ent->client->pers.teamSelection == TEAM_ALIENS &&
        traceEnt->s.modelindex == BA_A_SPAWN )
    {
      if( level.numAlienSpawns <= 1 )
        lastSpawn = qtrue;
    }
    else if( ent->client->pers.teamSelection == TEAM_HUMANS &&
             traceEnt->s.modelindex == BA_H_SPAWN )
    {
      if( level.numHumanSpawns <= 1 )
        lastSpawn = qtrue;
    }

    if( lastSpawn && !g_cheats.integer )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_LASTSPAWN );
      return;
    }

    // Don't allow destruction of buildables that cannot be rebuilt
    if( !IS_WARMUP && G_TimeTilSuddenDeath( ) <= 0 ) {
      if(G_SD_Mode( ) == SDMODE_SELECTIVE) {
        if(!level.sudden_death_replacable[traceEnt->s.modelindex]) {
          G_TriggerMenu(ent->client->ps.clientNum, MN_B_SD_NODECON);
          return;
        } else if(!G_BuildableIsUnique(traceEnt)) {
          G_TriggerMenu(ent->client->ps.clientNum, MN_B_SD_NODECON);
          return;
        }
      } else if(
        G_SD_Mode( ) != SDMODE_BP ||
        BG_Buildable(traceEnt->s.modelindex)->buildPoints){
        G_TriggerMenu(ent->client->ps.clientNum, MN_B_SD_NODECON);
        return;
      }
    }

    if( IS_WARMUP || ( ent->client->pers.teamSelection == TEAM_HUMANS &&
          !G_FindPower( traceEnt, qtrue ) ) )
    {
      if( ent->client->ps.misc[ MISC_MISC ] > 0 )
      {
        G_AddEvent( ent, EV_BUILD_DELAY, ent->client->ps.clientNum );
        return;
      }
    }

    if( traceEnt->health > 0 )
    {
      if( !deconstruct )
      {
        G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                  0, DAMAGE_INSTAGIB, MOD_SUICIDE );
      }
      else
      {
        if( ent->client->ps.misc[ MISC_MISC ] > 0 )
        {
          G_AddEvent( ent, EV_BUILD_DELAY, ent->client->ps.clientNum );
          return;
        }

        if( !g_cheats.integer && !IS_WARMUP ) // add a bit to the build timer
        {
            ent->client->ps.misc[ MISC_MISC ] +=
              BG_Buildable( traceEnt->s.modelindex )->buildTime / 4;
        }
        G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                  0, DAMAGE_INSTAGIB, MOD_DECONSTRUCT );
        G_RemoveRangeMarkerFrom( traceEnt );
        G_FreeEntity( traceEnt );
      }
    }
  }
}

/*
=================
Cmd_ActivateItem_f

Activate an item
=================
*/
void Cmd_ActivateItem_f( gentity_t *ent )
{
  char  s[ MAX_TOKEN_CHARS ];
  int   upgrade, weapon;

  Cmd_ArgvBuffer( 1, s, sizeof( s ) );

  // "weapon" aliased to whatever weapon you have
  if( !Q_stricmp( "weapon", s ) )
  {
    if( ent->client->ps.weapon == WP_BLASTER &&
        BG_PlayerCanChangeWeapon( &ent->client->ps, &ent->client->pmext ) )
      G_ForceWeaponChange( ent, WP_NONE );
    return;
  }

  upgrade = BG_UpgradeByName( s )->number;
  weapon = BG_WeaponByName( s )->number;

  if( upgrade != UP_NONE && BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
    BG_ActivateUpgrade( upgrade, ent->client->ps.stats );
  else if( weapon != WP_NONE &&
           BG_InventoryContainsWeapon( weapon, ent->client->ps.stats ) )
  {
    if( ent->client->ps.weapon != weapon &&
        BG_PlayerCanChangeWeapon( &ent->client->ps, &ent->client->pmext ) )
      G_ForceWeaponChange( ent, weapon );
  }
  else
    SV_GameSendServerCommand( ent-g_entities, va( "print \"You don't have the %s\n\"", s ) );
}


/*
=================
Cmd_DeActivateItem_f

Deactivate an item
=================
*/
void Cmd_DeActivateItem_f( gentity_t *ent )
{
  char      s[ MAX_TOKEN_CHARS ];
  upgrade_t upgrade;

  Cmd_ArgvBuffer( 1, s, sizeof( s ) );
  upgrade = BG_UpgradeByName( s )->number;

  if( BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
    BG_DeactivateUpgrade( upgrade, ent->client->ps.stats );
  else
    SV_GameSendServerCommand( ent-g_entities, va( "print \"You don't have the %s\n\"", s ) );
}


/*
=================
Cmd_ToggleItem_f
=================
*/
void Cmd_ToggleItem_f( gentity_t *ent )
{
  char      s[ MAX_TOKEN_CHARS ];
  weapon_t  weapon;
  upgrade_t upgrade;

  Cmd_ArgvBuffer( 1, s, sizeof( s ) );
  upgrade = BG_UpgradeByName( s )->number;
  weapon = BG_WeaponByName( s )->number;

  if( weapon != WP_NONE )
  {
    if( !BG_PlayerCanChangeWeapon( &ent->client->ps, &ent->client->pmext ) )
      return;

    //special case to allow switching between
    //the blaster and the primary weapon
    if( ent->client->ps.weapon != WP_BLASTER )
      weapon = WP_BLASTER;
    else
      weapon = WP_NONE;

    G_ForceWeaponChange( ent, weapon );
  }
  else if( BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
  {
    if( BG_UpgradeIsActive( upgrade, ent->client->ps.stats ) )
      BG_DeactivateUpgrade( upgrade, ent->client->ps.stats );
    else
      BG_ActivateUpgrade( upgrade, ent->client->ps.stats );
  }
  else
    SV_GameSendServerCommand( ent-g_entities, va( "print \"You don't have the %s\n\"", s ) );
}

/*
=================
G_CanSellUpgrade
=================
*/
static sellErr_t G_CanSellUpgrade(
  gentity_t *ent, const upgrade_t upgrade, int *value, qboolean force) {
  if(force || (IS_WARMUP && BG_Upgrade(upgrade)->warmupFree)) {
    *value = 0;
  }
  else {
    *value = BG_Upgrade(upgrade)->price;
  }

  //are we /allowed/ to sell this?
  if( !BG_Upgrade(upgrade)->purchasable && !force) {
    return ERR_Sell_NOT_PURCHASABLE;
  }

  //remove upgrade if carried
  if(BG_InventoryContainsUpgrade(upgrade, ent->client->ps.stats)) {
    // shouldn't really need to test for this, but just to be safe
    if(upgrade == UP_BATTLESUIT) {
      vec3_t newOrigin;

      if(!G_RoomForClassChange(ent, PCL_HUMAN, newOrigin)) {
        return ERR_SELL_NOROOMBSUITOFF;
      }
    }
  } else {
    return ERR_SELL_NOT_ITEM_HELD;
  }

  return ERR_SELL_NONE;
}

/*
=================
G_CanSell
=================
*/
sellErr_t G_CanSell(gentity_t *ent, const char *itemName, int *value, qboolean force)
{
  int       i;
  weapon_t  weapon;
  upgrade_t upgrade;
  

  Com_Assert( ent &&
              "G_CanSell: ent is NULL" );
  Com_Assert( value &&
              "G_CanSell: value is NULL" );

  *value = 0;

  if( !itemName )
    return ERR_SELL_SPECIFY;

  if(!force) {
    if( ent->client->ps.eFlags & EF_OCCUPYING )
      return ERR_SELL_OCCUPY;

    //no armoury nearby
    if( !G_BuildableRange( ent->client->ps.origin, 100, BA_H_ARMOURY ) )
      return ERR_SELL_NO_ARM;
  }

  if( !Q_stricmpn( itemName, "weapon", 6 ) )
    weapon = ent->client->ps.stats[ STAT_WEAPON ];
  else
    weapon = BG_WeaponByName( itemName )->number;

  upgrade = BG_UpgradeByName( itemName )->number;

  if( weapon != WP_NONE )
  {
    if( force || (IS_WARMUP && BG_Weapon( weapon )->warmupFree) )
    {
      *value = 0;
    } else
    {
      *value = BG_Weapon( weapon )->price;
      if( BG_Weapon( weapon )->roundPrice && !BG_Weapon( weapon )->usesEnergy &&
          ( BG_Weapon( weapon )->ammoPurchasable ||
            ( ent->client->ps.ammo == BG_Weapon( weapon )->maxAmmo &&
              ent->client->ps.clips == BG_Weapon( weapon )->maxClips ) ) )
      {
        int totalAmmo = ent->client->ps.ammo +
                        ( ent->client->ps.clips *
                          BG_Weapon( weapon )->maxAmmo );

        *value += ( totalAmmo * BG_Weapon( weapon )->roundPrice);
      }
    }

    if(!force) {
      if( !BG_PlayerCanChangeWeapon( &ent->client->ps, &ent->client->pmext ) )
        return ERR_SELL_NO_CHANGE_ALLOWED_NOW;

      //are we /allowed/ to sell this?
      if( !BG_Weapon( weapon )->purchasable )
        return ERR_Sell_NOT_PURCHASABLE;
    }

    //remove weapon if carried
    if( BG_InventoryContainsWeapon( weapon, ent->client->ps.stats ) )
    {
      //guard against selling the HBUILD weapons exploit
      if( !force && weapon == WP_HBUILD && ent->client->ps.misc[ MISC_MISC ] > 0 ) {
        return ERR_SELL_ARMOURYBUILDTIMER;
      }
    } else
      return ERR_SELL_NOT_ITEM_HELD;
  }
  else if( upgrade != UP_NONE )
  {
    sellErr_t sell_err = G_CanSellUpgrade(ent, upgrade, value, force);

    if(sell_err != ERR_SELL_NONE) {
      return sell_err;
    }
  }
  else if( !Q_stricmp( itemName, "upgrades" ) )
  {
    for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
    {
      int       upgrade_value;
      sellErr_t sell_err = G_CanSellUpgrade(ent, i, &upgrade_value, force);

      if(sell_err == ERR_SELL_NONE) {
        *value += upgrade_value;
      }
    }
  }
  else
    return ERR_SELL_UNKNOWNITEM;

  return ERR_SELL_NONE;
}

/*
=================
G_SellErrHandling
=================
*/
void G_SellErrHandling( gentity_t *ent, const sellErr_t error )
{
  switch ( error )
  {
    case ERR_SELL_SPECIFY:
      SV_GameSendServerCommand( ent-g_entities, "print \"Specify an item to sell\n\"" );
      break;

    case ERR_SELL_OCCUPY:
      SV_GameSendServerCommand( ent-g_entities, "print \"You can't sell while occupying another structure\n\"" );
      break;

    case ERR_SELL_NO_ARM:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOARMOURYHERE );
      break;

    case ERR_SELL_NO_CHANGE_ALLOWED_NOW:
      break;

    case ERR_Sell_NOT_PURCHASABLE:
      SV_GameSendServerCommand( ent-g_entities, "print \"This item has a no return policy at this establishment\n\"" );
      break;

    case ERR_SELL_NOT_ITEM_HELD:
      break;

    case ERR_SELL_ARMOURYBUILDTIMER:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_ARMOURYBUILDTIMER );
      break;

    case ERR_SELL_NOROOMBSUITOFF:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOROOMBSUITOFF );
      break;

    case ERR_SELL_UNKNOWNITEM:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_UNKNOWNITEM );
      break;

    default:
      break;
  }
}

/*
=================
G_TakeUpgrade
=================
*/
static void G_TakeUpgrade(
  gentity_t *ent, const upgrade_t upgrade, qboolean force) {
  if(upgrade == UP_BATTLESUIT) {
    vec3_t newOrigin;

    G_RoomForClassChange(ent, PCL_HUMAN_BSUIT, newOrigin);
    VectorCopy( newOrigin, ent->client->ps.origin );
    ent->client->ps.stats[STAT_CLASS] = PCL_HUMAN;
    ent->client->pers.classSelection = PCL_HUMAN;
    ent->client->ps.eFlags ^= EF_TELEPORT_BIT;
  }

  //remove from inventory
  BG_RemoveUpgradeFromInventory(upgrade, ent->client->ps.stats);

  if(upgrade == UP_BATTPACK) {
    G_GiveClientMaxAmmo(ent, qtrue);
  }
}

/*
=================
G_TakeItem
=================
*/
void G_TakeItem(
  gentity_t *ent, const char *itemName, const int value, qboolean force) {
  int       i;
  weapon_t  weapon;
  upgrade_t upgrade;

  Com_Assert( ent &&
              "G_TakeItem: ent is NULL" );
  Com_Assert( itemName &&
              "G_TakeItem: itemName is NULL" );

  if( !Q_stricmpn( itemName, "weapon", 6 ) )
    weapon = ent->client->ps.stats[ STAT_WEAPON ];
  else
    weapon = BG_WeaponByName( itemName )->number;

  upgrade = BG_UpgradeByName( itemName )->number;

  if( weapon != WP_NONE )
  {
    weapon_t selected = BG_GetPlayerWeapon( &ent->client->ps );

      ent->client->ps.stats[ STAT_WEAPON ] = WP_NONE;
      // Cancel ghost buildables
      ent->client->ps.stats[ STAT_BUILDABLE ] = BA_NONE;

      //add to funds
      G_AddCreditToClient( ent->client, (short)value, qfalse );

    //if we have this weapon selected, force a new selection
    if( weapon == selected )
      G_ForceWeaponChange( ent, WP_NONE );
  }
  else if( upgrade != UP_NONE )
  {
      G_TakeUpgrade(ent, upgrade, force);

      //add to funds
      G_AddCreditToClient( ent->client, (short)value, qfalse );
  }
  else if( !Q_stricmp( itemName, "upgrades" ) )
  {
    for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
    {
      int       dummy_value;

      if(G_CanSellUpgrade(ent, i, &dummy_value, force) != ERR_SELL_NONE) {
        continue;
      }

      G_TakeUpgrade(ent, i, force);
    }

    //add to funds
    G_AddCreditToClient( ent->client, (short)value, qfalse );
  }

  //update ClientInfo
  ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );
  ent->client->pers.infoChangeTime = level.time;
}

/*
=================
G_TakeItemAfterCheck
=================
*/
qboolean G_TakeItemAfterCheck(gentity_t *ent, const char *itemName, qboolean force)
{
  int  value;
  sellErr_t sellErr;

  sellErr = G_CanSell( ent, itemName, &value, force );

  if( sellErr != ERR_SELL_NONE )
  {
    G_SellErrHandling( ent, sellErr );
    return qfalse;
  }

  G_TakeItem( ent, itemName, value, force );
  return qtrue;
}

/*
=================
Cmd_Sell_f
=================
*/
void Cmd_Sell_f( gentity_t *ent )
{
    char s[ MAX_TOKEN_CHARS ];

    Cmd_ArgvBuffer( 1, s, sizeof( s ) );

    G_TakeItemAfterCheck(ent, s, qfalse);
}

/*
=================
G_CanAutoSell

For automatically selling items that are
taking up slots for another item that will
be bought
=================
*/
int G_CanAutoSell( gentity_t *ent, const char *itemToBuyName,
                        weapon_t *weaponToSell, int *upgradesToSell,
                        int *values, int numValues, sellErr_t *autoSellErrors, qboolean force )
{
  int       i;
  int       currentValue[numValues];
  int       slotsToCheck = 0;
  int       slotsCanFree = 0;
  int       upgradesToSellTemp = 0;
  weapon_t  weaponToSellTemp = WP_NONE;
  weapon_t  weaponToBuy;
  upgrade_t upgradeToBuy;

  Com_Assert( ent &&
              "G_CanAutoSell: ent is NULL" );
  Com_Assert( ent->client &&
              "G_CanAutoSell: ent->client is NULL" );
  Com_Assert( weaponToSell &&
              "G_CanAutoSell: weaponToSell is NULL" );
  Com_Assert( upgradesToSell &&
              "G_CanAutoSell: upgradesToSell is NULL" );
  Com_Assert( values &&
              "G_CanAutoSell: values is NULL" );
  Com_Assert( numValues == ( UP_NUM_UPGRADES + 1 ) &&
              "G_CanAutoSell: numValues is invalid" );
  Com_Assert( autoSellErrors &&
              "G_CanAutoSell: autoSellErrors is NULL" );

  *weaponToSell = WP_NONE;
  *upgradesToSell = 0;
  for( i = 0; i < numValues; i++ )
  {
    currentValue[i] = values[i] = 0;
    autoSellErrors[i] = ERR_SELL_NONE;
  }

  if( !itemToBuyName )
    return 0;

  weaponToBuy = BG_WeaponByName( itemToBuyName )->number;
  upgradeToBuy = BG_UpgradeByName( itemToBuyName )->number;

  //check if any slots need to be freed
  if( weaponToBuy != WP_NONE )
    slotsToCheck = BG_Weapon( weaponToBuy )->slots;
  else if( upgradeToBuy )
    slotsToCheck = BG_Upgrade( upgradeToBuy )->slots;

  slotsToCheck &= BG_SlotsForInventory( ent->client->ps.stats );

  //nothing to do
  if( !slotsToCheck )
    return qfalse;

  if( slotsToCheck & BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->slots )
  {
    int value;

    autoSellErrors[ UP_NUM_UPGRADES ] = G_CanSell( ent,
                                                   BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->name,
                                                   &value, force );

    if( autoSellErrors[ UP_NUM_UPGRADES ] == ERR_SELL_NONE )
    {
      weaponToSellTemp = ent->client->ps.stats[ STAT_WEAPON ];
      currentValue[UP_NUM_UPGRADES] += value;
      slotsCanFree |= BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->slots;
      slotsToCheck &= ~BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->slots;
    } else
      return 0;
  }

  if( slotsToCheck )
  {
    int u;

    for( u = ( UP_NONE + 1 ); u < UP_NUM_UPGRADES; u++ )
    {
      int value;

      if( !BG_InventoryContainsUpgrade( u, &ent->client->ps.stats ) )
        continue;

      //check if this upgrade is blocking
      if( !( slotsToCheck & BG_Upgrade( u )->slots ) )
        continue;

      autoSellErrors[ u ] = G_CanSell( ent,
                                       BG_Upgrade( u )->name,
                                       &value, force );

      if( autoSellErrors[ u ] == ERR_SELL_NONE )
      {
        upgradesToSellTemp |= ( 1 << u );
        currentValue[u] += value;
        slotsCanFree |= BG_Upgrade( u )->slots;
        slotsToCheck &= ~BG_Upgrade( u )->slots;
      } else
        return 0;
        

      //no remaining blocked slots
      if( !slotsToCheck )
        break;
    }
  }

  //if attempting to buy a non-energy weapon, there is no need to keep a
  //battery pack, however, if for some reason a battery pack can't be sold,
  //don't make its sale necessary.
  if(
    !force && weaponToBuy != WP_NONE && !BG_Weapon(weaponToBuy)->usesEnergy &&
    BG_InventoryContainsUpgrade(UP_BATTPACK, ent->client->ps.stats) &&
    !(upgradesToSellTemp & (1 << UP_BATTPACK))) {
    int value;

    autoSellErrors[ UP_BATTPACK ] = G_CanSell( ent,
                                     BG_Upgrade( UP_BATTPACK )->name,
                                     &value, force );

    if( autoSellErrors[ UP_BATTPACK ] == ERR_SELL_NONE )
    {
      upgradesToSellTemp |= ( 1 << UP_BATTPACK );
      currentValue[UP_BATTPACK] += value;
      slotsCanFree |= BG_Upgrade( UP_BATTPACK )->slots;
      slotsToCheck &= ~BG_Upgrade( UP_BATTPACK )->slots;
    }
  }

  //check if all blocked slots necessary for the purchase can be freed
  if( slotsToCheck )
    return 0;

  //set the output values
  *weaponToSell = weaponToSellTemp;
  *upgradesToSell = upgradesToSellTemp;
  for( i = 0; i < numValues; i++ )
  {
    values[i] = currentValue[i];
  }

  return slotsCanFree;
}

/*
=================
G_CanBuy
=================
*/
buyErr_t G_CanBuy( gentity_t *ent, const char *itemName, int *price,
                   qboolean *energyOnly, const int slotsFreeFromAutoSell,
                   const int fundsFromAutoSell, qboolean force )
{
  weapon_t  weapon;
  upgrade_t upgrade;

  Com_Assert( ent &&
              "G_CanBuy: ent is NULL" );
  Com_Assert( price &&
              "G_CanBuy: price is NULL" );
  Com_Assert( energyOnly &&
              "G_CanBuy: energyOnly is NULL" );

  *price = -1;

  if( !itemName )
    return ERR_BUY_SPECIFY;

  if( (ent->client->ps.eFlags & EF_OCCUPYING) && !force )
    return ERR_BUY_OCCUPY;

  weapon = BG_WeaponByName( itemName )->number;
  upgrade = BG_UpgradeByName( itemName )->number;

  // Only give energy from reactors or repeaters
  if( G_BuildableRange( ent->client->ps.origin, 100, BA_H_ARMOURY ) || force )
    *energyOnly = qfalse;
  else if( upgrade == UP_AMMO &&
           BG_Weapon( ent->client->ps.stats[ STAT_WEAPON ] )->usesEnergy &&
           ( G_BuildableRange( ent->client->ps.origin, 100, BA_H_REACTOR ) ||
             G_BuildableRange( ent->client->ps.origin, 100, BA_H_REPEATER ) ) )
    *energyOnly = qtrue;
  else
  {
    if( upgrade == UP_AMMO &&
        BG_Weapon( ent->client->ps.weapon )->usesEnergy )
      return ERR_BUY_NO_ENERGY_AMMO;
    else
      return ERR_BUY_NO_ARM;
  }

  if( weapon != WP_NONE )
  {
    if((IS_WARMUP && BG_Upgrade( upgrade )->warmupFree) || force) {
      *price = 0;
    } else {
      *price = BG_TotalPriceForWeapon( weapon, IS_WARMUP );
    }

    //already got this?
    if( BG_InventoryContainsWeapon( weapon, ent->client->ps.stats ) )
      return ERR_BUY_ITEM_HELD;

    // Only humans can buy stuff
    if( BG_Weapon( weapon )->team != TEAM_HUMANS )
      return ERR_BUY_ALIEN_ITEM;

    if(!force) {
      //are we /allowed/ to buy this?
      if( !BG_Weapon( weapon )->purchasable )
        return ERR_BUY_NOT_PURCHASABLE;

      //are we /allowed/ to buy this?
      if( !BG_WeaponAllowedInStage( weapon, g_humanStage.integer, IS_WARMUP ) ||
                                    !BG_WeaponIsAllowed( weapon, g_cheats.integer ) )
        return ERR_BUY_NOT_ALLOWED;

      //can afford this?
      if( *price > (short)(ent->client->pers.credit + fundsFromAutoSell) )
        return ERR_BUY_NO_FUNDS;

      // In some instances, weapons can't be changed
      if( !BG_PlayerCanChangeWeapon( &ent->client->ps, &ent->client->pmext ) )
        return ERR_BUY_NO_CHANGE_ALLOWED_NOW;
    }

    //have space to carry this?
    if( BG_Weapon( weapon )->slots & ( BG_SlotsForInventory( ent->client->ps.stats ) & ~slotsFreeFromAutoSell ) )
      return ERR_BUY_NO_SLOTS;
  }
  else if( upgrade != UP_NONE )
  {
    // Only humans can buy stuff
    if( BG_Upgrade( upgrade )->team != TEAM_HUMANS )
      return ERR_BUY_ALIEN_ITEM;

    if( (IS_WARMUP && BG_Upgrade( upgrade )->warmupFree) || force )
      *price = 0;
    else
      *price = BG_Upgrade( upgrade )->price;

    // If buying grenades, do not allow if player still has unexploded grenades
    // (basenade protection)
    if( upgrade == UP_GRENADE && G_PlayerHasUnexplodedGrenades( ent ) && !force )
      return ERR_BUY_UNEXPLODEDGRENADE;

    //already got this?
    if( BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
      return ERR_BUY_ITEM_HELD;

    //can afford this?
    if( *price > (short)(ent->client->pers.credit + fundsFromAutoSell) && !force )
      return ERR_BUY_NO_FUNDS;

    //have space to carry this?
    if( BG_Upgrade( upgrade )->slots & ( BG_SlotsForInventory( ent->client->ps.stats ) & ~slotsFreeFromAutoSell ) )
      return ERR_BUY_NO_SLOTS;

    if(!force) {
      //are we /allowed/ to buy this?
      if( !BG_Upgrade( upgrade )->purchasable )
        return ERR_BUY_NOT_PURCHASABLE;

      //are we /allowed/ to buy this?
      if( !BG_UpgradeAllowedInStage( upgrade, g_humanStage.integer, IS_WARMUP ) ||
                                     !BG_UpgradeIsAllowed( upgrade, g_cheats.integer ) )
        return ERR_BUY_NOT_ALLOWED;
    }

    if( upgrade == UP_AMMO )
    {
      int rounds, clips, ammoPrice;

      //bursts must complete
      if( ent->client->pmext.burstRoundsToFire[ 2 ] ||
          ent->client->pmext.burstRoundsToFire[ 1 ] ||
          ent->client->pmext.burstRoundsToFire[ 0 ] )
        return ERR_BUY_BURST_UNFINISHED;

      if( ent->client->ps.weapon == WP_LIGHTNING &&
      ( ent->client->ps.misc[ MISC_MISC ] > LIGHTNING_BOLT_CHARGE_TIME_MIN ||
        ( ent->client->ps.stats[ STAT_STATE ] & SS_CHARGING ) ) )
        return ERR_BUY_CHARGING_LIGHTNING;

      if( !G_CanGiveClientMaxAmmo( ent, *energyOnly,
                                   &rounds, &clips, &ammoPrice ) )
        return ERR_BUY_NO_MORE_AMMO;
    }
    else
    {
      if( upgrade == UP_BATTLESUIT )
      {
        vec3_t newOrigin;

        if( !G_RoomForClassChange( ent, PCL_HUMAN_BSUIT, newOrigin ) )
          return ERR_BUY_NOROOMBSUITON;
      }
    }
  }
  else
    return ERR_BUY_UNKNOWNITEM;

  return ERR_BUY_NONE;
}

/*
=================
G_BuyErrHandling
=================
*/
static void G_BuyErrHandling( gentity_t *ent, const buyErr_t error )
{
  switch ( error )
  {
    case ERR_BUY_SPECIFY:
      SV_GameSendServerCommand( ent-g_entities, "print \"Specify an item to purchase\n\"" );
      break;

    case ERR_BUY_OCCUPY:
      SV_GameSendServerCommand( ent-g_entities, "print \"You can't buy while occupying another structure\n\"" );
      break;

    case ERR_BUY_NO_ENERGY_AMMO:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOENERGYAMMOHERE );
      break;

    case ERR_BUY_NO_ARM:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOARMOURYHERE );
      break;

    case ERR_BUY_ITEM_HELD:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_ITEMHELD );
      break;

    case ERR_BUY_ALIEN_ITEM:
      SV_GameSendServerCommand( ent-g_entities, "print \"You can't buy alien items\n\"" );
      break;

    case ERR_BUY_NOT_PURCHASABLE:
      SV_GameSendServerCommand( ent-g_entities, "print \"You can't buy this item\n\"" );
      break;

    case ERR_BUY_NOT_ALLOWED:
      SV_GameSendServerCommand( ent-g_entities, "print \"You can't buy this item\n\"" );
      break;

    case ERR_BUY_NO_FUNDS:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOFUNDS );
      break;

    case ERR_BUY_NO_SLOTS:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOSLOTS );
      break;

    case ERR_BUY_NO_CHANGE_ALLOWED_NOW:
      break;

    case ERR_BUY_UNEXPLODEDGRENADE:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_UNEXPLODEDGRENADE );
      break;

    case ERR_BUY_BURST_UNFINISHED:
      break;

    case ERR_BUY_CHARGING_LIGHTNING:
      break;

    case ERR_BUY_NO_MORE_AMMO:
      break;

    case ERR_BUY_NOROOMBSUITON:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOROOMBSUITON );
      break;

    case ERR_BUY_UNKNOWNITEM:
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_UNKNOWNITEM );
      break;

    default:
      break;
  }
}

/*
=================
G_GiveItem
=================
*/
void G_GiveItem( gentity_t *ent, const char *itemName, const int price,
                 const qboolean energyOnly, qboolean force )
{
  weapon_t  weapon;
  upgrade_t upgrade;

  Com_Assert( ent &&
              "G_GiveItem: ent is NULL" );
  Com_Assert( itemName &&
              "G_GiveItem: itemName is NULL" );

  weapon = BG_WeaponByName( itemName )->number;
  upgrade = BG_UpgradeByName( itemName )->number;

  if( weapon != WP_NONE )
  {
    ent->client->ps.stats[ STAT_WEAPON ] = weapon;
    ent->client->ps.ammo = BG_Weapon( weapon )->maxAmmo;
    ent->client->ps.clips = BG_Weapon( weapon )->maxClips;

    if( BG_Weapon( weapon )->usesEnergy &&
        BG_InventoryContainsUpgrade( UP_BATTPACK, ent->client->ps.stats ) )
      ent->client->ps.ammo *= BATTPACK_MODIFIER;

    G_ForceWeaponChange( ent, weapon );

    //set build delay/pounce etc to 0
    ent->client->ps.misc[ MISC_MISC ] = 0;

    //subtract from funds
    G_AddCreditToClient( ent->client, -(short)price,
                         qfalse );
  }
  else if( upgrade != UP_NONE )
  {
    if( upgrade == UP_AMMO )
    {
      G_GiveClientMaxAmmo( ent, energyOnly );
    }
    else
    {
      if( upgrade == UP_BATTLESUIT )
      {
        vec3_t newOrigin;

        G_RoomForClassChange( ent, PCL_HUMAN_BSUIT, newOrigin );
        VectorCopy( newOrigin, ent->client->ps.origin );
        ent->client->ps.stats[ STAT_CLASS ] = PCL_HUMAN_BSUIT;
        ent->client->pers.classSelection = PCL_HUMAN_BSUIT;
        ent->client->ps.eFlags ^= EF_TELEPORT_BIT;
      }

      //add to inventory
      BG_AddUpgradeToInventory( upgrade, ent->client->ps.stats );
    }

    if( upgrade == UP_BATTPACK )
      G_GiveClientMaxAmmo( ent, qtrue );

    //subtract from funds
    G_AddCreditToClient( ent->client, -(short)price, qfalse );
  }

  //update ClientInfo
  ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );
  ent->client->pers.infoChangeTime = level.time;
}

/*
=================
G_GiveItemAfterCheck
=================
*/
qboolean G_GiveItemAfterCheck(
  gentity_t *ent, const char *itemName, qboolean force, qboolean autosell) {
  int  price;
  int  slotsFreeFromAutoSell = 0;
  int  upgradesToAutoSell = 0;
  int  fundsFromAutoSell[UP_NUM_UPGRADES + 1];
  int  totafundsFromAutoSell = 0;
  int  i;
  weapon_t weaponToAutoSell = WP_NONE;
  sellErr_t autoSellErrors[UP_NUM_UPGRADES + 1];
  buyErr_t buyErr;
  qboolean  energyOnly = qfalse;

  if(autosell) {
    //attempt to automatically sell the current weapon
    slotsFreeFromAutoSell = G_CanAutoSell( ent, itemName, &weaponToAutoSell,
                                           &upgradesToAutoSell, fundsFromAutoSell,
                                           ARRAY_LEN( fundsFromAutoSell ),
                                           autoSellErrors, force );

    for( i = 0; i < ARRAY_LEN( fundsFromAutoSell ); i++ )
    {
      if( autoSellErrors[i] != ERR_SELL_NONE ) {
         G_SellErrHandling( ent, autoSellErrors[i] );
      }

      totafundsFromAutoSell += fundsFromAutoSell[ i ];
    }
  }
   

   buyErr = G_CanBuy( ent, itemName, &price, &energyOnly, slotsFreeFromAutoSell,
                      totafundsFromAutoSell, force );

   if( buyErr != ERR_BUY_NONE )
   {
     G_BuyErrHandling( ent, buyErr );
     return qfalse;
   }

   if( weaponToAutoSell != WP_NONE )
     G_TakeItem( ent, BG_Weapon( weaponToAutoSell )->name,
                 fundsFromAutoSell[ UP_NUM_UPGRADES ], force );

   if( upgradesToAutoSell )
   {
     for( i = 0; i < UP_NUM_UPGRADES; i++ )
     {
       if( !( upgradesToAutoSell & ( 1 << i ) ) )
         continue;

       G_TakeItem( ent, BG_Upgrade( i )->name, fundsFromAutoSell[ i ], force );
     }
   }

   G_GiveItem( ent, itemName, price, energyOnly, force );
   return qtrue;
}

/*
=================
Cmd_Buy_f
=================
*/
void Cmd_Buy_f( gentity_t *ent )
{
  char s[ MAX_TOKEN_CHARS ];
  char cmd[ MAX_TOKEN_CHARS ];

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );
  Cmd_ArgvBuffer( 1, s, sizeof( s ) );

  if( !Q_stricmp( cmd, "autosellbuy" ) ) {
    if(G_GiveItemAfterCheck(ent, s, qfalse, qtrue)) {
      // end damage protection early
      ent->dmgProtectionTime = 0;
    }
  } else {
    if(G_GiveItemAfterCheck(ent, s, qfalse, qfalse)) {
      // end damage protection early
      ent->dmgProtectionTime = 0;
    }
  }
}


/*
=================
Cmd_Build_f
=================
*/
void Cmd_Build_f( gentity_t *ent )
{
  char          s[ MAX_TOKEN_CHARS ];
  buildable_t   buildable;
  float         dist;
  vec3_t        origin, normal;
  int           groundEntNum;
  team_t        team;

  if( ent->client->pers.namelog->denyBuild )
  {
    G_TriggerMenu( ent->client->ps.clientNum, MN_B_REVOKED );
    return;
  }

  if( ent->client->pers.teamSelection == level.surrenderTeam )
  {
    G_TriggerMenu( ent->client->ps.clientNum, MN_B_SURRENDER );
    return;
  }

  Cmd_ArgvBuffer( 1, s, sizeof( s ) );

  buildable = BG_BuildableByName( s )->number;
  team = ent->client->ps.stats[ STAT_TEAM ];

  if( buildable == BA_NONE || !BG_BuildableIsAllowed( buildable, g_cheats.integer ) ||
      !( ( 1 << ent->client->ps.weapon ) & BG_Buildable( buildable )->buildWeapon ) ||
      ( team == TEAM_ALIENS && !BG_BuildableAllowedInStage( buildable, g_alienStage.integer, IS_WARMUP ) ) ||
      ( team == TEAM_HUMANS && !BG_BuildableAllowedInStage( buildable, g_humanStage.integer, IS_WARMUP ) ) )
  {
    G_TriggerMenu( ent->client->ps.clientNum, MN_B_CANNOT );
    return;
  }

  if(
    !IS_WARMUP && G_TimeTilSuddenDeath( ) <= 0 &&
    G_SD_Mode( ) == SDMODE_NO_BUILD)
  {
    G_TriggerMenu( ent->client->ps.clientNum, MN_B_SUDDENDEATH );
    return;
  }

  ent->client->ps.stats[ STAT_BUILDABLE ] = buildable;

  dist = BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->buildDist;

  //these are the errors displayed when the builder first selects something to use
  switch( G_CanBuild( ent, buildable, dist, origin, normal, &groundEntNum ) )
  {
    // can place right away, set the blueprint and the valid togglebit
    case IBE_NONE:
    case IBE_TNODEWARN:
    case IBE_RPTNOREAC:
    case IBE_RPTPOWERHERE:
    case IBE_SPWNWARN:
      ent->client->ps.stats[ STAT_BUILDABLE ] |= SB_VALID_TOGGLEBIT;
      break;

    // can't place yet but maybe soon: start with valid togglebit off
    default:
      break;
  }
}

/*
=================
Cmd_Reload_f
=================
*/
void Cmd_Reload_f( gentity_t *ent )
{
  playerState_t *ps = &ent->client->ps;

  if( ps->stats[ STAT_TEAM ] == TEAM_HUMANS &&
      ps->weapon != WP_HBUILD )
  {
    // reload is being attempted
    int ammo;

    // weapon doesn't ever need reloading
    if( BG_Weapon( ps->weapon )->infiniteAmmo )
      return;

    if( ps->clips <= 0 )
      return;

    if( BG_Weapon( ps->weapon )->usesEnergy &&
        BG_InventoryContainsUpgrade( UP_BATTPACK, ps->stats ) )
      ammo = BG_Weapon( ps->weapon )->maxAmmo * BATTPACK_MODIFIER;
    else
      ammo = BG_Weapon( ps->weapon )->maxAmmo;

    // don't reload when full
    if( ps->ammo >= ammo )
      return;

    // the animation, ammo refilling etc. is handled by PM_Weapon
    if( ent->client->ps.weaponstate != WEAPON_RELOADING )
      ent->client->ps.pm_flags |= PMF_WEAPON_RELOAD;
  } else if( g_markDeconstruct.integer )
  {
    // mark buildables for deconstruction is being attempted
    vec3_t      viewOrigin, forward, end;
    trace_t     tr;
    gentity_t   *traceEnt;
    qboolean    lastSpawn = qfalse;

    if( ent->client->pers.namelog->denyBuild &&
        ( ps->weapon == WP_HBUILD ||
          ps->weapon == WP_ABUILD ||
          ps->weapon == WP_ABUILD2 ) )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_B_REVOKED );
      return;
    }

    BG_GetClientViewOrigin( &ent->client->ps, viewOrigin );
    AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
    VectorMA( viewOrigin, 100, forward, end );

    SV_Trace( &tr, viewOrigin, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID,
      TT_AABB );
    traceEnt = &g_entities[ tr.entityNum ];

    if( tr.fraction < 1.0f &&
        ( traceEnt->s.eType == ET_BUILDABLE ) &&
        ( traceEnt->buildableTeam == ent->client->pers.teamSelection ) &&
        ( ( ent->client->ps.weapon >= WP_ABUILD ) &&
          ( ent->client->ps.weapon <= WP_HBUILD ) ) )
    {
      // Cancel deconstruction (unmark)
      if( !( IS_WARMUP && g_warmupBuildableRespawning.integer ) &&
          traceEnt->deconstruct )
      {
        traceEnt->deconstruct = qfalse;
        return;
      }

      // determine if the team has one or less spawns left
      if( ent->client->pers.teamSelection == TEAM_ALIENS &&
          traceEnt->s.modelindex == BA_A_SPAWN )
      {
        if( level.numAlienSpawns <= 1 )
          lastSpawn = qtrue;
      }
      else if( ent->client->pers.teamSelection == TEAM_HUMANS &&
               traceEnt->s.modelindex == BA_H_SPAWN )
      {
        if( level.numHumanSpawns <= 1 )
          lastSpawn = qtrue;
      }


      // Don't allow destruction of buildables that cannot be rebuilt
      if( !IS_WARMUP && G_TimeTilSuddenDeath( ) <= 0 ) {
        if(G_SD_Mode( ) == SDMODE_SELECTIVE) {
          if(!level.sudden_death_replacable[traceEnt->s.modelindex]) {
            G_TriggerMenu(ent->client->ps.clientNum, MN_B_SD_NODECON);
            return;
          } else if(!G_BuildableIsUnique(traceEnt)) {
            G_TriggerMenu(ent->client->ps.clientNum, MN_B_SD_NODECON);
            return;
          }
        } else if(
          G_SD_Mode( ) != SDMODE_BP ||
          BG_Buildable(traceEnt->s.modelindex)->buildPoints){
          G_TriggerMenu(ent->client->ps.clientNum, MN_B_SD_NODECON);
          return;
        }
      }

      if( traceEnt->health > 0 )
      {
        if( !( IS_WARMUP && g_warmupBuildableRespawning.integer ) &&
             ( ent->client->pers.teamSelection != TEAM_HUMANS ||
               G_FindPower( traceEnt , qtrue ) || lastSpawn ) )
        {
          traceEnt->deconstruct     = qtrue; // Mark buildable for deconstruction
          traceEnt->deconstructTime = level.time;
        }
      }
    }
  }
}

/*
=================
G_StopFromFollowing

stops any other clients from following this one
called when a player leaves a team or dies
=================
*/
void G_StopFromFollowing( gentity_t *ent )
{
  int i;

  for( i = 0; i < level.maxclients; i++ )
  {
    if( level.clients[ i ].sess.spectatorState == SPECTATOR_FOLLOW &&
        level.clients[ i ].sess.spectatorClient == ent->client->ps.clientNum )
    {
      if( !G_FollowNewClient( &g_entities[ i ], 1 ) )
        G_StopFollowing( &g_entities[ i ] );
    }
  }
}

/*
=================
G_StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void G_StopFollowing( gentity_t *ent )
{
  ent->client->ps.stats[ STAT_TEAM ] = ent->client->pers.teamSelection;

  if( ent->client->pers.teamSelection == TEAM_NONE )
  {
    ent->client->sess.spectatorState =
      ent->client->ps.persistant[ PERS_SPECSTATE ] = SPECTATOR_FREE;
  }
  else
  {
    vec3_t spawn_origin, spawn_angles;

    ent->client->sess.spectatorState =
      ent->client->ps.persistant[ PERS_SPECSTATE ] = SPECTATOR_LOCKED;

    if( ent->client->pers.teamSelection == TEAM_ALIENS )
      G_SelectAlienLockSpawnPoint( spawn_origin, spawn_angles );
    else if( ent->client->pers.teamSelection == TEAM_HUMANS )
      G_SelectHumanLockSpawnPoint( spawn_origin, spawn_angles );

    G_SetOrigin( ent, spawn_origin );
    VectorCopy( spawn_origin, ent->client->ps.origin );
    G_SetClientViewAngle( ent, spawn_angles );
  }
  ent->client->sess.spectatorClient = -1;
  ent->client->ps.pm_flags &= ~PMF_FOLLOW;
  ent->client->ps.groundEntityNum = ENTITYNUM_NONE;
  ent->client->ps.stats[ STAT_STATE ] = 0;
  ent->client->ps.stats[ STAT_VIEWLOCK ] = 0;
  ent->client->ps.eFlags &= ~( EF_WALLCLIMB | EF_WALLCLIMBCEILING );
  ent->client->ps.clientNum = ent - g_entities;
  ent->client->ps.persistant[ PERS_CREDIT ] = ent->client->pers.credit;

  if( ent->client->pers.teamSelection == TEAM_NONE )
  {
    vec3_t viewOrigin, angles;

    BG_GetClientViewOrigin( &ent->client->ps, viewOrigin );
    VectorCopy( ent->client->ps.viewangles, angles );
    angles[ ROLL ] = 0;
    TeleportPlayer( ent, viewOrigin, angles, qfalse );
  }

  CalculateRanks(qtrue);
}

/*
=================
G_FollowLockView

Client is still following a player, but that player has gone to spectator
mode and cannot be followed for the moment
=================
*/
void G_FollowLockView( gentity_t *ent )
{
  vec3_t spawn_origin, spawn_angles;
  int clientNum;

  clientNum = ent->client->sess.spectatorClient;
  ent->client->sess.spectatorState =
    ent->client->ps.persistant[ PERS_SPECSTATE ] = SPECTATOR_FOLLOW;
  ent->client->ps.clientNum = clientNum;
  ent->client->ps.pm_flags &= ~PMF_FOLLOW;
  ent->client->ps.stats[ STAT_TEAM ] = ent->client->pers.teamSelection;
  ent->client->ps.stats[ STAT_STATE ] &= ~SS_WALLCLIMBING;
  ent->client->ps.stats[ STAT_VIEWLOCK ] = 0;
  ent->client->ps.eFlags &= ~( EF_WALLCLIMB | EF_WALLCLIMBCEILING );
  ent->client->ps.eFlags ^= EF_TELEPORT_BIT;
  ent->client->ps.viewangles[ PITCH ] = 0.0f;

  // Put the view at the team spectator lock position
  if( level.clients[ clientNum ].pers.teamSelection == TEAM_ALIENS )
    G_SelectAlienLockSpawnPoint( spawn_origin, spawn_angles );
  else if( level.clients[ clientNum ].pers.teamSelection == TEAM_HUMANS )
    G_SelectHumanLockSpawnPoint( spawn_origin, spawn_angles );

  G_SetOrigin( ent, spawn_origin );
  VectorCopy( spawn_origin, ent->client->ps.origin );
  G_SetClientViewAngle( ent, spawn_angles );
}

/*
=================
G_FollowNewClient

This was a really nice, elegant function. Then I fucked it up.
=================
*/
qboolean G_FollowNewClient( gentity_t *ent, int dir )
{
  int       clientnum = ent->client->sess.spectatorClient;
  int       original = clientnum;
  qboolean  selectAny = qfalse;

  if( dir > 1 )
    dir = 1;
  else if( dir < -1 )
    dir = -1;
  else if( dir == 0 )
    return qtrue;

  if( ent->client->sess.spectatorState == SPECTATOR_NOT )
    return qfalse;

  // select any if no target exists
  if( clientnum < 0 || clientnum >= level.maxclients )
  {
    clientnum = original = 0;
    selectAny = qtrue;
  }

  do
  {
    clientnum += dir;

    if( clientnum >= level.maxclients )
      clientnum = 0;

    if( clientnum < 0 )
      clientnum = level.maxclients - 1;

    // can't follow self
    if( &g_entities[ clientnum ] == ent )
      continue;

    // avoid selecting existing follow target
    if( clientnum == original && !selectAny )
      continue; //effectively break;

    // can only follow connected clients
    if( level.clients[ clientnum ].pers.connected != CON_CONNECTED )
      continue;

    // can't follow a spectator
    if( level.clients[ clientnum ].pers.teamSelection == TEAM_NONE )
      continue;

    // if stickyspec is disabled, can't follow someone in queue either
    if( !ent->client->pers.stickySpec &&
        level.clients[ clientnum ].sess.spectatorState != SPECTATOR_NOT )
      continue;

    // can only follow teammates when dead and on a team
    if( ent->client->pers.teamSelection != TEAM_NONE &&
        ( level.clients[ clientnum ].pers.teamSelection !=
          ent->client->pers.teamSelection ) )
      continue;

    // this is good, we can use it
    ent->client->sess.spectatorClient = clientnum;
    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;

    // if this client is in the spawn queue, we need to do something special
    if( level.clients[ clientnum ].sess.spectatorState != SPECTATOR_NOT )
      G_FollowLockView( ent );

    return qtrue;

  } while( clientnum != original );

  return qfalse;
}

/*
=================
G_ToggleFollow
=================
*/
void G_ToggleFollow( gentity_t *ent )
{
  if( ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
    G_StopFollowing( ent );
  else
    G_FollowNewClient( ent, 1 );
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent )
{
  int   i;
  char  arg[ MAX_NAME_LENGTH ];

  // won't work unless spectating
  if( ent->client->sess.spectatorState == SPECTATOR_NOT )
    return;

  if( Cmd_Argc( ) != 2 )
  {
    G_ToggleFollow( ent );
  }
  else
  {
    char err[ MAX_STRING_CHARS ];
    Cmd_ArgvBuffer( 1, arg, sizeof( arg ) );

    i = G_ClientNumberFromString( arg, err, sizeof( err ) );

    if( i == -1 )
    {
      SV_GameSendServerCommand( ent - g_entities,
        va( "print \"follow: %s\"", err ) );
      return;
    }

    // can't follow self
    if( &level.clients[ i ] == ent->client )
      return;

    // can't follow another spectator if sticky spec is off
    if( !ent->client->pers.stickySpec &&
        level.clients[ i ].sess.spectatorState != SPECTATOR_NOT )
      return;

    // if not on team spectator, you can only follow teammates
    if( ent->client->pers.teamSelection != TEAM_NONE &&
        ( level.clients[ i ].pers.teamSelection !=
          ent->client->pers.teamSelection ) )
      return;

    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    ent->client->sess.spectatorClient = i;
  }
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent )
{
  char args[ 11 ];
  int  dir = 1;

  Cmd_ArgvBuffer( 0, args, sizeof( args ) );
  if( Q_stricmp( args, "followprev" ) == 0 )
    dir = -1;

  // won't work unless spectating
  if( ent->client->sess.spectatorState == SPECTATOR_NOT )
    return;

  G_FollowNewClient( ent, dir );
}

static void Cmd_Ignore_f( gentity_t *ent )
{
  int pids[ MAX_CLIENTS ];
  char name[ MAX_NAME_LENGTH ];
  char cmd[ 9 ];
  int matches = 0;
  int i;
  qboolean ignore = qfalse;

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );
  if( Q_stricmp( cmd, "ignore" ) == 0 )
    ignore = qtrue;

  if( Cmd_Argc() < 2 )
  {
    SV_GameSendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
      "usage: %s [clientNum | partial name match]\n\"", cmd ) );
    return;
  }

  Q_strncpyz( name, ConcatArgs( 1 ), sizeof( name ) );
  matches = G_ClientNumbersFromString( name, pids, MAX_CLIENTS );
  if( matches < 1 )
  {
    SV_GameSendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
      "%s: no clients match the name '%s'\n\"", cmd, name ) );
    return;
  }

  for( i = 0; i < matches; i++ )
  {
    if( ignore )
    {
      if( !Com_ClientListContains( &ent->client->sess.ignoreList, pids[ i ] ) )
      {
        Com_ClientListAdd( &ent->client->sess.ignoreList, pids[ i ] );
        ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );
        SV_GameSendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "ignore: added %s^7 to your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
      else
      {
        SV_GameSendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "ignore: %s^7 is already on your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
    }
    else
    {
      if( Com_ClientListContains( &ent->client->sess.ignoreList, pids[ i ] ) )
      {
        Com_ClientListRemove( &ent->client->sess.ignoreList, pids[ i ] );
        ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );
        SV_GameSendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "unignore: removed %s^7 from your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
      else
      {
        SV_GameSendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "unignore: %s^7 is not on your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
    }
  }
}

/*
=================
Cmd_ListMaps_f

List all maps on the server
=================
*/

static int SortStrings( const void *a, const void *b )
{
  return strcmp( *(char **)a, *(char **)b );
}

void Cmd_ListMaps_f( gentity_t *ent )
{
  char search[ 16 ] = {""};
  char fileList[ 4096 ] = {""};
  char *fileSort[ MAX_MAPLIST_MAPS ];
  char *filePtr, *p;
  int  numFiles;
  int  fileLen = 0;
  int  shown = 0;
  int  count = 0;
  int  page = 0;
  int  pages;
  int  row, rows;
  int  start, i, j;

  if( Cmd_Argc( ) > 1 )
  {
    Cmd_ArgvBuffer( 1, search, sizeof( search ) );
    for( p = search; ( *p ) && isdigit( *p ); p++ );
    if( !( *p ) )
    {
      page = atoi( search );
      search[ 0 ] = '\0';
    }
    else if( Cmd_Argc( ) > 2 )
    {
      char lp[ 8 ];
      Cmd_ArgvBuffer( 2, lp, sizeof( lp ) );
      page = atoi( lp );
    }

    if( page > 0 )
      page--;
    else if( page < 0 )
      page = 0;
  }

  numFiles = FS_GetFileList( "maps/", ".bsp",
                                  fileList, sizeof( fileList ) );
  filePtr = fileList;
  for( i = 0; i < numFiles && count < MAX_MAPLIST_MAPS; i++, filePtr += fileLen + 1 )
  {
    fileLen = strlen( filePtr );
    if ( fileLen < 5 )
      continue;

    filePtr[ fileLen - 4 ] = '\0';

    if( search[ 0 ] && !strstr( filePtr, search ) )
      continue;

    fileSort[ count ] = filePtr;
    count++;
  }
  qsort( fileSort, count, sizeof( fileSort[ 0 ] ), SortStrings );

  rows = ( count + 2 ) / 3;
  pages = MAX( 1, ( rows + MAX_MAPLIST_ROWS - 1 ) / MAX_MAPLIST_ROWS );
  if( page >= pages )
    page = pages - 1;

  start = page * MAX_MAPLIST_ROWS * 3;
  if( count < start + ( 3 * MAX_MAPLIST_ROWS ) )
    rows = ( count - start + 2 ) / 3;
  else
    rows = MAX_MAPLIST_ROWS;

  ADMBP_begin( );
  for( row = 0; row < rows; row++ )
  {
    for( i = start + row, j = 0; i < count && j < 3; i += rows, j++ )
    {
      ADMBP( va( "^7 %-20s", fileSort[ i ] ) );
      shown++;
    }
    ADMBP( "\n" );
  }
  if ( search[ 0 ] )
    ADMBP( va( "^3listmaps: ^7found %d maps matching '%s^7'", count, search ) );
  else
    ADMBP( va( "^3listmaps: ^7listing %d of %d maps", shown, count ) );
  if( pages > 1 )
    ADMBP( va( ", page %d of %d", page + 1, pages ) );
  if( page + 1 < pages )
    ADMBP( va( ", use 'listmaps %s%s%d' to see more",
               search, ( search[ 0 ] ) ? " ": "", page + 2 ) );
  ADMBP( ".\n" );
  ADMBP_end( );
}

/*
=================
Cmd_ListModels_f

List all the available player models installed on the server.
=================
*/
void Cmd_ListModels_f( gentity_t *ent )
{
    int i;

    ADMBP_begin();
    for (i = 0; i < level.playerModelCount; i++)
    {
        ADMBP(va("%d - %s\n", i+1, level.playerModel[i]));
    }
    ADMBP(va("^3listmodels: ^7showing %d player models\n", level.playerModelCount));
    ADMBP_end();

}

/*
=================
Cmd_ListSkins_f
=================
*/
void Cmd_ListSkins_f( gentity_t *ent )
{
    char modelname[ 64 ];
    char skins[ MAX_PLAYER_MODEL ][ 64 ];
    int numskins;
    int i;

    if ( Cmd_Argc() < 2 )
    {
        ADMP("^3listskins: ^7usage: listskins <model>\n");
        return;
    }

    Cmd_ArgvBuffer(1, modelname, sizeof(modelname));

    G_GetPlayerModelSkins(modelname, skins, MAX_PLAYER_MODEL, &numskins);

    ADMBP_begin();
    for (i = 0; i < numskins; i++)
    {
        ADMBP(va("%d - %s\n", i+1, skins[i]));
    }
    ADMBP(va("^3listskins: ^7default skin ^2%s\n", GetSkin(modelname, "default")));
    ADMBP(va("^3listskins: ^7showing %d skins for %s\n", numskins, modelname));
    ADMBP_end();
}

/*
=================
Cmd_ListVoices_f
=================
*/
void Cmd_ListVoices_f( gentity_t *ent )
{
  if ( !level.voices ) {
    ADMP( "^3listvoices: ^7voice system is not installed on this server\n" );
    return;
  }

  if ( !g_voiceChats.integer ) {
    ADMP( "^3listvoices: ^7voice system administratively disabled on this server\n" );
    return;
  }

  if ( Cmd_Argc() < 2 )
  {
    voice_t *v;
    int i = 0;

    ADMBP_begin();
    for( v = level.voices; v; v = v->next )
    {
      ADMBP(va("%d - %s\n", i+1, v->name));
      i++;
    }
    ADMBP(va("^3listvoices: ^7showing %d voices\n", i));
    ADMBP("^3listvoices: ^7run 'listvoices <voice>' to see available commands.\n");
    ADMBP_end();
    return;
  }
  else if ( Cmd_Argc() >= 2 )
  {
    voice_t *v;
    voiceCmd_t *c;
    int i = 0;

    char name[ MAX_VOICE_NAME_LEN ];
    Cmd_ArgvBuffer(1, name, sizeof(name));

    v = BG_VoiceByName(level.voices, name);
    if ( !v )
    {
      ADMP(va("^3listvoices: ^7no matching voice \"%s\"\n", name));
      return;
    }

    ADMBP_begin();
    for ( c = v->cmds; c; c = c->next )
    {
      ADMBP(va("%d - %s\n", i+1, c->cmd));
      i++;
    }
    ADMBP(va("^3listvoices: ^7showing %d voice commands for %s\n", i, v->name));
    ADMBP_end();
  }
}

/*
=================
Cmd_Test_f
=================
*/
void Cmd_Test_f( gentity_t *humanPlayer )
{
}

/*
=================
Cmd_Damage_f

Deals damage to you (for testing), arguments: [damage] [dx] [dy] [dz]
The dx/dy arguments describe the damage point's offset from the entity origin
=================
*/
void Cmd_Damage_f( gentity_t *ent )
{
  vec3_t point;
  char arg[ 16 ];
  float dx = 0.0f, dy = 0.0f, dz = 100.0f;
  int damage = BG_HP2SU( 100 );
  qboolean nonloc = qtrue;

  if( Cmd_Argc() > 1 )
  {
    Cmd_ArgvBuffer( 1, arg, sizeof( arg ) );
    damage = atoi( arg );
  }
  if( Cmd_Argc() > 4 )
  {
    Cmd_ArgvBuffer( 2, arg, sizeof( arg ) );
    dx = atof( arg );
    Cmd_ArgvBuffer( 3, arg, sizeof( arg ) );
    dy = atof( arg );
    Cmd_ArgvBuffer( 4, arg, sizeof( arg ) );
    dz = atof( arg );
    nonloc = qfalse;
  }
  VectorCopy( ent->r.currentOrigin, point );
  point[ 0 ] += dx;
  point[ 1 ] += dy;
  point[ 2 ] += dz;
  G_Damage( ent, NULL, NULL, NULL, point, damage,
            ( nonloc ? DAMAGE_NO_LOCDAMAGE : 0 ), MOD_TARGET_LASER );
}

/*
=================
Cmd_Share_f
=================
*/
void Cmd_Share_f( gentity_t *ent )
{
  int    clientNum = -1,
         shareAmount = 0;
  char   cmd[ MAX_TOKEN_CHARS ],
         target[ MAX_STRING_TOKENS ],
         amount[ MAX_STRING_TOKENS ];
  team_t team;
  char   err[ MAX_STRING_TOKENS ];

  if( !ent || !ent->client || ( ent->client->pers.teamSelection == TEAM_NONE ) )
  {
    return;
  }

  if( !g_allowShare.integer )
  {
    SV_GameSendServerCommand( ent-g_entities, "print \"Share has been disabled.\n\"" );
    return;
  }

  team = ent->client->pers.teamSelection;

  if( Cmd_Argc( ) < 3 )
  {
    ADMP( va( "usage: %s [name|slot#] [amount]\n", cmd ) );
  }

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );
  Cmd_ArgvBuffer( 1, target, sizeof( target ) );
  Cmd_ArgvBuffer( 2, amount, sizeof( amount ) );

  clientNum = G_ClientNumberFromString( target, err, sizeof( err ) );
  if( clientNum == -1 )
  {
    ADMP( va( "%s: %s", cmd, err ) );
    return;
  }

  // verify target player team
  if( ( clientNum < 0 ) || ( clientNum >= level.maxclients ) ||
      ( level.clients[ clientNum ].pers.teamSelection != team ) )
  {
    ADMP( va( "%s: %s is not on your team\n", cmd,
          level.clients[ clientNum ].pers.netname ) );
    return;
  }

  // prevent sharing with one's self
  if( &level.clients[ clientNum ] == ent->client )
  {
    ADMP( va( "%s: Seems a bit pointless sharing with yourself, no?\n", cmd ) );
    return;
  }

  // default credit count
  shareAmount = atoi( amount );

  // player specified "0" to transfer
  if( shareAmount <= 0 )
  {
    ADMP( va( "%s: Ooh, you are a generous one, indeed!\n", cmd ) );
    return;
  }

  // transfer at most only credits/evos the player really has
  if( shareAmount > ( ent->client->pers.credit /
        ( team == TEAM_ALIENS ? ALIEN_CREDITS_PER_KILL : 1 ) ) )
  {
    shareAmount = ent->client->pers.credit /
      ( team == TEAM_ALIENS ? ALIEN_CREDITS_PER_KILL : 1 );
  }

  // player has no credits
  if( shareAmount <= 0 )
  {
    ADMP( va( "%s: Earn some more first, you lazy bum!\n", cmd ) );
    return;
  }

  // allow transfers only up to the credit/evo limit
  if( team == TEAM_HUMANS &&
      shareAmount > ( HUMAN_MAX_CREDITS - level.clients[ clientNum ].pers.credit ) )
  {
    shareAmount = shareAmount - level.clients[ clientNum ].pers.credit;
  }
  else if( team == TEAM_ALIENS &&
      shareAmount > ( ALIEN_MAX_FRAGS -
        level.clients[ clientNum ].pers.credit / ALIEN_CREDITS_PER_KILL ) )
  {
    shareAmount = shareAmount - level.clients[ clientNum ].pers.credit /
      ALIEN_CREDITS_PER_KILL;
  }

  // target cannot take any more credits
  if( shareAmount <= 0 )
  {
    ADMP( va( "%s: %s cannot receive any more %s\n", cmd,
          level.clients[ clientNum ].pers.netname,
          team == TEAM_HUMANS ? "credits" : "evos" ) );
    return;
  }

  // deduct credits from sharing client and show it on their console
  G_AddCreditToClient( ent->client,
      -shareAmount * ( team == TEAM_ALIENS ? ALIEN_CREDITS_PER_KILL : 1 ), qfalse );
  ADMP( va( "%s: transferred %d %s to %s^7.\n\"", cmd, shareAmount,
        ( team == TEAM_HUMANS ) ? "credits" : "evos",
        level.clients[ clientNum ].pers.netname ) );

  // add credits to recipient and show it on their console
  G_AddCreditToClient( &(level.clients[ clientNum ]),
      shareAmount * ( team == TEAM_ALIENS ? ALIEN_CREDITS_PER_KILL : 1 ), qtrue );
  SV_GameSendServerCommand( clientNum,
      va( "print \"You have received %d %s from %s^7.\n\"", shareAmount,
        ( team == TEAM_HUMANS ) ? "credits" : "evos",
        ent->client->pers.netname ) );

  // output the share on the server console
  G_LogPrintf( "Share: %i %i %i %d: %s^7 transferred %d%s to %s^7\n",
      ent->client->ps.clientNum,
      clientNum,
      team,
      atoi( amount ),
      ent->client->pers.netname,
      atoi( amount ),
      ( team == TEAM_HUMANS ) ? "credits" : "evos",
      level.clients[ clientNum ].pers.netname );
}

/*
=================
G_DonateCredits

Used by Cmd_Donate_f and by g_overflowFundsj
=================
*/
int G_DonateCredits( gclient_t *client, int value, qboolean verbos )
{
  char *type;
  int i, divisor, portion, new_credits, total=0, max,
      amounts[ MAX_CLIENTS ], totals[ MAX_CLIENTS ], creditConversion;
  qboolean donated = qtrue;
  gentity_t *ent;

  if( !client )
    return 0;

  if( client->pers.teamSelection == TEAM_ALIENS )
  {
    divisor = level.numAlienClients - 1;
    max = ALIEN_MAX_FRAGS * ALIEN_CREDITS_PER_KILL;
    type = "evo(s)";
    creditConversion = ALIEN_CREDITS_PER_KILL;
  }
  else if( client->pers.teamSelection == TEAM_HUMANS )
  {
    divisor = level.numHumanClients - 1;
    max = HUMAN_MAX_CREDITS;
    type = "credit(s)";
    creditConversion = 1;
  }
  else
    return 0;

  if( divisor < 1 )
    return 0;

  if( value <= 0 )
    return 0;

  for( i = 0; i < level.maxclients; i++ )
  {
    amounts[ i ] = 0;
    totals[ i ] = 0;
  }

  // determine donation amounts for each client
  total = value;
  while( donated && value )
  {
    donated = qfalse;

    if( value >= creditConversion )
    {
      portion = value / divisor;
      if( portion < creditConversion )
        portion = creditConversion;
    } else
      portion = value;

    for( i = 0; i < level.maxclients; i++ )
    {
      if( level.clients[ i ].pers.connected == CON_CONNECTED &&
          client != level.clients + i &&
          level.clients[ i ].pers.teamSelection ==
          client->pers.teamSelection  &&
          level.clients[ i ].pers.credit < max )
      {

        new_credits = level.clients[ i ].pers.credit + portion;
        amounts[ i ] = portion;
        totals[ i ] += portion;

        if( new_credits > max )
        {
          amounts[ i ] -= new_credits - max;
          totals[ i ] -= new_credits - max;
          new_credits = max;
        }

        if( amounts[ i ] )
        {
          G_AddCreditToClient( &(level.clients[ i ]), amounts[ i ], qtrue );
          donated = qtrue;
          value -= amounts[ i ];
          if( value < portion )
          {
            if( value > 0 )
              portion = value;
            else
              break;
          }
        }
      }
    }
  }

  if( verbos )
  {
    // transfer funds
    for( i = 0; i < level.maxclients; i++ )
    {
      if( totals[ i ] )
      {
        SV_GameSendServerCommand( i,
            va( "print \"%s^7 donated %f %s to you, don't forget to say 'thank you'!\n\"",
                client->pers.netname,
                ( ( (double)totals[ i ] ) /
                  ( (double)( creditConversion ) ) ), type ) );
      }
    }

    if( total - value )
    {
      ent = &g_entities[ client->ps.clientNum];
      ADMP( va( "Donated %f %s to the cause.\n",
                ( ( (double)( total - value ) ) /
                  ( (double)( creditConversion ) )  ), type ) );
    }
  }

  return total -value;
}

/*
=================
Cmd_Donate_f

Alms for the poor
=================
*/
void Cmd_Donate_f( gentity_t *ent )
{
   char cmd[ MAX_TOKEN_CHARS ], donateAmount[ MAX_TOKEN_CHARS ];
   int value, divisor, amountDonated;

  if( !ent->client )
    return;

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );

  if( !g_allowShare.integer )
  {
    ADMP( "Donate has been disabled.\n" );
    return;
  }

  if( ent->client->pers.teamSelection == TEAM_ALIENS )
    divisor = level.numAlienClients - 1;
  else if( ent->client->pers.teamSelection == TEAM_HUMANS )
    divisor = level.numHumanClients - 1;
  else
  {
    ADMP( va( "%s: spectators cannot be so gracious\n", cmd ) );
    return;
  }

  if( divisor < 1 )
  {
    ADMP( va( "%s: get yourself some teammates first\n", cmd ) );
    return;
  }

  if( Cmd_Argc( ) < 2 )
  {
    ADMP( va( "usage: %s [amount]\n", cmd ) );
    return;
  }

  Cmd_ArgvBuffer( 1, donateAmount, sizeof( donateAmount ) );
  value = atoi( donateAmount );

  if( ent->client->pers.teamSelection == TEAM_ALIENS )
    value *= ALIEN_CREDITS_PER_KILL;

  if( value > ent->client->pers.credit )
    value = ent->client->pers.credit;

  if( value <= 0 )
  {
    ADMP( va( "%s: very funny\n", cmd ) );
    return;
  }

  amountDonated = G_DonateCredits( ent->client, value, qtrue );

  G_AddCreditToClient( ent->client, ( -amountDonated ) , qtrue );
}

/*
==================
G_FloodLimited

Determine whether a user is flood limited, and adjust their flood demerits
Print them a warning message if they are over the limit
Return is time in msec until the user can speak again
==================
*/
int G_FloodLimited( gentity_t *ent )
{
  int deltatime, ms;

  if( g_floodMinTime.integer <= 0 )
    return 0;

  // handles !ent
  if( G_admin_permission( ent, ADMF_NOCENSORFLOOD ) )
    return 0;

  deltatime = level.time + level.pausedTime - ent->client->pers.floodTime;

  ent->client->pers.floodDemerits += g_floodMinTime.integer - deltatime;
  if( ent->client->pers.floodDemerits < 0 )
    ent->client->pers.floodDemerits = 0;
  else if( ent->client->pers.floodDemerits > 2 * g_floodMaxDemerits.integer )
  {
    // cap the flood protection duration at the value of g_floodMaxDemerits
    ent->client->pers.floodDemerits = 2 * g_floodMaxDemerits.integer;
  }
  ent->client->pers.floodTime = level.time + level.pausedTime;

  ms = ent->client->pers.floodDemerits - g_floodMaxDemerits.integer;
  if( ms <= 0 )
    return 0;
  SV_GameSendServerCommand( ent - g_entities, va( "print \"You are flooding: "
                          "please wait %d second%s before trying again\n",
                          ( ms + 999 ) / 1000, ( ms > 1000 ) ? "s" : "" ) );
  return ms;
}

commands_t cmds[ ] = {
  { "a", CMD_MESSAGE|CMD_INTERMISSION, Cmd_AdminMessage_f },
  { "autosellbuy", CMD_HUMAN|CMD_ALIVE, Cmd_Buy_f },
  { "build", CMD_TEAM|CMD_ALIVE, Cmd_Build_f },
  { "buy", CMD_HUMAN|CMD_ALIVE, Cmd_Buy_f },
  { "callteamvote", CMD_MESSAGE|CMD_TEAM, Cmd_CallVote_f },
  { "callvote", CMD_MESSAGE, Cmd_CallVote_f },
  { "class", CMD_TEAM, Cmd_Class_f },
  { "damage", CMD_CHEAT|CMD_ALIVE, Cmd_Damage_f },
  { "deconstruct", CMD_TEAM|CMD_ALIVE, Cmd_Destroy_f },
  { "delag", 0, Cmd_Delag_f },
  { "destroy", CMD_CHEAT|CMD_TEAM|CMD_ALIVE, Cmd_Destroy_f },
  { "donate", CMD_TEAM, Cmd_Donate_f },
  { "follow", CMD_SPEC, Cmd_Follow_f },
  { "follownext", CMD_SPEC, Cmd_FollowCycle_f },
  { "followprev", CMD_SPEC, Cmd_FollowCycle_f },
  { "give", CMD_CHEAT|CMD_TEAM|CMD_ALIVE, Cmd_Give_f },
  { "god", CMD_CHEAT, Cmd_God_f },
  { "ignore", 0, Cmd_Ignore_f },
  { "itemact", CMD_HUMAN|CMD_ALIVE, Cmd_ActivateItem_f },
  { "itemdeact", CMD_HUMAN|CMD_ALIVE, Cmd_DeActivateItem_f },
  { "itemtoggle", CMD_HUMAN|CMD_ALIVE, Cmd_ToggleItem_f },
  { "kill", CMD_TEAM|CMD_ALIVE, Cmd_Kill_f },
  { "listmaps", CMD_MESSAGE|CMD_INTERMISSION, Cmd_ListMaps_f },
  { "listmodels", CMD_MESSAGE|CMD_INTERMISSION, Cmd_ListModels_f },
  { "listskins", CMD_MESSAGE|CMD_INTERMISSION, Cmd_ListSkins_f },
  { "listvoices", CMD_MESSAGE|CMD_INTERMISSION, Cmd_ListVoices_f },
  { "m", CMD_MESSAGE|CMD_INTERMISSION, Cmd_PrivateMessage_f },
  { "mt", CMD_MESSAGE|CMD_INTERMISSION, Cmd_PrivateMessage_f },
  { "noclip", CMD_CHEAT_TEAM, Cmd_Noclip_f },
  { "notarget", CMD_CHEAT|CMD_TEAM|CMD_ALIVE, Cmd_Notarget_f },
  { "r", CMD_MESSAGE|CMD_INTERMISSION, Cmd_PrivateMessage_f },
  { "ready", CMD_MESSAGE|CMD_TEAM, Cmd_Ready_f },
  { "reload", CMD_TEAM|CMD_ALIVE, Cmd_Reload_f },
  { "rt", CMD_MESSAGE|CMD_INTERMISSION, Cmd_PrivateMessage_f },
  { "say", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "say_area", CMD_MESSAGE|CMD_TEAM|CMD_ALIVE, Cmd_SayArea_f },
  { "say_team", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "score", CMD_INTERMISSION, ScoreboardMessage },
  { "sell", CMD_HUMAN|CMD_ALIVE, Cmd_Sell_f },
  { "setviewpos", CMD_CHEAT_TEAM, Cmd_SetViewpos_f },
  { "share", CMD_TEAM, Cmd_Share_f },
  { "specme", CMD_TEAM, Cmd_SpecMe_f },
  { "team", 0, Cmd_Team_f },
  { "teamstatus", CMD_MESSAGE|CMD_TEAM, Cmd_TeamStatus_f },
  { "teamvote", CMD_TEAM, Cmd_Vote_f },
  { "test", CMD_CHEAT, Cmd_Test_f },
  { "unignore", 0, Cmd_Ignore_f },
  { "vote", 0, Cmd_Vote_f },
  { "vsay", CMD_MESSAGE|CMD_INTERMISSION, Cmd_VSay_f },
  { "vsay_local", CMD_MESSAGE|CMD_INTERMISSION, Cmd_VSay_f },
  { "vsay_team", CMD_MESSAGE|CMD_INTERMISSION, Cmd_VSay_f },
  { "where", 0, Cmd_Where_f }
};
static size_t numCmds = ARRAY_LEN( cmds );

/*
=================
ClientCommand
=================
*/
Q_EXPORT void ClientCommand( int clientNum )
{
  gentity_t  *ent;
  char       cmd[ MAX_TOKEN_CHARS ];
  commands_t *command;

  ent = g_entities + clientNum;
  if( !ent->client || ent->client->pers.connected != CON_CONNECTED )
    return;   // not fully in game yet

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );

  command = bsearch( cmd, cmds, numCmds, sizeof( cmds[ 0 ] ), cmdcmp );

  // reset the inactivty timers
  ent->client->inactivityTime = level.time + g_inactivity.integer * 1000;
  ent->client->inactivityWarning = qfalse;
  ent->client->pers.voterInactivityTime = level.time + ( VOTE_TIME );

  if( !command )
  {
    if( !G_admin_cmd_check( ent ) )
      SV_GameSendServerCommand( clientNum,
        va( "print \"Unknown command %s\n\"", cmd ) );
    return;
  }

  // do tests here to reduce the amount of repeated code

  if( !( command->cmdFlags & CMD_INTERMISSION ) &&
      ( level.intermissiontime || level.pausedTime ) )
    return;

  if( command->cmdFlags & CMD_CHEAT && !g_cheats.integer )
  {
    G_TriggerMenu( clientNum, MN_CMD_CHEAT );
    return;
  }

  if( command->cmdFlags & CMD_MESSAGE && ( ent->client->pers.namelog->muted ||
      G_FloodLimited( ent ) ) )
    return;

  if( command->cmdFlags & CMD_TEAM &&
      ent->client->pers.teamSelection == TEAM_NONE )
  {
    G_TriggerMenu( clientNum, MN_CMD_TEAM );
    return;
  }

  if( command->cmdFlags & CMD_CHEAT_TEAM && !g_cheats.integer &&
      ent->client->pers.teamSelection != TEAM_NONE )
  {
    G_TriggerMenu( clientNum, MN_CMD_CHEAT_TEAM );
    return;
  }

  if( command->cmdFlags & CMD_SPEC &&
      ent->client->sess.spectatorState == SPECTATOR_NOT )
  {
    G_TriggerMenu( clientNum, MN_CMD_SPEC );
    return;
  }

  if( command->cmdFlags & CMD_ALIEN &&
      ent->client->pers.teamSelection != TEAM_ALIENS )
  {
    G_TriggerMenu( clientNum, MN_CMD_ALIEN );
    return;
  }

  if( command->cmdFlags & CMD_HUMAN &&
      ent->client->pers.teamSelection != TEAM_HUMANS )
  {
    G_TriggerMenu( clientNum, MN_CMD_HUMAN );
    return;
  }

  if( command->cmdFlags & CMD_ALIVE &&
    ( ent->client->ps.misc[ MISC_HEALTH ] <= 0 ||
      ent->client->sess.spectatorState != SPECTATOR_NOT ) )
  {
    G_TriggerMenu( clientNum, MN_CMD_ALIVE );
    return;
  }

  command->cmdHandler( ent );
}

void G_ListCommands( gentity_t *ent )
{
  int   i;
  char  out[ MAX_STRING_CHARS ] = "";
  int   len, outlen;

  outlen = 0;

  for( i = 0; i < numCmds; i++ )
  {
    // never advertise cheats
    if( cmds[ i ].cmdFlags & CMD_CHEAT )
      continue;

    len = strlen( cmds[ i ].cmdName ) + 1;
    if( len + outlen >= sizeof( out ) - 1 )
    {
      SV_GameSendServerCommand( ent - g_entities, va( "cmds%s\n", out ) );
      outlen = 0;
    }

    strcpy( out + outlen, va( " %s", cmds[ i ].cmdName ) );
    outlen += len;
  }

  SV_GameSendServerCommand( ent - g_entities, va( "cmds%s\n", out ) );
  G_admin_cmdlist( ent );
}

void G_DecolorString( char *in, char *out, int len )
{
  qboolean decolor = qtrue;

  len--;

  while( *in && len > 0 ) {
    if( *in == DECOLOR_OFF || *in == DECOLOR_ON )
    {
      decolor = ( *in == DECOLOR_ON );
      in++;
      continue;
    }
    if( Q_IsColorString( in ) && decolor ) {
      in += 2;
      continue;
    }
    *out++ = *in++;
    len--;
  }
  *out = '\0';
}

void G_UnEscapeString( char *in, char *out, int len )
{
  len--;

  while( *in && len > 0 )
  {
    if( *in >= ' ' || *in == '\n' )
    {
      *out++ = *in;
      len--;
    }
    in++;
  }
  *out = '\0';
}

void Cmd_PrivateMessage_f( gentity_t *ent )
{
  int pids[ MAX_CLIENTS ];
  char name[ MAX_NAME_LENGTH ];
  char cmd[ 12 ];
  char text[ MAX_STRING_CHARS ];
  char *msg;
  char color;
  int scrimming = 0;
  int i;
  int count = 0;
  qboolean teamonly = qfalse;
  qboolean sendToprevRecipients = qfalse;
  prevRecipients_t *prevRecipients = NULL;
  char scrimmers[ MAX_STRING_CHARS ] = "";
  char recipients[ MAX_STRING_CHARS ] = "";
  char disconnectedRecipients[ MAX_STRING_CHARS ] = "";
  int disconnectedRecipientsCount = 0;

  if( !g_privateMessages.integer && ent )
  {
    ADMP( "Sorry, but private messages have been disabled\n" );
    return;
  }

  Cmd_ArgvBuffer( 0, cmd, sizeof( cmd ) );
  if( !Q_stricmp( cmd, "r" ) ||
      !Q_stricmp( cmd, "rt" ) )
  {
    if( Cmd_Argc( ) < 2 )
    {
      ADMP( va( "usage: %s [message]\n", cmd ) );
      return;
    }

    sendToprevRecipients = qtrue;
    msg = ConcatArgs( 1 );
  }
  else
  {
    if( Cmd_Argc( ) < 3 )
    {
      ADMP( va( "usage: %s [name|slot#] [message]\n", cmd ) );
      return;
    }

    Cmd_ArgvBuffer( 1, name, sizeof( name ) );
    msg = ConcatArgs( 2 );
  }

  if( ent )
  {
    prevRecipients = &ent->client->pers.namelog->prevRecipients;
  } else
    prevRecipients = &level.prevRecipients;

  if( !Q_stricmp( cmd, "mt" ) ||
      !Q_stricmp( cmd, "rt" ) )
    teamonly = qtrue;

  G_CensorString( text, msg, sizeof( text ), ent );

  // send the message
  if( sendToprevRecipients )
  {
    for( i = 0; i < prevRecipients->count; i++ )
    {
      int slot;
      namelog_t *n;

      //find the matching namelog
      for( n = level.namelogs; n; n = n->next )
      {
        if( n->id == prevRecipients->id[i] )
          break;
      }

      if( n )
      {
        slot = n->slot;
        if( slot < 0 )
        {
          disconnectedRecipientsCount++;
          Q_strcat( disconnectedRecipients, sizeof( disconnectedRecipients ), va( "%s" S_COLOR_WHITE ", ",
            n->name[ n->nameOffset ] ) );
        } else if( G_SayTo( ent, &g_entities[ slot ],
                            (teamonly || 
                              (IS_SCRIM && level.scrim.mode != SCRIM_MODE_SETUP))
                              ? SAY_TPRIVMSG : SAY_PRIVMSG, text ) )
        {
          count++;
          Q_strcat( recipients, sizeof( recipients ), va( "%s" S_COLOR_WHITE ", ",
            level.clients[ slot ].pers.netname ) );
        } else if(
          !teamonly && (IS_SCRIM && level.scrim.mode != SCRIM_MODE_SETUP) && ent &&
          !OnSameTeam( ent, &g_entities[ slot ] )) {
          scrimming++;
          Q_strcat( scrimmers, sizeof( scrimmers ), va( "%s" S_COLOR_WHITE ", ",
            level.clients[ slot ].pers.netname ) );
        }
      }
    }
  }
  else
  {
    int pcount = G_ClientNumbersFromString( name, pids,
                                            MAX_CLIENTS );

    // reset the previous recipient list
    prevRecipients->count = 0;

    for( i = 0; i < pcount; i++ )
    {
      if( G_SayTo( ent, &g_entities[ pids[ i ] ],
          (teamonly || (IS_SCRIM && level.scrim.mode != SCRIM_MODE_SETUP))
            ? SAY_TPRIVMSG : SAY_PRIVMSG, text ) )
      {
        count++;
        Q_strcat( recipients, sizeof( recipients ), va( "%s" S_COLOR_WHITE ", ",
          level.clients[ pids[ i ] ].pers.netname ) );

        prevRecipients->id[prevRecipients->count] = level.clients[ pids[ i ] ].pers.namelog->id;
        prevRecipients->count++;
      }  else if(
        !teamonly && (IS_SCRIM && level.scrim.mode != SCRIM_MODE_SETUP) && ent &&
        !OnSameTeam( ent, &g_entities[ pids[ i ] ] )) {
          scrimming++;
          Q_strcat( scrimmers, sizeof( scrimmers ), va( "%s" S_COLOR_WHITE ", ",
            level.clients[ pids[ i ] ].pers.netname ) );
      }
    }
  }

  // report the results
  color = (teamonly || (IS_SCRIM && level.scrim.mode != SCRIM_MODE_SETUP))
    ? COLOR_CYAN : COLOR_YELLOW;

  if( scrimming )
  {
    // remove trailing ", "
    scrimmers[ strlen( scrimmers ) - 2 ] = '\0';
    ADMP(
      va(
        "^%ccan't send to %i player%s because scrim mode is ^1on^%c: " S_COLOR_WHITE "%s^7\n",
        color, scrimming, scrimming == 1 ? "" : "s", color, scrimmers ) );
  }

  if( !count )
  {
    if( sendToprevRecipients )
    {
      ADMP( va( "^%c%s: No connected %s on your previous recipients list.\n" ,
                color, cmd,
                (teamonly ||
                  (IS_SCRIM && level.scrim.mode != SCRIM_MODE_SETUP))
                  ? "teammates" : "players" ) );
      if( disconnectedRecipientsCount )
      {
        // remove trailing ", "
        disconnectedRecipients[ strlen( disconnectedRecipients ) - 2 ] = '\0';
        ADMP( va( "^%c%i disconnected previous recipient%s: " S_COLOR_WHITE "%s\n", color,
          disconnectedRecipientsCount,
          disconnectedRecipientsCount == 1 ? "" : "s", disconnectedRecipients ) );
      }
    }
    else
      ADMP( va( "^3No player matching ^7\'%s^7\' ^3to send message to.\n",
                name ) );
  }
  else
  {
    ADMP( va( "^%cPrivate message: ^7%s\n", color, text ) );
    // remove trailing ", "
    recipients[ strlen( recipients ) - 2 ] = '\0';
    ADMP( va( "^%csent to %i player%s: " S_COLOR_WHITE "%s\n", color, count,
      count == 1 ? "" : "s", recipients ) );
    if( disconnectedRecipientsCount )
    {
      // remove trailing ", "
      disconnectedRecipients[ strlen( disconnectedRecipients ) - 2 ] = '\0';
      ADMP( va( "^%c%i disconnected previous recipient%s: " S_COLOR_WHITE "%s\n", color,
        disconnectedRecipientsCount,
        disconnectedRecipientsCount == 1 ? "" : "s", disconnectedRecipients ) );
    }

    if( g_logPrivateMessages.integer )
      G_LogPrintf( "%s: %d \"%s" S_COLOR_WHITE "\" \"%s" S_COLOR_WHITE "\": ^%c%s\n",
        ( (teamonly || 
            (IS_SCRIM && level.scrim.mode != SCRIM_MODE_SETUP)) )
            ? "TPrivMsg" : "PrivMsg",
        (int)( ( ent ) ? ent - g_entities : -1 ),
        ( ent ) ? ent->client->pers.netname : "console",
        recipients, color, msg );
  }
}

/*
=================
Cmd_AdminMessage_f

Send a message to all active admins
=================
*/
void Cmd_AdminMessage_f( gentity_t *ent )
{
  // Check permissions and add the appropriate user [prefix]
  if( !G_admin_permission( ent, ADMF_ADMINCHAT ) )
  {
    if( !g_publicAdminMessages.integer )
    {
      ADMP( "Sorry, but use of /a by non-admins has been disabled.\n" );
      return;
    }
    else
    {
      ADMP( "Your message has been sent to any available admins "
            "and to the server logs.\n" );
    }
  }

  if( Cmd_Argc( ) < 2 )
  {
    ADMP( "usage: a [message]\n" );
    return;
  }

  G_AdminMessage( ent, ConcatArgs( 1 ) );
}

/*
=================
Cmd_Delag_f

Resend Gamestate
TODO: We may need a longer flood limit time.
=================
*/
void Cmd_Delag_f( gentity_t *ent )
{
  const int clientNum = ent->s.number;

  if( clientNum < 0 || clientNum >= MAX_CLIENTS )
    return;

  SV_SendClientGameState2( clientNum );
}
