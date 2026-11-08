#include <QtTest>
#include <QFile>
#include <QJsonObject>
#include "acpframereader.h"

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
    QCOMPARE(c.messages.count(), 2);
    QCOMPARE(msgId(c.messages.at(0)), 11LL);
    QCOMPARE(msgId(c.messages.at(1)), 12LL);
    QVERIFY(c.discarded.count() >= 1);
    r.feedEof();
    QCOMPARE(c.messages.count(), 2);
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

QTEST_APPLESS_MAIN(TestRygradParser)
#include "tst_main.moc"
