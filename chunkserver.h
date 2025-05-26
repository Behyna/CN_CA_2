#ifndef CHUNKSERVER_H
#define CHUNKSERVER_H

#include "node.h"
#include "tcpsocket.h"
#include "errorcorrection.h"
#include "udpsocket.h"
#include <QString>
#include <QDir>
#include <QTimer>
#include <QThread>
#include <QNetworkInterface>
#include <QCoreApplication>
#include "manager.h"

class ChunkServer : public Node
{
    Q_OBJECT
public:
    explicit ChunkServer(QObject *parent = nullptr);
    virtual ~ChunkServer();

    bool initialize() override;
    void shutdown() override;

    bool initializeServer();
    bool connectToManager();

    int chunkServerId() const { return m_chunkServerId; }
    QString storagePath() const { return m_storagePath; }
    qint64 availableSpace() const;
    qint64 totalSpace() const;

    void setStoragePath(const QString &path);
    void setChunkServerId(int id);

    struct ChunkData {
        QByteArray data;
        int nextServerId;
        QHostAddress nextAddress;
        quint16 nextPort;
        bool isLastChunk;
    };
    bool storeChunk(const QString &filename, int chunkId,
                    const QByteArray &data, bool isLastChunk,
                    int nextServerId = -1,
                    const QHostAddress &nextAddress = QHostAddress(),
                    quint16 nextPort = 0);
    ChunkData retrieveChunk(const QString &filename, int chunkId);

    bool hasChunk(const QString &filename, int chunkId) const;
    QString getChunkPath(const QString &filename, int chunkId) const;

    int calculateNextServerDFS();

    bool isConnectedToManager() const {
        return m_managerConnection && m_managerConnection->isConnected();
    }

    void setManagerAddress(const QHostAddress &address) { m_managerAddress = address; }
    void setManagerPort(quint16 port) { m_managerPort = port; }

signals:
    void managerDiscovered();

private:
    int m_chunkServerId;
    QString m_storagePath;
    QDir m_storageDir;

    TcpServer *m_tcpServer = nullptr;
    TcpSocket *m_managerConnection = nullptr;
    QMap<QString, TcpSocket*> m_clientConnections;
    QHostAddress m_managerAddress = QHostAddress::LocalHost;
    quint16 m_managerPort;

    UdpSocket *m_udpSocket = nullptr;

    bool establishTcpConnection();
    bool discoverManagerViaBroadcast();
    void determineHostAddress();
    bool m_peerDiscovered = false;
    int m_discoveryAttempts = 0;

    void sendDiscoveryPacket();

private slots:
    void handleNewTcpConnection(TcpSocket *socket);
    void handleMessage(const TcpMessage &message);
    void handleManagerDisconnected();
    void handleUdpData(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);
};

#endif // CHUNKSERVER_H
