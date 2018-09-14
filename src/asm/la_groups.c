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
#include <inttypes.h>

#include "libmasm.h"

extern int group_reg_isvect(group_t*, reg_t*);
extern void lcore_group_stride_group(group_t*);
extern void lcore_group_memory_group(group_t*, void*);

////////////////////////////////////////////////////////////////////////////
//                                                 To manage a group_data element                       //
////////////////////////////////////////////////////////////////////////////
/*
 * Creates a new group element
 * \param code used to know if it is a load or a store: {GRP_LOAD, GRP_STORE}
 * \param insn the instruction represented by this group
 * \param pos_param position of the memory parameter in the instruction
 * \param an initialized group element
 */
group_elem_t* group_data_new(char code, insn_t* insn, int pos_param)
{
   group_elem_t* gdat = lc_malloc0(sizeof(group_elem_t));

   gdat->code = code;
   gdat->insn = insn;
   gdat->pos_param = pos_param;

   return (gdat);
}

/*
 * Frees a group element
 * \param gdat a group element to free
 */
void group_data_free(group_elem_t* gdat)
{
   lc_free(gdat);
}

/*
 * Duplicates a group element
 * \param src a group element to duplicate
 * \return a copy of the input group element
 */
group_elem_t* group_data_dup(group_elem_t* src)
{
   group_elem_t* gdat = NULL;
   if (src != NULL) {
      gdat = lc_malloc0(sizeof(group_elem_t));
      gdat->code = src->code;
      gdat->insn = src->insn;
      gdat->pos_param = src->pos_param;
   }
   return (gdat);
}

////////////////////////////////////////////////////////////////////////////
//                                                       To manage an output element                         //
////////////////////////////////////////////////////////////////////////////
/*
 * Creates a new group
 * \param key a string containing the accessed virtual representation, used as key
 * \param loop the loop the group belongs to
 * \param filter_fct a function used during group printing to know if an element must be printed. Takes as
 * input:
 *    * an element of the group
 *    * a user value
 *    * return 1 if the element must be printed, else 0
 * \return a new initialized group structure
 */
group_t* group_new(char* key, loop_t* loop,
      int (*filter_fct)(struct group_elem_s*, void*))
{
   group_t* n = lc_malloc0(sizeof(group_t));
   n->key = lc_strdup(key);
   n->loop = loop;
   n->gdat = queue_new();
   n->filter_fct = filter_fct;
   return (n);
}

/*
 * Duplicates an existing group
 * \param src a group to duplicate
 * \return a copy of the group
 */
group_t* group_dup(group_t* src)
{
   group_t* dst = NULL;

   if (src != NULL) {
      dst = lc_malloc0(sizeof(group_t));
      dst->loop = src->loop;
      dst->key = lc_strdup(src->key);
      dst->gdat = queue_new();
      FOREACH_INQUEUE(src->gdat, gdat_it)
         queue_add_tail(dst->gdat, group_data_dup(gdat_it->data));
      dst->filter_fct = src->filter_fct;
   }
   return (dst);
}

/*
 * Adds an element in a group
 * \param o an existing group
 * \param gdat an existing group element
 */
void group_add_elem(group_t* o, group_elem_t* gdat)
{
   if (o != NULL && gdat != NULL
         && (gdat->code == GRP_STORE || gdat->code == GRP_LOAD )) {
      queue_add_tail(o->gdat, gdat);
   }
}

/*
 * Frees a group and all elements it contains
 * \param o a group to free
 */
void group_free(void* o)
{
   if (o != NULL) {
      group_t* g = (group_t*) o;
      FOREACH_INQUEUE(g->gdat, gdat_it)
         group_data_free(gdat_it->data);
      queue_free(g->gdat, NULL);
      //if (g->stride_insns != NULL)
      //   queue_free (g->stride_insns, NULL);
      if (g->touched_sets)
         lc_free(g->touched_sets);
      lc_free(g->key);
      lc_free(g);
   }
}

/*
 * Compares two groups
 * \param o1 an existing group
 * \param o2 an existing group
 * \return 1 if there is a difference, else 0
 */
int group_cmp(group_t* o1, group_t* o2)
{
   if (o1 != NULL && o2 != NULL) {
      if (queue_length(o1->gdat) != queue_length(o2->gdat))
         return (1);
      if (o1->loop != o2->loop)
         return (1);
      list_t* o2_dat = queue_iterator(o2->gdat);
      FOREACH_INQUEUE(o1->gdat, it)
      {
         group_elem_t* d1 = GET_DATA_T(group_elem_t*, it);
         group_elem_t* d2 = GET_DATA_T(group_elem_t*, o2_dat);

         if (INSN_GET_ADDR(d1->insn) != INSN_GET_ADDR(d2->insn))
            return (1);
         o2_dat = o2_dat->next;
      }
   }
   return (0);
}

/*
 * Gets the size of the group, using the filter function
 * \param o a group
 * \param user a user data given to the filter function
 * \return the number of group element not filtered by the filter function
 */
int group_get_size(group_t* o, void* user)
{
   int n_size = 0;
   if (o != NULL) {
      FOREACH_INQUEUE(o->gdat, gdat_it0)
      {
         group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it0);
         if (o->filter_fct != NULL && ((o->filter_fct)(gdat, user) == 1))
            n_size++;
      }
   }
   return (n_size);
}

/*
 * Gets the pattern of the group, using the filter function
 * \param o a group
 * \param user a user data given to the filter function
 * \return the pattern, composed of group element not filtered by the filter function
 */
char* group_get_pattern(group_t* o, void* user)
{
   int size = group_get_size(o, user);
   char* pattern = lc_malloc((size + 1) * sizeof(char));
   int i = 0;

   FOREACH_INQUEUE(o->gdat, gdat_it1) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1)
         pattern[i++] = gdat->code;
   }
   pattern[i] = '\0';
   return (pattern);
}

/*
 * Gets the pattern element at index n
 * \param o a group
 * \param n index of the pattern element to retrieve
 * \param user a user data given to the filter function
 * \return GRP_LOAD ('L') or GRP_STORE ('S') if no problem, else 0
 */
char group_get_pattern_n(group_t* o, int n, void* user)
{
   int size = group_get_size(o, user);
   int i = 0;

   if (n >= size)
      return (0);
   FOREACH_INQUEUE(o->gdat, gdat_it1) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i == n)
            return (gdat->code);
         i++;
      }
   }
   return (0);
}

/*
 * Gets the instruction at index n in the group
 * \param o a group
 * \param n index of the instruction to retrieve
 * \param user a user data given to the filter function
 * \return the instruction, else NULL if there are errors
 */
insn_t* group_get_insn_n(group_t* o, int n, void* user)
{
   int size = group_get_size(o, user);
   int i = 0;

   if (n >= size)
      return (NULL);
   FOREACH_INQUEUE(o->gdat, gdat_it1) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i == n)
            return (gdat->insn);
         i++;
      }
   }
   return (NULL);
}

/*
 * Gets the instruction address at index n in the group
 * \param o a group
 * \param n index of the instruction to retrieve
 * \param user a user data given to the filter function
 * \return the instruction address, else 0 if there are errors
 */
int64_t group_get_address_n(group_t* o, int n, void* user)
{
   int size = group_get_size(o, user);
   int i = 0;

   if (n >= size)
      return (0x0);
   FOREACH_INQUEUE(o->gdat, gdat_it1) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i == n)
            return (INSN_GET_ADDR(gdat->insn));
         i++;
      }
   }
   return (0x0);
}

/*
 * Gets the instruction opcode at index n in the group
 * \param o a group
 * \param n index of the instruction to retrieve
 * \param user a user data given to the filter function
 * \return the instruction opcode, else NULL if there are errors
 */
char* group_get_opcode_n(group_t* o, int n, void* user)
{
   int size = group_get_size(o, user);
   int i = 0;

   if (n >= size)
      return (NULL);
   FOREACH_INQUEUE(o->gdat, gdat_it1) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i == n)
            return (insn_get_opcode(gdat->insn));
         i++;
      }
   }
   return (NULL);
}

/*
 * Gets the instruction offset at index n in the group
 * \param o a group
 * \param n index of the instruction to retrieve
 * \param user a user data given to the filter function
 * \return the instruction offset, else 0 if there are errors
 */
int64_t group_get_offset_n(group_t* o, int n, void* user)
{
   int size = group_get_size(o, user);
   int i = 0;

   if (n >= size)
      return (0x0);
   FOREACH_INQUEUE(o->gdat, gdat_it1) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i == n)
            return (oprnd_get_offset(
                  insn_get_oprnd(gdat->insn, gdat->pos_param)));
         i++;
      }
   }
   return (0x0);
}

/*
 * Gets the innermost loop a group belongs to
 * \param o a group
 * \return the loop, else NULL if there are errors
 */
loop_t* group_get_loop(group_t* o)
{
   return ((o != NULL) ? o->loop : NULL);
}

/*
 * Gets the group span
 * \param o a group
 * \return the group span
 */
int group_get_span(group_t* o)
{
   return ((o != NULL) ? o->span : 0);
}

/*
 * Gets the group head
 * \param o a group
 * \return the group head
 */
int group_get_head(group_t* o)
{
   return ((o != NULL) ? o->head : 0);
}

/*
 *
 *
 */
char* group_get_stride_status(group_t* o)
{
   if (o == NULL)
      return ("Error");

   switch (o->s_status) {
   case SS_NA:
      return (SS_MSG_NA);
   case SS_OK:
      return (SS_MSG_OK);
   case SS_MB:
      return (SS_MSG_MB);
   case SS_VV:
      return (SS_MSG_VV);
   case SS_O:
      return (SS_MSG_O);
   case SS_RIP:
      return (SS_MSG_RIP);
   }
   return (SS_MSG_NA);
}

/*
 * Gets the group increment
 * \param o a group
 * \return the group increment
 */
int group_get_increment(group_t* o)
{
   return ((o != NULL) ? o->stride : 0);
}

/*
 *
 *
 */
char* group_get_memory_status(group_t* o)
{
   if (o == NULL)
      return ("Error");

   switch (o->m_status) {
   case MS_NA:
      return (MS_MSG_NA);
   case MS_OK:
      return (MS_MSG_OK);
   }
   return ("Error");
}

int group_get_accessed_memory(group_t* o)
{
   return ((o != NULL) ? o->memory_all : 0);
}

int group_get_accessed_memory_nooverlap(group_t* o)
{
   return ((o != NULL) ? o->memory_nover : 0);
}

int group_get_accessed_memory_overlap(group_t* o)
{
   return ((o != NULL) ? o->memory_overl : 0);
}

int group_get_unroll_factor(group_t* o)
{
   return ((o != NULL) ? o->unroll_factor : 0);
}

/**
 * Print offsets in most of cases
 */
static void print_group_offsets_regular(group_t* o, FILE* f_out, void* user,
      char sep)
{
   int i = 0;
   FOREACH_INQUEUE(o->gdat, gdat_it4)
   {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it4);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i > 0)
            fprintf(f_out, "%c", sep);
         i++;
         fprintf(f_out, "%"PRId64,
               oprnd_get_offset(insn_get_oprnd(gdat->insn, gdat->pos_param)));
      }
   }
}

/**
 * Print offsets function used by C.M.
 */
static void print_group_offsets_special(group_t* o, FILE* f_out, void* user,
      char sep)
{
   int i = 0;
   FOREACH_INQUEUE(o->gdat, gdat_it4)
   {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it4);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         oprnd_t* op = insn_get_oprnd(gdat->insn, gdat->pos_param);
         if (i > 0)
            fprintf(f_out, "%c", sep);
         i++;

         if ((reg_get_type(oprnd_get_base(op)) == RIP_TYPE)
               && (oprnd_get_index(op) == NULL)) {
            insn_t* next_in = INSN_GET_NEXT(gdat->insn);
            fprintf(f_out, "%"PRId64,
                  oprnd_get_offset(op) + INSN_GET_ADDR(next_in));
         } else
            fprintf(f_out, "%"PRId64, oprnd_get_offset(op));
      }
   }
}

/**
 * Print offsets function used by pcr.
 */
static void print_group_offsets_pcr(group_t* o, FILE* f_out, void* user,
      char sep)
{
   int i = 0;

   // Print offsets as regular mode
   FOREACH_INQUEUE(o->gdat, gdat_it) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i > 0)
            fprintf(f_out, "%c", sep);
         i++;
         fprintf(f_out, "%"PRId64,
               oprnd_get_offset(insn_get_oprnd(gdat->insn, gdat->pos_param)));
      }
   }

   // Then print instructions with there operands
   i = 0;
   FOREACH_INQUEUE(o->gdat, gdat_it1)
   {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);

      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         char buff[256];
         buff[0] = '\0';
         insn_print(gdat->insn, buff, sizeof(buff));
         if (i > 0)
            fprintf(f_out, "%c", sep);
         i++;
         fprintf(f_out, "%s", buff);
      }
   }
}

/**
 * Prints a group using a specific function for offsets
 * \param o an existing group
 * \param f_out a file where to print
 * \param user a user value given to the filter function
 * \param print_offsets a function used to print offsets of groups
 * \param sep used to separate several values in a same fields
 */
static void group_print_core(group_t* o, FILE* f_out, void* user,
      void (*print_offsets)(group_t*, FILE*, void*, char), char sep)
{
   if (f_out == NULL || o == NULL || queue_length(o->gdat) <= 0)
      return;
   int n_size = 0;
   int i = 0;

   // Get group size
   FOREACH_INQUEUE(o->gdat, gdat_it0) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it0);
      if (o->filter_fct != NULL && ((o->filter_fct)(gdat, user) == 1))
         n_size++;
   }

   if (n_size <= 0)
      return;
   fprintf(f_out, "%d;", n_size);

   //print pattern
   FOREACH_INQUEUE(o->gdat, gdat_it1) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1)
         fprintf(f_out, "%c", gdat->code);
   }
   fprintf(f_out, ";");

   //print addresses
   i = 0;
   FOREACH_INQUEUE(o->gdat, gdat_it2) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it2);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i > 0)
            fprintf(f_out, "%c", sep);
         i++;
         fprintf(f_out, "%"PRIx64, INSN_GET_ADDR(gdat->insn));
      }
   }
   fprintf(f_out, ";");

   //print opcodes
   i = 0;
   FOREACH_INQUEUE(o->gdat, gdat_it3) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it3);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         if (i > 0)
            fprintf(f_out, "%c", sep);
         i++;
         fprintf(f_out, "%s", insn_get_opcode(gdat->insn));
      }
   }
   fprintf(f_out, ";");

   //print offsets
   if (print_offsets != NULL)
      print_offsets(o, f_out, user, sep);
   fprintf(f_out, ";");

   fprintf(f_out, "%d;%d;", loop_get_id(o->loop), loop_get_nb_insns(o->loop));

   switch (o->s_status) {
   case SS_NA:
      fprintf(f_out, "%s;", SS_MSG_NA);
      break;
   case SS_OK:
      fprintf(f_out, "%s;", SS_MSG_OK);
      break;
   case SS_MB:
      fprintf(f_out, "%s;", SS_MSG_MB);
      break;
   case SS_VV:
      fprintf(f_out, "%s;", SS_MSG_VV);
      break;
   case SS_O:
      fprintf(f_out, "%s;", SS_MSG_O);
      break;
   case SS_RIP:
      fprintf(f_out, "%s;", SS_MSG_RIP);
      break;
   }
   fprintf(f_out, "%d;", o->stride);

   switch (o->m_status) {
   case MS_NA:
      fprintf(f_out, "%s;", MS_MSG_NA);
      break;
   case MS_OK:
      fprintf(f_out, "%s;", MS_MSG_OK);
      break;
   }
   fprintf(f_out, "%d;%d;%d;%d;%d;%d", o->memory_all, o->memory_nover,
         o->memory_overl, o->span, o->head, o->unroll_factor);

   fprintf(f_out, "\n");
}

/**
 * Prints a group in a human readable format
 * \param o an existing group
 * \param f_out a file where to print
 * \param user a user value given to the filter function
 */
static void group_print_text(group_t* o, FILE* f_out, void* user)
{
   if (f_out == NULL || o == NULL || queue_length(o->gdat) <= 0)
      return;

   int n_size = 0;
   char* pattern = NULL;
   int cpt = 0;
   char* s_status = NULL;
   char* m_status = NULL;
   int buffer_size = 256 * sizeof(char);
   char* insn = lc_malloc(buffer_size);

   // Get group size
   FOREACH_INQUEUE(o->gdat, gdat_it0) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it0);
      if (o->filter_fct != NULL && ((o->filter_fct)(gdat, user) == 1))
         n_size++;
   }
   if (n_size <= 0)
      return;

   pattern = lc_malloc((n_size + 1) * sizeof(char));
   pattern[n_size] = '\0';

   //compute pattern
   FOREACH_INQUEUE(o->gdat, gdat_it1) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it1);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1)
         pattern[cpt++] = gdat->code;
   }

   switch (o->s_status) {
   case SS_NA:
      s_status = SS_MSG_NA;
      break;
   case SS_OK:
      s_status = SS_MSG_OK;
      break;
   case SS_MB:
      s_status = SS_MSG_MB;
      break;
   case SS_VV:
      s_status = SS_MSG_VV;
      break;
   case SS_O:
      s_status = SS_MSG_O;
      break;
   case SS_RIP:
      s_status = SS_MSG_RIP;
      break;
   }

   switch (o->m_status) {
   case MS_NA:
      m_status = MS_MSG_NA;
      break;
   case MS_OK:
      m_status = MS_MSG_OK;
      break;
   }

   fprintf(f_out, "Group ***********************************\n");
   fprintf(f_out, "  loop:               %-3d, %d instructions\n",
         loop_get_id(o->loop), loop_get_nb_insns(o->loop));
   fprintf(f_out, "  group size:         %d elements\n", n_size);
   fprintf(f_out, "  pattern:            %s\n", pattern);
   fprintf(f_out, "  stride              %-3d        [%s]\n", o->stride,
         s_status);
   fprintf(f_out, "  all bytes accessed: %-3d bytes  [%s]\n", o->memory_all,
         m_status);
   fprintf(f_out, "    no overlap:       %-3d bytes  \n", o->memory_nover);
   fprintf(f_out, "    overlap:          %-3d bytes  \n", o->memory_overl);
   fprintf(f_out, "  instructions:\n");

   FOREACH_INQUEUE(o->gdat, gdat_it2) {
      group_elem_t* gdat = GET_DATA_T(group_elem_t*, gdat_it2);
      if (o->filter_fct != NULL && o->filter_fct(gdat, user) == 1) {
         insn_print(gdat->insn, insn, sizeof(buffer_size));
         fprintf(f_out, "     0x%"PRIx64" %s\n", INSN_GET_ADDR(gdat->insn),
               insn);
      }
   }
   fprintf(f_out, "  span:               %-3d bytes\n", o->span);
   fprintf(f_out, "  head:               %-3d bytes\n", o->head);
   fprintf(f_out, "  unroll factor:      %-3d\n", o->unroll_factor);
   fprintf(f_out, "\n");

   lc_free(pattern);
   lc_free(insn);
}

void group_print(group_t* group, FILE* output, void* user, int format)
{
   if (group == NULL || output == NULL)
      return;

   switch (format) {
   case GROUPING_FORMAT_LINE:
      group_print_core(group, output, user, &print_group_offsets_regular, ';');
      break;
   case GROUPING_FORMAT_CSV:
      group_print_core(group, output, user, &print_group_offsets_regular, ',');
      break;
   case GROUPING_FORMAT_TEXT:
      group_print_text(group, output, user);
      break;
   case GROUPING_FORMAT_CM:
      group_print_core(group, output, user, &print_group_offsets_special, ';');
      break;
   case GROUPING_FORMAT_PCR:
      group_print_core(group, output, user, &print_group_offsets_pcr, ';');
      break;

   default:
      break;
   }
}

/*
 * Extract a subgroup from a group according to a filter
 * \param group a group
 * \param mode a value for a filter
 *       - 0: keep elements which use vectorial registers
 *       - 1: keep elements which use non-vectorial registers
 * \return a new group to free when it is not used anymore
 */
group_t* group_filter(group_t* group, int mode)
{
   if (group == NULL)
      return (NULL);
   group_t* group_V = group_new(group->key, group->loop, group->filter_fct);

   // Before adding groups in the loop, check if it contains
   // instructions using R_reg and (XMM / YMM) _reg
   // In this case, split the group in two groups
   //if (group_isvect != NULL)
   {
      FOREACH_INQUEUE(group->gdat, it_gdat)
      {
         group_elem_t* gdat = GET_DATA_T(group_elem_t*, it_gdat);
         insn_t* insn = gdat->insn;
         int i = 0;

         // Check registers used by the instruction
         for (i = 0; i < insn_get_nb_oprnds(insn); i++) {
            if (i != gdat->pos_param) {
               oprnd_t* op = insn_get_oprnd(insn, i);
               if (oprnd_is_reg(op) == TRUE) {
                  // Reg is vectorial and we want vectorial elements
                  if (mode == 0
                        && group_reg_isvect(group_V, oprnd_get_reg(op))
                              == TRUE) {
                     group_add_elem(group_V, group_data_dup(gdat));
                  }
                  // Reg is not vectorial and we want not-vectorial elements
                  else if (mode == 1
                        && group_reg_isvect(group_V, oprnd_get_reg(op))
                              == FALSE) {
                     group_add_elem(group_V, group_data_dup(gdat));
                  }
               }
            }
         }
      }
   }
   lcore_group_stride_group(group_V);
   lcore_group_memory_group(group_V, 0);
   return (group_V);
}

