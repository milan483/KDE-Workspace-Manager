#include "kwin/kwin_request_queue.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace kwm::kwin;

class KWinRequestQueueTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void returnsEmptyWhenNoRequestIsPending();
    void keepsAnInFlightRequestUntilAcknowledged();
    void rejectsMismatchedCompletionIdentifiers();
    void cancelsAnActiveRequestAndAdvancesTheQueue();
    void cancelsAQueuedRequestWithoutAffectingTheActiveRequest();
    void clearsTheActiveRequestBeforeNotifyingSubscribers();
    void advancesToTheNextQueuedRequestAfterCompletion();
};

void KWinRequestQueueTest::returnsEmptyWhenNoRequestIsPending()
{
    KWinRequestQueue queue;

    QVERIFY(queue.takeNextRequest().isEmpty());
    QCOMPARE(queue.pendingRequestCount(), 0);
    QVERIFY(!queue.hasPendingRequests());
}

void KWinRequestQueueTest::keepsAnInFlightRequestUntilAcknowledged()
{
    KWinRequestQueue queue;
    const QString requestId = queue.enqueueCaptureRequest();

    QVERIFY(!requestId.isEmpty());
    QCOMPARE(queue.pendingRequestCount(), 1);
    QVERIFY(queue.hasPendingRequests());

    const QString firstDelivery = queue.takeNextRequest();
    const QString secondDelivery = queue.takeNextRequest();
    QCOMPARE(firstDelivery, secondDelivery);

    const QJsonObject request = QJsonDocument::fromJson(firstDelivery.toUtf8()).object();
    QCOMPARE(request.value(QStringLiteral("protocol_version")).toInt(), KWinRequestQueue::ProtocolVersion);
    QCOMPARE(request.value(QStringLiteral("request_id")).toString(), requestId);
    QCOMPARE(request.value(QStringLiteral("operation")).toString(), QStringLiteral("capture"));
}

void KWinRequestQueueTest::rejectsMismatchedCompletionIdentifiers()
{
    KWinRequestQueue queue;
    QVERIFY(!queue.enqueueCaptureRequest().isEmpty());
    QVERIFY(!queue.takeNextRequest().isEmpty());

    QVERIFY(!queue.completeRequest(QStringLiteral("not-the-active-request"), QStringLiteral("{}")));
    QCOMPARE(queue.pendingRequestCount(), 1);
    QVERIFY(queue.hasPendingRequests());
}

void KWinRequestQueueTest::cancelsAnActiveRequestAndAdvancesTheQueue()
{
    KWinRequestQueue queue;
    const QString firstId = queue.enqueueCaptureRequest();
    const QString secondId = queue.enqueueCaptureRequest();
    QVERIFY(!queue.takeNextRequest().isEmpty());

    QVERIFY(queue.cancelRequest(firstId));
    QCOMPARE(queue.pendingRequestCount(), 1);
    const QJsonObject next = QJsonDocument::fromJson(queue.takeNextRequest().toUtf8()).object();
    QCOMPARE(next.value(QStringLiteral("request_id")).toString(), secondId);
    QVERIFY(!queue.cancelRequest(firstId));
    QVERIFY(queue.cancelRequest(secondId));
    QVERIFY(!queue.hasPendingRequests());
}

void KWinRequestQueueTest::cancelsAQueuedRequestWithoutAffectingTheActiveRequest()
{
    KWinRequestQueue queue;
    const QString firstId = queue.enqueueCaptureRequest();
    const QString secondId = queue.enqueueCaptureRequest();
    const QString firstRequest = queue.takeNextRequest();

    QVERIFY(queue.cancelRequest(secondId));
    QCOMPARE(queue.takeNextRequest(), firstRequest);
    QCOMPARE(queue.pendingRequestCount(), 1);
    QVERIFY(!queue.cancelRequest(secondId));
    QVERIFY(queue.completeRequest(firstId, QStringLiteral("{}")));
    QVERIFY(!queue.hasPendingRequests());
}

void KWinRequestQueueTest::clearsTheActiveRequestBeforeNotifyingSubscribers()
{
    KWinRequestQueue queue;
    const QString firstId = queue.enqueueCaptureRequest();
    const QString secondId = queue.enqueueCaptureRequest();
    QVERIFY(!queue.takeNextRequest().isEmpty());

    QString nextRequestDuringSignal;
    connect(&queue, &KWinRequestQueue::requestCompleted, &queue,
            [&](const QString &, const QString &) { nextRequestDuringSignal = queue.takeNextRequest(); });

    QVERIFY(queue.completeRequest(firstId, QStringLiteral("{}")));
    const QJsonObject next = QJsonDocument::fromJson(nextRequestDuringSignal.toUtf8()).object();
    QCOMPARE(next.value(QStringLiteral("request_id")).toString(), secondId);
    QCOMPARE(queue.pendingRequestCount(), 1);
}

void KWinRequestQueueTest::advancesToTheNextQueuedRequestAfterCompletion()
{
    KWinRequestQueue queue;
    QSignalSpy completedSpy(&queue, &KWinRequestQueue::requestCompleted);

    const QString firstId = queue.enqueueCaptureRequest();
    const QString secondId = queue.enqueueCaptureRequest();
    const QString firstRequest = queue.takeNextRequest();
    const QString response = QStringLiteral("{\"request_id\":\"%1\"}").arg(firstId);

    QVERIFY(queue.completeRequest(firstId, response));
    QCOMPARE(completedSpy.size(), 1);
    QCOMPARE(completedSpy.constFirst().at(0).toString(), firstId);
    QCOMPARE(completedSpy.constFirst().at(1).toString(), response);

    const QString secondRequest = queue.takeNextRequest();
    QVERIFY(firstRequest != secondRequest);
    const QJsonObject request = QJsonDocument::fromJson(secondRequest.toUtf8()).object();
    QCOMPARE(request.value(QStringLiteral("request_id")).toString(), secondId);
    QVERIFY(queue.completeRequest(secondId, QStringLiteral("{}")));
    QVERIFY(!queue.hasPendingRequests());
    QCOMPARE(queue.pendingRequestCount(), 0);
    QVERIFY(queue.takeNextRequest().isEmpty());
}

QTEST_APPLESS_MAIN(KWinRequestQueueTest)

#include "test_kwin_request_queue.moc"
