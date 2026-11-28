#include "profiles/profile_migrator.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>

#include <cmath>
#include <functional>
#include <limits>

namespace kwm::profiles
{
namespace
{

using MigrationFunction = std::function<ProfileResult<QJsonObject>(const QJsonObject &)>;

ProfileError error(ProfileErrorCode code, const QString &message, const QString &path = {})
{
    return {code, message, path, {}};
}

ProfileResult<QJsonObject> migrateV1ToV2(const QJsonObject &document)
{
    QJsonValue applicationsValue = document.value(QStringLiteral("applications"));
    if (!applicationsValue.isArray())
    {
        return error(ProfileErrorCode::InvalidStructure,
                     QStringLiteral("Required property must be an array."), QStringLiteral("applications"));
    }

    QJsonArray applications = applicationsValue.toArray();
    for (qsizetype applicationIndex = 0; applicationIndex < applications.size(); ++applicationIndex)
    {
        const QString applicationPath = QStringLiteral("applications[%1]").arg(applicationIndex);
        QJsonValue applicationValue = applications.at(applicationIndex);
        if (!applicationValue.isObject())
        {
            return error(ProfileErrorCode::InvalidStructure,
                         QStringLiteral("Application entry must be an object."), applicationPath);
        }

        QJsonObject application = applicationValue.toObject();
        const QJsonValue windowsValue = application.value(QStringLiteral("windows"));
        if (!windowsValue.isArray())
        {
            return error(ProfileErrorCode::InvalidStructure,
                         QStringLiteral("Required property must be an array."),
                         applicationPath + QStringLiteral(".windows"));
        }

        QJsonArray windows = windowsValue.toArray();
        for (qsizetype windowIndex = 0; windowIndex < windows.size(); ++windowIndex)
        {
            const QString windowPath = applicationPath + QStringLiteral(".windows[%1]").arg(windowIndex);
            const QJsonValue windowValue = windows.at(windowIndex);
            if (!windowValue.isObject())
            {
                return error(ProfileErrorCode::InvalidStructure,
                             QStringLiteral("Window entry must be an object."), windowPath);
            }

            QJsonObject window = windowValue.toObject();
            const QString statePath = windowPath + QStringLiteral(".state");
            const QJsonValue stateValue = window.value(QStringLiteral("state"));
            if (!stateValue.isObject())
            {
                return error(ProfileErrorCode::InvalidStructure,
                             QStringLiteral("Required property must be an object."), statePath);
            }

            QJsonObject state = stateValue.toObject();
            if (state.contains(QStringLiteral("maximize_mode")))
            {
                return error(ProfileErrorCode::InvalidStructure,
                             QStringLiteral("Schema version 1 must not contain maximize_mode."),
                             statePath + QStringLiteral(".maximize_mode"));
            }

            const QJsonValue maximizedValue = state.value(QStringLiteral("maximized"));
            if (!maximizedValue.isBool())
            {
                return error(ProfileErrorCode::InvalidStructure,
                             QStringLiteral("Required property must be a boolean."),
                             statePath + QStringLiteral(".maximized"));
            }

            state.remove(QStringLiteral("maximized"));
            state.insert(QStringLiteral("maximize_mode"),
                         maximizedValue.toBool() ? QStringLiteral("full") : QStringLiteral("none"));
            window.insert(QStringLiteral("state"), state);
            windows[windowIndex] = window;
        }

        application.insert(QStringLiteral("windows"), windows);
        applications[applicationIndex] = application;
    }

    QJsonObject migrated = document;
    migrated.insert(QStringLiteral("schema_version"), 2);
    migrated.insert(QStringLiteral("applications"), applications);
    return migrated;
}

const QHash<int, MigrationFunction> &migrationRegistry()
{
    static const QHash<int, MigrationFunction> migrations{{1, migrateV1ToV2}};
    return migrations;
}

ProfileResult<int> readSchemaVersion(const QJsonObject &document)
{
    if (!document.contains(QStringLiteral("schema_version")))
    {
        return error(ProfileErrorCode::MissingSchemaVersion,
                     QStringLiteral("Profile document is missing schema_version."), QStringLiteral("schema_version"));
    }

    const QJsonValue value = document.value(QStringLiteral("schema_version"));
    if (!value.isDouble())
    {
        return error(ProfileErrorCode::InvalidSchemaVersion, QStringLiteral("schema_version must be an integer."),
                     QStringLiteral("schema_version"));
    }

    const double numericVersion = value.toDouble();
    if (!std::isfinite(numericVersion) || std::floor(numericVersion) != numericVersion ||
        numericVersion < static_cast<double>(std::numeric_limits<int>::min()) ||
        numericVersion > static_cast<double>(std::numeric_limits<int>::max()))
    {
        return error(ProfileErrorCode::InvalidSchemaVersion,
                     QStringLiteral("schema_version must be a finite integer in the supported numeric range."),
                     QStringLiteral("schema_version"));
    }

    return static_cast<int>(numericVersion);
}

} // namespace

ProfileResult<QJsonObject> ProfileMigrator::migrateToCurrent(const QJsonObject &document)
{
    auto versionResult = readSchemaVersion(document);
    if (const auto *failure = std::get_if<ProfileError>(&versionResult))
    {
        return *failure;
    }

    int version = std::get<int>(versionResult);
    if (version < 1 || version > CurrentSchemaVersion)
    {
        return error(
            ProfileErrorCode::UnsupportedSchemaVersion,
            QStringLiteral("Profile schema version %1 is not supported; this build supports versions 1 through %2.")
                .arg(version)
                .arg(CurrentSchemaVersion),
            QStringLiteral("schema_version"));
    }

    QJsonObject migrated = document;
    while (version < CurrentSchemaVersion)
    {
        const auto migration = migrationRegistry().constFind(version);
        if (migration == migrationRegistry().cend())
        {
            return error(ProfileErrorCode::UnsupportedSchemaVersion,
                         QStringLiteral("No migration is registered from profile schema version %1.").arg(version),
                         QStringLiteral("schema_version"));
        }

        auto nextResult = migration.value()(migrated);
        if (const auto *failure = std::get_if<ProfileError>(&nextResult))
        {
            return *failure;
        }

        QJsonObject next = std::get<QJsonObject>(std::move(nextResult));
        auto nextVersionResult = readSchemaVersion(next);
        if (const auto *failure = std::get_if<ProfileError>(&nextVersionResult))
        {
            return *failure;
        }

        const int nextVersion = std::get<int>(nextVersionResult);
        if (nextVersion != version + 1)
        {
            return error(ProfileErrorCode::InvalidSchemaVersion,
                         QStringLiteral("A migration must advance exactly one schema version."),
                         QStringLiteral("schema_version"));
        }

        migrated = std::move(next);
        version = nextVersion;
    }

    return migrated;
}

} // namespace kwm::profiles
