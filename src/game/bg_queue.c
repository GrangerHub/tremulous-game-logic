/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * bgqueue_t: Double ended queue implementation, piggy backed on bglist_t.
 * Copyright (C) 1998 Tim Janik
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
 * MT safe
 */

/**
 * SECTION:queue
 * @Title: Double-ended Queues
 * @Short_description: double-ended queue data structure
 *
 * The #bgqueue_t structure and its associated functions provide a standard
 * queue data structure. Internally, bgqueue_t uses the same data structure
 * as #bglist_t to store elements.
 *
 * The data contained in each element are pointers to any type of data.
 *
 * To create a new bgqueue_t, use BG_Queue_New().
 *
 * To initialize a statically-allocated bgqueue_t, use #BG_QUEUE_INIT or
 * BG_Queue_Init().
 *
 * To add elements, use BG_Queue_Push_Head(), BG_Queue_Push_Head_Link(),
 * BG_Queue_Push_Tail() and BG_Queue_Push_Tail_Link().
 *
 * To remove elements, use BG_Queue_Pop_Head() and BG_Queue_Pop_Tail().
 *
 * To free the entire queue, use BG_Queue_Free().
 */

 #include "../qcommon/q_shared.h"
 #include "bg_public.h"

 /*
 --------------------------------------------------------------------------------
 Custom Dynamic Memory Management for queues
 */
  #ifdef GAME
  # define  MAX_QUEUES 1024
  #else
  #ifdef CGAME
  # define  MAX_QUEUES 1024
  #else
  # define  MAX_QUEUES 128
  #endif
  #endif

  
  allocator_protos( bgqueue )
  allocator( bgqueue, sizeof(bgqueue_t), MAX_QUEUES )
  
 /*
 ===============
 BG_Queue_Init_Memory

 Executed in BG_InitMemory()
 ===============
 */
 void BG_Queue_Init_Memory( char *calledFile, int calledLine )
 {
   initPool_bgqueue( calledFile, calledLine );
 }

 /*
 ===============
 _BG_List_Alloc0

 Returns an allocated list initialized to NULL
 ===============
 */
 static ID_INLINE bgqueue_t *_BG_Queue_Alloc0( void )
 {
   return memset( alloc_bgqueue( __FILE__, __LINE__ ), 0, sizeof( bgqueue_t ) );
 }

 /*
 ===============
 BG_List_Memory_Info

 Displays info related to the allocation of linked lists
 ===============
 */
 void BG_Queue_Memory_Info( void )
 {
   memoryInfo_bgqueue(  );
 }

 /*
 --------------------------------------------------------------------------------
 */

/**
 * BG_Queue_New:
 *
 * Creates a new #bgqueue_t.
 *
 * Returns: a newly allocated #bgqueue_t
 **/
bgqueue_t *BG_Queue_New( void )
{
  return _BG_Queue_Alloc0( );
}

/**
 * BG_Queue_Free:
 * @queue: a #bgqueue_t
 *
 * Frees the memory allocated for the #bgqueue_t. Only call this function
 * if @queue was created with BG_Queue_New(). 
 *
 * If queue elements contain dynamically-allocated memory, you should
 * either use BG_Queue_Free_Full() or free them manually first.
 **/
void BG_Queue_Free ( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  BG_List_Free ( queue->head );
  free_bgqueue( queue, __FILE__, __LINE__ );
}

/**
 * BG_Queue_Free_Full:
 * @queue: a pointer to a #bgqueue_t
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #bgqueue_t,
 * and calls the specified destroy function on every element's data.
 *
 * Since: 2.32
 */
void BG_Queue_Free_Full ( bgqueue_t *queue, BG_DestroyNotify free_func )
{
  BG_Queue_Foreach (queue, (BG_Func) free_func, NULL);
  BG_Queue_Free (queue);
}

/**
 * BG_Queue_Init:
 * @queue: an uninitialized #bgqueue_t
 *
 * A statically-allocated #bgqueue_t must be initialized with this function
 * before it can be used. Alternatively you can initialize it with
 * #BG_QUEUE_INIT. It is not necessary to initialize queues created with
 * BG_Queue_New().
 *
 * Since: 2.14
 */
void BG_Queue_Init( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  queue->head = queue->tail = NULL;
  queue->length = 0;
}

/**
 * BG_Queue_Clear:
 * @queue: a #bgqueue_t
 *
 * Removes all the elements in @queue. If queue elements contain
 * dynamically-allocated memory, they should be freed first, or use 
 * BG_Queue_Clear_Full().
 *
 * Since: 2.14
 */
void BG_Queue_Clear( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  BG_List_Free( queue->head );
  BG_Queue_Init( queue );
}

/**
 * BG_Queue_Free_Full:
 * @queue: a pointer to a #bgqueue_t
 * @free_func: the function to be called to free each element's data
 *
 * Removes all the elements in @queue, and calls the specified destroy
 * function on every element's data.
 *
 */
void BG_Queue_Clear_Full( bgqueue_t *queue, BG_DestroyNotify free_func )
{
  Com_Assert( queue != NULL );

  BG_List_Free_Full( queue->head, free_func );
  BG_Queue_Init( queue );
}

/**
 * BG_Queue_Is_Empty:
 * @queue: a #bgqueue_t.
 *
 * Returns: %TRUE if the queue is empty
 */
qboolean BG_Queue_Is_Empty( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  return queue->head == NULL;
}

/**
 * BG_Queue_Get_Length:
 * @queue: a #bgqueue_t
 * 
 * Returns: the number of items in @queue
 * 
 * Since: 2.4
 */
size_t BG_Queue_Get_Length( bgqueue_t *queue )
{
  Com_Assert(  queue != NULL );

  return queue->length;
}

/**
 * BG_Queue_Reverse:
 * @queue: a #bgqueue_t
 * 
 * Reverses the order of the items in @queue.
 * 
 * Since: 2.4
 */
void BG_Queue_Reverse( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  queue->tail = queue->head;
  queue->head = BG_List_Reverse( queue->head );
}

/**
 * BG_Queue_Copy:
 * @queue: a #bgqueue_t
 * 
 * Copies a @queue. Note that is a shallow copy. If the elements in the
 * queue consist of pointers to data, the pointers are copied, but the
 * actual data is not.
 * 
 * Returns: a copy of @queue
 * 
 * Since: 2.4
 */
bgqueue_t *BG_Queue_Copy( bgqueue_t *queue )
{
  bgqueue_t *result;
  bglist_t *list;

  Com_Assert( queue != NULL );

  result = BG_Queue_New( );

  for( list = queue->head; list != NULL; list = list->next )
    BG_Queue_Push_Tail( result, list->data );

  return result;
}

/**
 * BG_Queue_Foreach:
 * @queue: a #bgqueue_t
 * @func: the function to call for each element's data
 * @user_data: user data to pass to @func
 * 
 * Calls @func for each element in the queue passing @user_data to the
 * function.
 * 
 * Since: 2.4
 */
void BG_Queue_Foreach( bgqueue_t *queue, BG_Func func, void * user_data )
{
  bglist_t *list;

  Com_Assert( queue != NULL );
  Com_Assert( func != NULL );
  
  list = queue->head;
  while( list )
    {
      bglist_t *next = list->next;
      func( list->data, user_data );
      list = next;
    }
}

/**
 * BG_Queue_Find:
 * @queue: a #bgqueue_t
 * @data: data to find
 * 
 * Finds the first link in @queue which contains @data.
 * 
 * Returns: the first link in @queue which contains @data
 * 
 * Since: 2.4
 */
bglist_t *BG_Queue_Find( bgqueue_t *queue, const void *data )
{
  Com_Assert( queue != NULL );

  return BG_List_Find( queue->head, data );
}

/**
 * BG_Queue_Find_Custom:
 * @queue: a #bgqueue_t
 * @data: user data passed to @func
 * @func: a #BG_CompareFunc to call for each element. It should return 0
 *     when the desired element is found
 *
 * Finds an element in a #bgqueue_t, using a supplied function to find the
 * desired element. It iterates over the queue, calling the given function
 * which should return 0 when the desired element is found. The function
 * takes two const void * arguments, the #bgqueue_t element's data as the
 * first argument and the given user data as the second argument.
 * 
 * Returns: the found link, or %NULL if it wasn't found
 * 
 * Since: 2.4
 */
bglist_t * BG_Queue_Find_Custom( bgqueue_t *queue, const void *data,
                                 BG_CompareFunc func )
{
  Com_Assert( queue != NULL );
  Com_Assert( func != NULL );

  return BG_List_Find_Custom( queue->head, data, func );
}

/**
 * BG_Queue_Sort:
 * @queue: a #bgqueue_t
 * @compare_func: the #BG_CompareDataFunc used to sort @queue. This function
 *     is passed two elements of the queue and should return 0 if they are
 *     equal, a negative value if the first comes before the second, and
 *     a positive value if the second comes before the first.
 * @user_data: user data passed to @compare_func
 * 
 * Sorts @queue using @compare_func. 
 * 
 * Since: 2.4
 */
void BG_Queue_Sort( bgqueue_t *queue, BG_CompareDataFunc compare_func,
                    void *user_data )
{
  Com_Assert( queue != NULL );
  Com_Assert( compare_func != NULL );

  queue->head = BG_List_Sort_With_Data( queue->head, compare_func, user_data );
  queue->tail = BG_List_Last( queue->head );
}

/**
 * BG_Queue_Push_Head:
 * @queue: a #bgqueue_t.
 * @data: the data for the new element.
 *
 * Adds a new element at the head of the queue.
 */
void BG_Queue_Push_Head( bgqueue_t *queue, void *data )
{
  Com_Assert( queue != NULL );

  queue->head = BG_List_Prepend( queue->head, data );
  if( !queue->tail )
    queue->tail = queue->head;
  queue->length++;
}

/**
 * BG_Queue_Push_nth:
 * @queue: a #bgqueue_t
 * @data: the data for the new element
 * @n: the position to insert the new element. If @n is negative or
 *     larger than the number of elements in the @queue, the element is
 *     added to the end of the queue.
 * 
 * Inserts a new element into @queue at the given position.
 * 
 * Since: 2.4
 */
void BG_Queue_Push_nth( bgqueue_t *queue, void *data, int n )
{
  Com_Assert( queue != NULL );

  if( n < 0 || n >= queue->length )
    {
      BG_Queue_Push_Tail( queue, data );
      return;
    }

  BG_Queue_Insert_Before( queue, BG_Queue_Peek_nth_Link( queue, n ), data );
}

/**
 * BG_Queue_Push_Head_Link:
 * @queue: a #bgqueue_t
 * @link_: a single #bglist_t element, not a list with more than one element
 *
 * Adds a new element at the head of the queue.
 */
void
BG_Queue_Push_Head_Link (bgqueue_t *queue,
                        bglist_t  *link)
{
  Com_Assert (queue != NULL);
  Com_Assert (link != NULL);
  Com_Assert (link->prev == NULL);
  Com_Assert (link->next == NULL);

  link->next = queue->head;
  if(queue->head)
    queue->head->prev = link;
  else
    queue->tail = link;
  queue->head = link;
  queue->length++;
}

/**
 * BG_Queue_Push_Tail:
 * @queue: a #bgqueue_t
 * @data: the data for the new element
 *
 * Adds a new element at the tail of the queue.
 */
void
BG_Queue_Push_Tail (bgqueue_t   *queue,
                   void *  data)
{
  Com_Assert (queue != NULL);

  queue->tail = BG_List_Append (queue->tail, data);
  if(queue->tail->next)
    queue->tail = queue->tail->next;
  else
    queue->head = queue->tail;
  queue->length++;
}

/**
 * BG_Queue_Push_Tail_Link:
 * @queue: a #bgqueue_t
 * @link_: a single #bglist_t element, not a list with more than one element
 *
 * Adds a new element at the tail of the queue.
 */
void
BG_Queue_Push_Tail_Link (bgqueue_t *queue,
                        bglist_t  *link)
{
  Com_Assert (queue != NULL);
  Com_Assert (link != NULL);
  Com_Assert (link->prev == NULL);
  Com_Assert (link->next == NULL);

  link->prev = queue->tail;
  if(queue->tail)
    queue->tail->next = link;
  else
    queue->head = link;
  queue->tail = link;
  queue->length++;
}

/**
 * BG_Queue_Push_nth_Link:
 * @queue: a #bgqueue_t
 * @n: the position to insert the link. If this is negative or larger than
 *     the number of elements in @queue, the link is added to the end of
 *     @queue.
 * @link_: the link to add to @queue
 * 
 * Inserts @link into @queue at the given position.
 * 
 * Since: 2.4
 */
void BG_Queue_Push_nth_Link( bgqueue_t *queue, int n, bglist_t *link_ )
{
  bglist_t *next;
  bglist_t *prev;
  
  Com_Assert( queue != NULL );
  Com_Assert( link_ != NULL );

  if( n < 0 || n >= queue->length )
    {
      BG_Queue_Push_Tail_Link( queue, link_ );
      return;
    }

  Com_Assert( queue->head );
  Com_Assert( queue->tail );

  next = BG_Queue_Peek_nth_Link( queue, n );
  prev = next->prev;

  if( prev )
    prev->next = link_;
  next->prev = link_;

  link_->next = next;
  link_->prev = prev;

  if( queue->head->prev )
    queue->head = queue->head->prev;

  if( queue->tail->next )
    queue->tail = queue->tail->next;
  
  queue->length++;
}

/**
 * BG_Queue_Pop_Head:
 * @queue: a #bgqueue_t
 *
 * Removes the first element of the queue and returns its data.
 *
 * Returns: the data of the first element in the queue, or %NULL
 *     if the queue is empty
 */
void *BG_Queue_Pop_Head( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  if( queue->head )
    {
      bglist_t *node = queue->head;
      void * data = node->data;

      queue->head = node->next;
      if(queue->head)
        queue->head->prev = NULL;
      else
        queue->tail = NULL;
      BG_List_Free_1( node );
      queue->length--;

      return data;
    }

  return NULL;
}

/**
 * BG_Queue_Pop_Head_Link:
 * @queue: a #bgqueue_t
 *
 * Removes and returns the first element of the queue.
 *
 * Returns: the #bglist_t element at the head of the queue, or %NULL
 *     if the queue is empty
 */
bglist_t *BG_Queue_Pop_Head_Link( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  if( queue->head )
    {
      bglist_t *node = queue->head;

      queue->head = node->next;
      if( queue->head )
        {
          queue->head->prev = NULL;
          node->next = NULL;
        }
      else
        queue->tail = NULL;
      queue->length--;

      return node;
    }

  return NULL;
}

/**
 * BG_Queue_Peek_Head_Link:
 * @queue: a #bgqueue_t
 * 
 * Returns the first link in @queue.
 * 
 * Returns: the first link in @queue, or %NULL if @queue is empty
 * 
 * Since: 2.4
 */
bglist_t *BG_Queue_Peek_Head_Link( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  return queue->head;
}

/**
 * BG_Queue_Peek_Tail_Link:
 * @queue: a #bgqueue_t
 * 
 * Returns the last link in @queue.
 * 
 * Returns: the last link in @queue, or %NULL if @queue is empty
 * 
 * Since: 2.4
 */
bglist_t *BG_Queue_Peek_Tail_Link( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  return queue->tail;
}

/**
 * BG_Queue_Pop_Tail:
 * @queue: a #bgqueue_t
 *
 * Removes the last element of the queue and returns its data.
 *
 * Returns: the data of the last element in the queue, or %NULL
 *     if the queue is empty
 */
void *BG_Queue_Pop_Tail( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  if( queue->tail )
    {
      bglist_t *node = queue->tail;
      void * data = node->data;

      queue->tail = node->prev;
      if( queue->tail )
        queue->tail->next = NULL;
      else
        queue->head = NULL;
      queue->length--;
      BG_List_Free_1( node );

      return data;
    }
  
  return NULL;
}

/**
 * BG_Queue_Pop_nth:
 * @queue: a #bgqueue_t
 * @n: the position of the element
 * 
 * Removes the @n'th element of @queue and returns its data.
 * 
 * Returns: the element's data, or %NULL if @n is off the end of @queue
 * 
 * Since: 2.4
 */
void *BG_Queue_Pop_nth( bgqueue_t *queue, size_t n )
{
  bglist_t *nth_link;
  void * result;
  
  Com_Assert( queue != NULL );

  if( n >= queue->length )
    return NULL;
  
  nth_link = BG_Queue_Peek_nth_Link( queue, n );
  result = nth_link->data;

  BG_Queue_Delete_Link( queue, nth_link );

  return result;
}

/**
 * BG_Queue_Pop_Tail_link:
 * @queue: a #bgqueue_t
 *
 * Removes and returns the last element of the queue.
 *
 * Returns: the #bglist_t element at the tail of the queue, or %NULL
 *     if the queue is empty
 */
bglist_t *BG_Queue_Pop_Tail_link( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );
  
  if( queue->tail )
    {
      bglist_t *node = queue->tail;
      
      queue->tail = node->prev;
      if( queue->tail )
        {
          queue->tail->next = NULL;
          node->prev = NULL;
        }
      else
        queue->head = NULL;
      queue->length--;
      
      return node;
    }
  
  return NULL;
}

/**
 * BG_Queue_Pop_nth_Link:
 * @queue: a #bgqueue_t
 * @n: the link's position
 * 
 * Removes and returns the link at the given position.
 * 
 * Returns: the @n'th link, or %NULL if @n is off the end of @queue
 * 
 * Since: 2.4
 */
bglist_t*BG_Queue_Pop_nth_Link( bgqueue_t *queue, size_t n )
{
  bglist_t *link;
  
  Com_Assert( queue != NULL );

  if( n >= queue->length )
    return NULL;
  
  link = BG_Queue_Peek_nth_Link( queue, n );
  BG_Queue_Unlink( queue, link) ;

  return link;
}

/**
 * BG_Queue_Peek_nth_Link:
 * @queue: a #bgqueue_t
 * @n: the position of the link
 * 
 * Returns the link at the given position
 * 
 * Returns: the link at the @n'th position, or %NULL
 *     if @n is off the end of the list
 * 
 * Since: 2.4
 */
bglist_t *BG_Queue_Peek_nth_Link( bgqueue_t *queue, size_t n )
{
  bglist_t *link;
  int i;
  
  Com_Assert( queue != NULL );

  if( n >= queue->length )
    return NULL;
  
  if( n > queue->length / 2 )
    {
      n = queue->length - n - 1;

      link = queue->tail;
      for( i = 0; i < n; ++i )
        link = link->prev;
    }
  else
    {
      link = queue->head;
      for( i = 0; i < n; ++i )
        link = link->next;
    }

  return link;
}

/**
 * BG_Queue_Link_Index:
 * @queue: a #bgqueue_t
 * @link_: a #bglist_t link
 * 
 * Returns the position of @link_ in @queue.
 * 
 * Returns: the position of @link_, or -1 if the link is
 *     not part of @queue
 * 
 * Since: 2.4
 */
int BG_Queue_Link_Index( bgqueue_t *queue, bglist_t  *link_ )
{
  Com_Assert( queue != NULL );

  return BG_List_Position( queue->head, link_ );
}

/**
 * BG_Queue_Unlink:
 * @queue: a #bgqueue_t
 * @link_: a #bglist_t link that must be part of @queue
 *
 * Unlinks @link_ so that it will no longer be part of @queue.
 * The link is not freed.
 *
 * @link_ must be part of @queue.
 * 
 * Since: 2.4
 */
void BG_Queue_Unlink( bgqueue_t *queue, bglist_t *link_ )
{
  Com_Assert( queue != NULL );
  Com_Assert( link_ != NULL );

  if( link_ == queue->tail )
    queue->tail = queue->tail->prev;
  
  queue->head = BG_List_Remove_Link( queue->head, link_ );
  queue->length--;
}

/**
 * BG_Queue_Delete_Link:
 * @queue: a #bgqueue_t
 * @link_: a #bglist_t link that must be part of @queue
 *
 * Removes @link_ from @queue and frees it.
 *
 * @link_ must be part of @queue.
 * 
 * Since: 2.4
 */
void BG_Queue_Delete_Link( bgqueue_t *queue, bglist_t  *link_ )
{
  Com_Assert( queue != NULL );
  Com_Assert( link_ != NULL );

  BG_Queue_Unlink( queue, link_ );
  BG_List_Free( link_ );
}

/**
 * BG_Queue_Peek_Head:
 * @queue: a #bgqueue_t
 *
 * Returns the first element of the queue.
 *
 * Returns: the data of the first element in the queue, or %NULL
 *     if the queue is empty
 */
void *BG_Queue_Peek_Head( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  return queue->head ? queue->head->data : NULL;
}

/**
 * BG_Queue_Peek_Tail:
 * @queue: a #bgqueue_t
 *
 * Returns the last element of the queue.
 *
 * Returns: the data of the last element in the queue, or %NULL
 *     if the queue is empty
 */
void *BG_Queue_Peek_Tail( bgqueue_t *queue )
{
  Com_Assert( queue != NULL );

  return queue->tail ? queue->tail->data : NULL;
}

/**
 * BG_Queue_Peek_nth:
 * @queue: a #bgqueue_t
 * @n: the position of the element
 * 
 * Returns the @n'th element of @queue. 
 * 
 * Returns: the data for the @n'th element of @queue,
 *     or %NULL if @n is off the end of @queue
 * 
 * Since: 2.4
 */
void *BG_Queue_Peek_nth( bgqueue_t *queue, size_t n )
{
  bglist_t *link;
  
  Com_Assert( queue != NULL );

  link = BG_Queue_Peek_nth_Link( queue, n );

  if( link )
    return link->data;

  return NULL;
}

/**
 * BG_Queue_Index:
 * @queue: a #bgqueue_t
 * @data: the data to find
 * 
 * Returns the position of the first element in @queue which contains @data.
 * 
 * Returns: the position of the first element in @queue which
 *     contains @data, or -1 if no element in @queue contains @data
 * 
 * Since: 2.4
 */
int BG_Queue_Index( bgqueue_t *queue, const void *data )
{
  Com_Assert( queue != NULL );

  return BG_List_Index( queue->head, data );
}

/**
 * BG_Queue_Remove:
 * @queue: a #bgqueue_t
 * @data: the data to remove
 * 
 * Removes the first element in @queue that contains @data. 
 * 
 * Returns: %TRUE if @data was found and removed from @queue
 *
 * Since: 2.4
 */
qboolean BG_Queue_Remove( bgqueue_t *queue, const void *data )
{
  bglist_t *link;
  
  Com_Assert( queue != NULL );

  link = BG_List_Find( queue->head, data );

  if( link )
    BG_Queue_Delete_Link( queue, link );

  return ( link != NULL );
}

/**
 * BG_Queue_Remove_All:
 * @queue: a #bgqueue_t
 * @data: the data to remove
 * 
 * Remove all elements whose data equals @data from @queue.
 * 
 * Returns: the number of elements removed from @queue
 *
 * Since: 2.4
 */
size_t BG_Queue_Remove_All( bgqueue_t *queue, const void *data )
{
  bglist_t *list;
  size_t old_length;
  
  Com_Assert( queue != NULL );

  old_length = queue->length;

  list = queue->head;
  while( list )
    {
      bglist_t *next = list->next;

      if(list->data == data)
        BG_Queue_Delete_Link( queue, list );
      
      list = next;
    }

  return ( old_length - queue->length );
}

/**
 * BG_Queue_Insert_Before:
 * @queue: a #bgqueue_t
 * @sibling: (nullable): a #bglist_t link that must be part of @queue, or %NULL to
 *   push at the tail of the queue.
 * @data: the data to insert
 * 
 * Inserts @data into @queue before @sibling.
 *
 * @sibling must be part of @queue. Since GLib 2.44 a %NULL sibling pushes the
 * data at the tail of the queue.
 * 
 * Since: 2.4
 */
void BG_Queue_Insert_Before( bgqueue_t *queue, bglist_t *sibling, void *data )
{
  Com_Assert( queue != NULL );

  if( sibling == NULL )
    {
      /* We don't use BG_List_Insert_Before() with a NULL sibling because it
       * would be a O(n) operation and we would need to update manually the tail
       * pointer.
       */
      BG_Queue_Push_Tail( queue, data );
    }
  else
    {
      queue->head = BG_List_Insert_Before( queue->head, sibling, data );
      queue->length++;
    }
}

/**
 * BG_Queue_Insert_After:
 * @queue: a #bgqueue_t
 * @sibling: (nullable): a #bglist_t link that must be part of @queue, or %NULL to
 *   push at the head of the queue.
 * @data: the data to insert
 *
 * Inserts @data into @queue after @sibling.
 *
 * @sibling must be part of @queue. Since GLib 2.44 a %NULL sibling pushes the
 * data at the head of the queue.
 * 
 * Since: 2.4
 */
void BG_Queue_Insert_After( bgqueue_t *queue, bglist_t *sibling, void *data )
{
  Com_Assert( queue != NULL );

  if( sibling == NULL )
    BG_Queue_Push_Head( queue, data );
  else
    BG_Queue_Insert_Before( queue, sibling->next, data );
}

/**
 * BG_Queue_Insert_Sorted:
 * @queue: a #bgqueue_t
 * @data: the data to insert
 * @func: the #BG_CompareDataFunc used to compare elements in the queue. It is
 *     called with two elements of the @queue and @user_data. It should
 *     return 0 if the elements are equal, a negative value if the first
 *     element comes before the second, and a positive value if the second
 *     element comes before the first.
 * @user_data: user data passed to @func
 * 
 * Inserts @data into @queue using @func to determine the new position.
 * 
 * Since: 2.4
 */
void BG_Queue_Insert_Sorted( bgqueue_t *queue, void * data,
                             BG_CompareDataFunc func, void *user_data )
{
  bglist_t *list;
  
  Com_Assert( queue != NULL );

  list = queue->head;
  while(list && func( list->data, data, user_data) < 0 )
    list = list->next;

  BG_Queue_Insert_Before( queue, list, data );
}
