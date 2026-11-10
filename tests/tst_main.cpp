#include <QtTest>
#include <QFile>
#include <QJsonObject>
#include <QVariantMap>
#include <QSettings>
#include "acpframereader.h"
#include "acpclient.h"

static QByteArray toolCallLine();
static QByteArray endTurnLine();
static QByteArray permissionRequestLine();
static QByteArray diffToolCallLine();
static QByteArray outputUpdateLine();
static QByteArray configResponseLine(const char *currentValue);
static QByteArray userChunkLine(const char *text);
static QString readVerdict();

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
    void rygrad_cap_rydder_praefiks();
};

class TestVaerktoejskald : public QObject
{
    Q_OBJECT
private slots:
    void vaerktoejskald_felter();
    void vaerktoejskald_tekstchunk();
    void selvtest_verdict();
    void selvtest_fail_uden_toolcall();
    void selvtest_ingen_dobbelt_verdict();
    void selvtest_verdict_ved_agentdød();
    void selvtest_verdict_ved_startfejl();
};

class FrameCollector : public QObject
{
    Q_OBJECT
public:
    QStringList frames;
public slots:
    void onFrame(const QByteArray &f) { frames.append(QString::fromUtf8(f)); }
};

class TestTilladelser : public QObject
{
    Q_OBJECT
private slots:
    void tilladelse_anmodning();
    void tilladelse_tillad_svar();
    void tilladelse_afvis_svar();
    void tilladelse_stop_raekkefoelge();
    void tilladelse_tom_titel_faar_fallback();
    void tool_status_opdateres();
    void anmodning_ukendt_metode_faar_fejl();
    void selvtest_vaelger_tillad_ved_navn();
};

static QByteArray permissionEmptyTitleLine()
{
    return QByteArray("{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"session/request_permission\",\"params\":{\"toolCall\":{\"toolCallId\":\"c2\",\"kind\":\"execute\",\"rawInput\":{\"url\":\"https://github.com/storefyr/Skrynja-1\"}},\"options\":[{\"optionId\":\"allow_once\",\"name\":\"Allow Once\"},{\"optionId\":\"reject\",\"name\":\"Afvis\"}]}}\n");
}

class TestDiffUddata : public QObject
{
    Q_OBJECT
private slots:
    void diff_blokk_samles();
    void uddata_blokk_samles();
};

class TestKonfiguration : public QObject
{
    Q_OBJECT
private slots:
    void configoptions_samles();
    void configoptions_opdateres_ved_svar();
};

class TestGenoptagelse : public QObject
{
    Q_OBJECT
private slots:
    void load_fallback_ved_fejl();
    void load_ok_saetter_session();
    void load_ok_starter_selvtest_probe();
    void replay_vises_i_lasttext();
    void klient_henter_gammel_tekst_ved_start();
};

static QByteArray userChunkLine(const char *text)
{
    return QByteArray("{\"jsonrpc\":\"2.0\",\"method\":\"session/update\",\"params\":{\"sessionId\":\"s\",\"update\":{\"sessionUpdate\":\"user_message_chunk\",\"content\":{\"type\":\"text\",\"text\":\"") + text + QByteArray("\"}}}}\n");
}

void TestGenoptagelse::load_fallback_ved_fejl()
{
    AcpClient acp;
    FrameCollector fc;
    QObject::connect(&acp, SIGNAL(frameWritten(QByteArray)), &fc, SLOT(onFrame(QByteArray)));
    const qint64 id = acp.sendRequestForTest(QStringLiteral("session/load"), QJsonObject());
    acp.feedBytesForTest(QByteArray("{\"jsonrpc\":\"2.0\",\"id\":") + QByteArray::number(id) +
                        QByteArray(",\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}\n"));
    QVERIFY(fc.frames.join(QStringLiteral("\n")).contains(QStringLiteral("session/new")));
}

void TestGenoptagelse::load_ok_saetter_session()
{
    AcpClient acp;
    const qint64 id = acp.sendRequestForTest(QStringLiteral("session/load"), QJsonObject());
    acp.feedBytesForTest(QByteArray("{\"jsonrpc\":\"2.0\",\"id\":") + QByteArray::number(id) +
                        QByteArray(",\"result\":{\"sessionId\":\"ny-session-123\"}}\n"));
    QCOMPARE(acp.sessionId(), QStringLiteral("ny-session-123"));
}

void TestGenoptagelse::load_ok_starter_selvtest_probe()
{
    AcpClient acp;
    acp.setSelvtest(true);
    FrameCollector fc;
    QObject::connect(&acp, SIGNAL(frameWritten(QByteArray)), &fc, SLOT(onFrame(QByteArray)));
    const qint64 id = acp.sendRequestForTest(QStringLiteral("session/load"), QJsonObject());
    acp.feedBytesForTest(QByteArray("{\"jsonrpc\":\"2.0\",\"id\":") + QByteArray::number(id) +
                        QByteArray(",\"result\":{\"sessionId\":\"ny-session-123\"}}\n"));
    QString all = fc.frames.join(QStringLiteral("\n"));
    QVERIFY(all.contains(QStringLiteral("session/set_config_option")));
    QVERIFY(!all.contains(QStringLiteral("session/prompt")));
    acp.feedBytesForTest(QByteArray("{\"jsonrpc\":\"2.0\",\"id\":") + QByteArray::number(id + 1) +
                        QByteArray(",\"result\":{\"configOptions\":[{\"id\":\"mode\",\"name\":\"Mode\",\"type\":\"select\",\"currentValue\":\"ask\",\"options\":[{\"value\":\"ask\",\"name\":\"Ask\"}]}]}}\n"));
    all = fc.frames.join(QStringLiteral("\n"));
    QVERIFY(all.contains(QStringLiteral("session/prompt")));
    QVERIFY(all.contains(QStringLiteral("cat /tmp/skrynja-1-probe.txt")));
}

void TestGenoptagelse::replay_vises_i_lasttext()
{
    AcpClient acp;
    acp.feedBytesForTest(userChunkLine("gammel besked"));
    QVERIFY(acp.lastText().contains(QStringLiteral("gammel besked")));
}

void TestGenoptagelse::klient_henter_gammel_tekst_ved_start()
{
    QSettings s;
    s.setValue(QStringLiteral("acp/lastText"), QStringLiteral("gammel samtale fra før"));
    {
        AcpClient acp;
        QCOMPARE(acp.lastText(), QStringLiteral("gammel samtale fra før"));
        QVERIFY(acp.hadRestoredHistory());
    }
    s.remove(QStringLiteral("acp/lastText"));
}

void TestKonfiguration::configoptions_samles()
{
    AcpClient acp;
    acp.feedBytesForTest(configResponseLine("ask"));
    QCOMPARE(acp.configOptions().count(), 1);
    QVariantMap c = acp.configOptions().first().toMap();
    QCOMPARE(c.value(QStringLiteral("id")).toString(), QStringLiteral("mode"));
    QCOMPARE(c.value(QStringLiteral("currentValue")).toString(), QStringLiteral("ask"));
    QCOMPARE(c.value(QStringLiteral("options")).toList().count(), 2);
}

void TestKonfiguration::configoptions_opdateres_ved_svar()
{
    AcpClient acp;
    acp.feedBytesForTest(configResponseLine("ask"));
    acp.feedBytesForTest(configResponseLine("accept-edits"));
    QCOMPARE(acp.configOptions().count(), 1);
    QCOMPARE(acp.configOptions().first().toMap().value(QStringLiteral("currentValue")).toString(),
             QStringLiteral("accept-edits"));
}

void TestDiffUddata::diff_blokk_samles()
{
    AcpClient acp;
    acp.feedBytesForTest(diffToolCallLine());
    QCOMPARE(acp.diffs().count(), 1);
    QVariantMap d = acp.diffs().first().toMap();
    QCOMPARE(d.value(QStringLiteral("sti")).toString(), QStringLiteral("/tmp/skrynja-1-probe.txt"));
    QVERIFY(d.value(QStringLiteral("diff")).toString().contains(QStringLiteral("+OK")));
}

void TestDiffUddata::uddata_blokk_samles()
{
    AcpClient acp;
    acp.feedBytesForTest(toolCallLine() + outputUpdateLine());
    QCOMPARE(acp.outputs().count(), 1);
    QVERIFY(acp.outputs().first().toMap().value(QStringLiteral("tekst")).toString()
            .contains(QStringLiteral("defaultuser")));
}

void TestTilladelser::tilladelse_anmodning()
{
    AcpClient acp;
    acp.feedBytesForTest(permissionRequestLine());
    QVERIFY(acp.hasPendingPermission());
    QCOMPARE(acp.permissionRequests(), 1);
    QVariantMap p = acp.pendingPermission();
    QCOMPARE(p.value(QStringLiteral("title")).toString(), QStringLiteral("id -un"));
    QCOMPARE(p.value(QStringLiteral("kind")).toString(), QStringLiteral("shell"));
    QCOMPARE(p.value(QStringLiteral("kommando")).toString(), QStringLiteral("id -un"));
    QCOMPARE(p.value(QStringLiteral("options")).toList().count(), 2);
}

void TestTilladelser::tilladelse_tillad_svar()
{
    AcpClient acp;
    FrameCollector fc;
    QObject::connect(&acp, SIGNAL(frameWritten(QByteArray)), &fc, SLOT(onFrame(QByteArray)));
    acp.feedBytesForTest(permissionRequestLine());
    acp.answerPermission(QStringLiteral("allow"));
    QVERIFY(!acp.hasPendingPermission());
    QVERIFY(fc.frames.last().contains(QStringLiteral("\"selected\"")));
    QVERIFY(fc.frames.last().contains(QStringLiteral("\"allow\"")));
    QVERIFY(fc.frames.last().contains(QStringLiteral("\"id\":7")));
}

void TestTilladelser::tilladelse_afvis_svar()
{
    AcpClient acp;
    FrameCollector fc;
    QObject::connect(&acp, SIGNAL(frameWritten(QByteArray)), &fc, SLOT(onFrame(QByteArray)));
    acp.feedBytesForTest(permissionRequestLine());
    acp.dismissPermission();
    QVERIFY(fc.frames.last().contains(QStringLiteral("\"cancelled\"")));
}

void TestTilladelser::tilladelse_tom_titel_faar_fallback()
{
    AcpClient acp;
    acp.feedBytesForTest(permissionEmptyTitleLine());
    QVariantMap p = acp.pendingPermission();
    QVERIFY(p.value(QStringLiteral("title")).toString().isEmpty());
    QVERIFY(p.value(QStringLiteral("rå")).toString().contains(QStringLiteral("github.com")));
}

void TestTilladelser::tool_status_opdateres()
{
    AcpClient acp;
    acp.feedBytesForTest(diffToolCallLine());
    QCOMPARE(acp.toolCalls().first().toMap().value(QStringLiteral("status")).toString(), QStringLiteral("in_progress"));
    acp.feedBytesForTest(QByteArray("{\"jsonrpc\":\"2.0\",\"method\":\"session/update\",\"params\":{\"sessionId\":\"s\",\"update\":{\"sessionUpdate\":\"tool_call_update\",\"toolCallId\":\"c1\",\"status\":\"failed\",\"content\":[{\"type\":\"text\",\"text\":\"boom-fejl\"}]}}}\n"));
    QCOMPARE(acp.toolCalls().first().toMap().value(QStringLiteral("status")).toString(), QStringLiteral("failed"));
    QVERIFY(acp.lastError().contains(QStringLiteral("boom-fejl")));
}

static QByteArray permissionRejectFirstLine()
{
    return QByteArray("{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"session/request_permission\",\"params\":{\"toolCall\":{\"title\":\"t\",\"kind\":\"shell\",\"rawInput\":{\"command\":\"x\"}},\"options\":[{\"optionId\":\"reject\",\"name\":\"Afvis\"},{\"optionId\":\"allow_once\",\"name\":\"Allow Once\"}]}}\n");
}

void TestTilladelser::anmodning_ukendt_metode_faar_fejl()
{
    AcpClient acp;
    FrameCollector fc;
    QObject::connect(&acp, SIGNAL(frameWritten(QByteArray)), &fc, SLOT(onFrame(QByteArray)));
    acp.feedBytesForTest(QByteArray("{\"jsonrpc\":\"2.0\",\"id\":55,\"method\":\"fs/read_text_file\",\"params\":{}}\n"));
    QVERIFY(fc.frames.last().contains(QStringLiteral("\"error\"")));
    QVERIFY(fc.frames.last().contains(QStringLiteral("-32601")));
    QVERIFY(fc.frames.last().contains(QStringLiteral("\"id\":55")));
}

void TestTilladelser::selvtest_vaelger_tillad_ved_navn()
{
    AcpClient acp;
    acp.setSelvtest(true);
    FrameCollector fc;
    QObject::connect(&acp, SIGNAL(frameWritten(QByteArray)), &fc, SLOT(onFrame(QByteArray)));
    acp.feedBytesForTest(permissionRejectFirstLine());
    QVERIFY(fc.frames.last().contains(QStringLiteral("\"selected\"")));
    QVERIFY(fc.frames.last().contains(QStringLiteral("allow_once")));
    QVERIFY(!fc.frames.last().contains(QStringLiteral("\"reject\"")));
}

void TestTilladelser::tilladelse_stop_raekkefoelge()
{
    AcpClient acp;
    FrameCollector fc;
    QObject::connect(&acp, SIGNAL(frameWritten(QByteArray)), &fc, SLOT(onFrame(QByteArray)));
    acp.feedBytesForTest(permissionRequestLine());
    acp.stopAgent();
    int cancelIdx = -1;
    int cancelledIdx = -1;
    for (int i = 0; i < fc.frames.count(); i++) {
        if (fc.frames.at(i).contains(QStringLiteral("session/cancel")))
            cancelIdx = i;
        if (fc.frames.at(i).contains(QStringLiteral("\"cancelled\"")))
            cancelledIdx = i;
    }
    QVERIFY(cancelIdx >= 0);
    QVERIFY(cancelledIdx >= 0);
    QVERIFY(cancelIdx < cancelledIdx);
}

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

static QByteArray diffToolCallLine()
{
    return QByteArray("{\"jsonrpc\":\"2.0\",\"method\":\"session/update\",\"params\":{\"sessionId\":\"s\",\"update\":{\"sessionUpdate\":\"tool_call\",\"toolCallId\":\"c1\",\"title\":\"Skriv fil\",\"kind\":\"edit\",\"status\":\"in_progress\",\"rawInput\":{\"file_path\":\"/tmp/skrynja-1-probe.txt\"},\"content\":[{\"type\":\"diff\",\"path\":\"/tmp/skrynja-1-probe.txt\",\"oldText\":\"\",\"newText\":\"+OK\"}]}}}\n");
}

static QByteArray outputUpdateLine()
{
    return QByteArray("{\"jsonrpc\":\"2.0\",\"method\":\"session/update\",\"params\":{\"sessionId\":\"s\",\"update\":{\"sessionUpdate\":\"tool_call_update\",\"toolCallId\":\"c9\",\"status\":\"completed\",\"content\":[{\"type\":\"text\",\"text\":\"defaultuser\\n\"}]}}}\n");
}

static QByteArray configResponseLine(const char *currentValue)
{
    QByteArray v = currentValue;
    return QByteArray("{\"jsonrpc\":\"2.0\",\"id\":5,\"result\":{\"sessionId\":\"s1\",\"configOptions\":[{\"id\":\"mode\",\"name\":\"Mode\",\"type\":\"select\",\"currentValue\":\"") + v +
        QByteArray("\",\"options\":[{\"value\":\"ask\",\"name\":\"Ask\"},{\"value\":\"accept-edits\",\"name\":\"Accept Edits\"}]}]}}\n");
}

static QByteArray permissionRequestLine()
{
    return QByteArray("{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"session/request_permission\",\"params\":{\"toolCall\":{\"title\":\"id -un\",\"kind\":\"shell\",\"rawInput\":{\"command\":\"id -un\"}},\"options\":[{\"optionId\":\"allow\",\"name\":\"Tillad\"},{\"optionId\":\"reject\",\"name\":\"Afvis\"}]}}\n");
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
    QFile probe(QStringLiteral("/tmp/skrynja-1-probe.txt"));
    QVERIFY(probe.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    probe.write("OK");
    probe.close();
    QObject::connect(&acp, &AcpClient::promptDone, &acp, [&acp]() { acp.finishSelvtest(); });
    acp.uiToolCallsSeen(1);
    acp.feedBytesForTest(configResponseLine("ask") + permissionRequestLine() + diffToolCallLine() + toolCallLine() + endTurnLine());
    const QString v = readVerdict();
    QVERIFY(v.contains(QStringLiteral("tool_calls=1")));
    QVERIFY(v.contains(QStringLiteral("ui_tool_calls=1")));
    QVERIFY(v.contains(QStringLiteral("permission_requests=1")));
    QVERIFY(v.contains(QStringLiteral("probe_fil=OK")));
    QVERIFY(v.contains(QStringLiteral("diffs=1")));
    QVERIFY(v.contains(QStringLiteral("config_options=1")));
    QVERIFY(v.contains(QStringLiteral("VERDICT=PASS")));
    QFile::remove(QStringLiteral("/tmp/skrynja-1-probe.txt"));
}

void TestVaerktoejskald::selvtest_fail_uden_toolcall()
{
    AcpClient acp;
    acp.setSelvtest(true);
    QFile::remove(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    QObject::connect(&acp, &AcpClient::promptDone, &acp, [&acp]() { acp.finishSelvtest(); });
    acp.uiToolCallsSeen(0);
    acp.feedBytesForTest(endTurnLine());
    const QString v = readVerdict();
    QVERIFY(v.contains(QStringLiteral("tool_calls=0")));
    QVERIFY(v.contains(QStringLiteral("ui_tool_calls=0")));
    QVERIFY(v.contains(QStringLiteral("VERDICT=FAIL")));
}

void TestVaerktoejskald::selvtest_ingen_dobbelt_verdict()
{
    AcpClient acp;
    acp.setSelvtest(true);
    QFile::remove(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    QFile probe(QStringLiteral("/tmp/skrynja-1-probe.txt"));
    QVERIFY(probe.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    probe.write("OK");
    probe.close();
    QObject::connect(&acp, &AcpClient::promptDone, &acp, [&acp]() { acp.finishSelvtest(); });
    acp.uiToolCallsSeen(1);
    acp.feedBytesForTest(configResponseLine("ask") + permissionRequestLine() + diffToolCallLine() + toolCallLine() + endTurnLine());
    acp.uiToolCallsSeen(0);
    acp.finishSelvtest();
    const QString v = readVerdict();
    QVERIFY(v.contains(QStringLiteral("ui_tool_calls=1")));
    QVERIFY(v.contains(QStringLiteral("VERDICT=PASS")));
    QFile::remove(QStringLiteral("/tmp/skrynja-1-probe.txt"));
}

void TestVaerktoejskald::selvtest_verdict_ved_agentdød()
{
    AcpClient acp;
    acp.setSelvtest(true);
    QFile::remove(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    QVERIFY(QMetaObject::invokeMethod(&acp, "onProcessFinished", Qt::DirectConnection, Q_ARG(int, 0)));
    const QString v = readVerdict();
    QVERIFY(v.contains(QStringLiteral("VERDICT=FAIL")));
    QVERIFY(v.contains(QStringLiteral("slut_aarsag=agent_afsluttet")));
}

void TestVaerktoejskald::selvtest_verdict_ved_startfejl()
{
    AcpClient acp;
    acp.setSelvtest(true);
    QFile::remove(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    acp.start(QStringLiteral("/nonexistent/agent"), QStringLiteral("/tmp"));
    const QString v = readVerdict();
    QVERIFY(v.contains(QStringLiteral("slut_aarsag=agent_start_fejlede")));
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

void TestRygradParser::rygrad_cap_rydder_praefiks()
{
    AcpFrameReader r;
    Collector c;
    connectAll(r, c);
    r.setLineCap(32);
    QByteArray big = QByteArray("{\"id\":40,\"pad\":\"") + QByteArray(100, 'x') + QByteArray("\"}\n");
    QByteArray small = QByteArray("{\"id\":41}\n");
    r.feed(big.left(20));
    QCOMPARE(c.messages.count(), 0);
    r.feed(big.mid(20) + small);
    QCOMPARE(c.overCap, 1);
    QCOMPARE(c.messages.count(), 1);
    QCOMPARE(msgId(c.messages.first()), 41LL);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("org.skrynja"));
    app.setApplicationName(QStringLiteral("Skrynja-1-test"));
    int status = 0;
    {
        TestRygradParser t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestVaerktoejskald t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestTilladelser t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestDiffUddata t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestKonfiguration t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestGenoptagelse t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}

#include "tst_main.moc"

