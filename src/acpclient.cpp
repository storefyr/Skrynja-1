#include "acpclient.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QDebug>

QString AcpClient::defaultAgentPath()
{
    return QStringLiteral("/home/defaultuser/.local/bin/vibe-acp");
}

AcpClient::AcpClient(QObject *parent) : QObject(parent)
{
    QSettings s;
    m_sessionId = s.value(QStringLiteral("acp/sessionId")).toString();
    m_lastText = s.value(QStringLiteral("acp/lastText")).toString();
    m_hadRestored = !m_lastText.isEmpty();
    connect(&m_reader, SIGNAL(messageReady(QJsonObject)), this, SLOT(onMessage(QJsonObject)));
    connect(&m_reader, SIGNAL(lineDiscarded(QString)), this, SLOT(onDiscarded(QString)));
    connect(&m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(onReadyRead()));
    connect(&m_process, SIGNAL(finished(int)), this, SLOT(onProcessFinished(int)));
    connect(this, &AcpClient::sessionIdChanged, this, [this]() {
        QSettings s2;
        s2.setValue(QStringLiteral("acp/sessionId"), m_sessionId);
    });
    connect(this, &AcpClient::lastTextChanged, this, [this]() {
        QSettings s2;
        s2.setValue(QStringLiteral("acp/lastText"), m_lastText);
    });
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

qint64 AcpClient::sendSessionNew(const QString &cwd)
{
    QJsonObject params;
    params.insert(QStringLiteral("cwd"), cwd);
    params.insert(QStringLiteral("mcpServers"), QJsonArray());
    return sendRequest(QStringLiteral("session/new"), params);
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
        if (obj.contains(QStringLiteral("id"))) {
            const qint64 id = obj.value(QStringLiteral("id")).toVariant().toLongLong();
            const QString method = obj.value(QStringLiteral("method")).toString();
            if (method == QStringLiteral("session/request_permission"))
                handlePermissionRequest(id, obj.value(QStringLiteral("params")).toObject());
            return;
        }
        if (obj.value(QStringLiteral("method")).toString() == QStringLiteral("session/update")) {
            const QJsonObject upd = obj.value(QStringLiteral("params")).toObject()
                    .value(QStringLiteral("update")).toObject();
            const QString kind = upd.value(QStringLiteral("sessionUpdate")).toString();
            if (kind == QStringLiteral("agent_message_chunk") || kind == QStringLiteral("user_message_chunk")) {
                const QString t = upd.value(QStringLiteral("content")).toObject()
                        .value(QStringLiteral("text")).toString();
                if (!t.isEmpty()) {
                    m_lastText += t;
                    emit lastTextChanged();
                    if (kind == QStringLiteral("user_message_chunk"))
                        m_replayedUser++;
                }
            } else if (kind == QStringLiteral("tool_call") || kind == QStringLiteral("tool_call_update")) {
                const QString toolCallId = upd.value(QStringLiteral("toolCallId")).toString();
                if (kind == QStringLiteral("tool_call")) {
                    QVariantMap tc;
                    tc.insert(QStringLiteral("toolCallId"), toolCallId);
                    tc.insert(QStringLiteral("kind"), upd.value(QStringLiteral("kind")).toString());
                    tc.insert(QStringLiteral("title"), upd.value(QStringLiteral("title")).toString());
                    const QJsonObject raw = upd.value(QStringLiteral("rawInput")).toObject();
                    QString detalje = raw.value(QStringLiteral("command")).toString();
                    if (detalje.isEmpty())
                        detalje = raw.value(QStringLiteral("path")).toString();
                    if (detalje.isEmpty())
                        detalje = raw.value(QStringLiteral("file_path")).toString();
                    if (detalje.isEmpty())
                        detalje = upd.value(QStringLiteral("location")).toObject()
                                .value(QStringLiteral("path")).toString();
                    tc.insert(QStringLiteral("detalje"), detalje);
                    m_toolCalls.append(tc);
                    emit toolCallsChanged();
                }
                const QJsonArray content = upd.value(QStringLiteral("content")).toArray();
                for (int i = 0; i < content.size(); i++) {
                    const QJsonObject block = content.at(i).toObject();
                    const QString type = block.value(QStringLiteral("type")).toString();
                    if (type == QStringLiteral("diff")) {
                        QVariantMap d;
                        QString sti = block.value(QStringLiteral("path")).toString();
                        if (sti.isEmpty())
                            sti = block.value(QStringLiteral("file_path")).toString();
                        d.insert(QStringLiteral("sti"), sti);
                        QString diffText = block.value(QStringLiteral("newText")).toString();
                        if (diffText.isEmpty())
                            diffText = block.value(QStringLiteral("diff")).toString();
                        d.insert(QStringLiteral("diff"), diffText);
                        m_diffs.append(d);
                        emit diffsChanged();
                    } else {
                        QVariantMap o;
                        o.insert(QStringLiteral("toolCallId"), toolCallId);
                        o.insert(QStringLiteral("tekst"), block.value(QStringLiteral("text")).toString());
                        m_outputs.append(o);
                        emit outputsChanged();
                    }
                }
            }
        }
        return;
    }
    const qint64 id = obj.value(QStringLiteral("id")).toVariant().toLongLong();
    const QString method = m_methods.take(id);
    if (obj.contains(QStringLiteral("error"))) {
        qWarning() << "[acp] fejl fra agent, id" << id << obj.value(QStringLiteral("error")).toObject();
        if (method == QStringLiteral("session/load"))
            sendSessionNew(m_cwd);
        return;
    }
    const QJsonObject result = obj.value(QStringLiteral("result")).toObject();
    if (result.contains(QStringLiteral("configOptions"))) {
        QVariantList list;
        const QJsonArray arr = result.value(QStringLiteral("configOptions")).toArray();
        for (int i = 0; i < arr.size(); i++) {
            const QJsonObject o = arr.at(i).toObject();
            QVariantMap m;
            m.insert(QStringLiteral("id"), o.value(QStringLiteral("id")).toString());
            m.insert(QStringLiteral("name"), o.value(QStringLiteral("name")).toString());
            m.insert(QStringLiteral("type"), o.value(QStringLiteral("type")).toString());
            m.insert(QStringLiteral("currentValue"), o.value(QStringLiteral("currentValue")).toVariant());
            QVariantList opts;
            const QJsonArray oa = o.value(QStringLiteral("options")).toArray();
            for (int j = 0; j < oa.size(); j++) {
                QVariantMap oo;
                oo.insert(QStringLiteral("value"), oa.at(j).toObject().value(QStringLiteral("value")).toVariant());
                oo.insert(QStringLiteral("name"), oa.at(j).toObject().value(QStringLiteral("name")).toString());
                opts.append(oo);
            }
            m.insert(QStringLiteral("options"), opts);
            list.append(m);
        }
        m_configOptions = list;
        emit configOptionsChanged();
    }
    if (method == QStringLiteral("initialize")) {
        if (!m_sessionId.isEmpty()) {
            QJsonObject params;
            params.insert(QStringLiteral("sessionId"), m_sessionId);
            params.insert(QStringLiteral("cwd"), m_cwd);
            sendRequest(QStringLiteral("session/load"), params);
        } else {
            sendSessionNew(m_cwd);
        }
    } else if (method == QStringLiteral("session/load")) {
        m_sessionLoaded = true;
        const QString sid = result.value(QStringLiteral("sessionId")).toString();
        if (!sid.isEmpty()) {
            m_sessionId = sid;
            emit sessionIdChanged();
        }
    } else if (method == QStringLiteral("session/new")) {
        m_sessionId = result.value(QStringLiteral("sessionId")).toString();
        emit sessionIdChanged();
        m_mode = result.value(QStringLiteral("modes")).toObject()
                    .value(QStringLiteral("currentModeId")).toString();
        if (m_selvtest) {
            sendSetConfigOption(QStringLiteral("mode"), QStringLiteral("ask"));
            sendPrompt(QStringLiteral("opret filen /tmp/skrynja-1-probe.txt med teksten OK"));
        }
    } else if (method == QStringLiteral("session/set_config_option")) {
        const QJsonArray opts = result.value(QStringLiteral("configOptions")).toArray();
        for (int i = 0; i < opts.size(); i++) {
            const QJsonObject o = opts.at(i).toObject();
            if (o.value(QStringLiteral("id")).toString() == QStringLiteral("mode"))
                m_mode = o.value(QStringLiteral("currentValue")).toString();
        }
    }
    if (m_selvtest && result.contains(QStringLiteral("stopReason")))
        emit promptDone();
}

void AcpClient::handlePermissionRequest(qint64 id, const QJsonObject &params)
{
    m_permissionRequests++;
    QVariantMap p;
    p.insert(QStringLiteral("id"), id);
    const QJsonObject tc = params.value(QStringLiteral("toolCall")).toObject();
    p.insert(QStringLiteral("kind"), tc.value(QStringLiteral("kind")).toString());
    p.insert(QStringLiteral("title"), tc.value(QStringLiteral("title")).toString());
    const QJsonObject raw = tc.value(QStringLiteral("rawInput")).toObject();
    QString cmd = raw.value(QStringLiteral("command")).toString();
    if (cmd.isEmpty())
        cmd = raw.value(QStringLiteral("path")).toString();
    p.insert(QStringLiteral("kommando"), cmd);
    QVariantList opts;
    const QJsonArray arr = params.value(QStringLiteral("options")).toArray();
    for (int i = 0; i < arr.size(); i++) {
        const QJsonObject vo = arr.at(i).toObject();
        QVariantMap o;
        o.insert(QStringLiteral("optionId"), vo.value(QStringLiteral("optionId")).toString());
        o.insert(QStringLiteral("name"), vo.value(QStringLiteral("name")).toString());
        opts.append(o);
    }
    p.insert(QStringLiteral("options"), opts);
    m_pendingPermission = p;
    emit pendingPermissionChanged();
    if (m_selvtest)
        answerPermission(opts.isEmpty() ? QStringLiteral("allow")
                                       : opts.first().toMap().value(QStringLiteral("optionId")).toString());
}

void AcpClient::answerPermission(const QString &optionId)
{
    if (m_pendingPermission.isEmpty())
        return;
    const qint64 id = m_pendingPermission.value(QStringLiteral("id")).toLongLong();
    QJsonObject outcome;
    outcome.insert(QStringLiteral("outcome"), QStringLiteral("selected"));
    outcome.insert(QStringLiteral("optionId"), optionId);
    QJsonObject result;
    result.insert(QStringLiteral("outcome"), outcome);
    sendResult(id, result);
    m_pendingPermission.clear();
    emit pendingPermissionChanged();
}

void AcpClient::dismissPermission()
{
    if (m_pendingPermission.isEmpty())
        return;
    const qint64 id = m_pendingPermission.value(QStringLiteral("id")).toLongLong();
    QJsonObject outcome;
    outcome.insert(QStringLiteral("outcome"), QStringLiteral("cancelled"));
    QJsonObject result;
    result.insert(QStringLiteral("outcome"), outcome);
    sendResult(id, result);
    m_pendingPermission.clear();
    emit pendingPermissionChanged();
}

void AcpClient::stopAgent()
{
    QJsonObject params;
    if (!m_sessionId.isEmpty())
        params.insert(QStringLiteral("sessionId"), m_sessionId);
    sendNotification(QStringLiteral("session/cancel"), params);
    dismissPermission();
}

qint64 AcpClient::sendSetConfigOption(const QString &configId, const QVariant &value)
{
    QJsonObject params;
    params.insert(QStringLiteral("sessionId"), m_sessionId);
    params.insert(QStringLiteral("configId"), configId);
    params.insert(QStringLiteral("value"), QJsonValue::fromVariant(value));
    return sendRequest(QStringLiteral("session/set_config_option"), params);
}

void AcpClient::sendResult(qint64 id, const QJsonObject &result)
{
    QJsonObject resp;
    resp.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    resp.insert(QStringLiteral("id"), id);
    resp.insert(QStringLiteral("result"), result);
    writeFrame(resp);
}

void AcpClient::sendNotification(const QString &method, const QJsonObject &params)
{
    QJsonObject note;
    note.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    note.insert(QStringLiteral("method"), method);
    if (!params.isEmpty())
        note.insert(QStringLiteral("params"), params);
    writeFrame(note);
}

void AcpClient::finishSelvtest()
{
    QString probe;
    QFile pf(QStringLiteral("/tmp/skrynja-1-probe.txt"));
    if (pf.open(QIODevice::ReadOnly | QIODevice::Text))
        probe = QString::fromUtf8(pf.readAll());
    const bool probeOk = probe.contains(QStringLiteral("OK"));
    const bool pass = !m_toolCalls.isEmpty() && m_uiToolCalls > 0 && m_permissionRequests > 0 && probeOk
                      && (!m_diffs.isEmpty() || !m_outputs.isEmpty()) && !m_configOptions.isEmpty();
    qDebug() << "[selvtest] tool_calls=" << m_toolCalls.count() << " ui_tool_calls=" << m_uiToolCalls
             << " permission_requests=" << m_permissionRequests << " probe_fil=" << (probeOk ? "OK" : "mangler");
    QFile f(QStringLiteral("/tmp/skrynja-1-selvtest.txt"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write("tool_calls=" + QByteArray::number(m_toolCalls.count()) + "\n");
        f.write("ui_tool_calls=" + QByteArray::number(m_uiToolCalls) + "\n");
        f.write("permission_requests=" + QByteArray::number(m_permissionRequests) + "\n");
        f.write("probe_fil=" + QByteArray(probeOk ? "OK" : "mangler") + "\n");
        f.write("diffs=" + QByteArray::number(m_diffs.count()) + "\n");
        f.write("outputs=" + QByteArray::number(m_outputs.count()) + "\n");
        f.write("config_options=" + QByteArray::number(m_configOptions.count()) + "\n");
        f.write("session_load=" + QByteArray(m_sessionLoaded ? "ok" : (m_hadRestored || !m_sessionId.isEmpty() ? "fallback" : "ingen")) + "\n");
        f.write("genoptaget=" + QByteArray(m_hadRestored ? "ja" : "nej") + "\n");
        f.write("replayed_user=" + QByteArray::number(m_replayedUser) + "\n");
        f.write("mode=" + m_mode.toUtf8() + "\n");
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
