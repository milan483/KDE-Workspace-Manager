#include "profiles/profile.h"

namespace kwm::profiles
{

QString applicationIdentityKindToString(ApplicationIdentityKind kind)
{
    switch (kind)
    {
    case ApplicationIdentityKind::DesktopFileName:
        return QStringLiteral("desktop_file_name");
    case ApplicationIdentityKind::ResourceClass:
        return QStringLiteral("resource_class");
    }

    return {};
}

std::optional<ApplicationIdentityKind> applicationIdentityKindFromString(const QString &value)
{
    if (value == QStringLiteral("desktop_file_name"))
    {
        return ApplicationIdentityKind::DesktopFileName;
    }
    if (value == QStringLiteral("resource_class"))
    {
        return ApplicationIdentityKind::ResourceClass;
    }

    return std::nullopt;
}

QString desktopMembershipModeToString(DesktopMembershipMode mode)
{
    switch (mode)
    {
    case DesktopMembershipMode::Specific:
        return QStringLiteral("specific");
    case DesktopMembershipMode::All:
        return QStringLiteral("all");
    }

    return {};
}

std::optional<DesktopMembershipMode> desktopMembershipModeFromString(const QString &value)
{
    if (value == QStringLiteral("specific"))
    {
        return DesktopMembershipMode::Specific;
    }
    if (value == QStringLiteral("all"))
    {
        return DesktopMembershipMode::All;
    }

    return std::nullopt;
}

QString windowMaximizeModeToString(WindowMaximizeMode mode)
{
    switch (mode)
    {
    case WindowMaximizeMode::None:
        return QStringLiteral("none");
    case WindowMaximizeMode::Vertical:
        return QStringLiteral("vertical");
    case WindowMaximizeMode::Horizontal:
        return QStringLiteral("horizontal");
    case WindowMaximizeMode::Full:
        return QStringLiteral("full");
    }

    return {};
}

std::optional<WindowMaximizeMode> windowMaximizeModeFromString(const QString &value)
{
    if (value == QStringLiteral("none"))
    {
        return WindowMaximizeMode::None;
    }
    if (value == QStringLiteral("vertical"))
    {
        return WindowMaximizeMode::Vertical;
    }
    if (value == QStringLiteral("horizontal"))
    {
        return WindowMaximizeMode::Horizontal;
    }
    if (value == QStringLiteral("full"))
    {
        return WindowMaximizeMode::Full;
    }

    return std::nullopt;
}

} // namespace kwm::profiles
