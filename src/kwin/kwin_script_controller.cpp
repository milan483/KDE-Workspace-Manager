#include "kwin/kwin_script_controller.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QTimer>

namespace kwm::kwin
{
namespace
{

constexpr int DbusTimeoutMs = 5000;
const QString KWinService = QStringLiteral("org.kde.KWin");
const QString ScriptingPath = QStringLiteral("/Scripting");
const QString ScriptingInterface = QStringLiteral("org.kde.kwin.Scripting");
const QString ScriptInterface = QStringLiteral("org.kde.kwin.Script");

QDBusMessage methodCall(const QString &path, const QString &interface, const QString &method,
                        const QVariantList &arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(KWinService, path, interface, method);
    message.setArguments(arguments);
    return message;
}

} // namespace

QString KWinScriptController::pluginName()
{
    return QStringLiteral("org.kde.kdeworkspacemanager.capture");
}

KWinScriptController::KWinScriptController(QObject *parent)
    : QObject(parent)
    , m_connection(QDBusConnection::sessionBus())
{
}

KWinScriptController::~KWinScriptController()
{
    // KWin scripts outlive this process unless the plugin is explicitly unloaded.
    if (!m_connection.isConnected() || (!m_starting && !m_running && m_scriptId < 0))
    {
        return;
    }

    QDBusMessage message = methodCall(ScriptingPath, ScriptingInterface, QStringLiteral("unloadScript"),
                                      {pluginName()});
    m_connection.send(message);
}

bool KWinScriptController::isStarting() const
{
    return m_starting;
}

bool KWinScriptController::isRunning() const
{
    return m_running;
}

void KWinScriptController::start(const QString &scriptPath)
{
    if (m_starting || m_running || m_stopping)
    {
        return;
    }
    if (!m_connection.isConnected())
    {
        fail(QStringLiteral("The session D-Bus connection is unavailable."));
        return;
    }
    if (!QFileInfo(scriptPath).isFile())
    {
        fail(QStringLiteral("The KWin script file does not exist: %1").arg(scriptPath));
        return;
    }

    m_scriptPath = scriptPath;
    m_starting = true;

    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(ScriptingPath, ScriptingInterface, QStringLiteral("isScriptLoaded"),
                                          {pluginName()}),
                               DbusTimeoutMs),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *finished) {
        Q_UNUSED(finished)
        const QDBusPendingReply<bool> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
        {
            fail(reply.error().message());
            return;
        }
        if (reply.value())
        {
            unloadExistingScript();
            return;
        }
        loadScript();
    });
}

void KWinScriptController::unloadExistingScript()
{
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(ScriptingPath, ScriptingInterface, QStringLiteral("unloadScript"),
                                          {pluginName()}),
                               DbusTimeoutMs),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *finished) {
        Q_UNUSED(finished)
        const QDBusPendingReply<bool> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
        {
            fail(reply.error().message());
            return;
        }
        m_unloadAttempts = 0;
        QTimer::singleShot(100, this, &KWinScriptController::verifyExistingScriptUnloaded);
    });
}

void KWinScriptController::verifyExistingScriptUnloaded()
{
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(ScriptingPath, ScriptingInterface, QStringLiteral("isScriptLoaded"),
                                          {pluginName()}),
                               DbusTimeoutMs),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *finished) {
        Q_UNUSED(finished)
        const QDBusPendingReply<bool> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
        {
            fail(reply.error().message());
            return;
        }
        if (!reply.value())
        {
            loadScript();
            return;
        }
        if (++m_unloadAttempts >= 10)
        {
            fail(QStringLiteral("The previous KDE Workspace Manager KWin script could not be unloaded."));
            return;
        }
        QTimer::singleShot(100, this, &KWinScriptController::verifyExistingScriptUnloaded);
    });
}

void KWinScriptController::loadScript()
{
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(ScriptingPath, ScriptingInterface, QStringLiteral("loadScript"),
                                          {m_scriptPath, pluginName()}),
                               DbusTimeoutMs),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *finished) {
        Q_UNUSED(finished)
        const QDBusPendingReply<int> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
        {
            fail(reply.error().message());
            return;
        }

        m_scriptId = reply.value();
        if (m_scriptId < 0)
        {
            fail(QStringLiteral("KWin refused to load the capture script; its plugin ID may already be registered."));
            return;
        }
        runScript(m_scriptId);
    });
}

void KWinScriptController::runScript(int scriptId)
{
    const QString path = QStringLiteral("/Scripting/Script%1").arg(scriptId);
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(path, ScriptInterface, QStringLiteral("run")), DbusTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *finished) {
        Q_UNUSED(finished)
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
        {
            fail(reply.error().message());
            return;
        }

        m_starting = false;
        m_running = true;
        Q_EMIT started();
        if (m_stopRequested)
        {
            m_stopRequested = false;
            stop();
        }
    });
}

void KWinScriptController::stop()
{
    if (m_starting)
    {
        m_stopRequested = true;
        return;
    }
    if (!m_running || m_stopping)
    {
        return;
    }

    m_stopping = true;
    auto *watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(ScriptingPath, ScriptingInterface, QStringLiteral("unloadScript"),
                                          {pluginName()}),
                               DbusTimeoutMs),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *finished) {
        Q_UNUSED(finished)
        const QDBusPendingReply<bool> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
        {
            m_stopping = false;
            Q_EMIT failed(reply.error().message());
            return;
        }

        m_scriptId = -1;
        m_running = false;
        m_stopping = false;
        Q_EMIT stopped();
    });
}

void KWinScriptController::fail(const QString &message)
{
    m_starting = false;
    m_stopRequested = false;
    if (m_scriptId >= 0)
    {
        m_connection.asyncCall(methodCall(ScriptingPath, ScriptingInterface, QStringLiteral("unloadScript"),
                                          {pluginName()}),
                               DbusTimeoutMs);
        m_scriptId = -1;
    }
    Q_EMIT failed(message);
}

} // namespace kwm::kwin
