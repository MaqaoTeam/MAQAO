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

#ifdef _WIN32
#include <io.h>
#include "ar.h"
#else
#include <ar.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libelf.h"

/**
 * Size of ar_hdr_t members
 */
#define SIZE_AR_NAME     16      /**<Size of ar_name member*/
#define SIZE_AR_DATE     12      /**<Size of ar_date member*/
#define SIZE_AR_UID      6       /**<Size of ar_uid member*/
#define SIZE_AR_GID      6       /**<Size of ar_gid member*/
#define SIZE_AR_MODE     8       /**<Size of ar_mode member*/
#define SIZE_AR_SIZE     10      /**<Size of ar_size member*/
#define SIZE_AR_FMAG     2       /**<Size of ar_fmag member*/

typedef struct ar_hdr ar_hdr_t; /** AR header defined in ar.h file*/

/**\todo TODO (2015-04-24) I'm recreating the system of DBGMSG from libmcommon.h here.
 * If we include libmcommon.h here later we could remove this*/
#ifndef NDEBUG /* it will be paired with assert inhibition */
#include <inttypes.h>
static int __verbose_enabled = -1;

#define VERBOSE(LVL, ...) if (__verbose_enabled) {\
   if (__verbose_enabled > LVL) {\
      fprintf(stderr, "[LIBELF] ");\
      fprintf(stderr, __VA_ARGS__);\
   } else if (__verbose_enabled == -1) {\
      char *__verbose=getenv("LIBELF_VERBOSE");\
      __verbose_enabled = ( (__verbose)?atoi(__verbose) + 1:0 );\
      if (LVL < __verbose_enabled) {\
         fprintf(stderr, "[LIBELF] ");\
         fprintf(stderr, __VA_ARGS__);\
      }\
   }\
}

#define VERBOSE_ACTION(LVL, X) if (__verbose_enabled) {\
   if (__verbose_enabled > LVL) {\
      fprintf(stderr, "[LIBELF] ");\
      X;\
   } else if (__verbose_enabled == -1) {\
      char *__verbose=getenv("LIBELF_VERBOSE");\
      __verbose_enabled = ( (__verbose)?atoi(__verbose) + 1:0 );\
      if (LVL < __verbose_enabled) {\
         fprintf(stderr, "[LIBELF] ");\
         X;\
      }\
   }\
}

#else
#define VERBOSE(LVL, ...)
#define VERBOSE_ACTION(LVL, X)
#endif
/*
 * \struct Elf_Scn
 * Descriptor for ELF file section.  
 */
struct Elf_Scn {
   // Structure containing the section
   Elf* elf; /**< Elf file*/
   // Section header. Only of them is not null, according to
   // the input file architecture
   Elf64_Shdr* shdr_64; /**< Section header in 64b */
   Elf32_Shdr* shdr_32; /**< Section header in 32b */
   // Bits contained in the section 
   Elf_Data* data; /**< Section data*/
};

/*
 * \struct Elf
 * Descriptor for the ELF file.
 */
struct Elf {
   // Elf header. Only of them is not null, according to
   // the input file architecture
   Elf64_Ehdr* ehdr_64; /**< Elf header in 64b */
   Elf32_Ehdr* ehdr_32; /**< Elf header in 32b */
   char* name; /**< Name of the file (if an ELF file belongs to 
    an archive.*/
   int fildes; /**< File descriptor associated to the
    input file */
   Elf_Scn** scn; /**< Array of sections. The number of sections is
    defined in e_shnum, a field of the Elf header*/
   Elf_Cmd mode; /**< Mode used to open the file*/
   Elf_Kind type; /**< Store the Elf_Kind of the structure*/
   uint64_t off; /**< Offset of the ELF structure in the file (0 for
    ELF files, > 0 for AR files)*/
   /**\todo (2015-04-24) Was declared as unsigned long long int, but I don't quite see the point*/
   ar_t* ar; /**< If the file is an archive, pointer on the structure 
    containing parsed header, else NULL*/
   Elf64_Phdr* phdr_64; /**< Program header in 64b*/
   Elf32_Phdr* phdr_32; /**< Program header in 32b*/

   Elf64_Shdr* shdr_64; /**< Section header in 64b*/
   Elf32_Shdr* shdr_32; /**< Section header in 32b*/

/**\todo TODO (2014-06-06) Those two members are there to retrieve easily the sections containing labels (according
 * to ELF standard, there should be only one of each). Later on, we need to have something more streamlined, such as
 * moving the indexes array that is in the elffile_t structure in the Elf structure (and the definitions of indexes
 * in libelf.h), but right now I don't feel like refactoring my code once again in the middle of a refactoring. Sue me.*/
//   Elf64_Half symtabidx;      /**<Index of the SHT_SYMTAB section*/
//   Elf64_Half dynsymidx;      /**<Index of the SHT_DYNSYM section*/
};

/*
 * \struct ar_s
 * \brief Represents a .a file
 */
struct ar_s {
   char* fileName; /**< Name of the .a file*/
   ar_hdr_t* globalHeader; /**< Header of the .a file*/
   char* fcts; /**< Array of function names*/
   char* longNames; /**< Array of long member names*/
   char** objectNames; /**< Array of names of the different objects. Its size is nbObjectFiles*/
   unsigned nbObjectFiles; /**< Number of ELF file in the .a file*/
   ar_hdr_t* headerFiles; /**< Array of headers (one per ELF file)*/
   long* ELFpositionInFile; /**< Offset of ELF files in the .a file (one per ELF file)*/
   unsigned int current; /**< Index of the current ELF file (increased for each 
    extracted file)*/
};

// ============================================================================
//                               AR HANDLING
// ============================================================================
/**
 * Retrieves the value of a numerical field in the ar_t structure
 * \param field Field from the ar_t structure
 * \param field_len Length of the field in bytes
 * \param value Return parameter: will contain the numerical value stored in the field
 * \return 1 if successful, 0 otherwise
 * */
static int AR_getfieldvalue_long(char* field, size_t field_len, long *value)
{
   if (!field || !value)
      return 0;
   char* end = NULL;
   char tmp[field_len + 1];
   strncpy(tmp, field, field_len);
   tmp[field_len] = '\0';
   *value = strtol(tmp, &end, 10);

   if (end == field)
      return 0;
   return 1;
}

/*
 * Checks the AR magic number
 * \param fd a file descriptor
 * \return 1 if the magic number is good, else 0
 */
static int AR_readMagic(int fd)
{
   int error = 0;

   //SARMAG and ARMAG are defined in ar.h
   //char* magicNumber = malloc (SARMAG * sizeof(*magicNumber));
   char magicNumber[SARMAG];
//    if (magicNumber == NULL)
//    {
//        fprintf (stderr, "Error during AR file parsing: Error during memory allocation. Exit\n");
//        exit (1);
//    }
   error = read(fd, magicNumber, sizeof(char) * SARMAG);
   if (error <= 0) {
//        free (magicNumber);
      return (0);
   }

   if (strncmp(magicNumber, ARMAG, SARMAG)) {
//        free (magicNumber);
      return (0);
   }
//    else
//        free (magicNumber);

   return (1);
}

/*
 * Reads the .a global header
 * \param fd a file descriptor
 * \return an initialized ar_hdr_t structure
 */
static ar_hdr_t* AR_readGlobalHeader(int fd)
{
   int error = 0;
   ar_hdr_t* globHeader = malloc(sizeof(*(globHeader)));
   if (globHeader == NULL) {
      fprintf(stderr,
            "Error during AR file parsing: Error during memory allocation. Exit\n");
      exit(1);
   }

   //TO DO: CHECK RETURN OF FREAD
   error += read(fd, globHeader, sizeof(*globHeader));
   //error += read (fd, &(globHeader->ar_name), SIZE_AR_NAME);
   //globHeader->ar_name[SIZE_AR_NAME-1] = '\0';
   //error += read (fd, &(globHeader->ar_date), SIZE_AR_DATE);
   //globHeader->ar_date[SIZE_AR_DATE-1] = '\0';
   //error += read (fd, &(globHeader->ar_uid),  SIZE_AR_UID);
   //globHeader->ar_uid[SIZE_AR_UID-1] = '\0';
   //error += read (fd, &(globHeader->ar_gid),  SIZE_AR_GID);
   //globHeader->ar_gid[SIZE_AR_GID-1] = '\0';
   //error += read (fd, &(globHeader->ar_mode), SIZE_AR_MODE);
   //globHeader->ar_mode[SIZE_AR_MODE-1] = '\0';
   //error += read (fd, &(globHeader->ar_size), SIZE_AR_SIZE);
   //globHeader->ar_size[SIZE_AR_SIZE-1] = '\0';
   //error += read (fd, &(globHeader->ar_fmag), SIZE_AR_FMAG);
   //globHeader->ar_fmag[SIZE_AR_FMAG-1] = '\0';

   if (error <= 0) {
      free(globHeader);
      return (NULL);
   }

   return (globHeader);
}

/*
 * Reads the functions name in the virtual file before the first ELF file
 * \param ar an ar_t structure
 * \param fd a file descriptor
 * \return a flat array containing NULL terminated strings. The array size
 * is the size in the global header
 */
static char* AR_readFcts(ar_t* ar, int fd)
{
   char* end = NULL;
   int error = 0;
   long sizeListFunctions;    // = strtol (ar->globalHeader->ar_size,&end,10);

//   if (end == ar->globalHeader->ar_size)
   if (!AR_getfieldvalue_long(ar->globalHeader->ar_size, SIZE_AR_SIZE,
         &sizeListFunctions)) {
      fprintf(stderr,
            "ERROR (strtol): ar->globHeader->ar_size is not a number\n");
      return NULL;
   }

   //TO DO: FILL IN ar->fcts double array()
   char* ELFinfos = malloc(sizeListFunctions * sizeof(*ELFinfos));
   if (ELFinfos == NULL) {
      fprintf(stderr,
            "Error during AR file parsing: Error during memory allocation. Exit\n");
      exit(1);
   }
   error = read(fd, ELFinfos, sizeListFunctions * sizeof(char));
   if (error <= 0) {
      free(ELFinfos);
      return (NULL);
   }

   return (ELFinfos);
}

/**
 * Retrieves the name of an object
 * \param strName String where the name is stored
 * \param endStr String signaling the end of the name in \c strName
 * \return An allocated string containing the name of the object, or NULL if it could not be found
 * */
static char* AR_getobjectName(char* strName, char* endStr)
{
   char* endName = strstr(strName, endStr);
   if (!endName)
      return NULL;   //Could not find the end

   //End found: copying the name of the object
   size_t nameLen = endName - strName;
   char* out = malloc(sizeof(*out) * (nameLen + 1));
   memcpy(out, strName, nameLen);
   out[nameLen] = '\0';
   return out;
}

/**
 * Read headers of ELF files
 * \param ar an ar_t structure
 * \param fd a file descriptor
 * \return an array of header representing the header of each ELF file
 */
static ar_hdr_t* AR_readHeadersFiles(ar_t* ar, int fd)
{
   ar_hdr_t* headersFiles = NULL;
   ar->ELFpositionInFile = NULL;

   unsigned i = 0;
   int error = 0;
   char* end;
   //long savePosition = ftell(file);
   //fseek(file,0,SEEK_END);
   //long fileEnd = ftell(file);
   //fseek(file,savePosition,SEEK_SET);

   long savePosition = lseek(fd, 0, SEEK_CUR);
   long fileEnd = lseek(fd, 0, SEEK_END);
   lseek(fd, savePosition, SEEK_SET);

   while (lseek(fd, 0, SEEK_CUR) < fileEnd) {
      ar_hdr_t hdrBuf;  //Buffer for reading the header

      //First, loads the header in a buffer in order to check some specificities
      error += read(fd, &hdrBuf, sizeof(hdrBuf));

      if (!strncmp(hdrBuf.ar_name, "//", 2)) {
         //Special case: this is a table of names, not a file
         long namesSz;
         if (!AR_getfieldvalue_long(hdrBuf.ar_size, SIZE_AR_SIZE, &namesSz)) {
            fprintf(stderr,
                  "ERROR (strtol): size of names list is not a number\n");
            return NULL;
         } else {
            //Reads the names that follow
            ar->longNames = malloc(sizeof(*ar->longNames) * (namesSz + 1));
            if (ar->longNames == NULL) {
               fprintf(stderr,
                     "Error during AR file parsing: Error during memory allocation. Exit\n");
               exit(1);
            }
            error = read(fd, ar->longNames, namesSz * sizeof(char));
            if (error <= 0) {
               free(ar->longNames);
               ar->longNames = NULL;
            }
            //Terminating the string
            ar->longNames[namesSz] = '\0';
            continue;   //Not handling this as a standard file
         }
      }
      //Standard case: retrieving the elf object and storing it
      headersFiles = realloc(headersFiles, (i + 1) * sizeof(ar_hdr_t));
      ar->ELFpositionInFile = realloc(ar->ELFpositionInFile,
            (i + 1) * sizeof(long));
      ar->objectNames = realloc(ar->objectNames,
            (i + 1) * sizeof(*ar->objectNames));

      memcpy(&(headersFiles[i]), &hdrBuf, sizeof(hdrBuf));
      //error += read (fd, &(headersFiles[i].ar_name), SIZE_AR_NAME);
      //headersFiles[i].ar_name[SIZE_AR_NAME-1] = '\0';
      //error += read (fd, &(headersFiles[i].ar_date), SIZE_AR_DATE);
      //headersFiles[i].ar_date[SIZE_AR_DATE-1] = '\0';
      //error += read (fd, &(headersFiles[i].ar_uid), SIZE_AR_UID);
      //headersFiles[i].ar_uid[SIZE_AR_UID-1] = '\0';
      //error += read (fd, &(headersFiles[i].ar_gid), SIZE_AR_GID);
      //headersFiles[i].ar_gid[SIZE_AR_GID-1] = '\0';
      //error += read (fd, &(headersFiles[i].ar_mode), SIZE_AR_MODE);
      //headersFiles[i].ar_mode[SIZE_AR_MODE-1] = '\0';
      //error += read (fd, &(headersFiles[i].ar_size), SIZE_AR_SIZE);
      //headersFiles[i].ar_size[SIZE_AR_SIZE-1] = '\0';
      //error += read (fd, &(headersFiles[i].ar_fmag), SIZE_AR_FMAG);
      //headersFiles[i].ar_fmag[SIZE_AR_FMAG-1] = '\0';
      ar->ELFpositionInFile[i] = lseek(fd, 0, SEEK_CUR);

      long nextARHeader;      // = strtol (headersFiles[i].ar_size, &end, 10);
      if (!AR_getfieldvalue_long(headersFiles[i].ar_size, SIZE_AR_SIZE,
            &nextARHeader)) {
         fprintf(stderr,
               "ERROR (strtol): ar->headerFiles[%d].ar_size is not a number\n",
               i);
         return NULL;
      }

      // THE DATA SECTION OF EACH .O NEED TO BE 2 BYTES ALIGNED
      if (((lseek(fd, 0, SEEK_CUR) + nextARHeader) % 2) != 0)
         nextARHeader++;

      char* objectName = NULL;
      //Getting the name of the member
      if (hdrBuf.ar_name[0] == '/') {
         //The name field begins with a /: the full name of the archive member is stored into the string of large names
         if (ar->longNames) {
            long nameIdx;
            //Retrieving the index of the name in the string of large files, which is stored in the name field of the header
            if (!AR_getfieldvalue_long(headersFiles[i].ar_name + 1,
                  SIZE_AR_NAME - 1, &nameIdx)) {
               fprintf(stderr,
                     "ERROR (strtol): ar->headerFiles[%d].ar_name is not a valid name nor a number\n",
                     i);
            } else {
               //Retrieving the name of the object inside the list of large names
               if (nameIdx > strlen(ar->longNames)) {
                  fprintf(stderr,
                        "ERROR: Index of name of object %d out of range\n", i);
               } else {
                  objectName = AR_getobjectName(ar->longNames + nameIdx,
                        "\x2f\x0a");
                  if (!objectName)
                     fprintf(stderr,
                           "ERROR: Unable to retrieve the full name of object %d\n",
                           i);
               } //End of retrieving the extended name
            } //End of retrieving the extended names
         } else {
            //No list of names was found
            fprintf(stderr,
                  "ERROR: No list of large names found while object %d uses it\n",
                  i);
         }
      } else {
         //Standard name: retrieving the end of the name
         objectName = AR_getobjectName(headersFiles[i].ar_name, "/");
         if (!objectName)
            fprintf(stderr,
                  "ERROR: Unable to retrieve the full name of object %d\n", i);
      }
      ar->objectNames[i] = objectName;

      //if (end == headersFiles[i].ar_size)

      i++;
      lseek(fd, nextARHeader, SEEK_CUR);
   }

   if (error <= 0) {
      return (NULL);
   }

   ar->nbObjectFiles = i;
   return (headersFiles);
}

/*
 * Frees an ar_t structure
 * \param ar a structure to free
 */
static void _AR_free(ar_t* ar)
{
   if (ar == NULL)
      return;

   if (ar->globalHeader != NULL)
      free(ar->globalHeader);
   if (ar->fcts != NULL)
      free(ar->fcts);
   if (ar->longNames != NULL)
      free(ar->longNames);
   if (ar->headerFiles != NULL)
      free(ar->headerFiles);
   if (ar->ELFpositionInFile != NULL)
      free(ar->ELFpositionInFile);
   if (ar->objectNames) {
      unsigned int i;
      for (i = 0; i < ar->nbObjectFiles; i++)
         free(ar->objectNames[i]);
      free(ar->objectNames);
   }
   free(ar);
}

/*
 * Loads a .a file and parses it
 * \param fd a file descriptor
 * \return a loaded ar_t structure or NULL if there is a problem
 */
static ar_t* _AR_load(int fd)
{
   ar_t* ar = malloc(sizeof(*ar));
   memset(ar, 0, sizeof(*ar));
//   ar->globalHeader  = NULL;
//	ar->fcts          = NULL;
//   ar->nbObjectFiles = 0;
//	ar->headerFiles   = NULL;
//   ar->current       = 0;
//	ar->ELFpositionInFile = NULL;

   // Check the magic number
   lseek(fd, 0, SEEK_SET);
   if (AR_readMagic(fd) == 0) {
      fprintf(stderr, "Error during AR file parsing: Magic number is wrong\n");
      _AR_free(ar);
      return (NULL);
   }

   // Read the global header
   ar->globalHeader = AR_readGlobalHeader(fd);
   if (ar->globalHeader == NULL) {
      _AR_free(ar);
      fprintf(stderr, "Error during AR file parsing: Global Header is wrong\n");
      return (NULL);
   }

   // Read the list of functions
   ar->fcts = AR_readFcts(ar, fd);
   if (ar->fcts == NULL) {
      _AR_free(ar);
      return (NULL);
   }

   // Read headers of all ELF files
   ar->headerFiles = AR_readHeadersFiles(ar, fd);
   if (ar->headerFiles == NULL) {
      _AR_free(ar);
      fprintf(stderr, "Error during AR file parsing: Local Header are wrong\n");
      return (NULL);
   }

   return ar;
}

// ============================================================================
//                            ELF STATIC FUNCTIONS
// ============================================================================
/*
 * Translates a SHT_ value into a ELF_T_ value
 * \param sh_type a SHT_ value defined in the elf.h file
 * \return a ELF_T_ value defined in the libelf.h file
 */
static int scntype_to_datatype(int sh_type)
{
   switch (sh_type) {
   case SHT_NULL:
      return (ELF_T_BYTE);
      break;
   case SHT_PROGBITS:
      return (ELF_T_BYTE);
      break;
   case SHT_SYMTAB:
      return (ELF_T_SYM);
      break;
   case SHT_STRTAB:
      return (ELF_T_BYTE);
      break;
   case SHT_RELA:
      return (ELF_T_RELA);
      break;
   case SHT_HASH:
      return (ELF_T_GNUHASH);
      break;
   case SHT_DYNAMIC:
      return (ELF_T_DYN);
      break;
   case SHT_NOTE:
      return (ELF_T_NHDR);
      break;
   case SHT_NOBITS:
      return (ELF_T_BYTE);
      break;
   case SHT_REL:
      return (ELF_T_REL);
      break;
   case SHT_SHLIB:
      return (ELF_T_BYTE);
      break;
   case SHT_DYNSYM:
      return (ELF_T_SYM);
      break;
   case SHT_INIT_ARRAY:
      return (ELF_T_BYTE);
      break;
   case SHT_FINI_ARRAY:
      return (ELF_T_BYTE);
      break;
   case SHT_PREINIT_ARRAY:
      return (ELF_T_BYTE);
      break;
   case SHT_GROUP:
      return (ELF_T_BYTE);
      break;
   case SHT_SYMTAB_SHNDX:
      return (ELF_T_BYTE);
      break;
   case SHT_NUM:
      return (ELF_T_BYTE);
      break;
   case SHT_LOOS:
      return (ELF_T_BYTE);
      break;
   case SHT_GNU_ATTRIBUTES:
      return (ELF_T_BYTE);
      break;
   case SHT_GNU_HASH:
      return (ELF_T_GNUHASH);
      break;
   case SHT_GNU_LIBLIST:
      return (ELF_T_BYTE);
      break;
   case SHT_CHECKSUM:
      return (ELF_T_BYTE);
      break;
   case SHT_LOSUNW:
      return (ELF_T_BYTE);
      break;
   case SHT_SUNW_COMDAT:
      return (ELF_T_BYTE);
      break;
   case SHT_SUNW_syminfo:
      return (ELF_T_SYMINFO);
      break;
   case SHT_GNU_verdef:
      return (ELF_T_VDEF);
      break;
   case SHT_GNU_verneed:
      return (ELF_T_VNEED);
      break;
   case SHT_GNU_versym:
      return (ELF_T_HALF);
      break;
   case SHT_LOPROC:
      return (ELF_T_BYTE);
      break;
   case SHT_HIPROC:
      return (ELF_T_BYTE);
      break;
   case SHT_LOUSER:
      return (ELF_T_BYTE);
      break;
   case SHT_HIUSER:
      return (ELF_T_BYTE);
      break;
   default:
      return (ELF_T_BYTE);
      break;
   }
   return (ELF_T_BYTE);
}

/*
 * Loads a 32b Elf file
 * \param elf an Elf structure
 * \return the fulled Elf structure
 */
static Elf* _elf_begin32(Elf* elf)
{
   int error = 0;
   int i = 0;
   int fd = elf->fildes;
   VERBOSE(0,
         "*Warning* Not verbose output was added when parsing 32 bits files\n")
   // read the elf header
   Elf32_Ehdr* header = malloc(sizeof(Elf32_Ehdr));
   error = lseek(fd, elf->off, SEEK_SET);
   error += read(fd, header, sizeof(Elf32_Ehdr));
   elf->ehdr_32 = header;

   //TODO: check error
   (void) error;

   // read sections
   if (elf->ehdr_32->e_shoff != 0) {
      elf->shdr_32 = malloc(elf->ehdr_32->e_shnum * sizeof(Elf32_Shdr));
      error = lseek(fd, elf->off + elf->ehdr_32->e_shoff, SEEK_SET);
      error += read(fd, elf->shdr_32,
            elf->ehdr_32->e_shnum * sizeof(Elf32_Shdr));

      elf->scn = malloc(elf->ehdr_32->e_shnum * sizeof(Elf_Scn*));
      for (i = 0; i < elf->ehdr_32->e_shnum; i++) {
         Elf32_Shdr* scn = malloc(sizeof(Elf32_Shdr));
         //error += read (fd, scn,   sizeof (Elf32_Shdr));
         memcpy(scn, &elf->shdr_32[i], sizeof(Elf32_Shdr));
         /**\todo TODO (2015-04-08) If this works, replace scn with a simple pointer to &elf->shdr_32[i]
          * and update the free functions and writing functions accordingly*/

         elf->scn[i] = malloc(sizeof(Elf_Scn));
         elf->scn[i]->shdr_64 = NULL;
         elf->scn[i]->shdr_32 = scn;
         elf->scn[i]->data = NULL;
         elf->scn[i]->elf = elf;
         /**\todo TODO See the todo in the definition of the Elf structure. Either move this elsewhere or do more than that*/
//      switch(scn->sh_type) {
//      case SHT_SYMTAB: elf->symtabidx = i; break;
//      case SHT_DYNSYM: elf->dynsymidx = i; break;
//      default:break; //I'm not quite sure if this is needed, but I'm entering the "fed up and want to finish this quickly" phase
//      }
      }
   }

   //TODO: check error
   (void) error;

   // read segments
   if (elf->ehdr_32->e_phoff != 0) {
      elf->phdr_32 = malloc(elf->ehdr_32->e_phnum * sizeof(Elf32_Phdr));
      error = lseek(fd, elf->off + elf->ehdr_32->e_phoff, SEEK_SET);
      error += read(fd, elf->phdr_32,
            elf->ehdr_32->e_phnum * sizeof(Elf32_Phdr));

      //TODO: check error
      (void) error;
   }

   return (elf);
}

/*
 * Loads a 64b Elf file
 * \param elf an Elf structure
 * \return the fulled Elf structure
 */
static Elf* _elf_begin64(Elf* elf)
{
   int error = 0;
   int i = 0;
   int fd = elf->fildes;

   // read the elf header
   Elf64_Ehdr* header = malloc(sizeof(Elf64_Ehdr));
   error = lseek(fd, elf->off, SEEK_SET);
   error += read(fd, header, sizeof(Elf64_Ehdr));
   elf->ehdr_64 = header;

   VERBOSE(0,
         "Parsed ELF 64 header:\n" "\te_type:%u - e_machine:%u - e_version:%u - e_entry:%#"PRIx64" - e_phoff:%#"PRIx64" - e_shoff:%#"PRIx64"\n" "\te_flags:%d - e_ehsize:%u - e_phentsize:%u - e_phnum:%u - e_shentsize:%u - e_shnum:%u - e_shstrndx:%u\n",
         header->e_type, header->e_machine, header->e_version, header->e_entry,
         header->e_phoff, header->e_shoff, header->e_flags, header->e_ehsize,
         header->e_phentsize, header->e_phnum, header->e_shentsize,
         header->e_shnum, header->e_shstrndx)

   //TODO: check error
   (void) error;

   // read sections
   if (elf->ehdr_64->e_shoff != 0) {
      elf->shdr_64 = malloc(elf->ehdr_64->e_shnum * sizeof(Elf64_Shdr));
      error = lseek(fd, elf->off + elf->ehdr_64->e_shoff, SEEK_SET);
      error += read(fd, elf->shdr_64,
            elf->ehdr_64->e_shnum * sizeof(Elf64_Shdr));

      VERBOSE(0, "Parsed section header at offset %#"PRIx64"\n",
            elf->off + elf->ehdr_64->e_shoff)

      elf->scn = malloc(elf->ehdr_64->e_shnum * sizeof(Elf_Scn*));
      for (i = 0; i < elf->ehdr_64->e_shnum; i++) {
         Elf64_Shdr* scn = malloc(sizeof(Elf64_Shdr));
         //error += read (fd, scn,   sizeof (Elf64_Shdr));

         VERBOSE(1,
               "Section header %d:\n" "\tsh_name:%u - sh_type:%u - sh_flags:%#"PRIx64" - sh_addr:%#"PRIx64" - sh_offset:%#"PRIx64"\n" "\tsh_size:%#"PRIx64" - sh_link:%u - sh_info:%u - sh_addralign:%#"PRIx64" - sh_entsize:%"PRIu64"\n",
               i, elf->shdr_64[i].sh_name, elf->shdr_64[i].sh_type,
               elf->shdr_64[i].sh_flags, elf->shdr_64[i].sh_addr,
               elf->shdr_64[i].sh_offset, elf->shdr_64[i].sh_size,
               elf->shdr_64[i].sh_link, elf->shdr_64[i].sh_info,
               elf->shdr_64[i].sh_addralign, elf->shdr_64[i].sh_entsize)

         memcpy(scn, &elf->shdr_64[i], sizeof(Elf64_Shdr));
         /**\todo TODO (2015-04-08) If this works, replace scn with a simple pointer to &elf->shdr_64[i]
          * and update the free functions and writing functions accordingly*/

         VERBOSE_ACTION(3, fprintf(stderr, "Bytes: ");
            unsigned char* __scn_buf = NULL;
            int __error = lseek(fd, elf->off + elf->shdr_64[i].sh_offset, SEEK_SET);
            if (elf->shdr_64[i].sh_type != SHT_NOBITS && elf->shdr_64[i].sh_size > 0) {
               __scn_buf = malloc(elf->shdr_64[i].sh_size);
               __error += read(fd, __scn_buf, elf->shdr_64[i].sh_size);
            }
            if (__scn_buf == NULL) fprintf(stderr, "<NULL>\n"); else {
               Elf64_Xword c;
               for (c = 0; c < elf->shdr_64[i].sh_size; c++) {
                  fprintf(stderr, "%02x ", __scn_buf[c]);
               }
               fprintf(stderr, "\n");
               free(__scn_buf);
            }
         )

         elf->scn[i] = malloc(sizeof(Elf_Scn));
         elf->scn[i]->shdr_32 = NULL;
         elf->scn[i]->shdr_64 = scn;
         elf->scn[i]->data = NULL;
         elf->scn[i]->elf = elf;
         //      /**\todo TODO See the todo in the definition of the Elf structure. Either move this elsewhere or do more than that*/
         //      switch(scn->sh_type) {
         //      case SHT_SYMTAB: elf->symtabidx = i; break;
         //      case SHT_DYNSYM: elf->dynsymidx = i; break;
         //      default:break; //I'm not quite sure if this is needed, but I'm entering the "fed up and want to finish this quickly" phase
         //      }
      }
   }

   //TODO: check error
   (void) error;

   // read segments
   if (elf->ehdr_64->e_phoff != 0) {
      elf->phdr_64 = malloc(elf->ehdr_64->e_phnum * sizeof(Elf64_Phdr));
      error = lseek(fd, elf->off + elf->ehdr_64->e_phoff, SEEK_SET);
      error += read(fd, elf->phdr_64,
            elf->ehdr_64->e_phnum * sizeof(Elf64_Phdr));

      VERBOSE(0, "Parsed segment header at offset %#"PRIx64"\n",
            elf->off + elf->ehdr_64->e_phoff)
      //TODO: check error
      (void) error;
#ifndef NDEBUG
      for (i = 0; i < elf->ehdr_64->e_phnum; i++) {
         VERBOSE(1,
               "Segment header %d:\n" "\tp_type:%u - p_flags:%#"PRIx32" - p_offset:%#"PRIx64" - p_vaddr:%#"PRIx64"\n" "\tp_paddr:%#"PRIx64" - p_filesz:%#"PRIx64" - p_memsz:%#"PRIx64" - p_align:%#"PRIx64"\n",
               i, elf->phdr_64[i].p_type, elf->phdr_64[i].p_flags,
               elf->phdr_64[i].p_offset, elf->phdr_64[i].p_vaddr,
               elf->phdr_64[i].p_paddr, elf->phdr_64[i].p_filesz,
               elf->phdr_64[i].p_memsz, elf->phdr_64[i].p_align)
      }
#endif
   }
   return (elf);
}

/*
 * Initialize an Elf structure
 * \param fd   File descriptor
 * \param cmd  Mode used to open the file
 * \param kind Kind of the 
 * \param off
 *
 */
static Elf* _elf_init(int fd, Elf_Cmd cmd, Elf_Kind kind, long off)
{
   Elf* elf = malloc(sizeof(Elf));
   memset(elf, 0, sizeof(Elf));
   elf->fildes = fd;
//   elf->ehdr_64    = NULL;
//   elf->ehdr_32    = NULL;
//   elf->scn        = NULL;
   elf->mode = cmd;
   elf->type = kind;
   elf->off = off;
//   elf->ar         = NULL;
//   elf->name       = NULL;
//   elf->phdr_64    = NULL;
//   elf->phdr_32    = NULL;

//   /**\todo TODO See the todo in the definition of the Elf structure. Either move this elsewhere or do more than that*/
//   elf->symtabidx = 0; //Section 0 is always empty or something, so a value of 0 means uninitialised
//   elf->dynsymidx = 0;

   return (elf);
}

// ============================================================================
//                              ELF API FUNCTIONS
// ============================================================================
/*
 * Loads an Elf file
 * \param __filedes the input file file descriptor
 * \param __cmd flags used to open the file. 
 *              Only ELF_C_READ and ELF_C_READ_MMAP are supported
 * \param __ref Reference Elf file. If __ref is an Elf file, it is
 *              returned. If __ref is an archive (AR file), then 
 *              an object file is extracted from the archive.
 *              In this case, the function works as an iterator: at
 *              each call with the same ref, it returns the next 
 *              object file or NULL if all files have been returned.
 *              The iterator can be reset using elf_reset_ar_iterator
 * \return the fulled Elf structure
 */
Elf *elf_begin(int __fildes, Elf_Cmd __cmd, Elf *__ref)
{
   int error;
   if (__fildes < 0) {
      return (NULL);
   }

   if ((__cmd & (ELF_C_READ | ELF_C_READ_MMAP)) == 0) {
      fprintf(stderr, "[elf_begin] Unsupported __cmd value %d\n", __cmd);
      return (NULL);
   }
   if (elf_kind(__ref) == ELF_K_ELF)
      return (__ref);
   else if (elf_kind(__ref) == ELF_K_AR) {
      // no more objects, return NULL
      if (__ref->ar->current >= __ref->ar->nbObjectFiles)
         return (NULL);
      // else, load the next object from the archive and
      // return it
      unsigned char ident[EI_NIDENT];
      lseek(__fildes, __ref->ar->ELFpositionInFile[__ref->ar->current],
            SEEK_SET);
      error = read(__fildes, ident, EI_NIDENT);
      lseek(__fildes, __ref->ar->ELFpositionInFile[__ref->ar->current],
            SEEK_SET);
      __ref->ar->current += 1;

      //TODO: check error
      (void) error;

      if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1
            || ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3) {
         return (elf_begin(__fildes, __cmd, __ref));
      }

      Elf* elf = _elf_init(__fildes, __cmd, ELF_K_ELF,
            __ref->ar->ELFpositionInFile[__ref->ar->current - 1]);
      elf->name = __ref->ar->objectNames[__ref->ar->current - 1]; //__ref->ar->headerFiles[__ref->ar->current - 1].ar_name;

      switch (ident[EI_CLASS]) {
      case ELFCLASSNONE:
         fprintf(stderr, "[elf_begin] No class\n");
         exit(1);
         break;
      case ELFCLASS32:
         return (_elf_begin32(elf));
         break;
      case ELFCLASS64:
         return (_elf_begin64(elf));
         break;
      default:
         fprintf(stderr, "[elf_begin] Unknown class\n");
         break;
      }
      return (elf);
   }

   unsigned char ident[EI_NIDENT];  // Used to load ELF M.N.
   char armag[SARMAG + 1];          // Used to load AR M.N.
   int type = ELF_K_NONE;

   //get the magic number
   error = lseek(__fildes, 0, SEEK_SET);
   error += read(__fildes, armag, SARMAG);
   armag[SARMAG] = '\0';
   error += lseek(__fildes, 0, SEEK_SET);
   error += read(__fildes, ident, EI_NIDENT);

   //TODO: check error
   (void) error;

   if (error <= 0)
      return (NULL);

   VERBOSE(0, "Magic number: %c %c %c %c\n", ident[EI_MAG0], ident[EI_MAG1],
         ident[EI_MAG2], ident[EI_MAG3])

   //check the ELF magic word to be sure that it is an ELF file
   if (ident[EI_MAG0] == ELFMAG0 && ident[EI_MAG1] == ELFMAG1
         && ident[EI_MAG2] == ELFMAG2 && ident[EI_MAG3] == ELFMAG3)
      type = ELF_K_ELF;
   else if (strcmp(armag, ARMAG) == 0)
      type = ELF_K_AR;

   //the magic number is not handled so return NULL
   //!! Warning !! AR files are not handled for the moment
   //so a NULL value is return. This will be fixed later.
   if (type == ELF_K_NONE)
      return (NULL);

   //initialize the Elf structure
   Elf* elf = _elf_init(__fildes, __cmd, type, 0);

   //the file is an archive, do not try to open it
   //as a regular ELF file
   if (type == ELF_K_AR) {
      elf->ar = _AR_load(__fildes);
      elf->name = elf->ar->objectNames[elf->ar->current]; //elf->ar->headerFiles[elf->ar->current].ar_name;
      return (elf);
   }
   //get the mode (32b / 64b) and load the
   //file using the corresponding function
   switch (ident[EI_CLASS]) {
   case ELFCLASSNONE:
      fprintf(stderr, "[elf_begin] No class\n");
      elf_end(elf);
      exit(1);
      break;
   case ELFCLASS32:
      VERBOSE(0, "File is 32 bits\n")
      return (_elf_begin32(elf));
      break;
   case ELFCLASS64:
      VERBOSE(0, "File is 64 bits\n")
      return (_elf_begin64(elf));
      break;
   default:
      elf_end(elf);
      fprintf(stderr, "[elf_begin] Unknown class\n");
      break;
   }
   return (NULL);
}

/*
 * Gets the kind of an Elf file (Ar file, ELF file ...)
 * \param __elf an Elf structure
 * \return an Elf_Kind containing the Elf kind
 */
Elf_Kind elf_kind(Elf *__elf)
{
   if (__elf == NULL)
      return (ELF_K_NONE);
   else
      return (__elf->type);
}

/*
 * Returns the data corresponding to a section
 * \param __scn an initialized Elf_Scn structure
 * \param __data Not used but present in the standard API
 * \return The Elf_Data corresponding to __scn or NULL if
 *         there is an error
 */
Elf_Data *elf_getdata(Elf_Scn *__scn, Elf_Data *__data)
{
   (void) __data;
   if (__scn == NULL)
      return (NULL);
   if (__scn->data == NULL) {
      int error = 0;
      //load the data and save it into data structures
      if (__scn->shdr_32 != NULL) {
         error = lseek(__scn->elf->fildes,
               __scn->elf->off + __scn->shdr_32->sh_offset, SEEK_SET);
         __scn->data = malloc(sizeof(Elf_Data));
         if (__scn->shdr_32->sh_type != SHT_NOBITS) {
            __scn->data->d_buf = malloc(__scn->shdr_32->sh_size);
            error += read(__scn->elf->fildes, __scn->data->d_buf,
                  __scn->shdr_32->sh_size);
         } else {
            __scn->data->d_buf = NULL;
         }

         //TODO: check error
         (void) error;

         __scn->data->d_size = __scn->shdr_32->sh_size;
         __scn->data->d_type = scntype_to_datatype(__scn->shdr_32->sh_type);
         __scn->data->d_version = EV_CURRENT;
         __scn->data->d_off = 0;
         __scn->data->d_align = __scn->shdr_32->sh_addralign;
      } else if (__scn->shdr_64 != NULL) {
         error = lseek(__scn->elf->fildes,
               __scn->elf->off + __scn->shdr_64->sh_offset, SEEK_SET);
         __scn->data = malloc(sizeof(Elf_Data));
         if (__scn->shdr_64->sh_type != SHT_NOBITS) {
            __scn->data->d_buf = malloc(__scn->shdr_64->sh_size);
            error += read(__scn->elf->fildes, __scn->data->d_buf,
                  __scn->shdr_64->sh_size);
         } else {
            __scn->data->d_buf = NULL;
         }

         //TODO: check error
         (void) error;

         __scn->data->d_type = scntype_to_datatype(__scn->shdr_64->sh_type);
         __scn->data->d_version = EV_CURRENT;
         __scn->data->d_size = __scn->shdr_64->sh_size;
         __scn->data->d_off = 0;
         __scn->data->d_align = __scn->shdr_64->sh_addralign;
      } else
         return (NULL);
   }
   return (__scn->data);
}

/*
 * Checks the Elf version
 * \param __version the version of the Elf file
 * \return EV_CURRENT if there is no problem, else EV_NONE
 */
unsigned int elf_version(unsigned int __version)
{
   (void) __version;
   return (EV_CURRENT);
}

/*
 * Frees an Elf structure (internal function used for refactoring)
 * \param __elf an Elf structure to free
 * \param freedata If set to 1, the data from the sections will be freed as well
 * \return 1 if there is no problem, else 0
 */
static int elf_end_s(Elf *__elf, int freedata)
{
   if (__elf == NULL)
      return (0);

   int i = 0;

   if (__elf->ar != NULL)
      _AR_free(__elf->ar);

   // As only one of the header can be not null, several cases can not
   // appear together
   if (__elf->ehdr_64 != NULL) {  // the file is in 64b
      for (i = 0; i < __elf->ehdr_64->e_shnum; i++) {
         free(__elf->scn[i]->shdr_64);
         if (__elf->scn[i]->data != NULL) {
            if (freedata)
               free(__elf->scn[i]->data->d_buf);
            /**\todo TODO (2015-06-05) Find a way to avoid making the test for each section without duplicating the code
             * Maybe use a macro (this would also allow to get rid of the double code for 32/64)*/
            free(__elf->scn[i]->data);
         }
         free(__elf->scn[i]);
      }
      free(__elf->scn);
      free(__elf->ehdr_64);
      free(__elf->shdr_64);
      free(__elf->phdr_64);
      __elf->ehdr_64 = NULL;
   } else if (__elf->ehdr_32 != NULL) {  // the file is in 32b
      for (i = 0; i < __elf->ehdr_32->e_shnum; i++) {
         free(__elf->scn[i]->shdr_32);
         if (__elf->scn[i]->data != NULL) {
            if (freedata)
               free(__elf->scn[i]->data->d_buf);
            /**\todo TODO (2015-06-05) Find a way to avoid making the test for each section without duplicating the code
             * Maybe use a macro (this would also allow to get rid of the double code for 32/64)*/
            free(__elf->scn[i]->data);
         }
         free(__elf->scn[i]);
      }
      free(__elf->scn);
      free(__elf->ehdr_32);
      free(__elf->shdr_32);
      free(__elf->phdr_32);
      __elf->ehdr_32 = NULL;
   }

   free(__elf);
   return (0);
}

/*
 * Frees an Elf structure, but not the data from the sections (this is for handling the cases where they have been allocated elsewhere)
 * \param __elf an Elf structure to free
 * \return 1 if there is no problem, else 0
 */
int elf_end_nodatafree(Elf *__elf)
{
   return elf_end_s(__elf, 0);
}

/*
 * Frees an Elf structure
 * \param __elf an Elf structure to free
 * \return 1 if there is no problem, else 0
 */
int elf_end(Elf *__elf)
{
   return elf_end_s(__elf, 1);
}

/*
 * Returns the string located in the section __index at the offset __offset
 * \param __elf an Elf structure
 * \param __index index of the section the string belongs to
 * \param __offset offset in the section of the string
 * \return NULL if there is a problem, else the string located in the section
 *         __index at the offset __offset
 */
char *elf_strptr(Elf *__elf, size_t __index, size_t __offset)
{
   if (__elf == NULL)
      return (NULL);

   Elf_Scn* scn = elf_getscn(__elf, __index);
   if (scn == NULL)
      return (NULL);

   if (__elf->ehdr_64 != NULL) {  // the file is in 64b
      Elf64_Shdr* shdr = elf64_getshdr(scn);
      if (shdr->sh_size < __offset)
         return (NULL);
      else {
         Elf_Data * data = elf_getdata(scn, NULL);
         if (data == NULL)
            return (NULL);
         else
            return (&((char*) data->d_buf)[__offset]);
      }
   } else if (__elf->ehdr_32 != NULL) {  // the file is in 32b
      Elf32_Shdr* shdr = elf32_getshdr(scn);
      if (shdr->sh_size < __offset)
         return (NULL);
      else {
         Elf_Data * data = elf_getdata(scn, NULL);
         if (data == NULL)
            return (NULL);
         else
            return (&((char*) data->d_buf)[__offset]);
      }
   } else
      return (NULL);
}

/*
 * Returns the section at index __index
 * \param __elf an Elf structure
 * \param __index index of the section to return 
 * \return NULL if there is a problem, else the Elf_Scn structure
 */
Elf_Scn *elf_getscn(Elf *__elf, size_t __index)
{
   if (__elf == NULL)
      return (NULL);

   if (__elf->ehdr_64 != NULL) {
      if (__index > __elf->ehdr_64->e_shnum)
         return (NULL);
      else
         return (__elf->scn[__index]);
   } else if (__elf->ehdr_32 != NULL) {
      if (__index > __elf->ehdr_32->e_shnum)
         return (NULL);
      else
         return (__elf->scn[__index]);
   } else
      return (NULL);
}

/*
 * Returns the 32b section header associated to a section.
 * \param __scn an initialized Elf_Scn structure
 * \return the 32b section header or NULL if there is a problem
 */
Elf32_Shdr *elf32_getshdr(Elf_Scn *__scn)
{
   if (__scn == NULL)
      return (NULL);
   return (__scn->shdr_32);
}

/*
 * Returns the 64b section header associated to a section.
 * \param __scn an initialized Elf_Scn structure
 * \return the 64b section header or NULL if there is a problem
 */
Elf64_Shdr *elf64_getshdr(Elf_Scn *__scn)
{
   if (__scn == NULL)
      return (NULL);
   return (__scn->shdr_64);
}

/*
 * Returns the ident from an ELF file.
 * \param __elf an initialized Elf structure
 * \param __nbytes Not used but present in the standard API
 * \return the elf ident string or NULL if there is a problem
 */
char *elf_getident(Elf *__elf, size_t *__nbytes)
{
   (void) __nbytes;
   if (__elf == NULL)
      return NULL;

   if (__elf->ehdr_64 != NULL)
      return ((char*) __elf->ehdr_64->e_ident);
   else if (__elf->ehdr_32 != NULL)
      return ((char*) __elf->ehdr_32->e_ident);
   else
      return (NULL);
}

/*
 * Returns the 32b elf header associated to an Elf structure.
 * \param __elf an initialized Elf structure
 * \return the 32b elf header or NULL if there is a problem (no 32b
 *         elf header for example)
 */
Elf32_Ehdr *elf32_getehdr(Elf *__elf)
{
   if (__elf == NULL)
      return NULL;
   return (__elf->ehdr_32);
}

/*
 * Returns the 64b elf header associated to an Elf structure.
 * \param __elf an initialized Elf structure
 * \return the 64b elf header or NULL if there is a problem (no 64b
 *         elf header for example)
 */
Elf64_Ehdr *elf64_getehdr(Elf *__elf)
{
   if (__elf == NULL)
      return NULL;
   return (__elf->ehdr_64);
}

/*
 * Returns the 32b program header
 * \param __elf an initialized Elf structure
 * \return the 32b program header or NULL if there is a problem
 */
Elf32_Phdr *elf32_getphdr(Elf *__elf)
{
   if (__elf == NULL)
      return NULL;
   return (__elf->phdr_32);
}

/*
 * Returns the 64b program header
 * \param __elf an initialized Elf structure
 * \return the 64b program header or NULL if there is a problem
 */
Elf64_Phdr *elf64_getphdr(Elf *__elf)
{
   if (__elf == NULL)
      return NULL;
   return (__elf->phdr_64);
}

/**\todo TODO (2015-04-08) The two following functions can't be named elfxx_getshdr as there are already
 * functions with that name returning the header of a specific function, and which are used by the
 * libdwarf. If we can rename those functions in the libdwarf, we could rename them and the elfxx_getshdr
 * functions to have more appropriate names.*/
/*
 * Returns the 32b section header
 * \param __elf an initialized Elf structure
 * \return the 32b section header or NULL if there is a problem
 */
Elf32_Shdr *elf32_getfullshdr(Elf *__elf)
{
   if (__elf == NULL)
      return NULL;
   return (__elf->shdr_32);
}

/*
 * Returns the 64b section header
 * \param __elf an initialized Elf structure
 * \return the 64b section header or NULL if there is a problem
 */
Elf64_Shdr *elf64_getfullshdr(Elf *__elf)
{
   if (__elf == NULL)
      return NULL;
   return (__elf->shdr_64);
}
// ============================================================================
//            FOLLOWING FUNCTIONS ARE NOT DEFINED IN LIBELF API
// ============================================================================
/*
 * Returns the name of an elf file if it has been extracted from 
 * an archive file
 * \param __elf an initialized Elf structure
 * \return the file name if possible, else a NULL value
 */
char* elf_getname(Elf* __elf)
{
   if (__elf == NULL)
      return NULL;
   return (__elf->name);
}

/*
 * Returns the Elf machine code of the file
 * \param __elf an Elf structure
 * \return an Elf machine code (EM_ macro from elf.h)
 */
int elf_getmachine(Elf* __elf)
{
   if (__elf == NULL)
      return (EM_NONE);

   if (__elf->ehdr_64 != NULL)
      return (__elf->ehdr_64->e_machine);
   else if (__elf->ehdr_32 != NULL)
      return (__elf->ehdr_32->e_machine);

   return (EM_NONE);
}

/*
 * Reset ELF object file iterator to the first element for
 * an archive file
 * \param __elf an Elf structure
 */
void elf_reset_ar_iterator(Elf* __elf)
{
   if (elf_kind(__elf) == ELF_K_AR)
      __elf->ar->current = 0;
}

/*
 * Returns the number of element in an AR file
 * \param __elf an Elf structure
 * \return the number of element in the AR file
 */
unsigned int elf_get_ar_size(Elf* __elf)
{
   if (__elf == NULL || __elf->ar == NULL)
      return 0;
   return (__elf->ar->nbObjectFiles);
}

/*
 * Returns the machine code of a file. This function performs minimal parsing of the file
 * \param Valid file descriptor
 * \return Machine code of the file, or EM_NONE if the file is not a valid ELF file
 * */
unsigned int get_elf_machine_code(int filedes)
{
   if (filedes <= 0)
      return EM_NONE;
   int error;
   unsigned char ident[EI_NIDENT];  // Used to load ELF M.N.
   int type = ELF_K_NONE;
   /**\todo (2016-12-13) Some of the code below was copy/pasted from elf_begin and _elf_begin64. For the sake of
    * factorisation, it might be interesting to move it to static functions or macros*/

   //get the magic number
   error = lseek(filedes, 0, SEEK_SET);
   if (error < 0)
      return EM_NONE;
   error = read(filedes, ident, EI_NIDENT);
   if (error <= 0)
      return EM_NONE;

   //check the ELF magic word to be sure that it is an ELF file
   if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1
         || ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3)
      return EM_NONE;   //Not an ELF file

   //get the mode (32b / 64b) and load the
   //file using the corresponding function
   switch (ident[EI_CLASS]) {
      Elf64_Ehdr hdr64;
      Elf32_Ehdr hdr32;
   case ELFCLASSNONE:
      return EM_NONE;
   case ELFCLASS32:
      // read the elf header
      error = lseek(filedes, 0, SEEK_SET);
      if (error < 0)
         return EM_NONE;
      error = read(filedes, &hdr32, sizeof(Elf32_Ehdr));
      if (error <= 0)
         return EM_NONE;
      else
         return hdr32.e_machine;
   case ELFCLASS64:
      // read the elf header
      error = lseek(filedes, 0, SEEK_SET);
      if (error < 0)
         return EM_NONE;
      error = read(filedes, &hdr64, sizeof(Elf64_Ehdr));
      if (error <= 0)
         return EM_NONE;
      else
         return hdr64.e_machine;
   default:
      return EM_NONE;
   }


}

// ============================================================================
//            GETTER AND SETTERS FOR ELF OBJECT MEMBERS
// ============================================================================

unsigned char* Elf_Ehdr_get_e_ident(Elf* elf)
{
   if (elf == NULL)
      return NULL;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_ident);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_ident);
   return (NULL);
}

Elf64_Half Elf_Ehdr_get_e_type(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_type);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_type);
   return (0);
}

Elf64_Half Elf_Ehdr_get_e_machine(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_machine);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_machine);
   return (0);
}

Elf64_Word Elf_Ehdr_get_e_version(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_version);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_version);
   return (0);
}

Elf64_Addr Elf_Ehdr_get_e_entry(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_entry);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_entry);
   return (0);
}

Elf64_Off Elf_Ehdr_get_e_phoff(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_phoff);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_phoff);
   return (0);
}

Elf64_Off Elf_Ehdr_get_e_shoff(Elf* elf)
{
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_shoff);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_shoff);
   return (0);
}

Elf64_Word Elf_Ehdr_get_e_flags(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_flags);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_flags);
   return (0);
}

Elf64_Half Elf_Ehdr_get_e_ehsize(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_ehsize);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_ehsize);
   return (0);
}

Elf64_Half Elf_Ehdr_get_e_phentsize(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_phentsize);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_phentsize);
   return (0);
}

Elf64_Half Elf_Ehdr_get_e_phnum(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_phnum);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_phnum);
   return (0);
}

Elf64_Half Elf_Ehdr_get_e_shentsize(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_shentsize);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_shentsize);
   return (0);
}

Elf64_Half Elf_Ehdr_get_e_shnum(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_shnum);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_shnum);
   return (0);
}
Elf64_Half Elf_Ehdr_get_e_shstrndx(Elf* elf)
{
   if (elf == NULL)
      return 0;
   if (elf->ehdr_64 != NULL)
      return (elf->ehdr_64->e_shstrndx);
   else if (elf->ehdr_32 != NULL)
      return (elf->ehdr_32->e_shstrndx);
   return (0);
}

void Elf_Ehdr_set_e_ident(Elf* elf, unsigned char* e_ident)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      memcpy(elf->ehdr_64->e_ident, e_ident, sizeof(unsigned char) * EI_NIDENT);
   else if (elf->ehdr_32 != NULL)
      memcpy(elf->ehdr_32->e_ident, e_ident, sizeof(unsigned char) * EI_NIDENT);
}

void Elf_Ehdr_set_e_type(Elf* elf, Elf64_Half e_type)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_type = e_type;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_type = e_type;
}

void Elf_Ehdr_set_e_machine(Elf* elf, Elf64_Half e_machine)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_machine = e_machine;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_machine = e_machine;
}

void Elf_Ehdr_set_e_version(Elf* elf, Elf64_Word e_version)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_version = e_version;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_version = e_version;
}

void Elf_Ehdr_set_e_entry(Elf* elf, Elf64_Addr e_entry)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_entry = e_entry;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_entry = e_entry;
}

void Elf_Ehdr_set_e_phoff(Elf* elf, Elf64_Off e_phoff)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_phoff = e_phoff;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_phoff = e_phoff;
}

void Elf_Ehdr_set_e_shoff(Elf* elf, Elf64_Off e_shoff)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_shoff = e_shoff;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_shoff = e_shoff;
}

void Elf_Ehdr_set_e_flags(Elf* elf, Elf64_Word e_flags)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_flags = e_flags;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_flags = e_flags;
}

void Elf_Ehdr_set_e_ehsize(Elf* elf, Elf64_Half e_ehsize)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_ehsize = e_ehsize;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_ehsize = e_ehsize;
}

void Elf_Ehdr_set_e_phentsize(Elf* elf, Elf64_Half e_phentsize)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_phentsize = e_phentsize;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_phentsize = e_phentsize;
}

void Elf_Ehdr_set_e_phnum(Elf* elf, Elf64_Half e_phnum)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_phnum = e_phnum;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_phnum = e_phnum;
}

void Elf_Ehdr_set_e_shentsize(Elf* elf, Elf64_Half e_shentsize)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_shentsize = e_shentsize;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_shentsize = e_shentsize;
}

void Elf_Ehdr_set_e_shnum(Elf* elf, Elf64_Half e_shnum)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_shnum = e_shnum;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_shnum = e_shnum;
}

void Elf_Ehdr_set_e_shstrndx(Elf* elf, Elf64_Half e_shstrndx)
{
   if (elf == NULL)
      return;
   if (elf->ehdr_64 != NULL)
      elf->ehdr_64->e_shstrndx = e_shstrndx;
   else if (elf->ehdr_32 != NULL)
      elf->ehdr_32->e_shstrndx = e_shstrndx;
}

Elf64_Word Elf_Shdr_get_sh_name(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_shnum(elf) && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_name);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_name);
      }
   }
   return (0);
}

Elf64_Word Elf_Shdr_get_sh_type(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_shnum(elf) && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_type);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_type);
      }
   }
   return (0);
}

Elf64_Xword Elf_Shdr_get_sh_flags(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_flags);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_flags);
      }
   }
   return (0);
}

Elf64_Addr Elf_Shdr_get_sh_addr(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_addr);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_addr);
      }
   }
   return (0);
}

Elf64_Off Elf_Shdr_get_sh_offset(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_offset);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_offset);
      }
   }
   return (0);
}

Elf64_Xword Elf_Shdr_get_sh_size(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_size);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_size);
      }
   }
   return (0);
}

Elf64_Word Elf_Shdr_get_sh_link(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_link);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_link);
      }
   }
   return (0);
}

Elf64_Word Elf_Shdr_get_sh_info(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_info);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_info);
      }
   }
   return (0);
}

Elf64_Xword Elf_Shdr_get_sh_addralign(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_addralign);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_addralign);
      }
   }
   return (0);
}

Elf64_Xword Elf_Shdr_get_sh_entsize(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            return (elf->scn[indx]->shdr_64->sh_entsize);
         else if (elf->scn[indx]->shdr_32 != NULL)
            return (elf->scn[indx]->shdr_32->sh_entsize);
      }
   }
   return (0);
}

void Elf_Shdr_set_sh_name(Elf* elf, Elf64_Half indx, Elf64_Word sh_name)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_name = sh_name;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_name = sh_name;
      }
   }
}

void Elf_Shdr_set_sh_type(Elf* elf, Elf64_Half indx, Elf64_Word sh_type)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_type = sh_type;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_type = sh_type;
      }
   }
}

void Elf_Shdr_set_sh_flags(Elf* elf, Elf64_Half indx, Elf64_Xword sh_flags)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_flags = sh_flags;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_flags = sh_flags;
      }
   }
}

extern void Elf_Shdr_set_sh_addr(Elf* elf, Elf64_Half indx, Elf64_Addr sh_addr)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_addr = sh_addr;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_addr = sh_addr;
      }
   }
}

void Elf_Shdr_set_sh_offset(Elf* elf, Elf64_Half indx, Elf64_Off sh_offset)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_offset = sh_offset;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_offset = sh_offset;
      }
   }
}

void Elf_Shdr_set_sh_size(Elf* elf, Elf64_Half indx, Elf64_Xword sh_size)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_size = sh_size;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_size = sh_size;
      }
   }
}

void Elf_Shdr_set_sh_link(Elf* elf, Elf64_Half indx, Elf64_Word sh_link)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_link = sh_link;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_link = sh_link;
      }
   }
}

void Elf_Shdr_set_sh_info(Elf* elf, Elf64_Half indx, Elf64_Word sh_info)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_info = sh_info;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_info = sh_info;
      }
   }
}

void Elf_Shdr_set_sh_addralign(Elf* elf, Elf64_Half indx,
      Elf64_Xword sh_addralign)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_addralign = sh_addralign;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_addralign = sh_addralign;
      }
   }
}

void Elf_Shdr_set_sh_entsize(Elf* elf, Elf64_Half indx, Elf64_Xword sh_entisze)
{
   if (elf != NULL) {
      if (indx
            < Elf_Ehdr_get_e_shnum(
                  elf) && elf->scn != NULL && elf->scn[indx] != NULL) {
         if (elf->scn[indx]->shdr_64 != NULL)
            elf->scn[indx]->shdr_64->sh_entsize = sh_entisze;
         else if (elf->scn[indx]->shdr_32 != NULL)
            elf->scn[indx]->shdr_32->sh_entsize = sh_entisze;
      }
   }
}

Elf64_Word Elf_Phdr_get_p_type(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            return (elf->phdr_64[indx].p_type);
         else if (elf->phdr_32 != NULL)
            return (elf->phdr_32[indx].p_type);
      }
   }
   return (0);
}

Elf64_Word Elf_Phdr_get_p_flags(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            return (elf->phdr_64[indx].p_flags);
         else if (elf->phdr_32 != NULL)
            return (elf->phdr_32[indx].p_flags);
      }
   }
   return (0);
}

Elf64_Off Elf_Phdr_get_p_offset(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            return (elf->phdr_64[indx].p_offset);
         else if (elf->phdr_32 != NULL)
            return (elf->phdr_32[indx].p_offset);
      }
   }
   return (0);
}

Elf64_Addr Elf_Phdr_get_p_vaddr(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            return (elf->phdr_64[indx].p_vaddr);
         else if (elf->phdr_32 != NULL)
            return (elf->phdr_32[indx].p_vaddr);
      }
   }
   return (0);
}

Elf64_Addr Elf_Phdr_get_p_paddr(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            return (elf->phdr_64[indx].p_paddr);
         else if (elf->phdr_32 != NULL)
            return (elf->phdr_32[indx].p_paddr);
      }
   }
   return (0);
}

Elf64_Xword Elf_Phdr_get_p_filesz(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            return (elf->phdr_64[indx].p_filesz);
         else if (elf->phdr_32 != NULL)
            return (elf->phdr_32[indx].p_filesz);
      }
   }
   return (0);
}

Elf64_Xword Elf_Phdr_get_p_memsz(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            return (elf->phdr_64[indx].p_memsz);
         else if (elf->phdr_32 != NULL)
            return (elf->phdr_32[indx].p_memsz);
      }
   }
   return (0);
}

Elf64_Xword Elf_Phdr_get_p_align(Elf* elf, Elf64_Half indx)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            return (elf->phdr_64[indx].p_align);
         else if (elf->phdr_32 != NULL)
            return (elf->phdr_32[indx].p_align);
      }
   }
   return (0);
}

void Elf_Phdr_set_p_type(Elf* elf, Elf64_Half indx, Elf64_Word p_type)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            elf->phdr_64[indx].p_type = p_type;
         else if (elf->phdr_32 != NULL)
            elf->phdr_32[indx].p_type = p_type;
      }
   }
}

void Elf_Phdr_set_p_flags(Elf* elf, Elf64_Half indx, Elf64_Word p_flags)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            elf->phdr_64[indx].p_flags = p_flags;
         else if (elf->phdr_32 != NULL)
            elf->phdr_32[indx].p_flags = p_flags;
      }
   }
}

extern void Elf_Phdr_set_p_offset(Elf* elf, Elf64_Half indx, Elf64_Off p_offset)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            elf->phdr_64[indx].p_offset = p_offset;
         else if (elf->phdr_32 != NULL)
            elf->phdr_32[indx].p_offset = p_offset;
      }
   }
}

void Elf_Phdr_set_p_vaddr(Elf* elf, Elf64_Half indx, Elf64_Addr p_vaddr)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            elf->phdr_64[indx].p_vaddr = p_vaddr;
         else if (elf->phdr_32 != NULL)
            elf->phdr_32[indx].p_vaddr = p_vaddr;
      }
   }
}

void Elf_Phdr_set_p_paddr(Elf* elf, Elf64_Half indx, Elf64_Addr p_paddr)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            elf->phdr_64[indx].p_paddr = p_paddr;
         else if (elf->phdr_32 != NULL)
            elf->phdr_32[indx].p_paddr = p_paddr;
      }
   }
}

void Elf_Phdr_set_p_filesz(Elf* elf, Elf64_Half indx, Elf64_Xword p_filesz)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            elf->phdr_64[indx].p_filesz = p_filesz;
         else if (elf->phdr_32 != NULL)
            elf->phdr_32[indx].p_filesz = p_filesz;
      }
   }
}

void Elf_Phdr_set_p_memsz(Elf* elf, Elf64_Half indx, Elf64_Xword p_memsz)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            elf->phdr_64[indx].p_memsz = p_memsz;
         else if (elf->phdr_32 != NULL)
            elf->phdr_32[indx].p_memsz = p_memsz;
      }
   }
}

void Elf_Phdr_set_p_align(Elf* elf, Elf64_Half indx, Elf64_Xword p_align)
{
   if (elf != NULL) {
      if (indx < Elf_Ehdr_get_e_phnum(elf)) {
         if (elf->phdr_64 != NULL)
            elf->phdr_64[indx].p_align = p_align;
         else if (elf->phdr_32 != NULL)
            elf->phdr_32[indx].p_align = p_align;
      }
   }
}

/*
 * Sets the data of an ELF section (updates its \c data member).
 * This function assumes that all the elements of the section have been already set.
 * \param scn The ELF section.
 * \param data Data to set into the section
 * */
void elf_setdata(Elf_Scn* scn, void* data)
{
   if (!scn)
      return;
   //Initialises the data inside the section if it was not already
   if (!scn->data)
      scn->data = malloc(sizeof(*scn->data));

   //Initialises the members of the data
   scn->data->d_buf = data;
   scn->data->d_version = EV_CURRENT;
   scn->data->d_off = 0;
   if (scn->shdr_64) {
      scn->data->d_type = scntype_to_datatype(scn->shdr_64->sh_type);
      scn->data->d_size = scn->shdr_64->sh_size;
      scn->data->d_align = scn->shdr_64->sh_addralign;
   } else if (scn->shdr_32) {
      scn->data->d_type = scntype_to_datatype(scn->shdr_32->sh_type);
      scn->data->d_size = scn->shdr_32->sh_size;
      scn->data->d_align = scn->shdr_32->sh_addralign;
   }
}

// ============================================================================
//            ELF WRITING FUNCTIONS
// ============================================================================
/*
 * Add a filler if needed between current address and addr
 */
static void add_filler(FILE* file, int64_t addr)
{
   int error;

   if (ftell(file) < addr) {
      VERBOSE(1,
            "Offset in file is %#"PRIx64". Adding filler to reach offset %#"PRIx64"\n",
            ftell(file), addr)
      char* buff = malloc(addr - ftell(file));
      memset(buff, 0, addr - ftell(file));
      error = fwrite(buff, sizeof(char), addr - ftell(file), file);
      free(buff);
   }
   if (ftell(file) > addr) {
      VERBOSE(1, "Offset in file is %#"PRIx64". Jumping to offset %#"PRIx64"\n",
            ftell(file), addr)
      fseek(file, addr, SEEK_SET);
   }
}

/**
 * Print the header into a file
 * \param elf Complete ELF structure
 * \param file File stream to write to
 * \return 1 if successful, 0 otherwise
 */
static int elf_save_header(Elf* elf, FILE* file)
{
   if (!elf || !file)
      return 0;
   int error = 0;
   VERBOSE(0, "Writing header at offset %#"PRIx64"\n", ftell(file))
   if (elf->ehdr_64 != NULL)
      error = fwrite(elf->ehdr_64, sizeof(Elf64_Ehdr), 1, file);
   if (elf->ehdr_32 != NULL)
      error = fwrite(elf->ehdr_32, sizeof(Elf32_Ehdr), 1, file);
   return (error == 1);
}

/**
 * Print the segment header into a file
 * \param elf Complete ELF structure
 * \param file File stream to write to
 * \return 1 if successful, 0 otherwise
 */
static int elf_save_segment_header(Elf* elf, FILE* file)
{
   int i, error = 0;

   if (!elf || !file)
      return 0;

   if (ftell(file) < (long) Elf_Ehdr_get_e_phoff(elf))
      add_filler(file, Elf_Ehdr_get_e_phoff(elf));

   VERBOSE(0, "Writing segment header at offset %#"PRIx64"\n", ftell(file))
   if (elf->phdr_64) {
      for (i = 0; i < Elf_Ehdr_get_e_phnum(elf); i++) {
         VERBOSE(1, "Writing segment header entry %i at offset %#"PRIx64"\n", i,
               ftell(file));
         error += fwrite(&elf->phdr_64[i], sizeof(Elf64_Phdr), 1, file);
      }
   } else if (elf->phdr_32) {
      for (i = 0; i < Elf_Ehdr_get_e_phnum(elf); i++) {
         VERBOSE(1, "Writing segment header entry %i at offset %#"PRIx64"\n", i,
               ftell(file));
         error += fwrite(&elf->phdr_32[i], sizeof(Elf32_Phdr), 1, file);
      }
   }
   VERBOSE(0,
         "Number of segment headers successfully written: %u (expected: %u)\n",
         error, Elf_Ehdr_get_e_phnum(elf));
   return (error == Elf_Ehdr_get_e_phnum(elf));
}

/**
 * Print the section header into a file
 * \param elf Complete ELF structure
 * \param file File stream to write to
 * \return 1 if successful, 0 otherwise
 */
static int elf_save_section_header(Elf* elf, FILE* file)
{
   /**\todo TODO (2014-10-07) Improve how we detect the type of file (32/64)*/
   int i, error = 0;
   if (!elf || !file)
      return 0;
   if (ftell(file) < (long) Elf_Ehdr_get_e_shoff(elf))
      add_filler(file, Elf_Ehdr_get_e_shoff(elf));

   VERBOSE(0, "Writing section header at offset %#"PRIx64"\n", ftell(file))
   if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS64) {
      for (i = 0; i < Elf_Ehdr_get_e_shnum(elf); i++) {
         VERBOSE(1, "Writing section header entry %i at offset %#"PRIx64"\n", i,
               ftell(file));
         error += fwrite(elf->scn[i]->shdr_64, sizeof(Elf64_Shdr), 1, file);
      }
      fflush(file);
   } else if (Elf_Ehdr_get_e_ident(elf)[EI_CLASS] == ELFCLASS32) {
      for (i = 0; i < Elf_Ehdr_get_e_shnum(elf); i++) {
         VERBOSE(1, "Writing section header entry %i at offset %#"PRIx64"\n", i,
               ftell(file));
         error += fwrite(elf->scn[i]->shdr_32, sizeof(Elf32_Shdr), 1, file);
      }
      fflush(file);
   }
   VERBOSE(0,
         "Number of section headers successfully written: %u (expected: %u)\n",
         error, Elf_Ehdr_get_e_shnum(elf));
   return (error == Elf_Ehdr_get_e_shnum(elf));
}

/**
 * Print the section bytes into a file
 * \param elf Complete ELF structure
 * \param file File stream to write to
 * \return 1 if successful, 0 otherwise
 */
static int elf_save_bytes_from_sections(Elf* elf, FILE* file)
{
   int i, ret = 1;
   uint64_t error = 0, ref = 0;
   int flag = 0;
   if (!elf || !file)
      return 0;

   VERBOSE(0, "Writing bytes of sections\n")
   for (i = 0; i < Elf_Ehdr_get_e_shnum(elf); i++) {
      unsigned char* out = elf->scn[i]->data->d_buf;
      uint64_t sh_offset = Elf_Shdr_get_sh_offset(elf, i);
      uint64_t sh_size = Elf_Shdr_get_sh_size(elf, i);
      int sh_type = Elf_Shdr_get_sh_type(elf, i);

      if (flag == 0 && sh_offset > Elf_Ehdr_get_e_shoff(elf)) {
         ret &= elf_save_section_header(elf, file);
         flag = 1;
      }
      if (ftell(file) < (long) sh_offset)
         add_filler(file, sh_offset);

      VERBOSE(1, "Writing %"PRIu64" bytes of section %d at offset %"PRIu64"\n",
            sh_size, i, sh_offset)
      if (sh_type != SHT_NOBITS) {
         error += fwrite(out, sizeof(char), sh_size, file);
         ref += sh_size;
      }
      VERBOSE(1,
            "Written %"PRIu64" bytes after writing section %d (%"PRIu64" expected)\n",
            error, i, ref)
   }

   if (flag == 0) {
      if (ftell(file) < (long) Elf_Ehdr_get_e_shoff(elf))
         add_filler(file, Elf_Ehdr_get_e_shoff(elf));
      ret &= elf_save_section_header(elf, file);
   }
   VERBOSE(0, "Written %"PRIu64" bytes for the sections (%"PRIu64" expected)\n",
         error, ref);
   return (error == ref && ret);
}

/*
 * Writes an Elf structure into a file
 * \param elf an elffile to write
 * \return 1 (TRUE) if success, else 0 (FALSE)
 */
int elf_write(Elf* elf, void* stream)
{
   if (elf == NULL || stream == NULL)
      return (0);
   int out = 1;
   VERBOSE(0, "Writing ELF file to stream\n")
   out &= elf_save_header(elf, (FILE*) stream);
   out &= elf_save_segment_header(elf, (FILE*) stream);
   out &= elf_save_bytes_from_sections(elf, (FILE*) stream);

   return (out);
}

// ============================================================================
//            ELF DUPLICATION FUNCTIONS
// ============================================================================

/*
 * Copies an ELF data structure to another
 * \param dest Structure representing the elf data to copy to. Must have already been initialised (see \ref elf_init_sections)
 * \param origin Structure representing the elf data to copy
 * \return 1 if successful, 0 otherwise
 * */
static int elf_data_copy(Elf_Data* dest, Elf_Data* origin)
{
   if (!origin || !dest)
      return 0;
   //Copies the whole structure
   memcpy(dest, origin, sizeof(*dest));
   if (origin->d_buf) {
      //Duplicates the data
      dest->d_buf = malloc(dest->d_size);
      //Copies the data from the original
      memcpy(dest->d_buf, origin->d_buf, dest->d_size);
   }
   return 1;
}

/*
 * Copies an ELF section to another
 * \param dest Structure representing the elf section to copy to. Must have already been initialised (see \ref elf_init_sections)
 * \param origin Structure representing the elf section to copy
 * \return 1 if successful, 0 if if origin is NULL or has neither 32 nor 64 bits header
 * */
int elf_scn_copy(Elf_Scn* dest, Elf_Scn* origin)
{
   if (!origin || !dest)
      return 0;
   if (!((origin->shdr_32 && dest->shdr_32)
         || (origin->shdr_64 && dest->shdr_64)))
      return 0; //Discrepancies in headers between destination and original
   int out = 1;
   if (origin->shdr_32) {  //32 bits
      //Copies section header
      memcpy(dest->shdr_32, origin->shdr_32, sizeof(*dest->shdr_32));
   } else if (origin->shdr_64) { //64 bits
      //Copies section header
      memcpy(dest->shdr_64, origin->shdr_64, sizeof(*dest->shdr_64));
   }
   //Copies section data but not its bytes
   //out = elf_data_copy(dest->data, origin->data);
   memcpy(dest->data, origin->data, sizeof(*dest->data));

   return out;
}

/*
 * Sets the bytes in a section. No other value will be initialised
 * \param scn Structure representing an elf section
 * \param data Bytes to store in the section
 * \return 1 if successful, 0 otherwise
 * */
int elf_scn_setdatabytes(Elf_Scn* scn, unsigned char* data)
{
   if (!scn || !scn->data)
      return 0;
   scn->data->d_buf = data;
   return 1;
}

/*
 * Gets the bytes in a section.
 * \param scn Structure representing an elf section
 * \return Bytes stored in the section
 * */
unsigned char* elf_scn_getdatabytes(Elf_Scn* scn)
{
   if (!scn || !scn->data)
      return NULL;
   return scn->data->d_buf;
}

/*
 * Duplicates an Elf structure for writing
 * \param origin File to copy from. Only the header will be copied
 * \param stream File stream, must be associated to a file opened for writing
 * \return Empty Elf structure
 * */
Elf* elf_copy(Elf* origin, void* stream)
{
   if (!origin || !stream)
      return NULL;
   Elf* elf = _elf_init(fileno((FILE*) stream), ELF_C_WRITE, ELF_K_ELF, 0);
   if (origin->ehdr_32) {
      elf->ehdr_32 = malloc(sizeof(*elf->ehdr_32));
      memcpy(elf->ehdr_32, origin->ehdr_32, sizeof(*elf->ehdr_32));
   } else if (origin->ehdr_64) {
      elf->ehdr_64 = malloc(sizeof(*elf->ehdr_64));
      memcpy(elf->ehdr_64, origin->ehdr_64, sizeof(*elf->ehdr_64));
   }
   return elf;
}

/*
 * Initialises the segments in an Elf file
 * \param elf Structure representing an ELF file.
 * \param phnum Number of segments
 * \return 1 (TRUE) if successful, 0 (FALSE) otherwise (this happens if the file segments have already been initialised)
 * */
int elf_init_segments(Elf* elf, Elf64_Half phnum)
{
   /**\todo (2014-10-08) Find some way to avoid subsequent tests on the size (32/64) each time it is used*/
   if (!elf || elf->phdr_32 || elf->phdr_64)
      return 0;
   if (elf->ehdr_32) {
      //32 bits file
      //Sets number of program segments
      elf->ehdr_32->e_phnum = phnum;
      //Creates array of segments
      if (phnum > 0)
         elf->phdr_32 = malloc(sizeof(*elf->phdr_32) * phnum);
   } else if (elf->ehdr_64) {
      //64 bits file
      //Sets number of program segments
      elf->ehdr_64->e_phnum = phnum;
      //Creates array of segments
      if (phnum > 0)
         elf->phdr_64 = malloc(sizeof(*elf->phdr_64) * phnum);
   }
   return 1;
}

/*
 * Initialises the sections in an Elf file
 * \param elf Structure representing an ELF file.
 * \param shnum Number of sections
 * \return 1 (TRUE) if successful, 0 (FALSE) otherwise (this happens if the file sections have already been initialised)
 * */
int elf_init_sections(Elf* elf, Elf64_Half shnum)
{
   /**\todo (2014-10-08) Find some way to avoid subsequent tests on the size (32/64) each time it is used*/
   if (!elf || elf->scn)
      return 0;
   if (elf->ehdr_32) {
      //32 bits file
      //Sets number of sections
      elf->ehdr_32->e_shnum = shnum;
      //Creates array of sections
      if (shnum > 0) {
         Elf32_Half i;
         elf->scn = malloc(sizeof(*elf->scn) * shnum);
         for (i = 0; i < shnum; i++) {
            elf->scn[i] = malloc(sizeof(*elf->scn[i]));
            elf->scn[i]->shdr_32 = malloc(sizeof(*elf->scn[i]->shdr_32));
            elf->scn[i]->elf = elf;
            elf->scn[i]->data = malloc(sizeof(*elf->scn[i]->data));
         }
      }
   } else if (elf->ehdr_64) {
      //64 bits file
      //Sets number of sections
      elf->ehdr_64->e_shnum = shnum;
      //Creates array of sections
      if (shnum > 0) {
         Elf64_Half i;
         elf->scn = malloc(sizeof(*elf->scn) * shnum);
         for (i = 0; i < shnum; i++) {
            elf->scn[i] = malloc(sizeof(*elf->scn[i]));
            elf->scn[i]->shdr_64 = malloc(sizeof(*elf->scn[i]->shdr_64));
            elf->scn[i]->elf = elf;
            elf->scn[i]->data = malloc(sizeof(*elf->scn[i]->data));
         }
      }
   }
   return 1;
}
