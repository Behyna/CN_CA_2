#include "chunkserver.h"
#include "udpsocket.h"
#include <QStorageInfo>
#include <QDebug>
#include <QStandardPaths>

//tODO late change

ChunkServer::ChunkServer(QObject *parent) : Node("chunk-server-1", 9001, parent), m_chunkServerId(1)
{
    m_udpSocket = new UdpSocket(this);
}

ChunkServer::~ChunkServer(){
    qDebug() << "Destroying ChunkServer" << m_chunkServerId;
}

bool ChunkServer::initialize()
{
    bool result = Node::initialize();

    determineHostAddress();

    if (!m_udpSocket->bind(address(), port())) {
        qDebug() << "Failed to bind ChunkServer UDP socket to" << address().toString() << ":" << port();
        return false;
    }

    connect(m_udpSocket, &UdpSocket::dataReceived, this, &ChunkServer::handleUdpData);

    if (!initializeServer()) {
        qDebug() << "Failed to initialize TCP for ChunkServer";
        return false;
    }

    if (discoverManagerViaBroadcast()) {
        qDebug() << "Manager discovered successfully via broadcast";
    } else {
        qDebug() << "Manager discovery failed, using default localhost";
        setManagerAddress(QHostAddress::LocalHost);
    }

    if (!connectToManager()) {
        qDebug() << "Warning: Failed to connect to Manager via TCP";
    }

    qDebug() << "ChunkServer" << m_chunkServerId << "initialized at" << address().toString() << ":" << port();
    qDebug() << "UDP socket listening on same port:" << port();
    qDebug() << "Storage path:" << m_storagePath;
    qDebug() << "Available space:" << availableSpace() / (1024 * 1024) << "MB";

    return result;
}

void ChunkServer::shutdown()
{
    qDebug() << "ChunkServer" << m_chunkServerId << "shutting down...";

    if (m_isShuttingDown) {
        return;
    }
    m_isShuttingDown = true;

    this->disconnect();
    this->blockSignals(true);

    QCoreApplication::processEvents();

    QList<TcpSocket*> clientSockets;
    for (auto it = m_clientConnections.begin(); it != m_clientConnections.end(); ++it) {
        clientSockets.append(it.value());
    }
    m_clientConnections.clear();

    for (TcpSocket* socket : clientSockets) {
        if (socket) {
            socket->disconnect();
            socket->blockSignals(true);
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }

    QCoreApplication::processEvents();

    if (m_managerConnection) {
        m_managerConnection->disconnect();
        m_managerConnection->blockSignals(true);
        m_managerConnection->disconnectFromHost();
        m_managerConnection->deleteLater();
        m_managerConnection = nullptr;
    }

    QCoreApplication::processEvents();
    QThread::msleep(50);

    if (m_tcpServer) {
        m_tcpServer->disconnect();
        m_tcpServer->blockSignals(true);
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
    }

    if (m_udpSocket) {
        m_udpSocket->disconnect();
        m_udpSocket->blockSignals(true);
        m_udpSocket->deleteLater();
        m_udpSocket = nullptr;
    }

    QCoreApplication::processEvents();

    Node::shutdown();
}

void ChunkServer::setChunkServerId(int id)
{
    if (id <= 0 || id > 15) {
        qWarning() << "Invalid chunk server ID:" << id << "- Must be between 1 and 15";
        return;
    }

    m_chunkServerId = id;
    setNodeId(QString("chunk-server-%1").arg(m_chunkServerId));

    setPort(9000 + m_chunkServerId);

    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_storagePath = QString("%1/CHUNK-%2").arg(baseDir).arg(m_chunkServerId);
    setStoragePath(m_storagePath);
}

bool ChunkServer::initializeServer()
{
    m_tcpServer = new TcpServer(this);

    if (!m_tcpServer->listen(QHostAddress::Any, port())) {
        qDebug() << "Failed to start TCP server for ChunkServer";
        return false;
    }

    connect(m_tcpServer, &TcpServer::newConnection, this, &ChunkServer::handleNewTcpConnection);

    qDebug() << "ChunkServer TCP server started on" << "0.0.0.0" << ":" << port();

    return true;
}

bool ChunkServer::connectToManager()
{
    qDebug() << "Starting direct TCP connection to Manager";
    return establishTcpConnection();
}

bool ChunkServer::establishTcpConnection()
{
    m_managerConnection = new TcpSocket(this);

    // qDebug() << this->m_managerAddress;
    // qDebug() << this ->m_managerPort;
    // qDebug() << this->m_port;

    // // TODO: change later

    // m_managerAddress = QHostAddress("192.168.1.4");
    // m_managerPort = 5554;


    // qDebug() << this->m_managerAddress;
    // qDebug() << this ->m_managerPort;
    // qDebug() << this->m_port;

    connect(m_managerConnection, &TcpSocket::connected, this, [this]() {
        qDebug() << "ChunkServer" << m_chunkServerId << "connected to Manager via TCP";

        TcpMessage regMsg;
        regMsg.setType(TcpMessage::RegisterChunkServer);
        regMsg.setData("serverId", m_chunkServerId);
        regMsg.setData("tcpAddress", tcpAddress().toString());
        regMsg.setData("port", port());

        m_managerConnection->sendMessage(regMsg);
    });

    connect(m_managerConnection, &TcpSocket::messageReceived, this, &ChunkServer::handleMessage);
    connect(m_managerConnection, &TcpSocket::disconnected, this, &ChunkServer::handleManagerDisconnected);

    return m_managerConnection->connectToHost(m_managerAddress, m_managerPort);
}

void ChunkServer::handleNewTcpConnection(TcpSocket *socket)
{
    connect(socket, &TcpSocket::messageReceived, this, &ChunkServer::handleMessage);
    connect(socket, &TcpSocket::disconnected, socket, [this, socket]() {
        for (auto it = m_clientConnections.begin(); it != m_clientConnections.end(); ++it) {
            if (it.value() == socket) {
                m_clientConnections.remove(it.key());
                socket->deleteLater();
                break;
            }
        }
    });

    qDebug() << "New client connected to ChunkServer via TCP";
}

void ChunkServer::setStoragePath(const QString &path)
{
    m_storagePath = path;
    m_storageDir = QDir(path);

    if (!m_storageDir.exists()) {
        m_storageDir.mkpath(".");
        qDebug() << "Created storage directory:" << path;
    }
}

qint64 ChunkServer::availableSpace() const
{
    QStorageInfo storage(m_storagePath);
    return storage.bytesAvailable();
}

qint64 ChunkServer::totalSpace() const
{
    QStorageInfo storage(m_storagePath);
    return storage.bytesTotal();
}

void ChunkServer::handleMessage(const TcpMessage &message)
{
    TcpSocket *socket = qobject_cast<TcpSocket*>(sender());
    switch (message.type()) {
        case TcpMessage::RegisterChunkServerResponse: {
            bool success = message.getData("success").toBool();
            int serverId = message.getData("serverId").toInt();

            qDebug() << "Received registration response from Manager: "
                     << (success ? "Success" : "Failed") << "for server" << serverId;

            break;
        }

        case TcpMessage::StoreChunk: {
            QString filename = message.getData("filename").toString();
            int chunkId = message.getData("chunkId").toInt();
            bool isLastChunk = message.getData("isLastChunk").toBool();
            QByteArray chunkData = message.getData("chunkData").toByteArray();

            qDebug() << "Received request to store chunk" << chunkId
                     << "of file" << filename << "(" << chunkData.size() << "bytes)";

            int nextServerId = -1;
            QHostAddress nextServerAddr;
            quint16 nextServerPort = 0;

            if (!isLastChunk) {
                nextServerId = calculateNextServerDFS();
                nextServerAddr = tcpAddress();
                nextServerPort = 9000 + nextServerId;
            }

            bool stored = storeChunk(filename, chunkId, chunkData, isLastChunk,
                                     nextServerId, nextServerAddr, nextServerPort);

            TcpMessage response;
            response.setType(TcpMessage::StoreChunkResponse);
            response.setData("success", stored);
            response.setData("filename", filename);
            response.setData("chunkId", chunkId);

            if (stored && !isLastChunk) {
                response.setData("nextServerId", nextServerId);
                response.setData("nextServerAddr", nextServerAddr.toString());
                response.setData("nextServerPort", nextServerPort);
            }

            socket->sendMessage(response);
            break;
        }
        case TcpMessage::GetChunk: {
            QString filename = message.getData("filename").toString();
            int chunkId = message.getData("chunkId").toInt();

            qDebug() << "Received request to retrieve chunk" << chunkId << "of file" << filename;

            ChunkData chunk = retrieveChunk(filename, chunkId);

            TcpMessage response;
            response.setType(TcpMessage::GetChunkResponse);
            response.setData("filename", filename);
            response.setData("chunkId", chunkId);

            if (chunk.data.isEmpty()) {
                response.setData("success", false);
                response.setData("error", "Chunk not found");
                qDebug() << "CHUNK" << chunkId << "IS CORRUPTED - Sending 'chunk corrupt' response";
            } else {
                response.setData("success", true);
                response.setData("chunkData", chunk.data);

                if (!chunk.isLastChunk) {
                    response.setData("nextServerId", chunk.nextServerId);
                    response.setData("nextServerAddr", chunk.nextAddress.toString());
                    response.setData("nextServerPort", chunk.nextPort);
                }
                qDebug() << "Sending recovered chunk" << chunkId << "(" << chunk.data.size() << "bytes)";
            }

            socket->sendMessage(response);
            break;
        }
        default:
            qDebug() << "Received unknown TCP message type:" << message.type();
            break;
    }
}

void ChunkServer::handleManagerDisconnected()
{
    if (m_isShuttingDown) {
        qDebug() << "Disconnected from Manager during shutdown, not attempting to reconnect";
        return;
    }

    qDebug() << "Disconnected from Manager, will attempt to reconnect";
    QTimer::singleShot(5000, this, &ChunkServer::connectToManager);
}

bool ChunkServer::storeChunk(const QString &filename, int chunkId, const QByteArray &data, bool isLastChunk, int nextServerId, const QHostAddress &nextAddress, quint16 nextPort)
{
    QString chunkPath = getChunkPath(filename, chunkId);

    QFile file(chunkPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open chunk file for writing:" << chunkPath;
        return false;
    }

    QDataStream stream(&file);

    QByteArray encodedData = ErrorCorrection::encodeWithRedundancy(data);

    qDebug() << "ChunkServer: Storing chunk" << chunkId
             << "- Original:" << data.size() << "bytes"
             << "- Encoded:" << encodedData.size() << "bytes";

    stream << encodedData;
    stream << nextServerId;
    stream << nextAddress.toString();
    stream << nextPort;
    stream << isLastChunk;

    file.close();

    qDebug() << "Stored chunk" << chunkId << "of file" << filename << "with Reed-Solomon encoding and noise injection";

    if (isLastChunk) {
        qDebug() << "This is the final chunk of the file";
    } else {
        qDebug() << "Next chunk is on server" << nextServerId << "at" << nextAddress.toString() << ":" << nextPort;
    }

    return true;

}

ChunkServer::ChunkData ChunkServer::retrieveChunk(const QString &filename, int chunkId)
{
    ChunkData result;

    QString chunkPath = getChunkPath(filename, chunkId);

    QFile file(chunkPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open chunk file for reading:" << chunkPath;
        return result;
    }

    QDataStream stream(&file);

    QByteArray cleanEncodedData;
    stream >> cleanEncodedData;

    stream >> result.nextServerId;

    QString nextAddressStr;
    stream >> nextAddressStr;
    result.nextAddress = QHostAddress(nextAddressStr);

    stream >> result.nextPort;
    stream >> result.isLastChunk;

    file.close();

    if (ErrorCorrection::isErrorInjectionEnabled()) {
        qDebug() << "ChunkServer: Simulating transmission errors for chunk" << chunkId;
        QByteArray corruptedEncodedData = ErrorCorrection::injectNoise(cleanEncodedData);

        int bitErrors = ErrorCorrection::countBitErrors(cleanEncodedData, corruptedEncodedData);
        qDebug() << "ChunkServer: Injected" << bitErrors << "bit errors during transmission simulation";

        ErrorCorrection::CorrectionResult correction = ErrorCorrection::decodeAndCorrect(corruptedEncodedData);

        if (correction.isRecoverable) {
            result.data = correction.data;
            qDebug() << "Retrieved chunk" << chunkId
                     << "- Successfully recovered from" << bitErrors << "bit errors";
        } else {
            result.data = QByteArray();
            qDebug() << "Failed to retrieve chunk" << chunkId
                     << "- Corruption too severe to recover";
        }
    } else {
        ErrorCorrection::CorrectionResult correction = ErrorCorrection::decodeAndCorrect(cleanEncodedData);
        result.data = correction.data;
    }

    return result;
}

bool ChunkServer::hasChunk(const QString &filename, int chunkId) const
{
    QString chunkPath = getChunkPath(filename, chunkId);
    return QFile::exists(chunkPath);
}

QString ChunkServer::getChunkPath(const QString &filename, int chunkId) const
{
    return QString("%1/%2_chunk%3").arg(m_storagePath, filename, QString::number(chunkId));
}

int ChunkServer::calculateNextServerDFS() {
    static const std::vector<int> dfsOrder = {1, 2, 4, 8, 9, 5, 10, 11, 3, 6, 12, 13, 7, 14, 15};

    auto it = std::find(dfsOrder.begin(), dfsOrder.end(), m_chunkServerId);

    if (it != dfsOrder.end()) {
        if (std::next(it) == dfsOrder.end()) {
            return dfsOrder.front();
        } else {
            return *std::next(it);
        }
    }

    return 1;
}

void ChunkServer::determineHostAddress()
{
    QHostAddress hostAddress = QHostAddress::LocalHost;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags() & QNetworkInterface::IsLoopBack) {
            continue;
        }

        if (!(interface.flags() & QNetworkInterface::IsUp) ||
            !(interface.flags() & QNetworkInterface::IsRunning)) {
            continue;
        }

        QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            QHostAddress addr = entry.ip();

            if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                QString addrStr = addr.toString();

                if (addrStr.startsWith("192.168.") ||   // 192.168.0.0/16
                    addrStr.startsWith("10.") ||        // 10.0.0.0/8
                    (addrStr.startsWith("172.") &&      // 172.16.0.0/12
                     addrStr.split('.')[1].toInt() >= 16 &&
                     addrStr.split('.')[1].toInt() <= 31)) {

                    hostAddress = addr;
                    qDebug() << "Found LAN interface:" << interface.name()
                             << "with address:" << addrStr;
                    goto found;
                }
            }
        }
    }

found:
    setAddress(hostAddress);
    setTcpAddress(hostAddress);
    qDebug() << "ChunkServer host address set to:" << hostAddress.toString();
}

void ChunkServer::sendDiscoveryPacket()
{
    if (m_peerDiscovered || m_discoveryAttempts >= 5) {
        return;
    }

    m_discoveryAttempts++;
    qDebug() << "Sending discovery packet attempt" << m_discoveryAttempts << "to port" << m_managerPort;

    QByteArray discoveryMessage = "DFS_MANAGER_DISCOVERY";

    QList<QHostAddress> broadcastAddresses;
    broadcastAddresses.append(QHostAddress::Broadcast);

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags() & QNetworkInterface::IsLoopBack) continue;
        if (!(interface.flags() & QNetworkInterface::IsUp)) continue;

        QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                QHostAddress broadcast = entry.broadcast();
                if (!broadcast.isNull()) {
                    broadcastAddresses.append(broadcast);
                }
            }
        }
    }

    bool messageSent = false;
    for (const QHostAddress &broadcastAddr : broadcastAddresses) {
        bool sent = m_udpSocket->sendData(broadcastAddr, m_managerPort, discoveryMessage);
        if (sent) {
            qDebug() << "Successfully sent discovery to" << broadcastAddr.toString();
            messageSent = true;
        }
    }

    if (messageSent && !m_peerDiscovered) {
        QTimer::singleShot(1000, this, [this]() {
            sendDiscoveryPacket();
        });
    }
}

void ChunkServer::handleUdpData(const QByteArray &data, const QHostAddress &sender, quint16 senderPort)
{
    QString message = QString::fromUtf8(data);

    if (message == "DFS_CLIENT_DISCOVERY") {
        qDebug() << "ChunkServer" << m_chunkServerId
                 << "received discovery request from Client at" << sender.toString() << ":" << senderPort;

        // Send discovery ACK to Client
        QByteArray ackMessage = "DFS_CLIENT_DISCOVERY_ACK";
        bool ackSent = m_udpSocket->sendData(sender, senderPort, ackMessage);

        if (ackSent) {
            qDebug() << "Sent discovery ACK to Client at" << sender.toString() << ":" << senderPort;
        } else {
            qDebug() << "Failed to send discovery ACK to Client";
        }
    }
    else if (data == UdpSocket::PING_MESSAGE) {
        qDebug() << "Received PING from client" << sender.toString() << ":" << senderPort;
        m_udpSocket->sendPong(sender, senderPort);
    }
    else if (message == "DFS_MANAGER_DISCOVERY_ACK") {
        qDebug() << "Manager discovered at" << sender.toString();
        setManagerAddress(sender);
        m_peerDiscovered = true;
        emit managerDiscovered();
    }
}

bool ChunkServer::discoverManagerViaBroadcast()
{
    qDebug() << "ChunkServer" << m_chunkServerId << "attempting Manager discovery via UDP broadcast...";

    m_peerDiscovered = false;
    m_discoveryAttempts = 0;

    sendDiscoveryPacket();

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    connect(this, &ChunkServer::managerDiscovered, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start(10000);
    loop.exec();

    if (m_peerDiscovered) {
        qDebug() << "Discovery successful!";
        return true;
    } else {
        qDebug() << "Discovery failed after timeout";
        return false;
    }
}
