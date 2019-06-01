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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

 #ifndef __BG_LIST_H__
 #define __BG_LIST_H__

#include "../qcommon/q_shared.h"

typedef struct bglink_s bglink_t;
typedef struct bglist_s bglist_t;

/**
 * bglink_t:
 * @list: points to the list the link is a member of. If the link
 *        is not a member of any list, this pointer is set to NULL.
 * @data: holds the element's data, which can be a pointer to any kind
 *        of data, or any integer value using the
 *        [Type Conversion Macros][glib-Type-Conversion-Macros]
 * @next: contains the link to the next element in the list
 * @prev: contains the link to the previous element in the list
 *
 * The #bglink_t struct is used for each element in a doubly-linked list.
 **/
struct bglink_s
{
  bglist_t *list;
  void     *data;
  bglink_t *next;
  bglink_t *prev;
};

/**
 * bglist_t:
 * @head: a pointer to the first element of the list
 * @tail: a pointer to the last element of the list
 * @length: the number of elements in the list
 *
 * Contains the public fields of a
 * Doubly-linked list.
 */
struct bglist_s
{
  bglink_t *head;
  bglink_t *tail;
  size_t   length;
};

/* Generic Function Pointer Types
 */
 typedef int  (*BG_CompareFunc) ( const void *a, const void *b );
 typedef int  (*BG_CompareDataFunc) ( const void *a, const void *b,
                                      const void *user_data );
 typedef void (*BG_DestroyNotify) ( void *data );
 typedef void (*BG_Func) ( void *data, void *user_data );
 
 /**
 * BG_CopyFunc:
 * @src: (not nullable): A pointer to the data which should be copied
 * @data: Additional data
 *
 * A function of this signature is used to copy the node data 
 * when doing a deep-copy of a tree.
 *
 * Returns: (not nullable): A pointer to the copy
 *
 * Since: 2.4
 */
typedef void *(*BG_CopyFunc) ( const void *src, void *data );

/**
 * BG_List_INIT:
 *
 * A statically-allocated #bglist_t must be initialized with this
 * macro before it can be used. This macro can be used to initialize
 * a variable, but it cannot be assigned to a variable. In that case
 * you have to use BG_List_Init().
 *
 * |[
 * bglist_t my_list = BG_List_INIT;
 * ]|
 *
 * Since: 2.14
 */
#define BG_LIST_INIT { NULL, NULL, 0 }

/* Linked lists
 */

void        BG_List_Init_Memory(char *calledFile, int calledLine);
void        BG_List_Memory_Info(void);
bglist_t    *BG_List_New(void);
void        BG_List_Free(bglist_t *list);
void        BG_List_Free_Full(bglist_t *list, BG_DestroyNotify free_func);

qboolean    BG_List_Valid(bglist_t *list);

void        BG_List_Init(bglist_t *list);
void        BG_List_Clear(bglist_t *list) ;
void        BG_List_Clear_Full(bglist_t *list, BG_DestroyNotify free_func);

qboolean    BG_List_Is_Empty(bglist_t *list);
size_t      BG_List_Get_Length(bglist_t *list);

void        BG_List_Reverse(bglist_t *list);
bglist_t    *BG_List_Copy(bglist_t *list);
bglist_t    *BG_List_Copy_Deep(
  bglist_t *list, BG_CopyFunc  func, void *user_data);
void        BG_List_Foreach(
  bglist_t *list, bglink_t *startLink, BG_Func func, void *user_data );

bglink_t    *BG_List_Find(bglist_t *list, const void *data);
bglink_t    *BG_List_Find_Custom (
  bglist_t *list, const void *data, BG_CompareFunc func);

void        BG_List_Sort(
  bglist_t *list, BG_CompareDataFunc compare_func, void *user_data);

bglink_t    *BG_List_Push_Head(bglist_t *list, void *data);
bglink_t    *BG_List_Push_Tail(bglist_t *list, void *data);
bglink_t    *BG_List_Push_nth(bglist_t *list, void *data, int n);

void        *BG_List_Pop_Head(bglist_t *list);
void        *BG_List_Pop_Tail(bglist_t *list);
void        *BG_List_Pop_nth(bglist_t *list, size_t n);

void        *BG_List_Peek_Head(bglist_t *list);
void        *BG_List_Peek_Tail(bglist_t *list);
void        *BG_List_Peek_nth(bglist_t *list, size_t n);

int         BG_List_Index(bglist_t *list, const void *data);

void        BG_List_Concat(bglist_t *list1, bglist_t *list2);

qboolean    BG_List_Remove(bglist_t *list, const void *data);
size_t      BG_List_Remove_All(bglist_t *list, const void *data);

void        BG_List_Insert_Before(
  bglist_t *list, bglink_t *sibling, void *data);
void        BG_List_Insert_After(
  bglist_t *list, bglink_t *sibling, void *data);
void        BG_List_Insert_Sorted(
  bglist_t *list, void *data,  BG_CompareDataFunc func, void *user_data);

void        BG_List_Push_Head_Link(bglist_t *list, bglink_t *link);
void        BG_List_Push_Tail_Link(bglist_t *list, bglink_t *link);
void        BG_List_Push_nth_Link(bglist_t *list, int n, bglink_t *link);

bglink_t    *BG_List_Pop_Head_Link(bglist_t *list);
bglink_t    *BG_List_Pop_Tail_link(bglist_t *list);
bglink_t    *BG_List_Pop_nth_Link(bglist_t *list, size_t n);

bglink_t    *BG_List_Peek_Head_Link(bglist_t *list);
bglink_t    *BG_List_Peek_Tail_Link(bglist_t *list);
bglink_t    *BG_List_Peek_nth_Link(bglist_t *list, size_t n);

int         BG_List_Link_Index(bglist_t *list, bglink_t *link);
void        BG_List_Unlink(bglink_t *link);
void        BG_List_Delete_Link(bglink_t *link);

/* Doubly linked list links
 */
void             BG_Link_Init_Memory(char *calledFile, int calledLine);
void             BG_Link_Memory_Info(void);
bglink_t*        BG_Link_Alloc(void);

void             BG_Link_Foreach(bglink_t *list, BG_Func func, void *user_data);
bglink_t*        BG_Link_Delete_Link(bglink_t *list, bglink_t *link_);
bglink_t*        BG_Link_Prepend(bglink_t *head, void *data);
bglink_t*        BG_Link_First(bglink_t *link );
bglink_t*        BG_Link_nth_Prev(bglink_t *link, unsigned int n);

#define BG_Link_Previous(link) ((link) ? (((bglink_t *)(link))->prev) : NULL)
#define BG_Link_Next(link)	   ((link) ? (((bglink_t *)(link))->next) : NULL)
#define BG_Link_Head(link)     ((link) ? (((((bglink_t *)(link))->list) ? (((((bglink_t *)(link))->list)->head) : NULL ) : NULL)
#define BG_Link_Tail(link)     ((link) ? (((((bglink_t *)(link))->list) ? (((((bglink_t *)(link))->list)->tail) : NULL ) : NULL)

#endif /* __BG_LIST_H__ */
