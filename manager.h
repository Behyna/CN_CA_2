#ifndef MANAGER_H
#define MANAGER_H

#include "node.h"
#include "tcpsocket.h"
#include <QMap>
#include <QMutex>
#include <QThread>
#include "filemanager.h"
#include "udpsocket.h"
#include <QNetworkInterface>

class Manager : public Node
{
    Q_OBJECT

public:
    static Manager* instance(quint16 port);
    static void cleanupInstance() {
        QMutexLocker locker(&m_mutex);
        if (m_instance) {
            m_instance->disconnect();
            m_instance->blockSignals(true);
            delete m_instance;
            m_instance = nullptr;
        }
    }

    bool initialize() override;
    void shutdown() override;

    bool initializeServer();

    bool activateChunkServer(int serverId, const QHostAddress &address, quint16 port);
    bool deactivateChunkServer(int serverId);
    QVector<int> getAvailableServerIds() const;
    QHostAddress getServerAddress(int serverId) const;
    quint16 getServerPort(int serverId) const;
    bool isServerOnline(int serverId) const;

    int selectServerForFirstChunk();
    int findNextServerDFS(int currentServerId);

    struct FileInfo {
        QString filename;
        qint64 size;
        int chunkCount;
        qint64 chunkSize;
        QDateTime creationTime;
        QDateTime lastModified;

        int firstServerId;
    };

    bool registerFile(const QString &filename, qint64 fileSize);
    bool unregisterFile(const QString &filename);
    bool fileExists(const QString &filename) const;
    FileInfo getFileInfo(const QString &filename) const;

    bool setFirstChunkLocation(const QString &filename, int serverId);
    int calculateChunkCount(qint64 fileSize) const;

    qint64 getChunkSize() const { return m_chunkSize; }
    void setChunkSize(qint64 size) { m_chunkSize = size; }

protected:
    explicit Manager(quint16 port, QObject *parent = nullptr);

private:
    static Manager* m_instance;
    static QMutex m_mutex;

    qint64 m_chunkSize = 8 * 1024;

    struct ServerNode {
        int id;
        QHostAddress address;
        quint16 port;
        bool isOnline;
    };
    QMap<int, ServerNode> m_servers;

    TcpServer *m_tcpServer = nullptr;
    UdpSocket *m_udpSocket = nullptr;
    QMap<int, TcpSocket*> m_chunkServerConnections;
    QList<TcpSocket*> m_clientConnections;
    QMap<QString, FileInfo> m_files;

    void initializeServerTree();
    bool initializeUdpDiscovery();
    void determineHostAddress();

private slots:
    void handleNewTcpConnection(TcpSocket *socket);
    void handleMessage(const TcpMessage &message);
    void handleSocketDisconnected();
    void handleUdpDiscovery(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);
};

#endif // MANAGER_H
