#include "profiles/profile_validator.h"

#include <QSet>

#include <cmath>

namespace kwm::profiles
{
namespace
{

void addIssue(QVector<ProfileIssue> &issues, const QString &path, const QString &message)
{
    issues.push_back({path, message});
}

bool registerId(const QUuid &id, const QString &path, QSet<QUuid> &ids, QVector<ProfileIssue> &issues)
{
    if (id.isNull())
    {
        addIssue(issues, path, QStringLiteral("Identifier must be a non-null UUID."));
        return false;
    }

    if (ids.contains(id))
    {
        addIssue(issues, path, QStringLiteral("Identifier is duplicated within this profile."));
        return false;
    }

    ids.insert(id);
    return true;
}

} // namespace

QVector<ProfileIssue> ProfileValidator::validate(const Profile &profile)
{
    QVector<ProfileIssue> issues;
    QSet<QUuid> allIds;
    QSet<QUuid> desktopIds;

    registerId(profile.id, QStringLiteral("profile.id"), allIds, issues);

    if (profile.name.trimmed().isEmpty())
    {
        addIssue(issues, QStringLiteral("profile.name"), QStringLiteral("Profile name must not be empty."));
    }

    for (qsizetype index = 0; index < profile.virtualDesktops.size(); ++index)
    {
        const auto &desktop = profile.virtualDesktops.at(index);
        const QString path = QStringLiteral("virtual_desktops[%1]").arg(index);

        if (registerId(desktop.id, path + QStringLiteral(".id"), allIds, issues))
        {
            desktopIds.insert(desktop.id);
        }

        if (desktop.name.trimmed().isEmpty())
        {
            addIssue(issues, path + QStringLiteral(".name"), QStringLiteral("Desktop name must not be empty."));
        }
    }

    if (profile.activeDesktopId.has_value())
    {
        if (profile.activeDesktopId->isNull())
        {
            addIssue(issues, QStringLiteral("active_desktop_id"),
                     QStringLiteral("Active desktop identifier must be a non-null UUID."));
        }
        else if (!desktopIds.contains(*profile.activeDesktopId))
        {
            addIssue(issues, QStringLiteral("active_desktop_id"),
                     QStringLiteral("Active desktop does not reference a desktop in this profile."));
        }
    }

    for (qsizetype applicationIndex = 0; applicationIndex < profile.applications.size(); ++applicationIndex)
    {
        const auto &application = profile.applications.at(applicationIndex);
        const QString applicationPath = QStringLiteral("applications[%1]").arg(applicationIndex);

        registerId(application.id, applicationPath + QStringLiteral(".id"), allIds, issues);

        if (application.launch.has_value())
        {
            const auto &launch = *application.launch;
            const QString launchPath = applicationPath + QStringLiteral(".launch");

            if (launch.desktopEntryId.trimmed().isEmpty())
            {
                addIssue(issues, launchPath + QStringLiteral(".desktop_entry_id"),
                         QStringLiteral("Desktop entry identifier must not be empty."));
            }

            if (launch.workingDirectory.has_value() && launch.workingDirectory->trimmed().isEmpty())
            {
                addIssue(issues, launchPath + QStringLiteral(".working_directory"),
                         QStringLiteral("Working directory must not be empty when provided."));
            }
        }

        for (qsizetype windowIndex = 0; windowIndex < application.windows.size(); ++windowIndex)
        {
            const auto &window = application.windows.at(windowIndex);
            const QString windowPath = applicationPath + QStringLiteral(".windows[%1]").arg(windowIndex);

            registerId(window.id, windowPath + QStringLiteral(".id"), allIds, issues);

            if (applicationIdentityKindToString(window.match.applicationIdentity.kind).isEmpty())
            {
                addIssue(issues, windowPath + QStringLiteral(".match.application_identity.kind"),
                         QStringLiteral("Application identity kind is not supported."));
            }

            if (window.match.applicationIdentity.value.trimmed().isEmpty())
            {
                addIssue(issues, windowPath + QStringLiteral(".match.application_identity.value"),
                         QStringLiteral("Application identity value must not be empty."));
            }

            if (window.match.role.has_value() && window.match.role->trimmed().isEmpty())
            {
                addIssue(issues, windowPath + QStringLiteral(".match.role"),
                         QStringLiteral("Window role must not be empty when provided."));
            }

            switch (window.desktopMembership.mode)
            {
            case DesktopMembershipMode::Specific: {
                if (window.desktopMembership.desktopIds.isEmpty())
                {
                    addIssue(issues, windowPath + QStringLiteral(".desktop_membership.desktop_ids"),
                             QStringLiteral("Specific desktop membership requires at least one desktop."));
                }

                QSet<QUuid> memberships;
                for (qsizetype desktopIndex = 0; desktopIndex < window.desktopMembership.desktopIds.size();
                     ++desktopIndex)
                {
                    const QUuid desktopId = window.desktopMembership.desktopIds.at(desktopIndex);
                    const QString membershipPath =
                        windowPath + QStringLiteral(".desktop_membership.desktop_ids[%1]").arg(desktopIndex);

                    if (desktopId.isNull())
                    {
                        addIssue(issues, membershipPath, QStringLiteral("Desktop reference must be a non-null UUID."));
                    }
                    else if (!desktopIds.contains(desktopId))
                    {
                        addIssue(issues, membershipPath,
                                 QStringLiteral("Desktop reference does not exist in this profile."));
                    }

                    if (memberships.contains(desktopId))
                    {
                        addIssue(issues, membershipPath,
                                 QStringLiteral("Desktop membership contains a duplicate reference."));
                    }
                    memberships.insert(desktopId);
                }
                break;
            }
            case DesktopMembershipMode::All:
                if (!window.desktopMembership.desktopIds.isEmpty())
                {
                    addIssue(issues, windowPath + QStringLiteral(".desktop_membership.desktop_ids"),
                             QStringLiteral("All-desktop membership must not contain specific desktop identifiers."));
                }
                break;
            default:
                addIssue(issues, windowPath + QStringLiteral(".desktop_membership.mode"),
                         QStringLiteral("Desktop membership mode is not supported."));
                break;
            }

            if (window.geometry.has_value())
            {
                const auto &geometry = *window.geometry;
                const bool finite = std::isfinite(geometry.x) && std::isfinite(geometry.y) &&
                                    std::isfinite(geometry.width) && std::isfinite(geometry.height);
                const bool positiveSize = geometry.width > 0.0 && geometry.height > 0.0;
                const bool normalizedOrigin = geometry.x >= 0.0 && geometry.y >= 0.0;
                const bool contained = geometry.x + geometry.width <= 1.0 && geometry.y + geometry.height <= 1.0;

                if (!finite || !positiveSize || !normalizedOrigin || !contained)
                {
                    addIssue(issues, windowPath + QStringLiteral(".geometry"),
                             QStringLiteral("Geometry must be finite, have positive dimensions, and fit within the "
                                            "normalized work area."));
                }
            }

            if (window.state.maximized && window.state.fullscreen)
            {
                addIssue(issues, windowPath + QStringLiteral(".state"),
                         QStringLiteral("A window cannot be maximized and fullscreen simultaneously."));
            }
        }
    }

    return issues;
}

} // namespace kwm::profiles
