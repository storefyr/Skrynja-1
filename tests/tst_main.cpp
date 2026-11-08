#include <QtTest>
#include <QFile>
#include <QJsonObject>
#include <QVariantMap>
#include "acpframereader.h"
#include "acpclient.h"

class Collector : public QObject
{
    Q_OBJECT
public:
    QList<QJsonObject> messages;
    QList<QJsonObject> hales;
    QStringList discarded;
    int overCap = 0;
public slots:
    void onMessage(const QJsonObject &o) { messages.append(o); }
    void onHale(const QJsonObject &o) { hales.append(o); }
    void onDiscarded(const QString &r) { discarded.append(r); }
    void onOverCap(qint64, qint64) { overCap++; }
};

static QByteArray readFixture(const QString &name)
{
    QFile f(QStringLiteral("fixtures/") + name);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

static void connectAll(AcpFrameReader &r, Collector &c)
{
    QObject::connect(&r, SIGNAL(messageReady(QJsonObject)), &c, SLOT(onMessage(QJsonObject)));
    QObject::connect(&r, SIGNAL(haleReady(QJsonObject)), &c, SLOT(onHale(QJsonObject)));
    QObject::connect(&r, SIGNAL(lineDiscarded(QString)), &c, SLOT(onDiscarded(QString)));
    QObject::connect(&r, SIGNAL(lineOverCap(qint64,qint64)), &c, SLOT(onOverCap(qint64,qint64)));
}

static qint64 msgId(const QJsonObject &o)
{
    return o.value(QStringLiteral("id")).toVariant().toLongLong();
}

class TestRygradParser : public QObject
{
    Q_OBJECT
private slots:
    void rygrad_fixture_hoved();
    void rygrad_sammenklaebning();
    void rygrad_ugyldig();
    void rygrad_utf8_ugyldig();
    void rygrad_crlf();
    void rygrad_split();
    void rygrad_cap();
};

class TestVaerktoejskald : public QObject
{
    Q_OBJECT
private slots:
    void vaerktoejskald_felter();
    void vaerktoejskald_tekstchunk();
    void selvtest_verdict();
    void selvtest_fail_uden_toolcall();
};

void TestVaerktoejskald::vaerktoejskald_felter()
{
    AcpClient acp;
    QByteArray line = QByteArray("{\"jsonrpc\":\"2.0\",\"method\":\"session/update\",\"params\":{\"sessionId\":\"s1\",\"update\":{\"sessionUpdate\":\"tool_call\",\"toolCallId\":\"call_0001\",\"title\":\"id -un\",\"kind\":\"shell\",\"status\":\"in_progress\",\"rawInput\":{\"command\":\"id -un\"}}}}\n");
    acp.feedBytesForTest(line);
    QCOMPARE(acp.toolCalls().count(), 1);
    QVariantMap tc = acp.toolCalls().first().toMap();
    QCOMPARE(tc.value(QStringLiteral("kind")).toString(), QStringLiteral("shell"));
    QCOMPARE(tc.value(QStringLiteral("title")).toString(), QStringLiteral("id -un"));
    QCOMPARE(tc.value(QStringLiteral("detalje")).toString(), QStringLiteral("id -un"));
}

void TestVaerktoejskald::vaerktoejskald_tekstchunk()
{
    AcpClient acp;
    QByteArray line = QByteArray("{\"jsonrpc\":\"2.0\",\"method\":\"session/update\",\"params\":{\"sessionId\":\"s1\",\"update\":{\"sessionUpdate\":\"agent_message_chunk\",\"content\":{\"type\":\"text\",\"text\":\"Hej\"}}}}\n");
    acp.feedBytesForTest(line);
    QCOMPARE(acp.lastText(), QStringLiteral("Hej"));
}

static QByteArray toolCallLine()
{
    return QByteArray("{\"jsonrpc\":\"2.0\",\"method\":\"session/update\",\"params\":{\"sessionId\":\"s1\",\"update\":{\"sessionUpdate\":\"tool_call\",\"toolCallId\":\"c1\",\"title\":\"id -un\",\"kind\":\"shell\",\"rawInput\":{\"command\":\"id -un\"}}}}\n");
}

static QByteArray endTurnLine()
{
    return QByteArray("{\"jsonrpc\":\"2.0\",\"id\":99,\"result\":{\"stopReason\":\"end_turn\"}}\n");
}

static QString readVerdict()
{
    QFile f(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

void TestVaerktoejskald::selvtest_verdict()
{
    AcpClient acp;
    acp.setSelvtest(true);
    QFile::remove(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    QObject::connect(&acp, &AcpClient::promptDone, &acp, [&acp]() { acp.finishSelvtest(1); });
    acp.feedBytesForTest(toolCallLine() + endTurnLine());
    const QString v = readVerdict();
    QVERIFY(v.contains(QStringLiteral("tool_calls=1")));
    QVERIFY(v.contains(QStringLiteral("ui_tool_calls=1")));
    QVERIFY(v.contains(QStringLiteral("VERDICT=PASS")));
}

void TestVaerktoejskald::selvtest_fail_uden_toolcall()
{
    AcpClient acp;
    acp.setSelvtest(true);
    QFile::remove(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    QObject::connect(&acp, &AcpClient::promptDone, &acp, [&acp]() { acp.finishSelvtest(0); });
    acp.feedBytesForTest(endTurnLine());
    const QString v = readVerdict();
    QVERIFY(v.contains(QStringLiteral("tool_calls=0")));
    QVERIFY(v.contains(QStringLiteral("ui_tool_calls=0")));
    QVERIFY(v.contains(QStringLiteral("VERDICT=FAIL")));
}

void TestRygradParser::rygrad_fixture_hoved()
{
    AcpFrameReader r;
    Collector c;
    connectAll(r, c);
    QByteArray data = readFixture(QStringLiteral("fixture_rygrad.ndjson"));
    QVERIFY(!data.isEmpty());
    r.feed(data);
    QCOMPARE(c.messages.count(), 13);
    QCOMPARE(msgId(c.messages.first()), 1LL);
    QCOMPARE(c.messages.first().value(QStringLiteral("result")).toObject()
                 .value(QStringLiteral("protocolVersion")).toInt(), 1);
    r.feedEof();
    QCOMPARE(c.messages.count(), 13);
    QCOMPARE(c.hales.count(), 0);
    QVERIFY(c.discarded.count() >= 1);
}

void TestRygradParser::rygrad_sammenklaebning()
{
    AcpFrameReader r;
    Collector c;
    connectAll(r, c);
    QByteArray data = readFixture(QStringLiteral("fixture_rygrad_sammenklæbning.ndjson"));
    QVERIFY(!data.isEmpty());
    r.feed(data);
    QCOMPARE(c.messages.count(), 2);
    QCOMPARE(msgId(c.messages.at(0)), 7LL);
    QCOMPARE(msgId(c.messages.at(1)), 8LL);
    r.feedEof();
    QCOMPARE(c.messages.count(), 2);
    QCOMPARE(c.hales.count(), 0);
    QVERIFY(c.discarded.count() >= 1);
}

void TestRygradParser::rygrad_ugyldig()
{
    AcpFrameReader r;
    Collector c;
    connectAll(r, c);
    QByteArray data = readFixture(QStringLiteral("fixture_rygrad_ugyldig.ndjson"));
    QVERIFY(!data.isEmpty());
    r.feed(data);
    // linje 1 og 2 har efter-garbage og kasseres hele (rollback); kun id 12 er ren
    QCOMPARE(c.messages.count(), 1);
    QCOMPARE(msgId(c.messages.first()), 12LL);
    QVERIFY(c.discarded.count() >= 2);
    r.feedEof();
    QCOMPARE(c.messages.count(), 1);
}

void TestRygradParser::rygrad_utf8_ugyldig()
{
    AcpFrameReader r;
    Collector c;
    connectAll(r, c);
    QByteArray data = readFixture(QStringLiteral("fixture_rygrad_utf8_ugyldig.ndjson"));
    QVERIFY(!data.isEmpty());
    r.feed(data);
    QCOMPARE(c.messages.count(), 0);
    QVERIFY(c.discarded.count() >= 2);
    r.feedEof();
    QCOMPARE(c.messages.count(), 0);
}

void TestRygradParser::rygrad_crlf()
{
    AcpFrameReader r;
    Collector c;
    connectAll(r, c);
    r.feed(QByteArray("{\"jsonrpc\":\"2.0\",\"id\":9,\"result\":{}}\r\n"));
    QCOMPARE(c.messages.count(), 1);
    QCOMPARE(msgId(c.messages.first()), 9LL);
}

void TestRygradParser::rygrad_split()
{
    AcpFrameReader r;
    Collector c;
    connectAll(r, c);
    QByteArray frame = QByteArray("{\"jsonrpc\":\"2.0\",\"id\":30,\"result\":{\"ok\":true}}\n");
    r.feed(frame.left(10));
    QCOMPARE(c.messages.count(), 0);
    r.feed(frame.mid(10));
    QCOMPARE(c.messages.count(), 1);
    QCOMPARE(msgId(c.messages.first()), 30LL);
}

void TestRygradParser::rygrad_cap()
{
    AcpFrameReader r;
    Collector c;
    connectAll(r, c);
    r.setLineCap(64);
    QByteArray big = QByteArray("{\"jsonrpc\":\"2.0\",\"id\":40,\"result\":{\"pad\":\"") + QByteArray(200, 'x') + QByteArray("\"}}\n");
    QByteArray small = QByteArray("{\"jsonrpc\":\"2.0\",\"id\":41,\"result\":{}}\n");
    r.feed(big + small);
    QCOMPARE(c.overCap, 1);
    QCOMPARE(c.messages.count(), 1);
    QCOMPARE(msgId(c.messages.first()), 41LL);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;
    {
        TestRygradParser t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestVaerktoejskald t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}

#include "tst_main.moc"

