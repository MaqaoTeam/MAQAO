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

/* Defines functions used only by the inherit-based sampling engine, main function being inherit_sampler */

#include <stdio.h>       // printf
#include <unistd.h>      // close
#include <sys/types.h>   // waitpid
#include <sys/wait.h>    // waitpid
#include <poll.h>        // poll
#include <pthread.h>     // pthread_create...
#include "sampling_engine_shared.h" // smpl_context_t, enable/disable_events_group, process_overflow...

#define POLL_TIMEOUT 500 // poll timeout in milliseconds. Too small: overhead, too high: can miss forks/exits

// Inherit-specific data passed to the "user-guided" thread routine
typedef struct {
   unsigned nb_sampler_threads;
   smpl_context_t *global_context;
   array_t **fds_array;
} inherit_UG_data_t;

static void enable_disable_all_cpus (inherit_UG_data_t *data, boolean_t enable)
{
   unsigned sampler_rank;
   // For each sampler thread
   for (sampler_rank=0; sampler_rank < data->nb_sampler_threads; sampler_rank++) {
      // For each events group
      FOREACH_INARRAY (data->fds_array [sampler_rank], fds_iter) {
         // Enable/disable group
         perf_event_desc_t *const fds = ARRAY_GET_DATA (NULL, fds_iter);
         if (enable) enable_events_group (fds, data->global_context);
         else disable_events_group (fds, data->global_context);
      }
   }
}

// Enables all events groups (for all CPUs)
void enable_all_cpus (void *UG_data)
{
   if (UG_data) enable_disable_all_cpus (UG_data, TRUE);
}

// Disables all events groups (for all CPUs)
void disable_all_cpus (void *UG_data)
{
   if (UG_data) enable_disable_all_cpus (UG_data, FALSE);
}

// inherit_worker_routine parameters
typedef struct {
   smpl_context_t *context;
   array_t *fds_array;
   sampler_data_t *sampler_data;
} iwk_params_t;

static void process_events (struct pollfd pollfds[], unsigned nb_fds, iwk_params_t *params)
{
   if (poll (pollfds, nb_fds, POLL_TIMEOUT) > 0) { // At least one ring buffer ready
      unsigned i;
      for (i=0; i<nb_fds; i++) {                   // For each ring buffer
         if (pollfds[i].revents & POLLIN)          // If ready
            process_overflow (params->context,     // Process samples in this ring buffer
                              array_get_elt_at_pos (params->fds_array, i),
                              params->sampler_data);
      }
   }
}

// Inherit-based sampler (for one worker thread)
static void *inherit_worker_routine (void *pparams)
{
   iwk_params_t *params = pparams;
   const unsigned nb_fds = array_length (params->fds_array);
   struct pollfd pollfds [nb_fds];
   boolean_t events_enabled = TRUE;

   // Builds list of file descriptors to listen by poll
   unsigned i = 0;
   FOREACH_INARRAY (params->fds_array, fds_iter) {
      perf_event_desc_t *const fds = ARRAY_GET_DATA (NULL, fds_iter);
      pollfds[i].fd = fds[0].fd;
      pollfds[i].events = POLLIN;
      i++;
   }

   // While application not finished, listen for ready ring buffers and process them
   while (waitpid (params->context->child_pid, NULL, WNOHANG) == 0) {
      if (params->context->emergency_stop == FALSE) {
         process_events (pollfds, nb_fds, params);
      }
      else if (events_enabled) {
         FOREACH_INARRAY (params->fds_array, fds_iter) {
            perf_event_desc_t *const fds = ARRAY_GET_DATA (NULL, fds_iter);
            disable_events_group (fds, params->context);
         }
         events_enabled = FALSE;
         process_events (pollfds, nb_fds, params);
      }
      else sleep (1);
   }

   return NULL;
}

/* Collects samples using the inherit mode of perf-events
 * \param context sampling context
 * \param nprocs number of CPUs to collect
 * \param wait_pipe pipe used to synchronize with application start
 * \param cpu_array list of CPUs to collect */
void inherit_sampler (smpl_context_t *const context, unsigned nprocs, int *wait_pipe, unsigned *cpu_array)
{
   // Initialize parameters for each sampler thread
   unsigned i; const unsigned nb_sampler_threads = context->nb_sampler_threads;
   iwk_params_t params [nb_sampler_threads];
   for (i=0; i<nb_sampler_threads; i++) {
      params[i].context = context;
      params[i].fds_array = array_new ();
      params[i].sampler_data = &(context->sampler_data[i]);
   }

   // Start sampling for all CPUs defined by cpu_array
   for (i=0; i<nprocs; i++) {
      perf_event_desc_t *const fds =
         _start_sampling (context, cpu_array[i], -1);
      unsigned thread_rank = i%nb_sampler_threads;
      fds[0].idx = thread_rank; // cpu rank (if sampling CPU4 to 7, idx=0 when cpu=4)
      array_add (params [thread_rank].fds_array, fds);
   }

   // Set user-guided data
   array_t *fds_array_all [nb_sampler_threads];
   for (i=0; i<nb_sampler_threads; i++) fds_array_all[i] = params[i].fds_array;
   inherit_UG_data_t UG_data = { nb_sampler_threads, context, fds_array_all };
   context->UG_data = &UG_data;

   // Notify application to start
   close (wait_pipe[1]);

   pthread_t worker [nb_sampler_threads];

   // Creating/starting workers
   for (i=0; i<nb_sampler_threads; i++) {
      if (pthread_create (&worker[i], NULL, inherit_worker_routine, &params[i])) {
         perror ("Cannot create a worker thread");
         clean_abort (context->child_pid, context->output_path);
      }
   }

   // At this point, workers are processing samples

   // Joining workers (when finished)
   for (i=0; i<nb_sampler_threads; i++) pthread_join (worker[i], NULL); // join: wait for thread termination

   context->UG_data = NULL;

   // Stop sampling for all CPUs defined by cpu_array
   for (i=0; i<nb_sampler_threads; i++) {
      FOREACH_INARRAY (params[i].fds_array, fds_iter) {
         perf_event_desc_t *const fds = ARRAY_GET_DATA (NULL, fds_iter);
         _stop_sampling (context, fds, params[i].sampler_data);
      }
      array_free (params[i].fds_array, NULL);
   }
}
