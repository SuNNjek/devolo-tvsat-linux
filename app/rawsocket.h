//////////////////////////////////////////////////////////////////////////
// devolo dLAN TV Sat control application
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
/// @file rawsocket.h
/// @brief "dLAN TV Sat Raw Socket Wrapper" - header
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#ifndef __RAWSOCKET_H
#define __RAWSOCKET_H

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <string>
#include <sys/socket.h>

//////////////////////////////////////////////////////////////////////////
/// Raw socket abstraction class
//////////////////////////////////////////////////////////////////////////
class CRawSocket
{
	public:
		CRawSocket() : m_family( AF_PACKET ), m_fd( -1 ),
				m_ifindex( 0 ),	m_proto( ETH_P_ALL ) {};
		CRawSocket( int family, int proto, int sub_proto );
		~CRawSocket();

		void close();
		bool open( unsigned short port,
				const char *interface = 0 );
		size_t receive( unsigned char *buf, size_t len,
				bool blocking = false,
				int timeout = 100000 ) const;
		bool send( const unsigned char *data, size_t data_len,
				char *addr, size_t addr_len ) const;

	private:
		int m_family;
		int m_fd;
		int m_ifindex;
		unsigned short m_port;
		int m_proto;
		int m_sub_proto;
};

#endif
