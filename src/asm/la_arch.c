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
 * \file la_arch.c
 * */
#include "libmasm.h"
#include "arch.h"

///////////////////////////////////////////////////////////////////////////////
//                              	    arch                                    //
///////////////////////////////////////////////////////////////////////////////
/*
 * Gets the usual name of a register represented by its codes
 * \param a an architecture
 * \param t a register type
 * \param n a register name
 * \return a string which is the usual register name
 */
char* arch_get_reg_name(arch_t* a, char t, char n)
{
   if (t == RIP_TYPE)
      return arch_get_reg_rip_name(a);
   if (a == NULL || a->reg_names == NULL || t > a->nb_type_registers
         || n > a->nb_names_registers || n < 0 || t < 0)
      return PTR_ERROR;
   if (a->reg_names[(int) t][(int) n] != NULL)
      return a->reg_names[(int) t][(int) n];
   else
      return BAD_INSN;
}

/*
 * Gets the name of the register used to represent the instruction pointer
 * \param a An architecture
 * \return The name of the register representing the instruction pointer for this architecture, R_RIP if no such
 * name was defined for this architecture, and NULL if a is NULL
 * */
char* arch_get_reg_rip_name(arch_t* a)
{
   return (a) ? ((a->reg_rip_name) ? a->reg_rip_name : R_RIP) : NULL;
}

/*
 * Gets an opcode name
 * \param a an architecture
 * \param o an opcode code
 * \return a string with the opcode name
 */
char* arch_get_opcode_name(arch_t* a, short o)
{
   if (a == NULL || o == BAD_INSN_CODE || o > a->size_opcodes
         || a->opcodes == NULL || o < 0)
      return BAD_INSN;
   if (a->opcodes[(int) o] != NULL)
      return a->opcodes[(int) o];
   else
      return BAD_INSN;
}

/*
 * Gets an prefix or suffix name
 * \param a an architecture
 * \param c a code
 * \return a string with the name
 */
char* arch_get_prefsuff_name(arch_t* a, short c)
{
   if (a == NULL || c > a->size_pref_suff || a->pref_suff == NULL || c < 0)
      return PTR_ERROR;
   return a->pref_suff[c];
}

/*
 * Gets the simd flag of a specified instruction variant
 * \param a an architecture
 * \param iv an instruction variant
 * \return the simd flag associated to the instruction variant, or UNSIGNED_ERROR if there is a problem
 */
unsigned short arch_insnvariant_is_simd(arch_t* a, uint32_t iv)
{
   if (a == NULL || iv > a->nb_insnvariants)
      return UNSIGNED_ERROR;

   return a->variants_simd[iv];
}

/*
 * Gets an instruction family
 * \param a an architecture
 * \param c an opcode code
 * \return the family associated to the opcode, or FM_UNDEF if there is a problem
 */
unsigned short arch_get_family(arch_t* a, short c)
{
   if (a == NULL || c > a->size_opcodes || a->families == NULL)
      return FM_UNDEF;

   if (c >= 0)
      return a->families[c];
   else
      return FM_UNDEF;
}

/*
 * Gets an instruction class
 * \param a an architecture
 * \param c an opcode code
 * \return the class associated to the opcode, or UNSIGNED_ERROR if there is a problem
 */
unsigned short arch_get_class(arch_t* a, short c)
{
   if (a == NULL || c > a->size_opcodes || a->families == NULL)
      return UNSIGNED_ERROR;

   if (c >= 0)
      return ((a->families[c] & (unsigned short) 0xF000) >> 12);
   else
      return UNSIGNED_ERROR;
}

/*
 * Gets the endianness of an architecture
 * \param a en architecture
 * \return the endianness of an architecture
 */
unsigned int arch_get_endianness(arch_t* a)
{
   return (a) ? a->endianness : UNSIGNED_ERROR;
}

/*
 * Gets an architecture's name
 * \param a an architecture
 * \return the name in string form of the architecture, or PTR_ERROR if there is a problem
 */
char* arch_get_name(arch_t* a)
{
   return ((a != NULL) ? a->name : PTR_ERROR);
}

/*
 * Gets a register family
 * \param a A structure describing an architecture
 * \param t The type of the register
 * \return The identifier of the register family (those are defined in the appropriate <archname>_arch.h header)
 * or SIGNED_ERROR if the register type is not valid (or RIP_TYPE) or if \c a is NULL
 * */
char arch_get_reg_family(arch_t* a, short t)
{
   return (
         (a != NULL && t >= 0 && t < a->nb_type_registers) ?
               a->reg_families[t] : SIGNED_ERROR);
}

/*
 * Gets a register type's size
 * \param a A structure describing an architecture
 * \param t The type of the register
 * \return The size of registers of this type or SIGNED_ERROR if the register type is not valid (or RIP_TYPE) or if \c a is NULL
 * */
char arch_get_reg_size(arch_t* a, short t)
{
   return (
         (a != NULL && t >= 0 && t < a->nb_type_registers) ?
               a->reg_sizes[t] : SIGNED_ERROR);
}

/*
 * Retrieves the code associated to an architecture
 * \param a A structure describing an architecture
 * \return The code associated to this architecture as defined in arch.h, or ARCH_NONE if a is NULL
 * */
char arch_get_code(arch_t* a)
{
   return (a) ? a->code : ARCH_NONE;
}

/*
 * Retrieves the number of instruction sets for this architecture
 * \param a A structure describing an architecture
 * \return The number of instruction sets, or 0 if \c a is NULL
 * */
unsigned int arch_get_nb_isets(arch_t* a)
{
   return (a) ? a->nb_isets : 0;
}

/*
 * Retrieves the name of an instruction set
 * \param a A structure describing an architecture
 * \param iset The code representing an instruction set for this architecture
 * \return The name of the given instruction set, or NULL if \c a is NULL or \c iset is out of the range of valid instruction sets codes
 * */
char* arch_get_iset_name(arch_t* a, unsigned int iset)
{
   return ((a && iset < a->nb_isets) ? a->iset_names[iset] : NULL);
}

/*
 * Retrieves the function used to free an instruction
 * \param a A structure describing an architecture
 * \return pointer to the function
 */
insn_free_fct_t arch_get_insn_free(arch_t* a)
{
   return (a) ? a->insn_free : PTR_ERROR;
}

/**
 * Returns the array of micro-architectures associated to this architecture
 * @param a The architecture
 * @return Array of associated micro-architectures or NULL if \c a is NULL
 */
uarch_t** arch_get_uarchs(arch_t* a)
{
   return (a && a->arch_specs) ? a->arch_specs->uarchs : PTR_ERROR;
}

/**
 * Returns the number of micro-architectures associated to this architecture
 * @param a The architecture
 * @return Number of associated micro-architectures or 0 if \c a is NULL
 */
uint16_t arch_get_nb_uarchs(arch_t* a)
{
   return (a && a->arch_specs) ? a->arch_specs->nb_uarchs : 0;
}

/**
 * Returns the array of processor versions associated to this architecture
 * @param a The architecture
 * @return Array of associated processor versions or NULL if \c a is NULL
 */
proc_t** arch_get_procs(arch_t* a)
{
   return (a && a->arch_specs) ? a->arch_specs->procs : PTR_ERROR;
}

/**
 * Returns the number of processor versions associated to this architecture
 * @param a The architecture
 * @return Number of associated processor versions or NULL if \c a is NULL
 */
uint16_t arch_get_nb_procs(arch_t* a)
{
   return (a && a->arch_specs) ? a->arch_specs->nb_procs : 0;
}

/*
 * Returns a micro-architecture with the given code
 * \param arch The architecture
 * \param uarch_id The code of the micro-architecture
 * \return The micro-architecture or NULL if arch is NULL or uarch_code is not a valid micro-architecture identifier for this architecture
 * */
uarch_t* arch_get_uarch_by_id(arch_t* arch, uint16_t uarch_id)
{
   if (arch == NULL)
      return NULL;
   if (uarch_id < arch->arch_specs->nb_uarchs)
      return arch->arch_specs->uarchs[uarch_id];
   else
      return NULL;
}

/*
 * Returns a micro-architecture with the given name
 * \param arch The architecture
 * \param uarch_name The name of the micro-architecture
 * \return The micro-architecture or NULL if arch or is NULL or uarch_name is NULL or not a valid micro-architecture name for this architecture
 * */
uarch_t* arch_get_uarch_by_name(arch_t* arch, char* uarch_name)
{
   if (arch == NULL || uarch_name == NULL)
      return NULL;
   uint16_t i;
   for (i = 0; i < arch->arch_specs->nb_uarchs; i++) {
      if (str_equal(uarch_name, uarch_get_name(arch->arch_specs->uarchs[i])) || str_equal(uarch_name, uarch_get_alias(arch->arch_specs->uarchs[i])))
            return arch->arch_specs->uarchs[i];
   }
   return NULL;
}

/*
 * Returns a processor version with the given code
 * \param arch The architecture
 * \param proc_id The code of the processor version
 * \return The processor version or NULL if arch is NULL or proc_code is not a valid processor identifier for this architecture
 * */
proc_t* arch_get_proc_by_id(arch_t* arch, uint16_t proc_id)
{
   if (arch == NULL)
      return NULL;
   if (proc_id < arch->arch_specs->nb_procs)
      return arch->arch_specs->procs[proc_id];
   else
      return NULL;
}

/*
 * Returns a processor version with the given name
 * \param arch The architecture
 * \param proc_name The code of the processor version
 * \return The processor version or NULL if arch is NULL or proc_name is NULL or not a valid processor name for this architecture
 * */
proc_t* arch_get_proc_by_name(arch_t* arch, char* proc_name)
{
   if (arch == NULL || proc_name == NULL)
      return NULL;
   uint16_t i;
   for (i = 0; i < arch->arch_specs->nb_procs; i++) {
      if (str_equal(proc_name, proc_get_name(arch->arch_specs->procs[i])))
            return arch->arch_specs->procs[i];
   }
   return NULL;

}

/*
 * Returns the default processor to consider for a given micro-architecture
 * \param arch The architecture
 * \param uarch The micro-architecture
 * \return The default processor version to consider for this architecture
 * */
proc_t* arch_get_uarch_default_proc(arch_t* arch, uarch_t* uarch)
{
   if (arch == NULL || uarch == NULL)
      return NULL;
   //First, tries to invoke the arch-specific function
   if (arch->arch_specs->uarch_get_default_proc != NULL)
      return arch->arch_specs->uarch_get_default_proc(uarch);
   else if (uarch_get_nb_procs(uarch) > 0)   //Otherwise, returns the first processor associated to this micro-architecture, if it exists
      return uarch_get_procs(uarch)[0];
   else
      return NULL;
}

/*
 * Returns an array of processors containing a given instruction set
 * \param arch The architecture
 * \param iset The instruction set
 * \param nb_procs Return parameter, will contain the size of the returned array
 * \return An array of structures describing a processor version, or NULL if \c arch or nb_procs is NULL or if the no
 * processor in this architecture contains the given instruction set. This array must be freed using lc_free
 * */
proc_t** arch_get_procs_from_iset(arch_t* arch, int16_t iset, uint16_t* nb_procs)
{
   if (arch == NULL || nb_procs == NULL)
      return NULL;
   uint16_t i;
   proc_t** procs_tab = NULL;
   for (i = 0; i < arch->arch_specs->nb_procs; i++) {
      if (arch->arch_specs->procs[i] != NULL) {
         uint16_t j;
         for (j = 0; j < arch->arch_specs->procs[i]->nb_isets; j++) {
            if (iset == arch->arch_specs->procs[i]->isets[j]) {
               ADD_CELL_TO_ARRAY(procs_tab, *nb_procs, arch->arch_specs->procs[i]);
               break;
            }
         }
      }
   }
   return procs_tab;
}

/*
 * Returns an array of micro-architectures containing at least one processor version containing a given instruction set
 * \param arch The architecture
 * \param iset The instruction set
 * \param nb_uarchs Return parameter, will contain the size of the returned array
 * \return An array of structures describing a micro-architecture, or NULL if \c arch or nb_uarchs is NULL or if the no
 * processor in this architecture contains the given instruction set
 * */
uarch_t** arch_get_uarchs_from_iset(arch_t* arch, int16_t iset, uint16_t* nb_uarchs)
{
   if (arch == NULL || nb_uarchs == NULL)
      return NULL;
   uint16_t i;
   uarch_t** uarchs_tab = NULL;
   for (i = 0; i < arch->arch_specs->nb_uarchs; i++) {
      if (arch->arch_specs->uarchs[i] != NULL) {
         proc_t* proc = NULL;
         uint16_t j;
         for (j = 0; j < arch->arch_specs->uarchs[i]->nb_procs; j++) {
            if (arch->arch_specs->uarchs[i]->procs[j] != NULL) {
               uint16_t k;
               for (k = 0; k < arch->arch_specs->uarchs[i]->procs[j]->nb_isets; k++) {
                  if (iset == arch->arch_specs->uarchs[i]->procs[j]->isets[k]) {
                     proc = arch->arch_specs->uarchs[i]->procs[j];
                     break;
                  }
               }
            }
            if (proc != NULL)
               break;
         }
         if (proc != NULL) {
            ADD_CELL_TO_ARRAY(uarchs_tab, *nb_uarchs, arch->arch_specs->uarchs[i]);
            continue;   //No need to keep scanning for other processors in this micro-architecture
         }
      }
   }
   return uarchs_tab;
}

///////////////////////////////////////////////////////////////////////////////
//                                   uarch                                   //
///////////////////////////////////////////////////////////////////////////////

/*
 * Returns the architecture for which a micro-architecture is defined
 * \param uarch The micro-architecture
 * \param The associated architecture or NULL if \c uarch is NULL
 * */
arch_t* uarch_get_arch(uarch_t* uarch)
{
   return (uarch != NULL)?uarch->arch:NULL;
}

/*
 * Returns the display name of a micro-architecture
 * \param uarch The micro-architecture
 * \param The display name of the micro-architecture or NULL if \c uarch is NULL
 * */
char* uarch_get_display_name(uarch_t* uarch)
{
   return (uarch != NULL)?uarch->display_name:NULL;
}

/*
 * Returns the name of a micro-architecture
 * \param uarch The micro-architecture
 * \param The name of the micro-architecture or NULL if \c uarch is NULL
 * */
char* uarch_get_name(uarch_t* uarch)
{
   return (uarch != NULL)?uarch->name:NULL;
}

/*
 * Returns the alias of a micro-architecture
 * \param uarch The micro-architecture
 * \param The alias of the micro-architecture or NULL if \c uarch is NULL
 * */
char* uarch_get_alias(uarch_t* uarch)
{
   return (uarch != NULL)?uarch->alias:NULL;
}

/*
 * Returns the array of processors for a micro-architecture
 * \param uarch The micro-architecture
 * \param The processors associated to this micro-architecture or NULL if \c uarch is NULL
 * */
proc_t** uarch_get_procs(uarch_t* uarch)
{
   return (uarch != NULL)?uarch->procs:NULL;
}

/*
 * Returns the number of processors for a micro-architecture
 * \param uarch The micro-architecture
 * \param The number of processors associated to this micro-architecture or 0 if \c uarch is NULL
 * */
uint16_t uarch_get_nb_procs(uarch_t* uarch)
{
   return (uarch != NULL)?uarch->nb_procs:0;
}

/*
 * Returns the identifier of a micro-architecture
 * \param uarch The micro-architecture
 * \param The identifier of the micro-architecture or 0 if \c uarch is NULL
 * */
uint16_t uarch_get_id(uarch_t* uarch)
{
   return (uarch != NULL)?uarch->uarch_id:0;
}

/*
 * Returns the default processor to consider for a given micro-architecture
 * \param uarch The micro-architecture
 * \return The default processor version to consider for this architecture
 * */
proc_t* uarch_get_default_proc(uarch_t* uarch)
{
   return arch_get_uarch_default_proc(uarch_get_arch(uarch), uarch);
}

/*
 * Returns a list of instruction sets supported by at least one processor variant of a given micro-architecture.
 * \param uarch The micro-architecture
 * \param nb_isets Return parameter. Will be updated with the size of the returned array
 * \return An array containing the instruction sets supported by at least one processor in the micro-architecture,
 * or NULL if \c uarch or \c nb_isets is NULL. This array must be freed using lc_free
 * */
uint8_t* uarch_get_isets(uarch_t* uarch, uint16_t* nb_isets)
{
   if (uarch == NULL || nb_isets == NULL)
      return NULL;
   *nb_isets = 0;
   //Retrieves the number of instruction sets in the architecture
   unsigned int nb_arch_isets = arch_get_nb_isets(uarch_get_arch(uarch));
   if (nb_arch_isets == 0) {
      //Handling the case where the architecture has no instruction sets (unlikely)
      return NULL;
   }
   unsigned int isets[nb_arch_isets];  //Temporary array storing if the corresponding iset is present
   memset(isets, 0, nb_arch_isets*sizeof(*isets));
   uint16_t i;
   for (i = 0; i < uarch->nb_procs; i++) {
      proc_t* proc = uarch->procs[i];
      if (proc != NULL) {
         uint16_t j;
         for (j = 0; j < proc->nb_isets; j++) {
            isets[proc->isets[j] - 1] = 1;   //Instruction sets begin at 1
         }
      }
   }
   //Now computing the number of instruction sets
   for (i = 0; i < nb_arch_isets; i++)
      if (isets[i] == 1)
         *nb_isets = *nb_isets + 1;

   //Creating the array of instruction sets and filling it
   uint8_t* uarch_isets = lc_malloc(sizeof(*uarch_isets) * (*nb_isets));
   unsigned int i_iset = 0;
   for (i = 0; i < nb_arch_isets; i++)
      if (isets[i] == 1)
         uarch_isets[i_iset++] = i + 1;

   return uarch_isets;
}

///////////////////////////////////////////////////////////////////////////////
//                                   proc                                    //
///////////////////////////////////////////////////////////////////////////////

/*
 * Returns the micro-architecture for which a processor version is defined
 * \param proc The processor version
 * \param The associated micro-architecture or NULL if \c proc is NULL
 * */
uarch_t* proc_get_uarch(proc_t* proc)
{
   return (proc != NULL)?proc->uarch:NULL;
}

/*
 * Returns the name of a processor version
 * \param proc The processor version
 * \param The name of the processor version or NULL if \c proc is NULL
 * */
char* proc_get_name(proc_t* proc)
{
   return (proc != NULL)?proc->name:NULL;
}

/*
 * Returns the display name of a processor version
 * \param proc The processor version
 * \param The display name of the processor version or NULL if \c proc is NULL
 * */
char* proc_get_display_name(proc_t* proc)
{
   return (proc != NULL)?proc->display_name:NULL;
}

/*
 * Returns the information provided by the processor to identify its version
 * \param proc The processor version
 * \param The information provided by the processor to identify its version or NULL if \c proc is NULL
 * */
void* proc_get_cpuid_code(proc_t* proc)
{
   return (proc != NULL)?proc->cpuid_code:NULL;
}

/*
 * Returns the array of instruction set identifiers supported by this processor version
 * \param proc The processor version
 * \return Array of instruction set identifiers or NULL if \c proc is NULL
 * */
uint8_t* proc_get_isets(proc_t* proc)
{
   return (proc != NULL)?proc->isets:NULL;
}

/*
 * Returns the number of instruction set identifiers supported by this processor version
 * \param proc The processor version
 * \return Number of instruction set identifiers or 0 if \c proc is NULL
 * */
uint16_t proc_get_nb_isets(proc_t* proc)
{
   return (proc != NULL)?proc->nb_isets:0;
}

/*
 * Returns the identifier of a processor
 * \param uarch The processor
 * \param The identifier of the processor 0 if \c proc is NULL
 * */
uint16_t proc_get_id(proc_t* proc)
{
   return (proc != NULL)?proc->proc_id:0;
}

