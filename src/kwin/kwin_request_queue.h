#pragma once

#include <QObject>
#include <QQueue>
#include <QString>

#include <optional>

namespace kwm::kwin
{

class KWinRequestQueue final : public QObject
{
    Q_OBJECT

    public:
    static constexpr int ProtocolVersion = 1;

    explicit KWinRequestQueue(QObject *parent = nullptr);

    [[nodiscard]] QString enqueueCaptureRequest();
    [[nodiscard]] QString takeNextRequest();
    [[nodiscard]] bool completeRequest(const QString &requestId, const QString &responseJson);
    [[nodiscard]] bool cancelRequest(const QString &requestId);
    [[nodiscard]] int pendingRequestCount() const;
    [[nodiscard]] bool hasPendingRequests() const;

    Q_SIGNALS:
    void requestCompleted(const QString &requestId, const QString &responseJson);

    private:
    struct Request
    {
        QString id;
        QString json;
    };

    QQueue<Request> m_requests;
    std::optional<Request> m_activeRequest;
};

} // namespace kwm::kwin
