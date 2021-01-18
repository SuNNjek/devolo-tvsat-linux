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
/// @file config.h
/// @brief "dLAN TV Sat Config" - header
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#ifndef __TVSAT_CONFIG_H
#define __TVSAT_CONFIG_H

#include <map>
#include <regex.h>
#include <string>
#include <stdint.h>

//////////////////////////////////////////////////////////////////////////
/// Configuration options
//////////////////////////////////////////////////////////////////////////
struct config_t {
    uint16_t broadcast_interval;
    std::map<std::string, uint8_t> device_map;
    std::string interface;
};

//////////////////////////////////////////////////////////////////////////
/// Regular expressions that match the configuration options
//////////////////////////////////////////////////////////////////////////
struct config_regex_t {
    regex_t broadcast_interval;
    regex_t device_map_ip;
    regex_t device_map_mac;
    regex_t interface;
};

void defaultConfig(config_t *config);

void loadConfig(config_t *config, const char *filename);

#endif
