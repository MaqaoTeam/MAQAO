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

#ifndef __LIBMILRT_H_
#define __LIBMILRT_H_

#include "lua.h"
#include "lauxlib.h"
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <omp.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

extern void milrt_load(int);
extern void milrt_exec(char *);
extern void milrt_unload(void);

extern void trace_register_func(char *,int);
extern void traceEntry(int);
extern void traceExit(int);
extern void tau_dyninst_cleanup();

#endif
