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

/*
 * MT safe
 */
#include "../qcommon/q_shared.h"
#include "bg_public.h"

/**
 * SECTION:linked_lists_double
 * @title: Doubly-Linked Lists
 * @short_description: linked lists that can be iterated over in both directions
 *
 * The #bglist_t structure and its associated functions provide a standard
 * doubly-linked list data structure.
 *
 * Each element in the list contains a piece of data, together with
 * pointers which link to the previous and next elements in the list.
 * Using these pointers it is possible to move through the list in both
 * directions (unlike the singly-linked [GSList][glib-Singly-Linked-Lists],
 * which only allows movement through the list in the forward direction).
 *
 * The double linked list does not keep track of the number of items
 * and does not keep track of both the start and end of the list. If
 * you want fast access to both the start and the end of the list,
 * and/or the number of items in the list, use a
 * [GQueue][glib-Double-ended-Queues] instead.
 *
 * The data contained in each element can be either integer values, by
 * using one of the [Type Conversion Macros][glib-Type-Conversion-Macros],
 * or simply pointers to any type of data.
 *
 * List elements are allocated from the [slice allocator][glib-Memory-Slices],
 * which is more efficient than allocating elements individually.
 *
 * Note that most of the #bglist_t functions expect to be passed a pointer
 * to the first element in the list. The functions which insert
 * elements return the new start of the list, which may have changed.
 *
 * There is no function to create a #bglist_t. %NULL is considered to be
 * a valid, empty list so you simply set a #bglist_t* to %NULL to initialize
 * it.
 *
 * To add elements, use bg_list_append(), bg_list_prepend(),
 * bg_list_insert() and bg_list_insert_sorted().
 *
 * To visit all elements in the list, use a loop over the list:
 * |[<!-- language="C" -->
 * bglist_t *l;
 * for( l = list; l != NULL; l = l->next )
 *   {
 *     // do something with l->data
 *   }
 * ]|
 *
 * To call a function for each element in the list, use bg_list_foreach().
 *
 * To loop over the list and modify it (e.g. remove a certain element)
 * a while loop is more appropriate, for example:
 * |[<!-- language="C" -->
 * bglist_t *l = list;
 * while( l != NULL )
 *   {
 *     bglist_t *next = l->next;
 *     if( should_be_removed( l ) )
 *       {
 *         // possibly free l->data
 *         list = bg_list_delete_link( list, l );
 *       }
 *     l = next;
 *   }
 * ]|
 *
 * To remove elements, use bg_list_remove().
 *
 * To navigate in a list, use bg_list_first(), bg_list_last(),
 * g_list_next(), g_list_previous().
 *
 * To find elements in the list use bg_list_nth(), bg_list_nth_data(),
 * bg_list_find() and bg_list_find_custom().
 *
 * To find the index of an element use bg_list_position() and
 * bg_list_index().
 *
 * To free the entire list, use bg_list_free() or bg_list_free_full().
 */

/**
 * bglist_t:
 * @data: holds the element's data, which can be a pointer to any kind
 *        of data, or any integer value using the
 *        [Type Conversion Macros][glib-Type-Conversion-Macros]
 * @next: contains the link to the next element in the list
 * @prev: contains the link to the previous element in the list
 *
 * The #bglist_t struct is used for each element in a doubly-linked list.
 **/

/**
 * g_list_previous:
 * @list: an element in a #bglist_t
 *
 * A convenience macro to get the previous element in a #bglist_t.
 * Note that it is considered perfectly acceptable to access
 * @list->previous directly.
 *
 * Returns: the previous element, or %NULL if there are no previous
 *          elements
 **/

/**
 * g_list_next:
 * @list: an element in a #bglist_t
 *
 * A convenience macro to get the next element in a #bglist_t.
 * Note that it is considered perfectly acceptable to access
 * @list->next directly.
 *
 * Returns: the next element, or %NULL if there are no more elements
 **/


/*
--------------------------------------------------------------------------------
Custom Dynamic Memory Management for linked lists
*/
 #ifdef GAME
 # define  MAX_LISTS ( 16 * 1024 )
 #else
 #ifdef CGAME
 # define  MAX_LISTS ( 16 * 1024 )
 #else
 # define  MAX_LISTS ( 16 * 1024 )
 #endif
 #endif

 
 allocator_protos( bglist )
 allocator( bglist, sizeof(bglist_t), MAX_LISTS )
 
/*
===============
BG_InitListMemory

Executed in the primary initialization functions of
the modules that use linked lists.
===============
*/
void BG_InitListMemory( char *calledFile, int calledLine )
{
  initPool_bglist( calledFile, calledLine );
}

/*
===============
_bg_list_alloc0

Returns an allocated list initialized to NULL
===============
*/
static ID_INLINE bglist_t *_bg_list_alloc0( void )
{
  return memset( alloc_bglist( __FILE__, __LINE__ ), 0, sizeof( bglist_t ) );
}

/*
===============
BG_MemoryInfoForLinkedLists

Displays info related to the allocation of linked lists
===============
*/
void BG_MemoryInfoForLinkedLists( void )
{
  memoryInfo_bglist(  );
}

/*
--------------------------------------------------------------------------------
*/

/**
 * bg_list_alloc:
 *
 * Allocates space for one #bglist_t element. It is called by
 * bg_list_append(), bg_list_prepend(), bg_list_insert() and
 * bg_list_insert_sorted() and so is rarely used on its own.
 *
 * Returns: a pointer to the newly-allocated #bglist_t element
 **/
bglist_t *bg_list_alloc( void )
{
  return _bg_list_alloc0();
}

/**
 * bg_list_free:
 * @list: a #bglist_t
 *
 * Frees all of the memory used by a #bglist_t.
 * The freed elements are returned to the slice allocator.
 *
 * If list elements contain dynamically-allocated memory, you should
 * either use bg_list_free_full() or free them manually first.
 */
void bg_list_free( bglist_t *list )
{
  bglist_t *tmp;

  while( list )
  {
    tmp = list->next;
    free_bglist( list, __FILE__, __LINE__ );
    list = tmp;
  }
}

/**
 * bg_list_free_1:
 * @list: a #bglist_t element
 *
 * Frees one #bglist_t element, but does not update links from the next and
 * previous elements in the list, so you should not call this function on an
 * element that is currently part of a list.
 *
 * It is usually used after bg_list_remove_link().
 */
/**
 * bg_list_free1:
 *
 * Another name for bg_list_free_1().
 **/
void bg_list_free_1( bglist_t *list )
{
  free_bglist(list, __FILE__, __LINE__);
}

/**
 * bg_list_free_full:
 * @list: a pointer to a #bglist_t
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #bglist_t,
 * and calls @free_func on every element's data.
 *
 * Since: 2.28
 */
void bg_list_free_full( bglist_t *list, BG_DestroyNotify  free_func )
{
  bg_list_foreach( list, (BG_Func) free_func, NULL );
  bg_list_free( list );
}

/**
 * bg_list_append:
 * @list: a pointer to a #bglist_t
 * @data: the data for the new element
 *
 * Adds a new element on to the end of the list.
 *
 * Note that the return value is the new start of the list,
 * if @list was empty; make sure you store the new value.
 *
 * bg_list_append() has to traverse the entire list to find the end,
 * which is inefficient when adding multiple elements. A common idiom
 * to avoid the inefficiency is to use bg_list_prepend() and reverse
 * the list with bg_list_reverse() when all elements have been added.
 *
 * |[<!-- language="C" -->
 * // Notice that these are initialized to the empty list.
 * bglist_t *string_list = NULL, *number_list = NULL;
 *
 * // This is a list of strings.
 * string_list = bg_list_append( string_list, "first" );
 * string_list = bg_list_append( string_list, "second" );
 *
 * // This is a list of integers.
 * number_list = bg_list_append( number_list, GINT_TO_POINTER(27) );
 * number_list = bg_list_append( number_list, GINT_TO_POINTER(14) );
 * ]|
 *
 * Returns: either @list or the new start of the #bglist_t if @list was %NULL
 */
bglist_t *bg_list_append( bglist_t *list, void *data )
{
  bglist_t *new_list;
  bglist_t *last;

  new_list = alloc_bglist( __FILE__, __LINE__ );
  new_list->data = data;
  new_list->next = NULL;

  if(list)
    {
      last = bg_list_last( list );
      /* g_assert( last != NULL ); */
      last->next = new_list;
      new_list->prev = last;

      return list;
    }
  else
    {
      new_list->prev = NULL;
      return new_list;
    }
}

/**
 * bg_list_prepend:
 * @list: a pointer to a #bglist_t, this must point to the top of the list
 * @data: the data for the new element
 *
 * Prepends a new element on to the start of the list.
 *
 * Note that the return value is the new start of the list,
 * which will have changed, so make sure you store the new value.
 *
 * |[<!-- language="C" -->
 * // Notice that it is initialized to the empty list.
 * bglist_t *list = NULL;
 *
 * list = bg_list_prepend( list, "last" );
 * list = bg_list_prepend( list, "first" );
 * ]|
 *
 * Do not use this function to prepend a new element to a different
 * element than the start of the list. Use bg_list_insert_before() instead.
 *
 * Returns: a pointer to the newly prepended element, which is the new
 *     start of the #bglist_t
 */
bglist_t *bg_list_prepend( bglist_t *list, void *data )
{
  bglist_t *new_list;

  new_list = alloc_bglist( __FILE__, __LINE__ );
  new_list->data = data;
  new_list->next = list;

  if( list )
    {
      new_list->prev = list->prev;
      if( list->prev )
        list->prev->next = new_list;
      list->prev = new_list;
    }
  else
    new_list->prev = NULL;

  return new_list;
}

/**
 * bg_list_insert:
 * @list: a pointer to a #bglist_t, this must point to the top of the list
 * @data: the data for the new element
 * @position: the position to insert the element. If this is
 *     negative, or is larger than the number of elements in the
 *     list, the new element is added on to the end of the list.
 *
 * Inserts a new element into the list at the given position.
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
bglist_t *bg_list_insert( bglist_t *list, void *data, int position )
{
  bglist_t *new_list;
  bglist_t *tmp_list;

  if(position < 0)
    return bg_list_append( list, data );
  else if( position == 0 )
    return bg_list_prepend( list, data );

  tmp_list = bg_list_nth( list, position );
  if( !tmp_list )
    return bg_list_append( list, data );

  new_list = alloc_bglist( __FILE__, __LINE__ );
  new_list->data = data;
  new_list->prev = tmp_list->prev;
  tmp_list->prev->next = new_list;
  new_list->next = tmp_list;
  tmp_list->prev = new_list;

  return list;
}

/**
 * bg_list_insert_before:
 * @list: a pointer to a #bglist_t, this must point to the top of the list
 * @sibling: the list element before which the new element
 *     is inserted or %NULL to insert at the end of the list
 * @data: the data for the new element
 *
 * Inserts a new element into the list before the given position.
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
bglist_t *
bg_list_insert_before(bglist_t    *list,
                      bglist_t    *sibling,
                      void *data)
{
  if( !list )
    {
      list = bg_list_alloc();
      list->data = data;
      assert( sibling == NULL );
      return list;
    }
  else if( sibling )
    {
      bglist_t *node;

      node = alloc_bglist( __FILE__, __LINE__ );
      node->data = data;
      node->prev = sibling->prev;
      node->next = sibling;
      sibling->prev = node;
      if( node->prev )
        {
          node->prev->next = node;
          return list;
        }
      else
        {
          assert( sibling == list );
          return node;
        }
    }
  else
    {
      bglist_t *last;

      last = list;
      while( last->next )
        last = last->next;

      last->next = alloc_bglist( __FILE__, __LINE__ );
      last->next->data = data;
      last->next->prev = last;
      last->next->next = NULL;

      return list;
    }
}

/**
 * bg_list_concat:
 * @list1: a #bglist_t, this must point to the top of the list
 * @list2: the #bglist_t to add to the end of the first #bglist_t,
 *     this must point  to the top of the list
 *
 * Adds the second #bglist_t onto the end of the first #bglist_t.
 * Note that the elements of the second #bglist_t are not copied.
 * They are used directly.
 *
 * This function is for example used to move an element in the list.
 * The following example moves an element to the top of the list:
 * |[<!-- language="C" -->
 * list = bg_list_remove_link( list, llink );
 * list = bg_list_concat(llink, list);
 * ]|
 *
 * Returns: the start of the new #bglist_t, which equals @list1 if not %NULL
 */
bglist_t *bg_list_concat( bglist_t *list1, bglist_t *list2 )
{
  bglist_t *tmp_list;

  if( list2 )
    {
      tmp_list = bg_list_last( list1 );
      if( tmp_list )
        tmp_list->next = list2;
      else
        list1 = list2;
      list2->prev = tmp_list;
    }

  return list1;
}

static ID_INLINE bglist_t *_bg_list_remove_link( bglist_t *list, bglist_t *link )
{
  if( link == NULL )
    return list;

  if( link->prev )
    {
      if( link->prev->next == link )
        link->prev->next = link->next;
      else
        Com_Error( ERR_DROP,
               "_bg_list_remove_link: corrupted double-linked list detected!" );
    }
  if( link->next )
    {
      if( link->next->prev == link )
        link->next->prev = link->prev;
      else
        Com_Error( ERR_DROP,
               "_bg_list_remove_link: corrupted double-linked list detected!" );
    }

  if( link == list )
    list = list->next;

  link->next = NULL;
  link->prev = NULL;

  return list;
}

/**
 * bg_list_remove:
 * @list: a #bglist_t, this must point to the top of the list
 * @data: the data of the element to remove
 *
 * Removes an element from a #bglist_t.
 * If two elements contain the same data, only the first is removed.
 * If none of the elements contain the data, the #bglist_t is unchanged.
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
bglist_t *bg_list_remove( bglist_t *list, const void *data  )
{
  bglist_t *tmp;

  tmp = list;
  while( tmp )
    {
      if( tmp->data != data )
        tmp = tmp->next;
      else
        {
          list = _bg_list_remove_link( list, tmp );
          free_bglist( tmp, __FILE__, __LINE__ );

          break;
        }
    }
  return list;
}

/**
 * bg_list_remove_all:
 * @list: a #bglist_t, this must point to the top of the list
 * @data: data to remove
 *
 * Removes all list nodes with data equal to @data.
 * Returns the new head of the list. Contrast with
 * bg_list_remove() which removes only the first node
 * matching the given data.
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
bglist_t *bg_list_remove_all( bglist_t *list, const void *data )
{
  bglist_t *tmp = list;

  while( tmp )
    {
      if( tmp->data != data )
        tmp = tmp->next;
      else
        {
          bglist_t *next = tmp->next;

          if( tmp->prev )
            tmp->prev->next = next;
          else
            list = next;
          if( next )
            next->prev = tmp->prev;

          free_bglist( tmp, __FILE__, __LINE__ );
          tmp = next;
        }
    }
  return list;
}

/**
 * bg_list_remove_link:
 * @list: a #bglist_t, this must point to the top of the list
 * @llink: an element in the #bglist_t
 *
 * Removes an element from a #bglist_t, without freeing the element.
 * The removed element's prev and next links are set to %NULL, so
 * that it becomes a self-contained list with one element.
 *
 * This function is for example used to move an element in the list
 * (see the example for bg_list_concat()) or to remove an element in
 * the list before freeing its data:
 * |[<!-- language="C" -->
 * list = bg_list_remove_link( list, llink );
 * free_some_data_that_may_access_the_list_again( llink->data );
 * bg_list_free( llink );
 * ]|
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
bglist_t *bg_list_remove_link( bglist_t *list, bglist_t *llink )
{
  return _bg_list_remove_link( list, llink );
}

/**
 * bg_list_delete_link:
 * @list: a #bglist_t, this must point to the top of the list
 * @link_: node to delete from @list
 *
 * Removes the node link_ from the list and frees it.
 * Compare this to bg_list_remove_link() which removes the node
 * without freeing it.
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
bglist_t *bg_list_delete_link( bglist_t *list, bglist_t *link_ )
{
  list = _bg_list_remove_link( list, link_ );
  free_bglist( link_, __FILE__, __LINE__ );

  return list;
}

/**
 * bg_list_copy:
 * @list: a #bglist_t, this must point to the top of the list
 *
 * Copies a #bglist_t.
 *
 * Note that this is a "shallow" copy. If the list elements
 * consist of pointers to data, the pointers are copied but
 * the actual data is not. See bg_list_copy_deep() if you need
 * to copy the data as well.
 *
 * Returns: the start of the new list that holds the same data as @list
 */
bglist_t *bg_list_copy( bglist_t *list )
{
  return bg_list_copy_deep( list, NULL, NULL );
}

/**
 * bg_list_copy_deep:
 * @list: a #bglist_t, this must point to the top of the list
 * @func: a copy function used to copy every element in the list
 * @user_data: user data passed to the copy function @func, or %NULL
 *
 * Makes a full (deep) copy of a #bglist_t.
 *
 * In contrast with bg_list_copy(), this function uses @func to make
 * a copy of each list element, in addition to copying the list
 * container itself.
 *
 * @func, as a #BG_CopyFunc, takes two arguments, the data to be copied
 * and a @user_data pointer. It's safe to pass %NULL as user_data,
 * if the copy function takes only one argument.
 *
 * For instance, if @list holds a list of GObjects, you can do:
 * |[<!-- language="C" -->
 * another_list = bg_list_copy_deep( list, (BG_CopyFunc) g_object_ref, NULL);
 * ]|
 *
 * And, to entirely free the new list, you could do:
 * |[<!-- language="C" -->
 * bg_list_free_full( another_list, g_object_unref );
 * ]|
 *
 * Returns: the start of the new list that holds a full copy of @list,
 *     use bg_list_free_full() to free it
 *
 * Since: 2.34
 */
bglist_t *bg_list_copy_deep( bglist_t *list, BG_CopyFunc  func, void *user_data )
{
  bglist_t *new_list = NULL;

  if( list )
    {
      bglist_t *last;

      new_list = alloc_bglist( __FILE__, __LINE__ );
      if( func )
        new_list->data = func( list->data, user_data );
      else
        new_list->data = list->data;
      new_list->prev = NULL;
      last = new_list;
      list = list->next;
      while( list )
        {
          last->next = alloc_bglist( __FILE__, __LINE__ );
          last->next->prev = last;
          last = last->next;
          if( func )
            last->data = func( list->data, user_data );
          else
            last->data = list->data;
          list = list->next;
        }
      last->next = NULL;
    }

  return new_list;
}

/**
 * bg_list_reverse:
 * @list: a #bglist_t, this must point to the top of the list
 *
 * Reverses a #bglist_t.
 * It simply switches the next and prev pointers of each element.
 *
 * Returns: the start of the reversed #bglist_t
 */
bglist_t *bg_list_reverse( bglist_t *list )
{
  bglist_t *last;

  last = NULL;
  while( list )
    {
      last = list;
      list = last->next;
      last->next = last->prev;
      last->prev = list;
    }

  return last;
}

/**
 * bg_list_nth:
 * @list: a #bglist_t, this must point to the top of the list
 * @n: the position of the element, counting from 0
 *
 * Gets the element at the given position in a #bglist_t.
 *
 * This iterates over the list until it reaches the @n-th position. If you
 * intend to iterate over every element, it is better to use a for-loop as
 * described in the #bglist_t introduction.
 *
 * Returns: the element, or %NULL if the position is off
 *     the end of the #bglist_t
 */
bglist_t *bg_list_nth( bglist_t *list, unsigned int  n )
{
  while( ( n-- > 0 ) && list )
    list = list->next;

  return list;
}

/**
 * bg_list_nth_prev:
 * @list: a #bglist_t
 * @n: the position of the element, counting from 0
 *
 * Gets the element @n places before @list.
 *
 * Returns: the element, or %NULL if the position is
 *     off the end of the #bglist_t
 */
bglist_t *bg_list_nth_prev( bglist_t *list, unsigned int  n)
{
  while( ( n-- > 0 ) && list )
    list = list->prev;

  return list;
}

/**
 * bg_list_nth_data:
 * @list: a #bglist_t, this must point to the top of the list
 * @n: the position of the element
 *
 * Gets the data of the element at the given position.
 *
 * This iterates over the list until it reaches the @n-th position. If you
 * intend to iterate over every element, it is better to use a for-loop as
 * described in the #bglist_t introduction.
 *
 * Returns: the element's data, or %NULL if the position
 *     is off the end of the #bglist_t
 */
void *bg_list_nth_data( bglist_t *list, unsigned int  n )
{
  while( ( n-- > 0 ) && list )
    list = list->next;

  return list ? list->data : NULL;
}

/**
 * bg_list_find:
 * @list: a #bglist_t, this must point to the top of the list
 * @data: the element data to find
 *
 * Finds the element in a #bglist_t which contains the given data.
 *
 * Returns: the found #bglist_t element, or %NULL if it is not found
 */
bglist_t *bg_list_find( bglist_t *list, const void *data )
{
  while( list )
    {
      if( list->data == data )
        break;
      list = list->next;
    }

  return list;
}

/**
 * bg_list_find_custom:
 * @list: a #bglist_t, this must point to the top of the list
 * @data: user data passed to the function
 * @func: the function to call for each element.
 *     It should return 0 when the desired element is found
 *
 * Finds an element in a #bglist_t, using a supplied function to
 * find the desired element. It iterates over the list, calling
 * the given function which should return 0 when the desired
 * element is found. The function takes two #const void * arguments,
 * the #bglist_t element's data as the first argument and the
 * given user data.
 *
 * Returns: the found #bglist_t element, or %NULL if it is not found
 */
bglist_t *bg_list_find_custom( bglist_t *list, const void *data,
                                BG_CompareFunc func )
{
  assert( func != NULL );

  while( list )
    {
      if( ! func( list->data, data ) )
        return list;
      list = list->next;
    }

  return NULL;
}

/**
 * bg_list_position:
 * @list: a #bglist_t, this must point to the top of the list
 * @llink: an element in the #bglist_t
 *
 * Gets the position of the given element
 * in the #bglist_t (starting from 0).
 *
 * Returns: the position of the element in the #bglist_t,
 *     or -1 if the element is not found
 */
int bg_list_position( bglist_t *list, bglist_t *llink )
{
  int i;

  i = 0;
  while( list )
    {
      if( list == llink )
        return i;
      i++;
      list = list->next;
    }

  return -1;
}

/**
 * bg_list_index:
 * @list: a #bglist_t, this must point to the top of the list
 * @data: the data to find
 *
 * Gets the position of the element containing
 * the given data (starting from 0).
 *
 * Returns: the index of the element containing the data,
 *     or -1 if the data is not found
 */
int bg_list_index( bglist_t *list, const void *data )
{
  int i;

  i = 0;
  while( list )
    {
      if( list->data == data )
        return i;
      i++;
      list = list->next;
    }

  return -1;
}

/**
 * bg_list_last:
 * @list: any #bglist_t element
 *
 * Gets the last element in a #bglist_t.
 *
 * Returns: the last element in the #bglist_t,
 *     or %NULL if the #bglist_t has no elements
 */
bglist_t * bg_list_last( bglist_t *list )
{
  if(list)
    {
      while(list->next)
        list = list->next;
    }

  return list;
}

/**
 * bg_list_first:
 * @list: any #bglist_t element
 *
 * Gets the first element in a #bglist_t.
 *
 * Returns: the first element in the #bglist_t,
 *     or %NULL if the #bglist_t has no elements
 */
bglist_t *bg_list_first( bglist_t *list )
{
  if( list )
    {
      while( list->prev )
        list = list->prev;
    }

  return list;
}

/**
 * bg_list_length:
 * @list: a #bglist_t, this must point to the top of the list
 *
 * Gets the number of elements in a #bglist_t.
 *
 * This function iterates over the whole list to count its elements.
 * Use a #GQueue instead of a bglist_t if you regularly need the number
 * of items. To check whether the list is non-empty, it is faster to check
 * @list against %NULL.
 *
 * Returns: the number of elements in the #bglist_t
 */
unsigned int
bg_list_length( bglist_t *list )
{
  unsigned int length;

  length = 0;
  while( list )
    {
      length++;
      list = list->next;
    }

  return length;
}

/**
 * bg_list_foreach:
 * @list: a #bglist_t, this must point to the top of the list
 * @func: the function to call with each element's data
 * @user_data: user data to pass to the function
 *
 * Calls a function for each element of a #bglist_t.
 */
/**
 * BG_Func:
 * @data: the element's data
 * @user_data: user data passed to bg_list_foreach() or g_slist_foreach()
 *
 * Specifies the type of functions passed to bg_list_foreach() and
 * g_slist_foreach().
 */
void bg_list_foreach( bglist_t *list, BG_Func func, void *user_data )
{
  while( list )
    {
      bglist_t *next = list->next;
      (*func)( list->data, user_data );
      list = next;
    }
}

static bglist_t *bg_list_insert_sorted_real( bglist_t *list, void *data,
                                             BG_Func func, void *user_data )
{
  bglist_t *tmp_list = list;
  bglist_t *new_list;
  int cmp;

  assert( func != NULL );

  if ( !list )
    {
      new_list = _bg_list_alloc0 ();
      new_list->data = data;
      return new_list;
    }

  cmp = ( ( BG_CompareDataFunc ) func ) ( data, tmp_list->data, user_data );

  while ( ( tmp_list->next ) && ( cmp > 0 ) )
    {
      tmp_list = tmp_list->next;

      cmp = ( (BG_CompareDataFunc) func) ( data, tmp_list->data, user_data );
    }

  new_list = _bg_list_alloc0 ();
  new_list->data = data;

  if ( ( !tmp_list->next ) && ( cmp > 0 ) )
    {
      tmp_list->next = new_list;
      new_list->prev = tmp_list;
      return list;
    }

  if ( tmp_list->prev )
    {
      tmp_list->prev->next = new_list;
      new_list->prev = tmp_list->prev;
    }
  new_list->next = tmp_list;
  tmp_list->prev = new_list;

  if ( tmp_list == list )
    return new_list;
  else
    return list;
}

/**
 * bg_list_insert_sorted:
 * @list: a pointer to a #bglist_t, this must point to the top of the
 *     already sorted list
 * @data: the data for the new element
 * @func: the function to compare elements in the list. It should
 *     return a number > 0 if the first parameter comes after the
 *     second parameter in the sort order.
 *
 * Inserts a new element into the list, using the given comparison
 * function to determine its position.
 *
 * If you are adding many new elements to a list, and the number of
 * new elements is much larger than the length of the list, use
 * bg_list_prepend() to add the new items and sort the list afterwards
 * with bg_list_sort().
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
bglist_t *bg_list_insert_sorted( bglist_t *list, void * data,
                                 BG_CompareFunc func )
{
  return bg_list_insert_sorted_real ( list, data, (BG_Func) func, NULL );
}

/**
 * bg_list_insert_sorted_with_data:
 * @list: a pointer to a #bglist_t, this must point to the top of the
 *     already sorted list
 * @data: the data for the new element
 * @func: the function to compare elements in the list. It should
 *     return a number > 0 if the first parameter  comes after the
 *     second parameter in the sort order.
 * @user_data: user data to pass to comparison function
 *
 * Inserts a new element into the list, using the given comparison
 * function to determine its position.
 *
 * If you are adding many new elements to a list, and the number of
 * new elements is much larger than the length of the list, use
 * bg_list_prepend() to add the new items and sort the list afterwards
 * with bg_list_sort().
 *
 * Returns: the (possibly changed) start of the #bglist_t
 *
 * Since: 2.10
 */
bglist_t *bg_list_insert_sorted_with_data( bglist_t *list, void *data,
                                           BG_CompareDataFunc func,
                                           void *user_data)
{
  return bg_list_insert_sorted_real( list, data, (BG_Func) func, user_data );
}

static bglist_t *bg_list_sort_merge( bglist_t *l1, bglist_t     *l2,
                                     BG_Func     compare_func, void *user_data)
{
  bglist_t list, *l, *lprev;
  int cmp;

  l = &list;
  lprev = NULL;

  while( l1 && l2 )
    {
      cmp = ((BG_CompareDataFunc) compare_func) (l1->data, l2->data, user_data);

      if( cmp <= 0 )
        {
          l->next = l1;
          l1 = l1->next;
        }
      else
        {
          l->next = l2;
          l2 = l2->next;
        }
      l = l->next;
      l->prev = lprev;
      lprev = l;
    }
  l->next = l1 ? l1 : l2;
  l->next->prev = l;

  return list.next;
}

static bglist_t *bg_list_sort_real( bglist_t *list, BG_Func compare_func,
                                    void *user_data)
{
  bglist_t *l1, *l2;

  if (!list)
    return NULL;
  if (!list->next)
    return list;

  l1 = list;
  l2 = list->next;

  while( ( l2 = l2->next ) != NULL )
    {
      if( ( l2 = l2->next ) == NULL )
        break;
      l1 = l1->next;
    }
  l2 = l1->next;
  l1->next = NULL;

  return bg_list_sort_merge( bg_list_sort_real( list, compare_func, user_data ),
                             bg_list_sort_real( l2, compare_func, user_data ),
                             compare_func, user_data );
}

/**
 * bg_list_sort:
 * @list: a #bglist_t, this must point to the top of the list
 * @compare_func: the comparison function used to sort the #bglist_t.
 *     This function is passed the data from 2 elements of the #bglist_t
 *     and should return 0 if they are equal, a negative value if the
 *     first element comes before the second, or a positive value if
 *     the first element comes after the second.
 *
 * Sorts a #bglist_t using the given comparison function. The algorithm
 * used is a stable sort.
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
/**
 * BG_CompareFunc:
 * @a: a value
 * @b: a value to compare with
 *
 * Specifies the type of a comparison function used to compare two
 * values.  The function should return a negative integer if the first
 * value comes before the second, 0 if they are equal, or a positive
 * integer if the first value comes after the second.
 *
 * Returns: negative value if @a < @b; zero if @a = @b; positive
 *          value if @a > @b
 */
bglist_t *bg_list_sort( bglist_t *list, BG_CompareFunc compare_func )
{
  return bg_list_sort_real( list, (BG_Func) compare_func, NULL );
}

/**
 * bg_list_sort_with_data:
 * @list: a #bglist_t, this must point to the top of the list
 * @compare_func: comparison function
 * @user_data: user data to pass to comparison function
 *
 * Like bg_list_sort(), but the comparison function accepts
 * a user data argument.
 *
 * Returns: the (possibly changed) start of the #bglist_t
 */
/**
 * BG_CompareDataFunc:
 * @a: a value
 * @b: a value to compare with
 * @user_data: user data
 *
 * Specifies the type of a comparison function used to compare two
 * values.  The function should return a negative integer if the first
 * value comes before the second, 0 if they are equal, or a positive
 * integer if the first value comes after the second.
 *
 * Returns: negative value if @a < @b; zero if @a = @b; positive
 *          value if @a > @b
 */
bglist_t *bg_list_sort_with_data( bglist_t *list,
                                  BG_CompareDataFunc compare_func,
                                  void *user_data )
{
  return bg_list_sort_real( list, (BG_Func) compare_func, user_data );
}
