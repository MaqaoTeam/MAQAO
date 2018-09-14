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

// CF generate_metafile_shared.c

#ifndef __GENERATE_METAFILE_SHARED_H__
#define __GENERATE_METAFILE_SHARED_H__

#include <stdint.h> // uint32_t...
#include "binary_format.h" // lprof_loop_t
#include "libmasm.h" // loop_t

/* Sets block ranges info to a lprof loop (lprof_loop_t) from basic blocks
 * Ranges are contiguous blocks sequences */
unsigned loop_get_ranges(loop_t* l, lprof_loop_t* lprofLoop);

/* Returns loop hierarchical level:
 * - SINGLE_LOOP (no parent + no children)
 * - INNERMOST_LOOP (parent + no children)
 * - OUTERMOST_LOOP (no parent + children)
 * - INBETWEEN_LOOP (parent + children) */
int get_loop_level (loop_t* loop);

/* Returns children loops, as an array of IDs */
unsigned loop_get_children (loop_t* l, uint32_t** childrenList);

// Returns physical (target) to symbolic (link) index
hashtable_t *get_phy2sym (const char *exe_name);

#endif // __GENERATE_METAFILE_SHARED_H__
