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
 * \file fsmutils.c
 * \brief This file contains the functions needed to run a disassembler based on a 
 * finite state machine automate
 * */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "dsmb_archinterface.h"
#include "fsmutils.h"

#define CST 1 		/**<Type of a constant variable (a bitfield)*/
#define VARIABLE 2 	/**<Type of a non-constant variable*/

#define NOVARIDX 0 	/**<Hard-coded value of the index in the array for the variables at which there is nothing*/
#define AXIOMIDX 1 	/**<Hard-coded value of the index for the axiom in the array for the variables (set by minjag)*/
#define TMPLTIDX 2 	/**<Hard-coded value of the index for the template in the array for the variables (set by minjag)*/

/**
 * Structure holding a disassembled value
 * */
typedef struct value_s {
   int type; /**<Type of the value (constant or grammar variable)*/
   unsigned char* bin_start; /**<Byte at which the constant value begins*/
   unsigned char* bin_stop; /**<Byte at which the constant value ends*/
   uint8_t bin_start_off; /**<Offset in the \c bin_start byte at which the value begins*/
   uint8_t bin_stop_off; /**<Offset in the \c bin_stop byte at which the value ends*/
   int varname; /**<Identifier of the grammar symbol this value represents (or 0 if it does not represent a symbol)*/
} value_t;

/**
 * Structure holding an element inside a buffer
 * */
typedef struct buffer_s {
   fsmstate_t* state; /**<State associated to this buffer element*/
   value_t value; /**<Pointer to a structure holding the value for this element*/
#ifndef NDEBUG
   int stateid; /**<Identifier of the state (used only for debug printing)*/
#endif
} buffer_t;

/**
 * Structure representing a stack
 * This stack holds an array of elements that can increase in size but never decreases while the stack exists
 * The actual size of the stack (number of element presents in it) is stored as one of the members.
 * This allows to use stacks while limiting the number of allocations and deletions
 * */
typedef struct fsmstack_s {
   void* stack; /**<Array containing the elements in the stack*/
   int stacksz; /**<Maximum size of the stack*/
   int stacktop; /**<Number of elements currently in the stack*/
} fsmstack_t;

/**
 * \brief Structure containing all necessary information about a parsing in progress.
 * */
typedef struct fsmparser_s {
   fsmstack_t* buffer; /**<Buffer for the variables already identified in the instruction being disassembled*/
   paramcoding_t* decoded_syms; /**<Array of variables and tokens decoded during parsing*/
   fsmstack_t* semactions; /**<List of semantic actions to execute once the parsing is complete*/
   void** variables; /**<Array of variables manipulated during parsing*/
   unsigned char* input; /**<Pointer to the beginning of the current transition from the input stream*/
   unsigned char* next; /**<Pointer to the next character in the current transition from the input stream*/
   unsigned int lastreducvar; /**<Identifier of the last reduced symbol*/
   uint8_t input_off; /**<Offset in the \c input byte where the unread bits actually begins*/
   uint8_t next_off; /**<Offset in the \c next byte where the next character actually ends*/
} fsmparser_t;

/**
 * \brief Structure holding the information about an fsm execution
 * \todo Clean up what is not necessary here and reorder to optimise size.
 * \todo The definition could be moved to the source file. The architecture specific functions would need some interface to update the context
 * => Ok, I did that, but I'm still ambivalent about the use of fsmload_t and fsmparser_t. Check how this does in the long run, if it becomes to
 * bothersome to code, remove fsmload_t and simply copy its content into fsmcontext_t, and find something clever for altparser. Besides, altparser
 * may not work in the long run either.
 * */
struct fsmcontext_s {
   unsigned char* input0; /**<Pointer to the first character in the input stream*/
   void (*errorhandler)(fsmcontext_t*, void**, void*);/**<Function to invoke when a parsing error is encountered*/
   extfct_t final_action; /**<Function to execute at the end of the parsing of the current symbol*/
   int64_t first_address; /**<Address of the first instruction in a list*/
   int inputlen; /**<Length of the input buffer*/
   /**\bug Storing \c inputlen on an int means we will not be able to handle streams larger than 4 GB for disassembly*/
   unsigned char* coding_start; /**<Pointer to the first byte of the word being parsed*/
   fsmload_t fsmvars; /**<Information specific to the FSM being run*/
   fsmparser_t parser; /**<Information specific to the parsing in progress*/
   fsmstack_t* altparser; /**<Stack of resume points for the parser set by a shift/reduce state*/
   int fsmerror; /**<Error code encountered during parsing*/
   uint8_t coding_start_off; /**<Offset in \c coding_start where the word begins (should always be 0)*/
   char altrdc; /**<Specifies an alternate reduction due to a shift/reduce state is possible*/
   char parsecomplete; /**<Current state of the parsing*/
};

/**
 * Adds element E of type T to the stack S
 * \param S The stack
 * \param E The element to add
 * \param T The type of E
 * */
#define STACK_ADDELT(S,E,T) {\
    assert(S != NULL);\
    if ( S->stacktop >= S->stacksz ) {\
        S->stack = lc_realloc(S->stack, sizeof(T)*(S->stacksz+1));\
        DBGMSGLVL(3,"Stack %p increased to size %lu\n",S->stack,sizeof(T)*(S->stacksz+1));\
        S->stacksz++;\
    }\
    T* stack__ = (T*)S->stack;\
    stack__[S->stacktop] = E;\
    S->stacktop++;\
}

/**
 * Adds an empty element to the stack of type T to the stack S
 * \warning The new element must be set immediately after,
 * otherwise we could having an old element on top
 * \param S The stack
 * \param T The type
 * \param X Action to execute if the stack is extended
 * */
#define STACK_ADD(S,T,X) {\
    assert(S != NULL);\
    if ( S->stacktop >= S->stacksz ) {\
        S->stack = lc_realloc(S->stack, sizeof(T)*(S->stacksz+1));\
        DBGMSGLVL(3,"Stack %p increased to size %lu\n",S->stack,sizeof(T)*(S->stacksz+1));\
        S->stacksz++;\
        X;\
    }\
    S->stacktop++;\
}

/**
 * Removes top of stack S and returns it in E of type T
 * \param S The stack
 * \param E The element to remove (return parameter)
 * \param T The type of E
 * \todo We might be able to use typeof(E) to get rid of the T parameter
 * */
#define STACK_REMOVE(S,E,T) {\
    assert(S != NULL);\
    if( S->stacktop > 0 ) {\
        T* stack__ = (T*)S->stack;\
        E = stack__[S->stacktop - 1];\
        S->stacktop--;\
    }\
}

/**
 * Duplicates stack O into stack C. The contents of the stack are copied directly into C (beware of allocations)
 * \param O Origin stack
 * \param C Copy stack
 * \param T Type of element contained in the stacks
 * */
#define STACK_COPY(C, O, T) {\
	assert(O && C);\
	if (O->stacksz > C->stacksz) {\
	   C->stack = lc_realloc(C->stack, sizeof(T)*O->stacksz);\
	   C->stacksz = O->stacksz;\
	}\
	memcpy(C->stack, O->stack, sizeof(T)*O->stacktop);\
	C->stacktop = O->stacktop;\
}

/**
 * Empties a stack (sets its size to 0)
 * \param S The stack to empty
 * */
#define STACK_EMPTY(S) S->stacktop = 0

/**
 * Retrieves the element at a given index in the stack
 * \warning This macro is intended to be used in loops, no test is made on the range of the given index in regard to the size of the stack
 * \param S The stack
 * \param I The index
 * \param T The type of the element in the stack
 * */
#define STACK_GETELT(S,I,T) ((T*)(S->stack))[I]

/**
 * Returns the current size of the stack (number of elements it actually contains)
 * \param S The stack
 * */
#define STACK_GETSIZE(S) S->stacktop

/**
 * Forces the size of the stack (number of elements it actually contains) to a given number
 * \param S The stack
 * \param s The size to set the stack to
 * */
#define STACK_RESETSIZE(S, s) S->stacktop = s;

/**
 * Returns the element on top of the stack (but does not remove it)
 * \param S The stack
 * \param T The type of the element to retrieve
 * */
#define STACK_GETTOP(S,T) ((T*)(S->stack))[S->stacktop-1]

/**
 * Returns the last element in the stack (it may be beyond the current top)
 * \param S The stack
 * \param T The type of the element to retrieve
 * \note Mainly intended to be used with STACK_ADD to initialise the last element of a newly extended stack
 * */
#define STACK_GETLAST(S, T) ((T*)(S->stack))[S->stacksz-1]

/**
 * Cuts the top of the stack
 * \warning No test is made to check the stack is not empty
 * \param S The stack
 * */
#define STACK_CUT(S) S->stacktop--

/**
 * Increases the stack's size
 * \warning No test is made to check this does not exceed its maximal size
 * \warning This means the top of the stack may be an old or unitialised value
 * \param S The stack
 * */
#define STACK_EXPAND(S) S->stacktop++

/**
 * Flushes the contents of a stack by applying a given function to each allocated element.
 * Not needed if the elements in the stack were not initialised using an allocation
 * \param S The stack
 * \param F The function to apply
 * \param T The type of elements contained in the stack (needed to avoid compilation warnings about void* dereferencing)
 * */
#define STACK_FLUSH(S, F, T ) {\
   int __i_stack_flush;\
   for (__i_stack_flush = 0; __i_stack_flush < S->stacksz; __i_stack_flush++) {\
      F(&(STACK_GETELT(S, __i_stack_flush, T)));\
   }\
   S->stacksz = 0;\
   S->stacktop = 0;\
}

/**
 * Mask for retrieving a number of less significant bits in a byte
 * */
static unsigned char bytelsbmask[] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f,
      0x7f, 0xff };

/**
 * Mask for retrieving a number of most significant bits in a byte
 * */
static unsigned char bytemsbmask[] = { 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc,
      0xfe, 0xff };

/**
 * Expression calculating the length in bits between two bytes and their offsets
 * \param B1 First byte
 * \param B2 Second byte
 * \param O1 Offset in first byte where to start
 * \param O2 Offset in second byte where to start
 * */
#define STREAM_LEN(B1,B2,O1,O2) ( ( B2 - B1 ) * 8 + O2 - O1 )

/**
 * Expression updating the position in a stream by adding a given length
 * \param B Address of the byte pointing at the position in a stream
 * \param O Offset in the byte of the position
 * \param L Length in bits to add to the position
 * \param BL Byte pointing to the L bits after the position
 * \param OL Offset in the byte L bits after the position
 * */
#define STREAM_ADDLEN(B, O, BL, OL, L) BL = B + (L>>3);\
   if ( L%8 ) {\
      uint8_t skip = O + (L%8);\
      if ( skip >= 8) {\
         BL = BL + 1;\
         OL = skip%8;\
      } else {\
         OL = skip;\
      }\
   } else {\
       OL = O;\
   }

/**
 * Macro for easily retrieving the last byte of the value at the top of the buffer
 * \param FC The FSM context
 * */
#define STATEBUFFER_GETTOPBYTE(FC) ((STACK_GETSIZE(FC->parser.buffer)>1)?STACK_GETTOP(FC->parser.buffer,buffer_t).value.bin_stop:FC->coding_start)

/**
 * Macro for easily retrieving the offset for the last byte of the value at the top of the buffer
 * \param FC The FSM context
 * */
#define STATEBUFFER_GETTOPBYTEOFF(FC) ((STACK_GETSIZE(FC->parser.buffer)>1)?STACK_GETTOP(FC->parser.buffer,buffer_t).value.bin_stop_off:FC->coding_start_off)
/***********Functions for handling the token values**********/

/*
 * Retrieves the length of a parameter coding
 * \param pc Pointer to the parameter coding structure
 * \return The length of the parameter coding type, or -1 if the pointer is NULL
 * */
uint8_t paramcoding_getlength(paramcoding_t* pc)
{
   return (pc) ? pc->length : -1;
}

/**
 * Sets the length of a parameter coding
 * \param pc Pointer to the parameter coding structure
 * \param length The new length of the parameter coding type
 * */
static void paramcoding_setlength(paramcoding_t* pc, int length)
{
   assert(pc != NULL);
   pc->length = length;
}

/*
 * Retrieves the value of a parameter coding
 * \param pc Pointer to the parameter coding structure
 * \return The value of the parameter coding, or -1 if the pointer is NULL
 * */
int64_t paramcoding_getvalue(paramcoding_t* pc)
{
   return (pc) ? pc->value : -1;
}

/*
 * Returns the value contained in a paramcoding taking its size
 * into account to return the correct sign as well
 * \param pc The paramcoding structure
 * \return The value contained in the paramcoding, appropriately signed, or -1 if the paramcoding is NULL
 * */
int64_t paramcoding_getsignedvalue(paramcoding_t* pc)
{
   int64_t out = -1;
   uint32_t tmp32;
   if (pc) {
      switch (pc->length) {
      case 8:
         out = (int64_t) ((int8_t) pc->value);
         break;
      case 16:
         out = (int64_t) ((int16_t) pc->value);
         break;
      case 19:
         tmp32 = ((uint32_t) pc->value);
         if (tmp32 >> 18)
            out = (int64_t) ((uint64_t) tmp32 | (uint64_t) 0xFFFFFFFFFFF80000);
         else
            out = (int64_t) (tmp32);
         break;
      case 24:
         tmp32 = ((uint32_t) pc->value);
         if (tmp32 >> 23)
            out = (int64_t) ((uint64_t) tmp32 | (uint64_t) 0xFFFFFFFFFF000000);
         else
            out = (int64_t) (tmp32);
         break;
      case 26:
         tmp32 = ((uint32_t) pc->value);
         if (tmp32 >> 25)
            out = (int64_t) ((uint64_t) tmp32 | (uint64_t) 0xFFFFFFFFFC000000);
         else
            out = (int64_t) (tmp32);
         break;
      case 32:
         out = (int64_t) ((int32_t) pc->value);
         break;
      case 64:
      default:
         out = (int64_t) ((int64_t) pc->value);
         break;
      }
   }
   return out;
}

/**
 * Sets the value of a parameter coding
 * \param pc Pointer to the parameter coding structure
 * \param v The new value of the parameter coding
 * */
static void paramcoding_setvalue(paramcoding_t* pc, int64_t v)
{
   assert(pc != NULL);
   pc->value = v;
}

/************** Dumping functions (mainly used for tests and debugs) ****************/

#ifndef NDEBUG //These functions are used for debug only, so we avoid compilation warnings. To be changed if we need them in non-debug mode

static int value_getbinlen(value_t* val);
static int64_t getstreamval(unsigned char* start, uint8_t start_off,
      unsigned char* stop, uint8_t stop_off, int endianness);
/**
 * Returns the binary value contained in a value_t object
 * \param val The value_t whose binary value is needed
 * \param endianness Endianness of the value
 * \return The numerical value
 * \warning If we use this parser on grammars where tokens can be longer than 64 bits, this won't work and we will need to return a bitvector
 * */
static int64_t value_getbinval(value_t* val, int endianness)
{
   return getstreamval(val->bin_start, val->bin_start_off, val->bin_stop,
         val->bin_stop_off, endianness);
}

/**
 * Dumps the content of a value_t object to standard output (used for tests)
 * \param fc Structure describing the FSM context
 * \param val Pointer to the value_t object
 * \param stream The stream to write to
 * */
static void value_dump(fsmcontext_t* fc, value_t* val, FILE* stream)
{
   if (val == NULL)
      fprintf(stream, "(value NULL)");
   else {
      if (val->type == VARIABLE)
         fprintf(stream, "{var %s}", fc->fsmvars.varnames[val->varname]);
      else if (val->type == CST) {
         int64_t value = value_getbinval(val, BIG_ENDIAN_BIT);
         /**\note There will be some trouble if in debug level >=3 because value_getbinval will print its debug output*/
         fprintf(stream, "%#"PRIx64"", value);
      }
      fprintf(stream, " (len: %d)", value_getbinlen(val));
   }
}

/**
 * Dumps the content of a buffer_t object to standard output (used for tests)
 * \param fc Structure describing the FSM context
 * \param buf Pointer to the buffer_t object
 * \param stream The stream to write to
 * */
static void buffer_dump(fsmcontext_t* fc, buffer_t* buf, FILE* stream)
{
   if (buf == NULL)
      fprintf(stream, "\tNULL\n");
   else {
      fprintf(stream, "\t[ State: %d (%p) ; Value:", buf->stateid, buf->state);
      value_dump(fc, &buf->value, stream);
      fprintf(stream, "]\n");
   }
}

/**
 * Dumps the content of the stack buffer to a stream
 * \param fc Structure describing the FSM context
 * \param stream The stream to write to
 * */
static void stackbuffer_dump(fsmcontext_t* fc, FILE* stream)
{
   int i;
   for (i = STACK_GETSIZE(fc->parser.buffer) - 1; i >= 0; i--)
      buffer_dump(fc, &(STACK_GETELT(fc->parser.buffer, i, buffer_t)), stream);
}
#endif

/*********************** End of dumping functions **********************/

/**
 * Function creating a stack
 * \return A new stack structure
 * */
static fsmstack_t* fsmstack_new()
{
   fsmstack_t *stack = lc_malloc0(sizeof(fsmstack_t));
   return stack;
}

/**
 * Function freeing a stack
 * \param stack The stack to free
 * */
static void fsmstack_free(fsmstack_t* stack)
{
   if (stack == NULL)
      return;
   lc_free(stack->stack);
   lc_free(stack);
}

/**
 * Initialises a fsmparser_t structure. Must already be allocated
 * \param parser Pointer to the structure to initialise
 * \param fl Structure containing the characteristic of the FSM for which this parser is defined
 * */
static void fsmparser_init(fsmparser_t* parser, fsmload_t* fl)
{
   assert(parser && fl);
   if (fl->n_variables > 0) {
      parser->variables = lc_malloc0(sizeof(void*) * fl->n_variables);
      parser->decoded_syms = (paramcoding_t*) lc_malloc0(
            sizeof(paramcoding_t) * fl->n_variables);
   }
   parser->buffer = fsmstack_new();
   parser->semactions = fsmstack_new();
}

/**
 * Reinitialises a fsmparser_t structure. Must already be allocated
 * \param parser Pointer to the structure to initialise
 * \param fl Structure containing the characteristic of the FSM for which this parser is defined
 * */
static void fsmparser_reinit(fsmparser_t* parser, fsmload_t* fl)
{
   assert(parser && fl);
   //Frees the current arrays
   if (parser->variables) {
      lc_free(parser->variables);
      lc_free(parser->decoded_syms);
      parser->variables = NULL;
      parser->decoded_syms = NULL;
   }
   if (fl->n_variables > 0) {
      parser->variables = lc_malloc0(sizeof(void*) * fl->n_variables);
      parser->decoded_syms = (paramcoding_t*) lc_malloc0(
            sizeof(paramcoding_t) * fl->n_variables);
   }
}

/**
 * Frees the contents of a fsmparser_t structure. Does not free the structure
 * \param parser Pointer to the structure to terminate
 * */
static void fsmparser_terminate(fsmparser_t* parser)
{
   assert(parser);
   if (parser->variables) {
      lc_free(parser->variables);
      lc_free(parser->decoded_syms);
   }
   fsmstack_free(parser->semactions);
   fsmstack_free(parser->buffer);
}

/**
 * Returns the length of the binary value contained in a value_t structure
 * \param val The value_t structure
 * \return Length in bits of the binary value it contains
 * */
static int value_getbinlen(value_t* val)
{
   return STREAM_LEN(val->bin_start, val->bin_stop, val->bin_start_off,
         val->bin_stop_off);
}

/**
 * Sets the next state in the fsm
 * \param fc Pointer to the structure containing the context of the FSM
 * \param st_id The identifier of the next state (its index in the states array from fc)
 * \note Not necessary, but makes the code more readable. I think.
 * */
static void fsm_setnextstate(fsmcontext_t* fc, int st_id)
{

   (STACK_GETTOP(fc->parser.buffer, buffer_t)).state =
         fc->fsmvars.states[st_id];
#ifndef NDEBUG
   (STACK_GETTOP(fc->parser.buffer, buffer_t)).stateid = st_id;
#endif
   DBGMSG("Next state is %d (%p)\n", st_id,
         (STACK_GETTOP(fc->parser.buffer,buffer_t)).state);
}

/**
 * Adds a value to the buffer for identified variables
 * \param fc Pointer to the structure containing the FSM context
 * \param stid The identifier of the corresponding state to add to the buffer
 * \param valuetype The type of the value (CST or VARIABLE)
 * */
static void statebuffer_add(fsmcontext_t* fc, int stid, int valuetype)
{
   buffer_t *new;

   STACK_ADD(fc->parser.buffer, buffer_t,);
   new = &(STACK_GETTOP(fc->parser.buffer, buffer_t));

   new->value.type = valuetype;
   /**\todo Possible improvement: create a type for an start/top + offsets, use it in value and fsmcontext.
    * This would align the values and may help the compiler to copy them faster
    * Alternatively, simply move input, input_off,... in fsmcontext so that they are aligned like in value
    * => Done that, no special improvement in perfs but keeping it since it can't hurt
    * => Changing it, the affectations now vary depending on whether we add a bit field or a variable..
    * but it may change again later. If this comment is still here while the ARM update works, it did not change.*/
   new->value.bin_stop = fc->parser.next;
   new->value.bin_stop_off = fc->parser.next_off;

   new->value.varname = fc->parser.lastreducvar;

   fsm_setnextstate(fc, stid);

}

/**
 * Empties the state+values buffer while keeping the first entry in the stack
 * \param fc Pointer to the structure containing the FSM context
 * */
static void statebuffer_empty(fsmcontext_t* fc)
{
   assert(fc->parser.buffer != NULL);
   STACK_RESETSIZE(fc->parser.buffer, 1);
}

/**
 * Adds a bitfield to the buffer for identified variables
 * \param fc Pointer to the structure containing the FSM context
 * \param stid The identifier of the corresponding state to add to the buffer
 * */
static void statebuffer_addbit(fsmcontext_t* fc, int stid)
{
   statebuffer_add(fc, stid, CST);
   (STACK_GETTOP(fc->parser.buffer, buffer_t)).value.bin_start =
         fc->parser.input;
   (STACK_GETTOP(fc->parser.buffer, buffer_t)).value.bin_start_off =
         fc->parser.input_off;
   DBGLVL(2, DBGMSG0("New buffer is:\n"); stackbuffer_dump(fc, stderr););
}

/*
 * Termination function that must be called after a binary stream 
 * has been successfully disassembled
 * \param fc Pointer to the structure containing the context of the FSM
 * */
void fsm_parseend(fsmcontext_t* fc)
{

   DBGMSG0("Entering fsm_parseend\n");

   statebuffer_empty(fc);
}

/**
 * Copies the contents of a fsmparser_t structure to another.
 * \param copy Parser to copy to. Must be already initialised
 * \param origin Parser to copy from
 * \param fl Structure containing the characterisation of the current FSM
 * */
static void fsmparser_copy(fsmparser_t *copy, fsmparser_t *origin,
      fsmload_t *fl)
{
   assert(copy && origin && fl); //
   //Copies the state buffer stack
   STACK_COPY(copy->buffer, origin->buffer, buffer_t);
   //Copies semantic action stack
   STACK_COPY(copy->semactions, origin->semactions, semfct_t);
   //Copies other variables from the parser
   copy->input = origin->input;
   copy->input_off = origin->input_off;
   copy->next = origin->next;
   copy->next_off = origin->next_off;
   copy->lastreducvar = origin->lastreducvar; //Should not be needed, but better be thorough
   //Copies the arrays of symbols
   memcpy(copy->decoded_syms, origin->decoded_syms,
         sizeof(paramcoding_t) * fl->n_variables);
   memcpy(copy->variables, origin->variables, sizeof(void*) * fl->n_variables);
}

/**
 * Saves the state of the parser.
 * \param fc Current FSM context
 * */
static void fsmcontext_saveparser(fsmcontext_t* fc)
{
   assert(fc);
   int newparser = FALSE; //This is to initialise the parser if it was not already in the stack
   DBGMSG("Saving parser state after encountering state %d (%p)\n",
         (STACK_GETTOP(fc->parser.buffer,buffer_t)).stateid,
         (STACK_GETTOP(fc->parser.buffer,buffer_t)).state);
   DBGLVL(2, DBGMSG0("Saved buffer is:\n"); stackbuffer_dump(fc, stderr););
   //Adds a new element to the stack of saved parsers
   STACK_ADD(fc->altparser, fsmparser_t, newparser = TRUE);
   if (newparser)
      fsmparser_init(&(STACK_GETTOP(fc->altparser, fsmparser_t)), &fc->fsmvars);
   /**\todo Yep, it's ugly. Change your macros to have something more clever that avoids the newparser. Simply passing the init function
    * to the STACK_ADD would be nice to do*/
   fsmparser_t* newalt = &(STACK_GETTOP(fc->altparser, fsmparser_t));

   //Copies the current parser state to the new element
   fsmparser_copy(newalt, &fc->parser, &fc->fsmvars);
}

/**
 * Restores the state of the parser.
 * \param fc Current FSM context
 * \return State at the top of the buffer stack if a saved parser was indeed restored, NULL if no parser was previously saved
 * */
static fsmstate_t* fsmcontext_restoreparser(fsmcontext_t* fc)
{
   assert(fc);
   if ( STACK_GETSIZE(fc->altparser) > 0) {
      fsmparser_t altparser;
      STACK_REMOVE(fc->altparser, altparser, fsmparser_t);
      fsmparser_copy(&fc->parser, &altparser, &fc->fsmvars);
      DBGMSG("Restoring parser state. New top state is %d (%p)\n",
            (STACK_GETTOP(fc->parser.buffer,buffer_t)).stateid,
            (STACK_GETTOP(fc->parser.buffer,buffer_t)).state);
      DBGLVL(2, DBGMSG0("Restored buffer is:\n"); stackbuffer_dump(fc, stderr););
      return (STACK_GETTOP(fc->parser.buffer, buffer_t)).state;
   } else
      return NULL; //No element in the stack of saved parsers
}

/**
 * Handles an error in the FSM: the stack of next states is emptied and sets the error flag to 1
 * \param fc Pointer to the structure containing the context of the FSM
 * \param errcode Error code to set
 * */
static void fsm_error(fsmcontext_t* fc, int errcode)
{
   /*Empties the stack of states*/
   statebuffer_empty(fc);
   /*Sets the error flag*/
   fc->fsmerror = errcode;
   /*Sets the parsing as completed*/
   fc->parsecomplete = TRUE;
}

/**
 * Shifts the specified length of bits from entrybuffer into statebuffer
 * \param fc Pointer to the structure containing the context of the FSM
 * \param stid The identifier of the corresponding state to add to the buffer
 * */
static void shift_bits(fsmcontext_t* fc, int stid)
{

   statebuffer_addbit(fc, stid);

   //Shifting the progression pointer at the end of the transition
   fc->parser.input = fc->parser.next;
   fc->parser.input_off = fc->parser.next_off;
}

/**
 * Shift the marker to the end of the current transition by a given length. The value is not returned
 * \param fc Pointer to the structure containing the context of the FSM
 * \param len Length in bits of the value (max. 8)
 * \return EXIT_SUCCESS if the value could be returned, ERR_DISASS_FSM_END_OF_STREAM_REACHED if the end of the stream was reached before \c len was reached
 * */
static int fsmcontext_nexttrans(fsmcontext_t* fc, uint16_t len)
{

   if ((fc->parser.next + ((fc->parser.next_off + len) >> 3) - fc->input0)
         > fc->inputlen) {
      DBGMSG0("End of stream reached\n");
      return ERR_DISASS_FSM_END_OF_STREAM_REACHED; //Attempted to read bits beyond the end of the stream
   }
   STREAM_ADDLEN(fc->parser.next, fc->parser.next_off, fc->parser.next,
         fc->parser.next_off, len);

   return EXIT_SUCCESS;
}

/**
 * Returns a part of a sub value to compare with the next transition in the input stream
 * \param fc Pointer to the structure containing the context of the FSM
 * \param start Offset from the beginning of the current transition at which the part begins
 * \param len Length in bits of the value (max. 24)
 * \param next Return parameter, will contain the value
 * \return EXIT_SUCCESS if the value could be returned, ERR_DISASS_FSM_END_OF_STREAM_REACHED if the end of the stream was reached before \c len was reached
 * \note While this function shifts the end of the current transition as does fsmcontext_nexttrans, that function is not used here
 * for optimisation purposes to avoid doing the same tests twice
 * */
static int fsmcontext_nexttranspart(fsmcontext_t* fc, uint16_t start,
      uint16_t len, uint32_t* next)
{
   assert(next != NULL); //This is the return value, it is not NULL and that's it
   unsigned char* startbyte;
   uint8_t startbyte_off;
   //Identifying the beginning of the transition value
   STREAM_ADDLEN(fc->parser.input, fc->parser.input_off, startbyte,
         startbyte_off, start);
   if ((startbyte + ((startbyte_off + len) >> 3) - fc->input0) > fc->inputlen) {
      DBGMSG0("End of stream reached\n");
      return ERR_DISASS_FSM_END_OF_STREAM_REACHED; //Attempted to read bits beyond the end of the stream
   }
   assert(len <= 24);
   /**\todo Maybe reorder this differently to have less cases*/
   if (len == 24) {
      //The requested value is exactly 3 bytes
      if (startbyte_off == 0) {
         //No offset, we are at the beginning of a byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         // ^*******  ********  ********  => Returned value: [ABCDEFGH IJKLMNOP QRSTUVWX]
         *next = (*startbyte << 16) | (*(startbyte + 1) << 8)
               | *(startbyte + 2);
      } else {
         //We are in the middle of a byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         //   ^*****  ********  ********  ** => Returned word: [CDEFGHIJ KLMNOPQR STUVWXYZ]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off])
               << (startbyte_off + 16)) /*End of the current byte*/
         | ((*(startbyte + 1)) << (startbyte_off + 8)) /*Byte n+1 (taken in full)*/
         | ((*(startbyte + 2)) << startbyte_off) /*Byte n+2 (taken in full)*/
               | (((*(startbyte + 2)) & bytemsbmask[startbyte_off])
                     >> (8 - startbyte_off))); /*Beginning of the byte n+3*/
      }
   } else if (len > 16) {
      //The requested value is between 2 and 3 bytes long
      if (startbyte_off == 0) {
         //No offset, we are at the beginning of a byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         // ^*******  ********  *** => Returned value: [ABCDEFGH IJKLMNOP QRS]
         *next = (*startbyte << (len - 8)) | ((*(startbyte + 1)) << (len - 16))
               | (((*(startbyte + 2)) & bytemsbmask[len - 16]) >> (24 - len));
      } else if ((startbyte_off + len) > 24) {
         //We begin in the middle of a byte and end in the middle of the n+3 byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         //    ^****  ********  ********  * => Returned word: [DEFGHIJK LMNOPQRS TUVWXY]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off])
               << (startbyte_off + len - 8)) /*End of the current byte*/
         | ((*(startbyte + 1)) << (startbyte_off + len - 16)) /*Byte n+1 (taken in full)*/
         | ((*(startbyte + 2)) << (startbyte_off + len - 24)) /*Byte n+1 (taken in full)*/
               | (((*(startbyte + 3)) & bytemsbmask[startbyte_off + len - 24])
                     >> (32 - startbyte_off - len))); /*Beginning of the byte n+3*/
      } else if ((startbyte_off + len) == 24) {
         //We begin in the middle of a byte and end at the end of the n+2 byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         //     ^***  ********  ********  => Returned word: [EFGHIJKL MNOPQRST UVWX]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off]) << 16) /*End of the current byte*/
         | ((*(startbyte + 1)) << 8) /*Byte n+1 (taken in full)*/
         | (*(startbyte + 2))); /*Byte n+2 (taken in full)*/
      } else {
         //We begin in the middle of a byte and end before the end of the n+2 byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         //     ^***  ********  *******  => Returned word: [EFGHIJKL MNOPQRST UVW]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off])
               << (startbyte_off + len - 8)) /*End of the current byte*/
         | ((*(startbyte + 1)) << (startbyte_off + len - 16)) /*Byte n+1 (taken in full)*/
               | (((*(startbyte + 2)) & bytemsbmask[startbyte_off + len - 8])
                     >> (24 - startbyte_off - len))); /*Beginning of the byte n+2*/
      }
   } else if (len == 16) {
      //The requested value is a full word
      if (startbyte_off == 0) {
         //No offset, we are at the beginning of a byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         // ^*******  ******** => Returned word: [ABCDEFGH IJKLMNOP]
         *next = (*startbyte << 8) | *(startbyte + 1);
      } else {
         //We are in the middle of a byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         //   ^*****  ********  ** => Returned word: [CDEFGHIJ KLMNOPQR]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off])
               << (startbyte_off + 8)) /*End of the current byte*/
         | ((*(startbyte + 1)) << startbyte_off) /*Next byte (taken in full)*/
               | (((*(startbyte + 2)) & bytemsbmask[startbyte_off])
                     >> (8 - startbyte_off))); /*Beginning of the second byte*/
      }
   } else if (len > 8) {
      //The requested value is less than a word but more than a byte
      if (startbyte_off == 0) {
         //No offset, we are at the beginning of a byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         // ^*******  ***** => Returned word: [ABCDEFGH IJKLM]
         *next = (*startbyte << (len - 8))
               | (((*(startbyte + 1)) & bytemsbmask[len - 8]) >> (16 - len));
      } else if ((startbyte_off + len) > 16) {
         //We begin in the middle of a byte and end in the middle of the n+2 byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         //    ^****  ********  * => Returned word: [DEFGHIJK LMNOPQ]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off])
               << (startbyte_off + len - 8)) /*End of the current byte*/
         | ((*(startbyte + 1)) << (startbyte_off + len - 16)) /*Next byte (taken in full)*/
               | (((*(startbyte + 2)) & bytemsbmask[startbyte_off + len - 16])
                     >> (24 - startbyte_off - len))); /*Beginning of the second byte*/
      } else if ((startbyte_off + len) == 16) {
         //We begin in the middle of a byte and end at the end of the n+1 byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         //     ^***  ******** => Returned word: [EFGHIJKL MNOP]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off]) << 8) /*End of the current byte*/
         | (*(startbyte + 1))); /*Next byte (taken in full)*/
      } else {
         //We begin in the middle of a byte and end before the end of the n+1 byte
         //[ABCDEFGH][IJKLMNOP][QRSTUVWX][YZABCDEF]
         //     ^***  *******  => Returned word: [EFGHIJKL MNO]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off])
               << (startbyte_off + len - 8)) /*End of the current byte*/
               | (((*(startbyte + 1)) & bytemsbmask[startbyte_off + len - 8])
                     >> (16 - startbyte_off - len))); /*Beginning of the next byte*/
      }
   } else if (len == 8) {
      //Nice case: the requested value is a byte
      if (startbyte_off == 0) {
         //No offset, we are at the beginning of a byte
         //[ABCDEFGH][IJKLMNOP]
         // ^******* => Returned byte: [ABCDEFGH]
         *next = *startbyte;
      } else {
         //We are in the middle of a byte
         //[ABCDEFGH][IJKLMNOP]
         //   ^*****  ** => Returned byte: [CDEFGHIJ]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off])
               << startbyte_off) /*End of the current byte*/
               | (((*(startbyte + 1)) & bytemsbmask[startbyte_off])
                     >> (8 - startbyte_off))); /*Beginning of the next byte*/
      }
   } else {
      //The requested value is less than a byte
      if ((startbyte_off + len) <= 8) {
         //The bits we need are still in the same word
         //[ABCDEFGH][IJKLMNOP]
         //   ^***   => Returned value: [CDEF]
         *next = ((*startbyte) >> (8 - len - startbyte_off)) & bytelsbmask[len];
      } else {
         //General case: we need some bits in the current byte and some in the next
         //[ABCDEFGH][IJKLMNOP]
         //     ^***  **   => Returned value: [EFGHIJ]
         *next = ((((*startbyte) & bytelsbmask[8 - startbyte_off])
               << (startbyte_off + len) % 8) /*End of the current byte*/
               | (((*(startbyte + 1)) & bytemsbmask[(startbyte_off + len) % 8])
                     >> (8 - (startbyte_off + len) % 8))); /*Beginning of the next byte*/
      }
   }
   return EXIT_SUCCESS;
}

/**
 * Returns a sub value for comparison with the next transition in the input stream
 * \param fc Pointer to the structure containing the context of the FSM
 * \param starts Array of offsets at which the parts begin
 * \param lens Length in bits of the parts
 * \param nparts Number of parts
 * \param next Return parameter, will contain the value
 * \return EXIT_SUCCESS if the value could be returned, error code if the end of the stream was reached before \c len was reached
 * */
static int fsmcontext_nexttransval(fsmcontext_t* fc, uint16_t *starts,
      uint16_t *lens, uint8_t nparts, uint32_t* next)
{
   assert(next != NULL); //This is the return value, it is not NULL and that's it
   unsigned int i;
   uint32_t out = 0;
   *next = 0;
   int res = fsmcontext_nexttranspart(fc, starts[0], lens[0], &out);
   if (ISERROR(res))
      return res;
   for (i = 1; i < nparts; i++) {
      uint32_t out2;
      res = fsmcontext_nexttranspart(fc, starts[i], lens[i], &out2);
      if (ISERROR(res))
         return res;
      out = (out << lens[i]) | out2;
   }
   *next = out;
   DBG(
         DBGMSG("Next transition value is %x (from offsets/lengths: ", *next); for (i=0;i<nparts;i++) STDMSG("[%u,%u]", starts[i], lens[i]); STDMSG(")\n");)
   return res;
}

/*Macros used to factorise some code in state_findnext for better clarity*/
#define MATCH_FAILED next_state = STATE_NONE;continue /**<Code snippet for a failure in state_findnext*/
#define FIND_MATCH_IN_LIST  for (val_id = 0; val_id < lst->n_vals; val_id++)\
    if ( (testbyte & lst->vals[val_id]->mask) == lst->vals[val_id]->value )\
        break;\
    if ( val_id == lst->n_vals ) {\
        MATCH_FAILED;\
    }   /**<Code snippet for looking for a value in a list*/

/**
 * Finds the next state after a shift state.
 * \param fc Pointer to the structure containing the context of the FSM
 * \param st The state for which we are looking for a successor
 * \return TRUE if a match could be done, FALSE otherwise
 * */
static int state_findnext(fsmcontext_t* fc, stateshift_t* st)
{
   uint32_t testbyte;
   int next_state = STATE_LOOKING; //Still looking for the next state
   fsmtrans_subtbl_t* tbl = st->begintbl;  //Current table where we are looking
   if (tbl != NULL) {
      while (next_state == STATE_LOOKING) {
         fsmtrans_sublst_t* lst; //List of values in the table
         int val_id = 0;    //Identifier of the matching value
         /**\todo The code for testing an entry in a list is duplicated between the cases where the table is an array
          * and where it contains a single value. This is an attempt at optimisation to avoid any duplicated tests.
          * Find some way to avoid this (maybe simply using a macro)
          * => Done with FIND_MATCH_IN_LIST, but it's a bit of a cheat
          * */
         //Retrieving the entry in the table for this byte
         switch (tbl->type) {
         case SUBTBL_ALWAYSOK:                              //Value is always ok
            lst = tbl->lsts[0];
            break;
         case SUBTBL_SINGLEVALUE:                  //Only one value in the table
            //Retrieving the byte to test
            if (fsmcontext_nexttransval(fc, tbl->offsets, tbl->sizes,
                  tbl->noffs, &testbyte)) {
               MATCH_FAILED
;               ; //End of stream reached
            }
            lst = tbl->lsts[0];
            switch (lst->type) {
            case SUBLST_1VAL:       //List contains only 1 value, needs checking
               if ((testbyte & lst->vals[0]->mask) != lst->vals[0]->value) {
                  MATCH_FAILED
;               }
               DBGMSG0LVL(1,"Single value matches (1 possibility)\n");
               break;
               case SUBLST_NVALS: //Multiple values, we need to check which is the right one
               FIND_MATCH_IN_LIST;
               DBGMSGLVL(1,"Single value matches (%d possibilities)\n", lst->n_vals);
               break;
               default:
               HLTMSG("[INTERNAL] Unknown value for FSM sub transition type\n");//This should never happen
            }
            break;
            case SUBTBL_HASHTABLE:                           //Table is an array
            //Retrieving the byte to test
            if (fsmcontext_nexttransval(fc, tbl->offsets, tbl->sizes, tbl->noffs, &testbyte)) {
               MATCH_FAILED; //End of stream reached
            }
            lst = tbl->lsts[testbyte];
            /**\todo Check if the performances would be improved by setting tbl->lsts at NULL and testing it instead of using
             * a list with type SUBLST_NOMATCH. My guess is it would not as it would add a test*/
            switch(lst->type) {
               case SUBLST_NOMATCH: //Match fails (no value in array for this input)
               MATCH_FAILED;
               break;//Not useful actually
               case SUBLST_ALWAYSOK:
               case SUBLST_1VAL://List contains only 1 value, no need to check!
               DBGMSG0LVL(1,"Value matches in table (1 possibility)\n");
               break;
               case SUBLST_NVALS://Multiple values, we need to check which is the right one
               FIND_MATCH_IN_LIST;
               DBGMSGLVL(1,"Value matches in table (%d possibilities)\n", lst->n_vals);
               break;
               default:
               HLTMSG("[INTERNAL] Unknown value for FSM sub transition type\n");//This should never happen
            }
            break;
            default:
            HLTMSG("[INTERNAL] Unknown value for FSM sub table type\n"); //This should never happen
         }

         //At this point val_id contains the identifier of the element in the list matching with the entry
         if (lst->vals[val_id]->nextsubval == NULL) {
            //Transition has no further subvalue: it is complete and we can set the next state
            next_state = lst->vals[val_id]->next_state_id;
            //Shifts the next byte to the end of the completed transition
            if (fsmcontext_nexttrans(fc, lst->vals[val_id]->translen)) {
               MATCH_FAILED
;               //End of stream reached
               /**\todo Try to avoid this double check on the exit of the stream (it may have been already done in fsmcontext_nexttransval)
                * Maybe add in the sub tables the max length of the transitions being tested, but beware of transitions of multiple lengths*/
            }
            DBGMSGLVL(1, "Next state found: %d\n", next_state);
         } else {
            //Transition is not complete: we will have to match further bytes
            tbl = lst->vals[val_id]->nextsubval;
         }
      }
   }
   //At this point, either we identified the next state or we failed
   if ((next_state == STATE_NONE) || (next_state == STATE_LOOKING)) {
      //No state was found
      if (st->elsestate != STATE_NONE) {
         //State has a transition of length 0
         statebuffer_add(fc, st->elsestate, 0);
         DBGMSG0("Match made with empty value\n");
         //No need to rewind, as the next byte has not been moved
      } else {
         DBGMSG0("No match found on binary values\n");
         return FALSE;
      }
   } else {
      //A state was found: we will shift the value into the buffer
      shift_bits(fc, next_state);
   }
   return TRUE;
}

/**
 * Adds a variable to the buffer for identified variables
 * \param fc Pointer to the structure containing the context of the FSM
 * \param stid The identifier of the corresponding state to add to the buffer
 * */
static void statebuffer_addvar(fsmcontext_t* fc, int stid)
{
   //Saves the position of the stream at the top of the stack of states/buffer of variables
   unsigned char* oldtop =
         (STACK_GETTOP(fc->parser.buffer, buffer_t)).value.bin_stop;
   uint8_t oldtop_off =
         (STACK_GETTOP(fc->parser.buffer, buffer_t)).value.bin_stop_off;
   //Adds the variable to the stack
   statebuffer_add(fc, stid, VARIABLE);
   //Updates the variable to begin where the former top of the stack ended (its end is already set to next/next_off, which is equal to input/input_off)
   (STACK_GETTOP(fc->parser.buffer, buffer_t)).value.bin_start = oldtop;
   (STACK_GETTOP(fc->parser.buffer, buffer_t)).value.bin_start_off = oldtop_off;
   DBGLVL(1,
         DBGMSG0("Adding value: "); value_dump(fc, &((STACK_GETTOP(fc->parser.buffer,buffer_t))).value,stderr); STDMSG("\n");)
   DBGLVL(2, DBGMSG0("New buffer is:\n"); stackbuffer_dump(fc, stderr););
}

/**
 * Removes a variable from the buffer for identified variables and stores it in the appropriate in decoded_syms
 * \param fc Pointer to the structure containing the context of the FSM
 * \param name The code name of the variable to remove
 * \return Size of the reduced variable
 * */
static int statebuffer_removevar(fsmcontext_t* fc, int name)
{

   buffer_t buf = { NULL, { 0, NULL, NULL, 0, 0, 0 }
#ifndef NDEBUG
         , 0
#endif
         };

   DBGMSGLVL(1, "Removing variable %s\n", fc->fsmvars.varnames[name]);

   /**\todo [MINJAG] Implement something more streamlined once everything is stabilised
    * => I'm not quite sure what I meant by "more streamlined", but this seems streamlined enough now*/

   buf = STACK_GETTOP(fc->parser.buffer, buffer_t);
   /**\todo There is no test to check if the element could actually be retrieved from the stack and what to do otherwise*/
   if ((buf.value.type == VARIABLE) && (buf.value.varname == name)) {
      STACK_CUT(fc->parser.buffer);
      buf.value.type = 0;
   } //Otherwise the variable is supposed to be empty and remains in the buffer*/

   DBGLVL(2, DBGMSG0("New buffer is:\n"); stackbuffer_dump(fc, stderr););
   return value_getbinlen(&(buf.value));
}

/**
 * Returns a value between two bytes in a stream.
 * If the bytes cover more than 64 bits, the value corresponding to the rightmost 64 bits will be returned
 * \note This copies the behaviour of bitvector_fullvalue. It may be changed to handle differently values coded on more than 64 bits
 * \param start First byte of the value
 * \param start_off Offset in the first byte where to start
 * \param stop Last byte (not included, unless offset is positive) of the value
 * \param stop_off Offset in the last byte where to stop (number of bits to add)
 * \param endianness Endianness of the value
 * \return The numerical value or 0 if the length between bytes is 0
 * \todo Improve this function. Also, try to factorise the code while keeping it as efficient as possible. For instance, the cases LITTLE_ENDIAN_BYTE
 * and BIG_ENDIAN_BYTE are almost identical, save the order into which we are progressing in the bytes
 * \warning If we use this parser on grammars where tokens can be longer than 64 bits, this won't work and we will need to return a bitvector
 * */
static int64_t getstreamval(unsigned char* start, uint8_t start_off,
      unsigned char* stop, uint8_t stop_off, int endianness)
{
   int64_t out = 0;
   int i = 0, off = 0;
   unsigned char* step;
   //Length of the value (max 64 bits)
   int len =
         (STREAM_LEN(start, stop, start_off, stop_off) > 64) ?
               64 : STREAM_LEN(start, stop, start_off, stop_off);
   if (len == 0)
      return out; //Returning value 0 if the number of bits to retrieve is 0
   DBGMSGLVL(3,
         "Calculating value of %d bits (endianness %d) between [%x + %d] and [%x + %d]\n",
         len, endianness, *start, start_off, *stop, stop_off);
   switch (endianness) {
   case LITTLE_ENDIAN_BIT:
      if (stop_off == 0) {
         step = stop - 1;
         off = 8;
      } else {
         step = stop;
         off = stop_off;
      }
      //Begins step at the last byte containing bits in the value
      for (i = 0; i < len; i++) {
         //Bit i becomes bit len-i in value
         out += (int64_t) ((*step >> (8 - off)) & 1) << (len - 1 - i);
         DBGMSGLVL(3, "In byte %x and offset %d: Value is %#"PRIx64"\n", *step,
               off, out);
         off--;
         if (off == 0) {
            step = step - 1;
            off = 8;
         }
      }
      break;
   case LITTLE_ENDIAN_BYTE:
      len = ((len + 7) >> 3) << 3;
      DBGMSGLVL(3, "Updating length to %d\n", len)
      ;
      i = len - 8;
      if (stop_off == 0) { //Easy case: the value begins at a boundary
         step = stop - 1;
         while (step > start) {
            out += (int64_t) *step << i;
            DBGMSGLVL(3, "In byte %x: Adding %"PRIx64" = %x << %d\n", *step,
                  (int64_t )*step << i, *step, i);
            i -= 8;
            step--;
         }
         out += (int64_t) (*start & bytelsbmask[8 - start_off]) << i;
         DBGMSGLVL(3, "Adding final value: %"PRIx64" = ( %x & %x ) << %d \n",
               (int64_t )(*start & bytelsbmask[8 - start_off]) << i, *start,
               bytelsbmask[8 - start_off], i);
      } else { //Hard case: the value begins in the middle of a byte
         unsigned char* first = (start_off > stop_off) ? start + 1 : start;
         step = stop;
         off = stop_off;
         while (step > first) {
            out += (int64_t) (((*step & bytemsbmask[off]) >> (8 - off))
                  + ((*(step - 1) & bytelsbmask[8 - off]) << off)) << i;
            DBGMSGLVL(3,
                  "In byte %x: Adding %"PRIx64" = (((%x & %x)>>%d) + ((%x & %x)<<%d) ) << %d = (%x + %x) << %d\n",
                  *step,
                  (int64_t )(((*step & bytemsbmask[off]) >> (8 - off))
                        + ((*(step - 1) & bytelsbmask[8 - off]) << off)) << i,
                  *step, bytemsbmask[off], (8 - off), *(step - 1),
                  bytelsbmask[8 - off], off, i, (*step & bytemsbmask[off]),
                  ((*(step - 1) & bytelsbmask[8 - off]) << off), i);
            i -= 8;
            step--;
         }
         if (start_off > stop_off) { //We take less than 1 byte in the last byte
            //First byte of the value (step = start+1)
            out += (int64_t) (((*step & bytemsbmask[off]) >> (8 - off))
                  + ((*(step - 1) & bytelsbmask[8 - start_off]) << off)) << i;
            DBGMSGLVL(3,
                  "Adding final value: %"PRIx64" = (((%x & %x)>>%d) + ((%x & %x) << %d) << %d) = (%x + %x) << %d\n",
                  (int64_t )(((*step & bytemsbmask[off]) >> (8 - off))
                        + ((*(step - 1) & bytelsbmask[8 - start_off]) << off))
                        << i, *step, bytemsbmask[off], (8 - off), *(step - 1),
                  bytelsbmask[8 - start_off], off, i,
                  (*step & bytemsbmask[off]),
                  ((*(step - 1) & bytelsbmask[8 - start_off]) << off), i);
         } else if (start_off < stop_off) { //We take more than 8 bits in the last byte, so we have to create an additional byte
            //Here step = start
            out += (int64_t) (((*step & bytemsbmask[off]
                  & bytelsbmask[8 - start_off]) >> (8 - off))) << i;
            DBGMSGLVL(3,
                  "Adding final value: %"PRIx64" = (((%x & %x & %x) >>%d) ) << %d) = %x << %d\n",
                  (int64_t )(((*step & bytemsbmask[off]
                        & bytelsbmask[8 - start_off]) >> (8 - off))) << i,
                  *step, bytemsbmask[off], bytelsbmask[8 - start_off],
                  (8 - off), i,
                  (((*step & bytemsbmask[off] & bytelsbmask[8 - start_off])
                        >> (8 - off))), i);
         }
      }
      break;
   case BIG_ENDIAN_BIT:
   case BIG_ENDIAN_BYTE:
   default:
      if (stop_off == 0) {           //Easy case: the value begins at a boundary
         step = stop - 1;
         while (step > start) {
            out += (int64_t) *step << i;
            DBGMSGLVL(3, "In byte %x: Adding %"PRIx64" = %x << %d\n", *step,
                  (int64_t )*step << i, *step, i);
            i += 8;
            step--;
         }
         out += (int64_t) (*start & bytelsbmask[8 - start_off]) << i;
         DBGMSGLVL(3, "Adding final value: %"PRIx64" = (%x & %x) << %d\n",
               (int64_t )(*start & bytelsbmask[8 - start_off]) << i, *start,
               bytelsbmask[8 - start_off], i);
      } else {             //Hard case: the value begins in the middle of a byte
         unsigned char* first = (start_off > stop_off) ? start + 1 : start;
         step = stop;
         off = stop_off;
         while (step > first) {
            out += (int64_t) (((*step & bytemsbmask[off]) >> (8 - off))
                  + ((*(step - 1) & bytelsbmask[8 - off]) << off)) << i;
            DBGMSGLVL(3,
                  "In byte %x: Adding %"PRIx64" = (((%x & %x)>>%d) + ((%x & %x)<<%d) ) << %d = (%x + %x) << %d\n",
                  *step,
                  (int64_t )(((*step & bytemsbmask[off]) >> (8 - off))
                        + ((*(step - 1) & bytelsbmask[8 - off]) << off)) << i,
                  *step, bytemsbmask[off], (8 - off), *(step - 1),
                  bytelsbmask[8 - off], off, i,
                  ((*step & bytemsbmask[off]) >> (8 - off)),
                  ((*(step - 1) & bytelsbmask[8 - off]) << off), i);
            i += 8;
            step--;
         }
         if (start_off > stop_off) { //We take less than 1 byte in the last byte
            //First byte of the value (step = start+1)
            out += (int64_t) (((*step & bytemsbmask[off]) >> (8 - off))
                  + ((*(step - 1) & bytelsbmask[8 - start_off]) << off)) << i;
            DBGMSGLVL(3,
                  "Adding final value: %"PRIx64" = (((%x & %x)>>%d) + ((%x & %x)<<%x) )<<%d = (%x + %x) << %d\n",
                  (int64_t )(((*step & bytemsbmask[off]) >> (8 - off))
                        + ((*(step - 1) & bytelsbmask[8 - start_off]) << off))
                        << i, *step, bytemsbmask[off], (8 - off), *(step - 1),
                  bytelsbmask[8 - start_off], off, i,
                  ((*step & bytemsbmask[off]) >> (8 - off)),
                  ((*(step - 1) & bytelsbmask[8 - start_off]) << off), i);
         } else if (start_off < stop_off) { //We take more than 8 bits in the last byte, so we have to create an additional byte
            //Here step = start
            out += (int64_t) (((*step & bytemsbmask[off]
                  & bytelsbmask[8 - start_off]) >> (8 - off))) << i;
            DBGMSGLVL(3,
                  "Adding final value: %"PRIx64" = (((%x & %x & %x) >>%d) ) << %d) = %x << %d\n",
                  (int64_t )(((*step & bytemsbmask[off]
                        & bytelsbmask[8 - start_off]) >> (8 - off))) << i,
                  *step, bytemsbmask[off], bytelsbmask[8 - start_off],
                  (8 - off), i,
                  (((*step & bytemsbmask[off] & bytelsbmask[8 - start_off])
                        >> (8 - off))), i);
         }
      }
      break;
   }
   DBGMSGLVL(3, "Value is %#"PRIx64"\n", out);
   return out;
}

/**
 * Crops the last bits of a value_t object
 * \param val The value_t to crop. Its binary value will be reduced of \c len bits after this
 * \param len Length in bits (smaller than the total length of the binary value in \c val) to crop
 * */
static void value_crop(value_t* val, int len)
{
   unsigned char* newstop; //End of the binary value after removing \c len bits from it
   uint8_t newstop_off;
   //First, find where to stop
   if (val->bin_stop_off >= len) {
      //Removing only bits in the last byte of the value
      newstop = val->bin_stop;
      newstop_off = val->bin_stop_off - len;
   } else {
      //Removing more bytes
      newstop = val->bin_stop - ((len - val->bin_stop_off + 7) >> 3);
      newstop_off = (8 - (len % 8) + val->bin_stop_off) % 8;
   }
   //Cropping the last \c len bits from the value
   val->bin_stop = newstop;
   val->bin_stop_off = newstop_off;
}

/**
 * Removes (for reduction) the specified length of bits from the buffer for identified
 * variables
 * \param fc Pointer to the structure containing the context of the FSM
 * \param length The number of bits to remove
 * */
static void statebuffer_removebits(fsmcontext_t* fc, int length)
{
   int l = 0;
   buffer_t *buf;

   DBGMSGLVL(1, "Removing %d bits\n", length);

   while ((STACK_GETSIZE(fc->parser.buffer) > 1) && (l < length)) {
      buf = &STACK_GETTOP(fc->parser.buffer, buffer_t);
      /*Size to remove is smaller than last buffer size*/
      if ((length - l) < value_getbinlen(&buf->value)) {
         value_crop(&buf->value, (length - l));
         l = length;
      } else {
         l += value_getbinlen(&buf->value);
         STACK_CUT(fc->parser.buffer);
         buf->value.type = 0;/**\todo Check if this is useful, just marking this buffer element is deleted*/
      }
   }
   DBGLVL(1,
         DBGMSG("Removed value starting at index %"PRId64"\n", STREAM_LEN(fc->coding_start, STATEBUFFER_GETTOPBYTE(fc),fc->coding_start_off, STATEBUFFER_GETTOPBYTEOFF(fc)));
         //For the record, if we need the index of a paramcoding ever again, this is the formula to retrieve it.
         )
   DBGLVL(2, DBGMSG0("New buffer is:\n"); stackbuffer_dump(fc, stderr););
}

/**
 * Removes from the statebuffer a length of bits corresponding to a bitfield
 * \param fc Pointer to the structure containing the context of the FSM
 * \param size Size of bits to remove
 * */
static void reduc_bitfield(fsmcontext_t* fc, int size)
{
#ifndef NDEBUG
   //For debug only, saves the position of the last byte in the buffer
   unsigned char* lastbyte = STATEBUFFER_GETTOPBYTE(fc);
   uint8_t lastbyte_off = STATEBUFFER_GETTOPBYTEOFF(fc);
#endif
   statebuffer_removebits(fc, size);
   DBGMSGLVL(1,
         "Removed bitfield with value %#"PRIx64" and length %d from statebuffer\n",
         getstreamval(STATEBUFFER_GETTOPBYTE(fc), STATEBUFFER_GETTOPBYTEOFF(fc), lastbyte, lastbyte_off, BIG_ENDIAN_BIT),
         size);
}

/**
 * Removes from the statebuffer a length of bits corresponding to a token and puts the result in the appropriate
 * cell of the decoded_sym array in the fsm context
 * \param fc Pointer to the structure containing the context of the FSM
 * \param size Size of bits to remove
 * \param endian Endianness of the bits to remove
 * \param type Type of the token (index of the corresponding cell in decoded_sym)
 * */
static void reduc_token(fsmcontext_t* fc, int size, int endian, int type)
{
   //Saves the position of the last byte in the buffer
   unsigned char* lastbyte = STATEBUFFER_GETTOPBYTE(fc);
   uint8_t lastbyte_off = STATEBUFFER_GETTOPBYTEOFF(fc);
   //Removes the specified length of bits of the token in the buffer
   statebuffer_removebits(fc, size);
   //Calculates the value of the removed bits
   paramcoding_setvalue(&(fc->parser.decoded_syms[type]),
         getstreamval(STATEBUFFER_GETTOPBYTE(fc), STATEBUFFER_GETTOPBYTEOFF(fc),
               lastbyte, lastbyte_off, endian));
   /**\todo [MINJAG] Implement something more streamlined once everything is stabilised
    * => Did I mean the variables array containing the pointer to the cell in decoded_syms ?? Because this seems streamlined enough to me now*/
   paramcoding_setlength(&(fc->parser.decoded_syms[type]), size);
   DBGMSGLVL(1,
         "Removed token %s with value %#"PRIx64" and length %d from statebuffer\n",
         fc->fsmvars.varnames[type],
         paramcoding_getvalue(&(fc->parser.decoded_syms[type])), size);
   fc->parser.variables[type] = &(fc->parser.decoded_syms[type]);
}

/**
 * Removes a variable from the statebuffer
 * \param fc Pointer to the structure containing the FSM context
 * \param name Code for the name of the variable
 * \return Size in bits of the variable reduced
 * */
static int reduc_variable(fsmcontext_t* fc, int name)
{
   int reduced = statebuffer_removevar(fc, name);
   DBGMSGLVL(1,
         "Removed variable with value %#"PRIx64" and length %d from statebuffer\n",
         paramcoding_getvalue(&(fc->parser.decoded_syms[name])), reduced);
   return reduced;
}

/**
 * Removes a symbol from the statebuffer of a FSM.
 * If it is a token, its value will be added as a paramcoding
 * to the array of variables values in the FSM context
 * If it is a variable, its associated semantic action will be added
 * \param fc Pointer to the structure containing the FSM context
 * \param reduc Details of the reduction operation to perform
 * \return Number of bits reduced
 * */
static int reduc_symbol(fsmcontext_t* fc, fsmreduction_t* reduc)
{
   int reduced = 0;
   switch (reduc->symtype) {
   case REDUC_CST:
      reduc_bitfield(fc, reduc->symlen);
      reduced = reduc->symlen;
      break;
   case REDUC_TOK:
      reduced = reduc->symlen;
      reduc_token(fc, reduc->symlen, reduc->endianness, reduc->sym_id);
      break;
   case REDUC_VAR:
      reduced = reduc_variable(fc, reduc->sym_id);
      /**\note In the case of variables we will not store the result of the reduction in the variables array, as it will be only
       * used for storing the result of semantic actions. It is not stored in the decoded_syms array either
       * See the comment in state_process on how to proceed if we want at some point to retrieve the result of the reduction
       * of a given variable*/
      break;
   default:
      HLTMSG("[INTERNAL] Unknown symbol type found in the FSM buffer\n")
      ;
      /**\todo Maybe add a clean exit instead (should never happen but you never know)*/
   }
   return reduced;
}

/**
 * Sets a given semantic action for a symbol.
 * \param fc Pointer to the structure containing the context of the FSM
 * \param semactfct Pointer to the semantic action function to add
 * */
static void semaction_setinfsm(fsmcontext_t* fc, semfct_t semactfct)
{
   assert(fc != NULL);
   STACK_ADDELT(fc->parser.semactions, semactfct, semfct_t);
   /* It is not possible to try executing the actions in an order based on their operands because of recursive symbols (especially when the recursivity
    * is on multiple levels). So we just need to store the actions and execute them in the order into which they were stored (since it will be the order
    * of reduction, and it is the same one we want for our actions. It is no use trying to store the actions by symbol because a symbol can have multiple
    * actions in the case of recursivity.*/
}

/**
 * Processes a shift state in the FSM
 * \param fc Pointer to the structure containing the context of the FSM
 * \param shiftstate The state to process
 * \return TRUE if the shift was successful, FALSE otherwise
 * */
static int stateshift_process(fsmcontext_t* fc, stateshift_t* stateshift)
{
   int out;
   /*Shift state*/
   if (fc->parser.lastreducvar > 0) {
      /*We have just reduced a variable*/
      DBGMSG("Variable %s has just been reduced\n",
            fc->fsmvars.varnames[fc->parser.lastreducvar]);
      /*Looking for a transition over this variable*/
      if (stateshift->vartrans[fc->parser.lastreducvar] == STATE_NONE) { //No match found
         DBGMSG("No match found for variable %s\n",
               fc->fsmvars.varnames[fc->parser.lastreducvar]);
         out = FALSE;
      } else {
         //Adds variable and next state to buffer
         statebuffer_addvar(fc, stateshift->vartrans[fc->parser.lastreducvar]);
         //If there are bits to shift after a variable, add another value to the buffer
         if (stateshift->shiftvars[fc->parser.lastreducvar] > 0) {
            if (fsmcontext_nexttrans(fc,
                  stateshift->shiftvars[fc->parser.lastreducvar])) {
               DBGMSG(
                     "End of stream reached while adding %u bits after variable %d\n",
                     stateshift->shiftvars[fc->parser.lastreducvar],
                     fc->parser.lastreducvar);
               out = FALSE; //End of stream reached
            }
            //Adds a value with the given length to the buffer, associated with the state
            DBGMSGLVL(1, "Shifting additional %u bits after variable %d\n",
                  stateshift->shiftvars[fc->parser.lastreducvar],
                  fc->parser.lastreducvar);
            shift_bits(fc, stateshift->vartrans[fc->parser.lastreducvar]);
            out = TRUE;
         } else {
            out = TRUE;
         }
      }
   } else {
      /*We are still looking for a leaf*/
      DBGMSG0("Looking for next state based on binary transitions\n");
      out = state_findnext(fc, stateshift);
   }
   fc->parser.lastreducvar = 0;
   return out;
}

/**
 * Processes a reduction state in the FSM
 * \param fc Pointer to the structure containing the context of the FSM
 * \param statereduc The state to process
 * */
static void statereduc_process(fsmcontext_t* fc, statereduc_t* statereduc)
{
   int i;
   int reduced_bits = 0;
   /*Reduction state*/
   fc->parser.lastreducvar = statereduc->var_id;
   /**\todo Write the reduction list in MINJAG in the correct order to avoid having to revert this*/
   for (i = statereduc->n_reducs - 1; i >= 0; i--) {
      reduced_bits += reduc_symbol(fc, statereduc->reducs[i]);
      /**\note If at some point in the future we need to retrieve the value of the bits composing a reduced variable
       * (only the tokens should be ever needed), we would need here to store the start and end of the variable in the
       * stream. This could be done by replacing the int fc->lastreducvar with a value_t structure where we could store the end byte/offset
       * of the variable (it's input/input_off) and its beginning byte/offset (it's the end byte/offset of what is at the top
       * of the buffer stack after the loop above. Then when adding this variable to the buffer in the next shift state
       * it would contain the correct start/end bits.
       * Warning: this means that, if we attempt to compute the value of such a variable, it may exceed 64 bits.
       * I don't implement this yet for optimisation purposes
       * => I'm somehow implementing this now, by simply updating the value boundaries in the buffer. This should be enough*/

      //Removing additional states with a null length and whose already tested bits are
      while ((STACK_GETSIZE(fc->parser.buffer) > 1)
            && (value_getbinlen(&(STACK_GETTOP(fc->parser.buffer,buffer_t).value)) == 0 )
      && ( STACK_GETTOP(fc->parser.buffer,buffer_t).state->firsttested < reduced_bits ) ) {
         DBGMSGLVL(2, "Removing additional state (first tested bit %u, reduced bits %d)\n",
         STACK_GETTOP(fc->parser.buffer,buffer_t).state->firsttested, reduced_bits);
         STACK_CUT(fc->parser.buffer);
      }
   }
   /*Sets semantic action*/
   semaction_setinfsm(fc, statereduc->semactfct);
   DBGMSG("Symbol %s has been reduced and has action (%p)\n",
         fc->fsmvars.varnames[statereduc->var_id], statereduc->semactfct);

   /*Updates the action to perform at the end*/
   if (fc->fsmvars.finalfcts[statereduc->endact_id] != NULL) {
      fc->final_action = fc->fsmvars.finalfcts[statereduc->endact_id];
      DBGMSG("Found finalizing action %p for the reduced state\n",
            fc->final_action);
   }

   //Special case: we have a reduction with no reduction steps (empty symbol). We have to remove a state from the stack
   if (statereduc->n_reducs == 0) {
      STACK_CUT(fc->parser.buffer);
   }
   //Invalidate any saved parser, as this would be too complicated to switch back if reductions occurred
   fc->altrdc = FALSE;
   DBGMSG("Next state is %d (%p)\n",
         (STACK_GETTOP(fc->parser.buffer,buffer_t)).stateid,
         (STACK_GETTOP(fc->parser.buffer,buffer_t)).state);

}
/**
 * Processes a state in the FSM
 * \param fc Pointer to the structure containing the context of the FSM
 * \param state The state to process
 * */
static void state_process(fsmcontext_t* fc, fsmstate_t* state)
{
   switch (state->type) {
   case FSMSTATE_SHIFT:
      if (stateshift_process(fc, state->state.shift) == FALSE) {
         fsmstate_t* altstate = fsmcontext_restoreparser(fc); //Check if a parser was backed up due to a shift/reduce state
         if (altstate) {
            //A shift/reduce state occurred before and an alternate parser state was saved
            assert(altstate->type == FSMSTATE_SHRDC); /**\todo Maybe add a CRITICAL on FSM INTERNAL here*/
            //Execute the reduction associated to this shift/reduce state
            statereduc_process(fc, altstate->state.shrdc->statereduc);
         } else {
            //No parser backup: this is a parser error
            fsm_error(fc, ERR_DISASS_FSM_NO_MATCH_FOUND);
         }
      }
      break;
   case FSMSTATE_REDUC:
      statereduc_process(fc, state->state.reduc);
      break;
   case FSMSTATE_SHRDC:
      /*Shift / reduction state*/
      /**\note Shift/reduction states attempt to perform the shift, then default back to the reduction if it fails. However, the case
       * can arise when the shift error occurs in a later state, which will then result in a parser error. In order to handle this,
       * we save the parser variables when such a state is encountered, so that we are able to restore it if an error occurs later.
       * The saved parsers are stored in a stack since we might encounter multiple shift/reduce state in a row.*/
      /*We save the parser state at this point in case the error happens later, so that we will be able to switch back to this reduction.
       * We do this before invoking stateshift_process, so that the parser is in its original state*/
      fsmcontext_saveparser(fc);
      if (stateshift_process(fc, state->state.shrdc->stateshift) == FALSE) {
         //Performs the reduction
         statereduc_process(fc, state->state.shrdc->statereduc);
         //Deletes the saved parser state, since it has already been used
         STACK_CUT(fc->altparser);
      }
      break;
   case FSMSTATE_FINAL:
      fc->parsecomplete = TRUE;
      //Emptying the stack of alternate parsers as it is now useless
      STACK_EMPTY(fc->altparser);
      DBGMSG0("Final state reached\n")
      ;
      break;

   }
   DBGMSGLVL(1, "New stack size is %d\n", STACK_GETSIZE(fc->parser.buffer));
}

/**
 * Runs the fsm for one parsing operation. It must have been initialised first (first state loaded into the states stack)
 * \param fc Pointer to the structure containing the context of the FSM
 * */
static void fsm_parseword(fsmcontext_t* fc)
{

   while (!fc->parsecomplete) {
      DBGMSG("Processing state %d (%p)\n",
            (STACK_GETTOP(fc->parser.buffer,buffer_t)).stateid,
            (STACK_GETTOP(fc->parser.buffer,buffer_t)).state);
      state_process(fc, (STACK_GETTOP(fc->parser.buffer, buffer_t)).state);
      DBGLVL(2,
            DBGMSG0("Variable array:\n"); int z; for (z = 0; z < fc->fsmvars.n_variables; z++) if(fc->parser.variables[z]) STDMSG("\tvariable[%s]=%p\n",fc->fsmvars.varnames[z],fc->parser.variables[z]);)
   }
   DBGMSG0("Last state reached\n");
}

/*
 * Checks if the parsing is completed
 * \param fc Pointer to the structure containing the FSM context
 * \return 1 if there is no more bytes to parse, 0 otherwise
 * */
int fsm_isparsecompleted(fsmcontext_t* fc)
{
   return ((fc->parser.input - fc->input0) >= fc->inputlen);
}

/*
 * Initialises the variables in a FSM context used for a parsing.
 * Has to be called before launching a parse operation
 * \param fc Pointer to the structure containing the FSM context (must already be initialised)
 * */
void fsm_parseinit(fsmcontext_t* fc)
{
   /**\todo In order to allow parsing to jump from one address to another, we should add, here or elsewhere, a function allowing
    * to reset the entrybuffer in relation to the position in the stream. Or simply, after each parse, emptying the entrybuffer and resetting the
    * input variable (this may be tied to a given mode, to improve performances depending on whether we are parsing instruction by instruction or in bundle)
    * => This is partly done with fsm_setstream*/

   DBGMSG0("Entering fsm_parseinit\n");

   fc->fsmerror = EXIT_SUCCESS;
}

/*
 * Creates a new FSM context from its initialisation function
 * \param fsmloader Function for loading the states table
 * \return A pointer to a structure containing the initialised FSM context
 * */
static fsmcontext_t* fsmcontext_new(void (*fsmloader)(fsmload_t*))
{
   /**\todo The data updated by the fsmloader parameter could be provided differently */
   fsmcontext_t* fc;

   fc = (fsmcontext_t*) lc_malloc0(sizeof(fsmcontext_t));
   fsmloader(&(fc->fsmvars));
   fsmparser_init(&fc->parser, &fc->fsmvars);

   statebuffer_add(fc, 0, 0);

   fc->altparser = fsmstack_new();
   fc->parsecomplete = TRUE;

   return fc;
}

/**
 * Frees a FSM context
 * \param fc Pointer to the structure containing the FSM context to free
 * */
static void fsmcontext_free(fsmcontext_t* fc)
{
   fsmparser_terminate(&fc->parser);

   STACK_FLUSH(fc->altparser, fsmparser_terminate, fsmparser_t);
   fsmstack_free(fc->altparser);

   lc_free(fc);
}

/*
 * Initialises the FSM. Must be invoked before any parsing operation
 * \param fsmloader Pointer to the function for loading the FSM values
 * \return Structure storing the FSM environment
 * */
fsmcontext_t* fsm_init(void (*fsmloader)(fsmload_t*))
{
   if (!fsmloader)
      HLTMSG("[INTERNAL] Attempted to initialise FSM with empty loader\n");
   return fsmcontext_new(fsmloader);
}

/*
 * Reinitialises an existing FSM. This allows to change the FSM values while keeping the FSM state.
 * This must not be performed during an parsing operation
 * \param fc A pointer to the structure containing an already initialised FSM context. Must not be NULL
 * \param fsmloader Pointer to the function for loading the FSM values
 * */
void fsm_reinit(fsmcontext_t* fc, void (*fsmloader)(fsmload_t*))
{
   if (!fc || !fsmloader) {
      ERRMSG(
            "[INTERNAL] Attempted to reinitialise a NULL FSM or using an empty loader\n");
      return;
   }
   if (!fc->parsecomplete) {
      ERRMSG("[INTERNAL] Attempted to reinitialise an FSM during parsing\n");
      return;
   }
   //Resets the internal values of the FSM
   fsmloader(&(fc->fsmvars));
   fsmparser_reinit(&fc->parser, &fc->fsmvars);
   STACK_FLUSH(fc->altparser, fsmparser_terminate, fsmparser_t);

   //Reinitialises the stack of states with the first state of the new FSM
   STACK_EMPTY(fc->parser.buffer);
   statebuffer_add(fc, 0, 0);
}

/*
 * Terminates the FSM. No parsing operation will be possible afterwards
 * \param fc Structure storing the FSM environment
 * */
void fsm_terminate(fsmcontext_t* fc)
{
   if (fc)
      fsmcontext_free(fc);
}

/*
 * Sets the stream the FSM will have to parse
 * \param fc Pointer to the structure containing the FSM context
 * \param stream The stream of bytes to be parsed
 * \param streamlen The length in bytes of \e stream
 * \param startaddr The address associated to the beginning of \e stream
 * */
void fsm_setstream(fsmcontext_t* fc, unsigned char* stream, int streamlen,
      int64_t startaddr)
{
   fc->parser.input = stream;
   fc->input0 = stream;
   fc->inputlen = streamlen;
   fc->first_address = startaddr;
}

/*
 * Resets the position of the FSM on the current stream. This allows to jump in another position of the stream being disassembled.
 * This function must not be called in the middle of a parsing operation
 * \param fc Pointer to  the structure containing the FSM context
 * \param newaddr New address in the stream at which the parsing must resume
 * \return EXIT_SUCCESS / error code
 * */
int fsm_resetstream(fsmcontext_t* fc, int64_t newaddr)
{
   if ((newaddr < fc->first_address)
         || (newaddr > (fc->first_address + fc->inputlen))) {
      WRNMSG(
            "Address %#"PRIx64" is out of parsing range. Parsing will resume from address %#"PRIx64"\n",
            newaddr, fsm_getcurrentaddress(fc));
      return WRN_DISASS_FSM_RESET_ADDRESS_OUT_OF_RANGE;
   }
   if ((!fc->parsecomplete)) {
      WRNMSG(
            "Attempted resetting the parsed stream while parsing a word. Parsed stream will not be reset\n");
      return WRN_DISASS_FSM_RESET_ADDRESS_PARSING_IN_PROGRESS;
   }
   //Resetting pointer in stream
   fc->parser.input = fc->input0 + (newaddr - fc->first_address);
   DBGMSG("Parsed stream was reset to address %#"PRIx64"\n",
         fsm_getcurrentaddress(fc));
   return EXIT_SUCCESS;
}

/*
 * Sets the function to execute when an error is encountered
 * \param fc Pointer to the structure containing the FSM context
 * \param errorhandler Function to execute when a parsing error is encountered
 * */
void fsm_seterrorhandler(fsmcontext_t* fc,
      void (*errorhandler)(fsmcontext_t*, void**, void*))
{
   if (fc != NULL)
      fc->errorhandler = errorhandler;
}

/**
 * Execute all the stored semantic actions of a parsed file, by invoking the semaction_execute
 * for each non NULL element of the actions array
 * \param fc Pointer to the structure containing the context of the FSM
 * */
static void actions_execute(fsmcontext_t* fc)
{

   int i;
   for (i = 0; i < STACK_GETSIZE(fc->parser.semactions); i++) {
      semfct_t sf = NULL;
      sf = STACK_GETELT(fc->parser.semactions, i, semfct_t);
      if (sf != NULL) {
         DBGMSG("Executing semantic action function %p\n", sf);
         sf(fc->parser.variables);
      }
   }
   STACK_EMPTY(fc->parser.semactions);
}

/**
 * Resets the fsm for a new parsing on the same data stream
 * \param fc Pointer to the structure containing the context of the FSM
 * */
static void fsm_reset(fsmcontext_t* fc)
{

   fc->fsmerror = EXIT_SUCCESS;
   fc->parsecomplete = FALSE;

   fc->coding_start = fc->parser.input;
   fc->coding_start_off = fc->parser.input_off;

   fc->parser.next = fc->parser.input;
   fc->parser.next_off = fc->parser.input_off;

   fc->parser.lastreducvar = 0;

   /*Empties the state buffer*/
   /**\todo Find a more elegant way to do this. It is only useful because Axiom remains in the buffer at the end given the currently unoptimised way
    * the end of processing is detected => Check if this is the case now that the end of processing is detected by a special type of state*/
   statebuffer_empty(fc);
   /*Resets the first state (which is at the top of the stack) to point to the current stream.
    * This is to ensure the calculation of variables sizes is correct*/
   /**\todo If I keep this, factorise it with the code in statebuffer_add (a macro could do the trick)*/
   STACK_GETTOP(fc->parser.buffer, buffer_t).value.bin_start = fc->parser.input;
   STACK_GETTOP(fc->parser.buffer, buffer_t).value.bin_stop = fc->parser.next;
   STACK_GETTOP(fc->parser.buffer, buffer_t).value.bin_start_off = fc->parser.input_off;
   STACK_GETTOP(fc->parser.buffer, buffer_t).value.bin_stop_off = fc->parser.next_off;
}

/**
 * Frees the contents of the arrays containing variables and semantic actions
 * \param fc Pointer to the structure containing the context of the FSM
 * */
static void free_vars_actions(fsmcontext_t* fc)
{

   assert(fc->fsmvars.n_variables > 0);
   memset(fc->parser.variables, 0, sizeof(void*) * fc->fsmvars.n_variables);
   fc->final_action = NULL;
}

/*
 * Returns the address being parsed, based on the bytes already read and the first address of the parsed stream
 * \param fc Pointer to the structure containing the current FSM context
 * \return The current address being parsed (in bytes)
 * */
int64_t fsm_getcurrentaddress(fsmcontext_t* fc)
{
   return (fc->parser.input - fc->input0 + fc->first_address);
}

/*
 * Returns the binary coding of the word that was parsed. It is advised to invoke this only after completing fsm_parse
 * \param fc Pointer to the structure containing the current FSM context
 * \return Allocated bitvector_t containing the binary coding of the word
 * */
bitvector_t* fsm_getcurrentcoding(fsmcontext_t* fc)
{
   return bitvector_new_from_stream(fc->coding_start, fc->coding_start_off,
         fc->parser.input, fc->parser.input_off);
}

/*
 * Returns the maximum instruction's length in the current FSM
 * \param fc Pointer to the structure containing the current FSM context
 * \return The maximum instruction's length
 */
unsigned int fsm_getmaxinsnlength(fsmcontext_t* fc)
{
   return fc->fsmvars.insn_maxlen;
}

/*
 * Performs the parsing of a binary stream
 * \param fc Pointer to the structure holding the details about a FSM operation
 * \param out Return parameter. Will contain a pointer to the object generated by the parsing
 * \param finalinfo Structure to pass to the finalizing macro
 * \param userinfo Structure to pass to the error handler
 * \return EXIT_SUCCESS if the parsing was successful, error code otherwise
 * */
int fsm_parse(fsmcontext_t* fc, void** out, void* finalinfo, void* userinfo)
{
   /**\todo (2015-07-29) userinfo should also be passed to all semantic action macros*/
   int output = EXIT_SUCCESS;

   /*Initialises the FSM*/
   fsm_reset(fc);

   /*Launches the parser*/
   fsm_parseword(fc);

   /*Executes the semantic actions*/
   if (fc->fsmerror) { //An error occurred during parsing
      DBGMSG0("Parser error\n");
      /*Resets the pointer to the first unparsed byte in the input stream to skip as many bits from its last position
       * as the length of the smallest instruction. */
      STREAM_ADDLEN(fc->coding_start, fc->coding_start_off, fc->parser.input,
            fc->parser.input_off, fc->fsmvars.insn_minlen);

      /*Executes the custom error handler*/
      if (fc->errorhandler)
         fc->errorhandler(fc, out, userinfo);

      statebuffer_empty(fc);

      output = fc->fsmerror;
   } else { //No error: executing the semantic actions
      DBGMSG0("Executing semantic actions\n");
      actions_execute(fc);
      *out = fc->parser.variables[TMPLTIDX];
      /**\todo This is the hard-coded value of where the template variable will have been reduced (it's set by minjag).
       * A better process would be to retrieve what is in Axiom (which could also be a fixed index in the array or somewhere else),
       * which would have been set by an artificial macro added to the macro lists by minjag.
       * And of course, to actually reduce the Axiom instead of leaving it hang like that. Or something similar.
       * Additionally, this would allow to invoke the semantic action for Axiom automatically*/

      /*Execute the function associated to the decoded instruction*/
      if (fc->final_action && finalinfo) {
         DBGMSG0("Executing finalizing action\n");
         fc->final_action(*out, finalinfo);
         fc->final_action = NULL;
      }
   }
   /**\todo The coding is now calculated on demand using fsm_getcurrentcoding, instead of being stored in the fsmcontext_t instance and
    * accessed by the disassembler. The performances may be impacted because of the extra call so if some day we need to save every last cycle,
    * we could restore the calculation of the coding here and its storing in fsmcontext_t*/

   /*Resets the FSM variables*/
   free_vars_actions(fc);
   return output;
}
