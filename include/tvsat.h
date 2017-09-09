//////////////////////////////////////////////////////////////////////////
// devolo dLAN TV Sat driver
// Copyright (C) 2008 devolo AG. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// Contact information:
//    devolo AG
//    Sonnenweg 11
//    D-52070 Aachen, Germany
//    gpl@devolo.de
//////////////////////////////////////////////////////////////////////////
#ifndef __TVSAT_H
#define __TVSAT_H

#ifdef __KERNEL__
  #include <linux/types.h>
#else
  #ifdef __CPLUSPLUS
    #include <cstdint>
  #else
    #include <stdint.h>
  #endif
#endif

#define TVSAT_CONTROL_DEVICE_NAME "tvsctl"
#define TVSAT_MAX_DISEQC_CMDS     8

#define TVS_GET_EVENT           _IOR( 'T', 1, struct tvsat_event )
#define TVS_HAS_LOCK            _IO ( 'T', 2 )
#define TVS_REGISTER_DEVICE     _IOWR( 'T', 3, struct tvsat_dev_id )
#define TVS_UNREGISTER_DEVICE   _IOW( 'T', 4, struct tvsat_dev_id )

enum tvsat_event_type
{
  TVSAT_EVENT_TUNE,
  TVSAT_EVENT_PID,
  TVSAT_EVENT_CONNECT,
  TVSAT_EVENT_DISCONNECT
};

enum tvsat_pid_action
{
  PID_START,
  PID_STOP
};

struct tvsat_diseqc_parameters
{
  uint8_t   type; // 0 = not in use, 1 = full command, 2 = simple burst
  uint16_t  burst_data;
  uint8_t   message[ 6 ];
  uint8_t   message_len;
};

struct tvsat_tuning_parameters
{
  unsigned int                    band;
  struct tvsat_diseqc_parameters  diseqc[ TVSAT_MAX_DISEQC_CMDS ];
  unsigned int                    fec;
  unsigned int                    frequency;
  unsigned int                    inversion;
  unsigned int                    modulation;
  unsigned int                    pilot;
  unsigned int                    polarization;
  unsigned int                    roll_off;
  unsigned int                    symbol_rate;
};

struct tvsat_pid_selection
{
  enum tvsat_pid_action action;
  unsigned int          pid;
  int                   type;
};

struct tvsat_dev_id
{
  int16_t   adapter;
  uint8_t   ip_addr[ 4 ];
  uint16_t  port;
  uint8_t   minor;
};

struct tvsat_event
{
  struct tvsat_event       *next;
  enum tvsat_event_type     type;
  union
  {
    struct tvsat_tuning_parameters  tune;
    struct tvsat_pid_selection      pid;
  } event;
};

#endif
