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
/// @file streamin.cpp
/// @brief "dLAN TV Sat Stream Input" - implementation
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <iostream>
#include <netinet/ip.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>

#include "log.h"
#include "streamin.h"
#include "tvsatctl.h"

//////////////////////////////////////////////////////////////////////////
/// Constructor
///
/// Opens a listening socket for incoming stream data and sets the initial
/// state of the state machine to 'disconnected'
//////////////////////////////////////////////////////////////////////////
CTVSatStreamIn::CTVSatStreamIn() {
    m_do_connect = 0;
    m_do_tune = 0;
    m_is_tuned = 0;
    m_retry = 0;
    m_select_pids = 0;
    m_state = eDisconnected;
    m_stop = 0;
    m_stop_thread = 0;
    m_thread_started = 0;
    m_try_pilot1 = 0;
    m_tvsat_ip[0] = '\0';
    m_tune = 0;
    m_wait = 0;

    memset(m_client_ip, 0, 4);

    m_sock.open(0);

    pthread_mutex_init(&m_stop_access, 0);
}

//////////////////////////////////////////////////////////////////////////
/// Destructor
///
/// Stops the receiving thread and closes the socket.
//////////////////////////////////////////////////////////////////////////
CTVSatStreamIn::~CTVSatStreamIn() {
    pthread_mutex_lock(&m_stop_access);
    m_stop_thread = 1;
    m_sock.close();
    pthread_mutex_unlock(&m_stop_access);

    if (m_thread_started)
        pthread_join(m_thread, 0);
}

//////////////////////////////////////////////////////////////////////////
/// Adds a PID to the filter
/// @param pid a pid to add to the filter
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::addPID(uint16_t pid) {
    if (pid > 0x1fff)
        return;

    if (m_del_pids.erase(pid) == 0) {
        m_pids.insert(pid);
        m_select_pids = 1;
    }
}

//////////////////////////////////////////////////////////////////////////
/// Resets the connection to the NAT device
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::cleanUp() {
    logDbg("Resetting connection");

    m_sock.close();
    m_sock.open(0);
}

//////////////////////////////////////////////////////////////////////////
/// Marks a PID as deleted
///
/// The PID will be removed a few seconds after it was marked as deleted
/// unless it is added again during this waiting period
/// @param pid a pid to remove from the filter
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::delPID(uint16_t pid) {
    if (pid > 0x1fff)
        return;

    if (m_pids.find(pid) != m_pids.end()) {
        timeval tv;
        gettimeofday(&tv, 0);
        tv.tv_sec += 10;
        m_del_pids[pid] = tv;
    }
}

//////////////////////////////////////////////////////////////////////////
/// Checks the list of pids that are marked as deleted and removes every
/// pid whose time has expired
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::delPIDs() {
    if (!m_is_tuned)
        return;

    timeval t;
    gettimeofday(&t, 0);

    std::map<uint16_t, timeval>::iterator it = m_del_pids.begin();

    while (it != m_del_pids.end()) {
        if (tvlt(&it->second, &t)) {
            m_pids.erase(it->first);
            m_del_pids.erase(it);
            m_select_pids = 1;
            it = m_del_pids.begin();
        } else
            ++it;
    }
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a connect request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveConnectResponse() const {
    if (receiveResponse(cCmdConnect) != 0)
        return -1;

    logDbg("Received response to connect request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a disconnect request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveDisconnectResponse() const {
    if (receiveResponse(cCmdDisconnect) != 0)
        return -1;

    logDbg("Received response to disconnect request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a DiSEqC send burst request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveDiseqcSendBurstResponse() const {
    if (receiveResponse(cCmdFeDiseqcSendBurst) != 0)
        return -1;

    logDbg("Received response to DiSEqC send burst request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a DiSEqC send master command request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveDiseqcSendMasterCommandResponse() const {
    if (receiveResponse(cCmdFeDiseqcSendMasterCommand) != 0)
        return -1;

    logDbg("Received response to DiSEqC send master command request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a keepalive request
///
/// @return -1, if there was no response
/// @return  1, if there was a response, but no lock
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveKeepaliveResponse() const {
    int ret = receiveResponse(cCmdFeReadStatus);

    if (ret < 0)
        return -1;

    return (ret > 0);
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a prepare tone request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receivePrepareToneResponse() const {
    if (receiveResponse(cCmdFeSetTone) != 0)
        return -1;

    logDbg("Received response to prepare tone request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a reset filter request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveResetFilterResponse() const {
    if (receiveResponse(cCmdTseStart2) != 0)
        return -1;

    logDbg("Received response to reset filter request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a specific request
///
/// The receive call is non-blocking, meaning, if there is no packet in
/// the socket's buffer by the time Receive is called, the function fails.
///
/// @param cmd the command to look for in the response packet
/// @return -1, if there was no response to the specified command
/// @return -2, if the received packet was truncated
/// @return -3, if the device returned an error
/// @return  1, if the command was cCmdFeReadStatus and there was no
///             signal lock
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveResponse(uint16_t cmd) const {
    uint8_t buf[IP_MAXPACKET];
    int rbytes = m_sock.receive(buf, IP_MAXPACKET, false);

    if (rbytes < 6)
        return -1;

    ResponseHeader *rh = (ResponseHeader *) buf;

    // check for truncated packets
    if (rbytes < ntohs(rh->mSize)) {
        logErr("Received truncated packet");
        return -2;
    }

#if _DEBUG
    if( ntohs( rh->mCommand ) == cCmdFeReadStatus )
    {
      ResponseFeReadStatus *rfrs = ( ResponseFeReadStatus * )rh;
      uint16_t status = ntohs( rfrs->mData );

      if (status != 0)
        logDbg( "Current Status: %x", status );
    }
#endif

    // check if this is a response to the requested command
    if (ntohs(rh->mCommand) != cmd) {
        logDbg("Received response, but to some other command: 0x%x", ntohs(rh->mCommand));
        return -1;
    }

    // check if the packet contains an error code
    if (rh->mResult != 0) {
        logErr("Device returned code %i on command %x", ntohs(rh->mResult), ntohs(rh->mCommand));
        return -3;
    }

    // in the special case of a status response, check if the NAT device reports a signal lock
    if (cmd == cCmdFeReadStatus) {
        ResponseFeReadStatus *rfrs = (ResponseFeReadStatus *) rh;
        uint16_t status = ntohs(rfrs->mData);

        if ((status && 0x10))
            logDbg("Signal locked");
        else
            return 1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a set filter request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveSetFilterResponse() const {
    if (receiveResponse(cCmdTseStart2) != 0)
        return -1;

    logDbg("Received response to set filter request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a set frontend request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveSetFrontendResponse() const {
    if (receiveResponse(cCmdFeSetFrontend) != 0)
        return -1;

    logDbg("Received response to set frontend request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a set tone request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveSetToneResponse() const {
    if (receiveResponse(cCmdFeSetTone) != 0)
        return -1;

    logDbg("Received response to set tone request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to a set voltage request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveSetVoltageResponse() const {
    if (receiveResponse(cCmdFeSetVoltage) != 0)
        return -1;

    logDbg("Received response to set voltage request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to start request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveStartResponse() const {
    if (receiveResponse(cCmdStart) != 0)
        return -1;

    logDbg("Received response to start request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive a response to stop request
///
/// @return -1, if there was no response
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::receiveStopResponse() const {
    if (receiveResponse(cCmdStop) != 0)
        return -1;

    logDbg("Received response to stop request");

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Tries to receive all packets from the stream socket's buffer
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::receiveStreamData() {
    char rbuf[IP_MAXPACKET];
    int rbytes = m_stream_sock.receive((unsigned char *) rbuf, IP_MAXPACKET, false);

    if (rbytes > 0)
        write(m_input_dev, rbuf, rbytes);
}

//////////////////////////////////////////////////////////////////////////
/// Main loop of the receiver thread
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::receiverLoop() {
    while (1) {
        receiveStreamData();

        pthread_mutex_lock(&m_stop_access);

        if (m_stop_thread) {
            pthread_mutex_unlock(&m_stop_access);
            return;
        }

        pthread_mutex_unlock(&m_stop_access);
    }
}

//////////////////////////////////////////////////////////////////////////
/// Sends a connect request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendConnectRequest() const {
    if (m_client_ip[0] == 0)
        return -1;

    RequestConnect rc;
    memcpy(rc.mClientIpAddress, m_client_ip, 4);
    rc.mConnectTimeout = htons(30);
    rc.mHeader.mCommand = htons(cCmdConnect);
    rc.mHeader.mSize = htons(sizeof(RequestConnect));

    if (sendRequest((RequestHeader *) &rc) != 0) {
        logErr("Connect request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a disconnect request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendDisconnectRequest() const {
    if (m_client_ip[0] == 0)
        return -1;

    RequestDisconnect rd;
    rd.mHeader.mCommand = htons(cCmdDisconnect);
    rd.mHeader.mSize = htons(sizeof(RequestDisconnect));

    if (sendRequest((RequestHeader *) &rd) != 0) {
        logErr("Disconnect request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a DiSEqC send burst request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendDiseqcSendBurstRequest() const {
    if (!m_tune)
        return -1;

    RequestFeDiseqcSendBurst rdsb;
    rdsb.mHeader.mCommand = htons(cCmdFeDiseqcSendBurst);
    rdsb.mHeader.mSize = htons(sizeof(RequestFeDiseqcSendBurst));
    rdsb.mData = htons(m_tune->diseqc[m_diseqc_cmd].burst_data);

    if (sendRequest((RequestHeader *) &rdsb) != 0) {
        logErr("DiSEqC send burst request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a DiSEqC send master command request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendDiseqcSendMasterCommandRequest() const {
    if (!m_tune)
        return -1;

    RequestFeDiseqcSendMasterCommand rdsmc;
    rdsmc.mHeader.mCommand = htons(cCmdFeDiseqcSendMasterCommand);
    rdsmc.mHeader.mSize = htons(sizeof(RequestFeDiseqcSendMasterCommand));
    memcpy(rdsmc.mMsg, m_tune->diseqc[m_diseqc_cmd].message, 6);
    memset(rdsmc.mReserved, 0, 2);
    rdsmc.mMsgLen = m_tune->diseqc[m_diseqc_cmd].message_len;

    if (sendRequest((RequestHeader *) &rdsmc) != 0) {
        logErr("DiSEqC send master command request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a keepalive request
///
/// A keepalive request is actually a frontend status request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendKeepaliveRequest() const {
    RequestFeReadStatus rfrs;
    rfrs.mHeader.mCommand = htons(cCmdFeReadStatus);
    rfrs.mHeader.mSize = htons(sizeof(RequestFeReadStatus));

    if (sendRequest((RequestHeader *) &rfrs) != 0) {
        logErr("Keepalive request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a prepare tone request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendPrepareToneRequest() const {
    if (!m_tune) {
        logErr("Tuning parameters missing");
        return -1;
    }

    RequestFeSetTone rfst;
    rfst.mHeader.mCommand = htons(cCmdFeSetTone);
    rfst.mHeader.mSize = htons(sizeof(RequestFeSetTone));
    rfst.mData = htons(1);

    if (sendRequest((RequestHeader *) &rfst) != 0) {
        logErr("Prepare tone request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends request to the NAT device
///
/// @param packet the request packet to send to the NAT device
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendRequest(const RequestHeader *packet) const {
    if (!m_tvsat_ip)
        return -1;

    if (!m_sock.send((const uint8_t *) packet, ntohs(packet->mSize), m_tvsat_ip, 11111))
        return -1;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a reset filter request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendResetFilterRequest() {
    RequestTseStart2 rts;
    memset(&rts, 0, sizeof(RequestTseStart2));
    rts.mHeader.mCommand = htons(cCmdTseStart2);
    rts.mHeader.mSize = htons(sizeof(RequestTseStart2));
    rts.mFilterMode = htons(0x8000);
    rts.mNumPids = 0;

    if (sendRequest((RequestHeader *) &rts) != 0) {
        logErr("Reset filter request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a set filter request that asks for the stored PIDs
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendSetFilterRequest() {
    uint16_t *pids = new uint16_t[m_pids.size()];
    unsigned int i = 0;
    std::set<uint16_t>::const_iterator it = m_pids.begin();

    for (; i < m_pids.size() && it != m_pids.end(); ++i, ++it)
        pids[i] = *it;

    int ret = sendSetFilterRequest(pids, m_pids.size());

    delete[] pids;

    return ret;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a set filter request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendSetFilterRequest(uint16_t *pids, uint16_t num_pids) {
    RequestTseStart2 rts;
    memset(&rts, 0, sizeof(RequestTseStart2));
    rts.mHeader.mCommand = htons(cCmdTseStart2);
    rts.mHeader.mSize = htons(sizeof(RequestTseStart2));
    rts.mFilterMode = htons(0x8001);
    rts.mNumPids = htons((num_pids <= 168) ? num_pids : 168);

    for (int i = 0; (i < num_pids) && (i < 168); ++i) {
        logDbg("Select PID %u", pids[i]);
        rts.mPids[i] = htons(pids[i]);
    }

    if (sendRequest((RequestHeader *) &rts) != 0) {
        logErr("Set filter request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a set frontend request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendSetFrontendRequest() {
    if (!m_tune) {
        logErr("Tuning parameters missing");
        return -1;
    }

    RequestFeSetFrontend rfsf;
    memset(&rfsf, 0, sizeof(RequestFeSetFrontend));
    rfsf.mHeader.mCommand = htons(cCmdFeSetFrontend);
    rfsf.mHeader.mSize = htons(sizeof(RequestFeSetFrontend));
    rfsf.mType = htons(3); //our NAT always uses the DVB-S2 struct
    rfsf.mInversion = htons(m_tune->inversion);
    rfsf.mFrequency = htonl(m_tune->frequency);
    rfsf.mUnion.mS2.mSymbolRate = htonl(m_tune->symbol_rate / 1000);
    rfsf.mUnion.mS2.mFecInner = htons(m_tune->fec);
    rfsf.mUnion.mS2.mRollOff = htons(m_tune->roll_off);
    rfsf.mUnion.mS2.mModulation = htons(m_tune->modulation);

    // the NAT device is unable to automatically detect the use of pilot symbols,
    // so we have to tune once for each setting and try to get a lock
    m_try_pilot1 = 0;

    if (m_tune->pilot == 2) {
        logDbg("Automatic detection of pilot symbols: trying without PS");
        rfsf.mUnion.mS2.mPilot = htons(0);
        m_try_pilot1 = 1;
    } else
        rfsf.mUnion.mS2.mPilot = htons(m_tune->pilot);

    if (sendRequest((RequestHeader *) &rfsf) != 0) {
        logErr("Set frontend request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a set tone request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendSetToneRequest() const {
    if (!m_tune) {
        logErr("Tuning parameters missing");
        return -1;
    }

    RequestFeSetTone rfst;
    rfst.mHeader.mCommand = htons(cCmdFeSetTone);
    rfst.mHeader.mSize = htons(sizeof(RequestFeSetTone));
    rfst.mData = htons(m_tune->band);

    if (sendRequest((RequestHeader *) &rfst) != 0) {
        logErr("ERROR: Set tone request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a set voltage request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendSetVoltageRequest() const {
    if (!m_tune) {
        logErr("Tuning parameters missing");
        return -1;
    }

    RequestFeSetVoltage rfsv;
    rfsv.mHeader.mCommand = htons(cCmdFeSetVoltage);
    rfsv.mHeader.mSize = htons(sizeof(RequestFeSetVoltage));
    rfsv.mData = htons(m_tune->polarization);

    if (sendRequest((RequestHeader *) &rfsv) != 0) {
        logErr("Set voltage request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a start request
///
/// This request also tells the NAT device where it should send the
/// stream to
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendStartRequest() {
    if ((m_client_ip[0] == 0) || (m_client_port == 0))
        return -1;

    RequestStart rs;
    rs.mHeader.mCommand = htons(cCmdStart);
    rs.mHeader.mSize = htons(sizeof(RequestStart));
    memcpy(rs.mClientIpAddress, m_client_ip, 4);
    rs.mRecvPort = htons(m_client_port);
    rs.mDirection = 0;

    if (sendRequest((RequestHeader *) &rs) != 0) {
        logErr("Start request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sends a stop request
///
/// @return -1, if the request failed
/// @return  0, otherwise
//////////////////////////////////////////////////////////////////////////
int CTVSatStreamIn::sendStopRequest() const {
    RequestStop rs;
    rs.mHeader.mCommand = htons(cCmdStop);
    rs.mHeader.mSize = htons(sizeof(RequestStop));

    if (sendRequest((RequestHeader *) &rs) != 0) {
        logErr("Stop request failed");
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
/// Sets the IP address of the client
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::setClientIP(const uint8_t *ip) {
    if (!ip)
        return;

    memcpy(m_client_ip, ip, 4);
}

//////////////////////////////////////////////////////////////////////////
/// Sets the IP address of the NAT device
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::setTVSatIP(const uint8_t *ip) {
    if (!ip)
        return;

    snprintf(m_tvsat_ip, 16, "%i.%i.%i.%i", ip[0], ip[1], ip[2], ip[3]);
}

//////////////////////////////////////////////////////////////////////////
/// Sets the tuning parameters
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::setTuningParameters(const tvsat_tuning_parameters *tune) {
    if (!tune)
        return;

    if (m_tune) {
        delete m_tune;
        m_tune = 0;
    }

    m_tune = new tvsat_tuning_parameters(*tune);
}

//////////////////////////////////////////////////////////////////////////
/// Restarts the state machine by setting its state to 'connected'
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::start() {
    if (!m_tune)
        return;

    m_do_tune = 1;
    m_stop = 0;

    if ((m_state != eDisconnected) && (m_state != eSentDisconnectRequest)) {
        m_state = eConnected;
        tick();
    }
}

//////////////////////////////////////////////////////////////////////////
/// Starts the receiver thread
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::startReceiver() {
    pthread_create(&m_thread, 0, startThread, (void *) this);
    m_thread_started = 1;
}

//////////////////////////////////////////////////////////////////////////
/// Entry point for a new receiver thread
/// @param sin an instance of CTVSatStreamIn
//////////////////////////////////////////////////////////////////////////
void *CTVSatStreamIn::startThread(void *sin) {
    ((CTVSatStreamIn *) sin)->receiverLoop();
    pthread_exit(0);
}

//////////////////////////////////////////////////////////////////////////
/// Resets the state machine to the 'connected' state and tells the NAT
/// device to stop
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::stop() {
    m_do_tune = 0;
    m_stop = 1;

    if ((m_state != eDisconnected) && (m_state != eSentDisconnectRequest))
        m_state = eConnected;

    if (m_tune) {
        delete m_tune;
        m_tune = 0;
    }

    tick();
}

//////////////////////////////////////////////////////////////////////////
/// Makes a transition from one state to another
//////////////////////////////////////////////////////////////////////////
void CTVSatStreamIn::tick() {
    int rv;

    switch (m_state) {
        case eConnected:
            if (!m_do_connect) {
                if (sendDisconnectRequest() == 0) {
                    m_state = eSentDisconnectRequest;
                    m_retry = 10;
                    break;
                }

                m_state = eError;
                break;
            }

            if (m_do_tune || m_stop) {
                if (sendStopRequest() == 0) {
                    m_state = eSentStopRequest;
                    m_retry = 10;
                    break;
                }

                m_state = eError;
                break;
            }

            if (m_select_pids) {
                m_select_pids = 0;

                if (sendSetFilterRequest() == 0) {
                    m_state = eSentSetFilterRequest;
                    m_retry = 50;
                    break;
                }

                m_state = eError;
                break;
            }

            if (m_wait) {
                --m_wait;
                break;
            }

            if (sendKeepaliveRequest() == 0) {
                m_state = eSentKeepaliveRequest;
                m_retry = 10;
                break;
            }

            m_state = eError;
            break;

        case eDisconnected:
            if (!m_do_connect)
                break;

            if (sendConnectRequest() == 0) {
                m_state = eSentConnectRequest;
                m_retry = 10;
                break;
            }

            m_state = eError;
            break;

        case eDiSEqC:
            if (m_diseqc_cmd == TVSAT_MAX_DISEQC_CMDS) {
                if (sendSetToneRequest() == 0) {
                    m_state = eSentSetToneRequest;
                    m_retry = 10;
                    break;
                }

                m_state = eError;
                break;
            }

            if (!m_tune) {
                m_state = eError;
                break;
            }

            switch (m_tune->diseqc[m_diseqc_cmd].type) {
                case 0:
                    m_diseqc_cmd = TVSAT_MAX_DISEQC_CMDS;
                    return;

                case 1:
                    if (sendDiseqcSendMasterCommandRequest() == 0) {
                        m_state = eSentDiseqcSendMasterCommandRequest;
                        m_retry = 10;
                        ++m_diseqc_cmd;
                        return;
                    }

                    break;

                case 2:
                    if (sendDiseqcSendBurstRequest() == 0) {
                        m_state = eSentDiseqcSendBurstRequest;
                        m_retry = 10;
                        ++m_diseqc_cmd;
                        return;
                    }

                    break;

                default:
                    break;
            }

            m_state = eError;
            break;

        case eError:
            cleanUp();
            m_state = eDisconnected;
            break;

        case eSentConnectRequest:
            if (receiveConnectResponse() == 0) {
                if (m_tune)
                    m_do_tune = 1;

                m_state = eConnected;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentDisconnectRequest:
            if (receiveDisconnectResponse() == 0) {
                m_state = eDisconnected;
                break;
            }

            m_state = eError;
            break;

        case eSentDiseqcSendBurstRequest:
            if (receiveDiseqcSendBurstResponse() == 0) {
                m_state = eDiSEqC;
                CTVSatCtl::sleepMS(100);
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentDiseqcSendMasterCommandRequest:
            if (receiveDiseqcSendMasterCommandResponse() == 0) {
                m_state = eDiSEqC;
                CTVSatCtl::sleepMS(100);
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentKeepaliveRequest:
            if (receiveKeepaliveResponse() >= 0) {
                m_state = eConnected;
                m_wait = 100;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentPrepareToneRequest:
            if (receivePrepareToneResponse() == 0) {
                if (sendSetVoltageRequest() == 0) {
                    m_state = eSentSetVoltageRequest;
                    m_retry = 10;
                    break;
                }

                m_state = eError;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentResetFilterRequest:
            if (receiveResetFilterResponse() == 0) {
                if (sendPrepareToneRequest() == 0) {
                    m_state = eSentPrepareToneRequest;
                    m_retry = 10;
                    break;
                }

                m_state = eError;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentSetFilterRequest:
            if (receiveSetFilterResponse() == 0) {
                m_state = eConnected;
                m_wait = 5;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentSetFrontendRequest:
            if (receiveSetFrontendResponse() == 0) {
                if (sendStartRequest() == 0) {
                    m_state = eSentStartRequest;
                    m_retry = 10;
                    break;
                }

                m_state = eError;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentSetToneRequest:
            if (receiveSetToneResponse() == 0) {
                if (sendSetFrontendRequest() == 0) {
                    m_state = eSentSetFrontendRequest;
                    m_retry = 10;
                    break;
                }

                m_state = eError;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentSetVoltageRequest:
            if (receiveSetVoltageResponse() == 0) {
                m_state = eDiSEqC;
                m_diseqc_cmd = 0;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentStartRequest:
            if (receiveStartResponse() == 0) {
                if (sendKeepaliveRequest() == 0) {
                    m_state = eTuning;
                    m_retry = 10;
                    m_wait = 1000;
                    break;
                }

                m_state = eError;
                break;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eSentStopRequest:
            if (receiveStopResponse() == 0) {
                m_stop = 0;
                m_is_tuned = 0;

                if (m_do_tune) {
                    m_do_tune = 0;

                    if (sendResetFilterRequest() == 0) {
                        m_state = eSentResetFilterRequest;
                        m_retry = 10;
                        break;
                    }

                    m_state = eError;
                    break;
                }

                m_state = eConnected;
                m_wait = 100;
            }

            if (m_retry) {
                --m_retry;
                break;
            }

            m_state = eError;
            break;

        case eTuning:
            rv = receiveKeepaliveResponse();

            if (rv == -1) {
                if (m_retry) {
                    --m_retry;
                    break;
                }
            }

            if (rv == 0) {
                m_is_tuned = 1;
                m_select_pids = 1;

                if (sendKeepaliveRequest() == 0) {
                    m_state = eSentKeepaliveRequest;
                    m_retry = 5;
                    break;
                }

                m_state = eError;
                break;
            }

            if (rv == 1) {
                if (m_wait) {
                    --m_wait;

                    if (sendKeepaliveRequest() == 0) {
                        m_retry = 10;
                        break;
                    }
                } else {
                    if (m_try_pilot1) {
                        logDbg("Automatic detection of pilot symbols: trying with PS");

                        if (m_tune) {
                            m_tune->pilot = 1;

                            if (sendSetFrontendRequest() == 0) {
                                m_state = eSentSetFrontendRequest;
                                m_retry = 10;
                                break;
                            }
                        }
                    }
                }
                m_state = eError;
                break;
            }

            m_state = eError;
            break;

        default:
            break;
    }
}

//////////////////////////////////////////////////////////////////////////
/// Implements a less-than operator for struct timeval
/// @param tv1 first time
/// @param tv2 second time
/// @return true, if the first time is shorter than the second time
//////////////////////////////////////////////////////////////////////////
bool CTVSatStreamIn::tvlt(const timeval *tv1, const timeval *tv2) const {
    if (tv1->tv_sec < tv2->tv_sec)
        return true;

    if (tv1->tv_sec > tv2->tv_sec)
        return false;

    if (tv1->tv_usec < tv2->tv_usec)
        return true;

    return false;
}
