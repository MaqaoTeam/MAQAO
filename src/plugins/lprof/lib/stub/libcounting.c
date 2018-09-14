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
 * Defines the libcounting library probes to insert in a target executable
 * to measure HW events (via perf-events). Probes can be inserted by using the MADRAS API.
 * Instrumented code has to be run under LD_PRELOAD=path/to/libcounting.so
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <err.h>
#include <errno.h>

#include "libcounting.h"
#include "utils.h"
#include "deprecated_shared.h"

#define MAX_LENGTH 512

//DRAM COUNTERS
#define BAR_OFFSET (0x0048)
#define DRAM_DATA_READS (0x5050)
#define DRAM_DATA_WRITES (0x5054)
#define MMAP_SIZE (0x6000)


/**
 ** Macro used for setting a given sub value inside a flag
 ** \param FLAG The value of the flag to update
 ** \param VALUE The sub value to set
 ** \param POS Position (in bits) of the sub value, starting from the less significant bit of the flag
 ** \param SIZE Size (in bits) of the sub value
 ** */
#define FLAG_UPDSUBVALUE(FLAG, VALUE, POS, SIZE) ((FLAG & ~((typeof(FLAG))(((1<<SIZE)-1)<<POS))) | ((VALUE << POS) &  (((1<<SIZE)-1)<<POS)))

//LIST OF ALL STRINGS FLAGS ACCEPTED
static char* str_flags_opt[IDX_MAX_FLAG] = {"event=","umask=","usr=","os=","e=","pc=","int=","any=","en=","inv=","cmask="};
// FLAG IDX TO BIT POSITION INTO THE PMU
static int bit_flags_position[IDX_MAX_FLAG] = {BIT_EVENT_SELECT,BIT_UNIT_MASK,BIT_USR_FLAG,BIT_OS_FLAG,BIT_EDGE_DETECT_FLAG,BIT_PIN_CONTROL_FLAG,BIT_INT_FLAG,BIT_ANY_FLAG,BIT_ENABLE_COUNTERS_FLAG,BIT_INV_FLAG,BIT_COUNTER_MASK};

SinstruInfo instruInfo;
char* mmapBar = NULL;

// Called in dead code... Still useful ?
/* static int check_cpu_platform () */
/* { */
/*    int platform_model = 0; */
/*    char line[MAX_BUF_SIZE] = ""; */
/*    FILE* fCpuInfo = fopen("/proc/cpuinfo","r"); */

/*    if (fCpuInfo != NULL) */
/*    { */
/*       while (fgets(line, MAX_BUF_SIZE, fCpuInfo) != NULL) */
/*       { */
/*          if (strstr(line,"model name")) */
/*          { */
/*             if (strstr(line,"Xeon")) */
/*             { */
/*                platform_model = INTEL_SERVER_CPU; */
/*             } */
/*             else */
/*             { */
/*                platform_model = INTEL_DESKTOP_CPU; */
/*             } */
/*          } */
/*       } */
/*       fclose(fCpuInfo); */
/*    } */

/*    return platform_model; */
/* } */


static void check_avaibility_uncore_events(int type)
{
   FILE* fType = NULL;

   //if (check_cpu_platform () != INTEL_SERVER_CPU)
   //{
   //   fprintf(stderr,"[MAQAO] These type of events (uncore) are only available on Xeon CPUs!");
   //   fprintf(stderr,"[MAQAO] If you want to acces to RAM events on a desktop CPU use these events :\n"
   //                           "\t -DRAM_DATA_READS\n"
   //                           "\t -DRAM_DATA_WRITES\n");
   //   exit(-1);
   //}

   switch (type)
   {
   case UNCORE_IMC_0 :
      fType = fopen(PATH_IMC_0_TYPE,"r");
      break;
   case UNCORE_IMC_1 :
      fType = fopen(PATH_IMC_1_TYPE,"r");
      break;
   case UNCORE_IMC_2 :
      fType= fopen(PATH_IMC_2_TYPE,"r");
      break;
   case UNCORE_IMC_3 :
      fType = fopen(PATH_IMC_3_TYPE,"r");
      break;
   default:
      return; //
      //fprintf(stderr,"[MAQAO] Unknown type of uncore event\n");
      //available = -1;
   }

   if (fType == NULL)
   {
      fprintf(stderr ,"[MAQAO] Your kernel is too old to deal with uncore performance counters\n");
      fprintf(stderr ,"[MAQAO] You can try to update your kernel (at least 2.6.38) \n");
      exit(-1);
   }
   fclose (fType);

   return;
}

//Check String Format : COUNTER_NAME_1[,COUNTER_NAME_2...]
static int check_string_format (char* string)
{
   if (string == NULL)
   {
      fprintf(stderr,"ERROR : String == NULL\n");
      return -1;
   }

   size_t length = strlen (string);
   if (length == 0)
   {
      fprintf(stderr,"ERROR : String is empty\n");
      return -2;
   }
   if (strchr(string+length-1,','))
   {
      fprintf(stderr,"ERROR : Bad format for string\n");
      return -3;
   }
   return 0;
}

// UPDATE BIT VALUE OF struct perf_event_attr->config
// CF : Layout of IA32_PERFEVTSELx MSRs from the Intel64_developer_model_vol3b.pdf
void apply_flags (unsigned hwcIdx, perf_event_desc_t* eventInfo)
{
   int flagIdx;
   int strLength = 0;
   char* tmpCounterName = NULL;

   // SEARCH ALL FLAGS
   for ( flagIdx =0; flagIdx < IDX_MAX_FLAG; flagIdx++)
   {
      //FLAG HAS BEEN SET BY THE USER
      if (instruInfo.countersFlags[hwcIdx].value[flagIdx] != 0)
      {
         // GET THE SIZE IN BIT OF THE SUB-VALUE
         unsigned int size = (unsigned int)log2(instruInfo.countersFlags[hwcIdx].value[flagIdx]) + 1;
         // APPLY THE FLAG TO THE EVENT
         eventInfo->hw.config = FLAG_UPDSUBVALUE(eventInfo->hw.config, instruInfo.countersFlags[hwcIdx].value[flagIdx], bit_flags_position[flagIdx], size);
         //FIND THE RIGHT SIZE : +4 for ':' + '0x' + '\0'
         strLength = strlen (instruInfo.counterNames[hwcIdx]) + strlen(str_flags_opt[flagIdx])+ 4;
         tmpCounterName = realloc (instruInfo.counterNames[hwcIdx],strLength);
         if ( tmpCounterName == NULL)
         {
            //REALLOC ERROR
            perror("ERROR : Realloc in apply_flags");
         }
         instruInfo.counterNames[hwcIdx] = tmpCounterName;
         //ADD TO THE COUNTER NAME THE FLAG (HWC_NAME:STR_FLAG_OPT=0xVALUE)
         sprintf(instruInfo.counterNames[hwcIdx],"%s:%s0x%"PRIx64"",instruInfo.counterNames[hwcIdx],str_flags_opt[flagIdx],instruInfo.countersFlags[hwcIdx].value[flagIdx]);
      }
   }
}

//INIT SInstruParameters STRUCT
static void init_instruInfo (void)
{
   instruInfo.nbCallSites = 0;
   instruInfo.collectedInfo = NULL;
   instruInfo.accumulateSamples = NULL;
   instruInfo.counterInfo = NULL;
   instruInfo.nbCounters = 0;
   instruInfo.nbRawCounters = 0;
   instruInfo.counterNames = NULL;
   instruInfo.counterCore = NULL;
   instruInfo.counterPid = NULL;
   instruInfo.pfmFds = NULL;
   instruInfo.nbGroups = 0;
   instruInfo.nbCountersPerGroup = NULL;
   instruInfo.hwcIdxToGroup = NULL;
   instruInfo.groupIdxToPfmFds = NULL;
}

void set_struct_instruInfo (unsigned int nbCallSites)
{
   static int init_done = 0;
   unsigned int i,j;
   int groupIdx;

   if (init_done == 0)
   {
      instruInfo.nbCallSites = nbCallSites;
      instruInfo.counterInfo = malloc (nbCallSites * sizeof(*instruInfo.counterInfo));
      instruInfo.collectedInfo = malloc (nbCallSites * sizeof(*instruInfo.collectedInfo));
      instruInfo.accumulateSamples = malloc (nbCallSites * sizeof(*instruInfo.accumulateSamples));
      for (i=0; i < instruInfo.nbCallSites; i++)
      {
         instruInfo.counterInfo[i] = NULL;
         instruInfo.counterInfo[i] = realloc (instruInfo.counterInfo[i],
                                              instruInfo.nbCounters * sizeof(*(instruInfo.counterInfo[i])));
         instruInfo.collectedInfo[i] = queue_new();
         instruInfo.accumulateSamples[i] = malloc (instruInfo.nbCounters * sizeof(*instruInfo.accumulateSamples[i]));
         for (j=0; j< instruInfo.nbCounters; j++)
            instruInfo.accumulateSamples[i][j] = 0;
      }
      instruInfo.bufferSize = malloc (instruInfo.nbGroups * sizeof (*instruInfo.bufferSize));
      instruInfo.buffer = malloc (instruInfo.nbGroups * sizeof (*instruInfo.buffer));
      //SIZE OF A THE ACTUAL GROUP TO READ (cf. start_counting / stop_counting)
      for ( groupIdx = 0; groupIdx < instruInfo.nbGroups; groupIdx++ )
      {
         instruInfo.bufferSize[groupIdx] = sizeof(uint64_t) * (3 + (instruInfo.nbCountersPerGroup[groupIdx] * 2));
         instruInfo.buffer[groupIdx]     = malloc ( instruInfo.bufferSize[groupIdx] );
      }
      init_done = 1;
   }
}

// INITIALIZATION OF THE HWC
void counting_start_counters(unsigned int nbCallSites)
{
   unsigned int i;
   int ret;
   int arch, uarch;
   int64_t rawCode = 0;

   set_struct_instruInfo(nbCallSites);
   uarch = get_uarch(&arch);

   for (i=0; i < instruInfo.nbCounters; i++)
   {
      if (strncmp(instruInfo.counterNames[i],"DRAM_DATA",9) != 0) //NOT DRAM COUNTERS?
      {
         perf_event_desc_t eventInfo;
         memset(&(eventInfo.hw),0,sizeof(struct perf_event_attr));

         //RAW COUNTER IN THE LIST ?
         rawCode = perf_utils_readhex (instruInfo.counterNames[i]);
         if (rawCode != 0)
         {
            eventInfo.hw.config = rawCode;
            eventInfo.hw.type = instruInfo.counterPerfTypes[i];
         }
         else
         {
            // Encode the HWC (PMU info)
            maqao_get_os_event_encoding(arch,uarch,&eventInfo,instruInfo.counterNames[i],-1,NULL, TRUE);
         }

         // APPLY FLAGS
         apply_flags (i,&eventInfo);

         // request scaling in case of shared counters
         eventInfo.hw.read_format =    PERF_FORMAT_TOTAL_TIME_ENABLED |
            PERF_FORMAT_TOTAL_TIME_RUNNING |
            PERF_FORMAT_ID                 |
            PERF_FORMAT_GROUP;
         eventInfo.hw.size = sizeof(struct perf_event_attr);

         // EXCLUDE KERNEL EVENT ? APIC
         // WARNING : NOT COMPATIBLE WITH UNCORE EVENTS!
         if (eventInfo.hw.type == PERF_TYPE_RAW)
            eventInfo.hw.exclude_kernel = 1; //FOR CORE EVENTS
         else
            eventInfo.hw.exclude_kernel = 0; //FOR UNCORE EVENTS

         int group = instruInfo.hwcIdxToGroup[i];

         // Open the file descriptor
         if (( eventInfo.hw.type != PERF_TYPE_RAW) || (instruInfo.counterCore[i] != -1) ) //UNCORE,CBOX EVENTS
         {
            eventInfo.hw.exclude_kernel = 0; //FOR UNCORE EVENTS
            check_avaibility_uncore_events(eventInfo.hw.type);
            instruInfo.pfmFds[i] = perf_event_open (&(eventInfo.hw), -1, instruInfo.counterCore[i], instruInfo.pfmFds[group], 0);
         }
         else
         {
            instruInfo.pfmFds[i] = perf_event_open (&(eventInfo.hw), instruInfo.counterPid[i], -1, instruInfo.pfmFds[group], 0); //CORE EVENTS
         }

         if (instruInfo.pfmFds[i] == -1)
         {
            //utils_print_struct_event_attr(&(eventInfo.hw));
            fprintf(stderr,"ERROR = %s (%d)\n",strerror(errno),instruInfo.pfmFds[i]);
            err(1, "ERROR : Cannot open counter %s", instruInfo.counterNames[i]);
            //CALL FREE FUNCTION
            //free(instruInfo);
            exit(-1);
         }

         ret = ioctl(instruInfo.pfmFds[i], PERF_EVENT_IOC_ENABLE, 0);
         if (ret)
         {
            err(1, "Cannot enable event %s\n", instruInfo.counterNames[i]);
            //CALL FREE FUNCTION
            //free(instruInfo);
            exit(-1);
         }
      }
      else //DRAM COUNTERS
      {
         if (mmapBar == NULL)
         {
            int fdMem, fdPci;
            uint64_t imcBar = 0;

            //OPEN /dev/mem FILE
            fdMem = open ("/dev/mem", O_RDONLY);
            if (fdMem < 0)
            {
               perror("ERROR : DRAM COUNTERS NEED SUDO PERMISSION ");
               exit(-1);
            }

            //OPEN pci FILE
            fdPci = open ("/proc/bus/pci/00/00.0", O_RDWR);
            if (fdPci < 0)
            {
               perror("ERROR : ");
               exit(-1);
            }

            if (pread (fdPci,(void *)&imcBar,sizeof(uint64_t),BAR_OFFSET) == -1) {
               DBGMSG0 ("Cannot read /proc/bus/pci/00/00.0\n");
               perror("pread");
               exit (-1);
            }
            if (imcBar == 0)
            {
               exit(-1);
            }

            uint64_t startAddr = imcBar & (~(4096-1)); // round down to 4K (PAGE ALIGNMENT)

            //MAP THE /dev/mem FILE
            mmapBar = mmap (NULL, MMAP_SIZE, PROT_READ, MAP_SHARED, fdMem, startAddr);
            if (mmapBar == MAP_FAILED)
            {
               perror("ERROR : MMAP BAR : ");
               //return EXIT_FAILURE;
            }
         }
      }
   }

   return;
}



// Parse Flags
//  - USR flag
//  - OS flag
//  - Edge Dectect (E)
//  - Pin Control (PC)
//  - INT (APIC interrupt enable) flag
//  - Enable Counters (EN) flag
//  - Inversion (IN) flag
//  - Counter mask (CMASK) field
void parse_flags (char* startFlags, unsigned hwcIdx)
{

   if (startFlags == NULL)
      perror("EVENT FLAGS FORMAT IS NOT CORRECT!\n");

   char* end;
   const char* strFlagValue = NULL;
   int64_t convertedFlag = 0;
   unsigned flagIdx = -1;

   while ( *startFlags != '\0'  )
   {
      for (flagIdx = 0; flagIdx < IDX_MAX_FLAG; flagIdx++)
      {
         // GET LENGTH OF THE FLAG OPTION (See str_flags_opt char array)
         size_t strLength = strlen(str_flags_opt[flagIdx]);

         if (strncmp (startFlags,str_flags_opt[flagIdx],strLength) == 0)
         {
            // FLAG VALUE POSITION
            // "cmask=0x5" --> we need to be placed after "cmask=" to get 0x5
            strFlagValue = startFlags + strLength;

            convertedFlag = strtoll(strFlagValue,&end,16);
            if (strFlagValue != end)
            {
               instruInfo.countersFlags[hwcIdx].value[flagIdx] = convertedFlag;
            }
            else
               perror("ERROR WITH FLAG PARAMETER IN YOUR HWC LIST");

            break; //WE FOUND THE FLAG SO WE CAN SEARCH THE NEXT ONE (IF AVAILABLE)
         }

      }
      //GET THE NEXT FLAG
      if (*end != '\0')
         startFlags = end+1;
      else // NO MORE FLAG
         startFlags = end;
   }
}

//PARSE ONE HWC NAME IN THE LIST
//FORMAT : HWC[@TYPE][-FLAG=VALUE,...]
//         HWC    : A name or the raw code (0x...)
//         TYPE   : Type of the event (Core, Uncore, Cbox etc.)
//         FLAG   : Get the flags of the event if specified by the user. (See parse_flags)
void parse_hwc_name (char* startWord, unsigned hwcIdx)
{
   char* isAt = NULL;
   char* end;
   int64_t convertedType = 0;

   // THE USER HAS SPECIFIED THE TYPE
   if ( (isAt = strchr(startWord,'@')) )
   {
      *isAt = '\0'; //REPLACE EACH '@' by '\0'
      instruInfo.counterNames[hwcIdx] = strdup(startWord); //GET NAME OR RAW CODE
      const char* strType = (isAt+1);
      convertedType = strtoll(strType,&end,10);
      if (strType != end)
         instruInfo.counterPerfTypes[hwcIdx] = convertedType;
      else
         fprintf(stderr,"ERROR WITH TYPE PARAMETER IN YOUR HWC LIST\n");

      // CHECK FOR FLAGS
      if ( (isAt = strchr(end,'-')) )
      {
         *isAt = '\0'; //REPLACE THE FIRST '-' by '\0'
         parse_flags(isAt+1,hwcIdx);
      }
   }
   // NO TYPE
   else
   {
      //CHECK FOR FLAGS
      if ( (isAt = strchr(startWord,'-')) )
      {
         *isAt = '\0'; //REPLACE THE FIRST '-' by '\0'
         parse_flags(isAt+1,hwcIdx);
      }
      instruInfo.counterNames[hwcIdx] = strdup(startWord);
      instruInfo.counterPerfTypes[hwcIdx] = PERF_TYPE_RAW; //DEFAULT TYPE (4)
   }

   // GET INTERNAL TYPE FOR THE HWC
   if (strncmp(instruInfo.counterNames[hwcIdx],"DRAM_DATA_READS",15) == 0)
      instruInfo.counterLibTypes[hwcIdx] = TYPE_DATA_READS;
   else if (strncmp(instruInfo.counterNames[hwcIdx],"DRAM_DATA_WRITES",16) == 0)
      instruInfo.counterLibTypes[hwcIdx] = TYPE_DATA_WRITES;
   else
   {
      instruInfo.nbRawCounters++;
      instruInfo.counterLibTypes[hwcIdx] = TYPE_RAW;
   }

}


//ORDER THE HWC LIST GIVEN BY THE USER
// --> THE DRAM COUNTERS ARE ALWAYS PUT AT THE END OF THE HWC LIST
//     IT PERMITS TO REDUCE THE OVERHEAD WITH THE START/STOP PROBES
char* sort_hwc_list (char* hwcList)
{
   char* sortedHwcList = calloc ((strlen (hwcList) + 1) , sizeof(char));
   char* dramHwcList    = calloc ((strlen (hwcList) + 1) , sizeof(char));
   char* copyHwcList = strdup(hwcList);
   char* endWord   = copyHwcList;
   char* beginWord = copyHwcList;

   unsigned int nbDrams = 0; // Number of Dram counters
   unsigned int nbRaws  = 0; // Number of Raw counters

   memset (sortedHwcList, 0, (strlen (hwcList) + 1) * sizeof(char));
   memset (dramHwcList, 0, (strlen (hwcList) + 1) * sizeof(char));

   // PARSE THE STRING UNTIL THE NEXT ','
   while ((endWord = strchr(endWord,',')))
   {
      //DRAM
      if (strncmp (beginWord,"DRAM_",5) == 0)
      {
         //ADD IN DRAM LIST
         strncat(dramHwcList, beginWord, endWord-beginWord);
         strcat(dramHwcList,",");
         nbDrams++;
      }
      else
      {
         //ADD IN THE ORDERED LIST
         strncat(sortedHwcList, beginWord, endWord-beginWord);
         strcat(sortedHwcList,",");
         nbRaws++;
      }
      endWord++;//to be placed just after the ','
      beginWord = endWord;
   }

   // STRING DOES NOT TERMINATE WITH A ','
   // SO WE NEED TO PARSE THE LAST WORD
   if (strncmp (beginWord,"DRAM_",5) == 0)
   {
      //ADD LAST COUNTER
      strcat(dramHwcList, beginWord);
      //CONCATENATE THE DRAM LIST A THE END
      strcat(sortedHwcList,dramHwcList);
   }
   else
   {
      //ADD LAST COUNTER
      strcat(sortedHwcList, beginWord);
      // IF WE HAVE DRAM COUNTERS WE NEED TO ADD ',' AT THE END OF
      // THE ORDERED LIST BEFORE TO CONCATENATE THE DRAM LIST
      if (nbDrams > 0)
         strcat(sortedHwcList,",");
      // WE NEED TO REMOVE THE ','
      if (nbRaws != 0 && nbDrams > 0)
         dramHwcList[strlen(dramHwcList)-1] = '\0';
      strcat(sortedHwcList,dramHwcList);
   }

   free(copyHwcList);
   free(dramHwcList);

   return sortedHwcList;
}

//ADD HARDWARE COUNTERS IN THE HWC LIST TO MONITOR
unsigned int counting_add_hw_counters (char* hwcList,int core, int pid)
{
   static int init_done = 0;

   if (hwcList == NULL)
      return 0;

   if (check_string_format (hwcList))
      exit(-1);

   if (!init_done)
   {
      init_instruInfo();
      init_done = 1;
   }

   unsigned int i;
   unsigned int hwcIdx = instruInfo.nbCounters;
   unsigned int prevNbCounters = instruInfo.nbCounters;
   unsigned int prevNbRawCounters = instruInfo.nbRawCounters;

   //ORDER THE LIST GIVEN BY THE USER TO PUT THE DRAM AT THE END
   // --> IT PERMITS TO REDUCE PROBE START/STOP OVERHEAD
   char* sortedHwcList = sort_hwc_list(hwcList);

   char* copyHwcList = strdup(sortedHwcList);
   char* curPos = copyHwcList;
   char* startWord = copyHwcList;


   //DETERMINE THE NUMBER OF COUNTER IN THE LIST
   instruInfo.nbCounters++;
   while ((curPos = strchr(curPos,',')))
   {
      instruInfo.nbCounters++;
      curPos++;//to be placed just after the ','
   }

   int32_t* tmpCounterCore = realloc (instruInfo.counterCore, instruInfo.nbCounters * sizeof(*tmpCounterCore));
   if ( tmpCounterCore == NULL)
   {
      //REALLOC ERROR
      free (instruInfo.counterCore);
      return -2;
   }
   instruInfo.counterCore = tmpCounterCore;

   int32_t* tmpCounterPid = realloc (instruInfo.counterPid, instruInfo.nbCounters * sizeof(*tmpCounterCore));
   if ( tmpCounterPid == NULL)
   {
      //REALLOC ERROR
      free (instruInfo.counterPid);
      return -2;
   }
   instruInfo.counterPid = tmpCounterPid;

   int* tmpPfmFds = realloc (instruInfo.pfmFds, instruInfo.nbCounters * sizeof(*instruInfo.pfmFds));
   if ( tmpPfmFds == NULL)
   {
      //REALLOC ERROR
      return -2;
   }
   instruInfo.pfmFds = tmpPfmFds;

   int* tmpHwcIdxToGroup = realloc (instruInfo.hwcIdxToGroup, instruInfo.nbCounters * sizeof(*tmpHwcIdxToGroup));
   if ( tmpHwcIdxToGroup == NULL)
   {
      return -2;
   }
   instruInfo.hwcIdxToGroup = tmpHwcIdxToGroup;



   //ALLOCATE THE counterNames ARRAY
   char** tmp = realloc (instruInfo.counterNames,instruInfo.nbCounters * sizeof(char*));
   if ( tmp == NULL)
   {
      //REALLOC ERROR
      for (i=0; i < instruInfo.nbCounters; i++)
         free(instruInfo.counterNames[i]);
      free (instruInfo.counterNames);
      return -2;
   }
   instruInfo.counterNames = tmp;

   //ALLOCATE THE counterFlags ARRAY
   ScounterFlags* tmpFlags = realloc (instruInfo.countersFlags,instruInfo.nbCounters * sizeof(*tmpFlags));
   if ( tmpFlags == NULL)
   {
      //REALLOC ERROR
      return -2;
   }
   instruInfo.countersFlags = tmpFlags;

   //PERF TYPE
   uint32_t* tmpPerfTypes = realloc (instruInfo.counterPerfTypes, instruInfo.nbCounters * sizeof(*tmpPerfTypes));
   if ( tmpPerfTypes == NULL)
   {
      //REALLOC ERROR
      free (instruInfo.counterPerfTypes);
      return -2;
   }
   instruInfo.counterPerfTypes = tmpPerfTypes;

   // LIBCOUNTING INTERNAL TYPE LIST
   uint32_t* tmpTypes = realloc (instruInfo.counterLibTypes, instruInfo.nbCounters * sizeof(*tmpTypes));
   if ( tmpTypes == NULL)
   {
      //REALLOC ERROR
      free (instruInfo.counterLibTypes);
      return -2;
   }
   instruInfo.counterLibTypes = tmpTypes;


   //ADD THE COUNTER NAMES IN THE LIST
   char* isComma = NULL;
   while ( (isComma = strchr(startWord,',')) )
   {
      *isComma = '\0';
      memset (instruInfo.countersFlags[hwcIdx].value,0,IDX_MAX_FLAG * sizeof(int64_t));
      // INIT countersFlags ARRAY FOR THE CURRENT HWC
      parse_hwc_name (startWord, hwcIdx);
      startWord = isComma+1;
      hwcIdx++;
   }
   //INIT countersFlags ARRAY FOR THE CURRENT HWC AND PARSE THE LAST ONE
   memset (instruInfo.countersFlags[hwcIdx].value,0,IDX_MAX_FLAG * sizeof(int64_t));
   parse_hwc_name (startWord, hwcIdx);

   //instruInfo.counterNames[i] = strdup(startWord);


   // counting_add_hw_counters CAN BE CALLED MULTIPLE TIMES.
   // EACH TIME, WE CREATE A GROUP OF HWC. EACH HWC OF A GROUP WILL BE START/STOP AT THE SAME TIME.
   if ( prevNbRawCounters < instruInfo.nbRawCounters)
   {
      instruInfo.nbGroups++;
      int* tmpGroupIdxToPfmFds = realloc (instruInfo.groupIdxToPfmFds, instruInfo.nbGroups * sizeof(*tmpHwcIdxToGroup));
      if ( tmpGroupIdxToPfmFds == NULL)
      {
         return -2;
      }
      instruInfo.groupIdxToPfmFds = tmpGroupIdxToPfmFds;
      instruInfo.groupIdxToPfmFds[instruInfo.nbGroups-1] = prevNbCounters;//instruInfo.nbGroups - 1;
      //fprintf(stderr,"groupIdxToPfmFds[%d] = %d\n",instruInfo.nbGroups-1,prevNbCounters);


      // ADD THE PID,CORE INFO TO THE NEW GROUP
      for ( i = prevNbCounters; i < instruInfo.nbRawCounters; i++ )
      {
         instruInfo.counterPid[i]  = pid;
         instruInfo.counterCore[i] = core;
         instruInfo.pfmFds[i]      = -1;
         instruInfo.hwcIdxToGroup[i] = prevNbCounters;//instruInfo.nbGroups - 1; //Idx of the first HWC of the new group
         //fprintf(stderr ,"pfmFds[%d] = %d\n",i,instruInfo.pfmFds[i]);
      }

      unsigned int* tmpNbCountersPerGroup = realloc (instruInfo.nbCountersPerGroup, instruInfo.nbGroups * sizeof(*tmpNbCountersPerGroup));
      if ( tmpNbCountersPerGroup == NULL)
      {
         //free (instruInfo.nbCountersPerGroup);
         return -2;
      }
      instruInfo.nbCountersPerGroup = tmpNbCountersPerGroup;
      instruInfo.nbCountersPerGroup[instruInfo.nbGroups-1] = instruInfo.nbRawCounters - prevNbRawCounters; //Add the number of HWC of this group
   }

   free(sortedHwcList);
   free(copyHwcList);
   return instruInfo.nbCounters;
}


//      HOW TO READ IN THE FILE DESCRIPTOR RETURNED!
//      STRUCT read_format {
//      { u64       value;
//        { u64     time_enabled; } && PERF_FORMAT_ENABLED
//        { u64     time_running; } && PERF_FORMAT_RUNNING
//        { u64     id;           } && PERF_FORMAT_ID
//      }
//
//      In buf [0] = value = raw count
//      In buf [1] = time_enabled
//      In buf [2] = time_running
//
// IF PERF_FORMAT_GROUP IS ENABELD :
//    struct read_format {
//        u64 nr;            /* The number of events */
//        u64 time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
//        u64 time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
//        struct {
//            u64 value;     /* The value of the event */
//            u64 id;        /* if PERF_FORMAT_ID */
//        } values[nr];
//    };
//
//
//      In buf [0] = nr ( number of events)
//      In buf [1] = time_enabled
//      In buf [2] = time_running
//      values[event_id]
//      {
//          In buf [3+(event_id*2)] = /* The value of the event */
//          In buf [4+(event_id*2)] = /* if PERF_FORMAT_ID */
//      }
//
void counting_start_counting (unsigned int callSiteId)
{
   if (callSiteId >= instruInfo.nbCallSites)
   {
      fprintf(stderr,"ERROR : WRONG CALL SITE ID : #%u\n", callSiteId);
      fprintf(stderr,"ERROR : ONLY [0,%d] IS POSSIBLE HERE!\n", instruInfo.nbCallSites-1);
      exit(-1);
   }

   unsigned int idx;
   ssize_t tmp;
   int groupIdx;

   // WARMUP THE CACHE
   for (idx=0; idx < instruInfo.nbCounters; idx++)
      instruInfo.counterInfo[callSiteId][idx].value = 0;

   // HANDLING CORE HWC IF ANY
   if (instruInfo.nbRawCounters > 0)
   {
      for (groupIdx = 0; groupIdx < instruInfo.nbGroups; groupIdx++)
      {
         //fprintf(stderr,"START_COUNTING - GROUP = %d\n",groupIdx);
         tmp = read (instruInfo.pfmFds[instruInfo.groupIdxToPfmFds[groupIdx]], instruInfo.buffer[groupIdx], instruInfo.bufferSize[groupIdx]);
         if (tmp != instruInfo.bufferSize[groupIdx])
         {
            fprintf(stderr, "ERROR : counting_start_counting failed to read counter %s\n", instruInfo.counterNames[0]);
            perror( "ERROR");
         }

         for ( idx = 0 ; idx < instruInfo.nbCountersPerGroup[groupIdx]; idx++)
         {
            // handle scaling to allow PMU sharing
            instruInfo.counterInfo[callSiteId][ idx + instruInfo.groupIdxToPfmFds[groupIdx]].value = (uint64_t)((double) instruInfo.buffer[groupIdx][3+(idx*2)] * instruInfo.buffer[groupIdx][1] / instruInfo.buffer[groupIdx][2]);
            //fprintf(stderr,"IDX = %d # [0] = %"PRId64", [1] = %"PRId64", [2] = %"PRId64"\n",idx + instruInfo.groupIdxToPfmFds[groupIdx], instruInfo.buffer[groupIdx][3+(idx*2)],instruInfo.buffer[groupIdx][1], instruInfo.buffer[groupIdx][2]);
         }
      }
   }

   // HANDLING UNCORE COUNTERS IF ANY
   for (idx = 0; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_DATA_WRITES:
         instruInfo.counterInfo[callSiteId][idx].value = *((uint32_t*)(mmapBar + DRAM_DATA_WRITES));
         break;

      case TYPE_DATA_READS:
         instruInfo.counterInfo[callSiteId][idx].value = *((uint32_t*)(mmapBar + DRAM_DATA_READS));
         break;

      default:
         break;
      }
   }
}


// STOP THE HWC, COMPUTE THE DIFFERENCE BETWEEN START AND STOP AND STORE IT.
// EACH INSTANCE HAS IS OWN RESULT (STOP-START VALUE).
void counting_stop_counting (unsigned int callSiteId)
{
   if (callSiteId >= instruInfo.nbCallSites)
   {
      fprintf(stderr,"ERROR : WRONG CALL SITE ID : #%u\n", callSiteId);
      fprintf(stderr,"ERROR : ONLY [0,%d] IS POSSIBLE HERE!\n", instruInfo.nbCallSites-1);
      exit(-1);
   }

   unsigned int idx;
   int tmp;
   uint64_t stopValue=0, resultValue=0;
   uint32_t bufDram[TYPE_NB_MAX] = {0};
   int idxDram = -1;
   int groupIdx = 0;

   // HANDLING CORE HWC IF ANY
   if (instruInfo.nbRawCounters > 0)
   {
      for (groupIdx = 0; groupIdx < instruInfo.nbGroups; groupIdx++)
      {
         tmp = read (instruInfo.pfmFds[instruInfo.groupIdxToPfmFds[groupIdx]], instruInfo.buffer[groupIdx], instruInfo.bufferSize[groupIdx]);
         if (tmp != instruInfo.bufferSize[groupIdx])
         {
            fprintf(stderr, "ERROR : Failed to read counter %s\n", instruInfo.counterNames[0]);
         }
      }
   }


   // HANDLING UNCORE COUNTERS IF ANY
   for (idx= 0; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_DATA_WRITES:
         bufDram[TYPE_DATA_WRITES-1] = *((uint32_t*)(mmapBar + DRAM_DATA_WRITES));
         break;

      case TYPE_DATA_READS:
         bufDram[TYPE_DATA_READS-1] = *((uint32_t*)(mmapBar + DRAM_DATA_READS));
         break;

      default:
         break;
      }
   }


   // NOW IT'S TIME TO COMPUTE THE DIFFERENCE BETWEEN START/STOP VALUES

   // RAW COUNTERS
   unsigned int hwcIdx;
   for (groupIdx = 0; groupIdx < instruInfo.nbGroups; groupIdx++)
   {
      for (hwcIdx = 0; hwcIdx < instruInfo.nbCountersPerGroup[groupIdx]; hwcIdx++)
      {
         //fprintf(stderr,"IDX = %d # [0] = %"PRId64", [1] = %"PRId64", [2] = %"PRId64"\n",hwcIdx + instruInfo.groupIdxToPfmFds[groupIdx], instruInfo.buffer[groupIdx][3+(hwcIdx*2)],instruInfo.buffer[groupIdx][1], instruInfo.buffer[groupIdx][2]);
         stopValue = (uint64_t)((double) instruInfo.buffer[groupIdx][3+(hwcIdx*2)] * instruInfo.buffer[groupIdx][1] / instruInfo.buffer[groupIdx][2]);
         resultValue = stopValue - instruInfo.counterInfo[callSiteId][instruInfo.groupIdxToPfmFds[groupIdx] + hwcIdx].value;


         //ISSUE (OVERFLOW?)
         if (instruInfo.counterInfo[callSiteId][instruInfo.groupIdxToPfmFds[groupIdx] + hwcIdx].value > stopValue)
         {
            resultValue = 0;
         }

         ScounterInfo* result  = malloc (sizeof(*result));
         result->name = instruInfo.counterNames[instruInfo.groupIdxToPfmFds[groupIdx] + hwcIdx];
         result->id = callSiteId;
         result->value = resultValue;
         queue_add_tail(instruInfo.collectedInfo[callSiteId],(void*)result);
      }
   }


   // DRAM COUNTERS
   for ( idx=0; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_DATA_WRITES:
         idxDram = TYPE_DATA_WRITES - 1;
      case TYPE_DATA_READS:
         if (idxDram == -1)
            idxDram = TYPE_DATA_READS - 1;
         //OVERFLOW?
         if ( instruInfo.counterInfo[callSiteId][idx].value >  bufDram[idxDram])
         {
            resultValue = (UINT32_MAX - instruInfo.counterInfo[callSiteId][idx].value) + bufDram[idxDram];
         }
         else
            resultValue = bufDram[idxDram] - instruInfo.counterInfo[callSiteId][idx].value;
         idxDram = -1;

         ScounterInfo* resultDram  = malloc (sizeof(*resultDram));
         resultDram->name = instruInfo.counterNames[idx];
         resultDram->id = callSiteId;
         resultDram->value = resultValue;
         queue_add_tail(instruInfo.collectedInfo[callSiteId],(void*)resultDram);
         break;

      default:
         break;
      }
   }
}

void counting_stop_counting_dumb (unsigned int callSiteId)
{
   if (callSiteId >= instruInfo.nbCallSites)
   {
      fprintf(stderr,"ERROR : WRONG CALL SITE ID : #%u\n", callSiteId);
      fprintf(stderr,"ERROR : ONLY [0,%d] IS POSSIBLE HERE!\n", instruInfo.nbCallSites-1);
      exit(-1);
   }

   static unsigned first_visit = 1;
   unsigned int idx;
   int tmp;
   uint64_t stopValue=0, resultValue=0;
   ssize_t size = sizeof(uint64_t) * (3 + (instruInfo.nbRawCounters*2));
   uint64_t *buf = malloc (size);
   uint32_t bufDram[TYPE_NB_MAX] = {0};
   int idxDram = -1;
   int idxFirstDram = instruInfo.nbRawCounters; // DRAM_DATA counters MUST be after the raw counters.

   // HANDLING CORE HWC IF ANY
   if (instruInfo.nbRawCounters > 0)
   {
      tmp = read (instruInfo.pfmFds[0], buf, size);
      if (tmp != size)
      {
         fprintf(stderr, "ERROR : Failed to read counter %s\n", instruInfo.counterNames[0]);
      }
   }

   // HANDLING UNCORE COUNTERS IF ANY
   for (idx= idxFirstDram; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_DATA_WRITES:
         bufDram[TYPE_DATA_WRITES-1] = *((uint32_t*)(mmapBar + DRAM_DATA_WRITES));
         break;

      case TYPE_DATA_READS:
         bufDram[TYPE_DATA_READS-1] = *((uint32_t*)(mmapBar + DRAM_DATA_READS));
         break;

      default:
         break;
      }
   }

   for ( idx=0; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_RAW:
         stopValue = (uint64_t)((double) buf [3+(idx*2)] * buf [1] / buf [2]);
         resultValue = stopValue - instruInfo.counterInfo[callSiteId][idx].value;

         //ISSUE (OVERFLOW?)
         if (instruInfo.counterInfo[callSiteId][idx].value > stopValue)
         {
            resultValue = 0;
         }
         break;

      case TYPE_DATA_WRITES:
         idxDram = TYPE_DATA_WRITES - 1;
      case TYPE_DATA_READS:
         if (idxDram == -1)
            idxDram = TYPE_DATA_READS - 1;
         //OVERFLOW?
         if ( instruInfo.counterInfo[callSiteId][idx].value >  bufDram[idxDram])
         {
            resultValue = (UINT32_MAX - instruInfo.counterInfo[callSiteId][idx].value) + bufDram[idxDram];
         }
         else
            resultValue = bufDram[idxDram] - instruInfo.counterInfo[callSiteId][idx].value;
         idxDram = -1;
         break;

      default:
         break;
      }

      ScounterInfo* result  = malloc (sizeof(*result));
      result->name = instruInfo.counterNames[idx];
      result->id = callSiteId;
      result->value = resultValue;
      queue_add_tail(instruInfo.collectedInfo[callSiteId],(void*)result);
   }

   if (first_visit)
   {
      for ( idx=0; idx < instruInfo.nbCounters; idx++)
      {
         free(queue_remove_tail(instruInfo.collectedInfo[callSiteId]));
      }
      first_visit = 0;
   }
}

// STOP THE HWCS, COMPUTE THE DIFFERENCE BETWEEN START AND STOP AND STORE IT.
// FOR EACH CALLSITE (START/STOP COMBO), WE ACCUMULATE THE RESULTS FOR ALL INSTANCES.
void counting_stop_counting_and_accumulate (unsigned int callSiteId)
{
   if (callSiteId >= instruInfo.nbCallSites)
   {
      fprintf(stderr,"ERROR : WRONG CALL SITE ID : #%u\n", callSiteId);
      fprintf(stderr,"ERROR : ONLY [0,%d] IS POSSIBLE HERE!\n", instruInfo.nbCallSites-1);
      exit(-1);
   }

   unsigned int idx;
   int tmp;
   uint64_t stopValue=0, resultValue=0;
   ssize_t size = sizeof(uint64_t) * (3 + (instruInfo.nbRawCounters*2));
   uint64_t *buf = malloc (size);
   uint32_t bufDram[TYPE_NB_MAX] = {0};
   int idxDram = -1;
   int idxFirstDram = instruInfo.nbRawCounters; // DRAM_DATA counters MUST be after the raw counters.

   // HANDLING CORE HWC IF ANY
   if (instruInfo.nbRawCounters > 0)
   {
      tmp = read (instruInfo.pfmFds[0], buf, size);
      if (tmp != size)
      {
         fprintf(stderr, "ERROR : Failed to read counter %s\n", instruInfo.counterNames[0]);
      }
   }


   // HANDLING UNCORE COUNTERS IF ANY
   for (idx= idxFirstDram; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_DATA_WRITES:
         bufDram[TYPE_DATA_WRITES-1] = *((uint32_t*)(mmapBar + DRAM_DATA_WRITES));
         break;

      case TYPE_DATA_READS:
         bufDram[TYPE_DATA_READS-1] = *((uint32_t*)(mmapBar + DRAM_DATA_READS));
         break;

      default:
         break;
      }
   }


   // NOW IT'S TIME TO COMPUTE THE DIFFERENCE BETWEEN START/STOP VALUES
   for ( idx = 0; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_RAW:
         stopValue = (uint64_t)((double) buf [3+(idx*2)] * buf [1] / buf [2]);
         resultValue = stopValue - instruInfo.counterInfo[callSiteId][idx].value;
         if (instruInfo.counterInfo[callSiteId][idx].value > stopValue)
         {
            resultValue = 0;
         }
         instruInfo.accumulateSamples[callSiteId][idx] += resultValue;
         break;

      case TYPE_DATA_WRITES:
         idxDram = TYPE_DATA_WRITES - 1;
      case TYPE_DATA_READS:
         if (idxDram == -1)
            idxDram = TYPE_DATA_READS - 1;
         //OVERFLOW?
         if ( instruInfo.counterInfo[callSiteId][idx].value >  bufDram[idxDram])
         {
            resultValue = (UINT32_MAX - instruInfo.counterInfo[callSiteId][idx].value) + bufDram[idxDram];
         }
         else
            resultValue = bufDram[idxDram] - instruInfo.counterInfo[callSiteId][idx].value;
         instruInfo.accumulateSamples[callSiteId][idx] += resultValue;
         idxDram = -1;
         break;

      default:
         break;
      }
   }
}

void counting_stop_counting_and_accumulate_dumb (unsigned int callSiteId)
{
   if (callSiteId >= instruInfo.nbCallSites)
   {
      fprintf(stderr,"ERROR : WRONG CALL SITE ID : #%u\n", callSiteId);
      fprintf(stderr,"ERROR : ONLY [0,%d] IS POSSIBLE HERE!\n", instruInfo.nbCallSites-1);
      exit(-1);
   }

   static unsigned first_visit = 1;
   unsigned int idx;
   int tmp;
   uint64_t stopValue=0, resultValue=0;
   ssize_t size = sizeof(uint64_t) * (3 + (instruInfo.nbRawCounters*2));
   uint64_t *buf = malloc (size);
   uint32_t bufDram[TYPE_NB_MAX] = {0};
   int idxDram = -1;
   int idxFirstDram = instruInfo.nbRawCounters; // DRAM_DATA counters MUST be after the raw counters.

   // HANDLING CORE HWC IF ANY
   if (instruInfo.nbRawCounters > 0)
   {
      tmp = read (instruInfo.pfmFds[0], buf, size);
      if (tmp != size)
      {
         fprintf(stderr, "ERROR : Failed to read counter %s\n", instruInfo.counterNames[0]);
      }
   }


   // HANDLING UNCORE COUNTERS IF ANY
   for (idx= idxFirstDram; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_DATA_WRITES:
         bufDram[TYPE_DATA_WRITES-1] = *((uint32_t*)(mmapBar + DRAM_DATA_WRITES));
         break;

      case TYPE_DATA_READS:
         bufDram[TYPE_DATA_READS-1] = *((uint32_t*)(mmapBar + DRAM_DATA_READS));
         break;

      default:
         break;
      }
   }


   for ( idx = 0; idx < instruInfo.nbCounters; idx++)
   {
      switch (instruInfo.counterLibTypes[idx])
      {
      case TYPE_RAW:
         stopValue = (uint64_t)((double) buf [3+(idx*2)] * buf [1] / buf [2]);
         resultValue = stopValue - instruInfo.counterInfo[callSiteId][idx].value;
         if (instruInfo.counterInfo[callSiteId][idx].value > stopValue)
            resultValue = 0;

         if (first_visit)
            first_visit = 0;
         else
            instruInfo.accumulateSamples[callSiteId][idx] += resultValue;
         break;

      case TYPE_DATA_WRITES:
         idxDram = TYPE_DATA_WRITES - 1;
      case TYPE_DATA_READS:
         if (idxDram == -1)
            idxDram = TYPE_DATA_READS - 1;
         //OVERFLOW?
         if ( instruInfo.counterInfo[callSiteId][idx].value >  bufDram[idxDram])
            resultValue = (UINT32_MAX - instruInfo.counterInfo[callSiteId][idx].value) + bufDram[idxDram];
         else
            resultValue = bufDram[idxDram] - instruInfo.counterInfo[callSiteId][idx].value;

         if (!first_visit) //IF IT'S NOT THE FIRST VISIT WE CAN ADD SAMPLES
            instruInfo.accumulateSamples[callSiteId][idx] += resultValue;

         idxDram = -1;
         break;

      default:
         break;
      }
   }
   first_visit = 0;
}

//DUMP RESULT IN A FILE
//FORMAT  #CALLSITE_ID,#HWC_VALUE // FOR INSTANCE #1
//        [...]                   // FOR INSTANCE #2 ...
void counting_dump_file (char* fileName)
{
   if (fileName == NULL)
   {
      fprintf(stderr,"ERROR : Output file name is not defined!");
   }
   FILE* outputFile = NULL;
   outputFile = fopen(fileName,"w+");

   unsigned int i;
   for (i=0; i < instruInfo.nbCallSites; i++)
   {
      FOREACH_INQUEUE(instruInfo.collectedInfo[i],queueIter)
      {
         ScounterInfo* result = GET_DATA_T(ScounterInfo*, queueIter);
         fprintf(outputFile,"%d,%s,%"PRId64"\n",result->id,result->name,result->value);
      }
   }
   fclose(outputFile);
}

//DUMP RESULT IN A FILE
//FORMAT: ID,HWC_NAME_1[,HWC_NAME_2...]
//        #CALLSITE_ID_1,#HWC_VALUE_1[,#HWC_VALUE_2...] // FOR ONE INSTANCE
//        [...]                                         // SECOND INSTANCE IF ANY...
void counting_dump_file_by_line (char* fileName)
{
   if (fileName == NULL)
   {
      fprintf(stderr,"ERROR : Output file name is not defined!");
   }
   FILE* outputFile = NULL;
   outputFile = fopen(fileName,"w+");

   unsigned int i;
   fprintf(outputFile,"ID,");
   for (i=0; i < instruInfo.nbCounters; i++)
      fprintf(outputFile,"%s,",instruInfo.counterNames[i]);
   fprintf(outputFile,"\n");

   int counterPrinted = 0;
   int idPrinted = 0;
   for (i=0; i < instruInfo.nbCallSites; i++)
   {
      counterPrinted = 0;
      idPrinted = 0;
      //WRITE THE ID
      FOREACH_INQUEUE(instruInfo.collectedInfo[i],queueIter)
      {
         if (idPrinted == 0)
         {
            fprintf(outputFile,"%d,",i);
            idPrinted=1;
         }
         ScounterInfo* result = GET_DATA_T(ScounterInfo*, queueIter);
         fprintf(outputFile,"%"PRId64",",result->value);
         counterPrinted++;
         if (counterPrinted % instruInfo.nbCounters == 0)
         {
            fprintf(outputFile,"\n");
            idPrinted = 0;
         }
      }
   }
   fclose(outputFile);
}



unsigned int get_nb_callsites ()
{
   return instruInfo.nbCallSites;
}

unsigned int get_nb_counters ()
{
   return instruInfo.nbCounters;
}

unsigned int* get_nb_counters_per_group ()
{
   return instruInfo.nbCountersPerGroup;
}

int get_nb_groups ()
{
   return instruInfo.nbGroups;
}

uint64_t** get_counter_info_accumulate ()
{

   if ( instruInfo.accumulateSamples != NULL )
      return instruInfo.accumulateSamples;
   else
      return NULL;
}

ScounterInfo*** get_counter_info ()
{
   unsigned int idx;
   ScounterInfo*** results = NULL;
   results = calloc ( instruInfo.nbCallSites, sizeof (*results) );
   for ( idx = 0; idx < instruInfo.nbCallSites; idx++ )
   {
      results[idx] = calloc ( instruInfo.nbCounters, sizeof (*results[idx]) );
      int idxCounter = instruInfo.nbCounters -1;
      FOREACH_INQUEUE_REVERSE( instruInfo.collectedInfo[idx], queueIter )
      {
         if (idxCounter  < 0)
            break;
         ScounterInfo* result = GET_DATA_T(ScounterInfo*, queueIter);
         results[idx][idxCounter] = result;
         idxCounter--;
      }
   }
   return results;
}

//DUMP THE RESULTS IN A CHAR ARRAY FINISHING BY NULL
char** counting_dump (void)
{
   char** output = NULL;
   int line = 1;
   unsigned int i;
   for (i=0; i < instruInfo.nbCallSites; i++)
   {
      FOREACH_INQUEUE(instruInfo.collectedInfo[i],queueIter)
      {
         output = realloc (output,line * sizeof(*output));
         output[line-1] = malloc ( 512 * sizeof(char) );
         ScounterInfo* result = GET_DATA_T(ScounterInfo*, queueIter);
         sprintf(output[line-1],"%d,%s,%"PRId64"\n",result->id,result->name,result->value);
         line++;
      }
   }
   output = realloc (output,line * sizeof(*output));
   output[line-1] = NULL;
   return output;
}

//DUMP THE CUMULATIVE RESULTS IN A CHAR ARRAY FINISHING BY NULL
char*** counting_dump_accumulate (void)
{
   char*** output = NULL;
   unsigned int i,j;
   output = malloc ((instruInfo.nbCallSites+1) * sizeof(*output));
   for (i=0; i < instruInfo.nbCallSites; i++)
   {
      output[i] = malloc (instruInfo.nbCounters * sizeof(*output[i]));
      for (j=0; j < instruInfo.nbCounters; j++)
      {
         output[i][j] = malloc (1024 * sizeof(char));
         sprintf(output[i][j],"%d,%s,%"PRId64"\n",i,instruInfo.counterNames[j],instruInfo.accumulateSamples[i][j]);
      }
   }
   output[i] = NULL;
   return output;
}

//DUMP RESULT IN A FILE
//FORMAT:   #CALLSITE_ID,#HWC_VALUE_1 //FOR ALL INSTANCES
//          [...]
void counting_dump_file_accumulate (char* fileName)
{
   if (fileName == NULL)
   {
      fprintf(stderr,"ERROR : Output file name is not defined!");
   }
   FILE* outputFile = NULL;
   outputFile = fopen(fileName,"w+");

   unsigned int i,j;
   for (i=0; i < instruInfo.nbCallSites; i++)
   {
      for (j=0; j < instruInfo.nbCounters; j++)
      {
         fprintf(outputFile,"%d,%s,%"PRId64"\n",i,instruInfo.counterNames[j],instruInfo.accumulateSamples[i][j]);
      }
   }
   fclose(outputFile);
}





//DUMP RESULT IN A FILE
void counting_dump_file_accumulate_and_reset (FILE* outfile, int callsite_id, int asmf_id, int instance, int nb_areas, char** areas, char** asmfs, char** variants)
{
   int areaid = callsite_id % (nb_areas);
   int variantid = (int)(callsite_id / nb_areas);
   unsigned int j = 0;

   for (j = 0; j < instruInfo.nbCounters; j++)
   {
      fprintf (outfile, "%s;%s;%s;0;%d;%s;%"PRId64";\n",
               areas[areaid], asmfs[asmf_id], variants[variantid], instance, instruInfo.counterNames[j], instruInfo.accumulateSamples[callsite_id][j]);

      instruInfo.accumulateSamples[callsite_id][j] = 0;
   }
}
