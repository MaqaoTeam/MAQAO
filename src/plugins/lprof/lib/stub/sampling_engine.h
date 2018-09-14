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

// CF sampling_engine.c

#ifndef __SAMPLING_ENGINE_H__
#define __SAMPLING_ENGINE_H__

#include "deprecated_shared.h" // returnInfo_t

/*
 * Samples an application with perf_events
 * \param cmd command to run
 * \param output_path path to the lprof output/experiment directory
 * \param sampling_period sampling period (number of events per sample)
 * \param hwc_list List of hardware events name+period to sample. Ex: "CPU_CLK_THREAD_UNHALTED:REF_XCLK@500003,MEM_LOAD_UOPS_RETIRED:L1_HIT@500003
 * \param user_guided -1 = user control disabled, 0 = CTRL+Z pauses/resumes sampling and n > 0 = delays sampling from n seconds
 * \param backtrace_mode Backtraces/callchains mode: OFF, CALL, STACK and BRANCH
 * \param cpu_list "0,1,2,3" to limit profiling to CPU0-3
 * \param mpi_target path to application executable if masked to MAQAO by MPI command (inherit-mode only)
 * \param nb_sampler_threads number of threads to process samples in ring buffers
 * \param inherit boolean true if inherit mode requested, ptrace-mode otherwise
 * \param sync boolean true if syncronous tracer requested, asyncronous otherwise (ptrace-mode only)
 * \param finalize_signal Signal used by some parallel launchers to notify ranks finalization
 * \param verbose boolean true if less critical warnings has to be displayed. Forwarded from args.verbose
 * \param max_buf_MB maximum memory buffer size in MB
 * \param files_buf_MB temporary files buffer size in MB
 * \param max_files_MB maximum total temporary files size in MB
 */
extern returnInfo_t sample (const char *cmd, const char* output_path,
                            unsigned sampling_period, const char* hwc_list,
                            const int user_guided, const int backtrace_mode,
                            const char* cpu_list, const char *mpi_target,
                            unsigned nb_sampler_threads, unsigned sampling_engine,
                            boolean_t sync, int finalize_signal, boolean_t verbose,
                            size_t max_buf_MB, size_t files_buf_MB, size_t max_files_MB);
#endif // __SAMPLING_ENGINE_H__
