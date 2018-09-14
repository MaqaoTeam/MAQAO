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

#ifndef _WIN32
#include <libgen.h>
#endif
#include <string.h>
#include <inttypes.h>

#include "libmasm.h"
#include "libelf.h"
#include "libmtroll.h"
#include "libmdbg.h"
#include "DwarfLight.h"

/*
 * Load debug data for a given function from DWARF
 * \param f a function
 * \param dwarf A structure containing DWARF debug data
 */
static void dbg_fct_load_dwarf(fct_t* f, DwarfAPI* dwarf)
{
   if (f->debug != NULL)
      return;

   insn_t* in = fct_get_first_insn(f);
   DwarfFunction* dfct = NULL;

   dfct = dwarf_api_get_function_by_linkname(dwarf, fct_get_name(f));

   if (dfct == NULL) {
      if (f->dbg_addr == -1)
         dfct = dwarf_api_get_function_by_addr(dwarf, INSN_GET_ADDR(in));
      else
         dfct = dwarf_api_get_function_by_addr(dwarf, f->dbg_addr);
   }

   if (dfct == NULL && in->debug != NULL)
      dfct = dwarf_api_get_function_by_src(dwarf, in->debug->srcfile,
            in->debug->srcline);

   if (dfct == NULL) {
      f->debug = NULL;
      return;
   }
   DwarfFile* dfile = dwarf_function_get_file(dfct);

   f->debug = lc_malloc(sizeof(dbg_fct_t));
   f->debug->file = dwarf_file_get_name(dfile);
   f->debug->data = dfct;
   f->debug->language = dwarf_file_get_language(dfile);
   f->debug->compiler = dwarf_file_get_vendor(dfile);
   f->debug->lang_code = dwarf_file_get_language_code(dfile);
   f->debug->version = dwarf_file_get_version(dfile);
   f->debug->decl_line = dwarf_function_get_srcl(dfct);
   f->debug->comp_code = dwarf_file_get_producer_code(dfile);

   f->debug->name = dwarf_function_get_name(dfct);

   if (f->demname == NULL && f->debug->lang_code == LANG_CPP
         && (f->debug->comp_code == COMP_GNU
               || f->debug->comp_code == COMP_INTEL))
      f->demname = fct_demangle(fct_get_name(f), f->debug->comp_code,
            f->debug->lang_code);
}



/*
 * Load debug data for all instructions in an asmfile from DWARF
 * \param asmf an asmfile containing instructions to analyze
 * \param dwarf A structure containing DWARF debug data
 */
static void dbg_insn_load_dwarf(asmfile_t* asmf, DwarfAPI* dwarf)
{
   maddr_t* addrs = NULL;
   int* srcs = NULL;
   char** filename = NULL;
   unsigned int size = 0;
   unsigned int current_dbg_addr = 0;  //Index of the current dwarf address
   unsigned int current_dbg_range = 0; //Index of the current dwarf range of addresses
   maddr_t* ranges_starts = NULL;
   maddr_t* ranges_stops = NULL;
   unsigned int n_ranges = 0;

   // Retrieve all debug data from Dwarf
   dwarf_api_get_all_lines(dwarf, &filename, &addrs, &srcs, &size);
   // Retrieve all function info from Dwarf
   n_ranges = dwarf_api_get_debug_ranges(dwarf, &ranges_starts, &ranges_stops);

   DBGLVL(1, FCTNAMEMSG0("Debug data:\n");
      unsigned int j;
      STDMSG(" Instruction lines:\n");
      for (j = 0; j < size; j++) {
         STDMSG("  %#"PRIx64": %s:%d\n", addrs[j], filename[j], srcs[j]);
      }
      STDMSG(" Debug ranges:\n");
      for (j = 0; j < n_ranges; j++) {
         STDMSG("[%#"PRIx64"-%#"PRIx64"]\n", ranges_starts[j], ranges_stops[j]);
      }
   )

   queue_t* insns = asmfile_get_insns(asmf);

   //Skips addresses from the dwarf lower than the address of the first instruction (instructions and dwarf addresses are ordered)
   maddr_t first_insn_addr = insn_get_addr(queue_peek_head(insns));
   while(current_dbg_addr < size && first_insn_addr > addrs[current_dbg_addr])
      current_dbg_addr++;
   // No debug data or all debug at addresses lower than the first one: nothing to do (debug is set at NULL at instruction creation)
   if (size == current_dbg_addr)
      return;
   //Also skips ranges whose ending address is lower than the first address
   while(current_dbg_range < n_ranges && first_insn_addr > ranges_stops[current_dbg_range])
      current_dbg_range++;

   //At this point, first_insn_addr <= addrs[current_dbg_addr] and first_insn_addr <= ranges_stops[current_dbg_range]
   //There is no guarantee though that first_insn_addr >= ranges_starts[current_dbg_range]

   FOREACH_INQUEUE(insns, it_i) {
      insn_t* in = GET_DATA_T(insn_t*, it_i);
      maddr_t in_addr = insn_get_addr(in);
      DBGMSGLVL(2, "[%#"PRIx64"] Updating debug information\n", in_addr)
      while (current_dbg_addr < (size - 1) && in_addr >= addrs[current_dbg_addr + 1]) {
         current_dbg_addr++;  //Reached the next debug address: switching to it as the current address
         DBGMSGLVL(2, "Skip to debug address [%#"PRIx64"]\n", addrs[current_dbg_addr])
      }
      while (current_dbg_range < n_ranges && in_addr >= ranges_stops[current_dbg_range]) {
         current_dbg_range++; //Moved beyond the current range of addresses: switching to next range of addresses
         DBGMSGLVL(2, "Skip to range [%#"PRIx64" - %#"PRIx64"]\n", ranges_starts[current_dbg_range], ranges_stops[current_dbg_range])
      }
      if (current_dbg_range == n_ranges)
         break;   //Beyond the last range of addresses: exiting. Debug data of instructions should have been initialised to NULL, no need to set it
      if (in_addr < ranges_starts[current_dbg_range]) {
         assert(current_dbg_range == 0 || in_addr >= ranges_stops[current_dbg_range - 1]);
         /*Between two ranges, or before the first one. In theory, we should not have any debug address here
         However, we will still update the instruction debug data if there is one precisely at this address
         and we are not at the boundary of the previous range*/
         /**\todo (2018-04-13) There is still a case here where this can cause a function to lack its debug name.
          * In some files, there is a function beginning at the address of the end of a range
          * (don't ask me why this function is not in the debug data).
          * Sometimes taking this into account causes the first instruction of the function to be affected
          * a wrong debug source line (and later the wrong debug function) and some other times it should be
          * in order to associate the function with the correct demangled name.
          * In the meantime, I'm using the conservative approach: this will not associate functions to the wrong
          * demangled name, but some will not be able to be associated to one.
          * The commented code is an attempt to fix this by detecting that there was a debug line different
          * from the previous one, but it ended up creating more errors.*/
//         if (in_addr == addrs[current_dbg_addr]
//               && (current_dbg_range == 0
//                     || in_addr > ranges_stops[current_dbg_range - 1]
//                     || current_dbg_addr == 0
//                     || addrs[current_dbg_addr] != addrs[current_dbg_addr - 1]))
            if (in_addr == addrs[current_dbg_addr] && (current_dbg_range == 0
                        || in_addr > ranges_stops[current_dbg_range - 1]))
            insn_set_debug_info(in, filename[current_dbg_addr], srcs[current_dbg_addr]);
         continue;
      }
      //Now we should be in the middle of a range of addresses
      //Updating the instruction debug data with those of the previous encountered address, if they belong to the current range
      if (addrs[current_dbg_addr] >= ranges_starts[current_dbg_range] && addrs[current_dbg_addr] < ranges_stops[current_dbg_range])
         insn_set_debug_info(in, filename[current_dbg_addr], srcs[current_dbg_addr]);
   }
   lc_free(filename);
   lc_free(addrs);
   lc_free(srcs);
   lc_free(ranges_starts);
   lc_free(ranges_stops);
}

/*
 * Free debug data associated to a function
 * \param f a function
 */
static void dbg_fct_free(fct_t* f)
{
   if (f->debug == NULL)
      return;
   lc_free(f->debug);
   f->debug = NULL;
}

/*
 * Free debug data associated to an instruction
 * \param in an instruction
 */
static void dbg_insn_free(insn_t* in)
{
   if (in->debug == NULL)
      return;
   lc_free(in->debug);
   in->debug = NULL;
}

/*
 * Creates a new structure describing the debug information found in a file
 * \param debug Format specific structure containing the debug information from the file
 * \param format Descriptor of the format
 * \return A new structure containing the debug information from the file, or NULL if \c debug is NULL and \c format is not DBG_NONE
 * */
dbg_file_t* dbg_file_new(void* debug, dbgformat_t format)
{
   if (!debug && format != DBG_NONE)
      return NULL;
   dbg_file_t* out = lc_malloc0(sizeof(*out));
   out->data = debug;
   out->format = format;
   return out;
}
/*
 * Frees a structure describing the debug information found in a file
 * \param dbg The structure to free
 * */
void dbg_file_free(dbg_file_t* dbg)
{
   if (!dbg)
      return;
   lc_free(dbg);
}

static DwarfAPI* asmfile_debug_handle_link(const char* libpath, asmfile_t* asmf);
static void asmfile_load_dbg_with_dwarf(asmfile_t* asmf, DwarfAPI* dwarf)
{
   if ((asmf == NULL) || (asmfile_get_binfile(asmf) == NULL))
      return;
   DBGMSG("Loading debug data for file %s\n", asmfile_get_name(asmf));
   // Check if debug data are already present in the file
   // and create them if needed
   if (asmf->debug == NULL || asmf->debug->format == DBG_FORMAT_UNDEF) {
      if (asmf->debug == NULL)
         asmf->debug = lc_malloc0(sizeof(dbg_file_t));
      if (dwarf != NULL) {
         asmf->debug->format = DBG_FORMAT_DWARF;
         asmf->debug->data = dwarf;
         dwarf_api_set_asmfile(dwarf, asmf);
      } else {
         // else, no debug or format not already handled
         asmf->debug->format = DBG_FORMAT_UNDEF;
         asmf->debug->data = NULL;
      }
   }

   // Once debug data are loaded, load them for instructions
   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF:
      dbg_insn_load_dwarf(asmf, (DwarfAPI*) asmf->debug->data);
      break;

   case DBG_FORMAT_UNDEF:
      // SPECIFIC CASE? : check .gnu_debug_link
      asmfile_debug_handle_link(asmf->name, asmf);
      break;
   default:
      break;
   }
}

/*
 * Load debug data associated to a binary file and its instructions
 * \param asmf a binary file
 * \return EXIT_SUCCESS if successful, error code otherwise
 */
int asmfile_load_dbg(asmfile_t* asmf)
{
   if (asmfile_get_binfile(asmf) == NULL)
      return ERR_BINARY_MISSING_BINFILE;
   DBGMSG("Loading debug data for file %s\n", asmfile_get_name(asmf));

   if (asmf->debug == NULL) {
      //Parsing the debug information if it was not already
      dbg_file_t* dbgfct = binfile_parse_dbg(asmfile_get_binfile(asmf));
//      DwarfAPI* dwarf = ((elffile_t*)asmf->origin)->dwarf;
      asmfile_setdebug(asmf, dbgfct);
      //asmfile_load_dbg_with_dwarf (asmf, dbgfct);
   }
   /**\todo TODO (2015-05-06) Check if this new behaviour does not interfere with the handling of libraries
    * whose debug information are in a different file (needed by lprof). Now the parsing is done in a first
    * pass by invoking binfile_parse_dbg, then we load it into the instructions*/
   //Parsing the debug information and loading it into instructions
   if (asmf->debug && asmf->debug->format != DBG_NONE) {
      // Once debug data are loaded, load them for instructions
      switch (asmf->debug->format) {
      case DBG_FORMAT_DWARF:
         dbg_insn_load_dwarf(asmf, (DwarfAPI*) asmf->debug->data);
         break;
      case DBG_FORMAT_UNDEF:
         // SPECIFIC CASE? : check .gnu_debug_link
         asmfile_debug_handle_link(asmf->name, asmf);
         break;
      default:
         WRNMSG(
               "Unsupported or unknown debug format for file %s: unable to load debug information\n",
               asmfile_get_name(asmf))
         ;
         return ERR_BINARY_UNKNOWN_DEBUG_FORMAT;
         break;
      }
   } else {
      DBGMSG("File %s does not contain debug data\n", asmfile_get_name(asmf));
   }
   return EXIT_SUCCESS;
}
/*
 * Load debug data associated to a function
 * \param f a function
 */
void asmfile_load_fct_dbg(fct_t* f)
{
   if (f == NULL || f->asmfile == NULL || f->asmfile->debug == NULL)
      return;
   asmfile_t* asmf = f->asmfile;
   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF:
      dbg_fct_load_dwarf(f, (DwarfAPI*) asmf->debug->data);
      break;

   case DBG_FORMAT_UNDEF:
   default:
      break;
   }
}

/*
 * Free all debug data associated to a binary file
 * \param asmf a binary file
 */
void asmfile_unload_dbg(asmfile_t* asmf)
{
   if (asmf == NULL || asmf->debug == NULL || asmf->debug->format == DBG_NONE)
      return;

   // iterate over functions to remove debug structure
   if (queue_length(asmfile_get_fcts(asmf)) > 0) {
      FOREACH_INQUEUE(asmfile_get_fcts(asmf), it_f)
      {
         fct_t* f = GET_DATA_T(fct_t*, it_f);
         dbg_fct_free(f);
      }
   }
   // iterate over instructions to remove debug structure
   FOREACH_INQUEUE(asmfile_get_insns(asmf), it_i) {
      insn_t* in = GET_DATA_T(insn_t*, it_i);
      dbg_insn_free(in);
   }
   // according to the format, free the original debug data
   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF:
      //dwarf_api_end ((DwarfAPI*) asmf->debug->data);
      dwarf_api_close_light((DwarfAPI*) asmf->debug->data);
      break;

   case DBG_FORMAT_UNDEF:
   default:
      break;
   }
   dbg_file_free(asmf->debug);
   asmf->debug = NULL;
}

/*
 * Get the producer (a string in debug data to save compiler, version ...)
 * \param f a function
 * \return the producer string or NULL if not available
 */
char* fct_getproducer(fct_t* f)
{
   if (f == NULL || f->debug == NULL)
      return NULL;

   asmfile_t* asmf = fct_get_asmfile(f);
   if (asmf == NULL || asmf->debug == NULL)
      return NULL;

   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF:
      return (dwarf_file_get_producer(
            dwarf_function_get_file((DwarfFunction *) f->debug->data)));
      break;

   case DBG_FORMAT_UNDEF:
   default:
      break;
   }
   return (NULL);
}

/*
 * Add a ranges in a function debug data
 * \param f a function
 * \param start first instruction of the range
 * \param stop last instruction of the range
 */
void fct_add_range(fct_t* f, insn_t* start, insn_t* stop)
{
   if (f == NULL)
      return;
   else if (f->debug == NULL)
      return;

   asmfile_t* asmf = fct_get_asmfile(f);
   if (asmf == NULL || asmf->debug == NULL)
      return;

   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF:
      if (f->debug != NULL)
         dwarf_function_add_range(f->debug->data, start, stop);
      break;

   case DBG_FORMAT_UNDEF:
   default:
      break;
   }
}

/*
 * Return function ranges
 * \param f a function
 * \return a queue of ranges
 */
queue_t* fct_get_ranges(fct_t* f)
{
   if (f == NULL)
      return (NULL);
   else if (f->debug == NULL)
      return (f->ranges);

   asmfile_t* asmf = fct_get_asmfile(f);
   if (asmf == NULL || asmf->debug == NULL)
      return NULL;

   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF:
      return (dwarf_function_get_ranges((DwarfFunction *) (f->debug->data)));
      break;

   case DBG_FORMAT_UNDEF:
   default:
      break;
   }
   return (NULL);
}

/*
 * Get the directory where the file was located during compilation
 * \param f a function
 * \return the file directory or NULL if not available
 */
char* fct_getdir(fct_t* f)
{
   if (f == NULL || f->debug == NULL)
      return NULL;

   asmfile_t* asmf = fct_get_asmfile(f);
   if (asmf == NULL || asmf->debug == NULL)
      return NULL;

   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF:
      return (dwarf_file_get_dir(
            dwarf_function_get_file((DwarfFunction *) f->debug->data)));
      break;

   case DBG_FORMAT_UNDEF:
   default:
      break;
   }
   return (NULL);
}

/*
 * Checks if a debug function exists at a specific address
 * \param asmf an assembly file containing debug data
 * \param start_addr First address where to look for the function
 * \param end_addr Last address where to look for the function
 * \param ret_addr used to return address of debug data
 * \return the function name is there is a function, else NULL
 */
char* asmfile_has_dbg_function(asmfile_t* asmf, int64_t start_addr,
      int64_t end_addr, int64_t* ret_addr)
{
   if (asmf == NULL || asmf->debug == NULL || start_addr < 0)
      return NULL;

   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF: {
      DwarfAPI* dwarf = asmf->debug->data;
      DwarfFunction* dfct = NULL;
      if (end_addr < start_addr)
         dfct = dwarf_api_get_function_by_addr(dwarf, start_addr);
      else
         dfct = dwarf_api_get_function_by_interval(dwarf, start_addr, end_addr);
      if (dfct == NULL)
         return (NULL);
      else if (ret_addr != NULL)
         *ret_addr = dwarf_function_get_lowpc(dfct);
      return (dwarf_function_get_name(dfct));
      break;
   }
   case DBG_FORMAT_UNDEF:
   default:
      break;
   }
   return NULL;
}

static void dbg_get_compile_options_intel(asmfile_t* asmf)
{
   assert(asmf);
   if (asmfile_get_binfile(asmf) == NULL || asmf->debug->data == NULL)
      return;
//   elffile_t* efile = (elffile_t*)(asmf->origin);
   binfile_t* bf = asmfile_get_binfile(asmf);
   int _comment_scn = 0;
   uint64_t i = 0;
   unsigned char* bits = NULL;
   uint64_t bits_size = 0;
   queue_t* files = dwarf_api_get_files(asmf->debug->data);

   // Iterate over sections to locate sections which can contain command line options
   for (i = 0; i < binfile_get_nb_sections(bf); i++) {
      binscn_t* scn = binfile_get_scn(bf, i);
      if (strcmp(binscn_get_name(scn), ".comment") == 0)
         _comment_scn = i;
   }

   if (_comment_scn > 0) {
      binscn_t* scn = binfile_get_scn(bf, _comment_scn);
      bits = binscn_get_data(scn, &bits_size); //ELF_get_scn_bits (efile, _comment_scn);
      //bits_size = binscn_get_size(scn);//Elf_Shdr_get_sh_size (efile, _comment_scn, NULL);
      //efile->sections[_comment_scn]->scnhdr->sh_size;
      char* str = lc_malloc0((bits_size + 1) * sizeof(char));
      memcpy(str, bits, bits_size * sizeof(char));
      for (i = 0; i < bits_size; i++) {
         if (str[i] == '\0')
            str[i] = ' ';
      }

      // Analyze the string to look for "-?comment:" pattern. It is used by Intel compilers
#ifdef _WIN32
      if (strstr(str, "comment:") != NULL)
#else
      if (str_contain(str, "-?comment:") == TRUE)
#endif
            {
         char* tok = NULL;
         char* sstr = str;
         int ptoks = 1;
         char** toks = lc_malloc(sizeof(char*));
         toks[0] = sstr;
         while ((tok = strstr(sstr, "-?comment:")) != NULL) {
            toks = lc_realloc(toks, (ptoks + 1) * sizeof(char*));
            *tok = '\0';
            *(tok + 9) = '\0';
            toks[ptoks++] = tok + 10;
            sstr = tok + 10;
         }

         for (i = 0; i < ptoks; i++) {
            // Looking for the source file ": <file> :"
            tok = strstr(toks[i], ": ");
            if (tok == NULL)
               continue;
            int size = 1;
            while (tok[size + 2] != ' ')
               size++;
            char* srcfilename = lc_malloc((size + 1) * sizeof(char));
            memcpy(srcfilename, tok + 2, size * sizeof(char));
            srcfilename[size] = '\0';

            // Looking for the Dwarf file with the corresponding name
            FOREACH_INQUEUE(files, it_f) {
               DwarfFile* df = GET_DATA_T(DwarfFile*, it_f);
               if (strcmp(lc_basename(dwarf_file_get_name(df)),
                     lc_basename(srcfilename)) == 0)
                  dwarf_file_set_command_line_opts(df, toks[i]);
            }
            lc_free(srcfilename);
         }
      }
   }
   return;
}

static int dbg_get_compile_options_gnu(asmfile_t* asmf)
{
   assert(asmf);
   if (asmfile_get_binfile(asmf) == NULL || asmf->debug->data == NULL)
      return;
   // This function handled cases:
   //  - -frecord-gcc-switches option (GNU 4.7+)
   int i = 0;
   int j = 0;
//   elffile_t* efile = (elffile_t*)(asmfile_getorigin(asmf));
   binfile_t* bf = asmfile_get_binfile(asmf);
   int _GCC_command_line_scn = -1;
   char* bits = NULL;
   int bits_size = 0;

   // Looking for ".GCC.command.line" section
   for (i = 0; i < binfile_get_nb_sections(bf); i++) {
      binscn_t* scn = binfile_get_scn(bf, i);
      if (strcmp(binscn_get_name(scn), ".GCC.command.line") == 0)
         _GCC_command_line_scn = i;
   }
   if (_GCC_command_line_scn == -1)
      return -1;

   WRNMSG(
         "Using .GCC.command.line section content can be inaccurate if there are multiple files\n");
   DBGMSG(".GCC.command.line section found at index %d\n",
         _GCC_command_line_scn);

   // If ".GCC.command.line" section found, parse it
   binscn_t* scn = binfile_get_scn(bf, _GCC_command_line_scn);
   bits = binscn_get_data(scn, &bits_size); //(char*)ELF_get_scn_bits (efile, _GCC_command_line_scn);
   //bits_size = efile->sections[_GCC_command_line_scn]->scnhdr->sh_size;

   for (i = 0; i < bits_size;) {
      asmf->debug->nb_command_line = asmf->debug->nb_command_line + 1;
      i = i + strlen(&bits[i]) + 1;
   }
   asmf->debug->command_line = lc_malloc0(
         asmf->debug->nb_command_line * sizeof(char*));
   asmf->debug->command_line_linear = lc_malloc0(bits_size);
   for (i = 0; i < bits_size;) {
      asmf->debug->command_line[j] = lc_strdup(&bits[i]);
      sprintf(asmf->debug->command_line_linear, "%s %s",
            asmf->debug->command_line_linear, &bits[i]);
      j = j + 1;
      i = i + strlen(&bits[i]) + 1;
   }

   return 0;
}

/*
 * Look into the elf file to get command line options if there are available
 * and store results in debug data. Even if command line data are not based on
 * DWARF, they are needed to link options to the corresponding source file.
 * \param asmf an assembly file
 */
static void dbg_get_compile_options(asmfile_t* asmf)
{
   assert(asmf);
   if (asmfile_get_binfile(asmf) == NULL || asmf->debug->data == NULL)
      return;
   queue_t* files = dwarf_api_get_files(asmf->debug->data);

   // Check the file has been compiled using an Intel compiler
   DwarfFile* df = queue_peek_tail(files);
   if (dwarf_file_get_producer_code(df) == COMP_INTEL)
      dbg_get_compile_options_intel(asmf);
   else if (dwarf_file_get_producer_code(df) == COMP_GNU)
      dbg_get_compile_options_gnu(asmf);
}

/*
 * Analyzes the binary to get options used to compile the binary
 * and save them into debug data
 * \param asmf an assembly file
 */
char* asmfile_get_compile_options(asmfile_t* asmf)
{
   DBGMSG0("Getting command line options\n");

   if (asmf == NULL)
      return NULL;

   if (asmf->debug == NULL) {
      asmf->debug = lc_malloc0(sizeof(dbg_file_t));
      asmf->debug->format = DBG_FORMAT_UNDEF;
   }

   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF: {
      DBGMSG0("Getting command line options for DWARF\n");
      dbg_get_compile_options(asmf);

      if (asmf->debug != NULL)
      {
    	  if (asmf->debug->command_line_linear == NULL)
    	  {
             queue_t* files = dwarf_api_get_files(asmf->debug->data);
             DwarfFile* df = queue_peek_tail(files);
             char* str = dwarf_file_get_producer(df);
             if (str != NULL)
                return str;
          }
    	  return (asmf->debug->command_line_linear);
      }
      break;
   }
   case DBG_FORMAT_UNDEF:
   default:
      DBGMSG0("Getting command line options for UNDEF\n");
      dbg_get_compile_options_gnu(asmf);
      if (asmf->debug != NULL)
         return (asmf->debug->command_line_linear);
      break;
   }
   return NULL;
}

/*
 * Gets the options used to compile the source file containing the function
 * \param f a function
 * \return a string containing options on what is used to compile the function or NULL
 */
char* fct_get_compile_options(fct_t* f)
{
   if (f == NULL)
      return NULL;

   asmfile_t* asmf = fct_get_asmfile(f);
   if (asmf == NULL || asmf->debug == NULL)
      return NULL;

   switch (asmf->debug->format) {
   case DBG_FORMAT_DWARF: {
      if (f->debug == NULL)
         return NULL;
      DwarfFunction * df = (DwarfFunction *) f->debug->data;
      if (df == NULL)
         return (NULL);
      DwarfFile* dfile = dwarf_function_get_file(df);
      char* ret = dwarf_file_get_command_line_opts(dfile);
      if (ret != NULL)
         return (ret);
      return (asmf->debug->command_line_linear);
      break;
   }
   case DBG_FORMAT_UNDEF:
   default:
      return (asmf->debug->command_line_linear);
      break;
   }
   return (NULL);
}

/**
 * Get Dwarf info with debian os system libraries (read .gnu_debug_link section in elf)
 * \param libpath The path of the library (.so) to analyze
 * \param asmf The assembly file
 * \return the dwarf structure filled out OR NULL if not available.
 */

static DwarfAPI* asmfile_debug_handle_link(const char* libpath, asmfile_t* asmf)
{
   /**\todo TODO UPDATE THIS WITH THE NEW FORMAT*/
   if (asmf == NULL || libpath == NULL)
      return NULL;

   if (asmf->debug != NULL && asmf->debug->data != NULL)
      return asmf->debug->data;

   //DBGMSG ("===> asmfile debug = %p\n", asmf->debug->data);
   const char* scnname = ".gnu_debuglink";
   const char* os_debug_path = "/usr/lib/debug/";

   // Looks into asmf if a section called .gnu_debuglink is present
   int i = 0;
//   elffile_t* elfexe = (elffile_t*)asmfile_getorigin(asmf);
   binfile_t* bf = asmfile_get_binfile(asmf);
   char* debugfile_name = NULL;

   for (i = 1; i < binfile_get_nb_sections(bf); i++) {
      binscn_t* scn = binfile_get_scn(bf, i);
      if (strcmp(binscn_get_name(scn), scnname) == 0) {
         DBGMSG("===> Section found at index %d\n", i);
         uint64_t scnlen = 0;
         int64_t scnaddr = binscn_get_addr(scn);
         unsigned char* bits = binscn_get_data(scn, &scnlen); //elffile_get_scnbits (elfexe, i, &scnlen, &scnaddr);
         if (bits != NULL)
            debugfile_name = lc_strdup((char*) bits);
         DBGMSG("===> Content : [%s]\n", debugfile_name);
         break;
      }
   }

   if (debugfile_name == NULL) {
      return NULL;
   }

   // At this point, debugfile_name contains the basename of the library to load and libpath
   // contains the full name of the library
   char *libpath_copy = lc_dirname(libpath);
   DBGMSG("===> libpath : [%s]\n", libpath_copy);
   DBGMSG("===> dirname(libpath) : [%s]\n", libpath_copy);
   DBGMSG("===> os_debug_path : [%s]\n", os_debug_path);
   DBGMSG("===> debugfile_name : [%s]\n", debugfile_name);

   char* linkname = lc_malloc(
         sizeof(char)
               * (strlen(os_debug_path) + strlen(libpath_copy)
                     + strlen(debugfile_name) + 5));
   sprintf(linkname, "%s%s/%s", os_debug_path, libpath_copy, debugfile_name);
   lc_free(libpath_copy); lc_free(debugfile_name);
   DBGMSG("===> Debug version of the library : %s\n", linkname);
   FILE* f = fopen(linkname, "r");
   if (f == NULL) {
      lc_free(linkname);
      return NULL;
   }
   int fd = fileno(f);
   Elf * elf = elf_begin(fd, ELF_C_READ, NULL);
   DwarfAPI* dwarfApi = dwarf_api_init_light(elf, linkname, asmf);
   if (dwarfApi == NULL)
      return dwarfApi;
   DBGMSG0("===> DWARF loaded\n");

   // Load debug in the file
   asmfile_load_dbg_with_dwarf(asmf, dwarfApi);

   FOREACH_INQUEUE(asmf->functions, it_f) {
      fct_t* func = GET_DATA_T(fct_t*, it_f);
      dbg_fct_load_dwarf(func, dwarfApi);

      FOREACH_INQUEUE(func->ranges, it_r)
      {
         fct_range_t* range = GET_DATA_T(fct_range_t*, it_r);
         DBGMSG("=======> range [0x%"PRIx64" -> 0x%"PRIx64"]\n",
               range->start->address, range->stop->address);
         fct_add_range(func, range->start, range->stop);
      }
   }
   DBGMSG0("===> Debug data loaded\n");
   lc_free(linkname);
   return dwarfApi;
}

/*
 * Update labels of the given asmfile in using debug info if they are available.
 * \param asmfile The asmfile to update
 * \return EXIT_SUCCESS if the labels could be added successfully, error code otherwise
 */
int asmfile_add_debug_labels(asmfile_t* asmfile)
{
   if (asmfile == NULL)
      return ERR_LIBASM_MISSING_ASMFILE;
   if (!asmfile->debug) {
      DBGMSG(
            "Unable to add labels from debug information to representation of file %s: file does not contain debug data\n",
            asmfile_get_name(asmfile));
      return EXIT_SUCCESS;
   }
   switch (asmfile->debug->format) {
   case DBG_FORMAT_DWARF:
      //SPECIAL CASE : HANDLE DEBIAN OS SYSTEM LIBRARIES WITH .gnu_debug_link
      if (asmfile->debug->data == NULL && asmfile->name != NULL)
         asmfile->debug->data = asmfile_debug_handle_link(asmfile->name,
               asmfile);
      DwarfAPI* dwarfInfo = NULL;
      queue_t* dwarfFunctionsInfo = NULL;
      if (asmfile->debug->data != NULL) {
         dwarfInfo = asmfile->debug->data;
         dwarfFunctionsInfo = dwarf_api_get_functions(dwarfInfo);
         //Sorts the existing labels in the file to allow a search by address
         asmfile_sort_labels(asmfile);
         //FOR EACH DWARF FUNCTION
         FOREACH_INQUEUE(dwarfFunctionsInfo, fDwarfIter) {
            DwarfFunction* dwarfFunc = GET_DATA_T(DwarfFunction*, fDwarfIter);
            int64_t low_pc_addr = dwarf_function_get_lowpc(dwarfFunc);
            //int64_t high_pc_addr = dwarf_function_get_lowpc(dwarfFunc);
            char* fnew_name = NULL;
            //ADDRESS IS CORRECT?
            if (low_pc_addr != -1) {
               label_t* label = asmfile_get_last_label(asmfile, low_pc_addr,
                     NULL);
               //LABEL IS ALREADY PRESENT IN THE LIST (FROM .symtab)
               if (label != NULL && label->address == low_pc_addr) {
                  DBGMSGLVL(2, "%s LABEL MATCH WITH DWARF INFO\n", label->name);
               }
               //NEW LABEL ( NOT PRESENT IN .symtab)
               else {
                  DBGMSGLVL(2, "NEW LABEL : %s (#%"PRId64")\n",
                        dwarf_function_get_name(dwarfFunc), low_pc_addr);
                  char* label_name = dwarf_function_get_name(dwarfFunc);
                  char** fct_name = NULL;
                  int nb_subpatterns, subpattern_idx;
                  if (label_name != NULL) {
                     //CHECK LABEL NAME : IF IT MATCHES WITH OMP LABELS WE CHANGE THE FORMAT NAME
                     //OMP PARALLEL REGION LABEL?
                     if ((nb_subpatterns =
                           str_match(label_name,
                                 "L_([a-zA-Z0-9_]+)__[0-9]+__par_region([0-9]+)_[0-9]+_[0-9]+",
                                 &fct_name)) > 0) {
                        fnew_name = lc_malloc(
                              (strlen(fct_name[1]) + strlen("#omp_region_") + 4)
                                    * sizeof(char));
                        sprintf(fnew_name, "%s%s%s", fct_name[1],
                              "#omp_region_", fct_name[2]);
                        //FREE
                        for (subpattern_idx = 0;
                              subpattern_idx < nb_subpatterns;
                              subpattern_idx++) {
                           free(fct_name[subpattern_idx]);
                        }
                        free(fct_name);
                        fct_name = NULL;
                     }
                     //OMP PARALLEL LOOP LABEL?
                     else if ((nb_subpatterns =
                           str_match(label_name,
                                 "L_([a-zA-Z0-9_]+)__[0-9]+__par_loop([0-9]+)_[0-9]+_[0-9]+",
                                 &fct_name)) > 0) {
                        fnew_name = lc_malloc(
                              (strlen(fct_name[1]) + strlen("#omp_loop_") + 4)
                                    * sizeof(char));
                        sprintf(fnew_name, "%s%s%s", fct_name[1], "#omp_loop_",
                              fct_name[2]);
                        //FREE
                        for (subpattern_idx = 0;
                              subpattern_idx < nb_subpatterns;
                              subpattern_idx++) {
                           free(fct_name[subpattern_idx]);
                        }
                        free(fct_name);
                        fct_name = NULL;
                     } else {
                        fnew_name = label_name;
                     }
                     //FILL OUT LABEL INFO AND ADD IT INTO THE LABEL LIST (FCTLABEL)
                     label_t* new_label = label_new(fnew_name, low_pc_addr,
                           TARGET_INSN, NULL);
                     label_set_type(new_label, LBL_FUNCTION);
                     label_set_scn(new_label,
                           binfile_lookup_scn_span_addr(
                                 asmfile_get_binfile(asmfile), low_pc_addr));
                     asmfile_add_label_unsorted(asmfile, new_label);
                  }
               }
            }
         }
         queue_free(dwarfFunctionsInfo, NULL);
      }
      return EXIT_SUCCESS;
      break;
   case DBG_NONE:
      DBGMSG(
            "File %s has no debug information: unable to add function labels from debug information\n",
            asmfile_get_name(asmfile))
      ;
      return EXIT_SUCCESS;
      break;
   case DBG_FORMAT_UNDEF:
   default:
      WRNMSG(
            "Unsupported or unknown debug format for file %s: unable to add function labels from debug information\n",
            asmfile_get_name(asmfile))
      ;
      return ERR_BINARY_UNKNOWN_DEBUG_FORMAT;
      break;
   }
   return EXIT_FAILURE; //We should never arrive here actually, but it makes the compiler happy
}

