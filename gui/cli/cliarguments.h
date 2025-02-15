#ifndef CLIARGUMENTS_H
#define CLIARGUMENTS_H

#include <QString>

enum CliCommand { CLI_COMMAND_NONE, CLI_COMMAND_HELP ,
                  CLI_COMMAND_CONNECT, CLI_COMMAND_CONNECT_BEST, CLI_COMMAND_CONNECT_LOCATION,
                  CLI_COMMAND_DISCONNECT,
                  CLI_COMMAND_FIREWALL_ON, CLI_COMMAND_FIREWALL_OFF,
                  CLI_COMMAND_LOCATIONS, CLI_COMMAND_LOGIN, CLI_COMMAND_SIGN_OUT };

class CliArguments
{
public:
    explicit CliArguments();
    void processArguments();

    CliCommand cliCommand() const;
    const QString &location() const;
    const QString &username() const;
    const QString &password() const;
    const QString &code2fa() const;
    bool keepFirewallOn() const;

private:
    CliCommand cliCommand_ = CLI_COMMAND_NONE;
    QString locationStr_ = "";
    QString username_;
    QString password_;
    QString code2fa_;
    bool keepFirewallOn_ = false;
};

#endif // CLIARGUMENTS_H
