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

// g_utils.c -- misc utility functions for game module

#include "g_local.h"

typedef struct
{
  char oldShader[ MAX_QPATH ];
  char newShader[ MAX_QPATH ];
  float timeOffset;
} shaderRemap_t;

#define MAX_SHADER_REMAPS 128

int remapCount = 0;
shaderRemap_t remappedShaders[ MAX_SHADER_REMAPS ];

void AddRemap( const char *oldShader, const char *newShader, float timeOffset )
{
  int i;

  for( i = 0; i < remapCount; i++ )
  {
    if( Q_stricmp( oldShader, remappedShaders[ i ].oldShader ) == 0 )
    {
      // found it, just update this one
      strcpy( remappedShaders[ i ].newShader,newShader );
      remappedShaders[ i ].timeOffset = timeOffset;
      return;
    }
  }

  if( remapCount < MAX_SHADER_REMAPS )
  {
    strcpy( remappedShaders[ remapCount ].newShader,newShader );
    strcpy( remappedShaders[ remapCount ].oldShader,oldShader );
    remappedShaders[ remapCount ].timeOffset = timeOffset;
    remapCount++;
  }
}

const char *BuildShaderStateConfig( void )
{
  static char buff[ MAX_STRING_CHARS * 4 ];
  char        out[ ( MAX_QPATH * 2 ) + 5 ];
  int         i;

  memset( buff, 0, MAX_STRING_CHARS );

  for( i = 0; i < remapCount; i++ )
  {
    Com_sprintf( out, ( MAX_QPATH * 2 ) + 5, "%s=%s:%5.2f@", remappedShaders[ i ].oldShader,
                 remappedShaders[ i ].newShader, remappedShaders[ i ].timeOffset );
    Q_strcat( buff, sizeof( buff ), out );
  }
  return buff;
}


/*
=========================================================================

model / sound configstring indexes

=========================================================================
*/

/*
================
G_FindConfigstringIndex

================
*/
int G_FindConfigstringIndex( const char *name, int start, int max, qboolean create )
{
  int   i;
  char  s[ MAX_STRING_CHARS ];

  if( !name || !name[ 0 ] )
    return 0;

  for( i = 1; i < max; i++ )
  {
    SV_GetConfigstring( start + i, s, sizeof( s ) );
    if( !s[ 0 ] )
      break;

    if( !strcmp( s, name ) )
      return i;
  }

  if( !create )
    return 0;

  if( i == max )
    Com_Error( ERR_DROP, "G_FindConfigstringIndex: overflow" );

  SV_SetConfigstring( start + i, name );

  return i;
}

int G_ParticleSystemIndex( const char *name )
{
  return G_FindConfigstringIndex( name, CS_PARTICLE_SYSTEMS, MAX_GAME_PARTICLE_SYSTEMS, qtrue );
}

int G_ShaderIndex( const char *name )
{
  return G_FindConfigstringIndex( name, CS_SHADERS, MAX_GAME_SHADERS, qtrue );
}

int G_ModelIndex( const char *name )
{
  return G_FindConfigstringIndex( name, CS_MODELS, MAX_MODELS, qtrue );
}

int G_SoundIndex( const char *name )
{
  return G_FindConfigstringIndex( name, CS_SOUNDS, MAX_SOUNDS, qtrue );
}

//=====================================================================

/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the entity after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
gentity_t *G_Find( gentity_t *from, int fieldofs, const char *match )
{
  char  *s;

  if( !from )
    from = g_entities;
  else
    from++;

  for( ; from < &g_entities[ level.num_entities ]; from++ )
  {
    if( !from->inuse )
      continue;
    s = *(char **)( (byte *)from + fieldofs );

    if( !s )
      continue;

    if( !Q_stricmp( s, match ) )
      return from;
  }

  return NULL;
}


/*
=============
G_PickTarget

Selects a random entity from among the targets
=============
*/
#define MAXCHOICES  32

gentity_t *G_PickTarget( char *targetname )
{
  gentity_t *ent = NULL;
  int       num_choices = 0;
  gentity_t *choice[ MAXCHOICES ];

  if( !targetname )
  {
    Com_Printf("G_PickTarget called with NULL targetname\n");
    return NULL;
  }

  while( 1 )
  {
    ent = G_Find( ent, FOFS( targetname ), targetname );

    if( !ent )
      break;

    choice[ num_choices++ ] = ent;

    if( num_choices == MAXCHOICES )
      break;
  }

  if( !num_choices )
  {
    Com_Printf( "G_PickTarget: target %s not found\n", targetname );
    return NULL;
  }

  return choice[ rand( ) / ( RAND_MAX / num_choices + 1 ) ];
}

typedef struct loggedActivation_s {
  int entityNum;
  const char *classname;
  const char *targetname;
  int TargetGate;
  int GateStateBefore;
  int GateStateAfter;
  char *source;
  struct loggedActivation_s *parent;
  struct loggedActivation_s *children;
  struct loggedActivation_s *next;
} loggedActivation_t;

static loggedActivation_t *G_MakeLA(gentity_t *self) {
  loggedActivation_t *la = BG_Alloc(sizeof(loggedActivation_t));

  la->entityNum = (int)(self-g_entities);
  la->classname = self->classname;
  la->targetname = self->targetname;
  la->TargetGate = self->TargetGate;
  la->GateStateBefore = self->GateState;
  la->source = NULL;
  la->parent = NULL;
  la->children = NULL;
  la->next = NULL;
  return la;
}

static loggedActivation_t *leafLA;

static char laTimeStamp[32];
static char laTimeSpace[32];
static char *laTimePfx;

static void G_PrepareTimePrefix(void) {
  int time = level.time - level.startTime;

  Com_sprintf( laTimeStamp, sizeof(laTimeStamp), "%i:%02i.%03i",
               time / 1000 / 60, (time / 1000) % 60, time % 1000 );
  memset(laTimeSpace, ' ', strlen(laTimeStamp));
  laTimeSpace[ strlen(laTimeStamp)] = '\0';
  laTimePfx = laTimeStamp;
}

qboolean doPrintLa = qtrue;
#define LA_DOWN '|'
#define LA_BEND '\''
#define LA_RIGHT '-'

static void G_PrintLA(loggedActivation_t *la) {
  static char laPfx[2048] = "";
  static size_t laPfxLen = 0;

  while(la != NULL) {
    loggedActivation_t *del;

    if(la->parent != NULL && la->next == NULL) {
      laPfx[ laPfxLen - 2 ] = LA_BEND;
    }

    if(doPrintLa) {
      Com_Printf( "%s %s#%i (%s) %s |%i| %i->%i%s\n", laTimePfx, laPfx,
                la->entityNum, la->classname, la->targetname ? va( "[%s]", la->targetname ) : "][",
                la->TargetGate, la->GateStateBefore, la->GateStateAfter,
                la->source ? va( " {%s}", la->source ) : "" );
    }
    laTimePfx = laTimeSpace;

    if(la->parent != NULL) {
      if(la->next == NULL) {
        laPfx[laPfxLen - 2] = ' ';
      }
      laPfx[laPfxLen - 1] = ' ';
    }
    laPfx[laPfxLen++] = LA_DOWN;
    laPfx[laPfxLen++] = LA_RIGHT;
    laPfx[laPfxLen] = '\0';

    G_PrintLA(la->children);

    laPfxLen -= 2;
    laPfx[ laPfxLen ] = '\0';
    if(la->parent != NULL)
      laPfx[ laPfxLen - 1 ] = la->next == NULL ? ' ' : LA_RIGHT;

    del = la;
    la = la->next;
    if(del->source != NULL)
      BG_Free(del->source);
    BG_Free(del);
  }
}

qboolean G_LoggedActivation(
  gentity_t *self, 
  gentity_t *other,
  gentity_t *activator,
  trace_t *trace,
  const char *source,
  logged_activation_t act_type) {
  loggedActivation_t *la;

  qboolean top;
  qboolean occupy = qfalse;

  la = G_MakeLA(self);
  la->source = G_CopyString(source);

  top = leafLA == NULL;
  if(!top) {
    la->parent = leafLA;
    leafLA->children = la;
  }

  leafLA = la;

  switch (act_type) {
    case LOG_ACT_TOUCH:
      Com_Assert(trace && "G_LoggedActivation: trace is NULL");
      self->touch( self, other, trace );
      break;

    case LOG_ACT_ACTIVATE:
      occupy = self->activation.activate(self,activator);
      break;

    case LOG_ACT_USE:
      self->use(self, other, activator);
      break;
  }

  leafLA->GateStateAfter = self->GateState;

  if(top) {
    G_PrepareTimePrefix( );
    doPrintLa = trace == NULL || g_trigger_success;
    G_PrintLA(leafLA);

    leafLA = NULL;
  } else {
    leafLA = leafLA->parent;
  }

  return occupy;
}

static void G_LoggedMultiUse(
  gentity_t *self,
  gentity_t *other,
  gentity_t *activator,
  qboolean subsequent) {
  loggedActivation_t *la;

  la = G_MakeLA( self );

  if(subsequent) {
    leafLA->next = la;
    la->parent = leafLA->parent;
  } else {
    leafLA->children = la;
    la->parent = leafLA;
  }

  leafLA = la;

  self->use(self, other, activator);

  leafLA->GateStateAfter = self->GateState;
}

static void G_LoggedMultiUsesEnd(qboolean subsequent) {
  if(subsequent) {
    leafLA = leafLA->parent;
  }
}

/*
==============================
G_UseTargets

"activator" should be set to the entity that initiated the firing.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets(gentity_t *ent, gentity_t *activator) {
  gentity_t   *t;
  int i, j;
  qboolean top = qfalse;
  qboolean subsequent = qfalse;

  if(ent->targetShaderName && ent->targetShaderNewName) {
    float f = level.time * 0.001;
    AddRemap(ent->targetShaderName, ent->targetShaderNewName, f);
    SV_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig( ));
  }

  if(g_debugAMP.integer) {
    top = leafLA == NULL;
    if(top) {
      leafLA = G_MakeLA(ent);
      leafLA->source = G_CopyString("generic");
    }
  }

  for( i = 0; i < MAX_TARGETS; i++ ) {
    if( !ent->multitarget[ i ] ) {
      continue;
    }

    for(j = 0; j < MAX_TARGETNAMES; j++) {
      t = NULL;
      while((t = G_Find(t, FOFS(multitargetname[j]), ent->multitarget[i])) != NULL) {
        if(t->use) {
          if(g_debugAMP.integer) {
            G_LoggedMultiUse(t, ent, activator, subsequent);
            subsequent = qtrue;
          } else {
            t->use(t, ent, activator);
          }
        }

        if(!ent->inuse) {
          Com_Printf("entity was removed while using targets\n");
          break;
        }
      }
    }
  }

  if(g_debugAMP.integer) {
    G_LoggedMultiUsesEnd(subsequent);

    if(top) {
      leafLA->GateStateAfter = ent->GateState;

      G_PrepareTimePrefix( );
      doPrintLa = qtrue;
      G_PrintLA(leafLA);

      leafLA = NULL;
    }
  }
}


/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float *tv( float x, float y, float z )
{
  static  int     index;
  static  vec3_t  vecs[ 8 ];
  float           *v;

  // use an array so that multiple tempvectors won't collide
  // for a while
  v = vecs[ index ];
  index = ( index + 1 ) & 7;

  v[ 0 ] = x;
  v[ 1 ] = y;
  v[ 2 ] = z;

  return v;
}


/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
char *vtos( const vec3_t v )
{
  static  int   index;
  static  char  str[ 8 ][ 32 ];
  char          *s;

  // use an array so that multiple vtos won't collide
  s = str[ index ];
  index = ( index + 1 ) & 7;

  Com_sprintf( s, 32, "(%i %i %i)", (int)v[ 0 ], (int)v[ 1 ], (int)v[ 2 ] );

  return s;
}


/*
===============
G_SetMovedir

The editor only specifies a single value for angles (yaw),
but we have special constants to generate an up or down direction.
Angles will be cleared, because it is being used to represent a direction
instead of an orientation.
===============
*/
void G_SetMovedir( vec3_t angles, vec3_t movedir )
{
  static vec3_t VEC_UP        = { 0, -1, 0 };
  static vec3_t MOVEDIR_UP    = { 0, 0, 1 };
  static vec3_t VEC_DOWN      = { 0, -2, 0 };
  static vec3_t MOVEDIR_DOWN  = { 0, 0, -1 };

  if( VectorCompare( angles, VEC_UP ) )
    VectorCopy( MOVEDIR_UP, movedir );
  else if( VectorCompare( angles, VEC_DOWN ) )
    VectorCopy( MOVEDIR_DOWN, movedir );
  else
    AngleVectors( angles, movedir, NULL, NULL );

  VectorClear( angles );
}


float vectoyaw( const vec3_t vec )
{
  float yaw;

  if( vec[ YAW ] == 0 && vec[ PITCH ] == 0 )
  {
    yaw = 0;
  }
  else
  {
    if( vec[ PITCH ] )
      yaw = ( atan2( vec[ YAW ], vec[ PITCH ] ) * 180 / M_PI );
    else if( vec[ YAW ] > 0 )
      yaw = 90;
    else
      yaw = 270;

    if( yaw < 0 )
      yaw += 360;
  }

  return yaw;
}


void G_InitGentity( gentity_t *e )
{
  e->inuse = qtrue;
  e->classname = "noclass";
  e->s.number = e - g_entities;
  e->r.ownerNum = ENTITYNUM_NONE;
  BG_Queue_Init(&e->targeted);
}

/*
=================
G_Spawn

Either finds a free entity, or allocates a new one.

  The slots from 0 to MAX_CLIENTS-1 are always reserved for clients, and will
never be used by anything else.

Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
gentity_t *G_Spawn( void )
{
  int       i, force;
  gentity_t *e;

  e = NULL; // shut up warning
  i = 0;    // shut up warning

  for( force = 0; force < 2; force++ )
  {
    // if we go through all entities and can't find one to free,
    // override the normal minimum times before use
    e = &g_entities[ MAX_CLIENTS ];

    for( i = MAX_CLIENTS; i < level.num_entities; i++, e++ )
    {
      if( e->inuse )
        continue;

      // the first couple seconds of server time can involve a lot of
      // freeing and allocating, so relax the replacement policy
      if( !force && e->freetime > level.startTime + 2000 && level.time - e->freetime < 1000 )
        continue;

      // reuse this slot
      G_InitGentity( e );
      G_Entity_id_init( e );
      return e;
    }

    if( i != MAX_GENTITIES )
      break;
  }

  if( i == ENTITYNUM_MAX_NORMAL )
  {
    for( i = 0; i < MAX_GENTITIES; i++ )
      Com_Printf( "%4i: %s\n", i, g_entities[ i ].classname );

    Com_Error( ERR_DROP, "G_Spawn: no free entities" );
  }

  // open up a new slot
  level.num_entities++;

  // let the server system know that there are more entities
  SV_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ),
    &level.clients[ 0 ].ps, sizeof( level.clients[ 0 ] ) );

  G_InitGentity( e );
  G_Entity_id_init( e );
  return e;
}


/*
=================
G_EntitiesFree
=================
*/
qboolean G_EntitiesFree( void )
{
  int     i;
  gentity_t *e;

  e = &g_entities[ MAX_CLIENTS ];

  for( i = MAX_CLIENTS; i < level.num_entities; i++, e++ )
  {
    if( e->inuse )
      continue;

    // slot available
    return qtrue;
  }

  return qfalse;
}


char *G_CopyString( const char *str )
{
  size_t size = strlen( str ) + 1;
  char *cp = BG_Alloc0( size );
  memcpy( cp, str, size );
  return cp;
}


/*
=================
G_FreeEntity

Marks the entity as free
=================
*/
void G_FreeEntity( gentity_t *ent )
{
  SV_UnlinkEntity( ent );   // unlink from world

  if( ent->neverFree )
    return;

  G_UnlaggedClear( ent );
  BG_Queue_Clear(&ent->targeted);
  memset( ent, 0, sizeof( *ent ) );
  ent->classname = "freent";
  ent->freetime = level.time;
  ent->inuse = qfalse;
}

/*
=================
G_RemoveEntity

Safely remove an entity, perform reasonable cleanup logic
=================
*/
void G_RemoveEntity( gentity_t *ent )
{
  gentity_t *e;

  // Address occupation
  if( ent->flags & FL_OCCUPIED )
    G_UnoccupyEnt( ent, ent->occupation.occupant, NULL, qtrue );

  if( ( ent->client && ( ent->client->ps.eFlags & EF_OCCUPYING ) ) ||
      ( !ent->client && ( ent->s.eFlags & EF_OCCUPYING ) ) )
    G_UnoccupyEnt( ent->occupation.occupied, ent, NULL, qtrue );

  if( ent->client )
  {
    // removing a player causes the player to "unspawn"
    class_t class = ent->client->pers.classSelection; // back up the spawn queue choice
    weapon_t weapon = ent->client->pers.humanItemSelection; // back up
    ent->client->pers.classSelection = PCL_NONE;
    ent->client->pers.humanItemSelection = WP_NONE;
    ent->suicideTime = 0; // cancel any timed suicides
    ClientSpawn( ent, NULL, NULL, NULL, qtrue );
    ent->client->pers.classSelection = class; // restore the spawn queue choice
    ent->client->pers.humanItemSelection = weapon; // restore
    return;
  }
  else if( ent->s.eType == ET_BUILDABLE )
  {
    // the range marker (if any) goes away with the buildable
    G_RemoveRangeMarkerFrom( ent );
  }
  else if( ent->s.eType == ET_LEV2_ZAP_CHAIN )
  {
    if( ent->zapLink &&
        ((zap_t *)(ent->zapLink->data))->effectChannel == ent )
    {
      G_DeleteZapData( ent->zapLink->data );
      lev2ZapList = BG_List_Delete_Link( lev2ZapList, ent->zapLink );
    }
  }
  else if( ent->s.eType == ET_MOVER )
  {
    if( !strcmp( ent->classname, "func_door" ) ||
        !strcmp( ent->classname, "func_door_rotating" ) ||
        !strcmp( ent->classname, "func_door_model" ) ||
        !strcmp( ent->classname, "func_door_model_clip_brush" ) ||
        !strcmp( ent->classname, "func_plat" ) )
    {
      // each func_door_model entity is paired with a clip brush, remove the other
      if( ent->clipBrush != NULL )
        G_FreeEntity( ent->clipBrush );
      for( e = &g_entities[ MAX_CLIENTS ]; e < &g_entities[ level.num_entities ]; ++e )
      {
        if( e->parent == ent )
        {
          // this mover has a trigger area brush
          if( ent->teammaster != NULL && ent->teammaster->teamchain != NULL )
          {
            // the mover is part of a team of at least 2
            e->parent = ent->teammaster->teamchain; // hand the brush over to the next mover in command
          }
          else
            G_FreeEntity( e ); // remove the teamless or to-be-orphaned brush
          break;
        }
      }
    }
    // removing a mover opens the relevant portal
    SV_AdjustAreaPortalState( ent, qtrue );
  }
  else if( !strcmp( ent->classname, "path_corner" ) )
  {
    for( e = &g_entities[ MAX_CLIENTS ]; e < &g_entities[ level.num_entities ]; ++e )
    {
      if( e->nextTrain == ent )
        e->nextTrain = ent->nextTrain; // redirect func_train and path_corner entities
    }
  }
  else if( !strcmp( ent->classname, "info_player_intermission" ) ||
           !strcmp( ent->classname, "info_player_deathmatch" ) )
  {
    for( e = &g_entities[ MAX_CLIENTS ]; e < &g_entities[ level.num_entities ]; ++e )
    {
      if( e->inuse && e != ent &&
          ( !strcmp( e->classname, "info_player_intermission" ) ||
            !strcmp( e->classname, "info_player_deathmatch" ) ) )
      {
        break;
      }
    }
    // refuse to remove the last info_player_intermission/info_player_deathmatch entity
    //  (because it is required for initial camera placement)
    if( e >= &g_entities[ level.num_entities ] )
      return;
  }
  else if( !strcmp( ent->classname, "target_location" ) )
  {
    if( ent == level.locationHead )
      level.locationHead = ent->nextTrain;
    else
    {
      for( e = level.locationHead; e != NULL; e = e->nextTrain )
      {
        if( e->nextTrain == ent )
        {
          e->nextTrain = ent->nextTrain;
          break;
        }
      }
    }
  }
  else if( !Q_stricmp( ent->classname, "misc_portal_camera" ) )
  {
    for( e = &g_entities[ MAX_CLIENTS ]; e < &g_entities[ level.num_entities ]; ++e )
    {
      if( e->r.ownerNum == ent - g_entities )
      {
        // disown the surface
        e->r.ownerNum = ENTITYNUM_NONE;
      }
    }
  }

  if( ent->teammaster != NULL )
  {
    // this entity is part of a mover team
    if( ent == ent->teammaster )
    {
      // the entity is the master
      gentity_t *snd = ent->teamchain;
      for( e = snd; e != NULL; e = e->teamchain )
        e->teammaster = snd; // put the 2nd entity (if any) in command
      if( snd )
      {
        if( !strcmp( ent->classname, snd->classname ) )
        {
          // transfer certain activity properties
          snd->think = ent->think;
          snd->nextthink = ent->nextthink;
        }
        snd->flags &= ~FL_TEAMSLAVE; // put the 2nd entity (if any) in command
      }
    }
    else
    {
      // the entity is a slave
      for( e = ent->teammaster; e != NULL; e = e->teamchain )
      {
        if( e->teamchain == ent )
        {
          // unlink it from the chain
          e->teamchain = ent->teamchain;
          break;
        }
      }
    }
  }

  G_FreeEntity( ent );
}

/*
=================
G_TempEntity

Spawns an event entity that will be auto-removed
The origin will be snapped to save net bandwidth, so care
must be taken if the origin is right on a surface (snap towards start vector first)
=================
*/
gentity_t *G_TempEntity( const vec3_t origin, int event )
{
  gentity_t *e;
  vec3_t    snapped;

  e = G_Spawn( );
  e->s.eType = ET_EVENTS + event;

  e->classname = "tempEntity";
  e->eventTime = level.time;
  e->freeAfterEvent = qtrue;

  VectorCopy( origin, snapped );
  SnapVector( snapped );    // save network bandwidth
  G_SetOrigin( e, snapped );

  // find cluster for PVS
  SV_LinkEntity( e );

  return e;
}



/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
G_KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
void G_KillBox( gentity_t *ent )
{
  int       i, num;
  int       touch[ MAX_GENTITIES ];
  gentity_t *hit;
  vec3_t    mins, maxs;

  VectorAdd( ent->r.currentOrigin, ent->r.mins, mins );
  VectorAdd( ent->r.currentOrigin, ent->r.maxs, maxs );
  num = SV_AreaEntities( mins, maxs, touch, MAX_GENTITIES );

  for( i = 0; i < num; i++ )
  {
    hit = &g_entities[ touch[ i ] ];

    // in warmup telefrag buildables as well
    // (prevents buildings respawning within buildings)
    if( !IS_WARMUP && ent->client && !hit->client )
      continue;

    // impossible to telefrag self
    if( ent == hit )
      continue;

    // buildables dont telefrag enemies. trapping them has other consequences
    if( !ent->client && hit->client &&
        hit->client->pers.teamSelection != ent->buildableTeam )
    {
      continue;
    }

    // nail it
    G_Damage( hit, ent, ent, NULL, NULL,
      0, DAMAGE_INSTAGIB|DAMAGE_NO_PROTECTION, MOD_TELEFRAG );
  }

}

//==============================================================================

/*
===============
G_AddPredictableEvent

Use for non-pmove events that would also be predicted on the
client side: jumppads and item pickups
Adds an event+parm and twiddles the event counter
===============
*/
void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm )
{
  if( !ent->client )
    return;

  BG_AddPredictableEventToPlayerstate( event, eventParm, &ent->client->ps );
}


/*
===============
G_AddEvent

Adds an event+parm and twiddles the event counter
===============
*/
void G_AddEvent( gentity_t *ent, int event, int eventParm )
{
  int bits;

  if( !event )
  {
    Com_Printf( "G_AddEvent: zero event added for entity %i\n", ent->s.number );
    return;
  }

  // eventParm is converted to uint8_t (0 - 255) in msg.c
  if( eventParm & ~0xFF )
  {
    Com_Printf( S_COLOR_YELLOW "WARNING: G_AddEvent( %s ) has eventParm %d, "
              "which will overflow\n", BG_EventName( event ), eventParm );
  }

  // clients need to add the event in playerState_t instead of entityState_t
  if( ent->client )
  {
    bits = ent->client->ps.externalEvent & EV_EVENT_BITS;
    bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
    ent->client->ps.externalEvent = event | bits;
    ent->client->ps.externalEventParm = eventParm;
    ent->client->ps.externalEventTime = level.time;
  }
  else
  {
    bits = ent->s.event & EV_EVENT_BITS;
    bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
    ent->s.event = event | bits;
    ent->s.eventParm = eventParm;
  }

  ent->eventTime = level.time;
}


/*
===============
G_BroadcastEvent

Sends an event to every client
===============
*/
void G_BroadcastEvent( int event, int eventParm )
{
  gentity_t *ent;

  ent = G_TempEntity( vec3_origin, event );
  ent->s.eventParm = eventParm;
  ent->r.svFlags = SVF_BROADCAST; // send to everyone
}


/*
=============
G_Sound
=============
*/
void G_Sound( gentity_t *ent, int channel, int soundIndex )
{
  gentity_t *te;

  te = G_TempEntity( ent->r.currentOrigin, EV_GENERAL_SOUND );
  te->s.eventParm = soundIndex;
}


/*
=============
G_ClientIsLagging
=============
*/
qboolean G_ClientIsLagging( gclient_t *client )
{
  if( client )
  {
    if( client->ps.ping >= 999 )
      return qtrue;
    else
      return qfalse;
  }

  return qfalse; //is a non-existant client lagging? woooo zen
}

//==============================================================================


/*
================
G_SetOrigin

Sets the pos trajectory for a fixed position
================
*/
void G_SetOrigin( gentity_t *ent, const vec3_t origin )
{
  VectorCopy( origin, ent->s.pos.trBase );
  ent->s.pos.trType = TR_STATIONARY;
  ent->s.pos.trTime = level.time;
  ent->s.pos.trDuration = 0;
  VectorClear( ent->s.pos.trDelta );

  VectorCopy( origin, ent->r.currentOrigin );
}

// from quakestyle.telefragged.com
// (NOBODY): Code helper function
//
gentity_t *G_FindRadius( gentity_t *from, vec3_t org, float rad )
{
  vec3_t  eorg;
  int j;

  if( !from )
    from = g_entities;
  else
    from++;

  for( ; from < &g_entities[ level.num_entities ]; from++ )
  {
    if( !from->inuse )
      continue;

    for( j = 0; j < 3; j++ )
      eorg[ j ] = org[ j ] - ( from->r.currentOrigin[ j ] + ( from->r.mins[ j ] + from->r.maxs[ j ] ) * 0.5 );

    if( VectorLength( eorg ) > rad )
      continue;

    return from;
  }

  return NULL;
}

/*
===============
G_Visible

Test for a LOS between two entities
===============
*/
qboolean G_Visible( gentity_t *ent1, gentity_t *ent2, int contents )
{
  trace_t trace;

  SV_Trace( &trace, ent1->s.pos.trBase, NULL, NULL, ent2->s.pos.trBase,
              ent1->s.number, contents, TT_AABB );

  return trace.fraction >= 1.0f || trace.entityNum == ent2 - g_entities;
}

/*
===============
G_Visible

Test for a LOS between two bboxes
===============
*/
qboolean G_BBOXes_Visible(
  int source_num,
  const vec3_t source_origin, const vec3_t source_mins, const vec3_t source_maxs,
  int destination_num,
  const vec3_t dest_origin, const vec3_t dest_mins, const vec3_t dest_maxs,
  int contents) {
  trace_t        tr;
  vec3_t         source_midpoint, destination_midpoint, absmins, absmaxs;
  bboxPoint_t    source_point, destination_point;

  Com_Assert(source_origin && "G_BBOXes_Visible: source_origin is NULL");
  Com_Assert(dest_origin && "G_BBOXes_Visible: dest_origin is NULL" );

  if(source_num < 0 || source_num >= MAX_GENTITIES) {
    source_num = ENTITYNUM_NONE;
  }

  if(destination_num < 0 || destination_num >= MAX_GENTITIES) {
    destination_num = ENTITYNUM_NONE;
  }

  if(source_mins && source_maxs) {
    // use the midpoint of the bounds instead of the origin, because
    // bmodels may have their origin set to 0,0,0
    VectorAdd(source_origin, source_mins, absmins);
    VectorAdd(source_origin, source_maxs, absmaxs);
    VectorAdd( absmins, absmaxs, source_midpoint );
    VectorScale( source_midpoint, 0.5, source_midpoint );
  } else {
    VectorCopy(source_origin, source_midpoint);
  }

  if(dest_mins && dest_maxs) {
    // use the midpoint of the bounds instead of the origin, because
    // bmodels may have their origin set to 0,0,0
    VectorAdd(dest_origin, dest_mins, absmins);
    VectorAdd(dest_origin, dest_maxs, absmaxs);
    VectorAdd( absmins, absmaxs, destination_midpoint );
    VectorScale( destination_midpoint, 0.5, destination_midpoint );
  } else {
    VectorCopy(dest_origin, destination_midpoint);
  }
  

  source_point.num = 0;

  do {//check between the different points
    BG_EvaluateBBOXPoint( &source_point, source_midpoint, source_mins, source_maxs );

    destination_point.num = 0;
    do {
      BG_EvaluateBBOXPoint( &destination_point, destination_midpoint, dest_mins, dest_maxs );
      SV_Trace(
        &tr, source_point.point, NULL, NULL, destination_point.point,
        source_num, contents, TT_AABB );
      if( tr.fraction == 1.0  || tr.entityNum == destination_num ) {
        return qtrue;
      }

      // check only the midpoint if mins or maxs are NULL
      if(!dest_mins || !dest_maxs) {
        break;
      }

      destination_point.num++;
    } while( destination_point.num < NUM_NONVERTEX_BBOX_POINTS );

    // check only the midpoint if mins or maxs are NULL
    if(!source_mins || !source_maxs) {
      break;
    }

    source_point.num++;
  } while( source_point.num < NUM_BBOX_POINTS );

  return qfalse;
}

/*
===============
G_ClosestEnt

Test a list of entities for the closest to a particular point
===============
*/
gentity_t *G_ClosestEnt( vec3_t origin, gentity_t **entities, int numEntities )
{
  int       i;
  float     nd, d;
  gentity_t *closestEnt;

  if( numEntities <= 0 )
    return NULL;

  closestEnt = entities[ 0 ];
  d = DistanceSquared( origin, closestEnt->r.currentOrigin );

  for( i = 1; i < numEntities; i++ )
  {
    gentity_t *ent = entities[ i ];

    nd = DistanceSquared( origin, ent->r.currentOrigin );
    if( nd < d )
    {
      d = nd;
      closestEnt = ent;
    }
  }

  return closestEnt;
}

/*
================
G_Get_Foundation_Ent_Num

Recursively determines if an entity is ultimately
supported by the world, a mover, or has no foundational support
================
*/
int G_Get_Foundation_Ent_Num(gentity_t *ent) {
  int groundEntityNum;

  Com_Assert(ent && "G_Get_Foundation_Ent_Num: ent is NULL");

  if(!ent->inuse) {
    return ENTITYNUM_NONE;
  }

  if(ent->client) {
    groundEntityNum = ent->client->ps.groundEntityNum;
  } else {
    groundEntityNum = ent->s.groundEntityNum;
  }

  //check to see if the ground entity is a foundation entitity
  if(
    groundEntityNum == ENTITYNUM_WORLD ||
    groundEntityNum == ENTITYNUM_NONE ||
    groundEntityNum < 0 || groundEntityNum >= MAX_GENTITIES ||
    g_entities[groundEntityNum].s.eType == ET_MOVER ) {
    return groundEntityNum;
  }

  if(groundEntityNum == ent->s.number) {
    return ENTITYNUM_NONE;
  }

  //check the ground entity to see if it is on a foundation entity
  return G_Get_Foundation_Ent_Num(&g_entities[groundEntityNum]);
}

/*
===============
G_TriggerMenu

Trigger a menu on some client
===============
*/
void G_TriggerMenu( int clientNum, dynMenu_t menu )
{
  char buffer[ 32 ];

  Com_sprintf( buffer, sizeof( buffer ), "servermenu %d", menu );
  SV_GameSendServerCommand( clientNum, buffer );
}

/*
===============
G_TriggerMenuArgs

Trigger a menu on some client and passes an argument
===============
*/
void G_TriggerMenuArgs( int clientNum, dynMenu_t menu, int arg )
{
  char buffer[ 64 ];

  Com_sprintf( buffer, sizeof( buffer ), "servermenu %d %d", menu, arg );
  SV_GameSendServerCommand( clientNum, buffer );
}

/*
===============
G_CloseMenus

Close all open menus on some client
===============
*/
void G_CloseMenus( int clientNum )
{
  char buffer[ 32 ];

  Com_sprintf( buffer, 32, "serverclosemenus" );
  SV_GameSendServerCommand( clientNum, buffer );
}


/*
===============
G_AddressParse

Make an IP address more usable
===============
*/
static const char *addr4parse( const char *str, addr_t *addr )
{
  int i;
  int octet = 0;
  int num = 0;
  memset( addr, 0, sizeof( addr_t ) );
  addr->type = IPv4;
  for( i = 0; octet < 4; i++ )
  {
    if( isdigit( str[ i ] ) )
      num = num * 10 + str[ i ] - '0';
    else
    {
      if( num < 0 || num > 255 )
        return NULL;
      addr->addr[ octet ] = (byte)num;
      octet++;
      if( str[ i ] != '.' || str[ i + 1 ] == '.' )
        break;
      num = 0;
    }
  }
  if( octet < 1 )
    return NULL;
  return str + i;
}

static const char *addr6parse( const char *str, addr_t *addr )
{
  int i;
  qboolean seen = qfalse;
  /* keep track of the parts before and after the ::
     it's either this or even uglier hacks */
  byte a[ ADDRLEN ], b[ ADDRLEN ];
  size_t before = 0, after = 0;
  int num = 0;
  /* 8 hexadectets unless :: is present */
  for( i = 0; before + after <= 8; i++ )
  {
    //num = num << 4 | str[ i ] - '0';
    if( isdigit( str[ i ] ) )
      num = num * 16 + str[ i ] - '0';
    else if( str[ i ] >= 'A' && str[ i ] <= 'F' )
      num = num * 16 + 10 + str[ i ] - 'A';
    else if( str[ i ] >= 'a' && str[ i ] <= 'f' )
      num = num * 16 + 10 + str[ i ] - 'a';
    else
    {
      if( num < 0 || num > 65535 )
        return NULL;
      if( i == 0 )
      {
        //
      }
      else if( seen ) // :: has been seen already
      {
        b[ after * 2 ] = num >> 8;
        b[ after * 2 + 1 ] = num & 0xff;
        after++;
      }
      else
      {
        a[ before * 2 ] = num >> 8;
        a[ before * 2 + 1 ] = num & 0xff;
        before++;
      }
      if( !str[ i ] )
        break;
      if( str[ i ] != ':' || before + after == 8 )
        break;
      if( str[ i + 1 ] == ':' )
      {
        // ::: or multiple ::
        if( seen || str[ i + 2 ] == ':' )
          break;
        seen = qtrue;
        i++;
      }
      else if( i == 0 ) // starts with : but not ::
        return NULL;
      num = 0;
    }
  }
  if( seen )
  {
    // there have to be fewer than 8 hexadectets when :: is present
    if( before + after == 8 )
      return NULL;
  }
  else if( before + after < 8 ) // require exactly 8 hexadectets
    return NULL;
  memset( addr, 0, sizeof( addr_t ) );
  addr->type = IPv6;
  if( before )
    memcpy( addr->addr, a, before * 2 );
  if( after )
    memcpy( addr->addr + ADDRLEN - 2 * after, b, after * 2 );
  return str + i;
}

qboolean G_AddressParse( const char *str, addr_t *addr )
{
  const char *p;
  int max;
  if( strchr( str, ':' ) )
  {
    p = addr6parse( str, addr );
    max = 128;
  }
  else if( strchr( str, '.' ) )
  {
    p = addr4parse( str, addr );
    max = 32;
  }
  else
    return qfalse;
  Q_strncpyz( addr->str, str, sizeof( addr->str ) );
  if( !p )
    return qfalse;
  if( *p == '/' )
  {
    addr->mask = atoi( p + 1 );
    if( addr->mask < 1 || addr->mask > max )
      addr->mask = max;
  }
  else
  {
    if( *p )
      return qfalse;
    addr->mask = max;
  }
  return qtrue;
}

/*
===============
G_AddressCompare

Based largely on NET_CompareBaseAdrMask from ioq3 revision 1557
===============
*/
qboolean G_AddressCompare( const addr_t *a, const addr_t *b )
{
  int i, netmask;
  if( a->type != b->type )
    return qfalse;
  netmask = a->mask;
  if( a->type == IPv4 )
  {
    if( netmask < 1 || netmask > 32 )
      netmask = 32;
  }
  else if( a->type == IPv6 )
  {
    if( netmask < 1 || netmask > 128 )
      netmask = 128;
  }
  for( i = 0; netmask > 7; i++, netmask -= 8 )
    if( a->addr[ i ] != b->addr[ i ] )
      return qfalse;
  if( netmask )
  {
    netmask = ( ( 1 << netmask ) - 1 ) << ( 8 - netmask );
    return ( a->addr[ i ] & netmask ) == ( b->addr[ i ] & netmask );
  }
  return qtrue;
}

void G_Entity_id_init(gentity_t *ptr){
  static unsigned int inc_id = 1;
  Com_Assert(ptr);
  ptr->id = inc_id++;
}

void G_Entity_id_set(gentity_id *id,gentity_t *target){
  Com_Assert(id);
  id->ptr = target;
  if(target) {
    id->id = target->id;
  }
}

gentity_t *G_Entity_id_get(gentity_id *id){
  assert(id);
  if(id->ptr == NULL){
    return NULL;
  }
  if(id->id != id->ptr->id){
    id->ptr = NULL;
  }
  return id->ptr;
}

/*
===============
G_TraceWrapper

Wraps trace for QVM shared code
===============
*/
void G_TraceWrapper( trace_t *results, const vec3_t start,
                            const vec3_t mins, const vec3_t maxs,
                            const vec3_t end, int passEntityNum,
                            int contentMask )
{
  SV_Trace( results, start, mins, maxs, end, passEntityNum, contentMask, TT_AABB );
}

/*
================
G_SetPlayersLinkState

Links or unlinks all the player entities, with the option to skip a specific player for unlinking
================
*/
void G_SetPlayersLinkState( qboolean link, gentity_t *skipPlayer )
{
  int       i;
  gentity_t *ent;

  for ( i = 0, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if( ent->s.eType != ET_PLAYER )
      continue;

    if( ent->client &&
        ent->client->pers.connected != CON_CONNECTED )
      continue;

    if( link )
      SV_LinkEntity( ent );
    else
    {
      if( skipPlayer && 
          ent->s.number == skipPlayer->s.number )
        continue;

      SV_UnlinkEntity( ent );
    }
  }
}

/*
================
G_Expired

Checks to see if an expireTimes time stamp expired.  If ent is null, Checks
the level expireTimes.  New expiration timers can be created by adding a new
member to the expire_t enum in g_local.h
================
*/

qboolean G_Expired( gentity_t *ent, expire_t index )
{
  int expireTime;

  Com_Assert( ( index < NUM_EXPIRE_TYPES ) && ( index >= 0 ) &&
              "G_Expired: expire_t out of bounds" );

  if( ent )
    expireTime = ent->expireTimes[ index ];
  else
    expireTime = level.expireTimes[ index ];

  if( expireTime < level.time )
    return qtrue;

  return qfalse;
}

/*
================
G_SetExpiration

Sets the expirationTimes for a given entity.  If ent is NULL, expirationTime for
the level is used.
================
*/

void G_SetExpiration( gentity_t *ent, expire_t index, int expiration )
{
  Com_Assert( ( index < NUM_EXPIRE_TYPES ) && ( index >= 0 ) &&
              "G_Expired: expire_t out of bounds" );

  if( ent )
    ent->expireTimes[ index ] = expiration + level.time;
  else
    level.expireTimes[ index ] = expiration + level.time;
}
