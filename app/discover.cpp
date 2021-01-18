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
/// @file discover.cpp
/// @brief "dLAN TV Sat device discovery" - implementation
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <utility>
#include <unistd.h>

#include "discover.h"

bool operator<(const STVSatDev &tvs1, const STVSatDev &tvs2) {
    return (memcmp(tvs1.dev_mac, tvs2.dev_mac, 6) < 0);
}

bool operator==(const STVSatDev &tvs1, const STVSatDev &tvs2) {
    return (memcmp(tvs1.dev_mac, tvs2.dev_mac, 6) == 0);
}

//////////////////////////////////////////////////////////////////////////
/// Checks if a timestamp is older than a given amount of seconds
//////////////////////////////////////////////////////////////////////////
int expired(const timeval &stv, int sec) {
    timeval ctv;
    gettimeofday(&ctv, 0);

    if ((ctv.tv_sec == (stv.tv_sec + sec) &&
         ctv.tv_usec >= stv.tv_usec) ||
        ctv.tv_sec > (stv.tv_sec + sec))
        return 1;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Gathers some information on all ip capable network interfaces
///
/// @param[out] ifs a list of interface information structs
/// @param bind_if the network interface to bind to
//////////////////////////////////////////////////////////////////////////
void getIfInfo(std::list<SNetIf> &ifs, const std::string &bind_if) {
    uint8_t buf[2048];

    ifconf ifc;
    ifc.ifc_len = 2048;
    ifc.ifc_buf = (char *) buf;

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0) {
        std::cerr << "Error: failed to create IP socket"
                  << std::endl;
        return;
    }

    // get configuration information on all network interfaces
    if (ioctl(s, SIOCGIFCONF, &ifc)) {
        std::cerr << "Error: failed to get ifconfig" << std::endl;
        return;
    }

    // for each entry in the config information...
    ifreq *ifr = (ifreq *) ifc.ifc_req;

    for (unsigned int i = 0; i < ifc.ifc_len / sizeof(ifreq);
         ++i, ++ifr) {
        // ...check if this is the correct interface...
        if (!bind_if.empty() && bind_if != ifr->ifr_name)
            continue;

        // ...get the ip address...
        char ip[INET_ADDRSTRLEN];
        sockaddr_in *s_addr = (sockaddr_in *) &ifr->ifr_broadaddr;
        inet_ntop(AF_INET, &s_addr->sin_addr, ip,
                  INET_ADDRSTRLEN);

        if (ioctl(s, SIOCGIFBRDADDR, ifr)) {
            std::cerr << "Error: failed to get broadcast ip "
                         "address" << std::endl;
            return;
        }

        // ...and the broadcast address
        char bip[INET_ADDRSTRLEN];
        s_addr = (sockaddr_in *) &ifr->ifr_broadaddr;
        inet_ntop(AF_INET, &s_addr->sin_addr, bip,
                  INET_ADDRSTRLEN);

        if (strcmp(bip, "0.0.0.0")) {
            SNetIf net_if;
            net_if.if_bcast = bip;
            net_if.if_ip = ip;
            net_if.if_name = ifr->ifr_name;
            ifs.insert(ifs.end(), net_if);
        }
    }

    close(s);
}

//////////////////////////////////////////////////////////////////////////
/// Broadcasts info requests on all network interfaces to find devices
///
/// @param[out]	found_devs a list of ip addresses of the discovered
///		devices together with the ip address of the respective
///		network interface
/// @param bind_if network interface to bind to
/// @param raw if true, use a raw socket to receive replies
//////////////////////////////////////////////////////////////////////////
void findDevices(std::list<STVSatDev> &found_devs,
                 const std::string &bind_if, bool raw) {
    uint8_t buf[IP_MAXPACKET];
    ResponseHeader *rh;

    std::list<SNetIf> ifs;
    getIfInfo(ifs, bind_if);

    // construct a GetInfo request
    RequestGetInfo rgi;
    memset(&rgi, 0, sizeof(RequestGetInfo));
    rgi.mHeader.mCommand = htons(cCmdGetInfo);
    rgi.mHeader.mSize = htons(sizeof(RequestGetInfo));

    CUDPSocket sock;
    sock.open(0);

    // send GetInfo requests to all broadcast addresses
    for (std::list<SNetIf>::iterator it = ifs.begin();
         it != ifs.end(); ++it) {
        sock.send((const uint8_t *) &rgi,
                  sizeof(RequestGetInfo), it->if_bcast,
                  11111);

        CRawSocket rsock = CRawSocket(AF_INET, ETH_P_IP,
                                      IPPROTO_UDP);
        if (raw)
            rsock.open(sock.getPort(), it->if_name.c_str());

        timeval stv;
        gettimeofday(&stv, 0);

        // wait for a correct response
        while (1) {
            if (expired(stv))
                break;

            if (raw)
                rh = receiveRaw(rsock, buf,
                                ETHER_MAX_LEN,
                                sizeof(ResponseGetInfo),
                                cCmdGetInfo);
            else
                rh = receiveUDP(sock, buf,
                                IP_MAXPACKET,
                                sizeof(ResponseGetInfo),
                                cCmdGetInfo);

            if (!rh)
                continue;

            ResponseGetInfo *rgi = (ResponseGetInfo *) rh;

            // add a device to the list
            STVSatDev tsdev;
            memcpy(tsdev.dev_mac, rgi->mMacAddress,
                   sizeof(tsdev.dev_mac));

            char ip[16];
            snprintf(ip, 16, "%u.%u.%u.%u",
                     rgi->mIpAddress[0],
                     rgi->mIpAddress[1],
                     rgi->mIpAddress[2],
                     rgi->mIpAddress[3]);

            tsdev.dev_ip = ip;
            tsdev.net_if = *it;
            found_devs.insert(found_devs.end(), tsdev);
        }

    }

    sock.close();
}

//////////////////////////////////////////////////////////////////////////
/// Parses an IP address string
///
/// @param[out] out array containing the IP address in raw 4-byte format
/// @param in string containing an IP address in 'nnn.nnn.nnn.nnn'
///	notation
//////////////////////////////////////////////////////////////////////////
int parseIP(uint8_t *out, const char *in) {
    in_addr addr, haddr;
    int c = 0;
    char *ptr = strchr(const_cast<char *>(in), '.');

    while (ptr) {
        ++c;
        ptr = strchr(ptr + 1, '.');
    }

    if (c != 3)
        return -EINVAL;

    if (inet_aton(in, &addr) == 0)
        return -EINVAL;

    haddr.s_addr = ntohl(addr.s_addr);

    for (int i = 0; i < 4; ++i)
        out[i] = ((uint8_t *) &haddr.s_addr)[3 - i];

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Parses a MAC address string
///
/// @param[out] out array containing the MAC address in raw 6-byte format
/// @param in string containing a MAC address in 'xx:xx:xx:xx:xx:xx'
///	notation
//////////////////////////////////////////////////////////////////////////
int parseMAC(uint8_t *out, const char *in) {
    ether_addr *addr;

    if (!(addr = ether_aton(in)))
        return -EINVAL;

    memcpy(out, addr->ether_addr_octet, 6);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response on a raw socket
///
/// @param rsock the raw socket to listen on
/// @param buf buffer for the received packet
/// @param buf_len length of the buffer
/// @param exp_size minimum expected size of the packet
/// @param exp_cmd expected command
/// @return a pointer to the response, if one was received
/// @return 0, otherwise
//////////////////////////////////////////////////////////////////////////
ResponseHeader *receiveRaw(const CRawSocket &rsock, uint8_t *buf,
                           int buf_len, size_t exp_size, uint16_t exp_cmd) {
    const size_t hdr_size = sizeof(ether_header) + sizeof(iphdr) +
                            sizeof(udphdr);

    int rbytes = rsock.receive(buf, buf_len, false, 1);

    if (rbytes == 0)
        return 0;

    if ((rbytes - hdr_size) < exp_size)
        return 0;

    ResponseHeader *rh = (ResponseHeader *) (buf + hdr_size);

    if ((ntohs(rh->mCommand) != exp_cmd) ||
        (rh->mResult != 0))
        return 0;

    return rh;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response on a UDP socket
///
/// @param rsock the raw socket to listen on
/// @param buf buffer for the received UDP payload
/// @param buf_len length of the buffer
/// @param exp_size minimum expected size of the packet
/// @param exp_cmd expected command
/// @return a pointer to the response, if one was received
/// @return 0, otherwise
//////////////////////////////////////////////////////////////////////////
ResponseHeader *receiveUDP(const CUDPSocket &sock, uint8_t *buf,
                           int buf_len, size_t exp_size, uint16_t exp_cmd) {
    timeval stv;
    gettimeofday(&stv, 0);

    int rbytes = sock.receive(buf, buf_len, false, 1);

    if (rbytes == 0)
        return 0;

    if (rbytes < (int) exp_size)
        return 0;

    ResponseHeader *rh = (ResponseHeader *) buf;

    if ((ntohs(rh->mCommand) != exp_cmd) ||
        (rh->mResult != 0))
        return 0;

    return rh;
}
