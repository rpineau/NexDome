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

    mNbTicksPerRev = 0;

    mCurrentAzPosition = 0.0;
    mCurrentElPosition = 0.0;

    mHomeAz = 0;

    mCloseShutterBeforePark = true;
    mShutterOpened = false;
    
    mParked = true;
    mHomed = false;

    mCalibrating  = false;
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
    return bIsConnected;
}


void CNexDome::Disconnect(void)
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
    memset(respBuffer, 0, (size_t) bufferLen);
    // Look for a CR  character, until time out occurs or MAX_BUFFER characters was read
    while (*respBuffer != '\n' || nBytesRead < bufferLen )
    {
        err = pSerx->readFile(respBuffer, 1, nBytesRead, MAX_TIMEOUT);
        if (nBytesRead !=1) // timeout
            err = ND_BAD_CMD_RESPONSE;
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

    err = domeCommand("q\n", resp, 'Q', SERIAL_BUFFER_SIZE);
    if(err)
        return err;
    
    // convert Az string to float
    domeAz = atof(resp);
    return err;
}

int CNexDome::getDomeEl(double &domeEl)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    err = domeCommand("b\n", resp, 'B', SERIAL_BUFFER_SIZE);
    if(err)
        return err;
    
    // convert El string to float
    domeEl = atof(resp);
    return err;
}


int CNexDome::getDomeHomeAz(double &Az)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    err = domeCommand("z\n", resp, 'Z', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    // convert Az string to float
    Az = atof(resp);
    return err;
}


int CNexDome::getShutterState(int &state)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    err = domeCommand("u\n", resp, 'U', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    state = atoi(resp);

    return err;
}

bool CNexDome::isDomeMoving()
{
    bool isMoving;
    int tmp;
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    err = domeCommand("m\n", resp, 'M', SERIAL_BUFFER_SIZE);
    if(err)
        return false;   // Not really correct but will do for now.

    isMoving = false;
    tmp = atoi(resp);
    if(tmp)
        isMoving = true;

    return isMoving;
}

int CNexDome::syncDome(double dAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];

    mCurrentAzPosition = dAz;
    snprintf(buf, SERIAL_BUFFER_SIZE, "s %3.2f\n", dAz);
    err = domeCommand(buf, NULL, 'S', SERIAL_BUFFER_SIZE);
    return err;
}

int CNexDome::parkDome(void)
{
    int err;
    err = gotoAzimuth(mParkAz);
    return err;

}

int CNexDome::unparkDome(void)
{
    mParked = false;
    return 0;
}

int CNexDome::gotoAzimuth(double newAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];

    snprintf(buf, SERIAL_BUFFER_SIZE, "g %3.2f\n", newAz);
    err = domeCommand(buf, NULL, 'G', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    mGotoAz = newAz;

    return err;
}

int CNexDome::openShutter()
{
    return (domeCommand("d\n", NULL, 'D', SERIAL_BUFFER_SIZE));
}

int CNexDome::closeShutter()
{
    return (domeCommand("e\n", NULL, 'D', SERIAL_BUFFER_SIZE));
}


int CNexDome::getFirmwareVersion(char *version, int strMaxLen)
{
    int err = 0;
    char resp[SERIAL_BUFFER_SIZE];

    err = domeCommand("v\n", resp, 'V', SERIAL_BUFFER_SIZE);
    if(err)
        return err;
    
    strncpy(version, resp, strMaxLen);
    return err;
}

int CNexDome::goHome()
{
    return (domeCommand("h\n", NULL, 'H', SERIAL_BUFFER_SIZE));
}

int CNexDome::isGoToComplete(bool &complete)
{
    int err = 0;
    double domeAz;

    if(isDomeMoving()) {
        complete = false;
        return err;
    }

    getDomeAz(domeAz);

    if (mGotoAz == domeAz)
        complete = true;
    else {
        // we're not moving and we're not at the final destination !!!
        complete = false;
        err = ERR_CMDFAILED;
    }

    return err;
}

int CNexDome::isOpenComplete(bool &complete)
{
    int err=0;
    int state;

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

    if(isDomeMoving()) {
        complete = false;
        return err;
    }

    getDomeAz(domeAz);

    if (mParkAz == domeAz)
    {
        mParked = true;
        complete = true;
    }
    else {
        // we're not moving and we're not at the final destination !!!
        complete = false;
        err = ERR_CMDFAILED;
        mParked = false;
    }

    return err;
}

int CNexDome::isUnparkComplete(bool &complete)
{
    int err=0;

    mParked = false;
    complete = true;

    return err;
}

int CNexDome::isFindHomeComplete(bool &complete)
{
    int err = 0;
    double domeAz;

    if(isDomeMoving()) {
        mHomed = false;
        complete = false;
        return err;
    }

    getDomeAz(domeAz);

    if (mHomeAz == domeAz)
    {
        mHomed = true;
        complete = true;
    }
    else {
        // we're not moving and we're not at the final destination !!!
        mHomed = false;
        err = ERR_CMDFAILED;
        mParked = false;
    }

    return err;
}

bool CNexDome::isCalibrating()
{
    return mCalibrating;
}

int CNexDome::abortCurrentCommand()
{
    return (domeCommand("a\n", NULL, 'A', SERIAL_BUFFER_SIZE));
}

#pragma mark - Getter / Setter

int CNexDome::getNbTicksPerRev()
{
    return mNbTicksPerRev;
}

void CNexDome::setNbTicksPerRev(int nbTicksPerRev)
{
    mNbTicksPerRev = nbTicksPerRev;
}


double CNexDome::getHomeAz()
{
    return mHomeAz;
}

int CNexDome::setHomeAz(double dAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];

    snprintf(buf, 20, "j %3.2f\n", dAz);
    err = domeCommand(buf, resp, 'J', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    mHomeAz = dAz;

    return err;
}


double CNexDome::getParkAz()
{
    return mParkAz;

}

int CNexDome::setParkAz(double dAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];

    snprintf(buf, 20, "l %3.2f\n", dAz);
    err = domeCommand(buf, resp, 'L', SERIAL_BUFFER_SIZE);
    if(err)
        return err;

    mParkAz = dAz;

    return err;
}

bool CNexDome::getCloseShutterBeforePark()
{
    return mCloseShutterBeforePark;
}

void CNexDome::setCloseShutterBeforePark(bool close)
{
    mCloseShutterBeforePark = close;
}

double CNexDome::getCurrentAz()
{
    return mCurrentAzPosition;
}

double CNexDome::getCurrentEl()
{
    return mCurrentElPosition;
}

void CNexDome::setCurrentAz(double dAz)
{
    mCurrentAzPosition = dAz;
}

char * CNexDome::getVersion()
{
    return firmwareVersion;
}

void CNexDome::setShutterOnly(bool bMode)
{
    mShutterOnly = bMode;
}
