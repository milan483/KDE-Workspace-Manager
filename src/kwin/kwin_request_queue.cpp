#include "kwin/kwin_request_queue.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace kwm::kwin
{

KWinRequestQueue::KWinRequestQueue(QObject *parent)
    : QObject(parent)
{
}

QString KWinRequestQueue::enqueueCaptureRequest()
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject request{{QStringLiteral("protocol_version"), ProtocolVersion},
                              {QStringLiteral("request_id"), requestId},
                              {QStringLiteral("operation"), QStringLiteral("capture")}};
    m_requests.enqueue({requestId, QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact))});
    return requestId;
}

QString KWinRequestQueue::takeNextRequest()
{
    if (!m_activeRequest.has_value())
    {
        if (m_requests.isEmpty())
        {
            return {};
        }
        m_activeRequest = m_requests.dequeue();
    }

    return m_activeRequest->json;
}

bool KWinRequestQueue::completeRequest(const QString &requestId, const QString &responseJson)
{
    if (!m_activeRequest.has_value() || requestId != m_activeRequest->id)
    {
        return false;
    }

    m_activeRequest.reset();
    Q_EMIT requestCompleted(requestId, responseJson);
    return true;
}

bool KWinRequestQueue::cancelRequest(const QString &requestId)
{
    if (m_activeRequest.has_value() && m_activeRequest->id == requestId)
    {
        m_activeRequest.reset();
        return true;
    }

    for (qsizetype index = 0; index < m_requests.size(); ++index)
    {
        if (m_requests.at(index).id == requestId)
        {
            m_requests.removeAt(index);
            return true;
        }
    }

    return false;
}

int KWinRequestQueue::pendingRequestCount() const
{
    return m_requests.size() + (m_activeRequest.has_value() ? 1 : 0);
}

bool KWinRequestQueue::hasPendingRequests() const
{
    return m_activeRequest.has_value() || !m_requests.isEmpty();
}

} // namespace kwm::kwin
