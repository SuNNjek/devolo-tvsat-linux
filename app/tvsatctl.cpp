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
/// @file tvsatctl.cpp
/// @brief "dLAN TV Sat Control" - implementation
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include "log.h"
#include "tvsatctl.h"

//////////////////////////////////////////////////////////////////////////
/// Constructor
//////////////////////////////////////////////////////////////////////////
CTVSatCtl::CTVSatCtl(std::string &client_ip, std::string &device_ip, uint8_t *device_mac, int adapter_num, bool verbose) {
    m_verbose = verbose;
    m_init = 1;
    m_is_tuned = 0;
    m_run = 1;
    m_sin = new CTVSatStreamIn(verbose);

    m_ip_addr = device_ip;
    memcpy(m_mac_addr, device_mac, 6);

    // convert ip addresses from strings to four-byte arrays
    uint8_t *cip = new uint8_t[4];
    uint8_t *dip = new uint8_t[4];
    in_addr cia, dia;

    inet_pton(AF_INET, client_ip.c_str(), &cia);
    inet_pton(AF_INET, device_ip.c_str(), &dia);

    for (int i = 0; i < 4; ++i) {
        cip[i] = (cia.s_addr & (0xff << (i * 8))) >> (i * 8);
        dip[i] = (dia.s_addr & (0xff << (i * 8))) >> (i * 8);
    }

    m_sin->setClientIP(cip);
    m_sin->setTVSatIP(dip);

    // register the device with the kernel module
    memset(&m_dev_id, 0, sizeof(tvsat_dev_id));
    m_dev_id.adapter = adapter_num;
    memcpy(m_dev_id.ip_addr, dip, 4);
    m_dev_id.port = 11111;
    m_dev_id.minor = 0;

    int ctl_dev = open(TVSAT_DEV_CONTROL_DEVICE_NAME, O_RDWR);
    int ret = ioctl(ctl_dev, TVS_REGISTER_DEVICE, &m_dev_id);
    close(ctl_dev);

    // give udev some time to create the device node
    sleep(1);

    // if the kernel module signals success, open the newly created input device
    if ((ret == 0) && (m_dev_id.minor > 0)) {
        m_sin->setClientPort(11110 + m_dev_id.minor);

        char dev_name[12];
        snprintf(dev_name, 12, "/dev/tvs%u", m_dev_id.minor - 1);

        m_input_dev = open(dev_name, O_RDWR);
        m_sin->setInputDev(m_input_dev);

        if (m_verbose)
            LOG_DBG(m_verbose, "successfully opened input device %s", dev_name);
    } else
        m_init = 0;

    delete[] cip;
    delete[] dip;

    pthread_mutex_init(&m_run_access, 0);
}

//////////////////////////////////////////////////////////////////////////
/// Destructor
//////////////////////////////////////////////////////////////////////////
CTVSatCtl::~CTVSatCtl() {
    // unregister the device from the kernel module
    int ctl_dev = open(TVSAT_DEV_CONTROL_DEVICE_NAME, O_RDWR);
    ioctl(ctl_dev, TVS_UNREGISTER_DEVICE, &m_dev_id);
    close(ctl_dev);

    delete m_sin;

    if (m_input_dev >= 0)
        close(m_input_dev);
}

//////////////////////////////////////////////////////////////////////////
/// Starts the main event loop
//////////////////////////////////////////////////////////////////////////
void CTVSatCtl::run() {
    int lc = 0;
    int run = 1;

    // stop immediately if the constructor failed
    if (!m_init) {
        logErr("ERROR: Failed to initialize controller. Is the tvsat kernel module loaded?");
        return;
    }

    m_sin->startReceiver();

    // main loop
    while (run) {
        tvsat_event ev;
        int ret;

        // don't poll for events each time around
        if (lc == 5) {
            lc = 0;

            // retrieve and process all pending messages from the tvsat kernel module
            do {
                ret = ioctl(m_input_dev, TVS_GET_EVENT, &ev);

                if (ret == 0) {
                    LOG_DBG(m_verbose, "Received event from the kernel:");

                    switch (ev.type) {
                        case TVSAT_EVENT_PID:
                            LOG_DBG(m_verbose, "start/stop pid");
                            selectPID(&ev.event.pid);
                            break;
                        case TVSAT_EVENT_TUNE:
                            LOG_DBG(m_verbose, "tune");
                            tune(&ev.event.tune);
                            break;
                        case TVSAT_EVENT_CONNECT:
                            LOG_DBG(m_verbose, "connect");
                            m_sin->connect();
                            break;
                        case TVSAT_EVENT_DISCONNECT:
                            LOG_DBG(m_verbose, "disconnect");
                            m_sin->disconnect();
                            m_sin->stop();
                            break;
                        default:
                            LOG_DBG(m_verbose, "unknown event\n");
                    }
                }
            } while (ret == 0);
        }

        // report lock to the kernel module
        if (m_sin->isTuned() && !m_is_tuned) {
            ioctl(m_input_dev, TVS_HAS_LOCK);
            m_is_tuned = 1;
        } else if (!m_sin->isTuned() && m_is_tuned)
            m_is_tuned = 0;

        // trigger the stream input state machine
        m_sin->delPIDs();
        m_sin->tick();

        // we don't need to do this all the time, so we just sleep for a while
        sleepMS(25);
        ++lc;

        pthread_mutex_lock(&m_run_access);
        run = m_run;
        pthread_mutex_unlock(&m_run_access);
    }

    m_sin->stop();
}

//////////////////////////////////////////////////////////////////////////
/// Starts the main event loop in a separate thread
//////////////////////////////////////////////////////////////////////////
void CTVSatCtl::runThreaded() {
    pthread_create(&m_thread, 0, startThread, (void *) this);
}

//////////////////////////////////////////////////////////////////////////
/// Processes a PID selection event from the kernel
//////////////////////////////////////////////////////////////////////////
void CTVSatCtl::selectPID(const tvsat_pid_selection *pid_sel) {
    if (pid_sel->action == PID_START)
        m_sin->addPID(pid_sel->pid);

    if (pid_sel->action == PID_STOP)
        m_sin->delPID(pid_sel->pid);
}

//////////////////////////////////////////////////////////////////////////
/// Sleeps for the given amount of time
///
/// Although it is based on nanosleep, this function is designed to be
/// non-interruptible
///
/// @param ms the amount of time to sleep in milliseconds
//////////////////////////////////////////////////////////////////////////
int CTVSatCtl::sleepMS(double ms) {
    timespec rm = {0, 0}, orm = {0, 0};
    rm.tv_nsec = (int) (ms * 1000000);

    // nanosleep can be interrupted but we really want to sleep for the requested number of milliseconds
    do {
        orm = rm;

        if (nanosleep(&orm, &rm) == 0)
            return 0;

        if (orm.tv_nsec == rm.tv_nsec)
            return -EFAULT;

    } while (rm.tv_nsec > 0);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Entry point for a new thread that runs the event loop of a CTVSatCtl
/// instance
/// @param tvsat_ctl an instance of CTVSatCtl
//////////////////////////////////////////////////////////////////////////
void *CTVSatCtl::startThread(void *tvsat_ctl) {
    ((CTVSatCtl *) tvsat_ctl)->run();
    pthread_exit(0);
}

//////////////////////////////////////////////////////////////////////////
/// Stops the event loop
//////////////////////////////////////////////////////////////////////////
void CTVSatCtl::stop() {
    pthread_mutex_lock(&m_run_access);
    m_run = 0;
    pthread_mutex_unlock(&m_run_access);

    pthread_join(m_thread, 0);
}

//////////////////////////////////////////////////////////////////////////
/// Processes a tuning event from the kernel
/// @param tune tuning parameters (frequency, polarization, etc.)
//////////////////////////////////////////////////////////////////////////
void CTVSatCtl::tune(const tvsat_tuning_parameters *tune) {
    LOG_DBG(m_verbose, "Type: %s, Freq: %u, Band: %s, Pol: %s, Sym: %u, Fec: %u, Modulation: %u, Pilot: %u, Roll off: %u\n",
           tune->delivery_system ? "DVB-S2" : "DVB-S",
           tune->frequency, tune->band ? "low" : "high", tune->polarization ? "horizontal" : "vertical",
           tune->symbol_rate, tune->fec,
           tune->modulation, tune->pilot, tune->roll_off);

    m_sin->setTuningParameters(tune);
    m_sin->start();
}
