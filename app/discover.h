//////////////////////////////////////////////////////////////////////////
// devolo dLAN TV Sat device configuration
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
/// @file discover.h
/// @brief "dLAN TV Sat device discovery" - header
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#ifndef __TVSAT_DISCOVER_H
#define __TVSAT_DISCOVER_H

#include <list>
#include <stdint.h>
#include <string>

#include "rawsocket.h"
#include "streamin.h"
#include "udpsocket.h"

struct SNetIf {
	std::string	if_bcast;
	std::string	if_ip;
	std::string	if_name;
};

struct STVSatDev {
	std::string	dev_ip;
	uint8_t		dev_mac[ 6 ];
	SNetIf		net_if;
};

bool operator<( const STVSatDev &tvs1, const STVSatDev &tvs2 );
bool operator==( const STVSatDev &tvs1, const STVSatDev &tvs2 );

int expired( const timeval &stv, int sec = 1 );
void getIfInfo( std::list< SNetIf > &ifs, const std::string &bind_if );
void findDevices( std::list< STVSatDev > &found_devs,
		const std::string &bind_if = "", bool raw = false );
int parseIP( uint8_t *out, const char *in );
int parseMAC( uint8_t *out, const char *in );
ResponseHeader *receiveRaw( const CRawSocket &rsock, uint8_t *buf,
		int buf_len, size_t exp_size, uint16_t exp_cmd );
ResponseHeader *receiveUDP( const CUDPSocket &sock, uint8_t *buf,
		int buf_len, size_t exp_size, uint16_t exp_cmd );
#endif
