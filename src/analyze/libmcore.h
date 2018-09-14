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

#ifndef __LC_LIBMCORE_H__
#define __LC_LIBMCORE_H__

#include "libmcommon.h"
#include "libmasm.h"
#include "config.h"

/**
 \file libmcore.h
 \brief Libmcore provides several analysis for a disassembled file.
 \page page3 Libmcore documentation page
 
 \section intro Introduction
 Libmcore is a C library which provides several static analysis for a
 disassembled binary file. They are:
 - flow graph analysis
 - domination analysis
 - loop analysis

 Some of these used home made algorithm described in following sections,
 others are based on existing algorithms.




 \section flowgraph Flow Graph analysis
 This analysis must be the first one to be done. Indeed, it creates some
 structures which are needed by all other libcore analysis. It uses as input
 a list of instructions (which represent the original assembly source code)
 and it generates a partial Control Flow Graph (CFG) and the Call Graph (CG).
 It is done in two steps:
 - the flow initialisation. This step parses all instructions to find which one are:
 -# a block begin
 -# a function begin

 - the graphs computation. Thanks to the first step, basic blocks are delimited
 (reminder: a basic block is the maximal set of consecutive instructions which
 are always executed. If the first instruction of a basic block is executed,
 then all other will be executed one after the other). This step builds
 basic blocks (all instructions between two basic block first instructions)
 and links them each other using some rules determined by the instruction at
 the end of the block:
 -# if it is a conditional jump, two edges are created (to the target block and to
 the following lexicographic block) in the CFG.
 -# if it is an unconditional jmp, one edge is created (to the target block) in the CFG.
 -# if it is a RET, no edges are created.
 -# if it is a CALL, an edge between the current function and the calle function is created in the CG.
 -# else, an edge is added between the current block and the following lexicographic block in the CFG.




 \section ibmanag Indirect branch management

 Indirect branches management is an important problem because if a direct branch
 can not create more than two edges in a CFG, an indirect branch can create a
 lot of edges (reminder: a indirect branch is a branch instruction (jump or call)
 whose target is represented by a register content (and not an immediate value as
 direct branch)). Most of our tools analysis used a connected graph, so if a part
 of a graph is not linked to the root node (because some edges are missing), it
 will not be analysed and this can create wrong analysis, especially if this
 orphan subgraph is a hotspot.

 Libcore provides several features to manage indirect branch, some of them
 can be activated or deactivated and some other are always activated.




 \subsection cmpalgo CMP Algorithm

 This algorithm is based on the following observation:

 A part of indirect branches have the following structure:
 \code
 ...
 (1) CMP <imm>, <index reg>
 (2) CONDITIONAL JUMP
 ...
 (3) MOV <offset>(, <index reg>, scale), <target>
 ...
 (4) JMP <target>
 \endcode
 This structure is for example used to implement switches.
 - (1) A compare instruction between an immediate value and the register used as index into a memory address
 - (2) then a jump if this value is too bigger
 - (3) a load of a memory value into a register
 - (4) then a branch on this register.

 CMP algorithm try to solve this specific structure. First, it gets the register
 used as target by the branch. Then it looks for a load into this register in
 the current basic block and in its predecessors, while there is only one
 predecessor per block. If the memory address has no base register, then a CMP
 instruction using the index register is searched into the block and its
 predecessors, again while there is only one predecessor per block.
 If all these instructions are found, the branch can be solved. Indeed, the
 \<offset\> gives the address of the first element of an indirection table
 saved in memory. \<index\> gives the number of element of this table, and
 \<scale\> give the size of one element. With these data, values from memory
 are retrieved, then an edge from the branch to the instruction which correspond
 to the memory value is added (and block are split if needed)

 This algorithm is always used because it does not create false edges. It is
 called during CFG computation.




 \subsection defaultalgo Default behavior

 If an indirect branch can not be solved, a default behavior is used to keep
 a connected graph. First, a virtual block is created. This block has no
 instruction and it allows the function to have more than one entry point
 (a possible thing when it is hand written assembly code or with some
 openmp code). Then an edge from the first block of a function to each orphan
 block is created. It has the same drawback as the Uniq algorithm (create
 false edges), but it assures that the graph is connected.

 This is automatically done before domination computation.




 \section domanal Domination analysis

 This analysis uses the algorithm described by
 Cooper, Harvey, Kennedy, 1999: A Simple, Fast Dominance Algorithm.

 It uses as input a CFG and it computes the function immediate domination
 (reminder: a block a dominates another block b is every paths from the CFG
 root to the block b contains a. If a is the closest block of b in b
 domination set, a immediately dominate b).




 \section loopanal Loop analysis

 This analysis used the algorithm described by
 Tao Wei, Jian Mao, Wei Zou and Yu Chen, A New Algorithm for Identifying Loops in Decompilation.

 It uses as input a CFG to find and create loops. The analysis is done withing a single
 DFS traversal of the CFG; during which, both loops and hierarchy of loops are created.
 The algorithm enables also to detect irreducible loops, and the complexity of the algorithm
 is almost linear.

 \section looppattern Loop pattern recognition

 This analysis detects some loop patterns in order to detect the body of the loop.

 */

///////////////////////////////////////////////////////////////////////////////
//                            Analysis functions                             //
///////////////////////////////////////////////////////////////////////////////
/**
 * Builds the control flow graph and the call graph of the asmfile.
 * \param asmfile an existing asmfile
 */
extern void lcore_analyze_flow(asmfile_t *asmfile);

/**
 * Solve indirect branches found in f using CMP algorithm
 * \param f a function
 */
extern void lcore_solve_using_cmp(fct_t* f);

/**
 * Builds the immediate dominators of all asmfile blocks.
 * The dominator tree is built as well.
 * \param asmfile an existing asmfile
 */
extern void lcore_analyze_dominance(asmfile_t *asmfile);

/**
 * Builds the immediate post-dominators of all asmfile blocks.
 * The post-dominator tree is built as well.
 * \param asmfile an existing asmfile
 */
extern void lcore_analyze_post_dominance(asmfile_t *asmfile);

/**
 * Launches the loop detection analysis for all functions
 * \param asmfile an existing asmfile
 */
extern void lcore_analyze_loops(asmfile_t *asmfile);

/**
 * Launches the connected components analysis for all functions
 * \param asmfile an existing asmfile
 */
extern void lcore_analyze_components(asmfile_t *asmfile);

/**
 * Analyzes functions to get connected compontents (CC) heads.
 * \param asmfile an existing asmfile
 */
extern void lcore_analyze_connected_components(asmfile_t *asmfile);

/**
 * Extract sub-functions from all asmfile functions, based on connected components
 * \param asmfile an existing asmfile
 */
extern void lcore_asmfile_extract_functions_from_cc(asmfile_t* asmfile);

/**
 * Analyzes a function to compute groups
 * \param function the function the analyze
 */
extern void lcore_fct_analyze_groups(fct_t* function);

/**
 * Analyzes a function to compute groups
 * \param asmf an assembly file to analyze
 * \param fctname the function the analyze
 */
extern void lcore_asmf_analyze_groups(asmfile_t* asmf, char* fctname);

/**
 * Computes the group increment, in bytes.
 * \param group a group to analyze
 */
extern void lcore_group_stride_group(group_t* group);

/**
 * Computes the group increment, in bytes, in all groups in the function.
 * \param function a function whose groups must be analyzed.
 * \warning Groups had to been computed before to call this function
 */
extern void lcore_group_stride(fct_t* function);

/**
 * Computes the memory accessed by a group
 * \param group a group to analyze.
 * \param user A user value given to the filter function:
 *    - 0: no filter
 *    - 1: only SSE
 *    - 2: only AVX
 *    - 3: only SSE and AVX
 */
extern void lcore_group_memory_group(group_t* group, void* user);

/**
 * Computes the memory accessed by all groups in a function
 * \param function a function whose groups must be analyzed.
 * \param user A user value given to the filter function:
 *    - 0: no filter
 *    - 1: only SSE
 *    - 2: only AVX
 *    - 3: only SSE and AVX
 * \warning Groups had to been computed before to call this function
 */
extern void lcore_group_memory(fct_t* function, void* user);

///////////////////////////////////////////////////////////////////////////////
//                          Live registers analysis                          //
///////////////////////////////////////////////////////////////////////////////
//@const_start
#define IN_FLAG 1    /**<Flag indicating that the variable belongs to IN set*/
#define OUT_FLAG 2   /**<Flag indicating that the variable belongs to OUT set*/
//@const_stop

#define USE_FLAG 1   /**<Flag indicating that the variable belongs to USE set*/
#define DEF_FLAG 2   /**<Flag indicating that the variable belongs to DEF set*/

/**
 * Returns an id corresponding to a register
 * Used in Live register analysis and SSA computation
 * \param X a register (reg_t*)
 * \param A an architecture (arch_t*)
 * \return an int with the register id
 */
extern int __regID(reg_t* X, arch_t* A);

/**
 * Returns a register corresponding to an id
 * Used in Live register analysis and SSA computation
 * \param id a register id
 * \param A an architecture (arch_t*) * \return
 * \return the register corresponding to the id
 */
extern reg_t* __IDreg(int id, arch_t* A);

#ifdef _ARCHDEF_arm64
/**
 * Returns a register corresponding to an id
 * Used in Live register analysis and SSA computation
 * \param id a register id
 * \param A an architecture (arch_t*) * \return
 * \return the register corresponding to the id
 */
extern reg_t* arm64_cs_IDreg (int id, arch_t* A);

/**
 * Returns an id corresponding to a register
 * Used in Live register analysis and SSA computation
 * \param X a register (reg_t*)
 * \param A an architecture (arch_t*)
 * \return an int with the register id
 */
extern int arm64_cs_regID(reg_t* X, arch_t* A);
#endif

/**
 * Computes live registers in a given function
 * \param fct a function to analyze
 * \param nb_reg used to return the number of registers
 * \param mode if TRUE, use the context saving specific version
 * \return NULL if problem, else an array containing IN and OUT sets.
 *         The returned array has fct_get_nb_blocks() entries and each entry
 *         is an array of nb_reg char. Basically, ret[i] contains IN and OUT
 *         sets for block whose id is i, and ret[i][j] contains IN and OUT
 *         data for register whose id is j. The register id is available using
 *         __regID() macro. This id is defined only for this analysis.
 *         Each register data is a set of flags. Possible values are
 *         IN_FLAG and OUT_FLAGS. If ((ret[i][j] & IN_FLAG) != 0), this means
 *         the variable j belongs to b IN set. It is the same for OUT set.
 */
extern char** lcore_compute_live_registers(fct_t* fct, int* nb_reg, char mode);

/**
 * Compute Use/Def set for a basic block
 * \param b a basic block to analyze
 * \param UseDef an array used to store use / def, built as the array returned
 *         by lcore_compute_live_registers functions.
 * \param mode if TRUE, use the context saving specific version
 */
extern void lcore_compute_use_def_in_block(block_t* b, char** UseDef, char mode);

/**
 * Compute the number of registers found in the architecture. If two registers
 * have the same name but different families, they are considered as the same
 * register.
 * \param arch an architecture
 * \return the number of registers.
 */
extern int lcore_get_nb_registers(arch_t* arch);

/**
 * Frees the array returned by lcore_compute_live_registers
 * \param fct a function to analyze
 */
extern void lcore_free_live_registers(fct_t* fct);

/* ************************************************************************* *
 *                               SSA analysis
 * ************************************************************************* */
typedef struct ssa_var_s ssa_var_t;
typedef struct ssa_insn_s ssa_insn_t;
typedef struct ssa_block_s ssa_block_t;

/**
 * \struct ssa_var_s
 * Used to store a SSA variable
 */
struct ssa_var_s {
   reg_t* reg; /**<A register*/
   ssa_insn_t* insn; /**<Instruction defining the variable.
    If the variable is used, then ptr contains
    the phi_function or the ssa_insn defining the
    variable.
    If the variable is defining, then ptr contains
    the last phi_function or the last ssa_insn
    defining the variable*/
   int index; /**<Index used to distinguish register versions*/
};

/**
 * \struct ssa_insn_s
 * Used to store an instruction modified for SSA computation
 */
struct ssa_insn_s {
   ssa_block_t* ssab; /**<SSA Block the instruction belongs to*/
   insn_t* in; /**<Original instruction*/
   ssa_var_t** oprnds; /**<New variables saved in a 2D-flatted array.
    For each operand at index i:
    If the operand is REGISTER
    - oprnds[i][0] is for the register
    - oprnds[i][1] is NULL
    If the operand is MEMORY
    - oprnds[i][0] is for the base
    - oprnds[i][1] is for the index
    If the instruction uses implicite registers,
    they are added at the end of the array.
    If the instruction is a phi-function, the
    array is a NULL terminated 1D array*/
   ssa_var_t** output; /**<SSA variables used to return the result*/
   int nb_implicit_oprnds; /**<Number of implicite operands added at the end
    of oprnds*/
   int nb_output; /**<Size of output array*/
};

/**
 * \struct ssa_block_s
 * Used to store a block modified for SSA computation
 */
struct ssa_block_s {
   block_t* block; /**<Original basic block*/
   queue_t* first_insn; /**<First (ssa_insn_t*) element of the block*/
};

/**
 * Computes the SSA form for a given function
 * \param fct a function to analyze
 * \return an array of ssa_blocks_t, n ssa_block per block
 */
extern ssa_block_t** lcore_compute_ssa(fct_t* fct);

/**
 * Equal function on ssa_var_t  for hashtable
 * \param v1 a pointer on a ssa_var_t
 * \param v1 a pointer on a ssa_var_t
 * \return TRUE if v1 == v2, else FALSE
 */
extern int ssa_var_equal(const void *v1, const void *v2);

/**
 * Hash function on ssa_var_t  for hashtable
 * \param v a pointer on a ssa_var_t
 * \param size current size of the hashtable
 * \return an hash of v
 */
extern hashtable_size_t ssa_var_hash(const void *v, hashtable_size_t size);

/**
 * Prints a SSA variable
 * \param reg a SSA variable to print
 * \param arch current architecture
 * \param outfile file where to print the register
 */
extern void print_SSA_register(ssa_var_t* reg, arch_t* arch, FILE* outfile);

/**
 * Prints a SSA instruction
 * \param in a SSA instruction to print
 * \param arch current architecture
 * \param outfile file where to print the instruction
 */
extern void print_SSA_insn(ssa_insn_t* in, arch_t* arch, FILE* outfile);

/**
 * Frees data on SSA
 * \param f function containing SSA results to free
 */
extern void lcore_free_ssa(fct_t* f);

extern ssa_block_t** fct_get_ssa(fct_t* f);

extern ssa_insn_t*** fct_get_ssa_defs(fct_t* f);

/* ************************************************************************* *
 *                       Advanced Dataflow Analysis
 * ************************************************************************* */
// List of flags used to customize interpretation of instructions
#define ADFA_NO_UNRESOLVED_SHIFT    0x1      /**< Do not used unresolved shift
                                                  in adfa_val_t structure*/
#define ADFA_NO_MEMORY              0x2      /**< Do not save memory address
                                                  representation but the instruction
                                                  address*/

/**
 * Alias for struct adfa_driver_s structure
 */
typedef struct adfa_driver_s adfa_driver_t;

/**
 * Alias for struct adfa_val_s structure
 */
typedef struct adfa_val_s adfa_val_t;

/**
 * Alias for struct adfa_cntxt_s structure
 */
typedef struct adfa_cntxt_s adfa_cntxt_t;

/**
 * \struct adfa_driver_s
 * This structure contains functions used to custom the Advanced Data Flow Analysis.
 * Each function is optional.
 */
struct adfa_driver_s {
   // Function called once at the beginning of the analysis.
   // Can be used to initialized a user data structure
   // \param fct_t* current fct
   // \param adfa_cntxt_t* adfa context
   // \return a structure defined and initialized by the user
   //         If this function is NULL, the user structure is NULL
   void* (*init)(fct_t*, adfa_cntxt_t*);

   // Function called for each instruction.
   // Can be used to filter instructions to analyze
   // \param ssa_insn_t* an instruction to filter or not
   // \param void* the data structure returned by the init function
   // \return TRUE if the instruction should be analyzed, else FALSE.
   //         If this function is NULL, all instructions are analyzed
   int (*insn_filter)(ssa_insn_t*, void*);

   // Function called for each instruction to analyze
   // Can be used to perform additional analysis on each unfiltered instruction
   // \param ssa_insn_t* an instruction to analyze
   // \param adfa_val_t* a structure representing to output of current instruction
   // \param hashtable_t* all computed adfa_val_t* structures, indexed by ssa_var_t*
   //        (only registers values are saved, nothing is done for memory)
   // \param void* the data structure returned by the init function
   void (*insn_execute)(ssa_insn_t*, adfa_val_t*, hashtable_t*, void*);

   // Function called at the end of each block analysis
   // Can be used to propagate data computed for current block to its successors
   // \param void* the data structure returned by the init function
   // \param ssa_block_t* current analyzed block
   // \return the user structure
   void* (*propagate)(void*, ssa_block_t*);

   void* user_struct; /**< Used to store the structure return by the init
    function*/
   int flags; /**< Array of flags*/
};

/**
 * \enum
 * Possible values for adfa_val_t type member
 */
enum {
   ADFA_TYPE_NULL = 0, /**< No type */
   ADFA_TYPE_REG, /**< ssa_var_t* */
   ADFA_TYPE_SONS, /**< pa_val_t*[2] */
   ADFA_TYPE_IMM, /**< int64_t */
   ADFA_TYPE_MEM_MTL /**< int64_t, store the address
    of the memory access */
};

/**
 * \enum
 * Possible values for adfa_val_t op member
 */
enum {
   ADFA_OP_NULL = 0, /**< No operation (leaf) */
   ADFA_OP_ADD, /**< Addition */
   ADFA_OP_SUB, /**< Subtraction */
   ADFA_OP_MUL, /**< Multiplication */
   ADFA_OP_DIV, /**< Division */
   ADFA_OP_SL, /**< Shift Left */
   ADFA_OP_SR, /**< Shift Right */
   ADFA_OP_SQRT /**< Square root */
};

/**
 * \struct adfa_val_s
 * Tree node used to represent a value in ADFA
 */
struct adfa_val_s {
   union {
      ssa_var_t* reg; /**< data is a register (leaf) */
      int64_t imm; /**< data is an immediate (leaf) */
      adfa_val_t* sons[2]; /**< data is not a leaf */
   } data;
   char type; /**< type of data (based on ADFA_TYPE_ enum) */
   char op; /**< operation (based of ADFA_OP_ enum) */
   char is_mem; /**< TRUE => sub-tree is a memory, else FALSE*/
};

/**
 * Analyzes a function using advanced data flow analyzis
 * \param f a function to analyze
 * \param driver a driver containing user functions
 * \return the context used for the analyzis
 */
extern adfa_cntxt_t* ADFA_analyze_function(fct_t* f, adfa_driver_t* driver);

/**
 * Creates a adfa_val_t structure from a given instruction
 * \param ssain an instruction
 * \param cntxt current context
 * \return an initialized adfa_val_t structure, else NULL
 */
extern adfa_val_t* ADFA_analyze_insn(ssa_insn_t* ssain, adfa_cntxt_t* cntxt);

/**
 * Prints an adfa_val_t structure
 * \param val an adfa_val_t structure to print
 * \param arch current architecture
 */
extern void adfa_print_val(adfa_val_t* val, arch_t* arch);

/**
 * Frees an adfa_cntxt_t structure
 * \param cntxt a structure to free
 */
extern void ADFA_free(adfa_cntxt_t* cntxt);

/* ************************************************************************* *
 *                               Induction analysis
 * ************************************************************************* */
enum ind_node_type {
   IND_NODE_NULL = 0, IND_NODE_IMM, IND_NODE_INV, IND_NODE_SONS
};

enum ind_node_op {
   IND_OP_NULL = 0, IND_OP_ADD, IND_OP_SUB, IND_OP_MUL, IND_OP_DIV
};

typedef struct ind_invariant_s ind_invariant_t;
typedef struct ind_node_s ind_node_t;
typedef struct ind_triple_s ind_triple_t;

struct ind_invariant_s {
   union {
      int imm;
      ssa_var_t* inv;
   } data;
   int type;
};

struct ind_node_s {
   union {
      int imm;
      ssa_var_t* inv;
      ind_node_t* sons[2];
   } data;
   int type;
   int op;
};

struct ind_triple_s {
   ssa_var_t* family;
   ind_node_t* add;
   ind_node_t* mul;
};

/**
 * \struct context_s
 * Save global data about current analysis
 */
typedef struct ind_context_s {
   ssa_block_t** ssa; /**<*/
   fct_t* f; /**<*/
   loop_t* l; /**<*/
   arch_t* arch; /**<*/
   hashtable_t* derived_induction; /**<*/
   hashtable_t** invariants; /**<*/
   hashtable_t* induction_limits; /**<*/
   queue_t* allocs_node; /**<*/
   char* blocks; /**<*/
   void (*interp_insn)(hashtable_t*, hashtable_t*, loop_t*, ssa_insn_t*,
         queue_t*);
   int (*analyze_cmp)(ssa_insn_t*, hashtable_t*);
} ind_context_t;

/**
 * Computes induction variables for a given function
 * \param fct a function to analyze
 */
extern ind_context_t* lcore_compute_function_induction_fromSSA(fct_t* fct,
      ssa_block_t** ssa);

/**
 * Computes induction variables for a given function
 * \param fct a function to analyze
 * \return
 */
extern ind_context_t* lcore_compute_function_induction(fct_t* fct);

/**
 * Frees induction results
 * \param cntxt induction results
 */
extern void lcore_free_induction(ind_context_t* cntxt);

extern void print_induction_triple(ind_triple_t* triple, arch_t* arch,
      FILE* outfile);

/* ************************************************************************* *
 *                               Stack analysis
 * ************************************************************************* */
typedef struct poly_context_s polytope_context_t;
typedef struct polytope_s polytope_t;

/**
 * \struct polytope_s
 * Represent a polytope
 */
struct polytope_s {
   fct_t* f; /**< Function the polytope belongs to*/
   loop_t* loop; /**< Loop the polytope belongs to*/
   adfa_val_t* acces; /**< ADFA representation of accessed address*/
   char* acces_str; /**< String representation of access member*/
   queue_t* registers; /**< List of registers used in acces member*/
   ssa_insn_t* ssain; /**< Instruction the polytope belongs to*/
   int level; /**< Loop level of the polytope*/
   char computed; /**< Boolean set to FALSE if the instruction should
    be instrumented, else TRUE*/
   ind_triple_t* induction; /**< Triple containing the induction result (or NULL)*/
   ssa_insn_t* start_bound_insn; /**< Instruction setting the start bound of the loop*/
   adfa_val_t* start_bound_val; /**< Value of the start bound*/
   ssa_insn_t* stop_bound_insn; /**< Instruction setting the stop bound of the loop*/
};

/**
 * Compute polytopes for a function
 * \param f a function to analyze
 */
extern void lcore_fct_analyze_polytopes(fct_t* f);

/**
 * Gets polytopes from a function (the function should be analyzed before)
 * \param f a function
 * \return an array of queue of polytopes. One queue per function's loop id
 */
extern queue_t** lcore_get_polytopes_from_fct(fct_t* f);

/**
 * Prints a value into a string
 * \param val a value to print
 * \param arch current architecture
 * \param str the string where to print
 */
extern void polytope_val_tostring(adfa_val_t* val, arch_t* arch, char* str);

/**
 * Generate the Lua code used to generate a graph representing the expression
 * \param polytope a polytope to analyze
 * \return a string to free
 */
extern char* polytope_toLuagraph(polytope_t* polytope);

/**
 * Frees polytopes computed for a function
 * \param f a function
 */
extern void lcore_free_polytopes(fct_t* f);

/* ************************************************************************* *
 *                               Stack analysis
 * ************************************************************************* */
typedef struct st_cntxt_s st_cntxt_t;

/**
 * Analyzes stack for current function
 * \param f a function
 * \return context used for this analysis.
 */
extern st_cntxt_t* lcore_fct_analyze_stack_(fct_t* f);

/**
 * Prints a adfa_val_t structure
 * \param val structure to print
 * \param arch current architecture
 */
extern void stack_print_val(adfa_val_t* val, arch_t* arch);

/**
 * Gets the value associated to an instruction
 * \param cntxt context retrived by lcore_function_analyse_stack
 * \param insn an instruction accessing to the stack
 * \param a adfa_val_t structure, else NULL
 */
extern adfa_val_t* insn_get_accessed_stack(st_cntxt_t* cntxt, insn_t* in);

/**
 * Frees a context created by lcore_fct_analyze_stack_
 * \param cntxt a context to free
 */
extern void lcore_free_cntxt(st_cntxt_t* cntxt);

///////////////////////////////////////////////////////////////////////////////
//                               API functions                               //
///////////////////////////////////////////////////////////////////////////////

/**
 * MACRO : defines the maximum number of paths that will be computed on a function.
 * Some functions may contain sufficiently paths to kill the MAQAO process (out of memory)
 */
#define FCT_MAX_PATHS 100000

/**
 * MACRO : defines the maximum number of paths that will be computed on a loop.
 * Some loops may contain sufficiently paths to kill the MAQAO process (out of memory)
 */
#define LOOP_MAX_PATHS 100000

typedef struct {
   uint16_t min;
   uint16_t max;
} DDG_latency_t;

/**
 * \struct data_dependence_t
 *  Used for DDG
 */
typedef struct {
   DDG_latency_t latency;
   int distance;
   char kind[4];
} data_dependence_t;

typedef DDG_latency_t (*get_DDG_latency_t) (insn_t *src, insn_t *dst);

/**
 * Computes paths for a function and overwrite its 'paths' field
 * \note Functions with more than one entry are not considered
 * \param f a function
 */
extern void lcore_fct_computepaths(fct_t *f);

/**
 * Computes paths for a loop and overwrite its 'paths' field
 * \note Loops with more than one entry are not considered
 * \param l a loop
 */
extern void lcore_loop_computepaths(loop_t *l);

/**
 * \brief frees the memory allocated for the paths of a function
 * \param f a function
 */
extern void lcore_fct_freepaths(fct_t *f);

/**
 * \brief frees the memory allocated for the paths of a loop
 * \param l a loop
 */
extern void lcore_loop_freepaths(loop_t *l);

/**
 * \brief returns the number of paths without computing them
 * \param f the targeted function
 * \return the number of paths
 */
extern int lcore_fct_getnpaths(fct_t *f);

/**
 * \brief returns the number of paths without computing them
 * \param l the targeted loop
 * \param take_branch decide whether to take a cycle once encountered or not
 * \return the number of paths
 */
extern int lcore_loop_getnpaths(loop_t *l, int take_branch);

/**
 * \brief check if a block "block" is an ancestor of a
 * "root" block obtained with a direct back edge from
 * "root" to "block". This enables to do an acyclic
 * traversal of the CFG.
 * \param root The block in the tail of the edge
 * \param block The block in the head of the edge
 * \return 1 if \e block is an ancestor of \e root obtained
 * with a direct back edge from \e root to \e block, 0 otherwise
 */
extern int lcore_blocks_backedgenodes(block_t *root, block_t *block);

/**
 * \brief Creates the list of initial heads of connected components. As a function may
 * contain more than just one component(especially in the case of openMP programs).
 * Hence, component headers are mendatory to cover all the function's elements within
 * the different analyses such as loop analysis.
 * \param f the target function
 * \return the list of initial heads of components
 **/
extern list_t *lcore_collect_init_heads(fct_t *f);

/**
 * Returns DDG for all paths of a function, with only RAW dependencies
 * \param fct function
 * \return queue of DDGs (CF lcore_fct_getddg)
 */
extern queue_t *lcore_fctpath_getddg(fct_t *fcts);

/**
 * Returns DDG for a function, with only RAW dependencies
 * If multipaths function, path DDGs are merged into a single DDG
 * \param fct function
 * \return DDG
 */
extern graph_t *lcore_fct_getddg(fct_t *fcts);

/**
 * Idem lcore_fctpath_getddg with WAW and WAR
 */
extern queue_t *lcore_fctpath_getddg_ext(fct_t *fct);

/**
 * Idem lcore_fct_getddg with WAW and WAR
 */
extern graph_t *lcore_fct_getddg_ext(fct_t *fct);

/**
 * Returns DDG for all paths of a loop, with only RAW dependencies
 * \param loop loop
 * \return queue of DDGs (CF lcore_loop_getddg)
 */
extern queue_t *lcore_looppath_getddg(loop_t *loops);

/**
 * Returns DDG for a loop, with only RAW dependencies
 * If multipaths loop, path DDGs are merged into a single DDG
 * \param loop loop
 * \return DDG
 */
extern graph_t *lcore_loop_getddg(loop_t *loops);

/**
 * Idem lcore_looppath_getddg with WAW and WAR
 */
extern queue_t *lcore_looppath_getddg_ext(loop_t *loop);

/**
 * Idem lcore_loop_getddg with WAW and WAR
 */
extern graph_t *lcore_loop_getddg_ext(loop_t *loop);

/**
 * Returns DDG for a path (array of blocks), with only RAW dependencies
 * \param path array of blocks
 * \return DDG
 */
extern graph_t *lcore_path_getddg(array_t *path);

/**
 * Idem lcore_path_getddg with WAW and WAR
 */
extern graph_t *lcore_path_getddg_ext(array_t *path);

/**
 * Returns DDG for a block, with only RAW dependencies
 * \param block a block
 * \return DDG
 */
extern graph_t *lcore_block_getddg(block_t *block);

/**
 * Idem lcore_block_getddg with WAW and WAR
 */
extern graph_t *lcore_block_getddg_ext(block_t *block);

/**
 * Returns DDG for a sequence (array) of instructions (array of blocks), with only RAW dependencies
 * \param insns array of instructions
 * \return DDG
 */
extern graph_t *lcore_getddg(array_t *insns);

/**
 * Idem lcore_getddg with WAW and WAR
 */
extern graph_t *lcore_getddg_ext(array_t *insns);

/**
 * Sets latency information in the DDG
 * \param DDG a DDG (graph)
 * \param get_latency function that returns latency from source and destination instructions
 */
extern void lcore_set_ddg_latency (graph_t *ddg, get_DDG_latency_t get_latency);

/**
 * Returns a loop RecMII (longest latency chain) from its DDG
 * \param ddg loop DDG
 * \param max_paths maximum number of paths to compute for each CC. If negative value or zero, equivalent to DDG_MAX_PATHS
 * \param min RecMII using min latency
 * \param max RecMII using max latency
 */
extern void get_RecMII(graph_t *ddg, int max_paths, float *min, float *max);

/**
 * Returns critical paths for a DDG
 * \param ddg DDG (a graph)
 * \param max_paths maximum number of paths to consider from an entry node
 * \param min_lat_crit_paths pointer to an array, set to critical paths considering minimum latency values
 * \param max_lat_crit_paths idem min_lat_crit_paths considering maximum latencies
 */
extern void lcore_ddg_get_critical_paths (graph_t *ddg, int max_paths,
                                          array_t **min_lat_crit_paths,
                                          array_t **max_lat_crit_paths);

/**
 * Frees memory allocated for a DDG
 * \param ddg DDG as returned by lcore_*_getddg[_ext]
 */
extern void lcore_freeddg(graph_t *ddg);

///////////////////////////////////////////////////////////////////////////////
//                               Print functions                             //
///////////////////////////////////////////////////////////////////////////////

/**
 * Prints a graph to a dot file and returns the path to this file.
 * \param graph graph (set of connected components)
 * \param filename dot file base name. Full name is GRAPHS_PATH + filename + ".dot"
 * \param print_graph_node function to print individual node
 * \return dot file path (string)
 */
extern char *lcore_print_graph (graph_t *graph, char *filename,
                                void (*print_graph_node) (graph_node_t *node, void *user));

/**
 * For each path of an object (function, loop), prints the DDG to a dot file
 * \param DDGs queue of DDGs (one per path)
 * \param type "function" or "loop"
 * \param global_id function/loop global ID
 */
extern void lcore_print_DDG_paths (queue_t *DDGs, char *type, int global_id);

/**
 * Prints the DDG to a dot file, all paths being merged (paths are ignored)
 * \param DDG DDG
 * \param type "function", "loop" or "block"
 * \param global_id function/loop/block global ID
 */
extern char *lcore_print_DDG_merged_paths (graph_t *DDG, char *type, int global_id);

/**
 * For each path of a function, prints the DDG to a dot file
 * \param fct a function
 */
extern void lcore_print_fct_ddg_paths (fct_t *fct);

/**
 * Prints a function's DDG to a dot file (different paths are merged)
 * and returns the path to this file
 * \param fct a function
 * \return dot file path (string)
 */
extern char *lcore_print_fct_ddg (fct_t *fct);

/**
 * For each path of a loop, prints the DDG to a dot file
 * \param loop a loop
 */
extern void lcore_print_loop_ddg_paths(loop_t *loop);

/**
 * Prints a loop's DDG to a dot file (different paths are merged)
 * and returns the path to this file
 * \param loop a loop
 * \return dot file path (string)
 */
extern char *lcore_print_loop_ddg(loop_t *loop);

/**
 * Prints a block's DDG to a dot file and returns the path to this file
 * \param block a block
 * \return dot file path (string)
 */
extern char *lcore_print_block_ddg(block_t *block);

/**
 * Prints out the control flow graph of a function with dot format.
 * \param f The function whose CFG is to be printed
 * \return a string with the file name where the CFG has been printed
 */
extern char* lcore_print_function_cfg(fct_t *f);

/**
 * Prints out the dominator tree with dot format
 * \param f The function whose dominator tree is to be printed
 * \return a string with the file name where the domination tree has been printed
 */
extern char* lcore_print_function_dominance(fct_t *f);

/**
 * Prints a function post domination tree into a file
 * \param f a function
 * \return a string with the file name where the post domination tree has been printed
 */
extern char * lcore_print_function_post_dominance(fct_t *f);

/**
 * Prints out the call graph with dot format.
 * \param project an existing project
 * \return a string with the file name where the CG has been printed
 */
extern char* lcore_print_cg(project_t* project);

/**
 * Prints the loops of a function
 * \param f fct_t* the function whose loops will be printed
 */
extern void lcore_print_function_loops(fct_t *f);

/* ************************************************************************* *
 *                            Loop pattern
 * ************************************************************************* */

/**
 * While: single entry-exit block */
struct loop_pattern_while {
   block_t * entry_exit;
};

/**
 * Repeat:
 *  - single-entry, single-exit
 *  - link between exit and entry is conditional or direct
 */
struct loop_pattern_repeat {
   block_t * entry;
   block_t * exit;
};

/**
 * MultiRepeat:
 *  - single-entry, multiple-exits
 *  - link between exits and entry are conditional or direct
 */
struct loop_pattern_multirepeat {
   block_t * entry;
};

enum LoopPatternType {
   While, Repeat, MultiRepeat
};

struct loop_pattern_s {
   enum LoopPatternType type;
   union {
      struct loop_pattern_while pattern_while;
      struct loop_pattern_repeat pattern_repeat;
      struct loop_pattern_repeat pattern_multirepeat;
   };
};

typedef struct loop_pattern_s * loop_pattern;

extern loop_pattern maqao_loop_pattern_detect(loop_t * loop);

/** Link type between two blocks (if any) */
enum BlockLink {
   BlockLinkDirect             // Direct instruction sequence
   ,
   BlockLinkUnconditionalJump  // Unconditional jump
   ,
   BlockLinkConditionalJump    // Conditional jump
   ,
   BlockLinkNone               // No link between the two blocks
};

/**
 * Indicate the kind of the link between two blocks (if any)
 * \param b1 First block
 * \param b2 Second block
 * \return Identifier of the type of link
 * */
enum BlockLink maqao_block_link_type(block_t *b1, block_t * b2);

#endif /* __LC_LIBMCORE_H__ */
