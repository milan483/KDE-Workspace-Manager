#include "kwin/kwin_capture_client.h"
#include "kwin/kwin_script_controller.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QTimer>

#include <cstdio>

using namespace kwm::kwin;
using kwm::workspaces::WorkspaceCaptureSnapshot;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2)
    {
        std::fprintf(stderr, "Expected path to the installed KWin script.\n");
        return 64;
    }

    auto *client = new KWinCaptureClient(&app);
    if (!client->isBridgeReady())
    {
        std::fprintf(stderr, "KWin bridge registration failed: %s\n",
                     client->bridgeRegistrationError().toUtf8().constData());
        return 2;
    }

    int exitCode = 1;
    QObject::connect(client, &KWinCaptureClient::started, &app, [&]() {
        const QString requestId = client->capture(QStringLiteral("KWin Integration Test"));
        if (requestId.isEmpty())
        {
            std::fprintf(stderr, "KWin capture request could not be queued.\n");
            exitCode = 3;
            client->stop();
            return;
        }
        std::fprintf(stderr, "KWin capture request queued: %s\n", requestId.toUtf8().constData());
    });

    QObject::connect(client, &KWinCaptureClient::captureCompleted, &app,
                     [&](const QString &requestId, const WorkspaceCaptureSnapshot &snapshot) {
        int windowCount = 0;
        for (const auto &application : snapshot.profile.applications)
        {
            windowCount += application.windows.size();
        }
        std::fprintf(stderr,
                     "KWin capture passed: request=%s desktops=%lld applications=%lld windows=%d "
                     "excluded=%d geometry_omitted=%d state_geometry_unavailable=%d\n",
                     requestId.toUtf8().constData(),
                     static_cast<long long>(snapshot.profile.virtualDesktops.size()),
                     static_cast<long long>(snapshot.profile.applications.size()), windowCount,
                     snapshot.excludedWindowCount, snapshot.geometryOmittedCount,
                     snapshot.stateGeometryUnavailableCount);
        exitCode = 0;
        client->deleteLater();
        QTimer::singleShot(250, &app, [&]() {
            QDBusMessage message = QDBusMessage::createMethodCall(
                QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                QStringLiteral("org.kde.kwin.Scripting"), QStringLiteral("isScriptLoaded"));
            message << KWinScriptController::pluginName();
            const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
            if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty() ||
                reply.arguments().constFirst().toBool())
            {
                std::fprintf(stderr, "KWin capture script remained loaded after client destruction.\n");
                exitCode = 7;
            }
            else
            {
                std::fprintf(stderr, "KWin capture script unloaded when the client was destroyed.\n");
            }
            app.exit(exitCode);
        });
    });

    QObject::connect(client, &KWinCaptureClient::captureFailed, &app,
                     [&](const QString &requestId, const QString &path, const QString &message) {
        std::fprintf(stderr, "KWin capture failed: request=%s path=%s message=%s\n",
                     requestId.toUtf8().constData(), path.toUtf8().constData(), message.toUtf8().constData());
        exitCode = 4;
        client->stop();
    });

    QObject::connect(client, &KWinCaptureClient::failed, &app, [&](const QString &message) {
        std::fprintf(stderr, "KWin script controller failed: %s\n", message.toUtf8().constData());
        exitCode = 5;
        app.exit(exitCode);
    });
    QObject::connect(client, &KWinCaptureClient::stopped, &app, [&]() {
        app.exit(exitCode);
    });

    QTimer::singleShot(20000, &app, [&]() {
        std::fprintf(stderr, "KWin capture integration test timed out.\n");
        exitCode = 6;
        client->stop();
        if (!client->isRunning())
        {
            app.exit(exitCode);
        }
    });

    client->start(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}
