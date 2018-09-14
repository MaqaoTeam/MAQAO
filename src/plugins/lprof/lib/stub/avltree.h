/*
   Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)

   This file is part of MAQAO.

  MAQAO is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// CF avltree.c

#ifndef AVLTREE_H_
#define AVLTREE_H_

#include <stdint.h>

#include  "libmmaqao.h"

typedef struct avlTree
{
   unsigned long key;
   void* value;
   int height;
   struct avlTree *left;
   struct avlTree *right;
}avlTree_t;

typedef struct infoFunc{
   char* name;
   uint64_t start;
   uint64_t stop;
   char* src_file;
   int src_line;
   int inlined;
   uint32_t*** hwcInfo;
   hashtable_t*** callChainsInfo;
   uint32_t** totalCallChains;
   int32_t libraryIdx;
}SinfoFunc;

typedef struct infoLoop{
   uint64_t start;
   uint64_t stop;
   char* src_file;
   char* func_name;
   int src_line_start;
   int src_line_end;
   char* level;
   int loop_id;
   uint32_t*** hwcInfo;
   int32_t libraryIdx;
}SinfoLoop;

enum TREE_TYPE{PERF_FUNC=1,PERF_LOOP=2,PERF_EXT_LIB=3};

void print_infoFunc(SinfoFunc* infoFunc, unsigned nbProcesses, unsigned nbThreads, unsigned nbHwc);
int get_height(avlTree_t * tree);
void set_height(avlTree_t * tree);
avlTree_t* init(unsigned long key, void*  value, avlTree_t * left, avlTree_t * right);
avlTree_t* left_rotation (avlTree_t* tree);
avlTree_t* right_rotation (avlTree_t* tree);
avlTree_t* balancing(avlTree_t* tree);
avlTree_t* insert(unsigned long key, void* value, avlTree_t* tree);
avlTree_t* delete_node(unsigned long key, avlTree_t* tree);
avlTree_t* delete_root(avlTree_t* tree);
avlTree_t* last_right(avlTree_t* tree);
avlTree_t* last_left(avlTree_t* tree);
avlTree_t* search(unsigned long key, avlTree_t* tree);
avlTree_t* search_address(unsigned long key, avlTree_t* tree,int infoType);
void destroy(avlTree_t* tree);
void print_tree(avlTree_t* tree , unsigned long h);
#endif
