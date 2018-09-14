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
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lua_exec.h"
#include "libmcommon.h"
#include "libmmaqao.h"

/**
 * Loads and executes a Lua code chunk contained in a text buffer
 * \param params An instance of microbench wrapper structure
 * \return An error string
 * */
int maqao_launch_microbench(wrapper_microbench_params_t params)
{
   lua_State *context = NULL;
   char *arguments = NULL;
   char *empty_str_arg = "";
   char* arguments_tmpl;
   char* lua_msg = NULL;
   int error;

   if (params.config_file == NULL) {
      params.config_file = empty_str_arg;
   }

   if (params.config_template == NULL) {
      params.config_template = empty_str_arg;
   }

   if (params.arch == NULL) {
      params.arch = empty_str_arg;
   }

   context = init_maqao_lua();
   if (context == NULL) {
      ERRMSG("Lua context initialization failed\n");
   }

   arguments = "proj = project.new(\"microbench\");";
   lua_msg = lua_exec_str(context, arguments, 0, "init_project");
   if (lua_msg != NULL) {
      STDMSG("%s", lua_msg);
      lc_free(lua_msg);
   }
   DBGMSG("%s\n", arguments);

   arguments_tmpl = "args = {}; args.config = \"%s\"; args.arch = \"%s\"; args.configtemplate = \"%s\"; ";
   int arguments_length = sizeof(char) * strlen(params.config_file)
         + strlen(params.arch) + sizeof(char) * strlen(params.config_template)
         + sizeof(char) * strlen(arguments_tmpl) + 1;
   arguments = lc_malloc(arguments_length);
   lc_sprintf(arguments, arguments_length, arguments_tmpl, params.config_file,
         params.arch, params.config_template);
   error = lua_exec(context, arguments, 0, "set_main_args");
   DBGMSG("%s\n", arguments);
   lc_free(arguments);

   switch(params.mode) {
   case MICROBENCH_GEN_RUN:
      arguments = "";
      break;
   case MICROBENCH_GEN_ONLY:
      arguments = "args[\"generate-only\"]=true;";
      break;
   case MICROBENCH_RUN_ONLY:
      arguments = "args[\"run-only\"]=true;";
      break;
   default:
      WRNMSG("Unsupported value for microbench generation mode: %d. Ignoring\n", params.mode);
      arguments = "";
   }

   if (strlen(arguments) > 0) {
      error = lua_exec(context, arguments, 0, "set_gen_mode");
      DBGMSG("%s\n", arguments);
   }

   arguments = "Message:disable(); Message:set_exit_mode('lib'); microbench:microbench_launch(args,proj);";
   error = lua_exec(context, arguments, 0, "launch_microbench");
   DBGMSG("%s\n", arguments);

   arguments = "proj:free();";
   lua_msg = lua_exec_str(context, arguments, 0, "close_project");
   if (lua_msg != NULL) {
      STDMSG("%s", lua_msg);
      lc_free(lua_msg);
   }
   DBGMSG("%s\n", arguments);

   lua_close(context);

   return error;
}

/**
 * Wrapper function to launch the cqa module
 * \param params An instance CQA Wrapper structure
 * \return An error code
 */
int maqao_launch_cqa(wrapper_cqa_params_t params)
{

   lua_State *context = NULL;
   char *arguments = NULL;
   char *empty_str_arg = "";
   int error;
   char* arguments_tmpl;
   size_t arguments_length = 0;

   if (params.arch == NULL) {
      params.arch = empty_str_arg;
   }
   if (params.uarch_name == NULL) {
      params.uarch_name = empty_str_arg;
   }
   if (params.ml == NULL) {
      params.ml = empty_str_arg;
   }
   if (params.mlf_insn == NULL) {
      params.mlf_insn = empty_str_arg;
   }
   if (params.mlf_pattern == NULL) {
      params.mlf_pattern = empty_str_arg;
   }
   if (params.asm_input_file == NULL) {
      params.asm_input_file = empty_str_arg;
   }
   if (params.csv_output_file == NULL) {
      params.csv_output_file = empty_str_arg;
   }

   context = init_maqao_lua();
   if (context == NULL) {
      ERRMSG("Lua context initialization failed\n");
   }

   arguments_tmpl = "proj = project.new(\"cqa\"); proj:init_proc_infos(\"%s\", \"%s\", \"%s\");";
   arguments_length = sizeof(*arguments)
         * (strlen(arguments_tmpl) + strlen(params.asm_input_file)
               + strlen(params.arch) + strlen(params.uarch_name) + 1);
   arguments = lc_malloc(arguments_length);
   lc_sprintf(arguments, arguments_length, arguments_tmpl,
         params.asm_input_file, params.arch, params.uarch_name);
   error = lua_exec(context, arguments, 0, "init_project");
   DBGMSG("%s\n", arguments);
   lc_free(arguments);

   if (ISERROR(error) == FALSE) {
      arguments_tmpl = "args = {}; args.bin = \"%s\"; args.arch = \"%s\""
            "args.uarch = \"%s\"; args.of = \"csv\";args.op = \"%s\";"
            "args.ml = \"%s\"; args.mlf = \"%s\";";
      arguments_length = sizeof(*arguments)
            * (strlen(arguments_tmpl) + strlen(params.asm_input_file)
                  + strlen(params.arch) + strlen(params.uarch_name)
                  + strlen(params.csv_output_file) + strlen(params.ml)
                  + strlen(params.mlf_insn) + 1);

      arguments = lc_malloc(arguments_length);
      lc_sprintf(arguments, arguments_length, arguments_tmpl,
            params.asm_input_file, params.arch, params.uarch_name,
            params.csv_output_file, params.ml, params.mlf_insn);
      error = lua_exec(context, arguments, 0, "set_args");
      DBGMSG("%s\n", arguments);
      lc_free(arguments);
   }

   if (ISERROR(error) == FALSE && params.mlf_pattern != NULL) {
      if (params.mlf_insn == NULL) {
         arguments_tmpl = "args.mlf = \"%s\";";
      } else {
         arguments_tmpl = "args.mlf = args.mlf..','..\"%s\";";
      }
      arguments_length = sizeof(*arguments) * (strlen(arguments_tmpl) + strlen(params.mlf_pattern) + 1);
      arguments = lc_malloc(arguments_length);
      lc_sprintf(arguments, arguments_length, arguments_tmpl, params.mlf_pattern);
      error = lua_exec(context, arguments, 0, "set_arg_user_data");
      DBGMSG("%s\n", arguments);
      lc_free(arguments);
   }

   if (ISERROR(error) == FALSE && params.user != NULL) {
      arguments_tmpl = "args.ud = \"%s\";";
      arguments_length = sizeof(*arguments) * (strlen(arguments_tmpl) + strlen(params.user) + 1);
      arguments = lc_malloc(arguments_length);
      lc_sprintf(arguments, arguments_length, arguments_tmpl, params.user);
      error = lua_exec(context, arguments, 0, "set_arg_user_data");
      DBGMSG("%s\n", arguments);
      lc_free(arguments);
   }

   if (ISERROR(error) == FALSE && params.mode >= 0 && params.mode <= 2 && params.value != NULL) {
      switch (params.mode) {
      case 0:
         arguments_tmpl = "args.l = \"%s\";";
         break;
      case 1:
         arguments_tmpl = "args.fl = \"%s\";";
         break;
      case 2:
         arguments_tmpl = "args.f = \"%s\";";
         break;
      }
      arguments_length = sizeof(*arguments) * (strlen(arguments_tmpl) + strlen(params.value) + 1);
      arguments = lc_malloc(arguments_length);
      lc_sprintf(arguments, arguments_length, arguments_tmpl, params.value);
      error = lua_exec(context, arguments, 0, "set_arg_mod_value");
      DBGMSG("%s\n", arguments);
      lc_free(arguments);
   }

   if (ISERROR(error) == FALSE && params.vunroll > 0) {
      arguments_tmpl = "args.vu = \"%d\";";
      arguments_length = sizeof(*arguments) * (strlen(arguments_tmpl) + 4 + 1); //Assuming the int will not be printed over more than 4 digits
      arguments = lc_malloc(arguments_length);
      lc_sprintf(arguments, arguments_length, arguments_tmpl, params.vunroll);
      error = lua_exec(context, arguments, 0, "set_arg_vu");
      DBGMSG("%s\n", arguments);
      lc_free(arguments);
   }

   if (ISERROR(error) == FALSE && params.fc != NULL) {
      arguments_tmpl = "args.fc = \"%s\";";
      arguments_length = sizeof(*arguments) * (strlen(arguments_tmpl) + strlen(params.fc) + 1);
      arguments = lc_malloc(arguments_length);
      lc_sprintf(arguments, arguments_length, arguments_tmpl, params.fc);
      error = lua_exec(context, arguments, 0, "set_arg_fc");
      DBGMSG("%s\n", arguments);
      lc_free(arguments);
   }

   if (ISERROR(error) == FALSE) {
      arguments = "Message:disable(); Message:set_exit_mode('lib');cqa:cqa_launch(args,proj);";
      error = lua_exec(context, arguments, 0, "launch_cqa");
      DBGMSG("%s\n", arguments);
      //STDMSG("%s", lua_exec_str(context,arguments,0,"launch_cqa"));
   }
   arguments = "proj:free();";
   char* lua_msg = lua_exec_str(context, arguments, 0, "close_project");
   if (lua_msg != NULL) {
      STDMSG("%s", lua_msg);
      lc_free(lua_msg);
   }
   DBGMSG("%s\n", arguments);

   lua_close(context);

   return error;
}
