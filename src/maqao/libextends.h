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
 \file libextends.h
 \brief This file declares what is needed to expose additional informations (typically architecture-specific) for disassembled instructions.
 \warning This file has been automatically generated and should not be modified
*/

#ifndef LIBEXTENDS_H
#define LIBEXTENDS_H

#include <stdint.h>

typedef struct {
   uint16_t min;
   uint16_t max;
} uint16_min_max_t;

#define UNITS_LEN  6
#define UOPS_GROUPS_LEN 5

typedef struct {
   uint8_t nb_uops;   // number of uops going into ports/units listed in 'units'
   uint8_t nb_units;            // number of elements in 'units'
   uint8_t units[UNITS_LEN];   // array of ports/units
} uops_group_t;

typedef struct {
   float min;
   float max;
} float_min_max_t;

typedef struct {
   uint16_min_max_t nb_uops; // number of front-end/fused uops
   uint8_t nb_uops_groups; // number of back-end/unfused uops
   uops_group_t uops_groups[UOPS_GROUPS_LEN]; // number of back-end/unfused uops
   uint16_min_max_t latency; // cycles per instruction in dependency chain
   float_min_max_t recip_throughput; // cycles per instruction with independent instructions
} intel_ooo_t;
// Definition of the macro to set extensions for an instruction
#define SET_EXT(I,X,U)  insn_set_ext(I, (X) [uarch_get_id(proc_get_uarch(asmfile_get_proc(U)))] ) //(((insn_t*)(I))->ext = (X) [*((int *) (U))])

typedef struct {
   uint8_t F0;
   uint8_t F1;
   uint8_t I0;
   uint8_t I1;
   uint8_t M;
   uint8_t L;
   uint8_t S;
   uint8_t B;
} uop_dispatch_t;

#define MAX_UOPS 2
typedef struct {
   uint8_t nb_uops;
   uop_dispatch_t dispatch[MAX_UOPS];
   float_min_max_t throughput;
   float_min_max_t latency;
   float_min_max_t lf_latency; //Late forwarding latency
} arm64_ooo_t;

#endif /* LIBEXTENDS_H */
