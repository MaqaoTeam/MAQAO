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

#ifndef TYPESDEFS_
#define TYPESDEFS_


//Typedefs
typedef unsigned long long int ulong64_t;
typedef long long int long64_t;
typedef unsigned long int ulong32_t;
typedef long int long32_t;
typedef ulong64_t ulong_t;
typedef long64_t long_t;
typedef ulong_t count_t;

typedef unsigned long array_type;

//Defines
#define NB_TUPLES_REPOS 16
#define DEBUG(...) fprintf(stderr,__VA_ARGS__ );

#endif
