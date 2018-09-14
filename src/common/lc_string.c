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
 * \file
 * */
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#ifndef _WIN32 
#include <regex.h>
#endif

#include "libmcommon.h"

///////////////////////////////////////////////////////////////////////////////
//                            string functions                               //
///////////////////////////////////////////////////////////////////////////////

/*
 * Parses a string to find a value (in decimal or hexadecimal format)
 * \param strinsn The string to parse
 * \param pos Pointer to the index value in the string at which to start the search.
 * Will be updated to point to the character immediately after the value
 * \param value Return parameter, will point to the parsed value
 * \return EXIT_SUCCESS if the parsing succeeded, EXIT_FAILURE otherwise
 * \todo Remove the use of pos and only use the string pointer (update also oprnd_parsenew and insn_parsenew)
 * */
int parse_number__(char* strinsn, int* pos, int64_t* value)
{
   int c = 0, sign;
   char valuestr[24]; //An int64_t value will not be coded on more than 24 characters, including the 0x and the sign

   if (pos)
      c = *pos;

   if (strinsn[c] == '-') {
      /*Offset begins with a negative sign*/
      sign = 1;
      c++;
   } else {
      sign = 0;
   }

   if ((strinsn[c] >= '0') && (strinsn[c] <= '9')) {
      /*There is a value*/
      int64_t val;
      int start = c, hex;

      /*Checking value base*/
      if ((strinsn[c] == '0')
            && ((strinsn[c + 1] == 'x') || (strinsn[c + 1] == 'X'))) {
         /*The value is coded in hexadecimal*/
         hex = 1;
         c += 2;
         start = c;
         /*Scanning the value*/
         while (((strinsn[c] >= '0') && (strinsn[c] <= '9'))
               || ((strinsn[c] >= 'A') && (strinsn[c] <= 'F'))
               || ((strinsn[c] >= 'a') && (strinsn[c] <= 'f'))) {
            valuestr[c - start] = strinsn[c];
            c++;
         }
      } else {
         hex = 0;
         /*Scanning the value*/
         while ((strinsn[c] >= '0') && (strinsn[c] <= '9')) {
            valuestr[c - start] = strinsn[c];
            c++;
         }
      }

      valuestr[c - start] = '\0';

      /*Retrieving the value*/
      if (hex == 1)
#ifdef _WIN32
         sscanf_s(valuestr,"%"PRIx64,&val);
         else
         sscanf_s(valuestr,"%"PRId64,&val);
#else
         sscanf(valuestr, "%"PRIx64, &val);
      else
         sscanf(valuestr, "%"PRId64, &val);
#endif

      /*Applying sign*/
      if (sign == 1)
         val = -val;

      if (pos)
         *pos = c;
      *value = val;
      return EXIT_SUCCESS;
   }

   return EXIT_FAILURE;
}

/*
 * Create a new string of size \e size
 * \param size number of character of the string
 * \return an initialized string
 */
char* str_new(unsigned int size)
{
   DBGMSG("Create a string with size %u\n", size);
   if (size == 0)
      return NULL;
   char* new = lc_malloc((size + 1) * sizeof *new);
   new[0] = '\0';
   return new;
}

/*
 * Free a string
 * \param p a pointer on a string to free
 */
void str_free(void* p)
{
   lc_free(p);
}

/*
 * Utility function for strings. Counts the number of substring delimited by \e delim
 * \param str The string to search
 * \param delim A delimiter
 * \return  the number of substring delimited by \e delim
 */
int str_count_field(const char* str, char delim)
{
   if (str == NULL)
      return 0;

   int num = 1;

   size_t i;
   for (i = 0; i < strlen(str); i++)
      if (str[i] == delim)
         num++;

   return num;
}

/*
 * Get the \e numarg field of a string, delimeted by delim
 * \param str a string
 * \param numarg index of the field to return
 * \param \param delim a delimitor
 * \return a string with the field, else NULL
 */
char* str_field(char* str, int numarg, char delim) // .h =char(FIELDDELIM)
{
   size_t i = 0;
   int narg_found = 0;

   while (narg_found < numarg && i < strlen(str)) {
      if (str[i] == delim)
         narg_found++;
      i++;
   }

   if (i == strlen(str))
      return NULL;

   size_t pos_deb = i;
   size_t pos_end = pos_deb;

   while (pos_end < strlen(str) && str[pos_end] != delim)
      pos_end++;

   if (pos_deb != pos_end) {
      char *pchar = lc_malloc((pos_end - pos_deb + 1) * sizeof *pchar);

      pchar = lc_strncpy(pchar, (pos_end - pos_deb + 1) * sizeof *pchar,
            &str[pos_deb], pos_end - pos_deb);
      pchar[pos_end - pos_deb] = '\0';

      return pchar;
   }

   return NULL;
}

/*
 * Appends two 0-terminated character strings to each others
 * \param text The string to append a suffix to (is freed at the completion of the function)
 * \param textlen The number of characters in \e text after which the suffix has to be appended
 * (-1 if the whole string must be used).
 * \param suffix The string to append to \e text
 * \param suffixlen The number of characters in \e suffix to append to \e text (-1 if the whole
 * string has to be appended)
 * \return A 0-terminated string containing the \e n first characters of \e text
 * (or all if \e n is -1), followed by the 0 character then the first \e m characters of \e suffix
 */
char* str_append(char* text, int textlen, const char* suffix, int suffixlen)
{
   int _n, _m;

   if (textlen == -1 && text != NULL)
      _n = strlen(text);
   else
      _n = textlen;

   if (!text)
      _n = 0;

   if (suffixlen == -1)
      _m = strlen(suffix);
   else
      _m = suffixlen;

   int newtext_size = (_n + _m + 1) * sizeof(char);
   char *newtext = lc_malloc0(newtext_size);

   if (_n != 0) {
      lc_strncpy(newtext, newtext_size, text, _n);
      newtext[_n] = '\0';
#ifdef _WIN32
      strncat_s(newtext, newtext_size, suffix, _m);
#else
      strncat(newtext, suffix, _m);
#endif
   } else if (_m != 0)
      lc_strncpy(newtext, newtext_size, suffix, _m);

   newtext[_n + _m] = '\0';
   lc_free(text);

   return newtext;
}

/*
 * Replace a substring \e pattern by a substring \e replacement in a string \e str
 * \param str original string
 * \param pattern substring to replace
 * \param replacement substring which replace pattern
 * \return a new string
 */
char* str_replace(const char* str, const char* pattern, const char* replacement)
{
   if (str == NULL)
      return NULL;

   if (replacement == NULL || pattern == NULL)
      return lc_strdup(str);

   int lasti = 0, nbreplace = 0;
   char* replaced = NULL;

   size_t i;
   for (i = 0; i < strlen(str); i++) {
      size_t j = 0;
      while (j < strlen(pattern) && (i + j) < strlen(str)
            && str[i + j] == pattern[j])
         j++;

      /* we reach the end of the pattern string, we found the pattern at the i'th position in string */
      if (j == strlen(pattern)) {
         if (replaced == NULL) {
            replaced = lc_malloc((i + 1) * sizeof *replaced);
            lc_strncpy(replaced, (i + 1) * sizeof *replaced, str, i);
            replaced[i] = '\0';
         } else
            replaced = str_append(replaced, -1, str + lasti + strlen(pattern),
                  i - lasti - strlen(pattern));

         replaced = str_append(replaced, -1, replacement, -1);
         lasti = i;
         nbreplace++;
      }
   }

   /* copy the string tail */
   if (nbreplace > 0 && (unsigned int) lasti < i)
      replaced = str_append(replaced, -1, str + lasti + strlen(pattern),
            i - lasti + strlen(pattern));
   else
      replaced = str_append(replaced, -1, str, -1);

   return replaced;
}

/*
 * Replaces all occurrences of a character in a string by another
 * \param str The string to update
 * \param from The character to replace
 * \param to The character to set in place of \e from
 */
void str_replace_char(char* str, char from, char to)
{
   if (str == NULL)
      return;

   size_t i;
   for (i = 0; i < strlen(str); i++)
      if (str[i] == from)
         str[i] = to;
}

/*
 * Replaces all non standard ASCII characters (ie everything not A-Z ,a-z, 0-9 or '_')
 * by the character '_' in a string
 * \param str a string
 */
void str_replace_char_non_c(char* str)
{
   if (str == NULL)
      return;

   size_t i;
   for (i = 0; i < strlen(str); i++) {
      char c = str[i];

      if ((isalnum(c) == 0) && (c != '_'))
         str[i] = '_';
   }
}

/*
 * Concatenate two strings. The result can be released with lc_free
 * Both original strings are not destroyed after the operation
 * \param str1 a string
 * \param str2 a string
 * \return a new string equal to str1+str2
 */
char* str_concat(const char* str1, const char* str2)
{
   if (!str1 && !str2)
      return NULL;

   if (!str2)
      return lc_strdup(str1);
   if (!str1)
      return lc_strdup(str2);

   size_t l1 = strlen(str1);
   size_t l2 = strlen(str2);
   char *str12 = lc_malloc(l1 + l2 + 1);

   /* faster than strcpy + strcat since already known string lengths */
   memcpy(str12, str1, l1);
   memcpy(str12 + l1, str2, l2 + 1); /* +1 for copying the terminating '\0' from str2 */

   return str12;
}

static char *str_get_modified_copy(const char *str, int (*f)(int))
{
   if (!str)
      return NULL;

   char *res = lc_malloc((strlen(str) + 1) * sizeof *res);

   size_t i = 0;
   for (i = 0; i <= strlen(str); i++)
      res[i] = f(str[i]);

   return res;

}

/*
 * Converts a string to upper case characters. The result can be released with lc_free
 * The original string is not changed
 * \param a string to convert
 * \return a new string with upper case characters
 */
char* str_toupper(const char* str)
{
   return str_get_modified_copy(str, toupper);
}

/*
 * Converts a string to lower case characters. The result can be released with lc_free
 * The original string is not changed
 * \param a string to convert
 * \return a new string with lower case characters
 */
char* str_tolower(const char* str)
{
   return str_get_modified_copy(str, tolower);
}

/**
 * Copies a string to another while transforming each character
 * */
static char *strcopy_modif(char* str1, const char* str2, int (*f)(int))
{
   assert(str1 && str2 && f);
   size_t i = 0;
   while (str2[i] != '\0') {
      str1[i] = f(str2[i]);
      i++;
   }
   str1[i] = str2[i];
   return str1;
}

/*
 * Copies the content of str2 to str1 converted to upper characters.
 * Copies up to and including the NULL character of str2.
 * \param str1 String to copy to. Must be already allocated
 * \param str2 String to copy
 * \return str1
 * */
char* strcpy_toupper(char* str1, const char* str2)
{
   return strcopy_modif(str1, str2, toupper);
}

/*
 * Copies the content of str2 to str1 converted to lower characters.
 * Copies up to and including the NULL character of str2.
 * \param str1 String to copy to. Must be already allocated
 * \param str2 String to copy
 * \return str1
 * */
char* strcpy_tolower(char* str1, const char* str2)
{
   return strcopy_modif(str1, str2, tolower);
}

/*
 * Returns a string of length l filled with the character c
 * \param c a character used to fill the string
 * \param l length of the new string
 * \return a string filled with c
 */
char* str_fill(char c, int l)
{
   char *res = lc_malloc((l + 1) * sizeof *res);

   memset(res, c, l);
   res[l] = '\0';

   return res;
}

#ifndef _WIN32
/*
 * Search if \a txt contains \a exp
 * \param txt a string to examine
 * \param exp a regular expression
 * \return 1 if \a txt contains exp else 0
 */
int str_contain(const char* txt, const char* exp)
{
   if ((txt == NULL) || (exp == NULL))
      return 0;
   regex_t preg;
   if (regcomp(&preg, exp, REG_NOSUB | REG_EXTENDED | REG_ICASE) == 0) {
      int match = regexec(&preg, txt, 0, NULL, 0);
      regfree(&preg);
      return (match == 0) ? 1 : 0;
   } else {
      fprintf(stderr, "Utils error : contain function crashed\n");
      exit(-1);
   }
   return 0;
}
#endif

/*
 * Wrapper to the strcmp function allowing qsort to work
 * \param a a pointer on a string (char**)
 * \param b a pointer on a string (char**)
 * \return the result of strcmp on a and b
 */
int strcmp_qsort(const void* a, const void* b)
{
   const char* A = *(const char**) a;
   const char* B = *(const char**) b;

   return strcmp(A, B);
}

/*
 * Wrapper to the strcmp function allowing bsearch to work
 * \param a A string
 * \param b A string
 * \return The result of strcmp on a and b
 */
int strcmp_bsearch(const void* a, const void* b)
{
   return strcmp(a, b);
}

static int* _split_version(const char* str, int* size)
{
   if (str == NULL || size == NULL)
      return (NULL);
   int s = str_count_field(str, '.');
   char* cstr = lc_strdup(str);
   int* substr = lc_malloc(s * sizeof(int));
   int i = 0;
   char* tmp = NULL;

   substr[i++] = atoi(strtok(cstr, "."));
   while ((tmp = strtok(NULL, ".")) != NULL)
      substr[i++] = atoi(tmp);
   lc_free (cstr);
   (*size) = s;
   return (substr);
}

/*
 * Compare two strings containing version number matching "[0-9]+(.[0-9]+)*"
 * \param v1
 * \param v2
 * \return a negative value is v1 > v2, 0 is v1 == v2 and a positive value if v1 < v2
 */
int str_compare_version(const char* v1, const char* v2)
{
   if (v1 == NULL && v2 != NULL)
      return (1);
   else if (v1 != NULL && v2 == NULL)
      return (-1);
   else if (v1 == v2)
      return (0);

   if (strcmp(v1, v2) == 0)
      return (0);

   int s1 = 0;
   int s2 = 0;
   int* sv1 = _split_version(v1, &s1);
   int* sv2 = _split_version(v2, &s2);
   int min_s = 0;
   int i = 0;

   if (s1 < s2)
      min_s = s1;
   else
      min_s = s2;

   int ret = 0;
   for (i = 0; i < min_s; i++) {
      if (sv1[i] < sv2[i]) {
         ret = 1;
         break;
      }
      else if (sv1[i] > sv2[i]) {
         ret = -1;
         break;
      }
   }
   if (ret == 0) {
      if (s1 < s2 && sv2[s1] > 0)
         ret = 1;
      else if (s1 > s2 && sv1[s2] > 0)
         ret = -1;
   }
   lc_free (sv1);
   lc_free (sv2);
   return ret;
}

#ifndef _WIN32
/*
 * Search if \a txt contains \a exp and collects all the subpatterns (use parenthesis for subpattern)
 * \param txt a string to examine
 * \param exp a regular expression
 * \param ptr_str_matched If matches is provided, then it is filled with the results of search. 
 *                        str_matched[0] will contain the text that matched the full pattern, 
 *                        str_matched[1] will have the text that matched the first captured parenthesized subpattern, 
 *                        and so on.
 * \return The number of subpatterns if \a txt contains exp.
 *         0  No match.
 *         -1 A problem occurred.
 */
int str_match(const char* txt, const char* exp, char*** ptr_str_matched)
{
   int err;
   int ret = -1; // -1 IF AN ERROR OCCURRED
   size_t nmatch = 0;
   regex_t preg;
   char** str_matched = *ptr_str_matched;
   str_matched = NULL;

   //Compile the reg exp (exp) into a form that is suitable for subsequent regexec() searches. 
   err = regcomp(&preg, exp, REG_EXTENDED);
   if (err == 0) {
      int match;
      regmatch_t *pmatch = NULL;
      nmatch = preg.re_nsub + 1; // +1 for the full patern
      pmatch = lc_malloc(sizeof(*pmatch) * nmatch);
      //Search the reg exp and the subpatterns in txt
      match = regexec(&preg, txt, nmatch, pmatch, 0);
      regfree(&preg);
      if (match == 0) {
         //Pattern and subpattern(s) have been detected
         size_t match_id = 0;
         str_matched = lc_calloc(nmatch, sizeof(*str_matched));

         //For each subpattern (+ the full pattern in pmatch[0])
         while (match_id < nmatch) {
            int start = pmatch[match_id].rm_so; //Start of the subpattern
            int end = pmatch[match_id].rm_eo;   //End of the subpattern
            size_t size = end - start;          //Size of the subpattern

            str_matched[match_id] = lc_malloc(
                  sizeof(*str_matched[match_id]) * (size + 1));
            strncpy(str_matched[match_id], &txt[start], size);
            str_matched[match_id][size] = '\0';
            DBGMSG("MATCH : str_matched[%zu]<%s>\n", match_id,
                  str_matched[match_id]);
            match_id++;
         }
         ret = match_id;
      } else {
         //REG EXP DOES NOT MATCH!
         ret = 0;
      }
      free(pmatch);
   }
   *ptr_str_matched = str_matched;
   return ret;
}
#endif

/*
 * Compares two string without taking the cases into account
 * \param str1 First string
 * \param str2 Second string
 * \return TRUE if both string are identical, not taking the case into account, FALSE otherwise
 * */
int str_equal_nocase(const char* str1, const char* str2)
{
   if (!str1 || !str2)
      return (str1 == str2);
   while (*str1 && toupper(*str1) == toupper(*str2)) {
      str1++;
      str2++;
   }
   return (*str1 == *str2);
}
