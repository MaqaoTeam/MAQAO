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
#include <regex.h>
#include <cxxabi.h>
#endif
#include <cstring>
#include <exception>

extern "C" {
#include "libmasm.h"
}

#define REGEX_GNU_FORTRAN "(^__[a-z][a-z_0-9]*_MOD_[a-z][a-z_0-9]*$)|(^[a-z][a-z_0-9]*_$)"
#define REGEX_INTEL_FORTRAN "^([a-z][a-z_0-9]*_mp_)?[a-z][a-z_0-9]*_$"
#define REGEX_GNU_INTEL_CPP "^_Z[0-9a-zA-Z_]+"

#ifndef _WIN32
static int str_contains(const char* txt, const char* exp)
{
   regex_t preg;
   if ((txt == NULL) || (exp == NULL))
      return (0);
   if (regcomp(&preg, exp, REG_NOSUB | REG_EXTENDED | REG_ICASE) == 0) {
      int match = regexec(&preg, txt, 0, NULL, 0);
      regfree(&preg);
      if (match == 0)
         return (1);
      else
         return (0);
   } else {
      DBGMSG0("str_contains function crashed\n");
      exit(-1);
   }
   return (0);
}

// Demangle a string from GNU / Fortran
static char* ldem_demangle_f90_GNU(const char* name)
{
   char* res = NULL;

   // if there is a module, get the substring located after D_
   if (str_contains(name, "_MOD_") == TRUE) {
      const char* after_MOD = (strchr(name, 'D') + 2);
      res = (char*) lc_strdup(after_MOD);
   } else {
      int size = strlen(name);
      res = lc_strdup(name);
      res[size] = '\0';
   }
   return (res);
}

// Demangle a string from Intel / Fortran
static char* ldem_demangle_f90_INTEL(const char* name)
{
   char* res = NULL;

   // if there is at least once the pattern _mp_, 
   //get the substring located after the first one
   if (str_contains(name, "mp_mp") == TRUE) {
      //warning: can not determine module and function
      printf("Warning: Current name [%s] can not be obviously demangled\n",
            name);
      int i = 0;
      while (strstr(&name[i], "_mp_") != NULL)
         i++;
      res = (char*) lc_strdup(&name[i + 3]);
   } else if (str_contains(name, "_mp_") == TRUE) {
      const char* after_MOD = (strstr(name, "_mp_") + 4);
      res = (char*) lc_strdup(after_MOD);
   } else
      res = lc_strdup(name);

   if (res[strlen(res) - 1] == '_')
      res[strlen(res) - 1] = '\0';
   return (res);
}

// Demangle a string from Intel/GNU / Cpp using cxxabi
static char* ldem_demangle_cpp_using_cxxabi(const char* name)
{
   int status = 0;				//useless, needed to the call to the libc++
   char* demangled = NULL;		//contains the result of the call to the libc++
   char* res = NULL;	         //returned string
   //int pos_2dot = 0;				//position of the last ':' or ' ' charactere to get the function name start
   int pos_parG = 0;	//position of the first '<' or '(' charactere to get the function name stop
   int i = 0;						//counter
   int size = 0;					//size of the demangled string
   int nb_param = 0;

   if (strcmp(name, "main") == 0)
      return (lc_strdup(name));

#ifdef IS_STDCXX
   // call cxxabi demngled function to get a demangled version of the string
   try
   {
      demangled = abi::__cxa_demangle(name, 0, 0, &status);
   }
   catch (const std::bad_exception &) {}

   // check the return value is not null. Else, this means the input string is bad
   if (demangled == NULL)
   {
      DBGMSG ("Input name %s is not GNU C++ \n",name);
      return (NULL);
   }
   size = strlen (demangled);
   // get the position of the opening bracket / template
   for (pos_parG = 0; pos_parG < size; pos_parG++)
   if (demangled[pos_parG ] == '(' || demangled[pos_parG] == '<')
   break;

   // get the position of the last ':' charactere
   //for (i = 0; i < size; i++)
   // if ((demangled[i] == ':' || demangled[i] == ' ') && i < pos_parG)
   //    pos_2dot = i + 1;
   //
   // extract the function name
   //res = (char*)lc_malloc ((pos_parG - pos_2dot + 1) * sizeof (char));
   //res = strncpy(res, &demangled[pos_2dot], pos_parG - pos_2dot);
   //res [pos_parG - pos_2dot] = '\0';

   // for the moment, the returned name have parameters. Uncommented three last lines
   // and comment the next line to get the string without params
   res = lc_strdup (demangled);

   // get the number of parameters (count the number of commas between brackets)
   // get the position of the oppening bracket (at this level, pos_parG can contains '(' or '<')
   for (; pos_parG < size; pos_parG++)
   if (demangled[pos_parG] == '(')
   break;

   for (i = pos_parG; i < size || demangled[i] == ')'; i++)
   if (demangled[i] == ',')
   nb_param ++;
   // if at least one comma, add one to the counter
   // (the story of poles and intervals ...)
   if (nb_param > 0)
   nb_param ++;
   // if no commat, check there is something between '(' and ')'
   if (nb_param == 0 && pos_parG != size && demangled[pos_parG + 1] != ')')
   nb_param ++;

   free (demangled);
   return (res);
#else
   return (NULL);
#endif
}
#endif

// Demangles a string from C. Has there is no mangling, the name is just copy in the structure
static char* ldem_demangle_C(const char* name)
{
   char* res = NULL;

   if (name == NULL) {
      DBGMSG0("No name given\n");
      return (NULL);
   }
   res = lc_strdup(name);

   return (res);
}

/*
 * Demangles a function name
 * \param name function name to demangle
 * \param compiler can be used to specify the compiler if it is known
 * \param language specify the language.
 * \return a demangled sting
 */
char* fct_demangle(const char* name, char compiler, char language)
{
   char comp = compiler;
   char* res = NULL;

   if (name == NULL) {
      DBGMSG0("No name given\n");
      return (NULL);
   }

#ifndef _WIN32
   // if the compiler is not given, try to find it
   if (comp == COMP_ERR) {
      if (language == LANG_FORTRAN) {
         if (str_contains(name, REGEX_INTEL_FORTRAN) == TRUE)
            comp = COMP_INTEL;
         else if (str_contains(name, REGEX_GNU_FORTRAN) == TRUE)
            comp = COMP_GNU;
      } else if (language == LANG_CPP) {
         if (str_contains(name, REGEX_GNU_INTEL_CPP) == TRUE
               || strcmp(name, "main") == 0)
            comp = COMP_INTEL;
      } else if (language == LANG_C)
         comp = COMP_INTEL;
      else if (str_contains(name, "^_Z"))
         return (ldem_demangle_cpp_using_cxxabi(name));
   }

   // if the compiler is known, run the demangling
   if (comp != COMP_ERR) {
      if (language == LANG_CPP && (comp == COMP_GNU || comp == COMP_INTEL))
         res = ldem_demangle_cpp_using_cxxabi(name);
      else if (language == LANG_FORTRAN && comp == COMP_GNU)
         res = ldem_demangle_f90_GNU(name);
      else if (language == LANG_FORTRAN && comp == COMP_INTEL)
         res = ldem_demangle_f90_INTEL(name);
      else if (language == LANG_C)
         res = ldem_demangle_C(name);
   }
#endif

   return (res);
}

