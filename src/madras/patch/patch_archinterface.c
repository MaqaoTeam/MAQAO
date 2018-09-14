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
 * \file patch_archinterface.c
 * \brief This file contains the functions used to register architecture-specific functions
 *
 * \date 20 sept. 2011
 */
#include "arch.h"
#include "patch_archinterface.h"

/*Declaring the architecture-specific functions prototypes*/

/**
 * Defining the macro used in patcher_supportedarchs.h as a declaration of function prototype
 * \param A Architecture identifier
 * */
#define ARCH_PATCH(A)  \
   extern queue_t* MACRO_CONCAT(A,_generate_insnlist_functioncall)(modif_t*, insn_t**, pointer_t*, data_t*);\
	extern queue_t* MACRO_CONCAT(A,_generate_insnlist_smalljmpaddr)(int64_t*, insn_t**, pointer_t**);\
   extern queue_t* MACRO_CONCAT(A,_generate_insnlist_jmpaddr)(int64_t*,insn_t**,pointer_t**);\
	/*extern queue_t* MACRO_CONCAT(A,_generate_insn_jmpaddr)(int* i,int64_t* ia,insn_t** ins);*/\
   extern insn_t* MACRO_CONCAT(A,_generate_insn_nop)(unsigned int l);\
   extern void MACRO_CONCAT(A,_upd_dataref_coding)(void* v,int64_t ia,void* vz, int a, int b);\
	/*extern queue_t* MACRO_CONCAT(A,_generate_insnlist_pltcall)(int s,insn_t* ins,insn_t** inr,insn_t** inrr);*/\
   extern int MACRO_CONCAT(A,_instruction_is_nop)(insn_t* ins);\
   extern oprnd_t* MACRO_CONCAT(A,_generate_oprnd_globvar)(int type);\
   extern insn_t* MACRO_CONCAT(A,_generate_opposite_branch)(insn_t* in, oprnd_t** op, int64_t* val, char* condtype);\
   extern void MACRO_CONCAT(A,_add_conditions_to_insnlist)(queue_t* inslist, insertconds_t* insconds,data_t*,int64_t);\
    extern queue_t* MACRO_CONCAT(A,_generate_insnlist_return)(int64_t* startaddr);\
    /**NEW ELEMENTS FROM PATCHER REFACTORING*/\
    extern queue_t* MACRO_CONCAT(A,_generate_insnlist_ripjmpaddr)(int64_t*, insn_t**, pointer_t**);\
    extern queue_t* MACRO_CONCAT(A,_generate_insnlist_indjmpaddr)(int64_t*, insn_t**, pointer_t**);\
    extern int MACRO_CONCAT(A,_smalljmp_reachaddr)(int64_t, int64_t);\
    extern int64_t MACRO_CONCAT(A,_get_smalljmp_maxdistneg)();\
    extern int64_t MACRO_CONCAT(A,_get_smalljmp_maxdistpos)();\
    extern int64_t MACRO_CONCAT(A,_get_jmp_maxdistneg)();\
    extern int64_t MACRO_CONCAT(A,_get_jmp_maxdistpos)();\
    extern int64_t MACRO_CONCAT(A,_get_relmem_maxdistneg)();\
    extern int64_t MACRO_CONCAT(A,_get_relmem_maxdistpos)();\
    extern uint16_t MACRO_CONCAT(A,_get_smalljmpsz)();\
    extern uint16_t MACRO_CONCAT(A,_get_jmpsz)();\
    extern uint16_t MACRO_CONCAT(A,_get_relmemjmpsz)();\
    extern uint16_t MACRO_CONCAT(A,_get_indjmpaddrsz)();\
    extern uint8_t MACRO_CONCAT(A,_get_addrsize)(bfwordsz_t);\
    extern uint8_t MACRO_CONCAT(A,_movedinsn_getmaxbytesize)(insn_t*);
/*Including this file here will write the declaration for every architecture listed in it*/
#include "patch_supportedarchs.h"

#undef ARCH_PATCH

/**
 * Defining the macro used in patcher_supportedarchs.h as a case in a switch statement registering
 * the architecture-specific functions
 * \param A Architecture identifier
 * */
#define ARCH_PATCH(A) case(MACRO_CONCAT(ARCH_, A)):\
   d->generate_insnlist_functioncall = MACRO_CONCAT(A,_generate_insnlist_functioncall);\
   d->generate_insnlist_smalljmpaddr = MACRO_CONCAT(A,_generate_insnlist_smalljmpaddr);\
   d->generate_insnlist_jmpaddr = MACRO_CONCAT(A,_generate_insnlist_jmpaddr);\
   d->generate_insn_nop = MACRO_CONCAT(A,_generate_insn_nop);\
   d->upd_dataref_coding = MACRO_CONCAT(A,_upd_dataref_coding);\
	/*d->generate_insnlist_pltcall = MACRO_CONCAT(A,_generate_insnlist_pltcall);*/\
   d->instruction_is_nop = MACRO_CONCAT(A,_instruction_is_nop);\
   d->generate_oprnd_globvar = MACRO_CONCAT(A,_generate_oprnd_globvar);\
   d->generate_opposite_branch = MACRO_CONCAT(A,_generate_opposite_branch);\
   d->add_conditions_to_insnlist = MACRO_CONCAT(A,_add_conditions_to_insnlist);\
   d->generate_insnlist_return = MACRO_CONCAT(A,_generate_insnlist_return);\
	/**NEW ELEMENTS FROM PATCHER REFACTORING*/\
   d->generate_insnlist_ripjmpaddr = MACRO_CONCAT(A,_generate_insnlist_ripjmpaddr);\
   d->generate_insnlist_indjmpaddr = MACRO_CONCAT(A,_generate_insnlist_indjmpaddr);\
	d->smalljmp_reachaddr = MACRO_CONCAT(A,_smalljmp_reachaddr);\
	d->get_smalljmp_maxdistneg = MACRO_CONCAT(A,_get_smalljmp_maxdistneg);\
	d->get_smalljmp_maxdistpos = MACRO_CONCAT(A,_get_smalljmp_maxdistpos);\
	d->get_jmp_maxdistneg = MACRO_CONCAT(A,_get_jmp_maxdistneg);\
	d->get_jmp_maxdistpos = MACRO_CONCAT(A,_get_jmp_maxdistpos);\
	d->get_relmem_maxdistneg = MACRO_CONCAT(A,_get_relmem_maxdistneg);\
	d->get_relmem_maxdistpos = MACRO_CONCAT(A,_get_relmem_maxdistpos);\
	d->get_smalljmpsz = MACRO_CONCAT(A,_get_smalljmpsz);\
	d->get_jmpsz = MACRO_CONCAT(A,_get_jmpsz);\
	d->get_relmemjmpsz = MACRO_CONCAT(A,_get_relmemjmpsz);\
	d->get_indjmpaddrsz = MACRO_CONCAT(A,_get_indjmpaddrsz);\
	d->get_addrsize = MACRO_CONCAT(A,_get_addrsize);\
   d->movedinsn_getmaxbytesize = MACRO_CONCAT(A,_movedinsn_getmaxbytesize);\
	break;

/*
 * Creates a new driver structure and initializes its function pointers from the library
 * relevant to the given architecture
 * \param arch The structure representing the architecture we have to load (as present in an arch_t structure)
 * \return The new driver for the given architecture
 * */
patchdriver_t *patchdriver_load(arch_t* arch)
{
   /**\todo Streamline this*/
   patchdriver_t *d = (patchdriver_t*) lc_malloc(sizeof(patchdriver_t));

   char archcode = arch_get_code(arch);
   switch (archcode) {

   /*Including this file here will write case statements for each architecture defined in the file*/
#include "patch_supportedarchs.h"

   default:
      lc_free(d);
      d = NULL;
      ERRMSG("Architecture %s not recognised or not supported for patching.\n",
            arch_get_name(arch))
      ;
   }
   return d;
}
#undef ARCH_PATCH

/*
 * Frees a driver and closes its link to the relevant library file
 * \param d The driver to free
 * */
void patchdriver_free(patchdriver_t *d)
{
   lc_free(d);
}
