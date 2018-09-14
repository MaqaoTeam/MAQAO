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

#include "libmasm.h"
#include "ctype.h"
#include "inttypes.h"

/**
 * Maximum size to print for the content of a data entry
 * */
#define DATAPRINT_MAXLENGTH 80

/*
 * Creates a new data_t structure from an existing reference.
 * \param type Type of data contained in the data_t structure.
 * \param data Reference to the data contained in the structure (can be NULL).
 * This data is considered to be already allocated and will not be freed when freeing the data_t structure.
 * \param size Size in bytes of the data contained in the structure
 * \return A pointer to the newly allocated structure
 * */
data_t* data_new(datatype_t type, void* data, uint64_t size)
{
   data_t* new = lc_malloc0(sizeof(*new));
   if (type >= DATA_LAST_TYPE)
      type = DATA_RAW;  //Incorrect type: type set as raw data
   new->type = type;
   new->local = FALSE;
   new->size = size;
   new->data = data;
   DBGMSG(
         "Created new data %p with type %d and size %"PRIu64" and containing data %p\n",
         new, type, size, data);
   return new;
}

/**
 * Creates a new data_t structure flagged as local (data pointed will be freed when the structure is freed)
 * \param size Size in bytes of the data contained in the structure
 * \return A pointer to the newly allocated structure
 * */
static data_t* data_newlocal(uint64_t size)
{
   data_t* new = lc_malloc0(sizeof(*new));
   new->local = TRUE;
   new->size = size;
   return new;
}

/*
 * Creates a new data_t structure containing data of undefined type
 * \param size Size in bytes of the data contained in the structure
 * \param data Pointer to the data contained in the structure. If not NULL, its value will be copied to the data_t structure
 * \return A pointer to the newly allocated structure
 * */
data_t* data_new_raw(uint64_t size, void* data)
{
   data_t* new = data_newlocal(size);
   new->type = DATA_RAW;
   new->data = lc_malloc(size);
   if (data)
      memcpy(new->data, data, size);
   else
      memset(new->data, 0, size);
   DBGMSG(
         "Created new raw data %p with size %"PRIu64" and containing data %p\n",
         new, size, data);
   return new;
}

/*
 * Creates a new data_t structure containing a pointer to another element
 * \param size Size in bytes of the data contained in the structure
 * \param address Address of the pointer target
 * \param next Element targeted by the pointer (NULL if unknown)
 * \param type Type of the pointer value (using the constants defined in \ref pointer_type)
 * \param target_type Type of the pointer target (using the constants defined in \ref target_type)
 * \return A pointer to the newly allocated structure
 * */
data_t* data_new_ptr(uint64_t size, int64_t address, int64_t offset, void* next,
      pointer_type_t type, target_type_t target_type)
{
   data_t* new = data_newlocal(size);
   new->type = DATA_PTR;
   new->data = pointer_new(address, offset, next, type, target_type);
   DBGMSG(
         "Created new pointer data %p with size %"PRIu64" and pointing to object %p at address %#"PRIx64"\n",
         new, size, next, address);
   return new;
}

/*
 * Changes a data_t structure of type DATA_RAW or DATA_NIL to contain a pointer to another element
 * \param data An existing data_t structure of type DATA_RAW or DATA_NIL
 * \param size Size in bytes of the data contained in the structure
 * \param address Address of the pointer target
 * \param next Element targeted by the pointer (NULL if unknown)
 * \param type Type of the pointer value (using the constants defined in \ref pointer_type)
 * \param target_type Type of the pointer target (using the constants defined in \ref target_type)
 * */
void data_upd_type_to_ptr(data_t* data, uint64_t size, int64_t address, int64_t offset,
      void* next, pointer_type_t type, target_type_t target_type)
{
   /**\todo TODO (2015-04-13) I am creating this function now because I need it in the libtroll for the patcher, so if we find
    * another way to do it, we can remove it. However, such a function could be useful if we later use it to turn unidentified data_t structures
    * into more specific ones after analysis (and in that case we would need to create a function for each data_new* function)*/
   if (!data || (data->type != DATA_RAW && data->type != DATA_NIL))
      return;
   if (data->local)
      lc_free(data->data); //Freeing the content if it was local
   data->size = size;
   data->type = DATA_PTR;
   data->local = TRUE;  //data_t structures of type pointer are always local
   data->data = pointer_new(address, offset, next, type, target_type);
}

/*
 * Creates a new data_t structure containing a string.
 * \param string The string contained in the data. It will be duplicated into the structure
 * \return A pointer to the newly allocated structure
 * */
data_t* data_new_str(char* string)
{
   uint64_t len = (string) ? strlen(string) + 1 : 0;
   data_t* new = data_newlocal(len);
   new->type = DATA_STR;
   new->data = (string) ? lc_strdup(string) : NULL;
   DBGMSG("Created new string data %p containing string %s\n", new, string);
   return new;
}

/*
 * Creates a new data_t structure containing numerical data
 * \param size Size in bytes of the data contained in the structure
 * \param value Value of the data
 * \return A pointer to the newly allocated structure
 * */
data_t* data_new_imm(uint64_t size, int64_t value)
{
   data_t* new = data_newlocal(size);
   new->type = DATA_VAL;
   new->data = lc_malloc(sizeof(value));
   *(int64_t*) new->data = value;
   DBGMSG("Created new value data %p containing value %"PRId64"\n", new, value);
   return new;
}

/**
 * Duplicates a data object of type pointer
 * \param data The data object to copy (must be of type pointer and not NULL)
 * \return A copy of the data object
 * */
static data_t* data_copyptr(data_t* data)
{
   assert(data && (data->type == DATA_PTR));
   data_t* copy = data_newlocal(data->size);
   copy->type = DATA_PTR;
   copy->data = pointer_copy(data->data);
   return copy;
}

/*
 * Duplicates a data object.
 * \param data The data object to copy
 * \return A copy of the data object, or NULL if \c data is NULL
 * */
data_t* data_copy(data_t* data)
{
   data_t* out = NULL;
   if (!data)
      return out;
   if ((data->local == FALSE) || (data->type == DATA_NIL)) {
      //Structure references an existing object: simply duplicating it
      out = data_new(data->type, data->data, data->size);
   } else {
      //Lazily copying the structure using the appropriate constructors
      switch (data->type) {
      case DATA_RAW:
         out = data_new_raw(data->size, data->data);
         break;
      case DATA_PTR:
         out = data_copyptr(data);
         break;
      case DATA_STR:
         out = data_new_str(data->data);
         break;
      case DATA_VAL:
         out = data_new_imm(data->size, *(int64_t*) data->data);
         break;
      default:
         break; //This data has been badly formatted anyway so we will return NULL

      }
   }
   data_set_addr(out, data->address);
   return out;
}

/*
 * Frees a data structure
 * \param d Data structure to free
 * */
void data_free(void* d)
{
   data_t* data = d;
   if (!data)
      return;
   DBGMSG("Freeing data %p\n", data);
   //Freeing the data if it has been allocated
   if (data->local) {
      switch (data->type) {
      case DATA_PTR:
         pointer_free(data->data);
         break;
      default:
         lc_free(data->data);
         break;
      }
   }
   lc_free(data);
}

/*
 * Returns a pointer contained in a data_t structure
 * \param data The structure representing a data element
 * \return The pointer_t structure it contains, or PTR_ERROR if \c data is NULL or does not contain a pointer
 * */
pointer_t* data_get_pointer(data_t* data)
{
   return
         (data && (data->type == DATA_PTR)) ?
               (pointer_t*) data->data : PTR_ERROR;
}

/*
 * Returns a string contained in a data_t structure
 * \param data The structure representing a data element
 * \return A pointer to the string it contains, or PTR_ERROR if \c data is NULL or does not contain a string
 * */
char* data_get_string(data_t* data)
{
   return (data && (data->type == DATA_STR)) ? (char*) data->data : PTR_ERROR;
}

/*
 * Returns a value contained in a data_t structure
 * \param data The structure representing a data element
 * \return The value it contains, or SIGNED_ERROR if \c data is NULL or does not contain a value
 * */
int64_t data_getval(data_t* data)
{
   return
         (data && data->type == DATA_VAL && data->data) ?
               *(int64_t*) data->data : SIGNED_ERROR;
}

/*
 * Returns a label contained in a data_t structure
 * \param data The structure representing a data element
 * \return The label_t structure it contains, or PTR_ERROR if \c data is NULL or does not contain a label
 * */
label_t* data_get_data_label(data_t* data)
{
   return (data && (data->type == DATA_LBL)) ? (label_t*) data->data : PTR_ERROR;
}

/*
 * Returns a relocation contained in a data_t structure
 * \param data The structure representing a data element
 * \return The binrel_t structure it contains, or PTR_ERROR if \c data is NULL or does not contain a relocation
 * */
binrel_t* data_get_binrel(data_t* data)
{
   return
         (data && (data->type == DATA_REL)) ? (binrel_t*) data->data : PTR_ERROR;
}

/*
 * Returns the raw content contained in a data_t structure
 * \param data The structure representing a data element
 * \return A pointer to its content, or PTR_ERROR if \c data is NULL.
 * */
void* data_get_raw(data_t* data)
{
   return (data) ? data->data : PTR_ERROR;
}

/*
 * Returns the size of the data inside a data_t structure
 * \param data The structure representing a data element
 * \return The size of the data in the structure, or 0 if \c data is NULL
 * */
uint64_t data_get_size(data_t* data)
{
   return (data) ? data->size : 0;
}

/*
 * Returns the type of the data inside a data_t structure
 * \param data The structure representing a data element
 * \return The type of the data in the structure, or DATA_UNKNOWN if \c data is NULL
 * */
unsigned int data_get_type(data_t* data)
{
   return (data) ? data->type : DATA_UNKNOWN;
}

/*
 * Retrieves the label associated to a data structure
 * \param data The structure representing a data element
 * \param label The label associated to the address at which the data element is located or the latest label encountered in this section,
 * or NULL if \c data is NULL or not associated to a label
 * */
label_t* data_get_label(data_t* data)
{
   return
         (data && (data->reftype == DATAREF_LABEL)) ?
               data->reference.label : NULL;
}

/*
 * Retrieves a binary section associated to a data structure
 * \param data The structure representing a data element
 * \param label The binary section where the data element is located, retrieved either directly or from its label, or NULL if \c data is NULL
 * */
binscn_t* data_get_section(data_t* data)
{
   if (!data)
      return NULL;
   if (data->reftype == DATAREF_BSCN)
      return data->reference.section;
   if (data->reference.label)
      return label_get_scn(data->reference.label);
   return NULL; //You are a hopeless case
}

/*
 * Returns the reference pointer contained in a data entry of type relocation or pointer
 * \param data Structure representing a data element
 * \return Pointer contained in data if data is not NULL and of type DATA_PTR or DATA_REL, NULL otherwise
 * */
pointer_t* data_get_ref_ptr(data_t* data)
{
   if (!data)
      return NULL;
   switch (data->type) {
   case DATA_PTR:
      return (pointer_t*) data->data;
   case DATA_REL:
      return binrel_get_pointer((binrel_t*) data->data);
   default:
      return NULL;
   }
}

/*
 * Returns the address of the data inside a data_t structure
 * \param data The structure representing a data element
 * \return The address of the data in the structure, or ADDRESS_ERROR if \c data is NULL
 * */
int64_t data_get_addr(data_t* data)
{
   return (data) ? data->address : ADDRESS_ERROR ;
}

/*
 * Returns the end address of the data inside a data_t structure
 * \param data The structure representing a data element
 * \return The address at which the data in the structure ends, or ADDRESS_ERROR if \c data is NULL
 * */
int64_t data_get_end_addr(data_t* data)
{
   return (data) ? data->address + data->size : ADDRESS_ERROR ;
}

/*
 * Associates a label to a data structure
 * \param data The structure representing a data element
 * \param label The label associated to the address at which the data element is located
 * */
void data_set_label(data_t* data, label_t* label)
{
   if (!data)
      return;
   data->reference.label = label;
   data->reftype = DATAREF_LABEL;
}

/*
 * Associates a binary section to a data structure
 * \param data The structure representing a data element
 * \param label The binary section where the data element is located
 * */
void data_set_scn(data_t* data, binscn_t* scn)
{
   if (!data)
      return;
   data->reference.section = scn;
   data->reftype = DATAREF_BSCN;
}

/*
 * Sets the address of a data structure
 * \param data The structure representing a data element
 * \param addr The address where the element is located
 * */
void data_set_addr(data_t* data, int64_t addr)
{
   if (data)
      data->address = addr;
}

/*
 * Sets the size of a data structure
 * \param data The structure representing a data element
 * \param size The size of the element
 * */
void data_set_size(data_t* data, uint64_t size)
{
   if (data)
      data->size = size;
}

/*
 * Sets the type of a data structure
 * \param data The structure representing a data element
 * \param size The type of the element
 * */
void data_set_type(data_t* data, datatype_t type)
{
   if (data)
      data->type = type;
}

/*
 * Sets the content of a data structure. Only possible if the data is not local to the structure
 * \param data The structure representing a data element
 * \param raw The data to set into the element
 * \param type Type of the data
 * */
void data_set_content(data_t* data, void* raw, datatype_t type)
{
   if (!data || data->local)
      return;
   data->data = raw;
   data->type = type;
}

/*
 * Compares the address of a data structure with a given address. This function is intended to be used by bsearch
 * \param addr Pointer to the value of the address to compare
 * \param data Pointer to the data structure
 * \return -1 if address of data is less than address, 0 if both are equal, 1 if address of data is higher than the address
 * */
int data_cmp_by_addr_bsearch(const void* address, const void* data)
{
   if (!address || !data)
      return (address != data);
   int64_t* addr = (int64_t*) address;
   data_t** dat = (data_t**) data;
   if (!*dat)
      return (addr != *dat);
   if ((*dat)->address > *addr)
      return -1;
   else if ((*dat)->address < *addr)
      return 1;
   return 0;
}

/*
 * Points the label of the data to the given label structure
 * Also updates the label to set it to point to the data if the label and the data
 * have the same address
 * \param in The instruction to update
 * \param fctlbl The structure containing the details about the label
 */
void data_link_label(data_t* data, label_t* label)
{
   if (data == NULL)
      return;

   data_set_label(data, label);
   if (label_get_addr(label) == data->address)
      label_set_target_to_data(label, data);
}

/*
 * Compares two data_t structures based on the address referenced by a pointer they contain. This function is intended to be used with qsort.
 * \param d1 First data
 * \param d2 Second data
 * \return -1 if the address referenced by the pointer contained in i1 is less than the address referenced by p2, 0 if they are equal and 1 otherwise
 * */
int data_cmp_by_ptr_addr_qsort(const void* d1, const void* d2)
{
   data_t* data1 = *(data_t**) d1;
   data_t* data2 = *(data_t**) d2;
   int64_t addr1 = pointer_get_addr(data_get_ref_ptr(data1));
   int64_t addr2 = pointer_get_addr(data_get_ref_ptr(data2));
   ;
   if (addr1 < addr2)
      return -1;
   else if (addr1 == addr2)
      return 0;
   else
      return 1;
}

/**
 * Prints the content of a data entry of type numerical value
 * \param data The entry
 * \param str The string to print to
 * \param size Size of the string
 * \return EXIT_SUCCESS if the value could be printed, EXIT_FAILURE otherwise
 * */
static int dataval_print(data_t* data, char* str, size_t size)
{
   assert(data && str); //Note: since this can be invoked from dataraw_print, we don't test the data type
   switch (data->size) {
   case 1:
      lc_sprintf(str, size, "%"PRIx8, *(uint8_t*) data->data);
      return EXIT_SUCCESS;
   case 2:
      lc_sprintf(str, size, "%"PRIx16, *(uint16_t*) data->data);
      return EXIT_SUCCESS;
   case 3:
   case 4:
      lc_sprintf(str, size, "%"PRIx32, *(uint32_t*) data->data);
      return EXIT_SUCCESS;
   case 5:
   case 6:
   case 7:
   case 8:
      lc_sprintf(str, size, "%"PRIx64, *(uint64_t*) data->data);
      return EXIT_SUCCESS;
   default:
      return EXIT_FAILURE;
   }

}

/**
 * Prints the content of a data entry of type string
 * \param data The entry
 * \param str The string to print to
 * \param size Size of the string
 * */
static void datastr_print(data_t* data, char* str, size_t size)
{
   assert(data && str); //Note: since this can be invoked from dataraw_print, we don't test the data type

   char* datastr = data->data;
   size_t datastrlen = strlen(datastr);
   uint16_t maxlen =
         (datastrlen > DATAPRINT_MAXLENGTH) ? DATAPRINT_MAXLENGTH : datastrlen;
   //Value can be cast as a string
   size_t i;
   PRINT_INSTRING(str, size, "\"");
   for (i = 0; i < maxlen; i++) {
      /**\todo (2015-05-21) I'm using this code to force escaped characters to be printed as they are,
       * in order to handle strings containing them. If this code is used elsewhere, it could be moved to lc_string.c.*/
      switch (datastr[i]) {
      case '\"':
         *str++ = '\\';
         *str++ = '\"';
         break;
      case '\'':
         *str++ = '\\';
         *str++ = '\'';
         break;
      case '\\':
         *str++ = '\\';
         *str++ = '\\';
         break;
      case '\n':
         *str++ = '\\';
         *str++ = 'n';
         break;
      case '\t':
         *str++ = '\\';
         *str++ = 't';
         break;
      case '\a':
         *str++ = '\\';
         *str++ = 'a';
         break;
      case '\b':
         *str++ = '\\';
         *str++ = 'b';
         break;
      default:
         *str++ = datastr[i];
         break;   //Stupid Eclipse
      }
   }
   //PRINT_INSTRING(str, "%-"MACROVAL_TOSTRING(DATAPRINT_MAXLENGTH)"s", datastr);
   if (maxlen < datastrlen)
      PRINT_INSTRING(str, size, "..."); //Add periods if we printed less than the size of the string
   PRINT_INSTRING(str, size, "\"");
}

/**
 * Prints the content of a data entry of type pointer
 * \param data The data entry
 * \param str The string where to print the content
 * \param size Size of the string
 * */
static void dataptr_print(data_t* data, char* str, size_t size)
{
   assert(data && str && (data->type = DATA_PTR));
   pointer_print((pointer_t*) data->data, str, size);
}

/**
 * Prints raw data as separate hexadecimal characters
 * \param raw The data
 * \param size The size in bytes of the data
 * \param str The string where to print
 * \param str_size Size of the string
 * */
static void print_rawdata(unsigned char* raw, uint64_t size, char* str,
      size_t str_size)
{
   assert(raw);
   uint16_t i;
   uint16_t maxlen = (size > DATAPRINT_MAXLENGTH) ? DATAPRINT_MAXLENGTH : size;
   uint16_t nbchars = 0;
   for (i = 0; i < maxlen; i++) {
      PRINT_INSTRING(str, str_size, "%02x ", raw[i]);
      nbchars += 3;
      if (nbchars > DATAPRINT_MAXLENGTH)
         break;
   }
   if (nbchars < size)
      PRINT_INSTRING(str, str_size, "..."); //Add periods if we printed less than the size of the string
}

/**
 * Prints the content of a data entry of type raw, attempting to format it to another known type
 * \param data The entry
 * \param str The string to print to
 * \param size Size of the string
 * */
static void dataraw_print(data_t* data, char* str, size_t size)
{
   assert(data && str);

   uint16_t maxlen, i;
   //First attempt to cast the entry as a string
   /**\note (2014-12-01) The following "heuristic" attempts to test if a data contains a string and is probably not very reliable.
    * Here's what it does: 1) check if it contains another '\0' character apart from the last one 2) Check if the first 80 characters
    * don't contain more than a fourth of non-ASCII characters. If those two tests pass, it is considered as a string. Of course, the extended
    * ASCII characters are detected as non printable characters that way and may cause the test to fail.*/
   char* datastr = data->data;
   if (datastr[data->size - 1] == 0) {
      //Last character is a '\0': checking if there is another one before that
      size_t datastrlen = strlen(datastr);
      if (datastrlen == (data->size - 1)) {
         //The data contained in the entry contains a single '0' at the end: we now have to check if it contains a significant number of characters
         uint16_t nb_nochars = 0;
         i = 0;
         maxlen =
               (datastrlen > DATAPRINT_MAXLENGTH) ?
                     DATAPRINT_MAXLENGTH : datastrlen;
         //Counts the number of characters in the string that are not standard ASCII characters
         while (i < maxlen) {
            if (!isgraph(datastr[i]) && !isspace(datastr[i]))
               nb_nochars++;
            if (nb_nochars > maxlen >> 2)
               break;
            i++;
         }
         if (nb_nochars < (maxlen >> 2)) {
            datastr_print(data, str, size);
            return;
         }
      } //Otherwise there is another '\0' character in the middle of the entry
   } //Otherwise the last character is not a '\0'
     //Otherwise we will try to print the data as a numerical value if it has a size that can be cast as such
   if (dataval_print(data, str, size) == EXIT_SUCCESS)
      return;
   // Otherwise we will print the numerical characters directly
   print_rawdata((unsigned char*) datastr, data->size, str, size);
   return;
}

/**
 * Prints the content of a data entry to a string
 * \param data The entry
 * \param str The string to print to
 * \param size Size of the string
 * */
static void data_print(data_t* data, char* str, size_t size)
{

   if (!data || !str)
      return;

   PRINT_INSTRING(str, size, "(len: %"PRId64" byte%s) ", data->size,
         ((data->size > 1) ? "s" : ""));
   /**\todo TODO (2014-12-02) I'm adding this test exclusively because of the case of labels whose address is outside the range of addresses
    * of the section to which they are associated, and whose corresponding data entries are created with a NULL data. If/when we find something
    * more foolprof we could resume this test here. And beside it will probably cause other crashes afterward.*/
   if (!data->data) {
      PRINT_INSTRING(str, size, "%p", data->data);
      return;
   }

   switch (data->type) {
   case DATA_RAW: /**<Data type not defined*/
      dataraw_print(data, str, size);
      break;
   case DATA_PTR: /**<Data is a pointer to another element in the file*/
      dataptr_print(data, str, size);
      break;
   case DATA_STR: /**<Data is a string*/
      datastr_print(data, str, size);
      break;
   case DATA_LBL: /**<Data is a \ref label_t structure*/
      PRINT_INSTRING(str, size, "Label %s at address %#"PRIx64" in section %s",
            label_get_name((label_t* )data->data),
            label_get_addr((label_t* )data->data),
            binscn_get_name(label_get_scn((label_t* )data->data)))
      ;
      break;
   case DATA_REL: /**<Data is a \ref binrel_t structure*/
      binrel_print((binrel_t*) data->data, str, size);
      break;
   case DATA_VAL: /**<Data is a numerical value*/
      if (dataval_print(data, str, size) == EXIT_FAILURE)
         print_rawdata(data->data, data->size, str, size);
      break;
   case DATA_NIL: /**<Data represents a data of type NULL or a data with no value but a given size*/
      PRINT_INSTRING(str, size, "0")
      ;
      break;
   }
}

/*
 * Prints the content of a data structure to a stream
 * \param data The data structure
 * \param stream The stream
 * */
void data_fprint(data_t* data, FILE* stream)
{
   /**\todo TODO (2014-11-18) Copy/paste what data_print does here and adapt the code. Or use a macro to define both*/
   if (!data || !stream)
      return;
   char out[256 + 3 * DATAPRINT_MAXLENGTH];
   data_print(data, out, sizeof(out));
   fprintf(stream, "%s", out);
}

/**
 * Retrieves the content of a data_t structure of type numerical value as a string of bytes
 * \param data A data_t structure
 * \return String of bytes corresponding to the content of the structure or NULL if it could not be
 * cast as either size of an immediate value
 * */
static unsigned char* dataval_tobytes(data_t* data)
{
   assert(data && data->type == DATA_VAL);
   unsigned char* out = NULL;
   switch (data->size) {
   case 1:
      out = (unsigned char*) ((uint8_t*) data->data);
      break;
   case 2:
      out = (unsigned char*) ((uint16_t*) data->data);
      break;
   case 3:
   case 4:
      out = (unsigned char*) ((uint32_t*) data->data);
      break;
   default:
      out = (unsigned char*) ((uint64_t*) data->data);
      break;
   }
   return out;
}

/*
 * Retrieves the content of a data_t structure as a string of bytes
 * \param data A data_t structure
 * \return String of bytes corresponding to the content of the structure.
 * The size of the string is the same as the size of the data structure.
 * Returns NULL if \c data is NULL or of type DATA_LBL or DATA_REL (they require format-specific handling) or DATA_NIL (by design)
 * */
unsigned char* data_to_bytes(data_t* data)
{
   if (!data)
      return NULL;
   unsigned char* out = NULL;
   switch (data->type) {
   case DATA_RAW:
      //Data type not defined: simply returning the data contained into the structure
      out = data->data;
      break;
   case DATA_PTR:
      //Data is a pointer to another element in the file: casting the value depending on the type of the pointer
      out = pointer_tobytes(data->data, data->size);
      break;
   case DATA_STR:
      //Data is a string: simply casting
      out = data->data;
      break;
   case DATA_LBL:
      //Data is a \ref label_t structure: not supported
   case DATA_REL:
      //Data is a \ref binrel_t structure: not supported
      break;
   case DATA_VAL:
      //Data is a numerical value: casting it
      out = dataval_tobytes(data);
      break;
   case DATA_NIL:
      //Data represents a data of type NULL or a data with no value but a given size: returning NULL
      break;
   }
   return out;
}
