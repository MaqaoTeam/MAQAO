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
 * \file patchutils.c
 * \brief This file contains helper functions for manipulating the structures used by libmpatch
 *
 * */

#ifndef PATCHUTILS_H_
#define PATCHUTILS_H_

#include "libmcommon.h"
#include "libmasm.h"

/**
 * Identifiers for the types of libraries to handle or insert into a file
 * */
enum libtypes {
   UNDEF_LIBRARY = 0, /**<Library type undefined*/
   STATIC_LIBRARY, /**<Code identifying a static library*/
   DYNAMIC_LIBRARY /**<Code identifying a dynamic library*/
};

/**
 * \enum condcodes
 * Type of possible conditions to use
 * */
enum condcodes {
   COND_VOID = 0, /**<Should NOT be used (it should not be needed either, but who knows)*/
   COND_AND, /**<AND condition (to be used between conditions)*/
   COND_OR, /**<OR condition (to be used between conditions)*/
   COND_LAST_LOGICAL, /**<Last logical condition (must NOT be used as a condition type)*/
   COND_EQUAL, /**<EQUAL condition (to be used between an operand and a value)*/
   COND_NEQUAL, /**<NOT EQUAL condition  (to be used between an operand and a value)*/
   COND_LESS, /**<LESS condition (to be used between an operand and a value)*/
   COND_GREATER, /**<MORE condition (to be used between an operand and a value)*/
   COND_EQUALLESS, /**<LESS or EQUAL condition (to be used between an operand and a value)*/
   COND_EQUALGREATER, /**<MORE or EQUAL condition (to be used between an operand and a value)*/
   N_CONDTYPES /**<Should always be the last entry in the enum. Not accepted as a valid condition type*/
};

/**
 * Types of possible insertion request
 * */
enum instypes {
   INSFCT = 0, /**<Function insertion*/
   INSLISTI /**<Instruction list insertion*/
};

/**
 * Types of possible invocation types of functions
 * */
enum calltypes {
   CALL_DIRECT = 0, CALL_INDIRECT
};

/**
 * Types of possible modifications to request
 * \warning The order of the types in this enum is used by modif_cmp_qsort and corresponds to the priority into
 * which modifications are handled when performed at an identical address.
 * */
typedef enum modiftype_e {
   MODTYPE_NONE = 0, /**<No modification to perform*/
   MODTYPE_INSERT, /**<Code insertion**/
   MODTYPE_MODIFY, /**<Modification of an instruction*/
   MODTYPE_REPLACE, /**<Replacement of whole instructions by others*/
   MODTYPE_DELETE, /**<Code deletions*/
   MODTYPE_RELOCATE /**<Simply relocate the instruction without further modification*/
} modiftype_t;

/**
 * Types of possible library modifications to request
 * */
enum modiflibtype {
   ADDLIB = 0, /**<Adds a new library*/
   RENAMELIB /**<Rename a library*/
};

/**
 * Types of possible variable modifications to request
 * */
enum modifvartype {
   NOUPDATE = 0, /**<No update to perform, this is an existing global variable, used for functions referencing it*/
   ADDGLOBVAR, /**<Insertion of a global variable*/
   ADDTLSVAR /**<Insertion of a tls variable*/
};

/**
 * Types of possible label modifications to request
 * */
enum modiflbltype {
   NEWLABEL = 0, /**<Add a new label*/
   RENAMELABEL /**<Rename a label*/
//Further modifications: change the name or type of a label, move it to another address or instruction, or delete it
};

/**
 * Types of variables
 * */
enum vartype {
   VAR_EXIST = 0, /**<Existing variable*/
   VAR_CREATED /**<New variable*/
};

/**
 * Types of tls variables
 * */
enum tlsvartype {
   UNINITIALIZED = 0, /**<Unitiliazed variables (going into the tbss)*/
   INITIALIZED /**<Initiliazed variables (going into the tdata)*/
};

/**
 * Utility macro for retrieving the id of a structure, or 0 if it is NULL
 * \param S The structure
 * \param T Its type, assumed to be the prefix of the member containing its id
 * */
#define STRUCT_ID(S,T) ((S)?S->T##_id:0)

/**
 * Macro retrieving the id of a condition, or 0 if it is NULL
 * \param C Pointer to the condition
 * */
#define COND_ID(C) STRUCT_ID(C,cond)

/**
 * Macro retrieving the id of a global variable, or 0 if it is NULL
 * \param G Pointer to the global variable
 * */
#define GLOBVAR_ID(G) STRUCT_ID(G,globvar)

/**
 * Macro retrieving the id of a tls variable, or 0 if it is NULL
 * \param G Pointer to the global variable
 * */
#define TLSVAR_ID(G) STRUCT_ID(G,tlsvar)

/**
 * Macro retrieving the id of a modification, or 0 if it is NULL
 * \param M Pointer to the modification
 * */
#define MODIF_ID(M) STRUCT_ID(M,modif)

/**
 * Macro retrieving the id of a library modification, or 0 if it is NULL
 * \param M Pointer to the library modification
 * */
#define MODIFLIB_ID(M) STRUCT_ID(M,modiflib)

/**
 * Flags used by inslib_t flags member
 */
#define LIBFLAG_NO_PRIORITY  0x0000 /**<The inserted lib has no priority*/
#define LIBFLAG_PRIORITY     0x0001 /**<The inserted lib has priority*/

//typedef struct cond_s cond_t;
//typedef struct modif_s modif_t;
////////////////STRUCTURES ADDED BY PATCHER REFACTORING
/**Specifies the type of branch instruction used to jump from a site to the displaced block*/
typedef enum {
   JUMP_NONE = 0, /**<No jump allows to reach the displaced block. Used for returning error codes*/
   JUMP_DIRECT, /**<Direct jump*/
   JUMP_MEMREL, /**<Indirect jump using a memory relative address*/
   JUMP_INDIRECT, /**<Independent indirect jump*/
   JUMP_TRAMPOLINE, /**<Trampoline to another block using a small direct jump ==> Not sure that one needs to be kept*/
   JUMP_MAXTYPES /**<Must always be last*/
} jumptype_t;
/**
 * Structure holding the details about a block of instructions moved due to a patching operation
 * (note that this is not necessarily a basic block)
 * */
typedef struct movedblock_s {
//   queue_t* jump;                   /**<Instruction(s) used to jump from this site to the moved block.*/
   queue_t* newinsns; /**<Instructions replacing the original block*/
   /**\todo TODO (2015-04-15) newinsns will contain the jump instructions to the displaced code, including those
    * from trampolines. To be seen whether we store that as a patchmodif or something like that*/
   uint8_t jumptype; /**<Type of jump used to jump from this site to the moved block.
    If using a trampoline, this is the jump appearing in the trampoline block*/
   list_t* firstinsn; /**<Node containing the first instruction of the block in the original code*/
   list_t* lastinsn; /**<Node containing the first instruction of the block in the original code*/
   list_t* sequence; /**<Node containing this movedblock in the list of movedblock_t structures*/
   int64_t newfirstaddr; /**<New address of the first instruction of the block after its move*/
   int64_t newlastaddr; /**<New address of the last instruction of the block after its move*/
   uint64_t availsz; /**<Remaining available size in bytes of the block (used for trampolines)*/
   queue_t* modifs; /**<Queue of modif_t structures targeting an address belonging to this block*/
   queue_t* patchinsns; /**<Queue of patchinsn_t structures representing all instructions in the block (built during finalisation)*/
   queue_t* trampsites; /**<Queue of movedblock_t structures referencing blocks using this block as a trampoline*/
   queue_t* localdata; /**<Queue of globvar_t structures that have to be added to the binary coding of the block (instead of the section for added data)*/
   struct movedblock_s *trampoline; /**<Pointer to the block used by this block as a trampoline (NULL if no trampoline is used)*/
   list_t* spacenode; /**<Pointer to the node from the list of empty spaces in the patchfile containing an empty space where this block will be moved*/
   /**\todo TODO (2015-03-18) Check if newfirstaddr and newlastaddr are redundant because of space or if we can find another use to them.
    * Also, they avoid using the getter functions for interval*/
//   queue_t* trampjump;              /**<Instruction(s) used to jump to the trampoline if one is used.*/
   /**\todo TODO (2014-09-16) Check which one between jump and trampjump should represent the instructions for jumping
    * to the displaced block and for jumping to the trampoline jump (which one is more coherent)
    * (2014-09-19) Also with regard to jumptype, do we still need jump or not ? A very vicious case would be a block being
    * used as a trampoline and using a given branch type to reach the displaced block, but needing another branch type
    * to reach the displaced block of the trampolined jump (could happen if we displaced the block first and could
    * reach the displacement using a direct jump, then much later use this block as a trampoline and having the section
    * of displaced code grown so much that we now need indirect branches).
    * (2014-09-25) I'm settling now on considering that a block using a trampoline implicitly uses a small jump. In this
    * case, the jumptype is the jump used in the trampoline block to reach the moved block.*/
   binscn_t* newscn; /**< Pointer to the section where this block will be moved*/
   uint64_t newsize; /**< Total size in bytes of the new block, including the size of the modifications it contains*/
   uint64_t maxsize; /**< Maximum size in bytes of the moved block, considering all instructions with relative operands reach their maximal sizes*/
} movedblock_t;
/**
 * Structure holding the details about an instruction modified due to a patching operation
 * */
typedef struct patchinsn_s {
   insn_t* origin; /**<Original instruction*/
   insn_t* patched; /**<Modified instruction (may be partially empty if its attributes are to be considered identical to the original)*/
   list_t* seq; /**<Node containing this structure*/
} patchinsn_t;
///////////////END OF STRUCTURES ADDED BY PATCHER REFACTORING

/**
 * Structure linking a patched instruction to its original address
 * */
typedef struct insnaddr_s {
   insn_t* insn; /**<Instruction in a patched file*/
   int64_t addr; /**<Original address of the instruction*/
} insnaddr_t;

/**
 * Holds the details about a series of conditions to set for a code insertion
 * */
typedef struct insertconds_s {
   oprnd_t** condoprnds; /**<Array of operands to use for each comparison that must be performed for this insertion to occur*/
   char* condtypes; /**<Array of comparison types to perform ('e' for equal,'n' for non equal,'l' for less or equal,'L' for less strict,'g' for greater or equal,'G' for greater strict)*/
   int64_t *condvals; /**<Array of values to use in the comparisons*/
   int* conddst; /**<Array of destinations to jump to if each comparison is successful (index of another condition, 0 if it is the beginning of the insertion, -1 its end*/
   int64_t* varoffsets; /**<Array of offsets of global variables to use for operands used in comparison (or -1 if this operand does not reference a global variable)*/
   int nconds; /**<Size of the arrays*/
   queue_t* elsecode; /**<List of instructions to execute if the conditions are not met*/
   char flags_nosave; /**<Specifies whether the flags storing the result of a comparison must be saved before inserting the code for these conditions (FALSE by default)*/
} insertconds_t;

struct modif_s;
/**
 * Structure used to store the details about a condition for the execution of an inserted code
 * */
typedef struct cond_s {
   oprnd_t* condop; /**<Operand whose value will be used in the condition (must be used with val)*/
   int64_t condval; /**<Value to compare the operand with (must be used with condval)*/
   struct cond_s* cond1; /**<First condition to satisfy (must be used with cond2)*/
   struct cond_s* cond2; /**<Second condition to satisfy (must be used with cond1)*/
   struct cond_s* parent; /**<Pointer to the parent condition if this condition is used in another condition*/
   insertconds_t* insertconds; /**<Description of the code modifications to perform to implement this condition*/
   struct modif_s* elsemodif; /**<Modification to perform if the condition is not met*/
   int cond_id; /**<Unique identifier of the condition*/
   char type; /**<Type of condition*/
} cond_t;

/*These structures are used for storing modification requests for a file*/

/**
 * \struct globvar_t
 * Holds the details about a global variable (for insertion, update, or reference)
 * */
typedef struct globvar_s {
   /**\todo TODO (2014-12-09) Change the sizes of the members in this structure to optimise size*/
//   void *value;      /**<Pointer to the value of the variable*/
//   int64_t offset;   /**<Offset of the global variable in the section containing it*/
//   int size;         /**<Size in bytes of the variable*/
   data_t* data; /**<Structure representing the variable*/
   char* name; /**<Name of the global variable. If initialised, a symbol will be added to the file and associated to its address*/
   list_t* spacenode;/**<Pointer to the node from the list of empty spaces in the patchfile containing an empty space where this variable will be inserted*/
   int type; /**<Type of the variable (existing or new). This field could be used later to store
    additional information about a variable, to avoid being too much redundant with
    the value in modifvar_t*/
   uint64_t align; /**<Alignement of the variable (in bytes). Its virtual address will have to verify: addr%align = 0 (or nothing if align is null)*/
//   int
   int globvar_id; /**<Unique identifier of the global variable*/
} globvar_t;

typedef struct tlsvar_s {
   void *value; /**<Pointer to the value of the variable*/
   int64_t offset; /**<Offset of the tls variable in the section containing it*/
   int size; /**<Size of the variable in bytes*/
   int type; /**<Type of the variable (initialized or not)*/
   int tlsvar_id; /**<Unique identifier of the tls variable*/
} tlsvar_t;

/**
 * Structure storing the details about a variable modification request
 * */
typedef struct modifvar_s {
   int type; /**<Type of the pending modification*/
   union {
      globvar_t* newglobvar; /**<Insertion of a global variable*/
      tlsvar_t* newtlsvar; /**<Insertion of a tls variable*/
   } data; /**<Pointer to the structure describing the variable modification to perform (depends on type)*/
} modifvar_t;

/**
 * Structure storing the details about a label modification request
 * \todo See if it is needed to split this structure like the other modification requests.
 * */
typedef struct modiflbl_s {
   char* lblname; /**<Name of the label to create or to rename into*/
   char* oldname; /**<Old name of the label (in case of label renaming or deletion)*/
   int64_t addr; /**<Address at which to move or add the label*/
   list_t* linkednode; /**<Container of the instruction at the address of which the label must be added*/
   char type; /**<Type of the modification to perform*/
   char lbltype; /**<Type of the label to add or change to*/
} modiflbl_t;

/**
 * Holds the details about an inserted library
 * */
typedef struct inslib_s {
   int type; /**<Type of the library (static or dynamic)*/
   char* name; /**<Name of the library*/
   asmfile_t** files; /**<Array of asmfiles for the object files in a static library
    (in the current version, NULL for a dynamic library)*/
   int n_files; /**<Size of the \e files array*/
   int flags; /**<A set of flags*/
} inslib_t;

/**
 * This structure is used to rename a dynamic library
 */
typedef struct renamed_lib_s {
   char* oldname; /**<Old name of the library (name before patching)*/
   char* newname; /**<New name of the library (name after patching)*/
} renamed_lib_t;

/**
 * Structure storing the details about a library modification request
 * */
typedef struct modiflib_s {
   int type; /**<Type of the pending modification*/
   int modiflib_id; /**<Unique identifier of the modification*/
   union {
      inslib_t* inslib; /**<Library to insert*/
      renamed_lib_t* rename; /**<Library to rename*/
   } data; /**<Library modification details*/
} modiflib_t;

/**
 * Holds the details about an inserted function used in inserted function calls
 * */
typedef struct insertfunc_s {
   unsigned type :4; /**<Type of the inserted function (internal, static or dynamic)*/
   unsigned calltype :4; /**<Type of the call (direct or indirect)*/
   char* name; /**<Name of the inserted function*/
//    insn_t* firstinsn;  /**<First instruction of the inserted function (or its stub)*/
   pointer_t* fctptr; /**<Pointer to use to invoke the function (points to an instruction for direct calls and for a data for indirect)*/
   char* libname; /**<Name of the library containing the function*/
   asmfile_t* objfile; /**<Structure holding the disassembled object file where the instruction is defined (if type STATIC)*/
} insertfunc_t;

/**
 * Holds the parameters for the insertion of a function call
 * - funcname: Name of the function to add a call to
 * - \e removed addr: Address at which the call must be inserted
 * - \e removed after: 1 if the insertion must be made after the instruction at the given address,
 * 0 otherwise
 * - parameters: Array of the function parameters (in the order they appear in the
 *               function prototype in C)
 * \warning The current version supports only 6 parameters for the x86_64 architecture
 * - nparams: Number of parameters
 * - optparams: Char array containing options for the parameters
 * */
typedef struct insfct_s {
   char* funcname; /**<Name of the function to insert*/
   oprnd_t** parameters; /**<Array of the functions parameters (in their C order)*/
   int nparams; /**<Number of parameters. The current version only supports 6 parameters
    for the x86_64 architecture*/
   int* sparams; /**<Array containing the bit-size of each parameter*/
   int functype; /**<The type of the function to insert (where it is defined: static or dynamic library, or the executable)*/
   char* optparam; /**<Contains options for the parameters*/
   globvar_t** paramvars; /**<Array of global variables used as parameters, in the same order as in parameters
    (NULL if the parameter does not use a global variable)*/
   globvar_t* retvar; /**<Global variable to put the return value into, if the function has a return value*/
   tlsvar_t** paramtlsvars;
   tlsvar_t* rettlsvar;
   modiflib_t* srclib; /**<Structure containing the details on the library file containing the function to insert
    (if NULL, it means it comes from the source file)*/
   insertfunc_t* insfunc; /**<Pointer to the structure describing the function to which we are inserting a call*/
   reg_t** reglist; /**<List of registers to save and restore.*/
   int nreg; /**<Number of register to save and restore.*/
} insfct_t;

#if REMOVING_THEM
/**
 * \struct insert_t
 * Holds the parameters for an insertion into a file
 * */
typedef struct insert_s {
   char after; /**<Specifies whether the insertion must be made before (0) or after (1) the instruction at \e addr*/
   int type; /**<Type of insertion to perform*/
   insfct_t* fct; /**<Insertion of a function call*/
   queue_t* insnlist; /**<List of instructions to insert*/
   /**\todo TODO (2014-12-15) This list should contain patchinsn_t structures (modify the driver functions accordingly) to avoid having
    * to create them afterward*/
}insert_t;

/**
 * Holds the parameters for a replacement into a disassembled file
 * */
typedef struct replace_s {
//   int ninsn;              /**<Number of instructions to delete after the given address*/
   queue_t *deleted; /**<List of deleted instructions (is NULL before deletion)*///Is it useful ?
   /**\todo TODO (2014-12-15) This list should contain patchinsn_t structures to avoid having to create them afterward*/
   int fillerver; /**<Indentifier of the function used to  =====> WILL BE REPLACED WITH PADDINGINSN*/
//Further devs: Flag to set the type of deletion (NOP, XOR, etc) (partly done by fillerver)
}replace_t;

/**
 * Holds the parameters for a deletion into a disassembled file
 * */
typedef struct delete_s {
//   int ninsn;              /**<Number of instructions to delete after the given address*/
   queue_t *deleted; /**<List of deleted instructions (is NULL before deletion)*///To avoid errors
   //caused when the deleted instructions exist in a hashtable
   /**\todo TODO (2014-12-15) This list should contain patchinsn_t structures to avoid having to create them afterward*/
}delete_t;
#endif
/**
 * \struct insnmodify_t
 * Holds the parameters for an instruction modification into a disassembled file
 * */
typedef struct insnmodify_s {
   char* newopcode; /**<New opcode of the instruction*/
   oprnd_t** newparams; /**<Array of new parameters*/
   uint8_t n_newparams; /**<Number of new parameters*/
   char withpadding; /**<Indicates that a padding must be used if the modified instruction is shorter than the original one*/
} insnmodify_t;

/**
 * Identifier specifying where a modification occurs with regard to the instruction
 * Do not change the positions of the identifiers in this enum as they are used for ordering modifs
 * */
typedef enum modifpos_e {
   MODIFPOS_BEFORE, /**<Modification is before the original instruction(s)*/
   MODIFPOS_REPLACE, /**<Modification replaces the original instruction(s) (modify, replace, delete)*/
   MODIFPOS_KEEP, /**<Modification does not change the original instruction (used for forced relocations)*/
   MODIFPOS_AFTER, /**<Modification is after the original instruction(s)*/
   MODIFPOS_FLOATING /**<"Floating" modification that is not tied to an instruction*/
} modifpos_t;

/**
 * Structure containing the details about a modification ready to be applied
 * \todo TODO (2014-10-31 happy halloween) Once this is settled, maybe merge this into modif_t. And get rid of insnmodify_t, delete_t, etc
 * */
typedef struct patchmodif_s {
   list_t* firstinsnseq; /**<Node containing the first instruction modified by this modification*/
   list_t* lastinsnseq; /**<Node containing the last instruction modified by this modification*/
   /**\todo TODO (2014-12-19) I don't think we should need or use lastinsnseq. This structure, if I keep it, will be only used to link an instruction
    * to a list of patchinsn_t. However I'm now using it only for modifs of type replace, otherwise it should be equal to firstinsnseq. Even though
    * it can cause problem (and actually I'm not even sure we should keep this structure)*/
   queue_t* newinsns; /**<Queue of insn_t structures associated to this modification (will replaces the original or be appended/prepended).
    Can be NULL in case of a deletion*/
   /**\note (2014-10-31) newinsns must exclusively contain new instructions.
    * In particular, in the case of an insert, it will *NOT* contain the duplication of the original instruction
    * => Especially now that we are using patchinsn_t structures
    * => Not anymore, they are insn_t structures, the patchinsn_t will only be created during finalisation*/
//   queue_t* newdatas;      /**<Queue of data_t structures added by this modification (when the arch-specific functions return them)*/
   /**\todo TODO (2014-10-31) The datas may be present in the modif_t instead of here. This would allow to handle insertions using globvar_t*/
   int64_t size; /**<Size in bytes of the modification*/
   uint8_t position; /**<Position of the modification with regard to the original instruction, defined in enum modif_position*/
} patchmodif_t;
/**
 * Store a modification request for a file
 * \todo Reorder the members of this structure to optimise its size
 * */
typedef struct modif_s {
   int64_t addr; /**<Address at which the modification must be made*/
   /**\todo We could use the list_t object pointing to the instruction at this address in order to
    avoid looking for it again during commit => Being done with modifnode*/
   list_t* modifnode; /**<Object containing the instruction at which the modification must occur*/
   uint8_t position; /**<Position of the modification with regard to the original instruction, defined in enum modifpos_t*/
   int64_t size; /**<Size in bytes of the modification*/
//   union {
//      insert_t* insert;    /**<Insertion request*/
//      replace_t* replace;  /**<Replacement request*/
//      delete_t* delete;    /**<Deletion request*/
   insnmodify_t* insnmodify; /**<Details for an instruction modification request*/
//   } data;                 /**<Pointer to the structure describing the modification to perform (depends on type)*/
   insfct_t* fct; /**<Details for a function call insertion request*/
   queue_t* newinsns; /**<Queue of instructions added by the modification (for insertions or instruction modifications)*/
   cond_t *condition; /**<Condition on the execution of the modified code in the patched file*/
   insn_t* paddinginsn; /**<Instruction to use for padding when moving blocks or replacing instructions by shorter ones (overrides the value in patchfile_t)*/
//  list_t* displaced_firstnode; /**<Node containing the first instruction to be displaced for performing this modification*/
//  uint64_t displacedsz;        /**<Size of the code to displace to perform this modification*/
//  int64_t finaloffset;     /**<Address of the modified code in the final patched file (not used for MODTYPE_DELETE modifications)*/
   struct modif_s* nextmodif; /**<Modification to execute after this modified code is executed in the patched file (only applicable to MODTYPE_INSERT modifications)*/
   insn_t* nextinsn; /**<Instruction to execute after this modified code is executed in the patched file (only applicable to MODTYPE_INSERT modifications)*/
   int64_t stackshift; /**<The value to shift the stack from (if needed) if the shift stack method has been used for this modification (overrides value in patchfile)*/
   int32_t flags; /**<Flags for altering the implementation of the modification*/
   uint8_t type; /**<Type of pending modification, using values from the modiftype_t enums*/
   int32_t modif_id; /**<Unique identifier of the modification*/
   int8_t annotate; /**<Flags characterising the processing of the modification*/
   /**\todo TODO (2014-11-03) Once the refactoring is over, rename the annotate field to \c state or \c progress or something like that to be more explicit*/
   //// ADDED BY PATCHER REFACTORING
   movedblock_t* movedblock; /**<Block moved by this modification request. May be NULL*/
   //patchmodif_t* patchmodif;   /**<Modifications to the instruction list brought by this modification*/
   pointer_t* nextmodifptr; /**<Pointer to the next modification, if \c nextmodif is not NULL*/
   queue_t* previousmodifs; /**<Queue of modifications pointing to this one (only for modifications with fixed address)*/
   queue_t* linkedcondmodifs; /**<Modifications linked to this one with regard to conditions. Linked modifications share the same condition and will
    point to the same patchmodif_t object*/
} modif_t;

///**
// * Holds the boundaries of an ELF section in an instruction list
// * \todo This structure can be removed when we get rid of the dependency to ELF sections
// * */
//typedef struct elfscnbounds_s {
//    list_t* begin;  /**< Node containing the first element of the section */
//    list_t* end;    /**< Node containing the last element of the section */
//} elfscnbounds_t;
//
/**
 * Links an instruction to a variable
 * */
typedef struct insnvar_s {
   insn_t* insn; /**<The instruction.*/
   union {
      globvar_t* gvar; /**<The global variable*/
      tlsvar_t* tlsvar; /**<The tls variable*/
   } var;
   int type; /**<Type of the variable*/
} insnvar_t;

enum insnlink {
   GLOBAL_VAR = 0, /**<The variable is a global var*/
   TLS_VAR /**<The variable is a tls var*/
};

/**
 * Types of an inserted function (as used in an insertfunc_t structure)
 * */
enum insertfunc_types {
   UNDEFINED = 0, /**<The function type is not defined and will have to be identified*/
   INTERNAL, /**<The function is one from the file*/
   STATIC, /**<The function is defined in a static library (object file)*/
   DYNAMIC /**<The function is defined in a dynamic library*/
};

/**
 * Types of inserted or updated labels
 * */
enum labels_type {
   LABELTYPE_NONE = 0, /**< Label has no associated type*/
   LABELTYPE_FCT, /**< Label has type function*/
   LABELTYPE_DUMMY, /**< Label has special "comment" type*/
   LABELTYPE_MAX /**< Max number of label types (must always be last)*/
};

/**
 * Adds the library \e extlibname as a mandatory external library
 * Returns a pointer to the insertion object for the library if the operation succeeded, NULL otherwise
 * We use this function for coherence with the traces
 * \param patchf Pointer to the madras structure
 * \param extlibname Name of the external library
 * \param filedesc Descriptor of the file containing the library (not mandatory, set to 0 if not to be used)
 * \param n_disassembler Pointer to a function allowing to disassemble a file possibly containing multiple binary files
 * \return Pointer to the inslib_t structure containing the details about the insertion or NULL if an error occurred.
 * In this case, the last error code in \c patchf will be updated
 * */
extern modiflib_t* add_extlib(void* patchf, char* extlibname, int filedesc,
      int (*n_disassembler)(asmfile_t*, asmfile_t***, int));

/**
 * Adds an instruction list insertion request
 * \param patchf File
 * \param insnq Instruction list
 * \param addr Address.
 * \param node Where to insert the list
 * \param pos Like everywhere else, use your brain
 * \param linkedvars Linked global variables
 * \return A pointer to the modification object containing the insertion to perform or NULL if \c pf is NULL.
 * If an error occurred, the last error code in \c patchf will be updated
 * */
extern modif_t* insert_newlist(void* patchf, queue_t* insnq, int64_t addr,
      list_t* node, modifpos_t pos, globvar_t** linkedgvars,
      tlsvar_t** linkedtlsvars);

/**
 * Creates a new condition
 * \param patchf Structure containing the patched file
 * \param condtype Type of the condition
 * \param oprnd Operand whose value is needed for the comparison
 * \param condval Value to compare the operand to (for comparison conditions)
 * \param cond1 Sub-condition to use (for logical conditions)
 * \param cond2 Sub-condition to use (for logical conditions)
 * \return A pointer to the new condition, or NULL if an error occurred
 * In this case, the last error code in \c patchf will be updated
 * */
extern cond_t* cond_new(void* patchf, int condtype, oprnd_t* oprnd,
      int64_t condval, cond_t* cond1, cond_t* cond2);

/**
 * Creates a new structure storing the data needed to generate the code associated to a condition
 * \param nconds The number of comparisons used in the condition
 * \return A new insertcond structure
 * */
extern insertconds_t* insertconds_new(int nconds);

/**
 * Frees a structure storing the data needed to generate the code associated to a condition
 * \param insertconds The insertcond structure to free
 * */
extern void insertconds_free(insertconds_t* insertconds);

/**
 * Checks whether a modification is processed.
 * \param modif The modification
 * \return TRUE if the modification exists and has been processed, FALSE otherwise
 * */
extern int modif_isprocessed(modif_t* modif);

/**
 * Checks whether a modification is fixed. A fixed modification can not be removed nor marked as not fixed.
 * \param modif The modification
 * \return TRUE if the modification exists, is flagged as fixed and has been processed, FALSE otherwise
 * */
extern int modif_isfixed(modif_t* modif);

/**
 * Adds a condition to a modification request
 * \param patchf Structure containing the patched file
 * \param modif The modification to add a condition to
 * \param cond The condition to add
 * \param strcond String representation of the condition. Will be used if \c cond is NULL
 * \param condtype If an existing condition was already present for this insertion, the new condition will be logically added to the existing one using
 * this type. If set to 0, COND_AND will be used
 * \param gvars Array of global variables to use in the condition. NOT USED IN THE CURRENT VERSION
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int modif_addcond(void* patchf, modif_t* modif, cond_t* cond,
      char* strcond, int condtype, globvar_t** gvars);

/**
 * Adds a request to a new global variable insertion into the file
 * \param patchf Structure containing the patched file
 * \param name Name of the global variable
 * \param type Type of the variable. Currently this is redundant with the value in modvar_t
 * \param size The size in bytes of the global variable
 * \param value A pointer to the value of the global variable (if NULL, will be filled with 0)
 * \return A pointer to the object defining the global variable
 * */
extern globvar_t* globvar_new(void* patchf, char* name, int type, int size,
      void* value);

/**
 * Adds a request to a new tls variable insertion into the file
 * \param pf Structure containing the patched file
 * \param type Type of the variable. Currently this is redundant with the value in modvar_t
 * \param size The size in bytes of the tls variable
 * \param value A pointer to the value of the tls variable (if NULL, will be filled with 0)
 * \return A pointer to the object defining the tls variable
 * */
extern tlsvar_t* tlsvar_new(void* patchf, int type, int size, void* value);

/**
 * Compares two modifications depending on their addresses, type, position, and order of insertion
 * \param m1 First modification
 * \param m2 Second modification
 * \return -1 if m1 is less than m2, 0 if m1 equals m2, 1 if m1 is greater than m2
 * m1 is less than m2 if, in order of priority:
 * - The address at which m1 takes place is strictly less than the address at which m2 takes place
 * - m1 is an insertion (MODTYPE_INSERT) and m2 is a deletion (MODTYPE_DELETE)
 * - m1 and m2 are insertions (MODTYPE_INSERT), with m1 being set before the address and m2 being set after the address
 * - m1 and m2 are insertions before the address, and m1 has a lower modif_id than m2 (it was requested before m2)
 * - m1 and m2 are insertions after the address, and m1 has a higher modif_id than m2 (it was requested after m2)
 * */
extern int modif_cmp_qsort(const void* m1, const void* m2);

/**
 * Compares two modifications depending on their addresses, type, position, and order of insertion
 * \param m1 First modification
 * \param m2 Second modification
 * \return -1 if m1 is less than m2, 0 if m1 equals m2, 1 if m1 is greater than m2
 * The ordering obeys the following rules, in order of priority:
 * - Order of addresses (m1->addr < m2->addr => m1 < m2)
 * - Types: MODTYPE_INSERT+before < MODTYPE_DELETE < MODTYPE_REPLACE < MODTYPE_MODIFY < MODTYPE_INSERT+after
 * - The address at which m1 takes place is strictly less than the address at which m2 takes place
 * - Between two insertions at the same address and the same position after/before the address:
 *   - m1 and m2 are insertions before the address, and m1 has a lower modif_id than m2 (it was requested before m2)
 *   - m1 and m2 are insertions after the address, and m1 has a higher modif_id than m2 (it was requested after m2)
 * */
extern int modif_cmp_qsort(const void* m1, const void* m2);

/**
 * Adds the modification request in a list of modifications
 * The list is ordered with the lowest insertion addresses at the beginning
 * \param patchf The structure representing the file being patched
 * \param addr The address at which the modification must take place
 * \param modifnode List object containing the instruction at \e addr
 * \param type The type of modification
 * \param pos The position of the modification with regard to the given instruction or address
 * \return A pointer to the structure representing the modification
 * */
extern modif_t* modif_add(void* patchf, int64_t addr, list_t* modifnode,
      modiftype_t type, modifpos_t pos);

/**
 * Adds the library modification request in a list of library modifications
 * \param patchf Structure containing the patched file
 * \param type The type of modification
 * \param mod The modification to add (its type must be in accordance with \e type)
 * \return A pointer to the structure representing the modification
 * */
extern modiflib_t* modiflib_add(void* patchf, int type, void* mod);

/**
 * Adds the variable modification request in a list of variable modifications
 * \param patchf Structure containing the patched file
 * \param type The type of modification
 * \param mod The modification to add (its type must be in accordance with /c type)
 * */
extern void modifvars_add(void* patchf, int type, void* mod);

/**
 * Compares two variable modification requests by alignment
 * \param m1 First modification
 * \param m2 Second modification
 * \return -1 if m1 has a higher alignment than m2, 1 if m2 has a higher alignment than m1, 0 if both have the
 * same alignment. So far we assume all modifications are variable insertions
 * */
extern int modifvar_cmpbyalign_qsort(const void* m1, const void* m2);

/**
 * Identifies a library type from its name.
 * \param libname Name of the library
 * \return The type of the library, as a constant defined in the enum libtypes. A library whose name ends with .a is
 * considered to be static, while a library whose name ends with .so is considered dynamic. Otherwise UNDEF_LIBRARY is returned
 * */
extern int getlibtype_byname(char* libname);

/**
 * Prints a condition to a string
 * \param cond The condition
 * \param str The string to print to
 * \param size The size of the string to print to
 * \param arch The target architecture the condition is defined for (assembly operands will be printed as "(null)" if not set)
 * */
extern void cond_print(cond_t* cond, char* str, size_t size, arch_t* arch);

/**
 * Creates a new insertfunc_t structure
 * \param funcname Name of the function
 * \param functype Type of the function
 * \param libname Library where the function is defined
 * \return New structure
 * */
extern insertfunc_t* insertfunc_new(char* funcname, unsigned functype,
      char* libname);

/**
 * Creates a new insfct_t object with the given parameters
 * \param funcname Name of the function to insert
 * \param parameters List of operands to use as parameters for the function
 * \param nparams Size of \e parameters
 * \param optparam Array of options for each parameters
 * \return Pointer to the new insfct_t structure
 * */
extern insfct_t* insfct_new(char* funcname, oprnd_t** parameters, int nparams,
      char* optparam, reg_t** reglist, int nreg);

/**
 * Creates a new insert_t object with the given parameters
 * \param after Flag specifying if the insertion must be made after the address or before
 * \param insert A pointer to a insfct if the insertion is a function call insert, NULL otherwise
 * \param insns List of instructions to insert if this is not a function call insert
 * \return Pointer to the new insert_t structure
 * */
//extern insert_t* insert_new (char after,insfct_t* insert, queue_t* insns);
/**
 * Creates a new replace_t structure
 * \param dels List of replaced instructions
 * \param ni Number of instructions
 * \param fillver Version of the filler to use
 * \return Pointer to the new replace_t structure
 * \todo Update this function to take into account the current way replace_t is used
 * */
//extern replace_t* replace_new (queue_t* dels);
/**
 * Creates a new delete_t structure
 * \param ni Number of instructions to delete
 * \return Pointer to the new delete_t structure
 * */
//extern delete_t* delete_new ();
/**
 * Creates a new modiflbl_t structure
 * \param addr Address where the label must be inserted of modified
 * \param lblname Name of the label to add or modify
 * \param lbltype Type of the label
 * \param linkednode Node containing the instruction to link the label to
 * \param oldname Old name of the label for renaming (not implemented)
 * \param type Type of the label modification to perform
 * \return Pointer to the new modiflbl_t structure or NULL if an error occurred
 * */
extern modiflbl_t* modiflbl_new(int64_t addr, char* lblname, int lbltype,
      list_t* linkednode, char* oldname, int type);

/**
 * Creates a new \ref insnmodify_t structure
 * \param newopcode New name of the opcode for the modified instruction
 * \param newparams Array of new parameters for the modified instruction
 * \param n_newparams Size of \e newparams
 * \param withpadding Flag specifying if a padding must be added if the new instruction has a smaller coding
 * \return A new \ref insnmodify_t structure with the given parameters
 * */
extern insnmodify_t* insnmodify_new(char* newopcode, oprnd_t** newparams,
      int n_newparams, int withpadding);

/**
 * Frees a condition and all sub-conditions
 * \param cond Pointer to the condition to free
 * */
extern void cond_free(cond_t* cond);

/**
 * Frees and insertfunc_t structure
 * \param i The structure to free
 * */
extern void insertfunc_free(void* i);

/**
 * Frees an insertion function request
 * \param ifc The insertion function request structure
 * \bug This function does not frees the allocated oprnd_t structures (as they can be pointers
 * to parameters in the instruction list)
 * */
extern void insfct_free(void* ifc);

/**
 * Frees an insertion request
 * \param ins The insertion request structure
 * */
extern void insert_free(void *ins);

/**
 * Frees a deletion request
 * \param d The deletion request structure
 * */
extern void delete_free(void* d);

/**
 * Frees an instruction replacement request
 * \param r The replacement request structure
 * \todo Like all uses of replace_t, this has to be updated when the actual use of this structure is clarified
 * */
extern void replace_free(void* r);

/**
 * Returns the labels defined in an inserted library.
 * \param modlib The \ref modiflib_t structure describing an library insertion
 * \param lbltbl Hashtable (indexed on label names) to be filled with the labels in the library.
 * Must be initialised (using str_hash and str_equal for hashing and comparison functions) or set to NULL if it is not needed.
 * It is assumed at least either \c lbltbl or \c lblqueue is not NULL
 * \param lblqueue Queue to be filled with the labels in the library.
 * Must be initialised or set to NULL if it is not needed. It is assumed at least either \c lbltbl or \c lblqueue is not NULL
 * \return EXIT_SUCCESS if the operation went successfully, error code otherwise (e.g. \c modlib is not a library insertion)
 * */
extern int modiflib_getlabels(modiflib_t* modlib, hashtable_t* lbltbl,
      queue_t* lblqueue);

/**
 * Frees a library insertion request
 * \param inslib Pointer to the library insertion request
 * */
extern void inslib_free(inslib_t* inslib);

/**
 * Frees an instruction modification request
 * \param imod The modification request structure
 * */
extern void insnmodify_free(void *imod);

/**
 * Frees a global variable structure
 * \param gv The structure to free
 * */
extern void globvar_free(globvar_t* gv);

/**
 * Frees a variable modification request
 * \param vmod Pointer to the structure holding the details about the variable modification request
 * */
extern void modifvar_free(void* vmod);

/**
 * Frees a library modification request
 * \param lmod Pointer to the structure holding the details about the library modification request
 * */
extern void modiflib_free(void* lmod);

/**
 * Frees a label modification request
 * \param lblmod Pointer to the structure holding the details about the label modification request
 * */
extern void modiflbl_free(void* lblmod);

/**
 * Creates a new patchmodif_t structure
 * \param firstinsnseq Node containing the first instruction modified by this modification
 * \param lastinsnseq Node containing the last instruction modified by this modification
 * \param newinsns Queue of insn_t associated to this modification (can be NULL for deletion)
 * \param position Where the modification takes place with regard to the original instruction(s)
 * \param size Size in byte of the modification
 * */
extern patchmodif_t* patchmodif_new(list_t* firstinsnseq, list_t* lastinsnseq,
      queue_t* newinsns, uint8_t position, int64_t size);

/**
 * Frees a patchmodif_t structure
 * \param pm The patchmodif structure
 * */
extern void patchmodif_free(patchmodif_t* pm);

/**
 * Frees an code modification request (this includes
 * \param cmod Pointer to the structure holding the details about the instruction modification request
 * */
extern void modif_free(void* cmod);

/**
 * Adds a parameter to a function call request
 * \param ifct Pointer to the structure containing the details about the function call request
 * \param oprnd Pointer to an operand structure describing the operand to use as parameter
 * \param opt Option about this operand
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int fctcall_add_param(insfct_t* ifct, oprnd_t* oprnd, char opt);

#endif /* PATCHUTILS_H_ */
