#pragma once

#include "workspaces/workspace_capture_snapshot.h"

#include <QObject>
#include <QString>

namespace kwm::workspaces
{

class WorkspaceCaptureBackend : public QObject
{
    Q_OBJECT

    public:
    explicit WorkspaceCaptureBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~WorkspaceCaptureBackend() override = default;

    [[nodiscard]] virtual bool isRunning() const = 0;

    public Q_SLOTS:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual QString capture(const QString &profileName) = 0;

    Q_SIGNALS:
    void started();
    void stopped();
    void failed(const QString &message);
    void captureCompleted(const QString &requestId, const WorkspaceCaptureSnapshot &snapshot);
    void captureFailed(const QString &requestId, const QString &path, const QString &message);
};

} // namespace kwm::workspaces
