#ifndef __UNWIND_H__
#define __UNWIND_H__

#include "libmcommon.h" // array_t

#define PERF_STACK_USER_SIZE 4096 // maximum user stack size (bytes) (perf_event_attr->sample_stack_user)

typedef struct {
   uint64_t start;
   uint64_t end;
   uint64_t offset;
   char    *name;
   int      fd;
   void    *data;
   size_t   length;
   unw_dyn_info_t *di;
} map_t;

typedef struct {
   uint64_t ip;
   uint64_t bp;
   uint64_t sp;
   char stack [PERF_STACK_USER_SIZE];
   array_t *maps; // map_t *
} unwind_context_t;

unw_accessors_t *get_unw_accessors (void);

#endif // __UNWIND_H__
