#include "profile_test_utils.h"

#include "profiles/profile_validator.h"

#include <QtTest/QtTest>

using namespace kwm::profiles;

class ProfileValidatorTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void acceptsValidProfile();
    void rejectsDuplicateIdentifiersAcrossEntityTypes();
    void rejectsUnknownActiveDesktop();
    void rejectsInvalidDesktopMembershipReferences();
    void rejectsDuplicateDesktopMembershipReferences();
    void rejectsEmptySpecificMembership();
    void rejectsSpecificIdsForAllDesktopMembership();
    void rejectsOutOfBoundsGeometry();
    void rejectsConflictingMaximizedAndFullscreenStates();
    void allowsMinimizedWindowsToRetainUnderlyingState();
    void rejectsMissingApplicationIdentity();
};

namespace
{

bool hasIssueAt(const QVector<ProfileIssue> &issues, const QString &path)
{
    for (const ProfileIssue &issue : issues)
    {
        if (issue.path == path)
        {
            return true;
        }
    }
    return false;
}

} // namespace

void ProfileValidatorTest::acceptsValidProfile()
{
    QVERIFY(ProfileValidator::validate(test::makeValidProfile()).isEmpty());
}

void ProfileValidatorTest::rejectsDuplicateIdentifiersAcrossEntityTypes()
{
    Profile profile = test::makeValidProfile();
    profile.applications[0].id = profile.id;

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("applications[0].id")));
}

void ProfileValidatorTest::rejectsUnknownActiveDesktop()
{
    Profile profile = test::makeValidProfile();
    profile.activeDesktopId = QUuid::createUuid();

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("active_desktop_id")));
}

void ProfileValidatorTest::rejectsInvalidDesktopMembershipReferences()
{
    Profile profile = test::makeValidProfile();
    profile.applications[0].windows[0].desktopMembership.desktopIds[0] = QUuid::createUuid();

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("applications[0].windows[0].desktop_membership.desktop_ids[0]")));
}

void ProfileValidatorTest::rejectsDuplicateDesktopMembershipReferences()
{
    Profile profile = test::makeValidProfile();
    const QUuid desktopId = profile.virtualDesktops[0].id;
    profile.applications[0].windows[0].desktopMembership.desktopIds = {desktopId, desktopId};

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("applications[0].windows[0].desktop_membership.desktop_ids[1]")));
}

void ProfileValidatorTest::rejectsEmptySpecificMembership()
{
    Profile profile = test::makeValidProfile();
    profile.applications[0].windows[0].desktopMembership.desktopIds.clear();

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("applications[0].windows[0].desktop_membership.desktop_ids")));
}

void ProfileValidatorTest::rejectsSpecificIdsForAllDesktopMembership()
{
    Profile profile = test::makeValidProfile();
    profile.applications[0].windows[1].desktopMembership.desktopIds.push_back(profile.virtualDesktops[0].id);

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("applications[0].windows[1].desktop_membership.desktop_ids")));
}

void ProfileValidatorTest::rejectsOutOfBoundsGeometry()
{
    Profile profile = test::makeValidProfile();
    profile.applications[0].windows[0].geometry = Geometry{0.5, 0.1, 0.6, 0.5};

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("applications[0].windows[0].geometry")));
}

void ProfileValidatorTest::rejectsConflictingMaximizedAndFullscreenStates()
{
    Profile profile = test::makeValidProfile();
    profile.applications[0].windows[0].state.maximized = true;
    profile.applications[0].windows[0].state.fullscreen = true;

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("applications[0].windows[0].state")));
}

void ProfileValidatorTest::allowsMinimizedWindowsToRetainUnderlyingState()
{
    Profile profile = test::makeValidProfile();
    profile.applications[0].windows[0].state = {true, true, false};
    profile.applications[0].windows[1].state = {true, false, true};

    QVERIFY(ProfileValidator::validate(profile).isEmpty());
}

void ProfileValidatorTest::rejectsMissingApplicationIdentity()
{
    Profile profile = test::makeValidProfile();
    profile.applications[0].windows[0].match.applicationIdentity.value.clear();

    const auto issues = ProfileValidator::validate(profile);
    QVERIFY(hasIssueAt(issues, QStringLiteral("applications[0].windows[0].match.application_identity.value")));
}

QTEST_APPLESS_MAIN(ProfileValidatorTest)

#include "test_profile_validator.moc"
