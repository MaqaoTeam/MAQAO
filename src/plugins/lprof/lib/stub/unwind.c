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

/* Defines libunwind call-back routines */

#ifdef __LIBUNWIND__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dwarf.h>
#include "libelf.h"
#include <libunwind.h>

#include <sys/mman.h>
// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libmcommon.h" // array_t
#include "unwind.h" // unwind_context_t

extern int
UNW_OBJ(dwarf_search_unwind_table) (unw_addr_space_t as,
				    unw_word_t ip,
				    unw_dyn_info_t *di,
				    unw_proc_info_t *pi,
				    int need_unwind_info, void *arg);
#define dwarf_search_unwind_table UNW_OBJ(dwarf_search_unwind_table)

#ifdef HAVE_DWARF
extern int
UNW_OBJ(dwarf_find_debug_frame) (int found, unw_dyn_info_t *di_debug,
				 unw_word_t ip,
				 unw_word_t segbase,
				 const char *obj_name, unw_word_t start,
				 unw_word_t end);
#define dwarf_find_debug_frame UNW_OBJ(dwarf_find_debug_frame)
#endif

#ifdef HAVE_LIBELF_MMAP_SUPPORT
#  define LPROF_ELF_C_READ_MMAP ELF_C_READ_MMAP
#else
#  define LPROF_ELF_C_READ_MMAP ELF_C_READ
#endif

#ifdef HAVE_DWARF
// OK
static int elf_is_exec (int fd)
{
   Elf *elf = elf_begin (fd, LPROF_ELF_C_READ_MMAP, NULL);
   if (!elf) return 0;

   Elf64_Ehdr *ehdr = elf64_getehdr (elf);
   if (ehdr == NULL) {
      elf_end (elf);
      return 0;
   }

   int ret = (ehdr->e_type == ET_EXEC);
   elf_end (elf);
   return ret;
}
#endif

static ssize_t read_dw_encoded_value (uint8_t *data, unsigned char enc,
                                      uint64_t cur, uint64_t *value)
{
   if (enc == DW_EH_PE_omit  ) { *value = 0; return 0; }
   if (enc == DW_EH_PE_absptr) { *value = *((uint64_t *) data); return sizeof *value; }

   int64_t number = 0;
   size_t size;
   switch (enc & 0xf) {
   case DW_EH_PE_udata2:
      number = *((uint16_t *) data);
      size = 2;
      break;

   case DW_EH_PE_sdata2:
      number = *((int16_t *) data);
      size = 2;
      break;

   case DW_EH_PE_udata4:
      number = *((uint32_t *) data);
      size = 4;
      break;

   case DW_EH_PE_sdata4:
      number = *((int32_t *) data);
      size = 4;
      break;

   case DW_EH_PE_udata8:
      number = *((uint64_t *) data);
      size = 8;
      break;

   case DW_EH_PE_sdata8:
      number = *((int64_t *) data);
      size = 8;
      break;

   default:
      DBGMSG ("unwind/cannot parse %d encoding in .eh_frame_hdr\n", (int) enc);
      return -1;
   }

   switch (enc & 0xf0) {
   case DW_EH_PE_absptr:
      *value = number;
      break;

   case DW_EH_PE_pcrel:
      *value = cur + number;
      break;

   default:
      return -1;
   }

   return size;
}

static int decode_eh_frame_header (uint8_t *data, size_t pos,
                                   uint64_t *table_data, uint64_t *fde_count)
{
    ssize_t size;

    uint8_t version = data[0];
    if (version != 1) {
       DBGMSG ("unwind/decode_eh_frame_header: invalid .eh_frame_hdr version = %hhu\n", version);
       return -1;
    }
    char eh_frame_ptr_enc = data[1];
    char fde_count_enc    = data[2];

    // Go to eh_frame_ptr field (encoded) and read (skip) it
    data += 4; pos += 4;
    uint64_t eh_frame_ptr;
    size = read_dw_encoded_value (data, eh_frame_ptr_enc, pos, &eh_frame_ptr);
    if (size < 0) return -1;

    // Go to fde_count field (encoded) and read it
    data += size; pos += size;
    size = read_dw_encoded_value (data, fde_count_enc, pos, fde_count);
    if (size < 0) return -2;

    *table_data = pos + size;

    return 0;
}

static int read_eh_frame_header (int fd, uint64_t *table_data,
                                 uint64_t *segbase, uint64_t *fde_count)
{
   Elf *elf = elf_begin (fd, LPROF_ELF_C_READ_MMAP, NULL);
   if (!elf) return -1;

   Elf64_Ehdr *ehdr = elf64_getehdr (elf);
   if (ehdr == NULL) {
      elf_end (elf);
      return -2;
   }

   int offset = 0;
   unsigned i;
   for (i = 0; i < ehdr->e_shnum; i++) {
      Elf_Scn *scn  = elf_getscn (elf, i);
      Elf64_Shdr *shdr = elf64_getshdr (scn);
      if (shdr == NULL) {
         DBGMSG0 ("unwind/read_eh_frame_header: cannot get section header\n");
         break;
      }

      char *str = elf_strptr (elf, ehdr->e_shstrndx, shdr->sh_name);
      if (str != NULL && !strcmp (str, ".eh_frame_hdr")) {
         Elf_Data *data = NULL;

         if ((data = elf_getdata (scn, data)) == NULL) {
            DBGMSG0 ("unwind/read_eh_frame_header: cannot get .eh_frame_hdr data\n");
            break;
         }

         offset = *segbase = shdr->sh_offset;
         decode_eh_frame_header (data->d_buf, offset, table_data, fde_count);
         break;
      }
   }

   elf_end (elf);
   return offset ? 0 : -3;
}

int cmp_maps (const void *a, const void *b)
{
   const map_t *ma = a;
   const map_t *mb = *((map_t **) b);

   if (ma->start >= mb->start && ma->end < mb->end) return 0;
   return ma->start > mb->start ? 1 : -1;
}

// Improve me (bsearch on data structure...)
static map_t *find_map (unw_word_t ip, unwind_context_t *context)
{
   array_t *maps = context->maps;
   map_t key = { .start = ip, .end = ip };
   map_t **found = bsearch (&key, maps->mem, array_length (maps), sizeof maps->mem[0], cmp_maps);
   DBG (if (found) {
         map_t *map = *found;
         fprintf (stderr, "Found map for ip=%"PRIx64": %"PRIx64"-%"PRIx64" (%s)\n",
                  ip, map->start, map->end, map->name);
      });
   return found ? *found : NULL;
}

static int find_proc_info(unw_addr_space_t as, unw_word_t ip,
                          unw_proc_info_t *pip, int need_unwind_info, void *arg)
{
   DBGMSG ("find_proc_info (as=%p, ip=%"PRIx64", arg=%p)\n", as, ip, arg);
   unwind_context_t *context = arg;   

   map_t *map = find_map (ip, context);
   if (!map) {
      DBGMSG ("No map found for ip=%"PRIx64"\n", ip);
      return -UNW_EINVAL;
   }
   if (map->di == NULL) {
      uint64_t table_data = 0, segbase = 0, fde_count = 0;
      int map_fd = open (map->name, O_RDONLY);
      if (!read_eh_frame_header (map_fd, &table_data, &segbase, &fde_count)) {
         map->di = lc_malloc0 (sizeof *(map->di));
         map->di->format   = UNW_INFO_FORMAT_REMOTE_TABLE;
         map->di->start_ip = map->start;
         map->di->end_ip   = map->end;
         map->di->u.rti.segbase    = map->start + segbase - map->offset;
         map->di->u.rti.table_data = map->start + table_data - map->offset;
         map->di->u.rti.table_len  = fde_count * sizeof(uint64_t) / sizeof(unw_word_t);
      }
      close (map_fd);
   }

   int ret = -UNW_EINVAL;

   if (map->di != NULL) {
      ret = dwarf_search_unwind_table (as, ip, map->di, pip, need_unwind_info, arg);
      if (ret == 0) return 0;
   }

#ifdef HAVE_DWARF
   // TODO: improve this (di caching) if used and useful
   int map_fd = open (map->name, O_RDONLY);
   unw_word_t base = elf_is_exec (map_fd) ? 0 : map->start;
   close (map_fd);
   unw_dyn_info_t di; memset (&di, 0, sizeof di);
   if (dwarf_find_debug_frame (0, &di, &ip, base, map->name,
                               (unw_word_t) map->start,
                               (unw_word_t) map->end)) {
      ret = dwarf_search_unwind_table (as, ip, &di, pip, need_unwind_info, arg);
      return ret;
   }
#endif

   return ret;
}

// OK
static void put_unwind_info(unw_addr_space_t as,
                            unw_proc_info_t *pip, void *arg)
{
   // unused
   (void) as;
   (void) pip;
   (void) arg;
}

// OK
static int get_dyn_info_list_addr(unw_addr_space_t as,
                                  unw_word_t *dilap, void *arg)
{
   // unused
   (void) as;
   (void) dilap;
   (void) arg;
   return -UNW_ENOINFO;
}

// Optimize me (AVL tree, cache, mutex...)
static int access_mem_ext (unwind_context_t *context, unw_word_t addr,
                           unw_word_t *data)
{
   map_t *map = find_map (addr, context);
   if (!map) {
      DBGMSG ("Cannot find map matching %"PRIx64"\n", addr);
      return -1;
   }

   int map_fd = map->fd;
   if (map_fd < 0) {
      // First time: open file and map it to userspace
      map_fd = open (map->name, O_RDONLY);
      if (map_fd < 0) {
         DBGMSG ("Cannot read-only %s\n", map->name);
         return -1;
      }

      struct stat stbuf;
      if (fstat (map_fd, &stbuf) < 0) {
         DBGMSG ("Cannot stat %s size\n", map->name);
         close (map_fd);
         return -1;
      }

      if (stbuf.st_size >= 0 && map->offset > (uint64_t) stbuf.st_size) {
         DBGMSG ("offset=%"PRIu64" > filesize=%"PRIu64"\n", map->offset, stbuf.st_size);
         close (map_fd);
         return -1;
      }

      size_t mmap_len;
      if ((map->end - map->start) > (stbuf.st_size - map->offset))
         mmap_len = stbuf.st_size - map->offset;
      else
         mmap_len = map->end - map->start;

      void *mmap_data = mmap (NULL, mmap_len, PROT_READ, MAP_SHARED, map_fd, map->offset);
      if (mmap_data == MAP_FAILED) {
         DBGMSG ("Cannot map %s+%"PRIx64" to userspace\n", map->name, map->offset);
         perror ("mmap");
         close (map_fd);
         return -1;
      }

      map->fd = map_fd;
      map->length = mmap_len;
      map->data   = mmap_data;
   }

   off_t fd_offset = addr - (uint64_t) map->start;
   *data = *(unw_word_t *) (map->data + fd_offset);
   /* off_t fd_offset = map->offset + addr - map->start; */
   /* if (pread (map_fd, data, sizeof *data, fd_offset) < (long) sizeof *data) */
   /*    return -3; */

   DBGMSG ("Read %"PRIx64" from %s+%lu\n", *data, map->name, fd_offset);
   return 0;
}

// OK
static int access_mem(unw_addr_space_t as, unw_word_t addr,
                      unw_word_t *valp, int write, void *arg)
{
   (void) as;
   unwind_context_t *context = arg;

   // No write support
   if (write) {
      *valp = 0; return 0;
   }

   uint64_t start, end;
   start = context->sp;
   end = start + PERF_STACK_USER_SIZE;
   DBGMSG ("looking offset of %"PRIx64" in %"PRIx64"-%"PRIx64"\n",
           addr, start, end);
   if (addr + sizeof *valp < addr) return -UNW_EINVAL; // overflow check

   if (addr < start || addr + sizeof *valp >= end) {
      DBGMSG0 ("out of range => DSO ?\n");
      int ret = access_mem_ext (context, addr, valp);
      if (ret) {
         DBGMSG0 ("Failed to access unstack map\n");
         *valp = 0;
         return ret;
      }
      return 0;
   }
    
   unsigned offset = addr - start;
   *valp = *((unw_word_t *) &(context->stack [offset]));
   DBGMSG ("addr=%p val=%"PRIx64" offset=%u\n",
           (void *) addr, *valp, offset);
   return 0;
}

// OK
static int access_reg(unw_addr_space_t as, unw_regnum_t regnum,
                      unw_word_t *valp, int write, void *arg)
{
   (void) as; // unused

   if (write) return 0; // No write support

   unwind_context_t *context = arg;

   switch (regnum) {
   default:
      *valp = 0; DBGMSG ("unwind/access_reg: unknown (regnum=%d)\n", regnum);
   }

   return 0;
}

// OK
static int access_fpreg(unw_addr_space_t as, unw_regnum_t regnum,
                        unw_fpreg_t *fpvalp, int write, void *arg)
{
   // unused
   (void) as;
   (void) regnum;
   (void) fpvalp;
   (void) write;
   (void) arg;

   return -UNW_EINVAL;
}

// OK
static int resume(unw_addr_space_t as, unw_cursor_t *cp, void *arg)
{
   // unused
   (void) as;
   (void) cp;
   (void) arg;

   return -UNW_EINVAL;
}

// OK
static int get_proc_name(unw_addr_space_t as, unw_word_t addr, char *bufp,
                         size_t buf_len, unw_word_t *offp, void *arg)
{
   // unused
   (void) as;
   (void) addr;
   (void) bufp;
   (void) buf_len;
   (void) offp;
   (void) arg;
   
   return -UNW_EINVAL;
}

static unw_accessors_t unw_accessors = {
   .find_proc_info = find_proc_info,
   .put_unwind_info = put_unwind_info,
   .get_dyn_info_list_addr = get_dyn_info_list_addr,
   .access_mem = access_mem,
   .access_reg = access_reg,
   .access_fpreg = access_fpreg,
   .resume = resume,
   .get_proc_name = get_proc_name,
};

unw_accessors_t *get_unw_accessors (void)
{
   return &unw_accessors;
}

#endif
