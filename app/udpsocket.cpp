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
/// @file udpsocket.cpp
/// @brief "dLAN TV Sat UDP Socket Wrapper" - implementation
/// @author Christian Petry
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "udpsocket.h"

//////////////////////////////////////////////////////////////////////////
/// Constructor
//////////////////////////////////////////////////////////////////////////
CUDPSocket::CUDPSocket()
{
  m_fd = -1;
  memset( &m_sock_addr, 0, sizeof( sockaddr_in ) );
}

//////////////////////////////////////////////////////////////////////////
/// Destructor
//////////////////////////////////////////////////////////////////////////
CUDPSocket::~CUDPSocket()
{
  if ( m_fd >= 0 )
    ::close( m_fd );
}

//////////////////////////////////////////////////////////////////////////
/// Opens a socket and binds it to the specified port
//////////////////////////////////////////////////////////////////////////
bool CUDPSocket::open( unsigned short port )
{
  bool rc = false;

  close();

  m_fd = socket( PF_INET, (int)SOCK_DGRAM, IPPROTO_UDP );

  if( m_fd < 0)
  {
    std::cerr << "socket() failed" << std::endl;
    close();
  }
  else
  {
    int reuse = 1;
    setsockopt( m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( int ) );

    int bcast = 1;
    setsockopt( m_fd, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof( int ) );

    m_sock_addr.sin_family      = AF_INET;
    m_sock_addr.sin_port        = htons( port );
    m_sock_addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if( bind( m_fd, (sockaddr *)&m_sock_addr, sizeof( sockaddr_in ) ) < 0)
    {
      std::cerr << "bind() failed" << std::endl;
      close();
    }
    else
      rc = true;
  }

  return rc;
}

//////////////////////////////////////////////////////////////////////////
/// Closes an open socket
//////////////////////////////////////////////////////////////////////////
void CUDPSocket::close()
{
  if( m_fd >= 0 )
    ::close( m_fd );

  m_fd = -1;
  memset( &m_sock_addr, 0, sizeof( sockaddr_in ) );
}

//////////////////////////////////////////////////////////////////////////
/// Gets the local port number of the socket
//////////////////////////////////////////////////////////////////////////
unsigned short CUDPSocket::getPort()
{
      sockaddr_in sa;
      memset( &sa, 0, sizeof( sockaddr_in ) );
      socklen_t sa_len = sizeof( sockaddr_in );
      getsockname( m_fd, (sockaddr *)&sa, &sa_len );
      return ntohs( sa.sin_port );
}

//////////////////////////////////////////////////////////////////////////
/// Sends a UDP packet
/// @param data pointer to the buffer that holds the payload
/// @param len payload size
/// @param ipaddr destination IP address (can be a broadcast address)
/// @param port destination port number
/// @return true, if successful
/// @return false, otherwise
//////////////////////////////////////////////////////////////////////////
bool CUDPSocket::send( const unsigned char *data, size_t len, const std::string &ipaddr, unsigned short port ) const
{
  bool rc = false;

  in_addr a;

  if( !inet_aton( ipaddr.c_str(), &a ) )
    std::cerr << "Invalid IP address" << std::endl;
  else
    if( m_fd < 0)
      std::cerr << "UDP socket not open" << std::endl;
    else
    {
      sockaddr_in sa;
      memset( &sa, 0, sizeof( sockaddr_in ) );
      sa.sin_family      = AF_INET;
      sa.sin_addr.s_addr = a.s_addr;
      sa.sin_port        = htons( port );

      if( sendto( m_fd, data, len, 0, (sockaddr *)&sa, sizeof( sockaddr_in ) ) <= 0 )
        std::cerr << "sendto() failed" << std::endl;
      else
        rc = true;
    }

  return rc;
}

//////////////////////////////////////////////////////////////////////////
/// Receives a UDP packet in blocking or non-blocking mode
/// @param buf pointer to the buffer that will hold the received payload
/// @param len size of the payload buffer
/// @param blocking determines, if the reception should be blocking or not
/// @return payload size of the received packet, if a packet has been
///         received
/// @return 0, if no packet has been received
//////////////////////////////////////////////////////////////////////////
size_t CUDPSocket::receive( unsigned char *buf, size_t len, bool blocking, int timeout ) const
{
  size_t ret = 0;

  if( m_fd < 0)
    std::cerr <<  "UDP socket not open" << std::endl;
  else
  {
    fd_set set;
    FD_ZERO( &set );
    FD_SET( m_fd, &set );

    timeval to;
    to.tv_usec = timeout;
    to.tv_sec = 0;

    int sel = select( FD_SETSIZE, &set, 0, 0, blocking ? 0 : &to );

    if( sel < 0 )
      std::cerr << "select() failed" << std::endl;
    else
      if( sel > 0 )
      {
        sockaddr_in sa;
        socklen_t salen = sizeof( sockaddr_in );
        memset( &sa, 0, salen );

        int rbytes = recvfrom( m_fd, buf, len, 0, (sockaddr *)&sa, &salen );

        if( rbytes < 0 )
          std::cerr << "recv() failed" << std::endl;
        else
          ret = (size_t)rbytes;
      }
  }

  return ret;
}
