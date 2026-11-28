#include "kwin/kwin_snapshot_parser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

using namespace kwm::kwin;
using namespace kwm::profiles;

class KWinSnapshotParserTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void parsesSnapshotAndMapsRuntimeDesktopIdentifiers();
    void rejectsMalformedJson();
    void rejectsMismatchedRequestIdentifier();
    void rejectsUnsupportedMonitorCount();
    void rejectsUnknownDesktopMembership();
    void rejectsDesktopIdsOnAllDesktopMembership();
    void rejectsUnsupportedMaximizeMode();
    void rejectsConflictingFullscreenAndMaximizeState();
    void rejectsRemoteCaptureErrors();
};

namespace
{

const QString requestId = QStringLiteral("10000000-0000-4000-8000-000000000001");
const QString firstRuntimeDesktopId = QStringLiteral("20000000-0000-4000-8000-000000000001");
const QString secondRuntimeDesktopId = QStringLiteral("20000000-0000-4000-8000-000000000002");

QJsonObject makeWindow(const QString &kind, const QString &value, const QJsonArray &desktopIds,
                       bool allDesktops, const QString &maximizeMode, bool minimized = false,
                       bool fullscreen = false)
{
    QJsonObject window{
        {QStringLiteral("application_identity"),
         QJsonObject{{QStringLiteral("kind"), kind}, {QStringLiteral("value"), value}}},
        {QStringLiteral("all_desktops"), allDesktops},
        {QStringLiteral("state"),
         QJsonObject{{QStringLiteral("minimized"), minimized},
                     {QStringLiteral("maximize_mode"), maximizeMode},
                     {QStringLiteral("fullscreen"), fullscreen}}}};
    if (!allDesktops)
    {
        window.insert(QStringLiteral("desktop_ids"), desktopIds);
    }
    return window;
}

QJsonObject validResponse()
{
    const QJsonArray desktops{
        QJsonObject{{QStringLiteral("id"), firstRuntimeDesktopId}, {QStringLiteral("name"), QStringLiteral("Development")}},
        QJsonObject{{QStringLiteral("id"), secondRuntimeDesktopId}, {QStringLiteral("name"), QStringLiteral("Communication")}}};

    QJsonObject editor = makeWindow(QStringLiteral("desktop_file_name"), QStringLiteral("org.kde.kate"),
                                    QJsonArray{firstRuntimeDesktopId, secondRuntimeDesktopId}, false,
                                    QStringLiteral("vertical"));
    editor.insert(QStringLiteral("role"), QStringLiteral("main"));
    editor.insert(QStringLiteral("geometry"),
                  QJsonObject{{QStringLiteral("x"), 0.05}, {QStringLiteral("y"), 0.05},
                              {QStringLiteral("width"), 0.9}, {QStringLiteral("height"), 0.9}});

    const QJsonObject stickyTerminal = makeWindow(QStringLiteral("resource_class"), QStringLiteral("konsole"),
                                                   {}, true, QStringLiteral("none"), true, true);

    const QJsonObject snapshot{
        {QStringLiteral("screen_count"), 1},
        {QStringLiteral("active_desktop_id"), firstRuntimeDesktopId},
        {QStringLiteral("virtual_desktops"), desktops},
        {QStringLiteral("windows"), QJsonArray{editor, stickyTerminal}},
        {QStringLiteral("excluded_window_count"), 2},
        {QStringLiteral("geometry_omitted_count"), 1},
        {QStringLiteral("state_geometry_unavailable_count"), 1}};

    return {{QStringLiteral("protocol_version"), KWinSnapshotParser::ProtocolVersion},
            {QStringLiteral("request_id"), requestId},
            {QStringLiteral("operation"), QStringLiteral("capture")},
            {QStringLiteral("snapshot"), snapshot}};
}

ProfileResult<KWinCaptureSnapshot> parse(const QJsonObject &response)
{
    return KWinSnapshotParser::parse(QJsonDocument(response).toJson(QJsonDocument::Compact), requestId,
                                     QStringLiteral("Development"));
}

} // namespace

void KWinSnapshotParserTest::parsesSnapshotAndMapsRuntimeDesktopIdentifiers()
{
    const auto result = parse(validResponse());
    QVERIFY(std::holds_alternative<KWinCaptureSnapshot>(result));

    const KWinCaptureSnapshot captured = std::get<KWinCaptureSnapshot>(result);
    const Profile &profile = captured.profile;
    QVERIFY(!profile.id.isNull());
    QCOMPARE(profile.name, QStringLiteral("Development"));
    QCOMPARE(profile.virtualDesktops.size(), 2);
    QVERIFY(profile.activeDesktopId == profile.virtualDesktops[0].id);
    QVERIFY(profile.virtualDesktops[0].id.toString(QUuid::WithoutBraces) != firstRuntimeDesktopId);
    QCOMPARE(profile.applications.size(), 2);

    const Application &editor = profile.applications[0];
    QVERIFY(editor.launch == std::nullopt);
    QCOMPARE(editor.windows.size(), 1);
    QCOMPARE(editor.windows[0].match.applicationIdentity.kind, ApplicationIdentityKind::DesktopFileName);
    QCOMPARE(editor.windows[0].match.applicationIdentity.value, QStringLiteral("org.kde.kate"));
    QVERIFY(editor.windows[0].match.role == QStringLiteral("main"));
    QCOMPARE(editor.windows[0].desktopMembership.mode, DesktopMembershipMode::Specific);
    QCOMPARE(editor.windows[0].desktopMembership.desktopIds,
             QVector<QUuid>({profile.virtualDesktops[0].id, profile.virtualDesktops[1].id}));
    QCOMPARE(editor.windows[0].state.maximizeMode, WindowMaximizeMode::Vertical);
    QVERIFY(editor.windows[0].geometry.has_value());
    QCOMPARE(editor.windows[0].geometry->x, 0.05);

    const Application &terminal = profile.applications[1];
    QCOMPARE(terminal.windows.size(), 1);
    QCOMPARE(terminal.windows[0].match.applicationIdentity.kind, ApplicationIdentityKind::ResourceClass);
    QCOMPARE(terminal.windows[0].desktopMembership.mode, DesktopMembershipMode::All);
    QVERIFY(terminal.windows[0].desktopMembership.desktopIds.isEmpty());
    QVERIFY(terminal.windows[0].state.minimized);
    QVERIFY(terminal.windows[0].state.fullscreen);
    QVERIFY(!terminal.windows[0].geometry.has_value());

    QCOMPARE(captured.excludedWindowCount, 2);
    QCOMPARE(captured.geometryOmittedCount, 1);
    QCOMPARE(captured.stateGeometryUnavailableCount, 1);
}

void KWinSnapshotParserTest::rejectsMalformedJson()
{
    const auto result = KWinSnapshotParser::parse(QByteArrayLiteral("{not-json"), requestId,
                                                   QStringLiteral("Development"));
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidJson);
}

void KWinSnapshotParserTest::rejectsMismatchedRequestIdentifier()
{
    QJsonObject response = validResponse();
    response.insert(QStringLiteral("request_id"), QStringLiteral("different-request"));

    const auto result = parse(response);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidArgument);
    QCOMPARE(std::get<ProfileError>(result).path, QStringLiteral("request_id"));
}

void KWinSnapshotParserTest::rejectsUnsupportedMonitorCount()
{
    QJsonObject response = validResponse();
    QJsonObject snapshot = response.value(QStringLiteral("snapshot")).toObject();
    snapshot.insert(QStringLiteral("screen_count"), 2);
    response.insert(QStringLiteral("snapshot"), snapshot);

    const auto result = parse(response);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QCOMPARE(std::get<ProfileError>(result).path, QStringLiteral("snapshot.screen_count"));
}

void KWinSnapshotParserTest::rejectsUnknownDesktopMembership()
{
    QJsonObject response = validResponse();
    QJsonObject snapshot = response.value(QStringLiteral("snapshot")).toObject();
    QJsonArray windows = snapshot.value(QStringLiteral("windows")).toArray();
    QJsonObject editor = windows[0].toObject();
    editor.insert(QStringLiteral("desktop_ids"), QJsonArray{QStringLiteral("unknown-desktop")});
    windows[0] = editor;
    snapshot.insert(QStringLiteral("windows"), windows);
    response.insert(QStringLiteral("snapshot"), snapshot);

    const auto result = parse(response);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidArgument);
}

void KWinSnapshotParserTest::rejectsDesktopIdsOnAllDesktopMembership()
{
    QJsonObject response = validResponse();
    QJsonObject snapshot = response.value(QStringLiteral("snapshot")).toObject();
    QJsonArray windows = snapshot.value(QStringLiteral("windows")).toArray();
    QJsonObject sticky = windows[1].toObject();
    sticky.insert(QStringLiteral("desktop_ids"), QJsonArray{});
    windows[1] = sticky;
    snapshot.insert(QStringLiteral("windows"), windows);
    response.insert(QStringLiteral("snapshot"), snapshot);

    const auto result = parse(response);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
    QCOMPARE(std::get<ProfileError>(result).path, QStringLiteral("snapshot.windows[1].desktop_ids"));
}

void KWinSnapshotParserTest::rejectsUnsupportedMaximizeMode()
{
    QJsonObject response = validResponse();
    QJsonObject snapshot = response.value(QStringLiteral("snapshot")).toObject();
    QJsonArray windows = snapshot.value(QStringLiteral("windows")).toArray();
    QJsonObject editor = windows[0].toObject();
    QJsonObject state = editor.value(QStringLiteral("state")).toObject();
    state.insert(QStringLiteral("maximize_mode"), QStringLiteral("diagonal"));
    editor.insert(QStringLiteral("state"), state);
    windows[0] = editor;
    snapshot.insert(QStringLiteral("windows"), windows);
    response.insert(QStringLiteral("snapshot"), snapshot);

    const auto result = parse(response);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidStructure);
}

void KWinSnapshotParserTest::rejectsConflictingFullscreenAndMaximizeState()
{
    QJsonObject response = validResponse();
    QJsonObject snapshot = response.value(QStringLiteral("snapshot")).toObject();
    QJsonArray windows = snapshot.value(QStringLiteral("windows")).toArray();
    QJsonObject editor = windows[0].toObject();
    QJsonObject state = editor.value(QStringLiteral("state")).toObject();
    state.insert(QStringLiteral("fullscreen"), true);
    editor.insert(QStringLiteral("state"), state);
    windows[0] = editor;
    snapshot.insert(QStringLiteral("windows"), windows);
    response.insert(QStringLiteral("snapshot"), snapshot);

    const auto result = parse(response);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::ValidationFailed);
}

void KWinSnapshotParserTest::rejectsRemoteCaptureErrors()
{
    QJsonObject response{{QStringLiteral("protocol_version"), KWinSnapshotParser::ProtocolVersion},
                         {QStringLiteral("request_id"), requestId},
                         {QStringLiteral("operation"), QStringLiteral("capture")},
                         {QStringLiteral("error"),
                          QJsonObject{{QStringLiteral("code"), QStringLiteral("unsupported_screen_count")},
                                      {QStringLiteral("message"), QStringLiteral("Multiple screens are not supported.")}}}};

    const auto result = parse(response);
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::InvalidArgument);
    QVERIFY(std::get<ProfileError>(result).message.contains(QStringLiteral("Multiple screens")));
}

QTEST_APPLESS_MAIN(KWinSnapshotParserTest)

#include "test_kwin_snapshot_parser.moc"
