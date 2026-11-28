#include "kwin/kwin_bridge_service.h"

#include "kwin/kwin_request_queue.h"

#include <QDBusError>

namespace kwm::kwin
{

QString KWinBridgeService::serviceName()
{
    return QStringLiteral("org.kde.kdeworkspacemanager.KWinBridge");
}

QString KWinBridgeService::objectPath()
{
    return QStringLiteral("/org/kde/kdeworkspacemanager/KWinBridge");
}

QString KWinBridgeService::interfaceName()
{
    return QStringLiteral("org.kde.kdeworkspacemanager.KWinBridge1");
}

KWinBridgeService::KWinBridgeService(KWinRequestQueue *queue, QObject *parent)
    : QObject(parent)
    , m_connection(QDBusConnection::sessionBus())
    , m_queue(queue)
{
    if (!m_queue)
    {
        m_registrationError = QStringLiteral("A request queue is required.");
        return;
    }

    if (!m_connection.isConnected())
    {
        m_registrationError = QStringLiteral("The session D-Bus connection is unavailable.");
        return;
    }

    if (!m_connection.registerService(serviceName()))
    {
        m_registrationError = m_connection.lastError().message();
        return;
    }
    m_serviceRegistered = true;

    if (!m_connection.registerObject(objectPath(), this, QDBusConnection::ExportAllSlots))
    {
        m_registrationError = m_connection.lastError().message();
        m_connection.unregisterService(serviceName());
        m_serviceRegistered = false;
        return;
    }
    m_objectRegistered = true;
}

KWinBridgeService::~KWinBridgeService()
{
    if (m_objectRegistered)
    {
        m_connection.unregisterObject(objectPath());
    }
    if (m_serviceRegistered)
    {
        m_connection.unregisterService(serviceName());
    }
}

bool KWinBridgeService::isRegistered() const
{
    return m_serviceRegistered && m_objectRegistered;
}

QString KWinBridgeService::registrationError() const
{
    return m_registrationError;
}

QString KWinBridgeService::takeNextRequest()
{
    return m_queue ? m_queue->takeNextRequest() : QString{};
}

bool KWinBridgeService::completeRequest(const QString &requestId, const QString &responseJson)
{
    return m_queue && m_queue->completeRequest(requestId, responseJson);
}

} // namespace kwm::kwin
