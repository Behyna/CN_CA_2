#ifndef NODE_H
#define NODE_H

#include <QObject>
#include <QString>
#include <QHostAddress>
#include <QDateTime>
#include <QCoreApplication>

class Node : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        Offline,
        Online
    };

    explicit Node(const QString &id, quint16 port, QObject *parent = nullptr);
    virtual ~Node();

    virtual bool initialize();
    virtual void shutdown();

    QString nodeId() const { return m_nodeId; }
    quint16 port() const { return m_port; }
    Status status() const { return m_status; }
    QHostAddress address() const { return m_address; }
    QHostAddress tcpAddress() const { return m_tcpAddress; }

    void setNodeId(const QString &id) { m_nodeId = id; }
    void setPort(quint16 port) { m_port = port; }
    void setAddress(const QHostAddress &address) { m_address = address; }
    void setTcpAddress(const QHostAddress &address) { m_tcpAddress = address; }

protected:
    virtual bool prepareShutdown();

    QString m_nodeId;
    QHostAddress m_address;
    quint16 m_port;
    Status m_status;
    QHostAddress m_tcpAddress;
    bool m_isShuttingDown = false;
};

#endif // NODE_H
