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

#ifndef __LA_LIBMDBG_H__
#define __LA_LIBMDBG_H__
#include "DwarfLight.h"

/**
 * Load debug data associated to a binary file and its instructions
 * \param asmf a binary file
 * \return EXIT_SUCCESS if successful, error code otherwise
 */
extern int asmfile_load_dbg(asmfile_t* asmf);

/**
 * Load debug data associated to a function
 * \param f a function
 */
extern void asmfile_load_fct_dbg(fct_t* f);

/**
 * Add labels from virtual functions extracted from debug data
 * \param asmf a binary file
 */
extern void asmfile_add_label_from_dbg(asmfile_t* asmf);

/** 
 * Free all debug data associated to a binary file
 * \param asmf a binary file
 */
extern void asmfile_unload_dbg(asmfile_t* asmf);

/**
 * Checks if a debug function exists at a specific address
 * \param asmf an assembly file containing debug data
 * \param start_addr First address where to look for the function
 * \param end_addr Last address where to look for the function
 * \param ret_addr used to return address of debug data
 * \return the function name is there is a function, else NULL
 */
extern char* asmfile_has_dbg_function(asmfile_t* asmf, int64_t start_addr,
      int64_t end_addr, int64_t* ret_addr);

/**
 * Uses debug data to find ranges (can be used to detect inlining)
 * \param asmf an assembly file containing debug data
 */
extern void asmfile_detect_debug_ranges(asmfile_t* asmf);

/**
 * Update labels of the given asmfile in using debug info if they are available.
 * \param asmfile The asmfile to update
 * \return EXIT_SUCCESS if the labels could be added successfully, error code otherwise
 */
extern int asmfile_add_debug_labels(asmfile_t* asmfile);

/**
 * Analyzes the binary to get options used to compile the binary
 * and saves them into debug data
 * \param asmf an assembly file
 */
extern char* asmfile_get_compile_options(asmfile_t* asmf);

/**
 * Get the producer (a string in debug data to save compiler, version ...)
 * \param f a function
 * \return the producer string or NULL if not available
 */
extern char* fct_getproducer(fct_t* f);

/**
 * Get the directory where the file was located during compilation
 * \param f a function
 * \return the file directory or NULL if not available
 */
extern char* fct_getdir(fct_t* f);

/**
 * Return function ranges
 * \param f a function
 * \return a queue of ranges
 */
extern queue_t* fct_get_ranges(fct_t* f);

/**
 * Gets the options used to compile the source file containing the function
 * \param f a function
 * \return a string containing options on what is used to compile the function or NULL
 */
extern char* fct_get_compile_options(fct_t* f);

/**
 * Add a ranges in a function debug data
 * \param f a function
 * \param start first instruction of the range
 * \param stop last instruction of the range
 */
extern void fct_add_range(fct_t* f, insn_t* start, insn_t* stop);

///**
// * Get Dwarf info with debian os system libraries (read .gnu_debug_link section in elf)
// * \param libpath The path of the library (.so) to analyze
// * \param asmf The assembly file
// * \return the dwarf structure filled out OR NULL if not available.
// */
//extern DwarfAPI* asmfile_debug_handle_link (const char* libpath, asmfile_t* asmf);

#endif
