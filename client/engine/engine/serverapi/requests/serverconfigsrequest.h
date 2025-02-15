#pragma once

#include "baserequest.h"

namespace server_api {

class ServerConfigsRequest : public BaseRequest
{
    Q_OBJECT
public:
    explicit ServerConfigsRequest(QObject *parent, const QString &authHash);

    QUrl url(const QString &domain) const override;
    QString name() const override;
    void handle(const QByteArray &arr) override;

    // output values
    QByteArray ovpnConfig() const { return ovpnConfig_; }

private:
    QString authHash_;

    // output values
    QByteArray ovpnConfig_;
};

} // namespace server_api {

