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
/// @file config.cpp
/// @brief "dLAN TV Sat Config" - implementation
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdio>

#include "config.h"

//////////////////////////////////////////////////////////////////////////
/// Compiles the regular expressions
/// @param regex a set of (not yet) compiled regular expressions
//////////////////////////////////////////////////////////////////////////
static void initRegex( config_regex_t *regex )
{
	regcomp( &regex->broadcast_interval,
			"(^[ \t]*)"
			"(broadcast_interval)"
			"([ \t]*)"
			"(=)"
			"([ \t]*)"
			"([0-9]{1,4})"
			"([ \t]*(#.*){0,1}$)",
			REG_NEWLINE | REG_EXTENDED );
	regcomp( &regex->device_map_ip,
			"(^[ \t]*)"
			"(ip_)"
			"("
			"([0-9]{1,3}\\.){3}"
			"([0-9]{1,3})"
			")"
			"([ \t]*)"
			"(=)"
			"([ \t]*)"
			"([0-9]{1,3})"
			"([ \t]*(#.*){0,1}$)",
			REG_NEWLINE | REG_EXTENDED );
	regcomp( &regex->device_map_mac,
			"(^[ \t]*)"
			"(mac_)"
			"("
			"([0-9abcdefABCDEF]{2}:){5}"
			"([0-9abcdefABCDEF]{2})"
			")"
			"([ \t]*)"
			"(=)"
			"([ \t]*)"
			"([0-9]{1,3})"
			"([ \t]*(#.*){0,1}$)",
			REG_NEWLINE | REG_EXTENDED );
	regcomp( &regex->interface,
			"(^[ \t]*)"
			"(interface)"
			"([ \t]*)"
			"(=)"
			"([ \t]*)"
			"([^ \t#]*)"
			"([ \t]*(#.*){0,1}$)",
			REG_NEWLINE | REG_EXTENDED );
}

//////////////////////////////////////////////////////////////////////////
/// Frees the compiled regular expressions
/// @param regex a set of compiled regular expressions
//////////////////////////////////////////////////////////////////////////
static void freeRegex( config_regex_t *regex )
{
	regfree( &regex->broadcast_interval );
	regfree( &regex->device_map_ip );
	regfree( &regex->device_map_mac );
	regfree( &regex->interface );
}

//////////////////////////////////////////////////////////////////////////
/// Copies a matched subexpression from an input string to a buffer
/// @param in_buf the string that contains the matched subexpression
/// @param match subexpression matches from a regexec() call
/// @param out_buf the buffer where the substring should be copied to
/// @param out_buf_len the size of the output buffer
//////////////////////////////////////////////////////////////////////////
static bool copyFromMatch( const char *in_buf, const regmatch_t *match,
		char *out_buf, int out_buf_len )
{
	int len = match->rm_eo - match->rm_so;

	if( len >= out_buf_len )
		return false;

	strncpy( out_buf, in_buf + match->rm_so, len );
	out_buf[ len ] = '\0';

	return true;
}

//////////////////////////////////////////////////////////////////////////
/// Parses one configuration entry
/// @param config a set of config options
/// @param regex a set of compiled regular expressions
/// @param line line from a config file
//////////////////////////////////////////////////////////////////////////
static void parseConfigLine( config_t *config, config_regex_t *regex,
		const char *line )
{
	const int buf_len = 1024;
	regmatch_t match[ 20 ];
	char buf[ buf_len ];

	if( regexec( &regex->broadcast_interval, line, 20, match, 0 )
			== 0 )
		if( copyFromMatch( line, &match[ 6 ], buf, buf_len ) )
		{
			int bcast_int = atoi( buf );

			if( (bcast_int >= 5) && (bcast_int <= 1000) )
				config->broadcast_interval = bcast_int;
		}

	if( (regexec( &regex->device_map_ip, line, 20, match, 0 ) == 0) ||
			(regexec( &regex->device_map_mac, line, 20, match,
			0 ) == 0) )
		if( copyFromMatch( line, &match[ 3 ], buf, buf_len ) )
		{
			std::string ipmac = buf;

			if( copyFromMatch( line, &match[ 9 ], buf,
					buf_len ) )
			{
				int adapter_num = atoi( buf );

				if( (adapter_num >= 0) &&
						(adapter_num < 8) )
					config->device_map[ ipmac ] =
							adapter_num;
			}
		}

	if( regexec( &regex->interface, line, 20, match, 0 ) == 0 )
		if( copyFromMatch( line, &match[ 6 ], buf, buf_len ) )
			config->interface = buf;
}

//////////////////////////////////////////////////////////////////////////
/// Loads a configuration file
/// @param config a set of config options
/// @param filename configuration file
//////////////////////////////////////////////////////////////////////////
void loadConfig( config_t *config, const char *filename )
{
	FILE *file = fopen( filename, "r" );

	if( !file )
		return;

	config_regex_t regex;

	initRegex( &regex );

	char buf[ 1024 ];

	while( fgets( buf, 1024, file ) )
		parseConfigLine( config, &regex, buf );

	freeRegex( &regex );

	fclose( file );
}

//////////////////////////////////////////////////////////////////////////
/// Sets the default values to a configuration structure
/// @param config a set of config options
//////////////////////////////////////////////////////////////////////////
void defaultConfig( config_t *config )
{
	config->broadcast_interval = 10;
	config->device_map.clear();
}

