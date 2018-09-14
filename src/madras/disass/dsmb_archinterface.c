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
 * \file dsmb_archinterface.c
 * \brief This file contains the functions used to register architecture-specific functions
 *
 * \date 6 sept. 2011
 */
#include "dsmb_archinterface.h"

/*Declaring the architecture-specific functions prototypes*/

/**
 * Loads the members of a driver depending on the architecture
 * \param D The driver
 * \param A The architecture
 * */
#define LOAD_ARCH_DRIVER(D,A) \
   DBGMSG("Loading driver for architecture %s\n", MACRO_TOSTRING(A));\
    D->fsmloader = MACRO_CONCAT(A,_fsmloader);\
    D->getarch = MACRO_CONCAT(A,_getarch);\
    D->switchfsm = MACRO_CONCAT(A,_switchfsm);

/**
 * Defining the macro used in dsmb_supportedarchs.h as a declaration of function prototype
 * \param A Architecture identifier
 * */
#define ARCH_DISASS(A) \
    extern void MACRO_CONCAT(A,_fsmloader)(fsmload_t* fc);\
    extern arch_t* MACRO_CONCAT(A,_getarch)();\
    extern int MACRO_CONCAT(A, _switchfsm) (asmfile_t*, int64_t, int64_t*, list_t**);
/*Including this file here will write the declaration for every architecture listed in it*/
#include "dsmb_supportedarchs.h"

#undef ARCH_DISASS

/**
 * Defining the macro used in dsmb_supportedarchs.h as a case in a switch statement registering
 * the architecture-specific functions
 * \param A Architecture identifier
 * */
#define ARCH_DISASS(A) case(MACRO_CONCAT(ARCH_,A)):\
    LOAD_ARCH_DRIVER(d, A)\
	break;

/*
 * Creates a new disassembly driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param arch The structure representing the architecture the disassembler driver of which we have to load
 * \return The new driver for the given architecture
 * */
dsmbldriver_t *dsmbldriver_load(arch_t* arch)
{
   /**\todo (2014-07-08) If in the future, this driver is part of the arch_t structure, this function could simply
    * return a pointer to it (and dsmbldriver_free would do nothing)*/
   /**\todo (2015-08-06) This function could invoke dsmbldriver_load_byarchcode,
    * but we would lose the information of the architecture name*/
   dsmbldriver_t *d = NULL;
   d = (dsmbldriver_t*) lc_malloc(sizeof(dsmbldriver_t));
   char archcode = arch_get_code(arch);
   switch (archcode) {

   /*Including this file here will write case statements for each architecture defined in the file*/
#include "dsmb_supportedarchs.h"

   default:
      lc_free(d);
      d = NULL;
      ERRMSG(
            "Architecture %s is not recognized or not supported for disassembly.\n",
            arch_get_name(arch))
      ;
   }
   return d;
}

/*
 * Creates a new disassembly driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param archcode The code of the architecture we have to load (as defined in arch.h)
 * \return The new driver for the given architecture
 * */
dsmbldriver_t *dsmbldriver_load_byarchcode(arch_code_t archcode)
{
   /**\todo (2014-07-08) If in the future, this driver is part of the arch_t structure, this function could simply
    * return a pointer to it (and dsmbldriver_free would do nothing)*/
   dsmbldriver_t *d = NULL;
   d = (dsmbldriver_t*) lc_malloc(sizeof(dsmbldriver_t));
   switch (archcode) {

   /*Including this file here will write case statements for each architecture defined in the file*/
#include "dsmb_supportedarchs.h"

   default:
      lc_free(d);
      d = NULL;
      ERRMSG(
            "Architecture %d is not recognized or not supported for disassembly.\n",
            archcode)
      ;
   }
   return d;
}

#undef ARCH_DISASS

/**
 * Defining the macro used in dsmb_supportedarchs.h as a test over the architecture name for registering
 * the architecture-specific functions
 * \param A Architecture identifier
 * */
#define ARCH_DISASS(A) if(str_equal(#A, archname)) {\
    d = (dsmbldriver_t*)lc_malloc(sizeof(dsmbldriver_t));\
    LOAD_ARCH_DRIVER(d,A)\
    return d;\
}

/*
 * Creates a new driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param archname The name of the architecture we have to load
 * \return The new driver for the given architecture
 * */
dsmbldriver_t *dsmbldriver_load_byarchname(char* archname)
{
   /**\todo (2014-07-08) If in the future, this driver is part of the arch_t structure, this function could simply
    * return a pointer to it (and dsmbldriver_free would do nothing)*/
   dsmbldriver_t *d = NULL;

   /*Including this file here will write case statements for each architecture defined in the file*/
#include "dsmb_supportedarchs.h"

   ERRMSG("Unrecognized or unsupported architecture %s\n", archname);
   return d;
}

#undef ARCH_DISASS

/*
 * Frees a driver and closes its link to the relevant library file
 * \param d The driver to free
 * */
void dsmbldriver_free(dsmbldriver_t *d)
{
   lc_free(d);
}
