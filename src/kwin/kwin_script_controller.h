#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

namespace kwm::kwin
{

class KWinScriptController final : public QObject
{
    Q_OBJECT

    public:
    static QString pluginName();

    explicit KWinScriptController(QObject *parent = nullptr);

    [[nodiscard]] bool isStarting() const;
    [[nodiscard]] bool isRunning() const;

    public Q_SLOTS:
    void start(const QString &scriptPath);
    void stop();

    Q_SIGNALS:
    void started();
    void stopped();
    void failed(const QString &message);

    private:
    void unloadExistingScript();
    void verifyExistingScriptUnloaded();
    void loadScript();
    void runScript(int scriptId);
    void fail(const QString &message);

    QDBusConnection m_connection;
    QString m_scriptPath;
    int m_scriptId = -1;
    bool m_starting = false;
    bool m_running = false;
    bool m_stopping = false;
    bool m_stopRequested = false;
    int m_unloadAttempts = 0;
};

} // namespace kwm::kwin
