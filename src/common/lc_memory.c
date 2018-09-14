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

long mem_cum_allocs = 0;			// Size of allocated memory
long mem_cum_frees = 0;				// Size of freed memory
int mem_allocations = 0;			// Number of allocations
int mem_frees = 0;					// Number of frees

///////////////////////////////////////////////////////////////////////////////
//                            memory functions                               //
///////////////////////////////////////////////////////////////////////////////
#ifndef MEMORY_LEAK_DEBUG

#ifndef MEMORY_TRACE_USE

#ifndef MEMORY_TRACE_SIZE
// Mode no memory trace

void* lc_malloc(unsigned long size)
{
   void* ptr;

   ptr = malloc(size);
   if (ptr == NULL) {
      HLTMSG("[MAQAO] Impossible to allocate memory!\n");
   }

   return ptr;
}

void* lc_malloc0(unsigned long size)
{
   void* ptr;

   ptr = lc_malloc(size);
   memset(ptr, 0, size);

   return ptr;
}

void* lc_calloc(unsigned long nmemb, unsigned long size)
{
   return lc_malloc0(nmemb * size);
}

char* lc_strdup(const char *str)
{
   if (!str) return NULL;

   char *ptr = lc_malloc (strlen (str) + 1); // +1 for final '\0'
   memcpy (ptr, str, strlen (str) + 1); // faster than strcpy since length is already known

   return ptr;
}

void* lc_realloc(void* src, unsigned long size)
{
   void* tmp;

   tmp = realloc(src, size);
   if (tmp == NULL && size != 0) {
      HLTMSG("[MAQAO] Impossible to reallocate memory!\n");
   } else {
      src = tmp;
   }

   return src;
}

#else //Mode MEMORY_TRACE_SIZE

void mem_usage ()
{
   fprintf(stderr,"allocated:\t%16lu bytes\t%10d #allocations\nfreed\t\t%16lu bytes\t%10d #frees\ndiff:\t\t%16lu bytes\t%10d\n\n",
         mem_cum_allocs,mem_allocations,mem_cum_frees,mem_frees,mem_cum_allocs-mem_cum_frees,mem_allocations-mem_frees);
}

void lc_free(void *ptr)
{
   if (!ptr) return;

   mem_frees++;
   mem_cum_frees+=*(unsigned long *)(ptr-sizeof(unsigned long));
   free(ptr-sizeof(unsigned long));
}

void *lc_malloc(unsigned long size)
{
   void *ptr;
   ptr = malloc(size+sizeof(unsigned long));
   if (ptr == NULL)
   {
      HLTMSG("[MAQAO] : Impossible to allocate memory!\n");
   }
   //assert(ptr!=NULL);

   *(unsigned long *)ptr=size;
   mem_cum_allocs += size;
   mem_allocations++;
   return (ptr + sizeof(unsigned long));
}

void *lc_malloc0(unsigned long size)
{
   void *ptr;
   ptr = lc_malloc(size);
   memset(ptr, 0, size);
   return ptr;
}

void* lc_calloc(unsigned long nmemb, unsigned long size)
{
   return lc_malloc0(nmemb * size);
}

char *lc_strdup(const char *str) {
   char *ptr;
   ptr = lc_malloc(strlen(str) + 1);
   strcpy(ptr, str);
   return ptr;
}

void* lc_realloc (void* src, unsigned long size)
{
   unsigned long newsize;
   void* ptrbase;
   if(src != NULL)
   ptrbase = src - sizeof(unsigned long);
   else
   ptrbase = src;
   if(size != 0)
   newsize = size + sizeof(unsigned long);
   else
   newsize = size;
   src = realloc (ptrbase, newsize);
   assert (src != NULL);
   mem_cum_allocs += size;
   mem_allocations++;
   return (src + sizeof(unsigned long));
}

#endif
#else //Mode MEMORY_TRACE_USE
unsigned long __memptr_id = 0;

void __lc_free_memtrace(void *ptr, const char* src, int l)
{
   if (!ptr) return;

   fprintf(stderr,"%lu;free;%lu;%s:%d\n",*(unsigned long *)(ptr-2*sizeof(unsigned long)),*(unsigned long *)(ptr-sizeof(unsigned long)),src,l);
   free(ptr-2*sizeof(unsigned long));
}

void *__lc_malloc_memtrace(unsigned long size, const char* src, int l)
{
   void *ptr;
   ptr = malloc(size+2*sizeof(unsigned long));
   assert(ptr!=NULL);

   *(unsigned long *)ptr=__memptr_id++;
   *(unsigned long *)(ptr + sizeof(unsigned long))=size;
   fprintf(stderr,"%lu;malloc;%lu;%s:%d\n",*(unsigned long *)ptr,size,src,l);
   return (ptr + 2*sizeof(unsigned long));
}

void *__lc_malloc0_memtrace(unsigned long size, const char* src, int l)
{
   void *ptr;
   ptr = malloc(size+2*sizeof(unsigned long));
   assert(ptr!=NULL);

   *(unsigned long *)ptr=__memptr_id++;
   *(unsigned long *)(ptr + sizeof(unsigned long))=size;
   memset(ptr + 2*sizeof(unsigned long), 0, size);
   fprintf(stderr,"%lu;malloc0;%lu;%s:%d\n",*(unsigned long *)ptr,size,src,l);
   return (ptr + 2*sizeof(unsigned long));
}

char *__lc_strdup_memtrace(const char *str, const char* src, int l) {
   char *ptr;

   ptr = malloc(strlen(str) + 1 + 2*sizeof(unsigned long));
   assert(ptr!=NULL);

   *(unsigned long *)ptr=__memptr_id++;
   *(unsigned long *)(ptr + sizeof(unsigned long))=strlen(str) + 1;
   fprintf(stderr,"%lu;strdup;%lu;%s:%d\n",*(unsigned long *)ptr,strlen(str) + 1,src,l);
   strcpy((ptr + 2*sizeof(unsigned long)), str);
   return (ptr + 2*sizeof(unsigned long));
}

void* __lc_realloc_memtrace (void* ptr, unsigned long size, const char* src, int l)
{
   unsigned long newsize, oldsize;
   unsigned long id;
   void* ptrbase;
   if(ptr != NULL)
   {
      ptrbase = ptr - 2*sizeof(unsigned long);
      id = *(unsigned long *)ptrbase;
      oldsize = *(unsigned long *)(ptrbase + sizeof(unsigned long));
   }
   else
   {
      ptrbase = ptr;
      id = __memptr_id++;
      oldsize = 0;
   }
   if(size != 0)
   newsize = size + 2*sizeof(unsigned long);
   else
   newsize = size;
   ptr = realloc (ptrbase, newsize);
   assert (ptr != NULL);
   *(unsigned long *)ptr=id;
   *(unsigned long *)(ptr + sizeof(unsigned long))=size;
   fprintf(stderr,"%lu;realloc;%ld;%s:%d\n",*(unsigned long *)ptr,size-oldsize,src,l);

   return (ptr + 2*sizeof(unsigned long));
}

#endif

#else //Mode MEMORY_LEAK_DEBUG

// Memory leaks alloc debugging functions
typedef struct _list_alloc_s {
   void* address; /*< */
   char * id; /*< */
   unsigned long size; /*< */
   struct _list_alloc_s *next; /*< */
   struct _list_alloc_s *prev; /*< */
}list_alloc_t;

static unsigned long listalloclen=0;
static list_alloc_t* listalloc=NULL;

void mem_usage() {
   //dump listalloc
   list_alloc_t *n=listalloc,*l=listalloc;
   unsigned long i=0;
   while(i < listalloclen) {
      l=n;
      printf("mem_usage %p %p %s %d [%s]\n",l,l->address,l->id,l->size,l->address+sizeof(unsigned long));
      n=l->next;
      free(l->id);
      free(l);
      i++;
      listalloclen--;
   }
   listalloc=NULL;
   listalloclen=0;
   mem_cum_allocs=mem_allocations=mem_cum_frees=mem_frees=0;

   fprintf(stderr,"allocated:\t%16lu bytes\t%10d #allocations\nfreed\t\t%16lu bytes\t%10d #frees\ndiff:\t\t%16lu bytes\t%10d\n\n",
         mem_cum_allocs,mem_allocations,mem_cum_frees,mem_frees,mem_cum_allocs-mem_cum_frees,listalloclen);
}

void lc_free(void *ptr) {
   if (!ptr) return;

   mem_frees++;

   list_alloc_t* cell = *(list_alloc_t**)(ptr-sizeof(unsigned long));
   mem_cum_frees+=cell->size;
   list_alloc_t *n;
   assert(cell!=NULL);
   if(cell) {
      listalloclen--;
      if(cell->prev)
      {
         if(cell->prev->next!=cell) {fprintf(stderr,"pointer already freed"); abort();}
         cell->prev->next=cell->next;
      }
      if(cell->next)
      {
         if(cell->next->prev!=cell) {fprintf(stderr,"pointer already freed"); abort();}
         cell->next->prev=cell->prev;
      }

      if(cell==listalloc)
      listalloc=cell->next;

      free(cell->id);
      free(cell);
   }

   free(ptr-sizeof(unsigned long));
}

void *lc_malloc(unsigned long size, char* id) {
   void *ptr;
   void *ptr2;

   assert(id!=NULL);

   ptr = malloc(size+sizeof(unsigned long));
   assert(ptr!=NULL);

   //add head to the alloc list
   list_alloc_t* l = (list_alloc_t*)malloc(sizeof(list_alloc_t));
   assert(l!=NULL);
   l->address = ptr;
   l->size=size;
   l->id=NULL;
   if(id)
   l->id = strdup(id);

   // add the cell
   l->next=listalloc;
   if(listalloc)
   listalloc->prev=l;

   l->prev=NULL;
   listalloc=l;
   listalloclen++;

   assert(l->prev!=l);
   assert(l->next!=l);

   *(unsigned long *)ptr=l;

   mem_cum_allocs += size;
   mem_allocations++;
   return ptr+sizeof(unsigned long);
}

void *lc_malloc0(unsigned long size,char* id)
{
   void *ptr;
   ptr = lc_malloc(size,id);
   memset(ptr, 0, size);
   return ptr;
}

char *lc_strdup(const char *str,char* id) {
   char *ptr;
   ptr = lc_malloc(strlen(str) + 1,id);
   strcpy(ptr, str);
   return ptr;
}

void * __builtin_return_address (unsigned int level)
void * __builtin_frame_address (unsigned int level)

Dl_info d;
dladdr(__builtin_return_address(0), &d);
printf(" (dli_sname = %s)\n", d.dli_sname);
STANDALONE

/* Obtain a backtrace and print it to stdout. */
void
print_trace (void)
{
   void *array[10];
   size_t size;
   char **strings;
   size_t i;

   size = backtrace (array, 10);
   strings = backtrace_symbols (array, size);

   printf ("Obtained %zd stack frames.\n", size);

   for (i = 0; i < size; i++)
   printf ("%s\n", strings[i]);

   free (strings);
}

/* A dummy function to make the backtrace more interesting. */
void
dummy_function (void)
{
   print_trace ();
}

int
main (void)
{
   dummy_function ();
   return 0;
}

void myfunction()
{
#if DEBUG == 1
   {
      Dl_info dli;
      printf("dladdr return : %d\n",dladdr(__builtin_return_address(0), &dli));
      fprintf(stderr, "debug trace [%d]: %s "
            "called by %p [ %s(%p) %s(%p) ].\n",
            getpid(), __func__,
            __builtin_return_address(0),
            strrchr(dli.dli_fname, '/') ?
            strrchr(dli.dli_fname, '/')+1 : dli.dli_fname,
            dli.dli_fbase, dli.dli_sname, dli.dli_saddr);
      printf("dladdr return : %d\n",dladdr(__builtin_return_address(1), &dli));
      fprintf(stderr, "debug trace [%d]: %*s "
            "called by %p [ %s(%p) %s(%p) ].\n",
            getpid(), strlen(__func__), "...",
            __builtin_return_address(1),
            strrchr(dli.dli_fname, '/') ?
            strrchr(dli.dli_fname, '/')+1 : dli.dli_fname,
            dli.dli_fbase, dli.dli_sname, dli.dli_saddr);
   }
#endif
}

#endif

