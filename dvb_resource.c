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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "dvb_resource.h"

// Saves error parameters and returns -1
int _dvbres_error(struct dvb_resource* res, char* msg, int code) 
{
    strncpy(res->error_msg, msg, sizeof(res->error_msg));
    res->error_msg[sizeof(res->error_msg) - 1] = 0;
    res->error_code = code;
    return -1;
}

// All OK with return value - zeroes error_msg and error_code and returns retval 
int _dvbres_ok_retval(struct dvb_resource* res, int retval) 
{
    res->error_msg[0] = 0;
    res->error_code = 0;	
    return retval;
}

// All OK - zeroes error_msg and error_code and return zero 
int _dvbres_ok(struct dvb_resource* res) 
{
	return _dvbres_ok_retval(res, 0);
}

int dvbres_init(struct dvb_resource* res) 
{
	memset(res, 0, sizeof(struct dvb_resource));
	return _dvbres_ok(res);
}

int dvbres_listdevices(struct dvb_resource* res, char* buffer, int max_length) 
{
	
  memset(buffer, 0, max_length);
  buffer[--max_length] = 0;
  buffer[0] = 0;
	
  char frontname[128];
  char adaptername[128];
  int rc;
  int pos = 0;
  
  int ifnum = 0;
  while (pos < max_length) 
  {
      // generate the root device name	to devprefix		
      sprintf(adaptername, "/dev/dvb/adapter%d", ifnum);
	    
      // generate the next device name and open it
      sprintf(frontname, "%s/frontend0", adaptername);

      // opening device
      int front = open(frontname, O_RDWR);
      if (front == -1 && ifnum == 0)
	  return _dvbres_error(res, "Error opening frontend.", errno);
      if (front == -1 && ifnum > 0)
	  return _dvbres_ok(res);	  
	  
      // reading status with the purpose of identifying DVB-S
      struct dvb_frontend_info finfo;
      rc = ioctl(front, FE_GET_INFO, &finfo);
      if (rc) 
      {
	  close(front);
	  return _dvbres_error(res, "Rading frontend info", errno);
      }
      
      // closing device
      rc = close(front);
      if (rc)
	  return _dvbres_error(res, "Closing frontend", errno);
      
      // replace tabs with spaces (tab is used as separator)
      unsigned int i;
      for (i=0; i<sizeof(finfo.name); i++)
	  if (finfo.name[i] == '\t')
	      finfo.name[i] = ' ';
      
      
      int slen;
      
      // tab (if not the first device)
      if (ifnum > 0) {
	  if (pos >= max_length)
	      return _dvbres_error(res, "Device enum buffer too small", -1);
	  buffer[pos++] = '\t';
      }
      
      // space check
      slen = strlen(finfo.name);
      if (pos + slen >= max_length)	
	  return _dvbres_error(res, "Device enum buffer too small", -1);
      // copy device name
      buffer[pos++] = '0' + ifnum;
      buffer[pos++] = ':';
      buffer[pos++] = ' ';
      
      strncpy(&buffer[pos], finfo.name, max_length - pos);
      pos += slen;
      
      // tab
      if (pos >= max_length)
	  return _dvbres_error(res, "Device enum buffer too small", -1);
      buffer[pos++] = '\t';
      
      // space check
      slen = strlen(adaptername);
      if (pos + slen >= max_length)	
	  return _dvbres_error(res, "Device enum buffer too small", -1);
      // copy device name
      strncpy(&buffer[pos], adaptername, max_length - pos);
      pos += slen;
      

      // next device
      ifnum++;
  }
  
  return _dvbres_error(res, "Device enum buffer to small", -1);
}

int dvbres_open(struct dvb_resource* res, uint64_t freq, char* device, int layer_info) {

    // return value (code) of calls
    int rc;
    
    // the index of adaper actually used	
    int adapternum = 0;
    
    // temporaray field to hold the root path (/dev/dvb/adapterN)
    char devprefix[64];
    
    // temporaray field to hold device names (/dev/dvb/adapterN/{something}M)
    char devname[64];
    
    // information about the actual frontend	
    struct dvb_frontend_info finfo;
    
    // if no device is given
    if (device == NULL) {

	// opening device
	do {
	    
	    // generate the root device name	to devprefix		
	    sprintf(devprefix, "/dev/dvb/adapter%d", adapternum);
	    
	    // generate the next device name and open it
	    sprintf(devname, "%s/frontend0", devprefix);
	    res->frontend = open(devname, O_RDWR);
	    if (res->frontend == -1)
		return _dvbres_error(res, "Error opening frontend device.", errno);
	    
	    // reading status with the purpose of identifying tuner type
	    rc = ioctl(res->frontend, FE_GET_INFO, &finfo);
	    if (rc) {
		close(res->frontend);
		return _dvbres_error(res, "Error reading frontend information.", errno);
	    }
	    
	    // if not DVB-T then we skip to the next adapter
	    if (finfo.type != 2) {
		close(res->frontend);
		res->frontend = 0;
		adapternum++;
	    }
	} while (!res->frontend);
	
    } else { // if device
	
	// copy the device parameter to the devprefix
	strncpy(devprefix, device, sizeof(devprefix));
	
	// opening device
	sprintf(devname, "%s/frontend0", devprefix);
	res->frontend = open(devname, O_RDWR);
	if (!res->frontend)
	    return _dvbres_error(res, "Error opening frontend.", errno);
	
	// reading status with the purpose of identifying tuner type
	rc = ioctl(res->frontend, FE_GET_INFO, &finfo);
	if (rc) {
	    close(res->frontend);
	    return _dvbres_error(res, "Reading frontend info", errno);
	}
	
	// if not DVB-T then we close and return an error
	if (finfo.type != 2) {
	    close(res->frontend);
			res->frontend = 0;
			return _dvbres_error(res, "Device is not a DVB-T frontend", -1);
	}
	
    } // if device
    
    int inversion = (finfo.caps & FE_CAN_INVERSION_AUTO) ? INVERSION_AUTO : INVERSION_OFF;
    int partial_reception = 0;
    int layers = 0;
    int segment_count = 0;
    switch (layer_info) 
    {
    case LAYER_FULL:
	layers = 7; // 0x1 | 0x2 | 0x4
	partial_reception = 1;
	segment_count = -1; // -1 means AUTO
	break;
    case LAYER_A:
	layers = 1; // 0x1
	partial_reception = 1;
	segment_count = 1;
	break;
    case LAYER_B:
	layers = 2; // 0x2
	partial_reception = 0;
	segment_count = -1;
	break;
    case LAYER_C:
	layers = 4; // 0x4
	partial_reception = 0;
	segment_count = -1;
	break;
    }
    
    if (partial_reception)
    {

	struct dtv_property myproperties[] = {
	    { .cmd = DTV_CLEAR }, /* Clear the driver before everything */ 
	    { .cmd = DTV_DELIVERY_SYSTEM, .u.data = SYS_ISDBT },
	    { .cmd = DTV_FREQUENCY, .u.data = freq }, /* Set frequency */ 
	    { .cmd = DTV_ISDBT_PARTIAL_RECEPTION, .u.data = partial_reception}, 
	    { .cmd = DTV_ISDBT_LAYERA_SEGMENT_COUNT, .u.data = segment_count},  // we only need to make sure one-seg has the correct settings
	    //		{ .cmd = DTV_ISDBT_LAYERB_SEGMENT_COUNT, .u.data = 12},  
	    //		{ .cmd = DTV_ISDBT_LAYERC_SEGMENT_COUNT, .u.data = 0},  
	    { .cmd = DTV_ISDBT_LAYER_ENABLED, .u.data = layers }, 
	    { .cmd = DTV_BANDWIDTH_HZ, .u.data = 6000000}, 
	    { .cmd = DTV_INVERSION, .u.data = inversion}, 
	    { .cmd = DTV_TUNE }, /* now actually tune to that frequency (no parameters needed) */ 
	};
	
	struct dtv_properties mydtvproperties = { 
	    .num = 9, /* The number of commands in the array */ 
	    .props = myproperties /* Pointer to the array */ 
	};
	
	rc = ioctl(res->frontend, FE_SET_PROPERTY, &mydtvproperties);
	if (rc) {
	    close(res->frontend);
	    return _dvbres_error(res, "Setting properties", errno);
	}
	
    }
    else
    {
	struct dtv_property myproperties[] = {
	    { .cmd = DTV_CLEAR }, /* now actually tune to that frequency (no parameters needed) */ 
	    { .cmd = DTV_DELIVERY_SYSTEM, .u.data = SYS_ISDBT }, /* Select DVB-S2 */ 
	    { .cmd = DTV_FREQUENCY, .u.data = freq }, /* Set frequency */ 
	    { .cmd = DTV_ISDBT_LAYER_ENABLED, .u.data = layers }, /* Set layers */ 
	    { .cmd = DTV_ISDBT_PARTIAL_RECEPTION, .u.data = partial_reception}, 
	    { .cmd = DTV_BANDWIDTH_HZ, .u.data = 6000000}, 
	    { .cmd = DTV_INVERSION, .u.data = inversion}, 
	    { .cmd = DTV_TUNE }, /* now actually tune to that frequency (no parameters needed) */ 
	};
	
	struct dtv_properties mydtvproperties = { 
	    .num = 8, /* The number of commands in the array */ 
	    .props = myproperties /* Pointer to the array */ 
	};
	
	rc = ioctl(res->frontend, FE_SET_PROPERTY, &mydtvproperties);
	if (rc) {
	    close(res->frontend);
	    return _dvbres_error(res, "Setting properties", errno);
	}
	
    }
    
    
    // setting up demux to forward ALL pids to the DVR device
    
    sprintf(devname, "%s/demux0", devprefix);
    res->demux = open(devname, O_RDWR);
    if (!res->demux) {
	close(res->frontend);
	return _dvbres_error(res, "Opening demux", errno);
    }
    
    struct dmx_pes_filter_params filter;
    filter.pid = 8192;
    filter.input = DMX_IN_FRONTEND;
    filter.output = DMX_OUT_TS_TAP;
    filter.pes_type = DMX_PES_OTHER;
    filter.flags = DMX_IMMEDIATE_START;

    rc = ioctl(res->demux, DMX_SET_PES_FILTER, &filter);
    if (rc) {
	close(res->frontend);
	close(res->demux);
	return _dvbres_error(res, "Setting up pes filter", errno);
    }
    
    
    //	opening DVR device (non-blocking mode)
    sprintf(devname, "%s/dvr0", devprefix);
    res->dvr = open(devname, O_RDONLY | O_NONBLOCK);
    if (!res->dvr) {
	close(res->frontend);
	close(res->demux);
	return _dvbres_error(res, "Opening dvr", errno);
    }
    
    // all ok
    return _dvbres_ok(res);
}

int dvbres_close(struct dvb_resource* res) {
    int rc;
    
    struct dtv_property myproperties[] = {
	{ .cmd = DTV_CLEAR }, /* Clear the driver before everything */ 
    };
	
    struct dtv_properties mydtvproperties = { 
	.num = 1, /* The number of commands in the array */ 
	.props = myproperties /* Pointer to the array */ 
    };
	
    rc = ioctl(res->frontend, FE_SET_PROPERTY, &mydtvproperties);
    if (rc)
	return _dvbres_error(res, "Reseting the tuner.", errno);

    // closing
    if (res->frontend) {
	close(res->frontend);
	res->frontend = 0;
    }
    if (res->demux) {
	close(res->demux);
	res->demux = 0;
    }
    if (res->dvr) {
	close(res->dvr);
	res->dvr = 0;
    }
    
    // all ok
    return _dvbres_ok(res);
}

// get if signal is present
int dvbres_signalpresent(struct dvb_resource* res) {
    int rc;
    int status;
    rc = ioctl(res->frontend, FE_READ_STATUS, &status);
    if (rc)
	return _dvbres_error(res, "Reading status.", errno);
    return (status & FE_HAS_SIGNAL) != 0;
}

// get if signal is locked
int dvbres_signallocked(struct dvb_resource* res) {
    int rc;
    int status;
    rc = ioctl(res->frontend, FE_READ_STATUS, &status);
    if (rc)
	return _dvbres_error(res, "Reading status.", errno);
    return (status & FE_HAS_LOCK) != 0;
}

// get signal level 0: bad, 100: good
int dvbres_getsignalstrength(struct dvb_resource* res) {
    int rc;
    int strength = 0;
    rc = ioctl(res->frontend, FE_READ_SIGNAL_STRENGTH, &strength);
    if (rc)
	return _dvbres_error(res, "Reading signal strength.", errno);
    return strength * 100 / 65535;
}

// get signal quality 0: bad, 100: good
int dvbres_getsignalquality(struct dvb_resource* res) {
    int rc;
    int snr = 0;
    rc = ioctl(res->frontend, FE_READ_SNR, &snr);
    if (rc)
	return _dvbres_error(res, "Reading signal strength.", errno);
    return snr * 100 / 65535;
}
