#pragma once

#include "workspaces/workspace_capture_backend.h"

#include <QString>
#include <QTimer>

#include <QHash>

namespace kwm::kwin
{

class KWinBridgeService;
class KWinRequestQueue;
class KWinScriptController;

class KWinCaptureClient final : public kwm::workspaces::WorkspaceCaptureBackend
{
    Q_OBJECT

    public:
    explicit KWinCaptureClient(QObject *parent = nullptr);

    [[nodiscard]] static QString installedScriptPath();
    [[nodiscard]] bool isBridgeReady() const;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] QString bridgeRegistrationError() const;
    void start(const QString &scriptPath);

    public Q_SLOTS:
    void start() override;
    void stop() override;
    QString capture(const QString &profileName) override;

    private:
    void handleResponse(const QString &requestId, const QString &responseJson);
    void handleCaptureTimeout();
    void failPendingCapture(const QString &path, const QString &message);

    KWinRequestQueue *m_queue = nullptr;
    KWinBridgeService *m_bridge = nullptr;
    KWinScriptController *m_scriptController = nullptr;
    QHash<QString, QString> m_pendingProfileNames;
    QTimer m_captureTimeout;
};

} // namespace kwm::kwin
