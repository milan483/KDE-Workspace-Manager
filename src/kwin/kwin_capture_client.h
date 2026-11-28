#pragma once

#include "kwin/kwin_snapshot_parser.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <QHash>

namespace kwm::kwin
{

class KWinBridgeService;
class KWinRequestQueue;
class KWinScriptController;

class KWinCaptureClient final : public QObject
{
    Q_OBJECT

    public:
    explicit KWinCaptureClient(QObject *parent = nullptr);

    [[nodiscard]] static QString installedScriptPath();
    [[nodiscard]] bool isBridgeReady() const;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QString bridgeRegistrationError() const;

    public Q_SLOTS:
    void start(const QString &scriptPath = {});
    void stop();
    QString capture(const QString &profileName);

    Q_SIGNALS:
    void started();
    void stopped();
    void failed(const QString &message);
    void captureCompleted(const QString &requestId, const kwm::kwin::KWinCaptureSnapshot &snapshot);
    void captureFailed(const QString &requestId, const QString &path, const QString &message);

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

Q_DECLARE_METATYPE(kwm::kwin::KWinCaptureSnapshot)
