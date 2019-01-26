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

// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay


#include "cg_local.h"
#include "../ui/ui_shared.h"

menuDef_t *menuScoreboard = NULL;

static void CG_AlignText( rectDef_t *rect, const char *text, float scale,
                          float w, float h,
                          int align, int valign,
                          float *x, float *y )
{
  float tx, ty;

  if( scale > 0.0f )
  {
    w = UI_Text_Width( text, scale );
    h = UI_Text_Height( text, scale );
  }

  switch( align )
  {
    default:
    case ALIGN_LEFT:
      tx = 0.0f;
      break;

    case ALIGN_RIGHT:
      tx = rect->w - w;
      break;

    case ALIGN_CENTER:
      tx = ( rect->w - w ) / 2.0f;
      break;

    case ALIGN_NONE:
      tx = 0;
      break;
  }

  switch( valign )
  {
    default:
    case VALIGN_BOTTOM:
      ty = rect->h;
      break;

    case VALIGN_TOP:
      ty = h;
      break;

    case VALIGN_CENTER:
      ty = h + ( ( rect->h - h ) / 2.0f );
      break;

    case VALIGN_NONE:
      ty = 0;
      break;
  }

  if( x )
    *x = rect->x + tx;

  if( y )
    *y = rect->y + ty;
}

/*
==============
CG_DrawFieldPadded

Draws large numbers for status bar
==============
*/
static void CG_DrawFieldPadded( int x, int y, int width, int cw, int ch, int value )
{
  char  num[ 16 ], *ptr;
  int   l, orgL;
  int   frame;
  int   charWidth, charHeight;

  if( !( charWidth = cw ) )
    charWidth = CHAR_WIDTH;

  if( !( charHeight = ch ) )
    charHeight = CHAR_HEIGHT;

  if( width < 1 )
    return;

  // draw number string
  if( width > 4 )
    width = 4;

  switch( width )
  {
    case 1:
      value = value > 9 ? 9 : value;
      value = value < 0 ? 0 : value;
      break;
    case 2:
      value = value > 99 ? 99 : value;
      value = value < -9 ? -9 : value;
      break;
    case 3:
      value = value > 999 ? 999 : value;
      value = value < -99 ? -99 : value;
      break;
    case 4:
      value = value > 9999 ? 9999 : value;
      value = value < -999 ? -999 : value;
      break;
  }

  Com_sprintf( num, sizeof( num ), "%d", value );
  l = strlen( num );

  if( l > width )
    l = width;

  orgL = l;

  x += ( 2.0f * cgDC.aspectScale );

  ptr = num;
  while( *ptr && l )
  {
    if( width > orgL )
    {
      CG_DrawPic( x,y, charWidth, charHeight, cgs.media.numberShaders[ 0 ] );
      width--;
      x += charWidth;
      continue;
    }

    if( *ptr == '-' )
      frame = STAT_MINUS;
    else
      frame = *ptr - '0';

    CG_DrawPic( x,y, charWidth, charHeight, cgs.media.numberShaders[ frame ] );
    x += charWidth;
    ptr++;
    l--;
  }
}

/*
==============
CG_DrawField

Draws large numbers for status bar
==============
*/
void CG_DrawField( float x, float y, int width, float cw, float ch, int value )
{
  char  num[ 16 ], *ptr;
  int   l;
  int   frame;
  float charWidth, charHeight;

  if( !( charWidth = cw ) )
    charWidth = CHAR_WIDTH;

  if( !( charHeight = ch ) )
    charHeight = CHAR_HEIGHT;

  if( width < 1 )
    return;

  // draw number string
  if( width > 4 )
    width = 4;

  switch( width )
  {
    case 1:
      value = value > 9 ? 9 : value;
      value = value < 0 ? 0 : value;
      break;
    case 2:
      value = value > 99 ? 99 : value;
      value = value < -9 ? -9 : value;
      break;
    case 3:
      value = value > 999 ? 999 : value;
      value = value < -99 ? -99 : value;
      break;
    case 4:
      value = value > 9999 ? 9999 : value;
      value = value < -999 ? -999 : value;
      break;
  }

  Com_sprintf( num, sizeof( num ), "%d", value );
  l = strlen( num );

  if( l > width )
    l = width;

  x += ( 2.0f * cgDC.aspectScale ) + charWidth * ( width - l );

  ptr = num;
  while( *ptr && l )
  {
    if( *ptr == '-' )
      frame = STAT_MINUS;
    else
      frame = *ptr -'0';

    CG_DrawPic( x,y, charWidth, charHeight, cgs.media.numberShaders[ frame ] );
    x += charWidth;
    ptr++;
    l--;
  }
}

static void CG_DrawProgressBar( rectDef_t *rect, vec4_t color, float scale,
                                int align, int textalign, int textStyle,
                                float borderSize, float progress )
{
  float   rimWidth;
  float   doneWidth, leftWidth;
  float   tx, ty;
  char    textBuffer[ 8 ];

  if( borderSize >= 0.0f )
    rimWidth = borderSize;
  else
  {
    rimWidth = rect->h / 20.0f;
    if( rimWidth < 0.6f )
      rimWidth = 0.6f;
  }

  if( progress < 0.0f )
    progress = 0.0f;
  else if( progress > 1.0f )
    progress = 1.0f;

  doneWidth = ( rect->w - 2 * rimWidth ) * progress;
  leftWidth = ( rect->w - 2 * rimWidth ) - doneWidth;

  trap_R_SetColor( color );

  //draw rim and bar
  if( align == ALIGN_RIGHT )
  {
    CG_DrawPic( rect->x, rect->y, rimWidth, rect->h, cgs.media.whiteShader );
    CG_DrawPic( rect->x + rimWidth, rect->y,
      leftWidth, rimWidth, cgs.media.whiteShader );
    CG_DrawPic( rect->x + rimWidth, rect->y + rect->h - rimWidth,
      leftWidth, rimWidth, cgs.media.whiteShader );
    CG_DrawPic( rect->x + rimWidth + leftWidth, rect->y,
      rimWidth + doneWidth, rect->h, cgs.media.whiteShader );
  }
  else
  {
    CG_DrawPic( rect->x, rect->y, rimWidth + doneWidth, rect->h, cgs.media.whiteShader );
    CG_DrawPic( rimWidth + rect->x + doneWidth, rect->y,
      leftWidth, rimWidth, cgs.media.whiteShader );
    CG_DrawPic( rimWidth + rect->x + doneWidth, rect->y + rect->h - rimWidth,
      leftWidth, rimWidth, cgs.media.whiteShader );
    CG_DrawPic( rect->x + rect->w - rimWidth, rect->y, rimWidth, rect->h, cgs.media.whiteShader );
  }

  trap_R_SetColor( NULL );

  //draw text
  if( scale > 0.0 )
  {
    Com_sprintf( textBuffer, sizeof( textBuffer ), "%d%%", (int)( progress * 100 ) );
    CG_AlignText( rect, textBuffer, scale, 0.0f, 0.0f, textalign, VALIGN_CENTER, &tx, &ty );

    UI_Text_Paint( tx, ty, scale, color, textBuffer, 0, 0, textStyle );
  }
}

//=============== TA: was cg_newdraw.c

#define NO_CREDITS_TIME 2000

static void CG_DrawPlayerCreditsValue( rectDef_t *rect, vec4_t color, qboolean padding )
{
  int           value;
  playerState_t *ps;
  centity_t     *cent;

  cent = &cg_entities[ cg.snap->ps.clientNum ];
  ps = &cg.snap->ps;

  //if the build timer pie is showing don't show this
  if( ( cent->currentState.weapon == WP_ABUILD ||
      cent->currentState.weapon == WP_ABUILD2 ) && ps->misc[ MISC_MISC ] )
    return;

  value = ps->persistant[ PERS_CREDIT ];
  if( value > -1 )
  {
    if( cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_ALIENS )
    {
      if( !BG_AlienCanEvolve( cg.predictedPlayerState.stats[STAT_CLASS],
                              value, cgs.alienStage, cgs.warmup, cgs.devMode,
                              (cg.predictedPlayerState.stats[STAT_FLAGS] & SFL_CLASS_FORCED)) &&
          cg.time - cg.lastEvolveAttempt <= NO_CREDITS_TIME &&
          ( ( cg.time - cg.lastEvolveAttempt ) / 300 ) & 1 )
      {
        color[ 3 ] = 0.0f;
      }

      value /= ALIEN_CREDITS_PER_KILL;
    }

    trap_R_SetColor( color );

    if( padding )
      CG_DrawFieldPadded( rect->x, rect->y, 4, rect->w / 4, rect->h, value );
    else
      CG_DrawField( rect->x, rect->y, 1, rect->w, rect->h, value );

    trap_R_SetColor( NULL );
  }
}

static void CG_DrawPlayerCreditsFraction( rectDef_t *rect, vec4_t color, qhandle_t shader )
{
  float fraction;
  float height;

  if( cg.predictedPlayerState.stats[ STAT_TEAM ] != TEAM_ALIENS )
    return;

  fraction = ((float)(cg.predictedPlayerState.persistant[ PERS_CREDIT ] %
    ALIEN_CREDITS_PER_KILL)) / ALIEN_CREDITS_PER_KILL;

  CG_AdjustFrom640( &rect->x, &rect->y, &rect->w, &rect->h );
  height = rect->h * fraction;

  trap_R_SetColor( color );
  trap_R_DrawStretchPic( rect->x, rect->y - height + rect->h, rect->w,
    height, 0.0f, 1.0f - fraction, 1.0f, 1.0f, shader );
  trap_R_SetColor( NULL );
}


/*
==============
CG_DrawPlayerStamina
==============
*/
static void CG_DrawPlayerStamina( int ownerDraw, rectDef_t *rect,
                                  vec4_t backColor, vec4_t foreColor,
                                  qhandle_t shader )
{
  playerState_t *ps = &cg.snap->ps;
  float         stamina = ps->stats[ STAT_STAMINA ];
  float         maxStaminaBy3 = (float)STAMINA_MAX / 3.0f;
  float         progress;
  vec4_t        color;

  switch( ownerDraw )
  {
    case CG_PLAYER_STAMINA_1:
      progress = ( stamina - 2 * (int)maxStaminaBy3 ) / maxStaminaBy3;
      break;
    case CG_PLAYER_STAMINA_2:
      progress = ( stamina - (int)maxStaminaBy3 ) / maxStaminaBy3;
      break;
    case CG_PLAYER_STAMINA_3:
  progress = stamina / maxStaminaBy3;
      break;
    case CG_PLAYER_STAMINA_4:
      progress = ( stamina + STAMINA_MAX ) / STAMINA_MAX;
      break;
    default:
      return;
  }

  if( progress > 1.0f )
    progress  = 1.0f;
  else if( progress < 0.0f )
    progress = 0.0f;

  Vector4Lerp( progress, backColor, foreColor, color );

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawPlayerStaminaBolt
==============
*/
static void CG_DrawPlayerStaminaBolt( rectDef_t *rect, vec4_t backColor,
                                      vec4_t foreColor, qhandle_t shader )
{
  float         stamina = cg.snap->ps.stats[ STAT_STAMINA ];
  vec4_t        color;

  if( cg.predictedPlayerState.stats[ STAT_STATE ] & SS_SPEEDBOOST )
  {
    if( stamina >= 0 )
      Vector4Lerp( ( sin( cg.time / 150.0f ) + 1 ) / 2,
                   backColor, foreColor, color );
    else
      Vector4Lerp( ( sin( cg.time / 2000.0f ) + 1 ) / 2,
                   backColor, foreColor, color );
  }
  else
  {
    if( stamina < 0 )
      Vector4Copy( backColor, color );
    else
      Vector4Copy( foreColor, color );
  }

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawPlayerClipsRing
==============
*/
static void CG_DrawPlayerClipsRing( rectDef_t *rect, vec4_t backColor,
                                    vec4_t foreColor, qhandle_t shader )
{
  playerState_t *ps = &cg.snap->ps;
  centity_t     *cent;
  float         buildTime = ps->misc[ MISC_MISC ];
  float         progress;
  float         maxDelay;
  weapon_t      weapon;
  vec4_t        color;

  cent = &cg_entities[ cg.snap->ps.clientNum ];
  weapon = BG_GetPlayerWeapon( ps );

  switch( weapon )
  {
    case WP_ABUILD:
    case WP_ABUILD2:
    case WP_HBUILD:
      if( buildTime > MAXIMUM_BUILD_TIME )
        buildTime = MAXIMUM_BUILD_TIME;
      progress = ( MAXIMUM_BUILD_TIME - buildTime ) / MAXIMUM_BUILD_TIME;

      Vector4Lerp( progress, backColor, foreColor, color );
      break;

    default:
      if( ps->weaponstate == WEAPON_RELOADING )
      {
        maxDelay = (float)BG_Weapon( cent->currentState.weapon )->reloadTime;
        progress = ( maxDelay - (float)ps->weaponTime ) / maxDelay;

        Vector4Lerp( progress, backColor, foreColor, color );
      }
      else
        Com_Memcpy( color, foreColor, sizeof( color ) );
      break;
  }

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawPlayerBuildTimerRing
==============
*/
static void CG_DrawPlayerBuildTimerRing( rectDef_t *rect, vec4_t backColor,
                                         vec4_t foreColor, qhandle_t shader )
{
  playerState_t *ps = &cg.snap->ps;
  float         buildTime = ps->misc[ MISC_MISC ];
  float         progress;
  vec4_t        color;

  if( buildTime > MAXIMUM_BUILD_TIME )
    buildTime = MAXIMUM_BUILD_TIME;

  progress = ( MAXIMUM_BUILD_TIME - buildTime ) / MAXIMUM_BUILD_TIME;

  Vector4Lerp( progress, backColor, foreColor, color );

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawPlayerBoosted
==============
*/
static void CG_DrawPlayerBoosted( rectDef_t *rect, vec4_t backColor,
                                  vec4_t foreColor, qhandle_t shader )
{
  if( cg.snap->ps.stats[ STAT_STATE ] & SS_BOOSTED )
    trap_R_SetColor( foreColor );
  else
    trap_R_SetColor( backColor );

  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawPlayerBoosterBolt
==============
*/
static void CG_DrawPlayerBoosterBolt( rectDef_t *rect, vec4_t backColor,
                                      vec4_t foreColor, qhandle_t shader )
{
  vec4_t color;

  // Flash bolts when the boost is almost out
  if( ( cg.snap->ps.stats[ STAT_STATE ] & SS_BOOSTED ) &&
      ( cg.snap->ps.stats[ STAT_STATE ] & SS_BOOSTEDWARNING ) )
    Vector4Lerp( ( sin( cg.time / 100.0f ) + 1 ) / 2,
                 backColor, foreColor, color );
  else
    Vector4Copy( foreColor, color );

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawPlayerPoisonBarbs
==============
*/
static void CG_DrawPlayerPoisonBarbs( rectDef_t *rect, vec4_t color, qhandle_t shader )
{
  qboolean vertical;
  float    x = rect->x, y = rect->y;
  float    width = rect->w, height = rect->h;
  float    diff;
  int      iconsize, numBarbs, maxBarbs;

  maxBarbs = BG_Weapon( cg.snap->ps.weapon )->maxAmmo;
  numBarbs = cg.snap->ps.ammo;
  if( maxBarbs <= 0 || numBarbs <= 0 )
    return;

  // adjust these first to ensure the aspect ratio of the barb image is
  // preserved
  CG_AdjustFrom640( &x, &y, &width, &height );

  if( height > width )
  {
    vertical = qtrue;
    iconsize = width;
    if( maxBarbs != 1 ) // avoid division by zero
      diff = ( height - iconsize ) / (float)( maxBarbs - 1 );
    else
      diff = 0; // doesn't matter, won't be used
  }
  else
  {
    vertical = qfalse;
    iconsize = height;
    if( maxBarbs != 1 )
      diff = ( width - iconsize ) / (float)( maxBarbs - 1 );
    else
      diff = 0;
  }

  trap_R_SetColor( color );

  for( ; numBarbs > 0; numBarbs-- )
  {
    trap_R_DrawStretchPic( x, y, iconsize, iconsize, 0, 0, 1, 1, shader );
    if( vertical )
      y += diff;
    else
      x += diff;
  }

  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawPlayerWallclimbing
==============
*/
static void CG_DrawPlayerWallclimbing( rectDef_t *rect, vec4_t backColor, vec4_t foreColor, qhandle_t shader )
{
  if( cg.snap->ps.stats[ STAT_STATE ] & SS_WALLCLIMBING )
    trap_R_SetColor( foreColor );
  else
    trap_R_SetColor( backColor );

  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

static void CG_DrawPlayerAmmoValue( rectDef_t *rect, vec4_t color )
{
  int value;
  int valueMarked = -1;
  qboolean bp = qfalse;

  switch( cg.snap->ps.stats[ STAT_WEAPON ] )
  {
    case WP_NONE:
    case WP_BLASTER:
      return;

    case WP_ABUILD:
    case WP_ABUILD2:
    case WP_HBUILD:
      value = cg.snap->ps.persistant[ PERS_BP ];
      valueMarked = cg.snap->ps.persistant[ PERS_MARKEDBP ];
      bp = qtrue;
      break;

    default:
      value = cg.snap->ps.ammo;
      break;
  }

  if( value > 999 )
    value = 999;
  if( valueMarked > 999 )
    valueMarked = 999;

  if( value > -1 )
  {
    float tx, ty;
    char *text;
    float scale;
    int len;

    trap_R_SetColor( color );
    if( !bp )
    {
      CG_DrawField( rect->x - 5, rect->y, 4, rect->w / 4, rect->h, value );
      trap_R_SetColor( NULL );
      return;
    }

    if( valueMarked > 0 )
      text = va( "%d+(%d)", value, valueMarked );
    else
      text = va( "%d", value );

    len = strlen( text );

    if( len <= 4 )
      scale = 0.50f;
    else if( len <= 6 )
      scale = 0.43f;
    else if( len == 7 ) 
      scale = 0.36f;
    else if( len == 8 )
      scale = 0.33f;
    else
      scale = 0.31f;

    CG_AlignText( rect, text, scale, 0.0f, 0.0f, ALIGN_RIGHT, VALIGN_CENTER, &tx, &ty );
    UI_Text_Paint( tx + 1, ty, scale, color, text, 0, 0, ITEM_TEXTSTYLE_NORMAL );
    trap_R_SetColor( NULL );
  }
}


/*
==============
CG_DrawAlienSense
==============
*/
static void CG_DrawAlienSense( rectDef_t *rect )
{
  if( BG_ClassHasAbility( cg.snap->ps.stats[ STAT_CLASS ], SCA_ALIENSENSE ) )
    CG_AlienSense( rect );
}

/*
===============
CG_HelmetVision
===============
*/
static void CG_HelmetVision( void )
{
  vec4_t    color = { 0.0f, 0.0f, 0.0f, 1.0f };

  if( BG_InventoryContainsUpgrade( UP_HELMET, cg.snap->ps.stats ) &&
      !cg.intermissionStarted )
  {
    if( !cg.predictedPlayerEntity.helmetVision )
    {
      cg.predictedPlayerEntity.helmetVisionTime = cg.time;
      cg.predictedPlayerEntity.helmetVision = qtrue;
    }
  }
  else
  {
    if( cg.predictedPlayerEntity.helmetVision )
    {
      cg.predictedPlayerEntity.helmetVisionTime = cg.time;
      cg.predictedPlayerEntity.helmetVision = qfalse;
    }
  }

  if( cg.renderingThirdPerson ||
      cg.snap->ps.persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT )
    return;

  if( cg.predictedPlayerEntity.helmetVision )
  {
    if( cg.time - cg.predictedPlayerEntity.helmetVisionTime >= 500 )
      color[ 3 ] = 1.0f;
    else
    {
      color[ 3 ] = ( (float)( cg.time - cg.predictedPlayerEntity.helmetVisionTime ) ) / 500.0f;
    }
  } else if( cg.time - cg.predictedPlayerEntity.helmetVisionTime < 500 )
  {
    color[ 3 ] = ( (float)( 500 - ( cg.time - cg.predictedPlayerEntity.helmetVisionTime ) ) ) / 500.0f;
    
  } else
    return;

  trap_R_SetColor( color );
  CG_DrawPic( 0, 0, 640, 480, cgs.media.helmetViewShader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawHumanScanner
==============
*/
static void CG_DrawHumanScanner( rectDef_t *rect, qhandle_t shader, vec4_t color )
{
  if( BG_InventoryContainsUpgrade( UP_HELMET, cg.snap->ps.stats ) )
    CG_Scanner( rect, shader, color );
}


/*
==============
CG_DrawUsableBuildable
==============
*/
static void CG_DrawUsableBuildable( rectDef_t *rect, qhandle_t shader, vec4_t color )
{
  entityState_t *es;

  es = &cg_entities[ cg.predictedPlayerState.persistant[ PERS_ACT_ENT ] ].currentState;

  if( cg.predictedPlayerState.persistant[ PERS_ACT_ENT ] != ENTITYNUM_NONE )
  {
    trap_R_SetColor( color );
    CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
    trap_R_SetColor( NULL );
    
    if( es->eType == ET_BUILDABLE )
    {
      cg.nearUsableBuildable = es->modelindex;
    }
    else
      cg.nearUsableBuildable = BA_NONE;
  } else
  {
    cg.nearUsableBuildable = BA_NONE;
  }
}


#define BUILD_DELAY_TIME  2000

static void CG_DrawPlayerBuildTimer( rectDef_t *rect, vec4_t color )
{
  int           index;
  playerState_t *ps;

  ps = &cg.snap->ps;

  if( ps->misc[ MISC_MISC ] <= 0 )
    return;

  switch( ps->stats[ STAT_WEAPON ] )
  {
    case WP_ABUILD:
    case WP_ABUILD2:
    case WP_HBUILD:
      break;

    default:
      return;
  }

  index = 8 * ( ps->misc[ MISC_MISC ] - 1 ) / MAXIMUM_BUILD_TIME;
  if( index > 7 )
    index = 7;
  else if( index < 0 )
    index = 0;

  if( cg.time - cg.lastBuildAttempt <= BUILD_DELAY_TIME &&
      ( ( cg.time - cg.lastBuildAttempt ) / 300 ) % 2 )
  {
    color[ 0 ] = 1.0f;
    color[ 1 ] = color[ 2 ] = 0.0f;
    color[ 3 ] = 1.0f;
  }

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h,
              cgs.media.buildWeaponTimerPie[ index ] );
  trap_R_SetColor( NULL );
}

static void CG_DrawPlayerClipsValue( rectDef_t *rect, vec4_t color )
{
  int           value;
  playerState_t *ps = &cg.snap->ps;

  switch( ps->stats[ STAT_WEAPON ] )
  {
    case WP_NONE:
    case WP_BLASTER:
    case WP_ABUILD:
    case WP_ABUILD2:
    case WP_HBUILD:
      return;

    default:
      value = ps->clips;

      if( value > -1 )
      {
        trap_R_SetColor( color );
        CG_DrawField( rect->x, rect->y, 4, rect->w / 4, rect->h, value );
        trap_R_SetColor( NULL );
      }
      break;
  }
}

static void CG_DrawPlayerHealthValue( rectDef_t *rect, vec4_t color )
{
  trap_R_SetColor( color );
  CG_DrawField( rect->x, rect->y, 4, rect->w / 4, rect->h,
                BG_SU2HP( cg.snap->ps.misc[ MISC_HEALTH ] ) );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawPlayerHealthCross
==============
*/
static void CG_DrawPlayerHealthCross( rectDef_t *rect, vec4_t ref_color )
{
  qhandle_t shader;
  vec4_t color;
  float ref_alpha;

  // Pick the current icon
  shader = cgs.media.healthCross;
  if( cg.snap->ps.stats[ STAT_STATE ] & SS_HEALING_3X )
    shader = cgs.media.healthCross3X;
  else if( cg.snap->ps.stats[ STAT_STATE ] & SS_HEALING_2X )
  {
    if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      shader = cgs.media.healthCross2X;
    else
      shader = cgs.media.healthCrossMedkit;
  }
  else if( cg.snap->ps.stats[ STAT_STATE ] & SS_POISONED )
    shader = cgs.media.healthCrossPoisoned;

  // Pick the alpha value
  Vector4Copy( ref_color, color );
  if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
      BG_SU2HP( cg.snap->ps.misc[ MISC_HEALTH ] ) < 10  )
  {
    color[ 0 ] = 1.0f;
    color[ 1 ] = color[ 2 ] = 0.0f;
  }
  ref_alpha = ref_color[ 3 ];
  if( cg.snap->ps.stats[ STAT_STATE ] & SS_HEALING_ACTIVE )
    ref_alpha = 1.0f;

  // Don't fade from nothing
  if( !cg.lastHealthCross )
    cg.lastHealthCross = shader;

  // Fade the icon during transition
  if( cg.lastHealthCross != shader )
  {
    cg.healthCrossFade += cg.frametime / 500.0f;
    if( cg.healthCrossFade > 1.0f )
    {
      cg.healthCrossFade = 0.0f;
      cg.lastHealthCross = shader;
    }
    else
    {
      // Fading between two icons
      color[ 3 ] = ref_alpha * cg.healthCrossFade;
      trap_R_SetColor( color );
      CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
      color[ 3 ] = ref_alpha * ( 1.0f - cg.healthCrossFade );
      trap_R_SetColor( color );
      CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cg.lastHealthCross );
      trap_R_SetColor( NULL );
      return;
    }
  }

  // Not fading, draw a single icon
  color[ 3 ] = ref_alpha;
  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawEquipmentHUD
==============
*/
static void CG_DrawEquipmentHUD( rectDef_t *rect, vec4_t color,
                                 qhandle_t shader )
{

  if( !BG_InventoryContainsUpgrade( UP_JETPACK,
                                    cg.predictedPlayerState.stats ) )
    return;

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawJetpackIcon
==============
*/
static void CG_DrawJetpackIcon( rectDef_t *rect, vec4_t color,
                                qhandle_t shader )
{
  if( !BG_InventoryContainsUpgrade( UP_JETPACK,
                                    cg.predictedPlayerState.stats ) )
    return;

  if( BG_UpgradeIsActive( UP_JETPACK, cg.predictedPlayerState.stats ) )
    color[3] = 1.0f;

  if( cg.predictedPlayerState.stats[ STAT_FUEL ] <= JETPACK_FUEL_LOW )
  {
    if( ( cg.predictedPlayerState.stats[ STAT_FUEL ] > 0 &&
          BG_UpgradeIsActive( UP_JETPACK, cg.predictedPlayerState.stats ) ) ||
        cg.predictedPlayerState.stats[ STAT_FUEL ] > JETPACK_FUEL_MIN_START )
    {
      cg.jetpackIconAlert += cg.frametime / 500.0f;
      if( color[0] + cg.jetpackIconAlert > 1.0f )
      {
        cg.jetpackIconAlert = 0.0f;
      } else
      {
        color[0] += cg.jetpackIconAlert;
        color[1] -= cg.jetpackIconAlert;
        if( color[1] < 0 )
          color[1] = 0;
        color[2] -= cg.jetpackIconAlert;
        if( color[2] < 0 )
          color[2] = 0;
      }
    } else
    {
      cg.jetpackIconAlert = 0.0f;
      color[0] = 1.0f;
      color[1] = 0.0f;
      color[2] = 0.0f;
      color[3] = 1.0f;
    }
  }

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawJetpackFuel
==============
*/
static void CG_DrawJetpackFuel( rectDef_t *rect, vec4_t color )
{
  float fuel;
  float fuelRounded;

  if( !BG_InventoryContainsUpgrade( UP_JETPACK,
                                    cg.predictedPlayerState.stats ) )
    return;

  if( BG_UpgradeIsActive( UP_JETPACK, cg.predictedPlayerState.stats ) )
    color[3] = 1.0f;

  if( cg.predictedPlayerState.stats[ STAT_FUEL ] <= JETPACK_FUEL_LOW )
  {
    if( ( cg.predictedPlayerState.stats[ STAT_FUEL ] > 0 &&
          BG_UpgradeIsActive( UP_JETPACK, cg.predictedPlayerState.stats ) ) ||
        cg.predictedPlayerState.stats[ STAT_FUEL ] > JETPACK_FUEL_MIN_START )
    {
      color[0] += cg.jetpackIconAlert;
      if( color[0] > 1.0f )
        color[0] = 1.0f;
      color[1] -= cg.jetpackIconAlert;
      if( color[1] < 0 )
        color[1] = 0;
      color[2] -= cg.jetpackIconAlert;
      if( color[2] < 0 )
        color[2] = 0;
    } else
    {
      color[0] = 1.0f;
      color[1] = 0.0f;
      color[2] = 0.0f;
      color[3] = 1.0f;
    }
  }

  fuel = 100.0f * (float)cg.predictedPlayerState.stats[ STAT_FUEL ] / JETPACK_FUEL_FULL;
  fuelRounded = ceil( fuel );
  if( fuelRounded != 100 || !( fuel - fuelRounded ) )
    fuel = fuelRounded;

  trap_R_SetColor( color );
  CG_DrawField( rect->x, rect->y, 4, rect->w / 4, rect->h, (int)fuel );
  trap_R_SetColor( NULL );
}

static float CG_ChargeProgress( qboolean chargeStamina )
{
  float progress, rawProgress;
  int min = 0, max = 0;

  if( chargeStamina )
  {
    rawProgress = (float)cg.predictedPlayerState.stats[ STAT_STAMINA ];
    min = 0;
    max = BG_Class( cg.snap->ps.stats[STAT_CLASS] )->chargeStaminaMax;
  } else
  {
    rawProgress = (float)cg.predictedPlayerState.misc[ MISC_MISC ];
    if( cg.snap->ps.weapon == WP_ALEVEL3 )
    {
      min = LEVEL3_POUNCE_TIME_MIN;
      max = LEVEL3_POUNCE_TIME;
    }
    else if( cg.snap->ps.weapon == WP_ALEVEL3_UPG )
    {
      min = LEVEL3_POUNCE_TIME_MIN;
      max = LEVEL3_POUNCE_TIME_UPG;
    }
    else if( cg.snap->ps.weapon == WP_ALEVEL4 )
    {
      if( cg.predictedPlayerState.stats[ STAT_STATE ] & SS_CHARGING )
      {
        min = 0;
        max = LEVEL4_TRAMPLE_DURATION;
      }
      else
      {
        min = LEVEL4_TRAMPLE_CHARGE_MIN;
        max = LEVEL4_TRAMPLE_CHARGE_MAX;
      }
    }
    else if( cg.snap->ps.weapon == WP_LUCIFER_CANNON )
    {
      min = LCANNON_CHARGE_TIME_MIN;
      max = LCANNON_CHARGE_TIME_MAX;
    }
    else if( cg.snap->ps.weapon == WP_LIGHTNING )
    {
      min = LIGHTNING_BOLT_CHARGE_TIME_MIN;
      max = LIGHTNING_BOLT_CHARGE_TIME_MAX;
    }
  }

  if( max - min <= 0.0f )
    return 0.0f;

  progress = ( rawProgress - min ) / ( max - min );

  if( progress > 1.0f )
    return 1.0f;

  if( progress < 0.0f )
    return 0.0f;

  return progress;
}

#define CHARGE_BAR_FADE_RATE 0.002f

static void CG_DrawPlayerChargeBarBG( rectDef_t *rect, vec4_t ref_color,
                                      qhandle_t shader, qboolean chargeStamina )
{
  vec4_t color;
  float *meterAlpha;

  if( chargeStamina )
  {
    if( !BG_ClassHasAbility( cg.snap->ps.stats[STAT_CLASS],
                             SCA_CHARGE_STAMINA ) )
      return;

    meterAlpha = &cg.chargeStaminaMeterAlpha;
  }
  else
    meterAlpha = &cg.chargeMeterAlpha;

  if( !cg_drawChargeBar.integer || *meterAlpha <= 0.0f )
    return;

  color[ 0 ] = ref_color[ 0 ];
  color[ 1 ] = ref_color[ 1 ];
  color[ 2 ] = ref_color[ 2 ];
  color[ 3 ] = ref_color[ 3 ] * *meterAlpha;

  // Draw meter background
  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

// FIXME: This should come from the element info
#define CHARGE_BAR_CAP_SIZE 3

static void CG_DrawPlayerChargeBar( rectDef_t *rect, vec4_t ref_color,
                                    qhandle_t shader, qboolean chargeStamina )
{
  vec4_t color;
  float x, y, width, height, cap_size, progress;
  float *meterAlpha;
  float *meterValue;
  qboolean fadeMeter = qfalse;

  if( !cg_drawChargeBar.integer )
    return;

  // Get progress proportion and pump fade
  progress = CG_ChargeProgress( chargeStamina );
  if( chargeStamina )
  {
    if( !BG_ClassHasAbility( cg.snap->ps.stats[STAT_CLASS],
                             SCA_CHARGE_STAMINA ) )
      return;

    meterAlpha = &cg.chargeStaminaMeterAlpha;
    meterValue = &cg.chargeStaminaMeterValue;
    if( progress >= 1.0f )
      fadeMeter = qtrue;
  } else
  {
    meterAlpha = &cg.chargeMeterAlpha;
    meterValue = &cg.chargeMeterValue;
    if( progress <= 0.0f )
      fadeMeter = qtrue;
  }
  if( fadeMeter )
  {
    *meterAlpha -= CHARGE_BAR_FADE_RATE * cg.frametime;
    if( *meterAlpha <= 0.0f )
    {
      *meterAlpha = 0.0f;
      return;
    }
  }
  else
  {
    *meterValue = progress;
    *meterAlpha += CHARGE_BAR_FADE_RATE * cg.frametime;
    if( *meterAlpha > 1.0f )
      *meterAlpha = 1.0f;
  }

  color[ 0 ] = ref_color[ 0 ];
  color[ 1 ] = ref_color[ 1 ];
  color[ 2 ] = ref_color[ 2 ];
  color[ 3 ] = ref_color[ 3 ] * *meterAlpha;

  // Flash red for Lucifer Cannon warning
  if( cg.snap->ps.weapon == WP_LUCIFER_CANNON &&
      cg.snap->ps.misc[ MISC_MISC ] >= LCANNON_CHARGE_TIME_WARN &&
      ( cg.time & 128 ) )
  {
    color[ 0 ] = 1.0f;
    color[ 1 ] = 0.0f;
    color[ 2 ] = 0.0f;
  }

  x = rect->x;
  y = rect->y;

  // Horizontal charge bar
  if( rect->w >= rect->h )
  {
    width = ( rect->w - CHARGE_BAR_CAP_SIZE * 2 ) * *meterValue;
    height = rect->h;
    CG_AdjustFrom640( &x, &y, &width, &height );
    cap_size = CHARGE_BAR_CAP_SIZE * cgs.screenXScale;

    // Draw the meter
    trap_R_SetColor( color );
    trap_R_DrawStretchPic( x, y, cap_size, height, 0, 0, 1, 1, shader );
    trap_R_DrawStretchPic( x + width + cap_size, y, cap_size, height,
                           1, 0, 0, 1, shader );
    trap_R_DrawStretchPic( x + cap_size, y, width, height, 1, 0, 1, 1, shader );
    trap_R_SetColor( NULL );
  }

  // Vertical charge bar
  else
  {
    y += rect->h;
    width = rect->w;
    height = ( rect->h - CHARGE_BAR_CAP_SIZE * 2 ) * *meterValue;
    CG_AdjustFrom640( &x, &y, &width, &height );
    cap_size = CHARGE_BAR_CAP_SIZE * cgs.screenYScale;

    // Draw the meter
    trap_R_SetColor( color );
    trap_R_DrawStretchPic( x, y - cap_size, width, cap_size,
                           0, 1, 1, 0, shader );
    trap_R_DrawStretchPic( x, y - height - cap_size * 2, width,
                           cap_size, 0, 0, 1, 1, shader );
    trap_R_DrawStretchPic( x, y - height - cap_size, width, height,
                           0, 1, 1, 1, shader );
    trap_R_SetColor( NULL );
  }
}

static void CG_DrawProgressLabel( rectDef_t *rect, float text_x, float text_y, vec4_t color,
                                  float scale, int textalign, int textvalign,
                                  const char *s, float fraction )
{
  vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
  float tx, ty;

  CG_AlignText( rect, s, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );

  if( fraction < 1.0f )
    UI_Text_Paint( text_x + tx, text_y + ty, scale, white,
      s, 0, 0, ITEM_TEXTSTYLE_NORMAL );
  else
    UI_Text_Paint( text_x + tx, text_y + ty, scale, color,
      s, 0, 0, ITEM_TEXTSTYLE_NEON );
}

static void CG_DrawMediaProgress( rectDef_t *rect, vec4_t color, float scale,
                                  int align, int textalign, int textStyle,
                                  float borderSize )
{
  CG_DrawProgressBar( rect, color, scale, align, textalign, textStyle,
                      borderSize, cg.mediaFraction );
}

static void CG_DrawMediaProgressLabel( rectDef_t *rect, float text_x, float text_y,
                                       vec4_t color, float scale, int textalign, int textvalign )
{
  CG_DrawProgressLabel( rect, text_x, text_y, color, scale, textalign, textvalign,
                        "Map and Textures", cg.mediaFraction );
}

static void CG_DrawBuildablesProgress( rectDef_t *rect, vec4_t color,
                                       float scale, int align, int textalign,
                                       int textStyle, float borderSize )
{
  CG_DrawProgressBar( rect, color, scale, align, textalign, textStyle,
                      borderSize, cg.buildablesFraction );
}

static void CG_DrawBuildablesProgressLabel( rectDef_t *rect, float text_x, float text_y,
                                            vec4_t color, float scale, int textalign, int textvalign )
{
  CG_DrawProgressLabel( rect, text_x, text_y, color, scale, textalign, textvalign,
                        "Buildable Models", cg.buildablesFraction );
}

static void CG_DrawCharModelProgress( rectDef_t *rect, vec4_t color,
                                      float scale, int align, int textalign,
                                      int textStyle, float borderSize )
{
  CG_DrawProgressBar( rect, color, scale, align, textalign, textStyle,
                      borderSize, cg.charModelFraction );
}

static void CG_DrawCharModelProgressLabel( rectDef_t *rect, float text_x, float text_y,
                                           vec4_t color, float scale, int textalign, int textvalign )
{
  CG_DrawProgressLabel( rect, text_x, text_y, color, scale, textalign, textvalign,
                        "Character Models", cg.charModelFraction );
}

static void CG_DrawOverallProgress( rectDef_t *rect, vec4_t color, float scale,
                                    int align, int textalign, int textStyle,
                                    float borderSize )
{
  float total;

  total = cg.charModelFraction + cg.buildablesFraction + cg.mediaFraction;
  total /= 3.0f;

  CG_DrawProgressBar( rect, color, scale, align, textalign, textStyle,
                      borderSize, total );
}

static void CG_DrawLevelShot( rectDef_t *rect )
{
  const char  *s;
  const char  *info;
  qhandle_t   levelshot;
  qhandle_t   detail;

  info = CG_ConfigString( CS_SERVERINFO );
  s = Info_ValueForKey( info, "mapname" );
  levelshot = trap_R_RegisterShaderNoMip( va( "levelshots/%s.tga", s ) );

  if( !levelshot )
    levelshot = trap_R_RegisterShaderNoMip( "gfx/2d/load_screen" );

  trap_R_SetColor( NULL );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, levelshot );

  // blend a detail texture over it
  detail = trap_R_RegisterShader( "gfx/misc/detail" );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, detail );
}

static void CG_DrawLevelName( rectDef_t *rect, float text_x, float text_y,
                              vec4_t color, float scale,
                              int textalign, int textvalign, int textStyle )
{
  const char  *s;

  s = CG_ConfigString( CS_MESSAGE );

  UI_DrawTextBlock( rect, text_x, text_y, color, scale, textalign, textvalign, textStyle, s );
}

static void CG_DrawMOTD( rectDef_t *rect, float text_x, float text_y,
                         vec4_t color, float scale,
                         int textalign, int textvalign, int textStyle )
{
  const char  *s;
  char parsed[ MAX_STRING_CHARS ];

  s = CG_ConfigString( CS_MOTD );

  Q_ParseNewlines( parsed, s, sizeof( parsed ) );

  UI_DrawTextBlock( rect, text_x, text_y, color, scale, textalign, textvalign, textStyle, parsed );
}

static void CG_DrawHostname( rectDef_t *rect, float text_x, float text_y,
                             vec4_t color, float scale,
                             int textalign, int textvalign, int textStyle )
{
  char buffer[ 1024 ];
  const char  *info;

  info = CG_ConfigString( CS_SERVERINFO );

  UI_EscapeEmoticons( buffer, Info_ValueForKey( info, "sv_hostname" ), sizeof( buffer ) );
  Q_CleanStr( buffer );

  UI_DrawTextBlock( rect, text_x, text_y, color, scale, textalign, textvalign, textStyle, buffer );
}

/*
==============
CG_DrawDemoPlayback
==============
*/
static void CG_DrawDemoPlayback( rectDef_t *rect, vec4_t color, qhandle_t shader )
{
  if( !cg_drawDemoState.integer )
    return;

  if( trap_GetDemoState( ) != DS_PLAYBACK )
    return;

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
==============
CG_DrawDemoRecording
==============
*/
static void CG_DrawDemoRecording( rectDef_t *rect, vec4_t color, qhandle_t shader )
{
  if( !cg_drawDemoState.integer )
    return;

  if( trap_GetDemoState( ) != DS_RECORDING )
    return;

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
  trap_R_SetColor( NULL );
}

/*
======================
CG_UpdateMediaFraction

======================
*/
void CG_UpdateMediaFraction( float newFract )
{
  cg.mediaFraction = newFract;

  trap_UpdateScreen( );
}

/*
====================
CG_DrawLoadingScreen

Draw all the status / pacifier stuff during level loading
====================
*/
void CG_DrawLoadingScreen( void )
{
  menuDef_t *menu = Menus_FindByName( "Loading" );

  Menu_Update( menu );
  Menu_Paint( menu, qtrue );
}

float CG_GetValue( int ownerDraw )
{
  playerState_t *ps;
  weapon_t weapon;

  ps = &cg.snap->ps;
  weapon = BG_GetPlayerWeapon( ps );

  switch( ownerDraw )
  {
    case CG_PLAYER_AMMO_VALUE:
      if( weapon )
        return ps->ammo;
      break;
    case CG_PLAYER_CLIPS_VALUE:
      if( weapon )
        return ps->clips;
      break;
    case CG_PLAYER_HEALTH:
      return ( BG_SU2HP( ps->misc[ MISC_HEALTH ] ) );
      break;
    default:
      break;
  }

  return -1;
}

const char *CG_GetKillerText( )
{
  const char *s = "";
  if( cg.killerName[ 0 ] )
    s = va( "Fragged by %s", cg.killerName );

  return s;
}


static void CG_DrawKiller( rectDef_t *rect, float scale, vec4_t color,
                           qhandle_t shader, int textStyle )
{
  // fragged by ... line
 if( cg.killerName[ 0 ] )
 {
   int x = rect->x + rect->w / 2;
   UI_Text_Paint( x - UI_Text_Width( CG_GetKillerText( ), scale ) / 2,
     rect->y + rect->h, scale, color, CG_GetKillerText( ), 0, 0, textStyle );
  }
}


#define SPECTATORS_PIXELS_PER_SECOND 30.0f

/*
==================
CG_DrawTeamSpectators
==================
*/
static void CG_DrawTeamSpectators( rectDef_t *rect, float scale, int textvalign, vec4_t color, qhandle_t shader )
{
  float y;
  char  *text = cg.spectatorList;
  float textWidth = UI_Text_Width( text, scale );

  CG_AlignText( rect, text, scale, 0.0f, 0.0f, ALIGN_LEFT, textvalign, NULL, &y );

  if( textWidth > rect->w )
  {
    // The text is too wide to fit, so scroll it
    int now = trap_Milliseconds( );
    int delta = now - cg.spectatorTime;

    CG_SetClipRegion( rect->x, rect->y, rect->w, rect->h );

    UI_Text_Paint( rect->x - cg.spectatorOffset, y, scale, color, text, 0, 0, 0 );
    UI_Text_Paint( rect->x + textWidth - cg.spectatorOffset, y, scale, color, text, 0, 0, 0 );

    CG_ClearClipRegion( );

    cg.spectatorOffset += ( delta / 1000.0f ) * SPECTATORS_PIXELS_PER_SECOND;

    while( cg.spectatorOffset > textWidth )
      cg.spectatorOffset -= textWidth;

    cg.spectatorTime = now;
  }
  else
  {
    UI_Text_Paint( rect->x, y, scale, color, text, 0, 0, 0 );
  }
}

#define FOLLOWING_STRING "following "
#define CHASING_STRING "chasing "

/*
==================
CG_DrawFollow
==================
*/
static void CG_DrawFollow( rectDef_t *rect, float text_x, float text_y,
    vec4_t color, float scale, int textalign, int textvalign, int textStyle )
{
  float tx, ty;

  if( cg.snap && cg.snap->ps.pm_flags & PMF_FOLLOW )
  {
    char buffer[ MAX_STRING_CHARS ];

    if( !cg.chaseFollow )
      strcpy( buffer, FOLLOWING_STRING );
    else
      strcpy( buffer, CHASING_STRING );

    strcat( buffer, cgs.clientinfo[ cg.snap->ps.clientNum ].name );

    CG_AlignText( rect, buffer, scale, 0, 0, textalign, textvalign, &tx, &ty );
    UI_Text_Paint( text_x + tx, text_y + ty, scale, color, buffer, 0, 0,
                   textStyle );
  }
}

/*
==================
CG_DrawScrimSettings
==================
*/
static void CG_DrawScrimSettings( rectDef_t *rect, float text_x, float text_y,
    vec4_t color, float scale, int textalign, int textvalign, int textStyle )
{
  char  *settings;
  char  *sd_mode;
  float tx, ty;

  switch(cgs.scrim.sudden_death_mode) {
    case SDMODE_BP:
      sd_mode = "Build Point";
      break;

    case SDMODE_NO_BUILD:
      sd_mode = "No Build";
      break;

    case SDMODE_SELECTIVE:
      sd_mode = "Selective";
      break;

    case SDMODE_NO_DECON:
      sd_mode = "No Deconstruct";
      break;
  }

  if(cgs.scrim.time_limit) {
    if(cgs.scrim.sudden_death_time) {
      settings = 
        va(
          "Sudden Death Mode: %s | Sudden Death Time: %i | Time Limit: %i",
          sd_mode, cgs.scrim.sudden_death_time, cgs.scrim.time_limit);
    } else {
      settings = 
        va(
          "Sudden Death: disabled | Time Limit: %i", cgs.scrim.time_limit);
    }
  } else {
    if(cgs.scrim.sudden_death_time) {
      settings = 
        va(
          "Sudden Death Mode: %s | Sudden Death Time: %i | Time Limit: unlimited",
          sd_mode, cgs.scrim.sudden_death_time);
    } else {
      settings = "Sudden Death: disabled | Time Limit: unlimited";
    }
  }

  CG_AlignText( rect, settings, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );
  UI_Text_Paint( text_x + tx, text_y + ty, scale, color, settings, 0, 0, textStyle );
}

/*
==================
CG_DrawScrimWinCondition
==================
*/
static void CG_DrawScrimWinCondition( rectDef_t *rect, float text_x, float text_y,
    vec4_t color, float scale, int textalign, int textvalign, int textStyle )
{
  char  *win_condition;
  float tx, ty;

  switch(cgs.scrim.win_condition) {
    case SCRIM_WIN_CONDITION_DEFAULT:
      win_condition = "Default; first team to win 2 rounds in a row, round draws are ignored";
      break;

    case SCRIM_WIN_CONDITION_SHORT:
      win_condition = "Short; out of 2 rounds, the team that wins at least 1 round and loses no round";
  }

  win_condition = va("Scrim Victory Condition: %s", win_condition);

  CG_AlignText( rect, win_condition, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );
  UI_Text_Paint( text_x + tx, text_y + ty, scale, color, win_condition, 0, 0, textStyle );
}

/*
==================
CG_DrawScrimStatus
==================
*/
static void CG_DrawScrimStatus( rectDef_t *rect, float text_x, float text_y,
    vec4_t color, float scale, int textalign, int textvalign, int textStyle )
{
  char  *status;
  char  *scrim_mode;
  float tx, ty;

  switch(cgs.scrim.mode) {
    case SCRIM_MODE_OFF:
      scrim_mode = "Off";
      break;

    case SCRIM_MODE_SETUP:
      scrim_mode = "Set up";
      break;

    case SCRIM_MODE_STARTED:
      scrim_mode = "Started";
      break;

    case SCRIM_MODE_TIMEOUT:
      scrim_mode = "Time out";
      break;
  }

  if(cgs.scrim.previous_round_win == SCRIM_TEAM_NONE) {
    status = va(
      "Scrim Mode: %s | Round: %i/%i",
      scrim_mode,
      cgs.scrim. mode == SCRIM_MODE_SETUP ? 0 :
        (
          cg.intermissionStarted ?
            cgs.scrim.rounds_completed : cgs.scrim.rounds_completed + 1),
      cgs.scrim.max_rounds);
  } else {
    status = va(
      "Scrim Mode: %s | Round: %i/%i | Previous Round Win was by: %s^7",
      scrim_mode,
      cg.intermissionStarted ? cgs.scrim.rounds_completed : cgs.scrim.rounds_completed + 1,
      cgs.scrim.max_rounds,
      cgs.scrim.team[cgs.scrim.previous_round_win].name);
  }

  CG_AlignText( rect, status, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );
  UI_Text_Paint( text_x + tx, text_y + ty, scale, color, status, 0, 0, textStyle );
}

/*
==================
CG_DrawScrimTeamStatus
==================
*/
static void CG_DrawScrimTeamStatus( rectDef_t *rect, team_t team, float text_x, float text_y,
    vec4_t color, float scale, int textalign, int textvalign, int textStyle )
{
  char  stats[ MAX_TOKEN_CHARS ];
  float tx, ty;
  scrim_team_t scrim_team;

  stats[ 0 ] = '\0';

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(cgs.scrim.team[scrim_team].current_team != team) {
      continue;
    }

    Com_sprintf(
      stats, MAX_TOKEN_CHARS,
      "Rounds Won: %i | Rounds Lost: %i | Rounds Drawn: %i",
      cgs.scrim.team[scrim_team].wins,
      cgs.scrim.team[scrim_team].losses,
      cgs.scrim.team[scrim_team].draws);
  }

  CG_AlignText( rect, stats, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );
  UI_Text_Paint( text_x + tx, text_y + ty, scale, color, stats, 0, 0, textStyle );
}

/*
==================
CG_DrawScrimTeamLabel
==================
*/
static void CG_DrawScrimTeamLabel( rectDef_t *rect, team_t team, float text_x, float text_y,
    vec4_t color, float scale, int textalign, int textvalign, int textStyle )
{
  char  *name;
  float tx, ty;
  scrim_team_t scrim_team;

  for(scrim_team = 0; scrim_team < NUM_SCRIM_TEAMS; scrim_team++) {
    if(cgs.scrim.team[scrim_team].current_team != team) {
      continue;
    }

    name = cgs.scrim.team[scrim_team].name;
  }

  CG_AlignText( rect, name, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );
  UI_Text_Paint( text_x + tx, text_y + ty, scale, color, name, 0, 0, textStyle );
}

/*
==================
CG_DrawTeamLabel
==================
*/
static void CG_DrawTeamLabel( rectDef_t *rect, team_t team, float text_x, float text_y,
    vec4_t color, float scale, int textalign, int textvalign, int textStyle )
{
  char  *t;
  char  stage[ MAX_TOKEN_CHARS ];
  char  *s;
  float tx, ty;

  stage[ 0 ] = '\0';

  switch( team )
  {
    case TEAM_ALIENS:
      t = "Aliens";
      if( cg.intermissionStarted )
        Com_sprintf( stage, MAX_TOKEN_CHARS, "(Stage %d)", cgs.alienStage + 1 );
      break;

    case TEAM_HUMANS:
      t = "Humans";
      if( cg.intermissionStarted )
        Com_sprintf( stage, MAX_TOKEN_CHARS, "(Stage %d)", cgs.humanStage + 1 );
      break;

    default:
      t = "";
      break;
  }

  switch( textalign )
  {
    default:
    case ALIGN_LEFT:
      s = va( "%s %s", t, stage );
      break;

    case ALIGN_RIGHT:
      s = va( "%s %s", stage, t );
      break;
  }

  CG_AlignText( rect, s, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );
  UI_Text_Paint( text_x + tx, text_y + ty, scale, color, s, 0, 0, textStyle );
}

/*
==================
CG_DrawStageReport
==================
*/
static const char *CG_SpawnReport( qboolean eggs )
{
  const char *s;
  
  s = ( cg.snap->ps.persistant[ PERS_SPAWNS ] == 1 ? "" : "s" );

  return va( "%d %s%s left",
             cg.snap->ps.persistant[ PERS_SPAWNS ],
             ( eggs ? "egg" : "telenode" ),
             s );
}
static void CG_DrawStageReport( rectDef_t *rect, float text_x, float text_y,
    vec4_t color, float scale, int textalign, int textvalign, int textStyle )
{
  char  s[ MAX_TOKEN_CHARS ];
  float tx, ty;

  if( cg.intermissionStarted )
    return;

  if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_NONE )
    return;

  if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
  {
    int kills = ceil( (float)(cgs.alienNextStageThreshold - cgs.alienCredits) / ALIEN_CREDITS_PER_KILL );
    if( kills < 0 )
      kills = 0;

    if( cgs.alienNextStageThreshold < 0 )
      Com_sprintf( s, MAX_TOKEN_CHARS, "Stage %d, %s", cgs.alienStage + 1,
                   CG_SpawnReport( qtrue ) );
    else if( kills == 1 )
      Com_sprintf( s, MAX_TOKEN_CHARS, "Stage %d, 1 frag for next stage, %s",
                   cgs.alienStage + 1, CG_SpawnReport( qtrue ) );
    else
      Com_sprintf( s, MAX_TOKEN_CHARS, "Stage %d, %d frags for next stage, %s",
                   cgs.alienStage + 1, kills, CG_SpawnReport( qtrue ) );
  }
  else if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
  {
    int credits = cgs.humanNextStageThreshold - cgs.humanCredits;

    if( credits < 0 )
      credits = 0;

    if( cgs.humanNextStageThreshold < 0 )
      Com_sprintf( s, MAX_TOKEN_CHARS, "Stage %d, %s", cgs.humanStage + 1,
                   CG_SpawnReport( qfalse ) );
    else if( credits == 1 )
      Com_sprintf( s, MAX_TOKEN_CHARS, "Stage %d, 1 credit for next stage, %s",
                   cgs.humanStage + 1, CG_SpawnReport( qfalse ) );
    else
      Com_sprintf( s, MAX_TOKEN_CHARS, "Stage %d, %d credits for next stage, %s",
          cgs.humanStage + 1, credits, CG_SpawnReport( qfalse ) );
  }

  CG_AlignText( rect, s, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );

  UI_Text_Paint( text_x + tx, text_y + ty, scale, color, s, 0, 0, textStyle );
}

/*
==================
CG_DrawFPS
==================
*/
#define FPS_FRAMES  20
#define FPS_STRING  "fps"
static void CG_DrawFPS( rectDef_t *rect, float text_x, float text_y,
                        float scale, vec4_t color,
                        int textalign, int textvalign, int textStyle,
                        qboolean scalableText )
{
  char        *s;
  float       tx, ty;
  float       w, h, totalWidth;
  int         strLength;
  static int  previousTimes[ FPS_FRAMES ];
  static int  index;
  int         i, total;
  int         fps;
  static int  previous;
  int         t, frameTime;

  if( !cg_drawFPS.integer )
    return;

  // don't use serverTime, because that will be drifting to
  // correct for internet lag changes, timescales, timedemos, etc
  t = trap_Milliseconds( );
  frameTime = t - previous;
  previous = t;

  previousTimes[ index % FPS_FRAMES ] = frameTime;
  index++;

  if( index > FPS_FRAMES )
  {
    // average multiple frames together to smooth changes out a bit
    total = 0;

    for( i = 0 ; i < FPS_FRAMES ; i++ )
      total += previousTimes[ i ];

    if( !total )
      total = 1;

    fps = 1000 * FPS_FRAMES / total;

    s = va( "%d", fps );
    w = UI_Text_Width( "0", scale );
    h = UI_Text_Height( "0", scale );
    strLength = CG_DrawStrlen( s );
    totalWidth = UI_Text_Width( FPS_STRING, scale ) + w * strLength;

    CG_AlignText( rect, s, 0.0f, totalWidth, h, textalign, textvalign, &tx, &ty );

    if( scalableText )
    {
      for( i = 0; i < strLength; i++ )
      {
        char c[ 2 ];

        c[ 0 ] = s[ i ];
        c[ 1 ] = '\0';

        UI_Text_Paint( text_x + tx + i * w, text_y + ty, scale, color, c, 0, 0, textStyle );
      }

      UI_Text_Paint( text_x + tx + i * w, text_y + ty, scale, color, FPS_STRING, 0, 0, textStyle );
    }
    else
    {
      trap_R_SetColor( color );
      CG_DrawField( rect->x, rect->y, 3, rect->w / 3, rect->h, fps );
      trap_R_SetColor( NULL );
    }
  }
}


/*
=================
CG_DrawTimerMins
=================
*/
static void CG_DrawTimerMins( rectDef_t *rect, vec4_t color )
{
  int     mins, seconds;
  int     msec;

  if( !cg_drawTimer.integer )
    return;

  msec = cg.time - cgs.levelStartTime;

  seconds = msec / 1000;
  mins = seconds / 60;
  seconds -= mins * 60;

  trap_R_SetColor( color );
  CG_DrawField( rect->x, rect->y, 3, rect->w / 3, rect->h, mins );
  trap_R_SetColor( NULL );
}


/*
=================
CG_DrawTimerSecs
=================
*/
static void CG_DrawTimerSecs( rectDef_t *rect, vec4_t color )
{
  int     mins, seconds;
  int     msec;

  if( !cg_drawTimer.integer )
    return;

  msec = cg.time - cgs.levelStartTime;

  seconds = msec / 1000;
  mins = seconds / 60;
  seconds -= mins * 60;

  trap_R_SetColor( color );
  CG_DrawFieldPadded( rect->x, rect->y, 2, rect->w / 2, rect->h, seconds );
  trap_R_SetColor( NULL );
}


/*
=================
CG_DrawTimer
=================
*/
static void CG_DrawTimer( rectDef_t *rect, float text_x, float text_y,
                          float scale, vec4_t color,
                          int textalign, int textvalign, int textStyle )
{
  char    *s;
  float   tx, ty;
  int     i, strLength;
  float   w, h, totalWidth;
  int     mins, seconds, tens;
  int     msec;

  if( !cg_drawTimer.integer )
    return;

  msec = cg.time - cgs.levelStartTime;

  seconds = msec / 1000;
  mins = seconds / 60;
  seconds -= mins * 60;
  tens = seconds / 10;
  seconds -= tens * 10;

  s = va( "%d:%d%d", mins, tens, seconds );
  w = UI_Text_Width( "0", scale );
  h = UI_Text_Height( "0", scale );
  strLength = CG_DrawStrlen( s );
  totalWidth = w * strLength;

  CG_AlignText( rect, s, 0.0f, totalWidth, h, textalign, textvalign, &tx, &ty );

  for( i = 0; i < strLength; i++ )
  {
    char c[ 2 ];

    c[ 0 ] = s[ i ];
    c[ 1 ] = '\0';

    UI_Text_Paint( text_x + tx + i * w, text_y + ty, scale, color, c, 0, 0, textStyle );
  }
}

/*
=================
CG_DrawTeamOverlay
=================
*/

typedef enum
{
  TEAMOVERLAY_OFF,
  TEAMOVERLAY_ALL,
  TEAMOVERLAY_SUPPORT,
  TEAMOVERLAY_NEARBY,
} teamOverlayMode_t;

typedef enum
{
  TEAMOVERLAY_SORT_NONE,
  TEAMOVERLAY_SORT_SCORE,
  TEAMOVERLAY_SORT_WEAPONCLASS,
} teamOverlaySort_t;

static int QDECL SortScore( const void *a, const void *b )
{
  int na = *(int *)a;
  int nb = *(int *)b;

  if(cgs.clientinfo[ nb ].rank != cgs.clientinfo[ na ].rank) {
    return( cgs.clientinfo[ nb ].rank - cgs.clientinfo[ na ].rank );
  }

  return( cgs.clientinfo[ nb ].score - cgs.clientinfo[ na ].score );
}

static int QDECL SortWeaponClass( const void *a, const void *b )
{
  int out;
  clientInfo_t *ca = cgs.clientinfo + *(int *)a;
  clientInfo_t *cb = cgs.clientinfo + *(int *)b;

  if(cb->rank != ca->rank) {
    return( cb->rank - ca->rank );
  }

  out = cb->curWeaponClass - ca->curWeaponClass;

  // We want grangers on top. ckits are already on top without the special case.
  if( ca->team == TEAM_ALIENS )
  {
    if( ca->curWeaponClass == PCL_ALIEN_BUILDER0_UPG || 
        cb->curWeaponClass == PCL_ALIEN_BUILDER0_UPG ||
        ca->curWeaponClass == PCL_ALIEN_BUILDER0 || 
        cb->curWeaponClass == PCL_ALIEN_BUILDER0 )
    {
      out = -out;
    }
  }

  return( out );
}

static void CG_DrawTeamOverlay( rectDef_t *rect, float scale, vec4_t color )
{
  char              *s;
  int               i;
  float             x = rect->x;
  float             y;
  clientInfo_t      *ci, *pci;
  vec4_t            tcolor;
  float             iconSize = rect->h / 8.0f;
  float             leftMargin = 4.0f;
  float             iconTopMargin = 2.0f;
  float             midSep = 2.0f;
  float             backgroundWidth = rect->w;
  float             fontScale = 0.30f;
  float             vPad = 0.0f;
  float             nameWidth = 0.5f * rect->w;
  char              name[ MAX_NAME_LENGTH + 2 ];
  int               maxDisplayCount = 0;
  int               displayCount = 0;
  float             nameMaxX, nameMaxXCp;
  float             maxX = rect->x + rect->w;
  float             maxXCp = maxX;
  weapon_t          curWeapon = WP_NONE;
  teamOverlayMode_t mode = cg_drawTeamOverlay.integer;
  teamOverlaySort_t sort = cg_teamOverlaySortMode.integer;
  int               displayClients[ MAX_CLIENTS ];

  if( cg.predictedPlayerState.pm_type == PM_SPECTATOR )
    return;

  if( mode == TEAMOVERLAY_OFF || !cg_teamOverlayMaxPlayers.integer )
    return;

  if( !cgs.teaminfoReceievedTime )
    return;

  if( cg.showScores ||
      cg.predictedPlayerState.pm_type == PM_INTERMISSION )
    return;

  pci = cgs.clientinfo + cg.snap->ps.clientNum;

  if( mode == TEAMOVERLAY_ALL || mode == TEAMOVERLAY_SUPPORT )
  {
    for( i = 0; i < MAX_CLIENTS; i++ )
    {
      ci = cgs.clientinfo + i;
      if( ci->infoValid && pci != ci && ci->team == pci->team )
      {
        if( mode == TEAMOVERLAY_ALL )
          displayClients[ maxDisplayCount++ ] = i;
        else
        {
          if( ci->curWeaponClass == PCL_ALIEN_BUILDER0 || 
              ci->curWeaponClass == PCL_ALIEN_BUILDER0_UPG ||
              ci->curWeaponClass == PCL_ALIEN_LEVEL1 || 
              ci->curWeaponClass == PCL_ALIEN_LEVEL1_UPG ||
              ci->curWeaponClass == WP_HBUILD )
          {
            displayClients[ maxDisplayCount++ ] = i;
          }
        }
      }
    }
  }
  else // find nearby
  {
    for( i = 0; i < cg.snap->numEntities; i++ )
    {
      centity_t *cent = &cg_entities[ cg.snap->entities[ i ].number ];
      vec3_t relOrigin = { 0.0f, 0.0f, 0.0f };
      int team = cent->currentState.misc & 0x00FF;

      if( cent->currentState.eType != ET_PLAYER || 
          team != pci->team ||
          cent->currentState.eFlags & EF_DEAD )
      {
        continue;
      }

      VectorSubtract( cent->lerpOrigin, cg.predictedPlayerState.origin, relOrigin );

      if( VectorLength( relOrigin ) < HELMET_RANGE )
        displayClients[ maxDisplayCount++ ] = cg.snap->entities[ i ].number;
    }
  }

  // Sort
  if( sort == TEAMOVERLAY_SORT_SCORE )
  {
    qsort( displayClients, maxDisplayCount,
      sizeof( displayClients[ 0 ] ), SortScore );
  }
  else if( sort == TEAMOVERLAY_SORT_WEAPONCLASS )
  {
    qsort( displayClients, maxDisplayCount,
      sizeof( displayClients[ 0 ] ), SortWeaponClass );
  }

  if( maxDisplayCount > cg_teamOverlayMaxPlayers.integer )
    maxDisplayCount = cg_teamOverlayMaxPlayers.integer;

  iconSize *= scale;
  leftMargin *= scale;
  iconTopMargin *= scale;
  midSep *= scale;
  backgroundWidth *= scale;
  fontScale *= scale;
  nameWidth *= scale;

  vPad = ( rect->h - ( (float) maxDisplayCount * iconSize ) ) / 2.0f;
  y = rect->y + vPad;

  tcolor[ 0 ] = 1.0f;
  tcolor[ 1 ] = 1.0f;
  tcolor[ 2 ] = 1.0f;
  tcolor[ 3 ] = color[ 3 ];

  for( i = 0; i < MAX_CLIENTS && displayCount < maxDisplayCount; i++ )
  {
    ci = cgs.clientinfo + displayClients[ i ];

    if( !ci->infoValid || pci == ci || ci->team != pci->team )
      continue;

    Com_sprintf( name, sizeof( name ), "%s^7", ci->name );

    trap_R_SetColor( color );
    CG_DrawPic( x, y, backgroundWidth,
                iconSize, cgs.media.teamOverlayShader );
    trap_R_SetColor( tcolor );
    if( ci->health <= 0 || !ci->curWeaponClass )
      s = "";
    else
    {
      if( ci->team == TEAM_HUMANS )
        curWeapon = ci->curWeaponClass;
      else if( ci->team == TEAM_ALIENS )
        curWeapon = BG_Class( ci->curWeaponClass )->startWeapon;

      CG_DrawPic( x + leftMargin, y, iconSize, iconSize,
                  cg_weapons[ curWeapon ].weaponIcon );
      if( cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_HUMANS )
      {
        if( ci->upgrade != UP_NONE )
        {
          CG_DrawPic( x + iconSize + leftMargin, y, iconSize, 
                      iconSize, cg_upgrades[ ci->upgrade ].upgradeIcon );
        }
      }
      else
      {
        if( curWeapon == WP_ABUILD2 || curWeapon == WP_ALEVEL1_UPG ||
            curWeapon == WP_ALEVEL2_UPG || curWeapon == WP_ALEVEL3_UPG )
        {
          CG_DrawPic( x + iconSize + leftMargin, y, iconSize, 
                      iconSize, cgs.media.upgradeClassIconShader );
        }
      }

      s = va( " [^%c%3d^7] ^7%s",
              CG_GetColorCharForHealth( displayClients[ i ] ),
              BG_SU2HP( ci->health ),
              CG_ConfigString( CS_LOCATIONS + ci->location ) );
    }

    trap_R_SetColor( NULL );
    nameMaxX = nameMaxXCp = x + 2.0f * iconSize +
                            leftMargin + midSep + nameWidth;
    UI_Text_Paint_Limit( &nameMaxXCp, x + 2.0f * iconSize + leftMargin + midSep, 
                         y + iconSize - iconTopMargin, fontScale, tcolor, name,
                         0, 0 );

    maxXCp = maxX;

    UI_Text_Paint_Limit( &maxXCp, nameMaxX, y + iconSize - iconTopMargin, 
                         fontScale, tcolor, s, 0, 0 );
    y += iconSize;
    displayCount++;
  }
}

/*
=================
CG_DrawClock
=================
*/
static void CG_DrawClock( rectDef_t *rect, float text_x, float text_y,
                          float scale, vec4_t color,
                          int textalign, int textvalign, int textStyle )
{
  char    *s;
  float   tx, ty;
  int     i, strLength;
  float   w, h, totalWidth;
  qtime_t qt;

  if( !cg_drawClock.integer )
    return;

  trap_RealTime( &qt );

  if( cg_drawClock.integer == 2 )
  {
    s = va( "%02d%s%02d", qt.tm_hour, ( qt.tm_sec % 2 ) ? ":" : " ",
      qt.tm_min );
  }
  else
  {
    char *pm = "am";
    int h = qt.tm_hour;

    if( h == 0 )
      h = 12;
    else if( h == 12 )
      pm = "pm";
    else if( h > 12 )
    {
      h -= 12;
      pm = "pm";
    }

    s = va( "%d%s%02d%s", h, ( qt.tm_sec % 2 ) ? ":" : " ", qt.tm_min, pm );
  }
  w = UI_Text_Width( "0", scale );
  h = UI_Text_Height( "0", scale );
  strLength = CG_DrawStrlen( s );
  totalWidth = w * strLength;

  CG_AlignText( rect, s, 0.0f, totalWidth, h, textalign, textvalign, &tx, &ty );

  for( i = 0; i < strLength; i++ )
  {
    char c[ 2 ];

    c[ 0 ] = s[ i ];
    c[ 1 ] = '\0';

    UI_Text_Paint( text_x + tx + i * w, text_y + ty, scale, color, c, 0, 0, textStyle );
  }
}

/*
==================
CG_DrawSnapshot
==================
*/
static void CG_DrawSnapshot( rectDef_t *rect, float text_x, float text_y,
                             float scale, vec4_t color,
                             int textalign, int textvalign, int textStyle )
{
  char    *s;
  float   tx, ty;

  if( !cg_drawSnapshot.integer )
    return;

  s = va( "time:%d snap:%d cmd:%d", cg.snap->serverTime,
    cg.latestSnapshotNum, cgs.serverCommandSequence );

  CG_AlignText( rect, s, scale, 0.0f, 0.0f, textalign, textvalign, &tx, &ty );

  UI_Text_Paint( text_x + tx, text_y + ty, scale, color, s, 0, 0, textStyle );
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_SAMPLES   128

typedef struct
{
  int frameSamples[ LAG_SAMPLES ];
  int frameCount;
  int snapshotFlags[ LAG_SAMPLES ];
  int snapshotSamples[ LAG_SAMPLES ];
  int snapshotCount;
} lagometer_t;

lagometer_t   lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo( void )
{
  int     offset;

  offset = cg.time - cg.latestSnapshotTime;
  lagometer.frameSamples[ lagometer.frameCount & ( LAG_SAMPLES - 1 ) ] = offset;
  lagometer.frameCount++;
}

/*
==============
CG_AddLagometerSnapshotInfo

Each time a snapshot is received, log its ping time and
the number of snapshots that were dropped before it.

Pass NULL for a dropped packet.
==============
*/
#define PING_FRAMES 40
void CG_AddLagometerSnapshotInfo( snapshot_t *snap )
{
  static int  previousPings[ PING_FRAMES ];
  static int  index;
  int         i;

  // dropped packet
  if( !snap )
  {
    lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1 ) ] = -1;
    lagometer.snapshotCount++;
    return;
  }

  // add this snapshot's info
  lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1 ) ] = snap->ping;
  lagometer.snapshotFlags[ lagometer.snapshotCount & ( LAG_SAMPLES - 1 ) ] = snap->snapFlags;
  lagometer.snapshotCount++;

  cg.ping = 0;
  if( cg.snap )
  {
    previousPings[ index++ ] = cg.snap->ping;
    index = index % PING_FRAMES;

    for( i = 0; i < PING_FRAMES; i++ )
    {
      cg.ping += previousPings[ i ];
    }

    cg.ping /= PING_FRAMES;
  }
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect( void )
{
  float       x, y;
  int         cmdNum;
  usercmd_t   cmd;
  const char  *s;
  int         w;
  vec4_t      color = { 1.0f, 1.0f, 1.0f, 1.0f };

  // draw the phone jack if we are completely past our buffers
  cmdNum = trap_GetCurrentCmdNumber( ) - CMD_BACKUP + 1;
  trap_GetUserCmd( cmdNum, &cmd );

  // special check for map_restart
  if( cmd.serverTime <= cg.snap->ps.commandTime || cmd.serverTime > cg.time )
    return;

  // also add text in center of screen
  s = "Connection Interrupted";
  w = UI_Text_Width( s, 0.7f );
  UI_Text_Paint( 320 - w / 2, 100, 0.7f, color, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED );

  // blink the icon
  if( ( cg.time >> 9 ) & 1 )
    return;

  x = 640 - 48;
  y = 480 - 48;

  CG_DrawPic( x, y, 48, 48, trap_R_RegisterShader( "gfx/2d/net.tga" ) );
}

#define MAX_LAGOMETER_PING  900
#define MAX_LAGOMETER_RANGE 300


/*
==============
CG_DrawLagometer
==============
*/
static void CG_DrawLagometer( rectDef_t *rect, float text_x, float text_y,
    float scale, vec4_t textColor )
{
  int     a, x, y, i;
  float   v;
  float   ax, ay, aw, ah, mid, range;
  int     color;
  vec4_t  adjustedColor;
  float   vscale;
  char    *ping;

  if( cg.snap->ps.pm_type == PM_INTERMISSION )
    return;

  if( !cg_lagometer.integer )
    return;

  if( cg.demoPlayback )
    return;

  Vector4Copy( textColor, adjustedColor );
  adjustedColor[ 3 ] = 0.25f;

  trap_R_SetColor( adjustedColor );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.whiteShader );
  trap_R_SetColor( NULL );

  //
  // draw the graph
  //
  ax = x = rect->x;
  ay = y = rect->y;
  aw = rect->w;
  ah = rect->h;

  trap_R_SetColor( NULL );

  CG_AdjustFrom640( &ax, &ay, &aw, &ah );

  color = -1;
  range = ah / 3;
  mid = ay + range;

  vscale = range / MAX_LAGOMETER_RANGE;

  // draw the frame interpoalte / extrapolate graph
  for( a = 0 ; a < aw ; a++ )
  {
    i = ( lagometer.frameCount - 1 - a ) & ( LAG_SAMPLES - 1 );
    v = lagometer.frameSamples[ i ];
    v *= vscale;

    if( v > 0 )
    {
      if( color != 1 )
      {
        color = 1;
        trap_R_SetColor( g_color_table[ ColorIndex( COLOR_YELLOW ) ] );
      }

      if( v > range )
        v = range;

      trap_R_DrawStretchPic( ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
    }
    else if( v < 0 )
    {
      if( color != 2 )
      {
        color = 2;
        trap_R_SetColor( g_color_table[ ColorIndex( COLOR_BLUE ) ] );
      }

      v = -v;
      if( v > range )
        v = range;

      trap_R_DrawStretchPic( ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
    }
  }

  // draw the snapshot latency / drop graph
  range = ah / 2;
  vscale = range / MAX_LAGOMETER_PING;

  for( a = 0 ; a < aw ; a++ )
  {
    i = ( lagometer.snapshotCount - 1 - a ) & ( LAG_SAMPLES - 1 );
    v = lagometer.snapshotSamples[ i ];

    if( v > 0 )
    {
      if( lagometer.snapshotFlags[ i ] & SNAPFLAG_RATE_DELAYED )
      {
        if( color != 5 )
        {
          color = 5;  // YELLOW for rate delay
          trap_R_SetColor( g_color_table[ ColorIndex( COLOR_YELLOW ) ] );
        }
      }
      else
      {
        if( color != 3 )
        {
          color = 3;

          trap_R_SetColor( g_color_table[ ColorIndex( COLOR_GREEN ) ] );
        }
      }

      v = v * vscale;

      if( v > range )
        v = range;

      trap_R_DrawStretchPic( ax + aw - a, ay + ah - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
    }
    else if( v < 0 )
    {
      if( color != 4 )
      {
        color = 4;    // RED for dropped snapshots
        trap_R_SetColor( g_color_table[ ColorIndex( COLOR_RED ) ] );
      }

      trap_R_DrawStretchPic( ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, cgs.media.whiteShader );
    }
  }

  trap_R_SetColor( NULL );

  if( cg_nopredict.integer || cg_synchronousClients.integer )
    ping = "snc";
  else
    ping = va( "%d", cg.ping );
  ax = rect->x + ( rect->w / 2.0f ) -
       ( UI_Text_Width( ping, scale ) / 2.0f ) + text_x;
  ay = rect->y + ( rect->h / 2.0f ) +
       ( UI_Text_Height( ping, scale ) / 2.0f ) + text_y;

  Vector4Copy( textColor, adjustedColor );
  adjustedColor[ 3 ] = 0.5f;
  UI_Text_Paint( ax, ay, scale, adjustedColor, ping, 0, 0,
                 ITEM_TEXTSTYLE_NORMAL );

  CG_DrawDisconnect( );
}

#define SPEEDOMETER_NUM_SAMPLES 4096
#define SPEEDOMETER_NUM_DISPLAYED_SAMPLES 160
#define SPEEDOMETER_DRAW_TEXT   0x1
#define SPEEDOMETER_DRAW_GRAPH  0x2
#define SPEEDOMETER_IGNORE_Z    0x4
float speedSamples[ SPEEDOMETER_NUM_SAMPLES ];
int speedSampleTimes[ SPEEDOMETER_NUM_SAMPLES ];
// array indices
int oldestSpeedSample = 0;
int maxSpeedSample = 0;
int maxSpeedSampleInWindow = 0;

/*
===================
CG_AddSpeed

append a speed to the sample history
===================
*/
void CG_AddSpeed( void )
{
  float speed;
  vec3_t vel;
  int windowTime;
  qboolean newSpeedGteMaxSpeed, newSpeedGteMaxSpeedInWindow;

  VectorCopy( cg.snap->ps.velocity, vel );

  if( cg_drawSpeed.integer & SPEEDOMETER_IGNORE_Z )
    vel[ 2 ] = 0;

  speed = VectorLength( vel );

  windowTime = cg_maxSpeedTimeWindow.integer;
  if( windowTime < 0 )
    windowTime = 0;
  else if( windowTime > SPEEDOMETER_NUM_SAMPLES * 1000 )
    windowTime = SPEEDOMETER_NUM_SAMPLES * 1000;

  if( ( newSpeedGteMaxSpeed = ( speed >= speedSamples[ maxSpeedSample ] ) ) )
    maxSpeedSample = oldestSpeedSample;

  if( ( newSpeedGteMaxSpeedInWindow = ( speed >= speedSamples[ maxSpeedSampleInWindow ] ) ) )
    maxSpeedSampleInWindow = oldestSpeedSample;

  speedSamples[ oldestSpeedSample ] = speed;
  speedSampleTimes[ oldestSpeedSample ] = cg.time;

  if( !newSpeedGteMaxSpeed && maxSpeedSample == oldestSpeedSample )
  {
    // if old max was overwritten find a new one
    int i;
    for( maxSpeedSample = 0, i = 1; i < SPEEDOMETER_NUM_SAMPLES; i++ )
    {
      if( speedSamples[ i ] > speedSamples[ maxSpeedSample ] )
        maxSpeedSample = i;
    }
  }

  if( !newSpeedGteMaxSpeedInWindow && ( maxSpeedSampleInWindow == oldestSpeedSample ||
        cg.time - speedSampleTimes[ maxSpeedSampleInWindow ] > windowTime ) )
  {
    int i;
    do {
      maxSpeedSampleInWindow = ( maxSpeedSampleInWindow + 1 ) % SPEEDOMETER_NUM_SAMPLES;
    } while( cg.time - speedSampleTimes[ maxSpeedSampleInWindow ] > windowTime );
    for( i = maxSpeedSampleInWindow; ; i = ( i + 1 ) % SPEEDOMETER_NUM_SAMPLES )
    {
      if( speedSamples[ i ] > speedSamples[ maxSpeedSampleInWindow ] )
        maxSpeedSampleInWindow = i;
      if( i == oldestSpeedSample )
        break;
    }
  }

  oldestSpeedSample = ( oldestSpeedSample + 1 ) % SPEEDOMETER_NUM_SAMPLES;
}

#define SPEEDOMETER_MIN_RANGE 900
#define SPEED_MED 1000.f
#define SPEED_FAST 1600.f

/*
===================
CG_DrawSpeedGraph
===================
*/
static void CG_DrawSpeedGraph( rectDef_t *rect, vec4_t foreColor,
                               vec4_t backColor )
{
  int i;
  float val, max, top;
  // colour of graph is interpolated between these values
  const vec3_t slow = { 0.0, 0.0, 1.0 };
  const vec3_t medium = { 0.0, 1.0, 0.0 };
  const vec3_t fast = { 1.0, 0.0, 0.0 };
  vec4_t color;

  max = speedSamples[ maxSpeedSample ];
  if( max < SPEEDOMETER_MIN_RANGE )
    max = SPEEDOMETER_MIN_RANGE;

  trap_R_SetColor( backColor );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.whiteShader );

  Vector4Copy( foreColor, color );

  for( i = 1; i < SPEEDOMETER_NUM_DISPLAYED_SAMPLES; i++ )
  {
    val = speedSamples[ ( oldestSpeedSample + i + SPEEDOMETER_NUM_SAMPLES -
      SPEEDOMETER_NUM_DISPLAYED_SAMPLES ) % SPEEDOMETER_NUM_SAMPLES ];
    if( val < SPEED_MED )
      VectorLerp2( val / SPEED_MED, slow, medium, color );
    else if( val < SPEED_FAST )
      VectorLerp2( ( val - SPEED_MED ) / ( SPEED_FAST - SPEED_MED ),
                  medium, fast, color );
    else
      VectorCopy( fast, color );
    trap_R_SetColor( color );
    top = rect->y + ( 1 - val / max ) * rect->h;
    CG_DrawPic( rect->x + ( i / (float)SPEEDOMETER_NUM_DISPLAYED_SAMPLES ) * rect->w, top,
                rect->w / (float)SPEEDOMETER_NUM_DISPLAYED_SAMPLES, val * rect->h / max,
                cgs.media.whiteShader );
  }
  trap_R_SetColor( NULL );
}

/*
===================
CG_DrawSpeedText
===================
*/
static void CG_DrawSpeedText( rectDef_t *rect, float text_x, float text_y,
                              float scale, vec4_t foreColor )
{
  char speedstr[ 16 ];
  float val;
  vec4_t color;

  VectorCopy( foreColor, color );
  color[ 3 ] = 1;
  if( cg.predictedPlayerState.clientNum == cg.clientNum )
  {
    vec3_t vel;
    VectorCopy( cg.predictedPlayerState.velocity, vel );
    if( cg_drawSpeed.integer & SPEEDOMETER_IGNORE_Z )
      vel[ 2 ] = 0;
    val = VectorLength( vel );
  }
  else
    val = speedSamples[ ( oldestSpeedSample - 1 + SPEEDOMETER_NUM_SAMPLES ) % SPEEDOMETER_NUM_SAMPLES ];

  Com_sprintf( speedstr, sizeof( speedstr ), "%d / %d", (int)val, (int)speedSamples[ maxSpeedSampleInWindow ] );

  UI_Text_Paint(
      rect->x + ( rect->w - UI_Text_Width( speedstr, scale ) ) / 2.0f,
      rect->y + ( rect->h + UI_Text_Height( speedstr, scale ) ) / 2.0f,
      scale, color, speedstr, 0, 0, ITEM_TEXTSTYLE_NORMAL );
}

/*
===================
CG_DrawSpeed
===================
*/
static void CG_DrawSpeed( rectDef_t *rect, float text_x, float text_y,
                          float scale, vec4_t foreColor, vec4_t backColor )
{
  if( cg_drawSpeed.integer & SPEEDOMETER_DRAW_GRAPH )
    CG_DrawSpeedGraph( rect, foreColor, backColor );
  if( cg_drawSpeed.integer & SPEEDOMETER_DRAW_TEXT )
    CG_DrawSpeedText( rect, text_x, text_y, scale, foreColor );
}

/*
===================
CG_DrawConsole
===================
*/
static void CG_DrawConsole( rectDef_t *rect, float text_x, float text_y, vec4_t color,
                            float scale, int textalign, int textvalign, int textStyle )
{
  UI_DrawTextBlock( rect, text_x, text_y, color, scale, textalign, textvalign, textStyle, cg.consoleText );
}

/*
===================
CG_DrawTutorial
===================
*/
static void CG_DrawTutorial( rectDef_t *rect, float text_x, float text_y, vec4_t color,
                            float scale, int textalign, int textvalign, int textStyle )
{
  if( !cg_tutorial.integer )
    return;

  UI_DrawTextBlock( rect, text_x, text_y, color, scale, textalign, textvalign, textStyle, CG_TutorialText( ) );
}

/*
===================
CG_DrawWeaponIcon
===================
*/
void CG_DrawWeaponIcon( rectDef_t *rect, vec4_t color )
{
  int           maxAmmo;
  playerState_t *ps;
  weapon_t      weapon;

  ps = &cg.snap->ps;
  weapon = BG_GetPlayerWeapon( ps );

  maxAmmo = BG_Weapon( weapon )->maxAmmo;

  // don't display if dead
  if( cg.predictedPlayerState.misc[ MISC_HEALTH ] <= 0 )
    return;

  if( weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS )
    return;

  if( !cg_weapons[ weapon ].registered )
  {
    Com_Printf( S_COLOR_YELLOW "WARNING: CG_DrawWeaponIcon: weapon %d (%s) "
        "is not registered\n", weapon, BG_Weapon( weapon )->name );
    return;
  }

  if( ps->clips == 0 && !BG_Weapon( weapon )->infiniteAmmo )
  {
    float ammoPercent = (float)ps->ammo / (float)maxAmmo;

    if( ammoPercent < 0.33f )
    {
      color[ 0 ] = 1.0f;
      color[ 1 ] = color[ 2 ] = 0.0f;
    }
  }

  if( cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_ALIENS &&
      !BG_AlienCanEvolve( cg.predictedPlayerState.stats[ STAT_CLASS ],
                          ps->persistant[ PERS_CREDIT ], cgs.alienStage,
                          cgs.warmup, cgs.devMode,
                          (cg.predictedPlayerState.stats[STAT_FLAGS] & SFL_CLASS_FORCED)) )
  {
    if( cg.time - cg.lastEvolveAttempt <= NO_CREDITS_TIME )
    {
      if( ( ( cg.time - cg.lastEvolveAttempt ) / 300 ) % 2 )
        color[ 3 ] = 0.0f;
    }
  }

  trap_R_SetColor( color );
  CG_DrawPic( rect->x, rect->y, rect->w, rect->h,
              cg_weapons[ weapon ].weaponIcon );
  trap_R_SetColor( NULL );
}



/*
================================================================================

CROSSHAIR

================================================================================
*/



/*
=================
CG_DrawCrosshair
=================
*/
static void CG_DrawCrosshair( rectDef_t *rect, vec4_t color )
{
  float         w, h;
  qhandle_t     hShader;
  float         x, y;
  weaponInfo_t  *wi;
  weapon_t      weapon;

  weapon = BG_GetPlayerWeapon( &cg.snap->ps );

  if( cg_drawCrosshair.integer == CROSSHAIR_ALWAYSOFF )
    return;

  if( cg_drawCrosshair.integer == CROSSHAIR_RANGEDONLY &&
      !BG_Weapon( weapon )->longRanged )
    return;

  if( cg.snap->ps.persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT )
    return;

  if( cg.renderingThirdPerson )
    return;

  if( cg.snap->ps.pm_type == PM_INTERMISSION )
    return;

  wi = &cg_weapons[ weapon ];

  w = h = wi->crossHairSize * cg_crosshairSize.value;
  w *= cgDC.aspectScale;

  //FIXME: this still ignores the width/height of the rect, but at least it's
  //neater than cg_crosshairX/cg_crosshairY
  x = rect->x + ( rect->w / 2 ) - ( w / 2 );
  y = rect->y + ( rect->h / 2 ) - ( h / 2 );

  hShader = wi->crossHair;

  if( cg.time <= cg.lastHitTime + 75 )
  {
    if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
    {
      color[0] = 0.0;
      color[1] = 1.0;
      color[2] = 0.0;
    }
    else
    {
      color[0] = 1.0;
      color[1] = 0.0;
      color[2] = 0.0;
    }
  }
  else if( cg.time == cg.crosshairClientTime || cg.crosshairBuildable >= 0 )
  {
    int i;

    //aiming at a friendly player/buildable, dim the crosshair
    for( i = 0; i < 3; i++ )
      color[i] *= .5f;

  }

  if( hShader != 0 )
  {

    trap_R_SetColor( color );
    CG_DrawPic( x, y, w, h, hShader );
    trap_R_SetColor( NULL );
  }
}



/*
=================
CG_ScanForCrosshairEntity
=================
*/
static void CG_ScanForCrosshairEntity( void )
{
  trace_t   trace;
  vec3_t    start, end;
/*  int       content;*/
  team_t    team;

  VectorCopy( cg.refdef.vieworg, start );
  VectorMA( start, 131072, cg.refdef.viewaxis[ 0 ], end );

  CG_Trace( &trace, start, vec3_origin, vec3_origin, end,
    cg.snap->ps.clientNum, CONTENTS_SOLID|CONTENTS_BODY );

  // if the player is in fog, don't show it
/*  content = trap_CM_PointContents( trace.endpos, 0 );*/
/*  if( content & CONTENTS_FOG )*/
/*    return;*/

  if( trace.entityNum >= MAX_CLIENTS )
  {
    entityState_t *s = &cg_entities[ trace.entityNum ].currentState;
    if( s->eType == ET_BUILDABLE )
    {
      if( BG_Buildable( s->modelindex )->team == cg.snap->ps.stats[ STAT_TEAM ] )
      {
        if( cg.crosshairBuildable != trace.entityNum )
          cg.crosshairNewBuildableTime = cg.time;
        cg.crosshairBuildable = trace.entityNum;
      }
      else
      {
        cg.crosshairBuildable = -1;
      }
    }
    else
      cg.crosshairBuildable = -1;

    return;
  }

  team = cgs.clientinfo[ trace.entityNum ].team;


  if( ( team == cg.snap->ps.stats[ STAT_TEAM ] ) ||
      ( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_NONE ) )
  {
    // update the fade timer
    cg.crosshairClientNum = trace.entityNum;
    cg.crosshairClientTime = cg.time;
  }
  // only display team names of those on the same team as this player,


}


/*
=====================
CG_DrawLocation
=====================
*/
static void CG_DrawLocation( rectDef_t *rect, float scale, int textalign, vec4_t color )
{
  const char    *location;
  centity_t     *locent;
  float         maxX;
  float         tx = rect->x, ty = rect->y;

  if( cg.intermissionStarted )
    return;

  maxX = rect->x + rect->w;

  locent = CG_GetPlayerLocation( );
  if( !locent )
    return;
  location = CG_ConfigString( CS_LOCATIONS + locent->currentState.generic1 );

  // need to skip horiz. align if it's too long, but valign must be run either way
  if( UI_Text_Width( location, scale ) < rect->w )
  {
    CG_AlignText( rect, location, scale, 0.0f, 0.0f, textalign, VALIGN_CENTER, &tx, &ty );
    UI_Text_Paint( tx, ty, scale, color, location, 0, 0, ITEM_TEXTSTYLE_NORMAL );
  }
  else
  {
    CG_AlignText( rect, location, scale, 0.0f, 0.0f, ALIGN_NONE, VALIGN_CENTER, &tx, &ty );
    UI_Text_Paint_Limit( &maxX, tx, ty, scale, color, location, 0, 0 );
  }

  trap_R_SetColor( NULL );
}

/*
=====================
CG_DrawWarmup
=====================
*/
static void CG_DrawWarmup( int ownerDraw, rectDef_t *rect, float textScale, int textAlign, int textStyle, vec4_t color )
{
  char          warmupText[ MAX_STRING_CHARS ];
  float         tx = rect->x, ty = rect->y;

  if( cg.intermissionStarted )
    return;

  if( !cgs.warmup ) {
    if(
      IS_SCRIM &&
      (
        (cg.snap && (cg.snap->ps.pm_flags & PMF_FOLLOW)) ||
        cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_NONE) &&
      ownerDraw == CG_WARMUP) {
      //always indicate to specfators if a scrim is in process
      Com_sprintf( warmupText, sizeof( warmupText ), "SCRIM" );
    } else {
      return;
    }
  }
  else {
    switch( ownerDraw )
    {
      case CG_WARMUP:
        if(IS_SCRIM) {
          switch(cgs.scrim.mode) {
            case SCRIM_MODE_SETUP:
              Com_sprintf( warmupText, sizeof( warmupText ), "SCRIM SETUP" );
              break;

            case SCRIM_MODE_TIMEOUT:
              Com_sprintf( warmupText, sizeof( warmupText ), "SCRIM TIME OUT" );;
              break;

            default:
              Com_sprintf( warmupText, sizeof( warmupText ), "SCRIM WARMUP" );
              break;
          }
        } else {
          Com_sprintf( warmupText, sizeof( warmupText ), "PRE-GAME WARMUP" );
        }
        break;
      case CG_WARMUP_PLAYER_READY:
        if( cg.predictedPlayerState.stats[ STAT_TEAM ] == TEAM_NONE )
          return;
        Com_sprintf( warmupText, sizeof( warmupText ), "( You Are %sReady )",
            (cg.predictedPlayerState.stats[ STAT_FLAGS ] & SFL_READY ) ? "" : "Not " );
        break;
      case CG_WARMUP_ALIENS_READY_HDR:
        Com_sprintf( warmupText, sizeof( warmupText ), "Aliens" );
        break;
      case CG_WARMUP_ALIENS_READY:
        Com_sprintf( warmupText, sizeof( warmupText ), "%d/%d Ready",
            cgs.numAliensReady, cgs.numAliens );
        break;
      case CG_WARMUP_HUMANS_READY_HDR:
        Com_sprintf( warmupText, sizeof( warmupText ), "Humans" );
        break;
      case CG_WARMUP_HUMANS_READY:
        Com_sprintf( warmupText, sizeof( warmupText ), "%d/%d Ready",
            cgs.numHumansReady, cgs.numHumans );
        break;
    }
  }

  CG_AlignText( rect, warmupText, textScale, 0.0f, 0.0f, textAlign, VALIGN_CENTER, &tx, &ty );
  UI_Text_Paint( tx, ty, textScale, color, warmupText, 0, 0, textStyle );

  trap_R_SetColor( NULL );
}

/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void CG_DrawCrosshairNames( rectDef_t *rect, float scale, int textStyle )
{
  float   *color;
  char    *name;
  float   w, x;

  if( !cg_drawCrosshairNames.integer )
    return;

  if( cg.renderingThirdPerson )
    return;

  // scan the known entities to see if the crosshair is sighted on one
  CG_ScanForCrosshairEntity( );

  // draw the name of the player being looked at
  color = CG_FadeColor( cg.crosshairClientTime, CROSSHAIR_CLIENT_TIMEOUT );
  if( !color )
  {
    trap_R_SetColor( NULL );
    return;
  }

  // add health from overlay info to the crosshair client name
  name = cgs.clientinfo[ cg.crosshairClientNum ].name;
  if( cg_teamOverlayUserinfo.integer &&
      cg.snap->ps.stats[ STAT_TEAM ] != TEAM_NONE &&
      cgs.teaminfoReceievedTime &&
      cgs.clientinfo[ cg.crosshairClientNum ].health > 0 )
  {
    name = va( "%s ^7[^%c%d^7]", name,
               CG_GetColorCharForHealth( cg.crosshairClientNum ),
               BG_SU2HP( cgs.clientinfo[ cg.crosshairClientNum ].health ) );
  }

  if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_NONE )
  {
    name = va( "^7Looking at %s^7", name );
  }

  w = UI_Text_Width( name, scale );
  x = rect->x + rect->w / 2.0f;
  UI_Text_Paint( x - w / 2.0f, rect->y + rect->h, scale, color, name, 0, 0, textStyle );
  trap_R_SetColor( NULL );
}

/*
===============
CG_OwnerDraw

Draw an owner drawn item
===============
*/
void CG_OwnerDraw( float x, float y, float w, float h, float text_x,
                   float text_y, int ownerDraw, int ownerDrawFlags,
                   int align, int textalign, int textvalign, float borderSize,
                   float scale, vec4_t foreColor, vec4_t backColor,
                   qhandle_t shader, int textStyle )
{
  rectDef_t rect;

  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;

  switch( ownerDraw )
  {
    case CG_PLAYER_CREDITS_VALUE:
      CG_DrawPlayerCreditsValue( &rect, foreColor, qtrue );
      break;
    case CG_PLAYER_CREDITS_FRACTION:
      CG_DrawPlayerCreditsFraction( &rect, foreColor, shader );
      break;
    case CG_PLAYER_CREDITS_VALUE_NOPAD:
      CG_DrawPlayerCreditsValue( &rect, foreColor, qfalse );
      break;
    case CG_PLAYER_STAMINA_1:
    case CG_PLAYER_STAMINA_2:
    case CG_PLAYER_STAMINA_3:
    case CG_PLAYER_STAMINA_4:
      CG_DrawPlayerStamina( ownerDraw, &rect, backColor, foreColor, shader );
      break;
    case CG_PLAYER_STAMINA_BOLT:
      CG_DrawPlayerStaminaBolt( &rect, backColor, foreColor, shader );
      break;
    case CG_PLAYER_AMMO_VALUE:
      CG_DrawPlayerAmmoValue( &rect, foreColor );
      break;
    case CG_PLAYER_CLIPS_VALUE:
      CG_DrawPlayerClipsValue( &rect, foreColor );
      break;
    case CG_PLAYER_BUILD_TIMER:
      CG_DrawPlayerBuildTimer( &rect, foreColor );
      break;
    case CG_PLAYER_HEALTH:
      CG_DrawPlayerHealthValue( &rect, foreColor );
      break;
    case CG_PLAYER_HEALTH_CROSS:
      CG_DrawPlayerHealthCross( &rect, foreColor );
      break;
    case CG_PLAYER_CHARGE_BAR_BG:
      CG_DrawPlayerChargeBarBG( &rect, foreColor, shader, qfalse );
      break;
    case CG_PLAYER_CHARGE_BAR:
      CG_DrawPlayerChargeBar( &rect, foreColor, shader, qfalse );
      break;
    case CG_PLAYER_CHARGE_STAMINA_BAR_BG:
      CG_DrawPlayerChargeBarBG( &rect, foreColor, shader, qtrue );
      break;
    case CG_PLAYER_CHARGE_STAMINA_BAR:
      CG_DrawPlayerChargeBar( &rect, foreColor, shader, qtrue );
      break;
    case CG_PLAYER_CLIPS_RING:
      CG_DrawPlayerClipsRing( &rect, backColor, foreColor, shader );
      break;
    case CG_PLAYER_BUILD_TIMER_RING:
      CG_DrawPlayerBuildTimerRing( &rect, backColor, foreColor, shader );
      break;
    case CG_PLAYER_WALLCLIMBING:
      CG_DrawPlayerWallclimbing( &rect, backColor, foreColor, shader );
      break;
    case CG_PLAYER_BOOSTED:
      CG_DrawPlayerBoosted( &rect, backColor, foreColor, shader );
      break;
    case CG_PLAYER_BOOST_BOLT:
      CG_DrawPlayerBoosterBolt( &rect, backColor, foreColor, shader );
      break;
    case CG_PLAYER_POISON_BARBS:
      CG_DrawPlayerPoisonBarbs( &rect, foreColor, shader );
      break;
    case CG_PLAYER_ALIEN_SENSE:
      CG_DrawAlienSense( &rect );
      break;
    case CG_PLAYER_HUMAN_SCANNER:
      CG_DrawHumanScanner( &rect, shader, foreColor );
      break;
    case CG_PLAYER_USABLE_BUILDABLE:
      CG_DrawUsableBuildable( &rect, shader, foreColor );
      break;
    case CG_KILLER:
      CG_DrawKiller( &rect, scale, foreColor, shader, textStyle );
      break;
    case CG_PLAYER_SELECT:
      CG_DrawItemSelect( &rect, foreColor );
      break;
    case CG_PLAYER_WEAPONICON:
      CG_DrawWeaponIcon( &rect, foreColor );
      break;
    case CG_PLAYER_SELECTTEXT:
      CG_DrawItemSelectText( &rect, scale, textStyle );
      break;
    case CG_SPECTATORS:
      CG_DrawTeamSpectators( &rect, scale, textvalign, foreColor, shader );
      break;
    case CG_PLAYER_LOCATION:
      CG_DrawLocation( &rect, scale, textalign, foreColor );
      break;
    case CG_FOLLOW:
      CG_DrawFollow( &rect, text_x, text_y, foreColor, scale,
                     textalign, textvalign, textStyle );
      break;
    case CG_PLAYER_CROSSHAIRNAMES:
      CG_DrawCrosshairNames( &rect, scale, textStyle );
      break;
    case CG_PLAYER_CROSSHAIR:
      CG_DrawCrosshair( &rect, foreColor );
      break;
    case CG_STAGE_REPORT_TEXT:
      CG_DrawStageReport( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_SCRIM_SETTINGS:
      CG_DrawScrimSettings( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_SCRIM_WIN_CONDITION:
      CG_DrawScrimWinCondition( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_SCRIM_STATUS:
      CG_DrawScrimStatus( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_ALIENS_SCRIM_TEAM_STATUS:
      CG_DrawScrimTeamStatus( &rect, TEAM_ALIENS, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_HUMANS_SCRIM_TEAM_STATUS:
      CG_DrawScrimTeamStatus( &rect, TEAM_HUMANS, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_ALIENS_SCRIM_TEAM_LABEL:
      CG_DrawScrimTeamLabel( &rect, TEAM_ALIENS, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_HUMANS_SCRIM_TEAM_LABEL:
      CG_DrawScrimTeamLabel( &rect, TEAM_HUMANS, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_ALIENS_SCORE_LABEL:
      CG_DrawTeamLabel( &rect, TEAM_ALIENS, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_HUMANS_SCORE_LABEL:
      CG_DrawTeamLabel( &rect, TEAM_HUMANS, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;

    //loading screen
    case CG_LOAD_LEVELSHOT:
      CG_DrawLevelShot( &rect );
      break;
    case CG_LOAD_MEDIA:
      CG_DrawMediaProgress( &rect, foreColor, scale, align, textalign, textStyle,
                            borderSize );
      break;
    case CG_LOAD_MEDIA_LABEL:
      CG_DrawMediaProgressLabel( &rect, text_x, text_y, foreColor, scale, textalign, textvalign );
      break;
    case CG_LOAD_BUILDABLES:
      CG_DrawBuildablesProgress( &rect, foreColor, scale, align, textalign,
                                 textStyle, borderSize );
      break;
    case CG_LOAD_BUILDABLES_LABEL:
      CG_DrawBuildablesProgressLabel( &rect, text_x, text_y, foreColor, scale, textalign, textvalign );
      break;
    case CG_LOAD_CHARMODEL:
      CG_DrawCharModelProgress( &rect, foreColor, scale, align, textalign,
                                textStyle, borderSize );
      break;
    case CG_LOAD_CHARMODEL_LABEL:
      CG_DrawCharModelProgressLabel( &rect, text_x, text_y, foreColor, scale, textalign, textvalign );
      break;
    case CG_LOAD_OVERALL:
      CG_DrawOverallProgress( &rect, foreColor, scale, align, textalign, textStyle,
                              borderSize );
      break;
    case CG_LOAD_LEVELNAME:
      CG_DrawLevelName( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_LOAD_MOTD:
      CG_DrawMOTD( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;
    case CG_LOAD_HOSTNAME:
      CG_DrawHostname( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;

    case CG_FPS:
      CG_DrawFPS( &rect, text_x, text_y, scale, foreColor, textalign, textvalign, textStyle, qtrue );
      break;
    case CG_FPS_FIXED:
      CG_DrawFPS( &rect, text_x, text_y, scale, foreColor, textalign, textvalign, textStyle, qfalse );
      break;
    case CG_TIMER:
      CG_DrawTimer( &rect, text_x, text_y, scale, foreColor, textalign, textvalign, textStyle );
      break;
    case CG_CLOCK:
      CG_DrawClock( &rect, text_x, text_y, scale, foreColor, textalign, textvalign, textStyle );
      break;
    case CG_TIMER_MINS:
      CG_DrawTimerMins( &rect, foreColor );
      break;
    case CG_TIMER_SECS:
      CG_DrawTimerSecs( &rect, foreColor );
      break;
    case CG_SNAPSHOT:
      CG_DrawSnapshot( &rect, text_x, text_y, scale, foreColor, textalign, textvalign, textStyle );
      break;
    case CG_LAGOMETER:
      CG_DrawLagometer( &rect, text_x, text_y, scale, foreColor );
      break;
    case CG_TEAMOVERLAY:
      CG_DrawTeamOverlay( &rect, scale, foreColor );
      break;
    case CG_SPEEDOMETER:
      CG_DrawSpeed( &rect, text_x, text_y, scale, foreColor, backColor );
      break;

    case CG_DEMO_PLAYBACK:
      CG_DrawDemoPlayback( &rect, foreColor, shader );
      break;
    case CG_DEMO_RECORDING:
      CG_DrawDemoRecording( &rect, foreColor, shader );
      break;

    case CG_CONSOLE:
      CG_DrawConsole( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;

    case CG_TUTORIAL:
      if( !( cgs.voteTime[ TEAM_NONE ] || cgs.voteTime[ cg.predictedPlayerState.stats[ STAT_TEAM ] ] ) )
        CG_DrawTutorial( &rect, text_x, text_y, foreColor, scale, textalign, textvalign, textStyle );
      break;

    // Warmup stuff
    case CG_WARMUP: case CG_WARMUP_PLAYER_READY:
    case CG_WARMUP_ALIENS_READY_HDR: case CG_WARMUP_ALIENS_READY:
    case CG_WARMUP_HUMANS_READY_HDR: case CG_WARMUP_HUMANS_READY:
      CG_DrawWarmup( ownerDraw, &rect, scale, textalign, textStyle, foreColor );
      break;

    // Equipment values
    case CG_PLAYER_EQUIP_HUD:
      CG_DrawEquipmentHUD( &rect, foreColor, shader );
      break;
    // Jet Fuel
    case CG_PLAYER_JETPACK_ICON:
      CG_DrawJetpackIcon( &rect, foreColor, shader );
      break;
    case CG_PLAYER_JETPACK_FUEL:
      CG_DrawJetpackFuel( &rect, foreColor );
      break;

    default:
      break;
  }
}

void CG_MouseEvent( int x, int y )
{
  int n;

  if( ( cg.predictedPlayerState.pm_type == PM_NORMAL ||
        cg.predictedPlayerState.pm_type == PM_SPECTATOR ) &&
        cg.showScores == qfalse )
  {
    trap_Key_SetCatcher( 0 );
    return;
  }

  cgs.cursorX += x;
  if( cgs.cursorX < 0 )
    cgs.cursorX = 0;
  else if( cgs.cursorX > 640 )
    cgs.cursorX = 640;

  cgs.cursorY += y;
  if( cgs.cursorY < 0 )
    cgs.cursorY = 0;
  else if( cgs.cursorY > 480 )
    cgs.cursorY = 480;

  n = Display_CursorType( cgs.cursorX, cgs.cursorY );
  cgs.activeCursor = 0;
  if( n == CURSOR_ARROW )
    cgs.activeCursor = cgs.media.selectCursor;
  else if( n == CURSOR_SIZER )
    cgs.activeCursor = cgs.media.sizeCursor;

  if( cgs.capturedItem )
    Display_MouseMove( cgs.capturedItem, x, y );
  else
    Display_MouseMove( NULL, cgs.cursorX, cgs.cursorY );
}

/*
==================
CG_HideTeamMenus
==================

*/
void CG_HideTeamMenu( void )
{
  Menus_CloseByName( "teamMenu" );
  Menus_CloseByName( "getMenu" );
}

/*
==================
CG_ShowTeamMenus
==================

*/
void CG_ShowTeamMenu( void )
{
  Menus_ActivateByName( "teamMenu" );
}

/*
==================
CG_EventHandling

type 0 - no event handling
     1 - team menu
     2 - hud editor
==================
*/
void CG_EventHandling( int type )
{
  cgs.eventHandling = type;

  if( type == CGAME_EVENT_NONE )
    CG_HideTeamMenu( );
}



void CG_KeyEvent( int key, qboolean down )
{
  if( !down )
    return;

  if( cg.predictedPlayerState.pm_type == PM_NORMAL ||
      ( cg.predictedPlayerState.pm_type == PM_SPECTATOR &&
        cg.showScores == qfalse ) )
  {
    CG_EventHandling( CGAME_EVENT_NONE );
    trap_Key_SetCatcher( 0 );
    return;
  }

  Display_HandleKey( key, down, cgs.cursorX, cgs.cursorY );

  if( cgs.capturedItem )
    cgs.capturedItem = NULL;
  else
  {
    if( key == K_MOUSE2 && down )
      cgs.capturedItem = Display_CaptureItem( cgs.cursorX, cgs.cursorY );
  }
}

int CG_ClientNumFromName( const char *p )
{
  int i;

  for( i = 0; i < cgs.maxclients; i++ )
  {
    if( cgs.clientinfo[ i ].infoValid &&
        Q_stricmp( cgs.clientinfo[ i ].name, p ) == 0 )
      return i;
  }

  return -1;
}

void CG_RunMenuScript( char **args )
{
}
//END TA UI


/*
================
CG_DrawLighting

================
*/
static void CG_DrawLighting( void )
{
  //fade to black if stamina is low
   if( cgs.humanBlackout && 
       !BG_InventoryContainsUpgrade( UP_BATTLESUIT, cg.predictedPlayerState.stats ) &&
       ( cg.snap->ps.stats[ STAT_STAMINA ] < STAMINA_BLACKOUT_LEVEL ) &&
       ( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ) )
  {
    vec4_t black = { 0, 0, 0, 0 };
    black[ 3 ] = 1.0 - ( (float)( cg.snap->ps.stats[ STAT_STAMINA ] + 1000 ) / 200.0f );
    trap_R_SetColor( black );
    CG_DrawPic( 0, 0, 640, 480, cgs.media.whiteShader );
    trap_R_SetColor( NULL );
  }
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/


/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint( int index, const char *str, int charWidth, int showTime )
{
  char newlineParsed[ MAX_STRING_CHARS ];
  const char *wrapped;
  static int maxWidth = (int)( ( 2.0f / 3.0f ) * (float)SCREEN_WIDTH );
  struct centerPrinting_s *ce_pr;

  if( index < 0 )
  {
    int i;
    index = -index;
    for( i = -index; i < CP_MAX; i++ )
    {
      if( cg.centerPrinting[ i ].expireTime < cg.time )
      {
        index = i;
        break;
      }
    }
  }

  if( index >= CP_MAX )
  {
    index = CP_MAX - 1;
  }

  ce_pr = &cg.centerPrinting[ index ];

  Q_ParseNewlines( newlineParsed, str, sizeof( newlineParsed ) );

  wrapped = Item_Text_Wrap( newlineParsed, 0.5f, maxWidth );

  Q_strncpyz( ce_pr->str, wrapped, sizeof( ce_pr->str ) );

  ce_pr->createTime = cg.time;
  if( showTime <= 0 )
  {
    ce_pr->expireTime = 1000 * cg_centertime.value;
  } else
  {
    ce_pr->expireTime = showTime;
  }
  ce_pr->charWidth = charWidth;
}


/*
===================
CG_DrawCenterStrings
===================
*/
static void CG_DrawCenterStrings( void )
{
  char  *start;
  int   i, l, y = cg_centerYOffset.integer;
  float *color;
  struct centerPrinting_s *ce_pr = cg.centerPrinting;

  for( i = 0; i < CP_MAX; i++, ce_pr++ )
  {
    if( ce_pr->expireTime + ce_pr->createTime < cg.time )
    continue;

    color = CG_FadeColor( ce_pr->createTime,
                           ce_pr->expireTime );
    if( !color )
      continue;

    trap_R_SetColor( color );

    start = ce_pr->str;
    while( 1 )
    {
      int w, h, x;
      char linebuffer[ MAX_STRING_CHARS ];

      for( l = 0; l < sizeof(linebuffer) - 1; l++ )
      {
        if( !start[ l ] || start[ l ] == '\n' )
          break;

        linebuffer[ l ] = start[ l ];
      }

      linebuffer[ l ] = 0;

      w = UI_Text_Width( linebuffer, 0.5 );
      h = UI_Text_Height( linebuffer, 0.5 );
      x = ( SCREEN_WIDTH - w ) / 2;
      UI_Text_Paint( x, y + h, 0.5, color, linebuffer, 0, 0,
                     ITEM_TEXTSTYLE_SHADOWEDMORE );
      y += h + 6;

      while( *start && ( *start != '\n' ) )
        start++;
      if( !*start )
        break;

        start++;
      }
  }

  trap_R_SetColor( NULL );
}





//==============================================================================

//FIXME: both vote notes are hardcoded, change to ownerdrawn?

/*
=================
CG_DrawVote
=================
*/
static void CG_DrawVote( team_t team )
{
  char    *s;
  int     sec;
  int     offset = 0;
  vec4_t  white = { 1.0f, 1.0f, 1.0f, 1.0f };
  char    yeskey[ 32 ] = "", nokey[ 32 ] = "";

  if( !cgs.voteTime[ team ] )
    return;

    if( cgs.voteAlarmPlay[ team ] )
  {
    cgs.voteAlarmPlay[ team ] = qfalse;
    cgs.voteModified[ team ] = qfalse;
    trap_S_StartLocalSound( cgs.media.voteAlarmSound, CHAN_LOCAL_SOUND );
  }

  // play a talk beep whenever it is modified
  if( cgs.voteModified[ team ] )
  {
    cgs.voteModified[ team ] = qfalse;
    trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
  }

  sec = ( VOTE_TIME - ( cg.time - cgs.voteTime[ team ] ) ) / 1000;

  if( sec < 0 )
    sec = 0;


  Com_sprintf( yeskey, sizeof( yeskey ), "[%s]", 
    CG_KeyBinding( va( "%svote yes", team == TEAM_NONE ? "" : "team" ) ) );
  Com_sprintf( nokey, sizeof( nokey ), "[%s]", 
    CG_KeyBinding( va( "%svote no", team == TEAM_NONE ? "" : "team" ) ) );

  if( team != TEAM_NONE )
    offset = ( ( cgs.voteTime[ TEAM_NONE ] ? -65 : 0 ) - 
               ( Q_PrintStrlen( cgs.voteString[ VOTE_EXTRA_STRING ][ TEAM_NONE ] ) ? 20 : 0  ) );

  s = va( "^5[granger]%sVOTE: %s ^4|^5%s%s%i^5", team == TEAM_NONE ? "" : "^6TEAM^5",
          cgs.voteString[ VOTE_DESCRIPTION_STRING ][ team ],
          ( ( sec > 10 ) ? " " : "  " ), ( ( sec > 7 ) ? "^3" : "^1" ) , sec );

  UI_Text_Paint( 8, ( ( 335 + offset ) - ( Q_PrintStrlen( cgs.voteString[ VOTE_EXTRA_STRING ][ team ] ) ? 20 : 0  ) ),
                    0.42f, white, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED );

  if( Q_PrintStrlen( cgs.voteString[ VOTE_EXTRA_STRING ][ team ] ) )
  {
    s = va( "^5  \"%s\"", cgs.voteString[ VOTE_EXTRA_STRING ][ team ] );

    UI_Text_Paint( 8, 335 + offset, 0.3f, white, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED );

  }

  s = va( "^5  Called by: \"%s\"", cgs.voteCaller[ team ] );

  UI_Text_Paint( 8, 355 + offset, 0.3f, white, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED );

  if( cgs.voteActive[ team ] < 0 )
  {
    s = va( "^5  %i total vote%s(press ^2%s^5 to vote ^2yes^5, or press ^1%s^5 to vote ^1no^5)",
            cgs.voteCast[ team ], ( ( cgs.voteCast[ team ] == 1 ) ? " " : "s " ), yeskey, nokey );
  }
  else
  {
    s = va( "^5  %i total vote%sout of %i active player%s(press ^2%s^5 to vote ^2yes^5, or press ^1%s^5 to vote ^1no^5)",
            cgs.voteCast[ team ], ( ( cgs.voteCast[ team ] == 1 ) ? " " : "s " ),
            cgs.voteActive[ team ], ( ( cgs.voteActive[ team ] == 1 ) ? " " : "s " ), yeskey, nokey );
  }

  UI_Text_Paint( 8, 375 + offset, 0.3f, white, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED );
}


static qboolean CG_DrawScoreboard( void )
{
  static qboolean firstTime = qtrue;

  if( menuScoreboard )
    menuScoreboard->window.flags &= ~WINDOW_FORCED;

  if( cg_paused.integer )
  {
    cg.deferredPlayerLoading = 0;
    firstTime = qtrue;
    return qfalse;
  }

  if( !cg.showScores &&
      cg.predictedPlayerState.pm_type != PM_INTERMISSION )
  {
    cg.deferredPlayerLoading = 0;
    cg.killerName[ 0 ] = 0;
    firstTime = qtrue;
    return qfalse;
  }

  CG_RequestScores( );

  if( menuScoreboard == NULL )
    menuScoreboard = Menus_FindByName( "teamscore_menu" );

  if( menuScoreboard )
  {
    if( firstTime )
    {
      cg.spectatorTime = trap_Milliseconds();
      CG_SetScoreSelection( menuScoreboard );
      firstTime = qfalse;
    }

    Menu_Update( menuScoreboard );
    Menu_Paint( menuScoreboard, qtrue );
  }

  return qtrue;
}

/*
=================
CG_DrawIntermission
=================
*/
static void CG_DrawIntermission( void )
{
  menuDef_t *menu = Menus_FindByName( "default_hud" );

  Menu_Update( menu );
  Menu_Paint( menu, qtrue );

  cg.scoreFadeTime = cg.time;
  cg.scoreBoardShowing = CG_DrawScoreboard( );
}

/*
=================
CG_DrawQueue
=================
*/
static qboolean CG_DrawQueue( void )
{
  float       w;
  vec4_t      color;
  int         position;
  char        *ordinal, buffer[ MAX_STRING_CHARS ];

  if( !( cg.snap->ps.pm_flags & PMF_QUEUED ) )
    return qfalse;

  color[ 0 ] = 1;
  color[ 1 ] = 1;
  color[ 2 ] = 1;
  color[ 3 ] = 1;

  position = cg.snap->ps.persistant[ PERS_QUEUEPOS ] + 1;
  if( position < 1 )
    return qfalse;

  switch( position % 100 )
  {
    case 11:
    case 12:
    case 13:
      ordinal = "th";
      break;
    default:
      switch( position % 10 )
      {
        case 1:  ordinal = "st"; break;
        case 2:  ordinal = "nd"; break;
        case 3:  ordinal = "rd"; break;
        default: ordinal = "th"; break;
      }
      break;
  }

  Com_sprintf( buffer, MAX_STRING_CHARS, "You are %d%s in the spawn queue",
               position, ordinal );

  w = UI_Text_Width( buffer, 0.7f );
  UI_Text_Paint( 320 - w / 2, 360, 0.7f, color, buffer, 0, 0, ITEM_TEXTSTYLE_SHADOWED );

  if( cg.snap->ps.persistant[ PERS_SPAWNS ] == 0 )
    Com_sprintf( buffer, MAX_STRING_CHARS, "There are no spawns remaining" );
  else if( cg.snap->ps.persistant[ PERS_SPAWNS ] == 1 )
    Com_sprintf( buffer, MAX_STRING_CHARS, "There is 1 spawn remaining" );
  else
    Com_sprintf( buffer, MAX_STRING_CHARS, "There are %d spawns remaining",
                 cg.snap->ps.persistant[ PERS_SPAWNS ] );

  w = UI_Text_Width( buffer, 0.7f );
  UI_Text_Paint( 320 - w / 2, 400, 0.7f, color, buffer, 0, 0, ITEM_TEXTSTYLE_SHADOWED );

  return qtrue;
}


/*
=================
CG_Draw
=================
*/
static void CG_DrawCountdown(void) {
  int    sec = 0;
  int    w;
  int    h;
  float  size = 0.5f;
  char   text[MAX_STRING_CHARS] = "";

  if(!cg.countdownTime) {
    return;
  }

  sec = (cg.countdownTime - cg.time) / 1000;

  if(cgs.warmup) {
    Q_strncpyz(text, "^3Warmup is Ending:^7", sizeof(text));
  }
  else if(sec > 0) {
    Q_strncpyz(text, "Countdown to Battle:", sizeof(text));
  }

  if(sec < 0) {
    return;
  }

  w = UI_Text_Width(text, size);
  h = UI_Text_Height(text, size);
  UI_Text_Paint(320 - w / 2, 200, size, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);

  Com_sprintf(text, sizeof(text), "%s", (sec || cgs.warmup) ? va("%d", sec) : "DESTROY THE ENEMY!");    

  w = UI_Text_Width(text, size);
  UI_Text_Paint(320 - w / 2, 200 + 1.5f * h, size, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

//==================================================================================

/*
=================
CG_Draw2D
=================
*/
static void CG_Draw2D( void )
{
  menuDef_t *menu = NULL;

  // fading to black if stamina runs out
  // (only 2D that can't be disabled)
  CG_DrawLighting( );

  if( cg_draw2D.integer == 0 )
    return;

  if( cg.snap->ps.pm_type == PM_INTERMISSION )
  {
    CG_DrawIntermission( );
    return;
  }

  if( cg.snap->ps.persistant[ PERS_SPECSTATE ] == SPECTATOR_NOT &&
      cg.snap->ps.misc[ MISC_HEALTH ] > 0 )
  {
    menu = Menus_FindByName( BG_ClassConfig(
      cg.predictedPlayerState.stats[ STAT_CLASS ] )->hudName );

    CG_DrawBuildableStatus( );
  }

  if( !menu )
  {
    menu = Menus_FindByName( "default_hud" );

    if( !menu ) // still couldn't find it
      CG_Error( "Default HUD could not be found" );
  }

  Menu_Update( menu );
  Menu_Paint( menu, qtrue );

  CG_DrawVote( TEAM_NONE );
  CG_DrawVote( cg.predictedPlayerState.stats[ STAT_TEAM ] );
  CG_DrawCountdown( );
  CG_DrawQueue( );

  // don't draw center string if scoreboard is up
  cg.scoreBoardShowing = CG_DrawScoreboard( );

  if( !cg.scoreBoardShowing )
    CG_DrawCenterStrings( );
}

/*
===============
CG_InvincibleVision
===============
*/
static void CG_InvincibleVision( void )
{
  vec4_t    color = { 0.0f, 0.0f, 0.0f, 1.0f };
  qhandle_t shader;

  if( cg.renderingThirdPerson ||
      cg.snap->ps.persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT ||
      !cg_drawInvincibleVision.integer )
    return;

  if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
    shader = cgs.media.humanInvincibleViewShader;
  else if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
    shader = cgs.media.alienInvincibleViewShader;

  if( cg.predictedPlayerEntity.invincible )
  {
    if( cg.time - cg.predictedPlayerEntity.invincibleTime >= 500 )
      color[ 3 ] = 1.0f;
    else
    {
      color[ 3 ] = ( (float)( cg.time - cg.predictedPlayerEntity.invincibleTime ) ) / 500.0f;
    }
  } else if( cg.time - cg.predictedPlayerEntity.invincibleTime < 500 )
  {
    color[ 3 ] = ( (float)( 500 - ( cg.time - cg.predictedPlayerEntity.invincibleTime ) ) ) / 500.0f;
    
  } else
    return;

  trap_R_SetColor( color );
  CG_DrawPic( 0, 0, 640, 480, shader );
  trap_R_SetColor( NULL );
}

/*
===============
CG_ScalePainBlendTCs
===============
*/
static void CG_ScalePainBlendTCs( float* s1, float *t1, float *s2, float *t2 )
{
  *s1 -= 0.5f;
  *t1 -= 0.5f;
  *s2 -= 0.5f;
  *t2 -= 0.5f;

  *s1 *= cg_painBlendZoom.value;
  *t1 *= cg_painBlendZoom.value;
  *s2 *= cg_painBlendZoom.value;
  *t2 *= cg_painBlendZoom.value;

  *s1 += 0.5f;
  *t1 += 0.5f;
  *s2 += 0.5f;
  *t2 += 0.5f;
}

#define PAINBLEND_BORDER    0.15f

/*
===============
CG_PainBlend
===============
*/
static void CG_PainBlend( void )
{
  vec4_t      color;
  int         damage;
  float       damageAsFracOfMax;
  qhandle_t   shader = cgs.media.viewBloodShader;
  float       x, y, w, h;
  float       s1, t1, s2, t2;

  if( cg.snap->ps.persistant[ PERS_SPECSTATE ] != SPECTATOR_NOT || cg.intermissionStarted )
    return;

  damage = cg.lastHealth - cg.snap->ps.misc[ MISC_HEALTH ];

  if( damage < 0 )
    damage = 0;

  damageAsFracOfMax = (float)damage / BG_Class( cg.snap->ps.stats[ STAT_CLASS ] )->health;
  cg.lastHealth = cg.snap->ps.misc[ MISC_HEALTH ];

  cg.painBlendValue += damageAsFracOfMax * cg_painBlendScale.value;

  if( cg.painBlendValue > 0.0f )
  {
    cg.painBlendValue -= ( cg.frametime / 1000.0f ) *
      cg_painBlendDownRate.value;
  }

  if( cg.painBlendValue > 1.0f )
    cg.painBlendValue = 1.0f;
  else if( cg.painBlendValue <= 0.0f )
  {
    cg.painBlendValue = 0.0f;
    return;
  }

  if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
    VectorSet( color, 0.43f, 0.8f, 0.37f );
  else if( cg.snap->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
    VectorSet( color, 0.8f, 0.0f, 0.0f );

  if( cg.painBlendValue > cg.painBlendTarget )
  {
    cg.painBlendTarget += ( cg.frametime / 1000.0f ) *
      cg_painBlendUpRate.value;
  }
  else if( cg.painBlendValue < cg.painBlendTarget )
    cg.painBlendTarget = cg.painBlendValue;

  if( cg.painBlendTarget > cg_painBlendMax.value )
    cg.painBlendTarget = cg_painBlendMax.value;

  color[ 3 ] = cg.painBlendTarget;

  trap_R_SetColor( color );

  //left
  x = 0.0f; y = 0.0f;
  w = PAINBLEND_BORDER * 640.0f; h = 480.0f;
  CG_AdjustFrom640( &x, &y, &w, &h );
  s1 = 0.0f; t1 = 0.0f;
  s2 = PAINBLEND_BORDER; t2 = 1.0f;
  CG_ScalePainBlendTCs( &s1, &t1, &s2, &t2 );
  trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );

  //right
  x = 640.0f - ( PAINBLEND_BORDER * 640.0f ); y = 0.0f;
  w = PAINBLEND_BORDER * 640.0f; h = 480.0f;
  CG_AdjustFrom640( &x, &y, &w, &h );
  s1 = 1.0f - PAINBLEND_BORDER; t1 = 0.0f;
  s2 = 1.0f; t2 =1.0f;
  CG_ScalePainBlendTCs( &s1, &t1, &s2, &t2 );
  trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );

  //top
  x = PAINBLEND_BORDER * 640.0f; y = 0.0f;
  w = 640.0f - ( 2 * PAINBLEND_BORDER * 640.0f ); h = PAINBLEND_BORDER * 480.0f;
  CG_AdjustFrom640( &x, &y, &w, &h );
  s1 = PAINBLEND_BORDER; t1 = 0.0f;
  s2 = 1.0f - PAINBLEND_BORDER; t2 = PAINBLEND_BORDER;
  CG_ScalePainBlendTCs( &s1, &t1, &s2, &t2 );
  trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );

  //bottom
  x = PAINBLEND_BORDER * 640.0f; y = 480.0f - ( PAINBLEND_BORDER * 480.0f );
  w = 640.0f - ( 2 * PAINBLEND_BORDER * 640.0f ); h = PAINBLEND_BORDER * 480.0f;
  CG_AdjustFrom640( &x, &y, &w, &h );
  s1 = PAINBLEND_BORDER; t1 = 1.0f - PAINBLEND_BORDER;
  s2 = 1.0f - PAINBLEND_BORDER; t2 = 1.0f;
  CG_ScalePainBlendTCs( &s1, &t1, &s2, &t2 );
  trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );

  trap_R_SetColor( NULL );
}

/*
=====================
CG_ResetPainBlend
=====================
*/
void CG_ResetPainBlend( void )
{
  cg.painBlendValue = 0.0f;
  cg.painBlendTarget = 0.0f;
  cg.lastHealth = cg.snap->ps.misc[ MISC_HEALTH ];
}

/*
================
CG_DrawBinaryShadersFinalPhases
================
*/
static void CG_DrawBinaryShadersFinalPhases( void )
{
  float ss, f, l, u;
  polyVert_t verts[ 4 ] = {
    { { 0, 0, 0 }, { 0, 0 }, { 255, 255, 255, 255 } },
    { { 0, 0, 0 }, { 1, 0 }, { 255, 255, 255, 255 } },
    { { 0, 0, 0 }, { 1, 1 }, { 255, 255, 255, 255 } },
    { { 0, 0, 0 }, { 0, 1 }, { 255, 255, 255, 255 } }
  };
  int i, j, k;

  if( !cg.numBinaryShadersUsed )
    return;

  ss = cg_binaryShaderScreenScale.value;
  if( ss <= 0.0f )
  {
    cg.numBinaryShadersUsed = 0;
    return;
  }
  else if( ss > 1.0f )
    ss = 1.0f;

  ss = sqrt( ss );

  f = 1.01f; // FIXME: is this a good choice to avoid near-clipping?
  l = f * tan( DEG2RAD( cg.refdef.fov_x / 2 ) ) * ss;
  u = f * tan( DEG2RAD( cg.refdef.fov_y / 2 ) ) * ss;

  VectorMA( cg.refdef.vieworg, f, cg.refdef.viewaxis[ 0 ], verts[ 0 ].xyz );
  VectorMA( verts[ 0 ].xyz, l, cg.refdef.viewaxis[ 1 ], verts[ 0 ].xyz );
  VectorMA( verts[ 0 ].xyz, u, cg.refdef.viewaxis[ 2 ], verts[ 0 ].xyz );
  VectorMA( verts[ 0 ].xyz, -2*l, cg.refdef.viewaxis[ 1 ], verts[ 1 ].xyz );
  VectorMA( verts[ 1 ].xyz, -2*u, cg.refdef.viewaxis[ 2 ], verts[ 2 ].xyz );
  VectorMA( verts[ 0 ].xyz, -2*u, cg.refdef.viewaxis[ 2 ], verts[ 3 ].xyz );

  trap_R_AddPolyToScene( cgs.media.binaryAlpha1Shader, 4, verts );

  for( i = 0; i < cg.numBinaryShadersUsed; ++i )
  {
    for( j = 0; j < 4; ++j )
    {
      for( k = 0; k < 3; ++k )
        verts[ j ].modulate[ k ] = cg.binaryShaderSettings[ i ].color[ k ];
    }

    if( cg.binaryShaderSettings[ i ].drawFrontline )
      trap_R_AddPolyToScene( cgs.media.binaryShaders[ i ].f3, 4, verts );
    if( cg.binaryShaderSettings[ i ].drawIntersection )
      trap_R_AddPolyToScene( cgs.media.binaryShaders[ i ].b3, 4, verts );
  }

  cg.numBinaryShadersUsed = 0;
}

/*
=====================
CG_DrawActive

Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive( stereoFrame_t stereoView )
{
  float   separation;
  vec3_t    baseOrg;

  // optionally draw the info screen instead
  if( !cg.snap )
    return;

  switch ( stereoView )
  {
    case STEREO_CENTER:
      separation = 0;
      break;
    case STEREO_LEFT:
      separation = -cg_stereoSeparation.value / 2;
      break;
    case STEREO_RIGHT:
      separation = cg_stereoSeparation.value / 2;
      break;
    default:
      separation = 0;
      CG_Error( "CG_DrawActive: Undefined stereoView" );
  }

  // clear around the rendered view if sized down
  CG_TileClear( );

  // offset vieworg appropriately if we're doing stereo separation
  VectorCopy( cg.refdef.vieworg, baseOrg );

  if( separation != 0 )
    VectorMA( cg.refdef.vieworg, -separation, cg.refdef.viewaxis[ 1 ],
              cg.refdef.vieworg );

  CG_DrawBinaryShadersFinalPhases( );

  //emit lantern light
  if( cg_lanternLight.integer )
  {
    classConfig_t *classConfig;
    vec3_t        lanternOrigin;
    vec3_t        lanternAngles;
    vec3_t        forward, right, up;

    if( cg_lanternLightPitch.value ||
        cg_lanternLightYaw.value )
    {
      lanternAngles[ PITCH ] = cg_lanternLightPitch.value;
      lanternAngles[ YAW ] = cg_lanternLightYaw.value;
      lanternAngles[ ROLL ] = 0;

      AnglesSubtract( lanternAngles, cg.refdefViewAngles,
                      lanternAngles );
    } else
    {
      VectorCopy( cg.refdefViewAngles, lanternAngles );
    }

    AngleVectors( lanternAngles, forward, right, up );

    classConfig = BG_ClassConfig( cg.snap->ps.stats[ STAT_CLASS ] );
    VectorMA( cg.refdef.vieworg, classConfig->shoulderOffsets[ 0 ], forward, lanternOrigin );
    VectorMA( cg.refdef.vieworg, classConfig->shoulderOffsets[ 1 ], right, lanternOrigin );
    VectorMA( cg.refdef.vieworg, classConfig->shoulderOffsets[ 2 ], up, lanternOrigin );

    VectorMA( lanternOrigin, -( cg_lanternLightRange.value ), forward, lanternOrigin );

    //prevent the lantern light from clipping through solid objects
    if( cg_lanternLightClip.integer )
    {
      trace_t trace;
      const   vec3_t mins = { -10, -10, -10 };
      const   vec3_t maxs = { 10, 10, 10 };

      CG_CapTrace( &trace, cg.refdef.vieworg, mins, maxs,
                   lanternOrigin,
                   cg.predictedPlayerState.clientNum,
                   MASK_DEADSOLID );

      if( trace.fraction != 1.0f )
      {
        //slide the lantern light up the wall
        VectorMA( trace.endpos,
                  sqrt( 1 - ( trace.fraction * trace.fraction ) ) * cg_lanternLightRange.value,
                  up, lanternOrigin);

        // Try another trace to this position, because a tunnel may have the ceiling
        // close enogh that this is poking out.
        CG_CapTrace( &trace, cg.refdef.vieworg, mins, maxs,
                     lanternOrigin,
                     cg.predictedPlayerState.clientNum,
                     MASK_DEADSOLID );
        VectorCopy( trace.endpos, lanternOrigin );
      }
    }

    trap_R_AddLightToScene( lanternOrigin,
                            cg_lanternLightIntensity.value,
                            1.0f, 1.0f, 1.0f );
  }

  // draw 3D view
  trap_R_RenderScene( &cg.refdef );

  // restore original viewpoint if running stereo
  if( separation != 0 )
    VectorCopy( baseOrg, cg.refdef.vieworg );

  //for invincible players to know that they are invincible
  CG_InvincibleVision( );

  CG_HelmetVision();

  // first person blend blobs, done after AnglesToAxis
  if( !cg.renderingThirdPerson )
    CG_PainBlend( );

  // draw status bar and other floating elements
  CG_Draw2D( );
}
