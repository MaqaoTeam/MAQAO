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

// CF prepare_sampling_display_shared.c

#ifndef __PREPARE_SAMPLING_DISPLAY_SHARED_H__
#define __PREPARE_SAMPLING_DISPLAY_SHARED_H__

#include <stdint.h> // uint64_t...
#include "libmcommon.h" // hashtable_t...
#include "avltree.h" // avlTree_t, SinfoFunc...
#include "binary_format.h" // lprof_loop_t

#define BIN_CATEGORY       0
#define MPI_CATEGORY       1
#define OMP_CATEGORY       2
#define MATH_CATEGORY      3
#define SYSTEM_CATEGORY    4
#define PTHREAD_CATEGORY   5
#define IO_CATEGORY        6
#define STRING_CATEGORY    7
#define MEMORY_CATEGORY    8
#define OTHERS_CATEGORY    9
#define TOTAL_CATEGORY     10
#define NB_CATEGORIES      11

#define LIBC_IO_CATEGORY      1
#define LIBC_MEMORY_CATEGORY  2
#define LIBC_PTHREAD_CATEGORY 3
#define LIBC_STRING_CATEGORY  4
#define LIBC_OTHER_CATEGORY   5
#define LIBC_UNKNOWN_FCT      6
#define LIBC_TOTAL_CATEGORY   7
#define LIBC_NB_CATEGORIES    8

#define SAMPLE_TYPE_BINARY          1
#define SAMPLE_TYPE_LIBRARY         2
#define SAMPLE_TYPE_SYSTEM          3
#define CALLCHAIN_FILTER_IGNORE_ALL 4

// From a lprof function (lprof_fct_t), creates a AVLtree function (SinfoFunc)
SinfoFunc* function_to_infoFunc (lprof_fct_t* myFct,unsigned rangeIdx, unsigned nbPids);

// From a lprof loop (lprof_loop_t), creates a AVLtree loop (SinfoLoop)
SinfoLoop* lprof_loop_to_infoLoop (lprof_loop_t* myLoop,unsigned rangeIdx,unsigned nbPids);

// From a sample IP (instruction pointer), look for the matching function or loop in executable
void* search_addr_in_binary (uint64_t* address, avlTree_t* binTree, int displayType,unsigned* category, uint32_t* nbOccurrences, uint64_t binary_offset);

// From a sample IP (instruction pointer), look for the matching function or loop in libraries
void* search_addr_in_libraries_new (unsigned int processIdx, uint64_t* address, avlTree_t** libraryTrees, lprof_libraries_info_t* libsInfo, unsigned nbLibraries, int displayType, unsigned* category, hashtable_t* libcFunctionToCategory, unsigned* libcCategory, uint32_t* nbOccurrences);

// According to library name (and, for libc [libdl,libc,ld], function name), returns a library category
unsigned select_category (char* libraryName, char* fct_name, hashtable_t* libcFunctionToCategory);

// Initializes an AVL tree function (SinfoFunc), i.e. allocates arrays used to save HW events and callchains
void init_sinfo_func_hwc (SinfoFunc* infoFunc, unsigned pidIdx, unsigned nbThreads, unsigned nbHwc);

// Initializes an AVL tree loop (SinfoLoop), i.e. allocates arrays used to save HW events and callchains
void init_sinfo_loop_hwc (SinfoLoop* infoLoop, unsigned pidIdx, unsigned nbThreads, unsigned nbHwc);

// From an AVL tree function (SinfoFunc), returns a ready-to-print string (displaying name, coverage...)
char* create_fct_line ( SinfoFunc* myFct,
                        unsigned pidIdx,
                        unsigned threadIdx,
                        uint32_t nbHwc,
                        unsigned sampling_period,
                        float cpu_freq,
                        float ref_freq,
                        uint64_t* totalHwcEvents,
                        int showSampleValue,
                        int extendedMode,
                        lprof_libraries_info_t libsInfo,
                        char* binaryName);

// Idem create_fct_line with custom HW events
char* create_fct_line_custom ( SinfoFunc* myFct,
                               unsigned pidIdx,
                               unsigned threadIdx,
                               uint32_t nbHwc,
                               uint64_t* totalHwcEvents,
                               int showSampleValue,
                               lprof_libraries_info_t libsInfo,
                               char* binaryName);

// Similar to create_fct_line
char* create_loop_line ( SinfoLoop* myLoop,
                         unsigned pidIdx,
                         unsigned threadIdx,
                         uint32_t nbHwc,
                         unsigned sampling_period,
                         float cpu_freq,
                         float ref_freq,
                         uint64_t* totalHwcEvents,
                         int showSampleValue,
                         int extendedMode,
                         lprof_libraries_info_t libsInfo,
                         char* binaryName);

// Similar to create_fct_line_custom
char* create_loop_line_custom (  SinfoLoop* myLoop,
                                 unsigned pidIdx,
                                 unsigned threadIdx,
                                 uint32_t nbHwc,
                                 uint64_t* totalHwcEvents,
                                 int showSampleValue,
                                 lprof_libraries_info_t libsInfo,
                                 char* binaryName);

// Return executable offset (address of first instruction in virtual memory)
uint64_t get_exe_offset (const char *path, const char *header_version);

#endif // __PREPARE_SAMPLING_DISPLAY_SHARED_H__
