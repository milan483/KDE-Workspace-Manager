#include "kwin/kwin_snapshot_parser.h"

#include "profiles/profile_validator.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

#include <cmath>
#include <limits>
#include <utility>

namespace kwm::kwin
{
namespace
{

using namespace profiles;

ProfileError structureError(const QString &path, const QString &message)
{
    return {ProfileErrorCode::InvalidStructure, message, path, {}};
}

ProfileError invalidArgument(const QString &path, const QString &message)
{
    return {ProfileErrorCode::InvalidArgument, message, path, {}};
}

bool requiredString(const QJsonObject &object, const QString &key, const QString &path, QString &result,
                    ProfileError &error)
{
    const QJsonValue value = object.value(key);
    const QString propertyPath = path.isEmpty() ? key : path + QLatin1Char('.') + key;
    if (!value.isString() || value.toString().trimmed().isEmpty())
    {
        error = structureError(propertyPath, QStringLiteral("Required property must be a non-empty string."));
        return false;
    }
    result = value.toString();
    return true;
}

bool requiredBoolean(const QJsonObject &object, const QString &key, const QString &path, bool &result,
                     ProfileError &error)
{
    const QJsonValue value = object.value(key);
    const QString propertyPath = path.isEmpty() ? key : path + QLatin1Char('.') + key;
    if (!value.isBool())
    {
        error = structureError(propertyPath, QStringLiteral("Required property must be a boolean."));
        return false;
    }
    result = value.toBool();
    return true;
}

bool requiredCount(const QJsonObject &object, const QString &key, const QString &path, int &result,
                   ProfileError &error)
{
    const QJsonValue value = object.value(key);
    const QString propertyPath = path.isEmpty() ? key : path + QLatin1Char('.') + key;
    if (!value.isDouble())
    {
        error = structureError(propertyPath, QStringLiteral("Required property must be a non-negative integer."));
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < 0.0 || number > static_cast<double>(std::numeric_limits<int>::max()))
    {
        error = structureError(propertyPath, QStringLiteral("Required property must be a non-negative integer."));
        return false;
    }
    result = static_cast<int>(number);
    return true;
}

bool requiredNumber(const QJsonObject &object, const QString &key, const QString &path, double &result,
                    ProfileError &error)
{
    const QJsonValue value = object.value(key);
    const QString propertyPath = path.isEmpty() ? key : path + QLatin1Char('.') + key;
    if (!value.isDouble() || !std::isfinite(value.toDouble()))
    {
        error = structureError(propertyPath, QStringLiteral("Required property must be a finite number."));
        return false;
    }
    result = value.toDouble();
    return true;
}

bool requiredObject(const QJsonObject &object, const QString &key, const QString &path, QJsonObject &result,
                    ProfileError &error)
{
    const QJsonValue value = object.value(key);
    const QString propertyPath = path.isEmpty() ? key : path + QLatin1Char('.') + key;
    if (!value.isObject())
    {
        error = structureError(propertyPath, QStringLiteral("Required property must be an object."));
        return false;
    }
    result = value.toObject();
    return true;
}

bool parseGeometry(const QJsonObject &object, const QString &path, std::optional<Geometry> &geometry,
                   ProfileError &error)
{
    if (!object.contains(QStringLiteral("geometry")))
    {
        return true;
    }

    QJsonObject geometryObject;
    const QString geometryPath = path + QStringLiteral(".geometry");
    if (!requiredObject(object, QStringLiteral("geometry"), path, geometryObject, error))
    {
        return false;
    }

    Geometry parsed;
    if (!requiredNumber(geometryObject, QStringLiteral("x"), geometryPath, parsed.x, error) ||
        !requiredNumber(geometryObject, QStringLiteral("y"), geometryPath, parsed.y, error) ||
        !requiredNumber(geometryObject, QStringLiteral("width"), geometryPath, parsed.width, error) ||
        !requiredNumber(geometryObject, QStringLiteral("height"), geometryPath, parsed.height, error))
    {
        return false;
    }
    geometry = parsed;
    return true;
}

} // namespace

ProfileResult<KWinCaptureSnapshot> KWinSnapshotParser::parse(const QByteArray &responseJson,
                                                              const QString &expectedRequestId,
                                                              const QString &profileName)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(responseJson, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        return ProfileError{ProfileErrorCode::InvalidJson, parseError.errorString(), {}, {}};
    }
    if (!document.isObject())
    {
        return structureError({}, QStringLiteral("KWin bridge response must be a JSON object."));
    }

    const QJsonObject response = document.object();
    if (response.value(QStringLiteral("protocol_version")).toInt(-1) != ProtocolVersion)
    {
        return invalidArgument(QStringLiteral("protocol_version"), QStringLiteral("Unsupported KWin bridge protocol version."));
    }

    QString requestId;
    ProfileError error;
    if (!requiredString(response, QStringLiteral("request_id"), {}, requestId, error))
    {
        return error;
    }
    if (requestId != expectedRequestId)
    {
        return invalidArgument(QStringLiteral("request_id"), QStringLiteral("KWin bridge response does not match the pending request."));
    }

    QString operation;
    if (!requiredString(response, QStringLiteral("operation"), {}, operation, error))
    {
        return error;
    }
    if (operation != QStringLiteral("capture"))
    {
        return invalidArgument(QStringLiteral("operation"), QStringLiteral("Unexpected KWin bridge operation."));
    }

    if (response.value(QStringLiteral("error")).isObject())
    {
        const QJsonObject remoteError = response.value(QStringLiteral("error")).toObject();
        const QString message = remoteError.value(QStringLiteral("message")).toString(QStringLiteral("KWin capture failed."));
        const QString code = remoteError.value(QStringLiteral("code")).toString();
        return invalidArgument(QStringLiteral("error") + (code.isEmpty() ? QString{} : QStringLiteral(".") + code), message);
    }

    QJsonObject snapshot;
    if (!requiredObject(response, QStringLiteral("snapshot"), {}, snapshot, error))
    {
        return error;
    }

    int screenCount = 0;
    if (!requiredCount(snapshot, QStringLiteral("screen_count"), QStringLiteral("snapshot"), screenCount, error))
    {
        return error;
    }
    if (screenCount != 1)
    {
        return invalidArgument(QStringLiteral("snapshot.screen_count"),
                               QStringLiteral("This profile format currently supports exactly one monitor."));
    }

    QString activeRuntimeDesktopId;
    if (!requiredString(snapshot, QStringLiteral("active_desktop_id"), QStringLiteral("snapshot"),
                        activeRuntimeDesktopId, error))
    {
        return error;
    }

    const QJsonValue desktopsValue = snapshot.value(QStringLiteral("virtual_desktops"));
    if (!desktopsValue.isArray() || desktopsValue.toArray().isEmpty())
    {
        return structureError(QStringLiteral("snapshot.virtual_desktops"),
                              QStringLiteral("At least one virtual desktop is required."));
    }

    KWinCaptureSnapshot captured;
    Profile &profile = captured.profile;
    if (profileName.trimmed().isEmpty())
    {
        return invalidArgument(QStringLiteral("profile_name"), QStringLiteral("Profile name must not be empty."));
    }
    profile.id = QUuid::createUuid();
    profile.name = profileName;

    QHash<QString, QUuid> logicalDesktopIds;
    QSet<QString> runtimeDesktopIds;
    const QJsonArray desktops = desktopsValue.toArray();
    for (qsizetype index = 0; index < desktops.size(); ++index)
    {
        const QString path = QStringLiteral("snapshot.virtual_desktops[%1]").arg(index);
        if (!desktops.at(index).isObject())
        {
            return structureError(path, QStringLiteral("Virtual desktop entry must be an object."));
        }
        const QJsonObject desktopObject = desktops.at(index).toObject();
        QString runtimeId;
        QString name;
        if (!requiredString(desktopObject, QStringLiteral("id"), path, runtimeId, error) ||
            !requiredString(desktopObject, QStringLiteral("name"), path, name, error))
        {
            return error;
        }
        if (runtimeDesktopIds.contains(runtimeId))
        {
            return invalidArgument(path + QStringLiteral(".id"), QStringLiteral("Runtime desktop identifier is duplicated."));
        }
        runtimeDesktopIds.insert(runtimeId);

        const QUuid logicalId = QUuid::createUuid();
        logicalDesktopIds.insert(runtimeId, logicalId);
        profile.virtualDesktops.push_back({logicalId, name});
    }

    const auto activeDesktop = logicalDesktopIds.constFind(activeRuntimeDesktopId);
    if (activeDesktop == logicalDesktopIds.cend())
    {
        return invalidArgument(QStringLiteral("snapshot.active_desktop_id"),
                               QStringLiteral("Active desktop does not reference a captured virtual desktop."));
    }
    profile.activeDesktopId = activeDesktop.value();

    int excludedWindowCount = 0;
    int geometryOmittedCount = 0;
    int stateGeometryUnavailableCount = 0;
    if (!requiredCount(snapshot, QStringLiteral("excluded_window_count"), QStringLiteral("snapshot"),
                       excludedWindowCount, error) ||
        !requiredCount(snapshot, QStringLiteral("geometry_omitted_count"), QStringLiteral("snapshot"),
                       geometryOmittedCount, error) ||
        !requiredCount(snapshot, QStringLiteral("state_geometry_unavailable_count"), QStringLiteral("snapshot"),
                       stateGeometryUnavailableCount, error))
    {
        return error;
    }

    const QJsonValue windowsValue = snapshot.value(QStringLiteral("windows"));
    if (!windowsValue.isArray())
    {
        return structureError(QStringLiteral("snapshot.windows"), QStringLiteral("Required property must be an array."));
    }

    QHash<QString, qsizetype> applicationIndexes;
    const QJsonArray windows = windowsValue.toArray();
    for (qsizetype index = 0; index < windows.size(); ++index)
    {
        const QString path = QStringLiteral("snapshot.windows[%1]").arg(index);
        if (!windows.at(index).isObject())
        {
            return structureError(path, QStringLiteral("Window entry must be an object."));
        }
        const QJsonObject windowObject = windows.at(index).toObject();

        QJsonObject identityObject;
        if (!requiredObject(windowObject, QStringLiteral("application_identity"), path, identityObject, error))
        {
            return error;
        }
        QString identityKind;
        QString identityValue;
        if (!requiredString(identityObject, QStringLiteral("kind"), path + QStringLiteral(".application_identity"),
                            identityKind, error) ||
            !requiredString(identityObject, QStringLiteral("value"), path + QStringLiteral(".application_identity"),
                            identityValue, error))
        {
            return error;
        }
        const auto parsedIdentityKind = applicationIdentityKindFromString(identityKind);
        if (!parsedIdentityKind.has_value())
        {
            return structureError(path + QStringLiteral(".application_identity.kind"),
                                  QStringLiteral("Application identity kind is not supported."));
        }

        const QString applicationKey = identityKind + QChar(0x1f) + identityValue;
        qsizetype applicationIndex = -1;
        const auto existingApplication = applicationIndexes.constFind(applicationKey);
        if (existingApplication == applicationIndexes.cend())
        {
            Application application;
            application.id = QUuid::createUuid();
            profile.applications.push_back(application);
            applicationIndex = profile.applications.size() - 1;
            applicationIndexes.insert(applicationKey, applicationIndex);
        }
        else
        {
            applicationIndex = existingApplication.value();
        }

        Window window;
        window.id = QUuid::createUuid();
        window.match.applicationIdentity = {*parsedIdentityKind, identityValue};
        if (windowObject.contains(QStringLiteral("role")))
        {
            if (!windowObject.value(QStringLiteral("role")).isString())
            {
                return structureError(path + QStringLiteral(".role"), QStringLiteral("Window role must be a string when provided."));
            }
            const QString role = windowObject.value(QStringLiteral("role")).toString();
            if (!role.trimmed().isEmpty())
            {
                window.match.role = role;
            }
        }

        bool allDesktops = false;
        if (!requiredBoolean(windowObject, QStringLiteral("all_desktops"), path, allDesktops, error))
        {
            return error;
        }
        if (allDesktops)
        {
            if (windowObject.contains(QStringLiteral("desktop_ids")))
            {
                return structureError(path + QStringLiteral(".desktop_ids"),
                                      QStringLiteral("All-desktop membership must omit desktop_ids."));
            }
            window.desktopMembership.mode = DesktopMembershipMode::All;
        }
        else
        {
            const QJsonValue desktopIdsValue = windowObject.value(QStringLiteral("desktop_ids"));
            if (!desktopIdsValue.isArray() || desktopIdsValue.toArray().isEmpty())
            {
                return structureError(path + QStringLiteral(".desktop_ids"),
                                      QStringLiteral("Specific desktop membership requires at least one desktop."));
            }
            window.desktopMembership.mode = DesktopMembershipMode::Specific;
            const QJsonArray desktopIds = desktopIdsValue.toArray();
            QSet<QUuid> uniqueMemberships;
            for (qsizetype desktopIndex = 0; desktopIndex < desktopIds.size(); ++desktopIndex)
            {
                if (!desktopIds.at(desktopIndex).isString())
                {
                    return structureError(path + QStringLiteral(".desktop_ids[%1]").arg(desktopIndex),
                                          QStringLiteral("Runtime desktop identifier must be a string."));
                }
                const QString runtimeId = desktopIds.at(desktopIndex).toString();
                const auto logicalId = logicalDesktopIds.constFind(runtimeId);
                if (logicalId == logicalDesktopIds.cend())
                {
                    return invalidArgument(path + QStringLiteral(".desktop_ids[%1]").arg(desktopIndex),
                                           QStringLiteral("Window references an unknown runtime desktop."));
                }
                if (uniqueMemberships.contains(logicalId.value()))
                {
                    return invalidArgument(path + QStringLiteral(".desktop_ids[%1]").arg(desktopIndex),
                                           QStringLiteral("Window desktop membership contains a duplicate."));
                }
                uniqueMemberships.insert(logicalId.value());
                window.desktopMembership.desktopIds.push_back(logicalId.value());
            }
        }

        QJsonObject stateObject;
        if (!requiredObject(windowObject, QStringLiteral("state"), path, stateObject, error) ||
            !requiredBoolean(stateObject, QStringLiteral("minimized"), path + QStringLiteral(".state"),
                             window.state.minimized, error) ||
            !requiredBoolean(stateObject, QStringLiteral("fullscreen"), path + QStringLiteral(".state"),
                             window.state.fullscreen, error))
        {
            return error;
        }
        QString maximizeMode;
        if (!requiredString(stateObject, QStringLiteral("maximize_mode"), path + QStringLiteral(".state"),
                            maximizeMode, error))
        {
            return error;
        }
        const auto parsedMaximizeMode = windowMaximizeModeFromString(maximizeMode);
        if (!parsedMaximizeMode.has_value())
        {
            return structureError(path + QStringLiteral(".state.maximize_mode"),
                                  QStringLiteral("Maximize mode must be none, vertical, horizontal, or full."));
        }
        window.state.maximizeMode = *parsedMaximizeMode;

        if (!parseGeometry(windowObject, path, window.geometry, error))
        {
            return error;
        }

        profile.applications[applicationIndex].windows.push_back(std::move(window));
    }

    const QVector<ProfileIssue> issues = ProfileValidator::validate(profile);
    if (!issues.isEmpty())
    {
        return ProfileError{ProfileErrorCode::ValidationFailed,
                            QStringLiteral("Captured KWin snapshot failed profile validation."), {}, issues};
    }

    captured.excludedWindowCount = excludedWindowCount;
    captured.geometryOmittedCount = geometryOmittedCount;
    captured.stateGeometryUnavailableCount = stateGeometryUnavailableCount;
    return captured;
}

} // namespace kwm::kwin
