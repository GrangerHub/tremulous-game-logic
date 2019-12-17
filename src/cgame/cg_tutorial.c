/*
===========================================================================
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

// cg_tutorial.c -- the tutorial system

#include "cg_local.h"

typedef struct
{
  char      *command;
  char      *humanName;
  keyNum_t  keys[ 2 ];
} bind_t;

static bind_t bindings[ ] =
{
  { "+button2",       "Activate Upgrade",       { -1, -1 } },
  { "+speed",         "Run/Walk",               { -1, -1 } },
  { "+button6",       "Dodge",                  { -1, -1 } },
  { "+button8",       "Sprint",                 { -1, -1 } },
  { "+moveup",        "Jump",                   { -1, -1 } },
  { "+movedown",      "Crouch",                 { -1, -1 } },
  { "+attack",        "Primary Attack",         { -1, -1 } },
  { "+button5",       "Secondary Attack",       { -1, -1 } },
  { "reload",         "Reload",                 { -1, -1 } },
  { "buy ammo",       "Buy Ammo / Refuel", { -1, -1 } },
  { "itemact medkit", "Use Medkit",             { -1, -1 } },
  { "+button7",       "Use Structure/Evolve",   { -1, -1 } },
  { "deconstruct",    "Deconstruct Structure",  { -1, -1 } },
  { "weapprev",       "Previous Upgrade",       { -1, -1 } },
  { "weapnext",       "Next Upgrade",           { -1, -1 } },
  { "ready",          "Ready",                  { -1, -1 } }
};

static const size_t numBindings = ARRAY_LEN( bindings );

/*
=================
CG_GetBindings
=================
*/
static void CG_GetBindings( void )
{
  int   i, j, numKeys;
  char  buffer[ MAX_STRING_CHARS ];

  for( i = 0; i < numBindings; i++ )
  {
    bindings[ i ].keys[ 0 ] = bindings[ i ].keys[ 1 ] = K_NONE;
    numKeys = 0;

    for( j = 0; j < K_LAST_KEY; j++ )
    {
      trap_Key_GetBindingBuf( j, buffer, MAX_STRING_CHARS );

      if( buffer[ 0 ] == 0 )
        continue;

      if( !Q_stricmp( buffer, bindings[ i ].command ) )
      {
        bindings[ i ].keys[ numKeys++ ] = j;

        if( numKeys > 1 )
          break;
      }
    }
  }
}

/*
===============
CG_KeyNameForCommand
===============
*/
static const char *CG_KeyNameForCommand( const char *commandIn )
{
  int         i, j;
  static char buffer[ MAX_STRING_CHARS ];
  const char  *command = !cg_swapAttacks.integer ?
                              commandIn :
                              ( !Q_stricmp( commandIn, "+attack" ) ?
                                  "+button5" :
                                  ( !Q_stricmp( commandIn, "+button5" ) ?
                                      "+attack" :
                                      commandIn ) ) ;
  int         firstKeyLength;

  buffer[ 0 ] = '\0';

  for( i = 0; i < numBindings; i++ )
  {
    if( !Q_stricmp( command, bindings[ i ].command ) )
    {
      if( bindings[ i ].keys[ 0 ] != K_NONE )
      {
        trap_Key_KeynumToStringBuf( bindings[ i ].keys[ 0 ],
            buffer, MAX_STRING_CHARS );
        firstKeyLength = strlen( buffer );

        for( j = 0; j < firstKeyLength; j++ )
          buffer[ j ] = toupper( buffer[ j ] );

        if( bindings[ i ].keys[ 1 ] != K_NONE )
        {
          Q_strcat( buffer, MAX_STRING_CHARS, " or " );
          trap_Key_KeynumToStringBuf( bindings[ i ].keys[ 1 ],
              buffer + strlen( buffer ), MAX_STRING_CHARS - strlen( buffer ) );

          for( j = firstKeyLength + 4; j < strlen( buffer ); j++ )
            buffer[ j ] = toupper( buffer[ j ] );
        }
      }
      else
      {
        Com_sprintf( buffer, MAX_STRING_CHARS, "\"%s\" (unbound)",
          bindings[ i ].humanName );
      }

      return buffer;
    }
  }

  return "";
}

#define MAX_TUTORIAL_TEXT 4096

/*
===============
CG_BuildableInRange
===============
*/
static entityState_t *CG_BuildableInRange( playerState_t *ps, float *healthFraction )
{
  vec3_t        view, point;
  trace_t       trace;
  entityState_t *es;
  int           health;

  AngleVectors( cg.refdefViewAngles, view, NULL, NULL );
  VectorMA( cg.refdef.vieworg, 64, view, point );
  CG_Trace( &trace, cg.refdef.vieworg, NULL, NULL,
            point, ps->clientNum, *Temp_Clip_Mask(MASK_SHOT, 0) );

  es = &cg_entities[ trace.entityNum ].currentState;

  if( healthFraction )
  {
    health = es->misc;
    *healthFraction = (float)health / BG_Buildable( es->modelindex )->health;
  }

  if( es->eType == ET_BUILDABLE &&
      ps->stats[ STAT_TEAM ] == BG_Buildable( es->modelindex )->team )
    return es;
  else
    return NULL;
}

/*
===============
CG_AlienBuilderText
===============
*/
static void CG_AlienBuilderText( char *text, playerState_t *ps )
{
  buildable_t   buildable = ps->stats[ STAT_BUILDABLE ] & ~SB_VALID_TOGGLEBIT;
  entityState_t *es;

  if( cgs.warmup )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s when ready to start the game\n",
          CG_KeyNameForCommand( "ready" ) ) );
  }

  if( !( ps->stats[ STAT_STATE ] & SS_HOVELING ) )
  {
    if( buildable > BA_NONE )
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to place the %s\n",
            CG_KeyNameForCommand( "+attack" ),
            BG_Buildable( buildable )->humanName ) );

      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to cancel placing the %s\n",
            CG_KeyNameForCommand( "+button5" ),
            BG_Buildable( buildable )->humanName ) );

      if(!BG_Class(ps->stats[ STAT_CLASS ])->buildPreciseForce) {
        if( ps->stats[ STAT_STATE ] & SS_PRECISE_BUILD )
        {
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Release %s to position the %s by line of sight\n",
                CG_KeyNameForCommand( "+speed" ),
                BG_Buildable( buildable )->humanName ) );
        } else
        {
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Hold down %s to position the %s relative to you\n",
                CG_KeyNameForCommand( "+speed" ),
                BG_Buildable( buildable )->humanName ) );
        }
      }
    }
    else
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to build a structure\n",
            CG_KeyNameForCommand( "+attack" ) ) );
    }
  }

  if( ps->stats[ STAT_STATE ] & SS_HOVELING )
  {
    if( cgs.markDeconstruct )
    {
      if( ps->stats[ STAT_STATE ] & SS_HOVEL_MARKED )
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to unmark this hovel for replacement\n",
              CG_KeyNameForCommand( "reload" ) ) );
      }
      else
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to mark this hovel for replacement\n",
              CG_KeyNameForCommand( "reload" ) ) );
      }
    }

    Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to destroy this hovel\n",
                  CG_KeyNameForCommand( "deconstruct" ) ) );
  } else 
  {
    if( ( es = CG_BuildableInRange( ps, NULL ) ) )
    {
      if( cgs.markDeconstruct )
      {
        if( es->eFlags & EF_B_MARKED )
        {
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to unmark this structure for replacement\n",
                CG_KeyNameForCommand( "reload" ) ) );
        }
        else
        {
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to mark this structure for replacement\n",
                CG_KeyNameForCommand( "reload" ) ) );
        }
      }

      Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to destroy this structure\n",
                CG_KeyNameForCommand( "deconstruct" ) ) );
    }

    if( ( ps->stats[ STAT_BUILDABLE ] & ~SB_VALID_TOGGLEBIT ) == BA_NONE )
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to swipe\n",
              CG_KeyNameForCommand( "+button5" ) ) );
    }

    if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_BUILDER0_UPG )
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to launch a projectile\n",
              CG_KeyNameForCommand( "+button2" ) ) );
                        
      Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to walk on walls\n",
              CG_KeyNameForCommand( "+movedown" ) ) );
    }
  }
}

/*
===============
CG_AlienLevel0Text
===============
*/
static void CG_AlienLevel0Text( char *text, playerState_t *ps )
{
  if( cgs.warmup )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s when ready to start the game\n",
          CG_KeyNameForCommand( "ready" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      "Touch humans to damage them\n" );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to walk on walls\n",
        CG_KeyNameForCommand( "+movedown" ) ) );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down and release %s to pounce\n",
        CG_KeyNameForCommand( "+button5" ) ) );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down %s to keep pounces short\n",
        CG_KeyNameForCommand( "+speed" ) ) );
}

/*
===============
CG_AlienLevel1Text
===============
*/
static void CG_AlienLevel1Text( char *text, playerState_t *ps )
{
  if( cgs.warmup )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s when ready to start the game\n",
          CG_KeyNameForCommand( "ready" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      "swipe humans to grab them\n" );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to swipe\n",
        CG_KeyNameForCommand( "+attack" ) ) );

        

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down %s to charge for invisibility\n",
        CG_KeyNameForCommand( "+button5" ) ) );

  if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL1_UPG )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to spray poisonous gas\n",
          CG_KeyNameForCommand( "+button5" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to walk on walls\n",
        CG_KeyNameForCommand( "+movedown" ) ) );
}

/*
===============
CG_AlienLevel2Text
===============
*/
static void CG_AlienLevel2Text( char *text, playerState_t *ps )
{
  if( cgs.warmup )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s when ready to start the game\n",
          CG_KeyNameForCommand( "ready" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to bite\n",
        CG_KeyNameForCommand( "+attack" ) ) );

  if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL2 )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Hold %s to charge up for a kamikaze zap explosion\n",
          CG_KeyNameForCommand( "+button5" ) ) );
  }
  else if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL2_UPG )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to invoke an electrical attack\n",
          CG_KeyNameForCommand( "+button5" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down %s then touch a wall to wall jump\n",
        CG_KeyNameForCommand( "+moveup" ) ) );
}

/*
===============
CG_AlienLevel3Text
===============
*/
static void CG_AlienLevel3Text( char *text, playerState_t *ps )
{
  if( cgs.warmup )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s when ready to start the game\n",
          CG_KeyNameForCommand( "ready" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to bite\n",
        CG_KeyNameForCommand( "+attack" ) ) );

  if( ps->stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL3_UPG )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to launch a projectile\n",
          CG_KeyNameForCommand( "+button2" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down and release %s to pounce\n",
        CG_KeyNameForCommand( "+button5" ) ) );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down %s for greater pounces\n",
        CG_KeyNameForCommand( "+speed" ) ) );
}

/*
===============
CG_AlienLevel4Text
===============
*/
static void CG_AlienLevel4Text( char *text, playerState_t *ps )
{
  if( cgs.warmup )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s when ready to start the game\n",
          CG_KeyNameForCommand( "ready" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to swipe\n",
        CG_KeyNameForCommand( "+attack" ) ) );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Hold down and release %s to trample\n",
        CG_KeyNameForCommand( "+button5" ) ) );
}

/*
===============
CG_AlienSpitfireText
===============
*/
static void CG_AlienSpitfireText( char *text, playerState_t *ps )
{
  usercmd_t cmd;
  int cmdNum;
  vec3_t  point;
  int     cont;
  vec3_t  mins;

  cmdNum = trap_GetCurrentCmdNumber( );
  trap_GetUserCmd( cmdNum, &cmd );

  if( cgs.warmup )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s when ready to start the game\n",
          CG_KeyNameForCommand( "ready" ) ) );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s to zap\n",
        CG_KeyNameForCommand( "+attack" ) ) );

  if(!(cmd.buttons & BUTTON_USE_HOLDABLE)) {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Hold %s to spray a gas trail\n",
          CG_KeyNameForCommand( "+button2" ) ) );
  } else {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Release %s to detonate the gas trail\n",
          CG_KeyNameForCommand( "+button2" ) ) );
  }

  BG_ClassBoundingBox( ps->stats[ STAT_CLASS ], mins, NULL, NULL, NULL, NULL );

  point[ 0 ] = ps->origin[ 0 ];
  point[ 1 ] = ps->origin[ 1 ];
  point[ 2 ] = ps->origin[ 2 ] + mins[2] + ( ( ps->viewheight - mins[2] ) / 2 );
  cont = CG_PointContents( point, ps->clientNum );

  if( cont & MASK_WATER )
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Exit the water, then hold down and release %s to air pounce\n",
          CG_KeyNameForCommand( "+button5" ) ) );
  else
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Hold down and release %s to air pounce\n",
          CG_KeyNameForCommand( "+button5" ) ) );

  if( !(cont & MASK_WATER) )
  {
    if( ps->groundEntityNum == ENTITYNUM_NONE )
    {
      if( !( cmd.buttons & BUTTON_WALKING ) )
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold down %s to hover\n",
              CG_KeyNameForCommand( "+speed" ) ) );
      else
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Release %s to stop hovering\n",
              CG_KeyNameForCommand( "+speed" ) ) );
    }
    else
    {
      if( !( cmd.buttons & BUTTON_WALKING ) )
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold down %s to walk slower\n",
              CG_KeyNameForCommand( "+speed" ) ) );
      else
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Release %s to walk faster\n",
              CG_KeyNameForCommand( "+speed" ) ) );
    }
  }

  if( cmd.upmove >= 0 )
  {
    if( cont & MASK_WATER )
    {
      if( cmd.upmove == 0 )
        Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to swim up\n",
                CG_KeyNameForCommand( "+moveup" ) ) );
      else
        Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Release %s to stop swimming up\n",
                CG_KeyNameForCommand( "+moveup" ) ) );
    }
    else if( ps->groundEntityNum == ENTITYNUM_NONE )
    {
      if( cmd.upmove == 0 )
        Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to ascend\n",
                CG_KeyNameForCommand( "+moveup" ) ) );
      else
        Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Release %s to stop ascending\n",
                CG_KeyNameForCommand( "+moveup" ) ) );
    }
    else
      Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to jump\n",
              CG_KeyNameForCommand( "+moveup" ) ) );
  }

  if( cont & MASK_WATER )
  {
    if( cmd.upmove < 0 )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Release %s to stop swimming down\n",
              CG_KeyNameForCommand( "+movedown" ) ) );
    else if( cmd.upmove == 0 )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to swim down\n",
              CG_KeyNameForCommand( "+movedown" ) ) );
  }
  else if( cmd.buttons & BUTTON_WALKING )
  {
    if( cmd.upmove < 0 )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Release %s to not descend while hovering\n",
              CG_KeyNameForCommand( "+movedown" ) ) );
    else if( cmd.upmove == 0 )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to descend while hovering\n",
              CG_KeyNameForCommand( "+movedown" ) ) );
  }
  else if( cmd.upmove == 0  )
    Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Hold down %s to free fall while in the air\n",
            CG_KeyNameForCommand( "+movedown" ) ) );
  else if( cmd.upmove < 0 )
    Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Release %s to not free fall while in the air\n",
            CG_KeyNameForCommand( "+movedown" ) ) );
}

/*
===============
CG_HumanCkitText
===============
*/
static void CG_HumanCkitText( char *text, playerState_t *ps )
{
  buildable_t   buildable = ps->stats[ STAT_BUILDABLE ] & ~SB_VALID_TOGGLEBIT;
  entityState_t *es;

  if( buildable > BA_NONE )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to place the %s\n",
          CG_KeyNameForCommand( "+attack" ),
          BG_Buildable( buildable )->humanName ) );

    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to cancel placing the %s\n",
          CG_KeyNameForCommand( "+button5" ),
          BG_Buildable( buildable )->humanName ) );

    if(!BG_Class(ps->stats[ STAT_CLASS ])->buildPreciseForce) {
      if( ps->stats[ STAT_STATE ] & SS_PRECISE_BUILD )
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Release %s to position the %s by line of sight\n",
              CG_KeyNameForCommand( "+speed" ),
              BG_Buildable( buildable )->humanName ) );
      } else
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold down %s to position the %s relative to you\n",
              CG_KeyNameForCommand( "+speed" ),
              BG_Buildable( buildable )->humanName ) );
      }
    }
  }
  else
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to build a structure\n",
          CG_KeyNameForCommand( "+attack" ) ) );
  }

  if( ( es = CG_BuildableInRange( ps, NULL ) ) )
  {
    if( cgs.markDeconstruct )
    {
      if( es->eFlags & EF_B_MARKED )
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to unmark this structure\n",
              CG_KeyNameForCommand( "reload" ) ) );
      }
      else
      {
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to mark this structure\n",
              CG_KeyNameForCommand( "reload" ) ) );
      }
    }

    Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to destroy this structure\n",
                   CG_KeyNameForCommand( "deconstruct" ) ) );
  }
}

/*
===============
CG_HumanText
===============
*/
static void CG_HumanText( char *text, playerState_t *ps )
{
  char      *name;
  upgrade_t upgrade = UP_NONE;

  if( cgs.warmup )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s when ready to start the game\n",
          CG_KeyNameForCommand( "ready" ) ) );
  }

  if( cg.weaponSelect < WP_NUM_WEAPONS )
    name = cg_weapons[ cg.weaponSelect ].humanName;
  else
  {
    name = cg_upgrades[ cg.weaponSelect - WP_NUM_WEAPONS ].humanName;
    upgrade = cg.weaponSelect - WP_NUM_WEAPONS;
  }

  if( !ps->ammo && !ps->clips && !BG_Weapon( ps->weapon )->infiniteAmmo )
  {
    //no ammo
    if( !BG_Weapon( ps->weapon )->ammoPurchasable )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          "Ammo can't be purchased for this weapon.\nFind an armoury to exchange this weapon for another one\n" );
    else if( BG_Weapon( ps->weapon )->usesEnergy )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Find an Armoury, Reactor, or Repeater and press %s for more ammo\n",
            CG_KeyNameForCommand( "buy ammo" ) ) );
    else
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Find an Armoury and press %s for more ammo\n",
            CG_KeyNameForCommand( "buy ammo" ) ) );
  }
  else
  {
    switch( ps->weapon )
    {
      case WP_BLASTER:
      case WP_MACHINEGUN:
      case WP_SHOTGUN:
      case WP_LAS_GUN:
      case WP_CHAINGUN:
      case WP_PULSE_RIFLE:
      case WP_FLAMER:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to fire the %s\n",
              CG_KeyNameForCommand( "+attack" ),
              BG_Weapon( ps->weapon )->humanName ) );
        break;

        
      case WP_LAUNCHER:
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to launch an impact grenade\n",
            CG_KeyNameForCommand( "+attack" ) ) );

            
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to launch a timed grenade\n",
            CG_KeyNameForCommand( "+button5" ) ) );
      break;

      case WP_MASS_DRIVER:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to fire the %s\n",
              CG_KeyNameForCommand( "+attack" ),
              BG_Weapon( ps->weapon )->humanName ) );

        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold %s to zoom\n",
              CG_KeyNameForCommand( "+button5" ) ) );
        break;

      case WP_PAIN_SAW:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold %s to activate the %s\n",
              CG_KeyNameForCommand( "+attack" ),
              BG_Weapon( ps->weapon )->humanName ) );
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold %s to activate the %s with pull\n",
              CG_KeyNameForCommand( "+button5" ),
              BG_Weapon( ps->weapon )->humanName ) );
        break;

      case WP_LUCIFER_CANNON:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold and release %s to fire a charged shot\nHold %s while charging to reduce the charge\n",
              CG_KeyNameForCommand( "+attack" ), CG_KeyNameForCommand( "+button5" ) ) );

        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Press %s to fire the %s\n",
              CG_KeyNameForCommand( "+button5" ),
              BG_Weapon( ps->weapon )->humanName ) );
        break;

      case WP_PORTAL_GUN:
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to fire a red portal\n",
            CG_KeyNameForCommand( "+attack" ) ) );

      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s to fire a blue portal\n",
            CG_KeyNameForCommand( "+button5" ) ) );

      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Hold down %s when firing a portal to turn off relative fire velocity\n",
            CG_KeyNameForCommand( "+speed" ) ) );
      break;

      case WP_LIGHTNING:
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold %s to charge and fire pulsating lightning bolts\n",
              CG_KeyNameForCommand( "+attack" ) ) );

        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Hold %s to spray a burst of ball lightning\n",
              CG_KeyNameForCommand( "+button5" ) ) );

              
        Q_strcat( text, MAX_TUTORIAL_TEXT,
            va( "Release %s to emmit an EMP field that destabilizes ball lightning\n",
              CG_KeyNameForCommand( "+button5" ) ) );
        break;

      case WP_HBUILD:
      case WP_HBUILD2:
        CG_HumanCkitText( text, ps );
        break;

      default:
        break;
    }
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s and ",
          CG_KeyNameForCommand( "weapprev" ) ) );
  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "%s to select an upgrade\n",
          CG_KeyNameForCommand( "weapnext" ) ) );

  if( upgrade == UP_NONE ||
      ( upgrade > UP_NONE && BG_Upgrade( upgrade )->usable ) )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to use the %s\n",
            CG_KeyNameForCommand( "+button2" ),
            name ) );
  }

  if( ps->misc[ MISC_HEALTH ] <= 35 &&
      BG_InventoryContainsUpgrade( UP_MEDKIT, ps->stats ) )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to use your %s\n",
          CG_KeyNameForCommand( "itemact medkit" ),
          BG_Upgrade( UP_MEDKIT )->humanName ) );
  }

  if( ps->stats[ STAT_STAMINA ] <= STAMINA_BLACKOUT_LEVEL )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        "You are blacking out. Stop sprinting to recover stamina\n" );
  }
  else if( ps->stats[ STAT_STAMINA ] <= STAMINA_SLOW_LEVEL )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        "Your stamina is low. Stop sprinting to recover\n" );
  }

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s and any direction to sprint\n",
        CG_KeyNameForCommand( "+button8" ) ) );

  Q_strcat( text, MAX_TUTORIAL_TEXT,
      va( "Press %s and back or strafe to dodge\n",
        CG_KeyNameForCommand( "+button6" ) ) );

  if( BG_InventoryContainsUpgrade( UP_JETPACK, ps->stats ) )
  {
    if( ps->stats[ STAT_FUEL ] >= JETPACK_FUEL_MIN_START )
    {
      char key_string_1[MAX_TUTORIAL_TEXT];
      char key_string_2[MAX_TUTORIAL_TEXT];
      char key_string_3[MAX_TUTORIAL_TEXT];

      if(BG_UpgradeIsActive(UP_JETPACK, ps->stats)) {
        usercmd_t cmd;
        int       cmdNum;

        cmdNum = trap_GetCurrentCmdNumber( );
        trap_GetUserCmd( cmdNum, &cmd );

        if(( cmd.buttons & BUTTON_WALKING )) {
          if(cmd.upmove > 0.0f) {
            Q_strncpyz(
              key_string_1,
              CG_KeyNameForCommand( "+speed" ), sizeof(key_string_1));
            Q_strncpyz(
              key_string_2,
              CG_KeyNameForCommand( "+moveup" ), sizeof(key_string_2));
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Release %s and %s to deactivate with the jetpack\n",
                  key_string_1,
                  key_string_2) );

            Q_strncpyz(
              key_string_1,
              CG_KeyNameForCommand( "+moveup" ), sizeof(key_string_1));
            Q_strncpyz(
              key_string_2,
              CG_KeyNameForCommand( "+speed" ), sizeof(key_string_2));
            Q_strncpyz(
              key_string_3,
              CG_KeyNameForCommand( "+movedown" ), sizeof(key_string_3));
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Release %s while holding %s and %s to descend with the jetpack\n",
                  key_string_1,
                  key_string_2,
                  key_string_3 ) );
          } else if(cmd.upmove < 0.0f) {
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Release %s to deactivate the jetpack\n",
                  CG_KeyNameForCommand( "+speed" )) );

                  

            Q_strncpyz(
              key_string_1, 
              CG_KeyNameForCommand( "+movedown" ), sizeof(key_string_1));
            Q_strncpyz(
              key_string_2, 
              CG_KeyNameForCommand( "+moveup" ), sizeof(key_string_2));
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Release %s and hold %s to ascend with the jetpack\n",
                  key_string_1,
                  key_string_2 ) );
          } else {
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Release %s to deactivate the jetpack\n",
                  CG_KeyNameForCommand( "+speed" )) );

                  
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Hold down %s to ascend with the jetpack\n",
                  CG_KeyNameForCommand( "+moveup" ) ) );

            Q_strncpyz(
              key_string_1,
              CG_KeyNameForCommand( "+movedown" ), sizeof(key_string_1));
            Q_strncpyz(
              key_string_2,
              CG_KeyNameForCommand( "+speed" ), sizeof(key_string_2));
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Hold down %s and %s to descend with the jetpack\n",
                  key_string_1,
                  key_string_2 ) );
          }
        } else {
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Release %s to deactivate with the jetpack\n",
                CG_KeyNameForCommand( "+moveup" )) );

          Q_strncpyz(
            key_string_1,
            CG_KeyNameForCommand( "+speed" ), sizeof(key_string_1));
          Q_strncpyz(
            key_string_2,
            CG_KeyNameForCommand( "+moveup" ), sizeof(key_string_2));
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Release %s and hold down %s to hover with the jetpack\n",
                key_string_1,
                key_string_2) );
        }
      } else {
        Q_strncpyz(
          key_string_1,
          CG_KeyNameForCommand( "+moveup" ), sizeof(key_string_1));
        Q_strncpyz(
          key_string_2,
          CG_KeyNameForCommand( "+speed" ), sizeof(key_string_2));
        Q_strcat( text, MAX_TUTORIAL_TEXT,
                   va( "%sress and hold %s or %s while off of the ground to activate the jetpack\n",
                       ps->groundEntityNum == ENTITYNUM_NONE ? "P" : va( "Jump off the ground by pressing %s, then p",
                                                                         CG_KeyNameForCommand( "+moveup" ) ),
                       key_string_1,
                       key_string_2) );
      }
    }

    if( ps->stats[ STAT_FUEL ] < JETPACK_FUEL_MIN_START &&
             !BG_UpgradeIsActive( UP_JETPACK, ps->stats ) )
    {
             Q_strcat( text, MAX_TUTORIAL_TEXT,
                       va( "You don't have enough jet fuel to activate your jetpack. Find an Armoury to buy jet fuel, or wait for your jet fuel to regenerate\n" ) );
    }
    else if( ps->stats[ STAT_FUEL ] <= JETPACK_FUEL_LOW &&
             ps->stats[ STAT_FUEL ] > 0 )
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "You are running low on jet fuel. Find an Armoury to buy jet fuel, or wait for your jet fuel to regenerate while your jetpack is deactivated\n" ) );
    }
    else if( ps->stats[ STAT_FUEL ] <= 0 )
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "You are out of jet fuel. You can no longer fly. Find an Armoury to buy jet fuel, or wait for your jet fuel to regenerate\n" ) );
    }
  }
}

/*
===============
CG_SpectatorText
===============
*/
static void CG_SpectatorText( char *text, playerState_t *ps )
{
  if( cgs.clientinfo[ cg.clientNum ].team != TEAM_NONE )
  {
    if( cgs.warmup )
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s when ready to start the game\n",
            CG_KeyNameForCommand( "ready" ) ) );
    }

    if( ps->persistant[ PERS_STATE ] & PS_QUEUED )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to leave spawn queue\n",
                    CG_KeyNameForCommand( "+attack" ) ) );
    else
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to spawn\n",
                    CG_KeyNameForCommand( "+attack" ) ) );
  }
  else 
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to join a team\n",
          CG_KeyNameForCommand( "+attack" ) ) );
  }

  if( ps->pm_flags & PMF_FOLLOW )
  {
    if( !cg.chaseFollow )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to switch to chase-cam spectator mode\n",
                    CG_KeyNameForCommand( "+button2" ) ) );
    else if( cgs.clientinfo[ cg.clientNum ].team == TEAM_NONE )
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to return to free spectator mode\n",
                    CG_KeyNameForCommand( "+button2" ) ) );
    else
      Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to stop following\n",
                    CG_KeyNameForCommand( "+button2" ) ) );

    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s or ",
          CG_KeyNameForCommand( "weapprev" ) ) );
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "%s to change player\n",
          CG_KeyNameForCommand( "weapnext" ) ) );
  }
  else
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT,
        va( "Press %s to follow a player\n",
            CG_KeyNameForCommand( "+button2" ) ) );
  }
}

#define BINDING_REFRESH_INTERVAL 30

/*
===============
CG_TutorialText

Returns context help for the current class/weapon
===============
*/

const char *CG_TutorialText( void )
{
  playerState_t *ps;
  static char   text[ MAX_TUTORIAL_TEXT ];
  static int    refreshBindings = 0;

  if( refreshBindings == 0 )
    CG_GetBindings( );

  refreshBindings = ( refreshBindings + 1 ) % BINDING_REFRESH_INTERVAL;

  text[ 0 ] = '\0';
  ps = &cg.snap->ps;

  if( !cg.intermissionStarted && !cg.demoPlayback )
  {
    if( ps->persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT ||
        ps->pm_flags & PMF_FOLLOW )
    {
      CG_SpectatorText( text, ps );
    }
    else if( ps->misc[ MISC_HEALTH ] > 0 )
    {
      switch( ps->stats[ STAT_CLASS ] )
      {
        case PCL_ALIEN_BUILDER0:
        case PCL_ALIEN_BUILDER0_UPG:
          CG_AlienBuilderText( text, ps );
          break;

        case PCL_ALIEN_LEVEL0:
          CG_AlienLevel0Text( text, ps );
          break;

        case PCL_ALIEN_LEVEL1:
        case PCL_ALIEN_LEVEL1_UPG:
          CG_AlienLevel1Text( text, ps );
          break;

        case PCL_ALIEN_LEVEL2:
        case PCL_ALIEN_LEVEL2_UPG:
          CG_AlienLevel2Text( text, ps );
          break;

        case PCL_ALIEN_SPITFIRE:
          CG_AlienSpitfireText( text, ps );
          break;

        case PCL_ALIEN_LEVEL3:
        case PCL_ALIEN_LEVEL3_UPG:
          CG_AlienLevel3Text( text, ps );
          break;

        case PCL_ALIEN_LEVEL4:
          CG_AlienLevel4Text( text, ps );
          break;

        case PCL_HUMAN:
        case PCL_HUMAN_BSUIT:
          CG_HumanText( text, ps );
          break;

        default:
          break;
      }

      if( ps->eFlags & EF_OCCUPYING )
      {
        switch( cg.nearUsableBuildable )
        {
          case BA_NONE:
            Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to leave the structure\n",
                CG_KeyNameForCommand( "+button7" ) ) );
            break;
          case BA_H_TELEPORTER:
            Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to cancel teleportation\n",
                CG_KeyNameForCommand( "+button7" ) ) );
              break;
          default:
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to exit the %s\n",
                CG_KeyNameForCommand( "+button7" ),
                BG_Buildable( cg.nearUsableBuildable )->humanName ) );
          break;
        }
      } else if( ps->persistant[ PERS_ACT_ENT ] != ENTITYNUM_NONE )
      {
        switch( cg.nearUsableBuildable )
        {
          case BA_A_HOVEL:
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to enter the hovel\n",
                  CG_KeyNameForCommand( "+button7" ) ) );
            break;
          case BA_H_TELEPORTER:
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to teleport\n",
                  CG_KeyNameForCommand( "+button7" ) ) );
            break;
          case BA_H_ARMOURY:
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to buy equipment upgrades at the %s. Sell your old weapon first!\n",
                  CG_KeyNameForCommand( "+button7" ),
                  BG_Buildable( cg.nearUsableBuildable )->humanName ) );
            break;
          case BA_H_REPEATER:
          case BA_H_REACTOR:
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to refill your energy weapon's ammo at the %s\n",
                  CG_KeyNameForCommand( "+button7" ),
                  BG_Buildable( cg.nearUsableBuildable )->humanName ) );
            break;
          case BA_NONE:
            Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to activate the structure\n",
                CG_KeyNameForCommand( "+button7" ) ) );
            break;
          default:
            Q_strcat( text, MAX_TUTORIAL_TEXT,
                va( "Press %s to activate the %s\n",
                  CG_KeyNameForCommand( "+button7" ),
                  BG_Buildable( cg.nearUsableBuildable )->humanName ) );
            break;
        }
      } else if( ps->stats[ STAT_TEAM ] == TEAM_ALIENS )
      {
        if( BG_AlienCanEvolve( ps->stats[ STAT_CLASS ],
                               ps->persistant[ PERS_CREDIT ],
                               cgs.alienStage,
                               cgs.warmup, cgs.devMode,
                               (ps->stats[STAT_FLAGS] & SFL_CLASS_FORCED)) )
        {
          Q_strcat( text, MAX_TUTORIAL_TEXT,
              va( "Press %s to evolve\n",
                CG_KeyNameForCommand( "+button7" ) ) );
        }
      }
    }
  }
  else if( !cg.demoPlayback )
  {
    if( !CG_ClientIsReady( ps->clientNum ) )
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT,
          va( "Press %s when ready to continue\n",
            CG_KeyNameForCommand( "+attack" ) ) );
    }
    else
    {
      Q_strcat( text, MAX_TUTORIAL_TEXT, "Waiting for other players to be ready\n" );
    }
  }

  if( !cg.demoPlayback )
  {
    Q_strcat( text, MAX_TUTORIAL_TEXT, "Press ESC for the menu" );
  }

  return text;
}
