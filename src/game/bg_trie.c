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

#include "../qcommon/q_shared.h"
#include "bg_public.h"

/*
--------------------------------------------------------------------------------
Custom Dynamic Memory Management for tries
*/
#ifdef GAME
#define  MAX_TRIES 256
#else
#ifdef CGAME
#define  MAX_TRIES 256
#else
#define  MAX_TRIES 1024
#endif
#endif

allocator_protos(bgtrie)
allocator(bgtrie, sizeof(bgtrie_t), MAX_TRIES)

#ifdef GAME
#define  MAX_TRIE_NODES (2 * 1024)
#else
#ifdef CGAME
#define  MAX_TRIE_NODES (2 * 1024)
#else
#define  MAX_TRIE_NODES (16 * 1024)
#endif
#endif

allocator_protos(bgtrie_node)
allocator(bgtrie_node, sizeof(bgtrie_node_t), MAX_TRIE_NODES)
#ifdef GAME
#define  MAX_TRIE_MEMBERS 256
#else
#ifdef CGAME
#define  MAX_TRIE_MEMBERS 256
#else
#define  MAX_TRIE_MEMBERS 1024
#endif
#endif

allocator_protos(bgtrie_member)
allocator(bgtrie_member, sizeof(bgtrie_member_t), MAX_TRIE_MEMBERS)

/*
===============
BG_Trie_Init_Memory

Executed in BG_InitMemory()
===============
*/
void BG_Trie_Init_Memory(char *calledFile, int calledLine) {
  initPool_bgtrie(calledFile, calledLine);
  initPool_bgtrie_node(calledFile, calledLine);
  initPool_bgtrie_member(calledFile, calledLine);
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

// creates a new trie.
bgtrie_t* BG_Trie_Create()
{
    return malloc(sizeof(bgtrie_t));
}

// returns pointer to thing at key
void* trie_traverse(bgtrie_t* data, char* key)
{
    bgtrie_t* current_node = data;

    // increment the pointer position of key until null terminator is reached
    while (*key != 0)
    {
        // try to find the correct child node
        size_t index = *key;
        bgtrie_t* next_node = current_node->children[index];

        // if next_node doesn't actually exist, return NULL
        if (!next_node) 
        {
            return NULL;
        }

        // set current node to the correct child node
        current_node = next_node;

        // increment key pointer position
        key++;
    }

    return current_node;
}

// inserts val at key
void* BG_Trie_Insert(bgtrie_t* data, char* key, void* val)
{
    bgtrie_t* current_node = data;

    // increment the pointer position of key until null terminator is reached
    while (*key != 0)
    {
        // try to find the correct child node
        size_t index = *key;
        bgtrie_t* next_node = current_node->children[index];

        switch (current_node->type)
        {
            case trienode_twig:
                // if this twig goes in the same direction
                if (next_node)
                {
                    break;
                }
                // else it will promote to a branch
            case trienode_leaf:
                current_node->type++;
        }

        // if next_node doesn't actually exist, create it
        if (!next_node) 
        {
            current_node->children[index] = BG_Trie_Create();
            next_node = current_node->children[index];
            if (trienode_twig == current_node->type)
            {
                // children[0] is never used; we'll hijack it as a fast-forward key
                *current_node->children = next_node;
            }
        }

        // set current node to the correct child node
        current_node = next_node;

        // increment key pointer position
        key++;
    }

    // set val at current_node
    current_node->val = val;

    return current_node;
}

// returns void pointer of thing at key
void* BG_Trie_Lookup(bgtrie_t* data, char* key)
{   
    bgtrie_t* current_node = trie_traverse(data, key);

    if (!current_node)
    {
        return NULL;
    }

    return current_node->val;
}

// If there is only one key that maches the given `char* prefix`, returns its value. If there are several, returns the given `void* ambiguous`, if there are none, returns null.
void* BG_Trie_Lookup_Prefix(bgtrie_t* data, char* prefix, void* ambiguous)
{
    bgtrie_t* current_node = trie_traverse(data, prefix);

    if (!current_node)
    {
        return NULL;
    }

    // exact match
    if(NULL!=current_node->val)
    {
        return current_node->val;
    }

    // prefix matched, but we still have to find the key itself
    while(NULL==current_node->val)
    {
        switch(current_node->type)
        {
            case trienode_leaf:
		// we can do nothing but return null
                return current_node->val;
            case trienode_twig:
		// fast-forward to the next (only) child
                current_node = *current_node->children;
		continue;
            case trienode_branch:
		return ambiguous;
        }
    }

    // we found a key with the requested prefix, but there are more
    if(trienode_twig==current_node->type)
    {
        return ambiguous;
    }

    return current_node->val;
}

// removes thing at key
void BG_Trie_Remove(bgtrie_t* data, char* key)
{
    bgtrie_t* current_node = data;

    // increment the pointer position of key until null terminator is reached
    while (*key != 0)
    {
        // try to find the correct child node
        size_t index = *key;
        bgtrie_t* next_node = current_node->children[index];

        // if next_node doesn't actually exist, return NULL
        if (!next_node) 
        {
            return NULL;
        }

        // set current node to the correct child node
        current_node = next_node;

        // increment key pointer position
        key++;
    }

    free(current_node->val);
    return;
}

// destroys trie at pointer
void BG_Trie_Destroy(bgtrie_t* data)
{
    int a;
    for (a = 1; a < 256; a++)
    {
        if (data->children[a])
        {
            BG_Trie_Destroy(data->children[a]);
        }
    }

    free(data);
    return;
}
