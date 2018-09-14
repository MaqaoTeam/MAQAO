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



#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <omp.h>
#include <dlfcn.h>
#include "libinstru.h"

#define _GNU_SOURCE
#define SVALS 4
#define IVALS 4

//Change these values to switch between multithreaded mode
#define PROGRAM_IS_MULTITHREADED 1 
#define NUM_THREADS 2 

void __attribute__ ((constructor)) dummy_load(void);
//void __attribute__ ((destructor)) dummy_unload(void);

struct timespec start_time;
unsigned long long start_cycles;

void dummy_unload(void)
{
  struct timespec stop_time;
  unsigned long long stop_cycles;
  unsigned long long tmp_cycles;
  double tmp_time;

  instru_terminate();
  rdtscll(stop_cycles);
  clock_gettime(CLOCK_MONOTONIC,&stop_time);
  tmp_time 	= difftime(stop_time.tv_sec,start_time.tv_sec) +
  				(stop_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
  tmp_cycles = stop_cycles - start_cycles;
  fprintf(stderr,"dummy_unload\n");
  instru_dump(tmp_time,tmp_cycles);
  instru_free();
}

void dummy_unload2(void)
{
  instru_dump2();
}

void dummy_load(void)
{

	char *envs[SVALS][IVALS] = {
		{"MI_PNAME","MI_BIN","MI_COMPANION","MI_BINFILE_HASH"},
		{"MI_NUM_FCTS","MI_NUM_CALLS","MI_NUM_LOOPS","MI_NUM_EDGES"}
	};
	char *env,*svals[SVALS];
	int ivals[IVALS];
	int threading_type,nb_threads,i;
	//bench_instru_fcts *imcp;
	//CHANGE THE VALUES HERE TO HAVE THE INSTRUMENTATION WORK IN MULTI THREADED MODE
	//threading_type = 1 if multi-thread, 0 otherwise
	//nb_threads must be equal to $OMP_GET_NUM_THREADS
	
#if PROGRAM_IS_MULTITHREADED
	threading_type 	= 1;
	nb_threads	= NUM_THREADS;
#else
	threading_type 	= 0;
	nb_threads	= 1;
#endif
	fprintf(stderr,"threading_type=%d - nb_threads=%d\n",threading_type,nb_threads);
	for(i = 0;i < SVALS;i++)
	{
		if((env = getenv(envs[0][i])) != NULL)
		{
			if(strcmp(env,"") != 0){
				svals[i] = env;
			}else{
				fprintf(stderr,"%s not defined\n",envs[0][i]);
			}
		}
		else
			fprintf(stderr,"%s not defined\n",envs[0][i]);
	}
	for(i = 0;i < IVALS;i++)
	{
		if((env = getenv(envs[1][i])) != NULL)
		{
			if(strcmp(env,"") != 0){
				ivals[i] = atoi(env);
			}else{
				fprintf(stderr,"%s not defined\n",envs[0][i]);
			}
		}
		else
			fprintf(stderr,"%s not defined\n",envs[0][i]);
	}
	instru_init(svals[0],svals[1],threading_type,nb_threads,
				ivals[0],ivals[1],ivals[2],ivals[3],
				svals[2],svals[3]);
	//imcp = intru_madras_call_price();

	//instru_init("instru","/home/acharifrubial/dev/workspace/PERF/sources/maqao/examples/instrumentation/test",0,1,2,3,0,0,"instru_1319645935_0.meta","instru_1319645935_0");
	atexit(dummy_unload);
	clock_gettime(CLOCK_MONOTONIC,&start_time);
	rdtscll(start_cycles);
}
