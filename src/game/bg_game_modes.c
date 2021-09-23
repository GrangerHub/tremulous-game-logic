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
  char          *game_mode_clean_ptr = game_mode_clean;
  fileHandle_t  f;

  while(*game_mode_raw && len > 0) {
    if(Q_IsColorString( game_mode_raw)) {
      game_mode_raw += Q_ColorStringLength(game_mode_raw);    // skip color code
      continue;
    } else if(Q_IsColorEscapeEscape(game_mode_raw)) {
      game_mode_raw++;
    }

    if(isalnum(*game_mode_raw)) {
        *(game_mode_clean_ptr++) = tolower(*game_mode_raw);
        len--;
    }
    game_mode_raw++;
  }
  *game_mode_clean_ptr = 0;

  len = FS_FOpenFileByMode(va("game_modes/%s/game_mode.cfg", game_mode_clean), &f, FS_READ);
  if(len < 0) {
    return qfalse;
  } else {
    FS_FCloseFile(f);
    return qtrue;
  }
}

static void BG_InitTeamConfigs(char *game_mode);
static void BG_InitMODConfigs(char *game_mode);
static void BG_InitClassConfigs(char *game_mode);
static void BG_InitBuildableConfigs(char *game_mode);
static void BG_InitUpgradeConfigs(char *game_mode);
static void BG_InitWeaponConfigs(char *game_mode);

static char game_mode[MAX_TOKEN_CHARS];

/*
================
BG_Is_Current_Game_Mode
================
*/
qboolean BG_Is_Current_Game_Mode(const char *game_mode_raw_check) {
  char       game_mode_check[MAX_TOKEN_CHARS];
  char       *game_mode_check_ptr = game_mode_check;
  const char *game_mode_raw_check_ptr = game_mode_raw_check;
  int        len = MAX_TOKEN_CHARS;

  //clean game_mode_raw_check
  while(*game_mode_raw_check_ptr && len > 0) {
    if(Q_IsColorString( game_mode_raw_check_ptr)) {
      game_mode_raw_check_ptr += Q_ColorStringLength(game_mode_raw_check_ptr);    // skip color code
      continue;
    } else if(Q_IsColorEscapeEscape(game_mode_raw_check_ptr)) {
      game_mode_raw_check_ptr++;
    }

    if(isalnum(*game_mode_raw_check_ptr)) {
        *(game_mode_check_ptr++) = tolower(*game_mode_raw_check_ptr);
        len--;
    }
    game_mode_raw_check_ptr++;
  }
  *game_mode_check_ptr = 0;

  return !Q_stricmp(game_mode_check, game_mode);
}

/*
================
BG_Init_Game_Mode
================
*/
void BG_Init_Game_Mode(char *game_mode_raw) {
  if(!BG_Check_Game_Mode_Name(game_mode_raw, game_mode, MAX_TOKEN_CHARS)) {
    Q_strncpyz(game_mode, DEFAULT_GAME_MODE, MAX_TOKEN_CHARS);
  }

  BG_InitTeamConfigs(game_mode);
  BG_InitMODConfigs(game_mode);
  BG_InitClassConfigs(game_mode);
  BG_InitBuildableConfigs(game_mode);
  BG_InitUpgradeConfigs(game_mode);
  BG_InitWeaponConfigs(game_mode);
}

/*
======================================================================

Parse Utilities

======================================================================
*/

/*
==============
BG_Parse_String
==============
*/
static qboolean BG_Parse_String(char *string, char **text_p, size_t string_size) {
  char  *token;

  Com_Assert(string);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(S_COLOR_RED "ERROR: expected string token\n");
    return qfalse;
  }

  Q_strncpyz(string, token, string_size);
  return qtrue;
}

/*
==============
BG_Parse_Bool
==============
*/
static qboolean BG_Parse_Bool(qboolean *bool_var, char **text_p) {
  char  *token;

  Com_Assert(bool_var);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(S_COLOR_RED "ERROR: expected boolean token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "")) {
    Com_Printf(S_COLOR_RED "ERROR: expected boolean token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "true")) {
    *bool_var = qtrue;
    return qtrue;
  } else if(!Q_stricmp(token, "false")) {
    *bool_var = qfalse;
    return qtrue;
  }

  Com_Printf(S_COLOR_RED "ERROR: unknown token '%s', expected boolean token\n", token);
  return qfalse;
}

/*
==============
BG_Parse_Integer
==============
*/
static qboolean BG_Parse_Integer(
  int *int_var, char **text_p, qboolean nonnegative) {
  char  *token;

  Com_Assert(int_var);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(S_COLOR_RED "ERROR: expected integer token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "")) {
    Com_Printf(S_COLOR_RED "ERROR: expected integer token\n");
    return qfalse;
  }

  *int_var = atoi(token);

  if(nonnegative) {
    if(*int_var < 0) {
      *int_var = 0;
    }
  }

  return qtrue;
}

/*
==============
BG_Parse_Float
==============
*/
static qboolean BG_Parse_Float(
  float *float_var, char **text_p, qboolean nonnegative) {
  char  *token;

  Com_Assert(float_var);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(S_COLOR_RED "ERROR: expected float token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "")) {
    Com_Printf(S_COLOR_RED "ERROR: expected float token\n");
    return qfalse;
  }

  *float_var = atof(token);

  if(nonnegative) {
    if(*float_var < 0.0) {
      *float_var = 0.0;
    }
  }

  return qtrue;
}

/*
==============
BG_Parse_Vect2
==============
*/
/*
static qboolean BG_Parse_Vect2(vec2_t *vect_var, char **text_p) {
  char  *token;
  int   i;

  Com_Assert(vect_var);

  for (i = 0; i < 2; i++) {
    token = COM_Parse(text_p);
    if(!token) {
      Com_Printf(S_COLOR_RED "ERROR: expected vector 2 token\n");
      return qfalse;
    }

    if(!Q_stricmp(token, "")) {
      Com_Printf(S_COLOR_RED "ERROR: expected vector 2 token\n");
      return qfalse;
    }

    (*vect_var)[i] = atof(token);
  }

  return qtrue;
}
*/

/*
==============
BG_Parse_Vect3
==============
*/
static qboolean BG_Parse_Vect3(vec3_t *vect_var, char **text_p) {
  char  *token;
  int   i;

  Com_Assert(vect_var);

  for (i = 0; i < 3; i++) {
    token = COM_Parse(text_p);
    if(!token) {
      Com_Printf(S_COLOR_RED "ERROR: expected vector 3 token\n");
      return qfalse;
    }

    if(!Q_stricmp(token, "")) {
      Com_Printf(S_COLOR_RED "ERROR: expected vector 3 token\n");
      return qfalse;
    }

    (*vect_var)[i] = atof(token);
  }

  return qtrue;
}

/*
==============
BG_Parse_Vect4
==============
*/
/*
static qboolean BG_Parse_Vect4(vec4_t *vect_var, char **text_p) {
  char  *token;
  int   i;

  Com_Assert(vect_var);

  for (i = 0; i < 4; i++) {
    token = COM_Parse(text_p);
    if(!token) {
      Com_Printf(S_COLOR_RED "ERROR: expected vector 4 token\n");
      return qfalse;
    }

    if(!Q_stricmp(token, "")) {
      Com_Printf(S_COLOR_RED "ERROR: expected vector 4 token\n");
      return qfalse;
    }

    (*vect_var)[i] = atof(token);
  }

  return qtrue;
}
*/

/*
==============
BG_Parse_Vect5
==============
*/
/*
static qboolean BG_Parse_Vect5(vec5_t *vect_var, char **text_p) {
  char  *token;
  int   i;

  Com_Assert(vect_var);

  for (i = 0; i < 5; i++) {
    token = COM_Parse(text_p);
    if(!token) {
      Com_Printf(S_COLOR_RED "ERROR: expected vector 5 token\n");
      return qfalse;
    }

    if(!Q_stricmp(token, "")) {
      Com_Printf(S_COLOR_RED "ERROR: expected vector 5 token\n");
      return qfalse;
    }

    (*vect_var)[i] = atof(token);
  }

  return qtrue;
}
*/

typedef const char *(*BG_Enum_Name)(const int num);

/*
==============
BG_Parse_Enum
==============
*/
static qboolean BG_Parse_Enum(
  char **text_p, int *enum_var, const int max_enum,
  BG_Enum_Name func, const char *error_str) {
  char *token;
  int  num;

  Com_Assert(enum_var);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(error_str);
    return qfalse;
  }

  for (num = 0; num < max_enum; num++) {
    if(!Q_stricmp(token, func(num))) {
      *enum_var = num;
      break;
    }
  }

  return qtrue;
}

/*
==============
BG_MOD_Name
==============
*/
static const char *BG_MOD_Name(const int num) {
  meansOfDeath_t mod = (meansOfDeath_t)num;
  return BG_MOD(mod)->name;
}

/*
==============
BG_Parse_Means_Of_Death
==============
*/
static qboolean BG_Parse_Means_Of_Death(
  meansOfDeath_t *mod_var, char **text_p) {
    return BG_Parse_Enum(
      text_p, (int *)mod_var, NUM_MODS, BG_MOD_Name,
      S_COLOR_RED "ERROR: expected means of death token\n");
}

/*
==============
BG_Parse_Traj_Type
==============
*/
static qboolean BG_Parse_Traj_Type(trType_t *trType_var, char **text_p) {
  char  *token;

  Com_Assert(trType_var);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(S_COLOR_RED "ERROR: expected trajectory type token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "")) {
    Com_Printf(S_COLOR_RED "ERROR: expected trajectory type token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "TR_STATIONARY")) {
    *trType_var = TR_STATIONARY;
    return qtrue;
  } else if(!Q_stricmp(token, "TR_INTERPOLATE")) {
    *trType_var = TR_INTERPOLATE;
    return qtrue;
  } else if(!Q_stricmp(token, "TR_LINEAR")) {
    *trType_var = TR_LINEAR;
    return qtrue;
  } else if(!Q_stricmp(token, "TR_LINEAR_STOP")) {
    *trType_var = TR_LINEAR_STOP;
    return qtrue;
  } else if(!Q_stricmp(token, "TR_SINE")) {
    *trType_var = TR_SINE;
    return qtrue;
  } else if(!Q_stricmp(token, "TR_GRAVITY")) {
    *trType_var = TR_GRAVITY;
    return qtrue;
  } else if(!Q_stricmp(token, "TR_BUOYANCY")) {
    *trType_var = TR_BUOYANCY;
    return qtrue;
  } else if(!Q_stricmp(token, "TR_ACCEL")) {
    *trType_var = TR_ACCEL;
    return qtrue;
  } else if(!Q_stricmp(token, "TR_HALF_GRAVITY")) {
    *trType_var = TR_HALF_GRAVITY;
    return qtrue;
  }

  Com_Printf(S_COLOR_RED "ERROR: unknown token '%s', expected trajectory type token\n", token);
  return qfalse;
}

/*
==============
BG_Parse_Bounce_Type
==============
*/
static qboolean BG_Parse_Bounce_Type(bounce_t *bounce_var, char **text_p) {
  char  *token;

  Com_Assert(bounce_var);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(S_COLOR_RED "ERROR: expected bounce type token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "")) {
    Com_Printf(S_COLOR_RED "ERROR: expected bounce type token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "BOUNCE_NONE")) {
    *bounce_var = BOUNCE_NONE;
    return qtrue;
  } else if(!Q_stricmp(token, "BOUNCE_HALF")) {
    *bounce_var = BOUNCE_HALF;
    return qtrue;
  } else if(!Q_stricmp(token, "BOUNCE_FULL")) {
    *bounce_var = BOUNCE_FULL;
    return qtrue;
  }

  Com_Printf(S_COLOR_RED "ERROR: unknown token '%s', expected bounce type token\n", token);
  return qfalse;
}

/*
==============
BG_Parse_Portal_Type
==============
*/
static qboolean BG_Parse_Portal_Type(portal_t *portal_var, char **text_p) {
  char  *token;

  Com_Assert(portal_var);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(S_COLOR_RED "ERROR: expected portal type token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "")) {
    Com_Printf(S_COLOR_RED "ERROR: expected portal type token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "PORTAL_NONE")) {
    *portal_var = PORTAL_NONE;
    return qtrue;
  } else if(!Q_stricmp(token, "PORTAL_BLUE")) {
    *portal_var = PORTAL_BLUE;
    return qtrue;
  } else if(!Q_stricmp(token, "PORTAL_RED")) {
    *portal_var = PORTAL_RED;
    return qtrue;
  }

  Com_Printf(S_COLOR_RED "ERROR: unknown token '%s', expected portal type token\n", token);
  return qfalse;
}

/*
==============
BG_Parse_Impede_Move_Type
==============
*/
static qboolean BG_Parse_Impede_Move_Type(
  impede_move_t *impeade_move_var, char **text_p) {
  char  *token;

  Com_Assert(impeade_move_var);

  token = COM_Parse(text_p);
  if(!token) {
    Com_Printf(S_COLOR_RED "ERROR: expected impede movement type token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "")) {
    Com_Printf(S_COLOR_RED "ERROR: expected impede movement type token\n");
    return qfalse;
  }

  if(!Q_stricmp(token, "IMPEDE_MOVE_NONE")) {
    *impeade_move_var = IMPEDE_MOVE_NONE;
    return qtrue;
  } else if(!Q_stricmp(token, "IMPEDE_MOVE_SLOW")) {
    *impeade_move_var = IMPEDE_MOVE_SLOW;
    return qtrue;
  } else if(!Q_stricmp(token, "IMPEDE_MOVE_LOCK")) {
    *impeade_move_var = IMPEDE_MOVE_LOCK;
    return qtrue;
  }

  Com_Printf(S_COLOR_RED "ERROR: unknown token '%s', expected impede movement type token\n", token);
  return qfalse;
}

/*
==============
BG_Parse_Contents
==============
*/
static qboolean BG_Parse_Contents(int *contents, char **text_p) {
  char  *token;

  Com_Assert(contents);

  *contents = 0;

  while(1) {
    token = COM_Parse(text_p);
    if(!token){
      return qtrue;
    }

    if(!Q_stricmp(token, "")) {
      Com_Printf(S_COLOR_RED "ERROR: expected contents or mask token\n");
      return qfalse;
    }

    if(!Q_stricmp(token, "MASK_ALL")) {
      *contents = MASK_ALL;
      return qtrue;
    } else if(!Q_stricmp(token, "0")) {
      continue;
    } else if(!Q_stricmp(token, "MASK_SOLID")) {
      *contents |= MASK_SOLID;
      continue;
    } else if(!Q_stricmp(token, "MASK_PLAYERSOLID")) {
      *contents |= MASK_PLAYERSOLID;
      continue;
    } else if(!Q_stricmp(token, "MASK_DEADSOLID")) {
      *contents |= MASK_DEADSOLID;
      continue;
    } else if(!Q_stricmp(token, "MASK_WATER")) {
      *contents |= MASK_WATER;
      continue;
    } else if(!Q_stricmp(token, "MASK_OPAQUE")) {
      *contents |= MASK_OPAQUE;
      continue;
    } else if(!Q_stricmp(token, "MASK_SHOT")) {
      *contents |= MASK_SHOT;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_SOLID")) {
      *contents |= CONTENTS_SOLID;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_ASTRAL")) {
      *contents |= CONTENTS_ASTRAL;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_LAVA")) {
      *contents |= CONTENTS_LAVA;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_SLIME")) {
      *contents |= CONTENTS_SLIME;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_WATER")) {
      *contents |= CONTENTS_WATER;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_FOG")) {
      *contents |= CONTENTS_FOG;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_NOTTEAM1")) {
      *contents |= CONTENTS_NOTTEAM1;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_NOTTEAM2")) {
      *contents |= CONTENTS_NOTTEAM2;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_NOBOTCLIP")) {
      *contents |= CONTENTS_NOBOTCLIP;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_DOOR")) {
      *contents |= CONTENTS_DOOR;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_AREAPORTAL")) {
      *contents |= CONTENTS_AREAPORTAL;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_PLAYERCLIP")) {
      *contents |= CONTENTS_PLAYERCLIP;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_MONSTERCLIP")) {
      *contents |= CONTENTS_MONSTERCLIP;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_TELEPORTER")) {
      *contents |= CONTENTS_TELEPORTER;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_JUMPPAD")) {
      *contents |= CONTENTS_JUMPPAD;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_CLUSTERPORTAL")) {
      *contents |= CONTENTS_CLUSTERPORTAL;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_DONOTENTER")) {
      *contents |= CONTENTS_DONOTENTER;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_BOTCLIP")) {
      *contents |= CONTENTS_BOTCLIP;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_MOVER")) {
      *contents |= CONTENTS_MOVER;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_ORIGIN")) {
      *contents |= CONTENTS_ORIGIN;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_BODY")) {
      *contents |= CONTENTS_BODY;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_CORPSE")) {
      *contents |= CONTENTS_CORPSE;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_DETAIL")) {
      *contents |= CONTENTS_DETAIL;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_STRUCTURAL")) {
      *contents |= CONTENTS_STRUCTURAL;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_TRANSLUCENT")) {
      *contents |= CONTENTS_TRANSLUCENT;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_TRIGGER")) {
      *contents |= CONTENTS_TRIGGER;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_NODROP")) {
      *contents |= CONTENTS_NODROP;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_NOALIENBUILD")) {
      *contents |= CONTENTS_NOALIENBUILD;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_NOHUMANBUILD")) {
      *contents |= CONTENTS_NOHUMANBUILD;
      continue;
    } else if(!Q_stricmp(token, "CONTENTS_NOBUILD")) {
      *contents |= CONTENTS_NOBUILD;
      continue;
    } else if(!Q_stricmp(token, "}")) {
      return qtrue; // reached the end of the contents
    }

    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s', expected contents or mask token\n", token);
    return qfalse;
  }

  return qfalse;
}/*
==============
BG_Parse_Clip_Mask
==============
*/
static qboolean BG_Parse_Clip_Mask(content_mask_t *clip_mask, char **text_p) {
  char  *token;
  char  substruct[MAX_TOKEN_CHARS] = "";

  Com_Assert(clip_mask);

  clip_mask->exclude = 0;
  clip_mask->include = 0;

  while(1) {
    token = COM_Parse(text_p);
    if(!token){
      break;
    }

    if(!Q_stricmp(token, "")) {
      Com_Printf(S_COLOR_RED "ERROR: expected exclude or include token\n");
      return qfalse;
    }

    if(!Q_stricmp(token, "{")) {
      if(!Q_stricmp(substruct, "")) {
        Com_Printf( S_COLOR_RED "ERROR: clip_mask contents parsing started without a declaration\n" );
        return qfalse;
      } else if(!Q_stricmp(substruct, "exclude")) {
        if(!BG_Parse_Contents(&clip_mask->exclude, text_p)) {
          Com_Printf( S_COLOR_RED "ERROR: failed to parse exclude contents\n" );
          return qfalse;
        }
      } else if(!Q_stricmp(substruct, "include")) {
        if(!BG_Parse_Contents(&clip_mask->include, text_p)) {
          Com_Printf( S_COLOR_RED "ERROR: failed to parse include contents\n" );
          return qfalse;
        }
      } else {
        Com_Printf(S_COLOR_RED "ERROR: unknown substruct '%s'\n", substruct);
        return qfalse;
      }

      substruct[0] = '\0';

      continue;
    } else if(!Q_stricmp(token, "exclude")) {
      Q_strncpyz(substruct, token, sizeof(substruct));
      continue;
    } else if(!Q_stricmp(token, "include")) {
      Q_strncpyz(substruct, token, sizeof(substruct));
      continue;
    } else if(!Q_stricmp(token, "}")) {
      return qtrue; // reached the end of the clip mask
    }

    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s', expected exclude or include token\n", token);
    return qfalse;
  }

  return qfalse;
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
const teamConfig_t *BG_TeamConfig(team_t team) {
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
    tc = &bg_teamConfigList[i];
    Com_Memset(tc, 0, sizeof(teamConfig_t));

    BG_ParseTeamFile(
      va("game_modes/%s/teams/%s.cfg", game_mode, BG_Team( i )->name), tc);
  }
}

/*
======================================================================

Means of Death

======================================================================
*/

static modConfig_t bg_modConfigList[NUM_MODS];

/*
==============
BG_MODConfig
==============
*/
const modConfig_t *BG_MODConfig(meansOfDeath_t mod) {
  return &bg_modConfigList[mod];
}

/*
======================
BG_ParseMODFile

Parses a configuration file describing a means of death
======================
*/
static qboolean BG_ParseMODFile(const char *filename, modConfig_t *mc) {
  char          *text_p;
  int           len;
  char          *token;
  char          text[20000];
  fileHandle_t  f;

  // load the file
  len = FS_FOpenFileByMode(filename, &f, FS_READ);
  if(len < 0) {
    Com_Printf(S_COLOR_RED "ERROR: Means of death file %s doesn't exist\n", filename);
    return qfalse;
  }

  if(len == 0 || len >= sizeof(text) - 1) {
    FS_FCloseFile(f);
    Com_Printf(S_COLOR_RED "ERROR: Means of death file %s is %s\n", filename,
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

    if(!Q_stricmp(token, "single_message")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(
        mc->single_message,
        token,
        sizeof(mc->single_message));

      continue;
    }

    if(!Q_stricmp(token, "suicide_message")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      if(!Q_stricmp(token, "female")) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        Q_strncpyz(
          mc->suicide_message[GENDER_FEMALE],
          token,
          sizeof(mc->suicide_message[GENDER_FEMALE]));

        continue;
      } else if(!Q_stricmp(token, "male")) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        Q_strncpyz(
          mc->suicide_message[GENDER_MALE],
          token,
          sizeof(mc->suicide_message[GENDER_MALE]));

        continue;
      } else if(!Q_stricmp(token, "neuter")) {
        token = COM_Parse(&text_p);
        if(!token) {
          break;
        }

        Q_strncpyz(
          mc->suicide_message[GENDER_NEUTER],
          token,
          sizeof(mc->suicide_message[GENDER_NEUTER]));

        continue;
      } else {
        Com_Printf(
          S_COLOR_RED "ERROR: Means of death file %s has unknown gender %s\n",
          filename,
          token);
        return qfalse;
      }
    }

    if(!Q_stricmp(token, "message1")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(
        mc->message1,
        token,
        sizeof(mc->message1));

      continue;
    }

    if(!Q_stricmp(token, "message2")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(
        mc->message2,
        token,
        sizeof(mc->message2));

      continue;
    }

    Com_Printf( S_COLOR_RED "ERROR: unknown token '%s'\n", token );
    return qfalse;
  }

  return qtrue;
}

/*
===============
BG_InitMODConfigs
===============
*/
static void BG_InitMODConfigs(char *game_mode) {
  int               i;
  modConfig_t *mc;

  for(i = 0; i < NUM_MODS; i++) {
    mc = &bg_modConfigList[i];
    Com_Memset(mc, 0, sizeof(teamConfig_t));

    BG_ParseMODFile(
      va(
        "game_modes/%s/means_of_death/%s.cfg",
        game_mode,
        BG_MOD( i )->name),
      mc);
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
const classConfig_t *BG_ClassConfig(class_t class) {
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
      MODEL            = 1 << 0,
      DESCRIPTION      = 1 << 1,
      SKIN             = 1 << 2,
      HUD              = 1 << 3,
      MODELSCALE       = 1 << 4,
      SHADOWSCALE      = 1 << 5,
      MINS             = 1 << 6,
      MAXS             = 1 << 7,
      DEADMINS         = 1 << 8,
      DEADMAXS         = 1 << 9,
      CROUCHMAXS       = 1 << 10,
      VIEWHEIGHT       = 1 << 11,
      CVIEWHEIGHT      = 1 << 12,
      ZOFFSET          = 1 << 13,
      NAME             = 1 << 14,
      SHOULDEROFFSETS  = 1 << 15,
      SILENCEFOOTSTEPS = 1 << 16
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
    } else if(!Q_stricmp(token, "silenceFootsteps")) {
      if(BG_Parse_Bool(&cc->silenceFootsteps, &text_p)) {
        defined |= SILENCEFOOTSTEPS;
      }

      continue;
    }

    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s'\n", token);
    return qfalse;
  }

  if(      !( defined & MODEL            ) ) {token = "model";}
  else if( !( defined & DESCRIPTION      ) ) {token = "description";}
  else if( !( defined & SKIN             ) ) {token = "skin";}
  else if( !( defined & HUD              ) ) {token = "hud";}
  else if( !( defined & MODELSCALE       ) ) {token = "modelScale";}
  else if( !( defined & SHADOWSCALE      ) ) {token = "shadowScale";}
  else if( !( defined & MINS             ) ) {token = "mins";}
  else if( !( defined & MAXS             ) ) {token = "maxs";}
  else if( !( defined & DEADMINS         ) ) {token = "deadMins";}
  else if( !( defined & DEADMAXS         ) ) {token = "deadMaxs";}
  else if( !( defined & CROUCHMAXS       ) ) {token = "crouchMaxs";}
  else if( !( defined & VIEWHEIGHT       ) ) {token = "viewheight";}
  else if( !( defined & CVIEWHEIGHT      ) ) {token = "crouchViewheight";}
  else if( !( defined & ZOFFSET          ) ) {token = "zOffset";}
  else if( !( defined & NAME             ) ) {token = "name";}
  else if( !( defined & SHOULDEROFFSETS  ) ) {token = "shoulderOffsets";}
  else if( !( defined & SILENCEFOOTSTEPS ) ) {token = "silenceFootsteps";}
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
    cc = &bg_classConfigList[i];

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
const buildableConfig_t *BG_BuildableConfig(buildable_t buildable) {
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
    bc = &bg_buildableConfigList[i];
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
const weaponConfig_t *BG_WeaponConfig(weapon_t weapon) {
  return &bg_weaponConfigList[weapon];
}

static const weaponModeConfig_t weapon_mode_default =
{
  {                   //missileConfig_t missile;
    qfalse,             //qboolean       enabled;
    "",                 //char           *class_name;
    MOD_UNKNOWN,        //meansOfDeath_t mod;
    MOD_UNKNOWN,        //meansOfDeath_t splash_mod;
    qfalse,             //qboolean       selective_damage;
    0,                  //int            damage;
    0,                  //int            splash_damage;
    0.0f,               //float          splash_radius;
    qtrue,              //qboolean       point_against_world;
    {0.0f, 0.0f, 0.0f}, //vec3_t         mins;
    {0.0f, 0.0f, 0.0f}, //vec3_t         maxs;
    {0.0f, 0.0f, 0.0f}, //vec3_t         muzzle_offset;
    0.0f,               //float          speed;
    TR_LINEAR,          //trType_t       trajectory_type;
    0,                  //int            activation_delay;
    0,                  //int            explode_delay;
    qtrue,              //qboolean       explode_miss_effect;
    qfalse,             //qboolean       team_only_trail;
    BOUNCE_NONE,        //bounce_t       bounce_type;
    qtrue,              //qboolean       damage_on_bounce;
    qtrue,              //qboolean       bounce_sound;
    qfalse,             //qboolean       impact_stick;
    qfalse,             //qboolean       impact_miss_effect;
    PORTAL_NONE,        //portal_t       impact_create_portal;
    0,                  //int            health;
    0.0f,               //float          kick_up_speed;
    0,                  //int            kick_up_time;
    0.0f,               //float          tripwire_range;
    0,                  //int            tripwire_check_frequency;
    0,                  //int            search_and_destroy_change_period;
    qfalse,             //qboolean       return_after_damage;
    qtrue,              //qboolean       has_knockback;
    IMPEDE_MOVE_NONE,   //impede_move_t  impede_move;
    0,                  //int            charged_time_max;
    qfalse,             //qboolean       charged_damage;
    qfalse,             //qboolean       charged_speed;
    0,                  //int            charged_speed_min;
    0,                  //int            charged_speed_min_damage_mod;
    qfalse,             //qboolean       save_missiles;
    qfalse,             //qboolean       detonate_saved_missiles;
    100,                //int            detonate_saved_missiles_delay;
    1000,               //int            detonate_saved_missiles_repeat;
    qfalse,             //qboolean       relative_missile_speed;
    qtrue,              //qboolean       relative_missile_speed_along_aim;
    1.0f,               //float          relative_missile_speed_lag;
    {0, MASK_SHOT},     //content_mask_t clip_mask;
    qfalse,             //qboolean       scale;
    0,                  //int            scale_start_time;
    0,                  //int            scale_stop_time;
    {0.0f, 0.0f, 0.0f}, //vec3_t         scale_stop_mins;
    {0.0f, 0.0f, 0.0f}  //vec3_t         scale_stop_maxs;
  }
};



/*
==============
BG_Missile
==============
*/
const missileConfig_t *BG_Missile(weapon_t weapon, weaponMode_t mode) {
  if((mode < 0) || (mode >= WPM_NUM_WEAPONMODES)) {
    return &weapon_mode_default.missile;
  }

  return &BG_WeaponConfig(weapon)->mode[mode].missile;
}

static qboolean BG_ParseMissileConfig(missileConfig_t *missile, char **text_p)
{
  char  *token;
  char  substruct[MAX_TOKEN_CHARS] = "";

  while(1) {
    token = COM_Parse(text_p);

    if(!token) {
      break;
    }

    if(!Q_stricmp(token, "")) {
      break;
    }

    if(!Q_stricmp(token, "{")) {
      if(!Q_stricmp(substruct, "")) {
        Com_Printf( S_COLOR_RED "ERROR: missile config substruct parsing started without a declaration\n" );
        return qfalse;
      } else if(!Q_stricmp(substruct, "clip_mask")) {
        if(!BG_Parse_Clip_Mask(&missile->clip_mask, text_p)) {
          Com_Printf( S_COLOR_RED "ERROR: failed to parse clip mask\n" );
          return qfalse;
        }
      } else {
        Com_Printf(S_COLOR_RED "ERROR: unknown substruct '%s'\n", substruct);
        return qfalse;
      }

      substruct[0] = '\0';

      continue;
    } else if(!Q_stricmp(token, "enabled")) {
      if(!BG_Parse_Bool(&missile->enabled, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse enabled token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "class_name")) {
      if(
        !BG_Parse_String(
          missile->class_name, text_p, sizeof(missile->class_name))) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse  token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "means_of_death")) {
      if(!BG_Parse_Means_Of_Death(&missile->mod, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse means_of_death token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "splash_means_of_death")) {
      if(!BG_Parse_Means_Of_Death(&missile->splash_mod, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse splash_means_of_death token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "selective_damage")) {
      if(!BG_Parse_Bool(&missile->selective_damage, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse selective_damage token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "damage")) {
      if(!BG_Parse_Integer(&missile->damage, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse damage token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "splash_damage")) {
      if(!BG_Parse_Integer(&missile->splash_damage, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse splash_damage token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "splash_radius")) {
      if(!BG_Parse_Float(&missile->splash_radius, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse splash_radius token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "point_against_world")) {
      if(!BG_Parse_Bool(&missile->point_against_world, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse point_against_world token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "mins")) {
      if(!BG_Parse_Vect3(&missile->mins, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse mins token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "maxs")) {
      if(!BG_Parse_Vect3(&missile->maxs, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse maxs token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "muzzle_offset")) {
      if(!BG_Parse_Vect3(&missile->muzzle_offset, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse muzzle_offset token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "speed")) {
      if(!BG_Parse_Float(&missile->speed, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse speed token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "trajectory_type")) {
      if(!BG_Parse_Traj_Type(&missile->trajectory_type, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse trajectory_type token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "activation_delay")) {
      if(!BG_Parse_Integer(&missile->activation_delay, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse activation_delay token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "explode_delay")) {
      if(!BG_Parse_Integer(&missile->explode_delay, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse explode_delay token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "explode_miss_effect")) {
      if(!BG_Parse_Bool(&missile->explode_miss_effect, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse explode_miss_effect token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "team_only_trail")) {
      if(!BG_Parse_Bool(&missile->team_only_trail, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse team_only_trail token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "bounce_type")) {
      if(!BG_Parse_Bounce_Type(&missile->bounce_type, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse bounce_type token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "damage_on_bounce")) {
      if(!BG_Parse_Bool(&missile->damage_on_bounce, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse damage_on_bounce token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "bounce_sound")) {
      if(!BG_Parse_Bool(&missile->bounce_sound, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse bounce_sound token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "impact_stick")) {
      if(!BG_Parse_Bool(&missile->impact_stick, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse impact_stick token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "impact_miss_effect")) {
      if(!BG_Parse_Bool(&missile->impact_miss_effect, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse impact_miss_effect token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "impact_create_portal")) {
      if(!BG_Parse_Portal_Type(&missile->impact_create_portal, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse impact_create_portal token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "health")) {
      if(!BG_Parse_Integer(&missile->health, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse health token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "kick_up_speed")) {
      if(!BG_Parse_Float(&missile->kick_up_speed, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse kick_up_speed token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "kick_up_time")) {
      if(!BG_Parse_Integer(&missile->kick_up_time, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse kick_up_time token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "tripwire_range")) {
      if(!BG_Parse_Float(&missile->tripwire_range, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse tripwire_range token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "tripwire_check_frequency")) {
      if(!BG_Parse_Integer(&missile->tripwire_check_frequency, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse tripwire_check_frequency token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "search_and_destroy_change_period")) {
      if(
        !BG_Parse_Integer(
          &missile->search_and_destroy_change_period, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse search_and_destroy_change_period token\n" );
        return qfalse;
      }

      continue;
    }  else if(!Q_stricmp(token, "return_after_damage")) {
      if(!BG_Parse_Bool(&missile->return_after_damage, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse return_after_damage token\n" );
        return qfalse;
      }

      continue;
    }  else if(!Q_stricmp(token, "has_knockback")) {
      if(!BG_Parse_Bool(&missile->has_knockback, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse has_knockback token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "impede_move")) {
      if(!BG_Parse_Impede_Move_Type(&missile->impede_move, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse impede_move token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "charged_time_max")) {
      if(!BG_Parse_Integer(&missile->charged_time_max, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse charged_time_max token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "charged_damage")) {
      if(!BG_Parse_Bool(&missile->charged_damage, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse charged_damage token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "charged_speed")) {
      if(!BG_Parse_Bool(&missile->charged_speed, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse charged_speed token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "charged_speed_min")) {
      if(!BG_Parse_Integer(&missile->charged_speed_min, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse charged_speed_min token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "charged_speed_min_damage_mod")) {
      if(!BG_Parse_Integer(
        &missile->charged_speed_min_damage_mod, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse charged_speed_min_damage_mod token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "save_missiles")) {
      if(!BG_Parse_Bool(&missile->save_missiles, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse save_missiles token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "detonate_saved_missiles")) {
      if(!BG_Parse_Bool(&missile->detonate_saved_missiles, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse detonate_saved_missiles token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "detonate_saved_missiles_delay")) {
      if(!BG_Parse_Integer(&missile->detonate_saved_missiles_delay, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse detonate_saved_missiles_delay token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "detonate_saved_missiles_repeat")) {
      if(!BG_Parse_Integer(&missile->detonate_saved_missiles_repeat, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse detonate_saved_missiles_repeat token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "relative_missile_speed")) {
      if(!BG_Parse_Bool(&missile->relative_missile_speed, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse relative_missile_speed token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "relative_missile_speed_along_aim")) {
      if(!BG_Parse_Bool(&missile->relative_missile_speed_along_aim, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse relative_missile_speed_along_aim token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "relative_missile_speed_lag")) {
      if(!BG_Parse_Float(&missile->relative_missile_speed_lag, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse relative_missile_speed_lag token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "clip_mask")) {
      Q_strncpyz(substruct, token, sizeof(substruct));
      continue;
    } else if(!Q_stricmp(token, "scale")) {
      if(!BG_Parse_Bool(&missile->scale, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse scale token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "scale_start_time")) {
      if(!BG_Parse_Integer(&missile->scale_start_time, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse scale_start_time token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "scale_stop_time")) {
      if(!BG_Parse_Integer(&missile->scale_stop_time, text_p, qtrue)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse scale_stop_time token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "scale_stop_mins")) {
      if(!BG_Parse_Vect3(&missile->scale_stop_mins, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse scale_stop_mins token\n" );
        return qfalse;
      }

      continue;
    } else if(!Q_stricmp(token, "scale_stop_maxs")) {
      if(!BG_Parse_Vect3(&missile->scale_stop_maxs, text_p)) {
        Com_Printf( S_COLOR_RED "ERROR: failed to parse scale_stop_maxs token\n" );
        return qfalse;
      }

      continue;
    }  else if(!Q_stricmp(token, "}")) {
      return qtrue; // reached the end of the missile config
    }

    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s'\n", token);
    return qfalse;
  }
  return qfalse;
}

static qboolean BG_ParseWeaponModeConfig(weaponModeConfig_t *wmc, char **text_p)
{
  char  *token;
  char  substruct[MAX_TOKEN_CHARS] = "";

  while(1) {
    token = COM_Parse(text_p);

    if(!token) {
      break;
    }

    if(!Q_stricmp(token, "")) {
      break;
    }

    if(!Q_stricmp(token, "{")) {
      if(!Q_stricmp(substruct, "")) {
        Com_Printf( S_COLOR_RED "ERROR: weapon mode substruct parsing started without a declaration\n" );
        return qfalse;
      } else if(!Q_stricmp(substruct, "missile")) {
        if(!BG_ParseMissileConfig(&wmc->missile, text_p)) {
         Com_Printf(S_COLOR_RED "ERROR: failed missile configuration\n");
         return qfalse;
       }
      } else {
        Com_Printf(S_COLOR_RED "ERROR: unknown substruct '%s'\n", substruct);
        return qfalse;
      }

      substruct[0] = '\0';

      continue;
    } else if(!Q_stricmp(token, "missile")) {
      Q_strncpyz(substruct, token, sizeof(substruct));
      continue;
    }  else if(!Q_stricmp(token, "}")) {
      return qtrue; // reached the end of the weapon mode config
    }

    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s'\n", token);
    return qfalse;
  }
  return qfalse;
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
  weaponMode_t  mode = WPM_NONE;
  char          *token;
  char          text[25000];
  fileHandle_t  f;
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

    if(!Q_stricmp(token, "{")) {
      if(mode == WPM_NONE) {
        Com_Printf( S_COLOR_RED "ERROR: weapon mode section started without a declaration\n" );
        return qfalse;
      } else if(!BG_ParseWeaponModeConfig(&wc->mode[mode], &text_p)) {
        Com_Printf(S_COLOR_RED "ERROR: failed to parse weapon mode configuration for file %s\n", filename);
        return qfalse;
      }

      //start parsing ejectors again
      mode = WPM_NONE;

      continue;
    } else if(!Q_stricmp(token, "primary")) {
      mode = WPM_PRIMARY;
      continue;
    } else if(!Q_stricmp(token, "secondary")) {
      mode = WPM_SECONDARY;
      continue;
    } else if(!Q_stricmp( token, "tertiary")) {
      mode = WPM_TERTIARY;
      continue;
    } if(!Q_stricmp(token, "description")) {
      token = COM_Parse(&text_p);
      if(!token) {
        break;
      }

      Q_strncpyz(wc->description, token, sizeof(wc->description));
      continue;
    }


    Com_Printf(S_COLOR_RED "ERROR: unknown token '%s'\n", token);
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
  weaponMode_t  mode = WPM_NONE;

  for(i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++) {
    wc = &bg_weaponConfigList[i];
    Com_Memset(wc, 0, sizeof(weaponConfig_t));

    // initialize default values
    for (mode = 0; mode < WPM_NUM_WEAPONMODES; mode++) {
      wc->mode[mode] = weapon_mode_default;
    }

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
const upgradeConfig_t *BG_UpgradeConfig(upgrade_t upgrade) {
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
    uc = &bg_upgradeConfigList[i];
    Com_Memset(uc, 0, sizeof( upgradeConfig_t ));

    BG_ParseUpgradeFile(
      va("game_modes/%s/upgrades/%s.cfg", game_mode, BG_Upgrade(i)->name), uc);
  }
}
