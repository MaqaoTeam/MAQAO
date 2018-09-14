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

/* Defines the dump_collect_data function() to dump smaples to IP_events.lprof and cpu_ids.info */

#include "sampling_engine_shared.h"
#include "IP_events_format.h"
#include "utils.h"

#define INIT_MERGE_BUF_SZ   (10 * 1024 * 1024) // 10 MB
#define MAX_MERGE_BUF_SZ  (1024 * 1024 * 1024) // 1 GB

// Write data contained in tid2ipt to IP_events.lprof
static void write_to_IP_events_dot_lprof (const smpl_context_t *context, FILE *fp,
                                          hashtable_t *tid2ipt, IP_events_t *(*get_IP_events)
                                          (const smpl_context_t *context, void *ip_data,
                                           buf_t **buf, raw_IP_events_t *raw_IP_events))
{
   // Write header data
   char *HW_event_name [context->events_per_group];
   unsigned i;
   if (context->sampling_engine != SAMPLING_ENGINE_TIMERS)
      for (i=0; i<context->events_per_group; i++)
         HW_event_name [i] = context->fds [i].name;
   else
      HW_event_name [0] = "OS_CLK";
   TID_events_header_t TID_events_header = { .nb_threads      = hashtable_size (tid2ipt),
                                             .HW_evts_per_grp = context->events_per_group,
                                             .HW_evts_name    = HW_event_name,
                                             .HW_evts_list    = context->eventsList,
                                             .sampleTypesList = context->sampleTypesList };
   if (write_TID_events_header (fp, &TID_events_header) != 0) {
      ERRMSG ("Cannot write TID events header\n");
      return;
   }

   // Allocate data/buffers reused for all TIDs/IPs
   raw_IP_events_t *raw_IP_events = raw_IP_events_new (context->events_per_group);
   buf_t *merge_buf = NULL;

   // For each thread/TID
   FOREACH_INHASHTABLE (tid2ipt, tid_iter) {
      const uint64_t tid = (uint64_t) GET_KEY (uint64_t, tid_iter);
      hashtable_t *ip2smp = GET_DATA_T (hashtable_t *, tid_iter);

      if (write_IP_events_header (fp, tid, hashtable_size (ip2smp)) != 0) {
         ERRMSG ("Cannot write IP events header\n");
         break;
      }

      // For each address/IP
      FOREACH_INHASHTABLE (ip2smp, ip_iter) {
         uint64_t ip = (uint64_t) GET_KEY (uint64_t, ip_iter);
         void *ip_data = GET_DATA_T (void *, ip_iter);

         // Get data (sample) related to current TID, IP and event
         IP_events_t *IP_events;
         if (get_IP_events != NULL)
            IP_events = get_IP_events (context, ip_data, &merge_buf, raw_IP_events);
         else
            IP_events = ip_data;

         if (write_IP_events (fp, ip, IP_events, context->events_per_group) != 0) {
            ERRMSG ("Cannot write IP events\n");
            raw_IP_events_free (raw_IP_events);
            buf_free (merge_buf);
            return;
         }
      } // for each IP
   } // for each thread/TID

   // Deallocate reused data/buffers
   raw_IP_events_free (raw_IP_events);
   buf_free (merge_buf);
}

// Write to cpu_ids.info the line corresponding to the thread 'tid'
static int write_tid_cpus (const smpl_context_t *context, FILE *fp, uint64_t tid, const hits_nb_t *cpus)
{
   // Count total number of CPU hits
   cpu_id_t cpu; uint64_t sum = 0;
   for (cpu=0; cpu < context->online_cpus; cpu++)
      sum += cpus [cpu];

   if (sum == 0) return 0;

   if (fprintf (fp, "%"PRIu64",", tid) < 0) return -1;

   // Print distribution of CPUs used by the thread 'tid'
   for (cpu=0; cpu < context->online_cpus; cpu++) {
      if (cpus [cpu] > 0) {
         if (fprintf (fp, "%"PRIu16",%f,", cpu, cpus [cpu] / (float) sum) < 0) return -1;
      }
   }

   if (fprintf (fp, "\n") < 0) return -1;

   return 0;
}

typedef struct {
   FILE *fp;
   unsigned pos; // in fp
} file_pos_t;

typedef struct {
   array_t *events; // array of IP_events_t (from memory)
   array_t *file_idx; // array of file_pos_t (each pointing to an IP_events_t object)
} index_ip_data_t;

/* Lookups by TID the (IP to IP_events) hashtable in index_tid2ipt
 * Creates empty one if needed */
static hashtable_t *get_insert_ip2smp (hashtable_t *index_tid2ipt, uint64_t tid)
{
   hashtable_t *ip2smp = hashtable_lookup (index_tid2ipt, (void *) tid);
   if (ip2smp == NULL) {
      ip2smp = hashtable_new (direct_hash, direct_equal);
      hashtable_insert (index_tid2ipt, (void *) tid, ip2smp);
   }

   return ip2smp;
}

/* Lookups by IP the related data in index_ip2smp
 * Creates empty one if needed */
static index_ip_data_t *get_insert_ip_data (hashtable_t *index_ip2smp, uint64_t ip)
{
   index_ip_data_t * ip_data = hashtable_lookup (index_ip2smp, (void *) ip);
   if (ip_data == NULL) {
      ip_data = lc_malloc0 (sizeof *ip_data);
      hashtable_insert (index_ip2smp, (void *) ip, ip_data);
   }

   return ip_data;
}

/* Gathers CPU IDs from all temporary files, by thread/TID
 * Returns as TID-indexed hashtable */
static hashtable_t *index_cpu_files (const smpl_context_t *context)
{
   hashtable_t *index_tid2cpu = hashtable_new (direct_hash, direct_equal);
   unsigned i;
   for (i=0; i<context->nb_sampler_threads; i++) {
      sampler_data_t *const sd = &(context->sampler_data[i]);

      FILE *fp_idx = fopen (sd->cpu_idx_file_name, "rb");
      if (!fp_idx) continue;
      FILE *fp = fopen (sd->cpu_file_name, "rb");
      if (!fp) { fclose (fp_idx); continue; }

      uint64_t tid;
      while (fread (&tid, sizeof tid, 1, fp_idx) == 1) {
         unsigned pos;
         if (fread (&pos, sizeof pos, 1, fp_idx) != 1) {
            ERRMSG ("Read error in %s\n", sd->cpu_idx_file_name);
            break;
         }

         file_pos_t *fpos = lc_malloc (sizeof *fpos);
         fpos->fp  = fp;
         fpos->pos = pos;
         hashtable_insert (index_tid2cpu, (void *) tid, fpos);
      }

      fclose (fp_idx);
   }

   return index_tid2cpu;
}

// Writes cpu_id.info from data aggregated in tid2cpu and using tid2ipt to iterate over threads
static void write_merged_cpus (const smpl_context_t *context, FILE *glob_fp_cpu,
                               hashtable_t *tid2ipt, hashtable_t *tid2cpu)
{
   hits_nb_t merged_cpus [context->online_cpus]; // reused for each thread/TID

   // For each thread/TID
   FOREACH_INHASHTABLE (tid2ipt, tid_iter) {
      uint64_t tid = (uint64_t) GET_KEY (uint64_t, tid_iter);
      cpu_id_t cpu;

      // Save number of times a CPU were hit by a sample for current thread
      memset (merged_cpus, 0, sizeof merged_cpus);

      // In memory
      unsigned i;
      // Accumulate counts from all sampler threads
      for (i=0; i<context->nb_sampler_threads; i++) {
         // Get histogram for current sampler thread
         hits_nb_t *cpus = lprof_hashtable_lookup (context->sampler_data[i].mem->tid2cpu, tid);
         if (cpus == NULL) continue;

         // Accumulate it to global (TID) histogram
         for (cpu=0; cpu < context->online_cpus; cpu++)
            merged_cpus[cpu] += cpus[cpu];
      }

      // In files
      array_t *file_pos = hashtable_lookup_all_array (tid2cpu, (void *) tid);
      unsigned nb_skip = 0;
      // Accumulate counts from all matching records in temporary files
      FOREACH_INARRAY (file_pos, file_iter) {
         file_pos_t *fpos = ARRAY_GET_DATA (file_pos_t *, file_iter);
         // set file position to desired record
         if (fseek (fpos->fp, fpos->pos, SEEK_SET) != 0) {
            nb_skip++; continue;
         }

         // Get histogram for current record
         hits_nb_t cpus [context->online_cpus];
         if (fread (cpus, sizeof cpus, 1, fpos->fp) != 1) {
            ERRMSG ("Read error in %s\n", context->sampler_data[i].cpu_file_name);
            break;
         }

         // Accumulate it to global (TID) histogram
         for (cpu=0; cpu < context->online_cpus; cpu++)
            merged_cpus[cpu] += cpus[cpu];
      }

      if (nb_skip > 0) WRNMSG ("Ignored CPU info for %u IP events records\n", nb_skip);
      array_free (file_pos, NULL); // hashtable_lookup_all_array...

      // Now histogram fully merged for thread 'tid': write it
      if (write_tid_cpus (context, glob_fp_cpu, tid, merged_cpus) != 0) {
         ERRMSG ("Write error in global CPU-info file\n");
         break;
      }
   } // for each thread
}

static hashtable_t *index_samples (const smpl_context_t *context)
{
   hashtable_t *tid2ipt = hashtable_new (direct_hash, direct_equal);

   unsigned i;
   // For each sampler thread
   for (i=0; i<context->nb_sampler_threads; i++) {
      sampler_data_t *const sd = &(context->sampler_data[i]);
      // For each TID, add data from memory
      FOREACH_IN_LPROF_HASHTABLE (sd->mem->tid2ipt, tid_iter) {
         uint64_t tid = GET_KEY (uint64_t, tid_iter);
         hashtable_t *ip2smp = get_insert_ip2smp (tid2ipt, tid);

         // For each IP/address, add data from memory
         lprof_hashtable_t *mem_ip2smp = GET_DATA_T (lprof_hashtable_t *, tid_iter);
         FOREACH_IN_LPROF_HASHTABLE (mem_ip2smp, ip_iter) {
            uint64_t ip = GET_KEY (uint64_t, ip_iter);
            IP_events_t *events = GET_DATA_T (IP_events_t *, ip_iter);

            index_ip_data_t *ip_data = get_insert_ip_data (ip2smp, ip);
            if (ip_data->events == NULL) ip_data->events = array_new();
            array_add (ip_data->events, events);
         }
      }

      // Add data from file
      FILE *fp_idx = fopen (sd->smp_idx_file_name, "rb");
      if (!fp_idx) continue;
      FILE *fp = fopen (sd->smp_file_name, "rb");
      if (!fp) { fclose (fp_idx); continue; }

      uint64_t tid; boolean_t read_error = FALSE;
      while (fread (&tid, sizeof tid, 1, fp_idx) == 1) {
         hashtable_t *ip2smp = get_insert_ip2smp (tid2ipt, tid);

         uint64_t ip;
         if (fread (&ip, sizeof ip, 1, fp_idx) != 1) {
            read_error = TRUE; break;
         }
         unsigned pos;
         if (fread (&pos, sizeof pos, 1, fp_idx) != 1) {
            read_error = TRUE; break;
         }

         file_pos_t *file_pos = lc_malloc (sizeof *file_pos);
         file_pos->fp = fp;
         file_pos->pos = pos;

         index_ip_data_t *ip_data = get_insert_ip_data (ip2smp, ip);
         if (ip_data->file_idx == NULL) ip_data->file_idx = array_new();
         array_add (ip_data->file_idx, file_pos);
      }

      if (read_error == TRUE) ERRMSG ("Read error in %s\n", sd->smp_idx_file_name);

      fclose (fp_idx);
   }

   return tid2ipt;
}

static IP_events_t *_get_merged_IP_events (unsigned events_per_group, index_ip_data_t *ip_data,
                                           buf_t *buf, raw_IP_events_t *IP_events)
{
   // Create empty IP_events structure
   IP_events_t *merged_IP_events = buf_add (buf, sizeof *merged_IP_events);
   size_t eventsNb_size = events_per_group * sizeof merged_IP_events->eventsNb[0];
   merged_IP_events->eventsNb   = buf_add (buf, eventsNb_size);
   memset (merged_IP_events->eventsNb, 0, eventsNb_size);
   merged_IP_events->callchains = lprof_queue_new (buf);

   // Copy from memory
   if (ip_data->events != NULL) {
      FOREACH_INARRAY (ip_data->events, ev_iter) {
         IP_events_t *mem_IP_events = ARRAY_GET_DATA (IP_events_t *, ev_iter);
         // Copy from memory: eventsNb
         unsigned ev_rank;
         for (ev_rank=0; ev_rank<events_per_group; ev_rank++)
            merged_IP_events->eventsNb[ev_rank] += mem_IP_events->eventsNb[ev_rank];

         // Copy from memory: callchains
         FOREACH_IN_LPROF_QUEUE (mem_IP_events->callchains, cc_iter) {
            IP_callchain_t *callchain = GET_DATA_T(IP_callchain_t *, cc_iter);
            IP_callchain_t *found = lookup_IP_callchain (merged_IP_events->callchains,
                                                         callchain->nb_IPs, callchain->IPs);
            if (found == NULL) {
               lprof_queue_add (merged_IP_events->callchains, callchain);
            } else
               found->nb_hits += callchain->nb_hits;
         }
      }
   }

   if (ip_data->file_idx == NULL) return merged_IP_events;

   // Merge from files
   unsigned nb_skip = 0;
   FOREACH_INARRAY (ip_data->file_idx, file_iter) {
      file_pos_t *file_pos = ARRAY_GET_DATA (file_pos_t *, file_iter);
      if ((fseek (file_pos->fp, file_pos->pos, SEEK_SET) != 0) ||
          (read_IP_events (file_pos->fp, IP_events, events_per_group) != 0)) {
         nb_skip++;
         continue;
      }

      // eventsNb
      unsigned i;
      for (i=0; i<events_per_group; i++)
         merged_IP_events->eventsNb[i] += IP_events->eventsNb[i];

      // callchains
      // callchains: read number of callchains
      size_t nb_callchains = IP_events->nb_callchains;

      // callchains: for each callchain
      for (i=0; i<nb_callchains; i++) {
         IP_callchain_t *callchain = &(IP_events->callchains[i]);
         IP_callchain_t *found = lookup_IP_callchain (merged_IP_events->callchains,
                                                      callchain->nb_IPs, callchain->IPs);
         if (found == NULL) {       
            // If too big for current buffer, give up
            size_t cc_size = callchain->nb_IPs * sizeof callchain->IPs[0];
            if ((sizeof *callchain + cc_size) > buf_avail (buf)) return NULL;

            IP_callchain_t *new_callchain = buf_add (buf, sizeof *new_callchain);
            new_callchain->nb_IPs = callchain->nb_IPs;
            new_callchain->IPs    = buf_add (buf, cc_size);
            memcpy (new_callchain->IPs, callchain->IPs, cc_size);
            new_callchain->nb_hits = callchain->nb_hits;

            lprof_queue_add (merged_IP_events->callchains, new_callchain);
         } else
            found->nb_hits += callchain->nb_hits;
      }
   }

   if (nb_skip > 0) WRNMSG ("Ignored %u IP events records\n", nb_skip);

   return merged_IP_events;
}

// Returns a merged IP_events data, from all memory buffers and all tmp files
static IP_events_t *get_merged_IP_events (const smpl_context_t *context, void *ip_data,
                                          buf_t **merge_buf, raw_IP_events_t *raw_IP_events)
{
#ifndef NDEBUG
   static uint64_t cycles = 0;
   static unsigned nb_visits = 1;
   uint64_t start; rdtscll (start);
#endif

   IP_events_t *merged_IP_events = NULL; // return value

   // Get merge buffer (create or flush existing)
   if (*merge_buf == NULL) // first invocation
      *merge_buf = buf_new (INIT_MERGE_BUF_SZ);
   else // buffer already created
      buf_flush (*merge_buf);

   // If 10 MB buffer not big enough, retry with doubled size until success
   while (merged_IP_events == NULL) {
      merged_IP_events = _get_merged_IP_events (context->events_per_group,
                                                ip_data, *merge_buf, raw_IP_events);
      if (merged_IP_events == NULL) {
         size_t buf_size = buf_length (*merge_buf);
         if (buf_size > MAX_MERGE_BUF_SZ) {
            ERRMSG ("Refusing to double merge buffer size after %.1f MB: probable bug\n",
                    MAX_MERGE_BUF_SZ / (float) (1024 * 1024));
            break;
         }
         buf_free (*merge_buf);
         *merge_buf = buf_new (buf_size * 2);
      }
   }

#ifndef NDEBUG
   uint64_t stop; rdtscll (stop);
   DBG (cycles += stop - start;
        if (nb_visits % (10 * 1000) == 0)
           fprintf (stderr, "%uK get_merged_IP_events visits: %lu RDTSC cycles (%.2f seconds @1 GHz)\n",
                    nb_visits / 1000, cycles, (float) cycles / (1000 * 1000 * 1000));
        nb_visits++);
#endif

   return merged_IP_events;
}

void dump_collect_data (smpl_context_t *context, const char *process_path, int64_t walltime)
{
   generate_walltime_uarch_files (process_path, walltime, context->uarch);

   FILE *glob_fp_smp = fopen_in_directory (process_path, "IP_events.lprof", "wb");
   FILE *glob_fp_cpu = fopen_in_directory (process_path, "cpu_id.info"  , "w");
   if (!glob_fp_smp || !glob_fp_cpu) return;

   if (context->nb_sampler_threads == 1 && context->sampler_data[0].file == NULL) {
      // No merge needed, can directly write files
      sampler_data_buf_t *mem = context->sampler_data[0].mem;

      // Dump samples to <process_path>/IP_events.lprof
      hashtable_t *tid2ipt = hashtable_new (direct_hash, direct_equal);
      {
         FOREACH_IN_LPROF_HASHTABLE (mem->tid2ipt, tid_iter) {
            uint64_t tid = GET_KEY (uint64_t, tid_iter);
            hashtable_t *ip2smp = hashtable_new (direct_hash, direct_equal);
            lprof_hashtable_t *mem_ip2smp = GET_DATA_T (lprof_hashtable_t *, tid_iter);
            FOREACH_IN_LPROF_HASHTABLE (mem_ip2smp, ip_iter) {
               uint64_t ip = GET_KEY (uint64_t, ip_iter);
               IP_events_t *IP_events = GET_DATA_T (IP_events_t *, ip_iter);
               hashtable_insert (ip2smp, (void *) ip, IP_events);
            }
            hashtable_insert (tid2ipt, (void *) tid, ip2smp);
         }
      }
      write_to_IP_events_dot_lprof (context, glob_fp_smp, tid2ipt, NULL);
      {
         FOREACH_INHASHTABLE (tid2ipt, tid_iter) {
            hashtable_t *ip2smp = GET_DATA_T (hashtable_t *, tid_iter);
            hashtable_free (ip2smp, NULL, NULL);
         }
      }
      hashtable_free (tid2ipt, NULL, NULL);

      // Print cpu info to <process_path>/cpu_id.info
      FOREACH_IN_LPROF_HASHTABLE (mem->tid2cpu, tid_iter) {
         const uint64_t tid = GET_KEY (uint64_t, tid_iter);
         hits_nb_t *cpus = GET_DATA_T (hits_nb_t *, tid_iter);
         if (write_tid_cpus (context, glob_fp_cpu, tid, cpus) != 0) {
            ERRMSG ("Write error in global CPU-info file\n");
            break;
         }
      }
   }
   else {
      // Merge needed
      // Dump samples to <process_path>/IP_events.lprof
      DBGMSG0 ("index_samples\n");
      hashtable_t *tid2ipt = index_samples (context);
      DBGMSG0 ("write_to_IP_events_dot_lprof\n");
#ifndef NDEBUG
      uint64_t start; rdtscll (start);
#endif
      write_to_IP_events_dot_lprof (context, glob_fp_smp, tid2ipt, get_merged_IP_events);
#ifndef NDEBUG
      uint64_t stop; rdtscll (stop);
      DBGMSG ("write_to_IP_events_dot_lprof: %lu RDTSC cycles (%.2f seconds @1 GHz)\n",
              stop - start, (float) (stop - start) / (1000 * 1000 * 1000));
#endif

      hashtable_t *tid2cpu = index_cpu_files (context);
      write_merged_cpus (context, glob_fp_cpu, tid2ipt, tid2cpu);

      // Close temporary files opened (but not closed) by index_samples and index_cpu_files
      unsigned i;
      for (i=0; i<context->nb_sampler_threads; i++) {
         sampler_data_t *const sd = &(context->sampler_data[i]);
         if (sd->fp_smp != NULL) fclose (sd->fp_smp);
         if (sd->fp_cpu != NULL) fclose (sd->fp_cpu);
      }

      // Free memory used for merge tables
      hashtable_free (tid2cpu, lc_free, NULL);
      FOREACH_INHASHTABLE (tid2ipt, tid_iter) {
         hashtable_t *ip2smp = GET_DATA_T (hashtable_t *, tid_iter);
         FOREACH_INHASHTABLE (ip2smp, ip_iter) {
            index_ip_data_t *ip_data = GET_DATA_T (index_ip_data_t *, ip_iter);
            array_free (ip_data->events, NULL);
            array_free (ip_data->file_idx, lc_free); /* file_pos */
         }
         hashtable_free (ip2smp, lc_free, NULL); /* ip_data: get_insert_ip_data */
      }
      hashtable_free (tid2ipt, NULL, NULL);
   } // merge needed

   fclose (glob_fp_smp);
   fclose (glob_fp_cpu);
}
