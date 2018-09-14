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
 * \file libmadras.h
 * \brief Contains the Madras API functions definitions
 * */

/**\todo Add examples on how to use the API*/

/**
 * \page madras_api Libmadras documentation page
 * \brief Description of the MADRAS API
 *
 * \e Libmadras is an API allowing to access the functionalities of the MADRAS (Multi Architecture Disassembler Rewriter and ASsembler) disassembler
 * (\e libmdisass) and patcher (\e libmpatch).
 *
 * The \e libmadras API allows to disassemble a binary file, examine its contents, and patch it. Patching operations cover inserting function
 * calls or assembly instruction, delete or modify existing assembly instructions, and modify the list of libraries needed by a file.
 *
 * Since \e libmadras relies on \e libmdisass and \e libmpatch, it is available on architectures those libraries have been compiled to support.
 * In the current version, only the x86-64 architecture and the ELF binary format are supported.
 *
 * \section madrasdoc_structs Libmadras structures
 *
 * The main structure used by \e libmadras is \ref elfdis_t. It is used to store a disassembled file (as a \ref asmfile_t structure),
 * the modification requests and logging settings. An \ref elfdis_t structure is returned upon disassembling a new file. It must be freed
 * using \ref madras_terminate.
 *
 * All subsequent \e libmadras functions need a pointer to the current \ref elfdis_t structure. It is unadvised to access directly the members
 * of an \ref elfdis_t structure.
 *
 * \section madrasdoc_diss Disassembling functions
 *
 * The main function for disassembly is \ref madras_disass_file. It needs a valid filename and will return a pointer to a new \ref elfdis_t
 * structure containing the disassembled file. None of the structural analysis functions from \e libmcore are performed on the disassembled
 * file. Only the information from the disassembly are available.
 *
 * \section madrasdoc_info Retrieving informations about a disassembled file
 *
 * The API allows to define a cursor on instructions present in the disassembled file. It is possible to move the cursor along the list of instructions and to retrieve
 * informations on the instruction the cursor currently points to.
 *
 * This part of the API is subject to change in the following releases of MADRAS.
 *
 * \todo Update this section when the status of the API is clarified.
 *
 * \section madrasdoc_patch Patching functions
 *
 * The functions allow to patch a file and save the results to a binary file. A patching session contains the following steps:
 * -# Patcher initialisation: this operation sets the variables needed for the patching operation
 * -# Patch requests: a series of patching requests. No patching is actually performed at this stage.
 * -# Patch commit: this step must always be the last one. It performs all the patch requests and writes the result to another file.
 *
 * In the current version, only one patching session can be performed on a disassembled file. It is also not possible to produce multiple patched
 * files from a single disassembled file. It is advised to terminate the \ref elfdis_t structure after a patching session has been completed. If
 * other patching operations are needed, a new structure must be created by disassembling the file once again.
 *
 * Because patching operations are all performed during the commit, potential patching errors will all be reported at that time.
 *
 * \subsection madrasdoc_patch_init Patcher initialisation
 *
 * Patcher initialisation must occur before applying any patch requests. It is performed through the function \ref madras_modifs_init.
 *
 * This function also allows to choose the method used for saving the stack when performing code insertions. This is important, since an inserted
 * code can use the stack for saving the context before performing a function call, and such an inserted function can itself be using the stack.
 * As insertions can be performed anywhere in the code, there is no guarantee that the necessary steps for preserving the stack before a call have
 * been done (unlike what happens on a compiled code), so the content of the stack can be corrupted by the insertions. This can lead to errors in the
 * patched file if the stack contained values used by the original code and that have been changed by the inserted code. The patched executable may even
 * crash if pointers were present on the stack.
 *
 * \e Libmadras offers three different ways of dealing with the stack:
 * - \b Keep: The stack pointer is not modified in any way. This mode is advised for insertions into codes where the stack is not used, or for patching
 * sessions which will not involve code insertions. It can lead to crashes if used for insertion into codes where the stack is used.
 * - \b Move: The stack is moved to an area of memory that has been added to the file. This ensures that the stack used by the inserted code does not
 * overlap with existing memory areas. However, it is not supported in multi-threaded mode, as the same area will be used for all threads. It can also
 * lead to an overflow if the inserted functions make a heavy use of the stack, as the size reserved for the moved stack is limited (currently to 1 Mb).
 * - \b Shift: The stack is shift upward by a number of bytes defined by the user. This allows to skip the area used by the current stack while
 * remaining thread-safe. As there is currently no way to know the top size of the current stack, this method may still lead to errors in the
 * patched file if the shift is not important enough. It has been observed that a shift of 512 bytes is enough for all tested codes. This mode
 * is advised for most patching sessions involving function call insertions.
 *
 * \subsection madras_datamodifs Data modification
 *
 * The MADRAS patcher allows to insert a new global variable to the file, using the \ref madras_globalvar_new function. The inserted global variable can be
 * accessed from any place of the executable; as its address is fixed, it is not protected against multi-thread access.
 *
 * An inserted global variable will be filled with zeroes by default. It is possible to specify a value to initialise it. This is the standard behaviour when
 * inserting strings.
 *
 * It is possible to use either the address or the value of global variables as parameters to inserted functions (see \ref madrasdoc_insfctcall) or
 * operands to inserted instructions (see \ref madrasdoc_insasminsns).
 *
 * \subsection madras_libsmodifs Libraries modification
 *
 * It is possible to modify the list of dynamic libraries needed by an executable. The name of a needed library can be changed using the \ref madras_extlib_rename function.
 * This can be useful for instance when a needed dynamic library has been itself patched and saved under a different name.
 *
 * The dynamic library where is defined a function whose call is inserted (see \ref madrasdoc_insfctcall) is automatically added to the list of needed libraries.
 * The function \ref madras_extlib_add also allows to do that but it is currently redundant. In future implementations, it will allows to insert static libraries to a file.
 *
 * \subsection madras_codemodifs Code modification
 *
 * The MADRAS patcher allows to insert calls to functions present in a file or defined in a dynamic library, insert assembly code, and delete or modify
 * instructions.
 *
 * All modifications are performed in the order of ascending addresses. In case of multiple modifications at the same address, the modifications will
 * be performed in the order into which their respective requests were made. It is also possible to choose whether an insertion must be performed
 * before or after the instruction present at the given address.
 *
 * The behaviour of the MADRAS patcher is undefined if an instruction is requested to be deleted as well as modified. It is also not possible to modify
 * an instruction that is being added by the patcher.
 *
 * All functions allowing to modify the code return a pointer to a \ref modif_t structure. This pointer represents the modification request and allows
 * to add further options to it.
 *
 * All modifications performing code insertions can be set at a null address. Those modifications are intended to be used either when linked to another
 * insertion (see \ref madrasdoc_linkmodifs) or as an "else" statement to an insertion with conditions (see \ref madrasdoc_conds). An error will be raised
 * when committing modifications requests if some unlinked modifications are set at a null address.
 *
 *
 * \subsubsection madrasdoc_insfctcall Inserting a function call
 *
 * It is possible to insert a call to a function either defined in an dynamic library file or already present in the executable, using the
 * \ref madras_fctcall_new function. If no library name is provided, the MADRAS patcher will assume the function is present in the executable, and will
 * display an error if not found. The current implementation does not support the insertion of functions defined in a static library, and does not
 * support the insertion of functions in a statically compiled executable.
 *
 * When inserting a function call, the MADRAS patcher will automatically surround the inserted call with the appropriate assembly instructions for saving
 * and restoring all registers, as well as aligning the stack pointer and saving the current stack depending on the chosen method (as described in
 * \ref madrasdoc_patch_init). The \ref madras_fctcall_new_nowrap function allows to insert only the function call without any such surrounding
 * instructions. This function can be useful to reduce the overhead of saving and restoring the context provided the user takes care of it (using
 * for instance \ref madras_insnlist_add to save and restore only a subset of registers) or if the inserted function can not change the execution
 * context or on the contrary if it is the expected behaviour.
 * \note \ref madras_fctcall_new_nowrap is now deprecated, as the same result can be obtained using \ref madras_fctcall_new then \ref madras_modif_addopt
 * with the \ref PATCHOPT_FCTCALL_NOWRAP flag.
 *
 * Both functions return a pointer to a \ref modif_t structure, which contains all the details on an insertion request. The pointer can be used
 * to add parameters and return value to the function call by invoking the \c madras_fctcall_add* functions.
 *
 * An inserted function call can accept up to 6 parameters (additional parameters will be ignored). Parameters can either be a valid assembly
 * operand passed in string form (using \ref madras_fctcall_addparam_fromstr), an operand from another instruction
 * (using \ref madras_fctcall_addparam_frominsn), an integer value (using \ref madras_fctcall_addparam_imm) or a global variable added by the
 * MADRAS patcher (using \ref madras_fctcall_addparam_fromglobvar). It is not possible to use an existing global variable as parameter.
 *
 * In the current version, all parameters are passed as 64 bits integers, so an inserted function can use either 64 bits integer or pointers as
 * parameters.
 *
 * An inserted function call can be set to return a value, using \ref madras_fctcall_addreturnval. The return value must be a global variable added
 * by the MADRAS patcher. Like the parameters, the return value is a 64 bits integer, so it can be either an integer or a pointer.
 *
 * \subsubsection madrasdoc_insasminsns Inserting assembly instructions
 *
 * It is possible to add assembly instructions either in string format using \ref madras_insnlist_add or as a queue of \ref insn_t structures
 * using \ref madras_add_insns. The instructions will be added at the given address without any check nor wrapping instructions.
 *
 * Instructions present in inserted lists can reference a global variable added by the MADRAS patcher. An instruction referencing such a variable must
 * use a memory operand using the instruction pointer as base and a null displacement (\c 0(%RIP)). An array of pointers to the
 * \c globvar_t structures describing the references global variables must be passed as parameter to the function, with the pointers appearing
 * in the same order in the array as they appear in the instruction list (it is possible for a pointer to appear more than once if the corresponding
 * global variable is to be referenced multiple times).
 *
 * Instructions added using \ref madras_insnlist_add must be passed as strings, all written in upper case, and separated by "\n" characters.
 * Such lists can use labels (identified as followed by a colon character ":" and beginning a line) to reference branch destinations. It is
 * not possible to reference a label in the patched file in an inserted list.
 *
 * \subsubsection madrasdoc_insertbranch Inserting a branch instruction
 *
 * It is possible to insert an unconditional branch instruction using \ref madras_branch_insert. The branches added using this function can point either to an
 * existing instruction in the file (referenced by its address) or another modification request (only insertions modification requests are currently supported).
 * In the latter case, the branch will point to the first instruction inserted by the modification.
 *
 * It is also possible to choose whether the branch must be updated if a code insertion is performed before its destination. The standard behaviour would be to update
 * the branch to point to the beginning of the inserted code, but an option allows to override this behaviour to ensure the branch always points to the same instruction.
 *
 * \subsubsection madrasdoc_modifinsn Modifying an instruction
 *
 * It is possible to modify an instruction at a given address using \ref madras_modify_insn. This function allows to change the opcode and/or some or all
 * operands of the instruction; it is also possible to remove or add operands. Opcodes and operands must be provided as strings identical to their
 * assembly representation, written in capital case.
 *
 * A flag allows to choose the behaviour of the patcher when the modified instruction has a smaller coding than the original. In such a case, the
 * MADRAS patcher can pad the remaining space with \c nop instructions, which allows to avoid moving the basic block containing the modified instruction
 * as is the standard behaviour.
 *
 * There is no test on the validity of the requested modifications when performing the request. If the modified instruction is invalid, an assembly
 * error will be returned when committing the changes.
 *
 * \subsubsection madrasdoc_delinsns Deleting instructions
 *
 * It is possible to delete one or more instructions in the file using \ref madras_delete_insns. The instructions will be removed from the patched file.
 * All direct branch targets pointing to a deleted instruction will be updated to point to the first non deleted next instruction.
 *
 * The function \ref madras_replace_insns allows to delete one or more instructions and replace them with \c nop instructions to preserve the size
 * of the program, thus eliminating the need to move a basic block. Direct branches instructions pointing to any replaced instruction are updated
 * to point to the first instruction of the replacement block instead.
 *
 * \subsubsection madrasdoc_linkmodifs Linking modification requests
 *
 * It is possible to link modification requests from one to another. This is only supported for modifications representing a insertion request (for code,
 * function call, branches...). Linking a modification M' to modification M will ensure that the code of M' is executed after the one of M.
 * - If the linked modification M' has an non null address, an unconditional branch will be added after the last instruction inserted by M, branching to
 * the first instruction inserted by M'.
 * - If the linked modification M' has a null address, the code that it should insert will be directly appended to the code inserted by M.
 *
 * \subsubsection madrasdoc_conds Adding conditions on modification requests
 *
 * It is possible to add conditions for the execution of the code generated by the modification request when running the patched file. This is currently
 * supported only for modifications representing a insertion (of code, function call, branch...).
 *
 * A condition object must first be created using \ref madras_cond_new. A condition can be formed with either:
 * - A numerical value, an assembly operand (in a \ref oprnd_t structure) and a comparison operator (<, =, <=...)
 * - Two other conditions and a logical operator (AND or OR).
 *
 * It is thus possible to build a complex conditions using multiple comparison of operands and values. The current version does not support a comparison
 * between two random assembly operands.
 *
 * A condition object can be attached to an existing modification using \ref madras_modif_addcond. If another condition was already attached to this modification,
 * the final condition attached to the modification will be formed by both conditions and the specified logical operand (AND by default).
 *
 * It is also possible to add a condition to a modification from its string representation, using \ref madras_modif_setcond_fromstr. The string representation
 * of a condition must obey a syntax close to the one used in C:
 * - All conditions are enclosed by brackets (\"(\" and \")\")
 * - A condition is either formed by two conditions separated by a logical operator (\"&&\" or \"||\"), or an assembly operand and a value separated by
 * a comparison operator (==, !=, <, >, <=, >=)
 * - Assembly operands used in conditions must be enclosed by quotes (\")
 *
 * A modification to which a condition was added can also get an else statement, using \ref madras_modif_addelse.
 * An else statement is a modification (only insertions are supported) set at a null address (an error will be raised if it is not).
 * The code inserted by this modification will be executed if the condition is not met.
 *
 * \subsection madrasdoc_patchopt Patcher options
 *
 * The \ref madras_modifs_addopt function allows to tweak the behaviour of the patcher for a whole patching session. The \ref madras_modif_addopt offers
 * the same functionality, but limited to a single modification request.
 *
 * \subsubsection madrasdoc_patchopt_insmode Options altering code displacement
 *
 * It is possible to alter how the MADRAS patcher handles code displacement when performing modifications that implies a change of size.
 *
 * By default, the MADRAS patcher will not perform an insertion if there is not enough space in the code (see documentation of \e libmpatch for more detailed
 * information). It is possible to set an option (\ref PATCHOPT_FORCEINS) forcing it to perform all requested insertions even when there is
 * not enough space in the code. For specific cases this can cause insertions to actually succeed because adjacent blocks are also moved, thus leaving
 * enough space for the insertions.
 *
 * By default, the MADRAS patcher moves basic blocks when inserting code. This may lead to insertion failing when a basic block is too small for the insertion.
 * An option (\ref PATCHOPT_MOVEFCTS) allows the MADRAS patcher to attempt moving whole functions when such a case is encountered. This allows most patching
 * operations to succeed, but can lead to errors in the patched file if a function has been incorrectly detected or if there are indirect branches
 * from other functions pointing to the middle of the moved function (this is often the case in OpenMP code).
 *
 * Conversely, the \ref PATCHOPT_MOV1INSN allows the MADRAS patcher to move only one instruction. This option must be used with caution as it disables
 * other checks and the use of trampolines. It should be reserved for cases where it can be sure that the address at a given modification is
 * large enough to insert a branch (for x86-64 this is 5 bytes).
 *
 * Those options may allow more patching operations to succeed, by increasing the risk of causing the patched executable to crash.
 * Using those options is unadvised on generic cases. The behaviour of the MADRAS patcher is also not defined if those options are used on some
 * but not all of the modification requests in a same basic block.
 *
 * \subsubsection madrasdoc_patchopt_branchupd Options altering branch updates
 *
 * It is possible to alter how the MADRAS patcher handles the updates of branches to instruction before which some code is added.
 *
 * The default behaviour of the patcher is to update all branches to an instruction to point to the first instruction of code inserted before it.
 *
 * The \ref PATCHOPT_NO_UPD_EXTERNAL_BRANCHES and \ref PATCHOPT_NO_UPD_INTERNAL_BRANCHES allow to restrict those updates to the branch
 * instructions respectively belonging or not to the same function as the patched instruction.
 *
 * The \ref PATCHOPT_BRANCHINS_NO_UPD_DST concerns inserted branch instructions, and prevents the branch to be updated if instructions are inserted
 * before its destination.
 *
 * \subsection madrasdoc_changepadding Changing the padding instruction
 *
 * By default, the instruction used to pad sections of code moved because of code displacement when performing modifications is the shortest \c nop
 * instruction available for the given architecture. It is possible to impose another instruction either for a given modification or for the whole
 * patching session, using respectively \ref madras_modifs_setpaddinginsn and \ref madras_modif_setpaddinginsn. The given instruction must have the same
 * length as \c nop, or an error will be raised.
 *
 * \subsection madrasdoc_commit Committing changes
 *
 * The final step in all patching sessions must be committing the changes using the \ref madras_modifs_commit function. It will perform all staged modifications and
 * generate a patched file with the requested name (it is advised not to use the same name as the original).
 *
 * The operations will be performed in the following order:
 * - Applying requests for renaming of dynamic libraries
 * - Applying requests for addition of dynamic libraries (and static libraries, but this is currently not implemented)
 * - Applying requests for addition of global variables
 * - Applying all requests for code modification (insertions, deletions and modifications) in the order of their addresses
 * - Saving the patched file under the given name
 * - Freeing the list of pending requests
 *
 * At this point the generated patched file contains all requested changes (except for those that caused an error during patching). It is not advised to keep using the MADRAS
 * \ref elfdis_t structure or the \c asmfile_t structure it contains for other operations.
 *
 * \section madrasdoc_logging Logging
 *
 * The \e libmadras API allows to record all requests performed on a disassembled file and write them to a log file. By default, this feature is turned off and can be
 * activated using the \ref madras_traceon function. It is also possible to choose the name of the log file (by default \c madras_trace.log).
 *
 * All invoked API functions will be recorded in the log, along with their parameters. The script \c madras_log2C.sh allows to turn such a file into a C source file, allowing
 * to compile it and rerun the requests.
 *
 * */

#ifndef LIBMADRAS_H_
#define LIBMADRAS_H_

#include "libmcommon.h"
#include "libmasm.h"
#include "libmtroll.h"
#include "libmworm.h"
#include "libmdisass.h"
#include "libmpatch.h"

//@const_start
/*Methods for handling the stack before inserting function calls*/
#define STACK_KEEP 0 /**<The stack is not moved in any way
                        This can cause some bugs for code using RSP and RBP as
                        base addresses*/
#define STACK_MOVE 1 /**<The stack is moved to a new section in the ELF file
                        This method prevents the above mentioned bugs but fails
                        if the program is multi-threaded*/
#define STACK_SHIFT 2 /**<The stack will be shifted before inserting function calls
                        This method works in multi threads, but can cause crashes if
                        the stack is too deep or the shift value too low*/

#define DFLT_TRACELOG "madras_trace.log" /**<Defaut name for the log file*/
//@const_stop

/**
 * Options controlling the behaviour of the patcher. If applied on an file using \ref madras_modifs_addopt, they will affect it during the
 * whole patching operation. If applied on a modification using \ref madras_modif_addopt, they will affect the patcher's behaviour for the execution
 * of this modification only
 * */
/**\todo TODO (2014-10-28) I said it before and I will say it again, I HATE having to duplicate those flags between libmadras.h and libmpatch.h
 * Reorder and regroup them, then find some way to avoid it or be sure that some will not be missed when switching from libmadras to libmpatch
 * See also the comment in flags_madras2patcher*/
#define PATCHOPT_NONE                      0x00000000   /**<Default mode (also safe mode: patch operations won't be performed if they can cause the code to crash)*/
#define PATCHOPT_FORCEINS                  0x00000001   /**<Option flag for forcing insertions when there is not enough space for them*/
#define PATCHOPT_MOVEFCTS                  0x00000002   /**<Option flag for moving functions when a basic block is not large enough to perform a patch operation*/
#define PATCHOPT_MOV1INSN                  0x00000004   /**<A single instruction will be moved when performing a patch operation.
                                                            In the current version, this option overrides PATCHOPT_MOVEFCTS.
                                                            If PATCHOPT_FORCEINS is also set, the instructions following the
                                                            instruction to move will be moved if the instruction is not large enough and if they
                                                            still belong to the same function. The insertion will not be made if there is still not
                                                            enough space available.
                                                            It is advised to used this flag with caution on a modification, as its behaviour is
                                                            undefined if a modification with this flag is performed inside the same basic block
                                                            as other modifications without this flag*/
#define PATCHOPT_STACK_MOVE                0x00000008 /**<The stack is moved to a new section in the ELF file (see \ref STACK_MOVE) */
#define PATCHOPT_NO_UPD_INTERNAL_BRANCHES  0x00000010 /**<Branches pointing to an insertion site must not be updated if originating from the same function*/
#define PATCHOPT_NO_UPD_EXTERNAL_BRANCHES  0x00000020 /**<Branches pointing to an insertion site must not be updated if originating from another function*/
//@const_stop
/**\todo The following flags are not needed especially as they are added directly by special functions prototypes, but kept here for keeping a bijection
 * with the flags in libmpatch. Either get rid of this dependency or remove the functions setting them implicitly (\ref madras_fctcall_new_nowrap and \ref madras_fct_add)
 * They can be useful as is however if we affect them to the patching session*/
//@const_start
#define PATCHOPT_FCTCALL_NOWRAP            0x00000040 /**<An inserted function call will not be wrapped by instruction for saving/restoring the registers context*/
#define PATCHOPT_FCTCALL_FCTONLY           0x00000080 /**<Only a function code or stub will added to the file without any call site*/
#define PATCHOPT_BRANCHINS_NO_UPD_DST      0x00000100 /**<Inserted branch(es) pointing to an instruction is/are not updated if an insertion is performed before this instruction*/
#define PATCHOPT_TRACK_ADDRESSES           0x00000200 /**<Allows to establish a correspondence between original addresses and those in the patched file*/

#define PATCHOPT_MODIF_FIXED               0x00000200 /**<Code relocated due to modification must be moved at a fixed address*/

#define PATCHOPT_LIBFLAG_NO_PRIORITY       0x00000000 /**<The inserted library has no priority*/
#define PATCHOPT_LIBFLAG_PRIORITY          0x00000001 /**<The inserted library has priority*/
//@const_stop

/**\todo REFACTOR Avoir un mode pour gérer la recherche des trampolines : automatic, on-demand, et disable.
 * Créer une fonction lookup trampoline qui prend une modif et cherche à lui trouver un trampoline (appelable si on-demand est activée);
 * Créer aussi une fonction d'API modif_init qui crée une modif à blanc, et permet d'empiler les options dessus avant de faire la
 * demande proprement dit. Les fctcall_new etc correspondent à un "package par défaut"*/
//@const_start
/**
 * \enum condtypes
 * Type of possible conditions to use
 */
typedef enum condtypes_e {
   COND_NONE = 0, /**<No condition*/
   LOGICAL_AND, /**<AND condition (to be used between conditions)*/
   LOGICAL_OR, /**<OR condition (to be used between conditions)*/
   LAST_LOGICAL_CONDTYPE, /**<Last logical condition (must NOT be used as a condition type)*/
   COMP_EQUAL, /**<EQUAL condition (to be used between an operand and a value)*/
   COMP_NEQUAL, /**<NOT EQUAL condition  (to be used between an operand and a value)*/
   COMP_LESS, /**<LESS condition (to be used between an operand and a value)*/
   COMP_GREATER, /**<MORE condition (to be used between an operand and a value)*/
   COMP_EQUALLESS, /**<LESS or EQUAL condition (to be used between an operand and a value)*/
   COMP_EQUALGREATER /**<MORE or EQUAL condition (to be used between an operand and a value)*/
} condtypes_t;
//@const_stop

/** \enum labeltypes
 * Type of possible labels to add
 */
enum labeltypes {
   NOTYPE_LBL = 0, /**<The label should have default type*/
   FUNCTION_LBL, /**<The label should have the type associated to a function*/
   DUMMY_LBL, /**<The label should have a specific "dummy" type, not identified as function by the disassembler*/
   N_LBLTYPES /**<Should always be the last entry in the enum. Not accepted as a valid label type*/
};

/**
 * \enum filetypes
 * Types of files
 * */
enum filetypes {
   UNKNOWN_FT = 0, /**< File type unknown*/
   EXECUTABLE_FT, /**< File is an executable*/
   DYNLIBRARY_FT, /**< File is a shared dynamic library*/
   RELOCATABLE_FT, /**< FIle is a relocatable (for use by the linker)*/
   N_FILETYPES /**< Should always be the last entry in the enum. Not accepted as a valid file type*/
};

/**
 * \enum insertpos_e
 * Identifiers to use for insertions specifying where the insertion must take place relatively to the instruction
 * */
typedef enum insertpos_e {
   INSERT_BEFORE = 0, INSERT_AFTER
} insertpos_t;
/********************************/
/*    Structures definitions    */
/********************************/

/***********************/
/*MADRAS API Structures*/
/***********************/

/**\todo This file is updated quickly to enable access to the same functionalities as MADRAS before the shiva update
 * without too much impact on the interface.
 * It MUST be updated later to remove all functions that became useless since the shiva update (mainly the functions for accessing
 * the data in the file) and update the functions for the patcher*/

/**
 * Structure used to store informations about logging by MADRAS
 * */
typedef struct logger_s logger_t;

/**
 * Structure used to store a correspondence between addresses in the original and patched file
 * */
typedef struct madras_addr_s {
   int64_t oldaddr; /**<Address of an instruction in the original file*/
   int64_t newaddr; /**<Address of an instruction in the patched file (will be -1 if the instruction has been deleted)*/
} madras_addr_t;

/*These structures hold the results of a disassembly*/

/**
 *  Stores a whole disassembled elf file and the informations relative to a patching session
 * */
typedef struct elfdis_s {
   char* name; /**<Name of the file*/
   list_t* cursor; /**<Cursor in the instruction list*/
   asmfile_t* afile; /**<Disassembled file*/
   patchfile_t *patchfile; /**<Modifications for this file*/
   logger_t *loginfo; /**<Informations about MADRAS logging*/
   int options; /**<Set of flags for controlling the patcher behaviour*/
   int last_error_code; /**<Code of the last error encountered*/
   char loaded; /**<Set to TRUE if the file was loaded from an existing asmfile, FALSE otherwise*/
} elfdis_t;

/******************************/
/*End of MADRAS API Structures*/
/******************************/

/**********************/
/*Madras API Functions*/
/**********************/

/*******************/
/*Utility functions*/
/*******************/
/**
 * Enable trace logging of the operations
 * \param ed A pointer to the structure holding the disassembled file
 * \param filename Name of the file to log the trace to. If set to NULL at the first invocation of \c madras_traceon,
 * the file specified by DFLT_TRACELOG will be used. This parameter is ignored for subsequent invocation of the function
 * \param lvl Level of the tracing. Currently not used
 * */
extern void madras_traceon(elfdis_t* ed, char* filename, unsigned int lvl);

/**
 * Disable trace logging of the operations
 * \param ed A pointer to the structure holding the disassembled file
 * \param filename Name of the concerned trace file. This parameter is currently not used
 * */
extern void madras_traceoff(elfdis_t* ed, char* filename);

/**
 * Returns the code of the last error encountered and resets it to EXIT_SUCCESS
 * \param ed A pointer to the MADRAS structure
 * \return The existing error code stored in \c ed or ERR_MADRAS_MISSING_MADRAS_STRUCTURE if \c ed is NULL
 * */
extern int madras_get_last_error_code(elfdis_t* ed);

/**
 * Prints a condition to a string
 * \param ed A pointer to the MADRAS structure
 * \param cond The condition
 * \param str The string to print to
 * \param size Size of the string to print to
 * */
extern void madras_cond_print(elfdis_t* ed, cond_t* cond, char* str,
      size_t size);

/***Functions used to initialise an elfdis object from an ELF file***/
/*******************************************************************/
/**
 * Disassembles a file 
 * \param filename The name or relative path of the file to disassemble
 * \return A pointer to a structure holding the disassembling results, or a NULL or negative value
 * if an error occurred
 * */
extern elfdis_t* madras_disass_file(char* filename);

/**
 * Generate an \c elfdis_t structure from an already disassembled file.
 * \warning This function is here to allow a bridge between applications using the high-level API (libmadras.h)
 * and applications which used MADRAS low-level API (madrasapi.h) to disassemble a file. madras_disass_file should be
 * used instead.
 * \param parsed A asmfile_t structure (from libmasm.h)
 * \return A pointer to a structure holding the disassembling results, or a NULL or negative value
 * if an error occurred
 * \todo Update or remove
 * */
extern elfdis_t* madras_load_parsed(asmfile_t* parsed);

/**
 * Removes a parsed file from an \em elfdis_t structure.
 * \warning This function should only be used on elfdis_t structures retrieved from a call to \em madras_load_parsed.
 * It allows to use \em madras_terminate on the structure, but the parsedfile_t structure used in madras_load_parsed must
 * be freed elsewhere
 * \param ed A pointer to the structure holding the disassembled file
 * \return A pointer to the parsedfile_t structure used to initialise the file
 * \todo Update or remove
 */
extern asmfile_t* madras_unload_parsed(elfdis_t* ed);

/******************************************************************/
/*Functions for retrieving information about the disassembled file*/
/******************************************************************/

/**
 * Tests whether a file is a valid ELF file
 * \param filename The path of the file to check
 * \param archcode If not NULL, the value pointed to by it will be set to the code
 * corresponding to the architecture of the file (as defined in \b elf.h)
 * \note The code for x86_64 is 62. The code for i386 is 3. The code for i64 is 50.
 * \param filecode If not NULL, the value pointed to by it will be set to the code
 * corresponding to the type of the file (as defined in \b elf.h)
 * \note The code for an executable file is 2. The code for an object file is 1. 
 * The code for a shared library is 3. The code for a core file is 4.
 * \return TRUE if the file is a valid ELF file, FALSE otherwise or if an error occurred
 */
extern boolean_t madras_is_file_valid(char* filename, int* archcode,
      int *filecode);

/**
 * Checks if the file has dedug data
 * \param ed A pointer to the structure holding the disassembled file
 * \return TRUE if the file has debug data, FALSE otherwise
 */
extern boolean_t madras_check_dbgdata(elfdis_t* ed);

/**
 * Retrieves the type of a disassembled file (executable, shared, relocatable)
 * \param ed A pointer to the structure holding the disassembled file
 * \return An integer equal to the type coding of the file, as defined in \ref bf_types.
 * */
extern int madras_get_type(elfdis_t* ed);

/**
 * Retrieves the architecture for which a disassembled file is intended
 * \param ed A pointer to the structure holding the disassembled file
 * \return An string representation of the architecture coding of the file
 * */
extern char* madras_get_arch(elfdis_t* ed);

/**
 * Retrieves the list of dynamic libraries
 * \param ed A pointer to the structure holding the disassembled file
 * \return a list of strings with dynamic library names. Do not free these strings
 */
extern list_t* madras_get_dynamic_libraries(elfdis_t* ed);

/**
 * Retrieves the list of dynamic libraries from a previously not parsed file
 * \param filepath Path to the file
 * \return A queue of strings (allocated with lc_malloc) of the dynamic library names,
 * or NULL if filepath is NULL or the file could not be found.
 * */
extern queue_t* madras_get_file_dynamic_libraries(char* filepath);

/** Read the .Debug_opt_report section generated by ICC */
int madras_get_intel_debug_info(elfdis_t* ed, uint64_t * n, uint64_t **p);

/**
 * Retrieves the start and end addresses of a given ELF section
 * \param ed Structure holding the disassembled file
 * \param scnname Name of the ELF section
 * \param scnidx Index of the ELF section (will be taken into account only if scnname is NULL)
 * \param start Return parameter, will contain the starting address of the section if not NULL
 * \param end Return parameter, will contain the end address of the section if not NULL
 * \return EXIT_SUCCESS if successful, error code otherwise.
 * */
extern int madras_get_scn_boundaries(elfdis_t* ed, char* scnname, int scnidx,
      int64_t* start, int64_t* end);

/**
 * Returns a branch instruction opposite to the instruction provided
 * \param ed Pointer to the structure holding the disassembled file containing the instruction.
 * \param in Pointer to the branch instruction to reverse
 * \param addr Address of the branch instruction to reverse (will be taken into account if \c in is NULL)
 * \param cond Return parameter. Will contain a condition corresponding to the reverse branch if it has no opposite in this architecture
 * \return Pointer to a new instruction corresponding to the reversed branch, NULL if \c in is not a branch or is not reversible
 * In this case, the last error code in \c ed will be updated
 * */
extern insn_t* madras_get_oppositebranch(elfdis_t* ed, insn_t* in, int64_t addr,
      cond_t** cond);

/********************************************************/
/*Functions used for navigating in the disassembled file*/
/********************************************************/
/**
 * Positions the instruction cursor at the given location
 * \param ed A pointer to the structure holding the disassembled file
 * \param label Label at which the cursor must be set. 
 * \param addr Address at which the cursor must be set, if \e label is NULL
 * \param scnname Name of the ELF section at the beginning of which the cursor must be set,
 *            if \e label is NULL and addr is -1. If also NULL, the cursor is set at the 
 *            beginning of the first section in the disassembled file
 * \return EXIT_SUCCESS if the positioning was successful, error code if it failed
 * (label not found, address not found or section name not found)
 * */
extern int madras_init_cursor(elfdis_t* ed, char* label, int64_t addr,
      char* scnname);

/**
 * Positions the instruction cursor at the location of the given instruction object
 * \warning This function is here to allow a bridge between applications using the high-level API (libmadras.h)
 * and applications which used MADRAS low-level API (madrasapi.h) to disassemble a file. It should not be used on files
 * disassembled with madras_disass_file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param ins A pointer to a structure holding an instruction
 * \return EXIT_SUCCESS if the positioning was successful, error code otherwise
 * */
extern int madras_align_cursor(elfdis_t* ed, insn_t* ins);

/**
 * Steps to the next instruction.
 * If the instruction is at the end of its section, steps to the beginning 
 * of the next disassembled section   
 * \param ed A pointer to the structure holding the disassembled file
 * \return EXIT_FAILURE if the current instruction is the last of the disassembled file,
 * an error code if an error occurred, EXIT_SUCCESS otherwise
 * */
extern int madras_insn_next(elfdis_t* ed);

/**
 * Steps to the previous instruction.
 * If the instruction is at the beginning of its section, steps to the end  
 * of the previous disassembled section   
 * \param ed A pointer to the structure holding the disassembled file
 * \return EXIT_FAILURE if the current instruction is the first of the disassembled file,
 * an error code if an error occurred, EXIT_SUCCESS otherwise
 * */
extern int madras_insn_prev(elfdis_t* ed);

/**
 * Checks if the cursor instruction is at the end of the current ELF section
 * \param ed A pointer to the structure holding the disassembled file
 * \return TRUE if the cursor instruction is at the end of the current ELF section,
 * FALSE otherwise. If an error occurred, the last error code will be filled in \c ed
 * */
extern boolean_t madras_insn_endofscn(elfdis_t* ed);

/*************************************************************/
/*Functions for retrieving data about the current instruction*/
/*************************************************************/
/**
 * Retrieves the name of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return The name (mnemonic) of the instruction the cursor is at, or NULL if an error occurred
 * (in this case, the last error code will be updated in \c ed if it is not NULL)
 * */
extern char* madras_get_insn_name(elfdis_t* ed);

/**
 * Retrieves the bit size of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return Size in bits of the instruction, or -1 if an error occurred
 * (in this case, the last error code will be updated in \c ed if it is not NULL)
 * */
extern int madras_get_insn_size(elfdis_t* ed);

/**
 * Retrieves the address of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return The virtual address of the instruction, or ADDRESS_ERROR if an error occurred
 * (in this case, the last error code will be updated in \c ed if it is not NULL)
 * */
extern int64_t madras_get_insn_addr(elfdis_t* ed);

/**
 * Retrieves the type of a parameter of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the parameter as it appears in an assembly instruction written in
 *        AT&T language (source first), starting at 0
 * \return 
 * - Value \c OT_REGISTER for registries
 * - Value \c OT_IMMEDIATE for immediate
 * - Value \c OT_POINTER for pointers
 * - Value \c OT_MEMORY for memory
 * - A null value if \e pos is an incorrect parameter index or
 * an error occurred. In this case, the last error code in \c ed will be filled
 * */
extern int madras_get_insn_paramtype(elfdis_t* ed, int pos);

/**
 * Retrieves the number of parameters of the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return The number of operands of the instruction or 0 if an error occurred.
 * In this case the last error code in \c ed will be filled
 * */
extern int madras_get_insn_paramnum(elfdis_t* ed);

/**
 * Retrieves a given operand from the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return The operand in string form in the AT&T assembly format, or a NULL
 * if an error occurred. In this case the last error code in \c ed will be filled
 * */
extern char* madras_get_insn_paramstr(elfdis_t* ed, int pos);

/**
 * Return a register name used in a parameter (of OT_REGISTER type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A string with the register name, or NULL if an error occurred.
 * In this case the last error code in \c ed will be filled
 * */
extern char* madras_get_register_name(elfdis_t* ed, int pos);

/**
 * Return a register name used as base in a parameter (of OT_MEMORY type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A string with the register name, or NULL if an error occurred.
 * In this case the last error code in \c ed will be filled
 * */
extern char* madras_get_base_name(elfdis_t* ed, int pos);

/**
 * Return a register name used as index in a parameter (of OT_MEMORY type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A string with the register name, or NULL if an error occurred.
 * In this case the last error code in \c ed will be filled
 * */
extern char* madras_get_index_name(elfdis_t* ed, int pos);

/**
 * Return an offset used in a parameter (of OT_MEMORY type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A 64b integer with the offset, 0 if there is no offset or if an error occured
 * In this case the last error code in \c ed will be filled
 * */
extern int64_t madras_get_offset_value(elfdis_t* ed, int pos);

/**
 * Return a integer used as scale in a parameter (of OT_MEMORY type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return An integer with the scale, 0 if there is no scale or a negative value if an error occured
 * In this case the last error code in \c ed will be filled
 * */
extern int madras_get_scale_value(elfdis_t* ed, int pos);

/**
 * Return a constant value in a parameter (of OT_CONSTANT type)
 * \param ed A pointer to the structure holding the disassembled file
 * \param pos The index of the operand (starting at 0 for the first operand)
 * \return A 64b integer with the value, or 0 value if an error occured
 * In this case the last error code in \c ed will be filled
 * */
extern int64_t madras_get_constant_value(elfdis_t* ed, int pos);

/**
 * Retrieves the name of the ELF section the instruction pointed to by the cursor
 * belongs to
 * \param ed A pointer to the structure holding the disassembled file
 * \return The name of the ELF section the cursor is in, or a NULL or negative value
 * if an error occurred
 * */
extern char* madras_get_scn_name(elfdis_t* ed);

/**
 * Retrieves the ELF label (function name or label) associated to the instruction pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \return The name of the label. If no label is associated with this instruction, returns NULL.
 * If an error occurred, the last error code in \c ed will be updated
 * */
extern char* madras_get_insn_lbl(elfdis_t* ed);

/**
 * Retrieves the line in the source file corresponding to the instruction
 * pointed to by the cursor
 * \param ed A pointer to the structure holding the disassembled file
 * \param srcfilename If not NULL, will contain a pointer to a char array
 * containing the name of the source file the line belongs to
 * \param srcline If not NULL, contains:
 * - The line in the source file corresponding to the instruction
 * - 0 if no line is associated to this instruction
 * - -1 if the file contains no debug information
 * \param srccol If not NULL, contain the column corresponding to the instruction.
 * \warning In the current version, column index is not retrieved (always 0).
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise (no defined cursor or no
 * debug information present in the file).
 * */
extern int madras_get_insn_srcline(elfdis_t* ed, char** srcfilename,
      int64_t* srcline, int64_t* srccol);

/**
 * Prints the current instruction in a format similar to objdump
 * \param ed A pointer to the structure holding the disassembled file
 * */
extern void madras_insn_print(elfdis_t* ed);

///**
// * Prints the current instruction in a format similar to objdump into an open file
// * \param ed A pointer to the structure holding the disassembled file
// * \param out An open file
// * */
//extern void madras_insn_fprint(elfdis_t* ed, FILE* out);

/**
 * Prints a list of instructions
 * \param ed A pointer to the structure holding the disassembled file
 * \param stream The stream to print to
 * \param startaddr The address at which printing must begin. If 0 or negative, the first address in the instruction list will be taken
 * \param stopaddr The address at which printing must end. If 0 or negative, the last address in the instruction list will be taken
 * \param printaddr Prints the address before an instruction
 * \param printcoding Prints the coding before an instruction
 * \param printlbl Prints the labels (if present)
 * \param before Function to execute before printing an instruction (for printing additional informations)
 * \param after Function to execute after printing an instruction (for printing additional informations)
 * */
extern void madras_insns_print(elfdis_t* ed, FILE* stream, int64_t startaddr,
      int64_t stopaddr, int printlbl, int printaddr, int printcoding,
      void (*before)(elfdis_t*, insn_t*, FILE*),
      void (*after)(elfdis_t*, insn_t*, FILE*));

/**
 * Prints a list of instructions in shell code
 * \param ed A pointer to the structure holding the disassembled file
 * \param stream The stream to print to
 * \param startaddr The address at which printing must begin. If 0 or negative, the first address in the instruction list will be taken
 * \param stopaddr The address at which printing must end. If 0 or negative, the last address in the instruction list will be taken
 * */
extern void madras_insns_print_shellcode(elfdis_t* ed, FILE* stream,
      int64_t startaddr, int64_t stopaddr);

/**
 * Return the hexadecimal coding of the instruction in a string format
 * \param ed A pointer to the structure holding the disassembled file
 * \return A string containing the representation of the coding, or NULL if the instruction has no coding.
 * \note This string has been allocated using malloc
 * */
extern char* madras_get_insn_hexcoding(elfdis_t* ed);

/**
 * Checks if a label is of type function.
 * \param ed A pointer to the structure holding the disassembled file
 * \param label Name of the label
 * \return 1 if the label is of type function in the ELF file, 0 otherwise. If the label was
 * not found or if no symbol table is present in the file, returns -1.
 * \note This function only checks if the label has ELF type \e STT_FUNC. This does not guarantee
 * that the associated label marks the beginning of a function in the binary
 * */
extern int madras_label_isfunc(elfdis_t* ed, char* label);

/**
 * Get bytes from memory
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr start address in the memory of the part to return 
 * \param len size (in number of bytes) of the part to return 
 * \return An array of bytes or NULL if there is a problem.
 * In this case, the last error code in \c ed will be filled
 */
extern unsigned char* madras_getbytes(elfdis_t* ed, int64_t addr,
      unsigned int len);

/**********************************************/
/*Functions used to modify a disassembled file*/
/**********************************************/
/**
 * Prepares a disassembled file for modification.
 * \param ed A pointer to the structure holding the disassembled file
 * \param stacksave Specifies the method to use to save the stack before 
 * inserting a function call. 
 * - \e STACK_KEEP will not move the stack (can cause bugs for codes 
 * using \c \%rsp or \c \%rbp as a base address)
 * - \e STACK_MOVE will move the stack to a new section in the ELF file 
 * (can cause crashes for multi-threaded codes)
 * - \e STACK_SHIFT will shift the stack before its bottom (can cause crashes
 * depending on the structure of the memory)
 * \param stackshift Value to shift the stack of. Must be specified is 
 * \c stacksave is set to \e STACK_SHIFT (a warning will be printed 
 * if specified for another value)
 * \return EXIT_SUCCESS if everything is ok, or an error code if an error
 * happened (file already being modified for instance)
 * */
extern int madras_modifs_init(elfdis_t* ed, char stacksave, int64_t stackshift);

/**
 * Adds a global option to the current patching session
 * \param ed Pointer to the madras structure
 * \param option Modification flag(s) to add
 * \return EXIT_SUCCESS if update was successful, error code otherwise
 * \note Options concerning the whole patching session are only taken into account when \ref madras_modifs_commit is invoked. In particular,
 * adding an option, adding modifications and removing the option will not add this option to the modifications added in between (\ref madras_modif_addopt
 * should be used to add the option to those modifications). There is currently no way to enable an option for the whole patching session and disable
 * it for some modifications
 * */
extern int madras_modifs_addopt(elfdis_t* ed, int option);

/**
 * Removes a global option to the current patching session
 * \param ed Pointer to the madras structure
 * \param option Modification flag(s) to remove
 * \return EXIT_SUCCESS if update was successful, error code otherwise
 * \note Options concerning the whole patching session are only taken into account when \ref madras_modifs_commit is invoked. In particular,
 * adding an option, adding modifications and removing the option will not add this option to the modifications added in between (\ref madras_modif_addopt
 * should be used to add the option to those modifications). There is currently no way to enable an option for the whole patching session and disable
 * it for some modifications
 * */
extern int madras_modifs_remopt(elfdis_t* ed, int option);

/**
 * Overrides the default choice of instruction used to pad blocks moved because of modifications for all modifications.
 * By default, the smallest \c nop instruction (no operation) for the relevant architecture is used.
 * If an instruction larger than this \c nop instruction is provided, an error will be returned
 * \param ed Pointer to the madras structure
 * \param insn Pointer to a structure representing the instruction to use
 * \param strinsn String representation of the assembly code for the instruction (used if \c insn is NULL)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int madras_modifs_setpaddinginsn(elfdis_t* ed, insn_t* insn,
      char* strinsn);

/**
 * Adds a library as a mandatory external library
 * \param ed A pointer to the structure holding the disassembled file
 * \param extlibname The name of the library to add
 * \return A pointer to the library modification object representing the addition if the operation succeeded, a null value otherwise
 * In that case, the last error code in \c ed will be filled
 * */
extern modiflib_t* madras_extlib_add(elfdis_t* ed, char* extlibname);

/**
 * Adds a library as a mandatory external library from a file descriptor
 * \param ed A pointer to the structure holding the disassembled file
 * \param extlibname The name of the library to add (mandatory). It may be different than the actual name of the file whose descriptor is provided
 * in \c filedesc.
 * \param filedesc The descriptor of the file containing the library to add (must be > 0)
 * \return A pointer to the library modification object representing the addition if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 * */
extern modiflib_t* madras_extlib_add_fromdescriptor(elfdis_t* ed,
      char* extlibname, int filedesc);

/**
 * Renames a dynamic library
 * \param ed A disassembled ELF file
 * \param oldname Name of the library to rename. If this name does not contain the '/' character, every references to this name (including
 * when preceded by a path) will be renamed (the preceding path will be kept identical if present).
 * Otherwise (if the name contains at least one '/' character), only the references matching exactly with the name given will be renamed.
 * \param newname New name of the library
 * \return A pointer to the library modification object representing the modification if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 */
extern modiflib_t* madras_extlib_rename(elfdis_t* ed, char* oldname,
      char* newname);

/**
 * Renames a dynamic symbol
 * \param ed a disassembled ELF file
 * \param library The library the new label must point to
 * \param oldname The old name of the label
 * \param newname The new name of the label
 * \return EXIT_SUCCESS or an error code
 */
extern int madras_extfct_rename(elfdis_t* ed, char* library, char* oldname,
      char* newname);

/**
 * Adds a request to a new global variable insertion into the file
 * \param ed The madras structure
 * \param size The size in bytes of the global variable
 * \param value A pointer to the value of the global variable (if NULL, will be filled with 0)
 * \return A pointer to the object defining the global variable or NULL if \c ed is NULL
 * */
extern globvar_t* madras_globalvar_new(elfdis_t* ed, int size, void* value);

extern int madras_fctcall_addparam_fromtlsvar(elfdis_t* ed, modif_t* modif,
      tlsvar_t* tlsv, char* str, char opt);

/**
 * Adds a request to a new tls variable insertion into the file
 * \param ed The madras structure
 * \param size The size in bytes of the tls variable
 * \param value A pointer to the value of the tls variable (if NULL, will be filled with 0)
 * \param type The type of the tls variable (INITIALIZED or UNINITIALIZED)
 * \return A pointer to the object defining the tls variable or NULL if \c ed is NULL
 */
extern tlsvar_t* madras_tlsvar_new(elfdis_t* ed, int size, void* value,
      int type);

/**
 * Updates the value of a global variable. This does not allow to change its size.
 * This function can be invoked after the invocation of madras_modifs_precommit
 * \param ed The madras structure
 * \param gv The global variable
 * \param value The new value of the variable
 * \return EXIT_SUCCESS if the operation is successful, error code otherwise
 * */
extern int madras_globvar_updatevalue(elfdis_t* ed, globvar_t* gv, void* value);

/**
 * Inserts a list of instruction into the file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param insns A queue of insn_t structures containing the instructions to add. \note It will be freed by \ref madras_terminate
 * \param addr Address at which the instruction list must be inserted, or 0 if the address of the instruction
 * pointed to by the cursor must be used
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param linkedvars Array of pointer to global variables structures. This array must contain exactly as many entries as \c insns contains
 * instructions referencing a global variable, in the order into which they appear in the list. It is advised to have it NULL-terminated to
 * detect errors more easily, but this is not required.
 * \param reassemble Boolean used to know if given instructions must be reassemble (TRUE) or not (FALSE).
 * \return A pointer to the code modification object representing the insertion if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 * */
extern modif_t* madras_add_insns(elfdis_t* ed, queue_t* insns, int64_t addr,
      insertpos_t after, globvar_t** linkedvars, tlsvar_t** linkedtlsvars,
      boolean_t reassemble);

/**
 * Inserts one instructions into the file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param insn A pointer to an insn_t structure representing the instruction to add
 * \param addr Address at which the instruction must be inserted, or 0 if the address of the instruction
 * pointed to by the cursor must be used
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param linkedvar A pointer to a global variable structure, if the inserted instruction references one.
 * \param reassemble Boolean used to know if given instructions must be reassemble (TRUE) or not (FALSE).
 * \return A pointer to the code modification object representing the insertion if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will be filled
 * */
extern modif_t* madras_add_insn(elfdis_t* ed, insn_t* insn, int64_t addr,
      insertpos_t after, globvar_t* linkedvar, tlsvar_t* linkedtlsvar,
      boolean_t reassemble);

/**
 * Inserts a list of instruction into the file.
 * \param ed A pointer to the structure holding the disassembled file
 * \param insnlist The list of instructions to add (terminated with character '\\0'). 
 * The instructions must be separated from one another by a '\\n' character from the next.
 * An instruction must respect the following format: \c MNEMO \c OPLIST where \c MNEMO is
 * the mnemonic of the instruction, and \c OPLIST the list of its operands, separated from one 
 * another by ',' and not containing any space. The operands format and order must be conform
 * to the AT&T syntax. 
 * \param addr Address at which the function call must be inserted, or 0 if the address of the instruction
 * pointed to by the cursor must be used
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param linkedvars Array of pointer to global variables structures. This array must contain exactly as many entries as \e insnlist contains
 * instructions referencing a global variable, in the order into which they appear in the list. It is advised to have it NULL-terminated to
 * detect errors more easily, but this is not required.
 * \return A pointer to the code modification object representing the insertion if the operation succeeded, a null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
extern modif_t* madras_insnlist_add(elfdis_t* ed, char* insn_list, int64_t addr,
      insertpos_t after, globvar_t** linkedvars, tlsvar_t** linkedtlsvars);

/**
 * Creates a new request for the insertion of a function in a file. The function is not necessarily called
 * \param ed A pointer to the structure holding the disassembled file
 * \param fctname Name of the function to insert
 * \param libname Name of the library containing the function.
 * - If not NULL, the library will be also added as with \ref madras_extlib_add.
 * - If NULL, this means the function is already present in the patched file.
 * \param fctcode Not implemented, reserved for future evolutions
 * \return A pointer to the code modification object representing the insertion if successful, NULL otherwise.
 * If an error occurred, the last error code in \c ed will have been filled
 * \warning This function is currently mainly intended for added function stubs needed by MIL. Its behaviour is undefined for many operations
 * */
extern modif_t* madras_fct_add(elfdis_t* ed, char* fctname, char* libname,
      char* fctcode);

/**
 * Creates a new request for the insertion of a function call. This function is identical to \ref madras_fctcall_new except that
 * the inserted function call will not be surrounded by instructions for saving/restoring the context and aligning the stack.
 * Therefore, it is up to the inserted function to take care of those task if needed, otherwise the patched file may crash or
 * present an undefined behaviour.
 * \param ed A pointer to the structure holding the disassembled file
 * \param fctname Name of the function to which a call must be inserted
 * \param libname Name of the library containing the function. If not NULL, equivalent to adding
 * \e libname through \ref madras_extlib_add
 * \param addr Address at which the function call must be inserted, or 0 if the address of the instruction
 * pointed to by the cursor must be used. If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \return A pointer to the code modification object representing the insertion request object or a NULL value if a failure occurred.
 * In that case, the last error code in \c ed will have been filled
 * \note This function can be replaced by a call to \ref madras_fctcall_new then \ref madras_modif_addopt with the option PATCHOPT_FCTCALL_NOWRAP
 * */
extern modif_t* madras_fctcall_new_nowrap(elfdis_t* ed, char* fctname,
      char* libname, int64_t addr, insertpos_t after);

/**
 * Links a branch instruction to another instruction at a given address
 * \param ed Pointer to the madras structure
 * \param in Pointer to the branch instruction to link
 * \param addr Address in the file to which the instruction must point
 * \param update Indicate if the target address has to be updated when insertion are performed before it
 * \return EXIT_SUCCESS if successful, error code if no instruction was found at the given address
 * \note The use of \ref madras_branch_insert is recommended for inserting branch instructions to a given address
 * */
int madras_set_branch_target(elfdis_t* ed, insn_t* in, int64_t addr, int update) __attribute__ ((deprecated)); // "Use madras_modify_branch instead"

/* Call madras_set_branch_target with update set to False */
extern int madras_linkbranch_toaddr(elfdis_t* ed, insn_t* in, int64_t addr) __attribute__ ((deprecated)); // "Use madras_modify_branch instead"

/**
 * Inserts an unconditional branch in the code to an existing address or another modification
 * \param ed A pointer to the MADRAS structure
 * \param addr The address where to insert the branch or -1 if the address of the instruction
 * pointed to by the cursor must be used. If 0, the branch will be inserted in the file but not linked to any address
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param modif An existing modification to point to (only MODTYPE_INSERT modifications are allowed)
 * \param dstaddr An address in the file (take into account only if \c modif is NULL) to point to. If set to -1 and \c modif is NULL, a return
 * instruction will be inserted
 * \param upd_if_patched Conditions the way the branch will be updated if \c dstaddr is used and and insertion is performed before the instruction
 * at this address. If set to TRUE, the inserted branch will point to the code inserted before the instruction, otherwise it will always point
 * to the instruction that was present at \c dstaddr
 * \return A pointer to the modification, or a NULL if the operation failed. In that case, the last error code in \c ed will be updated
 * */
extern modif_t* madras_branch_insert(elfdis_t* ed, int64_t addr,
      insertpos_t after, modif_t* modif, int64_t dstaddr,
      boolean_t upd_if_patched);

/**
 * Creates a new request for the insertion of a function call
 * \param ed A pointer to the structure holding the disassembled file
 * \param fctname Name of the function to which a call must be inserted
 * \param libname Name of the library containing the function.
 * - If not NULL, the library will be also added as with \ref madras_extlib_add.
 * - If NULL, this means the function is already present in the patched file.
 * \warning The behaviour of parameter \e libname is different from release 1.0
 * \param addr Address at which the function call must be inserted, or -1 if the address of the instruction
 * pointed to by the cursor must be used. If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \param pos Specifies where the instructions must be inserted relatively to the given address or instruction, using values from insertpos_t
 * \param reglist List of registers to save before the inserted call and restore afterwards. Set to NULL if all must be saved
 * \param nreg Size of \c reglist
 * \return A pointer to the code modification object representing the insertion request object or NULL if a failure occurred
 * In that case, the last error code in \c ed will have been filled
 * */
extern modif_t* madras_fctcall_new(elfdis_t* ed, char* fctname, char* libname,
      int64_t addr, insertpos_t pos, reg_t** reglist, int nreg);

/**\todo All occurrences of the "char opt" parameter should be replaced by an integer that would use values from an enum
 * This is much more coherent and explicit than using characters which are referenced nowhere else*/

/**
 * Adds a parameter, given in string format, to a function call request
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param param Parameter to add, given in the format used by the assembly code of the
 *          current architecture in AT&T format
 *          (e.g. for x86: \%RAX for a register, $42 for an immediate, 0x42(\%RAX) for memory) 
 * \param opt Contains options for the parameter: 
 *        - 'a' means the address pointed to by the memory operand must be taken as parameter
 *        - 'q', 'l', 'w' or 'b' means the value of the area pointed to by the memory 
 *         operand must be taken as parameter, with the given size.
 *         In the current version, all of these values are treated as 'q' (quadword)
 * \note \e opt is only used for memory and register operands
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
extern int madras_fctcall_addparam_fromstr(elfdis_t* ed, modif_t* modif,
      char* param, char opt);

/**
 * Adds a parameter to a function call equal to a pointer to a global variable.
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param gv Pointer to the global variable to use as parameter
 * \param str Pointer to a string to use for parameter (will be used only if \e gv is NULL)
 * \param opt Contains options for the parameter:
 *        - 'a' means the address of the global variable must be taken as parameter
 *        - 'q', 'l', 'w' or 'b' means the value of the area pointed to by the global variable
 *        must be taken as parameter, with the given size.
 *        In the current version, all of these values are treated as 'q' (quadword)
 *        This parameter is ignored if the \e str parameter is used
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 */
extern int madras_fctcall_addparam_fromglobvar(elfdis_t* ed, modif_t* modif,
      globvar_t* gv, char* str, char opt);

/**
 * Adds a parameter, taken from the instruction the cursor points to or at a given address, 
 * to a function call request 
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param idx Index of the parameter to add (starting at 0)
 * \param opt Contains options for the parameter: 
 *        - 'a' means the address pointed to by the memory operand must be taken as parameter
 *        - 'q', 'l', 'w' or 'b' means the value of the area pointed to by the memory 
 *         operand must be taken as parameter, with the given size
 *         In the current version, all of these values are treated as 'q' (quadword)
 * \param addr Address of the instruction from which the parameter must be taken, or 0 if the instruction
 * pointed to by the cursor must be used. If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \note So far \e opt it is used only for memory operands
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
extern int madras_fctcall_addparam_frominsn(elfdis_t* ed, modif_t* modif,
      int idx, char opt, int64_t addr);

/**
 * Adds an immediate parameter to a function call request.
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param imm Value of the immediate
 * \param opt Contains options for the parameter. 
 * \note In the current version, \e opt it is ignored (always as "\0")
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
extern int madras_fctcall_addparam_imm(elfdis_t* ed, modif_t* modif,
      int64_t imm, char opt);

/**
 * Adds a return value to a function call request. The return value will be copied to a global variable
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param ret Pointer to the global variable object, which must already be initialised
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
extern int madras_fctcall_addreturnval(elfdis_t* ed, modif_t* modif,
      globvar_t* ret);

/*
 * Adds a return value to a function call request. The return value will be copied to a tls variable
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif Pointer to the modification object representing the function call insertion request
 * \param ret Pointer to the tls variable object, which must already be initialised
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
extern int madras_fctcall_addreturntlsval(elfdis_t* ed, modif_t* modif,
      tlsvar_t* ret);

/**
 * Creates a new request for a replacement of a list of instructions
 * \param ed A pointer to the structure holding the disassembled file
 * \param ninsn Number of instructions to replace, starting either from the cursor's location or \e addr
 * \param addr Address of the first instruction to be replaced, or 0 if the instruction pointed to by
 * the cursor is the first address to be deleted.  If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \param fillerver Version of the function generating the instruction for replacing the instruction. It must have been
 * defined in the source file associated to the architecture. If set to 0, NOP instructions will be used.
 * \return A pointer to the code modification object representing the replacement if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * \warning! Return value used to be 0 if the operation succeeded. Update code accordingly.
 * */
extern modif_t* madras_replace_insns(elfdis_t* ed, int ninsn, int64_t addr,
      int fillerver) __attribute__ ((deprecated));

/**
 * Creates a new request for a replacement an instruction
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr Address of the instruction to be replaced, or 0 if the instruction pointed to by
 * the cursor is the first address to be deleted.  If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \param fillerver Version of the function generating the instruction for replacing the instruction. It must have been
 * defined in the source file associated to the architecture. If set to 0, NOP instructions will be used.
 * \return A pointer to the code modification object representing the replacement if the operation succeeded, null otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
extern modif_t* madras_replace_insn(elfdis_t* ed, int64_t addr);

/**
 * Creates a new request for a deletion of a list of instructions
 * \param ed A pointer to the structure holding the disassembled file
 * \param ninsn Number of instructions to delete, starting either from the cursor's location or \e addr
 * \param addr Address of the first instruction to be deleted, or 0 if the instruction pointed to by
 * the cursor is the first address to be deleted.  If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \return A pointer to the code modification object representing the deletion if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * \warning! Return value used to be 0 if the operation succeeded. Update code accordingly.
 * */
extern modif_t* madras_delete_insns(elfdis_t* ed, int ninsn, int64_t addr) __attribute__ ((deprecated));

/**
 * Creates a new request for a deletion of an instruction
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr Address of the instruction to be deleted, or 0 if the instruction pointed to by
 * the cursor is the first address to be deleted.  If an address is provided, the cursor will be positioned
 * at this address (equivalent to a call to \ref madras_init_cursor)
 * \return A pointer to the code modification object representing the deletion if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
extern modif_t* madras_delete_insn(elfdis_t* ed, int64_t addr);

/**
 * Creates a new request for a modification of an instruction
 * \warning The current implementation of this function does not make any "sanity check" concerning the
 * validity of the instruction provided as replacement.
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr The address of the instruction to replace. If set to -1, the instruction pointed to by
 * the cursor will updated instead. Otherwise, it will be set at this address
 * \param withpadding If set to 1, and if an instruction is replaced by a shorter one, the difference in
 * size will be padded by a NOP instruction. Otherwise, no padding will take place. This is ignored if the
 * instruction is replaced by a longer one.
 * \param newopcode The opcode to replace the original opcode with, or NULL if the original must be kept
 * \param noperands The number of operands of the new instruction.
 * \param ... The operands, each in char* format, that must replace the operands of the original instruction.
 * They must appear in the order they would appear in in the AT&T assembly representation of the instruction.
 * A NULL value indicates that the corresponding operand must not be replaced. The number of operands in this
 * last list must be the same as the value of noperands.
 * \return A pointer to the code modification object representing the modification if the operation succeeded, null value otherwise
 * If an error occurred, the last error code in \c ed will have been filled
 * */
extern modif_t* madras_modify_insn(elfdis_t* ed, int64_t addr,
      boolean_t withpadding, char* newopcode, int noperands, ...);

/* Exactly the same as `madras_modif_insn` except that it takes an array of operands instead of varargs
 */
extern modif_t* madras_modify_insn_array(elfdis_t* ed, int64_t addr,
      boolean_t withpadding, char* newopcode, int noperands, char ** operands);

/**
 * Creates a new request for modifying a direct branch instruction
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr The address of the branch instruction to replace. If set to -1, the instruction pointed to by
 * the cursor will updated instead. Otherwise, it will be set at this address. If the instruction at this address
 * is not a branch, an error will be returned.
 * \param withpadding If set to 1, and if an instruction is replaced by a shorter one, the difference in
 * size will be padded by a NOP instruction. Otherwise, no padding will take place. This is ignored if the
 * instruction is replaced by a longer one.
 * \param newopcode The opcode to replace the original opcode with, or NULL if the original must be kept.
 * If the new opcode does not correspond to a branch instruction, an error will be returned
 * \param newdestaddr New destination address of the branch
 * \return A pointer to the code modification object representing the modification if the operation succeeded, null or negative value otherwise
 * */
extern modif_t* madras_modify_branch(elfdis_t* ed, int64_t addr,
      int withpadding, char* newopcode, int64_t newdestaddr);

/**
 * Flags an instruction at a given address to be moved to the section of relocated code. No other modification is performed.
 * The size of the block moved around the instruction will depend on the flags set on the modif_t object.
 * \param ed A pointer to the structure holding the disassembled file
 * \param addr Address of the instruction to move, or 0 if the address of the instruction
 * pointed to by the cursor must be used
 * \return A pointer to the code modification object representing the forced relocation if the operation succeeded, NULL otherwise.
 * If an error occurred, the last error code in \c ed will be filled
 * */
extern modif_t* madras_relocate_insn(elfdis_t* ed, int64_t addr);

/**
 * Adds a request for inserting label into the file
 * \param ed The madras structure
 * \param lblname The name of the label to add
 * \param addr The address at which the label must be added. If set to -1, the current cursor will be used
 * \param lbltype The type of the label to add (FUNCTION, NONE or DUMMY)
 * \param fixed If set to TRUE, the label will be inserted at the given address. If set to FALSE,
 * the label will be inserted at the closest possible location of instruction at adress \e addr, even if it was moved or deleted
 * \return EXIT_SUCCESS if operation succeeded, error code otherwise
 * */
extern int madras_label_add(elfdis_t* ed, char* lblname, int64_t addr,
      int lbltype, int fixed);

/**
 * Adds a request for inserting label into the file
 * \param ed The madras structure
 * \param lblname The name of the label to add
 * \param list An instruction list where the label must be added.
 * \param lbltype The type of the label to add (FUNCTION, NONE or DUMMY)
 * the label will be inserted at the closest possible location of instruction at adress \e addr, even if it was moved or deleted
 * \return EXIT_SUCCESS if operation succeeded, error code otherwise
 * */
extern int madras_label_add_to_insnlist(elfdis_t* ed, char* lblname,
      list_t* list, int lbltype);

/**
 * Creates a new condition
 * \param ed A pointer to the MADRAS structure
 * \param condtype Type of the condition. If the condition is logical, \c cond1 and \c cond2 will be taken into account,
 * otherwise if it is a comparison, \c insn, \c opidx and \c condval will be used.
 * \param oprnd Operand whose value is needed for the comparison
 * \param condval Value to compare the operand to (for comparison conditions)
 * \param cond1 Sub-condition to use (for logical conditions).
 * \param cond2 Sub-condition to use (for logical conditions)
 * \return A pointer to the new condition
 * \warning \c cond1 and \c cond2 must not be sub-conditions of another condition or already associated to a modification
 * */
extern cond_t* madras_cond_new(elfdis_t* ed, int condtype, oprnd_t* oprnd,
      int64_t condval, cond_t* cond1, cond_t* cond2);

/**
 * Force the padding instruction to be used for a given modification. This overrides the instruction chosen for the patching
 * sessions (the \c nop instruction by default, or the instruction provided by \ref madras_modifs_setpaddinginsn).
 * If an instruction larger than this \c nop instruction is provided, an error will be returned
 * \param ed Pointer to the madras structure
 * \param modif The modification for which we want to update the padding type
 * \param insn Pointer to a structure representing the instruction to use
 * \param strinsn String representation of the assembly code for the instruction (used if \c in is NULL)
 * \return EXIT_SUCCESS if successful, error code otherwise
 * \todo Define this function in libmpatch and invoke it here
 * */
extern int madras_modif_setpaddinginsn(elfdis_t* ed, modif_t* modif,
      insn_t* insn, char* strinsn);

/**
 * Returns the labels defined in an inserted library.
 * \param ed Pointer to the madras structure
 * \param modlib The \ref modiflib_t structure describing an library insertion
 * \param labels Queue to be filled with the labels in the library.
 * Must be initialised or set to NULL if it is not needed.
 * \param labels_table Hashtable (indexed on label names) to be filled with the labels in the library.
 * Must be initialised (using str_hash and str_equal for hashing and comparison functions) or set to NULL if it is not needed.
 * \return EXIT_SUCCESS if the operation went successfully, error code otherwise (e.g. \c modlib is not a library insertion).
 * TRUE will be returned if \c labels and \c labels_table are both NULL (but it is useless)
 * */
extern int madras_modiflib_getlabels(elfdis_t* ed, modiflib_t* modlib,
      queue_t* labels, hashtable_t* labels_table);

/**
 * Adds a flag to an inserted library.
 * \param ed Pointer to the madras structure
 * \param modlib The \ref modiflib_t structure describing an library insertion
 * \param flag a flag to add. Allowed flags are PATCHOPT_LIBFLAG_...
 * \return EXIT_SUCCESS if the operation went successfully, error code otherwise
 */
extern int madras_modiflib_add_flag(elfdis_t* ed, modiflib_t* modlib, int flag);

/**
 * Returns the library associated to a new function call
 * \param ed Pointer to the madras structure
 * \param modif a function insertion
 * \return The modiflib associated to a function insertion, else NULL.
 * In this case, the last error code in \c ed will be filled
 */
extern modiflib_t* madras_fctlib_getlib(elfdis_t* ed, modif_t* modif);

/**
 * Adds a condition to the execution of a modified code. If the condition is satisfied the modified code will be executed, otherwise the original code will be.
 * \warning The current version of this function only supports code insertion modifications. Use with deletion or modification is not supported
 * \param ed A pointer to the MADRAS structure
 * \param modif A pointer to the modification object to add the condition to
 * \param cond The condition to add
 * \param condtype If an existing condition was already present for this insertion, the new condition will be logically added to the existing one
 * If set to 0, LOGICAL_AND will be used
 * \return EXIT_SUCCESS if successful, error code otherwise
 * \warning \c cond must not be sub-condition of another condition or already associated to a modification
 * */
extern int madras_modif_addcond(elfdis_t* ed, modif_t* modif, cond_t* cond,
      int condtype);

/**
 * Adds a condition from its string representation to the execution of a modified code. See \ref madras_modif_addcond for precisions on conditions
 * \warning The current version of this function only supports code insertion modifications. Use with deletion or modification is not supported
 * \param ed A pointer to the MADRAS structure
 * \param modif A pointer to the modification object to add the condition to
 * \param strcond The string representation of the condition. Conditions must obey the following syntax:
 * - A condition must be enclosed by parenthesis ( '(' and ')' )
 * - A condition is either written as: ( oprnd comparison value ) or ( condition1 logical condition2 )
 * -- oprnd is a register or memory assembly operand, enclosed by quotes ( '"' )
 * -- comparison is one of the following comparison operators: ==, !=, <, >, <=, >=
 * -- value is a numerical value
 * -- condition1 and condition2 are other conditions (obeying the same syntax)
 * -- logical is one of the following logical operators: &&, ||
 * -- Spaces can be present between the various elements of a comparison
 * \param gvars Array of added global variables to be used in the conditions. \note NOT USED IN THIS VERSION
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int madras_modif_setcond_fromstr(elfdis_t* ed, modif_t* modif,
      char* strcond, globvar_t** gvars);

/**
 * Adds a modification to perform if the modification made on a condition is not met. This has no effect if the modification has no condition
 * \note In the current version only modifications of type MODTYPE_INSERT are supported for \c modif and \c elsemod
 * \param ed The madras structure
 * \param modif The modification for which we want to add an else condition
 * \param elsemod The modification to execute if the condition is not met
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
extern int madras_modif_addelse(elfdis_t* ed, modif_t* modif, modif_t* elsemod);

/**
 * Links a code modification to another or an address. The control flow in the patched file will jump to the beginning
 * of the modification or the address in the original code after having executed the first modification, regardless of where
 * it was supposed to continue to.
 * \note In the current version only modifications of type MODTYPE_INSERT are supported for \c modif and \c modln.
 * \param ed The madras structure
 * \param modif The modification we want to link to another
 * \param modln Another modification to link to. The code from this modification will be executed right after this one.
 * \param addrln If \c modln is NULL, the address in the original code to link to.
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
extern int madras_modif_setnext(elfdis_t* ed, modif_t* modif, modif_t* modln,
      int64_t addrln);

/**
 * Adds an option flag to an existing modification. Any flag added this way will override flags set for the patching session using
 * \ref madras_modifs_addopt
 * \param ed The madras structure
 * \param modif The modification to update
 * \param opt The flag corresponding to the option to add to the modification
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * */
extern int madras_modif_addopt(elfdis_t* ed, modif_t* modif, int opt);

/**
 * Removes an option flag to an existing modification
 * \param ed The madras structure
 * \param modif The modification to update
 * \param opt The flag corresponding to the option to remove to the modification
 * \return EXIT_SUCCESS if the operation succeeded, error code otherwise
 * \warning Using this function to remove an option flag that is enforced for the whole patching session using \ref madras_modifs_addopt will
 * have no effect
 * */
extern int madras_modif_remopt(elfdis_t* ed, modif_t* modif, int opt);

/**
 * Finalises a modification.
 * \param ed A pointer to the structure holding the disassembled file
 * \param modif The modification
 * \return TRUE if the modif can be enqueued, FALSE otherwise
 * */
extern int madras_modif_commit(elfdis_t* ed, modif_t* modif);

/**
 * Changes the OS the binary is computed for
 * \param ed The madras structure
 * \param code The code of the new targeted OS. Values are defined
 *             by macros ELFOSABI_... in elf.h file
 * \return EXIT_SUCCESS if operation succeeded, error code otherwise
 */
extern int madras_change_OSABI(elfdis_t* ed, char code);

/**
 * Change the targeted machine the binary is compute for
 * \param ed The madras structure
 * \param machine The new value of the targeted machine, defined by macros EM_...
 *        in elf.h file
 * \return EXIT_SUCCESS if operation succeeded, error code otherwise
 */
extern int madras_change_ELF_machine(elfdis_t* ed, int ELF_machine_code);

extern insn_t* madras_generate_NOP(elfdis_t* ed, int size);

/**
 * Retrieves a queue of madras_addr_t structures, containing the correspondence between addresses in the original file and
 * in the patched file. This function must be invoked after madras_modif_precommit, and the PATCHOPT_TRACK_ADDRESSES option
 * must have been set beforehand.
 * \param ed The madras structure
 * \param modifonly If set to TRUE, the returned list will only contain modified addresses, otherwise the full list of addresses
 * will be returned
 * \return Queue of madras_addr_t structures, containing an element per instruction in the original file if modifonly is FALSE,
 * or one element per modified address if modifonly is TRUE, and ordered by increasing original address,
 * or NULL if the PATCHOPT_TRACK_ADDRESSES option was not set.
 * \warning This function may display undefined behaviour and possibly cause a crash if the patching session involved invocations
 * of \ref madras_replace_insns or madras_modify_insn
 * */
extern queue_t* madras_getnewaddresses(elfdis_t* ed, int modifonly);

/**
 * Frees a queue of madras_addr_t structures, such as returned by madras_getnewaddresses
 * \param madras_addrs A queue of madras_addr_t structures
 * */
extern void madras_newaddresses_free(queue_t* madras_addrs);

/**
 * Commits the modifications made to a disassembled file and saves the result to another file
 * All pending modification requests will be committed, then flushed.
 * \param ed A pointer to the structure holding the disassembled file
 * \param newfilename Name of the file under which the modified file must be saved 
 * \return EXIT_SUCCESS if successful or error code otherwise
 * */
extern int madras_modifs_commit(elfdis_t* ed, char* newfilename);

/**
 * Commits the modifications made to a disassembled file without writing the patched file
 * All pending modification requests will be committed.
 * \param ed A pointer to the structure holding the disassembled file
 * \param newfilename Name of the file under which the modified file must be saved
 * \return EXIT_SUCCESS if successful or error code otherwise
 * */
extern int madras_modifs_precommit(elfdis_t* ed, char* newfilename);

/**
 * Writes a patched file whose modifications have already been committed
 * All pending modification requests will be flushed.
 * \param ed A pointer to the structure holding the disassembled file
 * \return EXIT_SUCCESS if successful or error code otherwise
 * */
extern int madras_modifs_write(elfdis_t* ed);

/***********************/
/*Termination functions*/
/***********************/
/**
 * Frees the elfdis structure, and closes the associated file
 * \param ed A pointer to the structure holding the disassembled file
 * */
extern void madras_terminate(elfdis_t *ed);

#endif /*LIBMADRAS_H_*/
