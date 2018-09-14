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

/*
 * Defines small functions shared by files here in lib/stub.
 * TODO: move some of them (most generic ones) upper in MAQAO => reuse them in another MAOAQ C/LUA modules
 */

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_HOSTNAME 1024
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <lua.h>
#include <lualib.h>


#include "libmcommon.h" // lc_strdup...
#include "utils.h"

// Parse a string and split each element into an array from 0 to nbElements-1
// str         The string to parse.
// delimiter   The delimiter to split the string.
// nbElements  The number of elements after the parse into the array.
// return      A char array of nbElements containing the split strings.
char** split_string (const char* str, const char delimiter, unsigned* nbElements)
{
   if (str == NULL)
      return NULL;

   char* copyStr = lc_strdup(str);
   char* curPos = copyStr;
   char* startWord = copyStr;
   *nbElements = 1;
   char** splitStr = NULL;
   unsigned i = 0;

   //DETERMINE THE NUMBER OF ELEMENTS IN THE LIST
   while ((curPos = strchr(curPos,delimiter)))
   {
      *nbElements = *nbElements + 1;
      curPos++;//to be placed just after the delimiter
   }

   //ALLOCATE THE RIGHT SIZE
   char** tmp = lc_realloc (splitStr, *nbElements * sizeof(char*));
   //REALLOC ERROR ?
   if ( tmp == NULL)
      return NULL;
   splitStr = tmp;

   //ADD THE ELEMENT(S) IN THE ARRAY
   curPos = copyStr;
   startWord = copyStr;
   while ( (curPos = strchr(curPos,delimiter)) )
   {
      *curPos = '\0'; //REPLACE EACH delimiter by '\0'
      splitStr[i] = lc_strdup(startWord);
      curPos++;
      startWord = curPos;
      i++;
   }
   //THE LAST ONE
   splitStr[i] = lc_strdup(startWord);
   free(copyStr);
   return splitStr;
}

/**
 * Utility function to retrieve an addess in decimal or binary form
 * \param longaddr The address in string format
 * \return The value of the address
 */
int64_t perf_utils_readhex (char* longaddr)
{
   int64_t val = 0;
   if(longaddr)
   {
      if ((strlen (longaddr) > 2) && ((longaddr[0] == '0') && (longaddr[1] == 'x')))   //Hexa
         sscanf(longaddr,"%"PRIx64,&val);
      else                                                                             //Decimal
         sscanf(longaddr,"%"PRId64,&val);
   }
   return val;
}

// Get the extension of a file
const char* get_filename_extension(const char *fileName)
{
   const char *dot = strrchr(fileName, '.');
   if(!dot)
      return "";
   else
      return dot + 1;
}

// Unused but could be useful to debug LUA stack
void stackDump(lua_State* l)
{
   int i;
   int top = lua_gettop(l);

   fprintf(stderr,"Total in stack %d\n",top);

   for (i = 1; i <= top; i++)
   {
      //EACH LEVEL OF THE STACK
      int t = lua_type(l, i);
      switch (t) {
      case LUA_TSTRING:
         fprintf(stderr,"string: '%s'\n", lua_tostring(l, i));
         break;
      case LUA_TBOOLEAN:
         fprintf(stderr,"boolean %d\n",lua_toboolean(l, i) ? 1 : 0);
         break;
      case LUA_TNUMBER:
         fprintf(stderr,"number: %g\n", lua_tonumber(l, i));
         break;
      default:
         fprintf(stderr,"%s\n", lua_typename(l, t));
         break;
      }
      fprintf(stderr," ");
   }
   fprintf(stderr,"\n");
}

// Open a file in a given directory
FILE *fopen_in_directory (const char *dir_name, const char *file_name, const char *mode)
{
   // Forge full name as <dir_name>/<file_name>
   char *full_name = lc_malloc (strlen (dir_name) + strlen (file_name) + 2);
   sprintf (full_name, "%s/%s", dir_name, file_name);

   // Open file
   FILE *fp = fopen (full_name, mode);
   if (!fp) {
      ERRMSG ("Cannot open %s in %s mode\n", full_name, mode);
      perror ("open");
   }
   lc_free (full_name);

   return fp;
}

static void for_each_in_directory (const char *path, unsigned char d_type,
                                   void (*f) (const char *path, const char *d_name, void *data),
                                   void *data)
{
   // Open the host directory
   DIR *const dir = opendir (path);
   if (!dir) return;

   // For each entry
   struct dirent *dir_entry;
   while ((dir_entry = readdir (dir)) != NULL) {
      const char *const d_name = dir_entry->d_name;
      // Ignoring "." (current directory) and ".." (parent directory)
      if (strcmp (d_name, ".") == 0 || strcmp (d_name, "..") == 0) continue;
      // Ignoring invalid type entries
      if (dir_entry->d_type == DT_UNKNOWN) {
         struct stat st_buf;
         char *d_path = lc_malloc (strlen (path) + strlen (d_name) + 2);
         sprintf (d_path, "%s/%s", path, d_name);
         int ret = stat (d_path, &st_buf);
         lc_free (d_path);
         if (ret == -1) {
            DBG(fprintf (stderr, "Cannot guess %s type (directory, regular file etc.)\n", d_name); \
                perror ("stat");)
            continue;
         }
         if (d_type == DT_DIR && !S_ISDIR (st_buf.st_mode)) continue;
         if (d_type == DT_REG && !S_ISREG (st_buf.st_mode)) continue;
      } else
         if (dir_entry->d_type != d_type) continue;

      f (path, d_name, data);
   }

   closedir (dir);
}

/* Calls a function for each file inside a given directory
 * \param path path to directory
 * \param f function to call (arg1=path, arg2=<file name>, arg3=data)
 * \param data data to pass to f (generic: void *) */
void for_each_file_in_directory (const char *path,
                                 void (*f) (const char *path, const char *d_name, void *data),
                                 void *data)
{
   for_each_in_directory (path, DT_REG, f, data);
}

/* Similar to for_each_file_in_directory but function receives a directory name as second argument */
void for_each_directory_in_directory (const char *path,
                                      void (*f) (const char *path, const char *d_name, void *data),
                                      void *data)
{
   for_each_in_directory (path, DT_DIR, f, data);
}
