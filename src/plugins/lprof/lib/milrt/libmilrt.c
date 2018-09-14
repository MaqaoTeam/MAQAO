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

#include "libmilrt.h"

static lua_State **context;
static int n_threads;

char *lua_exec(lua_State *L,char *buff,int bufferlen)
{
	char *str_error="";
	int error;
	int bufflen;

	if(L == NULL)
	{
		strcpy(str_error,"Invalid internal interpreter context : Impossible to execute your query");
		return str_error;
	}

	if(bufferlen > 0)
		bufflen = bufferlen;
	else
		bufflen = strlen(buff);
	
	error = luaL_loadbuffer(L, buff, bufflen, "line") || lua_pcall(L, 0, 0, 0);
	if (error)
	{
		str_error = (char *)malloc(2048 * sizeof(char));
		sprintf(str_error, "MAQAO> %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return str_error;
	}

	return str_error;
}

void milrt_load(int threads)
{
  char *env;
  int i;
  
  if( (env = getenv("OMP_NUM_THREADS")) != NULL)
  {
    if(strcmp(env,"") != 0){
      n_threads = atoi(env);
    }/*else{
      fprintf(stderr,"OMP_NUM_THREADS not defined\n");
    }*/
  }
//   else
//     fprintf(stderr,"OMP_NUM_THREADS not defined\n");
  
  if(n_threads <= 0)
  {
    fprintf(stderr,"OMP_NUM_THREADS contains an invalid value or is not defined\nSetting OMP_NUM_THREADS=1\n");
    setenv("OMP_NUM_THREADS","1",1);
    n_threads = 1;
  }
  
  context = (lua_State **)malloc(sizeof(lua_State*)*n_threads);
  for(i=0;i<n_threads;i++)
  {
    //Intialize Lua environment
    context[i] = (lua_State *)luaL_newstate();
    //Load libraries
    luaL_openlibs(context[i]);
  }  
  
  fprintf(stdout,"Lua environment started with %d threads\n",n_threads);
}

void milrt_exec(char *lua_cmd)
{  
  int error,tid;
  
  tid = omp_get_thread_num();
  //printf("T%d L=%p | lua_exec(%s) \n",tid,context[tid],lua_cmd);
  error = luaL_loadbuffer(context[tid], lua_cmd, strlen(lua_cmd), "line") || lua_pcall(context[tid], 0, 0, 0);
  if(error){
    fprintf(stderr, "[T%d L=%p]MAQAO> %s\n",tid,context[tid],lua_tostring(context[tid], -1));
  }  
  
//printf("%s", lua_exec(context,lua_cmd,strlen(lua_cmd)));
//    int rslt = luaL_dostring(context,lua_cmd);
//    if(rslt == 1) {
//       fprintf(stderr,"Some error occured while excuting runtime code\n");
//    }
}

void milrt_exec_all(char *lua_cmd)
{  
  int error,i;

  for(i=0;i<n_threads;i++)
  {    
    //printf("T%d lua_exec(%s) \n",i,lua_cmd);
    error = luaL_loadbuffer(context[i], lua_cmd, strlen(lua_cmd), "line") || lua_pcall(context[i], 0, 0, 0);
    if(error){
      fprintf(stderr, "[T%d][all]MAQAO> %s\n",i, lua_tostring(context[i], -1));
    }  
  }
}

void milrt_register_function(char *fname,char *flib,char *lua_name)
{
  void *handle,*resfunc;
  const char *lt;int t;
  int i;

  fprintf(stdout,"Registering : %s %s %s\n",fname,flib,lua_name);
  handle = dlopen(flib,RTLD_LAZY);
  resfunc = (void *)dlsym( handle, fname);
  
  for(i=0;i<n_threads;i++)
  {
    printf( "Pointer val = %p\n",resfunc);  
    lua_pushcfunction (context[i], resfunc);
  t = lua_type(context[i],-1);
  lt = lua_typename(context[i],t);
    printf( "Type = %s (%d). Setting %s as global name\n",lt,t,lua_name); 
    lua_setglobal(context[i], lua_name);
  }
}

int milrtw_trace_register_func(lua_State *L)
{
  char *str = (char *) luaL_checkstring (L,1);
  int fid = (int)luaL_checkint (L, 2);    
  trace_register_func(str,fid);
  return 0;
}

int milrtw_traceEntry(lua_State *L)
{
  int fid = (int)luaL_checkint (L, 1); 
  traceEntry(fid);  
  return 0;
}

int milrtw_traceExit(lua_State *L)
{
  int fid = (int)luaL_checkint (L, 1); 
  traceExit(fid);  
  return 0;  
}

int milrtw_tau_dyninst_cleanup(lua_State *L)
{ 
  tau_dyninst_cleanup();  
  return 0; 
}


void milrt_unload(void)
{
  int i;
  
  for(i=0;i<n_threads;i++){
     lua_close(context[i]);
  }  
  
}

int get_rdtsc(lua_State *L)
{
    unsigned long long timer;  

    rdtscll(timer); 
    lua_pushnumber(L,timer);
    
    return 1;
}

//#include "runtime.c"
