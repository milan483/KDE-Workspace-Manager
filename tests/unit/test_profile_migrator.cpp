#include "profiles/profile_migrator.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest/QtTest>

using namespace kwm::profiles;

class ProfileMigratorTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void acceptsCurrentSchemaWithoutChangingIt();
    void migratesV1MaximizeStateToV2Modes();
    void rejectsMalformedV1Applications();
    void rejectsMalformedV1WindowState();
    void rejectsNonBooleanV1MaximizedState();
    void rejectsV1StateThatAlreadyContainsV2Property();
    void rejectsMissingSchemaVersion();
    void rejectsNonNumericSchemaVersion();
    void rejectsFractionalSchemaVersion();
    void rejectsLegacyVersionWithoutInventingMigration();
    void rejectsFutureSchemaVersion();
};

namespace
{

QJsonObject v1Window(bool minimized, bool maximized, bool fullscreen)
{
    return {{QStringLiteral("state"),
             QJsonObject{{QStringLiteral("minimized"), minimized},
                         {QStringLiteral("maximized"), maximized},
                         {QStringLiteral("fullscreen"), fullscreen}}}};
}

QJsonObject v1Document(const QJsonArray &windows)
{
    return {{QStringLiteral("schema_version"), 1},
            {QStringLiteral("applications"),
             QJsonArray{QJsonObject{{QStringLiteral("windows"), windows}}}}};
}

} // namespace

void ProfileMigratorTest::acceptsCurrentSchemaWithoutChangingIt()
{
    const QJsonObject document{
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("profile"), QJsonObject{{QStringLiteral("name"), QStringLiteral("Example")}}}};

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<QJsonObject>(result));
    QCOMPARE(std::get<QJsonObject>(result), document);
}

void ProfileMigratorTest::migratesV1MaximizeStateToV2Modes()
{
    const QJsonObject document = v1Document(QJsonArray{
        v1Window(false, true, false),
        v1Window(true, false, false),
        v1Window(true, false, true),
    });

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<QJsonObject>(result));

    const QJsonObject migrated = std::get<QJsonObject>(result);
    QCOMPARE(migrated.value(QStringLiteral("schema_version")).toInt(), 2);
    const QJsonArray applications = migrated.value(QStringLiteral("applications")).toArray();
    const QJsonArray windows = applications.at(0).toObject().value(QStringLiteral("windows")).toArray();
    const QJsonObject firstState = windows.at(0).toObject().value(QStringLiteral("state")).toObject();
    const QJsonObject secondState = windows.at(1).toObject().value(QStringLiteral("state")).toObject();
    const QJsonObject thirdState = windows.at(2).toObject().value(QStringLiteral("state")).toObject();

    QCOMPARE(firstState.value(QStringLiteral("maximize_mode")).toString(), QStringLiteral("full"));
    QCOMPARE(secondState.value(QStringLiteral("maximize_mode")).toString(), QStringLiteral("none"));
    QCOMPARE(thirdState.value(QStringLiteral("maximize_mode")).toString(), QStringLiteral("none"));
    QVERIFY(firstState.value(QStringLiteral("minimized")).isBool());
    QVERIFY(secondState.value(QStringLiteral("minimized")).toBool());
    QVERIFY(thirdState.value(QStringLiteral("fullscreen")).toBool());
    QVERIFY(!firstState.contains(QStringLiteral("maximized")));
}

void ProfileMigratorTest::rejectsMalformedV1Applications()
{
    const QJsonObject document{{QStringLiteral("schema_version"), 1},
                               {QStringLiteral("applications"), QJsonArray{QStringLiteral("not-an-object")}}};

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
    QCOMPARE(std::get<ProfileError>(result).path, QStringLiteral("applications[0]"));
}

void ProfileMigratorTest::rejectsMalformedV1WindowState()
{
    const QJsonObject document = v1Document(QJsonArray{QJsonObject{{QStringLiteral("state"), QJsonArray{}}}});

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
    QCOMPARE(std::get<ProfileError>(result).path, QStringLiteral("applications[0].windows[0].state"));
}

void ProfileMigratorTest::rejectsNonBooleanV1MaximizedState()
{
    QJsonObject window = v1Window(false, true, false);
    QJsonObject state = window.value(QStringLiteral("state")).toObject();
    state.insert(QStringLiteral("maximized"), 1);
    window.insert(QStringLiteral("state"), state);

    const auto result = ProfileMigrator::migrateToCurrent(v1Document(QJsonArray{window}));
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
    QCOMPARE(std::get<ProfileError>(result).path,
             QStringLiteral("applications[0].windows[0].state.maximized"));
}

void ProfileMigratorTest::rejectsV1StateThatAlreadyContainsV2Property()
{
    QJsonObject window = v1Window(false, true, false);
    QJsonObject state = window.value(QStringLiteral("state")).toObject();
    state.insert(QStringLiteral("maximize_mode"), QStringLiteral("vertical"));
    window.insert(QStringLiteral("state"), state);

    const auto result = ProfileMigrator::migrateToCurrent(v1Document(QJsonArray{window}));
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
    QCOMPARE(std::get<ProfileError>(result).path,
             QStringLiteral("applications[0].windows[0].state.maximize_mode"));
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
    const QJsonObject document{{QStringLiteral("schema_version"), 3}};

    const auto result = ProfileMigrator::migrateToCurrent(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::UnsupportedSchemaVersion);
}

QTEST_APPLESS_MAIN(ProfileMigratorTest)

#include "test_profile_migrator.moc"
