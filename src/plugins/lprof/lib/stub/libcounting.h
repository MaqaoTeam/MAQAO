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

/*
 * CF libcounting.c
 * Defines structures used by the libcounting library, in particular SinstruInfo.
 */

#ifndef __LIBCOUNTING_H__
#define __LIBCOUNTING_H__

#include <stdlib.h>
#include <inttypes.h>
#include "libmcommon.h"

enum
{
   TYPE_RAW,
   TYPE_DATA_WRITES,
   TYPE_DATA_READS,
   TYPE_NB_MAX
};

enum
{
   BIT_EVENT_SELECT=0,
   BIT_UNIT_MASK = 8,
   BIT_USR_FLAG=16,
   BIT_OS_FLAG=17,
   BIT_EDGE_DETECT_FLAG=18,
   BIT_PIN_CONTROL_FLAG=19,
   BIT_INT_FLAG=20,
   BIT_ANY_FLAG=21,
   BIT_ENABLE_COUNTERS_FLAG=22,
   BIT_INV_FLAG=23,
   BIT_COUNTER_MASK=24
};


enum
{
   IDX_EVENT_SELECT,
   IDX_UNIT_MASK,
   IDX_USR_FLAG,
   IDX_OS_FLAG,
   IDX_EDGE_DETECT_FLAG,
   IDX_PIN_CONTROL_FLAG,
   IDX_INT_FLAG,
   IDX_ANY_FLAG,
   IDX_ENABLE_COUNTERS_FLAG,
   IDX_INV_FLAG,
   IDX_COUNTER_MASK,
   IDX_MAX_FLAG
};


typedef struct counterFlags
{
   int64_t value[IDX_MAX_FLAG];
}ScounterFlags;

typedef struct counterInfo
{
   char* name;                  // HWC NAME
   int id;                      // CALL SITE ID
   uint64_t value;              // HWC VALUE
}ScounterInfo;

typedef struct instruParameters
{
   queue_t** collectedInfo;            // STORE ALL DATA IN TIMELINE MODE
   uint64_t** accumulateSamples;       // STORE ALL DATA IN ACCUMULATE MODE
   unsigned int nbCallSites;           // NUMBER OF CALL SITE
   unsigned int nbCounters;            // NUMBER OF COUNTERS
   unsigned int nbRawCounters;         // NUMBER OF RAW COUNTERS (ALL COUNTERS EXCEPT DRAM COUNTERS : ONLY COUNTERS USING PERF_EVENT_OPEN AND READ FCT)
   char** counterNames;                // ARRAY CONTAINING HWC NAMES
   ScounterFlags* countersFlags;       // ARRAY CONTAINING THE FLAGS FOR ALL THE COUNTER
   uint32_t* counterPerfTypes;         // ARRAY CONTAINING HWC TYPES VALUE FOR PERF_EVENT_OPEN (PERF_TYPE_RAW ETC.)
   uint32_t* counterLibTypes;          // ARRAY CONTAINING HWC TYPES VALUE FOR LIBCOUNTING (cf enum counting_types)
   int32_t* counterCore;               // ARRAY CONTAINING CORE INFO OF EACH GROUP
   int32_t* counterPid;                // ARRAY CONTAINING PID INFO OF EACH GROUP
   int* pfmFds;                        // PERF FILE DESCRIPTOR (cf. perf_event_open)
   int nbGroups;                       // NB OF GROUP FILES DESCRIPTOR USED WITH perf_event_open
   unsigned int* nbCountersPerGroup;   // NB OF HWC PER GROUP
   int* hwcIdxToGroup;                 // HWC IDX TO GROUP ID
   int* groupIdxToPfmFds;              // GROUP IDX TO PFM FILE DESCRIPTOR
   ScounterInfo** counterInfo;         // GET HWC INFO FOR EACH CALL SITE
   ssize_t* bufferSize;                // ARRAY CONTAINING BUFFER SIZE INFO OF EACH HWC GROUP (perf hwc group).
   uint64_t** buffer;                  // ARRAY CONTAINING THE BUFFER OF EACH HWC GROUP (perf hwc group).
}SinstruInfo;


//INIT FUNCTIONS
unsigned int counting_add_hw_counters(char* hwcList,int core, int pid);
void counting_start_counters(unsigned int nbCallSites);

//START & STOP FUNCTIONS
void counting_start_counting (unsigned int callSiteId);
void counting_stop_counting (unsigned int callSiteId);
void counting_stop_counting_and_accumulate (unsigned int callSiteId);

//DUMP FUNCTIONS
void counting_dump_file (char* fileName);
void counting_dump_file_by_line (char* fileName);
void counting_dump_file_accumulate (char* fileName);
char** counting_dump (void);
char*** counting_dump_accumulate (void);


uint64_t** get_counter_info_accumulate ();
ScounterInfo*** get_counter_info ();
unsigned int get_nb_callsites ();
unsigned int get_nb_counters ();
unsigned int* get_nb_counters_per_group ();
int get_nb_groups ();

extern void counting_dump_file_accumulate_and_reset (FILE*, int, int, int, int, char** , char**, char**);
#endif
