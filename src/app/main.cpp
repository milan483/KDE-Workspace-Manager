#include "core/build_info.h"

#include <KAboutData>
#include <KIconTheme>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QString>
#include <QVariant>

#include <cstdlib>

namespace
{

[[nodiscard]] QString currentPageTitle(const QObject *pageStack)
{
    const auto currentPage = pageStack->property("currentItem").value<QObject *>();
    return currentPage ? currentPage->property("title").toString() : QString{};
}

[[nodiscard]] bool verifyNavigation(QObject *rootObject)
{
    auto *pageStack = rootObject->findChild<QObject *>(QStringLiteral("mainPageStack"));
    auto *workspacesAction = rootObject->findChild<QObject *>(QStringLiteral("workspacesNavigationAction"));
    auto *settingsAction = rootObject->findChild<QObject *>(QStringLiteral("settingsNavigationAction"));

    if (!pageStack || !workspacesAction || !settingsAction) {
        qCritical() << "The main window is missing its page stack or navigation actions.";
        return false;
    }

    if (currentPageTitle(pageStack) != QStringLiteral("Workspaces")) {
        qCritical() << "The application did not open on the Workspaces page.";
        return false;
    }

    if (!QMetaObject::invokeMethod(settingsAction, "trigger", Qt::DirectConnection)) {
        qCritical() << "Could not trigger the Settings navigation action.";
        return false;
    }
    QCoreApplication::processEvents();

    if (currentPageTitle(pageStack) != QStringLiteral("Settings")) {
        qCritical() << "The Settings navigation action did not select the Settings page.";
        return false;
    }

    if (!QMetaObject::invokeMethod(workspacesAction, "trigger", Qt::DirectConnection)) {
        qCritical() << "Could not trigger the Workspaces navigation action.";
        return false;
    }
    QCoreApplication::processEvents();

    if (currentPageTitle(pageStack) != QStringLiteral("Workspaces")) {
        qCritical() << "The Workspaces navigation action did not return to the Workspaces page.";
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    KIconTheme::initTheme();

    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kde-workspace-manager"));

    KAboutData aboutData(
        QStringLiteral("kde-workspace-manager"),
        i18n("KDE Workspace Manager"),
        kwm::core::applicationVersion(),
        i18n("Workspace profile manager for KDE Plasma"),
        KAboutLicense::Unknown
    );
    aboutData.setDesktopFileName(QStringLiteral("org.kde.kdeworkspacemanager"));
    KAboutData::setApplicationData(aboutData);
    QApplication::setDesktopFileName(QStringLiteral("org.kde.kdeworkspacemanager"));

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption smokeTestOption(
        QStringLiteral("smoke-test"),
        i18n("Load the application interface, verify navigation, and exit.")
    );
    parser.addOption(smokeTestOption);
    parser.process(app);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("org.kde.kdeworkspacemanager", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load the application interface.";
        return EXIT_FAILURE;
    }

    if (parser.isSet(smokeTestOption)) {
        auto *rootObject = engine.rootObjects().constFirst();
        if (rootObject->objectName() != QStringLiteral("mainWindow") || !verifyNavigation(rootObject)) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    return app.exec();
}
