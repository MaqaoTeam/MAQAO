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

#ifndef __LMAQAO_UARCH_DETECTOR_H__
#define __LMAQAO_UARCH_DETECTOR_H__

/* Intel-defined CPU features, CPUID level 0x00000001 (edx) */
#define  X86_FEATURE_FPU  (0) /* Onboard FPU */
#define  X86_FEATURE_VME  (1) /* Virtual Mode Extensions */
#define  X86_FEATURE_DE   (2) /* Debugging Extensions */
#define  X86_FEATURE_PSE  (3) /* Page Size Extensions */
#define  X86_FEATURE_TSC  (4) /* Time Stamp Counter */
#define  X86_FEATURE_MSR  (5) /* Model-Specific Registers */
#define  X86_FEATURE_PAE  (6) /* Physical Address Extensions */
#define  X86_FEATURE_MCE  (7) /* Machine Check Exception */
#define  X86_FEATURE_CX8  (8) /* CMPXCHG8 instruction */
#define  X86_FEATURE_APIC (9) /* Onboard APIC */
#define  X86_FEATURE_SEP  (11) /* SYSENTER/SYSEXIT */
#define  X86_FEATURE_MTRR (12) /* Memory Type Range Registers */
#define  X86_FEATURE_PGE  (13) /* Page Global Enable */
#define  X86_FEATURE_MCA  (14) /* Machine Check Architecture */
#define  X86_FEATURE_CMOV (15) /* CMOV instructions */
/* (plus FCMOVcc, FCOMI with FPU) */
#define  X86_FEATURE_PAT        (16)  /* Page Attribute Table */
#define  X86_FEATURE_PSE36      (17)  /* 36-bit PSEs */
#define  X86_FEATURE_PN         (18)  /* Processor serial number */
#define  X86_FEATURE_CLFLUSH    (19)  /* CLFLUSH instruction */
#define  X86_FEATURE_DS         (21)  /* "dts" Debug Store */
#define  X86_FEATURE_ACPI       (22)  /* ACPI via MSR */
#define  X86_FEATURE_MMX        (23)  /* Multimedia Extensions */
#define  X86_FEATURE_FXSR       (24)  /* FXSAVE/FXRSTOR, CR4.OSFXSR */
#define  X86_FEATURE_XMM        (25)  /* "sse" */
#define  X86_FEATURE_XMM2       (26)  /* "sse2" */
#define  X86_FEATURE_SELFSNOOP  (27)  /* "ss" CPU self snoop */
#define  X86_FEATURE_HT         (28)  /* Hyper-Threading */
#define  X86_FEATURE_ACC        (29)  /* "tm" Automatic clock control */
#define  X86_FEATURE_IA64       (30)  /* IA-64 processor */
#define   X86_FEATURE_PBE       (31) /* Pending Break Enable */

/* Intel-defined CPU features, CPUID level 0x00000001 (ecx)*/
#define X86_FEATURE_XMM3               (0) /* "pni" SSE-3 */
#define X86_FEATURE_PCLMULQDQ          (1) /* PCLMULQDQ instruction */
#define X86_FEATURE_DTES64             (2) /* 64-bit Debug Store */
#define X86_FEATURE_MWAIT              (3) /* "monitor" Monitor/Mwait support */
#define X86_FEATURE_DSCPL              (4) /* "ds_cpl" CPL Qual. Debug Store */
#define X86_FEATURE_VMX                (5) /* Hardware virtualization */
#define X86_FEATURE_SMX                (6) /* Safer mode */
#define X86_FEATURE_EST                (7) /* Enhanced SpeedStep */
#define X86_FEATURE_TM2                (8) /* Thermal Monitor 2 */
#define X86_FEATURE_SSSE3              (9) /* Supplemental SSE-3 */
#define X86_FEATURE_CID                (10) /* Context ID */
#define X86_FEATURE_SDBG               (11) //NOT DEFINE IN cpufeature.h /* Silicon Debug interface */
#define X86_FEATURE_FMA                (12) /* Fused multiply-add */
#define X86_FEATURE_CX16               (13) /* CMPXCHG16B */
#define X86_FEATURE_XTPR               (14) /* Send Task Priority Messages */
#define X86_FEATURE_PDCM               (15) /* Performance Capabilities */
#define X86_FEATURE_PCID               (17) /* Process Context Identifiers */
#define X86_FEATURE_DCA                (18) /* Direct Cache Access */
#define X86_FEATURE_XMM4_1             (19) /* "sse4_1" SSE-4.1 */
#define X86_FEATURE_XMM4_2             (20) /* "sse4_2" SSE-4.2 */
#define X86_FEATURE_X2APIC             (21) /* x2APIC */
#define X86_FEATURE_MOVBE              (22) /* MOVBE instruction */
#define X86_FEATURE_POPCNT             (23) /* POPCNT instruction */
#define X86_FEATURE_TSC_DEADLINE_TIMER (24) /* Tsc deadline timer */
#define X86_FEATURE_AES                (25) /* AES instructions */
#define X86_FEATURE_XSAVE              (26) /* XSAVE/XRSTOR/XSETBV/XGETBV */
#define X86_FEATURE_OSXSAVE            (27) /* "" XSAVE enabled in the OS */
#define X86_FEATURE_AVX                (28) /* Advanced Vector Extensions */
#define X86_FEATURE_F16C               (29) /* 16-bit fp conversions */
#define X86_FEATURE_RDRAND             (30) /* The RDRAND instruction */
#define X86_FEATURE_HYPERVISOR         (31) /* Running on a hypervisor */

// Intel defined CPU features : CPUID EAX=7 , ECX = 0
// READ EBX
#define X86_FEATURE_FSGSBASE     (0) /* {RD/WR}{FS/GS}BASE instructions*/
#define X86_FEATURE_TSC_ADJUST   (1) /* TSC adjustment MSR 0x3b */
#define X86_FEATURE_BMI1         (3) /* 1st group bit manipulation extensions */
#define X86_FEATURE_HLE          (4) /* Hardware Lock Elision */
#define X86_FEATURE_AVX2         (5) /* AVX2 instructions */
#define X86_FEATURE_SMEP         (7) /* Supervisor Mode Execution Protection */
#define X86_FEATURE_BMI2         (8) /* 2nd group bit manipulation extensions */
#define X86_FEATURE_ERMS         (9) /* Enhanced REP MOVSB/STOSB */
#define X86_FEATURE_INVPCID     (10) /* Invalidate Processor Context ID */
#define X86_FEATURE_RTM         (11) /* Restricted Transactional Memory */
#define X86_FEATURE_CQM         (12) /* Cache QoS Monitoring */
#define X86_FEATURE_MPX         (14) /* Memory Protection Extension */
#define X86_FEATURE_AVX512F     (16) /* AVX-512 Foundation */
#define X86_FEATURE_AVX512DQ    (17) // NOT DEFINE IN cpufeature.h /* AVX-512 Doubleword and Quadword Instructions */ 
#define X86_FEATURE_RDSEED      (18) /* The RDSEED instruction */
#define X86_FEATURE_ADX         (19) /* The ADCX and ADOX instructions */
#define X86_FEATURE_SMAP        (20) /* Supervisor Mode Access Prevention */
#define X86_FEATURE_AVX512IFMA  (21) // NOT DEFINE IN cpufeature.h /* AVX-512 Integer Fused Multiply-Add Instructions */
#define X86_FEATURE_PCOMMIT     (22) /* PCOMMIT instruction */
#define X86_FEATURE_CLFLUSHOPT  (23) /* CLFLUSHOPT instruction */
#define X86_FEATURE_CLWB        (24) /* CLWB instruction */
#define X86_FEATURE_PT          (25) //NOT DEFINE IN cpufeature.h /* Intel Processor Trace */
#define X86_FEATURE_AVX512PF    (26) /* AVX-512 Prefetch */
#define X86_FEATURE_AVX512ER    (27) /* AVX-512 Exponential and Reciprocal */
#define X86_FEATURE_AVX512CD    (28) /* AVX-512 Conflict Detection */
#define X86_FEATURE_SHA         (29) /* Intel SHA extensions */
#define X86_FEATURE_AVX512BW    (30) /* AVX-512 Byte and Word Instruction */
#define X86_FEATURE_AVX512VL    (31) /* Vector Length Extensions */

// READ ECX
#define X86_FEATURE_PREFETCHWT1 (0) /* PREFETCHWT1 instruction */
#define X86_FEATURE_AVX512VBMI  (1) /* Vector Bit Manipulation Instructions */

//Smallest type in C
typedef unsigned char bool;

typedef struct {

   /* If CPUID called with EAX = 1, returns general flags */

   // Keep the raw output ?
   unsigned edx;
   unsigned ecx;
   unsigned ebx;

   // features returned in EDX !
   bool fpu;
   bool vme;
   bool de;
   bool pse;
   bool tsc;
   bool msr;
   bool pae;
   bool mce;
   bool cx8;
   bool apic;
   // bit 10 is reserved !
   bool sep;
   bool mtrr;
   bool pge;
   bool mca;
   bool cmov;
   bool pat;
   bool pse36;
   bool psn;
   bool clflush;
   // bit 20 is reserved !
   bool ds;
   bool acpi;
   bool mmx;
   bool fxsr;
   bool sse;
   bool sse2;
   bool ss;
   bool ht;
   bool tm;
   bool ia64;
   bool pbe;

   // features returned in ECX !
   bool sse3;
   bool pclmulqdq;
   bool dtes64;
   bool monitor;
   bool ds_cpl;
   bool vmx;
   bool smx;
   bool est;
   bool tm2;
   bool ssse3;
   bool cnxt_id;
   bool sdbg;
   bool fma;
   bool cx16;
   bool xtpr;
   bool pdcm;
   // bit 16 is reserved ! 
   bool pcid;
   bool dca;
   bool sse4_1;
   bool sse4_2;
   bool x2apic;
   bool movbe;
   bool popcnt;
   bool tsc_deadline;
   bool aes;
   bool xsave;
   bool osxsave;
   bool avx;
   bool f16c;
   bool rdrand;
   bool hypervisor;

   /* If CPUID called with EAX = 7 and ECX = 0, returns AVX related flags */

   // features returned in EBX !
   bool fsgsbase;
   // bit 1 is reserved !
   // bit 2 is reserved !
   bool bmi1;
   bool hle;
   bool avx2;
   // bit 6 is reserved !
   bool smep;
   bool bmi2;
   bool erms;
   bool invpcid;
   bool rtm;
   bool cqm;
   // bit 12 is reserved !
   // bit 13 is reserved !
   bool mpx;
   // bit 15 is reserved !
   bool avx512f;
   bool avx512dq;
   bool rdseed;
   bool adx;
   bool smap;
   bool avx512ifma;
   bool pcommit;
   bool clflushopt;
   bool clwb;
   bool pt; //Intel processor trace !
   bool avx512pf;
   bool avx512er;
   bool avx512cd;
   bool sha;
   bool avx512bw;
   bool avx512vl;

   // features returned in ECX !
   bool prefetchwt1;
   bool avx512vbmi;

   bool fxsr_opt;
   bool threed_now;
   bool abm;
   bool xop;
   bool fma4;

} uarch_flags;

uarch_flags get_cpuid_flags();

// UDC: Uarch Detector / Cache
typedef enum {
   UDC_UNDEF_ALLOC_POL = -1, UDC_WR_ALLOC, UDC_RD_ALLOC, UDC_RW_ALLOC
} udc_alloc_policy_t;
typedef enum {
   UDC_UNDEF_TYPE = -1, UDC_INSTRUCTION, UDC_DATA, UDC_UNIFIED
} udc_type_t;
typedef enum {
   UDC_UNDEF_WRITE_POL = -1, UDC_WRITE_THROUGH, UDC_WRITE_BACK
} udc_write_policy_t;

// CF https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-devices-system-cpu
typedef struct {
   udc_alloc_policy_t allocation_policy;
   uint16_t coherency_line_size;
   uint8_t level;
   uint32_t number_of_sets;
   uint8_t physical_line_partition;
   char shared_cpu_list[32]; //TODO: []
   int8_t is_core_private; // -1 if don't know, TRUE/FALSE otherwise
   char shared_cpu_map[256]; //TODO: []
   uint32_t size;
   udc_type_t type;
   uint8_t ways_of_associativity;
   udc_write_policy_t write_policy;
} udc_index_entry_t;

typedef struct {
   uint8_t index_entry_nb;
   udc_index_entry_t index[8]; //TODO: []
} udc_cache_entries_t;

// Checks whether an instruction set is supported by host (from CPUID)
boolean_t utils_is_iset_supported_by_host(uint8_t iset);

// Converts an as (assembler) flag to a MAQAO instruction set
uint8_t utils_as_flag_to_iset(const char *flag_name);

// Sets a structure from cache information data
int utils_set_cache_info(udc_cache_entries_t *entries);

// Returns size in KB for the given level
uint32_t utils_get_data_cache_size(udc_cache_entries_t *entries, uint8_t level);

// Returns size in KB for the given level
uint8_t utils_get_data_cache_nb_levels(udc_cache_entries_t *entries);

// Returns number of sockets (physical processor packages)
uint32_t utils_get_nb_sockets();
#endif
