#ifndef TCPMESSAGE_H
#define TCPMESSAGE_H

#include <QVariant>
#include <QIODevice>
#include <QMap>
#include <QByteArray>
#include <QDataStream>

class TcpMessage {
public:
    enum MessageType {
        Unknown = 0,

        RegisterChunkServer,
        RegisterChunkServerResponse,
        RegisterFile,
        RegisterFileResponse,
        GetFileInfo,
        GetFileInfoResponse,

        StoreChunk,
        StoreChunkResponse,
        GetChunk,
        GetChunkResponse,
    };

    TcpMessage() : m_type(Unknown) {}
    TcpMessage(MessageType type) : m_type(type) {}

    MessageType type() const { return m_type; }
    void setType(MessageType type) { m_type = type; }

    void setData(const QString &key, const QVariant &value) { m_data[key] = value; }
    QVariant getData(const QString &key) const { return m_data.value(key); }
    bool hasData(const QString &key) const { return m_data.contains(key); }

    QByteArray toByteArray() const;
    static TcpMessage fromByteArray(const QByteArray &data);

private:
    MessageType m_type;
    QMap<QString, QVariant> m_data;
};

#endif
