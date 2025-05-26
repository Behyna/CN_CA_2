#include "tcpmessage.h"

QByteArray TcpMessage::toByteArray() const
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    stream << (qint32)m_type;
    stream << (qint32)m_data.size();

    for (auto it = m_data.constBegin(); it != m_data.constEnd(); ++it) {
        stream << it.key() << it.value();
    }

    return result;
}

TcpMessage TcpMessage::fromByteArray(const QByteArray &data)
{
    TcpMessage result;
    QDataStream stream(data);

    qint32 type;
    stream >> type;
    result.m_type = (MessageType)type;

    qint32 count;
    stream >> count;

    for (int i = 0; i < count; i++) {
        QString key;
        QVariant value;
        stream >> key >> value;
        result.m_data[key] = value;
    }

    return result;
}
