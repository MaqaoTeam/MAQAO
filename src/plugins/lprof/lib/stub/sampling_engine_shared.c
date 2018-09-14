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

/* Defines functions shared by both inherit- and ptrace-based sampling engines */

#include <string.h>    // memset, memcpy
#include <assert.h>    // assert
#include <errno.h>     // errno
#include <sys/mman.h>  // mmap/unmap
#include <sys/types.h> // kill
#include <signal.h>    // kill
#ifndef NDEBUG
#include "utils.h" // rdtscll
#endif
#ifdef __LIBUNWIND__
#include <libunwind.h> // unw_init_remote...
#include "unwind.h" // unwind_context_t
#endif
#include "IP_events_format.h" // write_IP_events
#include "sampling_engine_shared.h" // smpl_context_t, enable/disable_events_group, process_overflow...
#include "sampling_engine_data_struct.h" // buf_t, lprof_queue_t...

#define CC_MAX_LEN   100 // Maximum callchain length for lprof
#define TID2X_SIZE  4096
#define IP2SMP_SIZE 12251 // Big prime number far away from 2 consecutive powers of 2 (8K and 16K)

sampler_data_buf_t *sampler_data_buf_new (size_t buf_size)
{
   sampler_data_buf_t *new = lc_malloc (sizeof *new);

   new->buf = buf_new (buf_size);
   new->tid2ipt = lprof_hashtable_new (new->buf, TID2X_SIZE);
   new->tid2cpu = lprof_hashtable_new (new->buf, TID2X_SIZE);

   return new;
}

void sampler_data_buf_reset (sampler_data_buf_t *sampler_data_buf)
{
   buf_t *buf = sampler_data_buf->buf;

   buf_flush (buf);
   sampler_data_buf->tid2ipt = lprof_hashtable_new (buf, TID2X_SIZE);
   sampler_data_buf->tid2cpu = lprof_hashtable_new (buf, TID2X_SIZE);
}

void sampler_data_buf_free (sampler_data_buf_t *sampler_data_buf)
{
   buf_free (sampler_data_buf->buf);
   lc_free (sampler_data_buf);
}

/* Creates a "done" file in experiment path
 * Useful to notify "meta" applications waiting for lprof completion */
void touch_done_file (const char *output_path)
{
   char buf [strlen ("touch /done") + strlen (output_path) + 1];
   sprintf (buf, "touch %s/done", output_path);
   if (system (buf) == -1) {
      DBGMSG ("Failed to create %s/done\n", output_path);
   }
}

/* Cleanly aborts lprof execution:
 * - kills a process by its PID (in general: target application)
 * - creates a done file in experiment directory
 * - exits (self) */
void clean_abort (pid_t pid, const char *output_path)
{
   if (pid > 0) kill (pid, SIGTERM);
   touch_done_file (output_path);
   exit (-1);
}

// Set ID for all events in an perf-events group: legacy method (old kernels)
static inline int set_events_id_legacy (perf_event_desc_t *fds, int nb_fds)
{
   // PERF_FORMAT_GROUP not specified
   struct { uint64_t value, time_enabled, time_running, id; } data;

   int i;
   for (i=0; i<nb_fds; i++) {
      if (read (fds[i].fd, &data, sizeof data) == -1) {
         DBG(fprintf (stderr, "Cannot read ID for event %s\n", fds[i].name); \
             perror ("read");)
            return -1;
      } else {
         DBGMSG ("Event %s has ID %"PRIu64"\n", fds[i].name, data.id);
         fds[i].id = data.id;
      }
   }

   return 0;
}

#ifdef PERF_EVENT_IOC_ID
// Idem set_events_id_legacy for newer kernels
static inline int set_events_id_modern (perf_event_desc_t *fds, int nb_fds)
{
   int i;
   for (i=0; i<nb_fds; i++) {
      if (ioctl (fds[i].fd, PERF_EVENT_IOC_ID, &fds[i].id) == -1) {
         DBG(fprintf (stderr, "Cannot read ID for event %s\n", fds[i].name); \
             perror ("ioctl (PERF_EVENT_IOC_ID)");)
            return -1;
      } else {
         DBGMSG ("Event %s has ID %"PRIu64"\n", fds[i].name, fds[i].id);
      }
   }

   return 0;
}
#endif

/* Creates a perf-events group (with mmap region to access common ring buffer) for a given CPU or thread.
 * Samples are produced to ring buffer (i.e. sampling is effectively started) only when related
 * perf-events are enabled. Except for the "user-guided" mode, events are created enabled.
 * \param context sampling engine context
 * \param cpu CPU rank
 * \param tid thread ID */
perf_event_desc_t *_start_sampling (smpl_context_t *context, unsigned cpu, pid_t tid)
{
   if (context->sampling_engine == SAMPLING_ENGINE_INHERIT) {
      DBGMSG ("Starting sampling CPU%u\n", cpu);
   } else {
      DBGMSG ("Starting sampling TID %d\n", tid);
   }

   // Get events
   const size_t fds_size = context->events_per_group * sizeof (perf_event_desc_t);
   perf_event_desc_t *fds = lc_malloc (fds_size);
   memcpy (fds, context->fds, fds_size);

   unsigned i; const unsigned nb_fds = context->events_per_group;
   perf_event_desc_t *group_leader = &fds[0];
   group_leader->fd = -1;
   const uint64_t disabled = context->start_enabled ? 0 : 1;

   // perf_event_open on events
   for (i=0; i<nb_fds; i++) {
      struct perf_event_attr *hw = &fds[i].hw;
      DBG(if ((context->sampling_engine == SAMPLING_ENGINE_INHERIT && cpu == 0) || \
              (context->sampling_engine != SAMPLING_ENGINE_INHERIT && tid == context->child_pid)) \
             utils_print_struct_event_attr (hw));
      int group_leader_fd = -1;
      if (i > 0 && context->can_group [i-1] == TRUE) group_leader_fd = fds[0].fd;
      hw->disabled = disabled;

      int fd = perf_event_open (hw, context->sampling_engine == SAMPLING_ENGINE_INHERIT ? context->child_pid : tid,
                                context->sampling_engine == SAMPLING_ENGINE_INHERIT ? (int) cpu : -1,
                                group_leader_fd, 0 /* no flags */);
      if (fd == -1) {
         if (errno == ESRCH) { lc_free (fds); return NULL; }
         ERRMSG ("Cannot perf_event_open %s\n", fds[i].name);
         perror ("");
         clean_abort (context->child_pid, context->output_path);
      }

      fds[i].fd = fd;
   }

   // mmap group leader ring buffer
   void *buf = mmap (NULL, context->mmap_size, PROT_READ|PROT_WRITE,
                     MAP_SHARED, group_leader->fd, 0);
   if (buf == MAP_FAILED) {
      perror ("Cannot mmap perf_event_open buffer");
      clean_abort (context->child_pid, context->output_path);
   }
   group_leader->buf = buf;

   // Redirect samples from group followers to the group leader
   for (i=1; i<nb_fds; i++) {
      if (ioctl (fds[i].fd, PERF_EVENT_IOC_SET_OUTPUT, group_leader->fd) == -1) {
         perror ("Cannot redirect samples to the group leader");
         clean_abort (context->child_pid, context->output_path);
      }
   }

   // Save events ID
   /* TODO: as soon as Linux 3.12 will be really obsolete and
    * more recent perf_event.h included, remove legacy version */
#ifdef PERF_EVENT_IOC_ID
   if (ioctl (group_leader->fd, PERF_EVENT_IOC_ID, &(group_leader->id)) == -1) {
      perror ("PERF_EVENT_IOC_ID not supported (kernel < 3.12 ?)\n");
      if (set_events_id_legacy (fds, nb_fds) == -1) clean_abort (context->child_pid, context->output_path);
   } else {
      if (set_events_id_modern (fds, nb_fds) == -1) clean_abort (context->child_pid, context->output_path);
   }
#else
   if (set_events_id_legacy (fds, nb_fds) == -1) clean_abort (context->child_pid, context->output_path);
#endif

   // Set CPU (used as TID by ptrace engine)
   for (i=0; i<nb_fds; i++) fds[i].cpu = context->sampling_engine == SAMPLING_ENGINE_INHERIT ? (int) cpu : tid;

   return fds;
}

/* Returns TRUE if samples needs to be written to files instead of memory
 * In that case, swap_to_files() needs to be called */
static boolean_t needs_swap_to_files (const sampler_data_t *sampler_data, size_t size)
{
   // Check current output buffer for samples
   sampler_data_buf_t *mem = sampler_data->mem;
   if (sampler_data->cur != mem) return FALSE;

   // Check for sufficient available space in memory buffer
   buf_t *buf = mem->buf;
   return (size > buf_avail (buf)) ? TRUE : FALSE;
}

/* Redirect new samples data to files buffer (instead of memory buffer)
 * Triggered by needs_swap_to_files(). */
static void swap_to_files (const smpl_context_t *context, sampler_data_t *sampler_data)
{
   if (context->verbose == TRUE) {
      char hostname [256];
      gethostname (hostname, sizeof hostname);
      WRNMSG ("Switching to tmp files for samples output (host %s, process %d, worker %lu/%u)",
              hostname, context->child_pid,
              (sampler_data - context->sampler_data) + 1,
              context->nb_sampler_threads);
   }

   // Create files buffer and swap current buffer pointer (from memory to files buffer)
   sampler_data->file = sampler_data_buf_new (context->files_buf_size);
   sampler_data->cur = sampler_data->file;

   // Open temporary files related to the files buffer
   sampler_data->fp_smp     = fopen (sampler_data->smp_file_name    , "wb");
   sampler_data->fp_smp_idx = fopen (sampler_data->smp_idx_file_name, "wb");
   sampler_data->fp_cpu     = fopen (sampler_data->cpu_file_name    , "wb");
   sampler_data->fp_cpu_idx = fopen (sampler_data->cpu_idx_file_name, "wb");
}

/* Returns TRUE if files buffer is full and needs to be flushed to related files
 * In that case, dump_to_files_and_reset() needs to be called */
static boolean_t needs_dump_to_files (sampler_data_t *sampler_data, size_t size)
{
   // Check current output buffer for samples
   sampler_data_buf_t *file = sampler_data->file;
   if (sampler_data->cur != file) return FALSE;

   // Check for sufficient available space in files buffer
   buf_t *buf = file->buf;
   return (size > buf_avail (buf)) ? TRUE : FALSE;
}

/* Flushes content of files buffer to related samples files (sampler_data->fp_smp*)
 * For each sampler_data->fp_smp record, write related offsets to sampler_data->fp_smp_idx */
static void dump_to_smp_file (const smpl_context_t *context, sampler_data_t *sampler_data)
{
   FILE *fp_smp = sampler_data->fp_smp;
   FILE *fp_smp_idx = sampler_data->fp_smp_idx;

   // For each thread/TID
   FOREACH_IN_LPROF_HASHTABLE (sampler_data->file->tid2ipt, tid2ipt_iter) {
      const uint64_t tid = GET_KEY (uint64_t, tid2ipt_iter);
      lprof_hashtable_t *const ip2smp = GET_DATA (tid2ipt_iter);

      // For each address/IP
      FOREACH_IN_LPROF_HASHTABLE (ip2smp, ip2smp_iter) {
         const uint64_t ip = GET_KEY (uint64_t, ip2smp_iter);
         IP_events_t *const IP_events = GET_DATA (ip2smp_iter);

         // Write indexing data to index file: TID, IP and file offset
         boolean_t write_error = FALSE;
         unsigned pos = ftell (fp_smp);
         if (fwrite (&tid, sizeof tid, 1, fp_smp_idx) != 1) write_error = TRUE;
         if (fwrite (&ip , sizeof ip , 1, fp_smp_idx) != 1) write_error = TRUE;
         if (fwrite (&pos, sizeof pos, 1, fp_smp_idx) != 1) write_error = TRUE;
         if (write_error == TRUE) {
            ERRMSG ("Write error in %s\n", sampler_data->smp_idx_file_name);
            return;
         }

         if (write_IP_events (fp_smp, ip, IP_events, context->events_per_group) != 0) {
            ERRMSG ("Cannot write IP events\n");
            return;
         }
      }
   }
}

/* Flushes content of files buffer to related CPUs files (sampler_data->fp_cpu*)
 * For each sampler_data->fp_cpu record, write related offsets to sampler_data->fp_cpu_idx */
static void dump_to_cpu_file (const smpl_context_t *context, sampler_data_t *sampler_data)
{
   // For each thread/TID
   FOREACH_IN_LPROF_HASHTABLE (sampler_data->file->tid2cpu, tid2cpu_iter) {
      const uint64_t tid = GET_KEY (uint64_t, tid2cpu_iter);
      hits_nb_t *cpus = GET_DATA_T (hits_nb_t *, tid2cpu_iter);

      // Write indexing data to index file: TID and file offset
      boolean_t write_error = FALSE;
      unsigned pos = ftell (sampler_data->fp_cpu);
      if (fwrite (&tid, sizeof tid, 1, sampler_data->fp_cpu_idx) != 1) write_error = TRUE;
      if (fwrite (&pos, sizeof pos, 1, sampler_data->fp_cpu_idx) != 1) write_error = TRUE;
      if (write_error == TRUE) {
         ERRMSG ("Write error in %s\n", sampler_data->cpu_idx_file_name);
         return;
      }

      // Write CPUs histogram (cpus[4] = 8 => 8 samples hitting CPU4 for thread <tid>)
      if (fwrite (cpus, sizeof cpus[0], context->online_cpus, sampler_data->fp_cpu) != context->online_cpus) {
         ERRMSG ("Cannot write CPU-info in %s", sampler_data->cpu_file_name);
         return;
      }
   }
}

// Flushes content of files buffer to related files
void dump_to_files (const smpl_context_t *context, sampler_data_t *sampler_data)
{
   DBG ( char hostname [256];                     \
         gethostname (hostname, sizeof hostname);                       \
         WRNMSG ("Flushing files buffer to related temporary files (host %s, process %d, worker %lu/%u)", \
                 hostname, context->child_pid,                          \
                 (sampler_data - context->sampler_data) + 1,            \
                 context->nb_sampler_threads); )

      dump_to_smp_file (context, sampler_data);
   dump_to_cpu_file (context, sampler_data);
}

// Flushes content of files buffer to related files and resets it for next "window"
static void dump_to_files_and_reset (smpl_context_t *context, sampler_data_t *sampler_data)
{
   dump_to_files (context, sampler_data);
   sampler_data_buf_reset (sampler_data->file);

   // Get total files size
   unsigned i; size_t tot_files_size = 0;
   for (i=0; i<context->nb_sampler_threads; i++) {
      sampler_data_t *const sd = &(context->sampler_data[i]);
      tot_files_size += ftell (sd->fp_smp) + ftell (sd->fp_smp_idx);
      tot_files_size += ftell (sd->fp_cpu) + ftell (sd->fp_cpu_idx);
   }

   // If 2nd size limit (total files size > limit) exceeded...
   // send a signal (set a global/context boolean) to sampler threads
   if (context->emergency_stop == FALSE && tot_files_size > context->max_files_size) {
      ERRMSG ("[MAQAO] Reached size limit for samples dump file, no more samples will be saved.\n");
      ERRMSG ("[MAQAO] Rerun with g=large, btm=off and/or --maximum-tmpfiles-megabytes=X with X much greater than %lu\n.", tot_files_size / (1024 * 1024));
      context->emergency_stop = TRUE; // Disables samples collection and interrupts sampler threads
   }
}

// PERFORMANCE AND RESULTS CRITICAL: HIGH EFFORT NEEDED FOR DESIGN/IMPLEMENTATION
static int read_sample_callchain (perf_event_desc_t *group_leader, size_t *sz,
                                  sampleInfo_t *sampleInfo)
{
   uint64_t nr; // number of records
   if (perf_read_buffer_64 (group_leader, &nr) == -1) {
      DBGMSG0 ("Cannot read callchain length\n");
      return -1;
   }
   *sz -= sizeof (nr);

   if (nr < 3) {
      DBGMSGLVL (1, "Too small callchain (nr=%"PRIu64")\n", nr);
      const size_t size_to_skip = nr * sizeof (uint64_t);
      perf_skip_buffer (group_leader, size_to_skip); *sz -= size_to_skip;
      return 0;
   }

   // 1st entry is on kernel (probably related to perf-events) and 2nd is target IP => skip them
   const size_t size_to_skip = 2 * sizeof (uint64_t);
   perf_skip_buffer (group_leader, size_to_skip); *sz -= size_to_skip;
   nr -= 2;

   sampleInfo->nbAddresses = (nr <= (CC_MAX_LEN) ? nr : CC_MAX_LEN);

   const size_t cc_size = sampleInfo->nbAddresses * sizeof sampleInfo->callChainAddress[0];
   if (perf_read_buffer (group_leader, sampleInfo->callChainAddress, cc_size) == -1) {
      DBGMSG0 ("Cannot read callchain\n");
      return -1;
   }
   *sz -= cc_size; nr -= sampleInfo->nbAddresses;

   // Skip unused records
   if (nr > 0) {
      const size_t size_to_skip = nr * sizeof (uint64_t);
      perf_skip_buffer (group_leader, size_to_skip); *sz -= size_to_skip;
   }

   return 0;
}

#ifdef __LIBUNWIND__
static array_t *load_maps (pid_t pid, pid_t tid)
{
   char file_name [strlen ("/proc/999999/task/999999/maps") + 1];
   sprintf (file_name, "/proc/%d/task/%d/maps", pid, tid);
   FILE *fp = fopen (file_name, "r");
   if (!fp) {
      DBGMSG ("Cannot open %s in read-only mode\n", file_name);
      return NULL;
   }
   array_t *maps = array_new();
   
   // For each line in the maps file
   char buf[1024]; char *line;
   while ((line = fgets (buf, sizeof buf, fp)) != NULL) {
      // Delete final newline character
      line [strlen (line) - 1] = '\0';

      // Parse maps line: addr perms offset dev inode name
      const char *const delim = " ";
      const char *const addr  = strtok (line, delim);
      const char *const perms = strtok (NULL, delim);
      if (strchr (perms, 'x') == NULL) continue; // Ignore non executable maps

      const char *const offset = strtok (NULL, delim);
      strtok (NULL, delim); strtok (NULL, delim); // skip dev, inode
      const char *const name = strtok (NULL, delim);

      if (!name || strlen (name) == 0) continue;

      map_t *map = lc_malloc (sizeof *map);
      char *addr_dup = lc_strdup (addr);
      map->start = strtol (strtok (addr_dup, "-"), NULL, 16);
      map->end   = strtol (strtok (NULL    , "-"), NULL, 16);
      lc_free (addr_dup);
      map->offset = strtol (offset, NULL, 16);
      map->name = lc_strdup (name);
      map->fd = -1;
      map->data = NULL;
      map->length = 0;
      map->di = NULL;
      array_add (maps, map);
      DBGMSG ("ADDED %"PRIx64"-%"PRIx64" %lu %s\n", map->start, map->end, map->offset, map->name);
   }

   fclose (fp);
   return maps;
}

static size_t read_sample_regs_user (perf_event_desc_t *group_leader, size_t *sz,
                                     unwind_context_t *unwind_context)
{
   // Read ABI
   uint64_t abi;
   if (perf_read_buffer_64 (group_leader, &abi) == -1) {
      DBGMSG0 ("Cannot read user regs ABI\n");
      return -1;
   }
   *sz -= sizeof (abi); 
   if (abi == 0) return -2;

   // Read frame-pointer register
   if (perf_read_buffer_64 (group_leader, &(unwind_context->bp)) == -1) {
      DBGMSG0 ("Cannot read frame-pointer register\n");
      return -1;
   }
   DBGMSG ("read_sample_regs_user: BP=%"PRIx64"\n", unwind_context->bp);
   *sz -= sizeof (unwind_context->bp);

   // Read stack-pointer register
   if (perf_read_buffer_64 (group_leader, &(unwind_context->sp)) == -1) {
      DBGMSG0 ("Cannot read stack-pointer register\n");
      return -1;
   }
   DBGMSG ("read_sample_regs_user: SP=%"PRIx64"\n", unwind_context->sp);
   *sz -= sizeof (unwind_context->sp);

   return 0;
}

static int read_sample_stack_user (perf_event_desc_t *group_leader,
                                   size_t *sz, sampleInfo_t *sampleInfo,
                                   unwind_data_t *unwind_data)
{
   uint64_t size; // stack size
   if (perf_read_buffer_64 (group_leader, &size) == -1) {
      DBGMSG0 ("Cannot read user stack size\n");
      return -1;
   }
   *sz -= sizeof (size);

   if (perf_read_buffer (group_leader, unwind_data->context.stack, size) != 0) {
      DBGMSG0 ("Cannot read user stack data\n");
      return -1;
   }
   *sz -= size;

   uint64_t dyn_size; // size effectively written
   if (perf_read_buffer_64 (group_leader, &dyn_size) == -1) {
      DBGMSG0 ("Cannot read user stack effective size\n");
      return -1;
   }
   *sz -= sizeof (dyn_size);

   // Read callchain in stack (unwind)
   uint64_t *ips = sampleInfo->callChainAddress;
   size_t nr = 0;
   unw_cursor_t cursor;
   int ret = unw_init_remote (&cursor, unwind_data->addr_space, &(unwind_data->context));
   if (ret < 0) {
      DBGMSG ("Cannot unw_init_remote: returned %d\n", ret);
      return 0;
   };
   do {
      unw_get_reg (&cursor, UNW_REG_IP, &ips[nr++]);
      DBGMSG ("[%lu] rip=%"PRIx64"\n", nr, ips[nr-1]);
   } while (unw_step (&cursor) > 0 && nr < CC_MAX_LEN);
   DBGMSG ("nr=%lu\n", nr);

   if (nr > 1) {
      sampleInfo->nbAddresses = (nr <= CC_MAX_LEN ? nr : CC_MAX_LEN);
   } else {
      DBGMSGLVL (1, "Too small callchain (nr=%"PRIu64")\n", nr);
      return 0;
   }

   return 0;
}
#endif // __LIBUNWIND__

IP_callchain_t *lookup_IP_callchain (const lprof_queue_t *callchains,
                                     uint32_t nb_IPs, const uint64_t *IPs)
{
   FOREACH_IN_LPROF_QUEUE (callchains, cc_iter) {
      IP_callchain_t *cc = GET_DATA_T (IP_callchain_t *, cc_iter);

      // Check for exact match
      if (cc->nb_IPs != nb_IPs) continue;
      unsigned i; boolean_t diff = FALSE;
      for (i=0; i<nb_IPs; i++)
         if (cc->IPs[i] != IPs[i]) {
            diff = TRUE; break;
         }
      if (diff == FALSE) return cc;
   }

   return NULL;
}

// PERFORMANCE AND RESULTS CRITICAL: HIGH EFFORT NEEDED FOR DESIGN/IMPLEMENTATION
/* Saves sample data to lprof internal structures (per thread/IP hashtables)
 * Tightly coupled with what comes next in lprof...
 * Present implementation based on old lprof data structures (not optimal...) */
void save_sample_in_results (smpl_context_t *context,
                             uint64_t ip, uint32_t tid,
                             int rank, uint32_t cpu,
                             sampleInfo_t *callchain,
                             sampler_data_t *sampler_data)
{
#ifndef NDEBUG
   static uint64_t cycles = 0;
   static uint64_t visits = 1;
   uint64_t start; rdtscll (start);
#endif

   sampler_data_buf_t *cur = sampler_data->cur;
   buf_t *buf = cur->buf;
   lprof_hashtable_t *const tid2ipt = cur->tid2ipt;
   lprof_hashtable_t *const tid2cpu = cur->tid2cpu;

   //SEARCH THREAD
   lprof_hashtable_t* ip2smp = lprof_hashtable_lookup (tid2ipt, tid);
   if (ip2smp == NULL) {
      //FIRST TIME THAT WE SEE THIS THREAD
      DBGMSG0LVL(1,  "FIRST OVERFLOW COMING FROM A NEW THREAD\n");
      //inc_nbThreadsDetected(); // needed by generate_lprof_file...
      ip2smp = lprof_hashtable_new (buf, IP2SMP_SIZE);
      lprof_hashtable_insert (tid2ipt, tid, ip2smp);
   }

   DBGMSGLVL(1, "THREAD IDENTIFIED :  %p\n", ip2smp);

   //SEARCH ADDRESS
   IP_events_t *IP_events = lprof_hashtable_lookup (ip2smp, ip);
   if (IP_events == NULL) {
      //FIRST TIME THAT WE SEE THIS ADDRESS
      unsigned i;

      IP_events = buf_add (buf, sizeof *IP_events);
      IP_events->callchains = lprof_queue_new (buf);
      IP_events->eventsNb = buf_add (buf, context->events_per_group *
                                     sizeof IP_events->eventsNb[0]);
      for (i=0; i<context->events_per_group; i++) IP_events->eventsNb[i] = 0;
      lprof_hashtable_insert (ip2smp, ip, IP_events);
   }

   //Adding one event to the address and to the total of event for the corresponding HWC
   DBGMSG0LVL(1,  "INSERTION IN HASHTABLE DONE\n");
   IP_events->eventsNb [rank] = IP_events->eventsNb [rank] + 1;

   // INSERT CALLCHAIN INFO
   if (rank == 0 && callchain != NULL && callchain->nbAddresses > 0) {
      IP_callchain_t *found_callchain = lookup_IP_callchain (IP_events->callchains,
                                                             callchain->nbAddresses,
                                                             callchain->callChainAddress);
      if (found_callchain == NULL) {
         // First time for this callchain => create
         IP_callchain_t *cc = buf_add (buf, sizeof *cc);
         cc->nb_hits = 1;
         cc->nb_IPs = callchain->nbAddresses;
         const size_t cc_size = callchain->nbAddresses * sizeof cc->IPs[0];
         cc->IPs = buf_add (buf, cc_size);
         memcpy (cc->IPs, callchain->callChainAddress, cc_size);
         lprof_queue_add (IP_events->callchains, cc);   
      } else {
         found_callchain->nb_hits++;
      }
   }

   //INSERT CPU ID INFO
   if (rank == 0) {
      // cpus is an histogram: cpus[4] = 8 => 8 samples hitting CPU4
      hits_nb_t *cpus = lprof_hashtable_lookup (tid2cpu, tid);
      if (cpus == NULL) {
         cpus = buf_add (buf, context->online_cpus * sizeof cpus[0]);
         memset (cpus, 0, context->online_cpus * sizeof cpus[0]);
         lprof_hashtable_insert (tid2cpu, tid, cpus);
      }
      cpus[cpu]++;
   }

#ifndef NDEBUG
   uint64_t stop; rdtscll (stop);
   DBG (cycles += (stop - start);
        if (visits % (100 * 1000) == 0)
           fprintf (stderr, "Average %"PRIu64" RDTSC cycles per save_sample_in_results call\n",
                    cycles / visits); // integer division sufficient here
        visits++);
#endif
}

// PERFORMANCE AND RESULTS CRITICAL: HIGH EFFORT NEEDED FOR DESIGN/IMPLEMENTATION
// Reads + processes a sample of type PERF_RECORD_SAMPLE
static int read_record_sample (smpl_context_t *context, perf_event_desc_t *group_fds, struct perf_event_header *header, sampler_data_t *sampler_data)
{
   // Info read from the sample
   //   For all events in the group
   uint64_t ip = 0;
   uint64_t id = 0;
   pid_t pid = -1;
   pid_t tid = -1;
   uint32_t cpu = 0;

   //   Only for group leaders (first event in the group)
   uint64_t ip_buf [CC_MAX_LEN];
   sampleInfo_t sampleInfo = { .nbAddresses = 0, .callChainAddress = ip_buf }; // Callchain/stack/branch infos

   // Get payload size (excluding metadata/header)
   size_t sz = header->size - sizeof *header;

   perf_event_desc_t *group_leader = &group_fds[0];
   const uint64_t type = group_leader->hw.sample_type;

   // Read address of the instruction responsible of overflow (IP = Instruction Pointer)
   if (type & PERF_SAMPLE_IP) {
      if (perf_read_buffer_64 (group_leader, &ip) == -1) {
         DBGMSG0 ("Cannot read IP\n");
         return -1;
      }
      DBGMSGLVL(1, "IP=%p\n", (void *) ip);
      sz -= sizeof ip;
   }

   // Read thread ID (TID = Thread ID)
   // TODO: remove this in ptrace-mode (since each perf-events group mapped to 1 thread)
   if (type & PERF_SAMPLE_TID) {
      struct { uint32_t pid, tid; } pid_tid;
      if (perf_read_buffer_64 (group_leader, &pid_tid) == -1) {
         DBGMSG0 ("Cannot read TID\n");
         return -1;
      }
      DBGMSGLVL(1, "PID=%"PRIu32" TID=%"PRIu32"\n", pid_tid.pid, pid_tid.tid);
      sz -= sizeof pid_tid;
      pid = pid_tid.pid;
      tid = pid_tid.tid;
   }

   // Read event ID, allow to disambiguate members in perf-events group
   if (type & PERF_SAMPLE_ID) {
      if (perf_read_buffer_64 (group_leader, &id) == -1) {
         DBGMSG0 ("Cannot read ID\n");
         return -1;
      }
      DBGMSGLVL(1, "ID=%"PRIu64"\n", id);
      sz -= sizeof id;
   }

   // Get rank of current event in perf-events group
   int rank = perf_id2event (group_fds, context->events_per_group, id);
   if (rank == -1) {
      DBGMSG ("Failed to get rank in the group (id=%"PRIu64")\n", id);
      return -1;
   }

   // If group leader
   if (id == group_fds[0].id) {
      // Read CPU rank
      if (type & PERF_SAMPLE_CPU) {
         struct { uint32_t cpu, res; } _cpu;
         if (perf_read_buffer_64 (group_leader, &_cpu) == -1) {
            DBGMSG0 ("Cannot read CPU\n");
            return -1;
         }
         DBGMSGLVL(1, "CPU=%"PRIu32"\n", _cpu.cpu);
         sz -= sizeof _cpu;
         cpu = _cpu.cpu;
      }

      // Read callchain
      if (type & PERF_SAMPLE_CALLCHAIN) {
         int ret = read_sample_callchain (group_leader, &sz, &sampleInfo);
         if (ret != 0) return ret;
      }

      // Read branch stack (Intel LBR, HW support)
      if (type & PERF_SAMPLE_BRANCH_STACK) {
         int ret = read_sample_branch_stack (group_leader, NULL /* TODO: &sampleInfo*/);
         sz -= ret;
      }

#ifdef __LIBUNWIND__
      // Read user registers and stack (allows unwinding)
      if ((type & PERF_SAMPLE_REGS_USER) && (type & PERF_SAMPLE_STACK_USER)) {
         // Get/create unwinding data
         unwind_data_t *unwind_data = hashtable_lookup (sampler_data->unwind_data, (void *)(uint64_t) tid);
         if (unwind_data == NULL) {
            unwind_data = lc_malloc (sizeof *unwind_data);
            unwind_data->addr_space = unw_create_addr_space (get_unw_accessors(), 0);
            memset (&unwind_data->context, 0, sizeof unwind_data->context);
            unwind_data->context.maps = load_maps (pid, tid);
            hashtable_insert (sampler_data->unwind_data, (void *)(uint64_t) tid, unwind_data);
         }
         unwind_data->context.ip = ip;
         int ret_regs = read_sample_regs_user (group_leader, &sz, &(unwind_data->context));
         if (ret_regs == -1) return -1; // sampling corruption
         if (ret_regs == 0) {
            int ret_stack = read_sample_stack_user (group_leader, &sz, &sampleInfo, unwind_data);
            if (ret_stack != 0) return ret_stack; // -1: sampling corruption, -2: emergency stop
         }
      }
#endif // __LIBUNWIND__
   }

   // Read/consume potentially leftover data in sample. Ring buffer corruption otherwise !
   if (sz) {
      DBGMSG ("%lu leftover bytes in sample\n", sz);
      perf_skip_buffer (group_leader, sz);
   }

   // Save previously read data to internal structures
   if (tid > 0) {
      size_t needed_size = 200 + IP2SMP_SIZE * sizeof (hashnode_t *);
      if (needs_swap_to_files (sampler_data, needed_size)) {
         swap_to_files (context, sampler_data);
      } else if (needs_dump_to_files (sampler_data, needed_size)) {
         dump_to_files_and_reset (context, sampler_data);
         if (context->emergency_stop == TRUE)
            return -2;
      }
      save_sample_in_results (context, ip, tid, rank,
                              context->sampling_engine == SAMPLING_ENGINE_INHERIT ? (uint32_t) group_leader->cpu : cpu,
                              &sampleInfo, sampler_data);
   }
   return 0;
}

// Reads PERF_RECORD_LOST records, containing number of lost events (due to buffer being full)
static int read_record_lost (perf_event_desc_t *group_fds, struct perf_event_header *header, sampler_data_t *sampler_data)
{
   uint64_t nb;
   size_t sz = header->size - sizeof *header;
   perf_event_desc_t *group_leader = &group_fds[0];

   perf_skip_buffer (group_leader, sizeof (uint64_t)); // id seems not reliable...

   if (perf_read_buffer_64 (group_leader, &nb) == -1) {
      DBGMSG0 ("Cannot read nb lost events\n");
      return -1;
   }
   sz -= 2*8;

   if (sz) {
      DBGMSG ("%lu leftover bytes in loss record\n", sz);
      perf_skip_buffer (group_leader, sz);
   }

   sampler_data->lost_events += nb;

   return 0;
}

// PERFORMANCE AND RESULTS CRITICAL: HIGH EFFORT NEEDED FOR DESIGN/IMPLEMENTATION
// At each overflow, process all data present in the perf_event_open ring buffer
// TODO: rename this (for example to process_samples_in_ring_buffer)
void process_overflow (smpl_context_t *context, perf_event_desc_t *group_fds, sampler_data_t *sampler_data)
{
   perf_event_desc_t *group_leader = &(group_fds[0]);
   struct perf_event_header header;

   // Loop over all records in ring buffer
   for(;;) {
      // Read record header => allows to know its type and read its content accordingly
      int ret = perf_read_buffer (group_leader, &header, sizeof header);
      if (ret || header.size == 0) return; // nothing to read

      /* Remark: All records needs to be entirely read: use perf_read_buffer (or perf_skip_buffer
       * if not interested with record content) and check for leftover bytes */

      // Get size available in buffer: should be at least equal to record size
      struct perf_event_mmap_page *metadata = group_leader->buf;
      size_t avail_sz = metadata->data_head - metadata->data_tail;
      size_t record_sz = header.size - sizeof(header);
      if (avail_sz < record_sz) {
         WRNMSG ("Corrupted sampling: lprof tries to continue but events will probably be lost\n");
         DBGMSG ("Available buffer space (%lu) lower than record size (%lu)\n", avail_sz, record_sz);
         perf_skip_buffer (group_leader, record_sz);
         continue;
      }

      switch (header.type) {

         // Sample record (most interesting)
      case PERF_RECORD_SAMPLE:
         {
            sampler_data->coll_events++;
            int ret = read_record_sample (context, group_fds, &header, sampler_data);
            if (ret != 0 && ret != -2) {
               ERRMSG ("Corrupted sampling");
               clean_abort (context->child_pid, context->output_path);
            }
            else if (ret == -2) return; // emergency stop
         }
         break;

         // Records containing data about perf-events throttling
      case PERF_RECORD_THROTTLE:
      case PERF_RECORD_UNTHROTTLE:
         sampler_data->coll_events++;
         DBGMSG0LVL(2, "PERF_RECORD_(UN)THROTTLE\n");
         perf_skip_buffer (group_leader, header.size - sizeof(header));
         break;

         // Records containing an estimation for the number of events lost due to buffer being full
      case PERF_RECORD_LOST:
         DBGMSG0LVL(2, "PERF_RECORD_LOST\n");
         if (read_record_lost (group_fds, &header, sampler_data) != 0) {
            ERRMSG ("Corrupted sampling");
            clean_abort (context->child_pid, context->output_path);
         }
         break;

         // Other records, not handled by lprof
      default:
         DBGMSG ("Unexpected PERF_RECORD type: %d\n", header.type);
         perf_skip_buffer (group_leader, header.size - sizeof(header));
         break;
      }
   }
}

/* Disables and unallocates a perf-events group.
 * Remaining samples still present in ring buffer are flushed (processed) */
void _stop_sampling (smpl_context_t *context, perf_event_desc_t *group_fds, sampler_data_t *sampler_data) {
   assert (group_fds != NULL);

   if (context->sampling_engine == SAMPLING_ENGINE_INHERIT) {
      DBGMSG ("Stopping sampling CPU%u\n", group_fds[0].cpu);
   } else {
      DBGMSG ("Stopping sampling TID%u\n", group_fds[0].cpu); // cpu used as tid
   }

   // Disable and close perf_events
   disable_events_group (group_fds, context);
   unsigned i;
   for (i=0; i < context->events_per_group; i++)
      close (group_fds[i].fd);

   // Flush buffer
   if (context->emergency_stop == FALSE) process_overflow (context, group_fds, sampler_data);

   // Unmap group leader ring buffer
   munmap (group_fds[0].buf, context->mmap_size);

   lc_free (group_fds);
}
