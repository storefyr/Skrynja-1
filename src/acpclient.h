#ifndef ACPCLIENT_H
#define ACPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QHash>
#include <QJsonObject>
#include "acpframereader.h"

// Lean ACP client for skive_rygrad: spawns the local agent (vibe-acp) over
// stdio, sends initialize (protocolVersion 1, empty clientCapabilities),
// then session/new, then session/prompt; agent_message_chunk text is
// accumulated into lastText for the UI.
class AcpClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(QString lastText READ lastText NOTIFY lastTextChanged)
public:
    explicit AcpClient(QObject *parent = nullptr);

    static QString defaultAgentPath();

    Q_INVOKABLE void start(const QString &agentPath, const QString &cwd);
    Q_INVOKABLE qint64 sendPrompt(const QString &text);

    QString sessionId() const { return m_sessionId; }
    QString lastText() const { return m_lastText; }

signals:
    void sessionIdChanged();
    void lastTextChanged();
    void started();
    void finished(int exitCode);
    void frameWritten(const QByteArray &frame);

private slots:
    void onReadyRead();
    void onMessage(const QJsonObject &obj);
    void onDiscarded(const QString &reason);
    void onProcessFinished(int exitCode);

private:
    qint64 sendRequest(const QString &method, const QJsonObject &params);
    void writeFrame(const QJsonObject &obj);
    void dispatch(const QJsonObject &obj);

    AcpFrameReader m_reader;
    QProcess m_process;
    QString m_sessionId;
    QString m_lastText;
    QString m_cwd;
    qint64 m_nextId = 1;
    QHash<qint64, QString> m_methods;
};

#endif // ACPCLIENT_H
