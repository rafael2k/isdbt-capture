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
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>


#define BUFFER_SIZE 4096
#define MAX_RETRIES 2


#include "dvb_resource.h"
#include "ring_buffer.h"

// Latin America channel assignments for ISDB-T International
uint64_t tv_channels[] =  
{ 
    /* 0 */ 0, /* index placeholders... */
    /* 1 */ 0, // never used for broadcasting
    /* 2 */ 0, // what to do?
    /* 3 */ 0, // what to do?
    /* 4 */ 0, // what to do?
    /* 5 */ 0, // future allocation for radio broadcasting
    /* 6 */ 0, // future allocation for radio broadcasting
    /* 7 */ 177142000,
    /* 8 */ 183142000,
    /* 9 */ 189142000,
    /* 10 */ 195142000,
    /* 11 */ 201142000,
    /* 12 */ 207142000,
    /* 13 */ 213142000,
    /* 14 */ 473142000,
    /* 15 */ 479142000,
    /* 16 */ 485142000,
    /* 17 */ 491142000,
    /* 18 */ 497142000,
    /* 19 */ 503142000,
    /* 20 */ 509142000,
    /* 21 */ 515142000,
    /* 22 */ 521142000,
    /* 23 */ 527142000,
    /* 24 */ 533142000,
    /* 25 */ 539142000,
    /* 26 */ 545142000,
    /* 27 */ 551142000,
    /* 28 */ 557142000,
    /* 29 */ 563142000,
    /* 30 */ 569142000,
    /* 31 */ 575142000,
    /* 32 */ 581142000,
    /* 33 */ 587142000,
    /* 34 */ 593142000,
    /* 35 */ 599142000,
    /* 36 */ 605142000,
    /* 37 */ 611142000, // radio astronomy reserved (we are in rx mode anyway, so no harm here...
    /* 38 */ 617142000, 
    /* 39 */ 623142000,
    /* 40 */ 629142000,
    /* 41 */ 635142000,
    /* 42 */ 641142000,
    /* 43 */ 647142000,
    /* 44 */ 653142000,
    /* 45 */ 659142000,
    /* 46 */ 665142000,
    /* 47 */ 671142000,
    /* 48 */ 677142000,
    /* 49 */ 683142000,
    /* 50 */ 689142000,
    /* 51 */ 695142000,
    /* 52 */ 701142000,
    /* 53 */ 707142000,
    /* 54 */ 713142000,
    /* 55 */ 719142000,
    /* 56 */ 725142000,
    /* 57 */ 731142000,
    /* 58 */ 737142000, 
    /* 59 */ 743142000,
    /* 60 */ 749142000,
    /* 61 */ 755142000,
    /* 62 */ 761142000,
    /* 63 */ 767142000,
    /* 64 */ 773142000,
    /* 65 */ 779142000,
    /* 66 */ 785142000,
    /* 67 */ 791142000,
    /* 68 */ 797142000,
    /* 69 */ 803142000,
};

/* global variables */
struct dvb_resource res;
FILE *ts = NULL;
FILE *player = NULL;

// thread and ring buffer variables...
pthread_t output_thread_id;
pthread_mutex_t output_mutex;
pthread_cond_t output_cond;
struct ring_buffer output_buffer; 

int keep_reading;

int scan_channels(char *output_file, bool uhf_only) 
{
  struct dvb_resource res;
  int layer_info = LAYER_A;

  FILE *fp = fopen(output_file, "w");
  int channel_id = 0;

  for (int channel_counter = (uhf_only == true)? 14: 7;
       channel_counter <= 69; channel_counter++)
  {

      fprintf(stderr, "------------------------------------------------------------------\n");
      fprintf(stderr, "Channel = %d  Frequency = %lluHz\n", channel_counter, (unsigned long long) tv_channels[channel_counter]);

      dvbres_init(&res);

      if (dvbres_open(&res, tv_channels[channel_counter], NULL, layer_info) < 0)
      {
	  fprintf(stderr, "%s\n", res.error_msg);
	  exit(EXIT_FAILURE);
      }
      
      int i;
      fprintf(stderr, "Tuning");
      for (i = 0; i < MAX_RETRIES && dvbres_signallocked(&res) <= 0; i++)
      {
	  sleep(1);
	  fprintf(stderr, ".");
      }
      if (i == MAX_RETRIES)
      {
	  fprintf(stderr, "\nSignal not locked, next channel\n");
	  dvbres_close(&res); 
	  continue;
      }
      fprintf(stderr, "\nSignal locked!\n");

      channel_id++;
      fprintf(fp, "id %.2d name Canal_%.2d frequency %llu segment 1SEG\n", channel_id, channel_counter, (unsigned long long) tv_channels[channel_counter]);

      int power = dvbres_getsignalstrength(&res);
      if (power != 0)
	  fprintf(stderr, "Signal power = %d\n", power);
      
      int snr = dvbres_getsignalquality(&res);
      if (snr != 0)
	  fprintf(stderr, "Signal quality = %d\n", snr);
      
      dvbres_close(&res);
  }

  fclose(fp);

  return 0;
}

void finish(int s){
    fprintf(stderr, "\nExiting...\n");

    keep_reading = 0;

    pthread_join(output_thread_id, NULL);

    dvbres_close(&res);

    if (ts)
	fclose(ts);

    if (player)
	fclose(player);

    exit(EXIT_SUCCESS); 
}

void *output_thread(void *nothing)
{
    void *addr;
    int bytes_read;
    char buffer[BUFFER_SIZE];
    
    while (keep_reading)
    {
	bytes_read = read(res.dvr, buffer, BUFFER_SIZE);
	if (bytes_read <= 0)
	{
	    struct pollfd fds[1];
	    fds[0].fd = res.dvr;
	    fds[0].events = POLLIN;
	    poll(fds, 1, -1);
	    continue;
	}
	
    try_again_write:
	if (ring_buffer_count_free_bytes (&output_buffer) >= bytes_read)
	{ 
	    pthread_mutex_lock(&output_mutex);
            addr = ring_buffer_write_address (&output_buffer);
            memcpy(addr, buffer, bytes_read);
            ring_buffer_write_advance(&output_buffer, bytes_read);
	    pthread_cond_signal(&output_cond);
            pthread_mutex_unlock(&output_mutex);
	}
        else
        {
	    fprintf(stderr, "Buffer full, nich gut...\n");
	    pthread_mutex_lock(&output_mutex);
            pthread_cond_wait(&output_cond, &output_mutex);
            pthread_mutex_unlock(&output_mutex);
            goto try_again_write;
        }
    }

    return NULL;
}

int main (int argc, char *argv[])
{
    uint64_t freq = 599142000ULL;
    int bytes_written;
    char buffer[BUFFER_SIZE];
    char output_file[512];
    char scan_file[512];
    int layer_info = LAYER_FULL;
    bool scan_mode = false, info_mode = false, player_mode = false, tsoutput_mode = false;
    char temp_file[] = "/tmp/out.ts";
    char player_cmd[256];

    int opt;
    void *addr;


    pthread_mutex_init(&output_mutex, NULL); 
    pthread_cond_init(&output_cond, NULL); 
    ring_buffer_create(&output_buffer, 28); 
    
    signal (SIGINT,finish);
    
    fprintf(stderr, "isdbt-capture by Rafael Diniz -  rafael (AT) riseup (DOT) net\n");
    fprintf(stderr, "License: GPLv3+\n\n");

    if (argc < 2)
    {
    manual:
	fprintf(stderr, "Usage modes: \n%s -c channel_number -p player -o output.ts [-l layer_info]\n", argv[0]);
	fprintf(stderr, "%s [-s channels.txt]\n", argv[0]);
	fprintf(stderr, "%s [-i]\n", argv[0]);
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, " -c                Channel number (7-69) (Mandatory).\n");
	fprintf(stderr, " -o                Output TS filename (Optional).\n");
	fprintf(stderr, " -p                Choose a player to play the selected channel (Eg. \"mplayer -vf yadif\" or \"vlc\") (Optional).\n");
	fprintf(stderr, " -l                Layer information. Possible values are: 0 (All layers), 1 (Layer A), 2 (Layer B), 3 (Layer C) (Optional).\n\n");
	fprintf(stderr, " -s channels.cfg   Scan for channels, store them in a file and exit.\n");
        fprintf(stderr, " -i                Print ISDB-T device information and exit.\n");
	fprintf(stderr, "\nTo quit press 'Ctrl+C'.\n");
	exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "io:c:l:s:p:")) != -1) 
    {
        switch (opt) 
        {
	case 'i':
	    info_mode = true;
	    break;
	case 'o':
	    tsoutput_mode = true;
	    strcpy(output_file, optarg);
	    break;
	case 'c':
	    freq = tv_channels[atoi(optarg)];
	    fprintf(stderr, "Frequency %llu (CH %d) selected.\n", (unsigned long long) freq, atoi(optarg));
	    break;
	case 'l':
	    layer_info = atoi(optarg);
	    fprintf(stderr, "Layer %d selected\n", layer_info);
	    break;
	case 's':
	    scan_mode = true;
	    strcpy(scan_file, optarg);
	    break;
	case 'p':
	    player_mode = true;
	    strcpy(player_cmd, optarg);
	    break;
	default:
	    goto manual;
	}
    }
    
    if (info_mode == true)
    {
	memset(buffer, 0, BUFFER_SIZE);
	fprintf(stderr, "Devices information:\n");
	if (dvbres_listdevices(&res, buffer, BUFFER_SIZE) < 0)
	{
	    fprintf(stderr, "%s\n\n", res.error_msg);
	}
	else
	{
	    fprintf(stderr, "%s\n\n", buffer);
	}
    }

    if(scan_mode == true)
    {
	fprintf(stderr, "Scan information:\n");
	scan_channels(scan_file, true);
    }

    if (info_mode == true || scan_mode == true)
    {
	exit(EXIT_SUCCESS);
    }


    fprintf(stderr, "Initializing DVB structures.\n");
    dvbres_init(&res);

    fprintf(stderr, "Opening DVB devices.\n");
    if (dvbres_open(&res, freq, NULL, layer_info) < 0)
    {
	fprintf(stderr, "%s\n", res.error_msg);
	exit(EXIT_FAILURE);
    }

    int i;
    fprintf(stderr, "Tuning.");
    for (i = 0; i < MAX_RETRIES && dvbres_signallocked(&res) <= 0; i++)
    {
	sleep(1);
	fprintf(stderr, ".");
    }
    if (i == MAX_RETRIES)
    {
	fprintf(stderr, "\nSignal not locked.\n");
	dvbres_close(&res);
	return -1;
    }
    fprintf(stderr, "\nSignal locked!\n");
    
    
    if (player_mode == true)
    {
	unlink(temp_file);
	mkfifo(temp_file, S_IRWXU | S_IRWXG | S_IRWXO);

	char cmd[512];
	sprintf(cmd, "%s %s &", player_cmd, temp_file);
	fprintf(stderr, "Running: %s\n", cmd);
	system(cmd);
	
	player = fopen(temp_file, "w");
	if (player == NULL)
	{
	    fprintf(stderr, "Error opening fifo: %s.\n", temp_file);
	    exit(EXIT_FAILURE);
	}
	else
	{
	    fprintf(stderr, "Fifo %s opened.\n", temp_file);
	}

    }

    if (tsoutput_mode == true)
    {
	ts = fopen(output_file, "w");
	if (ts == NULL)
	{
	    fprintf(stderr, "Error opening file: %s.\n", output_file);
	    exit(EXIT_FAILURE);
	}
	else
	{
	    fprintf(stderr, "File %s opened.\n", output_file);
	}
    }


    int power = dvbres_getsignalstrength(&res);
    if (power != 0)
	fprintf(stderr, "Signal power = %d\n", power);

    int snr = dvbres_getsignalquality(&res);
    if (snr != 0)
	fprintf(stderr, "Signal quality = %d\n", snr);

    // starting output thread
    keep_reading = 1;
    pthread_create(&output_thread_id, NULL, output_thread, NULL);

    while (1) 
    {
    try_again_read:
	if (ring_buffer_count_bytes(&output_buffer) >= BUFFER_SIZE)
	{
	    pthread_mutex_lock(&output_mutex);
	    addr = ring_buffer_read_address(&output_buffer);
	    memcpy(buffer, addr, BUFFER_SIZE);
	    ring_buffer_read_advance(&output_buffer, BUFFER_SIZE);
	    pthread_cond_signal(&output_cond);
	    pthread_mutex_unlock(&output_mutex);
	}
	else
	{	    
	    pthread_mutex_lock(&output_mutex);
	    pthread_cond_wait(&output_cond, &output_mutex);
	    pthread_mutex_unlock(&output_mutex);
	    goto try_again_read;
	}

	if (tsoutput_mode == true)
	    bytes_written = fwrite(buffer, 1, BUFFER_SIZE, ts);

	if (player_mode == true)
	    bytes_written = fwrite(buffer, 1, BUFFER_SIZE, player);

	if (bytes_written != BUFFER_SIZE)
	    fprintf(stderr, "bytes_written = %d != bytes_read = %d\n", bytes_written, BUFFER_SIZE);

	// small trick to not call the api too much
	if (!(i++ % 100))
	{
	    power = dvbres_getsignalstrength(&res);
	    fprintf(stderr, "Signal power = %d%%\r", power);
	    
	}
	
    }
    
    return EXIT_SUCCESS;
}
