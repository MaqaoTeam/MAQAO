/*
   Copyright (C) 2004 - 2017 Université de Versailles Saint-Quentin-en-Yvelines (UVSQ)

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
 * \brief This file defines arm64 specific functions related to DDG (Data Dependency Graph)
 * */

#include "libextends.h"
#include "libmcommon.h"
#include "libmcore.h"
#include "arm64_arch.h"

DDG_latency_t arm64_get_DDG_latency (insn_t *src, insn_t *dst)
{
   DDG_latency_t lat;

   arm64_ooo_t *ext = insn_get_ext(src);
   if (ext != NULL) 
   {
      unsigned short src_family = insn_get_family(src);
      unsigned short dst_family = insn_get_family(dst);
      if ((src_family == FM_FMA || src_family == FM_FMS) && (dst_family == FM_FMA || dst_family == FM_FMS))
      {
         lat.min = ext->lf_latency.min;
         lat.max = ext->lf_latency.max;
      } 
      else
      {
         lat.min = ext->latency.min;
         lat.max = ext->latency.max;
      }
   } 
   else 
   {
      lat.min = 0;
      lat.max = 0;
   }

   return lat;
}