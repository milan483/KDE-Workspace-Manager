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
};

void ProfileTest::domainModelPreservesDistinctWindowStates()
{
    const Profile profile = test::makeValidProfile();
    const auto &windows = profile.applications.constFirst().windows;

    QVERIFY(windows.at(0).state.maximized);
    QVERIFY(!windows.at(0).state.minimized);
    QVERIFY(!windows.at(0).state.fullscreen);

    QVERIFY(windows.at(1).state.minimized);
    QVERIFY(!windows.at(1).state.maximized);
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

QTEST_APPLESS_MAIN(ProfileTest)

#include "test_profile.moc"
