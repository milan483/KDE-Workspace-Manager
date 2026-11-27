#pragma once

#include "profiles/profile.h"

namespace kwm::profiles::test
{

inline Profile makeValidProfile()
{
    Profile profile;
    profile.id = QUuid::createUuid();
    profile.name = QStringLiteral("Development");

    const QUuid firstDesktopId = QUuid::createUuid();
    const QUuid secondDesktopId = QUuid::createUuid();
    profile.virtualDesktops = {{firstDesktopId, QStringLiteral("Development")},
                               {secondDesktopId, QStringLiteral("Communication")}};
    profile.activeDesktopId = firstDesktopId;

    Application application;
    application.id = QUuid::createUuid();

    LaunchSpecification launch;
    launch.desktopEntryId = QStringLiteral("org.kde.kate");
    launch.arguments = {QStringLiteral("--new-window"), QStringLiteral("/tmp/project")};
    launch.workingDirectory = QStringLiteral("/home/user/project");
    application.launch = launch;

    Window editorWindow;
    editorWindow.id = QUuid::createUuid();
    editorWindow.match.applicationIdentity = {ApplicationIdentityKind::DesktopFileName, QStringLiteral("org.kde.kate")};
    editorWindow.match.role = QStringLiteral("main");
    editorWindow.desktopMembership = {DesktopMembershipMode::Specific, {firstDesktopId, secondDesktopId}};
    editorWindow.geometry = Geometry{0.05, 0.05, 0.9, 0.9};
    editorWindow.state = {false, true, false};

    Window stickyWindow;
    stickyWindow.id = QUuid::createUuid();
    stickyWindow.match.applicationIdentity = {ApplicationIdentityKind::ResourceClass, QStringLiteral("org.kde.kate")};
    stickyWindow.desktopMembership = {DesktopMembershipMode::All, {}};
    stickyWindow.state = {true, false, true};

    application.windows = {editorWindow, stickyWindow};
    profile.applications.push_back(application);

    return profile;
}

} // namespace kwm::profiles::test
