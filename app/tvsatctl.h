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
/// @file tvsatctl.h
/// @brief "dLAN TV Sat Control" - header
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#ifndef __TVSATCTL_H
#define __TVSATCTL_H

#include "../include/tvsat.h"
#include "streamin.h"

//////////////////////////////////////////////////////////////////////////
// DEFINITIONS
//////////////////////////////////////////////////////////////////////////
#define TVSAT_DEV_CONTROL_DEVICE_NAME  "/dev/" TVSAT_CONTROL_DEVICE_NAME

//////////////////////////////////////////////////////////////////////////
/// dLAN TV Sat Control
//////////////////////////////////////////////////////////////////////////
class CTVSatCtl {
public:
    CTVSatCtl(std::string &client_ip, std::string &device_ip, uint8_t *device_mac, int adapter_num, bool verbose);

    ~CTVSatCtl();

    const std::string &getTVSatIP() { return m_ip_addr; }

    const uint8_t *getTVSatMAC() { return m_mac_addr; }

    void run();

    void runThreaded();

    void stop();

    static int sleepMS(double ms);

private:
    CTVSatCtl() {};

    void handleExitSignal(int signal);

    void selectPID(const tvsat_pid_selection *pid);

    void tune(const tvsat_tuning_parameters *tune);

    static void *startThread(void *tvsat_ctl);

    tvsat_dev_id m_dev_id;
    int m_init;
    int m_input_dev;
    std::string m_ip_addr;
    int m_is_tuned;
    uint8_t m_mac_addr[6];
    int m_run;
    pthread_mutex_t m_run_access;
    CTVSatStreamIn *m_sin;
    pthread_t m_thread;
    bool m_verbose;
};

#endif
