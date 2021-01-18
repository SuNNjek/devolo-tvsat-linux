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
/// @file rawsocket.cpp
/// @brief "dLAN TV Sat Raw Socket Wrapper" - implementation
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "rawsocket.h"

//////////////////////////////////////////////////////////////////////////
/// Constructor
//////////////////////////////////////////////////////////////////////////
CRawSocket::CRawSocket(int family, int proto, int sub_proto) :
        m_family(family), m_proto(proto),
        m_sub_proto(sub_proto) {
    m_fd = -1;
    m_ifindex = 0;
}

//////////////////////////////////////////////////////////////////////////
/// Destructor
//////////////////////////////////////////////////////////////////////////
CRawSocket::~CRawSocket() {
    if (m_fd >= 0)
        ::close(m_fd);
}

//////////////////////////////////////////////////////////////////////////
/// Closes an open socket
//////////////////////////////////////////////////////////////////////////
void CRawSocket::close() {
    if (m_fd >= 0)
        ::close(m_fd);

    m_fd = -1;
}

//////////////////////////////////////////////////////////////////////////
/// Opens a socket and binds it to the specified port and interface
//////////////////////////////////////////////////////////////////////////
bool CRawSocket::open(unsigned short port, const char *interface) {
    ifreq ifr;

    close();
    m_port = port;
    m_fd = socket(PF_PACKET, SOCK_RAW, htons(m_proto));

    if (m_fd < 0) {
        std::cerr << "socket() failed" << std::endl;
        close();
        return false;
    }

    if (interface) {
        strncpy(ifr.ifr_name, interface, IFNAMSIZ);
        ioctl(m_fd, SIOCGIFINDEX, &ifr);
        m_ifindex = ifr.ifr_ifindex;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a raw ethernet packet
/// @param data pointer to the buffer that holds the packet
/// @param data_len packet size
/// @param addr destination address
/// @param addr_len length of the destination address
/// @return true, if successful
/// @return false, otherwise
//////////////////////////////////////////////////////////////////////////
bool CRawSocket::send(const unsigned char *data, size_t data_len,
                      char *addr, size_t addr_len) const {
    if (addr_len > 8) {
        std::cerr << "address too long" << std::endl;
        return false;
    }

    if (m_fd < 0) {
        std::cerr << "socket not open" << std::endl;
        return false;
    }

    if (!m_ifindex) {
        std::cerr << "no interface specified" << std::endl;
        return false;
    }

    sockaddr_ll sa;
    memset(&sa, 0, sizeof(sockaddr_ll));
    memcpy(sa.sll_addr, addr, addr_len);
    sa.sll_halen = addr_len;
    sa.sll_family = m_family;
    sa.sll_ifindex = m_ifindex;

    if (sendto(m_fd, data, data_len, 0, (sockaddr *) &sa,
               sizeof(sockaddr_ll)) <= 0)
        std::cerr << "sendto() failed" << std::endl;

    return true;
}

//////////////////////////////////////////////////////////////////////////
/// Receives a raw packet in blocking or non-blocking mode
/// @param buf pointer to the buffer that will hold the received packet
/// @param len size of the packet buffer
/// @param blocking determines, if the reception should be blocking or not
/// @param timeout the time to wait for an incoming packet
/// @return	payload size of the received packet, if a packet has been
///		received
/// @return	0, if no packet has been received
//////////////////////////////////////////////////////////////////////////
size_t CRawSocket::receive(unsigned char *buf, size_t len,
                           bool blocking, int timeout) const {
    if (m_fd < 0) {
        std::cerr << "UDP socket not open" << std::endl;
        return 0;
    }

    fd_set set;
    FD_ZERO(&set);
    FD_SET(m_fd, &set);

    timeval to;
    to.tv_usec = timeout;
    to.tv_sec = 0;

    int sel = select(FD_SETSIZE, &set, 0, 0,
                     blocking ? 0 : &to);

    if (sel < 0) {
        std::cerr << "select() failed" << std::endl;
        return 0;
    }

    if (sel > 0) {
        sockaddr_ll sa;
        socklen_t salen = sizeof(sockaddr_ll);
        memset(&sa, 0, salen);

        int rbytes = recv(m_fd, buf, len, 0);

        if (rbytes < 0) {
            std::cerr << "recv() failed" << std::endl;
            return 0;
        }

        iphdr *iph = (iphdr *) (buf + sizeof(ether_header));

        if (m_sub_proto != 0 && iph->protocol != m_sub_proto)
            return 0;

        udphdr *udph;
        tcphdr *tcph;

        switch (m_sub_proto) {
            case IPPROTO_UDP:
                udph = (udphdr *) (buf + sizeof(iphdr) + sizeof(ether_header));
#ifdef __FAVOR_BSD
                if( udph->uh_dport != htons( m_port ) )
#else
                if (udph->dest != htons(m_port))
#endif
                    return 0;
                break;
            case IPPROTO_TCP:
                tcph = (tcphdr *) (buf + sizeof(iphdr) + sizeof(ether_header));
#ifdef __FAVOR_BSD
                if( tcph->th_dport != htons( m_port ) )
#else
                if (tcph->dest != htons(m_port))
#endif
                    return 0;
                break;
            default:
                break;
        }

        return (size_t) rbytes;
    }

    return 0;
}
