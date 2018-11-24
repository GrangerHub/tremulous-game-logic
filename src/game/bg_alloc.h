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

// Macros used for debugging the BGAME dynamic memory.  To turn off memory
// debuggin, #define NDEBUGMEM
//#ifndef NDEBUGMEM
//#define NDEBUGMEM
//#endif
#ifdef NDEBUGMEM
  #define assertMem(ignore, calledFile, calledLine)((void) 0)
  #define assertMemSize(ignore, blank, calledFile, calledLine)((void) 0)
  #define assertMemChunk(ignore, blank, calledFile, calledLine)((void) 0)
  #define assertMemKind(ignore, blank, calledFile, calledLine)((void) 0)
  #define assertMemKindChunk( ignore, blank, nothing, calledFile, calledLine )((void) 0)
  #define assertMemStack(ignore, calledFile, calledLine)((void) 0)
  #define assertMemStackChunk(ignore, blank,nothing, calledFile, calledLine)((void) 0)
#else
  #define BG_MemFuncCalledFrom( calledFile, calledLine ) \
    Com_Printf( "^3Called from: ^6%s^3:^6%d^7\n", calledFile, calledLine );
  #define assertMem( expr, calledFile, calledLine ) \
      if( !( expr ) ){ \
        BG_MemoryInfo(); \
        BG_MemFuncCalledFrom( calledFile, calledLine ); \
        Com_Error( ERR_DROP, "%s:%d: Assertion `%s' failed", \
                   __FILE__, __LINE__, #expr ); \
      }
  #define assertMemSize( expr, size, calledFile, calledLine ) \
      if( !( expr ) ){ \
        BG_MemoryInfo(); \
        Com_Printf("^3Chunk Size: (^6%lu^3)^7\n", ( ( size ) ) ); \
        BG_MemFuncCalledFrom( calledFile, calledLine ); \
        Com_Error( ERR_DROP, "%s:%d: Assertion `%s' failed", \
                   __FILE__, __LINE__, #expr ); \
      }
  #define assertMemChunk( expr, chunkAddress, calledFile, calledLine ) \
  if( !( expr ) ){ \
    BG_MemoryInfo(); \
    Com_Printf("^3Memory Chunk Address: (^60x%p^3)^7\n", ( chunkAddress ) ); \
    BG_MemFuncCalledFrom( calledFile, calledLine ); \
    Com_Error( ERR_DROP, "%s:%d: Assertion `%s' failed", \
               __FILE__, __LINE__, #expr ); \
  }
  #define assertMemKind( expr, kind, calledFile, calledLine ) \
      if( !( expr ) ){ \
        BG_MemoryInfo(); \
        memoryInfo_##kind(); \
        BG_MemFuncCalledFrom( calledFile, calledLine ); \
        Com_Error( ERR_DROP, "%s:%d: Assertion `%s' failed", \
                   __FILE__, __LINE__, #expr ); \
      }
  #define assertMemKindChunk( expr, kind, chunkAddress, calledFile, calledLine ) \
      if( !( expr ) ){ \
        BG_MemoryInfo(); \
        memoryInfo_##kind(); \
        Com_Printf("^3Memory Chunk Address: (^6%p^3)\n", ( chunkAddress ) ); \
        BG_MemFuncCalledFrom( calledFile, calledLine ); \
        assert(expr); \
        Com_Error( ERR_DROP, "%s:%d: Assertion `%s' failed", \
                   __FILE__, __LINE__, #expr ); \
      }
  #define assertMemStack(expr, errString, calledFile, calledLine) \
      if( !( expr ) ){ \
        BG_MemoryInfo(); \
        BG_StackPoolMemoryInfo(); \
        BG_MemFuncCalledFrom( calledFile, calledLine ); \
        Com_Error( ERR_DROP, "%s:%d: Assertion `%s' failed", \
                   __FILE__, __LINE__, #errString ); \
      }
  #define assertMemStackChunk( expr, errString, chunkAddress, size, calledFile, calledLine ) \
      if( !( expr ) ){ \
        BG_MemoryInfo(); \
        BG_StackPoolMemoryInfo(); \
        Com_Printf("^3Chunk Size: (^6%lu^3)\n", ( ( size ) ) ); \
        Com_Printf("^3Memory Chunk Address: (^6%p^3)\n", ( chunkAddress )); \
        BG_MemFuncCalledFrom( calledFile, calledLine ); \
        Com_Error( ERR_DROP, "%s:%d: Assertion `%s' failed", \
                   __FILE__, __LINE__, #errString ); \
      }
#endif

/*
======================
allocator_protos

This macro must be used before using the allocator macro. To limit the access of
a custom allocator's variables, use this macro in the same file you used the
allocator macro, and then you can make use of wrappers for external calls to the
allocator functions.  Or use this macro in a header file for direct access to a
custom allocator by all files that include said header.
======================
*/
#define allocator_protos(name) \
  static int unused_memory_chunks_##name(void); \
  static void memoryInfo_##name(void); \
  static void initPool_##name(char *calledFile, int calledLine); \
  static void *alloc_##name(char *calledFile, int calledLine); \
  static void free_##name(void *ptr, char *calledFile, int calledLine);

/*
======================
allocator

This macro can be used to create custom memory allocators that are used
frequesntly. This macro requires the use of the allocator_protos macro.  Be sure
to place a call (either directly or with wrappers) to memoryInfo_##name() in
BG_MemoryInfo() and to initPool_##name() in BG_InitMemory().

name indicates the name of the allocator which sets the names of its variables
and functions. size indicates the size in bytes of each chunk in the allocator's
memory pool (all chunks are the same size in a given allocator). count indicates
the total number of chunks in the allocator's memory pool.  The total amount of
memory in the allocator's memory pool in bytes is determined by multiplying the
chunk size by the total number of chunks.

buffer_##name[] is the allocator's memory pool. unused_##name[] is an array of
uint8_t pointers that point to the beginning of each chunk in buffer_##name[]. 
stack_##name is a pointer to an element in unused_##name[]. stack_##name is
associated with the last free chunk in the memory pool that would also be the
next memory chunk used for memory allocation by the allocator.  When memory is
allocated by the allocator, stack_##name is decremented torwards the beginning
of unused_##name[], and when memory is freed by the allocator, stack_##name is
inremented towards the end of unused_##name[].
======================
*/
#define allocator(name,size,count) \
  static uint8_t buffer_##name[(count) * (((size_t)(size) & ~((size_t)0xF)) + (size_t)16)] __attribute__ ((aligned (16))); \
  static uint8_t *unused_##name[(count)]; \
  static uint8_t **stack_##name; \
  static int unused_memory_chunks_##name(void) { \
    return ((int)(stack_##name - unused_##name)+1); \
  } \
  static void memoryInfo_##name(void){ \
    Com_Printf("^5---------------------------------------------^7\n"); \
    Com_Printf("^3Memory Info for Allocator ( ^6" #name "^3 )^7\n"); \
    Com_Printf("^5---------------------------------------------^7\n"); \
    if(!stack_##name ) \
    { \
      Com_Printf("^3pool not initialized.^7\n"); \
      Com_Printf("^5---------------------------------------------^7\n"); \
      return; \
    } \
    Com_Printf("^3Memory Pool Size: ^6%i bytes^7\n", (int)((count) * (((size_t)(size) & ~((size_t)0xF)) + (size_t)16))); \
    Com_Printf("^3Free Memory: ^6%i bytes^7\n", \
               (int)((stack_##name - unused_##name) * (((size_t)(size) & ~((size_t)0xF)) + (size_t)16))); \
    Com_Printf("^3Allocated Memory: ^6%i bytes^7\n", \
               (int)((&unused_##name[(count)-1] - stack_##name) * (((size_t)(size) & ~((size_t)0xF)) + (size_t)16))); \
    Com_Printf("^3Size of Chunks: ^6%i bytes^7\n", (int)(((size_t)(size) & ~((size_t)0xF)) + (size_t)16)); \
    Com_Printf("^3Total Number of Chunks: ^6%i^7\n", (int)(count)); \
    Com_Printf("^3Number of Unused Chunks: ^6%i^7\n", \
               unused_memory_chunks_##name( )); \
    Com_Printf("^3Number of Used Chunks: ^6%i^7\n", \
               (int)(&unused_##name[(count)-1] - stack_##name)); \
    Com_Printf("^3Memory Pool Address Range: from (^60x%p^3) to (^60x%p^3)^7\n", \
               buffer_##name, &buffer_##name[ ARRAY_LEN( buffer_##name ) - 1 ]); \
    Com_Printf("^5---------------------------------------------^7\n"); \
  } \
  static void initPool_##name(char *calledFile, int calledLine){ \
    int i; \
    uint8_t *temp = buffer_##name; \
    assertMem(((size) > 0 && "size must be greater than 0."), calledFile, calledLine); \
    assertMem(((count) > 0 && "count must be greater than 0."), calledFile, calledLine); \
    for(i = 0;i < (count);i++,temp += (((size_t)(size) & ~((size_t)0xF)) + (size_t)16)){ \
      *((uint8_t *)temp + (size)) = 0; \
      unused_##name[i] = temp; \
    } \
    stack_##name = unused_##name + ((count) - 1); \
  } \
  static void *alloc_##name(char *calledFile, int calledLine){ \
  void *ptr = (void *)*stack_##name; \
    assertMem((stack_##name != 0 && "pool not initialized."), calledFile, calledLine); \
    assertMemKind(((stack_##name >= unused_##name && "out of memory.")), name, calledFile, calledLine); \
    assertMemKind((*((uint8_t *)ptr + (size)) == 0 && "invalid memory access"), name, calledFile, calledLine); \
    stack_##name--; \
    *((uint8_t *)ptr + (size)) = 0x1A; \
    return ptr; \
  } \
  static void free_##name(void *ptr, char *calledFile, int calledLine){ \
    assertMemKindChunk(((uint8_t *)ptr >= buffer_##name &&  \
                    (uint8_t *)ptr < buffer_##name + ((count) * (((size_t)(size) & ~((size_t)0xF)) + (size_t)16)) && \
                    "out of bounds."), name, ptr, calledFile, calledLine); \
    assertMemKindChunk((((uint8_t *)ptr - buffer_##name) % (((size_t)(size) & ~((size_t)0xF)) + (size_t)16) == 0 && \
                    "not head of chunk."), name, ptr, calledFile, calledLine); \
    assertMemKindChunk((stack_##name < unused_##name + ((count) - 1) && \
                    "exceeds pool size."), name, ptr, calledFile, calledLine); \
    assertMemKindChunk((*((uint8_t *)ptr + (size)) == 0x1A && \
                       "invalid memory access or pointer freed more than once"), \
                       name, ptr, calledFile, calledLine); \
    *((uint8_t *)ptr + (size)) = 0; \
    stack_##name++; \
    *stack_##name = ptr; \
  }

// Used to init all BGAME memory allocators  
void  _BG_InitMemory( char *calledFile, int calledLine );

#define BG_InitMemory( ) _BG_InitMemory( __FILE__, __LINE__ )

// Used to display debug info of all BGAME memory allocators
void  BG_MemoryInfo( void );

// For general memory allocation of misclanous allocations of types that are
// infrequently used.  Utilizes a binary search of multiple memory allacotor
// "class sizes".  For types that are frequently allocated, custom allocators
// can be created using the allocator_protos() and allocator() macros.
void  *_BG_Alloc( size_t size, char *calledFile, int calledLine ); // returns unitialized memory
void  *_BG_Alloc0( size_t size, char *calledFile, int calledLine ); // returns memory with all bits set to 0
void  _BG_Free( void *ptr, char *calledFile, int calledLine );

// Macro wrappers
#define BG_Alloc( size ) _BG_Alloc( size, __FILE__, __LINE__ )
#define BG_Alloc0( size ) _BG_Alloc0( size, __FILE__, __LINE__ )
#define BG_Free( ptr ) _BG_Free( ptr, __FILE__, __LINE__ )

// Function wrappers that are used only for passing as arguments via a function
// pointers, otherwise use the corresponding macro wrappers as using these
// function wrappers will result in loss of debuggin infomration.
void  *BG_AllocPassed( size_t size );
void  *BG_Alloc0Passed( size_t size );
void  BG_FreePassed( void *ptr );

// for the stack pool allocator, which is used for temporary allocation within
// function stacks
void *_BG_StackPoolAlloc( size_t size, char *calledFile, int calledLine );
void _BG_StackPoolFree( void *ptr, char *calledFile, int calledLine );

#define BG_StackPoolAlloc( size ) _BG_StackPoolAlloc( size,  __FILE__, __LINE__ )
#define BG_StackPoolFree( ptr ) _BG_StackPoolFree( ptr, __FILE__, __LINE__ )

void BG_StackPoolReset( void ); // BG_StackPoolReset is used in
                                        // BG_InitMemory() to init the stack 
                                        // pool allocator, and effectively 
                                        // clears all the memory in the stack 
                                        // pool allocator if used elsewhere. So
                                        // use with caution.
void BG_StackPoolMemoryInfo( void ); // Used for debugging the stack pool
                                     // allocator
/*
================================================================================
*/
