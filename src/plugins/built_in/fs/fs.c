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

#ifdef _WIN32
#define LUA __declspec(dllexport)
#include <io.h>
#include <direct.h>
#include <windows.h>
#else
#define LUA
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "libmcommon.h"

static int l_read_dir (lua_State * L)
{
  int i = 0;
  char *buffer = NULL;
  size_t buffer_size = 0;
  const char *path = luaL_checkstring (L, 1);

#ifdef _WIN32
  struct _finddata_t c_file;
  intptr_t hFile;
  char pattern[MAX_PATH + 1];

  if (strlen(path) > MAX_PATH - 2)
     //Path too long
     return 0;
  else
     lc_sprintf(pattern, MAX_PATH + 1, "%s/*", path);

  //Looking for a file
  hFile = _findfirst(pattern, &c_file);
  if (hFile == -1L)
     //No file in the directory
     return 0;

  lua_newtable(L);
  do
  {
     char t = 0;

     //Directory
     if (c_file.attrib & _A_SUBDIR)
        t = 1;
     //"Regular" file
     else if (c_file.attrib & _A_NORMAL || c_file.attrib & _A_RDONLY)
        t = 2;

     if (t)
     {
        lua_pushnumber(L, ++i);	/* push key */
        lua_createtable(L, 0, 2);
        lua_pushstring(L, "type");
        lua_pushinteger(L, t);
        lua_rawset(L, -3);	/* table[key].file=value */
        lua_pushstring(L, "name");
        lua_pushstring(L, c_file.name);	/* push value */
        lua_rawset(L, -3);	/* table[key].name=value */
        lua_rawset(L, -3);	/* table[key]={file,name} */
     }

  } while (_findnext(hFile, &c_file) != -1L);

  _findclose(hFile);

#else
  DIR *dir;
  struct dirent *entry;
  struct stat st;

  /* open directory */
  dir = opendir (path);

  if (dir == NULL)
    {				/* error opening the directory? */
      return 0;			/* number of results */
    }
  i = 1;
  lua_newtable (L);
  /* create result table */
  while ((entry = readdir (dir)))
    {
      char t = 0;
      if ((buffer == NULL)
	  || (strlen (buffer) < strlen (entry->d_name) + strlen (path) + 2))
	{
         buffer_size = strlen(entry->d_name) + strlen(path) + 2;
         buffer = (char *)realloc(buffer, buffer_size);
	}
      lc_sprintf (buffer, buffer_size, "%s/%s", path, entry->d_name);
      if (!stat (buffer, &st))
	{
	  if (S_ISREG (st.st_mode))
	    {
	      t = 2;
	    }
	  else if (S_ISDIR (st.st_mode))
	    {
	      t = 1;
	    }
	  if (t)
	    {
	      lua_pushnumber (L, i++);	/* push key */
	      lua_createtable (L, 0, 2);
	      lua_pushstring (L, "type");
	      lua_pushinteger (L, t);
	      lua_rawset (L, -3);	/* table[key].file=value */
	      lua_pushstring (L, "name");
	      lua_pushstring (L, entry->d_name);	/* push value */
	      lua_rawset (L, -3);	/* table[key].name=value */
	      lua_rawset (L, -3);	/* table[key]={file,name} */
	    }
	}
    }
  free (buffer);
  closedir (dir);
#endif

  return 1;			/* table is already on top */
}


static int l_read_file (lua_State * L)
{
   char *buffer;
   const char *filename = luaL_checkstring (L, 1);
   int fd;
   struct stat st;

   if (!stat (filename, &st))
   {
#ifdef _WIN32
      fd = _open(filename, _O_RDONLY);
#else
      fd = open (filename, O_RDONLY);
#endif
      if (fd < 0)
         return 0;
      buffer = (char *) malloc (st.st_size);
#ifdef _WIN32
      if (_read (fd, buffer, st.st_size) < 0)
#else
      if (read (fd, buffer, st.st_size) < 0)
#endif
      {
         free (buffer);
         fprintf (stderr, "error");
         return 0;
      }

      lua_pushstring (L, buffer);
      free (buffer);
#ifdef _WIN32
      _close(fd);
#else
      close (fd);
#endif
      return 1;
   }
   else
      return 0;
}

static int l_count_lines (lua_State * L)
{
   char *buffer, *c;
   const char *filename = luaL_checkstring (L, 1);
   int fd;
   struct stat st;
   int i;
   if (!stat (filename, &st))
   {
#ifdef _WIN32
      fd = _open(filename, _O_RDONLY);
#else
      fd = open (filename, O_RDONLY);
#endif
      if (fd < 0)
      {
         lua_pushinteger (L, 0);
         return 1;
      }
      buffer = (char *) malloc (st.st_size);
#ifdef _WIN32
      if (_read (fd, buffer, st.st_size) < 0)
#else
      if (read (fd, buffer, st.st_size) < 0)
#endif
      {
         free (buffer);
         lua_pushinteger (L, 0);
         return 1;
      }
      for (i = 0, c = buffer; c != NULL; i++)
      {
         c = strchr (c + 1, '\n');
      }
      lua_pushinteger (L, i);
      free (buffer);
#ifdef _WIN32
      _close(fd);
#else
      close (fd);
#endif
      return 1;
   }
   else
   {
      lua_pushinteger (L, 0);
      return 1;
   }
}

static int l_read_line (lua_State * L)
{
   char buffer[4096];
   const char *filename = luaL_checkstring (L, 1);
   const int line = luaL_checkinteger (L, 2);
   FILE *fd;
   struct stat st;
   int i = 1;

   if (!stat (filename, &st))
   {
#ifdef _WIN32
      fopen_s(&fd, filename, "r");
#else
      fd = fopen (filename, "r");
#endif
      if (fd == NULL)
         return 0;

      while (i < line && fgets (buffer, 4096, fd) != NULL)
         i++;

      if (fgets (buffer, 4096, fd) == NULL)
         return 0;

      lua_pushstring (L, buffer);
      fclose (fd);
      return 1;
   }
   else
      return 0;
}

static int l_exists(lua_State * L)
{
	const char *filename = luaL_checkstring (L, 1);

#ifdef _WIN32
   if (_access(filename, 0) == 0)
      lua_pushboolean(L, 1);
   else
      lua_pushboolean(L, 0);
#else
   FILE *fd;
	if((fd = fopen(filename, "r")) == NULL)
	  lua_pushboolean(L,0);
	else
	{
	  lua_pushboolean(L,1);
	  fclose(fd);
	}
#endif
	return 1;
}

static int l_open(lua_State * L)
{
	const char *filename = luaL_checkstring (L, 1);
	const char *mode = luaL_checkstring (L, 2);
	FILE *fd;

	if(strcmp(filename,"stderr") == 0)
		fd = stderr;
	else if(strcmp(filename,"stdout") == 0)
		fd = stdout;
	else
#ifdef _WIN32
	   fopen(&fd, filename, mode);
#else
		fd = fopen(filename, mode);
#endif

	if(fd == NULL)
		lua_pushnil(L);
	else
		lua_pushlightuserdata(L,fd);

	return 1;
}

static int l_close(lua_State * L)
{
	FILE *fd = lua_touserdata(L, 1);

	if(fd != stdout && fd != stderr && fd != stdin)
		fclose(fd);

	return 0;
}

static int l_basename(lua_State * L)
{
  char *lua_string = (char *)luaL_checkstring (L, 1);
  // Duplicate the string to prevent any modifications (due to the basename function)
  // cf BUG #3847
#ifdef _WIN32
  char *path = _strdup(lua_string);
#else
  char *path = strdup(lua_string);
#endif

  if(strcmp(path,"") != 0)
	  lua_pushstring(L,lc_basename(path));
  else
  	lua_pushliteral(L,"");

  return 1;
}

static int l_dirname(lua_State * L)
{
  char *path   = (char *)luaL_checkstring (L, 1);
  char dir_sep = '/';
  char *tmp;
  //Replace dirname's default behavior: dirname(/usr/bin/)=/usr but we want /usr/bin
  if(path[strlen(path)-1] == dir_sep)
  {
#ifdef _WIN32
     tmp = _strdup(path);
#else
	  tmp = strdup(path);
#endif
	  tmp[strlen(path)-1] = '\0';
	  lua_pushstring(L,tmp);
	  free(tmp);
  }
  else if(strcmp(path,"") != 0)
  {
#ifdef _WIN32
     tmp = _strdup(path);
#else
     tmp = strdup(path);
#endif
	  lua_pushstring(L, lc_dirname(tmp));
	  free(tmp);
  }
  else
  	lua_pushliteral(L,"");

  return 1;
}

static int l_chmod (lua_State * L)
{
   char* path  = (char *)luaL_checkstring (L, 1);
   char* pstr  = (char *)luaL_checkstring (L, 2);
   int mode    = 0;

#ifdef _WIN32
   if (pstr[0] == 'r')
      mode |= _S_IREAD;
   if (pstr[1] == 'w')
      mode |= _S_IWRITE;

   if (_chmod(path, mode) < 0)
      lua_pushboolean (L, 0);
   else
      lua_pushboolean (L, 1);
#else
   if (pstr[0] == 'r')
      mode |= S_IRUSR;
   if (pstr[1] == 'w')
      mode |= S_IWUSR;
   if (pstr[2] == 'x')
      mode |= S_IXUSR;

   if (pstr[3] == 'r')
      mode |= S_IRGRP;
   if (pstr[4] == 'w')
      mode |= S_IWGRP;
   if (pstr[5] == 'x')
      mode |= S_IXGRP;

   if (pstr[6] == 'r')
      mode |= S_IROTH;
   if (pstr[7] == 'w')
      mode |= S_IWOTH;
   if (pstr[8] == 'x')
      mode |= S_IXOTH;

   if (chmod (path, mode) < 0)
      lua_pushboolean (L, 0);
   else
      lua_pushboolean (L, 1);
#endif

   return 1;
}


static int l_fsize (lua_State * L)
{
   struct stat st;
   const char* filename = luaL_checkstring (L, 1);
   stat(filename, &st);
   lua_pushinteger (L, st.st_size);
   return 1;
}


static const struct luaL_Reg fs[] = {
  {"readdir", l_read_dir},
  {"readfile", l_read_file},
  {"readline", l_read_line},
  {"countlines", l_count_lines},
  {"exists", l_exists},
  {"open", l_open},
  {"close", l_close},
  {"basename", l_basename},
  {"dirname", l_dirname},
  {"chmod", l_chmod},
  {"fsize", l_fsize},
  {NULL, NULL}
};

LUA int luaopen_fs(lua_State * L)
{
  luaL_register (L, "fs", fs);
  return 1;
}
