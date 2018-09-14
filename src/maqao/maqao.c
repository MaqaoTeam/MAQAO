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

#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "libmcommon.h"
#include "lua_exec.h"

#define MAQAO_ERROR_ROOT_STR "MAQAO> "

#ifndef _WIN32
extern int madras_main(int argc, char **argv);
#endif

void usage(void)
{
   printf(
         "Usage : maqao [module=<module>|madras|a_script_to_execute_in lua_environment.lua]\n");
}

int launch_maqao_lua(int argc, char **argv)
{
   lua_State *context = NULL;
   char *buffer = lc_malloc(sizeof(char));
   int i;
   char* lua_msg;
   int status = EXIT_SUCCESS;

   context = init_maqao_lua();
   if (context == NULL) {
      ERRMSG("Lua context initialization failed\n");
   }
   //Set arguments
   int buffer_size = 0;
   for (i = 0; i < argc; i++) {
      char* buf = str_replace(argv[i], "\\", "\\\\");
      char* buf2 = str_replace(buf, "\"", "\\\"");
      buffer_size = (20 + strlen(buf2)) * sizeof(char);
      buffer = lc_realloc(buffer, buffer_size);
      lc_sprintf(buffer, buffer_size, "arg[%d] = \"%s\";", i, buf2);
      lc_free(buf);
      lc_free(buf2);

      lua_msg = lua_exec_str(context, buffer, 0, "set_param");
      if (lua_msg != NULL) {
         STDMSG("%s", lua_msg);
         lc_free(buffer);
         lc_free(lua_msg);
         return ERR_LUAEXE_RUNTIME_ERROR;
      }
   }
   lc_free(buffer);



#include "lua_mainmodule.lua.b64.c"
   //Launch MAQAO Lua main module
   lua_msg = lua_exec_str(context, decode(lua_mainmodule, lua_mainmodule_size),
               lua_mainmodule_size, lua_mainmodule_name);
   if (lua_msg != NULL) {
      STDMSG("%s", lua_msg);
      lc_free(lua_msg);
      status = ERR_LUAEXE_RUNTIME_ERROR;
   }
   // Going through the text interface
   //MESSAGE("Starting internal interpreter shell ...\n");
   //MESSAGE("You can now type your queries\n\n");
   /*
    char pexit = 'n';
    while (pexit != 'o')
    {
    memset(&buff,(int)'\0',sizeof(buff));
    MESSAGE("MAQAO> ");
    if(fgets(buff, sizeof(buff), stdin) != NULL)
    {
    if(strcmp(buff,"exit\n") == 0){
    pexit='o';
    }else{
    printf("%s",lua_exec(context,buff,0,"line"));
    }
    }
    else
    break;
    }
    */
   lua_close(context);

   return status;
}

int main(int argc, char **argv)
{
   int out = EXIT_SUCCESS;
   char *new_argc[argc - 1];

   DBGMSG0("Into MAQAO C main function\n");

   if (argc > 1 && argv[1] != NULL) {
      // module=madras => special case
      if (strcmp(argv[1], "module=madras") == 0
            || strcmp(argv[1], "madras") == 0) {
         int i;
         for (i = 1; i < argc; i++) {
            new_argc[i - 1] = argv[i];
         }
#ifndef _WIN32
         return madras_main(argc - 1, new_argc);
#endif
      }
   }

   out = launch_maqao_lua(argc, argv);

   return out;
}

