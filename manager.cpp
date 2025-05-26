#include "manager.h"
#include <QDebug>

Manager* Manager::m_instance = nullptr;
QMutex Manager::m_mutex;

Manager* Manager::instance(quint16 port)
{
    if (m_instance == nullptr) {
        QMutexLocker locker(&m_mutex);
        if (m_instance == nullptr) {
            m_instance = new Manager(port);
            m_instance->initialize();
        }
    }
    return m_instance;
}

Manager::Manager(quint16 port, QObject *parent) : Node("manager", port, parent){
    m_udpSocket = new UdpSocket(this);
}

bool Manager::initialize()
{
    bool result = Node::initialize();

    determineHostAddress();

    if (!initializeServer()) {
        qDebug() << "Failed to initialize server for Manager";
        return false;
    }

    if (!initializeUdpDiscovery()) {
        qDebug() << "Failed to initialize UDP discovery for Manager";
        return false;
    }

    qDebug() << "Manager initialized at" << address().toString() << ":" << port();

    initializeServerTree();

    return result;
}

bool Manager::initializeUdpDiscovery()
{
    if (!m_udpSocket->bind(QHostAddress::Any, port())) {
        qDebug() << "Failed to bind Manager UDP socket to port" << port();
        return false;
    }

    connect(m_udpSocket, &UdpSocket::dataReceived, this, &Manager::handleUdpDiscovery);

    qDebug() << "Manager UDP socket bound to Any interface on port" << port();
    return true;
}



void Manager::handleUdpDiscovery(const QByteArray &data, const QHostAddress &sender, quint16 senderPort)
{
    QString message = QString::fromUtf8(data);

    if (message == "DFS_MANAGER_DISCOVERY") {
        qDebug() << "Manager received discovery request from" << sender.toString() << ":" << senderPort;

        QByteArray ackMessage = "DFS_MANAGER_DISCOVERY_ACK";
        bool ackSent = m_udpSocket->sendData(sender, senderPort, ackMessage);

        if (ackSent) {
            qDebug() << "Sent discovery ACK to" << sender.toString() << ":" << senderPort;
        } else {
            qDebug() << "Failed to send discovery ACK to" << sender.toString() << ":" << senderPort;
        }
    }
}

bool Manager::initializeServer()
{
    m_tcpServer = new TcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::Any, port())) {
        qDebug() << "Failed to start TCP server for Manager";
        return false;
    }

    connect(m_tcpServer, &TcpServer::newConnection, this, &Manager::handleNewTcpConnection);

    qDebug() << "Manager TCP server started on ALL interfaces (0.0.0.0):" << port();

    return true;
}

void Manager::handleNewTcpConnection(TcpSocket *socket)
{
    connect(socket, &TcpSocket::messageReceived, this, &Manager::handleMessage);
    connect(socket, &TcpSocket::disconnected, this, &Manager::handleSocketDisconnected);

    m_clientConnections.append(socket);
    qDebug() << "New TCP connection received";
}

void Manager::initializeServerTree()
{
    for (int id = 1; id <= 15; id++) {
        ServerNode server;
        server.id = id;
        server.address = QHostAddress::Any;
        server.port = port() + id;
        server.isOnline = false;

        m_servers[id] = server;
    }

    qDebug() << "Initialized server tree with 15 server slots";
}

bool Manager::activateChunkServer(int serverId, const QHostAddress &address, quint16 port)
{
    if (serverId < 1 || serverId > 15) {
        qDebug() << "Server ID must be between 1 and 15";
        return false;
    }

    ServerNode &server = m_servers[serverId];
    server.address = address;
    server.port = port;
    server.isOnline = true;

    qDebug() << "Activated ChunkServer" << serverId << "at" << address.toString() << ":" << port;

    return true;
}

bool Manager::deactivateChunkServer(int serverId)
{
    if (!m_servers.contains(serverId)) {
        qDebug() << "ChunkServer with ID" << serverId << "not registered";
        return false;
    }

    m_servers[serverId].isOnline = false;
    qDebug() << "Deactivated ChunkServer with ID" << serverId;
    return true;
}

QVector<int> Manager::getAvailableServerIds() const
{
    QVector<int> result;

    for (auto it = m_servers.constBegin(); it != m_servers.constEnd(); ++it) {
        if (it.value().isOnline) {
            result.append(it.key());
        }
    }

    return result;
}

QHostAddress Manager::getServerAddress(int serverId) const
{
    if (!m_servers.contains(serverId)) {
        return QHostAddress();
    }
    return m_servers[serverId].address;
}

quint16 Manager::getServerPort(int serverId) const
{
    if (!m_servers.contains(serverId)) {
        return 0;
    }
    return m_servers[serverId].port;
}

bool Manager::isServerOnline(int serverId) const
{
    if (!m_servers.contains(serverId)) {
        return false;
    }
    return m_servers[serverId].isOnline;
}

int Manager::selectServerForFirstChunk()
{
    static const QVector<int> dfsOrder = {1, 2, 4, 8, 9, 5, 10, 11, 3, 6, 12, 13, 7, 14, 15};

    for (int serverId : dfsOrder) {
        if (isServerOnline(serverId)) {
            return serverId;
        }
    }

    qDebug() << "No online servers available for file storage";
    return -1;
}

int Manager::findNextServerDFS(int currentServerId)
{
    static const QVector<int> dfsOrder = {1, 2, 4, 8, 9, 5, 10, 11, 3, 6, 12, 13, 7, 14, 15};

    int currentIndex = dfsOrder.indexOf(currentServerId);
    if (currentIndex < 0 || currentIndex >= dfsOrder.size() - 1) {
        return -1;
    }

    for (int i = currentIndex + 1; i < dfsOrder.size(); ++i) {
        int serverId = dfsOrder[i];
        if (isServerOnline(serverId)) {
            return serverId;
        }
    }

    return -1;
}

int Manager::calculateChunkCount(qint64 fileSize) const
{
    return FileManager::calculateChunkCount(fileSize, m_chunkSize);
}

bool Manager::registerFile(const QString &filename, qint64 fileSize)
{
    if (m_files.contains(filename)) {
        qDebug() << "File" << filename << "already registered";
        return false;
    }

    FileInfo info;
    info.filename = filename;
    info.size = fileSize;
    info.chunkSize = m_chunkSize;
    info.chunkCount = calculateChunkCount(fileSize);
    info.creationTime = QDateTime::currentDateTime();
    info.lastModified = info.creationTime;
    info.firstServerId = -1;

    m_files[filename] = info;
    qDebug() << "Registered file" << filename << "of size" << fileSize
             << "requiring" << info.chunkCount << "chunks of" << m_chunkSize << "bytes each";
    return true;
}

bool Manager::setFirstChunkLocation(const QString &filename, int serverId)
{
    if (!m_files.contains(filename)) {
        qDebug() << "File" << filename << "not registered";
        return false;
    }

    if (!m_servers.contains(serverId)) {
        qDebug() << "Invalid server ID:" << serverId;
        return false;
    }

    FileInfo &info = m_files[filename];
    info.firstServerId = serverId;
    info.lastModified = QDateTime::currentDateTime();

    qDebug() << "Set first chunk of file" << filename << "to server" << serverId << "at" << m_servers[serverId].address.toString() << ":" << m_servers[serverId].port;
    return true;
}

bool Manager::unregisterFile(const QString &filename)
{
    if (!m_files.contains(filename)) {
        qDebug() << "File" << filename << "not registered";
        return false;
    }

    m_files.remove(filename);
    qDebug() << "Unregistered file" << filename
             << "(Note: chunks still exist on servers and must be cleaned up separately)";
    return true;
}

bool Manager::fileExists(const QString &filename) const
{
    return m_files.contains(filename);
}

Manager::FileInfo Manager::getFileInfo(const QString &filename) const
{
    if (!m_files.contains(filename)) {
        qDebug() << "File" << filename << "not found";
        return FileInfo();
    }

    return m_files[filename];
}

void Manager::handleMessage(const TcpMessage &message)
{
    TcpSocket *socket = qobject_cast<TcpSocket*>(sender());
    switch (message.type()) {
        case TcpMessage::RegisterChunkServer: {
            int serverId = message.getData("serverId").toInt();

            QHostAddress tcpAddress(message.getData("tcpAddress").toString());
            quint16 port = message.getData("port").toUInt();

            qDebug() << "Received ChunkServer registration for server" << serverId << "at" << tcpAddress.toString() << ":" << port;

            bool success = activateChunkServer(serverId, tcpAddress, port);

            if (success && m_servers.contains(serverId)) {
                m_clientConnections.removeOne(socket);
                m_chunkServerConnections[serverId] = socket;
            }

            TcpMessage response;
            response.setType(TcpMessage::RegisterChunkServerResponse);
            response.setData("success", success);
            response.setData("serverId", serverId);

            socket->sendMessage(response);
            break;
        }
        case TcpMessage::RegisterFile: {
            QString filename = message.getData("filename").toString();
            qint64 fileSize = message.getData("fileSize").toString().toLongLong();

            qDebug() << "Received file registration for" << filename << "of size" << fileSize << "bytes";

            bool registered = registerFile(filename, fileSize);

            int firstServerId = selectServerForFirstChunk();

            TcpMessage response;
            response.setType(TcpMessage::RegisterFileResponse);
            response.setData("filename", filename);

            if (registered && firstServerId > 0) {
                setFirstChunkLocation(filename, firstServerId);

                FileInfo info = getFileInfo(filename);

                QHostAddress serverAddr = getServerAddress(firstServerId);
                quint16 serverPort = getServerPort(firstServerId);

                response.setData("success", true);
                response.setData("serverId", firstServerId);
                response.setData("serverAddress", serverAddr.toString());
                response.setData("serverPort", serverPort);
                response.setData("chunkSize", m_chunkSize);
                response.setData("chunkCount", info.chunkCount);
            } else {
                response.setData("success", false);
                response.setData("error", "Failed to register file or no servers available");
            }

            socket->sendMessage(response);
            break;
        }
        case TcpMessage::GetFileInfo: {

            QString filename = message.getData("filename").toString();

            qDebug() << "Received file info request for" << filename;

            TcpMessage response;
            response.setType(TcpMessage::GetFileInfoResponse);
            response.setData("filename", filename);

            if (fileExists(filename)) {
                FileInfo info = getFileInfo(filename);
                int serverId = info.firstServerId;

                response.setData("success", true);
                response.setData("fileSize", QString::number(info.size));
                response.setData("chunkCount", info.chunkCount);
                response.setData("serverId", serverId);
                response.setData("serverAddress", getServerAddress(serverId).toString());
                response.setData("serverPort", getServerPort(serverId));
            } else {
                response.setData("success", false);
                response.setData("error", "File not found");
            }

            socket->sendMessage(response);
            break;
        }
        default:
            qDebug() << "Received unknown TCP message type:" << message.type();
            break;
    }
}

void Manager::handleSocketDisconnected()
{
    TcpSocket *socket = qobject_cast<TcpSocket*>(sender());

    if (m_clientConnections.contains(socket)) {
        m_clientConnections.removeOne(socket);
        socket->deleteLater();
        qDebug() << "Client disconnected";
        return;
    }

    for (auto it = m_chunkServerConnections.begin(); it != m_chunkServerConnections.end(); ++it) {
        if (it.value() == socket) {
            int serverId = it.key();

            deactivateChunkServer(serverId);

            m_chunkServerConnections.remove(serverId);
            socket->deleteLater();

            qDebug() << "ChunkServer" << serverId << "disconnected";
            return;
        }
    }
}

void Manager::shutdown()
{
    qDebug() << "Manager shutting down...";

    prepareShutdown();
    this->disconnect();

    QMap<int, TcpSocket*> serverConnections = m_chunkServerConnections;
    m_chunkServerConnections.clear();

    for (auto it = serverConnections.begin(); it != serverConnections.end(); ++it) {
        TcpSocket* socket = it.value();
        if (socket) {
            socket->disconnect();
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }

    QCoreApplication::processEvents();

    QList<TcpSocket*> clientSockets = m_clientConnections;
    m_clientConnections.clear();

    for (TcpSocket* socket : clientSockets) {
        if (socket) {
            socket->disconnect();
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }

    QCoreApplication::processEvents();

    QThread::msleep(100);

    if (m_tcpServer) {
        m_tcpServer->disconnect();
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

    m_servers.clear();
    m_files.clear();

    Node::shutdown();
}

void Manager::determineHostAddress()
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
                    qDebug() << "Manager found LAN interface:" << interface.name()
                             << "with address:" << addrStr;
                    goto found;
                }
            }
        }
    }

found:
    setAddress(hostAddress);
    setTcpAddress(hostAddress);
    qDebug() << "Manager host address set to:" << hostAddress.toString();
}
