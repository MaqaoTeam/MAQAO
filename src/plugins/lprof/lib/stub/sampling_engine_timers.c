#include <stdio.h>
#include <errno.h>
#include <time.h>        // nanosleep
#include <dirent.h>      // DIR, opendir...
#include <signal.h>
#include <sys/types.h>   // waitpid
#include <sys/wait.h>    // waitpid
#include <sys/ptrace.h>  // ptrace
#include <sys/user.h>    // user_regs_struct
#include <pthread.h>     // pthread_create...
#include "libmcommon.h"  // array_t
#include "sampling_engine_shared.h" // smpl_context_t, enable/disable_events_group, process_overflow...

static void get_tids (pid_t pid, array_t *tids)
{
   array_flush (tids, NULL);

   char task_dir_name [strlen ("/proc/999999/task") + 1];
   sprintf (task_dir_name, "/proc/%d/task", pid);

   DIR *dir = opendir (task_dir_name);
   if (!dir) {
      perror ("opendir");
      return;
   }

   struct dirent *dir_entry;
   while ((dir_entry = readdir (dir)) != NULL) {
      char *endptr;
      long tid = strtol (dir_entry->d_name, &endptr, 10);
      if (tid < 1 || *endptr != '\0') continue;
      array_add (tids, (void *)(uint64_t) tid);
   }

   closedir (dir);
}

typedef struct {
   pid_t child_pid;
   size_t period;
} OSC_params_t;

static void *OS_clock_routine (void *pparams)
{
   OSC_params_t *params = pparams;

   struct timespec req = { .tv_sec = params->period/1000, .tv_nsec = (params->period%1000) * (1000*1000) };
   while (TRUE) {
      nanosleep (&req, NULL);
      kill (params->child_pid, SIGSTOP);
   }

   return NULL;
}

/* Collects samples using OS timers
 * \param context sampling context
 * \param period sampling period in milliseconds
 * \param finalize_signal Signal used by some parallel launchers to notify ranks finalization */
void timers_sampler (smpl_context_t *const context, size_t period, int finalize_signal)
{
   const pid_t child_pid = context->child_pid;

   pthread_t OS_clock;
   OSC_params_t params = { .child_pid = context->child_pid, .period = period };
   if (pthread_create (&OS_clock, NULL, OS_clock_routine, &params)) {
      perror ("Cannot create a worker thread");
      clean_abort (context->child_pid, context->output_path);
   }
  
   array_t *tids = array_new();
   int status; pid_t child;
   // Wait for signals from any process forked from the original application process
   while ((child = waitpid (-child_pid, &status, WUNTRACED | WCONTINUED | __WALL)) != -1) {
      DBGMSG ("tracer %d: got %d from waitpid\n", getpid(), child);

      // An application process exited or were signaled (killed...)
      if (WIFEXITED(status)) {
         DBGMSG ("tracer %d: %d exited with status %d\n", getpid(), child, WEXITSTATUS(status));
         continue;
      }

      if (WIFSIGNALED(status)) {
         DBGMSG ("tracer %d: %d terminated by signal %d\n", getpid(), child, WTERMSIG(status));
         kill (child, SIGKILL);
         continue;
      }

      if (!WIFSTOPPED(status)) continue;
      DBGMSG ("tracer %d: %d stopped by signal %d\n", getpid(), child, WSTOPSIG(status));

      if (WSTOPSIG(status) == SIGSTOP) {
         get_tids (child, tids);
         FOREACH_INARRAY (tids, tid_iter) {
            pid_t tid = (pid_t)(uint64_t) ARRAY_GET_DATA (uint64_t, tid_iter);
            ptrace (PTRACE_ATTACH, tid, (void *)0, (void *)0);
            struct user_regs_struct regs;
            ptrace (PTRACE_GETREGS, tid, &regs, &regs);
            ptrace (PTRACE_DETACH, tid, (void *)0, (void *)0);
         }
      }
      else {
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
      }

      kill (child, SIGCONT);
   }

   array_free (tids, NULL);
   pthread_cancel (OS_clock);
}
