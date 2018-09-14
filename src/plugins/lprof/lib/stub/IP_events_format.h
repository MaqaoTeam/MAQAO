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

#include <stdio.h>
#include "binary_format.h"          // call_chain_t
#include "sampling_engine_shared.h" // IP_events_t

typedef struct {
   unsigned nb_threads;
   unsigned HW_evts_per_grp;
   char **HW_evts_name;
   char *HW_evts_list; // eventsList in legacy code
   uint64_t *sampleTypesList;
} TID_events_header_t;

typedef struct {
   uint64_t ip;
   hits_nb_t *eventsNb; // eventsNb [events_per_group]

   // Callchains
   size_t nb_callchains;
   size_t max_nb_callchains;
   size_t max_callchain_len;
   IP_callchain_t *callchains;
} raw_IP_events_t;

int write_TID_events_header (FILE *fp, const TID_events_header_t *header);
int read_TID_events_header  (FILE *fp, TID_events_header_t *header);
void free_TID_events_header (TID_events_header_t *header);

int write_IP_events_header (FILE *fp, uint64_t tid, unsigned events_nb);
int read_IP_events_header  (FILE *fp, uint64_t *tid, unsigned *events_nb);

int write_IP_events (FILE *fp, uint64_t ip, const IP_events_t *IP_events, unsigned HW_evts_per_grp);
int read_IP_events  (FILE *fp, raw_IP_events_t *raw_IP_events, unsigned HW_evts_per_grp);

raw_IP_events_t *raw_IP_events_new (unsigned HW_evts_per_grp);
void raw_IP_events_free (raw_IP_events_t *IP_events);
