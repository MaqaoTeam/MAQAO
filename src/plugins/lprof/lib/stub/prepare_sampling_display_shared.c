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
 * Defines functions shared between prepare_sampling_display.c and deprecated.c
 * Except get_exe_offset, all this code was never refactored: after removing deprecated code
 * still calling following functions, some simplifications could happen there...
 */

#include <stdint.h> // uint64_t...
#include <libgen.h> // basename

#include "libmcommon.h" // hashtable_t...
#include "utils.h" // fopen_in_directory...
#include "avltree.h" // avlTree_t, SinfoFunc...
#include "binary_format.h" // lprof_loop_t
#include "prepare_sampling_display_shared.h" // Categories constants...

// From a lprof function (lprof_fct_t), creates a AVLtree function (SinfoFunc)
SinfoFunc* function_to_infoFunc (lprof_fct_t* myFct,unsigned rangeIdx, unsigned nbPids)
{
   assert(myFct != NULL);
   SinfoFunc* myInfoFunc       = lc_malloc (sizeof(*myInfoFunc));
   myInfoFunc->name            = myFct->name;
   myInfoFunc->start           = myFct->startAddress[rangeIdx];
   myInfoFunc->stop            = myFct->stopAddress[rangeIdx];
   myInfoFunc->src_file        = myFct->srcFile;
   myInfoFunc->src_line        = myFct->srcLine;
   //myInfoFunc->hwcInfo         = NULL;
   myInfoFunc->hwcInfo         = lc_calloc (nbPids, sizeof(*myInfoFunc->hwcInfo));
   myInfoFunc->callChainsInfo  = lc_calloc (nbPids, sizeof(*myInfoFunc->callChainsInfo));
   myInfoFunc->totalCallChains = lc_calloc (nbPids, sizeof(*myInfoFunc->totalCallChains));
   myInfoFunc->libraryIdx      = -1;
   return myInfoFunc;
}

static char* loop_level_enum_to_char (unsigned loopLevel)
{
   char* strLoopLevel = NULL;
   if (loopLevel == SINGLE_LOOP)
      strLoopLevel = lc_strdup ("Single");
   if (loopLevel == INNERMOST_LOOP)
      strLoopLevel = lc_strdup ("Innermost");
   if (loopLevel == OUTERMOST_LOOP)
      strLoopLevel = lc_strdup ("Outermost");
   if (loopLevel == INBETWEEN_LOOP)
      strLoopLevel = lc_strdup ("InBetween");

   return strLoopLevel;
}

// From a lprof loop (lprof_loop_t), creates a AVLtree loop (SinfoLoop)
SinfoLoop* lprof_loop_to_infoLoop (lprof_loop_t* myLoop,unsigned rangeIdx,unsigned nbPids)
{
   assert(myLoop != NULL);
   SinfoLoop* myInfoLoop       = lc_malloc (sizeof(*myInfoLoop));
   myInfoLoop->loop_id         = myLoop->id;
   myInfoLoop->start           = myLoop->startAddress[rangeIdx];
   myInfoLoop->stop            = myLoop->stopAddress[rangeIdx];
   myInfoLoop->src_file        = myLoop->srcFile;
   myInfoLoop->func_name       = myLoop->srcFunctionName;
   myInfoLoop->src_line_start  = myLoop->srcStartLine;
   myInfoLoop->src_line_end    = myLoop->srcStopLine;
   myInfoLoop->level           = loop_level_enum_to_char(myLoop->level);
   //myInfoLoop->hwcInfo         = NULL;
   myInfoLoop->hwcInfo         = lc_calloc (nbPids, sizeof(*myInfoLoop->hwcInfo));
   myInfoLoop->libraryIdx      = -1;
   //fprintf(stderr,"INSERT LOOP #%d\n",myInfoLoop->loop_id);
   return myInfoLoop;
}

/* From a sample IP (instruction pointer), look for the matching function or loop in executable
 * For efficient lookup, executable functions/loops are structured as an AVL tree */
void* search_addr_in_binary (uint64_t* address, avlTree_t* binTree, int displayType,unsigned* category, uint32_t* nbOccurrences, uint64_t binary_offset)
{
   avlTree_t* node = NULL;
   int found = 0;
   uint64_t addrToSearch = *address - binary_offset;

   switch (displayType)
   {
   case PERF_FUNC:
      node = search_address(addrToSearch, binTree, PERF_FUNC);
      if (node != NULL)
      {
         found = 1;
         SinfoFunc* fctInfo = node->value;
         if (category != NULL)
         {
            // KIND OF UGLY :( -- TO DETECT GOMP FUNCTION INSIDE THE BINARY
            //if ( (strstr(fctInfo->name,"omp_fn") != NULL) ||
            //     (strstr(fctInfo->name,"#omp_")!= NULL))
            //   category[OMP_CATEGORY] += *nbOccurrences;
            //else
            //   category[BIN_CATEGORY] += *nbOccurrences;


            if ( fctInfo->libraryIdx != -2 )
               category[BIN_CATEGORY] += *nbOccurrences;
            else // SYSTEM CALL
               category[SYSTEM_CATEGORY] += *nbOccurrences;
            category[TOTAL_CATEGORY] += *nbOccurrences;
         }
         //fprintf(stderr,"[%s]\n",fctInfo->name);
         return fctInfo;
      }
      break;
   case PERF_LOOP:
      node = search_address(addrToSearch, binTree, PERF_LOOP);
      if (node != NULL)
      {
         found = 1;
         SinfoLoop* loopInfo = node->value;
         //fprintf(stderr,"[%d]",loopInfo->loop_id);
         //fprintf(stderr,"- %s <%"PRIx64"-%"PRIx64">\n",loopInfo->func_name,loopInfo->start,loopInfo->stop);
         return loopInfo;
      }
      break;
   default:
      break;
   }

   //FIXME: found not used
   (void)found;

   return NULL;
}

/* From a sample IP (instruction pointer), look for the matching function or loop in libraries
 * For efficient lookup, executable functions/loops are structured as an AVL tree */
void* search_addr_in_libraries_new (unsigned int processIdx, uint64_t* address, avlTree_t** libraryTrees, lprof_libraries_info_t* libsInfo, unsigned nbLibraries, int displayType, unsigned* category, hashtable_t* libcFunctionToCategory, unsigned* libcCategory, uint32_t* nbOccurrences)
{
   unsigned libIdx         = 0;
   uint64_t addrToSearch   = *address;
   avlTree_t* ptrTree      = NULL;
   avlTree_t* node         = NULL;
   unsigned categoryIdx, libcCategoryIdx;
   //uint64_t processIdx = 0;

   if ( addrToSearch > 0x3000000)
   {
      //SEARCH IN LIBRARIES
      // - HANDLE LIBC ADDRESS
      // - SEARCH AND SCAN THE TARGET LIBRARY
      for (libIdx=0; libIdx < nbLibraries; libIdx++)
      {
         //fprintf(stderr,"PID[%u] :%s %#"PRIx64" -> %#"PRIx64"\n", processIdx,
         //                                                       libsInfo->libraries[libIdx].name,
         //                                                       libsInfo->libraries[libIdx].startMapAddress[processIdx],
         //                                                       libsInfo->libraries[libIdx].stopMapAddress[processIdx]);
         if ( addrToSearch >= libsInfo->libraries[libIdx].startMapAddress[processIdx] &&
              addrToSearch <= libsInfo->libraries[libIdx].stopMapAddress[processIdx]
            )
         {
            //print_library(&libsInfo->libraries[libIdx]);
            // SUBSTRACTING THE OFFSET
            //    WITH SPECIFIC LIB LIKE LIBC, LIBLD ...
            //    WE DON'T NEED TO SUBSTRACT THE OFFSET
            //    THIS KINDS OF LIBS ARE ALWAYS MAPPED BETWEEN 0x3000000000 AND 0x4000000000
            if (addrToSearch > 0x3000000000 && addrToSearch < 0x4000000000)
            {
               // SPECIFIC LIB LIKE LIBC, LIBLD ...
               // DO NOT SUBSTRACT
            }
            else
            {
               addrToSearch = addrToSearch - libsInfo->libraries[libIdx].startMapAddress[processIdx];
            }

            ptrTree = libraryTrees[libIdx];
            if (ptrTree != NULL)
            {
               switch (displayType)
               {
               case PERF_FUNC:
                  node = search_address(addrToSearch, ptrTree, PERF_FUNC);
                  if (node != NULL)
                  {
                     SinfoFunc* fctInfo = node->value;
                     fctInfo->libraryIdx = libIdx;

                     //SELECT CATEGORY
                     if(category != NULL)
                     {
                        categoryIdx = select_category(libsInfo->libraries[libIdx].name,fctInfo->name,libcFunctionToCategory);
                        category[categoryIdx] += *nbOccurrences;
                        category[TOTAL_CATEGORY] += *nbOccurrences;
                     }

                     if ( libcFunctionToCategory != NULL &&
                          (strstr(libsInfo->libraries[libIdx].name,"libc.") != NULL ||
                           strstr(libsInfo->libraries[libIdx].name,"libc-") != NULL)
                        )
                     {
                        //DIRTY CAST TO REMOVE A WARNING :(
                        if ((libcCategoryIdx = (unsigned)((int64_t) hashtable_lookup(libcFunctionToCategory, (void*)fctInfo->name))) != 0)
                           libcCategory[libcCategoryIdx] += 1;
                        else
                           libcCategory[LIBC_UNKNOWN_FCT] += 1;
                        libcCategory[LIBC_TOTAL_CATEGORY] += 1;
                     }

                     return fctInfo;
                  }
                  break;
               case PERF_LOOP:
                  node = search_address(addrToSearch, ptrTree, PERF_LOOP);
                  if (node != NULL)
                  {
                     SinfoLoop* loopInfo = node->value;
                     loopInfo->libraryIdx = libIdx;
                     return loopInfo;
                  }
                  break;
               default:
                  break;
               }
            }
            break;
         }

      }
   }
   return NULL;
}

/* According to library name (and, for libc [libdl,libc,ld], function name), returns a library category
 * Example: libgomp => OMP_CATEGORY */
unsigned select_category (char* libraryName, char* fct_name, hashtable_t* libcFunctionToCategory)
{
   unsigned libcCategoryIdx = -1;
   if ( strstr(libraryName,"libmpi")               != NULL ||
        strstr(libraryName,"libmpi_usempi.so")     != NULL ||
        strstr(libraryName,"libopen-rte.so")       != NULL ||
        strstr(libraryName,"libmca_")              != NULL ||
        strstr(libraryName,"mca_")                 != NULL ||
        strstr(libraryName,"libpami.so")           != NULL || // MPI IBM
        strstr(libraryName,"libpsm_infinipath.so") != NULL ||
        strstr(libraryName,"libopen-pal.so")       != NULL
      )
   {
      return MPI_CATEGORY;
   }

   if ( strstr(libraryName,"libiomp5.") != NULL ||
        strstr(libraryName,"libcraymp") != NULL ||
        strstr(libraryName,"libgomp") != NULL
      )
   {
      return OMP_CATEGORY;
   }

   if ( strstr(libraryName,"libmkl_")        != NULL ||
        strstr(libraryName,"libm.")          != NULL ||
        strstr(libraryName,"libm-")          != NULL ||
        strstr(libraryName,"libcraymath")    != NULL ||
        strstr(libraryName,"libblas")        != NULL ||
        strstr(libraryName,"libimf.")        != NULL ||
        strstr(libraryName,"libquadmath.")   != NULL ||
        strstr(libraryName,"libfft")         != NULL
      )
   {
      return MATH_CATEGORY;
   }

   if (strstr(libraryName,"libtcmalloc_minimal") != NULL)
   {
      return MEMORY_CATEGORY;
   }

   if ( strstr(libraryName,"libdl")       != NULL ||
        strstr(libraryName,"libc-")       != NULL ||
        strstr(libraryName,"libc.")       != NULL ||
        strstr(libraryName,"ld-")         != NULL ||
        strstr(libraryName,"ld-linux.")   != NULL
      )
   {
      if ( libcFunctionToCategory != NULL)
      {
         //DIRTY CAST TO REMOVE A WARNING :(
         if ((libcCategoryIdx = (unsigned)((int64_t) hashtable_lookup(libcFunctionToCategory, (void*)fct_name))) != 0)
         {
            switch (libcCategoryIdx)
            {
            case LIBC_IO_CATEGORY:
               return IO_CATEGORY;
               break;
            case LIBC_STRING_CATEGORY:
               return STRING_CATEGORY;
               break;
            case LIBC_MEMORY_CATEGORY:
               return MEMORY_CATEGORY;
               break;
            default:
               return SYSTEM_CATEGORY;
            }
         }
      }
      return SYSTEM_CATEGORY;
   }

   if ( strstr(libraryName,"libpthread-") != NULL )
   {
      return PTHREAD_CATEGORY;
   }


   return OTHERS_CATEGORY;
}

// Initializes an AVL tree function (SinfoFunc), i.e. allocates arrays used to save HW events and callchains
void init_sinfo_func_hwc (SinfoFunc* infoFunc, unsigned pidIdx, unsigned nbThreads, unsigned nbHwc)
{
   unsigned i;

   if (infoFunc->hwcInfo[pidIdx] == NULL)
   {
      infoFunc->hwcInfo[pidIdx] = lc_malloc (nbThreads * sizeof(*infoFunc->hwcInfo[pidIdx]));
      infoFunc->callChainsInfo[pidIdx] = lc_calloc (nbThreads,  sizeof(*infoFunc->callChainsInfo[pidIdx]));
      infoFunc->totalCallChains[pidIdx] = lc_calloc (nbThreads, sizeof(*infoFunc->totalCallChains[pidIdx]));
      for ( i = 0; i < nbThreads; i++)
      {
         size_t sizeToAlloc = nbHwc * sizeof(*infoFunc->hwcInfo[pidIdx][i]);
         infoFunc->hwcInfo[pidIdx][i] = lc_calloc (nbHwc,sizeToAlloc);
         infoFunc->callChainsInfo[pidIdx][i] = NULL;
      }
   }
}

// Initializes an AVL tree loop (SinfoLoop), i.e. allocates arrays used to save HW events and callchains
void init_sinfo_loop_hwc (SinfoLoop* infoLoop, unsigned pidIdx, unsigned nbThreads, unsigned nbHwc)
{
   unsigned i;

   if (infoLoop->hwcInfo[pidIdx] == NULL)
   {
      infoLoop->hwcInfo[pidIdx] = lc_malloc (nbThreads * sizeof(*infoLoop->hwcInfo[pidIdx]));
      for ( i = 0; i < nbThreads; i++)
      {
         size_t sizeToAlloc = nbHwc * sizeof(*infoLoop->hwcInfo[pidIdx][i]);
         infoLoop->hwcInfo[pidIdx][i] = lc_malloc (nbHwc * sizeof(*infoLoop->hwcInfo[pidIdx][i]));
         memset (infoLoop->hwcInfo[pidIdx][i], 0, sizeToAlloc);
      }
   }
}

/* From an AVL tree function (SinfoFunc), returns a ready-to-print string (displaying name, coverage...)
 * Should be removed... (let to display) and at least factorized with create_fct_* and create_loop_* */
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
                        char* binaryName)
{
   char* line = NULL;
   char* moduleName         = NULL;
   float cyclesEvents       = myFct->hwcInfo[pidIdx][threadIdx][0];
   float instrRetiredEvents = myFct->hwcInfo[pidIdx][threadIdx][1];
   float totalCyclesEvents  = totalHwcEvents[0];
   float timeInPercent     = 0.0;
   float timeInSec         = (cyclesEvents * sampling_period) / ref_freq;
   float cpiRatio          = 0.0;
   //MAXIMUM LENGTH : 75 CHARACTERS
   char* fctName        = strdup(myFct->name);
   char* basenameSrcFile   = basename(myFct->src_file);

   if (instrRetiredEvents != 0)
      cpiRatio = cyclesEvents * (cpu_freq / ref_freq) / instrRetiredEvents;

   if (totalCyclesEvents != 0)
      timeInPercent     = cyclesEvents * 100 / totalCyclesEvents;

   float l1dEvents = 0;
   float totalL1dEvents = 0;
   float l2Events = 0;
   float totalL2Events = 0;
   // EVENT ONLY FOR NEHALEM / SANDY /IVY / HASWELL UARCH
   if ( nbHwc > 2 )
   {
      l1dEvents      = myFct->hwcInfo[pidIdx][threadIdx][2];
      totalL1dEvents = totalHwcEvents[2];
      l2Events       = myFct->hwcInfo[pidIdx][threadIdx][3];
      totalL2Events  = totalHwcEvents[3];
   }

   float arithdEvents = 0;
   float totalArithEvents = 0;
   // EVENT ONLY FOR NEHALEM / SANDY /IVY UARCH
   if ( nbHwc == 5 )
   {
      arithdEvents     = myFct->hwcInfo[pidIdx][threadIdx][4];
      totalArithEvents = totalHwcEvents[4];
   }

   if (extendedMode)
   {
      if (totalL1dEvents != 0)
         l1dEvents = l1dEvents * 100 / totalL1dEvents;
      if (totalL2Events != 0)
         l2Events  = l2Events * 100 / totalL2Events;
      if (totalArithEvents != 0)
         arithdEvents = arithdEvents * 100 / totalArithEvents;
   }

   if (timeInPercent == 0)
      return NULL;

   if (myFct->libraryIdx > -1)
      moduleName  = libsInfo.libraries[myFct->libraryIdx].name;
   else if (myFct->libraryIdx == -2)
      moduleName = strdup("SYSTEM CALL");
   else
      moduleName = binaryName;

   line = lc_calloc (strlen(fctName)+1024,sizeof(*line));

   //DEBUG INFO
   if (strcmp(basenameSrcFile,".") != 0)
      sprintf(line,"%s;%s;%s:%d",
              fctName,
              basename(moduleName),
	      basenameSrcFile,
              myFct->src_line);
   else
      sprintf(line,"%s;%s;-1",
              fctName,
              basename(moduleName));

   if (showSampleValue)
   {
      if (extendedMode)
         sprintf(line,"%s;%.2f (%d);%.2f;%.2f;%.2f;%.2f;%.2f", line, timeInPercent, (int)cyclesEvents, timeInSec, cpiRatio, l1dEvents,l2Events,arithdEvents);
      else
         sprintf(line,"%s;%.2f (%d);%.2f;%.2f", line, timeInPercent, (int)cyclesEvents, timeInSec, cpiRatio);
   }
   else
   {
      if (extendedMode)
         sprintf(line,"%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f", line, timeInPercent, timeInSec, cpiRatio, l1dEvents,l2Events,arithdEvents);
      else
         sprintf(line,"%s;%.2f;%.2f;%.2f", line, timeInPercent, timeInSec, cpiRatio);
   }

   return line;
}

// Idem create_fct_line with custom HW events
char* create_fct_line_custom ( SinfoFunc* myFct,
                               unsigned pidIdx,
                               unsigned threadIdx,
                               uint32_t nbHwc,
                               uint64_t* totalHwcEvents,
                               int showSampleValue,
                               lprof_libraries_info_t libsInfo,
                               char* binaryName)
{
   char* line = NULL;

   unsigned i = 0;
   char* moduleName         = NULL;
   //MAXIMUM LENGTH : 75 CHARACTERS
   char* fctName        = strndup(myFct->name,75);
   char* basenameSrcFile   = basename(myFct->src_file);

   if (myFct->libraryIdx > -1)
      moduleName  = libsInfo.libraries[myFct->libraryIdx].name;
   else if (myFct->libraryIdx == -2)
      moduleName = strdup("SYSTEM CALL");
   else
      moduleName = binaryName;

   line = lc_calloc (1024,sizeof(*line));

   //DEBUG INFO
   if (strcmp(basenameSrcFile,".") != 0)
      sprintf(line, "%s;%s;%s:%d",
              fctName,
              basename(moduleName),
	      basenameSrcFile,
              myFct->src_line);
   else
      sprintf(line,"%s;%s;-1",
              fctName,
              basename(moduleName));

   if (showSampleValue)
   {
      for ( i=0; i < nbHwc; i++)
      {
         float value = (float)myFct->hwcInfo[pidIdx][threadIdx][i]/(float)totalHwcEvents[i] * 100;
         sprintf(line,"%s;%.2f (%d)",line,value,myFct->hwcInfo[pidIdx][threadIdx][i]);
      }
   }
   else
   {
      for ( i=0; i < nbHwc; i++)
      {
         float value = (float)myFct->hwcInfo[pidIdx][threadIdx][i]/(float)totalHwcEvents[i] * 100;
         sprintf(line,"%s;%.2f",line, value);
      }
   }

   return line;
}

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
                         char* binaryName)
{
   char* line = NULL;
   char* moduleName = NULL;
   float cyclesEvents       = myLoop->hwcInfo[pidIdx][threadIdx][0];
   float instrRetiredEvents = myLoop->hwcInfo[pidIdx][threadIdx][1];
   float totalCyclesEvents  = totalHwcEvents[0];
   float timeInPercent     = 0.0;
   float timeInSec         = (cyclesEvents * sampling_period) / ref_freq;
   float cpiRatio          = 0.0;

   float l1dEvents = 0;
   float totalL1dEvents = 0;
   float l2Events = 0;
   float totalL2Events = 0;

   // EVENT ONLY FOR NEHALEM / SANDY /IVY / HASWELL UARCH
   if ( nbHwc > 2 )
   {
      l1dEvents      = myLoop->hwcInfo[pidIdx][threadIdx][2];
      totalL1dEvents = totalHwcEvents[2];
      l2Events       = myLoop->hwcInfo[pidIdx][threadIdx][3];
      totalL2Events  = totalHwcEvents[3];
   }

   float arithdEvents = 0;
   float totalArithEvents = 0;
   // EVENT ONLY FOR NEHALEM / SANDY /IVY UARCH
   if ( nbHwc == 5 )
   {
      arithdEvents      = myLoop->hwcInfo[pidIdx][threadIdx][4];
      totalArithEvents  = totalHwcEvents[4];
   }

   //MAXIMUM LENGTH : 75 CHARACTERS
   char* fctName        = strdup(myLoop->func_name);

   if (instrRetiredEvents != 0)
      cpiRatio = cyclesEvents * (cpu_freq / ref_freq) / instrRetiredEvents;

   if (totalCyclesEvents != 0)
      timeInPercent     = cyclesEvents * 100 / totalCyclesEvents;

   if (extendedMode)
   {
      if (totalL1dEvents != 0)
         l1dEvents = l1dEvents * 100 / totalL1dEvents;
      if (totalL2Events != 0)
         l2Events  = l2Events * 100 / totalL2Events;
      if (totalArithEvents != 0)
         arithdEvents = arithdEvents * 100 / totalArithEvents;
   }




   if (timeInPercent == 0)
      return NULL;

   if (myLoop->libraryIdx > -1)
      moduleName  = libsInfo.libraries[myLoop->libraryIdx].name;
   else if (myLoop->libraryIdx == -2)
      moduleName = strdup("SYSTEM CALL");
   else
      moduleName = binaryName;

   line = lc_calloc (strlen(fctName)+1024,sizeof(*line));
   //DEBUG INFO
   if (myLoop->src_line_start != 0)
      sprintf (line, "%d;%s;%s;%s:%d-%d;%s",
               myLoop->loop_id,
               basename(moduleName),
               fctName,
               basename(myLoop->src_file),
               myLoop->src_line_start,
               myLoop->src_line_end,
               myLoop->level);

   else
      sprintf (line, "%d;%s;%s;-1;%s",
               myLoop->loop_id,
               basename(moduleName),
               fctName,
               myLoop->level);

   if (showSampleValue)
   {
      if (extendedMode)
         sprintf(line,"%s;%.2f (%d);%.2f;%.2f;%.2f;%.2f;%.2f", line, timeInPercent, (int)cyclesEvents, timeInSec, cpiRatio, l1dEvents,l2Events,arithdEvents);
      else
         sprintf(line,"%s;%.2f (%d);%.2f;%.2f", line, timeInPercent, (int)cyclesEvents, timeInSec, cpiRatio);
   }
   else
   {
      if (extendedMode)
         sprintf(line,"%s;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f", line, timeInPercent, timeInSec, cpiRatio, l1dEvents,l2Events,arithdEvents);
      else
         sprintf(line,"%s;%.2f;%.2f;%.2f", line, timeInPercent, timeInSec, cpiRatio);
   }
   //}
   ////NO DEBUG INFO
   //else
   //{
   //   sprintf(line,"%d;%s;%s", myLoop->loop_id, fctName, myLoop->level);
   //   if (showSampleValue)
   //      sprintf(line,"%s;%.2f (%d);%.2f;%.2f;%s",line, timeInPercent, (int)cyclesEvents, timeInSec, cpiRatio, basename(moduleName));
   //   else
   //      sprintf(line,"%s;%.2f;%.2f;%.2f;%s",line, timeInPercent, timeInSec, cpiRatio, basename(moduleName));
   //}


   return line;
}

// Similar to create_fct_line_custom
char* create_loop_line_custom (  SinfoLoop* myLoop,
                                 unsigned pidIdx,
                                 unsigned threadIdx,
                                 uint32_t nbHwc,
                                 uint64_t* totalHwcEvents,
                                 int showSampleValue,
                                 lprof_libraries_info_t libsInfo,
                                 char* binaryName)
{
   unsigned i;
   char* line = NULL;
   char* moduleName = NULL;
   //MAXIMUM LENGTH : 75 CHARACTERS
   char* fctName        = strndup(myLoop->func_name,75);


   if (myLoop->libraryIdx > -1)
      moduleName  = libsInfo.libraries[myLoop->libraryIdx].name;
   else if (myLoop->libraryIdx == -2)
      moduleName = strdup("SYSTEM CALL");
   else
      moduleName = binaryName;

   line = lc_calloc (1024,sizeof(*line));
   //DEBUG INFO
   if (myLoop->src_line_start != 0)
      sprintf (line, "%d;%s;%s;%s:%d-%d;%s",
               myLoop->loop_id,
               basename(moduleName),
               fctName,
               basename(myLoop->src_file),
               myLoop->src_line_start,
               myLoop->src_line_end,
               myLoop->level);

   else
      sprintf (line, "%d;%s;%s;-1;%s",
               myLoop->loop_id,
               basename(moduleName),
               fctName,
               myLoop->level);

   if (showSampleValue)
   {
      for ( i=0; i < nbHwc; i++)
      {
         float value = (float)myLoop->hwcInfo[pidIdx][threadIdx][i]/(float)totalHwcEvents[i] * 100;
         sprintf(line,"%s;%.2f (%d)",line,value,myLoop->hwcInfo[pidIdx][threadIdx][i]);
      }
   }
   else
   {
      for ( i=0; i < nbHwc; i++)
      {
         float value = (float)myLoop->hwcInfo[pidIdx][threadIdx][i]/(float)totalHwcEvents[i] * 100;
         sprintf(line,"%s;%.2f",line, value);
      }
   }

   return line;
}

// Return executable offset (address of first instruction in virtual memory)
uint64_t get_exe_offset (const char *path, const char *header_version)
{
   // Starting at version 2.1, binary_offset.lprof value takes into account executable type
   // (> 0 only if generated as dynamic-library). Before, was always > 0

   // With an old experiment directory, check on executable type was not done during metafiles generation
   // Considering generic case ("true" executable, no offseting required)
   float hd_ver; sscanf (header_version, "%f", &hd_ver);
   if (hd_ver < 2.1f) {
      WRNMSG ("metadata collected with lprof %s (running: %s)\n",
              header_version, MAQAO_LPROF_VERSION);
      WRNMSG ("Functions/loops will incorrectly be retrieved from samples ");
      WRNMSG ("if application executable was generated as a dynamic library.\n");
      WRNMSG ("In that case, please rerun.\n");
      return 0;
   }

   FILE *fp = fopen_in_directory (path, "binary_offset.lprof", "r");
   if (!fp) {
      DBGMSG0 ("Cannot load executable offset\n");
      return 0;
   }

   uint64_t exe_offset = 0;
   if (fscanf (fp, "%"PRIu64"", &exe_offset) != 1) {
      DBG(fprintf (stderr, "Failed to get executable address offset\n"); \
          perror ("fscanf");)
   }
   fclose (fp);

   return exe_offset;
}
