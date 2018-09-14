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

// CF utils.c

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h> // FILE
#include <stdint.h> // int64_t...

#define rdtscll(val) (val) = 0;

// Parse a string and split each element into an array from 0 to nbElements-1
// str         The string to parse.
// delimiter   The delimiter to split the string.
// nbElements  The number of elements after the parse into the array.
// return      A char array of nbElements containing the split strings.
char** split_string (const char* str, const char delimiter, unsigned* nbElements);

/*
 * Utility function to retrieve an addess in decimal or binary form
 * \param longaddr The address in string format
 * \return The value of the address
 */
int64_t perf_utils_readhex (char* longaddr);

// Get the extension of a file
const char* get_filename_extension(const char *fileName);

// Open a file in a given directory
FILE *fopen_in_directory (const char *dir_name, const char *file_name, const char *mode);

/* Calls a function for each file inside a given directory
 * \param path path to directory
 * \param f function to call (arg1=path, arg2=<file name>, arg3=data)
 * \param data data to pass to f (generic: void *) */
void for_each_file_in_directory (const char *path,
                                 void (*f) (const char *path, const char *d_name, void *data),
                                 void *data);

/* Similar to for_each_file_in_directory but function receives a directory name as second argument */
void for_each_directory_in_directory (const char *path,
                                      void (*f) (const char *path, const char *d_name, void *data),
                                      void *data);

#endif
