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

#define SERIAL_BUFFER_SIZE 20
#define MAX_TIMEOUT 500

// error codes
// Error code
enum NexDomeErrors {ND_OK=0, NOT_CONNECTED, ND_CANT_CONNECT, ND_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum NexDomeShutterState {OPEN=1, OPENING, CLOSED, CLOSING, SHUTTER_ERROR};

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
    int syncDome(double dAz, double dEl);
    int parkDome(void);
    int unparkDome(void);
    int gotoAzimuth(double newAz);
    int openShutter();
    int closeShutter();
    int getFirmwareVersion(char *version, int strMaxLen);
    int goHome();
    int calibrate();

    // command complete functions
    int isGoToComplete(bool &complete);
    int isOpenComplete(bool &complete);
    int isCloseComplete(bool &complete);
    int isParkComplete(bool &complete);
    int isUnparkComplete(bool &complete);
    int isFindHomeComplete(bool &complete);
    int isCalibratingComplete(bool &complete);

    int abortCurrentCommand();

    // getter/setter
    int getNbTicksPerRev();

    double getHomeAz();
    int setHomeAz(double dAz);

    double getParkAz();
    int setParkAz(double dAz);

    double getCurrentAz();
    double getCurrentEl();

    void setShutterOnly(bool bMode);

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

    bool            bIsConnected;
    bool            mHomed;
    bool            mParked;
    bool            mShutterOpened;
    
    int             mNbStepPerRev;

    double          mHomeAz;
    
    double          mParkAz;

    double          mCurrentAzPosition;
    double          mCurrentElPosition;

    double          mGotoAz;
    
    SerXInterface   *pSerx;
    
    char            firmwareVersion[SERIAL_BUFFER_SIZE];
    bool            mShutterOnly; // roll off roof so the arduino is running the shutter firmware only.

};

#endif
