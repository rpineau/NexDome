//
//  nexdome.cpp
//  NexDome X2 plugin
//
//  Created by Rodolphe Pineau on 6/11/2016.


#include "nexdome.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

CNexDome::CNexDome()
{
    // set some sane values
    m_bDebugLog = true;
    
    m_pSerx = NULL;
    m_bIsConnected = false;

    m_nNbStepPerRev = 0;
    m_dShutterBatteryVolts = 0.0;
    
    m_dHomeAz = 0;
    m_dParkAz = 0;

    m_dCurrentAzPosition = 0.0;
    m_dCurrentElPosition = 0.0;

    m_bCalibrating = false;
    
    m_bShutterOpened = false;
    
    m_bParked = true;
    m_bHomed = false;

    m_fVersion = 0.0;

    memset(m_szFirmwareVersion,0,SERIAL_BUFFER_SIZE);
    memset(m_szLogBuffer,0,ND_LOG_BUFFER_SIZE);
}

CNexDome::~CNexDome()
{

}

int CNexDome::Connect(const char *szPort)
{
    int nErr;
    
    // 9600 8N1
    if(m_pSerx->open(szPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::Connect] Connected.");
        m_pLogger->out(m_szLogBuffer);

        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::Connect] Getting Firmware.");
        m_pLogger->out(m_szLogBuffer);
    }
    // if this fails we're not properly connected.
    nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
    if(nErr) {
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::Connect] Error Getting Firmware.");
            m_pLogger->out(m_szLogBuffer);
        }
        m_bIsConnected = false;
        m_pSerx->close();
        return FIRMWARE_NOT_SUPPORTED;
    }

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::Connect] Got Firmware %s", m_szFirmwareVersion);
        m_pLogger->out(m_szLogBuffer);
    }

    getDomeParkAz(m_dCurrentAzPosition);
    getDomeHomeAz(m_dHomeAz);
    return SB_OK;
}


void CNexDome::Disconnect()
{
    if(m_bIsConnected) {
        m_pSerx->purgeTxRx();
        m_pSerx->close();
    }
    m_bIsConnected = false;
}


int CNexDome::readResponse(char *szRespBuffer, int nBufferLen)
{
    int nErr = ND_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;

    memset(szRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = szRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
            if (m_bDebugLog) {
                snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::readResponse] readFile error.");
                m_pLogger->out(m_szLogBuffer);
            }
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
            if (m_bDebugLog) {
                snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::readResponse] readFile Timeout.");
                m_pLogger->out(m_szLogBuffer);
            }
            nErr = ND_BAD_CMD_RESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
    } while (*pszBufPtr++ != '\n' && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the \n

    return nErr;
}


int CNexDome::domeCommand(const char *szCmd, char *szResult, char respCmdCode, int nResultMaxLen)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;

    m_pSerx->purgeTxRx();
//    if (m_bDebugLog) {
//        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::domeCommand] Sending %s",szCmd);
//        m_pLogger->out(m_szLogBuffer);
//    }
    nErr = m_pSerx->writeFile((void *)szCmd, strlen(szCmd), ulBytesWrite);
    m_pSerx->flushTx();
    if(nErr)
        return nErr;
    // read response
    nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

//    if (m_bDebugLog) {
//        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::domeCommand] response = %s", szResp);
//        m_pLogger->out(m_szLogBuffer);
//    }

    if(szResp[0] != respCmdCode)
        nErr = ND_BAD_CMD_RESPONSE;

    if(szResult)
        strncpy(szResult, &szResp[1], nResultMaxLen);

    return nErr;

}

int CNexDome::getDomeAz(double &dDomeAz)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    nErr = domeCommand("q\n", szResp, 'Q', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeAz] ERROR = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }
    // convert Az string to double
    dDomeAz = atof(szResp);
    m_dCurrentAzPosition = dDomeAz;

    return nErr;
}

int CNexDome::getDomeEl(double &dDomeEl)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    if(!m_bShutterOpened)
    {
        dDomeEl = 0.0;
        return nErr;
    }
        
    nErr = domeCommand("b\n", szResp, 'B', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeEl] ERROR = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }
    
    // convert El string to double
    dDomeEl = atof(szResp);
    m_dCurrentElPosition = dDomeEl;
    
    return nErr;
}


int CNexDome::getDomeHomeAz(double &dAz)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    nErr = domeCommand("i\n", szResp, 'I', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeHomeAz] ERROR = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }

    // convert Az string to double
    dAz = atof(szResp);
    m_dHomeAz = dAz;
    return nErr;
}

int CNexDome::getDomeParkAz(double &dAz)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    nErr = domeCommand("n\n", szResp, 'N', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeParkAz] ERROR = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }

    // convert Az string to double
    dAz = atof(szResp);
    m_dParkAz = dAz;
    return nErr;
}


int CNexDome::getShutterState(int &nState)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    nErr = domeCommand("u\n", szResp, 'U', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getShutterState] ERROR = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }

    nState = atoi(szResp);

    return nErr;
}


int CNexDome::getDomeStepPerRev(int &nStepPerRev)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = domeCommand("t\n", szResp, 'T', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeStepPerRev] ERROR = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }

    nStepPerRev = atoi(szResp);
    m_nNbStepPerRev = nStepPerRev;
    return nErr;
}

int CNexDome::getBatteryLevels(double &domeVolts, double &shutterVolts)
{
    int nErr = 0;
    int rc = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;
    
    nErr = domeCommand("k\n", szResp, 'K', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getBatteryLevels] ERROR = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }

    rc = sscanf(szResp, "%lf %lf", &domeVolts, &shutterVolts);
    if(rc == 0) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getBatteryLevels] sscanf ERROR ");
        m_pLogger->out(m_szLogBuffer);
        return COMMAND_FAILED;
    }

    domeVolts = domeVolts / 100.0;
    shutterVolts = shutterVolts / 100.0;
    return nErr;
}

void CNexDome::setDebugLog(bool bEnable)
{
    m_bDebugLog = bEnable;
}

bool CNexDome::isDomeMoving()
{
    bool bIsMoving;
    int nTmp;
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = domeCommand("m\n", szResp, 'M', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isDomeMoving] isDomeMoving = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return false;
    }

    bIsMoving = false;
    nTmp = atoi(szResp);
    if(nTmp)
        bIsMoving = true;

    return bIsMoving;
}

bool CNexDome::isDomeAtHome()
{
    bool bAthome;
    int nTmp;
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;
    
    nErr = domeCommand("z\n", szResp, 'Z', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isDomeAtHome] ERROR isDomeAtHome = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return false;
    }

    bAthome = false;
    nTmp = atoi(szResp);
    if(nTmp)
        bAthome = true;
    
    return bAthome;
  
}

int CNexDome::syncDome(double dAz, double dEl)
{
    int nErr = 0;
    char szBuf[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_dCurrentAzPosition = dAz;
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "s %3.2f\n", dAz);
    nErr = domeCommand(szBuf, NULL, 'S', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::syncDome] ERROR syncDome ");
        m_pLogger->out(m_szLogBuffer);
    }
    // TODO : Also set Elevation when supported by the firware.
    // m_dCurrentElPosition = dEl;
    return nErr;
}

int CNexDome::parkDome()
{
    int nErr = 0;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = gotoAzimuth(m_dParkAz);
    return nErr;

}

int CNexDome::unparkDome()
{
    m_bParked = false;
    m_dCurrentAzPosition = m_dParkAz;
    syncDome(m_dCurrentAzPosition,m_dCurrentElPosition);
    return 0;
}

int CNexDome::gotoAzimuth(double dNewAz)
{
    int nErr = 0;
    char szBuf[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;


    while(dNewAz >= 360)
        dNewAz = dNewAz - 360;
    
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "g %3.2f\n", dNewAz);
    nErr = domeCommand(szBuf, NULL, 'G', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::gotoAzimuth] ERROR gotoAzimuth");
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }

    m_dGotoAz = dNewAz;
    return nErr;
}

int CNexDome::openShutter()
{
    int nErr = 0;
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return SB_OK;

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::openShutter] Opening shutter");
        m_pLogger->out(m_szLogBuffer);
    }
    nErr = domeCommand("d\n", NULL, 'D', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::openShutter] ERROR gotoAzimuth");
        m_pLogger->out(m_szLogBuffer);
    }

    return nErr;
}

int CNexDome::closeShutter()
{
    int nErr = 0;
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return SB_OK;

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::closeShutter] Closing shutter");
        m_pLogger->out(m_szLogBuffer);
    }
    nErr = domeCommand("e\n", NULL, 'D', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::closeShutter] ERROR Closing shutter");
        m_pLogger->out(m_szLogBuffer);
    }

    return nErr;
}

int CNexDome::getFirmwareVersion(char *szVersion, int nStrMaxLen)
{
    int nErr = 0;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> firmwareFields;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return SB_OK;

    nErr = domeCommand("v\n", szResp, 'V', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::closeShutter] ERROR getFirmwareVersion = %s", szResp);
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }

    nErr = parseFields(szResp,firmwareFields, ' ');
    if(nErr)
        return nErr;

    if(firmwareFields.size()>2) {
        strncpy(szVersion, firmwareFields[2].c_str(), nStrMaxLen);
        m_fVersion = atof(firmwareFields[2].c_str());
    }
    else {
        strncpy(szVersion, szResp, nStrMaxLen);
        m_fVersion = atof(szResp);
    }
    return nErr;
}

int CNexDome::getFirmwareVersion(float &fVersion)
{
    int nErr;

    if(m_fVersion == 0.0) {
        nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
        if(nErr)
            return nErr;
    }

    return m_fVersion;
}

int CNexDome::goHome()
{
    int nErr = 0;
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating) {
        return SB_OK;
    }
    else if(isDomeAtHome()){
            m_bHomed = true;
            return ND_OK;
    }
    nErr = domeCommand("h\n", NULL, 'H', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::closeShutter] ERROR goHome");
        m_pLogger->out(m_szLogBuffer);
        return nErr;
    }

    return nErr;
}

int CNexDome::calibrate()
{
    int nErr = 0;
    if(!m_bIsConnected)
        return NOT_CONNECTED;


    nErr = domeCommand("c\n", NULL, 'C', SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    m_bCalibrating = true;
    
    return nErr;
}

int CNexDome::isGoToComplete(bool &bComplete)
{
    int nErr = 0;
    double dDomeAz = 0;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        bComplete = false;
        getDomeAz(dDomeAz);
        return nErr;
    }

    getDomeAz(dDomeAz);
    if(dDomeAz >0 && dDomeAz<1)
        dDomeAz = 0;
    
    while(ceil(m_dGotoAz) >= 360)
          m_dGotoAz = ceil(m_dGotoAz) - 360;

    while(ceil(dDomeAz) >= 360)
        dDomeAz = ceil(dDomeAz) - 360;

    if ((ceil(m_dGotoAz) <= ceil(dDomeAz)+1) && (ceil(m_dGotoAz) >= ceil(dDomeAz)-1))
        bComplete = true;
    else {
        // we're not moving and we're not at the final destination !!!
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isGoToComplete] domeAz = %f, m_dGotoAz = %f", ceil(dDomeAz), ceil(m_dGotoAz));
            m_pLogger->out(m_szLogBuffer);
        }
        bComplete = false;
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}

int CNexDome::isOpenComplete(bool &bComplete)
{
    int nErr = 0;
    int nState;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = getShutterState(nState);
    if(nErr)
        return ERR_CMDFAILED;
    if(nState == OPEN){
        m_bShutterOpened = true;
        bComplete = true;
        m_dCurrentElPosition = 90.0;
    }
    else {
        m_bShutterOpened = false;
        bComplete = false;
        m_dCurrentElPosition = 0.0;
    }

    return nErr;
}

int CNexDome::isCloseComplete(bool &bComplete)
{
    int nErr = 0;
    int nState;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = getShutterState(nState);
    if(nErr)
        return ERR_CMDFAILED;
    if(nState == CLOSED){
        m_bShutterOpened = false;
        bComplete = true;
        m_dCurrentElPosition = 0.0;
    }
    else {
        m_bShutterOpened = true;
        bComplete = false;
        m_dCurrentElPosition = 90.0;
    }

    return nErr;
}


int CNexDome::isParkComplete(bool &bComplete)
{
    int nErr = 0;
    double dDomeAz=0;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        getDomeAz(dDomeAz);
        bComplete = false;
        return nErr;
    }

    getDomeAz(dDomeAz);

    if (ceil(m_dParkAz) == ceil(dDomeAz))
    {
        m_bParked = true;
        bComplete = true;
    }
    else {
        // we're not moving and we're not at the final destination !!!
        bComplete = false;
        m_bHomed = false;
        m_bParked = false;
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}

int CNexDome::isUnparkComplete(bool &bComplete)
{
    int nErr = 0;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_bParked = false;
    bComplete = true;

    return nErr;
}

int CNexDome::isFindHomeComplete(bool &bComplete)
{
    int nErr = 0;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        m_bHomed = false;
        bComplete = false;
        return nErr;
    }

    if(isDomeAtHome()){
        m_bHomed = true;
        bComplete = true;
        syncDome(m_dHomeAz, m_dCurrentElPosition);
    }
    else {
        // we're not moving and we're not at the home position !!!
        if (m_bDebugLog) {
            snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isFindHomeComplete] Not moving and not at home !!!");
            m_pLogger->out(m_szLogBuffer);
        }
        bComplete = false;
        m_bHomed = false;
        m_bParked = false;
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}


int CNexDome::isCalibratingComplete(bool &bComplete)
{
    int nErr = 0;
    double dDomeAz = 0;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        getDomeAz(dDomeAz);
        m_bHomed = false;
        bComplete = false;
        return nErr;
    }

    
    nErr = getDomeAz(dDomeAz);

    if (ceil(m_dHomeAz) != ceil(dDomeAz)) {
        // We need to resync the current position to the home position.
        m_dCurrentAzPosition = m_dHomeAz;
        syncDome(m_dCurrentAzPosition,m_dCurrentElPosition);
        m_bHomed = true;
        bComplete = true;
    }

    nErr = getDomeStepPerRev(m_nNbStepPerRev);
    m_bHomed = true;
    bComplete = true;
    m_bCalibrating = false;
    return nErr;
}


int CNexDome::abortCurrentCommand()
{
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_bCalibrating = false;

    return (domeCommand("a\n", NULL, 'A', SERIAL_BUFFER_SIZE));
}

#pragma mark - Getter / Setter

int CNexDome::getNbTicksPerRev()
{
    if(m_bIsConnected)
        getDomeStepPerRev(m_nNbStepPerRev);
    return m_nNbStepPerRev;
}


double CNexDome::getHomeAz()
{
    if(m_bIsConnected)
        getDomeHomeAz(m_dHomeAz);
    return m_dHomeAz;
}

int CNexDome::setHomeAz(double dAz)
{
    int nErr = 0;
    char szBuf[SERIAL_BUFFER_SIZE];

    m_dHomeAz = dAz;
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;
    
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "j %3.2f\n", dAz);
    nErr = domeCommand(szBuf, NULL, 'J', SERIAL_BUFFER_SIZE);
    return nErr;
}


double CNexDome::getParkAz()
{
    if(m_bIsConnected)
        getDomeParkAz(m_dParkAz);

    return m_dParkAz;

}

int CNexDome::setParkAz(double dAz)
{
    int nErr = 0;
    char szBuf[SERIAL_BUFFER_SIZE];

    m_dParkAz = dAz;
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    snprintf(szBuf, SERIAL_BUFFER_SIZE, "l %3.2f\n", dAz);
    nErr = domeCommand(szBuf, NULL, 'L', SERIAL_BUFFER_SIZE);
    return nErr;
}


double CNexDome::getCurrentAz()
{
    if(m_bIsConnected)
        getDomeAz(m_dCurrentAzPosition);
    
    return m_dCurrentAzPosition;
}

double CNexDome::getCurrentEl()
{
    if(m_bIsConnected)
        getDomeEl(m_dCurrentElPosition);
    
    return m_dCurrentElPosition;
}

int CNexDome::getCurrentShutterState()
{
    if(m_bIsConnected)
        getShutterState(m_nShutterState);

    return m_nShutterState;
}


int CNexDome::parseFields(char *pszResp, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = ND_OK;
    std::string sSegment;
    std::stringstream ssTmp(pszResp);

    svFields.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
        svFields.push_back(sSegment);
    }

    if(svFields.size()==0) {
        nErr = ERR_CMDFAILED;
    }
    return nErr;
}

