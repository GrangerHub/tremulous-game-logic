/*
===========================================================================
The MIT License (MIT)

Copyright (C) 2014 Elvin Yung
Copyright (C) 2015-2021 GrangerHub

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

===========================================================================
*/

#ifndef __BG_TRIE_H__
#define __BG_TRIE_H__

#include "../qcommon/q_shared.h"
#include "bg_list.h"

// trie struct declarations
typedef struct bgtrie_s bgtrie_t;
typedef struct bgtrie_node_s bgtrie_node_t;

typedef struct bgtrie_member_s
{
    bgtrie_t *trie;
    char     key[256];
    void     *data;
} bgtrie_member_t;

struct bgtrie_node_s 
{
    bglink_t      *member; // void* pointing at this node's data
    bgtrie_node_t *child_arr[63]; // constant size array of 256 child tries (one for each character)
    bglist_t      *child_list;
    bglink_t      *parent_link;
};

struct bgtrie_s
{
    bgtrie_node_t root;
    bglist_t      member_list;
};

// Executed in BG_InitMemory()
void BG_Trie_Init_Memory(char *calledFile, int calledLine);

// Displays info related to the allocation of tries
void BG_Trie_Memory_Info(void);

// creates a new trie and returns a pointer to that trie.
bgtrie_t* BG_Trie_Create();

// inserts val at key
void* BG_Trie_Insert(bgtrie_t* data, char* key, void* val);

// returns thing at key
void* BG_Trie_Lookup(bgtrie_t* data, char* key);

// returns thing at key matching prefix, if there are multiple matches, returns the ambiguous pointer
void* BG_Trie_Lookup_Prefix(bgtrie_t* data, char* prefix, void* ambiguous);

// Calls @func for each element in the trie with the given prefix
// (@func is applied to all of the elements in the trie if the prefix is NULL)
// passing @user_data to the function.
void  BG_Trie_Foreach(
  bgtrie_t *trie, char *prefix, BG_Func func, void *user_data );

// removes thing at key
void BG_Trie_Remove(bgtrie_t* data, char* key);

// destroys trie at pointer
void BG_Trie_Destroy(bgtrie_t* data);

#endif /* __BG_TRIE_H__ */
