/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2018 GrangerHub

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

/*
================================================================================
BGAME Dynamic Memory Allocation

Use BG_Alloc(), BG_Alloc0() and BG_Free() for general memory allocation of
misclanous allocations of types that are infrequently reallocated.  Utilizes a
binary search of multiple memory allacotor "class sizes".  BG_Alloc() returns
unitialized memory chunks, while BG_Alloc0() returns memory chunks with all bits
set to 0.

To add a new general allocator memory class, define its chunk size, define its
maximum number of chunks (can be different for each QVM), create the allocator
for the new memory class with the createMemoryAllocatorForClass() macro, and
then instantiate the memory class inside the memClasses[] array with the
instantiateMemoryClass() macro.  All of this is done inside the bg_alloc.c file
only.

The same memory chunk size can't be set in more than one memory class, if more
memory chunks of the same size are needed, simply increase the max number of
chunks for the class that has that memory chunk size. Keep in mind that more
general allocator memory classes may require more searching for BG_Alloc(),
BG_Alloc0() and BG_Free().  Definning the memory classes in a specific order is
not functionally necessary, as all required sorting is built into the allocator
system, and performed inside BG_InitMemory().

Use BG_StackPoolAlloc(), BG_StackPoolFree(), BG_StackPoolReset(), and
BG_StackPoolMemoryInfo for the stack pool allocator, which is used for temporary
allocation within function stacks.

For types that are frequently reallocated, custom allocators dedicated to
specific types can be created using the allocator_protos() and allocator()
macros.  Those two macros are located in bg_alloc.h

For memory debugging, BG_MemoryInfo() gives a general overview of all the
allocators, and each custom allocotor comes with its own memoryInfo_##name()
function, so be sure to add a call to that function inside BG_MemoryInfo() when
creating a new custom memory allocator.  More detailed debugging can be done if
NDEBUGMEM is not defined inside bg_alloc.h

__FILE__ and __LINE__ are ultimately passed through as arguments to many of the
allocator functions for debugging purposes, macro wrappers can be created in
such cases not requiring you to writeout __FILE__ & __LINE__ for every external
use, while still passing the file names and line numbers of the locations of the
macro wrappers.
--------------------------------------------------------------------------------
*/

#define MEMORY_CLASS_00_CHUNK_SIZE        8
#define MEMORY_CLASS_01_CHUNK_SIZE        16
#define MEMORY_CLASS_02_CHUNK_SIZE        32
#define MEMORY_CLASS_03_CHUNK_SIZE        64
#define MEMORY_CLASS_04_CHUNK_SIZE        128
#define MEMORY_CLASS_05_CHUNK_SIZE        256
#define MEMORY_CLASS_06_CHUNK_SIZE        512
#define MEMORY_CLASS_07_CHUNK_SIZE        1024
#define MEMORY_CLASS_08_CHUNK_SIZE        2048
#define MEMORY_CLASS_09_CHUNK_SIZE        4096
#define MEMORY_CLASS_10_CHUNK_SIZE        8192

// Set the maximum memory usage for each module here.
#ifdef GAME
#define STACK_POOL_SIZE                  ( 64 * 1024 )
#define MAX_MEMORY_CLASS_00_NUM_OF_CHUNKS ( 4 * 1024 )
#define MAX_MEMORY_CLASS_01_NUM_OF_CHUNKS ( 2 * 1024 )
#define MAX_MEMORY_CLASS_02_NUM_OF_CHUNKS ( 2 * 1024 )
#define MAX_MEMORY_CLASS_03_NUM_OF_CHUNKS 1024
#define MAX_MEMORY_CLASS_04_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_05_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_06_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_07_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_08_NUM_OF_CHUNKS 1024
#define MAX_MEMORY_CLASS_09_NUM_OF_CHUNKS 64
#define MAX_MEMORY_CLASS_10_NUM_OF_CHUNKS 64
#else
#ifdef CGAME
#define STACK_POOL_SIZE                  ( 64 * 1024 )
#define MAX_MEMORY_CLASS_00_NUM_OF_CHUNKS ( 4 * 1024 )
#define MAX_MEMORY_CLASS_01_NUM_OF_CHUNKS ( 2 * 1024 )
#define MAX_MEMORY_CLASS_02_NUM_OF_CHUNKS 1024
#define MAX_MEMORY_CLASS_03_NUM_OF_CHUNKS 1024
#define MAX_MEMORY_CLASS_04_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_05_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_06_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_07_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_08_NUM_OF_CHUNKS 512
#define MAX_MEMORY_CLASS_09_NUM_OF_CHUNKS 64
#define MAX_MEMORY_CLASS_10_NUM_OF_CHUNKS 64
#else // for the UI
#define STACK_POOL_SIZE                   ( 32 * 1024 )
#define MAX_MEMORY_CLASS_00_NUM_OF_CHUNKS 1024
#define MAX_MEMORY_CLASS_01_NUM_OF_CHUNKS 512
#define MAX_MEMORY_CLASS_02_NUM_OF_CHUNKS 256
#define MAX_MEMORY_CLASS_03_NUM_OF_CHUNKS 128
#define MAX_MEMORY_CLASS_04_NUM_OF_CHUNKS 64
#define MAX_MEMORY_CLASS_05_NUM_OF_CHUNKS 64
#define MAX_MEMORY_CLASS_06_NUM_OF_CHUNKS 64
#define MAX_MEMORY_CLASS_07_NUM_OF_CHUNKS 16
#define MAX_MEMORY_CLASS_08_NUM_OF_CHUNKS 16
#define MAX_MEMORY_CLASS_09_NUM_OF_CHUNKS 4
#define MAX_MEMORY_CLASS_10_NUM_OF_CHUNKS 4
#endif
#endif

#define createMemoryAllocatorForClass( chunkSize, numOfChunks ) \
  allocator_protos( chunkSize ) \
  allocator( chunkSize, chunkSize, numOfChunks )

// create the memory allocators for the memory classes here
createMemoryAllocatorForClass( MEMORY_CLASS_00_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_00_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_01_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_01_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_02_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_02_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_03_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_03_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_04_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_04_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_05_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_05_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_06_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_06_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_07_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_07_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_08_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_08_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_09_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_09_NUM_OF_CHUNKS)
createMemoryAllocatorForClass( MEMORY_CLASS_10_CHUNK_SIZE,
                               MAX_MEMORY_CLASS_10_NUM_OF_CHUNKS)

typedef struct bgMemoryClass_s
{
  const size_t chunkSize; // Used for sorting and searching by chunk size

  // used for sorting and searching by memory address  
  uint8_t * const bufferStart;  // The first element of the memory class buffer
  uint8_t * const bufferEnd; // The last element of the memory class buffer

  void    (* const memoryInfo)(void);
  void    (* const initPool)(char *calledFile, int calledLine);
  void    *(* const alloc)(char *calledFile, int calledLine);
  void    (* const free)(void *ptr, char *calledFile, int calledLine);
} bgMemoryClass_t;

// A macro used for easily adding memory classes in the memClasses[] array, and
// for better readability
#define _instantiateMemoryClass( classChunkSize ) \
  { \
    (size_t)(classChunkSize), \
    buffer_##classChunkSize, \
    &buffer_##classChunkSize[ ARRAY_LEN( buffer_##classChunkSize ) - 1 ], \
    memoryInfo_##classChunkSize, \
    initPool_##classChunkSize, \
    alloc_##classChunkSize, \
    free_##classChunkSize \
  }

// Macro used to ensure that a #define constant used as an argument expands
#define instantiateMemoryClass( classChunkSize ) _instantiateMemoryClass( classChunkSize )

// Instatiate memory classes here in this array. A binary search is performed
// in this array to find the memory class with the size for the best fit in the
// general memory allocator.
static const bgMemoryClass_t memClasses[ ] =
{
  instantiateMemoryClass( MEMORY_CLASS_00_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_01_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_02_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_03_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_04_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_05_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_06_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_07_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_08_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_09_CHUNK_SIZE ),
  instantiateMemoryClass( MEMORY_CLASS_10_CHUNK_SIZE )
};

#define NUMBER_OF_MEMORY_CLASSES ( ARRAY_LEN( memClasses ) )

static const bgMemoryClass_t *memClassesSortedChunkSize[ NUMBER_OF_MEMORY_CLASSES ]; // used for BG_Alloc()
static const bgMemoryClass_t *memClassesSortedBufferAddress[ NUMBER_OF_MEMORY_CLASSES ]; // used for BG_Free()

/*
======================
BG_MemClassNumFromChunkSize

Utilizes a binary search based on the allocator classes' chunk sizes to
determine which allocator class would have the best fit for the memory requested
in BG_Alloc
======================
*/
static ID_INLINE const bgMemoryClass_t *BG_MemClassNumFromChunkSize( size_t size,
                                                                     char *calledFile,
                                                                     int calledLine )
{
  size_t low = 0, high = ( NUMBER_OF_MEMORY_CLASSES -1 ), middle;

  assertMemSize( ( size ) > 0, size,
                 calledFile, calledLine );
  assertMemSize( ( size ) <= memClassesSortedChunkSize[ high ]->chunkSize, size,
                 calledFile, calledLine );

  while( low < high )
  {
    middle = low + (high - low) / 2;
    if( memClassesSortedChunkSize[ middle ]->chunkSize < size )
      low = middle + 1;
    else
      high = middle;
  }

  return memClassesSortedChunkSize[ high ];
}

/*
======================
BG_MemClassNumFromAddress

Utilizes a binary search based on the allocator classes' buffer addresses to
determine which allocator class (if any) a given memory chunk that is attempted
to be freed by BG_Free().
======================
*/
static ID_INLINE const bgMemoryClass_t *BG_MemClassNumFromAdress( void *ptr, char *calledFile, int calledLine )
{
  size_t low = 0, high = ( NUMBER_OF_MEMORY_CLASSES -1 ), middle;
  int             passes = 0;

  assertMemChunk( ((uint8_t *)ptr >= memClassesSortedBufferAddress[ low ]->bufferStart &&
                  (uint8_t *)ptr <= memClassesSortedBufferAddress[ high ]->bufferEnd),
                  ptr, calledFile, calledLine );

  while( low < high )
  {
    middle = low + (high - low) / 2;
    passes++;

    if( (uint8_t *)(ptr) < memClassesSortedBufferAddress[ middle ]->bufferStart )
      high = middle;
    else if( memClassesSortedBufferAddress[ middle ]->bufferEnd < (uint8_t *)(ptr) )
      low = middle + 1;
    else
      return memClassesSortedBufferAddress[ middle ];
  }

  if( low == high )
  {
    passes++;

    if( ( (uint8_t *)(ptr) >= memClassesSortedBufferAddress[ low ]->bufferStart ) &&
        ( memClassesSortedBufferAddress[ low ]->bufferEnd >= (uint8_t *)(ptr) ) )
      return memClassesSortedBufferAddress[ low ];
  }

  // The memory chunk is not from any of the general allocator memory classes
  BG_MemoryInfo();
  Com_Printf("^3Memory Chunk Address: (^6%p^3)\n", ( ptr ) );
  BG_MemFuncCalledFrom( calledFile, calledLine );
  Com_Error( ERR_DROP, "%s:%d: Assertion `%s' failed",
             __FILE__, __LINE__, "BG_MemClassNumFromAdress: out of bounds." );

  return NULL;
}

/*
======================
BG_CmpMemoryClassChunkSize

Compares the chunk sizes of memory classes for a qsort used in BG_InitMemory
======================
*/
static int BG_CmpMemoryClassChunkSize( const void *a, const void *b )
{
  const bgMemoryClass_t *classA = *(const bgMemoryClass_t **)a;
  const bgMemoryClass_t *classB = *(const bgMemoryClass_t **)b;

  return ( classA->chunkSize - classB->chunkSize );
}

/*
======================
BG_CmpMemoryClassBufferAddress

Compares the buffer addresses of memory classes for a qsort used in BG_InitMemory
======================
*/
static int BG_CmpMemoryClassBufferAddress( const void *a, const void *b )
{
  const bgMemoryClass_t *classA = *(const bgMemoryClass_t **)a;
  const bgMemoryClass_t *classB = *(const bgMemoryClass_t **)b;

  if( ( classA->bufferStart -  classB->bufferStart ) < 0 )
    return -1;
  else if( ( classA->bufferStart -  classB->bufferStart ) > 0 )
    return 1;
  else
    return 0;
}

/*
======================
_BG_InitMemory

Executed in the primary initialization functions of
the modules that use BGAME Memory Allocation.
======================
*/
void  _BG_InitMemory( char *calledFile, int calledLine )
{
  int i;

  // Init the general use memory pools
  for( i = 0; i < NUMBER_OF_MEMORY_CLASSES; i++ )
    memClasses[ i ].initPool( calledFile, calledLine );

  // initialize the memory class pointer arrays
  for( i = 0; i < NUMBER_OF_MEMORY_CLASSES; i++ )
  {
    memClassesSortedChunkSize[ i ] = &memClasses[ i ];
    memClassesSortedBufferAddress[ i ] = &memClasses[ i ];
  }

  // Sort the memClassesSortedChunkSize and memClassesSortedBufferAddress arrays
  qsort( memClassesSortedChunkSize, ARRAY_LEN( memClassesSortedChunkSize ), sizeof( memClassesSortedChunkSize[0] ),
         BG_CmpMemoryClassChunkSize );
  qsort( memClassesSortedBufferAddress, ARRAY_LEN( memClassesSortedBufferAddress ), sizeof( memClassesSortedBufferAddress[0] ),
         BG_CmpMemoryClassBufferAddress );

  // Init the memory stack pool
  BG_StackPoolReset( );

  // Init the memory for custom allocators
  BG_List_Init_Memory( calledFile, calledLine );
  BG_Queue_Init_Memory( calledFile, calledLine );
}

/*
======================
_BG_Alloc

Allocates memory from the general memory class that has the best fit for the
given size. Utilizes a binary search, so this function is best for miscelanous
types that are infrequently reallocated. Only use _BG_Free() or BG_Free() to
free memory allocated by BG_Alloc().  For frequently reallocated types, consider
making a custom allocator using the allocator_protos() and allocator() macros.
The returned memory chunk is unitialized.
======================
*/
void  *_BG_Alloc( size_t size, char *calledFile, int calledLine )
{
  const bgMemoryClass_t *memoryClass = BG_MemClassNumFromChunkSize( size,
                                                                    calledFile,
                                                                    calledLine );

  return memoryClass->alloc( calledFile, calledLine );
}

/*
======================
_BG_Alloc0

Same as _BG_Alloc(), but initializes all the bits to 0 with memset.
======================
*/
void  *_BG_Alloc0( size_t size, char *calledFile, int calledLine )
{
  const bgMemoryClass_t *memoryClass = BG_MemClassNumFromChunkSize( size,
                                                                    calledFile,
                                                                    calledLine );
  void *ptr = memoryClass->alloc( calledFile, calledLine );

  memset( ptr, 0, memoryClass->chunkSize  );
  return ptr;
}

/*
======================
_BG_Free

Frees memory that was allocated by BG_Alloc. For freeing memory allocated by
another allocator, utilize that other allocator's corresponding free function
instead.
======================
*/
void  _BG_Free( void *ptr, char *calledFile, int calledLine )
{
  const bgMemoryClass_t *memoryClass = BG_MemClassNumFromAdress( ptr,
                                                                 calledFile,
                                                                 calledLine );

  memoryClass->free(ptr, calledFile, calledLine);
}

/*
--------------------------------------------------------------------------------
Function wrappers that are used only for passing as arguments via a function
pointers, otherwise use the corresponding macro wrappers as using these function
wrappers will result in loss of debuggin infomration.
*/
void  *BG_AllocPassed( size_t size )
{
  return _BG_Alloc( size, __FILE__, __LINE__ );
}
void  *BG_Alloc0Passed( size_t size )
{
  return _BG_Alloc0( size, __FILE__, __LINE__ );
}
void  BG_FreePassed( void *ptr )
{
  _BG_Free( ptr, __FILE__, __LINE__ );
}
/*
--------------------------------------------------------------------------------
*/
/*
======================
BG_MemoryInfo

Prints detailed info about the BGAME dynamic memory for debugging
======================
*/
void BG_MemoryInfo( void )
{
  int i;

  Com_Printf( "^5============================================^7\n" );
  Com_Printf( "^3Number of BGAME General Memory Classes: ^6%lu^7\n",
              NUMBER_OF_MEMORY_CLASSES  );
  Com_Printf( "^5============================================^7\n" );

  // cycle through all the general memory classes
  for( i = 0; i < NUMBER_OF_MEMORY_CLASSES; i++ )
    (*memClassesSortedChunkSize[ i ]).memoryInfo( );

  BG_StackPoolMemoryInfo( );

  Com_Printf( "^5=================^7\n" );
  Com_Printf( "^3Custom Allocators^7\n"  );
  Com_Printf( "^5=================^7\n" );

  BG_List_Memory_Info( );
  BG_Queue_Memory_Info( );
}

/*
-------------------------------------------------------------------------------
Stack Pool Allocator

For the stack pool allocator, which is used for temporary allocation within
function stacks
--------------------------------------------------------------------------------
*/
static uint8_t       stack_pool[STACK_POOL_SIZE] __attribute__ ((aligned (16))) = {0};
static uint8_t       *stack_pool_ptr = stack_pool;
static const size_t  stack_pool_size_t_size =
                                 (sizeof(size_t) & ~((size_t)0xF)) + (size_t)16;

/*
======================
_BG_StackPoolAlloc
======================
*/
void *_BG_StackPoolAlloc( size_t size, char *calledFile, int calledLine )
{
  uint8_t *ptr = stack_pool_ptr;

  size = (size & ~((size_t)0xF)) + (size_t)16;
  assertMemStack( (size > 0),
                  "BG_StackPoolAlloc",
                  calledFile, calledLine );
  assertMemStack( ( stack_pool_ptr + size <= stack_pool + STACK_POOL_SIZE ),
                  "BG_StackPoolAlloc", calledFile, calledLine );

  stack_pool_ptr += size;

  // store the size for later freeing
  *(size_t *)stack_pool_ptr = size;
  stack_pool_ptr += stack_pool_size_t_size;

  return ptr;
}

/*
======================
_BG_StackPoolFree
======================
*/
void _BG_StackPoolFree( void *ptr, char *calledFile, int calledLine )
{
  size_t size;

  stack_pool_ptr -= stack_pool_size_t_size;
  size = *(size_t *)stack_pool_ptr;
  assertMemStackChunk( ( (uint8_t *)ptr >= stack_pool &&
                         (uint8_t *)ptr < stack_pool + STACK_POOL_SIZE ),
                       "BG_StackPoolFree", ptr, size, calledFile, calledLine );
  assertMemStackChunk( ( size > 0 ),
                       "BG_StackPoolFree",
                       ptr, size, calledFile, calledLine );
 assertMemStackChunk( ( (size) % 16 == 0 ),
                      "BG_StackPoolFree",
                      ptr, size, calledFile, calledLine );
  assertMemStackChunk( ( stack_pool_ptr >= stack_pool + size ),
                       "BG_StackPoolFree", ptr, size, calledFile, calledLine );

  stack_pool_ptr -= size;

  assertMemStackChunk( ( stack_pool_ptr == ptr ),
                       "BG_StackPoolFree",
                       ptr, size, calledFile, calledLine );
}

/*
======================
BG_StackPoolReset

Used in BG_InitMemory() to init the stack pool allocator, and effectively clears
all the memory in the stack pool allocator if used elsewhere. So use with
caution.
======================
*/
void BG_StackPoolReset( void )
{
  stack_pool_ptr = stack_pool;
}

/*
======================
BG_StackPoolMemoryInfo

Used for debugging the pool allocator
======================
*/
void BG_StackPoolMemoryInfo( void )
{
  Com_Printf( "^5==============================\n" );
  Com_Printf( "^3Memory Info for the Stack Pool\n" );
  Com_Printf( "^5==============================\n" );
  Com_Printf( "^3Stack Pool Size: ^6%i bytes\n", STACK_POOL_SIZE );
  Com_Printf( "^3Free Memory: ^6%ld bytes\n",
             ( ( stack_pool + STACK_POOL_SIZE ) - stack_pool_ptr ) );
  Com_Printf( "^3Allocated Memory: ^6%ld bytes\n",
              (stack_pool_ptr - stack_pool ) );
  Com_Printf( "^3Stack Pool Address Range: from (^60x%p^3) to (^60x%p^3)\n",
              stack_pool, &stack_pool[STACK_POOL_SIZE - 1] );
  Com_Printf( "^5---------------------------------------------^7\n" );
}
