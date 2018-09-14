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

#ifndef __LA_LIBMASM_H__
#define __LA_LIBMASM_H__

#include "dwarf.h"
#include "libmcommon.h"

//Special case for XeonPhi where whe need the c++ compiler to be replaced by the c one
//The C compiler still consider the cpp file as c++ and that is why this symbol is needed
#ifndef IS_STDCXX
void *__gxx_personality_v0;
#endif

/**
 \file libmasm.h
 \brief This file contains structures used to manipulate an assembly file;
 - an instruction and its operands
 - a function and its abstract structures (blocks, loops...)
 */

#define BAD_INSN "(bad)"            /**<Opcode of an incomplete instruction*/
#define BAD_INSN_CODE SIGNED_ERROR      /**<Opcode of an incomplete instruction*/

#define RIP_TYPE        -2                /**<type value for the RIP register (Relative Instruction Pointer)*/
#define R_RIP   "RIP"                /**<Name of the RIP register to print*/

#define ADDRESS_ERROR   (maddr_t)-1 /**<Value indicating that an address is uninitialised or erroneous*/
#define OFFSET_ERROR    UINT64_MAX  /**<Value indicating that an offset is uninitialised or erroneous*/

//@const_start
#define ISET_NONE 0  /**<Identifier of an erroneous or absent instruction set (instruction sets identifiers begin at 1 by design)*/

#define UARCH_NONE 0 /**<Identifier of an erroneous or absent micro-architecture. DO NOT CHANGE TO OTHER VALUE THAN 0 (used for enums begins)*/
#define PROC_NONE 0  /**<Identifier of an erroneous or absent processor version. DO NOT CHANGE TO OTHER VALUE THAN 0 (used for enums begins)*/

/**
 * \enum classes_e
 * Enumeration of classes
 */
enum classes_e {
   C_OTHER = 0, /**<Undefined class*/
   C_ARITH, /**<Instruction performs arithmetic computations*/
   C_CONV, /**<Instruction performs conversions*/
   C_CRYPTO, /**<Instruction performs cryptographic computations*/
   C_CONTROL, /**<Instruction performs control operations*/
   C_LOGIC, /**<Instruction performs logic computations*/
   C_MATH, /**<Instruction performs mathematics computations*/
   C_MOVE, /**<Instruction performs memory operations*/
   C_APP_SPECIFIC /**<Instruction performs application specifics operations*/
};

/**
 * \enum fam_flags_e
 * Enumeration of flags
 */
enum fam_flags_e {
   FL_NONE = 0, /**<Instruction has no family flag*/
   FL_COND, /**<Instruction always operates under conditions*/
   FL_MASK /**<Instruction operates under a mask*/
};

/**
 * Enumeration of families, classes and flags
 */
#define FM_UNDEF 0x0000
#define FM_ABS 0x1001
#define FM_ADD 0x1002
#define FM_AND 0x5003
#define FM_ATAN 0x6004
#define FM_AVG 0x1005
#define FM_BROADCAST 0x7006
#define FM_BSWAP 0x7007
#define FM_CALL 0x4008
#define FM_CLR 0x0009
#define FM_CMP 0x100A
#define FM_CVT 0x200B
#define FM_COS 0x600C
#define FM_CNT 0x500D
#define FM_CRYPTO 0x800E
#define FM_DEC 0x100F
#define FM_DIV 0x1010
#define FM_DSP 0x8011
#define FM_ENV 0x4012
#define FM_EXP 0x6013
#define FM_FMA 0x1014
#define FM_FMS 0x1015
#define FM_GATHER 0x7016
#define FM_HASH 0x8017
#define FM_HW 0x4018
#define FM_IN 0x7019
#define FM_INC 0x101A
#define FM_INFO 0x001B
#define FM_JUMP 0x401C
#define FM_CJUMP 0x411C
#define FM_LEA 0x101D
#define FM_LOAD 0x701E
#define FM_LOCK 0x001F
#define FM_LOG 0x6020
#define FM_LOOP 0x4021
#define FM_MASK 0x0022
#define FM_MAX 0x1023
#define FM_MERGE 0x7024
#define FM_MIN 0x1025
#define FM_MOV 0x7026
#define FM_CMOV 0x7126
#define FM_MMOV 0x7226
#define FM_MUL 0x1027
#define FM_NEG 0x5028
#define FM_NOP 0x0029
#define FM_NOT 0x502A
#define FM_OR 0x502B
#define FM_OS 0x402C
#define FM_OTHER_APP_SPECIFIC 0x802D
#define FM_OTHER_ARITH 0x102E
#define FM_OTHER_CONTROL 0x402F
#define FM_OTHER_LOGIC 0x5030
#define FM_OTHER_MATH 0x6031
#define FM_OUT 0x7032
#define FM_PACK 0x7033
#define FM_POP 0x7034
#define FM_PREFETCH 0x0035
#define FM_PUSH 0x7036
#define FM_RAND 0x8037
#define FM_RCP 0x1038
#define FM_REM 0x1039
#define FM_RET 0x403A
#define FM_ROT 0x503B
#define FM_RND 0x103C
#define FM_RSQRT 0x603D
#define FM_SCAN 0x503E
#define FM_SCATTER 0x703F
#define FM_SET 0x7040
#define FM_SHIFT 0x5041
#define FM_SHUFFLE 0x7042
#define FM_SIN 0x6043
#define FM_SQRT 0x6044
#define FM_STACK_INIT 0x4045
#define FM_STACK_RELEASE 0x4046
#define FM_STACK_ROT 0x4047
#define FM_STORE 0x7048
#define FM_SUB 0x1049
#define FM_SYNC 0x404A
#define FM_TAN 0x604B
#define FM_TM 0x404C
#define FM_UNPACK 0x704D
#define FM_VIRT 0x404E
#define FM_XCHG 0x704F
#define FM_XOR 0x5050

/**
 * Simd flag
 */
#define S_NO            0
#define S_YES           1

/**
 * \def T_MASK
 * Mask used to access the type in ::insn_s::elt_in and ::insn_s::elt_out fields
 */
#define T_MASK          0xF0
/**
 * \enum elt_types_e
 * Enumeration of element's types
 */
enum elt_types_e {
   T_UNDEF = 0, /**<Undefined elements' type*/
   T_BIT, /**<Elements are bits*/
   T_INT, /**<Elements are integers*/
   T_UINT, /**<Elements are unsigned integers*/
   T_SINT, /**<Elements are signed integers*/
   T_FP, /**<Elements are floating-points*/
   T_POL, /**<Elements are 'polynomials'*/
   T_BCD, /**<Elements are Binary Coded Decimal*/
   T_STR, /**<Elements are strings*/
};

/**
 * \def T_MAKS
 * Mask used to access the size in ::insn_s::elt_in and ::insn_s::elt_out fields
 */
#define SZ_MASK         0x0F
/**
 * \enum elt_sizes_e
 * Enumeration of element's sizes
 */
enum elt_sizes_e {
   SZ_UNDEF = 0, /**<Undefined elements' size*/
   SZ_8, /**<Elements are 8 bits long*/
   SZ_16, /**<Elements are 16 bits long*/
   SZ_32, /**<Elements are 32 bits long*/
   SZ_64, /**<Elements are 64 bits long*/
   SZ_80, /**<Elements are 80 bits long*/
   SZ_128, /**<Elements are 128 bits long*/
   SZ_256, /**<Elements are 256 bits long*/
   SZ_512 /**<Elements are 512 bits long*/
};
//@const_stop

/**
 * Enumeration of semantic flags. These flags are cumulative (an instruction
 * can be a branch and have a bitstop).
 */
#define A_NA           0x00000000    /**<No flag*/
// Flags initialized during the disassembling process
#define A_JUMP         0x00000001    /**<Branch*/
#define A_CALL         0x00000002    /**<CALL*/
#define A_RTRN         0x00000004    /**<RET*/
#define A_CONDITIONAL  0x00000008    /**<If a branch, is conditional*/
//#define A_SCTTXT       0x00000010    /**<instruction in .text section*/
//#define A_SCTINI       0x00000020    /**<instruction in .init section*/
//#define A_SCTFIN       0x00000040    /**<instruction in .fini section*/
//#define A_SCTPLT       0x00000080    /**<instruction in .plt section*/
#define A_STDCODE      0x00000010    /**<Instruction belongs to a standard code section*/
#define A_EXTFCT       0x00000020    /**<Instruction is a part of a stub for invoking an external function*/
#define A_PATCHED      0x00000040    /**<Instruction belongs to a patched section*/

#define A_SUSPICIOUS   0x00000100    /**<Instruction may be bad (bad instructions have been found around it)*/
#define A_UNREACHABLE  0x00000200    /**<Instruction is not reachable with direct branches*/
#define A_SETFLAGS     0x00000400    /**<Instruction sets up the comparison flags*/
//Flags initialised either during disassembly or during flow analysis
/**\todo TODO (2014-07-15) Those two annotates will be used to detect gaps in a list of instructions - for instance, if
 * there are data inside). We may not need both of them after all, so check this once refactoring is complete*/
#define A_BEGIN_LIST   0x00000800    /**<Instruction is the first in a list of consecutive instructions (the bytes preceding it are not instructions)*/
#define A_END_LIST     0x00008000    /**<Instruction is the last in a list of consecutive instructions (the bytes following it are not instructions)*/
// Flag initialised during flow analysis
#define A_BEGIN_PROC   0x00001000    /**<first instruction of a function*/
#define A_END_PROC     0x00002000    /**<last instruction of a function*/
#define A_BEGIN_BLOCK  0x00004000    /**<first instruction of a block*/
// Flags initialized by "asmfile_detect_end_of_functions"
#define A_POTENTIAL_EX 0x00010000    /**<Instruction is a potential exit (indirect jump)*/
#define A_NATURAL_EX   0x00020000    /**<Instruction is a natural exit (RET)*/
#define A_HANDLER_EX   0x00040000    /**<Instruction call an exit handler (CALL)*/
#define A_EARLY_EX     0x00080000    /**<Instruction is an early exit (left the function with a JUMP)*/
// Flags initialized by the indirect branch solver (during flow analysis)
#define A_IBSOLVE      0x00100000    /**<instruction is a solved indirect branch*/
#define A_IBNOTSOLVE   0x00200000    /**<instruction is an unsolved indirect branch*/
// Flags used by the patcher during a patching session
#define A_PATCHMOV     0x01000000    /**<Instruction has been moved from its original
                                        location during a patching operation. Can also
                                        be set by the disassembler to flag an instruction
                                        in a block displaced by the patcher*/
#define A_PATCHNEW     0x02000000    /**<Instruction has been added to the file during
                                        a patching operation*/
/**\todo TODO (2014-12-17) I think A_PATCHNEW is redundant and could be removed. If this is confirmed, use its code for A_PATCHMOD instead
 * => (2015-05-11) Now I plan on using it, and conversely I don't think I need A_PATCHMOD (was merged with A_PATCHUPD)*/
#define A_PATCHDEL     0x04000000    /**<Instruction has been deleted from the file
                                        during a patching operation*/
#define A_PATCHUPD     0x08000000    /**<Instruction has been updated because of a
                                        patching operation*/
//#define A_PATCHMOD     0x10000000   /**<Instruction has been modified by a patching operation*/
#define A_EX ( A_POTENTIAL_EX | A_NATURAL_EX |                          \
               A_HANDLER_EX   | A_EARLY_EX ) /**<Instruction is an exit*/

#define A_JCOND ( A_JUMP | A_CONDITIONAL ) /**<Concatenated flag for representing a
                                              conditional jump. Mainly intended for
                                              setting flags (use of this for testing
                                              flags has to be adapted to the fact
                                              that it is a multiple flag instead of
                                              a single one)*/
/**\todo A_JCOND is needed by the addition of default annotations in arch_t, and the
 fact that it was used in the x86_64 binary description file. To be get rid of.*/
#define A_JRET ( A_JUMP | A_RTRN ) /**<Concatenated flag for representing a return
                                      instruction. Mainly intended for setting flags
                                      (use of this for testing flags has to be adapted
                                      to the fact that it is a multiple flag instead
                                      of a single one)*/
/**\todo Like A_JCOND, A_JRET is needed by the addition of default annotations in arch_t,
 and the fact that it was used in the x86_64 binary description file. To be get rid of.*/

#define LABEL_PATCHMOV  "@_patchmov_@"  /**<Label present at the beginning of a section
                                           containing moved code after a patching operation*/
/**\todo This is used to allow the libcore to process patched file, but another solution should be
 * implemented if possible, as this causes some interconnection between the MAQAO components*/
#define LABEL_NEW_BLOCK "._maqao_new_block"  /**<Label added by some tools indicating to the libcore
                                                that a new basic block must be created, even if the
                                                instruction is in the middle of a "real" basic block*/

/**
 * Flags used to determine which analysis have been done on an asmfile. They are cumulatives
 */
#define NO_ANALYZE       0x000       /**<No analyze has been done*/
#define PAR_ANALYZE      0x001       /**<The file has been parsed*/
#define DIS_ANALYZE      0x002       /**<The file has been disassembled*/
#define CFG_ANALYZE      0x004       /**<CFG and CG have been analysed*/
#define DOM_ANALYZE      0x008       /**<Domination has been analysed*/
#define LOO_ANALYZE      0x010       /**<Loops has been analysed*/
#define COM_ANALYZE      0x020       /**<Connected components has been analyzed*/
#define EXT_ANALYZE      0x040       /**<Functions have been extracted from connected components*/
#define PDO_ANALYZE      0x080       /**<Post Dominance has been analysed*/

/**
 * Flags used to determine the role of an operand. They are cumulative
 */
#define OP_ROLE_UNDEF   0x00       /**<No defined role. Default state of an operand*/
#define OP_ROLE_SRC     0x01       /**<Operand is a source*/
#define OP_ROLE_DST     0x02       /**<Operand is a destination*/

/**
 * Instruction size suffixes to use when printing
 * */
#define DATASZ_UNDEF_SUF '\0'    /**<No suffix*/
#define DATASZ_8b_SUF    'B'     /**<Byte suffix*/
#define DATASZ_16b_SUF   'W'     /**<Word suffix*/
#define DATASZ_32b_SUF   'L'     /**<Long suffix*/
#define DATASZ_64b_SUF   'Q'     /**<Quarter suffix*/
#define DATASZ_128b_SUF  'X'     /**<Xmm suffix*/
#define DATASZ_256b_SUF  'Y'     /**<Ymm suffix*/
#define DATASZ_512b_SUF  'Z'     /**<Zmm suffix*/

/**
 * Flags used by grouping analysis
 */
#define GRP_LOAD     (int)'L'    /**<Represent a load*/
#define GRP_STORE    (int)'S'    /**<Represent a store*/

//@const_start
/**
 * Languages supported by the demangling library. These macros can represent
 * several DW_LANG_ macros
 */
#define LANG_ERR        0     /**<Unknown language*/
#define LANG_C          1     /**<C language*/
#define LANG_CPP        2     /**<C++ language*/
#define LANG_FORTRAN    3     /**<Fortran language*/
//@const_stop

/**
 * Strings correponding to DEM_LANG macros
 */
#define DEM_LANG_STR_C        "c"       /**<C language*/
#define DEM_LANG_STR_CPP      "c++"     /**<C++ language*/
#define DEM_LANG_STR_FORTRAN  "fortran" /**<Fortran language*/

//@const_start
/**
 * Compilers supported by the demangling library. A macro is a compiler family
 * (for example, all Intel compilers (icc, icpc, ifort ...) have the same family)
 */
#define COMP_ERR        0     /**<Unknown compiler*/
#define COMP_GNU        1     /**<GNU compilers (gcc, gfortran, g++ ...)*/
#define COMP_INTEL      2     /**<Intel compilers (icc, icpc, ifort ...)*/
//@const_stop

/**
 * Strings correponding to DEM_COMP macros
 */
#define DEM_COMP_STR_GNU        "GNU compiler"      /**<GNU compilers (gcc, gfortran, g++ ...)*/
#define DEM_COMP_STR_INTEL      "Intel compiler"    /**<Intel compilers (icc, icpc, ifort ...)*/

/**
 * Debug format supported by MAQAO
 */
typedef enum dbgformat_e {
   DBG_NONE = 0, /**<File has no debug information*/
   DBG_FORMAT_UNDEF, /**<Debug format undefined*/
   DBG_FORMAT_DWARF, /**<Debug format DWARF*/
   DBG_FORMAT_MAX /**<Number of DBG_FORMAT_ value*/
} dbgformat_t;

/**
 * Values for group s_status field
 */
#define SS_NA  0     /**<Not Analyzed*/
#define SS_OK  1     /**<Successfully analyzed*/
#define SS_MB  2     /**<Loop Multi-block*/
#define SS_VV  3     /**<Iterator not constant*/
#define SS_O   4     /**<Other*/
#define SS_RIP 5     /**<RIP based address*/

/**
 * Message corresponding to group s_status field
 */
#define SS_MSG_NA  "Not Analyzed"           /**<Not Analyzed*/
#define SS_MSG_OK  "Success"                /**<Successfully analyzed*/
#define SS_MSG_MB  "Not mono block loop"    /**<Loop Multi-block*/
#define SS_MSG_VV  "Iteration not constant" /**<Iterator not constant*/
#define SS_MSG_O   "Not found"              /**<Other (not found)*/
#define SS_MSG_RIP "RIP based value"        /**<RIP based address*/

/**
 * Values for group m_status field
 */
#define MS_NA   0                           /**<Not analyze / error*/
#define MS_OK   1                           /**<Success*/

/**
 * Values for group m_status field
 */
#define MS_MSG_NA  "Not Analyzed"           /**<Not Analyzed*/
#define MS_MSG_OK  "Success"                /**<Successfully analyzed*/

//@const_start
/**
 * Suffix of external labels
 * \todo Use a non ELF-specific name
 */
#define EXT_LBL_SUF "@plt"

/**
 * Comma separated list of function names that do not return the control flow after their call
 * These functions have to be handled as a RET for the purpose of building the graph
 * Names should not include a suffix for external labels
 * \warning Make sure to add there only unambiguous names!
 * */
#define DEFAULT_EXIT_FUNCTIONS_NAMES "exit", "__assert_fail", "abort"

/**
 * Values for project cc_mode member
 * Define how functions are extracted from connected components (CCs)
 */
#define CCMODE_OFF   0                      /**<Do not extract functions from CCs*/
#define CCMODE_DEBUG 1                      /**<Extract only functions whose get debug data*/
#define CCMODE_ALL   2                      /**<Extract all functions from CCs*/
//@const_stop

/**
 * Identifier of the format of a binary file
 * \warning When updating this list, also update the bf_format_names array in la_binfile.c
 * */
typedef enum bf_format_e {
   BFF_UNKNOWN = 0, /**<Unsupported or unidentified format*/
   BFF_ELF, /**<ELF file format*/
   BFF_WINPE, /**<Windows PE file format*/
   BFF_MACHO, /**<Mach-O file format*/
   BFF_MAX /**<Maximum number of supported formats*/
} bf_format_t;

/**
 * Identifier of the word size of a binary file
 * */
typedef enum bfwordsz_e {
   BFS_UNKNOWN = 0, /**<Unsupported or unknown size*/
   BFS_32BITS, /**<32 bits*/
   BFS_64BITS, /**<64 bits*/
   BGS_MAX /**<Maximum number of word size*/
} bfwordsz_t;

/**
 * Identifier of the type of a binary file
 * \warning Do not exceed 16 types or change the size of \c type in \ref binfile_t
 * */
typedef enum bf_type_e {
   BFT_UNKNOWN = 0, /**<Unsupported or unidentified format*/
   BFT_EXECUTABLE, /**<Executable file*/
   BFT_LIBRARY, /**<Dynamic library*/
   BFT_RELOCATABLE, /**<Relocatable file intended for a linker*/
   BFT_ARCHIVE, /**<Archive of other binary files*/
   BFT_MAX /**<Maximum number of types*/
} bf_type_t;

/**
 * Identifier of the patching state of a binary file
 * \warning Do not exceed 16 types or change the size of \c patch in \ref binfile_t
 * \warning The BFP_PATCHED identifier must always be located after the last identifier
 * signaling a file in the process of being patched (set during a patching session)
 * and the first identifier signaling a patched file (set by the disassembler upon recognising it)
 * */
enum bf_patch {
   BFP_NONE = 0, /**<File is not patched*/
   BFP_PATCHING, /**<File is in the process of being patched*/
   BFP_FINALISED, /**<File is in the process of being patched and has been finalised*/
   BFP_REORDERED, /**<File is in the process of being patched and has been reordered*/
   BFP_PATCHED, /**<Identifiers greater than this one signal that the file has been patched*/
   BFP_MADRAS, /**<File has been patched by MADRAS*/
   BFP_DYNINST, /**<File has been patched by DynInst*/
   BFP_OTHER, /**<File has been patched by another patching tool*/
   BFP_MAX /**<Maximum number of patching types*/
};

/**
 * Identifier of the byte order of a binary file
 */
enum bf_byte_order {
   BFO_HOST = 0, /**<File has the same byte order as the host*/
   BFO_REVERSED, /**<File has a reversed byte order*/
   BFO_MAX
};

/**
 * Identifier of the type of a section in a binary file
 * */
typedef enum scntype_e {
   SCNT_UNKNOWN = 0, /**<Type of section is unknown or format-specific*/
   SCNT_CODE, /**<Section contains executable code. May contain entries if it contains data interleaved with code*/
   SCNT_DATA, /**<Section contains data. Will contain entries if parsed using labels or instruction references.*/
   SCNT_REFS, /**<Section contains references to other sections. Expected to contain an array of pointer data*/
   /**\todo Maybe another name for REFS (2014-04-18)*/
   SCNT_RELOC, /**<Section is a relocation section. Expected to contain an array of data containing binrel_t pointers*/
   SCNT_LABEL, /**<Section contains labels. Expected to contain an array of data containing label_t pointers*/
   SCNT_STRING, /**<Section contains strings only. Expected to contain an array of strings.*/
   SCNT_DEBUG, /**<Section contains debug data. Will be parsed by the appropriate driver*/
   SCNT_ZERODATA, /**<Section contains uninitialised data. Will contain entries if parsed using labels or instruction references*/
   SCNT_HEADER, /**<Section represents a header from the file that is represented as a section*/
   SCNT_PATCHCOPY, /**<Section has been duplicated during a patching operation but is identical to its original*/
   SCNT_PATCHSTATIC, /**<Section has been added from a static library during a patching operation. Its \c binfile member points to the file it was copied from*/
   SCNT_MAX /**<Maximum number of section types*/
} scntype_t;

/*Masks for the attributes of a section in a binary file*/
/**\todo TODO (2015-02-17) I'm using the same masks for the attributes of segments. Depending on what we do with segments,
 * we may have to define separate flags for those for better clarity
 * => (2015-04-14) WARNING! I've extended the attributes for the section on 16 bits, but not in the segments (as I should only need
 * the first ones anyway). Keep in mind and possibly update the segments if we need to use all the flags for them as well*/
#define SCNA_NONE    0x0000  /**<Empty mask*/
#define SCNA_READ    0x0001  /**<Mask signaling a binary section is accessible for reading*/
#define SCNA_WRITE   0x0002  /**<Mask signaling a binary section is accessible for writing*/
#define SCNA_EXE     0x0004  /**<Mask signaling a binary section is accessible for execution*/
#define SCNA_LOADED  0x0008  /**<Mask signaling a binary section is loaded into memory during execution*/
#define SCNA_TLS     0x0010  /**<Mask signaling a binary section used for thread local storage (loaded with specific handling)*/
/**\todo TODO (2014-11-28) One way to avoid the annotation on instructions for specifying which belong to sections used for analysis
 * would be to set the attributes heres (SCNA_INIT, SCNA_FINI, SCNA_TEXT, SCNA_DYN instead of A_SCTINI, A_SCTFIN, A_SCTTXT and A_SCTPLT)
 * and access the section from the instruction to check whether they are to be analysed or not. But it may increase the size of the attributes
 * Or merge it with scn_type on an uint16_t, using for instance 10 bits for attributes and 6 for type
 * =>(2015-02-11) Doing this now with the flags below, to be seen whether it works. For clarity we may store those flags in a separate field
 * =>(2015-05-29) No we don't store them in separate fields anymore*/
//The following masks are deduced from the properties of the section and are not present in the actual binary file
#define SCNA_STDCODE       0x0020  /**<The section is a standard code section for the format and can be analysed by default*/
#define SCNA_INSREF        0x0040  /**<The section contains entries that are referenced by instructions in the file*/
#define SCNA_PATCHREORDER  0x0080  /**<The section has been reordered during a patching operation and its address has possibly been updated*/
#define SCNA_LOCALDATA     0x0100   /**<The data contained in the section has been allocated locally and must be freed when the section is freed*/
#define SCNA_PATCHED       0x0200   /**<The section has been added by a patch operation*/
#define SCNA_EXTFCTSTUBS   0x0400   /**<The section contains stubs for external functions*/
/**\todo TODO (2015-04-09) Use these defines in la_binfile.c to offer a unified front for retrieving sections
 * based on their ids and get the headers and symbol table as well.*/
#define BF_SEGHDR_ID (UINT16_MAX-1) /**<Identifier of the section used to store the segment header*/
#define BF_SCNHDR_ID (UINT16_MAX-2) /**<Identifier of the section used to store the section header*/
#define BF_SYMTBL_ID (UINT16_MAX-3) /**<Identifier of the section used to store the symbol table*/
#define BF_LAST_ID   (UINT16_MAX-4) /**<Must always be smaller than the smallest special identifier*/

#define BF_SCNID_ERROR  UINT16_MAX  /**<Identifier of an erroneous section identifier*/
#define BF_ENTID_ERROR  UINT32_MAX  /**<Identifier of an erroneous entry index in a section*/

/**
 * Type representing an address in an assembly file
 * */
typedef int64_t maddr_t;

/**
 * Alias for struct interval_s structure
 * */
typedef struct interval_s interval_t;

/**
 * Alias for struct binfile_s structure
 * */
typedef struct binfile_s binfile_t;

/**
 * Alias for struct binscn_s structure
 * */
typedef struct binscn_s binscn_t;

/**
 * Alias for struct binseg_s structure
 * */
typedef struct binseg_s binseg_t;

/**
 * Alias for struct binrel_s structure
 * */
typedef struct binrel_s binrel_t;

/**
 * Alias for struct bf_driver_s structure
 * */
typedef struct bf_driver_s bf_driver_t;

/**
 * Alias for struct block_s structure
 */
typedef struct block_s block_t;

/**
 * Alias for struct loop_s structure
 */
typedef struct loop_s loop_t;

/**
 * Alias for struct fct_s structure
 */
typedef struct fct_s fct_t;

/**
 * Alias for struct asmfile_s structure
 */
typedef struct asmfile_s asmfile_t;

/**
 * Alias for struct insn_s structure
 */
typedef struct insn_s insn_t;

/**
 * Alias for struct pointer_s structure
 */
typedef struct pointer_s pointer_t;

/**
 * Alias for struct memory_s structure
 */
typedef struct memory_s memory_t;

/**
 * Alias for struct memrel_s structure
 */
typedef struct memrel_s memrel_t;

/**
 * Alias for struct oprnd_s structure
 */
typedef struct oprnd_s oprnd_t;

/**
 * Alias for struct label_s structure
 */
typedef struct label_s label_t;

/**
 * Alias for struct data_s structure
 */
typedef struct data_s data_t;

/**
 * Alias for struct insn_dbg_s structure
 */
typedef struct insn_dbg_s insn_dbg_t;

/**
 * Alias for struct project_s structure
 */
typedef struct project_s project_t;

/**
 * Alias for struct reg_s structure
 */
typedef struct reg_s reg_t;

/**
 * Alias for struct regidx_s structure
 */
typedef struct regidx_s regidx_t;

/**
 * Alias for struct arch_s structure
 */
typedef struct arch_s arch_t;

/**
 * Alias for struct abi_s structure
 */
typedef struct abi_s abi_t;

/**
 * Alias for struct arch_specs_s structure
 */
typedef struct arch_specs_s arch_specs_t;

/**
 * Alias for struct uarch_s structure
 */
typedef struct uarch_s uarch_t;

/**
 * Alias for struct proc_s structure
 */
typedef struct proc_s proc_t;

/**
 * Alias for struct group_elem_s structure
 */
typedef struct group_elem_s group_elem_t;

/**
 * Alias for struct group_s structure
 */
typedef struct group_s group_t;

/**
 * Alias for struct fct_dbg_s structure
 */
typedef struct fct_dbg_s fct_dbg_t;

/**
 * Alias for struct fct_range_s structure
 */
typedef struct fct_range_s fct_range_t;

/**
 * Alias for struct dbg_insn_s structure
 */
typedef struct dbg_insn_s dbg_insn_t;

/**
 * Alias for struct dbg_fct_s structure
 */
typedef struct dbg_fct_s dbg_fct_t;

/**
 * Alias for struct dbg_file_s structure
 */
typedef struct dbg_file_s dbg_file_t;

//@const_start
/**
 * \enum data_sizes
 * Code for the different types of data sizes
 * The values in the enum must appear in the same relative order as the order
 * of the sizes they represent
 */
typedef enum data_size_e {
   DATASZ_UNDEF = 0, /**<Size is not defined*/
   DATASZ_1b, /**<Size is 1 bit*/
   DATASZ_2b, /**<Size is 2 bits*/
   DATASZ_3b, /**<Size is 3 bits*/
   DATASZ_4b, /**<Size is 4 bits*/
   DATASZ_5b, /**<Size is 5 bits*/
   DATASZ_6b, /**<Size is 6 bits*/
   DATASZ_7b, /**<Size is 7 bits*/
   DATASZ_8b, /**<Size is 8 bits (1 Byte)*/
   DATASZ_9b, /**<Size is 9 bits*/
   DATASZ_10b, /**<Size is 10 bits*/
   DATASZ_11b, /**<Size is 11 bits*/
   DATASZ_12b, /**<Size is 12 bits*/
   DATASZ_13b, /**<Size is 13 bits*/
   DATASZ_14b, /**<Size is 14 bits*/
   DATASZ_16b, /**<Size is 16 bits (2 Bytes)*/
   DATASZ_20b, /**<Size is 20 bits*/
   DATASZ_21b, /**<Size is 21 bits*/
   DATASZ_23b, /**<Size is 23 bits*/
   DATASZ_24b, /**<Size is 24 bits (3 Bytes)*/
   DATASZ_25b, /**<Size is 25 bits*/
   DATASZ_26b, /**<Size is 26 bits*/
   DATASZ_28b, /**<Size is 28 bits*/
   DATASZ_32b, /**<Size is 32 bits (4 Bytes)*/
   DATASZ_64b, /**<Size is 64 bits (8 Bytes)*/
   DATASZ_80b, /**<Size is 80 bits (10 Bytes)*/
   DATASZ_112b, /**<Size is 112 bits (14 Bytes)*/
   DATASZ_128b, /**<Size is 128 bits (16 Bytes)*/
   DATASZ_224b, /**<Size is 224 bits (28 Bytes)*/
   DATASZ_256b, /**<Size is 256 bits (32 Bytes)*/
   DATASZ_512b, /**<Size is 512 bits (64 Bytes)*/
   DATASZ_672b, /**<Size is 672 bits (94 Bytes)*/
   DATASZ_864b, /**<Size is 864 bits (108 Bytes)*/
   DATASZ_4096b, /**<Size is 4096 bits (512 Bytes)*/
   DATASZ_MAX /**<Maximal representable size*/
} data_size_t;

/**
 * \enum memalign_e
 * Enumeration of flags representing the alignment value of a memory operand
 */
typedef enum memalign_e {
   M_NO_ALIGN = 0,
   M_ALIGN_16,
   M_ALIGN_32,
   M_ALIGN_64,
   M_ALIGN_128,
   M_ALIGN_256
} memalign_t;

#define M_POSITIVE_SIGN 0x0
#define M_NEGATIVE_SIGN 0x1

/**
 * \enum oprnd_type_e
 * Codes indicating the type of a oprnd
 */
typedef enum oprnd_type_e {
   OT_REGISTER = 0, /**<oprnd is a register*/
   OT_MEMORY, /**<oprnd is a memory address*/
   OT_IMMEDIATE, /**<oprnd is an immediate value*/
   OT_POINTER, /**<oprnd is a pointer to another instruction*/
   OT_MEMORY_RELATIVE, /**<oprnd is a memory address referencing an address in the file*/
   OT_IMMEDIATE_ADDRESS, /**<oprnd is an immediate value representing an address*/
   OT_REGISTER_INDEXED, /**<oprnd is an element inside a register*/
   OT_UNKNOWN /**<Max number of operand types, also used to indicate an erroneous operand type*/
} oprnd_type_t;

#define OT_NAME_REGISTER            "register"           /**<Name of the operand type register*/
#define OT_NAME_REGISTER_INDEXED    "indexed register"   /**<Name of the operand type indexed register*/
#define OT_NAME_MEMORY              "memory"             /**<Name of the operand type memory*/
#define OT_NAME_MEMORY_RELATIVE     "relative memory"    /**<Name of the operand type relative memory*/
#define OT_NAME_IMMEDIATE           "immediate"          /**<Name of the operand type immediate*/
#define OT_NAME_IMMEDIATE_ADDRESS   "immediate address"  /**<Name of the operand type immediate address*/
#define OT_NAME_POINTER             "pointer"            /**<Name of the operand type pointer*/
//@const_stop

/**
 * \enum targettype_e
 * Codes indicating the type of object targeted by a label or pointer structure
 * \warning The number of elements in this list must not exceed 16
 */
typedef enum target_type_e {
   TARGET_UNDEF = 0, /**<The object type is undefined*/
   TARGET_INSN, /**<The object is an instruction (insn_t structure)*/
   TARGET_DATA, /**<The object is a data variable (data_t structure)*/
   TARGET_BSCN, /**<The object is a binary section (binscn_t structure)*/
   TARGET_MAX /**<Maximum number of target types (must always be last)*/
} target_type_t;

/**
 * \enum label_type
 * Codes indicating the type of a label. To ease comparisons between labels, they are ordered by decreasing importance.
 * \warning Changing the order of elements in this list impacts the behaviour of label_cmp_qsort and stream_parse in libmdisass
 * In particular, LBL_NOFUNCTION should be the first type of label that can't be used to for the \c fctlbl member in an \ref insn_t structure
 * \warning The number of elements in this list must not exceed 16
 */
typedef enum label_type_e {
   LBL_FUNCTION = 0, /**<Label identifies a function (it is expected its target will be of type INSN)*/
   LBL_EXTFUNCTION, /**<Label identifies a stub for invoking an external function*/
   LBL_GENERIC, /**<Label has no special type and does not identify a function*/
   LBL_PATCHSCN, /**<Label marks the beginning of the patched section*/
   LBL_NOFUNCTION, /**<Label can not be used to identify a function*/
   LBL_VARIABLE, /**<Label identifies a variable (data structure)*/
   LBL_EXTERNAL, /**<Label is defined in an external file (for object files)*/
   LBL_NOVARIABLE, /**<Label can not identify a variable*/
   LBL_DUMMY, /**<Label is a dummy label added by the patcher*/
   LBL_OTHER, /**<Label type is undefined*/
   LBL_ERROR /**<Maximum number of label types (must always be last), also used to indicate an erroneous label type */
} label_type_t;

/**
 * \enum pointer_type
 * Codes indicating the type of a pointer
 * \warning The number of elements in this list must not exceed 16
 */
typedef enum pointer_type_e {
   POINTER_UNKNOWN = 0, /**<The pointer type is undefined*/
   POINTER_ABSOLUTE, /**<The pointer is an absolute address*/
   POINTER_RELATIVE, /**<The pointer is a relative address from the current instruction*/
//   POINTER_RELOCATED,   /**<The pointer actual value is stored in a relocation table*/
   POINTER_NOADDRESS, /**<The pointer is used to link to an element, but its address or offset is not used*/
   POINTER_MAXTYPES /**<Maximum number of pointer types*/
} pointer_type_t;

/**
 * \eneum range_type
 * Codes indicating the type of a range
 */
enum range_type {
   RANGE_ORIGINAL = 0, /**< The range belongs to the original function*/
   RANGE_INLINED /**< The range has been inlined*/
};

//@const_start

/**
 * \enum grouping_format_type
 * Defines available formats when groups are displayed
 */
enum grouping_format_type {
   GROUPING_FORMAT_LINE,   // One group per line, default mode
   GROUPING_FORMAT_CSV,    // Based on LINE format but respecting CSV format
   GROUPING_FORMAT_TEXT,   // Readable format
   GROUPING_FORMAT_CM,     // Special
   GROUPING_FORMAT_PCR     // Special
};

/**
 * \enum params_modules_id_e
 * Defines available values for module parameters
 */
enum params_modules_id_e {
   PARAM_MODULE_DEBUG = 0, // Options about debug data handling
   PARAM_MODULE_DISASS,    // Options about disassembly
   PARAM_MODULE_LCORE,
   PARAM_MODULE_BINARY,    // Options about handling binary files
   _NB_PARAM_MODULE        // Keep this element at the end
};

// Defines the maximal number of parameters for a module
#define _NB_OPT_BY_MODULE   64

/**
 * \enum params_DEBUG_id_e
 * Defines available values for DEBUG module parameters
 */
enum params_DEBUG_id_e {
   PARAM_DEBUG_DISABLE_DEBUG = 0, // Select if debug data must be loaded or not (boolean)
   PARAM_DEBUG_ENABLE_VARS, // Select if debug vars must be loaded or not (boolean)
   _NB_PARAM_DEBUG                  // Keep this element at the end
};

/**
 * \enum params_DISASS_id_e
 * Defines available values for DISASS module parameters
 */
enum params_DISASS_id_e {
   PARAM_DISASS_OPTIONS = 0, // Mask of options for disassembly. Use values from DISASS_OPTIONS_* defines
   _NB_PARAM_DISASS           // Keep this element at the end
};

/**
 * \enum params_LCORE_id_e
 * Defines available values for libcore module parameters
 */
enum params_LCORE_id_e {
   PARAM_LCORE_FLOW_ANALYZE_ALL_SCNS, //Select if all executable sections must be analyzed during flow analysis
   _NB_PARAM_LCORE                  // Keep this element at the end
};

/**
 * \enum params_BINARY_id_e
 * Defines available values for BINARY module parameters
 */
enum params_BINARY_id_e {
   PARAM_BINPRINT_OPTIONS = 0, // Mask of options for printing a binary file. Use values from BINPRINT_OPTIONS_* defines
   _NB_PARAM_BINARY              // Keep this element at the end
};

/*Options to use as masks in disassembly*/
#define DISASS_OPTIONS_FULLDISASS   0x00   // Default option, corresponds to a full disassembly
#define DISASS_OPTIONS_NODISASS     0x01   // Option for disabling disassembly (only binary parsing will be done)
#define DISASS_OPTIONS_NODATAPARSE  0x02   // Option for disabling parsing of data sections
/**\todo TODO (2015-03-09) BUG! Some of those names are also defined in libmdisass.h, and with different values. This leads to the
 * disassembly to ignore some options. Choose one header where to define those and stick to it
 * => (2015-05-29) Has been fixed*/

//Options to use as masks for printing the contents of a parsed binary file
#define BINPRINT_OPTIONS_NOPRINT 0x00  // Does not print anything (defined mainly for coherence)
#define BINPRINT_OPTIONS_HDR     0x01  // Prints the header of the file
#define BINPRINT_OPTIONS_SCNHDR  0x02  // Prints the sections header
#define BINPRINT_OPTIONS_SEGHDR  0x04  // Prints the segments header, if the binary format support segments
#define BINPRINT_OPTIONS_DYN     0x08  // Prints the dynamic informations for the file
#define BINPRINT_OPTIONS_REL     0x10  // Prints the relocation informations for the file
#define BINPRINT_OPTIONS_SYM     0x20  // Prints the symbols from the file
#define BINPRINT_OPTIONS_VER     0x40  // Prints version information for the file
#define BINPRINT_OPTIONS_ALL     0xff  // Prints all informations for the file (if the number of options increases above 8, update accordingly to always be 0xfff...)

/**
 * Defines identifiers for all the fields potentially associated with an assembly instruction
 * */
typedef enum insn_fields_e {
   INSNF_FULL_ASSEMBLY = 0, /**<Full assembly representation of the instruction*/
   INSNF_ADDRESS, /**<Address of the instruction*/
   INSNF_ANNOTATE, /**<Annotate of the instruction*/
   INSNF_ARCH, /**<Architecture of the instruction*/
   INSNF_BLOCK_ID, /**<Block id of the instruction*/
   INSNF_CODING, /**<Binary coding of the instruction*/
   INSNF_DBG_SRCFILE, /**<Source file of the instruction*/
   INSNF_DBG_SRCLINE, /**<Source line of the instruction*/
   INSNF_ELT_IN, /**<Size of input elements of the instruction*/
   INSNF_ELT_OUT, /**<Size of output elements of the instruction*/
   INSNF_FCTLBL, /**<Label of the instruction*/
   INSNF_LOOP_ID, /**<Loop identifier of the instruction*/
   INSNF_NB_OPRND, /**<Number of operands of the instruction*/
   INSNF_OPCODE, /**<Opcode of the instruction*/
   INSNF_MAX /**<Maximum number of fields, must always be last*/
} insn_fields_t;

/**
 * Defines identifiers for all the fields potentially associated with an assembly label
 * */
typedef enum label_fields_e {
   LBLF_NAME = 0, /**<Name of the label*/
   LBLF_ADDRESS, /**<Address of the label*/
   LBLF_MAX /**<Maximum number of fields, must always be last*/
} label_fields_t;

/**
 * Defines identifiers for all the fields potentially associated with an assembly block
 * */
typedef enum block_fields_e {
   BLOCKF_GLOBAL_ID = 0, /**<Block global id*/
   BLOCKF_ID, /**<Block id*/
   BLOCKF_FIRST_INSN_ADDR, /**<Address of the first instruction in the block*/
   BLOCKF_LAST_INSN_ADDR, /**<Address of the last instruction in the block*/
   BLOCKF_FUNCTION_NAME, /**<Name of the function to which the block belongs*/
   BLOCKF_IS_LOOP_EXIT, /**<Specifies whether the block is an exit for the loop to which it belongs*/
   BLOCKF_IS_PADDING, /**<Specifies whether the block is a padding block*/
   BLOCKF_LOOP_ID, /**<Identifier of the loop to which the block belongs*/
   BLOCKF_MAX /**<Maximum number of fields, must always be last*/
} block_fields_t;

/**
 * Defines identifiers for all the fields potentially associated with an assembly loop
 * */
typedef enum loop_fields_e {
   LOOPF_GLOBAL_ID = 0, /**<Loop global id*/
   LOOPF_ID, /**<Loop id*/
   LOOPF_FUNCTION_NAME, /**<Name of the function to which the loop belongs*/
   LOOPF_BLOCKS, /**<List of blocks presents in the loop*/
   LOOPF_MAX /**<Maximum number of fields, must always be last*/
} loop_fields_t;

/**
 * Defines identifiers for all the fields potentially associated with an assembly function
 * */
typedef enum fct_fields_e {
   FCTF_GLOBAL_ID = 0, /**<Function global id*/
   FCTF_ID, /**<Function id*/
   FCTF_FIRST_INSN_ADDR, /**<Address of the first instruction in the function*/
   FCTF_LAST_INSN_ADDR, /**<Address of the last instruction in the function*/
   FCTF_NAME, /**<Name of the function*/
   FCTF_MAX /**<Maximum number of fields, must always be last*/
} fct_fields_t;

/**
 * Defines identifiers for all the fields potentially associated with an architecture
 * */
typedef enum arch_fields_e {
   ARCHF_NAME = 0, /**<Name of the architecture*/
   ARCHF_MAX /**<Maximum number of fields, must always be last*/
} arch_fields_t;

/**
 * Defines identifiers for all the fields potentially associated with a formatted text file
 * */
typedef enum file_fields_e {
   TXTFILEF_BLOCKID_SCOPE = 0,/**<Specifies whether the scope over which block identifiers are unique (file, function, loop)*/
   TXTFILEF_MAX /**<Maximum number of fields, must always be last*/
} file_fields_t;

//@const_stop

#define DISASS_OPTIONS_PARSEONLY  (DISASS_OPTIONS_NODISASS | DISASS_OPTIONS_NODATAPARSE) //Option for parsing only the binary file
///////////////////////////////////////////////////////////////////////////////
//                                   abi                                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * \struct abi_s
 * \brief This structure contains abi-specific data, such as the registers used to pass function parameters
 * or return values
 */
struct abi_s {
   reg_t** arg_regs; /**<Array which contains all registers which can be used as
    an argument for a function*/
   reg_t** return_regs; /**<Array which contains all registers which can be used to
    return a value at the end of a function*/
   int nb_arg_regs; /**<Number of registers which can be used as
    an argument for a function*/
   int nb_return_regs; /**<Number of registers which can be used to return a value at
    the end of a function*/
};
/**\todo Remove those fields from arch_t and only use abi_t for using them*/
///////////////////////////////////////////////////////////////////////////////
//                 micro-architecture and processor version                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Micro-architectures and processor variants must be defined in the <arch>/<arch>_uarch.[ch] files, with <arch> being the identifier
 * of the architecture to which they belong.
 *
 * Below are the guidelines for updating these files:
 *
 * - Adding a new processor variant (belonging to an existing micro-architecture, e.g. <arch>_uarch_foo)
 * 1) In <arch>_uarch.h: Add the corresponding identifier in the <arch>_procs_e enum, e.g. <arch>_PROC_ID42, after the last identifier
 * of a processor variant belonging to the same micro-architecture
 * 2) In <arch>_uarch.c: Find the block dedicated to the declaration of the micro-architecture and update as follows:
 *   2.1) If none of the previously declared processor variant supported the same list of instruction sets as the new processor,
 *   add a new array for the instruction sets supported by this variant.
 *   2.2) Declare the structure defining the results of the CPUID command identifying this processor variant
 *   E.g: static <arch>_cpuid_code_t id42 = 0x42;
 *   2.3) Declare the structure representing the processor variant.
 *   E.g: proc_t <arch>_proc_id42 = {&<arch>_uarch_foo, "Full name of the variant", "Unique abbreviated name of the variant",
 *                                    &id42, <arch>_isets_foo42, ARRAY_NB_ELTS(<arch>_isets_foo42), <arch>_PROC_ID42};
 *        In the above example, <arch>_isets_foo42 is either the list of instruction sets defined in 2.1, or an existing one
 *   \warning The fields in the proc_t structure may change in the future, update as necessary
 *   2.4) Add a pointer to the newly declared proc_t structure in the list of processors variants supported by the micro-architecture
 *   (e.g. <arch>_procs_foo)
 *   2.5) Update the array declaring the processors for the architecture (<arch>_procs) to add a pointer to the new proc_t structure
 *   \warning IMPORTANT! The position of the processor in the list must correspond to the index of its identifier in the enum.
 *   Also, a NULL entry must be added in the #else case corresponding to the #ifndef of UARCH_EXCLUDE_<ARCH>_FOO
 *   2.6) If necessary, update the architecture-specific functions to take into account the new processor variant
 *
 * - Adding a new micro-architecture (with a first processor variant)
 * 1) In <arch>_uarch.h: Add the corresponding identifier in the <arch>_uarchs_e enum, e.g. <arch>_UARCH_FOO, before <arch>_UARCH_MAX
 * 2) In <arch>_uarch.c: Add a new block for declaring a micro architecture:
 *    2.1) The declaration must be enclosed between #ifndef UARCH_EXCLUDE_<ARCH>_FOO and #endif directives
 *    2.2) First, declare of the micro-arch structure:
 *      e.g: uarch_t <arch>_uarch_foo;
 *    2.3) Define the structures representing the processor variant (see above):
 *      - array of instruction sets supported by the processor variant
 *      - structure representing the results of the CPUID command for identifying it
 *      - structure representing the processor variant
 *    2.4) Declare the array containing the processor variants associated to this micro-architecture
 *      e.g: proc_t *<arch>_procs_foo[] = { &<arch>_proc_id42 };
 *    2.5) Declare the structure representing the micro-architecture.
 *      e.g:  uarch_t <arch>_uarch_foo = {&<arch>_arch, "Full name of the micro-arch", "Unique abbreviated name of the micro-arch",
 *                               "Unique alias of the micro-arch", <arch>_procs_foo, ARRAY_NB_ELTS(<arch>_procs_foo), <arch>_UARCH_FOO};
 *    2.6) Update the array declaring the processors for the architecture (<arch>_procs) to add a new block, structured as follows:
 *      #ifndef UARCH_EXCLUDE_<ARCH>_FOO
 *      ,&<arch>_proc_id42
 *      #else
 *      ,NULL
 *      #endif
 *    \warning IMPORTANT! As above, the position of the processor variant in the list must correspond to the index of its identifier in the enum
 *    2.7) Update the array declaring the uarchs for the architecture (<arch>_uarchs) to add a new block, structured as follows:
 *      #ifndef UARCH_EXCLUDE_<ARCH>_FOO
 *       ,&<arch>_uarch_foo
 *      #else
 *       ,NULL
 *      #endif
 *    \warning IMPORTANT! The position of the micro-arch in the list must correspond to the index of its identifier in the enum.
 *    2.8) If necessary, update the architecture-specific functions to take into account the new micro-arch
 * */


/**
 * \struct arch_specs_s
 * \brief Structure regrouping specificities/specifics about a given architecture
 * */
struct arch_specs_s {
   uarch_t** uarchs;                                              /**< Array of micro-architectures available for this architecture*/
   proc_t** procs;                                                /**< Array of processors available for this architecture*/
   proc_t* (*uarch_get_default_proc) (uarch_t*);                  /**< Returns the default processor to consider for a given micro-architecture*/
   uint16_t nb_uarchs;                                            /**< Number of micro-architectures available for this architecture*/
   uint16_t nb_procs;                                             /**< Number of processors available for this architecture*/
};

/**
 * \struct uarch_s
 * \brief A structure representing a micro-architecture (variant of an architecture)
 */
struct uarch_s {
   arch_t* arch;        /**<Architecture for which this micro-architecture is defined*/
   char* display_name;  /**<Name of this micro-architecture to use for display*/
   char* name;          /**<Simplified name of this micro-architecture*/
   char* alias;        /**<Alternate name of the micro-architecture*/
   proc_t** procs;      /**<Array of the processors belonging to this micro-architecture*/
   uint16_t nb_procs;   /**<Number of processors belonging to this micro-architecture (size of \c procs)*/
   uint16_t uarch_id;   /**<Identifier of the micro-architecture (unique for the uarchs of a given architecture)*/
};

/**
 * \struct proc_s
 * \brief A structure representing a processor variant
 * */
struct proc_s {
   uarch_t* uarch;      /**<Micro-architecture to which this processor belongs*/
   char* display_name;  /**<Name of this processor to use for display*/
   char* name;          /**<Name of this processor*/
   void* cpuid_code;    /**<Constructor code of the processor (as returned by the cpuid instruction or equivalent)*/
   uint8_t* isets;      /**<Array of the instruction sets identifiers available for this processor*/
   uint16_t nb_isets;   /**<Number of instruction sets available for this processor (size of \c isets)*/
   uint16_t proc_id;    /**<Identifier of the processor (unique for a given architecture)*/
};

/**
 * Returns the architecture for which a micro-architecture is defined
 * \param uarch The micro-architecture
 * \param The associated architecture or NULL if \c uarch is NULL
 * */
extern arch_t* uarch_get_arch(uarch_t* uarch);

/**
 * Returns the display name of a micro-architecture
 * \param uarch The micro-architecture
 * \param The display name of the micro-architecture or NULL if \c uarch is NULL
 * */
extern char* uarch_get_display_name(uarch_t* uarch);

/**
 * Returns the name of a micro-architecture
 * \param uarch The micro-architecture
 * \param The name of the micro-architecture or NULL if \c uarch is NULL
 * */
extern char* uarch_get_name(uarch_t* uarch);

/**
 * Returns the alias of a micro-architecture
 * \param uarch The micro-architecture
 * \param The alias of the micro-architecture or NULL if \c uarch is NULL
 * */
extern char* uarch_get_alias(uarch_t* uarch);

/**
 * Returns the array of processors for a micro-architecture
 * \param uarch The micro-architecture
 * \param The processors associated to this micro-architecture or NULL if \c uarch is NULL
 * */
extern proc_t** uarch_get_procs(uarch_t* uarch);

/**
 * Returns the number of processors for a micro-architecture
 * \param uarch The micro-architecture
 * \param The number of processors associated to this micro-architecture or 0 if \c uarch is NULL
 * */
extern uint16_t uarch_get_nb_procs(uarch_t* uarch);

/**
 * Returns the identifier of a micro-architecture
 * \param uarch The micro-architecture
 * \param The identifier of the micro-architecture or 0 if \c uarch is NULL
 * */
extern uint16_t uarch_get_id(uarch_t* uarch);

/**
 * Returns the default processor to consider for a given micro-architecture
 * \param uarch The micro-architecture
 * \return The default processor version to consider for this architecture
 * */
extern proc_t* uarch_get_default_proc(uarch_t* uarch);

/**
 * Returns a list of instruction sets supported by at least one processor variant of a given micro-architecture.
 * \param uarch The micro-architecture
 * \param nb_isets Return parameter. Will be updated with the size of the returned array
 * \return An array containing the instruction sets supported by at least one processor in the micro-architecture,
 * or NULL if \c uarch or \c nb_isets is NULL. This array must be freed using lc_free
 * */
extern uint8_t* uarch_get_isets(uarch_t* uarch, uint16_t* nb_isets);

/**
 * Returns the micro-architecture for which a processor version is defined
 * \param proc The processor version
 * \param The associated micro-architecture or NULL if \c proc is NULL
 * */
extern uarch_t* proc_get_uarch(proc_t* proc);

/**
 * Returns the name of a processor version
 * \param proc The processor version
 * \param The name of the processor version or NULL if \c proc is NULL
 * */
extern char* proc_get_name(proc_t* proc);

/**
 * Returns the display name of a processor version
 * \param proc The processor version
 * \param The display name of the processor version or NULL if \c proc is NULL
 * */
extern char* proc_get_display_name(proc_t* proc);

/**
 * Returns the information provided by the processor to identify its version
 * \param proc The processor version
 * \param The information provided by the processor to identify its version or NULL if \c proc is NULL
 * */
extern void* proc_get_cpuid_code(proc_t* proc);

/**
 * Returns the array of instruction set identifiers supported by this processor version
 * \param proc The processor version
 * \return Array of instruction set identifiers or NULL if \c proc is NULL
 * */
extern uint8_t* proc_get_isets(proc_t* proc);

/**
 * Returns the number of instruction set identifiers supported by this processor version
 * \param proc The processor version
 * \return Number of instruction set identifiers or 0 if \c proc is NULL
 * */
extern uint16_t proc_get_nb_isets(proc_t* proc);

/**
 * Returns the identifier of a processor
 * \param uarch The processor
 * \param The identifier of the processor 0 if \c proc is NULL
 * */
extern uint16_t proc_get_id(proc_t* proc);

/**
 * Low-level function for returning a list of micro-architectures containing a given instruction set
 * This function is intended to be invoked from the architecture-specific functions
 * \param iset The code of the instruction set
 * \param nb_uarchs Return parameter, will contain the number of micro-architectures found
 * \param arch_uarchs Array of micro-architectures to inspect
 * \param nb_arch_uarchs Size of the array of micro-architectures to inspect
 * \return A list of micro-architectures containing the given instruction set, or NULL if nb_uarchs or arch_uarchs is NULL or if
 * no micro-architecture in the array to inspect contains this instruction set
 * */
extern uarch_t* uarch_get_uarchs_from_isets(int16_t iset, unsigned int* nb_uarchs, uarch_t** arch_uarchs, unsigned int nb_arch_uarchs);

///////////////////////////////////////////////////////////////////////////////
//                                          arch                                    //
///////////////////////////////////////////////////////////////////////////////
#define R_NONE -1       /**<Code used if a file has no value*/

/*Definition of function types used in the arch_t*/
typedef void (*insn_free_fct_t)(void* in); /**<Function used to free an instruction*/
typedef insn_t* (*insn_parse_fct_t)(char* strinsn); /**<Function used to parse an instruction*/
typedef void (*insn_print_fct_t)(insn_t* insn, char* c, size_t size); /**<Function used to print an instruction*/
typedef void (*insn_fprint_fct_t)(insn_t* insn, FILE* f); /**<Function used to print an instruction to a file stream*/
typedef oprnd_t* (*oprnd_parse_fct_t)(char* stroprnd, int* pos); /**<Function used to parse an operand*/
typedef void (*oprnd_updptr_fct_t)(insn_t* insn, pointer_t* ptr); /**<Function used to update a pointer based on its target or its address based on its offset*/
typedef reg_t** (*get_implicit_fct_t)(arch_t* arch, int opcode, int* nb_reg);
typedef boolean_t (*insn_equal_fct_t)(insn_t* i1, insn_t* i2); /**<Function used to test if two instructions are equal*/
typedef boolean_t (*oprnd_equal_fct_t)(oprnd_t* o1, oprnd_t* o2); /**<Function used to test if two operands are equal*/
typedef oprnd_t* (*oprnd_copy_fct_t)(oprnd_t*);  /**Function used to copy an operand*/

/**
 * \struct arch_s
 * \brief This structure contains architecture-specific data, such as opcode names, registers names ...
 */
struct arch_s {
   char* name; /**<Architecture name*/
   char** opcodes; /**<Array of opcode names*/
   unsigned short* families; /**<Array of families, mapped on opcode array*/
   unsigned int* variants_simd; /**<Array indexed on the instruction variant id containing the corresponding simd flag*/
   int size_opcodes; /**<Size of the array of opcode names*/
   uint32_t nb_insnvariants; /**<Number of instruction variants*/
   int16_t* variants_opcodes; /**<Array indexed on the instruction variant id containing the corresponding opcode*/
   uint8_t* variants_nboprnd; /**<Array indexed on the instruction variant id containing the corresponding number of operands*/
   uint16_t** variants_oprndtmpl; /**<Array indexed on the instruction variant id of the corresponding identifiers of the templates of operand for this variant
    The second dimension of the array is the number of operands for this particular variant (in variants_nboprnd)*/
   uint8_t* variants_isets; /**<Array indexed on the instruction variant id containing the corresponding instruction set identifier*/
   oprnd_t** oprndtmpls; /**<Array of the different operand templates, indexed on the operand template identifier*/
   uint16_t nb_oprndtmpls; /**<Number of different templates (size of \c oprndtmpls)*/
   int size_pref_suff; /**<Size of the array of possible prefixes and suffixes*/

   char** pref_suff; /**<Array of possible instruction prefixes and suffixes. Suffixes appear last in the array*/
   char*** reg_names; /**<Array of register names. If a register type has
    less possible names than the others, its line in the array 
    will be filled up by NULL pointers.*/
   reg_t*** regs; /**<Array of register instances. If a register type has less
    possible names than the others, its line in the array will
    be filled up by NULL pointers.*/
   char* reg_sizes; /**<Array of different register sizes (defined in an enum) of
    size \e nb_type_registers*/
   char* reg_families; /**<Array of different register families (defined in an enum)
    of size \e nb_type_registers*/
   unsigned char* noprnd_min; /**<Array of minimal number of operands for an instruction, 
    mapped on opcode array */
   unsigned char* noprnd_max; /**<Array of maximal number of operands for an instruction, 
    mapped on opcode array */
   unsigned int* dflt_anno; /**<Array of default annotate flag for an instruction, mapped
    on opcode array*/
   int nb_type_registers; /**<Number of different register types*/
   int nb_names_registers; /**<Maximum number of register names per types*/
   void* ext; /**<Generic pointer used to reference an application-specific
    sub-structure*/
   /**\todo Move those values to abi_t and update other files accordingly*/
   reg_t** arg_regs; /**<Array which contains all registers which can be used as
    an argument for a function*/
   reg_t** return_regs; /**<Array which contains all registers which can be used to
    return a value at the end of a function*/
   int nb_arg_regs; /**<Number of registers which can be used as
    an argument for a function*/
   int nb_return_regs; /**<Number of registers which can be used to return a value at
    the end of a function*/
   char** iset_names; /**<Array containing the names of instruction sets.*/
   unsigned int nb_isets; /**<Number of instruction sets in this architecture*/
   reg_t* reg_rip; /**<Register used to save the instruction pointer*/
   char* reg_rip_name; /**<Name of the register used to save the instruction pointer*/
   arch_specs_t *arch_specs;   /**<Specifications of the architectures (micro-architectures and processor versions)*/
   insn_free_fct_t insn_free; /**<Function used to free an instruction*/
   insn_parse_fct_t insn_parse; /**<Function used to parse an instruction*/
   insn_print_fct_t insn_print; /**<Function used to print an instruction*/
   insn_fprint_fct_t insn_fprint; /**<Function used to print an instruction to a file stream*/
   oprnd_parse_fct_t oprnd_parse; /**<Function used to parse an operand*/
   oprnd_updptr_fct_t oprnd_updptr; /**<Function used to update a pointer based on its target or its address based on its offset*/
   get_implicit_fct_t get_implicite_src;
   get_implicit_fct_t get_implicite_dst;
   insn_equal_fct_t insn_equal; /**<Function used to test if two instructions are equal*/
   oprnd_equal_fct_t oprnd_equal; /**<Function used to test if two operands are equal*/
   oprnd_copy_fct_t oprnd_copy; /**<Function used to copy an operand*/
   uint8_t first_suff_idx; /**<Index of the first element representing a suffix in the \c pref_suff array (the previous elements represent prefixes)*/
   char code; /**<Identifier for this architecture (values from arch_code_t)*/
   char endianness; /**<Specifies the 'endianness' of the instructions*/
};

/**
 * Get the usual name of a register represented by its codes
 * \param arch an architecture
 * \param reg_type a register type code (defined in arch_t structure)
 * \param reg_name a register name code (defined in arch_t structure)
 * \return a string which is the usual register name
 */
extern char* arch_get_reg_name(arch_t* arch, char reg_type, char reg_name);

/**
 * Gets the name of the register used to represent the instruction pointer
 * \param arch an architecture
 * \return The name of the register representing the instruction pointer for this architecture, R_RIP if no such
 * name was defined for this architecture, and NULL if arch is NULL
 * */
extern char* arch_get_reg_rip_name(arch_t* arch);

/**
 * Get an opcode name
 * \param arch an architecture
 * \param opcode an opcode code
 * \return a string with the opcode name
 */
extern char* arch_get_opcode_name(arch_t* arch, short opcode);

/**
 * Get a prefix or suffix name
 * \param arch an architecture
 * \param prefix_or_suffix a prefix/suffix code
 * \return a string with the name
 */
extern char* arch_get_prefsuff_name(arch_t* arch, short prefix_or_suffix);

/**
 * Gets the simd flag of a specified instruction variant
 * \param a an architecture
 * \param iv an instruction variant
 * \return the simd flag associated to the instruction variant, or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short arch_insnvariant_is_simd(arch_t* a, uint32_t iv);

/**
 * Gets instruction family related to an instruction opcode
 * \param arch an architecture
 * \param opcode an opcode code
 * \return the family associated to the opcode, or FM_UNDEF if there is a problem
 */
extern unsigned short arch_get_family(arch_t* arch, short opcode);

/**
 * Gets class related to an instruction opcode
 * \param arch an architecture
 * \param opcode an opcode code
 * \return the class associated to the opcode, or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short arch_get_class(arch_t* arch, short opcode);

/**
 * Gets the endianness of an architecture
 * \param arch an architecture
 * \return The endianness of an architecture
 */
extern unsigned int arch_get_endianness(arch_t* arch);

/**
 * Gets an architecture's name
 * \param arch an architecture
 * \return the name in string form of the architecture, or PTR_ERROR if there is a problem
 */
extern char* arch_get_name(arch_t* arch);

/**
 * Gets a register family
 * \param arch an architecture
 * \param reg_type a register type code
 * \return The identifier of the register family (those are defined in the appropriate
 * \<archname\>_arch.h header)
 * or SIGNED_ERROR if the register type is not valid (or RIP_TYPE) or if \c arch is NULL
 * */
extern char arch_get_reg_family(arch_t* arch, short reg_type);

/**
 * Gets a register type's size
 * \param arch an architecture
 * \param reg_type a register type code
 * \return The size of registers of this type or SIGNED_ERROR if the register type is
 * not valid (or RIP_TYPE) or if \c arch is NULL
 * */
extern char arch_get_reg_size(arch_t* arch, short reg_type);

/**
 * Retrieves the code associated to an architecture
 * \param arch an architecture
 * \return The code associated to this architecture as defined in arch.h, or ARCH_NONE if arch is NULL
 * */
extern char arch_get_code(arch_t* arch);

/**
 * Retrieves the number of instruction sets for this architecture
 * \param arch an architecture
 * \return The number of instruction sets, or 0 if \c arch is NULL
 * */
extern unsigned int arch_get_nb_isets(arch_t* arch);

/**
 * Retrieves the name of an instruction set
 * \param arch an architecture
 * \param iset The code representing an instruction set for this architecture
 * \return The name of the given instruction set, or NULL if \c arch is NULL or \c iset is out of the range of valid instruction sets codes
 * */
extern char* arch_get_iset_name(arch_t* arch, unsigned int iset);

/**
 * Retrieves the function used to free an instruction
 * \param arch an architecture
 * \return pointer to the function
 */
extern insn_free_fct_t arch_get_insn_free(arch_t* arch);

/**
 * Returns the array of micro-architectures associated to this architecture
 * @param a The architecture
 * @return Array of associated micro-architectures or NULL if \c a is NULL
 */
extern uarch_t** arch_get_uarchs(arch_t* a);

/**
 * Returns the number of micro-architectures associated to this architecture
 * @param a The architecture
 * @return Number of associated micro-architectures or 0 if \c a is NULL
 */
extern uint16_t arch_get_nb_uarchs(arch_t* a);

/**
 * Returns the array of processor versions associated to this architecture
 * @param a The architecture
 * @return Array of associated processor versions or NULL if \c a is NULL
 */
extern proc_t** arch_get_procs(arch_t* a);

/**
 * Returns the number of processor versions associated to this architecture
 * @param a The architecture
 * @return Number of associated processor versions or NULL if \c a is NULL
 */
extern uint16_t arch_get_nb_procs(arch_t* a);

/**
 * Returns a micro-architecture with the given code
 * \param arch The architecture
 * \param uarch_id The id of the micro-architecture
 * \return The micro-architecture or NULL if arch is NULL or uarch_code is not a valid micro-architecture identifier for this architecture
 * */
extern uarch_t* arch_get_uarch_by_id(arch_t* arch, uint16_t uarch_id);

/**
 * Returns a micro-architecture with the given name
 * \param arch The architecture
 * \param uarch_name The name of the micro-architecture
 * \return The micro-architecture or NULL if arch or is NULL or uarch_name is NULL or not a valid micro-architecture name for this architecture
 * */
extern uarch_t* arch_get_uarch_by_name(arch_t* arch, char* uarch_name);

/**
 * Returns a processor version with the given code
 * \param arch The architecture
 * \param proc_id The id of the processor version
 * \return The processor version or NULL if arch is NULL or proc_code is not a valid processor identifier for this architecture
 * */
extern proc_t* arch_get_proc_by_id(arch_t* arch, uint16_t proc_id);

/**
 * Returns a processor version with the given name
 * \param arch The architecture
 * \param proc_name The code of the processor version
 * \return The processor version or NULL if arch is NULL or proc_name is NULL or not a valid processor name for this architecture
 * */
extern proc_t* arch_get_proc_by_name(arch_t* arch, char* proc_name);

/**
 * Returns the default processor to consider for a given micro-architecture
 * \param arch The architecture
 * \param uarch The micro-architecture
 * \return The default processor version to consider for this architecture
 * */
extern proc_t* arch_get_uarch_default_proc(arch_t* arch, uarch_t* uarch);

/**
 * Returns an array of processors containing a given instruction set
 * \param arch The architecture
 * \param iset The instruction set
 * \param nb_procs Return parameter, will contain the size of the returned array
 * \return An array of structures describing a processor version, or NULL if \c arch or nb_procs is NULL or if the no
 * processor in this architecture contains the given instruction set. This array must be freed using lc_free
 * */
extern proc_t** arch_get_procs_from_iset(arch_t* arch, int16_t iset, uint16_t* nb_procs);

/**
 * Returns an array of micro-architectures containing at least one processor version containing a given instruction set
 * \param arch The architecture
 * \param iset The instruction set
 * \param nb_uarchs Return parameter, will contain the size of the returned array
 * \return An array of structures describing a micro-architecture, or NULL if \c arch or nb_uarchs is NULL or if the no
 * processor in this architecture contains the given instruction set
 * */
extern uarch_t** arch_get_uarchs_from_iset(arch_t* arch, int16_t iset, uint16_t* nb_uarchs);

///////////////////////////////////////////////////////////////////////////////
//                                        register                           //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct reg_s
 * \brief A structure representing a register
 */
struct reg_s {
   char type; /**<Type of a register (defined in arch_t)*/
   char name; /**<Name of a register (defined in arch_t)*/
};

/**
 * Create a new register from its codes
 * \param reg_name register name code (defined in arch_t structure)
 * \param reg_type register type code (defined in arch_t structure)
 * \param arch architecture the register belongs to
 * \return a new register
 */
extern reg_t* reg_new(int reg_name, int reg_type, arch_t* arch);

/**
 * Parses a string to find a register name belonging to the given architecture
 * \param insn_str The string to parse
 * \param pos Pointer to the index value in the string at which to start the search.
 * Will be updated to point to the character immediately after the register name
 * \param arch The architecture against which the register must be parsed
 * \return Pointer to the register whose name is in the string, or NULL if no register was parsed
 * */
extern reg_t* reg_parsenew(char* insn_str, int *pos, arch_t* arch);

/**
 * Gets a register family
 * \param reg a register
 * \param arch an architecture
 * \return The identifier of the register family (those are defined in the appropriate <archname>_arch.h header)
 * or SIGNED_ERROR if the register type is not valid (or RIP_TYPE) or if \c arch is NULL
 */
extern char reg_get_family(reg_t* reg, arch_t* arch);

/**
 * Gets register type code
 * \param reg a register
 * \return the register type code
 */
extern char reg_get_type(reg_t* reg);

/**
 * Gets register name code
 * \param reg a register
 * \return the register name code
 */
extern char reg_get_name(reg_t* reg);

/**
 * Delete an existing register
 * \param p a pointer on a register
 */
extern void reg_free(void* p);

///////////////////////////////////////////////////////////////////////////////
//                                 indexed register                          //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct regidx_s
 * \brief A structure representing a register
 */
struct regidx_s {
   reg_t* reg; /**<Register (defined in arch_t)*/
   uint8_t idx; /**<Index of the register*/
};

/**
 * Creates a new structure for storing an indexed register
 * \param reg a register
 * \param idx The index of the register
 * \return A new regidx_t structure
 * */
extern regidx_t* regidx_new(reg_t* reg, uint8_t idx);

/**
 * Frees a structure storing an indexed register
 * \param regidx A regidx_t structure
 * */
extern void regidx_free(regidx_t* regidx);

///////////////////////////////////////////////////////////////////////////////
//                                      pointer                              //
///////////////////////////////////////////////////////////////////////////////
// Used only in pointer_s
typedef int64_t pointer_offset_t;
typedef uint16_t pointer_offset_intarget_t;

/**
 * \struct pointer_s
 * \brief Pointer
 * This structure stores a pointer referencing an object at a given address.
 * It can be used as an operand for an assembly instruction (used for branches or call instruction)
 * or to reference a data structure
 * \todo TODO (2014-12-04) Attempt to reduce the memory footprint of this structure by using unions. We could also
 * reduce the offset on 32 bits.
 */
struct pointer_s {
   pointer_offset_t offset; /**<Offset between the instruction address and the destination (relative pointer)*/
   maddr_t addr; /**<Address referenced by the pointer (absolute pointer)*/
   void* relative_origin; /**<Element whose address must be used as base to compute the destination address from the offset
    if it is not the data or insn containing this pointer*/
   union {
      insn_t* insn; /**<Instruction present at the pointed address*/
      data_t* data; /**<Data present at the pointed address*/
      binscn_t* bscn;/**<Binary section present at the pointed address*/
   } target; /**<Element pointed to by the pointer*/
   pointer_offset_intarget_t offset_intarget; /**<Offset in bytes inside the target object of the actual destination of the pointer*/
   unsigned type :4; /**<Type of the pointer value (using the constants defined in \ref pointertype_t*/
   unsigned target_type :4; /**<Type of the element to which the pointer points (using the constants defined in \ref targettype_t)*/
   unsigned origin_type :4; /**<Type of the element whose address must be used for computing the destination from the offset for relative pointers (constants defined in \ref targettype_t)*/
};

/**
 * Creates a new pointer
 * \param addr Address of the target object
 * \param offset Offset from the pointer to the target object (for relative pointers)
 * \param next Element targeted by the pointer (NULL if unknown)
 * \param pointer_type Type of the pointer value
 * \param target_type Type of the pointer target
 * \return A new initialized pointer
 */
extern pointer_t* pointer_new(maddr_t addr, pointer_offset_t offset, void* next,
      pointer_type_t pointer_type, target_type_t target_type);

/**
 * Frees a pointer
 * \param p Pointer to free
 * */
extern void pointer_free(void* p);

/**
 * Duplicates a pointer operand from an existing one. The object targeted by the copy
 * will be the same as the original
 * \param src Source pointer to duplicate
 * \return A newly allocated pointer_t structure targeting the same element, or NULL if src is NULL
 * */
extern pointer_t* pointer_copy(pointer_t* src);

/**
 * Gets the address referenced by a pointer
 * \param pointer A pointer
 * \return The address referenced by the pointer, or ADDRESS_ERROR if pointer is NULL
 * */
extern maddr_t pointer_get_addr(pointer_t* pointer);

/**
 * Gets the offset of a pointer (will be 0 for absolute pointers)
 * \param pointer A pointer
 * \return The offset to the address referenced by the pointer, or SIGNED_ERROR if pointer is NULL
 * */
extern pointer_offset_t pointer_get_offset(pointer_t* pointer);

/**
 * Gets the instruction pointed by the pointer
 * \param pointer A pointer
 * \return A pointer on the targeted instruction or PTR_ERROR if pointer is null or does not target an instruction
 */
extern insn_t* pointer_get_insn_target(pointer_t* pointer);

/**
 * Get the data pointed by the pointer
 * \param p A pointer
 * \return a pointer on the targeted data or PTR_ERROR if p is null or does not target a data element
 */
extern data_t* pointer_get_data_target(pointer_t* p);

/**
 * Get the binary section pointed by the pointer
 * \param p a pointer
 * \return a pointer on the targeted binary section or PTR_ERROR if p is null or does not target a binary section
 */
extern binscn_t* pointer_get_bscn_target(pointer_t* p);

/**
 * Gets the address of the object targeted to by the pointer
 * \param pointer A pointer
 * \return The address of the object targeted by the pointer, or SIGNED_ERROR if p or its target is NULL
 * */
extern maddr_t pointer_get_target_addr(pointer_t* pointer);

/**
 * Gets the type of a pointer
 * \param pointer a pointer
 * \return The pointer type or POINTER_UNKNOWN if pointer is null
 */
extern unsigned pointer_get_type(pointer_t* pointer);

/**
 * Gets the type of the target of a pointer
 * \param pointer a pointer
 * \return The pointer type or TARGET_UNDEF if pointer is null
 */
extern unsigned pointer_get_target_type(pointer_t* pointer);

/**
 * Gets the offset inside the target object of the actual destination of the pointer
 * \param pointer A pointer
 * \return The value of the pointer or UINT16_MAX if \c pointer is NULL
 * */
extern pointer_offset_intarget_t pointer_get_offset_in_target(pointer_t* pointer);

/**
 * Gets the element whose address must be used to compute the destination address of a relative pointer if different from the element
 * containing the pointer
 * \param pointer A pointer operand
 * \return The element to use to compute its destination address or NULL if \c pointer is NULL
 * */
extern void* pointer_get_relative_origin(pointer_t* pointer);

/**
 * Gets the type of the element whose address must be used to compute the destination address of a relative pointer if different from the element
 * containing the pointer
 * \param pointer A pointer operand
 * \return The type of the element to use to compute its destination address or TARGET_UNDEF if \c pointer is NULL
 * */
extern unsigned pointer_get_origin_type(pointer_t* pointer);

/**
 * Sets the address referenced by a pointer
 * \param pointer A pointer
 * \param addr The address referenced by the pointer
 * */
extern void pointer_set_addr(pointer_t* pointer, maddr_t addr);

/**
 * Sets the offset to the address referenced by a pointer
 * \param pointer A pointer
 * \param offset The offset to the address referenced by the pointer
 * */
extern void pointer_set_offset(pointer_t* pointer, pointer_offset_t offset);

/**
 * Sets the offset inside the target object of the actual destination of the pointer
 * \param pointer The pointer
 * \param offset_intarget The value of the offset from the beginning of the object representing its target target
 * */
extern void pointer_set_offset_in_target(pointer_t* pointer,
      pointer_offset_intarget_t offset_intarget);

/**
 * Updates the target of a pointer. New target is assumed to have the same type as the existing one
 * \param pointer The pointer
 * \param target The new target object
 * */
extern void pointer_upd_target(pointer_t* pointer, void* target);

/**
 * Updates the address of a pointer with regard to its target.
 * This function should not be used for pointers found in the operand of an instruction (\ref insn_oprnd_updptr should be used instead)
 * \param p The pointer
 * */
extern void pointer_upd_addr(pointer_t* pointer);

/**
 * Sets an instruction object as the target of a pointer.
 * \param pointer The pointer
 * \param target The destination instruction
 * */
extern void pointer_set_insn_target(pointer_t* pointer, insn_t* target);

/**
 * Sets a data object as the target of a pointer.
 * \param p The pointer
 * \param target The destination data
 * */
extern void pointer_set_data_target(pointer_t* p, data_t* target);

/**
 * Sets a section  object as the target of a pointer.
 * \param p The pointer
 * \param target The destination section
 * */
extern void pointer_set_bscn_target(pointer_t* p, binscn_t* target);

/**
 * Sets the origin of a relative pointer (object whose address must be used to compute the destination from the offset)
 * \param pointer The pointer. Must be of type POINTER_RELATIVE
 * \param origin Object whose address must be used for computing the destination from the origin for relative pointers
 * \param origin_type Type of the origin object
 * */
extern void pointer_set_relative_origin(pointer_t* pointer, void* origin,
      target_type_t origin_type);

/**
 * Sets the type of a pointer
 * \param pointer The pointer
 * \param type Type of the pointer value
 */
extern void pointer_set_type(pointer_t* pointer, pointer_type_t type);

/**
 * Checks if a pointer has been linked to a target
 * \param pointer The pointer
 * \return TRUE if the pointer has a non NULL target object, FALSE otherwise
 * */
extern int pointer_has_target(pointer_t* pointer);

/**
 * Prints a pointer. This is to be used for printing a pointer alone, for instance when it is in a data or relocation object
 * \param pointer The pointer
 * \param str The string to which to print
 * \param size The size of the string to which to print
 * */
extern void pointer_print(pointer_t* pointer, char* str, size_t size);

/**
 * Returns the value of a pointer as a string of bytes, encoded on a given length
 * \param pointer The pointer
 * \param len Length in bytes over which the value must be encoded
 * \return String of size len representing either the address targeted by the pointer if it is of type POINTER_ABSOLUTE
 * or its displacement if it is of type POINTER_RELATIVE, and NULL if \c pointer is NULL or if its value can not be encoded
 * over the given length. This string is NOT allocated.
 * */
extern unsigned char* pointer_tobytes(pointer_t* pointer, uint64_t len);

///////////////////////////////////////////////////////////////////////////////
//                                memory                                     //
///////////////////////////////////////////////////////////////////////////////
typedef int64_t memory_offset_t;

/**
 * \struct memory_s
 * \brief This structure stores an operand containing a memory address
 * \todo The displacement should not be stored as int64_t for
 * better support of multiple architectures
 */
struct memory_s {
   reg_t* seg; /**<Base segment of the memory address*/
   reg_t* base; /**<Register containing the base memory address*/
   reg_t* index; /**<Register containing the index value for the memory address*/
   memory_offset_t offset; /**<Displacement of the memory address (defined in arch_t structure)*/
   unsigned char scale; /**<Scale to the index of the memory address (defined in arch_t structure)*/
   unsigned sign :1; /**<1st lsb: Sign of the operation made when processing the index register (1=add)*/
   unsigned align :3; /**<Memory alignment (as defined in \ref memalign_e)*/
   unsigned bcflag :1; /**<Flag specifying whether the memory is being broadcast*/
};

/**
 * Creates a new empty structure holding a memory address
 * \return An empty memory address (correspond to a displacement of 0 with no base)
 */
extern memory_t* memory_new();

/**
 * Frees a memory address structure
 * \param mem memory address structure to free
 */
extern void memory_free(memory_t* mem);

/**
 * Returns the segment register in a memory operand
 * \param mem memory address in a memory operand
 * \return segment register or PTR_ERROR if \e mem is NULL
 */
extern reg_t* memory_get_seg(memory_t* mem);

/**
 * Returns the base register in a memory operand
 * \param mem memory address in a memory operand
 * \return base register or PTR_ERROR if \e mem is NULL
 */
extern reg_t* memory_get_base(memory_t* mem);

/**
 * Returns the index register in a memory operand
 * \param mem memory address in a memory operand
 * \return index register or PTR_ERROR if \e mem is NULL
 */
extern reg_t* memory_get_index(memory_t* mem);

/**
 * Returns offset (displacement) for a memory operand
 * \param mem memory address in a memory operand
 * \return Value of the offset or PTR_ERROR if \e mem is NULL
 */
extern memory_offset_t memory_get_offset(memory_t* mem);

/**
 * Returns scale (index scaling factor) for a memory operand
 * \param mem memory address in a memory operand
 * \return Value of the scale or PTR_ERROR if \e mem is NULL
 */
extern int memory_get_scale(memory_t* mem);

/**
 * Checks whether the memory is subject to a broadcast
 * \param mem memory address in a memory operand
 * \return TRUE if the memory is broadcast, FALSE otherwise
 */
extern uint8_t memory_is_broadcast(memory_t* mem);

/**
 * Returns the sign to apply on the index value
 * \param mem memory address in a memory operand
 */
extern uint8_t memory_get_sign(memory_t* mem);

/**
 * Returns the memory alignment
 * \param mem memory address in a memory operand
 */
extern uint8_t memory_get_align(memory_t* mem);

/**
 * Sets the value of sgement in a memory address
 * \param mem a memory address
 * \param seg Pointer to a structure describing a register to use as segment
 */
extern void memory_set_seg(memory_t* mem, reg_t* seg);

/**
 * Sets the base register in a memory address
 * \param mem memory address in a memory operand
 * \param base register to use as base
 */
extern void memory_set_base(memory_t* mem, reg_t* base);

/**
 * Sets the index register in a memory address
 * \param mem memory address in a memory operand
 * \param index register to use as index
 */
extern void memory_set_index(memory_t* mem, reg_t* index);

/**
 * Sets the value of offset in a memory address
 * \param mem memory address in a memory operand
 * \param offset Value of the offset
 */
extern void memory_set_offset(memory_t* mem, memory_offset_t offset);

/**
 * Sets the value of scale in a memory address
 * \param mem memory address in a memory operand
 * \param scale Value of the scale
 */
extern void memory_set_scale(memory_t* mem, int scale);

/**
 * Sets the sign to apply on the index value
 * \param mem memory address in a memory operand
 * \param sign sign to apply (0: positive)
 */
extern void memory_set_sign(memory_t* mem, uint8_t sign);

/**
 * Flags the memory as being broadcasted
 * \param mem memory address in a memory operand
 */
extern void memory_set_bcflag(memory_t* mem);

/**
 * Sets the memory alignment
 * \param mem memory address in a memory operand
 * \param align the memory alignment
 */
extern void memory_set_align(memory_t* mem, memalign_t align);

/**
 * Changes the register used as base in a memory operand
 * \param oprnd An operand
 * \param reg The new register used as base
 */
extern void oprnd_changebase(oprnd_t* oprnd, reg_t* reg);

/**
 * Changes the register used as index in a memory operand
 * \param oprnd An operand
 * \param reg The new register used as index
 */
extern void oprnd_changeindex(oprnd_t* oprnd, reg_t* reg);

///////////////////////////////////////////////////////////////////////////////
//                                memory pointer                             //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct memrel_s
 * \brief This structure stores an operand containing a memory address referencing a location in the file
 * Its base is the RIP register
 * \todo The displacement should not be stored as int64_t for better support of multiple architectures
 * \todo (2014-07-11) This structure currently contains a redundancy, as the offset is present in the memory_t and the
 * pointer_t structure (albeit in different forms). Additionally, the base register could be removed, as it is always the RIP
 * register. However I'm keeping it that way now because 1) This will make implementation easier by invoking existing functions
 * 2) A file should not contain a lot of these operands, so the impact on memory and time should not be too high
 * 4) Regarding the base register, we might end up encountering an architecture with different RIP registers.
 * Of course, if we have the time someday, we could get rid of the offset in the memory operand.
 */
struct memrel_s {
   memory_t* mem; /**<Memory address*/
   pointer_t* ptr; /**<Pointer representing the link to the memory address*/
};

/**
 * Creates a new empty structure holding a memory pointer
 * \param mem A pointer to the structure describing the memory
 * \param ptr A pointer to a structure describing the reference
 * \return An empty memory pointer (correspond to a displacement of 0)
 */
extern memrel_t* memrel_new(memory_t* mem, pointer_t* ptr);

/**
 * Frees a memory pointer structure
 * \param mem The memory pointer structure to free
 */
extern void memrel_free(memrel_t* mem);

/**
 * Updates the pointer of a relative memory
 * \param mpt The memory pointer structure
 * \param ptr The new pointer to set
 * */
extern void memrel_set_ptr(memrel_t* mpt, pointer_t* ptr);

///////////////////////////////////////////////////////////////////////////////
//                                 operand                                   //
///////////////////////////////////////////////////////////////////////////////
typedef int64_t imm_t;

/**
 * \struct oprnd_s
 * \brief This structure stores an generic operand for an assembly instruction
 */
struct oprnd_s {
   void* ext; /**<Extension*/
   union {
      reg_t* reg; /**<Name of the register if operand is a register*/
      memory_t* mem; /**<Memory operand if it is a memory addrress*/
      imm_t imm; /**<Immediate value if the operand is an immediate*/
      pointer_t *ptr; /**<Pointer operand if it is a pointer*/
      memrel_t* mpt; /**<Relative memory operand*/
      regidx_t* rix; /**<Indexed register*/
   } data; /**<This field contains the representation of the operand*/
   unsigned bitsize :6; /**<Size in bits of the operand (for registers and memory, represent
    the actual size of the register, for immediate and pointer, the size on which they are coded)*/
   unsigned type :3; /**<Type of the operand (Register, memory, pointer or immediate)*/
   unsigned role :2; /**<Role of the operand (source, dest or both)*/
   unsigned writeback :1; /**<Flag for memory base write back*/
   unsigned postindex :1; /**<Flag for memory post indexing*/
};

/**
 * Creates a memory operand from a memory address structure
 * \param mem A pointer to a structure describing memory address
 * \return A memory operand corresponding to the memory address
 * */
extern oprnd_t* oprnd_new_memory(memory_t* mem);

/**
 * Retrieves the type of a register operand
 * \param oprnd An operand
 * \return The type of the register, or -1 if \e oprnd is not a register operand
 * */
extern char oprnd_get_reg_type(oprnd_t* oprnd);

/**
 * Remove the index from an memory operand and returns the resulting register
 * \param oprnd The operand to update
 * \return The register that was used as index
 * */
extern reg_t* oprnd_rm_memory_index(oprnd_t* oprnd);

/**
 * Creates a new immediate oprnd
 * \param imm The value of the immediate oprnd
 * the oprnd
 * \return A oprnd of type immediate with the specified value
 */
extern oprnd_t* oprnd_new_imm(imm_t imm);

/**
 * Creates a new oprnd of type memory
 * \param seg The name of the base segment register for the memory address (can be NULL)
 * \param base The name of the register containing the base memory address (can be NULL)
 * \param index The name of the index register for the memory address (can be NULL)
 * \param scale The scale associated to the index (if present)
 * \param offset The value of displacement from the base for the memory address
 * \return A oprnd of type memory with the specified elements
 */
extern oprnd_t* oprnd_new_mem(reg_t* seg, reg_t* base, reg_t* index, int scale,
      memory_offset_t offset);

/**
 * Creates a new pointer operand
 * \param addr The address pointed by the pointer
 * \param offset The offset (relative displacement) of the pointer (for relative pointers)
 * \param type Type of the pointer operand
 * \return An operand of type pointer pointing to the specified address
 */
extern oprnd_t* oprnd_new_ptr(maddr_t addr, pointer_offset_t offset,
      pointer_type_t type);

/**
 * Creates a new oprnd of type register
 * \param reg The register
 * \return A oprnd structure of type REGISTER
 */
extern oprnd_t* oprnd_new_reg(reg_t* reg);

/**
 * Creates a new memory relative operand
 * \param seg The name of the base segment register for the memory address (can be NULL)
 * \param base The name of the register containing the base memory address. It must be of type RIP_TYPE.
 * \param index The name of the index register for the memory address (can be NULL)
 * \param scale The scale associated to the index (if present)
 * \param addr Address pointed by the relative memory
 * \param offset Offset (relative displacement) to reach the address
 * \param type Type of the pointer operand
 * \return A oprnd of type relative memory pointing to the specified address. Its base will be set to NULL if \c base is not of type RIP_TYPE
 * */
extern oprnd_t* oprnd_new_memrel(reg_t* seg, reg_t* base, reg_t* index,
      int scale, maddr_t addr, memory_offset_t offset, pointer_type_t type);

/**
 * Creates a relative memory operand from a memory and a pointer structures
 * \param mem A pointer to the structure describing the memory
 * \param ptr A pointer to a structure describing the reference
 * \return A relative memory operand corresponding to the pointer
 * */
extern oprnd_t* oprnd_new_memory_pointer(memory_t* mem, pointer_t* ptr);

/**
 * Creates a new pointer operand from an existing pointer structure
 * \param ptr A pointer to a structure describing an operand
 * \return A oprnd of type pointer corresponding to the given pointer, or NULL if \c ptr is NULL
 * */
extern oprnd_t* oprnd_new_pointer(pointer_t* ptr);

/**
 * Parses an memory operand (starting at the offset) from a string representation of an instruction, based on a given architecture
 * \param strinsn The string containing the representation of the instruction.
 * \param pos Pointer to the index into the string at which the parsing starts. Will be updated to contain the
 * index at which the memory operand ends (operand separator or end of string)
 * \param arch Pointer to the architecture structure for which the operand must be parsed
 * \return The parsed operand, or NULL if the parsing failed
 * */
extern oprnd_t* oprnd_parsenew_memory(char* strinsn, int *pos, arch_t* arch);

/**
 * Parses an operand from a string representation of an instruction, based on a given architecture
 * \param strinsn The string containing the representation of the instruction.
 * \param pos Pointer to the index into the string at which the parsing starts. Will be updated to contain the
 * index at which the operand ends (operand separator or end of string
 * \param arch Pointer to the architecture structure for which the operand must be parsed
 * \return The parsed operand, or NULL if the parsing failed
 * */
extern oprnd_t* oprnd_parsenew(char* strinsn, int *pos, arch_t* arch);

/**
 * Checks if two operands are equal
 * \param oprnd1 First operand
 * \param oprnd2 Second operand
 * \return TRUE if both operands are equal (same type, same register names and types, same offset for pointer operands), FALSE otherwise
 * */
extern boolean_t oprnd_equal(oprnd_t* oprnd1, oprnd_t* oprnd2);

/**
 * Frees a oprnd
 * \param p A pointer to the oprnd to be freed
 */
extern void oprnd_free(void* p);

/**
 * Dumps the contents of a oprnd structure
 * \param p A pointer to the oprnd
 * \param arch architecture the operand belongs to
 */
extern void oprnd_dump(void* p, arch_t* arch);

/**
 * Get the oprnd field if oprnd type is POINTER
 * \param oprnd an operand
 * \return a pointer structure or PTR_ERRor PTR_ERROR if oprnd is NULL or its type is not POINTER
 */
extern pointer_t* oprnd_get_ptr(oprnd_t* oprnd);

/**
 * Retrieves the address pointed to by an operand containing a pointer
 * \param oprnd The operand to analayze
 * \return The address pointed to by \c oprnd, or SIGNED_ERROR if it is NULL or does not contain a pointer
 */
extern maddr_t oprnd_get_refptr_addr(oprnd_t* oprnd);

/**
 * Returns the instruction pointed by an operand containing a pointer
 * \param oprnd The operand
 * \return The instruction pointed by \c oprnd, or NULL if it is NULL, does not contain a pointer , or not
 * referencing an instruction
 * */
extern insn_t* oprnd_get_refptr_insn_target(oprnd_t* oprnd);

/**
 * Returns the data referenced by an operand of type POINTER or MEMORY_RELATIVE
 * \param oprnd The operand
 * \return The data referenced by \c oprnd, or NULL if it is NULL, not of type POINTER or MEMORY_RELATIVE, or not
 * referencing a data
 * */
extern data_t* oprnd_get_ptr_data_target(oprnd_t* oprnd);

/**
 * Get the pointer associated to an oprnd if its type is MEMORY_RELATIVE
 * \param oprnd an operand
 * \return a pointer structure or PTR_ERROR if oprnd is NULL or its type is not MEMORY_RELATIVE
 */
extern pointer_t* oprnd_get_memrel_pointer(oprnd_t* oprnd);

/**
 * Get the pointer associated to an oprnd if it contains a pointer
 * \param oprnd an operand
 * \return a pointer structure or PTR_ERROR if oprnd is NULL or does not contain a pointer
 */
extern pointer_t* oprnd_get_refptr(oprnd_t* oprnd);

/**
 * Get the memory structure associated to an oprnd if its type is MEMORY
 * \param oprnd an operand
 * \return a memory structure or PTR_ERROR if oprnd is NULL or its type is not MEMORY
 */
extern memory_t* oprnd_get_memory(oprnd_t* oprnd);

/**
 * Get the memory pointer structure associated to an oprnd if its type is MEMORY_RELATIVE
 * \param oprnd an operand
 * \return a memory structure or PTR_ERROR if oprnd is NULL or its type is not MEMORY_RELATIVE
 */
extern memrel_t* oprnd_get_memrel(oprnd_t* oprnd);

/**
 * Get the pointer associated to an oprnd if its type is MEMORY_RELATIVE
 * \param oprnd an operand
 * \return a pointer structure or PTR_ERROR if oprnd is NULL or its type is not MEMORY_RELATIVE
 */
extern pointer_t* oprnd_get_memrel_pointer(oprnd_t* oprnd);

/**
 * Returns the register base name for a memory oprnd
 * \param oprnd A pointer to the memory oprnd
 * \return The name of the base register or PTR_ERROR if the oprnd is not a memory address
 */
extern reg_t* oprnd_get_base(oprnd_t* oprnd);

/**
 * Returns the value of an immediate oprnd
 * \param oprnd A pointer to the immediate oprnd
 * \return The oprnd value or 0 if the oprnd is not an immediate
 * \todo Add an error code handling if the oprnd is not an immediate
 * as it could legitimely be 0
 */
extern imm_t oprnd_get_imm(oprnd_t* oprnd);

/**
 * Returns the register index name for a memory oprnd
 * \param oprnd A pointer to the memory oprnd
 * \return The name of the index register or PTR_ERROR if the oprnd is not a memory address
 */
extern reg_t* oprnd_get_index(oprnd_t* oprnd);

/**
 * Returns the offset value for a memory operand oprnd
 * \param oprnd A pointer to the oprnd
 * \return The value of the offset or SIGNED_ERROR if the oprnd is not a memory operand
 */
extern memory_offset_t oprnd_get_offset(oprnd_t* oprnd);

/**
 * Returns the register name for a register oprnd
 * \param oprnd A pointer to the register oprnd
 * \return The name of the register or PTR_ERROR if the oprnd is not a register
 */
extern reg_t* oprnd_get_reg(oprnd_t* oprnd);

/**
 * Returns the scale value for a memory operand oprnd
 * \param oprnd A pointer to the oprnd
 * \return The value of the scale or SIGNED_ERROR if the oprnd is not a memory operand
 */
extern int oprnd_get_scale(oprnd_t* oprnd);

/**
 * Returns the register segment name for a memory oprnd
 * \param oprnd A pointer to the memory oprnd
 * \return The name of the segment register or PTR_ERROR if the oprnd is not a memory address
 */
extern reg_t* oprnd_get_seg(oprnd_t* oprnd);

/**
 * Returns the type of a oprnd
 * \param oprnd The oprnd to analyze
 * \return The type of the oprnd
 */
extern oprnd_type_t oprnd_get_type(oprnd_t* oprnd);

/**
 * Returns the extension of an operand
 * \param oprnd A pointer to the operand
 * \return A pointer to the operand extension
 */
extern void* oprnd_get_ext(oprnd_t* oprnd);

/**
 * Returns the length of the coding of a oprnd (if has a single coding)
 * \param oprnd A pointer to the structure holding the oprnd
 * \return oprnd size or 0 in oprnd is NULL
 */
extern int oprnd_get_length(oprnd_t* oprnd);

/**
 * Returns the size identifier of the operand (immediate and relative) or the size
 * of the data it contains
 * \param oprnd A pointer to the structure holding the operand
 * \return Identifier of the operand's size (as defined in the \ref data_sizes enum)
 * or DATASZ_UNDEF if oprnd is NULL
 */
extern data_size_t oprnd_get_bitsize(oprnd_t* oprnd);

/**
 * Gets the size value for an operand size identifier
 * \param datasz The operand size identifier
 * \return The associated size in bits
 * */
extern int datasz_getvalue(int datasz);

/**
 * Returns the name of an operand type
 * \param oprnd_type Operand type
 * \return Name of the type of operand
 * */
extern char* oprnd_type_get_name(oprnd_type_t oprnd_type);

/**
 * Gets the representation of the enum value for operand size (bitsize)
 * \param oprnd an operand
 * \return the operand size (in bit)
 */
extern int oprnd_get_size_value(oprnd_t* oprnd);

/**
 * Checks if a oprnd is an immediate
 * \param oprnd The oprnd to analyze
 * \return TRUE if a oprnd is of type immediate, FALSE otherwise
 */
extern int oprnd_is_imm(oprnd_t* oprnd);

/**
 * Checks if a oprnd is a memory address
 * \param oprnd The oprnd to analyze
 * \return TRUE if a oprnd is of type address, FALSE otherwise
 */
extern int oprnd_is_mem(oprnd_t* oprnd);

/**
 * Checks if a oprnd is a memory relative address
 * \param oprnd The oprnd to analyze
 * \return TRUE if a oprnd is of type relative memory address, FALSE otherwise
 */
extern int oprnd_is_memrel(oprnd_t* oprnd);

/**
 * Checks if a oprnd is a register
 * \param oprnd The oprnd to analyze
 * \return TRUE if the oprnd is of type register, FALSE otherwise
 */
extern int oprnd_is_reg(oprnd_t* oprnd);

/**
 * Checks if a oprnd is a pointer
 * \param oprnd The oprnd to analyze
 * \return TRUE if a oprnd is of type pointer, FALSE otherwise
 */
extern int oprnd_is_ptr(oprnd_t* oprnd);

/**
 * Checks if a oprnd references another address
 * \param oprnd The oprnd to analyze
 * \return TRUE if a oprnd is of type pointer, memory relative or immediate address, FALSE otherwise
 */
extern int oprnd_is_refptr(oprnd_t* oprnd);

/**
 * Sets the offset value for a memory operand oprnd.
 * If the oprnd is not a memory operand, nothing is done
 * \param oprnd A pointer to the oprnd
 * \param offset The new value of the offset
 */
extern void oprnd_set_offset(oprnd_t* oprnd, memory_offset_t offset);

/**
 * Sets the size identifier of the operand (immediate and relative) or the size
 * of the data it contains
 * \param oprnd A pointer to the structure holding the oprnd
 * \param s Identifier of the operand's size (as defined in the \ref data_sizes enum)
 */
extern void oprnd_set_bitsize(oprnd_t* oprnd, data_size_t s);

/**
 * Sets the address pointed to by an operand containing a pointer
 * \param oprnd The operand to update
 * \param addr The new address to be pointed by the operand
 */
extern void oprnd_set_ptr_addr(oprnd_t* oprnd, maddr_t addr);

/**
 * Updates the annotate flag of an instruction. The given value will be or-ed
 * to its existing value
 * \param insn The instruction to update
 * \param annotate The flag to add to the existing annotate flag
 * */
extern void insn_upd_annotate(insn_t* insn, unsigned int annotate);

/**
 * Prints an oprnd in a format suited for display
 * \param insn The instruction the operand belongs to. If not NULL, the destination of
 * relative operands will be printed, and only their value otherwise
 * \param oprnd The operand to print
 * \param str An existing char array.
 * \param size Size of the char array
 * \param arch Pointer to the structure describing the architecture this instruction is defined in
 * \return The number of written characters
 */
extern int oprnd_print(insn_t* insn, void* p, char* str, size_t size,
      arch_t* arch);

/**
 * Prints a oprnd in a format similar to objdump to a file stream
 * \param insn The instruction the operand belongs to. If not NULL, the destination of
 * relative operands will be printed, and only their value otherwise
 * \param oprnd The operand to print
 * \param f A file stream.
 * \param arch The architecture the operand belongs to
 */
extern void oprnd_fprint(insn_t* insn, void* p, FILE* f, arch_t* arch);

/**
 * Sets the role of an operand
 * \param oprnd The operand to update
 * \param role The value of the role
 */
extern void oprnd_set_role(oprnd_t* oprnd, char role);

/**
 * Flags the operand as a source
 * \param oprnd The operand to update
 */
extern void oprnd_set_role_src(oprnd_t* oprnd);

/**
 * Flags the operand as a destination
 * \param oprnd The operand to update
 */
extern void oprnd_set_role_dst(oprnd_t* oprnd);

/**
 * Flags the memory operand to have a base register write back after access
 * \param oprnd The operand to update
 */
extern void oprnd_mem_set_wbflag(oprnd_t* oprnd);

/**
 * Flags the memory operand to be post indexed
 * \param oprnd The operand to update
 */
extern void oprnd_mem_set_piflag(oprnd_t* oprnd);

/**
 * Flags the register operand as an indexed register
 * \param oprnd The operand to update
 */
extern void oprnd_reg_set_irflag(oprnd_t* oprnd);

/**
 * Sets the index of an indexed register operand
 * \param oprnd The operand to update
 * \param index The index of the register operand
 */
extern void oprnd_reg_set_index(oprnd_t* oprnd, char index);

/**
 * Sets the value of an immediate oprnd
 * \param oprnd A pointer to the immediate oprnd
 * \param The value to set
 */
extern void oprnd_imm_set_value(oprnd_t* oprnd, imm_t value);

/**
 * Gets the index of an indexed register operand
 * \param oprnd The operand to update
 * \return The index of the register operand
 */
extern uint8_t oprnd_reg_get_index(oprnd_t* oprnd);

/**
 * Gets the role of an operand
 * \param oprnd The operand to analyse
 * \return The operand's role
 */
extern char oprnd_get_role(oprnd_t* oprnd);

/**
 * Checks if an operand register has an index
 * \param oprnd The operand to analyse
 * \return TRUE if the regiser has an index
 */
extern char oprnd_reg_is_indexed(oprnd_t* oprnd);

/**
 * Checks if the base register of a memory operand is write back
 * \param oprnd The operand to analyse
 * \return TRUE if the base register is write back, FALSE otherwise
 */
extern char oprnd_mem_base_reg_is_dst(oprnd_t* oprnd);

/**
 * Checks if a memory operand is post indexed
 * \param oprnd The operand to analyse
 * \return TRUE if the memory is post indexed, FALSE otherwise
 */
extern char oprnd_mem_is_postindexed(oprnd_t* oprnd);

/**
 * Checks if an operand is a source
 * \param oprnd The operand to analyse
 * \return TRUE if the operand is a source, FALSE otherwise
 */
extern char oprnd_is_src(oprnd_t* oprnd);

/**
 * Checks if an operand is a destination
 * \param oprnd The operand to analyse
 * \return TRUE if the operand is a destination, FALSE otherwise
 */
extern char oprnd_is_dst(oprnd_t* oprnd);

/**
 * Changes the reg field in an operand (this operand must already be a register operand)
 * \param oprnd an operand
 * \param reg the new value of the register operand
 */
extern void oprnd_change_reg(oprnd_t* oprnd, reg_t* reg);

/**
 * Creates a new operand from a model. The copy does not contain any arch-specific elements
 * \param src The operand to copy from.
 * \return A new operand with the same characteristics as the source
 * */
extern oprnd_t* oprnd_copy_generic(oprnd_t* src);

/**
 * Sets the extension of an operand
 * \param oprnd The operand to update
 * \param role The extension to link with the operand
 */
extern void oprnd_set_ext(oprnd_t* oprnd, void* ext);

///////////////////////////////////////////////////////////////////////////////
//                             insn debug data                               //
///////////////////////////////////////////////////////////////////////////////
/**
 * Macro for optimising the retrieval of the the address of an instruction
 * \param I an instruction
 * \return Its address or SIGNED_ERROR if \e in is NULL
 */
#define INSN_GET_ADDR(I) ( ( (insn_t*)I ) ? ( (insn_t*)(I) )->address:SIGNED_ERROR )

#define insn_next_addr(I) (INSN_GET_ADDR(I) + insn_get_size(I) / 8)

///////////////////////////////////////////////////////////////////////////////
//                                  insn                                     //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct insn_s
 *      \brief Describes an instruction
 */
struct insn_s {
   oprnd_t** oprndtab; /**<array of the instruction's operands*/
   bitvector_t *coding; /**<Holds the binary coding of the instruction*/
   void *ext; /**<Extensions*/
   block_t* block; /**<Block which contain this instruction*/
   label_t* fctlbl; /**<Label of the function the instruction belongs to (the latest label encountered)*/
   list_t *sequence; /**<Pointer to the list object containing the instruction */
   dbg_insn_t* debug; /**<Pointer to the debug data for this instruction*/
   arch_t* arch; /**<Architecture the instruction belongs to*/
   maddr_t address; /**<Address at which the instruction is located*/
   uint32_t variant_id; /**<Identifier of the variant of this instruction*/
   uint32_t annotate; /**<A set of flags used to get data on the role of the instruction in the CFG
    (jump, start of block ...). Flags are described at the beginning of the file*/
   int16_t opcode; /**<Name id of the assembler instruction (defined in asmfile_t structure)*/
   uint8_t opprefx; /**<Prefix of of the instruction (defined in asmfile_t structure)*/
   uint8_t opsuffx; /**<Suffix of of the instruction (defined in asmfile_t structure)*/
   uint8_t nb_oprnd; /**<Number of operand of the instruction*/
   uint8_t read_size; /**<Size of the elements the instruction work with*/
   uint8_t elt_out; /**<4 lsb: Size; 5th to 8th lsb: type of the elements the instruction returns */
   uint8_t elt_in; /**<4 lsb: Size; 5th to 8th lsb: type of the elements the instruction get as input*/
};

/**
 * \struct dbg_insn_s
 * \brief This structure contains debug data for instructions
 */
struct dbg_insn_s {
   char* srcfile; /**<Source file of the instruction*/
   unsigned int srcline; /**<Source line of the instruction*/
};

/**
 * Retrieves the register type of a given instruction's operand
 * \param insn An instruction
 * \param pos The index of the register operand whose type we want to retrieve (starting at 0)
 * \return The type of the register operand at the given index, or -1 if it is not a register or does not exist
 * */
extern char insn_get_reg_oprnd_type(insn_t* insn, int pos);

/**
 * Gets the type of given instruction's operand
 * \param insn The instruction to update
 * \param pos The index of the operand whose type we want to retrieve (starting at 0)
 * \return The type of the operand at the given index
 * */
extern int insn_get_oprnd_type(insn_t* insn, int pos);

/**
 * Gets the size of given instruction's operand
 * \param insn The instruction
 * \param pos The index of the operand whose size we want to retrieve (starting at 0)
 * \return The bitsize value for the operand at the given position, or -1 if the operand does not exist
 * */
extern int insn_get_oprnd_bitsize(insn_t* insn, int pos);

/**
 * Creates a new instruction
 * \param arch The architecture for which the instruction is defined
 * \return A blank instruction with no opcode, empty coding and operand list, bit size
 * equal to 0 and an address equal to SIGNED_ERROR, or NULL if \c arch is NULL
 */
extern insn_t* insn_new(arch_t* arch);

/**
 * Creates a new instruction from its string representation
 * \param strinsn The instruction in string form. It must have the same format as an instruction printed using insn_print
 * \param arch The architecture this instruction belongs to
 * \return The structure containing the instruction (without its coding), or PTR_ERROR if the instruction could not be parsed
 * */
extern insn_t* insn_parsenew(char* strinsn, arch_t* arch);

/**
 * Frees an instruction
 * \param p The instruction to free
 */
extern void insn_free(void* p);

/**
 * Frees an instruction structure
 * \param p The instruction to be freed
 */
extern void insn_free_common(void* p);

/**
 * Adds an instruction into a list of instruction
 * \param insn An instruction
 * \param insn_list A list of instructions
 */
extern void add_insn_to_insnlst(insn_t* insn, queue_t* insn_list);

/**
 * Adds an instruction into a block
 * \param insn An instruction
 * \param block a block
 */
extern void add_insn_to_block(insn_t* insn, block_t* block);

/**
 * Checks if two instructions are identical using the architecture-specific functions.
 * \param insn1 First instruction
 * \param insn2 Second instruction
 * \return TRUE if the instructions are equal (same opcode, same number of operands, same types of operands), FALSE otherwise
 */
extern boolean_t insn_equal(insn_t* insn1, insn_t* insn2);

/**
 * Updates the annotate flag of an instruction by adding a flag
 * \param insn The instruction to update
 * \param annotate The annotate flag to add to the existing annotate flag of the instruction
 * */
extern void insn_add_annotate(insn_t* insn, unsigned int annotate);

/**
 * Adds an immediate operand to an instruction
 * \param insn The instruction to update
 * \param imm The value of the immediate operand
 */
extern void insn_add_imm_oprnd(insn_t* insn, imm_t imm);

/**
 * Adds a memory operand to an instruction
 * \param insn The instruction to update
 * \param seg The name of the base segment register for the memory address (can be NULL)
 * \param base The name of the register containing the base memory address (can be NULL)
 * \param index The name of the index register for the memory address (can be NULL)
 * \param offset The value of displacement from the base for the memory address
 * \param scale The scale associated to the index (if present)
 */
extern void insn_add_mem_oprnd(insn_t* insn, reg_t* seg, reg_t* base,
      reg_t* index, memory_offset_t offset, int scale);

/**
 * Adds a oprnd to an instruction
 * \param insn The instruction to update
 * \param oprnd A pointer to the structure holding the operand
 */
extern void insn_add_oprnd(insn_t* insn, oprnd_t* oprnd);

/**
 * Adds a relative address operand to an instruction
 * \param insn The instruction to update
 * \param addr The address pointed to by the pointer
 * \param offset For pointer of type POINTER_RELATIVE, the offset (relative displacement) from the current address
 * \param type Type of the pointer
 */
extern void insn_add_ptr_oprnd(insn_t* insn, maddr_t addr,
      pointer_offset_t offset, pointer_type_t type);

/**
 * Adds a register operand to an instruction
 * \param insn The instruction to update
 * \param name A pointer to the structure containing the register
 */
extern void insn_add_reg_oprnd(insn_t* insn, reg_t* name);

/**
 * Updates the coding of the instruction by appending the additional coding
 * \param insn The instruction to update
 * \param appendcode A bitvector holding the bits to append to the instruction's coding
 */
extern void insn_append_coding(insn_t* insn, bitvector_t* appendcode);

/**
 * Checks if the annotate flag of an instruction contains a given flag (or flags)
 * \param insn An instruction
 * \param annotate The flags to check in the instruction
 * \return TRUE if all the flags contained in \e annotate are present in the annotate flag
 * of the instruction, FALSE otherwise
 * */
extern int insn_check_annotate(insn_t* insn, unsigned int annotate);

/**
 * Checks if an instruction contains a reference to another part of the code or the file
 * \param insn The instruction to check
 * \param isinsn Return parameter. Contains the guessed type of destination
 * (1 if destination is an instruction and 0 otherwise (data block)
 * \return the address of the destination, or -1 if the instruction does not reference any other part of the code
 * \todo Rewrite this function using annotate
 * */
extern maddr_t insn_check_refs(insn_t* insn, int *isinsn);

/**
 * Retrieves an operand referencing another address, either through a pointer or a relative memory address
 * \param insn The instruction
 * \return An operand referencing another address, or NULL if the instruction has no such operand
 * */
extern oprnd_t* insn_lookup_ref_oprnd(insn_t* insn);

/**
 * Copies an instruction into another.
 * \param insn The instruction to copy
 * \return A pointer to a new instruction structure with identical values to \e in
 * \warning If the instruction is a branch, its destination will be identical for the copy, so additional treatments may have
 * to be performed when duplicating a list. The extension, block and label are not initialised in the copy.
 * Use this function with caution
 * */
extern insn_t* insn_copy(insn_t* insn);

/**
 * Retrieves the variant identifier of an instruction
 * \param insn The instruction
 * \return Its variant identifier, or 0 if the instruction is NULL
 * */
extern uint32_t insn_get_variant_id(insn_t* insn);

/**
 * Returns the opcode name of an instruction
 * \param insn The instruction
 * \return The instruction's opcode or PTR_ERROR if insn is NULL
 */
extern char* insn_get_opcode(insn_t* insn);

/**
 * Returns the opcode code of an instruction
 * \param insn The instruction
 * \return The instruction's opcode code or UNSIGNED_ERROR if insn is NULL
 */
extern int16_t insn_get_opcode_code(insn_t* insn);

/**
 * Retrieves the instruction prefix
 * \param insn The instruction
 * \return The instruction's prefix or PTR_ERROR if insn is NULL
 */
extern int8_t insn_get_prefix(insn_t* insn);

/**
 * Retrieves the instruction suffix
 * \param insn The instruction
 * \return The instruction's suffix or PTR_ERROR if insn is NULL
 */
extern int8_t insn_get_suffix(insn_t* insn);

/**
 * Retrieves the instruction set
 * \param insn The instruction
 * \return The instruction set or 0 if \c insn is NULL
 */
extern uint8_t insn_get_iset(insn_t* insn);

/**
 * Returns the number of oprnds of an instruction
 * \param insn The instruction
 * \return The number of operands or SIGNED_ERROR if insn is NULL
 */
extern uint8_t insn_get_nb_oprnds(insn_t* insn);

/**
 * Returns the annotate flags associated to an instruction
 * \param insn an instruction
 * \return the flags or 0 if insn is NULL
 */
extern uint32_t insn_get_annotate(insn_t* insn);

/**
 * Returns the default annotate flags associated to an instruction. This function is mainly
 * intended to retrieve information about an instruction that was not retrieved through disassembly
 * \param insn an instruction
 * \return the flags or 0 if \c insn or \c arch is NULL
 */
extern uint32_t insn_get_default_annotate(insn_t* insn);

/**
 * \warning Deprecated and will be removed, use insn_get_arch instead
 * */
extern arch_t* insn_getarch_fromblock(insn_t* insn);

/**
 * \warning Deprecated and will be removed, use insn_get_asmfile instead
 * */
extern asmfile_t* insn_get_asmfile_fromblock(insn_t* insn);

/**
 * Retrieves the address of an instruction
 * \param insn The instruction to analyze
 * \return Its address or SIGNED_ERROR if \e insn is NULL
 */
extern maddr_t insn_get_addr(insn_t* insn);

/**
 * Retrieves the address at the end of an instruction
 * \param insn an instruction
 * \return The address immediately following its end or ADDRESS_ERROR if \c insn is NULL
 */
extern maddr_t insn_get_end_addr(insn_t* insn);

/**
 * Retrieves tha array of operands
 * \param insn an instruction
 * \return an array of pointers on oprnds (its size can be got using insn_get_nb_oprnds)
 */
extern oprnd_t** insn_get_oprnds(insn_t* insn);

/**
 * Retrieves debug data for an instruction
 * \param insn an instruction
 * \return debug data or PTR_ERROR if insn is NULL
 */
extern insn_dbg_t* insn_get_dbg(insn_t* insn);

/**
 * Retrieves the binary coding of an instruction
 * \param insn The instruction to analyze
 * \return Its coding or PTR_ERROR if insn is NULL
 */
extern bitvector_t* insn_get_coding(insn_t* insn);

/**
 * Retrieves extensions associated to an instruction
 * \param insn an instruction
 * \return a structure (defined by the user) which contains extensions
 */
extern void* insn_get_ext(insn_t* insn);

/**
 * Retrieves the block which contains the instruction
 * \param insn The instruction
 * \return a block or PTR_ERROR if insn has no block or is NULL
 */
extern block_t* insn_get_block(insn_t* insn);

/**
 * Retrieves the function which contains the instruction
 * \warning flow analysis has to be already done
 * \param insn an instruction
 * \return a function or PTR_ERROR if insn has no function or is NULL
 */
extern fct_t* insn_get_fct(insn_t* insn);

/**
 * Retrieves the loop which contains the instruction
 * \warning flow and loop analysis have to be already done
 * \param insn an instruction
 * \return a loop or PTR_ERROR if insn has no loop or is NULL
 */
extern loop_t* insn_get_loop(insn_t* insn);

/**
 * Retrieves the asmfile which contains the instruction
 * \warning flow analysis has to be already done
 * \param insn an instruction
 * \return a asmfile or PTR_ERROR if insn has no asmfile or is NULL
 */
extern asmfile_t* insn_get_asmfile(insn_t* insn);

/**
 * Retrieves the project which contains the instruction
 * \warning flow analysis has to be already done
 * \param insn an instruction
 * \return a project or PTR_ERROR if insn has no project or is NULL
 */
extern project_t* insn_get_project(insn_t* insn);

/**
 * Retrieves the label of the function the instruction belongs to
 * \param insn The instruction
 * \return The label of the function the instruction belongs to (last label before it) or PTR_ERROR if insn is NULL
 */
extern label_t* insn_get_fctlbl(insn_t* insn);

/**
 * Retrieves the list element which contains the instruction in the global instrucion list
 * \param insn an instruction
 * \return The list element or PTR_ERROR if insn is NULL
 */
extern list_t* insn_get_sequence(insn_t* insn);

/**
 * Retrieves the previous instruction in the global instruction list
 * \note if performance critical use INSN_GET_PREV
 * \param insn an instruction
 * \return previous instruction or PTR_ERROR if anything NULL in the way
 */
extern insn_t* insn_get_prev(insn_t* insn);

/**
 * Retrieves the previous instruction in the global instruction list
 * \warning no check
 * \param insn an instruction
 * \return previous instruction
 */
#define INSN_GET_PREV(insn) ((insn)->sequence->prev->data)

/**
 * Retrieves the next instruction in the global instruction list
 * \note if performance critical use INSN_GET_NEXT
 * \param insn an instruction
 * \return next instruction or PTR_ERROR if anything NULL in the way
 */
extern insn_t* insn_get_next(insn_t* insn);

/**
 * Retrieves the next instruction in the global instruction list
 * \warning no check
 * \param insn an instruction
 * \return next instruction
 */
#define INSN_GET_NEXT(insn) ((insn)->sequence->next->data)

/**
 * Retrieves the destination address of a branch instruction (0 if instruction is not a branch)
 * \param insn A pointer to the structure holding the branch instruction
 * \return The destination's address, or ADDRESS_ERROR if the instruction is not a branch or is NULL
 * \note This function assumes that a branch instruction may only have one pointer operand
 */
extern maddr_t insn_find_pointed(insn_t* insn);

/**
 * Returns a pointer to the oprnd of an instruction at the specified position
 * (0 is the first oprnd)
 * \param insn The instruction
 * \param pos The index of the oprnd to be looked for (starting at 0)
 * \return A pointer to the structure holding the oprnd, or PTR_ERROR if pos is out of range
 * (negative or greater than the number of oprnds of insn)
 */
extern oprnd_t* insn_get_oprnd(insn_t* insn, int pos);

/**
 * Retrieves the size in bits of an instruction
 * \param insn The instruction to analyze
 * \return Its size in bits or 0 if insn is NULL: DW_LANG_Fortran77, DW_LANG_Fortran90, DW_LANG_Fortran95
 */
extern int insn_get_size(insn_t* insn);

/**
 * Retrieves the size in bytes of an instruction
 * \param insn The instruction to analyze
 * \return Its size in bytes or 0 if insn is NULL
 */
extern unsigned int insn_get_bytesize(insn_t* insn);

/**
 * Retrieves the pointer to the next instruction of a branch instruction (NULL if not a branch)
 * \param insn The instruction to analyze
 * \return A pointer to the instruction pointed to by insn, or PTR_ERROR if none was found
 * \note This function assumes that a branch instruction has one and only one operand
 * \todo Add a better way to identify branches (maybe from the grammar)
 */
extern insn_t* insn_get_branch(insn_t* insn);

/**
 * Checks whether an instruction is SIMD (uses SIMD registers and/or units)
 * \param insn An instruction
 * \return The simd flag associated to the opcode, or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_simd(insn_t *insn);

/**
 * Checks whether an instruction is SIMD (uses SIMD registers and/or units)
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_SIMD(insn_t* insn);

/**
 * Checks whether an instruction processes at input integer elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_INT(insn_t *insn);

/**
 * Checks whether an instruction processes at input FP elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_FP(insn_t *insn);

/**
 * Checks whether an instruction processes at input a structure or a string
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_struct_or_str(insn_t *insn);

/**
 * Checks whether an instruction is SIMD and processes integer elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_SIMD_INT(insn_t *insn);

/**
 * Checks whether an instruction is SIMD and processes FP (Floating-point) elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_SIMD_FP(insn_t *insn);

/**
 * Checks whether an instruction is SIMD and processes non FP (Floating-point) elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_SIMD_NOT_FP(insn_t *insn);

/**
 * Returns the SIMD width for an SIMD instruction, that is the number of elements processed at input
 * \param insn An instruction
 * \return number of elements (number)
 */
extern unsigned short insn_get_SIMD_width(insn_t *insn);

/**
 * Checks whether an instruction is packed (a vector instruction)
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_packed(insn_t *insn);

/**
 * Checks whether an instruction processes single-precision (32 bits) FP elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_single_prec(insn_t *insn);

/**
 * Checks whether an instruction processes double-precision (64 bits) FP elements
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_double_prec(insn_t *insn);

/**
 * Checks whether an instruction is a prefetch
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_prefetch(insn_t *insn);

/**
 * Checks whether an instruction has a source (read) memory operand
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_has_src_mem_oprnd(insn_t *insn);

/**
 * Checks whether an instruction is a load, that is reads data from memory into a register
 * Warning: it is assumed that there are no implicit (out of operands) loads !
 * A warning is emitted for instructions with no operands
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_load(insn_t *insn);

/**
 * Checks whether an instruction has a destination (written) memory operand
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_has_dst_mem_oprnd(insn_t *insn);

/**
 * Checks whether an instruction is a store, that is writes data from a register to memory
 * Warning: it is assumed that there are no implicit (out of operands) stores !
 * A warning is emitted for instructions with no operands
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_store(insn_t *insn);

/**
 * Returns the first memory operand
 * \param insn An instruction
 * \return oprnd
 */
extern oprnd_t *insn_get_first_mem_oprnd(insn_t *insn);

/**
 * Returns position of the first memory operand
 * \param insn An instruction
 * \return index (or UNSIGNED_ERROR)
 */
extern int insn_get_first_mem_oprnd_pos(insn_t *insn);

/**
 * Returns position of the first memory operand, if read
 * NOTE : this function only considers mov sources (GAS syntax)
 * \return source operand index (index value starts at 0)
 */
extern int insn_get_oprnd_src_index(insn_t *insn);

/**
 * Returns position of the first memory operand, if written
 * NOTE : this function only considers mov sources (GAS syntax)
 * \return destination index (index value starts at 0)
 */
extern int insn_get_oprnd_dst_index(insn_t *insn);

/**
 * Checks whether an instruction is a add or a sub
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_add_sub(insn_t *insn);

/**
 * Checks whether an instruction is a mul
 * \param insn An instruction
 * \see insn:is_add_sub
 */
extern boolean_t insn_is_mul(insn_t *insn);

/**
 * Checks whether an instruction is an FMA (fused multiply-add/sub)
 * \param insn An instruction
 * \return FALSE/TRUE
 */
extern boolean_t insn_is_fma(insn_t *insn);

/**
 * Checks whether an instruction is a div
 * \param insn An instruction
 * \see insn:is_add_sub
 */
extern boolean_t insn_is_div(insn_t *insn);

/**
 * Checks whether an instruction is a reciprocal approximate
 * \param insn An instruction
 * \see insn:is_add_sub
 */
extern boolean_t insn_is_rcp(insn_t *insn);

/**
 * Checks whether an instruction is a sqrt (square root)
 * \param insn An instruction
 * \see insn:is_add_sub
 */
extern boolean_t insn_is_sqrt(insn_t *insn);

/**
 * Checks whether an instruction is a reciprocal sqrt (square root) approximate
 * \param insn An instruction
 * \see insn:is_add_sub
 */
extern boolean_t insn_is_rsqrt(insn_t *insn);

/**
 * Checks whether an instruction is arithmetical (add, sub, mul, fma, div, rcp, sqrt or rsqrt)
 * \param insn An instruction
 * \see insn:is_add_sub
 */
extern boolean_t insn_is_arith(insn_t *insn);

/**
 * Gets the family associated to an instruction
 * \param insn An instruction
 * \return The family or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_family(insn_t* insn);

/**
 * Faster/unsafe version of insn_get_family, reusing a given architecture
 * \warning You must be sure that you can reuse the architecture (on some processors, architecture can be switched on instruction granularity)
 * \warning instruction must be not NULL
 * \param insn An instruction
 * \return The family or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_family_from_arch(insn_t* insn, arch_t* arch);

/**
 * \warning Deprecated and will be removed, use insn_get_family or insn_get_family_from_arch instead
 */
extern unsigned short insn_getfamily_fromblock(insn_t* insn);

/**
 * Gets the class associated to an instruction
 * \param insn An instruction
 * \return The class or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_class(insn_t* insn);

/**
 * Gets the input element size the instruction works on
 * \param insn an instruction
 * \return the element size or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_input_element_size(insn_t* insn);

/**
 * Gets the input element size the instruction work on
 * \param insn an instruction
 * \return the element size (SZ enum type) or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_input_element_size_raw(insn_t* insn);

/**
 * Gets the output element size the instruction returns
 * \param insn an instruction
 * \return the element size or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_output_element_size(insn_t* insn);

/**
 * Gets the output element size the instruction returns
 * \param insn an instruction
 * \return the element size (SZ enum type) or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_output_element_size_raw(insn_t* insn);

/**
 * Gets the input element type the instruction work on
 * \param insn an instruction
 * \return the element type or UNSIGNED_ERROR if there is a problem
 */
extern unsigned int insn_get_input_element_type(insn_t* insn);

/**
 * Gets the output element type the instruction returns
 * \param insn an instruction
 * \return the element type or UNSIGNED_ERROR if there is a problem
 */
extern unsigned int insn_get_output_element_type(insn_t* insn);

/**
 * Gets the input element size the instruction works on
 * \param insn an instruction
 * \return the element size or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_input_element_size(insn_t* insn);

/**
 * Gets the input element size the instruction work on
 * \param insn an instruction
 * \return the element size (SZ enum type) or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_input_element_size_raw(insn_t* insn);

/**
 * Gets the output element size the instruction returns
 * \param insn an instruction
 * \return the element size or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_output_element_size(insn_t* insn);

/**
 * Gets the output element size the instruction returns
 * \param insn an instruction
 * \return the element size (SZ enum type) or UNSIGNED_ERROR if there is a problem
 */
extern unsigned short insn_get_output_element_size_raw(insn_t* insn);

/**
 * Gets the input element type the instruction work on
 * \param insn an instruction
 * \return the element type or UNSIGNED_ERROR if there is a problem
 */
extern unsigned int insn_get_input_element_type(insn_t* insn);

/**
 * Gets the output element type the instruction returns
 * \param insn an instruction
 * \return the element type or UNSIGNED_ERROR if there is a problem
 */
extern unsigned int insn_get_output_element_type(insn_t* insn);

/**
 * Gets the size actually read by the instruction
 * \param insn An instruction
 * \return The size read (can be DATASZ_UNDEF) or DATASZ_UNDEF if \c insn is NULL
 */
extern data_size_t insn_get_read_size(insn_t* insn);

/**
 * Gets groups (as a list) containing the instruction
 * \param insn An instruction
 * \return list of groups
 */
extern list_t *insn_get_groups(insn_t *insn);

/**
 * Gets the first group containing the instruction
 * \param insn An instruction
 * \return first group or PTR_ERROR if not found
 */
extern group_t *insn_get_first_group(insn_t *insn);

/**
 * Gets the architecture the instruction belongs to
 * \param insn The instruction
 */
extern arch_t* insn_get_arch(insn_t* insn);

/**
 * Dumps the raw contents of an insn_t structure
 * \param p The pointer to the instruction
 */
extern void insn_dump(void* p);

/**
 * Function for printing an instruction in a format suited for display
 * \param insn A pointer to the instruction
 * \param c A char pointer to an allocated string
 * \param size The size of the allocated string
 */
extern void insn_print(insn_t* insn, char* c, size_t size);

/**
 * Function for printing an instruction in the objdump style directly to a file stream
 * \param insn A pointer to the instruction
 * \param f A pointer to a file stream
 */
extern void insn_fprint(insn_t* insn, FILE *fp);

/**
 * Updates the annotate flag of an instruction by removing a flag
 * \param insn The instruction to update
 * \param annotate The annotate flag to remove from the existing annotate flag of the instruction
 * */
extern void insn_rem_annotate(insn_t* insn, unsigned int annotate);

/**
 * Checks whether an instruction is a branch
 * \param insn A pointer to the structure holding the instruction
 * \return TRUE if the instruction is a branch, FALSE if not a branch
 */
extern int insn_is_branch(insn_t* insn);

/**
 * Checks if an instruction is an indirect branch
 * \param insn A pointer to the structure holding the instruction
 * \return TRUE if the instruction is an indirect branch, FALSE otherwise
 */
extern int insn_is_indirect_branch(insn_t* insn);

/**
 * Checks if an instruction is an direct branch
 * \param insn A pointer to the structure holding the instruction
 * \return TRUE if the instruction is a direct branch, FALSE otherwise
 */
extern int insn_is_direct_branch(insn_t* insn);

/**
 * Checks whether an instruction is a jump (conditional or unconditional)
 * \param insn An instruction
 * \return TRUE/FALSE
 */
extern boolean_t insn_is_jump(insn_t *insn);

/**
 * Checks whether an instruction is a conditional jump (taken/not taken depends on a condition)
 * \param insn An instruction
 * \return TRUE/FALSE
 */
extern boolean_t insn_is_cond_jump(insn_t *insn);

/**
 * Checks whether an instruction is an unconditional jump (always taken or not taken)
 * \param insn An instruction
 * \return TRUE/FALSE
 */
extern boolean_t insn_is_uncond_jump(insn_t *insn);

/**
 * Points the function label of the instruction to the given string pointer
 * \param insn The instruction to update
 * \param fctlbl The pointer to the string holding the label
 */
extern void insn_link_fct_lbl(insn_t* insn, label_t* fctlbl);

/**
 * Sets the label of an instruction
 * \param insn The instruction to update
 * \param label The new label of the instruction
 */
extern void insn_set_fct_lbl(insn_t* insn, label_t* label);

/**
 * Sets the address of an instruction
 * \param insn The instruction to update
 * \param addr The new address of the instruction
 */
extern void insn_set_addr(insn_t* insn, maddr_t addr);

/**
 * Sets the annotate flag of an instruction (replacing the existing value)
 * \param insn The instruction to update
 * \param annotate The new value of the annotate flag
 * */
extern void insn_set_annotate(insn_t* insn, unsigned int annotate);

/**
 * Updates the pointer to the next instruction of a branch instruction
 * \param insn The instruction to update (must be a branch instruction)
 * \param dest The instruction to which in branches
 */
extern void insn_set_branch(insn_t* insn, insn_t* dest);

/**
 * Sets the instructions coding from a character string or a bitvector
 * \param insn The instruction to update
 * \param bytecode A byte string from which the coding will be read
 * \param len The lenght of the byte string
 * \param bvcoding A bitvector holding the new coding of the instruction (will be used if
 * bytecode is NULL)
 */
extern void insn_set_coding(insn_t* insn, unsigned char* bytecode, int len,
      bitvector_t* bvcoding);

/**
 * Sets the extensions associated to an instruction
 * \param insn an instruction
 * \param a structure (defined by the user) which contains extensions
 */
extern void insn_set_ext(insn_t* insn, void* ext);

/**
 * Sets the debug information of an instruction
 * \param insn The instruction
 * \param srcname Name of the source file associated to the instruction
 * \param srcline Line number in the source file associated to the instruction
 * */
extern void insn_set_debug_info(insn_t* insn, char* srcname, unsigned int srcline);

/**
 * Sets the input element size
 * \param insn The instruction
 * \param The element size
 */
extern void insn_set_input_element_size(insn_t* insn, uint8_t element_size);

/**
 * Sets the input element size
 * \param insn The instruction
 * \param The element size (SZ enum type)
 */
extern void insn_set_input_element_size_raw(insn_t* insn, uint8_t element_size);

/**
 * Sets the output element size
 * \param insn The instruction
 * \param The element size
 */
extern void insn_set_output_element_size(insn_t* insn, uint8_t element_size);

/**
 * Sets the output element size
 * \param insn The instruction
 * \param The element size (SZ enum type)
 */
extern void insn_set_output_element_size_raw(insn_t* insn, uint8_t element_size);

/**
 * Sets the input element size
 * \param insn The instruction
 * \param The element size
 */
extern void insn_set_input_element_type(insn_t* insn, uint8_t element_type);

/**
 * Sets the output element size
 * \param insn The instruction
 * \param The element size
 */
extern void insn_set_output_element_type(insn_t* insn, uint8_t element_type);

/**
 * Sets the size actually read by the instruction
 * \param insn An instruction
 * \param size The size read
 */
extern void insn_set_read_size(insn_t* insn, data_size_t size);

/**
 * Sets the architecture
 * \param insn The instruction
 * \param arch The architecture
 */
extern void insn_set_arch(insn_t* insn, arch_t* arch);

/**
 * Sets the identifier of the variant of an instruction
 * \param insn The instruction
 * \param variant_id The identifier of its variant
 * */
extern void insn_set_variant_id(insn_t* insn, uint32_t variant_id);

/**
 * Sets the instruction opcode name
 * \param insn The instruction to update
 * \param opcode The new name of the instruction
 */
extern void insn_set_opcode(insn_t* insn, int16_t opcode);

/**
 * Sets the instruction suffix
 * \param insn The instruction to update
 * \param suffix The new suffix of the instruction
 */
extern void insn_set_suffix(insn_t* insn, uint8_t suffix);

/**
 * Sets the instruction prefix
 * \param insn The instruction to update
 * \param prefix The new prefix of the instruction
 */
extern void insn_set_prefix(insn_t* insn, uint8_t prefix);

/**
 * Sets the instruction opcode name
 * \param insn The instruction to update
 * \param opcodestr The new name of the instruction
 */
extern void insn_set_opcode_str(insn_t* insn, char* opcodestr);

/**
 * Sets the instruction suffix
 * \param insn The instruction to update
 * \param suffixstr The new suffix of the instruction
 */
extern void insn_set_suffix_str(insn_t* insn, char* suffixstr);

/**
 * Sets the number of operands for the instruction and updates the
 * size of the array of operands accordingly
 * If the instruction already had operands, they will be kept
 * if the new number is bigger, otherwise the exceeding operands will be deleted
 * Any new created operand will be set to NULL
 * \param insn The instruction to update
 * \param nb_oprnds The new number of operands
 */
extern void insn_set_nb_oprnds(insn_t* insn, int nb_oprnds);

/**
 * Sets the operand at the given index with the given operand pointer
 * \param insn The instruction to update
 * \param pos The index of the uperand to set
 * \param oprnd The pointer to the new operand to set
 */
extern void insn_set_oprnd(insn_t* insn, int pos, oprnd_t* oprnd);

/**
 * Sets the sequence (list object containing it) of an instruction
 * \param insn The instruction to update
 * \param sequence The list object containing it
 * */
extern void insn_set_sequence(insn_t* insn, list_t* sequence);

/**
 * Indicate if an instruction modifies flag register (x86 specific)
 * */
extern int insn_alter_flags(insn_t *insn);

/**
 * Traverses all instructions in an insnlist
 * \param insn_list a queue of instructions
 * \param start Element in the queue at which to start the search (NULL if must be its head)
 * \param stop Element in the queue at which to stop the search (NULL if it must be its tail)
 * \param it name of an iterator (not already existing)
 */
#define FOREACH_INSN_IN_INSNLIST(insn_list,start,stop,it) list_t* it; \
   for (it = (start) ? start : queue_iterator (insn_list); it != stop; it = it->next)

/**
 * Returns a pointer to the instruction in an instruction list which is at given address
 * \param insn_list The instruction list to look up
 * \param addr The address at which an instruction is looked for
 * \param start Element in the queue at which to start the search (NULL if must be its head)
 * \param stop Element in the queue at which to stop the search (NULL if it must be its tail)
 * \return A pointer to the list containing the instruction at the given address,
 * or PTR_ERROR if none was found
 */
extern list_t* insnlist_addrlookup(queue_t* insn_list, maddr_t addr,
      list_t* start, list_t* stop);

/**
 * Returns the length in bits of an instruction list
 * \param insn_list The instruction list
 * \param start Element in the queue at which to start the count (NULL if must be its head)
 * \param stop Element in the queue at which to stop the count (NULL if it must be its tail)
 * \return The sum of all lengths in bits of the instructions in the list
 */
extern uint64_t insnlist_bitsize(queue_t* insn_list, list_t* start,
      list_t* stop);

/**
 * Returns the length in bytes of an instruction list
 * \param inl The instruction list
 * \param start Element in the queue at which to start the count (NULL if must be its head)
 * \param stop Element in the queue at which to stop the count (NULL if it must be its tail)
 * \return The sum of all lengths in bits of the instructions in the list
 */
extern uint64_t insnlist_findbytesize(queue_t* inl, list_t* start, list_t* stop);

/**
 * Copies parts of an instruction list
 * \param insn_list An instruction queue
 * \param start First element to copy, or NULL to copy from the start
 * \param stop Last element to copy, or NULL to copy up to the end
 * \return A copied instruction queue, in the same size and order as insn_list. Any copied instructions whose original pointed
 * to another instruction to be copied will now point to the copy of that instruction, while copied instructions whose original pointed
 * to an instruction that was not to be copied will point to the same instruction as the original.
 * Labels and blocks are not initialised in the copies. If \e insn_list is NULL, will return NULL
 * \warning Instructions copied this way will not be linked to any asmfile structure, so they will have to be freed manually
 * */
extern queue_t* insnlist_copy(queue_t* insn_list, list_t* start, list_t* stop);

/**
 * Inserts an instruction queue at the correct place in the asmfile.
 * \param af The asmfile in wich the instructions will be added.
 * \param insn_list A queue of instructions that will be inserted (no need
 * to free it, it is done in the function).
 * */
extern void insnlist_add_inplace(asmfile_t *asmfile, queue_t *insn_list);

/**
 * Retrieves the instruction at the corresponding address
 * \param insn_list The instruction list
 * \param addr The address at which to look up
 * \param start Element in the queue at which to start the search (NULL if must be its head)
 * \param stop Element in the queue at which to stop the search (NULL if it must be its tail)
 * \return A pointer to the instruction at the given address, or PTR_ERROR if none was found
 */
extern insn_t* insnlist_insnataddress(queue_t* insn_list, maddr_t addr,
      list_t* start, list_t* stop);

/**
 * Returns 1 if an instruction list has been fully disassembled (its last instruction is
 * not NULL or BAD_INSN).
 * \param insn_list The instruction list to check
 * \return TRUE if last instruction is not NULL or BAD_INSN, FALSE otherwise
 * \note This function does not check for BAD_INSN instructions in the middle of the list
 */
extern int insnlist_is_valid(queue_t* insn_list);

/**
 * Replaces in an instruction list the instructions at an address by the ones given
 * Uses the padding instruction (created with the genpadding function) to match the correct length
 * \param insn_list The instruction list to update
 * \param repl The instruction list to insert into insn_list
 * \param addr The address at which repl must be inserted
 * \todo It would probably be safer to get rid of the addr oprnd and use only seq
 * \param seq The list_t element containing the instruction at the address of which repl must be inserted
 * \param insnpadding The instruction to be used as padding (usually the NOP instruction)
 * \param nextinsn Will hold a pointer to the instruction in the list immediately after the one inserted
 * \param start Element in the queue at which to start the search for instructions to replace (NULL if must be its head)
 * \param stop Element in the queue at which to stop the search for instructions to replace (NULL if it must be its tail)
 * \return The instruction list that has been replaced in the original (including by padding)
 */
extern queue_t* insnlist_replace(queue_t* insn_list, queue_t* repl,
      maddr_t addr, list_t* seq, insn_t* insnpadding, insn_t **nextinsn,
      list_t** start, list_t** stop);

/**
 * Updates the branch instructions in a list with a link to the instruction at their
 * destination's address, using a branches hashtable
 * \param insn_list The instruction list whose branches we want to update
 * \param branches A hashtable containing the branch instructions in the list, indexed by their destination addresses
 * */
extern void insnlist_linkbranches(queue_t* insn_list, hashtable_t* branches);

/**
 * Resets all addresses in an instruction list to SIGNED_ERROR
 * \param insn_list The list of instructions to update
 * \param start Element in the queue at which to start the reset (NULL if must be its head)
 * \param stop Element in the queue at which to stop the reset (NULL if it must be its tail)
 * \warning The function insnlist_upd_addresses will have to be used afterwards on this list
 * in order for it to have coherent information
 */
extern void insnlist_reset_addresses(queue_t* insn_list, list_t* start,
      list_t* stop);

/**
 * Updates all addresses in an instructions list (if necessary) based on
 * the length of their coding and the address of the first instruction
 * \param insn_list The list of instructions to update
 * \param startaddr The address of the first instruction in the list
 * \param start Element in the queue at which to start the update (NULL if must be its head)
 * \param stop Element in the queue at which to stop the update (NULL if it must be its tail)
 * \return The number of instructions whose address was modified
 */
extern unsigned int insnlist_upd_addresses(queue_t* insn_list,
      maddr_t startaddr, list_t* start, list_t* stop);

/**
 * Updates a register oprnd in an instruction, including its coding if the driver
 * oprnd is not NULL. The function displays an error message and exits if the oprnd
 * is not found or is not a register
 * \param insn The instruction to update
 * \param reg The new register
 * \param oprndidx The index of the oprnd to update (starting at 0)
 * \param updinsncoding Pointer to the function to use for updating the instruction's coding
 * \param updopcd If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 */
extern void insn_upd_reg_oprnd(insn_t* insn, reg_t* reg, int oprndidx,
      void*(*updinsncoding)(insn_t*, int, int64_t*), int updopcd);

/**
 * Updates a register operand in an instruction, including its coding if the \e updinsncoding parameter
 * is not NULL. The function displays an error message and exits if the operand
 * is not found or is not a immediate
 * \param insn The instruction to update
 * \param immval The new value of the immediate
 * \param oprndidx The index of the operand to update (starting at 0)
 * \param updinsncoding Pointer to the function to use for updating the instruction's coding
 * \param updopcd If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 */
extern void insn_upd_imm_oprnd(insn_t* insn, imm_t immval, int oprndidx,
      void*(*updinsncoding)(insn_t*, int, int64_t*), int updopcd);

/**
 * Updates a memory operand in an instruction, including its coding if the \e updinsncoding parameter
 * is not NULL. The function displays an error message and exits if the operand
 * is not found or is not a memory address
 * \param insn The instruction to update
 * \param newmem A pointer to the structure holding the new memory operand
 * \param oprndidx The index of the oprnd to update (starting at 0)
 * \param updinsncoding Pointer to the function to use for updating the instruction's coding
 * \param updopcd If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 */
extern void insn_upd_mem_oprnd(insn_t* insn, oprnd_t* newmem, int oprndidx,
      void*(*updinsncoding)(insn_t*, int, int64_t*), int updopcd);

/**
 * Updates the address and offset in an operand of type pointer or memory relative.
 * This function performs the following operations for a pointer_t inside the oprnd_t:
 * - If it is has a target, its address will be updated to that of the target
 * - If it is of type POINTER_RELATIVE and does not have a target, its address will be updated based on the address of insn_t and the offset,
 *   otherwise its offset will be updated based on its address and the address of insn_t.
 * This function invokes the architecture-specific function retrieved from the instruction architecture
 * \param insn The instruction containing the operand of type pointer
 * \param oprnd The operand. It is assumed to belong to the instruction, no additional test will be performed
 * */
extern void insn_oprnd_updptr(insn_t* insn, oprnd_t* ptroprnd);

/**
 * Compares two insn_t structures based on the address referenced by a pointer they contain. This function is intended to be used with qsort.
 * \param i1 First instruction
 * \param i2 Second instruction
 * \return -1 if the address referenced by the pointer contained in i1 is less than the address referenced by p2, 0 if they are equal and 1 otherwise
 * */
extern int insn_cmpptraddr_qsort(const void* i1, const void* i2);

/**
 * Updates all offsets of branch instructions in an instruction list, including their coding
 * if the \e updinsncoding parameter is not NULL
 * \param insn_list The instruction list
 * \param updinsncoding Pointer to the function to use for updating the instruction's coding
 * \param updopcd If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 * \param driver The architecture specific driver for the instruction list
 * \param asmfile The asmfile where the instructions are stored
 * \param start Element in the queue at which to start the update (NULL if must be its head)
 * \param stop Element in the queue at which to stop the update (NULL if it must be its tail)
 */
extern void insnlist_upd_branchaddr(queue_t* insn_list,
      int (*updinsncoding)(insn_t*, void*, int, int64_t*), int updopcd,
      void* driver, asmfile_t* asmfile, list_t* start, list_t* stop);

/**
 * Updates the coding of instructions in an instruction list, based on the assembler function from the \e updinsncoding parameter
 * \param insn_list The instruction list
 * \param updinsncoding Pointer to the function to use for updating the instruction's coding
 * \param updopcd If set to 1, will try to update the whole coding of the instruction including
 * the opcode. Otherwise will only try to update the operands (and display an warning if an error
 * is encountered)
 * \param start Element in the queue at which to start the update (NULL if must be its head)
 * \param stop Element in the queue at which to stop the update (NULL if it must be its tail)
 */
extern void insnlist_upd_coding(queue_t* insn_list,
      void*(*updinsncoding)(insn_t*, int, int64_t*), int updopcd, list_t* start,
      list_t* stop);

/**
 * Returns the entire coding of the instruction list in the form of a character string
 * \param insn_list The instruction list
 * \param size Pointer to the length of the returned array
 * \param start Element in the queue at which to start the retrieval (NULL if must be its head)
 * \param stop Element in the queue at which to stop the retrieval (NULL if it must be its tail)
 * \return The coding of the list in the form of an array of characters
 */
extern unsigned char* insnlist_getcoding(queue_t* insn_list, int* size,
      list_t* start, list_t* stop);

/**
 * Parses a list of instructions. The instructions must be separated by '\n' characters and use a syntax compatible with the architecture
 * Labels must be a chain of characters beginning with a letter, dot ('.'), or underscore ('_'), followed by a colon (':').
 * \param insn_list List of instructions separated by '\n' characters
 * \param arch The architecture the instructions to parse belong to
 * \return Queue of pointers to insn_t structures
 * */
extern queue_t* insnlist_parse(char* insn_list, arch_t* arch);

/**
 * Gets instruction source line
 * \param insn an instruction
 * \return instruction source line or 0 if not available
 */
extern unsigned int insn_get_src_line(insn_t* insn);

/**
 * Gets instruction source column
 * \param insn an instruction
 * \return instruction source column or 0 if not available
 */
extern unsigned int insn_get_src_col(insn_t* insn);

/**
 * Gets instruction source file
 * \param insn an instruction
 * \return instruction source file or NULL if not available
 */
extern char* insn_get_src_file(insn_t* insn);

/**
 * Set the global variable display_assembling_error value
 * \param value TRUE (default, display error messages) or
 * FALSE (do not display error messages)
 */
extern void insn_set_display_assembling_error(char value);

//////////////////////////////////////////////////////////////////////////////
//                                  data                                    //
//////////////////////////////////////////////////////////////////////////////
/**
 * \enum datatype_t
 * Contains the identifiers of the type of data contained in a data_t structure
 * \warning If the number of elements reaches 32, change the size of the \c type member in \ref data_s
 * */
typedef enum datatype_e {
   DATA_UNKNOWN = 0,/**<Data of unknown type, used for errors*/
   DATA_RAW, /**<Data type not defined*/
   DATA_PTR, /**<Data is a pointer to another element in the file*/
   DATA_STR, /**<Data is a string*/
   DATA_LBL, /**<Data is a \ref label_t structure*/
   DATA_REL, /**<Data is a \ref binrel_t structure*/
   DATA_VAL, /**<Data is a numerical value*/
   DATA_NIL, /**<Data represents a data of type NULL or a data with no value but a given size*/
   DATA_LAST_TYPE /**<Max number of data types, must always be last in this enum*/
} datatype_t;
/**
 * \enum data_reftypes
 * Contains the identifiers of the type of reference used to locate a data_t structure
 * \warning If the number of elements reaches 4, change the size of the \c reftype member in \ref data_s
 * */
enum data_reftypes {
   DATAREF_LABEL = 0, /**<Data is referenced by a label*/
   DATAREF_BSCN, /**<Data is referenced by a binary section*/
   DATAREF_MAX /**<Max number, must always be last. Beware of the size of the \ref reftype member in \ref data_t if adding more types*/
};
/**
 * \struct data_s
 * \brief Describes a data element
 * */
struct data_s {
   int64_t address; /**<Address of the element*/
   uint64_t size; /**<Size in bytes of the element*/
   union {
      label_t* label; /**<Label associated to the element*/
      binscn_t* section;/**<Section where the element is located, if no label from this section could be found before*/
   } reference; /**<Reference to find the data in the file to which it belongs*/
   void* data; /**<Pointer to the data contained in the element*/
   unsigned type :5; /**<Type of data contained in the element*/
   unsigned reftype :2; /**<Type of the reference for locating the data in the file*/
   unsigned local :1; /**<Set to TRUE if the data is allocated with the data_t structure and must be freed with it*/
};

/**
 * Creates a new data_t structure from an existing reference.
 * \param type Type of data contained in the data_t structure.
 * \param data Reference to the data contained in the structure (can be NULL).
 * This data is considered to be already allocated and will not be freed when freeing the data_t structure.
 * \param size Size in bytes of the data contained in the structure
 * \return A pointer to the newly allocated structure
 * */
extern data_t* data_new(datatype_t type, void* data, uint64_t size);

/**
 * Creates a new data_t structure containing data of undefined type
 * \param size Size in bytes of the data contained in the structure
 * \param data Pointer to the data contained in the structure. If not NULL, its value will be copied to the data_t structure
 * \return A pointer to the newly allocated structure
 * */
extern data_t* data_new_raw(uint64_t size, void* data);

/**
 * Creates a new data_t structure containing a pointer to another element
 * \param size Size in bytes of the data contained in the structure
 * \param address Address of the pointer target
 * \param next Element targeted by the pointer (NULL if unknown)
 * \param type Type of the pointer value (using the constants defined in \ref pointer_type)
 * \param target_type Type of the pointer target (using the constants defined in \ref target_type)
 * \return A pointer to the newly allocated structure
 * */
extern data_t* data_new_ptr(uint64_t size, int64_t address, int64_t offset,
      void* next, pointer_type_t type, target_type_t target_type);

/**
 * Changes a data_t structure of type DATA_RAW or DATA_NIL to contain a pointer to another element
 * \param data An existing data_t structure of type DATA_RAW or DATA_NIL
 * \param size Size in bytes of the data contained in the structure
 * \param address Address of the pointer target
 * \param next Element targeted by the pointer (NULL if unknown)
 * \param type Type of the pointer value (using the constants defined in \ref pointer_type)
 * \param target_type Type of the pointer target (using the constants defined in \ref target_type)
 * */
extern void data_upd_type_to_ptr(data_t* data, uint64_t size, int64_t address,
      int64_t offset, void* next, pointer_type_t type,
      target_type_t target_type);

/**
 * Creates a new data_t structure containing a string.
 * \param string The string contained in the data. It will be duplicated into the structure
 * \return A pointer to the newly allocated structure
 * */
extern data_t* data_new_str(char* string);

/**
 * Creates a new data_t structure containing numerical data
 * \param size Size in bytes of the data contained in the structure
 * \param value Value of the data
 * \return A pointer to the newly allocated structure
 * */
extern data_t* data_new_imm(uint64_t size, int64_t value);

/**
 * Duplicates a data object.
 * \param data The data object to copy
 * \return A copy of the data object, or NULL if \c data is NULL
 * */
extern data_t* data_copy(data_t* data);

/**
 * Frees a data structure
 * \param d Data structure to free
 * */
extern void data_free(void* d);

/**
 * Returns a pointer contained in a data_t structure
 * \param data The structure representing a data element
 * \return The pointer_t structure it contains, or PTR_ERROR if \c data is NULL or does not contain a pointer
 * */
extern pointer_t* data_get_pointer(data_t* data);

/**
 * Returns the size of the data inside a data_t structure
 * \param data The structure representing a data element
 * \return The size of the data in the structure, or 0 if \c data is NULL
 * */
extern uint64_t data_get_size(data_t* data);

/**
 * Returns the address of the data inside a data_t structure
 * \param data The structure representing a data element
 * \return The address of the data in the structure, or ADDRESS_ERROR if \c data is NULL
 * */
extern int64_t data_get_addr(data_t* data);

/**
 * Returns the end address of the data inside a data_t structure
 * \param data The structure representing a data element
 * \return The address at which the data in the structure ends, or ADDRESS_ERROR if \c data is NULL
 * */
extern int64_t data_get_end_addr(data_t* data);

/**
 * Returns a string contained in a data_t structure
 * \param data The structure representing a data element
 * \return A pointer to the string it contains, or PTR_ERROR if \c data is NULL or does not contain a string
 * */
extern char* data_get_string(data_t* data);

/**
 * Retrieves the label associated to a data structure
 * \param data The structure representing a data element
 * \param label The label associated to the address at which the data element is located or the latest label encountered in this section,
 * or NULL if \c data is NULL or not associated to a label
 * */
extern label_t* data_get_label(data_t* data);

/**
 * Retrieves a binary section associated to a data structure
 * \param data The structure representing a data element
 * \param label The binary section where the data element is located, retrieved either directly or from its label, or NULL if \c data is NULL
 * */
extern binscn_t* data_get_section(data_t* data);

/**
 * Returns the type of the data inside a data_t structure
 * \param data The structure representing a data element
 * \return The type of the data in the structure, or DATA_UNKNOWN if \c data is NULL
 * */
extern unsigned int data_get_type(data_t* data);

/**
 * Returns a label contained in a data_t structure
 * \param data The structure representing a data element
 * \return The label_t structure it contains, or PTR_ERROR if \c data is NULL or does not contain a label
 * */
extern label_t* data_get_data_label(data_t* data);

/**
 * Returns a relocation contained in a data_t structure
 * \param data The structure representing a data element
 * \return The binrel_t structure it contains, or PTR_ERROR if \c data is NULL or does not contain a relocation
 * */
extern binrel_t* data_get_binrel(data_t* data);

/**
 * Returns the raw content contained in a data_t structure
 * \param data The structure representing a data element
 * \return A pointer to its content, or PTR_ERROR if \c data is NULL.
 * */
extern void* data_get_raw(data_t* data);

/**
 * Returns the reference pointer contained in a data entry of type relocation or pointer
 * \param data Structure representing a data element
 * \return Pointer contained in data if data is not NULL and of type DATA_PTR or DATA_REL, NULL otherwise
 * */
extern pointer_t* data_get_ref_ptr(data_t* data);

/**
 * Associates a label to a data structure
 * \param data The structure representing a data element
 * \param label The label associated to the address at which the data element is located
 * */
extern void data_set_label(data_t* data, label_t* label);

/**
 * Associates a binary section to a data structure
 * \param data The structure representing a data element
 * \param label The binary section where the data element is located
 * */
extern void data_set_scn(data_t* data, binscn_t* scn);

/**
 * Sets the address of a data structure
 * \param data The structure representing a data element
 * \param addr The address where the element is located
 * */
extern void data_set_addr(data_t* data, int64_t addr);

/**
 * Sets the size of a data structure
 * \param data The structure representing a data element
 * \param size The size of the element
 * */
extern void data_set_size(data_t* data, uint64_t size);

/**
 * Sets the type of a data structure
 * \param data The structure representing a data element
 * \param size The type of the element
 * */
extern void data_set_type(data_t* data, datatype_t type);

/**
 * Sets the content of a data structure. Only possible if the data is not local to the structure
 * \param data The structure representing a data element
 * \param raw The data to set into the element
 * \param type Type of the data
 * */
extern void data_set_content(data_t* data, void* raw, datatype_t type);

/**
 * Points the label of the data to the given label structure
 * Also updates the label to set it to point to the data if the label and the data
 * have the same address
 * \param in The instruction to update
 * \param fctlbl The structure containing the details about the label
 */
extern void data_link_label(data_t* data, label_t* label);

/**
 * Compares the address of a data structure with a given address. This function is intended to be used by bsearch
 * \param addr Pointer to the value of the address to compare
 * \param data Pointer to the data structure
 * \return -1 if address of data is less than address, 0 if both are equal, 1 if address of data is higher than the address
 * */
extern int data_cmp_by_addr_bsearch(const void* address, const void* data);

/**
 * Compares two data_t structures based on the address referenced by a pointer they contain. This function is intended to be used with qsort.
 * \param d1 First data
 * \param d2 Second data
 * \return -1 if the address referenced by the pointer contained in i1 is less than the address referenced by p2, 0 if they are equal and 1 otherwise
 * */
extern int data_cmp_by_ptr_addr_qsort(const void* d1, const void* d2);

/**
 * Prints the content of a data structure to a stream
 * \param data The data structure
 * \param stream The stream
 * */
extern void data_fprint(data_t* data, FILE* stream);

/**
 * Retrieves the content of a data_t structure as a string of bytes
 * \param data A data_t structure
 * \return String of bytes corresponding to the content of the structure.
 * The size of the string is the same as the size of the data structure.
 * Returns NULL if \c data is NULL or of type DATA_LBL or DATA_REL (they require format-specific handling) or DATA_NIL (by design)
 * */
extern unsigned char* data_to_bytes(data_t* data);
///////////////////////////////////////////////////////////////////////////////
//                                  block                                    //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct block_s
 * \brief Describes a basic block
 */
struct block_s {
   unsigned int id; /**<local identifier. usage unknown*/
   unsigned int global_id; /**<global identifier. Unique for the project*/
   list_t* begin_sequence; /**<First instruction of the block*/
   list_t* end_sequence; /**<Last instruction of the block*/
   fct_t *function; /**<function the block belongs to*/
   loop_t *loop; /**<loop the block belongs to (can be null)*/
   graph_node_t *cfg_node; /**<node of this block in CFG*/
   tree_t *domination_node; /**<node of this block in domination tree*/
   tree_t *postdom_node; /**<node of this block in post domination tree*/
   char is_loop_exit; /**<1 if this block is a loop exit, else 0*/
   char is_padding; /**<1 if this block is a padding block, else 0 (-1 if not analyzed)*/
};

/**
 * Create a new empty block
 * \param fct The function the block belongs to
 * \param insn First instruction in the block
 * \return a new block
 */
extern block_t* block_new(fct_t* fct, insn_t* insn);

/**
 * Delete an existing block
 * \param p a pointer on a block to free
 */
extern void block_free(void* p);

/**
 * Traverses all instructions of a basic block
 * \param B a block
 * \param it name of an iterator (not already existing)
 */
#define FOREACH_INSN_INBLOCK(B,it) list_t* it=B->begin_sequence; \
   for (;it != NULL && ((insn_t*)it->data)->block == B; it = it->next)

/**
 * Retrieves the cell containing first instruction in instructions sequence
 * \param block a block
 * \return first instruction cell
 */
extern list_t* block_get_begin_sequence(block_t* block);

/**
 * Retrieves the cell containing last instruction in instructions sequence
 * \param block a block
 * \return last instruction cell
 */
extern list_t* block_get_end_sequence(block_t* block);

/**
 * Retrieves block instructions
 * \param block a block
 * \return list of instructions
 */
extern list_t * block_get_insns(block_t* block);

/**
 * Retrieves a block id uniq for each block
 * \param block a block
 * \return block id, or SIGNED_ERROR is block is NULL
 */
extern unsigned int block_get_id(block_t* block);

/**
 * Retrieves the first instruction of a block
 * \param block a block
 * \return the first instruction of b, or PTR_ERROR if block is NULL
 */
extern insn_t* block_get_first_insn(block_t* block);

/**
 * Retrieves the address of the first instruction of a block
 * \param block a block
 * \return the address of the first instruction of b, or SIGNED_ERROR if block is NULL
 */
extern maddr_t block_get_first_insn_addr(block_t* block);

/**
 * Retrieves the last instruction of a block
 * \param block a block
 * \return the last instruction of b, or PTR_ERROR if block is NULL
 */
extern insn_t* block_get_last_insn(block_t* block);

/**
 * Retrieves the address of the last instruction of a block
 * \param b a block
 * \return the address of the last instruction of b, or SIGNED_ERROR if b is NULL
 */
extern int64_t block_get_last_insn_addr(block_t* b);

/**
 * Retrieves the function the block belongs to
 * \param block a block
 * \return the function which contains block or PTR_ERROR if block is NULL
 */
extern fct_t* block_get_fct(block_t* block);

/**
 * Retrieves the innmermost loop the block belongs to
 * \param block a block
 * \return the innermost loop which contains block or PTR_ERROR if block is NULL
 */
extern loop_t* block_get_loop(block_t* block);

/**
 * Retrieves the asmfile the block belongs to
 * \param block a block
 * \return the asmfile which contains block or PTR_ERROR if block is NULL
 */
extern asmfile_t* block_get_asmfile(block_t* block);

/**
 * Retrieves the project the block belongs to
 * \param block a block
 * \return the project which contains block or PTR_ERROR if block is NULL
 */
extern project_t* block_get_project(block_t* block);

/**
 * Retrieves the block CFG node
 * \param block a block
 * \return The block CFG node or PTR_ERROR if block is NULL
 */
extern graph_node_t* block_get_CFG_node(block_t* block);

/**
 * Returns predecessors (in the CFG) of a block
 * \param b a block
 * \return queue of blocks or PTR_ERROR if no CFG node
 */
extern queue_t * block_get_predecessors(block_t* b);

/**
 * Returns successors (in the CFG) of a block
 * \param b a block
 * \return queue of blocks or PTR_ERROR if no CFG node
 */
extern queue_t * block_get_successors(block_t* b);

/**
 * Retrieves the block dominance tree node
 * \param block a block
 * \return The dominance tree node or PTR_ERROR if block is NULL
 */
extern tree_t* block_get_domination_node(block_t* block);

/**
 * Retrieves the block post dominance tree node
 * \param block a block
 * \return The post dominance tree node or PTR_ERROR if block is NULL
 */
extern tree_t* block_get_postdom_node(block_t* block);

/**
 * Check if a block is a function exit
 * \param block a block
 * \return TRUE if block is a function exit, else FALSE
 */
extern boolean_t block_is_function_exit(block_t* block);

/**
 * Check if a block is a loop entry
 * \param block a block
 * \return TRUE if block is a loop entry, else FALSE
 */
extern char block_is_loop_entry(block_t* block);

/**
 * Check if a block is a loop exit
 * \param block a block
 * \return TRUE if block is a loop exit, else FALSE
 */
extern char block_is_loop_exit(block_t* block);

/**
 * Checks if a block is virtual (= it has no instructions)
 * \param block a block to test
 * \return TRUE if the block is virtual, else FALSE
 */
extern int block_is_virtual(block_t* block);

/**
 * \brief tells if a given block is an entry of a given loop
 * \param block The potential entry
 * \param loop The loop that contains the list of entries of the test
 * \return TRUE if the block an entry, FALSE if not
 */
extern int block_is_entry_of_loop(block_t *block, loop_t *loop);

/**
 * \brief tells if a given block is an exit of a given loop
 * \param block The potential exit
 * \param loop The loop that contains the list of exits of the test
 * \return TRUE if the block an exit, FALSE if not
 */
extern int block_is_exit_of_loop(block_t *block, loop_t *loop);

/**
 * Gets the number of instructions in a block
 * \param block a block
 * \return The number of instructions in block
 */
extern int block_get_size(block_t* block);

/**
 * Retrieves the parent node associated to block in the dominant tree
 * \param block a block
 * \return A parent node (tree_t*) or PTR_ERROR if block or block_get_domination_node (block) is NULL
 */
extern tree_t* block_get_dominant_parent(block_t* block);

/**
 * Retrieves the children node associated to block in the dominant tree
 * \param block a block
 * \return A children node (tree_t*) or PTR_ERROR if block or block_get_domination_node (block) is NULL
 */
extern tree_t* block_get_dominant_children(block_t* block);

/**
 * Retrieves the parent node associated to block in the post dominant tree
 * \param block a block
 * \return a parent node (tree_t*) or PTR_ERROR if block or block_get_postdom_node (block) is NULL
 */
extern tree_t* block_get_post_dominant_parent(block_t* block);

/**
 * Retrieves the children node associated to block in the post dominant tree
 * \param block a block
 * \return a children node (tree_t*) or PTR_ERROR if block or block_get_postdom_node (block) is NULL
 */
extern tree_t* block_get_post_dominant_children(block_t* block);

/**
 * Check if a bloc contains padding: only NOP, MOV and LEA, with the same register (ex: MOV EAX,EAX     LEA 0x0(RAX),RAX)
 * \param block a basic block
 * \return 1 if it is padding, else 0
 */
extern int block_is_padding(block_t* block);

/**
 * Checks if the block is a natural exit block
 * \param block a basic block
 * \return TRUE if block is a natural exit, else FALSE
 */
extern int block_is_natural_exit(block_t* block);

/**
 * Checks if the block is a potential exit block
 * \param block a basic block
 * \return TRUE if block is a potential exit, else FALSE
 */
extern int block_is_potential_exit(block_t* block);

/**
 * Checks if the block is a early exit block
 * \param block a basic block
 * \return TRUE if block is a early exit, else FALSE
 */
extern int block_is_early_exit(block_t* block);

/**
 * Checks if the block is a handler exit block
 * \param block a basic block
 * \return TRUE if block is a handler exit, else FALSE
 */
extern int block_is_handler_exit(block_t* block);

/**
 * Recursively checks if parent dominates child
 * \param parent a basic bloc
 * \param child a basic bloc
 * \return TRUE if parent dominates child, else FALSE
 */
extern int block_is_dominated(block_t* parent, block_t* child);

/**
 * Returns path to the source file defining a block
 * \warning for some blocks instructions are defined in different files (according to insn_get_src_file)
 * In that case reference file is the one defined in the first instruction
 * \param block a block
 * \return path to source file (string)
 */
extern char *block_get_src_file_path (block_t *block);

/**
 * Provides first and last source line of a block
 * \see block_get_src_file_path
 * \param block a block
 * \param min pointer to first line rank
 * \param max pointer to last line rank
 */
extern void block_get_src_lines (block_t *block, unsigned *min, unsigned *max);

/**
 * Returns source regions for a set of blocks as a queue of strings.
 * Source regions are formatted as "<file>: <start_line>-<end_line>"
 * \param blocks queue of blocks
 * \return queue of strings
 */
extern queue_t *blocks_get_src_regions (queue_t *blocks);

/**
 * Returns source regions for a loop
 * \see blocks_get_src_regions
 */
extern queue_t *block_get_src_regions (block_t *block);

/**
 * Sets a block unique id
 * \param block a block
 * \param global_id The new id for b
 * \warning USE WITH CAUTION
 */
extern void block_set_id(block_t* block, unsigned int global_id);

/**
 * Compares two blocks based on the addresses of their first instruction
 * \param block1 First block
 * \param block2 Second block
 * \return positive integer if the address of the first instruction of block1 is higher than the one of block2,
 * negative if it is lower, and 0 if both are equal
 */
extern int block_cmpbyaddr_qsort(const void* block1, const void* block2);

///////////////////////////////////////////////////////////////////////////////
//                                 groups                                    //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct group_elem_s
 * \brief Structure used to save one element of one group.
 */
struct group_elem_s {
   insn_t* insn; /**<A pointer on the instruction*/
   int pos_param; /**<position of the memory parameter in the parameter array*/
   char code; /**<L (load) / S (store)*/
};

/**
 * Creates a new group element
 * \param code Used to know if it is a load or a store: {GRP_LOAD, GRP_STORE}
 * \param insn The instruction represented by this group
 * \param pos_param Position of the memory parameter in the instruction
 * \return An initialized group element
 */
extern group_elem_t* group_data_new(char code, insn_t* insn, int pos_param);

/**
 * Frees a group element
 * \param group_elem A group element to free
 */
extern void group_data_free(group_elem_t* group_elem);

/**
 * Duplicates a group element
 * \param src A group element to duplicate
 * \return A copy of the input group element
 */
extern group_elem_t* group_data_dup(group_elem_t* src);

/**
 * \struct group_s
 * \brief Structure used to save a group for a loop.
 */
struct group_s {
   char* key; /**<Contains the address representation*/
   loop_t* loop; /**<Loop the group belongs to*/
   queue_t* gdat; /**<A list of element, each one representing one element of the group*/
   int (*filter_fct)(struct group_elem_s*, void*); /**<Function used to filter output. Return 1 if to print, else 0*/
   char s_status; /**<Stride computation status. Use flags defined begining with SS_*/
   int stride; /**<Stride computation result.*/
   char m_status; /**<Memory loaded computation status. Use flags defined begining with MS_.*/
   int memory_all; /**<Number of bytes loaded by the group.*/
   int memory_nover; /**<Number of bytes loaded by the group, removing overlaped bytes.*/
   int memory_overl; /**<Number of overlaped bytes loaded by the group.*/

   int span; /**<Maximum distance between two accessed bytes into a group*/
   int head; /**<Number of bytes loaded by the group from an iteration to another*/
   int unroll_factor; /**<unroll factor of the group*/
   queue_t* stride_insns; /**<A queue containing instructions used to compute the stride*/

   int* touched_sets;/**<Sets of touched bytes. Each set has 2 elements. touched[2 * n] is the first
    element of the set, and touched[2 * n + 1] is the last element, with n between
    0 and nb_sets.*/
   int nb_sets; /**<number of elements in touched_sets*/
};

/**
 * Creates a new group
 * \param key a string containing the accessed virtual representation, used as key
 * \param loop the loop the group belongs to
 * \param filter_fct a function used during group printing to know if an element must be printed. Takes as
 * input:
 *    * an element of the group
 *    * a user value
 *    * return 1 if the element must be printed, else 0
 * \return a new initialized group structure
 */
extern group_t* group_new(char* key, loop_t* loop,
      int (*filter_fct)(struct group_elem_s*, void*));

/**
 * Duplicates an existing group
 * \param src A group to duplicate
 * \return A copy of the group
 */
extern group_t* group_dup(group_t* src);

/**
 * Adds an element in a group
 * \param group An existing group
 * \param group_elem An existing group element
 */
extern void group_add_elem(group_t* group, group_elem_t* group_elem);

/**
 * Frees a group and all elements it contains
 * \param p A group to free
 */
extern void group_free(void* p);

/**
 * Prints a group
 * \param group An existing group
 * \param f_out A file where to print
 * \param user A user value given to the filter function
 * \param format format used to display groups
 */
extern void group_print(group_t* group, FILE* f_out, void* user, int format);

/**
 * Compares two groups
 * \param group1 An existing group
 * \param group2 An existing group
 * \return 1 if there is a difference, else 0
 */
extern int group_cmp(group_t* group1, group_t* group2);

/**
 * Gets the size of the group, using the filter function
 * \param group A group
 * \param user A user data given to the filter function
 * \return The number of group element not filtered by the filter function
 */
extern int group_get_size(group_t* group, void* user);

/**
 * Gets the pattern of the group, using the filter function
 * \param group A group
 * \param user A user data given to the filter function
 * \return The pattern, composed of group element not filtered by the filter function
 */
extern char* group_get_pattern(group_t* group, void* user);

/**
 * Gets the pattern element at index n
 * \param group A group
 * \param n Index of the pattern element to retrieve
 * \param user A user data given to the filter function
 * \return GRP_LOAD ('L') or GRP_STORE ('S') if no problem, else 0
 */
extern char group_get_pattern_n(group_t* group, int n, void* user);

/**
 * Gets the instruction at index n in the group
 * \param group A group
 * \param n Index of the instruction to retrieve
 * \param user A user data given to the filter function
 * \return The instruction, else NULL if there are errors
 */
extern insn_t* group_get_insn_n(group_t* group, int n, void* user);

/**
 * Gets the instruction address at index n in the group
 * \param group A group
 * \param n Index of the instruction to retrieve
 * \param user A user data given to the filter function
 * \return The instruction address, else 0 if there are errors
 */
extern maddr_t group_get_address_n(group_t* group, int n, void* user);

/**
 * Gets the instruction opcode at index n in the group
 * \param group A group
 * \param n Index of the instruction to retrieve
 * \param user A user data given to the filter function
 * \return The instruction opcode, else NULL if there are errors
 */
extern char* group_get_opcode_n(group_t* group, int n, void* user);

/**
 * Gets the instruction offset at index n in the group
 * \param group A group
 * \param n Index of the instruction to retrieve
 * \param user A user data given to the filter function
 * \return The instruction offset, else 0 if there are errors
 */
extern int64_t group_get_offset_n(group_t* group, int n, void* user);

/**
 * Gets the innermost loop a group belongs to
 * \param group A group
 * \return The loop, else NULL if there are errors
 */
extern loop_t* group_get_loop(group_t* group);

/**
 * Gets the group span
 * \param group a group
 * \return the group span
 */
extern int group_get_span(group_t* group);

/**
 * Gets the group head
 * \param group a group
 * \return the group head
 */
extern int group_get_head(group_t* group);

/**
 * Gets the group increment
 * \param group a group
 * \return the group increment
 */
extern int group_get_increment(group_t* group);

/*
 *
 *
 */
extern char* group_get_stride_status(group_t* group);
extern char* group_get_memory_status(group_t* group);
extern int group_get_accessed_memory(group_t* group);
extern int group_get_accessed_memory_nooverlap(group_t* group);
extern int group_get_accessed_memory_overlap(group_t* group);
extern int group_get_unroll_factor(group_t* group);

/**
 * Extract a subgroup from a group according to a filter
 * \param group a group
 * \param mode a value for a filter
 *       - 0: keep elements which use vectorial registers
 *       - 1: keep elements which use non-vectorial registers
 * \return a new group to free when it is not used anymore
 */
extern group_t* group_filter(group_t* group, int mode);

///////////////////////////////////////////////////////////////////////////////
//                                   loop                                    //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct loop_s
 * \brief Structure describing a loop.
 */
struct loop_s {
   unsigned int id; /**<local identifier. usage unknown*/
   unsigned int global_id; /**<global identifier. Uniq for the project*/
   list_t *entries; /**<list of all entries of the loop*/
   list_t *exits; /**<list of all exits of the loop*/
   queue_t *blocks; /**<list of all basic blocks of the loop*/
   fct_t *function; /**<function the loop belongs to*/
   queue_t *paths; /**<queue of the lists of the different possible paths in the loop*/
   tree_t *hierarchy_node; /**<The node corresponding to the loop in the loopnest forest*/
   list_t* groups; /**<A list of group_t* structures containing groups for this loop*/
   int nb_insns; /**<Number of instructions in the loop*/
};

/**
 * Create a new empty loop
 * \param entry an entry block
 * \return a new loop
 */
extern loop_t* loop_new(block_t *entry);

/**
 * Delete an existing loop and all data it contains
 * \param p a pointer on a loop to free
 */
extern void loop_free(void* p);

/**
 * Retrieves a uniq loop id
 * \param loop a loop
 * \return loop id or 0 if loop is NULL
 */
extern unsigned int loop_get_id(loop_t* loop);

/**
 * Retrieves a list of all blocks which are loop entries
 * \param loop a loop
 * \return a list of entries or PTR_ERROR if loop is NULL
 */
extern list_t* loop_get_entries(loop_t* loop);

/**
 * Retrieves a list of all blocks which are loop exits
 * \param loop a loop
 * \return a list of exits or PTR_ERROR if loop is NULL
 */
extern list_t* loop_get_exits(loop_t* loop);

/**
 * Retrieves a list of all blocks which are in loop
 * \param loop a loop
 * \return a list of block or PTR_ERROR if loop is NULL
 */
extern queue_t* loop_get_blocks(loop_t* loop);

/**
 * Retrieves the number of blocks which are in loop
 * \param loop a loop
 * \return the number of blocks
 */
extern int loop_get_nb_blocks(loop_t* loop);

/**
 * Retrieves the number of blocks which are in loop, excluding virtual blocks
 * \param loop a loop
 * \return the number of blocks
 */
extern int loop_get_nb_blocks_novirtual(loop_t* loop);

/**
 * Retrieves paths of loop
 * \param loop a loop
 * \return a queue of paths or PTR_ERROR if loop is NULL
 */
extern queue_t* loop_get_paths(loop_t* loop);

/**
 * Retrieves a list of all groups which are in loop
 * \param loop a loop
 * \return a list of groups or PTR_ERROR if loop is NULL
 */
extern list_t* loop_get_groups(loop_t* loop);

/**
 * Retrieves a the function loop belongs to
 * \param loop a loop
 * \return a function or PTR_ERROR if loop is NULL
 */
extern fct_t* loop_get_fct(loop_t* loop);

/**
 * Retrieves the asmfile containing loop
 * \param loop a loop
 * \return an asmfile or PTR_ERROR if loop is NULL
 */
extern asmfile_t* loop_get_asmfile(loop_t* loop);

/**
 * Retrieves the project containing loop
 * \param loop a loop
 * \return a project or PTR_ERROR if loop is NULL
 */
extern project_t* loop_get_project(loop_t* loop);

/**
 * Retrieves a the hierarchy node associated to loop
 * \param loop a loop
 * \return a hierarchy node (tree_t*) or PTR_ERROR if loop is NULL
 */
extern tree_t* loop_get_hierarchy_node(loop_t* loop);

/**
 * Retrieves the parent node associated to loop
 * \param loop a loop
 * \return a parent node (tree_t*) or PTR_ERROR if loop or loop_get_hierarchy_node (loop) is NULL
 */
extern tree_t* loop_get_parent_node(loop_t* loop);

/**
 * Retrieves the children node associated to loop
 * \param loop a loop
 * \return a children node (tree_t*) or PTR_ERROR if loop or loop_get_hierarchy_node (loop) is NULL
 */
extern tree_t* loop_get_children_node(loop_t* loop);

/**
 * Adds a group in a loop
 * \param loop an existing loop
 * \param group an existing group
 */
extern void loop_add_group(loop_t* loop, group_t* group);

/**
 * Gets the number of instructions in a loop
 * \param loop a loop
 * \return number of instructions
 */
extern int loop_get_nb_insns(loop_t* loop);

/**
 * Checks if a loop is innermost
 * \param loop a loop
 * \return boolean or SIGNED_ERROR if failed to access to the hierarchy node
 */
extern int loop_is_innermost(loop_t *loop);

/**
 * Returns depth of a loop
 * \param loop a loop
 * \return depth or SIGNED_ERROR if failure
 */
extern int loop_get_depth(loop_t *loop);

/**
 * Returns backedges of a loop
 * A backedge E for the loop L is such as E->to is an entry block for L and E->from belongs to L
 * \param loop a loop
 * \return queue of CFG edges
 */
extern queue_t *loop_get_backedges (loop_t *loop);

/**
 * Returns backedge instructions of a loop (instructions branching to same loop)
 * \param loop a loop
 * \return queue of instructions
 */
extern queue_t *loop_get_backedge_insns (loop_t *loop);

/**
 * Returns path to the source file defining a loop
 * \warning for some loops instructions are defined in different files (according to insn_get_src_file)
 * In that case returning the one defined in the first instruction (in the first entry block)
 * \param loop a loop
 * \return path to source file (string)
 */
extern char *loop_get_src_file_path (loop_t *loop);

/**
 * Provides first and last source line of a loop
 * \see loop_get_src_file_path
 * \param loop a loop
 * \param min pointer to first line rank
 * \param max pointer to last line rank
 */
extern void loop_get_src_lines (loop_t *loop, unsigned *min, unsigned *max);

/**
 * Returns source regions for a loop
 * \see blocks_get_src_regions
 */
extern queue_t *loop_get_src_regions (loop_t *loop);

/**
 * Sets the unique id for a loop
 * \param loop a loop
 * \param global_id The new id for the loop
 * \warning USE WITH CAUTION
 */
extern void loop_set_id(loop_t* loop, unsigned int global_id);

/**
 * Macro
 * return the number of entries of the loop
 * \param L Pointer to a loop structure
 */
#define LOOP_NB_ENTRIES(L) list_length(L->entries)

/**
 * Macro
 * return the number of exits of the loop
 * \param L Pointer to a loop structure
 */
#define LOOP_NB_EXITS(L) list_length(L->exits)

/**
 * Macro
 * return the number of blocks being part of the loop
 * \param L Pointer to a loop structure
 */
#define LOOP_NB_BLOCKS(L) list_length(L->blocks)

/**
 * Macro
 * iterate over the paths in a loop
 * The path's list can be accessed from the third argument PELEM
 * \note If the loop has no exit no path will be returned
 * \param L Pointer to a loop structure
 * \param PELEM Iterator (name must be unique for the current scope)
 */
#define FOREACH_PATH_INLOOP(L,PELEM) \
   list_t *PITER; list_t *PELEM; for(PITER=queue_iterator(L->paths) , PELEM=PITER?GET_DATA(PELEM,PITER):NULL ; PITER!=NULL; PITER=PITER->next , PELEM=PITER?GET_DATA(PELEM,PITER):NULL)

///////////////////////////////////////////////////////////////////////////////
//                              functions ranges                             //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct fct_range_s
 * \brief Structure describing a range of consecutive instructions
 *        belonging to the same function.
 */
struct fct_range_s {
   insn_t* start;
   insn_t* stop;
   char type;
};

/**
 * Creates a new range from limits
 * \param start first instruction of the range
 * \param stop last instruction of the range
 * \return an initialized range
 */
extern fct_range_t* fct_range_new(insn_t* start, insn_t* stop);

/**
 * Returns the list of ranges for a function
 * \param fct a function
 * \return the list of ranges or NULL
 */
extern queue_t* fct_getranges(fct_t *fct);

/**
 * Frees an existing range
 * \param p a pointer on a fct_range_t structure
 */
extern void fct_range_free(void* p);

/**
 * Returns the first instruction of a range
 * \param range a fct_range_t structure
 * \return the first instruction of the range, or NULL
 */
extern insn_t* fct_range_getstart(fct_range_t* range);

/**
 * Returns the last instruction of a range
 * \param range a fct_range_t structure
 * \return the last instruction of the range, or NULL
 */
extern insn_t* fct_range_getstop(fct_range_t* range);

///////////////////////////////////////////////////////////////////////////////
//                                  functions                                //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct fct_s
 * \brief Structure describing a function.
 */
struct fct_s {
   unsigned int id; /**<local identifier. usage unknown*/
   unsigned int global_id; /**<global identifier. Uniq for the project*/
   label_t *namelbl; /**<Label representing the function name*/
   char *demname; /**<demangled function name (if available)*/
   queue_t *blocks; /**<a queue of all basic blocks the function contains*/
   queue_t* padding_blocks; /**<a queue of all padding blocks of the function*/
   graph_node_t *cg_node; /**<function node in the Call Graph*/
   queue_t *loops; /**<a queue of all loops the function contains*/
   queue_t *components; /**<a queue of all the connected components represented by lists of blocks*/
   asmfile_t *asmfile; /**<the binary the function belongs to*/
   insn_t *first_insn; /**<first instruction of the function*/
   insn_t *last_insn; /**<last instruction of the function (biggest address)*/
   fct_t* original_function; /**<Used to know if the current function is a "real" function (NULL) or if it
    has been extracted from another function CC (! NULL)*/
   queue_t *paths; /**<queue of the lists of the different possible paths in the function*/
   queue_t* entries; /**<List of blocks, of per entry. First elementis the main entry*/
   queue_t* exits; /**<List of exits*/
   queue_t* ranges; /**<List of fct_range_t structures*/
   int cfg_depth; /**<??*/
   int nb_insns; /**<the number of instructions*/
   block_t* virtual_exit; /**<Virtual block with uniq function exit (all exits go into it)*/
   dbg_fct_t* debug; /**<Pointer to the debug data for this function*/
   void* ssa; /**<Result of SSA computation. Should be casted in (ssa_block_t**)
    where libmcore.h is included*/
   void* polytopes; /**<Structure created by lcore_fct_analyze_polytopes. Should be casted
    in (polytope_context_t*) where libmcore.h is included*/
   char** live_registers; /**<Results of live register analysis*/
   char is_grouping_analyzed; //*<Boolean set to TRUE when lcore_fct_analyze_groups is called*/
   maddr_t dbg_addr; /**< Private*/
};

/**
 * \struct dbg_fct_s
 * \brief This structure contains debug data for functions
 */
struct dbg_fct_s {
   char* name; /**<Name of the function*/
   char* file; /**<File containing the function*/
   char* language; /**<Source language of the function*/
   char* compiler; /**<Compiler used to compile the function*/
   char* version; /**<Version of the compiler*/
   void* data; /**<Original debug data*/
   int decl_line; /**<Source line where the function is declared*/
   char lang_code; /**<Code used to represente the language*/
   char comp_code; /**<Code used to represente the compiler*/
};

/**
 * Create a new empty function
 * \param asmfile assembly file containing the function
 * \param label Label representing the name of the function (must be not null)
 * \param insn first instruction of the function
 * \return a new function, or PTR_ERROR is \e name is NULL
 */
extern fct_t* fct_new(asmfile_t* asmfile, label_t* label, insn_t* insn);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Demangles a function name
 * \param name function name to demangle
 * \param compiler can be used to specify the compiler if it is known, else must DEM_COMP_ERR
 * \param language specify the language.
 * \return a demangled structure
 */
extern char* fct_demangle(const char* name, char compiler, char language);

#ifdef __cplusplus
}
#endif

/**
 * Get the first block of a function
 * \param F A function
 */
#define FCT_ENTRY(F) ((block_t *)queue_peek_head((F)->blocks))

/**
 * Macro
 * return the number of entries of the function
 * \param L Pointer to a function structure
 */
#define FCT_NB_ENTRIES(F) queue_length(F->entries)

/**
 * Delete an existing functions and all data it contains
 * \param p A pointer on a function to free
 */
extern void fct_free(void* p);

/**
 * Delete an existing functions and all data it contains except cg_node
 * Allow dramatical improvement of asmfile_free (all CG can be destroyed at once)
 * \param p a pointer on a function to free
 */
extern void fct_free_except_cg_node(void* p);

/**
 * Retrieves the connected components
 * \param fct a function
 * \return a queue of connected components (lists of blocks) or NULL if fct is NULL
 */
extern queue_t* fct_get_components(fct_t *fct);

/**
 * Retrieves the original function (if CC of a real function)
 * \param fct a function
 * \return a function or NULL if fct is NULL
 */
extern fct_t* fct_get_original_function(fct_t* fct);

/**
 * Retrieves a uniq ID associated to a function
 * \param fct a function
 * \return a uniq ID or 0 if fct is NULL
 */
extern unsigned int fct_get_id(fct_t* fct);

/**
 * Retrieves a function name
 * \param fct a function
 * \return fct name or PTR_ERROR if fct is NULL
 */
extern char* fct_get_name(fct_t* fct);

/**
 * Retrieves the label representing a function name
 * \param fct a function
 * \return The label representing the function name or PTR_ERROR if fct is NULL
 */
extern label_t* fct_get_lblname(fct_t* fct);

/**
 * Return the source line where the function was declared
 * \param fct a function
 * \return 0 if error, else the line where the function was declared
 */
extern int fct_get_decl_line(fct_t* fct);

/**
 * Retrieves a demangled function name
 * \param fct a function
 * \return fct demangled name or PTR_ERROR if fct is NULL
 */
extern char* fct_get_demname(fct_t* fct);

/**
 * Retrieves a list of blocks which belongs to fct
 * \param fct a function
 * \return a list of fct blocks or NULL if fct is NULL
 */
extern queue_t* fct_get_blocks(fct_t* fct);

/**
 * Retrieves a list of padding blocks which belongs to fct
 * \param fct a function
 * \return a list of fct padding block or NULL if fct is NULL
 */
extern queue_t* fct_get_padding_blocks(fct_t* fct);

/**
 * Retrieves paths of fct
 * \param fct a function
 * \return a queue of paths or PTR_ERROR if fct is NULL
 */
extern queue_t* fct_get_paths(fct_t* fct);

/**
 * Retrieves a CG node associated to a function
 * \param fct a function
 * \return fct CG node or PTR_ERROR if fct is NULL
 */
extern graph_node_t* fct_get_CG_node(fct_t* fct);

/**
 * Retrieves a list of loops which belongs to fct
 * \param fct a function
 * \return a list of fct loops or NULL if fct is NULL
 */
extern queue_t* fct_get_loops(fct_t* fct);

/**
 * Retrieves an asmfile which contains fct
 * \param fct a function
 * \return an asmfile which contains fct or PTR_ERROR if fct is NULL
 */
extern asmfile_t* fct_get_asmfile(fct_t* fct);

/**
 * Retrieves a project which contains fct
 * \param fct a function
 * \return a project which contains fct or PTR_ERROR if fct is NULL
 */
extern project_t* fct_get_project(fct_t* fct);

/**
 * Macro
 * return the number of connected components of
 * the function's (F) CFG
 * \param F Pointer to a fct_t structure
 */
#define CFG_NB_CC(F)  F->components->length

/**
 * Records the heads of the connected components in a list
 * \param fct A function
 * \return A list of the first header of each component
 */
extern list_t *fct_get_CC_heads(fct_t *fct);

/**
 * Gets the number of loops for a function
 * \param fct a function
 * \return the number of loops of fct
 */
extern int fct_get_nb_loops(fct_t* fct);

/**
 * Gets the number of blocks for a function
 * \param fct a function
 * \return the number of blocks of fct
 */
extern int fct_get_nb_blocks(fct_t* fct);

/**
 * Gets the number of blocks for a function, excluding virtual blocks
 * \param fct a function
 * \return the number of blocks of fct
 */
extern int fct_get_nb_blocks_novirtual(fct_t* fct);

/**
 * Gets the number of instructions for a function
 * \param fct a function
 * \return the number of instructions of fct
 */
extern int fct_get_nb_insns(fct_t* fct);

/**
 * Retrieves the first instruction of fct
 * \param fct a function
 * \return the first instruction of fct or PTR_ERROR if fct is NULL
 */
extern insn_t *fct_get_first_insn(fct_t* fct);

/**
 * Checks whether a function has debug data
 * (allowing to call corresponding getters as fct_get_compiler...)
 * \param fct a function
 * \return TRUE or FALSE
 */
extern int fct_has_debug_data(fct_t* fct);

/**
 * Checks if a function is an external stub
 * \param fct a function
 * \return TRUE if the label containing the function name is of type LBL_EXTFUNCTION, FALSE otherwise
 * */
extern int fct_is_external_stub(fct_t* fct);

/**
 * Return the source file name of the function
 * \param fct a function
 * \return the source file name or NULL if not available
 */
extern char* fct_get_src_file(fct_t* fct);

/**
 * Return the full path (directory + name) to the source file of the function
 * \param fct a function
 * \return the source file path or NULL if directory or name not available
 */
extern char* fct_get_src_file_path(fct_t* fct);

/**
 * Provides first and last source line of a function
 * \see fct_get_src_file_path
 * \param fct a function
 * \param min pointer to first line rank
 * \param max pointer to last line rank
 */
extern void fct_get_src_lines (fct_t *fct, unsigned *min, unsigned *max);

/**
 * Returns source regions for a function
 * \see blocks_get_src_regions
 */
extern queue_t *fct_get_src_regions (fct_t *fct);

/**
 * Return the compiler used to compile the function
 * \param fct a function
 * \return the compiler name or NULL if not available
 */
extern char* fct_get_compiler(fct_t* fct);

/**
 * Return the compiler version used to compile the function
 * \param fct a function
 * \return the compiler version or NULL if not available
 */
extern char* fct_get_version(fct_t* fct);

/**
 * Return the source language of the function
 * \param fct a function
 * \return the source language or NULL if not available
 */
extern char* fct_get_language(fct_t* fct);

/**
 * Return the source language code
 * \param fct a function
 * \return a DEM_LANG_ code
 */
extern char fct_get_language_code(fct_t* fct);

/**
 * Updates all blocks local identifiers
 * \param fct a function
 */
extern void fct_upd_blocks_id(fct_t* fct);

/**
 * Updates all loops local identifiers
 * \param fct a function
 */
extern void fct_upd_loops_id(fct_t* fct);

/**
 * Returns the list of entry blocks for a function
 * \param fct a function
 * \return the list of entry blocks or NULL
 */
extern queue_t* fct_get_entry_blocks(fct_t* fct);

/**
 * Returns the list of exit blocks for a function
 * \param fct a function
 * \return the list of exit blocks or NULL
 */
extern queue_t* fct_get_exit_blocks(fct_t* fct);

/**
 * Returns the list of entry instructions for a function. The return queue
 * should be free using queue_free (<returned queue>, NULL)
 * \param fct a function
 * \return the list of entry instructions or NULL
 */
extern queue_t* fct_get_entry_insns(fct_t* fct);

/**
 * Returns the list of exit instructions for a function. The return queue
 * should be free using queue_free (<returned queue>, NULL)
 * \param fct a function
 * \return the list of exit instructions or NULL
 */
extern queue_t* fct_get_exit_insns(fct_t* fct);

/**
 * Returns the function main entry (block containing the instruction which is at
 * the function label)
 * \param fct a function
 * \return a basic block wich is the main entry or NULL if there is a problem
 */
extern block_t* fct_get_main_entry(fct_t* fct);

///////////////////////////////////////////////////////////////////////////////
//                                  label                                    //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct label_s
 * \brief Structure describing a label.
 */
struct label_s {
   char* name; /**<Name of the label*/
   maddr_t address; /**<Address of the label*/
   void* target; /**<Object pointed by the label*/
   binscn_t* scn; /**<Section in the binary file where this label is defined*/
   unsigned target_type :4; /**<Type of the target. Must be an element of the enum target_type_e*/
   unsigned type :4; /**<Type of the label. Must be an element of the enum label_type_e*/
};

/**
 * Create a new label
 * \param name name of the label
 * \param addr address where the label is in the code
 * \param type type of the object pointed by the label. Must be an element of the enum target_type
 * \param t object pointed by the label
 * \return an initialized label
 */
extern label_t* label_new(char* name, maddr_t addr, target_type_t type, void* t);

/**
 * Copies a label
 * \param label Label to copy
 * \return Copy of \c label or NULL if \c label is NULL. Its target and section will be identical to the origin
 * */
extern label_t* label_copy(label_t* label);

/**
 * Retrieves the address of a label
 * \param lbl The label
 * \return The address associated to the label
 */
extern maddr_t label_get_addr(label_t* lbl);

/**
 * Retrieves the name of a label
 * \param lbl The label
 * \return The name of the label
 */
extern char* label_get_name(label_t* lbl);

/**
 * Retrieves the object associated to a label
 * \param lbl The label
 * \return The object pointed to by the label
 */
extern void* label_get_target(label_t* lbl);

/**
 * Retrieves the type of the target of a label
 * \param lbl The label
 * \return The type of target of the label
 * */
extern target_type_t label_get_target_type(label_t* lbl);

/**
 * Retrieves the type of a label
 * \param lbl The label
 * \return The type of the label or LBL_ERROR if \c lbl is NULL
 * */
extern label_type_t label_get_type(label_t* lbl);

/**
 * Checks if a label can be used to identify a function
 * \param lbl The label
 * \return TRUE if the label is not NULL and has a type that can be associated to a function, FALSE otherwise
 * */
extern int label_is_type_function(label_t* lbl);

/**
 * Checks if a label can be used to identify a function
 * \param lbl The label
 * \return TRUE if the label is not NULL and has a type that can be associated to a function, FALSE otherwise
 * */
extern int label_is_type_function(label_t* lbl);

/**
 * Retrieves the section a label belongs to
 * \param lbl The label
 * \return The section from the binary file where the label is defined, or NULL if lbl is NULL
 * */
extern binscn_t* label_get_scn(label_t* lbl);

/**
 * Sets the type of a label
 * \param lbl The label
 * \param type The type of the label
 * */
extern void label_set_type(label_t* lbl, label_type_t type);

/**
 * Associates an instruction to a label
 * \param lbl The label
 * \param insn The instruction to associate
 * */
extern void label_set_target_to_insn(label_t* lbl, insn_t* insn);

/**
 * Associates a data entry to a label
 * \param lbl The label
 * \param data The data entry to associate
 * */
extern void label_set_target_to_data(label_t* lbl, data_t* data);

/**
 * Sets the address of a label
 * \param lbl The label
 * \param addr The address to be associated to the label
 */
extern void label_set_addr(label_t* lbl, maddr_t addr);

/**
 * Sets the section a label belongs to
 * \param lbl The label
 * \param scn The section from the binary file where the label is defined
 * */
extern void label_set_scn(label_t* lbl, binscn_t* scn);

/**
 * Updates the address of a label depending on the address of its target
 * \param lbl The label
 * */
extern void label_upd_addr(label_t* lbl);

/**
 * Compares two labels based on their addresses (for use with qsort)
 * \param a First label
 * \param b Second label
 * \return The result of the comparison between a and b by \ref label_cmp
 * */
extern int label_cmp_qsort(const void *a, const void *b);

/**
 * Compares the address of a label structure with a given address. This function is intended to be used by bsearch
 * \param addr Pointer to the value of the address to compare
 * \param label Pointer to the label structure
 * \return -1 if address of label is less than address, 0 if both are equal, 1 if address of label is higher than the address
 * */
extern int label_cmpaddr_forbsearch(const void* address, const void* label);

/**
 * Free an existing label
 * \param p a pointer on a label
 */
extern void label_free(void* p);

///////////////////////////////////////////////////////////////////////////////
//                                     intervals                             //
///////////////////////////////////////////////////////////////////////////////

/**
 * \struct interval_s
 * Represents an interval of addresses characterised by its beginning address (or offset) and its size
 * */
struct interval_s {
   int64_t address; /**<Starting address of the interval*/
   uint64_t size; /**<Size of the interval*/
   void* data; /**<User-defined data associated to the interval*/
//   unsigned reach:2; /**<Flags specifying how the interval can be reached (using INTERVAL_REACHABLE_* flags)*/
//   unsigned flags:6; /**<Flags characterising the interval (can use the values from intervaltype_t enum)*/
   uint8_t flags; /**<Flags characterising the interval*/
};

/**
 * Creates a new interval
 * \param address Starting address of the interval
 * \param size Size of the interval
 * \return A new interval structure with the specified starting address and size
 * */
extern interval_t* interval_new(int64_t address, uint64_t size);

/**
 * Frees an interval structure
 * \param interval The interval structure to free
 * */
extern void interval_free(interval_t* interval);

/**
 * Gets the starting address of an interval
 * \param interval The interval structure
 * \return The starting address of the interval, or SIGNED_ERROR if \c interval is NULL
 * */
extern int64_t interval_get_addr(interval_t* interval);

/**
 * Gets the size of an interval
 * \param interval The interval structure
 * \return The size of the interval, or UNSIGNED_ERROR if \c interval is NULL
 * */
extern uint64_t interval_get_size(interval_t* interval);

/**
 * Gets the ending address of an interval
 * \param interval The interval structure
 * \return The ending address of the interval, or SIGNED_ERROR if \c interval is NULL
 * */
extern int64_t interval_get_end_addr(interval_t* interval);

/**
 * Updates the starting address of an interval. Its size will be adjusted accordingly
 * \param interval The interval structure
 * \param newaddr New starting address of the interval. If superior to the current ending address,
 * the interval size will be reduced to 0
 * */
extern void interval_upd_addr(interval_t* interval, int64_t newaddr);

/**
 * Sets the size of an interval. Its starting address will not be modified
 * \param interval The interval
 * \param newsize The new size
 * */
extern void interval_set_size(interval_t* interval, uint64_t newsize);

/**
 * Updates the ending address of an interval by changing its size
 * \param interval The interval structure
 * \param newend New ending address of the interval. If inferior to the current starting address,
 * the interval size will be reduced to 0
 * */
extern void interval_upd_end_addr(interval_t* interval, int64_t newend);

/**
 * Checks the reachable status of an interval
 * \param interval The interval structure
 * \param reachable Flag specifying the reachable status to check for
 * \return TRUE if the interval can be reached as described by \c reachable, FALSE otherwise
 * */
//extern int interval_checkreachable(interval_t* interval, uint8_t reachable);
/**
 * Adds a flag characterising the reachable status of an interval
 * \param interval The interval structure
 * \param reachable Flag specifying the reachable status to flag the interval with
 * */
//extern void interval_addreachable(interval_t* interval, uint8_t reachable);
/**
 * Sets a flag of an interval (not one about reachability)
 * \param interval The interval structure
 * \param flag Flag to set
 * */
extern void interval_set_flag(interval_t* interval, uint8_t flag);

/**
 * Retrieves a flag of an interval (not one about reachability)
 * \param interval The interval structure
 * \return flag Flag set on the interval, or 0 if \c interval is NULL
 * */
extern uint8_t interval_get_flag(interval_t* interval);

/**
 * Sets the used-defined data of an interval.
 * \param interval The interval
 * \param data Pointer to the data to link to the interval
 * */
extern void interval_set_data(interval_t* interval, void* data);

/**
 * Retrieves the user-defined data linked to the interval
 * \param interval The interval
 * \return Pointer to the data linked to the interval or NULL if \c interval is NULL
 * */
extern void* interval_get_data(interval_t* interval);

/**
 * Splits an existing interval around a given address. A new interval beginning at the address
 * of the interval and ending at the given address will be created, while the address of the existing
 * interval will be set to the given address
 * \param interval The interval to split. Its beginning address will be updated to \c splitaddr
 * \param splitaddr Address around which to split
 * \return The new interval, beginning at the original address of \c interval and ending at splitaddr, or NULL
 * if \c interval is NULL or \c splitaddr is outside of the range covered by \c interval
 * */
extern interval_t* interval_split(interval_t* interval, int64_t splitaddr);

/**
 * Appends an interval at the end of another, only if the end address of the first is equal to the address of the second
 * \param interval Interval to which another must be appended
 * \param appended The interval to append to \c interval. It will not be freed
 * \return TRUE if \c merged had the same address as the end address of \c interval and was successfully merged, FALSE otherwise
 * */
extern int interval_merge(interval_t* interval, interval_t* merged);

/**
 * Compares two intervals based on their starting address. This function is intended to be used with qsort
 * \param i1 First interval
 * \param i2 Second interval
 * \return -1 if the starting address of i1 is less than the starting address of i2, 0 if they are equal and 1 otherwise
 *
 * */
extern int interval_cmp_addr_qsort(const void *i1, const void *i2);

/**
 * Prints an interval
 * \param interval An interval to print
 * */
extern void interval_fprint(interval_t* interval, FILE* stream);

/**
 * Checks if a given interval can contain a given size with the given alignment
 * \param interval The interval
 * \param size The size to check against the size of the interval.
 * \param align Alignment over which the beginning address of the given size must be aligned. If not null,
 * it will be considered that the given size is inserted into the interval at an address satisfying addr%align = 0
 * \return The total size needed to insert the given size into the interval, starting at the address of the interval,
 * or 0 if the size can't fit inside the interval or if \c interval is NULL
 * */
extern uint64_t interval_can_contain_size(interval_t* interval, uint64_t size,
      uint64_t align);

///////////////////////////////////////////////////////////////////////////////
//                                  binrel                                   //
///////////////////////////////////////////////////////////////////////////////

/**
 * Structure representing a relocation in a binary file
 * */
struct binrel_s {
   label_t* label; /**<Label of the relocation*/
   pointer_t* ptr; /**<Pointer to the object targeted by the relocation*/
   uint32_t reltype; /**<Format-specific identifier of the relocation type*/
/**\todo (2014-10-21) So far I don't see the need to change this type, it's just that it becomes too difficult to juggle
 * with in the patcher, so we simply have to carry it around and possibly access a format/architecture specific driver to
 * use it. If at some point in the future we need more data, we *could* make this some kind of MAQAO specific type
 * (like we do for section types, for instance), and convert it when needed to the format-architecture specific types*/
};

/**
 * Returns the label associated to a relocation
 * \param rel Structure representing a relocation
 * \return The label associated to the relocation or NULL if \c rel is NULL
 * */
extern label_t* binrel_get_label(binrel_t* rel);

/**
 * Returns the pointer associated to a relocation
 * \param rel Structure representing a relocation
 * \return The pointer associated to the relocation or NULL if \c rel is NULL
 * */
extern pointer_t* binrel_get_pointer(binrel_t* rel);

/**
 * Returns the format specific type associated to a relocation
 * \param rel Structure representing a relocation
 * \return The format specific type associated to the relocation or UINT32_MAX if \c rel is NULL
 * */
extern uint32_t binrel_get_rel_type(binrel_t* rel);

/**
 * Prints a binary relocation
 * \param rel The relocation
 * \param str The string to print to
 * \param size Size of the string
 * */
extern void binrel_print(binrel_t* rel, char* str, size_t size);

///////////////////////////////////////////////////////////////////////////////
//                                  binscn                                   //
///////////////////////////////////////////////////////////////////////////////

/**
 * Structure representing a section in a binary file
 * \todo Reorder fields for size optimisation once stabilised (2014-04-17)
 * */
struct binscn_s {
   char* name; /**<Name of the section*/
   unsigned char* data; /**<Raw data contained in the section*/
   uint64_t vsize; /**<Size in bytes of the section in terms of virtual memory*/
   uint64_t size; /**<Size in bytes of the section*/
   uint64_t entrysz; /**<Size of the entries in the section. Set to 0 if the section does not contain entries with a fixed size*/
   uint64_t align; /**<Value over which the virtual address of the section must be aligned*/
   uint16_t scnid; /**<Identifier of the section (its index in the file). For patched files, this is the index of the section they are copied from in the original file*/
   int64_t address; /**<Address at which the section will be loaded into memory*/
   uint64_t offset; /**<Offset in the file where the section begins*/
   data_t** entries; /**<Array of entries contained in the section*/
   list_t* firstinsnseq;/**<List element containing the first instruction of the section (if it contains code)*/
   list_t* lastinsnseq; /**<List element containing the last instruction of the section (if it contains code)*/
   uint32_t n_entries; /**<Number of entries (size of the \c entries array)*/
   uint8_t type; /**<Type of data contained in the section*/
   uint16_t attrs; /**<Attributes over the section (mask from SCNA_* values)*/
   binseg_t** binsegs; /**<Array of segments to which this section belongs. Can be NULL if the binary format does not define segments or if the section is not loaded*/
   uint16_t n_binsegs; /**<Number of segments to which this section belongs (size of the \c binsegs array)*/
   binfile_t* binfile; /**<Binary file to which this section belongs*/
};

/**
 * Retrieves the name of a section in a binary file
 * \param scn The section
 * \return The name of the section or NULL if the section is NULL
 * */
extern char* binscn_get_name(binscn_t* scn);

/**
 * Returns the data contained in a section
 * \param scn Structure representing a parsed binary section
 * \param len Return parameter. If not NULL, contains the size in bytes of the data
 * \return A stream of data corresponding to the bytes present in the section, or NULL if the \c scn is NULL
 * */
extern unsigned char* binscn_get_data(binscn_t* scn, uint64_t* len);

/**
 * Returns a pointer to the data contained in a section at a given offset
 * \param scn Structure representing a parsed binary section
 * \param off Offset where to look in the section data.
 * \return A stream of data corresponding to the bytes present in the section at the given offset, or NULL if \c scn is NULL
 * or if \c off is larger than its length
 * */
extern unsigned char* binscn_get_data_at_offset(binscn_t* scn, uint64_t off);

/**
 * Retrieves the size of a section in a binary file
 * \param scn The section
 * \return The size of the section data or UNSIGNED_ERROR if the section is NULL
 * */
extern uint64_t binscn_get_size(binscn_t* scn);

/**
 * Retrieves the index of a section in a binary file
 * \param scn The section
 * \return The index of the section or BF_SCNID_ERROR if the section is NULL
 * */
extern uint16_t binscn_get_index(binscn_t* scn);

/**
 * Returns the attributes of a section
 * \param scn The section
 * \return The attributes of the section
 */
extern uint16_t binscn_get_attrs(binscn_t* scn);

/**
 * Retrieves the virtual address of a section in a binary file
 * \param scn The section
 * \return The address of the section or ADDRESS_ERROR if the section is NULL
 * */
extern int64_t binscn_get_addr(binscn_t* scn);

/**
 * Retrieves the virtual address of the end of a section in a binary file
 * \param scn The section
 * \param The sum of the address of the section with its size, or ADDRESS_ERROR if the section is null
 * */
extern int64_t binscn_get_end_addr(binscn_t* scn);

/**
 * Retrieves the offset of a section in a binary file
 * \param scn The section
 * \return The offset of the section or SIGNED_ERROR if the section is NULL
 * */
extern uint64_t binscn_get_offset(binscn_t* scn);

/**
 * Retrieves the offset of the end of a section in a binary file
 * \param scn The section
 * \param The sum of the offset of the section with its size if it is not of type SCNT_ZERODATA, the offset
 * of the section if it is, and 0 if the section is null
 * */
extern uint64_t binscn_get_end_offset(binscn_t* scn);

/**
 * Retrieves the array of entries of a section in a binary file
 * \param scn The section
 * \return The array of entries or PTR_ERROR if the section is NULL
 * */
extern data_t** binscn_get_entries(binscn_t* scn);

/**
 * Retrieves the number of data entries of a section in a binary file
 * \param scn The section
 * \return The number of data entries of the section or UNSIGNED_ERROR if the section is NULL
 * */
extern uint32_t binscn_get_nb_entries(binscn_t* scn);

/**
 * Retrieves an entry from a section in a binary file
 * \param scn The section
 * \param entryid The index of the entry in the section
 * \return Pointer to the data_t structure representing the entry, or NULL if scn is NULL or entryid greater than the number of its entries
 * */
extern data_t* binscn_get_entry(binscn_t* scn, uint32_t entryid);

/**
 * Retrieves the type of a section in a binary file
 * \param scn The section
 * \return The type of the section or SCNT_UNKNOWN if the section is NULL
 * */
extern uint8_t binscn_get_type(binscn_t* scn);

/**
 * Retrieves the size of entries in a section
 * \param scn Structure representing a binary section
 * \return The size of entries in the section or UNSIGNED_ERROR if the section is NULL
 * */
extern uint64_t binscn_get_entry_size(binscn_t* scn);

/**
 * Retrieves the internal structure associated to a given entry in an binary section.
 * \param scn A parsed binary section
 * \param entryid Index of the entry in the section
 * \return Pointer to the object corresponding to the entry. This only works for sections containing entries with a fixed size
 * (should be SCNT_REFS, SCNT_RELOC, SCNT_LABEL). It's up to the caller to cast it into the appropriate type
 * */
extern void* binscn_get_entry_data(binscn_t* scn, uint32_t entryid);

/**
 * Retrieves the first instruction in the section
 * \param scn Structure representing a binary section
 * \return List node containing the first instruction of the section or NULL if \c scn is NULL
 * */
extern list_t* binscn_get_first_insn_seq(binscn_t* scn);

/**
 * Retrieves the last instruction in the section
 * \param scn Structure representing a binary section
 * \return List node containing the last instruction of the section or NULL if \c scn is NULL
 * */
extern list_t* binscn_get_last_insn_seq(binscn_t* scn);

/**
 * Compares two sections based on their address. This function is intended to be used by qsort
 * \param s1 First section
 * \param s2 Second section
 * \return 1 if s1 has a higher address than s2, 0 if both have equal address and -1 otherwise
 * */
extern int binscn_cmp_by_addr_qsort(const void* s1, const void* s2);

/**
 * Checks if a given flag is present among the attributes of a section
 * \param scn The section
 * \param attr The flag to check in the section attributes
 * \return TRUE if the attribute is present, FALSE if not or if the section is NULL
 * */
extern int binscn_check_attrs(binscn_t* scn, uint16_t attr);

/**
 * Finds the index of a label stored in a section
 * \param scn The section
 * \param label The label
 * \return Index of the label, or BF_ENTID_ERROR if not found
 * \warning The use of this function may slow down execution
 * */
extern uint32_t binscn_find_label_id(binscn_t* scn, label_t* label);

/**
 * Retrieves the data object at a given offset inside a binary section
 * \param bs Structure representing a binary section
 * \param off Offset from the beginning of the section of the data to find
 * \param diff Return parameter. If not NULL, will contain the difference between the beginning of the returned entry and the actual offset
 * \return Data object beginning at or containing the given offset, or NULL if no object at this offset was found or if \c bs is NULL
 * */
extern data_t* binscn_lookup_entry_by_offset(binscn_t* bs, uint64_t off,
      uint64_t* diff);

/**
 * Retrieves the binary file to which a section belongs
 * \param scn The section
 * \return A pointer to the binary file containing the section or PTR_ERROR if the section is NULL
 * */
extern binfile_t* binscn_get_binfile(binscn_t* scn);

/**
 * Retrieves one of the segments to which the section is associated
 * \param scn A section
 * \param sgid Index of the segment in the list of segments to which the section is associated
 * \return Pointer to the segment at the given index among the segments associated to this section,
 * or NULL if scn is NULL or sgid greater than or equal the number of segments associated to this section
 * */
extern binseg_t* binscn_get_binseg(binscn_t* scn, uint16_t sgid);

/**
 * Retrieves the number of segments to which the section is associated
 * \param scn A section
 * \return Number of segments associated to this section or 0 if scn is NULL
 * */
extern uint16_t binscn_get_nb_binsegs(binscn_t* scn);

/**
 * Sets the name of a binary section
 * \param scn Structure representing a binary section
 * \param name The name of the section. It is assumed it is allocated elsewhere and will not be freed when freeing the section
 * */
extern void binscn_set_name(binscn_t* scn, char* name);

/**
 * Sets the data bytes of a binary section
 * \param scn Structure representing a binary section
 * \param data The data bytes contained in the section.
 * \param local Set to TRUE if the data must be freed along with the section,
 * and FALSE if it is allocated elsewhere and will not be freed when freeing the section
 * */
extern void binscn_set_data(binscn_t* scn, unsigned char* data, unsigned local);

/**
 * Sets the size of a binary section
 * \param scn Structure representing a binary section
 * \param size The size in bytes of the section
 * */
extern void binscn_set_size(binscn_t* scn, uint64_t size);

/**
 * Sets the index of a section in a binary file
 * \param scn  Structure representing a binary section
 * \param scnid Index of the section in the binary file to which it belongs
 * */
extern void binscn_set_id(binscn_t* scn, uint16_t scnid);

/**
 * Sets the address of a binary section
 * \param scn Structure representing a binary section
 * \param address The virtual address of the section.
 * */
extern void binscn_set_addr(binscn_t* scn, int64_t address);

/**
 * Sets the alignement of a section in a binary file
 * \param scn The section
 * \param align The alignment of the section
 * */
extern void binscn_set_align(binscn_t* scn, uint64_t align);

/**
 * Sets the offset of a binary section
 * \param scn Structure representing a binary section
 * \param offset The offset of the section.
 * */
extern void binscn_set_offset(binscn_t* scn, uint64_t offset);

/**
 * Sets the first instruction in the section
 * \param scn Structure representing a binary section
 * \param insnseq List node containing the first instruction of the section
 * */
extern void binscn_set_first_insn_seq(binscn_t* scn, list_t* insnseq);

/**
 * Sets the last instruction in the section
 * \param scn Structure representing a binary section
 * \param insnseq List node containing the last instruction of the section
 * */
extern void binscn_set_last_insn_seq(binscn_t* scn, list_t* insnseq);

/**
 * Adds an entry to a binary section at a given index.
 * If the index is larger than the number of entries in the section, the array of entries will be increased by one
 * and the entry added as the last.
 * If the section is flagged as loaded to memory, the address of the entry will be updated based on the address of the previous entry.
 * \param scn The section
 * \param entry The entry to add
 * \param entryid The index where to insert the entry. If higher than the number of entries (e.g. BF_ENTID_ERROR or scn->n_entries),
 * the array of entries in the section will be increased by one and the entry inserted last.
 * */
extern void binscn_add_entry(binscn_t* scn, data_t* entry, uint32_t entryid);

/**
 * Initialises the array of entries in a binary section
 * \param scn Structure representing a binary section
 * \param n_entries Number of entries
 * */
extern void binscn_set_nb_entries(binscn_t* scn, uint32_t n_entries);

/**
 * Sets the type of a binary section
 * \param scn Structure representing a binary section
 * \param type Type of the section
 * */
extern void binscn_set_type(binscn_t* scn, uint8_t type);

/**
 * Adds an attribute to the attributes of a binary section
 * \param scn Structure representing a binary section
 * \param attrs Attributes to add
 * */
extern void binscn_add_attrs(binscn_t* scn, uint16_t attrs);

/**
 * Removes an attribute from the attributes of a binary section
 * \param scn Structure representing a binary section
 * \param attrs Attributes to remove
 * */
extern void binscn_rem_attrs(binscn_t* scn, uint16_t attrs);

/**
 * Sets the attributes of a binary section
 * \param scn Structure representing a binary section
 * \param attrs Attributes mask to set
 * */
extern void binscn_set_attrs(binscn_t* scn, uint16_t attrs);

/**
 * Sets the size of entries in a section
 * \param scn Structure representing a binary section
 * \param entrysz Size of entries
 * */
extern void binscn_set_entry_size(binscn_t* scn, uint64_t entrysz);

/**
 * Sets the binary file to which a binary section belongs
 * \param scn Structure representing a binary section
 * \param binfile Pointer to the binary file
 * */
extern void binscn_set_binfile(binscn_t* scn, binfile_t* binfile);

/**
 * Retrieves alignement of a section in a binary file
 * \param scn The section
 * \return The alignment of the section or UINT32_MAX if the section is NULL
 * */
extern uint64_t binscn_get_align(binscn_t* scn);

/**
 * Parses a section containing strings to build an array of data entries.
 * \param scn The binscn_t structure to fill. Its type must be SCNT_STRING and its \c data must be filled
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise
 * */
extern int binscn_load_str_scn(binscn_t* scn);

/**
 * Parses a section containing entries with a fixed length and not associated to a type requiring a special handling.
 * \param scn The binscn_t structure to fill. Its \c data, \c n_entries and \c entrysz members must be filled
 * \param type Type of the data to create. Accepted values are DATA_RAW, DATA_VAL and DATA_NIL
 * Entries created with type DATA_RAW will point to the data element in the original section, while entries with DATA_VAL will contain
 * the corresponding value.
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise, including if the section has more than one entry while entrysz is set to 0
 * */
extern int binscn_load_entries(binscn_t* scn, datatype_t type);

/**
 * Adds a list of instruction to a patched section. This is valid only if the section has been created during patching session
 * \param scn Section to which instructions must be added
 * \param insns List of instructions. Will be freed after addition
 * \param firstinsn Node containing the first instruction to add. Will be used if insns is NULL and lastinsn is not
 * \param lastinsn Node containing the last instruction to add. Will be used if insns is NULL and firstinsn is not
 * \return EXIT_SUCCESS if successful, error code if \c scn is NULL, belongs to a file not being patched, or exists in the original file
 * */
extern int binscn_patch_add_insns(binscn_t* scn, queue_t* insns,
      list_t* firstinsn, list_t* lastinsn);

/**
 * Retrieves the type of a section in a file being patched. If the section is of type SCNT_PATCHCOPY, the type from its origin
 * will be returned instead
 * \param scn A section
 * \return The type of the section, or the type of the section from which it was copied if it is of type SCNT_PATCHCOPY, or SCNT_UNKNOWN
 * if \c scn is NULL or is not associated to a file being patched
 * */
extern uint8_t binscn_patch_get_type(binscn_t* scn);

/**
 * Retrieves the first instruction of a section in a file being patched.
 * If the section is of type SCNT_PATCHCOPY or if its first instruction is NULL, the first instructions from its origin will be returned instead
 * \param scn A section
 * \return The node containing the first instruction of the section, or the container containing the first instruction of the section
 * from which it was copied if it is of type SCNT_PATCHCOPY, or NULL if \c scn is NULL or is not associated to a file being patched
 * */
extern list_t* binscn_patch_get_first_insn_seq(binscn_t* scn);

/**
 * Retrieves the last instruction of a section in a file being patched.
 * If the section is of type SCNT_PATCHCOPY or if its last instruction is NULL, the last instructions from its origin will be returned instead
 * \param scn A section
 * \return The node containing the last instruction of the section, or the container containing the last instruction of the section
 * from which it was copied if it is of type SCNT_PATCHCOPY, or NULL if \c scn is NULL or is not associated to a file being patched
 * */
extern list_t* binscn_patch_get_last_insn_seq(binscn_t* scn);

/**
 * Checks if a given section has been added through a patching operation
 * \param scn A section
 * \return TRUE if the section belongs to a file being patched and does not exist in the creator, FALSE otherwise
 * */
extern int binscn_patch_is_new(binscn_t* scn);

/**
 * Checks if a section has been impacted by of a patching operation
 * \param scn The section
 * \return TRUE if the section is new or has a different size than the section at the same index in the original, FALSE otherwise
 * */
extern int binscn_patch_is_bigger(binscn_t* scn);

/**
 * Checks if the address of a section has changed from the original because of a patching operation
 * \param scn The section
 * \return TRUE if the section is flagged as reordered (SCNA_PATCHREORDER) and has a different address
 * than in the original file or is new, FALSE otherwise
 * */
extern int binscn_patch_is_moved(binscn_t* scn);

/**
 * Returns the data bytes of a binary section in a file being patched.
 * If the section had not been modified yet, the bytes from the original section will be copied
 * \param scn Structure representing a binary section
 * \return A pointer to the bytes contained in the section, or NULL if the section is NULL or does not belong to a file being patched
 * */
extern unsigned char* binscn_patch_get_data(binscn_t* scn);

/**
 * Sets the data bytes of a binary section
 * This will also flag the section has having been modified (if its type was set to SCNT_PATCHCOPY, it will be updated to the type of the original)
 * \param scn Structure representing a binary section
 * \param data The data bytes contained in the section. They will be freed along with the section.
 * \return EXIT_SUCCESS if the section belongs to a file being patched and has no data or data allocated locally, error code otherwise
 * */
extern int binscn_patch_set_data(binscn_t* scn, unsigned char* data);

/**
 * Generates the data bytes of a section from its entries
 * \param scn Structure representing a binary section
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int binscn_patch_set_data_from_entries(binscn_t* scn);

/**
 * Returns the original section of a section in a patched binary file
 * \param scn A section. Must belong to a file being patched
 * \return The section in the original file from which this section is a copy, or NULL if \c scn is NULL,
 * does not belong to a valid file being patched, or has been added by the patching session
 * */
extern binscn_t* binscn_patch_get_origin(binscn_t* scn);

/**
 * Adds an entry to a patched section. The entry will be appended to the array of entries
 * \param scn The section. Must belong to a file being patched
 * \param entry The entry to add
 * \return EXIT_SUCCESS if successful, error code if \c scn or \c entry is NULL or if \c scn does not belong to a file being patched
 * */
extern int binscn_patch_add_entry(binscn_t* scn, data_t* entry);

///////////////////////////////////////////////////////////////////////////////
//                                  binseg                                   //
///////////////////////////////////////////////////////////////////////////////

/**
 * Structure representing a group of loaded sections (segment) in a binary file
 * */
struct binseg_s {
   char* name; /**<Name of the section*/
   uint64_t offset; /**<Offset of the segment in the file*/
   int64_t address; /**<Virtual address of the segment*/
   uint64_t fsize; /**<Size in bytes of the segment in the file*/
   uint64_t msize; /**<Size in bytes of the segment in memory*/
   uint64_t align; /**<Alignement between the offset of the segment in the file and its virtual address*/
   binscn_t** scns; /**<Sections contained in the segment*/
   uint16_t n_scns; /**<Number of sections contained in the segment*/
   uint16_t segid; /**<Index of the segment inside the array of segments in the file*/
   uint8_t attrs; /**<Attributes of the segment (mask from SCNA_* values)*/
   binfile_t* binfile; /**<Binary file to which this section belongs*/
};

/**
 * Retrieves the name of a segment in a binary file
 * \param scn The segment
 * \return The name of the segment or PTR_ERROR if the segment is NULL
 * */
extern char* binseg_get_name(binseg_t* seg);

/**
 * Returns the number of sections associated to a segment
 * \param seg The segment
 * \return The number of sections associated to the segment or 0 if \c seg is NULL
 * */
extern uint16_t binseg_get_nb_scns(binseg_t* seg);

/**
 * Returns a section associated to a segment
 * \param seg The segment
 * \param scnid The index of the section inside the segment (this is not the index of the section in the binary file)
 * \return The section at the given index among the sections associated to the segment, or NULL if \c seg is NULL our scnid is
 * greater than or equal the number of sections associated to this segment
 * */
extern binscn_t* binseg_get_scn(binseg_t* seg, uint16_t scnid);

/**
 * Returns the offset of a segment in the file
 * \param seg The segment
 * \return The offset of the segment in the file, or 0 if \c seg is NULL
 * */
extern uint64_t binseg_get_offset(binseg_t* seg);

/**
 * Returns the offset of the end of a segment in the file
 * \param seg The segment
 * \return The offset of the end of the segment in the file, or 0 if \c seg is NULL
 * */
extern uint64_t binseg_get_end_offset(binseg_t* seg);

/**
 * Returns the Virtual address of a segment
 * \param seg The segment
 * \return The virtual address of the segment, or SIGNED_ERROR if \c seg is NULL
 * */
extern int64_t binseg_get_addr(binseg_t* seg);

/**
 * Returns the virtual address of the end of a segment
 * \param seg The segment
 * \return The virtual address of the end of the segment, or SIGNED_ERROR if \c seg is NULL
 * */
extern int64_t binseg_get_end_addr(binseg_t* seg);

/**
 * Returns the size in bytes of a segment in the file
 * \param seg The segment
 * \return The size in bytes of the segment in the file, or 0 if \c seg is NULL
 * */
extern uint64_t binseg_get_fsize(binseg_t* seg);

/**
 * Returns the size in bytes of a segment in memory
 * \param seg The segment
 * \return The size in bytes of the segment in memory, or 0 if \c seg is NULL
 * */
extern uint64_t binseg_get_msize(binseg_t* seg);

/**
 * Returns the id of a segment
 * \param seg The segment
 * \return The id of the segment
 */
extern uint16_t binseg_get_id(binseg_t* seg);

/**
 * Returns the attributes of a segment
 * \param seg The segment
 * \return The attributes of the segment
 */
extern uint8_t binseg_get_attrs(binseg_t* seg);

/**
 * Retrieves the binary file to which a segment belongs
 * \param seg The segment
 * \return A pointer to the binary file containing the segment or PTR_ERROR if the segment is NULL
 * */
extern binfile_t* binseg_get_binfile(binseg_t* seg);

/**
 * Returns the alignment of a segment
 * \param seg The segment
 * \return The alignment of the segment or 0 if \c seg is NULL
 * */
extern uint64_t binseg_get_align(binseg_t* seg);

/**
 * Checks if a given mask of attributes is set for a segment
 * \param seg The segment
 * \param attrs Mask of attributes (using SCNA_* values) to check against those of the segment
 * \return TRUE if all attributes set in \c attr are set for \c seg, FALSE otherwise
 * */
extern int binseg_check_attrs(binseg_t* seg, uint8_t attrs);

/**
 * Adds an attribute to the attributes of a binary segment
 * \param seg Structure representing a binary segment
 * \param attrs Attributes to add
 * */
extern void binseg_add_attrs(binseg_t* seg, uint8_t attrs);

/**
 * Sets the alignment of a segment
 * \param seg The segment
 * \param align The alignment
 * */
extern void binseg_set_align(binseg_t* seg, uint64_t align);

/**
 * Sets the offset of a segment in the file
 * \param seg The segment
 * \param offset Offset of the segment in the file
 * */
extern void binseg_set_offset(binseg_t* seg, uint64_t offset);

/**
 * Sets the virtual address of a segment
 * \param seg The segment
 * \param address The virtual address of the segment
 * */
extern void binseg_set_addr(binseg_t* seg, int64_t address);

/**
 * Sets the size in bytes of a segment in the file
 * \param seg The segment
 * \param fsize The size in bytes of the segment in the file
 * */
extern void binseg_set_fsize(binseg_t* seg, uint64_t fsize);

/**
 * Sets the size in bytes of a segment in memory
 * \param seg The segment
 * \param msize The size in bytes of the segment in memory
 * */
extern void binseg_set_msize(binseg_t* seg, uint64_t msize);

/**
 * Removes a section from those associated to a segment
 * \param seg A segment
 * \param scn A section associated to the segment
 * */
extern void binseg_rem_scn(binseg_t* seg, binscn_t* scn);

/**
 * Prints the details of a segment to a stream
 * \param seg The segment
 * \param stream The stream
 * */
extern void binseg_fprint(binseg_t* seg, FILE* stream);

///////////////////////////////////////////////////////////////////////////////
//                                  binfile                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Interface driver for binary files for accessing format-specific functions
 * \todo TODO (2014-07-10) All those functions could be declared independently with binfile or asmfile as a
 * parameter, and then invoke the function from the driver afterwards (just to make the use of the driver transparent).
 * To be done once the contents of the driver are stabilised.
 * => (2015-02-25) The better way is to create binfile_* function encapsulating them. The main interest for this
 * is that it allows to enforce that some functions are invoked at certain stages (e.g. computation of empty spaces
 * when initialising a patching operation). Besides, it's neater and easier to write in code using the libbin
 * */
struct bf_driver_s {
   void* parsedbin;/**<Format specific structure containing the parsed binary*/
   dbg_file_t* (*binfile_parse_dbg)(binfile_t*); /**<Retrieves the debug information for the file.*/
   void (*parsedbin_free)(void*); /**<Frees a parsed binary file*/
   int (*asmfile_add_ext_labels)(asmfile_t*); /**<Adds labels of the external functions to an asmfile.*/
//   int (*binfile_getscnannotate)(binfile_t*, binscn_t*);   /**<Retrieves the annotate to set on instructions in a section depending on its content */
   char* (*generate_ext_label_name)(char*); /**<Generates the label of an external function*/
   void (*asmfile_print_binfile)(asmfile_t*); /**<Prints the binary file*/
   int (*asmfile_print_external_fcts)(asmfile_t*, char*); /**<Prints the external functions*/
   /**Functions for patching a binary file. Return value, if an int, must be TRUE if successful, FALSE otherwise.
    * The first parameter of these functions is always a pointer to a binary file being patched (\c patch set to \ref BFP_PATCHING)*/
   queue_t* (*binfile_build_empty_spaces)(binfile_t*); /**<Builds the array of empty spaces in the file.*/
   /**\todo TODO (2015-02-19) The binfile_build_empty_spaces function may be moved somewhere else, or implicitly invoked during initcopy.
    * It could also return a queue of intervals instead of building an array inside the binfile (and we would remove it from the binfile)*/
   int64_t (*binfile_patch_get_first_load_addr)(binfile_t*); /**<Retrieves the first loaded address in the file*/
   int64_t (*binfile_patch_get_last_load_addr)(binfile_t*); /**<Retrieves the last loaded address in the file*/
   interval_t* (*binfile_patch_move_scn_to_interval)(binfile_t*, uint16_t, interval_t*); /**<Checks if a modified section can be moved into a given interval.
    This function must only perform format specific checks that are not done in binfile_patch_move_scn_to_interval
    NOTE: If it does not do any handling and leaves binfile_patch_move_scn_to_interval to do the work, it must return the 3rd parameter*/
   /**\todo TODO (2015-03-31) Find a better way to handle binfile_patch_move_scn_to_interval and the format-specific version, or document it in detail*/
   int (*binfile_patch_init_copy)(binfile_t*); /**<Initialises the format specific internal structure of a file being patched*/
   int (*binfile_patch_add_ext_lib)(binfile_t*, char*, boolean_t); /**<Adds an external library to the file. Parameters: Name of the library to add, priority.*/
   int (*binfile_patch_rename_ext_lib)(binfile_t*, char*, char*); /**<Rename an existing external library. Parameters: Old name, new name*/
   int (*binfile_patch_add_label)(binfile_t*, label_t*); /**<Adds a label to the file. Parameter: Label to add. Its type will must be set to allow identifying how and where to insert it*/
   int (*binfile_patch_add_scn)(binfile_t*, binscn_t*); /**<Adds a new section to a file. Parameter: The section. If the address of the section is set it must use it.*/
   int (*binfile_patch_add_seg)(binfile_t*, binseg_t*); /**<Adds a new segment to a file. Parameter: The segment. If the address of the segment is set it must use it.*/
   pointer_t* (*binfile_patch_add_ext_fct)(binfile_t*, char*, char*, int); /**<Adds a call to a function from an external library to a file.
    Parameters: Name of the function to add,
    Name of the external library where the function is defined (may be optional depending on the format)
    TRUE if the function address must be loaded when loading the executable,
    FALSE if the function address is to be loaded only when first invoked, with pre-loaded or dynamic loading
    Return: A pointer containing a reference to the object to reference for invoking this function */
   int (*binfile_patch_finalise)(binfile_t*, queue_t*); /**<Finalises a binary file being patched by building its format specific structure*/

   int (*binfile_writefile)(binfile_t*, char*); /**<Writes a parsed binary file into a file. Parameters: A binary file, a filename*/
   int (*binfile_patch_write_file)(binfile_t*); /**<Writes a patched binary file into a file. Parameters: A binary file*/
   char* codescnname; /**<Name for a section containing code.*/
   char* fixcodescnname; /**<Name for a section containing code at a fixed address.*/
   char* datascnname; /**<Name for a section containing data.*/
};

/**
 * Structure representing a binary file
 * \todo Reorder fields for size optimisation once stabilised (2014-04-17)
 * \todo Maybe change the sizes of some elements to use bit fields
 * */
struct binfile_s {
   char* filename; /**<Name of the binary file*/
   FILE* filestream; /**<Stream to the binary file*/
   uint8_t format; /**<Format of the binary file. Uses values from enum \ref bf_format*/
   unsigned type :4; /**<Type of the binary file. Uses values from enum \ref bf_type*/
   unsigned patch :4; /**<State of this file with regard to patching: whether it is a patched file or being patched*/
   uint8_t wordsize; /**<Word size of the binary (32/64). Uses values from enum \ref bfwordsz_t*/
   arch_t* arch; /**<Architecture for which the file is compiled*/
   abi_t* abi; /**<ABI for which the file is compiled */

   binscn_t* scnheader; /**<Representation of the header of sections*/
   binscn_t* segheader; /**<Representation of the header of segments*/
   binscn_t* symtable; /**<Representation of the symbol table*/

   binscn_t** sections; /**<Array of sections present in the file*/
   uint16_t n_sections; /**<Number of sections in the file (size of the \c sections array)*/
   binscn_t **codescns; /**<Array of executable loaded sections, ordered by address*/
   uint16_t n_codescns; /**<Number of executable loaded sections (size of the \c codescns array)*/
   binscn_t **loadscns; /**<Array of loaded code sections, ordered by address*/
   uint16_t n_loadscns; /**<Number of loaded code sections (size of the \c loadscns array)*/

   binseg_t** segments; /**<Array of segments present in the file (may be NULL if the format does not support segments)*/
   uint16_t n_segments; /**<Number of segments present in the file (size of the \c segments array)*/
   binscn_t** lblscns; /**<Array of sections containing label definitions*/
   uint16_t n_lblscns; /**<Number of sections containing label definitions (size of the \c lblscns array)*/

   uint32_t n_labels; /**<Number of labels in the file (size of the \c labels array)*/
   label_t** labels; /**<Array of label_t structures present in the file, in the order into which they can be found in the sections defining them*/
   hashtable_t* data_ptrs_by_target_data; /**<Table of data_t structures containing a pointer_t, indexed by the data_t structure the pointer references*/

   hashtable_t* data_ptrs_by_target_scn;/**<Table of data_t structures containing a pointer_t, indexed by the binscn_t structure the pointer references*/
   binrel_t** relocs; /**<Array of relocations present in the file*/
   uint32_t n_relocs; /**<Number of relocations in the file (size of \c relocs)*/
   data_t** extlibs; /**<Table of entries representing the names of external libraries used by the binary file*/
   uint16_t n_extlibs; /**<Number of external libraries used by the binary file (size of the \c extlibs array)*/

   /**\todo TODO (2014-06-06) This one may be temporary as we only need it during parsing. Find some way to get rid of it if it really
    * is only used then and if we never need to have a list of ordered labels by section somewhere later.
    * It's not very useful for code sections, as we need to check those that are eligible (and we add some).
    * However, it might be useful for data sections, in which case we would need to keep the array of labels by sections
    * in binfile_t (hello memory consumption).*/
   label_t*** lbls_by_scn; /**<Array of labels by section, ordered by address. Size of the array is the number of sections (\c n_sections)*/
   uint32_t *n_lbls_by_scn; /**<Array of the number of labels by section (element i contains the number of labels in section i, which is the size of lblscns[i])*/

   bf_driver_t driver; /**<Driver containing functions for accessing the underlying format-dependent structure representing the binary file*/
   binfile_t** ar_elts; /**<For archive files, array of structures representing the archive members*/
   uint16_t n_ar_elts; /**<Number of archive members (size of \c ar_elts)*/
   binfile_t* creator; /**<Binary file from which this is a copy*/
   binfile_t* archive; /**<Pointer to the binary file structure containing this file*/
   asmfile_t* asmfile; /**<Pointer to the asmfile structure containing this binary file*/

   int last_error_code; /**<Code of the last encountered error*/

   uint8_t byte_order; /**<Specifying the byte order. Uses values from enum \ref bf_byte_order*/

   hashtable_t* entrycopies; /**<Hashtable containing all entries in a file being patched that were duplicated from the original, indexed by a pointer
    to the original entry in the creator file*/
};

/**
 * Function type for the loader of a parsed file into a binary file structure
 * */
typedef int (*binfile_loadfct_t)(binfile_t*);

/**
 * Retrieves the name of a binary format
 * \param formatid Identifier of the binary format
 * \return Constant string representing the name of the format or NULL if formatid is out of range
 * */
extern char* bf_format_getname(unsigned int formatid);

/**
 * Returns the code of the last error encountered and resets it
 * \param bf The binfile
 * \return The last error code encountered. It will be reset to EXIT_SUCCESS afterward.
 * If \c bf is NULL, ERR_BINARY_MISSING_BINFILE will be returned
 * */
extern int binfile_get_last_error_code(binfile_t* bf);

/**
 * Sets the code of the last error encountered.
 * \param bf The binfile
 * \param error_code The error code to set
 * \return ERR_BINARY_MISSING_BINFILE if \c bf is NULL, or the previous error value of the last error code in the binfile
 * */
extern int binfile_set_last_error_code(binfile_t* bf, int error_code);

/**
 * Initialises a new structure representing a binary file
 * \param filename Name of the binary file (string will be duplicated). Can be NULL
 * \return An empty structure representing a binary file
 * */
extern binfile_t* binfile_new(char* filename);

/**
 * Frees a structure representing a binary file
 * \param bf The binary file structure to free
 * */
extern void binfile_free(binfile_t* bf);

/**
 * Returns the name of the file of this binary file
 * \param bf Structure representing a binary file
 * \return Name of the file, or NULL if \c bf is NULL
 * */
extern char* binfile_get_file_name(binfile_t* bf);

/**
 * Returns the stream to the file from which this binary file structure was parsed
 * \param bf Structure representing a binary file
 * \return Pointer to the stream or PTR_ERROR if \c bf is NULL
 * */
extern FILE* binfile_get_file_stream(binfile_t* bf);

/**
 * Retrieves the format of a binary file
 * \param bf Structure representing a binary file
 * \return Identifier of the format or BFF_UNKNOWN if \c bf is NULL or not parsed
 * */
extern uint8_t binfile_get_format(binfile_t* bf);

/**
 * Returns the type of a parsed binary file
 * \param bf Structure representing a binary file
 * \return Identifier of the format, or BFF_UNKNOWN if \c bf is NULL
 * */
extern unsigned binfile_get_type(binfile_t* bf);

/**
 * Returns the patching status of a parsed binary file (as defined in \ref bf_patch)
 * \param bf Structure representing a binary file
 * \return Identifier of the patching status, or BFP_NONE if \c bf is NULL
 * */
extern unsigned binfile_get_patch_status(binfile_t* bf);

/**
 * Returns the word size of a parsed binary file (as defined in \ref bfwordsz_t)
 * \param bf Structure representing a binary file
 * \return Identifier of the word size, or BFS_UNKNOWN if \c bf is NULL
 * */
extern uint8_t binfile_get_word_size(binfile_t* bf);

/**
 * Prints a formatted representation of the code areas. A code area contains a number of consecutive
 * sections containing executable code
 * \param bf Structure representing a binary file
 * */
extern void binfile_print_code_areas(binfile_t* bf);

/**
 * Returns the architecture for which this binary file was compiled
 * \param bf Structure representing a binary file
 * \return arch_t structure characterising the architecture, or NULL if \c bf is NULL
 * */
extern arch_t* binfile_get_arch(binfile_t* bf);

/**
 * Returns the ABI for which a binary file is compiled
 * \param bf Structure representing a binary file
 * \return abi_t structure characterising the ABI, or NULL if \c bf is NULL
 * */
extern abi_t* binfile_get_abi(binfile_t* bf);

/**
 * Returns the array of sections in a binary file
 * \param bf Structure representing a binary file
 * \return Array of sections in the file, or NULL if \c bf is NULL
 * */
extern binscn_t** binfile_get_scns(binfile_t* bf);

/**
 * Returns the number of sections in a binary file
 * \param bf Structure representing a binary file
 * \return The number of sections in the file
 * */
extern uint16_t binfile_get_nb_sections(binfile_t* bf);

/**
 * Returns the number of segments in a binary file
 * \param bf Structure representing a binary file
 * \return The number of segments in the file
 * */
extern uint16_t binfile_get_nb_segments(binfile_t* bf);

/**
 * Gets a segment at a given index
 * \param bf Structure representing a binary file
 * \param segid Index of the segment
 * \return Pointer to the binseg_t structure representing the section or NULL if \c bf is NULL or segid greater than its number of sections
 * */
extern binseg_t* binfile_get_seg(binfile_t* bf, uint16_t segid);

/**
 * Gets a segment at a given index among segments ordered by starting address
 * \param bf Structure representing a binary file
 * \param segid Index of the segment among the ordered segments
 * \return Pointer to the binseg_t structure representing the section or NULL if \c bf is NULL or segid greater than its number of sections
 * */
extern binseg_t* binfile_get_seg_ordered(binfile_t* bf, uint16_t segid);

/**
 * Returns the array of sections containing executable code in a binary file
 * \param bf Structure representing a binary file
 * \return The array of sections containing executable code in the file, or NULL if \c bf is NULL
 * */
extern binscn_t **binfile_get_code_scns(binfile_t* bf);

/**
 * Returns a section containing code base on its id among other section containing code
 * \param bf Structure representing a binary file
 * \param codescnid Index of a section in the array of sections containing code
 * \return The section containing code at the index \c codescnid in the codescns array or NULL if codescnid is out of range or if \c bf is NULL
 * */
extern binscn_t* binfile_get_code_scn(binfile_t* bf, uint16_t codescnid);

/**
 * Returns the name of an external dynamic library used by this binary file.
 * \param bf Structure representing a binary file
 * \param i Index of the external library
 * \return Name of the external library at the given address, or NULL if \c bf is NULL
 * or \c i is superior to the number of external libraries to the file
 * */
extern char* binfile_get_ext_lib_name(binfile_t* bf, unsigned int i);

/**
 * Returns an array of labels associated to a given section, ordered by address
 * \param bf Structure representing a binary file
 * \param scnid Index of the section
 * \param n_lbls Return parameter. Will contain the number of labels in the array if not NULL
 * \return Array of labels associated to the section \c scnid, ordered by address, or NULL if bf is NULL or scnid out of range
 * */
extern label_t** binfile_get_labels_by_scn(binfile_t* bf, uint16_t scnid,
      uint32_t* n_lbls);

/**
 * Returns the number of external dynamic libraries used by this binary file.
 * \param bf Structure representing a binary file
 * \return Number of external libraries, or UNSIGNED_ERROR if \c bf is NULL
 * */
extern uint16_t binfile_get_nb_ext_libs(binfile_t* bf);

/**
 * Returns the number of executable loaded sections in this binary file.
 * \param bf Structure representing a binary file
 * \return Number of executable loaded sections, or UNSIGNED_ERROR if \c bf is NULL
 * */
extern uint16_t binfile_get_nb_code_scns(binfile_t* bf);

/**
 * Returns the array of loaded sections in this binary file.
 * \param bf Structure representing a binary file
 * \return Array of loaded sections, or NULL if \c bf is NULL
 * */
extern binscn_t **binfile_get_load_scns(binfile_t* bf);

/**
 * Returns a loaded section based on its id among other section containing code
 * \param bf Structure representing a binary file
 * \param loadscnid Index of a section in the array of loaded sections
 * \return The loaded section at the index \c loadscnid in the loadscns array or NULL if loadscnid is out of range or if \c bf is NULL
 * */
extern binscn_t* binfile_get_load_scn(binfile_t* bf, uint16_t loadscnid);

/**
 * Returns the number of loaded sections in this binary file.
 * \param bf Structure representing a binary file
 * \return Number of loaded sections, or UNSIGNED_ERROR if \c bf is NULL
 * */
extern uint16_t binfile_get_nb_load_scns(binfile_t* bf);

/**
 * Returns the number of labels in this binary file.
 * \param bf Structure representing a binary file
 * \return Number of labels, or UNSIGNED_ERROR if \c bf is NULL
 * */
extern uint32_t binfile_get_nb_labels(binfile_t* bf);

/**
 * Returns the array of labels in this binary file.
 * \param bf Structure representing a binary file
 * \param labelid Index of a label
 * \return Label at this index in the list of labels defined in the file, or NULL if \c bf is NULL or labelid above the number of labels
 * */
extern label_t* binfile_get_file_label(binfile_t* bf, uint32_t labelid);

/**
 * Retrieves the section representing the section header
 * \param bf Structure representing a binary file
 * \return The section representing the section header, or NULL if \c bf is NULL or not parsed
 * */
extern binscn_t* binfile_get_scn_header(binfile_t* bf);

/**
 * Retrieves the section representing the segment header
 * \param bf Structure representing a binary file
 * \return The section representing the segment header, or NULL if \c bf is NULL or not parsed
 * */
extern binscn_t* binfile_get_seg_header(binfile_t* bf);

/**
 * Removes a data entry of type DATA_PTR with an unlinked pointer from the table of entries
 * \param bf Structure representing a binary file
 * \param unlinked The data entry containing an unlinked pointer. Will not be removed if the pointer is linked to a target of type data
 * */
extern void binfile_remove_unlinked_target(binfile_t* bf, data_t* unlinked);

/**
 * Returns a pointer to the driver containing functions for accessing the underlying format-dependent structure representing the binary file
 * \param bf Structure representing a binary file
 * \return Driver containing functions for accessing the underlying format-dependent structure representing the binary file, or NULL if \c bf is NULL
 * */
extern bf_driver_t* binfile_get_driver(binfile_t* bf);

/**
 * Returns a pointer to the format-specific structure representing the parsed binary file
 * \param bf Structure representing a binary file
 * \return Pointer to a format-specific structure built by the format-specific binary parser or NULL if \c bf is NULL or has not been parsed
 * */
extern void* binfile_get_parsed_bin(binfile_t* bf);

/**
 * Returns the structure representing the given archive member in this binary file if it is an archive.
 * \param bf Structure representing a binary file
 * \param i Index of the archive member to be retrieved
 * \return Structure representing the archive member at the given index in an archive file,
 * or NULL if \c bf is NULL, not an archive, or if i is higher than its number of archive elements
 * */
extern binfile_t* binfile_get_ar_elt(binfile_t* bf, uint16_t i);

/**
 * Returns the number of archive members in this binary file.
 * \param bf Structure representing a binary file
 * \return Number of members, or UNSIGNED_ERROR if \c bf is NULL
 * */
extern uint16_t binfile_get_nb_ar_elts(binfile_t* bf);

/**
 * Returns a pointer to the binary file from which this file is a copy
 * \param bf Structure representing a binary file
 * \return A pointer to the original file, or NULL if \c bf is NULL or not copied from another binfile_t structure
 * */
extern binfile_t* binfile_get_creator(binfile_t* bf);

/**
 * Returns a pointer to the binary file representing an archive from which this file is a member
 * \param bf Structure representing a binary file
 * \return A pointer to the binfile_t structure representing the archive containing file, or NULL if \c bf is NULL or not an archive member
 * */
extern binfile_t* binfile_get_archive(binfile_t* bf);

/**
 * Returns a pointer to the asm file based on this binary file
 * \param bf Structure representing a binary file
 * \return A pointer to the asmfile_t structure representing the file contained in this binary file or NULL if \c bf is NULL
 * */
extern asmfile_t* binfile_get_asmfile(binfile_t* bf);

/**
 * Gets a section at a given index
 * \param bf Structure representing a binary file
 * \param scnid Index of the section
 * \return Pointer to the binscn_t structure representing the section or NULL if \c bf is NULL or scnid greater than its number of sections
 * */
extern binscn_t* binfile_get_scn(binfile_t* bf, uint16_t scnid);

/**
 * Returns the array of empty intervals in a file
 * \param bf Structure representing a binary file
 * \return Array of empty intervals in the file
 * */
extern interval_t** binfile_get_empty_spaces(binfile_t* bf);

/**
 * Returns the number of empty intervals in a file
 * \param bf Structure representing a binary file
 * \return Size of the array of empty intervals in the file
 * */
extern uint32_t binfile_get_nb_empty_spaces(binfile_t* bf);
/**
 * Returns the name of a section in a binary file
 * \param bf Structure representing a parsed binary file * \return The na
 * \param scnid Index of the section
 * \return The name of the section or NULL if \c bf is NULL or scnid greater than its number of sections
 * */
extern char* binfile_get_scn_name(binfile_t* bf, uint16_t scnid);

/**
 * Returns the byte order in a binary file
 * \param bf Structure representing a parser binary file
 * \return The byte order of the binary file or UNSIGNED_ERROR if \c bf is NULL
 */
extern uint8_t binfile_get_byte_order(binfile_t* bf);

/**
 * Sets the name of a binary file
 * \param bf Structure representing a binary file
 * \param filename Name of the binary file
 * */
extern void binfile_set_filename(binfile_t* bf, char* filename);

/**
 * Sets the stream to a binary file
 * \param bf Structure representing a binary file
 * \param filestream Stream to the binary file
 * */
extern void binfile_set_filestream(binfile_t* bf, FILE* filestream);

/**
 * Sets the format of a binary file.
 * \param bf Structure representing a binary file
 * \param format Format of the binary file. Uses values from enum \ref bf_format
 * */
extern void binfile_set_format(binfile_t* bf, uint8_t format);

/**
 * Sets the type to a binary file
 * \param bf Structure representing a binary file
 * \param type Type of the binary file. Uses values from enum \ref bf_type
 * */
extern void binfile_set_type(binfile_t* bf, unsigned type);

/**
 * Sets the patching status to a binary file
 * \param bf Structure representing a binary file
 * \param patch State of this file with regard to patching: whether it is a patched file or being patched. Uses values from enum \ref bf_patch
 * */
extern void binfile_set_patch_status(binfile_t* bf, unsigned patch);

/**
 * Sets the wordsize of a binary file
 * \param bf Structure representing a binary file
 * \param wordsize Word size of the binary (32/64). Uses values from enum \ref bfwordsz_t
 * */
extern void binfile_set_word_size(binfile_t* bf, uint8_t wordsize);

/**
 * Sets the byte order in a binary file
 * \param bf Structure representing a binary file
 * \param byte_order Byte order scheme of the binary file. Uses values from enum \ref bf_byte_order
 * */
extern void binfile_set_byte_order(binfile_t* bf, uint8_t byte_order);

/**
 * Sets the architecture of a binary file
 * \param bf Structure representing a binary file
 * \param arch Pointer to the structure describing the architecture for which the file is compiled
 * */
extern void binfile_set_arch(binfile_t* bf, arch_t* arch);

/**
 * Sets the abi of a binary file
 * \param bf Structure representing a binary file
 * \param abi Pointer to the structure describing the ABI for which the file is compiled
 * */
extern void binfile_set_abi(binfile_t* bf, abi_t* abi);

/**
 * Sets the array of sections of a binary file
 * \param bf Structure representing a binary file
 * \param sections Array of pointers to binscn_t structures representing the sections present in the file
 * */
extern void binfile_set_scns(binfile_t* bf, binscn_t** sections);

/**
 * Sets a section at a given index in a binary file
 * \param bf Structure representing a binary file
 * \param section binscn_t structure representing the section to set in the file.
 * \param scnid Index of the section to add. The number of sections will be increased if it is higher than the current number.
 * */
extern void binfile_set_scn(binfile_t* bf, binscn_t* section, uint16_t scnid);

/**
 * Adds a section in a binary file to the list of executable loaded sections
 * \param bf Structure representing a binary file
 * \param scnid Index of the section
 * */
extern void binfile_addcodescn(binfile_t* bf, uint16_t scnid);

/**
 * Sets the number of labels in a binary file. This also initialises the array of labels.
 * If the array is already initialised, it will be resized if the new size is
 * bigger than the current size.
 * \param bf Structure representing a binary file
 * \param n_labels Number of labels in the file
 * */
extern void binfile_set_nb_labels(binfile_t* bf, uint32_t n_labels);

/**
 * Adds a data entry to a binary file at a given address and returns the result.
 * If an entry already exists at this address, it is returned instead.
 * The data is only created if the address is found to be in a section containing data.
 * This function is intended to allow the creation of data objects corresponding to a target
 * from an instruction.
 * \param bf Structure representing a binary file
 * \param add Address at which the data is located.
 * \param off Return parameter. If present, will be updated to contain the offset between the address of the returned
 * data and the actual required address. This is only the case when the data entry is not created (section not of type SCNT_DATA)
 * \param label Optional parameter. If not NULL, will be used to retrieve the address and section of the data to create
 * \return A data object found at the given address, or NULL if \c bf is NULL, \c addr is outside the range
 * of addresses existing in the file, or if its corresponds to a section not containing data nor an existing entry
 * at this address.
 * */
extern data_t* binfile_adddata(binfile_t* bf, int64_t addr, uint64_t *off,
      label_t* label);

/**
 * Adds a label in a binary file
 * \param bf Structure representing a binary file
 * \param scnid Section where the label is defined
 * \param entryid The index where to add the data entry representing the label in the section. If higher than the current number of
 * entries in the section, it will be inserted after the last one.
 * \param labelid The index of the label in the array of labels from the file. If higher than the current number of labels in the file,
 * it will be inserted after the last one
 * \param label label_t structure representing the label to add
 * \param size Size of the entry representing the label
 * \param symscnid Index of the section associated to the label, or BF_SCNID_ERROR if the label is not associated to a section
 * \return A pointer to the entry representing the label
 * */
extern data_t* binfile_addlabel(binfile_t* bf, uint32_t scnid, uint32_t entryid,
      uint32_t labelid, label_t* label, uint64_t size, uint32_t symscnid);

/**
 * Adds a reference to a binary file. If the destination can't be found it will be added to the table of unlinked data_ptrs_by_target_data
 * \param bf Structure representing a binary file
 * \param scnid Index of the section to which the reference is to be added (must be of type REF or REL)
 * \param entryid The index where to add the data entry representing the label in the section. If higher than the current number of
 * entries in the section, it will be inserted after the last one.
 * \param addr Address pointed by the reference
 * \param size Size of the entry representing the reference
 * \param dstscn Section pointed by the reference (if not NULL, will be used as the pointed reference)
 * \return A pointer to the entry representing the reference
 * */
extern data_t* binfile_add_ref(binfile_t* bf, uint16_t scnid, uint32_t entryid,
      int64_t addr, uint64_t size, uint16_t refscnid, binscn_t* dstscn);

/**
 * Adds an internal reference in a binary file (pointer to an object inside the file) based on an offset inside a section
 * If the destination can't be found it will be added to the table of unlinked targets
 * The size of the entry representing the reference will be set to the size of entries in the section (\c entrysz).
 * If it is not fixed for this section, it's up to the caller to update it afterwards. => Ok, remove this if we keep \c size as a parameter
 * \param bf Structure representing a binary file
 * \param scnid Index of the section to which the reference is to be added (must be of type SCNT_REF)
 * \param entryid The index where to add the data entry representing the reference in the section. If higher than the current number of
 * entries in the section, it will be inserted after the last one.
 * \param refscnid Index of the section containing the reference. Must be set
 * \param offset Offset pointed by the reference inside the section \c refscnid
 * \param size Size of the entry representing the reference (needed only if the size of entries in the section is not fixed)
 * \return A Pointer to the entry representing the reference or NULL if the file is invalid or no entry could be found at this offset
 * */
extern data_t* binfile_add_ref_byoffset(binfile_t* bf, uint16_t scnid,
      uint32_t entryid, uint16_t refscnid, uint64_t offset, uint64_t size);

/**
 * Adds a relocation to a binary file
 * If the destination of the relocation can't be found it will be added to the table of unlinked targets.
 * The size of the entry representing the relocation will be set to the size of entries in the section (\c entrysz).
 * If it is not fixed for this section, it's up to the caller to update it afterwards. => Ok, remove this if we keep \c size as a parameter
 * \param bf Structure representing a binary file
 * \param scnid Index of the section containing the relocation (must be of type SCNT_RELOC)
 * \param entryid The index where to add the data entry representing the label in the section. If higher than the current number of
 * entries in the section, it will be inserted after the last one.
 * \param label Label associated to the relocation
 * \param addr Address pointed by the relocation. If set to ADDRESS_ERROR, \c offset will be used instead
 * \param offset Offset in relscnid pointed by the relocation. If set to UINT64_MAX while \c addr is set to ADDRESS_ERROR, an error will be returned
 * \param size Size of the entry representing the relocation (needed only if the size of entries in the section is not fixed)
 * \param relscnid Index of the section pointed by the relocation. If higher than the current number of sections, the section will be searched based on \c addr
 * \param reltype Format specific type of the relocation
 * \return A Pointer to the entry representing the relocation
 * */
extern data_t* binfile_addreloc(binfile_t* bf, uint16_t scnid, uint32_t entryid,
      label_t* label, uint64_t size, int64_t addr, uint64_t offset,
      uint16_t relscnid, uint32_t reltype);

/**
 * Adds a pointer in a binary file
 * \param bf Structure representing a binary file
 * \param ptr pointer_t structure representing the pointer
 * \param addr Address referenced by the pointer (will be used as an index)
 * */
extern void binfile_addtarget(binfile_t* bf, pointer_t* ptr, int64_t addr);

/**
 * Retrieves the pointers addressing a particular address
 * \param bf Structure representing a binary file
 * \param dest Object referenced by the pointer
 * \return Queue of data_t structures containing pointers or relocations referencing this address
 * */
extern queue_t* binfile_lookup_ptrs_by_target(binfile_t* bf, void* dest);

/**
 * Retrieves a section spanning over a given address. The section is assumed to be loaded
 * \param bf Structure representing a parsed binary file
 * \param addr Address to search for
 * \return Pointer to the section spanning over this address, or NULL if it is not found
 * */
extern binscn_t* binfile_lookup_scn_span_addr(binfile_t* bf, int64_t addr);

/**
 * Retrieves a section with a given name
 * \param bf Structure representing a parsed binary file
 * \param name Name of the section to search for
 * \param Pointer to the first section found with that name, or NULL if no such section was found
 * */
extern binscn_t* binfile_lookup_scn_by_name(binfile_t* bf, char* name);

/**
 * Looks up a label at a given address
 * \param bf Structure representing a parsed binary file
 * \param scn Structure representing the binary section where to find the label. If NULL, will be deduced from the address
 * \param addr The address where to look up for a label
 * \return The label if one was found at this address and in this section (if not NULL), and NULL otherwise
 * */
extern label_t* binfile_lookup_label_at_addr(binfile_t* bf, binscn_t* scn,
      int64_t addr);

/**
 * Retrieves the pointers referencing a particular address
 * \param bf Structure representing a binary file
 * \param dest Object referenced by the pointer
 * \param addr Address of the object
 * \return Queue of data_t structures containing pointers or relocations referencing this object or address
 * \warning This function may cause overhead as it will attempt to find unlinked pointers as well
 * */
extern queue_t* binfile_lookup_ptrs_by_addr(binfile_t* bf, void* dest,
      int64_t addr);

/**
 * Retrieves a queue of data_t structures referencing an address with an unknown target, ordered by the referenced address
 * \param bf Structure representing a binary file
 * \return A queue of data_t structure of type DATA_POINTER containing a pointer_t with a NULL target, ordered by the address
 * referenced by the pointer
 * */
extern queue_t* binfile_lookup_unlinked_ptrs(binfile_t* bf);

/**
 * Looks up a segment in a file from its start and stop address
 * \param bf Structure representing a parsed binary file. Must have been fully loaded and finalised
 * \param start First address from which to look up the segment
 * \param stop Last address up to which to look up the segment
 * \return The first segment encountered whose starting address is equal to or above \c start and
 * ending address is equal to or below \c stop, or NULL if \c bf is NULL or if no such segment can be found
 * */
extern binseg_t* binfile_lookup_seg_in_interval(binfile_t* bf, maddr_t start,
      maddr_t stop);

/**
 * Retrieves a pointer to the data stored in a binary file depending on its address
 * \param bf A parsed binary file
 * \param address The virtual address at which the researched data is to be looked
 * \return Pointer to the data bytes stored in the binary that are loaded at the given
 * address, or NULL if \c bf is NULL or \c address is outside of the load addresses ranges
 * */
extern unsigned char* binfile_get_data_at_addr(binfile_t* bf, int64_t address);

/**
 * Retrieves a queue of external labels (type LBL_EXTERNAL, indicating a label defined in another file)
 * \param bf Structure representing a parsed binary file
 * \return Queue of labels of type LBL_EXTERNAL or NULL if \c bf is NULL, of an unsupported type, or without labels
 * */
extern queue_t* binfile_find_ext_labels(binfile_t* bf);

/**
 * Adds an external library to the array in a binary file
 * \param bf Structure representing a binary file
 * \param extlib Data structure containing a pointer to the name of the external library to add
 * */
extern void binfile_addextlib(binfile_t* bf, data_t* extlib);

/**
 * Sets the structure representing the member of an archive binary file at a given index
 * \param bf Structure representing a binary file
 * \param ar_elt Structure describing the binary member
 * \param eltid Index of the archive member to add. If higher than the current number of archive members, the size of the
 * array will be increased by one.
 * */
extern void binfile_set_ar_elt(binfile_t* bf, binfile_t* ar_elt, uint16_t eltid);

/**
 * Sets the number of structures representing the members of an archive binary file. This will resize the array accordingly
 * \param bf Structure representing a binary file
 * \param n_ar_elts Number of archive members
 * */
extern void binfile_set_nb_ar_elts(binfile_t* bf, uint16_t n_ar_elts);

/**
 * Sets the creator of a copied binary file
 * \param bf Structure representing a binary file
 * \param creator Pointer to the binfile_t structure representing the binary file from which this is a copy
 * */
extern void binfile_set_creator(binfile_t* bf, binfile_t* creator);

/**
 * Sets the archive from which this binary file is a member
 * \param bf Structure representing a binary file
 * \param archive Pointer to the binary file structure containing this file
 * */
extern void binfile_set_archive(binfile_t* bf, binfile_t* archive);

/**
 * Sets the asm file based on this binary file
 * \param bf Structure representing a binary file
 * \param A pointer to the asmfile_t structure representing the file contained in this binary file
 * */
extern void binfile_set_asmfile(binfile_t* bf, asmfile_t* asmfile);

/**
 * Sets the number of sections in a binary file and initialises the associated arrays.
 * If the array is already initialised, it will be resized.
 * \param bf Structure representing a binary file
 * \param nscns Number of sections
 * */
extern void binfile_set_nb_scns(binfile_t* bf, uint16_t nscns);

/**
 * Sets the number of segments in a binary file and initialises the associated arrays.
 * If the array is already initialised, it will be resized.
 * \param bf Structure representing a binary file
 * \param nsegs Number of segments
 * */
extern void binfile_set_nb_segs(binfile_t* bf, uint16_t nsegs);

/**
 * Initialises a section at a given index in a binary file.
 * \param bf Structure representing a binary file
 * \param scnid Index of the section to add. The number of sections will be increased if it is higher than the current number.
 * \param name Name of the section
 * \param type Type of the section
 * \param address Address of the section
 * \param attrs Attributes of the section
 * \return The new section
 * */
extern binscn_t* binfile_init_scn(binfile_t* bf, uint16_t scnid, char* name,
      scntype_t type, int64_t address, unsigned attrs);

/**
 * Initialises the section containing the symbol table.
 * This is reserved for binary formats where the symbol table is not a section
 * \param bf The binary file
 * \return EXIT_SUCCESS or error code
 * */
extern int binfile_init_sym_table(binfile_t* bf);

/**
 * Adds a segment to a binary file
 * \param bf Structure representing a binary file
 * \param segid Index of the segment to add. The number of segments will be increased if it is higher than the current number.
 * \param offset Offset in the file at which the segment starts
 * \param address Virtual address at which the segment is loaded to memory
 * \param fsize Size of the segment in the file
 * \param msize Size of the segment in memory
 * \param attrs Attributes of the segment
 * \param align Alignment of the segment
 * \return The structure representing the segment
 * */
extern binseg_t* binfile_init_seg(binfile_t* bf, uint16_t segid,
      uint64_t offset, int64_t address, uint64_t fsize, uint64_t msize,
      uint8_t attrs, uint64_t align);

/**
 * Adds a section to one or more segments. If the segment is not provided, it will be searched for based on the offset in the file
 * This requires that the segments have already been loaded in the binfile
 * \param bf Structure representing a binary file
 * \param scn The section
 * \param seg The segment. If NULL, eligible segments will be searched using the offset in the file of the section
 * \return EXIT_SUCCESS if the section is not NULL could be successfully added to at least one segment, error code otherwise
 * */
extern int binfile_addsection_tosegment(binfile_t* bf, binscn_t* scn,
      binseg_t* seg);

/**
 * Updates the labels in a binary file associated to labels or string sections. This function must be invoked once all the
 * sections containing labels have been parsed, and before other sections are parsed
 * \param bf Structure representing a binary file
 * */
extern void binfile_updatelabelsections(binfile_t* bf);

/**
 * Attempts to link unlinked pointer data entries
 * \param bf Structure representing a binary file. It must be fully loaded and the array of loaded sections sorted by address
 * */
extern void binfile_link_data_ptrs(binfile_t* bf);

/**
 * Loads the table of section headers from a binary file
 * \param bf The binary file
 * \param offset Offset into the file at which the table of section headers can be found
 * \param address Address into which the table of section headers is loaded in memory
 * \param size Size in bytes of the table of section headers
 * \param hdrentsz Size in bytes of an entry in the table of section headers
 * \param data The table of section headers
 * \return EXIT_SUCCESS if the operation was successful, error code otherwise or if the section header was already loaded
 * */
extern int binfile_load_scn_header(binfile_t* bf, uint64_t offset,
      int64_t address, uint64_t size, uint64_t hdrentsz, void* data);

/**
 * Loads the table of segment headers from a binary file
 * \param bf The binary file
 * \param offset Offset into the file at which the table of segment headers can be found
 * \param address Address into which the table of segment headers is loaded in memory
 * \param size Size in bytes of the table of segment headers
 * \param hdrentsz Size in bytes of an entry in the table of segment headers
 * \param data The table of segment headers
 * \return TRUE if the operation was successful, FALSE otherwise or if the segment header was already loaded
 * */
extern int binfile_load_seg_header(binfile_t* bf, uint64_t offset,
      int64_t address, uint64_t size, uint64_t hdrentsz, void* data);

/**
 * Finalises the loading of a binary file by filling internal fields. This function has to be called when the loading is complete
 * \param bf Structure representing a binary file
 * */
extern void binfile_finalise_load(binfile_t* bf);

/**
 * Parses a binary file and stores the result in a new structure
 * \param filename Path to the binary file to parse
 * \param binfile_loader Pointer to a function performing the loading of a binary file in a binfile structure and returning TRUE if successful
 * \return A newly allocated structure representing the parsed file
 * */
extern binfile_t* binfile_parse_new(char* filename,
      binfile_loadfct_t binfile_loader);

/**
 * Attempts to load the debug information contained in the file and returns a description of them
 * \param bf Structure representing a binary file
 * \return A structure describing the debug information contained in the file, or NULL if \c bf is NULL.
 * If the file does not contain debug information, the returned structure will have its format set to \DBG_NONE
 * */
extern dbg_file_t* binfile_parse_dbg(binfile_t* bf);

/**
 * Adds an interval representing an empty space to a file
 * \param bf The binary file
 * \param start Starting address of the empty space
 * \param end Ending address of the empty space. If set to -1 an interval of infinite size is created
 * */
/**\todo TODO (2015-02-19) Depending on how the intervals are eventually handled, this function may have to go*/
extern void binfile_addemptyspace(binfile_t* bf, int64_t start, int64_t end);

///**
// * Flags empty intervals as reachable from the code using either branch instructions or data references
// * \param bf Binary file. Its empty spaces must have already been computed
// * \param start Lowest address to flag. If this address is inside an interval, it will be split.
// * \param end Highest address to flag. If this address is inside an interval, it will be split.
// * \param reachable Flag specifying how the addresses are reachable from original code (using INTERVAL_REACHABLE_* flags)
// * */
//extern void binfile_flagemptyspaces_reachable(binfile_t* bf, int64_t start, int64_t end, uint8_t reachable);
//
///**
// * Flags the intervals in a file depending on their reachable status.
// * \param bf Binary file
// * \param reachable Flag specifying how this interval is reachable from the original code (using INTERVAL_REACHABLE_* flags)
// * \param flag Flag to set on the intervals reachable as specified by the \c reachable flag.
// * \param override If set to TRUE, allows to flag intervals already flagged as something else than INTERVAL_TYPE_NONE.
// * Otherwise, those intervals will not be flagged
// * \param maxsize Maximum size of intervals to flag. If the size is reached in the middle of an interval,
// * it will be split. Can be set to UINT64_MAX if all available size is to be flagged
// * \return The total size of the flagged empty spaces
// * */
//extern uint64_t binfile_flagemptyspaces_byreach(binfile_t* bf, unsigned reachable, intervaltype_t flag, int override, uint64_t maxsize);

/**
 * Initialises a new binary file structure as a copy of another that will be used for patching.
 * \param bf The original binary file
 * \return A binary file referencing \c bf as its creator or NULL if an error occurred (file could not be created for instance)
 * */
extern binfile_t* binfile_patch_init_copy(binfile_t* bf);

/**
 * Retrieves a section in a file being patched. If it has not yet been duplicated from the original, the section
 * from the original will be returned.
 * \warning This function is intended solely when wanting to test some properties on a section whether it has been duplicated
 * or not, NOT when modifying it (\ref binfile_patch_get_scn_copy must be used instead)
 * \param bf The binary file. Must be in the process of being patched
 * \param scnid The index of the section
 * \return A pointer to the new section, or NULL if \c bf is NULL, not in the process of being patched, or \c scnid invalid
 * */
extern binscn_t* binfile_patch_get_scn(binfile_t* bf, uint16_t scnid);

/**
 * Retrieves an entry in a file being patched. If it has not yet been duplicated from the original, the entry
 * from the original will be returned.
 * \warning This function is intended solely when wanting to test some properties on an entry whether it has been duplicated
 * or not, NOT when modifying it (\ref binfile_patch_get_scn_entrycopy must be used instead)
 * \param bf The binary file. Must be in the process of being patched
 * \param scnid The index of the section
 * \param entryid The index of the entry
 * \return A pointer to the new entry or the original, or NULL if \c bf is NULL, not in the process of being patched, or \c scnid or \c entryid invalid
 * */
extern data_t* binfile_patch_get_scn_entry(binfile_t* bf, uint16_t scnid,
      uint32_t entryid);

/**
 * Returns the duplicated entry for a given entry
 * \param bf The binary file. It must be in the process of being patched
 * \param entry An existing entry in the original binary file
 * \return The entry in the patched file, or NULL if bf or entry is NULL or entry was not found in the patched file or its creator
 * */
extern data_t* binfile_patch_get_entry_copy(binfile_t* bf, data_t* entry);

/**
 * Retrieves an entry from a section in a file being patched, duplicating it from the original if needed.
 * This is intended to be used when the entry will be modified
 * \param bf The binary file
 * \param scnid The index of the section
 * \param entryid The index of the entry
 * \return A pointer to the entry, or NULL if \c bf is NULL or not in the process of being patched
 * */
extern data_t* binfile_patch_get_scn_entrycopy(binfile_t* bf, uint16_t scnid,
      uint32_t entryid);

/**
 * Adds a code section to a binary file.
 * \param bf Structure representing a binary file being patched
 * \param name Name of the section. If left to NULL, the default name for code sections from the format will be used
 * \param address Address at which the section must be set. If set to -1, its address will be computed later
 * \param size Size in bytes of the section
 * \return Pointer to the new section or NULL if bf is NULL or not in the process of being patched
 * */
extern binscn_t* binfile_patch_add_code_scn(binfile_t* bf, char* name,
      int64_t address, uint64_t size);

/**
 * Adds a code section at a fixed address to a binary file.
 * \param bf Structure representing a binary file being patched
 * \param name Name of the section. If left to NULL, the default name for code sections from the format will be used
 * \param address Address at which the section must be set. If set to -1, its address will be computed later
 * \param size Size in bytes of the section
 * \return Pointer to the new section or NULL if bf is NULL or not in the process of being patched
 * */
extern binscn_t* binfile_patch_add_code_scn_fixed_addr(binfile_t* bf, char* name,
      int64_t address, uint64_t size);

/**
 * Adds a data section to a binary file.
 * \param bf Structure representing a binary file being patched
 * \param name Name of the section. If left to NULL, the default name for code sections from the format will be used
 * \param address Address at which the section must be set. If set to -1, its address will be computed later
 * \param size Size in bytes of the section
 * \return Pointer to the new section or NULL if bf is NULL or not in the process of being patched
 * */
extern binscn_t* binfile_patch_add_data_scn(binfile_t* bf, char* name,
      int64_t address, uint64_t size);

/**
 * Adds a relocation to a file being patched
 * \param bf Structure representing a binary file being patched
 * \param scnid Index of the section where to add the relocation (must be of type SCNT_RELOC)
 * \param label Structure representing the label to link to the relocation
 * \param addr Address of the object concerned by the relocation
 * \param dest Destination of the relocation
 * \param type Type of the relocation pointer
 * \param target_type Type of the destination of the relocation
 * \param reltype Format specific type of the relocation
 * \return TRUE if successful, FALSE if \c bf is NULL or not in the process of being patched
 * */
extern int binfile_patch_add_reloc(binfile_t* bf, uint16_t scnid, label_t* label,
      int64_t addr, void* dest, unsigned type, unsigned target_type,
      uint32_t reltype);

/**
 * Adds a label to a file being patched
 * \param bf Structure representing a binary file being patched
 * \param label Structure representing the label to add
 * \return EXIT_SUCCESS if successful, error code if \c bf is NULL or not in the process of being patched
 * */
extern int binfile_patch_add_label(binfile_t* bf, label_t* label);

/**
 * Adds an entry to a string table in a section of a file being patched
 * \param bf Structure representing a binary file being patched
 * \param str The string to add
 * \param scnid The index of the section where to insert this entry
 * \return A pointer to the corresponding added entry if successful,
 * NULL if \c bf is NULL, not in the process of being patched, or if the section at scnid does not contain strings
 * */
extern data_t* binfile_patch_add_str_entry(binfile_t* bf, char* str,
      uint16_t scnid);

/**
 * Adds an entry to a section in a file being patched.
 * \param bf Structure representing a binary file being patched
 * \param entry The entry to add
 * \param scnid The index of the section where to insert this entry
 * \return TRUE if successful, FALSE if \c bf is NULL or not in the process of being patched
 * */
extern int binfile_patch_add_entry(binfile_t* bf, data_t* entry, uint16_t scnid);

/**
 * Adds a new segment to a binary file in the process of being patched
 * \param bf The binary file. It must be being patched
 * \param attrs Attributes to set on the segment
 * \param align Alignment of the new segment
 * \return A pointer to an empty new segment, or NULL if \c bf is NULL or not being patched
 * */
extern binseg_t* binfile_patch_add_seg(binfile_t* bf, unsigned int attrs,
      uint64_t align);

/**
 * Retrieves a section in a file being patched, duplicating it from the original if needed.
 * This is intended to be used when the section will be modified
 * \param bf The binary file
 * \param scnid The index of the section
 * \return A pointer to the new section, or NULL if \c bf is NULL or not in the process of being patched
 * */
extern binscn_t* binfile_patch_get_scn_copy(binfile_t* bf, uint16_t scnid);

/**
 * Adds an external library to a file
 * \param bf A binary file. Must be in the process of being patched
 * \param extlibname Name of the file
 * \param priority Set to TRUE if it must have priority over the others, FALSE otherwise
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int binfile_patch_add_ext_lib(binfile_t* bf, char* extlibname,
      boolean_t priority);

/**
 * Adds an external function to a file
 * \param bf A binary file. Must be in the process of being patched
 * \param fctname Name of the function
 * \param libname Name of the library where it is defined
 * \param preload Set to TRUE if the function address is preloaded, FALSE otherwise (lazy binding)
 * \return A pointer to the function or NULL if an error occurred
 * */
extern pointer_t* binfile_patch_add_ext_fct(binfile_t* bf, char* fctname,
      char* libname, int preload);

/**
 * Renames an existing library in a file
 * \param bf A binary file. Must be in the process of being patched
 * \param oldname Name of the library
 * \param newname New name of the library
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int binfile_patch_rename_ext_lib(binfile_t* bf, char* oldname,
      char* newname);

///**
// * Modifies an entry in a section in a file being patched.
// * \param bf Structure representing a binary file being patched
// * \param entry The entry to set
// * \param scnid The index of the section where to insert this entry
// * \param entryid Index of the entry to replace with \c entry
// * \return TRUE if successful, FALSE if \c bf is NULL or not in the process of being patched
// * */
//extern int binfile_patch_modifentry(binfile_t* bf, data_t* entry, uint16_t scnid, uint32_t entryid);

/**
 * Finds the offset of a given entry in a section belonging to a patched file
 * \param bf Structure representing a binary file being patched
 * \param scnid Index of the section in the patched file
 * \param creatorscnid Index of the section this
 * \return Offset in the section at which the entry is found,
 * or OFFSET_ERROR if it does not belong to the section or \c scn or \c entry is NULL
 * */
extern uint32_t binfile_patch_find_entry_offset_in_scn(binfile_t* bf, uint16_t scnid,
      data_t* entry);

/**
 * Checks if a section has been impacted by of a patching operation
 * \param bf Binary file. Must be being patched
 * \param scnid Index of the section
 * \return TRUE if the section is new or has a different size than the section at the same index in the original, FALSE otherwise
 * */
extern int binfile_patch_is_scn_bigger(binfile_t* bf, uint16_t scnid);

/**
 * Function testing if a patched section can be moved to an interval representing an empty space in the file.
 * If the section can be moved, its address will be updated and the SCNA_PATCHREORDER attribute will be set for it.
 * \param bf Binary file. Must be being patched
 * \param scnid Index of the section
 * \param interval Interval where we attempt to move the section.
 * \return Interval representing a subrange of addresses inside \c interval and where the section can be moved,
 * or NULL if the section can't be moved into this interval or does not need to be moved
 * */
extern interval_t* binfile_patch_move_scn_to_interval(binfile_t* bf, uint16_t scnid,
      interval_t* interval);

/**
 * Checks if the address of a section has changed from the original because of a patching operation
 * \param bf Binary file, must be being patched
 * \param scnid Index of the section
 * \return TRUE if the section is flagged as reordered (SCNA_PATCHREORDER) and has a different address
 * than in the original file or is new, FALSE otherwise
 * */
extern int binfile_patch_is_scn_moved(binfile_t* bf, uint16_t scnid);

/**
 * Checks if a binfile is being patched. Intended to factorise tests done at the beginning of each binfile_patch_* function
 * \param bf Binary file
 * \return TRUE if the bf is not NULL, is flagged as being patched, and has a non NULL creator
 * */
extern int binfile_patch_is_patching(binfile_t* bf);

/**
 * Checks if a binfile is in the middle of a patching session. Intended to factorise tests done at the beginning of each binfile_patch_* function
 * \param bf Binary file
 * \return TRUE if the bf is not NULL, is flagged as being patched or finalised, and has a non NULL creator
 * */
extern int binfile_patch_is_valid(binfile_t* bf);

/**
 * Reorders the sections in a binary file according to their offset
 * \param bf The binary file. It must be being patched
 * \return EXIT_SUCCESS if successful, error code if \c is NULL or not being patched
 * */
extern int binfile_patch_reorder_scn_by_offset(binfile_t* bf);

/**
 * Attempts to create the patched file
 * \param bf Binary file
 * \param newfilename Name under which to save the file
 * \return EXIT_SUCCESS if the file could be created, error code otherwise
 * */
extern int binfile_patch_create_file(binfile_t* bf, char* newfilename);

/**
 * Finalises a patched file
 * \param bf Binary file
 * \param spaces Queue of intervals representing the remaining empty spaces in the file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int binfile_patch_finalise(binfile_t* bf, queue_t* spaces);

/**
 * Writes a finalised binary file and closes the stream
 * \param bf A binary file. Must be finalised
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int binfile_patch_write_file(binfile_t* bf);

/**
 * Terminates a file being patched
 * \param bf A binary file. Must be in the process of being patched or finalised
 * \return TRUE if successful, FALSE otherwise
 * */
extern int binfile_patch_terminate(binfile_t* bf);
///////////////////////////////////////////////////////////////////////////////
//                                  asmfile                                  //
///////////////////////////////////////////////////////////////////////////////
/**
 * Enum specifying the type of origin of an asmfile
 * */
typedef enum asmfile_origin_type_e {
   ASMF_ORIGIN_UNKNOWN = 0, /**<Unknown type*/
   ASMF_ORIGIN_BIN, /**<File origin is a binary file (stored in the binfile member)*/
   ASMF_ORIGIN_TXT, /**<File origin is a formatted text file (stored in the txtfile member)*/
   ASMF_ORIGIN_ASM, /**<File origin is an assembly file*/
   ASMF_ORIGIN_MAX /**<Maximum number of possible origins*/
} asmfile_origin_type_t;
/**\todo (2015-07-03) In the patcher refactor branch, change ASMF_ORIGIN_ELF to ASMF_ORIGIN_BIN, and move the binfile member to an union
 * that will also contain txtfile*/

/**
 * Structure describing the names of the fields describing the elements of an asm file when parsed from a text file
 * */
typedef struct asm_txt_fields_s {
   char* insnfieldnames[INSNF_MAX]; /**<Names of the fields corresponding to characteristics of instructions*/
   char* labelfieldnames[LBLF_MAX]; /**<Names of the fields corresponding to characteristics of labels*/
   char* blockfieldnames[BLOCKF_MAX]; /**<Names of the fields corresponding to characteristics of blocks*/
   char* loopfieldnames[LOOPF_MAX]; /**<Names of the fields corresponding to characteristics of loops*/
   char* fctfieldnames[FCTF_MAX]; /**<Names of the fields corresponding to characteristics of functions*/
   char* filefieldnames[TXTFILEF_MAX]; /**<Names of the fields corresponding to characteristics of the file*/
   char* archfieldnames[ARCHF_MAX]; /**<Names of the fields corresponding to characteristics of the architecture*/
   char* scninsns; /**<Name of the section type in the file containing instructions*/
   char* scnfctlbls; /**<Name of the section type in the file containing labels definitions to be used as function labels*/
   char* scnbrchlbls; /**<Name of the section type in the file containing labels definitions to be used as branch destinations*/
   char* scnblocks; /**<Name of the section type in the file containing blocks definitions*/
   char* scnloops; /**<Name of the section type in the file containing loops definitions*/
   char* scnendloops; /**<Name of the section type in the file containing end of loops tags*/
   char* scnfcts; /**<Name of the section type in the file containing functions definitions*/
   char* scnfile; /**<Name of the section type in the file containing file characteristics definitions*/
   char* scnarch; /**<Name of the section type in the file containing architecture definitions*/
} asm_txt_fields_t;

/**
 * Structure storing the origin of an asmfile parsed from a formatted text file
 * */
typedef struct asm_txt_origin_s {
   txtfile_t* txtfile; /**<Structure containing the result of the parsed file*/
   asm_txt_fields_t* fields; /**<Structure describing the names of the fields*/
} asm_txt_origin_t;

/**
 * \struct asmfile_s
 * \brief Structure describing an asmfile.
 */
struct asmfile_s {
   void* params[_NB_PARAM_MODULE][_NB_OPT_BY_MODULE]; /**< Store parameters for some internal parts*/
   queue_t *insns; /**< A queue of instruction*/
   queue_t *insns_gaps; /**< A queue designating the gaps in the instructions.
    Contains a pointer to the list item just after the gap (in insns).
    Should remain empty for a linear disassembly. Its elements are
    ordered by increasing address. Later, some merging should be done.*/
   insn_t** insns_table; /**< A table of instructions sorted by address, used to improve instruction search*/
   hashtable_t *label_table; /**< A table with all labels, indexed by the name*/
   list_t* dbg_list; /**< A list with all debug data (linked to instructions). Used to reduce memory consumption*/
   queue_t* label_list; /**< Used to decrease time of an instruction research, sorted by label address*/
   label_t** fctlabels; /**< Array of labels eligible to be associated to instructions. This is a subset of the labels in label_list*/
   label_t** varlabels; /**< Array of labels eligible to be associated to data entries representing variables.  This is a subset of the labels in label_list*/
   queue_t *functions; /**< A queue of functions*/
   hashtable_t* ht_functions; /**< A table of functions indexed by name*/
   queue_t* datas; /**< List of data elements in the file, represented as data_t structures, ordered by address*/
   /**\todo TODO (2014-11-20) Not sure if \c datas has to be here or remain in binfile_t. We *could* copy here all data_t structures from
    * the file and order them by address, but this is something rather easily done by looping on the loaded sections from the binary file
    * and accessing their data. To be decided whether we need to do this often (and the queue in asmfile_t is useful to build) or not (and
    * then we can simply scan the sections and their data entries)*/
   char *name; /**< Binary name*/
   unsigned int n_loops; /**< Number of loop in the file*/
   unsigned int n_insns; /**< Number of instructions in the file*/
   unsigned int n_blocks; /**< Number of blocks in the file*/
   unsigned int n_functions; /**< Number of functions in the file*/
   arch_t* arch; /**< Pointer to the architecture structure for this file*/
   proc_t* proc; /**< Pointer to the structure specifying the processor version for which the file is analysed*/
   project_t* project; /**< Pointer to the container structure*/
   union {
      binfile_t* binfile; /**< Parsed binary file from which this structure originates*/
      asm_txt_origin_t* txtorigin; /**< Parsed formatted text file from which this structure originates*/
   } origin; /**< Contains the structure representing the file from which this asmfile was built*/
   void (*free_disass)(void*); /**< Function used to free data generated during disassembling*/
   hashtable_t* branches_by_target_insn; /**< Hashtable containing the branch instructions, indexed by the address they point to
    \todo This element may be declared in another structure
    \todo TODO (2014-07-10) Rename this table to references. It will now be used to store all insn_t structures
    containing a pointer_t operand. This will include those pointing to data_t structures
    => (2014-11-20) No, no, no. This table will remain as it is. We will have to create another one for those
    pointing to data structures. And possibly index on the object, not the address. I think
    => (2014-12-03) YES WE WILL*/
   hashtable_t* insn_ptrs_by_target_data; /**< Hashtable containing the instructions referencing a data, indexed by the data_t object referenced*/
   hashtable_t* data_ptrs_by_target_insn; /**< Hashtable containing the data referencing an instruction, indexed by the insn_t object referenced*/
   /**\todo TODO (2014-11-21) Use the two hashtable above (create them, free them, update them, etc). Also change the index of the hashtable branches to use insn_t instead of addrs
    * => (2015-05-22) Has been done some time ago. Check if the tables are freed*/
   list_t* plt_fct; /**< Contains a list of plt functions. used to remove them from memory*/
   uint8_t* used_isets; /**< Array specifying which instruction set are used in this file. Size is arch->nb_isets*/
   int analyze_flag; /**< A list of flags used to know which analyses have been done*/
   int maxid_loop; /**< Maximal ID for a loop in this file*/
   int maxid_block; /**< Maximal ID for a block in this file*/
   int maxid_fct; /**< Maximal ID for a function in this file*/
   unsigned char* (*getbytes)(asmfile_t*, int64_t, int); /**<Return the bytes from a section.*/
   char comp_code; /**<Specify the compiler used to compile this binary (must be 0 or DEM_COMP_ macro)*/
   char lang_code; /**<Specify the source language (must be 0 or DEM_LANG_ macro)*/
   dbg_file_t* debug; /**<Debug data*/
   void (*unload_dbg)(asmfile_t*); /**<Function to unload debug data*/
   void (*load_fct_dbg)(fct_t*); /**<Function to load debug data into functions*/
   void (*free_ssa)(fct_t*); /**<Function to free SSA data*/
   void (*free_polytopes)(fct_t*); /**<Function to free polytopes results*/
   void (*free_live_registers)(fct_t*); /**<Function to free live registers results*/
   unsigned int n_fctlabels; /**< Size of the \c fctlabels array*/
   unsigned int n_varlabels; /**< Size of the \c varlabels array*/
   int last_error_code; /**<Last error code encountered*/
   uint8_t origin_type; /**< Specifies the type of structure present in origin*/
};

/**
 * \struct dbg_file_s
 * \brief This structure contains debug data for asmfile
 */
struct dbg_file_s {
   void* data; /**<Original debug data*/
   char** command_line; /**<An array of string, each entry is a element of the command line*/
   char* command_line_linear; /**<All elements from command_line in a single string*/
   char format; /**<Format of debug data (DWARF, ...)*/
   unsigned int nb_command_line; /**<Number of elements in command_line*/
};

/**
 * Create a new empty asmfile
 * \param asmfile_name name of the file (must be not null)
 * \return a new asmfile, or PTR_ERROR is asmfile_name is NULL
 */
extern asmfile_t* asmfile_new(char* asmfile_name);

/**
 * Prints the list of instructions in an ASM file
 * \param asmfile The ASM file to print
 * \param stream The stream to print to
 * \param startaddr The address at which printing must begin. If 0 or negative, the first address in the instruction list will be taken
 * \param stopaddr The address at which printing must end. If 0 or negative, the last address in the instruction list will be taken
 * \param printaddr Prints the address before an instruction
 * \param printcoding Prints the coding before an instruction
 * \param printlbl Prints the labels (if present)
 * \param before Function to execute before printing an instruction (for printing additional informations)
 * \param after Function to execute after printing an instruction (for printing additional informations)
 */
extern void asmfile_print_insns(asmfile_t* asmfile, FILE* stream,
      maddr_t startaddr, maddr_t stopaddr, int printlbl, int printaddr,
      int printcoding, void (*before)(asmfile_t*, insn_t*, FILE*),
      void (*after)(asmfile_t*, insn_t*, FILE*));

/**
 * Delete an existing asmfile and all data it contains
 * \param p a pointer on an asmfile to free
 */
extern void asmfile_free(void* p);

/**
 * Update counters such as n_loops in an existing asmfile
 * \param asmfile an existing asmfile
 */
extern void asmfile_update_counters(asmfile_t* asmfile);

/**
 * Set the queue of instructions of an asmfile.
 * \param asmfile an asmfile
 * \param insns a queue of insn_t*
 */
extern void asmfile_set_insns(asmfile_t* asmfile, queue_t* insns);

/**
 * Sets the text file for an asmfile
 * \param asmfile An asmfile
 * \param tf The formatted text file structure
 * \param fieldnames The structure containing the names of the fields
 */
extern void asmfile_set_txtfile(asmfile_t* asmf, txtfile_t* tf,
      asm_txt_fields_t* fieldnames);

/**
 * Sets the binary file for an asmfile
 * \param f An asmfile
 * \param bf The binary file structure
 * */
extern void asmfile_set_binfile(asmfile_t* f, binfile_t* bf);

/**
 * Resets the origin of an asmfile
 * \param f An asmfile
 * */
extern void asmfile_clearorigin(asmfile_t* f);

/**
 * Sets the debug information for an asmfile
 * \param f The asmfile
 * \param df The structure describing the debug information
 * */
extern void asmfile_setdebug(asmfile_t* f, dbg_file_t* df);

/**
 * Sets the architecture for an asmfile
 * \param asmfile An asmfile
 * \param arch Pointer to the instance of the architecture structure for this architecture
 */
extern void asmfile_set_arch(asmfile_t* asmfile, arch_t* arch);

/**
 * Sets the pointer to the operand extension printer function for an asmfile
 * \param asmfile An asmfile
 * \param print Pointer to the operand extension printer function
 */
extern void asmfile_set_oprnd_ext_print(asmfile_t* asmfile,
      void (*print)(oprnd_t*, char*, arch_t*));

/**
 * Sets the processor version of an asmfile
 * \param asmfile An asmfile
 * \param proc a processor version
 */
extern void asmfile_set_proc(asmfile_t* asmfile, proc_t* proc);

/**
 * Updates the flag for performed analysis to adds a new analysis step to the file
 * \param asmfile The asmfile
 * \param analyzis_flag The flag to add
 */
extern void asmfile_add_analyzis(asmfile_t* asmfile, int analyzis_flag);

/**
 * Gets an asmfile project
 * \param asmfile an asmfile
 * \return a project or PTR_ERROR if there is a problem
 */
extern project_t* asmfile_get_project(asmfile_t* asmfile);

/**
 * Gets an asmfile name
 * \param asmfile an asmfile
 * \return a string or PTR_ERROR if there is a problem
 */
extern char* asmfile_get_name(asmfile_t* asmfile);

/**
 * Gets functions for an asmfile
 * \param asmfile an asmfile
 * \return a queue of functions or PTR_ERROR if there is a problem
 */
extern queue_t* asmfile_get_fcts(asmfile_t* asmfile);

/**
 * Get the list of plt functions
 * \param asmfile The asmfile to look into
 * \return a list that contains plt's functions name or PTR_ERROR if there is a problem.
 */
extern list_t* asmfile_get_fct_plt(asmfile_t* asmfile);

/**
 * Gets instructions of an asmfile
 * \param asmfile an asmfile
 * \return a queue of instructions or PTR_ERROR if there is a problem
 */
extern queue_t* asmfile_get_insns(asmfile_t* asmfile);

/**
 * Gets the array of labels associated to functions in an asmfile
 * \param asmfile The asmfile
 * \param nfctlabels Return parameter. Will contain the size of the array if not NULL
 * \return A pointer to the array of labels associated to functions or NULL if asmfile is NULL
 * */
extern label_t** asmfile_get_fct_labels(asmfile_t* asmfile,
      unsigned int* nfctlabels);

/**
 * Gets the array of labels associated to variables in an asmfile
 * \param asmf The asmfile
 * \param nfctlabels Return parameter. Will contain the size of the array if not NULL
 * \return A pointer to the array of labels associated to variables or NULL if asmf is NULL
 * */
extern label_t** asmfile_getvarlabels(asmfile_t* asmf, unsigned int* nvarlabels);

/**
 * Gets the positions of gaps between decompiled instructions of an
 * asmfile.
 * \param asmfile an asmfile
 * \return a queue of list_t of instructions from the instruction list
 * or PTR_ERROR if there is a problem. If it returns an empty queue, any
 * a instructions should be added to the end of the instruction queue.
 * */
extern queue_t* asmfile_get_insns_gaps(asmfile_t* asmfile);

/**
 * Finds an instruction in an asmfile depending on its address
 * \param asmfile The asmfile to look into
 * \param addr The address at which an instruction must be looked for
 * \return A pointer to the instruction at this address, or NULL if no address was found or an error occurred
 * */
extern insn_t* asmfile_get_insn_by_addr(asmfile_t* asmfile, maddr_t addr);

/**
 * Finds an instruction in an asmfile depending on its label
 * \param asmfile The asmfile to look into
 * \param lblname The name of the label at which an application is looked for
 * \return A pointer to the instruction at this label, or NULL if no such label was found or an error occurred
 * */
extern insn_t* asmfile_get_insn_by_label(asmfile_t* asmfile, char* lblname);

/**
 * Retrieves a pointer to the list of labels in a file
 * \param asmfile The asmfile to look into
 * \return The queue containing the labels of the asmfile, or NULL if problem.
 * */
extern queue_t* asmfile_get_labels(asmfile_t* asmfile);

/**
 * Finds the first label before the given address in an asmfile
 * \param asmfile The asmfile
 * \param addr The address at which the label must be searched
 * \param lastcontainer If not pointing to a NULL pointer, will be used as the starting
 * point for the search in the list of ordered labels, and will be updated to contain the container
 * of the label found
 * \return If there is a label at the given address, will return it, otherwise returns the first label found
 * immediately before the \e addr and after the label contained in \e lastcontainer (or the beginning of
 * the label list if NULL). If the address is lower than the address of the starting point, the address of the
 * starting point will be returned
 */
extern label_t* asmfile_get_last_label(asmfile_t* asmfile, maddr_t addr,
      list_t** lastcontainer);

/**
 * Finds the label at a given address which matches the searched string
 * \param asmf The asmfile
 * \param addr The address at which the label must be searched
 * \param name The name or a part of the label's name searched
 * \param container If not pointing to a NULL pointer, will be used as the starting
 * point for the search in the list of ordered labels, and will be updated to contain the container
 * of the label found
 * \return If there is a label at the given address which match the searched string, will return it.
 * If the address is lower than the address of the starting point, NULL will be returned
 */
extern label_t* asmfile_getlabel_byaddressandname(asmfile_t* asmf, int64_t addr,
      char* name, list_t** container);

/**
 * Finds the first label eligible to be associated to an instruction before the given address in an asmfile
 * \param asmfile The asmfile
 * \param addr The address before which we look for a label
 * \param lblidx Return parameter. If not NULL, will contain the index of the label in the array, or -1 if NULL is returned
 * \return The label eligible to be associated to an instruction whose address is inferior or equal to \c addr, or NULL
 * if the file does not contain any function labels or if the address is below the first label
 * */
extern label_t* asmfile_get_last_fct_label(asmfile_t* asmfile, maddr_t addr,
      int* lblidx);

/**
 * Adds to an asmfile the labels from external functions at the location of the corresponding stubs
 * \param asmf The asmfile
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int asmfile_add_ext_labels(asmfile_t* asmf);

/**
 * Gets the number of instructions in an asmfile
 * \param asmfile an asmfile
 * \return number of instructions
 */
extern int asmfile_get_nb_insns(asmfile_t* asmfile);

/**
 * Gets the number of blocks in an asmfile
 * \param asmfile An asmfile
 * \return number of blocks
 */
extern int asmfile_get_nb_blocks(asmfile_t* asmfile);

/**
 * Gets the number of blocks in an asmfile, excluding virtual blocks
 * \param asmfile An asmfile
 * \return number of blocks
 */
extern int asmfile_get_nb_blocks_novirtual(asmfile_t* asmfile);

/**
 * Gets the number of loops in an asmfile
 * \param asmfile An asmfile
 * \return Aumber of loops
 */
extern int asmfile_get_nb_loops(asmfile_t* asmfile);

/**
 * Gets the number of functions in an asmfile
 * \param asmfile An asmfile
 * \return Number of functions
 */
extern int asmfile_get_nb_fcts(asmfile_t* asmfile);

///**
// * Gets the origin structure of an asmfile
// * \param asmf an asmfile
// * \return Pointer to the origin structure
// */
//extern void* asmfile_getorigin (asmfile_t* asmf);

/**
 * Gets the binary file from which an asmfile is built
 * \param asmfile an asmfile
 * \return Pointer to the origin structure if it is a binary file, NULL otherwise
 */
extern binfile_t* asmfile_get_binfile(asmfile_t* asmfile);

/**
 * Gets the origin structure of an asmfile if it's a formatted text file
 * \param asmfile an asmfile
 * \return Pointer to the origin structure if it is a formatted text file, NULL otherwise
 */
extern txtfile_t* asmfile_get_txtfile(asmfile_t* asmfile);

/**
 * Retrieves the structure describing the name of the fields used in an asmfile created from a formatted assembly file
 * \param asmfile The asmfile
 * \return A structure describing the names of the fields used in the formatted assembly file represented by the asmfile,
 * or NULL is \c asmfile is NULL or not parsed from a formatted assembly file
 * */
extern asm_txt_fields_t* asmfile_get_txtfile_field_names(asmfile_t* asmfile);

/**
 * Gets the txtfile_t structure contained in the origin structure of an asmfile if it's a formatted text file
 * \param asmf an asmfile
 * \return Pointer to the structure representing the formatted text file from the origin structure if
 * the asmfile represents a formatted assembly file, NULL otherwise
 */
extern asm_txt_origin_t* asmfile_get_txt_origin(asmfile_t* asmf);

/**
 * Gets the type of the origin structure of an asmfile
 * \param asmfile an asmfile
 * \return Identifier of the origin structure
 */
extern uint8_t asmfile_get_origin_type(asmfile_t* asmfile);

/**
 * Gets the architecture of an asmfile
 * \param asmfile An asmfile
 * \return A pointer to the structure describing the architecture of the asmfile
 */
extern arch_t* asmfile_get_arch(asmfile_t* asmfile);

/**
 * Gets the architecture name of an asmfile
 * \param asmfile An asmfile
 * \return The name of the architecture of the asmfile
 * */
extern char* asmfile_get_arch_name(asmfile_t* asmfile);

/**
 * Gets the architecture code of an asmfile
 * \param asmfile An asmfile
 * \return The code of the architecture of the asmfile as defined in arch.h
 * */
extern char asmfile_get_arch_code(asmfile_t* asmfile);

/**
 * Gets the table containing the branches in an asmfile
 * \param asmfile The asmfile
 * \return A pointer to its table containing branch instructions (indexed on their destination), or NULL if asmfile is NULL
 * */
extern hashtable_t* asmfile_get_branches(asmfile_t* asmfile);

/**
 * Gets the processor version of an asmfile
 * \param asmfile An asmfile
 * \return the processor version associated to the project
 */
extern proc_t* asmfile_get_proc(asmfile_t* asmfile);

/**
 * Gets the name of a processor version of a asmfile
 * \param asmfile an asmfile
 * \return the name of the processor version associated to the asmfile
 */
extern char* asmfile_get_proc_name(asmfile_t* asmfile);

/**
 * Retrieves the name of the micro architecture for an asmfile
 * \param asmfile An asmfile
 * \return The name of the micro architecture associated to the asmfile
 */
extern char *asmfile_get_uarch_name(asmfile_t* asmfile);

/**
 * Retrieves the identifier of the micro architecture for an asmfile
 * \param asmfile An asmfile
 * \return The identifier of the micro architecture associated to the asmfile
 * */
extern unsigned int asmfile_get_uarch_id(asmfile_t* asmfile);

/**
 * Finds a label in an asmfile depending on its name
 * \param asmfile The asmfile to look into
 * \param lblname The name of the label
 * \return A pointer to the label with this name, or NULL if no such label was found
 * */
extern label_t* asmfile_lookup_label(asmfile_t* asmfile, char* lblname);

/**
 * Checks if a given analysis was performed on the file
 * \param asmfile The asmfile
 * \param flag The flag to test for
 * \return TRUE if the analysis specified by the flag has already been
 * performed, FALSE otherwise, and PTR_ERROR if asmfile is NULL
 */
extern int asmfile_test_analyze(asmfile_t* asmfile, int flag);

/**
 * Returns the code of the last error encountered and resets it
 * \param asmfile The asmfile
 * \return The last error code encountered. It will be reset to EXIT_SUCCESS afterward.
 * If \c asmfile is NULL, ERR_LIBASM_MISSING_ASMFILE will be returned
 * */
extern int asmfile_get_last_error_code(asmfile_t* asmfile);

/**
 * Sets the code of the last error encountered.
 * \param asmfile The asmfile
 * \param error_code The error code to set
 * \return ERR_LIBASM_MISSING_ASMFILE if \c asmfile is NULL, or the previous error value of the last error code in the asmfile
 * */
extern int asmfile_set_last_error_code(asmfile_t* asmfile, int error_code);

/**
 * Retrieves the number of archive elements contained in an asmfile
 * \param asmf The asmfile
 * \return The number of archive elements contained in the file if it has been parsed (PAR_ANALYZE) and is an archive, 0 otherwise
 * */
extern uint16_t asmfile_get_nb_archive_members(asmfile_t* asmf);

/**
 * Retrieves the asmfile associated to a given archive element in an asmfile
 * \param asmf The asmfile
 * \param i Index of the archive element to retrieve
 * \return The asmfile associated to the archive member at the given index, or NULL if asmf is NULL, has not been parsed (PAR_ANALYZE),
 * is not an archive, or if i is bigger than its number of archive members
 * */
extern asmfile_t* asmfile_get_archive_member(asmfile_t* asmf, uint16_t i);

/**
 * Checks if an asmfile is an archive
 * \param asmf The asmf
 * \return TRUE if the file has been parsed and its associated binary file is an archive, FALSE otherwise
 * */
extern int asmfile_is_archive(asmfile_t* asmf);

/**
 * Looks for functions exits
 * \param asmfile an asmfile containing functions to analyze
 */
extern void asmfile_detect_end_of_functions(asmfile_t* asmfile);

/**
 * Looks for functions rangess
 * \param asmfile an asmfile containing functions to analyze
 */
extern void asmfile_detect_ranges(asmfile_t* asmfile);

/**
 * Add a label into an asmfile
 * \param lab an existing label
 * \param asmfile an existing asmfile
 * \note This function is the one to use when adding labels to a file after asmfile_upd_labels has been invoked for this asmfile
 * This should be the case for all files whose PAR_ANALYZE flag is set
 */
extern void asmfile_add_label(asmfile_t* asmfile, label_t* lab);

/**
 * Add a label into an asmfile, without sorting
 * \param lab an existing label
 * \param asmfile an existing asmfile
 * \note This function is the one to use when adding labels to a file before amsfile_updatelabels has been invoked for this asmfile
 * This should be the case for files whose PAR_ANALYZE flag is not set
 */
extern void asmfile_add_label_unsorted(asmfile_t* asmfile, label_t* lab);

/**
 * Indexes a branch instruction with its destination in an asmfile_t
 * \param asmf The asmfile
 * \param branch The branch instruction. It will be added to the table of branches indexed by \c dest.
 * \param dest The instruction targeted by the branch. It will be the index of the branch in the table.
 * \warning For performance reasons, no test is performed on whether \c branch actually points to \c dest.
 * */
extern void asmfile_add_branch(asmfile_t* asmf, insn_t* branch, insn_t* dest);

/**
 * Indexes an instruction with the data it references in an asmfile_t
 * \param asmf The asmfile
 * \param refinsn The instruction referencing a data. It will be added to the table of instructions referencing data indexed by \c dest.
 * \param dest The data referenced by the instruction. It will be the index of the instruction in the table.
 * \warning For performance reasons, no test is performed on whether \c refinsn actually references to \c dest.
 * */
extern void asmfile_add_insn_ptr_to_data(asmfile_t* asmf, insn_t* refinsn, data_t* dest);

/**
 * Indexes a data entry with the instruction it references an asmfile_t
 * \param asmf The asmfile
 * \param refdata The data entry. It will be added to the table of data referencing instructions indexed by \c dest.
 * \param dest The instruction referenced by the entry. It will be the index of the data entry in the table.
 * \warning For performance reasons, no test is performed on whether \c branch actually points to \c dest.
 * */
extern void asmfile_add_data_ptr_to_insn(asmfile_t* asmf, data_t* refdata, insn_t* dest);

/**
 * Retrieves the table of instructions referencing a data structure
 * \param asmf The asmfile
 * \return A table containing all instructions referencing a data structure, indexed by the referenced data,
 * or NULL if \c asmf is NULL
 * */
extern hashtable_t* asmfile_get_insn_ptrs_by_target_data(asmfile_t* asmf);

/**
 * Retrieves the table of data structures referencing an instruction
 * \param asmf The asmfile
 * \return A table containing all data structures referencing an instruction, indexed by the referenced instruction,
 * or NULL if \c asmf is NULL
 * */
extern hashtable_t* asmfile_get_data_ptrs_by_target_insn(asmfile_t* asmf);

/**
 * Reorders the queue label_list only.
 * \note This function should only be needed when a label search based on address must be performed before asmfile_upd_labels has been invoked
 * \param asmfile An existing asmfile
 * */
extern void asmfile_sort_labels(asmfile_t* asmfile);

/**
 * Updates the labels in an asmfile: reorders the queue label_list and builds the fctlabels array containing only
 * labels eligible to be associated with instructions.
 * \param asmfile An existing asmfile
 * */
extern void asmfile_upd_labels(asmfile_t* asmfile);

/**
 * Updates the instructions in an asmfile with regard to branches. This involves identifying the targets of branches and flagging unreachable instructions.
 * \param af The asmfile
 * \param branches A queue of the branch instructions present in the file. It will be emptied upon completion (but not freed)
 * */
extern void asmfile_upd_insns_with_branches(asmfile_t* af, queue_t* branches);

/**
 * Set a paramter in an asmfile
 * \param asmfile an asmfile
 * \param module_id a value from params_modules_id_e enum corresponding to a module
 * \param param_id a value from params_<module>_id_e enum corresponding to the option
 * \param value the value of the parameter
 */
extern void asmfile_add_parameter(asmfile_t* asmfile, int module_id,
      int param_id, void* value);

/**
 * Get a parameter from an asmfile
 * \param asmfile an asmfile
 * \param module_id a value from params_modules_id_e enum corresponding to a module
 * \param param_id a value from params_<module>_id_e enum corresponding to the option
 * \return the corresponding value or NULL
 */
extern void* asmfile_get_parameter(asmfile_t* asmfile, int module_id,
      int param_id);

/**
 * Specifies that a given instruction set is used in this file
 * \param asmfile An asmfile
 * \param iset A code for an instruction set
 * */
extern void asmfile_set_iset_used(asmfile_t* asmfile, unsigned int iset);

/**
 * Checks if a given instruction set is used in this file
 * \param asmfile An asmfile
 * \param iset A code for an instruction set
 * \return TRUE if at least an instruction in this file belongs to the given instruction set, FALSE otherwise
 * */
extern int asmfile_check_iset_used(asmfile_t* asmfile, unsigned int iset);

/**
 * Creates a new structure describing the debug information found in a file
 * \param debug Format specific structure containing the debug information from the file
 * \param format Descriptor of the format
 * \return A new structure containing the debug information from the file, or NULL if \c debug is NULL and \c format is not DBG_NONE
 * */
extern dbg_file_t* dbg_file_new(void* debug, dbgformat_t format);

/**
 * Frees a structure describing the debug information found in a file
 * \param dbg The structure to free
 * */
extern void dbg_file_free(dbg_file_t* dbg);

/**
 * Creates a structure for storing the origin of an asmfile parsed from a formatted assembly file
 * \param txtfile The structure storing the results of the parsing of the formatted file
 * \param fields The structure describing the names of the fields (will be duplicated)
 * \return A new structure containing the description of the origin of the file
 * */
extern asm_txt_origin_t* asm_txt_origin_new(txtfile_t* txtfile,
      const asm_txt_fields_t* fields);

/**
 * Frees a structure storing the origin of an asmfile parsed from a formatted assembly file
 * \param txtorigin The structure
 * */
extern void asm_txt_origin_free(asm_txt_origin_t* txtorigin);

///////////////////////////////////////////////////////////////////////////////
//                                  project                                  //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct project_s
 * \brief Structure that describes a set of asmfile interacting by cross asmfile function calls.
 */
struct project_s {
   void* params[_NB_PARAM_MODULE][_NB_OPT_BY_MODULE]; /**< Store parameters for some internal parts*/
   char** exit_functions; /**< NULL terminated array containing exit functions list*/
   queue_t* asmfiles; /**< List of asmfile*/
   hashtable_t* asmfile_table; /**< Asmfile by their assemblyfile name and by their md5 hash sum: TODO */
   char *file; /**< File with project data*/
   int cg_depth; /**< Maximum number of edges in represented paths */
   char comp_code; /**<Specify the compiler used in the project (must be 0 or DEM_COMP_ macro)*/
   char lang_code; /**<Specify the source language  used in the project (must be 0 or DEM_LANG_ macro)*/
   proc_t* proc; /**< Pointer to the structure specifying the processor version for which all asmfiles in the projects have to be analysed*/
   char* proc_name; /**< Name of the processor version for which the user requested all files in the project to be analysed*/
   char* uarch_name; /**< Name of the micro-architecture for which the user requested all files in the project to be analysed*/
   char cc_mode; /**<Specify the mode used to extract functions from connected components*/
};

/**
 * Creates a new project
 * \param projectfile name of the project (and file which contains project data)
 * \return a new initialized project
 */
extern project_t* project_new(char* projectfile);

/**
 * Sets the list of exit functions
 * \param project a project
 * \param exits a NULL terminated array containing names of exit functions
 * The array and the names must have been allocated through lc_malloc and will be freed at the destruction of the project
 * The function names must not contain any extension indicating their potential dynamic origin (e.g. @plt)
 */
extern void project_set_exit_fcts(project_t* project, char** exits);

/**
 * Appends a list of exit functions to the existing list
 * \param p a project
 * \param exits a NULL terminated array containing names of exit functions
 * The array and the names must have been allocated through lc_malloc and will be freed at the destruction of the project
 * The function names must not contain any extension indicating their potential dynamic origin (e.g. @plt)
 */
extern void project_add_exit_fcts(project_t* p, char** exits);

/**
 * Removes a function from the list of exit functions
 * \param p a project
 * \param exit a function name to be removed from the list.
 * */
extern void project_rem_exit_fct(project_t* p, char* exit);

/**
 * Sets the value of cc_mode member (supported values are CCMODE_.*)
 * \param project a project
 * \param cc_mode value for cc_mode member
 */
extern void project_set_ccmode(project_t* project, char cc_mode);

/**
 * Deletes an existing project
 * \param project a project
 */
extern void project_free(project_t* project);

/**
 * Adds a file into an existing project
 * \param project an existing project
 * \param filename an assembly file to add
 * \return an asmfile structure associated to the given assembly file
 */
extern asmfile_t* project_add_file(project_t* project, char *filename);

/**
 * Duplicates an existing project
 * \param project a project to duplicate
 * \return a copy of project or NULL
 */
extern project_t* project_dup(project_t* project);

/**
 * Removes a file into an existing project
 * \param project an existing project
 * \param asmfile an asmfile to remove from a project
 * \return 1 or 0 if there is a problem
 */
extern int project_remove_file(project_t* project, asmfile_t* asmfile);

/**
 * Gets the asmfiles of a project
 * \param project a project
 * \return asmfiles of project
 */
extern queue_t* project_get_asmfiles(project_t* project);

/**
 * Gets the asmfile table of a project
 * \param project a project
 * \return asmfile table of project
 */
extern hashtable_t* project_get_asmfile_table(project_t* project);

/**
 * Gets the compiler used for a project
 * \param project a project
 * \return compiler code of project
 */
extern char project_get_compiler_code(project_t* project);

/**
 * Gets the language used for a project
 * \param project a project
 * \return language code of project
 */
extern char project_get_language_code(project_t* project);

/**
 * Gets the CG depth of a project
 * \param project a project
 * \return depth of project
 */
extern int project_get_CG_depth(project_t* project);

/**
 * Gets the name of a project
 * \param project a project
 * \return project name
 */
extern char* project_get_name(project_t* project);

/**
 * Gets the processor version of a project
 * \param project a project
 * \return the processor version associated to the project
 */
extern proc_t* project_get_proc(project_t* project);

/**
 * Gets the name of a processor version of a project
 * \param project a project
 * \return the name of the processor version associated to the project
 */
extern char* project_get_proc_name(project_t* project);

/**
 * Gets the name of the uarch of a project
 * \param project a project
 * \return the name of the uarch associated to the project
 */
extern char* project_get_uarch_name(project_t* project);

/**
 * Retrieves the identifier of the micro architecture for a project
 * \param project A project
 * \return The identifier of the micro architecture associated to the project
 * */
extern unsigned int project_get_uarch_id(project_t* project);

/**
 * Gets the architecture associated to a project
 * \param project A project
 * \return The architecture associated to the project, or NULL if \c project is NULL
 * */
extern arch_t* project_get_arch(project_t* project);

/**
 * Gets the CC mode of a project (used to extract functions from connected components)
 * \param project a project
 * \return CC mode of project (CCMODE_.*)
 */
extern char project_get_cc_mode(project_t* project);

/**
 * Gets exits functions of a project
 * \param project a project
 * \return exit functions
 */
extern char** project_get_exit_fcts(project_t* project);

/**
 * Gets the number of instructions for a project
 * \param project a project
 * \return the number of instructions of project
 */
extern int project_get_nb_insns(project_t* project);

/**
 * Gets the number of blocks for a project
 * \param project a project
 * \return the number of blocks of project
 */
extern int project_get_nb_blocks(project_t* project);

/**
 * Gets the number of blocks for a project, excluding virtual blocks
 * \param project a project
 * \return the number of blocks of project
 */
extern int project_get_nb_blocks_novirtual(project_t* project);

/**
 * Gets the number of loops for a project
 * \param project a project
 * \return the number of loops of project
 */
extern int project_get_nb_loops(project_t* project);

/**
 * Gets the number of functions for a project
 * \param project a project
 * \return the number of functions of project
 */
extern int project_get_nb_fcts(project_t* project);

/**
 * Gets the number of asmfiles for a project
 * \param project a project
 * \return the number of asmfiles of project
 */
extern int project_get_nb_asmfiles(project_t* project);

/**
 * Sets a parameter in a project
 * \param project a project
 * \param module_id a value from params_modules_id_e enum corresponding to a module
 * \param param_id a value from params_<module>_id_e enum corresponding to the option
 * \param value the value of the parameter
 */
extern void project_add_parameter(project_t* project, int module_id,
      int param_id, void* value);

/**
 * Gets a parameter from a project
 * \param project a project
 * \param module_id a value from params_modules_id_e enum corresponding to a module
 * \param param_id a value from params_<module>_id_e enum corresponding to the option
 * \return the corresponding value or NULL
 */
extern void* project_get_parameter(project_t* project, int module_id,
      int param_id);

/**
 * Sets the processor version of a project
 * \param project a project
 * \param proc a processor version
 */
extern void project_set_proc(project_t* project, proc_t* proc);

/**
 * Sets the name of a processor version of a project
 * \param project a project
 * \param proc_name the name of the processor version
 */
extern void project_set_proc_name(project_t* project, char* proc_name);

/**
 * Sets the uarch name of a project
 * \param project a project
 * \param uarch_name a uarch
 */
extern void project_set_uarch_name(project_t* project, char* uarch_name);

/**
 * Sets the compiler used for a project
 * \param project a project
 * \param comp_code a compiler code
 */
extern void project_set_compiler_code(project_t* project, char comp_code);

/**
 * Sets the language used for a project
 * \param project a project
 * \param lang_code a language code
 */
extern void project_set_language_code(project_t* project, char lang_code);

/**
 * Sets the CG depth of a project
 * \param project a project
 * \param cg_depth CG depth
 */
extern void project_set_CG_depth(project_t* project, int cg_depth);

/**
 * Sets the name of a project
 * \param project a project
 * \param name a name
 */
extern void project_set_name(project_t* project, char* name);

/**
 * Return the opcode of the "INC" instruction
 */
int insn_inc_opcode();

/**
 * Indicate flags that are
 *    - read
 *    - set (to 1)
 *    - cleared (to 0)
 *    - defined: according to the result
 *    - undefined
 * for the given opcode
 */
int opcode_altered_flags(int opcode, uint8_t *read, uint8_t *set,
      uint8_t *cleared, uint8_t * def, uint8_t *undef);

/**
 * Indicate flags that are
 *    - read
 *    - set (to 1)
 *    - cleared (to 0)
 *    - defined: according to the result
 *    - undefined
 * for the given instruction
 */
extern int insn_altered_flags(insn_t *in, uint8_t *read, uint8_t *set,
      uint8_t *cleared, uint8_t * def, uint8_t *undef);

/**
 * Indicate if flags modified by the instruction override
 * given flags
 * */
int insn_flags_override_test(int opcode, uint8_t flags);

/**
 * Find an instruction in the block that overwrites the
 * flags modified by the instruction corresponding to the
 * given opcode. Return NULL otherwise
 * */
insn_t * block_find_flag_overriding_insn(block_t *block, int opcode);

/**
 * Find an instruction in the block that overwrites the
 * flags modified by the INC instruction
 * */
insn_t * block_find_flag_overriding_insn_inc(block_t *block);

#endif /* __LA_LIBMASM_H__ */
