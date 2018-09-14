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
 * archinterface.c
 *
 *  Created on: 28 avr. 2014
 */
#include "libmcommon.h"
#include "arch.h"
#include "archinterface.h"
/*Including headers for binary formats*/
#include "libmtroll.h"
#include "libstone.h"
#include "machine.h"
//Add header files for other formats here

/**
 * Macro writing the name of the arch_t structure for a given architecture
 * \param ARCH The architecture
 * */
#define ARCH_NAME(ARCH) MACROVAL_CONCAT(ARCH, _arch)

/**
 * Macro writing the name of the pointer to the arch_t structure for a given architecture
 * \param ARCH The architecture
 * */
#define ARCH_PTRNAME(ARCH) &(ARCH_NAME(ARCH))

/*Including headers for architectures*/
#define ARCH_AND_CODES(ARCH, ...) extern arch_t ARCH_NAME(ARCH); //Defining the macro used in supportedarchs.h to declare arch_t objects
#include "supportedarchs.h" //Including this here means the arch-specific architecture structures will be declared depending on the compilation options chosen
#undef ARCH_AND_CODES

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Declaring some overly complicated macros for retrieving an architecture name from its codes as defined in supportedarchs.h
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**Macro retrieving the first argument of a macro*/
#define GET_MACROARG_0(_0, ...) _0
/**Macro retrieving the second argument of a macro*/
#define GET_MACROARG_1(_0, _1, ...) _1
/**Macro retrieving the third argument of a macro*/
#define GET_MACROARG_2(_0, _1, _2, ...) _2
/**Macro retrieving the fourth argument of a macro*/
#define GET_MACROARG_3(_0, _1, _2, _3, ...) _3
/**Macro retrieving the fifth argument of a macro*/
#define GET_MACROARG_4(_0, _1, _2, _3, _4, ...) _4
/**Macro retrieving the sixth argument of a macro*/
#define GET_MACROARG_5(_0, _1, _2, _3, _4, _5, ...) _5
/**Macro retrieving the seventh argument of a macro*/
#define GET_MACROARG_6(_0, _1, _2, _3, _4, _5, _6, ...) _6
/**Macro retrieving the eighth argument of a macro*/
#define GET_MACROARG_7(_0, _1, _2, _3, _4, _5, _6, _7, ...) _7
// To support macros with more arguments, add macros on the same formats

/**
 * Macro retrieving the nth argument of a macro
 * \param N Rank of the argument to retrieve (starting with 0)
 * \param ... Arguments of the macro
 * \return The Nth argument
 * */
#ifdef _WIN32
/* In order to avoir rude talking i do not explain here why Windows does not expand the variadic arguments properly...
 But i can tell you that the following macros are here to do the job for this non C compliant shitty operating system
 */
#define GET_MACROARG_STUB(N, tuple) GET_MACROARG_##N tuple 
#define GET_MACROARG(N, ...) GET_MACROARG_STUB(N, (__VA_ARGS__, , , , , , , , ))
#else
#define GET_MACROARG(N, ...) GET_MACROARG_##N(__VA_ARGS__, , , , , , , , )
#endif
//To support macros with more arguments, add a "," at the end. There must be at least as much ',' as the max number of GET_MACROARG_x

/**
 * Macro writing a switch statement returning an architecture structure depending on a code
 * \param CODE The code representing the architecture under a given format
 * \param ARCH The name of the architecture
 * */
#define _WRITE_CASE(CODE, ARCH) case CODE: return ARCH_PTRNAME(ARCH);break;

/**
 * Macro disabling the writing of a switch statement if the architecture code is not defined for this binary format
 * \param CODE The code representing the architecture under a given format
 * \param ARCH The name of the architecture
 * */
#define WRITECASE_NO(CODE, ARCH)

/**
 * Macro enabling the writing of a switch statement if the architecture code is not defined for this binary format
 * \param CODE The code representing the architecture under a given format
 * \param ARCH The name of the architecture
 * */
#define WRITECASE_YES(CODE, ARCH) _WRITE_CASE(CODE, ARCH)

/**
 * Macro used for turning an empty argument into an additional argument
 * */
#define EMPTYARG(...) ,

/**
 * Macro detecting if a list of arguments contains an architecture code
 * \return YES if it contains an architecture code, NO otherwise (used for concatenation to get the name of the correct macro)
 * */
#define CODE_ISDEFINED(...) GET_MACROARG(4, __VA_ARGS__, NO, YES)

/**
 * Writes a switch/case statement or not
 * \param CODE Code of a binary format
 * \param ARCH Code of an arch in this format
 * \param HASCODE YES if ARCH exists, NO otherwise
 * */
#define WRITECASE_IFHASCODE(CODE, ARCH, HASCODE) WRITECASE_##HASCODE(CODE, ARCH)

/**
 * Writes a switch/case statement or not (used to be able to concatenate the correct suffix to the macro name)
 * \param CODE Code of a binary format
 * \param ARCH Code of an arch in this format
 * \param HASCODE YES if ARCH exists, NO otherwise
 * */
#define WRITECASE_IFCODE(CODE, ARCH, HASCODE) WRITECASE_IFHASCODE(CODE, ARCH, HASCODE)

/**
 * Writes a switch/case statement for a given code representing an architecture under a given binary format
 * \param CODE Code of a binary format
 * \param ARCH Code of an architecture in this format. If empty, nothing will be written
 * */
#define WRITECASE(CODE, ARCH) WRITECASE_IFCODE( CODE, ARCH, CODE_ISDEFINED(EMPTYARG CODE (), CODE, ARCH) )
//#define WRITECASE(CODE, ARCH) WRITECASE_IFCODE( CODE, ARCH, CODE_ISDEFINED(CODE, ARCH) )

/**
 * Retrieves the argument corresponding to the binary code of a given format in the list of arguments
 * \param POS The position (using the *_POS macros defined in supportedarchs.h)
 * \param ... Arguments of ARCH_AND_CODES
 * */
#define GET_BINCODE_FROM_ARGS(POS, ...) GET_MACROARG(POS, __VA_ARGS__)

/**
 * Retrieves the argument corresponding to the architecture name in the list of arguments
 * \param ... Arguments of ARCH_AND_CODES
 * */
#define GET_ARCH_FROM_ARGS(...) GET_MACROARG(0, __VA_ARGS__)

/**
 * Writes a case statement for matching a code in a given binary format to an architecture
 * \param BINCODE Code of the binary format (ELF, WINPE, MACHO). Use the same name as everywhere else (libmbinfile.h, supportedarchs.h) to make things easier
 * \param ... Arguments to ARCH_AND_CODES
 * */
#define WRITE_CASE_FOR_BINCODE(BINCODE, ...) WRITECASE(GET_BINCODE_FROM_ARGS(BINCODE##_POS, __VA_ARGS__), GET_ARCH_FROM_ARGS(__VA_ARGS__))

////////////////////////////////////////////////////////
// End of macro nightmare. Human readable code is below
////////////////////////////////////////////////////////

/*
 * NULL-terminated array of pointers to the structures describing the architectures supported by MAQAO
 * */
arch_t* MAQAO_archs[] = {
#define ARCH_AND_CODES(ARCH, ...) ARCH_PTRNAME(ARCH),
#include "supportedarchs.h"
#undef ARCH_AND_CODES
      NULL
};

/*
 * Returns the architecture structure corresponding to a given code for a given binary format
 * \param bincode Identifier of the binary format (as defined in libmasm.h)
 * \param archcode Code of the architecture in this format (as defined in the relevant headers and referenced in supportedarchs.h)
 * \return A pointer to the structure representing the architecture, or NULL if the code or binary format is unknown
 * */
arch_t* getarch_bybincode(bf_format_t bincode, uint16_t archcode)
{
   /**
    * \note This function uses the list of macros declared in supportedarchs.h to generate code automatically.
    * For each binary format, the macro ARCH_AND_CODES is defined as something that writes a case statement
    * returning a pointer to the arch_t structure corresponding to the architecture associated with the code
    * representing it for the given binary format. We then include supportedarchs.h, which contains a list
    * of invocation of the ARCH_AND_CODES macro, which will therefore be processed as a list of case statements.
    * */
   switch (bincode) {
   case BFF_ELF: /**<ELF file format*/
      switch (archcode) {
#define ARCH_AND_CODES(...) WRITE_CASE_FOR_BINCODE(ELF, __VA_ARGS__)
#include "supportedarchs.h" //This writes a list of case statements for each architecture valid for this format and selected for compilation
#undef ARCH_AND_CODES
      default:
         ERRMSG("Unrecognized architecture code %u for the %s binary format\n",
               archcode, bf_format_getname(bincode))
         ;
         return NULL;
      }
      break;

   case BFF_WINPE: /**<Windows PE file format*/
      switch (archcode) {
#define ARCH_AND_CODES(...) WRITE_CASE_FOR_BINCODE(WINPE, __VA_ARGS__)
#include "supportedarchs.h" //This writes a list of case statements for each architecture valid for this format and selected for compilation
#undef ARCH_AND_CODES
      default:
         ERRMSG("Unrecognized architecture code %u for the %s binary format\n",
               archcode, bf_format_getname(bincode))
         ;
         return NULL;
      }
      break;

   case BFF_MACHO: /**<Mach-O file format*/
      switch (archcode) {
#define ARCH_AND_CODES(...) WRITE_CASE_FOR_BINCODE(MACHO, __VA_ARGS__)
#include "supportedarchs.h" //This writes a list of case statements for each architecture valid for this format and selected for compilation
#undef ARCH_AND_CODES
      default:
         ERRMSG("Unrecognized architecture code %u for the %s binary format\n",
               archcode, bf_format_getname(bincode))
         ;
         return NULL;
      }
      break;

   default:
      ERRMSG("Unrecognized binary format with code %u\n", bincode)
      ;
      return NULL;
   }
}

/*
 * Returns the architecture structure corresponding to a given name
 * \param archname Name of the architecture, as defined in its MINJAG binary definition file
 * \return A pointer to the structure representing the architecture, or NULL if the name is unknown
 * */
arch_t* getarch_byname(char* archname)
{
   /**
    * \note This function uses the list of macros declared in supportedarchs.h to generate code automatically.
    * For each binary format, the macro ARCH_AND_CODES is defined as something that writes a test on the architecture name
    * returning a pointer to the arch_t structure corresponding to the given name. We then include supportedarchs.h, which contains a list
    * of invocation of the ARCH_AND_CODES macro, which will therefore be processed as a list of tests.
    * */
   if (archname == NULL)
      return NULL;
   //Defining the macro as a test
#define ARCH_AND_CODES(ARCH,...) if(str_equal(#ARCH, archname)) {return ARCH_PTRNAME(ARCH);}

#include "supportedarchs.h"//This writes a list of if statements for each architecture selected for compilation

#undef ARCH_AND_CODES

   ERRMSG("Unrecognized or unsupported architecture %s\n", archname);
   return NULL;

}

/*
 * Returns the architecture structure corresponding to a given code
 * \param archcode Code of the architecture, as defined arch.h
 * \return A pointer to the structure representing the architecture, or NULL if the code is unknown
 * */
arch_t* getarch_bycode(uint16_t archcode)
{
   /**
    * \note This function uses the list of macros declared in supportedarchs.h to generate code automatically.
    * For each binary format, the macro ARCH_AND_CODES is defined as something that writes a case statement
    * returning a pointer to the arch_t structure corresponding to the architecture associated with the code
    * defined in arch.h. We then include supportedarchs.h, which contains a list
    * of invocation of the ARCH_AND_CODES macro, which will therefore be processed as a list of case statements.
    * */
   // Defining the macro as a test
#define ARCH_AND_CODES(ARCH, ...) case MACROVAL_CONCAT(ARCH_, ARCH): return ARCH_PTRNAME(ARCH);

   switch (archcode) {
#include "supportedarchs.h"//This writes a list of case statements for each architecture selected for compilation
   default:
      ERRMSG("Unknown architecture with internal code %u\n", archcode)
      ;
      return NULL;
   }
#undef ARCH_AND_CODES
}

/*
 * Returns the architecture associated to a file, with minimal parsing or disassembling
 * \param filename Path to a binary file
 * \return Structure representing the architecture for which this file is compiled, or NULL
 * if \c filename is an invalid path, if the file is not a binary file with a format supported by
 * MAQAO, or if the architecture for which it is compiled is not supported by MAQAO
 * */
arch_t* file_get_arch(char* filename)
{
   int machine_code; //Format-specific machine code
   //Disabling error messages for invoking the functions
   int current_verbose_level = __MAQAO_VERBOSE_LEVEL__;
   //__MAQAO_VERBOSE_LEVEL__ = MAQAO_VERBOSE_CRITICAL;

   //Testing if the file could be an ELF file
   machine_code = elf_get_machine_code(filename);
   if (machine_code != ELF_MACHINE_CODE_ERR) {
      //File is an ELF file: returning the associated architecture structure, based on its code
      arch_t* arch = getarch_bybincode(BFF_ELF, machine_code);
      //Restores verbose level
      __MAQAO_VERBOSE_LEVEL__ = current_verbose_level;
      return arch;
   }  // Otherwise this is not an ELF file (or was not found: we will skip try the next supported file format

   //Add here the code for checking other file formats, using the same structure as above:
//   machine_code = get_{FILE_FORMAT}_machine_code(filename);
//   if (machine_code != {FILE_FORMAT}_MACHINE_CODE_ERR) {
//      //File is an {FILE_FORMAT} file: returning the associated architecture structure, based on its code
//      arch_t* arch = getarch_bybincode(BFF_{FILE_FORMAT}, machine_code);
//      //Restores verbose level
//      __MAQAO_VERBOSE_LEVEL__ = current_verbose_level;
//      return arch;
//   }  // Otherwise this is not an {FILE_FORMAT} file (or was not found: we will skip try the next supported file format

   return NULL;
}
