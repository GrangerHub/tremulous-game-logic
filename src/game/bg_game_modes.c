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

#include "../qcommon/q_shared.h"
#include "bg_public.h"

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

/*
================
BG_Check_Game_Mode_Name

Sanitizes the raw game mode name, and returns a qboolean that inidcates if the
game mode exists.
================
*/
qboolean BG_Check_Game_Mode_Name(
  char          *game_mode_raw, char *game_mode_clean, int len) {
  fileHandle_t  f;

  while(*game_mode_raw && len > 0) {
    if(Q_IsColorString( game_mode_raw)) {
      game_mode_raw += Q_ColorStringLength(game_mode_raw);    // skip color code
      continue;
    } else if(Q_IsColorEscapeEscape(game_mode_raw)) {
      game_mode_raw++;
    }

    if(isalnum(*game_mode_raw)) {
        *game_mode_clean++ = tolower(*game_mode_raw);
        len--;
    }
    game_mode_raw++;
  }
  *game_mode_clean = 0;

  len = FS_FOpenFileByMode(va("game_modes/%s/game_mode.cfg", game_mode_clean), &f, FS_READ);
  if(len < 0) {
    return qfalse;
  } else {
    return qtrue;
  }
}

static void BG_InitTeamConfigs(char *game_mode);
static void BG_InitClassConfigs(char *game_mode);
static void BG_InitBuildableConfigs(char *game_mode);
static void BG_InitUpgradeConfigs(char *game_mode);
static void BG_InitWeaponConfigs(char *game_mode);

/*
================
BG_Init_Game_Mode
================
*/
void BG_Init_Game_Mode(char *game_mode_raw) {
  char game_mode[MAX_TOKEN_CHARS];

  if(!BG_Check_Game_Mode_Name(game_mode_raw, game_mode, MAX_TOKEN_CHARS)) {
    Q_strncpyz(game_mode, DEFAULT_GAME_MODE, MAX_TOKEN_CHARS);
  }

  BG_InitTeamConfigs(game_mode);
  BG_InitClassConfigs(game_mode);
  BG_InitBuildableConfigs(game_mode);
  BG_InitUpgradeConfigs(game_mode);
  BG_InitWeaponConfigs(game_mode);
}

/*
======================================================================

Teams

======================================================================
*/

static teamConfig_t bg_teamConfigList[ NUM_TEAMS ];

/*
==============
BG_TeamConfig
==============
*/
teamConfig_t *BG_TeamConfig(team_t team) {
  return &bg_teamConfigList[team];
}

/*
======================
BG_ParseTeamFile

Parses a configuration file describing a team
======================
*/
static qboolean BG_ParseTeamFile(const char *filename, teamConfig_t *tc) {
  char          *text_p;
  int           len;
  char          *token;
  char          text[20000];
  fileHandle_t  f;
  int           defined = 0;
  enum
  {
      DESCRIPTION   = 1 << 0
  };


  // load the file
  len = FS_FOpenFileByMode(filename, &f, FS_READ);
  if(len < 0) {
    Com_Printf(S_COLOR_RED "ERROR: Team file %s doesn't exist\n", filename);
    return qfalse;
  }

  if(len == 0 || len >= sizeof(text) - 1) {
    FS_FCloseFile(f);
    Com_Printf(S_COLOR_RED "ERROR: Team file %s is %s\n", filename,
      len == 0 ? "empty" : "too long");
    return qfalse;
  }

  FS_Read2(text, len, f);
  text[len] = 0;
  FS_FCloseFile(f);

  // parse the text
  text_p = text;

  // read optional parameters
  while(1) {
    token = COM_Parse(&text_p);

    if(!token) {
      break;
    }

    if(!Q_stricmp(token, "")) {
      break;
    }

    if(!Q_stricmp(token, "description")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(tc->description, token, sizeof(tc->description));

      defined |= DESCRIPTION;
      continue;
    }


    Com_Printf( S_COLOR_RED "ERROR: unknown token '%s'\n", token );
    return qfalse;
  }

  if(     !( defined & DESCRIPTION ) )  {token = "description";}
  else                                  {token = "";}

  if(strlen( token ) > 0) {
      Com_Printf( S_COLOR_RED "ERROR: %s not defined in %s\n",
                  token, filename );
      return qfalse;
  }

return qtrue;
}

/*
===============
BG_InitTeamConfigs
===============
*/
static void BG_InitTeamConfigs(char *game_mode) {
  int               i;
  teamConfig_t *tc;

  for(i = TEAM_NONE; i < NUM_TEAMS; i++) {
    tc = BG_TeamConfig(i);
    Com_Memset(tc, 0, sizeof(teamConfig_t));

    BG_ParseTeamFile(
      va("game_modes/%s/teams/%s.cfg", game_mode, BG_Team( i )->name), tc);
  }
}

/*
======================================================================

Classes

======================================================================
*/

static classConfig_t bg_classConfigList[PCL_NUM_CLASSES];

/*
==============
BG_ClassConfig
==============
*/
classConfig_t *BG_ClassConfig(class_t class) {
  return &bg_classConfigList[class];
}

/*
======================
BG_ParseClassFile

Parses a configuration file describing a class
======================
*/
static qboolean BG_ParseClassFile(const char *filename, classConfig_t *cc) {
  char          *text_p;
  int           i;
  int           len;
  char          *token;
  char          text[20000];
  fileHandle_t  f;
  float         scale = 0.0f;
  int           defined = 0;
  enum {
      MODEL           = 1 << 0,
      DESCRIPTION     = 1 << 1,
      SKIN            = 1 << 2,
      HUD             = 1 << 3,
      MODELSCALE      = 1 << 4,
      SHADOWSCALE     = 1 << 5,
      MINS            = 1 << 6,
      MAXS            = 1 << 7,
      DEADMINS        = 1 << 8,
      DEADMAXS        = 1 << 9,
      CROUCHMAXS      = 1 << 10,
      VIEWHEIGHT      = 1 << 11,
      CVIEWHEIGHT     = 1 << 12,
      ZOFFSET         = 1 << 13,
      NAME            = 1 << 14,
      SHOULDEROFFSETS = 1 << 15
  };

  // load the file
  len = FS_FOpenFileByMode(filename, &f, FS_READ);
  if(len < 0)
    return qfalse;

  if(len == 0 || len >= sizeof(text) - 1) {
    FS_FCloseFile(f);
    Com_Printf(
      S_COLOR_RED "ERROR: Class file %s is %s\n", filename,
      len == 0 ? "empty" : "too long");
    return qfalse;
  }

  FS_Read2(text, len, f);
  text[len] = 0;
  FS_FCloseFile(f);

  // parse the text
  text_p = text;

  // read optional parameters
  while(1) {
    token = COM_Parse(&text_p);

    if(!token) {
      break;
    }

    if(!Q_stricmp(token, "")) {
      break;
    }

    if(!Q_stricmp( token, "model")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(cc->modelName, token, sizeof(cc->modelName));

      defined |= MODEL;
      continue;
    } else if(!Q_stricmp(token, "description")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(cc->description, token, sizeof(cc->description));

      defined |= DESCRIPTION;
      continue;
    } else if(!Q_stricmp(token, "skin")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(cc->skinName, token, sizeof(cc->skinName));

      defined |= SKIN;
      continue;
    } else if(!Q_stricmp(token, "hud")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(cc->hudName, token, sizeof(cc->hudName));

      defined |= HUD;
      continue;
    } else if(!Q_stricmp(token, "modelScale")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      scale = atof(token);

      if(scale < 0.0f) {
        scale = 0.0f;
      }

      cc->modelScale = scale;

      defined |= MODELSCALE;
      continue;
    } else if(!Q_stricmp(token, "shadowScale")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      scale = atof(token);

      if(scale < 0.0f) {
        scale = 0.0f;
      }

      cc->shadowScale = scale;

      defined |= SHADOWSCALE;
      continue;
    } else if(!Q_stricmp(token, "mins")) {
      for(i = 0; i <= 2; i++) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        cc->mins[i] = atof(token);
      }

      defined |= MINS;
      continue;
    } else if(!Q_stricmp(token, "maxs")) {
      for(i = 0; i <= 2; i++) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        cc->maxs[i] = atof(token);
      }

      defined |= MAXS;
      continue;
    }
    else if(!Q_stricmp(token, "deadMins"))
    {
      for(i = 0; i <= 2; i++) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        cc->deadMins[i] = atof(token);
      }

      defined |= DEADMINS;
      continue;
    } else if(!Q_stricmp( token, "deadMaxs")) {
      for(i = 0; i <= 2; i++) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        cc->deadMaxs[i] = atof(token);
      }

      defined |= DEADMAXS;
      continue;
    } else if(!Q_stricmp(token, "crouchMaxs")) {
      for(i = 0; i <= 2; i++) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        cc->crouchMaxs[i] = atof(token);
      }

      defined |= CROUCHMAXS;
      continue;
    } else if(!Q_stricmp(token, "viewheight")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      cc->viewheight = atoi(token);
      defined |= VIEWHEIGHT;
      continue;
    } else if(!Q_stricmp(token, "crouchViewheight")) {
      token = COM_Parse(&text_p);
      cc->crouchViewheight = atoi(token);
      defined |= CVIEWHEIGHT;
      continue;
    } else if(!Q_stricmp(token, "zOffset")) {
      float offset;

      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      offset = atof(token);

      cc->zOffset = offset;

      defined |= ZOFFSET;
      continue;
    } else if(!Q_stricmp(token, "name")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(cc->humanName, token, sizeof(cc->humanName));

      defined |= NAME;
      continue;
    } else if(!Q_stricmp(token, "shoulderOffsets")) {
      for(i = 0; i <= 2; i++) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        cc->shoulderOffsets[i] = atof(token);
      }

      defined |= SHOULDEROFFSETS;
      continue;
    }

    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s'\n", token);
    return qfalse;
  }

  if(      !( defined & MODEL           ) ) {token = "model";}
  else if( !( defined & DESCRIPTION     ) ) {token = "description";}
  else if( !( defined & SKIN            ) ) {token = "skin";}
  else if( !( defined & HUD             ) ) {token = "hud";}
  else if( !( defined & MODELSCALE      ) ) {token = "modelScale";}
  else if( !( defined & SHADOWSCALE     ) ) {token = "shadowScale";}
  else if( !( defined & MINS            ) ) {token = "mins";}
  else if( !( defined & MAXS            ) ) {token = "maxs";}
  else if( !( defined & DEADMINS        ) ) {token = "deadMins";}
  else if( !( defined & DEADMAXS        ) ) {token = "deadMaxs";}
  else if( !( defined & CROUCHMAXS      ) ) {token = "crouchMaxs";}
  else if( !( defined & VIEWHEIGHT      ) ) {token = "viewheight";}
  else if( !( defined & CVIEWHEIGHT     ) ) {token = "crouchViewheight";}
  else if( !( defined & ZOFFSET         ) ) {token = "zOffset";}
  else if( !( defined & NAME            ) ) {token = "name";}
  else if( !( defined & SHOULDEROFFSETS ) ) {token = "shoulderOffsets";}
  else                                      {token = "";}

  if(strlen( token ) > 0) {
      Com_Printf(
        S_COLOR_RED "ERROR: %s not defined in %s\n", token, filename);
      return qfalse;
  }

  return qtrue;
}

/*
===============
BG_InitClassConfigs
===============
*/
static void BG_InitClassConfigs(char *game_mode) {
  int           i;
  classConfig_t *cc;

  for(i = PCL_NONE; i < PCL_NUM_CLASSES; i++) {
    cc = BG_ClassConfig(i);

    BG_ParseClassFile(
      va("game_modes/%s/classes/%s.cfg", game_mode, BG_Class(i)->name), cc);
  }
}

/*
======================================================================

Buildables

======================================================================
*/

static buildableConfig_t bg_buildableConfigList[BA_NUM_BUILDABLES];

/*
==============
BG_BuildableConfig
==============
*/
buildableConfig_t *BG_BuildableConfig(buildable_t buildable) {
  return &bg_buildableConfigList[buildable];
}

/*
======================
BG_ParseBuildableFile

Parses a configuration file describing a buildable
======================
*/
static qboolean BG_ParseBuildableFile(
  const char *filename, buildableConfig_t *bc) {
  char          *text_p;
  int           i;
  int           len;
  char          *token;
  char          text[20000];
  fileHandle_t  f;
  float         scale;
  int           defined = 0;
  enum {
      MODEL         = 1 << 0,
      DESCRIPTION   = 1 << 1,
      MODELSCALE    = 1 << 2,
      MINS          = 1 << 3,
      MAXS          = 1 << 4,
      ZOFFSET       = 1 << 5
  };


  // load the file
  len = FS_FOpenFileByMode(filename, &f, FS_READ);
  if(len < 0) {
    Com_Printf(S_COLOR_RED "ERROR: Buildable file %s doesn't exist\n", filename);
    return qfalse;
  }

  if(len == 0 || len >= sizeof( text ) - 1) {
    FS_FCloseFile(f);
    Com_Printf(
      S_COLOR_RED "ERROR: Buildable file %s is %s\n", filename,
      len == 0 ? "empty" : "too long");
    return qfalse;
  }

  FS_Read2(text, len, f);
  text[len] = 0;
  FS_FCloseFile(f);

  // parse the text
  text_p = text;

  // read optional parameters
  while(1) {
    token = COM_Parse(&text_p);

    if(!token) {
      break;
    }

    if(!Q_stricmp( token, "")) {
      break;
    }

    if(!Q_stricmp(token, "model")) {
      int index = 0;

      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      index = atoi(token);

      if(index < 0) {
        index = 0;
      } else if(index > 3) {
        index = 3;
      }

      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(bc->models[index], token, sizeof(bc->models[0]));

      defined |= MODEL;
      continue;
    } else if(!Q_stricmp(token, "description")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(bc->description, token, sizeof(bc->description));

      defined |= DESCRIPTION;
      continue;
    } else if(!Q_stricmp(token, "modelScale")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      scale = atof(token);

      if(scale < 0.0f) {
        scale = 0.0f;
      }

      bc->modelScale = scale;

      defined |= MODELSCALE;
      continue;
    } else if(!Q_stricmp(token, "mins")) {
      for(i = 0; i <= 2; i++) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        bc->mins[i] = atof(token);
      }

      defined |= MINS;
      continue;
    } else if(!Q_stricmp(token, "maxs")) {
      for(i = 0; i <= 2; i++) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        bc->maxs[i] = atof(token);
      }

      defined |= MAXS;
      continue;
    } else if(!Q_stricmp(token, "zOffset")) {
      float offset;

      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      offset = atof(token);

      bc->zOffset = offset;

      defined |= ZOFFSET;
      continue;
    }


    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s'\n", token);
    return qfalse;
  }

  if(      !( defined & MODEL       ) )  {token = "model";}
  else if( !( defined & DESCRIPTION ) )  {token = "description";}
  else if( !( defined & MODELSCALE  ) )  {token = "modelScale";}
  else if( !( defined & MINS        ) )  {token = "mins";}
  else if( !( defined & MAXS        ) )  {token = "maxs";}
  else if( !( defined & ZOFFSET     ) )  {token = "zOffset";}
  else                                   {token = "";}

  if(strlen(token) > 0) {
      Com_Printf(
        S_COLOR_RED "ERROR: %s not defined in %s\n", token, filename);
      return qfalse;
  }

  return qtrue;
}

/*
===============
BG_InitBuildableConfigs
===============
*/
static void BG_InitBuildableConfigs(char *game_mode) {
  int               i;
  buildableConfig_t *bc;

  for(i = BA_NONE + 1; i < BA_NUM_BUILDABLES; i++) {
    bc = BG_BuildableConfig(i);
    Com_Memset(bc, 0, sizeof(buildableConfig_t));

    BG_ParseBuildableFile(
      va("game_modes/%s/buildables/%s.cfg", game_mode, BG_Buildable(i)->name),
      bc);
  }
}

/*
======================================================================

Weapons

======================================================================
*/

static weaponConfig_t bg_weaponConfigList[WP_NUM_WEAPONS];

/*
==============
BG_WeaponConfig
==============
*/
weaponConfig_t *BG_WeaponConfig(weapon_t weapon) {
  return &bg_weaponConfigList[weapon];
}

/*
======================
BG_ParseWeaponFile

Parses a configuration file describing a weapon
======================
*/
static qboolean BG_ParseWeaponFile(const char *filename, weaponConfig_t *wc) {
  char          *text_p;
  int           len;
  char          *token;
  char          text[20000];
  fileHandle_t  f;
  int           defined = 0;
  enum {
      DESCRIPTION   = 1 << 0
  };


  // load the file
  len = FS_FOpenFileByMode(filename, &f, FS_READ);
  if(len < 0) {
    Com_Printf(S_COLOR_RED "ERROR: Weapon file %s doesn't exist\n", filename);
    return qfalse;
  }

  if(len == 0 || len >= sizeof( text ) - 1) {
    FS_FCloseFile(f);
    Com_Printf(
      S_COLOR_RED "ERROR: Weapon file %s is %s\n", filename,
      len == 0 ? "empty" : "too long");
    return qfalse;
  }

  FS_Read2(text, len, f);
  text[len] = 0;
  FS_FCloseFile(f);

  // parse the text
  text_p = text;

  // read optional parameters
  while(1) {
    token = COM_Parse(&text_p);

    if(!token) {
      break;
    }

    if(!Q_stricmp(token, "")) {
      break;
    }

    if(!Q_stricmp(token, "description")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(wc->description, token, sizeof(wc->description));

      defined |= DESCRIPTION;
      continue;
    }


    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s'\n", token);
    return qfalse;
  }

  if(     !( defined & DESCRIPTION ) )  {token = "description";}
  else                                  {token = "";}

  if(strlen(token) > 0) {
      Com_Printf(
        S_COLOR_RED "ERROR: %s not defined in %s\n", token, filename);
      return qfalse;
  }

return qtrue;
}

/*
===============
BG_InitWeaponConfigs
===============
*/
static void BG_InitWeaponConfigs(char *game_mode) {
  int               i;
  weaponConfig_t    *wc;

  for(i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++) {
    wc = BG_WeaponConfig(i);
    Com_Memset(wc, 0, sizeof(weaponConfig_t));

    BG_ParseWeaponFile(
      va("game_modes/%s/weapons/%s.cfg", game_mode, BG_Weapon(i)->name), wc);
  }
}

/*
======================================================================

Upgrades

======================================================================
*/

static upgradeConfig_t bg_upgradeConfigList[UP_NUM_UPGRADES];

/*
==============
BG_UpgradeConfig
==============
*/
upgradeConfig_t *BG_UpgradeConfig(upgrade_t upgrade) {
  return &bg_upgradeConfigList[upgrade];
}

/*
======================
BG_ParseUpgradeFile

Parses a configuration file describing an upgrade
======================
*/
static qboolean BG_ParseUpgradeFile(const char *filename, upgradeConfig_t *uc) {
  char          *text_p;
  int           len;
  char          *token;
  char          text[20000];
  fileHandle_t  f;
  int           defined = 0;
  enum {
      DESCRIPTION   = 1 << 0
  };


  // load the file
  len = FS_FOpenFileByMode(filename, &f, FS_READ);
  if(len < 0) {
    Com_Printf(S_COLOR_RED "ERROR: Upgrade file %s doesn't exist\n", filename);
    return qfalse;
  }

  if(len == 0 || len >= sizeof(text) - 1) {
    FS_FCloseFile(f);
    Com_Printf(
      S_COLOR_RED "ERROR: Upgrade file %s is %s\n", filename,
      len == 0 ? "empty" : "too long");
    return qfalse;
  }

  FS_Read2(text, len, f);
  text[ len ] = 0;
  FS_FCloseFile(f);

  // parse the text
  text_p = text;

  // read optional parameters
  while(1) {
    token = COM_Parse(&text_p);

    if(!token) {
      break;
    }

    if(!Q_stricmp(token, "")) {
      break;
    }

    if(!Q_stricmp(token, "description")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(uc->description, token, sizeof(uc->description));

      defined |= DESCRIPTION;
      continue;
    }


    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s'\n", token);
    return qfalse;
  }

  if(     !( defined & DESCRIPTION ) )  {token = "description";}
  else                                  {token = "";}

  if(strlen( token ) > 0) {
      Com_Printf(
        S_COLOR_RED "ERROR: %s not defined in %s\n", token, filename);
      return qfalse;
  }

return qtrue;
}

/*
===============
BG_InitUpgradeConfigs
===============
*/
static void BG_InitUpgradeConfigs(char *game_mode) {
  int               i;
  upgradeConfig_t    *uc;

  for(i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++) {
    uc = BG_UpgradeConfig(i);
    Com_Memset(uc, 0, sizeof( upgradeConfig_t ));

    BG_ParseUpgradeFile(
      va("game_modes/%s/upgrades/%s.cfg", game_mode, BG_Upgrade(i)->name), uc);
  }
}
