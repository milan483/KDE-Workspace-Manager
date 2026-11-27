#include "profiles/profile_migrator.h"

#include <QHash>
#include <QJsonValue>

#include <cmath>
#include <functional>
#include <limits>

namespace kwm::profiles
{
namespace
{

using MigrationFunction = std::function<ProfileResult<QJsonObject>(const QJsonObject &)>;

const QHash<int, MigrationFunction> &migrationRegistry()
{
    // V1 is the first persisted schema. There is deliberately no synthetic v0 migration.
    // When V2 is introduced, register an explicit 1 -> 2 transformation here.
    static const QHash<int, MigrationFunction> migrations;
    return migrations;
}

ProfileError error(ProfileErrorCode code, const QString &message, const QString &path = {})
{
    return {code, message, path, {}};
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
