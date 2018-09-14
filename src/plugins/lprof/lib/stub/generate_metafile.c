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

/*
 * Defines functions to generate "metadata" for application executable and libraries,
 * mostly functions/loops structure with address ranges, function name and loop ID.
 * Finding address ranges needs to parse the /proc/pid/maps file to locate the file
 * (executable or a library) related to a given virtual address.
 * Main functions are: write_exe_metafile, write_exe_offset and write_libs_metafile.
 */

#include <time.h>
#include <stdio.h> // should have been included by binary_format.h !
#include <stdint.h> // uint64_t...
#include <libgen.h> // basename
#include <unistd.h> // access
#include <sys/stat.h> // mkdir
#include <sys/types.h> // mkdir

#include "utils.h" // fopen_in_directory...
#include "generate_metafile_shared.h" // loop_get_ranges...
#include "binary_format.h" // lprof_library_t...
#include "libmadras.h" // madras_is_file_valid, includes libmtroll declaring elffile_isdyn
#include "libmmaqao.h" // project_parse_file
#include "libmcommon.h" // hashtable_t...

static void add_maps_file (const char *process_path, const char *file_name, void *files_ptr)
{
   // Ignoring invalid name entries (expecting "maps_bin*")
   if (strstr (file_name, "maps_bin") == NULL) return;

   // Insert file name
   char *const file_path = lc_malloc (strlen (process_path) + strlen (file_name) + 2);
   sprintf (file_path, "%s/%s", process_path, file_name);
   array_add ((array_t *) files_ptr, file_path);
}

static array_t *get_process_maps (const char *process_path)
{
   // Get maps files inside the current process directory (host_path/process_name)
   array_t *files = array_new();
   for_each_file_in_directory (process_path, add_maps_file, files);

   if (array_length (files) == 0)
      WRNMSG ("found no maps file for %s\n", process_path);

   return files;
}

// For a library, update libs from a range line
static void process_lib_maps_line (const char *name, const char *addr, hashtable_t *lib_ranges)
{
   // Get or create related library
   lib_range_t *lib_range = hashtable_lookup (lib_ranges, name);
   if (lib_range == NULL) {
      char *lib_name = lc_strdup (name);
      lib_range = lc_malloc0 (sizeof *lib_range);
      lib_range->name = lib_name;
      hashtable_insert (lib_ranges, lib_name, lib_range);
   }

   // Extract start-stop addresses from the current map range
   char *addr_dup = lc_strdup (addr);
   uint64_t start = strtol (strtok (addr_dup, "-"), NULL, 16);
   uint64_t stop  = strtol (strtok (NULL    , "-"), NULL, 16);
   lc_free (addr_dup);

   // Update ranges for the current library
   if (lib_range->startMapAddress == 0 ||
       lib_range->startMapAddress > start)
      lib_range->startMapAddress = start;
   if (lib_range->stopMapAddress < stop)
      lib_range->stopMapAddress = stop;
}

/* Read maps files to extract data useful to write metafiles:
 *  - for libraries (libs hashtable): name and address ranges (start-stop)
 *  - for executable: start address (exe_offset) */
static void read_maps (const char *process_path, const char *exe_name,
                       hashtable_t *libs, uint64_t *exe_offset)
{
   // Get maps files for current process
   array_t *const file_names = get_process_maps (process_path);

   // To check once for all whether a library can be disassembled (quite expensive)
   hashtable_t *const is_valid = hashtable_new (str_hash, str_equal);

   // For each maps file
   FOREACH_INARRAY (file_names, file_names_iter) {
      // Open the maps file
      const char *const file_name = ARRAY_GET_DATA (NULL, file_names_iter);
      FILE *fp = fopen (file_name, "r");
      if (!fp) {
         WRNMSG ("Missing maps file: %s\n", file_name);
         perror ("fopen");
         continue;
      }

      // For each line in the maps file
      char buf[1024]; char *line;
      while ((line = fgets (buf, sizeof buf, fp)) != NULL) {
         // Delete final newline character
         line [strlen (line) - 1] = '\0';

         // Parse maps line: addr perms offset dev inode name
         const char *const delim = " ";
         const char *const addr  = strtok (line, delim);
         const char *const perms = strtok (NULL, delim);
         if (strchr (perms, 'x') == NULL) continue; // Ignore non executable maps

         strtok (NULL, delim); strtok (NULL, delim); strtok (NULL, delim); // skip offset, dev, inode
         const char *const name  = strtok (NULL, delim);

         // Check library name
         if (!name || strlen (name) == 0) continue;
         if (strcmp (name, "cp") == 0) continue; // Required for legacy sampling engine

         // Check whether maps corresponds to the application executable or a library
         // Remark: basename could modify its argument, requires copy
         char *exe_name_dup = lc_strdup (exe_name); char *map_name_dup = lc_strdup (name);
         if (strcmp (basename (exe_name_dup), basename (map_name_dup)) == 0) { // Binary maps
            char *addr_dup = lc_strdup (addr);
            *exe_offset = strtol (strtok (addr_dup, "-"), NULL, 16); // Get first (start) address
            lc_free (addr_dup);
         } else { // Library maps
            // Check (once for a given name) whether the library can be disassembled
            char *is_valid_val = hashtable_lookup (is_valid, name);
            if (is_valid_val == NULL) {
               is_valid_val = madras_is_file_valid ((char *) name, NULL, NULL) ? "true":"false";
               hashtable_insert (is_valid, lc_strdup (name), is_valid_val);
            }
            if (strcmp (is_valid_val, "true") == 0)
               process_lib_maps_line (name, addr, libs);
         }
         lc_free (exe_name_dup); lc_free (map_name_dup);
      }

      fclose (fp);
   } // for each maps file name

   hashtable_free (is_valid, NULL, lc_free); // keys allocated with lc_strdup
   array_free (file_names, NULL);
}

static boolean_t write_maps (const char *process_path, const hashtable_t *lib_ranges)
{
   boolean_t err = FALSE;
   FILE *fp = fopen_in_directory (process_path, "lib_ranges.lprof", "w");

   FOREACH_INHASHTABLE (lib_ranges, lib_ranges_iter) {
      const char *name = GET_KEY (NULL, lib_ranges_iter);
      size_t len = strlen (name);
      if (fwrite (&len, sizeof len, 1, fp) != 1) {
         err = TRUE; break;
      }
      if (fwrite (name, len, 1, fp) != 1) {
         err = TRUE; break;
      }

      const lib_range_t *lib_range = GET_DATA_T (lib_range_t *, lib_ranges_iter);
      if (fwrite (&(lib_range->startMapAddress), sizeof lib_range->startMapAddress, 1, fp) != 1) {
         err = TRUE; break;
      }
      if (fwrite (&(lib_range->stopMapAddress), sizeof lib_range->stopMapAddress, 1, fp) != 1) {
         err = TRUE; break;
      }
   }

   if (err == TRUE)
      perror ("fwrite");
   
   fclose (fp);
   return err;
}

// Set a "library" (lprof_library_t) by "parsing" it (fast disassembling, providing only function labels)
static asmfile_t *parse_lib (project_t *project, lprof_library_t *lib)
{
   asmfile_t *asmf = project_parse_file (project, lib->name, NULL);
   if (!asmf) return NULL;

   // Set functions info
   //  - Set nbFunctions as the number of function labels
   lib->nbFunctions = asmf->n_fctlabels;
   //  - Allocate fctsInfo
   lib->fctsInfo = lc_calloc (lib->nbFunctions, sizeof lib->fctsInfo[0]);

   // For each function
   unsigned i;
   for (i=0; i<lib->nbFunctions; i++) {
      lprof_fct_t *const lf = &(lib->fctsInfo[i]);

      // Name (priority to demangled if any)
      char *fct_demname = fct_demangle (asmf->fctlabels[i]->name, COMP_ERR, LANG_ERR);
      if (fct_demname != NULL)
         lf->name = fct_demname;
      else
         lf->name = lc_strdup (asmf->fctlabels[i]->name);

      // Ranges
      lf->nbParts = 1;
      lf->startAddress = lc_malloc (sizeof lf->startAddress[0]);
      lf->stopAddress  = lc_malloc (sizeof lf->stopAddress [0]);
      lf->startAddress[0] = asmf->fctlabels[i]->address;
      if (i == lib->nbFunctions-1)
         lf->stopAddress[0] = asmf->fctlabels[i]->address; // TODO: last label
      else
         lf->stopAddress[0] = asmf->fctlabels[i+1]->address;

      // Debug symbols (source file/line) => no data when only parsing
      lf->srcFile = NULL;
      lf->srcLine = 0;

      // Outermost loops info => no data when only parsing
      lf->nbOutermostLoops = 0;
      lf->outermostLoopsList = NULL;
   }

   // Set loops info
   lib->nbLoops = 0;
   lib->loopsInfo = NULL;

   return asmf;
}

// Set lprof_loop and lprof_fct data from loops found in fct
static void get_fct_loop_info (lprof_loop_t **lprof_loop, lprof_fct_t *lprof_fct, const fct_t *fct)
{
   lprof_loop_t *ll = *lprof_loop;
   array_t *outermost_loops = array_new();

   // For each loop in fct
   FOREACH_INQUEUE (fct_get_loops ((fct_t *) fct), loop_iter) {
      loop_t *l = GET_DATA_T(loop_t*, loop_iter);

      // Loop ID
      ll->id = loop_get_id (l);

      // Source file/line
      ll->srcFile = lc_strdup (loop_get_src_file_path (l));
      ll->srcFunctionLine = lprof_fct->srcLine;
      loop_get_src_lines (l, &(ll->srcStartLine), &(ll->srcStopLine));

      // Function name
      ll->srcFunctionName = lprof_fct->name;

      // Ranges
      loop_get_ranges (l, ll);

      // Hierarchy
      ll->level = get_loop_level (l);
      ll->nbChildren = loop_get_children (l, &(ll->childrenList));
      if (ll->level == SINGLE_LOOP || ll->level == OUTERMOST_LOOP)
         array_add (outermost_loops, (void *)(uint64_t) ll->id);

      ll++;
   }

   // Fill outermost loops
   lprof_fct->nbOutermostLoops = array_length (outermost_loops);
   lprof_fct->outermostLoopsList = lc_malloc (lprof_fct->nbOutermostLoops *
                                              sizeof lprof_fct->outermostLoopsList[0]);
   unsigned i = 0;
   FOREACH_INARRAY (outermost_loops, ol_iter) {
      uint32_t id = (uint64_t) ARRAY_GET_DATA (NULL, ol_iter);
      lprof_fct->outermostLoopsList [i++] = id;
   }
   array_free (outermost_loops, NULL);

   *lprof_loop = ll; // Update lprof_loop (position in lprof_loop objects array)
}

// Get a prettier name for OpenMP regions/loops
static char *get_simple_omp_name (const char *base, const char *name)
{
   // Set pattern according to type
   char *pattern;
   if (strcmp (base, "region") == 0)
      pattern = "L_([a-zA-Z0-9_]+)__[0-9]+__par_region([0-9]+)_[0-9]+_[0-9]+";
   else // base = "loop" (no check)
      pattern = "L_([a-zA-Z0-9_]+)__[0-9]+__par_loop([0-9]+)_[0-9]+_[0-9]+";

   // Get tokens (OMP region/loop ID) matching the pattern
   char **matches; unsigned nb_matches = str_match (name, pattern, &matches);
   if (nb_matches < 2) return NULL;

   // Forge the final OMP name from base string and matching tokens
   // "<matches[1]>#omp_<base>_<matches[2]>"
   size_t omp_name_len = strlen (matches[1]) + strlen ("#omp_") +
      strlen (base) + strlen ("_") + strlen (matches[2]) + 1;
   char *omp_name = lc_malloc (omp_name_len);
   sprintf (omp_name, "%s#omp_%s_%s", matches[1], base, matches[2]);

   // Free data allocated by str_match
   unsigned i; for (i=0; i<nb_matches; i++) lc_free (matches[i]);
   lc_free (matches);

   return omp_name;
}

// Disasemble a binary (executable or library) file
static asmfile_t *disass_bin (project_t *project, const char *name,
                              unsigned *nb_fcts , lprof_fct_t  **fcts,
                              unsigned *nb_loops, lprof_loop_t **loops)
{
   // Disassemble binary file (from its name)
   asmfile_t *asmf = project_load_file (project, (char *) name, NULL);
   if (!asmf) return NULL;

   // Set nb_fcts/loops and allocate fcts/loops accordingly
   *nb_fcts = asmfile_get_nb_fcts (asmf);
   *fcts  = lc_malloc (*nb_fcts * sizeof (*fcts)[0]);
   *nb_loops = asmfile_get_nb_loops (asmf);
   *loops = lc_malloc (*nb_loops * sizeof (*loops)[0]);

   lprof_fct_t  *lf = *fcts;
   lprof_loop_t *ll = *loops;

   // For each function in the file
   FOREACH_INQUEUE (asmfile_get_fcts (asmf), fct_iter) {
      fct_t *const fct = GET_DATA_T(fct_t*, fct_iter);

      // Name (priority to demangled if any)
      if (fct_get_demname (fct) != NULL)
         lf->name = fct_get_demname (fct);
      else
         lf->name = fct_get_name (fct);
      // Get prettier name in case of Open MP region/loop
      char *omp_name = get_simple_omp_name ("region", lf->name);
      if (!omp_name) omp_name = get_simple_omp_name ("loop", lf->name);
      lf->name = omp_name ? omp_name : lc_strdup (lf->name);

      // Ranges
      lf->nbParts = fct->ranges->length;
      lf->startAddress = lc_malloc (lf->nbParts * sizeof lf->startAddress[0]);
      lf->stopAddress  = lc_malloc (lf->nbParts * sizeof lf->stopAddress [0]);
      unsigned range_id = 0;
      FOREACH_INQUEUE (fct->ranges, range_iter) {
         fct_range_t *range = GET_DATA_T(fct_range_t*, range_iter);
         lf->startAddress [range_id] = range->start->address;
         lf->stopAddress  [range_id] = range->stop->address;
         range_id++;
      }

      // Source file/line
      lf->srcFile = NULL;
      lf->srcLine = 0;
      insn_t *first_insn = fct_get_first_insn (fct);
      if (first_insn->debug != NULL) {
         lf->srcFile = lc_strdup (insn_get_src_file (first_insn));
         lf->srcLine = insn_get_src_line (first_insn);
      }

      get_fct_loop_info (&ll, lf, fct);

      lf++;
   }

   return asmf;
}

// Free function-related data allocated by disass_bin
static void free_lprof_fcts (lprof_fct_t *lprof_fcts, unsigned nb_functions)
{
   lprof_fct_t *lf;
   for (lf = lprof_fcts; lf < lprof_fcts + nb_functions; lf++) {
      lc_free (lf->name);
      lc_free (lf->startAddress);
      lc_free (lf->stopAddress);
      lc_free (lf->srcFile);
      lc_free (lf->outermostLoopsList);
   }
   lc_free (lprof_fcts);
}

// Free loop-related data allocated by disass_bin
static void free_lprof_loops (lprof_loop_t *lprof_loops, unsigned nb_loops)
{
   lprof_loop_t *ll;
   for (ll = lprof_loops; ll < lprof_loops + nb_loops; ll++) {
      lc_free (ll->startAddress);
      lc_free (ll->stopAddress);
      lc_free (ll->blockIds);
      lc_free (ll->srcFile);
      lc_free (ll->childrenList);
   }
   lc_free (lprof_loops);
}

static void disass_libs (const char *exe_name, const hashtable_t *lib_ranges,
                         const char *host_path, const char *disass_list, project_t *proj)
{
   // Check whether libraries must be parsed of disassembled
   unsigned disass_mode; hashtable_t *disass_table = NULL;
   if      (strcmp (disass_list, "off") == 0) disass_mode = 0; // parse only
   else if (strcmp (disass_list, "on" ) == 0) disass_mode = 2; // disassemble
   else { // disassemble libraries in disass_list, parse others
      disass_mode = 1;
      disass_table = hashtable_new (str_hash, str_equal);
      unsigned i, len;
      char **str = split_string (disass_list, ',', &len); // array of lc_strdup strings
      for (i=0; i<len; i++) hashtable_insert (disass_table, str[i], str[i]);
      lc_free (str);
   }

   hashtable_t *phy2sym = (disass_mode == 1) ? get_phy2sym (exe_name) : NULL;

   FOREACH_INHASHTABLE (lib_ranges, lib_ranges_iter) {
      char *const phy_name = GET_KEY (char *, lib_ranges_iter);
      lib_range_t *lib_range = GET_DATA_T (lib_range_t *, lib_ranges_iter);

      if (disass_mode == 1) {
         char *sym_name = hashtable_lookup (phy2sym, phy_name);
         if (sym_name != NULL) lib_range->name = lc_strdup (sym_name);
      }

      char libs_path [strlen (host_path) + strlen ("/libs") + 1];
      sprintf (libs_path, "%s/libs", host_path);
      if (mkdir (libs_path, S_IRWXU) != 0) {}

      char *lib_name = lc_strdup (lib_range->name);
      char *lib_basename = basename (lib_name);
      char name [strlen (libs_path) + strlen (lib_basename) + strlen (".lprof") + 2];
      sprintf (name, "%s/%s.lprof", libs_path, lib_basename);
      if (access (name, F_OK) == 0) continue;

      // Create output file
      FILE *fp = fopen (name, "w");
      if (!fp) continue;
      long lprofHeaderPosition = write_lprof_header (fp);
      write_libraries_info_header (fp, 1, 0x0);

      asmfile_t *asmf;
      lprof_library_t lib = { .name = lib_range->name,
                              .nbProcesses = 0,
                              .startMapAddress = NULL,
                              .stopMapAddress = NULL };
      if (disass_mode == 0 || (disass_mode == 1 && !hashtable_lookup (disass_table, lib_basename)))
         asmf = parse_lib (proj, &lib);
      else {
         char *cp_host_path = lc_strdup (host_path);
         printf ("[MAQAO] ANALYZING LIBRARY %s (host %s)\n", lib.name, basename (cp_host_path));
         fflush (stdout);

         asmf = disass_bin (proj, lib.name,
                            &(lib.nbFunctions),
                            &(lib.fctsInfo),
                            &(lib.nbLoops),
                            &(lib.loopsInfo));

         printf ("[MAQAO] LIBRARY %s DONE (host %s)\n", lib.name, basename (cp_host_path));
         fflush (stdout);
         lc_free (cp_host_path);
      }

      write_library (fp, &lib);
      lc_free (lib_name);

      // Free data allocated by parse/disass_lib
      free_lprof_fcts  (lib.fctsInfo , lib.nbFunctions);
      free_lprof_loops (lib.loopsInfo, lib.nbLoops    );
      project_remove_file (proj, asmf);

      // Finalyze and close
      long strSerializedOffset = write_serialized_str_array (fp);
      update_lprof_header (fp, strSerializedOffset, lprofHeaderPosition);
      fclose (fp);
   } // for each lib file

   hashtable_free (phy2sym, lc_free, lc_free);
   hashtable_free (disass_table, NULL, lc_free); // key = data => lc_free only on keys
}

// General case is one executable per process (for the moment, executable common to all processes)
static boolean_t is_exe_dyn_lib (const char *exe_name)
{
   // On some environments, executables are generated as dynamic libraries
   // Only in this case, executable address offset (read in maps) has to be subtracted to samples addresses
   project_t *prj = project_new ("check for dyn lib executable");
   asmfile_t *asmf = project_parse_file (prj, (char *) exe_name, NULL);
   int   ret = (binfile_get_type (asmfile_get_binfile (asmf)) == BFT_LIBRARY);
   project_remove_file (prj, asmf);
   project_free (prj);

   return ret ? TRUE : FALSE;
}

// Write to <path>/binary_offset.lprof the lowest address found in maps for the application executable
static void write_exe_offset (const char *path, uint64_t exe_offset)
{
   // Open binary_offset.lprof
   FILE *fp = fopen_in_directory (path, "binary_offset.lprof", "w");
   if (!fp) return;

   // Write executable offset to binary_offset.lprof and close it
   fprintf (fp, "%"PRIu64"", exe_offset);
   fclose (fp);
}

// Write to <host_path>/binary.lprof information related to functions and loops of the application executable
static void write_exe_metafile (const char *exp_path, const char *exe_name, project_t *proj)
{
   // Check if already done (by another instance)
   char *binary_lprof_name = lc_malloc (strlen (exp_path) + strlen ("/binary.lprof") + 1);
   sprintf (binary_lprof_name, "%s/binary.lprof", exp_path);
   int ret = access (binary_lprof_name, F_OK);
   lc_free (binary_lprof_name);
   if (ret == 0) return;

   // Open binary.lprof
   FILE *fp = fopen_in_directory (exp_path, "binary.lprof", "w");
   if (!fp) return;

   // Disassemble executable
   lprof_binary_info_t lb;
   uint32_t nb_functions, nb_loops;

   printf ("[MAQAO] ANALYZING EXECUTABLE %s\n", exe_name);
   fflush (stdout);

   asmfile_t *asmf = disass_bin (proj, exe_name,
                                 &nb_functions, &(lb.functions),
                                 &nb_loops, &(lb.loops));

   printf ("[MAQAO] EXECUTABLE %s DONE\n", exe_name);
   fflush (stdout);

   // Write lprof header and executable data
   long lprofHeaderPosition = write_lprof_header (fp);
   write_binary_info_header (fp, (char *) exe_name, nb_functions, nb_loops);
   write_binary_info (fp, &lb, nb_functions, nb_loops);

   // Finalize and close binary.lprof
   long strSerializedOffset = write_serialized_str_array (fp);
   update_lprof_header (fp, strSerializedOffset, lprofHeaderPosition);
   fclose (fp);

   // Free data allocated by disass_bin
   free_lprof_fcts  (lb.functions, nb_functions);
   free_lprof_loops (lb.loops    , nb_loops    );
   project_remove_file (proj, asmf);
}

void generate_metafile_binformat_new (const char *exp_path, const char *hostname,
                                      pid_t pid, const char *exe_name,
                                      const char *disass_list, project_t *proj)
{
   // From maps, allocate/fill data structures used to write metafiles (libs and exe_offset)
   uint64_t exe_offset = 0;
   hashtable_t *lib_ranges = hashtable_new (str_hash, str_equal);

   char host_path [strlen (exp_path) + strlen (hostname) + 1];
   sprintf (host_path, "%s/%s", exp_path, hostname);
   char *const process_path = lc_malloc (strlen (host_path) + strlen ("/999999") + 1);
   sprintf (process_path, "%s/%d", host_path, pid);

   read_maps (process_path, exe_name, lib_ranges, &exe_offset);
   write_maps (process_path, lib_ranges);
   if (!is_exe_dyn_lib (exe_name)) exe_offset = 0;
   write_exe_offset (process_path, exe_offset);
   lc_free (process_path);

   // CRITICAL SECTION: ONE AT A TIME FOR CURRENT NODE
   char *host_lock_name = lc_malloc (strlen (host_path) + strlen ("/lockdir") + 1);
   sprintf (host_lock_name, "%s/lockdir", host_path);
   while (mkdir (host_lock_name, S_IRWXU) != 0) sleep (1); // Wait for lock (node level)

   disass_libs (exe_name, lib_ranges, host_path, disass_list, proj);

   rmdir (host_lock_name); // Release lock (node level)
   lc_free (host_lock_name);
   // END OF CRITICAL SECTION

   write_exe_metafile (exp_path, exe_name, proj); // Remark: only (first) one instance will do this
}
