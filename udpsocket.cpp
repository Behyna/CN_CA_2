#include "udpsocket.h"
#include <QDataStream>
#include <QDebug>

const QByteArray UdpSocket::PING_MESSAGE = "PING";
const QByteArray UdpSocket::PONG_MESSAGE = "PONG";

UdpSocket::UdpSocket(QObject *parent) : QObject(parent)
{
    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpSocket::onReadyRead);
    qDebug() << "UDP socket created for firewall punching";
}

UdpSocket::~UdpSocket()
{
    this->blockSignals(true);

    if (m_socket) {
        m_socket->disconnect();
        m_socket->blockSignals(true);
    }

    qDebug() << "UDP socket closed";
}

bool UdpSocket::bind(const QHostAddress &address, quint16 port)
{
    bool success = m_socket->bind(address, port);
    if (success) {
        qDebug() << "UDP socket bound to" << address.toString() << ":" << port;
    } else {
        qDebug() << "Failed to bind UDP socket to" << address.toString() << ":" << port;
    }
    return success;
}

bool UdpSocket::sendData(const QHostAddress &address, quint16 port, const QByteArray &data)
{
    qint64 bytesSent = m_socket->writeDatagram(data, address, port);
    return bytesSent == data.size();
}

bool UdpSocket::sendPing(const QHostAddress &address, quint16 port)
{
    return sendData(address, port, PING_MESSAGE);
}

bool UdpSocket::sendPong(const QHostAddress &address, quint16 port)
{
    return sendData(address, port, PONG_MESSAGE);
}

void UdpSocket::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        emit dataReceived(datagram, sender, senderPort);

        if (datagram == PING_MESSAGE) {
            emit pingReceived(sender, senderPort);
        } else if (datagram == PONG_MESSAGE) {
            emit pongReceived(sender, senderPort);
        }
    }
}
