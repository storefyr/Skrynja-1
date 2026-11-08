#ifndef ACPCLIENT_H
#define ACPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QHash>
#include <QJsonObject>
#include <QVariantList>
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
    Q_PROPERTY(QVariantList toolCalls READ toolCalls NOTIFY toolCallsChanged)
    Q_PROPERTY(QVariantMap pendingPermission READ pendingPermission NOTIFY pendingPermissionChanged)
    Q_PROPERTY(bool hasPendingPermission READ hasPendingPermission NOTIFY pendingPermissionChanged)
public:
    explicit AcpClient(QObject *parent = nullptr);

    static QString defaultAgentPath();

    Q_INVOKABLE void start(const QString &agentPath, const QString &cwd);
    Q_INVOKABLE qint64 sendPrompt(const QString &text);

    QString sessionId() const { return m_sessionId; }
    QString lastText() const { return m_lastText; }
    QVariantList toolCalls() const { return m_toolCalls; }

    void setSelvtest(bool on) { m_selvtest = on; }
    void feedBytesForTest(const QByteArray &d) { m_reader.feed(d); }
    Q_INVOKABLE void uiToolCallsSeen(int n) { m_uiToolCalls = n; }
    void finishSelvtest();

    Q_INVOKABLE void answerPermission(const QString &optionId);
    Q_INVOKABLE void dismissPermission();
    Q_INVOKABLE void stopAgent();
    Q_INVOKABLE qint64 sendSetConfigOption(const QString &configId, const QVariant &value);

    QVariantMap pendingPermission() const { return m_pendingPermission; }
    bool hasPendingPermission() const { return !m_pendingPermission.isEmpty(); }
    int permissionRequests() const { return m_permissionRequests; }

signals:
    void sessionIdChanged();
    void lastTextChanged();
    void toolCallsChanged();
    void started();
    void finished(int exitCode);
    void frameWritten(const QByteArray &frame);
    void promptDone();
    void pendingPermissionChanged();

private slots:
    void onReadyRead();
    void onMessage(const QJsonObject &obj);
    void onDiscarded(const QString &reason);
    void onProcessFinished(int exitCode);

private:
    qint64 sendRequest(const QString &method, const QJsonObject &params);
    void writeFrame(const QJsonObject &obj);
    void dispatch(const QJsonObject &obj);
    void handlePermissionRequest(qint64 id, const QJsonObject &params);
    void sendResult(qint64 id, const QJsonObject &result);
    void sendNotification(const QString &method, const QJsonObject &params);

    AcpFrameReader m_reader;
    QProcess m_process;
    QString m_sessionId;
    QString m_lastText;
    QVariantList m_toolCalls;
    QVariantMap m_pendingPermission;
    int m_uiToolCalls = 0;
    int m_permissionRequests = 0;
    bool m_selvtest = false;
    QString m_cwd;
    qint64 m_nextId = 1;
    QHash<qint64, QString> m_methods;
};

#endif // ACPCLIENT_H
