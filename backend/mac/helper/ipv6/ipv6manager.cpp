#include "ipv6manager.h"
#include <sstream>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include "../utils.h"
#include "../logger.h"

Ipv6Manager::Ipv6Manager() : enabled_(false)
{
}

Ipv6Manager::~Ipv6Manager()
{
    interfaceStates_.clear();
}

bool Ipv6Manager::setEnabled(bool bEnabled)
{
    if (bEnabled == enabled_) {
        LOG("IPv6 state already set");
        return false;
    }

    const std::vector<const std::string> interfaces = getInterfaces();
    if (interfaces.empty()) {
        LOG("Could not get interfaces");
        return false;
    }

    if (bEnabled) {
        LOG("Restoring IPv6");

        for (const std::string &interface : interfaces) {
            const IPv6State curState = getState(interface);
            const IPv6State savedState = interfaceStates_[interface];

            if (curState != savedState) {
                if (savedState.state == "Manual")
                    Utils::executeCommand("networksetup", {"-setv6manual", interface, savedState.ipAddress, savedState.prefixLength, savedState.routerAddress});
                else if (savedState.state == "Automatic")
                    Utils::executeCommand("networksetup", {"-setv6automatic", interface});
                else if (savedState.state == "Off")
                    Utils::executeCommand("networksetup", {"-setv6off", interface});
            }
        }
    } else {
        LOG("Disabling IPv6");

        saveIpv6States(interfaces);
        for (const std::string &interface : interfaces) {
            if (interfaceStates_[interface].state != "Off") {
                Utils::executeCommand("networksetup", {"-setv6off", interface});
            }
        }
    }

    enabled_ = bEnabled;
    return true;
}

void Ipv6Manager::saveIpv6States(const std::vector<const std::string> &interfaces)
{
    interfaceStates_.clear();
    for (const std::string &interface : interfaces) {
        IPv6State state = getState(interface);
        interfaceStates_[interface] = state;
        LOG("Save state for %s: %s, %s, %s, %s", interface.c_str(), state.state.c_str(), state.ipAddress.c_str(),
                                                state.routerAddress.c_str(), state.prefixLength.c_str());
    }
}

std::vector<const std::string> Ipv6Manager::getInterfaces()
{
    std::string out;
    std::vector<const std::string> interfaces;

    int status = Utils::executeCommand("networksetup", {"-listallnetworkservices"}, &out);
    if (status != 0) {
        LOG("Listing network services failed");
        return interfaces;
    }

    std::string str;
    std::stringstream ss(out);
    while (std::getline(ss, str)) {
        if (str.find("An asterisk") != std::string::npos) {
            continue;
        }
        interfaces.push_back(str);
    }

    return interfaces;
}

Ipv6Manager::IPv6State Ipv6Manager::getState(const std::string &interface)
{
    std::string out;
    int status = Utils::executeCommand("networksetup", {"-getinfo", interface}, &out);
    if (status != 0) {
        LOG("Get network info failed");
        return IPv6State();
    }

    IPv6State ipv6state;
    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        std::string::size_type posDelimiter = line.find(':');
        if (posDelimiter != std::string::npos) {
            std::string name, value;
            name = line.substr(0, posDelimiter);
            if (name.find("IPv6") != std::string::npos) {
                value = line.substr(posDelimiter + 1);
                boost::trim(name);
                boost::trim(value);
                if (name == "IPv6")
                    ipv6state.state = value;
                else if (name == "IPv6 IP address")
                    ipv6state.ipAddress = value;
                else if (name == "IPv6 Router")
                    ipv6state.routerAddress = value;
                else if (name == "IPv6 Prefix Length")
                    ipv6state.prefixLength = value;
                else
                    LOG("ipv6Manager::getState parse error, unknown name: %s", name.c_str());
            }
        }
    }

    return ipv6state;
}
