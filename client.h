#ifndef CLIENT_H
#define CLIENT_H

#include "node.h"
#include <QMutex>
#include <QFileInfo>
#include <QTimer>
#include <QThread>
#include "tcpsocket.h"
#include "filemanager.h"
#include "manager.h"
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QTimer>
#include <QProcess>
#include <QThread>

class Client : public Node
{
    Q_OBJECT
public:
    static Client* instance(quint16 managerPort);
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

    QHostAddress managerAddress() const { return m_managerAddress; }
    quint16 managerPort() const { return m_managerPort; }

    void setManagerAddress(const QHostAddress &address) { m_managerAddress = address; }
    void setManagerPort(quint16 port) { m_managerPort = port; }

    bool connectToManager();

    QList<QByteArray> splitFile(const QString &filePath, qint64 chunkSize = 8 * 1024);
    bool reassembleFile(const QString &outputPath, const QList<QByteArray> &chunks);

    bool storeFile(const QString &filePath);
    bool retrieveFile(const QString &filename, const QString &savePath);

    enum OperationStatus {
        Pending,
        InProgress,
        Completed,
        Failed
    };

    struct FileOperation {
        QString filename;
        QString localPath;
        QList<QByteArray> chunks;
        int currentChunkIndex;
        bool isStore;
        OperationStatus status;
        QString errorMessage;
        QMap<int, QHostAddress> serverAddresses;
        QMap<int, quint16> serverPorts;
    };

    bool isOperationComplete(const QString& filename) const {
        if (!m_operations.contains(filename)) {
            return false;
        }
        return m_operations[filename].status == Completed;
    }

    bool hasOperationFailed(const QString& filename) const {
        if (!m_operations.contains(filename)) {
            return false;
        }
        return m_operations[filename].status == Failed;
    }

    QString getOperationError(const QString& filename) const {
        if (!m_operations.contains(filename)) {
            return "Unknown file";
        }
        return m_operations[filename].errorMessage;
    }

protected:
    explicit Client(quint16 managerPort, QObject *parent = nullptr);
    virtual ~Client();

private slots:
    void handleMessage(const TcpMessage &message);
    void handleManagerDisconnected();

    void continueStoreOperation(const QString &filename, int serverId);
    void continueRetrieveOperation(const QString &filename, int serverId);
    void handleChunkServerError();

private:
    static Client* m_instance;
    static QMutex m_mutex;
    TcpSocket *m_managerConnection = nullptr;

    bool connectToChunkServer(int serverId, const QHostAddress &serverAddr, quint16 serverPort);

    QMap<int, TcpSocket*> m_serverConnections;

    QHostAddress m_managerAddress= QHostAddress::LocalHost;
    quint16 m_managerPort;

    QMap<QString, FileOperation> m_operations;

    void determineHostAddress();

    UdpSocket *m_udpSocket = nullptr;
    bool m_udpHolePunchSuccess = false;
    bool performUdpHolePunchingToServer(const QHostAddress &serverAddr, quint16 serverPort);


    bool connectToChunkServerFallback(int serverId, const QHostAddress &serverAddr, quint16 serverPort);
};

#endif
