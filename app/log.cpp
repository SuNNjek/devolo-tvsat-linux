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
/// @file log.cpp
/// @brief "dLAN TV Sat Logging" - implementation
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <stdarg.h>

#include "log.h"

//////////////////////////////////////////////////////////////////////////
/// Send a debug message to the syslog
//////////////////////////////////////////////////////////////////////////
void logDbg(const char *fmt, ...) {
    va_list va;

    va_start(va, fmt);
    vsyslog(LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG), fmt, va);
    va_end(va);
}

//////////////////////////////////////////////////////////////////////////
/// Send an error message to the syslog
//////////////////////////////////////////////////////////////////////////
void logErr(const char *fmt, ...) {
    va_list va;

    va_start(va, fmt);
    vsyslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), fmt, va);
    va_end(va);
}

//////////////////////////////////////////////////////////////////////////
/// Send an info message to the syslog
//////////////////////////////////////////////////////////////////////////
void logInf(const char *fmt, ...) {
    va_list va;

    va_start(va, fmt);
    vsyslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO), fmt, va);
    va_end(va);
}
