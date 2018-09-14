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
 * This file defines data structures and functions to handle the (binary) file format
 * used to dump/load to/from disk samples (mostly thread ID, IP, event),
 * executable and libraries data (mostly address ranges, function name and loop ID).
 * data are written at "collect" step and read at "display" step.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <inttypes.h>

#include "binary_format.h"
#include "libmcommon.h"
#include "perf_util.h"


char * serializedStr = NULL;
uint64_t sizeSerializedStr = 0;
hashtable_t* stringToOffset = NULL;


long write_lprof_header (FILE* file)
{
   assert(file != NULL);

   long savedFilePosition;

   uint64_t binInfoHdrOffset    = 0x400600; // FAKE ADDRESS
   uint64_t libInfoHdrOffset    = 0x400800; // FAKE ADDRESS
   uint64_t samplesHdrOffset    = 0x500000; // FAKE ADDRESS
   uint64_t serializedStrOffset = 0x42; // FAKE ADDRESS

   //WRITE MAGIC NUMBER
   fwrite (MAQAO_LPROF_MAGIC,sizeof(char),MAQAO_LPROF_MAGIC_SIZE,file);

   //WRITE VERSION
   fwrite (MAQAO_LPROF_VERSION,sizeof(char),MAQAO_LPROF_VERSION_SIZE,file);

   //WRITE BIN_INFO_HEADER OFFSET
   fwrite (&binInfoHdrOffset,sizeof(uint64_t),1,file);

   //WRITE LIBRARY INFO HEADER OFFSET
   fwrite (&libInfoHdrOffset,sizeof(uint64_t),1,file);

   //WRITE SAMPLES HEADER
   fwrite (&samplesHdrOffset,sizeof(uint64_t),1,file);

   savedFilePosition = ftell(file);
   //WRITE SERIALIZED STR SIZE + OFFSET INFO
   fwrite (&sizeSerializedStr,sizeof(uint64_t),1,file);
   fwrite (&serializedStrOffset,sizeof(uint64_t),1,file);

   return savedFilePosition;
}


int write_string (FILE* file, const char* string)
{
   assert (file !=NULL);

   static uint64_t offset = 1; // START AT 1, 0 IS RESERVED FOR EMPTY STRING
   uint64_t strOffset = 0;
   char* tmp = NULL;
   size_t stringLength = 0;
   uint64_t zero = 0;

   // INITIALIZATION OF THE HASHTABLE
   if ( stringToOffset == NULL )
   {
      stringToOffset = hashtable_new(str_hash,str_equal);
      offset = 1; // RESET OFFSET
      strOffset = offset; // RESET
   }

   // STRING IS NULL ? WE POINT TO THE OFFSET 0 : serializedStr[0] == '\0'
   if (string == NULL)
   {
      fwrite ( &zero,        sizeof(zero),  1, file);
      return 0;
   }


   // NEW STRING ?
   if ( (strOffset = (uint64_t) hashtable_lookup ( stringToOffset , string )) == 0 )
   {
      hashtable_insert (stringToOffset, (void*)string, (void*)offset);
      strOffset = offset;

      stringLength = strlen (string) + 1; // +1 for the '\0' character
      tmp = realloc (serializedStr, (stringLength + offset) * sizeof(*serializedStr));
      if (tmp == NULL)
      {
         //REALLOC ERROR
         // ERROR MESSAGE ...
         fprintf(stderr,"ERROR REALLOC %d\n",__LINE__);
         exit(-1);
      }
      serializedStr = tmp;
      strcpy (&serializedStr[offset], string);

      //WRITE THE NEW OFFSET
      fwrite ( &strOffset,        sizeof(strOffset),  1, file);

      offset += stringLength;
      sizeSerializedStr = offset;

   }
   else // ALREADY INSERTED : WRITE THE OFFSET
   {
      fwrite ( &strOffset,        sizeof(strOffset),  1, file);
   }

   return strOffset;
}

long write_serialized_str_array (FILE* file)
{
   assert(file != NULL);

   long savedFilePosition;
   savedFilePosition = ftell(file);
   //EMPTY STRING == serializedStr[0]
   if (serializedStr != NULL)
      serializedStr[0] = '\0';
   //ALWAYS WRITE THE SERIALIZED STRING AT THE END OF THE FILE
   fwrite ( serializedStr, sizeof(*serializedStr), sizeSerializedStr, file);

   // FREE AND RESET ( used for binary.lprof, libraries.lprof, samples.lprof)
   free(serializedStr);
   serializedStr = NULL;
   hashtable_free(stringToOffset,NULL,NULL);
   stringToOffset = NULL;

   return savedFilePosition;
}

void get_serialized_str_array (FILE* file, uint64_t size ,long offset)
{
   assert(file != NULL);
   long savedFilePosition;

   free(serializedStr);
   serializedStr = NULL;
   serializedStr = lc_malloc (size * sizeof(*serializedStr));

   savedFilePosition = ftell(file);
   fseek (file,offset, SEEK_SET);
   if (fread (serializedStr, sizeof(*serializedStr), size, file) != size)
      DBGMSG0 ("Read error\n");
   fseek (file, savedFilePosition, SEEK_SET);

}


int write_binary_info_header (FILE* file, char* binName, uint32_t nbFunctions, uint32_t nbLoops)
{
   assert(file != NULL);
   uint64_t startingFctsOffset = 0x400600;  //TODO : Fake info
   uint64_t startingLoopsOffset = 0x400800; //TODO : Fake info

   write_string (file, binName);
   fwrite (&nbFunctions,sizeof(uint32_t),1,file);
   fwrite (&nbLoops,sizeof(uint32_t),1,file);
   fwrite (&startingFctsOffset,sizeof(uint64_t),1,file);
   fwrite (&startingLoopsOffset,sizeof(uint64_t),1,file);
   //ACCELERATION TABLE
   return 0;
}

int write_call_chain (FILE* file, call_chain_t* myCallChain)
{
   assert( file != NULL);

   unsigned i;

   fwrite ( &myCallChain->nbFrames, sizeof(myCallChain->nbFrames), 1, file);
   //Write each address of the call chain
   for (i=0; i < myCallChain->nbFrames; i++)
      fwrite ( &myCallChain->addressList[i], sizeof(*myCallChain->addressList), 1, file);

   return 0;
}

int init_sample (lprof_sample_t* mySample, uint64_t addr, uint32_t nbOccurrences, uint32_t nbCallChains, call_chain_t* callChains)
{
   assert(mySample != NULL);

   mySample->address       = addr;
   mySample->nbOccurrences  = nbOccurrences;
   mySample->nbCallChains  = nbCallChains;
   mySample->callChain     = callChains;
   return 0;
}

int write_sample (FILE* file, uint64_t sampleDescriptor, lprof_sample_t* mySample)
{
   assert( file != NULL);

   unsigned i;

   if (sampleDescriptor & PERF_SAMPLE_IP)
   {
      fwrite ( &mySample->address,        sizeof(mySample->address),      1, file);
      fwrite ( &mySample->nbOccurrences,   sizeof(mySample->nbOccurrences), 1, file);
   }

   if ( (sampleDescriptor & PERF_SAMPLE_CALLCHAIN) || (sampleDescriptor & PERF_SAMPLE_STACK_USER) || (sampleDescriptor & PERF_SAMPLE_BRANCH_STACK))
   {
      fwrite ( &mySample->nbCallChains,   sizeof(mySample->nbCallChains),     1, file);

      //Write each callchain
      for (i=0; i < mySample->nbCallChains; i++)
      {
         write_call_chain (file,&mySample->callChain[i]);
      }
   }

   if (sampleDescriptor & PERF_SAMPLE_CPU)
   {
      fwrite ( &mySample->nbCpuIds,   sizeof(mySample->nbCpuIds),     1, file);

      //Write cpu ids list
      for (i=0; i < mySample->nbCpuIds; i++)
         fwrite ( &mySample->cpuIdsList[i],   sizeof(mySample->cpuIdsList[i]),     1, file);
   }

   return 0;
}

int init_event (lprof_event_t* myEvent, char* hwcName, uint32_t threadId, uint32_t nbSamples, uint64_t sampleTypes, lprof_sample_t* samples)
{
   myEvent->hwcName           = hwcName;
   myEvent->threadId          = threadId;
   myEvent->sampleDescriptor  = sampleTypes;
   myEvent->nbSamples         = nbSamples;
   myEvent->samples           = samples;

   return 0;
}

long write_event (FILE* file, lprof_event_t* myEvent)
{
   assert( file != NULL);

   long savedFilePosition;

   write_string (file, myEvent->hwcName);                                              // Write hwcName
   fwrite ( &myEvent->threadId,   sizeof(myEvent->threadId), 1, file);                 // Write threadId
   fwrite ( &myEvent->sampleDescriptor,   sizeof(myEvent->sampleDescriptor), 1, file); // Write sampleDescriptor
   savedFilePosition = ftell(file);                                                    // Save nbSamples position
   fwrite ( &myEvent->nbSamples,   sizeof(myEvent->nbSamples), 1, file);               // Write nbSamples

   //Write each address of the callchain
   //for (i=0; i < myEvent->nbSamples; i++)
   //   write_sample(file, &myEvent->samples[i]);

   return savedFilePosition;
}


void update_event (FILE* file, lprof_event_t* myEvent, long filePosition)
{
   assert( file != NULL);

   long savedFilePosition = ftell(file);

   //TODO: HANDLE ERROR FSEEK
   fseek (file, filePosition, SEEK_SET);
   fwrite ( &myEvent->nbSamples,   sizeof(myEvent->nbSamples), 1, file);
   fseek (file, savedFilePosition, SEEK_SET);

   return;
}


int init_library (lprof_library_t* myLib, const char* name, uint64_t* startMapAddr, uint64_t* stopMapAddr,uint32_t nbFunctions, uint32_t nbLoops ,lprof_fct_t* fctsInfo, lprof_loop_t* loopsInfo)
{
   assert(myLib != NULL);

   myLib->name             = lc_strdup (name);
   myLib->startMapAddress  = startMapAddr;
   myLib->stopMapAddress   = stopMapAddr;
   myLib->nbFunctions      = nbFunctions;
   myLib->nbLoops          = nbLoops;
   myLib->fctsInfo         = fctsInfo;
   myLib->loopsInfo        = loopsInfo;
   return 0;
}

int write_library (FILE* file, lprof_library_t* myLib)
{
   assert (file != NULL);

   unsigned i;
   write_string (file, myLib->name);
   fwrite ( &myLib->nbProcesses,     sizeof(myLib->nbProcesses),      1,                  file);
   fwrite ( myLib->startMapAddress,  sizeof(*myLib->startMapAddress), myLib->nbProcesses, file);
   fwrite ( myLib->stopMapAddress,   sizeof(*myLib->stopMapAddress),  myLib->nbProcesses, file);
   fwrite ( &myLib->nbFunctions,     sizeof(myLib->nbFunctions),      1,                  file);
   fwrite ( &myLib->nbLoops,         sizeof(myLib->nbLoops),          1,                  file);
   //Write each functions of the library
   for (i=0; i < myLib->nbFunctions; i++)
      write_function(file, &myLib->fctsInfo[i]);
   //Write each loops of the library
   for (i=0; i< myLib->nbLoops; i++)
      write_loop(file, &myLib->loopsInfo[i]);
   return 0;
}

int init_function (  lprof_fct_t* myFct,
                     char* name,
                     uint32_t nbParts,
                     uint64_t* startAddr,
                     uint64_t* stopAddr,
                     char* srcFile,
                     uint32_t srcLine,
                     uint32_t nbOutermostLoops,
                     uint32_t* outermostLoopsList)
{
   assert(myFct != NULL);

   myFct->name          = lc_strdup (name);
   myFct->nbParts       = nbParts;
   myFct->startAddress  = startAddr;
   myFct->stopAddress   = stopAddr;
   myFct->srcFile       = lc_strdup(srcFile);
   myFct->srcLine       = srcLine;
   myFct->nbOutermostLoops = nbOutermostLoops;
   myFct->outermostLoopsList = outermostLoopsList;
   return 0;
}


int write_function (FILE* file, lprof_fct_t* myFct)
{
   assert (file != NULL);

   //size_t nameLength    = strlen (myFct->name);
   //size_t srcFileLength = strlen (myFct->srcFile);

   //fwrite ( &nameLength,         sizeof(size_t),               1,              file);
   //fwrite ( myFct->name,         sizeof(char),                 nameLength,     file);
   write_string (file, myFct->name);
   fwrite ( &myFct->nbParts,     sizeof(myFct->nbParts),       1,              file);
   fwrite ( myFct->startAddress, sizeof(*myFct->startAddress), myFct->nbParts, file);
   fwrite ( myFct->stopAddress,  sizeof(*myFct->stopAddress),  myFct->nbParts, file);
   write_string (file, myFct->srcFile);
   fwrite ( &myFct->srcLine,     sizeof(myFct->srcLine),       1,              file);
   fwrite ( &myFct->nbOutermostLoops,   sizeof(myFct->nbOutermostLoops), 1, file);
   if (myFct->nbOutermostLoops != 0)
      fwrite ( myFct->outermostLoopsList, sizeof(*myFct->outermostLoopsList), myFct->nbOutermostLoops, file);
   return 0;
}


int init_loop ( lprof_loop_t* myLoop,
                uint32_t id,
                uint32_t nbParts,
                uint64_t* startAddr,
                uint64_t* stopAddr,
                char* srcFile,
                const char* srcFunctionName,
                uint32_t srcFunctionLine,
                uint32_t srcStartLine,
                uint32_t srcStopLine,
                uint8_t level,
                uint32_t nbChildren,
                uint32_t* childrenList )
{
   assert(myLoop != NULL);

   myLoop->id               = id;
   myLoop->nbParts          = nbParts;
   myLoop->startAddress     = startAddr;
   myLoop->stopAddress      = stopAddr;
   myLoop->srcFile          = lc_strdup(srcFile);
   myLoop->srcFunctionName  = lc_strdup(srcFunctionName);
   myLoop->srcFunctionLine  = srcFunctionLine;
   myLoop->srcStartLine     = srcStartLine;
   myLoop->srcStopLine      = srcStopLine;
   myLoop->level            = level;
   myLoop->nbChildren       = nbChildren;
   myLoop->childrenList     = childrenList;

   return 0;
}

int write_block (FILE* file, lprof_block_t* myBlock)
{
   fwrite ( &myBlock->blockId,      sizeof(myBlock->blockId),       1, file);
   fwrite ( &myBlock->startAddress, sizeof(myBlock->startAddress), 1, file);
   fwrite ( &myBlock->stopAddress,  sizeof(myBlock->stopAddress),  1, file);

   return 0;
}

int write_loop (FILE* file, lprof_loop_t* myLoop)
{
   assert (file != NULL);

   unsigned i;

   fwrite ( &myLoop->id,              sizeof(myLoop->id),              1,                     file);
   fwrite ( &myLoop->nbParts,         sizeof(myLoop->nbParts),         1,                     file);
   fwrite ( myLoop->startAddress,     sizeof(*myLoop->startAddress),   myLoop->nbParts,       file);
   fwrite ( myLoop->stopAddress,      sizeof(*myLoop->stopAddress),    myLoop->nbParts,       file);

   fwrite ( &myLoop->nbBlocks,        sizeof(myLoop->nbBlocks),        1,                     file);
   for (i=0; i< myLoop->nbBlocks; i++)
      write_block(file, &myLoop->blockIds[i]);

   write_string (file, myLoop->srcFile);
   write_string (file, myLoop->srcFunctionName);
   fwrite ( &myLoop->srcFunctionLine, sizeof(myLoop->srcFunctionLine), 1, file);
   fwrite ( &myLoop->srcStartLine,    sizeof(myLoop->srcStartLine),    1, file);
   fwrite ( &myLoop->srcStopLine,     sizeof(myLoop->srcStopLine),     1, file);
   fwrite ( &myLoop->level,           sizeof(myLoop->level),           1, file);
   fwrite ( &myLoop->nbChildren,      sizeof(myLoop->nbChildren),      1, file);
   fwrite ( myLoop->childrenList,     sizeof(*myLoop->childrenList),   myLoop->nbChildren, file);

   return 0;
}

int write_binary_info (FILE* file, lprof_binary_info_t* binaryInfo, uint32_t nbFunctions, uint32_t nbLoops)
{
   assert (file != NULL);
   assert (binaryInfo != NULL);

   unsigned i;

   for (i=0; i < nbFunctions; i++)
      write_function(file, &binaryInfo->functions[i]);
   //Write each loops of the library
   for (i=0; i< nbLoops; i++)
      write_loop(file, &binaryInfo->loops[i]);

   return 0;
}

int write_libraries_info_header (FILE* file,uint32_t nbLibraries, uint64_t startingLibrariesOffset)
{
   assert(file != NULL);
   //uint32_t nbLibraries = 1;
   //uint64_t startingLibrariesOffset = 0x400600;
   uint64_t emptyOffset[MAX_LIBRARIES] = {0x0};
   fwrite (&nbLibraries, sizeof(uint32_t),1,file);
   fwrite (&startingLibrariesOffset,sizeof(uint64_t),1,file);
   //ACCELERATION TABLE
   fwrite (&emptyOffset,sizeof(uint64_t),MAX_LIBRARIES,file);
   return 0;
}


long write_events_header (FILE* file, uint32_t nbThreads, uint32_t nbHwc,char* hwcListName)
{
   assert(file != NULL);

   //uint32_t nbThreads = 2;
   //uint32_t nbHwc = 2;
   //char* hwcListName = "UNHALTED_REFERENCE_CYCLES,INSTR_RETIRED";
   long saveFilePosition = ftell(file);
   uint64_t startingEventsOffset = 0x400600;


   fwrite (&nbThreads, sizeof(uint32_t), 1, file);
   fwrite (&nbHwc, sizeof(uint32_t), 1, file);
   write_string (file, hwcListName);
   fwrite (&startingEventsOffset, sizeof(uint64_t), 1, file);

   //ACCELERATION TABLE
   //uint64_t emptyOffset[MAX_THREADS] = {0x0};
   //fwrite (&emptyOffset,sizeof(uint64_t),MAX_THREADS,file);


   return saveFilePosition;
}

void update_events_header_nb_threads (FILE* file, uint32_t nbThreads, long filePosition)
{
   assert( file != NULL);

   long savedFilePosition = ftell(file);

   //TODO: HANDLE ERROR FSEEK
   fseek (file, filePosition, SEEK_SET);
   fwrite (&nbThreads, sizeof(uint32_t), 1, file);
   fseek (file, savedFilePosition, SEEK_SET);

}

void update_lprof_header (FILE* file, uint64_t serializedStrOffset, long filePosition)
{
   assert( file != NULL);

   long savedFilePosition = ftell(file);

   //TODO: HANDLE ERROR FSEEK
   fseek (file, filePosition, SEEK_SET);
   fwrite (&sizeSerializedStr, sizeof(serializedStrOffset), 1, file);
   fwrite (&serializedStrOffset, sizeof(serializedStrOffset), 1, file);
   fseek (file, savedFilePosition, SEEK_SET);

   sizeSerializedStr = 0;
}


void get_lprof_header (FILE* file, lprof_header_t* lprofHeader)
{
   assert ( file!= NULL);

   //GET MAGIC NUMBER
   char magicNumber[MAQAO_LPROF_MAGIC_SIZE];
   if (fread (magicNumber, sizeof(char), MAQAO_LPROF_MAGIC_SIZE, file) != MAQAO_LPROF_MAGIC_SIZE)
      DBGMSG0 ("Error while reading lprof magic number\n");
   if (strcmp(magicNumber,MAQAO_LPROF_MAGIC) != 0)
   {
      fprintf(stderr, "[ERROR] Unrecognized file format\n");
      return;
   }
   strcpy(lprofHeader->magic,magicNumber);

   //GET VERSION
   char version[MAQAO_LPROF_VERSION_SIZE];
   if (fread (version, sizeof(char), MAQAO_LPROF_VERSION_SIZE, file) != MAQAO_LPROF_VERSION_SIZE)
      DBGMSG0 ("Error while reading lprof version\n");
   strcpy(lprofHeader->version,version);

   // GET BINARY INFO HEADER OFFSET
   if (fread (&lprofHeader->binaryInfoHeaderOffset, sizeof(uint64_t), 1, file) != 1)
      DBGMSG0 ("Error while reading binary info header\n");

   // GET LIBRARY INFO HEADER OFFSET
   if (fread (&lprofHeader->libraryInfoHeaderOffset, sizeof(uint64_t), 1, file) != 1)
      DBGMSG0 ("Error while reading library info header\n");

   // GET EVENTS HEADER OFFSET
   if (fread (&lprofHeader->eventsHeaderOffset, sizeof(uint64_t), 1, file) != 1)
      DBGMSG0 ("Error while reading events header\n");

   // GET SERIALIZED STR SIZE + OFFSET
   if (fread (&sizeSerializedStr, sizeof(uint64_t), 1, file) != 1)
      DBGMSG0 ("Error while reading strings zone size\n");
   if (fread (&lprofHeader->serializedStrOffset, sizeof(uint64_t), 1, file) != 1)
      DBGMSG0 ("Error while reading strings zone offset\n");
   get_serialized_str_array (file,sizeSerializedStr , lprofHeader->serializedStrOffset);

}

void print_lprof_header (lprof_header_t* lprofHeader)
{
   fprintf(stderr , "------ MPERF_HEADER ------\n");
   fprintf(stderr , "magic number\t\t: %s\n",lprofHeader->magic);
   fprintf(stderr , "version\t\t\t: %s\n",lprofHeader->version);
   fprintf(stderr , "binaryInfoHeaderOffset\t: %p\n", (void*)lprofHeader->binaryInfoHeaderOffset);
   fprintf(stderr , "libraryInfoHeaderOffset : %p\n", (void*)lprofHeader->libraryInfoHeaderOffset);
   fprintf(stderr , "eventsHeaderOffset\t: %p\n",     (void*)lprofHeader->eventsHeaderOffset);
}


void get_bin_info_header (FILE* file, lprof_binary_info_header_t* lprofBinInfoHeader)
{
   assert ( file!= NULL);
   //GET BIN NAME
   get_string (file, &lprofBinInfoHeader->binName);

   //GET NB_FUNCTIONS
   if (fread (&lprofBinInfoHeader->nbFunctions, sizeof(lprofBinInfoHeader->nbFunctions), 1, file) != 1)
      DBGMSG0 ("Error while reading nb functions\n");

   //GET NB_LOOPS
   if (fread (&lprofBinInfoHeader->nbLoops, sizeof(lprofBinInfoHeader->nbLoops), 1, file) != 1)
      DBGMSG0 ("Error while reading nb loops\n");

   //GET FCTS INFO OFFSET
   if (fread (&lprofBinInfoHeader->fctsInfoOffset, sizeof(lprofBinInfoHeader->fctsInfoOffset), 1, file) != 1)
      DBGMSG0 ("Error while reading functions info offset\n");

   //GET LOOPS INFO OFFSET
   if (fread (&lprofBinInfoHeader->loopsInfoOffset, sizeof(lprofBinInfoHeader->loopsInfoOffset), 1, file) != 1)
      DBGMSG0 ("Error while reading loops info offset\n");
}

void print_bin_info_header (lprof_binary_info_header_t* lprofBinInfoHeader)
{
   fprintf(stderr , "------ MPERF_BIN_INFO_HEADER ------\n");
   fprintf(stderr , "binName\t: %s\n",lprofBinInfoHeader->binName);
   fprintf(stderr , "nbFunctions\t: %"PRId32"\n",lprofBinInfoHeader->nbFunctions);
   fprintf(stderr , "nbLoops\t\t: %"PRId32"\n",lprofBinInfoHeader->nbLoops);
   fprintf(stderr , "fctsInfoOffset\t: %p\n", (void*)lprofBinInfoHeader->fctsInfoOffset);
   fprintf(stderr , "loopsInfoOffset : %p\n", (void*)lprofBinInfoHeader->loopsInfoOffset);
}

void get_string (FILE* file, char** string)
{
   assert (file != NULL);

   uint64_t offset = 0;
   size_t stringLength = 0;


   //GET OFFSET
   if (fread (&offset, sizeof(offset), 1, file) != 1)
      DBGMSG0 ("Error while reading string offset\n");

   if (offset != 0)
      stringLength = strlen (&serializedStr[offset]);

   *string = lc_calloc (stringLength+1, sizeof(char));


   // GET CORRESPONDING STRING
   if (offset != 0)
      strcpy (*string, &serializedStr[offset]);
}


void get_function (FILE* file, lprof_fct_t* myFct)
{

   assert (file != NULL);

   uint64_t* startAddress, *stopAddress;
   uint32_t* outermostLoopsList;

   //GET NAME
   get_string (file, &myFct->name);

   //GET NB PARTS
   if (fread ( &myFct->nbParts, sizeof(myFct->nbParts), 1, file) != 1)
      DBGMSG0 ("Error while reading function nb parts\n");

   //GET START ADDRESS ARRAY
   startAddress = lc_malloc (myFct->nbParts * sizeof(*startAddress));
   if (fread ( startAddress, sizeof(*startAddress), myFct->nbParts, file) != myFct->nbParts)
      DBGMSG0 ("Error while reading start addresses for function parts\n");
   myFct->startAddress = startAddress;

   //GET STOP ADDRESS ARRAY
   stopAddress = lc_malloc (myFct->nbParts * sizeof(*stopAddress));
   if (fread ( stopAddress, sizeof(*stopAddress), myFct->nbParts, file) != myFct->nbParts)
      DBGMSG0 ("Error while reading stop addresses for function parts\n");
   myFct->stopAddress = stopAddress;

   //GET SRC FILE
   get_string (file, &myFct->srcFile);

   //GET SRC LINE
   if (fread ( &myFct->srcLine, sizeof(myFct->srcLine), 1, file) != 1)
      DBGMSG0 ("Error while reading function source line\n");

   //GET NB OUTERMOST LOOPS
   if (fread ( &myFct->nbOutermostLoops, sizeof(myFct->nbOutermostLoops), 1, file) != 1)
      DBGMSG0 ("Error while reading number of function outermost loops\n");

   if (myFct->nbOutermostLoops != 0)
   {
      //GET OUTERMOST LOOPS LIST
      outermostLoopsList = lc_malloc (myFct->nbOutermostLoops * sizeof(*outermostLoopsList));
      if (fread ( outermostLoopsList, sizeof(*outermostLoopsList), myFct->nbOutermostLoops, file) != myFct->nbOutermostLoops)
         DBGMSG0 ("Error while reading function outermost loops\n");
      myFct->outermostLoopsList = outermostLoopsList;
   }
   else
      myFct->outermostLoopsList = NULL;
}

void print_function (lprof_fct_t* myFct)
{
   unsigned i;

   fprintf(stderr , "\n------ FUNCTION  : %s ------\n",myFct->name);
   for (i=0; i< myFct->nbParts; i++)
      fprintf(stderr , "{%p -> %p}, ", (void*)myFct->startAddress[i],(void*)myFct->stopAddress[i]);
   fprintf(stderr , "\nsrcFile\t\t: %s\n",myFct->srcFile);
   fprintf(stderr , "srcLine\t\t: %"PRIu32"\n",myFct->srcLine);
   fprintf(stderr , "Outermost Loops :");
   for (i=0; i< myFct->nbOutermostLoops; i++)
      fprintf(stderr , " %u, ", myFct->outermostLoopsList[i]);
   fprintf(stderr,"\n");
}


void get_library (FILE* file, lprof_library_t* myLib)
{
   assert (file != NULL);

   unsigned i;
   uint64_t* startMapAddress, *stopMapAddress;

   //GET LIBRARY NAME
   get_string (file, &myLib->name);

   //GET NB PROCESSES
   if (fread ( &myLib->nbProcesses, sizeof(myLib->nbProcesses), 1, file) != 1)
      DBGMSG0 ("Error while reading number of library processes\n");

   startMapAddress = lc_malloc (myLib->nbProcesses * sizeof(*myLib->startMapAddress));
   //GET START MAPPED ADDRESS ARRAY
   if (fread ( startMapAddress, sizeof(*startMapAddress), myLib->nbProcesses, file) != myLib->nbProcesses)
      DBGMSG0 ("Error while reading library start map addresses\n");
   myLib->startMapAddress = startMapAddress;

   stopMapAddress = lc_malloc (myLib->nbProcesses * sizeof(*myLib->stopMapAddress));
   //GET STOP MAPPED ADDRESS ARRAY
   if (fread ( stopMapAddress, sizeof(*stopMapAddress), myLib->nbProcesses, file) != myLib->nbProcesses)
      DBGMSG0 ("Error while reading library stop map addresses\n");
   myLib->stopMapAddress = stopMapAddress;

   //GET NB FUNCTIONS
   if (fread ( &myLib->nbFunctions, sizeof(myLib->nbFunctions), 1, file) != 1)
      DBGMSG0 ("Error while reading number of library functions\n");

   //GET NB LOOPS
   if (fread ( &myLib->nbLoops, sizeof(myLib->nbLoops), 1, file) != 1)
      DBGMSG0 ("Error while reading number of library loops\n");

   //GET FCTS INFO
   myLib->fctsInfo = lc_malloc (myLib->nbFunctions * sizeof(*myLib->fctsInfo));
   for (i=0; i< myLib->nbFunctions; i++)
      get_function (file,&myLib->fctsInfo[i]);


   //GET LOOPS INFO
   myLib->loopsInfo = lc_malloc (myLib->nbLoops * sizeof(*myLib->loopsInfo));
   for (i=0; i< myLib->nbLoops; i++)
      get_loop (file,&myLib->loopsInfo[i]);

}

void print_library (lprof_library_t* myLib)
{
   unsigned i;

   fprintf(stderr , "\n------ LIBRARY  : %s ------\n",myLib->name);
   for (i=0; i < myLib->nbProcesses; i++)
      fprintf(stderr , "mapped address\t: {%p -> %p}\n", (void*)myLib->startMapAddress[i],(void*)myLib->stopMapAddress[i]);
   fprintf(stderr , "nbFunctions\t: %"PRIu32"\n",myLib->nbFunctions);
   fprintf(stderr , "nbLoops\t\t: %"PRIu32"\n",myLib->nbLoops);
   for (i=0; i < myLib->nbFunctions; i++)
      print_function (&myLib->fctsInfo[i]);
   for (i=0; i < myLib->nbLoops; i++)
      print_loop (&myLib->loopsInfo[i]);
}

void get_block (FILE* file, lprof_block_t* myBlock)
{
   //GET BLOCK ID
   if (fread ( &myBlock->blockId, sizeof(myBlock->blockId), 1, file) != 1)
      DBGMSG0 ("Error while reading block ID\n");

   //GET START ADDRESS
   if (fread ( &myBlock->startAddress, sizeof(myBlock->startAddress), 1, file) != 1)
      DBGMSG0 ("Error while reading block start address\n");

   //GET STOP ADDRESS
   if (fread ( &myBlock->stopAddress, sizeof(myBlock->stopAddress), 1, file) != 1)
      DBGMSG0 ("Error while reading block stop address\n");
}

void get_loop (FILE* file, lprof_loop_t* myLoop)
{
   assert (file != NULL);

   unsigned int i = 0;
   uint64_t* startAddress, *stopAddress;
   uint32_t* childrenList;

   //GET LOOP ID
   if (fread ( &myLoop->id, sizeof(myLoop->id), 1, file) != 1)
      DBGMSG0 ("Error while reading loop ID\n");

   //GET NB PARTS
   if (fread ( &myLoop->nbParts, sizeof(myLoop->nbParts), 1, file) != 1)
      DBGMSG0 ("Error while reading loop number of parts\n");

   //GET START ADDRESS ARRAY
   startAddress = lc_malloc (myLoop->nbParts * sizeof(*startAddress));
   if (fread ( startAddress, sizeof(*startAddress), myLoop->nbParts, file) != myLoop->nbParts)
      DBGMSG0 ("Error while reading loop start addresses\n");
   myLoop->startAddress = startAddress;

   //GET STOP ADDRESS ARRAY
   stopAddress = lc_malloc (myLoop->nbParts * sizeof(*stopAddress));
   if (fread ( stopAddress, sizeof(*stopAddress), myLoop->nbParts, file) != myLoop->nbParts)
      DBGMSG0 ("Error while reading loop stop addresses\n");
   myLoop->stopAddress = stopAddress;

   //GET BLOCKS IDS
   if (fread ( &myLoop->nbBlocks, sizeof(myLoop->nbBlocks), 1, file) != 1)
      DBGMSG0 ("Error while reading loop block IDs\n");
   myLoop->blockIds = malloc (myLoop->nbBlocks * sizeof(*myLoop->blockIds));
   for (i = 0; i < myLoop->nbBlocks; i++)
   {
      get_block(file, &myLoop->blockIds[i]);
   }

   //GET SRC FILE
   get_string (file, &myLoop->srcFile);

   //GET SRC FUNCTION NAME
   get_string (file, &myLoop->srcFunctionName);

   //GET FUNCTION SRC LINE
   if (fread ( &myLoop->srcFunctionLine, sizeof(myLoop->srcFunctionLine), 1, file) != 1)
      DBGMSG0 ("Error while reading function source line\n");

   //GET LOOP START LINE
   if (fread ( &myLoop->srcStartLine, sizeof(myLoop->srcStartLine), 1, file) != 1)
      DBGMSG0 ("Error while reading loop start line\n");

   //GET LOOP STOP LINE
   if (fread ( &myLoop->srcStopLine, sizeof(myLoop->srcStopLine), 1, file) != 1)
      DBGMSG0 ("Error while reading loop stop line\n");

   //GET LOOP LEVEL
   if (fread ( &myLoop->level, sizeof(myLoop->level), 1, file) != 1)
      DBGMSG0 ("Error while reading loop level\n");

   //GET NB CHILDREN
   if (fread ( &myLoop->nbChildren, sizeof(myLoop->nbChildren), 1, file) != 1)
      DBGMSG0 ("Error while reading loop children nb\n");

   //GET CHILDREN
   if (myLoop->nbChildren != 0)
   {
      childrenList = lc_malloc (myLoop->nbChildren * sizeof(*childrenList));
      if (fread ( childrenList, sizeof(*childrenList), myLoop->nbChildren, file) != myLoop->nbChildren)
         DBGMSG0 ("Error while reading loop children\n");
      myLoop->childrenList = childrenList;
   }
   else
      myLoop->childrenList = NULL;
}

void print_loop (lprof_loop_t* myLoop)
{

   unsigned i;

   fprintf(stderr , "\n------ LOOP %"PRIu32" ------\n",myLoop->id);
   for (i=0; i< myLoop->nbParts; i++)
      fprintf(stderr , "{%p -> %p}, ", (void*)myLoop->startAddress[i],(void*)myLoop->stopAddress[i]);
   fprintf(stderr,"\n");
   for (i = 0; i < myLoop->nbBlocks; i++)
      fprintf(stderr,"[Block %"PRId64" : %"PRIx64" -> %"PRIx64"], ", myLoop->blockIds[i].blockId,myLoop->blockIds[i].startAddress,myLoop->blockIds[i].stopAddress);

   fprintf(stderr , "\nsrcFile\t\t: %s\n",myLoop->srcFile);
   fprintf(stderr , "srcFunctionName\t: %s\n",myLoop->srcFunctionName);
   fprintf(stderr , "srcFunctionLine\t: %"PRIu32"\n",myLoop->srcFunctionLine);
   fprintf(stderr , "srcStartLine\t: %"PRIu32"\n",myLoop->srcStartLine);
   fprintf(stderr , "srcSopLine\t: %"PRIu32"\n",myLoop->srcStopLine);
   switch (myLoop->level)
   {
   case OUTERMOST_LOOP:
      fprintf(stderr ,"level\t\t: OUTERMOST\n");
      break;
   case INNERMOST_LOOP:
      fprintf(stderr ,"level\t\t: INNERMOST\n");
      break;
   case SINGLE_LOOP:
      fprintf(stderr ,"level\t\t: SINGLE\n");
      break;
   case INBETWEEN_LOOP:
      fprintf(stderr ,"level\t\t: INBETWEEN\n");
      break;
   default:
      fprintf(stderr ,"level\t\t: Unknown (%"PRIu8" \n",myLoop->level);
   }

   fprintf(stderr , "Children\t:");
   for (i=0; i< myLoop->nbChildren; i++)
      fprintf(stderr , " %u, ", myLoop->childrenList[i]);
   fprintf(stderr,"\n");

}


void get_event (FILE* file, lprof_event_t* myEvent)
{
   assert (file != NULL);

   unsigned i;
   lprof_sample_t* samples;

   //GET HWC NAME
   get_string (file, &myEvent->hwcName);

   //GET THREAD ID
   if (fread ( &myEvent->threadId, sizeof(myEvent->threadId), 1, file) != 1)
      DBGMSG0 ("Error while reading event thread ID\n");

   // GET SAMPLE DESCRIPTOR
   if (fread ( &myEvent->sampleDescriptor, sizeof(myEvent->sampleDescriptor), 1, file) != 1)
      DBGMSG0 ("Error while reading event sample descriptor\n");

   //GET NB SAMPLES
   if (fread ( &myEvent->nbSamples, sizeof(myEvent->nbSamples), 1, file) != 1)
      DBGMSG0 ("Error while reading samples nb\n");

   samples = lc_malloc (myEvent->nbSamples * sizeof(*samples));
   for (i=0; i< myEvent->nbSamples; i++)
      get_sample (file,myEvent->sampleDescriptor, &samples[i]);
   myEvent->samples = samples;

}

void print_event (lprof_event_t* myEvent)
{
   unsigned i;

   fprintf(stderr , "\n------ EVENT ------\n");
   fprintf(stderr , "hwcName \t\t: %s\n",myEvent->hwcName);
   fprintf(stderr , "threadId\t\t: %"PRId32"\n",myEvent->threadId);
   fprintf(stderr , "sampleDescriptor\t\t: %"PRId64"\n",myEvent->sampleDescriptor);
   fprintf(stderr , "nbSamples\t\t: %"PRId32"\n",myEvent->nbSamples);
   for (i=0; i< myEvent->nbSamples; i++)
      print_sample(&myEvent->samples[i],myEvent->sampleDescriptor);
}


void get_sample (FILE* file, uint64_t sampleDescriptor ,lprof_sample_t* mySample)
{
   assert (file != NULL);

   unsigned i;

   //GET ADDRESS
   if (sampleDescriptor & PERF_SAMPLE_IP)
   {
      if (fread ( &mySample->address, sizeof(mySample->address), 1, file) != 1)
         DBGMSG0 ("Error while reading sample address\n");
      if (fread ( &mySample->nbOccurrences, sizeof(mySample->nbOccurrences), 1, file) != 1)
         DBGMSG0 ("Error while reading sample occurrences nb\n");
   }


   //GET CALLCHAIN (BACKTRACE)
   if ( (sampleDescriptor & PERF_SAMPLE_CALLCHAIN) || (sampleDescriptor & PERF_SAMPLE_STACK_USER) || (sampleDescriptor & PERF_SAMPLE_BRANCH_STACK))
   {
      //GET NB CALL CHAINS
      if (fread ( &mySample->nbCallChains, sizeof(mySample->nbCallChains), 1, file) != 1)
         DBGMSG0 ("Error while reading callchains nb\n");

      if (mySample->nbCallChains == 0)
         mySample->callChain = NULL;
      else
      {
         mySample->callChain = lc_malloc (mySample->nbCallChains * sizeof(*mySample->callChain));
         for (i=0; i < mySample->nbCallChains; i++)
            get_call_chain (file,&mySample->callChain[i]);
      }
   }
   else
   {
      mySample->nbCallChains = 0;
      mySample->callChain = NULL;
   }


   //GET CPU IDS LIST
   if (sampleDescriptor & PERF_SAMPLE_CPU)
   {
      if (fread ( &mySample->nbCpuIds,   sizeof(mySample->nbCpuIds),     1, file) != 1)
         DBGMSG0 ("Error while reading CPU IDs\n");

      mySample->cpuIdsList = lc_malloc (mySample->nbCpuIds * sizeof(*mySample->cpuIdsList) );
      for (i=0; i < mySample->nbCpuIds; i++)
         if (fread ( &mySample->cpuIdsList[i],   sizeof(mySample->cpuIdsList[i]),     1, file) != 1)
            DBGMSG0 ("Error while reading a CPU IDs list\n");
   }
}

void get_call_chain (FILE* file, call_chain_t* myCallChain)
{
   assert (file != NULL);

   unsigned i;

   //GET NB FRAMES
   if (fread ( &myCallChain->nbFrames, sizeof(myCallChain->nbFrames), 1, file) != 1)
      DBGMSG0 ("Error while reading callchain frames nb\n");

   //GET ADDRESS LIST
   myCallChain->addressList = lc_malloc (myCallChain->nbFrames *  sizeof(*myCallChain->addressList));
   for (i=0; i < myCallChain->nbFrames; i++)
   {
      if (fread ( &myCallChain->addressList[i], sizeof(*myCallChain->addressList), 1, file) != 1)
         DBGMSG0 ("Error while reading callchain addresses\n");
   }
}

void print_call_chain (call_chain_t* myCallChain)
{
   assert (myCallChain != NULL);

   unsigned i;
   fprintf(stderr,"CALLCHAIN - %d\n",myCallChain->nbFrames);
   for (i=0; i< myCallChain->nbFrames; i++)
      fprintf(stderr,"%p - ",(void*)myCallChain->addressList[i]);
   fprintf(stderr,"\n");
}

void print_sample (lprof_sample_t* mySample, uint64_t sampleDescriptor)
{
   assert (mySample != NULL);

   unsigned i;

   fprintf(stderr , "\n------ SAMPLE ------\n");

   if (sampleDescriptor & PERF_SAMPLE_IP)
   {
      fprintf(stderr , "address \t\t: %p\n",(void*)mySample->address);
      fprintf(stderr , "nbOccurrences\t\t: %"PRId32"\n",mySample->nbOccurrences);
   }

   if ( (sampleDescriptor & PERF_SAMPLE_CALLCHAIN) || (sampleDescriptor & PERF_SAMPLE_STACK_USER) || (sampleDescriptor & PERF_SAMPLE_BRANCH_STACK))
   {
      for (i=0; i< mySample->nbCallChains; i++)
         print_call_chain (&mySample->callChain[i]);
   }


   if (sampleDescriptor & PERF_SAMPLE_CPU)
   {
      fprintf(stderr , "nbCpuIds\t\t: %"PRId64"\n",mySample->nbCpuIds);
      for (i=0; i< mySample->nbCpuIds; i++)
         fprintf(stderr , "%"PRId32",",mySample->cpuIdsList[i]);
      fprintf(stderr , "\n");
   }

}


void get_bin_info (FILE* file, uint32_t* nbFunctions, uint32_t* nbLoops, lprof_binary_info_t* lprofBinInfo)
{
   assert (file != NULL);

   unsigned i;

   lprof_fct_t* functions = lc_malloc (*nbFunctions * sizeof(*functions));;
   lprof_loop_t* loops = lc_malloc (*nbLoops * sizeof(*loops));

   for (i=0; i< *nbFunctions; i++)
      get_function (file,&functions[i]);

   lprofBinInfo->functions = functions;

   for (i=0; i< *nbLoops; i++)
      get_loop (file, &loops[i]);
   lprofBinInfo->loops = loops;
}

void print_bin_info (lprof_binary_info_t* lprofBinInfo, uint32_t* nbFunctions, uint32_t* nbLoops)
{
   unsigned i;

   for (i=0; i< *nbFunctions; i++)
      print_function (&lprofBinInfo->functions[i]);
   for (i=0; i< *nbLoops; i++)
      print_loop (&lprofBinInfo->loops[i]);
}


void get_libs_info_header (FILE* file, lprof_libraries_info_header_t* lprofLibInfoHeader)
{
   assert ( file!= NULL);

   //GET NB_LIBRARIES
   if (fread (&lprofLibInfoHeader->nbLibraries, sizeof(lprofLibInfoHeader->nbLibraries), 1, file) != 1)
      DBGMSG0 ("Error while reading libraries nb\n");

   //GET LIBRARIES INFO OFFSET
   if (fread (&lprofLibInfoHeader->librariesInfoOffset, sizeof(lprofLibInfoHeader->librariesInfoOffset), 1, file) != 1)
      DBGMSG0 ("Error while reading libraries info offset\n");

   //GET ACCELERATION ARRAY
   if (fread (&lprofLibInfoHeader->accelerationArray, sizeof(*lprofLibInfoHeader->accelerationArray), MAX_LIBRARIES, file) != MAX_LIBRARIES)
      DBGMSG0 ("Error while reading libraries data\n");

}

void print_libs_info_header (lprof_libraries_info_header_t* lprofLibInfoHeader)
{
   unsigned i;

   fprintf(stderr , "\n------ MPERF_LIBS_INFO_HEADER ------\n");
   fprintf(stderr , "nbLibraries\t\t: %"PRId32"\n",lprofLibInfoHeader->nbLibraries);
   fprintf(stderr , "librariesInfoOffset\t: %p\n",(void*)lprofLibInfoHeader->librariesInfoOffset);
   fprintf(stderr , "accelerationArray : ");
   for (i=0; i< MAX_LIBRARIES; i++)
      fprintf(stderr , "%p,", (void*)lprofLibInfoHeader->accelerationArray[i]);
   fprintf(stderr,"\n");

}

void get_libs_info (FILE* file, uint32_t* nbLibraries, lprof_libraries_info_t* lprofLibsInfo)
{
   assert (file != NULL);

   unsigned i;

   lprof_library_t* libraries = lc_malloc (*nbLibraries * sizeof(*libraries));;
   for (i=0; i< *nbLibraries; i++)
   {
      get_library (file,&libraries[i]);
      //print_library(&libraries[i]);
   }
   lprofLibsInfo->libraries = libraries;
}

void print_libs_info (lprof_libraries_info_t* lprofLibsInfo, uint32_t* nbLibraries)
{
   unsigned i;

   for (i=0; i< *nbLibraries; i++)
      print_library (&lprofLibsInfo->libraries[i]);

}


void get_events_header (FILE* file, lprof_events_header_t* lprofEventsHeader)
{
   assert ( file!= NULL);

   //GET NB_THREADS
   if (fread (&lprofEventsHeader->nbThreads, sizeof(lprofEventsHeader->nbThreads), 1, file) != 1)
      DBGMSG0 ("Error while reading events header: threads nb\n");

   //GET NB_HWC
   if (fread (&lprofEventsHeader->nbHwc, sizeof(lprofEventsHeader->nbHwc), 1, file) != 1)
      DBGMSG0 ("Error while reading events header: events nb\n");

   //GET HWC LIST NAME
   get_string (file, &lprofEventsHeader->hwcListName);

   //GET EVENTS OFFSET
   if (fread (&lprofEventsHeader->eventsOffset, sizeof(lprofEventsHeader->eventsOffset), 1, file) != 1)
      DBGMSG0 ("Error while reading events header: offset\n");

   //GET ACCELERATION ARRAY
   //fread (&lprofEventsHeader->accelerationArray, sizeof(*lprofEventsHeader->accelerationArray), , file);

}

void print_events_header (lprof_events_header_t* lprofEventsHeader)
{
   fprintf(stderr , "\n------ MPERF_EVENTS_HEADER ------\n");
   fprintf(stderr , "nbThreads\t: %"PRId32"\n",lprofEventsHeader->nbThreads);
   fprintf(stderr , "nbHwc\t\t: %"PRId32"\n",lprofEventsHeader->nbHwc);
   fprintf(stderr , "hwcListName\t: %s\n",lprofEventsHeader->hwcListName);
   fprintf(stderr , "eventsOffset\t: %p\n",(void*)lprofEventsHeader->eventsOffset);
}

void get_events_info (FILE* file, uint32_t* nbThreads, lprof_events_info_t* lprofEventsInfo)
{
   assert (file != NULL);

   unsigned i;

   lprof_event_t* events = lc_malloc (*nbThreads * sizeof(*events));
   for (i=0; i< *nbThreads; i++)
   {
      get_event (file,&events[i]);
      //print_event(&events[i]);
   }
   lprofEventsInfo->events = events;

}

void print_events_info (lprof_events_info_t* lprofEventsInfo, uint32_t* nbThreads)
{
   assert (lprofEventsInfo != NULL);

   unsigned i;

   for (i=0; i< *nbThreads; i++)
      print_event (&lprofEventsInfo->events[i]);
}
