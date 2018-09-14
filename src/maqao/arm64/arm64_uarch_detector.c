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

/**
 * \file arm64_uarch_detector.c
 * \brief This file contains architecture-specific functions for retrieving information about the current host CPU
 */

#include "arm64_arch.h"
#include "arm64_uarch.h"
#define MAX_SIZE 1024

/*
 * Architecture-specific function identifying the current host
 * \return Structure describing the processor version of the current host
 * */
proc_t* arm64_utils_get_proc_host()
{
   //DETECTION OF THE CPU INFORMATION USING /PROC/CPUINFO
   char line[MAX_SIZE] = "";
   char str1[128];
   char strarch[128];
   char struarch[128];
   char* endPtr;
   FILE* cpuInfoFile = fopen("/proc/cpuinfo", "r");
   if (cpuInfoFile != NULL) {
      while (fgets(line, MAX_SIZE, cpuInfoFile) != NULL) {
         if (strstr(line, "CPU architecture")) {
            sscanf(line, "%s\t: %s", str1, strarch);
         }
         if (strstr(line, "CPU part")) {
            sscanf(line, "%s\t: %s", str1, struarch);
         }
      }
      fclose(cpuInfoFile);
   }
   arch_t* arch = NULL;

   if (strcmp(strarch, "AArch64")) {
         arch = &arm64_arch;
   } else {
      int archNumber = strtol(strarch, &endPtr, 10);
      if (endPtr != strarch) {
         if (archNumber == 8)
            arch = &arm64_arch;
      }
   }

   if (arch != NULL) {
      if (strcmp(struarch, "0xd07"))
         return(arch_get_proc_by_id(arch, arm64_UARCH_CORTEX_A57));
   }
   return NULL;
}



