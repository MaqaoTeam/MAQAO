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

/* Defines functions to read/write IP events from/to disk, with new inherit- and ptrace-based sampling engines */

#include <stdio.h>
#include <string.h> // strlen
#include "IP_events_format.h" // TID_events_header_t, IP_events_t

#define MAX(x,y) ((x) > (y) ? (x) : (y))

int write_TID_events_header (FILE *fp, const TID_events_header_t *header)
{
   // Number of threads
   if (fwrite (&(header->nb_threads), sizeof header->nb_threads, 1, fp) != 1) return -1;

   // Number of HW events per group
   unsigned HW_evts_per_grp = header->HW_evts_per_grp;
   if (fwrite (&HW_evts_per_grp, sizeof HW_evts_per_grp, 1, fp) != 1)
      return -2;

   unsigned i; size_t len;
   // HW events name
   for (i=0; i<HW_evts_per_grp; i++) {
      len = strlen (header->HW_evts_name[i]);
      if (fwrite (&len, sizeof len, 1, fp) != 1) return -3;
      if (fwrite (header->HW_evts_name[i], 1, len, fp) != len) return -4;
   }

   // HW events list
   len = strlen (header->HW_evts_list);
   if (fwrite (&len, sizeof len, 1, fp) != 1) return -5;
   if (fwrite (header->HW_evts_list, 1, len, fp) != len) return -6;

   // sampleTypesList
   const uint64_t *sampleTypesList = header->sampleTypesList;
   if (fwrite (sampleTypesList, sizeof sampleTypesList[0], HW_evts_per_grp, fp) != HW_evts_per_grp)
      return -7;

   return 0;
}

int read_TID_events_header (FILE *fp, TID_events_header_t *header)
{
   // Number of threads
   if (fread (&(header->nb_threads), sizeof header->nb_threads, 1, fp) != 1) return -1;

   // Number of HW events per group
   unsigned HW_evts_per_grp;
   if (fread (&HW_evts_per_grp, sizeof HW_evts_per_grp, 1, fp) != 1)
      return -2;
   header->HW_evts_per_grp = HW_evts_per_grp;

   unsigned i; size_t len;
   // HW events name
   header->HW_evts_name = lc_malloc (HW_evts_per_grp * sizeof header->HW_evts_name[0]);
   for (i=0; i<HW_evts_per_grp; i++) {
      if (fread (&len, sizeof len, 1, fp) != 1) return -3;
      header->HW_evts_name[i] = lc_malloc (len + 1);
      if (fread (header->HW_evts_name[i], 1, len, fp) != len) return -4;
      header->HW_evts_name[i][len] = '\0';
   }

   // HW events list
   if (fread (&len, sizeof len, 1, fp) != 1) return -5;
   header->HW_evts_list = lc_malloc (len + 1);
   if (fread (header->HW_evts_list, 1, len, fp) != len) return -6;
   header->HW_evts_list[len] = '\0';

   // sampleTypesList
   uint64_t *sampleTypesList = lc_malloc (HW_evts_per_grp * sizeof sampleTypesList[0]);
   if (fread (sampleTypesList, sizeof sampleTypesList[0], HW_evts_per_grp, fp) != HW_evts_per_grp)
      return -7;
   header->sampleTypesList = sampleTypesList;

   return 0;
}

void free_TID_events_header (TID_events_header_t *header)
{
   // HW events name
   unsigned i;
   for (i=0; i<header->HW_evts_per_grp; i++) {
      lc_free (header->HW_evts_name[i]);
   }
   lc_free (header->HW_evts_name);

   // HW events list
   lc_free (header->HW_evts_list);

   // sampleTypesList
   lc_free (header->sampleTypesList);
}

int write_IP_events_header (FILE *fp, uint64_t tid, unsigned events_nb)
{
   // TID
   if (fwrite (&tid, sizeof tid, 1, fp) != 1) return -1;

   // Nb events
   if (fwrite (&events_nb, sizeof events_nb, 1, fp) != 1) return -2;

   return 0;
}

int read_IP_events_header  (FILE *fp, uint64_t *tid, unsigned *events_nb)
{
   // TID
   if (fread (tid, sizeof *tid, 1, fp) != 1) return -1;

   // Nb events
   if (fread (events_nb, sizeof *events_nb, 1, fp) != 1) return -2;

   return 0;
}

int write_IP_events (FILE *fp, uint64_t ip, const IP_events_t *IP_events, unsigned HW_evts_per_grp)
{
   // Address/IP
   if (fwrite (&ip, sizeof ip, 1, fp) != 1) return -1;

   // eventsNb (hit counts for each HW event)
   const hits_nb_t *eventsNb = IP_events->eventsNb;
   if (fwrite (eventsNb, sizeof eventsNb[0], HW_evts_per_grp, fp) != HW_evts_per_grp)
      return -2;

   // Callchains
   // Callchains: number of callchains
   size_t nb_callchains = lprof_queue_length (IP_events->callchains);
   if (fwrite (&nb_callchains, sizeof nb_callchains, 1, fp) != 1) return -3;

   // Callchains: for each callchain
   FOREACH_IN_LPROF_QUEUE (IP_events->callchains, cc_iter) {
      const IP_callchain_t* callchain = GET_DATA_T(IP_callchain_t *, cc_iter);

      // Callchains hits
      hits_nb_t nb_hits = callchain->nb_hits;
      if (fwrite (&nb_hits, sizeof nb_hits, 1, fp) != 1) return -4;

      // Callchain length (number of IPs/addresses)
      uint32_t nb_IPs = callchain->nb_IPs;
      if (fwrite (&nb_IPs, sizeof nb_IPs, 1, fp) != 1) return -5;

      // Callchain content (sequence of IPs/addresses)
      const uint64_t *IPs = callchain->IPs;
      if (fwrite (IPs, sizeof IPs[0], nb_IPs, fp) != nb_IPs)
         return -6;
   }

   return 0;
}

int read_IP_events  (FILE *fp, raw_IP_events_t *IP_events, unsigned HW_evts_per_grp)
{
   // Address/IP
   if (fread (&(IP_events->ip), sizeof IP_events->ip, 1, fp) != 1) return -1;

   // EventsNb (hit counts for each HW event)
   hits_nb_t *eventsNb = IP_events->eventsNb;
   if (fread (eventsNb, sizeof eventsNb[0], HW_evts_per_grp, fp) != HW_evts_per_grp)
      return -2;

   // Callchains
   // Callchains: number of callchains
   size_t nb_callchains;
   if (fread (&nb_callchains, sizeof nb_callchains, 1, fp) != 1) return -3;
   IP_events->nb_callchains = nb_callchains;

   if (nb_callchains > IP_events->max_nb_callchains) {
      size_t new_max = MAX (2 * IP_events->max_nb_callchains, nb_callchains);
      IP_events->callchains = lc_realloc (IP_events->callchains,
                                          new_max * sizeof IP_events->callchains[0]);

      unsigned i;
      for (i=IP_events->max_nb_callchains; i<new_max; i++) {
         uint64_t *IPs = lc_malloc0 (IP_events->max_callchain_len *
                                             sizeof IPs[0]);
         IP_events->callchains[i].IPs = IPs;
      }
      IP_events->max_nb_callchains = new_max;
   }

   // Callchains: for each callchain
   unsigned i;
   for (i=0; i<nb_callchains; i++) {
      // Callchain hits
      hits_nb_t nb_hits;
      if (fread (&nb_hits, sizeof nb_hits, 1, fp) != 1) return -4;

      // Callchain length (number of IPs/addresses)
      uint32_t nb_IPs;
      if (fread (&nb_IPs, sizeof nb_IPs, 1, fp) != 1) return -5;

      // Realloc/alloc as needed
      if (nb_IPs > IP_events->max_callchain_len) {
         size_t new_max = MAX (2 * IP_events->max_callchain_len, nb_IPs);
         unsigned j;
         for (j=0; j<IP_events->max_nb_callchains; j++) {
            uint64_t *IPs = lc_realloc (IP_events->callchains[j].IPs,
                                                new_max * sizeof IPs[0]);
            IP_events->callchains[j].IPs = IPs;
         }
         IP_events->max_callchain_len = new_max;
      }

      IP_callchain_t *cc = &(IP_events->callchains[i]);
      cc->nb_hits = nb_hits;
      cc->nb_IPs = nb_IPs;

      // Callchain content (sequence of IPs/addresses)
      if (fread (cc->IPs, sizeof cc->IPs[0], cc->nb_IPs, fp) != cc->nb_IPs)
         return -6;
   }

   return 0;
}

raw_IP_events_t *raw_IP_events_new (unsigned HW_evts_per_grp)
{
   raw_IP_events_t *new = lc_malloc0 (sizeof *new);
   if (!new) return NULL;

   const size_t max_nb_callchains =  100;
   const size_t max_callchain_len =   20;

   // Number of occurrences
   new->eventsNb = lc_malloc0 (HW_evts_per_grp * sizeof new->eventsNb [0]);

   // Callchains
   new->callchains = lc_malloc0 (max_nb_callchains * sizeof new->callchains [0]);
   new->max_nb_callchains = max_nb_callchains;
   new->max_callchain_len = max_callchain_len;
   unsigned i;
   for (i=0; i < max_nb_callchains; i++) {
      uint64_t *IPs = lc_malloc0 (max_callchain_len * sizeof IPs [0]);
      new->callchains[i].IPs = IPs;
   }

   return new;
}

void raw_IP_events_free (raw_IP_events_t *IP_events)
{
   lc_free (IP_events->eventsNb);
   unsigned i;
   for (i=0; i < IP_events->max_nb_callchains; i++) {
      lc_free (IP_events->callchains[i].IPs);
   }
   lc_free (IP_events->callchains);
   lc_free (IP_events);
}
