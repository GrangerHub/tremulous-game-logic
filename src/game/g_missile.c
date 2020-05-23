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

#define MISSILE_PRESTEP_TIME  50

/*
================
G_BounceMissile

================
*/
void G_BounceMissile(gentity_t *ent, trace_t *trace) {
  vec3_t       velocity;
  float        dot;
  int          hitTime;
  weapon_t     weapon = ent->s.weapon;
  weaponMode_t mode = ent->s.generic1;

  // reflect the velocity on the trace plane
  hitTime =
    level.previousTime + (level.time - level.previousTime) * trace->fraction;
  BG_EvaluateTrajectoryDelta(&ent->s.pos, hitTime, velocity);
  dot = DotProduct(velocity, trace->plane.normal);
  VectorMA(velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta);

  if(BG_Missile(weapon, mode)->bounce_type == BOUNCE_HALF) {
    VectorScale(ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta);
    // check for stop
    if(trace->plane.normal[2] > 0.2 && VectorLength(ent->s.pos.trDelta) < 40) {
      G_SetOrigin(ent, trace->endpos);
      return;
    }
  }

  VectorAdd(ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
  VectorCopy(ent->r.currentOrigin, ent->s.pos.trBase);
  ent->s.pos.trTime = level.time;
}


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile(gentity_t *ent) {
  vec3_t       dir;
  vec3_t       origin;
  weapon_t     weapon = ent->s.weapon;
  weaponMode_t mode = ent->s.generic1;

  BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
  SnapVector(origin);
  G_SetOrigin(ent, origin);

  // we don't have a valid direction, so just point straight up
  dir[0] = dir[1] = 0;
  dir[2] = 1;

  ent->s.eType = ET_GENERAL;

  ent->s.eFlags &= ~EF_WARN_CHARGE;

  G_SplatterFire(
    ent, ent->parent, ent->s.pos.trBase, dir, (rand() / (RAND_MAX / 0x100 + 1)),
    ent->s.weapon, ent->s.generic1, ent->splashMethodOfDeath);

  if(BG_Missile(weapon, mode)->explode_miss_effect) {
    G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(dir));
  }

  ent->freeAfterEvent = qtrue;

  // splash damage
  if(ent->splashDamage) {
    G_RadiusDamage(
      ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->parent,
      ent->splashDamage, ent->splashRadius, ent, ent->splashMethodOfDeath,
      qtrue);
  }

  SV_LinkEntity(ent);
}

/*
================
G_KickUpMissile

================
*/
void G_KickUpMissile(gentity_t *ent) {
  vec3_t       velocity, origin;
  weapon_t     weapon = ent->s.weapon;
  weaponMode_t mode = ent->s.generic1;
  

  BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
  BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
  velocity[2] = BG_Missile(weapon, mode)->kick_up_speed;
  VectorCopy(velocity, ent->s.pos.trDelta);
  ent->s.pos.trType = TR_GRAVITY;
  VectorCopy(origin, ent->s.pos.trBase);
  ent->s.pos.trTime = level.time;
  ent->kick_up_time = 0;
}

/*
================
G_MissileSearchAndDestroy

Adjust the trajectory to point towards the target
================
*/
void G_MissileSearchAndDestroy(gentity_t *self) {
  vec3_t         dir;
  trace_t        tr;
  gentity_t      *ent;
  bboxPoint_t    targeted_bbox_point;
  bboxPointNum_t bbox_point_num;
  int            i;
  float          d, nearest;
  weapon_t       weapon = self->s.weapon;
  weaponMode_t   mode = self->s.generic1;

  if(self->parent && !self->parent->inuse) {
    self->parent = NULL;
  }

  if(level.time >= self->explode_time) {
    VectorCopy( self->r.currentOrigin, self->s.pos.trBase );
    self->s.pos.trType = TR_STATIONARY;
    self->s.pos.trTime = level.time;

    self->think = G_ExplodeMissile;
    self->nextthink = level.time + 50;
    if( self->parent )
      self->parent->active = qfalse; //allow the parent to start again
    return;
  }

  ent = self->target_ent;
  if(
    ent && ent->health > 0 && ent->client &&
    ent->client->ps.stats[STAT_TEAM] == TEAM_HUMANS) {
      qboolean isVisible = qfalse;
      //check for visibility
      for(
        bbox_point_num = 0; bbox_point_num < NUM_NONVERTEX_BBOX_POINTS;
        bbox_point_num++) {
        bboxPoint_t bboxPoint;

        bboxPoint.num = bbox_point_num;
        BG_EvaluateBBOXPoint( &bboxPoint,
                              ent->r.currentOrigin,
                              ent->r.mins, ent->r.maxs);

        SV_Trace(
          &tr, self->r.currentOrigin, self->r.mins, self->r.maxs,
          bboxPoint.point, self->r.ownerNum, qfalse, self->clip_mask, TT_AABB);

        if(tr.entityNum != ENTITYNUM_WORLD) {
          isVisible = qtrue;
          targeted_bbox_point.num = bboxPoint.num;
          VectorCopy(bboxPoint.point, targeted_bbox_point.point);
          nearest =
            DistanceSquared(self->r.currentOrigin, targeted_bbox_point.point);
          break;
        }
      }

      if(!isVisible) {
        self->target_ent = NULL;
        nearest = 0; // silence warning
      }
  } else {
    self->target_ent = NULL;
    nearest = 0; // silence warning
  }

  //find the closest human
  for( i = 0; i < MAX_CLIENTS; i++ ) {
    ent = &g_entities[ i ];

    if(G_NoTarget(ent)) {
      continue;
    }

    if(
      ent->client &&
      ent->health > 0 &&
      ent->client->ps.stats[ STAT_TEAM ]!= self->buildableTeam &&
      (
        d = DistanceSquared( ent->r.currentOrigin, self->r.currentOrigin ),
        (self->target_ent == NULL || d < nearest))) {
      for(
        bbox_point_num = 0; bbox_point_num < NUM_NONVERTEX_BBOX_POINTS;
        bbox_point_num++) {
        bboxPoint_t bboxPoint;

        bboxPoint.num = bbox_point_num;
        BG_EvaluateBBOXPoint( &bboxPoint,
                              ent->r.currentOrigin,
                              ent->r.mins, ent->r.maxs);

        SV_Trace(
          &tr, self->r.currentOrigin, self->r.mins, self->r.maxs,
          bboxPoint.point, self->r.ownerNum, qfalse, self->clip_mask, TT_AABB);

        if(tr.entityNum != ENTITYNUM_WORLD) {
          nearest = d;
          self->target_ent = ent;
          targeted_bbox_point.num = bboxPoint.num;
          VectorCopy(bboxPoint.point, targeted_bbox_point.point);
          break;
        }
      }
    }
  }

  if(self->target_ent == NULL) {
    VectorClear(dir);
  } else {
    VectorSubtract(targeted_bbox_point.point, self->r.currentOrigin, dir);
    VectorNormalize(dir);
  }

  //change direction towards the player
  VectorScale(dir, BG_Missile(weapon, mode)->speed, self->s.pos.trDelta);
  SnapVector(self->s.pos.trDelta);      // save net bandwidth
  VectorCopy(self->r.currentOrigin, self->s.pos.trBase);
  self->s.pos.trTime = level.time;

  self->nextthink =
    level.time + BG_Missile(weapon, mode)->search_and_destroy_change_period;
}

/*
================
G_MissileImpact

================
*/
void G_MissileImpact( gentity_t *ent, trace_t *trace ) {
  gentity_t    *other, *attacker;
  qboolean     returnAfterDamage = qfalse;
  vec3_t       dir;
  weapon_t     weapon = ent->s.weapon;
  weaponMode_t mode = ent->s.generic1;

  other = &g_entities[ trace->entityNum ];
  attacker = &g_entities[ ent->r.ownerNum ];

  if(BG_Missile(weapon, mode)->impact_stick) {
    VectorCopy(trace->plane.normal, ent->s.origin2);
    vectoangles(trace->plane.normal, ent->r.currentAngles);
    VectorCopy(ent->r.currentAngles, ent->s.apos.trBase);
    G_SetOrigin(ent, trace->endpos);
    G_AddEvent(ent, EV_MISSILE_BOUNCE, 0);
    return;
  }

  // check for bounce
  if(
    BG_Missile(weapon, mode)->bounce_type != BOUNCE_NONE &&
    !(
      BG_Missile(weapon, mode)->damage_on_bounce &&
      G_TakesDamage(other) &&
      !(
        (other->s.eType == ET_MISSILE) &&
        (other->s.weapon == weapon) &&
        (other->s.generic1 == mode)))) {
    G_BounceMissile(ent, trace);

    //only play a sound if requested
    if(BG_Missile(weapon, mode)->bounce_sound) {
      G_AddEvent(ent, EV_MISSILE_BOUNCE, 0);
    }

    return;
  }

  if(
    other->client &&
    other->client->ps.stats[STAT_TEAM] != ent->buildableTeam) {
      switch(BG_Missile(weapon, mode)->impede_move) {
        case IMPEDE_MOVE_SLOW:
          other->client->ps.stats[ STAT_STATE ] |= SS_SLOWLOCKED;
          other->client->lastSlowTime = level.time;
          AngleVectors( other->client->ps.viewangles, dir, NULL, NULL );
          other->client->ps.stats[ STAT_VIEWLOCK ] = DirToByte( dir );
          break;

        case IMPEDE_MOVE_LOCK:
          other->client->ps.stats[STAT_STATE] |= SS_BLOBLOCKED;
          other->client->lastLockTime = level.time;
          AngleVectors(other->client->ps.viewangles, dir, NULL, NULL);
          other->client->ps.stats[STAT_VIEWLOCK] = DirToByte(dir);
          break;

        default:
          break;
      }
  }

  if(
    (BG_Missile(weapon, mode)->impact_create_portal != PORTAL_NONE) &&
    !other->client &&
    other->s.eType != ET_BUILDABLE && other->s.eType != ET_MOVER &&
    !(other->r.contents & CONTENTS_DOOR)) {
    G_Portal_Create( ent->parent, trace->endpos, trace->plane.normal, ent->s.modelindex2 );
  }

  if(BG_Missile(weapon, mode)->return_after_damage) {
    if(
      other->s.eType == ET_BUILDABLE &&
      (
        other->s.modelindex == BA_A_HIVE ||
        (
          ent->parent && ent->parent->s.eType == ET_BUILDABLE &&
          other->s.modelindex == ent->parent->s.modelindex))) {
      if(!ent->parent) {
        Com_Printf(
          va(S_COLOR_YELLOW "WARNING: %s entity has no parent in G_MissileImpact\n",
          BG_Buildable(other->s.modelindex)->humanName));
      } else {
        ent->parent->active = qfalse;
      }

      G_FreeEntity(ent);
      return;
    } else if(
      BG_Missile(weapon, mode)->return_after_damage &&
      ent->parent && ent->parent == other->parent) {
        ent->parent->active = qfalse;

        G_FreeEntity(ent);
    } else {
      //prevent collision with the client when returning
      ent->r.ownerNum = other->s.number;

      ent->think = G_ExplodeMissile;
      ent->nextthink = level.time + FRAMETIME;

      //only damage enemies
      if(other->client) {
        if(ent->buildableTeam != other->client->ps.stats[STAT_TEAM]) {
          returnAfterDamage = qtrue;
        }
      } else if(ent->buildableTeam != other->buildableTeam){
        returnAfterDamage = qtrue;
      } else {
        return;
      }
    }
  }

  // impact damage
  if(G_TakesDamage(other)) {
    // FIXME: wrong damage direction?
    if(ent->damage) {
      vec3_t  velocity;
      int     dflags;

      BG_EvaluateTrajectoryDelta(&ent->s.pos, level.time, velocity);
      if(VectorLength(velocity) == 0)
        velocity[2] = 1;  // stepped on a grenade

      dflags = DAMAGE_NO_LOCDAMAGE;

      if(ent->noKnockback) {
        dflags |= DAMAGE_NO_KNOCKBACK;
      }

      G_Damage(
        other, ent, attacker, velocity, ent->r.currentOrigin, ent->damage,
        dflags, ent->methodOfDeath);
    }
  }

  if(returnAfterDamage)
    return;

  // is it cheaper in bandwidth to just remove this ent and create a new
  // one, rather than changing the missile into the explosion?

  if(
    G_TakesDamage(other) &&
    (other->s.eType == ET_PLAYER || other->s.eType == ET_BUILDABLE)) {
    if(BG_Missile(weapon, mode)->impact_miss_effect) {
      G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
    } else {
      G_AddEvent(ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal));
      ent->s.otherEntityNum = other->s.number;
    }
  } else if(trace->surfaceFlags & SURF_METALSTEPS) {
    G_AddEvent(ent, EV_MISSILE_MISS_METAL, DirToByte(trace->plane.normal));
  } else {
    G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(trace->plane.normal));
  }

  ent->freeAfterEvent = qtrue;

  // change over to a normal entity right at the point of impact
  ent->s.eType = ET_GENERAL;

  SnapVectorTowards(trace->endpos, ent->s.pos.trBase);  // save net bandwidth

  G_SetOrigin(ent, trace->endpos);

  // splash damage (doesn't apply to person directly hit)
  if(ent->splashDamage) {
    G_RadiusDamage(
      trace->endpos, NULL, NULL, ent->parent, ent->splashDamage,
      ent->splashRadius, other, ent->splashMethodOfDeath, qtrue);
  }

  SV_LinkEntity(ent);
}

/*
================
G_CheckForMissileImpact

================
*/
static qboolean G_CheckForMissileImpact(
  gentity_t *ent, vec3_t origin, trace_t *tr) {
  int       passent;

  // ignore interactions with the missile
  passent = ent->s.number;

  // general trace to see if we hit anything at all
  SV_Trace(
    tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs,
    origin, passent, qtrue, ent->clip_mask, TT_AABB);

  if(tr->startsolid || tr->allsolid) {
    tr->fraction = 0.0f;
    VectorCopy(ent->r.currentOrigin, tr->endpos);
  }

  if(tr->fraction < 1.0f) {
    if(!ent->pointAgainstWorld || tr->contents & CONTENTS_BODY) {
      // We hit an entity or we don't care
      return qtrue;
    } else {
      SV_Trace(
        tr, ent->r.currentOrigin, NULL, NULL, origin,
        passent, qtrue, ent->clip_mask, TT_AABB);

      if(tr->fraction < 1.0f) {
        // Hit the world with point trace
        return qtrue;
      } else {
        if(tr->contents & CONTENTS_BODY) {
          // Hit an entity
          return qtrue;
        } else {
          SV_Trace(
            tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs,
            origin, passent, qtrue, *Temp_Clip_Mask(CONTENTS_BODY, 0), TT_AABB);

          if(tr->fraction < 1.0f){
            return qtrue;
          }
        }
      }
    }
  }

  return qfalse;
}

/*
================
G_MissileTripWire

Explode if the trip wire is tripped
================
*/
void G_MissileTripWire(gentity_t *ent) {
  trace_t     previous_trace;
  vec3_t      end;
  bgentity_id unlinked_humans[MAX_CLIENTS];
  int         num_unnlinked_humans = 0;
  int         i;
  weapon_t       weapon = ent->s.weapon;
  weaponMode_t   mode = ent->s.generic1;

  ent->tripwire_time =
    level.time + BG_Missile(weapon, mode)->tripwire_check_frequency;

  //backup the trace
  previous_trace = ent->tripwire_trace;

  //ignore friendly human players
  for(i = 0; i < MAX_CLIENTS; i++) {
    gentity_t *player = &g_entities[i];

    if(!player->inuse) {
      continue;
    }

    if(!player->r.linked) {
      continue;
    }

    if(!player->client) {
      continue;
    }

    if(player->client->pers.connected != CON_CONNECTED) {
      continue;
    }

    if(player->client->ps.stats[STAT_TEAM] != ent->buildableTeam) {
      continue;
    }

    SV_UnlinkEntity(player);
    BG_UEID_set(&unlinked_humans[num_unnlinked_humans], i);
    num_unnlinked_humans++;
  }

  VectorMA(
    ent->r.currentOrigin, BG_Missile(weapon, mode)->tripwire_range,
    ent->s.origin2, end);

  //perform a new trace
  SV_Trace(
    &ent->tripwire_trace, ent->r.currentOrigin, NULL, NULL, end, ent->s.number,
    qtrue, *Temp_Clip_Mask(MASK_SHOT, 0), TT_AABB);

  //relink unlinked human players
  for(i = 0; i < num_unnlinked_humans; i++) {
    gentity_t *ent = &g_entities[BG_UEID_get_ent_num(&unlinked_humans[i])];
    SV_LinkEntity(ent);
  }

  if(!ent->tripwire_set) {
    if(BG_Missile(weapon, mode)->impact_stick) {
      if(ent->s.pos.trType != TR_STATIONARY) {
        //only set if the missile stuck on impact
        return;
      }
    }

    //the laser mine is being set
    ent->s.eFlags |= EF_WARN_CHARGE;
    ent->tripwire_set = qtrue;
    G_AddEvent( ent, EV_TRIPWIRE_ARMED, 0 );
    return;
  }

  //check if the laser beam has been tripped
  if(
    ent->tripwire_trace.entityNum != previous_trace.entityNum ||
    ent->tripwire_trace.startsolid != previous_trace.startsolid ||
    ent->tripwire_trace.allsolid != previous_trace.allsolid ||
    ent->tripwire_trace.fraction != previous_trace.fraction) {
    ent->die(
      ent, ent, ent, BG_Missile(weapon, mode)->health,
      BG_Missile(weapon, mode)->mod);
    return;
  }
}

/*
==============
 G_MissileLerpFraction
==============
*/
static float G_MissileLerpFraction(int time, int start_time, int stop_time) {
  float frameMsec;

  if(start_time == stop_time) {
    return 0.0f;
  } else {
    frameMsec = stop_time - start_time;
    return (float)(time - start_time) / (float)frameMsec;
  }
}

/*
================
G_RunMissile

================
*/
void G_RunMissile(gentity_t *ent) {
  vec3_t    origin;
  trace_t   tr;
  qboolean  impact;
  int       contents;

  // get current position
  BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);

  impact = G_CheckForMissileImpact(ent, origin, &tr);

  VectorCopy(tr.endpos, ent->r.currentOrigin);

  if(impact) {
    if(tr.surfaceFlags & SURF_NOIMPACT) {
      // Never explode or bounce on sky
      G_FreeEntity( ent );
      return;
    }

    G_MissileImpact(ent, &tr);

    if(ent->s.eType != ET_MISSILE) {
      return;   // exploded
    }
  }

  //check for scaling of the missile dimensions
  if(ent->scale_missile.enabled && ent->scale_missile.start_time < level.time) {
    vec3_t    backup_mins;
    vec3_t    backup_maxs;
    qboolean  restore_backup = qfalse;

    VectorCopy(ent->r.mins, backup_mins);
    VectorCopy(ent->r.maxs, backup_maxs);

    if(ent->scale_missile.stop_time < level.time) {
      VectorCopy(ent->scale_missile.end_mins, ent->r.mins);
      VectorCopy(ent->scale_missile.end_maxs, ent->r.maxs);

      //don't scale if the missile would impact as a result
      if(G_CheckForMissileImpact(ent, origin, &tr)) {
        restore_backup = qtrue;
      } else {
        //the scaling has finished
        ent->scale_missile.enabled = qfalse;
      }
    } else {
      float lerp = G_MissileLerpFraction(
        level.time,
        ent->scale_missile.start_time,
        ent->scale_missile.stop_time);

      //try scaling
      VectorLerp2(
        lerp, ent->scale_missile.start_mins,
        ent->scale_missile.end_mins,
        ent->r.mins);
      VectorLerp2(
        lerp, ent->scale_missile.start_maxs,
        ent->scale_missile.end_maxs,
        ent->r.maxs);

      //don't scale if the missile would impact as a result
      if(G_CheckForMissileImpact(ent, origin, &tr)) {
        restore_backup = qtrue;
      }
    }

    if(restore_backup) {
      VectorCopy(backup_mins, ent->r.mins);
      VectorCopy(backup_maxs, ent->r.maxs);
    }
  }

  contents = ent->r.contents;
  if(!contents) {
    G_SetContents(ent, CONTENTS_SOLID, qfalse); //trick SV_LinkEntity into...
  }
  SV_LinkEntity(ent);
  if(!contents) {
    G_SetContents(ent, contents, qfalse); //...encoding bbox information
  }

  // check think function after bouncing
  G_RunThink(ent);
}

void G_MissileThink(gentity_t *ent) {
  weapon_t     weapon = ent->s.weapon;
  weaponMode_t mode = ent->s.generic1;

  ent->nextthink = level.time + 25;

  if(BG_Missile(weapon, mode)->kick_up_speed > 0.0f && ent->kick_up_time) {
    if(level.time >= ent->kick_up_time) {
      G_KickUpMissile(ent);
    }
  }

  if(BG_Missile(weapon, mode)->search_and_destroy_change_period > 0) {
    G_MissileSearchAndDestroy(ent);
  }

  if(BG_Missile(weapon, mode)->tripwire_range > 0.0f) {
    if(level.time >= ent->tripwire_time) {
      G_MissileTripWire(ent);
    }
  }

  if(BG_Missile(weapon, mode)->explode_delay > 0) {
    if(level.time >= ent->explode_time) {
      G_ExplodeMissile(ent);
    }
  }
}

void G_MissileDie(
  gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage,
  int mod) {
  self->nextthink = level.time + 25;
  self->think = G_ExplodeMissile;
}

void G_Detonate_Saved_Missiles(gentity_t *self) {
  int          i;
  int          *clips;
  int          *ammo;
  weaponMode_t mode;
  weapon_t     weapon;
  qboolean     decrement_clips = qfalse;

  Com_Assert(self && "G_Detonate_Saved_Missiles: self is NULL");

  for(weapon = 0; weapon < WP_NUM_WEAPONS; weapon++) {
    for(mode = 0; mode < WPM_NUM_WEAPONMODES; mode++) {
      qboolean missile_mode_detonated = qfalse;

      if(
        !BG_Missile(weapon, mode) ||
        !BG_Missile(weapon, mode)->detonate_saved_missiles ||
        self->num_saved_missiles[weapon][mode] <= 0) {
        self->num_saved_missiles[weapon][mode] = 0;
        continue;
      }

      for(i = self->num_saved_missiles[weapon][mode] - 1; i >= 0 ; i--) {
        gentity_t *missile =
          G_Entity_UEID_get(&self->saved_missiles[i][weapon][mode]);

        if(missile) {
          missile_mode_detonated = qtrue;
          missile->think = G_ExplodeMissile;
          missile->nextthink =
            level.time +
            (
              (
                self->num_saved_missiles[weapon][mode] - i) *
                BG_Missile(weapon, mode)->detonate_saved_missiles_delay);
        }
      }

      if(self->client) {

        if(
          BG_Missile(weapon, mode)->detonate_saved_missiles_repeat >
          self->client->ps.weaponTime) {
            self->client->ps.weaponTime +=
              BG_Missile(weapon, mode)->detonate_saved_missiles_repeat;
        }

        if(weapon == self->client->ps.weapon) {
          decrement_clips = qtrue;
        }
      }

      self->num_saved_missiles[weapon][mode] = 0;
    }
  }

  if(decrement_clips && BG_Weapon(self->client->ps.weapon)->usesBarbs) {
    if(self->client) {
      clips = BG_GetClips(&self->client->ps, self->client->ps.weapon);
      ammo = &self->client->ps.ammo;
    } else {
      clips = NULL;
      ammo = NULL;
    }

    if(clips && *clips > 0) {
      (*clips)--;
      if(ammo) {
        *ammo = BG_Weapon(self->client->ps.weapon)->maxAmmo;
      }
    }
  }
}

void G_LaunchMissile(
  gentity_t *self, weapon_t weapon, weaponMode_t mode, vec3_t start,
  vec3_t dir) {
  gentity_t *bolt;
  float     speed;

  if(!BG_Missile(weapon, mode)->enabled) {
    return;
  }

  VectorNormalize (dir);

  bolt = G_Spawn();
  bolt->think = G_MissileThink;
  if(BG_Missile(weapon, mode)->activation_delay > 0) {
    bolt->nextthink = level.time + BG_Missile(weapon, mode)->activation_delay;
  } else {
    bolt->nextthink = level.time + 25;
  }
  bolt->explode_time = level.time + BG_Missile(weapon, mode)->explode_delay;
  bolt->tripwire_time =
    level.time + BG_Missile(weapon, mode)->tripwire_check_frequency;
  bolt->s.eType = ET_MISSILE;
  bolt->r.ownerNum = bolt->s.otherEntityNum = self->s.number;
  bolt->parent = self;
  bolt->s.weapon = weapon;
  bolt->s.generic1 = mode;
  if(BG_Missile(weapon, mode)->impact_create_portal != PORTAL_NONE) {
    bolt->s.modelindex2 = BG_Missile(weapon, mode)->impact_create_portal;
  }
  bolt->classname = BG_Missile(weapon, mode)->class_name;
  bolt->methodOfDeath = BG_Missile(weapon, mode)->mod;
  bolt->splashMethodOfDeath = BG_Missile(weapon, mode)->splash_mod;
  if(BG_Missile(weapon, mode)->charged_damage) {
    bolt->damage =
      (
        self->client->ps.misc[MISC_MISC] *
        BG_Missile(weapon, mode)->damage) /
      BG_Missile(weapon, mode)->charged_time_max;
    bolt->splashDamage = bolt->damage / 2;
  } else {
    bolt->damage = BG_Missile(weapon, mode)->damage;
    bolt->splashDamage = BG_Missile(weapon, mode)->splash_damage;
  }
  bolt->splashRadius = BG_Missile(weapon, mode)->splash_radius;
  bolt->pointAgainstWorld = BG_Missile(weapon, mode)->point_against_world;
  VectorCopy(BG_Missile(weapon, mode)->mins, bolt->r.mins);
  VectorCopy(BG_Missile(weapon, mode)->maxs, bolt->r.maxs);
  bolt->s.pos.trType = BG_Missile(weapon, mode)->trajectory_type;
  if(BG_Missile(weapon, mode)->trajectory_type == TR_ACCEL) {
    bolt->s.pos.trDuration = BG_Missile(weapon, mode)->speed * 2;
  }
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  bolt->kick_up_time = level.time + BG_Missile(weapon, mode)->kick_up_time;

  if(self->client) {
    bolt->buildableTeam = self->client->ps.stats[STAT_TEAM];
  } else {
    bolt->buildableTeam = self->buildableTeam;
  }

  bolt->s.otherEntityNum2 = bolt->buildableTeam;

  if(BG_Missile(weapon, mode)->charged_speed) {
    speed =
      (
        BG_Missile(weapon, mode)->charged_speed_min +
        (
          self->client->ps.misc[MISC_MISC] -
          BG_Missile(weapon, mode)->charged_time_max) *
          (
            BG_Missile(weapon, mode)->speed -
            BG_Missile(weapon, mode)->charged_speed_min) /
          (
            (
              (
                BG_Missile(weapon, mode)->charged_time_max *
                BG_Missile(weapon, mode)->charged_speed_min_damage_mod) /
              BG_Missile(weapon, mode)->charged_damage) -
              BG_Missile(weapon, mode)->charged_time_max));
  } else {
    speed = BG_Missile(weapon, mode)->speed;
  }

  if(
    BG_Missile(weapon, mode)->charged_damage ||
    BG_Missile(weapon, mode)->charged_speed) {
    self->client->ps.misc[ MISC_MISC ] = 0;
  }

  bolt->scale_missile.enabled = BG_Missile(weapon, mode)->scale;
  bolt->scale_missile.start_time =
    bolt->s.pos.trTime +BG_Missile(weapon, mode)->scale_start_time;
  bolt->scale_missile.stop_time =
    bolt->scale_missile.start_time + BG_Missile(weapon, mode)->scale_stop_time;
  VectorCopy(bolt->r.mins, bolt->scale_missile.start_mins);
  VectorCopy(bolt->r.maxs, bolt->scale_missile.start_maxs);
  VectorCopy(
    BG_Missile(weapon, mode)->scale_stop_mins, bolt->scale_missile.end_mins);
  VectorCopy(
    BG_Missile(weapon, mode)->scale_stop_maxs, bolt->scale_missile.end_maxs);

  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale(dir, speed, bolt->s.pos.trDelta);
  if(self->client) {
    BG_ModifyMissleLaunchVelocity(
      self->s.pos.trDelta, self->client->ps.speed, bolt->s.pos.trDelta,
      weapon, mode);
  } else {
    float speed = VectorLength(self->s.pos.trDelta);

    BG_ModifyMissleLaunchVelocity(
      self->s.pos.trDelta, speed, bolt->s.pos.trDelta,
      weapon, mode);
  }
  SnapVector(bolt->s.pos.trDelta);      // save net bandwidth
  VectorCopy(start, bolt->r.currentOrigin);

  G_SetClipmask(
    bolt,
    BG_Missile(weapon, mode)->clip_mask.include,
    BG_Missile(weapon, mode)->clip_mask.exclude);

  if(BG_Missile(weapon, mode)->health > 0) {
    bolt->die = G_MissileDie;
    bolt->health = BG_Missile(weapon, mode)->health;
    bolt->takedamage = qtrue;
    G_SetContents(bolt, CONTENTS_BODY, qfalse);
    SV_LinkEntity(bolt);
  } else {
    G_SetContents(bolt, 0, qfalse);
  }

  if(BG_Missile(weapon, mode)->save_missiles) {
    G_Entity_UEID_set(
      &self->saved_missiles[
        self->num_saved_missiles[weapon][mode]][weapon][mode],
      bolt);
    self->num_saved_missiles[weapon][mode]++;

    if(
      BG_Weapon(weapon)->usesBarbs &&
      self->client &&
      BG_Missile(weapon, mode)->detonate_saved_missiles &&
      (
        !(self->client->pers.cmd.buttons & BUTTON_USE_HOLDABLE) ||
        self->client->ps.ammo <= 0)) {
      G_Detonate_Saved_Missiles(self);
    }
  }
}

//=============================================================================

/*
================
G_PlayerHasUnexplodedGrenades

Checks to see if a player still has unexploded grenades.
================
 */
qboolean G_PlayerHasUnexplodedGrenades( gentity_t *player )
{
  gentity_t *ent;

  if( !g_nadeSpamProtection.integer )
    return qfalse;

  // just in case this was called for a non-player
  if( !player->client )
    return qfalse;

  // loop through all entities
  for( ent = &g_entities[ MAX_CLIENTS ]; ent < &g_entities[ level.num_entities ]; ++ent )
  {
    // return true if an unexploded grenade belonging to this player is found
    if( ent->s.eType == ET_MISSILE && ent->s.weapon == WP_GRENADE &&
        ent->parent->client == player->client )
      return qtrue;
  }

  return qfalse;
}

//=============================================================================
