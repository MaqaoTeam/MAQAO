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

#ifndef __LC_LIBMCOMMON_H__
#define __LC_LIBMCOMMON_H__

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <inttypes.h>
#include "maqaoerrs.h"

/**
 \file libmcommon.h
 \brief Libmcommon provides several basic data structures such as lists, hashtables or graphs ... .
 \page page1 Libmcommon documentation page

 \section overview_libcommon Overview
 Libmcommon is a C library which provides severals basic data structures:
 - lists
 - queues
 - hashtables
 - bitvectors
 - graphs
 - trees

 Moreover, it provides several functions and macros for memory management and
 string manipulation.
 */

///////////////////////////////////////////////////////////////////////////////
//                            macro definition                               //
///////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
#undef FALSE
#undef TRUE
#endif
typedef enum boolean_e {
   FALSE = 0, /**<Can replace boolean false*/
   TRUE /**<Can replace boolean true*/
} boolean_t;

/**
 * Prints a message if the level of verbosity is higher than or equal the required level
 * \param LVL The level of verbosity for which to print this message
 * \param STREAM Stream where to print the message
 * \param x source code to execute
 */
#define PRINT_MESSAGE(LVL, STREAM, ...)  {\
    if (__MAQAO_VERBOSE_LEVEL__ >= LVL) {\
       fprintf(STREAM, __VA_ARGS__);\
       if (isatty (fileno (STREAM)))\
          fprintf(STREAM, "\e[0m");\
       fflush (STREAM);\
    }\
}

/**<Prints a message then exits with error code 1*/
#define HLTMSG(...) { \
   if (isatty (fileno (stderr)))\
      {PRINT_MESSAGE(MAQAO_VERBOSE_CRITICAL, stderr, "\n\e[31m\e[1m** Critical:: "__VA_ARGS__ ); exit(1);}\
   else \
      {PRINT_MESSAGE(MAQAO_VERBOSE_CRITICAL, stderr, "\n** Critical: " __VA_ARGS__ ); exit(1); }\
}

/**<Prints a message on stderr*/
#define ERRMSG(...)  {\
   if (isatty (fileno (stderr)))\
      {PRINT_MESSAGE(MAQAO_VERBOSE_ERROR, stderr, "\n\e[31m\e[1m** Error::\e[21m "__VA_ARGS__ )}\
   else \
      {PRINT_MESSAGE(MAQAO_VERBOSE_ERROR, stderr, "\n** Error: " __VA_ARGS__ )}\
}

/**<Prints a message on stderr*/
#define WRNMSG(...)  {\
   if (isatty (fileno (stderr)))\
      {PRINT_MESSAGE(MAQAO_VERBOSE_WARNING, stderr, "\n\e[33m\e[1m* Warning:\e[21m "__VA_ARGS__ )}\
   else \
      {PRINT_MESSAGE(MAQAO_VERBOSE_WARNING, stderr, "\n* Warning: "__VA_ARGS__ )}\
}
#define STDMSG(...)  PRINT_MESSAGE(MAQAO_VERBOSE_MESSAGE, stderr, __VA_ARGS__ )                 /**<Prints a message on stderr*/
#define INFOMSG(...) PRINT_MESSAGE(MAQAO_VERBOSE_INFO, stdout,"\n* Info: "__VA_ARGS__)          /**<Prints an info message on stdout*/

/**
 * Macro for printing something in a string. The string pointer is updated to point at the end of the printed data
 * \param S The string to write to
 * \param Size The size of the string to write to
 * \param ... What to write
 * */
#define PRINT_INSTRING(S, Size, ...) {lc_sprintf(S, Size, __VA_ARGS__);while(*S!='\0') S++;}

/**
 * Utility macro for retrieving the name of a file from its full path
 * \param p Path of the file
 * */
//#define __basename(p) ((strrchr(p, '/')) ? strrchr(p, '/')+1 : p)

#ifndef NDEBUG /* it will be paired with assert inhibition */

/**
 * Variable storing whether a file must execute debug macros or not.
 * This allows to reduce the number of invocations to \c getenv from the debug macros, as only the first instance of a debug
 * macro in a file will trigger it and initialise the variable.
 * The possible values are:
 * - \b -1: The environment variable must be checked
 * - \b 0: The debug macros must not be executed
 * - \b 1: The debug macros must be executed
 * - \b >1: The level-dependent debug macros of level strictly inferior to the value must be executed
 * */
#ifdef _WIN32
static int __is_debug_activated_for_this_file = -1;
#else
static int __is_debug_activated_for_this_file __attribute__((unused)) = -1;
#endif

/**
 * Executes a debug action with parameters if DEBUG_FILE = current source file
 * \param x source code
 */
#define DBG(x) DBGLVL(0, x)

/**
 * Executes a debug action with parameters if DEBUG_FILE = current source file and DEBUG_LVL >= LVL
 * DEBUG_LVL must be >= 0
 * \param LVL debug level of the message
 * \param x source code to execute
 */
#define DBGLVL(LVL, x)   {\
   if (__is_debug_activated_for_this_file) {\
      if (__is_debug_activated_for_this_file > LVL) {\
         x;\
      } else if (__is_debug_activated_for_this_file == -1) {\
         char *vf__=getenv("DEBUG_FILE"); char *vl__=getenv("DEBUG_LVL");\
         if (vf__ && strstr(vf__,lc_basename(__FILE__))) {\
            __is_debug_activated_for_this_file = ( ( vl__ && (atoi(vl__)>0) )?atoi(vl__):0 ) + 1;\
            if (LVL < __is_debug_activated_for_this_file) {\
               x ;\
            }\
         } else __is_debug_activated_for_this_file = 0;\
      }\
   }\
}

/**
 * Prints a debug message without parameters prefixed by the name of the function where it is invoked
 * \param F a string used a printf format
 */
#define FCTNAMEMSG0(F) fprintf(stderr,"%s:" F, __FUNCTION__)

/**
 * Prints a debug message with parameters prefixed by the name of the function where it is invoked
 * \param F a string used a printf format
 * \param ... user variables (such as printf)
 */
#define FCTNAMEMSG(F, ...) fprintf(stderr,"%s:" F, __FUNCTION__, __VA_ARGS__)

/**
 * Prints a debug message with parameters if DEBUG_FILE = current source file
 * \param F a string used a printf format
 * \param ... user variables (such as printf)
 */
#define DBGMSG(F,...) DBG(FCTNAMEMSG(F, __VA_ARGS__))

/**
 * Prints a debug message with parameters if DEBUG_FILE = current source file and DEBUG_LVL >= LVL
 * DEBUG_LVL must be >= 0
 * \param LVL debug level of the message
 * \param F a string used a printf format
 * \param ... user variables (such as printf)
 */
#define DBGMSGLVL(LVL, F,...) DBGLVL(LVL, FCTNAMEMSG(F, __VA_ARGS__))

/**
 * Prints a debug message with no parameters if DEBUG_FILE = current source file and DEBUG_LVL >= LVL
 * DEBUG_LVL must be >= 0
 * \param LVL debug level of the message
 * \param F a constant string to print
 */
#define DBGMSG0LVL(LVL, F) DBGLVL(LVL, FCTNAMEMSG0(F);)

/**
 * Prints a debug message with no parameters if DEBUG_FILE = current source file
 */
#define DBGMSG0(F) DBGMSG0LVL(0, F)

#else
/**
 * Do nothing. Defined to be compatible with debug mode
 * \param x useless
 */
#define DBG(x)

/**
 * Do nothing. Defined to be compatible with debug mode
 * \param LVL useless
 * \param x useless
 */
#define DBGLVL(LVL, x)

/**
 * Do nothing. Defined to be compatible with debug mode
 * \param F useless
 */
#define FCTNAMEMSG0(F)

/**
 * Do nothing. Defined to be compatible with debug mode
 * \param F useless
 */
#define FCTNAMEMSG(F, ...)

/**
 * Do nothing. Defined to be compatible with debug mode
 * \param F useless
 */
#define DBGMSG(F,...)

/**
 * Do nothing. Defined to be compatible with debug mode
 * \param F useless
 */
#define DBGMSGLVL(LVL, F,...)

/**
 * Do nothing. Defined to be compatible with debug mode
 * \param F useless
 */
#define DBGMSG0(F)

/**
 * Do nothing. Defined to be compatible with debug mode
 * \param LVL useless
 * \param F useless
 */
#define DBGMSG0LVL(LVL, F)
#endif

#define SIGNED_ERROR -1         /**<Error code for a signed type*/
#define UNSIGNED_ERROR 0        /**<Error code for an unsigned type*/
#define PTR_ERROR NULL          /**<Error code for a pointer*/

/**
 * Concatenates two macros.
 * MACRO_CONCAT(BEGIN,END) in the code will be pre-processed as BEGINEND
 * This macro is not very useful by itself but is needed by \ref MACROVAL_CONCAT
 * \param X First macro to concatenate
 * \param Y Second macro to concatenate
 * */
#define MACRO_CONCAT(X,Y) X ## Y

/**
 * Concatenates the values that two macros stand for
 * MACROVAL_CONCAT(MACRO1,MACRO2) in the code, where MACRO1 is preprocessed as \e foo and
 * MACRO2 as \e bar, will be preprocessed as \e foobar
 * \param X First macro to concatenate
 * \param Y Second macro to concatenate
 * */
#define MACROVAL_CONCAT(X,Y) MACRO_CONCAT(X,Y)

/**
 * Transforms a macro to string.
 * MACRO_TOSTRING(MACRO) in the code will be preprocessed as "MACRO"
 * \param X Macro to transform
 * */
#define MACRO_TOSTRING(X) #X

/**
 * Transforms the value a macro stands for into a string
 * MACROVAL_TOSTRING(MACRO) in the code, where MACRO is preprocessed as \e foo,
 * will be preprocessed as "foo"
 * \param X Macro to transform
 * */
#define MACROVAL_TOSTRING(X) MACRO_TOSTRING(X)

/** Macro for exporting the content of a hashtable to an array allocated in the stack
 * \param T Type of the data stored in the hashtable
 * \param H The hashtable to export
 * \param A The name of the output array
 * \param N The size of the array (must already be defined)
 * */
#define HASHTABLE_TO_ARRAY(T,H,A,N) T A[hashtable_size(H)];N=0;{FOREACH_INHASHTABLE(H,expiter) {\
        A[N++] = GET_DATA_T(T,expiter);};}\
        assert(N==hashtable_size(H));

/** Macro for exporting the content of a hashtable to an array allocated on the heap
 * \param T Type of the data stored in the hashtable
 * \param H The hashtable to export
 * \param A The name of the output array
 * \param N The size of the array (must already be defined)
 * */
#define HASHTABLE_TO_MALLOC_ARRAY(T,H,A,N) T* A = lc_malloc(sizeof(T)*hashtable_size(H));N=0;{FOREACH_INHASHTABLE(H,expiter) {\
    A[N++] = GET_DATA_T(T,expiter);};}\
    assert(N==hashtable_size(H));

/** Macro for exporting the keys of a hashtable to an array
 * \param T Type of the key of the hashtable
 * \param H The hashtable to export
 * \param A The name of the output array
 * \param N The size of the array (must already be declared)
 * */
#define HASHTABLEKEY_TO_ARRAY(T,H,A,N) T A[hashtable_size(H)];N=0;{FOREACH_INHASHTABLE(H,expiter) {\
    A[N++] = GET_KEY(T,expiter);};}\
    assert(N==hashtable_size(H));

/** Macro for exporting the content of a queue to an array allocated in the stack
 * \param T Type of the data stored in the queue
 * \param Q The queue to export
 * \param A The name of the output array
 * \param N The size of the array (must already be declared)
 * */
#define QUEUE_TO_ARRAY(T,Q,A,N) T A[queue_length(Q)];N=0;{FOREACH_INQUEUE(Q,expiter) {\
    A[N++] = GET_DATA_T(T,expiter);};}\
    assert(N==queue_length(Q));

/** Macro for exporting the content of a queue to an array allocated on the heap
 * \param T Type of the data stored in the queue
 * \param Q The queue to export
 * \param A The name of the output array
 * \param N The size of the array (must already be declared)
 * */
#define QUEUE_TO_MALLOC_ARRAY(T,Q,A,N) T* A = lc_malloc(sizeof(T)*queue_length(Q));N=0;{FOREACH_INQUEUE(Q,expiter) {\
    A[N++] = GET_DATA_T(T,expiter);};}\
    assert((int)N==queue_length(Q));

/**
 * Macro for adding a cell at the end of an allocated array
 * \param A The array
 * \param S The number of elements in the array
 * \param C The element to add in the last cell
 * */
#define ADD_CELL_TO_ARRAY(A, S, C) {A = lc_realloc(A, sizeof(*A)*(S+1));\
   A[S] = C;\
   S = S + 1;}

/**
 * Macro for inserting a cell inside an allocated array
 * \param A The array
 * \param S The size of the array
 * \param C The element to add in the cell
 * \param I Index of the cell into which to insert the cell (assumed smaller than S). The following cells will be shift on cell up
 * */
#define INSERT_CELL_INTO_ARRAY(A, S, C, I) {A = lc_realloc(A, sizeof(*A)*(S+1));\
   memmove(A + I + 1, A + I, sizeof(*A)*(S-I));\
   A[I] = C;\
   S = S + 1;}

/**
 * Macro from removing a cell inside an allocated array
 * \param A The array
 * \param S The size of the array
 * \param I The index of the cell to remove (assumed smaller than S)
 * */
#define REMOVE_CELL_FROM_ARRAY(A, S, I) {memmove(A + I, A + I + 1, sizeof(*A)*(S-I-1));\
   A = lc_realloc(A, sizeof(*A)*(S-1));\
   S = S - 1;}

/**\todo (2015-03-10) The following two macros are intended to be used for retrieving a sub flag in a flag. In structures,
 * it is advised to use bitfields instead (with the "unsigned flag:x" notation, where x is the size in bits of the flag field),
 * but those could be needed for structure with customisable flags fields or flags used in function parameters*/
/**
 * Macro used for getting a given sub value inside a flag
 * \param FLAG The full value of the flag
 * \param POS Position (in bits) of the sub value, starting from the less significant bit of the flag
 * \param SIZE Size (in bits) of the sub value
 * */
#define FLAG_GETSUBVALUE(FLAG, POS, SIZE) ((FLAG & (((1<<SIZE)-1)<<POS) ) >> POS)
/**
 * Macro used for setting a given sub value inside a flag
 * \param FLAG The value of the flag to update
 * \param VALUE The sub value to set
 * \param POS Position (in bits) of the sub value, starting from the less significant bit of the flag
 * \param SIZE Size (in bits) of the sub value
 * */
#define FLAG_UPDSUBVALUE(FLAG, VALUE, POS, SIZE) ((FLAG & ~((typeof(FLAG))(((1<<SIZE)-1)<<POS))) | ((VALUE << POS) &  (((1<<SIZE)-1)<<POS)))
/**
 * Alias for struct list_s structure
 */
typedef struct list_s list_t;

/**
 * Alias for struct queue_s structure
 */
typedef struct queue_s queue_t;

/**
 * Alias for struct array_s structure
 */
typedef struct array_s array_t;

/**
 * Alias for struct tree_s structure
 */
typedef struct tree_s tree_t;

/**
 * Alias for struct bitvector_s structure
 */
typedef struct bitvector_s bitvector_t;

/**
 * Alias for struct graph_node_s structure
 */
typedef struct graph_node_s graph_node_t;

/**
 * Alias for struct graph_edge_s structure
 */
typedef struct graph_edge_s graph_edge_t;

/**
 * Alias for struct graph_connected_component_s structure
 */
typedef struct graph_connected_component_s graph_connected_component_t;

/**
 * Alias for struct graph_s structure
 */
typedef struct graph_s graph_t;

/**
 * Alias for struct hashtable_s structure
 */
typedef struct hashtable_s hashtable_t;

/**
 * Alias for struct hashnode_s structure
 */
typedef struct hashnode_s hashnode_t;

///////////////////////////////////////////////////////////////////////////////
//                            memory functions                               //
///////////////////////////////////////////////////////////////////////////////
#ifndef MEMORY_LEAK_DEBUG

#ifndef MEMORY_TRACE_USE

#ifndef MEMORY_TRACE_SIZE
// Mode no memory trace
/**
 * Wrapper for the free function when no memory tracing is active. This is needed when a pointer to
 * the lc_free function will be used
 * \param ptr A pointer on a memory area to free
 * */
#ifdef _WIN32
static __inline void lc_free(void* ptr) {free(ptr);}
#else
static inline void lc_free(void* ptr)
{
   free(ptr);
}
#endif
///**
// * Wrapper for the malloc function when no memory tracing is active. This is needed when a pointer
// * to the lc_malloc function will be used
// * \param size Size of memory area to allocate
// * \return A pointer on an allocated area
// * */
//static inline void* lc_malloc(unsigned long size) { return malloc(size);} //This is needed when the function pointer will be used
///**
// * Wrapper for the strdup function when no memory tracing is active. This is needed when a pointer
// * to the lc_strdup function will be used
// * \param str A string to duplicate
// * \return A copy of \c str
// * */
//static inline char* lc_strdup(const char* str) { return strdup(str);} //This is needed when the function pointer will be used
///**
// * Wrapper for the realloc function when no memory tracing is active. This is needed when a pointer
// * to the lc_realloc function will be used
// * \param src Source pointer
// * \param size Size of the memory area to allocate
// * \return A pointer on the allocated area
// * */
//static inline void* lc_realloc(void* src, unsigned long size) { return realloc(src,size);} //This is needed when the function pointer will be used
//
#define lc_free(X) free(X)              /**<Wrapper for the free function when no memory tracing is active*/
//#define lc_malloc(X) malloc(X)          /**<Wrapper for the malloc function when no memory tracing is active*/
//#define lc_strdup(X) strdup(X)          /**<Wrapper for the strdup function when no memory tracing is active*/
//#define lc_realloc(X,Y) realloc(X,Y)    /**<Wrapper for the realloc function when no memory tracing is active*/

/**
 * Allocs and puts an area content to 0
 * \param size Size of memory area to allocate
 * \return A pointer on an allocated area
 */
extern void* lc_malloc(unsigned long size);
extern void* lc_malloc0(unsigned long size);
extern void* lc_calloc(unsigned long nmemb, unsigned long size);
extern char* lc_strdup(const char* str);
extern void* lc_realloc(void* src, unsigned long size);

#else //Mode MEMORY_TRACE_SIZE
/**
 * Prints memory usage
 */
extern void mem_usage ();

/**
 * Frees a memory area allocated with lc_malloc or lc_malloc0 or lc_strdup
 * \param ptr A pointer on a memory area to free
 */
extern void lc_free (void *ptr);

/**
 * Custom malloc
 * \param size Size of memory area to allocate
 * \return A pointer on an allocated area
 */
extern void *lc_malloc (unsigned long size);

/**
 * Allocs and puts an area content to 0
 * \param size Size of memory area to allocate
 * \return A pointer on an allocated area
 */
extern void *lc_malloc0 (unsigned long size);

extern void* lc_calloc(unsigned long nmemb, unsigned long size);

/**
 * Custom strdup using lc_malloc
 * \param str A string to duplicate
 * \return A copy of \c str
 */
extern char *lc_strdup (const char *str);

/**
 * Custom realloc, using lc_malloc
 * \param src Source pointer
 * \param size Size of the memory area to allocate
 * \return A pointer on the allocated area
 */
extern void* lc_realloc (void* src, unsigned long size);
#endif
#else //Mode MEMORY_TRACE_USE
/**
 * Prints memory usage
 */
extern void mem_usage ();

extern void __lc_free_memtrace(void *ptr, const char* src, int l);
extern void *__lc_malloc_memtrace(unsigned long size, const char* src, int l);
extern void *__lc_malloc0_memtrace(unsigned long size, const char* src, int l);
extern char *__lc_strdup_memtrace(const char *str, const char* src, int l);
extern void* __lc_realloc_memtrace (void* ptr, unsigned long size, const char* src, int l);

static inline void lc_free(void* ptr) {__lc_free_memtrace(ptr,"***ptrcall***", 0);} //This is needed when the function pointer will be used
static inline void* lc_malloc(unsigned long size) {return __lc_malloc_memtrace(size,"***ptrcall***", 0);} //This is needed when the function pointer will be used
static inline void* lc_malloc0(unsigned long size) {return __lc_malloc0_memtrace(size,"***ptrcall***", 0);} //This is needed when the function pointer will be used
static inline void* lc_strdup(const char* str) {return __lc_strdup_memtrace(str,"***ptrcall***", 0);} //This is needed when the function pointer will be used
static inline void* lc_realloc(void* src, unsigned long size) {return __lc_realloc_memtrace(src, size,"***ptrcall***", 0);} //This is needed when the function pointer will be used

#define lc_free(X) __lc_free_memtrace(X, __FUNCTION__,__LINE__)
#define lc_malloc(X) __lc_malloc_memtrace(X, __FUNCTION__,__LINE__)
#define lc_malloc0(X) __lc_malloc0_memtrace(X, __FUNCTION__,__LINE__)
#define lc_strdup(X) __lc_strdup_memtrace(X, __FUNCTION__,__LINE__)
#define lc_realloc(X,Y) __lc_realloc_memtrace(X, Y, __FUNCTION__,__LINE__)
#endif
#else //Mode MEMORY_LEAK_DEBUG
/**
 * Prints memory usage
 */
extern void mem_usage ();

extern void lc_free (void *ptr);
extern void *lc_malloc (unsigned long size,char* id);
extern void *lc_malloc0 (unsigned long size, char* id);
extern char *lc_strdup (const char *str, char * id );
#endif

///////////////////////////////////////////////////////////////////////////////
//                        Windows specific functions                         //
///////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define strtok __strtok
#define getenv __getenv

#if _MSC_VER < 1900
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

extern char * __strtok(char * str, const char * delimiters);

extern char * __getenv(const char * name);

extern char * strndup(const char * s, size_t size);

#endif

///////////////////////////////////////////////////////////////////////////////
//                       platform specific functions                         //
///////////////////////////////////////////////////////////////////////////////

/*
 * Retrieves the name of a file from its full path
 * \note The user needs to free the returned value
 * \param p Path of the file
 * \return The name of the file
 */
extern char* lc_basename(const char* path);

/*
 * Retrieves the name of the directory from a full path
 * \note The user needs to free the returned value
 * \param p Full path
 * \return The name of the directory
 */
extern char* lc_dirname(const char* path);

extern char * lc_strncpy(char * destination, size_t size, const char * source,
      size_t num);

extern int lc_sprintf(char * str, size_t size, const char * format, ...);

///////////////////////////////////////////////////////////////////////////////
//                          base 64  functions                               //
///////////////////////////////////////////////////////////////////////////////
/**
 * Parses a string to find a value (in decimal or hexadecimal format)
 * \param base64_str The string to decode
 * \param targetstr_size Size of the string to decode.
 * \return A string containing the decoded version of base64_str
 */
char* decode(char *base64_str, int targetstr_size);

///////////////////////////////////////////////////////////////////////////////
//                            string functions                               //
///////////////////////////////////////////////////////////////////////////////
/**
 * Parses a string to find a value (in decimal or hexadecimal format)
 * \param strinsn The string to parse
 * \param pos Pointer to the index value in the string at which to start the search.
 * Will be updated to point to the character immediately after the value
 * \param value Return parameter, will point to the parsed value
 * \return EXIT_SUCCESS if the parsing succeeded, EXIT_FAILURE otherwise
 * \todo Remove the use of pos and only use the string pointer (update also oprnd_parsenew and insn_parsenew)
 * */
extern int parse_number__(char* strinsn, int* pos, int64_t* value);

/**
 * Appends two 0-terminated character strings to each others
 * \param text The string to append a suffix to (is freed at the completion of the function)
 * \param textlen The number of characters in \e text after which the suffix has to be appended
 * (-1 if the whole string must be used).
 * \param suffix The string to append to \e text
 * \param suffixlen The number of characters in \e suffix to append to \e text (-1 if the whole
 * string has to be appended)
 * \return A 0-terminated string containing the \e n first characters of \e text
 * (or all if \e n is -1), followed by the 0 character then the first \e m characters of \e suffix
 */
extern char* str_append(char* text, int textlen, const char* suffix,
      int suffixlen);

/**
 * Concatenate two strings. The result can be released with lc_free
 * Both original strings are not destroyed after the operation
 * \param str1 a string
 * \param str2 a string
 * \return a new string equal to str1+str2
 */
extern char* str_concat(const char* str1, const char* str2);

/**
 * Search if \a txt contains \a exp
 * \param txt a string to examine
 * \param exp a regular expression
 * \return 1 if \a txt contains exp else 0
 */
extern int str_contain(const char* txt, const char* exp);

/**
 * Search if \a txt contains \a exp and collects all the subpatterns (use parenthesis for subpattern)           
 * \param txt a string to examine                                               
 * \param exp a regular expression                                              
 * \param ptr_str_matched If \txt contains \exp, then it is filled with the results of search. 
 *                        str_matched[0] will contain the text that matched the full pattern, 
 *                        str_matched[1] will have the text that matched the first captured parenthesized subpattern, 
 *                        and so on.                                            
 * \return The number of subpatterns if \a txt contains exp.                    
 *         0  No match.                                                         
 *         -1 A problem occurred.                                               
 */
extern int str_match(const char* txt, const char* exp, char*** ptr_str_matched);

/**
 * Compares two string without taking the cases into account
 * \param str1 First string
 * \param str2 Second string
 * \return TRUE if both string are identical, not taking the case into account, FALSE otherwise
 * */
extern int str_equal_nocase(const char* str1, const char* str2);

/**
 * Utility function for strings. Counts the number of substring delimited by \e delim
 * \param chaine The string to search
 * \param delim A delimiter
 * \return  the number of substring delimited by \e delim
 */
extern int str_count_field(const char* chaine, char delim);

/**
 * Get the \e numarg field of a string, delimited by delim
 * \param chaine A string
 * \param numarg Index of the field to return
 * \param delim A delimiter
 * \return A string with the field, else NULL
 */
extern char* str_field(char* chaine, int numarg, char delim);

/**
 * Returns a string of length l filled with the character c
 * \param c A character used to fill the string
 * \param l Length of the new string
 * \return A string filled with c
 */
extern char* str_fill(char c, int l);

/**
 * Free a string
 * \param p A pointer on a string to free
 */
extern void str_free(void* p);

/**
 * Create a new string of size \e size
 * \param size Number of character of the string
 * \return An initialized string
 */
extern char* str_new(unsigned int size);

/**
 * Replace a substring \e pattern by a substring \e replacement in a string \e string
 * \param string original string
 * \param pattern substring to replace
 * \param replacement substring which replace pattern
 * \return a new string
 */
extern char* str_replace(const char* string, const char* pattern,
      const char* replacement);

/**
 * Replaces all occurrences of a character in a string by another
 * \param chaine The string to update
 * \param from The character to replace
 * \param to The character to set in place of \e from
 */
extern void str_replace_char(char* chaine, char from, char to);

/**
 * Replaces all non standard ASCII characters (ie everything not A-Z ,a-z, 0-9 or '_')
 * by the character '_' in a string
 * \param chaine a string
 */
extern void str_replace_char_non_c(char* chaine);

/**
 * Converts a string to lower case characters. The result can be released with lc_free
 * The original string is not changed
 * \param str A string to convert
 * \return A new string with lower case characters
 */
extern char* str_tolower(const char* str);

/**
 * Converts a string to upper case characters. The result can be released with lc_free
 * The original string is not changed
 * \param str A string to convert
 * \return A new string with upper case characters
 */
extern char* str_toupper(const char* str);

/**
 * Copies the content of str2 to str1 converted to upper characters.
 * Copies up to and including the NULL character of str2.
 * \param str1 String to copy to. Must be already allocated
 * \param str2 String to copy
 * \return str1
 * */
extern char* strcpy_toupper(char* str1, const char* str2);

/**
 * Copies the content of str2 to str1 converted to lower characters.
 * Copies up to and including the NULL character of str2.
 * \param str1 String to copy to. Must be already allocated
 * \param str2 String to copy
 * \return str1
 * */
extern char* strcpy_tolower(char* str1, const char* str2);

/**
 * Compare two strings containing version number matching "[0-9]+(.[0-9]+)*"
 * \param v1
 * \param v2
 * \return a negative value is v1 > v2, 0 is v1 == v2 and a positive value if v1 < v2
 */
extern int str_compare_version(const char* v1, const char* v2);

///////////////////////////////////////////////////////////////////////////////
//                             time functions                                //
///////////////////////////////////////////////////////////////////////////////
/**
 * Get the user time
 * \return time in micro secondes
 */
extern unsigned long long int utime(void);

/**
 * Generates a timestamp formatted as (year)-(month)-(day)_(hour)-(minutes)-(seconds)_(microseconds)
 * If the microseconds are not available they will be omitted
 * \param str String containing the result. Must have already been initialised
 * \param str_len Size of the string
 * \return EXIT_SUCCESS/EXIT_FAILURE
 * */
extern int generate_timestamp(char* str, size_t str_len);

///////////////////////////////////////////////////////////////////////////////
//                             hash functions                                //
///////////////////////////////////////////////////////////////////////////////
/**
 * Hashes a string
 * \param h an original value
 * \param c a string to hash
 * \return the hash value of the string
 */
extern unsigned long add_hash(unsigned long h, char *c);

/**
 * Hashes an file
 * \param filename a file to hash
 * \return the hash value of a file
 */
extern unsigned long file_hash(char *filename);

///////////////////////////////////////////////////////////////////////////////
//                                  lists                                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * \struct list_s
 * \brief Structure holding a list of elements
 * */
struct list_s {
   void *data; /**<Pointer to the actual data contained in the list*/
   struct list_s *next; /**<Next list node*/
   struct list_s *prev; /**<Previous list node*/
};

/**
 * Macro used to traverse a list
 * X: a list
 * Y: name of an iterator (not alredy used)
 */
#define FOREACH_INLIST(X,Y)\
    list_t *Y;for(Y=(X); Y!=NULL; Y=Y->next)

/**
 * Macro used to traverse a list in reverse order
 * X: a list
 * Y: name of an iterator (not alredy used)
 */
#define FOREACH_INLIST_REVERT(X,Y)\
    list_t *Y; Y=(X); while(Y!=NULL && Y->next)Y=Y->next; for(Y=Y;Y!=NULL; Y=Y->prev)

/**
 * Get data when used with FOREACH macros
 * X: dst variable
 * Y: iterator
 * ex: var =  GET_DATA(var,it)
 */
#define  GET_DATA(Y) ((Y->data))

#define  GET_DATA_T(X,Y) ((X)(Y->data))

/**
 * Adds an element after a given element
 * \param list An element after whom the new element must be added
 * \param data An element to add
 * \return The modified list
 */
extern list_t* list_add_after(list_t *list, void *data);

/**
 * Adds an element at the beginning of a list
 * \param list A list
 * \param data Element to add
 * \return The list updated
 */
extern list_t* list_add_before(list_t *list, void *data);

/**
 * Cuts after the first cell in \e orig containing \e data
 * \param orig A list
 * \param data An element
 * \return The end of \e orig, else NULL if \e data not found
 */
extern list_t* list_cut_after(list_t* orig, void * data);

/**
 * Cuts before the first cell in \e orig containing \e data
 * \param orig a list
 * \param end the end of the list (after cell in \e orig containing data) or NULL if data not found
 * \param data an element
 * \return the beginning of \e orig, else NULL if \e data not found
 */
extern list_t* list_cut_before(list_t* orig, list_t** end, void * data);

/**
 * Duplicates a list
 * \param list A list
 * \return A list containing the same elements as the original
 */
extern list_t* list_dup(list_t *list);

/**
 * Executes a function \e f on every element in a list
 * \warning A macro described in libmcommon.c can also be used
 * \param list A list
 * \param f A function executed of each element. It must have the following prototype:
 *              void \<my_function\> (void* elem, void* user); where:
 *                      - elem is the element
 *                      - user a 2nd parameter given by the user
 * \param user A parameter given by the user for the function to execute
 */
extern void list_foreach(list_t *list, void (*f)(void *, void *), void *user);

/**
 * Frees a list and its elements if \e f not null
 * \param list A list to free
 * \param f A function used to free an element, with the following prototype:
 *      void \<my_function\> (void* data); where:
 *                      - data is the element to free
 */
extern void list_free(list_t *list, void (*f)(void*));

/**
 * Gets the size (number of element) in a list
 * \param list a list
 * \return size of the list
 */
extern int list_length(list_t *list);

/**
 * Get the next element of a list
 * \param l a list
 * \return the next element or PTR_ERROR if there is a problem
 */
extern list_t* list_getnext(list_t* l);

/**
 * Get the previous element of a list
 * \param l a list
 * \return the previous element or PTR_ERROR if there is a problem
 */
extern list_t* list_getprev(list_t* l);

/**
 * Get the data in an element of a list
 * \param l a list
 * \return the data or PTR_ERROR if there is a problem
 */
extern void* list_getdata(list_t* l);

/**
 * Finds the first element in the list containing \e data
 * \param list a list
 * \param data The data to look for
 * \return The first list element containing \e data
 */
extern list_t* list_lookup(list_t *list, void *data);

/**
 * Creates a new list
 * \param data element to add in the list
 * \return a new list
 */
extern list_t* list_new(void *data);

/**
 * Finds, removes and frees (if \e f not NULL) the element of a list corresponding to \e data
 * \param list a list
 * \param data the element to remove
 * \param f a function used to free the element, with the following prototype:
 *      void \<my_function\> (void* data); where:
 *                      - data is the element to free
 * \return the list without the element
 */
extern list_t* list_remove(list_t* list, void * data, void (*f)(void*));

/**
 * Removes a list element and returns the corresponding data
 * Remark: if \e list is the head of a list, save the address
 * of the next element or use list_remove_head instead.
 * \param list A valid list element
 * \return corresponding data
 */
extern void* list_remove_elt(list_t *list);

/**
 * Removes and  returns the first element of a list
 * \param list a list
 * \return the head element
 */
extern void* list_remove_head(list_t **list);

///////////////////////////////////////////////////////////////////////////////
//                                 queues                                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * \struct queue_s
 * \brief Structure holding a list of elements. Compared to a list,
 *        a queue saves the last element, allowing the addition of an
 *        element at the end to be faster with a queue than with a list structure
 * */
struct queue_s {
   list_t *head; /**<The first list node of the queue*/
   list_t *tail; /**<The last list node of the queue*/
   unsigned int length; /**<size of the queue*/
};

/**
 * Macro used to traverse a queue
 * X: A queue
 * Y: Name of an iterator (not already used)
 */
#define FOREACH_INQUEUE(X,Y)\
    list_t *Y;for(Y=queue_iterator(X); Y!=NULL; Y=Y->next)

/**
 * Macro used to traverse a queue starting from the end
 * X: A queue
 * Y: Name of an iterator (not already used)
 */
#define FOREACH_INQUEUE_REVERSE(X,Y)\
        list_t* Y; for (Y=queue_iterator_rev (X); Y!= NULL; Y=Y->prev)

/**
 * Adds an element at the head of a queue
 * \param q A queue
 * \param data An element to add
 */
extern void queue_add_head(queue_t* q, void* data);

/**
 * Adds an element at the tail of a queue
 * \param q A queue
 * \param data An element to add
 */
extern void queue_add_tail(queue_t *q, void* data);

/**
 * Appends queue q2 to queue q1 and frees q2
 * \param q1 Destination queue
 * \param q2 A queue to add to q1. It is deleted at the end of the function
 */
extern void queue_append(queue_t* q1, queue_t* q2);

/**
 * Appends queue q2 to queue q1 but does not free any queue
 * \param q1 a queue
 * \param q2 a queue to add to q1
 */
extern void queue_append_and_keep(queue_t* q1, queue_t* q2);

/**
 * Appends a node to a queue
 * \param q The queue
 * \param n The node to append (will become the new tail of q). Nothing will be done if n is NULL
 * */
extern void queue_append_node(queue_t* q, list_t* n);

/**
 * Duplicates a queue
 * \param q The queue to duplicate
 * \return A new queue structure holding the same data pointers in the same order
 */
extern queue_t* queue_dup(queue_t* q);

/**
 * Checks if two queues contains the same elements (not necessarily in the same order)
 * \param v1 a queue
 * \param v2 a queue
 * \return 1 if the two queues contain the same elements, 0 otherwise
 * */
extern int queue_equal(const void* v1, const void* v2);

/**
 * Extracts part of a queue following the given item (including or not this item)
 * \param q The queue to extract a part from
 * \param pos The list element after which extraction must take place
 * \param include 1 if \e pos must be included to the returned queue, 0 otherwise
 * \return The extracted queue
 */
extern queue_t* queue_extract_after(queue_t* q, list_t* pos, int include);

/**
 * Extract the node containing a given data from a queue
 * \param q The queue to extract from
 * \param data The data whose node we want to extract
 * \return The node containing the data or NULL if q is NULL or data not present in q
 * */
extern list_t* queue_extract_node(queue_t* q, void* data);

/**
 * Frees all elements in a queue and sets its length to 0
 * \param q a queue
 * \param f a function used to free an element, with the following prototype:
 *      void \<my_function\> (void* data); where:
 *                      - data is the element to free
 */
extern void queue_flush(queue_t *q, void (*f)(void*));

/**
 * Runs a function on every element in a queue
 * \param q a queue
 * \param f a function executed of each element. It must have the following prototype:
 *              void \<my_function\> (void* elem, void* user); where:
 *                      - elem is the element
 *                      - user a 2nd parameter given by the user
 * \param user a parameter given by the user for the function to execute
 * */
extern void queue_foreach(queue_t *q, void (*f)(void *, void *), void *user);

/**
 * Frees a queue and its elements if \e f is not null
 * \param q a queue
 * \param f a function used to free an element, with the following prototype:
 *      void \<my_function\> (void* data); where:
 *                      - data is the element to free
 */
extern void queue_free(queue_t *q, void (*f)(void*));

/**
 * Inserts the contents of a queue into another at a specified element
 * \param queue The queue to update
 * \param ins The queue to insert
 * \param elt The element in \e queue at which \e ins must be inserted
 * \param after Must be 1 if \e ins must be inserted after \e elt, 0 if it must be inserted before
 * \warning The inserted queue is incorporated into the insertee, so ins is freed
 * */
extern void queue_insert(queue_t* queue, queue_t* ins, list_t* elt, int after);


/**
 * Inserts the contents of a queue into another at a specified element
 * \param queue The queue to update
 * \param ins The queue to insert
 * \param elt The element in \e queue at which \e ins must be inserted
 * \param after Must be 1 if \e ins must be inserted after \e elt, 0 if it must be inserted before
 * \warning Neither queue is freed
 * */
extern void queue_insert_and_keep(queue_t* queue, queue_t* ins, list_t* elt, int after);
/**
 * Adds an element data in queue q before an element
 * \param q The queue to update
 * \param elt The element before which we want to insert the new data
 * \param data The data for the new element
 */
extern void queue_insertbefore(queue_t* q, list_t* elt, void* data);

/**
 * Adds an element data in queue q after an element
 * \param q The queue to update.
 * \param elt The element after which we want to insert the new data.
 * \param data The data for the new element.
 */
extern void queue_insertafter(queue_t* q, list_t* elt, void* data);

/**
 * Checks if a queue is empty
 * \param q a queue
 * \return 1 if \e q is empty, else 0
 */
extern int queue_is_empty(queue_t *q);

/**
 * Returns an iterator on the first element of the queue
 * \param q a queue
 * \return an iterator (list_t pointer)
 */
extern list_t* queue_iterator(queue_t *q);

/**
 * Returns an iterator for the reverse iteration of the queue (its tail)
 * \param q a queue
 * \return an iterator (list_t pointer)
 */
extern list_t* queue_iterator_rev(queue_t* q);

/**
 * Returns the length of a queue
 * \param q a queue
 * \return \e q length
 */
extern int queue_length(queue_t *q);

/**
 * Scans a queue looking for an element and return the first occurrence while using a custom
 * equal function for looking for the occurrence.
 * \param q a queue
 * \param f a function to compare two elements. It must have the following prototype:
 *              int \<my_function\> (const void* e1, const void* e2); where:
 *                      - e1 is an element
 *                      - e2 is an element
 *                      - f return true (nonzero) if its two parameters are equal, else 0
 * \param data an element to look for
 * \return the element if found, else NULL
 */
extern void* queue_lookup(queue_t* q, int (*f)(const void *, const void *),
      void *data);

/**
 * Scans a queue looking for an entry and returns the first occurrence
 * \param q The queue to search into
 * \param data The value of the element to search for
 * \return The first encountered list_t object containing \e data
 */
extern list_t* queue_lstlookup(queue_t* q, void* data);

/**
 * Creates and initializes a new queue
 * \return a new queue
 */
extern queue_t* queue_new();

/**
 * Returns the data contained by the first node in the queue
 * \param q a queue
 * \return the first element of a queue, else NULL
 */
extern void *queue_peek_head(queue_t *q);

/**
 * Returns the data contained by the last node in the queue
 * \param q a queue
 * \return the last element of a queue, else NULL
 */
extern void* queue_peek_tail(queue_t *q);

/**
 * Prepends queue q2 to queue q1 and frees it
 * \param q1 a queue
 * \param q2 a queue to add before q1. It is deleted at the end of the function
 */
extern void queue_prepend(queue_t* q1, queue_t* q2);

/**
 * Prepends queue q2 to queue q1 but does not free any queue
 * \param q1 a queue
 * \param q2 a queue to add before q1
 */
extern void queue_prepend_and_keep(queue_t* q1, queue_t* q2);

/**
 * Removes and returns the head of a queue (does not free the element)
 * \param q a queue
 * \return the element at the head of a queue
 */
extern void* queue_remove_head(queue_t *q);

/**
 * Finds and removes the first element element in a queue whose data value equals the one looked for,
 * and frees it if \e f is not null
 * \param q a queue
 * \param data an element to remove
 * \param f a function used to free an element, with the following prototype:
 *      void \<my_function\> (void* data); where:
 *                      - data is the element to free
 */
extern void queue_remove(queue_t* q, void* data, void (*f)(void*));

/**
 * Removes a given list element from a queue
 * \param q The queue
 * \param elt A node of the queue. It must belong to the queue (no test will be performed)
 * \return The data contained by the removed element
 * */
extern void* queue_remove_elt(queue_t* q, list_t* elt);

/**
 * Removes and returns the tail of a queue (does not free the element)
 * \param q a queue
 * \return the element at the tail of a queue
 */
extern void* queue_remove_tail(queue_t *q);

/**
 * Extracts a subqueue from the queue, starting at and including element start
 * and ending at and including element end, and replaces it with the queue given
 * in parameter. The subqueue extracted is returned into the replace parameter
 * \param q The queue to update
 * \param start The first element in the queue to be swapped
 * \param end The last element in the queue to be swapped
 * \param replace The queue to swap with elements from \e q. Contains the swapped elements
 * from \e q at the end of the operation
 */
extern void queue_swap(queue_t* q, void* start, void* end, queue_t* replace);

/**
 * Extracts a subqueue from the queue, starting at and including element start
 * and ending at and including element end, and replaces it with the queue given
 * in parameter. The subqueue extracted is returned into the replace parameter
 * \param queue The queue to update
 * \param startl The first element in the queue to be swapped. It is assumed to belong to \c queue and won't be checked
 * \param endl The last element in the queue to be swapped. It is assumed to belong to \c queue and won't be checked
 * \param replace The queue to swap with elements from \e queue. Contains the swapped elements
 * from \e queue at the end of the operation
 * \param len Number of elements between startl and endl
 * */
extern void queue_swap_elts(queue_t* queue, list_t* startl, list_t* endl,
      queue_t* replace);

/**
 * Sorts a queue using stdlib/qsort, according to a comparison function
 * To sort a list of \c item_t*, your function must be similar to the following:
 * \code
 * int compare_items (const void* p1, const void* p2) {
 *     const item_t *i1 = *((void **) p1);
 *     const item_t *i2 = *((void **) p2);
 *     // Here normal compare...
 * }
 * \endcode
 * Please also be aware that in the case of structures keeping track of their
 * iterator, for example \c insn_t with its \c sequence attribute, you will
 * need to update them, because they will \b not be correct.
 * \param queue a queue
 * \param compar comparison function (between data fields of two elements)
 */
extern void queue_sort(queue_t *queue,
      int (*compar)(const void *, const void *));

///////////////////////////////////////////////////////////////////////////////
//                               bitvectors                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * A bitvector is an array of bits presented in reverse order: bn-1 bn-2 ... b1 b0
 *
 * What you can do with a bitvector is similar to what you can do with a string:
 *  - copy elements, get or set an individual element...
 *
 * If bv is an n-bits bitvector:
 *  - BITVECTOR_GET_BITLENGTH (bv) returns n
 *  - BITVECTOR_GET_BIT  (bv, i) returns bi     (the i-th bit from the right)
 *  - BITVECTOR_GET_LBIT (bv, i) returns bn-1-i (the i-th bit from the left)
 *
 * It is physically stored as an array of integers called chunks, minimizing
 * memory footprint: one bit in the bitvector occupies one bit in memory.
 */

/**
 * \enum bitvector_endianness_t
 * Constants defining how a bitvector is stored
 */
typedef enum {
   BIG_ENDIAN_BIT = 0, /**<most significant bit  first (11010111 01001000 is 0xD748)*/
   BIG_ENDIAN_BYTE, /**<most significant byte first (11010111 01001000 is 0xD748)*/
   LITTLE_ENDIAN_BIT, /**<less signifivant bit  first (11010111 01001000 is 0x12EB)*/
   LITTLE_ENDIAN_BYTE /**<less significant byte first (11010111 01001000 is 0x48D7)*/
} bitvector_endianness_t;

/**Possible code's endianness*/
typedef enum code_endianness_e {
   CODE_ENDIAN_LITTLE_INFINITE = 0,
   CODE_ENDIAN_LITTLE_16B,
   CODE_ENDIAN_LITTLE_32B,
   CODE_ENDIAN_BIG_INFINITE,
   CODE_ENDIAN_BIG_16B,
   CODE_ENDIAN_BIG_32B
} code_endianness_t;

/**Type used to store bits in \ref bitvector_t structures*/
typedef uint32_t bitvector_chunk_t; /* uintX_t with X=8,16,32,64 */

/**
 * \struct bitvector_s
 * \brief Structure storing a vector of bits of variable size
 */
struct bitvector_s {
   unsigned bits; /**<total number of bits*/
   bitvector_chunk_t *vector; /**<Array containing the bits, grouped as bitvector_chunk_t integers*/
};

/**
 * Returns the size in bits of a bitvector
 * \param X Pointer to the bitvector
 */
#define BITVECTOR_GET_BITLENGTH(X) (((X) != NULL) ? (X)->bits : 0)

/**
 * Returns the size in bytes of a bitvector. This is the minimal number of bytes needed to represent all of its bytes
 * \param X Pointer to the bitvector
 */
#define BITVECTOR_GET_BYTELENGTH(X) (((X) != NULL) ? (((X)->bits + 7) >> 3) : 0)

/* Used to simplify expression of *_BITVECTOR_BIT macros */
/**Number of bits contained in a cell of the array*/
#define BITVECTOR_CHUNK_SIZE (8 * sizeof (bitvector_chunk_t))
/**
 * Returns the cell containing a bit
 * \param X A bitvector
 * \param B A bit index*/
#define BITVECTOR_BIT_CHUNK(X, B) ((X)->vector [(B) / BITVECTOR_CHUNK_SIZE])
/**
 * Returns the index of a bit in the cell containing it
 * \param X A bitvector
 * \param B A bit index
 * */
#define BITVECTOR_BIT_MASK( X, B) (1 <<        ((B) % BITVECTOR_CHUNK_SIZE))

/**
 * Gets individual bit B in vector X.
 * \param X Pointer to the bitvector
 * \param B Index of the bit to get, starting from the right of the bitvector (right-most bit has index 0)
 */
#define BITVECTOR_GET_BIT(X, B) ((BITVECTOR_BIT_CHUNK(X,B) & BITVECTOR_BIT_MASK(X,B)) ? 1 : 0)

/**
 * Sets individual bit B in vector X.
 * \param X Pointer to the bitvector
 * \param B Index of the bit to set, starting from the right of the bitvector (right-most bit has index 0)
 */
#define BITVECTOR_SET_BIT(X, B) (BITVECTOR_BIT_CHUNK(X,B) |= BITVECTOR_BIT_MASK(X,B))

/**
 * Clears individual bit B in vector X (set to 0).
 * \param X Pointer to the bitvector
 * \param B Index of the bit to clear, starting from the right of the bitvector (right-most bit has index 0)
 */
#define BITVECTOR_CLR_BIT(X, B) (BITVECTOR_BIT_CHUNK(X,B) &= ~BITVECTOR_BIT_MASK(X,B))

/**
 * Inverts individual bit B in vector X.
 * \param X Pointer to the bitvector
 * \param B Index of the bit to invert, starting from the right of the bitvector (right-most bit has index 0)
 */
#define BITVECTOR_INV_BIT(X, B) (BITVECTOR_BIT_CHUNK(X,B) ^= BITVECTOR_BIT_MASK(X,B))

/**
 * Gets individual bit B in vector X, starting from the left.
 * \param X Pointer to the bitvector
 * \param B Index of the bit to get, starting from the left of the bitvector (left-most bit has index 0)
 */
#define BITVECTOR_GET_LBIT(X, B) BITVECTOR_GET_BIT(X, (X)->bits - 1 - (B))

/**
 * Sets individual bit B in vector X, starting from the left.
 * \param X Pointer to the bitvector
 * \param B Index of the bit to set, starting from the left of the bitvector (left-most bit has index 0)
 */
#define BITVECTOR_SET_LBIT(X, B) BITVECTOR_SET_BIT(X, (X)->bits - 1 - (B))

/**
 * Clears individual bit B in vector X, starting from the left.
 * \param X Pointer to the bitvector
 * \param B Index of the bit to clear, starting from the left of the bitvector (left-most bit has index 0)
 */
#define BITVECTOR_CLR_LBIT(X, B) BITVECTOR_CLR_BIT(X, (X)->bits - 1 - (B))

/**
 * Inverts individual bit B in vector X.
 * \param X Pointer to the bitvector
 * \param B Index of the bit to invert, starting from the right of the bitvector (right-most bit has index 0)
 */
#define BITVECTOR_INV_LBIT(X, B) BITVECTOR_INV_BIT(X, (X)->bits - 1 - (B))

/**
 * Performs a bit-to-bit logical AND operation on two bitvectors \e a and \e b
 * In other words, res <= a & b
 * \param res resulting bitvector, assumed already initialized
 * \param a left source bitvector
 * \param b right source bitvector
 */
extern void bitvector_and(bitvector_t *res, const bitvector_t *a,
      const bitvector_t *b);

/**
 * Appends a bitvector \e right to another bitvector \e left
 * In other words, left <= left .. right
 * \param left left bitvector, that will be larger at the end of the operation
 * \param right right bitvector
 */
extern void bitvector_append(bitvector_t* left, const bitvector_t* right);

/**
 * Appends bits at the right of a bitvector, setting them to the specified value,
 * depending on endianness (for the bitvector).
 * \param bv bitvector to update
 * \param val value to append
 * \param len number of bits to append (if val is longer, only its \e len least significant bits will be appended)
 * \param endianness bitvector endianness (bitvector_endianness_t)
 * \warning This function does not allow to append values larger than 64bits (use bitvector_append for that)
 */
extern void bitvector_appendvalue(bitvector_t* bv, int64_t val, size_t len,
      bitvector_endianness_t endianness);

/**
 * Prints a bitvector (in binary form) to an already allocated string
 * \param bv bitvector to print
 * \param out destination string
 * \param size size of the destination string
 */
extern void bitvector_binprint(const bitvector_t *bv, char *out, size_t size);

/**
 * Returns the value of a bitvector, cast into a byte stream (NOT null-terminated)
 * and the size of the resulting stream in len
 * \param bv a bitvector
 * \param blen will contain the size of the result stream
 * \param endianness code's endianness of the instruction's architecture
 * \return bitvector value, cast into a byte stream (NOT null-terminated)
 */
extern unsigned char * bitvector_charvalue(const bitvector_t *bv, int *blen,
      code_endianness_t endianness);

/**
 * Prints the value of a bitvector inside a byte string (NOT null-terminated)
 * The length of the string must be at least (bf->bits + 7) >> 3
 * \param bv A bitvector
 * \param str An allocated string. It must be at least as large as BITVECTOR_GET_BYTELENGTH(bv)
 * \param endianness code's endianness of the instruction's architecture
 * \return The actual number of bytes printed
 * */
extern unsigned bitvector_printbytes(const bitvector_t* bv, unsigned char* str,
      code_endianness_t endianness);

/**
 * Clears a bitvector (sets all bits to 0)
 * \param bv a bitvector
 */
extern void bitvector_clear(bitvector_t *bv);

/**
 * Copies a bitvector \e src to another already allocated bitvector \e dst
 * If not enough chunks in \e dst, copy exits with leaving \e dst unchanged
 * \param src source bitvector
 * \param dst destination bitvector
 */
extern void bitvector_copy(const bitvector_t *src, bitvector_t *dst);

/**
 * Removes the leftmost \e len bits in bitvector \e bv, and returns them as bitvector
 * \param bv a bitvector, that will be shorter at the end of the operation
 * \param len number of bits to remove
 * \return new bitvector containing leftmost \e len bits of \e bv
 */
extern bitvector_t * bitvector_cutleft(bitvector_t *bv, int len);

/**
 * Removes the rightmost \e len bits in bitvector \e bv, and returns them as bitvector
 * \param bv a bitvector, that will be shorter at the end of the operation
 * \param len number of bits to remove
 * \return new bitvector containing rightmost \e len bits of \e bv
 */
extern bitvector_t* bitvector_cutright(bitvector_t *bv, int len);

/**
 * Dumps the raw contents of a \e bv bitvector in the following format:
 * { V[0],V[1], ... V[<GET_CHUNKLENGTH(bv)>-1]} - Bitsize=<bv->bits> (no '\n')
 * with V[i] = bv->vector[i] in decimal format.
 * \note this function is designed for debugging
 * \param bv a bitvector to print
 * \param out destination file (already open)
 */
extern void bitvector_dump(const bitvector_t* bv, FILE* out);

/**
 * Returns a copy of a bitvector
 * \param src source bitvector
 * \return copy
 */
extern bitvector_t *bitvector_dup(const bitvector_t *src);

/**
 * Compares two bitvectors.
 * \param a a bitvector
 * \param b a bitvector
 * \return TRUE if they have strictly identical values, FALSE otherwise
 */
extern int bitvector_equal(const bitvector_t *a, const bitvector_t *b);

/**
 * Compares bitvector \e value against bitvector \e model. Bitvector \e msk allows to specify which bits must be compared.
 * In other words, returns bitvector_equal (value & msk, model)
 * \warning This function assumes that model->bits == msk->bits and value->bits >= model->bits
 * \param value bitvector to compare
 * \param model bitvector compared
 * \param msk bitvector used as a mask for \e value
 * \return TRUE if the comparison is true, else FALSE
 */
extern int bitvector_equalmask(const bitvector_t* value,
      const bitvector_t* model, const bitvector_t* msk);

/**
 * Compares bitvector \e value against bitvector \e model, from the left. Bitvector \e msk allows to specify which bits must be compared.
 * In other words, returns bitvector_equal (value & msk, model) considering leftmost bits
 * \warning This function assumes that model->bits == msk->bits and value->bits >= model->bits
 * \param value bitvector to compare
 * \param model bitvector compared
 * \param msk bitvector used as a mask for \e value
 * \return TRUE if the comparison is true, else FALSE
 */
extern int bitvector_equalmaskleft(const bitvector_t* value,
      const bitvector_t* model, const bitvector_t* msk);
/**
 * Extracts the part of a bitvector \e src into another bitvector \e dst,
 * starting at the bit of index \e offset. The number of extracted bits equals dst->bits
 * \param src source bitvector, whose length will decrease
 * \param dst destination bitvector
 * \param offset offset in \e src
 */
extern void bitvector_extract(bitvector_t *src, bitvector_t *dst, size_t offset);

/**
 * Fills a bitvector \e bv from an array of chunks
 * \note this function is designed for debugging
 * \param bv a bitvector, assumed already allocated
 * \param array array of integers to be converted to bits
 */
extern void bitvector_fill_from_chunks(bitvector_t *bv,
      const bitvector_chunk_t *array);

/**
 * Fills the \e len first (rightmost) bits of a bitvector \e bv from a value \e val
 * with a given endianness \e endianness
 * \param bv a bitvector
 * \param val int64_t value to write in \e bv with a given endianness
 * \param endianness \e bv endianness (bitvector_endianness_t)
 * \param len number of bits to fill
 */
extern void bitvector_fill_from_value(bitvector_t *bv, int64_t val,
      bitvector_endianness_t endianness, size_t len);

/**
 * Fills a bitvector from \e l characters in the string \e str
 * \warning If \e l is bigger than the number of bytes in \e str, the behavior
 * of the function is undefined
 * \param bv The bitvector to fill. It must be at least of size \e l * 8
 * \param c string to read from
 * \param l number of bytes to read from \e str
 * */
extern void bitvector_fill_from_str(bitvector_t* bv, const unsigned char *c,
      int l);

/**
 * Frees a bitvector structure
 * \param bv a bitvector
 */
extern void bitvector_free(bitvector_t *bv);

/**
 * Returns the value represented by the 64 first (rightmost) bits of a bitvector,
 * depending on the required endianness.
 * \param bv a bitvector
 * \param endianness \e bv endianness (bitvector_endianness_t)
 * \return represented value
 */
extern int64_t bitvector_fullvalue(const bitvector_t *bv,
      bitvector_endianness_t endianness);

/**
 * Prints a bitvector in hexadecimal format
 * \param bv bitvector to print
 * \param our destination string
 * \param size size of the destination string
 * \param sep string to print between each byte
 */
extern void bitvector_hexprint(const bitvector_t *bv, char *out, size_t size,
      char* sep);

/**
 * Inserts the content of a bitvector \e src into another bitvector \e dst,
 * starting at the bit of index \e offset.
 * \param src source bitvector
 * \param dst destination bitvector, whose length will increase
 * \param offset offset in \e dst
 */
extern void bitvector_insert(const bitvector_t *src, bitvector_t *dst,
      size_t offset);

/**
 * Returns value of \e len bits of a bitvector \e bv,
 * starting at position \e offset from the left of \e bv
 * \param bv a bitvector
 * \param len number of bits to use for value computation
 * \param offset offset in \e bv
 * \return a decimal value of the bitvector
 * \warning This function does not check if \c bv is not NULL and will seg fault if it is
 */
extern uint64_t bitvector_leftvalue(const bitvector_t *bv, size_t len,
      size_t offset);

/**
 * Returns true if the shortest bitvector matches with a subset of the largest
 * \param a a bitvector
 * \param b a bitvector
 * \return TRUE if the shortest bitvector matches with a subset of the largest, else FALSE
 */
extern int bitvector_match(const bitvector_t *a, const bitvector_t *b);

/**
 * Creates a \e len bits bitvector, with all bits set to 0
 * \param len bitvector length, in bits
 * \return new bitvector
 */
extern bitvector_t * bitvector_new(size_t len);

/**
 * Creates a bitvector from its string representation ("1100" for 0xC for instance)
 * \note This function is mainly used for testing purposes
 * \param str string which contains '0' and '1'
 * \return a new bitvector
 */
extern bitvector_t * bitvector_new_from_binstr(const char *str);

/**
 * Creates a bitvector from \e len characters in the string \e str
 * \warning If \e len is bigger than the number of bytes in \e str, the behavior
 * of the function is undefined
 * \param str string to read from
 * \param len number of bytes to read from \e str
 * \return A bitvector holding the binary value of the \e len first bytes of \e str
 */
extern bitvector_t * bitvector_new_from_str(const unsigned char *str, int len);

/**
 * Creates a \e len bits bitvector from a value \e val considering a given endianness \e endianness.
 * In other words, calls bitvector_fill_from_value on a new bitvector and returns it
 * \param val int64_t value to write in the new bitvector with a given endianness
 * \param endianness bitvector endianness (bitvector_endianness_t)
 * \param len new bitvector length
 * \return new bitvector
 */
extern bitvector_t *bitvector_new_from_value(int64_t val,
      bitvector_endianness_t endianness, size_t len);

/**
 * Creates a bitvector from a binary stream
 * \param start First byte of the bitvector
 * \param start_off Offset in the first byte where to start
 * \param stop Last byte (not included, unless offset is positive) of the bitvector
 * \param stop_off Offset in the last byte where to stop (number of bits to add)
 * \return A new bitvector containing the bits between start+start_off and stop+stop_off
 * */
extern bitvector_t* bitvector_new_from_stream(unsigned char* start,
      uint8_t start_off, unsigned char* stop, uint8_t stop_off);

/**
 * Performs a bit-to-bit logical NOT operation on a bitvector \e bv
 * In other words, res <= ~bv
 * \param res resulting bitvector, assumed already allocated
 * \param bv a bitvector
 */
extern void bitvector_not(bitvector_t *res, const bitvector_t *bv);

/**
 * Performs a bit-to-bit logical OR operation on two bitvectors \e a and \e b
 * In other words, res <= a | b
 * \param res resulting bitvector, assumed already allocated
 * \param a left source bitvector
 * \param b right source bitvector
 */
extern void bitvector_or(bitvector_t *res, const bitvector_t *a,
      const bitvector_t *b);

/**
 * Prepends a bitvector \e left to another bitvector \e right
 * In other words, right <= left .. right
 * \param left left bitvector
 * \param right right bitvector, that will be larger at the end of the operation
 */
extern void bitvector_prepend(const bitvector_t* left, bitvector_t* right);

/**
 * Prints a bitvector (in binary form)
 * \param bv bitvector to print
 * \param out destination file (already open)
 */
extern void bitvector_print(const bitvector_t *bv, FILE* out);

/**
 * Prints a bitvector_t declaration from a bit field written as a string (containing '0' and '1' characters)
 * \param bf The string representing the bitvector value
 * \param name The name of the bitvector (the vector will have the same name but with the "vect" prefix)
 * \param str String to write the declaration to (must already be initialised)
 * \param size Size of the destination string
 */
extern void bitvector_printdeclaration_from_binstring(char* bf, char* name,
      char* str, size_t size);

/**
 * Reads \e len bits from bitvector \e src starting at position \e offset
 * and copy them into bitvector \e dst
 * In other words, copies src [offset ; offset+len-1] into dst [0 ; len-1].
 * \note If offset = 0 and len = src->bits (not tested for performance reason), bitvector_copy is faster
 * \param src source bitvector
 * \param dst destination bitvector, assumed already allocated
 * \param offset index of the first bit to read in \e src
 * \param len number of bits to read
 */
extern void bitvector_read(const bitvector_t *src, bitvector_t *dst,
      size_t offset, size_t len);

/**
 * Removes \e len bits from bitvector \e bv, starting at position \e offset from the left.
 * \param bv a bitvector
 * \param offset index of the removed bit, from the left
 * \param len number of bits to remove
 * \return Nonzero if an error occurred (bits to remove out of range or too large)
 */
extern int bitvector_removebitsleft(bitvector_t* bv, int offset, int len);

/**
 * Resizes a bitvector. Memory is reallocated only if more chunks are needed
 * \param bv a bitvector
 * \param new_len new length in bits
 */
extern void bitvector_resize(bitvector_t *bv, size_t new_len);

/**
 * Sets a bitvector (set all bits to 1)
 * \param bv a bitvector
 */
extern void bitvector_set(bitvector_t *bv);

/**
 * Trims a bitvector, that is realloc the vector to fit the number of bits
 * \note Inside this function, it is impossible to know if this function will gain space
 * since the number of allocated chunks is not stored.
 * \param bv bitvector to trim
 */
extern void bitvector_trim(bitvector_t *bv);

/**
 * Returns value of the first (rightmost) \e len bits of a bitvector, up to 64
 * \param bv a bitvector
 * \param len number of bits to use for value computation
 * \return bitvector value
 * \warning This function does not check if \c bv is not NULL and will seg fault if it is
 */
extern uint64_t bitvector_value(const bitvector_t *bv, size_t len);

/**
 * Writes \e len bits into bitvector \e dst starting at position \e offset,
 * bits are copied from bitvector \e src
 * In other words, copies src [0 ; offset-1] into dst [offset ; offset+len-1].
 * If offset = 0 and len = src->bits (not tested for performance reason), bitvector_copy is faster
 * \param src source bitvector
 * \param dst destination bitvector, assumed already allocated
 * \param offset index of the first bit to write in \e dst
 * \param len number of bits to write
 */
extern void bitvector_write(const bitvector_t *src, bitvector_t *dst,
      size_t offset, size_t len);

/**
 * Performs a bit-to-bit logical XOR operation on two bitvectors \e a and \e b
 * In other words, res <= a ^ b
 * \param bit resulting bitvector, assumed already allocated
 * \param a left source bitvector
 * \param b right source bitvector
 */
extern void bitvector_xor(bitvector_t *bit, const bitvector_t *a,
      const bitvector_t *b);

///////////////////////////////////////////////////////////////////////////////
//                                  trees                                    //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct tree_s
 * \brief Structure holding antree node.
 * */
struct tree_s {
   void *data; /**<user data*/
   struct tree_s *next; /**<next node with the same parent*/
   struct tree_s *prev; /**<previous node with the same parent*/
   struct tree_s *parent; /**<parent of the node*/
   struct tree_s *children; /**<one child of the node*/
};

/**
 * A function executed on each tree node during a tree traverse
 * \param node current tree node
 * \param data a data specified by the user
 * \return 0
 * */
typedef int (*traversefunc_t)(struct tree_s *node, void *data);

/**
 * Gets the depth of a node
 * \param t a node
 * \return dept of \e t or -1 if there is a problem
 */
extern int tree_depth(tree_t *t);

/**
 * Deletes and frees (if f not null) a tree and all its children
 * \param tree a tree
 * \param f a function used to free an element, with the following prototype:
 *      void \<my_function\> (void* data); where:
 *                      - data is the element to free
 */
extern void tree_free(tree_t *tree, void (*f)(void*));

/**
 * Inserts create and edge from \e parent to \e node
 * \param parent a node
 * \param node a node
 * \return node
 */
extern tree_t* tree_insert(tree_t *parent, tree_t *node);

/**
 * Checks if \e node is an ancestor of \e descendant
 * \param node a node
 * \param descendant a node
 * \return 1 if \e node is an ancestor of \e descendant, else 0
 */
extern int tree_is_ancestor(tree_t *node, tree_t *descendant);

/**
 * Create a new tree
 * \param data element of the tree
 * \return a new tree
 */
extern tree_t* tree_new(void *data);

/**
 * Remove a given child from a tree
 * \param parent a root
 * \param node a direct child of the root to delete
 * \return the deleted node
 */
extern tree_t* tree_remove_child(tree_t *parent, tree_t *node);

/**
 * Traverses a tree and exectues a function of each node
 * \param node a tree
 * \param f a function exectued of each element. It must have the following prototype:
 *              void \<my_function\> (void* elem, void* data); where:
 *                      - elem is the element
 *                      - data a 2nd parameter given by the user
 * \param data a user data
 * \return
 */
extern int tree_traverse(tree_t *node, traversefunc_t f, void *data);

/**
 * Checks if a tree node has a parent
 * \param node a tree node
 * \return 1 if there is a parent, else 0
 */
extern int tree_hasparent(tree_t* node);

/**
 * Get the data in an element of a tree
 * \param t a tree
 * \return the data or PTR_ERROR if there is a problem
 */
extern void* tree_getdata(tree_t* t);

/**
 * Returns the parent node
 * \param node a tree node
 * \return parent node
 */
extern tree_t* tree_get_parent(tree_t* node);

/**
 * Returns the children node (in general, the first child)
 * \param node a tree node
 * \return children node
 */
extern tree_t* tree_get_children(tree_t* node);

///////////////////////////////////////////////////////////////////////////////
//                                 hashtables                                //
///////////////////////////////////////////////////////////////////////////////
#define HASH_INIT_SIZE       769    /**<Hashtable initial size*/
#define HASH_MAX_LOAD_FACTOR 2.0f   /**<Maximal number of element for a single
                                        hash value*/
typedef uint32_t hashtable_nnodes_t;
#define HASHTABLE_MAX_NNODES UINT32_MAX
typedef uint32_t hashtable_size_t;
#define HASHTABLE_MAX_SIZE UINT32_MAX

/**
 * A hash function for hashtable
 * \param key the key of the element
 * \param size number of slots in the targeted hashtable
 * \return a value between 0 and HASHSIZE
 * */
typedef hashtable_size_t (*hashfunc_t)(const void * key, hashtable_size_t size);

/**
 * An equal function for hashtable (to compare two elements a and b)
 * \param a First element to compare
 * \param b Second element to compare
 * \return 1 if keys are equal, else 0
 * */
typedef int (*equalfunc_t)(const void *a, const void *b);

/**
 * \struct hashnode_s
 * \brief Definition of a hashnode (internal structure)
 * */
struct hashnode_s {
   void *key; /**<Unique identifier to the object*/
   void *data; /**<Pointer to the object*/
   struct hashnode_s *next; /**<Next description*/
};

/**
 * \struct hashtable_s
 * \brief Structure for an hastable
 * */
struct hashtable_s {
   hashtable_nnodes_t nnodes; /**<Total number of nodes*/
   hashtable_size_t size; /**<Number of slots, size of the 'nodes' array*/
   boolean_t fixed_size; /**<Boolean true if size must be kept constant*/
   struct hashnode_s** nodes; /**<Array containing the queues of nodes, indexed by they key's hash*/
   hashfunc_t hash_func; /**<Pointer to the function to use to calculate hashes*/
   equalfunc_t key_equal_func; /**<Pointer to the function to use to check for key equality*/
};

/**
 * Macro used to traverse a hashtable
 * X: a hashtable
 * Y: name of an iterator (not already used)
 */
#define FOREACH_INHASHTABLE(X,Y)\
        hashnode_t *Y; hashtable_size_t __i;\
        for (__i = 0; __i < X->size; __i++)\
                for (Y = X->nodes[__i]; Y != NULL; Y = Y->next)

/**
 * Macro used to traverse hashtable elements matching a given key
 * T: a hashtable
 * K: a key
 * N: name of an iterator (not already used)
 */
#define FOREACH_AT_HASHTABLE_KEY(T,K,N)                                 \
   const hashtable_size_t __slot = (T)->hash_func ((K), (T)->size);     \
   hashnode_t *N;                                                       \
   for (N = (T)->nodes[__slot]; N != NULL; N = N->next)                 \
      if ((T)->key_equal_func ((K), N->key))

/**
 * Get the key associated to a hashtable iterator
 * X: hashtable iterator
 */
#define GET_KEY(X,Y) ((Y->key))

/**
 * An equalfunc_t function which uses '==' to compare keys
 * \param v1 First key
 * \param v2 Second key
 * \return 1 if keys are equals, else 0
 * */
extern int direct_equal(const void *v1, const void *v2);

/**
 * A hashfunc_t function hashing a pointer
 * \param key The key of the element
 * \param size number of slots in the targeted hashtable
 * \return Hash of the key (a value between 0 and size-1)
 * */
extern hashtable_size_t direct_hash(const void *key, hashtable_size_t size);

/**
 * A hashfunc_t function hashing an integer
 * \param key The key of the element
 * \param size number of slots in the targeted hashtable
 * \return Hash of the key (a value between 0 and size-1)
 * */
#define int_hash direct_hash

/**
 * An equalfunc_t function which uses '==' to compare keys
 * \param a First key
 * \param b Second key
 * \return 1 if keys are equal, else 0
 * */
#define int_equal direct_equal

/**
 * An equalfunc_t function comparing keys as pointers to 64 bits integers
 * \param v1 First key, pointer to an int64_t
 * \param v2 Second key, pointer to an int64_t
 * \return 1 if the keys are equal, 0 otherwise
 * */
extern int int64p_equal(const void *v1, const void *v2);

/**
 * A hashfunc_t function hashing a pointer to a 64 bits integer
 * \param v Key of the element, interpreted as a pointer to an int64_t
 * \param size Number of slots in the targeted hashtable
 * \return Hash of the key (a value between 0 and size-1)
 * */
extern hashtable_size_t int64p_hash(const void *v, hashtable_size_t size);

/**
 * An equalfunc_t function comparing keys as pointers to 32 bits integers
 * \param v1 First key, pointer to an int32_t
 * \param v2 Second key, pointer to an int32_t
 * \return 1 if the keys are equal, 0 otherwise
 * */
extern int int32p_equal(const void *v1, const void *v2);

/**
 * A hashfunc_t function hashing a pointer to a 32 bits integer
 * \param v Key of the element, interpreted as a pointer to an int32_t
 * \param size Number of slots in the targeted hashtable
 * \return Hash of the key (a value between 0 and size-1)
 * */
extern hashtable_size_t int32p_hash(const void *v, hashtable_size_t size);

/**
 * Copies (key,data) pairs from a source table into a destination table
 * \param dst destination table
 * \param src source table
 * */
extern void hashtable_copy (hashtable_t *dst, const hashtable_t *src);

/**
 * Empties the hashtable, but does not free it
 * \param t The hashtable
 * \param f Function for freeing a node's data
 * \param fk Function for freeing a node's key
 * */
extern void hashtable_flush(hashtable_t *t, void (*f)(void*), void (*fk)(void*));

/**
 * Traverse an hashtable and execute a function of each element
 * \param t a hashtable
 * \param f a function executed of each element. It must have the following prototype:
 *              void \<my_function\> (void* key, void* elem, void* user); where:
 *                      - key is the key of the element
 *                      - elem is the element
 *                      - user a 3rd parameter given by the user
 * \param user a parameter given by the user for the function to execute
 */
extern void hashtable_foreach(const hashtable_t *t, void (*f)(void *, void *, void *),
      void *user);

/**
 * Delete a hashtable
 * \param t a hashtable
 * \param f a function used to free each element if it is needed. It must have the following prototype:
 *              void \<my_function\> (void* elem); where:
 *                      - elem is an element to free
 * \param fk a function used to free each key if it is needed. It must have the following prototype:
 *              void \<my_function\> (void* elem); where:
 *                      - elem is a key to free
 */
extern void hashtable_free(hashtable_t *t, void (*f)(void*), void (*fk)(void*));

/**
 * Insert an element into a hashtable
 * \param t a hashtable
 * \param key element key in the hashtable
 * \param data element to insert
 */
extern void hashtable_insert(hashtable_t *t, void *key, void *data);

/**
 * Look for an element which correspond to a key.
 * \param t a hashtable
 * \param key a key
 * \return the first element found which correspond to the key.
 */
extern void* hashtable_lookup(const hashtable_t *t, const void *key);

/**
 * Look for all elements which correspond to a key.
 * \param t a hashtable
 * \param key a key
 * \return a queue with all elements found which correspond to the key.
 */
extern queue_t* hashtable_lookup_all(const hashtable_t *t, const void *key);

/**
 * Look for all elements which correspond to a key.
 * \param t a hashtable
 * \param key a key
 * \return an array with all elements found which correspond to the key.
 */
extern array_t* hashtable_lookup_all_array(const hashtable_t* t, const void *key);

/**
 * Create a new hashtable
 * \param hash A hashfunc_t function used to hash keys
 * \param equal A equalfunc_t function used to compare keys
 * \return a new hashtable
 */
extern hashtable_t* hashtable_new(hashfunc_t hash, equalfunc_t equal);

/**
 * Create a new hashtable with a custom initial size
 * \param hash A hashfunc_t function used to hash keys
 * \param equal A equalfunc_t function used to compare keys
 * \param size initial size. If <= 0, use HASH_INIT_SIZE
 * \param fixed_size TRUE/FALSE to keep (or not) size fixed
 * \return a new hashtable
 */
extern hashtable_t* hashtable_new_with_custom_size(hashfunc_t hash,
      equalfunc_t equal, hashtable_size_t size, boolean_t fixed_size);

/**
 * Remove the first element found for the given key (but does not free it)
 * \param t t hashtable
 * \param key key of the element to return
 * \return an element if found, else NULL
 */
extern void* hashtable_remove(hashtable_t *t, const void *key);

/**
 * Removes one given element from a hashtable
 * \param t The hashtable
 * \param key The key of the element to remove
 * \param data The element to remove
 * \return 0 if no element was found, 1 if one was found and removed
 * \warning The element is not freed, this is up the the caller function
 * */
extern int hashtable_remove_elt(hashtable_t* t, const void* key,
      const void* data);

/**
 * Tries to find a given element in a hashtable
 * \param t The hashtable
 * \param key The key of the element to look up
 * \param data The element to look up
 * \return TRUE if the element was found, FALSE otherwise
 * */
extern boolean_t hashtable_lookup_elt(const hashtable_t* t, const void* key,
      const void* data);

/**
 * Get the size (number of element) of the hashtable
 * \param t The hashtable
 * \return the number of elements
 */
extern hashtable_nnodes_t hashtable_size(const hashtable_t *t);

/**
 * Get the maximal number of different hashes in a hashtable (HASHSIZE)
 * \param t Pointer to the hashtable
 * \return HASHSIZE
 */
extern hashtable_size_t hashtable_t_size(const hashtable_t *t);

/**
 * Prints a hashtable with a given verbosity level
 * \param t hashtable
 * \param verbose_lvl verbosity level:
 *  - 1: prints number of nodes, size and load factor
 *  - 2: 1 + least and most loaded slots
 *  - 3: 2 + each node represented with a dot
 *  - 4: 2 + each node represented as a key-value pair
 */
extern void hashtable_print(const hashtable_t *t, int verbose_lvl);

/**
 * An equalfunc_t function for string keys
 * \param a Pointer to the first string
 * \param b Pointer to the second string
 * \return 1 if keys are equal, else 0
 * */
extern int str_equal(const void *a, const void *b);

/**
 * A hashfunc_t function for string key
 * \param key The key of the element
 * \param size number of slots in the targeted hashtable
 * \return A value between 0 and size-1
 * */
extern hashtable_size_t str_hash(const void *key, hashtable_size_t size);

/**
 * Wrapper to the strcmp function allowing qsort to work
 * \param a A pointer on a string (char**)
 * \param b A pointer on a string (char**)
 * \return The result of strcmp on a and b
 */
extern int strcmp_qsort(const void* a, const void* b);

/**
 * Wrapper to the strcmp function allowing bsearch to work
 * \param a A string
 * \param b A string
 * \return The result of strcmp on a and b
 */
extern int strcmp_bsearch(const void* a, const void* b);

///////////////////////////////////////////////////////////////////////////////
//                                     arrays                                //
///////////////////////////////////////////////////////////////////////////////

/**
 * \struct array_s
 * \brief Structure holding a dynamic array of elements. Compared to a queue,
 *        insertion is possible only at the tail.
 *        To improve performance and reduce memory footprint, array should be
 *        used instead of queue as soon as operations are limited to
 *        new/add/flush/foreach/free (should apply to many MAQAO key sets).
 *        If the maximum number of elements is known or can be retrieved at
 *        low cost, create with array_new_with_custom_size to avoid reallocs.
 * */
struct array_s {
   unsigned int length; /**<size of the array*/
   unsigned int init_length; /**<initial size of the array (after new/flush)*/
   unsigned int max_length; /**<capacity of the array*/
   void **mem; /**<content of the array*/
};

/**
 * Macro used to traverse an array
 * X: An array
 * Y: Name of an iterator (not already used)
 */
#define FOREACH_INARRAY(X,Y)\
  void **Y; for(Y=X->mem; Y<X->mem+X->length; Y++)

/**
 * Macro used to traverse an array in reverse order
 * X: An array
 * Y: Name of an iterator (not already used)
 */
#define FOREACH_INARRAY_REVERSE(X,Y)\
  void **Y; for(Y=X->mem+(X->length-1); Y>=X->mem; Y--)

/**
 * Get data when used with the FOREACH_INARRAY macro
 * X: dst variable
 * Y: iterator
 * ex: var =  ARRAY_GET_DATA(var,it)
 */
#define ARRAY_GET_DATA(X,Y) (*(Y))

/**
 * Macro used to get/set value at a given position
 * \warning No check (array must be not NULL and pos <= array->size)
 * \param array array
 * \param pos position (starting from 0) in array
 */
#define ARRAY_ELT_AT_POS(array,pos) ((array)->mem[(pos)])

#define ARRAY_INIT_SIZE       50    /**<Default size for a new dynamic array*/

#define ARRAY_MAX_INCREASE_SIZE (10 * 1000 * 1000)

/**
 * Creates a new, empty array
 * \return An array of size 0
 */
extern array_t *array_new();

/**
 * Creates an array with a custom initial size
 * \param size initial size. If <= 0, use ARRAY_INIT_SIZE
 * \return An array of size 'size'
 */
extern array_t *array_new_with_custom_size(int size);

/**
 * Adds a new element at the end of an array
 * \param array The array to update
 * \param data The data of the new element to add
 */
extern void array_add(array_t *array, void *data);

/**
 * Removes an element at the end of an array
 * \param array The array to update
 * \return data The removed data
 */
extern void *array_remove(array_t *array);

/**
 * Returns the nth element in an array
 * \see use ARRAY_ELT_AT_POS (check-free macro) if performance critical
 * \param array an array
 * \param pos rank of the element to get
 * \return nth element
 */
extern void *array_get_elt_at_pos(array_t *array, int pos);

/**
 * Returns the first element in an array
 * \param array an array
 * \return first element
 */
extern void *array_get_first_elt(array_t *array);

/**
 * Returns the last element in an array
 * \param array an array
 * \return last element
 */
extern void *array_get_last_elt(array_t *array);

/**
 * Sets the nth element in an array
 * \see use ARRAY_ELT_AT_POS (check-free macro) if performance critical
 * \param array an array
 * \param pos rank of the element to set
 * \param data data to set
 */
extern void array_set_elt_at_pos(array_t *array, int pos, void *data);

/**
 * Returns the length of a array
 * \param array a array
 * \return \e array length, or 0 if array is NULL
 */
extern int array_length(array_t *array);

/**
 * Checks if a array is empty
 * \param array a array
 * \return 1 if \e array is empty or NULL, else 0
 */
extern int array_is_empty(array_t *array);

/**
 * Frees all elements in a array and sets its length to 0
 * \param array a array
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
extern void array_flush(array_t *array, void (*f)(void*));

/**
 * Frees a array and its elements if \e f is not null
 * \param array a array
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
extern void array_free(array_t *array, void (*f)(void*));

/**
 * Runs a function on every element in a array
 * \param array a array
 * \param f a function exectued of each element. It must have the following prototype:
 *              void < my_function > (void* elemen, void* user); where:
 *                      - elem is the element
 *                      - user a 2nd parameter given by the user
 * \param user a parameter given by the user for the function to execute
 * */
extern void array_foreach(array_t *array, void (*f)(void *, void *), void *user);

/**
 * Scans an array looking for an element and return the first occurrence while using a custom
 * equal function for looking for the occurrence.
 * \param array an array
 * \param f a function to compare two elements. It must have the following prototype:
 *              int \<my_function\> (const void* e1, const void* e2); where:
 *                      - e1 is an element
 *                      - e2 is an element
 *                      - f return true (nonzero) if its two parameters are equal, else 0
 * \param data an element to look for
 * \return the element if found, else NULL
 */
extern void *array_lookup(array_t* array, int (*f)(const void *, const void *),
                          void* data);

/**
 * Sorts an array using stdlib/qsort, according to a comparison function
 * \see queue_sort
 * \param array an array
 * \param compar comparison function (between two elements)
 */
extern void array_sort (array_t *array, int (*compar)(const void *, const void *));

/**
 * Duplicates an array
 * \param array The array to duplicate
 * \return A clone array
 */
extern array_t *array_dup (array_t *array);

/**
 * Appends to a1 content of a2 (a1 = a1 + a2)
 * \param a1 left/destination array
 * \param a2 right/source array
 */
extern void array_append (array_t *a1, array_t *a2);

///////////////////////////////////////////////////////////////////////////////
//                                     files                                 //
///////////////////////////////////////////////////////////////////////////////

/**
 * Adds prefixs to a path. This prefix is:
 *  \<local_install_dir\>/share/maqao
 * \param subpath a path to prefix
 * \return the prefixed path
 */
extern char* prefixed_path_to(char* subpath);

/**
 * Creates a new directory
 * \param name name of the directory to create
 * \param mode permissions for the directory
 * \return TRUE if the directory has been created, else FALSE
 */
extern int createDir(const char *name, int mode);

/**
 * Creates a new file
 * \param file name of the file to create
 * \return TRUE if the file has been created, else FALSE
 */
extern int createFile(char * file);

/**
 * Deletes a file
 * \param file name of the file to deletes
 * \return TRUE if the file has been deleted, else FALSE
 */
extern int fileDelete(char* file);

/**
 * Checks if a file
 * \param file name of the file to checks
 * \return TRUE if the file exists, else FALSE
 */
extern int fileExist(char* file);

/**
 * Checks if a directory
 * \param dir name of the directory to checks
 * \return TRUE if the directory exists, else FALSE
 */
extern int dirExist(char* dir);

/**
 * Opens a file, maps its contents to memory, and returns a pointer to the string containing it
 * \param filename The name of the file
 * \param filestream Must not be NULL. If pointing to a non-NULL value, will be used as the stream to the file.
 * Otherwise, will be updated to contain the file stream of the file (needed to be closed)
 * \param contentlen If not NULL, will contain the length of the string retrieved from the file
 * \return Pointer to the character stream contained in the file, or NULL if an error occurred, or if \c filename or \c filestream are NULL.
 * The stream must be released using \ref releaseFileContent
 * */
extern char* getFileContent(char* filename, FILE **filestream, size_t* contentlen);

/**
 * Closes a file opened with \ref getFileContent, and unmaps the string from its content
 * \param content Character stream contained in the file (returned by \ref getFileContent)
 * \param filestream The file stream (returned by \ref getFileContent or provided to it)
 * */
extern void releaseFileContent(char* content, FILE* filestream);

/**
 * Gets the path of a file
 * \param filename a file name
 * \return the path of the file (everything before the last '/' or "." if it did not contain any '/') or NULL if there is a problem
 */
extern char* getPath(char* filename);

/**
 * Gets a file basename (substring between the last '/' and before the last '.')
 * \param filename a file name
 * \return the base name (file name without its path and extension), or NULL if there is a problem
 */
extern char* getBasename(char* filename);

/**
 * Removes a subpath from a path
 * \param path a path
 * \param basepath a subpath
 * \return the modified path
 */
extern char* removeBasepath(char* path, char* basepath);

/**
 * Gets the common path between two paths
 * \param filename1 a path
 * \param filename2 a path
 * \param commonpath will contain the common path between filename1 and filename2
 * \return TRUE if a common path has been computed, else FALSE
 */
extern int commonPath(char* filename1, char* filename2, char* commonpath);

///////////////////////////////////////////////////////////////////////////////
//                         formatted text files                              //
///////////////////////////////////////////////////////////////////////////////

/**
 * The following structures are intended to be used for parsing simple text files.
 * The expected structure of the file is as follows:
 * - A header declares the types of sections that can be found in the file, and the list of fields for each section
 *   Fields are expected to be separated by spaces, tabs, or carriage return, and optionally prefixed with a single character
 * - The sections are delimited by tags signaling their beginning and end and containing their type
 * - A special section, the body section, contains the actual content of the file. Its entries are expected to be separated by carriage returns.
 *   It is expected to be unique in the file
 * - Tags signaling the begin and ending of sections must obey the following formatting:
 *   (begin tag)(section name)(begin section tag) and (begin tag)(section name)(end section tag)
 * - In the declaration header:
 *   - A field declaration must begin by a code word signaling its type (numerical or text)
 *   - The code word must be followed by a separator defined in decl_field_delim, then the name of the field (name must be unique in a section)
 *   - For numerical fields, the following additional fields (separated by the separator defined in decl_field_delim) are authorised:
 *     - Unsigned
 *     - Base
 *     - Size
 *   - A section declaration can contain properties (signaled by the prefix defined in propfieldid) signaling how the fields are to be found
 *     in it: by relative position or by name. In the latter case, the fields in the actual sections must be prefixed by their name
 *
 * All tags are defined in the txtfield_t itself and can be updated to adapt to the format of the file.
 * The function lc_txtfile.c:txtfile_initdefault initialises those tags with default values
 *
 * To parse a file, first open it from a file (txtfile_open), or load its content from a string (txtfile_load)
 * The function txtfile_parse parses the file.
 * After that, it is possible to access the fields in a section by their names, and to retrieve all sections from a given type.
 * Lines in the body of the file are considered as sections
 *
 * Example of file structure handled by this:
 * ----
 * @Header_Begin @SCN1_Begin str:field1 opt_str:%field2 int:field3:8:d @SCN1_End
 * @TXT_Begin str:field1 opt_int:/field4:16:h str:field5
 * @TXT_End
 * @Header_End
 * @SCN1_Begin foo 15 @SCN1_End
 * @SCN1_Begin bar %optional 42 @SCN1_End
 * @TXT_Begin foo bar
 * foo2 bar2
 * foo3 /42 bar3
 * @TXT_End
 * */

/**
 * Identifier of the base in which a number is expressed
 * */
typedef enum numbase_e {
   BASE_10 = 0, /**<Number is in base 10 (decimal)*/
   BASE_16, /**<Number is in base 16 (hexadecimal)*/
   BASE_08, /**<Number is in base 8 (octal)*/
   BASE_02, /**<Number is in base 2 (binary)*/
   BASE_MAX /**<Maximal number of supported bases, must always be last*/
} numbase_t;

/**
 * Structure describing a numerical field in a file
 * */
typedef struct num_s {
   int64_t value; /**<Value of the field*/
   uint8_t size; /**<Size of the field in bits*/
   unsigned isunsigned :1; /**<Specifies whether the value in the field is signed or not*/
   unsigned base :3; /**<Specifies the base in which the value is expressed (using values from numbase_e)*/
} num_t;

/**
 * Identifier of the type of a field in a file
 * */
typedef enum txtfieldtype_e {
   FIELD_TXT = 0, /**<Text field (represented as a string)*/
   FIELD_NUM, /**<Numerical field (representing a number)*/
   FIELD_SCNPROPERTY,/**<Field is a section property (only for declarations)*/
   FIELD_MAX /**<Maximal number of field types*/
} txtfieldtype_t;

/**
 * Structure describing a field in a file
 * */
typedef struct txtfield_s {
   char* name; /**<Name of the field*/
   union {
      char* txt; /**<Text value*/
      num_t* num; /**<Numerical value*/
   } field; /**<Value contained in the file*/
   uint32_t posinline; /**<Position of the field in the line (for sections with matching by alignment)*/
   char prefix; /**<Non-alphanumerical character prefixing the field (mandatory if the field is optional)*/
   unsigned type :4; /**<Type of the field, using values from txtfieldtype_e*/
   unsigned optional :1; /**<Specifies whether the field is optional or mandatory*/
   unsigned list :1; /**<Specifies whether the field contains a list of values or not*/
} txtfield_t;

/**
 * Enum containing the identifiers for specifying how the fields in a section must be matched
 * */
typedef enum matchmethod_e {
   MATCH_UNDEF = 0, /**<Undefined method*/
   MATCH_BYPOS, /**<Fields matched by relative position to one another*/
   MATCH_BYNAME, /**<Fields matched by name*/
   MATCH_BYALIGN, /**<Fields are matched by their position in the line*/
   MATCH_MAX /**<Max number of methods (increase the size of txtscn_t->matchfieldmethod if increased)*/
} matchmethod_t;

/**
 * Structure describing a section in a formatted text file
 * */
typedef struct txtscn_s {
   char* type; /**<Name of the type of the section (as referenced in table of section types by name in the txtfile_t structure)*/
   txtfield_t** fields; /**<Array of fields present in the section*/
   struct txtscn_s* nextbodyline;/**<Body line following this section if it is interleaved in the body*/
   uint16_t n_fields; /**<Number of fields present the section*/
   uint32_t line; /**<Line number at which the section was found*/
   unsigned interleaved :1; /**<Specifies whether the section can be interleaved with the text section*/
//   unsigned optional:1;          /**<Specifies whether the section is optional or mandatory*/
   unsigned matchfielmethod :3; /**<Specifies how the fields in the section are matched (by position or by name)*/
} txtscn_t;

/**
 * Structure describing a formatted text file
 * */
typedef struct txtfile_s {
   char* name; /**<Name of the file*/
   FILE* stream; /**<Stream to the file*/
   char* content; /**<String representation of the contents of the file*/
   char* cursor; /**<Cursor inside the string representation of the contents of the file*/
   uint32_t line; /**<Line number inside the file*/
   char* hdrname; /**<Name of the header, expected to begin a file*/
   char* tag_prfx; /**<String prefixing a tag*/
   char* tag_begin_sufx; /**<String suffixing a begin tag*/
   char* tag_end_sufx; /**<String suffixing an end tag*/
   char* commentline; /**<String signaling the beginning of a comment line (the remainder of the line is ignored)*/
   char* commentbegin; /**<String signaling the beginning of a comment */
   char* commentend; /**<String signaling the end of a comment*/
   char* bodyname; /**<String containing the name of the body section (actual content of the file)*/
   char* strfieldid; /**<Identifier of a string field in the declarations*/
   char* numfieldid; /**<Identifier of a numerical field in the declarations*/
   char* propfieldid; /**<Identifier of a section property in the declarations*/
   char* optfield_prefix; /**<Prefix used to signal the beginning of the declaration of an optional field*/
   char* scndecl_matchfieldbyname; /**<Keyword specifying in the declaration of a section if the fields must be matched by name*/
   char* scndecl_matchfieldbypos; /**<Keyword specifying in the declaration of a section if the fields must be matched by position*/
   char* scndecl_matchfieldbyalign; /**<Keyword specifying in the declaration of a section if the fields must be matched by alignment*/
   char* scndecl_interleaved; /**<Keyword specifying in the declaration of a section if it can be interleaved with the body*/
   char numdecl_unsigned; /**<Character specifying that a numerical field is unsigned in the declarations (default is signed)*/
   char numdecl_base[BASE_MAX]; /**<Characters specifying in which of the bases defined in numbase_t a numerical field is represented*/
   char numdecl_size; /**<Character prefixing the size in bits on which the value of a numerical field must be defined*/
   char fieldidsuf_list; /**<Suffix to a character identifier in the declarations signaling that the field is a list*/
   txtscn_t** sectiontemplates; /**<Array of the templates of sections, ordered by the name of their types*/
   txtscn_t** sections; /**<Array of sections found in the file*/
   txtscn_t** bodylines; /**<Array of lines from the body, each line represented as a section*/
   uint32_t n_bodylines; /**<Number of lines in the body*/
   uint16_t n_sectiontemplates; /**<Size of the array of templates of sections*/
   uint32_t n_sections; /**<Size of the array of sections*/
   char field_name_separator; /**<Character used to separate the type of a file from its content in the declarations (colon by default)*/
   char decl_field_delim; /**<Character used to separate the fields declarations (space by default)*/
   char field_delim; /**<Character used to separate the fields values in the sections and body (space by default)*/
   char txtfield_delim; /**<Delimiter of a string field (double quote by default)*/
   char listfield_delim; /**<Delimiter of the elements inside a list field (semicolon by default)*/
   size_t contentlen; /**<Size of the content of the file (length of \c content)*/
   unsigned int parsed :1; /**<Flag signaling if the filed has been correctly parsed or not*/
} txtfile_t;

/**
 * Opens a text file to be parsed and returns a txtfile_t structure
 * \param filename Name of the file
 * \return A new txtfile_t structure containing the contents of the file or NULL ifs filename is NULL or the file not found
 * */
extern txtfile_t* txtfile_open(char* filename);

/**
 * Loads a text file from a string representing its contents and returns a txtfile_t structure
 * \param content String containing the content of the file
 * \return A new txtfile_t structure containing the contents of the file
 * */
extern txtfile_t* txtfile_load(char* content);

/**
 * Closes a text file and frees its contents
 * \param Structure representing the text file
 * \return EXIT_SUCCESS / error code
 * */
extern int txtfile_close(txtfile_t* tf);

/**
 * Sets the comment delimiters for a text file
 * \param tf The text file
 * \param commentline Delimiter for a commented line (everything following will be ignored)
 * \param commentbegin Delimiter for the beginning of a comment
 * \param commentend Delimiter for the ending of a comment
 * */
extern void txtfile_setcommentsdelim(txtfile_t* tf, char* commentline,
      char* commentbegin, char* commentend);

/**
 * Sets the tags for identifying sections in the file
 * \param tf The text file
 * \param scnprfx Prefix signaling the beginning of a section tag
 * \param tag_begin_sufx Suffix for the tag signaling the beginning of a section
 * \param scnendsufx Suffix for the tag signaling the ending of a section
 * \param bodyname Name of the text section
 * */
extern void txtfile_setscntags(txtfile_t* tf, char* tag_prfx,
      char* tag_begin_sufx, char* tag_end_sufx, char* bodyname, char* hdrname);

/**
 * Sets the delimiters for identifying fields declarations in the header
 * \param tf The text file
 * \param strfieldid Identifier of a string field. Declaration of string fields must begin with this
 * \param numfieldid Identifier of an integer field. Declaration of interger fields must begin with this
 * \param field_name_separator Character separating the components of the field declarations
 * \param optfield_prefix Prefix used to signal the beginning of the declaration of an optional field
 * */
extern void txtfile_setfieldtags(txtfile_t* tf, char* strfieldid,
      char* numfieldid, char field_name_separator, char* optfield_prefix);

/**
 * Parses a text file
 * \param tf Structure representing the text file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int txtfile_parse(txtfile_t* tf);

/**
 * Returns the position of the cursor inside the file. This allows to know the character at which an error occurred
 * \param tf Structure representing the text file
 * \return Pointer to the character pointed by the cursor. For a file not yet parsed, it should be the first character in the
 * file. For a successfully parsed file, it should be the end string character '\0'. Otherwise, this should be the character
 * at which the parsing stopped, or NULL if \c tf is NULL
 * */
extern char* txtfile_getcursor(txtfile_t* tf);

/**
 * Returns the section found at given index in the body
 * \param tf Structure representing the text file
 * \param i Index of the line to retrieve (this is NOT the line in the original file)
 * \return Section representing the line or NULL if \c tf is NULL or i is out of range
 * */
extern txtscn_t* txtfile_getbodyline(txtfile_t* tf, uint32_t i);

/**
 * Returns the section not representing a body line at a given index
 * \param tf Structure representing the text file
 * \param i Index of the section to retrieve (this is the index among all sections not
 * representing a body line)
 * \return Section or NULL if \c tf is NULL or i is out of range
 * */
extern txtscn_t* txtfile_getsection(txtfile_t* tf, uint32_t i);

/**
 * Returns all sections of a given type
 * \param tf Structure representing the text file
 * \param type Type of section to look for
 * \param n_scns Return parameter, will be updated to contain the number of sections
 * \return Array of sections with that type or \c NULL if either parameter is NULL or no section with that type were found
 * */
extern txtscn_t** txtfile_getsections_bytype(txtfile_t* tf, char* type,
      uint32_t* n_scns);

/**
 * Returns all sections of a given type sorted over a given field in the section
 * \param tf Structure representing the text file
 * \param type Type of section to look for
 * \param n_scns Return parameter, will be updated to contain the number of sections
 * \param fieldname Name of the field over which to order the sections. If NULL or not defined for this type of sections,
 * the sections will not be ordered
 * \return Array of sections with that type or \c NULL if either parameter is NULL or no section with that type were found
 * */
extern txtscn_t** txtfile_getsections_bytype_sorted(txtfile_t* tf, char* type,
      uint32_t* n_scns, char* fieldname);

/**
 * Looks up into an array of sections extracted from a file for a section based on the value of a field
 * \warning The sections are expected to have been retrieved using txtfile_getsections_bytype_sorted. Using this function on another type
 * of array of sections will lead to undefined behaviour.
 * \param scns Array of sections of the same type and ordered according to \c fieldname
 * \param n_scns Size of the array
 * \param fieldname Name of the field over which to look for
 * \param txtval Value to look for if \c fieldname is a text field
 * \param numval Value to look for if \c fieldname is a numerical field
 * \return The section whose value of \c fieldname is either \c txtval or \c numval, or NULL if such a section was not found,
 * if \c scns is NULL or empty, or if \c fieldname is NULL or is a text field while \c txtval is NULL
 * */
extern txtscn_t* txtscns_lookup(txtscn_t** scns, uint32_t n_scns,
      char* fieldname, char* txtval, int64_t numval);

/**
 * Returns the name of a text file
 * \param tf Structure representing the text file
 * \return Name of the file or NULL if \c tf is NULL
 * */
extern char* txtfile_getname(txtfile_t* tf);

/**
 * Returns the number of body lines in a parsed text file
 * \param tf Structure representing the text file
 * \return Number of lines in the body
 * */
extern uint32_t txtfile_getn_bodylines(txtfile_t* tf);

/**
 * Returns the number of sections in a parsed text file
 * \param tf Structure representing the text file
 * \return Number of sections in the file, excluding those representing body lines
 * */
extern uint32_t txtfile_getn_sections(txtfile_t* tf);

/**
 * Retrieves the current line of a text file being parsed.
 * This is mainly useful for when a parsing error occurs, otherwise the current line should either be the first (file not parsed)
 * or the last (file successfully parsed)
 * \param tf The file
 * \return The line or 0 if the file is NULL
 * */
extern uint32_t txtfile_getcurrentline(txtfile_t* tf);

/**
 * Sorts the body lines in a file by a given field
 * \param tf The parsed text file. Must have already been parsed
 * \param fieldname The name of the field to use for the comparison. Must not be optional.
 * If NULL, the name of the field used for the last invocation of this function will be used, if this is the first invocation an error will be returned
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
extern int txtfile_sort_bodylines(txtfile_t* tf, char* fieldname);

/**
 * Returns a given field from a section
 * \param ts Structure representing the text section
 * \param field Name of the field to retrieve
 * \return Structure representing the field or NULL if \c ts or \c field is NULL or no field with that name exists
 * */
extern txtfield_t* txtscn_getfield(const txtscn_t* ts, char* field);

/**
 * Returns a given field from a section as a list. This function is intended to be used for fields of type list
 * \param ts Structure representing the text section
 * \param field Name of the field to retrieve
 * \param listsz Return parameter. Will be updated to contain the size of the list.
 * \return Array of the values for this field or NULL if \c ts, \c field or \c listsz is NULL or no field with that name exists.
 * The size of the array is contained in listsz. If the field is not a list, an array containing a single element from will be returned
 * */
extern txtfield_t** txtscn_getfieldlist(const txtscn_t* ts, char* field,
      uint16_t *listsz);

/**
 * Retrieves the line at which a section was declared
 * \param ts The section
 * \return The line or 0 if the section is NULL
 * */
extern uint32_t txtscn_getline(txtscn_t* ts);

/**
 * Returns the next body line following a section interleaved with the body
 * \param ts Structure representing the text section
 * \return Structure representing the next line, or NULL if \c ts is NULL or not interleaved
 * */
extern txtscn_t* txtscn_getnextbodyline(txtscn_t* ts);

/**
 * Returns the type of a section
 * \param ts Structure representing the text section
 * \return String representing the type of the section, or NULL if \c ts is NULL
 * */
extern char* txtscn_gettype(txtscn_t* ts);

/**
 * Returns the value of a text field
 * \param field The field
 * \return The value or NULL if field is not a text field
 * */
extern char* txtfield_gettxt(txtfield_t* field);

/**
 * Returns the value of a numerical field
 * \param field The field
 * \return The value or 0 if field is not a numerical field
 * */
extern int64_t txtfield_getnum(txtfield_t* field);

///////////////////////////////////////////////////////////////////////////////
//                                 graphs                                    //
///////////////////////////////////////////////////////////////////////////////
/**
 * \struct graph_node_s
 * \brief Structure holding a node of a graph
 * */
struct graph_node_s {
   list_t *out; /**<List of edge which left the node*/
   list_t *in; /**<List of edge which come into the node*/
   void *data; /**<user data*/
};

/**
 * \struct graph_edge_s
 * \brief Structure holding an edge linking two graph nodes
 * */
struct graph_edge_s {
   graph_node_t *from; /**<Nodes at the begining of the edge*/
   graph_node_t *to; /**<Nodes at the end of the edge*/
   void *data; /**<user data*/
};

/**
 * \struct graph_connected_component_s
 * \brief Structure holding a graph connected component
 * */
struct graph_connected_component_s {
   hashtable_t *entry_nodes; /**<Set of entry nodes*/
   hashtable_t *nodes; /**<Set of all nodes*/
   hashtable_t *edges; /**<Set of all edges*/
};

/**
 * \struct graph_s
 * \brief Structure holding a graph, that is a set of connected components
 * */
struct graph_s {
   queue_t *connected_components; /**<Queue of connected components*/
   hashtable_t *node2cc; /**<Index to get the connected component holding a given node*/
   hashtable_t *edge2cc; /**<Index to get the connected component holding a given edge*/
};

extern void   *graph_node_get_data           (graph_node_t *node);
extern list_t *graph_node_get_incoming_edges (graph_node_t *node);
extern list_t *graph_node_get_outgoing_edges (graph_node_t *node);

extern void    *graph_edge_get_data     (graph_edge_t *edge);
extern graph_node_t *graph_edge_get_src_node (graph_edge_t *edge);
extern graph_node_t *graph_edge_get_dst_node (graph_edge_t *edge);

extern void graph_node_set_data           (graph_node_t *node, void   *data );
extern void graph_node_set_incoming_edges (graph_node_t *node, list_t *edges);
extern void graph_node_set_outgoing_edges (graph_node_t *node, list_t *edges);

extern void graph_edge_set_data     (graph_edge_t *edge, void    *data);
extern void graph_edge_set_src_node (graph_edge_t *edge, graph_node_t *node);
extern void graph_edge_set_dst_node (graph_edge_t *edge, graph_node_t *node);

/**
 *      Adds an edge between two existing graph nodes
 * \param from a graph node at the begining of the edge
 * \param to a graph node at the end of the edge
 * \param data element of the edges
 * \return the new edge
 */
extern graph_edge_t* graph_add_edge(graph_node_t *from, graph_node_t *to, void* data);

/**
 * Wrapper for graph_add_edge function. Add an edge only if the edge does not already exist
 * \param src source node of the edge
 * \param dst destination node of the edge
 * \param data an user data
 * \return 1 if the edge is added, else 0
 */
extern int graph_add_uniq_edge(graph_node_t* src, graph_node_t* dst, void* data);

/**
 * Removes and frees a node (if f_node not null) and frees all edges linked to it (if f_edge not null)
 * \see graph_free_from_nodes to easily and quickly free an entire graph (connected component) or, better, use new interface (based on graph_connected_component_t)
 * \param graph Node to free
 * \param f_node a function used to free an element of a node, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 * \param f_edge a function used to free an element of an edge, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
extern void graph_node_free(graph_node_t* graph, void (*f_node)(void*),
      void (*f_edge)(void*));

/**
 * Frees an entire graph (connected component) defined by its nodes.
 * This function is fast since graph_node_free is bypassed (hashtables used to mark nodes and edges)
 * \param nodes set of graph nodes (as dynamic array)
 * \see graph_node_free for f_node and f_edge
 */
extern void graph_free_from_nodes(array_t *nodes, void (*f_node)(void*), void (*f_edge)(void*));

/**
 * Look for an edge from \e from to \e to, with value equals to \e data
 * \param from source node
 * \param to destination node
 * \param data data of the looked for edge. If it is NULL, this field is not uses for comparison
 * \return an edge if found, else NULL
 */
extern graph_edge_t* graph_lookup_edge(graph_node_t *from, graph_node_t *to, void *data);

/**
 * Creates a new graph node
 * \param data element of the node
 * \return a new graph node
 */
extern graph_node_t* graph_node_new(void *data);

/**
 * Removes and frees (if f not null) an edge
 * \param edge an edge to free
 * \param f a function used to free an element, with the following prototype:
 *      void < my_function > (void* data); where:
 *                      - data is the element to free
 */
extern void graph_remove_edge(graph_edge_t* edge, void (*f)(void*));

/**
 * Traverses a graph using standard Breadth First Search (BFS) algorithm from a source node.
 * Only nodes accessible from the source node will be explored.
 * \param root a graph (root node)
 * \param func_node a function to execute on a node, with the following prototype:
 *      void < my_function > (graph_node_t* data, void* un); where:
 *                      - data is the element
 *                      - un is a user data
 * \param func_edge a function to execute on an edge, with the following prototype:
 *      void < my_function > (graph_node_t* from, graph_node_t to); where:
 *                      - from is the begining of an edge
 *                      - to is the end of an edge
 * \param un_data passed to func_node
 */
extern void graph_node_BFS(graph_node_t *root, void (*func_node)(graph_node_t *, void*),
      void (*func_edge)(graph_node_t *, graph_node_t *), void* un_data);

/**
 * Returns nodes accessible from a root node, using a DFS.
 * \param root a graph (root node)
 * \return array of nodes
 */
extern array_t *graph_node_get_accessible_nodes(const graph_node_t *root);

/**
 * Traverse a graph using standard Depth First Search (DFS) algorithm from a source node.
 * Only nodes accessible from the source node will be explored.
 * \param root a graph (root node)
 * \param func_node_before function called before exploring children of a node
 * \param func_node_after function called after exploring children of a node
 * \param func_edge function called before traversing an edge
 * \param user_data pointer passed as parameter to the three previous functions
 */
extern void graph_node_DFS(graph_node_t *root,
      void (*func_node_before)(graph_node_t *, void *),
      void (*func_node_after)(graph_node_t *, void *),
      void (*func_edge)(graph_edge_t *, void *), void *user_data);

/**
 * Traverses a graph using standard Back Depth First Search (DFS) algorithm from a source node.
 * Only nodes accessible from the source node will be explored.
 * \param root a graph (root node)
 * \param func_node_before function called before exploring children of a node
 * \param func_node_after function called after exploring children of a node
 * \param func_edge function called before traversing an edge
 * \param user_data pointer passed as parameter to the three previous functions
 */
extern void graph_node_BackDFS(graph_node_t *root,
      void (*func_node_before)(graph_node_t *, void *),
      void (*func_node_after)(graph_node_t *, void *),
      void (*func_edge)(graph_edge_t *, void *), void *user_data);

/**
 * Topologically sorts a graph from a root node
 * \param root a graph (root node)
 * \return sorted list of nodes (as a dynamic array)
 */
extern array_t *graph_node_topological_sort(const graph_node_t *root);

/**
 * Returns a table that can be passed to graph_is_backedge_from_table
 * \param root a graph (root node)
 * \return a table for graph_is_backedge_from_table
 */
extern hashtable_t *graph_node_get_backedges_table(const graph_node_t *root);

/**
 * Checks whether an edge is a backedge, provided a table returned by graph_node_get_backedges_table.
 * Use this function to check at least two or three edges into a graph. In case of only one edge, graph_is_backedge_from_graph_node is simpler to use.
 * \param edge an edge
 * \param bet an object returned by graph_node_get_backedges_table
 * \return TRUE or FALSE
 */
extern int graph_is_backedge_from_table(const graph_edge_t *edge,
      const hashtable_t *bet);

/**
 * Checks whether an edge is a backedge, provided a graph (a root node).
 * Use this function to check only one or two edges into a graph. In other cases, use graph_node_get_backedges_table.
 * \param edge an edge
 * \param root a graph (root node)
 * \return TRUE or FALSE
 */
extern int graph_is_backedge_from_graph_node(const graph_edge_t *edge, const graph_node_t *root);

/**
 * Returns the set of paths (as a queue) in a graph, starting from a given source node.
 * \param root a graph (root node)
 * \return queue of paths starting from root
 */
extern queue_t *graph_node_compute_paths(const graph_node_t *root);

/**
 * Gets the number of paths in a graph starting from a given source (root) node.
 * Paths are not computed (faster than first computing paths and count them).
 * \param root a graph (root node)
 * \param user_max_paths maximum number of paths that the user accepts to compute. A negative or zero value is equivalent to MAX_PATHS
 * \return paths number
 */
extern int graph_node_get_nb_paths(const graph_node_t *root, int user_max_paths);

/**
 * Checks whether a graph is consistent
 * \param root a graph (root node)
 * \return FALSE or TRUE
 */
extern int graph_node_is_consistent(const graph_node_t *root);

/**
 * Frees the memory allocated for given paths
 * \param paths paths to free
 */
extern void graph_free_paths(queue_t *paths);

/**
 * Returns predecessors of a node
 * \param node a node
 * \return array of nodes or PTR_ERROR
 */
extern array_t *graph_node_get_predecessors (graph_node_t *node);

/**
 * Returns successors of a node
 * \param node a node
 * \return array of nodes or PTR_ERROR
 */
extern array_t *graph_node_get_successors (graph_node_t *node);

////////////// new graph functions, based on connected components /////////////

/**
 * Returns entry nodes from a graph connected component
 * \param cc connected component
 * \return array of entry nodes
 */
extern hashtable_t *graph_connected_component_get_entry_nodes (graph_connected_component_t *cc);

/**
 * Returns nodes from a graph connected component
 * \param cc connected component
 * \return hashtable of nodes
 */
extern hashtable_t *graph_connected_component_get_nodes (graph_connected_component_t *cc);

/**
 * Returns edges from a graph connected component
 * \param cc connected component
 * \return hashtable of edges
 */
extern hashtable_t *graph_connected_component_get_edges (graph_connected_component_t *cc);

/**
 * Creates a graph (set of connected components)
 * \return new graph or PTR_ERROR in case of failure
 */
extern graph_t *graph_new ();

/**
 * Returns connected components of a graph
 * \param graph a graph
 * \return queue of connected components
 */
extern queue_t *graph_get_connected_components (graph_t *graph);

/**
 * Returns node to connected component index of a graph
 * \param graph a graph
 * \return node to cc index (hashtable of (node,cc) pairs)
 */
extern hashtable_t *graph_get_node2cc (graph_t *graph);

/**
 * Returns edge to connected component index of a graph
 * \param graph a graph
 * \return edge to cc index (hashtable of (edge,cc) pairs)
 */
extern hashtable_t *graph_get_edge2cc (graph_t *graph);

/**
 * Creates a node from input data and inserts it into a graph
 * \param graph graph to fill
 * \param data data to save in new node
 * \return new node
 */
extern graph_node_t *graph_add_new_node (graph_t *graph, void *data);

/**
 * Creates an edge from input nodes (first one being origin) and inserts it into a graph
 * \param graph graph to fill
 * \param n1 origin node
 * \param n2 destination node
 * \param data data to save in new edge
 * \param new edge
 */
extern graph_edge_t *graph_add_new_edge (graph_t *graph, graph_node_t *n1, graph_node_t *n2, void *data);

/**
 * Wrapper for graph_add_new_edge function. Adds an edge only if the edge does not already exist
 * \param n1 origin node
 * \param n2 destination node
 * \param data user data
 * \return 1 if the edge is added, else 0
 */
extern int graph_add_new_edge_uniq (graph_t *graph, graph_node_t *n1, graph_node_t *n2, void *data);

/**
 * Frees a graph (set of connected components
 * \param graph graph to free
 * \param f_node function used to free a node
 * \param f_edge function used to free an edge
 * \see graph_node_free for f_node and f_edge
 */
extern void graph_free (graph_t *graph, void (*f_node)(void*), void (*f_edge)(void*));

extern void graph_for_each_path (graph_t *graph, int max_paths,
                                 void (*fct) (array_t *, void *), void *data);

extern array_t *graph_cycle_get_edges (queue_t *cycle, boolean_t (*ignore_edge) (const graph_edge_t *edge));

extern void graph_for_each_cycle (graph_t *graph, int max_paths,
                                  boolean_t (*ignore_edge) (const graph_edge_t *edge),
                                  void (*fct) (queue_t *cycle, void *), void *data);

/**
 * Type for the 'data' parameter of the graph_update_critical_paths function
 */
typedef struct {
   float max_length; // maximum length (sum of weights along edges)
   array_t *paths; // set of longest (critical) paths
   float (*get_edge_weight) (graph_edge_t *); // used to return the weight of an edge
} graph_update_critical_paths_data_t;

/**
 * Updates critical paths for a graph
 * Passed to the graph_for_each_path function that will invoke on each path
 * \param path a path (array of nodes)
 * \param data critical paths data (cast to graph_update_critical_paths_data_t *)
 */
extern void graph_update_critical_paths (array_t *path, void *data);

/**
 * Returns critical paths for a graph
 * \param graph a graph
 * \param max_paths maximum number of paths to consider from an entry node
 * \param crit_paths pointer to an array, set to critical paths
 * \param get_edge_weight function to return the weight of an edge (providing genericity)
 */
extern void graph_get_critical_paths (graph_t *graph, int max_paths,
                                      array_t **crit_paths,
                                      float (*get_edge_weight) (graph_edge_t *));

///////////////////////////////////////////////////////////////////////////////
//                                  help                                     //
///////////////////////////////////////////////////////////////////////////////
//
// !! DO NOT MODIFY MANUALLY THESE STRUCTURES. USE SETTERS !!
//

typedef struct help_s help_t;

/**
 * \enum option_type_val_e
 *
 */
typedef enum option_type_val_e {
   _HELPTYPE_SEP, /**< The option is a separator (longname is the name)*/
   _HELPTYPE_OPT /**< The option is a real option*/
} option_type_val_t;

/**
 * \enum example_type_val_e
 *
 */
typedef enum example_type_val_e {
   _EXAMPLE_CMD, /**< Command*/
   _EXAMPLE_DESC /**< Description*/
} example_type_val_t;

/**
 * \struct option_value_s
 */
typedef struct option_value_s {
   char* name; /**< Name of the value*/
   char* desc; /**< Description of the value (optional)*/
   char is_default; /**< Set to TRUE is the value is a default value*/
} option_value_t;

/**
 * \struct option_s
 */
typedef struct option_s {
   char* shortname; /**< Short name of the option (ex: -h)*/
   char* longname; /**< long name of the option (ex : help)*/
   /**< or name of the separator*/
   char* desc; /**< option descriptor*/
   char* arg; /**< option parameter*/
   option_type_val_t type; /**< type of the option*/
   char is_arg_opt; /**< TRUE if the parameter is optional*/
   queue_t* values; /**< A list of available values*/
} option_t;

/**
 * \struct help_s
 */
struct help_s {
   option_t** options; /**<*/
   char* usage; /**<*/
   char* description; /**<*/
   char*** examples; /**<*/
   char* program; /**<*/
   char* version; /**<*/
   char* bugs; /**<*/
   char* copyright; /**<*/
   char* author; /**<*/
   char* date; /**<*/
   char* build; /**<*/
   int size_options; /**<*/
   int size_examples; /**<*/
};

/**
 * Initialize an help_t* object
 * \return an initialized help_t object
 */
extern help_t* help_initialize();

/**
 * Add an option in an help object
 * \param help an initialized help_t object
 * \param short name shortname of the option
 * \param longname long_name of the option
 * \param desc description of the option
 * \param arg argument of the option
 * \param is_arg_opt TRUE is the argument is optionnal
 */
extern void help_add_option(help_t* help, const char* shortname,
      const char* longname, const char* desc, const char* arg, char is_arg_opt);

/**
 * Add a separator in the option list
 * \param help an initialized help_t object
 * \param name name of the separator
 */
extern void help_add_separator(help_t* help, const char* name);

/**
 * Set the help description section (it must been formatted)
 * \param help an initialized help_t object
 * \param desc the description
 */
extern void help_set_description(help_t* help, const char* desc);

/**
 * Set the help usage section (it must been formatted)
 * \param help an initialized help_t object
 * \param usage the usage
 */
extern void help_set_usage(help_t* help, const char* usage);

/**
 * Set the email address for bug reporting
 * \param help an initialized help_t object
 * \param addr email address for bug reporting
 */
extern void help_set_bugs(help_t* help, const char* addr);

/**
 * Set the copyright
 * \param help an initialized help_t object
 * \param copyright the program copyright
 */
extern void help_set_copyright(help_t* help, const char* copyright);

/**
 * Set the author
 * \param help an initialized help_t object
 * \param author the author
 */
extern void help_set_author(help_t* help, const char* author);

/**
 * Set the program name
 * \param help an initialized help_t object
 * \param prog the program name
 */
extern void help_set_program(help_t* help, const char* prog);

/**
 * Set the version
 * \param help an initialized help_t object
 * \param version the version
 */
extern void help_set_version(help_t* help, const char* version);

/**
 * Set the date
 * \param help an initialized help_t object
 * \param date the date
 */
extern void help_set_date(help_t* help, const char* date);

/**
 * Set the build number
 * \param help an initialized help_t object
 * \param build the build number
 */
extern void help_set_build(help_t* help, const char* build);

/**
 * Add an example in the help_t object
 * \param help an initialized help_t object
 * \param cmd command of the example
 * \param desc description of the example
 */
extern void help_add_example(help_t* help, const char* cmd, const char* desc);

/**
 * Display the help
 * \param help an initialized help_t object
 * \param output file where to display the help
 */
extern void help_print(help_t* help, FILE* output);

/**
 * Display the version
 * \param help an initialized help_t object
 * \param output file where to display the version
 */
extern void help_version(help_t* help, FILE* output);

/**
 * Delete a help_t structure
 * \param help an initialized help_t object
 */
extern void help_free(help_t* help);

#endif

