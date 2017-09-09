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
/// @file tvsatcfg.cpp
/// @brief "dLAN TV Sat device configuration" - implementation
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <list>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "discover.h"
#include "log.h"
#include "streamin.h"
#include "tvsatcfg.h"

#define VERSION	"0.9"
#define HELPTXT	"tvsatcfg (v" VERSION ")\n"\
		"Usage: tvsatcfg [options]\n"\
		"\n"\
		"Options:\n"\
		"  -a           --all           look for devices that are not in any local ip\n"\
		"                               network (needs root privileges)\n"\
		"  -b if        --bind if       bind to network interface 'if'\n"\
		"  -d dev       --device dev    specify a TV Sat device by its MAC address\n"\
		"  -h           --help          show this help text\n"\
		"  -i ip        --ipaddr ip     set the device's IP address to 'ip' (you need\n"\
		"                               to specify the device with -d; implies -a);\n"\
		"                               use the keyword 'dhcp' to enable the device's\n"\
		"                               dhcp client\n"\
		"  -m mask      --netmask mask  set the device's network mask to 'mask' (only\n"\
		"                               valid in combination with -i)\n"

static option long_opts[] = {
	{ "all", no_argument, 0, 'a' },
	{ "bind", required_argument, 0, 'b' },
	{ "device", required_argument, 0, 'd' },
	{ "help", no_argument, 0, 'h' },
	{ "ipaddr", required_argument, 0, 'i' },
	{ "netmask", required_argument, 0, 'm' }
};

//////////////////////////////////////////////////////////////////////////
/// Extracts one bit from a byte
//////////////////////////////////////////////////////////////////////////
bool getBitc( uint8_t c, int idx )
{
	uint8_t b = 0x80;
	return ((b >> idx) & c) != 0;
}

//////////////////////////////////////////////////////////////////////////
/// Extracts one bit from a long integer
//////////////////////////////////////////////////////////////////////////
bool getBitl( uint32_t l, int idx )
{
	uint32_t b = 0x80000000;
	return ((b >> idx) & l) != 0;
}

//////////////////////////////////////////////////////////////////////////
/// Calculates the CRC sum for the NVS user data
//////////////////////////////////////////////////////////////////////////
void calcCRC( NvsUserData *nvs )
{
	nvs->mCrc[ 0 ] = 0xbe;
	nvs->mCrc[ 1 ] = 0xef;
	nvs->mCrc[ 2 ] = 0xba;
	nvs->mCrc[ 3 ] = 0xbe;

	uint8_t *data = (uint8_t *)nvs;
	uint32_t crc = ~0;

	for( size_t i = 0; i < sizeof( NvsUserData ); ++i )
		for( int j = 0; j < 8; ++j ) {
			bool b1 = getBitc( data[ i ], j );
			bool b2 = getBitl( crc, 0 );

			crc <<= 1;

			if( b1 ^ b2 )
				crc ^= 0x04c11db7;
		}

	crc = htonl( crc );
	memcpy( nvs->mCrc, &crc, sizeof( nvs->mCrc ) );
}

//////////////////////////////////////////////////////////////////////////
/// Sets the IP address to a TV Sat device
///
/// @param dev description structure of a tv sat device
/// @param ip the new ip address
/// @param mask the new network mask
/// @return 0, if everything went well
/// @return 1, otherwise
//////////////////////////////////////////////////////////////////////////
int setDeviceIP( const STVSatDev &dev, const uint8_t *ip,
		const uint8_t *mask )
{
	timeval stv;
	uint8_t buf[ ETHER_MAX_LEN ];
	ResponseHeader *rh;

	// create a packet structure for a unicast embedded in a broadcast
	uint8_t bcast_buf[ IP_MAXPACKET ];
	uint8_t *bcast_pkt = bcast_buf + sizeof( RequestBroadcastCmd );
	RequestBroadcastCmd *rqbc = (RequestBroadcastCmd *)bcast_buf;
	memset( bcast_buf, 0, IP_MAXPACKET );
	memcpy( rqbc->mDstMac, dev.dev_mac, sizeof( rqbc->mDstMac ) );
	rqbc->mHeader.mCommand = htons( cCmdBroadcastCmd );

	CUDPSocket sock;
	CRawSocket rsock = CRawSocket( AF_INET, ETH_P_IP, IPPROTO_UDP );
	sock.open( 0 );

	// construct a connect request and embed it into a broadcast
	RequestConnect *rqc = (RequestConnect *)bcast_pkt;
	memset( rqc, 0, sizeof( RequestConnect ) );
	rqc->mConnectTimeout = htons( 30 );
	rqc->mHeader.mCommand = htons( cCmdConnect );
	rqc->mHeader.mSize = htons( sizeof( RequestConnect ) );
	rqbc->mHeader.mSize = htons( sizeof( RequestConnect )
			+ sizeof( RequestBroadcastCmd ) );

	// send the connect request
	sock.send( bcast_buf, ntohs( rqbc->mHeader.mSize ),
			dev.net_if.if_bcast, 11111 );

	// open a raw socket that listens on the other socket's port
	rsock.open( sock.getPort(), dev.net_if.if_name.c_str() );

	gettimeofday( &stv, 0 );

	// wait for a connect response
	while( 1 ) {
		if( expired( stv ) ) {
			std::cerr << "Error: connection failed"
					<< std::endl;
			return -EFAULT;
		}

		rh = receiveRaw( rsock, buf, ETHER_MAX_LEN,
				sizeof( ResponseConnect ), cCmdConnect );

		if( rh )
			break;
	}

	// construct an EEPROM read request and embed it into a broadcast
	RequestNvsEepromRead *rqner = (RequestNvsEepromRead *)bcast_pkt;
	memset( rqner, 0, sizeof( RequestNvsEepromRead ) );
	rqner->mAddr = htonl( cUserDataAddr );
	rqner->mLen = htonl( sizeof( NvsUserData ) );
	rqner->mHeader.mCommand = htons( cCmdNvsEepromRead );
	rqner->mHeader.mSize = htons( sizeof( RequestNvsEepromRead ) );
	rqbc->mHeader.mSize = htons( sizeof( RequestNvsEepromRead )
			+ sizeof( RequestBroadcastCmd ) );

	// send the EEPROM read request
	sock.send( bcast_buf, ntohs( rqbc->mHeader.mSize ),
			dev.net_if.if_bcast, 11111 );

	gettimeofday( &stv, 0 );

	// wait for an EEPROM read response
	while( 1 ) {
		if( expired( stv ) ) {
			std::cerr << "Error: config read failed"
					<< std::endl;
			return -EFAULT;
		}

		rh = receiveRaw( rsock, buf, ETHER_MAX_LEN,
				sizeof( ResponseNvsEepromRead ),
				cCmdNvsEepromRead );

		if( rh )
			break;
	}

	ResponseNvsEepromRead *rner = (ResponseNvsEepromRead *)rh;

	// construct an EEPROM write request and embed it into a broadcast
	// copy the user data from the EEPROM read response and change
	// ip address and network mask
	RequestNvsEepromWrite *rqnew = (RequestNvsEepromWrite *)bcast_pkt;
	memset( rqnew, 0, sizeof( RequestNvsEepromWrite ) );
	rqnew->mAddr = htonl( cUserDataAddr );
	rqnew->mLen = htonl( sizeof( NvsUserData ) );
	memcpy( rqnew->mData, rner->mData, sizeof( rqnew->mData ) );
	NvsUserData *data = (NvsUserData *)rqnew->mData;
	memcpy( data->mFip, ip, sizeof( data->mFip ) );
	memcpy( data->mMsk, mask, sizeof( data->mMsk ) );
	data->mFlg[ 0 ] &= (~1);
	calcCRC( data );
	rqnew->mHeader.mCommand = htons( cCmdNvsEepromWrite );
	rqnew->mHeader.mSize = htons( sizeof( RequestNvsEepromWrite ) );
	rqbc->mHeader.mSize = htons( sizeof( RequestNvsEepromWrite )
			+ sizeof( RequestBroadcastCmd ) );

	// send the EEPROM write request
	sock.send( bcast_buf, ntohs( rqbc->mHeader.mSize ),
			dev.net_if.if_bcast, 11111 );

	gettimeofday( &stv, 0 );

	// wait for an EEPROM write response
	while( 1 ) {
		if( expired( stv ) ) {
			std::cerr << "Error: config write failed"
					<< std::endl;
			return -EFAULT;
		}

		rh = receiveRaw( rsock, buf, ETHER_MAX_LEN,
				sizeof( ResponseNvsEepromWrite ),
				cCmdNvsEepromWrite );

		if( rh )
			break;
	}

	// change the command of the EEPROM write request to EEPROM verify
	RequestNvsEepromVerify *rqnev =
			(RequestNvsEepromVerify *)bcast_pkt;
	rqnev->mHeader.mCommand = htons( cCmdNvsEepromVerify );

	// send the EEPROM verify request
	sock.send( bcast_buf, ntohs( rqbc->mHeader.mSize ),
			dev.net_if.if_bcast, 11111 );

	gettimeofday( &stv, 0 );

	// wait for an EEPROM verify response
	while( 1 ) {
		if( expired( stv ) ) {
			std::cerr << "Error: verification failed"
					<< std::endl;
			return -EFAULT;
		}

		rh = receiveRaw( rsock, buf, ETHER_MAX_LEN,
				sizeof( ResponseNvsEepromVerify ),
				cCmdNvsEepromVerify );

		if( rh )
			break;
	}

	// construct a reboot request and embed it into a broadcast
	RequestReboot *rqr = (RequestReboot *)bcast_pkt;
	rqr->mHeader.mCommand = htons( cCmdReboot );

	// send the reboot request
	sock.send( bcast_buf, ntohs( rqbc->mHeader.mSize ),
			dev.net_if.if_bcast, 11111 );

	gettimeofday( &stv, 0 );

	// wait for a reboot response
	while( 1 ) {
		if( expired( stv ) ) {
			std::cerr << "Error: reboot failed" << std::endl;
			return -EFAULT;
		}

		rh = receiveRaw( rsock, buf, ETHER_MAX_LEN,
				sizeof( ResponseDisconnect ),
				cCmdDisconnect );

		if( rh )
			break;
	}

	// close both sockets
	rsock.close();
	sock.close();

	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// tvsatcfg main function
///
/// reads the command line parameters and acts accordingly
//////////////////////////////////////////////////////////////////////////
int main( int argc, char ** argv )
{
	std::list< STVSatDev > devs;
	int c, optidx;
	int opt_all = 0, opt_dev_set = 0, opt_devip_set = 0,
			opt_if_set = 0, opt_devmask_set = 0,
			opt_devip_dhcp = 0;
	uint8_t opt_dev[ 6 ], opt_devip[ 4 ],
			opt_devmask[ 4 ] = { 255, 255, 255, 0 };
	std::string opt_if;

	// parse the command line options
	while( (c = getopt_long( argc, argv, "ab:d:hi:m:", long_opts,
			&optidx )) != -1 ) {
		switch( c ) {
		case 'a':
			if( getuid() != 0 ) {
				std::cerr << "Error: you need to have "
						"root privileges in "
						"order to use the "
						"-a/--all option"
						<< std::endl;
				exit( 1 );
			}

			opt_all = 1;
			break;
		case 'b':
			opt_if = optarg;
			opt_if_set = 1;
			break;
		case 'd':
			if( parseMAC( opt_dev, optarg ) ) {
				std::cerr << "Error: bad MAC address" <<
						std::endl;
				exit( 2 );
			}

			opt_dev_set = 1;
			break;
		case 'h':
			std::cout << HELPTXT;
			exit( 0 );
		case 'i':
			if( getuid() != 0 ) {
				std::cerr << "Error: you need to have "
						"root privileges in "
						"order to use the "
						"-i/--ipaddr option"
						<< std::endl;
				exit( 1 );
			}

			if( strncmp( optarg, "dhcp", 4 ) == 0 &&
			 		strlen( optarg ) == 4 )
				opt_devip_dhcp = 1;
			else if( parseIP( opt_devip, optarg ) ) {
				std::cerr << "Error: bad IP address" <<
						std::endl;
				exit( 3 );
			}

			opt_devip_set = 1;
			break;
		case 'm':
			if( parseIP( opt_devmask, optarg ) ) {
				std::cerr << "Error: bad network mask" <<
						std::endl;
				exit( 7 );
			}

			opt_devmask_set = 1;
			break;
		default:
			exit( 4 );
		}
	}

	// issue some warnings
	if( opt_devmask_set && !opt_devip_set )
		std::cout << "Warning: specifying -n/--netmask has no "
				"effect without -i/--ipaddr" << std::endl;

	if( opt_devmask_set && opt_devip_dhcp )
		std::cout << "Warning: specifying -n/--netmask has no "
				"effect with -i/--ipaddr dhcp"
				<< std::endl;

	if( opt_dev_set && !opt_devip_set )
		std::cout << "Warning: specifying -d/--device has no "
				"effect without -i/--ipaddr" << std::endl;

	// ip address change
	if( opt_devip_set ) {
		if( !opt_dev_set ) {
			std::cerr << "Error: no device specified" <<
					std::endl;
			exit( 5 );
		}

		std::cout << "Looking for device " <<
				HEXFMT << (int)opt_dev[ 0 ] << ":" <<
				HEXFMT << (int)opt_dev[ 1 ] << ":" <<
				HEXFMT << (int)opt_dev[ 2 ] << ":" <<
				HEXFMT << (int)opt_dev[ 3 ] << ":" <<
				HEXFMT << (int)opt_dev[ 4 ] << ":" <<
				HEXFMT << (int)opt_dev[ 5 ] << std::endl;

		// find the specified device
		findDevices( devs, opt_if, true );

		bool found = false;
		STVSatDev found_dev;

		for( std::list< STVSatDev >::iterator it = devs.begin();
			it != devs.end(); ++it ) {
			if( memcmp( it->dev_mac, opt_dev, 6 ) == 0 ) {
				found = true;
				found_dev = *it;
				break;
			}
		}

		if( !found ) {
			std::cerr << "Error: unable to find device" <<
					std::endl;
			exit( 6 );
		}

		// if the TV Sat should use DHCP, set ip to 0.0.0.0 and
		// mask to 255.255.255.255
		if( opt_devip_dhcp ) {
			memset( opt_devip, 0, sizeof( opt_devip ) );
			memset( opt_devmask, 255, sizeof( opt_devmask ) );
		}

		std::cout << "Changing IP settings..." << std::endl;

		if( setDeviceIP( found_dev, opt_devip, opt_devmask ) == 0 )
			std::cout << "IP settings successfully changed"
				<< std::endl;
		else
			std::cout << "Failed to change IP settings"
				<< std::endl;

		return 0;
	}

	if( opt_if_set )
		std::cout << "Looking for TV Sat devices on " << opt_if <<
				std::endl;
	else
		std::cout << "Looking for TV Sat devices on all "
				"interfaces..." << std::endl;

	// discover TV Sat devices and print the retrieved information to
	// the console
	findDevices( devs, opt_if, (bool)opt_all );

	for( std::list< STVSatDev >::iterator it = devs.begin();
			it != devs.end(); ++it ) {
		std::cout << "Found device: " << std::endl;
		std::cout << "    MAC address:      " <<
				HEXFMT << (int)it->dev_mac[ 0 ] << ":" <<
				HEXFMT << (int)it->dev_mac[ 1 ] << ":" <<
				HEXFMT << (int)it->dev_mac[ 2 ] << ":" <<
				HEXFMT << (int)it->dev_mac[ 3 ] << ":" <<
				HEXFMT << (int)it->dev_mac[ 4 ] << ":" <<
				HEXFMT << (int)it->dev_mac[ 5 ] <<
				std::endl;
		std::cout << "    IP address:       " << it->dev_ip <<
				std::endl;
		std::cout << "    local interface:  "
				<< it->net_if.if_name << std::endl;
		std::cout << "    local IP address: "
				<< it->net_if.if_ip << std::endl;
		std::cout << std::endl;
	}

	std::cout << devs.size() << " device" <<
			((devs.size() == 1) ? "" : "s") << " found" <<
			std::endl;

	if( devs.size() > 8 ) {
		std::cout << "WARNING: the DVB-API supports a maximum of "
				"8 DVB adapters" << std::endl;
		std::cout << "         you will not be able to use all "
				"of your devices" << std::endl;
	}

	return 0;
}
