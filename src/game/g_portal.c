/*
===========================================================================
Copyright (C) 2008-2009 Amanieu d'Antras
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

#define PORTAL_MINRANGE 20.0f
#define PORTAL_MAXRANGE 100.0f
#define PORTAL_OFFSET sqrt( (PORTALGUN_SIZE * PORTALGUN_SIZE) + (PORTALGUN_SIZE * PORTALGUN_SIZE) )

/*
===============
G_Portal_Effect

Cool effects for the portals when they are used or cleared
===============
*/
static void G_Portal_Effect( portal_t portalindex, float speedMod )
{
	gentity_t *portal = level.humanPortals.portals[portalindex];
	gentity_t *effect;
	int       speed = (int)((float)(PORTALGUN_SPEED) * speedMod);

	if(!portal) {
		return;
	}

	if( !G_Expired( portal, EXP_PORTAL_EFFECT ) )
		return;

	G_SetExpiration( portal, EXP_PORTAL_PARTICLES, 500 );

	effect = G_Spawn( );

	VectorCopy( portal->r.currentOrigin , effect->r.currentOrigin );
	effect->s.eType = ET_MISSILE;
	effect->s.weapon = WP_PORTAL_GUN;
	if( portalindex == PORTAL_RED )
		effect->s.generic1 = WPM_PRIMARY;
	else
		effect->s.generic1 = WPM_SECONDARY;

	effect->s.pos.trType = TR_ACCEL;
  effect->s.pos.trDuration = speed * 2;
  effect->s.pos.trTime = level.time;
  VectorCopy( effect->r.currentOrigin, effect->s.pos.trBase );
  VectorScale( portal->s.origin2, speed, effect->s.pos.trDelta );
  SnapVector( effect->s.pos.trDelta );      // save net bandwidth

	if( G_Expired( portal, EXP_PORTAL_PARTICLES ) )
	{
		G_AddEvent( effect, EV_MISSILE_MISS, DirToByte( portal->s.origin2 ) );
		G_SetExpiration( portal, EXP_PORTAL_PARTICLES, 1000 );
	} else
		G_AddEvent( effect, EV_MISSILE_HIT, DirToByte( portal->s.origin2 ) );

	effect->freeAfterEvent = qtrue;
	SV_LinkEntity( effect );
}

/*
===============
G_Portal_Clear

Delete a portal
===============
*/
void G_Portal_Clear( portal_t portalindex )
{
	gentity_t *self = level.humanPortals.portals[portalindex];
	if (!self)
		return;

	if(!self) {
		return;
	}

	G_Portal_Effect( portalindex, 0.25 );
	level.humanPortals.createTime[ portalindex ] = 0;
	SV_SetConfigstring( ( CS_HUMAN_PORTAL_CREATETIME + portalindex ),
												va( "%i", 0 ) );
	level.humanPortals.portals[portalindex] = NULL;
	G_FreeEntity( self );
}

/*
 =================
portal_destroy_think

A wrapper think function for delayed clearing of a portal
=================
*/
static void portal_destroy_think( gentity_t *self )
{
	if(!self) {
		return;
	}
	G_Portal_Clear( self->s.modelindex2 );
}

/*
 =================
portal_die

=================
*/
static void portal_die( gentity_t *self, gentity_t *inflictor,
                         gentity_t *attacker, int damage, int mod )
{
  self->nextthink = level.time + 50;
	self->think = portal_destroy_think;
}

/*
===============
G_Portal_Touch

Send someone over to the other portal.
===============
*/
static void G_Portal_Touch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	gentity_t *portal;
	portal_t  portal_index = PORTAL_NONE;
	vec3_t    origin, dir, end, angles;
	trace_t   tr;
	int       speed, i;

	if (!other->client)
		return;

	if( other->client->ps.weapon == WP_ALEVEL3_UPG ||
	 		other->client->ps.weapon == WP_ALEVEL4 )
		return;

	if(self->s.modelindex2 == PORTAL_RED) {
		portal_index = PORTAL_BLUE;
	} else if(self->s.modelindex2 == PORTAL_BLUE) {
		portal_index = PORTAL_RED;
	}

	portal = level.humanPortals.portals[ portal_index ];
	if (!portal)
		return;

	// Check if there is room to spawn
	VectorCopy(portal->r.currentOrigin, origin);
	VectorCopy(portal->s.origin2, dir);
	for (i = PORTAL_MINRANGE; i < PORTAL_MAXRANGE; i++)
    {
		VectorMA(origin, i, dir, end);
		SV_Trace(
			&tr, origin, NULL, NULL, end, portal->s.number, qfalse,
			*Temp_Clip_Mask(MASK_SHOT, 0), TT_AABB);
		if (tr.fraction != 1.0f)
    {
			return;
    }
		SV_Trace(&tr, end, other->r.mins, other->r.maxs, end, -1, qfalse,
			*Temp_Clip_Mask((MASK_PLAYERSOLID|CONTENTS_TELEPORTER), 0), TT_AABB);
		if (tr.fraction == 1.0f)
    {
			break;
    }
	}

	if (i == PORTAL_MAXRANGE)
		return;

	// Teleport!
	SV_UnlinkEntity(other);
	VectorCopy(end, other->client->ps.origin);
	speed = VectorLength(other->client->ps.velocity);
	VectorScale(portal->s.origin2, speed, other->client->ps.velocity);
	other->client->ps.eFlags ^= EF_TELEPORT_BIT;
	G_UnlaggedClear(other);
	if (dir[0] || dir[1])
  {
		if (other->client->portalTime < level.time)
    {
			vectoangles(dir, angles);
			G_SetClientViewAngle(other, angles);
		}
		other->client->portalTime = level.time + 250;
	}
	BG_PlayerStateToEntityState(&other->client->ps, &other->s, &other->client->pmext );
	VectorCopy(other->client->ps.origin, other->r.currentOrigin);
	SV_LinkEntity(other);

	// Protect against targeting by buildables
  other->targetProtectionTime = level.time + TELEPORT_PROTECTION_TIME;

	// Show off some fancy effects
	for( i = 0; i < PORTAL_NUM; i++ )
		G_Portal_Effect( i, 0.0625 );
}

/*
===============
G_Portal_Create

This is used to spawn a portal.
===============
*/
void G_Portal_Create(gentity_t *ent, vec3_t origin, vec3_t normal, portal_t portalindex)
{
	gentity_t *portal;
	vec3_t range = { PORTAL_MINRANGE, PORTAL_MINRANGE, PORTAL_MINRANGE };
	vec3_t oldOrigin;
	trace_t tr;

	if(portalindex == PORTAL_NONE) {
		return;
	}

	if ( ent->health <= 0 || !ent->client || ent->client->ps.pm_type == PM_DEAD )
		return;

	// Create the portal
	portal = G_Spawn();
	G_SetContents(portal, (CONTENTS_TRIGGER|CONTENTS_TELEPORTER), qfalse);
	portal->s.eType = ET_TELEPORTAL;
	portal->touch = G_Portal_Touch;
	portal->s.modelindex = BA_H_SPAWN;
	portal->s.modelindex2 = portalindex;
	portal->s.frame = 3;
	portal->takedamage = qtrue;
  portal->health = PORTAL_HEALTH;
  portal->die = portal_die;
	VectorCopy( range, portal->r.maxs );
	VectorScale( range, -1, portal->r.mins) ;
	VectorCopy( origin, oldOrigin );
	VectorMA( origin, -PORTAL_OFFSET, normal, origin );
	SV_Trace(
		&tr, oldOrigin, NULL, NULL, origin, ent->s.number, qfalse,
		*Temp_Clip_Mask(MASK_PLAYERSOLID, 0), TT_AABB);
	VectorCopy( tr.endpos, origin);
	G_SetOrigin( portal, origin );
	VectorCopy( normal, portal->s.origin2 );
	SV_LinkEntity( portal );

	// Attach it to the client
	G_Portal_Clear( portalindex );
	portal->parent = ent;
	level.humanPortals.portals[ portalindex ] = portal;
	level.humanPortals.lifetime[ portalindex ] = level.time + PORTAL_LIFETIME;
	level.humanPortals.createTime[ portalindex ] = PORTAL_CREATED_REPEAT;
	SV_SetConfigstring( ( CS_HUMAN_PORTAL_CREATETIME + portalindex ),
												va( "%i", level.humanPortals.createTime[ portalindex ] ) );
}
