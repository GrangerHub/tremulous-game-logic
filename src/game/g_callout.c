
#include "g_local.h"

#define ITEM_RADIUS 15

//
// Launch Callout 
//
gentity_t *LaunchCallout (int callout, gentity_t *ent, vec3_t origin, vec3_t velocity)
{
  gentity_t *dropped;

  dropped = G_Spawn();

  dropped->s.eType = ET_CALLOUT;
  dropped->s.modelindex = callout;
  dropped->s.modelindex2 = 0;       // meaningless

  VectorSet (dropped->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
  VectorSet (dropped->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);
  dropped->r.contents = CONTENTS_TRIGGER;

  dropped->clipmask = MASK_DEADSOLID;

  dropped->touch = NULL;

  G_SetOrigin( dropped, origin );
  dropped->s.pos.trType = TR_GRAVITY;
  dropped->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;
  //VectorScale( ent->client->ps.velocity, 0.80f, velocity );
  VectorMA(ent->client->ps.velocity, 100, velocity, dropped->s.pos.trDelta );

  VectorCopy( velocity, dropped->s.pos.trDelta );

  dropped->s.eFlags |= EF_BOUNCE_HALF;
  dropped->think = G_FreeEntity;
  dropped->nextthink = level.time + 30000;

  dropped->flags = FL_DROPPED_ITEM;

  trap_LinkEntity (dropped);

  return dropped;
}
