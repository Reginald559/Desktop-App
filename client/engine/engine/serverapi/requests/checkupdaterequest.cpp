#include "checkupdaterequest.h"

#include <QJsonDocument>

#include "utils/logger.h"
#include "version/appversion.h"
#include "utils/utils.h"
#include "engine/utils/urlquery_utils.h"

namespace server_api {

CheckUpdateRequest::CheckUpdateRequest(QObject *parent, UPDATE_CHANNEL updateChannel) : BaseRequest(parent, RequestType::kGet),
    updateChannel_(updateChannel)
{
}

QUrl CheckUpdateRequest::url(const QString &domain) const
{
    QUrl url("https://" + hostname(domain, SudomainType::kApi) + "/CheckUpdate");

    QUrlQuery query;

    query.addQueryItem("version", AppVersion::instance().version());
    query.addQueryItem("build", AppVersion::instance().build());

    if (updateChannel_ == UPDATE_CHANNEL_BETA)
        query.addQueryItem("beta", "1");
    else if (updateChannel_ == UPDATE_CHANNEL_GUINEA_PIG)
        query.addQueryItem("beta", "2");
    else if (updateChannel_ == UPDATE_CHANNEL_INTERNAL)
        query.addQueryItem("beta", "3");

    // add OS version and build
    QString osVersion, osBuild;
    Utils::getOSVersionAndBuild(osVersion, osBuild);

    if (!osVersion.isEmpty())
        query.addQueryItem("os_version", osVersion);

    if (!osBuild.isEmpty())
        query.addQueryItem("os_build", osBuild);

    urlquery_utils::addAuthQueryItems(query);
    urlquery_utils::addPlatformQueryItems(query);
    url.setQuery(query);
    return url;
}

QString CheckUpdateRequest::name() const
{
    return "CheckUpdate";
}

void CheckUpdateRequest::handle(const QByteArray &arr)
{
    QJsonParseError errCode;
    QJsonDocument doc = QJsonDocument::fromJson(arr, &errCode);
    if (errCode.error != QJsonParseError::NoError || !doc.isObject()) {
        qCDebugMultiline(LOG_SERVER_API) << arr;
        qCDebug(LOG_SERVER_API) << "Failed parse JSON for CheckUpdate";
        setNetworkRetCode(SERVER_RETURN_INCORRECT_JSON);
        return;
    }

    QJsonObject jsonObject = doc.object();

    if (jsonObject.contains("errorCode")) {
        // Putting this debug line here to help us quickly troubleshoot errors returned from the
        // server API, which hopefully should be few and far between.
        qCDebugMultiline(LOG_SERVER_API) << arr;
        setNetworkRetCode(SERVER_RETURN_INCORRECT_JSON);
        return;
    }

    if (!jsonObject.contains("data")) {
        qCDebugMultiline(LOG_SERVER_API) << arr;
        qCDebug(LOG_SERVER_API) << "Failed parse JSON for CheckUpdate";
        setNetworkRetCode(SERVER_RETURN_INCORRECT_JSON);
        return;
    }
    QJsonObject jsonData =  jsonObject["data"].toObject();
    if (!jsonData.contains("update_needed_flag")) {
        qCDebugMultiline(LOG_SERVER_API) << arr;
        qCDebug(LOG_SERVER_API) << "Failed parse JSON for CheckUpdate";
        setNetworkRetCode(SERVER_RETURN_INCORRECT_JSON);
        return;
    }

    int updateNeeded = jsonData["update_needed_flag"].toInt();
    if (updateNeeded != 1) {
        return;
    }

    if (!jsonData.contains("latest_version")) {
        qCDebugMultiline(LOG_SERVER_API) << arr;
        qCDebug(LOG_SERVER_API) << "Failed parse JSON for CheckUpdate";
        setNetworkRetCode(SERVER_RETURN_INCORRECT_JSON);
        return;
    }
    if (!jsonData.contains("update_url")) {
        qCDebugMultiline(LOG_SERVER_API) << arr;
        qCDebug(LOG_SERVER_API) << "Failed parse JSON for CheckUpdate";
        setNetworkRetCode(SERVER_RETURN_INCORRECT_JSON);
        return;
    }

    QString err;
    bool outSuccess;
    checkUpdate_ = types::CheckUpdate::createFromApiJson(jsonData, outSuccess, err);
    if (!outSuccess) {
        qCDebug(LOG_SERVER_API) << err;
        setNetworkRetCode(SERVER_RETURN_INCORRECT_JSON);
        return;
    }
}

} // namespace server_api {
