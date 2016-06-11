//
//  nexdome.h
//  NexDome
//
//  Created by Rodolphe Pineau on 6/11/2016.
//  NexDome X2 plugin

#ifndef __NEXDOME__
#define __NEXDOME__
#include <math.h>
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"

class CNexDome
{
public:
    CNexDome();
    ~CNexDome();

    bool        Connect(const char *szPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return bIsConnected; }

    void        SetSerxPointer(SerXInterface *p) { pSerx = p; }

    // Dome commands
    int Sync_Dome(double dAz);
    int Park(void);
    int Unpark(void);

    // convertion functions
    void AzToTicks(double pdAz, int &dir, int &ticks);
    void TicksToAz(int ticks, double &pdAz);

    // command complete functions
    int IsGoToComplete(bool &complete);
    int IsOpenComplete(bool &complete);
    int IsCloseComplete(bool &complete);
    int IsParkComplete(bool &complete);
    int IsUnparkComplete(bool &complete);
    int IsFindHomeComplete(bool &complete);
    
    // getter/setter
    int getNbTicksPerRev();
    void setNbTicksPerRev(int nbTicksPerRev);

    double getHomeAz();
    void setHomeAz(double dAz);

    double getParkAz();
    void setParkAz(double dAz);

    bool getCloseShutterBeforePark();
    void setCloseShutterBeforePark(bool close);

    double getCurrentAz();
    void setCurrentAz(double dAz);

protected:

    bool            bIsConnected;

    bool            mHomed;
    bool            mParked;
    bool            mCloseShutterBeforePark;
    bool            mShutterOpened;
    
    unsigned        mNbTicksPerRev;

    unsigned        mHomeAzInTicks;
    double          mHomeAz;
    
    unsigned        mParkAzInTicks;
    double          mParkAz;

    unsigned        mCurrentAzPositionInTicks;
    double          mCurrentAzPosition;

    unsigned        mGotoTicks;
    SerXInterface   *pSerx;
};

#endif
