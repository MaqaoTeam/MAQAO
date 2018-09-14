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
#endif
#include <stdio.h>
#include <inttypes.h>
#include <lua.h>
#include <lauxlib.h>

#include "libmadras.h"
#include "libmcommon.h"
#include "abstract_objects_c.h"

extern help_t* madras_load_help();
extern void ao_init_help (lua_State *L, help_t* help);


#define MADRAS "madras"

typedef struct {
  elfdis_t *binfile;
  modif_t *latest_call;
  int64_t  latest_insnaddr;
} l_madras_t;

/* Helper function to avoid casting out the const modifier implied by luaL_checkstring */
static char *check_string (lua_State *L, int index) {
  return (char *) luaL_checkstring (L, index);
}

/**************************************************
 * Constructor and destructor for a madras object *
 **************************************************/

static int l_madras_new (lua_State * L) {
  char *file_name = check_string (L, 1);
  elfdis_t *file = madras_disass_file (file_name);

  if (file != NULL) {
    l_madras_t *mdr = lua_newuserdata (L, sizeof *mdr);

    mdr->binfile     = file;
    mdr->latest_call = NULL;

    luaL_getmetatable (L, MADRAS);
    lua_setmetatable  (L, -2);

    return 1;
  }

  return 0;
}

static int l_madras_terminate (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);

  madras_terminate (mdr->binfile);

  return 0;
}

/***********************
 * Getters and testers *
 ***********************/

static int l_madras_is_valid_binary (lua_State * L) {
  char *bin_path = check_string (L, 1);
  int arch_code = 0;
  int file_code = 0;

  int valid = madras_is_file_valid (bin_path, &arch_code, &file_code);

  if (valid == 1) {
    lua_pushboolean (L, TRUE);
    lua_pushinteger (L, arch_code);
    lua_pushinteger (L, file_code);

    return 3;
  }

  lua_pushboolean (L, FALSE);

  return 1;
}

static int l_madras_is_executable (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int type = madras_get_type (mdr->binfile);

  lua_pushboolean (L, (type == BFT_EXECUTABLE) ? TRUE : FALSE);

  return 1;
}

static int l_madras_is_dynamic_library (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int type = madras_get_type (mdr->binfile);

  lua_pushboolean (L, (type == BFT_LIBRARY) ? TRUE : FALSE);

  return 1;
}

static int l_madras_is_relocatable (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int type = madras_get_type (mdr->binfile);

  lua_pushboolean (L, (type == BFT_RELOCATABLE) ? TRUE : FALSE);

  return 1;
}

static int l_madras_get_dynamic_libraries (lua_State *L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  list_t *dyn_libs = madras_get_dynamic_libraries (mdr->binfile);

  if (dyn_libs != NULL) {
    int i = 1;

    lua_newtable (L);

    FOREACH_INLIST (dyn_libs, iter) {
      lua_pushstring (L, GET_DATA_T (char *, iter));
      lua_rawseti (L, -2, i++);
    }

    return 1;
  }

  return 0;
}

static int l_madras_get_file_dynamic_libraries (lua_State *L)
{
//   l_madras_t* mdr = luaL_checkudata(L, 1, MADRAS);
   char* filename = check_string(L, 2);
   queue_t* dyn_libs = madras_get_file_dynamic_libraries(filename);

   if (dyn_libs != NULL) {
     int i = 1;

     lua_newtable (L);

     FOREACH_INQUEUE (dyn_libs, iter) {
       lua_pushstring (L, GET_DATA_T (char *, iter));
       lua_rawseti (L, -2, i++);
     }
     queue_free(dyn_libs, lc_free);

     return 1;
   }

   return 0;
}

/***********************
 * ? *
 ***********************/
static int l_madras_linkbranch_toaddr(lua_State *L)
{
   l_madras_t *mdr  = luaL_checkudata (L,1,MADRAS);
   i_t *ip        = luaL_checkudata  (L,2, INSN);
   insn_t * insn  = ip->p;
   int64_t   addr = luaL_checklong  (L,3);

   madras_linkbranch_toaddr(mdr->binfile,insn,addr);

   return 0;
}

static int l_madras_get_oppositebranch (lua_State *L)
{
   l_madras_t *mdr  = luaL_checkudata (L,1,MADRAS);
   int64_t   addr = luaL_checklong  (L,2);
   i_t *   insn = addr != 0 ? NULL : luaL_checkudata  (L,3, INSN);
   insn_t * ip = addr != 0 ? NULL : insn->p;

   cond_t *cond;
   insn_t *answ = madras_get_oppositebranch(mdr->binfile, ip, addr, &cond);

   if((int64_t)answ > 0)
   {
      create_insn(L,answ);
      lua_pushnil(L);
      return 2;
   }
   else
   {
      switch((int64_t)answ)
      {
         case WRN_LIBASM_BRANCH_OPPOSITE_COND:
            lua_pushnil(L);
            lua_pushlightuserdata(L,cond);
            return 2;
            break;
         case ERR_LIBASM_INSTRUCTION_NOT_FOUND:
         case ERR_LIBASM_INSTRUCTION_NOT_BRANCH:
         case WRN_LIBASM_BRANCH_HAS_NO_OPPOSITE:
         default:
            lua_pushnil(L);
            lua_pushnil(L);
            return 2;
      }
   }

   return 0;
}

static int l_madras_modif_setnext(lua_State *L)
{
   l_madras_t *mdr   = luaL_checkudata (L,1,MADRAS);
   modif_t *modif1 = lua_touserdata  (L,2);
   modif_t *modif2 = lua_touserdata  (L,3);
   int64_t   addr  = luaL_checklong  (L,4);
   int ret;

   ret = madras_modif_setnext(mdr->binfile,modif1,modif2,addr);
   lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

   return 1;
}

static int l_madras_modif_setpaddinginsn(lua_State *L)
{
   l_madras_t *mdr  = luaL_checkudata  (L,1,MADRAS);
   modif_t *modif = lua_touserdata   (L,2);
   insn_t *insn   = lua_touserdata   (L,3);
   char* strinsn  = (char *)luaL_checkstring (L,4);
   int ret;

   ret = madras_modif_setpaddinginsn(mdr->binfile,modif,insn,strinsn);
   lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

   return 1;
}

/**************************************
 * External library related modifiers *
 **************************************/
static int l_madras_extlib_add (lua_State *L) {
  /* Check and get arguments */
  l_madras_t  *mdr = luaL_checkudata (L, 1, MADRAS);
  char *lib_name = check_string    (L, 2);

  modiflib_t* modlib = madras_extlib_add (mdr->binfile, lib_name);
  lua_pushlightuserdata (L, modlib);

  return 1;
}

static int l_madras_modiflib_getlabels (lua_State *L) {
  /* Check and get arguments */
  l_madras_t  *mdr     = luaL_checkudata (L,1,MADRAS);
  modiflib_t* modlib = lua_touserdata  (L,2);
  int i=1;

  //Initialises the queue for retrieving the labels
  queue_t* fctsinlib = queue_new();
  //Retrieves the labels in the library and copy them into the queue (the NULL parameter is for the hashtable)
  madras_modiflib_getlabels(mdr->binfile,modlib,fctsinlib,NULL);

  lua_newtable (L);
  //Populate the table with the labels returned by madras_modiflib_getlabels
  FOREACH_INQUEUE(fctsinlib,iter)
  {
   lua_pushnumber (L, i++);
   lua_pushstring (L,label_get_name(GET_DATA_T(label_t*,iter)));
   lua_settable   (L,-3);
  }
  //Frees the queue
  queue_free(fctsinlib,NULL);

  return 1;
}

#include <errno.h>

extern int errno; 

static int l_madras_extlib_add_fromdescriptor (lua_State * L) {
  /* Check and get arguments */
  l_madras_t  *mdr      = luaL_checkudata  (L, 1, MADRAS);
  char *extlibname    = check_string     (L, 2);
  char *b64_binextlib = lua_touserdata   (L, 3);
  int binextlib_size  = luaL_checkinteger(L, 4);
  char *binextlib;
  void *ret = (void *)0;
  FILE *fp,*filedesc;
  int fd;
  char template[] = "static_lib_XXXXXX";
  char ch;
  
  //printf("In l_madras_extlib_add_fromdescriptor : %s %p %d\n",extlibname,b64_binextlib,binextlib_size);
#ifdef _WIN32
  fd = _mktemp_s(template, sizeof(template));
#else
  fd = mkstemp(template);
#endif
  binextlib = decode(b64_binextlib,binextlib_size);
  filedesc = fdopen(fd,"wb");
#ifdef _WIN32
  fp = fopen(binextlib, "rb");
#else
  fp = fmemopen(binextlib, binextlib_size, "rb");
#endif
  /* File to file copy */
  //TODO : madras should use a File* since the new lelf uses it => this copy will then be avoided.
  while(!feof(fp))
  {
    ch = getc(fp);
    if(ferror(fp))
    {
      printf("Read Error");
      clearerr(fp);
      break;
    }
    else
    {
      if(!feof(fp))
         putc(ch, filedesc);
      if(ferror(filedesc))
      {
        printf("Write Error");
        clearerr(filedesc);
        break;
      }
    }
  }
  fclose(fp);
  fseek(filedesc,0,SEEK_SET);
  //printf("binextlib = %p %d %d\n",binextlib,fileno(filedesc),fd);
  
  if (fd != -1){
    ret = madras_extlib_add_fromdescriptor (mdr->binfile, extlibname, fd);
  }else{
    perror("Fileno error ");
  }

  lua_pushboolean (L, ret ? TRUE : FALSE);
  
  return 1;
}

static int l_madras_extlib_rename (lua_State * L) {
  /* Check and get arguments */
  l_madras_t     *mdr = luaL_checkudata (L, 1, MADRAS);
  char *old_libname = check_string    (L, 2);
  char *new_libname = check_string    (L, 3);

  void *ret = madras_extlib_rename (mdr->binfile, old_libname, new_libname);

  lua_pushboolean (L, ret ? TRUE : FALSE);

  return 1;
}

static int l_madras_extlib_set_priority (lua_State * L) {
  l_madras_t     *mdr  = luaL_checkudata (L, 1, MADRAS);
  modiflib_t* modlib = lua_touserdata  (L, 2);

  int ret = madras_modiflib_add_flag(mdr->binfile, modlib, LIBFLAG_PRIORITY);

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_extfct_rename (lua_State * L) {
  /* Check and get arguments */
  l_madras_t     *mdr = luaL_checkudata (L, 1, MADRAS);
  char *libname = check_string    (L, 2);
  char *oldname = check_string    (L, 3);
  char *newname = check_string    (L, 4);

  madras_extfct_rename (mdr->binfile, libname, oldname, newname);

  return 1;
}


/*********************************
 * Instruction related modifiers *
 *********************************/

static int l_madras_add_insn (lua_State * L)
{
  /* Get and check arguments */
  l_madras_t  *mdr = luaL_checkudata (L,1,MADRAS);
  i_t *ip        = luaL_checkudata  (L,2, INSN);
  insn_t *insn   = ip->p;
  int64_t   addr = luaL_checklong  (L,3);
  char       pos = luaL_checkint   (L,4);

  void *ret = madras_add_insn (mdr->binfile, insn, addr, pos, NULL, NULL, TRUE);

  lua_pushboolean (L, ret ? TRUE : FALSE);

  return 1;
}

static int l_madras_insnlist_add (lua_State * L) {
  /* Get and check arguments */
  l_madras_t  *mdr    = luaL_checkudata (L, 1, MADRAS);
  char *insnlist    = check_string    (L, 2);
  int64_t   addr    = luaL_checklong  (L, 3);
  char       pos    = luaL_checkint   (L, 4);
  //lua table containing gvars in L,5
  //lua table containing tlsvars in L,6

  modif_t *modif;
  int i = 0;

  globvar_t** gvars = NULL;
  if( lua_istable( L, 5 ) && lua_objlen(L,5) != 0 )
  {
    gvars = malloc(sizeof(globvar_t *) * lua_objlen(L, 5));
    lua_pushnil(L);                       // put the first index (0) on the stack for lua_next
    i = 0;
    while( lua_next( L, 5 ) )             // pop the index, read table[index+1], push index (-2) and value (-1)
    {
      gvars[i++] = lua_touserdata (L,-1); // store the value
      lua_pop( L, 1 );                    // remove the value from the stack (keep the next index)
    }
  }

  tlsvar_t** tlsvars = NULL;
  if( lua_istable( L, 6 ) && lua_objlen(L,6) != 0)
  {
    tlsvars = malloc(sizeof(tlsvar_t *) * lua_objlen(L, 6));
    lua_pushnil(L);
    i = 0;
    while( lua_next( L, 6 ) )
    {
      tlsvars[i++] = lua_touserdata (L,-1);
      lua_pop( L, 1 );
    }
  }

  modif = madras_insnlist_add (mdr->binfile, insnlist, addr, pos, gvars, tlsvars);

  if (modif != NULL) {
    lua_pushlightuserdata (L, modif);
  }
  else {
    lua_pushnil (L);
  }

  return 1;
}

/* Helper function used by l_madras_modify_insn to replace missing luaL_checkboolean */
static int check_boolean (lua_State *L, int index) {
  if (lua_isboolean (L, index))
    return lua_toboolean (L, index);

  luaL_error (L, "A boolean is expected for argument #%d", index);
  abort ();
}

static int l_madras_modify_insn (lua_State *L) {
  void *ret = NULL;

  /* Get and check operands */
  l_madras_t   *mdr = luaL_checkudata (L, 1, MADRAS);
  int64_t    addr = luaL_checklong  (L, 2);
  int     padding = check_boolean   (L, 3);
  char *newopcode = check_string    (L, 4);
  int   nb_oprnds = luaL_checkint   (L, 5);

  if (nb_oprnds == 0) {
    ret = madras_modify_insn (mdr->binfile, addr, padding, newopcode, 0);
  }
  else if (nb_oprnds == 1) {
    char *op1 = check_string (L, 6);

    ret = madras_modify_insn (mdr->binfile, addr, padding, newopcode, 1, op1);
  }
  else if (nb_oprnds == 2) {
    char *op1 = check_string (L, 6);
    char *op2 = check_string (L, 7);

    ret = madras_modify_insn (mdr->binfile, addr, padding, newopcode, 2, op1, op2);
  }
  else if (nb_oprnds == 3) {
    char *op1 = check_string (L, 6);
    char *op2 = check_string (L, 7);
    char *op3 = check_string (L, 8);

    ret = madras_modify_insn (mdr->binfile, addr, padding, newopcode, 3, op1, op2, op3);
  }
  else if (nb_oprnds == 4) {
    char *op1 = check_string (L, 6);
    char *op2 = check_string (L, 7);
    char *op3 = check_string (L, 8);
    char *op4 = check_string (L, 9);

    ret = madras_modify_insn (mdr->binfile, addr, padding, newopcode, 4, op1, op2, op3, op4);
  }

  lua_pushboolean (L, ret ? TRUE : FALSE);

  return 1;
}

static int l_madras_delete_insns (lua_State *L) {
  /* Get and check arguments */
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int     ninsn = luaL_checkint   (L, 2);
  int64_t  addr = luaL_checklong  (L, 3);

  void *ret = madras_delete_insns (mdr->binfile, ninsn, addr);

  lua_pushboolean (L, ret ? TRUE : FALSE);

  return 1;
}

static int l_madras_relocate_insn (lua_State *L) {
  /* Get and check arguments */
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int64_t  addr = luaL_checklong  (L, 2);

  void *ret = madras_relocate_insn (mdr->binfile, addr);

  lua_pushboolean (L, ret ? TRUE : FALSE);

  return 1;
}

/******************************
 * Function related modifiers *
 ******************************/

static int l_madras_fct_add (lua_State * L) {
  void *ret;

  /* Check and get arguments */
  l_madras_t  *mdr = luaL_checkudata (L, 1, MADRAS);
  char *fct_name = check_string    (L, 2);
  char *lib_name = check_string    (L, 3);
  char *fct_code = (char *) lua_tostring (L, 4);

  if (strcmp (lib_name, "") == 0) lib_name = NULL;

  ret = madras_fct_add (mdr->binfile, fct_name, lib_name, fct_code);

  lua_pushboolean (L, ret ? TRUE : FALSE);

  return 1;
}

/***********************************
 * Function call related modifiers *
 ***********************************/

/* Helper function for l_madras_fctcall_new[_nowrap] */
static int fctcall_new (lua_State * L, int no_wrap) {
  modif_t *fct_call;
  int i = 0;

  /* Check and get arguments */
  l_madras_t   *mdr = luaL_checkudata (L, 1, MADRAS);
  char  *fct_name = check_string    (L, 2);
  char  *lib_name = check_string    (L, 3);
  int64_t    addr = luaL_checklong  (L, 4);
  char        pos = luaL_checkint   (L, 5);
  int        nreg = luaL_checkint   (L, 6);
  reg_t** reglist = NULL;

  if (nreg > 0)
  {
    reglist = malloc(nreg * sizeof(reg_t*));
    lua_pushnil(L);
    while (lua_next(L,7) != 0)
    {
      reglist[i] = (reg_t*)lua_touserdata(L, -1);
      lua_pop(L,1);
      i++;
    }
  }

  if (strcmp (lib_name, "") == 0) lib_name = NULL;

  if (no_wrap == TRUE)
    fct_call = madras_fctcall_new_nowrap (mdr->binfile, fct_name, lib_name, addr, pos);
  else
    fct_call = madras_fctcall_new        (mdr->binfile, fct_name, lib_name, addr, pos, reglist, nreg);

  if ((int64_t)fct_call > 0)
  {
    mdr->latest_call = fct_call;
    mdr->latest_insnaddr = addr;
    lua_pushlightuserdata (L, fct_call);
  }
  else if(fct_call == NULL)
    lua_pushnil (L);
  else
   lua_pushinteger (L, (int64_t)fct_call);

  return 1;
}

static int l_madras_fctcall_new_nowrap (lua_State * L) { return fctcall_new (L, TRUE ); }
static int l_madras_fctcall_new        (lua_State * L) { return fctcall_new (L, FALSE); }

static int l_madras_fctcall_addparam_imm (lua_State * L) {
  /* Get and check arguments */
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int64_t   imm = luaL_checklong  (L, 2);

  int ret = EXIT_FAILURE;

  if (mdr->latest_call != NULL)
    ret = madras_fctcall_addparam_imm (mdr->binfile, mdr->latest_call, imm, 'a');

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}


union cast {
   int64_t intValue;
   double  doubleValue;
};

static int l_madras_fctcall_addparam_immdouble (lua_State * L) {
  /* Get and check arguments */
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  double   immd = luaL_checknumber  (L, 2);

  /* Cast without GCC warning about strict-aliasing */
  union cast c;
  c.doubleValue = immd;
  int64_t imm = c.intValue;

  int ret = EXIT_FAILURE;

  if (mdr->latest_call != NULL)
    ret = madras_fctcall_addparam_imm (mdr->binfile, mdr->latest_call, imm, 'a');

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_fctcall_addparam_frominsn (lua_State * L) {
  /* Get and check arguments */
  l_madras_t     *mdr = luaL_checkudata (L, 1, MADRAS);
  int     oprnd_idx = luaL_checkint   (L, 2);
  int64_t insn_addr = luaL_checklong  (L, 3);
  int     target    = luaL_checkint   (L, 4);

  int ret  = EXIT_FAILURE;
  char opt = 'q';

  if(target == 0)
     opt = 'a';
  if(insn_addr == 0)
     insn_addr = mdr->latest_insnaddr;

  if (mdr->latest_call != NULL && insn_addr != 0)
    ret = madras_fctcall_addparam_frominsn (mdr->binfile, mdr->latest_call, oprnd_idx, opt, insn_addr);

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_fctcall_addparam_reg (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  char  *regstr = check_string    (L, 2);

  int ret = EXIT_FAILURE;

  if (mdr->latest_call != NULL)
    ret = madras_fctcall_addparam_fromstr (mdr->binfile, mdr->latest_call, regstr, 'a');

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_fctcall_addparam_mem (lua_State * L) {
  /* Get and check arguments */
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  char  *memstr = check_string    (L, 2);

  int ret = EXIT_FAILURE;

  if (mdr->latest_call != NULL)
    ret = madras_fctcall_addparam_fromstr (mdr->binfile, mdr->latest_call, memstr, 'q');

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_fctcall_addparam_from_gvar (lua_State * L) {
  /* Get and check arguments */
  l_madras_t      *mdr = luaL_checkudata (L, 1, MADRAS);
  globvar_t    *gvar = lua_touserdata  (L, 2);
  char *string = (char *) lua_tostring (L, 3);
  char *opts   = (char *) lua_tostring (L, 4);

  int ret = EXIT_FAILURE;

  if (mdr->latest_call != NULL) {
    char opt = 'q';

    if ((opts != NULL) && (strcmp (opts, "a") == 0)) opt = 'a';

    ret = madras_fctcall_addparam_fromglobvar (mdr->binfile, mdr->latest_call, gvar, string, opt);
  }

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_fctcall_addparam_from_tlsvar (lua_State * L) {
  /* Get and check arguments */
  l_madras_t      *mdr = luaL_checkudata (L, 1, MADRAS);
  tlsvar_t    *tlsvar = lua_touserdata  (L, 2);
  char *string = (char *) lua_tostring (L, 3);
  char *opts   = (char *) lua_tostring (L, 4);

  int ret = EXIT_FAILURE;

  if (mdr->latest_call != NULL) {
    char opt = 'q';

    if ((opts != NULL) && (strcmp (opts, "a") == 0)) opt = 'a';

    ret = madras_fctcall_addparam_fromtlsvar (mdr->binfile, mdr->latest_call, tlsvar, string, opt);
  }

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}


static int l_madras_fctcall_addparam_from_str (lua_State * L) {
   l_madras_t* mdr = luaL_checkudata (L, 1, MADRAS);
   char* string    = (char *) lua_tostring (L, 2);
   int ret         = EXIT_FAILURE;

   if (mdr->latest_call != NULL)
   {
     char opt = 'q';
     ret = madras_fctcall_addparam_fromglobvar (mdr->binfile, mdr->latest_call, NULL, string, opt);
   }

   lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

   return 1;
}



static int l_madras_fctcall_addreturnval (lua_State * L) {
  /* Get and check arguments */
  l_madras_t   *mdr = luaL_checkudata (L, 1, MADRAS);
  globvar_t *gvar = lua_touserdata  (L, 2);

  int ret = EXIT_FAILURE;

  if (mdr->latest_call != NULL)
    ret = madras_fctcall_addreturnval (mdr->binfile, mdr->latest_call, gvar);

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_cond_new (lua_State * L)
{
   l_madras_t *mdr = luaL_checkudata   (L,1,MADRAS);
   int comp_op   = luaL_checkinteger (L,2);
   oprnd_t* op   = lua_touserdata    (L,3);
   int bound     = luaL_checkinteger (L,4);
   cond_t* cond1 = lua_touserdata    (L,5);
   cond_t* cond2 = lua_touserdata    (L,6);
   cond_t* cond;

   cond = madras_cond_new(mdr->binfile,comp_op,op,bound,cond1,cond2);
   if(cond != NULL)
      lua_pushlightuserdata(L,cond);
   else
      return 0;

   return 1;
}

static int l_madras_branch_insert(lua_State * L)
{
   l_madras_t *mdr   = luaL_checkudata   (L,1,MADRAS);
   int64_t addr    = luaL_checkinteger (L,2);
   int position    = luaL_checkinteger (L,3);
   modif_t* modif  = lua_touserdata    (L,4);
   int64_t daddr   = luaL_checkinteger (L,5);
   int update      = luaL_checkinteger (L,6);
   modif_t* tmodif;

   tmodif = madras_branch_insert(mdr->binfile,addr,position,modif,daddr,update);
   if ((int64_t)tmodif > 0)
      lua_pushlightuserdata(L, tmodif);
   else if(tmodif == NULL)
      lua_pushnil (L);
   else
      lua_pushinteger(L, (int64_t)tmodif);

   return 1;
}

static int l_madras_fctcall_getlib(lua_State * L)
{
   l_madras_t *mdr   = luaL_checkudata   (L,1,MADRAS);
   modiflib_t* ret = NULL; 
   
   if (mdr->latest_call != NULL)
      ret = madras_fctlib_getlib (mdr->binfile, mdr->latest_call);
   if (ret != NULL)
      lua_pushlightuserdata (L, ret);
   else
      lua_pushnil (L);
   return 1;
}


/*****************************
 * Tracing related functions *
 *****************************/

static int l_madras_traceon (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);

  madras_traceon (mdr->binfile, NULL, 0);

  return 0;
}

static int l_madras_traceoff (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);

  madras_traceoff (mdr->binfile, NULL);

  return 0;
}


/*********************************
 * Functions to control patching *
 *********************************/

static int l_madras_modifs_init (lua_State * L) {
  /* Get and check arguments */
  l_madras_t *mdr     = luaL_checkudata (L, 1, MADRAS);
  char stack_policy = luaL_checkint   (L, 2);
  int64_t shift     = luaL_checklong  (L, 3);

  int ret = madras_modifs_init (mdr->binfile, stack_policy, shift);/**\todo TODO (2015-04-16) Patcher refactoring -update the call as needed*/

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

/* Helper function for l_madras_modifs_addopt and l_madras_modifs_remopt */
static int modifs_opt (lua_State *L, int add) {
  /* Check and get arguments */
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int    option = luaL_checkint   (L, 2);

  int ret;

  if (add == TRUE)
    ret = madras_modifs_addopt (mdr->binfile, option);
  else
    ret = madras_modifs_remopt (mdr->binfile, option);

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_modifs_addopt (lua_State *L) { return modifs_opt (L, TRUE ); }
static int l_madras_modifs_remopt (lua_State *L) { return modifs_opt (L, FALSE); }

static int l_madras_modifs_commit (lua_State * L) {
  /* Get and check arguments */
  l_madras_t      *mdr = luaL_checkudata (L, 1, MADRAS);
  char *new_bin_name = check_string    (L, 2);

  int ret = madras_modifs_commit (mdr->binfile, new_bin_name);

  mdr->latest_call = NULL;

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

  return 1;
}

static int l_madras_modif_addcond (lua_State * L)
{
   l_madras_t *mdr  = luaL_checkudata  (L,1,MADRAS);
   modif_t* modif = lua_touserdata   (L,2);
   cond_t* cond   = lua_touserdata   (L,3);
   int comp_op    = luaL_checkinteger(L,4);
   int ret       = EXIT_FAILURE;

   if(modif == NULL)
      modif = mdr->latest_call;
   if(modif != NULL)
      ret = madras_modif_addcond(mdr->binfile,modif,cond,comp_op);

   lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

   return 1;
}

static int l_madras_modif_addelse (lua_State * L)
{
   l_madras_t *mdr   = luaL_checkudata (L,1,MADRAS);
   modif_t* modif1 = lua_touserdata  (L,2);
   modif_t* modif2 = lua_touserdata  (L,3);
   int ret         = EXIT_FAILURE;

   if (mdr->latest_call != NULL)
      ret = madras_modif_addelse(mdr->binfile,modif1,modif2);

   lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

   return 1;
}

static int l_madras_modif_commit (lua_State *L)
{
   l_madras_t *mdr   = luaL_checkudata (L,1,MADRAS);
   modif_t* modif = lua_touserdata  (L,2);
   int ret         = FALSE;

   if (modif != NULL)
      ret = madras_modif_commit(mdr->binfile, modif);

   lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);

   return 1;
}

/*******************
 * Other functions *
 *******************/

static int l_madras_gvar_new (lua_State * L) {
  globvar_t *gvar = NULL;

  /* Get and check arguments */
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int      type = luaL_checkint   (L, 2);
  int      size = luaL_checkint   (L, 3);

  if (type != 1 && size <= 0) return 0;

  switch (type) {
   case 0: { //integer
      int64_t tmp = luaL_checklong (L, 4);

      gvar = madras_globalvar_new (mdr->binfile, size, &tmp);
   }
   break;
   case 1: { //string
      char *tmp = check_string (L, 4);

      gvar = madras_globalvar_new (mdr->binfile, strlen(tmp)+1, tmp);
   }
   break;
   case 2: //pointer
      gvar = madras_globalvar_new (mdr->binfile, size, NULL);
      break;

   case 3: // float/double
      if (size == 8) {
         double tmp = luaL_checknumber (L, 4);
         printf("Value: %f\n", tmp);
         gvar = madras_globalvar_new (mdr->binfile, size, &tmp);
      }
      else if (size == 4) {
         float tmp = (float)luaL_checknumber (L, 4);
         gvar = madras_globalvar_new (mdr->binfile, size, &tmp);
      }
      else return 0;
      break;

   default: //unhandled type
      return 0;
  }

  if (gvar != NULL) {
    lua_pushlightuserdata (L, gvar);
    return 1;
  }

  return 0;
}

static int l_madras_tlsvar_new (lua_State * L) {
  tlsvar_t* tlsvar = NULL;

  /* Get and check arguments */
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int      type = luaL_checkint   (L, 2);
  int      size = luaL_checkint   (L, 3);

  if (type != 1 && size <= 0) return 0;
  int    initialized = luaL_checkint(L, 5);
  boolean_t is_initialized = (initialized)?TRUE:FALSE;

  switch (type) {
   case 0: { //integer
      int64_t tmp = luaL_checklong (L, 4);

      if (tmp || is_initialized)
         tlsvar = madras_tlsvar_new (mdr->binfile, size, &tmp, INITIALIZED);
      else
         tlsvar = madras_tlsvar_new (mdr->binfile, size, &tmp, UNINITIALIZED);
   }
   break;
   case 1: { //string
      char *tmp = check_string (L, 4);

      if (tmp || is_initialized)
         tlsvar = madras_tlsvar_new (mdr->binfile, strlen(tmp)+1, tmp, INITIALIZED);
      else
         tlsvar = madras_tlsvar_new (mdr->binfile, strlen(tmp)+1, tmp, UNINITIALIZED);
   }
   break;
   case 2: //pointer
      tlsvar = madras_tlsvar_new (mdr->binfile, size, NULL, (is_initialized)?INITIALIZED:UNINITIALIZED);
      break;

   default: //unhandled type
      return 0;
  }

  if (tlsvar != NULL) {
    lua_pushlightuserdata (L, tlsvar);
    return 1;
  }

  return 0;
}


static int l_madras_changeOSABI (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  int      code = luaL_checkint   (L, 2);
  int       ret = madras_change_OSABI (mdr->binfile, code);

  lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);
  return 1;
}

static int l_madras_changeOSABI_fromstr (lua_State * L) {
  l_madras_t *mdr = luaL_checkudata (L, 1, MADRAS);
  char     *str = check_string   (L, 2);
  int code = -1;
  
  if (strcmp (str, "SystemV") == 0)
    code = ELFOSABI_SYSV;
  else if (strcmp (str, "HP-UX") == 0)
    code = ELFOSABI_HPUX;
  else if (strcmp (str, "NetBSD") == 0)
    code = ELFOSABI_NETBSD;
  else if (strcmp (str, "Linux") == 0)
    code = ELFOSABI_LINUX;
  else if (strcmp (str, "Solaris") == 0)  
    code = ELFOSABI_SOLARIS;
  else if (strcmp (str, "AIX") == 0)  
    code = ELFOSABI_AIX;
  else if (strcmp (str, "Irix") == 0)  
    code = ELFOSABI_IRIX;
  else if (strcmp (str, "FreeBSD") == 0)
    code = ELFOSABI_FREEBSD;
  else if (strcmp (str, "TRU64") == 0)
    code = ELFOSABI_TRU64;
  else if (strcmp (str, "Modesto") == 0)
    code = ELFOSABI_MODESTO;
  else if (strcmp (str, "OpenBSD") == 0)
    code = ELFOSABI_OPENBSD;
  else if (strcmp (str, "ARM EABI") == 0)
    code = ELFOSABI_ARM_AEABI;
  else if (strcmp (str, "ARM") == 0)
    code = ELFOSABI_ARM;

  if (code != -1)
  {
     int       ret = madras_change_OSABI (mdr->binfile, code);
     lua_pushboolean (L, (ret == EXIT_SUCCESS) ? TRUE : FALSE);
  }
  else
     lua_pushboolean (L, FALSE);
  
  return 1;
}

static int l_madras_init_help (lua_State *L)
{
   help_t* help = madras_load_help();
   ao_init_help (L, help);
   return (1);
}


/******************
 * Meta functions *
 ******************/

static int madras_gc (lua_State * L) {
  (void) L;

  return 0;
}

static int madras_tostring (lua_State * L) {
  lua_pushstring (L, "Madras Library Object");

  return 1;
}

static const luaL_Reg madras_methods[] = {
  /* Constructor and destructor for a madras object */
  {"new"      , l_madras_new},
  {"terminate", l_madras_terminate},

  /* Getters and testers */
  {"is_valid_binary"      , l_madras_is_valid_binary},
  {"is_executable"        , l_madras_is_executable},
  {"is_dynamic_library"   , l_madras_is_dynamic_library},
  {"is_relocatable"       , l_madras_is_relocatable},
  {"get_dynamic_libraries", l_madras_get_dynamic_libraries},
  {"get_file_dynamic_libraries", l_madras_get_file_dynamic_libraries},

  /* ??? */
  {"linkbranch_toaddr"    , l_madras_linkbranch_toaddr},
  {"get_oppositebranch"   , l_madras_get_oppositebranch},

  /* External library related modifiers */
  {"extlib_add"                , l_madras_extlib_add},
  {"extlib_add_fromdescriptor" , l_madras_extlib_add_fromdescriptor},  
  {"extlib_rename"             , l_madras_extlib_rename},
  {"extlib_set_priority"       , l_madras_extlib_set_priority},
  {"modiflib_getlabels"        , l_madras_modiflib_getlabels},

  {"extfct_rename"             , l_madras_extfct_rename},

  /* Instruction related modifiers */
  {"add_insn"    , l_madras_add_insn},
  //{"add_insns"    , l_madras_add_insns},
  {"insnlist_add", l_madras_insnlist_add},
  {"modify_insn" , l_madras_modify_insn},
  {"delete_insns", l_madras_delete_insns},
  {"relocate_insn", l_madras_relocate_insn},

  /* Function related modifiers */
  {"fct_add", l_madras_fct_add},

  /* Function call related modifiers */
  {"fctcall_new"               , l_madras_fctcall_new},
  {"fctcall_new_nowrap"        , l_madras_fctcall_new_nowrap},
  {"fctcall_addparam_frominsn" , l_madras_fctcall_addparam_frominsn},
  {"fctcall_addparam_reg"      , l_madras_fctcall_addparam_reg},
  {"fctcall_addparam_mem"      , l_madras_fctcall_addparam_mem},
  {"fctcall_addparam_imm"      , l_madras_fctcall_addparam_imm},
  {"fctcall_addparam_immdouble", l_madras_fctcall_addparam_immdouble},
  {"fctcall_addparam_from_gvar", l_madras_fctcall_addparam_from_gvar},
  {"fctcall_addparam_from_tlsvar", l_madras_fctcall_addparam_from_tlsvar},
  {"fctcall_addparam_from_str" , l_madras_fctcall_addparam_from_str},
  {"fctcall_addreturnval"      , l_madras_fctcall_addreturnval},
  {"cond_new"                  , l_madras_cond_new},
  {"branch_insert"             , l_madras_branch_insert},
  {"fctcall_getlib"            , l_madras_fctcall_getlib},

  /* Tracing related functions */
  {"traceon" , l_madras_traceon},
  {"traceoff", l_madras_traceoff},

  /* Functions to control patching */
  {"modifs_init"         , l_madras_modifs_init},
  {"modifs_addopt"       , l_madras_modifs_addopt},
  {"modifs_remopt"       , l_madras_modifs_remopt},
  {"modifs_commit"       , l_madras_modifs_commit},
  {"modif_addcond"       , l_madras_modif_addcond},
  {"modif_addelse"       , l_madras_modif_addelse},
  {"modif_setnext"       , l_madras_modif_setnext},
  {"modif_setpaddinginsn", l_madras_modif_setpaddinginsn},
  {"modif_commit"        , l_madras_modif_commit},

  /* Other functions */
  {"gvar_new"            , l_madras_gvar_new},
  {"tlsvar_new"          , l_madras_tlsvar_new},
  {"changeOSABI"         , l_madras_changeOSABI},
  {"changeOSABI_fromstr" , l_madras_changeOSABI_fromstr},
  {"init_help"           , l_madras_init_help},
  {NULL, NULL}
};

/* Meta functions */
static const luaL_Reg madras_meta[] = {
  {"__gc"      , madras_gc},
  {"__tostring", madras_tostring},
  {NULL, NULL}
};

int luaopen_madras (lua_State * L)
{
  DBGMSG0 ("Registering madras module\n");

  luaL_register     (L, "madras", madras_methods);
  luaL_newmetatable (L, "madras");
  luaL_register     (L, 0, madras_meta);

  lua_pushliteral (L, "__index");
  lua_pushvalue   (L, -3);
  lua_rawset      (L, -3);

  lua_pushliteral (L, "__metatable");
  lua_pushboolean (L, 1);
  lua_rawset      (L, -3);

  lua_pop (L, 1);

  return 1;
}
