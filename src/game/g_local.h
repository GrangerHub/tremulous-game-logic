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

// g_local.h -- local definitions for game module

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "g_public.h"

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

#include "g_admin.h"
#include "g_playmap.h"

//==================================================================

#define INFINITE      1000000

#define FRAMETIME     100         // msec

#define INTERMISSION_DELAY_TIME 1000

/*
--------------------------------------------------------------------------------
gentity->flags
*/
#define FL_GODMODE          0x00000010
#define FL_NOTARGET         0x00000020
#define FL_TEAMSLAVE        0x00000400  // not the first on the team
#define FL_NO_KNOCKBACK     0x00000800
#define FL_DROPPED_ITEM     0x00001000
#define FL_NO_BOTS          0x00002000  // spawn point not for bot use
#define FL_NO_HUMANS        0x00004000  // spawn point just for bots
#define FL_FORCE_GESTURE    0x00008000  // spawn point just for bots
#define FL_BOUNCE           0x00010000  // for missiles
#define FL_BOUNCE_HALF      0x00020000  // for missiles
#define FL_NO_BOUNCE_SOUND  0x00040000  // for missiles
#define FL_OCCUPIED         0x00080000  // for occupiable entities

#define  VALUE_MASK       0x0001FFFF
#define  SIGN_BIT         0x00020000
#define  TEAM_BIT         0x00040000
#define  LOCKSTAGE_BIT    0x00080000
#define  AMP_TRIGGER      0x20000000
#define  RESET_AFTER_USE  0x40000000
#define  RESET_BIT        0x80000000

/*
--------------------------------------------------------------------------------
*/

// movers are things like doors, plats, buttons, etc
typedef enum
{
  MS_POS1,
  MS_POS2,
  MS_1TO2,
  MS_2TO1
} moverState_t;

typedef enum mover_type_s
{
  MM_LINEAR,
  MM_ROTATION,
  MM_MODEL
} mover_motion_t;

/*
--------------------------------------------------------------------------------
To create a new expiration timer, add a new member to the expire_t enum below.
Expiration timers are set by G_SetExpiration(), and are checked by G_Expired().
*/
typedef enum
{
  EXP_SPAWN_BLOCK,
  EXP_MAP_PRINT,
  EXP_READY,
  EXP_PORTAL_EFFECT,
  EXP_PORTAL_PARTICLES,

  NUM_EXPIRE_TYPES
} expire_t;

/*
--------------------------------------------------------------------------------
for linearly transforming the dimensions of a missile over a given scale period
*/

typedef struct scale_missile_s
{
  qboolean enabled;

  int start_time;
  int stop_time;

  vec3_t start_mins;
  vec3_t start_maxs;
  vec3_t end_mins;
  vec3_t end_maxs;
} scale_missile_t;

/*
--------------------------------------------------------------------------------
*/

//============================================================================

struct gentity_s
{
  entityState_t     s;        // communicated by server to clients
  entityShared_t    r;        // shared by both the server system and game

  // DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
  // EXPECTS THE FIELDS IN THAT ORDER!
  //================================

  bgentity_id       idAtLastDeath; // Used to ensure that entities don't die twice without respawning

  struct gclient_s  *client;        // NULL if not a client

  qboolean          inuse;

  char              *classname;     // set in QuakeEd
  int               spawnflags;     // set in QuakeEd

  qboolean          neverFree;      // if true, FreeEntity will only unlink
                                    // bodyque uses this

  int               flags;          // FL_* variables

  char              *model;
  char              *model2;
  int               freetime;       // level.time when the object was freed

  int               eventTime;      // events will be cleared EVENT_VALID_MSEC after set
  qboolean          freeAfterEvent;
  qboolean          unlinkAfterEvent;

  qboolean          physicsObject;  // if true, it can be pushed by movers and fall off edges
                                    // all game items are physicsObjects,
  float             physicsBounce;  // 1.0 = continuous bounce, 0.0 = no bounce
  content_mask_t    clip_mask;      // brushes with this content value will be collided against
                                    // when moving.  items and corpses do not collide against
                                    // players, for instance

  scale_missile_t   scale_missile;

  // movers
  moverState_t      moverState;
  mover_motion_t    moverMotionType;
  int               soundPos1;
  int               sound1to2;
  int               sound2to1;
  int               soundPos2;
  int               soundLoop;
  gentity_t         *parent;
  gentity_t         *nextTrain;
  vec3_t            pos1, pos2;
  float             rotatorAngle;
  gentity_t         *clipBrush;     // clipping brush for model doors

  char              *message;

  int               timestamp;      // body queue sinking, etc

  int               expireTimes[ NUM_EXPIRE_TYPES ]; // used by G_Expired()

  char              *target;
  char              *multitarget[ MAX_TARGETS ];
  char              *targetname;
  char              *multitargetname[ MAX_TARGETNAMES ];
  char              *team;
  char              *targetShaderName;
  char              *targetShaderNewName;
  gentity_t         *target_ent;

  float             speed;
  float             lastSpeed;      // used by trains that have been restarted
  vec3_t            movedir;

  // acceleration evaluation
  qboolean          evaluateAcceleration;
  vec3_t            oldVelocity;
  vec3_t            acceleration;
  vec3_t            oldAccel;
  vec3_t            jerk;

  int               nextthink;
  void              (*think)( gentity_t *self );
  void              (*reached)( gentity_t *self );  // movers call this when hitting endpoint
  void              (*blocked)( gentity_t *self, gentity_t *other );
  void              (*touch)( gentity_t *self, gentity_t *other, trace_t *trace );
  void              (*use)( gentity_t *self, gentity_t *other, gentity_t *activator );
  void              (*pain)( gentity_t *self, gentity_t *attacker, int damage );
  void              (*die)( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod );

  // for activation entities
  struct activation_s
  {
    int       flags; // Contains bit flags representing various abilities of a
                     // given activation entity.

    int       usable_map_trigger;

    dynMenu_t menuMsg; // Message sent to the activator when an activation
                       // fails.  Can be used in (*willActivate)().

    dynMenu_t menuMsgOvrd[ MAX_ACTMN ]; // Used to override the general
                                        // activation menu messages.

    // If qtrue is returned, an occupiable activation entity would then be
    // occupied.
    qboolean  (*activate)( gentity_t *self, gentity_t *activator );

    // Optional custom restrictions on the search for a nearby activation entity
    // that the general activation.flags don't address.
    qboolean  (*canActivate)( gentity_t *self, gclient_t *client );

    // Optional custom restrictions on the actual activation of a nearby found
    // activation entity.
    qboolean  (*willActivate)( gentity_t *actEnt, gentity_t *activator );
  } activation;

  // for occupation entities
  struct occupation_s
  {
    int       flags; // Contains bit flags representing various abilities of a
                     // given occupation entity.

    gentity_t *occupant; // The entity that is occupying this occupation entity

    gentity_t *occupantFound; // A temporary variable used in the occupying
                               // process. This can be set by (*findOccupant)().

    gentity_t *occupied; // The occupiable entity that is being considered.

    gentity_t *other;  // An optional additional entity involved in occupation.

    int       occupantFlags; // Contains bit flags used by occupants

    pmtype_t	pm_type; // Changes client's pm_type of an occupant.

    int       contents; // Changes the contents of an occupant.

    int       unoccupiedContents; // Used to restore the contents of an occupant
               // that leaves its occupied activation entity.

    content_mask_t clip_mask; // Changes the clip mask of an occupant.

    content_mask_t unoccupied_clip_mask; // Used to restore the clip mask of an occupant
               // that leaves its occupied activation entity.

   // Optional custom function called to perform additional operations for
   // occupation.
   void (*occupy)( gentity_t *occupied );

   // Optional custom function for leaving an occupiable entity.
   // Unless force is set to qtrue, if qfalse is returned, the entity remains
   // occupied.
   qboolean  (*unoccupy)( gentity_t *occupied, gentity_t *occupant,
                          gentity_t *activator, qboolean force );

   // Optional custom resets for occupation entities.
   void      (*occupiedReset)( gentity_t *occupied );
   void      (*occupantReset)( gentity_t *occupant );

   // Optional custom conditions that would force a client to unoccupy if qtrue
   // is returned.
   qboolean  (*occupyUntil)( gentity_t *occupied, gentity_t *occupant );

   // Optional funtion to find an occupant which isn't an activator.
   void      (*findOccupant)( gentity_t *actEnt, gentity_t *activator );

   // Optional function that returns another entity involved in the occupation.
   void      (*findOther)(gentity_t *actEnt, gentity_t *activator );
  } occupation;

  int               pain_debounce_time;
  int               last_move_time;

  int               health;
  int               lastHealth; // currently only used for overmind

  qboolean          takedamage;
  int               dmgProtectionTime; // momentarily protection against damage
  int               targetProtectionTime; // momentarily protectiion against targeting
  int               noTriggerHurtDmgTime; // time that trigger hurt can't damage

  int               damage;
  int               splashDamage; // quad will increase this without increasing radius
  int               splashRadius;
  int               methodOfDeath;
  int               splashMethodOfDeath;
  qboolean          noKnockback; // for direct impact by missiles.

  int               count;

  gentity_t         *chain;
  gentity_t         *enemy;
  bboxPointNum_t    trackedEnemyPointNum; // for tracking specific enemy points
  gentity_t         *activator;
  gentity_t         *teamchain;   // next entity in team
  gentity_t         *teammaster;  // master of the team

  int               watertype;
  int               waterlevel;

  int               noise_index;

  // timing variables
  float             wait;
  float             random;

  team_t            stageTeam;
  stage_t           stageStage;

  team_t            buildableTeam;      // buildable item team
  gentity_t         *parentNode;        // for creep and defense/spawn dependencies
  qboolean          rangeMarker;
  qboolean          active;             // for power repeater, but could be useful elsewhere
  qboolean          powered;            // for human buildables
  struct namelog_s  *builtBy;           // person who built this
  struct buildlog_s *buildLog;          // the build log for when this buildable was constructured (NULL if built by the world)
  int               dcc;                // number of controlling dccs
  qboolean          spawned;            // whether or not this buildable has finished spawning
  int               shrunkTime;         // time when a barricade shrunk or zero
  int               buildTime;          // when this buildable was built
  int               animTime;           // last animation change
  int               time1000;           // timer evaluated every second
  qboolean          deconstruct;        // deconstruct if no BP left
  int               deconstructTime;    // time at which structure marked
  int               markDeconstructor;  // number of the builder that marked the deconstructed buildable
  int               overmindAttackTimer;
  int               overmindDyingTimer;
  int               overmindSpawnsTimer;
  int               nextPhysicsTime;    // buildables don't need to check what they're sitting on
                                        // every single frame.. so only do it periodically
  int               clientSpawnTime;    // the time until this spawn can spawn a client
  int               spawnBlockTime;     // timer for anti spawn block
  int               attemptSpawnTime;

  qboolean          lev1Grabbed;        //TA: for turrets interacting with lev1s
  int               lev1GrabTime;       //TA: for turrets interacting with lev1s

  int               credits[ MAX_CLIENTS ];     // human credits for each client
  int               killedBy;                   // clientNum of killer

  bglist_t         targeted;           // a queue of currently valid targets for a turret
  vec3_t            turretAim;          // aim vector for turrets
  vec3_t            turretAimRate;      // track turn speed for norfenturrets
  int               turretSpinupTime;   // spinup delay for norfenturrets

  vec4_t            animation;          // animated map objects

  qboolean          nonSegModel;        // this entity uses a nonsegmented player model

  buildable_t       bTriggers[ BA_NUM_BUILDABLES ]; // which buildables are triggers
  class_t           cTriggers[ PCL_NUM_CLASSES ];   // which classes are triggers
  weapon_t          wTriggers[ WP_NUM_WEAPONS ];    // which weapons are triggers
  upgrade_t         uTriggers[ UP_NUM_UPGRADES ];   // which upgrades are triggers

  int               triggerGravity;                 // gravity for this trigger

  int               suicideTime;                    // when the client will suicide

  int               lastDamageTime;
  int               nextRegenTime;
  int               lastInflictDamageOnEnemyTime;

  qboolean          pointAgainstWorld;              // don't use the bbox for map collisions

  // variables for buildable stacking
  qboolean          damageDroppedBuildable;
  int               dropperNum;
  int               buildableStack[ MAX_GENTITIES ];
  int               numOfStackedBuildables;

  int               buildPointZone;                 // index for zone
  int               usesBuildPointZone;             // does it use a zone?

  // AMP variables
  int               hurt;
  char              *Cvar_Val;
  char              *sign;
  char              *inventory;
  int               AMPintVAR[ 4 ];
  qboolean          TrigOnlyRise;
  int               TargetGate;
  int               ResetValue;
  int               Charge;
  int               GateState;
  qboolean          MasterPower;
  int               PowerRadius;

  qboolean          lasermine_set;
  trace_t           lasermine_trace;
  int               lasermine_self_destruct_time;

  bglink_t          *zapLink;  // For ET_LEV2_ZAP_CHAIN
};

typedef enum
{
  CON_DISCONNECTED,
  CON_CONNECTING,
  CON_CONNECTED
} clientConnected_t;

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
typedef struct
{
  int               spectatorTime;    // for determining next-in-line to play
  spectatorState_t  spectatorState;
  int               spectatorClient;  // for chasecam and follow mode
  team_t            restartTeam; //for !restart keepteams and !restart switchteams
  rank_t            rank;
  qboolean          readyToPlay; // ready state for Warmup
  clientList_t      ignoreList;
} clientSession_t;

// for sending private messages to your previous recipients
typedef struct prevRecipients_s
{
  int  id[ ( MAX_CLIENTS * 2 ) ];
  int  count;
} prevRecipients_t;

// namelog
#define MAX_NAMELOG_NAMES 5
#define MAX_NAMELOG_ADDRS 5
typedef struct namelog_s
{
  struct namelog_s  *next;
  char              name[ MAX_NAMELOG_NAMES ][ MAX_NAME_LENGTH ];
  addr_t            ip[ MAX_NAMELOG_ADDRS ];
  char              guid[ 33 ];
  qboolean          guidless;
  int               slot;
  qboolean          banned;

  int               nameOffset;
  int               nameChangeTime;
  int               nameChanges;
  int               voteCount;

  int               newbieNumber;

  qboolean          muted;
  qboolean          denyBuild;
  int               specExpires; // level.time at which a player can join a
                                 // team again after being forced into spectator

  int               score;
  int               credits;
  team_t            team;

  prevRecipients_t  prevRecipients;

  int               id;
} namelog_t;

// client data that stays across multiple respawns, but is cleared
// on each level change or team change at ClientBegin()
typedef struct
{
  clientConnected_t   connected;
  qboolean            firstConnection;
  usercmd_t           cmd;                // we would lose angles if not persistant
  qboolean            localClient;        // true if "ip" info key is "localhost"
  qboolean            stickySpec;         // don't stop spectating a player after they get killed
  qboolean            pmoveFixed;         //
  char                netname[ MAX_NAME_LENGTH ];
  int                 enterTime;          // level.time the client entered the game
  int                 location;           // player locations
  int                 teamInfo;           // level.time of team overlay update (disabled = 0)
  float               flySpeed;           // for spectator/noclip moves
  int                 swapAttacks;
  float               wallJumperMinFactor;
  float               marauderMinJumpFactor;
  int                 buildableRangeMarkerMask;

  class_t             classSelection;     // player class (copied to ent->client->ps.stats[ STAT_CLASS ] once spawned)
  float               evolveHealthFraction;
  float               evolveChargeStaminaFraction;
  weapon_t            humanItemSelection; // humans have a starting item
  team_t              teamSelection;      // player team (copied to ps.stats[ STAT_TEAM ])

  int                 teamChangeTime;     // level.time of last team change
  namelog_t           *namelog;
  g_admin_admin_t     *admin;

  int                 secondsAlive;       // time player has been alive in seconds
  int                 lastSpawnedTime;    // level.time the client has last spawned
  qboolean            hasHealed;          // has healed a player (basi regen aura) in the last 10sec (for score use)

  int                 spawn_queue_pos;    // position in the spawn queue
  int                 spawnTime;          // can spawn when time > this

  int                 damageProtectionDuration; //length of time for damage protection for the next spawn

  // used to save persistant[] values while in SPECTATOR_FOLLOW mode
  int                 credit;

  //start of match "fight sound"
  qboolean fight;

  // voting state
  int                 voted;
  int                 vote;

  // timers
  int                 voterInactivityTime;   // doesn't count a client in vote results when time > this

  // used for checking if a client's command angles changed
  int                 previousCmdAngles[3];
  int                 previousCmdAnglesTime;
  qboolean            cmdAnglesChanged;

  // flood protection
  int                 floodDemerits;
  int                 floodTime;

  vec3_t              lastDeathLocation;
  int                 alternateProtocol;
  char                guid[ 33 ];
  qboolean            guidless;
  addr_t              ip;
  char                voice[ MAX_VOICE_NAME_LEN ];
  qboolean            useUnlagged;

  int                 replacable_buildables[MAX_REPLACABLE_BUILDABLES];

  // level.time when teamoverlay info changed so we know to tell other players
  int                 infoChangeTime;
} clientPersistant_t;

#define MAX_TRAMPLE_BUILDABLES_TRACKED 20
// this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s
{
  // ps MUST be the first element, because the server expects it
  playerState_t       ps;       // communicated by server to clients

  // exported into pmove, but not communicated to clients
  pmoveExt_t          pmext;

  // the rest of the structure is private to game
  clientPersistant_t  pers;
  clientSession_t     sess;

  qboolean            readyToExit;    // wishes to leave the intermission

  qboolean            noclip;
  int                 cliprcontents;  // the backup layer of ent->r.contents for when noclipping

  int                 lastCmdTime;    // level.time of last usercmd_t, for EF_CONNECTION
                                      // we can't just use pers.lastCommand.time, because
                                      // of the g_sycronousclients case
  int                 buttons;
  int                 oldbuttons;
  int                 latched_buttons;

  vec3_t              oldOrigin;

  // sum up damage over an entire frame, so
  // shotgun blasts give a single big kick
  int                 damage_armor;     // damage absorbed by armor
  int                 damage_blood;     // damage taken out of health
  int                 damage_knockback; // impact damage
  vec3_t              damage_from;      // origin for vector calculation
  qboolean            damage_fromWorld; // if true, don't use the damage_from vector

  //
  int                 lastkilled_client;// last client that this client killed
  int                 lasthurt_client;  // last client that damaged this client
  int                 lasthurt_mod;     // type of damage the client did

  // timers
  int                 respawnTime;      // can join queue when time > this
  qboolean            spawnReady;       // player will spawn when spawnTime > level.time and spawnReady
  int                 inactivityTime;   // kick players when time > this
  qboolean            inactivityWarning;// qtrue if the five seoond warning has been given
  int                 rewardTime;       // clear the EF_AWARD_IMPRESSIVE, etc when time > this
  int                 boostedTime;      // last time we touched a booster

  int                 airOutTime;

  qboolean            fireHeld;         // used for hook
  qboolean            fire2Held;        // used for alt fire
  gentity_t           *hook;            // grapple hook if out

  int                 switchTeamTime;   // time the player switched teams

  int                 time100;          // timer for 100ms interval events
  int                 time1000;         // timer for one second interval events
  int                 time10000;        // timer for ten second interval events

  char                *areabits;

  int                 lastSuffocationTime;
  int                 lastPoisonTime;
  int                 poisonImmunityTime;
  gentity_t           *lastPoisonClient;
  int                 lastPoisonCloudedTime;
  int                 grabExpiryTime;
  int                 lastLockTime;
  int                 lastSlowTime;
  int                 lastMedKitTime;
  int                 medKitHealthToRestore;
  int                 medKitIncrementTime;
  int                 nextMedKitRestoreStaminaTime;
  int                 medKitStaminaToRestore;
  int                 lastCreepSlowTime;    // time until creep can be removed

  qboolean            charging;

  bgentity_id         firedBallLightning[ LIGHTNING_BALL_BURST_ROUNDS ];
  int                 firedBallLightningNum;

  int                 lastFlameBall;        // s.number of the last flame ball fired

  float               voiceEnthusiasm;
  char                lastVoiceCmd[ MAX_VOICE_CMD_LEN ];

  int                 lcannonStartTime;
  int                 trampleBuildablesHitPos;
  int                 trampleBuildablesHit[ MAX_TRAMPLE_BUILDABLES_TRACKED ];

  int                 lastCrushTime;        // Tyrant crush

  int                 portalTime;

  gentity_t           *built; //temporary pointer for building fx, indacting which buildable a builder just built.
  int                 buildFireTime;
};

#define QUEUE_PLUS1(x)  (((x)+1)%MAX_CLIENTS)
#define QUEUE_MINUS1(x) (((x)+MAX_CLIENTS-1)%MAX_CLIENTS)

void      G_PrintSpawnQueue( team_t team );


#define MAX_DAMAGE_REGION_TEXT    8192
#define MAX_DAMAGE_REGIONS 16

// build point zone
typedef struct
{
  int active;

  int totalBuildPoints;
  int queuedBuildPoints;
  int nextQueueTime;
} buildPointZone_t;

// store locational damage regions
typedef struct damageRegion_s
{
  char      name[ 32 ];
  float     area, modifier, minHeight, maxHeight;
  int       minAngle, maxAngle;
  qboolean  crouch;
} damageRegion_t;

//status of the warning of certain events
typedef enum
{
  TW_NOT = 0,
  TW_IMMINENT,
  TW_PASSED
} timeWarning_t;

// fate of a buildable
typedef enum
{
  BF_CONSTRUCT,
  BF_DECONSTRUCT,
  BF_REPLACE,
  BF_DESTROY,
  BF_TEAMKILL,
  BF_UNPOWER,
  BF_AUTO
} buildFate_t;

// data needed to revert a change in layout
typedef struct buildlog_s
{
  int          time;
  buildFate_t  fate;
  namelog_t    *actor;
  namelog_t    *builtBy;
  buildable_t  attackerBuildable; // if destroyed by another buildable
  buildable_t  modelindex;
  qboolean     deconstruct;
  int          deconstructTime;
  vec3_t       origin;
  vec3_t       angles;
  vec3_t       origin2;
  vec3_t       angles2;
  buildable_t  powerSource;
  int          powerValue;
} buildLog_t;

//
// this structure is cleared as each map is entered
//
#define MAX_SPAWN_VARS      512
#define MAX_SPAWN_VARS_CHARS  4096
#define MAX_BUILDLOG          1024
#define MAX_PLAYER_MODEL    256

typedef struct scrim_timeout_s
{
  int start_time;
  int warmup1Time;
  int warmup2Time;
} scrim_timout_t;

typedef struct
{
  struct gclient_s  *clients;   // [maxclients]

  struct gentity_s  *gentities;
  int               gentitySize;
  int               num_entities;   // MAX_CLIENTS <= num_entities <= ENTITYNUM_MAX_NORMAL

  int               countdownTime;     // restart match at this time
  qboolean          fight;

  fileHandle_t      logFile;

  // store latched cvars here that we want to get at often
  int               maxclients;

  int               framenum;
  int               time;                         // in msec
  int               previousTime;                 // so movers can back up when blocked
  int               frameMsec;                    // Sys_Milliseconds() at end frame

  int               startTime;                    // level.time the map was started

  int               expireTimes[ NUM_EXPIRE_TYPES ]; // used by G_Expired()

  int               extendTimeLimit;              // set the time limit to level.matchBaseTimeLimit + this value
  int               extendVoteCount;
  int               matchBaseTimeLimit;
  qboolean          timeLimitInitialized;

  int               teamScores[ NUM_TEAMS ];
  int               lastTeamLocationTime;         // last time of client team location update

  qboolean          newSession;                   // don't use any old session data, because
                                                  // we changed gametype

  qboolean          restarted;                    // waiting for a map_restart to fire

  scrim_t           scrim;
  scrim_team_roster_t scrim_team_rosters[NUM_SCRIM_TEAMS];
  scrim_timout_t    scrim_timeout;
  qboolean          scrim_reset_time_on_restart;

  int               numConnectedClients;
  int               numNonSpectatorClients;       // includes connecting clients
  int               numPlayingClients;            // connected, non-spectators
  int               sortedClients[MAX_CLIENTS];   // sorted by score

  int               snd_fry;                      // sound index for standing in lava

  int               countdownModificationCount;      // for detecting if g_countdown is changed

  // warmup/ready state
  int               readyToPlay[ NUM_TEAMS ];
  int               warmup1Time;                  // 3 minute warmup timeout
  int               warmup2Time;                  // 1 minute warmup timeout

  // voting state
  int               voteThreshold[ NUM_TEAMS ];   // need at least this percent to pass
  char              voteString[ NUM_TEAMS ][ MAX_STRING_CHARS ];
  char              voteDisplayString[ NUM_TEAMS ][ MAX_STRING_CHARS ];
  int               voteTime[ NUM_TEAMS ];        // level.time vote was called
  int               voteExecuteTime[ NUM_TEAMS ]; // time the vote is executed
  int               voteDelay[ NUM_TEAMS ];       // it doesn't make sense to always delay vote execution
  int               voteYes[ NUM_TEAMS ];
  int               voteNo[ NUM_TEAMS ];
  gclient_t         *voteCaller[ NUM_TEAMS ];     // client that called the vote
  int               numVotingClients[ NUM_TEAMS ];// set by CalculateRanks
  int               numCountedVotingClients[ NUM_TEAMS ];// The total number of clients considered in vote calculations
  vote_t            voteType[ NUM_TEAMS ];

  // spawn variables
  qboolean          spawning;                     // the G_Spawn*() functions are valid
  int               numSpawnVars;
  char              *spawnVars[ MAX_SPAWN_VARS ][ 2 ];  // key / value pairs
  int               numSpawnVarChars;
  char              spawnVarChars[ MAX_SPAWN_VARS_CHARS ];

  // intermission state
  qboolean          exited;
  int               intermissionQueued;           // intermission was qualified, but
                                                  // wait INTERMISSION_DELAY_TIME before
                                                  // actually going there so the last
                                                  // frag can be watched.  Disable future
                                                  // kills during this delay
  int               intermissiontime;             // time the intermission was started
  char              *changemap;
  int               nextmap_when_empty_teams;
  int               forfeit_when_team_is_empty[NUM_SCRIM_TEAMS];
  qboolean          readyToExit;                  // at least one client wants to exit
  int               exitTime;
  vec3_t            intermission_origin;          // also used for spectator spawns
  vec3_t            intermission_angle;

  gentity_t         *locationHead;                // head of the location list

  int               numAlienSpawns;
  int               numHumanSpawns;
  int               num_buildables[BA_NUM_BUILDABLES];
  qboolean          core_buildable_constructing[NUM_TEAMS];
  int               core_buildable_health[NUM_TEAMS];

  int               numAlienClients;
  int               numHumanClients;

  float             averageNumAlienClients;
  int               numAlienSamples;
  float             averageNumHumanClients;
  int               numHumanSamples;

  int               numAlienClientsAlive;
  int               numHumanClientsAlive;

  int               lastTeamStatus[NUM_TEAMS];

  int               alienBuildPoints;
  int               alienBuildPointQueue;
  int               alienNextQueueTime;
  int               humanBuildPoints;
  int               humanBuildPointQueue;
  int               humanNextQueueTime;

  buildPointZone_t  *buildPointZones;

  gentity_t         *markedBuildables[ MAX_GENTITIES ];
  int               numBuildablesForRemoval;

  int               alienKills;
  int               humanKills;

  qboolean          overmindMuted;

  int               humanBaseAttackTimer;

  team_t            lastWin;

  char              winner_configstring[MAX_STRING_CHARS];

  int               suddenDeathABuildPoints;
  int               suddenDeathHBuildPoints;
  int               suddenDeathBeginTime;
  timeWarning_t     suddenDeathWarning;
  timeWarning_t     timelimitWarning;
  qboolean          sudden_death_replacable[BA_NUM_BUILDABLES];

  bglist_t         spawn_queue[NUM_TEAMS];

  int               alienStage2Time;
  int               alienStage3Time;
  int               humanStage2Time;
  int               humanStage3Time;

  int               alienNextStageThreshold;
  int               humanNextStageThreshold;

  qboolean          uncondAlienWin;
  qboolean          uncondHumanWin;
  qboolean          alienTeamLocked;
  qboolean          humanTeamLocked;
  int               pausedTime;

  char              layout[ MAX_QPATH ];

  team_t            surrenderTeam;
  int               lastTeamImbalancedTime;
  int               numTeamImbalanceWarnings;

  voice_t           *voices;

  emoticon_t        emoticons[ MAX_EMOTICONS ];
  int               emoticonCount;

  char              *playerModel[ MAX_PLAYER_MODEL ];
  int               playerModelCount;

  namelog_t         *namelogs;

  // for use by the server console
  prevRecipients_t  prevRecipients;

  buildLog_t        buildLog[ MAX_BUILDLOG ];
  int               buildId;
  int               numBuildLogs;
  int               lastLayoutReset;

  int               playmapFlags;
  int               epochStartTime;
  char              database_data[ DATABASE_DATA_MAX ];

  struct humanPortals_s
  {
    gentity_t       *portals[PORTAL_NUM];
    int             lifetime[PORTAL_NUM];
    int             createTime[PORTAL_NUM];
  } humanPortals;
} level_locals_t;

#define CMD_CHEAT         0x0001
#define CMD_CHEAT_TEAM    0x0002 // is a cheat when used on a team
#define CMD_MESSAGE       0x0004 // sends message to others (skip when muted)
#define CMD_TEAM          0x0008 // must be on a team
#define CMD_SPEC          0x0010 // must be a spectator
#define CMD_ALIEN         0x0020
#define CMD_HUMAN         0x0040
#define CMD_ALIVE         0x0080
#define CMD_INTERMISSION  0x0100 // valid during intermission

typedef struct
{
  char *cmdName;
  int  cmdFlags;
  void ( *cmdHandler )( gentity_t *ent );
} commands_t;

//
// g_spawn.c
//
qboolean  G_SpawnString( const char *key, const char *defaultString, char **out );
// spawn string returns a temporary reference, you must CopyString() if you want to keep it
qboolean  G_SpawnFloat( const char *key, const char *defaultString, float *out );
qboolean  G_SpawnInt( const char *key, const char *defaultString, int *out );
qboolean  G_SpawnVector( const char *key, const char *defaultString, float *out );
void      G_SpawnEntitiesFromString( void );
char      *G_NewString( const char *string );

//
// g_cmds.c
//
int      G_CheckEvolve( gentity_t *ent, class_t newClass,
                        vec3_t infestOrigin, qboolean give );
void     G_Evolve( gentity_t *ent, class_t newClass,
                   int cost, vec3_t infestOrigin, qboolean force );
qboolean G_EvolveAfterCheck( gentity_t *ent, class_t newClass, qboolean give );

typedef enum
{
  ERR_SELL_NONE,
  ERR_SELL_SPECIFY,
  ERR_SELL_OCCUPY,
  ERR_SELL_NO_ARM,
  ERR_SELL_NO_CHANGE_ALLOWED_NOW,
  ERR_Sell_NOT_PURCHASABLE,
  ERR_SELL_NOT_ITEM_HELD,
  ERR_SELL_ARMOURYBUILDTIMER,
  ERR_SELL_NOROOMBSUITOFF,
  ERR_SELL_UNKNOWNITEM
} sellErr_t;

sellErr_t G_CanSell( gentity_t *ent, const char *itemName, int *value, qboolean force );
void G_TakeItem( gentity_t *ent, const char *itemName, const int value, qboolean force );
int G_CanAutoSell( gentity_t *ent, const char *itemToBuyName,
                        weapon_t *weaponToSell, int *upgradesToSell,
                        int *values, int numValues, sellErr_t *autoSellErrors, qboolean force );
qboolean G_TakeItemAfterCheck(gentity_t *ent, const char *itemName, qboolean force);

typedef enum
{
  ERR_BUY_NONE,
  ERR_BUY_SPECIFY,
  ERR_BUY_OCCUPY,
  ERR_BUY_NO_ENERGY_AMMO,
  ERR_BUY_NO_ARM,
  ERR_BUY_ITEM_HELD,
  ERR_BUY_ALIEN_ITEM,
  ERR_BUY_NOT_PURCHASABLE,
  ERR_BUY_NOT_ALLOWED,
  ERR_BUY_NO_FUNDS,
  ERR_BUY_NO_SLOTS,
  ERR_BUY_NO_CHANGE_ALLOWED_NOW,
  ERR_BUY_UNEXPLODEDGRENADE,
  ERR_BUY_BURST_UNFINISHED,
  ERR_BUY_CHARGING_LIGHTNING,
  ERR_BUY_NO_MORE_AMMO,
  ERR_BUY_NO_JET,
  ERR_BUY_FUEL_FULL,
  ERR_BUY_NOROOMBSUITON,
  ERR_BUY_UNKNOWNITEM
} buyErr_t;

buyErr_t G_CanBuy( gentity_t *ent, const char *itemName, int *price,
                   qboolean *energyOnly, const int slotsFreeFromAutoSell,
                   const int fundsFromAutoSell, qboolean force );
void G_GiveItem( gentity_t *ent, const char *itemName, const int price,
                 const qboolean energyOnly, qboolean force );
qboolean G_GiveItemAfterCheck(
  gentity_t *ent, const char *itemName, qboolean force, qboolean autosell);

#define DECOLOR_OFF '\16'
#define DECOLOR_ON  '\17'

qboolean  G_RoomForClassChange( gentity_t *ent, class_t class, vec3_t newOrigin );
void      G_StopFollowing( gentity_t *ent );
void      G_StopFromFollowing( gentity_t *ent );
void      G_FollowLockView( gentity_t *ent );
qboolean  G_FollowNewClient( gentity_t *ent, int dir );
void      G_ToggleFollow( gentity_t *ent );
int       G_ClientNumberFromString( char *s, char *err, int len );
int       G_ClientNumbersFromString(
  char *s, int *plist, int max, qboolean alphanumeric );
char      *ConcatArgs( int start );
char      *ConcatArgsPrintable( int start );
void      G_Say( gentity_t *ent, saymode_t mode, const char *chatText );
void      G_DecolorString( char *in, char *out, int len );
void      G_UnEscapeString( char *in, char *out, int len );
void      G_SanitiseString( char *in, char *out, int len );
void      Cmd_PrivateMessage_f( gentity_t *ent );
void      Cmd_PlayMap_f( gentity_t *ent );
void      Cmd_ListMaps_f( gentity_t *ent );
void      Cmd_Test_f( gentity_t *ent );
void      Cmd_AdminMessage_f( gentity_t *ent );
int       G_DonateCredits( gclient_t *client, int value, qboolean verbos );
int       G_FloodLimited( gentity_t *ent );
void      G_ListCommands( gentity_t *ent );
void      G_LoadCensors( void );
void      G_CensorString( char *out, const char *in, int len, gentity_t *ent );
void      Cmd_Delag_f( gentity_t *ent );

//
// g_physics.c
//
void G_Physics( gentity_t *ent, int msec );

//
// g_buildable.c
//

#define MAX_ALIEN_BBOX  25

typedef enum
{
  IBE_NONE,

  IBE_NOOVERMIND,
  IBE_ONEOVERMIND,
  IBE_NOALIENBP,
  IBE_SPWNWARN, // not currently used
  IBE_NOCREEP,

  IBE_ONEREACTOR,
  IBE_NOPOWERHERE,
  IBE_TNODEWARN, // not currently used
  IBE_RPTNOREAC,
  IBE_RPTPOWERHERE,
  IBE_NOHUMANBP,
  IBE_NODCC,

  IBE_NORMAL, // too steep
  IBE_NOROOM,
  IBE_PERMISSION,
  IBE_LASTSPAWN,
  IBE_BLOCKEDBYENEMY,
  IBE_SD_UNIQUE,
  IBE_SD_IRREPLACEABLE,

  IBE_MAXERRORS
} itemBuildError_t;

gentity_t         *G_CheckSpawnPoint( int spawnNum, const vec3_t origin,
                                      const vec3_t normal, buildable_t spawn,
                                      vec3_t spawnOrigin );

buildable_t       G_IsPowered( vec3_t origin );
qboolean          G_IsDCCBuilt( void );
int               G_FindDCC( gentity_t *self );
gentity_t         *G_Reactor( void );
gentity_t         *G_Overmind( void );
qboolean          G_FindCreep( gentity_t *self );
gentity_t         *G_FindBuildable( buildable_t buildable );
qboolean          G_BuildableIsUnique(gentity_t *buildable);

qboolean G_FindBuildableInStack( int groundBuildableNum, int stackedBuildableNum, int *index );
void G_AddBuildableToStack( int groundBuildableNum, int stackedBuildableNum );
void G_RemoveBuildableFromStack( int groundBuildableNum, int stackedBuildableNum );
void G_SetBuildableDropper( int removedBuildableNum, int dropperNum );

void              AGeneric_Think( gentity_t *self );
void              ASpawn_Think( gentity_t *self );
void              AOvermind_Think( gentity_t *self );
void              ABarricade_Think( gentity_t *self );
void              AAcidTube_Think( gentity_t *self );
void              AHive_Think( gentity_t *self );
void              ATrapper_Think( gentity_t *self );
void              HSpawn_Think( gentity_t *self );
void              HRepeater_Think( gentity_t *self );
qboolean          HRepeater_CanActivate( gentity_t *self, gclient_t *client );
void              HReactor_Think( gentity_t *self );
void              HArmoury_Think( gentity_t *self );
void              HDCC_Think( gentity_t *self );
void              HMedistat_Think( gentity_t *self );
void              HMGTurret_Think( gentity_t *self );
void              HTeslaGen_Think( gentity_t *self );

void              G_BuildableThink( gentity_t *ent, int msec );
qboolean          G_BuildableRange( vec3_t origin, float r, buildable_t buildable );
void              G_ClearDeconMarks( void );
itemBuildError_t  G_CanBuild( gentity_t *ent, buildable_t buildable, int distance,
                              vec3_t origin, vec3_t normal, int *groundEntNum );
qboolean          G_BuildIfValid( gentity_t *ent, buildable_t buildable );
void              G_SetBuildableAnim( gentity_t *ent, buildableAnimNumber_t anim, qboolean force );
void              G_SetIdleBuildableAnim( gentity_t *ent, buildableAnimNumber_t anim );
void              G_SpawnBuildable(gentity_t *ent, buildable_t buildable);
void              FinishSpawningBuildable( gentity_t *ent );
void              G_LayoutSave( char *name );
int               G_LayoutList( const char *map, char *list, int len );
void              G_LayoutSelect( void );
void              G_LayoutLoad( char *lstr );
void              G_BaseSelfDestruct( team_t team );
int               G_NextQueueTime( int queuedBP, int totalBP, int queueBaseRate );
void              G_QueueBuildPoints( gentity_t *self );
int               G_GetBuildPoints( const vec3_t pos, team_t team );
int               G_GetMarkedBuildPoints( playerState_t *ps );
qboolean          G_FindPower( gentity_t *self, qboolean searchUnspawned );
gentity_t         *G_PowerEntityForPoint( const vec3_t origin );
gentity_t         *G_PowerEntityForEntity( gentity_t *ent );
gentity_t         *G_RepeaterEntityForPoint( vec3_t origin );
gentity_t         *G_InPowerZone( gentity_t *self );
buildLog_t        *G_BuildLogNew( gentity_t *actor, buildFate_t fate );
void              G_BuildLogSet( buildLog_t *log, gentity_t *ent );
void              G_BuildLogAuto( gentity_t *actor, gentity_t *buildable,
                                                    buildFate_t fate );
void              G_BuildLogRevert( int id );
void              G_RemoveRangeMarkerFrom( gentity_t *self );
void              G_UpdateBuildableRangeMarkers( void );

// activation entities functions
qboolean          G_CanActivateEntity( gclient_t *client, gentity_t *ent );
void              G_OvrdActMenuMsg( gentity_t *activator,
                                    actMNOvrdIndex_t index,
                                    dynMenu_t defaultMenu );
qboolean          G_WillActivateEntity( gentity_t *actEnt,
                                        gentity_t *activator );
void              G_ActivateEntity( gentity_t *actEnt, gentity_t *activator );
void              G_ResetOccupation( gentity_t *occupied,
                                     gentity_t *occupant ); // is called to reset
                                       // an occupiable activation entity and
                                       // its occupant.  Serves as a general
                                       //  wrapper for (*activation.reset)()
void              G_UnoccupyEnt( gentity_t *occupied,
                                           gentity_t *occupant,
                                           gentity_t *activator,
                                           qboolean force ); // wrapper called
                                             // for players leaving an
                                             // occupiable activation entity.
void              G_OccupyEnt( gentity_t *occupied );
void              G_SetClipmask(
  gentity_t *ent, int clip_mask_include, int clip_mask_exclude);
void              G_SetContents(
  gentity_t *ent, int contents, qboolean force_current_contents);
void              G_BackupUnoccupyClipmask( gentity_t *ent );
void              G_BackupUnoccupyContents( gentity_t *ent );
void              G_OccupantClip( gentity_t *occupant );
void              G_OccupantThink( gentity_t *occupant );

//
// g_utils.c
//
//addr_t in g_admin.h for g_admin_ban_t
qboolean    G_AddressParse( const char *str, addr_t *addr );
qboolean    G_AddressCompare( const addr_t *a, const addr_t *b );

int         G_ParticleSystemIndex( const char *name );
int         G_ShaderIndex( const char *name );
int         G_ModelIndex( const char *name );
int         G_SoundIndex( const char *name );
void        G_KillBox (gentity_t *ent);
gentity_t   *G_Find (gentity_t *from, int fieldofs, const char *match);
gentity_t   *G_PickTarget (char *targetname);
void        G_UseTargets (gentity_t *ent, gentity_t *activator);
void        G_SetMovedir ( vec3_t angles, vec3_t movedir);

void        G_InitGentity( gentity_t *e );
gentity_t   *G_Spawn( void );
gentity_t   *G_TempEntity( const vec3_t origin, int event );
void        G_Sound( gentity_t *ent, int channel, int soundIndex );
void        G_FreeEntity( gentity_t *e );
void        G_RemoveEntity( gentity_t *ent );
qboolean    G_EntitiesFree( void );
char        *G_CopyString( const char *str );

void        G_TouchTriggers( gentity_t *ent );

char        *vtos( const vec3_t v );

float       vectoyaw( const vec3_t vec );

void        G_AddPredictableEvent( gentity_t *ent, int event, int eventParm );
void        G_AddEvent( gentity_t *ent, int event, int eventParm );
void        G_BroadcastEvent( int event, int eventParm );
void        G_SetOrigin( gentity_t *ent, const vec3_t origin );
void        AddRemap(const char *oldShader, const char *newShader, float timeOffset);
const char  *BuildShaderStateConfig( void );


qboolean    G_ClientIsLagging( gclient_t *client );

void        G_TriggerMenu( int clientNum, dynMenu_t menu );
void        G_TriggerMenuArgs( int clientNum, dynMenu_t menu, int arg );
void        G_CloseMenus( int clientNum );

qboolean    G_Visible(
  gentity_t *ent1, gentity_t *ent2, const content_mask_t content_mask);
qboolean    G_BBOXes_Visible(
  int source_num,
  const vec3_t source_origin, const vec3_t source_mins, const vec3_t source_maxs,
  int destination_num,
  const vec3_t dest_origin, const vec3_t dest_mins, const vec3_t dest_maxs,
  const content_mask_t contents_mask);
gentity_t   *G_ClosestEnt( vec3_t origin, gentity_t **entities, int numEntities );

typedef enum {
  LOG_ACT_TOUCH,
  LOG_ACT_ACTIVATE,
  LOG_ACT_USE
} logged_activation_t;
qboolean    G_LoggedActivation(
  gentity_t *self, gentity_t *other, gentity_t *activator, trace_t *trace,
  const char *source, logged_activation_t act_type );

int          G_Get_Foundation_Ent_Num(gentity_t *ent);

void        G_Entity_UEID_init(gentity_t *ent);
void        G_Entity_UEID_set(bgentity_id *ueid,gentity_t *target);
gentity_t   *G_Entity_UEID_get(bgentity_id *ueid);

void        G_SetPlayersLinkState( qboolean link, gentity_t *skipPlayer );

qboolean    G_Expired( gentity_t *ent, expire_t index );
void        G_SetExpiration( gentity_t *ent, expire_t index, int expiration );
//
// g_combat.c
//
qboolean  G_IsRecentAgressor(const gentity_t *ent);
qboolean  CanDamage( gentity_t *targ, vec3_t origin,
                     vec3_t mins, vec3_t maxs );
void      G_Damage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
                    vec3_t dir, vec3_t point, int damage, int dflags, int mod );
void      G_SelectiveDamage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir,
                             vec3_t point, int damage, int dflags, int mod, int team );
qboolean  G_RadiusDamage( vec3_t origin, vec3_t originMins, vec3_t originMaxs,
                          gentity_t *attacker, float damage, float radius,
                          gentity_t *ignore, int mod, qboolean knockback );
qboolean  G_SelectiveRadiusDamage( vec3_t origin, vec3_t originMins, vec3_t originMaxs,
                                   gentity_t *attacker, float damage, float radius,
                                   gentity_t *ignore, int mod, int team, qboolean knockback );
void      G_Knockback( gentity_t *targ, vec3_t dir, int knockback );
qboolean  G_TakesDamage( gentity_t *ent );
float     G_RewardAttackers( gentity_t *self );
void      AddScore( gentity_t *ent, int score );
void      G_LogDestruction( gentity_t *self, gentity_t *actor, int mod );

void      G_InitDamageLocations( void );

// damage flags
#define DAMAGE_RADIUS         0x00000001  // damage was indirect
#define DAMAGE_NO_ARMOR       0x00000002  // armour does not protect from this damage
#define DAMAGE_NO_KNOCKBACK   0x00000004  // do not affect velocity, just view angles
#define DAMAGE_NO_PROTECTION  0x00000008  // kills everything except godmode
#define DAMAGE_NO_LOCDAMAGE   0x00000010  // do not apply locational damage
#define DAMAGE_INSTAGIB       0x00000020  // instally kill the target
#define DAMAGE_GODLESS        0x00000040  // godmode doesn't protect

//
// g_missile.c
//
void      G_RunMissile( gentity_t *ent );
qboolean  G_PlayerHasUnexplodedGrenades( gentity_t *player );

gentity_t *fire_flamer( gentity_t *self, vec3_t start, vec3_t aimdir );
gentity_t *fire_blaster( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *fire_pulseRifle( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *fire_luciferCannon( gentity_t *self, vec3_t start, vec3_t dir,
                               int damage, int radius, int speed );
gentity_t *fire_lockblob( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *fire_paraLockBlob( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *fire_slowBlob( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *fire_bounceBall( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *fire_hive( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *fire_portalGun( gentity_t *self, vec3_t start, vec3_t dir,
                           portal_t portal, qboolean relativeVelocity );
gentity_t *launch_grenade( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *launch_grenade2( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *launch_grenade3( gentity_t *self, vec3_t start, vec3_t dir,
                            qboolean impact );
gentity_t *launch_fragnade( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *launch_lasermine( gentity_t *self, vec3_t start, vec3_t dir );
gentity_t *fire_lightningBall( gentity_t *self, qboolean primary,
                               vec3_t start, vec3_t dir );

//
// g_mover.c
//
void G_ResetPusherNum(void);
void G_RunMover( gentity_t *ent );
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace );
void manualTriggerSpectator( gentity_t *trigger, gentity_t *player );

//
// g_trigger.c
//
extern qboolean g_trigger_success;
void trigger_teleporter_touch( gentity_t *self, gentity_t *other, trace_t *trace );
void G_Checktrigger_stages( team_t team, stage_t stage );


//
// g_misc.c
//
qboolean G_NoTarget( gentity_t *ent );
void     TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles,
                         float speed );

//
// g_weapon.c
//
typedef struct zapTarget_s
{
  gentity_t *targetEnt;
  float     distance;
} zapTarget_t;

typedef struct zap_s
{
  bglink_t      *zapLink;
  gentity_t     *creator;
  bglist_t     targetQueue;

  int           timeToLive;

  gentity_t     *effectChannel;
} zap_t;

extern bglink_t *lev2ZapList;

void      G_PackEntityNumbers( entityState_t *es, int creatorNum,
                               bglist_t *targetQueue );

void      Blow_up( gentity_t *ent );
void      G_ForceWeaponChange( gentity_t *ent, weapon_t weapon );
qboolean  G_CanGiveClientMaxAmmo( gentity_t *ent, qboolean buyingEnergyAmmo,
                                  int *rounds, int *clips, int *price );
void      G_GiveClientMaxAmmo( gentity_t *ent, qboolean buyingEnergyAmmo );
void      SnapVectorTowards( vec3_t v, vec3_t to );
void      G_SplatterFire( gentity_t *inflicter, gentity_t *attacker,
                          vec3_t origin, vec3_t dir,
                          weapon_t weapon, weaponMode_t weaponMode, meansOfDeath_t mod );
qboolean  CheckVenomAttack( gentity_t *ent );
void      CheckGrabAttack( gentity_t *ent );
qboolean  CheckPounceAttack( gentity_t *ent );
void      CheckCkitRepair( gentity_t *ent );
void      G_ChargeAttack( gentity_t *ent, gentity_t *victim );
void      G_CrushAttack( gentity_t *ent, gentity_t *victim );
void      G_DeleteZapData( void *data );
bglink_t  *G_FindZapLinkFromEffectChannel( const gentity_t *effectChannel );
void      G_UpdateZaps( int msec );
void      G_ClearPlayerZapEffects( gentity_t *player );


//
// g_client.c
//
void      G_AddCreditToClient( gclient_t *client, short credit, qboolean cap );
void      G_SetClientViewAngle( gentity_t *ent, const vec3_t angle );
gentity_t *G_SelectTremulousSpawnPoint( team_t team, vec3_t preference, vec3_t origin, vec3_t angles );
gentity_t *G_SelectSpawnPoint( vec3_t avoidPoint, vec3_t origin, vec3_t angles );
gentity_t *G_SelectAlienLockSpawnPoint( vec3_t origin, vec3_t angles );
gentity_t *G_SelectHumanLockSpawnPoint( vec3_t origin, vec3_t angles );
void      respawn( gentity_t *ent );
void      BeginIntermission( void );
void      ClientSpawn( gentity_t *ent, gentity_t *spawn, const vec3_t origin, const vec3_t angles, qboolean check_exit_rules );
void      body_die( gentity_t *self, gentity_t*, gentity_t*, int, int );
void      player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod );
qboolean  SpotWouldTelefrag( gentity_t *spot );
char      *GetSkin( char *modelname, char *wish );
qboolean  G_IsNewbieName( const char *name );
qboolean  G_Client_Alive( gentity_t *ent );
team_t    G_Client_Team( gentity_t *ent );
void      G_Client_For_All( void (*func)( gentity_t *ent ) );

//
// g_svcmds.c
//
qboolean  ConsoleCommand( void );
void      G_RegisterCommands( void );
void      G_UnregisterCommands( void );

//
// g_weapon.c
//
void FireWeapon( gentity_t *ent );
void FireWeapon2( gentity_t *ent );
void FireWeapon3( gentity_t *ent );

//
// g_scrim.c
//
void                G_Scrim_Restore_Player_Rank(int client_num);
qboolean G_Scrim_Set_Team_Captain(
  scrim_team_t scrim_team, const int roster_id, qboolean *removed_captain,
  char *err, int err_len);
void                G_Scrim_Load(void);
void                G_Scrim_Reset_Settings(void);
void                G_Scrim_Save(void);
void                G_Scrim_Send_Status(void);
void                G_Scrim_Broadcast_Status(void);
void                G_Scrim_Check_Win_Conditions(void);
void                G_Scrim_Check_Pause(void);
void                G_Scrim_Remove_Player_From_Rosters(namelog_t *namelog,
  qboolean force_ip);
void                G_Scrim_Remove_Player_From_Rosters_By_ID(size_t roster_id);
qboolean            G_Scrim_Add_Player_To_Roster(
  gclient_t *client, scrim_team_t scrim_team, char *err, int err_len);
qboolean G_Scrim_Add_Namelog_To_Roster(
  namelog_t *namelog, scrim_team_t scrim_team, char *err, int err_len);
scrim_team_t        G_Scrim_Find_Player_In_Rosters(gclient_t *client,
  int *roster_index);
scrim_team_member_t *G_Scrim_Roster_Member_From_String(
  char *s, scrim_team_t *scrim_team, char *err, int len);
void                G_Scrim_Player_Netname_Updated(gclient_t *client);
void                G_Scrim_Player_Set_Registered_Name(const char *guid,
  const char *name);
void                G_Scrim_Player_Refresh_Registered_Names( void );

//
// g_main.c
//
void     ScoreboardMessage( gentity_t *client );
void     G_ReplacableBuildablesMessage( gentity_t *ent );
void     MoveClientToIntermission( gentity_t *client );
void     G_MapConfigs( const char *mapname );
void     CalculateRanks( qboolean check_exit_rules );
void     FindIntermissionPoint( void );
void     G_CountBuildables( void );
void     G_RunThink( gentity_t *ent );
void     G_AdminMessage( gentity_t *ent, const char *string );
void     QDECL G_LogPrintf( const char *fmt, ... ) __attribute__ ((format (printf, 1, 2)));
void     SendScoreboardMessageToAllClients( void );
int      G_Time_Limit(void);
void     G_LevelRestart( qboolean stopWarmup );
void     G_LevelReady( void );
void     G_Vote( gentity_t *ent, team_t team, qboolean voting );
void     G_ExecuteVote( team_t team );
void     G_EndVote( team_t team, qboolean cancel );
void     G_CheckVote( team_t team );
void     LogExit( const char *string );
int      G_TimeTilSuddenDeath( void );
sdmode_t G_SD_Mode(void);
char     *G_SuddenDeathModeString( void );

//
// g_client.c
//
char *ClientConnect( int clientNum, qboolean firstTime );
char *ClientUserinfoChanged( int clientNum, qboolean forceName );
void ClientDisconnect( int clientNum );
void ClientBegin( int clientNum );
void ClientCommand( int clientNum );

//
// g_active.c
//
void VoterInactivityTimer( gentity_t *ent );
void ClientThink( int clientNum );
void ClientEndFrame( gentity_t *ent );
void G_RunClient( gentity_t *ent );

//
// g_unlagged.c
//
void G_Init_Unlagged(void);
void G_Unlagged_Memory_Info( void );
void G_Unlagged_Prepare_Store(void);
void G_Unlagged_Link_To_Store_Data(gentity_t *ent, qboolean dims, qboolean pos, qboolean apos, qboolean origin);
void G_UnlaggedStore( void );
void G_UnlaggedClear( gentity_t *ent );
void G_UnlaggedCalc(int time, gentity_t *rewindEnt);
void G_UnlaggedOn(unlagged_attacker_data_t *attacker_data);
void G_UnlaggedOff( void );
void G_UnlaggedDetectCollisions(gentity_t *ent);
void G_GetUnlaggedOrigin(gentity_t *ent, vec3_t origin);
void G_GetUnlaggedAngles(gentity_t *ent, vec3_t angles);
void G_GetUnlaggedDimensions(gentity_t *ent, vec3_t mins, vec3_t maxs);
void G_DisableUnlaggedCalc(gentity_t *ent);

//
// g_team.c
//
team_t    G_TeamFromString( char *str );
void      G_TeamCommand( team_t team, char *cmd );
qboolean  OnSameTeam(const gentity_t *ent1, const gentity_t *ent2);
void      G_LeaveTeam( gentity_t *self );
void      G_ChangeTeam( gentity_t *ent, team_t newTeam );
gentity_t *Team_GetLocation( gentity_t *ent );
void      TeamplayInfoMessage( gentity_t *ent );
void      CheckTeamStatus( void );
void      G_UpdateTeamConfigStrings( void );

//
// g_session.c
//
void G_ReadSessionData( gclient_t *client );
void G_InitSessionData( gclient_t *client, char *userinfo );
void G_WriteSessionData( void );

//
// g_maprotation.c
//
void      G_PrintRotations( void );
void      G_AdvanceMapRotation( int depth );
qboolean  G_StartMapRotation( char *name, qboolean advance,
                              qboolean putOnStack, qboolean reset_index, int depth );
void      G_StopMapRotation( void );
qboolean  G_MapRotationActive( void );
void      G_InitMapRotations( void );
void      G_ShutdownMapRotations( void );
qboolean  G_MapExists( const char *name );
qboolean  G_LayoutExists( const char *map, const char *layout );
void      G_ClearRotationStack( void );

//
// g_namelog.c
//

void G_namelog_connect( gclient_t *client );
void G_namelog_disconnect( gclient_t *client );
void G_namelog_restore( gclient_t *client );
void G_namelog_update_score( gclient_t *client );
void G_namelog_update_name( gclient_t *client );
void G_namelog_cleanup( void );

//
// g_playermodel.c
//
void G_InitPlayerModel(void);
void G_FreePlayerModel(void);
qboolean G_IsValidPlayerModel(const char *model);
void G_GetPlayerModelSkins( const char *modelname, char skins[MAX_PLAYER_MODEL][ 64 ], int maxskins, int *numskins );
char *GetSkin( char *modelname, char *wish );

//
// g_portal.c
//
void G_Portal_Create( gentity_t *ent, vec3_t origin, vec3_t normal, portal_t portal );
void G_Portal_Clear( portal_t portalIndex );

//some maxs
#define MAX_FILEPATH      144

extern  level_locals_t  level;
extern  gentity_t       g_entities[ MAX_GENTITIES ];

#define FOFS(x) ((size_t)&(((gentity_t *)0)->x))

extern  vmCvar_t  g_dedicated;
extern  vmCvar_t  g_cheats;
extern  vmCvar_t  g_maxclients;     // allow this many total, including spectators
extern  vmCvar_t  g_maxGameClients;   // allow this many active
extern  vmCvar_t  g_restarted;
extern  vmCvar_t  g_restartingFlags; // flags for restarting the map
extern  vmCvar_t  g_lockTeamsAtStart;
extern  vmCvar_t  g_minNameChangePeriod;
extern  vmCvar_t  g_maxNameChanges;

extern  vmCvar_t  g_timelimit;
extern  vmCvar_t  g_basetimelimit;  // this is for resetting the time limit after an extended match
extern  vmCvar_t  g_extendVotesPercent;
extern  vmCvar_t  g_extendVotesTime;
extern  vmCvar_t  g_extendVotesCount;
extern  vmCvar_t  g_suddenDeathTime;
extern  vmCvar_t  g_suddenDeathMode;

extern  vmCvar_t  g_allowScrims;
extern  vmCvar_t  g_scrimMaxPauseTime;

#define IS_SCRIM  (g_allowScrims.integer && level.scrim.mode)

extern  vmCvar_t  g_doWarmup;
extern  vmCvar_t  g_warmupTimers;
extern  vmCvar_t  g_warmup;
extern  vmCvar_t  g_doWarmupCountdown;
extern  vmCvar_t  g_warmupReadyThreshold;
extern  vmCvar_t  g_warmupTimeout1;
extern  vmCvar_t  g_warmupTimeout1Trigger;
extern  vmCvar_t  g_warmupTimeout2;
extern  vmCvar_t  g_warmupTimeout2Trigger;
extern  vmCvar_t  g_warmupBlockEnemyBuilding;
extern  vmCvar_t  g_warmupFriendlyBuildableFire;
extern  vmCvar_t  g_nextMapStartedMatchWhenEmptyTeams;

#define IS_WARMUP  ( g_doWarmup.integer && g_warmup.integer )

extern  vmCvar_t   g_damageProtection;
extern  vmCvar_t   g_targetProtection;

extern  vmCvar_t  g_humanStaminaMode; // when set to 0, human stamina doesn't drain
extern  vmCvar_t  g_playerAccelMode; // when set to 1, strafe jumping is enabled
extern  vmCvar_t  g_friendlyFire;
extern  vmCvar_t  g_friendlyBuildableFire;
extern  vmCvar_t  g_friendlyFireLastSpawnProtection;
extern  vmCvar_t  g_dretchPunt;
extern  vmCvar_t  g_game_mode;
extern  vmCvar_t  g_logPrivateMessages;
extern  vmCvar_t  g_password;
extern  vmCvar_t  g_needpass;
extern  vmCvar_t  g_autoGhost;
extern  vmCvar_t  g_gravity;
extern  vmCvar_t  g_speed;
extern  vmCvar_t  g_knockback;
extern  vmCvar_t  g_inactivity;
extern  vmCvar_t  g_impliedVoting;
extern  vmCvar_t  g_debugMove;
extern  vmCvar_t  g_debugDamage;
extern  vmCvar_t  g_debugPlayMap;
extern  vmCvar_t  g_synchronousClients;
extern  vmCvar_t  g_motd;
extern  vmCvar_t  g_countdown;
extern  vmCvar_t  g_doCountdown;
extern  vmCvar_t  g_humanSpawnCountdown;
extern  vmCvar_t  g_alienSpawnCountdown;
extern  vmCvar_t  g_allowVote;
extern  vmCvar_t  g_voteLimit;
extern  vmCvar_t  g_suddenDeathVotePercent;
extern  vmCvar_t  g_suddenDeathVoteDelay;
extern  vmCvar_t  g_intermissionReadyPercent;
extern  vmCvar_t  g_teamForceBalance;
extern  vmCvar_t  g_smoothClients;
extern  vmCvar_t  pmove_fixed;
extern  vmCvar_t  pmove_msec;

extern  vmCvar_t  g_AMPStageLock;
extern  vmCvar_t  g_debugAMP;

extern  vmCvar_t  g_allowShare;
extern  vmCvar_t  g_overflowFunds;

extern  vmCvar_t  g_allowBuildableStacking;
extern  vmCvar_t  g_alienBuildPoints;
extern  vmCvar_t  g_alienBuildQueueTime;
extern  vmCvar_t  g_humanBlackout;
extern  vmCvar_t  g_humanBuildPoints;
extern  vmCvar_t  g_humanBuildQueueTime;
extern  vmCvar_t  g_humanRepeaterBuildPoints;
extern  vmCvar_t  g_humanRepeaterBuildQueueTime;
extern  vmCvar_t  g_humanRepeaterMaxZones;
extern  vmCvar_t  g_humanStage;
extern  vmCvar_t  g_humanCredits;
extern  vmCvar_t  g_humanMaxStage;
extern  vmCvar_t  g_humanStage2Threshold;
extern  vmCvar_t  g_humanStage3Threshold;
extern  vmCvar_t  g_alienStage;
extern  vmCvar_t  g_alienCredits;
extern  vmCvar_t  g_alienMaxStage;
extern  vmCvar_t  g_alienStage2Threshold;
extern  vmCvar_t  g_alienStage3Threshold;
extern  vmCvar_t  g_teamImbalanceWarnings;
extern  vmCvar_t  g_freeFundPeriod;
extern  vmCvar_t  g_nadeSpamProtection;

extern  vmCvar_t  g_unlagged;

extern  vmCvar_t  g_disabledEquipment;
extern  vmCvar_t  g_disabledClasses;
extern  vmCvar_t  g_disabledBuildables;

extern  vmCvar_t  g_markDeconstruct;

extern  vmCvar_t  g_debugMapRotation;
extern  vmCvar_t  g_currentMapRotation;
extern  vmCvar_t  g_mapRotationNodes;
extern  vmCvar_t  g_mapRotationStack;
extern  vmCvar_t  g_nextMap;
extern  vmCvar_t  g_initialMapRotation;
extern  vmCvar_t  g_sayAreaRange;

extern  vmCvar_t  g_debugVoices;
extern  vmCvar_t  g_voiceChats;

extern  vmCvar_t  g_floodMaxDemerits;
extern  vmCvar_t  g_floodMinTime;

extern  vmCvar_t  g_shove;
extern  vmCvar_t  g_antiSpawnBlock;

extern  vmCvar_t  g_mapConfigs;

extern  vmCvar_t  g_nextLayout;
extern  vmCvar_t  g_layouts[ 9 ];
extern  vmCvar_t  g_layoutAuto;

extern  vmCvar_t  g_emoticonsAllowedInNames;
extern  vmCvar_t  g_newbieNameNumbering;
extern  vmCvar_t  g_newbieNamePrefix;

extern  vmCvar_t  g_admin;
extern  vmCvar_t  g_adminTempBan;
extern  vmCvar_t  g_adminMaxBan;
extern  vmCvar_t  g_adminTempSpec;

extern	vmCvar_t  g_playMapEnable;
extern  vmCvar_t  g_playMapPoolConfig;

extern  vmCvar_t  g_privateMessages;
extern  vmCvar_t  g_specChat;
extern  vmCvar_t  g_publicAdminMessages;
extern  vmCvar_t  g_allowTeamOverlay;
extern  vmCvar_t  g_teamStatus;

extern  vmCvar_t  g_censorship;

void      Com_Printf( const char *msg, ... );
void      Com_Error( int level, const char *error, ... );
int       Sys_Milliseconds( void );
int       Com_RealTime( qtime_t *qtime );
int       Cmd_Argc( void );
void      Cmd_ArgvBuffer( int n, char *buffer, int bufferLength );
int       FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode );
int       FS_Read2( void *buffer, int len, fileHandle_t f );
void      FS_Write( const void *buffer, int len, fileHandle_t f );
void      FS_FCloseFile( fileHandle_t f );
int       FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
int       FS_GetFilteredFiles( const char *path, const char *extension, char *filter, char *listbuf, int bufsize );
int       FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t
void      Cbuf_ExecuteText( int exec_when, const char *text );
void      Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags );
void      Cvar_Update( vmCvar_t *cvar );
void      Cvar_SetSafe( const char *var_name, const char *value );
void 	    Cvar_ForceReset(const char *var_name);
int       Cvar_VariableIntegerValue( const char *var_name );
void      Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
void      SV_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
                               playerState_t *gameClients, int sizeofGameClient );
void      SV_GameDropClient( int clientNum, const char *reason );
void      SV_GameSendServerCommand( int clientNum, const char *text );
void      SV_SendClientGameState2( int clientNum );
void      SV_PlayMap_Save_Queue_Entry( playMap_t pm, int index );
void      SV_PlayMap_Clear_Saved_Queue( int default_flags );
playMap_t SV_PlayMap_Get_Queue_Entry( int index );
void      SV_Scrim_Init(void);
void      SV_Scrim_Save(pers_scrim_t *scrim_input);
void      SV_Scrim_Load(pers_scrim_t *scrim_input);
size_t    SV_Scrim_Get_New_Roster_ID(void);
size_t    SV_Scrim_Get_Last_Roster_ID(void);
void      SV_SetConfigstring( int num, const char *string );
void      SV_GetConfigstring( int num, char *buffer, int bufferSize );
void      SV_SetConfigstringRestrictions( int num, const clientList_t *clientList );
void      SV_GetUserinfo( int num, char *buffer, int bufferSize );
void      SV_SetUserinfo( int num, const char *buffer );
void      SV_GetServerinfo( char *buffer, int bufferSize );
void      SV_SetBrushModel( gentity_t *ent, const char *name );
void      SV_ClipToEntity(
  trace_t *trace, const vec3_t start, vec3_t mins, vec3_t maxs,
  const vec3_t end, int entityNum, const content_mask_t content_mask,
  traceType_t type);
void      SV_Trace(
	trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs,
	const vec3_t end, int passEntityNum, const content_mask_t content_mask,
	traceType_t type);
void      SV_ClipToTestArea(
	trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs,
	const vec3_t end, const vec3_t test_mins, const vec3_t test_maxs,
	const vec3_t test_origin, int test_contents, const content_mask_t content_mask,
	traceType_t type);
int       SV_PointContents( const vec3_t point, int passEntityNum );
qboolean  SV_inPVS( const vec3_t p1, const vec3_t p2 );
qboolean  SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 );
void      SV_AdjustAreaPortalState( gentity_t *ent, qboolean open );
qboolean  CM_AreasConnected( int area1, int area2 );
void      SV_LinkEntity( gentity_t *ent );
void      SV_UnlinkEntity( gentity_t *ent );
int       SV_AreaEntities(
	const vec3_t mins, const vec3_t maxs, const content_mask_t *content_mask,
	int *entityList, int maxcount);
qboolean  SV_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent, traceType_t type );
void      SV_GetUsercmd( int clientNum, usercmd_t *cmd );
qboolean  SV_GetEntityToken( char *buffer, int bufferSize );

void      Q_SnapVector( float *v );

typedef void (*xcommand_t) (void);
void      Cmd_AddCommand( const char *cmdName, xcommand_t function );
void      Cmd_RemoveCommand( const char *cmdName );

int       sl_query( dbArray_t type, char *data, int *steps );
