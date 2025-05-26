#ifndef UDPSOCKET_H
#define UDPSOCKET_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>

class UdpSocket : public QObject
{
    Q_OBJECT

public:
    explicit UdpSocket(QObject *parent = nullptr);
    ~UdpSocket();

    bool bind(const QHostAddress &address, quint16 port);
    bool sendData(const QHostAddress &address, quint16 port, const QByteArray &data);

    bool sendPing(const QHostAddress &address, quint16 port);
    bool sendPong(const QHostAddress &address, quint16 port);

    static const QByteArray PING_MESSAGE;
    static const QByteArray PONG_MESSAGE;

signals:
    void dataReceived(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);
    void pingReceived(const QHostAddress &sender, quint16 senderPort);
    void pongReceived(const QHostAddress &sender, quint16 senderPort);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket;
};

#endif // UDPSOCKET_H
