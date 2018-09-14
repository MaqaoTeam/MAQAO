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

#ifndef __DEPRECATED_SHARED_H__
#define __DEPRECATED_SHARED_H__

#include "perf_util.h"

#define DEFAULT_THRESHOLD 2000003
#define DEFAULT_TOTAL_EVENTS 3

//ARCH VALUE FOR LIBPFM
#define KNC_ARCH 100
#define IVY_ARCH 74
#define SANDY_ARCH 71
#define SANDY_E3_ARCH 68

//KNC LIST
#define DEFAULT_EVENTS_LIST_KNC "CPU_CLK_UNHALTED,INSTRUCTIONS_EXECUTED"

// KNL
//#define DEFAULT_EVENTS_LIST_KNL "CPU_CLK_UNHALTED:REF_TSC,INST_RETIRED:ANY"
#define DEFAULT_EVENTS_LIST_KNL "CPU_CLK_UNHALTED_KNL:REF,INST_RETIRED:ANY_P"

// SKYLAKE
#define DEFAULT_EVENTS_LIST_SKYLAKE "UNHALTED_REFERENCE_CYCLES_SKL,INST_RETIRED,L1D:REPLACEMENT,L2_LINES_IN"

// BROADWELL
#define DEFAULT_EVENTS_LIST_BROADWELL "CPU_CLK_THREAD_UNHALTED:REF_XCLK,INST_RETIRED,L1D:REPLACEMENT,L2_LINES_IN"

//HASWELL
//#define DEFAULT_EVENTS_LIST_IVY "INST_RETIRED,CPU_CLK_UNHALTED:REF_P,UNHALTED_REFERENCE_CYCLES,CPU_CLK_UNHALTED:THREAD_P"
#define DEFAULT_EVENTS_LIST_HASWELL "CPU_CLK_THREAD_UNHALTED:REF_XCLK,INST_RETIRED,L1D:REPLACEMENT,L2_LINES_IN"

//IVY BRIDGE
//#define DEFAULT_EVENTS_LIST_IVY "INST_RETIRED,CPU_CLK_UNHALTED:REF_P,UNHALTED_REFERENCE_CYCLES,CPU_CLK_UNHALTED:THREAD_P"
#define DEFAULT_EVENTS_LIST_IVY "CPU_CLK_UNHALTED:REF_P,INST_RETIRED,L1D:REPLACEMENT,L2_LINES_IN,ARITH:FPU_DIV"

//SANDY BRIDGE LIST
//#define DEFAULT_EVENTS_LIST_SANDY "CPU_CLK_UNHALTED:REF_P,INST_RETIRED"
#define DEFAULT_EVENTS_LIST_SANDY "CPU_CLK_UNHALTED:REF_P,INST_RETIRED,L1D:REPLACEMENT,L2_LINES_IN:ANY,ARITH:FPU_DIV"
//#define DEFAULT_EVENTS_LIST_SANDY "CPU_CLK_UNHALTED:REF_P,INST_RETIRED,L1D:REPLACEMENT,L2_LINES_IN:ANY,RESOURCE_STALLS,ARITH:FPU_DIV,UNC_M_CAS_COUNT_0:RD,UNC_M_CAS_COUNT_0:WR,UNC_M_CAS_COUNT_1:RD,UNC_M_CAS_COUNT_1:WR"

//NEHALEM + WESTMERE
#define DEFAULT_EVENTS_LIST_NEHALEM "CPU_CLK_UNHALTED:REF_P,INST_RETIRED,L1D:REPL,L2_LINES_IN:ANY,ARITH:CYCLES_DIV_BUSY"

//CORE2_45 AND CORE2_65
#define DEFAULT_EVENTS_LIST_CORE2 "UNHALTED_REFERENCE_CYCLES,INSTRUCTION_RETIRED"

//CORTEX_A57
#define DEFAULT_EVENTS_LIST_CORTEXA57 "HW_CPU_CYCLES,INSTR_EXECUTED"


#define MISS_EVENTS_LIST "CPU_CLK_UNHALTED:REF_P,INST_RETIRED,L2_RQSTS:CODE_RD_MISS,L2_RQSTS:PF_MISS,L2_RQSTS:RFO_MISS,LLC_MISSES"
#define DTLB_EVENTS_LIST "CPU_CLK_UNHALTED:REF_P,INST_RETIRED,DTLB_LOAD_MISSES:CAUSES_A_WALK,DTLB_STORE_MISSES:CAUSES_A_WALK,HW_PRE_REQ:L1D_MISS"

//THRESHOLD VALUE FOR HWC
#define XSMALL_SAMPLING_PERIOD 250003
#define SMALL_SAMPLING_PERIOD 500003
#define MEDIUM_SAMPLING_PERIOD 2000003
#define DEFAULT_SAMPLING_PERIOD 2000003
#define BIG_SAMPLING_PERIOD 20000003

//THRESHOLD VALUE FOR TIMERS
#define TIMER_XSMALL_SAMPLING_PERIOD    2 // 500 Hz
#define TIMER_SMALL_SAMPLING_PERIOD     5 // 200 Hz
#define TIMER_MEDIUM_SAMPLING_PERIOD   10 // 100 Hz
#define TIMER_DEFAULT_SAMPLING_PERIOD  10 // 100 Hz
#define TIMER_BIG_SAMPLING_PERIOD     100 //  10 Hz


#define DEFAULT_MMAP_PAGES 1
#define DEFAULT_MAX_FD 2
#define FILE_NAME "instrument_sampling.rslt"

#define MAX_COUNTER_LENGTH 128
#define PIPE_BUF_SIZE 512

#define MAX_BUF_SIZE 1024

//Default value in the /sys/bus/event_source/devices/uncore_imc_*/type file
#define UNCORE_IMC_0 17
#define UNCORE_IMC_1 18
#define UNCORE_IMC_2 19
#define UNCORE_IMC_3 20

#define PATH_IMC_0_TYPE "/sys/bus/event_source/devices/uncore_imc_0/type"
#define PATH_IMC_1_TYPE "/sys/bus/event_source/devices/uncore_imc_1/type"
#define PATH_IMC_2_TYPE "/sys/bus/event_source/devices/uncore_imc_2/type"
#define PATH_IMC_3_TYPE "/sys/bus/event_source/devices/uncore_imc_3/type"

#define INTEL_SERVER_CPU   1
#define INTEL_DESKTOP_CPU  2

#define LPROF_SAMPLE_TYPE_LIST PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_ID
//#define LPROF_SAMPLE_TYPE_EXTRA PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_CPU
#define LPROF_SAMPLE_TYPE_EXTRA PERF_SAMPLE_CPU
//#define LPROF_SAMPLE_TYPE_EXTRA PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_BRANCH_STACK| PERF_SAMPLE_CPU
//#define LPROF_SAMPLE_TYPE_EXTRA PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_BRANCH_STACK | PERF_SAMPLE_STACK_USER | PERF_SAMPLE_CPU


#define BACKTRACE_MODE_CALL 1
#define BACKTRACE_MODE_STACK 2
#define BACKTRACE_MODE_BRANCH 3
#define BACKTRACE_MODE_OFF 4

typedef struct sampleInfo_s
{
   uint32_t nbAddresses;
   uint64_t* callChainAddress;
} sampleInfo_t;

typedef struct
{
   int pid;
   char hostname[1024];
} returnInfo_t;

int get_uarch (int *arch);

void utils_print_struct_event_attr(struct perf_event_attr* event);

/* Converts HW events symbolic name to raw code */
int maqao_get_os_event_encoding(    int arch,
                                    int uarch,
                                    perf_event_desc_t* event,
                                    const char* event_name,
                                    int raw_code_id, int64_t *raw_code,
                                    char kill_on_failure
                                    );

char *getHwcList (int arch, int uarch, int verbosity, char *uarch_string);

void set_sample_type (uint64_t sampleTypes, int backtrace_mode, int nbEvents, uint64_t* sampleTypesList);

size_t read_sample_branch_stack (perf_event_desc_t* hw, sampleInfo_t** ptrSampleInfo);
void generate_walltime_uarch_files (const char *dir_name, int64_t walltime, int uarch);

#endif // __DEPRECATED_SHARED_H__
