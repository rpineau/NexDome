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
    mCurrentAzPositionInTicks = 0;

    mHomeAz = 0;
    mHomeAzInTicks = 0;

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
    unsigned tmpAz;
    unsigned tmpHomePosition;

    // 19200 8N1
    if(pSerx->open(szPort,9600) == 0)
        bIsConnected = true;
    else
        bIsConnected = false;

    // Check to see if we can't even connect to the device
    if(!bIsConnected)
        return false;

    // bIsConnected = GetFirmware(szFirmware);
    pSerx->purgeTxRx();


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
        pSerx->close();
    }
    bIsConnected = false;
}


int CNexDome::Sync_Dome(double dAz)
{
    int err = 0;
    int dir;

    mCurrentAzPosition = dAz;
    AzToTicks(dAz, dir, (int &)mCurrentAzPositionInTicks);
    return err;
}

int CNexDome::Park(void)
{
    mParked = true;
    return 0;

}

int CNexDome::Unpark(void)
{
    mParked = false;
    return 0;
}
/*
	Convert pdAz to number of ticks from home and direction.

 */
void CNexDome::AzToTicks(double pdAz, int &dir, int &ticks)
{
    dir = 0;

    ticks = floor(0.5 + (pdAz - mHomeAz) * mNbTicksPerRev / 360.0);
    while (ticks > mNbTicksPerRev) ticks -= mNbTicksPerRev;
    while (ticks < 0) ticks += mNbTicksPerRev;

    // find the dirrection with the shortest path
    if( (mCurrentAzPosition < pdAz) && (mCurrentAzPosition <(pdAz -180)))
    {
        dir = 1;
    }
    else if (mCurrentAzPosition > pdAz)
    {
        dir = 1;
    }

}


/*
	Convert ticks from home to Az
 
*/

void CNexDome::TicksToAz(int ticks, double &pdAz)
{
    
    pdAz = mHomeAzInTicks + ticks * 360.0 / mNbTicksPerRev;
    while (pdAz < 0) pdAz += 360;
    while (pdAz >= 360) pdAz -= 360;
}


int CNexDome::IsGoToComplete(bool &complete)
{
    int err = 0;
    unsigned tmpAz;
    unsigned tmpHomePosition;

    complete = true;

    return err;
}

int CNexDome::IsOpenComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;

    complete = true;

    return err;
}

int CNexDome::IsCloseComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;

    complete = true;

    return err;
}


int CNexDome::IsParkComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;

    complete = true;

    return err;
}

int CNexDome::IsUnparkComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;

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
    AzToTicks(dAz, dir, (int &)mParkAzInTicks);


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

void CNexDome::setCurrentAz(double dAz)
{
    mCurrentAzPosition = dAz;
}
