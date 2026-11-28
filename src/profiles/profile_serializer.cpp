#include "profiles/profile_serializer.h"

#include "profiles/profile_migrator.h"
#include "profiles/profile_validator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <utility>

namespace kwm::profiles
{
namespace
{

ProfileError structureError(const QString &path, const QString &message)
{
    return {ProfileErrorCode::InvalidStructure, message, path, {}};
}

bool fail(ProfileError &error, const QString &path, const QString &message)
{
    error = structureError(path, message);
    return false;
}

QString propertyPath(const QString &parentPath, const QString &key)
{
    return parentPath.isEmpty() ? key : parentPath + QLatin1Char('.') + key;
}

bool checkOnlyKeys(const QJsonObject &object, const QStringList &allowedKeys, const QString &path, ProfileError &error)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it)
    {
        if (!allowedKeys.contains(it.key()))
        {
            return fail(error, propertyPath(path, it.key()),
                        QStringLiteral("Unknown property is not allowed in the current schema version."));
        }
    }
    return true;
}

bool requiredObject(const QJsonObject &parent, const QString &key, const QString &path, QJsonObject &result,
                    ProfileError &error)
{
    const QJsonValue value = parent.value(key);
    if (!value.isObject())
    {
        return fail(error, propertyPath(path, key), QStringLiteral("Required property must be an object."));
    }
    result = value.toObject();
    return true;
}

bool requiredArray(const QJsonObject &parent, const QString &key, const QString &path, QJsonArray &result,
                   ProfileError &error)
{
    const QJsonValue value = parent.value(key);
    if (!value.isArray())
    {
        return fail(error, propertyPath(path, key), QStringLiteral("Required property must be an array."));
    }
    result = value.toArray();
    return true;
}

bool requiredString(const QJsonObject &parent, const QString &key, const QString &path, QString &result,
                    ProfileError &error)
{
    const QJsonValue value = parent.value(key);
    if (!value.isString() || value.toString().trimmed().isEmpty())
    {
        return fail(error, propertyPath(path, key), QStringLiteral("Required property must be a non-empty string."));
    }
    result = value.toString();
    return true;
}

bool parseUuid(const QJsonValue &value, const QString &path, QUuid &result, ProfileError &error)
{
    if (!value.isString())
    {
        return fail(error, path, QStringLiteral("Identifier must be a canonical UUID string."));
    }

    result = QUuid::fromString(value.toString());
    if (result.isNull() || result.toString(QUuid::WithoutBraces) != value.toString())
    {
        return fail(error, path, QStringLiteral("Identifier must be a canonical lowercase UUID string."));
    }
    return true;
}

bool requiredUuid(const QJsonObject &parent, const QString &key, const QString &path, QUuid &result,
                  ProfileError &error)
{
    return parseUuid(parent.value(key), propertyPath(path, key), result, error);
}

bool requiredBoolean(const QJsonObject &parent, const QString &key, const QString &path, bool &result,
                     ProfileError &error)
{
    const QJsonValue value = parent.value(key);
    if (!value.isBool())
    {
        return fail(error, propertyPath(path, key), QStringLiteral("Required property must be a boolean."));
    }
    result = value.toBool();
    return true;
}

bool requiredNumber(const QJsonObject &parent, const QString &key, const QString &path, double &result,
                    ProfileError &error)
{
    const QJsonValue value = parent.value(key);
    if (!value.isDouble())
    {
        return fail(error, propertyPath(path, key), QStringLiteral("Required property must be a number."));
    }
    result = value.toDouble();
    return true;
}

bool parseDesktop(const QJsonValue &value, qsizetype index, VirtualDesktop &desktop, ProfileError &error)
{
    const QString path = QStringLiteral("virtual_desktops[%1]").arg(index);
    if (!value.isObject())
    {
        return fail(error, path, QStringLiteral("Desktop entry must be an object."));
    }

    const QJsonObject object = value.toObject();
    if (!checkOnlyKeys(object, {QStringLiteral("id"), QStringLiteral("name")}, path, error))
    {
        return false;
    }

    return requiredUuid(object, QStringLiteral("id"), path, desktop.id, error) &&
           requiredString(object, QStringLiteral("name"), path, desktop.name, error);
}

bool parseLaunch(const QJsonValue &value, const QString &path, LaunchSpecification &launch, ProfileError &error)
{
    if (!value.isObject())
    {
        return fail(error, path, QStringLiteral("Launch specification must be an object."));
    }

    const QJsonObject object = value.toObject();
    if (!checkOnlyKeys(
            object,
            {QStringLiteral("desktop_entry_id"), QStringLiteral("arguments"), QStringLiteral("working_directory")},
            path, error))
    {
        return false;
    }

    if (!requiredString(object, QStringLiteral("desktop_entry_id"), path, launch.desktopEntryId, error))
    {
        return false;
    }

    QJsonArray arguments;
    if (!requiredArray(object, QStringLiteral("arguments"), path, arguments, error))
    {
        return false;
    }
    for (qsizetype index = 0; index < arguments.size(); ++index)
    {
        const QJsonValue argument = arguments.at(index);
        if (!argument.isString())
        {
            return fail(error, path + QStringLiteral(".arguments[%1]").arg(index),
                        QStringLiteral("Application argument must be a string."));
        }
        launch.arguments.push_back(argument.toString());
    }

    if (object.contains(QStringLiteral("working_directory")))
    {
        QString workingDirectory;
        if (!requiredString(object, QStringLiteral("working_directory"), path, workingDirectory, error))
        {
            return false;
        }
        launch.workingDirectory = std::move(workingDirectory);
    }

    return true;
}

bool parseWindow(const QJsonValue &value, const QString &path, Window &window, ProfileError &error)
{
    if (!value.isObject())
    {
        return fail(error, path, QStringLiteral("Window entry must be an object."));
    }

    const QJsonObject object = value.toObject();
    if (!checkOnlyKeys(object,
                       {QStringLiteral("id"), QStringLiteral("match"), QStringLiteral("desktop_membership"),
                        QStringLiteral("geometry"), QStringLiteral("state")},
                       path, error))
    {
        return false;
    }

    if (!requiredUuid(object, QStringLiteral("id"), path, window.id, error))
    {
        return false;
    }

    QJsonObject matchObject;
    if (!requiredObject(object, QStringLiteral("match"), path, matchObject, error) ||
        !checkOnlyKeys(matchObject, {QStringLiteral("application_identity"), QStringLiteral("role")},
                       path + QStringLiteral(".match"), error))
    {
        return false;
    }

    QJsonObject identityObject;
    if (!requiredObject(matchObject, QStringLiteral("application_identity"), path + QStringLiteral(".match"),
                        identityObject, error) ||
        !checkOnlyKeys(identityObject, {QStringLiteral("kind"), QStringLiteral("value")},
                       path + QStringLiteral(".match.application_identity"), error))
    {
        return false;
    }

    QString identityKind;
    if (!requiredString(identityObject, QStringLiteral("kind"), path + QStringLiteral(".match.application_identity"),
                        identityKind, error) ||
        !requiredString(identityObject, QStringLiteral("value"), path + QStringLiteral(".match.application_identity"),
                        window.match.applicationIdentity.value, error))
    {
        return false;
    }

    const auto parsedKind = applicationIdentityKindFromString(identityKind);
    if (!parsedKind.has_value())
    {
        return fail(error, path + QStringLiteral(".match.application_identity.kind"),
                    QStringLiteral("Application identity kind must be desktop_file_name or resource_class."));
    }
    window.match.applicationIdentity.kind = *parsedKind;

    if (matchObject.contains(QStringLiteral("role")))
    {
        QString role;
        if (!requiredString(matchObject, QStringLiteral("role"), path + QStringLiteral(".match"), role, error))
        {
            return false;
        }
        window.match.role = std::move(role);
    }

    QJsonObject membershipObject;
    const QString membershipPath = path + QStringLiteral(".desktop_membership");
    if (!requiredObject(object, QStringLiteral("desktop_membership"), path, membershipObject, error) ||
        !checkOnlyKeys(membershipObject, {QStringLiteral("mode"), QStringLiteral("desktop_ids")}, membershipPath,
                       error))
    {
        return false;
    }

    QString membershipMode;
    if (!requiredString(membershipObject, QStringLiteral("mode"), membershipPath, membershipMode, error))
    {
        return false;
    }
    const auto parsedMode = desktopMembershipModeFromString(membershipMode);
    if (!parsedMode.has_value())
    {
        return fail(error, membershipPath + QStringLiteral(".mode"),
                    QStringLiteral("Desktop membership mode must be specific or all."));
    }
    window.desktopMembership.mode = *parsedMode;

    if (*parsedMode == DesktopMembershipMode::Specific)
    {
        QJsonArray desktopIds;
        if (!requiredArray(membershipObject, QStringLiteral("desktop_ids"), membershipPath, desktopIds, error))
        {
            return false;
        }
        for (qsizetype index = 0; index < desktopIds.size(); ++index)
        {
            QUuid desktopId;
            if (!parseUuid(desktopIds.at(index), membershipPath + QStringLiteral(".desktop_ids[%1]").arg(index),
                           desktopId, error))
            {
                return false;
            }
            window.desktopMembership.desktopIds.push_back(desktopId);
        }
    }
    else if (membershipObject.contains(QStringLiteral("desktop_ids")))
    {
        return fail(error, membershipPath + QStringLiteral(".desktop_ids"),
                    QStringLiteral("All-desktop membership must omit desktop_ids."));
    }

    if (object.contains(QStringLiteral("geometry")))
    {
        QJsonObject geometryObject;
        const QString geometryPath = path + QStringLiteral(".geometry");
        if (!requiredObject(object, QStringLiteral("geometry"), path, geometryObject, error) ||
            !checkOnlyKeys(
                geometryObject,
                {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height")},
                geometryPath, error) ||
            !requiredNumber(geometryObject, QStringLiteral("x"), geometryPath, window.geometry.emplace().x, error) ||
            !requiredNumber(geometryObject, QStringLiteral("y"), geometryPath, window.geometry->y, error) ||
            !requiredNumber(geometryObject, QStringLiteral("width"), geometryPath, window.geometry->width, error) ||
            !requiredNumber(geometryObject, QStringLiteral("height"), geometryPath, window.geometry->height, error))
        {
            return false;
        }
    }

    QJsonObject stateObject;
    const QString statePath = path + QStringLiteral(".state");
    if (!requiredObject(object, QStringLiteral("state"), path, stateObject, error) ||
        !checkOnlyKeys(stateObject,
                       {QStringLiteral("minimized"), QStringLiteral("maximize_mode"), QStringLiteral("fullscreen")},
                       statePath, error) ||
        !requiredBoolean(stateObject, QStringLiteral("minimized"), statePath, window.state.minimized, error) ||
        !requiredBoolean(stateObject, QStringLiteral("fullscreen"), statePath, window.state.fullscreen, error))
    {
        return false;
    }

    QString maximizeMode;
    if (!requiredString(stateObject, QStringLiteral("maximize_mode"), statePath, maximizeMode, error))
    {
        return false;
    }
    const auto parsedMaximizeMode = windowMaximizeModeFromString(maximizeMode);
    if (!parsedMaximizeMode.has_value())
    {
        return fail(error, statePath + QStringLiteral(".maximize_mode"),
                    QStringLiteral("Maximize mode must be none, vertical, horizontal, or full."));
    }
    window.state.maximizeMode = *parsedMaximizeMode;

    return true;
}

bool parseApplication(const QJsonValue &value, qsizetype index, Application &application, ProfileError &error)
{
    const QString path = QStringLiteral("applications[%1]").arg(index);
    if (!value.isObject())
    {
        return fail(error, path, QStringLiteral("Application entry must be an object."));
    }

    const QJsonObject object = value.toObject();
    if (!checkOnlyKeys(object, {QStringLiteral("id"), QStringLiteral("launch"), QStringLiteral("windows")}, path,
                       error) ||
        !requiredUuid(object, QStringLiteral("id"), path, application.id, error))
    {
        return false;
    }

    if (object.contains(QStringLiteral("launch")))
    {
        LaunchSpecification launch;
        if (!parseLaunch(object.value(QStringLiteral("launch")), path + QStringLiteral(".launch"), launch, error))
        {
            return false;
        }
        application.launch = std::move(launch);
    }

    QJsonArray windows;
    if (!requiredArray(object, QStringLiteral("windows"), path, windows, error))
    {
        return false;
    }
    for (qsizetype windowIndex = 0; windowIndex < windows.size(); ++windowIndex)
    {
        Window window;
        if (!parseWindow(windows.at(windowIndex), path + QStringLiteral(".windows[%1]").arg(windowIndex), window,
                         error))
        {
            return false;
        }
        application.windows.push_back(std::move(window));
    }

    return true;
}

bool parseProfileObject(const QJsonObject &document, Profile &profile, ProfileError &error)
{
    if (!checkOnlyKeys(document,
                       {QStringLiteral("schema_version"), QStringLiteral("profile"),
                        QStringLiteral("active_desktop_id"), QStringLiteral("virtual_desktops"),
                        QStringLiteral("applications")},
                       QString{}, error))
    {
        return false;
    }

    QJsonObject profileObject;
    if (!requiredObject(document, QStringLiteral("profile"), QString{}, profileObject, error) ||
        !checkOnlyKeys(profileObject, {QStringLiteral("id"), QStringLiteral("name")}, QStringLiteral("profile"),
                       error) ||
        !requiredUuid(profileObject, QStringLiteral("id"), QStringLiteral("profile"), profile.id, error) ||
        !requiredString(profileObject, QStringLiteral("name"), QStringLiteral("profile"), profile.name, error))
    {
        return false;
    }

    if (document.contains(QStringLiteral("active_desktop_id")))
    {
        QUuid activeDesktopId;
        if (!parseUuid(document.value(QStringLiteral("active_desktop_id")), QStringLiteral("active_desktop_id"),
                       activeDesktopId, error))
        {
            return false;
        }
        profile.activeDesktopId = activeDesktopId;
    }

    QJsonArray desktops;
    if (!requiredArray(document, QStringLiteral("virtual_desktops"), QString{}, desktops, error))
    {
        return false;
    }
    for (qsizetype index = 0; index < desktops.size(); ++index)
    {
        VirtualDesktop desktop;
        if (!parseDesktop(desktops.at(index), index, desktop, error))
        {
            return false;
        }
        profile.virtualDesktops.push_back(std::move(desktop));
    }

    QJsonArray applications;
    if (!requiredArray(document, QStringLiteral("applications"), QString{}, applications, error))
    {
        return false;
    }
    for (qsizetype index = 0; index < applications.size(); ++index)
    {
        Application application;
        if (!parseApplication(applications.at(index), index, application, error))
        {
            return false;
        }
        profile.applications.push_back(std::move(application));
    }

    return true;
}

QJsonObject geometryToJson(const Geometry &geometry)
{
    return {{QStringLiteral("x"), geometry.x},
            {QStringLiteral("y"), geometry.y},
            {QStringLiteral("width"), geometry.width},
            {QStringLiteral("height"), geometry.height}};
}

QJsonObject windowToJson(const Window &window)
{
    QJsonObject identity{
        {QStringLiteral("kind"), applicationIdentityKindToString(window.match.applicationIdentity.kind)},
        {QStringLiteral("value"), window.match.applicationIdentity.value}};

    QJsonObject match{{QStringLiteral("application_identity"), identity}};
    if (window.match.role.has_value())
    {
        match.insert(QStringLiteral("role"), *window.match.role);
    }

    QJsonObject membership{{QStringLiteral("mode"), desktopMembershipModeToString(window.desktopMembership.mode)}};
    if (window.desktopMembership.mode == DesktopMembershipMode::Specific)
    {
        QJsonArray desktopIds;
        for (const QUuid &desktopId : window.desktopMembership.desktopIds)
        {
            desktopIds.push_back(desktopId.toString(QUuid::WithoutBraces));
        }
        membership.insert(QStringLiteral("desktop_ids"), desktopIds);
    }

    QJsonObject state{{QStringLiteral("minimized"), window.state.minimized},
                      {QStringLiteral("maximize_mode"), windowMaximizeModeToString(window.state.maximizeMode)},
                      {QStringLiteral("fullscreen"), window.state.fullscreen}};

    QJsonObject object{{QStringLiteral("id"), window.id.toString(QUuid::WithoutBraces)},
                       {QStringLiteral("match"), match},
                       {QStringLiteral("desktop_membership"), membership},
                       {QStringLiteral("state"), state}};
    if (window.geometry.has_value())
    {
        object.insert(QStringLiteral("geometry"), geometryToJson(*window.geometry));
    }
    return object;
}

QJsonObject applicationToJson(const Application &application)
{
    QJsonObject object{{QStringLiteral("id"), application.id.toString(QUuid::WithoutBraces)}};

    if (application.launch.has_value())
    {
        const auto &launch = *application.launch;
        QJsonArray arguments;
        for (const QString &argument : launch.arguments)
        {
            arguments.push_back(argument);
        }

        QJsonObject launchObject{{QStringLiteral("desktop_entry_id"), launch.desktopEntryId},
                                 {QStringLiteral("arguments"), arguments}};
        if (launch.workingDirectory.has_value())
        {
            launchObject.insert(QStringLiteral("working_directory"), *launch.workingDirectory);
        }
        object.insert(QStringLiteral("launch"), launchObject);
    }

    QJsonArray windows;
    for (const Window &window : application.windows)
    {
        windows.push_back(windowToJson(window));
    }
    object.insert(QStringLiteral("windows"), windows);
    return object;
}

} // namespace

ProfileResult<QByteArray> ProfileSerializer::serialize(const Profile &profile)
{
    const QVector<ProfileIssue> issues = ProfileValidator::validate(profile);
    if (!issues.isEmpty())
    {
        return ProfileError{
            ProfileErrorCode::ValidationFailed, QStringLiteral("Profile failed semantic validation."), {}, issues};
    }

    QJsonObject profileObject{{QStringLiteral("id"), profile.id.toString(QUuid::WithoutBraces)},
                              {QStringLiteral("name"), profile.name}};

    QJsonArray desktops;
    for (const VirtualDesktop &desktop : profile.virtualDesktops)
    {
        desktops.push_back(QJsonObject{{QStringLiteral("id"), desktop.id.toString(QUuid::WithoutBraces)},
                                       {QStringLiteral("name"), desktop.name}});
    }

    QJsonArray applications;
    for (const Application &application : profile.applications)
    {
        applications.push_back(applicationToJson(application));
    }

    QJsonObject document{{QStringLiteral("schema_version"), CurrentSchemaVersion},
                         {QStringLiteral("profile"), profileObject},
                         {QStringLiteral("virtual_desktops"), desktops},
                         {QStringLiteral("applications"), applications}};
    if (profile.activeDesktopId.has_value())
    {
        document.insert(QStringLiteral("active_desktop_id"), profile.activeDesktopId->toString(QUuid::WithoutBraces));
    }

    return QJsonDocument(document).toJson(QJsonDocument::Indented);
}

ProfileResult<Profile> ProfileSerializer::deserialize(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        return ProfileError{ProfileErrorCode::InvalidJson, parseError.errorString(), {}, {}};
    }
    if (!document.isObject())
    {
        return ProfileError{
            ProfileErrorCode::InvalidStructure, QStringLiteral("Profile document root must be a JSON object."), {}, {}};
    }

    auto migrationResult = ProfileMigrator::migrateToCurrent(document.object());
    if (const auto *failure = std::get_if<ProfileError>(&migrationResult))
    {
        return *failure;
    }

    const QJsonObject migrated = std::get<QJsonObject>(std::move(migrationResult));
    Profile profile;
    ProfileError error;
    if (!parseProfileObject(migrated, profile, error))
    {
        return error;
    }

    const QVector<ProfileIssue> issues = ProfileValidator::validate(profile);
    if (!issues.isEmpty())
    {
        return ProfileError{ProfileErrorCode::ValidationFailed,
                            QStringLiteral("Deserialized profile failed semantic validation."),
                            {},
                            issues};
    }

    return profile;
}

} // namespace kwm::profiles
