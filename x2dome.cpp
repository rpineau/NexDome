#include "x2dome.h"


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
    m_bHomingDome = false;
    m_bCalibratingDome = false;
    m_nBattRequest = 0;
    
    m_NexDome.setSerxPointer(pSerX);
    m_NexDome.setSleeprPinter(pSleeper);
    m_NexDome.setLogger(pLogger);

    if (m_pIniUtil)
    {   
        m_NexDome.setHomeAz( m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_HOME_AZ, 0) );
        m_NexDome.setParkAz( m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, 0) );
        m_bHasShutterControl = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SHUTTER_CONTROL, false);

        m_bHomeOnPark = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_HOME_ON_PARK, false);
        m_bHomeOnUnpark = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_HOME_ON_UNPARK, false);
        m_NexDome.setHomeOnPark(m_bHomeOnPark);
        m_NexDome.setHomeOnUnpark(m_bHomeOnUnpark);
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
    int nErr;
    char szPort[DRIVER_MAX_STRING];

    X2MutexLocker ml(GetMutex());

    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr = m_NexDome.Connect(szPort);
    if(nErr) {
        m_bLinked = false;
        // nErr = ERR_COMMOPENING;
    }
    else
        m_bLinked = true;

	return nErr;
}

int X2Dome::terminateLink(void)					
{
    X2MutexLocker ml(GetMutex());

    m_NexDome.Disconnect();
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
    char szTmpBuf[SERIAL_BUFFER_SIZE];
    double dHomeAz;
    double dParkAz;
    double dDomeBattery, dDomeCutOff;
    double dShutterBattery, dShutterCutOff;
    bool nReverseDir;
    int n_nbStepPerRev;
    double dPointingError;
    int nRainSensorStatus = NOT_RAINING;
    int nRSpeed;
    int nRAcc;
    int nSSpeed;
    int nSAcc;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("NexDome.ui", deviceType(), m_nPrivateISIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    X2MutexLocker ml(GetMutex());

    memset(szTmpBuf,0,SERIAL_BUFFER_SIZE);
    // set controls state depending on the connection state
    if(m_bHasShutterControl) {
        dx->setChecked("hasShutterCtrl",true);
    }
    else {
        dx->setChecked("hasShutterCtrl",false);
    }

    if(m_bHomeOnPark) {
        dx->setChecked("homeOnPark",true);
    }
    else {
        dx->setChecked("homeOnPark",false);
    }

    if(m_bHomeOnUnpark) {
        dx->setChecked("homeOnUnpark",true);
    }
    else {
        dx->setChecked("homeOnUnpark",false);
    }

    if(m_bLinked) {
        dx->setEnabled("homePosition",true);
        dx->setEnabled("parkPosition",true);
        nErr = m_NexDome.getDefaultDir(nReverseDir);
        if(nReverseDir)
            dx->setChecked("needReverse",false);
        else
            dx->setChecked("needReverse",true);
        m_NexDome.getPointingError(dPointingError);
        snprintf(szTmpBuf,16,"%3.2f",dPointingError);
        dx->setPropertyString("domePointingError","text", szTmpBuf);

        // read values from dome controller
        dx->setEnabled("ticksPerRev",true);
        n_nbStepPerRev = m_NexDome.getNbTicksPerRev();
        dx->setPropertyInt("ticksPerRev","value", n_nbStepPerRev);

        dx->setEnabled("rotationSpeed",true);
        m_NexDome.getRotationSpeed(nRSpeed);
        dx->setPropertyInt("rotationSpeed","value", nRSpeed);

        dx->setEnabled("rotationAcceletation",true);
        m_NexDome.getRotationAcceleration(nRAcc);
        dx->setPropertyInt("rotationAcceletation","value", nRAcc);

        dx->setEnabled("shutterSpeed",true);
        m_NexDome.getShutterSpeed(nSSpeed);
        dx->setPropertyInt("shutterSpeed","value", nSSpeed);

        dx->setEnabled("shutterAcceleration",true);
        m_NexDome.getShutterAcceleration(nSAcc);
        dx->setPropertyInt("shutterAcceleration","value", nSAcc);


        m_NexDome.getBatteryLevels(dDomeBattery, dDomeCutOff, dShutterBattery, dShutterCutOff);
        snprintf(szTmpBuf,16,"%2.2f V",dDomeBattery);
        dx->setPropertyString("domeBatteryLevel","text", szTmpBuf);
        if(m_bHasShutterControl) {
            if(dShutterBattery>=0.0f)
                snprintf(szTmpBuf,16,"%2.2f V",dShutterBattery);
            else
                snprintf(szTmpBuf,16,"--");
            dx->setPropertyString("shutterBatteryLevel","text", szTmpBuf);
        }
        else {
            dx->setPropertyString("shutterBatteryLevel","text", "--");
        }
        nErr = m_NexDome.getRainSensorStatus(nRainSensorStatus);
        if(nErr)
            dx->setPropertyString("rainStatus","text", "--");
        else {
            snprintf(szTmpBuf, 16, nRainSensorStatus==NOT_RAINING ? "Not raining" : "Raining");
            dx->setPropertyString("rainStatus","text", szTmpBuf);
        }

        dx->setEnabled("pushButton",true);
    }
    else {
        dx->setEnabled("homePosition",false);
        dx->setEnabled("parkPosition",false);
        dx->setEnabled("ticksPerRev",false);
        dx->setEnabled("rotationSpeed",false);
        dx->setEnabled("rotationAcceletation",false);
        dx->setEnabled("shutterSpeed",false);
        dx->setEnabled("shutterAcceleration",false);

        dx->setPropertyString("domeBatteryLevel","text", "--");
        dx->setPropertyString("shutterBatteryLevel","text", "--");
        dx->setEnabled("pushButton",false);
        dx->setEnabled("needReverse",false);
        dx->setPropertyString("domePointingError","text", "--");
        dx->setPropertyString("rainStatus","text", "--");
    }
    dx->setPropertyDouble("homePosition","value", m_NexDome.getHomeAz());
    dx->setPropertyDouble("parkPosition","value", m_NexDome.getParkAz());


    m_bHomingDome = false;
    m_nBattRequest = 0;
    

    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
        dx->propertyInt("ticksPerRev", "value", n_nbStepPerRev);
        dx->propertyDouble("homePosition", "value", dHomeAz);
        dx->propertyDouble("parkPosition", "value", dParkAz);
        dx->propertyInt("rotationSpeed", "value", nRSpeed);
        dx->propertyInt("rotationAcceletation", "value", nRAcc);
        dx->propertyInt("shutterSpeed", "value", nSSpeed);
        dx->propertyInt("shutterAcceleration", "value", nSAcc);
        m_bHasShutterControl = dx->isChecked("hasShutterCtrl");
        m_bHomeOnPark = dx->isChecked("homeOnPark");
        m_bHomeOnUnpark = dx->isChecked("homeOnUnpark");
        m_NexDome.setHomeOnUnpark(m_bHomeOnUnpark);
        nReverseDir = dx->isChecked("needReverse");
        if(m_bLinked) {
            m_NexDome.setDefaultDir(!nReverseDir);
            m_NexDome.setHomeOnPark(m_bHomeOnPark);
            m_NexDome.setHomeOnUnpark(m_bHomeOnUnpark);
            m_NexDome.setHomeAz(dHomeAz);
            m_NexDome.setParkAz(dParkAz);
            m_NexDome.setNbTicksPerRev(n_nbStepPerRev);
            m_NexDome.setRotationSpeed(nRSpeed);
            m_NexDome.setRotationAcceleration(nRAcc);
            m_NexDome.setShutterSpeed(nSSpeed);
            m_NexDome.setShutterAcceleration(nSAcc);
        }

        // save the values to persistent storage
        nErr |= m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_HOME_AZ, dHomeAz);
        nErr |= m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, dParkAz);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SHUTTER_CONTROL, m_bHasShutterControl);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_HOME_ON_PARK, m_bHomeOnPark);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_HOME_ON_UNPARK, m_bHomeOnUnpark);
    }
    return nErr;

}

void X2Dome::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    bool bComplete = false;
    int nErr;
    double dDomeBattery, dDomeCutOff;
    double dShutterBattery, dShutterCutOff;
    char szTmpBuf[SERIAL_BUFFER_SIZE];    
    char szErrorMessage[LOG_BUFFER_SIZE];
    int nRainSensorStatus = NOT_RAINING;

    if (!strcmp(pszEvent, "on_pushButtonCancel_clicked") && (m_bCalibratingDome || m_bHomingDome))
        m_NexDome.abortCurrentCommand();

    if (!strcmp(pszEvent, "on_timer"))
    {
        m_bHasShutterControl = uiex->isChecked("hasShutterCtrl");
        if(m_bLinked) {
            // are we going to Home position to calibrate ?
            if(m_bHomingDome) {
                // are we home ?
                bComplete = false;
                nErr = m_NexDome.isFindHomeComplete(bComplete);
                if(nErr) {
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error homing dome while calibrating dome : Error %d", nErr);
                    uiex->messageBox("NexDome Calibrate", szErrorMessage);
                    m_bHomingDome = false;
                    m_bCalibratingDome = false;
                    return;
                }
                if(bComplete) {
                    m_bHomingDome = false;
                    m_bCalibratingDome = true;
                    m_NexDome.setNbTicksPerRev(16000000L);    // set this to a large value as the firmware only do 1 move of 1.5 time the current step per rev
                    m_NexDome.calibrate();
                    return;
                }
            }
            
            if(m_bCalibratingDome) {
                // are we still calibrating ?
                bComplete = false;
                nErr = m_NexDome.isCalibratingComplete(bComplete);
                if(nErr) {
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error calibrating dome : Error %d", nErr);
                    uiex->messageBox("NexDome Calibrate", szErrorMessage);
                    m_bHomingDome = false;
                    m_bCalibratingDome = false;
                    return;;
                }

                if(!bComplete) {
                    return;
                }

                // enable "ok" and "calibrate"
                uiex->setEnabled("pushButton",true);
                uiex->setEnabled("pushButtonOK",true);
                // read step per rev from controller
                uiex->setPropertyInt("ticksPerRev","value", m_NexDome.getNbTicksPerRev());
                m_bCalibratingDome = false;
            }
            
            if(m_bHasShutterControl && !m_bHomingDome && !m_bCalibratingDome) {
                // don't ask to often
                if (!(m_nBattRequest%4)) {
                    m_NexDome.getBatteryLevels(dDomeBattery, dDomeCutOff, dShutterBattery, dShutterCutOff);
                    snprintf(szTmpBuf,16,"%2.2f V",dDomeBattery);
                    uiex->setPropertyString("domeBatteryLevel","text", szTmpBuf);
                    if(m_bHasShutterControl) {
                        if(dShutterBattery>=0.0f)
                            snprintf(szTmpBuf,16,"%2.2f V",dShutterBattery);
                        else
                            snprintf(szTmpBuf,16,"--");
                        uiex->setPropertyString("shutterBatteryLevel","text", szTmpBuf);
                    }
                    else {
                        snprintf(szTmpBuf,16,"NA");
                        uiex->setPropertyString("shutterBatteryLevel","text", szTmpBuf);
                    }
                }
                m_nBattRequest++;
                nErr = m_NexDome.getRainSensorStatus(nRainSensorStatus);
                if(nErr)
                    uiex->setPropertyString("rainStatus","text", "--");
                else {
                    snprintf(szTmpBuf, 16, nRainSensorStatus==NOT_RAINING ? "Not raining" : "Raining");
                    uiex->setPropertyString("rainStatus","text", szTmpBuf);
                }
            }
        }
    }

    if (!strcmp(pszEvent, "on_pushButton_clicked"))
    {
        if(m_bLinked) {
            // disable "ok" and "calibrate"
            uiex->setEnabled("pushButton",false);
            uiex->setEnabled("pushButtonOK",false);
            m_NexDome.goHome();
            m_bHomingDome = true;
        }
    }
}

//
//HardwareInfoInterface
//
#pragma mark - HardwareInfoInterface

void X2Dome::deviceInfoNameShort(BasicStringInterface& str) const					
{
	str = "NexDome";
}

void X2Dome::deviceInfoNameLong(BasicStringInterface& str) const					
{
    str = "Nexdome";
}

void X2Dome::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
    str = "Nexdome Dome Rotation Kit";
}

 void X2Dome::deviceInfoFirmwareVersion(BasicStringInterface& str)					
{
    X2MutexLocker ml(GetMutex());

    if(m_bLinked) {
        char cFirmware[SERIAL_BUFFER_SIZE];
        m_NexDome.getFirmwareVersion(cFirmware, SERIAL_BUFFER_SIZE);
        str = cFirmware;

    }
    else
        str = "N/A";
}

void X2Dome::deviceInfoModel(BasicStringInterface& str)
{
    str = "Nexdome Dome Rotation Kit";
}

//
//DriverInfoInterface
//
#pragma mark - DriverInfoInterface

 void	X2Dome::driverInfoDetailedInfo(BasicStringInterface& str) const	
{
    str = "Nexdome Dome Rotation Kit X2 plugin by Rodolphe Pineau";
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

    *pdAz = m_NexDome.getCurrentAz();
    *pdEl = m_NexDome.getCurrentEl();
    return SB_OK;
}

int X2Dome::dapiGotoAzEl(double dAz, double dEl)
{
    int nErr;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr = m_NexDome.gotoAzimuth(dAz);
    if(nErr)
        return ERR_CMDFAILED;

    else
        return SB_OK;
}

int X2Dome::dapiAbort(void)
{

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    m_NexDome.abortCurrentCommand();

    return SB_OK;
}

int X2Dome::dapiOpen(void)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!m_bHasShutterControl)
        return SB_OK;

    nErr = m_NexDome.openShutter();
    if(nErr)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiClose(void)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!m_bHasShutterControl)
        return SB_OK;

    nErr = m_NexDome.closeShutter();
    if(nErr)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiPark(void)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;
    /*
    if(m_bHasShutterControl)
    {
        nErr = nexDome.closeShutter();
        if(nErr)
            return ERR_CMDFAILED;
    }
     */
    
    nErr = m_NexDome.parkDome();
    if(nErr)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiUnpark(void)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;
    /*
    if(m_bHasShutterControl)
    {
        nErr = nexDome.openShutter();
        if(nErr)
            return ERR_CMDFAILED;
    }
     */
    
    nErr = m_NexDome.unparkDome();
    if(nErr)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiFindHome(void)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr = m_NexDome.goHome();
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsGotoComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr = m_NexDome.isGoToComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;
    return SB_OK;
}

int X2Dome::dapiIsOpenComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;
    
    if(!m_bHasShutterControl)
    {
        *pbComplete = true;
        return SB_OK;
    }

    nErr = m_NexDome.isOpenComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int	X2Dome::dapiIsCloseComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!m_bHasShutterControl)
    {
        *pbComplete = true;
        return SB_OK;
    }

    nErr = m_NexDome.isCloseComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsParkComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr = m_NexDome.isParkComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsUnparkComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr = m_NexDome.isUnparkComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsFindHomeComplete(bool* pbComplete)
{
    int nErr;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr = m_NexDome.isFindHomeComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiSync(double dAz, double dEl)
{
    int nErr;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr = m_NexDome.syncDome(dAz, dEl);
    if(nErr)
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

    snprintf(pszPort, nMaxSize,DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}



