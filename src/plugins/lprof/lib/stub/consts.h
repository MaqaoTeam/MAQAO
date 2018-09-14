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

/*
 * Supposed to define C-level lprof constants...
 * In reality, many constants seems defined elsewhere and lprof_verbosity_level is used only by sampler.c
 * TODO: remove this file and define lprof_verbosity_level locally, in sampler.c
 */

#ifndef __LPROF_CONSTS_H__
#define __LPROF_CONSTS_H__


// Choose verbosity level
typedef enum lprof_verbosity_level {
   LPROF_VERBOSITY_OFF = 0,   // Verbosity OFF
   LPROF_VERBOSITY_ON  = 1,   // Verbosity ON
} lprof_verbosity_level;

#endif
