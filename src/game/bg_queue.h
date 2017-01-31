/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __G_QUEUE_H__
#define __G_QUEUE_H__

#include "bg_list.h"
#include "../qcommon/q_shared.h"

typedef struct bgqueue_s bgqueue_t;

/**
 * bgqueue_t:
 * @head: a pointer to the first element of the queue
 * @tail: a pointer to the last element of the queue
 * @length: the number of elements in the queue
 *
 * Contains the public fields of a
 * [Queue][glib-Double-ended-Queues].
 */
struct bgqueue_s
{
  bglist_t *head;
  bglist_t *tail;
  size_t  length;
};

/**
 * BG_QUEUE_INIT:
 *
 * A statically-allocated #bgqueue_t must be initialized with this
 * macro before it can be used. This macro can be used to initialize
 * a variable, but it cannot be assigned to a variable. In that case
 * you have to use BG_Queue_Init().
 *
 * |[
 * bgqueue_t my_queue = BG_QUEUE_INIT;
 * ]|
 *
 * Since: 2.14
 */
#define BG_QUEUE_INIT { NULL, NULL, 0 }

/* Queues
 */

void        BG_Queue_Init_Memory( char *calledFile, int calledLine );
void        BG_Queue_Memory_Info( void );
bgqueue_t   *BG_Queue_New( void );
void        BG_Queue_Free( bgqueue_t *queue );
void        BG_Queue_Free_Full( bgqueue_t *queue, BG_DestroyNotify free_func );

void        BG_Queue_Init( bgqueue_t *queue );
void        BG_Queue_Clear( bgqueue_t *queue ) ;
void        BG_Queue_Clear_Full( bgqueue_t *queue, BG_DestroyNotify free_func );

qboolean    BG_Queue_Is_Empty( bgqueue_t *queue );
size_t      BG_Queue_Get_Length( bgqueue_t *queue );

void        BG_Queue_Reverse( bgqueue_t *queue );
bgqueue_t   *BG_Queue_Copy( bgqueue_t *queue );
void        BG_Queue_Foreach( bgqueue_t *queue, BG_Func func, void *user_data );

bglist_t    *BG_Queue_Find( bgqueue_t *queue, const void *data);
bglist_t    *BG_Queue_Find_Custom ( bgqueue_t *queue, const void *data,
                                   BG_CompareFunc func );

void        BG_Queue_Sort ( bgqueue_t *queue, BG_CompareDataFunc compare_func,
                            void *user_data );

void        BG_Queue_Push_Head( bgqueue_t *queue, void *data);
void        BG_Queue_Push_Tail(bgqueue_t *queue, void *data );
void        BG_Queue_Push_nth( bgqueue_t *queue, void *data, int n );

void        *BG_Queue_Pop_Head( bgqueue_t *queue );
void        *BG_Queue_Pop_Tail( bgqueue_t *queue );
void        *BG_Queue_Pop_nth( bgqueue_t *queue, size_t n );

void        *BG_Queue_Peek_Head( bgqueue_t *queue );
void        *BG_Queue_Peek_Tail( bgqueue_t *queue );
void        *BG_Queue_Peek_nth( bgqueue_t *queue, size_t n );

int         BG_Queue_Index( bgqueue_t *queue, const void *data );

qboolean    BG_Queue_Remove( bgqueue_t *queue, const void *data );
size_t      BG_Queue_Remove_All( bgqueue_t *queue, const void *data );

void        BG_Queue_Insert_Before( bgqueue_t *queue, bglist_t *sibling,
                                   void *data );
void        BG_Queue_Insert_After( bgqueue_t *queue, bglist_t *sibling,
                                  void *data );
void        BG_Queue_Insert_Sorted( bgqueue_t *queue, void *data,
                                   BG_CompareDataFunc func, void *user_data );

void        BG_Queue_Push_Head_Link( bgqueue_t *queue, bglist_t *link_ );
void        BG_Queue_Push_Tail_Link( bgqueue_t *queue, bglist_t *link_ );
void        BG_Queue_Push_nth_Link( bgqueue_t *queue, int n, bglist_t *link_ );

bglist_t    *BG_Queue_Pop_Head_Link( bgqueue_t *queue );
bglist_t    *BG_Queue_Pop_Tail_link( bgqueue_t *queue );
bglist_t    *BG_Queue_Pop_nth_Link( bgqueue_t *queue, size_t n );

bglist_t    *BG_Queue_Peek_Head_Link( bgqueue_t *queue );
bglist_t    *BG_Queue_Peek_Tail_Link( bgqueue_t *queue );
bglist_t    *BG_Queue_Peek_nth_Link( bgqueue_t *queue, size_t n );

int         BG_Queue_Link_Index( bgqueue_t *queue, bglist_t *link_ );
void        BG_Queue_Unlink( bgqueue_t *queue, bglist_t *link_ );
void        BG_Queue_Delete_Link( bgqueue_t *queue, bglist_t *link_ );

#endif /* __G_QUEUE_H__ */
