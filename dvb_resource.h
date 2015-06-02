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

#ifndef _DVB_RESOURCE_H_
#define _DVB_RESOURCE_H_

#include <stdint.h>

#define LAYER_FULL 0
#define LAYER_A    1
#define LAYER_B    2
#define LAYER_C    3


// structure to hold the currentstate of the resource
struct dvb_resource {
	
	// frontend device fd
	int frontend;

	// demux device fd
	int demux;

	// DVR device fd
	int dvr;
	
	char error_msg[256];
	int error_code;
};


// list available devices: in form of <name> TAB <identifier> 
int dvbres_listdevices(struct dvb_resource* res, char* buffer, int max_length);

// initiates the structure
int dvbres_init(struct dvb_resource* res);

// open a resource (tuning) (returns -1 on error)
int dvbres_open(struct dvb_resource* res, uint64_t freq, char* device, int layer_info);

// get if signal is present
int dvbres_signalpresent(struct dvb_resource* res);

// get if signal is locked
int dvbres_signallocked(struct dvb_resource* res);

// get signal strength 0: bad, 100: good
int dvbres_getsignalstrength(struct dvb_resource* res);

// get signal quality 0: bad, 100: good
int dvbres_getsignalquality(struct dvb_resource* res);

// closes the resource (returns -1 on error)
int dvbres_close(struct dvb_resource* res);

// releases all resources previously allocated (returns -1 on error)
int dvbres_release(struct dvb_resource* res);


// Only the public interface is defined here. See dvb_resource.c for protected
// functions.

#endif /* _DVB_RESOURCE_H_ */
