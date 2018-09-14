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
 * \file fsmutils.h
 * \page disass_fsmdesc Finite state machine
 *
 * \brief This page describes the implementation of the LR(0) parser used by the 
 * \e libmdisass disassembler as a finite state machine automaton.
 *
 * We will not describe here the general principles behind LR-parsing and finite state machines.
 *
 * \section fsmmain Main principles
 *
 * The main components the automaton uses are the following:
 * - <b>Input stream</b>: The stream of data to parse, and the pointer to the character being currently read
 * - <b>Transition end pointer</b>: Pointer in the input stream to the byte immediately after the already processed bytes
 * - <b>States</b>: Array of states for the finite state machine being currently used to parse the input stream.
 * - <b>Buffer</b>: Stack containing the results of the reductions from the current parsing operation and the associated states
 * - <b>Tokens</b>: Array containing the values of the tokens decoded during the current parsing operation.
 * - <b>Variables</b>: Array containing the results of the executions of the semantic actions of reduced 
 *                     variables during the current parsing operation
 *
 * All informations needed by the automaton are stored in a \ref fsmcontext_t structure.
 *
 * \subsection fsmstates Automaton states
 * The states of an automaton are stored into \ref fsmstate_t structure. There are two types of states:
 * - <b>Shift states</b> are states leading to other states. They are associated to a list of transitions values, accessed
 * through the array of the first sub values of the transitions.
 * - <b>Reduction states</b> are "leaf" states corresponding to the reduction of a symbol. 
 *   They are associated to a semantic action.
 *
 * Some of the reduction states are also associated to a final action, that will have to be executed 
 * once a word is parsed. For the disassembler, these final actions are used to fill the extensions.
 *
 * \subsection fsmtrans Automaton transitions
 *
 * The transitions in a state are separated between transitions on a reduced grammar symbol and on binary values.
 *
 * Transitions on a reduced grammar symbol are stored as a table indexed on the identifier of the symbols, containing the
 * identifier of the next state associated to this transition value. If a state does not contain a transition over a symbol,
 * the corresponding cell in the table contains a special identifier indicating a match failure.
 *
 * Transitions on values are broken down into a series of sub values 1 byte long or smaller. Each sub value is accompanied by a mask specifying
 * which bits of the sub value are useful (the corresponding bit in the mask is set to 1 if the bit is useful, 0 otherwise).
 * Each sub value contains either a table containing the following sub values in the transition or the identifier of the next state if the
 * sub value was the last in the transition. A cell in a table of sub values may contain a list of sub values with different masks,
 * ordered by priority of the corresponding transitions.
 *
 * \section fsminit Initialisation
 * \subsection fsminitfsm Initialising FSM context
 * During initialisation of the FSM, a new \ref fsmcontext_t structure is created using \ref fsm_init.
 * This creation needs a pointer to the function loading the details of the FSM to use. Such a function must 
 * have the following characteristics:
 * - \b Input: A pointer to an already allocated \ref fsmcontext_t structure
 * - \b Actions: Initialises the following members of the structure:
 *  - Number of symbols (token and variables) used in the automaton.
 *  - Array of states used by the automaton. This array must be ordered so that the first state of the automaton is the first in the array.
 *  - Array of pointers to the functions to execute at the end of a successful parsing.
 *  - Maximum size in bits of an instruction.
 *  - Maximum size in bits of a transition.
 *
 * Those parameters will remain loaded until the FSM is terminated and its context is freed. In the context of the disassembler,
 * they are equivalent to loading the specifics for the parsing of a given architecture in the FSM.
 *
 * Optionally, an error handler can be set using \ref fsm_seterrorhandler. This allows to have a specific 
 * function being executed on the current output when a parsing error occurs.
 *
 * \subsection fsminitparser Initialising parsing run
 *
 * A parsing run is the parsing of a given input stream. The stream to parse is initialised using 
 * \ref fsm_setstream. This allows to set a string of bytes, the offset in the string at which the parsing 
 * must begin and the number of bytes to parse in the string.
 *
 * The \ref fsm_parseinit function must next be invoked to signal the parser that a new run will begin.
 *
 * This step can be repeated multiple times for a given FSM context if multiple runs on different streams are 
 * needed for a given FSM.
 *
 * \section fsmparse Parsing a stream
 *
 * During the parsing of a stream, the automaton will read the stream attempting to parse a word. Once 
 * the parsing is complete, either because a word has been parsed or a parsing error occurred, the automaton 
 * will be reset and the next word will be read. If the length of stream to parse is reached, the parsing 
 * will stop.
 *
 * \subsection fsmmatchtrans Matching with transitions
 *
 * Matching an input value against a group of transitions on values is performed through the following algorithm:
 * -# Initialising the array of sub values with the array of the first sub values in the transition
 * -# While the next state is not found
 *   - If the table is marked as always OK, check if the length remaining in the input stream is bigger than the
 *   length of the sub values in the table and raise a parser error if it is not
 *   - If the table is marked as containing a single sub value, retrieve a value with the length of the sub values from the input stream
 *   and check it against the sub value in the table. The transition end pointer is then moved by the length of the sub value.
 *   - If the table is marked as containing multiple values, retrieve a value with the length of the sub values from the input stream
 *   and looks up the cell whose index is this value. The transition end pointer is then moved by the length of the sub value.
 *     - If the cell does not contain a sub value, raise a parser error
 *     - If the cell contains a list of sub values and masks, check them against the input value
 *   - If no parser error was raised, a sub value matching the input was found.
 *     - If it contains a state identifier, the transition is complete.
 *     - Otherwise, it contains an array of the next sub values, which is loaded as the current array.
 *
 * \subsection fsmparseword Parsing a word
 *
 * The parsing of a word is performed according to the following steps:
 * -# Resetting the FSM. The buffer is emptied and the stack of states is reset to contain the first state of the automaton
 * -# Reading a number of bytes equal to the maximum length of an instruction (rounded up if necessary)
 * -# Processing the state on the top of the stack:
 *  - If it is a shift state, using its transitions to identify the next state
 *      - If a reduction just occurred, the transition will be attempted matching the identifier of the reduced symbol (see \ref fsmtrans).
 *      - If this match failed or if no reduction occurred, the transition will be attempted on the values from the input stream.
 *      The algorithm described in \ref fsmmatchtrans is executed, with the array of sub values initialised to the array of first sub values
 *      for the current state.
 *      - If a match is made, the state associated to the transition is added to the top of the stack of states. The pointer to the input stream
 *      is shift to the location of the transition end pointer.
 *      - If no match is made
 *         - If the state contains a default transition (transition over a null value), the corresponding state is added to the top
 *         of the stack of states.
 *         - Otherwise an error is raised. The error handler function is executed and the FSM is reset.
 *  - If it is a reduction state, it contains informations on the elements to retrieve and where to retrieve them
 *	 	-# The reductions will be performed by removing the bits from the entry buffer
 *	        - Bit fields are removed and discarded
 *	        - Tokens are reduced into \ref paramcoding_t structures containing their value and length, and
 *         stored in a cell of the array of reduced values from the \ref fsmcontext_t structure corresponding
 *         to their identifier from the grammar
 *          - Variables (symbols that have been reduced) are added to the buffer, but their value is not kept.
 *	 	-# The semantic action associated to the state is added to the list of semantic actions.
 *	 	-# If the state is associated to a final action, the final action to perform is set for the parsing 
 *       of this word. No word should be associated to more than one final action.
 *	 	-# The stack of states is updated to remove the states associated to all symbols removed from the buffer.
 *	    -# The reduced variable is returned, to be used as input for a possible match for the next shift state
 *	- If the symbol \c Axiom has been reduced, the processing of the states stops.
 * -# The list of semantic actions is executed, in the order of their occurrences
 * -# The pointer to the result of the parsing is added to the first cell of the array of reduced symbols
 * -# If there is a final action, it is executed
 * -# The array of reduced symbols and the list of semantic actions are reset
 * */

#ifndef FSMUTILS_H_
#define FSMUTILS_H_

#include "libmcommon.h"
#include "libmasm.h"

/**\todo Reorder the members of all structures defined here to align them and reduce the size, and update MINJAG accordingly*/
/**\todo Some of the functions here could be merged. For instance, \ref fsm_setstream and \ref fsm_parseinit,
 * or \ref fsmcontext_new and \ref fsm_seterrorhandler. This would simplify the API for the FSM*/
/**\todo Declare once and for all the types used for representing common elements, such as the size in bits of a transition or a value to test.
 * The code will gain in clarity and it will avoid those annoying "unsigned vs signed comparison" compiler warnings (not to mention accidental overflows)*/

/**
 * Enumeration of the possible types for states (used in \ref fsmstate_t)
 * */
enum fsmstate_type {
   FSMSTATE_SHIFT = 0, /**<Code identifying a shift state*/
   FSMSTATE_REDUC, /**<Code identifying a reduction state*/
   FSMSTATE_SHRDC, /**<Code identifying a shift/reduce state*/
   FSMSTATE_FINAL /**<Code identifying the final state of the FSM indicating the parsing is complete*/
};

/**
 * Enumeration of the possible types for reductions (used in \ref fsmreduction_t)
 * */
enum reduc_type {
   REDUC_CST = 0, /**<Code identifying a reduction of constant (bitfield) value*/
   REDUC_TOK, /**<Code identifying a reduction of a token (terminal)*/
   REDUC_VAR /**<Code identifying a reduction of a variable (nonterminal)*/
};

/**\warning The names for tose defines are duplicated in MINJAG (\e filegenerator.c). If they need to be changed, MINJAG should be updated accordingly
 * \todo Maybe use a common reference file for those kind of variables*/
#define STATE_FINAL     -1  /**<Code identifying the final state of a parsing operation*/
/**\todo See comment in \ref fsm_setnextstate for the need of getting rid of STATE_FINAL*/
#define STATE_NONE      -2  /**<Absent state (used to detect nonexistent transitions in the list of transitions on variables)*/
#define STATE_LOOKING   -3  /**<Still looking for the next state*/

/**Alias for struct fsmtrans_subtbl_s structure*/
typedef struct fsmtrans_subtbl_s fsmtrans_subtbl_t;

/**
 * Structure holding the details about a subvalue in a transition
 * */
typedef struct fsmtrans_subval_s {
   fsmtrans_subtbl_t* nextsubval; /**<Array of subvalues following this value*/
   int next_state_id; /**<Identifier of the next state after this transition (must be its index in the array containing all states)*/
   uint16_t translen; /**<Length in bits of the corresponding transition (if \c next_state_id is set)*/
   uint32_t value; /**<Value of the transition subvalue (undefined bits set to 0)*/
   uint32_t mask; /**<Mask to apply to the transition subvalue (undefined bits set to 0)*/
} fsmtrans_subval_t;

/**
 * Identifiers for the possible types for a transition subvalue list
 * */
enum sublst_types {
   SUBLST_NOMATCH = 0, /**<Subvalue does not match for this state*/
   SUBLST_ALWAYSOK, /**<List is always a match (no tests needed, array is of size 1)*/
   SUBLST_1VAL, /**<List contains one element but a test is needed (array is of size 1)*/
   SUBLST_NVALS /**<Subvalue list contains more than 1 element*/
};

/**
 * Structure holding a list of transition subvalues
 * */
typedef struct fsmtrans_sublst_s {
   fsmtrans_subval_t** vals; /**<Array of sub values*/
   uint8_t type; /**<Type of the list*/
   uint16_t n_vals; /**<Number of values in the list (size of array \c vals)*/
} fsmtrans_sublst_t;

/**
 * Identifiers for the possible types for a transition subvalue hash table
 * */
enum subtbl_types {
   SUBTBL_ALWAYSOK = 0,/**<Table contains a single value that always matches (no test needed)*/
   SUBTBL_SINGLEVALUE, /**<Table contains a single value (array is of size 1)*/
   SUBTBL_HASHTABLE /**<Table is an array*/
};

/**
 * Structure holding a table of transition subvalues
 * */
struct fsmtrans_subtbl_s {
   fsmtrans_sublst_t** lsts; /**<Hash table of transition lists*/
   uint16_t *offsets; /**<Offsets from the current input where the parts of the values begin*/
   uint16_t *sizes; /**<Size in bits of the parts of subvalues*/
   uint16_t noffs; /**<Number of parts of the subvalues (sizes of \c offsets and \c sizes)*/
   uint8_t type; /**<Type of the table*/
};

typedef void (*semfct_t)(void**); /**<Type of the functions corresponding to semantic actions*/
typedef void (*extfct_t)(void*, void*); /**<Type of the functions invoked at the end of a parsing after 
 having reduced an instruction*/

/**
 * \brief Structure holding the details about a reduction operation
 * */
typedef struct fsmreduction_s {
   uint16_t sym_id; /**<Identifier of the symbol to reduce*/
   uint16_t symlen; /**<Length of the symbol to reduce*/
   uint8_t symtype; /**<Type of the symbol to reduce (bitfield, token or variable)*/
   uint8_t endianness; /**<Endianness of the value to reduce*/
} fsmreduction_t;

/**
 * \brief Structure holding the details about a reduction state
 * */
typedef struct statereduc_s {
   fsmreduction_t** reducs; /**<Reductions to perform*/
   semfct_t semactfct; /**<Semantic action to perform after reduction*/
   uint16_t var_id; /**<Identifier of the symbol we are reducing (left hand side in the grammar)*/
   uint16_t n_reducs; /**<Number of reductions (size of the reducs array)*/
   uint16_t endact_id; /**<Identifier of the function to execute after the parsing of a word containing
    this symbol is complete (final action)*/
} statereduc_t;

/**
 * \brief Structure holding the details about a shift state
 * */
typedef struct stateshift_s {
   int16_t *vartrans; /**<Array of transitions over variables (contains the identifier of the next state, indexed by the variable identifier)*/
   uint16_t *shiftvars; /**<Array containing the additional number of bits to shift in the input buffer for each variable (can be 0)*/
   fsmtrans_subtbl_t* begintbl; /**<Hash table for the first values for matching with the shift state*/
   int elsestate; /**<Identifier of the next state if no match is performed on the table (could be STATE_NONE)*/
} stateshift_t;

/**
 * \brief Structure holding the details about a shift/reduce state
 * */
typedef struct stateshrdc_s {
   stateshift_t* stateshift; /**<Shift state containing the shifts to attempt*/
   statereduc_t* statereduc; /**<Reduction state containing the reduction to perform if the shift failed*/
} stateshrdc_t;

/**
 * \brief Structure holding the informations about an FSM state
 * */
typedef struct fsmstate_s {
   union {
      statereduc_t* reduc; /**<Pointer to a reduction state structure (if type is set to FSMSTATE_REDUC)*/
      stateshift_t* shift; /**<Pointer to a shift state structure (if type is set to FSMSTATE_SHIFT)*/
      stateshrdc_t* shrdc; /**<Pointer to a shift/reduce state structure (if type is set to FSMSTATE_SHRDC)*/
   } state; /**<Pointer to the details of the state (either shift or reduction)*/
   uint16_t firsttested; /**<Index of the first bit already tested ahead of this state. Will be 0 if all tested bits are before this state*/
   uint8_t type; /**<Specifies whether the state if a shift state or reduction state*/
} fsmstate_t;

/**
 * \brief Operand coding
 * This structure stores all data about how an operand's parameter is coded
 * inside the coding of the assembly instruction it belongs to
 * */
typedef struct paramcoding_s {
   int64_t value; /**<Value of the parameter in the coding*/
   uint8_t length; /**<Length of the parameter (in bits) in the coding of the instruction*/
} paramcoding_t;

/**
 * \brief Structure containing the information characterising the FSM being run. It is filled by architecture-specific loader.
 * \note This structure is mainly used to avoid exposing the contents of the fsmcontext structure
 * */
typedef struct fsmload_s {
   fsmstate_t** states; /**<Pointer to the array of all the states in the FSM. The first element in the array must be the first state of the FSM*/
   extfct_t* finalfcts; /**<Array of functions to execute at the end of a successful parsing*/
#ifndef NDEBUG
   char** varnames; /**<Array of names of the FSM variables, used only for debug printing*/
#endif
   unsigned int insn_maxlen; /**<Maximum length in bits of an instruction for the given architecture*/
   unsigned int insn_minlen; /**<Minimum length in bits of an instruction for the given architecture*/
   uint16_t n_variables; /**<Number of variables in the FSM. Will be used for the sizes of the \e actions and \e variables arrays in fsmcontext_t*/
} fsmload_t;

/**Structure containing the information about a FSM session in progress*/
typedef struct fsmcontext_s fsmcontext_t;

/**
 * Termination function that must be called after a binary stream
 * has been successfully disassembled
 * \param fc The context of the FSM
 * */
extern void fsm_parseend(fsmcontext_t* fc);

/**
 * Returns the address being parsed, based on the bytes already read and the first address of the parsed stream
 * \param fc The current FSM context
 * \return The current address being parsed (in bytes)
 * */
extern int64_t fsm_getcurrentaddress(fsmcontext_t* fc);

/**
 * Returns the binary coding of the word that was parsed. It is advised to invoke this only after completing fsm_parse
 * \param fc Pointer to the structure containing the current FSM context
 * \return Allocated bitvector_t containing the binary coding of the word
 * */
extern bitvector_t* fsm_getcurrentcoding(fsmcontext_t* fc);

/*
 * Returns the maximum instruction's length in the current FSM
 * \param fc Pointer to the structure containing the current FSM context
 * \return The maximum instruction's length
 */
extern unsigned int fsm_getmaxinsnlength(fsmcontext_t* fc);

/**
 * Initialises the variables in a FSM context used for a parsing.
 * Has to be called before launching a parse operation
 * \param fc The FSM context (must already be initialised)
 * */
extern void fsm_parseinit(fsmcontext_t* fc);

/**
 * Checks if the parsing is completed
 * \param fc The FSM context
 * \return 1 if there is no more byte to parse, 0 otherwise
 * */
extern int fsm_isparsecompleted(fsmcontext_t* fc);

/**
 * Performs the parsing of a binary stream
 * \param fc The structure holding the details about a FSM operation
 * \param out Return parameter. Will contain a pointer to the object generated by the parsing
 * \param finalinfo Structure to pass to the finalizing macro
 * \param userinfo Structure to pass to the error handler
 * \return EXIT_SUCCESS if the parsing was successful, error code otherwise
 * */
extern int fsm_parse(fsmcontext_t* fc, void** out, void* finalinfo,
      void* userinfo);

/**
 * Resets the position of the FSM on the current stream. This allows to jump in another position of the stream being disassembled.
 * This function must not be called in the middle of a parsing operation
 * \param fc Pointer to  the structure containing the FSM context
 * \param newaddr New address in the stream at which the parsing must resume
 * \return EXIT_SUCCESS / error code
 * */
extern int fsm_resetstream(fsmcontext_t* fc, int64_t newaddr);

/**
 * Sets the function to execute when an error is encountered
 * \param fc The FSM context
 * \param errorhandler Function to execute when a parsing error is encountered
 * */
extern void fsm_seterrorhandler(fsmcontext_t* fc,
      void (*errorhandler)(fsmcontext_t*, void**, void*));

/**
 * Sets the stream the FSM will have to parse
 * \param fc The FSM context
 * \param stream The stream of bytes to be parsed
 * \param streamlen The length in bytes of \e stream
 * \param startaddr The address associated to the beginning of \e stream
 * */
extern void fsm_setstream(fsmcontext_t* fc, unsigned char* stream,
      int streamlen, int64_t startaddr);

/**
 * Initialises the FSM. Must be invoked before any parsing operation
 * \param fsmloader Pointer to the function for loading the FSM values
 * \return Structure storing the FSM environment
 * */
extern fsmcontext_t* fsm_init(void (*fsmloader)(fsmload_t*));

/**
 * Terminates the FSM. No parsing operation will be possible afterwards
 * \param fc Structure storing the FSM environment
 * */
extern void fsm_terminate(fsmcontext_t* fc);

/*
 * Reinitialises an existing FSM. This allows to change the FSM values while keeping the FSM state.
 * This must not be performed during an parsing operation
 * \param fc A pointer to the structure containing an already initialised FSM context. Must not be NULL
 * \param fsmloader Pointer to the function for loading the FSM values
 * */
extern void fsm_reinit(fsmcontext_t* fc, void (*fsmloader)(fsmload_t*));

/****************************************************************************/

/**
 * Retrieves the length of a parameter coding
 * \param pc Pointer to the parameter coding structure
 * \return The length of the parameter coding type, or -1 if the pointer is NULL
 * */
extern uint8_t paramcoding_getlength(paramcoding_t* pc);

/**
 * Retrieves the value of a parameter coding
 * \param pc Pointer to the parameter coding structure
 * \return The value of the parameter coding, or -1 if the pointer is NULL
 * */
extern int64_t paramcoding_getvalue(paramcoding_t* pc);

/**
 * Returns the value contained in a paramcoding taking its size
 * into account to return the correct sign as well
 * \param pc The paramcoding structure
 * \return The value contained in the paramcoding, appropriately signed, or -1 if the paramcoding is NULL
 * */
extern int64_t paramcoding_getsignedvalue(paramcoding_t* pc);

#endif /*FSMUTILS_H_*/
