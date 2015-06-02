/* ISDB-T Capture. A DVB v5 API TS capture for Linux, for ISDB-TB 6MHz Latin American ISDB-T International.
 * Copyright (C) 2014 Rafael Diniz <rafael@riseup.net>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HAVE_RING_BUFFER__
#define HAVE_RING_BUFFER__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>

// Posix Ring Buffer implementation
// 

#define report_exceptional_condition() abort ()

struct ring_buffer
{
  void *address;
 
  unsigned long count_bytes;
  unsigned long write_offset_bytes;
  unsigned long read_offset_bytes;
};
 
void ring_buffer_create (struct ring_buffer *buffer, unsigned long order);

void ring_buffer_free (struct ring_buffer *buffer);
 
void *ring_buffer_write_address (struct ring_buffer *buffer);
 
void ring_buffer_write_advance (struct ring_buffer *buffer, unsigned long count_bytes);
 
void *ring_buffer_read_address (struct ring_buffer *buffer);
 
void ring_buffer_read_advance (struct ring_buffer *buffer, unsigned long count_bytes);
 
unsigned long ring_buffer_count_bytes (struct ring_buffer *buffer);
 
unsigned long ring_buffer_count_free_bytes (struct ring_buffer *buffer);
 
void ring_buffer_clear (struct ring_buffer *buffer);


#ifdef __cplusplus
};
#endif

#endif /* HAVE_RING_BUFFER__ */
