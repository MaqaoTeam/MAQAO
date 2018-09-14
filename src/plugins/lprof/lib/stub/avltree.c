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

/*
 * This file defines data structures and functions to handle AVL trees,
 * used to efficiently (in log(n)) find data from a key in intervals.
 * In lprof, used to search sample addresses in functions/loops address ranges.
 * TODO: consider moving this upper in MAQAO to allow reuse by another C/LUA modules
 * This will imply to rename functions like insert => AVL_tree_insert
 * and move lprof specific code: SinfoLoop and SinfoFct
 */

#include <inttypes.h>

#include "avltree.h"



void print_infoFunc(SinfoFunc* infoFunc, unsigned nbProcesses, unsigned nbThreads, unsigned nbHwc)
{
   assert(infoFunc != NULL);
   unsigned processIdx, threadIdx, hwcIdx;
   fprintf(stderr,"\nFunction %s (%p)\n",infoFunc->name,infoFunc);
   for ( processIdx = 0; processIdx < nbProcesses; processIdx++ )
   {
      for (threadIdx=0; threadIdx < nbThreads; threadIdx++)
      {
         fprintf(stderr,"THREAD #%u\n",threadIdx);
         for (hwcIdx=0; hwcIdx < nbHwc; hwcIdx++)
         {
            fprintf(stderr,"HWC #%"PRIu32" = %"PRIu32"\n",hwcIdx,infoFunc->hwcInfo[processIdx][threadIdx][hwcIdx]);
         }
      }
   }
}



int get_height(avlTree_t* tree)
{
   return (tree == NULL) ? -1 : tree->height;
}

void set_height(avlTree_t* tree)
{
   int heightLeft;
   int heightRight;
   heightLeft = get_height(tree->left);
   heightRight = get_height(tree->right);
   tree->height = 1 + ((heightLeft > heightRight) ? heightLeft : heightRight);
}

avlTree_t* init(unsigned long key, void* value, avlTree_t * left, avlTree_t * right)
{
   avlTree_t * tree;

   tree = malloc(sizeof(*tree));
   tree->key = key;
   tree->value = value;
   tree->left = left;
   tree->right = right;
   set_height(tree);
   return tree;
}

avlTree_t* left_rotation(avlTree_t* tree)
{
   avlTree_t * rotTree = tree->right;
   tree->right = rotTree->left;
   rotTree->left = tree;
   set_height(rotTree->left);
   set_height(rotTree);
   return rotTree;
}

avlTree_t* right_rotation(avlTree_t* tree)
{
   avlTree_t* rotTree = tree->left;
   tree->left = rotTree->right;
   rotTree->right = tree;
   set_height(rotTree->right);
   set_height(rotTree);
   return rotTree;
}

avlTree_t * balancing(avlTree_t* tree)
{
   set_height(tree);

   if (get_height(tree->left) - get_height(tree->right) == 2)
   {
      if (get_height(tree->left->left) < get_height(tree->left->right))
         tree->left = left_rotation(tree->left);
      return right_rotation(tree);
   }
   if (get_height(tree->left) - get_height(tree->right) == -2)
   {
      if (get_height(tree->right->right) < get_height(tree->right->left))
         tree->right = right_rotation(tree->right);
      return left_rotation(tree);
   }
   return tree;
}

avlTree_t * insert(unsigned long key,void* value, avlTree_t* tree)
{
   if (tree == NULL)
      return init(key, value, NULL, NULL);
   if (key < tree->key)
      tree->left = insert(key, value, tree->left);
   else if (key > tree->key)
      tree->right = insert(key, value, tree->right);
   else
      tree->value = value;
   return balancing(tree);
}

avlTree_t * delete_node(unsigned long key, avlTree_t* tree)
{
   if (tree == NULL)
      return tree;
   if (key == tree->key)
      return delete_root(tree);
   if (key < tree->key)
      tree->left = delete_node(key, tree->left);
   else
      tree->right = delete_node(key, tree->right);
   return balancing(tree);
}

avlTree_t * delete_root(avlTree_t* tree)
{
   avlTree_t* memory;
   if (tree->left == NULL && tree->right == NULL)
   {
      free(tree);
      return NULL;
   }
   if (tree->left == NULL)
   {
      memory = tree->right;
      free(tree);
      return balancing(memory);
   }
   if (tree->right == NULL)
   {
      memory = tree->left;
      free(tree);
      return balancing(memory);
   }
   avlTree_t * b = last_right(tree->left);
   tree->key = b->key;
   tree->value = b->value;
   tree->left = delete_node(tree->key, tree->left);
   return balancing(tree);
}

avlTree_t* last_right(avlTree_t* tree)
{
   if (tree->right == NULL)
      return tree;
   return last_right(tree->right);
}

avlTree_t* last_left(avlTree_t* tree)
{
   if (tree->left == NULL)
      return tree;
   return last_left(tree->left);
}

avlTree_t* search(unsigned long key, avlTree_t* tree)
{
   if (tree == NULL || key == tree->key)
      return tree;
   if (key < tree->key)
      return search(key, tree->left);
   return search(key, tree->right);
}

avlTree_t* search_address(unsigned long key, avlTree_t* tree,int infoType)
{
   SinfoLoop* infoLoop = NULL;
   SinfoFunc* infoFct   = NULL;

   if (tree == NULL)
      return tree;

   if (infoType == PERF_LOOP)
   {
      infoLoop = tree->value;
      if (infoLoop->start <= key && infoLoop->stop >= key)
         return tree;
   }

   if (infoType == PERF_FUNC)
   {
      infoFct   = tree->value;
      if (infoFct->start <= key && infoFct->stop >= key)
      {
         return tree;
      }
   }

   //BROWSE DEEPLY INTO THE TREE
   if (key < tree->key)
   {
      return search_address(key,tree->left,infoType);
   }
   else
   {
      if (key > tree->key)
         return search_address(key,tree->right,infoType);
      else
         return NULL;
   }
}

void destroy(avlTree_t* tree)
{
   if (!tree) return;

   if (tree->left != NULL)
   {
      destroy(tree->left);
   }
   if (tree->right != NULL)
   {
      destroy(tree->right);
   }
   free(tree);
}

int get_nb_elements(avlTree_t* tree)
{
   if (tree != NULL)
      return (1 + get_nb_elements(tree->right) + get_nb_elements(tree->left));
   else
      return 0;
}

void print_tree(avlTree_t* tree , unsigned long h)
{
   unsigned long i;

   if (tree == NULL)
      return;

   print_tree(tree->right,h+1);

   for(i=0; i < h; i++)
      printf("  ");
   printf("%p (%d) %p\n", (void*)tree->key,tree->height,(void*)tree->value);

   print_tree(tree->left,h+1);
}
