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

// bg_misc.c -- both games misc functions, all completely stateless

#include "../qcommon/q_shared.h"
#include "bg_public.h"

/*
============
BG_HP2SU

Convert health point into health subunits
============
*/
int BG_HP2SU( int healthPoints )
{
  if( healthPoints < ( INT_MAX / HEALTH_POINT_SUBUNIT_FACTOR ) )
    return healthPoints * HEALTH_POINT_SUBUNIT_FACTOR;

  // prevent integer overflow
  return INT_MAX;
}

/*
============
BG_SU2HP

Convert health subunits to health points
============
*/
int BG_SU2HP( int healthSubUnits )
{
  int healthPoints = healthSubUnits / HEALTH_POINT_SUBUNIT_FACTOR;

  // only let healthPoints = 0 if healthSubUnits is 0
  if( !healthPoints &&
      healthSubUnits )
    return 1;

  return healthPoints;
}

#ifdef Q3_VM
  #define FS_FOpenFileByMode trap_FS_FOpenFile
  #define FS_Read2 trap_FS_Read
  #define FS_Write trap_FS_Write
  #define FS_FCloseFile trap_FS_FCloseFile
  #define FS_Seek trap_FS_Seek
  #define FS_GetFileList trap_FS_GetFileList
  #define Cvar_VariableStringBuffer trap_Cvar_VariableStringBuffer
#else
  int  FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode );
  int  FS_Read2( void *buffer, int len, fileHandle_t f );
  void FS_FCloseFile( fileHandle_t f );
  int  FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize );
#endif

static entityState_t *bg_entityStates[MAX_GENTITIES];

entityState_t *BG_EntityState(int ent_num) {
  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  return bg_entityStates[ent_num];
}

void BG_Link_entityState(entityState_t *es, int ent_num) {
  Com_Assert(es);
  Com_Assert(ent_num >= 0 && ent_num < MAX_GENTITIES);

  bg_entityStates[ent_num] = es;
}

static const teamAttributes_t bg_teamList[ ] =
{
  {
    TEAM_NONE,             //int       number;
    "spectate",            //char     *name;
    "spectator",           //char     *name2;
    "Spectators",          //char     *humanName;
    "Spectators watch the game without participating.",
  },
  {
    TEAM_ALIENS,           //int       number;
    "aliens",              //char     *name;
    "alien",               //char     *name2;
    "Aliens",              //char     *humanName;
    "The strength of Aliens lie in their agility, fierce melee attacks and "
    "their ability to construct new bases without much restriction. They "
    "possess disorienting and lethal secondary attack skills such as "
    "psychotropic gas, electrical discharge, pouncing and trampling. Left "
    "to thrive, they may develop the capability of inflicting cripling "
    "poisons.",
  },
  {
    TEAM_HUMANS,           //int       number;
    "humans",              //char     *name;
    "human",               //char     *name2;
    "Humans",              //char     *humanName;
    "Humans are the masters of technology. Although their bases are "
    "restricted by power requirements, automated defenses help ensure that "
    "they stay built. To compensate for their lack of natural abilities, a "
    "wide range of upgrades and weapons are available to the humans, allowing "
    "them to eradicate the alien threat with lethal efficiency.",
  },
};

size_t bg_numTeams = ARRAY_LEN( bg_teamList );

static const teamAttributes_t nullTeam = { 0 };

static const rankAttributes_t bg_rankList[ ] =
{
  {
    RANK_NONE,       //rank_t number;
    "",              //char   *name;
    ""               //char   *humanName;
  },
  {
    RANK_CAPTAIN,    //rank_t number;
    "Cpt.",          //char   *name;
    "Captain"        //char   *humanName;
  }
};

size_t bg_numRanks = ARRAY_LEN( bg_rankList );

static const rankAttributes_t nullRank = { 0 };

//
// XXX This MUST match the order of "buildable_t"!
//
static const buildableAttributes_t bg_buildableList[ ] =
{
  {
    BA_A_SPAWN,            //int       number;
    qtrue,                 //qboolean  enabled;
    "eggpod",              //char      *name;
    "Egg",                 //char      *humanName;
    "The most basic alien structure. It allows aliens to spawn "
      "and protect the Overmind. Without any of these, the Overmind "
      "is left nearly defenseless and defeat is imminent.",
    "team_alien_spawn",    //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    ASPAWN_BP,             //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    ASPAWN_HEALTH,         //int       health;
    ASPAWN_REGEN,          //int       regenRate;
    ASPAWN_SPLASHDAMAGE,   //int       splashDamage;
    ASPAWN_SPLASHRADIUS,   //int       splashRadius;
    MOD_ASPAWN,            //int       meansOfDeath;
    TEAM_ALIENS,           //int       team;
    ( 1 << WP_ABUILD )|( 1 << WP_ABUILD2 ), //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    100,                   //int       nextthink;
    ASPAWN_BT,             //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.5f,                  //float     minNormal;
    qtrue,                 //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    ASPAWN_CREEPSIZE,      //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    ASPAWN_VALUE,          //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    CREEP_BASESIZE,        //float             rangeMarkerRange;
    SHC_LIGHT_GREEN,       //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SPAWN|
     ROLE_SUPPORT|
     ROLE_POWER_SOURCE|
     ROLE_PERVASIVE)       //int               role;
  },
  {
    BA_A_OVERMIND,         //int       number;
    qtrue,                 //qboolean  enabled;
    "overmind",            //char      *name;
    "Overmind",            //char      *humanName;
    "A collective consciousness that controls all the alien structures "
      "in its vicinity. It must be protected at all costs, since its "
      "death will render alien structures defenseless.",
    "team_alien_overmind", //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    OVERMIND_BP,           //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    OVERMIND_HEALTH,       //int       health;
    OVERMIND_REGEN,        //int       regenRate;
    OVERMIND_SPLASHDAMAGE, //int       splashDamage;
    OVERMIND_SPLASHRADIUS, //int       splashRadius;
    MOD_ASPAWN,            //int       meansOfDeath;
    TEAM_ALIENS,           //int       team;
    ( 1 << WP_ABUILD )|( 1 << WP_ABUILD2 ), //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    OVERMIND_ATTACK_REPEAT, //int      nextthink;
    OVERMIND_BT,           //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.0f,                 //float     minNormal;
    qtrue,                 //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    OVERMIND_CREEPSIZE,    //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qtrue,                 //qboolean  uniqueTest;
    OVERMIND_VALUE,        //int       value;
    qtrue,                 //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    CREEP_BASESIZE,        //float             rangeMarkerRange;
    SHC_DARK_GREEN,        //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_CORE|
     ROLE_SUPPORT|
     ROLE_POWER_SOURCE|
     ROLE_PERVASIVE)       //int               role;
  },
  {
    BA_A_BARRICADE,        //int       number;
    qtrue,                 //qboolean  enabled;
    "barricade",           //char      *name;
    "Barricade",           //char      *humanName;
    "Used to obstruct corridors and doorways, hindering humans from "
      "threatening the spawns and Overmind. Barricades will shrink "
      "to allow aliens to pass over them, however.",
    "team_alien_barricade", //char     *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    BARRICADE_BP,          //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    BARRICADE_HEALTH,      //int       health;
    BARRICADE_REGEN,       //int       regenRate;
    BARRICADE_SPLASHDAMAGE, //int      splashDamage;
    BARRICADE_SPLASHRADIUS, //int      splashRadius;
    MOD_ASPAWN,            //int       meansOfDeath;
    TEAM_ALIENS,           //int       team;
    ( 1 << WP_ABUILD )|( 1 << WP_ABUILD2 ), //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    100,                   //int       nextthink;
    BARRICADE_BT,          //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.707f,                //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qtrue,                 //qboolean  creepTest;
    BARRICADE_CREEPSIZE,   //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    BARRICADE_VALUE,       //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    0.0f,                  //float             rangeMarkerRange;
    SHC_GREY,              //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SUPPORT)         //int               role;
  },
  {
    BA_A_ACIDTUBE,         //int       number;
    qtrue,                 //qboolean  enabled;
    "acid_tube",           //char      *name;
    "Acid Tube",           //char      *humanName;
    "Ejects lethal poisonous acid at an approaching human. These "
      "are highly effective when used in conjunction with a trapper "
      "to hold the victim in place.",
    "team_alien_acid_tube", //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    ACIDTUBE_BP,           //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    ACIDTUBE_HEALTH,       //int       health;
    ACIDTUBE_REGEN,        //int       regenRate;
    ACIDTUBE_SPLASHDAMAGE, //int       splashDamage;
    ACIDTUBE_SPLASHRADIUS, //int       splashRadius;
    MOD_ASPAWN,            //int       meansOfDeath;
    TEAM_ALIENS,           //int       team;
    ( 1 << WP_ABUILD )|( 1 << WP_ABUILD2 ), //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    200,                   //int       nextthink;
    ACIDTUBE_BT,           //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.0f,                  //float     minNormal;
    qtrue,                 //qboolean  invertNormal;
    qtrue,                 //qboolean  creepTest;
    ACIDTUBE_CREEPSIZE,    //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    ACIDTUBE_VALUE,        //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    ACIDTUBE_RANGE,        //float             rangeMarkerRange;
    SHC_RED,               //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_OFFENSE)         //int               role;
  },
  {
    BA_A_TRAPPER,          //int       number;
    qtrue,                 //qboolean  enabled;
    "trapper",             //char      *name;
    "Trapper",             //char      *humanName;
    "Fires a blob of adhesive spit at any non-alien in its line of "
      "sight. This hinders their movement, making them an easy target "
      "for other defensive structures or aliens.",
    "team_alien_trapper",  //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    TRAPPER_BP,            //int       buildPoints;
    ( 1 << S2 )|( 1 << S3 ), //int     stages; //NEEDS ADV BUILDER SO S2 AND UP
    TRAPPER_HEALTH,        //int       health;
    TRAPPER_REGEN,         //int       regenRate;
    TRAPPER_SPLASHDAMAGE,  //int       splashDamage;
    TRAPPER_SPLASHRADIUS,  //int       splashRadius;
    MOD_ASPAWN,            //int       meansOfDeath;
    TEAM_ALIENS,           //int       team;
    ( 1 << WP_ABUILD2 ),   //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    100,                   //int       nextthink;
    TRAPPER_BT,            //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    TRAPPER_RANGE,         //int       turretRange;
    TRAPPER_REPEAT,        //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_LOCKBLOB_LAUNCHER,  //weapon_t  turretProjType;
    0.0f,                  //float     minNormal;
    qtrue,                 //qboolean  invertNormal;
    qtrue,                 //qboolean  creepTest;
    TRAPPER_CREEPSIZE,     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qtrue,                 //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    TRAPPER_VALUE,         //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERICAL_CONE_64, //rangeMarkerType_t rangeMarkerType;
    TRAPPER_RANGE,         //float             rangeMarkerRange;
    SHC_PINK,              //shaderColorEnum_t rangeMarkerColor;
    qtrue,                 //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_OFFENSE)         //int               role;
  },
  {
    BA_A_BOOSTER,          //int       number;
    qtrue,                 //qboolean  enabled;
    "booster",             //char      *name;
    "Booster",             //char      *humanName;
    "Laces the attacks of any alien that touches it with a poison "
      "that will gradually deal damage to any humans exposed to it. "
      "The booster also increases the rate of health regeneration for "
      "any nearby aliens.",
    "team_alien_booster",  //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    BOOSTER_BP,            //int       buildPoints;
    ( 1 << S2 )|( 1 << S3 ), //int     stages;
    BOOSTER_HEALTH,        //int       health;
    BOOSTER_REGEN,         //int       regenRate;
    BOOSTER_SPLASHDAMAGE,  //int       splashDamage;
    BOOSTER_SPLASHRADIUS,  //int       splashRadius;
    MOD_ASPAWN,            //int       meansOfDeath;
    TEAM_ALIENS,           //int       team;
    ( 1 << WP_ABUILD2 ),   //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    100,                   //int       nextthink;
    BOOSTER_BT,            //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.707f,                //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qtrue,                 //qboolean  creepTest;
    BOOSTER_CREEPSIZE,     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qtrue,                 //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    BOOSTER_VALUE,         //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    0.0f,                  //float             rangeMarkerRange;
    SHC_GREY,              //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SUPPORT|
     ROLE_PERVASIVE)       //int               role;
  },
  {
    BA_A_HIVE,             //int       number;
    qtrue,                 //qboolean  enabled;
    "hive",                //char      *name;
    "Hive",                //char      *humanName;
    "Houses millions of tiny insectoid aliens. When a human "
      "approaches this structure, the insectoids attack.",
    "team_alien_hive",     //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    HIVE_BP,               //int       buildPoints;
    ( 1 << S3 ),           //int       stages;
    HIVE_HEALTH,           //int       health;
    HIVE_REGEN,            //int       regenRate;
    HIVE_SPLASHDAMAGE,     //int       splashDamage;
    HIVE_SPLASHRADIUS,     //int       splashRadius;
    MOD_ASPAWN,            //int       meansOfDeath;
    TEAM_ALIENS,           //int       team;
    ( 1 << WP_ABUILD2 ),   //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    500,                   //int       nextthink;
    HIVE_BT,               //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_HIVE,               //weapon_t  turretProjType;
    0.0f,                  //float     minNormal;
    qtrue,                 //qboolean  invertNormal;
    qtrue,                 //qboolean  creepTest;
    HIVE_CREEPSIZE,        //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    HIVE_VALUE,            //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    HIVE_SENSE_RANGE,      //float             rangeMarkerRange;
    SHC_YELLOW,            //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qtrue,                 //qboolean          rangeMarkerOriginAtTop;
    (ROLE_OFFENSE)         //int               role;
  },
  {
    BA_A_HOVEL,            //int       buildNum;
    qtrue,                 //qboolean  enabled;
    "hovel",               //char      *buildName;
    "Hovel",               //char      *humanName;
    "Houses grangers",
    "team_alien_hovel",    //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.0,                   //float     bounce;
    HOVEL_BP,              //int       buildPoints;
    ( 1 << S3 ),           //int  stages
    HOVEL_HEALTH,          //int       health;
    HOVEL_REGEN,           //int       regenRate;
    HOVEL_SPLASHDAMAGE,    //int       splashDamage;
    HOVEL_SPLASHRADIUS,    //int       splashRadius;
    MOD_ASPAWN,            //int       meansOfDeath;
    TEAM_ALIENS,           //int       team;
    ( 1 << WP_ABUILD2 ),   //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    150,                   //int       nextthink;
    HOVEL_BT,              //int       buildTime;
    qtrue,                 //qboolean  activationEnt;
    (ACTF_TEAM|
     ACTF_ENT_ALIVE|
     ACTF_PL_ALIVE|
     ACTF_SPAWNED|
     ACTF_LINE_OF_SIGHT|
     ACTF_OCCUPY),         //int       activationFlags;
    (OCCUPYF_PM_TYPE|
     OCCUPYF_CONTENTS|
     OCCUPYF_CLIPMASK),    //int       occupationFlags;
    PM_FREEZE,             //pmtype_t  activationPm_type;
    MASK_ASTRALSOLID,      //int       activationContents;
    CONTENTS_ASTRAL_NOCLIP,//int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.95f,                 //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qtrue,                 //qboolean  creepTest;
    HOVEL_CREEPSIZE,       //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qtrue,                 //qboolean  uniqueTest;
    HOVEL_VALUE,           //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    0.0f,                  //float             rangeMarkerRange;
    SHC_GREY,              //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SUPPORT)         //int               role;
  },
  {
    BA_H_SPAWN,            //int       number;
    qtrue,                 //qboolean  enabled;
    "telenode",            //char      *name;
    "Telenode",            //char      *humanName;
    "The most basic human structure. It provides a means for humans "
      "to enter the battle arena. Without any of these the humans "
      "cannot spawn and defeat is imminent.",
    "team_human_spawn",    //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    HSPAWN_BP,             //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    HSPAWN_HEALTH,         //int       health;
    0,                     //int       regenRate;
    HSPAWN_SPLASHDAMAGE,   //int       splashDamage;
    HSPAWN_SPLASHRADIUS,   //int       splashRadius;
    MOD_HSPAWN,            //int       meansOfDeath;
    TEAM_HUMANS,           //int       team;
    ( 1 << WP_HBUILD ),    //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    100,                   //int       nextthink;
    HSPAWN_BT,             //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.95f,                 //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    0,                     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qtrue,                 //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    HSPAWN_VALUE,          //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    0.0f,                  //float             rangeMarkerRange;
    SHC_GREY,              //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SPAWN|
     ROLE_SUPPORT)         //int               role;
  },
  {
    BA_H_MGTURRET,         //int       number;
    qtrue,                 //qboolean  enabled;
    "mgturret",            //char      *name;
    "Machinegun Turret",   //char      *humanName;
    "Automated base defense that is effective against large targets "
      "but slow to begin firing. Should always be "
      "backed up by physical support.",
    "team_human_mgturret", //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    MGTURRET_BP,           //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    MGTURRET_HEALTH,       //int       health;
    0,                     //int       regenRate;
    MGTURRET_SPLASHDAMAGE, //int       splashDamage;
    MGTURRET_SPLASHRADIUS, //int       splashRadius;
    MOD_HSPAWN,            //int       meansOfDeath;
    TEAM_HUMANS,           //int       team;
    ( 1 << WP_HBUILD ),    //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    50,                    //int       nextthink;
    MGTURRET_BT,           //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    MGTURRET_RANGE,        //int       turretRange;
    MGTURRET_REPEAT,       //int       turretFireSpeed;
    qtrue,                 //qboolean  turretTrackOnlyOrigin;
    WP_MGTURRET,           //weapon_t  turretProjType;
    0.95f,                 //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    0,                     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qtrue,                 //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    MGTURRET_VALUE,        //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERICAL_CONE_240,//rangeMarkerType_t rangeMarkerType;
    MGTURRET_RANGE,        //float             rangeMarkerRange;
    SHC_ORANGE,            //shaderColorEnum_t rangeMarkerColor;
    qtrue,                 //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_OFFENSE)         //int               role;
  },
  {
    BA_H_TESLAGEN,         //int       number;
    qtrue,                 //qboolean  enabled;
    "tesla",               //char      *name;
    "Tesla Generator",     //char      *humanName;
    "A structure equipped with a strong electrical attack that fires "
      "instantly and always hits its target. It is effective against smaller "
      "aliens and for consolidating basic defense.",
    "team_human_tesla",    //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    TESLAGEN_BP,           //int       buildPoints;
    ( 1 << S3 ),           //int       stages;
    TESLAGEN_HEALTH,       //int       health;
    0,                     //int       regenRate;
    TESLAGEN_SPLASHDAMAGE, //int       splashDamage;
    TESLAGEN_SPLASHRADIUS, //int       splashRadius;
    MOD_HSPAWN,            //int       meansOfDeath;
    TEAM_HUMANS,           //int       team;
    ( 1 << WP_HBUILD ),    //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    150,                   //int       nextthink;
    TESLAGEN_BT,           //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    TESLAGEN_RANGE,        //int       turretRange;
    TESLAGEN_REPEAT,       //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_TESLAGEN,           //weapon_t  turretProjType;
    0.95f,                 //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    0,                     //int       creepSize;
    qtrue,                 //qboolean  dccTest;
    qtrue,                 //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    TESLAGEN_VALUE,        //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    TESLAGEN_RANGE,        //float             rangeMarkerRange;
    SHC_VIOLET,            //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qtrue,                 //qboolean          rangeMarkerOriginAtTop;
    (ROLE_OFFENSE)         //int               role;
  },
  {
    BA_H_ARMOURY,          //int       number;
    qtrue,                 //qboolean  enabled;
    "arm",                 //char      *name;
    "Armoury",             //char      *humanName;
    "An essential part of the human base, providing a means "
      "to upgrade the basic human equipment. A range of upgrades "
      "and weapons are available for sale from the armoury.",
    "team_human_armoury",  //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    ARMOURY_BP,            //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    ARMOURY_HEALTH,        //int       health;
    0,                     //int       regenRate;
    ARMOURY_SPLASHDAMAGE,  //int       splashDamage;
    ARMOURY_SPLASHRADIUS,  //int       splashRadius;
    MOD_HSPAWN,            //int       meansOfDeath;
    TEAM_HUMANS,           //int       team;
    ( 1 << WP_HBUILD ),    //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    100,                   //int       nextthink;
    ARMOURY_BT,            //int       buildTime;
    qtrue,                 //qboolean  activationEnt;
    (ACTF_TEAM|
     ACTF_ENT_ALIVE|
     ACTF_PL_ALIVE|
     ACTF_SPAWNED|
     ACTF_POWERED),        //int activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.95f,                 //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    0,                     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    ARMOURY_VALUE,         //int       value;
    qtrue,                 //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    0.0f,                  //float             rangeMarkerRange;
    SHC_GREY,              //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SUPPORT)         //int               role;
  },
  {
    BA_H_DCC,              //int       number;
    qtrue,                 //qboolean  enabled;
    "dcc",                 //char      *name;
    "Defense Computer",    //char      *humanName;
    "A structure coordinating and enhancing the action of base defense so that "
      "defense is distributed optimally among the enemy. The Defense "
      "Computer additionally eneables auto-repair in human structures.",
    "team_human_dcc",      //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    DC_BP,                 //int       buildPoints;
    ( 1 << S2 )|( 1 << S3 ), //int     stages;
    DC_HEALTH,             //int       health;
    0,                     //int       regenRate;
    DC_SPLASHDAMAGE,       //int       splashDamage;
    DC_SPLASHRADIUS,       //int       splashRadius;
    MOD_HSPAWN,            //int       meansOfDeath;
    TEAM_HUMANS,           //int       team;
    ( 1 << WP_HBUILD ),    //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    100,                   //int       nextthink;
    DC_BT,                 //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.5f,                  //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    0,                     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qtrue,                 //qboolean  uniqueTest;
    DC_VALUE,              //int       value;
    qtrue,                 //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    DC_RANGE,              //float             rangeMarkerRange;
    SHC_GREEN_CYAN,        //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SUPPORT|
     ROLE_PERVASIVE)       //int               role;
  },
  {
    BA_H_MEDISTAT,         //int       number;
    qtrue,                 //qboolean  enabled;
    "medistat",            //char      *name;
    "Medistation",         //char      *humanName;
    "A structure that automatically restores "
      "the health and stamina of any human that stands on it. "
      "It may only be used by one person at a time. This structure "
      "also issues medkits.",
    "team_human_medistat", //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    MEDISTAT_BP,           //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    MEDISTAT_HEALTH,       //int       health;
    0,                     //int       regenRate;
    MEDISTAT_SPLASHDAMAGE, //int       splashDamage;
    MEDISTAT_SPLASHRADIUS, //int       splashRadius;
    MOD_HSPAWN,            //int       meansOfDeath;
    TEAM_HUMANS,           //int       team;
    ( 1 << WP_HBUILD ),    //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    MEDISTAT_REPEAT,       //int       nextthink;
    MEDISTAT_BT,           //int       buildTime;
    qfalse,                //qboolean  activationEnt;
    0,                     //int       activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.95f,                 //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    0,                     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qtrue,                 //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    MEDISTAT_VALUE,        //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    0.0f,                  //float             rangeMarkerRange;
    SHC_GREY,              //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SUPPORT)         //int               role;
  },
  {
    BA_H_REACTOR,          //int       number;
    qtrue,                 //qboolean  enabled;
    "reactor",             //char      *name;
    "Reactor",             //char      *humanName;
    "All structures except the telenode rely on a reactor to operate. "
      "The reactor provides power for all the human structures either "
      "directly or via repeaters. Only one reactor can be built at a time.",
    "team_human_reactor",  //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    REACTOR_BP,            //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    REACTOR_HEALTH,        //int       health;
    0,                     //int       regenRate;
    REACTOR_SPLASHDAMAGE,  //int       splashDamage;
    REACTOR_SPLASHRADIUS,  //int       splashRadius;
    MOD_HSPAWN,            //int       meansOfDeath;
    TEAM_HUMANS,           //int       team;
    ( 1 << WP_HBUILD ),    //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    REACTOR_ATTACK_DCC_REPEAT, //int   nextthink;
    REACTOR_BT,            //int       buildTime;
    qtrue,                 //qboolean  activationEnt;
    (ACTF_TEAM|
     ACTF_ENT_ALIVE|
     ACTF_PL_ALIVE|
     ACTF_SPAWNED|
     ACTF_POWERED),        //int activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.95f,                 //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    0,                     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qtrue,                 //qboolean  uniqueTest;
    REACTOR_VALUE,         //int       value;
    qtrue,                 //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    REACTOR_BASESIZE,      //float             rangeMarkerRange;
    SHC_DARK_BLUE,         //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_CORE|
     ROLE_SUPPORT|
     ROLE_POWER_SOURCE|
     ROLE_PERVASIVE)       //int               role;
  },
  {
    BA_H_REPEATER,         //int       number;
    qtrue,                 //qboolean  enabled;
    "repeater",            //char      *name;
    "Repeater",            //char      *humanName;
    "A power distributor that transmits power from the reactor "
      "to remote locations, so that bases may be built far "
      "from the reactor.",
    "team_human_repeater", //char      *entityName;
    TR_GRAVITY,            //trType_t  traj;
    0.1,                   //float     bounce;
    REPEATER_BP,           //int       buildPoints;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    REPEATER_HEALTH,       //int       health;
    0,                     //int       regenRate;
    REPEATER_SPLASHDAMAGE, //int       splashDamage;
    REPEATER_SPLASHRADIUS, //int       splashRadius;
    MOD_HSPAWN,            //int       meansOfDeath;
    TEAM_HUMANS,           //int       team;
    ( 1 << WP_HBUILD ),    //weapon_t  buildWeapon;
    BANIM_IDLE1,           //int       idleAnim;
    100,                   //int       nextthink;
    REPEATER_BT,           //int       buildTime;
    qtrue,                 //qboolean  activationEnt;
    (ACTF_TEAM|
     ACTF_ENT_ALIVE|
     ACTF_PL_ALIVE|
     ACTF_SPAWNED|
     ACTF_POWERED),        //int activationFlags;
    0,                     //int       occupationFlags;
    PM_NORMAL,             //pmtype_t  activationPm_type;
    MASK_PLAYERSOLID,      //int       activationContents;
    CONTENTS_BODY,         //int       activationClipMask;
    0,                     //int       turretRange;
    0,                     //int       turretFireSpeed;
    qfalse,                //qboolean  turretTrackOnlyOrigin;
    WP_NONE,               //weapon_t  turretProjType;
    0.95f,                 //float     minNormal;
    qfalse,                //qboolean  invertNormal;
    qfalse,                //qboolean  creepTest;
    0,                     //int       creepSize;
    qfalse,                //qboolean  dccTest;
    qfalse,                //qboolean  transparentTest;
    qfalse,                //qboolean  uniqueTest;
    REPEATER_VALUE,        //int       value;
    qfalse,                //qboolean  stackable;
    RMT_SPHERE,            //rangeMarkerType_t rangeMarkerType;
    REPEATER_BASESIZE,     //float             rangeMarkerRange;
    SHC_LIGHT_BLUE,        //shaderColorEnum_t rangeMarkerColor;
    qfalse,                //qboolean          rangeMarkerUseNormal;
    qfalse,                //qboolean          rangeMarkerOriginAtTop;
    (ROLE_SUPPORT|
     ROLE_POWER_SOURCE|
     ROLE_PERVASIVE)       //int               role;
  }
};

size_t bg_numBuildables = ARRAY_LEN( bg_buildableList );

static const buildableAttributes_t nullBuildable = { 0 };

/*
==============
BG_BuildableByName
==============
*/
const buildableAttributes_t *BG_BuildableByName( const char *name )
{
  int i;

  for( i = 0; i < bg_numBuildables; i++ )
  {
    if( !Q_stricmp( bg_buildableList[ i ].name, name ) )
      return &bg_buildableList[ i ];
  }

  return &nullBuildable;
}

/*
==============
BG_BuildableByEntityName
==============
*/
const buildableAttributes_t *BG_BuildableByEntityName( const char *name )
{
  int i;

  for( i = 0; i < bg_numBuildables; i++ )
  {
    if( !Q_stricmp( bg_buildableList[ i ].entityName, name ) )
      return &bg_buildableList[ i ];
  }

  return &nullBuildable;
}

/*
==============
BG_Buildable
==============
*/
const buildableAttributes_t *BG_Buildable( buildable_t buildable )
{
  return ( buildable > BA_NONE && buildable < BA_NUM_BUILDABLES ) ?
    &bg_buildableList[ buildable - 1 ] : &nullBuildable;
}

/*
==============
BG_BuildableAllowedInStage
==============
*/
qboolean BG_BuildableAllowedInStage( buildable_t buildable,
                                     stage_t stage,
                                     int gameIsInWarmup  )
{
  int stages = BG_Buildable( buildable )->stages;

  if( gameIsInWarmup )
    return qtrue;

  if( stages & ( 1 << stage ) )
    return qtrue;
  else
    return qfalse;
}

static buildableConfig_t bg_buildableConfigList[ BA_NUM_BUILDABLES ];

/*
==============
BG_BuildableConfig
==============
*/
buildableConfig_t *BG_BuildableConfig( buildable_t buildable )
{
  return &bg_buildableConfigList[ buildable ];
}

/*
==============
BG_BuildableBoundingBox
==============
*/
void BG_BuildableBoundingBox( buildable_t buildable,
                              vec3_t mins, vec3_t maxs )
{
  buildableConfig_t *buildableConfig = BG_BuildableConfig( buildable );

  if( mins != NULL )
    VectorCopy( buildableConfig->mins, mins );

  if( maxs != NULL )
    VectorCopy( buildableConfig->maxs, maxs );
}

/*
======================
BG_ParseBuildableFile

Parses a configuration file describing a buildable
======================
*/
static qboolean BG_ParseBuildableFile( const char *filename, buildableConfig_t *bc )
{
  char          *text_p;
  int           i;
  int           len;
  char          *token;
  char          text[ 20000 ];
  fileHandle_t  f;
  float         scale;
  int           defined = 0;
  enum
  {
      MODEL         = 1 << 0,
      MODELSCALE    = 1 << 1,
      MINS          = 1 << 2,
      MAXS          = 1 << 3,
      ZOFFSET       = 1 << 4
  };


  // load the file
  len = FS_FOpenFileByMode( filename, &f, FS_READ );
  if( len < 0 )
  {
    Com_Printf( S_COLOR_RED "ERROR: Buildable file %s doesn't exist\n", filename );
    return qfalse;
  }

  if( len == 0 || len >= sizeof( text ) - 1 )
  {
    FS_FCloseFile( f );
    Com_Printf( S_COLOR_RED "ERROR: Buildable file %s is %s\n", filename,
      len == 0 ? "empty" : "too long" );
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

    if( !token )
      break;

    if( !Q_stricmp( token, "" ) )
      break;

    if( !Q_stricmp( token, "model" ) )
    {
      int index = 0;

      token = COM_Parse( &text_p );
      if( !token )
        break;

      index = atoi( token );

      if( index < 0 )
        index = 0;
      else if( index > 3 )
        index = 3;

      token = COM_Parse( &text_p );
      if( !token )
        break;

      Q_strncpyz( bc->models[ index ], token, sizeof( bc->models[ 0 ] ) );

      defined |= MODEL;
      continue;
    }
    else if( !Q_stricmp( token, "modelScale" ) )
    {
      token = COM_Parse( &text_p );
      if( !token )
        break;

      scale = atof( token );

      if( scale < 0.0f )
        scale = 0.0f;

      bc->modelScale = scale;

      defined |= MODELSCALE;
      continue;
    }
    else if( !Q_stricmp( token, "mins" ) )
    {
      for( i = 0; i <= 2; i++ )
      {
        token = COM_Parse( &text_p );
        if( !token )
          break;

        bc->mins[ i ] = atof( token );
      }

      defined |= MINS;
      continue;
    }
    else if( !Q_stricmp( token, "maxs" ) )
    {
      for( i = 0; i <= 2; i++ )
      {
        token = COM_Parse( &text_p );
        if( !token )
          break;

        bc->maxs[ i ] = atof( token );
      }

      defined |= MAXS;
      continue;
    }
    else if( !Q_stricmp( token, "zOffset" ) )
    {
      float offset;

      token = COM_Parse( &text_p );
      if( !token )
        break;

      offset = atof( token );

      bc->zOffset = offset;

      defined |= ZOFFSET;
      continue;
    }


    Com_Printf( S_COLOR_RED "ERROR: unknown token '%s'\n", token );
    return qfalse;
  }

  if(      !( defined & MODEL      ) )  token = "model";
  else if( !( defined & MODELSCALE ) )  token = "modelScale";
  else if( !( defined & MINS       ) )  token = "mins";
  else if( !( defined & MAXS       ) )  token = "maxs";
  else if( !( defined & ZOFFSET    ) )  token = "zOffset";
  else                                  token = "";

  if( strlen( token ) > 0 )
  {
      Com_Printf( S_COLOR_RED "ERROR: %s not defined in %s\n",
                  token, filename );
      return qfalse;
  }

  return qtrue;
}

/*
===============
BG_InitBuildableConfigs
===============
*/
void BG_InitBuildableConfigs( void )
{
  int               i;
  buildableConfig_t *bc;

  for( i = BA_NONE + 1; i < BA_NUM_BUILDABLES; i++ )
  {
    bc = BG_BuildableConfig( i );
    Com_Memset( bc, 0, sizeof( buildableConfig_t ) );

    BG_ParseBuildableFile( va( "configs/buildables/%s.cfg",
                               BG_Buildable( i )->name ), bc );
  }
}

////////////////////////////////////////////////////////////////////////////////

static const classAttributes_t bg_classList[ ] =
{
  {
    PCL_NONE,                                       //int     number;
    qtrue,                                          //qboolean enabled;
    "spectator",                                    //char    *name;
    "",
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ),            //int     stages;
    100000,                                         //int     health;
    0.0f,                                           //float   fallDamage;
    0.0f,                                           //float   regenRate;
    0,                                              //int     abilities;
    WP_NONE,                                        //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    90,                                             //int     fov;
    0.000f,                                         //float   bob;
    1.0f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    0,                                              //int     steptime;
    600,                                            //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    100.0f,                                         //float   stopSpeed;
    270.0f,                                         //float   jumpMagnitude;
    1.0f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_NONE, PCL_NONE, PCL_NONE },               //int     children[ 3 ];
    0,                                              //int     cost;
    qtrue,                                          //qboolean warmupFree;
    0,                                              //int     value;
    TEAM_NONE                                       //team_t  team;
  },
  {
    PCL_ALIEN_BUILDER0,                             //int     number;
    qtrue,                                          //qboolean enabled;
    "builder",                                      //char    *name;
    "Responsible for building and maintaining all the alien structures. "
      "Has a weak melee slash attack.",
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ),            //int     stages;
    ABUILDER_HEALTH,                                //int     health;
    0.2f,                                           //float   fallDamage;
    ABUILDER_REGEN,                                 //float   regenRate;
    SCA_TAKESFALLDAMAGE|SCA_FOVWARPS|SCA_ALIENSENSE|SCA_REGEN|SCA_CANHOVEL, //int    abilities;
    WP_ABUILD,                                      //weapon_t startWeapon;
    95.0f,                                          //float   buildDist;
    95.0f,                                          //float   buildDistPrecise;
    qtrue,                                          //qboolean buildPreciseForce;
    110,                                            //int     fov;
    0.001f,                                         //float   bob;
    2.0f,                                           //float   bobCycle;
    4.5f,                                           //float   landBob;
    150,                                            //int     steptime;
    ABUILDER_SPEED,                                 //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    100.0f,                                         //float   stopSpeed;
    195.0f,                                         //float   jumpMagnitude;
    1.0f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_BUILDER0_UPG, PCL_ALIEN_LEVEL0, PCL_NONE }, //int  children[ 3 ];
    ABUILDER_COST,                                  //int     cost;
    qtrue,                                          //qboolean warmupFree;
    ABUILDER_VALUE,                                 //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_BUILDER0_UPG,                         //int     number;
    qtrue,                                          //qboolean enabled;
    "builderupg",                                   //char    *name;
    "Similar to the base Granger, except that in addition to "
      "being able to build structures it has a spit attack "
      "that slows victims and the ability to crawl on walls.",
    ( 1 << S2 )|( 1 << S3 ),                        //int     stages;
    ABUILDER_UPG_HEALTH,                            //int     health;
    0.2f,                                           //float   fallDamage;
    ABUILDER_UPG_REGEN,                             //float   regenRate;
    SCA_TAKESFALLDAMAGE|SCA_FOVWARPS|SCA_WALLCLIMBER|SCA_ALIENSENSE|SCA_REGEN|SCA_CANHOVEL, //int  abilities;
    WP_ABUILD2,                                     //weapon_t startWeapon;
    105.0f,                                         //float   buildDist;
    105.0f,                                         //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    110,                                            //int     fov;
    0.001f,                                         //float   bob;
    2.0f,                                           //float   bobCycle;
    4.5f,                                           //float   landBob;
    100,                                            //int     steptime;
    ABUILDER_UPG_SPEED,                             //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    100.0f,                                         //float   stopSpeed;
    270.0f,                                         //float   jumpMagnitude;
    1.0f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_LEVEL0, PCL_NONE, PCL_NONE },       //int     children[ 3 ];
    ABUILDER_UPG_COST,                              //int     cost;
    qtrue,                                          //qboolean warmupFree;
    ABUILDER_UPG_VALUE,                             //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_LEVEL0,                               //int     number;
    qtrue,                                          //qboolean enabled;
    "level0",                                       //char    *name;
    "Has a lethal reflexive bite and the ability to crawl on "
      "walls and ceilings.",
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ),            //int     stages;
    LEVEL0_HEALTH,                                  //int     health;
    0.0f,                                           //float   fallDamage;
    LEVEL0_REGEN,                                   //float   regenRate;
    SCA_WALLCLIMBER|SCA_FOVWARPS|SCA_ALIENSENSE|SCA_REGEN,    //int     abilities;
    WP_ALEVEL0,                                     //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    140,                                            //int     fov;
    0.0f,                                           //float   bob;
    2.5f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    25,                                             //int     steptime;
    LEVEL0_SPEED,                                   //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    400.0f,                                         //float   stopSpeed;
    250.0f,                                         //float   jumpMagnitude;
    2.0f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_LEVEL1, PCL_NONE, PCL_NONE },       //int     children[ 3 ];
    LEVEL0_COST,                                    //int     cost;
    qtrue,                                          //qboolean warmupFree;
    LEVEL0_VALUE,                                   //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_LEVEL1,                               //int     number;
    qtrue,                                          //qboolean enabled;
    "level1",                                       //char    *name;
    "A support class able to crawl on walls and ceilings. Its melee "
      "attack is most effective when combined with the ability to grab "
      "and hold its victims in place. Provides a weak healing aura "
      "that accelerates the healing rate of nearby aliens.",
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ),            //int     stages;
    LEVEL1_HEALTH,                                  //int     health;
    0.0f,                                           //float   fallDamage;
    LEVEL1_REGEN,                                   //float   regenRate;
    SCA_FOVWARPS|SCA_WALLCLIMBER|SCA_ALIENSENSE|SCA_REGEN,    //int     abilities;
    WP_ALEVEL1,                                     //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    120,                                            //int     fov;
    0.001f,                                         //float   bob;
    1.8f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    60,                                             //int     steptime;
    LEVEL1_SPEED,                                   //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    300.0f,                                         //float   stopSpeed;
    310.0f,                                         //float   jumpMagnitude;
    1.2f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_LEVEL2, PCL_ALIEN_LEVEL1_UPG, PCL_NONE }, //int  children[ 3 ];
    LEVEL1_COST,                                    //int     cost;
    qtrue,                                          //qboolean warmupFree;
    LEVEL1_VALUE,                                   //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_LEVEL1_UPG,                           //int     number;
    qtrue,                                          //qboolean enabled;
    "level1upg",                                    //char    *name;
    "In addition to the basic Basilisk abilities, the Advanced "
      "Basilisk sprays a poisonous gas which disorients any "
      "nearby humans. Has a strong healing aura that "
      "that accelerates the healing rate of nearby aliens.",
    ( 1 << S2 )|( 1 << S3 ),                        //int     stages;
    LEVEL1_UPG_HEALTH,                              //int     health;
    0.0f,                                           //float   fallDamage;
    LEVEL1_UPG_REGEN,                               //float   regenRate;
    SCA_FOVWARPS|SCA_WALLCLIMBER|SCA_ALIENSENSE|SCA_REGEN,    //int     abilities;
    WP_ALEVEL1_UPG,                                 //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    120,                                            //int     fov;
    0.001f,                                         //float   bob;
    1.8f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    60,                                             //int     steptime;
    LEVEL1_UPG_SPEED,                               //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    300.0f,                                         //float   stopSpeed;
    310.0f,                                         //float   jumpMagnitude;
    1.1f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_LEVEL2, PCL_NONE, PCL_NONE },       //int     children[ 3 ];
    LEVEL1_UPG_COST,                                //int     cost;
    qtrue,                                          //qboolean warmupFree;
    LEVEL1_UPG_VALUE,                               //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_LEVEL2,                               //int     number;
    qtrue,                                          //qboolean enabled;
    "level2",                                       //char    *name;
    "Has a melee attack and the ability to jump off walls. This "
      "allows the Marauder to gather great speed in enclosed areas.",
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ),            //int     stages;
    LEVEL2_HEALTH,                                  //int     health;
    0.0f,                                           //float   fallDamage;
    LEVEL2_REGEN,                                   //float   regenRate;
    SCA_WALLJUMPER|SCA_FOVWARPS|SCA_ALIENSENSE|SCA_REGEN,     //int     abilities;
    WP_ALEVEL2,                                     //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    90,                                             //int     fov;
    0.001f,                                         //float   bob;
    1.5f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    80,                                             //int     steptime;
    LEVEL2_SPEED,                                   //float   speed;
    10.0f,                                          //float   acceleration;
    3.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    100.0f,                                         //float   stopSpeed;
    380.0f,                                         //float   jumpMagnitude;
    0.8f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_LEVEL3, PCL_ALIEN_LEVEL2_UPG, PCL_NONE }, //int  children[ 3 ];
    LEVEL2_COST,                                    //int     cost;
    qtrue,                                          //qboolean warmupFree;
    LEVEL2_VALUE,                                   //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_LEVEL2_UPG,                           //int     number;
    qtrue,                                          //qboolean enabled;
    "level2upg",                                    //char    *name;
    "The Advanced Marauder has all the abilities of the basic Marauder "
      "with the addition of an area effect electric shock attack.",
    ( 1 << S2 )|( 1 << S3 ),                        //int     stages;
    LEVEL2_UPG_HEALTH,                              //int     health;
    0.0f,                                           //float   fallDamage;
    LEVEL2_UPG_REGEN,                               //float   regenRate;
    SCA_WALLJUMPER|SCA_FOVWARPS|SCA_ALIENSENSE|SCA_REGEN,     //int     abilities;
    WP_ALEVEL2_UPG,                                 //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    90,                                             //int     fov;
    0.001f,                                         //float   bob;
    1.5f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    80,                                             //int     steptime;
    LEVEL2_UPG_SPEED,                               //float   speed;
    10.0f,                                          //float   acceleration;
    3.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    100.0f,                                         //float   stopSpeed;
    380.0f,                                         //float   jumpMagnitude;
    0.7f,                                           //float   knockbackScale
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_LEVEL3, PCL_NONE, PCL_NONE },       //int     children[ 3 ];
    LEVEL2_UPG_COST,                                //int     cost;
    qtrue,                                          //qboolean warmupFree;
    LEVEL2_UPG_VALUE,                               //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_LEVEL3,                               //int     number;
    qtrue,                                          //qboolean enabled;
    "level3",                                       //char    *name;
    "Possesses a melee attack and the pounce ability, which may "
      "be used as both an attack and a means to reach remote "
      "locations inaccessible from the ground.",
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ),            //int     stages;
    LEVEL3_HEALTH,                                  //int     health;
    0.0f,                                           //float   fallDamage;
    LEVEL3_REGEN,                                   //float   regenRate;
    SCA_FOVWARPS|SCA_ALIENSENSE|SCA_REGEN,          //int     abilities;
    WP_ALEVEL3,                                     //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    110,                                            //int     fov;
    0.0005f,                                        //float   bob;
    1.3f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    90,                                             //int     steptime;
    LEVEL3_SPEED,                                   //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    200.0f,                                         //float   stopSpeed;
    270.0f,                                         //float   jumpMagnitude;
    0.5f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_LEVEL4, PCL_ALIEN_LEVEL3_UPG, PCL_NONE }, //int  children[ 3 ];
    LEVEL3_COST,                                    //int     cost;
    qtrue,                                          //qboolean warmupFree;
    LEVEL3_VALUE,                                   //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_LEVEL3_UPG,                           //int     number;
    qtrue,                                          //qboolean enabled;
    "level3upg",                                    //char    *name;
    "In addition to the basic Dragoon abilities, the Advanced "
      "Dragoon has 3 barbs which may be used to attack humans "
      "from a distance.",
    ( 1 << S2 )|( 1 << S3 ),                        //int     stages;
    LEVEL3_UPG_HEALTH,                              //int     health;
    0.0f,                                           //float   fallDamage;
    LEVEL3_UPG_REGEN,                               //float   regenRate;
    SCA_FOVWARPS|SCA_ALIENSENSE|SCA_REGEN,          //int     abilities;
    WP_ALEVEL3_UPG,                                 //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    110,                                            //int     fov;
    0.0005f,                                        //float   bob;
    1.3f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    90,                                             //int     steptime;
    LEVEL3_UPG_SPEED,                               //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    200.0f,                                         //float   stopSpeed;
    270.0f,                                         //float   jumpMagnitude;
    0.4f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_ALIEN_LEVEL4, PCL_NONE, PCL_NONE },       //int     children[ 3 ];
    LEVEL3_UPG_COST,                                //int     cost;
    qfalse,                                         //qboolean warmupFree;
    LEVEL3_UPG_VALUE,                               //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_ALIEN_LEVEL4,                               //int     number;
    qtrue,                                          //qboolean enabled;
    "level4",                                       //char    *name;
    "A large alien with a strong melee attack, this class can "
      "also charge at enemy humans and structures, inflicting "
      "great damage. Any humans or their structures caught under "
      "a falling Tyrant will be crushed by its weight.",
    ( 1 << S3 ),                                    //int     stages;
    LEVEL4_HEALTH,                                  //int     health;
    0.0f,                                           //float   fallDamage;
    LEVEL4_REGEN,                                   //float   regenRate;
    SCA_FOVWARPS|SCA_ALIENSENSE|SCA_REGEN,                    //int     abilities;
    WP_ALEVEL4,                                     //weapon_t startWeapon;
    0.0f,                                           //float   buildDist;
    0.0f,                                           //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    90,                                             //int     fov;
    0.001f,                                         //float   bob;
    1.1f,                                           //float   bobCycle;
    0.0f,                                           //float   landBob;
    100,                                            //int     steptime;
    LEVEL4_SPEED,                                   //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    100.0f,                                         //float   stopSpeed;
    170.0f,                                         //float   jumpMagnitude;
    0.1f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_NONE, PCL_NONE, PCL_NONE },               //int     children[ 3 ];
    LEVEL4_COST,                                    //int     cost;
    qfalse,                                         //qboolean warmupFree;
    LEVEL4_VALUE,                                   //int     value;
    TEAM_ALIENS                                     //team_t  team;
  },
  {
    PCL_HUMAN,                                      //int     number;
    qtrue,                                          //qboolean enabled;
    "human_base",                                   //char    *name;
    "",
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ),            //int     stages;
    100000,                                         //int     health;
    1.0f,                                           //float   fallDamage;
    0.0f,                                           //float   regenRate;
    SCA_TAKESFALLDAMAGE|SCA_CANUSELADDERS|SCA_STAMINA,          //int     abilities;
    WP_NONE, //special-cased in g_client.c          //weapon_t startWeapon;
    110.0f,                                         //float   buildDist;
    110.0f,                                         //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    90,                                             //int     fov;
    0.002f,                                         //float   bob;
    1.0f,                                           //float   bobCycle;
    8.0f,                                           //float   landBob;
    100,                                            //int     steptime;
    1.0f,                                           //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    100.0f,                                         //float   stopSpeed;
    220.0f,                                         //float   jumpMagnitude;
    1.0f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_NONE, PCL_NONE, PCL_NONE },               //int     children[ 3 ];
    0,                                              //int     cost;
    qtrue,                                          //qboolean warmupFree;
    ALIEN_CREDITS_PER_KILL,                         //int     value;
    TEAM_HUMANS                                     //team_t  team;
  },
  {
    PCL_HUMAN_BSUIT,                                //int     number;
    qtrue,                                          //qboolean enabled;
    "human_bsuit",                                  //char    *name;
    "",
    ( 1 << S3 ),                                    //int     stages;
    100000,                                         //int     health;
    1.0f,                                           //float   fallDamage;
    0.0f,                                           //float   regenRate;
    SCA_TAKESFALLDAMAGE|SCA_CANUSELADDERS|SCA_STAMINA,          //int     abilities;
    WP_NONE, //special-cased in g_client.c          //weapon_t startWeapon;
    110.0f,                                         //float   buildDist;
    110.0f,                                         //float   buildDistPrecise;
    qfalse,                                         //qboolean buildPreciseForce;
    90,                                             //int     fov;
    0.002f,                                         //float   bob;
    1.0f,                                           //float   bobCycle;
    5.0f,                                           //float   landBob;
    100,                                            //int     steptime;
    1.0f,                                           //float   speed;
    10.0f,                                          //float   acceleration;
    1.0f,                                           //float   airAcceleration;
    6.0f,                                           //float   friction;
    100.0f,                                         //float   stopSpeed;
    220.0f,                                         //float   jumpMagnitude;
    1.0f,                                           //float   knockbackScale;
    CHARGE_STAMINA_MAX,                             //int     chargeStaminaMax;
    CHARGE_STAMINA_MIN,                             //int     chargeStaminaMin;
    CHARGE_STAMINA_USE_RATE,                        //float   chargeStaminaUseRate;
    CHARGE_STAMINA_RESTORE_RATE,                    //float   chargeStaminaRestoreRate;
    { PCL_NONE, PCL_NONE, PCL_NONE },               //int     children[ 3 ];
    0,                                              //int     cost;
    qtrue,                                          //qboolean warmupFree;
    ALIEN_CREDITS_PER_KILL,                         //int     value;
    TEAM_HUMANS                                     //team_t  team;
  }
};

size_t bg_numClasses = ARRAY_LEN( bg_classList );

static const classAttributes_t nullClass = { 0 };

/*
==============
BG_ClassByName
==============
*/
const classAttributes_t *BG_ClassByName( const char *name )
{
  int i;

  for( i = 0; i < bg_numClasses; i++ )
  {
    if( !Q_stricmp( bg_classList[ i ].name, name ) )
      return &bg_classList[ i ];
  }

  return &nullClass;
}

/*
==============
BG_Class
==============
*/
const classAttributes_t *BG_Class( class_t class )
{
  return ( class >= PCL_NONE && class < PCL_NUM_CLASSES ) ?
    &bg_classList[ class ] : &nullClass;
}

/*
==============
BG_ClassAllowedInStage
==============
*/
qboolean BG_ClassAllowedInStage( class_t class,
                                 stage_t stage,
                                 int gameIsInWarmup )
{
  int stages = BG_Class( class )->stages;

  if( gameIsInWarmup )
    return qtrue;

  return stages & ( 1 << stage );
}

static classConfig_t bg_classConfigList[ PCL_NUM_CLASSES ];

/*
==============
BG_ClassConfig
==============
*/
classConfig_t *BG_ClassConfig( class_t class )
{
  return &bg_classConfigList[ class ];
}

/*
==============
BG_ClassBoundingBox
==============
*/
void BG_ClassBoundingBox( class_t class,
                          vec3_t mins, vec3_t maxs,
                          vec3_t cmaxs, vec3_t dmins, vec3_t dmaxs )
{
  classConfig_t *classConfig = BG_ClassConfig( class );

  if( mins != NULL )
    VectorCopy( classConfig->mins, mins );

  if( maxs != NULL )
    VectorCopy( classConfig->maxs, maxs );

  if( cmaxs != NULL )
    VectorCopy( classConfig->crouchMaxs, cmaxs );

  if( dmins != NULL )
    VectorCopy( classConfig->deadMins, dmins );

  if( dmaxs != NULL )
    VectorCopy( classConfig->deadMaxs, dmaxs );
}

/*
==============
BG_ClassHasAbility
==============
*/
qboolean BG_ClassHasAbility( class_t class, int ability )
{
  int abilities = BG_Class( class )->abilities;

  return abilities & ability;
}

/*
==============
_BG_ClassCanEvolveFromTo
==============
*/
int _BG_ClassCanEvolveFromTo( class_t fclass,
                              class_t tclass,
                              int credits, int stage,
                              int cost,
                              int gameIsInWarmup,
                              qboolean devMode )
{
  int i, j, best, value;
  qboolean warmupFree = gameIsInWarmup && BG_Class(tclass)->warmupFree;

  if(warmupFree) {
    credits = ALIEN_MAX_CREDITS;
  }

  if( credits < cost || fclass == PCL_NONE || tclass == PCL_NONE ||
      fclass == tclass )
    return -1;

  for( i = 0; i < bg_numClasses; i++ )
  {
    if( bg_classList[ i ].number != fclass )
      continue;

    best = credits + 1;
    for( j = 0; j < 3; j++ )
    {
      int thruClass, evolveCost;

      thruClass = bg_classList[ i ].children[ j ];
      if( thruClass == PCL_NONE || !BG_ClassAllowedInStage( thruClass, stage, gameIsInWarmup ) ||
          !BG_ClassIsAllowed( thruClass, devMode ) )
        continue;

      evolveCost = BG_Class( thruClass )->cost * ALIEN_CREDITS_PER_KILL;
      if( thruClass == tclass )
        value = cost + evolveCost;
      else
        value = _BG_ClassCanEvolveFromTo( thruClass, tclass, credits, stage,
                                          cost + evolveCost,
                                          gameIsInWarmup, devMode );

      if( value >= 0 && value < best )
        best = value;
    }

    return best <= credits ? (warmupFree ? 0 : best) : -1;
  }

  Com_Printf( S_COLOR_YELLOW "WARNING: fallthrough in BG_ClassCanEvolveFromTo\n" );
  return -1;
}

/*
==============
BG_ClassCanEvolveFromTo
==============
*/
int BG_ClassCanEvolveFromTo(
  class_t fclass, class_t tclass, int credits, int stage, int cost,
  int gameIsInWarmup, qboolean devMode, qboolean class_forced) {
  int value;

  if(class_forced) {
    return -1;
  }

  value = _BG_ClassCanEvolveFromTo(
    fclass, tclass, credits, stage, cost, gameIsInWarmup, devMode);

  if(
    gameIsInWarmup && value >= 0 && fclass != PCL_ALIEN_BUILDER0 &&
    !BG_Class(tclass)->warmupFree) {
    value = _BG_ClassCanEvolveFromTo(
      PCL_ALIEN_BUILDER0, tclass, credits, stage, cost, gameIsInWarmup, devMode);
  }

  return value;
}

/*
==============
BG_AlienCanEvolve
==============
*/
qboolean BG_AlienCanEvolve( class_t class, int credits, int stage,
                            int gameIsInWarmup, qboolean devMode,
                            qboolean class_forced )
{
  int i, j, tclass;

  if(class_forced) {
    return qfalse;
  }

  for( i = 0; i < bg_numClasses; i++ )
  {
    if( bg_classList[ i ].number != class )
      continue;

    for( j = 0; j < 3; j++ )
    {
      tclass = bg_classList[ i ].children[ j ];
      if(tclass == PCL_NONE) {
        continue;
      }

      if( BG_ClassAllowedInStage( tclass, stage, gameIsInWarmup ) &&
          BG_ClassIsAllowed( tclass, devMode ) ) {
        if(
          gameIsInWarmup && class != PCL_ALIEN_BUILDER0 &&
          !BG_Class(tclass)->warmupFree) {
          int value;

          value = _BG_ClassCanEvolveFromTo(
            PCL_ALIEN_BUILDER0, tclass, credits, stage, 0, gameIsInWarmup, devMode);

          if(value < 0) {
            continue;
          } else {
            return qtrue;
          }
        }

        if(gameIsInWarmup || credits >= BG_Class(tclass)->cost * ALIEN_CREDITS_PER_KILL) {
          return qtrue;
        }
      }
  }

    return qfalse;
  }

  Com_Printf( S_COLOR_YELLOW "WARNING: fallthrough in BG_AlienCanEvolve\n" );
  return qfalse;
}

/*
======================
BG_ParseClassFile

Parses a configuration file describing a class
======================
*/
static qboolean BG_ParseClassFile( const char *filename, classConfig_t *cc )
{
  char          *text_p;
  int           i;
  int           len;
  char          *token;
  char          text[ 20000 ];
  fileHandle_t  f;
  float         scale = 0.0f;
  int           defined = 0;
  enum
  {
      MODEL           = 1 << 0,
      SKIN            = 1 << 1,
      HUD             = 1 << 2,
      MODELSCALE      = 1 << 3,
      SHADOWSCALE     = 1 << 4,
      MINS            = 1 << 5,
      MAXS            = 1 << 6,
      DEADMINS        = 1 << 7,
      DEADMAXS        = 1 << 8,
      CROUCHMAXS      = 1 << 9,
      VIEWHEIGHT      = 1 << 10,
      CVIEWHEIGHT     = 1 << 11,
      ZOFFSET         = 1 << 12,
      NAME            = 1 << 13,
      SHOULDEROFFSETS = 1 << 14
  };

  // load the file
  len = FS_FOpenFileByMode( filename, &f, FS_READ );
  if( len < 0 )
    return qfalse;

  if( len == 0 || len >= sizeof( text ) - 1 )
  {
    FS_FCloseFile( f );
    Com_Printf( S_COLOR_RED "ERROR: Class file %s is %s\n", filename,
      len == 0 ? "empty" : "too long" );
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

    if( !token )
      break;

    if( !Q_stricmp( token, "" ) )
      break;

    if( !Q_stricmp( token, "model" ) )
    {
      token = COM_Parse( &text_p );
      if( !token )
        break;

      Q_strncpyz( cc->modelName, token, sizeof( cc->modelName ) );

      defined |= MODEL;
      continue;
    }
    else if( !Q_stricmp( token, "skin" ) )
    {
      token = COM_Parse( &text_p );
      if( !token )
        break;

      Q_strncpyz( cc->skinName, token, sizeof( cc->skinName ) );

      defined |= SKIN;
      continue;
    }
    else if( !Q_stricmp( token, "hud" ) )
    {
      token = COM_Parse( &text_p );
      if( !token )
        break;

      Q_strncpyz( cc->hudName, token, sizeof( cc->hudName ) );

      defined |= HUD;
      continue;
    }
    else if( !Q_stricmp( token, "modelScale" ) )
    {
      token = COM_Parse( &text_p );
      if( !token )
        break;

      scale = atof( token );

      if( scale < 0.0f )
        scale = 0.0f;

      cc->modelScale = scale;

      defined |= MODELSCALE;
      continue;
    }
    else if( !Q_stricmp( token, "shadowScale" ) )
    {
      token = COM_Parse( &text_p );
      if( !token )
        break;

      scale = atof( token );

      if( scale < 0.0f )
        scale = 0.0f;

      cc->shadowScale = scale;

      defined |= SHADOWSCALE;
      continue;
    }
    else if( !Q_stricmp( token, "mins" ) )
    {
      for( i = 0; i <= 2; i++ )
      {
        token = COM_Parse( &text_p );
        if( !token )
          break;

        cc->mins[ i ] = atof( token );
      }

      defined |= MINS;
      continue;
    }
    else if( !Q_stricmp( token, "maxs" ) )
    {
      for( i = 0; i <= 2; i++ )
      {
        token = COM_Parse( &text_p );
        if( !token )
          break;

        cc->maxs[ i ] = atof( token );
      }

      defined |= MAXS;
      continue;
    }
    else if( !Q_stricmp( token, "deadMins" ) )
    {
      for( i = 0; i <= 2; i++ )
      {
        token = COM_Parse( &text_p );
        if( !token )
          break;

        cc->deadMins[ i ] = atof( token );
      }

      defined |= DEADMINS;
      continue;
    }
    else if( !Q_stricmp( token, "deadMaxs" ) )
    {
      for( i = 0; i <= 2; i++ )
      {
        token = COM_Parse( &text_p );
        if( !token )
          break;

        cc->deadMaxs[ i ] = atof( token );
      }

      defined |= DEADMAXS;
      continue;
    }
    else if( !Q_stricmp( token, "crouchMaxs" ) )
    {
      for( i = 0; i <= 2; i++ )
      {
        token = COM_Parse( &text_p );
        if( !token )
          break;

        cc->crouchMaxs[ i ] = atof( token );
      }

      defined |= CROUCHMAXS;
      continue;
    }
    else if( !Q_stricmp( token, "viewheight" ) )
    {
      token = COM_Parse( &text_p );
      cc->viewheight = atoi( token );
      defined |= VIEWHEIGHT;
      continue;
    }
    else if( !Q_stricmp( token, "crouchViewheight" ) )
    {
      token = COM_Parse( &text_p );
      cc->crouchViewheight = atoi( token );
      defined |= CVIEWHEIGHT;
      continue;
    }
    else if( !Q_stricmp( token, "zOffset" ) )
    {
      float offset;

      token = COM_Parse( &text_p );
      if( !token )
        break;

      offset = atof( token );

      cc->zOffset = offset;

      defined |= ZOFFSET;
      continue;
    }
    else if( !Q_stricmp( token, "name" ) )
    {
      token = COM_Parse( &text_p );
      if( !token )
        break;

      Q_strncpyz( cc->humanName, token, sizeof( cc->humanName ) );

      defined |= NAME;
      continue;
    }
    else if( !Q_stricmp( token, "shoulderOffsets" ) )
    {
      for( i = 0; i <= 2; i++ )
      {
        token = COM_Parse( &text_p );
        if( !token )
          break;

        cc->shoulderOffsets[ i ] = atof( token );
      }

      defined |= SHOULDEROFFSETS;
      continue;
    }

    Com_Printf( S_COLOR_RED "ERROR: unknown token '%s'\n", token );
    return qfalse;
  }

  if(      !( defined & MODEL           ) ) token = "model";
  else if( !( defined & SKIN            ) ) token = "skin";
  else if( !( defined & HUD             ) ) token = "hud";
  else if( !( defined & MODELSCALE      ) ) token = "modelScale";
  else if( !( defined & SHADOWSCALE     ) ) token = "shadowScale";
  else if( !( defined & MINS            ) ) token = "mins";
  else if( !( defined & MAXS            ) ) token = "maxs";
  else if( !( defined & DEADMINS        ) ) token = "deadMins";
  else if( !( defined & DEADMAXS        ) ) token = "deadMaxs";
  else if( !( defined & CROUCHMAXS      ) ) token = "crouchMaxs";
  else if( !( defined & VIEWHEIGHT      ) ) token = "viewheight";
  else if( !( defined & CVIEWHEIGHT     ) ) token = "crouchViewheight";
  else if( !( defined & ZOFFSET         ) ) token = "zOffset";
  else if( !( defined & NAME            ) ) token = "name";
  else if( !( defined & SHOULDEROFFSETS ) ) token = "shoulderOffsets";
  else                                      token = "";

  if( strlen( token ) > 0 )
  {
      Com_Printf( S_COLOR_RED "ERROR: %s not defined in %s\n",
                  token, filename );
      return qfalse;
  }

  return qtrue;
}

/*
===============
BG_InitClassConfigs
===============
*/
void BG_InitClassConfigs( void )
{
  int           i;
  classConfig_t *cc;

  for( i = PCL_NONE; i < PCL_NUM_CLASSES; i++ )
  {
    cc = BG_ClassConfig( i );

    BG_ParseClassFile( va( "configs/classes/%s.cfg",
                           BG_Class( i )->name ), cc );
  }
}

////////////////////////////////////////////////////////////////////////////////

static const weaponAttributes_t bg_weapons[ ] =
{
  {
    WP_ALEVEL0,           //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,                //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "level0",             //char      *name;
    "Bite",               //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LEVEL0_BITE_REPEAT,   //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LEVEL0_BITE_K_SCALE,  //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_ALEVEL1,           //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "level1",             //char      *name;
    "Claws",              //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LEVEL1_CLAW_REPEAT,   //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LEVEL1_CLAW_K_SCALE,  //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_ALEVEL1_UPG,       //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "level1upg",          //char      *name;
    "Claws Upgrade",      //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LEVEL1_CLAW_U_REPEAT, //int       repeatRate1;
    LEVEL1_PCLOUD_REPEAT, //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LEVEL1_CLAW_U_K_SCALE, //float    knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_ALEVEL2,           //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "level2",             //char      *name;
    "Bite",               //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LEVEL2_CLAW_REPEAT,   //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LEVEL2_CLAW_K_SCALE,  //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_ALEVEL2_UPG,       //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "level2upg",          //char      *name;
    "Zap",                //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LEVEL2_CLAW_U_REPEAT, //int       repeatRate1;
    LEVEL2_AREAZAP_REPEAT, //int      repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LEVEL2_CLAW_U_K_SCALE, //float    knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_ALEVEL3,           //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "level3",             //char      *name;
    "Pounce",             //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LEVEL3_CLAW_REPEAT,   //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LEVEL3_CLAW_K_SCALE,  //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_ALEVEL3_UPG,       //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "level3upg",          //char      *name;
    "Pounce (upgrade)",   //char      *humanName;
    "",
    4,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LEVEL3_CLAW_U_REPEAT, //int       repeatRate1;
    0,                    //int       repeatRate2;
    LEVEL3_BOUNCEBALL_REPEAT, //int   repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LEVEL3_CLAW_U_K_SCALE, //float    knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qtrue,                //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_ALEVEL4,           //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "level4",             //char      *name;
    "Charge",             //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LEVEL4_CLAW_REPEAT,   //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LEVEL4_CLAW_K_SCALE,  //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_BLASTER,           //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    0,                    //int       slots;
    "blaster",            //char      *name;
    "Blaster",            //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    BLASTER_REPEAT,       //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    BLASTER_K_SCALE,      //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_MACHINEGUN,        //int       number;
    qtrue,                //qboolean enabled;
    RIFLE_PRICE,          //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "rifle",              //char      *name;
    "Rifle",              //char      *humanName;
    "Basic weapon. Cased projectile weapon, with a slow clip based "
      "reload system.",
    RIFLE_CLIPSIZE,       //int       maxAmmo;
    RIFLE_MAXCLIPS,       //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    RIFLE_REPEAT,         //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    RIFLE_RELOAD,         //int       reloadTime;
    RIFLE_K_SCALE,        //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_PAIN_SAW,          //int       number;
    qtrue,                //qboolean enabled;
    PAINSAW_PRICE,        //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "psaw",               //char      *name;
    "Pain Saw",           //char      *humanName;
    "Similar to a chainsaw, but instead of a chain it has an "
      "electric arc capable of dealing a great deal of damage at "
      "close range.",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    PAINSAW_REPEAT,       //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    PAINSAW_K_SCALE,      //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_SHOTGUN,           //int       number;
    qtrue,                //qboolean enabled;
    SHOTGUN_PRICE,        //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "shotgun",            //char      *name;
    "Shotgun",            //char      *humanName;
    "Close range weapon that is useful against larger foes. "
      "It has a slow repeat rate, but can be devastatingly "
      "effective.",
    SHOTGUN_SHELLS,       //int       maxAmmo;
    SHOTGUN_MAXCLIPS,     //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    SHOTGUN_REPEAT,       //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    SHOTGUN_RELOAD,       //int       reloadTime;
    SHOTGUN_K_SCALE,      //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        SHOTGUN_PELLETS,  //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        SHOTGUN_SPREAD,   //float        spread;
        SHOTGUN_DMG,      //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        SHOTGUN_RANGE,    //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0.0f,             //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0,                //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_LAS_GUN,           //int       number;
    qtrue,                //qboolean enabled;
    LASGUN_PRICE,         //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "lgun",               //char      *name;
    "Las Gun",            //char      *humanName;
    "Slightly more powerful than the basic rifle, rapidly fires "
      "small packets of energy.",
    LASGUN_AMMO,          //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qtrue,                //int       usesEnergy;
    LASGUN_REPEAT,        //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    LASGUN_RELOAD,        //int       reloadTime;
    LASGUN_K_SCALE,       //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_MASS_DRIVER,       //int       number;
    qtrue,                //qboolean enabled;
    MDRIVER_PRICE,        //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "mdriver",            //char      *name;
    "Mass Driver",        //char      *humanName;
    "A portable particle accelerator which causes minor nuclear "
      "reactions at the point of impact. It has a very large "
      "payload, but fires slowly.",
    MDRIVER_CLIPSIZE,     //int       maxAmmo;
    MDRIVER_MAXCLIPS,     //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qtrue,                //int       usesEnergy;
    MDRIVER_REPEAT,       //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    MDRIVER_RELOAD,       //int       reloadTime;
    MDRIVER_K_SCALE,      //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qtrue,                //qboolean  canZoom;
    20.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_CHAINGUN,          //int       number;
    qtrue,                //qboolean enabled;
    CHAINGUN_PRICE,       //int       price;
    qfalse,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "chaingun",           //char      *name;
    "Chaingun",           //char      *humanName;
    "Belt drive, cased projectile weapon. It has a high repeat "
      "rate but a wide firing angle and is therefore relatively "
      "inaccurate.",
    CHAINGUN_BULLETS,     //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    CHAINGUN_REPEAT,      //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    CHAINGUN_K_SCALE,     //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_FLAMER,            //int       number;
    qtrue,                //qboolean enabled;
    FLAMER_PRICE,         //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int    stages;
    SLOT_WEAPON,          //int       slots;
    "flamer",             //char      *name;
    "Flame Thrower",      //char      *humanName;
    "Sprays fire at its target. It is powered by compressed "
      "gas. The relatively low rate of fire means this weapon is most "
      "effective against static targets.",
    FLAMER_GAS,           //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    FLAMER_REPEAT,        //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    FLAMER_K_SCALE,       //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_PULSE_RIFLE,       //int       number;
    qtrue,                //qboolean enabled;
    PRIFLE_PRICE,         //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int    stages;
    SLOT_WEAPON,          //int       slots;
    "prifle",             //char      *name;
    "Pulse Rifle",        //char      *humanName;
    "An energy weapon that fires rapid pulses of concentrated energy.",
    PRIFLE_CLIPS,         //int       maxAmmo;
    PRIFLE_MAXCLIPS,      //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qtrue,                //int       usesEnergy;
    PRIFLE_REPEAT,        //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    PRIFLE_RELOAD,        //int       reloadTime;
    PRIFLE_K_SCALE,       //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_LUCIFER_CANNON,    //int       number;
    qtrue,                //qboolean enabled;
    LCANNON_PRICE,        //int       price;
    qfalse,               //qboolean  warmupFree;
    ( 1 << S3 ),          //int       stages;
    SLOT_WEAPON,          //int       slots;
    "lcannon",            //char      *name;
    "Lucifer Cannon",     //char      *humanName;
    "Blaster technology scaled up to deliver devastating power. "
      "Primary fire must be charged before firing. It has a quick "
      "secondary attack that does not require charging. "
      "Primary fire's charge can be reduced before firing if the "
      "secondary attack is held down simultaneously with the primary attack.",
    LCANNON_AMMO,         //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qtrue,                //int       usesEnergy;
    LCANNON_REPEAT,       //int       repeatRate1;
    LCANNON_SECONDARY_REPEAT, //int   repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    LCANNON_RELOAD,       //int       reloadTime;
    LCANNON_K_SCALE,      //float     knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_GRENADE,           //int       number;
    qtrue,                //qboolean enabled;
    GRENADE_PRICE,        //int       price;
    qfalse,               //qboolean  warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int    stages;
    SLOT_NONE,            //int       slots;
    "grenade",            //char      *name;
    "Grenade",            //char      *humanName;
    "",
    1,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    GRENADE_REPEAT,       //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    GRENADE_K_SCALE,      //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_FRAGNADE,          //int       number;
    qfalse,               //qboolean enabled;
    FRAGNADE_PRICE,       //int       price;
    qfalse,               //qboolean  warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int    stages;
    SLOT_NONE,            //int       slots;
    "fragnade",           //char      *name;
    "Fragmentation Grenade", //char      *humanName;
    "",
    1,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    FRAGNADE_REPEAT,      //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    FRAGNADE_K_SCALE,     //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        FRAGNADE_FRAGMENTS, //unsigned int number;;
        SPLATP_MIRRORED_INVERSE_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_UNIFORM_ALTERNATING, //splatterDistribution_t distribution;
        FRAGNADE_PITCH_LAYERS, //unsigned int           pitchLayers;
        FRAGNADE_SPREAD, //float        spread;
        FRAGNADE_FRAGMENT_DMG, //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        FRAGNADE_RANGE, //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_LAUNCHER,          //int       number;
    qfalse,               //qboolean  enabled;
    LAUNCHER_PRICE,       //int       price;
    qfalse,               //qboolean  warmupFree;
    ( 1 << S3 ),          //int       stages;
    SLOT_WEAPON,          //int       slots;
    "grenade_launcher",   //char      *name;
    "Grenade Launcher",   //char      *humanName;
    "Has a primary fire that launches grenades that "
    "explode on impact, and a secondary fire that "
    "launches timed grenades.",
    LAUNCHER_AMMO,        //int       maxAmmo;
    LAUNCHER_MAXCLIPS,    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    LAUNCHER_ROUND_PRICE, //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    LAUNCHER_REPEAT,      //int       repeatRate1;
    LAUNCHER_REPEAT,      //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    LAUNCHER_RELOAD,      //int       reloadTime;
    LAUNCHER_K_SCALE,     //float     knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_LIGHTNING,         //int       number;
    qfalse,               //qboolean  enabled;
    LIGHTNING_PRICE,      //int       price;
    qfalse,               //qboolean  warmupFree;
    ( 1 << S3 ),          //int       stages;
    SLOT_WEAPON,          //int       slots;
    "lightning",          //char      *name;
    "Lightning Gun",      //char      *humanName;
    "Generates charged pulsating bolts of lightning as its "
    "primary attack. The secondary attack emmits a burst of ball "
    "lightning, that can destabilized into a high energy electrical "
    "explosion triggered by an EMP emitted from the lightning gun.",
    LIGHTNING_AMMO,       //int       maxAmmo;
    LIGHTNING_MAXCLIPS,   //int       maxClips;
    1,                    //int       ammoUsage1;
    LIGHTNING_BALL_AMMO_USAGE,//int       ammoUsage2;
    0,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qtrue,                //int       usesEnergy;
    LIGHTNING_BOLT_CHARGE_TIME_MIN,//int       repeatRate1;
    LIGHTNING_BALL_REPEAT,//int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    LIGHTNING_BALL_BURST_ROUNDS,//int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    LIGHTNING_BALL_BURST_DELAY,//int       burstDelay2;
    0,                    //int       burstDelay3;
    LIGHTNING_RELOAD,     //int       reloadTime;
    LIGHTNING_K_SCALE,    //float     knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qtrue,                //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_LOCKBLOB_LAUNCHER, //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "lockblob",           //char      *name;
    "Lock Blob",          //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    500,                  //int       repeatRate1;
    500,                  //int       repeatRate2;
    500,                  //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    LOCKBLOB_K_SCALE,     //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_HIVE,              //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "hive",               //char      *name;
    "Hive",               //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    500,                  //int       repeatRate1;
    500,                  //int       repeatRate2;
    500,                  //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    HIVE_K_SCALE,         //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_TESLAGEN,          //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "teslagen",           //char      *name;
    "Tesla Generator",    //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qtrue,                //int       usesEnergy;
    500,                  //int       repeatRate1;
    500,                  //int       repeatRate2;
    500,                  //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    TESLAGEN_K_SCALE,     //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_MGTURRET,          //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "mgturret",           //char      *name;
    "Machinegun Turret",  //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    0,                    //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    MGTURRET_K_SCALE,     //float     knockbackScale;
    qfalse,               //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qfalse,               //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_ABUILD,            //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "abuild",             //char      *name;
    "Alien build weapon", //char      *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    ABUILDER_BUILD_REPEAT, //int      repeatRate1;
    ABUILDER_CLAW_REPEAT, //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    ABUILDER_CLAW_K_SCALE, //float    knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_ABUILD2,           //int       number;
    qtrue,                //qboolean enabled;
    0,                    //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "abuildupg",          //char      *name;
    "Alien build weapon2", //char     *humanName;
    "",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    ABUILDER_BUILD_REPEAT, //int      repeatRate1;
    ABUILDER_CLAW_REPEAT, //int       repeatRate2;
    ABUILDER_BLOB_REPEAT, //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    ABUILDER_CLAW_K_SCALE, //float    knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qtrue,                //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_ALIENS           //team_t    team;
  },
  {
    WP_HBUILD,            //int       number;
    qtrue,                //qboolean enabled;
    HBUILD_PRICE,         //int       price;
    qtrue,               //qboolean  warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_WEAPON,          //int       slots;
    "ckit",               //char      *name;
    "Construction Kit",   //char      *humanName;
    "Used for building structures. This includes "
      "spawns, power and basic defense. More structures become "
      "available with new stages.",
    0,                    //int       maxAmmo;
    0,                    //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    0,                    //int       roundPrice;
    qfalse,               //qboolean  ammoPurchasable;
    qtrue,                //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    HBUILD_REPEAT,        //int       repeatRate1;
    0,                    //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    0.0f,                 //float     knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qfalse,               //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  },
  {
    WP_PORTAL_GUN,        //int       weaponNum;
    qfalse,                //qboolean enabled;
    PORTALGUN_PRICE,      //int       price;
    qfalse,               //qboolean  warmupFree;
    ( 1 << S3 ),          //int       stages
    SLOT_WEAPON,          //int       slots;
    "portalgun",          //char      *weaponName;
    "Portal Gun",         //char      *humanName;
    "Wormhole technology packed into a portable gun. Primary fire "
     "creates the blue portal, secondary fire creates the red portal.",
    PORTALGUN_AMMO,       //int       maxAmmo;
    PORTALGUN_MAXCLIPS,   //int       maxClips;
    1,                    //int       ammoUsage1;
    1,                    //int       ammoUsage2;
    1,                    //int       ammoUsage3;
    PORTALGUN_ROUND_PRICE,//int       roundPrice;
    qtrue,                //qboolean  ammoPurchasable;
    qfalse,               //int       infiniteAmmo;
    qfalse,               //int       usesEnergy;
    PORTALGUN_REPEAT,     //int       repeatRate1;
    PORTALGUN_REPEAT,     //int       repeatRate2;
    0,                    //int       repeatRate3;
    0,                    //int       burstRounds1;
    0,                    //int       burstRounds2;
    0,                    //int       burstRounds3;
    0,                    //int       burstDelay1;
    0,                    //int       burstDelay2;
    0,                    //int       burstDelay3;
    0,                    //int       reloadTime;
    0.0f,                 //float     knockbackScale;
    qtrue,                //qboolean  hasAltMode;
    qfalse,               //qboolean  hasThirdMode;
    qfalse,               //qboolean  canZoom;
    90.0f,                //float     zoomFov;
    qtrue,                //qboolean  purchasable;
    qtrue,                //qboolean  longRanged;
    {                     //struct    splatter
      {                   //struct    splatter[0]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      },
      {                   //struct    splatter[1]
        0,                //unsigned int number;;
        SPLATP_SPHERICAL_CONE, //splatterPattern_t pattern;
        SPLATD_RANDOM,    //splatterDistribution_t distribution;
        0,                //unsigned int           pitchLayers;
        0,                //float        spread;
        0,                //int          impactDamage;
        0.0f,             //float        impactDamageFalloff;
        0,                //int          impactDamageCap;
        0.0f,             //float        range;
      }
    },
    TEAM_HUMANS           //team_t    team;
  }
};

size_t bg_numWeapons = ARRAY_LEN( bg_weapons );

static const weaponAttributes_t nullWeapon = { 0 };

/*
==============
BG_WeaponByName
==============
*/
const weaponAttributes_t *BG_WeaponByName( const char *name )
{
  int i;

  for( i = 0; i < bg_numWeapons; i++ )
  {
    if( !Q_stricmp( bg_weapons[ i ].name, name ) )
    {
      return &bg_weapons[ i ];
    }
  }

  return &nullWeapon;
}

/*
==============
BG_Weapon
==============
*/
const weaponAttributes_t *BG_Weapon( weapon_t weapon )
{
  return ( weapon > WP_NONE && weapon < WP_NUM_WEAPONS ) ?
    &bg_weapons[ weapon - 1 ] : &nullWeapon;
}

/*
==============
BG_WeaponAllowedInStage
==============
*/
qboolean BG_WeaponAllowedInStage( weapon_t weapon, stage_t stage,
                                  int gameIsInWarmup )
{
  int stages = BG_Weapon( weapon )->stages;

  if( gameIsInWarmup )
    return qtrue;

  return stages & ( 1 << stage );
}

/*
==============
BG_AmmoUsage
==============
*/
int BG_AmmoUsage( playerState_t *ps )
{
  switch ( ps->generic1 )
  {
    case WPM_PRIMARY:
      return BG_Weapon( ps->weapon )->ammoUsage1;

    case WPM_SECONDARY:
      return BG_Weapon( ps->weapon )->ammoUsage2;

    case WPM_TERTIARY:
      return BG_Weapon( ps->weapon )->ammoUsage3;

    default:
      return 1;
  }
}

/*
==============
BG_TotalPriceForWeapon

Returns the base price of a weapon plus the cost of full ammo (if ammo isn't free)
==============
*/
int BG_TotalPriceForWeapon( weapon_t weapon , int gameIsInWarmup )
{
  if( gameIsInWarmup && BG_Weapon( weapon )->warmupFree )
    return 0;

  if( !BG_Weapon( weapon )->roundPrice || BG_Weapon( weapon )->usesEnergy )
    return BG_Weapon( weapon )->price;

  return ( BG_Weapon( weapon )->roundPrice * BG_Weapon( weapon )->maxAmmo *
           ( BG_Weapon( weapon )->maxClips + 1 ) ) + BG_Weapon( weapon )->price;

}

//TODO: redo all of the angles, vectors, and math code, and look into quaternions!

typedef struct splatterPatternData_s
{
  const splatterAttributes_t *splatter;
  int seed;
  const int *fragment_num;

  float (*distribution)( struct splatterPatternData_s *data, angleIndex_t angle_index );
  void (*pattern)( struct splatterPatternData_s *data, vec3_t out );
} splatterPatternData_t;

/*
==============
BG_SplatterRandom
==============
*/
static float BG_SplatterRandom( splatterPatternData_t *data, angleIndex_t angle_index ) {
  Com_Assert( data );

  return Q_random( &data->seed );
}

/*
==============
BG_SplatterUniform
==============
*/
static float BG_SplatterUniform( splatterPatternData_t *data, angleIndex_t angle_index ) {
  const int yaw_layers = data->splatter->number / data->splatter->pitchLayers;

  Com_Assert( data );
  Com_Assert( data->fragment_num );

  if( angle_index == YAW ) {
    const int yaw_layer_num = *data->fragment_num % yaw_layers;
    float yaw_position = ( ( data->seed & 0xffff ) / (float)0x10000 ); // start the overall pattern at a random YAW

    yaw_position += ( (float)yaw_layer_num ) / ( (float)( yaw_layers ) );
    return yaw_position - ( (int)yaw_position );
  }

  //at this point angle_index must be PITCH
  Com_Assert( angle_index == PITCH );

  {
    const int pitch_layer_num = *data->fragment_num / yaw_layers;

    return ( (float)pitch_layer_num ) / ( (float)data->splatter->pitchLayers - 1 );
  }
}

/*
==============
BG_SplatterUniformAlternating
==============
*/
static float BG_SplatterUniformAlternating( splatterPatternData_t *data, angleIndex_t angle_index ) {
  const int yaw_layers = data->splatter->number / data->splatter->pitchLayers;

  Com_Assert( data );
  Com_Assert( data->fragment_num );

  if( angle_index == YAW ) {
    const int pitch_layer_num = *data->fragment_num / yaw_layers;
    const int yaw_layer_num = *data->fragment_num % yaw_layers;
    float yaw_position = ( ( data->seed & 0xffff ) / (float)0x10000 ); // start the overall pattern at a random YAW

    //alternate by a half yaw position between pitch layers
    yaw_position += 0.5 * ( ( (float)pitch_layer_num ) / ( (float)yaw_layers ) );

    yaw_position += ( (float)yaw_layer_num ) / ( (float)( yaw_layers - 1 ) );
    return yaw_position - ( (int)yaw_position );
  }

  //at this point angle_index must be PITCH
  Com_Assert( angle_index == PITCH );

  {
    const int pitch_layer_num = *data->fragment_num / yaw_layers;

    return ( (float)pitch_layer_num ) / ( (float)( data->splatter->pitchLayers - 1 ) );
  }
}

/*
==============
BG_SplatterSphericalCone
==============
*/
static void BG_SplatterSphericalCone( splatterPatternData_t *data, vec3_t out ) {
  vec3_t splatter_angles;

  Com_Assert( data );
  Com_Assert( data->distribution );
  Com_Assert( out );

  splatter_angles[PITCH] =  data->distribution( data, PITCH ) * data->splatter->spread;
  AngleNormalize180( splatter_angles[PITCH] );
  splatter_angles[PITCH] -= 90; //the spread angle is in relation to pointing straight up
  splatter_angles[YAW] = data->distribution( data, YAW ) * 360;
  AngleNormalize360( splatter_angles[YAW] );
  splatter_angles[ROLL] = 0;

  AngleVectors( splatter_angles, out, NULL, NULL );
  VectorNormalize( out );
}

/*
==============
BG_SplatterMirroredInverseSphericalCone
==============
*/
static void BG_SplatterMirroredInverseSphericalCone( splatterPatternData_t *data, vec3_t out ) {
  vec3_t splatter_angles;

  Com_Assert( data );
  Com_Assert( data->distribution );
  Com_Assert( out );

  splatter_angles[PITCH] =  data->distribution( data, PITCH ) * data->splatter->spread;
  AngleNormalize180( splatter_angles[PITCH] );
  splatter_angles[PITCH] -= ( data->splatter->spread * 0.5f ); //the spread angle is centered at the horizontal
  splatter_angles[YAW] = data->distribution( data, YAW ) * 360;
  AngleNormalize360( splatter_angles[YAW] );
  splatter_angles[ROLL] = 0;

  AngleVectors( splatter_angles, out, NULL, NULL );
  VectorNormalize( out );
}

/*
==============
BG_SplatterPattern

For the shotgun, frag nade, acidtubes, flamer, etc...
==============
*/
void BG_SplatterPattern( vec3_t origin2, int seed, int passEntNum,
                         splatterData_t *data, void (*func)( splatterData_t *data ),
                         void (*trace)( trace_t *, const vec3_t,
                                        const vec3_t, const vec3_t,
                                        const vec3_t, int, int ) ) {
  int i;
  const int modeIndex = data->weaponMode - 1;
  weapon_t weapon = data->weapon;
  vec3_t    origin, forward, cross;
  const vec3_t    up_absolute = { 0.0f, 0.0f, 1.0f };
  float  rotation_angle, cross_length, dot;
  trace_t   tr;
  splatterPatternData_t splatterData;

  memset( &splatterData, 0, sizeof( splatterData ) );
  splatterData.splatter = &BG_Weapon( weapon )->splatter[modeIndex];

  Com_Assert( modeIndex >= 0 &&
              modeIndex < 3 );
  Com_Assert( trace);
  Com_Assert( func );
  Com_Assert( splatterData.splatter );
  Com_Assert( splatterData.splatter->spread >= 0 &&
              splatterData.splatter->spread <= 180 );
  Com_Assert( ( splatterData.distribution == SPLATD_RANDOM ||
                splatterData.splatter->pitchLayers > 0 ) );
  Com_Assert( splatterData.distribution == SPLATD_RANDOM ||
              splatterData.splatter->pitchLayers < splatterData.splatter->number );
  Com_Assert( splatterData.distribution == SPLATD_RANDOM ||
              !( splatterData.splatter->number % splatterData.splatter->pitchLayers ) );

  splatterData.seed = seed;

  VectorCopy( data->origin, origin );

  //select the pattern type
  switch ( splatterData.splatter->pattern ) {
    case SPLATP_SPHERICAL_CONE:
    splatterData.pattern = BG_SplatterSphericalCone;
      break;

    case SPLATP_MIRRORED_INVERSE_SPHERICAL_CONE:
      splatterData.pattern = BG_SplatterMirroredInverseSphericalCone;
      break;
  }

  Com_Assert( splatterData.pattern );

  switch( splatterData.splatter->distribution ) {
    case SPLATD_RANDOM:
      splatterData.distribution = BG_SplatterRandom;
      break;

    case SPLATD_UNIFORM:
      splatterData.distribution = BG_SplatterUniform;
      break;

    case SPLATD_UNIFORM_ALTERNATING:
      splatterData.distribution = BG_SplatterUniformAlternating;
      break;
  }

  Com_Assert( splatterData.distribution );

  //prepare for rotation to the facing direction
  VectorCopy( origin2, forward );
  CrossProduct( up_absolute, forward, cross );
  cross_length = VectorLength( cross );
  dot = DotProduct( up_absolute, forward );

  if( cross_length > 0 ) {
    VectorNormalize( cross );
    rotation_angle = RAD2DEG( atan2( cross_length, dot) );
  } else if( dot > 0.0f ) {
    rotation_angle = 0;
  } else {
    rotation_angle = 180;
  }

  // generate the pattern
  for( i = 0; i < splatterData.splatter->number; i++ ) {
    vec3_t dir, temp, end;

    splatterData.fragment_num = &i;

    //get the next pattern vector
    splatterData.pattern( &splatterData, temp );

    //rotate toward the facing direction
    if( cross_length > 0 ) {
      RotatePointAroundVector( dir, cross, temp, rotation_angle );
    } else if( dot > 0.0f ){
      VectorCopy( temp, dir );
    } else {
      VectorScale( temp, -1.0f, dir );
    }

    VectorMA( origin, splatterData.splatter->range, dir, end );

    //apply the impact
    trace( &tr, origin, NULL, NULL, end, passEntNum, MASK_SHOT );
    data->tr = &tr;
    func( data );
  }
}

////////////////////////////////////////////////////////////////////////////////

static const upgradeAttributes_t bg_upgrades[ ] =
{
  {
    UP_LIGHTARMOUR,         //int   number;
    qtrue,                  //qboolean enabled;
    LIGHTARMOUR_PRICE,      //int   price;
    qtrue,                  //qboolean warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_TORSO|SLOT_ARMS|SLOT_LEGS, //int   slots;
    "larmour",              //char  *name;
    "Light Armour",         //char  *humanName;
    "Protective armour that helps to defend against light alien melee "
      "attacks.",
    "icons/iconu_larmour",
    qtrue,                  //qboolean  purchasable;
    qfalse,                 //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_HELMET,              //int   number;
    qtrue,                  //qboolean enabled;
    HELMET_PRICE,           //int   price;
    qtrue,                  //qboolean warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_HEAD,              //int   slots;
    "helmet",               //char  *name;
    "Helmet",               //char  *humanName;
    "In addition to protecting your head, the helmet provides a "
      "scanner indicating the presence of any friendly or hostile "
      "lifeforms and structures in your immediate vicinity.",
    "icons/iconu_helmet",
    qtrue,                  //qboolean  purchasable;
    qfalse,                 //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_MEDKIT,              //int   number;
    qtrue,                  //qboolean enabled;
    MEDKIT_PRICE,           //int   price;
    qtrue,                  //qboolean warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_NONE,              //int   slots;
    "medkit",               //char  *name;
    "Medkit",               //char  *humanName;
    "",
    "icons/iconu_atoxin",
    qfalse,                 //qboolean  purchasable;
    qtrue,                  //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_BATTPACK,            //int   number;
    qtrue,                  //qboolean enabled;
    BATTPACK_PRICE,         //int   price;
    qtrue,                  //qboolean warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_BACKPACK,          //int   slots;
    "battpack",             //char  *name;
    "Battery Pack",         //char  *humanName;
    "Back-mounted battery pack that permits storage of one and a half "
      "times the normal energy capacity for energy weapons.",
    "icons/iconu_battpack",
    qtrue,                  //qboolean  purchasable;
    qfalse,                 //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_JETPACK,             //int   number;
    qtrue,                  //qboolean enabled;
    JETPACK_PRICE,          //int   price;
    qfalse,                 //qboolean warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_BACKPACK,          //int   slots;
    "jetpack",              //char  *name;
    "Jet Pack",             //char  *humanName;
    "Back-mounted jet pack that enables the user to fly to remote "
      "locations. It is very useful against alien spawns in hard "
      "to reach spots.",
    "icons/iconu_jetpack",
    qtrue,                  //qboolean  purchasable;
    qfalse,                  //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_BATTLESUIT,          //int   number;
    qtrue,                  //qboolean enabled;
    BSUIT_PRICE,            //int   price;
    qfalse,                 //qboolean warmupFree;
    ( 1 << S3 ),            //int   stages;
    SLOT_HEAD|SLOT_TORSO|SLOT_ARMS|SLOT_LEGS|SLOT_BACKPACK, //int  slots;
    "bsuit",                //char  *name;
    "Battlesuit",           //char  *humanName;
    "A full body armour that is highly effective at repelling alien attacks. "
      "It allows the user to enter hostile situations with a greater degree "
      "of confidence.",
    "icons/iconu_bsuit",
    qtrue,                  //qboolean  purchasable;
    qfalse,                 //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_GRENADE,             //int   number;
    qtrue,                  //qboolean enabled;
    GRENADE_PRICE,          //int   price;
    qfalse,                 //qboolean warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_NONE,              //int   slots;
    "gren",                 //char  *name;
    "Grenade",              //char  *humanName;
    "A small incendinary device ideal for damaging tightly packed "
      "alien structures. Has a five second timer.",
    0,
    qtrue,                  //qboolean  purchasable;
    qtrue,                  //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_FRAGNADE,            //int   number;
    qfalse,                 //qboolean enabled;
    FRAGNADE_PRICE,         //int   price;
    qfalse,                 //qboolean warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_NONE,              //int   slots;
    "frag",                 //char  *name;
    "Fragmentation Grenade", //char  *humanName;
    "A grenade with a heavy shell that, upon detonation, shatters "
      "into hundreds of high speed devestating fragments. Included is a "
      "built in gyroscope that kicks the grenade up to an optimal height before "
      "exploding. This grenade is most effective against larger targets.",
    0,
    qtrue,                  //qboolean  purchasable;
    qtrue,                  //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_AMMO,                //int   number;
    qtrue,                  //qboolean enabled;
    0,                      //int   price;
    qfalse,                 //qboolean warmupFree;
    ( 1 << S1 )|( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_NONE,              //int   slots;
    "ammo",                 //char  *name;
    "Ammunition",           //char  *humanName;
    "Ammunition for the currently held weapon.",
    0,
    qtrue,                  //qboolean  purchasable;
    qfalse,                 //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  },
  {
    UP_JETFUEL,             //int   number;
    qtrue,                  //qboolean enabled;
    0,                      //int   price;
    qfalse,                 //qboolean warmupFree;
    ( 1 << S2 )|( 1 << S3 ), //int  stages;
    SLOT_NONE,              //int   slots;
    "jetfuel",              //char  *name;
    "Jet Pack Fuel",        //char  *humanName;
    "Refuels the jet pack",
    0,
    qtrue,                  //qboolean  purchasable;
    qfalse,                 //qboolean  usable;
    TEAM_HUMANS             //team_t    team;
  }
};

size_t bg_numUpgrades = ARRAY_LEN( bg_upgrades );

static const upgradeAttributes_t nullUpgrade = { 0 };

/*
==============
BG_UpgradeByName
==============
*/
const upgradeAttributes_t *BG_UpgradeByName( const char *name )
{
  int i;

  for( i = 0; i < bg_numUpgrades; i++ )
  {
    if( !Q_stricmp( bg_upgrades[ i ].name, name ) )
    {
      return &bg_upgrades[ i ];
    }
  }

  return &nullUpgrade;
}

/*
==============
BG_Upgrade
==============
*/
const upgradeAttributes_t *BG_Upgrade( upgrade_t upgrade )
{
  return ( upgrade > UP_NONE && upgrade < UP_NUM_UPGRADES ) ?
    &bg_upgrades[ upgrade - 1 ] : &nullUpgrade;
}

/*
==============
BG_UpgradeAllowedInStage
==============
*/
qboolean BG_UpgradeAllowedInStage( upgrade_t upgrade, stage_t stage,
                                   int gameIsInWarmup )
{
  int stages = BG_Upgrade( upgrade )->stages;

  if( gameIsInWarmup )
    return qtrue;

  return stages & ( 1 << stage );
}


/*
===================================
BG_GetLCannonPrimaryFireSpeed

===================================
*/
int BG_GetLCannonPrimaryFireSpeed( int charge )
{
  return ( LCANNON_SPEED_MIN + ( charge -
           LCANNON_CHARGE_TIME_MAX ) * ( LCANNON_SECONDARY_SPEED - LCANNON_SPEED_MIN ) /
           ( ( ( LCANNON_CHARGE_TIME_MAX * LCANNON_SECONDARY_DAMAGE ) / LCANNON_DAMAGE ) -
           LCANNON_CHARGE_TIME_MAX ) );
}


////////////////////////////////////////////////////////////////////////////////

/*
================
BG_EvaluateTrajectory

================
*/
void BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result )
{
  float   deltaTime;
  float   phase;
  vec3_t  dir;

  switch( tr->trType )
  {
    case TR_STATIONARY:
    case TR_INTERPOLATE:
      VectorCopy( tr->trBase, result );
      break;

    case TR_LINEAR:
      deltaTime = ( atTime - tr->trTime ) * 0.001;  // milliseconds to seconds
      VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
      break;

    case TR_SINE:
      deltaTime = ( atTime - tr->trTime ) / (float)tr->trDuration;
      phase = sin( deltaTime * M_PI * 2 );
      VectorMA( tr->trBase, phase, tr->trDelta, result );
      break;

    case TR_LINEAR_STOP:
      if( atTime > tr->trTime + tr->trDuration )
        atTime = tr->trTime + tr->trDuration;

      deltaTime = ( atTime - tr->trTime ) * 0.001;  // milliseconds to seconds
      if( deltaTime < 0 )
        deltaTime = 0;

      VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
      break;

    case TR_GRAVITY:
      deltaTime = ( atTime - tr->trTime ) * 0.001;  // milliseconds to seconds
      VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
      result[ 2 ] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;   // FIXME: local gravity...
      break;

    case TR_BUOYANCY:
      deltaTime = ( atTime - tr->trTime ) * 0.001;  // milliseconds to seconds
      VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
      result[ 2 ] += 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;   // FIXME: local gravity...
      break;

    case TR_ACCEL:
      // time since missile fired in seconds
      deltaTime = ( atTime - tr->trTime ) * 0.001;

      // the .5*a*t^2 part. trDuration = acceleration,
      // phase gives the magnitude of the distance
      // we need to move
      phase = (tr->trDuration / 2) * (deltaTime * deltaTime);

      // Make dir equal to the velocity of the object
      VectorCopy (tr->trDelta, dir);

      // Sets the magnitude of vector dir to 1
      VectorNormalize (dir);

      // Move a distance "phase" in the direction "dir"
      // from our starting point
      VectorMA (tr->trBase, phase, dir, result);

      // The u*t part. Adds the velocity of the object
      // multiplied by the time to the last result.
      VectorMA (result, deltaTime, tr->trDelta, result);
      break;

    case TR_HALF_GRAVITY:
      deltaTime = ( atTime - tr->trTime ) * 0.001;  // milliseconds to seconds
      VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
      result[ 2 ] -= 0.5 * DEFAULT_GRAVITY * 0.5 * deltaTime * deltaTime;   // FIXME: local gravity...
      break;

    default:
      Com_Error( ERR_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trTime );
      break;
  }
}

/*
================
BG_EvaluateTrajectoryDelta

For determining velocity at a given time
================
*/
void BG_EvaluateTrajectoryDelta( const trajectory_t *tr, int atTime, vec3_t result )
{
  float deltaTime;
  float phase;
  vec3_t dir;

  switch( tr->trType )
  {
    case TR_STATIONARY:
    case TR_INTERPOLATE:
      VectorClear( result );
      break;

    case TR_LINEAR:
      VectorCopy( tr->trDelta, result );
      break;

    case TR_SINE:
      deltaTime = ( atTime - tr->trTime ) / (float)tr->trDuration;
      phase = cos( deltaTime * M_PI * 2 );  // derivative of sin = cos
      phase *= 2 * M_PI * 1000 / (float)tr->trDuration;
      VectorScale( tr->trDelta, phase, result );
      break;

    case TR_LINEAR_STOP:
      if( atTime > tr->trTime + tr->trDuration || atTime < tr->trTime )
      {
        VectorClear( result );
        return;
      }
      VectorCopy( tr->trDelta, result );
      break;

    case TR_GRAVITY:
      deltaTime = ( atTime - tr->trTime ) * 0.001;  // milliseconds to seconds
      VectorCopy( tr->trDelta, result );
      result[ 2 ] -= DEFAULT_GRAVITY * deltaTime;   // FIXME: local gravity...
      break;

    case TR_BUOYANCY:
      deltaTime = ( atTime - tr->trTime ) * 0.001;  // milliseconds to seconds
      VectorCopy( tr->trDelta, result );
      result[ 2 ] += DEFAULT_GRAVITY * deltaTime;   // FIXME: local gravity...
      break;

    case TR_ACCEL:
      // time since missile fired in seconds
      deltaTime = ( atTime - tr->trTime ) * 0.001;

      // Turn magnitude of acceleration into a vector
      VectorCopy(tr->trDelta,dir);
      VectorNormalize (dir);
      VectorScale (dir, tr->trDuration, dir);

      // u + t * a = v
      VectorMA (tr->trDelta, deltaTime, dir, result);
      break;

    case TR_HALF_GRAVITY:
      deltaTime = ( atTime - tr->trTime ) * 0.001;  // milliseconds to seconds
      VectorCopy( tr->trDelta, result );
      result[ 2 ] -= DEFAULT_GRAVITY * 0.5 * deltaTime;   // FIXME: local gravity...
      break;

    default:
      Com_Error( ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime );
      break;
  }
}

char *eventnames[ ] =
{
  "EV_NONE",

  "EV_FOOTSTEP",
  "EV_FOOTSTEP_METAL",
  "EV_FOOTSTEP_SQUELCH",
  "EV_FOOTSPLASH",
  "EV_FOOTWADE",
  "EV_SWIM",

  "EV_STEP_4",
  "EV_STEP_8",
  "EV_STEP_12",
  "EV_STEP_16",

  "EV_STEPDN_4",
  "EV_STEPDN_8",
  "EV_STEPDN_12",
  "EV_STEPDN_16",

  "EV_FALL_SHORT",
  "EV_FALL_MEDIUM",
  "EV_FALL_FAR",
  "EV_FALLING",

  "EV_JUMP",
  "EV_JETJUMP",
  "EV_WATER_TOUCH", // foot touches
  "EV_WATER_LEAVE", // foot leaves
  "EV_WATER_UNDER", // head touches
  "EV_WATER_CLEAR", // head leaves

  "EV_NOAMMO",
  "EV_CHANGE_WEAPON",
  "EV_FIRE_WEAPON",
  "EV_FIRE_WEAPON2",
  "EV_FIRE_WEAPON3",

  "EV_PLAYER_RESPAWN", // for fovwarp effects
  "EV_PLAYER_TELEPORT_IN",
  "EV_PLAYER_TELEPORT_OUT",

  "EV_GRENADE_BOUNCE",    // eventParm will be the soundindex

  "EV_GENERAL_SOUND",
  "EV_GLOBAL_SOUND",    // no attenuation

  "EV_BULLET_HIT_FLESH",
  "EV_BULLET_HIT_WALL",

  "EV_SPLATTER",
  "EV_MASS_DRIVER",

  "EV_MISSILE_HIT",
  "EV_MISSILE_MISS",
  "EV_MISSILE_MISS_METAL",
  "EV_TESLATRAIL",
  "EV_BULLET",        // otherEntity is the shooter

  "EV_LEV1_GRAB",
  "EV_LEV4_TRAMPLE_PREPARE",
  "EV_LEV4_TRAMPLE_START",

  "EV_PAIN",
  "EV_DEATH1",
  "EV_DEATH2",
  "EV_DEATH3",
  "EV_OBITUARY",

  "EV_GIB_PLAYER",

  "EV_BUILD_FIRE",
  "EV_BUILD_CONSTRUCT",
  "EV_BUILD_DESTROY",
  "EV_BUILD_DELAY",     // can't build yet
  "EV_BUILD_REPAIR",    // repairing buildable
  "EV_BUILD_REPAIRED",  // buildable has full health
  "EV_HUMAN_BUILDABLE_EXPLOSION",
  "EV_ALIEN_BUILDABLE_EXPLOSION",
  "EV_ALIEN_ACIDTUBE",

  "EV_MEDKIT_USED",

  "EV_ALIEN_SPAWN_PROTECTION_ENDED",
  "EV_ALIEN_EVOLVE",
  "EV_ALIEN_EVOLVE_FAILED",

  "EV_DEBUG_LINE",
  "EV_STOPLOOPINGSOUND",
  "EV_TAUNT",

  "EV_OVERMIND_ATTACK", // overmind under attack
  "EV_OVERMIND_DYING",  // overmind close to death
  "EV_OVERMIND_SPAWNS", // overmind needs spawns

  "EV_DCC_ATTACK",      // dcc under attack

  "EV_MGTURRET_SPINUP", // trigger a sound

  "EV_RPTUSE_SOUND",    // trigger a sound
  "EV_LEV2_ZAP",

  "EV_JETPACK_DEACTIVATE",
  "EV_JETPACK_REFUEL",

  "EV_FIGHT"
};

/*
===============
BG_EventName
===============
*/
const char *BG_EventName( int num )
{
  if( num < 0 || num >= ARRAY_LEN( eventnames ) )
    return "UNKNOWN";

  return eventnames[ num ];
}

/*
===============
BG_AddPredictableEventToPlayerstate

Handles the sequence numbers
===============
*/

void  Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

void BG_AddPredictableEventToPlayerstate( int newEvent, int eventParm, playerState_t *ps )
{
#ifdef DEBUG_EVENTS
  {
    char buf[ 256 ];
    Cvar_VariableStringBuffer( "showevents", buf, sizeof( buf ) );

    if( atof( buf ) != 0 )
    {
#ifdef GAME
      Com_Printf( " game event svt %5d -> %5d: num = %20s parm %d\n",
                  ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence,
                  BG_EventName( newEvent ), eventParm );
#else
      Com_Printf( "Cgame event svt %5d -> %5d: num = %20s parm %d\n",
                  ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence,
                  BG_EventName( newEvent ), eventParm );
#endif
    }
  }
#endif
  ps->events[ ps->eventSequence & ( MAX_PS_EVENTS - 1 ) ] = newEvent;
  ps->eventParms[ ps->eventSequence & ( MAX_PS_EVENTS - 1 ) ] = eventParm;
  ps->eventSequence++;
}


/*
========================
BG_PlayerStateToEntityState

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s,
                                  qboolean snap )
{
  int     i;

  if( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR || ps->pm_type == PM_FREEZE )
    s->eType = ET_INVISIBLE;
  else if( ps->persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT )
    s->eType = ET_INVISIBLE;
  else
    s->eType = ET_PLAYER;

  s->number = ps->clientNum;

  s->pos.trType = TR_INTERPOLATE;
  VectorCopy( ps->origin, s->pos.trBase );

  if( snap )
    SnapVector( s->pos.trBase );

  //set the trDelta for flag direction
  VectorCopy( ps->velocity, s->pos.trDelta );

  s->apos.trType = TR_INTERPOLATE;
  VectorCopy( ps->viewangles, s->apos.trBase );

  if( snap )
    SnapVector( s->apos.trBase );

  s->time2 = ps->movementDir;
  s->legsAnim = ps->legsAnim;
  s->torsoAnim = ps->torsoAnim;
  s->weaponAnim = ps->weaponAnim;
  s->clientNum = ps->clientNum;   // ET_PLAYER looks here instead of at number
                    // so corpses can also reference the proper config
  s->eFlags = ps->eFlags;
  if( ps->misc[ MISC_HEALTH ] <= 0 )
    s->eFlags |= EF_DEAD;
  else
    s->eFlags &= ~EF_DEAD;

  if( ps->stats[ STAT_STATE ] & SS_BLOBLOCKED )
    s->eFlags |= EF_BLOBLOCKED;
  else
    s->eFlags &= ~EF_BLOBLOCKED;

  if( ps->externalEvent )
  {
    s->event = ps->externalEvent;
    s->eventParm = ps->externalEventParm;
  }
  else if( ps->entityEventSequence < ps->eventSequence )
  {
    int   seq;

    if( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS )
      ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;

    seq = ps->entityEventSequence & ( MAX_PS_EVENTS - 1 );
    s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
    s->eventParm = ps->eventParms[ seq ];
    ps->entityEventSequence++;
  }

  s->weapon = ps->weapon;
  s->groundEntityNum = ps->groundEntityNum;

  //store items held and active items in modelindex and modelindex2
  s->modelindex = 0;
  s->modelindex2 = 0;
  for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
  {
    if( BG_InventoryContainsUpgrade( i, ps->stats ) )
    {
      s->modelindex |= 1 << i;

      if( BG_UpgradeIsActive( i, ps->stats ) )
        s->modelindex2 |= 1 << i;
    }
  }

  // use misc field to store team/class info:
  s->misc = ps->stats[ STAT_TEAM ] | ( ps->stats[ STAT_CLASS ] << 8 );

  // have to get the surfNormal through somehow...
  VectorCopy( ps->grapplePoint, s->angles2 );

  s->loopSound = ps->loopSound;
  s->generic1 = ps->generic1;

  if( s->generic1 <= WPM_NONE || s->generic1 >= WPM_NUM_WEAPONMODES )
    s->generic1 = WPM_PRIMARY;

  s->otherEntityNum = ps->otherEntityNum;

  s->otherEntityNum2 = ps->stats[ STAT_FLAGS ];

  s->constantLight = ps->stats[ STAT_MISC2 ];
}


/*
========================
BG_PlayerStateToEntityStateExtraPolate

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityStateExtraPolate( playerState_t *ps, entityState_t *s,
                                             int time, qboolean snap )
{
  int     i;

  if( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR || ps->pm_type == PM_FREEZE )
    s->eType = ET_INVISIBLE;
  else if( ps->persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT )
    s->eType = ET_INVISIBLE;
  else
    s->eType = ET_PLAYER;

  s->number = ps->clientNum;

  s->pos.trType = TR_LINEAR_STOP;
  VectorCopy( ps->origin, s->pos.trBase );

  if( snap )
    SnapVector( s->pos.trBase );

  // set the trDelta for flag direction and linear prediction
  VectorCopy( ps->velocity, s->pos.trDelta );
  // set the time for linear prediction
  s->pos.trTime = time;
  // set maximum extra polation time
  s->pos.trDuration = 50; // 1000 / sv_fps (default = 20)

  s->apos.trType = TR_INTERPOLATE;
  VectorCopy( ps->viewangles, s->apos.trBase );
  if( snap )
    SnapVector( s->apos.trBase );

  s->time2 = ps->movementDir;
  s->legsAnim = ps->legsAnim;
  s->torsoAnim = ps->torsoAnim;
  s->weaponAnim = ps->weaponAnim;
  s->clientNum = ps->clientNum;   // ET_PLAYER looks here instead of at number
                    // so corpses can also reference the proper config
  s->eFlags = ps->eFlags;

  if( ps->misc[ MISC_HEALTH ] <= 0 )
    s->eFlags |= EF_DEAD;
  else
    s->eFlags &= ~EF_DEAD;

  if( ps->stats[ STAT_STATE ] & SS_BLOBLOCKED )
    s->eFlags |= EF_BLOBLOCKED;
  else
    s->eFlags &= ~EF_BLOBLOCKED;

  if( ps->externalEvent )
  {
    s->event = ps->externalEvent;
    s->eventParm = ps->externalEventParm;
  }
  else if( ps->entityEventSequence < ps->eventSequence )
  {
    int   seq;

    if( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS )
      ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;

    seq = ps->entityEventSequence & ( MAX_PS_EVENTS - 1 );
    s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
    s->eventParm = ps->eventParms[ seq ];
    ps->entityEventSequence++;
  }

  s->weapon = ps->weapon;
  s->groundEntityNum = ps->groundEntityNum;

  //store items held and active items in modelindex and modelindex2
  s->modelindex = 0;
  s->modelindex2 = 0;

  for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
  {
    if( BG_InventoryContainsUpgrade( i, ps->stats ) )
    {
      s->modelindex |= 1 << i;

      if( BG_UpgradeIsActive( i, ps->stats ) )
        s->modelindex2 |= 1 << i;
    }
  }

  // use misc field to store team/class info:
  s->misc = ps->stats[ STAT_TEAM ] | ( ps->stats[ STAT_CLASS ] << 8 );

  // have to get the surfNormal through somehow...
  VectorCopy( ps->grapplePoint, s->angles2 );

  s->loopSound = ps->loopSound;
  s->generic1 = ps->generic1;

  if( s->generic1 <= WPM_NONE || s->generic1 >= WPM_NUM_WEAPONMODES )
    s->generic1 = WPM_PRIMARY;

  s->otherEntityNum = ps->otherEntityNum;

  s->otherEntityNum2 = ps->stats[ STAT_FLAGS ];

  s->constantLight = ps->stats[ STAT_MISC2 ];
}

/*
========================
BG_WeaponIsFull

Check if a weapon has full ammo
========================
*/
qboolean BG_WeaponIsFull( weapon_t weapon, int stats[ ], int ammo, int clips )
{
  int maxAmmo, maxClips;

  maxAmmo = BG_Weapon( weapon )->maxAmmo;
  maxClips = BG_Weapon( weapon )->maxClips;

  if( BG_Weapon(weapon)->usesEnergy && BG_InventoryContainsUpgrade( UP_BATTPACK, stats ) )
    maxAmmo = (int)( (float)maxAmmo * BATTPACK_MODIFIER );

  return ( maxAmmo == ammo ) && ( maxClips == clips );
}

/*
========================
BG_InventoryContainsWeapon

Does the player hold a weapon?
========================
*/
qboolean BG_InventoryContainsWeapon( int weapon, int stats[ ] )
{
  // humans always have a blaster
  if( stats[ STAT_TEAM ] == TEAM_HUMANS && weapon == WP_BLASTER )
    return qtrue;

  return ( stats[ STAT_WEAPON ] == weapon );
}

/*
========================
BG_SlotsForInventory

Calculate the slots used by an inventory and warn of conflicts
========================
*/
int BG_SlotsForInventory( int stats[ ] )
{
  int i, slot, slots;

  slots = BG_Weapon( stats[ STAT_WEAPON ] )->slots;
  if( stats[ STAT_TEAM ] == TEAM_HUMANS )
    slots |= BG_Weapon( WP_BLASTER )->slots;

  for( i = UP_NONE; i < UP_NUM_UPGRADES; i++ )
  {
    if( BG_InventoryContainsUpgrade( i, stats ) )
    {
      slot = BG_Upgrade( i )->slots;

      // this check should never be true
      if( slots & slot )
      {
        Com_Printf( S_COLOR_YELLOW "WARNING: held item %d conflicts with "
                    "inventory slot %d\n", i, slot );
      }

      slots |= slot;
    }
  }

  return slots;
}

/*
========================
BG_AddUpgradeToInventory

Give the player an upgrade
========================
*/
void BG_AddUpgradeToInventory( int item, int stats[ ] )
{
  stats[ STAT_ITEMS ] |= ( 1 << item );
}

/*
========================
BG_RemoveUpgradeFromInventory

Take an upgrade from the player
========================
*/
void BG_RemoveUpgradeFromInventory( int item, int stats[ ] )
{
  stats[ STAT_ITEMS ] &= ~( 1 << item );
}

/*
========================
BG_InventoryContainsUpgrade

Does the player hold an upgrade?
========================
*/
qboolean BG_InventoryContainsUpgrade( int item, int stats[ ] )
{
  return( stats[ STAT_ITEMS ] & ( 1 << item ) );
}

/*
========================
BG_ActivateUpgrade

Activates an upgrade
========================
*/
void BG_ActivateUpgrade( int item, int stats[ ] )
{
  if( item != UP_JETPACK || stats[ STAT_FUEL ] >  0)
    stats[ STAT_ACTIVEITEMS ] |= ( 1 << item );
}

/*
========================
BG_DeactivateUpgrade

Deactivates an upgrade
========================
*/
void BG_DeactivateUpgrade( int item, int stats[ ] )
{
  stats[ STAT_ACTIVEITEMS ] &= ~( 1 << item );
}

/*
========================
BG_UpgradeIsActive

Is this upgrade active?
========================
*/
qboolean BG_UpgradeIsActive( int item, int stats[ ] )
{
  return( stats[ STAT_ACTIVEITEMS ] & ( 1 << item ) );
}

/*
===============
BG_RotateAxis

Shared axis rotation function
===============
*/
qboolean BG_RotateAxis( vec3_t surfNormal, vec3_t inAxis[ 3 ],
                        vec3_t outAxis[ 3 ], qboolean inverse, qboolean ceiling )
{
  vec3_t  refNormal = { 0.0f, 0.0f, 1.0f };
  vec3_t  ceilingNormal = { 0.0f, 0.0f, -1.0f };
  vec3_t  localNormal, xNormal;
  float   rotAngle;

  //the grapplePoint being a surfNormal rotation Normal hack... see above :)
  if( ceiling )
  {
    VectorCopy( ceilingNormal, localNormal );
    VectorCopy( surfNormal, xNormal );
  }
  else
  {
    //cross the reference normal and the surface normal to get the rotation axis
    VectorCopy( surfNormal, localNormal );
    CrossProduct( localNormal, refNormal, xNormal );
    VectorNormalize( xNormal );
  }

  //can't rotate with no rotation vector
  if( VectorLength( xNormal ) != 0.0f )
  {
    rotAngle = RAD2DEG( acos( DotProduct( localNormal, refNormal ) ) );

    if( inverse )
      rotAngle = -rotAngle;

    AngleNormalize180( rotAngle );

    //hmmm could get away with only one rotation and some clever stuff later... but i'm lazy
    RotatePointAroundVector( outAxis[ 0 ], xNormal, inAxis[ 0 ], -rotAngle );
    RotatePointAroundVector( outAxis[ 1 ], xNormal, inAxis[ 1 ], -rotAngle );
    RotatePointAroundVector( outAxis[ 2 ], xNormal, inAxis[ 2 ], -rotAngle );
  }
  else
    return qfalse;

  return qtrue;
}

/*
===============
BG_GetClientNormal

Get the normal for the surface the client is walking on
===============
*/
void BG_GetClientNormal( const playerState_t *ps, vec3_t normal )
{
  if( ps->stats[ STAT_STATE ] & SS_WALLCLIMBING )
  {
    if( ps->eFlags & EF_WALLCLIMBCEILING )
      VectorSet( normal, 0.0f, 0.0f, -1.0f );
    else
      VectorCopy( ps->grapplePoint, normal );
  }
  else
    VectorSet( normal, 0.0f, 0.0f, 1.0f );
}

/*
===============
BG_GetClientViewOrigin

Get the position of the client's eye, based on the client's position, the surface's normal, and client's view height
===============
*/
void BG_GetClientViewOrigin( const playerState_t *ps, vec3_t viewOrigin )
{
  vec3_t normal;
  BG_GetClientNormal( ps, normal );
  VectorMA( ps->origin, ps->viewheight, normal, viewOrigin );
}

/*
===============
BG_CalcMuzzlePointFromPS

set muzzle location relative to pivoting eye
===============
*/
void BG_CalcMuzzlePointFromPS( const playerState_t *ps, vec3_t forward,
                               vec3_t right, vec3_t up, vec3_t muzzlePoint )
{
  vec3_t normal, view_angles;
  float  xyspeed;
  float  bobfracsin, bob, bob2;
  int    bobcycle;

  VectorCopy(ps->viewangles, view_angles);

  // initialize bobbing values;
  bobcycle = (ps->misc[MISC_BOB_CYCLE] & 2048) >> 11;
  bobfracsin = fabs(sin((ps->misc[MISC_BOB_CYCLE] & 2047) / 2047.0 * M_PI));
  xyspeed =
    sqrt(ps->velocity[0] * ps->velocity[0] + ps->velocity[1] * ps->velocity[1]);
  // the bob velocity should't get too fast to avoid jerking
  if(xyspeed > 300.0f) {
    xyspeed = 300.0f;
  }
  // bob amount is class dependant
  if( ps->persistant[PERS_SPECSTATE] != SPECTATOR_NOT) {
    bob2 = 0.0f;
  } else {
    bob2 = BG_Class(ps->stats[ STAT_CLASS ])->bob;
  }

  // adjust the angles for bobbing
  if(bob2 != 0.0f) {
    float speed;
    float delta;

    // make sure the bob is visible even at low speeds
    speed = xyspeed > 200 ? xyspeed : 200;

    delta = bobfracsin * bob2 * speed;
    if(ps->pm_flags & PMF_DUCKED) {
      delta *= 3;   // crouching
    }

    view_angles[ PITCH ] += delta;
    delta = bobfracsin * bob2 * speed;
    if(ps->pm_flags & PMF_DUCKED) {
      delta *= 3;   // crouching accentuates roll
    }

    if(bobcycle & 15) {
      delta = -delta;
    }

    view_angles[ ROLL ] += delta;
  }

  AngleVectors(view_angles, forward, right, up);
  VectorCopy( ps->origin, muzzlePoint );
  BG_GetClientNormal( ps, normal );
  VectorMA( muzzlePoint, ps->viewheight, normal, muzzlePoint );
  VectorMA( muzzlePoint, 1, forward, muzzlePoint );

  // add bob height
  bob = bobfracsin * xyspeed * bob2;

  if( bob > 6 )
    bob = 6;

  VectorMA( muzzlePoint, bob, normal, muzzlePoint );

  // snap to integer coordinates for more efficient network bandwidth usage
  SnapVector( muzzlePoint );
}

/*
===============
BG_LightningBoltRange

Finds the current lightning bolt range of a charged lightning gun
===============
*/
int BG_LightningBoltRange( const entityState_t *es,
                           const playerState_t *ps,
                           qboolean currentRange )
{
  if( ps )
  {
    int charge;

    Com_Assert( ps->weapon == WP_LIGHTNING );

    if( currentRange )
      charge = ps->misc[ MISC_MISC ];
    else
      charge = ps->stats[ STAT_MISC2 ];

    return ( charge * LIGHTNING_BOLT_RANGE_MAX ) / LIGHTNING_BOLT_CHARGE_TIME_MAX;
  }
  
  Com_Assert( es );
  Com_Assert( es->weapon == WP_LIGHTNING );
  return ( es->constantLight * LIGHTNING_BOLT_RANGE_MAX ) / LIGHTNING_BOLT_CHARGE_TIME_MAX;
}

/*
===============
BG_CheckBoltImpactTrigger

For firing lightning bolts early
===============
*/
void BG_CheckBoltImpactTrigger(
  pmove_t *pm,
  void (*trace)(
    trace_t *, const vec3_t, const vec3_t,
    const vec3_t, const vec3_t, int, int),
  void (*UnlaggedOn)(unlagged_attacker_data_t *),
  void (*UnlaggedOff)(void)) {
  if(
    pm->ps->weapon == WP_LIGHTNING &&
    pm->ps->misc[ MISC_MISC ] > LIGHTNING_BOLT_CHARGE_TIME_MIN &&
    pm->ps->misc[ MISC_MISC ] - pm->ps->stats[ STAT_MISC3 ] > 50 ) {
    vec3_t end;

    if(UnlaggedOn) {
      unlagged_attacker_data_t attacker_data;

      attacker_data.ent_num = pm->ps->clientNum;
      attacker_data.point_type = UNLGD_PNT_MUZZLE;
      attacker_data.range = BG_LightningBoltRange(NULL, pm->ps, qtrue);
      UnlaggedOn(&attacker_data);

      VectorMA(attacker_data.muzzle_out, BG_LightningBoltRange(NULL, pm->ps, qtrue),
                attacker_data.forward_out, end );

      trace(
        &pm->pmext->impactTriggerTrace, attacker_data.muzzle_out, NULL, NULL, end,
        pm->ps->clientNum, MASK_SHOT );

      if(UnlaggedOff) {
        UnlaggedOff( );
      }
    } else {
      vec3_t forward, right, up;
      vec3_t muzzle;

      BG_CalcMuzzlePointFromPS(pm->ps, forward, right, up, muzzle);

      VectorMA(muzzle, BG_LightningBoltRange(NULL, pm->ps, qtrue),
                forward, end );
      trace( &pm->pmext->impactTriggerTrace, muzzle, NULL, NULL, end,
                pm->ps->clientNum, MASK_SHOT );
    }

    pm->ps->stats[ STAT_MISC3 ] = pm->ps->misc[ MISC_MISC ];
    pm->pmext->impactTriggerTraceChecked = qtrue;
  } else
    pm->pmext->impactTriggerTraceChecked = qfalse;
}

/*
===============
BG_ResetLightningBoltCharge

Resets the charging of lightning bolts
===============
*/
void BG_ResetLightningBoltCharge( playerState_t *ps, pmoveExt_t *pmext )
{
  if( ps->weapon != WP_LIGHTNING )
    return;

  ps->stats[ STAT_MISC3 ] = 0;
  ps->misc[ MISC_MISC ] = 0;
  pmext->impactTriggerTraceChecked = qfalse;
}

/*
===============
BG_FindValidSpot
===============
*/
qboolean BG_FindValidSpot(
  void (*trace)(
    trace_t *, const vec3_t, const vec3_t, const vec3_t, const vec3_t, int, int),
  trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end,
  int passEntityNum, int contentmask, float incDist, int limit) {
  vec3_t start2, increment;

  VectorCopy(start, start2);

  VectorSubtract(end, start2, increment);
  VectorNormalize(increment);
  VectorScale(increment, incDist, increment);

  do {
    (*trace)(tr, start2, mins, maxs, end, passEntityNum, contentmask);
    VectorAdd(tr->endpos, increment, start2);
    if(!tr->allsolid) {
      return qtrue;
    }
    limit--;
  } while (tr->fraction < 1.0f && limit >= 0);
  return qfalse;
}

/*
===============
BG_PositionBuildableRelativeToPlayer

Find a place to build a buildable
===============
*/
//TODO: Partial move of canbuild to this function to allow quicker updates for the red shader.
void BG_PositionBuildableRelativeToPlayer(
  const playerState_t *ps, const qboolean builder_adjacent_placement,
  void (*trace)(
    trace_t *, const vec3_t, const vec3_t, const vec3_t, const vec3_t, int, int),
  vec3_t outOrigin, vec3_t outAngles, trace_t *tr) {
  vec3_t forward, targetOrigin;
  vec3_t playerNormal;
  vec3_t mins, maxs;
  float  buildDist;
  const  buildable_t buildable = ps->stats[STAT_BUILDABLE] & ~SB_VALID_TOGGLEBIT;
 
  BG_BuildableBoundingBox(buildable, mins, maxs);

  BG_GetClientNormal(ps, playerNormal);

  VectorCopy(ps->viewangles, outAngles);

  if(builder_adjacent_placement) {
    vec3_t right;

    AngleVectors(outAngles, NULL, right, NULL);
    CrossProduct(playerNormal, right, forward);
  } else {
    AngleVectors(outAngles, forward, NULL, NULL);
  }

  {
    vec3_t viewOrigin, startOrigin;
    vec3_t mins2, maxs2;
    vec3_t builderBottom;
    trace_t builder_bottom_trace;
    const float minNormal = BG_Buildable(buildable)->minNormal;
    const qboolean invertNormal = BG_Buildable(buildable)->invertNormal;
    qboolean validAngle;
    const int conditional_pass_ent_num =
      builder_adjacent_placement ? MAGIC_TRACE_HACK : ps->clientNum;
    float heightOffset = 0.0f;
    float builderDia;
    vec3_t builderMins, builderMaxs;
    const qboolean preciseBuild = 
      (ps->stats[STAT_STATE] & SS_PRECISE_BUILD) ||
      BG_Class(ps->stats[STAT_CLASS])->buildPreciseForce;
    qboolean onBuilderPlane = qfalse;

    BG_ClassBoundingBox(
      ps->stats[STAT_CLASS], builderMins, builderMaxs, NULL, NULL, NULL);
    builderDia = RadiusFromBounds(builderMins, builderMaxs);
    builderDia *= 2.0f;

    if(builder_adjacent_placement) {
      float projected_maxs, projected_mins;
  
      projected_maxs =
        fabs(maxs[0] * forward[0]) +
        fabs(maxs[1] * forward[1]) +
        fabs(maxs[2] * forward[2]);
      projected_mins =
        fabs(mins[0] * forward[0]) +
        fabs(mins[1] * forward[1]) +
        fabs(mins[2] * forward[2]);
      buildDist = max(projected_maxs, projected_mins) + builderDia + 5.0f;
    } else if(preciseBuild) {
      buildDist = BG_Class(ps->stats[STAT_CLASS])->buildDistPrecise;
    } else {
      buildDist = BG_Class(ps->stats[STAT_CLASS])->buildDist;
    }

    BG_GetClientViewOrigin(ps, viewOrigin);

    maxs2[ 0 ] = maxs2[ 1 ] = 14;
    maxs2[ 2 ] = ps->stats[ STAT_TEAM ] == TEAM_HUMANS ? 7 :maxs2[ 0 ];
    mins2[ 0 ] = mins2[ 1 ] = -maxs2[ 0 ];
    mins2[ 2 ] = 0;

    //trace the bottom of the builder
    VectorMA(viewOrigin, -builderDia, playerNormal, builderBottom);
    (*trace)(
      &builder_bottom_trace, viewOrigin, mins2, maxs2, builderBottom,
      ps->clientNum, MASK_PLAYERSOLID );
    VectorCopy(builder_bottom_trace.endpos, builderBottom);
    VectorMA(builderBottom, 0.5f, playerNormal, builderBottom);

    if(builder_adjacent_placement) {
      VectorCopy(builderBottom, startOrigin);
    } else {
      VectorCopy(viewOrigin, startOrigin);
    }

    VectorMA( startOrigin, buildDist, forward, targetOrigin );

    {
      {//Do a small bbox trace to find the true targetOrigin.
        vec3_t targetNormal;

        (*trace)( tr, startOrigin, mins2, maxs2, targetOrigin, ps->clientNum, MASK_PLAYERSOLID );
        if( tr->startsolid || tr->allsolid ) {
          VectorCopy( viewOrigin, outOrigin );
          tr->plane.normal[ 2 ] = 0.0f;
          tr->entityNum = ENTITYNUM_NONE;
          return;
        }
        VectorCopy( tr->endpos, targetOrigin );

        //check if tracing should be view the view or from the base of the builder
        VectorCopy(tr->plane.normal, targetNormal);
        if(
          ps->groundEntityNum != ENTITYNUM_NONE &&
          (
            builder_adjacent_placement ||
            VectorCompare(playerNormal, targetNormal))) {

          if(builder_adjacent_placement) {
            vec3_t end;
            trace_t tr2;

            VectorCopy(playerNormal, targetNormal);

            //move the target origin away from any collided surface
            VectorSubtract(startOrigin, targetOrigin, end);
            VectorNormalize(end);
            VectorMA(targetOrigin, 1.0f, end, end);
            (*trace)(
              &tr2, startOrigin, mins2, maxs2, end, MAGIC_TRACE_HACK,
              MASK_PLAYERSOLID);
            if(!tr2.allsolid && tr2.fraction >= 1.0f) {
              VectorCopy(tr2.endpos, startOrigin);
            } else {
              VectorCopy(viewOrigin, outOrigin);
              tr->plane.normal[2] = 0.0f;
              tr->entityNum = ENTITYNUM_NONE;
              return;
            }

            //nudge the target origin back down to undo the offset
            VectorMA(targetOrigin, -0.5f, targetNormal, end);
            (*trace)(
              &tr2, targetOrigin, mins2, maxs2, end, ps->clientNum,
              MASK_PLAYERSOLID);
            if(!tr2.startsolid && !tr2.allsolid) {
              VectorCopy(tr2.endpos, targetOrigin);
            }
          }

          if(!builder_bottom_trace.startsolid && !builder_bottom_trace.allsolid) {
            vec3_t end;
            trace_t tr2;

            //check that there is a clear trace from the builderBottom to the target
            VectorMA(targetOrigin, 0.5, targetNormal, end);
            (*trace)(
              &tr2, builderBottom, mins2, maxs2, end, ps->clientNum,
              MASK_PLAYERSOLID);
            if(!tr2.startsolid && !tr2.allsolid && tr2.fraction >= 1.0f) {
              //undo the offset
              VectorMA(builderBottom, -0.5f, playerNormal, builderBottom);
              //use the builderBottom
              VectorCopy(builderBottom, startOrigin);
              onBuilderPlane = qtrue;
            }
          }
        }
      }

      {//Center height and find the smallest axis.  This assumes that all x and y axis are the same magnitude.
        heightOffset = -(maxs[2] + mins[2]) / 2.0f;

        maxs[2] += heightOffset;
        mins[2] += heightOffset;
      }

      if(
        (minNormal > 0.0f && !invertNormal) ||
        preciseBuild ||
        onBuilderPlane ||
        builder_adjacent_placement) {
        //Raise origins by 1+maxs[2].
        VectorMA(startOrigin, maxs[2] + 1.0f, playerNormal, startOrigin);
        VectorMA(targetOrigin, maxs[2] + 1.0f, playerNormal, targetOrigin);
      }

      if(builder_adjacent_placement) {
        vec3_t  temp;
        trace_t tr2;

        //swap the startOrigin with the targetOrigin to position back against the builder
        VectorCopy(startOrigin, temp);
        VectorCopy(targetOrigin, startOrigin);
        VectorCopy(temp, targetOrigin);

        //position the target origin to collide back against the builder.
        (*trace)(
          &tr2, startOrigin, mins2, maxs2, targetOrigin, MAGIC_TRACE_HACK,
          MASK_PLAYERSOLID);
        if(!tr2.startsolid && !tr2.allsolid) {
          VectorCopy(tr2.endpos, targetOrigin);

          //allow for a gap between the builder and the builder
          VectorSubtract(startOrigin, targetOrigin, temp);
          VectorNormalize(temp);
          VectorMA(targetOrigin, 5.0f, temp, temp);
          (*trace)(
            &tr2, targetOrigin, mins2, maxs2, temp, ps->clientNum,
            MASK_PLAYERSOLID);
          if(!tr2.startsolid && !tr2.allsolid && tr2.fraction >= 1.0f) {
            VectorCopy(tr2.endpos, targetOrigin);
          } else {
            VectorCopy( viewOrigin, outOrigin );
            tr->plane.normal[ 2 ] = 0.0f;
            tr->entityNum = ENTITYNUM_NONE;
            return;
          }
        } else {
          VectorCopy( viewOrigin, outOrigin );
          tr->plane.normal[ 2 ] = 0.0f;
          tr->entityNum = ENTITYNUM_NONE;
          return;
        }
      }

      {//Do traces from behind the player to the target to find a valid spot.
        trace_t tr2;

        if(
          !BG_FindValidSpot(
            trace, tr, startOrigin, mins, maxs, targetOrigin,
            conditional_pass_ent_num, MASK_PLAYERSOLID, 5.0f, 5)) {
          VectorCopy(viewOrigin, outOrigin);
          tr->plane.normal[2] = 0.0f;
          tr->entityNum = ENTITYNUM_NONE;
          return;
        }

        //Check that the spot is not on the opposite side of a thin wall
        (*trace)(
          &tr2, startOrigin, NULL, NULL, tr->endpos, ps->clientNum,
          MASK_PLAYERSOLID);
        if(tr2.fraction < 1.0f || tr2.startsolid || tr2.allsolid) {
          VectorCopy(viewOrigin, outOrigin);
          tr->plane.normal[2] = 0.0f;
          tr->entityNum = ENTITYNUM_NONE;
          return;
        }
      }
    }

    validAngle =
      tr->plane.normal[2] >=
        minNormal || (invertNormal && tr->plane.normal[2] <= -minNormal);

    //Down trace if precision building, builder adjacent placement, no hit, or surface is too steep.
    if(
      preciseBuild ||
      builder_adjacent_placement ||
      tr->fraction >= 1.0f ||
      !validAngle ) {//TODO: These should be utility functions like "if(traceHit(&tr))"
      if(tr->fraction < 1.0f) {
        //Bring endpos away from surface it has hit.
        VectorAdd(tr->endpos, tr->plane.normal, tr->endpos);
      }

      {
        vec3_t startOrigin;

        VectorMA(tr->endpos, -buildDist / 2.0f, playerNormal, targetOrigin);

        VectorCopy(tr->endpos, startOrigin);

        (*trace)(
          tr, startOrigin, mins, maxs, targetOrigin, ps->clientNum,
          MASK_PLAYERSOLID);
      }
    }

    if(!builder_adjacent_placement) {
      trace_t tr2;

      //check if this position would collide with the builder
      (*trace)(
        &tr2, tr->endpos, mins, maxs, tr->endpos, MAGIC_TRACE_HACK,
        MASK_PLAYERSOLID);

      if((tr2.startsolid || tr2.allsolid) && tr2.entityNum == ps->clientNum) {
        //attempt to position buildable adjacent to the builder
        BG_PositionBuildableRelativeToPlayer(
          ps, qtrue, trace, outOrigin, outAngles, tr);
        return;
      }
    }

    tr->endpos[2] += heightOffset;
  }
  VectorCopy(tr->endpos, outOrigin);
}

/*
===============
BG_GetValueOfPlayer

Returns the credit value of a player
===============
*/
int BG_GetValueOfPlayer( playerState_t *ps )
{
  int worth = 0;

  worth = BG_Class( ps->stats[ STAT_CLASS ] )->value;

  // Humans have worth from their equipment as well
  if( ps->stats[ STAT_TEAM ] == TEAM_HUMANS )
  {
    upgrade_t i;
    for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
    {
      if( BG_InventoryContainsUpgrade( i, ps->stats ) )
        worth += BG_Upgrade( i )->price;
    }

    if( ps->stats[ STAT_WEAPON ] != WP_NONE )
    {
      int totalAmmo = ps->ammo +
                      ( ps->clips * BG_Weapon( ps->stats[ STAT_WEAPON ] )->maxAmmo );

      worth += BG_Weapon( ps->stats[ STAT_WEAPON ] )->price;
      if( !BG_Weapon( ps->stats[ STAT_WEAPON ] )->usesEnergy )
        worth += totalAmmo * BG_Weapon( ps->stats[ STAT_WEAPON ] )->roundPrice;
    }
  }

  return worth;
}

/*
=================
BG_PlayerCanChangeWeapon
=================
*/
qboolean BG_PlayerCanChangeWeapon( playerState_t *ps, pmoveExt_t *pmext )
{
  // Do not allow Lucifer Cannon "canceling" via weapon switch
  if( ps->weapon == WP_LUCIFER_CANNON &&
      ps->misc[ MISC_MISC ] > LCANNON_CHARGE_TIME_MIN )
    return qfalse;

  if( ps->weapon == WP_LIGHTNING &&
      ( ps->misc[ MISC_MISC ] > LIGHTNING_BOLT_CHARGE_TIME_MIN ||
        ( ps->stats[ STAT_STATE ] & SS_CHARGING ) ) )
    return qfalse;

  //bursts must complete
  if( pmext->burstRoundsToFire[ 2 ] ||
      pmext->burstRoundsToFire[ 1 ] ||
      pmext->burstRoundsToFire[ 0 ] )
    return qfalse;

  return ps->weaponTime <= 0 || ps->weaponstate != WEAPON_FIRING;
}

/*
=================
BG_PlayerPoisonCloudTime
=================
*/
int BG_PlayerPoisonCloudTime( playerState_t *ps )
{
  int time = LEVEL1_PCLOUD_TIME;

  if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, ps->stats ) )
    time -= BSUIT_PCLOUD_PROTECTION;
  if( BG_InventoryContainsUpgrade( UP_HELMET, ps->stats ) )
    time -= HELMET_PCLOUD_PROTECTION;
  if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, ps->stats ) )
    time -= LIGHTARMOUR_PCLOUD_PROTECTION;

  return time;
}

/*
=================
BG_GetPlayerWeapon

Returns the players current weapon or the weapon they are switching to.
Only needs to be used for human weapons.
=================
*/
weapon_t BG_GetPlayerWeapon( playerState_t *ps )
{
  if( ps->persistant[ PERS_NEWWEAPON ] )
    return ps->persistant[ PERS_NEWWEAPON ];

  return ps->weapon;
}

/*
===============
atof_neg

atof with an allowance for negative values
===============
*/
float atof_neg( char *token, qboolean allowNegative )
{
  float value;

  value = atof( token );

  if( !allowNegative && value < 0.0f )
    value = 1.0f;

  return value;
}

/*
===============
atoi_neg

atoi with an allowance for negative values
===============
*/
int atoi_neg( char *token, qboolean allowNegative )
{
  int value;

  value = atoi( token );

  if( !allowNegative && value < 0 )
    value = 1;

  return value;
}

#define MAX_NUM_PACKED_ENTITY_NUMS 10

/*
===============
BG_UnpackEntityNumbers

Unpack entity numbers from an entityState_t
===============
*/
int BG_UnpackEntityNumbers( entityState_t *es, int *entityNums, int count )
{
  int i;

  if( count > MAX_NUM_PACKED_ENTITY_NUMS )
    count = MAX_NUM_PACKED_ENTITY_NUMS;

  for( i = 0; i < count; i++ )
  {
    int *entityNum = &entityNums[ i ];

    switch( i )
    {
      case 0: *entityNum = es->misc;                                      break;
      case 1: *entityNum = es->time;                                      break;
      case 2: *entityNum = (es->time >> GENTITYNUM_BITS);                 break;
      case 3: *entityNum = (es->time >> (GENTITYNUM_BITS * 2));           break;
      case 4: *entityNum = es->time2;                                     break;
      case 5: *entityNum = (es->time2 >> GENTITYNUM_BITS);                break;
      case 6: *entityNum = (es->time2 >> (GENTITYNUM_BITS * 2));          break;
      case 7: *entityNum = es->constantLight;                             break;
      case 8: *entityNum = (es->constantLight >> GENTITYNUM_BITS);        break;
      case 9: *entityNum = (es->constantLight >> (GENTITYNUM_BITS * 2));  break;
      default: Com_Error( ERR_FATAL, "Entity index %d not handled", i );  break;
    }

    *entityNum &= GENTITYNUM_MASK;

    if( *entityNum == ENTITYNUM_NONE )
      break;
  }

  return i;
}

/*
===============
BG_ParseCSVEquipmentList
===============
*/
void BG_ParseCSVEquipmentList( const char *string, weapon_t *weapons, int weaponsSize,
    upgrade_t *upgrades, int upgradesSize )
{
  char      buffer[ MAX_STRING_CHARS ];
  int       i = 0, j = 0;
  char      *p, *q;
  qboolean  EOS = qfalse;

  Q_strncpyz( buffer, string, MAX_STRING_CHARS );

  p = q = buffer;

  while( *p != '\0' )
  {
    //skip to first , or EOS
    while( *p != ',' && *p != '\0' )
      p++;

    if( *p == '\0' )
      EOS = qtrue;

    *p = '\0';

    //strip leading whitespace
    while( *q == ' ' )
      q++;

    if( weaponsSize )
      weapons[ i ] = BG_WeaponByName( q )->number;

    if( upgradesSize )
      upgrades[ j ] = BG_UpgradeByName( q )->number;

    if( weaponsSize && weapons[ i ] == WP_NONE &&
        upgradesSize && upgrades[ j ] == UP_NONE )
      Com_Printf( S_COLOR_YELLOW "WARNING: unknown equipment %s\n", q );
    else if( weaponsSize && weapons[ i ] != WP_NONE )
      i++;
    else if( upgradesSize && upgrades[ j ] != UP_NONE )
      j++;

    if( !EOS )
    {
      p++;
      q = p;
    }
    else
      break;

    if( i == ( weaponsSize - 1 ) || j == ( upgradesSize - 1 ) )
      break;
  }

  if( weaponsSize )
    weapons[ i ] = WP_NONE;

  if( upgradesSize )
    upgrades[ j ] = UP_NONE;
}

/*
===============
BG_ParseCSVClassList
===============
*/
void BG_ParseCSVClassList( const char *string, class_t *classes, int classesSize )
{
  char      buffer[ MAX_STRING_CHARS ];
  int       i = 0;
  char      *p, *q;
  qboolean  EOS = qfalse;

  Q_strncpyz( buffer, string, MAX_STRING_CHARS );

  p = q = buffer;

  while( *p != '\0' && i < classesSize - 1 )
  {
    //skip to first , or EOS
    while( *p != ',' && *p != '\0' )
      p++;

    if( *p == '\0' )
      EOS = qtrue;

    *p = '\0';

    //strip leading whitespace
    while( *q == ' ' )
      q++;

    classes[ i ] = BG_ClassByName( q )->number;

    if( classes[ i ] == PCL_NONE )
      Com_Printf( S_COLOR_YELLOW "WARNING: unknown class %s\n", q );
    else
      i++;

    if( !EOS )
    {
      p++;
      q = p;
    }
    else
      break;
  }

  classes[ i ] = PCL_NONE;
}

/*
===============
BG_ParseCSVBuildableList
===============
*/
void BG_ParseCSVBuildableList( const char *string, buildable_t *buildables, int buildablesSize )
{
  char      buffer[ MAX_STRING_CHARS ];
  int       i = 0;
  char      *p, *q;
  qboolean  EOS = qfalse;

  Q_strncpyz( buffer, string, MAX_STRING_CHARS );

  p = q = buffer;

  while( *p != '\0' && i < buildablesSize - 1 )
  {
    //skip to first , or EOS
    while( *p != ',' && *p != '\0' )
      p++;

    if( *p == '\0' )
      EOS = qtrue;

    *p = '\0';

    //strip leading whitespace
    while( *q == ' ' )
      q++;

    buildables[ i ] = BG_BuildableByName( q )->number;

    if( buildables[ i ] == BA_NONE )
      Com_Printf( S_COLOR_YELLOW "WARNING: unknown buildable %s\n", q );
    else
      i++;

    if( !EOS )
    {
      p++;
      q = p;
    }
    else
      break;
  }

  buildables[ i ] = BA_NONE;
}

typedef struct gameElements_s
{
  buildable_t       buildables[ BA_NUM_BUILDABLES ];
  class_t           classes[ PCL_NUM_CLASSES ];
  weapon_t          weapons[ WP_NUM_WEAPONS ];
  upgrade_t         upgrades[ UP_NUM_UPGRADES ];
} gameElements_t;

static gameElements_t bg_disabledGameElements;

/*
============
BG_InitAllowedGameElements
============
*/
void BG_InitAllowedGameElements( void )
{
  char cvar[ MAX_CVAR_VALUE_STRING ];

  Cvar_VariableStringBuffer( "g_disabledEquipment",
      cvar, MAX_CVAR_VALUE_STRING );

  BG_ParseCSVEquipmentList( cvar,
      bg_disabledGameElements.weapons, WP_NUM_WEAPONS,
      bg_disabledGameElements.upgrades, UP_NUM_UPGRADES );

  Cvar_VariableStringBuffer( "g_disabledClasses",
      cvar, MAX_CVAR_VALUE_STRING );

  BG_ParseCSVClassList( cvar,
      bg_disabledGameElements.classes, PCL_NUM_CLASSES );

  Cvar_VariableStringBuffer( "g_disabledBuildables",
      cvar, MAX_CVAR_VALUE_STRING );

  BG_ParseCSVBuildableList( cvar,
      bg_disabledGameElements.buildables, BA_NUM_BUILDABLES );
}

/*
============
BG_WeaponIsAllowed
============
*/
qboolean BG_WeaponIsAllowed( weapon_t weapon, qboolean devMode )
{
  int i;

  if( devMode )
    return qtrue;

  for( i = 0; i < WP_NUM_WEAPONS &&
      bg_disabledGameElements.weapons[ i ] != WP_NONE; i++ )
  {
    if( bg_disabledGameElements.weapons[ i ] == weapon )
      return qfalse;
  }

  return BG_Weapon( weapon )->enabled;
}

/*
============
BG_UpgradeIsAllowed
============
*/
qboolean BG_UpgradeIsAllowed( upgrade_t upgrade, qboolean devMode )
{
  int i;

  if( devMode )
    return qtrue;

  for( i = 0; i < UP_NUM_UPGRADES &&
      bg_disabledGameElements.upgrades[ i ] != UP_NONE; i++ )
  {
    if( bg_disabledGameElements.upgrades[ i ] == upgrade )
      return qfalse;
  }

  return BG_Upgrade( upgrade )->enabled;
}

/*
============
BG_ClassIsAllowed
============
*/
qboolean BG_ClassIsAllowed( class_t class, qboolean devMode )
{
  int i;

  if( devMode )
    return qtrue;

  for( i = 0; i < PCL_NUM_CLASSES &&
      bg_disabledGameElements.classes[ i ] != PCL_NONE; i++ )
  {
    if( bg_disabledGameElements.classes[ i ] == class )
      return qfalse;
  }

  return BG_Class( class )->enabled;
}

/*
============
BG_GetPainState
============
*/
pain_t BG_GetPainState( playerState_t *ps )
{
  int maxHealth = BG_Class( ps->stats[ STAT_CLASS ] )->health;
  int health = ps->misc[ MISC_HEALTH ];

  if( health < ( maxHealth * 3 ) / 32 )
    return PAIN_25;
  else if( health < ( maxHealth * 3 ) / 8 )
    return PAIN_50;
  else if( health < ( maxHealth * 3 ) / 4 )
    return PAIN_75;
  else
    return PAIN_100;
}

/*
============
BG_BuildableIsAllowed
============
*/
qboolean BG_BuildableIsAllowed( buildable_t buildable, qboolean devMode )
{
  int i;

  if( devMode )
    return qtrue;

  for( i = 0; i < BA_NUM_BUILDABLES &&
      bg_disabledGameElements.buildables[ i ] != BA_NONE; i++ )
  {
    if( bg_disabledGameElements.buildables[ i ] == buildable )
      return qfalse;
  }

  return BG_Buildable( buildable )->enabled;
}

/*
============
BG_LoadEmoticons
============
*/
int BG_LoadEmoticons( emoticon_t *emoticons, int num )
{
  int numFiles;
  char fileList[ MAX_EMOTICONS * ( MAX_EMOTICON_NAME_LEN + 9 ) ] = {""};
  int i;
  char *filePtr;
  int fileLen;
  int count;

  numFiles = FS_GetFileList( "emoticons", "x1.tga", fileList,
    sizeof( fileList ) );

  if( numFiles < 1 )
    return 0;

  filePtr = fileList;
  fileLen = 0;
  count = 0;
  for( i = 0; i < numFiles && count < num; i++, filePtr += fileLen + 1 )
  {
    fileLen = strlen( filePtr );
    if( fileLen < 9 || filePtr[ fileLen - 8 ] != '_' ||
        filePtr[ fileLen - 7 ] < '1' || filePtr[ fileLen - 7 ] > '9' )
    {
      Com_Printf( S_COLOR_YELLOW "skipping invalidly named emoticon \"%s\"\n",
        filePtr );
      continue;
    }
    if( fileLen - 8 > MAX_EMOTICON_NAME_LEN )
    {
      Com_Printf( S_COLOR_YELLOW "emoticon file name \"%s\" too long (>%d)\n",
        filePtr, MAX_EMOTICON_NAME_LEN + 8 );
      continue;
    }
    if( !FS_FOpenFileByMode( va( "emoticons/%s", filePtr ), NULL, FS_READ ) )
    {
      Com_Printf( S_COLOR_YELLOW "could not open \"emoticons/%s\"\n", filePtr );
      continue;
    }

    Q_strncpyz( emoticons[ count ].name, filePtr, fileLen - 8 + 1 );
#ifndef GAME
    emoticons[ count ].width = filePtr[ fileLen - 7 ] - '0';
#endif
    count++;
  }

  Com_Printf( "Loaded %d of %d emoticons (MAX_EMOTICONS is %d)\n",
    count, numFiles, MAX_EMOTICONS );
  return count;
}

/*
============
BG_Team
============
*/
const teamAttributes_t *BG_Team( team_t team )
{
  return ( team < NUM_TEAMS ) ?
    &bg_teamList[ team ] : &nullTeam;
}

/*
============
BG_Rank
============
*/
const rankAttributes_t *BG_Rank( rank_t rank )
{
  return ( rank < NUM_OF_RANKS ) ?
    &bg_rankList[ rank ] : &nullRank;
}

/*
================
BG_CreateRotationMatrix
================
*/
void BG_CreateRotationMatrix(vec3_t angles, vec3_t matrix[3]) {
  AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
  VectorInverse(matrix[1]);
}

/*
================
BG_TransposeMatrix
================
*/
void BG_TransposeMatrix(vec3_t matrix[ 3 ], vec3_t transpose[ 3 ]) {
  int i, j;

  for(i = 0; i < 3; i++) {
    for(j = 0; j < 3; j++) {
      transpose[i][j] = matrix[j][i];
    }
  }
}

/*
================
BG_RotatePoint
================
*/
void BG_RotatePoint(vec3_t point, vec3_t matrix[3]) {
  vec3_t tvec;

  VectorCopy( point, tvec );
  point[0] = DotProduct(matrix[0], tvec);
  point[1] = DotProduct(matrix[1], tvec);
  point[2] = DotProduct(matrix[2], tvec);
}

/*
============
BG_EvaluateBBOXPoint
============
*/
void BG_EvaluateBBOXPoint( bboxPoint_t *bboxPoint, vec3_t origin,
                      const vec3_t minsIn, const vec3_t maxsIn )
{
  vec3_t      mins, maxs;
  const float inset = 1.0f;
  int         i;

  Com_Assert( bboxPoint );
  Com_Assert( origin );

  VectorCopy( origin, bboxPoint->point );

  // return the origin if either the mins or maxs is NULL
  if( !minsIn || !maxsIn )
    return;

  Com_Assert( minsIn[ 0 ] <= maxsIn[ 0 ] );
  Com_Assert( minsIn[ 1 ] <= maxsIn[ 1 ] );
  Com_Assert( minsIn[ 2 ] <= maxsIn[ 2 ] );

  VectorCopy( minsIn, mins );
  VectorCopy( maxsIn, maxs );

  //inset the points into the bbox
  for( i = 0; i < 3; i++ )
  {
    //ensure that the mins and maxs don't pass each other
    if( ( mins[ i ] + inset ) < ( maxs[ i ] - inset ) )
    {
      maxs[ i ] -= inset;
      mins[ i ] += inset;
    } else
    {
      //set the min and max values for the given axis equal to their mid point
      maxs[ i ] = mins[ i ] = ( ( maxs[ i ] + mins[ i ] ) / 2 );
    }
  }

  switch ( bboxPoint->num )
  {
    case BBXP_ORIGIN:
      //return the origin
      break;

    case BBXP_MIDFACE1:
      bboxPoint->point[2] += maxs[2];
      bboxPoint->point[1] += ( mins[1] + maxs[1] ) / 2;
      bboxPoint->point[0] += ( mins[0] + maxs[0] ) / 2;
      break;

    case BBXP_MIDFACE2:
      bboxPoint->point[2] += ( mins[2] + maxs[2] ) / 2;
      bboxPoint->point[1] += maxs[1];
      bboxPoint->point[0] += ( mins[0] + maxs[0] ) / 2;
      break;

    case BBXP_MIDFACE3:
      bboxPoint->point[2] += ( mins[2] + maxs[2] ) / 2;
      bboxPoint->point[1] += mins[1];
      bboxPoint->point[0] += ( mins[0] + maxs[0] ) / 2;
      break;

    case BBXP_MIDFACE4:
      bboxPoint->point[2] += ( mins[2] + maxs[2] ) / 2;
      bboxPoint->point[1] += ( mins[1] + maxs[1] ) / 2;
      bboxPoint->point[0] += maxs[0];
      break;

    case BBXP_MIDFACE5:
      bboxPoint->point[2] += ( mins[2] + maxs[2] ) / 2;
      bboxPoint->point[1] += ( mins[1] + maxs[1] ) / 2;
      bboxPoint->point[0] += mins[0];
      break;

    case BBXP_MIDFACE6:
      bboxPoint->point[2] += mins[2];
      bboxPoint->point[1] += ( mins[1] + maxs[1] ) / 2;
      bboxPoint->point[0] += ( mins[0] + maxs[0] ) / 2;
      break;

    case BBXP_VERTEX1:
      bboxPoint->point[2] += maxs[2];
      bboxPoint->point[1] += maxs[1];
      bboxPoint->point[0] += maxs[0];
      break;

    case BBXP_VERTEX2:
      bboxPoint->point[2] += maxs[2];
      bboxPoint->point[1] += mins[1];
      bboxPoint->point[0] += maxs[0];
      break;

    case BBXP_VERTEX3:
      bboxPoint->point[2] += maxs[2];
      bboxPoint->point[1] += maxs[1];
      bboxPoint->point[0] += mins[0];
      break;

    case BBXP_VERTEX4:
      bboxPoint->point[2] += maxs[2];
      bboxPoint->point[1] += mins[1];
      bboxPoint->point[0] += mins[0];
      break;

    case BBXP_VERTEX5:
      bboxPoint->point[2] += mins[2];
      bboxPoint->point[1] += maxs[1];
      bboxPoint->point[0] += maxs[0];
      break;

    case BBXP_VERTEX6:
      bboxPoint->point[2] += mins[2];
      bboxPoint->point[1] += mins[1];
      bboxPoint->point[0] += maxs[0];
      break;

    case BBXP_VERTEX7:
      bboxPoint->point[2] += mins[2];
      bboxPoint->point[1] += maxs[1];
      bboxPoint->point[0] += mins[0];
      break;

    case BBXP_VERTEX8:
      bboxPoint->point[2] += mins[2];
      bboxPoint->point[1] += mins[1];
      bboxPoint->point[0] += mins[0];
      break;

    default:
      Com_Error( ERR_DROP,
                 "BG_EvaluateBBOXPoint: unknown bboxPointNum: %i",
                 bboxPoint->num );
      break;
  }
}

/*
============
BG_PointIsInsideBBOX
============
*/
qboolean BG_PointIsInsideBBOX(
  const vec3_t point, const vec3_t mins, const vec3_t maxs) {
  Com_Assert(point);
  Com_Assert(mins);
  Com_Assert(maxs);

  if(
    point[0] < mins[0] ||
    point[1] < mins[1] ||
    point[2] < mins[2] ||
    point[0] > maxs[0] ||
    point[1] > maxs[1] ||
    point[2] > maxs[2]) {
    return qfalse;
  }

  return qtrue;
}

/*
============
BG_FindBBoxCenter
============
*/
void BG_FindBBoxCenter(
  const vec3_t origin, const vec3_t mins, const vec3_t maxs, vec3_t center) {
  vec3_t absmin;
  vec3_t absmax;
  int    i;

  VectorAdd(origin, mins, absmin);
  VectorAdd(origin, maxs, absmax);

  for(i = 0; i < 3; i++) {
    center[i] = (absmin[i] + absmax[i]) / 2;
  }
}

int cmdcmp( const void *a, const void *b )
{
  return Q_stricmp( (const char *)a, ((dummyCmd_t *)b)->name );
}

/*
============
BG_Scrim_Team_From_Playing_Team
============
*/
scrim_team_t BG_Scrim_Team_From_Playing_Team(scrim_t *scrim, team_t playing_team) {
  scrim_team_t scrim_team;

  if(playing_team == TEAM_NONE || playing_team >= NUM_TEAMS || playing_team < 0) {
    return SCRIM_TEAM_NONE;
  }

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(scrim_team == SCRIM_TEAM_NONE) {
      continue;
    }

    if(scrim->team[scrim_team].current_team == playing_team) {
      return scrim_team;
    }
  }

  return SCRIM_TEAM_NONE;
}
