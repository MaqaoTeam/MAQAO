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
 * \file lc_txtfile.c
 * \brief This file contains getters, setters, and functions for parsing text files
 */
#include <inttypes.h>
#include "libmcommon.h"

//#define BUFFER_SIZE 1024   /**<Buffer for storing a temporary string*/

static char* order_by_field = NULL; /**<Name of the field to use when ordering the lines in a parsed file*/

/**
 * Builds a tag for a given name. This macro assumes the structure containing the text file is called tf
 * \param TAG_NAME Variable where to store the name (it will be a char*)
 * \param NAME Name of the tag
 * \param POS begin / end
 * */
#define BUILD_TAG(TAG_NAME, NAME, POS)\
   char TAG_NAME[strlen(tf->tag_prfx) + strlen(NAME) + strlen(tf->tag_##POS##_sufx) + 1];\
   lc_sprintf(TAG_NAME, sizeof(TAG_NAME),\
         "%s%s%s", tf->tag_prfx, NAME, tf->tag_##POS##_sufx);

/**
 * Builds a begin tag for a given name. This macro assumes the structure containing the text file is called tf
 * \param BEGIN_TAG_NAME Variable where to store the name (it will be a char*)
 * \param NAME Name of the tag
 * */
#define BUILD_BEGIN_TAG(BEGIN_TAG_NAME, NAME) BUILD_TAG(BEGIN_TAG_NAME, NAME, begin)

/**
 * Builds an end tag for a given name. This macro assumes the structure containing the text file is called tf
 * \param END_TAG_NAME Variable where to store the name (it will be a char*)
 * \param NAME Name of the tag
 * */
#define BUILD_END_TAG(END_TAG_NAME, NAME) BUILD_TAG(END_TAG_NAME, NAME, end)

/**
 * Creates a stack-allocated array containing a sub string delimited between two pointers.
 * \param SUBSTRING Name of the array
 * \param START Pointer containing the first character to add to the array
 * \param END Pointer containing the first character not to add to the array
 * */
#define GET_SUBSTRING_INSTRING(SUBSTRING, START, END) char SUBSTRING[END - START + 1];\
   lc_strncpy(SUBSTRING, sizeof(SUBSTRING), tf->cursor, END - START);\
   SUBSTRING[END - START] = '\0';

/**
 * Creates a stack-allocated array containing a sub string delimited between the cursor and a given pointer.
 * This macro assumes the structure containing the text file is called tf
 * \param SUBSTRING Name of the array
 * \param END Pointer containing the first character not to add to the array
 * */
#define GET_SUBSTRING(SUBSTRING, END) GET_SUBSTRING_INSTRING(SUBSTRING, tf->cursor, END)

/**
 * Skips spaces and returns with an error if the function skipping spaces fails
 * This function assumes the structure containing the text file is called tf
 * \param RETCODE Variable to use for storing the return code
 * */
#define SKIP_SPACES(RETCODE) retcode = txtfile_skipspaces(tf);\
   if (ISERROR(retcode))\
      return retcode;

/**
 * Structure used for storing a field and its name in order to compare its value with the field of a section.
 * This is intended to be used by txtscn_cmpfield_bsearch
 * */
typedef struct txtfield_cmp_s {
   union {
      char* txtval; /**<Value of the field if a text field*/
      int64_t numval; /**<Value of the field if a numerical field*/
   } value;
   char* fieldname; /**<Name of the field*/
   txtfieldtype_t type; /**<Type of the field*/
} txtfield_cmp_t;

/// Initialisation functions ///
////////////////////////////////

/**
 * Initialises the file details with default values
 * \param tf Text file
 * */
static void txtfile_initdefault(txtfile_t* tf)
{
   assert(tf);
   //Tags and section delimiters
   tf->tag_prfx = lc_strdup("_@M_");
   tf->tag_begin_sufx = lc_strdup("_begin");
   tf->tag_end_sufx = lc_strdup("_end");
   tf->bodyname = lc_strdup("body");
   tf->hdrname = lc_strdup("header");
   //Fields delimiters and separators
   tf->decl_field_delim = ' ';
   tf->field_delim = ' ';
   tf->field_name_separator = ':';
   tf->txtfield_delim = '\"';
   tf->listfield_delim = ';';
   //Comments delimiters
   tf->commentline = lc_strdup("//");
   //Fields identifiers
   tf->strfieldid = lc_strdup("str");
   tf->numfieldid = lc_strdup("num");
   tf->propfieldid = lc_strdup("property");
   //Keywords
   tf->scndecl_interleaved = lc_strdup("interleaved");
   tf->scndecl_matchfieldbyname = lc_strdup("matchbyname");
   tf->scndecl_matchfieldbypos = lc_strdup("matchbypos");
   //Field identifiers
   tf->fieldidsuf_list = 'L';
   //Numerical fields identifiers
   tf->numdecl_unsigned = 'u';
   tf->numdecl_base[BASE_10] = 'd';
   tf->numdecl_base[BASE_16] = 'h';
   tf->numdecl_base[BASE_08] = 'o';
   tf->numdecl_base[BASE_02] = 'B';
   tf->numdecl_size = 's';

}

/*
 * Sets the comment delimiters for a text file
 * \param tf The text file
 * \param commentline Delimiter for a commented line (everything following will be ignored)
 * \param commentbegin Delimiter for the beginning of a comment
 * \param commentend Delimiter for the ending of a comment
 * */
void txtfile_setcommentsdelim(txtfile_t* tf, char* commentline,
      char* commentbegin, char* commentend)
{
   if (!tf)
      return;
   if (commentline) {
      if (tf->commentline)
         lc_free(tf->commentline);
      tf->commentline = lc_strdup(commentline);
   }
   if (commentbegin && commentend) {
      if (tf->commentbegin)
         lc_free(tf->commentbegin);
      if (tf->commentend)
         lc_free(tf->commentend);
      tf->commentbegin = lc_strdup(commentbegin);
      tf->commentend = lc_strdup(commentend);
   }
}

/*
 * Sets the tags for identifying sections in the file
 * \param tf The text file
 * \param scnprfx Prefix signaling the beginning of a section tag
 * \param tag_begin_sufx Suffix for the tag signaling the beginning of a section
 * \param scnendsufx Suffix for the tag signaling the ending of a section
 * \param bodyname Name of the text section
 * */
void txtfile_setscntags(txtfile_t* tf, char* tag_prfx, char* tag_begin_sufx,
      char* tag_end_sufx, char* bodyname, char* hdrname)
{
   if (!tf)
      return;
   if (tag_prfx) {
      if (tf->tag_prfx)
         lc_free(tf->tag_prfx);
      tf->tag_prfx = lc_strdup(tag_prfx);
   }
   if (tag_begin_sufx) {
      if (tf->tag_begin_sufx)
         lc_free(tf->tag_begin_sufx);
      tf->tag_begin_sufx = lc_strdup(tag_begin_sufx);
   }
   if (tag_end_sufx) {
      if (tf->tag_end_sufx)
         lc_free(tf->tag_end_sufx);
      tf->tag_end_sufx = lc_strdup(tag_end_sufx);
   }
   if (bodyname) {
      if (tf->bodyname)
         lc_free(tf->bodyname);
      tf->bodyname = lc_strdup(bodyname);
   }
   if (hdrname) {
      if (tf->hdrname)
         lc_free(tf->hdrname);
      tf->hdrname = lc_strdup(hdrname);
   }
}

/*
 * Sets the delimiters for identifying fields declarations in the header
 * \param tf The text file
 * \param strfieldid Identifier of a string field. Declaration of string fields must begin with this
 * \param numfieldid Identifier of an integer field. Declaration of interger fields must begin with this
 * \param field_name_separator Character separating the components of the field declarations
 * \param optfield_prefix Prefix used to signal the beginning of the declaration of an optional field
 * */
void txtfile_setfieldtags(txtfile_t* tf, char* strfieldid, char* numfieldid,
      char field_name_separator, char* optfield_prefix)
{
   if (!tf)
      return;
   if (strfieldid) {
      if (tf->strfieldid)
         lc_free(tf->strfieldid);
      tf->strfieldid = lc_strdup(strfieldid);
   }
   if (numfieldid) {
      if (tf->numfieldid)
         lc_free(tf->numfieldid);
      tf->numfieldid = lc_strdup(numfieldid);
   }
   tf->field_name_separator = field_name_separator;
   if (optfield_prefix) {
      if (tf->optfield_prefix)
         lc_free(tf->optfield_prefix);
      tf->optfield_prefix = lc_strdup(optfield_prefix);
   }
}

/// Utility functions ///
/////////////////////////

/**
 * Compares two sections based on their names
 * \param s1 First section
 * \param s2 Second section
 * */
static int txtscn_cmp(const void* s1, const void* s2)
{
   assert(s1 && s2);
   const txtscn_t* ts1 = s1;
   const txtscn_t* ts2 = s2;
   return strcmp(ts1->type, ts2->type);
}

/**
 * Compares two fields based on their names
 * \param f1 First field
 * \param f2 Second field
 * */
static int txtfield_cmp(const void* f1, const void* f2)
{
   assert(f1 && f2);
   const txtfield_t* field1 = f1;
   const txtfield_t* field2 = f2;
   return strcmp(field1->name, field2->name);
}

/**
 * Compares the type of a section with a given type. This is intended to be used in bsearch
 * \param k Key to compare (a string)
 * \param s Section
 * */
static int txtscn_type_cmp_bsearch(const void* k, const void* s)
{
   assert(k && s);
   char* key = (char*) k;
   txtscn_t** ts = (txtscn_t**) s;
   return strcmp(key, (*ts)->type);
}

/**
 * Compares the type of a field with a given type. This is intended to be used in bsearch
 * \param k Key to compare (a string)
 * \param f Field
 * */
static int txtfield_name_cmp_bsearch(const void* k, const void* f)
{
   assert(k && f);
   char* key = (char*) k;
   txtfield_t** field = (txtfield_t**) f;
   return strcmp(key, (*field)->name);
}

/**
 * Compares two sections based on their names. Wrapper for use in qsort
 * \param s1 First section
 * \param s2 Second section
 * */
static int txtscn_cmp_qsort(const void* s1, const void* s2)
{
   return txtscn_cmp(*(const void**) s1, *(const void**) s2);
}

/**
 * Compares two fields based on their names. Wrapper for use in qsort
 * \param f1 First field
 * \param f2 Second field
 * */
static int txtfield_cmp_qsort(const void* f1, const void* f2)
{
   return txtfield_cmp(*(const void**) f1, *(const void**) f2);
}

/**
 * Function used for comparing two sections.
 * The name of the field stored in order_by_field will be used for the comparison. This version is for the cases where the field is numerical
 * \param s1 First section
 * \param s2 Second section
 * \return -1 if the value of the field whose name is stored in order_by_field is less in s1 than in s2, 1 if it is greater, and 0 if equal
 * */
static int txtscn_numfieldcmp_qsort(const void* s1, const void* s2)
{
   assert(order_by_field);
   const txtscn_t* scn1 = *(const txtscn_t**) s1;
   const txtscn_t* scn2 = *(const txtscn_t**) s2;

   int64_t v1 = txtfield_getnum(txtscn_getfield(scn1, order_by_field));
   int64_t v2 = txtfield_getnum(txtscn_getfield(scn2, order_by_field));
   if (v1 < v2)
      return -1;
   else if (v1 > v2)
      return 1;
   else
      return 0;
}

/**
 * Function used for comparing two sections.
 * The name of the field stored in order_by_field will be used for the comparison. This version is for the cases where the field is text
 * \param s1 First section
 * \param s2 Second section
 * \return -1 if the value of the field whose name is stored in order_by_field is less in s1 than in s2, 1 if it is greater, and 0 if equal
 * */
static int txtscn_txtfieldcmp_qsort(const void* s1, const void* s2)
{
   assert(order_by_field);
   const txtscn_t* scn1 = *(const txtscn_t**) s1;
   const txtscn_t* scn2 = *(const txtscn_t**) s2;

   char* t1 = txtfield_gettxt(txtscn_getfield(scn1, order_by_field));
   char* t2 = txtfield_gettxt(txtscn_getfield(scn2, order_by_field));

   return strcmp(t1, t2);
}

/**
 * Checks if a string begins with a given substring
 * \param str The string
 * \param substr The substring
 * \return TRUE if the first characters of str are identical to those of substr, FALSE otherwise
 * */
static int str_beginwith(const char* str, const char* substr)
{
   if (str && substr) {
      return (!strncmp(str, substr, strlen(substr)));
   }
   return FALSE;
}

/**
 * Checks if a character is alphanumeric
 * \param c The character
 * \return TRUE if \c is a letter or a number FALSE otherwise
 * */
static int character_isalphanumeric(const char c)
{
   if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
         || (c >= '0' && c <= '9'))
      return TRUE;
   return FALSE;
}

/**
 * Checks if a character is an end line (carriage return / line feed)
 * \param c The character
 * \return TRUE if \c is a character signaling the end of a line, FALSE otherwise
 * */
static int character_isendline(const char c)
{
   if (c == '\n' || c == '\r')
      return TRUE;
   return FALSE;
}

/**
 * Checks if a character is a blank space (space, tabulation, carriage return / line feed)
 * \param c The character
 * \return TRUE if \c is a blank space FALSE otherwise
 * */
static int character_isblankspace(const char c)
{
   if (c == ' ' || c == '\t' || character_isendline(c))
      return TRUE;
   return FALSE;
}

/**
 * Skips the cursor to the next line
 * \param tf The text file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int txtfile_skipline(txtfile_t* tf)
{
   assert(tf);
   while (*tf->cursor != '\0' && character_isendline(*tf->cursor) == FALSE)
      tf->cursor++;
   if (*tf->cursor == '\0')
      return EXIT_SUCCESS; /**\todo TODO (2015-07-06) Or return a warning or special code*/
   if (*tf->cursor == '\r' && *(tf->cursor + 1) == '\n')
      tf->cursor += 2;
   else
      tf->cursor++;
   tf->line++;
   return EXIT_SUCCESS;
}

/**
 * Skips comments in a text file (positions the cursor at the first non-space character)
 * \param tf The text file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int txtfile_skipcomments(txtfile_t* tf)
{
   if (str_beginwith(tf->cursor, tf->commentline)) {
      DBGMSG("Skipping commented line %u\n", tf->line)
      //Comment delimiter found: skipping the remainder of the line
      int retcode = txtfile_skipline(tf);
      if (ISERROR(retcode))
         return retcode;
   }
   if (str_beginwith(tf->cursor, tf->commentbegin)) {
      //Found the beginning of a commented code
      assert(tf->commentend);
      char* endcomment = strstr(tf->cursor + strlen(tf->commentbegin),
            tf->commentend);
      if (!endcomment)
         return ERR_COMMON_TXTFILE_COMMENT_END_NOT_FOUND;
      //Skips to the end of the comment
      while (tf->cursor != (endcomment + strlen(tf->commentend))) {
         if (*tf->cursor == '\n')
            tf->line++;
         tf->cursor++;
      }
   }
   return EXIT_SUCCESS;
}

/**
 * Skips spaces in a text file (positions the cursor at the first non-space character)
 * \param tf The text file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int txtfile_skipspaces(txtfile_t* tf)
{
   assert(tf && tf->content && tf->cursor);
   int retcode;
   retcode = txtfile_skipcomments(tf);
   if (ISERROR(retcode))
      return retcode;
   while (*tf->cursor != '\0' && character_isblankspace(*tf->cursor) == TRUE) {
      if (*tf->cursor == '\n')
         tf->line++;
      tf->cursor++;
      retcode = txtfile_skipcomments(tf);
      if (ISERROR(retcode))
         return retcode;
   }
   return EXIT_SUCCESS;
}

/**
 * Moves the cursor to a given position and updates the line number
 * \param tf The text file
 * \param stop The character to reach in the content
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int txtfile_updcursor(txtfile_t* tf, char* stop)
{
   assert(tf && stop);
   int retcode;
   while (tf->cursor != stop && *tf->cursor != '\0') {
      if (*tf->cursor == '\n')
         tf->line++;
      retcode = txtfile_skipcomments(tf);
      if (ISERROR(retcode))
         return retcode;
      tf->cursor++;
   }
   return EXIT_SUCCESS;
}

/**
 * Finds the closest end line and returns a pointer to the first character of the sequence indicating the end line
 * \param tf The text file
 * \return Pointer to the first character of a sequence signaling an end line or NULL if no end line was found
 * */
static char* txtfile_findendline(txtfile_t* tf)
{
   assert(tf);
   char* endline = tf->cursor;
   //Finds the first character that could signal an end line
   while (*endline != '\0' && *endline != '\r' && *endline != '\n')
      endline++;
   if (*endline == '\0')
      return NULL;   //End of file reached without finding an end line
   return endline;
}

/**
 * Checks if a text file is correctly initialised and can be parsed
 * \param tf The text file
 * \return EXIT_SUCCESS if the file has been initialised correctly, error code otherwise
 * */
static int txtfile_isvalid(txtfile_t* tf)
{
   if (!tf || !tf->content || !tf->name)   //File nonexistent or not opened
      return ERR_COMMON_FILE_INVALID;
   if (!tf->hdrname || !tf->tag_begin_sufx || !tf->tag_end_sufx || !tf->tag_prfx
         || !tf->bodyname || !tf->strfieldid || !tf->numfieldid
         || !tf->field_name_separator) {
      //Missing delimiters
      ERRMSG("Unable to parse file %s: delimiters were not declared\n",
            tf->name);
      return EXIT_FAILURE; /**\todo TODO (2015-07-01) USE SPECIFIC ERROR CODE REFERENCED IN maqaoerrs.h - possibly depending on what is missing*/
   }
   if (tf->field_delim == tf->listfield_delim) {
      ERRMSG(
            "Unable to parse file %s: field delimiters and list elements delimiters are identical\n",
            tf->name);
      return EXIT_FAILURE; /**\todo TODO (2015-07-01) USE SPECIFIC ERROR CODE REFERENCED IN maqaoerrs.h - possibly depending on what is missing*/

   }
   /**\todo (2015-11-19) Add more coherence text to detect overlaps between delimiter characters*/
   return EXIT_SUCCESS;
}

/**
 * Looks up a given section type in a text file. This function is intended to be used after the array of section templates has been ordered
 * (after the parsing of the header)
 * \param tf The text file
 * \param scntype The name of the type of section to look up
 * \return The section or NULL if no template section has been declared for this file
 * */
static txtscn_t* txtfile_lookup_sectiontemplate(txtfile_t* tf, char* scntype)
{
   assert(tf && scntype);

   txtscn_t** template = bsearch(scntype, tf->sectiontemplates,
         tf->n_sectiontemplates, sizeof(*tf->sectiontemplates),
         txtscn_type_cmp_bsearch);

   return (template) ? *template : NULL;
}

/**
 * Checks if a given section template has been declared in a text file.
 * This function is intended to be used when the array of section templates has not been ordered (during the parsing of the header)
 * \param tf The text file
 * \param scntype The name of the type of section to look up
 * \return TRUE if a section template with that name exists, FALSE otherwise
 * */
static int txtfile_scntmpl_exists(txtfile_t* tf, char* scntype)
{
   assert(tf && scntype);
   uint16_t i;

   for (i = 0; i < tf->n_sectiontemplates; i++) {
      if (str_equal(scntype, tf->sectiontemplates[i]->type))
         return TRUE;
   }
   return FALSE;
}

/**
 * Looks up a given field in a section
 * \param ts The section
 * \param fieldname The name of the field to look up
 * \return The field or NULL if no such field exists for the section
 * */
static txtfield_t* txtscn_lookupfield(const txtscn_t* ts, char* fieldname)
{
   assert(ts && fieldname);

   txtfield_t** field = bsearch(fieldname, ts->fields, ts->n_fields,
         sizeof(*ts->fields), txtfield_name_cmp_bsearch);

   return (field) ? *field : NULL;
}

/**
 * Looks up a given field in a template section
 * \param ts The section
 * \param fieldname The name of the field to look up
 * \return The field or NULL if no such field exists for the section
 * */
static txtfield_t* txtscntmpl_lookupfield(const txtscn_t* ts, char* fieldname)
{
   assert(ts && fieldname);

   if (ts->matchfielmethod == MATCH_BYNAME) {
      txtfield_t** field = bsearch(fieldname, ts->fields, ts->n_fields,
            sizeof(*ts->fields), txtfield_name_cmp_bsearch);
      return (field) ? *field : NULL;
   } else {
      //The fields in a template section not matched by name are not ordered: we need to scan them one by one
      uint16_t i;
      for (i = 0; i < ts->n_fields; i++)
         if (str_equal(ts->fields[i]->name, fieldname))
            return ts->fields[i];
      return NULL;
   }

}

/**
 * Checks if the declaration of a section in a text file is valid
 * \param ts The section
 * \return EXIT_SUCCESS if it's OK, error code otherwise
 * */
static int txtscn_checkdeclaration(txtscn_t* ts)
{
   assert(ts);
   uint16_t i;
   if (ts->n_fields == 0)
      return ERR_COMMON_TXTFILE_SECTION_EMPTY;

   if (ts->matchfielmethod == MATCH_BYPOS) {
      //Matching fields by position: checking if there are no optional fields that can't be distinguished from the others
      //Optional fields must have a prefix
      if (ts->fields[0]->optional == TRUE && ts->fields[0]->prefix == 0)
         return ERR_COMMON_TXTFILE_OPTIONAL_FIELDS_CONFUSION;
      for (i = 1; i < ts->n_fields; i++) {
         //Optional fields must have a prefix
         if (ts->fields[i]->optional == TRUE && ts->fields[i]->prefix == 0)
            return ERR_COMMON_TXTFILE_OPTIONAL_FIELDS_CONFUSION;
         //We have to check that the previous field is not optional or has a different prefix
         if (ts->fields[i - 1]->optional == TRUE
               && ts->fields[i - 1]->prefix == ts->fields[i]->prefix)
            return ERR_COMMON_TXTFILE_OPTIONAL_FIELDS_CONFUSION;
         /**\todo TODO (2015-07-02) This is incomplete, I'm sure I had counted more cases but right now I don't remember them*/
      }
   } else if (ts->matchfielmethod == MATCH_BYNAME) {
      /**\todo (2015-07-02) If we need performance, at this point we could use a hashtable for storing the fields, but I fear
       * it may be overkill*/
      //Matching fields by name: checking if there are no fields with an identical name
      //Sorts the fields
      qsort(ts->fields, ts->n_fields, sizeof(*ts->fields), txtfield_cmp_qsort);
      //Checks if a field is identical to the previous one
      for (i = 1; i < ts->n_fields; i++) {
         if (str_equal(ts->fields[i]->name, ts->fields[i - 1]->name))
            return ERR_COMMON_TXTFILE_FIELD_NAME_DUPLICATED;
      }
   } // I don't think there is any test to do for aligned sections, but who knows
   return EXIT_SUCCESS;
}

/// Creators and destructors ///
////////////////////////////////

/**
 * Creates a new num_t structure
 * \return Blank structure for representing a number in a text file
 * */
static num_t* num_new()
{
   num_t* num = lc_malloc0(sizeof(*num));
   num->size = 32;
   return num;
}

/**
 * Creates a new num_t structure from a template
 * \param template Template to use. If NULL, num_new will be invoked
 * \param value Value of the field
 * \return Blank structure for representing a number in a text file
 * */
static num_t* num_new_fromtemplate(num_t* template, int64_t value)
{
   num_t* num = num_new();
   if (template) {
      num->base = template->base;
      num->isunsigned = template->isunsigned;
      num->size = template->size;
   }
   num->value = value;
   return num;
}

/**
 * Frees a num_t structure
 * \param num The num_t structure to free
 * */
static void num_free(void* n)
{
   if (!n)
      return;
   num_t* num = n;
   //Freeing the number
   lc_free(num);
}

/**
 * Creates a new txtfield_t structure
 * \param name Name of the field
 * \param type Type of the field
 * \param content Content of the field
 * \return Blank structure for representing a field in a text file
 * */
static txtfield_t* txtfield_new(char* name, txtfieldtype_t type, void* content)
{
   txtfield_t* field = lc_malloc0(sizeof(*field));
   field->name = lc_strdup(name);
   field->type = type;
   switch (type) {
   case FIELD_TXT:
      field->field.txt = content;
      break;
   case FIELD_NUM:
      field->field.num = content;
      break;
   case FIELD_SCNPROPERTY:
   case FIELD_MAX:
   default:
      assert(0);  //We should not create fields for these kind of entries
      break;
   }
   return field;
}

/**
 * Frees a txtfield_t structure
 * \param f The txtfield_t structure to free
 * */
static void txtfield_free(void* f)
{
   if (!f)
      return;
   txtfield_t* tf = f;
   //Freeing the field value
   switch (tf->type) {
   case FIELD_TXT:
      lc_free(tf->field.txt);
      break;
   case FIELD_NUM:
      num_free(tf->field.num);
      break;
   }
   //Freeing the field name
   if (tf->name)
      lc_free(tf->name);

   //Freeing the field
   lc_free(tf);
}

/**
 * Creates a new txtscn_t structure
 * \param type Name of the type of the section
 * \return Blank structure for representing a section in a text file
 * */
static txtscn_t* txtscn_new(char* type)
{
   txtscn_t* ts = lc_malloc0(sizeof(*ts));
   ts->type = lc_strdup(type);
   return ts;
}

/**
 * Frees a txtscn_t structure
 * \param s The txtscn_t structure to free
 * */
static void txtscn_free(void* s)
{
   if (!s)
      return;
   txtscn_t* ts = s;
   uint16_t i;
   //Freeing the fields
   for (i = 0; i < ts->n_fields; i++)
      txtfield_free(ts->fields[i]);
   lc_free(ts->fields);
   //Freeing the type
   if (ts->type)
      lc_free(ts->type);

   //Freeing the section
   lc_free(ts);
}

/**
 * Creates a new txtfile_t structure
 * \param name Name of the file
 * \param content Content of the file
 * \param stream File stream
 * \param contentlen Size of the content
 * \return Blank structure for representing a text file
 * */
static txtfile_t* txtfile_new(char* name, char* content, FILE *stream,
      size_t contentlen)
{
   txtfile_t *tf = lc_malloc0(sizeof(*tf));
   if (name)
      tf->name = lc_strdup(name);
   tf->content = content;
   tf->stream = stream;
   tf->contentlen = contentlen;
   tf->cursor = tf->content;
   tf->line = 1;
   txtfile_initdefault(tf);
   return tf;
}

/**
 * Frees a txtfile_t structure
 * \param tf The txtfile_t structure to free
 * */
static void txtfile_free(txtfile_t* tf)
{
   if (!tf)
      return;
   uint32_t i;
   //Freeing the fields
   if (tf->hdrname)
      lc_free(tf->hdrname);
   if (tf->tag_prfx)
      lc_free(tf->tag_prfx);
   if (tf->tag_begin_sufx)
      lc_free(tf->tag_begin_sufx);
   if (tf->tag_end_sufx)
      lc_free(tf->tag_end_sufx);
   if (tf->commentline)
      lc_free(tf->commentline);
   if (tf->commentbegin)
      lc_free(tf->commentbegin);
   if (tf->commentend)
      lc_free(tf->commentend);
   if (tf->bodyname)
      lc_free(tf->bodyname);
   if (tf->strfieldid)
      lc_free(tf->strfieldid);
   if (tf->numfieldid)
      lc_free(tf->numfieldid);
   if (tf->propfieldid)
      lc_free(tf->propfieldid);
   if (tf->scndecl_interleaved)
      lc_free(tf->scndecl_interleaved);
   if (tf->scndecl_matchfieldbyname)
      lc_free(tf->scndecl_matchfieldbyname);
   if (tf->scndecl_matchfieldbypos)
      lc_free(tf->scndecl_matchfieldbypos);
   for (i = 0; i < tf->n_sectiontemplates; i++)
      txtscn_free(tf->sectiontemplates[i]);
   lc_free(tf->sectiontemplates);
   for (i = 0; i < tf->n_sections; i++)
      txtscn_free(tf->sections[i]);
   lc_free(tf->sections);
   for (i = 0; i < tf->n_bodylines; i++)
      txtscn_free(tf->bodylines[i]);
   lc_free(tf->bodylines);

   //Unloading the contents and closing the file
   if (tf->stream)
      releaseFileContent(tf->content, tf->stream);
   else if (tf->content)
      lc_free(tf->content);   //File was created from a string

   //Freeing the name
   if (tf->name)
      lc_free(tf->name);

   //Freeing the structure
   lc_free(tf);
}

/*
 * Opens a text file to be parsed and returns a txtfile_t structure
 * \param filename Name of the file
 * \return A new txtfile_t structure containing the contents of the file or NULL ifs filename is NULL or the file not found
 * */
txtfile_t* txtfile_open(char* filename)
{
   /**\todo (2015-07-01) Add a mode detecting the size of the file, and if too large, use FILE* and fread/fseek/ftell and the likes
    * to parse it, instead of always loading its content into a string*/
   FILE* stream = NULL;
   size_t contentlen = 0;
   char* content = getFileContent(filename, &stream, &contentlen);
   if (!content) {
      ERRMSG("Unable to open file %s\n", filename);
      return NULL;
   }
   //Creates the structure
   txtfile_t* tf = txtfile_new(filename, content, stream, contentlen);

   return tf;
}

/*
 * Loads a text file from a string representing its contents and returns a txtfile_t structure
 * \param content String containing the content of the file
 * \return A new txtfile_t structure containing the contents of the file
 * */
txtfile_t* txtfile_load(char* content)
{
   //Creates the structure
   txtfile_t* tf = txtfile_new(NULL, lc_strdup(content), NULL, strlen(content));
   return tf;
}

/*
 * Closes a text file and frees its contents
 * \param Structure representing the text file
 * \return EXIT_SUCCESS / error code
 * */
int txtfile_close(txtfile_t* tf)
{
   if (!tf)
      return EXIT_FAILURE;
   txtfile_free(tf);
   return EXIT_SUCCESS;
}

/// Parsing functions ///
/////////////////////////

/**
 * Parses a field declaration in the header
 * \param tf Structure representing the text file
 * \param txtscn_t* ts The section to which the field belongs
 * \param endscn Pointer to the laster character of the section header
 * \param beginfields First character of the fields for sections containing aligned fields
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int txtfile_parsedecl_field(txtfile_t* tf, txtscn_t* ts, char *endscn,
      char* beginfields)
{
   assert(tf && ts && endscn);
   int retcode;

   if (tf->cursor == endscn)
      return EXIT_SUCCESS; //Found the end of the declaration
   char prefix = 0;
   int optional = FALSE;
   int islist = FALSE;
   txtfieldtype_t type;
   txtfield_t* field = NULL;
   char *match;
   char* fieldid;
   void* content = NULL;
   char* beginfield;
   //Attempting to detect an optional field prefix
   if (str_beginwith(tf->cursor, tf->optfield_prefix)) {
      optional = TRUE;
      tf->cursor += strlen(tf->optfield_prefix);
   }
   //Detecting declarations
   if (str_beginwith(tf->cursor, tf->propfieldid)) {
      //Declaration of a section property
      type = FIELD_SCNPROPERTY;
      fieldid = tf->propfieldid;
   } else if (str_beginwith(tf->cursor, tf->strfieldid)) {
      //String field
      type = FIELD_TXT;
      fieldid = tf->strfieldid;
   } else if (str_beginwith(tf->cursor, tf->numfieldid)) { //Attempting to detect the declaration of a numerical field
      //Numerical field
      type = FIELD_NUM;
      fieldid = tf->numfieldid;
   } else {
      return ERR_COMMON_TXTFILE_FIELD_IDENTIFIER_UNKNOWN;
   }
   //Saving the index of the field
   beginfield = tf->cursor;
   //Processing the field
   //Skipping to the end of the declaration
   tf->cursor += strlen(fieldid);
   //Detecting if suffixes are present
   if (type == FIELD_TXT || type == FIELD_NUM) {
      /**\todo If we add more suffixes, we will have to loop until we find a character not registered as a suffix*/
      //Detecting the suffix signaling that the character is a list is present
      if (*tf->cursor == tf->fieldidsuf_list) {
         islist = TRUE;
         tf->cursor++;
      }
   }
   //Skips spaces after the end
   SKIP_SPACES(retcode);
   //Checks that the declaration separator follows
   if (*tf->cursor != tf->field_name_separator)
      return ERR_COMMON_TXTFILE_FIELD_SEPARATOR_NOT_FOUND;
   tf->cursor++;  //Skips the separator
   //Skips spaces after the separator
   SKIP_SPACES(retcode);
   //Detects if there is a special character signaling the field
   if (!character_isalphanumeric(*tf->cursor)) {
      prefix = *tf->cursor++;
   }
   //Finds the end of the field name
   match = strchr(tf->cursor, tf->decl_field_delim);
   while (*match != tf->decl_field_delim && match != endscn)
      match++;
   //Reads the name of the field
   GET_SUBSTRING(fieldname, match);
   //Now special handling depending on the type
   if (type == FIELD_SCNPROPERTY) {
      DBGMSGLVL(1, "Found property %s for section declaration %s\n", fieldname,
            ts->type)
      //First check: if we encounter a property in a section declared as aligned, there is an error
      if (ts->matchfielmethod == MATCH_BYALIGN)
         return ERR_COMMON_TXTFILE_FIELD_UNAUTHORISED;
      //Checking that the property has one of the allowed names
      if (str_equal(fieldname, tf->scndecl_interleaved))
         ts->interleaved = TRUE;
      else if (str_equal(fieldname, tf->scndecl_matchfieldbyname)) {
         if (ts->matchfielmethod != MATCH_UNDEF
               && ts->matchfielmethod != MATCH_BYNAME)
            return ERR_COMMON_TXTFILE_PROPERTIES_MUTUALLY_EXCLUSIVE;
         ts->matchfielmethod = MATCH_BYNAME;
      } else if (str_equal(fieldname, tf->scndecl_matchfieldbypos)) {
         if (ts->matchfielmethod != MATCH_UNDEF
               && ts->matchfielmethod != MATCH_BYPOS)
            return ERR_COMMON_TXTFILE_PROPERTIES_MUTUALLY_EXCLUSIVE;
         ts->matchfielmethod = MATCH_BYPOS;
      } else if (str_equal(fieldname, tf->scndecl_matchfieldbyalign)) {
         if (ts->matchfielmethod != MATCH_UNDEF
               && ts->matchfielmethod != MATCH_BYALIGN)
            return ERR_COMMON_TXTFILE_PROPERTIES_MUTUALLY_EXCLUSIVE;
         if (ts->n_fields > 0)
            return ERR_COMMON_TXTFILE_FIELD_ALIGNMENT_NOT_RESPECTED;
         ts->matchfielmethod = MATCH_BYALIGN;
      } else {
         return ERR_COMMON_TXTFILE_SECTION_PROPERTY_UNKNOWN;
      }
   } else if (type == FIELD_NUM) {
      char* c = fieldname, *endnumfieldname;
      //Creates the field
      num_t* numfield = num_new();
      //Skips the name of the field
      while (*c != '\0' && *c != tf->field_name_separator)
         c++;
      GET_SUBSTRING_INSTRING(numfieldname, fieldname, c);
      endnumfieldname = c; //Stores the index of the end of the name
      if (*c != '\0')
         c++;  //Skips the field separator
      //Reads the fields of the field
      while (*c != '\0') {
         int value, i;
         char *beginsubfield;
         char oldc;
         if (*c == tf->field_name_separator) {
            c++; //Separator found: skipping it (I don't quite see how that would happen)
         } else if (*c == tf->numdecl_size) {
            //Reads the size number. Yes it's gory
            c++;
            beginsubfield = c;
            while (*c != '\0' && *c != tf->field_name_separator)
               c++;
            oldc = *c;
            *c = '\0'; //Temporarily closing the string there so we can use atoi
            value = atoi(beginsubfield);
            if (value == 0) {
               return ERR_COMMON_INTEGER_SIZE_INCORRECT;
            }
            numfield->size = value;
            DBGMSGLVL(2, "Numerical field %s has size %d\n", numfieldname,
                  value)
            *c = oldc;  //Restoring the string
            if (*c != '\0')
               c++;
         } else if (*c == tf->numdecl_unsigned) {
            //Reading the unsigned value
            numfield->isunsigned = TRUE;
            DBGMSGLVL(2, "Numerical field %s is unsigned\n", numfieldname)
            c++;
         } else {
            //Attempting to recognise one base identifier
            for (i = 0; i < BASE_MAX; i++) {
               if (*c == tf->numdecl_base[i]) {
                  numfield->base = i;
                  DBGMSGLVL(2, "Numerical field %s has base identifier %d\n",
                        numfieldname, i)
                  c++;
                  break;
               }
            }
            if (i == BASE_MAX) {
               num_free(numfield);
               return ERR_COMMON_UNEXPECTED_CHARACTER;
            }
         }
      }
      //Updates the name of the field by truncating the additional properties
      *endnumfieldname = '\0';
      content = numfield;
   } //Nothing else to be done for text fields

   //Skips to the end of the field
   retcode = txtfile_updcursor(tf, match);
   if (ISERROR(retcode))
      return retcode;
   SKIP_SPACES(retcode);

   if (type == FIELD_TXT || type == FIELD_NUM) {
      DBGMSGLVL(1,
            "Found declaration of %s field %s at line %u in section %s\n",
            (type == FIELD_TXT) ? "text" : "num", fieldname, tf->line, ts->type)
      //Creates the field
      field = txtfield_new(fieldname, type, content);
      field->optional = optional;
      field->prefix = prefix;
      field->list = islist;
      //Stores the index of the field for aligned sections
      if (ts->matchfielmethod == MATCH_BYALIGN)
         field->posinline = (uint32_t) (beginfield - beginfields);
      //Adds the field to the array
      ADD_CELL_TO_ARRAY(ts->fields, ts->n_fields, field);
   }

   return EXIT_SUCCESS;
}

/**
 * Parses a section declaration in the header
 * \param tf Structure representing the text file
 * \param endhdr Pointer to the last character of the header
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int txtfile_parsedecl_section(txtfile_t* tf, char* endhdr)
{
   assert(tf && endhdr);
   int retcode;
   char* beginfields = NULL;   //Beginning of the field for the aligned sections
   SKIP_SPACES(retcode);
   //Look up for the beginning of a section tag
   char* match = strstr(tf->cursor, tf->tag_prfx);
   if (!match || match >= endhdr) {
      retcode = EXIT_SUCCESS; /**\todo TODO (2015-07-02) Use WRN_COMMON_TXTFILE_HEADER_COMPLETED*/
      return retcode; //Reached the end before finding a definition
   }
   txtfile_updcursor(tf, match + strlen(tf->tag_prfx));
//   match = strstr(tf->cursor, tf->tag_begin_sufx);
//   if (match != tf->cursor) {
//      WARNING("Ignoring %ld characters before beginning of section declaration at line %u in header from file %s\n",
//            match - tf->cursor, tf->line, tf->name);
//      retcode = EXIT_SUCCESS; /**\todo TODO (2015-07-01) Use WRN_COMMON_TXTFILE_IGNORING_CHARACTERS instead*/
//   }
//   txtfile_updcursor(tf, match);

   //Finds the end of the tag
   match = strstr(tf->cursor, tf->tag_begin_sufx);
   if (!match) {
      return ERR_COMMON_TXTFILE_TAG_END_NOT_FOUND;
   }
   //Retrieves the type of the section
   GET_SUBSTRING(scntype, match);

   //Updates the cursor to point at the end of the begin tag
   retcode = txtfile_updcursor(tf, match + strlen(tf->tag_begin_sufx));
   if (ISERROR(retcode))
      return retcode;

   //Looks up for the end tag of the section
   BUILD_END_TAG(scnendtag, scntype);
   char *endscn = strstr(tf->cursor, scnendtag);
   if (!endscn || endscn > endhdr) {
      return ERR_COMMON_TXTFILE_SECTION_END_NOT_FOUND;
   }
   DBGMSG(
         "Parsing declaration of section %s at line %u and containing %zu characters\n",
         scntype, tf->line, endscn - tf->cursor)
   //Creates the section
   txtscn_t* ts = txtscn_new(scntype);
   ts->line = tf->line;

   SKIP_SPACES(retcode);
   //Now scans the section for detecting fields
   while (tf->cursor < endscn) {
      retcode = txtfile_parsedecl_field(tf, ts, endscn, beginfields);
      if (ts->matchfielmethod == MATCH_BYALIGN && beginfields == NULL) {
         //If we just discovered the property that the section matches fields by alignments, we store the beginning of the fields
         beginfields = tf->cursor;
      }
      if (ISERROR(retcode)) {
         return retcode;
      }
   }
   //Skips to the end of the section declaration
   txtfile_updcursor(tf, endscn + strlen(scnendtag));

   //Sanity check with regard to the fields
   retcode = txtscn_checkdeclaration(ts);
   if (ISERROR(retcode))
      return retcode;

   //Adds the section definition to the file
   if (!txtfile_scntmpl_exists(tf, scntype)) {
      ADD_CELL_TO_ARRAY(tf->sectiontemplates, tf->n_sectiontemplates, ts);
   } else
      return ERR_COMMON_TXTFILE_SECTION_DUPLICATED;

   return EXIT_SUCCESS;
}

/**
 * Parses the main header in a text file
 * \param tf Structure representing the text file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
static int txtfile_parseheader(txtfile_t* tf)
{
   int retcode = txtfile_isvalid(tf);
   if (ISERROR(retcode)) {
      return retcode;
   }
   DBGMSG("Parsing header of file %s\n", tf->name)

   //Building the name of the header begin and end tags
   BUILD_BEGIN_TAG(hdrbegintag, tf->hdrname)
   BUILD_END_TAG(hdrendtag, tf->hdrname)

   //Skipping any spaces at the beginning
   SKIP_SPACES(retcode);

   //Looking for the beginning of the header
   char* match = strstr(tf->cursor, hdrbegintag);
   if (match != tf->cursor) {
      WRNMSG("Ignoring %zd characters before beginning of header in file %s\n",
            match - tf->cursor, tf->name);
      retcode = EXIT_SUCCESS; /**\todo TODO (2015-07-01) Use WRN_COMMON_TXTFILE_IGNORING_CHARACTERS*/
   }
   //Skips the tag signaling the beginning of the header
   txtfile_updcursor(tf, match + strlen(hdrbegintag));
   //Finds the end of the header
   char* endhdr = strstr(tf->cursor, hdrendtag);
   if (!endhdr) {
      return ERR_COMMON_TXTFILE_HEADER_END_NOT_FOUND;
   }
   DBGMSG("File header found at line %u and containing %zu characters\n",
         tf->line, endhdr - tf->cursor)
   //Scans the header for sections declarations
   while (tf->cursor < endhdr) {
      retcode = txtfile_parsedecl_section(tf, endhdr);
      if (ISERROR(retcode)) {
         return retcode;
      }
   }
   //Skips to the end of the header declaration
   txtfile_updcursor(tf, endhdr + strlen(hdrendtag));

   //Orders the array of sections templates by name
   qsort(tf->sectiontemplates, tf->n_sectiontemplates,
         sizeof(*tf->sectiontemplates), txtscn_cmp_qsort);

   //Checks that we found at least one section definition in the header
   if (tf->sectiontemplates == 0)
      return ERR_COMMON_TXTFILE_HEADER_EMPTY;
   //Checks that there is a definition for the body
   if (!txtfile_lookup_sectiontemplate(tf, tf->bodyname))
      return ERR_COMMON_TXTFILE_BODY_DEFINITION_NOT_FOUND;

   return retcode;
}

/**
 * Parses a field in a string and adds it to a section. Invoked by txtfile_parse_field for factorisation
 * \param tf The text file
 * \param ts The section to which the field belongs.
 * \param template Template if the field
 * \param fieldvalue String containing the value of the field
 * */
static int txtfile_parse_fieldvalue(txtfile_t* tf, txtscn_t* ts,
      txtfield_t* template, char* fieldvalue)
{
   assert(tf && ts && template && fieldvalue);
   txtfield_t* field = NULL;
   //Processes the value depending on its type
   if (template->type == FIELD_TXT) {
      //Text field: easy case
      DBGMSGLVL(2,
            "Parsed text field of type %s with value %s in section %s at line %u\n",
            template->name, fieldvalue, ts->type, tf->line)
      field = txtfield_new(template->name, template->type,
            lc_strdup(fieldvalue));
   } else if (template->type == FIELD_NUM) {
      char* formatstr = NULL;
      int64_t value = 0;
      //Numerical field
      switch (template->field.num->base) {
      case BASE_10:
         if (template->field.num->isunsigned)
            formatstr = "%"PRIu64;
         else
            formatstr = "%"PRId64;
         break;
      case BASE_16:
         formatstr = "%"PRIx64;
         break;
      case BASE_08:
         formatstr = "%"PRIo64;
         break;
      case BASE_02:
         /**\todo TODO (2015-07-03) Implement it, but I don't have the time right now*/
         ERRMSG(
               "Reading of number in base 2 currently not supported by this parser\n")
         ;
         break;
      default:
         return ERR_COMMON_NUMERICAL_BASE_NOT_SUPPORTED;
      }
      //Reads the value
#ifdef _WIN32
      sscanf_s(fieldvalue, formatstr, &value);
#else
      sscanf(fieldvalue, formatstr, &value);
#endif
      DBGMSGLVL(2,
            "Parsed num field of type %s with value %#zx in section %s at line %u\n",
            template->name, value, ts->type, tf->line)
      //Creates the field
      num_t* fieldval = num_new_fromtemplate(template->field.num, value);
      field = txtfield_new(template->name, template->type, fieldval);
   }
   if (field) {
      //If the field was correctly parsed, adding it to the array of fields
      ADD_CELL_TO_ARRAY(ts->fields, ts->n_fields, field);
   }
   return EXIT_SUCCESS;
}

/**
 * Parses a field in a text file and adds it to the section to which it belongs
 * \param tf The text file
 * \param ts The section to which the field belongs.
 * \param template Template if the field
 * \param endline If not NULL, signals that this is the last field of the line and everything must be parsed until this line
 * \param endscn Beginning of the tag signaling the end of the section containing the field (can be NULL)
 * \return EXIT_SUCCESS or error code
 * */
static int txtfile_parse_field(txtfile_t* tf, txtscn_t* ts,
      txtfield_t* template, char* endline, char* endscn)
{
   assert(tf && ts && template);
   int res = EXIT_SUCCESS;
   //Skipping the prefix
   if (template->prefix != '\0') {
      if (*tf->cursor != template->prefix)
         return ERR_COMMON_TXTFILE_FIELD_PREFIX_NOT_FOUND;
      tf->cursor++;
   }
   //Now finding the end of the field
   char* endfield = tf->cursor;
   //Choose the delimiter of the end of the field if it begins with a text delimiter
   if (*tf->cursor == tf->txtfield_delim) {
      //Fields begins with a text delimiter
      endfield = ++tf->cursor;
      //Finding the end of the field
      while (*endfield != '\0' && endfield != endscn
            && *endfield != tf->txtfield_delim)
         endfield++;
   } else if (endline) {
      //Skips to the end of the line
      while (endfield != endline) {
         if (str_beginwith(endfield, tf->commentline)) {
            DBGMSG("Skipping commented line %d\n", tf->line)
            //Comment delimiter found: skipping the remainder of the line
            endfield--;
            break;
         }
         endfield++;
      }
   } else {
      //Finds the end of the field
      while (*endfield != '\0' && endfield != endscn
            && character_isblankspace(*endfield) == FALSE)
         endfield++;
   }
   if (endfield == '\0')
      return ERR_COMMON_TXTFILE_FIELD_ENDING_NOT_FOUND;
   if (endfield == tf->cursor && template->optional == FALSE)
      return ERR_COMMON_TXTFILE_MISSING_MANDATORY_FIELD;

   //Retrieves the field value
   GET_SUBSTRING(fieldvalue, endfield);

   //Parses the field value and stores it into the section
   if (template->list == TRUE) {
      //Field is a list: we have to split the string on the delimiters and parse each substring
      size_t c = 0;
      int finished = FALSE;
      while (finished == FALSE) {
         //Stores the beginning of the current sub string
         char* begin = fieldvalue + c;
         //Finds the next separator or the end of the field
         while (fieldvalue[c] != '\0' && fieldvalue[c] != tf->listfield_delim)
            c++;
         //Checking if we have reached the end of the main string
         if (fieldvalue[c] == '\0')
            finished = TRUE;  //We will stop after processing this sub string
         else
            fieldvalue[c] = '\0';   //Cuts the main string there
         //Builds the field from the sub string
         res = txtfile_parse_fieldvalue(tf, ts, template, begin);
         if (ISERROR(res))
            return res;
         c++;  //Skips to the next character
      }
   } else {
      //Single value: parsing it and storing it
      res = txtfile_parse_fieldvalue(tf, ts, template, fieldvalue);
      if (ISERROR(res))
         return res;
   }

   //Skips to the end of the field
   txtfile_updcursor(tf, endfield);

   return res;
}

/**
 * Parses a section in a text file depending on a template
 * \param tf Text file
 * \param template The section template
 * \param section Return parameter, will contain the parsed section if no error occurred
 * \param endscn Last character of the section
 * */
static int txtfile_parse_section(txtfile_t* tf, txtscn_t* template,
      txtscn_t** section, char* endscn)
{
   assert(tf && template && section && endscn);
   int retcode;
   //Initialises the section
   txtscn_t* ts = txtscn_new(template->type);
   ts->line = tf->line;
   char* beginscn = tf->cursor;

   if (template->matchfielmethod == MATCH_BYPOS) {
      //Section fields are matched by position
      uint16_t i;
      for (i = 0; i < template->n_fields; i++) {
         SKIP_SPACES(retcode);
         //Skips optional fields whose prefix is not met
         while (i < template->n_fields && template->fields[i]->optional
               && (*tf->cursor != template->fields[i]->prefix))
            i++;
         if (i == template->n_fields) {
            //Reached the max number of fields:exiting
            SKIP_SPACES(retcode);
            if (tf->cursor < endscn)
               return ERR_COMMON_TXTFILE_SECTION_TOO_MANY_FIELDS;
            break;
         }
         //Attempts to parse the field
         retcode = txtfile_parse_field(tf, ts, template->fields[i],
               (i == (template->n_fields - 1)) ? endscn : NULL, endscn);
         if (ISERROR(retcode))
            return retcode;
      }
   } else if (template->matchfielmethod == MATCH_BYNAME) {
      //Section fields are matched by name
      while (tf->cursor < endscn) {
         txtfield_t* fieldtemplate = NULL;
         SKIP_SPACES(retcode);
         if (tf->cursor == endscn)
            break;   //We reached the end of the section
         /**\todo (2015-07-22) Add a check that all the mandatory fields have been found there*/
         char* endname = strchr(tf->cursor, tf->field_name_separator);
         if (!endname || endname > endscn)
            return ERR_COMMON_TXTFILE_FIELD_SEPARATOR_NOT_FOUND;
         //Reads the name of the field
         GET_SUBSTRING(fieldname, endname)
         //Looks up the field in the fields of the section
         fieldtemplate = txtscn_lookupfield(template, fieldname);
         if (!fieldtemplate)
            return ERR_COMMON_TXTFILE_FIELD_NAME_UNKNOWN;
         //Skips cursor to the end of the field name (including the separator)
         txtfile_updcursor(tf, endname + 1);
         //Attempts to parse the field
         retcode = txtfile_parse_field(tf, ts, fieldtemplate, NULL, endscn);
         if (ISERROR(retcode))
            return ERR_COMMON_TXTFILE_FIELD_PARSING_ERROR;
      }
   } else if (template->matchfielmethod == MATCH_BYALIGN) {
      //Section fields are matched by alignment
      uint16_t i;
      for (i = 0; i < template->n_fields; i++) {
         //Reaches the index where the field is supposed to be
         retcode = txtfile_updcursor(tf,
               beginscn + template->fields[i]->posinline);
         if (ISERROR(retcode))
            return retcode;
         //Attempt to parse the field
         retcode = txtfile_parse_field(tf, ts, template->fields[i], NULL,
               endscn);
         if (ISERROR(retcode) && template->fields[i]->optional == FALSE)
            return retcode; //Exits only if the field was not correctly parsed and mandatory
      }

   }
   //Sorts the array of fields by name
   qsort(ts->fields, ts->n_fields, sizeof(*ts->fields), txtfield_cmp_qsort);
   *section = ts;
   return EXIT_SUCCESS;
}

/**
 * Parses a section in a text file.
 * \param tf Structure representing the text file to parse
 * \return EXIT_SUCCESS if successful, error code otherwise or if no more sections are available.
 * */
static int txtfile_parse_nextsection(txtfile_t* tf)
{
   int retcode = txtfile_isvalid(tf);
   if (ISERROR(retcode))
      return retcode;
   txtscn_t* ts = NULL;

   /**\todo TODO (2015-07-02) Factorise the following code, I already used it to parse the declarations. Beware however of the return codes*/
   //Looks up the beginning of a section
   SKIP_SPACES(retcode);
   char* match = strstr(tf->cursor, tf->tag_prfx);
   if (!match) {
      retcode = EXIT_SUCCESS; /**\todo TODO (2015-07-02) Use WRN_COMMON_TXTFILE_NO_SECTIONS_REMAINING*/
      return retcode; //No sections remaining
   }
   txtfile_updcursor(tf, match + strlen(tf->tag_prfx));

//   //Look up for the beginning of a section tag
//   match = strstr(tf->cursor, tf->tag_begin_sufx);
//   if (match != tf->cursor) {
//      WARNING("Ignoring %ld characters before beginning of section from file %s\n", match - tf->cursor, tf->name);
//      retcode = 0; /**\todo TODO (2015-07-01) Use WRN_COMMON_TXTFILE_IGNORING_CHARACTERS instead*/
//   }
//   txtfile_updcursor(tf, match);
   //Finds the end of the tag
   match = strstr(tf->cursor, tf->tag_begin_sufx);
   if (!match) {
      return ERR_COMMON_TXTFILE_TAG_END_NOT_FOUND;
   }
   //Retrieves the type of the section
   GET_SUBSTRING(scntype, match);
   DBGMSG("Found section with type %s at line %u\n", scntype, tf->line)

   //Looks up the type in the array of recognised sections
   txtscn_t* template = txtfile_lookup_sectiontemplate(tf, scntype);

   if (!template)
      return ERR_COMMON_TXTFILE_SECTION_TYPE_UNKNOWN;

   //Updates the cursor to point at the end of the begin tag
   retcode = txtfile_updcursor(tf, match + strlen(tf->tag_begin_sufx));
   if (ISERROR(retcode))
      return retcode;

   //Looks up for the end tag of the section
   BUILD_END_TAG(scnendtag, scntype);
   char *endscn = strstr(tf->cursor, scnendtag);
   if (!endscn) {
      retcode = ERR_COMMON_TXTFILE_SECTION_END_NOT_FOUND;
      return retcode;
   }
   if (str_equal(template->type, tf->bodyname)) {
      //This is the body of the file: special handling, we will be reading line by line
      queue_t* lastinterleaveds = queue_new(); //Section storing pending interleaved sections
      /**\todo (2015-07-22) We could use a stack or an array_t structure here instead, to reduce the number of malloc/free*/
      SKIP_SPACES(retcode)
      while (tf->cursor < endscn) {
         if (tf->cursor < endscn && str_beginwith(tf->cursor, tf->tag_prfx)) {
            DBGMSG("Parsing interleaved section at line %u\n", tf->line)
            //This is an interleaved section declaration: we will parse it, then return to our standard parsing
            retcode = txtfile_parse_nextsection(tf);
            if (ISERROR(retcode))
               return retcode;
            //Storing the interleaved section to be able to link it to the line following it
            txtscn_t* lastinterleaved = tf->sections[tf->n_sections - 1];
            lastinterleaved->interleaved = TRUE;
            queue_add_tail(lastinterleaveds, lastinterleaved);
            continue;
         }
         char* endline = txtfile_findendline(tf);
         if (!endline)
            return ERR_COMMON_TXTFILE_BODY_END_LINE_NOT_FOUND;
         DBGMSG("Parsing body line at line %u and containing %lu characters\n",
               tf->line, endline - tf->cursor)
         //Parses the line
         retcode = txtfile_parse_section(tf, template, &ts, endline);
         if (ISERROR(retcode))
            return retcode;   //Error during parsing of the section
         //Adds the line to the array of lines in the file
         ADD_CELL_TO_ARRAY(tf->bodylines, tf->n_bodylines, ts)
         while (queue_length(lastinterleaveds) > 0) {
            //There was one or more interleaved section(s) before that line: linking this line with it/them
            txtscn_t* lastinterleaved = queue_remove_tail(lastinterleaveds);
            DBGMSGLVL(1,
                  "Body line at line %u follows interleaved section of type %s at line %u\n",
                  tf->line, lastinterleaved->type, lastinterleaved->line)
            lastinterleaved->nextbodyline = ts;
         }
         //Skips to the next line
         txtfile_skipline(tf);
         SKIP_SPACES(retcode)
      }
      queue_free(lastinterleaveds, NULL);
   } else {
      DBGMSG(
            "Parsing section of type %s at line %u and containing %lu characters\n",
            template->type, tf->line, endscn - tf->cursor)
      //Standard section: simply parse its fields
      retcode = txtfile_parse_section(tf, template, &ts, endscn);
      if (ISERROR(retcode))
         return retcode;   //Error during parsing of the section
      //Adds the section to the array of sections in the file
      ADD_CELL_TO_ARRAY(tf->sections, tf->n_sections, ts)
   }
   //Skips to the end of the section
   txtfile_updcursor(tf, endscn + strlen(scnendtag));
   SKIP_SPACES(retcode)

   return EXIT_SUCCESS;
}

/*
 * Parses a text file
 * \param tf Structure representing the text file
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int txtfile_parse(txtfile_t* tf)
{
   int retcode = 0;
   retcode = txtfile_isvalid(tf);
   if (ISERROR(retcode)) {
      return retcode;
   }
   //Parsing the file header
   retcode = txtfile_parseheader(tf);
   if (ISERROR(retcode)) {
      ERRMSG("Unable to parse header of file %s\n", tf->name);
      return retcode;
   }
   //Parsing sections
   while (*tf->cursor != '\0' && *tf->cursor != EOF) {
      char *pos = tf->cursor;
      SKIP_SPACES(retcode);
      retcode = txtfile_parse_nextsection(tf);
      if (ISERROR(retcode))
         return retcode;
      //Parse and space handling did not make the cursor move
      if (tf->cursor == pos)
         break;
   }
   tf->parsed = TRUE;
   return EXIT_SUCCESS;
}

/*
 * Returns the position of the cursor inside the file. This allows to know the character at which an error occurred
 * \param tf Structure representing the text file
 * \return Pointer to the character pointed by the cursor. For a file not yet parsed, it should be the first character in the
 * file. For a successfully parsed file, it should be the end string character '\0'. Otherwise, this should be the character
 * at which the parsing stopped, or NULL if \c tf is NULL
 * */
char* txtfile_getcursor(txtfile_t* tf)
{
   return (tf) ? tf->cursor : NULL;
}

/// Getters for a parsed file ///
/////////////////////////////////

/*
 * Returns the section representing the line found at given index in the body
 * \param tf Structure representing the text file
 * \param i Index of the line to retrieve (this is NOT the line in the original file)
 * \return Section representing the line or NULL if \c tf is NULL or i is out of range
 * */
txtscn_t* txtfile_getbodyline(txtfile_t* tf, uint32_t i)
{
   if (!tf || i >= tf->n_bodylines)
      return NULL;
   return tf->bodylines[i];
}

/*
 * Returns the section not representing a body line at a given index
 * \param tf Structure representing the text file
 * \param i Index of the section to retrieve (this is the index among all sections not
 * representing a body line)
 * \return Section or NULL if \c tf is NULL or i is out of range
 * */
txtscn_t* txtfile_getsection(txtfile_t* tf, uint32_t i)
{
   if (!tf || i >= tf->n_sections)
      return NULL;
   return tf->sections[i];
}

/*
 * Returns all sections of a given type
 * \param tf Structure representing the text file
 * \param type Type of section to look for
 * \param n_scns Return parameter, will be updated to contain the number of sections
 * \return Array of sections with that type or \c NULL if either parameter is NULL or no section with that type were found
 * */
txtscn_t** txtfile_getsections_bytype(txtfile_t* tf, char* type,
      uint32_t* n_scns)
{
   /**\todo (2015-07-03) I would like to order the sections in the file as a 2 dimension array, ordered by their type (template)
    * The only thing preventing me to easily to that is that bsearch returns the template, not its index, when I'm looking for a
    * given template during parsing. When I have time, rewrite the txtfle_lookup_sectiontemplate to return the index, and change
    * tf->sections to a 2 dimensions array. This function would then simply return the array corresponding to this template*/
   if (!tf || !type || !n_scns)
      return NULL;
   txtscn_t** sections = NULL;
   uint32_t n_sections = 0;
   uint32_t i;
   for (i = 0; i < tf->n_sections; i++) {
      if (str_equal(tf->sections[i]->type, type)) {
         ADD_CELL_TO_ARRAY(sections, n_sections, tf->sections[i]);
      }
   }
   *n_scns = n_sections;
   return sections;
}

/*
 * Returns all sections of a given type sorted over a given field in the section
 * \param tf Structure representing the text file
 * \param type Type of section to look for
 * \param n_scns Return parameter, will be updated to contain the number of sections
 * \param fieldname Name of the field over which to order the sections. If NULL or not defined for this type of sections,
 * the sections will not be ordered
 * \return Array of sections with that type or \c NULL if either parameter is NULL or no section with that type were found
 * */
txtscn_t** txtfile_getsections_bytype_sorted(txtfile_t* tf, char* type,
      uint32_t* n_scns, char* fieldname)
{
   txtscn_t** sections = txtfile_getsections_bytype(tf, type, n_scns);
   if (!sections || !fieldname)
      return sections; //No field name over which to order or no sections found: returning what we retrieved

   //Checking the field name is valid (is defined for this section and not flagged as optional)
   txtscn_t* scntmpl = txtfile_lookup_sectiontemplate(tf, type);
   txtfield_t* fieldtmpl = txtscntmpl_lookupfield(scntmpl, fieldname);
   if (!fieldtmpl || fieldtmpl->optional == TRUE)
      return sections; //Field not defined for this section type or flagged as optional: unsuited for sorting
   /**\todo (2016-03-22) Find something more clean for storing which field is used for the comparison*/
   char* order_by_field_bkp = order_by_field; //Saving the current field used for ordering sections
   order_by_field = fieldname;    //Setting the field used for ordering sections
   //Sorting the array depending on its type
   switch (fieldtmpl->type) {
   case FIELD_TXT:
      qsort(sections, *n_scns, sizeof(*sections), txtscn_txtfieldcmp_qsort);
      break;
   case FIELD_NUM:
      qsort(sections, *n_scns, sizeof(*sections), txtscn_numfieldcmp_qsort);
      break;
   default:
      assert(0);
      break;
   }
   order_by_field = order_by_field_bkp; //Restoring the field used for ordering sections
   return sections;
}

/*
 * Returns the name of a text file
 * \param tf Structure representing the text file
 * \return Name of the file or NULL if \c tf is NULL
 * */
char* txtfile_getname(txtfile_t* tf)
{
   return (tf) ? tf->name : NULL;
}

/*
 * Returns a given field from a section
 * \param ts Structure representing the text section
 * \param field Name of the field to retrieve
 * \return Structure representing the field or NULL if \c ts or \c field is NULL or no field with that name exists.
 * If the field is a list, a random element from this list will be returned
 * */
txtfield_t* txtscn_getfield(const txtscn_t* ts, char* field)
{
   if (!ts || !field)
      return NULL;
   return txtscn_lookupfield(ts, field);
}

/*
 * Returns a given field from a section as a list. This function is intended to be used for fields of type list
 * \param ts Structure representing the text section
 * \param field Name of the field to retrieve
 * \param listsz Return parameter. Will be updated to contain the size of the list.
 * \return Array of the values for this field or NULL if \c ts, \c field or \c listsz is NULL or no field with that name exists.
 * The size of the array is contained in listsz. If the field is not a list, an array containing a single element from will be returned
 * */
txtfield_t** txtscn_getfieldlist(const txtscn_t* ts, char* field,
      uint16_t *listsz)
{
   if (!ts || !field || !listsz)
      return NULL;
   /**\todo (2015-11-19) We will do a stupid linear scan this time, because otherwise, we would have to:
    * 1) find the index of one cell containing this field (so, not bsearch), 2) look up backward
    * and forward of this cell to gather all the others.
    * If this linear scan proves to be too impacting the performance, either implement the algorithm above,
    * or change the way fields containing lists are handled, or use hashtables to store fields inside a section*/
   uint16_t i = 0, n_listsz = 0;
   txtfield_t** list = NULL;
   //Reaches the first field element with the required type
   while (i < ts->n_fields && strcmp(ts->fields[i]->name, field) < 0)
      i++;
   //Stores all entries with this value inside an array
   while (strcmp(ts->fields[i]->name, field) == 0) {
      ADD_CELL_TO_ARRAY(list, n_listsz, ts->fields[i]);
      i++;
   }

   *listsz = n_listsz;
   return list;
}

/*
 * Returns the next body line following a section interleaved with the body
 * \param ts Structure representing the text section
 * \return Structure representing the next line, or NULL if \c ts is NULL or not interleaved
 * */
txtscn_t* txtscn_getnextbodyline(txtscn_t* ts)
{
   return (ts && ts->interleaved) ? ts->nextbodyline : NULL;
}

/*
 * Returns the type of a section
 * \param ts Structure representing the text section
 * \return String representing the type of the section, or NULL if \c ts is NULL
 * */
char* txtscn_gettype(txtscn_t* ts)
{
   return (ts) ? ts->type : NULL;
}

/*
 * Retrieves the line at which a section was declared
 * \param ts The section
 * \return The line or 0 if the section is NULL
 * */
uint32_t txtscn_getline(txtscn_t* ts)
{
   return (ts) ? ts->line : 0;
}

/*
 * Retrieves the current line of a text file being parsed.
 * This is mainly useful for when a parsing error occurs, otherwise the current line should either be the first (file not parsed)
 * or the last (file successfully parsed)
 * \param tf The file
 * \return The line or 0 if the file is NULL
 * */
uint32_t txtfile_getcurrentline(txtfile_t* tf)
{
   return (tf) ? tf->line : 0;
}

/*
 * Returns the number of body lines in a parsed text file
 * \param tf Structure representing the text file
 * \return Number of lines in the body
 * */
uint32_t txtfile_getn_bodylines(txtfile_t* tf)
{
   return (tf) ? tf->n_bodylines : 0;
}

/*
 * Returns the number of sections in a parsed text file
 * \param tf Structure representing the text file
 * \return Number of sections in the file, excluding those representing body lines
 * */
uint32_t txtfile_getn_sections(txtfile_t* tf)
{
   return (tf) ? tf->n_sections : 0;
}

/*
 * Returns the value of a text field
 * \param field The field
 * \return The value or NULL if field is not a text field
 * */
char* txtfield_gettxt(txtfield_t* field)
{
   return (field && field->type == FIELD_TXT) ? field->field.txt : NULL;
}

/*
 * Returns the value of a numerical field
 * \param field The field
 * \return The value or 0 if field is not a numerical field
 * */
int64_t txtfield_getnum(txtfield_t* field)
{
   if (!field || field->type != FIELD_NUM)
      return 0;
   num_t* num = field->field.num;
   switch (num->size) {
   case 8:
      if (num->isunsigned)
         return (uint8_t) num->value;
      else
         return (int8_t) num->value;
      break;
   case 16:
      if (num->isunsigned)
         return (uint16_t) num->value;
      else
         return (int16_t) num->value;
      break;
   case 32:
      if (num->isunsigned)
         return (uint32_t) num->value;
      else
         return (int32_t) num->value;
      break;
   case 64:
      if (num->isunsigned)
         return (uint64_t) num->value;
      else
         return (int64_t) num->value;
      break;
   default:
      return 0;
   }
}

/// Utils for a parsed file ///
/////////////////////////////////

/*
 * Sorts the body lines in a file by a given field
 * \param tf The parsed text file. Must have already been parsed
 * \param fieldname The name of the field to use for the comparison. Must not be optional.
 * If NULL, the name of the field used for the last invocation of this function will be used, if this is the first invocation an error will be returned
 * \return EXIT_SUCCESS if successful, error code otherwise
 * */
int txtfile_sort_bodylines(txtfile_t* tf, char* fieldname)
{
   if (!tf)
      return ERR_COMMON_FILE_INVALID;
   if (tf->parsed == FALSE)
      return ERR_COMMON_TXTFILE_NOT_PARSED;

   //Setting the name of the field to use for ordering
   if (fieldname)
      order_by_field = fieldname;
   //Checking the field name exists
   if (!order_by_field)
      return ERR_COMMON_TXTFILE_FIELD_NAME_MISSING;
   //Checking the field name is valid (is defined for the body and not flagged as optional)
   txtscn_t* bodytmpl = txtfile_lookup_sectiontemplate(tf, tf->bodyname);
   txtfield_t* fieldtmpl = txtscntmpl_lookupfield(bodytmpl, order_by_field);
   if (!fieldtmpl)
      return ERR_COMMON_TXTFILE_FIELD_NAME_UNKNOWN;   //Unknown field name
   if (fieldtmpl->optional == TRUE)
      return ERR_COMMON_TXTFILE_FIELD_UNAUTHORISED; //Field flagged as optional: unsuited for sorting

   switch (fieldtmpl->type) {
   case FIELD_TXT:
      qsort(tf->bodylines, tf->n_bodylines, sizeof(*tf->bodylines),
            txtscn_txtfieldcmp_qsort);
      break;
   case FIELD_NUM:
      qsort(tf->bodylines, tf->n_bodylines, sizeof(*tf->bodylines),
            txtscn_numfieldcmp_qsort);
      break;
   default:
      assert(0);
   }
   return EXIT_SUCCESS;
}

/**
 * Compares the field in a section with a given value
 * \param f txtfield_t structure containing the name of the field and its value
 * \param s Section to compare
 * */
static int txtscn_cmpfield_bsearch(const void *f, const void *s)
{
   const txtfield_cmp_t* field = (const txtfield_cmp_t*) f;
   const txtscn_t** scnp = (const txtscn_t**) s;
   assert(scnp);
   const txtscn_t* scn = *scnp;

   assert(field && field->fieldname && scn);
   txtfield_t* scnfield = txtscn_lookupfield(scn, field->fieldname);
   assert(scnfield && field->type == scnfield->type);

   switch (field->type) {
   case FIELD_TXT:
      return (strcmp(field->value.txtval, scnfield->field.txt));
   case FIELD_NUM:
      if (field->value.numval < scnfield->field.num->value)
         return -1;
      else if (field->value.numval > scnfield->field.num->value)
         return 1;
      else
         return 0;
   default:
      assert(0);
      return 0;   //Should not happen
   }
}

/*
 * Looks up into an array of sections extracted from a file for a section based on the value of a field
 * \warning The sections are expected to have been retrieved using txtfile_getsections_bytype_sorted. Using this function on another type
 * of array of sections will lead to undefined behaviour.
 * \param scns Array of sections of the same type and ordered according to \c fieldname
 * \param n_scns Size of the array
 * \param fieldname Name of the field over which to look for
 * \param txtval Value to look for if \c fieldname is a text field
 * \param numval Value to look for if \c fieldname is a numerical field
 * \return The section whose value of \c fieldname is either \c txtval or \c numval, or NULL if such a section was not found,
 * if \c scns is NULL or empty, or if \c fieldname is NULL or is a text field while \c txtval is NULL
 * */
txtscn_t* txtscns_lookup(txtscn_t** scns, uint32_t n_scns, char* fieldname,
      char* txtval, int64_t numval)
{
   if (!scns || !fieldname || n_scns == 0)
      return NULL;
   txtfield_t* field = txtscn_getfield(scns[0], fieldname);
   if (!field)
      return NULL;   //Field not found in the section
   txtscn_t** scn = NULL;

   txtfield_cmp_t txtfieldcmp;
   txtfieldcmp.fieldname = fieldname;
   txtfieldcmp.type = field->type;
   switch (field->type) {
   case FIELD_TXT:
      txtfieldcmp.value.txtval = txtval;
      break;
   case FIELD_NUM:
      txtfieldcmp.value.numval = numval;
      break;
   }
   scn = bsearch(&txtfieldcmp, scns, n_scns, sizeof(*scns),
         txtscn_cmpfield_bsearch);

   return (scn) ? *scn : NULL;
}
