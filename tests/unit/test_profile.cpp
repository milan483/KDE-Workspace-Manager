#include "profile_test_utils.h"

#include <QtTest/QtTest>

using namespace kwm::profiles;

class ProfileTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void domainModelPreservesDistinctWindowStates();
    void domainModelSupportsMultipleAndAllDesktopMembership();
    void applicationLaunchSpecificationIsSeparateFromWindowIdentity();
    void identityKindsHaveStableFormatNames();
    void maximizeModesHaveStableFormatNames();
};

void ProfileTest::domainModelPreservesDistinctWindowStates()
{
    const Profile profile = test::makeValidProfile();
    const auto &windows = profile.applications.constFirst().windows;

    QVERIFY(windows.at(0).state.maximizeMode == WindowMaximizeMode::Full);
    QVERIFY(!windows.at(0).state.minimized);
    QVERIFY(!windows.at(0).state.fullscreen);

    QVERIFY(windows.at(1).state.minimized);
    QVERIFY(windows.at(1).state.maximizeMode == WindowMaximizeMode::None);
    QVERIFY(windows.at(1).state.fullscreen);
}

void ProfileTest::domainModelSupportsMultipleAndAllDesktopMembership()
{
    const Profile profile = test::makeValidProfile();
    const auto &windows = profile.applications.constFirst().windows;

    QVERIFY(windows.at(0).desktopMembership.mode == DesktopMembershipMode::Specific);
    QCOMPARE(windows.at(0).desktopMembership.desktopIds.size(), 2);
    QVERIFY(windows.at(1).desktopMembership.mode == DesktopMembershipMode::All);
    QVERIFY(windows.at(1).desktopMembership.desktopIds.isEmpty());
}

void ProfileTest::applicationLaunchSpecificationIsSeparateFromWindowIdentity()
{
    const Profile profile = test::makeValidProfile();
    const auto &application = profile.applications.constFirst();

    QVERIFY(application.launch.has_value());
    QCOMPARE(application.launch->desktopEntryId, QStringLiteral("org.kde.kate"));
    QCOMPARE(application.windows.constFirst().match.applicationIdentity.value, QStringLiteral("org.kde.kate"));
    QVERIFY(application.windows.constFirst().match.applicationIdentity.kind ==
            ApplicationIdentityKind::DesktopFileName);
}

void ProfileTest::identityKindsHaveStableFormatNames()
{
    QCOMPARE(applicationIdentityKindToString(ApplicationIdentityKind::DesktopFileName),
             QStringLiteral("desktop_file_name"));
    QCOMPARE(applicationIdentityKindToString(ApplicationIdentityKind::ResourceClass), QStringLiteral("resource_class"));
    QVERIFY(applicationIdentityKindFromString(QStringLiteral("desktop_file_name")) ==
            ApplicationIdentityKind::DesktopFileName);
    QVERIFY(applicationIdentityKindFromString(QStringLiteral("resource_class")) ==
            ApplicationIdentityKind::ResourceClass);
    QVERIFY(!applicationIdentityKindFromString(QStringLiteral("app_id")).has_value());
}

void ProfileTest::maximizeModesHaveStableFormatNames()
{
    QCOMPARE(windowMaximizeModeToString(WindowMaximizeMode::None), QStringLiteral("none"));
    QCOMPARE(windowMaximizeModeToString(WindowMaximizeMode::Vertical), QStringLiteral("vertical"));
    QCOMPARE(windowMaximizeModeToString(WindowMaximizeMode::Horizontal), QStringLiteral("horizontal"));
    QCOMPARE(windowMaximizeModeToString(WindowMaximizeMode::Full), QStringLiteral("full"));

    QVERIFY(windowMaximizeModeFromString(QStringLiteral("none")) == WindowMaximizeMode::None);
    QVERIFY(windowMaximizeModeFromString(QStringLiteral("vertical")) == WindowMaximizeMode::Vertical);
    QVERIFY(windowMaximizeModeFromString(QStringLiteral("horizontal")) == WindowMaximizeMode::Horizontal);
    QVERIFY(windowMaximizeModeFromString(QStringLiteral("full")) == WindowMaximizeMode::Full);
    QVERIFY(!windowMaximizeModeFromString(QStringLiteral("partial")).has_value());
}

QTEST_APPLESS_MAIN(ProfileTest)

#include "test_profile.moc"
