//
//  nexdome.h
//  NexDome
//
//  Created by Rodolphe Pineau on 6/11/2016.
//  NexDome X2 plugin

#ifndef __NEXDOME__
#define __NEXDOME__
#include <math.h>
#include <string.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>


#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"

#define SERIAL_BUFFER_SIZE 20
#define MAX_TIMEOUT 5000
#define ND_LOG_BUFFER_SIZE 256

// error codes
// Error code
enum NexDomeErrors {ND_OK=0, NOT_CONNECTED, ND_CANT_CONNECT, ND_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum NexDomeShutterState {OPEN=1, OPENING, CLOSED, CLOSING, SHUTTER_ERROR};

class CNexDome
{
public:
    CNexDome();
    ~CNexDome();

    int        Connect(const char *szPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; }

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; }
    void        setLogger(LoggerInterface *pLogger) { m_pLogger = pLogger; };

    // Dome commands
    int syncDome(double dAz, double dEl);
    int parkDome(void);
    int unparkDome(void);
    int gotoAzimuth(double dNewAz);
    int openShutter();
    int closeShutter();
    int getFirmwareVersion(char *szVersion, int nStrMaxLen);
    int getFirmwareVersion(float &fVersion);
    int goHome();
    int calibrate();

    // command complete functions
    int isGoToComplete(bool &bComplete);
    int isOpenComplete(bool &bComplete);
    int isCloseComplete(bool &bComplete);
    int isParkComplete(bool &bComplete);
    int isUnparkComplete(bool &bComplete);
    int isFindHomeComplete(bool &bComplete);
    int isCalibratingComplete(bool &bComplete);

    int abortCurrentCommand();

    // getter/setter
    int getNbTicksPerRev();
    int getBatteryLevel();

    double getHomeAz();
    int setHomeAz(double dAz);

    double getParkAz();
    int setParkAz(double dAz);

    double getCurrentAz();
    double getCurrentEl();

    int getCurrentShutterState();
    int getBatteryLevels(double &dDomeVolts, double &dShutterVolts);

    void setDebugLog(bool bEnable);

protected:
    
    int             readResponse(char *respBuffer, int bufferLen);
    int             getDomeAz(double &domeAz);
    int             getDomeEl(double &domeEl);
    int             getDomeHomeAz(double &Az);
    int             getDomeParkAz(double &Az);
    int             getShutterState(int &state);
    int             getDomeStepPerRev(int &stepPerRev);

    bool            isDomeMoving();
    bool            isDomeAtHome();
    int             domeCommand(const char *cmd, char *result, char respCmdCode, int resultMaxLen);
    int             parseFields(char *pszResp, std::vector<std::string> &svFields, char cSeparator);

    SerXInterface   *m_pSerx;
    LoggerInterface *m_pLogger;
    bool            m_bDebugLog;
    
    bool            m_bIsConnected;
    bool            m_bHomed;
    bool            m_bParked;
    bool            m_bShutterOpened;
    bool            m_bCalibrating;
    
    int             m_nNbStepPerRev;
    double          m_dShutterBatteryVolts;
    double          m_dHomeAz;
    
    double          m_dParkAz;

    double          m_dCurrentAzPosition;
    double          m_dCurrentElPosition;

    double          m_dGotoAz;

    float           m_fVersion;

    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];
    int             m_nShutterState;
    bool            m_bShutterOnly; // roll off roof so the arduino is running the shutter firmware only.
    char            m_szLogBuffer[ND_LOG_BUFFER_SIZE];

};

#endif
