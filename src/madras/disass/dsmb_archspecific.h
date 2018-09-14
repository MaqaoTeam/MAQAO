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
 * \file dsmb_archspecific.h
 * \brief This file contains the template for the architecture-specific functions used to load the disassembler states
 *
 * \date 5 sept. 2011
 */

#ifdef CURRENT_ARCH

#define ARCHPREFIX(X) MACROVAL_CONCAT(CURRENT_ARCH,X) /**<Concatenation of the architecture prefix with a name*/

/**
 * Architecture-specific loader of the final functions to be called after a successful disassembly
 * */
extern extfct_t* ARCHPREFIX(_getextfcts)();

/**
 * Architecture-specific loader of the FSM. It loads the state table for the disassembler FSM for this architecture
 * \param fc The FSM context
 * */
void ARCHPREFIX(_fsmloader)(fsmload_t* fl) {
   fl->n_variables = BDFVar__NUMBER;
   fl->states = ARCHPREFIX(_states);
   /**\bug [To be checked] There is a potential error here, apparently in some cases fc->states loses the value after leaving the function
    * => Never occurred since, I guess it was a side effect of some other dumb error I made elsewhere at the time*/
   fl->finalfcts = ARCHPREFIX(_getextfcts)();
   fl->insn_maxlen = ARCHPREFIX(_maxinsnlen);
   fl->insn_minlen = ARCHPREFIX(_mininsnlen);
#ifndef NDEBUG
   fl->varnames = ARCHPREFIX(__BDFVar_names);
#endif
}

/**
 * Architecture-specific getter of the arch_t structure for a given architecture
 * \return A pointer to the arch_t structure defining the given architecture
 * */
arch_t* ARCHPREFIX(_getarch)() {
   return &ARCHPREFIX(_arch);
}

/**
 * Architecture-specific function checking for a fsm switch
 */
extern int ARCHPREFIX(_switchfsm) (asmfile_t*, int64_t, int64_t*, list_t**);

#undef ARCHPREFIX

#endif
