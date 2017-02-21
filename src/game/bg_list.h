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

typedef struct bglist_s bglist_t;

struct bglist_s
{
  void     *data;
  bglist_t *next;
  bglist_t *prev;
};

/* Generic Function Pointer Types
 */
 typedef int  (*BG_CompareFunc) ( const void *a, const void *b );
 typedef int  (*BG_CompareDataFunc) ( const void *a, const void *b,
                                      void *user_data );
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

/* Doubly linked lists
 */
void         BG_List_Init_Memory( char *calledFile, int calledLine );
void         BG_List_Memory_Info( void );
bglist_t*    BG_List_Alloc( void );
void         BG_List_Free( bglist_t *list );
void         BG_List_Free_1( bglist_t *list );
#define      BG_List_Free1    BG_List_Free_1
void         BG_List_Free_Full( bglist_t *list,
                                BG_DestroyNotify free_func );

bglist_t*    BG_List_Append( bglist_t *list, void *data );
bglist_t*    BG_List_Prepend( bglist_t *list, void *data );
bglist_t*    BG_List_Insert( bglist_t *list, void *data, int position );
bglist_t*    BG_List_Insert_Sorted( bglist_t *list, void *data,
                                    BG_CompareFunc func );
bglist_t*    BG_List_Insert_Sorted_With_Data( bglist_t *list, void *data,
					                                    BG_CompareDataFunc  func,
					                                    void *user_data );
bglist_t*    BG_List_Insert_Before( bglist_t *list, bglist_t *sibling,
                                    void *data );

bglist_t*    BG_List_Concat( bglist_t *list1, bglist_t *list2 );
bglist_t*    BG_List_Remove( bglist_t *list, const void *data );
bglist_t*    BG_List_Remove_All( bglist_t *list, const void *data );
bglist_t*    BG_List_Remove_Link( bglist_t *list, bglist_t *llink );
bglist_t*    BG_List_Delete_Link( bglist_t *list, bglist_t *link_ );

bglist_t*    BG_List_Reverse( bglist_t *list );
bglist_t*    BG_List_Copy( bglist_t *list );
bglist_t*    BG_List_Copy_Deep( bglist_t *list, BG_CopyFunc func,
                                void *user_data );

bglist_t*    BG_List_nth( bglist_t *list, unsigned int n );
bglist_t*    BG_List_nth_Prev ( bglist_t *list, unsigned int n );
bglist_t*    BG_List_Find( bglist_t *list, const void *data );
bglist_t*    BG_List_Find_Custom( bglist_t *list, const void *data,
					                        BG_CompareFunc func );
int          BG_List_Position( bglist_t *list, bglist_t *llink );
int          BG_List_Index( bglist_t *list, const void *data );
bglist_t*    BG_List_Last( bglist_t *list );
bglist_t*    BG_List_First( bglist_t *list );

unsigned int BG_List_Length( bglist_t *list );

void         BG_List_Foreach( bglist_t *list, BG_Func func,
                              void *user_data );

bglist_t*    BG_List_Sort( bglist_t *list, BG_CompareFunc compare_func );
bglist_t*    BG_List_Sort_With_Data( bglist_t *list,
					                           BG_CompareDataFunc compare_func,
					                           void *user_data ) ;

void         *BG_List_nth_Data( bglist_t *list, unsigned int n );


#define BG_List_Previous(list) ((list) ? (((bglist_t *)(list))->prev) : NULL)
#define BG_List_Next(list)	   ((list) ? (((bglist_t *)(list))->next) : NULL)

#endif /* __BG_LIST_H__ */
