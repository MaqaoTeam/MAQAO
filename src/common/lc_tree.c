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

/**
 * \file
 * */
#include "libmcommon.h"

///////////////////////////////////////////////////////////////////////////////
//                               tree functions                              //
///////////////////////////////////////////////////////////////////////////////
/*
 * Create a new tree
 * \param data element of the tree
 * \return a new tree
 */
tree_t *tree_new(void *data)
{
   tree_t *node;
   node = (tree_t *) lc_malloc(sizeof(tree_t));
   node->prev = node->next = NULL;
   node->parent = node->children = NULL;
   node->data = data;
   return node;
}

/*
 * Deletes and frees (if f not null) a tree and all its children
 * \param tree a tree
 * \param f a function used to free an element, with the following prototype:
 *  	void < my_function > (void* data); where:
 *			- data is the element to free
 */
void tree_free(tree_t* t, void (*f)(void*))
{
   assert(t != NULL);

   // t is first child of its parent
   if (t->parent != NULL && t->parent->children == t)
      t->parent->children = t->next;

   // update prev
   if (t->prev != NULL)
      t->prev->next = t->next;

   // update next
   if (t->next != NULL)
      t->next->prev = t->prev;

   // free children
   while (t->children != NULL) {
      tree_t *next = t->children->next;
      tree_free(t->children, f);
      t->children = next;
   }

   // callback for associated data
   if (f != NULL)
      f(t->data);

   lc_free(t);
}

/*
 * Remove a given child from a tree
 * \param parent a root
 * \param node a direct child of the root to delete
 * \return the deleted node
 */
tree_t* tree_remove_child(tree_t *parent, tree_t *node)
{
   int rank = 0;
   tree_t* children;
   children = parent->children;
   while (children != NULL) {
      if (node == children) {
         if (children->prev) {
            children->prev->next = children->next;
         }
         if (children->next) {
            children->next->prev = children->prev;
         }
         if (!rank)
            parent->children = children->next;
         node->parent = NULL;
         node->next = NULL;
         node->prev = NULL;
         break;
      }
      children = children->next;
      rank++;
   }
   return node;
}

/*
 * Gets the depth of a node
 * \param t a node
 * \return dept of \e t or -1 if there is a problem
 */
int tree_depth(tree_t *t)
{
   int depth = 0;
   if (t == NULL)
      return (-1);

   while (t->parent) {
      depth++;
      t = t->parent;
   }
   return depth;
}

/*
 * Inserts create and edge from \e parent to \e node
 * \param parent a node
 * \param node a node
 * \return node
 */
tree_t *tree_insert(tree_t *parent, tree_t *node)
{
   tree_t *children, *iter;
   if (parent == NULL || node == NULL)
      return node;
   children = parent->children;

   //check that node is not already a child of parent
   for (iter = children; iter != NULL; iter = iter->next) {
      if (iter == node) {
         return (node);
      }
   }
   node->parent = parent;
   node->next = children;
   if (children)
      children->prev = node;
   parent->children = node;
   return node;
}

/*
 * Checks if \e node is an ancestor of \e descendant
 * \param node a node
 * \param descendant a node
 * \return 1 if \e node is an ancestor of \e descendant, else 0
 */
int tree_is_ancestor(tree_t *node, tree_t *descendant)
{
   if (node == NULL || descendant == NULL)
      return 0;

   while (descendant) {
      if (descendant->parent == node)
         return 1;
      descendant = descendant->parent;

   }
   return 0;
}

/*
 * Traverses a tree and exectues a function of each node
 * \param node a tree
 * \param f a function exectued of each element. It must have the following prototype:
 *		void < my_function > (void* elemen, void* data); where:
 *			- elem is the element
 *			- data a 2nd parameter given by the user 
 * \param data a user data
 * \return 
 */
int tree_traverse(tree_t *node, traversefunc_t f, void *data)
{
   if (node == NULL)
      return (0);
   if (f(node, data))
      return 1;
   if (node->children) {
      tree_t *child = node->children;
      while (child) {
         tree_t *current;
         DBGMSG("node %p child %p\n", node, child);
         current = child;
         child = child->next;
         if (tree_traverse(current, f, data))
            return 1;
      }
   }
   return 0;
}

/*
 * Checks if a tree node has a parent
 * \param node a tree node
 * \return 1 if there is a parent, else 0
 */
int tree_hasparent(tree_t* node)
{
   if (node != NULL && node->parent != NULL)
      return (1);
   else
      return (0);
}

/*
 * Get the data in an element of a tree
 * \param node a tree
 * \return the data or PTR_ERROR if there is a problem
 */
void* tree_getdata(tree_t* node)
{
   return (node != NULL) ? node->data : PTR_ERROR;
}

/*
 * Returns the parent node
 * \param node a tree node
 * \return parent node
 */
tree_t* tree_get_parent(tree_t* node)
{
   return (node != NULL) ? node->parent : PTR_ERROR;
}

/*
 * Returns the children node (in general, the first child)
 * \param node a tree node
 * \return children node
 */
tree_t* tree_get_children(tree_t* node)
{
   return (node != NULL) ? node->children : PTR_ERROR;
}
