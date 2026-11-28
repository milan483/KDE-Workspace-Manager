#pragma once

#include "profiles/profile_repository.h"
#include "workspaces/profile_list_model.h"
#include "workspaces/workspace_capture_backend.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace kwm::workspaces
{

class WorkspaceService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ProfileListModel *profileModel READ profileModel CONSTANT)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString warningMessage READ warningMessage NOTIFY warningMessageChanged)
    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY previewChanged)
    Q_PROPERTY(QVariantMap preview READ preview NOTIFY previewChanged)
    Q_PROPERTY(bool previewIsUpdate READ previewIsUpdate NOTIFY previewChanged)

    public:
    WorkspaceService(kwm::profiles::ProfileRepository repository,
                     WorkspaceCaptureBackend *captureBackend,
                     QObject *parent = nullptr);

    [[nodiscard]] ProfileListModel *profileModel() const;
    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString warningMessage() const;
    [[nodiscard]] bool hasPreview() const;
    [[nodiscard]] QVariantMap preview() const;
    [[nodiscard]] bool previewIsUpdate() const;

    Q_INVOKABLE void refreshProfiles();
    Q_INVOKABLE void saveWorkspace(const QString &name);
    Q_INVOKABLE void updateWorkspace(const QString &profileId);
    Q_INVOKABLE bool confirmSavePreview();
    Q_INVOKABLE void cancelPreview();

    Q_INVOKABLE bool renameProfile(const QString &profileId, const QString &newName);
    Q_INVOKABLE bool duplicateProfile(const QString &profileId);
    Q_INVOKABLE bool deleteProfile(const QString &profileId);
    Q_INVOKABLE QVariantMap profileDetails(const QString &profileId);

    Q_SIGNALS:
    void busyChanged();
    void statusMessageChanged();
    void errorMessageChanged();
    void warningMessageChanged();
    void previewChanged();
    void previewReady();

    private:
    void initialize();
    void beginCapture(const QString &name, std::optional<QUuid> updateProfileId);
    void startCaptureRequest();
    void handleCaptureStarted();
    void handleCaptureBackendFailed(const QString &message);
    void handleCaptureCompleted(const QString &requestId, const WorkspaceCaptureSnapshot &snapshot);
    void handleCaptureFailed(const QString &requestId, const QString &path, const QString &message);
    void failCapture(const QString &message);

    [[nodiscard]] bool ensureNoPendingOperation(const QString &operation);
    void setBusy(bool busy);
    void setStatusMessage(const QString &message);
    void setErrorMessage(const QString &message);
    void setWarningMessage(const QString &message);
    void clearPreview();

    [[nodiscard]] static int windowCount(const kwm::profiles::Profile &profile);
    [[nodiscard]] static int applicationsWithoutLaunchCount(const kwm::profiles::Profile &profile);
    [[nodiscard]] static QVariantMap capturePreview(const WorkspaceCaptureSnapshot &snapshot, bool isUpdate);
    [[nodiscard]] static QVariantMap profileSummary(const kwm::profiles::Profile &profile);
    [[nodiscard]] static bool updateSnapshotIdentity(const kwm::profiles::Profile &existing,
                                                     kwm::profiles::Profile &captured,
                                                     QString &errorMessage);
    [[nodiscard]] static bool duplicateWithNewIdentity(const kwm::profiles::Profile &source,
                                                        kwm::profiles::Profile &duplicate,
                                                        QString &errorMessage);

    kwm::profiles::ProfileRepository m_repository;
    WorkspaceCaptureBackend *m_captureBackend = nullptr;
    ProfileListModel *m_profileModel = nullptr;

    bool m_busy = false;
    bool m_waitingForCaptureStart = false;
    QString m_pendingCaptureId;
    QString m_pendingProfileName;
    std::optional<QUuid> m_pendingUpdateProfileId;
    std::optional<WorkspaceCaptureSnapshot> m_previewSnapshot;
    QVariantMap m_previewData;

    QString m_statusMessage;
    QString m_errorMessage;
    QString m_warningMessage;
};

} // namespace kwm::workspaces
