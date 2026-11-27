#pragma once

#include <QVector>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <optional>

namespace kwm::profiles
{

enum class ApplicationIdentityKind
{
    DesktopFileName,
    ResourceClass
};

[[nodiscard]] QString applicationIdentityKindToString(ApplicationIdentityKind kind);
[[nodiscard]] std::optional<ApplicationIdentityKind> applicationIdentityKindFromString(const QString &value);

enum class DesktopMembershipMode
{
    Specific,
    All
};

[[nodiscard]] QString desktopMembershipModeToString(DesktopMembershipMode mode);
[[nodiscard]] std::optional<DesktopMembershipMode> desktopMembershipModeFromString(const QString &value);

struct Geometry
{
    double x = 0.0;
    double y = 0.0;
    double width = 1.0;
    double height = 1.0;

    bool operator==(const Geometry &) const = default;
};

struct ApplicationIdentity
{
    ApplicationIdentityKind kind = ApplicationIdentityKind::DesktopFileName;
    QString value;

    bool operator==(const ApplicationIdentity &) const = default;
};

struct WindowMatch
{
    ApplicationIdentity applicationIdentity;
    std::optional<QString> role;

    bool operator==(const WindowMatch &) const = default;
};

struct DesktopMembership
{
    DesktopMembershipMode mode = DesktopMembershipMode::Specific;
    QVector<QUuid> desktopIds;

    bool operator==(const DesktopMembership &) const = default;
};

struct WindowState
{
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;

    bool operator==(const WindowState &) const = default;
};

struct Window
{
    QUuid id;
    WindowMatch match;
    DesktopMembership desktopMembership;
    std::optional<Geometry> geometry;
    WindowState state;

    bool operator==(const Window &) const = default;
};

struct LaunchSpecification
{
    QString desktopEntryId;
    QStringList arguments;
    std::optional<QString> workingDirectory;

    bool operator==(const LaunchSpecification &) const = default;
};

struct Application
{
    QUuid id;
    std::optional<LaunchSpecification> launch;
    QVector<Window> windows;

    bool operator==(const Application &) const = default;
};

struct VirtualDesktop
{
    QUuid id;
    QString name;

    bool operator==(const VirtualDesktop &) const = default;
};

struct Profile
{
    QUuid id;
    QString name;
    std::optional<QUuid> activeDesktopId;
    QVector<VirtualDesktop> virtualDesktops;
    QVector<Application> applications;

    bool operator==(const Profile &) const = default;
};

} // namespace kwm::profiles
