#include "tcpsocket.h"
#include <QNetworkInterface>
#include <QNetworkProxy>

TcpSocket::TcpSocket(QObject *parent) : QObject(parent), m_messageSize(0), m_waitingForSize(true)
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &TcpSocket::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpSocket::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpSocket::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpSocket::onBytesWritten);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpSocket::onError);

    qDebug() << "TCP socket created";
}

TcpSocket::TcpSocket(QTcpSocket *socket, QObject *parent) : QObject(parent), m_socket(socket), m_messageSize(0), m_waitingForSize(true)
{
    connect(m_socket, &QTcpSocket::connected, this, &TcpSocket::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpSocket::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpSocket::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpSocket::onBytesWritten);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpSocket::onError);

    qDebug() << "TCP socket created from existing connection";
}

TcpSocket::~TcpSocket()
{
    m_socket->close();
    qDebug() << "TCP socket closed";
}


void TcpSocket::setProxy(const QNetworkProxy &proxy)
{
    if (m_socket) {
        m_socket->setProxy(proxy);
    }
}

bool TcpSocket::connectToHost(const QString &hostname, quint16 port, 
                             QAbstractSocket::NetworkLayerProtocol protocol)
{
    qDebug() << "TcpSocket: Starting connection to" << hostname << ":" << port 
             << "with protocol:" << protocol;
    
    if (!m_socket) {
        m_socket = new QTcpSocket(this);

        connect(m_socket, &QTcpSocket::connected, this, &TcpSocket::onConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &TcpSocket::onDisconnected);
        connect(m_socket, &QTcpSocket::readyRead, this, &TcpSocket::onReadyRead);
        connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpSocket::onBytesWritten);
        connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpSocket::onError);
    }
    

    m_socket->setProxy(QNetworkProxy::NoProxy);

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    
    qDebug() << "TcpSocket: Connecting to" << hostname << ":" << port;
    m_socket->connectToHost(hostname, port, QIODevice::ReadWrite, protocol);
    
    return true;
}

bool TcpSocket::connectToHost(const QHostAddress &address, quint16 port)
{
    return connectToHost(address.toString(), port, QAbstractSocket::IPv4Protocol);
}

void TcpSocket::onConnected()
{
    qDebug() << "TcpSocket: Connected to" << m_socket->peerAddress().toString() << ":" << m_socket->peerPort();
    emit connected();
}

void TcpSocket::onError(QAbstractSocket::SocketError socketError)
{
    qDebug() << "Socket error:" << socketError << "-" << m_socket->errorString();
    emit error(socketError);
}


void TcpSocket::disconnectFromHost()
{
    m_socket->disconnectFromHost();
}

bool TcpSocket::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool TcpSocket::sendMessage(const TcpMessage &message)
{
  if (!isConnected()) {
        qDebug() << "TcpSocket: Cannot send message - not connected";
        return false;
    }
    
    QByteArray data = message.toByteArray();
    QByteArray packet;
    QDataStream sizeStream(&packet, QIODevice::WriteOnly);
    

    sizeStream << (quint32)data.size();

    packet.append(data);

    qint64 bytesWritten = m_socket->write(packet);
    
    if (bytesWritten == -1) {
        qDebug() << "TcpSocket: Error writing to socket:" << m_socket->errorString();
        return false;
    }
    
    if (bytesWritten != packet.size()) {
        qDebug() << "TcpSocket: Partial write - only" << bytesWritten << "of" 
                 << packet.size() << "bytes written";
        return false;
    }
    
    return true;

}

void TcpSocket::onReadyRead()
{
    while (m_socket->bytesAvailable() > 0) {
        if (m_waitingForSize) {
            if (m_socket->bytesAvailable() < sizeof(quint32)) {
                return;
            }

            QDataStream sizeStream(m_socket);
            sizeStream >> m_messageSize;

            m_waitingForSize = false;
            m_buffer.clear();
        }

        qint64 bytesToRead = qMin<qint64>(m_socket->bytesAvailable(), m_messageSize - m_buffer.size());
        m_buffer.append(m_socket->read(bytesToRead));

        if (m_buffer.size() == m_messageSize) {
            try {
                TcpMessage message = TcpMessage::fromByteArray(m_buffer);
                emit messageReceived(message);
            } catch (...) {
                qDebug() << "Error parsing TCP message";
            }

            m_waitingForSize = true;
            m_buffer.clear();
        }
    }
}

void TcpSocket::onBytesWritten(qint64 bytes)
{
    // Optional: track bytes written or implement flow control
}

void TcpSocket::onDisconnected()
{
    emit disconnected();
}

TcpServer::TcpServer(QObject *parent) : QObject(parent)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
}

TcpServer::~TcpServer()
{
    m_server->close();
}

bool TcpServer::listen(const QHostAddress &address, quint16 port)
{
    bool success = m_server->listen(address, port);
    if (success) {
        qDebug() << "TCP server listening on" << address.toString() << ":" << port;
    } else {
        qDebug() << "Failed to start TCP server on" << address.toString() << ":" << port
                 << "-" << m_server->errorString();
    }
    return success;
}

void TcpServer::close()
{
    m_server->close();
    qDebug() << "TCP server closed";
}

void TcpServer::onNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    TcpSocket *tcpSocket = new TcpSocket(socket, this);

    emit newConnection(tcpSocket);

    qDebug() << "New TCP connection from" << socket->peerAddress().toString() << ":" << socket->peerPort();
}
