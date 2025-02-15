#include "servicecontrolmanager.h"

#include <NTSecAPI.h>
#include <sddl.h>

#include <codecvt>
#include <sstream>

namespace wsl
{

static std::string wstring_to_string(const std::wstring &wideStr)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(wideStr);
}

//---------------------------------------------------------------------------
// ServiceControlManager class implementation

/******************************************************************************
* METHOD:  ServiceControlManager constructor
*
* PURPOSE: Constructs an instance of the ServiceControlManager class and initializes the
*          member data.  The user must then call OpenSCM() to open a connection
*          to a specific Service Control Manager before calling any other
*          methods.
*******************************************************************************/
ServiceControlManager::ServiceControlManager()
    : m_hSCM(NULL),
      m_hService(NULL),
      m_bBlockStartStopRequests(false)
{
}


/******************************************************************************
* METHOD:  ServiceControlManager destructor
*
* PURPOSE: Destructs an instance of the ServiceControlManager class.
*
* INPUT:   None.
*******************************************************************************/
ServiceControlManager::~ServiceControlManager()
{
    closeSCM();
}


/******************************************************************************
* METHOD:  OpenSCM(LPCTSTR pszServerName)
*
* PURPOSE: Opens a connection to the specified SCM.
*
* INPUT:   dwDesiredAccess: desired access rights, as defined by the OpenSCManager
*             Win32 API.
*
*          pszServerName: Points to a null-terminated string that names the
*             target computer. If the pointer is NULL or points to an empty
*             string, the method connects to the SCM on the local computer.
*
* THROWS:  A system_error object.
*******************************************************************************/
void ServiceControlManager::openSCM(DWORD dwDesiredAccess, LPCTSTR pszServerName)
{
    closeSCM();

    m_hSCM = ::OpenSCManager(pszServerName, NULL, dwDesiredAccess);

    if (m_hSCM != NULL) {
        if (pszServerName != NULL) {
            m_sServerName = pszServerName;
        }
        return;
    }

    DWORD dwLastError = ::GetLastError();
    std::wostringstream errorMsg;

    switch (dwLastError) {
    case ERROR_ACCESS_DENIED:
        errorMsg << "OpenSCM: insufficient user rights to open SCM on server " << serverNameForDebug();
        break;

    case ERROR_INVALID_PARAMETER:
        errorMsg << "OpenSCM: one of the parameters is incorrect";
        break;

    default:
        errorMsg << "OpenSCM failed to open SCM on server " << serverNameForDebug() << " - " << dwLastError;
        break;
    }

    throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
}


/******************************************************************************
* METHOD:  CloseSCM()
*
* PURPOSE: Closes the open SCM and service handles.
*
* THROWS:  This method does not throw an exception.  The method may be called
*          for 'clean-up' after another ServiceControlManager method raises an exception.
*          Thus we don't want this method raising an exception thereby 'cancelling'
*          the first exception.
*******************************************************************************/
void
ServiceControlManager::closeSCM() noexcept
{
    closeService();

    if (m_hSCM != NULL) {
        ::CloseServiceHandle(m_hSCM);
        m_hSCM = NULL;
    }

    m_sServerName.clear();
}


/******************************************************************************
* METHOD:  OpenService(LPCTSTR pszServiceName)
*
* PURPOSE: Opens a handle to an existing service.
*          OpenSCM must be called before calling this method.
*
* INPUT:   pszServiceName: Points to a null-terminated string that names the
*             service to open. The maximum string length is 256 characters. The
*             SCM database preserves the case of the characters, but service
*             name comparisons are always case insensitive. A slash (/),
*             backslash (\), comma, and space are invalid service name
*             characters.
*
* THROWS:  A system_error object.
*******************************************************************************/
void
ServiceControlManager::openService(LPCTSTR pszServiceName, DWORD dwDesiredAccess)
{
    if (pszServiceName == NULL) {
        throw std::system_error(ERROR_INVALID_NAME, std::system_category(), "OpenService: the service name parameter cannot be null");
    }

    closeService();

    // Open the specified service.
    m_hService = ::OpenService(m_hSCM, pszServiceName, dwDesiredAccess);

    if (m_hService != NULL) {
        m_sServiceName = pszServiceName;
        return;
    }

    DWORD dwLastError = ::GetLastError();
    std::wostringstream errorMsg;

    switch (dwLastError) {
    case ERROR_ACCESS_DENIED:
        errorMsg << "OpenService: insufficient user rights to open service - " << pszServiceName;
        break;

    case ERROR_INVALID_HANDLE:
        errorMsg << "OpenService: the SCM is not open to access service - " << pszServiceName;
        break;

    case ERROR_INVALID_NAME:
        errorMsg << "OpenService: service name parameter has an invalid format - " << pszServiceName;
        break;

    case ERROR_SERVICE_DOES_NOT_EXIST:
        errorMsg << "OpenService: no service with the specified name exists on this server - " << pszServiceName;
        break;

    default:
        errorMsg << "OpenService failed to open service " << pszServiceName << " - " << dwLastError;
        break;
    }

    throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
}


/******************************************************************************
* METHOD:  CloseService()
*
* PURPOSE: Closes an open service handle.
*
* INPUT:   None.
*
* THROWS:  This method does not throw an exception.  The method may be called
*          for 'clean-up' after another ServiceControlManager method raises an exception.
*          Thus we don't want this method raising an exception thereby 'cancelling'
*          the first exception.
*******************************************************************************/
void
ServiceControlManager::closeService() noexcept
{
    if (m_hService != NULL) {
        ::CloseServiceHandle(m_hService);
        m_hService = NULL;
        m_sServiceName.clear();
    }
}


/******************************************************************************
* METHOD:  QueryServiceStatus(DWORD& dwStatus)
*
* PURPOSE: Determines the status (started, stopped, pending, etc.) of the
*          service.  OpenService must be called before calling this method.
*
* OUTPUT:  dwStatus: receives one of the following status codes:
*
*          SERVICE_STOPPED
*          SERVICE_START_PENDING
*          SERVICE_STOP_PENDING
*          SERVICE_RUNNING
*          SERVICE_CONTINUE_PENDING
*          SERVICE_PAUSE_PENDING
*          SERVICE_PAUSED
*
* THROWS:  A system_error object.
*******************************************************************************/
DWORD
ServiceControlManager::queryServiceStatus() const
{
    SERVICE_STATUS status;
    if (::QueryServiceStatus(m_hService, &status)) {
        return status.dwCurrentState;
    }

    DWORD dwLastError = ::GetLastError();
    std::wostringstream errorMsg;

    switch (dwLastError) {
    case ERROR_ACCESS_DENIED:
        errorMsg << "QueryServiceStatus: insufficient user rights to query the service - " << m_sServiceName;
        break;

    case ERROR_INVALID_HANDLE:
        errorMsg << "QueryServiceStatus: the service has not been opened - " << m_sServiceName;
        break;

    default:
        errorMsg << "QueryServiceStatus failed to query the service " << m_sServiceName << " - " << dwLastError;
        break;
    }

    throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
}


/******************************************************************************
* METHOD:  QueryServiceConfig(std::string& sExePath, std::string& sAccountName,
*                             DWORD& dwStartType, bool& bServiceShareProcess)
*
* PURPOSE: Retrieves the configuration parameters of the service.
*          OpenService must be called before calling this method.
*
* OUTPUT:  sExePath: receives the full path to the service's executable.
*
*          sAccountName: receives the account name in the form of
*             DomainName\Username, which the service process will be logged
*             on as when it runs.
*
*          dwStartType: receives one of the following status codes:
*
*             SERVICE_AUTO_START    - NT starts service automatically
*             SERVICE_DEMAND_START  - user must start the service
*             SERVICE_DISABLED      - service cannot be started
*
*          bServiceShareProcess: true if the service shares a process with
*             other services.
*
* THROWS:  A system_error object.
*******************************************************************************/
void
ServiceControlManager::queryServiceConfig(std::wstring &sExePath, std::wstring &sAccountName,
                                          DWORD& dwStartType, bool& bServiceShareProcess) const
{
    DWORD dwNumBytesNeeded = 0;
    DWORD dwBufferSize = 1024;
    std::unique_ptr< unsigned char[] > buffer(new unsigned char[dwBufferSize]);

    bool b2ndTry = false;

    for (int i = 0; i < 2; i++) {
        if (::QueryServiceConfig(m_hService, (LPQUERY_SERVICE_CONFIG)buffer.get(),
                                 dwBufferSize, &dwNumBytesNeeded)) {
            break;
        }

        DWORD dwLastError = ::GetLastError();
        std::wostringstream errorMsg;

        switch (dwLastError) {
        case ERROR_INSUFFICIENT_BUFFER:
            if (b2ndTry) {
                errorMsg << "QueryServiceConfig: insufficient buffer space to query the service's configuration - " << m_sServiceName;
                break;
            }

            buffer.reset(new unsigned char[dwNumBytesNeeded]);
            dwNumBytesNeeded = 0;
            b2ndTry = true;
            continue;

        case ERROR_ACCESS_DENIED:
            errorMsg << "QueryServiceConfig: insufficient user rights to query the service's configuration - " << m_sServiceName;
            break;

        case ERROR_INVALID_HANDLE:
            errorMsg << "QueryServiceConfig: the service has not been opened - " << m_sServiceName;
            break;

        default:
            errorMsg << "QueryServiceConfig failed to query the service " << m_sServiceName << " - " << dwLastError;
            break;
        }

        throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
    }

    LPQUERY_SERVICE_CONFIG lpConfig = (LPQUERY_SERVICE_CONFIG)buffer.get();
    dwStartType          = lpConfig->dwStartType;
    sExePath             = lpConfig->lpBinaryPathName;
    bServiceShareProcess = (lpConfig->dwServiceType == SERVICE_WIN32_SHARE_PROCESS);
    sAccountName         = lpConfig->lpServiceStartName;
}


/******************************************************************************
* METHOD:  StartService()
*
* PURPOSE: Instructs the SCM to start the service.  OpenService must be called
*          before calling this method.
*
* INPUT:   None.
*
* THROWS:  A system_error object.
*******************************************************************************/
void
ServiceControlManager::startService()
{
    if (m_bBlockStartStopRequests) {
        throw std::system_error(ERROR_CANCELLED, std::system_category(), std::string("StartService: ") + wstring_to_string(m_sServiceName));
    }

    std::wostringstream errorMsg;

    ULONGLONG elapsedTime;
    ULONGLONG startTime = ::GetTickCount64();

    if (::StartService(m_hService, 0, NULL)) {
        // Wait for start service command to complete.
        for (int i = 0; !m_bBlockStartStopRequests && i < 80; i++) {
            DWORD dwStatus = queryServiceStatus();
            if (dwStatus == SERVICE_RUNNING) {
                return;
            }

            ::Sleep(250);
        }

        if (m_bBlockStartStopRequests) {
            throw std::system_error(ERROR_CANCELLED, std::system_category(), std::string("StartService: ") + wstring_to_string(m_sServiceName));
        }

        elapsedTime = ::GetTickCount64() - startTime;
        errorMsg << "StartService(" << m_sServiceName << ") API request succeeded, but the service did not report as running after " << elapsedTime << "ms";
        throw std::system_error(ERROR_SERVICE_REQUEST_TIMEOUT, std::system_category(), wstring_to_string(errorMsg.str()));
    }

    elapsedTime = ::GetTickCount64() - startTime;

    DWORD dwLastError = ::GetLastError();

    if (dwLastError == ERROR_SERVICE_ALREADY_RUNNING) {
        return;
    }

    switch (dwLastError) {
    case ERROR_ACCESS_DENIED:
        errorMsg << "StartService: insufficient user rights to request the service to start - " << m_sServiceName;
        break;

    case ERROR_INVALID_HANDLE:
        errorMsg << "StartService: the service has not been opened - " << m_sServiceName;
        break;

    case ERROR_PATH_NOT_FOUND:
        errorMsg << "StartService: the executable for the service could not be found - " << m_sServiceName;
        break;

    case ERROR_SERVICE_DATABASE_LOCKED:
        errorMsg << "StartService: the SCM database is locked by another process - " << m_sServiceName;
        break;

    case ERROR_SERVICE_DEPENDENCY_DELETED:
        errorMsg << "StartService: the service depends on a service that does not exist or has been marked for deletion - " << m_sServiceName;
        break;

    case ERROR_SERVICE_DEPENDENCY_FAIL:
        errorMsg << "StartService: the service depends on another service that has failed to start - " << m_sServiceName;
        break;

    case ERROR_SERVICE_DISABLED:
        errorMsg << "StartService: the service has been disabled - " << m_sServiceName;
        break;

    case ERROR_SERVICE_LOGON_FAILED:
        errorMsg << "StartService: the service failed to logon.  Check its startup logon parameters in the control panel - " << m_sServiceName;
        break;

    case ERROR_SERVICE_MARKED_FOR_DELETE:
        errorMsg << "StartService: the service has been marked for deletion - " << m_sServiceName;
        break;

    case ERROR_SERVICE_NO_THREAD:
        errorMsg << "StartService: a thread could not be created for the service - " << m_sServiceName;
        break;

    case ERROR_SERVICE_REQUEST_TIMEOUT:
        errorMsg << "StartService(" << m_sServiceName << ") API request failed after " << elapsedTime << "ms";
        break;

    default:
        errorMsg << "StartService failed to start the service " << m_sServiceName << " - " << dwLastError;
        break;
    }

    throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
}


/******************************************************************************
* METHOD:  StopService()
*
* PURPOSE: Instructs the SCM to stop the service.  OpenService must be called
*          before calling this method.
*
* INPUT:   None.
*
* THROWS:  A system_error object.
*******************************************************************************/
void
ServiceControlManager::stopService()
{
    if (m_bBlockStartStopRequests) {
        throw std::system_error(ERROR_CANCELLED, std::system_category(), std::string("StopService: ") + wstring_to_string(m_sServiceName));
    }

    SERVICE_STATUS status;
    if (::ControlService(m_hService, SERVICE_CONTROL_STOP, &status)) {
        // Wait for stop service command to complete.
        DWORD dwStatus = status.dwCurrentState;

        for (int i = 0; (!m_bBlockStartStopRequests && (dwStatus != SERVICE_STOPPED) && (i < 80)); i++) {
            ::Sleep(250);
            dwStatus = queryServiceStatus();
        }

        if (dwStatus == SERVICE_STOPPED) {
            return;
        }

        DWORD dwError = (m_bBlockStartStopRequests ? ERROR_CANCELLED : ERROR_SERVICE_REQUEST_TIMEOUT);
        throw std::system_error(dwError, std::system_category(), std::string("StopService: ") + wstring_to_string(m_sServiceName));
    }

    DWORD dwLastError = ::GetLastError();

    if (dwLastError == ERROR_SERVICE_NOT_ACTIVE) {
        return;
    }

    std::wostringstream errorMsg;

    switch (dwLastError) {
    case ERROR_ACCESS_DENIED:
        errorMsg << "StopService: insufficient user rights request the service to stop - " << m_sServiceName;
        break;

    case ERROR_INVALID_HANDLE:
        errorMsg << "StopService: the service has not been opened - " << m_sServiceName;
        break;

    case ERROR_DEPENDENT_SERVICES_RUNNING:
        errorMsg << "StopService: the service cannot be stopped because other running services are dependent on it - " << m_sServiceName;
        break;

    case ERROR_INVALID_SERVICE_CONTROL:
        errorMsg << "StopService: the service does not accept stop requests - " << m_sServiceName;
        break;

    case ERROR_SERVICE_CANNOT_ACCEPT_CTRL:
        errorMsg << "StopService: the service is already stopped or is in a pending state - " << m_sServiceName;
        break;

    case ERROR_SERVICE_REQUEST_TIMEOUT:
        errorMsg << "StopService: " << m_sServiceName;
        break;

    default:
        errorMsg << "StopService failed to stop the service " << m_sServiceName << " - " << dwLastError;
        break;
    }

    throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
}


/******************************************************************************
* METHOD:  DeleteService(LPCTSTR pszServiceName)
*
* PURPOSE: Deletes the service from the SCM database.
*          OpenSCM must be called before calling this method.
*
* INPUT:   pszServiceName: Points to a null-terminated string that names the
*             service to delete. The maximum string length is 256 characters.
*             The SCM database preserves the character's case, but service
*             name comparisons are always case insensitive. A slash (/),
*             backslash (\), comma, and space are invalid service name
*             characters.
*
*          bStopRunningService: attempt to stop the service, if it's running,
*             before asking the SCM to delete it.
*
* THROWS:  A system_error object.
*******************************************************************************/
void
ServiceControlManager::deleteService(LPCTSTR pszServiceName, bool bStopRunningService)
{
    openService(pszServiceName);

    if (bStopRunningService) {
        if (queryServiceStatus() != SERVICE_STOPPED) {
            stopService();
        }
    }

    BOOL bResult = ::DeleteService(m_hService);

    // Get error code before CloseService possibly changes it.
    DWORD dwLastError = ::GetLastError();

    // Close our handle to the service so the SCM can delete it.
    closeService();

    if (bResult) {
        return;
    }

    std::wostringstream errorMsg;

    switch (dwLastError) {
    case ERROR_ACCESS_DENIED:
        errorMsg << "DeleteService: insufficient user rights to delete service - " << pszServiceName;
        break;

    case ERROR_INVALID_HANDLE:
        errorMsg << "DeleteService: the SCM is not open - " << pszServiceName;
        break;

    case ERROR_SERVICE_MARKED_FOR_DELETE:
        errorMsg << "DeleteService: service has already been marked for deletion - " << pszServiceName;
        break;

    default:
        errorMsg << "DeleteService failed to delete the service " << pszServiceName << " - " << dwLastError;
        break;
    }

    throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
}


/******************************************************************************
* METHOD:  ServiceInstalled(LPCTSTR pszServiceName)
*
* PURPOSE: Queries the SCM database to see if the service is installed.
*          OpenSCM must be called before calling this method.
*
* INPUT:   pszServiceName: Points to a null-terminated string that names the
*             service to query. The maximum string length is 256 characters.
*             The SCM database preserves the character's case, but service
*             name comparisons are always case insensitive. A slash (/),
*             backslash (\), comma, and space are invalid service name
*             characters.
*
* THROWS:  A system_error object.
*******************************************************************************/
bool
ServiceControlManager::isServiceInstalled(LPCTSTR pszServiceName) const
{
    if (pszServiceName == NULL) {
        throw std::system_error(ERROR_INVALID_NAME, std::system_category(), "ServiceInstalled: the service name parameter cannot be null");
    }

    SC_HANDLE hService = ::OpenService(m_hSCM, pszServiceName, SERVICE_QUERY_STATUS);

    bool bInstalled = (hService != NULL);

    if (bInstalled) {
        ::CloseServiceHandle(hService);
    }

    return bInstalled;
}


/******************************************************************************
* METHOD:  InstallService(LPCTSTR pszServiceName, LPCTSTR pszBinaryPathName,
*                         LPCTSTR pszDisplayName, DWORD dwServiceType,
*                         DWORD dwStartType, LPCTSTR pszDependencies,
*                         LPCTSTR pszAccountName, LPCTSTR pszPassword)
*
* PURPOSE: Installs a service.
*          OpenSCM must be called before calling this method.
*
* INPUT:   pszServiceName: Points to a null-terminated string that names the
*             service to install. The maximum string length is 256 characters.
*             The SCM database preserves the character's case, but service
*             name comparisons are always case insensitive. A slash (/),
*             backslash (\), comma, and space are invalid service name
*             characters.
*
*          pszBinaryPathName: Points to a null-terminated string that contains
*             the fully qualified path to the service binary file.
*
*          pszDisplayName: Points to a null-terminated string that is to be
*             used by user interface programs to identify the service. This
*             string has a maximum length of 256 characters. The name is
*             case-preserved in the SCM. display name comparisons are always
*             case-insensitive.
*
*          pszDescription: Points to a null-terminated string that specifies
*             the description of the service.
*
*          dwServiceType: specify one of the following service types:
*
*             SERVICE_WIN32_OWN_PROCESS
*             SERVICE_WIN32_SHARE_PROCESS
*
*          dwStartType: specify one of the following start types:
*
*             SERVICE_AUTO_START   - started automatically by the SCM during
*                                    system startup.
*
*             SERVICE_DEMAND_START - started by the SCM when a process calls
*                                    the StartService function.
*
*          pszDependencies: Points to a double null-terminated array of
*             null-separated names of services that the system must start
*             before this service. Specify NULL or an empty string if the
*             service has no dependencies.
*
*          bAllowInteractiveUserStart: if true logged in non-admin users can
*             start the service.
*
* THROWS:  A system_error object.
*******************************************************************************/
void
ServiceControlManager::installService(LPCTSTR pszServiceName,  LPCTSTR pszBinaryPathName,
                                      LPCTSTR pszDisplayName,  LPCTSTR pszDescription,
                                      DWORD  dwServiceType,   DWORD  dwStartType,
                                      LPCTSTR pszDependencies, bool   bAllowInteractiveUserStart)
{
    // Can only call this method to install a service on a local machine.
    if (!m_sServerName.empty()) {
        throw std::system_error(ERROR_NOT_SUPPORTED, std::system_category(), "InstallService can only be run on the local machine");
    }

    m_hService = ::CreateService(m_hSCM, pszServiceName, pszDisplayName,
                                 SERVICE_ALL_ACCESS, dwServiceType, dwStartType,
                                 SERVICE_ERROR_NORMAL, pszBinaryPathName, NULL, NULL,
                                 pszDependencies, NULL, NULL);
    if (m_hService != NULL) {
        setServiceDescription(pszDescription);

        if (bAllowInteractiveUserStart) {
            grantUserStartPermission();
        }

        return;
    }

    DWORD dwLastError = ::GetLastError();
    std::wostringstream errorMsg;

    switch (dwLastError) {
    case ERROR_ACCESS_DENIED:
        errorMsg << "InstallService: insufficient user rights to install service - " << pszServiceName;
        break;

    case ERROR_INVALID_HANDLE:
        errorMsg << "InstallService: the SCM is not open - " << pszServiceName;
        break;

    case ERROR_INVALID_NAME:
        errorMsg << "InstallService: service name has an invalid format - " << pszServiceName;
        break;

    case ERROR_SERVICE_EXISTS:
        errorMsg << "InstallService: a service with this name already exists on this server - " << pszServiceName;
        break;

    case ERROR_INVALID_PARAMETER:
        errorMsg << "InstallService: one of the parameters is incorrect - " << pszServiceName;
        break;

    case ERROR_CIRCULAR_DEPENDENCY:
        errorMsg << "InstallService: a circular service dependency was specified - " << pszServiceName;
        break;

    case ERROR_DUP_NAME:
        errorMsg << "InstallService: the display name already exists in the SCM database either as a service name or as another display name - " << pszDisplayName;
        break;

    default:
        errorMsg << "InstallService failed to install the service " << pszServiceName << " - " << dwLastError;
        break;
    }

    throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
}


/******************************************************************************
* METHOD:  SendControlCode(DWORD dwCode)
*
* PURPOSE: Sends a control code to the service.  The service will ignore the
*          code if it does not recognize it.
*          OpenService must be called before calling this method.
*
* INPUT:   dwCode: the control code to send.
*
* THROWS:  A system_error object.
*******************************************************************************/
void
ServiceControlManager::sendControlCode(DWORD dwCode) const
{
    DWORD dwLastError;

    if ((dwCode >= 128) && (dwCode <= 256)) {
        SERVICE_STATUS status;
        if (::ControlService(m_hService, dwCode, &status)) {
            return;
        }

        dwLastError = ::GetLastError();
    }
    else {
        dwLastError = ERROR_INVALID_SERVICE_CONTROL;
    }

    std::wostringstream errorMsg;

    switch (dwLastError) {
    case ERROR_SERVICE_NOT_ACTIVE:
        errorMsg << "SendControlCode: the service is not running - " << m_sServiceName;
        break;

    case ERROR_ACCESS_DENIED:
        errorMsg << "SendControlCode: insufficient user rights to control the service - " << m_sServiceName;
        break;

    case ERROR_INVALID_HANDLE:
        errorMsg << "SendControlCode: the service has not been opened - " << m_sServiceName;
        break;

    case ERROR_DEPENDENT_SERVICES_RUNNING:
        errorMsg << "SendControlCode: the service cannot be controlled because other running services are dependent on it- " << m_sServiceName;
        break;

    case ERROR_INVALID_SERVICE_CONTROL:
        errorMsg << "SendControlCode: the service does not accept user control code " << dwCode << " - " << m_sServiceName;
        break;

    case ERROR_SERVICE_CANNOT_ACCEPT_CTRL:
        errorMsg << "SendControlCode: the service is not in a state where it can accept user control codes - " << m_sServiceName;
        break;

    case ERROR_SERVICE_REQUEST_TIMEOUT:
        errorMsg << "SendControlCode: the service did not respond to the control request in a timely fashion - " << m_sServiceName;
        break;

    default:
        errorMsg << "SendControlCode failed to control the service " << m_sServiceName << " - " << dwLastError;
        break;
    }

    throw std::system_error(dwLastError, std::system_category(), wstring_to_string(errorMsg.str()));
}


/******************************************************************************
* METHOD:  GetServerName(void) const
*
* PURPOSE: Returns the name of the server whose SCM we are attached to.
*
* INPUT:   None.
*
* RETURNS: NULL if we are attached to the local SCM.
*******************************************************************************/
LPCTSTR
ServiceControlManager::getServerName() const
{
    if (m_sServerName.empty()) {
        return NULL;
    }

    return m_sServerName.c_str();
}


//---------------------------------------------------------------------------
void
ServiceControlManager::setServiceDescription(LPCTSTR pszDescription) const
{
    if (pszDescription != NULL) {
        SERVICE_DESCRIPTION svcDesc = { (LPTSTR)pszDescription };
        BOOL bResult = ::ChangeServiceConfig2(m_hService, SERVICE_CONFIG_DESCRIPTION, &svcDesc);

        if (bResult == FALSE) {
            DWORD dwLastError = ::GetLastError();
            throw std::system_error(dwLastError, std::system_category(),
                std::string("SetServiceDescription: ChangeServiceConfig2 failed - ") + std::to_string(dwLastError));
        }
    }
}

//---------------------------------------------------------------------------
std::wstring ServiceControlManager::serverNameForDebug() const
{
    if (m_sServerName.empty()) {
        return std::wstring(L"local_pc");
    }

    return m_sServerName;
}

//---------------------------------------------------------------------------
void
ServiceControlManager::setServiceSIDType(DWORD dwServiceSidType) const
{
    SERVICE_SID_INFO ssi;
    ssi.dwServiceSidType = dwServiceSidType;
    BOOL bResult = ::ChangeServiceConfig2(m_hService, SERVICE_CONFIG_SERVICE_SID_INFO, &ssi);

    if (bResult == FALSE) {
        DWORD dwLastError = ::GetLastError();
        throw std::system_error(dwLastError, std::system_category(),
            std::string("SetServiceSIDType: ChangeServiceConfig2 failed - ") + std::to_string(dwLastError));
    }
}

//---------------------------------------------------------------------------
void
ServiceControlManager::grantUserStartPermission() const
{
    wchar_t sddl[] = L"D:"
      L"(A;;CCLCSWRPWPDTLOCRRC;;;SY)"           // default permissions for local system
      L"(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)"   // default permissions for administrators
      L"(A;;CCLCSWLOCRRC;;;AU)"                 // default permissions for authenticated users
      L"(A;;CCLCSWRPWPDTLOCRRC;;;PU)"           // default permissions for power users
      L"(A;;RP;;;IU)"                           // added permission: start service for interactive users
      ;

    // If you also want non-admin users to be able to stop the service, add the WP right to the last line above.
    // i.e. "(A;;RPWP;;;IU)"

    PSECURITY_DESCRIPTOR sd;
    BOOL bResult = ::ConvertStringSecurityDescriptorToSecurityDescriptor(sddl, SDDL_REVISION_1, &sd, NULL);

    if (bResult == FALSE) {
        DWORD dwLastError = ::GetLastError();
        throw std::system_error(dwLastError, std::system_category(),
            std::string("grantUserStartPermission: ConvertStringSecurityDescriptorToSecurityDescriptorA failed - ") + std::to_string(dwLastError));
    }

    bResult = ::SetServiceObjectSecurity(m_hService, DACL_SECURITY_INFORMATION, sd);

    if (bResult == FALSE) {
        DWORD dwLastError = ::GetLastError();
        throw std::system_error(dwLastError, std::system_category(),
            std::string("grantUserStartPermission: SetServiceObjectSecurity failed - ") + std::to_string(dwLastError));
    }
}

} // end namespace wsl
