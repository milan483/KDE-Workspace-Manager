#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

namespace kwm::kwin
{

class KWinRequestQueue;

class KWinBridgeService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeworkspacemanager.KWinBridge1")

    public:
    static QString serviceName();
    static QString objectPath();
    static QString interfaceName();

    explicit KWinBridgeService(KWinRequestQueue *queue, QObject *parent = nullptr);
    ~KWinBridgeService() override;

    [[nodiscard]] bool isRegistered() const;
    [[nodiscard]] QString registrationError() const;

    public Q_SLOTS:
    QString takeNextRequest();
    bool completeRequest(const QString &requestId, const QString &responseJson);

    private:
    QDBusConnection m_connection;
    KWinRequestQueue *m_queue = nullptr;
    bool m_serviceRegistered = false;
    bool m_objectRegistered = false;
    QString m_registrationError;
};

} // namespace kwm::kwin
