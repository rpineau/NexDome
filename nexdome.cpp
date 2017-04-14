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
    bDebugLog = true;
    
    pSerx = NULL;
    bIsConnected = false;

    mNbStepPerRev = 0;
    mShutterBatteryVolts = 0.0;
    
    mHomeAz = 180;
    mParkAz = 180;

    mCurrentAzPosition = 0.0;
    mCurrentElPosition = 0.0;

    bCalibrating = false;
    
    mShutterOpened = false;
    
    mParked = true;
    mHomed = false;
    memset(firmwareVersion,0,SERIAL_BUFFER_SIZE);
    memset(mLogBuffer,0,ND_LOG_BUFFER_SIZE);
}

CNexDome::~CNexDome()
{

}

int CNexDome::Connect(const char *szPort)
{
    int err;
    
    // 9600 8N1
    if(pSerx->open(szPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        bIsConnected = true;
    else
        bIsConnected = false;

    if(!bIsConnected)
        return ERR_COMMNOLINK;

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::Connect] Connected.");
        mLogger->out(mLogBuffer);

        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::Connect] Getting Firmware.");
        mLogger->out(mLogBuffer);
    }
    // if this fails we're not properly connected.
    err = getFirmwareVersion(firmwareVersion, SERIAL_BUFFER_SIZE);
    if(err) {
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::Connect] Error Getting Firmware.");
            mLogger->out(mLogBuffer);
        }
        bIsConnected = false;
        pSerx->close();
        return FIRMWARE_NOT_SUPPORTED;
    }

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::Connect] Got Firmware %s", firmwareVersion);
        mLogger->out(mLogBuffer);
    }
    // assume the dome was parked
    getDomeParkAz(mCurrentAzPosition);

    syncDome(mCurrentAzPosition,mCurrentElPosition);

    return SB_OK;
}


void CNexDome::Disconnect()
{
    if(bIsConnected) {
        pSerx->purgeTxRx();
        pSerx->close();
    }
    bIsConnected = false;
}


int CNexDome::readResponse(char *respBuffer, int bufferLen)
{
    int err = ND_OK;
    unsigned long nBytesRead = 0;
    unsigned long totalBytesRead = 0;
    char *bufPtr;

    memset(respBuffer, 0, (size_t) bufferLen);
    bufPtr = respBuffer;

    do {
        err = pSerx->readFile(bufPtr, 1, nBytesRead, MAX_TIMEOUT);
        if(err) {
            if (bDebugLog) {
                snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::readResponse] readFile error.");
                mLogger->out(mLogBuffer);
            }
            return err;
        }

        if (nBytesRead !=1) {// timeout
            if (bDebugLog) {
                snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::readResponse] readFile Timeout.");
                mLogger->out(mLogBuffer);
            }
            err = ND_BAD_CMD_RESPONSE;
            break;
        }
        totalBytesRead += nBytesRead;
    } while (*bufPtr++ != '\n' && totalBytesRead < bufferLen );

    *bufPtr = 0; //remove the \n
    return err;
}


int CNexDome::domeCommand(const char *cmd, char *result, char respCmdCode, int resultMaxLen)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    pSerx->purgeTxRx();
    //if (bDebugLog) {
    //  snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::domeCommand] Sending %s",cmd);
    //    mLogger->out(mLogBuffer);
    //}
    err = pSerx->writeFile((void *)cmd, strlen(cmd), nBytesWrite);
    pSerx->flushTx();
    if(err)
        return err;
    // read response
    err = readResponse(resp, SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    //if (bDebugLog) {
    //    snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::domeCommand] response = %s", resp);
    //    mLogger->out(mLogBuffer);
    //}

    if(resp[0] != respCmdCode)
        err = ND_BAD_CMD_RESPONSE;

    if(result)
        strncpy(result, &resp[1], resultMaxLen);

    return err;

}

int CNexDome::getDomeAz(double &domeAz)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return err;

    err = domeCommand("q\n", resp, 'Q', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeAz] ERROR = %s", resp);
        mLogger->out(mLogBuffer);
        return err;
    }
    // convert Az string to double
    domeAz = atof(resp);
    mCurrentAzPosition = domeAz;

    return err;
}

int CNexDome::getDomeEl(double &domeEl)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return err;

    if(!mShutterOpened)
    {
        domeEl = 0.0;
        return err;
    }
        
    err = domeCommand("b\n", resp, 'B', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeEl] ERROR = %s", resp);
        mLogger->out(mLogBuffer);
        return err;
    }
    
    // convert El string to double
    domeEl = atof(resp);
    mCurrentElPosition = domeEl;
    
    return err;
}


int CNexDome::getDomeHomeAz(double &Az)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return err;

    err = domeCommand("i\n", resp, 'I', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeHomeAz] ERROR = %s", resp);
        mLogger->out(mLogBuffer);
        return err;
    }

    // convert Az string to double
    Az = atof(resp);
    mHomeAz = Az;
    return err;
}

int CNexDome::getDomeParkAz(double &Az)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return err;

    err = domeCommand("n\n", resp, 'N', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeParkAz] ERROR = %s", resp);
        mLogger->out(mLogBuffer);
        return err;
    }

    // convert Az string to double
    Az = atof(resp);
    mParkAz = Az;
    return err;
}


int CNexDome::getShutterState(int &state)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return err;

    err = domeCommand("u\n", resp, 'U', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getShutterState] ERROR = %s", resp);
        mLogger->out(mLogBuffer);
        return err;
    }

    state = atoi(resp);

    return err;
}


int CNexDome::getDomeStepPerRev(int &stepPerRev)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    err = domeCommand("t\n", resp, 'T', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getDomeStepPerRev] ERROR = %s", resp);
        mLogger->out(mLogBuffer);
        return err;
    }

    stepPerRev = atoi(resp);
    mNbStepPerRev = stepPerRev;
    return err;
}

int CNexDome::getBatteryLevels(double &domeVolts, double &shutterVolts)
{
    int err = 0;
    int rc = 0;
    char resp[SERIAL_BUFFER_SIZE];

    
    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return err;
    
    err = domeCommand("k\n", resp, 'K', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getBatteryLevels] ERROR = %s", resp);
        mLogger->out(mLogBuffer);
        return err;
    }

    rc = sscanf(resp, "%lf %lf", &domeVolts, &shutterVolts);
    if(rc == 0) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::getBatteryLevels] sscanf ERROR ");
        mLogger->out(mLogBuffer);
        return COMMAND_FAILED;
    }

    domeVolts = domeVolts / 100.0;
    shutterVolts = shutterVolts / 100.0;
    return err;
}

void CNexDome::setDebugLog(bool enable)
{
    bDebugLog = enable;
}

bool CNexDome::isDomeMoving()
{
    bool isMoving;
    int tmp;
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    err = domeCommand("m\n", resp, 'M', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isDomeMoving] isDomeMoving = %s", resp);
        mLogger->out(mLogBuffer);
        return false;
    }

    isMoving = false;
    tmp = atoi(resp);
    if(tmp)
        isMoving = true;

    return isMoving;
}

bool CNexDome::isDomeAtHome()
{
    bool athome;
    int tmp;
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];
    
    if(!bIsConnected)
        return NOT_CONNECTED;
    
    err = domeCommand("z\n", resp, 'Z', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isDomeAtHome] ERROR isDomeAtHome = %s", resp);
        mLogger->out(mLogBuffer);
        return false;
    }

    athome = false;
    tmp = atoi(resp);
    if(tmp)
        athome = true;
    
    return athome;
  
}

int CNexDome::syncDome(double dAz, double dEl)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    mCurrentAzPosition = dAz;
    snprintf(buf, SERIAL_BUFFER_SIZE, "s %3.2f\n", dAz);
    err = domeCommand(buf, NULL, 'S', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::syncDome] ERROR syncDome ");
        mLogger->out(mLogBuffer);
    }
    // TODO : Also set Elevation when supported by the firware.
    // mCurrentElPosition = dEl;
    return err;
}

int CNexDome::parkDome()
{
    int err = 0;

    if(!bIsConnected)
        return NOT_CONNECTED;

    err = gotoAzimuth(mParkAz);
    return err;

}

int CNexDome::unparkDome()
{
    mParked = false;
    mCurrentAzPosition = mParkAz;
    syncDome(mCurrentAzPosition,mCurrentElPosition);
    return 0;
}

int CNexDome::gotoAzimuth(double newAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;


    while(newAz >= 360)
        newAz = newAz - 360;
    
    snprintf(buf, SERIAL_BUFFER_SIZE, "g %3.2f\n", newAz);
    err = domeCommand(buf, NULL, 'G', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::gotoAzimuth] ERROR gotoAzimuth");
        mLogger->out(mLogBuffer);
        return err;
    }

    mGotoAz = newAz;
    return err;
}

int CNexDome::openShutter()
{
    int err = 0;
    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return SB_OK;

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::openShutter] Opening shutter");
        mLogger->out(mLogBuffer);
    }
    err = domeCommand("d\n", NULL, 'D', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::openShutter] ERROR gotoAzimuth");
        mLogger->out(mLogBuffer);
    }

    return err;
}

int CNexDome::closeShutter()
{
    int err = 0;
    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return SB_OK;

    if (bDebugLog) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::closeShutter] Closing shutter");
        mLogger->out(mLogBuffer);
    }
    err = domeCommand("e\n", NULL, 'D', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::closeShutter] ERROR Closing shutter");
        mLogger->out(mLogBuffer);
    }

    return err;
}


int CNexDome::getFirmwareVersion(char *version, int strMaxLen)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return SB_OK;

    err = domeCommand("v\n", resp, 'V', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::closeShutter] ERROR getFirmwareVersion = %s", resp);
        mLogger->out(mLogBuffer);
        return err;
    }
    
    strncpy(version, resp, strMaxLen);
    return err;
}

int CNexDome::goHome()
{
    int err = 0;
    if(!bIsConnected)
        return NOT_CONNECTED;

    if(bCalibrating)
        return SB_OK;
    
    err = domeCommand("h\n", NULL, 'H', SERIAL_BUFFER_SIZE);
    if(err) {
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::closeShutter] ERROR goHome");
        mLogger->out(mLogBuffer);
        return err;
    }

    return err;
}

int CNexDome::calibrate()
{
    int err = 0;
    if(!bIsConnected)
        return NOT_CONNECTED;


    err = domeCommand("c\n", NULL, 'C', SERIAL_BUFFER_SIZE);
    if(err)
        return err;
    bCalibrating = true;
    
    return err;
}

int CNexDome::isGoToComplete(bool &complete)
{
    int err = 0;
    double domeAz = 0;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        complete = false;
        getDomeAz(domeAz);
        return err;
    }

    getDomeAz(domeAz);
    if(domeAz >0 && domeAz<1)
        domeAz = 0;
    
    while(ceil(mGotoAz) >= 360)
          mGotoAz = ceil(mGotoAz) - 360;

    while(ceil(domeAz) >= 360)
        domeAz = ceil(domeAz) - 360;

    if ((ceil(mGotoAz) <= ceil(domeAz)+1) && (ceil(mGotoAz) >= ceil(domeAz)-1))
        complete = true;
    else {
        // we're not moving and we're not at the final destination !!!
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isGoToComplete] domeAz = %f, mGotoAz = %f", ceil(domeAz), ceil(mGotoAz));
            mLogger->out(mLogBuffer);
        }
        complete = false;
        err = ERR_CMDFAILED;
    }

    return err;
}

int CNexDome::isOpenComplete(bool &complete)
{
    int err=0;
    int state;

    if(!bIsConnected)
        return NOT_CONNECTED;

    err = getShutterState(state);
    if(err)
        return ERR_CMDFAILED;
    if(state == OPEN){
        mShutterOpened = true;
        complete = true;
        mCurrentElPosition = 90.0;
    }
    else {
        mShutterOpened = false;
        complete = false;
        mCurrentElPosition = 0.0;
    }

    return err;
}

int CNexDome::isCloseComplete(bool &complete)
{
    int err=0;
    int state;

    if(!bIsConnected)
        return NOT_CONNECTED;

    err = getShutterState(state);
    if(err)
        return ERR_CMDFAILED;
    if(state == CLOSED){
        mShutterOpened = false;
        complete = true;
        mCurrentElPosition = 0.0;
    }
    else {
        mShutterOpened = true;
        complete = false;
        mCurrentElPosition = 90.0;
    }

    return err;
}


int CNexDome::isParkComplete(bool &complete)
{
    int err = 0;
    double domeAz=0;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        getDomeAz(domeAz);
        complete = false;
        return err;
    }

    getDomeAz(domeAz);

    if (ceil(mParkAz) == ceil(domeAz))
    {
        mParked = true;
        complete = true;
    }
    else {
        // we're not moving and we're not at the final destination !!!
        complete = false;
        mHomed = false;
        mParked = false;
        err = ERR_CMDFAILED;
    }

    return err;
}

int CNexDome::isUnparkComplete(bool &complete)
{
    int err=0;

    if(!bIsConnected)
        return NOT_CONNECTED;

    mParked = false;
    complete = true;

    return err;
}

int CNexDome::isFindHomeComplete(bool &complete)
{
    int err = 0;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        mHomed = false;
        complete = false;
        return err;
    }

    if(isDomeAtHome()){
        mHomed = true;
        complete = true;
    }
    else {
        // we're not moving and we're not at the home position !!!
        if (bDebugLog) {
            snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isFindHomeComplete] Not moving and not at home !!!");
            mLogger->out(mLogBuffer);
        }
        complete = false;
        mHomed = false;
        mParked = false;
        err = ERR_CMDFAILED;
    }

    return err;
}


int CNexDome::isCalibratingComplete(bool &complete)
{
    int err = 0;
    double domeAz = 0;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        getDomeAz(domeAz);
        mHomed = false;
        complete = false;
        return err;
    }

    
    err = getDomeAz(domeAz);

    if (ceil(mHomeAz) != ceil(domeAz)) {
        // We need to resync the current position to the home position.
        mCurrentAzPosition = mHomeAz;
        syncDome(mCurrentAzPosition,mCurrentElPosition);
        mHomed = true;
        complete = true;
    }

    err = getDomeStepPerRev(mNbStepPerRev);
    mHomed = true;
    complete = true;
    bCalibrating = false;
    return err;
}


int CNexDome::abortCurrentCommand()
{
    if(!bIsConnected)
        return NOT_CONNECTED;

    bCalibrating = false;

    return (domeCommand("a\n", NULL, 'A', SERIAL_BUFFER_SIZE));
}

#pragma mark - Getter / Setter

int CNexDome::getNbTicksPerRev()
{
    if(bIsConnected)
        getDomeStepPerRev(mNbStepPerRev);
    return mNbStepPerRev;
}


double CNexDome::getHomeAz()
{
    if(bIsConnected)
        getDomeHomeAz(mHomeAz);
    return mHomeAz;
}

int CNexDome::setHomeAz(double dAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];

    mHomeAz = dAz;
    
    if(!bIsConnected)
        return NOT_CONNECTED;
    
    snprintf(buf, SERIAL_BUFFER_SIZE, "j %3.2f\n", dAz);
    err = domeCommand(buf, NULL, 'J', SERIAL_BUFFER_SIZE);
    return err;
}


double CNexDome::getParkAz()
{
    if(bIsConnected)
        getDomeParkAz(mParkAz);

    return mParkAz;

}

int CNexDome::setParkAz(double dAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];

    mParkAz = dAz;
    
    if(!bIsConnected)
        return NOT_CONNECTED;

    snprintf(buf, SERIAL_BUFFER_SIZE, "l %3.2f\n", dAz);
    err = domeCommand(buf, NULL, 'L', SERIAL_BUFFER_SIZE);
    return err;
}


double CNexDome::getCurrentAz()
{
    if(bIsConnected)
        getDomeAz(mCurrentAzPosition);
    
    return mCurrentAzPosition;
}

double CNexDome::getCurrentEl()
{
    if(bIsConnected)
        getDomeEl(mCurrentElPosition);
    
    return mCurrentElPosition;
}

int CNexDome::getCurrentShutterState()
{
    if(bIsConnected)
        getShutterState(mShutterState);

    return mShutterState;
}

