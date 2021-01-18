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
/// @file udpsocket.h
/// @brief "dLAN TV Sat UDP Socket Wrapper" - header
/// @author Christian Petry
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#ifndef __UDPSOCKET_H
#define __UDPSOCKET_H

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>

//////////////////////////////////////////////////////////////////////////
/// UDP socket abstraction class
///
/// This class provides a simple abstraction of a UDP socket, which can
/// be used for both sending and receiving
//////////////////////////////////////////////////////////////////////////
class CUDPSocket {

public:
    CUDPSocket();

    ~CUDPSocket();

    void close();

    unsigned short getPort();

    bool open(unsigned short port);

    size_t receive(unsigned char *buf, size_t len, bool blocking = true, int timeout = 100000) const;

    bool send(const unsigned char *data, size_t len, const std::string &ipaddr, unsigned short port) const;

private:
    int m_fd;
    unsigned short m_port;
    sockaddr_in m_sock_addr;
};

#endif
