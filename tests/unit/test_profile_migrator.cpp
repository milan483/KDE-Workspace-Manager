#include "profiles/profile_migrator.h"

#include <QJsonObject>
#include <QtTest/QtTest>

using namespace kwm::profiles;

class ProfileMigratorTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void acceptsCurrentSchemaWithoutChangingIt();
    void rejectsMissingSchemaVersion();
    void rejectsNonNumericSchemaVersion();
    void rejectsFractionalSchemaVersion();
    void rejectsLegacyVersionWithoutInventingMigration();
    void rejectsFutureSchemaVersion();
};

void ProfileMigratorTest::acceptsCurrentSchemaWithoutChangingIt()
{
    const QJsonObject document{
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("profile"), QJsonObject{{QStringLiteral("name"), QStringLiteral("Example")}}}};

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<QJsonObject>(result));
    QCOMPARE(std::get<QJsonObject>(result), document);
}

void ProfileMigratorTest::rejectsMissingSchemaVersion()
{
    const auto result = ProfileMigrator::migrateToCurrent({});
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::MissingSchemaVersion);
}

void ProfileMigratorTest::rejectsNonNumericSchemaVersion()
{
    const QJsonObject document{{QStringLiteral("schema_version"), QStringLiteral("1")}};

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidSchemaVersion);
}

void ProfileMigratorTest::rejectsFractionalSchemaVersion()
{
    const QJsonObject document{{QStringLiteral("schema_version"), 1.5}};

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidSchemaVersion);
}

void ProfileMigratorTest::rejectsLegacyVersionWithoutInventingMigration()
{
    const QJsonObject document{{QStringLiteral("schema_version"), 0}};

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::UnsupportedSchemaVersion);
}

void ProfileMigratorTest::rejectsFutureSchemaVersion()
{
    const QJsonObject document{{QStringLiteral("schema_version"), 2}};

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::UnsupportedSchemaVersion);
}

QTEST_APPLESS_MAIN(ProfileMigratorTest)

#include "test_profile_migrator.moc"
