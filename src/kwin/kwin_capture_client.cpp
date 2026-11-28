#include "kwin/kwin_capture_client.h"

#include "kwin/kwin_bridge_service.h"
#include "kwin/kwin_request_queue.h"
#include "kwin/kwin_script_controller.h"

#include <QMetaType>

namespace
{

constexpr int CaptureTimeoutMs = 15000;

} // namespace
#include <QStandardPaths>

namespace kwm::kwin
{

KWinCaptureClient::KWinCaptureClient(QObject *parent)
    : QObject(parent)
    , m_queue(new KWinRequestQueue(this))
    , m_bridge(new KWinBridgeService(m_queue, this))
    , m_scriptController(new KWinScriptController(this))
{
    qRegisterMetaType<KWinCaptureSnapshot>();

    connect(m_queue, &KWinRequestQueue::requestCompleted, this,
            &KWinCaptureClient::handleResponse);
    connect(m_scriptController, &KWinScriptController::started, this,
            &KWinCaptureClient::started);
    m_captureTimeout.setSingleShot(true);
    m_captureTimeout.setInterval(CaptureTimeoutMs);
    connect(&m_captureTimeout, &QTimer::timeout, this, &KWinCaptureClient::handleCaptureTimeout);

    connect(m_scriptController, &KWinScriptController::stopped, this, [this]() {
        failPendingCapture(QStringLiteral("state"),
                           QStringLiteral("The KWin capture script stopped before returning the snapshot."));
        Q_EMIT stopped();
    });
    connect(m_scriptController, &KWinScriptController::failed, this, [this](const QString &message) {
        failPendingCapture(QStringLiteral("bridge"),
                           QStringLiteral("The KWin capture script failed before returning the snapshot: %1").arg(message));
        Q_EMIT failed(message);
    });
}

QString KWinCaptureClient::installedScriptPath()
{
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  QStringLiteral("kwin/scripts/kde-workspace-manager/contents/code/main.js"));
}

bool KWinCaptureClient::isBridgeReady() const
{
    return m_bridge->isRegistered();
}

bool KWinCaptureClient::isRunning() const
{
    return m_scriptController->isRunning();
}

QString KWinCaptureClient::bridgeRegistrationError() const
{
    return m_bridge->registrationError();
}

void KWinCaptureClient::start(const QString &scriptPath)
{
    if (!m_bridge->isRegistered())
    {
        Q_EMIT failed(m_bridge->registrationError());
        return;
    }
    const QString resolvedScriptPath = scriptPath.isEmpty() ? installedScriptPath() : scriptPath;
    if (resolvedScriptPath.isEmpty())
    {
        Q_EMIT failed(QStringLiteral("The installed KWin capture script could not be located."));
        return;
    }
    m_scriptController->start(resolvedScriptPath);
}

void KWinCaptureClient::stop()
{
    failPendingCapture(QStringLiteral("state"),
                       QStringLiteral("The KWin capture request was cancelled because the script was stopped."));
    m_scriptController->stop();
}

QString KWinCaptureClient::capture(const QString &profileName)
{
    if (!m_bridge->isRegistered())
    {
        Q_EMIT captureFailed({}, QStringLiteral("bridge"), m_bridge->registrationError());
        return {};
    }
    if (!m_scriptController->isRunning())
    {
        Q_EMIT captureFailed({}, QStringLiteral("state"),
                             QStringLiteral("The KWin capture script is not running."));
        return {};
    }
    if (profileName.trimmed().isEmpty())
    {
        Q_EMIT captureFailed({}, QStringLiteral("profile_name"),
                             QStringLiteral("Profile name must not be empty."));
        return {};
    }
    if (!m_pendingProfileNames.isEmpty())
    {
        Q_EMIT captureFailed({}, QStringLiteral("state"),
                             QStringLiteral("A KWin capture request is already in progress."));
        return {};
    }

    const QString requestId = m_queue->enqueueCaptureRequest();
    m_pendingProfileNames.insert(requestId, profileName);
    m_captureTimeout.start();
    return requestId;
}

void KWinCaptureClient::handleResponse(const QString &requestId, const QString &responseJson)
{
    const auto profileName = m_pendingProfileNames.find(requestId);
    if (profileName == m_pendingProfileNames.end())
    {
        return;
    }
    const QString name = profileName.value();
    m_pendingProfileNames.erase(profileName);
    m_captureTimeout.stop();

    const auto result = KWinSnapshotParser::parse(responseJson.toUtf8(), requestId, name);
    if (const auto *error = std::get_if<profiles::ProfileError>(&result))
    {
        Q_EMIT captureFailed(requestId, error->path, error->message);
        return;
    }

    Q_EMIT captureCompleted(requestId, std::get<KWinCaptureSnapshot>(result));
}

void KWinCaptureClient::handleCaptureTimeout()
{
    failPendingCapture(QStringLiteral("timeout"),
                       QStringLiteral("Timed out waiting for KWin to return the capture snapshot."));
}

void KWinCaptureClient::failPendingCapture(const QString &path, const QString &message)
{
    if (m_pendingProfileNames.isEmpty())
    {
        return;
    }

    const QString requestId = m_pendingProfileNames.constBegin().key();
    const bool requestCancelled = m_queue->cancelRequest(requestId);
    Q_UNUSED(requestCancelled)
    m_pendingProfileNames.remove(requestId);
    m_captureTimeout.stop();
    Q_EMIT captureFailed(requestId, path, message);
}

} // namespace kwm::kwin
