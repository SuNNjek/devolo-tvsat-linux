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
/// @file streamin.h
/// @brief "dLAN TV Sat Stream Input" - header
/// @author Michael Beckers
//////////////////////////////////////////////////////////////////////////

#ifndef __TVSAT_STREAMIN_H
#define __TVSAT_STREAMIN_H

#include <map>
#include <list>
#include <pthread.h>
#include <utility>
#include <set>
#include <stdint.h>
#include <sys/time.h>

#include "udpsocket.h"
#include "../include/tvsat.h"

//////////////////////////////////////////////////////////////////////////
// UDP PACKET STRUCTURES
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Header
//////////////////////////////////////////////////////////////////////////

struct RequestHeader
{
  uint16_t mSize;
  uint16_t mCommand;
}
__attribute__ ((packed));

struct ResponseHeader
{
  uint16_t mSize;
  uint16_t mCommand;
  uint16_t mResult;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// BroadcastCmd
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdBroadcastCmd = 0xc2f8;

struct RequestBroadcastCmd
{
  RequestHeader mHeader;
  uint8_t       mDstMac[ 6 ];
  uint8_t       mReserved[ 10 ];
}
__attribute__ ((packed));

struct ResponseBroadcastCmd
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// Connect
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdConnect = 0x1;

struct RequestConnect
{
  RequestHeader mHeader;
  uint8_t       mClientIpAddress[ 4 ];
  uint16_t      mConnectTimeout;
}
__attribute__ ((packed));

struct ResponseConnect
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// Connect
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdDisconnect = 0x3;

struct RequestDisconnect
{
  RequestHeader mHeader;
}
__attribute__ ((packed));

struct ResponseDisconnect
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// FeDiseqcSendMasterCommand
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdFeDiseqcSendMasterCommand = 0x1003;

struct RequestFeDiseqcSendMasterCommand
{
  RequestHeader mHeader;
  uint8_t       mMsg[ 6 ];
  uint8_t       mReserved[ 2 ];
  uint8_t       mMsgLen;
}
__attribute__ ((packed));

struct ResponseFeDiseqcSendMasterCommand
{
  ResponseHeader  mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// FeDiseqcSendBurst
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdFeDiseqcSendBurst = 0x1005;

struct RequestFeDiseqcSendBurst
{
  RequestHeader mHeader;
  uint16_t      mData;
}
__attribute__ ((packed));

struct ResponseFeDiseqcSendBurst
{
  ResponseHeader  mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// FeReadStatus
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdFeReadStatus = 0x1009;

struct RequestFeReadStatus
{
  RequestHeader mHeader;
}
__attribute__ ((packed));

struct ResponseFeReadStatus
{
  ResponseHeader  mHeader;
  uint16_t        mData;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// FeSetFrontend
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdFeSetFrontend = 0x100e;

struct RequestFeSetFrontend
{
  RequestHeader mHeader;
  uint32_t      mFrequency;
  uint16_t      mInversion;
  uint16_t      mType;

  union
  {
    struct
    {
      uint32_t  mSymbolRate;
      uint16_t  mFecInner;
    }
    mQpsk;

    struct
    {
      uint32_t  mSymbolRate;
      uint16_t  mFecInner;
      uint16_t  mModulation;
    }
    mQam;

    struct
    {
      uint16_t  mBandwidth;
      uint16_t  mCodeRateHP;
      uint16_t  mCodeRateLP;
      uint16_t  mConstellation;
      uint16_t  mTransmissionMode;
      uint16_t  mGuardInterval;
      uint16_t  mHierarchyInformation;
    }
    mOfdm;

    struct
    {
      uint32_t  mSymbolRate;
      uint16_t  mFecInner;
      uint16_t  mModulation;
      uint16_t  mRollOff;
      uint16_t  mPilot;
    }
    mS2;
  }
  mUnion;
}
__attribute__ ((packed));

struct ResponseFeSetFrontend
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// FeSetTone
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdFeSetTone = 0x1006;

struct RequestFeSetTone
{
  RequestHeader mHeader;
  uint16_t      mData;
}
__attribute__ ((packed));

struct ResponseFeSetTone
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// FeSetVoltage
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdFeSetVoltage = 0x1007;

struct RequestFeSetVoltage
{
  RequestHeader mHeader;
  uint16_t      mData;
}
__attribute__ ((packed));

struct ResponseFeSetVoltage
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// GetInfo
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdGetInfo = 0;

struct RequestGetInfo
{
  RequestHeader mHeader;
  uint16_t      mClientVersion;
  uint16_t      mAlignment;
}
__attribute__ ((packed));

struct ResponseGetInfo
{
  ResponseHeader  mHeader;
  uint16_t        mInterfaceVersion;
  uint16_t        mServerFlags;
  uint8_t         mIpAddress[ 4 ];
  uint8_t         mMacAddress[ 6 ];
  uint8_t         mDeviceType;
  uint8_t         mTunerType;
  uint16_t        mFirmwareVersion;
  uint8_t         mSerialNumber[ 16 ];
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// NvsEepromRead
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdNvsEepromRead = 0x0107;

struct RequestNvsEepromRead
{
  RequestHeader mHeader;
  uint32_t mAddr;
  uint32_t mLen;
  uint8_t  mData[ 256 ];
}
__attribute__ ((packed));

struct ResponseNvsEepromRead
{
  ResponseHeader  mHeader;
  uint8_t         mUnused[ 2 ];
  uint32_t        mAddr;
  uint32_t        mLen;
  uint8_t         mData[ 256 ];
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// NvsEepromVerify
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdNvsEepromVerify = 0x0105;

struct RequestNvsEepromVerify
{
  RequestHeader mHeader;
  uint32_t      mAddr;
  uint32_t      mLen;
  uint8_t       mData[ 256 ];
}
__attribute__ ((packed));

struct ResponseNvsEepromVerify
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// NvsEepromWrite
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdNvsEepromWrite = 0x0106;

struct RequestNvsEepromWrite
{
  RequestHeader mHeader;
  uint32_t      mAddr;
  uint32_t      mLen;
  uint8_t       mData[ 256 ];
}
__attribute__ ((packed));

struct ResponseNvsEepromWrite
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// Reboot
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdReboot = 0xc2f3;

struct RequestReboot
{
  RequestHeader mHeader;
}
__attribute__ ((packed));

struct ResponseReboot
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// Start
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdStart = 0x20;

struct RequestStart
{
  RequestHeader mHeader;
  uint8_t       mClientIpAddress[ 4 ];
  uint16_t      mRecvPort;
  uint16_t      mDirection;
}
__attribute__ ((packed));

struct ResponseStart
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// Stop
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdStop = 0x21;

struct RequestStop
{
  RequestHeader mHeader;
}
__attribute__ ((packed));

struct ResponseStop
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// TseStart2 (same as TseStart, but allows more PIDs)
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdTseStart2 = 0x1108;

struct RequestTseStart2
{
  RequestHeader mHeader;
  uint16_t      mFilterMode;
  uint16_t      mNumPids;
  uint16_t      mPids[ 168 ];
}
__attribute__ ((packed));

struct ResponseTseStart2
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));

//////////////////////////////////////////////////////////////////////////
// TseSetConfig
//////////////////////////////////////////////////////////////////////////

const uint16_t cCmdTseSetConfig = 0x1103;

struct RequestTseSetConfig
{
  RequestHeader mHeader;
  uint16_t      mTSPSize;     //188
  uint16_t      mTSPPerFrame; //7
  uint16_t      mLVDSConfig;  //7
  uint16_t      mLVDSPSize;   //0
  uint32_t      mLVDSFreq;    //0
}
__attribute__ ((packed));

struct ResponseTseSetConfig
{
  ResponseHeader mHeader;
}
__attribute__ ((packed));


//////////////////////////////////////////////////////////////////////////
// NVS User Data
//////////////////////////////////////////////////////////////////////////

const uint16_t cUserDataAddr = 0x7fc0;

struct NvsUserData
{
  uint8_t mCrc[ 4 ];
  uint8_t mRes[ 16 ];
  uint8_t mFlg[ 4 ];
  uint8_t mMsk[ 4 ];
  uint8_t mFip[ 4 ];
}
__attribute ((packed));

//////////////////////////////////////////////////////////////////////////
/// dLAN TV Sat Device Control and Stream Input
///
/// This class basically works like a state machine. With every call to
/// the tick() function it makes a transition from one state to another.
//////////////////////////////////////////////////////////////////////////
class CTVSatStreamIn
{
  public:
    enum state_t
    {
      eConnected,
      eDisconnected,
      eDiSEqC,
      eError,
      eSentConnectRequest,
      eSentDisconnectRequest,
      eSentDiseqcSendBurstRequest,
      eSentDiseqcSendMasterCommandRequest,
      eSentKeepaliveRequest,
      eSentPrepareToneRequest,
      eSentResetFilterRequest,
      eSentSetFilterRequest,
      eSentSetFrontendRequest,
      eSentSetToneRequest,
      eSentSetVoltageRequest,
      eSentStartRequest,
      eSentStopRequest,
      eTuning
    };

    CTVSatStreamIn  ();
    ~CTVSatStreamIn ();

    void  addPID      ( uint16_t pid );

    void  connect     ()
    { m_do_connect = 1; }

    void  delPID      ( uint16_t pid );
    void  delPIDs     ();

    void  disconnect  ()
    { m_do_connect = 0; }

    /// True, if the NAT device is tuned and has a signal lock
    int isTuned () const
    { return m_is_tuned; }

    /// Gets the current state of the state machine
    state_t getState      () const
    { return m_state; }

    void  receiveStreamData ();

    void  setClientIP         ( const uint8_t *ip );

    /// Sets the UDP port of the client
    void  setClientPort       ( uint16_t port )
    { m_client_port = port;
      m_stream_sock.open( m_client_port ); }

    void  setInputDev         ( int input_dev )
    { m_input_dev = input_dev; }

    void  setTVSatIP          ( const uint8_t *ip );
    void  setTuningParameters ( const tvsat_tuning_parameters *tune );

    void  start         ();
    void  startReceiver ();
    void  stop          ();
    void  tick          ();

  private:
    void  cleanUp                                 ();
    int   receiveConnectResponse                  () const;
    int   receiveDisconnectResponse               () const;
    int   receiveDiseqcSendBurstResponse          () const;
    int   receiveDiseqcSendMasterCommandResponse  () const;
    int   receiveKeepaliveResponse                () const;
    int   receivePrepareToneResponse              () const;
    int   receiveResetFilterResponse              () const;
    int   receiveResponse                         ( uint16_t cmd ) const;
    int   receiveSetFilterResponse                () const;
    int   receiveSetFrontendResponse              () const;
    int   receiveSetToneResponse                  () const;
    int   receiveSetVoltageResponse               () const;
    int   receiveStartResponse                    () const;
    int   receiveStopResponse                     () const;
    void  receiverLoop                            ();
    int   sendConnectRequest                      () const;
    int   sendDisconnectRequest                   () const;
    int   sendDiseqcSendBurstRequest              () const;
    int   sendDiseqcSendMasterCommandRequest      () const;
    int   sendKeepaliveRequest                    () const;
    int   sendPrepareToneRequest                  () const;
    int   sendRequest                             ( const RequestHeader *packet ) const;
    int   sendResetFilterRequest                  ();
    int   sendSetFilterRequest                    ();
    int   sendSetFilterRequest                    ( uint16_t *pids, uint16_t num_pids );
    int   sendSetFrontendRequest                  ();
    int   sendSetToneRequest                      () const;
    int   sendSetVoltageRequest                   () const;
    int   sendStartRequest                        ();
    int   sendStopRequest                         () const;
    bool  tvlt                                    ( const timeval *tv1, const timeval *tv2 ) const;

    static void * startThread ( void *sin );

    uint8_t                       m_client_ip[ 4 ];
    uint16_t                      m_client_port;
    std::map< uint16_t, timeval > m_del_pids;
    int                           m_diseqc_cmd;
    int                           m_do_connect;
    int                           m_do_tune;
    int                           m_input_dev;
    int                           m_is_tuned;
    std::set< uint16_t >          m_pids;
    int                           m_retry;
    int                           m_select_pids;
    CUDPSocket                    m_sock;
    state_t                       m_state;
    int                           m_stop;
    pthread_mutex_t               m_stop_access;
    int                           m_stop_thread;
    CUDPSocket                    m_stream_sock;
    pthread_t                     m_thread;
    int                           m_thread_started;
    int                           m_try_pilot1;
    tvsat_tuning_parameters      *m_tune;
    char                          m_tvsat_ip[ 16 ];
    int                           m_wait;
};

#endif
