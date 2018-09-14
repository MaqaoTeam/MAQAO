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

// Declares functions shared by both inherit and ptrace-based sampling engines

#ifndef __SAMPLING_ENGINE_SHARED_H__
#define __SAMPLING_ENGINE_SHARED_H__

#include <stdio.h>
#include <sys/ioctl.h>  // ioctl
#ifdef __LIBUNWIND__
#include <libunwind.h>
#include "unwind.h" // unwind_context_t
#endif
#include "perf_util.h"  // perf_setup_list_events, perf_read_buffer*
// WARNING: perf_util.h includes a local perf_event.h !
#include "libmcommon.h" // hashtable_t...
#include "deprecated_shared.h"
#include "sampling_engine_data_struct.h" // buf_t, lprof_hashtable_t

// Sampling engines/modes
#define SAMPLING_ENGINE_INHERIT 1
#define SAMPLING_ENGINE_PTRACE  2
#define SAMPLING_ENGINE_TIMERS  3

// Number of hits for given HW event or CPU
typedef uint32_t hits_nb_t; // remark: MAX_UINT32 => 50 days in same TID/IP/(HW-event/CPU)... !
typedef uint32_t cpu_id_t;

#ifdef __LIBUNWIND__
typedef struct {
   unw_addr_space_t addr_space;
   unwind_context_t context;
} unwind_data_t;
#endif

typedef struct {
   hits_nb_t nb_hits;
   uint32_t nb_IPs;
   uint64_t *IPs;
} IP_callchain_t;

// Structure to save samples data in sampler thread buffers (in lprof_hashtable_t objects)
typedef struct {
   hits_nb_t *eventsNb; // eventsNb [events_per_group]
   lprof_queue_t *callchains;
} IP_events_t;

// Gather data structures + related allocator
typedef struct {
   buf_t *buf;

   // allocated in buf + inserted data
   lprof_hashtable_t *tid2ipt; // thread ID to (IP table = IP to IP_events)
   lprof_hashtable_t *tid2cpu; // thread ID to CPUs histogram
} sampler_data_buf_t;

// Sampler-local data (one structure per sampler thread)
typedef struct {
   // Data structures and related allocators saving samples data to RAM and files
   sampler_data_buf_t *mem;
   sampler_data_buf_t *file;
   sampler_data_buf_t *cur; // cur value is 'mem' and then 'file'

   // Files to save samples (IP_events_t structures)
   char *smp_file_name, *smp_idx_file_name;
   FILE *fp_smp, *fp_smp_idx;

   // Files to save CPUs used for each system thread
   char *cpu_file_name, *cpu_idx_file_name;
   FILE *fp_cpu, *fp_cpu_idx;

   uint64_t lost_events; // nb of events lost
   uint64_t coll_events; // nb of events collected
#ifdef __LIBUNWIND__
   hashtable_t *unwind_data; // unwind_data_t [pid]
#endif
} sampler_data_t;

// Sampling context, shared by sampling-related routines
typedef struct {
   // Coming from legacy instru_session
   cpu_id_t online_cpus;
   int uarch;
   char* eventsList;
   uint64_t* sampleTypesList;      // bit fields to store all sample types used

   // Initialy present in refactored code
   unsigned sampling_engine;
   boolean_t start_enabled;
   pid_t child_pid;          /**<Child (application) PID*/
   const char *output_path;
   boolean_t *can_group;
   unsigned events_per_group;     /**<Number of events per group*/
   sampler_data_t *sampler_data; // data used by sampler threads
   unsigned nb_sampler_threads; // number of entries in sampler_data
   size_t mmap_size;           /**<ring buffer length in bytes*/
   perf_event_desc_t *fds;     /**<common fds, duplicate for each new thread*/
   void *UG_data; /**<used by threadUG_routine*/
   boolean_t verbose;
   size_t max_files_size; // maximum size, in Megabytes, for temporary files
   size_t files_buf_size; // size of files buffer
   boolean_t emergency_stop;
} smpl_context_t;

// Methods to handle sampler thread data (buffer/allocator and related tables)
sampler_data_buf_t *sampler_data_buf_new (size_t buf_size);
void sampler_data_buf_reset (sampler_data_buf_t *sampler_data_buf);
void sampler_data_buf_free  (sampler_data_buf_t *sampler_data_buf);

// Flushes content of files buffer to related files
void dump_to_files (const smpl_context_t *context, sampler_data_t *sampler_data);

/* Creates a "done" file in experiment path
 * Useful to notify "meta" applications waiting for lprof completion */
void touch_done_file (const char *output_path);

/* Cleanly aborts lprof execution:
 * - kills a process by its PID (in general: target application)
 * - creates a done file in experiment directory
 * - exits (self) */
void clean_abort (pid_t pid, const char *output_path);

// We want this inlined: defined in this header file
static inline void enable_events_group (perf_event_desc_t *fds, smpl_context_t *context)
{
   DBGMSG ("Enabling events for CPU/TID=%d\n", fds[0].cpu);
   unsigned i; for (i = 0; i < context->events_per_group; i++) {
      if (ioctl (fds[i].fd, PERF_EVENT_IOC_ENABLE, 0) == -1) {
         ERRMSG ("Cannot enable events on CPU%u\n", fds[0].cpu);
         perror ("ioctl (PERF_EVENT_IOC_ENABLE)");
         clean_abort (context->child_pid, context->output_path);
      }
   }
}

// We want this inlined: defined in this header file
static inline void disable_events_group (perf_event_desc_t *fds, smpl_context_t *context)
{
   DBGMSG ("Disabling events for CPU/TID=%d\n", fds[0].cpu);
   unsigned i; for (i = 0; i < context->events_per_group; i++) {
      if (ioctl (fds[i].fd, PERF_EVENT_IOC_DISABLE, 0) == -1) {
         ERRMSG ("Cannot disable events on CPU%u\n", fds[0].cpu);
         perror ("ioctl (PERF_EVENT_IOC_DISABLE)");
         clean_abort (context->child_pid, context->output_path);
      }
   }
}

IP_callchain_t *lookup_IP_callchain (const lprof_queue_t *callchains,
                                     uint32_t nb_IPs, const uint64_t *IPs);

/* Saves sample data to lprof internal structures (per thread/IP hashtables)
 * Tightly coupled with what comes next in lprof...
 * Present implementation based on old lprof data structures (not optimal...) */
void save_sample_in_results (smpl_context_t *context,
                             uint64_t ip, uint32_t tid,
                             int rank, uint32_t cpu,
                             sampleInfo_t *callchain,
                             sampler_data_t *sampler_data);

/* Creates a perf-events group (with mmap region to access common ring buffer) for a given CPU or thread.
 * Samples are produced to ring buffer (i.e. sampling is effectively started) only when related
 * perf-events are enabled. Except for the "user-guided" mode, events are created enabled.
 * \param context sampling engine context
 * \param cpu CPU rank
 * \param tid thread ID */
perf_event_desc_t *_start_sampling (smpl_context_t *context, unsigned cpu, pid_t tid);

/* Disables and unallocates a perf-events group.
 * Remaining samples still present in ring buffer are flushed (processed) */
void _stop_sampling (smpl_context_t *context, perf_event_desc_t *group_fds, sampler_data_t *sampler_data);

// At each overflow, process all data present in the perf_event_open ring buffer
void process_overflow (smpl_context_t *context, perf_event_desc_t *group_fds, sampler_data_t *sampler_data);

#endif // __SAMPLING_ENGINE_SHARED_H__
