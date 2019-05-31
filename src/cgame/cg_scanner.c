/*
===========================================================================
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


#include "cg_local.h"

static entityPos_t entityPositions;

#define HUMAN_SCANNER_UPDATE_PERIOD 50

/*
=============
CG_UpdateEntityPositions

Update this client's perception of entity positions
=============
*/
void CG_UpdateEntityPositions( void )
{
  centity_t *cent = NULL;
  int       i;

  if( cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_HUMANS )
  {
    if( entityPositions.lastUpdateTime + HUMAN_SCANNER_UPDATE_PERIOD > cg.time )
      return;
  }

  VectorCopy( cg.refdef.vieworg, entityPositions.origin );
  VectorCopy( cg.refdefViewAngles, entityPositions.vangles );
  entityPositions.lastUpdateTime = cg.time;

  entityPositions.numAlienBuildables = 0;
  entityPositions.numHumanBuildables = 0;
  entityPositions.numAlienClients = 0;
  entityPositions.numHumanClients = 0;

  for( i = 0; i < cg.snap->numEntities; i++ )
  {
    cent = &cg_entities[ cg.snap->entities[ i ].number ];

    if( cent->currentState.eType == ET_BUILDABLE &&
     !( cent->currentState.eFlags & EF_DEAD ) )
    {
      // add to list of item positions (for creep)
      if( cent->currentState.modelindex2 == TEAM_ALIENS )
      {
        VectorCopy( cent->lerpOrigin, entityPositions.alienBuildablePos[
            entityPositions.numAlienBuildables ] );
        entityPositions.alienBuildableTimes[
            entityPositions.numAlienBuildables ] = cent->miscTime;
        entityPositions.alienBuildable[
            entityPositions.numAlienBuildables ] = cent;

        if( entityPositions.numAlienBuildables < MAX_GENTITIES )
          entityPositions.numAlienBuildables++;
      }
      else if( cent->currentState.modelindex2 == TEAM_HUMANS )
      {
        VectorCopy( cent->lerpOrigin, entityPositions.humanBuildablePos[
            entityPositions.numHumanBuildables ] );
        entityPositions.humanBuildable[
            entityPositions.numHumanBuildables ] = cent;

        if( entityPositions.numHumanBuildables < MAX_GENTITIES )
          entityPositions.numHumanBuildables++;
      }
    }
    else if( cent->currentState.eType == ET_PLAYER )
    {
      int team = cent->currentState.misc & 0x00FF;

      if( team == TEAM_ALIENS )
      {
        VectorCopy( cent->lerpOrigin, entityPositions.alienClientPos[
            entityPositions.numAlienClients ] );
        entityPositions.alienClient[ entityPositions.numAlienClients ] = cent;

        if( entityPositions.numAlienClients < MAX_CLIENTS )
          entityPositions.numAlienClients++;
      }
      else if( team == TEAM_HUMANS )
      {
        VectorCopy( cent->lerpOrigin, entityPositions.humanClientPos[
            entityPositions.numHumanClients ] );
        entityPositions.humanClient[ entityPositions.numHumanClients ] = cent;

        if( entityPositions.numHumanClients < MAX_CLIENTS )
          entityPositions.numHumanClients++;
      }
    }
  }
}

#define STALKWIDTH  (2.0f * cgDC.aspectScale)
#define BLIPX       (16.0f * cgDC.aspectScale)
#define BLIPY       8.0f
#define FAR_ALPHA   0.8f
#define NEAR_ALPHA  1.2f

static float FindDegree(float y, float x) {
    float value = -(float)(RAD2DEG(atan2(y, x)));

    value += AngleNormalize360(entityPositions.vangles[YAW]);

    return AngleNormalize360(value);
}

/*
=============
CG_DrawBlips

Draw blips and stalks for the human scanner
=============
*/
static void CG_DrawBlips( rectDef_t *rect, vec3_t origin, vec4_t colour )
{
  vec3_t  drawOrigin;
  float   timeFractionSinceRefresh = 1.0f -
    ( (float)( cg.time - entityPositions.lastUpdateTime ) /
      (float)HUMAN_SCANNER_UPDATE_PERIOD );
  vec4_t  localColour;
  float orientation_offset_angle = FindDegree(origin[1], origin[0]); 
  float orientation_offset;
  float fov = AngleNormalize360(cg.refdef.fov_x);
  float half_fov = fov / 2.0f;
  float clip_offset =
    ((360.0f - fov) / 360.0f) * rect->w;

  Vector4Copy( colour, localColour );

  if(
    orientation_offset_angle > half_fov && orientation_offset_angle <= 180.0f) {
    orientation_offset_angle = half_fov;
  } else if(
    orientation_offset_angle > 180.0f &&
    orientation_offset_angle < 360.0f - half_fov) {
    orientation_offset_angle = 360.0f - half_fov;
  }

  orientation_offset = (orientation_offset_angle / 360.0f) * rect->w;

  drawOrigin[ 0 ] = rect->x + (rect->w / 2.0) + orientation_offset;
  drawOrigin[ 1 ] = rect->y + (rect->h / 2.0);
  drawOrigin[2] = origin[2];
  drawOrigin[2] /= ( 2 * HELMET_RANGE / rect->h );

  localColour[ 3 ] *= ( 0.5f + ( timeFractionSinceRefresh * 0.5f ) );

  if( localColour[ 3 ] > 1.0f )
    localColour[ 3 ] = 1.0f;
  else if( localColour[ 3 ] < 0.0f )
    localColour[ 3 ] = 0.0f;

  CG_SetClipRegion(
    rect->x + (clip_offset / 2), rect->y, rect->w - clip_offset, rect->h);
  trap_R_SetColor( localColour );

  if( drawOrigin[ 2 ] > 0 ) {
    CG_DrawPic( drawOrigin[ 0 ] - ( STALKWIDTH / 2 ),
                drawOrigin[ 1 ] - drawOrigin[ 2 ],
                STALKWIDTH, drawOrigin[ 2 ], cgs.media.scannerLineShader );
    CG_DrawPic( drawOrigin[ 0 ] - ( STALKWIDTH / 2 ) - rect->w,
                drawOrigin[ 1 ] - drawOrigin[ 2 ],
                STALKWIDTH, drawOrigin[ 2 ], cgs.media.scannerLineShader );
  } else {
    CG_DrawPic( drawOrigin[ 0 ] - ( STALKWIDTH / 2 ),
                drawOrigin[ 1 ],
                STALKWIDTH, -drawOrigin[ 2 ], cgs.media.scannerLineShader );
    CG_DrawPic( drawOrigin[ 0 ] - ( STALKWIDTH / 2 ) - rect->w,
                drawOrigin[ 1 ],
                STALKWIDTH, -drawOrigin[ 2 ], cgs.media.scannerLineShader );
  }
  CG_DrawPic( drawOrigin[ 0 ] - ( BLIPX / 2 ),
              drawOrigin[ 1 ] - ( BLIPY / 2 ) - drawOrigin[ 2 ],
              BLIPX, BLIPY, cgs.media.scannerBlipShader );
  CG_DrawPic( drawOrigin[ 0 ] - ( BLIPX / 2 ) - rect->w,
               drawOrigin[ 1 ] - ( BLIPY / 2 ) - drawOrigin[ 2 ],
              BLIPX, BLIPY, cgs.media.scannerBlipShader );
  trap_R_SetColor( NULL );
  CG_ClearClipRegion( );
}

#define BLIPX2  (24.0f * cgDC.aspectScale)
#define BLIPY2  24.0f

/*
=============
CG_DrawDir

Draw dot marking the direction to an enemy
=============
*/
static void CG_DrawDir( rectDef_t *rect, vec3_t origin, vec4_t colour )
{
  vec3_t  drawOrigin;
  vec3_t  noZOrigin;
  vec3_t  normal, antinormal, normalDiff;
  vec3_t  view, noZview;
  vec3_t  up  = { 0.0f, 0.0f,   1.0f };
  vec3_t  top = { 0.0f, -1.0f,  0.0f };
  float   angle;
  playerState_t *ps = &cg.snap->ps;

  BG_GetClientNormal( ps, normal );

  AngleVectors( entityPositions.vangles, view, NULL, NULL );

  ProjectPointOnPlane( noZOrigin, origin, normal );
  ProjectPointOnPlane( noZview, view, normal );
  VectorNormalize( noZOrigin );
  VectorNormalize( noZview );

  //calculate the angle between the images of the blip and the view
  angle = RAD2DEG( acos( DotProduct( noZOrigin, noZview ) ) );
  CrossProduct( noZOrigin, noZview, antinormal );
  VectorNormalize( antinormal );

  //decide which way to rotate
  VectorSubtract( normal, antinormal, normalDiff );
  if( VectorLength( normalDiff ) < 1.0f )
    angle = 360.0f - angle;

  RotatePointAroundVector( drawOrigin, up, top, angle );

  trap_R_SetColor( colour );
  CG_DrawPic( rect->x + ( rect->w / 2 ) - ( BLIPX2 / 2 ) - drawOrigin[ 0 ] * ( rect->w / 2 ),
              rect->y + ( rect->h / 2 ) - ( BLIPY2 / 2 ) + drawOrigin[ 1 ] * ( rect->h / 2 ),
              BLIPX2, BLIPY2, cgs.media.scannerBlipShader );
  trap_R_SetColor( NULL );
}

/*
=============
CG_AlienSense
=============
*/
void CG_AlienSense( rectDef_t *rect )
{
  int     i;
  vec3_t  origin;
  vec3_t  relOrigin;
  vec4_t  hBuildable = { 1.0f, 0.0f, 0.0f, 0.7f };
  vec4_t  hClient    = { 0.0f, 0.0f, 1.0f, 0.7f };
  vec4_t  aBuildable = { 0.0f, 1.0f, 0.0f, 0.7f };
  vec4_t  aClient    = { 1.0f, 1.0f, 0.0f, 0.7f };


  VectorCopy( entityPositions.origin, origin );

  //draw alien buildables
  for( i = 0; i < entityPositions.numAlienBuildables; i++ )
  {
    VectorClear( relOrigin );
    VectorSubtract( entityPositions.alienBuildablePos[ i ], origin, relOrigin );

    if( VectorLength( relOrigin ) < ALIENSENSE_RANGE &&
        ( !CG_Visible( entityPositions.alienBuildable[ i ], MASK_DEADSOLID ) ||
          entityPositions.alienBuildable[ i ]->currentState.modelindex == BA_A_OVERMIND ) )
      CG_DrawDir( rect, relOrigin, aBuildable );
  }

  //draw alien clients
  for( i = 0; i < entityPositions.numAlienClients; i++ )
  {
    VectorClear( relOrigin );
    VectorSubtract( entityPositions.alienClientPos[ i ], origin, relOrigin );

    if( VectorLength( relOrigin ) < ALIENSENSE_RANGE )
      CG_DrawDir( rect, relOrigin, aClient );
  }

  //draw human buildables
  for( i = 0; i < entityPositions.numHumanBuildables; i++ )
  {
    VectorClear( relOrigin );
    VectorSubtract( entityPositions.humanBuildablePos[ i ], origin, relOrigin );

    if( VectorLength( relOrigin ) < ALIENSENSE_RANGE &&
        ( ( entityPositions.humanBuildable[ i ]->currentState.eFlags & EF_SCAN_SPOTTED ) ||
           CG_Visible( entityPositions.humanBuildable[ i ], MASK_DEADSOLID ) ) )
      CG_DrawDir( rect, relOrigin, hBuildable );
  }

  //draw human clients
  for( i = 0; i < entityPositions.numHumanClients; i++ )
  {
    VectorClear( relOrigin );
    VectorSubtract( entityPositions.humanClientPos[ i ], origin, relOrigin );

    if( VectorLength( relOrigin ) < ALIENSENSE_RANGE &&
        !( ( entityPositions.humanClient[ i ]->currentState.eFlags & EF_INVISIBILE ) &&
           entityPositions.humanClient[ i ]->currentState.weapon != WP_LUCIFER_CANNON ) &&
        ( ( entityPositions.humanClient[ i ]->currentState.eFlags & EF_SCAN_SPOTTED ) ||
           CG_Visible( entityPositions.humanClient[ i ], MASK_DEADSOLID ) ) )
      CG_DrawDir( rect, relOrigin, hClient );
  }
}

/*
=============
CG_Scanner
=============
*/
void CG_Scanner( rectDef_t *rect, qhandle_t shader, vec4_t color )
{
  int     i;
  vec3_t  origin;
  vec3_t  relOrigin;
  vec4_t  hIabove;
  vec4_t  aIabove = { 1.0f, 0.0f, 0.0f, 0.9f };

  Vector4Copy( color, hIabove );
  hIabove[ 3 ] = 0.9f;

  VectorCopy( entityPositions.origin, origin );

  //draw human buildables above scanner plane
  for( i = 0; i < entityPositions.numHumanBuildables; i++ )
  {
    VectorClear( relOrigin );
    VectorSubtract( entityPositions.humanBuildablePos[ i ], origin, relOrigin );

    if( VectorLength( relOrigin ) < HELMET_RANGE &&
        ( !CG_Visible( entityPositions.humanBuildable[ i ], MASK_DEADSOLID ) ||
            entityPositions.humanBuildable[ i ]->currentState.modelindex == BA_H_REACTOR ) )
      CG_DrawBlips( rect, relOrigin, hIabove );
  }

  //draw human clients above scanner plane
  for( i = 0; i < entityPositions.numHumanClients; i++ )
  {
    VectorClear( relOrigin );
    VectorSubtract( entityPositions.humanClientPos[ i ], origin, relOrigin );

    if( VectorLength( relOrigin ) < HELMET_RANGE )
      CG_DrawBlips( rect, relOrigin, hIabove );
  }

  //draw alien buildables above scanner plane
  for( i = 0; i < entityPositions.numAlienBuildables; i++ )
  {
    VectorClear( relOrigin );
    VectorSubtract( entityPositions.alienBuildablePos[ i ], origin, relOrigin );

    if( VectorLength( relOrigin ) < HELMET_RANGE &&
        ( ( entityPositions.alienBuildable[ i ]->currentState.eFlags & EF_SCAN_SPOTTED ) ||
           CG_Visible( entityPositions.alienBuildable[ i ], MASK_DEADSOLID ) ) )
      CG_DrawBlips( rect, relOrigin, aIabove );
  }

  //draw alien clients above scanner plane
  for( i = 0; i < entityPositions.numAlienClients; i++ )
  {
    VectorClear( relOrigin );
    VectorSubtract( entityPositions.alienClientPos[ i ], origin, relOrigin );

    if( VectorLength( relOrigin ) < HELMET_RANGE &&
        !( ( entityPositions.alienClient[ i ]->currentState.eFlags & EF_INVISIBILE ) &&
           entityPositions.alienClient[ i ]->currentState.weapon != WP_LUCIFER_CANNON ) &&
        ( ( entityPositions.alienClient[ i ]->currentState.eFlags & EF_SCAN_SPOTTED ) ||
          CG_Visible( entityPositions.alienClient[ i ], MASK_DEADSOLID ) ) )
      CG_DrawBlips( rect, relOrigin, aIabove );
  }
}
