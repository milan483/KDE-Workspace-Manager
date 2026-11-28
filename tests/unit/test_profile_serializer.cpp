#include "profile_test_utils.h"

#include "profiles/profile_serializer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

using namespace kwm::profiles;

class ProfileSerializerTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void serializesAndDeserializesProfile();
    void emitsExplicitStickyMembershipWithoutDesktopIds();
    void emitsExplicitMaximizeMode();
    void roundTripsPartialMaximizeModes();
    void deserializesV1ProfileThroughMigration();
    void keepsLaunchSpecificationSeparateFromWindowMatching();
    void rejectsMalformedJson();
    void rejectsNonObjectRoot();
    void rejectsUnknownProperties();
    void rejectsMissingRequiredProperties();
    void rejectsNonCanonicalIdentifiers();
    void rejectsInvalidWindowStateTypes();
    void rejectsUnknownMaximizeMode();
    void rejectsLegacyMaximizedPropertyInCurrentSchema();
    void rejectsSemanticallyInvalidGeometry();
    void rejectsInvalidDomainObjects();
};

namespace
{

QJsonObject validDocument()
{
    const auto result = ProfileSerializer::serialize(test::makeValidProfile());
    Q_ASSERT(std::holds_alternative<QByteArray>(result));
    return QJsonDocument::fromJson(std::get<QByteArray>(result)).object();
}

ProfileResult<Profile> deserialize(const QJsonObject &object)
{
    return ProfileSerializer::deserialize(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

} // namespace

void ProfileSerializerTest::serializesAndDeserializesProfile()
{
    const Profile expected = test::makeValidProfile();
    const auto serialized = ProfileSerializer::serialize(expected);
    QVERIFY(std::holds_alternative<QByteArray>(serialized));

    const auto deserialized = ProfileSerializer::deserialize(std::get<QByteArray>(serialized));
    QVERIFY(std::holds_alternative<Profile>(deserialized));
    QVERIFY(std::get<Profile>(deserialized) == expected);
}

void ProfileSerializerTest::emitsExplicitStickyMembershipWithoutDesktopIds()
{
    const QJsonObject document = validDocument();
    QCOMPARE(document.value(QStringLiteral("schema_version")).toInt(), 2);

    const QJsonArray applications = document.value(QStringLiteral("applications")).toArray();
    const QJsonArray windows = applications.at(0).toObject().value(QStringLiteral("windows")).toArray();
    const QJsonObject membership = windows.at(1).toObject().value(QStringLiteral("desktop_membership")).toObject();

    QCOMPARE(membership.value(QStringLiteral("mode")).toString(), QStringLiteral("all"));
    QVERIFY(!membership.contains(QStringLiteral("desktop_ids")));
}

void ProfileSerializerTest::emitsExplicitMaximizeMode()
{
    const QJsonObject document = validDocument();
    const QJsonArray applications = document.value(QStringLiteral("applications")).toArray();
    const QJsonArray windows = applications.at(0).toObject().value(QStringLiteral("windows")).toArray();
    const QJsonObject firstState = windows.at(0).toObject().value(QStringLiteral("state")).toObject();
    const QJsonObject secondState = windows.at(1).toObject().value(QStringLiteral("state")).toObject();

    QCOMPARE(firstState.value(QStringLiteral("maximize_mode")).toString(), QStringLiteral("full"));
    QCOMPARE(secondState.value(QStringLiteral("maximize_mode")).toString(), QStringLiteral("none"));
    QVERIFY(!firstState.contains(QStringLiteral("maximized")));
}

void ProfileSerializerTest::roundTripsPartialMaximizeModes()
{
    Profile expected = test::makeValidProfile();
    expected.applications[0].windows[0].state.maximizeMode = WindowMaximizeMode::Vertical;
    expected.applications[0].windows[1].state.maximizeMode = WindowMaximizeMode::Horizontal;
    expected.applications[0].windows[1].state.fullscreen = false;

    const auto serialized = ProfileSerializer::serialize(expected);
    QVERIFY(std::holds_alternative<QByteArray>(serialized));

    const auto deserialized = ProfileSerializer::deserialize(std::get<QByteArray>(serialized));
    QVERIFY(std::holds_alternative<Profile>(deserialized));
    const Profile actual = std::get<Profile>(deserialized);
    QCOMPARE(actual.applications[0].windows[0].state.maximizeMode, WindowMaximizeMode::Vertical);
    QCOMPARE(actual.applications[0].windows[1].state.maximizeMode, WindowMaximizeMode::Horizontal);
}

void ProfileSerializerTest::deserializesV1ProfileThroughMigration()
{
    QJsonObject document = validDocument();
    document.insert(QStringLiteral("schema_version"), 1);

    QJsonArray applications = document.value(QStringLiteral("applications")).toArray();
    QJsonObject application = applications[0].toObject();
    QJsonArray windows = application.value(QStringLiteral("windows")).toArray();
    for (qsizetype index = 0; index < windows.size(); ++index)
    {
        QJsonObject window = windows[index].toObject();
        QJsonObject state = window.value(QStringLiteral("state")).toObject();
        const bool maximized = state.value(QStringLiteral("maximize_mode")).toString() == QStringLiteral("full");
        state.remove(QStringLiteral("maximize_mode"));
        state.insert(QStringLiteral("maximized"), maximized);
        window.insert(QStringLiteral("state"), state);
        windows[index] = window;
    }
    application.insert(QStringLiteral("windows"), windows);
    applications[0] = application;
    document.insert(QStringLiteral("applications"), applications);

    const auto deserialized = deserialize(document);
    QVERIFY(std::holds_alternative<Profile>(deserialized));
    const Profile migrated = std::get<Profile>(deserialized);
    QCOMPARE(migrated.applications[0].windows[0].state.maximizeMode, WindowMaximizeMode::Full);
    QCOMPARE(migrated.applications[0].windows[1].state.maximizeMode, WindowMaximizeMode::None);
}

void ProfileSerializerTest::keepsLaunchSpecificationSeparateFromWindowMatching()
{
    const QJsonObject document = validDocument();
    const QJsonObject application = document.value(QStringLiteral("applications")).toArray().at(0).toObject();
    const QJsonObject launch = application.value(QStringLiteral("launch")).toObject();
    const QJsonObject firstWindow = application.value(QStringLiteral("windows")).toArray().at(0).toObject();
    const QJsonObject identity =
        firstWindow.value(QStringLiteral("match")).toObject().value(QStringLiteral("application_identity")).toObject();

    QCOMPARE(launch.value(QStringLiteral("desktop_entry_id")).toString(), QStringLiteral("org.kde.kate"));
    QCOMPARE(identity.value(QStringLiteral("kind")).toString(), QStringLiteral("desktop_file_name"));
    QCOMPARE(identity.value(QStringLiteral("value")).toString(), QStringLiteral("org.kde.kate"));
    QVERIFY(!firstWindow.contains(QStringLiteral("launch")));
}

void ProfileSerializerTest::rejectsMalformedJson()
{
    const auto result = ProfileSerializer::deserialize(QByteArrayLiteral("{ invalid"));
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidJson);
}

void ProfileSerializerTest::rejectsNonObjectRoot()
{
    const auto result = ProfileSerializer::deserialize(QByteArrayLiteral("[]"));
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
}

void ProfileSerializerTest::rejectsUnknownProperties()
{
    QJsonObject document = validDocument();
    document.insert(QStringLiteral("unexpected"), true);

    const auto result = deserialize(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
}

void ProfileSerializerTest::rejectsMissingRequiredProperties()
{
    QJsonObject document = validDocument();
    document.remove(QStringLiteral("applications"));

    const auto result = deserialize(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
}

void ProfileSerializerTest::rejectsNonCanonicalIdentifiers()
{
    QJsonObject document = validDocument();
    QJsonObject profileObject = document.value(QStringLiteral("profile")).toObject();
    profileObject.insert(QStringLiteral("id"),
                         QStringLiteral("{%1}").arg(profileObject.value(QStringLiteral("id")).toString()));
    document.insert(QStringLiteral("profile"), profileObject);

    const auto result = deserialize(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
}

void ProfileSerializerTest::rejectsInvalidWindowStateTypes()
{
    QJsonObject document = validDocument();
    QJsonArray applications = document.value(QStringLiteral("applications")).toArray();
    QJsonObject application = applications[0].toObject();
    QJsonArray windows = application.value(QStringLiteral("windows")).toArray();
    QJsonObject window = windows[0].toObject();
    QJsonObject state = window.value(QStringLiteral("state")).toObject();
    state.insert(QStringLiteral("minimized"), 0);
    window.insert(QStringLiteral("state"), state);
    windows[0] = window;
    application.insert(QStringLiteral("windows"), windows);
    applications[0] = application;
    document.insert(QStringLiteral("applications"), applications);

    const auto result = deserialize(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
}

void ProfileSerializerTest::rejectsUnknownMaximizeMode()
{
    QJsonObject document = validDocument();
    QJsonArray applications = document.value(QStringLiteral("applications")).toArray();
    QJsonObject application = applications[0].toObject();
    QJsonArray windows = application.value(QStringLiteral("windows")).toArray();
    QJsonObject window = windows[0].toObject();
    QJsonObject state = window.value(QStringLiteral("state")).toObject();
    state.insert(QStringLiteral("maximize_mode"), QStringLiteral("diagonal"));
    window.insert(QStringLiteral("state"), state);
    windows[0] = window;
    application.insert(QStringLiteral("windows"), windows);
    applications[0] = application;
    document.insert(QStringLiteral("applications"), applications);

    const auto result = deserialize(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
    QCOMPARE(std::get<ProfileError>(result).path,
             QStringLiteral("applications[0].windows[0].state.maximize_mode"));
}

void ProfileSerializerTest::rejectsLegacyMaximizedPropertyInCurrentSchema()
{
    QJsonObject document = validDocument();
    QJsonArray applications = document.value(QStringLiteral("applications")).toArray();
    QJsonObject application = applications[0].toObject();
    QJsonArray windows = application.value(QStringLiteral("windows")).toArray();
    QJsonObject window = windows[0].toObject();
    QJsonObject state = window.value(QStringLiteral("state")).toObject();
    state.insert(QStringLiteral("maximized"), true);
    window.insert(QStringLiteral("state"), state);
    windows[0] = window;
    application.insert(QStringLiteral("windows"), windows);
    applications[0] = application;
    document.insert(QStringLiteral("applications"), applications);

    const auto result = deserialize(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
}

void ProfileSerializerTest::rejectsSemanticallyInvalidGeometry()
{
    QJsonObject document = validDocument();
    QJsonArray applications = document.value(QStringLiteral("applications")).toArray();
    QJsonObject application = applications[0].toObject();
    QJsonArray windows = application.value(QStringLiteral("windows")).toArray();
    QJsonObject window = windows[0].toObject();
    QJsonObject geometry = window.value(QStringLiteral("geometry")).toObject();
    geometry.insert(QStringLiteral("x"), 0.5);
    geometry.insert(QStringLiteral("width"), 0.6);
    window.insert(QStringLiteral("geometry"), geometry);
    windows[0] = window;
    application.insert(QStringLiteral("windows"), windows);
    applications[0] = application;
    document.insert(QStringLiteral("applications"), applications);

    const auto result = deserialize(document);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::ValidationFailed);
}

void ProfileSerializerTest::rejectsInvalidDomainObjects()
{
    Profile profile = test::makeValidProfile();
    profile.name.clear();

    const auto result = ProfileSerializer::serialize(profile);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::ValidationFailed);
    QVERIFY(!std::get<ProfileError>(result).issues.isEmpty());
}

QTEST_APPLESS_MAIN(ProfileSerializerTest)

#include "test_profile_serializer.moc"
