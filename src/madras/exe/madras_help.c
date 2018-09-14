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

#include "libmcommon.h"
#include "version.h"

/**
 * Load MADRAS help into an help_t object
 * \return an initialized help object
 */
help_t* madras_load_help()
{
   help_t* help = help_initialize ();

   help_set_description (help,
        "The maqao madras module is a program used to test and use some functionalities of libmadras.\n"
        "Today, it allows the user to disassemble a file (such as objdump, from GNU Binutils), get data\n"
        "on the ELF structure (such as readelf, also from GNU Binutils). It also allows the user to patch\n"
        "a binary (for the moment, only a simple function insertion can be done, but more features will\n"
        "be added in next releases).");
   help_set_usage (help, "maqao madras <filename> ...");
   help_set_bugs (help, MAQAO_BUGREPORT);
   help_set_author (help, MAQAO_AUTHORS);
   help_set_copyright (help, MAQAO_COPYRIGHT);
   help_set_program (help, "maqao-madras");
   help_set_version (help, MAQAO_VERSION);

   help_set_build (help, MAQAO_BUILD);
   help_set_date (help, MAQAO_DATE);


   // Disassembly options
   help_add_separator (help, "Disassembling");
   help_add_option (help, "d", "disassemble",     "Prints the disassembly of all sections in a format similar to objdump.", NULL, FALSE);
   help_add_option (help, "t", "disassemble-text", "Prints the disassembly of the .text section in a format similar to objdump.", NULL, FALSE);
   help_add_option (help, "D", "disassemble-full", "Prints the disassembly of all sections in a format similar to objdump plus the parsing of data sections.", NULL, FALSE);
   help_add_option (help, NULL, "data-only",      "Prints the parsing of data sections.", NULL, FALSE);
   help_add_option (help, NULL, "shell-code",     "Prints the disassembly of all sections in sheel code format.", NULL, FALSE);
   help_add_option (help, NULL, "label",          "[Disassembly filter] Prints instruction from the given label to the next one.", "<name>", FALSE);
   help_add_option (help, NULL, "color-mem",      "Adds colors during printing: colors instructions performing memory accesses in red\n"
                                                  "and floating point instructions in blue.", NULL, FALSE);
   help_add_option (help, NULL, "color-jmp",      "Adds colors during printing: colors unsolved indirect branches in red, solved\n"
                                                  "indirect branches in green and other branches in blue", NULL, FALSE);
   help_add_option (help, NULL, "no-coding",      "Does not print instruction codings.", NULL, FALSE);
   help_add_option (help, NULL, "raw-disass",     "Raw disassembly: disassembles the contents of the file without parsing the ELF using\n"
                                                  "architecture <arch>.", "<arch>", FALSE);
   help_add_option (help, NULL, "raw-start",      "[Raw disassembly option] Starts disassembly after <offset> bytes (0 if not set).", "<offset>", FALSE);
   help_add_option (help, NULL, "raw-len",        "[Raw disassembly option] Disassembles <len> bytes (whole file if not set or set to 0).\n"
                                                  "Ignored if raw-stop is used.", "<len>", FALSE);
   help_add_option (help, NULL, "raw-stop",       "[Raw disassembly option] Stops disassembly at <offset> bytes (whole file if not set\n"
                                                  "or set to 0). Ignored if raw-len is used.", "<offset>", FALSE);
   help_add_option (help, NULL, "raw-first",      "[Raw disassembly option] Assigns address <addr> to the first disassembled\n"
                                                  "instruction (0 if not set).", "<addr>", FALSE);
   help_add_option (help, NULL, "with-family",    "Adds instruction family during printing (for testing purpose).", NULL, FALSE);
   help_add_option (help, NULL, "with-annotate",  "Adds instruction annotations during printing (for testing purpose).", NULL, FALSE);
   help_add_option (help, NULL, "with-roles",     "Adds instruction roles during printing (for testing purpose).", NULL, FALSE);
   help_add_option (help, NULL, "with-isets",     "Adds instruction sets during printing (for testing purpose).", NULL, FALSE);
   help_add_option (help, NULL, "with-debug",     "Prints debug informations from the file (if available and retrieved).", NULL, FALSE);
   help_add_option (help, NULL, "no-debug",       "Does not attempt to retrieve debug informations from the file.", NULL, FALSE);

   // Binary format data
   help_add_separator (help, "Binary file data");
   help_add_option (help, "e", "printelf",        "Prints ELF structures. Filters can be used to print only a part of ELF data. If\n"
                                                  "no filters are set, all data are printed. Else, only specified data are printed.", NULL, FALSE);
   help_add_option (help, NULL, "elfhdr",         "Prints ELF header.", NULL, FALSE);
   help_add_option (help, NULL, "elfscn",         "Prints ELF section headers.", NULL, FALSE);
   help_add_option (help, NULL, "elfseg",         "Prints ELF program headers.", NULL, FALSE);
   help_add_option (help, NULL, "elfrel",         "Prints ELF relocation tables.", NULL, FALSE);
   help_add_option (help, NULL, "elfdyn",         "Prints ELF dynamic tables.", NULL, FALSE);
   help_add_option (help, NULL, "elfsym",         "Prints ELF symbol tables.", NULL, FALSE);
   help_add_option (help, NULL, "elfver",         "Prints ELF version tables.", NULL, FALSE);
   help_add_option (help, NULL, "elf-code-areas", "Prints the start, length and stop of consecutive sections containing executable code\n"
                                                  "in the file. Mainly for helping use of raw-disass.", NULL, FALSE);
   help_add_option (help, NULL, "get-external-fct", "Gets external functions using ELF data.", NULL, FALSE);
   help_add_option (help, NULL, "get-dynamic-lib", "Gets dynamic libraries using ELF data.", NULL, FALSE);

   help_add_option (help, NULL, "count-insns",    "Prints the number of instructions in the file.", NULL, FALSE);
   help_add_option (help, NULL, "print-insn-sets","Prints the instructions sets present in the file.", NULL, FALSE);

   //Assembly options
   help_add_separator (help, "Assembling");
   help_add_option (help, "a", "assemble-insn",	  "Assembles the instruction and prints the corresponding binary code (in hexadecimal)\n"
                                                   "<arch> is the architecture to use for assembling\n"
                                                   "In this cas, <filename> is interpreted as an assembly instruction (written in AT&T format for Intel architecture)", "<arch>", FALSE);
   help_add_option (help, "A", "assemble-file",	  "Assembles the instructions found in <filename> and prints the corresponding binary code (in hexadecimal)\n"
                                                   "<arch> is the architecture to use for assembling", "<arch>", FALSE);

   // Patching options
   help_add_separator (help, "Patching");
   help_add_option (help, NULL, "function",       "Inserts a function call. The function does not have any parameters.\n"
                                                  "<format> is a quote-delimited string containing parameters used to insert the function.\n"
                                                  "It has the following structure:\n"
                                                  "<fct name>;[@<address>[@<address>...]][;<library>][;<after|before>][;<wrap|no-wrap>]\n"
                                                  "<fct name> is the name of the function to insert,\n"
                                                  "<address> is where to put the function. If not specified, the function is inserted\n"
                                                  "but not called, <library> is a dynamic library containing the function. If not specified,\n"
                                                  "it is assumed that <function name> is an internal function and a call is added to the\n"
                                                  "function, <after|before> can be used to choose if the call is done at the instruction\n"
                                                  "before or after <address>. <before> is the default choice, <wrap-no-wrap> can be used\n"
                                                  "to choose if the context must be save before the function call. <wrap> is the default\n"
                                                  "choice. The stack saving behavior can be set used --stack-... options. --stack-shift=512\n"
                                                  "is the default behaviour.", "<format>", FALSE);
   help_add_option (help, NULL, "delete",         "Deletes one or several instructions. The <format> parameter has the following\n"
                                                  "structure:\n"
                                                  "@<address>[@<address>...][;<number>]\n"
                                                  "<address> is where to delete instructions,\n"
                                                  "<number> is the number of instruction to delete. If not specified, the default\n"
                                                  "value is 1. <number> must be a positive value.", "<format>", FALSE);
   help_add_option (help, NULL, "stack-keep",     "Sets the method for safeguarding the stack to STACK_KEEP (original stack is kept).", NULL, FALSE);
   help_add_option (help, NULL, "stack-move",     "Sets the method for safeguarding the stack to STACK_MOVE (stack is moved to new\n"
                                                  "location).", NULL, FALSE);
   help_add_option (help, NULL, "stack-shift",    "Sets the method for safeguarding the stack to STACK_SHIFT (stack is shifted from\n"
                                                  "<value>).", "<value>", FALSE);
   help_add_option (help, NULL, "set-machine",    "For ELF binaries, changes the machine data by <value> in the ELF header.", "<value>", FALSE);
   help_add_option (help, NULL, "rename-library", "Rename an external library referenced in the binary.\n"
                                                   "<format> is a quote-delimited with the following structure:\n"
                                                   "<oldname>;<newname>\n"
                                                   "where <oldname> is the name of an existing library referenced in the binary,\n"
                                                   "and <newname> is the name with which it must be replaced.", "<format>", FALSE);

   // Other options
   help_add_separator (help, "Other");
   help_add_option (help, NULL, "check-file",     "Check the binary is valid.", NULL, FALSE);
   help_add_option (help, "o", "output",          "Saves the file in <output>. If no patching command has been issued, the new\n"
                                                  "file will be identical. If omitted while a patching command has been issued,\n"
                                                  "the result file will be <filename>_mdrs.", "<output>", FALSE);
   help_add_option (help, "m", "mute",            "Disassembles but does not print anything.", NULL, FALSE);
   help_add_option (help, "h", "help",            "Prints this message.", NULL, FALSE);
   help_add_option (help, "v", "version",         "Displays the module version.", NULL, FALSE);

   help_add_example (help, "maqao madras -d <binary> --debug-print",
                           "Disassemble <binary> and print debug data.");
   help_add_example (help, "maqao madras <binary> --function=foo;@0x400000;libfoo.so",
                           "Insert function foo from libfoo.so at address 0x400000.");
   help_add_example (help, "maqao madras -a <arch> <instruction>",
                           "Assembles <instruction> using architecture <arch> and print its binary code.");
   return (help);
}
