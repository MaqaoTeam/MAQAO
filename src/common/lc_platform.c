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
 * \file
 * */
#ifdef _WIN32
#include <stdlib.h>
#include <stdio.h>
#else
#include <stdarg.h>
#include <libgen.h>
#endif

#include "libmcommon.h"

///////////////////////////////////////////////////////////////////////////////
//                        Windows specific functions                         //
///////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

char * __strtok(char * str, const char * delimiters)
{
   static char ** context = NULL;

   if ((context == NULL || *context == NULL) && str == NULL)
   return NULL;

   if (context == NULL || (str != NULL && context != &str))
   context = &str;

   return strtok_s(str, delimiters, context);
}

char * __getenv(const char * name)
{
   size_t required_size;
   char * env_value;

   getenv_s(&required_size, NULL, 0, name);
   if (required_size == 0)
   return NULL;

   env_value = (char *)lc_malloc0(required_size * sizeof(char));

   getenv_s(&required_size, env_value, required_size, name);

   return env_value;
}

char * strndup(const char * s, size_t size)
{
   char *dup, *end = memchr(s, 0, size);

   if (end)
   size = end - s + 1;

   dup = lc_malloc0(size);

   if (size)
   {
      memcpy(dup, s, size - 1);
      dup[size - 1] = '\0';
   }

   return dup;
}

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
char* lc_basename(const char* path)
{
   if (!path) return strdup (".");

#ifdef _WIN32
   char* fname = (char*)lc_malloc0(_MAX_FNAME);
   _splitpath_s(path, NULL, 0, NULL, 0, fname, _MAX_FNAME, NULL, 0);
   return fname;

#else
   char *basec = lc_strdup(path); // don't want path to be modified: save it
   char *fname = lc_strdup(basename(basec)); // basename return value cannotq be freed
   lc_free(basec); // now basename finished, no more need of basec copy
   return fname; // can safely be freed by the user after using it
#endif

}

/*
 * Retrieves the name of the directory from a full path
 * \note The user needs to free the returned value
 * \param p Full path
 * \return The name of the directory
 */
char* lc_dirname(const char* path)
{
   if (!path) return strdup (".");

#ifdef _WIN32
   char* dirname;
   char drive_dir[_MAX_DRIVE + _MAX_DIR];
   char dir[_MAX_DIR];

   _splitpath_s(path, drive_dir, _MAX_DRIVE, dir, _MAX_DIR, NULL, 0, NULL, 0);
   strcat_s(drive_dir, _MAX_DRIVE + _MAX_DIR, dir);
   dirname = _strdup(drive_dir);

   return dirname;
#else
   char *dirc = lc_strdup(path); // don't want path to be modified: save it
   char *dir_name = lc_strdup(dirname(dirc)); // dirname return value cannot be freed
   lc_free(dirc); // now dirname finished, no more need of dirc copy
   return dir_name; // can safely be freed by the user after using it
#endif
}

char * lc_strncpy(char * destination, size_t size, const char * source,
      size_t num)
{

#ifdef _WIN32
   strncpy_s(destination, size, source, num);
#else 
   (void) size;
   strncpy(destination, source, num);
#endif

   return destination;
}

/*
 * Prints to a string buffer at most 'size' characters (including the terminating null byte)
 * Buffer is supposed big enough. You must check remaining buffer size. Otherwise:
 *  - on Windows, a buffer overflow will be emitted
 *  - on Linux, undefined behaviour, segmentation faults or stack smashing
 * \param str output string buffer (address of first byte where to start writing)
 * \param size maximum number of characters to write
 * \param format, ... (CF printf)
 * \return number of characters written or -1 in case of error
 */
int lc_sprintf(char * str, size_t size, const char * format, ...)
{
   int written_chars;
   va_list argptr;
   va_start(argptr, format);

#ifdef _WIN32
   written_chars = vsnprintf_s(str, size, size-1, format, argptr);
#else
   written_chars = vsnprintf(str, size, format, argptr);
#endif
   va_end(argptr);

   return written_chars;
}
