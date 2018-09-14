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
 * \file libmpatch.h
 * \brief Libmpatch is a library allowing to patch binary files from multiple architectures.
 *
 *	\page patch_main Patcher documentation page
 *	\todo This page will need some rewriting after the refactoring of the patcher. 
 * For this reason, it focuses mainly on the general principles, and skips over the 
 * implementation details.
 *	\e Libmpatch is a library allowing to patch binary files from multiple architectures.
 *
 *	The current implementation supports the x86_64 architecture and the ELF file format.
 *
 *	\section patch_general General description
 *	The functions from \e libmpatch allow to patch executable files. The following operations are supported:
 *	- Insertion of function calls (function \ref patchfile_patch_insertfct)
 *	- Insertion of assembly code (function \ref patchfile_insertcode)
 *	- Replacing a group of instructions by another (function \ref patchfile_patch_replaceinsn)
 *	- Deletion of code (function \ref patchfile_patch_removecode)
 *	- Insertion of global variables (function \ref patchfile_patch_insertdata)
 *
 * All patching sessions must be finalised with a call to \ref patchfile_patch. This function is responsible for building the patched file.
 *
 * All data used by the patcher is stored into a \ref patchfile_t structure for the duration of the patching session.
 *
 * \section patch_principles General principles
 *
 * \subsection patch_challenge Main challenge
 *
 * All modifications that would change the size of the code (insertion or deletion of code, 
 * modification into an instruction coded on less bytes) are not possible to do directly, as 
 * this would shift all	addresses, including the ones pointing to the data segment (which are 
 * not directly identifiable).
 *
 * \subsection patch_move Code displacement
 *
 * The method used to avoid the problem described in \ref patch_challenge is code displacement: 
 * the code that must be modified is moved to another, new section of the file, whose virtual 
 * address will be out of the original range of virtual addresses of the file (this is taken 
 * care of by \e libmtroll). A branch is inserted, replacing the original code, and points 
 * to the moved code. Another branch is added at the end of the displaced code, pointing to 
 * the next instruction after the original code. The displaced code can then be modified 
 * without any impact on the remaining of the code.
 *
 * This method presents however a shortcoming in the case of architectures with instructions of 
 * different sizes (like x86). In this case, there is no assurance that the inserted branch will 
 * not be larger than the code to be moved. In such a case, it is necessary to also move the surrounding
 * instructions, but at the risk of also moving the target of another branch instruction, which 
 * would find itself pointing to the middle of the inserted jump instruction.
 *
 * The chosen solution is then to move the whole basic block containing the address where the code 
 * modification must take place. The block is replaced by a branch instruction, followed by 
 * \c nop instructions padding the length of the original block. The basic block is moved and 
 * modified according to the patching requests. A return branch is added at the end.
 *
 * Another advantage of this method is that it reduces the number of added branches if more than
 * one modification has to happen in the same basic block, as it will be moved only once.
 *
 * \subsubsection patch_displ_pbs Pitfalls and solutions
 *
 * The method of displacing basic blocks presents two other problems:
 * - Correct detection of basic blocks: it is possible that an instruction in the middle of a basic 
 *   block is actually the target of an indirect branch instruction. There is currently no way to 
 *   detect this, and it can lead to crashes when the patched file is run.
 * - Size of the basic block: it is again possible that the basic block is shorter than the branch 
 *   instructions, leading to the same problem as when attempting to move a single instruction.
 *
 * In the case of a basic block smaller than the branch instruction, a workaround is implemented if 
 * the architecture offers a smaller branch instruction, allowing for a smaller offset (this is the 
 * case in x86_64, where there are instructions allowing to branch to instructions distant of up to 128 bytes).
 *
 * If the smaller branch instruction can fit in the basic block to move, it is then used to displace the code 
 * using a trampoline. In this method, a nearby block, close enough to be reached with a small branch 
 * instruction from the original block and large enough to contain two larger branch instructions, is moved 
 * using the standard method for code displacement. In the space left, a large branch instruction is inserted
 * (designated as a trampoline branch), pointing to the displaced smaller block that needed to be moved in the 
 * first place. The smaller block is replaced by a smaller branch instruction pointing to this trampoline 
 * branch. The displaced smaller block is appended with a large branch instruction pointing to the end of the 
 * original smaller block, as is the normal behaviour.
 *
 * There is still a case where a basic block is smaller than the smallest branch instruction available. In 
 * this case, a workaround consists in moving the whole function containing the block to move. This method 
 * is not used by default and must be requested by the user, as the main policy of \e libmpatch is to avoid 
 * moving too much code to reduce the risk of moving the target of an undetected indirect branch. Some OpenMP 
 * codes have also been observed to contain indirect branches into the body of a function.
 *
 * In the current version, the default behaviour if an displacement can not be done is to skip it and print an 
 * error message. A flag allows to bypass this behaviour and force the displacement anyway. This can be useful 
 * when all the blocks adjacent to the one causing the error will also be displaced, thus solving the lack of 
 * space problem. Usually however, forcing the displacement may lead the patched file to crash.
 *
 * \section patching Patching operations
 * \todo The implementation of patching operation will change with the patcher refactoring. Because of this, 
 * some fine-grained implementation details are skipped.
 *
 * \subsection inscode Inserting assembly code
 *
 * When inserting code, the block containing the insertion site will be displaced as explained in
 * \ref patch_move. The inserted instructions are inserted into the displaced block at the appropriate address.
 *
 * If the code has to be inserted before the given address, an extra step will be the update of all branch instruction
 * to this address to point at the inserted branch pointing to the displaced code. This ensures that the inserted
 * code will be always executed before the original instruction.
 *
 * \subsection insfunc Inserting function call
 *
 * The patcher allows to insert a call to a function. In the current version, only calls to functions already 
 * present in the binary or to functions defined in a dynamic library can be inserted.
 *
 * When inserting a function call, the block containing the insertion site will be displaced as explained in 
 * \ref patch_move. The assembly instructions for the function call will be inserted into the displaced block 
 * at the appropriate address, surrounded by the instructions needed to save and restore the context if requested.
 *
 * The assembly instructions are retrieved from architecture-specific functions.
 *
 * If the function is defined in an external library, a new block of code containing the architecture-specific
 * instructions for a stub for invoking an external function is added, with the appropriate instructions. 
 * The inserted call will point to this stub.
 *
 * An function call insertion is handled as an insertion of assembly code, with the instructions for invoking the
 * function and saving or restoring the context considered as the instructions to insert.
 *
 * \subsection replacecode Replacing a group of instructions
 *
 * Replacing a group of instructions can serve two purposes:
 * - Modifying an instruction. In effect, this amounts to remove it and replace it by another instruction with 
 *   the modified characteristics.
 * - Replacing a group of instructions by \c nop instructions.
 *
 * If the replacement causes the code to change size, which happens when we are modifying an instruction 
 * to replace it with an instruction coded on a different length, the block containing the modification to 
 * performed will be moved as described in \ref patch_move. Otherwise, the modification will directly take 
 * place in the original code.
 *
 * \subsection delcode Deletion of code
 *
 * When deleting code, the block containing the deletion site will be displaced as explained in \ref patch_move. 
 * The instructions are then deleted from the displaced block at the appropriate address.
 *
 * \subsection insdata Insertion of global variables
 * Global variables are stored during the patching session in the \c data member of the \ref patchfile_t structure.
 * The content of each variable is copied here, and the \ref globvar_t structure representing the global variable
 * is updated to store the offset in the \c data memory area where the content of this variable was copied.
 *
 * Global variables used in inserted instruction lists (either assembly code or function insertion) are referenced
 * by adding a new ELF target using the \e libmtroll function \ref elffile_add_targetscnoffset. The references will
 * ensure the code of the instructions using theses variables as RIP-based operands are updated when building the
 * patched file.
 *
 * \subsection conditions Handling conditions
 * In the current version, conditions can be set on an insertion modification only (instruction list or function).
 * The conditions will be serialised before the actual insertion takes place, arranging them in a format convertible
 * into assembly instructions.
 *
 * A condition for an insertion is represented as a binary tree, whose leaves are the conditions on numerical value and an operand,
 * and the other nodes are logical comparisons between conditions.
 * The serialisation aims at ordering the leaves conditions so that they can be expressed as a sequence of comparison and conditional branches
 * Therefore the goal is to identify for each condition the test to be performed and the destination of the conditional branch, which can
 * be either another condition, the beginning of the insertion (means the whole condition on the insertion was evaluated as true),
 * or its end (means the whole condition on the insertion was evaluated as false).
 *
 * \todo We may not need this here, but maybe as the comments on \ref cond_serialize. Or, rewrite this algorithm as it should be, not as it is (badly)
 * implemented in \ref cond_serialize
 *
 * The serialisation algorithm is the following:
 * -# Numbering of the leaves from left to right using a depth-first search. This ensures conditions will be evaluated as in C
 * -# Looping over the leaves in the order into which they were numbered:
 *    - If the condition is the last leaf: the comparison will be made on the logical opposite of the condition's and branch to the end of the insertion
 *      (the last comparison will be followed by the inserted code, which will not be executed if the reverse of the condition is true)
 *    - If the condition is the first child of its parent
 *      - If the parent is an \c AND comparison
 *        -# Searches the tree upward for the first parent node that is an \c OR comparison and from which the condition is a descendant of its
 *           first child
 *           - If no such node is found, the comparison will be made on the logical opposite of the condition's and branch to the end of the insertion
 *             (this is the first condition of a series of \c AND statements, if it fails the inserted code will not be executed)
 *           - If such a parent node is found, the comparison will be made on the logical opposite of the condition's and branch to the leaf that
 *             is a descendant of all first children of the second child of the parent node
 *             (this is the first condition of a chain that contains at least an \c OR statement, if it fails we will have to test the first condition
 *             in the other argument of the \c OR)
 *      - Else if the parent is an \c OR comparison
 *        -# Searches the tree upward for the first parent node that is an \c AND comparison and from which the condition is a descendant of its
 *           first child
 *           - If no such node is found, the comparison will be the same as the one in the condition, and branch to the beginning of the insertion
 *              (this is the first condition in a series of \c OR statements, if it succeeds the inserted code will be executed)
 *           - If such a parent node is found, the comparison will be the same as the one in the condition and branch to the leaf that
 *             is a descendant of all first children of the second child of the parent node
 *             (this is the first condition of a chain that contains at least an \c AND statement, if it succeeds we will have to test the first condition
 *             in the other argument of the \c AND)
 *    - Else if the condition is the second child of its parent
 *      -# Searches the tree upward for the first condition that is an \c OR comparison and from which the condition is a descendant of its first child
 *         - If no such node is found and the root of the tree is an \c AND comparison, the test will be performed on the logical opposite of the condition's
 *           and branch to the end of the insertion (this is the second condition in a series of \c AND statements, if it fails the inserted code will not
 *           be executed)
 *         - If no such node is found and the root of the tree is an \c OR comparison, the test will be the same as the one in the condition and branch
 *           to the beginning of the insertion (this is the second condition in a series of \c OR statements, if it succeeds the inserted code will be executed)
 *         - If such a condition is found
 *           -# Searches the tree for the first parent node that is an \c AND comparison and from which the condition is a descendant of its second child
 *              - If no such node is found, the comparison will be the same as the one in the condition, and branch to the beginning of the insertion
 *              (this is the first condition in a series of \c OR statements, if it succeeds the inserted code will be executed)
 *              - If such a node is found, the comparison will be the same as the one in the condition, and branch to the leaf that
 *             is a descendant of all first children of the second child of the parent node (this is the first condition of a chain that contains at least an \c AND statement, if it succeeds we will have to test the first condition
 *             in the other argument of the \c AND)
 *
 * \section commit Committing modifications
 *
 * Committing modifications launches the actual patching of the file from the list of pending modifications. This is done in \ref patchfile_patch.
 *
 * The operation will first apply all pending modifications, then finalise the patching session and generate the patched file.
 *
 * \subsection apply Applying modifications
 *
 * Pending modifications are applied in the following order:
 * - Dynamic libraries renaming through invocation of \ref elffile_rename_dynamic_library
 * - Insertion of dynamic libraries through invocation of \ref elffile_add_dynlib
 * - Reservation of the space for global variables in the \c data member of the \ref patchfile_t structure used for representing the patch operation
 * and updating the offset of the variables in \c data
 * - Loops over the list of modification requests to apply them. This is actually done in two steps, first \e processing the modifications, then \e applying.
 * Only modifications that were successfully processed are applied. In the current version, only the insertions (code or function call) are processed,
 * other modifications being simply flagged as successfully processed.
 *  - The following actions are performed when processing a modification:
 *    - Conditions (if existing) are serialised
 *    - Associated \c else modifications are processed and linked to the insertion
 *    - If the insertion corresponds to a external function call, its entry point is inserted:
 *      - If it is a dynamic function, a new stub is added to the list of instructions representing the new .plt section (element \c plt_list of the
 *        \ref patchfile_t structure)
 *    - Links the insertion to other modifications (fixed or floating) if needed
 *  - The actions described in \ref inscode, \ref replacecode and \ref delcode are performed when applying a modification
 * - The patching session is finalised (see \ref finalpatch) invoking \ref patchfile_patch_finalize
 * - The label modifications requests are then applied
 *
 * \subsection finalpatch Finalising patching session
 * The following actions are performed when finalising a patching session:
 * - The symbols in added object files (static patching) are resolved, building a list of the necessary object files to add to the file
 * from the list of static libraries to insert
 * - Branch rebounds are removed by replacing unconditional direct branches (not a \c call) branching to another unconditional branch by
 * an unconditional branch to the destination of the second branch. Branches in the inserted code section pointing to the instruction
 * immediately following them are deleted.
 * - The size of the added code section is calculated. A safety length, equal to the difference between the length of a short and a long jump
 * times the number of branches in the inserted code, is added to this size.
 * - The binary coding of the inserted code section is updated
 * - The size of the added plt section is calculated.
 * - The \e libmtroll function \ref elfexe_scnreorder is invoked to assign section identifiers to the new sections:
 *   - Data, if global variables are inserted or if the stack is moved to a location in the file
 *   - PLT, if calls to dynamic functions are added
 *   - Code, if code modifications were performed
 * - The addresses of inserted object files (static patching) are updated
 * - All references to inserted data are updated by invoking the \e libmtroll function \ref elffile_add_targetscnoffset
 * - The allocated size of the data section is increased with the size of the moved stack if necessary
 * - The binary code and addresses of the new PLT sections are updated
 * - The addresses and coding in the inserted code section are updated. This is done through a loop until both are stable, as the different sizes of branch
 * instructions means that the size of an instruction may increase when updating its code, thus spreading another branch instruction further from its destination
 * and necessitating it to change coding as well, also increasing its size.
 * - The address references in the ELF file are updated using the \e libmtroll function elffile_updtargets
 * - The binary binary data contained in the sections holding code and the PLT are updated
 * - The \e libmtroll function \ref copy_elf_file_reorder is invoked to generate the patched file.
 *
 * \subpage patcher_driver
 * */

#ifndef LIBMPATCH_H_
#define LIBMPATCH_H_

#include "libmcommon.h"
#include "libmasm.h"
#include "libmtroll.h"
#include "assembler.h"
#include "patchutils.h"
#include "patch_archinterface.h"

/**Flags used by patchfile_t and modif_t
 * Some of those flags can be present in modif_t or patchfile_t. If a flag is set in modif_t, it overrides the 
 * flag from patchfile_t for this insertion*/
/**\todo TODO (2014-10-28) I said it before and I will say it again, I HATE having to duplicate those flags between libmadras.h and libmpatch.h
 * Reorder and regroup them, then find some way to avoid it or be sure that some will not be missed when switching from libmadras to libmpatch
 * See also the comment in flags_madras2patcher*/
#define PATCHFLAG_NONE                    0x00000000 /**<Default value, no flag*/
/**\todo The 3 following flags are currently not used. Either use them or remove them. They were intended to replace some options that are currently
 * stored either in a different flag (for INSERTAFTER) or handled differently (a different structure)*/
#define PATCHFLAG_INSERT_REPLACEINSNS     0x00000001 /**<For an insertcode_t, specifies that existing instructions are being replaced instead of being added*/
#define PATCHFLAG_INSERT_UPDINSN          0x00000002 /**<For an insertcode_t, specifies that an instruction is being updated (incompatible with PATCHFLAG_INSERT_REPLACEINSNS)*/
#define PATCHFLAG_INSERTAFTER             0x00000004 /**<For an insertcode_t, specifies it must be performed after the given address*/

#define PATCHFLAG_NEWSTACK                0x00000008 /**<Insertion(s) must use a new stack (all of them if in patchfile_t or just the current one in insertcode_t)*/
#define PATCHFLAG_FORCEINSERT             0x00000010 /**<Insertion(s) must be performed even if there is not enough space*/
#define PATCHFLAG_MOVEFCTS                0x00000020 /**<Insertion(s) can move functions if they can't find a large enough basic block*/
#define PATCHFLAG_MOV1INSN                0x00000040 /**<Insertion(s) must move 1 or 2 instructions only*/
#define PATCHFLAG_NOWRAPFCTCALL           0x00000080 /**<Function call insertion(s) must not be wrapped by instructions to save/restore the context*/
#define PATCHFLAG_INSERT_FCTONLY          0x00000100 /**<For a function insertion, specifies that only the function or function call stub must be inserted*/
#define PATCHFLAG_BRANCH_NO_UPD_DST       0x00000200 /**<For a branch insertion, specifies that the destination must not be updated something is added before*/
#define PATCHFLAG_INSERT_NO_UPD_FROMFCT   0x00000400 /**<For a before insertion, specifies that branches from the same function must not be updated to point to it*/
#define PATCHFLAG_INSERT_NO_UPD_OUTFCT    0x00000800 /**<For a before insertion, specifies that branches from the another function must not be updated to point to it*/
#define PATCHFLAG_INSERT_NO_UPD_FROMLOOP  0x00001000 /**<For a before insertion, specifies that branches from the same loop must not be updated to point to it*/
#define PATCHFLAG_MODIF_FIXED             0x00002000 /**<Specifies that the code relocated due to this modification must be set at a specific address*/

#define A_MODIF_ERROR           0x01 /**<Specifies that a modification was detected as erroneous and must not be applied*/
#define A_MODIF_ATTACHED        0x02 /**<Specifies that a modification has been linked to another*/
#define A_MODIF_PROCESSED       0x04 /**<Specifies that a modification has already been processed and is ready to be applied to the file*/
#define A_MODIF_APPLIED         0x08 /**<Specifies that a modification has been applied to the file*/
#define A_MODIF_ISELSE          0x10 /**<Specifies that a modification is the else case of another modification*/
////New states added for refactoring
#define A_MODIF_PENDING         0x00 /**<Default state of a modification*/
#define A_MODIF_FINALISED       0x20 /**<Specifies that this modification will not be modified and is ready to be applied*/
#define A_MODIF_CANCEL          0x40 /**<This modification has been canceled and shall be ignored*/

/**
 * \brief Structure holding additional informations about an elf file being patched
 * \todo Heavy updates to this structure, to remove most of the pointers that point to another structure
 * Also could be useful to add other informations to help the patching process
 * */
typedef struct patchfile_s {
   queue_t* insn_list; /**<Queue of assembler instructions*/
//    int* codescn_idx;               /**<Indexes of the ELF sections containing executable code*/
   binscn_t** codescn; /**<Binary sections containing executable code*/
//    elfscnbounds_t* codescn_bounds; /**<Boundaries of the ELF sections (another VERY temporary structure)*/
//    elffile_t* efile;               /**<Pointer to the structure holding the associated parsed ELF file*/
   binfile_t* bfile; /**<Pointer to the structure holding the associated parsed binary file*/
   asmbldriver_t *asmbldriver; /**<Pointer to the architecture specific driver*/
   patchdriver_t *patchdriver; /**<Pointer to the architecture specific driver*/

    /****\todo TODO (2014-06-13) This is temporary, and I mean it this time. We should not need those mechanisms once the libbin is implemented ***/
   int scniniidx; /**<Index of the .init section (identified by A_SCTINI)*/
   int scnfinidx; /**<Index of the .fini section (identified by A_SCTFIN)*/
   int scntxtidx; /**<Index of the .text section (identified by A_SCTTXT)*/
   int scnpltidx; /**<Index of the .plt section (identified by A_SCTPLT)*/
   /***End of temporary quick and dirty fix. Besides it's architecture dependent, so another reason to get rid of it quickly***/

   asmfile_t* afile; /**<Pointer to the structure containing the file being patched*/
   hashtable_t* branches; /**<Hashtable containing the initial branches instructions, indexed by 
    the address they point to*/
   queue_t* patch_list; /**<Queue of instructions containing all the added instructions*/
//    void* data;                     /**<Section of data to add to the patched file*/
   queue_t* data_list; /**<Queue of data_t structures containing the data to add to the patched file*/
   void* tdata; /**<Section of thread data to add to the patched file*/
//    queue_t* varrefs;               /**<Queue of references to global variables*/
   queue_t* insertedfcts; /**<Queue of the different inserted functions (insertfunc_t structures)*/
   queue_t* insertedobjs; /**<Queue of asmfile structures containing the relocation files to insert*/
   queue_t* insertedlibs; /**<Queue of asmfile structures containing the relocation files that 
    could need to be added (the archive they belong to is needed by an insert)*/
//	elffile_t** objfiles;			/**<Array of object files to insert into the file*/
   binfile_t** added_binfiles; /**<Array of object files to insert into the file*/
   hashtable_t* extsymbols; /**<Hashtable containing asmfile_t structures containing the definitions of inserted symbols, indexed on the symbols names*/
//    int* reordertbl;                /**<Array of indexes used to reorder the sections in the patched file*/
//    queue_t* plt_list;              /**<List of instructions stubs used to branch to external functions*/
   /**\todo See comment about lastrelfctidx, although that one could be kept as it is not necessarily ELF or x86 specific*/
   insn_t* paddinginsn; /**<Instruction to use for padding when moving blocks or replacing instructions
    by shorter ones (\c nop by default)*/
//    int64_t tlssize;                /**<Size of the existing tls sections*/
//    int64_t datalen;                /**<Size of the section of data to add to the patched file*/
//    int n_codescn_idx;              /**<Size of the array containing the code sections indexes*/
   int n_codescn; /**<Size of the array containing the code sections*/
//    int lastrelfctidx;              /**<Index of the last entry in the process linkage table*/
//    int64_t tdatalen;               /**<Size of the section of initialized thread data to add to the patched file*/
//    int64_t tbsslen;                /**<Size of the section of uninitialized thread data to add to the patched file*/
//    /**\todo lastrelfctidx is not only x86_64 specific, but also ELF specific. It should be handled either by
//     *       the libmtroll, or calculated each time. Right now we need it here because now the PLT is built
//     *       so that added stubs are not added to it, so elfexe_get_jmprel_lastidx will not work as before, and
//     *       it's faster to add this variable than rebuilding it*/
   int64_t stackshift; /**<The value to shift the stack from if the shift stack method has been used*/
   int flags; /**<Flags altering the behaviour of the patcher*/
   /**\todo Merge those flags into a single option flag. However if we want to set the options from libmadras, we 
    * will need to define them there but find some way to avoid redefining them here (without also including 
    * libmadras.h) => Partly done, the trouble of defining flags twice remains*/
   int current_flags; /**<Flags altering the behaviour of the patcher for the current operation (allows 
    to avoid passing flags as parameters to each function)*/

   queue_t* modifs; /**<Queue of modification requests (modif_t objects)*/
   queue_t* modifs_var; /**<Queue of modification requests for variables (modifvar_t objects)*/
   queue_t* modifs_lib; /**<Queue of modification requests for libraries (modiflib_t objects)*/
   queue_t* modifs_lbl; /**<Queue of modification requests for labels (modiflbl_t objects)*/
   int current_modif_id; /**<Identifier of the next created modification*/
   int current_modiflib_id; /**<Identifier of the next created library modification*/
   int current_globvar_id; /**<Identifier of the next created global variable*/
   int current_tlsvar_id; /**<Identifier of the next created tls variable*/
   int current_cond_id; /**<Identifier of the next created condition*/
   queue_t* insnvars; /**<Queue of \ref insnvar_t structures containing the global variables linked to 
    instructions for all modifications*/

   hashtable_t* branches_noupd; /**<Hashtable of branches that must not be updated if code has to be inserted 
    before the destination, indexed by the address they point to*/
   char new_OSABI; /**<The value of the new target OS*/
   //The following variables are added to allow updating a global variable value after finalising a patched file
   uint8_t ready2write; /**<Set to TRUE if all modifications have been done and the file can be written*/
   int datascnidx; /**<Index of the data section in the finalised file*/
   //The following variables are added to allow tracking instructions addresses
   queue_t* insnaddrs; /**<Queue of insnaddr_t structures representing all original instructions in the file and their original addresses*/
   int last_error_code; /**<Code of the last error encountered*/
//////////NEW ELEMENTS ADDED BY PATCHER REFACTORING
   arch_t* arch; /**<Architecture of the file. BEWARE IN CASE OF INTERWORKING!*/
   bf_driver_t* bindriver; /**<Binary format specific driver associated to the binary file (referenced here for easier access)*/
   queue_t* movedblocks; /**<Queue of all movedblock_t structures representing blocks to be moved*/
   queue_t* fix_movedblocks; /**<Queue of movedblock_t structures representing blocks to be moved at a fixed address*/
   hashtable_t* insnreplacemodifs; /**<Hashtable of all modif_t structures replacing an instruction, indexed by the instruction they target*/
   hashtable_t* insnbeforemodifs; /**<Hashtable of all modif_t structures inserted before an instruction, indexed by the instruction they target*/
   hashtable_t* movedblocksbyinsns; /**<Tables containing movedblock_t structures indexed by all the instructions they contain.
    This includes the instructions from the original file and the new ones that will be added to this block*/
   hashtable_t* patchedinsns; /**<Table of patchinsn_t structures indexed by the original instruction*/
   hashtable_t* movedblocksbyscn; /**<Table of movedblock_t structures indexed by the binscn_t where they will be moved*/
   binfile_t* patchbin; /**<Copy of the structure representing the binary file, used for patching*/
   queue_t* reladdrs; /**<Queue of data_t structures containing addresses to be used by jumps to displaced code using memory relative operands*/
   data_t* newstack; /**<Pointer to the data structure representing the new stack, if at least one modification required one*/
   queue_t* memreladdrs; /**<Queue of data_t structures containing addresses used by branches with a memrel_t operand to access displaced code*/
   uint64_t availsz_codedirect; /**<Available size for displaced code reached with a direct branch from the original code*/
   uint64_t availsz_datarefs; /**<Available size for data accessed from the original code with a data reference*/
   uint64_t codedirectsz; /**<Total size of the displaced code reachable with direct branches*/
   uint64_t coderefsz; /**<Total size of the displaced code reachable with branches using a memory relative operand*/
   uint64_t codeindirectsz; /**<Total size of the displaced code reachable with indirect branches not using a memory relative operand*/
   uint8_t addrsize; /**<Size in bytes of an address usable by a memory relative operand from a jump instruction to store its destination*/
   queue_t* emptyspaces; /**<Queue of interval_t structures representing the available empty spaces in the file*/
   ////patchedinsns REPLACES patch_list ===> I'm reusing patch_list
   hashtable_t* insnrefs; /**<Hashtable containing the instructions in the patched file referencing a data, indexed by the data_t object referenced*/
   hashtable_t* datarefs; /**<Hashtable containing the data structures in the patched file referencing an instruction, indexed by the insn_t object referenced*/
   hashtable_t* newbranches; /**<Hashtable containing the branches in the patched file, indexed by their target instruction*/
   /////// Values retrieved from the driver
   int64_t smalljmp_maxdistneg; /**<Smallest signed distance reachable with the smallest (in instruction size) direct jump instruction*/
   int64_t smalljmp_maxdistpos; /**<Largest signed distance reachable with the smallest (in instruction size) direct jump instruction*/
   int64_t jmp_maxdistneg; /**<Smallest signed distance reachable with the standard direct jump instruction*/
   int64_t jmp_maxdistpos; /**<Largest signed distance reachable with the standard direct jump instruction*/
   int64_t relmem_maxdistneg; /**<Smallest signed distance between an instruction using a relative memory operand and the referenced address*/
   int64_t relmem_maxdistpos; /**<Largest signed distance between an instruction using a relative memory operand and the referenced address*/

   uint16_t smalljmpsz; /**<Size in byte of the smallest direct jump instruction list*/
   uint16_t jmpsz; /**<Size in byte of the direct jump instruction list*/
   uint16_t relmemjmpsz; /**<Size in byte of the jump instruction list using a relative memory operand as destination.*/
   uint16_t indjmpaddrsz; /**<Size in bytes of the indirect jump instruction list*/

} patchfile_t;

/**
 * Returns the code of the last error encountered and resets it to EXIT_SUCCESS
 * \param pf A pointer to the patched file
 * \return The existing error code stored in \c pf or ERR_PATCH_NOT_INITIALISED if \c pf is NULL
 * */
extern int patchfile_get_last_error_code(patchfile_t* pf);

/**
 * Sets the code of the last error encountered
 * \param pf A pointer to the patched file
 * \return The existing error code stored in \c pf or ERR_PATCH_NOT_INITIALISED if \c pf is NULL
 * */
extern int patchfile_set_last_error_code(patchfile_t* pf, int errcode);

/**
 * Sets the code of the last error encountered and uses a default value if the error code given is 0
 * \param ed A pointer to the patched file
 * \param errcode The error code to set
 * \param dflterrcode The error code to use instead of \c errcode if errcode is EXIT_SUCCESS
 * \return The existing error code stored in \c ed or ERR_PATCH_NOT_INITIALISED if \c ed is NULL
 * */
extern int patchfile_transfer_last_error_code(patchfile_t* pf, int errcode,
      int dflterrcode);

/**
 * Flags a modification as an else modification. All its successors will be flagged as well
 * \param modif The modification
 * */
extern void modif_annotate_else(modif_t* modif);

/**
 * Links a jump instruction to its destination and updates the branch table
 * \param pf Pointer to the structure holding the disassembled file
 * \param jmp The jump instruction to link
 * \param dest The destination instruction
 * */
extern void patchfile_setbranch(patchfile_t* pf, insn_t* jmp, insn_t* dest,
      pointer_t* ptr);

/**
 * Updates the value of a global variable. If the file has already been finalised, this will be propagated to the binary code
 * \param pf Structure holding the details about the file being patched
 * \param vardata Structure describing the global variable to update
 * \param value New value of the global variable
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int patchfile_patch_updatedata(patchfile_t* pf, globvar_t* vardata,
      void* value);

/**
 * Links a jump instruction to its destination, without updating the main table for storing branches. This is intended
 * for branches that should not be updated when insertions are performed before the destination.
 * \param pf Pointer to the structure holding the disassembled file
 * \param jmp The jump instruction to link
 * \param dest The destination instruction
 * */
extern void patchfile_setbranch_noupd(patchfile_t* pf, insn_t* jmp,
      insn_t* dest);

/**
 * Builds a queue of insnaddr_t structures tying an instruction in the file with its address for all instructions in the file.
 * This queue is stored in the insnaddr member of patchfile_t
 * \param pf Pointer to the structure holding the disassembled file
 * \return EXIT_SUCCESS if successful, error code otherwise (including if the queue had already been built)
 * */
extern int patchfile_trackaddresses(patchfile_t* pf);

/**
 * Frees a patchfile structure
 * \param pf The patchfile_t structure to free
 * */
extern void patchfile_free(patchfile_t* pf);

///**
// * Creates a new patched file
// * \param af File to be patched
// * \return A new \ref patchfile_t structure
// * */
//extern patchfile_t* patchfile_new(asmfile_t* af);
extern patchfile_t* patchfile_init(asmfile_t* af);

/**
 * Attempts to finalise a modification. This includes trying to find its encompassing block
 * \param pf Structure containing the patched file
 * \param modif The modification
 * \return EXIT_SUCCESS if the modification could be successfully processed, error code otherwise.
 * */
extern int patchfile_modif_finalise(patchfile_t* pf, modif_t* modif);

/**
 * Finalises a patching session by building the list of instructions and binary codings, but not writing the file
 * \param pf The patched file
 * \param newfilename The file name under which the patched file must be saved
 * \return EXIT_SUCCESS for success, error code otherwise
 * */
extern int patchfile_finalise(patchfile_t* pf, char* newfilename);

/**
 *  Saves a patched file to a new file
 * \param pf A pointer to the structure holding the disassembled file
 * \param newname The name of the file to save the results to. If set to NULL, the patched will not be created (this allows
 * further updates to be performed before) and patchfile_patch_write will need to be invoked afterward.
 * \return EXIT_SUCCESS / error code
 * */
extern int patchfile_patch(patchfile_t* pf, char* newname);

/**
 *  Saves a patched file to a new file
 * \param pf A pointer to the structure holding the patched file
 * \return EXIT_SUCCESS / error code
 * */
extern int patchfile_patch_write(patchfile_t* pf);

/**
 * Change the targeted OS of a patched file
 * \param pf Structure holding the details about the file being patched
 * \param OSABI The new value of the targeted OS, defined by macros ELFOSABI_...
 *        in elf.h file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int patchfile_patch_changeOSABI(patchfile_t* pf, char OSABI);

/**
 * Change the targeted machine of a patched file
 * \param pf Structure holding the details about the file being patched
 * \param machine The new value of the targeted machine, defined by macros EM_...
 *        in elf.h file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int patchfile_patch_changemachine(patchfile_t* pf, int machine);

#endif /* LIBMPATCH_H_ */
