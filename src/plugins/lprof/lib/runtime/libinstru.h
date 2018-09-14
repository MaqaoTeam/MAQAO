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

#ifndef __LIBINSTRU_H_
#define __LIBINSTRU_H_


#include <omp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include "typedefs.h"
#include "rdtsc.h"

typedef struct loop_s{
   unsigned long long elapsed_cycles;
   unsigned long long start_cycles;
   unsigned long long stop_cycles;
   unsigned long long instances;
   unsigned long long iters;
}loop_t;

typedef struct function_s{
   unsigned long long elapsed_cycles;
   unsigned long long start_cycles;
   unsigned long long stop_cycles;
   unsigned long long depth;
   unsigned long long instances;
}function_t;

typedef struct call_s{
   unsigned long long elapsed_cycles;
   unsigned long long start_cycles;
   unsigned long long stop_cycles;
   unsigned long long depth;
   unsigned long long instances;
}call_t;

typedef struct thread_s{
   function_t *functions;     /**<table of functions*/
   loop_t *loops;             /**<table of loops*/
   unsigned long long *edges; /**<table of edges*/
   call_t *calls;             /**<table of calls*/
   /* All of these tables : indexed by id defined in the intrumentation's lua companion file*/
}thread_t;

typedef struct instru_s{
   char *pname;
   char *binfilename;
   char *binfile_hash;
   char *companion;
   int nb_threads;
   int threading_type;
   int nb_functions;
   int nb_calls;
   int nb_loops;
   int nb_edges;
   thread_t *threads;
}instru_t;

typedef struct bench_instru_fcts {
   int id;
   char *name;
   int avg_overhead;
} bench_instru_fcts;

extern void instru_load(char *);
extern void instru_unload(void);
extern void instru_init(char *pname,char* binfilename,int threading_type,int nb_threads,
						int nb_functions,int nb_calls,int nb_loops,int nb_edges,
						char *companion,char *binfile_hash);
extern bench_instru_fcts *intru_probes_price(void);
extern void intru_probes_warmup(void);
extern void instru_terminate();
extern void instru_fct_tstart(int fid);
extern void instru_fct_tstop(int fid);
extern void instru_fct_call_tstart(int callid);
extern void instru_fct_call_tstop(int callid);
extern void instru_loop_tstart(int lid);
extern void instru_loop_tstart_count(int lid,int edgeid);
extern void instru_loop_tstop(int lid);
extern void instru_loop_tstop_count(int lid,int edgeid);
extern void instru_loop_count(int edgeid);
extern void instru_dump(unsigned long long wall_cycles);
extern void instru_free(void);

#endif
