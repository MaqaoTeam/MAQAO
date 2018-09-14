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

/**
 \page MADRAS MADRAS executable
 The MADRAS executable is a program used to test and use some functionalities of libmadras.
 Today, it allows the user to disassemble a file (such as objdump, from GNU Binutils), get data
 on the ELF structure (such as readelf, also from GNU Binutils). It also allows the user to patch 
 a binary (for the moment, only a simple function insertion can be done, but more features will 
 be add in next releases).
 
 
 To disassemble a binary and print additionals data, the binary is disassembled using a MADRAS API function
 \ref madras_disass_file, then printed using another MADRAS function (\ref madras_insns_print). Additionnal 
 data are got using functions from libmasm and called in functions passed to (\ref madras_insns_print). These
 additionnal data can be colors, debug data or internal data.
 
 
 To print data on the ELF structure, data structures from the libelf are directly used. All data printed by
 this feature can be got using readelf with a different format. However, using some specific options, some
 data can be got in a more humain readable format, such as external dynamic libraries (--get-dynamic-lib) or
 external functions (--get-external-fct).
 
 
 To patch a binary, functions from MADRAS API (libmadras) are used. First, the binary is disassembled using
 \ref madras_disass_file. Then, the patcher is initialized using \ref madras_modifs_init and functions are added
 using \ref madras_fctcall_new or \ref madras_fctcall_new_nowrap. Finally, the patcher is closed using 
 \ref madras_modifs_commit and \ref madras_terminate.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>

#include "libmadras.h"
#include "libmdbg.h"

extern help_t* madras_load_help();

#define EXE_NAME "madras"
#define TEXTNAME ".text"
#define ADDR_SEP "@"
#define SEP ';'

// Operating modes for madras -------------------------------------------------
enum madras_options {
   DISASS_BASIC = 0, /**<Disassemble and print results in a simplified format*/
   DISASS_ADV, /**<Disassemble and print results in a objdump-like format*/
   DISASS_ALL, /**<Disassemble all sections in the file*/
   PRINTELF, /**<Print ELF parsing results*/
   PRINTDATA, /**<Print data entries*/
   _ELFPART, /**<Only a part of ELF data is printed*/
   FILTER_LABEL, /**<A label is used to filter printing*/
   ELFHDR, /**<Print ELF header*/
   ELFSCN, /**<Print ELF section headers*/
   ELFSEG, /**<Print ELF program headers*/
   ELFREL, /**<Print ELF relocation tables*/
   ELFDYN, /**<Print ELF dynamic sections*/
   ELFSYM, /**<Print ELF symbol tables*/
   ELFVER, /**<Print ELF version tables*/
   ELFCODESCNS, /**<Print size and offset of code sections in the file (mainly for helping the use of raw-disass)*/
   COLORS, /**<Uses colors during printing*/
   CODING, /**<Prints instructions codings*/
   FCT_EXTERNAL, /**<Gets external functions from ELF file*/
   LIB_EXTERNAL, /**<Gets external dynamic libraries from ELF file*/
   WITH_FAMILIES, /**<Add instruction families during printing*/
   WITH_ANNOTATES, /**<Add instruction annotations during printing*/
   WITH_ROLES, /**<Add instruction roles during printing*/
   WITH_ISETS, /**<Add instruction sets during printing*/
   NINSNS_PRINT, /**<Prints the number of instructions in the file*/
   ISETS_PRINT, /**<Prints the instruction sets used in the file*/
   DBG_PRINT, /**<Prints debug informations (if available)*/
   DISASS_RAW, /**<Disassembles the contents of the file without parsing the ELF*/
   DISASS_NODBG, /**<Does not attempt to retrieve the debug informations*/
   SHELLCODE, /**<Print disassembled file in shell code*/
   CHECK_FILE, /**<Check if the file is valid*/
   ASSEMBLE_INSN, /**<Assembles a single instruction*/
   ASSEMBLE_FILE, /**<Assembles a file containing a list of instructions*/
   PATCH, /**<Patch mode*/
   STACKSAVE_KEEP, /**<Method for safeguarding the stack is keeping it*/
   STACKSAVE_MOVE, /**<Method for safeguarding the stack is moving it*/
   STACKSAVE_SHIFT, /**<Method for safeguarding the stack is shifting it*/
   SET_ELF_MACHINE, /**<Change the machine field in the ELF header*/
   MAKE_COFFEE, /**<Surprise*/
   HELP, /**<Print help*/
   MUTE, /**<Mute mode*/
   VERSION, /**<Version printing mode*/
   _H2M, /**<Use to specify help format (human / help2txt)*/
   N_OPTIONS /**<Number of possible options (must always be last)*/
};

// Colors used during printing ------------------------------------------------
enum color_codes {
   COLOR_MEM = 1, /**<This set of color is used to color instructions performing memory accesses*/
   COLOR_JMP, /**<This set of color is used to color jumps*/
   N_COLOR_CODES
};

/**
 * Stores details about an insertion request
 * */
typedef struct insrq_s {
   char* lib; /**<Library where the inserted function is defined*/
   int nb_addr; /**<Number of address*/
   int64_t* addr; /**<Arry of addresses where the insertion must take place*/
   char* fct; /**<Name of the function to insert*/
   char after; /**<Flag set to 1 if the insertion must be made after the instruction at the given address*/
   char wrap; /**<Flag set to 1 if the insertion must save the context, else 0*/
} insrq_t;

/**
 * Stores details about an delete request
 * */
typedef struct delrq_s {
   int nb_addr; /**<Number of address*/
   int64_t* addr; /**<Arry of addresses where the insertion must take place*/
   int nb_delete; /**<Number of instruction to delete*/
} delrq_t;

/**
 * Stores details about a library renaming request
 * */
typedef struct renamelibrq_s {
   char* oldname; /**<Old library name*/
   char* newname; /**<New library name*/
} renamelibrq_t;

// Global variables used by the program ---------------------------------------
char optionlist[N_OPTIONS]; /**<Array of all options*/
char* archname = NULL; /**<Name of the architecture to use when performing raw disassembly or assembly*/
uint64_t rawstart = 0; /**<Offset at which to start the raw disassembly*/
uint64_t rawlen = 0; /**<Length of bytes to disassemble as raw disassembly*/
uint64_t rawstop = 0; /**<Offset at which to stop the raw disassembly*/
int64_t rawfirst = 0; /**<First */
char* outfile = NULL; /**<Name of the output file*/
char* infile = NULL; /**<Name of the input file*/
char** labels = NULL; /**<List of labels*/
int64_t stack_shift = 512; /**<Value to shift the stack from*/
char** libraries = NULL; /**<List of libraries*/
int64_t *addresses = NULL; /**<List of addresses*/
char** insnlists = NULL; /**<List of instruction lists*/
char** functions = NULL; /**<List of external function names*/
char* mnemonic = NULL; /**<Name of mnemonic to instrument*/
char optype = 0; /**<Type of operand to instrument*/
char* label_name; /**<Name of a label used to filter printing*/
insrq_t** inserts = NULL; /**<List of insertions*/
delrq_t** deletes = NULL; /**<List of deletes*/
renamelibrq_t** renamelibs = NULL; /**<List of library renaming requests*/
int ELF_machine_code = 0; /**<New value of ELF machine code in the header*/
uint8_t printbefore = FALSE;/**<Stores whether something was printed to a file before*/

/**
 * Easter egg.
 * \return Always EXIT_SUCCESS
 * */
static int make_coffee()
{
#include "madras_coff.h"
   return EXIT_SUCCESS;
}

/**
 * Displays the short help message of the program
 * \param progname name of this binary
 */
static void shortusage(char* progname)
{
   printf(
         "usage: \n"
               "%s OPTIONS <filename>\n"
               "%s [-d -D -o <outputname> -m ...] <filename> : For disassembly\n"
               //"%s --P [--mnemonic <mnemo> --optype <r|m|i|p> --value --addrvalue -o <outputname> ] <filename> : For patching\n"
               "%s --help for more help\n", progname, progname,
         progname/*,progname*/);
}

/**
 * Displays the help message of the program
 */
static void usage()
{
   help_t* help = madras_load_help();
   help_print(help, stdout);
   help_free(help);
}

/**
 * Print the current version
 */
static void version()
{
   help_t* help = madras_load_help();
   help_version(help, stdout);
   help_free(help);
}

/**
 * Utility function to retrieve an addess in decimal or binary form
 * \param longaddr The address in string format
 * \return The value of the address
 */
int64_t utils_readhex(char* longaddr)
{
   int64_t val = 0;
   if (longaddr) {
      if ((strlen(longaddr) > 2)
            && ((longaddr[0] == '0') && (longaddr[1] == 'x')))	//Hexa
         sscanf(longaddr, "%"PRIx64, &val);
      else
         //Decimal
         sscanf(longaddr, "%"PRId64, &val);
   }
   return val;
}

/**
 * Prints the coding of an instruction as a byte stream in hexadecimal format in a string
 * \param insn Instruction whose coding we want to print
 * \param buf Buffer where to print the stream
 * \param size Size of the buffer
 * */
static void utils_print_insn_coding_hex(insn_t* insn, char* buf, size_t size)
{
   int codlen = 0;
   unsigned char* hexval = bitvector_charvalue(insn_get_coding(insn), &codlen, arch_get_endianness(insn_get_arch(insn)));
   int i = 0;
   char* c = buf;
   size_t char_limit = size + (size_t)c;
   for (i = 0; i < codlen; i++) {
      size_t written = lc_sprintf(c, char_limit - (size_t)c, " %02x", hexval[i]);
      c += written;  //Beware buffer overflows
   }
   lc_free(hexval);
}

/**
 * Parses parameters given to the option --function
 * --function="<function name>[@<address1>[@<address>]...]|<library>[|<after/before>]"
 * \param line parameters given to the options
 * \return an insertion request or NULL if there is an error
 */
static insrq_t* parse_function(const char* line)
{
   if (line == NULL)
      return (NULL);

   char* tempstr1 = strdup(line);  //copy of input line
   char* fct = NULL;           //function name
   char* lib = NULL;           //library the function belongs to
   char* addresses = NULL;           //addresses where to onsert the function
   char* pos_str = NULL;           //string containing position (after / before)
   char* context_str = NULL; //string containing context save policy (wrap / no-wrap)
   size_t sizetemp = strlen(tempstr1);           //size of the input string
   size_t i = 0;                       //counter
   int nb_delim = 1;                   //number of delimiters (SEP)
   int idx = 0;                        //counter
   char** substr = NULL;               //array of substrings
   insrq_t* request = NULL;            //insert request

   // count the number of SEP characters
   // and create the substring table
   for (i = 0; i < sizetemp; i++)
      if (tempstr1[i] == SEP)
         nb_delim++;
   substr = malloc((nb_delim + 1) * sizeof(char*));

   // replace each SEP by a '\0' and save substrings into 
   // the table
   substr[idx++] = &tempstr1[0];
   for (i = 0; i < sizetemp; i++) {
      if (tempstr1[i] == SEP) {
         if (i + 1 < sizetemp)
            substr[idx++] = &tempstr1[i + 1];
         tempstr1[i] = '\0';
      }
   }

   // move substring into variables
   fct = substr[0];
   if (nb_delim > 1)
      addresses = substr[1];
   if (nb_delim > 2)
      lib = substr[2];
   if (nb_delim > 3)
      pos_str = substr[3];
   if (nb_delim > 4)
      context_str = substr[4];

   // error case: no function or no addresses and libs
   if ((strcmp(fct, "\0") == 0)
         || (strcmp(addresses, "\0") == 0 && strcmp(lib, "\0") == 0)) {
      if (strcmp(fct, "\0") == 0)
         printf("--function: No function specified\n");
      if (strcmp(addresses, "\0") == 0 && strcmp(lib, "\0") == 0)
         printf("--function: No addresses nor libraries specified\n");
      printf("See --help to get more help");
   }
   // create the request
   else {
      request = lc_malloc(sizeof(insrq_t));
      request->fct = lc_strdup(fct);
      if (lib != NULL)
         request->lib = lc_strdup(lib);
      if (context_str != NULL && strcmp(context_str, "no-wrap") == 0)
         request->wrap = 0;
      else
         request->wrap = 1;

      if (pos_str != NULL && strcmp(pos_str, "after") == 0)
         request->after = 1;
      else
         request->after = 0;

      request->nb_addr = 0;
      if (addresses != NULL) {
         for (i = 0; i < strlen(addresses); i++)
            if (addresses[i] == ADDR_SEP[0])
               request->nb_addr++;
         request->addr = lc_malloc(request->nb_addr * sizeof(int64_t));
         char* tempstr2 = strdup(addresses);
         char* addr = strtok(tempstr2, ADDR_SEP);
         i = 0;
         while (addr != NULL) {
            request->addr[i] = utils_readhex(addr);
            addr = strtok(NULL, ADDR_SEP);
            i++;
         }
      }
   }
   return (request);
}

/**
 * Parses parameters given to the option --delete
 * --delete="@<address1>[@<address>...][;nb]"
 * \param line parameters given to the options
 */
static delrq_t* parse_delete(const char* line)
{
   if (line == NULL)
      return (NULL);

   char* tempstr1 = strdup(line);  //copy of input line
   char* addresses = NULL;           //addresses where to delete instructions
   char* nb_str = NULL;            //string containing number of delete
   int sizetemp = strlen(tempstr1);   //size of the input string
   int i = 0;                          //counter
   int nb_delim = 1;                   //number of delimiters (SEP)
   int idx = 0;                        //counter
   char** substr = NULL;               //array of substrings
   delrq_t* request = NULL;
   int nb_delete = 1;

   // count the number of SEP caractere
   // and create the substring table
   for (i = 0; i < sizetemp; i++)
      if (tempstr1[i] == SEP)
         nb_delim++;
   substr = malloc((nb_delim + 1) * sizeof(char*));

   // replace each SEP by a '\0' and save substrings into 
   // the table
   substr[idx++] = &tempstr1[0];
   for (i = 0; i < sizetemp; i++) {
      if (tempstr1[i] == SEP) {
         if (i + 1 < sizetemp)
            substr[idx++] = &tempstr1[i + 1];
         tempstr1[i] = '\0';
      }
   }

   // move substring into variables
   addresses = substr[0];
   if (nb_delim > 1)
      nb_str = substr[1];

   nb_delete = 1;
   if (nb_str != NULL)
      nb_delete = utils_readhex(nb_str);

   // error case: no function or no addresses and libs
   if (strcmp(addresses, "\0") == 0 || nb_delete < 1) {
      if (strcmp(addresses, "\0") == 0)
         printf("--delete: No address specified\n");
      if (nb_delete < 1)
         printf(
               "--delete: Number to instruction to remove is not a positive number: %s\n",
               nb_str);
      printf("See --help to get more help");
   }
   // create the request
   else {
      request = lc_malloc(sizeof(delrq_t));
      request->nb_delete = nb_delete;
      request->nb_addr = 0;
      for (i = 0; i < sizetemp; i++)
         if (tempstr1[i] == ADDR_SEP[0])
            request->nb_addr++;
      request->addr = lc_malloc(request->nb_addr * sizeof(int64_t));
      char* tempstr2 = strdup(addresses);
      char* addr = strtok(tempstr2, ADDR_SEP);
      i = 0;
      while (addr != NULL) {
         request->addr[i++] = utils_readhex(addr);
         addr = strtok(NULL, ADDR_SEP);
      }
   }
   return (request);
}

/**
 * Parses parameters given to the option --rename-library
 * --rename-library="<oldname>;<newname>"
 * \param line parameters given to the options
 */
static renamelibrq_t* parse_renamelib(const char* line)
{
   if (line == NULL)
      return (NULL);

   renamelibrq_t* request = NULL;

   //Finds the end of the name of the library to rename
   char* tempstr1 = (char*) line;
   while (*tempstr1 != '\0' && *tempstr1 != ';')
      tempstr1++;
   if (*tempstr1 == '\0') {
      printf(
            "--rename-library: Missing new name of library\nFormat should be \"<oldname>;<newname>\"\n");
      return NULL;
   }
   if (tempstr1 == line) {
      printf(
            "--rename-library: Missing name of library to rename\nFormat should be \"<oldname>;<newname>\"\n");
      return NULL;
   }

   //Finds the end of the new name of the library
   char* tempstr2 = tempstr1 + 1;
   if (*tempstr2 == '\0') {
      printf(
            "--rename-library: Missing new name of library\nFormat should be \"<oldname>;<newname>\"\n");
      return NULL;
   }
   //Initialising request object
   request = lc_malloc(sizeof(request));
   request->oldname = lc_malloc(tempstr1 - line + 1);
   request->newname = lc_malloc(strlen(tempstr2));

   strncpy(request->oldname, line, tempstr1 - line);
   request->oldname[tempstr1 - line] = '\0';

   strcpy(request->newname, tempstr2);

   return (request);
}

/**
 * Retrieves the parameters
 * \param argc Number of parameters
 * \param argv Parameters
 */
static void getparams(int argc, char* argv[])
{
   int c;
   optionlist[CODING] = 1;

   // Unique identifiers for opt flags -------------------------------------------
   enum optflags {
      OPT_DUMP_EXT = 0x100, //Starting here to avoid any collision with the other flags (defined on characters)
      OPT_FILTER_LABEL,
      OPT_ELFHDR,
      OPT_ELFSCN,
      OPT_ELFSEG,
      OPT_ELFREL,
      OPT_ELFDYN,
      OPT_ELFSYM,
      OPT_ELFVER,
      OPT_DATA_ONLY,
      OPT_ELFCODESCNS,
      OPT_CODING,
      OPT_COLOR_MEM,
      OPT_COLOR_JMP,
      OPT_RAW_DISASS,
      OPT_RAW_START,
      OPT_RAW_LEN,
      OPT_RAW_STOP,
      OPT_RAW_FIRST,
      OPT_EXTERNALS,
      OPT_EXTERNALS_LIBS,
      OPT_FAMILIES,
      OPT_ANNOTATES,
      OPT_ROLES,
      OPT_ISETS,
      OPT_DBG_PRINT,
      OPT_NODBG,
      OPT_NINSNS_PRINT,
      OPT_ISETS_PRINT,
      OPT_SHELLCODE,
      OPT_CHECK_FILE,
      OPT_ASSEMBLE_INSN,
      OPT_ASSEMBLE_FILE,
      OPT_COFFEE,
      OPT_FUNCTIONS,
      OPT_DELETES,
      OPT_STACK_KEEP,
      OPT_STACK_MOVE,
      OPT_STACK_SHIFT,
      OPT_SET_ELF_MACHINE,
      OPT_RENAME_LIBS,
      OPT__H2M
   };

   static struct option long_options[] = {
         { "disassemble", no_argument, NULL, 'd' },
         { "disasssemble-full", no_argument, NULL, 'D' },
         { "data-only", no_argument, NULL, OPT_DATA_ONLY },
         { "shell-code", no_argument, NULL, OPT_SHELLCODE },
         { "label", required_argument, 0, OPT_FILTER_LABEL },
         { "disassemble-text", no_argument, NULL, 't' },

         { "printelf", no_argument, NULL, 'e' },
         { "elfhdr", no_argument, NULL, OPT_ELFHDR },
         { "elfscn", no_argument, NULL, OPT_ELFSCN },
         { "elfseg", no_argument, NULL, OPT_ELFSEG },
         { "elfrel", no_argument, NULL, OPT_ELFREL },
         { "elfdyn", no_argument, NULL, OPT_ELFDYN },
         { "elfsym", no_argument, NULL, OPT_ELFSYM },
         { "elfver", no_argument, NULL, OPT_ELFVER },
         { "elf-code-areas", no_argument, NULL, OPT_ELFCODESCNS },

         { "no-coding", no_argument, NULL, OPT_CODING },
         { "color-mem", no_argument, NULL, OPT_COLOR_MEM },
         { "color-jmp", no_argument, NULL, OPT_COLOR_JMP },
         { "get-external-fct", no_argument, 0, OPT_EXTERNALS },
         { "get-dynamic-lib", no_argument, 0, OPT_EXTERNALS_LIBS },
         { "with-family", no_argument, 0, OPT_FAMILIES },
         { "with-annotate", no_argument, 0, OPT_ANNOTATES },
         { "with-roles", no_argument, 0, OPT_ROLES },
         { "with-isets", no_argument, 0, OPT_ISETS },
         { "with-debug", no_argument, NULL, OPT_DBG_PRINT },
         { "no-debug", no_argument, NULL, OPT_NODBG },
         { "count-insns", no_argument, NULL, OPT_NINSNS_PRINT },
         { "print-insn-sets", no_argument, NULL, OPT_ISETS_PRINT },
         { "raw-disass", required_argument, NULL, OPT_RAW_DISASS },
         { "raw-start", required_argument, 0, OPT_RAW_START },
         { "raw-len", required_argument, 0, OPT_RAW_LEN },
         { "raw-stop", required_argument, 0, OPT_RAW_STOP },
         { "raw-first", required_argument, 0, OPT_RAW_FIRST },

         { "function", required_argument, 0, OPT_FUNCTIONS },
         { "delete", required_argument, 0, OPT_DELETES },
         { "stack-keep", no_argument, NULL, OPT_STACK_KEEP },
         { "stack-move", no_argument, NULL, OPT_STACK_MOVE },
         { "stack-shift", required_argument, NULL, OPT_STACK_SHIFT },
         { "set-machine", required_argument, NULL, OPT_SET_ELF_MACHINE },
         { "rename-library", required_argument, NULL, OPT_RENAME_LIBS },

         { "assemble-insn", required_argument, NULL, 'a' },
         { "assemble-file", required_argument, NULL, 'A' },
         { "check-file", no_argument, NULL, OPT_CHECK_FILE },
         { "make-coffee", no_argument, NULL, OPT_COFFEE },
         { "mute", no_argument, NULL, 'm' },
         { "help", no_argument, NULL, 'h' },
         { "version", no_argument, NULL, 'v' },
         { "outfile", required_argument, 0, 'o' },
         { "_h2m", no_argument, NULL, OPT__H2M },
         { 0, 0, 0, 0 }
   };

   while (1) {
      int option_index = 0;
      c = getopt_long(argc, argv, "dtDhvo:a:A:me", long_options, &option_index);
      if (c == -1)
         break;

      switch (c) {
      case 0:
         //Probably some error happened here
         break;
      case 't':
         optionlist[DISASS_ADV] = 1;
         break;
      case OPT_SHELLCODE:
         optionlist[SHELLCODE] = 1;
         break;
      case OPT_FILTER_LABEL:
         optionlist[FILTER_LABEL] = 1;
         label_name = lc_strdup(optarg);
         break;
      case 'D':
         optionlist[PRINTDATA] = 1;
      case 'd':
         optionlist[DISASS_ALL] = 1;
         break;
      case 'e':
         optionlist[PRINTELF] = 1;
         break;
      case OPT_ELFHDR:
         optionlist[ELFHDR] = 1;
         optionlist[_ELFPART] = 1;
         optionlist[PRINTELF] = 1;
         break;
      case OPT_ELFSCN:
         optionlist[ELFSCN] = 1;
         optionlist[_ELFPART] = 1;
         optionlist[PRINTELF] = 1;
         break;
      case OPT_ELFSEG:
         optionlist[ELFSEG] = 1;
         optionlist[_ELFPART] = 1;
         optionlist[PRINTELF] = 1;
         break;
      case OPT_ELFREL:
         optionlist[ELFREL] = 1;
         optionlist[_ELFPART] = 1;
         optionlist[PRINTELF] = 1;
         break;
      case OPT_ELFDYN:
         optionlist[ELFDYN] = 1;
         optionlist[_ELFPART] = 1;
         optionlist[PRINTELF] = 1;
         break;
      case OPT_ELFSYM:
         optionlist[ELFSYM] = 1;
         optionlist[_ELFPART] = 1;
         optionlist[PRINTELF] = 1;
         break;
      case OPT_ELFVER:
         optionlist[ELFVER] = 1;
         optionlist[_ELFPART] = 1;
         optionlist[PRINTELF] = 1;
         break;
      case OPT_ELFCODESCNS:
         optionlist[ELFCODESCNS] = 1;
         optionlist[_ELFPART] = 1;
         optionlist[PRINTELF] = 1;
         break;
      case OPT_CODING:
         optionlist[CODING] = 0;
         break;
      case OPT_DATA_ONLY:
         optionlist[PRINTDATA] = 1;
         break;
      case OPT_COLOR_MEM:
         optionlist[COLORS] = COLOR_MEM;
         break;
      case OPT_COLOR_JMP:
         optionlist[COLORS] = COLOR_JMP;
         break;
      case OPT_RAW_DISASS:
         optionlist[DISASS_RAW] = 1;
         archname = optarg;
         break;
      case OPT_RAW_START:
         rawstart = utils_readhex(optarg);
         break;
      case OPT_RAW_LEN:
         if (!rawstop)
            rawlen = utils_readhex(optarg);
         else {
            rawstop = 0;
            rawlen = 0; //Ignoring both if both are present
         }
         break;
      case OPT_RAW_STOP:
         if (!rawlen)
            rawstop = utils_readhex(optarg);
         else {
            rawstop = 0;
            rawlen = 0; //Ignoring both if both are present
         }
         break;
      case OPT_RAW_FIRST:
         rawfirst = utils_readhex(optarg);
         break;
      case OPT_EXTERNALS:
         optionlist[FCT_EXTERNAL] = 1;
         break;
      case OPT_EXTERNALS_LIBS:
         optionlist[LIB_EXTERNAL] = 1;
         break;
      case OPT_FAMILIES:
         optionlist[WITH_FAMILIES] = 1;
         break;
      case OPT_ANNOTATES:
         optionlist[WITH_ANNOTATES] = 1;
         break;
      case OPT_ROLES:
         optionlist[WITH_ROLES] = 1;
         break;
      case OPT_ISETS:
         optionlist[WITH_ISETS] = 1;
         break;
      case OPT_COFFEE:
         optionlist[MAKE_COFFEE] = 1;
         break;
      case OPT_DBG_PRINT:
         optionlist[DBG_PRINT] = 1;
         break;
      case OPT_NODBG:
         optionlist[DISASS_NODBG] = 1;
         break;
      case OPT_NINSNS_PRINT:
         optionlist[NINSNS_PRINT] = 1;
         break;
      case OPT_ISETS_PRINT:
         optionlist[ISETS_PRINT] = 1;
         break;
      case OPT_CHECK_FILE:
         optionlist[CHECK_FILE] = 1;
         break;
      case 'm':
         optionlist[MUTE] = 1;
         break;
      case 'v':
         optionlist[VERSION] = 1;
         break;
      case 'h':
         optionlist[HELP] = 1;
         break;
      case 'o':
         outfile = optarg;
         break;

      case 'a':
         optionlist[ASSEMBLE_INSN] = 1;
         archname = optarg;
         //In that case, we will be storing the instruction in \c infile to avoid adding to many variables and tests
         break;
      case 'A':
         optionlist[ASSEMBLE_FILE] = 1;
         archname = optarg;
         break;

      case OPT_FUNCTIONS: {
         int i = 0;
         // just count the number in insertion requests
         for (i = 0; inserts != NULL && inserts[i] != NULL; i++) {
         }

         insrq_t* req = parse_function(optarg);
         if (req != NULL) {
            optionlist[PATCH] = 1;
            inserts = realloc(inserts, (i + 2) * sizeof(insrq_t*));
            inserts[i] = req;
            inserts[i + 1] = NULL;
         }

      }
         break;
      case OPT_DELETES: {
         int i = 0;
         // just count the number in delete requests
         for (i = 0; deletes != NULL && deletes[i] != NULL; i++) {
         }

         delrq_t* req = parse_delete(optarg);
         if (req != NULL) {
            optionlist[PATCH] = 1;
            deletes = realloc(deletes, (i + 2) * sizeof(delrq_t*));
            deletes[i] = req;
            deletes[i + 1] = NULL;
         }

      }
         break;
      case OPT_RENAME_LIBS: {
         int i = 0;
         // just count the number in rename library requests
         for (i = 0; renamelibs != NULL && renamelibs[i] != NULL; i++) {
         }

         renamelibrq_t* req = parse_renamelib(optarg);
         if (req != NULL) {
            optionlist[PATCH] = 1;
            renamelibs = realloc(renamelibs, (i + 2) * sizeof(*renamelibs));
            renamelibs[i] = req;
            renamelibs[i + 1] = NULL;
         }

      }
         break;
      case OPT_STACK_KEEP:
         optionlist[STACKSAVE_KEEP] = 1;
         break;
      case OPT_STACK_MOVE:
         optionlist[STACKSAVE_MOVE] = 1;
         break;
      case OPT_STACK_SHIFT:
         optionlist[STACKSAVE_SHIFT] = 1;
         stack_shift = utils_readhex(optarg);
         break;
      case OPT_SET_ELF_MACHINE:
         optionlist[SET_ELF_MACHINE] = 1;
         optionlist[PATCH] = 1;
         ELF_machine_code = utils_readhex(optarg);
         break;
      case OPT__H2M:
         optionlist[_H2M] = 1;
         break;
      }
   }
   if (optind < argc) {
      while (optind < argc) {
         infile = argv[optind++];
      }
   }
}

/**
 * Function run before each instruction printing
 * \param asmf an assembly file
 * \param in current instruction
 * \param out stream where to print
 */
static void before_printing(elfdis_t* ed, insn_t* in, FILE* out)
{
   (void) ed;
   if (out != stdout && out != stderr)
      return;

   if (optionlist[COLORS] == COLOR_MEM) {
      // used to add colors on memory / fp instructions
      // red: memory instructions
      // blue: fp instructions
      // default: other instructions
      if (insn_get_family(in) != FM_LEA)//removing LEA family, does not perform a memory access
      {
         int i = 0;
         for (i = 0; i < insn_get_nb_oprnds(in); i++)	//look for a memory operand, if found, it is a memory instruction
               {
            if (oprnd_is_mem(insn_get_oprnd(in, i)) == 1) {
               fprintf(out, "\033[31m");
               return;
            }
         }
      }
      if (insn_get_family(in) == FM_POP//POP and PUSH are considered as memory instructions
      || insn_get_family(in) == FM_PUSH)
         fprintf(out, "\033[31m");
      /* else if (insn_get_family(in) == FM_NOP	//NOP, LEAVE and branchs are not considered as fp instructions */
      /* 			&& insn_is_branch (in) == 0		//but all other instructions are considered as fp instructions */
      /* && insn_get_family(in) == FM_LEAVE) */
      /* 	fprintf (out, "\033[34m"); */
   } else if (optionlist[COLORS] == COLOR_JMP) {
      // used to add colors on branchs
      // red: unsolved indirect branches
      // blue: direct branches
      // green: solved indirect branches
      // default: other instructions
      if (insn_is_branch(in) != 0) {
         if ((in->nb_oprnd == 1)
               && ((oprnd_is_mem(in->oprndtab[0]) == TRUE)
                     || (oprnd_is_reg(in->oprndtab[0]) == TRUE))) {
            if (in->annotate & A_IBNOTSOLVE)
               fprintf(out, "\033[31m");
            else
               fprintf(out, "\033[32m");
         } else
            fprintf(out, "\033[34m");
      }
   }
}

/**
 * Function run after each instruction printing
 * \param asmf an assembly file
 * \param in current instruction
 * \param out stream where to print
 */
static void after_printing(elfdis_t* ed, insn_t* in, FILE* out)
{
   asmfile_t* asmf = ed->afile;

   if ((asmf == NULL) || (in == NULL) || (out == NULL))
      return;

   if (optionlist[WITH_FAMILIES])
      fprintf(out, "\t(family: %d)", insn_get_family(in));
   if (optionlist[WITH_ANNOTATES])
      fprintf(out, "\t(annotate: %x)", insn_get_annotate(in));
   if (optionlist[DBG_PRINT]) {
      if (in->debug != NULL)
         fprintf(out, "\t(%s:%d)", in->debug->srcfile, in->debug->srcline);
   }
   if (optionlist[WITH_ROLES]) {
      int i;
      fprintf(out, "\t(roles: ");
      for (i = 0; i < insn_get_nb_oprnds(in); i++)
         fprintf(out, " %d ", oprnd_get_role(insn_get_oprnd(in, i)));
      fprintf(out, ")");
   }
   if (optionlist[WITH_ISETS])
      fprintf(out, "\t(iset: %s)",
            arch_get_iset_name(insn_get_arch(in), insn_get_iset(in)));
   if (out != stdout && out != stderr)
      return;
   if (optionlist[COLORS] == COLOR_JMP || optionlist[COLORS] == COLOR_MEM)
      fprintf(out, "\033[0m");
}

static FILE* openoutfile()
{
   FILE* out;                             // output file
   // opens the output stream. ---------------------------------------
   // It is stdout by default, and it can be set using -o option
   if (outfile == NULL)
      out = stdout;
   else {
      out = fopen(outfile, (printbefore) ? "a+" : "w");
      if (out == NULL) {
         ERRMSG("Unable to open file %s\n", outfile);
//         exit (-1);
      }
      printbefore = TRUE;
   }
   return out;
}

static void closeoutfile(FILE* out)
{
   // close the stream ----------------------------------------------
   // It is needed when the output stream is not stdout or stderr
   if (out != stdout && out != stderr)
      fclose(out);
}

/**
 * Prints a disassembled file
 * \param asmf an assembly file
 * \return EXIT_SUCCESS if the file can be disassembler, error code otherwise
 */
static int printfile(asmfile_t* asmf)
{
   FILE* out;										// output file
   int64_t start = 0, end = 0;	// addresses where to start and stop printing
   int answ = EXIT_SUCCESS;

   out = openoutfile();
   if (!out)
      return ERR_COMMON_UNABLE_TO_OPEN_FILE;

   // looks for .text sections bounds -------------------------------
   // It is needed with -d option
   if ((optionlist[DISASS_ADV] == 1)
         && (asmfile_test_analyze(asmf, DIS_ANALYZE))) {
      int indx = -1;
      uint16_t i = 0;
      binfile_t* bf = asmfile_get_binfile(asmf);
      for (i = 0; i < binfile_get_nb_sections(bf); i++)
         if (str_equal(binfile_get_scn_name(bf, i), TEXTNAME))
            indx = i;

      if (indx > -1) {
         binscn_t* txtscn = binfile_get_scn(bf, indx);
         start = binscn_get_addr(txtscn);
         end = start + binscn_get_size(txtscn) - 1;
      }
   }

   // if ask to print from a label to another one
   if (label_name != NULL) {
      // get the start register
      insn_t* insn_start = asmfile_get_insn_by_label(asmf, label_name);
      if (insn_start != NULL) {
         start = INSN_GET_ADDR(insn_start);
         // then get the address of the last instruction before next register
         FOREACH_INLIST(insn_start->sequence, it)
         {
            insn_t* insn = GET_DATA_T(insn_t*, it);
            if (insn->fctlbl == insn_start->fctlbl)
               end = INSN_GET_ADDR(insn);
            else
               break;
         }
      } else {
         fprintf(stderr, "Label %s can not been found in the binary\n",
               label_name);
      }
   }
   // prints instructions -------------------------------------------
   elfdis_t* mdrs = madras_load_parsed(asmf);

   if (optionlist[SHELLCODE] == 1)
      madras_insns_print_shellcode(mdrs, out, start, end);
   else if (optionlist[DISASS_ADV] == 1 || optionlist[DISASS_ALL] == 1)
      madras_insns_print(mdrs, out, start, end, TRUE, TRUE, optionlist[CODING],
            &before_printing, &after_printing);

   madras_unload_parsed(mdrs);
   madras_terminate(mdrs);

   closeoutfile(out);
   return answ;
}

/**
 * Prints the data entries contained in a parsed file
 * \param asmf an assembly file
 */
static void printdata(asmfile_t* asmf)
{
   /**\todo TODO (2014-12-01) Cobbling this quickly and dirtily to print something for my tests. Move this to libmadras.c and
    * make this more customisable and with a prettier output and some bells and whistles.*/
   FILE* out;                             // output file
   uint16_t i;

   out = openoutfile();
   binfile_t* bf = asmfile_get_binfile(asmf);
   if (!bf)
      return;

   for (i = 0; i < binfile_get_nb_load_scns(bf); i++) {
      uint32_t j;
      binscn_t* scn = binfile_get_load_scn(bf, i);
      fprintf(out, "\nVariables in section %s:\n", binscn_get_name(scn));
      for (j = 0; j < binscn_get_nb_entries(scn); j++) {
         data_t* entry = binscn_get_entry(scn, j);
         if (!entry) {
            fprintf(out, " %p", entry);
         } else {
            label_t* lbl = data_get_label(entry);
            if (label_get_addr(lbl) == data_get_addr(entry))
               fprintf(out, "<%s>:\n", label_get_name(lbl));
            fprintf(out, " %#"PRIx64": ", data_get_addr(entry));
            data_fprint(entry, out);
         }
         fprintf(out, "\n");
      }
   }
   closeoutfile(out);
}

/**
 * Prints external libraries using ELF file data
 * \param asmf an assembly file
 * \return EXIT_SUCCESS if the file is found and dynamic, error code otherwise
 */
static int printexternals_libs(asmfile_t* asmf)
{
   FILE* out;										// output file
   int answ = EXIT_SUCCESS;
   out = openoutfile();
   if (!out)
      return ERR_COMMON_UNABLE_TO_OPEN_FILE;
//			exit (-1);
//	list_t* libs = elffile_get_dynamic_libs ((elffile_t*) asmfile_getorigin(asmf));

   binfile_t* bf = asmfile_get_binfile(asmf);
   if (binfile_get_nb_ext_libs(bf) > 0) {
      uint16_t i;
         fprintf(out, "Dynamic libraries:\n");
//      FOREACH_INLIST (libs, it)
      for (i = 0; i < binfile_get_nb_ext_libs(bf); i++) {
         fprintf(out, "\t%s\n", binfile_get_ext_lib_name(bf, i));
      }
//      list_free (libs, NULL);

   } else {
      if (!optionlist[MUTE]) {
         fprintf(out, "%s is a static executable\n", asmf->name);
      }
      answ = ERR_BINARY_NO_EXTLIBS;
   }
   closeoutfile(out);

   return answ;
}

/**
 * Checks if a given file is a valid ELF binary
 * \param filename a file to check
 * \return EXIT_SUCCESS if the file is valid, ERR_COMMON_FILE_INVALID otherwise
 */
static int checkfile(char* filename)
{
   int archcode = 0, filecode = 0;
   int answ = EXIT_SUCCESS;
   printf("Testing file %s\n", filename);
   if (madras_is_file_valid(filename, &archcode, &filecode)) {
      if (!optionlist[MUTE]) {
         printf("File %s is valid, from type %d and from archi %d\n", filename,
               filecode, archcode);
      }
   } else {
      if (!optionlist[MUTE]) {
         printf("File %s is invalid\n", filename);
      }
      answ = ERR_COMMON_FILE_INVALID;
   }
   return answ;
}

/**
 * Inserts functions using insertion requests
 * \param madras Structure representing the file to patch
 * \return EXIT_SUCCESS if the insertion can be made successfully, error code otherwise
 */
static int insertfunctions(elfdis_t* madras)
{
   int i = 0;
   int answ = EXIT_SUCCESS;

   for (i = 0; inserts[i] != NULL; i++) {
      // insert a function at specific addresses
      if (inserts[i]->nb_addr > 0) {
         int iaddr = 0;
         for (iaddr = 0; iaddr < inserts[i]->nb_addr; iaddr++) {
            char* lib = (inserts[i]->lib != NULL) ? inserts[i]->lib : "(NULL)";
            char* pos = (inserts[i]->after == 1) ? "\"after\"" : "\"before\"";
            if (inserts[i]->wrap == 0) {
               // without saving the context
               printf(
                     "Insert call to %s from library %s at address 0x%"PRIx64", at position %s without saving the context\n",
                     inserts[i]->fct, lib, inserts[i]->addr[iaddr], pos);
               modif_t* mod = madras_fctcall_new_nowrap(madras, inserts[i]->fct,
                     inserts[i]->lib, inserts[i]->addr[iaddr],
                     inserts[i]->after);
               if (!mod)
                  answ = madras_get_last_error_code(madras);
            } else {
               // saving the context
               printf(
                     "Insert call to %s from library %s at address 0x%"PRIx64", at position %s\n",
                     inserts[i]->fct, lib, inserts[i]->addr[iaddr], pos);
               modif_t* mod = madras_fctcall_new(madras, inserts[i]->fct,
                     inserts[i]->lib, inserts[i]->addr[iaddr],
                     inserts[i]->after, NULL, 0);
               if (!mod)
                  answ = madras_get_last_error_code(madras);
            }
         }
      } else {
         // no address where to insert => no call
         printf("Insert function %s from library %s in the binary\n",
               inserts[i]->fct, inserts[i]->lib);
         modif_t* mod = madras_fct_add(madras, inserts[i]->fct, inserts[i]->lib,
               NULL);
         if (!mod)
            answ = madras_get_last_error_code(madras);
      }
   }
   return answ;
}

/**
 * Removes instructions using delete requests
 * \param madras Structure representing the file to patch
 * \return EXIT_SUCCESS if the deletion can be made successfully, error code otherwise
 */
static int removeinstructions(elfdis_t* madras)
{
   int i = 0;
   int answ = EXIT_SUCCESS;
   for (i = 0; deletes[i] != NULL; i++) {
      int iaddr = 0;
      for (iaddr = 0; iaddr < deletes[i]->nb_addr; iaddr++) {
         printf("Deletes %d instruction%s at address 0x%"PRIx64"\n",
               deletes[i]->nb_delete,
               ((deletes[i]->nb_delete > 1) ? "(s)" : ""),
               deletes[i]->addr[iaddr]);
         modif_t* mod = madras_delete_insns(madras, deletes[i]->nb_delete,
               deletes[i]->addr[iaddr]);
         if (!mod)
            answ = madras_get_last_error_code(madras);
         if (ISERROR(answ))
            break;
      }
   }
   return answ;
}

/**
 * Rename Libraries using library renaming requests
 * \param madras Structure representing the file to patch
 * \return EXIT_SUCCESS if the renaming can be made successfully, error code otherwise
 */
static int renamelibraries(elfdis_t* madras)
{
   int i = 0;
   int answ = EXIT_SUCCESS;
   for (i = 0; renamelibs[i] != NULL; i++) {
      printf("Renaming external library %s to %s\n", renamelibs[i]->oldname,
            renamelibs[i]->newname);
      modiflib_t* mod = madras_extlib_rename(madras, renamelibs[i]->oldname,
            renamelibs[i]->newname);
      if (!mod)
         answ = madras_get_last_error_code(madras);
      if (ISERROR(answ))
         break;
   }
   return answ;
}

/**
 * Changes to machine in the ELF header
 * \param madras Structure representing the file to patch
 * \return EXIT_SUCCESS if the change can be made successfully, error code otherwise
 */
static int changemachine(elfdis_t* madras)
{
   int answ = EXIT_SUCCESS;

   printf("Updating to new machine code : %d (0x%x)\n", ELF_machine_code,
         ELF_machine_code);
   answ = madras_change_ELF_machine(madras, ELF_machine_code);
   return answ;
}

/**
 * Prints ELF data
 * \param asmf an assembly file
 * \return EXIT_SUCCESS if the file can be found and parsed, error code otherwise
 */
static int printELFdata(asmfile_t* asmf)
{
   int answ = EXIT_SUCCESS;
   if (asmf == NULL)
      return ERR_LIBASM_MISSING_ASMFILE;
//   elffile_t* efile = (elffile_t*)asmfile_getorigin(asmf);
   binfile_t* bf = asmfile_get_binfile(asmf);
   int symtab_idx = -1;
   int dynstr_idx = -1;

   if (bf == NULL)
      return ERR_BINARY_MISSING_BINFILE;

   if (optionlist[ELFCODESCNS] == 1) {
      binfile_print_code_areas(bf);
      return EXIT_SUCCESS;
   }
   int64_t options = BINPRINT_OPTIONS_NOPRINT;
   if (optionlist[_ELFPART] == 0 || optionlist[ELFHDR] == 1)
      options |= BINPRINT_OPTIONS_HDR;
   //print_elf_header (efile);
   if (optionlist[_ELFPART] == 0 || optionlist[ELFSCN] == 1)
      options |= BINPRINT_OPTIONS_SCNHDR;
//      print_section_header (efile);
   if (optionlist[_ELFPART] == 0 || optionlist[ELFSEG] == 1)
      options |= BINPRINT_OPTIONS_SEGHDR;
//   {
//      if (Elf_Ehdr_get_e_phnum (efile, NULL) > 0)
//      {
//         print_segment_header (efile);
//         print_mapping_segment_sections (efile);
//      }
//   }

   if (optionlist[_ELFPART] == 0 || optionlist[ELFDYN] == 1)
      options |= BINPRINT_OPTIONS_DYN;
//   {
//      if (efile->indexes[DYN] != -1)
//         print_dynamic_section (efile);
//   }

//   lookfor_sections (efile, &dynstr_idx, &symtab_idx);
//   if (symtab_idx != -1 && dynstr_idx != -1)
//   {
   if (optionlist[_ELFPART] == 0 || optionlist[ELFREL] == 1)
      options |= BINPRINT_OPTIONS_REL;
//         print_rela_section (efile, dynstr_idx, symtab_idx);
   if (optionlist[_ELFPART] == 0 || optionlist[ELFSYM] == 1)
      options |= BINPRINT_OPTIONS_SYM;
//         print_sym_tables (efile, dynstr_idx);
//   }

   if (optionlist[_ELFPART] == 0 || optionlist[ELFVER] == 1)
      options |= BINPRINT_OPTIONS_VER;
//   {
//       unsigned int i;
//       for(i = 0; (unsigned int) i < (unsigned int) efile->nsections; i++)
//       {
//           if (Elf_Shdr_get_sh_type (efile, i, NULL) == SHT_GNU_verneed)
//               print_verneed(efile, i);
//           if (Elf_Shdr_get_sh_type (efile, i, NULL) == SHT_GNU_versym)
//               print_versym(efile, i);
//       }
//   }
   asmfile_add_parameter(asmf, PARAM_MODULE_BINARY, PARAM_BINPRINT_OPTIONS,
         (void*) options);
   /**\todo TODO (2014-11-19) We could create and independent asmfile_print_binfile function that would then invoke the function from the driver*/
   binfile_get_driver(bf)->asmfile_print_binfile(asmf);
   return answ;
}

/**
 * Runs all analysis / patch on a given asmfile
 * */
static int execute_file(asmfile_t* asmf)
{
   int answ = EXIT_SUCCESS;
   // Prints disassembled file
   if (!optionlist[MUTE]
         && (optionlist[DISASS_ADV] == 1 || optionlist[DISASS_ALL] == 1
               || optionlist[SHELLCODE] == 1))
      answ = printfile(asmf);
   if (!optionlist[MUTE] && optionlist[PRINTDATA] == 1)
      printdata(asmf);
   // Prints externals functions
   if (!optionlist[MUTE] && optionlist[FCT_EXTERNAL] == 1)
      answ =
            binfile_get_driver(asmfile_get_binfile(asmf))->asmfile_print_external_fcts(
                  asmf, outfile);
   // Prints externals libraries
   if (optionlist[LIB_EXTERNAL] == 1)
      answ = printexternals_libs(asmf);
   // Prints ELF data
   if (!optionlist[MUTE] && optionlist[PRINTELF] == 1)
      answ = printELFdata(asmf);
   // Prints number of instructions
   if (optionlist[NINSNS_PRINT] == 1)
      printf("Number of instructions in file: %d\n",
            queue_length(asmfile_get_insns(asmf)));
   // Prints instruction sets
   if (!optionlist[MUTE] && optionlist[ISETS_PRINT] == 1) {
      unsigned int i;
      printf("Instruction sets used in file:\n");
      for (i = 1; i < arch_get_nb_isets(asmfile_get_arch(asmf)); i++) {
         if (asmfile_check_iset_used(asmf, i))
            printf("\t%s\n", arch_get_iset_name(asmfile_get_arch(asmf), i));
      }
   }
   return answ;
}

/**
 * This function runs all analysis / patch on the binary
 */
static int execute()
{
   asmfile_t* asmf;
   //asmfile_t** asmfs = NULL;
   int answ = EXIT_SUCCESS;
   if (optionlist[CHECK_FILE])
      answ = checkfile(infile);
   else if (optionlist[PATCH] == 1) {
      //Disassembles the file
      elfdis_t* madras = madras_disass_file(infile);
      char* out = NULL;
      //Generates the name of the output file
      if (outfile == NULL) {
         out = lc_malloc((strlen(infile) + 6) * sizeof(char));
         sprintf(out, "%s_mdrs", infile);
      } else
         out = lc_strdup(outfile);
      printf("Output binary: %s\n", out);

      //Opens the file for modifications
      if (optionlist[STACKSAVE_KEEP])
         answ = madras_modifs_init(madras, STACK_KEEP, 0);
      else if (optionlist[STACKSAVE_MOVE])
         answ = madras_modifs_init(madras, STACK_MOVE, 0);
      else
         answ = madras_modifs_init(madras, STACK_SHIFT, stack_shift);

      //Performs the requested modifications
      if (!ISERROR(answ) && inserts != NULL)
         answ = insertfunctions(madras);
      if (!ISERROR(answ) && deletes != NULL)
         answ = removeinstructions(madras);
      if (!ISERROR(answ) && renamelibs != NULL)
         answ = renamelibraries(madras);
      if (!ISERROR(answ) && optionlist[SET_ELF_MACHINE])
         answ = changemachine(madras);

      if (!ISERROR(answ))
         answ = madras_modifs_commit(madras, out);
      madras_terminate(madras);
      lc_free(out);
   } else if (optionlist[ASSEMBLE_INSN] == 1
         || optionlist[ASSEMBLE_FILE] == 1) {
      if (archname == NULL) {
         ERRMSG("No architecture provided for assembly\n");
         return ERR_LIBASM_ARCH_MISSING;
      }
      asmbldriver_t* driver = asmbldriver_load_byarchname(archname);
      if (driver == NULL) {
         return ERR_LIBASM_ARCH_UNKNOWN;
      }
      if (optionlist[ASSEMBLE_INSN] == 1) {
         insn_t *in = insn_parsenew(infile, driver->getarch());
         if (!in) {
            ERRMSG(
                  "Unable to parse string %s as a valid instruction for architecture %s\n",
                  infile, archname);
            asmbldriver_free(driver);
            return ERR_LIBASM_INSTRUCTION_NOT_PARSED;
         }
         answ = assemble_insn(in, driver);
         if (!ISERROR(answ)) {
            char buf[256] = { 0 }, hex[256] = { 0 };
            insn_print(in, buf, 256);
            utils_print_insn_coding_hex(in, hex, sizeof(hex));
            printf("Coding of instruction \"%s\" is %s\n", buf, hex);
         }
         insn_free(in);
      } else if (optionlist[ASSEMBLE_FILE] == 1) {
         FILE* stream = NULL;
         size_t flen = 0;
         char* content = getFileContent(infile, &stream, &flen);
         if (content == NULL) {
            ERRMSG("Unable to read content of file %s\n", infile);
            return ERR_COMMON_UNABLE_TO_READ_FILE;
         }
         asmfile_t* af = asmfile_new(infile);
         asmfile_set_arch(af, driver->getarch());
         queue_t* insnlist = NULL;
         answ = assemble_strlist(driver, content, af, &insnlist);
         //if (!ISERROR(answ)) {
         /**\todo (2016-10-10) Removing the test on answ because it causes problems with the exhaustive tests
          * (there are cases where one instruction fails, but not the whole list)
          * Find something that works in all cases*/
            char coding[256];
            FOREACH_INQUEUE(insnlist, iter)
            {
               insn_t* insn = GET_DATA_T(insn_t*, iter);
               if (insn_get_coding(insn) != NULL) {
                  utils_print_insn_coding_hex(insn, coding, sizeof(coding));
                  printf("%s\n", coding);
               }
            }
         //}
         if (insnlist)
            queue_free(insnlist, insn_free);
         asmfile_free(af);
         releaseFileContent(content, stream);
      }
      asmbldriver_free(driver);
   } else {
      int disass_options = DISASS_OPTIONS_FULLDISASS;
      asmf = asmfile_new(infile);

      if ((optionlist[DISASS_BASIC] | optionlist[DISASS_ADV]
            | optionlist[DISASS_ALL] | optionlist[SHELLCODE]) == 0)
         disass_options |= DISASS_OPTIONS_NODISASS;
      if (optionlist[DISASS_NODBG] == 1)
         asmfile_add_parameter(asmf, PARAM_MODULE_DEBUG,
               PARAM_DEBUG_DISABLE_DEBUG, (void*) TRUE);
      if (optionlist[LIB_EXTERNAL] == 1) {
         disass_options |= DISASS_OPTIONS_NODISASS;
         asmfile_add_parameter(asmf, PARAM_MODULE_DEBUG,
               PARAM_DEBUG_DISABLE_DEBUG, (void*) TRUE);
      }

      asmfile_add_parameter(asmf, PARAM_MODULE_DISASS, PARAM_DISASS_OPTIONS,
            (void*) disass_options);
      if (optionlist[DISASS_RAW] == 1) {
         if (rawstop > 0 && rawstop > rawstart)
            rawlen = rawstop - rawstart;
         answ = asmfile_disassemble_raw(asmf, rawstart, rawlen, rawfirst,
               archname);
         optionlist[DISASS_ALL] = 1; // TO ensure the result will be printed
      } else
         answ = asmfile_disassemble(asmf);
      //n_members = asmfile_disassemble_n (asmf, &asmfs, disass_options, 0);
      if (ISERROR(answ))
         return answ;

      /**\todo TODO (2014-11-19) Restore disassembly of archives
       * => (2015-05-22) I'm on it*/
      if (asmfile_is_archive(asmf)) {
         uint16_t n_members = asmfile_get_nb_archive_members(asmf);
         ;
         uint16_t i;
         for (i = 0; i < n_members; i++) {
            asmfile_t* asmfm = asmfile_get_archive_member(asmf, i);
            //asmfile_load_dbg (asmfs[i]);
            if (!optionlist[MUTE])
               printf("* FILE: %s\n********\n", asmfile_get_name(asmfm));
            answ = execute_file(asmfm);
            //asmfile_unload_dbg (asmfs[i]);
            //asmfile_free(asmfs[i]);
         }
         //asmfile_free (asmf);
         //lc_free(asmfs);
      } else {
         //asmfile_load_dbg (asmf);
         answ = asmfile_get_last_error_code(asmf);
         if (!ISERROR(answ)) {
            if (answ != EXIT_SUCCESS)
               errcode_printfullmsg(answ);
            answ = execute_file(asmf);
         }
         //asmfile_unload_dbg (asmf);
      }
      asmfile_free(asmf);
   }
   return answ;
}

// Main function --------------------------------------------------------------
#ifdef STATIC_MODULE
//This is the name of the main function to invoke when calling madras from another program
int madras_main(int argc,char* argv[])
#else
int main(int argc, char* argv[])
#endif
{
   int answ = EXIT_SUCCESS;
   // Detecting the case where there are not enough arguments
   if (argc < 1) {
      shortusage(EXE_NAME);
      return ERR_COMMON_PARAMETER_MISSING;
   } else if (argc == 1) {
      shortusage(argv[0]);
      return ERR_COMMON_PARAMETER_MISSING;
   }

   // At least 1 arguments is present, we retrieve them
   getparams(argc, argv);

   // Prints selected options, used for debug purpose
   DBG( {
      int i;
      for (i = 0; i < N_OPTIONS; i++)
         printf("optionlist[%d]=%d\n", i, optionlist[i])
         ;
   });

   // Prints help, then exit 
   if (optionlist[HELP]) {
      usage();
      return EXIT_SUCCESS;
   }

   if (optionlist[MAKE_COFFEE]) {
      make_coffee();
      return EXIT_SUCCESS;
   }

   // Prints version, then exit
   if (optionlist[VERSION]) {
      version();
      return EXIT_SUCCESS;
   }

   // Checks if there is an input file
   if (!infile) {
      answ =
            (optionlist[ASSEMBLE_INSN] == 1) ?
                  ERR_LIBASM_INSTRUCTION_MISSING : ERR_COMMON_FILE_NAME_MISSING;
      errcode_printfullmsg(answ);
      return answ;
   }

   // Runs MADRAS
   answ = execute();

   /**\todo (2015-09-08) The following code is commented as long as __MAQAO_VERBOSE_LEVEL__ is forced to MAQAO_VERBOSE_ALL
    * to avoid printing too many error messages, as they will also be printed during execution. When we implement a way
    * to set the verbosity of MAQAO, this could should be uncommented (and possibly updated) to ensure that a message is
    * printed if an error occurred*/
   /*if (answ != EXIT_SUCCESS) {
    errcode_printfullmsg(answ);
    }*/

   // Cleans memory 
   //cleanup();
   return answ;
}
