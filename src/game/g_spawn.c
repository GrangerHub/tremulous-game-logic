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

#include "g_local.h"

qboolean G_SpawnString( const char *key, const char *defaultString, char **out )
{
  int   i;

  if( !level.spawning )
  {
    *out = (char *)defaultString;
//    Com_Error( ERR_DROP, "G_SpawnString() called while not spawning" );
    return qfalse;
  }

  for( i = 0; i < level.numSpawnVars; i++ )
  {
    if( !Q_stricmp( key, level.spawnVars[ i ][ 0 ] ) )
    {
      *out = level.spawnVars[ i ][ 1 ];
      return qtrue;
    }
  }

  *out = (char *)defaultString;
  return qfalse;
}

qboolean  G_SpawnFloat( const char *key, const char *defaultString, float *out )
{
  char    *s;
  qboolean  present;

  present = G_SpawnString( key, defaultString, &s );
  *out = atof( s );
  return present;
}

qboolean G_SpawnInt( const char *key, const char *defaultString, int *out )
{
  char      *s;
  qboolean  present;

  present = G_SpawnString( key, defaultString, &s );
  *out = atoi( s );
  return present;
}

qboolean  G_SpawnVector( const char *key, const char *defaultString, float *out )
{
  char    *s;
  qboolean  present;

  present = G_SpawnString( key, defaultString, &s );
  sscanf( s, "%f %f %f", &out[ 0 ], &out[ 1 ], &out[ 2 ] );
  return present;
}

qboolean  G_SpawnVector4( const char *key, const char *defaultString, float *out )
{
  char    *s;
  qboolean  present;

  present = G_SpawnString( key, defaultString, &s );
  sscanf( s, "%f %f %f %f", &out[ 0 ], &out[ 1 ], &out[ 2 ], &out[ 3 ] );
  return present;
}



//
// fields are needed for spawning from the entity string
//
typedef enum
{
  F_INT,
  F_FLOAT,
  F_STRING,
  F_VECTOR,
  F_VECTOR4,
  F_ANGLEHACK
} fieldtype_t;

typedef struct
{
  char  *name;
  size_t ofs;
  fieldtype_t type;
} field_t;

field_t fields[ ] =
{
  {"acceleration", FOFS(acceleration), F_VECTOR},
  {"alpha", FOFS(pos1), F_VECTOR},
  {"angle", FOFS(s.apos.trBase), F_ANGLEHACK},
  {"angles", FOFS(s.apos.trBase), F_VECTOR},
  {"animation", FOFS(animation), F_VECTOR4},
  {"bounce", FOFS(physicsBounce), F_FLOAT},
  {"class", FOFS(inventory), F_STRING},
  {"classname", FOFS(classname), F_STRING},
  {"count", FOFS(count), F_INT},
  {"cvar", FOFS(Cvar_Val), F_STRING},
  {"dmg", FOFS(damage), F_INT},
  {"health", FOFS(health), F_INT},
  {"inventory", FOFS(inventory), F_STRING},
  {"message", FOFS(message), F_STRING},
  {"model", FOFS(model), F_STRING},
  {"model2", FOFS(model2), F_STRING},
  {"origin", FOFS(s.pos.trBase), F_VECTOR},
  {"radius", FOFS(pos2), F_VECTOR},
  {"random", FOFS(random), F_FLOAT},
  {"rotatorAngle", FOFS(rotatorAngle), F_FLOAT},
  {"sign", FOFS(sign), F_STRING},
  {"spawnflags", FOFS(spawnflags), F_INT},
  {"speed", FOFS(speed), F_FLOAT},
  {"target", FOFS(target), F_STRING},
  {"target2", FOFS(multitarget[ 1 ]), F_STRING},
  {"target3", FOFS(multitarget[ 2 ]), F_STRING},
  {"target4", FOFS(multitarget[ 3 ]), F_STRING},
  {"targetname", FOFS(targetname), F_STRING},
  {"targetname2", FOFS(multitargetname[ 1 ]), F_STRING},
  {"targetname3", FOFS(multitargetname[ 2 ]), F_STRING},
  {"targetname4", FOFS(multitargetname[ 3 ]), F_STRING},
  {"targetShaderName", FOFS(targetShaderName), F_STRING},
  {"targetShaderNewName", FOFS(targetShaderNewName), F_STRING},
  {"wait", FOFS(wait), F_FLOAT},
  {"weapon", FOFS(inventory), F_STRING}
};


typedef struct
{
  char  *name;
  void  (*spawn)(gentity_t *ent);
} spawn_t;

void SP_info_player_start( gentity_t *ent );
void SP_info_player_deathmatch( gentity_t *ent );
void SP_info_player_intermission( gentity_t *ent );

void SP_info_alien_intermission( gentity_t *ent );
void SP_info_human_intermission( gentity_t *ent );

void SP_func_plat( gentity_t *ent );
void SP_func_static( gentity_t *ent );
void SP_func_rotating( gentity_t *ent );
void SP_func_bobbing( gentity_t *ent );
void SP_func_pendulum( gentity_t *ent );
void SP_func_button( gentity_t *ent );
void SP_func_door( gentity_t *ent );
void SP_func_door_rotating( gentity_t *ent );
void SP_func_door_model( gentity_t *ent );
void SP_func_train( gentity_t *ent );
void SP_func_timer( gentity_t *self);

void SP_func_destructable( gentity_t *self );
void SP_func_spawn( gentity_t *self );

void SP_trigger_always( gentity_t *ent );
void SP_trigger_multiple( gentity_t *ent );
void SP_trigger_push( gentity_t *ent );
void SP_trigger_teleport( gentity_t *ent );
void SP_trigger_hurt( gentity_t *ent );
void SP_trigger_stage( gentity_t *ent );
void SP_trigger_win( gentity_t *ent );
void SP_trigger_buildable( gentity_t *ent );
void SP_trigger_class( gentity_t *ent );
void SP_trigger_equipment( gentity_t *ent );
void SP_trigger_gravity( gentity_t *ent );
void SP_trigger_heal( gentity_t *ent );
void SP_trigger_ammo( gentity_t *ent );

void SP_target_delay( gentity_t *ent );
void SP_target_speaker( gentity_t *ent );
void SP_target_print( gentity_t *ent );
void SP_target_score( gentity_t *ent );
void SP_target_teleporter( gentity_t *ent );
void SP_target_relay( gentity_t *ent );
void SP_target_kill( gentity_t *ent );
void SP_target_position( gentity_t *ent );
void SP_target_location( gentity_t *ent );
void SP_target_push( gentity_t *ent );
void SP_target_rumble( gentity_t *ent );
void SP_target_alien_win( gentity_t *ent );
void SP_target_human_win( gentity_t *ent );
void SP_target_hurt( gentity_t *ent );

void SP_target_and( gentity_t *ent );
void SP_target_or( gentity_t *ent );
void SP_target_xor( gentity_t *ent );
void SP_target_count( gentity_t *ent );
void SP_target_stgctrl( gentity_t *ent );
void SP_target_if( gentity_t *ent );
void SP_target_fund( gentity_t *ent );
void SP_target_force_weapon( gentity_t *ent );
void SP_target_force_class( gentity_t *ent );
void SP_target_force_win( gentity_t *ent );
void SP_target_bpctrl( gentity_t *ent );
void SP_target_class( gentity_t *ent );
void SP_target_equipment( gentity_t *ent );
void SP_target_power( gentity_t *ent );
void SP_target_creep( gentity_t *ent );

void SP_light( gentity_t *self );
void SP_info_null( gentity_t *self );
void SP_info_notnull( gentity_t *self );
void SP_path_corner( gentity_t *self );

void SP_misc_teleporter_dest( gentity_t *self );
void SP_misc_model( gentity_t *ent );
void SP_misc_portal_camera( gentity_t *ent );
void SP_misc_portal_surface( gentity_t *ent );

void SP_misc_particle_system( gentity_t *ent );
void SP_misc_anim_model( gentity_t *ent );
void SP_misc_light_flare( gentity_t *ent );

spawn_t spawns[ ] =
{
  { "func_bobbing",             SP_func_bobbing },
  { "func_button",              SP_func_button },
  { "func_destructable",        SP_func_destructable },
  { "func_door",                SP_func_door },
  { "func_door_model",          SP_func_door_model },
  { "func_door_rotating",       SP_func_door_rotating },
  { "func_group",               SP_info_null },
  { "func_pendulum",            SP_func_pendulum },
  { "func_plat",                SP_func_plat },
  { "func_rotating",            SP_func_rotating },
  { "func_spawn",               SP_func_spawn },
  { "func_static",              SP_func_static },
  { "func_timer",               SP_func_timer },      // rename trigger_timer?
  { "func_train",               SP_func_train },

  // info entities don't do anything at all, but provide positional
  // information for things controlled by other processes
  { "info_alien_intermission",  SP_info_alien_intermission },
  { "info_human_intermission",  SP_info_human_intermission },
  { "info_notnull",             SP_info_notnull },    // use target_position instead
  { "info_null",                SP_info_null },
  { "info_player_deathmatch",   SP_info_player_deathmatch },
  { "info_player_intermission", SP_info_player_intermission },
  { "info_player_start",        SP_info_player_start },
  { "light",                    SP_light },
  { "misc_anim_model",          SP_misc_anim_model },
  { "misc_light_flare",         SP_misc_light_flare },
  { "misc_model",               SP_misc_model },
  { "misc_particle_system",     SP_misc_particle_system },
  { "misc_portal_camera",       SP_misc_portal_camera },
  { "misc_portal_surface",      SP_misc_portal_surface },
  { "misc_teleporter_dest",     SP_misc_teleporter_dest },
  { "path_corner",              SP_path_corner },

  // targets perform no action by themselves, but must be triggered
  // by another entity
  { "target_alien_win",         SP_target_alien_win },
  { "target_and",               SP_target_and },
  { "target_bpctrl",            SP_target_bpctrl },
  { "target_class",             SP_target_class },
  { "target_count",             SP_target_count },
  { "target_creep",             SP_target_creep },
  { "target_delay",             SP_target_delay },
  { "target_equipment",         SP_target_equipment },
  { "target_force_class",       SP_target_force_class },
  { "target_force_weapon",      SP_target_force_weapon },
  { "target_force_win",         SP_target_force_win },
  { "target_fund",              SP_target_fund },
  { "target_human_win",         SP_target_human_win },
  { "target_hurt",              SP_target_hurt },
  { "target_if",                SP_target_if },
  { "target_kill",              SP_target_kill },
  { "target_location",          SP_target_location },
  { "target_or",                SP_target_or },
  { "target_position",          SP_target_position },
  { "target_power",             SP_target_power },
  { "target_print",             SP_target_print },
  { "target_push",              SP_target_push },
  { "target_relay",             SP_target_relay },
  { "target_rumble",            SP_target_rumble },
  { "target_score",             SP_target_score },
  { "target_speaker",           SP_target_speaker },
  { "target_stgctrl",           SP_target_stgctrl },
  { "target_teleporter",        SP_target_teleporter },
  { "target_xor",               SP_target_xor },

  // Triggers are brush objects that cause an effect when contacted
  // by a living player, usually involving firing targets.
  // While almost everything could be done with
  // a single trigger class and different targets, triggered effects
  // could not be client side predicted (push and teleport).
  { "trigger_always",           SP_trigger_always },
  { "trigger_ammo",             SP_trigger_ammo },
  { "trigger_buildable",        SP_trigger_buildable },
  { "trigger_class",            SP_trigger_class },
  { "trigger_equipment",        SP_trigger_equipment },
  { "trigger_gravity",          SP_trigger_gravity },
  { "trigger_heal",             SP_trigger_heal },
  { "trigger_hurt",             SP_trigger_hurt },
  { "trigger_multiple",         SP_trigger_multiple },
  { "trigger_push",             SP_trigger_push },
  { "trigger_stage",            SP_trigger_stage },
  { "trigger_teleport",         SP_trigger_teleport },
  { "trigger_win",              SP_trigger_win }
};

/*
===============
G_CallSpawn

Finds the spawn function for the entity and calls it,
returning qfalse if not found
===============
*/
qboolean G_CallSpawn( gentity_t *ent )
{
  spawn_t     *s;
  buildable_t buildable;

  if( !ent->classname )
  {
    Com_Printf( "G_CallSpawn: NULL classname\n" );
    return qfalse;
  }

  //check buildable spawn functions
  buildable = BG_BuildableByEntityName( ent->classname )->number;
  if( buildable != BA_NONE )
  {
    // don't spawn built-in buildings if we are using a custom layout
    if( level.layout[ 0 ] && Q_stricmp( level.layout, "*BUILTIN*" ) )
      return qfalse;

    if( buildable == BA_A_SPAWN || buildable == BA_H_SPAWN )
    {
      ent->r.currentAngles[ YAW ] += 180.0f;
      AngleNormalize360( ent->r.currentAngles[ YAW ] );
      ent->s.apos.trBase[ YAW ] = ent->r.currentAngles[ YAW ];
    }

    G_SpawnBuildable( ent, buildable );
    return qtrue;
  }

  // check normal spawn functions
  s = bsearch( ent->classname, spawns, ARRAY_LEN( spawns ),
    sizeof( spawn_t ), cmdcmp );
  if( s )
  {
    // found it
    s->spawn( ent );
    return qtrue;
  }

  Com_Printf( "%s doesn't have a spawn function\n", ent->classname );
  return qfalse;
}

/*
=============
G_NewString

Builds a copy of the string, translating \n to real linefeeds
so message texts can be multi-line
=============
*/
char *G_NewString( const char *string )
{
  char  *newb, *new_p;
  int   i,l;

  l = strlen( string ) + 1;

  newb = BG_Alloc0( l );

  new_p = newb;

  // turn \n into a real linefeed
  for( i = 0 ; i < l ; i++ )
  {
    if( string[ i ] == '\\' && i < l - 1 )
    {
      i++;
      if( string[ i ] == 'n' )
        *new_p++ = '\n';
      else
        *new_p++ = '\\';
    }
    else
      *new_p++ = string[ i ];
  }

  return newb;
}




/*
===============
G_ParseField

Takes a key/value pair and sets the binary values
in a gentity
===============
*/
void G_ParseField( const char *key, const char *value, gentity_t *ent )
{
  field_t *f;
  byte    *b;
  float   v;
  vec3_t  vec;
  vec4_t  vec4;

  f = bsearch( key, fields, ARRAY_LEN( fields ),
    sizeof( field_t ), cmdcmp );
  if( !f )
    return;
  b = (byte *)ent;

  switch( f->type )
  {
    case F_STRING:
      *(char **)( b + f->ofs ) = G_NewString( value );
      break;

    case F_VECTOR:
      sscanf( value, "%f %f %f", &vec[ 0 ], &vec[ 1 ], &vec[ 2 ] );

      ( (float *)( b + f->ofs ) )[ 0 ] = vec[ 0 ];
      ( (float *)( b + f->ofs ) )[ 1 ] = vec[ 1 ];
      ( (float *)( b + f->ofs ) )[ 2 ] = vec[ 2 ];
      break;

    case F_VECTOR4:
      sscanf( value, "%f %f %f %f", &vec4[ 0 ], &vec4[ 1 ], &vec4[ 2 ], &vec4[ 3 ] );

      ( (float *)( b + f->ofs ) )[ 0 ] = vec4[ 0 ];
      ( (float *)( b + f->ofs ) )[ 1 ] = vec4[ 1 ];
      ( (float *)( b + f->ofs ) )[ 2 ] = vec4[ 2 ];
      ( (float *)( b + f->ofs ) )[ 3 ] = vec4[ 3 ];
      break;

    case F_INT:
      *(int *)( b + f->ofs ) = atoi( value );
      break;

    case F_FLOAT:
      *(float *)( b + f->ofs ) = atof( value );
      break;

    case F_ANGLEHACK:
      v = atof( value );
      ( (float *)( b + f->ofs ) )[ 0 ] = 0;
      ( (float *)( b + f->ofs ) )[ 1 ] = v;
      ( (float *)( b + f->ofs ) )[ 2 ] = 0;
      break;
  }
}




/*
===================
G_SpawnGEntityFromSpawnVars

Spawn an entity and fill in all of the level fields from
level.spawnVars[], then call the class specfic spawn function
===================
*/
void G_SpawnGEntityFromSpawnVars( void )
{
  int         i;
  gentity_t   *ent;

  // get the next free entity
  ent = G_Spawn( );

  for( i = 0 ; i < level.numSpawnVars ; i++ )
    G_ParseField( level.spawnVars[ i ][ 0 ], level.spawnVars[ i ][ 1 ], ent );

  G_SpawnInt( "notq3a", "0", &i );

  if( i )
  {
    G_FreeEntity( ent );
    return;
  }

  VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );
  VectorCopy( ent->s.apos.trBase, ent->r.currentAngles );

  G_SpawnInt( "gate", "255", &ent->TargetGate );
  ent->multitarget[ 0 ] = ent->target;
  ent->multitargetname[ 0 ] = ent->targetname;

  // if we didn't get a classname, don't bother spawning anything
  if( !G_CallSpawn( ent ) )
    G_FreeEntity( ent );
}



/*
====================
G_AddSpawnVarToken
====================
*/
char *G_AddSpawnVarToken( const char *string )
{
  int   l;
  char  *dest;

  l = strlen( string );
  if( level.numSpawnVarChars + l + 1 > MAX_SPAWN_VARS_CHARS )
    Com_Error( ERR_DROP, "G_AddSpawnVarToken: MAX_SPAWN_CHARS" );

  dest = level.spawnVarChars + level.numSpawnVarChars;
  memcpy( dest, string, l + 1 );

  level.numSpawnVarChars += l + 1;

  return dest;
}

/*
====================
G_ParseSpawnVars

Parses a brace bounded set of key / value pairs out of the
level's entity strings into level.spawnVars[]

This does not actually spawn an entity.
====================
*/
qboolean G_ParseSpawnVars( void )
{
  char keyname[ MAX_TOKEN_CHARS ];
  char com_token[ MAX_TOKEN_CHARS ];

  level.numSpawnVars = 0;
  level.numSpawnVarChars = 0;

  // parse the opening brace
  if( !SV_GetEntityToken( com_token, sizeof( com_token ) ) )
  {
    // end of spawn string
    return qfalse;
  }

  if( com_token[ 0 ] != '{' )
    Com_Error( ERR_DROP, "G_ParseSpawnVars: found %s when expecting {", com_token );

  // go through all the key / value pairs
  while( 1 )
  {
    // parse key
    if( !SV_GetEntityToken( keyname, sizeof( keyname ) ) )
      Com_Error( ERR_DROP, "G_ParseSpawnVars: EOF without closing brace" );

    if( keyname[0] == '}' )
      break;

    // parse value
    if( !SV_GetEntityToken( com_token, sizeof( com_token ) ) )
      Com_Error( ERR_DROP, "G_ParseSpawnVars: EOF without closing brace" );

    if( com_token[0] == '}' )
      Com_Error( ERR_DROP, "G_ParseSpawnVars: closing brace without data" );

    if( level.numSpawnVars == MAX_SPAWN_VARS )
      Com_Error( ERR_DROP, "G_ParseSpawnVars: MAX_SPAWN_VARS" );

    level.spawnVars[ level.numSpawnVars ][ 0 ] = G_AddSpawnVarToken( keyname );
    level.spawnVars[ level.numSpawnVars ][ 1 ] = G_AddSpawnVarToken( com_token );
    level.numSpawnVars++;
  }

  return qtrue;
}



/*QUAKED worldspawn (0 0 0) ?

Every map should have exactly one worldspawn.
"music"   music wav file
"gravity" 800 is default gravity
"message" Text to print during connection process
*/
void SP_worldspawn( void )
{
  char *s;
  int    index = rand( ) % MAX_WARMUP_SOUNDS; // randomly select an end of warmup sound set

  G_SpawnString( "classname", "", &s );

  if( Q_stricmp( s, "worldspawn" ) )
    Com_Error( ERR_DROP, "SP_worldspawn: The first entity isn't 'worldspawn'" );

  // make some data visible to connecting client
  SV_SetConfigstring( CS_GAME_VERSION, GAME_VERSION );

  SV_SetConfigstring( CS_LEVEL_START_TIME, va( "%i", level.startTime ) );

  G_SpawnString( "music", "", &s );
  SV_SetConfigstring( CS_MUSIC, s );

  G_SpawnString( "message", "", &s );
  SV_SetConfigstring( CS_MESSAGE, s );        // map specific message

  SV_SetConfigstring( CS_MOTD, g_motd.string );   // message of the day

  if( G_SpawnString( "gravity", "", &s ) )
    Cvar_SetSafe( "g_gravity", s );

  if( G_SpawnString( "humanMaxStage", "", &s ) )
    Cvar_SetSafe( "g_humanMaxStage", s );

  if( G_SpawnString( "alienMaxStage", "", &s ) )
    Cvar_SetSafe( "g_alienMaxStage", s );

  if( G_SpawnString( "humanRepeaterBuildPoints", "", &s ) )
    Cvar_SetSafe( "g_humanRepeaterBuildPoints", s );

  if( G_SpawnString( "humanBuildPoints", "", &s ) )
    Cvar_SetSafe( "g_humanBuildPoints", s );

  if( G_SpawnString( "alienBuildPoints", "", &s ) )
    Cvar_SetSafe( "g_alienBuildPoints", s );

  Cvar_SetSafe( "g_disabledEquipment", s );

  G_SpawnString( "disabledClasses", "", &s );
  Cvar_SetSafe( "g_disabledClasses", s );

  G_SpawnString( "disabledBuildables", "", &s );
  Cvar_SetSafe( "g_disabledBuildables", s );

  g_entities[ ENTITYNUM_WORLD ].s.number = ENTITYNUM_WORLD;
  g_entities[ ENTITYNUM_WORLD ].r.ownerNum = ENTITYNUM_NONE;
  g_entities[ ENTITYNUM_WORLD ].classname = "worldspawn";

  g_entities[ ENTITYNUM_NONE ].s.number = ENTITYNUM_NONE;
  g_entities[ ENTITYNUM_NONE ].r.ownerNum = ENTITYNUM_NONE;
  g_entities[ ENTITYNUM_NONE ].classname = "nothing";

  if( g_restarted.integer )
    Cvar_SetSafe( "g_restarted", "0" );

  // see if we want a countdown time
  if(!IS_WARMUP) {
    if(g_doWarmup.integer && g_doWarmupCountdown.integer) {
      level.countdownTime = level.time + 1000;
      G_LogPrintf( "Countdown: %i\n", 1 );
    } else if(g_doCountdown.integer) {
      level.countdownTime = level.startTime + (g_countdown.integer * 1000);
      G_LogPrintf( "Countdown: %i\n", g_countdown.integer );
    } else {
      level.countdownTime = 0;
    }
  } else
    level.countdownTime = 0;

  SV_SetConfigstring( CS_COUNTDOWN, va( "%i %i", level.countdownTime, index ) );
}


/*
==============
G_SpawnEntitiesFromString

Parses textual entity definitions out of an entstring and spawns gentities.
==============
*/
void G_SpawnEntitiesFromString( void )
{
  level.numSpawnVars = 0;

  // the worldspawn is not an actual entity, but it still
  // has a "spawn" function to perform any global setup
  // needed by a level (setting configstrings or cvars, etc)
  if( !G_ParseSpawnVars( ) )
    Com_Error( ERR_DROP, "SpawnEntities: no entities" );

  SP_worldspawn( );

  // parse ents
  while( G_ParseSpawnVars( ) )
    G_SpawnGEntityFromSpawnVars( );
}
