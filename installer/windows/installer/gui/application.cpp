#include "Application.h"

#include <VersionHelpers.h>
#include <system_error>

#include "ImageResources.h"
#include "../installer/installer.h"
#include "../installer/settings.h"
#include "../../utils/applicationinfo.h"
#include "../../utils/directory.h"
#include "../../utils/logger.h"
#include "../../utils/path.h"
#include "../../utils/registry.h"
#include "../../utils/utils.h"
#include "wincryptutils.h"
#include "wsscopeguard.h"

#pragma comment(lib, "gdiplus.lib")


Application *g_application = NULL;

Application::Application(HINSTANCE hInstance, int nCmdShow, bool isAutoUpdateMode,
                         bool isSilent, bool noDrivers, bool noAutoStart, bool isFactoryReset,
                         const std::wstring& installPath, const std::wstring &username, const std::wstring &password) :
    hInstance_(hInstance),
    nCmdShow_(nCmdShow),
    isAutoUpdateMode_(isAutoUpdateMode),
    isSilent_(isSilent),
    username_(username),
    password_(password)
{
    g_application = this;

    auto callback = std::bind(&Application::installerCallback, this,
                              std::placeholders::_1, std::placeholders::_2);

    installer_.reset(new Installer(callback));

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
    {
        gdiplusToken_ = NULL;
    }

    Settings::instance().readFromRegistry();
    const std::wstring& existingInstallFolder = Settings::instance().getPath();

    if (!isAutoUpdateMode_ && !installPath.empty()) {
        Settings::instance().setPath(installPath);
    }
    else if (existingInstallFolder.empty() || !Directory::DirExists(existingInstallFolder) ||
             Utils::in32BitProgramFilesFolder(existingInstallFolder) ||
             !Path::isOnSystemDrive(existingInstallFolder))
    {
        // We don't have an install folder specified in the Registry, or the folder specified
        // in the Registry no longer exists, indicating the user uninstalled the app.  Or we do
        // have an old install folder but it is referencing the 32-bit Program Files (x86) folder,
        // in which case we want to retarget the install path to the 64-bit Program Files folder.
        // Lastly, for security purposes, we only allow installation on the system drive.
        Settings::instance().setPath(Utils::defaultInstallPath());
    }

    Settings::instance().setInstallDrivers(!isSilent_ && !noDrivers);
    Settings::instance().setAutoStart(!isSilent_ && !noAutoStart);
    Settings::instance().setFactoryReset(isFactoryReset);

    imageResources_ = new ImageResources();
    fontResources_ = new FontResources();
    mainWindow_ = new MainWindow();
}

Application::~Application()
{
    g_application = NULL;

    delete mainWindow_;
    delete fontResources_;
    delete imageResources_;

    if (gdiplusToken_)
    {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
    }
}

bool Application::init(int windowCenterX, int windowCenterY)
{
    if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) != S_OK) {
        return false;
    }

    if (gdiplusToken_ == NULL) {
        return false;
    }

    if (!imageResources_->init()) {
        return false;
    }

    if (!fontResources_->init()) {
        return false;
    }

    bool bSuccess = mainWindow_->create(windowCenterX, windowCenterY);

    if (bSuccess)
    {
        if (isAutoUpdateMode_) {
            mainWindow_->gotoAutoUpdateMode();
        }
        else if (isSilent_) {
            mainWindow_->gotoSilentInstall();
        }
    }

    return bSuccess;
}

int Application::exec()
{
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Settings::instance().writeToRegistry();

    return static_cast<int> (msg.wParam);
}

void Application::installerCallback(unsigned int progress, INSTALLER_CURRENT_STATE state)
{
    // transform to window messages
    PostMessage(mainWindow_->getHwnd(), WI_INSTALLER_STATE, progress, state);
}


// return path of installed Windscribe app or empty string if not installed
std::wstring Application::getPreviousInstallPath()
{
    HKEY hKey = NULL;
    auto exitGuard = wsl::wsScopeGuard([&] {
        if (hKey != NULL) {
            ::RegCloseKey(hKey);
        }
    });

    // Check the 64-bit hive first.  If the entry is not found, check if it exists in the 32-bit hive
    // from an old 32-bit install.
    std::wstring subkeyName = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + ApplicationInfo::instance().getId() + L"_is1";
    auto status = Registry::RegOpenKeyExView(rvDefault, HKEY_LOCAL_MACHINE, subkeyName.c_str(), 0, KEY_QUERY_VALUE, hKey);

    if (status != ERROR_SUCCESS) {
        status = Registry::RegOpenKeyExView(rv32Bit, HKEY_LOCAL_MACHINE, subkeyName.c_str(), 0, KEY_QUERY_VALUE, hKey);
    }

    if (status == ERROR_SUCCESS) {
        std::wstring path;
        if (Registry::RegQueryStringValue1(hKey, L"InstallLocation", path)) {
            return path;
        }
    }

    return std::wstring(L"");
}

void Application::saveCredentials() const
{
    if (username_.empty() || password_.empty()) {
        return;
    }

    try {
        const auto encodedUsername = wsl::WinCryptUtils::encrypt(username_, wsl::WinCryptUtils::EncodeHex);
        const auto encodedPassword = wsl::WinCryptUtils::encrypt(password_, wsl::WinCryptUtils::EncodeHex);

        bool result = Registry::RegWriteStringValue(HKEY_CURRENT_USER, L"Software\\Windscribe\\Windscribe2", L"username", encodedUsername);
        if (!result) {
            throw std::system_error(0, std::generic_category(), "Application::saveCredentials() failed to write username to Registry");
        }

        result = Registry::RegWriteStringValue(HKEY_CURRENT_USER, L"Software\\Windscribe\\Windscribe2", L"password", encodedPassword);
        if (!result) {
            throw std::system_error(0, std::generic_category(), "Application::saveCredentials() failed to write password to Registry");
        }

        // Clear any existing authHash from a previous login.
        Registry::regDeleteProperty(HKEY_CURRENT_USER, L"Software\\Windscribe\\Windscribe2", L"authHash");
    }
    catch (std::system_error& ex) {
        Log::instance().out("Application::saveCredentials() %s (%lu)", ex.what(), ex.code().value());
    }
}
