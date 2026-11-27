#include "core/build_info.h"

#include <QtTest/QtTest>

class BuildInfoTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void applicationNameIsStable();
    void applicationVersionMatchesProjectVersion();
};

void BuildInfoTest::applicationNameIsStable()
{
    QCOMPARE(kwm::core::applicationName(), QStringLiteral("KDE Workspace Manager"));
}

void BuildInfoTest::applicationVersionMatchesProjectVersion()
{
    QCOMPARE(kwm::core::applicationVersion(), QStringLiteral("0.1.0"));
}

QTEST_APPLESS_MAIN(BuildInfoTest)

#include "test_build_info.moc"
