//
//  nexdome.cpp
//  NexDome X2 plugin
//
//  Created by Rodolphe Pineau on 6/11/2016.


#include "nexdome.h"
#include <stdlib.h>
#include <stdio.h>
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

    mCurrentAzPosition = 0;
    mCurrentElPosition = 0;

    mHomeAz = 0;

    mCloseShutterBeforePark = true;
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

    err = GetFirmwareVersion(firmwareVersion, SERIAL_BUFFER_SIZE);
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

int CNexDome::ReadResponse(char *respBuffer, int bufferLen)
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

int CNexDome::getDomeAz(double &domeAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;
    
    snprintf(buf, 20, "q\n");
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);
    
    // read response
    err = ReadResponse(resp, SERIAL_BUFFER_SIZE);
    if(resp[0] != 'Q')
        err = ND_BAD_CMD_RESPONSE;
    
    if(err)
        return err;
    
    // convert Az string to float
    domeAz = atof(&resp[1]);
    return err;
}

int CNexDome::getDomeEl(double &domeEl)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;
    
    snprintf(buf, 20, "b\n");
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);
    
    // read response
    err = ReadResponse(resp, SERIAL_BUFFER_SIZE);
    if(resp[0] != 'B')
        err = ND_BAD_CMD_RESPONSE;
    
    if(err)
        return err;
    
    // convert El string to float
    domeEl = atof(&resp[1]);
    return err;
}


int CNexDome::getDomeHomeAz(double &Az)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    snprintf(buf, 20, "z\n");
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);

    // read response
    err = ReadResponse(resp, SERIAL_BUFFER_SIZE);
    if(resp[0] != 'Z')
        err = ND_BAD_CMD_RESPONSE;

    if(err)
        return err;

    // convert Az string to float
    Az = atof(&resp[1]);
    return err;
}


int CNexDome::getShutterState(int &state)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    snprintf(buf, 20, "u\n");
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);

    // read response
    err = ReadResponse(resp, SERIAL_BUFFER_SIZE);
    if(resp[0] != 'U')
        err = ND_BAD_CMD_RESPONSE;
    if(err)
        return err;

    state = atoi(&resp[1]);

    return err;
}

bool CNexDome::isDomeMoving()
{
    bool isMoving;
    int tmp;
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    snprintf(buf, 20, "m\n");
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);

    // read response
    err = ReadResponse(resp, SERIAL_BUFFER_SIZE);
    if(resp[0] != 'M')
        err = ND_BAD_CMD_RESPONSE;
    if(err)
        return false;   // Not really correct but will do for now.

    isMoving = false;
    tmp = atoi(&resp[1]);
    if(tmp)
        isMoving = true;

    return isMoving;
}

int CNexDome::Sync_Dome(double dAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    mCurrentAzPosition = dAz;
    snprintf(buf, 20, "s %3.2f\n", dAz);
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);

    // read response
    err = ReadResponse(resp, SERIAL_BUFFER_SIZE);
    if(resp[0] != 'S')
        err = ND_BAD_CMD_RESPONSE;
    return err;
}

int CNexDome::Park(void)
{
    int err;
    err = Goto_Azimuth(mParkAz);
    return err;

}

int CNexDome::Unpark(void)
{
    mParked = false;
    return 0;
}

int CNexDome::Goto_Azimuth(double newAz)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;

    snprintf(buf, 20, "g %3.2f\n", newAz);
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);
    
    // read response
    err = ReadResponse(resp, SERIAL_BUFFER_SIZE);
    if(resp[0] != 'G')
        err = ND_BAD_CMD_RESPONSE;
    mGotoAz = newAz;

    return err;
}


int CNexDome::GetFirmwareVersion(char *version, int strMaxLen)
{
    int err = 0;
    char buf[SERIAL_BUFFER_SIZE];
    char resp[SERIAL_BUFFER_SIZE];
    unsigned long  nBytesWrite;
    
    snprintf(buf, 20, "v\n");
    err = pSerx->writeFile(buf, strlen(buf), nBytesWrite);
    
    // read response
    err = ReadResponse(resp, SERIAL_BUFFER_SIZE);
    if(resp[0] != 'V')
        err = ND_BAD_CMD_RESPONSE;
    
    if(err)
        return err;
    
    strncpy(version, &resp[1], strMaxLen);
    return err;

}


int CNexDome::IsGoToComplete(bool &complete)
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

int CNexDome::IsOpenComplete(bool &complete)
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

int CNexDome::IsCloseComplete(bool &complete)
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


int CNexDome::IsParkComplete(bool &complete)
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

int CNexDome::IsUnparkComplete(bool &complete)
{
    int err=0;

    mParked = false;
    complete = true;

    return err;
}

int CNexDome::IsFindHomeComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;

    complete = true;

    return err;
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

void CNexDome::setHomeAz(double dAz)
{
    mHomeAz = dAz;

}


double CNexDome::getParkAz()
{
    return mParkAz;

}

void CNexDome::setParkAz(double dAz)
{
    int dir;
    mParkAz = dAz;


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
    return mCurrentAzPosition;
}

void CNexDome::setCurrentAz(double dAz)
{
    mCurrentAzPosition = dAz;
}

char * CNexDome::getVersion()
{
    return firmwareVersion;
}

