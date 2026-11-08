#include "acpclient.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>

QString AcpClient::defaultAgentPath()
{
    return QStringLiteral("/home/defaultuser/.local/bin/vibe-acp");
}

AcpClient::AcpClient(QObject *parent) : QObject(parent)
{
    connect(&m_reader, SIGNAL(messageReady(QJsonObject)), this, SLOT(onMessage(QJsonObject)));
    connect(&m_reader, SIGNAL(lineDiscarded(QString)), this, SLOT(onDiscarded(QString)));
    connect(&m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(onReadyRead()));
    connect(&m_process, SIGNAL(finished(int)), this, SLOT(onProcessFinished(int)));
}

void AcpClient::start(const QString &agentPath, const QString &cwd)
{
    m_cwd = cwd;
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start(agentPath, QStringList());
    if (!m_process.waitForStarted(5000)) {
        qWarning() << "[acp] agent start fejlede:" << agentPath;
        return;
    }
    emit started();
    QJsonObject clientInfo;
    clientInfo.insert(QStringLiteral("name"), QStringLiteral("Skrynja-1"));
    clientInfo.insert(QStringLiteral("version"), QStringLiteral("0.1.0"));
    QJsonObject params;
    params.insert(QStringLiteral("protocolVersion"), 1);
    params.insert(QStringLiteral("clientCapabilities"), QJsonObject());
    params.insert(QStringLiteral("clientInfo"), clientInfo);
    sendRequest(QStringLiteral("initialize"), params);
}

qint64 AcpClient::sendPrompt(const QString &text)
{
    if (m_sessionId.isEmpty() || m_process.state() != QProcess::Running)
        return -1;
    QJsonObject content;
    content.insert(QStringLiteral("type"), QStringLiteral("text"));
    content.insert(QStringLiteral("text"), text);
    QJsonObject params;
    params.insert(QStringLiteral("sessionId"), m_sessionId);
    params.insert(QStringLiteral("prompt"), QJsonArray() << content);
    return sendRequest(QStringLiteral("session/prompt"), params);
}

void AcpClient::onReadyRead()
{
    m_reader.feed(m_process.readAllStandardOutput());
}

void AcpClient::onMessage(const QJsonObject &obj)
{
    qDebug() << "[acp][in]" << QJsonDocument(obj).toJson(QJsonDocument::Compact);
    dispatch(obj);
}

void AcpClient::onDiscarded(const QString &reason)
{
    qWarning() << "[acp] linje kasseret:" << reason;
}

void AcpClient::onProcessFinished(int exitCode)
{
    m_reader.feedEof();
    qDebug() << "[acp] agent afsluttet, exit-kode" << exitCode;
    emit finished(exitCode);
}

void AcpClient::dispatch(const QJsonObject &obj)
{
    if (obj.contains(QStringLiteral("method"))) {
        if (obj.value(QStringLiteral("method")).toString() == QStringLiteral("session/update")) {
            const QJsonObject upd = obj.value(QStringLiteral("params")).toObject()
                    .value(QStringLiteral("update")).toObject();
            const QString kind = upd.value(QStringLiteral("sessionUpdate")).toString();
            if (kind == QStringLiteral("agent_message_chunk")) {
                const QString t = upd.value(QStringLiteral("content")).toObject()
                        .value(QStringLiteral("text")).toString();
                if (!t.isEmpty()) {
                    m_lastText += t;
                    emit lastTextChanged();
                }
            } else if (kind == QStringLiteral("tool_call")) {
                QVariantMap tc;
                tc.insert(QStringLiteral("toolCallId"), upd.value(QStringLiteral("toolCallId")).toString());
                tc.insert(QStringLiteral("kind"), upd.value(QStringLiteral("kind")).toString());
                tc.insert(QStringLiteral("title"), upd.value(QStringLiteral("title")).toString());
                const QJsonObject raw = upd.value(QStringLiteral("rawInput")).toObject();
                QString detalje = raw.value(QStringLiteral("command")).toString();
                if (detalje.isEmpty())
                    detalje = raw.value(QStringLiteral("path")).toString();
                if (detalje.isEmpty())
                    detalje = upd.value(QStringLiteral("location")).toObject()
                            .value(QStringLiteral("path")).toString();
                tc.insert(QStringLiteral("detalje"), detalje);
                m_toolCalls.append(tc);
                emit toolCallsChanged();
            }
        }
        return;
    }
    const qint64 id = obj.value(QStringLiteral("id")).toVariant().toLongLong();
    const QString method = m_methods.take(id);
    if (obj.contains(QStringLiteral("error"))) {
        qWarning() << "[acp] fejl fra agent, id" << id << obj.value(QStringLiteral("error")).toObject();
        return;
    }
    const QJsonObject result = obj.value(QStringLiteral("result")).toObject();
    if (method == QStringLiteral("initialize")) {
        QJsonObject params;
        params.insert(QStringLiteral("cwd"), m_cwd);
        params.insert(QStringLiteral("mcpServers"), QJsonArray());
        sendRequest(QStringLiteral("session/new"), params);
    } else if (method == QStringLiteral("session/new")) {
        m_sessionId = result.value(QStringLiteral("sessionId")).toString();
        emit sessionIdChanged();
        if (m_selvtest)
            sendPrompt(QStringLiteral("kør id -un i terminalen"));
    }
    if (m_selvtest && result.contains(QStringLiteral("stopReason")))
        emit promptDone();
}

void AcpClient::finishSelvtest(int uiToolCalls)
{
    const bool pass = !m_toolCalls.isEmpty() && uiToolCalls > 0;
    QFile f(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write("tool_calls=" + QByteArray::number(m_toolCalls.count()) + "\n");
        f.write("ui_tool_calls=" + QByteArray::number(uiToolCalls) + "\n");
        f.write("last_text=" + m_lastText.toUtf8() + "\n");
        f.write(pass ? "VERDICT=PASS\n" : "VERDICT=FAIL\n");
        f.close();
    }
    if (m_process.state() == QProcess::Running)
        QCoreApplication::exit(pass ? 0 : 1);
}

qint64 AcpClient::sendRequest(const QString &method, const QJsonObject &params)
{
    const qint64 id = m_nextId++;
    QJsonObject req;
    req.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    req.insert(QStringLiteral("id"), id);
    req.insert(QStringLiteral("method"), method);
    if (!params.isEmpty())
        req.insert(QStringLiteral("params"), params);
    m_methods.insert(id, method);
    writeFrame(req);
    return id;
}

void AcpClient::writeFrame(const QJsonObject &obj)
{
    const QByteArray frame = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    qDebug() << "[acp][out]" << frame;
    if (m_process.state() == QProcess::Running)
        m_process.write(frame);
    emit frameWritten(frame);
}
