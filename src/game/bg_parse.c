/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2021 GrangerHub

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

typedef struct parser_token_data_s
{
  const char        *token;
  void              *variable;
  BG_FuncWithReturn parser_func;
  qboolean          require_definition;
  qboolean          errored;
} parser_token_data_t;

typedef struct parse_data_s
{
  char       **text_p;
  const char *current_token;
} parse_data_t;

/*
--------------------------------------------------------------------------------
Custom Dynamic Memory Management for parse data
*/
#ifdef GAME
# define  MAX_PARSER_TOKEN_DATA 4096
#else
#ifdef CGAME
# define  MAX_PARSER_TOKEN_DATA 4096
#else
# define  MAX_PARSER_TOKEN_DATA 4096
#endif
#endif

allocator_protos(parser_token_data)
allocator(parser_token_data, sizeof(parser_token_data_t), MAX_PARSER_TOKEN_DATA)

/*
===============
BG_Parser_Token_Data_Init_Memory

Executed in BG_InitMemory()
===============
*/
void BG_Parser_Token_Data_Init_Memory(char *calledFile, int calledLine) {
  initPool_parser_token_data(calledFile, calledLine);
}

/*
===============
BG_Parser_Token_Data_Alloc

Returns an allocated parser_token_data
===============
*/
static ID_INLINE parser_token_data_t *BG_Parser_Token_Data_Alloc(void) {
  return alloc_parser_token_data(__FILE__, __LINE__);
}

/*
===============
BG_Parser_Token_Data_Memory_Info

Displays info related to the allocation of parser token data
===============
*/
void BG_Parser_Token_Data_Memory_Info(void) {
  memoryInfo_parser_token_data( );
}

/*
===============
BG_Parser_Token_Data_Free

Frees the passed parser token data
===
*/
static void BG_Parser_Token_Data_Free (void *parser_token_data) {
  Com_Assert(parser_token_data != NULL);

  free_parser_token_data(parser_token_data, __FILE__, __LINE__);
}

/*
--------------------------------------------------------------------------------
*/

/*
===============
BG_Parse_Add_Parser_Token

Adds parser data to the given parser token list
===
*/
void BG_Parse_Add_Parser_Token(
  bglist_t *parser_token_list, const char *token, void *variable,
  BG_FuncWithReturn parser_func,
  qboolean require_definition) {
    parser_token_data_t *parser_token_data;

    Com_Assert(token != NULL);
    Com_Assert(parser_func != NULL);

    parser_token_data = BG_Parser_Token_Data_Alloc( );
    Com_Assert(parser_token_data != NULL);

    parser_token_data->token = token;
    parser_token_data->variable = variable;
    parser_token_data->parser_func = parser_func;
    parser_token_data->require_definition = require_definition;
    parser_token_data->errored = qfalse;

    BG_List_Push_Tail(parser_token_list, parser_token_data);
}

/*
===============
BG_Parse_Check_Parser_Token

Executes the provided parser function and returns qtrue if the token is a match.
===
*/
qboolean BG_Parse_Check_Parser_Token(void *data, void *user_data) {
  parser_token_data_t *parser_token_data = (parser_token_data_t *)data;
  parse_data_t *parse_data = (parse_data_t *)(user_data);

  Com_Assert(parser_token_data != NULL);
  Com_Assert(parse_data != NULL);

  if(!Q_stricmp(parse_data->current_token, parser_token_data->token)) {
    if(!parser_token_data->parser_func(data, user_data)) {
      parser_token_data->errored = qtrue;
    }
    return qtrue;
  }

  return qfalse;
}

/*
===============
BG_Parse_Undefined_Required_Parser_Token

Returns qtrue if the token is required and undefined
===
*/
qboolean BG_Parse_Undefined_Required_Parser_Token(void *data, void *user_data) {
  parser_token_data_t *parser_token_data = (parser_token_data_t *)data;

  Com_Assert(parser_token_data != NULL);

  if(parser_token_data->require_definition) {
    return qtrue;
  }

  return qfalse;
}

/*
===============
BG_Parse_Scan_Parser_Tokens

Itterates through the parser token list, if a token matches then the
corresponding parser function is executed and then the check ends.
Returns qfalse if an error occurs
===
*/
qboolean BG_Parse_Scan_Parser_Tokens(
  bglist_t     *parser_list, char **text_p, const char *substruct) {
  bglist_t     defined_parser_list = BG_LIST_INIT;
  bglink_t     *link = NULL;
  parse_data_t parse_data;
  qboolean     substruct_open = qfalse;

  Com_Assert(parser_list != NULL);
  Com_Assert(text_p != NULL);

  parse_data.text_p = text_p;

  while(1) {
    parse_data.current_token = COM_Parse(text_p);

    if(!parse_data.current_token || !Q_stricmp(parse_data.current_token, "")) {
      Com_Printf(S_COLOR_RED "ERROR: file ended early\n");
      BG_List_Clear_Full_Forced(parser_list, BG_Parser_Token_Data_Free);
      BG_List_Clear_Full_Forced(&defined_parser_list, BG_Parser_Token_Data_Free);
      return qfalse;
    }

    if(!Q_stricmp(parse_data.current_token, "{")) {
      if(!substruct || !Q_stricmp(substruct, "")) {
        Com_Printf(S_COLOR_RED "ERROR: Substruct parsing without a declaration\n" );
        BG_List_Clear_Full_Forced(parser_list, BG_Parser_Token_Data_Free);
        BG_List_Clear_Full_Forced(&defined_parser_list, BG_Parser_Token_Data_Free);
        return qfalse;
      }
    } else if((substruct && Q_stricmp(substruct, ""))) {
      if(substruct_open) {
        if(!Q_stricmp(parse_data.current_token, "}")) {

          if(!BG_List_Is_Empty(parser_list)) {
            link =
              BG_List_Foreach_With_Break(
                parser_list, NULL, BG_Parse_Undefined_Required_Parser_Token,
                NULL);
            if(link != NULL) {
              parser_token_data_t *parser_token_data =
                (parser_token_data_t *)link->data;

              Com_Assert(parser_token_data != NULL);
              
              Com_Printf(
                S_COLOR_RED "ERROR: '%s' is required to be defined\n",
                parser_token_data->token);
              BG_List_Clear_Full_Forced(parser_list, BG_Parser_Token_Data_Free);
              BG_List_Clear_Full_Forced(&defined_parser_list, BG_Parser_Token_Data_Free);
              return qfalse;
            }
          }

          BG_List_Clear_Full_Forced(parser_list, BG_Parser_Token_Data_Free);
          BG_List_Clear_Full_Forced(&defined_parser_list, BG_Parser_Token_Data_Free);
          return qtrue;
        }
      } else {
        Com_Printf(S_COLOR_RED "ERROR: '%s' is a substruct and requires braces\n" );
        BG_List_Clear_Full_Forced(parser_list, BG_Parser_Token_Data_Free);
        BG_List_Clear_Full_Forced(&defined_parser_list, BG_Parser_Token_Data_Free);
        return qfalse;
      }
    }

  }
}
