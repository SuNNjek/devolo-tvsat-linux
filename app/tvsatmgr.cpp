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
/// @file tvsatmgr.cpp
/// @brief "dLAN TV Sat Manager" - implementation
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <list>
#include <net/if.h>
#include <netinet/ip.h>
#include <utility>
#include <signal.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "config.h"
#include "discover.h"
#include "log.h"
#include "tvsatctl.h"
#include "tvsatmgr.h"
#include "udpsocket.h"

//////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
//////////////////////////////////////////////////////////////////////////
bool stop = false;

//////////////////////////////////////////////////////////////////////////
/// Prepares and forks a new background process and terminates the
/// original one
//////////////////////////////////////////////////////////////////////////
static void daemonize() {
    pid_t fpid = fork();

    if (fpid < 0)
        exit(1);

    if (fpid > 0)
        exit(0);

    umask(0);

    pid_t sid = setsid();

    if (sid < 0)
        exit(1);

    if (chdir("/") < 0)
        exit(1);

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    FILE *f = fopen(TVSAT_PID_FILE, "w");

    if (f) {
        fprintf(f, "%u", getpid());
        fclose(f);
    } else
        exit(1);
}

//////////////////////////////////////////////////////////////////////////
/// Tells the controller to stop after the next iteration of its main loop
/// @param signum signal (not used)
//////////////////////////////////////////////////////////////////////////
static void handleExitSignal(int signum) {
    stop = true;
}

//////////////////////////////////////////////////////////////////////////
/// Updates the lists of new and missing devices
//////////////////////////////////////////////////////////////////////////
static void updateDeviceLists(std::list<STVSatDev> &fdevs,
                              std::list<STVSatDev> &ndevs,
                              std::map<STVSatDev, int> &mdevs,
                              std::list<CTVSatCtl *> &ctls,
                              const config_t &cfg) {
    std::list<STVSatDev>::iterator fd_it, nd_it;
    std::map<STVSatDev, int>::iterator md_it;
    std::list<CTVSatCtl *>::iterator c_it;
    std::map<std::string, uint8_t>::const_iterator dm_it, dm_it2;

    // remove discovered devices from the list of missing devices
    for (fd_it = fdevs.begin(); fd_it != fdevs.end(); ++fd_it) {
        md_it = mdevs.find(*fd_it);

        if (md_it != mdevs.end())
            mdevs.erase(md_it);
    }

    // add all discovered devices that aren't already registered to
    // the list of new devices
    for (fd_it = fdevs.begin(); fd_it != fdevs.end(); ++fd_it) {
        bool found = false;

        for (c_it = ctls.begin(); c_it != ctls.end(); ++c_it)
            if (memcmp(fd_it->dev_mac, (*c_it)->getTVSatMAC(),
                       6) == 0)
                found = true;

        if (!found)
            ndevs.insert(ndevs.end(), *fd_it);
    }

    // register the new devices
    for (nd_it = ndevs.begin(); nd_it != ndevs.end(); ++nd_it) {
        logInf("Adding device at %s", nd_it->dev_ip.c_str());

        int adapter_num = -1;

        dm_it = cfg.device_map.find(nd_it->dev_ip);

        if (dm_it != cfg.device_map.end())
            adapter_num = dm_it->second;

        for (dm_it2 = cfg.device_map.begin();
             dm_it2 != cfg.device_map.end();
             ++dm_it2) {
            uint8_t mac[6];

            if (parseMAC(mac, dm_it2->first.c_str()) != 0)
                continue;

            if (memcmp(mac, nd_it->dev_mac, 6) == 0) {
                adapter_num = dm_it2->second;
                break;
            }
        }

        int i = 0;

        while (adapter_num == -1) {
            bool found = false;

            for (dm_it = cfg.device_map.begin();
                 dm_it != cfg.device_map.end();
                 ++dm_it)
                if (dm_it->second == i) {
                    found = true;
                    break;
                }

            if (!found)
                adapter_num = i;

            ++i;
        }

        CTVSatCtl *ctl = new CTVSatCtl(nd_it->net_if.if_ip,
                                       nd_it->dev_ip, nd_it->dev_mac,
                                       adapter_num);
        ctls.insert(ctls.end(), ctl);
        ctl->runThreaded();
    }

    // add registered devices that were not discovered to the list of
    // missing devices
    // unregister devices that were reported missing three times in a
    // row

    c_it = ctls.begin();

    while (c_it != ctls.end()) {
        bool found = false;

        for (fd_it = fdevs.begin(); fd_it != fdevs.end();
             ++fd_it) {
            if (memcmp(fd_it->dev_mac,
                       (*c_it)->getTVSatMAC(), 6)
                == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            bool found2 = false;

            for (md_it = mdevs.begin(); md_it != mdevs.end();
                 ++md_it) {
                if (memcmp(md_it->first.dev_mac,
                           (*c_it)->getTVSatMAC(),
                           6) == 0) {
                    found2 = true;
                    break;
                }
            }

            if (!found2) {
                STVSatDev mdev;
                memcpy(mdev.dev_mac,
                       (*c_it)->getTVSatMAC(),
                       6);
                mdevs[mdev] = 0;
                ++c_it;
            } else if (md_it->second < 3) {
                ++md_it->second;
                ++c_it;
            } else {
                logInf("Removing device at %s",
                       (*c_it)->getTVSatIP().c_str());

                (*c_it)->stop();
                delete *c_it;
                c_it = ctls.erase(c_it);
            }
        } else
            ++c_it;
    }
}

//////////////////////////////////////////////////////////////////////////
/// Registers the signal handlers, initializes and starts the TV Sat
/// controller
///
/// @param argc number of arguments
/// @param argv command line parameters
//////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) {
    // register the signal handler that stops the main loop
    signal(SIGTERM, handleExitSignal);
    signal(SIGQUIT, handleExitSignal);
    signal(SIGHUP, handleExitSignal);
    signal(SIGINT, handleExitSignal);

    // if we should start as a daemon, fork to background and create a
    // pid file
    if (argc > 1)
        if (strcmp(argv[1], "-d") == 0)
            daemonize();

    openlog("tvsatd", 0, LOG_ERR);

    config_t config;
    defaultConfig(&config);
    loadConfig(&config, "/etc/tvsatd/tvsatd.conf");

    logInf("dLAN TV Sat Controller started");

    std::list<STVSatDev> found_devs;
    std::map<STVSatDev, int> missing_devs;
    std::list<STVSatDev> new_devs;
    std::list<CTVSatCtl *> tvsat_ctls;
    timespec sleep_time;

    sleep_time.tv_sec = config.broadcast_interval;
    sleep_time.tv_nsec = 0;

    // main loop of the management thread
    while (!stop) {
        timeval tv1, tv2;

        gettimeofday(&tv1, 0);
        found_devs.clear();
        new_devs.clear();

        findDevices(found_devs, config.interface);
        updateDeviceLists(found_devs, new_devs, missing_devs,
                          tvsat_ctls, config);
        gettimeofday(&tv2, 0);

        // check for new devices only every few seconds and sleep
        // in between
        // use nanosleep to be interruptible, but resume sleeping
        // if 'stop' isn't set
        int tdiff = tv2.tv_sec - tv1.tv_sec;
        timespec orm, rm;
        rm = sleep_time;

        if (rm.tv_sec < tdiff)
            rm.tv_sec = 0;
        else
            rm.tv_sec -= tdiff;

        while (((rm.tv_nsec > 0) || (rm.tv_sec > 0)) && !stop) {
            orm = rm;

            if (nanosleep(&orm, &rm) == 0)
                break;

            if ((orm.tv_nsec == rm.tv_nsec) &&
                (orm.tv_sec == rm.tv_sec))
                break;
        }
    }

    // stop all running threads
    std::list<CTVSatCtl *>::iterator c_it;

    for (c_it = tvsat_ctls.begin();
         c_it != tvsat_ctls.end(); ++c_it) {
        (*c_it)->stop();
        delete *c_it;
    }

    logInf("dLAN TV Sat Controller terminated");
    closelog();

    return 0;
}
