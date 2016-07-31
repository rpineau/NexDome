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
// #ifdef SB_MAC_BUILD
#include <unistd.h>
// #endif

CNexDome::CNexDome()
{
    // set some sane values
    pSerx = NULL;
    bIsConnected = false;

    mNbStepPerRev = 0;
    mShutterBatteryVolts = 0.0;
    
    mHomeAz = 180;
    mParkAz = 180;

    mCurrentAzPosition = 0.0;
    mCurrentElPosition = 0.0;


    mShutterOpened = false;
    
    mParked = true;
    mHomed = false;
}

CNexDome::~CNexDome()
{

}

bool CNexDome::Connect(const char *szPort)
{
    int err;
    
    // 19200 8N1
    if(pSerx->open(szPort,9600) == 0)
        bIsConnected = true;
    else
        bIsConnected = false;

    // Check to see if we can't even connect to the device
    if(!bIsConnected)
        return false;

    pSerx->purgeTxRx();
    err = getFirmwareVersion(firmwareVersion, SERIAL_BUFFER_SIZE);
    if(err)
    {
        bIsConnected = false; // if this fails we're not properly connectiong.
        pSerx->close();
    }
    // assume the dome was parked
    getDomeParkAz(mCurrentAzPosition);
    
    return bIsConnected;
}


void CNexDome::Disconnect()
{
    if(bIsConnected)
    {
        pSerx->purgeTxRx();
        pSerx->close();
    }
    bIsConnected = false;
}

int CNexDome::readResponse(char *respBuffer, int bufferLen)
{
    int err = ND_OK;
    unsigned long nBytesRead = 0;
    char *bufPtr;
    
    memset(respBuffer, 0, (size_t) bufferLen);
    bufPtr = respBuffer;
    // Look for a CR  character, until time out occurs or MAX_BUFFER characters was read
    err = pSerx->readFile(bufPtr, 1, nBytesRead, MAX_TIMEOUT);
    while (*bufPtr != '\n' && nBytesRead < bufferLen )
    {
        bufPtr++;
        err = pSerx->readFile(bufPtr, 1, nBytesRead, MAX_TIMEOUT);
        
        if (nBytesRead !=1) {// timeout
            err = ND_BAD_CMD_RESPONSE;
            break;
        }
    }

    return err;
}

int CNexDome::domeCommand(const char *cmd, char *result, char respCmdCode, int resultMaxLen)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    memset(buf,0,SERIAL_BUFFER_SIZE);
    strncpy(buf,cmd,SERIAL_BUFFER_SIZE);
    pSerx->purgeTxRx();
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);

    // read response
    err = readResponse(resp, SERIAL_BUFFER_SIZE);

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

    err = domeCommand("q\n", resp, 'Q', SERIAL_BUFFER_SIZE);
    if(err)
        return err;
    
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

    if(!mShutterOpened)
    {
        domeEl = 0.0;
        return err;
    }
        
    err = domeCommand("b\n", resp, 'B', SERIAL_BUFFER_SIZE);
    if(err)
        return err;
    
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

    err = domeCommand("i\n", resp, 'I', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

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

    err = domeCommand("n\n", resp, 'N', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

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

    err = domeCommand("u\n", resp, 'U', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

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
    if(err)
        return err;

    stepPerRev = atoi(resp);
    mNbStepPerRev = stepPerRev;
    return err;
}

int CNexDome::getBatteryLevels(double &domeVolts, double &shutterVolts)
{
    int err = 0;
    int i = 0;
    int j = 0;
    char resp[SERIAL_BUFFER_SIZE];
    char voltData[SERIAL_BUFFER_SIZE];
    
    if(!bIsConnected)
        return NOT_CONNECTED;

    
    err = domeCommand("k\n", resp, 'K', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    
    // convert battery vols value string to int
    memset(voltData,0,SERIAL_BUFFER_SIZE);
    // skip the spaces:
    while(resp[j]==' ')
        j++;
    while(resp[j]!=' ' && i<(SERIAL_BUFFER_SIZE - 1))
        voltData[i++]=resp[j++];
    domeVolts = atof(voltData);

    // skip the spaces:
    while(resp[j]==' ')
        j++;
    memset(voltData,0,SERIAL_BUFFER_SIZE);
    i = 0;
    while(resp[j]!=0 && i<(SERIAL_BUFFER_SIZE - 1))
        voltData[i++]=resp[j++];
    shutterVolts = atof(voltData);

    domeVolts = domeVolts / 100.0;
    shutterVolts = shutterVolts / 100.0;
    return err;
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
    if(err)
        return false;   // Not really correct but will do for now.

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
    if(err)
        return false;

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
    // TODO : Also set Elevation when supported by the firware.
    // mCurrentElPosition = dEl;
    return err;
}

int CNexDome::parkDome()
{
    int err;

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

    snprintf(buf, SERIAL_BUFFER_SIZE, "g %3.2f\n", newAz);
    err = domeCommand(buf, NULL, 'G', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    mGotoAz = newAz;

    return err;
}

int CNexDome::openShutter()
{
    if(!bIsConnected)
        return NOT_CONNECTED;

    return (domeCommand("d\n", NULL, 'D', SERIAL_BUFFER_SIZE));
}

int CNexDome::closeShutter()
{
    if(!bIsConnected)
        return NOT_CONNECTED;

    return (domeCommand("e\n", NULL, 'D', SERIAL_BUFFER_SIZE));
}


int CNexDome::getFirmwareVersion(char *version, int strMaxLen)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    if(!bIsConnected)
        return NOT_CONNECTED;

    err = domeCommand("v\n", resp, 'V', SERIAL_BUFFER_SIZE);
    if(err)
        return err;
    
    strncpy(version, resp, strMaxLen);
    return err;
}

int CNexDome::goHome()
{
    if(!bIsConnected)
        return NOT_CONNECTED;

    return (domeCommand("h\n", NULL, 'H', SERIAL_BUFFER_SIZE));
}

int CNexDome::calibrate()
{
    int err = 0;
    if(!bIsConnected)
        return NOT_CONNECTED;


    err = domeCommand("c\n", NULL, 'C', SERIAL_BUFFER_SIZE);
    if(err)
        return err;
    return err;
}

int CNexDome::isGoToComplete(bool &complete)
{
    int err = 0;
    double domeAz;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        complete = false;
        getDomeAz(domeAz);
        return err;
    }

    getDomeAz(domeAz);

    if (ceil(mGotoAz) == ceil(domeAz))
        complete = true;
    else {
        // we're not moving and we're not at the final destination !!!
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isGoToComplete] domeAz = %f, mGotoAz = %f\n", ceil(domeAz), ceil(mGotoAz));
        mLogger->out(mLogBuffer);

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
    double domeAz;

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
        err = getDomeStepPerRev(mNbStepPerRev);
    }
    else {
        // we're not moving and we're not at the home position !!!
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isFindHomeComplete] Not moving and not at home !!!\n");
        mLogger->out(mLogBuffer);
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
    double domeAz;

    if(!bIsConnected)
        return NOT_CONNECTED;

    if(isDomeMoving()) {
        getDomeAz(domeAz);
        mHomed = false;
        complete = false;
        return err;
    }

    err = getDomeAz(domeAz);

    if (ceil(mHomeAz) == ceil(domeAz))
    {
        mHomed = true;
        complete = true;
    }
    else {
        // we're not moving and we're not at the final destination !!!
        snprintf(mLogBuffer,ND_LOG_BUFFER_SIZE,"[CNexDome::isCalibratingComplete] Not moving and not at home !!!\n");
        mLogger->out(mLogBuffer);
        complete = false;
        mHomed = false;
        mParked = false;
        err = ERR_CMDFAILED;
    }

    return err;
}


int CNexDome::abortCurrentCommand()
{
    if(!bIsConnected)
        return NOT_CONNECTED;
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

