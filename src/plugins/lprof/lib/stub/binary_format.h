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

// CF binary_format.c

#ifndef __BINARY_FORMAT__H
#define __BINARY_FORMAT__H

#include <inttypes.h>

#define MAQAO_LPROF_MAGIC      "<LPROF>\0"
#define MAQAO_LPROF_MAGIC_SIZE 8
#define MAQAO_LPROF_VERSION_SIZE 4
#define MAQAO_LPROF_VERSION_MAJOR 2
#define MAQAO_LPROF_VERSION_MINOR 2
#define MAQAO_LPROF_VERSION "2.2\0"

#define OUTERMOST_LOOP 0
#define INNERMOST_LOOP 1
#define SINGLE_LOOP 2
#define INBETWEEN_LOOP 3

//#define MAQAO_PERF_VERSION(str1,str2) #str1"."#str2

#define MAX_LIBRARIES         64

//#define ADDRESS      uint64_t
////#define STRING(x)
//#define SRC_LINE     uint32_t
//#define OCCURRENCES   uint32_t
//#define OFFSET       uint64_t
//#define ID           uint32_t
//#define LEVEL        uint8_t

#define STR_ARRAY_OFFSET char*

enum sample_type
{
   LPROF_SAMPLE_IP,
   LPROF_SAMPLE_TID,
   LPROF_SAMPLE_TIME,
   LPROF_SAMPLE_ADDR,
   LPROF_SAMPLE_READ,
   LPROF_SAMPLE_CALLCHAIN,
   LPROF_SAMPLE_ID,
   LPROF_SAMPLE_CPU,
   LPROF_SAMPLE_PERIOD,
   LPROF_SAMPLE_STREAM_ID,
   LPROF_SAMPLE_RAW,
   LPROF_SAMPLE_BRANCH_STACK, // Since Linux 3.4
   LPROF_SAMPLE_REGS_USER,    // Since Linux 3.7
   LPROF_SAMPLE_STACK_USER,   // Since Linux 3.7
   LPROF_SAMPLE_WEIGHT,       // Since Linux 3.10
   LPROF_SAMPLE_DATA_SRC,     // Since Linux 3.10
   LPROF_SAMPLE_IDENTIFIER,   // Since Linux 3.12
   LPROF_SAMPLE_TRANSACTION,  // Since Linux 3.13
   LPROF_SAMPLE_REGS_INTR     // Since Linux 3.19
};

// ALL INFO TO DESCRIBE A BLOCK
typedef struct lprof_block_s
{
   uint64_t   blockId;
   uint64_t   startAddress;        // The starting assembly addresses of the block
   uint64_t   stopAddress;         // The stoping assembly addresses of the block
} lprof_block_t;

// ALL THE STRINGS ARE ADDED INTO THIS SERIALIZED STRING ARRAY
typedef struct lprof_serialized_str_s {
   uint64_t nbCharacters;
   char*    serializedStr;
} lprof_serialized_t;

//FUNCTION TYPE : ALL INFO TO DESCRIBE A FUNCTION
typedef struct lprof_function_s
{
   STR_ARRAY_OFFSET       name;    // The name of the function
   uint32_t    nbParts;             // The parts number of the function (Monoblock fct = 1, Splitted fct = 2+)
   uint64_t*   startAddress;        // The starting assembly addresses of the function
   uint64_t*   stopAddress;         // The stoping assembly addresses of the function
   STR_ARRAY_OFFSET       srcFile; // The source file name where is coded the function
   uint32_t    srcLine;             // The source line of the function declaration
   uint32_t    nbOutermostLoops;    // The number of outermost loops in this function
   uint32_t*   outermostLoopsList;  // The list which contains outermost loops id;
}lprof_fct_t;

//LOOP TYPE : ALL INFO TO DESCRIBE A LOOP
typedef struct lprof_loop_s
{
   uint32_t  id;                           // The loop id in maqao
   uint32_t  nbParts;                      // The parts number of the function (Monoblock loop = 1, Splitted loop = 2+)
   uint64_t* startAddress;                 // Starting assembly addresses of the loop
   uint64_t* stopAddress;                  // Stoping assembly addresses of the loop
   uint32_t  nbBlocks;                     // The number of blocks of the loop
   lprof_block_t*  blockIds;               // Array of blocks
   STR_ARRAY_OFFSET     srcFile;          // Source file name containing the loop
   STR_ARRAY_OFFSET     srcFunctionName;  // The function name containing the loop
   uint32_t  srcFunctionLine;              // Source line of the function containing the loop
   uint32_t  srcStartLine;                 // The starting source line of the loop
   uint32_t  srcStopLine;                  // The stopping source line of the loop
   uint8_t   level;                        // The loop level (Outermost (0)/ Innermost(1)/ Single(2)/ In Between(3))
   uint32_t  nbChildren;                   // The number of children loops (0 if innermost)
   uint32_t* childrenList;                 // The list which contains the children
}lprof_loop_t;

typedef struct {
   char *name;
   uint64_t startMapAddress;
   uint64_t stopMapAddress;
} lib_range_t;

//LIBRARY TYPE : ALL INFO TO DESCRIBE A MAPPED LIBRARY
typedef struct lprof_library_s
{
   STR_ARRAY_OFFSET          name;  // The library name
   uint64_t       nbProcesses;      // Tje number of processes
   uint64_t*      startMapAddress;  // The mapped starting adress of the library for each process
   uint64_t*      stopMapAddress;   // The mapped stopping address of the binary for each process
   uint32_t       nbFunctions;      // The number of functions of the library
   uint32_t       nbLoops;          // The number of loops of the library
   lprof_fct_t*   fctsInfo;         // The array containing all the library's functions info.
   lprof_loop_t*  loopsInfo;        // The array containing all the library's loops info.
}lprof_library_t;

typedef struct call_chain_s
{
   uint32_t    nbFrames;         // The number of  frames stored in the callchain (number of addresses in the backtrace).
   uint64_t*   addressList;      // The array containing all the addresses of the callchain
}call_chain_t;

//SAMPLE TYPE : ALL INFO TO DESCRIBE A SAMPLE OBTAINED VIA PERF_EVENT_OPEN
typedef struct lprof_sample_s
{
   // #V1.1
   uint64_t       address;          // The address (Instruction Pointer) recorded.
   uint32_t       nbOccurrences;     // The number of occurrences.
   uint32_t       nbCallChains;     // The number of  callchains for this address.
   call_chain_t*  callChain;        // The callchains for this address (stack backtraces)
   uint64_t  nbCpuIds;              // The number of CPU id saved.
   uint32_t* cpuIdsList;            // The CPU id list

}lprof_sample_t;

//EVENT TYPE : ALL INFO TO DESCRIBE AN EVENT
typedef struct lprof_event_s
{
   STR_ARRAY_OFFSET     hwcName;   // The hardware counter name.
   uint32_t  threadId;              // The id of the sampled thread.
   uint64_t  sampleDescriptor;      // The the id of the info stored into the sample_t (cf. sample_type enum)
   uint32_t  nbSamples;             // The number of collected samples.
   lprof_sample_t* samples;         // The array containing all the samples collected for this hwc.
}lprof_event_t;


//LPROF HEADER SECTION
typedef struct lprof_header_s {
   char    magic[MAQAO_LPROF_MAGIC_SIZE];      // Magic Number : MAQAO_LPROF_MAGIC
   char    version[MAQAO_LPROF_VERSION_SIZE];  // Version of the mlprof format
   uint64_t binaryInfoHeaderOffset;             // Offset in the file of the binary info header section.
   uint64_t libraryInfoHeaderOffset;            // Offset in the file of the library info header section
   uint64_t eventsHeaderOffset;                 // Offset in the file of the events header.
   uint64_t serializedStrOffset;                // Offset in the file of the serialized string.
} lprof_header_t;

//BINARY INFO HEADER SECTION
typedef struct lprof_binary_info_header_s {
   STR_ARRAY_OFFSET    binName;          // Name of the binary
   uint32_t nbFunctions;                  // Number of disassembled functions in the binary.
   uint32_t nbLoops;                      // Number of disassembled loops in the binary.
   uint64_t fctsInfoOffset;               // Offset in the file of the functions info section.
   uint64_t loopsInfoOffset;              // Offset in the file of the loops info section.
   uint64_t serializedStrOffset;          // Offset in the file of the serialized string.
   //ACCELERATION_TABLE
} lprof_binary_info_header_t;

//LIBRARIES INFO HEADER SECTION
typedef struct lprof_libraries_info_header_s {
   uint32_t nbLibraries;                        // Number of disassembled libraries.
   uint64_t librariesInfoOffset;                // Offset in the file of the libraries info section.
   uint64_t accelerationArray[MAX_LIBRARIES];   // Acceleration table where starting offset of each library is stored.
                                                // It permits to parse in parallel the libraries'info.
   uint64_t serializedStrOffset;                // Offset in the file of the serialized string.
} lprof_libraries_info_header_t;

//EVENTS HEADER SECTION
typedef struct lprof_events_header_s {
   uint32_t nbThreads;                 // Number of catched threads.
   uint32_t nbHwc;                     // Number of hardware counters to read per threads.
   STR_ARRAY_OFFSET    hwcListName;   // The name of hardware counters used.
   uint64_t eventsOffset;              // Offset in the file of the events section.
   //ACCELERATION_TABLE
} lprof_events_header_t;


//BINARY INFO SECTION
typedef struct lprof_binary_info_s {
   lprof_fct_t*   functions;  // Array containing the functions info of the binary.
   lprof_loop_t*  loops;      // Array containing the loops info of the binary.
} lprof_binary_info_t;

//LIBRARIES INFO SECTION
typedef struct lprof_libraries_info_s {
   lprof_library_t*  libraries;  // Array containing the libraries info of the binary.
} lprof_libraries_info_t;

//EVENTS INFO SECTION
typedef struct lprof_events_info_s {
   lprof_event_t*    events;     // Array containing the event info for each threads which have been sampled.
} lprof_events_info_t;


int write_sample (FILE* file, uint64_t sampleDescriptor, lprof_sample_t* mySample);
int write_lprof_file();
int write_binary_info_header (FILE* file, char* binName, uint32_t nbFunctions, uint32_t nbLoops);
int write_binary_info (FILE* file, lprof_binary_info_t* binaryInfo, uint32_t nbFunctions, uint32_t nbLoops);

int init_function (  lprof_fct_t* myFct,
                     char* name,
                     uint32_t nbParts,
                     uint64_t* startAddr,
                     uint64_t* stopAddr,
                     char* srcFile,
                     uint32_t srcLine,
                     uint32_t nbOutermostLoops,
                     uint32_t* outermostLoopsList);

int write_function (FILE* file, lprof_fct_t* myFct);
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
                uint32_t* childrenList );

int write_loop (FILE* file, lprof_loop_t* myLoop);
int write_libraries_info_header (FILE* file,uint32_t nbLibraries, uint64_t startingLibrariesOffset);
int write_libraries_info (FILE* file);
int init_library (lprof_library_t* myLib, const char* name, uint64_t* startMapAddr, uint64_t* stopMapAddr, uint32_t nbFunctions, uint32_t nbLoops, lprof_fct_t* fctsInfo, lprof_loop_t* loopsInfo);
int write_library (FILE* file, lprof_library_t* myLib);
int write_events (FILE* file);
int init_sample (lprof_sample_t* mySample, uint64_t addr, uint32_t nbOccurrences, uint32_t nbFrames, call_chain_t* callChains);

int init_event (lprof_event_t* myEvent, char* hwcName, uint32_t threadId, uint32_t nbSamples, uint64_t sampleTypes, lprof_sample_t* samples);
long write_event (FILE* file, lprof_event_t* myEvent);
void update_event (FILE* file, lprof_event_t* myEvent, long filePosition);

long write_events_header (FILE* file, uint32_t nbThreads, uint32_t nbHwc, char* hwcListName);
void update_events_header_nb_threads (FILE* file, uint32_t nbThreads, long filePosition);


int write_string (FILE* file, const char* string);
void get_string (FILE* file, char** string);

void get_loop (FILE* file, lprof_loop_t* myLoop);
void print_loop (lprof_loop_t* myLoop);

void get_function (FILE* file, lprof_fct_t* myFct);
void print_function (lprof_fct_t* myFct);

void get_library (FILE* file, lprof_library_t* myLib);
void print_library (lprof_library_t* myLib);

void get_event (FILE* file, lprof_event_t* myEvent);
void print_event (lprof_event_t* myEvent);

void get_sample (FILE* file, uint64_t sampleDescriptor, lprof_sample_t* mySample);
void print_sample (lprof_sample_t* mySample,uint64_t sampleDescriptor);

long write_lprof_header (FILE* file);
void get_lprof_header (FILE* file, lprof_header_t* lprofHeader);
void update_lprof_header (FILE* file, uint64_t serializedStrOffset, long filePosition);
long write_serialized_str_array (FILE* file);
void get_serialized_str_array (FILE* file, uint64_t size ,long offset);

void print_lprof_header (lprof_header_t* lprofHeader);

void get_bin_info_header (FILE* file, lprof_binary_info_header_t* lprofBinInfoHeader);
void print_bin_info_header (lprof_binary_info_header_t* lprofBinInfoHeader);


void get_bin_info (FILE* file, uint32_t* nbFunctions, uint32_t* nbLoops, lprof_binary_info_t* lprofBinInfo);
void print_bin_info (lprof_binary_info_t* lprofBinInfo, uint32_t* nbFunctions, uint32_t* nbLoops);

void get_libs_info_header (FILE* file, lprof_libraries_info_header_t* lprofLibInfoHeader);
void print_libs_info_header (lprof_libraries_info_header_t* lprofLibInfoHeader);

void get_libs_info (FILE* file, uint32_t* nbLibraries, lprof_libraries_info_t* lprofLibsInfo);
void print_libs_info (lprof_libraries_info_t* lprofLibsInfo, uint32_t* nbLibraries);


void get_events_header (FILE* file, lprof_events_header_t* lprofEventsHeader);
void print_events_header (lprof_events_header_t* lprofEventsHeader);

void get_events_info (FILE* file, uint32_t* , lprof_events_info_t* lprofEventsInfo);
void print_events_info (lprof_events_info_t* lprofLibsInfo, uint32_t* nbThreads);

void get_call_chain (FILE* file, call_chain_t* myCallChain);
void print_call_chain (call_chain_t* myCallChain);
int write_call_chain (FILE* file, call_chain_t* myCallChain);
#endif
