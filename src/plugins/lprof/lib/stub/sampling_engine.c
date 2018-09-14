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

/* Defines the sample() function implementing the new, refactored sampling engine
 * TODO: simplify/remove precise-IP checks, since not really reliable */

#define _GNU_SOURCE      // sched_setaffinity
#include <stdio.h>
#include <stdint.h>      // uint64_t...
#include <inttypes.h>    // PRIu64 etc.
#include <sys/time.h>    // gettimeofday
#include <pthread.h>     // pthread_create...
#include <sys/ioctl.h>   // ioctl (fd, PERF_EVENT_IOC_*, ...)
#include <sys/stat.h>    // mkdir
#include <dirent.h>      // opendir
#include <sys/types.h>   // waitpid, mkdir
#include <sys/wait.h>    // waitpid
#include <sys/ptrace.h>  // ptrace
#include <unistd.h>      // pipe, open/read/write/close, fork
#include <signal.h>      // signal, sigprocmask
#include <errno.h>       // errno, for mkdir
#include <sched.h>       // sched_setaffinity, CPU_ZERO/SET

#ifdef __LIBUNWIND__
#include <sys/mman.h> // munmap
#include <libunwind.h>
#include "unwind.h" // PERF_STACK_USER_SIZE
// CF arch/x86/include/uapi/asm/perf_regs.h in kernel source tree
#define PERF_REG_X86_BP 6
#define PERF_REG_X86_SP 7
#endif

#include "binary_format.h" // lprof_sample_t...
#include "utils.h" // perf_utils_readhex
#include "consts.h" // LPROF_VERBOSITY_OFF
#include "deprecated_shared.h"
#include "sampling_engine_shared.h"  // smpl_context_t, clean_abort
#include "sampling_engine_inherit.h" // inherit_sampler, enable/disable_all_cpus
#include "sampling_engine_ptrace.h"  // tracer_new, create_ptrace_sampler, enable_disable_all_threads
#include "sampling_engine_timers.h"  // timers_sampler

#define MMAP_PAGES      4 // Default number of mmap pages per ring buffer (1 per thread)

extern void dump_collect_data (smpl_context_t *context, const char *process_path, int64_t walltime);

pid_t application_pid; // TODO: try to remove this
const char *exp_path;  // TODO: try to remove this

static int check_perf_event_paranoid ()
{
   int paranoid = 3; // Possible values are 2,1,0 and -1

   FILE *fp = fopen ("/proc/sys/kernel/perf_event_paranoid", "r");
   if (!fp) {
      ERRMSG ("[MAQAO] Kernel is not compatible with sampling instrumentation (too old).\n");
      return -1;
   }

   int ret = fscanf (fp, "%d", &paranoid);
   fclose (fp);

   if (ret != 1) {
      ERRMSG ("[MAQAO] Cannot parse perf_event_paranoid level.\n");
      return -2;
   }

   if (paranoid >= 2) {
      ERRMSG ("[MAQAO] You don't have the permission to access the performance counters.\n"
              "[MAQAO] Consider changing the value of /proc/sys/kernel/perf_event_paranoid:\n"
              "\t\t-1 - No restrictions.\n"
              "\t\t 0 - Allow access to CPU-specific data but not raw tracepoint samples.\n"
              "\t\t 1 - Allow both kernel and user measurements. (recommended)\n"
              "\t\t 2 - Only allow user-space measurements.\n");
      return -1;
   }

   return 0;
}

// Parses CPU list string (eg. "0,1,4") to array [0,1,4]
static void parse_cpu_list (const char *cpu_list, unsigned **cpu_array,
                            long *nprocs)
{
   // Counts the number of CPUs in cpu_list
   char *buf = lc_strdup (cpu_list); strtok (buf, ",");
   int nb_tokens = 1;
   while (strtok (NULL, ",") != NULL) nb_tokens++;
   lc_free (buf);
   *nprocs = nb_tokens;

   // Copies parsed CPUs from cpu_list to cpu_array
   *cpu_array = lc_malloc (nb_tokens * sizeof (*cpu_array)[0]);
   buf = lc_strdup (cpu_list); (*cpu_array)[0] = atoi (strtok (buf, ","));
   nb_tokens = 1; char *tok;
   while ((tok = strtok (NULL, ",")) != NULL)
      (*cpu_array) [nb_tokens++] = atoi (tok);
   lc_free (buf);
}

/* Checks whether perf-events can be opened separately
 * In case of failure, try fallback to non-precise IP */
static boolean_t can_open_separately (smpl_context_t *context)
{
   unsigned i;
   boolean_t failed = FALSE;
   boolean_t failed_precise_ip = FALSE;
   perf_event_desc_t *const fds = context->fds;

   // inherit mode needs CPU set and one group per CPU
   const int cpu = context->sampling_engine == SAMPLING_ENGINE_INHERIT ? 0 : -1;

   for (i=0; i<context->events_per_group; i++) {
      struct perf_event_attr *const hw = &(fds[i].hw);

      // Try without precise IP
      hw->precise_ip = 0;
      int fd = perf_event_open (hw, context->child_pid,
                                cpu, -1, 0 /* no group, no flags */);
      if (fd == -1) {
         failed = TRUE;
         DBG (fprintf (stderr, "Cannot perf_event_open %s\n", fds[i].name); \
              perror ("");)
            }
      else {
         close (fd);

         // Try with precise IP (SAMPLE_IP must have constant skid)
         hw->precise_ip = 1;
         int fd_precise_ip = perf_event_open (hw, context->child_pid,
                                              cpu, -1, 0);
         if (fd_precise_ip == -1) {
            if (context->verbose)
               WRNMSG ("Precise IP not supported for %s\n", fds[i].name);
            failed_precise_ip = TRUE;
         } else
            close (fd_precise_ip);
      }
   }

   if (failed_precise_ip) {
      if (context->verbose)
         WRNMSG ("Precise IP will be disabled for all events to try to measure them together");
      for (i=0; i<context->events_per_group; i++) fds[i].hw.precise_ip = 0;
   }

   return !failed;
}

/* Tries to group perf-events
 * In case of failure, try fallback to non precise-IP */
static void try_to_group (smpl_context_t *context)
{
   const unsigned nb_fds = context->events_per_group;
   if (nb_fds < 2) return;

   unsigned i;
   int group_leader = -1;
   perf_event_desc_t *const fds = context->fds;

   // inherit mode needs CPU set and one group per CPU
   const int cpu = context->sampling_engine == SAMPLING_ENGINE_INHERIT ? 0 : -1;

   for (i=0; i<nb_fds; i++) {
      int fd = perf_event_open (&(fds[i].hw), context->child_pid,
                                cpu, group_leader, 0 /* no flags */);

      if (fd == -1 && i == 0) { // Failed to open the group leader
         ERRMSG ("Cannot perf_event_open %s\n", fds[i].name);
         perror ("");
         clean_abort (context->child_pid, context->output_path);
      }
      else if (fd == -1) { // Failed to open a group follower
         struct perf_event_attr *const hw = &(fds[i].hw);
         if (hw->precise_ip == 1) {
            hw->precise_ip = 0;
            fd = perf_event_open (&(fds[i].hw), context->child_pid, 0, group_leader, 0);
         }
         if (fd == -1) {
            if (context->verbose)
               WRNMSG ("Cannot group %s with previous events: will be measured separately\n",
                       fds[i].name);
            context->can_group[i-1] = FALSE;
         } else {
            if (context->verbose)
               WRNMSG ("Cannot group %s with previous events in precise mode, disabled for this event\n",
                       fds[i].name);
         }
      }
      else if (i == 0) { // Succeeded in opening the leader
         group_leader = fd;
      }
      else { // Succeeded in opening a follower
         context->can_group[i-1] = TRUE;
      }

      fds[i].fd = fd; // to close later, maybe perf_event_open needs previous fd staying open to detect conflicts
   }

   // Cleanup: was a dryrun !
   for (i=0; i<nb_fds; i++) {
      if (fds[i].fd != -1) {
         close (fds[i].fd);
         fds[i].fd = -1;
      }
   }
}

// Closes file descriptors potentially opened during dryrun and restores sampling mode (overflow)
static void dryrun_multiplexing_cleanup (perf_event_desc_t *fds, unsigned nb_fds, uint64_t *sample_period)
{
   unsigned i;
   for (i=0; i<nb_fds; i++) {
      // Close/reset opened perf_events
      if (fds[i].fd != -1) {
         close (fds[i].fd);
         fds[i].fd = -1;
      }
      fds[i].hw.sample_period = sample_period[i]; // restore sampling period
   }
}

// Warns if an event was multiplexed (ie. part of life out of PMU)
static void dryrun_multiplexing (smpl_context_t *context)
{
   unsigned i;
   const unsigned nb_fds = context->events_per_group;
   for (i=1; i<nb_fds; i++) {
      if (!context->can_group [i-1]) return;
   }

   uint64_t sample_period [nb_fds];
   perf_event_desc_t *const fds = context->fds;

   // inherit mode needs CPU set and one group per CPU
   const int cpu = context->sampling_engine == SAMPLING_ENGINE_INHERIT ? 0 : -1;

   // Try to open perf_events
   fds[0].fd = -1;
   for (i=0; i<nb_fds; i++) {
      sample_period[i] = fds[i].hw.sample_period; // save sampling period
      fds[i].hw.sample_period = 0; // disable sampling => counting mode
      int fd = perf_event_open (&(fds[i].hw), 0 /* this thread */,
                                cpu, fds[0].fd, 0);
      if (fd == -1) {
         DBGMSG0 ("Cannot perf_event_open to dryrun multiplexing\n");
         dryrun_multiplexing_cleanup (fds, i+1, sample_period);
         return;
      } else
         fds[i].fd = fd;
   }

   // Start counting
   if (ioctl (fds[0].fd, PERF_EVENT_IOC_ENABLE, 0) == -1) {
      DBG(perror ("ioctl (PERF_EVENT_IOC_ENABLE) multiplexing");)
         dryrun_multiplexing_cleanup (fds, nb_fds, sample_period);
      return;
   }

   // Dummy payload
   float s = 0.0f; for (i=0; i < 1000*1000; i++) s += i;
   fds[0].idx = s; // idx not used: prevents dummy payload from being optimized out

   // Stop counting
   if (ioctl (fds[0].fd, PERF_EVENT_IOC_DISABLE, 0) == -1) {
      DBG(perror ("ioctl (PERF_EVENT_IOC_DISABLE) multiplexing");)
         dryrun_multiplexing_cleanup (fds, nb_fds, sample_period);
      return;
   }

   boolean_t has_mux = FALSE;
   for (i=0; i<nb_fds; i++) {
      // PERF_FORMAT_GROUP not specified
      struct { uint64_t value, time_enabled, time_running, id; } data;
      if (read (fds[i].fd, &data, sizeof data) == -1) {
         DBG (fprintf (stderr, "Cannot read time enabled/running for event %s\n",
                       fds[i].name); \
              perror ("read (dryrun_multiplexing)");)
            dryrun_multiplexing_cleanup (fds, nb_fds, sample_period);
         return;
      }

      if (data.time_running > 0 && data.time_enabled > 0) {
         const float scaling_factor = (float) data.time_running / (float) data.time_enabled;
         if (scaling_factor < 0.95f) {
            if (context->verbose)
               WRNMSG ("multiplexing occured for event %s (running only %.0f%% of enabled time)\n", fds[i].name, scaling_factor * 100.0f);
            has_mux = TRUE;
         }
      }
   }

   if (has_mux && context->verbose) WRNMSG ("lprof does not compensate multiplexing\n");

   dryrun_multiplexing_cleanup (fds, nb_fds, sample_period);
}

static size_t get_nb_events (const char *hwc_list)
{
   size_t nb = 0;

   const char *s;
   for (s = hwc_list; *s != '\0'; s++)
      if (*s == ',') nb++;

   return nb + 1;
}

/* Set context->eventsList, hwc_period and raw_code
 * Parse hwcList "HWC_NAME_1@THRESHOLD_VALUE_1,HWC_NAME_2@THRESHOLD_VALUE_2[,...]" */
static void set_context_evlist_custom (smpl_context_t *context, const char *hwcList,
                                       uint64_t *hwc_period, int64_t *raw_code,
                                       const char *output_path)
{
   char *cp_hwcList = lc_strdup (hwcList);
   char *pos = cp_hwcList;
   int i = 0;

   context->eventsList = lc_malloc (context->events_per_group * MAX_COUNTER_LENGTH);
   context->eventsList[0] = '\0';

   while (pos != NULL) {
      //Parse the HWC_NAME
      char *token = strsep (&pos, "@");
      if (pos == NULL) {
         ERRMSG ("ERROR : Bad format (Missing '@' delimiter)\n");
         ERRMSG ("Example : HWC_NAME_1@THRESHOLD_VALUE_1[,...]\n");
         clean_abort (0, output_path);
      }
      char *name = lc_strdup (token);
      strcat (context->eventsList, token);
      strcat (context->eventsList, ",");

      //Parse the THRESHOLD_VALUE
      token = strsep (&pos, ",");
      char *end; int period = strtol (token, &end, 10);
      if (token == end) {
         ERRMSG ("ERROR : Bad format (Missing threshold value after '@')\n");
         ERRMSG ("Example : HWC_NAME_1@THRESHOLD_VALUE_1[,...]\n");
         clean_abort (0, output_path);
      }
      hwc_period [i] = period;

      int64_t tmpRawCode = perf_utils_readhex (name);
      raw_code [i] = tmpRawCode ? tmpRawCode : -1;
      i++;
   }

   // Delete the last ','
   char *last_comma = strrchr (context->eventsList, ',');
   if (last_comma != NULL)
      *last_comma = '\0';

   lc_free (cp_hwcList);
}

// Sets data specific to each sampler thread: buffers, file pointers and names
static void init_sampler_data (smpl_context_t *context, const char *process_path, size_t max_buf_MB, const int backtrace_mode)
{
   context->sampler_data = lc_malloc (context->nb_sampler_threads *
                                      sizeof context->sampler_data[0]);

   unsigned i;
   for (i=0; i<context->nb_sampler_threads; i++) {
      sampler_data_t *sd = &(context->sampler_data[i]);

      // Initially, no files buffer and memory buffer equally shared across all threads
      sd->mem  = sampler_data_buf_new ((max_buf_MB * 1024 * 1024) / context->nb_sampler_threads);
      sd->file = NULL;
      sd->cur  = sd->mem;

      // smp*.tmp
      sd->smp_file_name = lc_malloc (strlen (process_path) +
                                     strlen ("/smp_999_999.tmp") + 1);
      sprintf (sd->smp_file_name, "%s/smp_%u_%u.tmp",
               process_path, i+1, context->nb_sampler_threads);
      sd->fp_smp = NULL;

      // smp_idx*.tmp
      sd->smp_idx_file_name = lc_malloc (strlen (process_path) +
                                         strlen ("/smp_idx_999_999.tmp") + 1);
      sprintf (sd->smp_idx_file_name, "%s/smp_idx_%u_%u.tmp",
               process_path, i+1, context->nb_sampler_threads);
      sd->fp_smp_idx = NULL;

      // cpu*.tmp
      sd->cpu_file_name = lc_malloc (strlen (process_path) +
                                     strlen ("/cpu_999_999.tmp") + 1);
      sprintf (sd->cpu_file_name, "%s/cpu_%u_%u.tmp",
               process_path, i+1, context->nb_sampler_threads);
      sd->fp_cpu = NULL;

      // cpu_idx*.tmp
      sd->cpu_idx_file_name = lc_malloc (strlen (process_path) +
                                         strlen ("/cpu_idx_999_999.tmp") + 1);
      sprintf (sd->cpu_idx_file_name, "%s/cpu_idx_%u_%u.tmp",
               process_path, i+1, context->nb_sampler_threads);
      sd->fp_cpu_idx = NULL;

      sd->lost_events = 0;
      sd->coll_events = 0;

#ifdef __LIBUNWIND__
      if (backtrace_mode == BACKTRACE_MODE_STACK)
         sd->unwind_data = hashtable_new (direct_hash, direct_equal);
#endif
   }
}

// Sets context and allocates hashtables/arrays for a HW counters based engine
static void init_context_hwc (smpl_context_t *context, unsigned sampling_period,
                              const char* hwc_list, const char *default_hwc_list,
                              const int backtrace_mode, const char *process_path,
                              size_t max_buf_MB)
{

   // TODO: remove as soon as dependency with maqao_get_os_event_encoding broken
   int arch; context->uarch = get_uarch (&arch);

   const char *hwcList = (default_hwc_list != NULL) ? default_hwc_list : lc_strdup (hwc_list);

   size_t nb_events = get_nb_events (hwcList);
   context->events_per_group = nb_events;
   uint64_t hwc_period [nb_events];
   int64_t  raw_code   [nb_events];

   if (hwcList != default_hwc_list) {
      // Custom
      set_context_evlist_custom (context, hwcList, hwc_period, raw_code, context->output_path);
   } else {
      // Default
      context->eventsList = (char *) hwcList;
      unsigned i;
      for (i=0; i<nb_events; i++) {
         hwc_period [i] = sampling_period;
         raw_code [i] = -1;
      }
   }

   // Checks for eventsList correctness + counts number of events
   perf_event_desc_t *fds = NULL; int _nb_fds = 0;
   if (perf_setup_list_events (context->eventsList, &fds, &_nb_fds, raw_code) == -1) {
      ERRMSG ("Cannot setup events\n");
      clean_abort (context->child_pid, context->output_path);
   }
   if (nb_events != (unsigned) _nb_fds) {
      ERRMSG ("Number of events differs from lprof front-end\n");
      clean_abort (context->child_pid, context->output_path);
   }

   // Sets most context fields
   const size_t page_size = sysconf(_SC_PAGESIZE);
   const unsigned nb_fds = (unsigned) _nb_fds;
   context->events_per_group = nb_fds;
   init_sampler_data (context, process_path, max_buf_MB, backtrace_mode);

#ifdef __LIBUNWIND__
   if (backtrace_mode == BACKTRACE_MODE_STACK)
      context->mmap_size = (64 + 1) * page_size;
   else
      context->mmap_size = (MMAP_PAGES + 1) * page_size;
#else
   context->mmap_size = (MMAP_PAGES + 1) * page_size;
#endif
   context->fds = fds;

   context->sampleTypesList = lc_calloc (nb_fds,
                                         sizeof context->sampleTypesList[0]);
   set_sample_type (0, backtrace_mode, nb_fds, context->sampleTypesList);

   // Sets fds: fds[i] is the perf-event structure (perf_event_desc_t) related to the (i+1)th HW event
   const size_t payload_size = context->mmap_size - page_size;
   // Sets fds: fd[*].hw
   unsigned i;
   for (i=0; i<nb_fds; i++) {
      struct perf_event_attr *hw = &fds[i].hw;

      hw->inherit = context->sampling_engine == SAMPLING_ENGINE_INHERIT ? 1 : 0;
      hw->sample_period = hwc_period [i];

      // TODO: remove as soon as legacy code removed
      if (i == 0 && context->sampling_engine == SAMPLING_ENGINE_INHERIT)
         hw->sample_type = context->sampleTypesList[0] & ~PERF_SAMPLE_CPU;
      else if (i == 0 && backtrace_mode == BACKTRACE_MODE_STACK) {
#ifdef __LIBUNWIND__
         hw->sample_type = context->sampleTypesList[0] | PERF_SAMPLE_REGS_USER;
         hw->sample_regs_user  = 1UL << PERF_REG_X86_BP | 1UL << PERF_REG_X86_SP;
         hw->sample_stack_user = PERF_STACK_USER_SIZE; // max size for data in PERF_SAMPLE_STACK_USER
#else
         hw->sample_type = context->sampleTypesList[0] & ~PERF_SAMPLE_STACK_USER;
#endif
      }
      else
         hw->sample_type = context->sampleTypesList[i];

      hw->watermark = 1;
      hw->wakeup_watermark = payload_size / 2;
      hw->read_format  = PERF_FORMAT_SCALE; // used by warn_multiplexing()
      hw->read_format |= PERF_FORMAT_ID;    // used by set_events_id_legacy()
   }

   // dryrun
   // TODO: simplify/remove can_open_separately and try_to_group
   if (can_open_separately (context) == FALSE) clean_abort (context->child_pid, context->output_path);
   context->can_group = lc_malloc ((nb_fds-1) * sizeof context->can_group[0]);
   try_to_group (context);
   dryrun_multiplexing (context);

   /* TODO: handle precise_ip via instru_session
    * For example, on Haswell, wrong samples count when enabling
    * precise IP for CPU_CLK_THREAD_UNHALTED:REF_XCLK+INST_RETIRED */
   for (i=0; i<nb_fds; i++) {
      struct perf_event_attr *hw = &fds[i].hw;
      hw->precise_ip = 0;
   }

   // Sets fds: fd[*].buf and .pgmsk (ease debug)
   fds[0].pgmsk = payload_size - 1; // exclude header page
   for (i=1; i<nb_fds; i++) {
      fds[i].buf = NULL;
      fds[i].pgmsk = 0;
   }
}

// Print lost events for each event rank
// Since IP is de facto not available, related contribution is not accounted
static void print_lost_events (smpl_context_t *context, const int backtrace_mode,
                               unsigned sampling_engine, unsigned sampling_period)
{
   unsigned i;

   uint64_t lost_events = 0;
   uint64_t coll_events = 0;

   // Sum contributions from sampler threads
   for (i=0; i < context->nb_sampler_threads; i++) {
      lost_events += context->sampler_data[i].lost_events;
      coll_events += context->sampler_data[i].coll_events;
   }

   if (lost_events == 0) return;

   const float loss_ratio = lost_events * 100 / (float) (coll_events + lost_events);
   if (loss_ratio >= 0.5f && loss_ratio <= 5.0f) {
      WRNMSG ("%.1f%% events lost (i.e related events counts probably underestimated by about %.1f%%)\n",
              loss_ratio, loss_ratio);
   }
   else if (loss_ratio > 5.0f) {
      ERRMSG ("%.1f%% events lost (i.e related events counts largely underestimated):\n",
              loss_ratio);
      const boolean_t high_rate = sampling_engine == SAMPLING_ENGINE_TIMERS ?
         sampling_period <= TIMER_MEDIUM_SAMPLING_PERIOD :
         sampling_period <= MEDIUM_SAMPLING_PERIOD;

      if (backtrace_mode == BACKTRACE_MODE_STACK) { // heavy backtrace
         INFOMSG ("Rerun without --backtrace-mode=stack or with another backtrace-mode%s.\n",
                  high_rate ? " and/or with lower sampling rate (e.g with --sampling-rate=low)" : "");
      } else if (backtrace_mode != BACKTRACE_MODE_OFF) { // light backtrace
         INFOMSG ("If you don't need callstacks and accept reduced categorization accuracy %s%s.\n",
                  "rerun with --backtrace-mode=off",
                  high_rate ? " and/or with lower sampling rate (e.g with --sampling-rate=low)" : "");
      } else if (high_rate) {
         INFOMSG ("Rerun with lower sampling rate (e.g with --sampling-rate=low)\n");
      }
   }
}

// Close files and free related buffers that are not used by dump_collect_data()
static void free_sampler_data_before_dump (const smpl_context_t *context, const int backtrace_mode)
{
   unsigned i;
   for (i=0; i<context->nb_sampler_threads; i++) {
      sampler_data_t *const sd = &(context->sampler_data[i]);

      if (sd->cur == sd->file) {
         dump_to_files (context, sd);
         fclose (sd->fp_smp);     sd->fp_smp     = NULL;
         fclose (sd->fp_smp_idx); sd->fp_smp_idx = NULL;
         fclose (sd->fp_cpu);     sd->fp_cpu     = NULL;
         fclose (sd->fp_cpu_idx); sd->fp_cpu_idx = NULL;
         sampler_data_buf_free (sd->file);
      }

#ifdef __LIBUNWIND__
      if (backtrace_mode == BACKTRACE_MODE_STACK) {
         FOREACH_INHASHTABLE (sd->unwind_data, ud_iter) {
            unwind_data_t *ud = GET_DATA_T (unwind_data_t *, ud_iter);
            unw_destroy_addr_space (ud->addr_space);
            FOREACH_INARRAY (ud->context.maps, map_iter) {
               map_t *map = ARRAY_GET_DATA (map_t *, map_iter);
               lc_free (map->name);
               if (map->data) munmap (map->data, map->length);
               if (map->fd >= 0) close (map->fd);
               lc_free (map->di);
            }
            array_free (ud->context.maps, lc_free);
            lc_free (ud);
         }
         hashtable_free (sd->unwind_data, NULL, NULL);
      }
#endif
   }
}

// Remove temporary files and free memory that were processed by dump_collect_data()
static void free_sampler_data_after_dump (const smpl_context_t *context)
{
   unsigned i;
   for (i=0; i<context->nb_sampler_threads; i++) {
      sampler_data_t *const sd = &(context->sampler_data[i]);

      sampler_data_buf_free (sd->mem);
      // remark: sd->file freed in free_sampler_data_before_dump()
      remove (sd->smp_file_name);     lc_free (sd->smp_file_name);
      remove (sd->smp_idx_file_name); lc_free (sd->smp_idx_file_name);
      remove (sd->cpu_file_name);     lc_free (sd->cpu_file_name);
      remove (sd->cpu_idx_file_name); lc_free (sd->cpu_idx_file_name);
   }
   lc_free (context->sampler_data);
}

static void destroy_context (smpl_context_t *context)
{
   lc_free (context->can_group);
   if (context->fds) perf_free_fds (context->fds, context->events_per_group);
   lc_free (context->sampleTypesList);
   lc_free (context->eventsList);
}

static int double_check_mkdir (const char *dir_name) {
   DIR *dir = opendir (dir_name);
   if (dir) {
      DBGMSG ("mkdir %s failed with (errno = %d) != EEXIST but this directory actually exists\n", dir_name, errno);
      closedir (dir);
      return 0;
   }
   else {
      perror ("opendir");
      return -1;
   }
}

// Used by listening_maps_file_new: file copy
static int _cp (char *src, char *dst, boolean_t verbose)
{
   // Opens source file (read-only)
   FILE *fp_src = fopen (src, "r");
   if (!fp_src) {
      if (verbose) perror ("listening_maps: cannot read-only source file");
      return -1;
   }

   // Opens destination file (write-only)
   FILE *fp_dst = fopen (dst, "w");
   if (!fp_dst) {
      if (verbose) perror ("listening_maps: cannot write destination file");
      fclose (fp_src);
      return -2;
   }

   char buf [1024 * 1024]; // 1 MB buffer
   size_t nb_read_bytes = 0;
   // Try to copy up to 1 MB each iteration to optimize efficiency
   while ((nb_read_bytes = fread (buf, 1, sizeof buf, fp_src)) > 0) {
      size_t nb_written_bytes = fwrite (buf, 1, nb_read_bytes, fp_dst);
      if (nb_written_bytes < nb_read_bytes) {
         if (verbose) fputs ("listening_maps: write error", stderr);
         fclose (fp_src);
         fclose (fp_dst);
         return -3;
      }
   }

   fclose (fp_src);
   fclose (fp_dst);
   return 0;
}

// Writes to a string PID of processes executing mpi_target
static int get_mpi_target_PIDs (const char *mpi_target, char *PIDs)
{
   // Calls pidof <MPI target>
   char *popen_buf = lc_malloc (strlen ("pidof ") + strlen (mpi_target));
   sprintf (popen_buf, "pidof %s", mpi_target);
   FILE *fp = popen (popen_buf, "r");
   lc_free (popen_buf);

   // Copy pidof output to PIDs
   if (fgets (PIDs, 1024, fp) != PIDs) {
      perror ("Cannot get PID of the MPI target executable");
      pclose (fp);
      return -1;
   }

   pclose (fp);
   return 0;
}

// listening_maps_file_new parameters
typedef struct {
   const char *process_path;
   int pid;
   const char *mpi_target;
   boolean_t verbose;
} lmf_params_t;

// Copy maps file for the given PID
static void copy_maps_file (const lmf_params_t *params, int pid, int nbCopy)
{
   char src [strlen ("/proc/999999/maps") + 1]; sprintf (src, "/proc/%d/maps", pid);
   char dst [strlen (params->process_path) + strlen ("/maps_bin_999999_99") + 1];
   sprintf (dst, "%s/maps_bin_%d_%d", params->process_path, pid, nbCopy);
   if (_cp (src, dst, params->verbose) != 0 && params->verbose) {
      WRNMSG ("listening_maps: failed to copy %s in %s\n", src, dst);
   }
}

// Executed by the thread that listens (copy 5 times, 1 sec interval) maps file for the application process
static void *listening_maps_file_new (void *pparams)
{
   DBGMSG ("Thread %ld will listen maps files\n", syscall(SYS_gettid));

   lmf_params_t *params = pparams;
   int nbCopy;

   // 5 copies from /proc/<PID>/maps to lprof experiment directory
   for (nbCopy = 1; nbCopy <= 5; nbCopy++) {
      sleep (1); // wait 1 second before next save

      if (params->mpi_target == NULL)
         copy_maps_file (params, params->pid, nbCopy);

      else { // MPI target defined: effective binary is not MAQAO default target !
         char mpi_target_PIDs [1024];
         if (get_mpi_target_PIDs (params->mpi_target, mpi_target_PIDs) != 0)
            // Failed to get PIDs related to MPI target, fallback to mpirun/exec PID
            copy_maps_file (params, params->pid, nbCopy);

         else {
            // Succeeded in getting application (MPI targets) PIDs
            char *tok = strtok (mpi_target_PIDs, " ");
            // Save maps file for each detected application process
            while (tok != NULL) {
               copy_maps_file (params, atoi (tok), nbCopy);
               tok = strtok (NULL, " ");
            }
         }
      }
   }

   return NULL;
}

// threadUG_routine parameters
typedef struct {
   smpl_context_t *context;
   int user_guided;
   sigset_t sigset;
} ug_params_t;

// Enables perf-events for all CPUs (inherit) or threads (ptrace-based)
static inline void enable_all_groups (smpl_context_t *const context)
{
   printf ("\r[MAQAO] STARTING COUNTERS\n"); fflush (stdout);
   context->start_enabled = TRUE;
   if (context->sampling_engine == SAMPLING_ENGINE_INHERIT) enable_all_cpus (context->UG_data);
   else enable_all_threads (context->UG_data);
}

// Disables perf-events for all CPUs (inherit) or threads (ptrace-based)
static inline void disable_all_groups (smpl_context_t *const context)
{
   printf ("\r[MAQAO] SHUTTING DOWN COUNTERS\n"); fflush (stdout);
   context->start_enabled = FALSE;
   if (context->sampling_engine == SAMPLING_ENGINE_INHERIT) disable_all_cpus (context->UG_data);
   else disable_all_threads (context->UG_data);
}

/* Run by the helper thread enabling/disabling events in "user-guided" mode
 *  - ug=42 (delay mode) => enables after 42 seconds
 *  - ug=on (interactive mode) => toggles at each SIGTSTP (CTRL+Z) */
static void *threadUG_routine (void *pparams)
{
   ug_params_t *const params = pparams;
   smpl_context_t *const context = params->context;

   if (params->user_guided > 0) {
      // delay mode: start after X secondes
      sleep (params->user_guided);
      enable_all_groups (context);
   } else {
      // interactive mode: listen for SIGTSTP signals (CTRL+Z)
      boolean_t smpl_enabled = FALSE;
      int sig;
      for (;;) {
         // Waits for next signal (from the main/parent lprof process)
         if (sigwait (&(params->sigset), &sig)) {
            DBG(perror ("sigwait");)
               }
         // Toggles state (enabled => disabled, disabled => enabled)
         if (sig == SIGTSTP) {
            if (smpl_enabled == TRUE) {
               disable_all_groups (context);
               smpl_enabled = FALSE;
            } else {
               enable_all_groups (context);
               smpl_enabled = TRUE;
            }
         }
      }
   }

   return NULL; // never reached in interactive mode, or in delay mode if before deadline
}

/*
 * Runs the target application via its command line
 * Effective start will wait for ready signal (pipe closing event)
 * or tracer to be ready
 */
static void run_application (int *wait_pipe, const char *cmd,
                             const char *output_path, const char *hostname)
{
   // Prepare arguments: extract binary and arguments from command line

   // Allocates arguments table
   char *buf = lc_strdup (cmd); strtok (buf, " ");
   int nb_tokens = 1;
   while (strtok (NULL, " ") != NULL) nb_tokens++;
   char *args [nb_tokens+1];
   lc_free (buf);

   // First token is binary and next tokens are arguments
   buf = lc_strdup (cmd); args[0] = strtok (buf, " ");
   nb_tokens = 1; while ((args [nb_tokens] = strtok (NULL, " ")) != NULL) nb_tokens++;
   // Cannot free buf: referenced by args[*]

   if (wait_pipe) {
      // Waits until sampler started
      char wait_buf;
      if (read (wait_pipe[0], &wait_buf, 1) == -1) {
         DBG(perror ("Cannot read wait_pipe to wait for parent");)
            };
      close (wait_pipe[0]);
   } else {
      setpgid (0, getpid()); // allows tracer to wait events only from application processes/threads
      // allows tracer to take control
      if (raise (SIGSTOP) != 0) {
         perror ("Cannot allow tracer take control of application (raise (SIGSTOP))");
         clean_abort (getppid(), output_path);
      };
   }

   printf ("[MAQAO] PROCESS LAUNCHED (host %s, process %d)\n", hostname, getpid());
   fflush (stdout);

   /* TODO: adapt main.lua to support pathless (no "/") options["bin"] to enable
    * execvp to run it from anywhere as soon as added to PATH */
   if (execvp (args[0], args) == -1) {
      perror ("Cannot run application (execvp)");
      clean_abort (getppid(), output_path);
   } else {
      // Cannot access here: process image is replaced during execv
   }
}

/* SIGINT (CTRL+C) handler for the inherit-based implementation
 * It is up to me (lprof process) to kill application before exiting */
static void SIGINT_handler_inherit (int status)
{
   (void) status;
   fprintf (stderr, "   /!\\ INTERRUPTING MAQAO ANALYSIS /!\\ \n");
   clean_abort (application_pid, exp_path);
}

/* SIGINT (CTRL+C) handler for the ptrace-based implementation
 * Me (tracer) has to kill tracee processes before exiting */
static void SIGINT_handler_ptrace (int status)
{
   (void) status;
   fprintf (stderr, "   /!\\ INTERRUPTING MAQAO ANALYSIS /!\\ \n");
   touch_done_file (exp_path);
   kill (-application_pid, SIGTERM); // TODO: tracee should receive SIGINT => try to remove this line
}

/*
 * Samples an application with perf_events
 * \param cmd command to run
 * \param output_path path to the lprof output/experiment directory
 * \param sampling_period string to define sampling period
 * \param hwc_list List of hardware events name+period to sample. Ex: "CPU_CLK_THREAD_UNHALTED:REF_XCLK@500003,MEM_LOAD_UOPS_RETIRED:L1_HIT@500003
 * \param user_guided -1 = user control disabled, 0 = CTRL+Z pauses/resumes sampling and n > 0 = delays sampling from n seconds
 * \param backtrace_mode Backtraces/callchains mode: OFF, CALL, STACK and BRANCH
 * \param cpu_list "0,1,2,3" to limit profiling to CPU0-3
 * \param mpi_target path to application executable if masked to MAQAO by MPI command (inherit-mode only)
 * \param nb_sampler_threads number of threads to process samples in ring buffers
 * \param sampling_engine SAMPLING_ENGINE_INHERIT, SAMPLING_ENGINE_PTRACE or SAMPLING_ENGINE_TIMERS
 * \param sync boolean true if syncronous tracer requested, asyncronous otherwise (ptrace-mode only)
 * \param finalize_signal Signal used by some parallel launchers to notify ranks finalization
 * \param verbose boolean true if less critical warnings has to be displayed. Forwarded from args.verbose
 * \param max_buf_MB maximum memory buffer size in MB
 * \param files_buf_MB temporary files buffer size in MB
 * \param max_files_MB maximum total temporary files size in MB
 */
returnInfo_t sample (const char *cmd, const char* output_path,
                     unsigned sampling_period, const char* hwc_list,
                     const int user_guided, const int backtrace_mode,
                     const char *cpu_list, const char *mpi_target,
                     unsigned nb_sampler_threads, unsigned sampling_engine,
                     boolean_t sync, int finalize_signal, boolean_t verbose,
                     size_t max_buf_MB, size_t files_buf_MB, size_t max_files_MB)
{
   returnInfo_t retInfo;
   retInfo.pid = 0;
   if (gethostname (retInfo.hostname, sizeof (retInfo.hostname)) != 0) return retInfo;

   if (sampling_engine != SAMPLING_ENGINE_TIMERS && check_perf_event_paranoid() != 0)
      return retInfo;

   // Check for default events list availability (host processor may not be supported)
   char *default_hwc_list = NULL;
   if (sampling_engine != SAMPLING_ENGINE_TIMERS &&
       (hwc_list == NULL || strlen (hwc_list) == 0)) {
      int arch; int uarch = get_uarch (&arch);
      default_hwc_list = getHwcList (arch, uarch, LPROF_VERBOSITY_OFF, NULL);
      if (default_hwc_list == NULL) return retInfo;
   }

   int wait_pipe[2]; // inherit only
   if (sampling_engine == SAMPLING_ENGINE_INHERIT) {
      // Creates a pipe to synchronize application and sampler
      // Application waits for perf_events to be enabled
      if (pipe (wait_pipe) == -1) {
         DBG(perror ("Cannot create pipe to synchronize application start");)
            return retInfo;
      }
   }

   // Creates a child process that will become the target application
   pid_t child_pid = fork();
   if (child_pid == -1) {
      perror ("Cannot fork application");
      return retInfo;
   }

   // In child: run the application
   if (child_pid == 0) {
      switch (sampling_engine) {
      case SAMPLING_ENGINE_INHERIT:
         close (wait_pipe[1]); // Listening (no write)
         break;
      case SAMPLING_ENGINE_PTRACE:
         // Prepare application process for ptrace monitoring
         if (ptrace (PTRACE_TRACEME, 0, NULL, NULL) != 0) {
            perror ("ptrace (PTRACE_TRACEME) cannot attach to application process");
            clean_abort (getppid(), output_path);
         }
         break;
      case SAMPLING_ENGINE_TIMERS:
         // TODO: see man prctl + try to simplify/improve this
         // TODO: related: behavior of while (waitpid) loop in timers_sampler()
         prctl (PR_SET_DUMPABLE, (long)1); // can work without SET_PTRACER ?
#ifdef PR_SET_PTRACER
         prctl (PR_SET_PTRACER, (long)getppid()); // Since Linux 3.4
#endif
         break;
      }
      if (user_guided >= 0) signal (SIGTSTP, SIG_IGN);
      run_application (sampling_engine == SAMPLING_ENGINE_INHERIT ? wait_pipe : NULL, cmd, output_path, retInfo.hostname);
      // Cannot reach this line: execv in run_application
   }

   /* ****************** STARTING FROM HERE: in father ********************* */

   retInfo.pid = child_pid;

   if (sampling_engine == SAMPLING_ENGINE_INHERIT) close (wait_pipe[0]); // Notifying (no need to read)

   application_pid = child_pid;
   exp_path = output_path;

   /* The father/lprof process needs to redefine CTRL+C (SIGINT)
    * Otherwise (default SIGINT handler) it will be aborted and application will continue */
   signal (SIGINT, sampling_engine == SAMPLING_ENGINE_INHERIT ? SIGINT_handler_inherit : SIGINT_handler_ptrace);

   if (sampling_engine == SAMPLING_ENGINE_PTRACE) {
      // Waits for application process to be ready for ptracing
      int status;
      if (waitpid (child_pid, &status, 0) < 0) {
         perror ("Application is not ready (waitpid)");
         clean_abort (child_pid, output_path);
      }
      if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP) {
         ERRMSG ("unexpected wait status: %x", status);
         clean_abort (child_pid, output_path);
      }

      // Starts ptrace monitoring, but application still not started
      uint64_t ptrace_opts = PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK;
      if (ptrace (PTRACE_SETOPTIONS, child_pid, NULL, ptrace_opts) != 0) {
         perror ("ptrace (PTRACE_SETOPTIONS) cannot attach to application process");
         clean_abort (child_pid, output_path);
      }
   }

   // Gets CPUs to use for affinity setting. If not specified, default to all online CPUs
   long nprocs; unsigned *cpu_array;
   long online_cpus = sysconf(_SC_NPROCESSORS_ONLN); // Online CPUs
   if (online_cpus == -1) {
      perror ("Cannot get number of online CPUs via sysconf(_SC_NPROCESSORS_ONLN)");
      clean_abort (child_pid, output_path);
   }
   if (cpu_list == NULL) {
      nprocs = online_cpus;
      // No explicit list: use all online CPUs
      cpu_array = lc_malloc (online_cpus * sizeof cpu_array[0]);
      unsigned cpu;
      for (cpu = 0; cpu < online_cpus; cpu++) cpu_array [cpu] = cpu;
   } else {
      // Explicit list: use it
      DBGMSG ("Using CPUs %s for pid %d\n", cpu_list, child_pid);
      parse_cpu_list (cpu_list, &cpu_array, &nprocs);
   }

   // If no explicit sampler threads number, use as many threads as in the CPU list
   if (nb_sampler_threads == 0) nb_sampler_threads = nprocs;

   // Creates a sampling context and output directory
   // <output path>/<hostname> directory
   char node_path [strlen (output_path) + strlen (retInfo.hostname) + 2];
   sprintf (node_path, "%s/%s", output_path, retInfo.hostname); // concat <output path> + <hostname>
   if (mkdir (node_path, 0755) == -1) {
      if (errno != EEXIST) { // mpirun lprof => possible multiple instances on the same host
         ERRMSG ("Cannot create, in output directory, a directory to save host-related data\n");
         perror ("mkdir");
         if (double_check_mkdir (node_path) != 0)
            clean_abort (child_pid, output_path);
      }
   }

   // <output path>/<hostname>/<pid> directory
   char process_path [strlen (node_path) + strlen ("/999999") + 1];
   sprintf (process_path, "%s/%d", node_path, child_pid);
   if (mkdir (process_path, 0755) == -1) {
      ERRMSG ("Cannot create, in output directory, a directory to save process-related data\n");
      perror ("mkdir");
      if (double_check_mkdir (process_path) != 0)
         clean_abort (child_pid, output_path);
   }

   smpl_context_t context = { .child_pid = child_pid,
                              .verbose = verbose,
                              .output_path = output_path,
                              .sampling_engine = sampling_engine,
                              .start_enabled = (user_guided == -1),
                              .nb_sampler_threads = nb_sampler_threads,
                              .online_cpus = online_cpus,
                              .emergency_stop = FALSE,
                              .max_files_size = max_files_MB * 1024 * 1024,
                              .files_buf_size = files_buf_MB * 1024 * 1024 };

   if (sampling_engine != SAMPLING_ENGINE_TIMERS)
      init_context_hwc (&context, sampling_period, hwc_list, default_hwc_list,
                        backtrace_mode, process_path, max_buf_MB);
   else {
      int arch; context.uarch = get_uarch (&arch);
      context.events_per_group = 1;
      context.eventsList = lc_strdup (hwc_list);
      init_sampler_data (&context, process_path, max_buf_MB, backtrace_mode);
      context.sampleTypesList = lc_calloc (1, sizeof context.sampleTypesList[0]);
      set_sample_type (0, backtrace_mode, 1, context.sampleTypesList);
   }

   // Block SIGTSTP (terminal STOP: CTRL+Z) in user guided mode to enable sigwait
   sigset_t sigset;
   if (user_guided >= 0) {
      sigemptyset (&sigset);
      sigaddset (&sigset, SIGTSTP);
      if (pthread_sigmask (SIG_BLOCK, &sigset, NULL)) {
         DBG(perror ("pthread_sigmask");)
            }
   }

   // Forks a thread to listen maps files
   pthread_t threadMaps;
   lmf_params_t lmf_params = { process_path, child_pid, mpi_target, verbose };
   if (pthread_create (&threadMaps, NULL, listening_maps_file_new, &lmf_params) != 0) {
      ERRMSG ("Failed to create Thread Maps %lu\n", threadMaps);
      clean_abort (child_pid, output_path);
   }

   // Forks a thread to drive sampling (user guiding)
   pthread_t threadUG;
   ug_params_t ug_params = { &context, user_guided, sigset };
   if (user_guided >= 0) {
      if (pthread_create (&threadUG, NULL, threadUG_routine, &ug_params) != 0) {
         ERRMSG ("Failed to create Thread UG %lu\n", threadUG);
         clean_abort (child_pid, output_path);
      }
   }

   // Limits affinity to (logical) CPUs only for an explicit CPU list
   if (cpu_list) {
      unsigned i;
      cpu_set_t cpu_mask;
      CPU_ZERO (&cpu_mask);
      for (i=0; i<nprocs; i++) CPU_SET(cpu_array[i], &cpu_mask);
      if (sched_setaffinity (child_pid, sizeof(cpu_set_t), &cpu_mask) == -1) {
         perror ("Cannot set affinity");
         clean_abort (child_pid, output_path);
      }
   }

   // Starts application walltime measurement
   // WARNING: Since most of time is spent waiting (poll), do not use CPU-time timers like clock() etc.
   uint64_t start; rdtscll (start); struct timeval tv1; gettimeofday (&tv1, NULL);

   // STARTING FROM HERE, HIGH EFFORTS NEEDED ON DESIGN AND IMPLEMENTATION
   switch (sampling_engine) {
   case SAMPLING_ENGINE_INHERIT:
      inherit_sampler (&context, nprocs, wait_pipe, cpu_array); break;
   case SAMPLING_ENGINE_PTRACE:
      tracer_new (&context, nprocs, sync, finalize_signal); break;
   case SAMPLING_ENGINE_TIMERS:
      timers_sampler (&context, sampling_period, finalize_signal); break;
   }
   // END OF HIGH EFFORTS ZONE

   // Stops application walltime measurement
   uint64_t stop; rdtscll (stop); struct timeval tv2; gettimeofday (&tv2, NULL);
   if (verbose == TRUE) {
      printf ("[MAQAO] PROCESS FINISHED (host %s, process %d)\n", retInfo.hostname, child_pid);
      fflush (stdout);
   }

   print_lost_events (&context, backtrace_mode, sampling_engine, sampling_period);

   // Properly ends current work...
   pthread_cancel (threadMaps); // cancel: no wait
   pthread_join (threadMaps, NULL); // cleanup (removes memory leak)
   if (user_guided >= 0) {
      pthread_cancel (threadUG); // cancel: no wait
      pthread_join (threadUG, NULL); // cleanup
   }
   lc_free (cpu_array);

   // TODO: consider moving this higher in code tree (main.lua, display.lua...)
   struct timeval elapsed_tv; timersub (&tv2, &tv1, &elapsed_tv);
   const double elapsed_seconds = elapsed_tv.tv_sec + elapsed_tv.tv_usec / (double) 1000000;
   free_sampler_data_before_dump (&context, backtrace_mode);
   if (elapsed_seconds >= 1.1f) { // listening maps waits 1 second before copying maps...
      if (elapsed_seconds < 3.0f)
         WRNMSG ("Run not long enough to obtain significant results. Rerun with a longer workload for more accurate results or increase sampling frequency (g=small).\n");

      // Dumps to samples.lprof and cpu_ids.info data accumulated in memory and temporary files
      printf ("[MAQAO] PROCESSING SAMPLES (host %s, process %d)\n", retInfo.hostname, child_pid);
      fflush (stdout);
      dump_collect_data (&context, process_path, stop - start);
      printf ("[MAQAO] FINISHED PROCESSING SAMPLES (host %s, process %d)\n", retInfo.hostname, child_pid);
      fflush (stdout);
   }
   free_sampler_data_after_dump (&context);
   destroy_context (&context);

   return retInfo;
}
