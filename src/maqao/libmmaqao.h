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

#ifndef __LMAQAO_LIBMAQAO_H__
#define __LMAQAO_LIBMAQAO_H__

/**
 * \file libmmaqao.h
 * \brief Libmmaqao provides some high level functions to disassemble and analyze binaries.
 * \page page8 Libmmaqao documentation page
 */

#include "libmcommon.h"
#include "libmasm.h"

/**
 * Wrapper for storing parameters when invoking CQA
 * */
typedef struct wrapper_cqa_params_s {
   char *arch; /**< Architecture */
   char* uarch_name; /**< Microarchitecture */
   char *ml; /**< Memory level(s) */
   char *mlf_insn; /**< Microbench instructions files */
   char *mlf_pattern; /**< Microbench patterns files */
   char *asm_input_file; /**< Input assembly file name */
   char *csv_output_file; /**< Output CSV file name */
   char *user; /**< User defined function to customize the output format/data */
   int mode; /**< Mode (loop, function loops, function) */
   char *value; /**< Value associated to the mode (either function name or loop id) */
   int vunroll; /**< Virtual unroll factor value */
   char *fc; /**< Follow call transformation (can be either inline or append) */
} wrapper_cqa_params_t;

/**
 * Microbench generation and execution mode
 * */
typedef enum microbench_modes_e {
   MICROBENCH_GEN_RUN = 0, /**<Samples are generated and run*/
   MICROBENCH_GEN_ONLY,    /**<Samples are generated only*/
   MICROBENCH_RUN_ONLY,    /**<Samples are executed only*/
   MICROBENCH_MAX_MODES    /**<Max number of possible modes (must always be last)*/
} microbench_modes_t;

/**
 * Wrapper for storing parameters when invoking microbench
 * */
typedef struct wrapper_microbench_params_s {
   char *arch;                /**< Architecture */
   char *config_file;         /**< Configuration file */
   char* config_template;     /**< Configuration template*/
   microbench_modes_t mode;   /**< Generation and execution mode*/
} wrapper_microbench_params_t;

/**
 * Sets the information relative to a processor version in a project.
 * If neither uarch_name nor proc_name are set, the function will attempt retrieving the processor version from the host
 * If either one is set, the function will attempt retrieving the architecture from the file, and deduce the processor version
 * from the name. If the architecture can not be deduced from the file (or filename is NULL), the values will be stored in the project
 * \param project The project
 * \param filename The name of a file associated to the project
 * \param arch_name The name of the architecture used in the project
 * \param uarch_name The name of the micro-architecture to associate to the project if \c proc_name is NULL
 * \param proc_name The name of the processor version to associate to the project
 * \return EXIT_SUCCESS or error code if the processor version could not be successfully deduced from the parameters
 * */
extern int project_init_proc(project_t* project, char* file_name, char* arch_name, char* uarch_name, char* proc_name);

/**
 * Adds and analyzes an asmfile into a project
 * \param project an existing project
 * \param filename name of the assembly file to load and analyze
 * \param uarch_name Name of the assembly file micro architecture
 * \return a new asmfile structure
 */
extern asmfile_t* project_load_file(project_t *project, char *filename,
      char *uarch_name);

/**
 * Parses a binary and fills an asmfile object
 * \param project an existing project
 * \param filename name of the assembly file to load and analyze
 * \param uarch_name Name of the assembly file micro architecture
 * \return a new asmfile structure
 */
extern asmfile_t* project_parse_file(project_t *project, char *filename,
      char *uarch_name);

/**
 * Adds and analyzes an asmfile into a project
 * \param project an existing project
 * \param filename name of the assembly file to load and analyze
 * \param archname Name of the architecture
 * \param uarch_name Name of the micro architecture
 * \return a new asmfile structure
 */
extern asmfile_t* project_load_asm_file(project_t *project, char *filename,
      char* archname, char *uarch_name);

/**
 * Adds and analyzes a formatted assembly text file into a project
 * \param project an existing project
 * \param filename Name of the formatted assembly file to load and analyze
 * \param content Content of the formatted assembly file to load and analyze. Will be used if \c filename is NULL
 * \param archname Name of the architecture
 * \param uarch_name Description of the micro architecture
 * \param fieldnames Names of the fields and sections containing informations about the assembly elements
 * \return A new asmfile structure
 */
extern asmfile_t* project_load_txtfile(project_t *project, char *filename,
      char* content, char* archname, char *uarch_name,
      const asm_txt_fields_t *fieldnames);

/**
 * Uses CPUID assembly instruction to found local processor version
 * \return NULL if processor version can not be retrieved, else a structure representing the processor version
 */
extern proc_t* utils_get_proc_host();

/**
 * Uses CPUID assembly instruction to get data on processor
 * \return a string to free or NULL
 */
extern char* utils_get_cpu_frequency();

/**
 * Wrapper function to launch the microbench module
 * \param params An instance of microbench wrapper structure
 * \return An error code
 */
extern int maqao_launch_microbench(wrapper_microbench_params_t params);

/**
 * Wrapper function to launch the cqa module
 * \param params An instance CQA Wrapper structure
 * \return An error code
 */
extern int maqao_launch_cqa(wrapper_cqa_params_t params);

#endif
