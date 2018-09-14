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
 * \file arm64_uarch_detector.h
 * \brief This file contains architecture-specific functions for retrieving information about the current host CPU
 */

#ifndef ARM64_UARCH_DETECTOR_H_
#define ARM64_UARCH_DETECTOR_H_

/**
 * Architecture-specific function identifying the current host
 * \return Structure describing the processor version of the current host
 * */
extern proc_t* arm64_utils_get_proc_host();

#endif /* ARM64_UARCH_DETECTOR_H_ */
