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

/* Declares functions used only by the inherit-based sampling engine */

#ifndef __SAMPLING_ENGINE_INHERIT_H__
#define __SAMPLING_ENGINE_INHERIT_H__

#include "sampling_engine_shared.h" // smpl_context_t

// Enables/disables all events groups (for all CPUs)
void enable_all_cpus  (void **UG_data);
void disable_all_cpus (void **UG_data);

/* Collects samples using the inherit mode of perf-events
 * \param context sampling context
 * \param nprocs number of CPUs to collect
 * \param wait_pipe pipe used to synchronize with application start
 * \param cpu_array list of CPUs to collect */
void inherit_sampler (smpl_context_t *const context, unsigned nprocs, int *wait_pipe, unsigned *cpu_array);

#endif // __SAMPLING_ENGINE_INHERIT_H__
