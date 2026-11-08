#include "acpframereader.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QList>

qint64 AcpFrameReader::defaultLineCap()
{
    return 8 * 1024 * 1024;
}

AcpFrameReader::AcpFrameReader(QObject *parent)
    : QObject(parent), m_cap(defaultLineCap())
{
}

void AcpFrameReader::feed(const QByteArray &data)
{
    qint64 pos = 0;
    while (pos < data.size()) {
        const int nl = data.indexOf('\n', pos);
        if (m_discarding) {
            if (nl < 0) {
                m_counter += data.size() - pos;
                pos = data.size();
            } else {
                pos = nl + 1;
                m_counter = 0;
                m_discarding = false;
                m_capNotified = false;
            }
            continue;
        }
        if (nl < 0) {
            const qint64 remaining = data.size() - pos;
            m_counter += remaining;
            if (m_counter > m_cap) {
                if (!m_capNotified) {
                    emit lineOverCap(m_counter, m_cap);
                    m_capNotified = true;
                }
                m_discarding = true;
                m_buffer.clear();
            } else {
                m_buffer.append(data.constData() + pos, remaining);
            }
            pos = data.size();
            continue;
        }
        m_counter += (nl - pos) + 1;
        if (m_counter > m_cap) {
            if (!m_capNotified) {
                emit lineOverCap(m_counter, m_cap);
                m_capNotified = true;
            }
            m_discarding = true;
            m_buffer.clear();
        } else {
            m_buffer.append(data.constData() + pos, nl - pos);
            parseAndDeliver(m_buffer, false);
            m_buffer.clear();
        }
        m_counter = 0;
        m_capNotified = false;
        pos = nl + 1;
    }
}

void AcpFrameReader::feedEof()
{
    if (!m_buffer.isEmpty()) {
        const QByteArray tail = m_buffer;
        m_buffer.clear();
        m_counter = 0;
        parseAndDeliver(tail, true);
    }
}

void AcpFrameReader::parseAndDeliver(const QByteArray &line, bool fromTail)
{
    QList<QJsonObject> objs;
    int offset = 0;
    while (offset < line.size()) {
        while (offset < line.size() &&
               (line[offset] == ' ' || line[offset] == '\t' ||
                line[offset] == '\r' || line[offset] == '\n'))
            offset++;
        if (offset >= line.size())
            break;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line.mid(offset), &err);
        if (err.error == QJsonParseError::NoError) {
            if (doc.isObject())
                objs.append(doc.object());
            else
                emit lineDiscarded(QStringLiteral("top level is not an object"));
            break;
        }
        if (err.error != QJsonParseError::ExtraData) {
            emit lineDiscarded(err.errorString());
            return;
        }
        QJsonParseError err2;
        QJsonDocument first = QJsonDocument::fromJson(line.mid(offset, err.offset), &err2);
        if (err2.error != QJsonParseError::NoError || !first.isObject()) {
            emit lineDiscarded(err2.errorString());
            return;
        }
        objs.append(first.object());
        offset += err.offset;
    }
    foreach (const QJsonObject &o, objs) {
        if (fromTail)
            emit haleReady(o);
        else
            emit messageReady(o);
    }
}
