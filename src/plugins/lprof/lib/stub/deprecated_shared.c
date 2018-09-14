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

// kill()
#include <sys/types.h>
#include <signal.h>

#include "libmmaqao.h"  // utils_get_proc_host
#include "libmasm.h"    // proc_t
#include "deprecated_shared.h"
#include "perf_event.h" // perf_event_attr
#include "perf_util.h"  // perf_event_desc_t
#include "utils.h"      // fopen_in_directory
#include "consts.h"     // LPROF_VERBOSITY_ON
#include "strings.h"    // STR_LPROF_MAQAO_TAG

//Declaring micro-architecture identifiers
#include "arch.h"

int get_uarch (int *arch)
{
   /**\todo (2016-12-05) Rewrite this function and all functions invoking it to make full use of the
    * new uarch_t and proc_t structures. Right now this version ensures compatibility with what existed
    * before, so it only returns identifiers.*/
   proc_t* proc = utils_get_proc_host(NULL);
   uarch_t* uarch = proc_get_uarch(proc);
   *arch = arch_get_code(uarch_get_arch(uarch));
   return uarch_get_id(uarch);
}

// Useful to debug get_os_event_encoding_default
void utils_print_struct_event_attr(struct perf_event_attr* event)
{
   fprintf(stderr, " event->.type = %#"PRIx32"\n",event->type);
   fprintf(stderr, " event->.size = %#"PRIx32"\n",event->size);
   fprintf(stderr, " event->.config = %#"PRIx64"\n",event->config);

   fprintf(stderr, " event->.sample_type = %#"PRIx64"\n",event->sample_type);
   fprintf(stderr, " event->.sample_period = %"PRIu64"\n",event->sample_period);
   fprintf(stderr, " event->.read_format = %#"PRIx64"\n",event->read_format);

   fprintf(stderr, " event->.disabled    = %d\n",event->disabled);
   fprintf(stderr, " event->.inherit     = %d\n",event->inherit);
   fprintf(stderr, " event->.pinned      = %d\n",event->pinned);
   fprintf(stderr, " event->.exclusive   = %d\n",event->exclusive);
   fprintf(stderr, " event->.exclude_use = %d\n",event->exclude_user);
   fprintf(stderr, " event->.exclude_ker = %d\n",event->exclude_kernel);
   fprintf(stderr, " event->.exclude_hv  = %d\n",event->exclude_hv);
   fprintf(stderr, " event->.exclude_idl = %d\n",event->exclude_idle);
   fprintf(stderr, " event->.mmap;       = %d\n",event->mmap);
   fprintf(stderr, " event->.comm;       = %d\n",event->comm);
   fprintf(stderr, " event->.freq;       = %d\n",event->freq);
   fprintf(stderr, " event->.inherit_sta = %d\n",event->inherit_stat);
   fprintf(stderr, " event->.enable_on_e = %d\n",event->enable_on_exec);
   fprintf(stderr, " event->.task;       = %d\n",event->task);
   fprintf(stderr, " event->.watermark   = %d\n",event->watermark);
   fprintf(stderr, " event->.precise_ip  = %d\n",event->precise_ip);
   fprintf(stderr, " event->.mmap_data   = %d\n",event->mmap_data);
   fprintf(stderr, " event->.sample_id_a = %d\n",event->sample_id_all);
   fprintf(stderr, " event->.exclude_hos = %d\n",event->exclude_host);
   fprintf(stderr, " event->.exclude_gue = %d\n",event->exclude_guest);
   //fprintf(stderr, " event->.exclude_cal   = %d\n",event->exclude_callchain_kernel);
   //fprintf(stderr, " event->.exclude_cal   = %d\n",event->exclude_callchain_user);
   //fprintf(stderr, " event->.__reserved_ = %llu\n",event->__reserved_1);
}

static int get_os_event_encoding_default (perf_event_desc_t *event, int arch, int uarch,
                                          const char *event_name, char kill_on_failure)
{
   event->hw.type=0x4;
   switch (arch)
   {
   default :
      fprintf(stderr,"NORMALLY IT'S AN IMPOSSIBLE ERROR... NORMALLY...:)\n");
      break;
   }
   //utils_print_struct_event_attr(&(event->hw));

   return 0;
}

/* Converts HW events symbolic name to raw code
 * TODO: remove this and use libpfm4 or CPU vendor databases (Intel: https://download.01.org/perfmon/)
 */
int maqao_get_os_event_encoding(    int arch,
                                    int uarch,
                                    perf_event_desc_t* event,
                                    const char* event_name,
                                    int raw_code_id, int64_t *rawCode,
                                    char kill_on_failure
   )
{
   //Custom events
   
   if (raw_code_id != -1)
   {
      if (rawCode[raw_code_id] != -1)
      {
         //fprintf(stderr,"rawCode[%d] = %#"PRIx64"\n",
         //                raw_code_id, rawCode[raw_code_id]);
         event->hw.type=0x4;
         event->hw.config = rawCode[raw_code_id];
         return 0;
      }
   }

   if (arch == ARCH_NONE || uarch == UARCH_NONE)
      uarch = get_uarch (&arch);

   return get_os_event_encoding_default (event, arch, uarch, event_name, kill_on_failure);
}

char* getHwcList(int arch,int uarch,int verbosity, char *uarch_string)
{
   char* hwcList = NULL;
   switch (arch)
   {
   default:
      ERRMSG ("%s %s\n", STR_LPROF_MAQAO_TAG, STR_LPROF_UNKNOWN_PROCESSOR_DETECTED);
      return NULL;
   }

   return hwcList;
}

void set_sample_type ( uint64_t sampleTypes, int backtrace_mode, int nbEvents, uint64_t* sampleTypesList )
{
   int eventIdx = 0;

   // DEFAULT LPROF VALUE
   if (sampleTypes == 0)
   {
      //ENABLE CALLCHAIN AND CPU INFO ONLY ON THE HWC THAT COUNTS UNHALTED CYCLES
      //sampleTypesList[eventIdx] = LPROF_SAMPLE_TYPE_LIST | LPROF_SAMPLE_TYPE_EXTRA;

      switch ( backtrace_mode )
      {
      case BACKTRACE_MODE_CALL:
         sampleTypesList[eventIdx] = LPROF_SAMPLE_TYPE_LIST | LPROF_SAMPLE_TYPE_EXTRA | PERF_SAMPLE_CALLCHAIN;
         break;

      case BACKTRACE_MODE_STACK:
         sampleTypesList[eventIdx] = LPROF_SAMPLE_TYPE_LIST | LPROF_SAMPLE_TYPE_EXTRA | PERF_SAMPLE_STACK_USER;
         break;

      case BACKTRACE_MODE_BRANCH:
         sampleTypesList[eventIdx] = LPROF_SAMPLE_TYPE_LIST | LPROF_SAMPLE_TYPE_EXTRA | PERF_SAMPLE_BRANCH_STACK;
         break;

      case BACKTRACE_MODE_OFF:
         sampleTypesList[eventIdx] = LPROF_SAMPLE_TYPE_LIST | LPROF_SAMPLE_TYPE_EXTRA;
         break;

      default:
         break;

      }

      // FOR THE OTHERS EVENT WE DON'T WANT THE CALLCHAIN AND CPU_ID
      for (eventIdx = 1; eventIdx < nbEvents; eventIdx++)
      {
         sampleTypesList[eventIdx] = LPROF_SAMPLE_TYPE_LIST;
      }
   }
   else // CUSTOM MODE
   {
      for (eventIdx = 0; eventIdx < nbEvents; eventIdx++)
      {
         sampleTypesList[eventIdx] = sampleTypes;
      }
   }
}

size_t read_sample_branch_stack (perf_event_desc_t* hw, sampleInfo_t** ptrSampleInfo)
{
   struct perf_branch_entry branch;
   uint64_t nbBranches, remainingBranches;
   int ret, idx = 0;

   sampleInfo_t* sampleInfo = NULL;
   sampleInfo = lc_malloc (sizeof(*sampleInfo));
   memset(sampleInfo,0,sizeof(*sampleInfo));

   ret = perf_read_buffer(hw, &nbBranches, sizeof(nbBranches));
   if (ret)
      errx(1, "[MAQAO] Cannot read nb branches");

   DBGMSGLVL (2 ,"\n\tBRANCH_STACK:%"PRIu64"\n", nbBranches);
   remainingBranches = nbBranches;
   sampleInfo->callChainAddress = lc_malloc ( nbBranches * sizeof( *sampleInfo->callChainAddress ) );
   sampleInfo->nbAddresses = (uint32_t) nbBranches;

   // FROM MOST RECENT TO LEAST RECENT TAKEN BRANCH
   while (remainingBranches--)
   {
      ret = perf_read_buffer(hw, &branch, sizeof(branch));
      if (ret)
         errx(1, "[MAQAO] Cannot read branch stack entry");

      // TODO: CHECK IF IT'S A MISPREDICTED BRANCH ?
      sampleInfo->callChainAddress[idx] = branch.to;
      idx++;

      DBGMSGLVL ( 2, "\tFROM:0x%016"PRIx64" TO:0x%016"PRIx64" MISPRED:%c\n",
                  branch.from,
                  branch.to,
                  !(branch.mispred || branch.predicted) ? '-':
                  (branch.mispred ? 'Y' :'N'));
   }
   *ptrSampleInfo = sampleInfo;

   return ( ( nbBranches * sizeof(branch) ) + sizeof(nbBranches) );
}

void generate_walltime_uarch_files (const char* dir_name, int64_t walltime, int uarch)
{
   FILE *fp;

   // Print walltime to <process_dir_name>/walltime
   if ((fp = fopen_in_directory (dir_name, "walltime", "w")) != NULL) {
      fprintf (fp, "%"PRId64"", walltime);
      fclose (fp);
   }

   // Print uarch to <process_dir_name>/uarch
   if ((fp = fopen_in_directory (dir_name, "uarch", "w")) != NULL) {
      fprintf (fp, "%d", uarch);
      fclose (fp);
   }
}
