#ifndef LOCALIPCSERVER_H
#define LOCALIPCSERVER_H

#include <QVector>
#include "ipc/iserver.h"
#include "ipc/iconnection.h"
#include "backend/backend.h"

// Local server for receive and execute commands from local processes (currently only from the CLI).
class LocalIPCServer : public QObject
{
    Q_OBJECT
public:
    explicit LocalIPCServer(Backend *backend, QObject *parent = nullptr);
    ~LocalIPCServer();

    void start();
    void sendLocationsShown();

signals:
    void showLocations();
    void connectToLocation(const LocationID &id);
    void attemptLogin(const QString &username, const QString &password, const QString &code2fa);

private slots:
    void onServerCallbackAcceptFunction(IPC::IConnection *connection);
    void onConnectionCommandCallback(IPC::Command *command, IPC::IConnection *connection);
    void onConnectionStateCallback(int state, IPC::IConnection *connection);

    void onBackendConnectStateChanged(const types::ConnectState &connectState);
    void onBackendFirewallStateChanged(bool isEnabled);
    void onBackendLoginFinished(bool isLoginFromSavedSettings);
    void onBackendSignOutFinished();
    void notifyCliSignOutFinished();
    void notifyCliLoginFinished();
    void notifyCliLoginFailed(LOGIN_RET loginError, const QString &errorMessage);

private:
    Backend *backend_;
    IPC::IServer *server_ = nullptr;
    QVector<IPC::IConnection *> connections_;
    bool isLoggedIn_ = false;

    void sendCommand(const IPC::Command &command);
    void sendLoginResult(bool isLoggedIn, const QString &errorMessage);
};

#endif // LOCALIPCSERVER_H
