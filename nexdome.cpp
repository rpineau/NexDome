//
//  nexdome.cpp
//  NexDome X2 plugin
//
//  Created by Rodolphe Pineau on 6/11/2016.


#include "nexdome.h"

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
    m_bParking = false;
    m_bUnParking = false;
    m_bHasBeenHome = false;

    m_bShutterOpened = false;
    
    m_bParked = true;
    m_bHomed = false;

    m_fVersion = 0.0;
    m_nHomingTries = 0;
    m_nGotoTries = 0;

    m_nIsRaining = NOT_RAINING;

    m_bHomeOnPark = false;
    m_bHomeOnUnpark = false;

    memset(m_szFirmwareVersion,0,SERIAL_BUFFER_SIZE);
    memset(m_szLogBuffer,0,ND_LOG_BUFFER_SIZE);

#ifdef ND_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\NexDomeLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/NexDomeLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/NexDomeLog.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

}

CNexDome::~CNexDome()
{
#ifdef	ND_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif

}

int CNexDome::Connect(const char *pszPort)
{
    int nErr;
    
#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::Connect Called %s\n", timestamp, pszPort);
    fflush(Logfile);
#endif

    // 9600 8N1
    nErr = m_pSerx->open(pszPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if(nErr) {
        m_bIsConnected = false;
        return nErr;
    }
    m_bIsConnected = true;
    m_bCalibrating = false;
    m_bUnParking = false;
    m_bHomed = false;

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::Connect connected to %s\n", timestamp, pszPort);
    fflush(Logfile);
#endif

    // the arduino take over a second to start as it need to init the XBee and there is 1100 ms pause in the code :(
    if(m_pSleeper)
        m_pSleeper->sleep(2000);

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::Connect Getting Firmware\n", timestamp);
    fflush(Logfile);
#endif

    // if this fails we're not properly connected.
    nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::Connect] Error Getting Firmware.\n", timestamp);
        fflush(Logfile);
#endif
        m_bIsConnected = false;
        m_pSerx->close();
        return FIRMWARE_NOT_SUPPORTED;
    }

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::Connect Got Firmware %s ( %f )\n", timestamp, m_szFirmwareVersion, m_fVersion);
    fflush(Logfile);
#endif
    if(m_fVersion < 2.0f && m_fVersion != 0.523f && m_fVersion != 0.522f)  {
        return FIRMWARE_NOT_SUPPORTED;
    }

    nErr = getDomeParkAz(m_dCurrentAzPosition);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::Connect getDomeParkAz nErr : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    nErr = getDomeHomeAz(m_dHomeAz);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::Connect getDomeHomeAz nErr : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    sendShutterHello();

    return SB_OK;
}


void CNexDome::Disconnect()
{
    if(m_bIsConnected) {
        abortCurrentCommand();
        m_pSerx->purgeTxRx();
        m_pSerx->close();
    }
    m_bIsConnected = false;
    m_bCalibrating = false;
    m_bUnParking = false;
    m_bHomed = false;

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::Disconnect] m_bIsConnected = %d\n", timestamp, m_bIsConnected);
    fflush(Logfile);
#endif
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
#if defined ND_DEBUG && ND_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CNexDome::readResponse] readFile error\n", timestamp);
            fflush(Logfile);
#endif
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
#if defined ND_DEBUG && ND_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] CNexDome::readResponse Timeout while waiting for response from controller\n", timestamp);
            fflush(Logfile);
#endif

            nErr = ND_BAD_CMD_RESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
    } while (*pszBufPtr++ != '#' && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the #

    return nErr;
}


int CNexDome::domeCommand(const char *pszCmd, char *pszResult, char respCmdCode, int nResultMaxLen)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;

    m_pSerx->purgeTxRx();

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::domeCommand sending : %s\n", timestamp, pszCmd);
    fflush(Logfile);
#endif

    nErr = m_pSerx->writeFile((void *)pszCmd, strlen(pszCmd), ulBytesWrite);
    m_pSerx->flushTx();
    if(nErr)
        return nErr;

    if (!respCmdCode)
        return nErr;

    // read response
    nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::domeCommand ***** ERROR READING RESPONSE **** error = %d , response : %s\n", timestamp, nErr, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }
#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::domeCommand response : %s\n", timestamp, szResp);
    fflush(Logfile);
#endif


    if (szResp[0] != respCmdCode)
        nErr = ND_BAD_CMD_RESPONSE;

    if(pszResult)
        strncpy(pszResult, &szResp[1], nResultMaxLen);

    return nErr;

}

int CNexDome::getDomeAz(double &dDomeAz)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    nErr = domeCommand("g#", szResp, 'g', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getDomeAz] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }
    // convert Az string to double
    dDomeAz = atof(szResp);
    m_dCurrentAzPosition = dDomeAz;

    return nErr;
}

int CNexDome::getDomeEl(double &dDomeEl)
{
    int nErr = ND_OK;
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
    else {
        dDomeEl = 90.0;
        return nErr;
    }

    nErr = domeCommand("G#", szResp, 'G', SERIAL_BUFFER_SIZE); // this doesn't work in firmware 2.0.0.0 as it return just 'G.#'
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getDomeEl] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }
    
    // convert El string to double
    dDomeEl = atof(szResp);
    m_dCurrentElPosition = dDomeEl;
    
    return nErr;
}


int CNexDome::getDomeHomeAz(double &dAz)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    nErr = domeCommand("i#", szResp, 'i', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getDomeHomeAz] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    // convert Az string to double
    dAz = atof(szResp);
    m_dHomeAz = dAz;
    return nErr;
}

int CNexDome::getDomeParkAz(double &dAz)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    nErr = domeCommand("l#", szResp, 'l', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getDomeParkAz] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    // convert Az string to double
    dAz = atof(szResp);
    m_dParkAz = dAz;
    return nErr;
}


int CNexDome::getShutterState(int &nState)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> shutterStateFileds;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;
	
	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
	
    nErr = domeCommand("M#", szResp, 'M', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getShutterState] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getShutterState] response = '%s'\n", timestamp, szResp);
    fflush(Logfile);
#endif

    nState = atoi(szResp);

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getShutterState] nState = '%d'\n", timestamp, nState);
    fflush(Logfile);
#endif

    return nErr;
}


int CNexDome::getDomeStepPerRev(int &nStepPerRev)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = domeCommand("t#", szResp, 't', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getDomeStepPerRev] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    nStepPerRev = atoi(szResp);
    m_nNbStepPerRev = nStepPerRev;
    return nErr;
}

int CNexDome::setDomeStepPerRev(int nStepPerRev)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    m_nNbStepPerRev = nStepPerRev;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    snprintf(szBuf, SERIAL_BUFFER_SIZE, "t%d#", nStepPerRev);
    nErr = domeCommand(szBuf, szResp, 'i', SERIAL_BUFFER_SIZE);
    return nErr;

}

int CNexDome::getBatteryLevels(double &domeVolts, double &dDomeCutOff, double &dShutterVolts, double &dShutterCutOff)
{
    int nErr = ND_OK;
    int rc = 0;
    char szResp[SERIAL_BUFFER_SIZE];

    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;
    // Dome
    nErr = domeCommand("k#", szResp, 'k', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    rc = sscanf(szResp, "%lf,%lf", &domeVolts, &dDomeCutOff);
    if(rc == 0) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] sscanf ERROR\n", timestamp);
        fflush(Logfile);
#endif
        return COMMAND_FAILED;
    }
#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] domeVolts = %f\n", timestamp, domeVolts);
    fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] dDomeCutOff = %f\n", timestamp, dDomeCutOff);
    fflush(Logfile);
#endif

    // do a proper conversion as for some reason the value is scaled weirdly ( it's multiplied by 3/2)
    // Arduino ADC convert 0-5V to 0-1023 which is 0.0049V per unit
    if(m_fVersion < 2.0f) {
        domeVolts = (domeVolts*2) * 0.0049f;
        dDomeCutOff = (dDomeCutOff*2) * 0.0049f;
    } else { // hopefully we can fix this in the next firmware and report proper values.
        domeVolts = domeVolts / 100.0;
        dDomeCutOff = dDomeCutOff / 100.0;
    }

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] domeVolts = %f\n", timestamp, domeVolts);
    fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] dDomeCutOff = %f\n", timestamp, dDomeCutOff);
    fflush(Logfile);
#endif

    //  Shutter
	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
    nErr = domeCommand("K#", szResp, 'K', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    if(strlen(szResp)<2) { // no shutter value
        dShutterVolts = -1;
        dShutterCutOff = -1;
        return nErr;
    }

    rc = sscanf(szResp, "%lf,%lf", &dShutterVolts, &dShutterCutOff);
    if(rc == 0) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] sscanf ERROR\n", timestamp);
        fflush(Logfile);
#endif
        return COMMAND_FAILED;
    }

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] shutterVolts = %f\n", timestamp, dShutterVolts);
    fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] dShutterCutOff = %f\n", timestamp, dShutterCutOff);
    fflush(Logfile);
#endif

    // do a proper conversion as for some reason the value is scaled weirdly ( it's multiplied by 3/2)
    // Arduino ADC convert 0-5V to 0-1023 which is 0.0049V per unit
    if(m_fVersion < 2.0f) {
        dShutterVolts = (dShutterVolts*2) * 0.0049f;
        dShutterCutOff = (dShutterCutOff*2) * 0.0049f;
    } else { // hopefully we can fix this in the next firmware and report proper values.
        dShutterVolts = dShutterVolts / 100.0;
        dShutterCutOff = dShutterCutOff / 100.0;
    }
#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] shutterVolts = %f\n", timestamp, dShutterVolts);
    fprintf(Logfile, "[%s] [CNexDome::getBatteryLevels] dShutterCutOff = %f\n", timestamp, dShutterCutOff);
    fflush(Logfile);
#endif

    return nErr;
}

int CNexDome::setBatteryCutOff(double dDomeCutOff, double dShutterCutOff)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];
    int nRotCutOff, nShutCutOff;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    if(m_fVersion < 2.0f) {
        nRotCutOff =  int(dDomeCutOff/0.0049f)/2;
        nShutCutOff =  int(dShutterCutOff/0.0049f)/2;
    }
    else {
        nRotCutOff = dDomeCutOff * 100.0;
        nShutCutOff = dShutterCutOff * 100.0;

    }

    // Dome
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "k%d#", nRotCutOff);
    nErr = domeCommand(szBuf, szResp, 'k', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::setBatteryCutOff] dDomeCutOff ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    // Shutter
	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "K%d#", nShutCutOff);
    nErr = domeCommand(szBuf, szResp, 'K', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::setBatteryCutOff] dShutterCutOff ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    return nErr;
}

int CNexDome::getPointingError(double &dPointingError)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return nErr;

    nErr = domeCommand("o#", szResp, 'o', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getPointingError] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    // convert Az string to double
    dPointingError = atof(szResp);
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
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = domeCommand("m#", szResp, 'm', SERIAL_BUFFER_SIZE);
    if(nErr & !m_bCalibrating) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::isDomeMoving] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return false;
    }
    else if (nErr & m_bCalibrating) {
        return true;
    }

    bIsMoving = false;
    nTmp = atoi(szResp);
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isDomeMoving nTmp : %d\n", timestamp, nTmp);
    fflush(Logfile);
#endif
    if(nTmp)
        bIsMoving = true;

#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isDomeMoving bIsMoving : %s\n", timestamp, bIsMoving?"True":"False");
    fflush(Logfile);
#endif

    return bIsMoving;
}

bool CNexDome::isDomeAtHome()
{
    bool bAthome;
    int nTmp;
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;
    
    nErr = domeCommand("z#", szResp, 'z', SERIAL_BUFFER_SIZE);
#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::isDomeAtHome] z# response = %s\n", timestamp, szResp);
    fflush(Logfile);
#endif
    if(nErr) {
        return false;
    }

    bAthome = false;
    nTmp = atoi(szResp);
#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::isDomeAtHome] nTmp = %d\n", timestamp, nTmp);
    fflush(Logfile);
#endif
    if(nTmp == ATHOME)
        bAthome = true;
#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isDomeAtHome bAthome : %s\n", timestamp, bAthome?"True":"False");
    fflush(Logfile);
#endif

    return bAthome;
  
}

int CNexDome::syncDome(double dAz, double dEl)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_dCurrentAzPosition = dAz;
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "s%3.2f#", dAz);
    nErr = domeCommand(szBuf, szResp, 's', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::syncDome] ERROR = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    // TODO : Also set Elevation when supported by the firmware.
    // m_dCurrentElPosition = dEl;
    return nErr;
}

int CNexDome::parkDome()
{
    int nErr = ND_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bHomeOnPark) {
        m_bParking = true;
        nErr = goHome();
    } else
        nErr = gotoAzimuth(m_dParkAz);

    return nErr;

}

int CNexDome::unparkDome()
{
    if(!m_bHasBeenHome && m_bHomeOnUnpark) {
        m_bUnParking = true;
        goHome();
    }
    else if(m_bHomeOnUnpark) {
        m_bUnParking = true;
        goHome();
    }
    else {
        syncDome(m_dParkAz, m_dCurrentElPosition);
        m_bParked = false;
        m_bUnParking = false;
    }

    return 0;
}

int CNexDome::gotoAzimuth(double dNewAz)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;


    while(dNewAz >= 360)
        dNewAz = dNewAz - 360;
    
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "g%3.2f#", dNewAz);
    nErr = domeCommand(szBuf, szResp, 'g', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::gotoAzimuth] ERROR = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    m_dGotoAz = dNewAz;
    m_nGotoTries = 0;
    return nErr;
}

int CNexDome::openShutter()
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return SB_OK;

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::openShutter] Opening shutter\n", timestamp);
    fflush(Logfile);
#endif

    if (m_bDebugLog) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::openShutter] Opening shutter");
        m_pLogger->out(m_szLogBuffer);
    }

	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
    nErr = domeCommand("O#", szResp, 'O', SERIAL_BUFFER_SIZE);
    if(nErr) {
        snprintf(m_szLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::openShutter] ERROR gotoAzimuth");
        m_pLogger->out(m_szLogBuffer);
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::openShutter] ERROR = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
    }

    return nErr;
}

int CNexDome::closeShutter()
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return SB_OK;

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::closeShutter] Closing shutter\n", timestamp);
    fflush(Logfile);
#endif

	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
    nErr = domeCommand("C#", szResp, 'C', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::openShutter] closeShutter = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
    }

    return nErr;
}

int CNexDome::getFirmwareVersion(char *szVersion, int nStrMaxLen)
{
    int nErr = ND_OK;
    int i;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> firmwareFields;
    std::vector<std::string> versionFields;
    std::string strVersion;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating)
        return SB_OK;

    nErr = domeCommand("v#", szResp, 'v', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::getFirmwareVersion] ERROR = %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
        return nErr;
    }

    nErr = parseFields(szResp,firmwareFields, 'v');
    if(nErr) {
        strncpy(szVersion, szResp, nStrMaxLen);
        m_fVersion = atof(szResp);
        return ND_OK;
    }

    nErr = parseFields(firmwareFields[0].c_str(),versionFields, '.');
    if(versionFields.size()>1) {
        strVersion=versionFields[0]+".";
        for(i=1; i<versionFields.size(); i++) {
            strVersion+=versionFields[i];
        }
        strncpy(szVersion, szResp, nStrMaxLen);
        m_fVersion = atof(strVersion.c_str());
    }
    else {
        strncpy(szVersion, szResp, nStrMaxLen);
        m_fVersion = atof(szResp);
    }
    return nErr;
}

int CNexDome::getFirmwareVersion(float &fVersion)
{
    int nErr = ND_OK;

    if(m_fVersion == 0.0f) {
        nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
        if(nErr)
            return nErr;
    }

    fVersion = m_fVersion;

    return nErr;
}

int CNexDome::goHome()
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(m_bCalibrating) {
        return SB_OK;
    }
    else if(isDomeAtHome()){
            m_bHomed = true;
            return ND_OK;
    }
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::goHome \n", timestamp);
    fflush(Logfile);
#endif

    m_nHomingTries = 0;
    nErr = domeCommand("h#", szResp, 'h', SERIAL_BUFFER_SIZE);
    if(nErr) {
#ifdef ND_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::goHome ERROR = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    return nErr;
}

int CNexDome::calibrate()
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;


    nErr = domeCommand("c#", szResp, 'c', SERIAL_BUFFER_SIZE);
    if(nErr) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::calibrate] ERROR = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    m_bCalibrating = true;
    
    return nErr;
}

int CNexDome::isGoToComplete(bool &bComplete)
{
    int nErr = ND_OK;
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

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isGoToComplete DomeAz = %3.2f\n", timestamp, dDomeAz);
    fflush(Logfile);
#endif

    // we need to test "large" depending on the heading error , this is new in firmware 1.10 and up
    if ((ceil(m_dGotoAz) <= ceil(dDomeAz)+3) && (ceil(m_dGotoAz) >= ceil(dDomeAz)-3)) {
        bComplete = true;
        m_nGotoTries = 0;
    }
    else {
        // we're not moving and we're not at the final destination !!!
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::isGoToComplete ***** ERROR **** domeAz = %3.2f, m_dGotoAz = %3.2f\n", timestamp, dDomeAz, m_dGotoAz);
        fflush(Logfile);
#endif
        if(m_nGotoTries == 0) {
            bComplete = false;
            m_nGotoTries = 1;
            gotoAzimuth(m_dGotoAz);
        }
        else {
            m_nGotoTries = 0;
            nErr = ERR_CMDFAILED;
        }
    }

    return nErr;
}

int CNexDome::isOpenComplete(bool &bComplete)
{
    int nErr = ND_OK;
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

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isOpenComplete bComplete = %s\n", timestamp, bComplete?"True":"False");
    fflush(Logfile);
#endif

    return nErr;
}

int CNexDome::isCloseComplete(bool &bComplete)
{
    int nErr = ND_OK;
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

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isCloseComplete bComplete = %s\n", timestamp, bComplete?"True":"False");
    fflush(Logfile);
#endif

    return nErr;
}


int CNexDome::isParkComplete(bool &bComplete)
{
    int nErr = ND_OK;
    double dDomeAz=0;
    bool bFoundHome;

    if(!m_bIsConnected)
        return NOT_CONNECTED;
#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isParkComplete m_bParking = %s\n", timestamp, m_bParking?"True":"False");
    fprintf(Logfile, "[%s] CNexDome::isParkComplete bComplete = %s\n", timestamp, bComplete?"True":"False");
    fflush(Logfile);
#endif

    if(isDomeMoving()) {
        getDomeAz(dDomeAz);
        bComplete = false;
        return nErr;
    }

    if(m_bParking) {
        bComplete = false;
        nErr = isFindHomeComplete(bFoundHome);
        if(bFoundHome) { // we're home, now park
#if defined ND_DEBUG && ND_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] CNexDome::isParkComplete found home, now parking\n", timestamp);
            fflush(Logfile);
#endif
            m_bParking = false;
            nErr = gotoAzimuth(m_dParkAz);
        }
        return nErr;
    }

    getDomeAz(dDomeAz);

    // we need to test "large" depending on the heading error
    if ((ceil(m_dParkAz) <= ceil(dDomeAz)+3) && (ceil(m_dParkAz) >= ceil(dDomeAz)-3)) {
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

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isParkComplete bComplete = %s\n", timestamp, bComplete?"True":"False");
    fflush(Logfile);
#endif

    return nErr;
}

int CNexDome::isUnparkComplete(bool &bComplete)
{
    int nErr = ND_OK;

    bComplete = false;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(!m_bParked) {
        bComplete = true;
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::isUnparkComplete UNPARKED \n", timestamp);
        fflush(Logfile);
#endif
    }
    else if (m_bUnParking) {
#if defined ND_DEBUG && ND_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::isUnparkComplete unparking.. checking if we're home \n", timestamp);
        fflush(Logfile);
#endif
        nErr = isFindHomeComplete(bComplete);
        if(nErr)
            return nErr;
        if(bComplete) {
            m_bParked = false;
        }
        else {
            m_bParked = true;
        }
    }

#if defined ND_DEBUG && ND_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isUnparkComplete m_bParked = %s\n", timestamp, m_bParked?"True":"False");
    fprintf(Logfile, "[%s] CNexDome::isUnparkComplete bComplete = %s\n", timestamp, bComplete?"True":"False");
    fflush(Logfile);
#endif

    return nErr;
}

int CNexDome::isFindHomeComplete(bool &bComplete)
{
    int nErr = ND_OK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CNexDome::isFindHomeComplete\n", timestamp);
    fflush(Logfile);
#endif

    if(isDomeMoving()) {
        m_bHomed = false;
        bComplete = false;
#ifdef ND_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::isFindHomeComplete still moving\n", timestamp);
        fflush(Logfile);
#endif
        return nErr;

    }

    if(isDomeAtHome()){
        m_bHomed = true;
        bComplete = true;
        if(m_bUnParking)
            m_bParked = false;
        syncDome(m_dHomeAz, m_dCurrentElPosition);
        m_bHasBeenHome = true;
        m_nHomingTries = 0;
#ifdef ND_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CNexDome::isFindHomeComplete At Home\n", timestamp);
        fflush(Logfile);
#endif
    }
    else {
        // we're not moving and we're not at the home position !!!
#ifdef ND_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::isFindHomeComplete] Not moving and not at home !!!\n", timestamp);
        fflush(Logfile);
#endif
        bComplete = false;
        m_bHomed = false;
        m_bParked = false;
        // sometimes we pass the home sensor and the dome doesn't rotate back enough to detect it.
        // this is mostly the case with firmware 1.10 with the new error correction ... 
        // so give it another try
        if(m_nHomingTries == 0) {
            m_nHomingTries = 1;
            goHome();
        }
    }

    return nErr;
}


int CNexDome::isCalibratingComplete(bool &bComplete)
{
    int nErr = ND_OK;
    double dDomeAz = 0;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        // getDomeAz(dDomeAz);
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
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getNbTicksPerRev] final m_nNbStepPerRev = %d\n", timestamp, m_nNbStepPerRev);
    fprintf(Logfile, "[%s] [CNexDome::getNbTicksPerRev] final m_bHomed = %s\n", timestamp, m_bHomed?"True":"False");
    fprintf(Logfile, "[%s] [CNexDome::getNbTicksPerRev] final m_bCalibrating = %s\n", timestamp, m_bCalibrating?"True":"False");
    fprintf(Logfile, "[%s] [CNexDome::getNbTicksPerRev] final bComplete = %s\n", timestamp, bComplete?"True":"False");
    fflush(Logfile);
#endif
    return nErr;
}


int CNexDome::abortCurrentCommand()
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    m_bCalibrating = false;
    m_bUnParking = false;
    nErr = domeCommand("a#", szResp, 'a', SERIAL_BUFFER_SIZE);

    getDomeAz(m_dGotoAz);

    return nErr;
}

int CNexDome::sendShutterHello()
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
	if(m_fVersion>=2.0f)
        nErr = domeCommand("H#", szResp, 'H', SERIAL_BUFFER_SIZE);
    else
        nErr = domeCommand("H#", NULL, 0, SERIAL_BUFFER_SIZE);
    return nErr;
}
#pragma mark - Getter / Setter

int CNexDome::getNbTicksPerRev()
{
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getNbTicksPerRev] m_bIsConnected = %s\n", timestamp, m_bIsConnected?"True":"False");
    fflush(Logfile);
#endif

    if(m_bIsConnected)
        getDomeStepPerRev(m_nNbStepPerRev);

#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getNbTicksPerRev] m_nNbStepPerRev = %d\n", timestamp, m_nNbStepPerRev);
    fflush(Logfile);
#endif

    return m_nNbStepPerRev;
}

int CNexDome::setNbTicksPerRev(int nSteps)
{
    int nErr = ND_OK;

    if(m_bIsConnected)
        nErr = setDomeStepPerRev(nSteps);
    return nErr;
}

double CNexDome::getHomeAz()
{
    if(m_bIsConnected)
        getDomeHomeAz(m_dHomeAz);
    return m_dHomeAz;
}

int CNexDome::setHomeAz(double dAz)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    m_dHomeAz = dAz;
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;
    
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "i%3.2f#", dAz);
    nErr = domeCommand(szBuf, szResp, 'i', SERIAL_BUFFER_SIZE);
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
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    m_dParkAz = dAz;
    
    if(!m_bIsConnected)
        return NOT_CONNECTED;

    snprintf(szBuf, SERIAL_BUFFER_SIZE, "l%3.2f#", dAz);
    nErr = domeCommand(szBuf, szResp, 'l', SERIAL_BUFFER_SIZE);
    return nErr;
}


double CNexDome::getCurrentAz()
{

    if(m_bIsConnected) {
        getDomeAz(m_dCurrentAzPosition);
   }
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


int CNexDome::getDefaultDir(bool &bNormal)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    bNormal = true;
    nErr = domeCommand("y#", szResp, 'y', SERIAL_BUFFER_SIZE);
    if(nErr) {
        return nErr;
    }

    bNormal = atoi(szResp) ? false:true;
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getDefaultDir] bNormal =  %s\n", timestamp, bNormal?"True":"False");
    fflush(Logfile);
#endif


    return nErr;
}

int CNexDome::setDefaultDir(bool bNormal)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    snprintf(szBuf, SERIAL_BUFFER_SIZE, "y %1d#", bNormal?0:1);

#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::setDefaultDir] bNormal =  %s\n", timestamp, bNormal?"True":"False");
    fprintf(Logfile, "[%s] [CNexDome::setDefaultDir] szBuf =  %s\n", timestamp, szBuf);
    fflush(Logfile);
#endif

    nErr = domeCommand(szBuf, szResp, 'y', SERIAL_BUFFER_SIZE);
    return nErr;

}

int CNexDome::getRainSensorStatus(int &nStatus)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    nStatus = NOT_RAINING;
    nErr = domeCommand("F#", szResp, 'F', SERIAL_BUFFER_SIZE);
    if(nErr) {
        return nErr;
    }

    nStatus = atoi(szResp) ? false:true;
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getRainSensorStatus] nStatus =  %s\n", timestamp, nStatus?"NOT RAINING":"RAINING");
    fflush(Logfile);
#endif


    m_nIsRaining = nStatus;
    return nErr;
}

int CNexDome::getRotationSpeed(int &nSpeed)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = domeCommand("r#", szResp, 'r', SERIAL_BUFFER_SIZE);
    if(nErr) {
        return nErr;
    }

    nSpeed = atoi(szResp);
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getRotationSpeed] nSpeed =  %d\n", timestamp, nSpeed);
    fflush(Logfile);
#endif

    return nErr;
}

int CNexDome::setRotationSpeed(int nSpeed)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    snprintf(szBuf, SERIAL_BUFFER_SIZE, "r%d#", nSpeed);
    nErr = domeCommand(szBuf, szResp, 'r', SERIAL_BUFFER_SIZE);
    return nErr;
}


int CNexDome::getRotationAcceleration(int &nAcceleration)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = domeCommand("e#", szResp, 'e', SERIAL_BUFFER_SIZE);
    if(nErr) {
        return nErr;
    }

    nAcceleration = atoi(szResp);
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getRotationAcceleration] nAcceleration =  %d\n", timestamp, nAcceleration);
    fflush(Logfile);
#endif

    return nErr;
}

int CNexDome::setRotationAcceleration(int nAcceleration)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    snprintf(szBuf, SERIAL_BUFFER_SIZE, "e%d#", nAcceleration);
    nErr = domeCommand(szBuf, szResp, 'e', SERIAL_BUFFER_SIZE);

    return nErr;
}

int CNexDome::getShutterSpeed(int &nSpeed)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
    nErr = domeCommand("R#", szResp, 'R', SERIAL_BUFFER_SIZE);
    if(nErr) {
        return nErr;
    }

    nSpeed = atoi(szResp);
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getShutterSpeed] nSpeed =  %d\n", timestamp, nSpeed);
    fflush(Logfile);
#endif

    return nErr;
}

int CNexDome::setShutterSpeed(int nSpeed)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "R%d#", nSpeed);
    nErr = domeCommand(szBuf, szResp, 'R', SERIAL_BUFFER_SIZE);

    return nErr;
}

int CNexDome::getShutterAcceleration(int &nAcceleration)
{
    int nErr = ND_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
    nErr = domeCommand("E#", szResp, 'E', SERIAL_BUFFER_SIZE);
    if(nErr) {
        return nErr;
    }

    nAcceleration = atoi(szResp);
#ifdef ND_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CNexDome::getShutterAcceleration] nAcceleration =  %d\n", timestamp, nAcceleration);
    fflush(Logfile);
#endif
    return nErr;
}

int CNexDome::setShutterAcceleration(int nAcceleration)
{
    int nErr = ND_OK;
    char szBuf[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

	m_pSleeper->sleep(INTER_COMMAND_PAUSE_MS);
    snprintf(szBuf, SERIAL_BUFFER_SIZE, "E%d#", nAcceleration);
    nErr = domeCommand(szBuf, szResp, 'E', SERIAL_BUFFER_SIZE);
    return nErr;
}

void CNexDome::setHomeOnPark(const bool bEnabled)
{
    m_bHomeOnPark = bEnabled;
}

void CNexDome::setHomeOnUnpark(const bool bEnabled)
{
    m_bHomeOnUnpark = bEnabled;
}

int CNexDome::parseFields(const char *pszResp, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = ND_OK;
    std::string sSegment;
    if(!pszResp) {
#ifdef ND_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::setDefaultDir] pszResp is NULL\n", timestamp);
        fflush(Logfile);
#endif
        return ERR_CMDFAILED;
    }

    if(!strlen(pszResp)) {
#ifdef ND_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CNexDome::setDefaultDir] pszResp is enpty\n", timestamp);
        fflush(Logfile);
#endif
        return ERR_CMDFAILED;
    }
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

