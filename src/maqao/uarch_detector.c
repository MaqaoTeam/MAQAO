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

#define MAX_SIZE 1024

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#include <tchar.h>
#include <intrin.h>
#else
#include <dirent.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "arch.h"
#include "libmcommon.h"
#include "libmasm.h"
#include "uarch_detector.h"

#if defined (_ARCHDEF_arm64)
#include "arm64_arch.h"
#endif

#ifdef _ARCHDEF_arm64
#include "arm64_uarch_detector.h"
#endif

/**\todo (2016-12-13) Move all architecture-specific functions to the appropriate <arch>/<arch>_uarch_detector.c file*/

static inline uint32_t bit(int bitno)
{
   return 1 << (bitno & 31);
}

static inline void native_cpuid(unsigned int *eax, unsigned int *ebx,
                                unsigned int *ecx, unsigned int *edx)
{
   /* ecx is often an input as well as an output. */
#ifdef _WIN32
   unsigned int regs[4];
   __cpuidex((int*)regs, (int) *eax, (int) *ecx);
   *eax = regs[0];
   *ebx = regs[1];
   *ecx = regs[2];
   *edx = regs[3];
#elif __x86_64__
   asm volatile("cpuid"
                : "=a" (*eax),
                  "=b" (*ebx),
                  "=c" (*ecx),
                  "=d" (*edx)
                : "0" (*eax), "2" (*ecx));
#endif
}

uarch_flags get_cpuid_flags()
{

   unsigned eax, ebx, ecx, edx;
   uarch_flags flags;

   // PROCESSOR INFO AND FEATURES BITS
   eax = 1;
   ecx = 0;
   native_cpuid(&eax, &ebx, &ecx, &edx);

   //READ EDX
   flags.fpu = (edx & bit(X86_FEATURE_FPU)) ? 1 : 0;
   flags.vme = (edx & bit(X86_FEATURE_VME)) ? 1 : 0;
   flags.de = (edx & bit(X86_FEATURE_DE)) ? 1 : 0;
   flags.pse = (edx & bit(X86_FEATURE_PSE)) ? 1 : 0;
   flags.tsc = (edx & bit(X86_FEATURE_TSC)) ? 1 : 0;
   flags.msr = (edx & bit(X86_FEATURE_MSR)) ? 1 : 0;
   flags.pae = (edx & bit(X86_FEATURE_PAE)) ? 1 : 0;
   flags.mce = (edx & bit(X86_FEATURE_MCE)) ? 1 : 0;
   flags.cx8 = (edx & bit(X86_FEATURE_CX8)) ? 1 : 0;
   flags.apic = (edx & bit(X86_FEATURE_APIC)) ? 1 : 0;
   flags.sep = (edx & bit(X86_FEATURE_SEP)) ? 1 : 0;
   flags.mtrr = (edx & bit(X86_FEATURE_MTRR)) ? 1 : 0;
   flags.pge = (edx & bit(X86_FEATURE_PGE)) ? 1 : 0;
   flags.mca = (edx & bit(X86_FEATURE_MCA)) ? 1 : 0;
   flags.cmov = (edx & bit(X86_FEATURE_CMOV)) ? 1 : 0;
   flags.pat = (edx & bit(X86_FEATURE_PAT)) ? 1 : 0;
   flags.pse36 = (edx & bit(X86_FEATURE_PSE36)) ? 1 : 0;
   flags.psn = (edx & bit(X86_FEATURE_PN)) ? 1 : 0;
   flags.clflush = (edx & bit(X86_FEATURE_CLFLUSH)) ? 1 : 0;
   flags.ds = (edx & bit(X86_FEATURE_DS)) ? 1 : 0;
   flags.acpi = (edx & bit(X86_FEATURE_ACPI)) ? 1 : 0;
   flags.mmx = (edx & bit(X86_FEATURE_MMX)) ? 1 : 0;
   flags.fxsr = (edx & bit(X86_FEATURE_FXSR)) ? 1 : 0;
   flags.sse = (edx & bit(X86_FEATURE_XMM)) ? 1 : 0;
   flags.sse2 = (edx & bit(X86_FEATURE_XMM2)) ? 1 : 0;
   flags.ss = (edx & bit(X86_FEATURE_SELFSNOOP)) ? 1 : 0;
   flags.ht = (edx & bit(X86_FEATURE_HT)) ? 1 : 0;
   flags.tm = (edx & bit(X86_FEATURE_ACC)) ? 1 : 0;
   flags.ia64 = (edx & bit(X86_FEATURE_IA64)) ? 1 : 0;
   flags.pbe = (edx & bit(X86_FEATURE_PBE)) ? 1 : 0;

   //READ ECX
   flags.sse3 = (ecx & bit(X86_FEATURE_XMM3)) ? 1 : 0;
   flags.pclmulqdq = (ecx & bit(X86_FEATURE_PCLMULQDQ)) ? 1 : 0;
   flags.dtes64 = (ecx & bit(X86_FEATURE_DTES64)) ? 1 : 0;
   flags.monitor = (ecx & bit(X86_FEATURE_MWAIT)) ? 1 : 0;
   flags.ds_cpl = (ecx & bit(X86_FEATURE_DSCPL)) ? 1 : 0;
   flags.vmx = (ecx & bit(X86_FEATURE_VMX)) ? 1 : 0;
   flags.smx = (ecx & bit(X86_FEATURE_SMX)) ? 1 : 0;
   flags.est = (ecx & bit(X86_FEATURE_EST)) ? 1 : 0;
   flags.tm2 = (ecx & bit(X86_FEATURE_TM2)) ? 1 : 0;
   flags.ssse3 = (ecx & bit(X86_FEATURE_SSSE3)) ? 1 : 0;
   flags.cnxt_id = (ecx & bit(X86_FEATURE_CID)) ? 1 : 0;
   flags.sdbg = (ecx & bit(X86_FEATURE_SDBG)) ? 1 : 0; // NOT DEFINE IN cpufeature.h
   flags.fma = (ecx & bit(X86_FEATURE_FMA)) ? 1 : 0;
   flags.cx16 = (ecx & bit(X86_FEATURE_CX16)) ? 1 : 0;
   flags.xtpr = (ecx & bit(X86_FEATURE_XTPR)) ? 1 : 0;
   flags.pdcm = (ecx & bit(X86_FEATURE_PDCM)) ? 1 : 0;
   flags.pcid = (ecx & bit(X86_FEATURE_PCID)) ? 1 : 0;
   flags.dca = (ecx & bit(X86_FEATURE_DCA)) ? 1 : 0;
   flags.sse4_1 = (ecx & bit(X86_FEATURE_XMM4_1)) ? 1 : 0;
   flags.sse4_2 = (ecx & bit(X86_FEATURE_XMM4_2)) ? 1 : 0;
   flags.x2apic = (ecx & bit(X86_FEATURE_X2APIC)) ? 1 : 0;
   flags.movbe = (ecx & bit(X86_FEATURE_MOVBE)) ? 1 : 0;
   flags.popcnt = (ecx & bit(X86_FEATURE_POPCNT)) ? 1 : 0;
   flags.tsc_deadline = (ecx & bit(X86_FEATURE_TSC_DEADLINE_TIMER)) ? 1 : 0;
   flags.aes = (ecx & bit(X86_FEATURE_AES)) ? 1 : 0;
   flags.xsave = (ecx & bit(X86_FEATURE_XSAVE)) ? 1 : 0;
   flags.osxsave = (ecx & bit(X86_FEATURE_OSXSAVE)) ? 1 : 0;
   flags.avx = (ecx & bit(X86_FEATURE_AVX)) ? 1 : 0;
   flags.f16c = (ecx & bit(X86_FEATURE_F16C)) ? 1 : 0;
   flags.rdrand = (ecx & bit(X86_FEATURE_RDRAND)) ? 1 : 0;
   flags.hypervisor = (ecx & bit(X86_FEATURE_HYPERVISOR)) ? 1 : 0;

   // GET EXTENDED FEATURES
   eax = 7;
   ecx = 0;
   native_cpuid(&eax, &ebx, &ecx, &edx);

   //READ EBX
   flags.fsgsbase = (ebx & bit(X86_FEATURE_FSGSBASE)) ? 1 : 0;
   flags.bmi1 = (ebx & bit(X86_FEATURE_TSC_ADJUST)) ? 1 : 0;
   flags.bmi1 = (ebx & bit(X86_FEATURE_BMI1)) ? 1 : 0;
   flags.hle = (ebx & bit(X86_FEATURE_HLE)) ? 1 : 0;
   flags.avx2 = (ebx & bit(X86_FEATURE_AVX2)) ? 1 : 0;
   flags.smep = (ebx & bit(X86_FEATURE_SMEP)) ? 1 : 0;
   flags.bmi2 = (ebx & bit(X86_FEATURE_BMI2)) ? 1 : 0;
   flags.erms = (ebx & bit(X86_FEATURE_ERMS)) ? 1 : 0;
   flags.invpcid = (ebx & bit(X86_FEATURE_INVPCID)) ? 1 : 0;
   flags.rtm = (ebx & bit(X86_FEATURE_RTM)) ? 1 : 0;
   flags.cqm = (ebx & bit(X86_FEATURE_CQM)) ? 1 : 0;
   flags.mpx = (ebx & bit(X86_FEATURE_MPX)) ? 1 : 0;
   flags.avx512f = (ebx & bit(X86_FEATURE_AVX512F)) ? 1 : 0;
   flags.avx512dq = (ebx & bit(X86_FEATURE_AVX512DQ)) ? 1 : 0; // NOT DEFINE IN cpufeature.h
   flags.rdseed = (ebx & bit(X86_FEATURE_RDSEED)) ? 1 : 0;
   flags.adx = (ebx & bit(X86_FEATURE_ADX)) ? 1 : 0;
   flags.smap = (ebx & bit(X86_FEATURE_SMAP)) ? 1 : 0;
   flags.avx512ifma = (ebx & bit(X86_FEATURE_AVX512IFMA)) ? 1 : 0; // NOT DEFINE IN cpufeature.h
   flags.pcommit = (ebx & bit(X86_FEATURE_PCOMMIT)) ? 1 : 0;
   flags.clflushopt = (ebx & bit(X86_FEATURE_CLFLUSHOPT)) ? 1 : 0;
   flags.clwb = (ebx & bit(X86_FEATURE_CLWB)) ? 1 : 0;
   flags.pt = (ebx & bit(X86_FEATURE_PT)) ? 1 : 0; // NOT DEFINE IN cpufeature.h
   flags.avx512pf = (ebx & bit(X86_FEATURE_AVX512PF)) ? 1 : 0;
   flags.avx512er = (ebx & bit(X86_FEATURE_AVX512ER)) ? 1 : 0;
   flags.avx512cd = (ebx & bit(X86_FEATURE_AVX512CD)) ? 1 : 0;
   flags.sha = (ebx & bit(X86_FEATURE_SHA)) ? 1 : 0;
   flags.avx512bw = (ebx & bit(X86_FEATURE_AVX512BW)) ? 1 : 0;
   flags.avx512vl = (ebx & bit(X86_FEATURE_AVX512VL)) ? 1 : 0;

   //READ ECX
   flags.prefetchwt1 = (ecx & bit(X86_FEATURE_PREFETCHWT1)) ? 1 : 0;
   ;
   flags.avx512vbmi = (ecx & bit(X86_FEATURE_AVX512VBMI)) ? 1 : 0;

   //GET EXTENDED PROCESSOR INFO AND FEATURES
   eax = 0x80000001;
   ecx = 0;
   native_cpuid(&eax, &ebx, &ecx, &edx);

   flags.fxsr_opt = (edx & bit(25)) ? 1 : 0;
   flags.threed_now = (edx & bit(31)) ? 1 : 0;
   ;

   flags.abm = (ecx & bit(5)) ? 1 : 0;
   flags.xop = (ecx & bit(11)) ? 1 : 0;
   flags.fma4 = (ecx & bit(16)) ? 1 : 0;

   // Remove some warnings
   flags.ebx = 0;
   flags.ecx = 0;
   flags.edx = 0;

   return flags;
}

// Checks whether an instruction set is supported by host (from CPUID)
boolean_t utils_is_iset_supported_by_host(uint8_t iset)
{
   static uarch_flags flags;
   static int cpuid_done = 0;

   if (cpuid_done == 0) {
      flags = get_cpuid_flags();
      cpuid_done = 1;
   } else
      // remove warning when no ISET defined
      (void) flags;

   switch (iset) {
#ifdef ISET_3DNow
   case ISET_3DNow:
      return (flags.threed_now ? TRUE : FALSE);
#endif
#ifdef ISET_8086
   case ISET_8086:
      return (flags.vme ? TRUE : FALSE);
#endif
#ifdef ISET_8087
   case ISET_8087:
      return (flags.fpu ? TRUE : FALSE);
#endif
#ifdef ISET_AES
   case ISET_AES:
      return (flags.aes ? TRUE : FALSE);
#endif
#ifdef ISET_AES_AVX
   case ISET_AES_AVX:
      return ((flags.aes && flags.avx) ? TRUE : FALSE);
#endif
#ifdef ISET_AVX512BW
   case ISET_AVX512BW:
      return (flags.avx512bw ? TRUE : FALSE);
#endif
#ifdef ISET_AVX512CD
   case ISET_AVX512CD:
      return (flags.avx512cd ? TRUE : FALSE);
#endif
#ifdef ISET_AVX512DQ
   case ISET_AVX512DQ:
      return (flags.avx512dq ? TRUE : FALSE);
#endif
#ifdef ISET_AVX512ER
   case ISET_AVX512ER:
      return (flags.avx512er ? TRUE : FALSE);
#endif
#ifdef ISET_AVX512PF
   case ISET_AVX512PF:
      return (flags.avx512pf ? TRUE : FALSE);
#endif
#ifdef ISET_AVX512F
   case ISET_AVX512F:
      return (flags.avx512f ? TRUE : FALSE);
#endif
#ifdef ISET_BMI1
   case  ISET_BMI1:
      return (flags.bmi1 ? TRUE : FALSE);
#endif
#ifdef ISET_BMI2
   case ISET_BMI2:
      return (flags.bmi2 ? TRUE : FALSE);
#endif
#ifdef ISET_CLMUL
   case ISET_CLMUL:
      return (flags.pclmulqdq ? TRUE : FALSE);
#endif
#ifdef ISET_CLMUL_AVX
   case ISET_CLMUL_AVX:
      return ((flags.pclmulqdq && flags.avx) ? TRUE : FALSE);
#endif
#ifdef ISET_F16C
   case ISET_F16C:
      return (flags.f16c ? TRUE : FALSE);
#endif
#ifdef ISET_FMA
   case ISET_FMA:
      return (flags.fma ? TRUE : FALSE);
#endif
#ifdef ISET_FMA4
   case ISET_FMA4:
      return (flags.fma4 ? TRUE : FALSE);
#endif
#ifdef ISET_FSGSBASE
   case ISET_FSGSBASE:
      return (flags.fsgsbase ? TRUE : FALSE);
#endif
#ifdef ISET_INVPCID
   case ISET_INVPCID:
      return (flags.invpcid ? TRUE : FALSE);
#endif
#ifdef ISET_LZCNT
   case ISET_LZCNT:
      return (flags.abm ? TRUE : FALSE);
#endif
#ifdef ISET_MMX
   case  ISET_MMX:
      return (flags.mmx ? TRUE : FALSE);
#endif
#ifdef ISET_RDRAND
   case ISET_RDRAND:
      return (flags.rdrand ? TRUE : FALSE);
#endif
#ifdef ISET_RTM
   case ISET_RTM:
      return (flags.rtm ? TRUE : FALSE);
#endif
#ifdef ISET_SMX
   case ISET_SMX:
      return (flags.smx ? TRUE : FALSE);
#endif
#ifdef ISET_VMX
   case ISET_VMX:
      return (flags.vmx ? TRUE : FALSE);
#endif
#ifdef ISET_XOP
   case ISET_XOP:
      return (flags.xop ? TRUE : FALSE);
#endif
#ifdef ISET_XSAVEOPT
   //case ISET_XSAVEOPT:
   //   return (flags.fxsr_opt ? TRUE : FALSE);
#endif
#ifdef ISET_SSE
   case ISET_SSE:
      return (flags.sse ? TRUE : FALSE);
#endif
#ifdef ISET_SSE2
   case ISET_SSE2:
      return (flags.sse2 ? TRUE : FALSE);
#endif
#ifdef ISET_SSE3
   case ISET_SSE3:
      return (flags.sse3 ? TRUE : FALSE);
#endif
#ifdef ISET_SSSE3
   case ISET_SSSE3:
      return (flags.ssse3 ? TRUE : FALSE);
#endif
#ifdef ISET_SSE4_1
   case ISET_SSE4_1:
      return (flags.sse4_1 ? TRUE : FALSE);
#endif
#ifdef ISET_SSE4_2
   case ISET_SSE4_2:
      return (flags.sse4_2 ? TRUE : FALSE);
#endif
#ifdef ISET_AVX
   case ISET_AVX:
      return (flags.avx ? TRUE : FALSE);
#endif
#ifdef ISET_AVX2
   case ISET_AVX2:
      return (flags.avx2 ? TRUE : FALSE);
#endif

   default:
      WRNMSG("Unknown ISET !\n");
      return FALSE;
   }
}

// Converts an as (assembler) flag to a MAQAO instruction set
uint8_t utils_as_flag_to_iset(const char *flag_name)
{
   static uarch_flags flags;
   static int cpuid_done = 0;
   uint8_t iset = 0;

   if (cpuid_done == 0) {
      flags = get_cpuid_flags();
      cpuid_done = 1;
   } else
      // remove warning when no ISET defined
      (void) flags;

   if (FALSE) {
   }
#ifdef ISET_3DNow
   else if (strcmp (flag_name,"3dnow")) {
      if (flags.threed_now == 1)
         iset = ISET_3DNow;
   }
#endif
#ifdef ISET_8086
   else if (strcmp (flag_name,"i8086")) {
      if (flags.vme == 1)
         iset = ISET_8086;
   }
#endif
#ifdef ISET_8087
   else if (strcmp (flag_name,"8087")) {
      if (flags.fpu == 1)
         iset = ISET_8087;
   }
#endif
#ifdef ISET_AES
   else if (strcmp (flag_name,"aes")) {
      if (flags.aes == 1)
         iset = ISET_AES;
   }
#endif
#ifdef ISET_AVX512BW
   else if (strcmp (flag_name,"avx512bw")) {
      if (flags.avx512bw == 1)
         iset = ISET_AVX512BW;
   }
#endif
#ifdef ISET_AVX512CD
   else if (strcmp (flag_name,"avx512cd")) {
      if (flags.avx512cd == 1)
         iset = ISET_AVX512CD;
   }
#endif
#ifdef ISET_AVX512DQ
   else if (strcmp (flag_name,"avx512dq")) {
      if (flags.avx512dq == 1)
         iset = ISET_AVX512DQ;
   }
#endif
#ifdef ISET_AVX512F
   else if (strcmp (flag_name,"avx512f")) {
      if (flags.avx512f == 1)
         iset = ISET_AVX512F;
   }
#endif
#ifdef ISET_BMI1
   else if (strcmp (flag_name,"bmi")) {
      if (flags.bmi1 == 1)
         iset = ISET_BMI1;
   }
#endif
#ifdef ISET_BMI2
   else if (strcmp (flag_name,"bmi2")) {
      if (flags.bmi2 == 1)
         iset = ISET_BMI2;
   }
#endif
#ifdef ISET_CLMUL
   else if (strcmp (flag_name,"pclmul")) {
      if (flags.pclmulqdq == 1)
         iset = ISET_CLMUL;
   }
#endif
#ifdef ISET_F16C
   else if (strcmp (flag_name,"f16c")) {
      if (flags.f16c == 1)
         iset = ISET_F16C;
   }
#endif
#ifdef ISET_FMA
   else if (strcmp (flag_name,"fma")) {
      if (flags.fma == 1)
         iset = ISET_FMA;
   }
#endif
#ifdef ISET_FMA4
   else if (strcmp (flag_name,"fma4")) {
      if (flags.fma4 == 1)
         iset = ISET_FMA4;
   }
#endif
#ifdef ISET_FSGSBASE
   else if (strcmp (flag_name,"fsgsbase")) {
      if (flags.fsgsbase == 1)
         iset = ISET_FSGSBASE;
   }
#endif
#ifdef ISET_INVPCID
   else if (strcmp (flag_name,"invpcid")) {
      if (flags.invpcid == 1)
         iset = ISET_INVPCID;
   }
#endif
#ifdef ISET_LZCNT
   else if (strcmp (flag_name,"lzcnt")) {
      if (flags.abm == 1)
         iset = ISET_LZCNT;
   }
#endif
#ifdef ISET_MMX
   else if (strcmp (flag_name,"mmx")) {
      if (flags.mmx == 1)
         iset = ISET_MMX;
   }
#endif
#ifdef ISET_RDRAND
   else if (strcmp (flag_name,"rdrnd")) {
      if (flags.rdrand == 1)
         iset = ISET_RDRAND;
   }
#endif
#ifdef ISET_RTM
   else if (strcmp (flag_name,"rtm")) {
      if (flags.rtm == 1)
         iset = ISET_RTM;
   }
#endif
#ifdef ISET_SMX
   else if (strcmp (flag_name,"smx")) {
      if (flags.smx == 1)
         iset = ISET_SMX;
   }
#endif
#ifdef ISET_VMX
   else if (strcmp (flag_name,"vmx")) {
      if (flags.vmx == 1)
         iset = ISET_VMX;
   }
#endif
#ifdef ISET_XOP
   else if (strcmp (flag_name,"xop")) {
      if (flags.xop == 1)
         iset = ISET_XOP;
   }
#endif
#ifdef ISET_SSE
   else if (strcmp (flag_name,"sse")) {
      if (flags.sse == 1)
         iset = ISET_SSE;
   }
#endif
#ifdef ISET_SSE2
   else if (strcmp (flag_name,"sse2")) {
      if (flags.sse2 == 1)
         iset = ISET_SSE2;
   }
#endif
#ifdef ISET_SSE3
   else if (strcmp (flag_name,"sse3")) {
      if (flags.sse3 == 1)
         iset = ISET_SSE3;
   }
#endif
#ifdef ISET_SSSE3
   else if (strcmp (flag_name,"ssse3")) {
      if (flags.ssse3 == 1)
         iset = ISET_SSSE3;
   }
#endif
#ifdef ISET_SSE4_1
   else if (strcmp (flag_name,"sse4.1")) {
      if (flags.sse4_1 == 1)
         iset = ISET_SSE4_1;
   }
#endif
#ifdef ISET_SSE4_2
   else if (strcmp (flag_name,"sse4.2")) {
      if (flags.sse4_2 == 1)
         iset = ISET_SSE4_2;
   }
#endif
#ifdef ISET_AVX
   else if (strcmp (flag_name,"avx")) {
      if (flags.avx == 1)
         iset = ISET_AVX;
   }
#endif
#ifdef ISET_AVX2
   else if (strcmp (flag_name,"avx2")) {
      if (flags.avx2 == 1)
         iset = ISET_AVX2;
   }
#endif

   return iset;
}

/*
 */
/* int utils_uarch_flags(uarch_flags *flags) */
/* { */
/*   // */
/*   uarch_flags *f = flags; */
/*   unsigned ebx = 0, ecx0 = 0, ecx1 = 0, edx = 0; */

/*   // EDX & ECX */
/*   __asm (".intel_syntax noprefix;" */
/* 	 "mov eax, 1 ;" */
/* 	 "cpuid;" */
/* 	 "att_syntax prefix;" */
/* 	 : "=r" (edx), "=r" (ecx0) // output */
/* 	 :  // input */
/* 	 : "%eax"); // clobber */

/*   return 0; */
/* } */

/*
 * Uses CPUID assembly instruction to found local processor version
 * \return NULL if processor version can not be retrieved, else a structure representing the processor version
 */
proc_t* utils_get_proc_host()
{
   //Invoking the appropriate architecture-specific function depending on the host architecture (if recognised)
   /**\note (2016-12-13) The compiler preprocessing directives identifying the host architecture (__x86_64, __MIC__, ...)
    * are supposed to be set exclusively, therefore only one of the function calls below at most should be compiled*/
#if (defined(__x86_64__) || defined(_M_X64)) && defined(_ARCHDEF_x86_64)
   return x86_64_utils_get_proc_host();
#endif
#if defined(__MIC__) && defined(_ARCHDEF_k1om)
   return k1om_utils_get_proc_host();
#endif
#if (defined(__i386__) || defined(_M_IX86)) && defined(_ARCHDEF_ia32)
   return ia32_utils_get_proc_host();
#endif
#if defined(__ARM_ARCH) && defined(_ARCHDEF_arm64)
   return arm64_utils_get_proc_host();
#endif
   /**\todo (2017-04-25) Add arm and thumb*/
   return NULL;
}

/*
 * Uses CPUID the assembly instruction or the bogomips to get the frequency of the CPU
 * In priority the CPUID assembly instruction is used.
 * \return a string to free or NULL
 */
char* utils_get_cpu_frequency()
{
#ifndef _WIN32
   //DETECTION OF THE CPU FREQUENCY USING THE CPUID
#if defined(__x86_64__) && !defined(__MIC__)
   uint32_t a, b, d, c;
   int size = (12 * sizeof(int)) + sizeof(char);
   char* str = malloc(size);
   memset(str, 0, size);
   a = 0;
   __asm ("mov $0x80000000, %%eax; "
          "cpuid;"
          "mov %%eax, %0;"
          :"=r"(a)
          :
          :"%eax"
      );

   if (a >= 0x80000004) {
      int i = 0;
      int val = 0x80000002;
      for (i = 0; i < 3; i++) {
         __asm ("mov %4, %%eax; "
                "cpuid;"
                "mov %%eax, %0;"
                "mov %%ebx, %1;"
                "mov %%edx, %2;"
                "mov %%ecx, %3;"
                :"=r"(a), "=r"(b), "=r"(d), "=r"(c)
                :"r"(val + i)
                :"%eax","%ebx","%ecx","%edx"
            );
         memcpy(&str[((i * 4) + 0) * sizeof(uint32_t)], &a, sizeof(uint32_t));
         memcpy(&str[((i * 4) + 1) * sizeof(uint32_t)], &b, sizeof(uint32_t));
         memcpy(&str[((i * 4) + 2) * sizeof(uint32_t)], &c, sizeof(uint32_t));
         memcpy(&str[((i * 4) + 3) * sizeof(uint32_t)], &d, sizeof(uint32_t));
      }
      if (str != NULL)
         return (str);
   } else {
      printf("Processor brand string is not supported for this processor\n");
   }
#endif
   //DETECTION OF THE CPU FREQUENCY USING /PROC/CPUINFO AND BOGOMIPS
   char line[MAX_SIZE] = "";
   char str1[128];
   char str2[128];
   char* endPtr;
   char* strCpuFrequency = malloc(128 * sizeof(char));
   FILE* cpuInfoFile = fopen("/proc/cpuinfo", "r");
   if (cpuInfoFile != NULL) {
      while (fgets(line, MAX_SIZE, cpuInfoFile) != NULL) {
         if (strstr(line, "bogomips")) {
            sscanf(line, "%s\t: %s", str1, str2);
            break;
         }
      }
      fclose(cpuInfoFile);
   }
   // Need to do conversion to get the equivalent of CPUID info (MHz)
   int bogomips = strtol(str2, &endPtr, 10);
   if (endPtr != str2) {
      int roundingBogomips = bogomips + abs((bogomips % 100) - 100);
      float cpuFrequency = roundingBogomips / 2; //2 because 2 per cycles
      cpuFrequency = cpuFrequency / 1000;
      sprintf(strCpuFrequency, "%f", cpuFrequency);
   } else {
      //DETECTION OF THE CPU FREQUENCY USING /SYS/DEVICES/SYSTEM/CPU/CPU0/CPUFREQ/SCALING_MAX_FREQ
      FILE* scalingMaxFreqInfoFile = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "r");

      if (scalingMaxFreqInfoFile != NULL)
      {
         if (fgets(line, MAX_SIZE, scalingMaxFreqInfoFile) != NULL) {
            int maxFrequency = strtol(line, NULL, 10);
            float maxFrequencyFloat = (float)maxFrequency / 1000000;
            sprintf(strCpuFrequency, "%f", maxFrequencyFloat);
         }
         fclose(scalingMaxFreqInfoFile);
      } else {
         fprintf(stderr,
                 "[MAQAO] : Error during the detection of the CPU Frequency!\n");
         return NULL;
      }
   }

   return strCpuFrequency;
#else
   return NULL;
#endif
}

#ifdef _WIN32
typedef struct {
   PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer;
   DWORD                                 returnLength;
} windows_info_buffer_t;

typedef BOOL(WINAPI *LPFN_GLPI)(
   PSYSTEM_LOGICAL_PROCESSOR_INFORMATION,
   PDWORD);

static windows_info_buffer_t get_windows_info_buffer()
{
   PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
   DWORD returnLength = 0;

   // Check whether logical processor info retrieval feature is available
   LPFN_GLPI glpi = (LPFN_GLPI)GetProcAddress(
      GetModuleHandle(TEXT("kernel32")),
      "GetLogicalProcessorInformation");
   if (glpi == NULL) {
      fprintf (stderr, "GetLogicalProcessorInformation is not supported.\n");
      windows_info_buffer_t wib = { buffer, returnLength };
      return wib;
   }

   // Write logical processor info to buffer
   BOOL done = FALSE;
   while (!done) {
      DWORD ret = glpi(buffer, &returnLength);

      if (ret == FALSE) {
         if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            if (buffer) free(buffer);

            buffer = malloc(returnLength);
            if (buffer == NULL) {
               fprintf (stderr, "Error: Allocation failure\n");
               break;
            }
         }
         else {
            fprintf (stderr, "Error %d\n", GetLastError());
            break;
         }
      }
      else {
         done = TRUE;
      }
   }

   windows_info_buffer_t wib = { buffer, returnLength };
   return wib;
}
#endif

#ifndef _WIN32
static FILE *get_file(const char *path, const char *filename)
{
   char *full_path = malloc(strlen(path) + strlen(filename) + 1);
   strcpy(full_path, path);
   strcat(full_path, filename);

   FILE *fp = fopen(full_path, "r");
   free(full_path);

   return fp;
}

static char *read_string(const char *path, const char *filename, char *buf)
{
   FILE *fp = get_file(path, filename);
   if (!fp)
      return NULL;

   int ret = fscanf(fp, "%s", buf);
   fclose(fp);

   return (ret == 1 ? buf : NULL);
}

static int read_int(const char *path, const char *filename)
{
   FILE *fp = get_file(path, filename);
   if (!fp)
      return -1;

   int val, ret = fscanf(fp, "%d", &val);
   fclose(fp);

   return (ret == 1 ? val : -1);
}

// TODO: compile only when compiled for Linux
static int cache_is_core_private(char *shared_cpu_list)
{
   const char *cpu_info_path =
      "/sys/devices/system/cpu/cpu0/topology/thread_siblings_list";
   FILE *fp = fopen(cpu_info_path, "r");
   if (!fp)
      return -1;

   char buf[128];
   char *ret = fgets(buf, sizeof(buf), fp);
   fclose(fp);

   if (!ret)
      return -1;

   buf[strlen(buf) - 1] = '\0'; // removes the final newline character
   return (strcmp(buf, shared_cpu_list) == 0 ? TRUE : FALSE);
}

static int utils_set_cache_info_linux(udc_cache_entries_t *entries)
{
   uint8_t index_entry_nb = 0;

   // check cache info directory
   const char *cache_info_path = "/sys/devices/system/cpu/cpu0/cache/";
   DIR* dir = opendir(cache_info_path);
   if (!dir)
      return -1;

   char buf[1024];

   struct dirent *dp;
   while ((dp = readdir(dir)) != NULL) {
      if (strstr(dp->d_name, "index") == NULL)
         continue;

      char *path = malloc(strlen(cache_info_path) + strlen(dp->d_name) + 2);
      strcpy(path, cache_info_path);
      strcat(path, dp->d_name);
      strcat(path, "/");

      char *str; // return value for read_string
      udc_index_entry_t *cur_entry = &(entries->index[index_entry_nb]);

      // allocation_policy
      str = read_string(path, "allocation_policy", buf);
      if (str != NULL) {
         if (strcmp(str, "WriteAllocate") == 0)
            cur_entry->allocation_policy = UDC_WR_ALLOC;
         else if (strcmp(str, "ReadAllocate") == 0)
            cur_entry->allocation_policy = UDC_RD_ALLOC;
         else if (strcmp(str, "ReadWriteAllocate") == 0)
            cur_entry->allocation_policy = UDC_RW_ALLOC;
      } else
         cur_entry->allocation_policy = UDC_UNDEF_ALLOC_POL;

      // coherency_line_size
      cur_entry->coherency_line_size = read_int(path, "coherency_line_size");

      // level
      cur_entry->level = read_int(path, "level");

      // number_of_sets
      cur_entry->number_of_sets = read_int(path, "number_of_sets");

      // physical_line_partition
      cur_entry->physical_line_partition = read_int(path,
                                                    "physical_line_partition");

      // shared_cpu_list
      str = read_string(path, "shared_cpu_list", buf);
      strcpy(cur_entry->shared_cpu_list, str ? str : "");
      cur_entry->is_core_private = cache_is_core_private(str);

      // shared_cpu_map
      str = read_string(path, "shared_cpu_map", buf);
      strcpy(cur_entry->shared_cpu_map, str ? str : "");

      // size
      cur_entry->size = read_int(path, "size");

      // type
      str = read_string(path, "type", buf);
      if (str != NULL) {
         if (strcmp(str, "Data") == 0)
            cur_entry->type = UDC_DATA;
         else if (strcmp(str, "Instruction") == 0)
            cur_entry->type = UDC_INSTRUCTION;
         else if (strcmp(str, "Unified") == 0)
            cur_entry->type = UDC_UNIFIED;
      } else
         cur_entry->type = UDC_UNDEF_TYPE;

      // ways_of_associativity
      cur_entry->ways_of_associativity = read_int(path,
                                                  "ways_of_associativity");

      // write_policy
      str = read_string(path, "write_policy", buf);
      if (str != NULL) {
         if (strcmp(str, "WriteThrough") == 0)
            cur_entry->write_policy = UDC_WRITE_THROUGH;
         else if (strcmp(str, "WriteBack") == 0)
            cur_entry->write_policy = UDC_WRITE_BACK;
      } else
         cur_entry->write_policy = UDC_UNDEF_WRITE_POL;

      free(path);
      index_entry_nb++;
   }

   entries->index_entry_nb = index_entry_nb;
   closedir(dir);

   return 0;
}
#else
static int utils_set_cache_info_windows(udc_cache_entries_t *entries)
{
   windows_info_buffer_t wib = get_windows_info_buffer ();
   if (wib.buffer == NULL) return -1;

   DWORD byteOffset = 0;

   // Index entry corresponding to a given level (max 4)
   // + type (data, instruction, unified and trace) => (L1D, L1I, L2...)
   udc_index_entry_t *index_entry [4][4];
   memset (index_entry, 0, sizeof index_entry);

   // Number of caches corresponding to a given level + type
   unsigned cache_count [4][4];
   memset (cache_count, 0, sizeof cache_count);

   // Number of (physical) cores
   unsigned core_count = 0;

   uint8_t index_entry_nb = 0;
   PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = wib.buffer;
   while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= wib.returnLength)
   {
      switch (ptr->Relationship) {
      case RelationCache:
         // Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache.
         PCACHE_DESCRIPTOR Cache = &ptr->Cache;

         cache_count [Cache->Level-1] [Cache->Type]++;

         // Insert only first for a given level + type
         if (index_entry [Cache->Level-1] [Cache->Type] != NULL) {
            byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
            ptr++;
            continue;
         }

         udc_index_entry_t *cur_entry = &(entries->index[index_entry_nb]);

         // allocation_policy
         cur_entry->allocation_policy = UDC_UNDEF_ALLOC_POL;

         // coherency_line_size
         cur_entry->coherency_line_size = Cache->LineSize;

         // level
         cur_entry->level = Cache->Level;

         // size (in KB)
         cur_entry->size = Cache->Size / 1024;

         // type
         switch (Cache->Type) {
         case CacheUnified:
            cur_entry->type = UDC_UNIFIED;
            break;
         case CacheInstruction:
            cur_entry->type = UDC_INSTRUCTION;
            break;
         case CacheData:
            cur_entry->type = UDC_DATA;
            break;
         default:
            cur_entry->type = UDC_UNDEF_TYPE;
         }

         // ways_of_associativity
         cur_entry->ways_of_associativity = Cache->Associativity;

         // write_policy
         cur_entry->write_policy = UDC_UNDEF_WRITE_POL;

         index_entry [Cache->Level-1][Cache->Type] = cur_entry;
         index_entry_nb++;
         break;

      case RelationProcessorCore:
         core_count++;
         break;
      }

      byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
      ptr++;
   }

   // A cache is core-private if, by definition,
   // number of such caches equals (physical) cores count
   int lvl, type;
   for (lvl=0; lvl<4; lvl++)
      for (type=0; type<3; type++) {
         udc_index_entry_t *cur_entry = index_entry [lvl][type];
         if (cur_entry != NULL) {
            cur_entry->is_core_private =
               (cache_count [lvl][type] == core_count) ? TRUE : FALSE;
         }
      }

   entries->index_entry_nb = index_entry_nb;
   free (wib.buffer);
   return 0;
}
#endif

// OS-independent
int utils_set_cache_info(udc_cache_entries_t *entries)
{
#ifdef _WIN32
   return utils_set_cache_info_windows(entries);
#else
   return utils_set_cache_info_linux(entries);
#endif
}

// Returns size in KB for the given level
uint32_t utils_get_data_cache_size(udc_cache_entries_t *entries, uint8_t level)
{
   int i;
   for (i = 0; i < entries->index_entry_nb; i++) {
      udc_index_entry_t cur_entry = entries->index[i];
      if (cur_entry.level == level
          && (cur_entry.type == UDC_DATA || cur_entry.type == UDC_UNIFIED))
         return cur_entry.size;
   }

   return 0;
}

// Returns number of data (or unified) cache levels
uint8_t utils_get_data_cache_nb_levels(udc_cache_entries_t *entries)
{
   int i;
   for (i = entries->index_entry_nb - 1; i >= 0; i--) {
      udc_index_entry_t cur_entry = entries->index[i];
      if (cur_entry.type == UDC_DATA || cur_entry.type == UDC_UNIFIED)
         return cur_entry.level;
   }

   return 0;
}

#ifdef _WIN32
static uint32_t get_nb_sockets_windows () {
   windows_info_buffer_t wib = get_windows_info_buffer ();
   if (wib.buffer == NULL) return 0;

   uint32_t nb = 0;
   DWORD byteOffset = 0;
   PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = wib.buffer;
   while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= wib.returnLength) {
      if (ptr->Relationship == RelationProcessorPackage) nb++;

      byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
      ptr++;
   }

   free (wib.buffer);
   return nb;
}
#else
static uint32_t get_nb_sockets_linux() {
   // check cache info directory
   const char *cache_info_path = "/sys/devices/system/cpu/";
   DIR* dir1 = opendir(cache_info_path);
   if (!dir1) return 0;

   int max = -1;

   struct dirent *dp1, *dp2;
   while ((dp1 = readdir(dir1)) != NULL) {
      unsigned cpu_id;
      if (sscanf (dp1->d_name, "cpu%u", &cpu_id) != 1)
         continue;

      char *path = malloc(strlen(cache_info_path) + strlen(dp1->d_name) + strlen("topology") + 3);
      strcpy(path, cache_info_path);
      strcat(path, dp1->d_name);
      strcat(path, "/");
      strcat(path, "topology");
      strcat(path, "/");

      DIR* dir2 = opendir(path);
      if (!dir2) return 0;
      while ((dp2 = readdir(dir2)) != NULL) {
         if (strcmp (dp2->d_name, "physical_package_id") != 0)
            continue;

         int socket_id = read_int (path, "physical_package_id");
         if (max == -1 || max < socket_id)
            max = socket_id;
      }

      free(path);
      closedir(dir2);
   }

   closedir(dir1);

   return max + 1;
}
#endif

// Returns number of sockets (physical processor packages)
uint32_t utils_get_nb_sockets()
{
#ifdef _WIN32
   return get_nb_sockets_windows();
#else
   return get_nb_sockets_linux();
#endif
}
