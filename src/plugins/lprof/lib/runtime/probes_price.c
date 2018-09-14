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

#include "libinstru.h"

// Only used at initialization for overhead benchmarking
static bench_instru_fcts functions_to_bench[] = {
		{0,"instru_probes_call_empty",0},
		{1,"instru_probes_call_rdtsc",0},
		/*{2,"instru_fct_tstop",0},
		{3,"instru_fct_call_tstop",0},
		{4,"instru_loop_tstop",0},
		{5,"instru_loop_tstop_count",0},*/
		{-1,"",0}
};

#define WARMITERS 1024
#define FORI 512
#define FORJ 512

/*
 * ...
 */
void intru_probes_warmup()
{
	int i;
	unsigned long long timer;
	//fprintf(stdout,"Starting warmup routine\n");

	for(i=0;i<WARMITERS;i++)
		rdtscll(timer);

   (void)timer;
}

/*
 * Does nothing. Used to simulate a function call
 */
void instru_probes_call_empty(void)
{
   return;
}

/*
 * Used to simulate a function call using rdtsc function inside
 */
void instru_probes_call_rdtsc(void)
{
	unsigned long long tmp_cycles;

	rdtscll(tmp_cycles);

   (void)tmp_cycles;
}

/*
 * Used to measurate a part of the instrumentation overhead
 * \param id action to do.
 *    0 => call a dummy function
 *    1 => function probe
 *    2 => call probe
 *    3 => loop probe
 *    4 => loop_count probe
 */
static void instru_dummy(int id)
{
}

/*
 *
 */
bench_instru_fcts *intru_probes_price(void)
{
   int i,j;
   unsigned long long start_cycles,stop_cycles,tmp_cycles;
   bench_instru_fcts *bif;

   for (bif = functions_to_bench; bif->id != -1; bif++)
   {
      rdtscll(start_cycles);
      for(i=0;i<FORI;i++)
         for(j=0;j<FORJ;j++)
            instru_dummy(bif->id);
      rdtscll(stop_cycles);
      tmp_cycles  = stop_cycles - start_cycles;
      bif->avg_overhead = tmp_cycles / (FORI * FORJ);
      //fprintf(stdout,"Bench time for %s => AVG Overhead = %d cycles\n",bif->name,bif->avg_overhead );
   }

   return functions_to_bench;
}

