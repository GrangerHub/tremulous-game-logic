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

level_locals_t  level;

typedef struct
{
  vmCvar_t  *vmCvar;
  char      *cvarName;
  char      *defaultString;
  int       cvarFlags;
  int       modificationCount; // for tracking changes
  qboolean  trackChange;       // track this variable, and announce if changed
  /* certain cvars can be set in worldspawn, but we don't want those values to
     persist, so keep track of non-worldspawn changes and restore that on map
     end. unfortunately, if the server crashes, the value set in worldspawn may
     persist */
  char      *explicit;
} cvarTable_t;

gentity_t   g_entities[ MAX_GENTITIES ];
gclient_t   g_clients[ MAX_CLIENTS ];

vmCvar_t  g_timelimit;
vmCvar_t  g_basetimelimit;
vmCvar_t  g_extendVotesPercent;
vmCvar_t  g_extendVotesTime;
vmCvar_t  g_extendVotesCount;
vmCvar_t  g_suddenDeathTime;
vmCvar_t  g_suddenDeathMode;

vmCvar_t  g_allowScrims;
vmCvar_t  g_scrimMaxPauseTime;

vmCvar_t  g_warmup;
vmCvar_t  g_warmupTimers;
vmCvar_t  g_doWarmup;
vmCvar_t  g_doWarmupCountdown;
vmCvar_t  g_warmupReadyThreshold;
vmCvar_t  g_warmupTimeout1;
vmCvar_t  g_warmupTimeout1Trigger;
vmCvar_t  g_warmupTimeout2;
vmCvar_t  g_warmupTimeout2Trigger;
vmCvar_t  g_warmupBlockEnemyBuilding;
vmCvar_t  g_warmupFriendlyBuildableFire;
vmCvar_t  g_nextMapStartedMatchWhenEmptyTeams;

vmCvar_t  g_damageProtection;
vmCvar_t  g_targetProtection;

vmCvar_t  g_humanStaminaMode;
vmCvar_t  g_playerAccelMode;
vmCvar_t  g_friendlyFire;
vmCvar_t  g_friendlyBuildableFire;
vmCvar_t  g_friendlyFireLastSpawnProtection;
vmCvar_t  g_dretchPunt;
vmCvar_t  g_password;
vmCvar_t  g_needpass;
vmCvar_t  g_autoGhost;
vmCvar_t  g_maxclients;
vmCvar_t  g_maxGameClients;
vmCvar_t  g_dedicated;
vmCvar_t  g_speed;
vmCvar_t  g_gravity;
vmCvar_t  g_cheats;
vmCvar_t  g_knockback;
vmCvar_t  g_inactivity;
vmCvar_t  g_impliedVoting;
vmCvar_t  g_debugMove;
vmCvar_t  g_debugDamage;
vmCvar_t  g_debugPlayMap;
vmCvar_t  g_motd;
vmCvar_t  g_synchronousClients;
vmCvar_t  g_countdown;
vmCvar_t  g_doCountdown;
vmCvar_t  g_humanSpawnCountdown;
vmCvar_t  g_alienSpawnCountdown;
vmCvar_t  g_restarted;
vmCvar_t  g_restartingFlags;
vmCvar_t  g_lockTeamsAtStart;
vmCvar_t  g_logFile;
vmCvar_t  g_logFileSync;
vmCvar_t  g_allowVote;
vmCvar_t  g_voteLimit;
vmCvar_t  g_suddenDeathVotePercent;
vmCvar_t  g_suddenDeathVoteDelay;
vmCvar_t  g_intermissionReadyPercent;
vmCvar_t  g_teamForceBalance;
vmCvar_t  g_smoothClients;
vmCvar_t  pmove_fixed;
vmCvar_t  pmove_msec;
vmCvar_t  g_minNameChangePeriod;
vmCvar_t  g_maxNameChanges;

vmCvar_t  g_allowShare;
vmCvar_t  g_overflowFunds;

vmCvar_t  g_AMPStageLock;
vmCvar_t  g_debugAMP;

vmCvar_t  g_allowBuildableStacking;
vmCvar_t  g_alienBuildPoints;
vmCvar_t  g_alienBuildPointsReserve;
vmCvar_t  g_alienBuildPointsReserveVampireMod;
vmCvar_t  g_alienBuildPointsStageMod0;
vmCvar_t  g_alienBuildPointsStageMod1;
vmCvar_t  g_alienBuildPointsStageMod2;
vmCvar_t  g_alienBuildQueueTime;
vmCvar_t  g_humanBlackout;
vmCvar_t  g_humanBuildPoints;
vmCvar_t  g_humanBuildPointsReserve;
vmCvar_t  g_humanBuildPointsReserveVampireMod;
vmCvar_t  g_humanBuildPointsStageMod0;
vmCvar_t  g_humanBuildPointsStageMod1;
vmCvar_t  g_humanBuildPointsStageMod2;
vmCvar_t  g_humanBuildQueueTime;
vmCvar_t  g_humanRepeaterBuildPoints;
vmCvar_t  g_humanRepeaterBuildQueueTime;
vmCvar_t  g_humanRepeaterMaxZones;
vmCvar_t  g_humanStage;
vmCvar_t  g_humanKills;
vmCvar_t  g_humanCredits;
vmCvar_t  g_humanMaxStage;
vmCvar_t  g_humanStage2Threshold;
vmCvar_t  g_humanStage3Threshold;
vmCvar_t  g_alienStage;
vmCvar_t  g_alienKills;
vmCvar_t  g_alienCredits;
vmCvar_t  g_alienMaxStage;
vmCvar_t  g_alienStage2Threshold;
vmCvar_t  g_alienStage3Threshold;
vmCvar_t  g_teamImbalanceWarnings;
vmCvar_t  g_freeFundPeriod;
vmCvar_t  g_nadeSpamProtection;

vmCvar_t  g_unlagged;

vmCvar_t  g_disabledEquipment;
vmCvar_t  g_disabledClasses;
vmCvar_t  g_disabledBuildables;

vmCvar_t  g_markDeconstruct;

vmCvar_t  g_debugMapRotation;
vmCvar_t  g_currentMapRotation;
vmCvar_t  g_mapRotationNodes;
vmCvar_t  g_mapRotationStack;
vmCvar_t  g_nextMap;
vmCvar_t  g_initialMapRotation;

vmCvar_t  g_debugVoices;
vmCvar_t  g_voiceChats;

vmCvar_t  g_shove;
vmCvar_t  g_antiSpawnBlock;
vmCvar_t  g_lastSpawnFFProtection;

vmCvar_t  g_mapConfigs;
vmCvar_t  g_sayAreaRange;

vmCvar_t  g_floodMaxDemerits;
vmCvar_t  g_floodMinTime;

vmCvar_t  g_nextLayout;
vmCvar_t  g_layouts[ 9 ];
vmCvar_t  g_layoutAuto;

vmCvar_t  g_emoticonsAllowedInNames;
vmCvar_t  g_newbieNameNumbering;
vmCvar_t  g_newbieNamePrefix;

vmCvar_t  g_admin;
vmCvar_t  g_adminTempBan;
vmCvar_t  g_adminMaxBan;
vmCvar_t  g_adminTempSpec;

vmCvar_t  g_playMapEnable;
vmCvar_t  g_playMapPoolConfig;

vmCvar_t  g_privateMessages;
vmCvar_t  g_game_mode;
vmCvar_t  g_logPrivateMessages;
vmCvar_t  g_specChat;
vmCvar_t  g_publicAdminMessages;
vmCvar_t  g_allowTeamOverlay;
vmCvar_t  g_teamStatus;

vmCvar_t  g_censorship;

vmCvar_t  g_tag;


// copy cvars that can be set in worldspawn so they can be restored later
static char cv_gravity[ MAX_CVAR_VALUE_STRING ];
static char cv_humanMaxStage[ MAX_CVAR_VALUE_STRING ];
static char cv_alienMaxStage[ MAX_CVAR_VALUE_STRING ];
static char cv_humanRepeaterBuildPoints[ MAX_CVAR_VALUE_STRING ];
static char cv_humanBuildPoints[ MAX_CVAR_VALUE_STRING ];
static char cv_alienBuildPoints[ MAX_CVAR_VALUE_STRING ];
static char cv_humanBuildPointsReserve[ MAX_CVAR_VALUE_STRING ];
static char cv_alienBuildPointsReserve[ MAX_CVAR_VALUE_STRING ];
static char cv_humanBuildPointsReserveVampireMod[ MAX_CVAR_VALUE_STRING ];
static char cv_alienBuildPointsReserveVampireMod[ MAX_CVAR_VALUE_STRING ];

static cvarTable_t   gameCvarTable[ ] =
{
  // don't override the cheat state set by the system
  { &g_cheats, "sv_cheats", "", 0, 0, qfalse },

  // scrim
  { &g_allowScrims, "g_allowScrims", "0", CVAR_ARCHIVE|CVAR_SERVERINFO, 0, qtrue },
  { &g_scrimMaxPauseTime, "g_scrimMaxPauseTime", "5", CVAR_ARCHIVE, 0, qtrue },

  // warmup
  { &g_warmup, "g_warmup", "1", 0, 0, qfalse },
  { &g_warmupTimers, "g_warmupTimers", "", CVAR_ROM, 0, qfalse },
  { &g_doWarmup, "g_doWarmup", "1", CVAR_ARCHIVE, 0, qtrue  },
  { &g_doWarmupCountdown, "g_doWarmupCountdown", "10", CVAR_ARCHIVE, 0, qtrue  },
  { &g_warmupReadyThreshold, "g_warmupReadyThreshold", "50", CVAR_ARCHIVE, 0,
    qtrue },
  { &g_warmupTimeout1, "g_warmupTimeout1", "300", CVAR_ARCHIVE, 0, qtrue },
  { &g_warmupTimeout1Trigger, "g_warmupTimeout1Trigger", "4", CVAR_ARCHIVE, 0,
    qtrue },
  { &g_warmupTimeout2, "g_warmupTimeout2", "60", CVAR_ARCHIVE, 0, qtrue },
  { &g_warmupTimeout2Trigger, "g_warmupTimeout2Trigger", "66", CVAR_ARCHIVE, 0,
    qtrue },
  { &g_warmupBlockEnemyBuilding,
    "g_warmupBlockEnemyBuilding", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0,
    qtrue },
  { &g_warmupFriendlyBuildableFire,
    "g_warmupFriendlyBuildableFire", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0,
    qtrue },
  { &g_nextMapStartedMatchWhenEmptyTeams,
    "g_nextMapStartedMatchWhenEmptyTeams", "120", CVAR_ARCHIVE, 0, qtrue },

  { &g_damageProtection, "g_damageProtection", "1", CVAR_ARCHIVE, 0, qtrue },
    { &g_targetProtection, "g_targetProtection", "1", CVAR_ARCHIVE, 0, qtrue },

  // noset vars
  { NULL, "gamename", GAME_VERSION , CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
  { &g_restarted, "g_restarted", "0", CVAR_ROM, 0, qfalse  },
  { &g_restartingFlags, "g_restartingFlags", "0", CVAR_ROM, 0, qfalse },
  { &g_lockTeamsAtStart, "g_lockTeamsAtStart", "0", CVAR_ROM, 0, qfalse  },
  { NULL, "sv_mapname", "", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
  { NULL, "P", "", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },

  // latched vars
  { &g_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse  },

  // change anytime vars
  { &g_maxGameClients, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse  },

  { &g_timelimit, "timelimit", "0", CVAR_SERVERINFO | CVAR_NORESTART, 0, qtrue },
  { &g_basetimelimit, "g_basetimelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
  { &g_extendVotesPercent, "g_extendVotesPercent", "50", CVAR_ARCHIVE, 0, qfalse },
  { &g_extendVotesTime, "g_extendVotesTime", "10", CVAR_ARCHIVE, 0, qfalse },
  { &g_extendVotesCount, "g_extendVotesCount", "5", CVAR_ARCHIVE, 0, qfalse },
  { &g_suddenDeathTime, "g_suddenDeathTime", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
  { &g_suddenDeathMode, "g_suddenDeathMode", "2", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART,0, qtrue },

  { &g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, 0, qfalse  },

  { &g_humanStaminaMode, "g_humanStaminaMode", "1", CVAR_ARCHIVE, 0, qtrue  },
  { &g_playerAccelMode, "g_playerAccelMode", "0",CVAR_ARCHIVE, 0, qtrue },
  { &g_friendlyFire, "g_friendlyFire", "75", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },
  { &g_friendlyBuildableFire, "g_friendlyBuildableFire", "100", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },
  { &g_friendlyFireLastSpawnProtection, "g_friendlyFireLastSpawnProtection", "1", CVAR_ARCHIVE, 0, qtrue },
  { &g_dretchPunt, "g_dretchPunt", "1", CVAR_ARCHIVE, 0, qtrue  },

  { &g_intermissionReadyPercent, "g_intermissionReadyPercent", "65", CVAR_ARCHIVE, 0, qtrue },

  { &g_teamForceBalance, "g_teamForceBalance", "0", CVAR_ARCHIVE, 0, qtrue },

  { &g_countdown, "g_countdown", "10", CVAR_ARCHIVE, 0, qtrue  },
  { &g_doCountdown, "g_doCountdown", "0", CVAR_ARCHIVE, 0, qtrue  },
  { &g_humanSpawnCountdown, "g_humanSpawnCountdown", "15", CVAR_ARCHIVE, 0, qtrue },
  { &g_alienSpawnCountdown, "g_alienSpawnCountdown", "8", CVAR_ARCHIVE,0,qtrue },
  { &g_logFile, "g_logFile", "games.log", CVAR_ARCHIVE, 0, qfalse  },
  { &g_logFileSync, "g_logFileSync", "0", CVAR_ARCHIVE, 0, qfalse  },

  { &g_password, "g_password", "", CVAR_USERINFO, 0, qfalse  },

  { &g_needpass, "g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse },

  { &g_autoGhost, "g_autoGhost", "1", CVAR_SERVERINFO, 0, qfalse },

  { &g_dedicated, "dedicated", "0", 0, 0, qfalse  },

  { &g_speed, "g_speed", "320", 0, 0, qtrue  },
  { &g_gravity, "g_gravity", "800", 0, 0, qtrue, cv_gravity },
  { &g_knockback, "g_knockback", "1000", 0, 0, qtrue  },
  { &g_inactivity, "g_inactivity", "180", 0, 0, qtrue },
  { &g_impliedVoting, "g_impliedVoting", "1", CVAR_ARCHIVE, 0, qtrue },
  { &g_debugMove, "g_debugMove", "0", 0, 0, qfalse },
  { &g_debugDamage, "g_debugDamage", "0", 0, 0, qfalse },
  { &g_debugPlayMap, "g_debugPlayMap", "0", 0, 0, qfalse },
  { &g_motd, "g_motd", "", 0, 0, qfalse },

  { &g_allowVote, "g_allowVote", "1", CVAR_ARCHIVE, 0, qfalse },
  { &g_voteLimit, "g_voteLimit", "3", CVAR_ARCHIVE, 0, qfalse },
  { &g_suddenDeathVotePercent, "g_suddenDeathVotePercent", "74", CVAR_ARCHIVE, 0, qfalse },
  { &g_suddenDeathVoteDelay, "g_suddenDeathVoteDelay", "180", CVAR_ARCHIVE, 0, qfalse },
  { &g_minNameChangePeriod, "g_minNameChangePeriod", "5", 0, 0, qfalse},
  { &g_maxNameChanges, "g_maxNameChanges", "5", 0, 0, qfalse},

  { &g_allowShare, "g_allowShare", "0", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qfalse},
  { &g_overflowFunds, "g_overflowFunds", "1", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qfalse},

  { &g_smoothClients, "g_smoothClients", "1", 0, 0, qfalse},
  { &pmove_fixed, "pmove_fixed", "0", CVAR_SYSTEMINFO, 0, qfalse},
  { &pmove_msec, "pmove_msec", "8", CVAR_SYSTEMINFO, 0, qfalse},

  { &g_AMPStageLock, "g_AMPStageLock", "0", 0, 0, qfalse },
  { &g_debugAMP, "g_debugAMP", "0", 0, 0, qfalse },

  { &g_allowBuildableStacking, "g_allowBuildableStacking", "1", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qfalse},
  { &g_alienBuildPoints, "g_alienBuildPoints", DEFAULT_ALIEN_BUILDPOINTS, 0, 0, qfalse, cv_alienBuildPoints },
  { &g_alienBuildPointsReserve, "g_alienBuildPointsReserve", DEFAULT_ALIEN_BUILDPOINTS_RESERVE, 0, 0, qfalse, cv_alienBuildPointsReserve },
  { &g_alienBuildPointsReserveVampireMod, "g_alienBuildPointsReserveVampireMod", "0.0", 0, 0, qfalse, cv_alienBuildPointsReserveVampireMod },
  { &g_alienBuildPointsStageMod0, "g_alienBuildPointsStageMod0", "0.75", 0, 0, qfalse },
  { &g_alienBuildPointsStageMod1, "g_alienBuildPointsStageMod1", "1.0", 0, 0, qfalse },
  { &g_alienBuildPointsStageMod2, "g_alienBuildPointsStageMod2", "1.25", 0, 0, qfalse },
  { &g_alienBuildQueueTime, "g_alienBuildQueueTime", DEFAULT_ALIEN_QUEUE_TIME, CVAR_ARCHIVE, 0, qfalse  },
  { &g_humanBlackout, "g_humanBlackout", "1", CVAR_SERVERINFO, 0, qfalse  },
  { &g_humanBuildPoints, "g_humanBuildPoints", DEFAULT_HUMAN_BUILDPOINTS, 0, 0, qfalse, cv_humanBuildPoints },
  { &g_humanBuildPointsReserve, "g_humanBuildPointsReserve", DEFAULT_HUMAN_BUILDPOINTS_RESERVE, 0, 0, qfalse, cv_humanBuildPointsReserve },
  { &g_humanBuildPointsReserveVampireMod, "g_humanBuildPointsReserveVampireMod", "0.0", 0, 0, qfalse, cv_humanBuildPointsReserveVampireMod },
  { &g_humanBuildPointsStageMod0, "g_humanBuildPointsStageMod0", "0.75", 0, 0, qfalse },
  { &g_humanBuildPointsStageMod1, "g_humanBuildPointsStageMod1", "1.0", 0, 0, qfalse },
  { &g_humanBuildPointsStageMod2, "g_humanBuildPointsStageMod2", "1.25", 0, 0, qfalse },
  { &g_humanBuildQueueTime, "g_humanBuildQueueTime", DEFAULT_HUMAN_QUEUE_TIME, CVAR_ARCHIVE, 0, qfalse  },
  { &g_humanRepeaterBuildPoints, "g_humanRepeaterBuildPoints", DEFAULT_HUMAN_REPEATER_BUILDPOINTS, CVAR_ARCHIVE, 0, qfalse, cv_humanRepeaterBuildPoints },
  { &g_humanRepeaterMaxZones, "g_humanRepeaterMaxZones", DEFAULT_HUMAN_REPEATER_MAX_ZONES, CVAR_ARCHIVE, 0, qfalse  },
  { &g_humanRepeaterBuildQueueTime, "g_humanRepeaterBuildQueueTime", DEFAULT_HUMAN_REPEATER_QUEUE_TIME, CVAR_ARCHIVE, 0, qfalse  },
  { &g_humanStage, "g_humanStage", "0", 0, 0, qfalse  },
  { &g_humanKills, "g_humanKills", "0", 0, 0, qfalse  },
  { &g_humanCredits, "g_humanCredits", "0", 0, 0, qfalse  },
  { &g_humanMaxStage, "g_humanMaxStage", DEFAULT_HUMAN_MAX_STAGE, 0, 0, qfalse, cv_humanMaxStage },
  { &g_humanStage2Threshold, "g_humanStage2Threshold", DEFAULT_HUMAN_STAGE2_THRESH, 0, 0, qfalse  },
  { &g_humanStage3Threshold, "g_humanStage3Threshold", DEFAULT_HUMAN_STAGE3_THRESH, 0, 0, qfalse  },
  { &g_alienStage, "g_alienStage", "0", 0, 0, qfalse  },
  { &g_alienKills, "g_alienKills", "0", 0, 0, qfalse  },
  { &g_alienCredits, "g_alienCredits", "0", 0, 0, qfalse  },
  { &g_alienMaxStage, "g_alienMaxStage", DEFAULT_ALIEN_MAX_STAGE, 0, 0, qfalse, cv_alienMaxStage },
  { &g_alienStage2Threshold, "g_alienStage2Threshold", DEFAULT_ALIEN_STAGE2_THRESH, 0, 0, qfalse  },
  { &g_alienStage3Threshold, "g_alienStage3Threshold", DEFAULT_ALIEN_STAGE3_THRESH, 0, 0, qfalse  },
  { &g_teamImbalanceWarnings, "g_teamImbalanceWarnings", "30", CVAR_ARCHIVE, 0, qfalse  },
  { &g_freeFundPeriod, "g_freeFundPeriod", DEFAULT_FREEKILL_PERIOD, CVAR_ARCHIVE, 0, qtrue },
  { &g_nadeSpamProtection, "g_nadeSpamProtection", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue },

  { &g_unlagged, "g_unlagged", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },

  { &g_disabledEquipment, "g_disabledEquipment", "", CVAR_ROM | CVAR_SYSTEMINFO, 0, qfalse  },
  { &g_disabledClasses, "g_disabledClasses", "", CVAR_ROM | CVAR_SYSTEMINFO, 0, qfalse  },
  { &g_disabledBuildables, "g_disabledBuildables", "", CVAR_ROM | CVAR_SYSTEMINFO, 0, qfalse  },

  { &g_sayAreaRange, "g_sayAreaRange", "1000", CVAR_ARCHIVE, 0, qtrue  },

  { &g_floodMaxDemerits, "g_floodMaxDemerits", "5000", CVAR_ARCHIVE, 0, qfalse  },
  { &g_floodMinTime, "g_floodMinTime", "2000", CVAR_ARCHIVE, 0, qfalse  },

  { &g_markDeconstruct, "g_markDeconstruct", "3", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },

  { &g_debugMapRotation, "g_debugMapRotation", "0", 0, 0, qfalse  },
  { &g_currentMapRotation, "g_currentMapRotation", "-1", 0, 0, qfalse  }, // -1 = NOT_ROTATING
  { &g_mapRotationNodes, "g_mapRotationNodes", "", CVAR_ROM, 0, qfalse  },
  { &g_mapRotationStack, "g_mapRotationStack", "", CVAR_ROM, 0, qfalse  },
  { &g_nextMap, "g_nextMap", "", 0 , 0, qtrue  },
  { &g_initialMapRotation, "g_initialMapRotation", "", CVAR_ARCHIVE, 0, qfalse  },
  { &g_debugVoices, "g_debugVoices", "0", 0, 0, qfalse  },
  { &g_voiceChats, "g_voiceChats", "1", CVAR_ARCHIVE, 0, qfalse },
  { &g_shove, "g_shove", "0.0", CVAR_ARCHIVE, 0, qfalse  },
  { &g_antiSpawnBlock, "g_antiSpawnBlock", "1", CVAR_ARCHIVE, 0, qtrue  },
  { &g_mapConfigs, "g_mapConfigs", "", CVAR_ARCHIVE, 0, qfalse  },
  { NULL, "g_mapConfigsLoaded", "0", CVAR_ROM, 0, qfalse  },

  { &g_nextLayout, "g_nextLayout", "", 0, 0, qfalse  },
  { &g_layouts[ 0 ], "g_layouts", "", 0, 0, qfalse  },
  { &g_layouts[ 1 ], "g_layouts2", "", 0, 0, qfalse  },
  { &g_layouts[ 2 ], "g_layouts3", "", 0, 0, qfalse  },
  { &g_layouts[ 3 ], "g_layouts4", "", 0, 0, qfalse  },
  { &g_layouts[ 4 ], "g_layouts5", "", 0, 0, qfalse  },
  { &g_layouts[ 5 ], "g_layouts6", "", 0, 0, qfalse  },
  { &g_layouts[ 6 ], "g_layouts7", "", 0, 0, qfalse  },
  { &g_layouts[ 7 ], "g_layouts8", "", 0, 0, qfalse  },
  { &g_layouts[ 8 ], "g_layouts9", "", 0, 0, qfalse  },
  { &g_layoutAuto, "g_layoutAuto", "1", CVAR_ARCHIVE, 0, qfalse  },

  { &g_emoticonsAllowedInNames, "g_emoticonsAllowedInNames", "1", CVAR_LATCH|CVAR_ARCHIVE, 0, qfalse  },
  { &g_newbieNameNumbering, "g_newbieNameNumbering", "1", CVAR_ARCHIVE, 0, qfalse  },
  { &g_newbieNamePrefix, "g_newbieNamePrefix", "UnnamedPlayer#", CVAR_ARCHIVE, 0, qfalse  },

  { &g_admin, "g_admin", "admin.dat", CVAR_ARCHIVE, 0, qfalse  },
  { &g_adminTempBan, "g_adminTempBan", "30m", CVAR_ARCHIVE, 0, qfalse  },
  { &g_adminMaxBan, "g_adminMaxBan", "2w", CVAR_ARCHIVE, 0, qfalse  },
  { &g_adminTempSpec, "g_adminTempSpec", "5m", CVAR_ARCHIVE, 0, qfalse },

  // playmap pool

  { &g_playMapEnable, "g_playMapEnable", "0", CVAR_ARCHIVE, 0, qfalse  },
  { &g_playMapPoolConfig, "g_playMapPoolConfig", "playmap_pool.dat", CVAR_ARCHIVE, 0, qfalse  },

  { &g_game_mode, "g_game_mode", DEFAULT_GAME_MODE, CVAR_SERVERINFO | CVAR_ROM, 0, qtrue },

  { &g_privateMessages, "g_privateMessages", "1", CVAR_ARCHIVE, 0, qfalse  },
  { &g_logPrivateMessages, "g_logPrivateMessages", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue },
  { &g_specChat, "g_specChat", "1", CVAR_ARCHIVE, 0, qfalse  },
  { &g_publicAdminMessages, "g_publicAdminMessages", "1", CVAR_ARCHIVE, 0, qfalse  },
  { &g_allowTeamOverlay, "g_allowTeamOverlay", "1", CVAR_ARCHIVE, 0, qtrue  },
  { &g_teamStatus, "g_teamStatus", "1", CVAR_ARCHIVE, 0, qtrue },

  { &g_censorship, "g_censorship", "", CVAR_ARCHIVE, 0, qfalse  },

  { &g_tag, "g_tag", "gpp", CVAR_INIT, 0, qfalse }
};

static size_t gameCvarTableSize = ARRAY_LEN( gameCvarTable );

void CheckExitRules( void );

void G_CountBuildables( void );
void G_CalculateBuildPoints( void );

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
Q_EXPORT intptr_t vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4,
                                       int arg5, int arg6, int arg7, int arg8, int arg9,
                                       int arg10, int arg11 )
{
  Com_Printf( "WARNING: vmMain should not be used!\n" );
  return -1;
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void )
{
  gentity_t *e, *e2;
  int       i, j;
  int       c, c2;

  c = 0;
  c2 = 0;

  for( i = MAX_CLIENTS, e = g_entities + i; i < level.num_entities; i++, e++ )
  {
    if( !e->team )
      continue;

    if( e->flags & FL_TEAMSLAVE )
      continue;

    e->teammaster = e;
    c++;
    c2++;

    for( j = i + 1, e2 = e + 1; j < level.num_entities; j++, e2++ )
    {
      if( !e2->team )
        continue;

      if( e2->flags & FL_TEAMSLAVE )
        continue;

      if( !strcmp( e->team, e2->team ) )
      {
        c2++;
        e2->teamchain = e->teamchain;
        e->teamchain = e2;
        e2->teammaster = e;
        e2->flags |= FL_TEAMSLAVE;

        // make sure that targets only point at the master
        if( e2->targetname )
        {
          e->multitargetname[ 0 ] = e->targetname = e2->targetname;
          e2->multitargetname[ 0 ] = e2->targetname = NULL;
        }
      }
    }
  }

  Com_Printf( "%i teams with %i entities\n", c, c2 );
}


/*
=================
G_RegisterCvars
=================
*/
void G_RegisterCvars( void )
{
  int         i;
  cvarTable_t *cv;

  for( i = 0, cv = gameCvarTable; i < gameCvarTableSize; i++, cv++ )
  {
    Cvar_Register( cv->vmCvar, cv->cvarName,
      cv->defaultString, cv->cvarFlags );

    if( cv->vmCvar )
      cv->modificationCount = cv->vmCvar->modificationCount;

    if( cv->explicit )
      strcpy( cv->explicit, cv->vmCvar->string );
  }
}

/*
=================
G_UpdateCvars
=================
*/
void G_UpdateCvars( void )
{
  int         i;
  cvarTable_t *cv;

  for( i = 0, cv = gameCvarTable; i < gameCvarTableSize; i++, cv++ )
  {
    if( cv->vmCvar )
    {
      Cvar_Update( cv->vmCvar );

      if( cv->modificationCount != cv->vmCvar->modificationCount )
      {
        cv->modificationCount = cv->vmCvar->modificationCount;

        if( cv->trackChange )
          SV_GameSendServerCommand( -1, va( "print \"Server: %s changed to %s\n\"",
            cv->cvarName, cv->vmCvar->string ) );

        if( !level.spawning && cv->explicit )
          strcpy( cv->explicit, cv->vmCvar->string );
      }
    }
  }
}

/*
=================
G_RestoreCvars
=================
*/
void G_RestoreCvars( void )
{
  int         i;
  cvarTable_t *cv;

  for( i = 0, cv = gameCvarTable; i < gameCvarTableSize; i++, cv++ )
  {
    if( cv->vmCvar && cv->explicit )
      Cvar_SetSafe( cv->cvarName, cv->explicit );
  }
}

/*
=================
G_MapConfigs
=================
*/
void G_MapConfigs(const char *mapname) {
  int mapConfigsLoadedVal;

  if(!g_mapConfigs.string[0]) {
    return;
  }

  mapConfigsLoadedVal = Cvar_VariableIntegerValue("g_mapConfigsLoaded");

  if(mapConfigsLoadedVal < 0 || mapConfigsLoadedVal > 1) {
    return;
  }

  Cbuf_ExecuteText(EXEC_APPEND,
    va("exec \"%s/default.cfg\"\n", g_mapConfigs.string));

  Cbuf_ExecuteText(EXEC_APPEND,
    va("exec \"%s/%s.cfg\"\n", g_mapConfigs.string, mapname));

  Cvar_SetSafe("g_mapConfigsLoaded", va("%d", (mapConfigsLoadedVal + 1)));
}

static int G_StageBuildPointReserveMaxForTeam( team_t team );

/*
============
G_InitGame

============
*/
Q_EXPORT void G_InitGame( int levelTime, int randomSeed, int restart )
{
  bg_collision_funcs_t collision_funcs;
  int                  i;

  srand( randomSeed );

  G_RegisterCvars( );

  Com_Printf( "------- Game Initialization -------\n" );
  Com_Printf( "gamename: %s\n", GAME_VERSION );

  // Dynamic memory
  BG_InitMemory( );

  // set some level globals
  memset( &level, 0, sizeof( level ) );

  sl_query( DB_OPEN, "game.db", NULL );
  sl_query( DB_TIME_GET, level.database_data, NULL );
  unpack_start( level.database_data, DATABASE_DATA_MAX );
  unpack_int( &level.epochStartTime );

  level.time = levelTime;

  // check for restoration of timers related to warmup reset
  if( g_restartingFlags.integer & RESTART_WARMUP_RESET )
  {
    char s[ MAX_STRING_CHARS ];

    Cvar_VariableStringBuffer( "g_warmupTimers", s, sizeof(s) );
    sscanf( s, "%i %i %i",
      &level.startTime,
      &level.warmup1Time,
      &level.warmup2Time );

    Cvar_SetSafe( "g_warmupTimers", "" );
  } else
  {
    Cvar_SetSafe( "g_warmupTimers", "" );

    level.startTime = levelTime;

    // reset the level's 1 minute and 5 minute timeouts
    level.warmup1Time = -1;
    level.warmup2Time = -1;
  }
  level.alienStage2Time = level.alienStage3Time =
    level.humanStage2Time = level.humanStage3Time = level.startTime;

  if(g_nextMapStartedMatchWhenEmptyTeams.integer > 0) {
    level.nextmap_when_empty_teams =
      level.time + (g_nextMapStartedMatchWhenEmptyTeams.integer * 1000);
  }

  // initialize the human portals

  for( i = 0; i < PORTAL_NUM; i++ )
  {
    level.humanPortals.createTime[ i ] = 0;
    level.humanPortals.portals[ i ] = NULL;
    level.humanPortals.lifetime[ i ] = -1;
  }

  // initialize time limit values
  level.matchBaseTimeLimit = g_basetimelimit.integer;
  Cvar_SetSafe( "timelimit", va( "%d", level.matchBaseTimeLimit ) );
  level.extendTimeLimit = 0;
  level.extendVoteCount = 0;
  level.timeLimitInitialized = qtrue;

  level.snd_fry = G_SoundIndex( "sound/misc/fry.wav" ); // FIXME standing in lava / slime

  if( g_logFile.string[ 0 ] )
  {
    if( g_logFileSync.integer )
      FS_FOpenFileByMode( g_logFile.string, &level.logFile, FS_APPEND_SYNC );
    else
      FS_FOpenFileByMode( g_logFile.string, &level.logFile, FS_APPEND );

    if( !level.logFile )
      Com_Printf( "WARNING: Couldn't open logfile: %s\n", g_logFile.string );
    else
    {
      char serverinfo[ MAX_INFO_STRING ];
      qtime_t qt;

      SV_GetServerinfo( serverinfo, sizeof( serverinfo ) );

      G_LogPrintf( "------------------------------------------------------------\n" );
      G_LogPrintf( "InitGame: %s\n", serverinfo );

      Com_RealTime( &qt );
      G_LogPrintf("RealTime: %04i-%02i-%02i %02i:%02i:%02i\n",
            qt.tm_year+1900, qt.tm_mon+1, qt.tm_mday,
            qt.tm_hour, qt.tm_min, qt.tm_sec );

    }
  }
  else
    Com_Printf( "Not logging to disk\n" );

  if(g_mapConfigs.string[0]) {
    if(!Cvar_VariableIntegerValue("g_mapConfigsLoaded")) {
      char map[ MAX_CVAR_VALUE_STRING ] = {""};

      Com_Printf("InitGame: executing map configuration scripts and restarting\n");
      Cvar_VariableStringBuffer("mapname", map, sizeof(map));
      G_MapConfigs(map);
      Cbuf_ExecuteText(EXEC_APPEND, "wait\nmap_restart 0\n");
    } else {
      char map[MAX_CVAR_VALUE_STRING] = {""};

      //execute the map configs one more time without a restart so that commands aren't reset
      Com_Printf("InitGame: executing map configuration scripts\n");
      Cvar_VariableStringBuffer("mapname", map, sizeof(map));
      G_MapConfigs(map);

      // we're done with g_mapConfigs, so reset this for the next map
      Cvar_SetSafe("g_mapConfigsLoaded", "0");
    }
  } else {
    // we're done with g_mapConfigs, so reset this for the next map
    Cvar_SetSafe("g_mapConfigsLoaded", "0");
  }

  // set this cvar to 0 if it exists, but otherwise avoid its creation
  if( Cvar_VariableIntegerValue( "g_rangeMarkerWarningGiven" ) )
    Cvar_SetSafe( "g_rangeMarkerWarningGiven", "0" );

  G_RegisterCommands( );
  G_admin_readconfig( NULL );
  G_LoadCensors( );

  // initialize the build points reserves
  level.alienBuildPointsReserve = G_StageBuildPointReserveMaxForTeam( TEAM_ALIENS );
  level.alienBuildPointsReserveLost = 0;
  level.humanBuildPointsReserve = G_StageBuildPointReserveMaxForTeam( TEAM_HUMANS );
  level.humanBuildPointsReserveLost = 0;

  // initialize all entities for this game
  memset( g_entities, 0, MAX_GENTITIES * sizeof( g_entities[ 0 ] ) );
  level.gentities = g_entities;

  // initialize all clients for this game
  level.maxclients = g_maxclients.integer;
  memset( g_clients, 0, MAX_CLIENTS * sizeof( g_clients[ 0 ] ) );
  level.clients = g_clients;

  // set client fields on player ents
  for( i = 0; i < level.maxclients; i++ )
    g_entities[ i ].client = level.clients + i;

  // always leave room for the max number of clients,
  // even if they aren't all used, so numbers inside that
  // range are NEVER anything but clients
  level.num_entities = MAX_CLIENTS;

  for( i = 0; i < MAX_CLIENTS; i++ )
    g_entities[ i ].classname = "clientslot";

  // let the server system know where the entites are
  SV_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ),
    &level.clients[ 0 ].ps, sizeof( level.clients[ 0 ] ) );

  //init enitity stuff for the bgame
  BG_Init_Entities(ENTITYNUM_NONE);
  for(i = 0; i < MAX_GENTITIES; i++) {
    if(i < MAX_CLIENTS) {
      BG_Locate_Entity_Data(
        i, &g_entities[i].s, &level.clients[i].ps, &g_entities[i].inuse,
        &g_entities[i].r.linked, &g_clients[i].pers.teamSelection);
    } else {
      BG_Locate_Entity_Data(
        i, &g_entities[i].s, NULL, &g_entities[i].inuse,
        &g_entities[i].r.linked, NULL);
    }
  }

  collision_funcs.trace = SV_Trace;
  collision_funcs.pointcontents = SV_PointContents;
  collision_funcs.unlagged_on = G_UnlaggedOn;
  collision_funcs.unlagged_off = G_UnlaggedOff;
  collision_funcs.area_entities = SV_AreaEntities;
  collision_funcs.clip_to_entity  = SV_ClipToEntity;
  collision_funcs.clip_to_test_area = SV_ClipToTestArea;
  BG_Init_Collision_Functions(collision_funcs);

  G_Init_Unlagged( );

  G_Init_Missiles( );

  G_Scrim_Load( );

  level.emoticonCount = BG_LoadEmoticons( level.emoticons, MAX_EMOTICONS );

  SV_SetConfigstring( CS_INTERMISSION, "0" );
  SV_SetConfigstring( CS_WARMUP, va( "%d", IS_WARMUP ) );

  G_InitPlayerModel( );

  // test to see if a custom buildable layout will be loaded
  G_LayoutSelect( );

  // this has to be flipped after the first UpdateCvars
  level.spawning = qtrue;
  // parse the key/value pairs and spawn gentities
  G_SpawnEntitiesFromString( );

  // load up a custom building layout if there is one
  G_LayoutLoad( level.layout );

  // the map might disable some things
  BG_InitAllowedGameElements( );

  // general initialization
  G_FindTeams( );

  BG_Init_Game_Mode(g_game_mode.string);
  G_InitDamageLocations( );
  G_InitMapRotations( );
  G_ReloadPlayMapPool();
  G_ReloadPlayMapQueue();
  G_ExecutePlaymapFlags( level.playmapFlags );

  for(i = 0; i < NUM_TEAMS; i++) {
    BG_List_Init(&level.spawn_queue[i]);
  }

  level.teleporters = NULL;

  if( g_debugMapRotation.integer )
    G_PrintRotations( );

  level.voices = BG_VoiceInit( );
  BG_PrintVoices( level.voices, g_debugVoices.integer );

  //reset stages
  Cvar_SetSafe( "g_alienStage", va( "%d", S1 ) );
  Cvar_SetSafe( "g_humanStage", va( "%d", S1 ) );
  Cvar_SetSafe( "g_alienCredits", 0 );
  Cvar_SetSafe( "g_humanCredits", 0 );
  Cvar_SetSafe( "g_alienKills", 0 );
  Cvar_SetSafe( "g_humanKills", 0 );
  if(IS_SCRIM) {
    level.suddenDeathBeginTime = level.scrim.sudden_death_time * 60000;
  } else {
    level.suddenDeathBeginTime = g_suddenDeathTime.integer * 60000;
  }

  Com_Printf( "-----------------------------------\n" );

  // So the server counts the spawns without a client attached
  G_CountBuildables( );

  for( i = 0; i < NUM_TEAMS; i++ )
    level.numUnspawnedBuildables[ i ] = 0;

  G_UpdateTeamConfigStrings( );

  if( g_lockTeamsAtStart.integer )
  {
    level.alienTeamLocked = qtrue;
    level.humanTeamLocked = qtrue;
    Cvar_SetSafe( "g_lockTeamsAtStart", "0" );
  }

  for( i = 0; i < NUM_TEAMS; ++i )
  {
    level.voteType[ i ] = VOID_VOTE;
  }

  if(!IS_WARMUP) {
    level.fight = qtrue;
    level.nextmap_when_empty_teams = level.time + (g_nextMapStartedMatchWhenEmptyTeams.integer * 1000);
  }
}

/*
==================
G_ClearVotes

remove all currently active votes
==================
*/
static void G_ClearVotes( void )
{
  int i;
  memset( level.voteTime, 0, sizeof( level.voteTime ) );
  memset( level.numVotingClients, 0, sizeof( level.numVotingClients ) );
  for( i = 0; i < NUM_TEAMS; i++ )
  {
    SV_SetConfigstring( CS_VOTE_TIME + i, "" );
    SV_SetConfigstring( CS_VOTE_STRING + i, "" );
    level.voteType[ i ] = VOID_VOTE;
  }
}

/*
=================
G_ShutdownGame
=================
*/
Q_EXPORT void G_ShutdownGame( int restart )
{
  int i;

  // in case of a map_restart
  G_ClearVotes( );

  for(i = 0; i < NUM_TEAMS; i++) {
    BG_List_Clear(&level.spawn_queue[i]);
  }

  if(IS_SCRIM && level.scrim.scrim_completed) {
    G_Scrim_Reset_Settings();
  } else {
    if(level.scrim.swap_teams) {
      team_t temp = level.scrim.team[SCRIM_TEAM_1].current_team;

      //swap teams
      level.scrim.team[SCRIM_TEAM_1].current_team =
        level.scrim.team[SCRIM_TEAM_2].current_team;
      level.scrim.team[SCRIM_TEAM_2].current_team = temp;
    }
    G_Scrim_Save();
  }

  G_RestoreCvars( );

  Com_Printf( "==== ShutdownGame ====\n" );

  if( level.logFile )
  {
    G_LogPrintf( "ShutdownGame:\n" );
    G_LogPrintf( "------------------------------------------------------------\n" );
    FS_FCloseFile( level.logFile );
    level.logFile = 0;
  }

  if( !restart )
  {
    int i;
    // reset everyone's ready state
    for( i = 0; i < level.maxclients; i++ )
      level.clients[ i ].sess.readyToPlay = qfalse;
  }

  // write all the client session data so we can get it back
  G_WriteSessionData( );

  // Save the playmap files here
  G_SavePlayMapPool();
  G_SavePlayMapQueue();

  G_admin_cleanup( );
  G_namelog_cleanup( );
  G_UnregisterCommands( );

  G_FreePlayerModel( );

  G_ShutdownMapRotations( );

  level.restarted = qfalse;
  level.surrenderTeam = TEAM_NONE;
  SV_SetConfigstring( CS_WINNER, "" );

  sl_query( DB_CLOSE, NULL, NULL );
}


/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/


/*
=============
SortRanks

=============
*/
int QDECL SortRanks( const void *a, const void *b )
{
  gclient_t *ca, *cb;

  ca = &level.clients[ *(int *)a ];
  cb = &level.clients[ *(int *)b ];

  // sort by rank for a given team
  if(ca->pers.teamSelection == cb->pers.teamSelection) {
    if(ca->sess.rank > cb->sess.rank) {
      return -1;
    } else if(ca->sess.rank < cb->sess.rank) {
      return 1;
    }
  }

  // then sort by score
  if( ca->ps.persistant[ PERS_SCORE ] > cb->ps.persistant[ PERS_SCORE ] )
    return -1;
  if( ca->ps.persistant[ PERS_SCORE ] < cb->ps.persistant[ PERS_SCORE ] )
    return 1;
  else
    return 0;
}

static void G_Print_Client_Num_For_Spawn_Pos(void *data, void *user_data) {
  gclient_t *client = (gclient_t *)data;

  Com_Printf( "%d:", client - g_clients);
}

/*
-============
-G_PrintSpawnQueue
-
-Print the contents of a spawn queue
-============
-*/
void G_PrintSpawnQueue(team_t team) {
  bglist_t *spawn_queue = &level.spawn_queue[team];
  gclient_t *head_client = (gclient_t *)spawn_queue->head;
  gclient_t *tail_client = (gclient_t *)spawn_queue->tail;
  int       head = head_client ? head_client - g_clients : -1;
  int       tail = tail_client ? tail_client - g_clients : -1;
  int length = BG_List_Get_Length(spawn_queue);

  Com_Printf("length:%d head:%d tail:%d    :", length, head, tail);

  BG_List_Foreach(spawn_queue, NULL, G_Print_Client_Num_For_Spawn_Pos, NULL);

  Com_Printf( "\n" );
}

/*
============
G_SpawnClients

Spawn queued clients
============
*/
void G_SpawnClients(void *data, void *user_data) {
  gclient_t *client = (gclient_t *)data;
  gentity_t *ent = NULL;
	team_t    team;
  bglist_t *spawn_queue;
  int       client_num = client - level.clients;
	int       numSpawns = 0;

  if(client && client_num < MAX_CLIENTS && client_num >= 0) {
    ent = &g_entities[client_num];
  }

	if(
		G_Client_Alive( ent ) ||
    ent->client->pers.connected != CON_CONNECTED ||
		!ent->client->spawnReady ||
    (!IS_WARMUP && ent->client->pers.spawnTime > level.time)) {
		return;
	}

	team = G_Client_Team( ent );
	if( team == TEAM_ALIENS ) {
		numSpawns = level.numAlienSpawns;
  } else if( team == TEAM_HUMANS ) {
		numSpawns = level.numHumanSpawns;
  }

  spawn_queue = &level.spawn_queue[team];
  if(!IS_WARMUP && BG_List_Index(spawn_queue, ent->client) > 0) {
    return;
  }

	if(numSpawns > 0) {
		gentity_t *spawn;
		vec3_t    spawn_origin, spawn_angles;

		spawn =
      G_SelectTremulousSpawnPoint(
        team, ent->client->pers.lastDeathLocation, spawn_origin, spawn_angles);
		if(spawn != NULL) {
			ent->client->sess.spectatorState = SPECTATOR_NOT;
			ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );
			ClientSpawn( ent, spawn, spawn_origin, spawn_angles, qtrue );
      BG_List_Remove_All(
        &level.spawn_queue[team], ent->client);
			ent->client->spawnReady = qfalse;
      ent->client->ps.persistant[ PERS_STATE ] &= ~PS_QUEUED;
		}
	}
}

/*
============
G_CountBuildables

Counts the number of buildables of each type, and the number of spawns for each team
============
*/
void G_CountBuildables( void ) {
  int i;
  gentity_t *ent;

  level.numAlienSpawns = 0;
  level.numHumanSpawns = 0;
  for(i = 0; i < BA_NUM_BUILDABLES; i++) {
    level.num_buildables[i] = 0;
  }
  for(i = 0; i < NUM_TEAMS; i++) {
    level.core_buildable_constructing[i] = qfalse;
    level.core_buildable_health[i] = 0;
  }

  for(i = 1, ent = g_entities + i ; i < level.num_entities ; i++, ent++) {
    buildable_t buildable;
    team_t      team;

    if(!ent->inuse || ent->s.eType != ET_BUILDABLE || ent->health <= 0) {
      continue;
    }

    buildable = ent->s.modelindex;
    team = BG_Buildable(buildable)->team;

    if(buildable == BA_A_SPAWN) {
      level.numAlienSpawns++;
    }

    if(buildable == BA_H_SPAWN) {
      level.numHumanSpawns++;
    }

    level.num_buildables[buildable]++;

    if(BG_Buildable(buildable)->role & ROLE_CORE) {
      int health = (BG_SU2HP(ent->health) * 100) / BG_SU2HP(BG_Buildable(buildable)->health);

      level.core_buildable_constructing[team] = !ent->spawned;
      level.core_buildable_health[team] = health;
    }
  }
}


/*
============
G_TimeTilSuddenDeath
============
*/
#define SUDDENDEATHWARNING 60000
int G_TimeTilSuddenDeath( void )
{
  if(IS_SCRIM) {
    if( ( !level.scrim.sudden_death_time && level.suddenDeathBeginTime == 0 ) ||
        ( level.suddenDeathBeginTime < 0 ) ||
        IS_WARMUP )
      return SUDDENDEATHWARNING + 1; // Always some time away
  } else {
    if( ( !g_suddenDeathTime.integer && level.suddenDeathBeginTime == 0 ) ||
        ( level.suddenDeathBeginTime < 0 ) ||
        IS_WARMUP )
      return SUDDENDEATHWARNING + 1; // Always some time away
  }

  return ( ( level.suddenDeathBeginTime ) - ( level.time - level.startTime ) );
}

/*
============
G_SD_Mode
============
*/
sdmode_t G_SD_Mode(void) {
  if(IS_SCRIM) {
    return level.scrim.sudden_death_mode;
  } else {
    return g_suddenDeathMode.integer;
  }
}

/*
============
G_SuddenDeathModeString
============
*/
char *G_SuddenDeathModeString(void) {
  switch (G_SD_Mode( )) {
    case SDMODE_BP:
      return "Build Point";

    case SDMODE_NO_BUILD:
      return "No Build";

    case SDMODE_SELECTIVE:
      return "Selective";

    case SDMODE_NO_DECON:
      return "No Deconstruct";
  }
  return "";
}

#define PLAYER_COUNT_MOD 5.0f

/*
============
G_StageBuildPointMaxForTeam

Returns the max build points for a given team's stage.
============
*/
static int G_StageBuildPointMaxForTeam( team_t team )
{
  float mod;
  float baseBP;

  switch ( team )
  {
    case TEAM_ALIENS:
      baseBP = g_alienBuildPoints.value;
      switch ( IS_WARMUP ? g_alienMaxStage.integer : g_alienStage.integer )
      {
        case S1:
          mod = g_alienBuildPointsStageMod0.value;
          break;

        case S2:
          mod = g_alienBuildPointsStageMod1.value;
          break;

        case S3:
          mod = g_alienBuildPointsStageMod2.value;
          break;

        default:
          mod = g_alienBuildPointsStageMod2.value;
          break;
      }
      break;

    case TEAM_HUMANS:
      baseBP = g_humanBuildPoints.value;
      switch ( IS_WARMUP ? g_alienMaxStage.integer : g_humanStage.integer )
      {
        case S1:
          mod = g_humanBuildPointsStageMod0.value;
          break;

        case S2:
          mod = g_humanBuildPointsStageMod1.value;
          break;

        case S3:
          mod = g_humanBuildPointsStageMod2.value;
          break;

        default:
          mod = g_humanBuildPointsStageMod2.value;
          break;
      }
      break;

    default:
      Com_Error( ERR_FATAL, "G_StageBuildPointMaxForTeam: Team# %d is not accounted for", team);
      return 0;
  }

  return (int)( mod * baseBP );
}

/*
============
G_StageBuildPointReserveMaxForTeam

Returns the max build point reserve for a given team's stage.
============
*/
static int G_StageBuildPointReserveMaxForTeam( team_t team )
{
  float mod;
  float baseBPReserve;

  switch ( team )
  {
    case TEAM_ALIENS:
      baseBPReserve = g_alienBuildPointsReserve.integer;
      switch ( IS_WARMUP ? g_alienMaxStage.integer : g_alienStage.integer )
      {
        case S1:
          mod = g_alienBuildPointsStageMod0.value;
          break;

        case S2:
          mod = g_alienBuildPointsStageMod1.value;
          break;

        case S3:
          mod = g_alienBuildPointsStageMod2.value;
          break;

        default:
          mod = g_alienBuildPointsStageMod2.value;
          break;
      }
      break;

    case TEAM_HUMANS:
      baseBPReserve = g_humanBuildPointsReserve.integer;
      switch ( IS_WARMUP ? g_alienMaxStage.integer : g_humanStage.integer )
      {
        case S1:
          mod = g_humanBuildPointsStageMod0.value;
          break;

        case S2:
          mod = g_humanBuildPointsStageMod1.value;
          break;

        case S3:
          mod = g_humanBuildPointsStageMod2.value;
          break;

        default:
          mod = g_humanBuildPointsStageMod2.value;
          break;
      }
      break;

    default:
      Com_Error( ERR_FATAL, "G_StageBuildPointReserveMaxForTeam: Team# %d is not accounted for", team);
      return 0;
  }

  return (int)( mod * baseBPReserve );
}

/*
============
G_CalculateBuildPoints

Recalculate the quantity of building points available to the teams
============
*/
void G_CalculateBuildPoints( void )
{
  int i;
  int total_existing_alien_bp, total_existing_human_bp;

  // BP queue updates
  while(
    level.alienBuildPointQueue > 0 &&
    (
      level.alienBuildPointsReserve > 0 ||
      (g_suddenDeathMode.integer == SDMODE_SELECTIVE &&
        G_TimeTilSuddenDeath() <= 0)) &&
    level.alienNextQueueTime < level.time) {
    level.alienBuildPointQueue--;
    if(!IS_WARMUP && G_TimeTilSuddenDeath( ) > 0)
      level.alienBuildPointsReserveLost++;
    level.alienNextQueueTime += G_NextQueueTime( level.alienBuildPointQueue,
                                               G_StageBuildPointMaxForTeam( TEAM_ALIENS ),
                                               g_alienBuildQueueTime.integer );
  }


  while(
    level.humanBuildPointQueue > 0 &&
    (
      level.humanBuildPointsReserve > 0 ||
      (g_suddenDeathMode.integer == SDMODE_SELECTIVE &&
        G_TimeTilSuddenDeath() <= 0)) &&
    level.humanNextQueueTime < level.time) {
    level.humanBuildPointQueue--;
    if( !IS_WARMUP && G_TimeTilSuddenDeath( ) > 0 )
      level.humanBuildPointsReserveLost++;
    level.humanNextQueueTime += G_NextQueueTime( level.humanBuildPointQueue,
                                               G_StageBuildPointMaxForTeam( TEAM_HUMANS ),
                                               g_humanBuildQueueTime.integer );
  }



  // Sudden Death checks (not applicable in warmup)
  if( !IS_WARMUP && G_TimeTilSuddenDeath( ) <= 0 && level.suddenDeathWarning < TW_PASSED )
  {
    buildable_t buildable;

    G_LogPrintf( "Beginning Sudden Death\n" );
    SV_GameSendServerCommand( -1,
                            va( "cp \"Sudden Death!\" %d",
                                CP_SUDDEN_DEATH ) );
    SV_GameSendServerCommand(
      -1,
      va(
        "print \"Beginning Sudden Death (Mode: %s).\n\"",
        G_SuddenDeathModeString()) );
    level.suddenDeathWarning = TW_PASSED;
    G_ClearDeconMarks( );

    // empty the build points reserves
    level.alienBuildPointsReserveLost = G_StageBuildPointReserveMaxForTeam( TEAM_ALIENS );
    level.humanBuildPointsReserveLost = G_StageBuildPointReserveMaxForTeam( TEAM_HUMANS );

    // Clear blueprints, or else structs that cost 0 BP can still be built after SD
    for( i = 0; i < level.maxclients; i++ )
    {
      if( g_entities[ i ].client->ps.stats[ STAT_BUILDABLE ] != BA_NONE )
        g_entities[ i ].client->ps.stats[ STAT_BUILDABLE ] = BA_NONE;
    }

    //for SDMODE_SELECTIVE
    for(buildable = 0; buildable < BA_NUM_BUILDABLES; buildable++) {
      //always allow core buildables to be rebuilt
      if(BG_Buildable(buildable)->role & ROLE_CORE) {
        level.sudden_death_replacable[buildable] = qtrue;
      }

      //don't allow spawns and offesnive buildables to be rebuilt
      if(BG_Buildable(buildable)->role & (ROLE_SPAWN|ROLE_OFFENSE)) {
        continue;
      }

      //determine if a buildable type already exists
      if(G_FindBuildable(buildable)) {
        level.sudden_death_replacable[buildable] = qtrue;
      }
    }

    level.humanBuildPointQueue = level.alienBuildPointQueue = 0;
  }
  else if( !IS_WARMUP && G_TimeTilSuddenDeath( ) <= SUDDENDEATHWARNING &&
    level.suddenDeathWarning < TW_IMMINENT )
  {
    SV_GameSendServerCommand( -1,
                            va( "cp \"Sudden Death in %d seconds!\" %d",
                                (int)( G_TimeTilSuddenDeath( ) / 1000 ),
                                CP_SUDDEN_DEATH ) );
    SV_GameSendServerCommand( -1, va( "print \"Sudden Death will begin in %d seconds.\n\"",
          (int)( G_TimeTilSuddenDeath( ) / 1000 ) ) );
    level.suddenDeathWarning = TW_IMMINENT;
  }

  if(G_TimeTilSuddenDeath() <= 0 && G_SD_Mode( ) == SDMODE_SELECTIVE) {
    buildable_t buildable;

    level.humanBuildPoints = 0;
    level.humanBuildPointsReserve = 0;
    level.humanBuildTimeMod = 1.0f;
    level.alienBuildPoints = 0;
    level.alienBuildPointsReserve = 0;
    level.alienBuildTimeMod = 1.0f;

    for(buildable = 0; buildable < BA_NUM_BUILDABLES; buildable++) {
      if(level.sudden_death_replacable[buildable] && !G_FindBuildable(buildable)) {
        switch(BG_Buildable(buildable)->team) {
          case TEAM_ALIENS:
            level.alienBuildPoints += BG_Buildable(buildable)->buildPoints;
            break;

          case TEAM_HUMANS:
            level.humanBuildPoints += BG_Buildable(buildable)->buildPoints;
            break;

          default:
            break;
        }
      }
    }

    level.humanBuildPoints -= level.humanBuildPointQueue;
    level.alienBuildPoints -= level.alienBuildPointQueue;

    return;
  }

  level.humanBuildPoints = G_StageBuildPointMaxForTeam( TEAM_HUMANS ) - level.humanBuildPointQueue;
  level.humanBuildPointsReserve = G_StageBuildPointReserveMaxForTeam( TEAM_HUMANS ) -
                                  level.humanBuildPointsReserveLost;
  total_existing_human_bp = level.humanBuildPoints + level.humanBuildPointsReserve;
  level.alienBuildPoints = G_StageBuildPointMaxForTeam( TEAM_ALIENS ) - level.alienBuildPointQueue;
  level.alienBuildPointsReserve = G_StageBuildPointReserveMaxForTeam( TEAM_ALIENS ) -
                                  level.alienBuildPointsReserveLost;
  total_existing_alien_bp = level.alienBuildPoints + level.alienBuildPointsReserve;

  // Iterate through entities
  for( i = MAX_CLIENTS; i < level.num_entities; i++ )
  {
    gentity_t         *ent = &g_entities[ i ];
    buildable_t       buildable;
    int               cost;

    if( ent->s.eType != ET_BUILDABLE || ent->s.eFlags & EF_DEAD )
      continue;

    // Subtract the BP from the appropriate pool
    buildable = ent->s.modelindex;
    cost = BG_Buildable( buildable )->buildPoints;

    if( ent->buildableTeam == TEAM_ALIENS ) {
      level.alienBuildPoints -= cost;
    }
    if( ent->buildableTeam == TEAM_HUMANS ) {
      level.humanBuildPoints -= cost;
    }
  }

  if( level.humanBuildPoints < 0 )
    level.humanBuildPoints = 0;

  if( level.alienBuildPoints < 0 )
    level.alienBuildPoints = 0;

  level.humanBuildTimeMod =
    ((float)total_existing_human_bp) /
    ((float)(
      G_StageBuildPointMaxForTeam( TEAM_HUMANS ) +
      G_StageBuildPointReserveMaxForTeam( TEAM_HUMANS )));

  level.alienBuildTimeMod =
    ((float)total_existing_alien_bp) /
    ((float)(
      G_StageBuildPointMaxForTeam( TEAM_ALIENS ) +
      G_StageBuildPointReserveMaxForTeam( TEAM_ALIENS )));

  if(level.humanBuildTimeMod > 1.0f) {
    level.humanBuildTimeMod = 1.0f;
  } else if(level.humanBuildTimeMod < 0.20f) {
    level.humanBuildTimeMod = 0.20f;
  }

  if(level.alienBuildTimeMod > 1.0f) {
    level.alienBuildTimeMod = 1.0f;
  } else if(level.alienBuildTimeMod < 0.20f) {
    level.alienBuildTimeMod = 0.20f;
  }
}
/*
============
G_CalculateStages
============
*/
void G_CalculateStages( void )
{
  float         alienPlayerCountMod     = level.averageNumAlienClients / PLAYER_COUNT_MOD;
  float         humanPlayerCountMod     = level.averageNumHumanClients / PLAYER_COUNT_MOD;
  static int    lastAlienStageModCount  = 1;
  static int    lastHumanStageModCount  = 1;
  static int    alienTriggerStage       = 0;
  static int    humanTriggerStage       = 0;

  if( alienPlayerCountMod < 0.1f )
    alienPlayerCountMod = 0.1f;

  if( humanPlayerCountMod < 0.1f )
    humanPlayerCountMod = 0.1f;

  if( g_alienKills.integer >=
      (int)( ceil( (float)g_alienStage2Threshold.integer * alienPlayerCountMod ) ) &&
      g_alienStage.integer == S1 && g_alienMaxStage.integer > S1 )
  {
    Cvar_SetSafe( "g_alienStage", va( "%d", S2 ) );
    level.alienStage2Time = level.time;
    lastAlienStageModCount = g_alienStage.modificationCount;
    G_LogPrintf("Stage: A 2: Aliens reached Stage 2\n");
  }

  if( g_alienKills.integer >=
      (int)( ceil( (float)g_alienStage3Threshold.integer * alienPlayerCountMod ) ) &&
      g_alienStage.integer == S2 && g_alienMaxStage.integer > S2 )
  {
    Cvar_SetSafe( "g_alienStage", va( "%d", S3 ) );
    level.alienStage3Time = level.time;
    lastAlienStageModCount = g_alienStage.modificationCount;
    G_LogPrintf("Stage: A 3: Aliens reached Stage 3\n");
  }

  if( g_humanKills.integer >=
      (int)( ceil( (float)g_humanStage2Threshold.integer * humanPlayerCountMod ) ) &&
      g_humanStage.integer == S1 && g_humanMaxStage.integer > S1 )
  {
    Cvar_SetSafe( "g_humanStage", va( "%d", S2 ) );
    level.humanStage2Time = level.time;
    lastHumanStageModCount = g_humanStage.modificationCount;
    G_LogPrintf("Stage: H 2: Humans reached Stage 2\n");
  }

  if( g_humanKills.integer >=
      (int)( ceil( (float)g_humanStage3Threshold.integer * humanPlayerCountMod ) ) &&
      g_humanStage.integer == S2 && g_humanMaxStage.integer > S2 )
  {
    Cvar_SetSafe( "g_humanStage", va( "%d", S3 ) );
    level.humanStage3Time = level.time;
    lastHumanStageModCount = g_humanStage.modificationCount;
    G_LogPrintf("Stage: H 3: Humans reached Stage 3\n");
  }

  if( g_alienStage.modificationCount > lastAlienStageModCount )
  {
    while( alienTriggerStage < MIN( g_alienStage.integer, S3 ) )
      G_Checktrigger_stages( TEAM_ALIENS, ++alienTriggerStage );

    if( g_alienStage.integer == S2 )
      level.alienStage2Time = level.time;
    else if( g_alienStage.integer == S3 )
      level.alienStage3Time = level.time;

    lastAlienStageModCount = g_alienStage.modificationCount;
  }

  if( g_humanStage.modificationCount > lastHumanStageModCount )
  {
    while( humanTriggerStage < MIN( g_humanStage.integer, S3 ) )
      G_Checktrigger_stages( TEAM_HUMANS, ++humanTriggerStage );

    if( g_humanStage.integer == S2 )
      level.humanStage2Time = level.time;
    else if( g_humanStage.integer == S3 )
      level.humanStage3Time = level.time;

    lastHumanStageModCount = g_humanStage.modificationCount;
  }

  if( g_alienStage.integer == S1 && g_alienMaxStage.integer > S1 )
    level.alienNextStageThreshold = (int)( ceil( (float)g_alienStage2Threshold.integer *
                                                  alienPlayerCountMod ) );
  else if( g_alienStage.integer == S2 && g_alienMaxStage.integer > S2 )
    level.alienNextStageThreshold = (int)( ceil( (float)g_alienStage3Threshold.integer *
                                                 alienPlayerCountMod ) );
  else
    level.alienNextStageThreshold = -1;

  if( g_humanStage.integer == S1 && g_humanMaxStage.integer > S1 )
    level.humanNextStageThreshold = (int)( ceil( (float)g_humanStage2Threshold.integer *
                                                 humanPlayerCountMod ) );
  else if( g_humanStage.integer == S2 && g_humanMaxStage.integer > S2 )
    level.humanNextStageThreshold = (int)( ceil( (float)g_humanStage3Threshold.integer *
                                                 humanPlayerCountMod ) );
  else
    level.humanNextStageThreshold = -1;

  SV_SetConfigstring( CS_ALIEN_STAGES, va( "%d %d %d",
        ( IS_WARMUP ? S3 : g_alienStage.integer ),
        ( IS_WARMUP ? 99999 : g_alienKills.integer ),
        ( IS_WARMUP ? 0 : level.alienNextStageThreshold ) ) );

  SV_SetConfigstring( CS_HUMAN_STAGES, va( "%d %d %d",
        ( IS_WARMUP ? S3 : g_humanStage.integer ),
        ( IS_WARMUP ? 99999 : g_humanKills.integer ),
        ( IS_WARMUP ? 0 : level.humanNextStageThreshold ) ) );
}

/*
============
CalculateAvgPlayers

Calculates the average number of players playing this game
============
*/
void G_CalculateAvgPlayers( void )
{
  //there are no clients or only spectators connected, so
  //reset the number of samples in order to avoid the situation
  //where the average tends to 0
  if( !level.numAlienClients )
  {
    level.numAlienSamples = 0;
    Cvar_SetSafe( "g_alienCredits", "0" );
    Cvar_SetSafe( "g_alienKills", "0" );
  }

  if( !level.numHumanClients )
  {
    level.numHumanSamples = 0;
    Cvar_SetSafe( "g_humanCredits", "0" );
    Cvar_SetSafe( "g_humanKills", "0" );
  }

  //calculate average number of clients for stats
  level.averageNumAlienClients =
    ( ( level.averageNumAlienClients * level.numAlienSamples )
      + level.numAlienClients ) /
    (float)( level.numAlienSamples + 1 );
  level.numAlienSamples++;

  level.averageNumHumanClients =
    ( ( level.averageNumHumanClients * level.numHumanSamples )
      + level.numHumanClients ) /
    (float)( level.numHumanSamples + 1 );
  level.numHumanSamples++;
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( qboolean check_exit_rules )
{
  int       i;
  char      P[ MAX_CLIENTS + 1 ] = {""};

  level.numConnectedClients = 0;
  level.numPlayingClients = 0;
  memset( level.numVotingClients, 0, sizeof( level.numVotingClients ) );
  level.numAlienClients = 0;
  level.numHumanClients = 0;
  level.numAlienClientsAlive = 0;
  level.numHumanClientsAlive = 0;

  for( i = 0; i < level.maxclients; i++ )
  {
    P[ i ] = '-';
    if ( level.clients[ i ].pers.connected != CON_DISCONNECTED )
    {
      level.sortedClients[ level.numConnectedClients ] = i;
      level.numConnectedClients++;
      P[ i ] = (char)'0' + level.clients[ i ].pers.teamSelection;

      level.numVotingClients[ TEAM_NONE ]++;

      if( level.clients[ i ].pers.connected != CON_CONNECTED )
        continue;

      if( level.clients[ i ].pers.teamSelection != TEAM_NONE )
      {
        level.numPlayingClients++;
        if( level.clients[ i ].pers.teamSelection == TEAM_ALIENS )
        {
          level.numAlienClients++;
          if( level.clients[ i ].sess.spectatorState == SPECTATOR_NOT )
            level.numAlienClientsAlive++;
        }
        else if( level.clients[ i ].pers.teamSelection == TEAM_HUMANS )
        {
          level.numHumanClients++;
          if( level.clients[ i ].sess.spectatorState == SPECTATOR_NOT )
            level.numHumanClientsAlive++;
        }
      }
    }
  }
  level.numNonSpectatorClients = level.numAlienClientsAlive +
    level.numHumanClientsAlive;
  level.numVotingClients[ TEAM_ALIENS ] = level.numAlienClients;
  level.numVotingClients[ TEAM_HUMANS ] = level.numHumanClients;
  P[ i ] = '\0';
  Cvar_SetSafe( "P", P );

  qsort( level.sortedClients, level.numConnectedClients,
    sizeof( level.sortedClients[ 0 ] ), SortRanks );

  if(check_exit_rules || IS_WARMUP) {
    // see if it is time to end the level
    CheckExitRules( );
  }

  // if we are at the intermission, send the new info to everyone
  if( level.intermissiontime )
    SendScoreboardMessageToAllClients( );
}


/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients( void )
{
  int   i;

  for( i = 0; i < level.maxclients; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_CONNECTED )
      ScoreboardMessage( g_entities + i );
  }
}

/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
void MoveClientToIntermission( gentity_t *ent )
{
  // take out of follow mode if needed
  if( ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
    G_StopFollowing( ent );

  // move to the spot
  VectorCopy( level.intermission_origin, ent->s.pos.trBase );
  VectorCopy( level.intermission_origin, ent->r.currentOrigin );
  VectorCopy( level.intermission_origin, ent->client->ps.origin );
  VectorCopy( level.intermission_angle, ent->s.apos.trBase );
  VectorCopy( level.intermission_angle, ent->client->ps.viewangles );
  ent->client->ps.pm_type = PM_INTERMISSION;

  // clean up powerup info
  memset( ent->client->ps.misc, 0, sizeof( ent->client->ps.misc ) );

  ent->client->ps.eFlags = 0;
  ent->s.eFlags = 0;
  ent->s.eType = ET_GENERAL;
  ent->s.loopSound = 0;
  ent->s.event = 0;

  G_SetContents(ent, 0, qtrue);
  G_BackupUnoccupyContents( ent );
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
void FindIntermissionPoint( void )
{
  gentity_t *ent, *target;
  vec3_t    dir;

  // find the intermission spot
  ent = G_Find( NULL, FOFS( classname ), "info_player_intermission" );

  if( !ent )
  { // the map creator forgot to put in an intermission point...
    G_SelectSpawnPoint( vec3_origin, level.intermission_origin, level.intermission_angle );
  }
  else
  {
    VectorCopy( ent->r.currentOrigin, level.intermission_origin );
    VectorCopy( ent->r.currentAngles, level.intermission_angle );
    // if it has a target, look towards it
    if( ent->target )
    {
      target = G_PickTarget( ent->target );

      if( target )
      {
        VectorSubtract( target->r.currentOrigin, level.intermission_origin, dir );
        vectoangles( dir, level.intermission_angle );
      }
    }
  }

}

/*
==================
BeginIntermission
==================
*/
void BeginIntermission( void )
{
  int     i;
  gentity_t *client;

  if( level.intermissiontime )
    return;   // already active

  level.intermissiontime = level.time;

  G_ClearVotes( );

  for(i = 0; i < NUM_TEAMS; i++) {
    BG_List_Clear(&level.spawn_queue[i]);
  }

  G_UpdateTeamConfigStrings( );

  FindIntermissionPoint( );

  // move all clients to the intermission point
  for( i = 0; i < level.maxclients; i++ )
  {
    client = g_entities + i;

    if( !client->inuse )
      continue;

    // respawn if dead
    if( client->health <= 0 )
      respawn(client);

    MoveClientToIntermission( client );
  }

  // send the current scoring to all clients
  SendScoreboardMessageToAllClients( );
}


/*
=============
ExitLevel

When the intermission has been exited, the server is either moved
to a new map based on the map rotation or the current map restarted
=============
*/
void ExitLevel( void )
{
  int       i;
  gclient_t *cl;
  clientList_t readyMasks;

  //clear the "ready" configstring
  Com_Memset( &readyMasks, 0, sizeof( readyMasks ) );
  SV_SetConfigstring( CS_CLIENTS_READY, Com_ClientListString( &readyMasks ) );

  if ( G_MapExists( g_nextMap.string ) )
  {
    char map[ MAX_CVAR_VALUE_STRING ];

    Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );

    if(
      IS_SCRIM &&
      !level.scrim.scrim_completed &&
      !Q_stricmp(g_nextMap.string, map)) {
      //if the nextmap for the scrim is the same as the current map, just restart
      level.scrim_reset_time_on_restart = qtrue;
      G_LevelRestart(qfalse);
    } else {
      G_MapConfigs( g_nextMap.string );
      Cbuf_ExecuteText( EXEC_APPEND, va( "%smap \"%s\"\n",
        ( g_cheats.integer ? "dev" : "" ), g_nextMap.string ) );
    }
  }
  else if( G_PlayMapActive( ) )
    G_NextPlayMap();
  else if( G_MapRotationActive( ) )
    G_AdvanceMapRotation( 0 );
  else // Otherwise just restart current map
  {
    char map[ MAX_CVAR_VALUE_STRING ];
    Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
    G_MapConfigs( map );
    Cbuf_ExecuteText( EXEC_APPEND, "map_restart\n" );
  }

  Cvar_SetSafe( "g_nextMap", "" );
  Cvar_SetSafe( "g_warmup", "1" );
  SV_SetConfigstring( CS_WARMUP, va( "%d", IS_WARMUP ) );

  level.restarted = qtrue;
  level.changemap = NULL;
  level.intermissiontime = 0;

  // reset all the scores so we don't enter the intermission again
  for( i = 0; i < g_maxclients.integer; i++ )
  {
    cl = level.clients + i;
    if( cl->pers.connected != CON_CONNECTED )
      continue;

    cl->ps.persistant[ PERS_SCORE ] = 0;
  }

  // we need to do this here before changing to CON_CONNECTING
  G_WriteSessionData( );

  // change all client states to connecting, so the early players into the
  // next level will know the others aren't done reconnecting
  for( i = 0; i < g_maxclients.integer; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_CONNECTED )
      level.clients[ i ].pers.connected = CON_CONNECTING;
  }

}

/*
=================
G_AdminMessage

Print to all active server admins, and to the logfile, and to the server console
=================
*/
void G_AdminMessage( gentity_t *ent, const char *msg )
{
  char    string[ 1024 ];
  int     i;

  Com_sprintf( string, sizeof( string ), "chat %d %d \"%s\"",
    (int)( ent ? ent - g_entities : -1 ),
    G_admin_permission( ent, ADMF_ADMINCHAT ) ? SAY_ADMINS : SAY_ADMINS_PUBLIC,
    msg );

  // Send to all appropriate clients
  for( i = 0; i < level.maxclients; i++ )
    if( G_admin_permission( &g_entities[ i ], ADMF_ADMINCHAT ) )
       SV_GameSendServerCommand( i, string );

  // Send to the logfile and server console
  G_LogPrintf( "%s: %d \"%s" S_COLOR_WHITE "\": " S_COLOR_MAGENTA "%s\n",
    G_admin_permission( ent, ADMF_ADMINCHAT ) ? "AdminMsg" : "AdminMsgPublic",
    (int)( ent ? ent - g_entities : -1 ), ent ? ent->client->pers.netname : "console",
    msg );
}


/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open, and to the server console
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... )
{
  va_list argptr;
  char    string[ 1024 ], decolored[ 1024 ];
  int     min, tens, sec;

  sec = ( level.time - level.startTime ) / 1000;

  min = sec / 60;
  sec -= min * 60;
  tens = sec / 10;
  sec -= tens * 10;

  Com_sprintf( string, sizeof( string ), "%3i:%i%i ", min, tens, sec );

  va_start( argptr, fmt );
  Q_vsnprintf( string + 7, sizeof( string ) - 7, fmt, argptr );
  va_end( argptr );

  if( g_dedicated.integer )
  {
    G_UnEscapeString( string, decolored, sizeof( decolored ) );
    Com_Printf( "%s", decolored + 7 );
  }

  if( !level.logFile )
    return;

  G_DecolorString( string, decolored, sizeof( decolored ) );
  FS_Write( decolored, strlen( decolored ), level.logFile );
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string )
{
  int         i, numSorted;
  gclient_t   *cl;
  gentity_t   *ent;
  char        map[ MAX_QPATH ];

  G_Scrim_Check_Win_Conditions( );

  SV_SetConfigstring(CS_WINNER, level.winner_configstring);

  Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
  pack_start( level.database_data, DATABASE_DATA_MAX );
  pack_text2( map );
  pack_text2( (char *)string );
  pack_int( level.numConnectedClients );
  pack_int( level.epochStartTime );
  sl_query( DB_MAPSTAT_ADD, level.database_data, NULL );

  level.exited = qtrue;

  G_LogPrintf( "Exit: %s\n", string );

  level.intermissionQueued = level.time;

  // this will keep the clients from playing any voice sounds
  // that will get cut off when the queued intermission starts
  SV_SetConfigstring( CS_INTERMISSION, "1" );

  // don't send more than 32 scores (FIXME?)
  numSorted = level.numConnectedClients;
  if( numSorted > 32 )
    numSorted = 32;

  for( i = 0; i < numSorted; i++ )
  {
    int   ping;

    cl = &level.clients[ level.sortedClients[ i ] ];

    if( cl->ps.stats[ STAT_TEAM ] == TEAM_NONE )
      continue;

    if( cl->pers.connected == CON_CONNECTING )
      continue;

    ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

    G_LogPrintf( "score: %i  ping: %i  client: %i %s\n",
      cl->ps.persistant[ PERS_SCORE ], ping, level.sortedClients[ i ],
      cl->pers.netname );

  }

  for(i = 1, ent = g_entities + i ; i < level.num_entities ; i++, ent++) {
    if(!ent->inuse) {
      continue;
    }

    if(!Q_stricmp(ent->classname, "trigger_win")) {
      if(level.lastWin == ent->stageTeam) {
        if(g_debugAMP.integer) {
          char *s = va("%s win", BG_Team(level.lastWin)->humanName);
          s[0] = tolower(s[0]);
          G_LoggedActivation(ent, ent, ent, NULL, s, LOG_ACT_USE);
        } else {
          ent->use(ent, ent, ent);
        }
      }
    }
  }
}


/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void )
{
  int       ready, notReady, numPlayers;
  int       i;
  gclient_t *cl;
  clientList_t readyMasks;

  //if no clients are connected, just exit
  if( level.numConnectedClients == 0 )
  {
    ExitLevel( );
    return;
  }

  // see which players are ready
  ready = 0;
  notReady = 0;
  numPlayers = 0;
  Com_Memset( &readyMasks, 0, sizeof( readyMasks ) );
  for( i = 0; i < g_maxclients.integer; i++ )
  {
    cl = level.clients + i;

    if( cl->pers.connected != CON_CONNECTED )
      continue;

    if( cl->ps.stats[ STAT_TEAM ] == TEAM_NONE )
      continue;

    if( cl->readyToExit )
    {
      ready++;

      Com_ClientListAdd( &readyMasks, i );
    }
    else
      notReady++;

    numPlayers++;
  }

  SV_SetConfigstring( CS_CLIENTS_READY, Com_ClientListString( &readyMasks ) );

  // never exit in less than five seconds
  if( level.time < level.intermissiontime + 5000 )
    return;

  // never let intermission go on for over 1 minute
  if( level.time > level.intermissiontime + 60000 )
  {
    ExitLevel( );
    return;
  }

  // if nobody wants to go, clear timer
  if( ready == 0 && notReady > 0 )
  {
    level.readyToExit = qfalse;
    return;
  }

  // if everyone wants to go, go now
  if( notReady == 0 )
  {
    ExitLevel( );
    return;
  }

  // if only a percent is needed to ready, check for it
  if( g_intermissionReadyPercent.integer && !IS_SCRIM && numPlayers &&
      ready * 100 / numPlayers >= g_intermissionReadyPercent.integer &&
      level.time > level.intermissiontime + 7000 )
  {
    ExitLevel( );
    return;
  }

  // the first person to ready starts the 15 second timeout
  if( !level.readyToExit )
  {
    level.readyToExit = qtrue;
    level.exitTime = level.time;
  }

  // if we have waited 15 seconds since at least one player
  // wanted to exit, go ahead
  if( level.time < level.exitTime + 15000 )
    return;

  ExitLevel( );
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied( void )
{
  int   a, b;

  if( level.numPlayingClients < 2 )
    return qfalse;

  a = level.clients[ level.sortedClients[ 0 ] ].ps.persistant[ PERS_SCORE ];
  b = level.clients[ level.sortedClients[ 1 ] ].ps.persistant[ PERS_SCORE ];

  return a == b;
}

/*
=================
G_Time_Limit

=================
*/
int G_Time_Limit(void) {
  if(IS_SCRIM) {
    if(IS_WARMUP) {
      //the general time limit doesn't apply to scrim warmups
      return 0;
    } else {
      return level.scrim.time_limit;
    }
  } else {
    return g_timelimit.integer;
  }
}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
void CheckExitRules( void )
{
  int    index = rand( ) % MAX_INTERMISSION_SOUND_SETS; // randomly select an intermission sound set
  team_t winningTeam = TEAM_NONE;

  // if at the intermission, wait for all non-bots to
  // signal ready, then go to next level
  if( level.intermissiontime )
  {
    CheckIntermissionExit( );
    return;
  }

  //don't exit if warmup is ending
  if(IS_WARMUP && level.countdownTime) {
    return;
  }

  if( level.intermissionQueued )
  {
    if( level.time - level.intermissionQueued >= INTERMISSION_DELAY_TIME )
    {
      level.intermissionQueued = 0;
      BeginIntermission( );
    }

    return;
  }

  if(
    !g_restartingFlags.integer &&
    ((g_doWarmup.integer && !g_warmup.integer) || IS_SCRIM) &&
    g_nextMapStartedMatchWhenEmptyTeams.integer > 0) {
    if(
        (level.numAlienClients || level.numHumanClients) &&
        !(g_allowScrims.integer && level.scrim.mode == SCRIM_MODE_SETUP)) {
      level.nextmap_when_empty_teams = level.time + (g_nextMapStartedMatchWhenEmptyTeams.integer * 1000);
    } else {
      int i;

      for(i = 0; i < level.maxclients; i++) {
        gclient_t *client = &g_clients[i];

        if(client->pers.connected == CON_CONNECTING) {
          level.nextmap_when_empty_teams = level.time + (g_nextMapStartedMatchWhenEmptyTeams.integer * 1000);
          break;
        } else if(client->pers.connected != CON_CONNECTED) {
          continue;
        }

        if(IS_SCRIM &&
          !G_admin_permission( &g_entities[i], "scrim" )) {
          continue;
        }

        if(client->pers.voterInactivityTime >= level.time) {
          const int prev_time = level.nextmap_when_empty_teams;

          level.nextmap_when_empty_teams = level.time + (g_nextMapStartedMatchWhenEmptyTeams.integer * 1000);
          if((level.nextmap_when_empty_teams - VOTE_TIME) > prev_time) {
            level.nextmap_when_empty_teams -= VOTE_TIME;
          }
          break;
        }
      }

      if(level.nextmap_when_empty_teams < level.time) {
        if(IS_SCRIM) {
          G_Scrim_Reset_Settings();
          level.scrim.mode = SCRIM_MODE_OFF;
        }
        level.lastWin = TEAM_NONE;
        SV_GameSendServerCommand( -1, "print \"Teams empty\n\"" );
        Q_strncpyz(
          level.winner_configstring,
          va("%i %i %i", MATCHOUTCOME_TIME, index, TEAM_NONE),
          sizeof(level.winner_configstring));
        LogExit("Teams empty.");
        return;
      }
    }
  }

  if(
    G_Time_Limit() &&
    !(
      g_allowScrims.integer &&
      (
        (level.scrim.mode == SCRIM_MODE_SETUP) ||
        (level.scrim.mode == SCRIM_MODE_TIMEOUT))) )
  {
    if( level.time - level.startTime >= G_Time_Limit() * 60000 )
    {
      level.lastWin = TEAM_NONE;
      SV_GameSendServerCommand( -1, "print \"Timelimit hit\n\"" );
      Q_strncpyz(
        level.winner_configstring,
        va("%i %i %i", MATCHOUTCOME_TIME, index, TEAM_NONE),
        sizeof(level.winner_configstring));
      LogExit("Timelimit hit.");
      return;
    }
    else if( level.time - level.startTime >= ( G_Time_Limit() - 5 ) * 60000 &&
          level.timelimitWarning < TW_IMMINENT )
    {
      SV_GameSendServerCommand( -1,
                             va( "cp \"^75 minutes remaining!\" %d",
                                  CP_TIME_LIMIT ) );
      level.timelimitWarning = TW_IMMINENT;
    }
    else if( level.time - level.startTime >= ( G_Time_Limit() - 1 ) * 60000 &&
          level.timelimitWarning < TW_PASSED )
    {
      SV_GameSendServerCommand( -1,
                              va( "cp \"^71 minute remaining!\" %d",
                                  CP_TIME_LIMIT ) );
      level.timelimitWarning = TW_PASSED;
    }
  }

  //automatic scrim forfeit
  if(
    IS_SCRIM && level.scrim.mode != SCRIM_MODE_SETUP &&
    level.scrim.scrim_forfeiter == SCRIM_TEAM_NONE) {
    scrim_team_t scrim_team;

    scrim_team = BG_Scrim_Team_From_Playing_Team(&level.scrim, TEAM_ALIENS);
    if(scrim_team != SCRIM_TEAM_NONE && level.numAlienClients) {
      level.forfeit_when_team_is_empty[scrim_team] = level.time + 125000;
    }

    scrim_team = BG_Scrim_Team_From_Playing_Team(&level.scrim, TEAM_HUMANS);
    if(scrim_team != SCRIM_TEAM_NONE && level.numHumanClients) {
      level.forfeit_when_team_is_empty[scrim_team] = level.time + 125000;
    }

    for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
      if(scrim_team == SCRIM_TEAM_NONE) {
        continue;
      }

      if(level.scrim.team[scrim_team].current_team == TEAM_NONE) {
        continue;
      }

      if(level.forfeit_when_team_is_empty[scrim_team] < level.time) {
        level.scrim.scrim_forfeiter = scrim_team;
      }
    }
  }

  //draw condition
  if(
    (level.uncondAlienWin && level.uncondHumanWin) ||
    (
      !(g_restartingFlags.integer & RESTART_WARMUP_RESET) &&
      (level.time > level.startTime + 1000) &&
      (level.numAlienSpawns == 0) && (level.numHumanSpawns == 0) &&
      (
        IS_WARMUP ||
        ((level.numAlienClientsAlive == 0) && (level.numHumanClientsAlive == 0))))) {
    // We do not want any team to win in warmup
    if(IS_WARMUP) {
      if(level.lastLayoutReset > (level.time - 5000)) {
        return;
      }
      // FIXME: this is awfully ugly
      G_LevelRestart(qfalse);
      SV_GameSendServerCommand(
        -1, 
        "print \"^3Warmup Reset: ^7A mysterious force restores balance in the universe.\n\"");
      return;
    }

    //draw
    SV_GameSendServerCommand( -1, "print \"Mutual destruction obtained\n\"" );
    level.lastWin = TEAM_NONE;
    Q_strncpyz(
      level.winner_configstring,
      va("%i %i %i", MATCHOUTCOME_MUTUAL, index, TEAM_NONE ),
      sizeof(level.winner_configstring));
    LogExit("Draw.");
    return;
  }

  if(
    IS_SCRIM &&
    level.scrim.scrim_forfeiter != SCRIM_TEAM_NONE &&
    TEAM_ALIENS ==
      level.scrim.team[level.scrim.scrim_forfeiter].current_team) {
    //humans win
    winningTeam = level.lastWin = TEAM_HUMANS;
  } else if(
    IS_SCRIM &&
    level.scrim.scrim_forfeiter != SCRIM_TEAM_NONE &&
    TEAM_HUMANS ==
      level.scrim.team[level.scrim.scrim_forfeiter].current_team) {
    //aliens win
    winningTeam = level.lastWin = TEAM_ALIENS;
  } else if( level.uncondHumanWin ||
      ( !level.uncondAlienWin &&
        ( level.time > level.startTime + 1000 ) &&
        ( level.numAlienSpawns == 0 ) &&
        ( ( level.numAlienClientsAlive == 0 ) || IS_WARMUP ) &&
        !( g_restartingFlags.integer & RESTART_WARMUP_RESET ) ) )
  {
    // We do not want any team to win in warmup
    if( IS_WARMUP )
    {
      if( level.lastLayoutReset > ( level.time - 5000 ) )
        return;
      // FIXME: this is awfully ugly
      G_LevelRestart( qfalse );
      SV_GameSendServerCommand( -1, "print \"^3Warmup Reset: ^7A mysterious force restores balance in the universe.\n\"");
      return;
    }

    //humans win
    winningTeam = level.lastWin = TEAM_HUMANS;
  }
  else if(
           (
             IS_SCRIM &&
             level.scrim.scrim_forfeiter != SCRIM_TEAM_NONE &&
             TEAM_HUMANS ==
               level.scrim.team[level.scrim.scrim_forfeiter].current_team) ||
           level.uncondAlienWin ||
           ( ( level.time > level.startTime + 1000 ) &&
             ( level.numHumanSpawns == 0 ) &&
             ( ( level.numHumanClientsAlive == 0 ) || IS_WARMUP ) &&
             !( g_restartingFlags.integer & RESTART_WARMUP_RESET ) ) )
  {
    // We do not want any team to win in warmup
    if( IS_WARMUP )
    {
      if( level.lastLayoutReset > ( level.time - 5000 ) )
        return;
      // FIXME: this is awfully ugly
      G_LevelRestart( qfalse );
      SV_GameSendServerCommand( -1, "print \"^3Warmup Reset: ^7A mysterious force restores balance in the universe.\n\"");
      return;
    }

    //aliens win
    winningTeam = level.lastWin = TEAM_ALIENS;
  }

  if( winningTeam != TEAM_NONE )
  {
    SV_GameSendServerCommand( -1, va( "print \"%s win\n\"", BG_Team( winningTeam )->humanName ));
    Q_strncpyz(
        level.winner_configstring,
        va("%i %i %i", MATCHOUTCOME_WON, index, winningTeam ),
        sizeof(level.winner_configstring));
    LogExit(va("%s win.", BG_Team( winningTeam )->humanName));
  }
}

/*
==================
G_LevelRestart

Restart level and optionally specify non-warmup
==================
*/
void G_LevelRestart( qboolean stopWarmup )
{
  char      map[ MAX_CVAR_VALUE_STRING ];
  int       i;
  gclient_t *cl;
  gentity_t *tent;

  for( i = 0; i < g_maxclients.integer; i++ )
  {
    cl = level.clients + i;
    if( cl->pers.connected != CON_CONNECTED )
      continue;

    if( cl->pers.teamSelection == TEAM_NONE )
      continue;

    cl->sess.restartTeam = cl->pers.teamSelection;
  }

  Cvar_SetSafe( "g_nextLayout", level.layout );
  if( stopWarmup )
  {
    Cvar_SetSafe( "g_warmup", "0" );
    if( IS_WARMUP )
    {
      Cvar_SetSafe( "g_restartingFlags",
        va( "%i", ( g_restartingFlags.integer | RESTART_WARMUP_END ) ) );
      Cvar_Update( &g_restartingFlags );
    }
    SV_SetConfigstring( CS_WARMUP, va( "%d", IS_WARMUP ) );
    SV_SetConfigstring( CS_ALIEN_STAGES, va( "%d %d %d",
        ( g_alienStage.integer ),
        ( g_alienKills.integer ),
        ( level.alienNextStageThreshold ) ) );

    SV_SetConfigstring( CS_HUMAN_STAGES, va( "%d %d %d",
          ( g_humanStage.integer ),
          ( g_humanKills.integer ),
          ( level.humanNextStageThreshold ) ) );

    // reset everyone's ready state
    for( i = 0; i < level.maxclients; i++ )
      level.clients[ i ].sess.readyToPlay = qfalse;

    // If dev mode is on, turn it off
    if( g_cheats.integer )
    {
      for( i = 0; i < g_maxclients.integer; i++ )
      {
        cl = level.clients + i;
        tent = &g_entities[ cl->ps.clientNum ];

        //disable noclip
        if( cl->noclip )
        {

          G_SetContents(tent, tent->client->cliprcontents, qtrue);
          cl->noclip = qfalse;

          if( tent->r.linked )
            SV_LinkEntity( tent );

          SV_GameSendServerCommand( tent - g_entities, va( "print \"noclip OFF\n\"" ) );
        }

        //dissable god mode
        if( tent->flags & FL_GODMODE )
        {
          tent->flags ^= FL_GODMODE;
          SV_GameSendServerCommand( tent - g_entities, va( "print \"godmode OFF\n\"" ) );
        }
      }

      //turn dev mode off
      Cvar_SetSafe( "sv_cheats", "0" );
      AP( va( "print \"^3warmup ended: ^7developer mode has been switched off\n\"" ) );

      if(IS_SCRIM) {
        G_Scrim_Broadcast_Status( );
      }
    }
  } else if( IS_WARMUP && !(IS_SCRIM && level.scrim_reset_time_on_restart) )
  {
    // save warmup reset information
    Cvar_SetSafe( "g_restartingFlags",
        va( "%i", ( g_restartingFlags.integer | RESTART_WARMUP_RESET ) ) );
    Cvar_Update( &g_restartingFlags );
    Cvar_SetSafe( "g_warmupTimers", va( "%i %i %i",
                                         level.startTime,
                                         level.warmup1Time,
                                         level.warmup2Time ) );
  }

  Cvar_Update( &g_cheats );
  Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
  G_MapConfigs( map );
  Cbuf_ExecuteText( EXEC_APPEND, "map_restart\n" );

  if( !stopWarmup )
    level.lastLayoutReset = level.time;

}

/*
==================
G_LevelReady

Check for readyness of players and get out of warmup to start the real game if
required percentages are met.
==================
*/
void G_LevelReady( void )
{
  int       i, numAliens = 0, numHumans = 0;
  float     percentAliens, percentHumans, threshold;
  qboolean  startGame;
  qboolean  scrimNoExit;
  clientList_t readyMasks;

  // do not proceed if not in warmup
  if( !IS_WARMUP )
    return;

  // do not proceed if intermission started
  if( level.intermissiontime ) {
    return;
  }

  // the map is still resetting
  if( g_restartingFlags.integer & RESTART_WARMUP_RESET )
    return;

  scrimNoExit = g_allowScrims.integer &&
    (
      (level.scrim.mode == SCRIM_MODE_SETUP) ||
      (level.scrim.mode == SCRIM_MODE_TIMEOUT));

  Com_Memset(&readyMasks, 0, sizeof(readyMasks));
  for(i = 0; i < g_maxclients.integer; i++) {
    gclient_t *cl = level.clients + i;

    if(cl->pers.connected != CON_CONNECTED) {
      continue;
    }

    if(cl->ps.stats[STAT_TEAM] == TEAM_NONE) {
      continue;
    }

    if(cl->sess.readyToPlay ) {
      Com_ClientListAdd(&readyMasks, i);
    }
  }

  SV_SetConfigstring( CS_CLIENTS_READY, Com_ClientListString( &readyMasks ) );

  if( !level.countdownTime )
  {
    // reset number of players ready to zero
    level.readyToPlay[ TEAM_HUMANS ] = level.readyToPlay[ TEAM_ALIENS ] = 0;

    // update number of clients ready to play
    for( i = 0; i < level.maxclients; i++ )
    {
      if ( level.clients[ i ].pers.connected != CON_DISCONNECTED )
      {
        // spectators are not counted
        if( level.clients[ i ].pers.teamSelection == TEAM_NONE )
          continue;

        if( level.clients[ i ].pers.teamSelection == TEAM_ALIENS )
        {
          numAliens++;
          if( level.clients[ i ].sess.readyToPlay )
            level.readyToPlay[ TEAM_ALIENS ]++;
        }
        else if( level.clients[ i ].pers.teamSelection == TEAM_HUMANS )
        {
          numHumans++;
          if( level.clients[ i ].sess.readyToPlay )
            level.readyToPlay[ TEAM_HUMANS ]++;
        }
      }
    }

    // calculate percentage ready for both teams
    percentAliens = ( (float) level.readyToPlay[ TEAM_ALIENS ] /
        ( numAliens > 0 ? (float) numAliens : 1.0 ) ) * 100.0;
    percentHumans = ( (float) level.readyToPlay[ TEAM_HUMANS ] /
        ( numHumans > 0 ? (float) numHumans : 1.0 ) ) * 100.0;

    // if cvar g_warmupReadyThreshold is > 50 use it as threshold, if not use 50
    threshold = g_warmupReadyThreshold.integer > 50 ?
      (float) g_warmupReadyThreshold.integer : 50.0;

    // default condition for ending warmup is when >= (threshold)% of both teams
    // are ready
    startGame = ( percentAliens >= threshold && percentHumans >= threshold );

    // if previous conditions are not met and there are at least
    // (g_warmupTimeout1Trigger) players who are not spectators, start a
    // (g_warmupTimeout1) second countdown until warmup timeout
    if(
      !startGame && !scrimNoExit &&
      (
        (
          (numAliens > 0) && (numHumans > 0) &&
          ((numAliens + numHumans ) >= g_warmupTimeout1Trigger.integer)) ||
        (IS_SCRIM && level.scrim.mode == SCRIM_MODE_STARTED)))
    {
      if( ( level.warmup1Time > -1 ) &&
          ( level.time - level.warmup1Time ) >= ( IS_SCRIM ? 180000 : ( g_warmupTimeout1.integer * 1000 ) ) )
      {
        startGame = qtrue;
      }
      else if ( level.warmup1Time == -1 ) // start countdown
      {
        if(IS_SCRIM && level.scrim.mode == SCRIM_MODE_STARTED) {
          SV_GameSendServerCommand( -1, va( "print \"A scrim is in "
                " progress. Scrim warmup will expire in 180"
                " seconds.\n\"" ) );
        } else {
          SV_GameSendServerCommand( -1, va( "print \"There are at least %d "
                " non-spectating players. Pre-game warmup will expire in %d"
                " seconds.\n\"", g_warmupTimeout1Trigger.integer,
                g_warmupTimeout1.integer ) );
        }

        level.warmup1Time = level.time;
      }
    }
    else
    {
      // reset the 5 minute timeout
      level.warmup1Time = -1;
    }

    // if previous conditions are not met and at least (g_warmupTimeout2Trigger)%
    // of players in one team is ready, start a (g_warmupTimeout2) second
    // countdown until warmup timeout
    if( !startGame && !scrimNoExit &&
        ( ( IS_SCRIM && ( ( numAliens > 0 ) && ( numHumans > 0 ) ) ) ||
          ( ( numAliens > 1 ) && ( numHumans > 1 ) ) ) &&
        ( percentAliens >= (float) g_warmupTimeout2Trigger.integer ||
          percentHumans >= (float) g_warmupTimeout2Trigger.integer ) )
    {
      if( ( level.warmup2Time > -1 ) &&
          ( level.time - level.warmup2Time ) >= ( g_warmupTimeout2.integer * 1000 ) )
      {
        startGame = qtrue;
      }
      else if ( level.warmup2Time == -1 ) // start countdown
      {
        SV_GameSendServerCommand( -1, va( "print \"At least %d percent of players"
              " in a team have become ready. %s warmup will expire in %d"
              " seconds.\n\"", g_warmupTimeout2Trigger.integer,
              IS_SCRIM ? "Scrim" : "Pre-game",
              g_warmupTimeout2.integer ) );
        level.warmup2Time = level.time;
      }
    }
    else
    {
      // reset the 1 minute timeout
      level.warmup2Time = -1;
    }

    SV_SetConfigstring( CS_WARMUP_READY, va( "%.0f %d %d %.2f %d %d %d %d",
          percentAliens, level.readyToPlay[ TEAM_ALIENS ], numAliens,
          percentHumans, level.readyToPlay[ TEAM_HUMANS ], numHumans,
          ( level.warmup1Time > -1 ?  ( level.time - level.warmup1Time ) / 1000 :
            -1 ),
          ( level.warmup2Time > -1 ?  ( level.time - level.warmup2Time ) / 1000 :
            -1 ) ) );
  } else
    startGame = qtrue;

  //don't end warmup while a scrim is being setup or is paused.
  if(scrimNoExit) {
    return;
  }

  // only continue to start game when conditions are met
  if( !startGame )
    return;

  if( g_doWarmupCountdown.integer && !level.countdownTime )
  {
    int    index = rand( ) % MAX_WARMUP_SOUNDS; // randomly select an end of warmup sound set

    level.countdownTime = level.time + ( g_doWarmupCountdown.integer * 1000 );
    SV_SetConfigstring( CS_COUNTDOWN, va( "%i %i", level.countdownTime, index ) );
    G_LogPrintf( "Countdown to the End of Warmup: %i\n", g_doWarmupCountdown.integer );
  }

  if( level.countdownTime <= level.time )
    G_LevelRestart( qtrue );
}

/*
==================
G_Vote
==================
*/
void G_Vote( gentity_t *ent, team_t team, qboolean voting )
{
  if( !level.voteTime[ team ] )
    return;

  if( voting && ent->client->pers.voted & ( 1 << team ) )
    return;

  if( !voting && !( ent->client->pers.voted & ( 1 << team ) ) )
    return;

  if(
    IS_SCRIM &&
    (
      BG_Scrim_Team_From_Playing_Team(
        &level.scrim, ent->client->pers.teamSelection) == SCRIM_TEAM_NONE)) {
    return;
  }

  ent->client->pers.voted |= 1 << team;

  if( ent->client->pers.vote & ( 1 << team ) )
  {
    if( voting )
      level.voteYes[ team ]++;
    else
      level.voteYes[ team ]--;

    SV_SetConfigstring( CS_VOTE_CAST + team,
      va( "%d", (level.voteYes[ team ] + level.voteNo[ team ] ) ) );
  }
  else
  {
    if( voting )
      level.voteNo[ team ]++;
    else
      level.voteNo[ team ]--;

    SV_SetConfigstring( CS_VOTE_CAST + team,
      va( "%d", (level.voteYes[ team ] + level.voteNo[ team ] ) ) );
  }
}


/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/


void G_ExecuteVote( team_t team )
{
  level.voteExecuteTime[ team ] = 0;

  if( !Q_stricmpn( level.voteString[ team ], "map_restart", 11 ) )
  {
    char map[ MAX_QPATH ];
    Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
    G_MapConfigs( map );
  }
  else if( !Q_stricmpn( level.voteString[ team ], "map", 3 ) )
  {
    char map[ MAX_QPATH ];
    char *p;
    Q_strncpyz( map, strchr( level.voteString[ team ], '"' ) + 1, sizeof( map ) );
    if( ( p = strchr( map, '"' ) ) )
      *p = '\0';
    G_MapConfigs( map );
  }

  Cbuf_ExecuteText( EXEC_APPEND, va( "%s%s\n",
    ( !Q_stricmp( level.voteString[ team ], "map" ) && g_cheats.integer ? "dev" : "" ),
    level.voteString[ team ] ) );

  if( !Q_stricmpn( level.voteString[ team ], "map", 3 ) )
    level.restarted = qtrue;
}

/*
==================
G_CheckVote
==================
*/
void G_CheckVote( team_t team )
{
  float    votePassThreshold = (float)level.voteThreshold[ team ] / 100.0f;
  qboolean pass = qfalse;
  char     *msg;
  int      i;
  int      voteYesPercent = 0 , voteNoPercent = 0;
  int      numActiveClients = 0;

  if( level.voteExecuteTime[ team ] &&
      level.voteExecuteTime[ team ] < level.time )
  {
    G_ExecuteVote( team );
  }

  if( !level.voteTime[ team ] )
    return;

  // Recalculate number of voting clients if there are players in teams
  // and only count a client as a voting client if they are either
  // (a) a client that has been active in the previous length of VOTE_TIME, and/or
  // (b) a client that has voted
  for( i = 0; i < level.maxclients; i++ )
  {
    if ( level.clients[ i ].pers.connected != CON_DISCONNECTED )
    {
      if(IS_SCRIM && level.clients[ i ].pers.teamSelection == TEAM_NONE) {
        continue;
      }
      switch( team )
      {
        case TEAM_ALIENS:
        case TEAM_HUMANS:

          if( ( ( level.clients[ i ].pers.teamSelection == team ) &&
                ( ( level.clients[ i ].pers.voted & ( 1 << team ) ) ||
                  ( level.time < level.clients[ i ].pers.voterInactivityTime ) ) ) )
            numActiveClients++;
          break;
        default:
          if( ( level.clients[ i ].pers.voted & ( 1 << team ) ) ||
              ( level.time < level.clients[ i ].pers.voterInactivityTime ) )
            numActiveClients++;
          break;
      }
    }
  }

  if( g_impliedVoting.integer && !( level.voteType[ team ] == POLL_VOTE ) )
  {
    level.numCountedVotingClients[ team ] = numActiveClients;
    SV_SetConfigstring( CS_VOTE_ACTIVE + team,
    va( "%d", level.numCountedVotingClients[ team ] ) );
  }
  else
  {
    level.numCountedVotingClients[ team ] = level.voteYes[ team ] + level.voteNo[ team ];
    SV_SetConfigstring( CS_VOTE_ACTIVE + team, va( "%d", -1 ) );
  }

  if( ( level.time - level.voteTime[ team ] >= VOTE_TIME ) ||
      ( level.voteYes[ team ] + level.voteNo[ team ] == level.numVotingClients[ team ] ) )
  {
    pass = ( level.voteYes[ team ] && ( level.numCountedVotingClients[ team ] > 0 ) &&
             ( (float)level.voteYes[ team ] / level.numCountedVotingClients[ team ] > votePassThreshold ) );
  }
  else
  {
    if( ( (float)level.voteYes[ team ] >
          (float)level.numVotingClients[ team ] * votePassThreshold ) && !( level.voteType[ team ] == POLL_VOTE ) )
    {
      pass = qtrue;
    }
    else if( (float)level.voteNo[ team ] <=
        (float)level.numVotingClients[ team ] * ( 1.0f - votePassThreshold ) )
    {
      return;
    }
  }

  if( pass )
    level.voteExecuteTime[ team ] = level.time + level.voteDelay[ team ];

  G_LogPrintf( "EndVote: %s %s %d(yes) %d(no) %d(num of voting clients) %d(num of counted voting clients) %s\n",
               team == TEAM_NONE ? "global" : BG_Team( team )->name2,
               pass ? "^2pass^7" : "^1fail^7",
               level.voteYes[ team ], level.voteNo[ team ], level.numVotingClients[ team ],
               level.numCountedVotingClients[ team ],
               ( g_impliedVoting.integer && !( level.voteType[ team ] == POLL_VOTE ) ) ?
               "implied" : "expressed"	 );

  if( level.numCountedVotingClients[ team ] > 0 )
  {
    voteYesPercent = ( 100 * level.voteYes[ team ] ) / level.numCountedVotingClients[ team ];
    voteNoPercent = ( 100 * level.voteNo[ team ] ) / level.numCountedVotingClients[ team ];
  }

  if( level.voteType[ team ] == POLL_VOTE )
  {
    if( level.numCountedVotingClients[ team ] == 0 )
      msg = va( "print \"%sote %sed^7 (No player voted)\n\"",
                team == TEAM_NONE ? "V" : "Team v", pass ? "^2pass" : "^1fail" );
    else if( level.numCountedVotingClients[ team ] == 1 )
      msg = va( "print \"%sote %sed^7 (Out of %d total vote %d%% voted yes and %d%% voted no)\n\"",
                team == TEAM_NONE ? "V" : "Team v", pass ? "^2pass" : "^1fail",
                ( level.voteYes[ team ] + level.voteNo[ team ] ), voteYesPercent, voteNoPercent );
    else
      msg = va( "print \"%sote %sed^7 (Out of %d total votes %d%% voted yes and %d%% voted no)\n\"",
                team == TEAM_NONE ? "V" : "Team v", pass ? "^2pass" : "^1fail",
                ( level.voteYes[ team ] + level.voteNo[ team ] ), voteYesPercent, voteNoPercent );
  }
  else
  {
    if( g_impliedVoting.integer )
    {
      if( level.numCountedVotingClients[ team ] == 1)
        msg = va( "print \"%sote %sed^7 (%d%% voted yes out of %d %s)\n\"",
                  team == TEAM_NONE ? "V" : "Team v", pass ? "^2pass" : "^1fail",
                  voteYesPercent, level.numCountedVotingClients[ team ],
                  team == TEAM_NONE ? "active player" : "active teammate" );
      else
        msg = va( "print \"%sote %sed^7 (%d%% voted yes out of %d %s)\n\"",
                    team == TEAM_NONE ? "V" : "Team v", pass ? "^2pass" : "^1fail",
                    voteYesPercent, level.numCountedVotingClients[ team ],
                    team == TEAM_NONE ? "active players" : "active teammates" );
    }
    else
    {
      if( level.numCountedVotingClients[ team ] == 1)
        msg = va( "print \"%sote %sed^7 (%d%% voted yes out of %d total vote)\n\"",
                  team == TEAM_NONE ? "V" : "Team v", pass ? "^2pass" : "^1fail",
                  voteYesPercent, level.numCountedVotingClients[ team ] );
      else
        msg = va( "print \"%sote %sed^7 (%d%% voted yes out of %d total votes)\n\"",
                    team == TEAM_NONE ? "V" : "Team v", pass ? "^2pass" : "^1fail",
                    voteYesPercent, level.numCountedVotingClients[ team ] );
    }
  }

  if( team == TEAM_NONE )
    SV_GameSendServerCommand( -1, msg );
  else
    G_TeamCommand( team, msg );

  level.voteTime[ team ] = 0;
  level.voteYes[ team ] = 0;
  level.voteNo[ team ] = 0;
  level.numCountedVotingClients[ team ] = 0;
  level.voteType[ team ] = VOID_VOTE;

  for( i = 0; i < level.maxclients; i++ )
    level.clients[ i ].pers.voted &= ~( 1 << team );

  SV_SetConfigstring( CS_VOTE_TIME + team, "" );
  SV_SetConfigstring( CS_VOTE_STRING + team, "" );
  SV_SetConfigstring( CS_VOTE_CAST + team, "0" );
  SV_SetConfigstring( CS_VOTE_ACTIVE + team, "0" );
}


/*
==================
G_EndVote
==================
*/
void G_EndVote( team_t team, qboolean cancel )
{
  int i;

  for( i = 0; i < level.maxclients; i++ )
  {
    if ( level.clients[ i ].pers.connected != CON_DISCONNECTED )
    {
      if ( team == TEAM_NONE || level.clients[ i ].pers.teamSelection == team )
      {
        if( cancel )
          level.clients[ i ].pers.vote &= ~( 1 << team );
        else
          level.clients[ i ].pers.vote |= 1 << team;

        level.clients[ i ].pers.voted |= 1 << team;
      }
    }
  }

  level.voteNo[ team ] = cancel ? level.numVotingClients[ team ] : 0;
  level.voteYes[ team ] = cancel ? 0 : level.numVotingClients[ team ];

  G_CheckVote( team );
}

/*
==================
CheckCvars
==================
*/
void CheckCvars(void) {
  static int lastPasswordModCount         = -1;
  static int lastMarkDeconModCount        = -1;
  static int lastSDTimeModCount           = -1;
  static int lastNumZones                 =  0;
  static int lastTimeLimitModCount        = -1;
  static int lastExtendTimeLimit          =  0;
  static int lastHumanStaminaModeModCount = -1;
  static int lastCheatsModCount           = -1;
  static int lastPlayerAccelModeModCount  = -1;
  static int lastUnlaggedModCount         = -1;
  static int lastDoWarmupModCount         = 0;
  static int lastAllowScrimsModCount      = -1;

  if(g_doWarmup.modificationCount != lastDoWarmupModCount) {
    lastDoWarmupModCount = g_doWarmup.modificationCount;
    if(g_warmup.integer && !g_doWarmup.integer) {
      //restart with warmup disabled
      G_LevelRestart(qtrue);
    }
  }

  if(g_allowScrims.modificationCount != lastAllowScrimsModCount) {
    lastAllowScrimsModCount = g_allowScrims.modificationCount;
    if(!g_allowScrims.integer && level.scrim.mode) {
      G_Scrim_Reset_Settings();
      level.scrim.mode = SCRIM_MODE_OFF;
      G_Scrim_Send_Status( );
    }
  }

  if(g_password.modificationCount != lastPasswordModCount) {
    lastPasswordModCount = g_password.modificationCount;

    if(*g_password.string && Q_stricmp(g_password.string, "none")) {
      Cvar_SetSafe("g_needpass", "1");
      Cvar_Update(&g_needpass);
    }
    else {
      Cvar_SetSafe("g_needpass", "0");
      Cvar_Update(&g_needpass);
    }
  }

  // Unmark any structures for deconstruction when
  // the server setting is changed
  if(g_markDeconstruct.modificationCount != lastMarkDeconModCount) {
    lastMarkDeconModCount = g_markDeconstruct.modificationCount;
    G_ClearDeconMarks( );
  }

  // If we change g_suddenDeathTime during a map, we need to update
  // when sd will begin
  if(g_suddenDeathTime.modificationCount != lastSDTimeModCount && !IS_SCRIM) {
    lastSDTimeModCount = g_suddenDeathTime.modificationCount;
    level.suddenDeathBeginTime = g_suddenDeathTime.integer * 60000;

    if(G_TimeTilSuddenDeath( ) > 0) {
      // reset the sudden death time warnings
      if(G_TimeTilSuddenDeath( ) <= SUDDENDEATHWARNING) {
        level.suddenDeathWarning = TW_IMMINENT;
      } else {
        level.suddenDeathWarning = TW_NOT;
      }

      // reset the replacable buildables for SDMODE_SELECTIVE
      memset(level.sudden_death_replacable, 0, sizeof(level.sudden_death_replacable));
    }
  }

  // If the number of zones changes, we need a new array
  if(g_humanRepeaterMaxZones.integer != lastNumZones) {
    buildPointZone_t  *newZones;
    size_t            newsize = g_humanRepeaterMaxZones.integer * sizeof(buildPointZone_t);
    size_t            oldsize = lastNumZones * sizeof(buildPointZone_t);

    newZones = BG_Alloc0(newsize);
    if(level.buildPointZones) {
      Com_Memcpy(newZones, level.buildPointZones, MIN(oldsize, newsize));
      BG_Free(level.buildPointZones);
    }

    level.buildPointZones = newZones;
    lastNumZones = g_humanRepeaterMaxZones.integer;
  }

  // adjust settings related to time limit extensions
  if(level.timeLimitInitialized) {
    lastTimeLimitModCount = g_timelimit.modificationCount;
    level.timeLimitInitialized = qfalse;
  }

  if(g_timelimit.modificationCount != lastTimeLimitModCount) {
    if(g_timelimit.integer < 0) {
      Cvar_SetSafe("timelimit", "0");
      Cvar_Update(&g_timelimit);
    }

    level.extendTimeLimit = 0;
    lastExtendTimeLimit = 0;
    level.extendVoteCount = 0;
    level.matchBaseTimeLimit = g_timelimit.integer;
    lastTimeLimitModCount = g_timelimit.modificationCount;

    // reset the time limit warnings
    if(level.time - level.startTime < (g_timelimit.integer - 5) * 60000) {
      level.timelimitWarning = TW_NOT;
    }
    else if(level.time - level.startTime < (g_timelimit.integer - 1) * 60000) {
      level.timelimitWarning = TW_IMMINENT;
    }
  } else if(level.extendTimeLimit != lastExtendTimeLimit) {
    const int timelimit = level.matchBaseTimeLimit + level.extendTimeLimit;

    Cvar_SetSafe("timelimit", va("%d", (timelimit)));
    Cvar_Update(&g_timelimit);
    lastExtendTimeLimit = level.extendTimeLimit;
    lastTimeLimitModCount = g_timelimit.modificationCount;

    // reset the time limit warnings
    if(level.time - level.startTime < ((timelimit) - 5) * 60000) {
      level.timelimitWarning = TW_NOT;
    }
    else if(level.time - level.startTime < (timelimit - 1) * 60000) {
      level.timelimitWarning = TW_IMMINENT;
    }
  }

  if(g_cheats.modificationCount != lastCheatsModCount) {
    SV_SetConfigstring(CS_DEVMODE, va("%i", g_cheats.integer));
  }

  if(
    g_playerAccelMode.modificationCount != lastPlayerAccelModeModCount ||
    g_humanStaminaMode.modificationCount != lastHumanStaminaModeModCount) {
    lastPlayerAccelModeModCount = g_playerAccelMode.modificationCount;
    lastHumanStaminaModeModCount = g_humanStaminaMode.modificationCount;
    SV_SetConfigstring(
      CS_PHYSICS,
      va("%i %i", g_playerAccelMode.integer, g_humanStaminaMode.integer));
  }

  if(g_unlagged.modificationCount != lastUnlaggedModCount) {
    lastUnlaggedModCount = g_unlagged.modificationCount;
    if(g_unlagged.integer) {
      G_Init_Unlagged( );
    }
  }

  level.frameMsec = Sys_Milliseconds( );
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink( gentity_t *ent )
{
  float thinktime;

  thinktime = ent->nextthink;
  if( thinktime <= 0 )
    return;

  if( thinktime > level.time )
    return;

  ent->nextthink = 0;
  if( !ent->think )
    Com_Error( ERR_DROP, "NULL ent->think" );

  ent->think( ent );
}

/*
=============
G_EvaluateAcceleration

Calculates the acceleration for an entity
=============
*/
void G_EvaluateAcceleration( gentity_t *ent, int msec )
{
  vec3_t  deltaVelocity;
  vec3_t  deltaAccel;

  VectorSubtract( ent->s.pos.trDelta, ent->oldVelocity, deltaVelocity );
  VectorScale( deltaVelocity, 1.0f / (float)msec, ent->acceleration );

  VectorSubtract( ent->acceleration, ent->oldAccel, deltaAccel );
  VectorScale( deltaAccel, 1.0f / (float)msec, ent->jerk );

  VectorCopy( ent->s.pos.trDelta, ent->oldVelocity );
  VectorCopy( ent->acceleration, ent->oldAccel );
}

/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
Q_EXPORT void G_RunFrame( int levelTime )
{
  int        i;
  gentity_t  *ent;
  int        msec;
  static int ptime3000 = 0;

  // if we are waiting for the level to restart, do nothing
  if( level.restarted )
    return;

  if( level.pausedTime )
  {
    msec = levelTime - level.time - level.pausedTime;
    level.pausedTime = levelTime - level.time;

    //adjust the start time
    level.startTime += msec;
    SV_SetConfigstring( CS_LEVEL_START_TIME, va( "%i", level.startTime ) );

    //adjust the warmup timers
    if(level.warmup1Time > -1) {
      level.warmup1Time += msec;
    }
    if(level.warmup2Time > -1) {
      level.warmup2Time += msec;
    }

    //adjust the countdownTime;
    if(level.countdownTime > 0) {
      int dummy, index;
      char  info[ MAX_STRING_CHARS ];

      level.countdownTime += msec;
      SV_GetConfigstring( CS_COUNTDOWN, info, sizeof( info ) );
      sscanf( info, "%i %i",
                  &dummy,
                  &index);
      SV_SetConfigstring( CS_COUNTDOWN, va( "%i %i", level.countdownTime, index ) );
    }

    ptime3000 += msec;
    while( ptime3000 > 3000 )
    {
      ptime3000 -= 3000;
      SV_GameSendServerCommand( -1,
                              va( "cp \"The game has been paused. Please wait.\" %d",
                                  CP_PAUSE ) );

      if( level.pausedTime >= 110000  && level.pausedTime <= 119000 )
        SV_GameSendServerCommand( -1, va( "print \"Server: Game will auto-unpause in %d seconds\n\"",
          (int) ( (float) ( 120000 - level.pausedTime ) / 1000.0f ) ) );
    }

    // Prevents clients from getting lagged-out messages
    for( i = 0; i < level.maxclients; i++ )
    {
      if( level.clients[ i ].pers.connected == CON_CONNECTED )
        level.clients[ i ].ps.commandTime = levelTime;
    }

    if( level.pausedTime > 120000 )
    {
      SV_GameSendServerCommand( -1, "print \"Server: The game has been unpaused automatically (2 minute max)\n\"" );
      SV_GameSendServerCommand( -1,
                              va( "cp \"The game has been unpaused!\" %d",
                                  CP_PAUSE ) );
      level.pausedTime = 0;
    }

    return;
  }

  G_Scrim_Check_Pause( );

  level.framenum++;
  level.previousTime = level.time;
  level.time = levelTime;
  msec = level.time - level.previousTime;

  // get any cvar changes
  G_UpdateCvars( );
  CheckCvars( );
  // now we are done spawning
  level.spawning = qfalse;

  for( i = 0; i < PORTAL_NUM; i++ )
  {
    // pump human portals fire timer delays
    if( level.humanPortals.createTime[ i ] > 0 )
      level.humanPortals.createTime[ i ] -= msec;
    if( level.humanPortals.createTime[ i ] < 0 )
      level.humanPortals.createTime[ i ] = 0;

    // check if a human portal's lifetime has expired
    if( !level.humanPortals.portals[ i ] )
      continue;

    if(  level.humanPortals.lifetime[ i ] <= level.time )
      G_Portal_Clear( i );
  }

  //
  // go through all allocated objects
  //
  G_Unlagged_Prepare_Store( );
  G_ResetPusherNum( );
  ent = &g_entities[ 0 ];

  for( i = 0; i < level.num_entities; i++, ent++ )
  {
    if( !ent->inuse )
      continue;

    // clear events that are too old
    if( level.time - ent->eventTime > EVENT_VALID_MSEC )
    {
      if( ent->s.event )
      {
        ent->s.event = 0; // &= EV_EVENT_BITS;
        if ( ent->client )
        {
          ent->client->ps.externalEvent = 0;
          //ent->client->ps.events[0] = 0;
          //ent->client->ps.events[1] = 0;
        }
      }

      if( ent->freeAfterEvent )
      {
        // tempEntities or dropped items completely go away after their event
        G_FreeEntity( ent );
        continue;
      }
      else if( ent->unlinkAfterEvent )
      {
        // items that will respawn will hide themselves after their pickup event
        ent->unlinkAfterEvent = qfalse;
        SV_UnlinkEntity( ent );
      }
    }

    // temporary entities don't think
    if( ent->freeAfterEvent )
      continue;

    // calculate the acceleration of this entity
    if( ent->evaluateAcceleration )
      G_EvaluateAcceleration( ent, msec );

    if( !ent->r.linked && ent->neverFree )
      continue;

    G_OccupantThink( ent );

    if( ent->s.eType == ET_MISSILE )
    {
      G_RunMissile( ent );
      continue;
    }

    if( ent->s.eType == ET_BUILDABLE )
    {
      G_BuildableThink( ent, msec );
      G_Unlagged_Link_To_Store_Data(
        ent,
        (ent->s.modelindex == BA_A_BARRICADE),
        qtrue,
        qfalse,
        qfalse);
      continue;
    }

    if( ent->s.eType == ET_CORPSE || ent->physicsObject )
    {
      G_Physics( ent, msec );
      G_Unlagged_Link_To_Store_Data(ent, qfalse, qtrue, qfalse, qfalse);
      continue;
    }

    if( ent->s.eType == ET_MOVER )
    {
      G_RunMover( ent );
      G_Unlagged_Link_To_Store_Data(ent, qfalse, qtrue, qtrue, qfalse);
      continue;
    }

    if( i < MAX_CLIENTS )
    {
      G_RunClient( ent );
      if(ent->client->pers.connected == CON_CONNECTED) {
        G_Unlagged_Link_To_Store_Data(ent, qtrue, qfalse, qfalse, qtrue);
      }
      continue;
    }

    G_RunThink( ent );
  }

  // perform final fixups on the players
  ent = &g_entities[ 0 ];

  for( i = 0; i < level.maxclients; i++, ent++ )
  {
    if( ent->inuse )
      ClientEndFrame( ent );
  }

  // save position information for all active clients and other shootable entities
  G_UnlaggedStore( );

  G_CountBuildables( );
  if( IS_WARMUP ||
      !g_doCountdown.integer ||
      level.countdownTime <= level.time )
  {
    G_CalculateBuildPoints( );
    G_CalculateAvgPlayers( );
    G_CalculateStages( );
    for(i = 0; i < NUM_TEAMS; i++) {
      BG_List_Foreach(&level.spawn_queue[i], NULL, G_SpawnClients, NULL);
    }
    G_UpdateZaps( msec );
  }

  //get all of the clients ready for the fight announcement
  if(level.fight) {
    if(!g_restartingFlags.integer) {
      if(level.countdownTime > 0) {
        if(level.countdownTime <= (level.time + 1000)) {
          for(i = 0; i < level.maxclients; i++) {
            gentity_t *ent2 = &g_entities[i];

            if(ent2->client->pers.connected == CON_CONNECTED ||
               ent2->client->pers.connected == CON_CONNECTING) {
              ent2->client->pers.fight = qtrue;
            }
          }

          level.fight = qfalse;
        }
      } else {
        level.fight = 0;
      }
    }
  } else {
    level.fight = 0;
  }

  G_UpdateBuildableRangeMarkers( );

  // check for warmup timeout
  G_LevelReady( );

  // see if it is time to end the level
  CheckExitRules( );

  // update to team status?
  CheckTeamStatus( );

  // cancel vote if timed out
  for( i = 0; i < NUM_TEAMS; i++ )
    G_CheckVote( i );

  level.frameMsec = Sys_Milliseconds();
}

/*
================
dllEntry

Needed to load dll
================
*/
Q_EXPORT void dllEntry( intptr_t (QDECL *syscallptr)( intptr_t arg,... ) )
{
}
