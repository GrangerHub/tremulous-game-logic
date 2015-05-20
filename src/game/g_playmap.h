/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015 GrangerHub

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/


/* MAP POOL */

#define MAX_PLAYMAP_POOL_ENTRIES 256

// map pool for the playmap system
typedef struct playMapPool_s
{
  char *mapname[ MAX_PLAYMAP_POOL_ENTRIES ];

  int  numMaps;
} playMapPool_t;


/* PLAYMAP FLAGS */

// available flags for playmap
typedef enum
{
  PLAYMAP_FLAG_DPUNT,
  PLAYMAP_FLAG_FF,
  PLAYMAP_FLAG_FBF,
  PLAYMAP_FLAG_SD,
  PLAYMAP_FLAG_LGRAV,
  PLAYMAP_FLAG_UBP,
  PLAYMAP_NUM_FLAGS
} playMapFlag_t;


/* PLAYMAP QUEUE */

// individual playmap entry in the queue
typedef struct playMap_s
{
  char *mapname;
  char *layout;

  gclient_t *client;

  playMapFlag_t *plusFlags;
  playMapFlag_t *minusFlags;
} playMap_t;

// playmap queue/playlist
typedef struct playMapQueue_s
{
  playMap_t playMap[ MAX_PLAYMAP_POOL_ENTRIES ];

  int front, back;
} playMapQueue_t;


/* PLAYMAP ERRORS */

// error codes
typedef enum playMapErrorCode_s
{
  // this item must always be the first in the enum list
  PLAYMAP_ERROR_NONE,

  // list of error codes (order does not matter)
  PLAYMAP_ERROR_MAP_ALREADY_IN_POOL,
  PLAYMAP_ERROR_MAP_NOT_FOUND,
  PLAYMAP_ERROR_LAYOUT_NOT_FOUND,
  PLAYMAP_ERROR_MAP_NOT_IN_POOL,
  PLAYMAP_ERROR_MAP_NOT_IN_QUEUE,
  PLAYMAP_ERROR_MAP_ALREADY_IN_QUEUE,
  PLAYMAP_ERROR_USER_ALREADY_IN_QUEUE,

  // an unknown error
  PLAYMAP_ERROR_UNKNOWN,

  // this item must always be the last in the enum list
  PLAYMAP_NUM_ERRORS,
} playMapErrorCode_t;

// error messages
typedef char *playMapErrorMessage_t;

// error messages
typedef struct playMapError_s
{
  int errorCode;
  playMapErrorMessage_t errorMessage;
} playMapError_t;
