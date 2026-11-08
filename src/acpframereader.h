#ifndef ACPFRAMEREADER_H
#define ACPFRAMEREADER_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>

// Newline-delimited JSON frame reader for ACP over stdio.
// Rules: split on 0x0A only (0x0D is left in and eaten as JSON whitespace);
// a whole line is validated before anything is delivered (invalid tail rolls
// back already collected objects); several objects on one line are extracted
// with the QJsonParseError::offset loop; lines over the cap are dropped at
// arrival with one lineOverCap per line; at EOF a non-empty tail is parsed
// and delivered via haleReady.
class AcpFrameReader : public QObject
{
    Q_OBJECT
public:
    explicit AcpFrameReader(QObject *parent = nullptr);

    static qint64 defaultLineCap();
    void setLineCap(qint64 cap) { m_cap = cap; }
    qint64 lineCap() const { return m_cap; }

    void feed(const QByteArray &data);
    void feedEof();

signals:
    void messageReady(const QJsonObject &obj);
    void haleReady(const QJsonObject &obj);
    void lineDiscarded(const QString &reason);
    void lineOverCap(qint64 receivedBytes, qint64 cap);

private:
    void parseAndDeliver(const QByteArray &line, bool fromTail);

    QByteArray m_buffer;
    qint64 m_counter = 0;
    qint64 m_cap;
    bool m_discarding = false;
    bool m_capNotified = false;
};

#endif // ACPFRAMEREADER_H
