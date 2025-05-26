#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QHostAddress>
#include <QDataStream>
#include "tcpmessage.h"

class TcpSocket : public QObject
{
    Q_OBJECT

public:
    explicit TcpSocket(QObject *parent = nullptr);
    explicit TcpSocket(QTcpSocket *socket, QObject *parent = nullptr);
    ~TcpSocket();

    bool connectToHost(const QHostAddress &address, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    bool sendMessage(const TcpMessage &message);

    void setProxy(const QNetworkProxy &proxy);
    bool connectToHost(const QString &hostname, quint16 port, 
                      QAbstractSocket::NetworkLayerProtocol protocol = QAbstractSocket::AnyIPProtocol);

    QString errorString() const { return m_socket->errorString(); }
    QAbstractSocket::SocketState state() const { return m_socket->state(); }
    QAbstractSocket::SocketError error() const { return m_socket->error(); }

signals:
    void connected();
    void disconnected();
    void messageReceived(const TcpMessage &message);
    void error(QAbstractSocket::SocketError socketError);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onBytesWritten(qint64 bytes);
    void onError(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    quint32 m_messageSize;
    bool m_waitingForSize;
};

class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

    bool listen(const QHostAddress &address, quint16 port);
    void close();

signals:
    void newConnection(TcpSocket *socket);

private slots:
    void onNewConnection();

private:
    QTcpServer *m_server;
};


#endif // TCPSOCKET_H
