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
#include <io.h>
#include <time.h>
#else
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <dirent.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include "libmcommon.h"
#include "config.h"

#define FILE_AND_DIR 0777 /* Previously used: S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH */
#define MAQAO_FILES_PATH "/share/maqao/" /**\todo to remove with prefixed_path_to */

///////////////////////////////////////////////////////////////////////////////
//                             time functions                                //
///////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
/* FILETIME of Jan 1 1970 00:00:00. */
static const unsigned __int64 epoch = ((unsigned __int64)116444736000000000ULL);
/*
 * timezone information is stored outside the kernel so tzp isn't used anymore.
 *
 */
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
   (void)tzp;

   FILETIME file_time;
   SYSTEMTIME system_time;
   ULARGE_INTEGER ularge;

   GetSystemTime(&system_time);
   SystemTimeToFileTime(&system_time, &file_time);
   ularge.LowPart = file_time.dwLowDateTime;
   ularge.HighPart = file_time.dwHighDateTime;

   tp->tv_sec = (long)((ularge.QuadPart - epoch) / 10000000L);
   tp->tv_usec = (long)(system_time.wMilliseconds * 1000);

   return 0;
}
#endif
/*
 * Get the user time
 * \return time in micro secondes
 */
unsigned long long int utime(void)
{
   struct timeval tv;

   gettimeofday(&tv, NULL);

   return tv.tv_sec * 1000000 + tv.tv_usec;
}

/*
 * Generates a timestamp formatted as (year)-(month)-(day)_(hour)-(minutes)-(seconds)_(microseconds)
 * If the microseconds are not available they will be omitted
 * \param str String containing the result. Must have already been initialised
 * \param str_len Size of the string
 * \return EXIT_SUCCESS/EXIT_FAILURE
 * */
int generate_timestamp(char* str, size_t str_len)
{
   if (str == NULL || str_len == 0)
      return EXIT_FAILURE;
   time_t timer = time(NULL);
   struct tm *timeptr = localtime(&timer);
   size_t stored = strftime(str, str_len, "%Y-%m-%d_%H-%M-%S", timeptr);
   if (stored == 0)
      return EXIT_FAILURE; //strftime failed in some way
   if ((stored + 8) > str_len)
      return EXIT_SUCCESS; //No space left to write the microseconds
   struct timeval t;
   int res = gettimeofday(&t, NULL);
   if (res == 0) {
      sprintf(str + stored, "_%"PRIu64, t.tv_usec);
   }
   return EXIT_SUCCESS;

}

///////////////////////////////////////////////////////////////////////////////
//                             hash functions                                //
///////////////////////////////////////////////////////////////////////////////
/*
 * Hashes a string
 * \param h an original value
 * \param c a string to hash
 * \return the hash value of the string
 */
unsigned long add_hash(unsigned long h, char *c)
{
   while (*c != '\0')
      h = h * 3 + *c++;

   return h;
}

/*
 * Hashes an file
 * \param filename a file to hash
 * \return the hash value of a file
 */
unsigned long file_hash(char *filename)
{
   struct stat st;

   if (stat(filename, &st)) {
      perror(filename);
      exit(1);
   }

   return add_hash((unsigned long) st.st_mtime, filename);
}

///////////////////////////////////////////////////////////////////////////////
//                           general functions                               //
///////////////////////////////////////////////////////////////////////////////

/*
 * Adds prefixs to a path. This prefix is:
 *  <local_install_dir>/share/maqao
 * \param subpath a path to prefix
 * \return the prefixed path
 */
char* prefixed_path_to(char* subpath)
{
   /**\todo Check that this function is still used and if it does not need to be adapted to the new architecture*/
   assert(subpath);

   int path_len = (strlen(PREFIX) + strlen(MAQAO_FILES_PATH) + strlen(subpath)
         + 1) * sizeof(char);
   char *full_path = lc_malloc(path_len);
   lc_sprintf(full_path, path_len, "%s%s%s", PREFIX, MAQAO_FILES_PATH, subpath);

   return full_path;
}

/*
 * Creates a new directory
 * \param name name of the directory to create
 * \param mode permissions for the directory
 * \return TRUE if the directory has been created, else FALSE
 */
int createDir(const char *name, int mode)
{
   if (!name)
      return FALSE;

   char *cp;
   char *cpOld;
   char *buf = lc_strdup(name);
   int retVal = 0;

   for (cp = buf; *cp == '/'; cp++)
      ;
   cp = strchr(cp, '/');

   while (cp) {
      cpOld = cp;
      cp = strchr(cp + 1, '/');
      *cpOld = '\0';
#ifdef _WIN32
      retVal = _mkdir(buf, cp ? FILE_AND_DIR : mode);
#else
      retVal = mkdir(buf, cp ? FILE_AND_DIR : mode);
#endif

      if (retVal != 0 && errno != EEXIST) {
         perror(buf);
         lc_free(buf);
         return FALSE;
      }

      *cpOld = '/';
   }

   lc_free(buf);
   return TRUE;
}

/*
 * Creates a new file
 * \param file name of the file to create
 * \return TRUE if the file has been created, else FALSE
 */
int createFile(char * file)
{
   if (!file)
      return FALSE;

   int exist = TRUE;
   int dir_length = sizeof(char) * (strrchr(file, '/') - file + 2);
   char *dir = lc_malloc0(dir_length);

   lc_strncpy(dir, dir_length, file, strrchr(file, '/') - file + 1);
   dir[strrchr(file, '/') - file + 1] = '\0';

   if (!dirExist(dir)) {
      /* directory is missing (and so database file too) */
      exist = FALSE;
      createDir(dir, FILE_AND_DIR);
#ifdef _WIN32
      int f = _open(file, _O_CREAT, _S_IREAD | _S_IWRITE);
      _close(f);
#else
      int f = open(file, O_CREAT, FILE_AND_DIR);
      close(f);
#endif
   } else if (!fileExist(file)) {
      /* directory already existing, but database missing */
      exist = FALSE;
#ifdef _WIN32
      int f = _open(file, _O_CREAT, _S_IREAD | _S_IWRITE);
      _close(f);
#else
      int f = open(file, O_CREAT, FILE_AND_DIR);
      close(f);
#endif
   }

   lc_free(dir);
   return exist;
}

/*
 * Deletes a file
 * \param file name of the file to deletes
 * \return TRUE if the file has been deleted, else FALSE
 */
int fileDelete(char* file)
{
#ifdef _WIN32
   if (!file || _unlink(file) == -1)
#else
   if (!file || unlink(file) == -1)
#endif
      return FALSE;

   return TRUE;
}

/*
 * Checks if a file
 * \param file name of the file to checks
 * \return TRUE if the file exists, else FALSE
 */
int fileExist(char* file)
{
   if (!file)
      return FALSE;

   FILE* filedesc;
#ifdef _WIN32
   fopen_s(&filedesc, file, "r");
#else
   filedesc = fopen(file, "r");
#endif

   if (filedesc) {
      fclose(filedesc);
      return TRUE;
   }

   return FALSE;
}

/*
 * Checks if a directory
 * \param dir name of the directory to checks
 * \return TRUE if the directory exists, else FALSE
 */
int dirExist(char* dir)
{
   if (!dir)
      return FALSE;

#ifdef _WIN32
   if (OpenFile(dir, NULL, OF_EXIST) != HFILE_ERROR) {
#else
   DIR* dirdesc = opendir(dir);

   if (dirdesc) {
      closedir(dirdesc);
#endif
      return TRUE;
   }

   return FALSE;
}

/*
 * Gets the path of a file
 * \param filename a file name
 * \return the path of the file (everything before the last '/' or "." if it did not contain any '/') or NULL if there is a problem
 */
char* getPath(char* filename)
{
   if ((!filename) || (filename[0] == '\0')) {
      DBG(printf("getPath(NULL)=NULL\n")
      ;);
      return NULL;
   }

   int i = strlen(filename);
   char* path = lc_strdup(filename);

   while (i > 0) {
      if (path[i] == '/')
         path[i] = '\0';

      if (dirExist(path)) {
         DBG(printf("getPath(\"%s\")=%s\n", filename, path)
         ;);
         return path;
      }

      i--;
   }
   if (i == 0) { //No valid directory found in the whole filename: maybe filename contains only the file name and not its path
      path[i] = '.';
      path[i + 1] = '\0'; //We know path[1] exists as we checked first that filename is at least of length 1
      if (dirExist(path)) {
         DBG(printf("getPath(\"%s\")=%s\n", filename, path)
         ;);
         return path;
      }
   }

   return NULL;
}

/*
 * Gets a file basename (substring between the last '/' and before the last '.')
 * \param filename a file name
 * \return the base name (file name without its path and extension), or NULL if there is a problem
 */
char* getBasename(char* filename)
{
   if (!filename) {
      DBG(printf("getBasename(NULL)=NULL\n")
      ;);
      return NULL;
   }

   char* basefile;
   char* lastslash, *lastdot;

   lastslash = strrchr(filename, '/');

   if (lastslash != NULL) {
      basefile = lc_malloc(strlen(lastslash) - 1 + 1);
      strcpy(basefile, lastslash + 1);
   } else {
      basefile = lc_malloc(strlen(filename) + 1);
      strcpy(basefile, filename);
   }

   lastdot = strrchr(basefile, '.');

   if (lastdot != NULL)
      *lastdot = '\0';

   DBG(printf("getBasename(\"%s\")=%s\n", filename, basefile)
   ;);

   return basefile;
}

/*
 * Removes a subpath from a path
 * \param path a path
 * \param basepath a subpath
 * \return the modified path
 */
char* removeBasepath(char* path, char* basepath)
{
   assert(path != NULL);
   assert(basepath != NULL);

   if (str_equal(path, basepath)) {
      DBG(
            printf("removeBasepath(\"%s\",\"%s\")=%s\n", path, basepath,
                  path + strlen(basepath))
            ;);
      return path + strlen(basepath);
   }

   int diff;
   char* pathdup = lc_strdup(path);

   // going to the last '/'
   for (diff = strlen(path) - 1; diff >= 0 && path[diff] != '/'; diff--)
      ;

   while (diff > 0) {
      if (path[diff] == '/') {
         pathdup[diff] = '\0';
         if (str_equal(pathdup, basepath))
            break;
      }

      diff--;
   }

   DBG(
         printf("removeBasepath(\"%s\",\"%s\")=%s\n", path, basepath,
               diff == 0 ? path : path + diff + 1)
         ;);
   lc_free(pathdup);

   if (diff > 0)
      return path + diff + 1; // return the pointer to the different part excluding the '/'

   return path;
}

/*
 * Gets the common path between two paths
 * \param filename1 a path
 * \param filename2 a path
 * \param commonpath will contain the common path between filename1 and filename2
 * \return TRUE if a common path has been computed, else FALSE
 */
int commonPath(char* filename1, char* filename2, char* commonpath)
{
   if (!filename1 || !filename2) {
      DBG(printf("commonPath(\"NULL?\",\"NULL?\")=NULL.\n")
      ;);
      return FALSE;
   }

   char *path1 = getPath(filename1);
   char *path2 = getPath(filename2);
   size_t i = 0;
   int lastslash = -1;

   strcpy(commonpath, "");
   while (i < strlen(path1) && i < strlen(path2) && path1[i] == path2[i]) {
      if (path1[i] == '/')
         lastslash = i;
      i++;
   }
   strncpy(commonpath, path1, i);

   if (lastslash != -1 && i != strlen(path1) && i != strlen(path2))
      commonpath[lastslash] = '\0';
   else
      commonpath[i] = '\0';

   if (i == 0)
      strcpy(commonpath, "/");

   DBG(
         printf("commonPath(\"%s(%s)\",\"%s(%s)\")=%s\n", filename1, path1,
               filename2, path2, commonpath)
         ;);

   lc_free(path1);
   lc_free(path2);

   return TRUE;
}

/*
 * Opens a file, maps its contents to memory, and returns a pointer to the string containing it
 * \param filename The name of the file
 * \param filestream Must not be NULL. If pointing to a non-NULL value, will be used as the stream to the file.
 * Otherwise, will be updated to contain the file stream of the file (needed to be closed)
 * \param contentlen If not NULL, will contain the length of the string retrieved from the file
 * \return Pointer to the character stream contained in the file, or NULL if an error occurred, or if \c filename or \c filestream are NULL.
 * The stream must be released using \ref releaseFileContent
 * */
char* getFileContent(char* filename, FILE **filestream, size_t* contentlen)
{
   if (!filename || !filestream)
      return NULL;

   int bytes_read, stream_size;
   FILE* stream;
   char* mm = NULL;

   //Checking the file stream or opening the file
   if (*filestream == NULL) {
#ifdef _WIN32
      fopen_s(&stream, filename, "rb");
#else
      stream = fopen(filename, "rb");
#endif
   } else {
      stream = *filestream;
   }
   if (stream == NULL) {
      DBGMSG("Unable to open file %s\n", filename);
      return NULL;
   }

   //Getting the file size
   fseek(stream, 0, SEEK_END);
   stream_size = ftell(stream);
   fseek(stream, 0, SEEK_SET);

   mm = (char*) lc_malloc0(sizeof(char) * (stream_size + 1));
   if (mm == NULL) {
      DBGMSG0("Memory allocation error\n");
   }

   //Getting the file content
   bytes_read = fread(mm, sizeof(char), stream_size, stream);
   //TODO We have issues with CRLF when reading a file...
#ifndef _WIN32
   if (bytes_read != stream_size) {
      DBGMSG("Reading error while opening %s\n", filename);
      return NULL;
   }
#endif
   //Ensuring the string ends with a \0 character
   mm[stream_size] = '\0';

   *filestream = stream;
   if (contentlen)
      *contentlen = stream_size;

   return mm;
}

/*
 * Closes a file opened with \ref getFileContent, and unmaps the string from its content
 * \param content Character stream contained in the file (returned by \ref getFileContent)
 * \param filestream The file descriptor (returned by \ref getFileContent or provided to it)
 * */
void releaseFileContent(char* content, FILE* filestream)
{
   if (content)
      lc_free(content);

   if (filestream != NULL)
      fclose(filestream);
}
