--
--  Copyright (C) 2004 - 2018 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)
--
-- This file is part of MAQAO.
--
-- MAQAO is free software; you can redistribute it and/or
--  modify it under the terms of the GNU Lesser General Public License
--  as published by the Free Software Foundation; either version 3
--  of the License, or (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU Lesser General Public License for more details.
--
--  You should have received a copy of the GNU Lesser General Public License
--  along with this program; if not, write to the Free Software
--  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
---

module ("lprof.help", package.seeall)

-- require "lprof.instru_mode.help"

function lprof:init_help()
   local help = Help:new();

   help:set_name ("maqao-lprof");
   help:set_usage ("<.br>\tData collection mode : maqao lprof [options] -- <APPLICATION> [arguments, if any]\n"..
                   "<.br>\tData display mode    : maqao lprof -xp=<EXPERIMENT_DIRECTORY> [--display-functions | --display-loops] [options]");

   help:set_description ("<.br>The MAQAO Lightweight Profiler (LProf) allows profiling of target applications to detect hot functions and loops in 2 steps.\n"..
                         "<.br>\t1) Data collection (sampling)\n"..
                         "<.br>\t2) Data display");

   help:add_option ("experiment-path", "xp=", "<path>", false,
                    "Experiment directory where the collected data is stored. It is generated in the current directory during data collection step.\n"..
                    "<.br>If not specified, a default pattern will be used: maqao_lprof_YYYY-MM-DD-hh-mm-ss.\n"..
                    "<.br>Y: year, M: month, D: day, h: hour, m: minute, s: second.\n"..
                    "<.br>/!\\ Warning: at display, this directory is used to load the collected data and is a mandatory parameter."
   );

   help:add_separator("DATA COLLECTION");

   help:add_option ("maximum-buffer-megabytes", nil, "<integer>", false,
                    "Limit RAM usage during samples collection to X Megabytes. Default is 1024 (1 GB).")

   help:add_hidden_option ("tmpfiles-buffer-megabytes", nil, "<integer>", false,
                           "Limit RAM used for dumping samples to temp. files to X Megabytes. Default is 20 MB.\n"..
                           "Cannot be greater than maximum-tmpfiles-megabytes.")

   help:add_hidden_option ("maximum-tmpfiles-megabytes", nil, "<integer>", false,
                           "Limit total temporary files size during samples collection to X Megabytes. Default is 1024 (1 GB).\n"..
                           "Cannot be lower than tmpfiles-buffer-megabytes.")

   help:add_option ("mpi-command", "mc=", "<MPI command>", false,
                    "MPI command used to interactively run application. LProf prepends this to application command.\n"..
                    "When combined with batch-script, substitutes <mpi_command> in job script")

   help:add_option ("batch-script", "bs=", "<path to batch script>", false,
                    "Batch script used to run application. LProf submits this file to <batch-command>.\n"..
                    "In this script, replace application run command with \"<run_command>\".")

   help:add_option ("batch-command", "bc=", "<batch submission command>", false,
                    "Tell LProf how to submit <batch-script>. If omitted, guessed from <batch-script> extension.")

   help:add_option ("checking-directory", nil, "on/off [DEPRECATED]", false,
                    "Use check-directory/cd."
   );

   help:add_option ("check-directory", "cd=", "on/off", false,
                    "Disable checking if the specified experiment directory already exists.\n"..
                    "<.br>By default, a check is always performed to avoid modifying an existing experiment directory."
   );

   help:add_option ( "granularity", "g=", "small/medium/large [DEPRECATED]", false,
                     "Use sampling-rate.")

   help:add_option ( "sampling-rate", nil, "low/medium/high/highest", false,
                     "Change the sampling rate (number of collected samples).\n"..
                     "<.br>Four rates are available depending on the execution time of the application:\n"..
                     "<.br>  - highest: a few seconds\n"..
                     "<.br>  - high   : less than 1 min\n"..
                     "<.br>  - medium : between 1 min and 1 hour (default)\n"..
                     "<.br>  - low    : over 1 hour"
   );

   help:add_option ("hardware-counters", "hwc=", "<list> [ADVANCED]", false,
                    "Provide a custom list of hardware counters to profile.\n"..
                    "<.br>For each hardware counter, the threshold value should be set using format \"@VALUE\".\n"..
                    "<.br>Raw codes can be used as well as hardware counter names.\n"..
                    "<.br>ex: hwc=CPU_CLK_UNHALTED@1000000,0x412e@1000000,INST_RETIRED@500000"
   );

   help:add_hidden_option ("profile", "p", "<string>", false,
                           "Use ready-to-use lists of hardware events.")

   help:add_option ("detect-kernel", nil, "on (default)/off", false,
                    "Disable kernel version check.\n"..
                    "<.br>/!\\ Warning: LProf handles kernels from 2.6.32-279 version, and higher.\n"..
                    "<.br>For other kernel versions, the tool's behavior may be undefined and the output data not valid."
   );

   help:add_option ("library-debug-info", "ldi=", "<list>/on/off", false,
                    "Analyze libraries debug information to locate loops with the --display-loops option, and also retrieve inlined library functions.\n"..
                    "<.br>Libraries need to be compiled with the -g option.\n"..
                    "<.br>Allowed values are:\n"..
                    "<.br>  - \"lib1.so, lib2.so, ...\" : Get loops information only for libraries in the list. Use names as displayed by ldd <application executable>.\n"..
                    "<.br>  - on                      : Get loops information for all libraries.\n"..
                    "<.br>  - off                     : Get only functions information using ELF information for all libraries (default).\n"..
                    "<.br><.br>/!\\ Warning: this option can increase the analysis overhead."
   );

   help:add_option ("user-guided", "ug=", "<delay (seconds)> (timer mode) / on (signal mode)", false,
                    "Allow user to control the sampling in two modes:\n"..
                    "<.br>  - timer mode  : user-defined delay (in seconds) before the data collection process.\n"..
                    "<.br>                  May be useful to ignore the initialization step of an application.\n"..
                    "<.br>  - signal mode : use SIGTSTP signal (CTRL+Z) to pause/resume the data collection process\n"..
                    "<.br>                  Can be used as many times as necessary during the run of the application."
   );

   help:add_option ( "backtrace-mode", "btm=", "call/stack/branch/off", false,
                     "[Advanced] Select the perf_event_open sample type used to collect callchains:\n"..
                     "<.br>  - call   : use the PERF_SAMPLE_CALLCHAIN sample type (default).\n"..
                     "<.br>  - stack  : use the PERF_SAMPLE_STACK_USER sample type.\n"..
                     "<.br>             Allows stack unwinding. Requires Linux 3.7.\n"..
                     "<.br>  - branch : use the PERF_SAMPLE_BRANCH_STACK sample type.\n"..
                     "<.br>             Uses CPU sampling hardware. Requires Linux 3.4.\n"..
                     "<.br>  - off    : Disable callchains collection.\n"..
                     "<.br>             Reduces sampling overhead and experiment directory size but some OpenMP/MPI functions/loops will no more be correctly categorized at display."
   );

   help:add_option ("target", "t=", "SX3... [DEPRECATED]", false,
                    "Use use-alternative-engine (*) [(*) hidden options].");
                    
   help:add_hidden_option ("use-alternative-engine", "ae", nil, false,
                           "Use alternative sampling engine (MAQAO version > 2.4).")

   help:add_option ("use-OS-timers", nil, nil, false,
                    "Use OS timers based sampling engine.\n"..
                    "Needed in case of unavailable HW counters or undetected processor.")

   help:add_hidden_option ("cpu-list", "cpu=", "<string>", false,
                           "CPUs to use with alternative/default engine (MAQAO version > 2.4).\n"..
                           "Ex: 0,2 to use CPU0 and CPU2.")

   help:add_hidden_option ("mpi-target", "mt=", "<path>", false,
                           "Path to application executable, to use with alternative engine (MAQAO version > 2.4).")

   help:add_hidden_option ("sampler-threads", "st=", "\"nprocs\" or <integer>", false,
                           "Number of threads to process records in ring buffers, to use with alternative/default engine (MAQAO version > 2.4).")

   help:add_hidden_option ("finalize-signal", "fs=", "<positive integer>", false,
                           "Signal used by the parallel launcher to notify ranks finalization.\n"..
                           "Allows default sampling engine to catch non standard launcher behavior.")

   help:add_hidden_option ("sync-tracer [EXPERIMENTAL]", "sy", nil, false,
                           "Synchronous (instead of asynchronous) ptracing for default engine.")

   -- lprof.instru_mode.help.add_collect (help);

   help:add_separator("DATA DISPLAY");
   -- help:add_separator("   SAMPLING MODE");

   help:add_option ("display-functions", "df", nil, false,
                    "Display the exclusive time spent in the aplication, libraries and system functions."
   );

   help:add_option ("display-loops", "dl", nil, false,
                    "Display the exclusive time spent in the application loops.\n"..
                    "<.br>If used with library-debug-information option during the collection, library loops information will be displayed too."
   );

   help:add_option ("view", "v=", "summary/full [DEPRECATED]", false,
                    "Use display-by-threads.")

   help:add_option ("display-by-threads", "dt", nil, false,
                    "Information is displayed by thread."
   );

   help:add_option ("categorization", "c=", "summary/<level>/full [DEPRECATED]", false,
                    "Use category-view.")

   help:add_option ("category-view", "cv=", "summary/<level>/full", false,
                    "Display categorization table at various levels:\n"..
                    "<.br>  - summary : The information for threads/processes/nodes are grouped in one table (default).\n"..
                    "<.br>  - node    : node level.\n"..
                    "<.br>  - process : process level.\n"..
                    "<.br>  - thread  : thread level.\n"..
                    "<.br>  - full    : all the above categorization tables (summary, node, process, thread).\n"..
                    "<.br>"..
                    "<.br>The categorization table shows the time percentage for each of the categories below:\n"..
                    "<.br>  - Application   : application executable.\n"..
                    "<.br>  - MPI           : MPI runtime (openmpi, mpich, intel mpi,...).\n"..
                    "<.br>  - OMP           : OpenMP runtime (gomp, iomp...).\n"..
                    "<.br>  - Math          : Math libraries (libm, libmkl, libblas...).\n"..
                    "<.br>  - System        : system interface (linux system calls).\n"..
                    "<.br>  - Pthread       : Pthread runtime.\n"..
                    "<.br>  - I/O           : I/O functions.\n"..
                    "<.br>  - String        : string manipulation functions (strcpy, trim...).\n"..
                    "<.br>  - Memory        : memory management functions (malloc, free...).\n"..
                    "<.br>  - Others        : functions that are not of the categories above."
   );

   help:add_option ("libraries-extra-categories", "lec=", "<comma-separated list>", false,
                    "Consider specified libraries as extra categories.\n"..
                    "Use libraries names as given by 'ldd <application>'.")

   help:add_option ( "output-format", "of=", "html/csv", false,
                     "Output results in a file of the given format:\n"..
                     "<.br>  - html : generate a web page in <PROFILING_DIRECTORY>/html directory. Open html/index.html in a web browser to view the results.\n"..
                     "<.br>  - csv  : generate a csv file for each thread (default name: <CURRENT_DIRECTORY>/maqao_<NODE-NAME>_<THREAD-ID>.csv)."
   );

   help:add_option ("output-path [SHOULD BE USED WITH THE output-format OPTION]", "op=", "<path>", false,
                    "Specifiy the path of the generated results files."
   );

   help:add_option ("output-prefix [SHOULD BE USED WITH THE output-format OPTION]", nil, "<string>", false,
                    "Add a custom prefix to the generated results files."
   );

   help:add_option ("callchain-lib [DEPRECATED]", "ccl", nil, false,
                    "Extend the callchain scope to external libraries function calls."
   );

   help:add_option ("callchain-all [DEPRECATED]", "cca", nil, false,
                    "Display the callchain with no limited scope (application + libraries + system calls)."
   );

   help:add_option ("callchain-off [DEPRECATED]", "cco", nil, false,
                    "Disable callchains analysis. Some OpenMP/MPI functions/loops will no more be correctly categorized. Use this only when display takes too much time/memory."
   );

   help:add_option ("callchain", "cc", "exe/lib/all/off", false,
                    "Specify objects for callchains analysis:\n"..
                    "<.br>  - exe: display the callchain (if available) for each function with a scope limited to the application.\n"..
                    "<.br>  - lib: extend the callchain scope to external libraries function calls.\n"..
                    "<.br>  - all: display the callchain with no limited scope (application + libraries + system calls).\n"..
                    "<.br>  - off: disable callchains analysis. Some OpenMP/MPI functions/loops will no more be correctly categorized. Use this only when display takes too much time/memory."
   );

   help:add_option ("callchain-value-filter", "cvf=", "<integer between 0 and 100> [DEPRECATED]", false,
                    "Use callchain-weight-filter.");
   
   help:add_option ("callchain-weight-filter", "cwf=", "<integer between 0 and 100>", false,
                    "Filter callchains that don't represent at least X percent of time in the function reference"
   );

   help:add_hidden_option ("show-samples-value", "ssv=", "on/off (default)", false,
                           "Display the number of samples collected (in between brackets).");

   help:add_option ("cumulative-threshold", "ct=", "<integer between 0 and 100>", false,
                    "Display the top loops/functions which cumulative percentage is greater than the given value (e.g: ct=50)."
   );

   -- lprof.instru_mode.help.add_display (help);

   --Utils:load_common_help_options (help)

   -- COLLECT SAMPLING MODE EXAMPLES
   help:add_example ("maqao lprof -- <APPLICATION>",
                     "Launch the profiler in collect sampling mode on a sequential application.\n"..
                     "<.br>It stores the results into a default experiment directory (maqao_lprof_YYYY-MM-DD-hh-mm-ss).\n"..
                     "<.br>Y: year, M: month, D: day, h: hour, m: minute, s: second\n");
   
   help:add_example ("maqao lprof --mpi-command=\"mpirun -n 4\" -- <APPLICATION>",
                     "Same as previous example but for MPI application with 4 processes.\n");

   help:add_example ("maqao lprof -xp=<EXPERIMENT_DIRECTORY> [--mpi-command=\"mpirun -n 4\"] -- <APPLICATION>  arg1 arg2 ...",
                     "If the application needs one or more arguments, make sure to use the '--' delimiter.\n"..
                     "<.br>Here, results are stored into the directory given by the user.\n");

   help:add_example ("maqao lprof -xp=<EXPERIMENT_DIRECTORY> -df",
                     "Display the list of functions coming from the experiment directory into the terminal.\n"..
                     "<.br>The function display mode allows to localized where are the hot functions of the application.\n");

   help:add_example ("maqao lprof -xp=<EXPERIMENT_DIRECTORY> -df -cc=exe -cv=full",
                     "Display the list of functions coming from the experiment directory into the terminal.\n"..
                     "<.br>The -cc=exe (--callchain) option allows to display the callchains.\n"..
                     "<.br>The -cv=full (--category-view) option allows to display all the categorization tables.\n"..
                     "<.br>The function display mode allows to localized where are the hot functions of the application.\n");

   help:add_example ("maqao lprof -xp=<EXPERIMENT_DIRECTORY> -df -dt -of=csv -op=$PWD/help_example",
                     "Generate a CSV file (-of=csv) for each thread (-dt) with the functions info (-df) into $PWD/help_example (-op=...).\n"..
                     "Specified directory for -op option must exist. If not, files will not be created.\n");


   help:add_example ("maqao lprof -xp=<EXPERIMENT_DIRECTORY> -dl",
                     "Display the list of loops coming from the experiment directory into the terminal.\n"..
                     "<.br>The loop display mode pinpoints hot loops in application.\n");

   help:add_example ("maqao lprof -xp=<EXPERIMENT_DIRECTORY> -of=html",
                     "Generate the \"html\" directory into <EXPERIMENT_DIRECTORY>/html.\n"..
                     "<.br>Open file <EXPERIMENT_DIRECTORY>/html/index.html in a web browser to view the results.\n");


   -- lprof.instru_mode.help.add_examples (help);

   return help;
end
