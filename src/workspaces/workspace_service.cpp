#include "workspaces/workspace_service.h"

#include "profiles/profile_validator.h"

#include <QHash>
#include <QSet>
#include <QVariantList>

#include <utility>

namespace kwm::workspaces
{
namespace
{

QString uuidString(const QUuid &id)
{
    return id.toString(QUuid::WithoutBraces);
}

QUuid parseProfileId(const QString &value)
{
    return QUuid::fromString(value);
}

QString applicationIdentityKey(const kwm::profiles::ApplicationIdentity &identity)
{
    return kwm::profiles::applicationIdentityKindToString(identity.kind) + QLatin1Char(':') + identity.value;
}

QString applicationIdentityLabel(const kwm::profiles::ApplicationIdentity &identity)
{
    const QString kind = kwm::profiles::applicationIdentityKindToString(identity.kind);
    if (kind == QStringLiteral("desktop_file_name"))
    {
        return QStringLiteral("Desktop entry: %1").arg(identity.value);
    }
    return QStringLiteral("Window class: %1").arg(identity.value);
}

void collectProfileIds(const kwm::profiles::Profile &profile, QSet<QUuid> &ids)
{
    ids.insert(profile.id);
    for (const kwm::profiles::VirtualDesktop &desktop : profile.virtualDesktops)
    {
        ids.insert(desktop.id);
    }
    for (const kwm::profiles::Application &application : profile.applications)
    {
        ids.insert(application.id);
        for (const kwm::profiles::Window &window : application.windows)
        {
            ids.insert(window.id);
        }
    }
}

QUuid createUniqueId(QSet<QUuid> &usedIds)
{
    QUuid id;
    do
    {
        id = QUuid::createUuid();
    } while (id.isNull() || usedIds.contains(id));
    usedIds.insert(id);
    return id;
}

} // namespace

WorkspaceService::WorkspaceService(kwm::profiles::ProfileRepository repository,
                                   WorkspaceCaptureBackend *captureBackend,
                                   QObject *parent)
    : QObject(parent)
    , m_repository(std::move(repository))
    , m_captureBackend(captureBackend)
    , m_profileModel(new ProfileListModel(this))
{
    if (m_captureBackend && !m_captureBackend->parent())
    {
        m_captureBackend->setParent(this);
    }
    initialize();
}

ProfileListModel *WorkspaceService::profileModel() const
{
    return m_profileModel;
}

bool WorkspaceService::isBusy() const
{
    return m_busy;
}

QString WorkspaceService::statusMessage() const
{
    return m_statusMessage;
}

QString WorkspaceService::errorMessage() const
{
    return m_errorMessage;
}

QString WorkspaceService::warningMessage() const
{
    return m_warningMessage;
}

bool WorkspaceService::hasPreview() const
{
    return m_previewSnapshot.has_value();
}

QVariantMap WorkspaceService::preview() const
{
    return m_previewData;
}

bool WorkspaceService::previewIsUpdate() const
{
    return hasPreview() && m_pendingUpdateProfileId.has_value();
}

void WorkspaceService::initialize()
{
    if (m_captureBackend)
    {
        connect(m_captureBackend, &WorkspaceCaptureBackend::started,
                this, &WorkspaceService::handleCaptureStarted);
        connect(m_captureBackend, &WorkspaceCaptureBackend::failed,
                this, &WorkspaceService::handleCaptureBackendFailed);
        connect(m_captureBackend, &WorkspaceCaptureBackend::captureCompleted,
                this, &WorkspaceService::handleCaptureCompleted);
        connect(m_captureBackend, &WorkspaceCaptureBackend::captureFailed,
                this, &WorkspaceService::handleCaptureFailed);
    }
    refreshProfiles();
}

void WorkspaceService::refreshProfiles()
{
    const auto result = m_repository.list();
    if (const auto *error = std::get_if<kwm::profiles::ProfileError>(&result))
    {
        setErrorMessage(error->message);
        setWarningMessage({});
        return;
    }

    const kwm::profiles::ProfileListResult &listing = std::get<kwm::profiles::ProfileListResult>(result);
    m_profileModel->setProfiles(listing.profiles);

    if (listing.errors.isEmpty())
    {
        setWarningMessage({});
    }
    else
    {
        const QString firstError = listing.errors.constFirst().message;
        setWarningMessage(QStringLiteral("%1 saved profile file(s) could not be loaded. %2")
                              .arg(listing.errors.size())
                              .arg(firstError));
    }
    setErrorMessage({});
}

void WorkspaceService::saveWorkspace(const QString &name)
{
    if (!ensureNoPendingOperation(QStringLiteral("save a workspace")))
    {
        return;
    }
    beginCapture(name.trimmed(), std::nullopt);
}

void WorkspaceService::updateWorkspace(const QString &profileId)
{
    if (!ensureNoPendingOperation(QStringLiteral("update a workspace")))
    {
        return;
    }

    const QUuid id = parseProfileId(profileId);
    if (id.isNull())
    {
        setErrorMessage(QStringLiteral("The selected profile identifier is invalid."));
        return;
    }

    const auto loaded = m_repository.load(id);
    if (const auto *error = std::get_if<kwm::profiles::ProfileError>(&loaded))
    {
        setErrorMessage(error->message);
        return;
    }

    const kwm::profiles::Profile &profile = std::get<kwm::profiles::Profile>(loaded);
    beginCapture(profile.name, id);
}

void WorkspaceService::beginCapture(const QString &name, std::optional<QUuid> updateProfileId)
{
    if (name.trimmed().isEmpty())
    {
        setErrorMessage(QStringLiteral("Workspace name must not be empty."));
        return;
    }
    if (!m_captureBackend)
    {
        setErrorMessage(QStringLiteral("The workspace capture backend is unavailable."));
        return;
    }

    clearPreview();
    m_pendingProfileName = name;
    m_pendingUpdateProfileId = updateProfileId;
    m_pendingCaptureId.clear();
    m_waitingForCaptureStart = false;

    setErrorMessage({});
    setStatusMessage(QStringLiteral("Preparing workspace capture…"));
    setBusy(true);

    if (m_captureBackend->isRunning())
    {
        startCaptureRequest();
        return;
    }

    m_waitingForCaptureStart = true;
    m_captureBackend->start();
}

void WorkspaceService::startCaptureRequest()
{
    if (!m_busy || !m_captureBackend)
    {
        return;
    }

    m_waitingForCaptureStart = false;
    setStatusMessage(QStringLiteral("Capturing the current workspace…"));
    const QString requestId = m_captureBackend->capture(m_pendingProfileName);
    if (requestId.isEmpty())
    {
        if (m_busy)
        {
            failCapture(QStringLiteral("The capture backend did not accept the request."));
        }
        return;
    }
    m_pendingCaptureId = requestId;
}

void WorkspaceService::handleCaptureStarted()
{
    if (!m_busy || !m_waitingForCaptureStart)
    {
        return;
    }
    startCaptureRequest();
}

void WorkspaceService::handleCaptureBackendFailed(const QString &message)
{
    if (m_busy)
    {
        failCapture(message);
    }
}

void WorkspaceService::handleCaptureCompleted(const QString &requestId,
                                              const WorkspaceCaptureSnapshot &snapshot)
{
    if (!m_busy || requestId != m_pendingCaptureId)
    {
        return;
    }

    m_waitingForCaptureStart = false;
    m_pendingCaptureId.clear();
    m_previewSnapshot = snapshot;
    m_previewData = capturePreview(snapshot, m_pendingUpdateProfileId.has_value());
    if (m_pendingUpdateProfileId.has_value())
    {
        m_previewData.insert(QStringLiteral("profileId"), uuidString(*m_pendingUpdateProfileId));
    }
    setErrorMessage({});
    setBusy(false);
    setStatusMessage(QStringLiteral("Capture ready for review."));
    Q_EMIT previewChanged();
    Q_EMIT previewReady();
}

void WorkspaceService::handleCaptureFailed(const QString &requestId, const QString &path, const QString &message)
{
    if (!m_busy)
    {
        return;
    }
    if (!requestId.isEmpty() && !m_pendingCaptureId.isEmpty() && requestId != m_pendingCaptureId)
    {
        return;
    }
    const QString detailedMessage = path.isEmpty()
        ? message
        : QStringLiteral("%1 (%2)").arg(message, path);
    failCapture(detailedMessage);
}

void WorkspaceService::failCapture(const QString &message)
{
    m_waitingForCaptureStart = false;
    m_pendingCaptureId.clear();
    m_pendingProfileName.clear();
    m_pendingUpdateProfileId.reset();
    clearPreview();
    setBusy(false);
    setStatusMessage(QStringLiteral("Capture failed."));
    setErrorMessage(message);
}

bool WorkspaceService::confirmSavePreview()
{
    if (m_busy || !m_previewSnapshot.has_value())
    {
        setErrorMessage(QStringLiteral("There is no completed capture preview to save."));
        return false;
    }

    setErrorMessage({});
    setBusy(true);

    kwm::profiles::Profile profile = m_previewSnapshot->profile;
    const bool updating = m_pendingUpdateProfileId.has_value();
    if (updating)
    {
        const auto loaded = m_repository.load(*m_pendingUpdateProfileId);
        if (const auto *error = std::get_if<kwm::profiles::ProfileError>(&loaded))
        {
            setBusy(false);
            setErrorMessage(error->message);
            return false;
        }

        const kwm::profiles::Profile &existing = std::get<kwm::profiles::Profile>(loaded);
        QString updateError;
        if (!updateSnapshotIdentity(existing, profile, updateError))
        {
            setBusy(false);
            setErrorMessage(updateError);
            return false;
        }
    }
    else
    {
        profile.name = m_pendingProfileName;
    }

    const QVector<kwm::profiles::ProfileIssue> issues = kwm::profiles::ProfileValidator::validate(profile);
    if (!issues.isEmpty())
    {
        setBusy(false);
        setErrorMessage(QStringLiteral("The profile failed validation: %1").arg(issues.constFirst().message));
        return false;
    }

    if (const auto error = m_repository.save(profile); error.has_value())
    {
        setBusy(false);
        setErrorMessage(error->message);
        return false;
    }

    m_profileModel->upsertProfile(profile);
    clearPreview();
    m_pendingProfileName.clear();
    m_pendingUpdateProfileId.reset();
    setBusy(false);
    setStatusMessage(updating ? QStringLiteral("Workspace updated.") : QStringLiteral("Workspace saved."));
    return true;
}

void WorkspaceService::cancelPreview()
{
    if (m_busy)
    {
        setErrorMessage(QStringLiteral("The capture is still in progress."));
        return;
    }
    if (!hasPreview())
    {
        return;
    }

    clearPreview();
    m_pendingProfileName.clear();
    m_pendingUpdateProfileId.reset();
    setErrorMessage({});
    setStatusMessage(QStringLiteral("Capture preview discarded."));
}

bool WorkspaceService::renameProfile(const QString &profileId, const QString &newName)
{
    if (!ensureNoPendingOperation(QStringLiteral("rename a profile")))
    {
        return false;
    }

    const QUuid id = parseProfileId(profileId);
    const QString name = newName.trimmed();
    if (id.isNull() || name.isEmpty())
    {
        setErrorMessage(QStringLiteral("A valid profile and non-empty name are required."));
        return false;
    }

    const auto loaded = m_repository.load(id);
    if (const auto *error = std::get_if<kwm::profiles::ProfileError>(&loaded))
    {
        setErrorMessage(error->message);
        return false;
    }

    kwm::profiles::Profile profile = std::get<kwm::profiles::Profile>(loaded);
    profile.name = name;
    if (const auto error = m_repository.save(profile); error.has_value())
    {
        setErrorMessage(error->message);
        return false;
    }

    m_profileModel->upsertProfile(profile);
    setErrorMessage({});
    setStatusMessage(QStringLiteral("Profile renamed."));
    return true;
}

bool WorkspaceService::duplicateProfile(const QString &profileId)
{
    if (!ensureNoPendingOperation(QStringLiteral("duplicate a profile")))
    {
        return false;
    }

    const QUuid id = parseProfileId(profileId);
    if (id.isNull())
    {
        setErrorMessage(QStringLiteral("The selected profile identifier is invalid."));
        return false;
    }

    const auto loaded = m_repository.load(id);
    if (const auto *error = std::get_if<kwm::profiles::ProfileError>(&loaded))
    {
        setErrorMessage(error->message);
        return false;
    }

    const kwm::profiles::Profile &source = std::get<kwm::profiles::Profile>(loaded);
    kwm::profiles::Profile duplicate;
    QString duplicateError;
    if (!duplicateWithNewIdentity(source, duplicate, duplicateError))
    {
        setErrorMessage(duplicateError);
        return false;
    }

    const QVector<kwm::profiles::ProfileIssue> issues = kwm::profiles::ProfileValidator::validate(duplicate);
    if (!issues.isEmpty())
    {
        setErrorMessage(QStringLiteral("The duplicated profile failed validation: %1")
                            .arg(issues.constFirst().message));
        return false;
    }

    if (const auto error = m_repository.save(duplicate); error.has_value())
    {
        setErrorMessage(error->message);
        return false;
    }

    m_profileModel->upsertProfile(duplicate);
    setErrorMessage({});
    setStatusMessage(QStringLiteral("Profile duplicated."));
    return true;
}

bool WorkspaceService::deleteProfile(const QString &profileId)
{
    if (!ensureNoPendingOperation(QStringLiteral("delete a profile")))
    {
        return false;
    }

    const QUuid id = parseProfileId(profileId);
    if (id.isNull())
    {
        setErrorMessage(QStringLiteral("The selected profile identifier is invalid."));
        return false;
    }

    if (const auto error = m_repository.remove(id); error.has_value())
    {
        setErrorMessage(error->message);
        return false;
    }

    if (!m_profileModel->removeProfile(id))
    {
        refreshProfiles();
    }
    setErrorMessage({});
    setStatusMessage(QStringLiteral("Profile deleted."));
    return true;
}

QVariantMap WorkspaceService::profileDetails(const QString &profileId)
{
    const QUuid id = parseProfileId(profileId);
    if (id.isNull())
    {
        setErrorMessage(QStringLiteral("The selected profile identifier is invalid."));
        return {};
    }

    const auto loaded = m_repository.load(id);
    if (const auto *error = std::get_if<kwm::profiles::ProfileError>(&loaded))
    {
        setErrorMessage(error->message);
        return {};
    }

    setErrorMessage({});
    return profileSummary(std::get<kwm::profiles::Profile>(loaded));
}

bool WorkspaceService::ensureNoPendingOperation(const QString &operation)
{
    if (m_busy)
    {
        setErrorMessage(QStringLiteral("Cannot %1 while a capture operation is in progress.").arg(operation));
        return false;
    }
    if (hasPreview())
    {
        setErrorMessage(QStringLiteral("Confirm or discard the current capture preview before continuing."));
        return false;
    }
    return true;
}

void WorkspaceService::setBusy(bool busy)
{
    if (m_busy == busy)
    {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

void WorkspaceService::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
    {
        return;
    }
    m_statusMessage = message;
    Q_EMIT statusMessageChanged();
}

void WorkspaceService::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message)
    {
        return;
    }
    m_errorMessage = message;
    Q_EMIT errorMessageChanged();
}

void WorkspaceService::setWarningMessage(const QString &message)
{
    if (m_warningMessage == message)
    {
        return;
    }
    m_warningMessage = message;
    Q_EMIT warningMessageChanged();
}

void WorkspaceService::clearPreview()
{
    const bool hadPreview = m_previewSnapshot.has_value() || !m_previewData.isEmpty();
    m_previewSnapshot.reset();
    m_previewData.clear();
    if (hadPreview)
    {
        Q_EMIT previewChanged();
    }
}

int WorkspaceService::windowCount(const kwm::profiles::Profile &profile)
{
    int count = 0;
    for (const kwm::profiles::Application &application : profile.applications)
    {
        count += application.windows.size();
    }
    return count;
}

int WorkspaceService::applicationsWithoutLaunchCount(const kwm::profiles::Profile &profile)
{
    int count = 0;
    for (const kwm::profiles::Application &application : profile.applications)
    {
        if (!application.launch.has_value())
        {
            ++count;
        }
    }
    return count;
}

QVariantMap WorkspaceService::capturePreview(const WorkspaceCaptureSnapshot &snapshot, bool isUpdate)
{
    const kwm::profiles::Profile &profile = snapshot.profile;
    QVariantMap result;
    result.insert(QStringLiteral("name"), profile.name);
    result.insert(QStringLiteral("profileId"), uuidString(profile.id));
    result.insert(QStringLiteral("desktopCount"), profile.virtualDesktops.size());
    result.insert(QStringLiteral("applicationCount"), profile.applications.size());
    result.insert(QStringLiteral("windowCount"), windowCount(profile));
    result.insert(QStringLiteral("excludedWindowCount"), snapshot.excludedWindowCount);
    result.insert(QStringLiteral("geometryOmittedCount"), snapshot.geometryOmittedCount);
    result.insert(QStringLiteral("stateGeometryUnavailableCount"), snapshot.stateGeometryUnavailableCount);
    result.insert(QStringLiteral("applicationsWithoutLaunchCount"), applicationsWithoutLaunchCount(profile));
    result.insert(QStringLiteral("isUpdate"), isUpdate);

    QHash<QUuid, QString> desktopNamesById;
    QStringList desktopNames;
    for (const kwm::profiles::VirtualDesktop &desktop : profile.virtualDesktops)
    {
        desktopNames.push_back(desktop.name);
        desktopNamesById.insert(desktop.id, desktop.name);
    }
    result.insert(QStringLiteral("desktopNames"), desktopNames);
    if (profile.activeDesktopId.has_value())
    {
        result.insert(QStringLiteral("activeDesktopName"),
                      desktopNamesById.value(*profile.activeDesktopId, QStringLiteral("Unknown desktop")));
    }

    QVariantList applications;
    QVariantList windows;
    for (const kwm::profiles::Application &application : profile.applications)
    {
        QVariantMap applicationItem;
        applicationItem.insert(QStringLiteral("windowCount"), application.windows.size());
        applicationItem.insert(QStringLiteral("launchAvailable"), application.launch.has_value());
        if (application.launch.has_value())
        {
            applicationItem.insert(QStringLiteral("label"), application.launch->desktopEntryId);
        }
        else if (!application.windows.isEmpty())
        {
            applicationItem.insert(
                QStringLiteral("label"),
                applicationIdentityLabel(application.windows.constFirst().match.applicationIdentity));
        }
        else
        {
            applicationItem.insert(QStringLiteral("label"), QStringLiteral("Application identity unavailable"));
        }
        applications.push_back(applicationItem);

        for (const kwm::profiles::Window &window : application.windows)
        {
            QVariantMap windowItem;
            windowItem.insert(QStringLiteral("identity"),
                              applicationIdentityLabel(window.match.applicationIdentity));
            windowItem.insert(QStringLiteral("role"), window.match.role.value_or(QString{}));
            windowItem.insert(QStringLiteral("geometryAvailable"), window.geometry.has_value());
            windowItem.insert(QStringLiteral("minimized"), window.state.minimized);
            windowItem.insert(QStringLiteral("maximizeMode"),
                              kwm::profiles::windowMaximizeModeToString(window.state.maximizeMode));
            windowItem.insert(QStringLiteral("fullscreen"), window.state.fullscreen);

            QStringList memberships;
            if (window.desktopMembership.mode == kwm::profiles::DesktopMembershipMode::All)
            {
                memberships.push_back(QStringLiteral("All desktops"));
            }
            else
            {
                for (const QUuid &desktopId : window.desktopMembership.desktopIds)
                {
                    memberships.push_back(desktopNamesById.value(desktopId, QStringLiteral("Unknown desktop")));
                }
            }
            windowItem.insert(QStringLiteral("desktops"), memberships);
            windows.push_back(windowItem);
        }
    }
    result.insert(QStringLiteral("applications"), applications);
    result.insert(QStringLiteral("windows"), windows);
    return result;
}

QVariantMap WorkspaceService::profileSummary(const kwm::profiles::Profile &profile)
{
    QVariantMap result;
    result.insert(QStringLiteral("profileId"), uuidString(profile.id));
    result.insert(QStringLiteral("name"), profile.name);
    result.insert(QStringLiteral("desktopCount"), profile.virtualDesktops.size());
    result.insert(QStringLiteral("applicationCount"), profile.applications.size());
    result.insert(QStringLiteral("windowCount"), windowCount(profile));
    result.insert(QStringLiteral("applicationsWithoutLaunchCount"), applicationsWithoutLaunchCount(profile));

    QHash<QUuid, QString> desktopNamesById;
    QStringList desktopNames;
    for (const kwm::profiles::VirtualDesktop &desktop : profile.virtualDesktops)
    {
        desktopNames.push_back(desktop.name);
        desktopNamesById.insert(desktop.id, desktop.name);
    }
    result.insert(QStringLiteral("desktopNames"), desktopNames);
    if (profile.activeDesktopId.has_value())
    {
        result.insert(QStringLiteral("activeDesktopName"),
                      desktopNamesById.value(*profile.activeDesktopId, QStringLiteral("Unknown desktop")));
    }

    QVariantList applications;
    QVariantList windows;
    for (const kwm::profiles::Application &application : profile.applications)
    {
        QVariantMap applicationItem;
        applicationItem.insert(QStringLiteral("windowCount"), application.windows.size());
        applicationItem.insert(QStringLiteral("launchAvailable"), application.launch.has_value());
        if (application.launch.has_value())
        {
            applicationItem.insert(QStringLiteral("label"), application.launch->desktopEntryId);
        }
        else if (!application.windows.isEmpty())
        {
            applicationItem.insert(
                QStringLiteral("label"),
                applicationIdentityLabel(application.windows.constFirst().match.applicationIdentity));
        }
        else
        {
            applicationItem.insert(QStringLiteral("label"), QStringLiteral("Application identity unavailable"));
        }
        applications.push_back(applicationItem);

        for (const kwm::profiles::Window &window : application.windows)
        {
            QVariantMap windowItem;
            windowItem.insert(QStringLiteral("identity"),
                              applicationIdentityLabel(window.match.applicationIdentity));
            windowItem.insert(QStringLiteral("role"), window.match.role.value_or(QString{}));
            windowItem.insert(QStringLiteral("geometryAvailable"), window.geometry.has_value());
            windowItem.insert(QStringLiteral("minimized"), window.state.minimized);
            windowItem.insert(QStringLiteral("maximizeMode"),
                              kwm::profiles::windowMaximizeModeToString(window.state.maximizeMode));
            windowItem.insert(QStringLiteral("fullscreen"), window.state.fullscreen);

            QStringList memberships;
            if (window.desktopMembership.mode == kwm::profiles::DesktopMembershipMode::All)
            {
                memberships.push_back(QStringLiteral("All desktops"));
            }
            else
            {
                for (const QUuid &desktopId : window.desktopMembership.desktopIds)
                {
                    memberships.push_back(desktopNamesById.value(desktopId, QStringLiteral("Unknown desktop")));
                }
            }
            windowItem.insert(QStringLiteral("desktops"), memberships);
            windows.push_back(windowItem);
        }
    }
    result.insert(QStringLiteral("applications"), applications);
    result.insert(QStringLiteral("windows"), windows);
    return result;
}

bool WorkspaceService::updateSnapshotIdentity(const kwm::profiles::Profile &existing,
                                              kwm::profiles::Profile &captured,
                                              QString &errorMessage)
{
    QSet<QUuid> usedIds;
    collectProfileIds(existing, usedIds);
    collectProfileIds(captured, usedIds);

    captured.id = existing.id;
    captured.name = existing.name;

    QHash<QString, const kwm::profiles::Application *> existingApplicationsByIdentity;
    QSet<QString> ambiguousExistingIdentities;
    for (const kwm::profiles::Application &application : existing.applications)
    {
        QSet<QString> applicationIdentities;
        for (const kwm::profiles::Window &window : application.windows)
        {
            applicationIdentities.insert(applicationIdentityKey(window.match.applicationIdentity));
        }

        for (const QString &identity : applicationIdentities)
        {
            const auto existingApplication = existingApplicationsByIdentity.constFind(identity);
            if (existingApplication == existingApplicationsByIdentity.cend())
            {
                existingApplicationsByIdentity.insert(identity, &application);
            }
            else if (existingApplication.value() != &application)
            {
                ambiguousExistingIdentities.insert(identity);
            }
        }
    }

    QSet<QString> capturedIdentities;
    QHash<const kwm::profiles::Application *, qsizetype> mergedApplicationIndexes;
    QVector<kwm::profiles::Application> mergedApplications;
    mergedApplications.reserve(captured.applications.size());

    for (kwm::profiles::Application &application : captured.applications)
    {
        QSet<QString> applicationIdentities;
        for (const kwm::profiles::Window &window : application.windows)
        {
            applicationIdentities.insert(applicationIdentityKey(window.match.applicationIdentity));
        }

        const kwm::profiles::Application *matchedExistingApplication = nullptr;
        for (const QString &identity : applicationIdentities)
        {
            if (capturedIdentities.contains(identity))
            {
                errorMessage = QStringLiteral("The captured profile contains multiple applications "
                                              "with the same identity.");
                return false;
            }
            capturedIdentities.insert(identity);

            if (ambiguousExistingIdentities.contains(identity))
            {
                errorMessage = QStringLiteral("Cannot safely preserve launch information because an application "
                                              "identity matches multiple existing applications.");
                return false;
            }

            const auto existingApplication = existingApplicationsByIdentity.constFind(identity);
            if (existingApplication == existingApplicationsByIdentity.cend())
            {
                continue;
            }
            if (matchedExistingApplication && matchedExistingApplication != existingApplication.value())
            {
                errorMessage = QStringLiteral("Cannot safely preserve launch information because a captured "
                                              "application matches multiple existing applications.");
                return false;
            }
            matchedExistingApplication = existingApplication.value();
        }

        if (!matchedExistingApplication)
        {
            mergedApplications.push_back(std::move(application));
            continue;
        }

        const auto mergedIndex = mergedApplicationIndexes.constFind(matchedExistingApplication);
        if (mergedIndex == mergedApplicationIndexes.cend())
        {
            if (matchedExistingApplication->launch.has_value() && !application.launch.has_value())
            {
                application.launch = matchedExistingApplication->launch;
            }
            const qsizetype index = mergedApplications.size();
            mergedApplications.push_back(std::move(application));
            mergedApplicationIndexes.insert(matchedExistingApplication, index);
            continue;
        }

        kwm::profiles::Application &mergedApplication = mergedApplications[static_cast<int>(mergedIndex.value())];
        if (!mergedApplication.launch.has_value() && application.launch.has_value())
        {
            mergedApplication.launch = application.launch;
        }
        mergedApplication.windows += std::move(application.windows);
    }

    captured.applications = std::move(mergedApplications);

    QHash<QUuid, QUuid> desktopIdMap;
    QVector<kwm::profiles::VirtualDesktop> updatedDesktops;
    updatedDesktops.reserve(captured.virtualDesktops.size());

    for (qsizetype index = 0; index < captured.virtualDesktops.size(); ++index)
    {
        const kwm::profiles::VirtualDesktop &capturedDesktop = captured.virtualDesktops.at(index);
        const QUuid logicalId = index < existing.virtualDesktops.size()
            ? existing.virtualDesktops.at(index).id
            : createUniqueId(usedIds);
        desktopIdMap.insert(capturedDesktop.id, logicalId);
        updatedDesktops.push_back({logicalId, capturedDesktop.name});
    }
    captured.virtualDesktops = std::move(updatedDesktops);

    if (captured.activeDesktopId.has_value())
    {
        const auto activeDesktop = desktopIdMap.constFind(*captured.activeDesktopId);
        if (activeDesktop == desktopIdMap.cend())
        {
            errorMessage = QStringLiteral("The captured active desktop is not part of the snapshot.");
            return false;
        }
        captured.activeDesktopId = activeDesktop.value();
    }

    QSet<QUuid> assignedIds;
    assignedIds.insert(captured.id);
    for (const kwm::profiles::VirtualDesktop &desktop : captured.virtualDesktops)
    {
        if (assignedIds.contains(desktop.id))
        {
            errorMessage = QStringLiteral("The updated profile contains a duplicate desktop identifier.");
            return false;
        }
        assignedIds.insert(desktop.id);
    }

    for (kwm::profiles::Application &application : captured.applications)
    {
        if (application.id.isNull() || assignedIds.contains(application.id))
        {
            application.id = createUniqueId(assignedIds);
        }
        else
        {
            assignedIds.insert(application.id);
        }

        for (kwm::profiles::Window &window : application.windows)
        {
            if (window.id.isNull() || assignedIds.contains(window.id))
            {
                window.id = createUniqueId(assignedIds);
            }
            else
            {
                assignedIds.insert(window.id);
            }

            if (window.desktopMembership.mode != kwm::profiles::DesktopMembershipMode::Specific)
            {
                continue;
            }

            for (QUuid &desktopId : window.desktopMembership.desktopIds)
            {
                const auto mappedId = desktopIdMap.constFind(desktopId);
                if (mappedId == desktopIdMap.cend())
                {
                    errorMessage = QStringLiteral("A captured window references a desktop outside the snapshot.");
                    return false;
                }
                desktopId = mappedId.value();
            }
        }
    }

    return true;
}

bool WorkspaceService::duplicateWithNewIdentity(const kwm::profiles::Profile &source,
                                                kwm::profiles::Profile &duplicate,
                                                QString &errorMessage)
{
    QSet<QUuid> usedIds;
    collectProfileIds(source, usedIds);

    duplicate = source;
    duplicate.id = createUniqueId(usedIds);
    duplicate.name = QStringLiteral("%1 (Copy)").arg(source.name);

    QHash<QUuid, QUuid> desktopIdMap;
    for (kwm::profiles::VirtualDesktop &desktop : duplicate.virtualDesktops)
    {
        const QUuid oldId = desktop.id;
        desktop.id = createUniqueId(usedIds);
        desktopIdMap.insert(oldId, desktop.id);
    }

    if (duplicate.activeDesktopId.has_value())
    {
        const auto mappedId = desktopIdMap.constFind(*duplicate.activeDesktopId);
        if (mappedId == desktopIdMap.cend())
        {
            errorMessage = QStringLiteral("The source profile has an invalid active desktop reference.");
            return false;
        }
        duplicate.activeDesktopId = mappedId.value();
    }

    for (kwm::profiles::Application &application : duplicate.applications)
    {
        application.id = createUniqueId(usedIds);
        for (kwm::profiles::Window &window : application.windows)
        {
            window.id = createUniqueId(usedIds);
            if (window.desktopMembership.mode != kwm::profiles::DesktopMembershipMode::Specific)
            {
                continue;
            }

            for (QUuid &desktopId : window.desktopMembership.desktopIds)
            {
                const auto mappedId = desktopIdMap.constFind(desktopId);
                if (mappedId == desktopIdMap.cend())
                {
                    errorMessage = QStringLiteral("The source profile contains an invalid window desktop reference.");
                    return false;
                }
                desktopId = mappedId.value();
            }
        }
    }

    return true;
}

} // namespace kwm::workspaces
