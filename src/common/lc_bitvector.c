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
#include "libmcommon.h"

/* Added for more readability */
typedef bitvector_chunk_t chunk_t;

/* Returns the size in bits of a chunk */
#define CHUNK_SIZE (8 * sizeof (chunk_t))

/* Returns the number of chunks in vector, from a given number of bits */
#define GET_LENGTH_FROM_BITS(b) (((b) + CHUNK_SIZE - 1) / CHUNK_SIZE)

/* Returns the size in chunks of a bitvector */
#define GET_CHUNKLENGTH(X) GET_LENGTH_FROM_BITS((X)->bits)

/**
 * mask[i] = 2^i-1. Used to mask the last slot of a bitvector
 * when not used at full capacity.
 * That is, for a given bitvector bv, if bv->bits%CHUNK_SIZE != 0,
 * use bv->vector[GET_CHUNKLENGTH(bv)-1] & mask[bv->bits%CHUNK_SIZE]
 * instead of bv->vector[GET_CHUNKLENGTH(bv)-1]
 */
static unsigned mask[] = { 0, 1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff, 0x1ff,
      0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff, 0x1ffff, 0x3ffff,
      0x7ffff, 0xfffff, 0x1fffff, 0x3fffff, 0x7fffff, 0xffffff, 0x1ffffff,
      0x3ffffff, 0x7ffffff, 0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
      0xffffffff };

///////////////////////////////////////////////////////////////////////////////
//                           bitvector functions                             //
///////////////////////////////////////////////////////////////////////////////

/* REMARK: for performance reason, bv->vector is not realloced when bv->bits is decreased */

/*
 * \TODO:
 *  - reduce size of bitvector.bits
 *  - add variants of cutleft and cutright taking an extra bitvector in parameter instead of returning a new one
 *  - avoid calling realloc when resizing: add an extra member in the structure or... ?
 *  - rename some functions
 *  - rewrite or redesign new_from_str and printdeclaration_from_binstring
 *  - try to remove one or two functions, especially by discussing with Cédric Valensi
 *  - think to modify some functions to have offsets starting from left (much more intuitive)
 */

/* Used only by _get_tmp and _free_tmp */
#define TMP_VECTOR_MAX_BYTES 100
#define TMP_CHUNKS_AVAILABLE (TMP_VECTOR_MAX_BYTES / sizeof (chunk_t))

/**
 * Returns a temporary bitvector, statically allocated
 * If need to store more than TMP_VECTOR_MAX_BYTES bytes,
 * dynamic memory is used (and will be freed by _free_tmp)
 */
static bitvector_t *_get_tmp(size_t bitlen)
{
   static bitvector_t tmp;
   static chunk_t vector[TMP_CHUNKS_AVAILABLE];

   const size_t chunks_required = GET_LENGTH_FROM_BITS(bitlen);

   tmp.bits = bitlen;
   tmp.vector = vector;

   if (chunks_required > TMP_CHUNKS_AVAILABLE) {
      tmp.vector = lc_malloc(chunks_required * sizeof(chunk_t));
   }

   return &tmp;
}

/**
 * Frees a bitvector returned by _get_tmp
 */
static void _free_tmp(bitvector_t *tmp)
{
   if (GET_CHUNKLENGTH (tmp) > TMP_CHUNKS_AVAILABLE)
      lc_free(tmp->vector);
}

/**
 * If memcpy is not inlined by the compiler, faster to use a copy loop
 * This function exists for performance reason => no tests
 * TODO: should auto-tune (if first call, rdtsc benchmark)
 */
static void _vector_cpy(const chunk_t *src, chunk_t *dst, size_t nb_chunks)
{
   /* We consider that copying less than 100 bytes chunk by chunk
    * is faster than calling memcpy and waiting for its completion */
   if (nb_chunks * sizeof(chunk_t) < 100) {
      unsigned i;

      for (i = 0; i < nb_chunks; i++)
         dst[i] = src[i];
   } else
      memcpy(dst, src, nb_chunks * sizeof(chunk_t));
}

/*
 * Creates a \e len bits bitvector, with all bits set to 0
 * \param len bitvector length, in bits
 * \return new bitvector
 */
bitvector_t *bitvector_new(size_t len)
{
   bitvector_t *new = lc_malloc(sizeof *new);
   if (!new)
      return NULL; /* TODO: remove this if lc_malloc aborts if malloc returns NULL */

   new->bits = len;

   if (len > 0)
      new->vector = lc_malloc0(
            GET_LENGTH_FROM_BITS(len) * sizeof *(new->vector));
   else
      new->vector = NULL;

   return new;
}

/*
 * Copies a bitvector \e src to another already allocated bitvector \e dst
 * If not enough chunks in \e dst, copy exits with leaving \e dst unchanged
 * \param src source bitvector
 * \param dst destination bitvector
 */
void bitvector_copy(const bitvector_t *src, bitvector_t *dst)
{
   if (!src || !dst)
      return;

   if (GET_CHUNKLENGTH (dst) < GET_CHUNKLENGTH(src)) {
      /* TODO: discuss if auto-realloc here */
      fputs(
            "bitvector_copy: too small destination bitvector, nothing is done\n",
            stderr);
      return;
   }

   if (src->bits > 0) {
      _vector_cpy(src->vector, dst->vector, GET_CHUNKLENGTH(src));
   }

   dst->bits = src->bits;
}

/*
 * Returns a copy of a bitvector
 * \param src source bitvector
 * \return copy
 */
bitvector_t *bitvector_dup(const bitvector_t *src)
{
   if (!src)
      return NULL;

   bitvector_t *new = bitvector_new(src->bits);
   if (!new)
      return NULL; /* TODO: remove this if lc_malloc aborts if malloc returns NULL */

   bitvector_copy(src, new);

   return new;
}

/*
 * Resizes a bitvector. Memory is reallocated only if more chunks are needed
 * \param bv a bitvector
 * \param new_len new length in bits
 */
void bitvector_resize(bitvector_t *bv, size_t new_len)
{
   if (!bv)
      return;

   const size_t chunks_required = GET_LENGTH_FROM_BITS(new_len);
   const size_t chunks_available = GET_LENGTH_FROM_BITS(bv->bits);

   if (chunks_required > chunks_available)
      bv->vector = lc_realloc(bv->vector,
            chunks_required * sizeof *(bv->vector));

   bv->bits = new_len;
}

/*
 * Frees a bitvector structure
 * \param bv a bitvector
 */
void bitvector_free(bitvector_t *bv)
{
   if (!bv)
      return;

   lc_free(bv->vector);
   lc_free(bv);
}

static chunk_t _bitvector_and(chunk_t a, chunk_t b)
{
   return a & b;
}
static chunk_t _bitvector_or(chunk_t a, chunk_t b)
{
   return a | b;
}
static chunk_t _bitvector_xor(chunk_t a, chunk_t b)
{
   return a ^ b;
}

static void bitvector_bitwise(bitvector_t *res, const bitvector_t *a,
      const bitvector_t *b, chunk_t (*f)(chunk_t, chunk_t))
{
   if (!res || !a || !b)
      return;

   res->bits = (a->bits <= b->bits) ? a->bits : b->bits;

   unsigned i;
   for (i = 0; i < GET_CHUNKLENGTH(res); i++)
      res->vector[i] = f(a->vector[i], b->vector[i]);
}

/*
 * Performs a bit-to-bit logical AND operation on two bitvectors \e a and \e b
 * In other words, res <= a & b
 * \param res resulting bitvector, assumed already initialized
 * \param a left source bitvector
 * \param b right source bitvector
 */
void bitvector_and(bitvector_t *res, const bitvector_t *a, const bitvector_t *b)
{
   bitvector_bitwise(res, a, b, _bitvector_and);
}

/*
 * Performs a bit-to-bit logical OR operation on two bitvectors \e a and \e b
 * In other words, res <= a | b
 * \param res resulting bitvector, assumed already allocated
 * \param a left source bitvector
 * \param b right source bitvector
 */
void bitvector_or(bitvector_t *res, const bitvector_t *a, const bitvector_t *b)
{
   bitvector_bitwise(res, a, b, _bitvector_or);
}

/*
 * Performs a bit-to-bit logical XOR operation on two bitvectors \e a and \e b
 * In other words, res <= a ^ b
 * \param res resulting bitvector, assumed already allocated
 * \param a left source bitvector
 * \param b right source bitvector
 */
void bitvector_xor(bitvector_t *res, const bitvector_t *a, const bitvector_t *b)
{
   bitvector_bitwise(res, a, b, _bitvector_xor);
}

/*
 * Performs a bit-to-bit logical NOT operation on a bitvector \e bv
 * In other words, res <= ~bv
 * \param res resulting bitvector, assumed already allocated
 * \param bv a bitvector
 */
void bitvector_not(bitvector_t *res, const bitvector_t *bv)
{
   if (!res || !bv)
      return;

   res->bits = bv->bits;

   unsigned i;
   for (i = 0; i < GET_CHUNKLENGTH(res); i++)
      res->vector[i] = ~(bv->vector[i]);
}

/*
 * Prints a bitvector (in binary form)
 * \param bv a bitvector to print
 * \param out destination file (already open)
 */
void bitvector_print(const bitvector_t *bv, FILE* out)
{
   if (!bv || !out)
      return;

   int i;
   /* for each bit, from the most to the least significant */
   for (i = bv->bits - 1; i >= 0; i--) {
      fprintf(out, "%d", BITVECTOR_GET_BIT(bv, i));

      if (i % 8 == 0)
         fprintf(out, "|");
      if (i % CHUNK_SIZE == 0)
         fprintf(out, "|");
   }
}

/*
 * Dumps the raw contents of a \e bv bitvector in the following format:
 * { V[0],V[1], ... V[<GET_CHUNKLENGTH(bv)>-1]} - Bitsize=<bv->bits> (no '\n')
 * with V[i] = bv->vector[i] in decimal format.
 * \note this function is designed for debugging
 * \param bv a bitvector to print
 * \param out destination file (already open)
 */
void bitvector_dump(const bitvector_t* bv, FILE* out)
{
   if (!bv || !out)
      return;

   const unsigned bits_in_leftmost_chunk = bv->bits % CHUNK_SIZE;
   if (bits_in_leftmost_chunk > 0)
      fprintf(out, "{ %lu",
            (unsigned long) bv->vector[0] & mask[bits_in_leftmost_chunk]);
   else
      fprintf(out, "{ %lu", (unsigned long) bv->vector[0]);

   unsigned i;
   for (i = 1; i < GET_CHUNKLENGTH(bv); i++)
      fprintf(out, ",%lu", (unsigned long) bv->vector[i]);

   fprintf(out, "} - Bitsize=%u", bv->bits);
}

/*
 * Prints a bitvector (in binary form) to an already allocated string
 * \param bv bitvector to print
 * \param out destination string
 * \param size size of the destination string
 */
void bitvector_binprint(const bitvector_t *bv, char *out, size_t size)
{
   if (!bv || !out)
      return;

   int i, char_limit = (size_t) out + size, written = 0;
   /* for each bit, from the most to the least significant */
   for (i = bv->bits - 1; i >= 0; i--) {
      lc_sprintf(out++, char_limit - written, "%d", BITVECTOR_GET_BIT(bv, i));
      written++;
   }

   *out = '\0';
}

/*
 * Prints a bitvector in hexadecimal format
 * \param bv bitvector to print
 * \param our destination string
 * \param size size of the destination string
 * \param sep string to print between each byte
 */
void bitvector_hexprint(const bitvector_t *bv, char *out, size_t size,
      char* sep)
{
   if (!bv || bv->bits == 0 || !out)
      return;

   int i, j;
   size_t char_limit = size, written = 0;

   /********************** Leftmost chunk **********************/
   unsigned bytes_in_leftmost_chunk = sizeof(chunk_t);
   if (bv->bits % CHUNK_SIZE > 0)
      bytes_in_leftmost_chunk = (bv->bits % CHUNK_SIZE) / 8;

   /* Leftmost byte, potentially containing outside bits */
   const chunk_t leftmost_byte_mask = bv->bits % 8 ? mask[bv->bits % 8] : 0xFF;
   PRINT_INSTRING(out, char_limit - written, "%s%02x", sep,
         (bv->vector[GET_CHUNKLENGTH(bv) - 1] >> (bytes_in_leftmost_chunk - 1) * 8) & leftmost_byte_mask);
   written += sizeof(sep) + 2;

   /* Other bytes */
   for (j = bytes_in_leftmost_chunk - 2; j >= 0; j--) {
      PRINT_INSTRING(out, char_limit - written, "%s%02x", sep,
            (bv->vector[GET_CHUNKLENGTH(bv) - 1] >> (j * 8)) & 0xFF);
      written += sizeof(sep) + 2;
   }

   /*********************** Other chunks ***********************/
   for (i = GET_CHUNKLENGTH(bv) - 2; i >= 0; i--) {
      for (j = sizeof(chunk_t) - 1; j >= 0; j--) {
         PRINT_INSTRING(out, char_limit - written, "%s%02x", sep,
               (bv->vector[i] >> (j * 8)) & 0xFF);
         written += sizeof(sep) + 2;
      }
   }

   *out = '\0';
}

/*
 * Sets a bitvector (all bits to 1)
 * \param bv a bitvector
 */
void bitvector_set(bitvector_t *bv)
{
   if (!bv)
      return;

   memset(bv->vector, 1, GET_CHUNKLENGTH (bv) * sizeof *(bv->vector));
}

/*
 * Clears a bitvector (sets all bits to 0)
 * \param bv a bitvector
 */
void bitvector_clear(bitvector_t *bv)
{
   if (!bv)
      return;

   memset(bv->vector, 0, GET_CHUNKLENGTH (bv) * sizeof *(bv->vector));
}

/*
 * Fills a bitvector \e bv from an array of chunks
 * \param bv a bitvector, assumed already allocated
 * \param array array of integers to be converted to bits
 */
void bitvector_fill_from_chunks(bitvector_t *bv, const chunk_t *array)
{
   if (!bv)
      return;

   _vector_cpy(array, bv->vector, GET_CHUNKLENGTH(bv));
}

/*
 * Reads \e len bits from bitvector \e src starting at position \e offset
 * and copy them to into bitvector \e dst
 * In other words, copies src [offset ; offset+len-1] into dst [0 ; len-1].
 * dst size is not modified
 * If offset = 0 and len = src->bits (not tested for performance reason), bitvector_copy is faster
 * \param src source bitvector
 * \param dst destination bitvector, assumed already allocated
 * \param offset index of the first bit to read
 * \param len number of bits to read
 */
void bitvector_read(const bitvector_t *src, bitvector_t *dst, size_t offset,
      size_t len)
{
   /* If nothing to read or if no destination bitvector */
   if (!src || len == 0 || !dst)
      return;

   /* If too small source bitvector */
   if (src->bits < len) {
      DBGMSG("[bitvector_read] Cannot read %zu bits into a %u bits bitvector\n",
            len, src->bits);
      return;
   }

   /* If attempt to read bits out of the source bitvector */
   if (offset > src->bits - len) {
      DBGMSG(
            "[bitvector_read] Cannot extract bits %zu-%zu in a %u bits bitvector\n",
            offset, offset + len - 1, src->bits);
      return;
   }

   /* If too small destination bitvector */
   if (GET_CHUNKLENGTH(dst) < GET_LENGTH_FROM_BITS(len)) {
      DBGMSG(
            "[bitvector_read] Too small destination bitvector (%zu chunks needed but only %lu available)\n",
            GET_LENGTH_FROM_BITS (len), GET_CHUNKLENGTH (dst));
      return;
   }

   /* The simplest algorithm is: */
   /* for (i=0; i<len; i++) */
   /*   if (BITVECTOR_GET_BIT (src, i+offset) == 1) */
   /*     BITVECTOR_SET_BIT (dst, i); */
   /*   else */
   /*     BITVECTOR_CLR_BIT (dst, i); */

   /* But, for performance reason, copy will be chunk-wide */

   const unsigned chunk_offset = offset / CHUNK_SIZE;
   const unsigned chunk_shift = offset % CHUNK_SIZE;

   /* Easy case: no need to shift bits, just copy from offset */
   if (chunk_shift == 0) {
      _vector_cpy(src->vector + chunk_offset, dst->vector,
            GET_LENGTH_FROM_BITS(len));
   }

   /* Hard case */
   /*
    * Illustration with 8-bits chunks, chunk_shift=5 and len=17
    * Logically:
    * src: [-----AAA][BBBBBCCC][DDEEEF--]
    * dst: [AAABBBBB][CCCDDEEE][F-------]
    * Physically:
    * src: [AAA-----][CCCBBBBB][--FEEEDD]
    * dst: [BBBBBAAA][EEEDDCCC][-------F]
    */
   else {
      unsigned i;

      /* All but last chunks */
      for (i = 0; i < GET_LENGTH_FROM_BITS (len) - 1; i++) {
         dst->vector[i] = (src->vector[i + chunk_offset + 0] >> chunk_shift)
               | (src->vector[i + chunk_offset + 1]
                     << (CHUNK_SIZE - chunk_shift));
      }

      /* Last chunk: avoid reading after the last source chunk */
      if (i + chunk_offset + 1 == GET_CHUNKLENGTH(src))
         dst->vector[i] = src->vector[i + chunk_offset] >> chunk_shift;
      else
         dst->vector[i] = (src->vector[i + chunk_offset + 0] >> chunk_shift)
               | (src->vector[i + chunk_offset + 1]
                     << (CHUNK_SIZE - chunk_shift));
   }
}

/*
 * Writes \e len bits into bitvector \e dst starting at position \e offset,
 * bits are copied from bitvector \e src
 * In other words, copies src [0 ; len-1] into dst [offset ; offset+len-1].
 * If offset = 0 and len = src->bits (not tested for performance reason), bitvector_copy is faster
 * \param src source bitvector
 * \param dst destination bitvector, assumed already allocated
 * \param offset index of the first bit to write in \e dst
 * \param len number of bits to write
 */
void bitvector_write(const bitvector_t *src, bitvector_t *dst, size_t offset,
      size_t len)
{
   /* If nothing to write or if no destination register */
   if (!src || len == 0 || !dst)
      return;

   /* If too small destination bitvector */
   if (dst->bits < len) {
      DBGMSG(
            "[bitvector_write] Cannot write %zu bits into a %u bits bitvector\n",
            len, dst->bits);
      return;
   }

   /* If attempt to write bits out of the destination bitvector */
   if (offset > dst->bits - len) {
      DBGMSG(
            "[bitvector_write] Cannot write bits %zu-%zu in a %u bits bitvector\n",
            offset, offset + len - 1, dst->bits);
      return;
   }

   /* If too small source bitvector */
   if (GET_CHUNKLENGTH(src) < GET_LENGTH_FROM_BITS(len)) {
      DBGMSG(
            "[bitvector_write] Too small source bitvector (%zu chunks needed but only %lu available)\n",
            GET_LENGTH_FROM_BITS (len), GET_CHUNKLENGTH (src));
      return;
   }

   const unsigned chunk_offset = offset / CHUNK_SIZE;
   const unsigned chunk_shift = offset % CHUNK_SIZE;

   /* Easy case: no need to shift bits, just copy from offset */
   if (chunk_shift == 0) {
      /* Copy all but last chunks */
      _vector_cpy(src->vector, dst->vector + chunk_offset,
            GET_LENGTH_FROM_BITS (len) - 1);

      /* Copy last chunk */
      const unsigned last = GET_LENGTH_FROM_BITS (len) - 1;

      if (len % CHUNK_SIZE == 0)
         dst->vector[last + chunk_offset] = src->vector[last];
      else {
         const chunk_t mask = (1 << len % CHUNK_SIZE) - 1; /* 0..01..1 */
         dst->vector[last + chunk_offset] = (dst->vector[last + chunk_offset]
               & ~mask) | (src->vector[last] & mask);
      }
   }

   /* Hard case */
   /*
    * Illustration with 8-bits chunks, chunk_shift=5 and len=17
    * Logical:
    * src: [AAABBBBB][CCCDDEEE][F-------]
    * dst: [-----AAA][BBBBBCCC][DDEEEF--]
    * Physical:
    * src: [BBBBBAAA][EEEDDCCC][-------F]
    * dst: [AAA-----][CCCBBBBB][--FEEEDD]
    */
   else {
      const chunk_t mask = (1 << chunk_shift) - 1;
      unsigned src_bits = 0; /* bits copied from src */

      /* First chunk: avoid reading before the first source chunk */
      dst->vector[0 + chunk_offset] = (src->vector[0] << chunk_shift)
            | (dst->vector[0 + chunk_offset] & mask);
      src_bits += CHUNK_SIZE - chunk_shift;

      /* If already finished */
      if (src_bits >= len)
         return;

      /* Next chunks */
      unsigned i;

      for (i = 1; i < GET_LENGTH_FROM_BITS(len); i++) {
         dst->vector[i + chunk_offset] = (src->vector[i] << chunk_shift)
               | (src->vector[i - 1] >> (CHUNK_SIZE - chunk_shift));
         src_bits += CHUNK_SIZE;
      }

      /* If already finished */
      if (src_bits >= len)
         return;

      /* Last chunk: avoid reading after the last source chunk */
      dst->vector[i + chunk_offset] = (dst->vector[i + chunk_offset] & ~mask)
            | (src->vector[i - 1] >> (CHUNK_SIZE - chunk_shift));
   }
}

/**
 * Returns value of the first \e len bits of a bitvector (up to 64), starting at \e offset
 * \param bv a bitvector
 * \param len number of bits to use for value computation
 * \return decimal value of the bitvector
 * \note This function is so far only used by the disassembler and aims at optimal performances to the detriment of checks
 */
static uint64_t _get_uint64_value(const bitvector_t *bv, size_t offset,
      size_t len)
{
   if (len == 0)
      return 0;

   uint64_t v = 0;

   unsigned start = offset / CHUNK_SIZE;
   unsigned stop = (offset + len) / CHUNK_SIZE;
   unsigned i;
   unsigned shift = offset % CHUNK_SIZE;

   switch (stop - start) {
   case 0:
      v = (bv->vector[start] >> shift) & mask[len];
      break;
   case 1:
      v = (bv->vector[start] >> shift)
            | ((uint64_t) (bv->vector[stop] & mask[(offset + len) % CHUNK_SIZE])
                  << (CHUNK_SIZE - shift));
      break;
   case 2:
      v = bv->vector[start] >> shift;
      v |= ((uint64_t) bv->vector[start + 1]) << (CHUNK_SIZE - shift);
      v |= ((uint64_t) (bv->vector[stop] & mask[(offset + len) % CHUNK_SIZE])
            << (2 * CHUNK_SIZE - shift));
      break;
   default:
      //If CHUNK_SIZE=32, this case should never happen
      v = bv->vector[start] >> shift;
      for (i = start + 1; i < stop; i++) {
         v |= (uint64_t) (bv->vector[i]) << (CHUNK_SIZE * (i - start) - shift);
      }
      v |= ((uint64_t) (bv->vector[stop] & mask[(offset + len) % CHUNK_SIZE])
            << (CHUNK_SIZE * (stop - start) - shift));
   }

   return v;
}

/*
 * Returns value of the first \e len bits of a bitvector, up to 64
 * \param bv a bitvector
 * \param len number of bits to use for value computation
 * \return bitvector value
 */
uint64_t bitvector_value(const bitvector_t *bv, size_t len)
{
   if (len > (8 * sizeof(uint64_t)))
      len = 8 * sizeof(uint64_t);
   if (len > bv->bits)
      len = bv->bits;
   return _get_uint64_value(bv, 0, len);
}

/*
 * Returns value of \e len bits of a bitvector \e bv,
 * starting at position \e offset from the left of \e bv
 * \param bv a bitvector
 * \param len number of bits to use for value computation
 * \param offset offset in \e bv
 * \return represented value
 */
uint64_t bitvector_leftvalue(const bitvector_t *bv, size_t len, size_t offset)
{
   //We have to make these tests here instead of factorising them in _get_uint64_value,
   //because offset does not have the same meaning in both functions
   if (offset >= bv->bits)
      return 0;
   if (len > (8 * sizeof(uint64_t)))
      len = 8 * sizeof(uint64_t);
   if (len > bv->bits - offset)
      len = bv->bits - offset;
   return _get_uint64_value(bv, bv->bits - offset - len, len);
}

/*
 * Returns the value of a bitvector, cast into a byte stream (NOT null-terminated)
 * and the size of the resulting stream in len
 * \param bv a bitvector
 * \param len will contain the size of the result stream
 * \param endianness code's endianness of the instruction's architecture
 * \return bitvector value, cast into a byte stream (NOT null-terminated)
 */
unsigned char * bitvector_charvalue(const bitvector_t *bv, int *len,
      code_endianness_t endianness)
{
   if (!bv || bv->bits == 0) {
      if (len)
         *len = 0;
      return NULL;
   }

   const int bytesize = (bv->bits + 7) >> 3;
   unsigned char *str = lc_malloc0(bytesize);

   bitvector_printbytes(bv, str, endianness);
   if (len)
      *len = bytesize;

   return str;
}

/*
 * Prints the value of a bitvector inside a byte string (NOT null-terminated)
 * The length of the string must be at least (bf->bits + 7) >> 3
 * \param bv A bitvector
 * \param str An allocated string. It must be at least as large as BITVECTOR_GET_BYTELENGTH(bv)
 * \param endianness code's endianness of the instruction's architecture
 * \return The actual number of bytes printed
 * */
unsigned bitvector_printbytes(const bitvector_t* bv, unsigned char* str,
      code_endianness_t endianness)
{
   if (!bv || bv->bits == 0 || !str)
      return 0;
   unsigned B = 0; /* position in the string, index of the current byte */

   unsigned int bytesize = (bv->bits + 7) >> 3;
   memset(str, 0, bytesize);
   int i;
   for (i = bv->bits - 1; i >= 0; i--) {
      int pos = i % 8; /* position in the byte */
      str[B] |= BITVECTOR_GET_BIT (bv, i) << pos;

      if (pos == 0)
         B++;
   }

   if ((endianness == CODE_ENDIAN_LITTLE_16B) && (bytesize % 2 == 0)) {
      for (i = 0; (i + 2) <= (int) B; i = i + 2) {
         unsigned char tmp = str[i];
         str[i] = str[i + 1];
         str[i + 1] = tmp;
      }
   } else if ((endianness == CODE_ENDIAN_LITTLE_32B) && (bytesize % 4 == 0)) {
      for (i = 0; (i + 4) <= (int) B; i = i + 4) {
         unsigned char tmp = str[i];
         str[i] = str[i + 3];
         str[i + 3] = tmp;
         tmp = str[i + 1];
         str[i + 1] = str[i + 2];
         str[i + 2] = tmp;
      }
   }

   return bytesize;
}

/*
 * Returns the value represented by the 64 first (rightmost) bits of a bitvector,
 * depending on the required endianness.
 * \param bv a bitvector
 * \param endianness \e bv endianness (bitvector_endianness_t)
 * \return represented value
 */
int64_t bitvector_fullvalue(const bitvector_t *bv,
      bitvector_endianness_t endianness)
{
   int k;
   unsigned i, j;
   unsigned len = (bv->bits > 64) ? 64 : bv->bits;
   int64_t v = 0;

   switch (endianness) {

   case LITTLE_ENDIAN_BIT:
      for (k = len - 1; k >= 0; k--) {
         if (BITVECTOR_GET_BIT(bv, k))
            v |= ((int64_t) 1 << (len - 1 - k));
      }
      break;

   case LITTLE_ENDIAN_BYTE:
      /**\todo Maybe changing the direction of the for loops from ++ to -- to reduce the number of operations*/
      for (i = 0; i < GET_LENGTH_FROM_BITS(len) - 1; i++) {
         v = v << CHUNK_SIZE;
         for (j = 0; j < sizeof(chunk_t); j++) {
            v |= ((((uint64_t) bv->vector[i] >> (sizeof(chunk_t) - 1 - j) * 8)
                  & mask[8]) << (j * 8));
         }
      }
      //Last element
      if (len % CHUNK_SIZE) {
         int firstbyte = (((len % CHUNK_SIZE) - 1) / 8 + 1);
         v = v << (firstbyte * 8);
         for (j = (sizeof(chunk_t) - firstbyte); j < sizeof(chunk_t); j++) {
            v |= ((((uint64_t) bv->vector[i] >> (sizeof(chunk_t) - 1 - j) * 8)
                  & mask[8]) << ((j - (sizeof(chunk_t) - firstbyte)) * 8));
         }
      } else {    //Last vector element is full
         v = v << CHUNK_SIZE;
         for (j = 0; j < sizeof(chunk_t); j++) {
            v |= ((((uint64_t) bv->vector[i] >> (sizeof(chunk_t) - 1 - j) * 8)
                  & mask[8]) << (j * 8));
         }
      }
      break;

   case BIG_ENDIAN_BIT:
   case BIG_ENDIAN_BYTE:
   default:
      for (i = 0; i < GET_LENGTH_FROM_BITS(len) - 1; i++) {
         v |= (uint64_t) bv->vector[i] << (i * CHUNK_SIZE);
      }
      //i is now the last element in the array, we have to remove the bits outside of the vector
      if (len % CHUNK_SIZE)
         v |= ((uint64_t) bv->vector[i] & mask[len % CHUNK_SIZE])
               << (i * CHUNK_SIZE);
      else
         //len is a multiple of chunks: we take the last chunk whole
         v |= (uint64_t) bv->vector[i] << (i * CHUNK_SIZE);
      break;

   }

   return v;
}

/*
 * Fills a bitvector from \e l characters in the string \e str
 * \warning If \e l is bigger than the number of bytes in \e str, the behavior
 * of the function is undefined
 * \param bv The bitvector to fill. It must be at least of size \e l * 8
 * \param str string to read from
 * \param l number of bytes to read from \e str
 * */
void bitvector_fill_from_str(bitvector_t* bv, const unsigned char *c, int l)
{
   int curseur_c = l - 1;        // position in the input string
   int curseur_b = 0;            // position of the current vector element
   int curseur_sh = 0; // count the number of shift in the current vector element (from 0 to 4)

   if (bv == NULL)
      return;

   while (curseur_c >= 0) {
      // Characters are read from the end to the beginning in order to avoid to make complicated shifts.
      // As an element is on CHUNK_SIZE bits and a character is on 8b, each element contains 4 characters.
      // For example, if string "abcd" is read, the vector element will contain "000d", then "00cd", "0bcd"
      // and at last, "abcd".
      // When an element is full (i.e. when 4 characters are read), a new element of the bitvector is used
      // to save bits.
      bv->vector[curseur_b] = bv->vector[curseur_b]
            + ((c[curseur_c] & 0xFF) << (8 * curseur_sh));/*\bug valgrind detects a read error here => Fixed, was caused when invoking this function at the end of a stream*/
      curseur_sh = (curseur_sh + 1) % sizeof(*bv->vector);
      if (curseur_sh == 0)
         curseur_b++;
      curseur_c--;
   }
}

/*
 * Creates a bitvector from \e l characters in the string \e str
 * \warning If \e l is bigger than the number of bytes in \e str, the behavior
 * of the function is undefined
 * \param str string to read from
 * \param l number of bytes to read from \e str
 * \return A bitvector holding the binary value of the \e l first bytes of \e str
 */
bitvector_t *bitvector_new_from_str(const unsigned char *c, int l)
{
   bitvector_t* output = bitvector_new(l * 8);
   bitvector_fill_from_str(output, c, l);
   return (output);
}

/*
 * Creates a bitvector from a binary stream
 * \param start First byte of the bitvector
 * \param start_off Offset in the first byte where to start
 * \param stop Last byte (not included, unless offset is positive) of the bitvector
 * \param stop_off Offset in the last byte where to stop (number of bits to add)
 * \return A new bitvector containing the bits between start+start_off and stop+stop_off
 * */
bitvector_t* bitvector_new_from_stream(unsigned char* start, uint8_t start_off,
      unsigned char* stop, uint8_t stop_off)
{
   /** \todo Improve this to avoid using cutright and cutleft*/
   bitvector_t* out = NULL;
   int l;
   if (stop >= start) {      //Last byte should be after first byte
      if ((start_off == 0) && (stop_off == 0)) {
         //Easy case (should always happen in the FSM actually): no offsets
         l = stop - start;
         if (l > 0)
            out = bitvector_new_from_str(start, l);
      } else {
         //Hard case: there are offsets. The current version will simply read the bitvector encompassing it, then cut the extremities
         l = stop + 1 - start;
         if (l > 0) {
            out = bitvector_new_from_str(start, l);
            //Cutting the extremities
            if (start_off > 0)
               bitvector_free(bitvector_cutleft(out, start_off));
            //if ( stop_off >= 0 )
            bitvector_free(bitvector_cutright(out, 8 - stop_off));
         }
      }
   }
   return out;
}

/**
 * Shifts a bitvector such as it becomes shorter, starting at \e offset and for \e shift_bits bits
 * In other words, remove (cut, extract...) bv [offset ; offset+shift_bits-1]
 * If 8-bits chunks, offset=10 and shift_bits=3
 * Logical:
 * Before: [AAAAAAAA][AAXXXBBB][CC------]
 * After : [AAAAAAAA][AABBBCC-]
 * \param bv a bitvector
 * \param offset index of the first modified bit
 * \param shift_bits number of bits to shift by
 */
static void _shift_shorter(bitvector_t *bv, size_t offset, size_t shift_bits)
{
   assert(offset + shift_bits <= bv->bits);

   /* The simplest algorithm is: */
   /* for (i=offset; i<bv->bits-shift_bits; i++) */
   /*   if (BITVECTOR_GET_BIT (bv, i+shift_bits) == 1) */
   /*     BITVECTOR_SET_BIT (bv, i); */
   /*   else */
   /*     BITVECTOR_CLR_BIT (bv, i); */

   /* But, for performance reason, copy will be chunk-wide */

   /* Easy case */
   /* No need to shift chunks */
   if (offset % CHUNK_SIZE == 0 && shift_bits % CHUNK_SIZE == 0) {

      const unsigned src_chunk_offset = (offset + shift_bits) / CHUNK_SIZE;
      const unsigned dst_chunk_offset = offset / CHUNK_SIZE;

      /* Move chunks */
      _vector_cpy(bv->vector + src_chunk_offset, bv->vector + dst_chunk_offset,
      GET_CHUNKLENGTH (bv) - src_chunk_offset);
   }

   /* Hard case */
   /*
    * Logically:
    * Before: [AAAAAAAA][AAXXXBBB][CC------]
    * After : [AAAAAAAA][AABBBCC-]
    * Physically:
    * Before: [AAAAAAAA][BBBXXXAA][------CC]
    * After : [AAAAAAAA][-CCBBBAA]
    */
   else {
      /* Creates a temporary bitvector to save/restore moved bits */
      bitvector_t *tmp = _get_tmp(bv->bits - shift_bits - offset);

      /* Copy from the original location to tmp */
      bitvector_read(bv, tmp, offset + shift_bits, tmp->bits);

      /* Copy from tmp to the final location */
      bitvector_write(tmp, bv, offset, tmp->bits);

      _free_tmp(tmp);
   }

   bv->bits -= shift_bits;
   /* bitvector_trim could be automatically called here,
    * which will reduce memory consumption at the expense of performance */
}

/*
 * Extracts the part of a bitvector \e src into another bitvector \e dst,
 * starting at the bit of index \e offset. The number of extracted bits equals dst->bits
 * \param src source bitvector, whose length will decrease
 * \param dst destination bitvector
 * \param offset offset in \e src
 */
void bitvector_extract(bitvector_t *src, bitvector_t *dst, size_t offset)
{
   /* If nothing to write or if no source bitvector */
   if (!src || !dst || dst->bits == 0)
      return;

   /* Out of range */
   if (offset >= src->bits)
      return;

   /* Save requested bits before overwriting them */
   bitvector_read(src, dst, offset, dst->bits);

   /* Need to move bits */
   if (offset + dst->bits < src->bits) {
      _shift_shorter(src, offset, dst->bits);
   }

   /* Just resize */
   else {
      bitvector_resize(src, src->bits - dst->bits);
   }
}

/*
 * Removes the leftmost \e len bits in bitvector \e bv, and returns them as bitvector
 * \param bv a bitvector, that will be shorter at the end of the operation
 * \param len number of bits to remove
 * \return new bitvector containing leftmost \e len bits of \e bv
 */
bitvector_t * bitvector_cutleft(bitvector_t *bv, int len)
{
   /* If nothing to cut */
   if (!bv || bv->bits == 0 || len == 0)
      return NULL;

   bitvector_t *new = bitvector_new(len);

   bitvector_extract(bv, new, bv->bits - len);

   return new;
}

/*
 * Removes the rightmost \e len bits in bitvector \e bv, and returns them as bitvector
 * \param bv a bitvector, that will be shorter at the end of the operation
 * \param len number of bits to remove
 * \return new bitvector containing rightmost \e len bits of \e bv
 */
bitvector_t * bitvector_cutright(bitvector_t *bv, int len)
{
   /* If nothing to cut */
   if (!bv || bv->bits == 0 || len == 0)
      return NULL;

   bitvector_t *new = bitvector_new(len);

   bitvector_extract(bv, new, 0);

   return new;
}

/*
 * Fills the \e len first (rightmost) bits of a bitvector \e bv from a value \e val
 * with a given endianness \e endianness
 * \param bv a bitvector
 * \param val int64_t value to write in \e bv with a given endianness
 * \param endianness \e bv endianness (bitvector_endianness_t)
 * \param len number of bits to fill
 */
void bitvector_fill_from_value(bitvector_t *bv, int64_t val,
      bitvector_endianness_t endianness, size_t len)
{
   unsigned i;
   switch (endianness) {

   case LITTLE_ENDIAN_BIT:
      for (i = 0; i < len; i++) {
         if (((val >> i) & 1) == 1)
            BITVECTOR_SET_LBIT(bv, i);
         else
            BITVECTOR_CLR_LBIT(bv, i);
      }
      break;

   case LITTLE_ENDIAN_BYTE:
      for (i = 0; i < len; i++) {
         if (((val >> ((((i >> 3) + 1) << 3) - i % 8 - 1)) & 1) == 1)
            BITVECTOR_SET_LBIT(bv, i);
         else
            BITVECTOR_CLR_LBIT(bv, i);
      }
      break;

   case BIG_ENDIAN_BIT:
   case BIG_ENDIAN_BYTE:
   default:
      for (i = 0; i < len; i++) {
         if (((val >> (len - i - 1)) & 1) == 1)
            BITVECTOR_SET_LBIT(bv, i);
         else
            BITVECTOR_CLR_LBIT(bv, i);
      }

   }
}

/*
 * Creates a \e len bits bitvector from a value \e val considering a given endianness \e endianness.
 * In other words, calls bitvector_fill_from_value on a new bitvector and returns it
 * \param val int64_t value to write in the new bitvector with a given endianness
 * \param endianness bitvector endianness (bitvector_endianness_t)
 * \param len new bitvector length
 * \return new bitvector
 */
bitvector_t *bitvector_new_from_value(int64_t val,
      bitvector_endianness_t endianness, size_t len)
{
   bitvector_t *new = bitvector_new(len);

   bitvector_fill_from_value(new, val, endianness, len);

   return new;
}

/*
 * Appends bits at the right of a bitvector, setting them to the specified value,
 * depending on endianness (for the bitvector).
 * \param bv bitvector to update
 * \param val value to append
 * \param len number of bits to append (if val is longer, only its \e len least significant bits will be appended)
 * \param endianness bitvector endianness (bitvector_endianness_t)
 * \warning This function does not allow to append values larger than 64bits (use bitvector_append for that)
 */
void bitvector_appendvalue(bitvector_t* bv, int64_t val, size_t len,
      bitvector_endianness_t endianness)
{
   if (!bv || len == 0)
      return;

   /* Create a temporary bitvector to contain the value */
   /* Cannot use _get_tmp since will be called by append */
   static bitvector_t value;
   chunk_t vector[GET_LENGTH_FROM_BITS(len)];
   value.bits = len;
   value.vector = vector;

   /* Copy the value to the temporary bitvector */
   bitvector_fill_from_value(&value, val, endianness, len);

   /* Append the value at the right of bv */
   bitvector_append(bv, &value);
}

/*
 * Returns true if the shortest bitvector matches with a subset of the largest
 * \param a a bitvector
 * \param b a bitvector
 * \return TRUE if the shortest bitvector matches with a subset of the largest, else FALSE
 */
int bitvector_match(const bitvector_t* a, const bitvector_t* b)
{
   if (!a || !b)
      return a == b;

   unsigned i, j;
   const bitvector_t* bv_short = (a->bits >= b->bits) ? b : a;
   const bitvector_t* bv_large = (a->bits >= b->bits) ? a : b;

   for (i = 0; i <= bv_large->bits - bv_short->bits; i++) {
      int match = TRUE;

      for (j = 0; j < bv_short->bits; j++) {
         if (BITVECTOR_GET_BIT(bv_short,
               j) != BITVECTOR_GET_BIT (bv_large, j+i)) {
            match = FALSE;
            break;
         }
      }

      if (match == TRUE)
         return TRUE;
   }

   return FALSE;
}

/*
 * Compares two bitvectors.
 * \param a a bitvector
 * \param b a bitvector
 * \return TRUE if they have strictly identical values, FALSE otherwise
 */
int bitvector_equal(const bitvector_t *a, const bitvector_t *b)
{
   if (a->bits != b->bits)
      return FALSE;

   unsigned i;
   const unsigned leftover_bits = a->bits % CHUNK_SIZE;

   if (leftover_bits) {
      /* for each full slot (the last one being not full) */
      for (i = 0; i < GET_CHUNKLENGTH(a) - 1; i++)
         if (a->vector[i] != b->vector[i])
            return FALSE;

      /* last slot, not full */
      if ((a->vector[i] & mask[leftover_bits])
            != (b->vector[i] & mask[leftover_bits]))
         return FALSE;
   }

   else
      /* for each slot (all are full) */
      for (i = 0; i < GET_CHUNKLENGTH(a); i++)
         if (a->vector[i] != b->vector[i])
            return FALSE;

   return TRUE;
}

/*
 * Compares bitvector \e value against bitvector \e model. Bitvector \e msk allows to specify which bits must be compared.
 * In other words, returns bitvector_equal (value & msk, model)
 * \warning This function assumes that model->bits == msk->bits and value->bits >= model->bits
 * \param value bitvector to compare
 * \param model bitvector compared
 * \param msk bitvector used as a mask for \e value
 * \return TRUE if the comparison is true, else FALSE
 */
int bitvector_equalmask(const bitvector_t* value, const bitvector_t* model,
      const bitvector_t* msk)
{
   assert(value && model && msk);
   assert(model->bits == msk->bits);
   assert(value->bits >= model->bits);

   unsigned i;
   const unsigned leftover_bits = model->bits % CHUNK_SIZE;

   if (leftover_bits) {
      /* for each full slot (the last one being not full) */
      for (i = 0; i < GET_CHUNKLENGTH(model) - 1; i++)
         if ((value->vector[i] & msk->vector[i]) != model->vector[i])
            return FALSE;

      /* last slot, not full */
      if ((value->vector[i] & msk->vector[i] & mask[leftover_bits])
            != (model->vector[i] & mask[leftover_bits]))
         return FALSE;
   }

   else
      /* for each slot (all are full) */
      for (i = 0; i < GET_CHUNKLENGTH(model); i++)
         if ((value->vector[i] & msk->vector[i]) != model->vector[i])
            return FALSE;

   return TRUE;
}

/*
 * Compares bitvector \e value against bitvector \e model, from the left. Bitvector \e msk allows to specify which bits must be compared.
 * In other words, returns bitvector_equal (value & msk, model) considering leftmost bits
 * \warning This function assumes that model->bits == msk->bits and value->bits >= model->bits
 * \param value bitvector to compare
 * \param model bitvector compared
 * \param msk bitvector used as a mask for \e value
 * \return TRUE if the comparison is true, else FALSE
 */
int bitvector_equalmaskleft(const bitvector_t* value, const bitvector_t* model,
      const bitvector_t* msk)
{
   /* Copy the left part of value in a temporary bitvector */
   bitvector_t *tmp = _get_tmp(model->bits);
   bitvector_read(value, tmp, value->bits - model->bits, model->bits);

   /* Compare bits */
   int ret = bitvector_equalmask(tmp, model, msk);

   _free_tmp(tmp);

   return ret;
}

/*
 * Creates a bitvector from its string representation ("1100" for 0xC for instance)
 * \note This function is mainly used for testing purposes
 * \param str a string which contains '0' and '1'
 * \return a new bitvector
 */
bitvector_t *bitvector_new_from_binstr(const char *str)
{
   if (!str)
      return bitvector_new(0);

   bitvector_t *new = bitvector_new(strlen(str));
   unsigned i;

   for (i = 0; i < strlen(str); i++) {
      if (str[i] == '1')
         BITVECTOR_SET_BIT(new, strlen(str) - i - 1);
      /* bitvector_new is supposed clearing bits after allocation => no else */
   }

   return new;
}

/*
 * Trims a bitvector, that is realloc the vector to fit the number of bits
 * \note Inside this function, it is impossible to know if this function will gain space
 * since the number of allocated chunks is not stored.
 * \param bv bitvector to trim
 */
void bitvector_trim(bitvector_t *bv)
{
   if (!bv)
      return;

   bv->vector = lc_realloc(bv->vector,
         GET_CHUNKLENGTH (bv) * sizeof *(bv->vector));
}

/**
 * Shifts a bitvector such as it becomes larger, starting at \e offset and for \e shift_bits bits
 * In other words, add (insert...) \e shift_bits '0' bits from position \e offset
 * If 8-bits chunks, offset=10 and shift_bits=5
 * Logical:
 * Before: [AAAAAAAA][AABBB---]
 * After : [AAAAAAAA][AA00000B][BB------]
 * \param bv a bitvector
 * \param offset index of the first bit to shift
 * \param shift_bits number of bits to shift by
 */
static void _shift_larger(bitvector_t *bv, size_t offset, size_t shift_bits)
{
   assert(offset < bv->bits);

   /* Increase bv size if necessary */
   if (GET_CHUNKLENGTH (bv) < GET_LENGTH_FROM_BITS(bv->bits + shift_bits))
      bv->vector = lc_realloc(bv->vector,
            GET_LENGTH_FROM_BITS(bv->bits + shift_bits)
                  * sizeof *(bv->vector));

   /* The simplest algorithm is: */
   /* for (i=bv->bits-1; i>=offset; i--) */
   /*   if (BITVECTOR_GET_BIT (bv, i) == 1) */
   /*     BITVECTOR_SET_BIT (bv, i+shift_bits); */
   /*   else */
   /*     BITVECTOR_CLR_BIT (bv, i+shift_bits); */
   /* for (i=0; i<shift_bits; i++) */
   /*   BITVECTOR_CLR_BIT (bv, i+offset); */

   /* But, for performance reason, copy will be chunk-wide */

   int i;

   /* Easy case */
   /* No need to shift chunks */
   if (offset % CHUNK_SIZE == 0 && shift_bits % CHUNK_SIZE == 0) {

      /* Distance in chunks between source and destination chunks */
      const int chunk_offset = shift_bits / CHUNK_SIZE;

      /* Move chunks */
      for (i = GET_CHUNKLENGTH (bv) - 1; i >= (int) (offset / CHUNK_SIZE);
            i--) {
         bv->vector[i + chunk_offset] = bv->vector[i];
      }

      /* Clear the hole */
      for (i = 0; i < chunk_offset; i++) {
         bv->vector[i + (offset / CHUNK_SIZE)] = 0;
      }

      bv->bits += shift_bits;
   }

   /* Hard case */
   /*
    * Logically:
    * Before: [AAAAAAAA][AABBB---]
    * After : [AAAAAAAA][AA00000B][BB------]
    * Physically:
    * Before: [AAAAAAAA][---BBBAA]
    * After : [AAAAAAAA][B000000A][------BB]
    */
   else {
      /* Create a temporary bitvector to restore/save moved bits */
      bitvector_t *tmp = _get_tmp(bv->bits - offset);

      /* Copy from original location to tmp */
      bitvector_read(bv, tmp, offset, tmp->bits);

      /* Do it now to ensure a correct behaviour in next routines */
      bv->bits += shift_bits;

      /* Copy from tmp to final location */
      bitvector_write(tmp, bv, offset + shift_bits, tmp->bits);

      _free_tmp(tmp);

      /* Clear the hole */
      for (i = 0; i < (int) shift_bits; i++)
         BITVECTOR_CLR_BIT(bv, i + offset);
   }
}

/*
 * Inserts the content of a bitvector \e src into another bitvector \e dst,
 * starting at the bit of index \e offset.
 * \param src source bitvector
 * \param dst destination bitvector, whose length will increase
 * \param offset in \e dst
 */
void bitvector_insert(const bitvector_t *src, bitvector_t *dst, size_t offset)
{
   /* If nothing to read or if no destination bitvector */
   if (!src || src->bits == 0 || !dst)
      return;

   /* Need to move bits */
   if (offset < dst->bits) {
      _shift_larger(dst, offset, src->bits);
   }

   /* Just resize */
   else if (offset == dst->bits) {
      bitvector_resize(dst, dst->bits + src->bits);
   }

   /* Out of range */
   else
      return;

   bitvector_write(src, dst, offset, src->bits);
}

/*
 * Appends a bitvector \e right to another bitvector \e left
 * In other words, left <= left .. right
 * \param left left bitvector, that will be larger at the end of the operation
 * \param right right bitvector
 */
void bitvector_append(bitvector_t* left, const bitvector_t* right)
{
   /* If no destionation bitvector or nothing to append from the source bitvector */
   if (!left || !right || right->bits == 0)
      return;

   if (left->bits == 0) {
      left->bits = right->bits;
      left->vector = lc_realloc(left->vector,
            GET_CHUNKLENGTH(right) * sizeof *(left->vector));

      _vector_cpy(right->vector, left->vector, GET_CHUNKLENGTH(right));
   } else {
      bitvector_insert(right, left, 0);
   }
}

/*
 * Prepends a bitvector \e left to another bitvector \e right
 * In other words, right <= left .. right
 * \param left left bitvector
 * \param right right bitvector, that will be larger at the end of the operation
 */
void bitvector_prepend(const bitvector_t* left, bitvector_t* right)
{
   /* If no destionation bitvector or nothing to append from the source bitvector */
   if (!right || !left || left->bits == 0)
      return;

   if (right->bits == 0) {
      right->bits = left->bits;
      right->vector = lc_realloc(right->vector,
            GET_CHUNKLENGTH(left) * sizeof *(right->vector));

      _vector_cpy(left->vector, right->vector, GET_CHUNKLENGTH(left));
   } else {
      bitvector_insert(left, right, right->bits);
   }
}

/*
 * Removes \e len bits from bitvector \e bv, starting at position \e offset from the left.
 * \param bv a bitvector
 * \param offset index of the removed bit, from the left
 * \param len number of bits to remove
 * \return Nonzero if an error occurred (bits to remove out of range or too large)
 */
int bitvector_removebitsleft(bitvector_t* bv, int offset, int len)
{
   if ((unsigned) (offset + len) > bv->bits)
      return 1;

   _shift_shorter(bv, bv->bits - (offset + len), len);

   return 0;
}

/*
 * Prints a bitvector_t declaration from a bit field written as a string (containing '0' and '1' characters)
 * \param bf The string representing the bitvector value
 * \param name The name of the bitvector (the vector will have the same name but with the "vect" prefix)
 * \param str String to write the declaration to (must already be initialised)
 * \param size Size of the destination string
 */
void bitvector_printdeclaration_from_binstring(char* bf, char* name, char* str,
      size_t size)
{
   /**\todo (2016-09-15) Remove the use of PRINT_INSTRING here, the function as written won't work on Windows*/
   chunk_t value = 0;
   size_t i;
   size_t len = strlen(bf);

   /*Printing the vector*/
   PRINT_INSTRING(str, size, "bitvector_chunk_t vect%s[] = ", name);

   //vector_printer(bf,str);
   /*Printing the vector*/
   PRINT_INSTRING(str, size, "{");

   /*Printing the vector values*/
   if (len != 0) {
      i = len;
      do {
         i--;
         if ((i < (len - 1)) && (((len - 1 - i) % CHUNK_SIZE) == 0)) {
            PRINT_INSTRING(str, size, "%u", value);
            value = 0;
            if (i < (len - 1))
               PRINT_INSTRING(str, size, ",");
         }
         if (bf[i] == '1')
            value |= 1 << ((len - 1 - i) % CHUNK_SIZE);
      } while (i != 0);
      PRINT_INSTRING(str, size, "%u", value);
   }
   PRINT_INSTRING(str, size, "}");

   /*Prints the vector value as comment (only for cosmetic and debug reasons)*/
   PRINT_INSTRING(str, size, ";\t/*%s*/\n", bf);

   /*Printing size and length of the bitvector*/
   PRINT_INSTRING(str, size, "bitvector_t %s = { %lu,vect%s};", name,
         strlen(bf), name);
}

#undef CHUNK_SIZE
