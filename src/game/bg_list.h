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
 typedef void (*BG_Func) ( void *data, void *user_data);
 
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
void         BG_InitListMemory( char *calledFile, int calledLine );
void         BG_MemoryInfoForLinkedLists( void );
bglist_t*    bg_list_alloc( void );
void         bg_list_free( bglist_t *list );
void         bg_list_free_1( bglist_t *list );
#define      bg_list_free1    bg_list_free_1
void         bg_list_free_full( bglist_t *list, BG_DestroyNotify free_func );
bglist_t*    bg_list_append( bglist_t *list, void *data );
bglist_t*    bg_list_prepend( bglist_t *list, void *data );
bglist_t*    bg_list_insert( bglist_t *list, void *data, int position );
bglist_t*    bg_list_insert_sorted( bglist_t *list, void *data,
                                    BG_CompareFunc func );
bglist_t*    bg_list_insert_sorted_with_data( bglist_t *list, void *data,
					                                    BG_CompareDataFunc  func,
					                                    void *user_data );
bglist_t*    bg_list_insert_before( bglist_t *list, bglist_t *sibling,
                                    void *data );
bglist_t*    bg_list_concat( bglist_t *list1, bglist_t *list2 );
bglist_t*    bg_list_remove( bglist_t *list, const void *data );
bglist_t*    bg_list_remove_all( bglist_t *list, const void *data );
bglist_t*    bg_list_remove_link( bglist_t *list, bglist_t *llink );
bglist_t*    bg_list_delete_link( bglist_t *list, bglist_t *link_ );
bglist_t*    bg_list_reverse( bglist_t *list );
bglist_t*    bg_list_copy( bglist_t *list );

bglist_t*    bg_list_copy_deep( bglist_t *list, BG_CopyFunc func,
                                void *user_data );

bglist_t*    bg_list_nth( bglist_t *list, unsigned int n );
bglist_t*    bg_list_nth_prev ( bglist_t *list, unsigned int n );
bglist_t*    bg_list_find( bglist_t *list, const void *data );
bglist_t*    bg_list_find_custom( bglist_t *list, const void *data,
					                        BG_CompareFunc func );
int          bg_list_position( bglist_t *list, bglist_t *llink );
int          bg_list_index( bglist_t *list, const void *data );
bglist_t*    bg_list_last( bglist_t *list );
bglist_t*    bg_list_first( bglist_t *list );
unsigned int bg_list_length( bglist_t *list );
void         bg_list_foreach( bglist_t *list, BG_Func func, void *user_data );
bglist_t*    bg_list_sort( bglist_t *list, BG_CompareFunc      compare_func );
bglist_t*    bg_list_sort_with_data( bglist_t *list,
					                          BG_CompareDataFunc compare_func,
					                          void *user_data ) ;
void         *bg_list_nth_data( bglist_t *list, unsigned int n );


#define g_list_previous(list)	((list) ? (((bglist_t *)(list))->prev) : NULL)
#define g_list_next(list)	    ((list) ? (((bglist_t *)(list))->next) : NULL)
