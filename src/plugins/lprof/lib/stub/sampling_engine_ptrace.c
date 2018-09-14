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

/* Defines functions used only by the ptrace-based sampling engine, main function being tracer_new */

#define _GNU_SOURCE      // pthread_yield
#include <stdio.h>       // printf
#include <unistd.h>      // close
#include <sys/types.h>   // waitpid
#include <sys/wait.h>    // waitpid
#include <sys/ptrace.h>  // ptrace
#include <poll.h>        // poll
#include <pthread.h>     // pthread_create...
#include "sampling_engine_shared.h" // smpl_context_t, enable/disable_events_group, process_overflow...

#define POLL_INIT_SZ   4 // Initial capacity of a sampler thread pollfds
                         // Corresponds to maximum expected events group per OS CPU
#define POLL_TIMEOUT 100 // poll timeout in milliseconds. Too small: overhead
#define MAX_PID_SIZE (6+1) // sizeof("999999") includind \0

// Data shared between tracer and sampler, using critical sections
// Remark: tid2fds_mutex prevents concurrent reads (should not be an issue with present implementation)
typedef struct {
   hashtable_t *tid2fds;
   pthread_mutex_t tid2fds_mutex; // protects tid2fds from being simultaneously read + written
   pthread_mutex_t rem_mutex;     // prevents ring_buffers from being read during removal
   sampler_data_t *sampler_data;
} shared_data_t;

// Context used only in this file
typedef struct {
   unsigned nb_ignored_threads;
   unsigned nb_sampler_threads;
   smpl_context_t *global_context;
   shared_data_t *shared_data;
   boolean_t finished;
} local_context_t;

// Enables/disables all events groups (for all threads)
static void enable_disable_all_threads (local_context_t *context, boolean_t enable)
{
   context->global_context->start_enabled = enable; // future threads will start enabled/disabled

   unsigned sampler_rank; shared_data_t *const shared_data = context->shared_data;

   // For each sampler thread
   for (sampler_rank=0; sampler_rank < context->nb_sampler_threads; sampler_rank++) {
      // Wait for events and tid2fds access
      pthread_mutex_lock (&(shared_data [sampler_rank].rem_mutex));
      pthread_mutex_lock (&(shared_data [sampler_rank].tid2fds_mutex));

      // For each group
      FOREACH_INHASHTABLE (shared_data [sampler_rank].tid2fds, tid2fds_iter) {
         // Enable/disable group
         perf_event_desc_t *const fds = GET_DATA (tid2fds_iter);
         if (enable) enable_events_group (fds, context->global_context);
         else disable_events_group (fds, context->global_context);
      }

      // Release access to events and tid2fds
      pthread_mutex_unlock (&(shared_data [sampler_rank].tid2fds_mutex));
      pthread_mutex_unlock (&(shared_data [sampler_rank].rem_mutex));
   }
}

// Enables all events groups (for all threads)
void enable_all_threads (void *UG_data)
{
   if (UG_data) enable_disable_all_threads (UG_data, TRUE);
}

// Disables all events groups (for all threads)
void disable_all_threads (void *UG_data)
{
   if (UG_data) enable_disable_all_threads (UG_data, FALSE);
}

/********************************** PTRACE-BASED SAMPLER ************************************/

// ptrace_sampler parameters
typedef struct {
   local_context_t *context;
   shared_data_t *shared_data;
   unsigned sampler_rank;
   size_t poll_init_size;
} ps_params_t;

// Run by each sampler thread
static void *ptrace_sampler (void *pparams)
{
   ps_params_t *const params = pparams;
   local_context_t *const context = params->context;
   shared_data_t *const shared_data = params->shared_data;

   if (context->nb_sampler_threads > 0) {
      DBGMSG ("Thread %ld will process chunck %u/%u samples\n", syscall(SYS_gettid), params->sampler_rank+1, context->nb_sampler_threads);
   } else {
      DBGMSG ("Thread %ld will process samples\n", syscall(SYS_gettid));
   }

   struct pollfd *pollfds = lc_malloc (params->poll_init_size * sizeof pollfds[0]);
   void **tid = lc_malloc (params->poll_init_size * sizeof tid[0]);
   size_t max_size = params->poll_init_size;

   // STARTING FROM HERE, HIGH EFFORTS NEEDED ON DESIGN AND IMPLEMENTATION
   // Main loop: while target application (in child process) is running
   while (1) {
      pthread_mutex_lock (&(shared_data->tid2fds_mutex));
      // Check if consumer (process_overflow) request production (perf_event_open) stop
      if (context->global_context->emergency_stop == TRUE) {
         FOREACH_INHASHTABLE (shared_data->tid2fds, tid2fds_iter) {
            perf_event_desc_t *const fds = GET_DATA (tid2fds_iter);
            disable_events_group (fds, context->global_context);
         }
         pthread_mutex_unlock (&(shared_data->tid2fds_mutex));
         break;
      }

      // Get number of inflight applications threads (in sampling)
      size_t nb_fds = hashtable_size (shared_data->tid2fds);
      if (context->finished) {
         if (nb_fds > 0) {
            pthread_mutex_lock (&(shared_data->rem_mutex));
            // Remove threads whose termination were not detected by main loop
            FOREACH_INHASHTABLE (shared_data->tid2fds, tid2fds_iter) {
               pid_t tid = (int64_t) GET_KEY (NULL, tid2fds_iter);
               perf_event_desc_t *const fds = GET_DATA (tid2fds_iter);
               DBGMSG ("Removing thread %d (not detected by main loop)\n", tid);
               _stop_sampling (context->global_context, fds, shared_data->sampler_data);
            }
            hashtable_flush (shared_data->tid2fds, NULL, NULL);
            pthread_mutex_unlock (&(shared_data->rem_mutex));
         }
         pthread_mutex_unlock (&(shared_data->tid2fds_mutex));
         break;
      }

      // Realloc if not big enough
      if (nb_fds > max_size) {
         const size_t new_max_size = (nb_fds > 2 * max_size) ? nb_fds : 2 * max_size;
         pollfds = lc_realloc (pollfds, new_max_size * sizeof pollfds[0]);
         tid     = lc_realloc (tid    , new_max_size * sizeof tid[0]);
         max_size = new_max_size;
      }

      // Build poll array
      size_t i = 0;
      FOREACH_INHASHTABLE (shared_data->tid2fds, tid2fds_iter) {
         void *const key = GET_KEY (NULL, tid2fds_iter);
         perf_event_desc_t *const fds = GET_DATA (tid2fds_iter);

         pollfds[i].fd     = fds[0].fd;
         pollfds[i].events = POLLIN;
         tid[i] = key;
         i++;
      }
      pthread_mutex_unlock (&(shared_data->tid2fds_mutex));

      // Wait for next wake-up signal, yield after POLL_TIMEOUT milliseconds to smoothly handle forks/exits
      if (poll (pollfds, nb_fds, POLL_TIMEOUT) > 0) { // Check for at least 1 ready ring buffer
         for (i=0; i<nb_fds; i++) {                   // For each ring buffer
            if (pollfds[i].revents & POLLIN) {        // If ready
               pthread_yield (); // Yield to more latency critical threads (events insertion/suppression)

               // Wait for both events and tid2fds
               pthread_mutex_lock (&(shared_data->rem_mutex));
               pthread_mutex_lock (&(shared_data->tid2fds_mutex));

               // Get group corresponding to the ring buffer
               perf_event_desc_t *const fds = hashtable_lookup (shared_data->tid2fds, tid[i]);

               pthread_mutex_unlock (&(shared_data->tid2fds_mutex)); // Release tid2fds access

               // Process samples in ring buffer
               if (fds) process_overflow (context->global_context, fds, shared_data->sampler_data);

               pthread_mutex_unlock (&(shared_data->rem_mutex)); // Release events access
            }
         }
      } else if (context->finished && hashtable_size (shared_data->tid2fds) > 0) {
         DBGMSG ("Detected %u uncaptured threads in sampler\n", hashtable_size (shared_data->tid2fds));
         return NULL;
      }
   }
   // END OF HIGH EFFORTS ZONE

   lc_free (pollfds);
   lc_free (tid);

   return NULL;
}

/*********************************** PTRACE-BASED TRACER *************************************/

/* Adds an application thread to sampler:
 *  - creates/starts events group
 *  - inserts group to tid2fds */
static inline void add_thread (local_context_t *context, shared_data_t *shared_data, pid_t tid)
{
   DBGMSG ("Adding thread %d\n", tid);

   pthread_mutex_lock (&(shared_data->tid2fds_mutex)); // wait for tid2fds

   // Create events group
   perf_event_desc_t *const fds = _start_sampling (context->global_context, 0, tid);

   // Insert to tid2fds
   if (!fds)
      context->nb_ignored_threads++;
   else
      hashtable_insert (shared_data->tid2fds, (void *)(uint64_t) tid, fds);

   pthread_mutex_unlock (&(shared_data->tid2fds_mutex)); // release tid2fds
}

/* Removes an application thread from sampler:
 *  - removes events group from tid2fds
 *  - stops sampling
 *  - frees related data */
static inline void rem_thread (local_context_t *context, shared_data_t *shared_data, pid_t tid)
{
   DBGMSG ("Removing thread %d\n", tid);

   // Wait for both event groups and tid2fds
   pthread_mutex_lock (&(shared_data->rem_mutex));
   pthread_mutex_lock (&(shared_data->tid2fds_mutex));

   // Remove group from tid2fds
   perf_event_desc_t *const fds = hashtable_remove (shared_data->tid2fds, (void *)(uint64_t) tid);

   // Stop/free group
   if (fds) _stop_sampling (context->global_context, fds, shared_data->sampler_data);

   // Release events/tid2fds access
   pthread_mutex_unlock (&(shared_data->tid2fds_mutex));
   pthread_mutex_unlock (&(shared_data->rem_mutex));
}

// add_rem_thread parameters
typedef struct {
   local_context_t *context;
   int *pipe;
} ar_params_t;

/* Listens to params->pipe for created or exited application threads:
 *  - for each created thread, calls add_thread to create/insert an events group
 *  - for each exited thread, calls rem_thread to remove/destroy related events group
 * This function is part of the asynchronous tracer */
static void *add_rem_thread (void *pparams)
{
   ar_params_t *const params = pparams;
   local_context_t *const context = params->context;
   shared_data_t *const shared_data = context->shared_data;

   const size_t nb_sampler_threads = context->nb_sampler_threads;
   char buf [MAX_PID_SIZE+1];
   // Wait for TIDs to be written in the pipe
   while (read (params->pipe[0], buf, sizeof buf) > 0) {
      const pid_t tid = atol (buf+1);
      const unsigned sampler_rank = nb_sampler_threads > 1 ? tid % nb_sampler_threads : 0;

      if (buf[0] == '+')
         add_thread (context, &(shared_data [sampler_rank]), tid);
      else if (buf[0] == '-')
         rem_thread (context, &(shared_data [sampler_rank]), tid);
   }

   close (params->pipe[0]);
   return NULL;
}

// Wrapper to synchronous/async. threads insertion
static inline void add_tid (boolean_t sync, local_context_t *context, int *_pipe, pid_t tid)
{
   if (!sync) {
      // Asynchronous processing of created thread: TID pushed to a queue
      char buf [MAX_PID_SIZE+1];
      memset (buf, 0, sizeof buf); // silents valgrind unitialized bytes errors
      snprintf (buf, sizeof buf, "+%d", tid);
      if (write (_pipe[1], buf, sizeof buf) == -1) {
         DBG(perror ("Cannot notify thread addition to sampler");)
            }
   } else {
      // Synchronous processing of created thread
      shared_data_t *const shared_data = context->shared_data;
      const size_t nb_sampler_threads = context->nb_sampler_threads;
      const unsigned sampler_rank = nb_sampler_threads > 1 ? tid % nb_sampler_threads : 0;
      add_thread (context, &(shared_data [sampler_rank]), tid);
   }
}

// Wrapper to synchronous/async. threads removal
static inline void rem_tid (boolean_t sync, local_context_t *context, int *_pipe, pid_t tid)
{
   if (!sync) {
      // Asynchronous processing of exited thread: TID pushed to a queue
      char buf [MAX_PID_SIZE+1];
      memset (buf, 0, sizeof buf); // silents valgrind unitialized bytes errors
      snprintf (buf, sizeof buf, "-%d", tid);
      if (write (_pipe[1], buf, sizeof buf) == -1) {
         DBG(perror ("Cannot notify thread removal to sampler");)
            }
   } else {
      // Synchronous processing of exited thread
      shared_data_t *const shared_data = context->shared_data;
      const size_t nb_sampler_threads = context->nb_sampler_threads;
      const unsigned sampler_rank = nb_sampler_threads > 1 ? tid % nb_sampler_threads : 0;
      rem_thread (context, &(shared_data [sampler_rank]), tid);
   }
}

/* Collects samples using ptrace (instead of perf-events inherit mode) to track processes/threads forks/exits
 * \param global_context sampling context
 * \param nprocs number of CPUs to collect, used only for pre-allocating resources
 * \param sync boolean true (resp. false) for synchronous (resp. async.) processing of forks/exits
 * \param finalize_signal Signal used by some parallel launchers to notify ranks finalization */
int tracer_new (smpl_context_t *const global_context, unsigned nprocs,
                boolean_t sync, int finalize_signal)
{
   boolean_t removed_child = FALSE;
   const pid_t child_pid = global_context->child_pid;

   // Initialize sampler threads data
   const unsigned nb_sampler_threads = global_context->nb_sampler_threads;
   unsigned i;
   shared_data_t *shared_data = lc_malloc (nb_sampler_threads * sizeof shared_data[0]);
   for (i=0; i<nb_sampler_threads; i++) {
      shared_data[i].tid2fds = hashtable_new (direct_hash, direct_equal);
      pthread_mutex_init (&(shared_data[i].tid2fds_mutex), NULL);
      pthread_mutex_init (&(shared_data[i].rem_mutex), NULL);
      shared_data[i].sampler_data = &(global_context->sampler_data[i]);
   }

   local_context_t context = { 0, nb_sampler_threads, global_context, shared_data, FALSE };
   global_context->UG_data = &context;

   // Create pool of sampler threads (at least 1)
   pthread_t threadSampler [nb_sampler_threads];
   ps_params_t ps_params [nb_sampler_threads];
   for (i=0; i<nb_sampler_threads; i++) {
      ps_params_t tmp = { &context, &(context.shared_data[i]), i,
                          (POLL_INIT_SZ * nprocs) / nb_sampler_threads };
      ps_params[i] = tmp;
      if (pthread_create (&threadSampler[i], NULL, ptrace_sampler, &ps_params[i]) != 0) {
         perror ("Cannot create sampler thread\n");
         clean_abort (child_pid, global_context->output_path);
      }
   }

   // Create structures to offload tracer from managing new application threads (start sampling)
   int pipe_add_rem_threads[2];
   pthread_t threadAddThreads;
   ar_params_t add_rem_params = { &context, pipe_add_rem_threads };
   if (!sync) {
      if (pipe (pipe_add_rem_threads) == -1) {
         DBG(perror ("Cannot create a pipe to notify added threads to sampler");)
            }
      pthread_create (&threadAddThreads, NULL, add_rem_thread, &add_rem_params);
   }

   // Take control (resume execution) of application
   ptrace (PTRACE_CONT, child_pid, NULL, NULL);

   int status; pid_t child;
   // Wait for signals from any process forked from the original application process
   while ((child = waitpid (-child_pid, &status, __WALL)) != -1) {
      DBGMSG ("tracer %d: got %d from waitpid\n", getpid(), child);

      // An application process exited or were signaled (killed...): remove related events group
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
         if (WIFEXITED(status)) {
            DBGMSG ("tracer %d: %d exited with status %d\n", getpid(), child, WEXITSTATUS(status));
         } else {
            DBGMSG ("tracer %d: %d terminated by signal %d\n", getpid(), child, WTERMSIG(status));
         }
         rem_tid (sync, &context, pipe_add_rem_threads, child);
         if (child == child_pid) removed_child = TRUE;
      }

      /* An application process stopped: insert/remove events group accordingly:
       *  - ptrace-stopped with fork detection, create/insert new events group
       *  - other cases: remove events group
       * In all cases, process execution is resumed */
      else if (WIFSTOPPED(status)) {
         DBGMSG ("tracer %d: %d stopped by signal %d\n", getpid(), child, WSTOPSIG(status));
         unsigned long newpid;
         switch (WSTOPSIG(status)) {

            // CF man 7 signal
         case SIGHUP:
         case SIGINT:
         case SIGQUIT:
         case SIGILL:
         case SIGABRT:
         case SIGFPE:
         case SIGKILL:
         case SIGSEGV:
         case SIGPIPE:
         case SIGALRM:
         case SIGTERM:
            DBGMSG ("tracer %d: %d terminated\n", getpid(), child);
            rem_tid (sync, &context, pipe_add_rem_threads,  child);
            if (child == child_pid) removed_child = TRUE;
            kill (child, SIGKILL);
            /* Implementation choice: lprof is not interrupted (only application is terminated)
             * and will account/display events up to application termination */
            break;

         case SIGTRAP:
            switch (status>>8) {

               // Application start
            case SIGTRAP | (PTRACE_EVENT_EXEC<<8):
               DBGMSG ("tracer %d: %d called exec\n", getpid(), child);
               if (child == child_pid)
                  add_tid (sync, &context, pipe_add_rem_threads, child_pid);
               break;

               // New process
            case SIGTRAP | (PTRACE_EVENT_CLONE<<8):
            case SIGTRAP | (PTRACE_EVENT_FORK <<8):
            case SIGTRAP | (PTRACE_EVENT_VFORK<<8):
               if (ptrace (PTRACE_GETEVENTMSG, child, NULL, &newpid) != -1) {
                  DBGMSG ("tracer %d: %d forked new PID=%lu\n", getpid(), child, newpid);
                  add_tid (sync, &context, pipe_add_rem_threads, newpid);
               } else
                  DBGMSG ("tracer %d: %d forked with unknown PID\n", getpid(), child);
               break;

            default:
               DBGMSG ("tracer %d: unexpected SIGTRAP status for %d\n", getpid(), child);
            } // end of switch status>>8
            break; // end of case SIGTRAP

         default:
            if (WSTOPSIG(status) == finalize_signal) {
               DBGMSG ("tracer %d: %d stopped by finalize signal\n", getpid(), child);
               kill (-child_pid, SIGTERM);
               break;
            }

#if defined (SIGRTMIN) && defined (SIGRTMAX)
            if (WSTOPSIG(status) >= SIGRTMIN && WSTOPSIG(status) <= SIGRTMAX) {
               DBGMSG ("tracer %d: %d stopped by real-time signal\n", getpid(), child);
            }
            else {
               DBGMSG ("tracer %d: unhandled STOPSIG for %d\n", getpid(), child);
            }
#else
            DBGMSG ("tracer %d: unhandled STOPSIG for %d\n", getpid(), child);
#endif
         } // end of switch WSTOPSIG

         ptrace (PTRACE_CONT, child, NULL, NULL);
      } // end of if (WIFSTOPPED)
      else {
         DBGMSG ("tracer %d: unhandled wait status for %d\n", getpid(), child);
      }
   }

   // Stop/free events group for application root process (if not already done)
   if (!removed_child) rem_tid (sync, &context, pipe_add_rem_threads, child_pid);

   // Asynchronous monitor: close pipe and wait for related thread termination
   if (!sync) { close (pipe_add_rem_threads[1]); pthread_join (threadAddThreads, NULL); }

   global_context->UG_data = NULL;

   context.finished = TRUE; // Notify helper threads

   // Wait for sampler threads termination
   for (i=0; i<nb_sampler_threads; i++) pthread_join (threadSampler[i], NULL);

   if (context.nb_ignored_threads > 0)
      WRNMSG ("%d threads exited while starting counters: ignored\n", context.nb_ignored_threads);

   // Free sampler threads data
   for (i=0; i<nb_sampler_threads; i++) {
      hashtable_free (shared_data[i].tid2fds, NULL, NULL);
      pthread_mutex_destroy (&(shared_data[i].tid2fds_mutex));
      pthread_mutex_destroy (&(shared_data[i].rem_mutex));
   }
   lc_free (shared_data);

   return 0;
}
