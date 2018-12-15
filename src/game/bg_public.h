/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2018 GrangerHub

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

// bg_public.h -- definitions shared by both the server game and client game modules

// tremulous balance header
#include "tremulous.h"

// linked lists
#include "bg_list.h"

// queues
#include "bg_queue.h"

// BGAME Dynamic Memory Allocation
#include "bg_alloc.h"

// because games can change separately from the main system version, we need a
// second version that must match between game and cgame
#define GAME_VERSION            "base"

int BG_HP2SU( int healthPoints );
int BG_SU2HP( int healthSubUnits );

#define DEFAULT_GRAVITY         800
#define GIB_HEALTH              ( BG_HP2SU( -125 ) )

#define VOTE_TIME               30000 // 30 seconds before vote times out

#define MAX_TARGETS             4 // warning: if you change these, then also change
#define MAX_TARGETNAMES         4 //          g_spawn.c to actually spawn extras

#define DEFAULT_VIEWHEIGHT      26
#define CROUCH_VIEWHEIGHT       12
#define DEAD_VIEWHEIGHT         4 // height from ground

// player teams
typedef enum
{
  TEAM_NONE,
  TEAM_ALIENS,
  TEAM_HUMANS,

  NUM_TEAMS
} team_t;

// human portals
typedef enum
{
  PORTAL_BLUE,
  PORTAL_RED,
  PORTAL_NUM
} portal_t;

/*
--------------------------------------------------------------------------------
Flags for map restarts
*/
#define  RESTART_WARMUP_RESET 0x00000001 // Warmup is resetting from a "win
                                         // condition" but isn't ending.

#define  RESTART_WARMUP_END   0x00000002 // Warmup is ending and the match is
                                         // beginning.
/*
--------------------------------------------------------------------------------
*/

//
// config strings are a general means of communicating variable length strings
// from the server to all connected clients.
//

// CS_SERVERINFO and CS_SYSTEMINFO are defined in q_shared.h
enum
{
  CS_MUSIC            = 2,
  CS_MESSAGE,               // from the map worldspawn's message field
  CS_MOTD,                  // g_motd string for server message of the day
  CS_COUNTDOWN,                // server time when the match will be restarted

  CS_VOTE_TIME,             // Vote stuff each needs NUM_TEAMS slots
  CS_VOTE_STRING      = CS_VOTE_TIME + NUM_TEAMS,
  CS_VOTE_CAST         = CS_VOTE_STRING + NUM_TEAMS,
  CS_VOTE_ACTIVE          = CS_VOTE_CAST + NUM_TEAMS,
  CS_VOTE_CALLER      = CS_VOTE_ACTIVE + NUM_TEAMS,

  CS_GAME_VERSION     = CS_VOTE_CALLER + NUM_TEAMS,
  CS_LEVEL_START_TIME,      // so the timer only shows the current level
  CS_INTERMISSION,          // when 1, fraglimit/timelimit has been hit and intermission will start in a second or two
  CS_WINNER ,               // string indicating round winner
  CS_SHADERSTATE,
  CS_BOTINFO,
  CS_CLIENTS_READY,

  CS_ALIEN_STAGES,
  CS_HUMAN_STAGES,

  CS_WARMUP,                // g_warmup
  CS_WARMUP_READY,

  CS_DEVMODE,

  CS_HUMAN_STAMINA_MODE,

  CS_HUMAN_PORTAL_CREATETIME,

  CS_MODELS           = CS_HUMAN_PORTAL_CREATETIME + PORTAL_NUM,
  CS_SOUNDS           = CS_MODELS + MAX_MODELS,
  CS_SHADERS          = CS_SOUNDS + MAX_SOUNDS,
  CS_PARTICLE_SYSTEMS = CS_SHADERS + MAX_GAME_SHADERS,
  CS_PLAYERS          = CS_PARTICLE_SYSTEMS + MAX_GAME_PARTICLE_SYSTEMS,
  CS_LOCATIONS        = CS_PLAYERS + MAX_CLIENTS,

  CS_MAX              = CS_LOCATIONS + MAX_LOCATIONS
};

#if CS_MAX > MAX_CONFIGSTRINGS
#error overflow: CS_MAX > MAX_CONFIGSTRINGS
#endif

#define MAX_INTERMISSION_SOUND_SETS 1
#define MAX_WARMUP_SOUNDS 2

typedef enum
{
  MATCHOUTCOME_WON,
  MATCHOUTCOME_EVAC,
  MATCHOUTCOME_TIME
} matchOutcomes_t;

typedef enum
{
  GENDER_MALE,
  GENDER_FEMALE,
  GENDER_NEUTER
} gender_t;

/*
===================================================================================

PMOVE MODULE

The pmove code takes a player_state_t and a usercmd_t and generates a new player_state_t
and some other output data.  Used for local prediction on the client game and true
movement on the server game.
===================================================================================
*/

typedef enum
{
  PM_NORMAL,        // can accelerate and turn
  PM_NOCLIP,        // noclip movement
  PM_SPECTATOR,     // still run into walls
  PM_JETPACK,       // jetpack physics
  PM_GRABBED,       // like dead, but for when the player is still alive
  PM_DEAD,          // no acceleration or turning, but free falling
  PM_FREEZE,        // stuck in place with no control
  PM_INTERMISSION   // no movement or status bar
} pmtype_t;

// pmtype_t categories
#define PM_Paralyzed( x ) ( (x) == PM_DEAD || (x) == PM_FREEZE ||\
                            (x) == PM_INTERMISSION )
#define PM_Alive( x )     ( (x) == PM_NORMAL || (x) == PM_JETPACK ||\
                            (x) == PM_GRABBED )

typedef enum
{
  WEAPON_READY,
  WEAPON_RAISING,
  WEAPON_DROPPING,
  WEAPON_FIRING,
  WEAPON_RELOADING,
} weaponstate_t;

// pmove->pm_flags
#define PMF_DUCKED          0x000001
#define PMF_JUMP_HELD       0x000002
#define PMF_CROUCH_HELD     0x000004
#define PMF_BACKWARDS_JUMP  0x000008 // go into backwards land
#define PMF_BACKWARDS_RUN   0x000010 // coast down to backwards run
#define PMF_TIME_LAND       0x000020 // pm_time is time before rejump
#define PMF_TIME_KNOCKBACK  0x000040 // pm_time is an air-accelerate only time
#define PMF_TIME_WATERJUMP  0x000080 // pm_time is waterjump
#define PMF_RESPAWNED       0x000100 // clear after attack and jump buttons come up
#define PMF_USE_ITEM_HELD   0x000200
#define PMF_WEAPON_RELOAD   0x000400 // force a weapon switch
#define PMF_FOLLOW          0x000800 // spectate following another player
#define PMF_QUEUED          0x001000 // player is queued
#define PMF_TIME_WALLJUMP   0x002000 // for limiting wall jumping
#define PMF_CHARGE          0x004000 // keep track of pouncing
#define PMF_WEAPON_SWITCH   0x008000 // force a weapon switch
#define PMF_SPRINTHELD      0x010000
#define PMF_HOPPED          0x020000 // a hop has occurred
#define PMF_BUNNY_HOPPING   0x040000 // bunny hopping is enabled
#define PMF_JUMPING         0x080000 // a jump has occurred but has not landed yet
#define PMF_PAUSE_BEAM      0x100000 //for special cases of when continous beam wepons are not being fired


#define PMF_ALL_TIMES (PMF_TIME_WATERJUMP|PMF_TIME_LAND|PMF_TIME_KNOCKBACK|PMF_TIME_WALLJUMP)

#define PMF_ALL_HOP_FLAGS (PMF_HOPPED|PMF_BUNNY_HOPPING|PMF_JUMPING)

typedef struct
{
  trace_t  impactTriggerTrace; // Used for the lightning gun
  int      pulsatingBeamTime[ 3 ];
  qboolean impactTriggerTraceChecked;
  int      burstRoundsToFire[ 3 ];
  int      pouncePayload;
  int      repairRepeatDelay;      // Used for for the construction kit
  float    fallVelocity;
  int      updateAnglesTime;
  float    diffAnglesPeriod;
  vec3_t   previousFrameAngles;
  vec3_t   previousUpdateAngles;
  vec3_t   angularVelocity;
} pmoveExt_t;

#define MAXTOUCH  32
typedef struct pmove_s pmove_t;
struct pmove_s
{
  // state (in / out)
  playerState_t *ps;
  pmoveExt_t *pmext;
  // command (in)
  usercmd_t     cmd;
  int           tracemask;      // collide against these types of surfaces
  int           debugLevel;     // if set, diagnostic output will be printed
  qboolean      noFootsteps;    // if the game is setup for no footsteps by the server
  qboolean      autoWeaponHit[ 32 ];

  int           framecount;

  // results (out)
  qboolean      touchents[ MAX_GENTITIES ];
  void (*ClientImpacts)( pmove_t *pm, trace_t *trace,
                         const vec3_t impactVelocity );

  vec3_t        mins, maxs;     // bounding box size

  int           watertype;
  int           waterlevel;

  float         xyspeed;

  // for fixed msec Pmove
  int           pmove_fixed;
  int           pmove_msec;

  int           humanStaminaMode; // when set to 0, human stamina doesn't drain

  // callbacks to test the world
  // these will be different functions during game and cgame
  /*void    (*trace)( trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask );*/
  void          (*trace)( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs,
                          const vec3_t end, int passEntityNum, int contentMask );


  int           (*pointcontents)( const vec3_t point, int passEntityNum );

  void          (*unlagged_on)( int attackerNum, vec3_t muzzle, float range );
  void          (*unlagged_off)( void );

  int           tauntSpam; // allow taunts to be spammed. only for clients that enable cg_tauntSpam
  int           humanPortalCreateTime[ PORTAL_NUM ];

  int           swapAttacks;
  float         wallJumperMinFactor;
  float         marauderMinJumpFactor;
};

// if a full pmove isn't done on the client, you can just update the angles
void PM_UpdateViewAngles( playerState_t *ps, const usercmd_t *cmd );
void PM_CalculateAngularVelocity( playerState_t *ps, const usercmd_t *cmd );
void Pmove( pmove_t *pmove );

//===================================================================================


// player_state->stats[] indexes
typedef enum
{
  STAT_ITEMS,
  STAT_ACTIVEITEMS,
  STAT_WEAPON,    // current primary weapon
  STAT_CLASS,     // player class (for aliens AND humans)
  STAT_TEAM,      // player team
  
  // Additional flags only used by clients.  Copied over to otherEntityNum2 in
  // the entity state, but keep in mind that otherEntityNum2 can only hold
  // GENTITYNUM_BITS number of bits (current default is 10).
  STAT_FLAGS,
  
  STAT_STAMINA,   // stamina for SCA_STAMINA or SCA_CHARGE_STAMINA
  STAT_STATE,     // client states e.g. wall climbing
  STAT_MISC2,     // for more misc stuff, copied into constantLight of the entity state.
  STAT_MISC3,     // for even more misc stuff.
  STAT_BUILDABLE, // which ghost model to display for building
  STAT_FALLDIST,  // the distance the player fell
  STAT_VIEWLOCK,   // direction to lock the view in
  STAT_SHAKE      // camera shake
  // netcode has space for 2 more
} statIndex_t;

#define SCA_WALLCLIMBER         0x00000001
#define SCA_TAKESFALLDAMAGE     0x00000002
#define SCA_CANZOOM             0x00000004
#define SCA_FOVWARPS            0x00000008
#define SCA_ALIENSENSE          0x00000010
#define SCA_CANUSELADDERS       0x00000020
#define SCA_WALLJUMPER          0x00000040
#define SCA_STAMINA             0x00000080
#define SCA_REGEN               0x00000100 // XXX kinda wasted- keeps alien class on human team from never dieing when team admits defeat.
#define SCA_CANHOVEL            0x00000200
#define SCA_CHARGE_STAMINA      0x00000400 // limits MISC_MISC use. can't be used with SCA_STAMINA

#define SS_WALLCLIMBING         0x00000001
#define SS_CREEPSLOWED          0x00000002
#define SS_SPEEDBOOST           0x00000004
#define SS_GRABBED              0x00000008
#define SS_BLOBLOCKED           0x00000010
#define SS_POISONED             0x00000020
#define SS_BOOSTED              0x00000040
#define SS_BOOSTEDWARNING       0x00000080 // booster poison is running out
#define SS_SLOWLOCKED           0x00000100
#define SS_CHARGING             0x00000200
#define SS_HEALING_ACTIVE       0x00000400 // medistat for humans, creep for aliens
#define SS_HEALING_2X           0x00000800 // medkit or double healing rate
#define SS_HEALING_3X           0x00001000 // triple healing rate

#define SB_VALID_TOGGLEBIT      0x00002000

#define SFL_READY               0x00000001 // player ready state
#define SFL_REFRESH_MISC        0x00000002 // hax to ensure the misc changes are broadcasted
#define SFL_GIBBED              0x00000004
#define SFL_CLASS_FORCED        0x00000008 // can't evolve from a class that a map forced

// player_state->persistant[] indexes
// these fields are the only part of player_state that isn't
// cleared on respawn
typedef enum
{
  PERS_SCORE,           // !!! MUST NOT CHANGE, SERVER AND GAME BOTH REFERENCE !!!
  PERS_HITS,            // total points damage inflicted so damage beeps can sound on change
  PERS_SPAWNS,          // how many spawns your team has
  PERS_SPECSTATE,
  PERS_SPAWN_COUNT,     // incremented every respawn
  PERS_ATTACKER,        // clientnum of last damage inflicter
  PERS_KILLED,          // count of the number of times you died

  PERS_STATE,
  PERS_CREDIT,    // human credit
  PERS_QUEUEPOS,  // position in the spawn queue
  PERS_NEWWEAPON,  // weapon to switch to
  PERS_BP,
  PERS_MARKEDBP,
  PERS_ACT_ENT,  // indicates the entity number of an entity a client can activate
  PERS_JUMPTIME // elapsed time since the previous jump
  // netcode has space for 1 more
} persEnum_t;

// hack to fit extra data in the misc[] array
#define MISC_HEALTH                    ( MAX_MISC - 1 )
#define MISC_MAX_HEALTH                ( MAX_MISC - 2 ) // for max health decay
#define MISC_ARMOR                     ( MAX_MISC - 3 )
//Do not use MISC_SEED outside of PM_PSRandom(),
//and don't use PM_PSRandom() outside of bg_pmove.c.
#define MISC_SEED                      ( MAX_MISC - 4 ) // for predicted psudorandom things

// for uh...misc stuff (pounce, trample, lcannon)
#define MISC_MISC                      ( MAX_MISC - 5 )
// for uh...other misc stuff (evolve cool down)
#define MISC_MISC2                     ( MAX_MISC - 6 )

// for gradually applying recoil
#define MISC_RECOIL_PITCH              ( MAX_MISC - 7 )
#define MISC_RECOIL_YAW                ( MAX_MISC - 8 )

#define MISC_LAST_DAMAGE_TIME          ( MAX_MISC - 9 )

//for storing more than just 32 different weapon numbers
#define MISC_HELD_WEAPON               ( MAX_MISC - 10 )

#define PS_WALLCLIMBINGFOLLOW   0x00000001
#define PS_WALLCLIMBINGTOGGLE   0x00000002
#define PS_NONSEGMODEL          0x00000004
#define PS_SPRINTTOGGLE         0x00000008
#define PS_SPRINTTOGGLEONSTOP   0x00000010

// entityState_t->eFlags
// notice that some flags are overlapped, so their meaning depends on context
#define EF_DEAD             0x00001    // don't draw a foe marker over players with EF_DEAD
#define EF_TELEPORT_BIT     0x00002    // toggled every time the origin abruptly changes
#define EF_PLAYER_EVENT     0x00004    // only used for eType > ET_EVENTS

// for occupation entities
#define EF_OCCUPYING        0x00010    // can result in bugs if applied to buildables

// buildable flags:
#define EF_B_SPAWNED        0x00008
#define EF_B_POWERED        0x00010
#define EF_B_MARKED         0x00020

#define EF_WARN_CHARGE      0x00020    // Lucifer Cannon is about to overcharge
#define EF_WALLCLIMB        0x00040    // wall walking
#define EF_WALLCLIMBCEILING 0x00080    // wall walking ceiling hack
#define EF_NODRAW           0x00100    // may have an event, but no model (unspawned items)
#define EF_MOVER_STOP       0x00200    // will push otherwise, used by ET_MOVER
#define EF_INVINCIBLE       0x00200    // used to indicate if a player can't be damaged
#define EF_ASTRAL_NOCLIP    0x00400    // EF_ASTRAL flagged entities don't clip with Astral entities,
                                       // must be equal to CONTENTS_ASTRAL_NOCLIP
#define EF_FIRING           0x00800    // for lightning gun
#define EF_FIRING2          0x01000    // alt fire
#define EF_FIRING3          0x02000    // third fire
#define EF_POISONCLOUDED    0x04000    // player hit with basilisk gas
#define EF_CONNECTION       0x08000    // draw a connection trouble sprite
#define EF_BLOBLOCKED       0x10000    // caught by a trapper


/*
--------------------------------------------------------------------------------
activation.flags
*/
#define  ACTF_TEAM                 0x0001 // A client must be on the same team
                                          // as this entity to activate it.

#define  ACTF_ENT_ALIVE            0x0002 // This entity's health must be
                                          // greater than 0 to be activated by a
                                          // client.

#define  ACTF_PL_ALIVE             0x0004 // A player must be alive to activate
                                          // this entity.

#define  ACTF_SPAWNED              0x0008 // This entity must be spawned to be
                                          // activated by a client.

#define  ACTF_POWERED              0x0010 // This entity must be powered to be
                                          // activated by a client.

#define  ACTF_GROUND               0x0020 // A client must stand on this entity
                                          // to activate it standing on this
                                          // entity gives that entity higher
                                          // preference over other nearby
                                          // activation entities.

#define  ACTF_LINE_OF_SIGHT        0x0040 // Clients must be able to have a
                                          // MASK_DEADSOLID trace to this
                                          // activation entity to activate it.

#define  ACTF_OCCUPY               0x0080 // When clients activate this
                                          // activation entity, they occupy the
                                          // entity and can't activate any other
                                          // entity while occupying.  Nor can
                                          // another player not occupying a given
                                          // occupiable activation entity
                                          // activate that entity while it is
                                          // occupied.
/*
--------------------------------------------------------------------------------
*/

/*
--------------------------------------------------------------------------------
occupation.flags
*/
#define  OCCUPYF_ACTIVATE       0x0100 // Keep activating an activation
                                       // entity while the client occupies
                                       // it.

#define  OCCUPYF_UNTIL_INACTIVE 0x0200 // Unoccupy an occupant that is no
                                       // longer activating this entity.
                                       // Requires OCCUPYF_ACTIVATE.

#define  OCCUPYF_PM_TYPE        0x0400 // change the pm_type of an occupant
                                       // to occupation.pm_type.

#define  OCCUPYF_CONTENTS       0x0800 // Change the contents of an occupant
                                       // to occupation.contents.

#define  OCCUPYF_CLIPMASK       0x1000 // Change the clip mask of an occupant
                                       // to occupation.clipMask.

#define  OCCUPYF_RESET_OTHER    0x2000 // When an occupied activation entity
                                       // is reset, also reset
                                       // occupation.other.
/*
--------------------------------------------------------------------------------
*/


typedef enum
{
  HI_NONE,

  HI_TELEPORTER,
  HI_MEDKIT,

  HI_NUM_HOLDABLE
} holdable_t;

typedef enum
{
  WPM_NONE,

  WPM_PRIMARY,
  WPM_SECONDARY,
  WPM_TERTIARY,

  WPM_NOTFIRING,

  WPM_NUM_WEAPONMODES
} weaponMode_t;

typedef enum
{
  WP_NONE,

  WP_ALEVEL0,
  WP_ALEVEL1,
  WP_ALEVEL1_UPG,
  WP_ALEVEL2,
  WP_ALEVEL2_UPG,
  WP_ALEVEL3,
  WP_ALEVEL3_UPG,
  WP_ALEVEL4,

  WP_BLASTER,
  WP_MACHINEGUN,
  WP_PAIN_SAW,
  WP_SHOTGUN,
  WP_LAS_GUN,
  WP_MASS_DRIVER,
  WP_CHAINGUN,
  WP_FLAMER,
  WP_PULSE_RIFLE,
  WP_LUCIFER_CANNON,
  WP_GRENADE,
  WP_FRAGNADE,
  WP_LAUNCHER,
  WP_LIGHTNING,

  WP_LOCKBLOB_LAUNCHER,
  WP_HIVE,
  WP_TESLAGEN,
  WP_MGTURRET,

  //build weapons must remain in a block
  WP_ABUILD,
  WP_ABUILD2,
  WP_HBUILD,
  WP_PORTAL_GUN,
  //ok?

  WP_NUM_WEAPONS
} weapon_t;

typedef enum
{
  UP_NONE,

  UP_LIGHTARMOUR,
  UP_HELMET,
  UP_MEDKIT,
  UP_BATTPACK,
  UP_JETPACK,
  UP_BATTLESUIT,
  UP_GRENADE,
  UP_FRAGNADE,

  UP_AMMO,

  UP_NUM_UPGRADES
} upgrade_t;

// bitmasks for upgrade slots
#define SLOT_NONE       0x00000000
#define SLOT_HEAD       0x00000001
#define SLOT_TORSO      0x00000002
#define SLOT_ARMS       0x00000004
#define SLOT_LEGS       0x00000008
#define SLOT_BACKPACK   0x00000010
#define SLOT_WEAPON     0x00000020
#define SLOT_SIDEARM    0x00000040

typedef enum
{
  BA_NONE,

  BA_A_SPAWN,
  BA_A_OVERMIND,

  BA_A_BARRICADE,
  BA_A_ACIDTUBE,
  BA_A_TRAPPER,
  BA_A_BOOSTER,
  BA_A_HIVE,

  BA_H_SPAWN,

  BA_H_MGTURRET,
  BA_H_TESLAGEN,

  BA_H_ARMOURY,
  BA_H_DCC,
  BA_H_MEDISTAT,

  BA_H_REACTOR,
  BA_H_REPEATER,

  BA_NUM_BUILDABLES
} buildable_t;

//entity roles
#define ROLE_SPAWN        0x00000001 // clients spawn from these
#define ROLE_CORE         0x00000002 // plays a central role
#define ROLE_POWER_SOURCE 0x00000004 // provides a power/creep source
#define ROLE_PERVASIVE    0x00000008 // its principal effects pass through walls
#define ROLE_SUPPORT      0x00000010 // provides support
#define ROLE_OFFENSE      0x00000020 // actively attacks enemies
#define ROLE_STRUCTUAL    0x00000040 // for structual support
#define ROLE_TRANSPORT    0x00000080 // Used for remote transportation

typedef enum
{
  RMT_SPHERE,
  RMT_SPHERICAL_CONE_64,
  RMT_SPHERICAL_CONE_240
} rangeMarkerType_t;

typedef enum
{
  SHC_GREY,
  SHC_DARK_BLUE,
  SHC_LIGHT_BLUE,
  SHC_GREEN_CYAN,
  SHC_VIOLET,
  SHC_YELLOW,
  SHC_ORANGE,
  SHC_LIGHT_GREEN,
  SHC_DARK_GREEN,
  SHC_RED,
  SHC_PINK,
  SHC_NUM_SHADER_COLORS
} shaderColorEnum_t;

// entityState_t->event values
// entity events are for effects that take place relative
// to an existing entities origin.  Very network efficient.

// two bits at the top of the entityState->event field
// will be incremented with each change in the event so
// that an identical event started twice in a row can
// be distinguished.  And off the value with ~EV_EVENT_BITS
// to retrieve the actual event number
#define EV_EVENT_BIT1   0x00000100
#define EV_EVENT_BIT2   0x00000200
#define EV_EVENT_BITS   (EV_EVENT_BIT1|EV_EVENT_BIT2)

#define EVENT_VALID_MSEC  300

const char *BG_EventName( int num );

typedef enum
{
  EV_NONE,

  EV_FOOTSTEP,
  EV_FOOTSTEP_METAL,
  EV_FOOTSTEP_SQUELCH,
  EV_FOOTSPLASH,
  EV_FOOTWADE,
  EV_SWIM,

  EV_STEP_4,
  EV_STEP_8,
  EV_STEP_12,
  EV_STEP_16,

  EV_STEPDN_4,
  EV_STEPDN_8,
  EV_STEPDN_12,
  EV_STEPDN_16,

  EV_FALL_SHORT,
  EV_FALL_MEDIUM,
  EV_FALL_FAR,
  EV_FALLING,

  EV_JUMP,
  EV_WATER_TOUCH, // foot touches
  EV_WATER_LEAVE, // foot leaves
  EV_WATER_UNDER, // head touches
  EV_WATER_CLEAR, // head leaves

  EV_NOAMMO,
  EV_CHANGE_WEAPON,
  EV_FIRE_WEAPON,
  EV_FIRE_WEAPON2,
  EV_FIRE_WEAPON3,

  EV_PLAYER_RESPAWN, // for fovwarp effects
  EV_PLAYER_TELEPORT_IN,
  EV_PLAYER_TELEPORT_OUT,

  EV_GRENADE_BOUNCE,    // eventParm will be the soundindex

  EV_GENERAL_SOUND,
  EV_GLOBAL_SOUND,    // no attenuation

  EV_BULLET_HIT_FLESH,
  EV_BULLET_HIT_WALL,

  EV_SPLATTER,
  EV_MASS_DRIVER,

  EV_MISSILE_HIT,
  EV_MISSILE_MISS,
  EV_MISSILE_MISS_METAL,
  EV_TESLATRAIL,
  EV_BULLET,        // otherEntity is the shooter

  EV_LEV1_GRAB,
  EV_LEV4_TRAMPLE_PREPARE,
  EV_LEV4_TRAMPLE_START,

  EV_PAIN,
  EV_DEATH1,
  EV_DEATH2,
  EV_DEATH3,
  EV_OBITUARY,

  EV_GIB_PLAYER,

  EV_BUILD_FIRE,
  EV_BUILD_CONSTRUCT,
  EV_BUILD_DESTROY,
  EV_BUILD_DELAY,     // can't build yet
  EV_BUILD_REPAIR,    // repairing buildable
  EV_BUILD_REPAIRED,  // buildable has full health
  EV_HUMAN_BUILDABLE_EXPLOSION,
  EV_ALIEN_BUILDABLE_EXPLOSION,
  EV_ALIEN_ACIDTUBE,

  EV_MEDKIT_USED,

  EV_ALIEN_SPAWN_PROTECTION_ENDED,
  EV_ALIEN_EVOLVE,
  EV_ALIEN_EVOLVE_FAILED,

  EV_DEBUG_LINE,
  EV_STOPLOOPINGSOUND,
  EV_TAUNT,

  EV_OVERMIND_ATTACK, // overmind under attack
  EV_OVERMIND_DYING,  // overmind close to death
  EV_OVERMIND_SPAWNS, // overmind needs spawns

  EV_DCC_ATTACK,      // dcc under attack

  EV_MGTURRET_SPINUP, // turret spinup sound should play

  EV_RPTUSE_SOUND,    // trigger a sound

  EV_FIGHT
} entity_event_t;

typedef enum
{
  MN_NONE,

  MN_TEAM,
  MN_A_TEAMFULL,
  MN_H_TEAMFULL,
  MN_A_TEAMLOCKED,
  MN_H_TEAMLOCKED,
  MN_PLAYERLIMIT,

  // cmd stuff
  MN_CMD_CHEAT,
  MN_CMD_CHEAT_TEAM,
  MN_CMD_TEAM,
  MN_CMD_SPEC,
  MN_CMD_ALIEN,
  MN_CMD_HUMAN,
  MN_CMD_ALIVE,

  //alien stuff
  MN_A_CLASS,
  MN_A_BUILD,
  MN_A_INFEST,
  MN_A_NOEROOM,
  MN_A_TOOCLOSE,
  MN_A_NOOVMND_EVOLVE,
  MN_A_EVOLVEBUILDTIMER,
  MN_A_CANTEVOLVE,
  MN_A_EVOLVEWALLWALK,
  MN_A_UNKNOWNCLASS,
  MN_A_CLASSNOTSPAWN,
  MN_A_CLASSNOTALLOWED,
  MN_A_CLASSNOTATSTAGE,

  //shared build
  MN_B_NOROOM,
  MN_B_NORMAL,
  MN_B_CANNOT,
  MN_B_LASTSPAWN,
  MN_B_SUDDENDEATH,
  MN_B_REVOKED,
  MN_B_SURRENDER,
  MN_B_BLOCKEDBYENEMY,
  MN_B_SD_UNIQUE,
  MN_B_SD_IRREPLACEABLE,
  MN_B_SD_NODECON,

  //alien build
  MN_A_ONEOVERMIND,
  MN_A_NOBP,
  MN_A_NOCREEP,
  MN_A_NOOVMND,

  //human stuff
  MN_H_SPAWN,
  MN_H_BUILD,
  MN_H_ARMOURY,
  MN_H_UNKNOWNITEM,
  MN_H_NOSLOTS,
  MN_H_NOFUNDS,
  MN_H_ITEMHELD,
  MN_H_UNEXPLODEDGRENADE,
  MN_H_NOARMOURYHERE,
  MN_H_NOENERGYAMMOHERE,
  MN_H_NOROOMBSUITON,
  MN_H_NOROOMBSUITOFF,
  MN_H_ARMOURYBUILDTIMER,
  MN_H_DEADTOCLASS,
  MN_H_UNKNOWNSPAWNITEM,

  //human build
  MN_H_NOPOWERHERE,
  MN_H_NOBP,
  MN_H_NOTPOWERED,
  MN_H_NODCC,
  MN_H_ONEREACTOR,
  MN_H_RPTPOWERHERE,

  // activation entity stuff
  MN_ACT_FAILED,
  MN_ACT_OCCUPIED,
  MN_ACT_OCCUPYING,
  MN_ACT_NOOCCUPANTS,
  MN_ACT_NOEXIT,
  MN_ACT_NOTPOWERED,
  MN_ACT_NOTCONTROLLED
} dynMenu_t;

// indicies for overriding activation entity menu messages
typedef enum
{
  ACTMN_ACT_FAILED,          // for general activation failure if a more
                             // specific message isn't set.

  // occupying activation entity stuff
  ACTMN_ACT_OCCUPIED,        // this entity is fully occupied
  ACTMN_ACT_OCCUPYING,       // the target potential occupant is occupying
                             // another entity
  ACTMN_ACT_NOOCCUPANTS,     // there are no target potential occupants
  ACTMN_ACT_NOEXIT,          // the activation entity can't be exited

  // this entitity must be powered to use
  ACTMN_H_NOTPOWERED,        // humans specific
  ACTMN_ACT_NOTCONTROLLED,   // alien specific
  ACTMN_ACT_NOTPOWERED,      // generic

  MAX_ACTMN
} actMNOvrdIndex_t;

// animations
typedef enum
{
  BOTH_DEATH1,
  BOTH_DEAD1,
  BOTH_DEATH2,
  BOTH_DEAD2,
  BOTH_DEATH3,
  BOTH_DEAD3,

  TORSO_GESTURE,

  TORSO_ATTACK,
  TORSO_ATTACK2,

  TORSO_DROP,
  TORSO_RAISE,

  TORSO_STAND,
  TORSO_STAND2,

  LEGS_WALKCR,
  LEGS_WALK,
  LEGS_RUN,
  LEGS_BACK,
  LEGS_SWIM,

  LEGS_JUMP,
  LEGS_LAND,

  LEGS_JUMPB,
  LEGS_LANDB,

  LEGS_IDLE,
  LEGS_IDLECR,

  LEGS_TURN,

  TORSO_GETFLAG,
  TORSO_GUARDBASE,
  TORSO_PATROL,
  TORSO_FOLLOWME,
  TORSO_AFFIRMATIVE,
  TORSO_NEGATIVE,

  MAX_PLAYER_ANIMATIONS,

  LEGS_BACKCR,
  LEGS_BACKWALK,
  FLAG_RUN,
  FLAG_STAND,
  FLAG_STAND2RUN,

  MAX_PLAYER_TOTALANIMATIONS
} playerAnimNumber_t;

// nonsegmented animations
typedef enum
{
  NSPA_STAND,

  NSPA_GESTURE,

  NSPA_WALK,
  NSPA_RUN,
  NSPA_RUNBACK,
  NSPA_CHARGE,

  NSPA_RUNLEFT,
  NSPA_WALKLEFT,
  NSPA_RUNRIGHT,
  NSPA_WALKRIGHT,

  NSPA_SWIM,

  NSPA_JUMP,
  NSPA_LAND,
  NSPA_JUMPBACK,
  NSPA_LANDBACK,

  NSPA_TURN,

  NSPA_ATTACK1,
  NSPA_ATTACK2,
  NSPA_ATTACK3,

  NSPA_PAIN1,
  NSPA_PAIN2,

  NSPA_DEATH1,
  NSPA_DEAD1,
  NSPA_DEATH2,
  NSPA_DEAD2,
  NSPA_DEATH3,
  NSPA_DEAD3,

  MAX_NONSEG_PLAYER_ANIMATIONS,

  NSPA_WALKBACK,

  MAX_NONSEG_PLAYER_TOTALANIMATIONS
} nonSegPlayerAnimNumber_t;

// for buildable animations
typedef enum
{
  BANIM_NONE,

  BANIM_CONSTRUCT1,
  BANIM_CONSTRUCT2,

  BANIM_IDLE1,
  BANIM_IDLE2,
  BANIM_IDLE3,

  BANIM_ATTACK1,
  BANIM_ATTACK2,

  BANIM_SPAWN1,
  BANIM_SPAWN2,

  BANIM_PAIN1,
  BANIM_PAIN2,

  BANIM_DESTROY1,
  BANIM_DESTROY2,
  BANIM_DESTROYED,

  MAX_BUILDABLE_ANIMATIONS
} buildableAnimNumber_t;

typedef enum
{
  WANIM_NONE,

  WANIM_IDLE,

  WANIM_DROP,
  WANIM_RELOAD,
  WANIM_RAISE,

  WANIM_ATTACK1,
  WANIM_ATTACK2,
  WANIM_ATTACK3,
  WANIM_ATTACK4,
  WANIM_ATTACK5,
  WANIM_ATTACK6,
  WANIM_ATTACK7,
  WANIM_ATTACK8,

  MAX_WEAPON_ANIMATIONS
} weaponAnimNumber_t;

typedef struct animation_s
{
  int   firstFrame;
  int   numFrames;
  int   loopFrames;     // 0 to numFrames
  int   frameLerp;      // msec between frames
  int   initialLerp;    // msec to get to first frame
  int   reversed;     // true if animation is reversed
  int   flipflop;     // true if animation should flipflop back to base
} animation_t;


// flip the togglebit every time an animation
// changes so a restart of the same anim can be detected
#define ANIM_TOGGLEBIT    0x80
#define ANIM_FORCEBIT     0x40

// Time between location updates
#define TEAM_LOCATION_UPDATE_TIME   500

// player classes
typedef enum
{
  PCL_NONE,

  //builder classes
  PCL_ALIEN_BUILDER0,
  PCL_ALIEN_BUILDER0_UPG,

  //offensive classes
  PCL_ALIEN_LEVEL0,
  PCL_ALIEN_LEVEL1,
  PCL_ALIEN_LEVEL1_UPG,
  PCL_ALIEN_LEVEL2,
  PCL_ALIEN_LEVEL2_UPG,
  PCL_ALIEN_LEVEL3,
  PCL_ALIEN_LEVEL3_UPG,
  PCL_ALIEN_LEVEL4,

  //human class
  PCL_HUMAN,
  PCL_HUMAN_BSUIT,

  PCL_NUM_CLASSES
} class_t;

// spectator state
typedef enum
{
  SPECTATOR_NOT,
  SPECTATOR_FREE,
  SPECTATOR_LOCKED,
  SPECTATOR_FOLLOW,
  SPECTATOR_SCOREBOARD
} spectatorState_t;

// modes of text communication
typedef enum
{
  SAY_ALL,
  SAY_TEAM,
  SAY_PRIVMSG,
  SAY_TPRIVMSG,
  SAY_AREA,
  SAY_ADMINS,
  SAY_ADMINS_PUBLIC,
  SAY_RAW
} saymode_t;

// means of death
typedef enum
{
  MOD_UNKNOWN,
  MOD_SHOTGUN,
  MOD_BLASTER,
  MOD_PAINSAW,
  MOD_MACHINEGUN,
  MOD_CHAINGUN,
  MOD_PRIFLE,
  MOD_MDRIVER,
  MOD_LASGUN,
  MOD_LCANNON,
  MOD_LCANNON_SPLASH,
  MOD_FLAMER,
  MOD_FLAMER_SPLASH,
  MOD_GRENADE,
  MOD_FRAGNADE,
  MOD_GRENADE_LAUNCHER,
  MOD_LIGHTNING,
  MOD_LIGHTNING_EMP,
  MOD_WATER,
  MOD_SLIME,
  MOD_LAVA,
  MOD_CRUSH,
  MOD_DROP,
  MOD_TELEFRAG,
  MOD_FALLING,
  MOD_SUICIDE,
  MOD_TARGET_LASER,
  MOD_TRIGGER_HURT,
  MOD_SUFFOCATION,

  MOD_ABUILDER_CLAW,
  MOD_LEVEL0_BITE,
  MOD_LEVEL1_CLAW,
  MOD_LEVEL1_PCLOUD,
  MOD_LEVEL3_CLAW,
  MOD_LEVEL3_POUNCE,
  MOD_LEVEL3_BOUNCEBALL,
  MOD_LEVEL2_CLAW,
  MOD_LEVEL2_ZAP,
  MOD_LEVEL4_CLAW,
  MOD_LEVEL4_TRAMPLE,
  MOD_LEVEL4_CRUSH,

  MOD_SLOWBLOB,
  MOD_POISON,
  MOD_SWARM,

  MOD_HSPAWN,
  MOD_TESLAGEN,
  MOD_MGTURRET,
  MOD_REACTOR,

  MOD_ASPAWN,
  MOD_ATUBE,
  MOD_OVERMIND,
  MOD_DECONSTRUCT,
  MOD_REPLACE,
  MOD_NOCREEP,
  MOD_SLAP
} meansOfDeath_t;

typedef enum
{
  PAIN_25,
  PAIN_50,
  PAIN_75,
  PAIN_100
} pain_t;

pain_t BG_GetPainState( playerState_t *ps );

//---------------------------------------------------------

// player class record
typedef struct
{
  class_t   number;

  qboolean  enabled;
  char      *name;
  char      *info;

  int       stages;

  int       health;
  float     fallDamage;
  float     regenRate;

  int       abilities;

  weapon_t  startWeapon;

  float     buildDist;

  int       fov;
  float     bob;
  float     bobCycle;
  float     landBob;
  int       steptime;

  float     speed;
  float     acceleration;
  float     airAcceleration;
  float     friction;
  float     stopSpeed;
  float     jumpMagnitude;
  float     knockbackScale;

  int       chargeStaminaMax;
  int       chargeStaminaRestore;

  int       children[ 3 ];
  int       cost;
  qboolean  warmupFree;
  int       value;

  team_t    team;
} classAttributes_t;

typedef struct
{
  char      modelName[ MAX_QPATH ];
  float     modelScale;
  char      skinName[ MAX_QPATH ];
  float     shadowScale;
  char      hudName[ MAX_QPATH ];
  char      humanName[ MAX_STRING_CHARS ];

  vec3_t    mins;
  vec3_t    maxs;
  vec3_t    crouchMaxs;
  vec3_t    deadMins;
  vec3_t    deadMaxs;
  int       viewheight;
  int       crouchViewheight;
  float     zOffset;
  vec3_t    shoulderOffsets;
} classConfig_t;

//stages
typedef enum
{
  S1,
  S2,
  S3
} stage_t;

// team attributes
typedef struct
{
  team_t         number;

  char          *name;
  char          *name2;
  char          *humanName;
  char          *info;
} teamAttributes_t;

#define MAX_BUILDABLE_MODELS 4

// buildable item record
typedef struct
{
  buildable_t   number;

  qboolean      enabled;
  char          *name;
  char          *humanName;
  char          *info;
  char          *entityName;

  trType_t      traj;
  float         bounce;

  int           buildPoints;
  int           stages;

  int           health;
  int           regenRate;

  int           splashDamage;
  int           splashRadius;

  int           meansOfDeath;

  team_t        team;
  weapon_t      buildWeapon;

  int           idleAnim;

  int           nextthink;
  int           buildTime;
  qboolean      activationEnt;
  int           activationFlags; // contains bit flags representing various
                                 //abilities of a given activation entity
  int           occupationFlags; // contains bit flags representing various
                                 //abilities of a given occupation entity
  pmtype_t			activationPm_type; // changes client's pm_type of an occupant
  int           activationContents; // changes the contents of an occupant
  int           activationClipMask; // changes the clip mask of an occupant

  int           turretRange;
  int           turretFireSpeed;
  qboolean      turretTrackOnlyOrigin;
  weapon_t      turretProjType;

  float         minNormal;
  qboolean      invertNormal;

  qboolean      creepTest;
  int           creepSize;

  qboolean      dccTest;
  qboolean      transparentTest;
  qboolean      uniqueTest;

  int           value;

  qboolean      stackable;

  rangeMarkerType_t rangeMarkerType;
  float             rangeMarkerRange;
  shaderColorEnum_t rangeMarkerColor;
  qboolean          rangeMarkerUseNormal;
  qboolean          rangeMarkerOriginAtTop;

  int               role;
} buildableAttributes_t;

typedef struct
{
  char      models[ MAX_BUILDABLE_MODELS ][ MAX_QPATH ];

  float     modelScale;
  vec3_t    mins;
  vec3_t    maxs;
  float     zOffset;
} buildableConfig_t;

typedef enum
{
  SPLATP_SPHERICAL_CONE,
  SPLATP_MIRRORED_INVERSE_SPHERICAL_CONE
} splatterPattern_t;

typedef enum
{
  SPLATD_RANDOM,
  SPLATD_UNIFORM,
  SPLATD_UNIFORM_ALTERNATING
} splatterDistribution_t;

typedef struct splatterAttributes_s
{
  unsigned int           number;
  splatterPattern_t      pattern;
  splatterDistribution_t distribution;
  unsigned int           pitchLayers; //doesn't apply to SPLATD_RANDOM
  float                  spread;
  int                    impactDamage;
  float                  impactDamageFalloff; //damage drops towards 0 as this distance
                                         //is approached, disabled if set to 0
  int                    impactDamageCap; //only applies if fallof is enabled
  float                  range;
} splatterAttributes_t;

// weapon record
typedef struct
{
  weapon_t  number;

  qboolean  enabled;
  int       price; // doesn't include the price for ammo
  qboolean  warmupFree;
  int       stages;

  int       slots;

  char      *name;
  char      *humanName;
  char      *info;

  int       maxAmmo;
  int       maxClips;
  int       ammoUsage1;
  int       ammoUsage2;
  int       ammoUsage3;
  int       roundPrice; // doesn't apply to energy weapons
  qboolean  ammoPurchasable;
  qboolean  infiniteAmmo;
  qboolean  usesEnergy;

  int       repeatRate1;
  int       repeatRate2;
  int       repeatRate3;
  int       burstRounds1;
  int       burstRounds2;
  int       burstRounds3;
  int       burstDelay1;
  int       burstDelay2;
  int       burstDelay3;
  int       reloadTime;
  float     knockbackScale;

  qboolean  hasAltMode;
  qboolean  hasThirdMode;

  qboolean  canZoom;
  float     zoomFov;

  qboolean  purchasable;
  qboolean  longRanged;

  splatterAttributes_t splatter[3];

  team_t    team;
} weaponAttributes_t;

// upgrade record
typedef struct
{
  upgrade_t number;

  qboolean  enabled;
  int       price;
  qboolean  warmupFree;
  int       stages;

  int       slots;

  char      *name;
  char      *humanName;
  char      *info;

  char      *icon;

  qboolean  purchasable;
  qboolean  usable;

  team_t    team;
} upgradeAttributes_t;

qboolean  BG_WeaponIsFull( weapon_t weapon, int stats[ ], int ammo, int clips );
qboolean  BG_InventoryContainsWeapon( int weapon, int stats[ ] );
int       BG_SlotsForInventory( int stats[ ] );
void      BG_AddUpgradeToInventory( int item, int stats[ ] );
void      BG_RemoveUpgradeFromInventory( int item, int stats[ ] );
qboolean  BG_InventoryContainsUpgrade( int item, int stats[ ] );
void      BG_ActivateUpgrade( int item, int stats[ ] );
void      BG_DeactivateUpgrade( int item, int stats[ ] );
qboolean  BG_UpgradeIsActive( int item, int stats[ ] );
qboolean  BG_RotateAxis( vec3_t surfNormal, vec3_t inAxis[ 3 ],
                         vec3_t outAxis[ 3 ], qboolean inverse, qboolean ceiling );
void      BG_GetClientNormal( const playerState_t *ps, vec3_t normal );
void      BG_GetClientViewOrigin( const playerState_t *ps, vec3_t viewOrigin );
void      BG_CalcMuzzlePointFromPS( const playerState_t *ps, vec3_t forward,
                                    vec3_t right, vec3_t up, vec3_t muzzlePoint );
int       BG_LightningBoltRange( const entityState_t *es,
                                 const playerState_t *ps,
                                 qboolean currentRange );
void      BG_CheckBoltImpactTrigger( pmove_t *pmove,
                                     void (*trace)( trace_t *, const vec3_t,
                                                    const vec3_t, const vec3_t,
                                                    const vec3_t, int, int ),
                                     void (*UnlaggedOn)( int, vec3_t, float ),
                                     void (*UnlaggedOff)( void ) );
void      BG_ResetLightningBoltCharge( playerState_t *ps, pmoveExt_t *pmext );
void      BG_PositionBuildableRelativeToPlayer( const playerState_t *ps,
                                                const vec3_t mins, const vec3_t maxs,
                                                void (*trace)( trace_t *, const vec3_t, const vec3_t,
                                                               const vec3_t, const vec3_t, int, int ),
                                                vec3_t outOrigin, vec3_t outAngles, trace_t *tr );
int       BG_GetValueOfPlayer( playerState_t *ps );
qboolean  BG_PlayerCanChangeWeapon( playerState_t *ps, pmoveExt_t *pmext );
int       BG_PlayerPoisonCloudTime( playerState_t *ps );
weapon_t  BG_GetPlayerWeapon( playerState_t *ps );
qboolean  BG_HasEnergyWeapon( playerState_t *ps );

#define MAX_NUM_PACKED_ENTITY_NUMS 10
int       BG_UnpackEntityNumbers( entityState_t *es, int *entityNums, int count );

const buildableAttributes_t *BG_BuildableByName( const char *name );
const buildableAttributes_t *BG_BuildableByEntityName( const char *name );
const buildableAttributes_t *BG_Buildable( buildable_t buildable );
qboolean                    BG_BuildableAllowedInStage( buildable_t buildable,
                                                        stage_t stage,
                                                        int gameIsInWarmup );

buildableConfig_t           *BG_BuildableConfig( buildable_t buildable );
void                        BG_BuildableBoundingBox( buildable_t buildable,
                                                     vec3_t mins, vec3_t maxs );
void                        BG_InitBuildableConfigs( void );

const classAttributes_t     *BG_ClassByName( const char *name );
const classAttributes_t     *BG_Class( class_t class );
qboolean                    BG_ClassAllowedInStage( class_t class,
                                                    stage_t stage,
                                                    int gameIsInWarmup );

classConfig_t               *BG_ClassConfig( class_t class );

void                        BG_ClassBoundingBox( class_t class, vec3_t mins,
                                                 vec3_t maxs, vec3_t cmaxs,
                                                 vec3_t dmins, vec3_t dmaxs );
qboolean                    BG_ClassHasAbility( class_t class, int ability );
int                         BG_ClassCanEvolveFromTo( class_t fclass,
                                                     class_t tclass,
                                                     int credits,
                                                     int alienStage, int num,
                                                     int gameIsInWarmup,
                                                     qboolean devMode,
                                                     qboolean class_forced);
qboolean                    BG_AlienCanEvolve( class_t class, int credits,
                                               int alienStage,
                                               int gameIsInWarmup,
                                               qboolean devMode,
                                               qboolean class_forced );

void                        BG_InitClassConfigs( void );

const weaponAttributes_t    *BG_WeaponByName( const char *name );
const weaponAttributes_t    *BG_Weapon( weapon_t weapon );
qboolean                    BG_WeaponAllowedInStage( weapon_t weapon,
                                                     stage_t stage,
                                                     int gameIsInWarmup );
int                         BG_AmmoUsage( playerState_t *ps );
int                         BG_TotalPriceForWeapon( weapon_t weapon,
                                                    int gameIsInWarmup );

typedef struct splatterData_s
{
  weapon_t     weapon;
  weaponMode_t weaponMode;
  vec3_t       origin;
  trace_t      *tr;
  void         *user_data;
} splatterData_t;
void BG_SplatterPattern( vec3_t origin2, int seed, int passEntNum,
                         splatterData_t *data, void (*func)( splatterData_t *data ),
                         void (*trace)( trace_t *, const vec3_t,
                                        const vec3_t, const vec3_t,
                                        const vec3_t, int, int ) );

const upgradeAttributes_t   *BG_UpgradeByName( const char *name );
const upgradeAttributes_t   *BG_Upgrade( upgrade_t upgrade );
qboolean                    BG_UpgradeAllowedInStage( upgrade_t upgrade,
                                                      stage_t stage,
                                                      int gameIsInWarmup );

// content masks
#define MASK_ALL          (-1)
#define MASK_SOLID        (CONTENTS_SOLID)
#define MASK_PLAYERSOLID  (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY)
#define MASK_DEADSOLID    (CONTENTS_SOLID|CONTENTS_PLAYERCLIP)
#define MASK_ASTRALSOLID  (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_ASTRAL_NOCLIP)
#define MASK_WATER        (CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define MASK_OPAQUE       (CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define MASK_SHOT         (CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE)


//
// entityState_t->eType
//
typedef enum
{
  ET_GENERAL,
  ET_PLAYER,
  ET_ITEM,

  ET_BUILDABLE,

  ET_LOCATION,

  ET_MISSILE,
  ET_MOVER,
  ET_BEAM,
  ET_PORTAL,
  ET_SPEAKER,
  ET_PUSH_TRIGGER,
  ET_TELEPORT_TRIGGER,
  ET_INVISIBLE,
  ET_GRAPPLE,       // grapple hooked on wall

  ET_CORPSE,
  ET_PARTICLE_SYSTEM,
  ET_ANIMMAPOBJ,
  ET_MODELDOOR,
  ET_LIGHTFLARE,
  ET_LEV2_ZAP_CHAIN,
  ET_TELEPORTAL,

  ET_EVENTS       // any of the EV_* events can be added freestanding
              // by setting eType to ET_EVENTS + eventNum
              // this avoids having to set eFlags and eventNum
} entityType_t;

void  BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result );
void  BG_EvaluateTrajectoryDelta( const trajectory_t *tr, int atTime, vec3_t result );

void  BG_AddPredictableEventToPlayerstate( int newEvent, int eventParm, playerState_t *ps );

void  BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s, qboolean snap );
void  BG_PlayerStateToEntityStateExtraPolate( playerState_t *ps, entityState_t *s, int time, qboolean snap );

qboolean  BG_PlayerTouchesItem( playerState_t *ps, entityState_t *item, int atTime );

#define ARENAS_PER_TIER   4
#define MAX_ARENAS      1024
#define MAX_ARENAS_TEXT   8192

#define MAX_BOTS      1024
#define MAX_BOTS_TEXT   8192

float   atof_neg( char *token, qboolean allowNegative );
int     atoi_neg( char *token, qboolean allowNegative );

void BG_ParseCSVEquipmentList( const char *string, weapon_t *weapons, int weaponsSize,
    upgrade_t *upgrades, int upgradesSize );
void BG_ParseCSVClassList( const char *string, class_t *classes, int classesSize );
void BG_ParseCSVBuildableList( const char *string, buildable_t *buildables, int buildablesSize );
void BG_InitAllowedGameElements( void );
qboolean BG_WeaponIsAllowed( weapon_t weapon, qboolean devMode );
qboolean BG_UpgradeIsAllowed( upgrade_t upgrade, qboolean devMode );
qboolean BG_ClassIsAllowed( class_t class, qboolean devMode );
qboolean BG_BuildableIsAllowed( buildable_t buildable, qboolean devMode );

// Friendly Fire Flags
#define FFF_HUMANS         1
#define FFF_ALIENS         2
#define FFF_BUILDABLES     4

// bg_voice.c
#define MAX_VOICES                8
#define MAX_VOICE_NAME_LEN        16
#define MAX_VOICE_CMD_LEN         16
#define VOICE_ENTHUSIASM_DECAY    0.5f // enthusiasm lost per second

typedef enum
{
  VOICE_CHAN_ALL,
  VOICE_CHAN_TEAM ,
  VOICE_CHAN_LOCAL,

  VOICE_CHAN_NUM_CHANS
} voiceChannel_t;

typedef struct voiceTrack_s
{
#ifdef CGAME
  sfxHandle_t            track;
  int                    duration;
#endif
  char                   *text;
  int                    enthusiasm;
  int                    team;
  int                    class;
  int                    weapon;
  struct voiceTrack_s    *next;
} voiceTrack_t;


typedef struct voiceCmd_s
{
  char              cmd[ MAX_VOICE_CMD_LEN ];
  voiceTrack_t      *tracks;
  struct voiceCmd_s *next;
} voiceCmd_t;

typedef struct voice_s
{
  char             name[ MAX_VOICE_NAME_LEN ];
  voiceCmd_t       *cmds;
  struct voice_s   *next;
} voice_t;

voice_t *BG_VoiceInit( void );
void BG_PrintVoices( voice_t *voices, int debugLevel );

voice_t *BG_VoiceByName( voice_t *head, char *name );
voiceCmd_t *BG_VoiceCmdFind( voiceCmd_t *head, char *name, int *cmdNum );
voiceCmd_t *BG_VoiceCmdByNum( voiceCmd_t *head, int num);
voiceTrack_t *BG_VoiceTrackByNum( voiceTrack_t *head, int num );
voiceTrack_t *BG_VoiceTrackFind( voiceTrack_t *head, team_t team,
                                 class_t class, weapon_t weapon,
                                 int enthusiasm, int *trackNum );

int BG_LoadEmoticons( emoticon_t *emoticons, int num );

const teamAttributes_t *BG_Team( team_t team );

entityState_t *BG_EntityState(int ent_num);
void BG_Link_entityState(entityState_t *es, int ent_num);

//for movers
void BG_CreateRotationMatrix( vec3_t angles, vec3_t matrix[ 3 ] );
void BG_TransposeMatrix( vec3_t matrix[ 3 ], vec3_t transpose[ 3 ] );
void BG_RotatePoint( vec3_t point, vec3_t matrix[ 3 ] );

typedef enum
{
  BBXP_ORIGIN = 0,

  BBXP_MIDFACE1,
  BBXP_MIDFACE2,
  BBXP_MIDFACE3,
  BBXP_MIDFACE4,
  BBXP_MIDFACE5,
  BBXP_MIDFACE6,

  BBXP_VERTEX1,
  BBXP_VERTEX2,
  BBXP_VERTEX3,
  BBXP_VERTEX4,
  BBXP_VERTEX5,
  BBXP_VERTEX6,
  BBXP_VERTEX7,
  BBXP_VERTEX8,

  NUM_BBOX_POINTS
} bboxPointNum_t;

typedef struct bboxPoint_s
{
  bboxPointNum_t num;
  vec3_t         point;
} bboxPoint_t;

void BG_EvaluateBBOXPoint( bboxPoint_t *bboxPoint, vec3_t origin,
                           const vec3_t minsIn, const vec3_t maxsIn );

typedef struct
{
  const char *name;
} dummyCmd_t;
int cmdcmp( const void *a, const void *b );

typedef enum
{
  KICK_VOTE,
  POLL_VOTE,
  MUTE_VOTE,
  UNMUTE_VOTE,
  MAP_RESTART_VOTE,
  MAP_VOTE,
  NEXTMAP_VOTE,
  DRAW_VOTE,
  SUDDEN_DEATH_VOTE,
  EXTEND_VOTE,
  DENYBUILD_VOTE,
  ALLOWBUILD_VOTE,
  ADMITDEFEAT_VOTE,

  NUM_VOTE_TYPES,
  VOID_VOTE
} vote_t;

//Center print order of importance
typedef enum
{
  CP_ADMIN,
  CP_ADMIN_CP,
  CP_PAUSE,
  CP_LIFE_SUPPORT,
    CP_INACTIVITY = CP_LIFE_SUPPORT,
  CP_SUDDEN_DEATH,
    CP_TIME_LIMIT = CP_SUDDEN_DEATH,
  CP_NEED_SPAWNS,
  CP_UNDER_ATTACK,
    CP_OVERMIND_AWAKEN = CP_UNDER_ATTACK,
  CP_STAGE_UP,
  CP_PRIVATE_MESSAGE,
  CP_TEAM_KILL,
  CP_SPAWN_BLOCK,
    CP_TELEPORT = CP_SPAWN_BLOCK,
  CP_MAP,
  CP_EXTRA1,//Use CP_EXTRA instead of these.
  CP_EXTRA2,
  CP_EXTRA3,
  CP_MAX,
  CP_EXTRA = -CP_EXTRA1
} centerPrintImportance;
//  Center prints that do not need to be ordered and are of low importance
//    can use CP_EXTRA
#define CP_EXTRA -CP_EXTRA1
