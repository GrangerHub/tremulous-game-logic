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

/*
--------------------------------------------------------------------------------
Declarations of static BG_Link functions
*/
static void      BG_Link_Free(bglink_t *link);
static void      BG_Link_Free_Full(bglink_t *link, BG_Destroy free_func);
static void      BG_Link_Free_1(bglink_t *link);
#define          BG_Link_Free1 BG_Link_Free_1

static bglink_t  *BG_Link_Append(bglink_t *link, void *data);
static bglink_t  *_BG_Link_Prepend(bglink_t *head, void *data);
static void      _BG_List_Insert_Before(
  bglist_t *list, bglink_t *sibling, void *data);

static bglink_t  *BG_Link_Remove_Link(bglink_t *head, bglink_t *llink);

static bglink_t  *BG_Link_Reverse(bglink_t *head);


static bglink_t  *BG_Link_Find(bglink_t *head, const void *data);
static bglink_t  *BG_Link_Find_Custom(
  bglink_t *head, const void *data, BG_CompareFunc func);
static int       BG_Link_Position(bglink_t *head, bglink_t *llink);
static int       BG_Link_Index(bglink_t *head, const void *data);
static bglink_t  *BG_Link_Last(bglink_t *link);

static bglink_t  *BG_Link_Sort_With_Data(
  bglink_t *head, BG_CompareDataFunc compare_func, void *user_data);
/*
--------------------------------------------------------------------------------
*/

/**
 * SECTION:list
 * @Title: Double-ended Linked Lists
 * @Short_description: double-ended list data structure
 *
 * The #bglist_t structure and its associated functions provide a standard
 * list data structure. Internally, bglist_t uses the same data structure
 * as #bglink_t to store elements.
 *
 * The data contained in each element are pointers to any type of data.
 *
 * To create a new bglist_t, use BG_List_New().
 *
 * To initialize a statically-allocated bglist_t, use #BG_List_INIT or
 * BG_List_Init().
 *
 * To add elements, use BG_List_Push_Head(), BG_List_Push_Head_Link(),
 * BG_List_Push_Tail() and BG_List_Push_Tail_Link().
 *
 * To remove elements, use BG_List_Pop_Head() and BG_List_Pop_Tail().
 *
 * To free the entire list, use BG_List_Free().
 */

/*
--------------------------------------------------------------------------------
Custom Dynamic Memory Management for lists
*/
#ifdef GAME
# define  MAX_LISTS 1024
#else
#ifdef CGAME
# define  MAX_LISTS 1024
#else
# define  MAX_LISTS 1024
#endif
#endif

allocator_protos(bglist)
allocator(bglist, sizeof(bglist_t), MAX_LISTS)

/*
===============
BG_List_Init_Memory

Executed in BG_InitMemory()
===============
*/
void BG_List_Init_Memory(char *calledFile, int calledLine) {
  initPool_bglist(calledFile, calledLine);
}

/*
===============
_BG_List_Alloc0

Returns an allocated list initialized to NULL
===============
*/
static ID_INLINE bglist_t *_BG_List_Alloc0(void) {
  return memset( alloc_bglist(__FILE__, __LINE__), 0, sizeof(bglist_t));
}

/*
===============
BG_List_Memory_Info

Displays info related to the allocation of linked lists
===============
*/
void BG_List_Memory_Info(void) {
  memoryInfo_bglist( );
}

/*
--------------------------------------------------------------------------------
*/

/*
--------------------------------------------------------------------------------
BG_List functions
*/
/**
 * BG_List_New:
 *
 * Creates a new #bglist_t.
 *
 * Returns: a newly allocated #bglist_t
 **/
bglist_t *BG_List_New(void) {
  return _BG_List_Alloc0( );
}

/**
 * BG_List_Free:
 * @list: a #bglist_t
 *
 * Frees the memory allocated for the #bglist_t. Only call this function
 * if @list was created with BG_List_New(). 
 *
 * If list elements contain dynamically-allocated memory, you should
 * either use BG_List_Free_Full() or free them manually first.
 **/
void BG_List_Free (bglist_t *list) {
  Com_Assert(list != NULL);

  BG_Link_Free(list->head);
  free_bglist(list, __FILE__, __LINE__);
}

/**
 * BG_List_Free_Full:
 * @list: a pointer to a #bglist_t
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #bglist_t,
 * and calls the specified destroy function on every element's data.
 *
 * Since: 2.32
 */
void BG_List_Free_Full(bglist_t *list, BG_Destroy free_func) {
  BG_List_Foreach(list, NULL, (BG_Func) free_func, NULL);
  BG_List_Free(list);
}

/**
 * BG_List_Init:
 * @list: an uninitialized #bglist_t
 *
 * A statically-allocated #bglist_t must be initialized with this function
 * before it can be used. Alternatively you can initialize it with
 * #BG_List_INIT. It is not necessary to initialize lists created with
 * BG_List_New().
 *
 * Since: 2.14
 */
void BG_List_Init(bglist_t *list) {
  Com_Assert(list != NULL);

  list->head = list->tail = NULL;
  list->length = 0;
  list->locked = qfalse;
}

/**
 * BG_List_Valid:
 * @list: any #bglist_t element
 *
 * Dertmines if @list and all the links in it are valid.
 *
 * Returns: qtrue if @list and all the links in it are valid or @list is NULL
 */
qboolean BG_List_Valid(bglist_t *list) {
  size_t   count = 0;
  bglink_t *link;
  bglink_t *prevLink = NULL;

  if(!list) {
    return qtrue;
  }

  link = list->head;

  while(link) {
    count++;

    if(link->list != list || link->prev != prevLink) {
      return qfalse;
    }

    prevLink = link;
    link = link->next;
  }

  if(list->length != count || list->tail != link) {
    return qfalse;
  }

  return qtrue;
}

/**
 * BG_List_Clear:
 * @list: a #bglist_t
 *
 * Removes all the elements in @list. If list elements contain
 * dynamically-allocated memory, they should be freed first, or use 
 * BG_List_Clear_Full_Forced().
 *
 * Since: 2.14
 */
void BG_List_Clear(bglist_t *list) {
  Com_Assert(list != NULL);

  BG_Link_Free(list->head);
  BG_List_Init(list);
}

/**
 * BG_List_Clear_Full:
 * @list: a pointer to a #bglist_t
 * @free_func: the function to be called to free each element's data
 *  returns qfalse if the data was failed to be freed.
 *
 * Removes all the elements in @list that can be freed, and calls the specified
 * destroy function on every element's data.  Returns qtrue if all of the
 * elements were successfully removed.
 *
 */
qboolean BG_List_Clear_Full(bglist_t *list, BG_DestroyNotify free_func) {
  bglink_t *link, *tmp;
  qboolean full_clear = qtrue;

  Com_Assert(list != NULL);

  link = list->head;
  while(link) {
    tmp = link->next;
    if(free_func(link->data)) {
      BG_List_Unlink(link);
      BG_Link_Free_1(link);
    } else {
      full_clear = qfalse;
    }
    link = tmp;
  }

  if(full_clear) {
    BG_List_Init(list);
  }

  return full_clear;
}

/**
 * BG_List_Clear_Full_Forced:
 * @list: a pointer to a #bglist_t
 * @free_func: the function to be called to free each element's data
 *
 * Removes all the elements in @list, and calls the specified destroy
 * function on every element's data.
 *
 */
void BG_List_Clear_Full_Forced(bglist_t *list, BG_Destroy free_func) {
  Com_Assert(list != NULL);

  BG_Link_Free_Full(list->head, free_func);
  BG_List_Init(list);
}

/**
 * BG_List_Is_Empty:
 * @list: a #bglist_t.
 *
 * Returns: %TRUE if the list is empty
 */
qboolean BG_List_Is_Empty(bglist_t *list) {
  Com_Assert(list != NULL);

  return list->head == NULL;
}

/**
 * BG_List_Get_Length:
 * @list: a #bglist_t
 * 
 * Returns: the number of items in @list
 * 
 * Since: 2.4
 */
size_t BG_List_Get_Length(bglist_t *list) {
  Com_Assert(list != NULL);

  return list->length;
}

/**
 * BG_List_Reverse:
 * @list: a #bglist_t
 * 
 * Reverses the order of the items in @list.
 * 
 * Since: 2.4
 */
void BG_List_Reverse(bglist_t *list) {
  Com_Assert(list != NULL);

  list->tail = list->head;
  list->head = BG_Link_Reverse(list->head);
}

/**
 * BG_List_Copy:
 * @list: a #bglist_t
 * 
 * Copies a @list. Note that is a shallow copy. If the elements in the
 * list consist of pointers to data, the pointers are copied, but the
 * actual data is not.
 * 
 * Returns: a copy of @list
 * 
 * Since: 2.4
 */
bglist_t *BG_List_Copy(bglist_t *list) {
  return BG_List_Copy_Deep(list, NULL, NULL);
}

/**
 * BG_List_Copy_Deep:
 * @list: a #bglist_t
 * @func: a copy function used to copy every element in the list
 * @user_data: user data passed to the copy function @func, or %NULL
 *
 * Makes a full (deep) copy of a #bglist_t.
 *
 * In contrast with BG_List_Copy(), this function uses @func to make
 * a copy of each list element, in addition to copying the list
 * container itself.
 *
 * @func, as a #BG_CopyFunc, takes two arguments, the data to be copied
 * and a @user_data pointer. It's safe to pass %NULL as user_data,
 * if the copy function takes only one argument.
 * 
 * Returns: a copy of @list
 * 
 * Since: 2.4
 */
bglist_t *BG_List_Copy_Deep(bglist_t *list, BG_CopyFunc  func, void *user_data) {
  bglist_t *result;
  bglink_t *link;

  Com_Assert(list != NULL);

  result = BG_List_New( );

  for(link = list->head; link != NULL; link = link->next) {
    Com_Assert(link->list == list);

    if(func) {
      BG_List_Push_Tail(result, func(link->data, user_data));
    }  else {
      BG_List_Push_Tail(result, link->data);
    }
  }

  return result;
}

/**
 * BG_List_Foreach:
 * @list: a #bglist_t
 * @startLink: The link in the list that the loop starts at. If NULL
               the loop would start at the head of the list.
 * @func: the function to call for each element's data
 * @user_data: user data to pass to @func
 * 
 * Calls @func for each element in the list passing @user_data to the
 * function.
 * 
 * Since: 2.4
 */
void BG_List_Foreach(
  bglist_t *list, bglink_t *startLink, BG_Func func, void *user_data) {
  bglink_t *link;

  Com_Assert(list != NULL);
  Com_Assert(func != NULL);

  if(startLink) {
    link = startLink;
  } else {
    link = list->head;
  }

  while(link) {
    bglink_t *next = link->next;
    Com_Assert(link->list == list);
    func(link->data, user_data);
    link = next;
  }
}

/**
 * BG_List_Foreach_With_Break:
 * @list: a #bglist_t
 * @startLink: The link in the list that the loop starts at. If NULL
               the loop would start at the head of the list.
 * @func: the function to call for each element's data. if it returns qtrue
          Foreach ends early.
 * @user_data: user data to pass to @func
 * 
 * Calls @func for each element in the list passing @user_data to the
 * function.  Like BG_List_Foreach but with the option to end early.
 * 
 * Returns: the current link if Foreach ends early.
 * 
 * Since: 2.4
 */
bglink_t *BG_List_Foreach_With_Break(
  bglist_t *list, bglink_t *startLink, BG_FuncWithReturn func, void *user_data) {
  bglink_t *link;

  Com_Assert(list != NULL);
  Com_Assert(func != NULL);

  if(startLink) {
    link = startLink;
  } else {
    link = list->head;
  }

  while(link) {
    bglink_t *next = link->next;
    Com_Assert(link->list == list);
    if(func(link->data, user_data)) {
      return link;
    }
    link = next;
  }

  return NULL;
}

/**
 * BG_List_Find:
 * @list: a #bglist_t
 * @data: data to find
 * 
 * Finds the first link in @list which contains @data.
 * 
 * Returns: the first link in @list which contains @data
 * 
 * Since: 2.4
 */
bglink_t *BG_List_Find(bglist_t *list, const void *data) {
  bglink_t *link;

  Com_Assert(list != NULL);

  link = BG_Link_Find(list->head, data);

  if(link) {
    Com_Assert(link->list == list);
  }

  return link;
}

/**
 * BG_List_Find_Custom:
 * @list: a #bglist_t
 * @data: user data passed to @func
 * @func: a #BG_CompareFunc to call for each element. It should return 0
 *     when the desired element is found
 *
 * Finds an element in a #bglist_t, using a supplied function to find the
 * desired element. It iterates over the list, calling the given function
 * which should return 0 when the desired element is found. The function
 * takes two const void * arguments, the #bglist_t element's data as the
 * first argument and the given user data as the second argument.
 * 
 * Returns: the found link, or %NULL if it wasn't found
 * 
 * Since: 2.4
 */
bglink_t * BG_List_Find_Custom(
  bglist_t *list, const void *data, BG_CompareFunc func) {
  bglink_t *link;

  Com_Assert(list != NULL);
  Com_Assert(func != NULL);

  link = BG_Link_Find_Custom(list->head, data, func);

  Com_Assert(link->list == list);

  return link;
}

/**
 * BG_List_Sort:
 * @list: a #bglist_t
 * @compare_func: the #BG_CompareDataFunc used to sort @list. This function
 *     is passed two elements of the list and should return 0 if they are
 *     equal, a negative value if the first comes before the second, and
 *     a positive value if the second comes before the first.
 * @user_data: user data passed to @compare_func
 * 
 * Sorts @list using @compare_func. 
 * 
 * Since: 2.4
 */
void BG_List_Sort(
  bglist_t *list, BG_CompareDataFunc compare_func, void *user_data) {
  Com_Assert(list != NULL);
  Com_Assert(compare_func != NULL);

  list->head = BG_Link_Sort_With_Data(list->head, compare_func, user_data);
  list->tail = BG_Link_Last(list->head);
}

/**
 * BG_List_Push_Head:
 * @list: a #bglist_t.
 * @data: the data for the new element.
 *
 * Adds a new element at the head of the list.
 *
 * Returns: the new link created for @data
 */
bglink_t *BG_List_Push_Head(bglist_t *list, void *data) {
  Com_Assert(list != NULL);

  list->head = _BG_Link_Prepend(list->head, data);
  if(!list->tail) {
    list->tail = list->head;
  }

  list->head->list = list;
  list->length++;
  return list->head;
}

/**
 * BG_List_Push_nth:
 * @list: a #bglist_t
 * @data: the data for the new element
 * @n: the position to insert the new element. If @n is negative or
 *     larger than the number of elements in the @list, the element is
 *     added to the end of the list.
 * 
 * Inserts a new element into @list at the given position.
 *
 * Returns: the new link created for @data
 * 
 * Since: 2.4
 */
bglink_t *BG_List_Push_nth(bglist_t *list, void *data, int n) {
  bglink_t *nextLink;

  Com_Assert(list != NULL);

  if(n < 0 || n >= list->length) {
    BG_List_Push_Tail(list, data);
    return list->tail;
  }

  nextLink = BG_List_Peek_nth_Link(list, n);

  BG_List_Insert_Before(list, nextLink, data);

  return nextLink->prev;
}

/**
 * BG_List_Push_Head_Link:
 * @list: a #bglist_t
 * @link_: a single #bglink_t element, not a list with more than one element
 *
 * Adds a new element at the head of the list.
 */
void BG_List_Push_Head_Link (bglist_t *list, bglink_t *link) {
  Com_Assert (list != NULL);
  Com_Assert (link != NULL);
  Com_Assert (link->prev == NULL);
  Com_Assert (link->next == NULL);

  link->next = list->head;
  if(list->head) {
    list->head->prev = link;
  } else {
    list->tail = link;
  }

  list->head = link;
  link->list = list;
  list->length++;
}

/**
 * BG_List_Push_Tail:
 * @list: a #bglist_t
 * @data: the data for the new element
 *
 * Adds a new element at the tail of the list.
 *
 * Returns: the new link created for @data
 */
bglink_t *BG_List_Push_Tail(bglist_t *list, void *data) {
  Com_Assert (list != NULL);

  list->tail = BG_Link_Append(list->tail, data);
  if(list->tail->next) {
    list->tail = list->tail->next;
  } else {
    list->head = list->tail;
  }

  list->tail->list = list;
  list->length++;
  return list->tail;
}

/**
 * BG_List_Push_Tail_Link:
 * @list: a #bglist_t
 * @link_: a single #bglink_t element, not a list with more than one element
 *
 * Adds a new element at the tail of the list.
 */
void
BG_List_Push_Tail_Link (bglist_t *list, bglink_t  *link) {
  Com_Assert (list != NULL);
  Com_Assert (link != NULL);
  Com_Assert (link->prev == NULL);
  Com_Assert (link->next == NULL);

  link->prev = list->tail;
  if(list->tail) {
    list->tail->next = link;
  } else {
    list->head = link;
  }

  list->tail = link;
  link->list = list;
  list->length++;
}

/**
 * BG_List_Push_nth_Link:
 * @list: a #bglist_t
 * @n: the position to insert the link. If this is negative or larger than
 *     the number of elements in @list, the link is added to the end of
 *     @list.
 * @link_: the link to add to @list
 * 
 * Inserts @link into @list at the given position.
 * 
 * Since: 2.4
 */
void BG_List_Push_nth_Link(bglist_t *list, int n, bglink_t *link) {
  bglink_t *next;
  bglink_t *prev;
  
  Com_Assert(list != NULL);
  Com_Assert(link != NULL);

  if(n < 0 || n >= list->length) {
    BG_List_Push_Tail_Link(list, link);
    return;
  }

  Com_Assert(list->head);
  Com_Assert(list->tail);

  next = BG_List_Peek_nth_Link(list, n);
  prev = next->prev;

  if(prev) {
    prev->next = link;
  }

  next->prev = link;
  link->next = next;
  link->prev = prev;

  if(list->head->prev) {
    list->head = list->head->prev;
  }

  if(list->tail->next) {
    list->tail = list->tail->next;
  }

  link->list = list;
  list->length++;
}

/**
 * BG_List_Pop_Head:
 * @list: a #bglist_t
 *
 * Removes the first element of the list and returns its data.
 *
 * Returns: the data of the first element in the list, or %NULL
 *     if the list is empty
 */
void *BG_List_Pop_Head(bglist_t *list) {
  Com_Assert(list != NULL);

  if(list->head) {
    bglink_t *link = list->head;
    void * data = link->data;

    Com_Assert(link->list == list);

    list->head = link->next;
    if(list->head) {
      list->head->prev = NULL;
    } else {
      list->tail = NULL;
    }

    BG_Link_Free_1(link);
    list->length--;

    return data;
  }

  return NULL;
}

/**
 * BG_List_Pop_Head_Link:
 * @list: a #bglist_t
 *
 * Removes and returns the first element of the list.
 *
 * Returns: the #bglink_t element at the head of the list, or %NULL
 *     if the list is empty
 */
bglink_t *BG_List_Pop_Head_Link(bglist_t *list) {
  Com_Assert(list != NULL);

  if(list->head) {
    bglink_t *link = list->head;

    Com_Assert(link->list == list);

    list->head = link->next;
    if(list->head) {
      list->head->prev = NULL;
      link->next = NULL;
    } else {
      list->tail = NULL;
    }

    list->length--;
    link->list = NULL;
    return link;
  }

  return NULL;
}

/**
 * BG_List_Peek_Head_Link:
 * @list: a #bglist_t
 * 
 * Returns the first link in @list.
 * 
 * Returns: the first link in @list, or %NULL if @list is empty
 * 
 * Since: 2.4
 */
bglink_t *BG_List_Peek_Head_Link(bglist_t *list) {
  Com_Assert(list != NULL);

  if(list->head) {
    Com_Assert(list->head->list == list);
  }

  return list->head;
}

/**
 * BG_List_Peek_Tail_Link:
 * @list: a #bglist_t
 * 
 * Returns the last link in @list.
 * 
 * Returns: the last link in @list, or %NULL if @list is empty
 * 
 * Since: 2.4
 */
bglink_t *BG_List_Peek_Tail_Link(bglist_t *list) {
  Com_Assert(list != NULL);
  if(list->tail) {
    Com_Assert(list->tail->list == list);
  }

  return list->tail;
}

/**
 * BG_List_Pop_Tail:
 * @list: a #bglist_t
 *
 * Removes the last element of the list and returns its data.
 *
 * Returns: the data of the last element in the list, or %NULL
 *     if the list is empty
 */
void *BG_List_Pop_Tail(bglist_t *list) {
  Com_Assert(list != NULL);

  if(list->tail) {
    bglink_t *link = list->tail;
    void * data = link->data;

    Com_Assert(link->list == list);

    list->tail = link->prev;
    if(list->tail) {
      list->tail->next = NULL;
    } else {
      list->head = NULL;
    }

    list->length--;
    BG_Link_Free_1(link);

    return data;
  }
  
  return NULL;
}

/**
 * BG_List_Pop_nth:
 * @list: a #bglist_t
 * @n: the position of the element
 * 
 * Removes the @n'th element of @list and returns its data.
 * 
 * Returns: the element's data, or %NULL if @n is off the end of @list
 * 
 * Since: 2.4
 */
void *BG_List_Pop_nth(bglist_t *list, size_t n) {
  bglink_t *nth_link;
  void * result;
  
  Com_Assert(list != NULL);

  if(n >= list->length) {
    return NULL;
  }

  nth_link = BG_List_Peek_nth_Link(list, n);
  Com_Assert(nth_link->list == list);
  result = nth_link->data;

  BG_List_Delete_Link(nth_link);

  return result;
}

/**
 * BG_List_Pop_Tail_link:
 * @list: a #bglist_t
 *
 * Removes and returns the last element of the list.
 *
 * Returns: the #bglink_t element at the tail of the list, or %NULL
 *     if the list is empty
 */
bglink_t *BG_List_Pop_Tail_link(bglist_t *list)
{
  Com_Assert(list != NULL);

  if(list->tail) {
    bglink_t *link = list->tail;

    Com_Assert(link->list == list);

    list->tail = link->prev;
    if(list->tail) {
      list->tail->next = NULL;
      link->prev = NULL;
    } else {
      list->head = NULL;
    }

    list->length--;

    link->list = NULL;
    return link;
  }

  return NULL;
}

/**
 * BG_List_Pop_nth_Link:
 * @list: a #bglist_t
 * @n: the link's position
 * 
 * Removes and returns the link at the given position.
 * 
 * Returns: the @n'th link, or %NULL if @n is off the end of @list
 * 
 * Since: 2.4
 */
bglink_t*BG_List_Pop_nth_Link(bglist_t *list, size_t n) {
  bglink_t *link;
  
  Com_Assert(list != NULL);

  if(n >= list->length) {
    return NULL;
  }
  
  link = BG_List_Peek_nth_Link(list, n);
  BG_List_Unlink(link) ;

  return link;
}

/**
 * BG_List_Peek_nth_Link:
 * @list: a #bglist_t
 * @n: the position of the link
 * 
 * Returns the link at the given position
 * 
 * Returns: the link at the @n'th position, or %NULL
 *     if @n is off the end of the list
 * 
 * Since: 2.4
 */
bglink_t *BG_List_Peek_nth_Link(bglist_t *list, size_t n) {
  bglink_t *link;
  int i;
  
  Com_Assert(list != NULL);

  if(n >= list->length) {
    return NULL;
  }
    
  
  if(n > list->length / 2) {
    n = list->length - n - 1;

    link = list->tail;
    for(i = 0; i < n; ++i)
      link = link->prev;
  } else {
    link = list->head;
    for(i = 0; i < n; ++i) {
      link = link->next;
    }
      
  }

  Com_Assert(link->list == list);

  return link;
}

/**
 * BG_List_Link_Index:
 * @list: a #bglist_t
 * @link_: a #bglink_t link
 * 
 * Returns the position of @link in @list.
 * 
 * Returns: the position of @link, or -1 if the link is
 *     not part of @list
 * 
 * Since: 2.4
 */
int BG_List_Link_Index(bglist_t *list, bglink_t  *link) {
  Com_Assert(list != NULL);
  Com_Assert(link->list == list);

  return BG_Link_Position(list->head, link);
}

/**
 * BG_List_Unlink:
 * @link_: a #bglink_t link that must be part of a list
 *
 * Unlinks @link so that it will no longer be part of a list.
 * The link is not freed.
 *
 * @link must be part of a list.
 * 
 * Since: 2.4
 */
void BG_List_Unlink(bglink_t *link) {
  Com_Assert(link != NULL);
  Com_Assert(link->list != NULL);

  if(link == link->list->tail) {
    link->list->tail = link->list->tail->prev;
  }

  link->list->head = BG_Link_Remove_Link(link->list->head, link);
  link->list->length--;
  link->list = NULL;
}

/**
 * BG_List_Delete_Link:
 * @link_: a #bglink_t link that must be part of a list
 *
 * Removes @link from a list and frees it.
 *
 * @link must be part of a list.
 * 
 * Since: 2.4
 */
void BG_List_Delete_Link(bglink_t  *link) {
  BG_List_Unlink(link);
  BG_Link_Free(link);
}

/**
 * BG_List_Peek_Head:
 * @list: a #bglist_t
 *
 * Returns the first element of the list.
 *
 * Returns: the data of the first element in the list, or %NULL
 *     if the list is empty
 */
void *BG_List_Peek_Head(bglist_t *list) {
  Com_Assert(list != NULL);
  if( list->head )
    Com_Assert(list->head->list == list);

  return list->head ? list->head->data : NULL;
}

/**
 * BG_List_Peek_Tail:
 * @list: a #bglist_t
 *
 * Returns the last element of the list.
 *
 * Returns: the data of the last element in the list, or %NULL
 *     if the list is empty
 */
void *BG_List_Peek_Tail(bglist_t *list) {
  Com_Assert(list != NULL);

  if(list->tail) {
    Com_Assert(list->tail->list == list);
  }

  return list->tail ? list->tail->data : NULL;
}

/**
 * BG_List_Peek_nth:
 * @list: a #bglist_t
 * @n: the position of the element
 * 
 * Returns the @n'th element of @list. 
 * 
 * Returns: the data for the @n'th element of @list,
 *     or %NULL if @n is off the end of @list
 * 
 * Since: 2.4
 */
void *BG_List_Peek_nth(bglist_t *list, size_t n) {
  bglink_t *link;
  
  Com_Assert(list != NULL);

  link = BG_List_Peek_nth_Link(list, n);

  Com_Assert(link->list == list);

  if(link) {
    return link->data;
  }

  return NULL;
}

/**
 * BG_List_Index:
 * @list: a #bglist_t
 * @data: the data to find
 * 
 * Returns the position of the first element in @list which contains @data.
 * 
 * Returns: the position of the first element in @list which
 *     contains @data, or -1 if no element in @list contains @data
 * 
 * Since: 2.4
 */
int BG_List_Index(bglist_t *list, const void *data) {
  Com_Assert(list != NULL);

  return BG_Link_Index(list->head, data);
}

/**
 * BG_List_Concat:
 * @list1: The target list that has the beginning links
 * @list2: the second list that has ending links
 * 
 * Concats the links from @list2 to the end of @list1. @list1 then contains
 * all of the links that were in both lists and @list2 becomes empty as a
 * result.
 */
void BG_List_Concat(bglist_t *list1, bglist_t *list2) {
  bglink_t *link;

  Com_Assert(list1 != NULL);
  Com_Assert(list2 != NULL);

  while((link = BG_List_Pop_Head_Link(list2))) {
    BG_List_Push_Tail_Link(list1, link);
  }
}

/**
 * BG_List_Remove:
 * @list: a #bglist_t
 * @data: the data to remove
 * 
 * Removes the first element in @list that contains @data. 
 * 
 * Returns: %qtrue if @data was found and removed from @list
 *
 * Since: 2.4
 */
qboolean BG_List_Remove( bglist_t *list, const void *data ){
  bglink_t *link;
  
  Com_Assert(list != NULL);

  link = BG_Link_Find(list->head, data);

  if(link) {
    BG_List_Delete_Link(link);
  }
    

  return (link != NULL);
}

/**
 * BG_List_Remove_All:
 * @list: a #bglist_t
 * @data: the data to remove
 * 
 * Remove all elements whose data equals @data from @list.
 * 
 * Returns: the number of elements removed from @list
 *
 * Since: 2.4
 */
size_t BG_List_Remove_All(bglist_t *list, const void *data) {
  bglink_t *link;
  size_t old_length;
  
  Com_Assert(list != NULL);

  old_length = list->length;

  link = list->head;
  while(link) {
    bglink_t *next = link->next;

    if(link->data == data) {
      BG_List_Delete_Link(link);
    }
    link = next;
  }

  return (old_length - list->length);
}

/**
 * BG_List_Insert_Before:
 * @list: a #bglist_t
 * @sibling: (nullable): a #bglink_t link that must be part of @list, or %NULL to
 *   push at the tail of the list.
 * @data: the data to insert
 * 
 * Inserts @data into @list before @sibling.
 *
 * @sibling must be part of @list. Since GLib 2.44 a %NULL sibling pushes the
 * data at the tail of the list.
 * 
 * Since: 2.4
 */
void BG_List_Insert_Before(bglist_t *list, bglink_t *sibling, void *data) {
  Com_Assert(list != NULL);

  if(sibling == NULL) {
    /* We don't use _BG_List_Insert_Before() with a NULL sibling because it
     * would be a O(n) operation and we would need to update manually the tail
     * pointer.
     */
    BG_List_Push_Tail(list, data);
  } else {
    _BG_List_Insert_Before(list, sibling, data);
  }
}

/**
 * BG_List_Insert_After:
 * @list: a #bglist_t
 * @sibling: (nullable): a #bglink_t link that must be part of @list, or %NULL to
 *   push at the head of the list.
 * @data: the data to insert
 *
 * Inserts @data into @list after @sibling.
 *
 * @sibling must be part of @list. Since GLib 2.44 a %NULL sibling pushes the
 * data at the head of the list.
 * 
 * Since: 2.4
 */
void BG_List_Insert_After(bglist_t *list, bglink_t *sibling, void *data) {
  Com_Assert(list != NULL);

  if(sibling == NULL) {
    BG_List_Push_Head( list, data );
  } else {
    BG_List_Insert_Before(list, sibling->next, data);
  }
}

/**
 * BG_List_Insert_Sorted:
 * @list: a #bglist_t
 * @data: the data to insert
 * @func: the #BG_CompareDataFunc used to compare elements in the list. It is
 *     called with two elements of the @list and @user_data. It should
 *     return 0 if the elements are equal, a negative value if the first
 *     element comes before the second, and a positive value if the second
 *     element comes before the first.
 * @user_data: user data passed to @func
 * 
 * Inserts @data into @list using @func to determine the new position.
 * 
 * Since: 2.4
 */
void BG_List_Insert_Sorted(
  bglist_t *list, void * data, BG_CompareDataFunc func, void *user_data) {
  bglink_t *link;
  
  Com_Assert(list != NULL);

  link = list->head;
  while(link && func(link->data, data, user_data) < 0) {
    link = link->next;
  }

  BG_List_Insert_Before(list, link, data);
}

/*
--------------------------------------------------------------------------------
*/

/*
--------------------------------------------------------------------------------
BG_Link functions
*/

/**
 * SECTION:linked_lists_double links
 * @title: Doubly-Linked Lists Links
 * @short_description: linked lists that can be iterated over in both directions
 *
 * The #bglink_t structure and its associated functions provide a standard
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
 * [bglist_t][glib-Double-ended-Queues] instead.
 *
 * The data contained in each element are pointers to any type of data.
 *
 * List elements are allocated from the [slice allocator][glib-Memory-Slices],
 * which is more efficient than allocating elements individually.
 *
 * Note that most of the #bglink_t functions expect to be passed a pointer
 * to the first element in the list. The functions which insert
 * elements return the new start of the list, which may have changed.
 *
 * There is no function to create a #bglink_t. %NULL is considered to be
 * a valid, empty list so you simply set a #bglink_t* to %NULL to initialize
 * it.
 *
 * To add elements, use BG_Link_Append(), BG_Link_Prepend(),
 * BG_Link_Insert() and BG_Link_Insert_Sorted().
 *
 * To visit all elements in the list, use a loop over the list:
 * |[<!-- language="C" -->
 * bglink_t *l;
 * for( l = list; l != NULL; l = l->next )
 *   {
 *     // do something with l->data
 *   }
 * ]|
 *
 *
 * To loop over the list and modify it (e.g. remove a certain element)
 * a while loop is more appropriate, for example:
 * |[<!-- language="C" -->
 * bglink_t *l = list;
 * while( l != NULL )
 *   {
 *     bglink_t *next = l->next;
 *     if( should_be_removed( l ) )
 *       {
 *         // possibly free l->data
 *         list = BG_Link_Delete_Link( list, l );
 *       }
 *     l = next;
 *   }
 * ]|
 *
 * To navigate in a list, use BG_Link_First(), BG_Link_Last(),
 * BG_Link_Next(), BG_Link_Previous().
 *
 * To find elements in the list use BG_Link_Find() and BG_Link_Find_Custom().
 *
 * To find the index of an element use BG_Link_Position() and
 * BG_Link_Index().
 *
 * To free the entire list, use BG_Link_Free() or BG_Link_Free_Full().
 */
/**
 * BG_Link_Previous:
 * @list: an element in a #bglink_t
 *
 * A convenience macro to get the previous element in a #bglink_t.
 * Note that it is considered perfectly acceptable to access
 * @list->previous directly.
 *
 * Returns: the previous element, or %NULL if there are no previous
 *          elements
 **/

/**
 * BG_Link_Next:
 * @list: an element in a #bglink_t
 *
 * A convenience macro to get the next element in a #bglink_t.
 * Note that it is considered perfectly acceptable to access
 * @list->next directly.
 *
 * Returns: the next element, or %NULL if there are no more elements
 **/

 /*
 --------------------------------------------------------------------------------
 Custom Dynamic Memory Management for list links
 */
#ifdef GAME
# define  MAX_LINKS (8 * 1024)
#else
#ifdef CGAME
# define  MAX_LINKS (8 * 1024)
#else
# define  MAX_LINKS (16 * 1024)
#endif
#endif

allocator_protos(bglink)
allocator(bglink, sizeof(bglink_t), MAX_LINKS)

/*
===============
BG_Link_Init_Memory

Executed in BG_InitMemory()
===============
*/
void BG_Link_Init_Memory(char *calledFile, int calledLine) {
  initPool_bglink(calledFile, calledLine);
}

/*
===============
_BG_Link_Alloc0

Returns an allocated list link initialized to NULL
===============
*/
static ID_INLINE bglink_t *_BG_Link_Alloc0(void) {
  return memset(alloc_bglink(__FILE__, __LINE__), 0, sizeof(bglink_t));
}

/*
===============
BG_Link_Memory_Info

Displays info related to the allocation of list links
===============
*/
void BG_Link_Memory_Info(void) {
  memoryInfo_bglink( );
}

/*
 --------------------------------------------------------------------------------
*/

/**
 * BG_Link_Alloc:
 *
 * Allocates space for one #bglink_t element. It is called by
 * BG_Link_Append(), BG_Link_Prepend(), BG_Link_Insert() and
 * BG_Link_Insert_Sorted() and so is rarely used on its own.
 *
 * Returns: a pointer to the newly-allocated #bglink_t element
 **/
bglink_t *BG_Link_Alloc(void) {
  return _BG_Link_Alloc0( );
}

/**
 * BG_Link_Free:
 * @link: a #bglink_t
 *
 * Frees all of the memory used by a #bglink_t.
 * The freed elements are returned to the slice allocator.
 *
 * If list elements contain dynamically-allocated memory, you should
 * either use BG_Link_Free_Full() or free them manually first.
 */
static void BG_Link_Free(bglink_t *link) {
  bglink_t *tmp;

  while(link) {
    tmp = link->next;
    free_bglink(link, __FILE__, __LINE__);
    link = tmp;
  }
}

/**
 * BG_Link_Free_1:
 * @link: a #bglink_t element
 *
 * Frees one #bglink_t element, but does not update links from the next and
 * previous elements in the list, so you should not call this function on an
 * element that is currently part of a list.
 *
 * It is usually used after BG_Link_Remove_Link().
 */
/**
 * BG_Link_Free1:
 *
 * Another name for BG_Link_Free_1().
 **/
static void BG_Link_Free_1(bglink_t *link) {
  link->list = NULL;
  free_bglink(link, __FILE__, __LINE__);
}

/**
 * BG_Link_Free_Full:
 * @link: a pointer to a #bglink_t
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #bglink_t,
 * and calls @free_func on every element's data.
 *
 * Since: 2.28
 */
static void BG_Link_Free_Full(bglink_t *link, BG_Destroy  free_func) {
  bglink_t *tmp;

  while(link) {
    tmp = link->next;
    free_func(link->data);
    free_bglink(link, __FILE__, __LINE__);
    link = tmp;
  }
}

/**
 * BG_Link_Append:
 * @link: a pointer to a #bglink_t
 * @data: the data for the new element
 *
 * Adds a new element on to the end of the list.
 *
 * Note that the return value is the new start of the list,
 * if @link was empty; make sure you store the new value.
 *
 * BG_Link_Append() has to traverse the entire list to find the end,
 * which is inefficient when adding multiple elements. A common idiom
 * to avoid the inefficiency is to use BG_Link_Prepend() and reverse
 * the list with BG_Link_Reverse() when all elements have been added.
 *
 * |[<!-- language="C" -->
 * // Notice that these are initialized to the empty list.
 * bglink_t *strinBG_Head = NULL, *number_head = NULL;
 *
 * // This is a list of strings.
 * strinBG_Head = BG_Link_Append( strinBG_Head, "first" );
 * strinBG_Head = BG_Link_Append( strinBG_Head, "second" );
 *
 * // This is a list of integers.
 * number_head = BG_Link_Append( number_head, GINT_TO_POINTER(27) );
 * number_head = BG_Link_Append( number_head, GINT_TO_POINTER(14) );
 * ]|
 *
 * Returns: either @link or the new start of the #bglink_t if @link was %NULL
 */
static bglink_t *BG_Link_Append(bglink_t *link, void *data) {
  bglink_t *new_link;
  bglink_t *last;

  new_link = _BG_Link_Alloc0( );
  new_link->data = data;
  new_link->next = NULL;

  if(link) {
    last = BG_Link_Last(link);
    /* Com_Assert( last != NULL ); */
    last->next = new_link;
    new_link->prev = last;

    return link;
  } else {
    new_link->prev = NULL;
    return new_link;
  }
}

static bglink_t *_BG_Link_Prepend(bglink_t *head, void *data) {
  bglink_t *new_head;

  new_head = _BG_Link_Alloc0( );
  new_head->data = data;
  new_head->next = head;

  if(head) {
    new_head->prev = head->prev;
    if(head->prev) {
      head->prev->next = new_head;
    }
    head->prev = new_head;
  } else {
    new_head->prev = NULL;
  }

  return new_head;
}

/**
 * BG_Link_Prepend:
 * @head: a pointer to a #bglink_t, this must point to the top of the list
 * @data: the data for the new element
 *
 * Prepends a new element on to the start of the list.
 *
 * Note that the return value is the new start of the list,
 * which will have changed, so make sure you store the new value.
 *
 * |[<!-- language="C" -->
 * // Notice that it is initialized to the empty link.
 * bglink_t *list = NULL;
 *
 * list = BG_Link_Prepend( list, "last" );
 * list = BG_Link_Prepend( list, "first" );
 * ]|
 *
 * Do not use this function to prepend a new element to a different
 * element than the start of the list. Use BG_Link_Insert_Before() instead.
 *
 * Returns: a pointer to the newly prepended element, which is the new
 *     start of the #bglink_t
 */
bglink_t *BG_Link_Prepend(bglink_t *head, void *data) {
  if(head && head->list) {
    int head_index = BG_List_Link_Index(head->list, head);

    Com_Assert(BG_List_Valid(head->list));
    Com_Assert(head_index > -1);

    return BG_List_Push_nth(head->list, data, head_index);
  } else {
    return _BG_Link_Prepend(head, data);
  }
}

/**
 * _BG_List_Insert_Before:
 * @list: a pointer to a #bglist_t
 * @sibling: the list element before which the new element
 *     is inserted or %NULL to insert at the end of the list
 * @data: the data for the new element
 *
 * Inserts a new element into the list before the given position.
 */
static void _BG_List_Insert_Before(
  bglist_t *list, bglink_t *sibling, void *data) {
  Com_Assert(list != NULL);

  if(!list->head) {
    list->tail = list->head = BG_Link_Alloc();
    list->head->list = list;
    list->head->data = data;
    Com_Assert(sibling == NULL);
  } else if(sibling) {
    bglink_t *link;

    Com_Assert(sibling->list == list);

    link = alloc_bglink(__FILE__, __LINE__);
    link->list = list;
    link->data = data;
    link->prev = sibling->prev;
    link->next = sibling;
    sibling->prev = link;
    if(link->prev) {
      link->prev->next = link;
    } else {
      list->head = link;
    }
  } else {
    bglink_t *last;

    last = list->head;
    while(last->next) {
      last = last->next;
    }

    last->next = alloc_bglink(__FILE__, __LINE__);
    last->next->list = list;
    last->next->data = data;
    last->next->prev = last;
    last->next->next = NULL;
    list->tail = last->next;
  }

  list->length++;
}

static ID_INLINE bglink_t *_BG_Link_Remove_Link(bglink_t *head, bglink_t *link) {
  if(link == NULL) {
    return head;
  }

  if(link->prev) {
    if(link->prev->next == link) {
      link->prev->next = link->next;
    } else {
      Com_Error( ERR_DROP,
             "_BG_Link_Remove_Link: corrupted double-linked list detected!" );
    }
  }
  if(link->next) {
    if(link->next->prev == link) {
      link->next->prev = link->prev;
    } else {
      Com_Error( ERR_DROP,
             "_BG_Link_Remove_Link: corrupted double-linked list detected!" );
    }
  }

  if(link == head) {
    head = head->next;
  }

  link->next = NULL;
  link->prev = NULL;

  return head;
}

/**
 * BG_Link_Remove_Link:
 * @head: a #bglink_t, this must point to the top of the list
 * @llink: an element in the #bglink_t
 *
 * Removes an element from a #bglink_t, without freeing the element.
 * The removed element's prev and next links are set to %NULL, so
 * that it becomes a self-contained list with one element.
 *
 * This function is for example used to move an element in the list
 * or to remove an element in the list before freeing its data:
 * |[<!-- language="C" -->
 * link = BG_Link_Remove_Link( list, llink );
 * free_some_data_that_may_access_the_link_again( llink->data );
 * BG_Link_Free( llink );
 * ]|
 *
 * Returns: the (possibly changed) start of the #bglink_t
 */
static bglink_t *BG_Link_Remove_Link(bglink_t *head, bglink_t *llink) {
  return _BG_Link_Remove_Link(head, llink);
}

/**
 * BG_Link_Delete_Link:
 * @list: a #bglink_t, this must point to the top of the list
 * @link_: node to delete from @list
 *
 * Removes the node link_ from the list and frees it.
 * Compare this to BG_Link_Remove_Link() which removes the node
 * without freeing it.
 *
 * Returns: the (possibly changed) start of the #bglink_t
 */
bglink_t *BG_Link_Delete_Link(bglink_t *list, bglink_t *link_) {
  bglink_t *returned_link = NULL;

  Com_Assert(list);
  Com_Assert(link_);
  Com_Assert(BG_Link_Position(list, link_) > -1);

  if(link_->list) {
    Com_Assert(BG_List_Valid(link_->list));
    Com_Assert(BG_List_Link_Index(link_->list, link_) > -1);
    Com_Assert(BG_List_Link_Index(link_->list, list) > -1);

    if(list == link_) {
      returned_link = link_->next;
    } else {
      returned_link = list;
    }

    if(link_ == link_->list->tail) {
      link_->list->tail = link_->list->tail->prev;
    }

    link_->list->head = BG_Link_Remove_Link(link_->list->head, link_);
    link_->list->length--;
    link_->list = NULL;
  } else {
    returned_link = _BG_Link_Remove_Link(list, link_);
  }

  link_->list = NULL;
  free_bglink(link_, __FILE__, __LINE__);

  return returned_link;
}

/**
 * BG_Link_Reverse:
 * @head: a #bglink_t, this must point to the top of the list
 *
 * Reverses a #bglink_t.
 * It simply switches the next and prev pointers of each element.
 *
 * Returns: the start of the reversed #bglink_t
 */
static bglink_t *BG_Link_Reverse(bglink_t *head) {
  bglink_t *last;

  last = NULL;
  while(head) {
    last = head;
    head = last->next;
    last->next = last->prev;
    last->prev = head;
  }

  return last;
}

/**
 * BG_Link_nth_Prev:
 * @link: a #bglink_t
 * @n: the position of the element, counting from 0
 *
 * Gets the element @n places before @link.
 *
 * Returns: the element, or %NULL if the position is
 *     off the end of the #bglink_t
 */
bglink_t *BG_Link_nth_Prev(bglink_t *link, unsigned int  n) {
  while((n-- > 0) && link) {
    link = link->prev;
  }

  return link;
}

/**
 * BG_Link_Find:
 * @head: a #bglink_t, this must point to the top of the list
 * @data: the element data to find
 *
 * Finds the element in a #bglink_t which contains the given data.
 *
 * Returns: the found #bglink_t element, or %NULL if it is not found
 */
static bglink_t *BG_Link_Find(bglink_t *head, const void *data) {
  while(head) {
    if(head->data == data) {
      break;
    }
    head = head->next;
  }

  return head;
}

/**
 * BG_Link_Find_Custom:
 * @head: a #bglink_t, this must point to the top of the list
 * @data: user data passed to the function
 * @func: the function to call for each element.
 *     It should return 0 when the desired element is found
 *
 * Finds an element in a #bglink_t, using a supplied function to
 * find the desired element. It iterates over the list, calling
 * the given function which should return 0 when the desired
 * element is found. The function takes two #const void * arguments,
 * the #bglink_t element's data as the first argument and the
 * given user data.
 *
 * Returns: the found #bglink_t element, or %NULL if it is not found
 */
static bglink_t *BG_Link_Find_Custom(
  bglink_t *head, const void *data, BG_CompareFunc func) {
  Com_Assert(func != NULL);

  while(head) {
    if(!func(head->data, data)) {
      return head;
    }
    head = head->next;
  }

  return NULL;
}

/**
 * BG_Link_Position:
 * @head: a #bglink_t, this must point to the top of the list
 * @llink: an element in the #bglink_t
 *
 * Gets the position of the given element
 * in the #bglink_t (starting from 0).
 *
 * Returns: the position of the element in the #bglink_t,
 *     or -1 if the element is not found
 */
static int BG_Link_Position(bglink_t *head, bglink_t *llink) {
  int i;

  i = 0;
  while(head) {
    if(head == llink) {
      return i;
    }
    i++;
    head = head->next;
  }

  return -1;
}

/**
 * BG_Link_Index:
 * @head: a #bglink_t, this must point to the top of the list
 * @data: the data to find
 *
 * Gets the position of the element containing
 * the given data (starting from 0).
 *
 * Returns: the index of the element containing the data,
 *     or -1 if the data is not found
 */
static int BG_Link_Index(bglink_t *head, const void *data) {
  int i;

  i = 0;
  while(head) {
    if(head->data == data) {
      return i;
    }
    i++;
    head = head->next;
  }

  return -1;
}

/**
 * BG_Link_Last:
 * @list: any #bglink_t element
 *
 * Gets the last element in a #bglink_t.
 *
 * Returns: the last element in the #bglink_t,
 *     or %NULL if the #bglink_t has no elements
 */
static bglink_t * BG_Link_Last(bglink_t *link) {
  if(link) {
    while(link->next) {
      link = link->next;
    }
  }

  return link;
}

/**
 * BG_Link_First:
 * @link: any #bglink_t element
 *
 * Gets the first element in a #bglink_t.
 *
 * Returns: the first element in the #bglink_t,
 *     or %NULL if the #bglink_t has no elements
 */
bglink_t *BG_Link_First(bglink_t *link) {
  if(link) {
    while(link->prev) {
      link = link->prev;
    }
  }

  return link;
}

static bglink_t *BG_Link_Sort_merge(
  bglink_t *l1, bglink_t *l2, BG_Func compare_func, void *user_data) {
  bglink_t list, *l, *lprev;
  int cmp;

  l = &list;
  lprev = NULL;

  while(l1 && l2) {
    cmp = ((BG_CompareDataFunc) compare_func) (l1->data, l2->data, user_data);

    if(cmp <= 0) {
      l->next = l1;
      l1 = l1->next;
    } else {
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

/**
 * BG_Link_Foreach:
 * @list: a #bglink_t, this must point to the top of the list
 * @func: the function to call with each element's data
 * @user_data: user data to pass to the function
 *
 * Calls a function for each element of a #bglink_t.
 */
/**
 * BG_Func:
 * @data: the element's data
 * @user_data: user data passed to BG_Link_Foreach() or g_slist_foreach()
 *
 * Specifies the type of functions passed to BG_Link_Foreach() and
 * g_slist_foreach().
 */
void BG_Link_Foreach(bglink_t *list, BG_Func func, void *user_data) {
  while(list) {
    bglink_t *next = list->next;
    (*func)( list->data, user_data);
    list = next;
  }
}

static bglink_t *BG_Link_Sort_real(
  bglink_t *head, BG_Func compare_func, void *user_data) {
  bglink_t *l1, *l2;

  if (!head)
    return NULL;
  if (!head->next)
    return head;

  l1 = head;
  l2 = head->next;

  while((l2 = l2->next) != NULL) {
    if((l2 = l2->next) == NULL) {
      break;
    }
    l1 = l1->next;
  }
  l2 = l1->next;
  l1->next = NULL;

  return BG_Link_Sort_merge(
    BG_Link_Sort_real(head, compare_func, user_data),
    BG_Link_Sort_real(l2, compare_func, user_data),
    compare_func, user_data);
}

/**
 * BG_Link_Sort_With_Data:
 * @head: a #bglink_t, this must point to the top of the list
 * @compare_func: the comparison function used to sort the #bglink_t.
 *     This function is passed the data from 2 elements of the #bglink_t
 *     and should return 0 if they are equal, a negative value if the
 *     first element comes before the second, or a positive value if
 *     the first element comes after the second.
 * @user_data: user data to pass to comparison function
 *
 * Sorts a #bglink_t using the given comparison function. The algorithm
 * used is a stable sort. The comparison function accepts
 * a user data argument.
 *
 * Returns: the (possibly changed) start of the #bglink_t
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
static bglink_t *BG_Link_Sort_With_Data(
  bglink_t *head, BG_CompareDataFunc compare_func, void *user_data) {
  return BG_Link_Sort_real(head, (BG_Func) compare_func, user_data);
}
/*
--------------------------------------------------------------------------------
*/
