#include <stdio.h>
#include <string.h>
#include "x2dome.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/serialportparams2interface.h"


X2Dome::X2Dome(const char* pszSelection, 
							 const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface*	pTheSkyXForMounts,
					SleeperInterface*					pSleeper,
					BasicIniUtilInterface*			pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*						pIOMutex,
					TickCountInterface*					pTickCount)
{

    m_nPrivateISIndex				= nISIndex;
	m_pSerX							= pSerX;
	m_pTheSkyXForMounts				= pTheSkyXForMounts;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;	
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;

	m_bLinked = false;
    mHomingDome = false;

    nexDome.SetSerxPointer(pSerX);

    if (m_pIniUtil)
    {   
        nexDome.setNbTicksPerRev( m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_TICKS_PER_REV, 0) );
        nexDome.setHomeAz( m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_HOME_AZ, 0) );
        nexDome.setParkAz( m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, 180) );
        mHasShutterControl = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SHUTTER_CONTROL, false);
    }
}


X2Dome::~X2Dome()
{
	if (m_pSerX)
		delete m_pSerX;
	if (m_pTheSkyXForMounts)
		delete m_pTheSkyXForMounts;
	if (m_pSleeper)
		delete m_pSleeper;
	if (m_pIniUtil)
		delete m_pIniUtil;
	if (m_pLogger)
		delete m_pLogger;
	if (m_pIOMutex)
		delete m_pIOMutex;
	if (m_pTickCount)
		delete m_pTickCount;

}


int X2Dome::establishLink(void)					
{
    bool connected;
    char szPort[DRIVER_MAX_STRING];

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    connected = nexDome.Connect(szPort);
    if(!connected)
        return ERR_COMMNOLINK;

    m_bLinked = true;
    // if(mIsRollOffRoof)
    //    nexDome.setShutterOnly(true);

	return SB_OK;
}

int X2Dome::terminateLink(void)					
{
    X2MutexLocker ml(GetMutex());
    nexDome.Disconnect();
	m_bLinked = false;
	return SB_OK;
}

 bool X2Dome::isLinked(void) const				
{
	return m_bLinked;
}


int X2Dome::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LoggerInterface_Name))
        *ppVal = GetLogger();
    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);
    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);
    
    return SB_OK;
}

#pragma mark - UI binding

int X2Dome::execModalSettingsDialog()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;

    double dHomeAz;
    double dParkAz;
    bool operateAnyAz;
    int nTicksPerRev;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("NexDome.ui", deviceType(), m_nPrivateISIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    // set controls state depending on the connection state
    if(mHasShutterControl)
    {
        dx->setChecked("hasShutterCtrl",true);

    }
    else
    {
        dx->setChecked("hasShutterCtrl",false);
    }
    
    if(m_bLinked)
        dx->setEnabled("autoCalibrate",true);
    else
        dx->setEnabled("autoCalibrate",false);

    dx->setPropertyInt("ticksPerRev","value", nexDome.getNbTicksPerRev());
    dx->setPropertyDouble("homePosition","value", nexDome.getHomeAz());
    dx->setPropertyDouble("parkPosition","value", nexDome.getParkAz());

    mHomingDome = false;

    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK)
    {
        dx->propertyDouble("homePosition", "value", dHomeAz);
        dx->propertyDouble("parkPosition", "value", dParkAz);
        operateAnyAz = dx->isChecked("radioButtonShutterAnyAz");
        dx->propertyInt("ticksPerRev", "value", nTicksPerRev);
        mHasShutterControl = dx->isChecked("hasShutterCtrl");

        if(m_bLinked)
        {
            nexDome.setHomeAz(dHomeAz);
            nexDome.setParkAz(dParkAz);
            nexDome.setNbTicksPerRev(nTicksPerRev);
        }

        // save the values to persistent storage
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_TICKS_PER_REV, nTicksPerRev);
        nErr |= m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_HOME_AZ, dHomeAz);
        nErr |= m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, dParkAz);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SHUTTER_CONTROL, mHasShutterControl);
    }
    return nErr;

}

void X2Dome::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    bool complete;
    
    if (!strcmp(pszEvent, "on_timer"))
    {
        if(m_bLinked) {
            // are we going to Home position to calibrate ?
            if(mHomingDome) {
                // are we home ?
                nexDome.isFindHomeComplete(complete);
                if(complete) {
                    mHomingDome = false;
                    nexDome.calibrate();
                }

            }

            // are we still calibrating ?
            nexDome.isCalibratingComplete(complete);
            if(!complete)
                return;

            // enable "ok" and "calibrate"
            uiex->setEnabled("autoCalibrate",true);
            uiex->setEnabled("pushButtonOK",true);
            uiex->setEnabled("pushButtonCancel",true);
            // read step per rev from dome
            uiex->setPropertyInt("ticksPerRev","value", nexDome.getNbTicksPerRev());
        }
    }
    if (!strcmp(pszEvent, "on_autoCalibrate_clicked"))
    {
        if(m_bLinked) {
            // disable "ok" and "calibrate"
            uiex->setEnabled("autoCalibrate",false);
            uiex->setEnabled("pushButtonOK",false);
            uiex->setEnabled("pushButtonCancel",false);

            nexDome.goHome();
            mHomingDome = true;
        }
    }

}

//
//HardwareInfoInterface
//
#pragma mark - HardwareInfoInterface

void X2Dome::deviceInfoNameShort(BasicStringInterface& str) const					
{
    printf("deviceInfoNameShort\n");
	str = "NexDome";
}
void X2Dome::deviceInfoNameLong(BasicStringInterface& str) const					
{
    printf("deviceInfoNameLong\n");
    str = "NexDome Dome Control System";
}
void X2Dome::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
    printf("deviceInfoDetailedDescription\n");
    str = "NexDome Dome Control System by Rodolphe Pineau";
}
 void X2Dome::deviceInfoFirmwareVersion(BasicStringInterface& str)					
{
    printf("deviceInfoFirmwareVersion\n");
    if(m_bLinked) {
        char cFirmware[SERIAL_BUFFER_SIZE];
        nexDome.getFirmwareVersion(cFirmware, SERIAL_BUFFER_SIZE);
        str = cFirmware;

    }
    else
        str = "N/A";
}

void X2Dome::deviceInfoModel(BasicStringInterface& str)
{
    printf("deviceInfoModel\n");

    str = "NexDome";
}

//
//DriverInfoInterface
//
#pragma mark - DriverInfoInterface

 void	X2Dome::driverInfoDetailedInfo(BasicStringInterface& str) const	
{
    str = "NexDome X2 plugin v1.0 beta";
}

double	X2Dome::driverInfoVersion(void) const
{
	return DRIVER_VERSION;
}

//
//DomeDriverInterface
//
#pragma mark - DomeDriverInterface

int X2Dome::dapiGetAzEl(double* pdAz, double* pdEl)
{
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    *pdAz = nexDome.getCurrentAz();
    *pdEl = nexDome.getCurrentEl();
    return SB_OK;
}

int X2Dome::dapiGotoAzEl(double dAz, double dEl)
{
    int err;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = nexDome.gotoAzimuth(dAz);

    if(err)
        return ERR_CMDFAILED;

    else
        return SB_OK;
}

int X2Dome::dapiAbort(void)
{

    if(!m_bLinked)
        return ERR_NOLINK;
    nexDome.abortCurrentCommand();
	return SB_OK;
}

int X2Dome::dapiOpen(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
        return SB_OK;

    err = nexDome.openShutter();
    if(err)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiClose(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
        return SB_OK;

    err = nexDome.closeShutter();
    if(err)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiPark(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mHasShutterControl)
    {
        err = nexDome.closeShutter();
        if(err)
            return ERR_CMDFAILED;
    }

    err = nexDome.parkDome();
    if(err)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiUnpark(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = nexDome.unparkDome();
    if(err)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiFindHome(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = nexDome.goHome();
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsGotoComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = nexDome.isGoToComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;
    return SB_OK;
}

int X2Dome::dapiIsOpenComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;
    
    if(!mHasShutterControl)
    {
        *pbComplete = true;
        return SB_OK;
    }

    err = nexDome.isOpenComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int	X2Dome::dapiIsCloseComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
    {
        *pbComplete = true;
        return SB_OK;
    }

    err = nexDome.isCloseComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsParkComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = nexDome.isParkComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsUnparkComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = nexDome.isUnparkComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsFindHomeComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = nexDome.isFindHomeComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiSync(double dAz, double dEl)
{
    int err;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = nexDome.syncDome(dAz, dEl);
    if (err)
        return ERR_CMDFAILED;
	return SB_OK;
}

//
// SerialPortParams2Interface
//
#pragma mark - SerialPortParams2Interface

void X2Dome::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Dome::setPortName(const char* szPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, szPort);
    
}


void X2Dome::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    sprintf(pszPort, DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}



