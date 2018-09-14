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


/* Only one session is handled at the moment (one process using the library)
 * To enable multisession we need an instru_sessions hashtable which key is ppid
 * This key should be passed in every (library's) function call
 * */
static int (*get_thread_id)(void) = NULL;
//Get probes' price
bench_instru_fcts *imcp;
static instru_t *instru_session;
static unsigned long long start_cycles;

/*
 * ...
 */
static int single_thread(void)
{
	return 0;
}

/*
 * Initializes the instrumentation library using all parameters in a single strings, 
 * delimited by the character '|'. It seams to be the library entry point.
 * \param parameters parameters delimited using '|'
 */
void instru_load(char *parameters)
{
	//fprintf(stdout,"PARAMS = %s\n",parameters);
	char *pname,*binfilename,*companion,*binfile_hash,*env;
	char tstr[8196],*tstr1[4];
	int threading_type = 0,nb_threads = 0,nb_functions,nb_calls,nb_loops,nb_edges;

  if( (env = getenv("OMP_NUM_THREADS")) != NULL)
  {
    if(strcmp(env,"") != 0)
    	nb_threads = atoi(env);
  }

  if(nb_threads <= 0)
  {
    fprintf(stderr,"OMP_NUM_THREADS not defined or contains an invalid value.\n");
  	exit(-1);
  }

	strcpy(tstr,parameters);
	pname = strtok(tstr,"|");
	binfilename = strtok(NULL,"|");
	tstr1[0] = strtok(NULL,"|");
	tstr1[1] = strtok(NULL,"|");
	tstr1[2] = strtok(NULL,"|");
	tstr1[3] = strtok(NULL,"|");
	companion = strtok(NULL,"|");
	binfile_hash = strtok(NULL,"|");

	nb_functions = atoi(tstr1[0]);
	nb_calls = atoi(tstr1[1]);
	nb_loops = atoi(tstr1[2]);
	nb_edges = atoi(tstr1[3]);
#if OPENMP
	threading_type 	= 1;
#endif

	instru_init("pname","binfilename",0,1,1,1,1,1,"companion","binfile_hash");
	intru_probes_warmup();
	imcp = intru_probes_price();
  instru_free();

	instru_init(pname,binfilename,threading_type,nb_threads,nb_functions,nb_calls,
							nb_loops,nb_edges,companion,binfile_hash);

	atexit(instru_unload);
	rdtscll(start_cycles);
}

/*
 * Initializes all structures used by the library. Called by instru_load
 * \param pname project name
 * \param binfilename name of the binary to instrument
 * \param threading_type 1 if the program is multi-threaded, else 0
 * \param nb_threads number of threads to instrument
 * \param nb_functions numbers of functions in the binary file
 * \param nb_calls numbers of call in the binary file
 * \param nb_loops numbers of loops in the binary file
 * \param nb_edges
 * \param companion name of the file containing additional data on the binary (such as names)
 * \param binfile_hash md5 hash of the file to instrument
 */
void instru_init(char *pname,char* binfilename,int threading_type,int nb_threads,int nb_functions,
								int nb_calls,int nb_loops,int nb_edges,char *companion,char *binfile_hash)
{
	/*fprintf(stdout,"instru_init(\"%s\",\"%s\",%d,%d,%d,%d,%d,%d,\"%s\",\"%s\")\n",
	   pname,binfilename,threading_type,nb_threads,nb_functions,nb_calls,nb_loops,nb_edges,companion,binfile_hash);*/
	int i,j;
	instru_session = (instru_t *)malloc(sizeof(instru_t));

	switch(threading_type)
	{
		case 0://Single threaded
			get_thread_id = single_thread;
			break;
		case 1://OpenMP
#ifdef OPENMP
			get_thread_id = omp_get_thread_num;
			fprintf(stdout,"MAQAO Instrumentation runtime: using OPENMP Runtime\n");
#else
			get_thread_id = single_thread;
#endif
			break;
		default:
			fprintf(stdout,"Threading type %d not supported\n",threading_type);
			break;
	}

	instru_session->pname 			 = (char *)malloc(strlen(pname)+1 * sizeof(char));
	strcpy(instru_session->pname,pname);

	instru_session->binfilename  = (char *)malloc(strlen(binfilename)+1 * sizeof(char));
	strcpy(instru_session->binfilename,binfilename);

	instru_session->binfile_hash = (char *)malloc(strlen(binfile_hash)+1 * sizeof(char));
	strcpy(instru_session->binfile_hash,binfile_hash);

	instru_session->companion 	 = (char *)malloc(strlen(companion)+1 * sizeof(char));
	strcpy(instru_session->companion,companion);

	instru_session->threading_type = threading_type;
	instru_session->nb_threads	 	 = nb_threads;
	instru_session->nb_functions	 = nb_functions;
	instru_session->nb_calls		   = nb_calls;
	instru_session->nb_loops	 	   = nb_loops;
	instru_session->nb_edges	 	   = nb_edges;
	instru_session->threads 		   = (thread_t *)malloc(sizeof(thread_t)*nb_threads);

	for(i=0;i<nb_threads;i++)
	{
		//fprintf(stdout,"Initializing thread %d data structure\n",i);
		instru_session->threads[i].functions = (function_t *)malloc(sizeof(function_t)*nb_functions);
		instru_session->threads[i].calls	 	 = (call_t *)malloc(sizeof(call_t)*nb_calls);
		instru_session->threads[i].loops 		 = (loop_t *)malloc(sizeof(loop_t)*nb_loops);
		instru_session->threads[i].edges		 = (unsigned long long*)malloc(sizeof(unsigned long long)*nb_edges);
		for(j=0;j<nb_functions;j++)
		{
			instru_session->threads[i].functions[j].elapsed_cycles = 0;
			instru_session->threads[i].functions[j].start_cycles	 = 0;
			instru_session->threads[i].functions[j].stop_cycles		 = 0;
			instru_session->threads[i].functions[j].depth					 = 0;
			instru_session->threads[i].functions[j].instances			 = 0;
		}
		for(j=0;j<nb_calls;j++)
		{
			instru_session->threads[i].calls[j].elapsed_cycles = 0;
			instru_session->threads[i].calls[j].start_cycles	 = 0;
			instru_session->threads[i].calls[j].stop_cycles		 = 0;
			instru_session->threads[i].calls[j].depth					 = 0;
			instru_session->threads[i].calls[j].instances			 = 0;
		}
		for(j=0;j<nb_loops;j++)
		{
			instru_session->threads[i].loops[j].elapsed_cycles = 0;
			instru_session->threads[i].loops[j].start_cycles	 = 0;
			instru_session->threads[i].loops[j].stop_cycles		 = 0;
			instru_session->threads[i].loops[j].iters				   = 0;
			instru_session->threads[i].loops[j].instances		   = 0;
		}
		for(j=0;j<nb_edges;j++){
			instru_session->threads->edges[j] = 0;
		}
	}
}

void instru_fct_tstart(int fid)
{
	unsigned long long tmp_cycles,tmp_cycles2;
	int tid	= get_thread_id();

	function_t *fct = &instru_session->threads[tid].functions[fid];
	//fprintf(stdout,"In instru_fct_tstart (fid=%d,tid=%d,depth=%lld)\n",fid,tid,fct->depth);

	fct->instances++;
	if(fct == NULL)
			fprintf(stdout,"Error trying to access an undefined function (fid %d,tid %d)\n",fid,tid);
	else
	{
		/* Recursivity is handled thanks to a depth attribute
		 * At the end of the profiling depth must be equal to zero (entries == exits).
		 */
		if(fct->depth == 0)
		{
			fct->depth++;
			rdtscll(fct->start_cycles);
			//printf("Set fct->start_cycles for the first time %lld\n",fct->start_cycles);
		}
		else
		{
			fct->depth++;
			rdtscll(tmp_cycles);
			tmp_cycles2 = tmp_cycles - fct->start_cycles;
			/*if(tmp_cycles2 < 0)
				fprintf(stdout,"In instru_fct_tstart : Detected an incoherent measure from rdtsc call (fid %d,tid %d)\n",fid,tid);*/
			fct->elapsed_cycles += tmp_cycles2;
			fct->start_cycles   = tmp_cycles;
		}
		//Copy start state to stop since stop only computes times from last stop
		fct->stop_cycles = fct->start_cycles;
	}

}

void instru_fct_tstop(int fid)
{
	unsigned long long tmp_cycles,tmp_cycles2;
	int tid	= get_thread_id();

	function_t *fct = &instru_session->threads[tid].functions[fid];
	//fprintf(stdout,"In instru_fct_stop (fid=%d,tid=%d,depth=%lld)\n",fid,tid,fct->depth-1);

	if(fct == NULL)
		fprintf(stdout,"Error trying to access an undefined function (fid %d,tid %d)\n",fid,tid);
	else
	{
		rdtscll(tmp_cycles);
		fct->depth--;
		tmp_cycles2 	= tmp_cycles - fct->stop_cycles;
		/*if(tmp_cycles2 < 0)
			fprintf(stdout,"In instru_fct_tstop : Detected an incoherent measure from rdtsc call (fid %d,tid %d)\n",fid,tid);*/
		fct->elapsed_cycles += tmp_cycles2;

		if(fct->depth == 0)
		{
			//printf("In instru_fct_stop : stopping\n");
			fct->start_cycles = 0;
			fct->stop_cycles 	= 0;
		}
		else
		{
			fct->stop_cycles  = tmp_cycles;
		}
	}
}

void instru_fct_call_tstart(int callid)
{
	unsigned long long tmp_cycles,tmp_cycles2;
	int tid = get_thread_id();
	//fprintf(stdout,"In instru_fct_call_tstart (cid=%d,tid=%d)\n",callid,tid);
	call_t *call = &instru_session->threads[tid].calls[callid];

	call->instances++;
	if(call == NULL)
		fprintf(stdout,"Error trying to access an undefined call (cid %d,tid %d)\n",callid,tid);
	else
	{
		/* Recursivity is handled thanks to a depth attribute
		 * At the end of the profiling depth must be equal to zero (#before == #after).
		 */
		if(call->depth == 0)
		{
			call->depth++;
			rdtscll(call->start_cycles);
			//fprintf(stdout,"RDTSC call %d start cycles = %llu | start time %ld\n",callid,call->start_cycles,call->start_time.tv_sec);
		}
		else
		{
			call->depth++;
			rdtscll(tmp_cycles);
			tmp_cycles2 	= tmp_cycles - call->start_cycles;
			/*if(tmp_cycles2 < 0)
				fprintf(stdout,"In instru_fct_call_tstart : Detected an incoherent measure from rdtsc call (fid %d,tid %d)\n",callid,tid);*/
			call->elapsed_cycles += tmp_cycles2;
			call->start_cycles   = tmp_cycles;
		}
		call->stop_cycles	= call->start_cycles;
	}
}

void instru_fct_call_tstop(int callid)
{
	unsigned long long tmp_cycles,tmp_cycles2;
	int tid	= get_thread_id();
	//fprintf(stdout,"In instru_fct_call_tstop (cid %d,tid = %d)\n",callid,tid);
	call_t *call = &instru_session->threads[tid].calls[callid];

	if(call == NULL)
		fprintf(stdout,"Error trying to access an undefined call (cid %d,tid %d)\n",callid,tid);
	else
	{
		rdtscll(tmp_cycles);
		call->depth--;
		tmp_cycles2 = tmp_cycles - call->stop_cycles;
		/*if(tmp_cycles2 < 0)
			fprintf(stdout,"Detected an incoherent measure from rdtsc call (cid %d,tid %d)\n",callid,tid);*/
		call->elapsed_cycles += tmp_cycles2;

		if(call->depth == 0)
		{
			//fprintf(stdout,"RDTSC call %d stop        = %llu cycles | diff = %llu \n",callid,call->stop_cycles,tmp_cycles);
			call->start_cycles = 0;
			call->stop_cycles  = 0;
		}
		else
		{
			call->stop_cycles = tmp_cycles;
		}
	}
}

void instru_loop_tstart(int lid)
{
	//fprintf(stdout,"In instru_loop_tstart lid = %d\n",lid);
	unsigned long long tmp_cycles,tmp_cycles2;
	int tid	= get_thread_id();
	loop_t *loop = &instru_session->threads[tid].loops[lid];

	if(loop == NULL || lid > instru_session->nb_loops)
		fprintf(stdout,"Error trying to access an undefined loop (lid %d,tid %d)\n",lid,tid);
	else
	{
		loop->instances++;
		if(loop->start_cycles != 0) // in case of entry/exit probe collision => just add time
		{
			rdtscll(tmp_cycles);
			tmp_cycles2 	= tmp_cycles - loop->start_cycles;
			/*if(tmp_cycles2 < 0)
				fprintf(stdout,"In instru_loop_tstart : Detected an incoherent measure from rdtsc call (lid %d,tid %d)\n",lid,tid);*/
			loop->elapsed_cycles += tmp_cycles2;
			loop->start_cycles   = tmp_cycles;
		}
		else
		{
			rdtscll(loop->start_cycles);
		}
	}
}

void instru_loop_tstop(int lid)
{
	//fprintf(stderr,"In instru_loop_tstop lid = %d\n",lid);
	int tid		 = get_thread_id();
	loop_t *loop = &instru_session->threads[tid].loops[lid];
	unsigned long long tmp_cycles;

	if(loop == NULL || lid > instru_session->nb_loops)
		fprintf(stdout,"Error trying to access an undefined loop (lid %d,tid %d)\n",lid,tid);
	else
	{
		rdtscll(loop->stop_cycles);
		tmp_cycles = loop->stop_cycles - loop->start_cycles;
		/*if(tmp_cycles < 0)
			fprintf(stdout,"Detected an incoherent measure from rdtsc call (lid %d,tid %d)\n",lid,tid);
		else
		{*/
			loop->elapsed_cycles	+= tmp_cycles;
			loop->start_cycles 		= 0;
			loop->stop_cycles 		= 0;
		//}
	}
}

void instru_loop_tstart_count(int lid,int edgeid)
{
	//fprintf(stderr,"In instru_loop_tstart_count  lid = %d edgeid = %d\n",lid,edgeid);
	int tid				= get_thread_id();
	loop_t *loop 	= &instru_session->threads[tid].loops[lid];
	unsigned long long *edges = instru_session->threads[tid].edges;

	if(loop == NULL || lid > instru_session->nb_loops)
		fprintf(stdout,"Error trying to update an undefined loop (lid %d,tid %d)\n",lid,tid);
	else
	{
		loop->instances++;
		rdtscll(loop->start_cycles);
		//fprintf(stdout,"In instru_loop_tstart_count  lid = %d edgeid = %d | cycles = %llu | time = %ld\n",lid,edgeid,loop->start_cycles,loop->start_time.tv_sec);
	}

	if(edgeid >= instru_session->nb_edges){
		fprintf(stdout,"Error trying to update an undefined edge (eid %d,tid %d)\n",edgeid,tid);
	}else{
		edges[edgeid]++;
	}
}

void instru_loop_tstop_count(int lid,int edgeid)
{
	//fprintf(stderr,"In instru_loop_tstop_count lid = %d edgeid = %d\n",lid,edgeid);
	unsigned long long tmp_cycles;
	int tid				            = get_thread_id();
	loop_t *loop 		          = &instru_session->threads[tid].loops[lid];
	unsigned long long *edges = instru_session->threads[tid].edges;

	if(loop == NULL || lid > instru_session->nb_loops)
	{
		fprintf(stdout,"Error trying to access an undefined loop (lid %d,tid %d)\n",lid,tid);
	}
	else
	{
		rdtscll(loop->stop_cycles);
		tmp_cycles  = loop->stop_cycles - loop->start_cycles;
		/*if(tmp_cycles < 0)
			fprintf(stdout,"Detected an incoherent measure from rdtsc call (lid %d,eid %d,tid %d)\n",lid,edgeid,tid);
		else
		{*/
			loop->elapsed_cycles 	+= tmp_cycles;
			//fprintf(stdout,"In instru_loop_tstop_count  lid = %d edgeid = %d | cycles = %llu - diff = %llu | time = %ld - diff = %f\n",lid,edgeid,loop->start_cycles,loop->elapsed_cycles,loop->start_time.tv_sec,loop->elapsed_time);
			loop->start_cycles 	 	= 0;
			loop->stop_cycles 	 	= 0;
		//}
	}
	if(edgeid >= instru_session->nb_edges){
		fprintf(stdout,"Error trying to update an undefined edge (eid %d,tid %d)\n",edgeid,tid);
	}else{
		edges[edgeid]++;
	}
}

void instru_loop_backedge_count(int lid,int edgeid)
{
	//fprintf(stderr,"In instru_loop_backedge_count lid = %d edgeid = %d\n",lid,edgeid);
	int tid				            = get_thread_id();
	loop_t *loop 		          = &instru_session->threads[tid].loops[lid];
	unsigned long long *edges = instru_session->threads[tid].edges;

	if(loop == NULL || lid > instru_session->nb_loops){
		fprintf(stdout,"Error trying to update an undefined loop (lid %d,tid %d)\n",lid,tid);
	}else{
		loop->iters++;
	}

	if(edgeid >= instru_session->nb_edges){
		fprintf(stdout,"Error trying to update an undefined edgeid (eid %d,tid %d)\n",edgeid,tid);
	}else{
		edges[edgeid]++;
	}
}

void instru_block_count(int edgeid)
{
	//fprintf(stdout,"In instru_loop_count edgeid = %d\n",edgeid);
	int tid	= get_thread_id();
	unsigned long long *edges = instru_session->threads[tid].edges;

	if(edgeid >= instru_session->nb_edges){
		fprintf(stdout,"Error trying to update an undefined edge (eid %d,tid %d)\n",edgeid,tid);
	}else{
		edges[edgeid]++;
	}
}

void instru_terminate(void)
{
   int i,j;
   //fprintf(stderr,"Start instru_terminate ntid = %d nfid = %d\n",instru_session->nb_threads,instru_session->nb_functions);
   for(i=0;i<instru_session->nb_threads;i++)
   {
      for(j=0;j<instru_session->nb_functions;j++)
      {
         if(instru_session->threads[i].functions[j].depth > 0)
         {
            fprintf(stdout,"Function %d being stop because of early exit (tid %d,depth %lld)\n",
            		j,i,instru_session->threads[i].functions[j].depth);
            instru_fct_tstop(j);
         }
      }
   }
}
/*
 * Dumps aggregated data from runtime into a Lua result file that will be loaded by the lprof module
 *
 */
/*TODO: To address Lua 64K constants limitation: split threads | functions | loops | calls | edges into function call
 *      => build functions with suffixes get_thread_THREADID,get_function_FUNCTIONID ...
 */
void instru_dump(unsigned long long wall_cycles)
{
	int i,j;
	char output_name[256];
	char str[256];
	char *hash,*tmp;
	int sid;

	strcpy(str,instru_session->binfile_hash);
	hash = strtok(str,"_");
	tmp = strtok(NULL,"_");

   // Disable warning "hash and tmp not used"
   (void)hash;
   (void)tmp;

	//sid = atoi(strtok(NULL,"_"));
	sid=0;

	//fprintf(stdout,"Dumping Threads [%d]\n",instru_session->nb_threads);
	/*fprintf(stdout,"instru_init(\"%s\",\"%s\",%d,%d,%d,%d,%d,%d,\"%s\",\"%s\");\n",instru_session->pname,instru_session->binfilename,
			instru_session->threading_type,instru_session->nb_threads,instru_session->nb_functions,instru_session->nb_calls,instru_session->nb_loops,
			instru_session->nb_edges,instru_session->companion,instru_session->binfile_hash);*/
	sprintf(output_name,"%s.rslt",instru_session->binfile_hash);
	FILE *trace = fopen(output_name,"w");
	fprintf(trace,"local instru_session = mil:project_instru_get_sess(\"%s\",%d);\n",instru_session->pname,sid);
	fprintf(trace,"if(instru_session == nil) then\n");
	fprintf(trace,"  print(\"Cannot load trace file\");\n  os.exit()\n");
	fprintf(trace,"end\n");
	fprintf(trace,"instru_session.rslt = {\n");

  /*bench_instru_fcts *bif;
  for (bif = imcp; bif->id != -1; bif++)
     fprintf(stdout,"Bench time for %s => AVG Overhead = %d cycles\n",bif->name,bif->avg_overhead );*/

  fprintf(trace,"price_tprobe = %d,\n",imcp[1].avg_overhead - imcp[0].avg_overhead);
	fprintf(trace,"price_fct = %d,\n",imcp[0].avg_overhead);
	fprintf(trace,"price_call = %d,\n",imcp[0].avg_overhead);
	fprintf(trace,"price_loop = %d,\n",imcp[0].avg_overhead);
	fprintf(trace,"price_loop_count = %d,\n",imcp[0].avg_overhead);
	fprintf(trace,"pname = \"%s\",\n",instru_session->pname);
	fprintf(trace,"binfilename = \"%s\",\n",instru_session->binfilename);
	fprintf(trace,"binfile_hash = \"%s\",\n",instru_session->binfile_hash);
	fprintf(trace,"companion = \"%s\",\n",instru_session->companion);
	fprintf(trace,"threading_type = %d,\n",instru_session->threading_type);
	fprintf(trace,"nb_threads = %d,\n",instru_session->nb_threads);
	fprintf(trace,"nb_functions = %d,\n",instru_session->nb_functions);
	fprintf(trace,"nb_calls = %d,\n",instru_session->nb_calls);
	fprintf(trace,"nb_loops = %d,\n",instru_session->nb_loops);
	fprintf(trace,"nb_edges = %d,\n",instru_session->nb_edges);
	fprintf(trace,"wallcycles = %llu,\n",wall_cycles);
	fprintf(trace,"callsite_edges = false,\n");
	fprintf(trace,"threads = {\n");

	for(i = 0;i < instru_session->nb_threads;i++)
	{
		fprintf(trace,"[%d] = {\n",i);
		fprintf(trace,"functions = {\n");
		for(j = 0;j < instru_session->nb_functions;j++)
		{
			fprintf(trace,"[%d] = {\n",j);
			fprintf(trace,"elapsed_cycles = %llu,\n",instru_session->threads[i].functions[j].elapsed_cycles);
			fprintf(trace,"instances = %llu,\n",instru_session->threads[i].functions[j].instances);
			fprintf(trace,"};\n");
		}
		fprintf(trace,"};\ncalls = {\n");
		for(j = 0;j < instru_session->nb_calls;j++)
		{
			fprintf(trace,"[%d] = {\n",j);
			fprintf(trace,"elapsed_cycles = %llu,\n",instru_session->threads[i].calls[j].elapsed_cycles);
			fprintf(trace,"instances = %llu,\n",instru_session->threads[i].calls[j].instances);
			fprintf(trace,"};\n");
		}
		fprintf(trace,"};\nloops = {\n");
		for(j = 0;j < instru_session->nb_loops;j++)
		{
			fprintf(trace,"[%d] = {\n",j);
			fprintf(trace,"elapsed_cycles = %llu,\n",instru_session->threads[i].loops[j].elapsed_cycles);
			fprintf(trace,"instances = %llu,\n",instru_session->threads[i].loops[j].instances);
			fprintf(trace,"iters = %llu,\n",instru_session->threads[i].loops[j].iters);
			fprintf(trace,"};\n");
		}
		fprintf(trace,"};\nedges = {\n");
		for(j=0;j<instru_session->nb_edges;j++){
			fprintf(trace,"[%d] = %llu,\n",j,instru_session->threads[i].edges[j]);
		}
		fprintf(trace,"}\n");//functions
		fprintf(trace,"};\n");//thread
	}
	fprintf(trace,"}\n");//threads
	fprintf(trace,"}\n");//sessions
	fclose(trace);
}

void instru_unload(void)
{
  unsigned long long stop_cycles;
  unsigned long long tmp_cycles;

  instru_terminate();
  rdtscll(stop_cycles);
  tmp_cycles = stop_cycles - start_cycles;
  //fprintf(stderr,"Instru Unload: WT cycles = %lld\n",tmp_cycles);
  instru_dump(tmp_cycles);
  instru_free();
}

void instru_free(void)
{
	//fprintf(stdout,"In instru_free \n");
}
