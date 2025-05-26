#include "client.h"
#include <QDebug>
#include "qevent.h"
#include <QLocalSocket>
#include <QNetworkProxy>


Client* Client::m_instance = nullptr;
QMutex Client::m_mutex;

Client* Client::instance(quint16 managerPort)
{
    if (m_instance == nullptr) {
        QMutexLocker locker(&m_mutex);
        if (m_instance == nullptr) {
            m_instance = new Client(managerPort);
            m_instance->initialize();
        }
    }
    return m_instance;
}

Client::Client(quint16 managerPort, QObject *parent) : Node("client", 0, parent)
{
    setManagerPort(managerPort);
}

Client::~Client()
{
    shutdown();
}

bool Client::initialize()
{
    bool result = Node::initialize();

    determineHostAddress();

    if (!connectToManager()) {
        qDebug() << "Failed to initialize TCP socket for Client";
        return false;
    }

    qDebug() << "Client initialized at" << address().toString() << ":" << port();
    qDebug() << "Manager connection:" << m_managerAddress.toString() << ":" << m_managerPort;
    return result;
}


bool Client::connectToManager()
{
    m_managerConnection = new TcpSocket(this);

    connect(m_managerConnection, &TcpSocket::connected, this, [this]() {
        qDebug() << "Connected to Manager via TCP";
    });
    
    connect(m_managerConnection, &TcpSocket::disconnected, this, &Client::handleManagerDisconnected);
    connect(m_managerConnection, &TcpSocket::messageReceived, this, &Client::handleMessage);

    bool started = m_managerConnection->connectToHost(m_managerAddress, m_managerPort);

    if (!started) {
        qDebug() << "Failed to start connection to Manager via TCP";
        m_managerConnection->deleteLater();
        m_managerConnection = nullptr;
        return false;
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    
    connect(m_managerConnection, &TcpSocket::connected, &loop, &QEventLoop::quit);

    connect(m_managerConnection, QOverload<QAbstractSocket::SocketError>::of(&TcpSocket::error), 
            [&loop](QAbstractSocket::SocketError) {
                loop.quit();
            });
            
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeout.start(3000); // 3 second timeout
    loop.exec();
    
    if (!m_managerConnection->isConnected()) {
        qDebug() << "Connection to manager failed to establish within timeout";
        qDebug() << "Socket error:" << m_managerConnection->errorString();
        m_managerConnection->deleteLater();
        m_managerConnection = nullptr;
        return false;
    }
    
    qDebug() << "Connected to Manager via TCP";
    return true;
}

void Client::handleManagerDisconnected()
{
    qDebug() << "Disconnected from Manager, will attempt to reconnect";

    QTimer::singleShot(5000, this, &Client::connectToManager);
}

void Client::shutdown()
{
    qDebug() << "Client shutting down...";

    if (m_isShuttingDown) {
        return;
    }

    this->disconnect();
    this->blockSignals(true);

    prepareShutdown();

    QCoreApplication::processEvents();

    QMap<int, TcpSocket*> serverConnections = m_serverConnections;
    m_serverConnections.clear();

    for (auto it = serverConnections.begin(); it != serverConnections.end(); ++it) {
        TcpSocket* socket = it.value();
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

    Node::shutdown();
}

QList<QByteArray> Client::splitFile(const QString &filePath, qint64 chunkSize)
{
    return FileManager::splitFile(filePath, chunkSize);
}

bool Client::reassembleFile(const QString &outputPath, const QList<QByteArray> &chunks)
{
    return FileManager::reassembleFile(outputPath, chunks);
}

bool Client::storeFile(const QString &filePath)
{

    if (!m_managerConnection) {
        qDebug() << "Manager connection is null";
        return false;
    }
    
    if (!m_managerConnection->isConnected()) {
        qDebug() << "Not connected to Manager - connection state:" << m_managerConnection->state();
        qDebug() << "Attempting to reconnect...";
        
        if (!connectToManager()) {
            qDebug() << "Failed to reconnect to manager";
            return false;
        }
    }

    // Rest of your code remains the same
    QString filename = FileManager::getFileName(filePath);
    qint64 fileSize = FileManager::getFileSize(filePath);


    if (fileSize < 0) {
        qDebug() << "File does not exist or cannot be read:" << filePath;
        return false;
    }

    qDebug() << "Storing file:" << filename << "of size" << fileSize << "bytes";

    FileOperation operation;
    operation.filename = filename;
    operation.localPath = filePath;
    operation.currentChunkIndex = 0;
    operation.isStore = true;
    operation.status = Pending;

    m_operations[filename] = operation;

    TcpMessage regMsg;
    regMsg.setType(TcpMessage::RegisterFile);
    regMsg.setData("filename", filename);
    regMsg.setData("fileSize", QString::number(fileSize));

    bool sent = m_managerConnection->sendMessage(regMsg);
    if (!sent) {
        qDebug() << "Failed to send file registration message";
        m_operations.remove(filename);
        return false;
    }

    m_operations[filename].status = InProgress;
    qDebug() << "File registration message sent to Manager";

    return true;
}

bool Client::retrieveFile(const QString &filename, const QString &savePath)
{
    if (!m_managerConnection || !m_managerConnection->isConnected()) {
        qDebug() << "Not connected to Manager";
        return false;
    }

    qDebug() << "Retrieving file:" << filename << "to" << savePath;

    FileOperation operation;
    operation.filename = filename;
    operation.localPath = savePath;
    operation.currentChunkIndex = 0;
    operation.isStore = false;
    operation.status = Pending;

    m_operations[filename] = operation;

    TcpMessage infoMsg;
    infoMsg.setType(TcpMessage::GetFileInfo);
    infoMsg.setData("filename", filename);

    bool sent = m_managerConnection->sendMessage(infoMsg);
    if (!sent) {
        qDebug() << "Failed to send file info request";
        m_operations.remove(filename);
        return false;
    }

    m_operations[filename].status = InProgress;
    qDebug() << "File info request sent to Manager";

    return true;
}

void Client::handleMessage(const TcpMessage &message)
{
    TcpSocket *socket = qobject_cast<TcpSocket*>(sender());
    if (socket == m_managerConnection){
        switch (message.type()) {
            case TcpMessage::RegisterFileResponse: {
                bool success = message.getData("success").toBool();
                QString filename = message.getData("filename").toString();

                if (!m_operations.contains(filename)) {
                    qDebug() << "No pending operation for file:" << filename;
                    return;
                }

                if (!success) {
                    QString error = message.getData("error").toString();
                    qDebug() << "Failed to register file:" << filename << "-" << error;

                    m_operations[filename].status = Failed;
                    m_operations[filename].errorMessage = error;
                    return;
                }

                int serverId = message.getData("serverId").toInt();
                QHostAddress serverAddr(message.getData("serverAddress").toString());
                quint16 serverPort = message.getData("serverPort").toUInt();
                qint64 chunkSize = message.getData("chunkSize").toLongLong();
                int chunkCount = message.getData("chunkCount").toInt();

                m_operations[filename].serverAddresses[serverId] = serverAddr;
                m_operations[filename].serverPorts[serverId] = serverPort;

                qDebug() << "File" << filename << "registered. First chunk on server" << serverId
                         << "at" << serverAddr.toString() << ":" << serverPort;

                m_operations[filename].chunks = FileManager::splitFile(m_operations[filename].localPath, chunkSize);

                if (m_operations[filename].chunks.isEmpty()) {
                    qDebug() << "FileManager failed to split file:" << m_operations[filename].localPath;
                    m_operations[filename].status = Failed;
                    m_operations[filename].errorMessage = "File splitting failed";
                    return;
                }

                qDebug() << "File split into" << m_operations[filename].chunks.size() << "chunks using FileManager";

                continueStoreOperation(filename, serverId);
                break;
            }
            case TcpMessage::GetFileInfoResponse: {
                bool success = message.getData("success").toBool();
                QString filename = message.getData("filename").toString();

                if (!m_operations.contains(filename)) {
                    qDebug() << "No pending operation for file:" << filename;
                    return;
                }

                if (!success) {
                    QString error = message.getData("error").toString();
                    qDebug() << "Failed to get file info:" << filename << "-" << error;

                    m_operations[filename].status = Failed;
                    m_operations[filename].errorMessage = error;
                    return;
                }

                int serverId = message.getData("serverId").toInt();
                QHostAddress serverAddr(message.getData("serverAddress").toString());
                quint16 serverPort = message.getData("serverPort").toUInt();

                qDebug() << "File" << filename << "found. First chunk on server" << serverId
                         << "at" << serverAddr.toString() << ":" << serverPort;

                m_operations[filename].serverAddresses[serverId] = serverAddr;
                m_operations[filename].serverPorts[serverId] = serverPort;

                continueRetrieveOperation(filename, serverId);
                break;
            }
        }
    } else {
        switch (message.type()) {
            case TcpMessage::StoreChunkResponse: {
                bool success = message.getData("success").toBool();
                QString filename = message.getData("filename").toString();
                int chunkId = message.getData("chunkId").toInt();

                if (!m_operations.contains(filename)) {
                    qDebug() << "No pending operation for file:" << filename;
                    return;
                }

                FileOperation &op = m_operations[filename];

                if (!success) {
                    qDebug() << "Failed to store chunk" << chunkId << "for file" << filename;
                    op.status = Failed;
                    op.errorMessage = "Failed to store chunk " + QString::number(chunkId);
                    return;
                }

                qDebug() << "Chunk" << chunkId << "of file" << filename << "stored successfully";

                op.currentChunkIndex++;

                if (op.currentChunkIndex >= op.chunks.size()) {
                    qDebug() << "All chunks of file" << filename << "stored successfully";
                    op.status = Completed;
                    return;
                }

                int nextServerId = message.getData("nextServerId").toInt();
                QHostAddress nextAddr(message.getData("nextServerAddr").toString());
                quint16 nextPort = message.getData("nextServerPort").toUInt();

                if (nextServerId <= 0) {
                    qDebug() << "Reached end of server chain or invalid next server ID:" << nextServerId;
                    if (op.currentChunkIndex >= op.chunks.size()) {
                        qDebug() << "All chunks stored successfully!";
                        op.status = Completed;
                    } else {
                        qDebug() << "Storage chain ended but not all chunks were stored";
                        op.status = Failed;
                        op.errorMessage = "Incomplete storage chain";
                    }
                    return;
                }

                qDebug() << "Next chunk will be sent to server" << nextServerId
                         << "at" << nextAddr.toString() << ":" << nextPort;

                op.serverAddresses[nextServerId] = nextAddr;
                op.serverPorts[nextServerId] = nextPort;

                continueStoreOperation(filename, nextServerId);
                break;
            }
            case TcpMessage::GetChunkResponse: {
                bool success = message.getData("success").toBool();
                QString filename = message.getData("filename").toString();
                int chunkId = message.getData("chunkId").toInt();

                if (!m_operations.contains(filename)) {
                    qDebug() << "No pending operation for file:" << filename;
                    return;
                }

                FileOperation &op = m_operations[filename];

                if (!success) {
                    QString error = message.getData("error").toString();
                    qDebug() << "CHUNK" << chunkId << "IS CORRUPTED:" << error;

                    // Add placeholder for corrupted chunk
                    QByteArray corruptPlaceholder = QString(">>> CHUNK %1 CORRUPT <<<").arg(chunkId).toUtf8();
                    op.chunks.append(corruptPlaceholder);

                    op.errorMessage += QString("Chunk %1 corrupted; ").arg(chunkId);
                } else {
                    QByteArray chunkData = message.getData("chunkData").toByteArray();
                    op.chunks.append(chunkData);
                    qDebug() << "Retrieved chunk" << chunkId << "successfully (" << chunkData.size() << "bytes)";
                }

                if (message.hasData("nextServerId")) {
                    int nextServerId = message.getData("nextServerId").toInt();
                    QHostAddress nextAddr(message.getData("nextServerAddr").toString());
                    quint16 nextPort = message.getData("nextServerPort").toUInt();

                    op.serverAddresses[nextServerId] = nextAddr;
                    op.serverPorts[nextServerId] = nextPort;

                    continueRetrieveOperation(filename, nextServerId);
                } else {
                    qDebug() << "File retrieval completed for" << filename;

                    if (!op.errorMessage.isEmpty()) {
                        qDebug() << "File retrieved with errors:" << op.errorMessage;
                    }

                    bool success = reassembleFile(op.localPath, op.chunks);
                    op.status = success ? Completed : Failed;
                }
                break;
            }
        }
    }
}

void Client::continueStoreOperation(const QString &filename, int serverId)
{
    if (!m_operations.contains(filename)) {
        qDebug() << "No operation found for file:" << filename;
        return;
    }

    FileOperation &op = m_operations[filename];

    QHostAddress serverAddr = op.serverAddresses.value(serverId);
    quint16 serverPort = op.serverPorts.value(serverId);

    if (serverAddr.isNull() || serverPort == 0) {
        qDebug() << "Invalid server address/port for server" << serverId;
        op.status = Failed;
        op.errorMessage = "Invalid server configuration";
        return;
    }

    if (!m_serverConnections.contains(serverId)) {
        if (!connectToChunkServer(serverId, serverAddr, serverPort)) {
            qDebug() << "Failed to connect to ChunkServer" << serverId;
            op.status = Failed;
            op.errorMessage = "Connection to ChunkServer failed";
            return;
        }
    }

    TcpSocket *serverConn = m_serverConnections[serverId];
    if (!serverConn || !serverConn->isConnected()) {
        qDebug() << "Connection to server" << serverId << "is not available, reconnecting...";
        m_serverConnections.remove(serverId);

        if (!connectToChunkServer(serverId, serverAddr, serverPort)) {
            qDebug() << "Failed to reconnect to ChunkServer" << serverId;
            op.status = Failed;
            op.errorMessage = "Reconnection to ChunkServer failed";
            return;
        }
        serverConn = m_serverConnections[serverId];
    }

    int chunkIndex = op.currentChunkIndex;
    int chunkId = chunkIndex + 1;
    QByteArray chunkData = op.chunks[chunkIndex];
    bool isLastChunk = (chunkIndex == op.chunks.size() - 1);

    TcpMessage chunkMsg;
    chunkMsg.setType(TcpMessage::StoreChunk);
    chunkMsg.setData("filename", filename);
    chunkMsg.setData("chunkId", chunkId);
    chunkMsg.setData("isLastChunk", isLastChunk);
    chunkMsg.setData("chunkData", chunkData);

    qDebug() << "Sending chunk" << chunkId << "of" << op.chunks.size()
             << "for file" << filename << "to server" << serverId;

    serverConn->sendMessage(chunkMsg);
}


void Client::continueRetrieveOperation(const QString &filename, int serverId)
{
    if (!m_operations.contains(filename)) {
        qDebug() << "No operation found for file:" << filename;
        return;
    }

    FileOperation &op = m_operations[filename];

    QHostAddress serverAddr = op.serverAddresses[serverId];
    quint16 serverPort = op.serverPorts[serverId];

    if (!m_serverConnections.contains(serverId)) {
        if (!connectToChunkServer(serverId, serverAddr, serverPort)) {
            qDebug() << "Failed to connect to ChunkServer" << serverId;
            op.status = Failed;
            op.errorMessage = "Connection to ChunkServer failed";
            return;
        }
    }

    TcpSocket *serverConn = m_serverConnections[serverId];
    if (!serverConn || !serverConn->isConnected()) {
        qDebug() << "Connection to server" << serverId << "is not available, reconnecting...";
        m_serverConnections.remove(serverId);

        if (!connectToChunkServer(serverId, serverAddr, serverPort)) {
            qDebug() << "Failed to reconnect to ChunkServer" << serverId;
            op.status = Failed;
            op.errorMessage = "Reconnection to ChunkServer failed";
            return;
        }
        serverConn = m_serverConnections[serverId];
    }

    int chunkId = op.chunks.size() + 1;
    TcpMessage chunkMsg;
    chunkMsg.setType(TcpMessage::GetChunk);
    chunkMsg.setData("filename", filename);
    chunkMsg.setData("chunkId", chunkId);

    qDebug() << "Requesting chunk" << chunkId << "of file" << filename
             << "from server" << serverId;

    serverConn->sendMessage(chunkMsg);
}

void Client::determineHostAddress()
{
    QHostAddress hostAddress = QHostAddress::LocalHost; // Default fallback

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

                // Check for common LAN ranges
                if (addrStr.startsWith("192.168.") ||   // 192.168.0.0/16
                    addrStr.startsWith("10.") ||        // 10.0.0.0/8
                    addrStr.startsWith("172.20.")) {    // Your specific subnet

                    hostAddress = addr;
                    qDebug() << "Client found LAN interface:" << interface.name()
                             << "with address:" << addrStr;
                    goto found; // Break out of nested loops
                }
            }
        }
    }

found:
    setAddress(hostAddress);
    setTcpAddress(hostAddress);
    qDebug() << "Client host address set to:" << hostAddress.toString();
}


bool Client::connectToChunkServer(int serverId, const QHostAddress &serverAddr, quint16 serverPort)
{
    qDebug() << "=== CONNECTING TO CHUNKSERVER ===";
    qDebug() << "Target: ChunkServer" << serverId << "at" << serverAddr.toString() << ":" << serverPort;


    QProcess ncProcess;
    ncProcess.start("nc", QStringList() << "-z" << "-v" << "-w" << "2" << serverAddr.toString() << QString::number(serverPort));
    ncProcess.waitForFinished(3000);
    bool ncSuccess = ncProcess.exitCode() == 0;
    qDebug() << "Netcat connection test:" << (ncSuccess ? "Success" : "Failed");
    QString ncOutput = ncProcess.readAllStandardError();
    if (!ncOutput.isEmpty()) {
        qDebug() << "Netcat output:" << ncOutput;
    }

    if (m_serverConnections.contains(serverId)) {
        TcpSocket *existing = m_serverConnections[serverId];
        if (existing && existing->isConnected()) {
            qDebug() << "Already connected to ChunkServer" << serverId;
            return true;
        } else {
            qDebug() << "Removing stale connection to ChunkServer" << serverId;
            m_serverConnections.remove(serverId);
            if (existing) {
                existing->disconnect();
                existing->deleteLater();
            }
        }
    }


    TcpSocket *serverConnection = new TcpSocket(this);
    

    serverConnection->setProxy(QNetworkProxy::NoProxy);
    

    serverConnection->setProperty("serverId", serverId);

    connect(serverConnection, &TcpSocket::connected, this, [this, serverId]() {
        qDebug() << "Successfully connected to ChunkServer" << serverId;
    });

    connect(serverConnection, &TcpSocket::messageReceived, this, &Client::handleMessage);

    connect(serverConnection, &TcpSocket::disconnected, this, [this, serverId]() {
        qDebug() << "Disconnected from ChunkServer" << serverId;
        m_serverConnections.remove(serverId);
    });

    connect(serverConnection, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handleChunkServerError()));

    qDebug() << "Attempting to connect to ChunkServer" << serverId;
    bool started = serverConnection->connectToHost(serverAddr.toString(), 
                                                  serverPort,
                                                  QAbstractSocket::IPv4Protocol);
    
    if (!started) {
        qDebug() << "Failed to start connection to ChunkServer" << serverId;
        serverConnection->deleteLater();
        return false;
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    
    connect(serverConnection, &TcpSocket::connected, &loop, &QEventLoop::quit);

    connect(serverConnection, SIGNAL(error(QAbstractSocket::SocketError)), 
            &loop, SLOT(quit()));
            
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    qDebug() << "Waiting for connection to be established...";
    timeout.start(5000); // 5 second timeout
    loop.exec();
    
    bool connected = serverConnection->isConnected();
    
    if (!connected) {
        qDebug() << "Failed to establish connection to ChunkServer" << serverId;
        qDebug() << "TCP socket error:" << serverConnection->errorString();
        qDebug() << "Socket state:" << serverConnection->state();
        serverConnection->deleteLater();
        return false;
    }
    
    m_serverConnections[serverId] = serverConnection;
    qDebug() << "Successfully connected to ChunkServer" << serverId;
    return true;
}

bool Client::connectToChunkServerFallback(int serverId, const QHostAddress &serverAddr, quint16 serverPort)
{
    qDebug() << "=== FALLBACK CONNECTION TO CHUNKSERVER ===";

    if (m_serverConnections.contains(serverId)) {
        TcpSocket *existing = m_serverConnections[serverId];
        if (existing) {
            existing->disconnect();
            existing->deleteLater();
        }
        m_serverConnections.remove(serverId);
    }

    QTcpSocket *directSocket = new QTcpSocket(this);

    connect(directSocket, &QTcpSocket::errorOccurred, 
            [directSocket](QAbstractSocket::SocketError socketError) {
        qDebug() << "Direct socket error:" << socketError << "-" << directSocket->errorString();
    });
    

    directSocket->connectToHost(serverAddr, serverPort);

    bool connected = directSocket->waitForConnected(5000);
    
    if (!connected) {
        qDebug() << "Direct socket connection failed:" << directSocket->errorString();
        directSocket->deleteLater();

        qDebug() << "Trying localhost connection as last resort...";
        QTcpSocket *localSocket = new QTcpSocket(this);
        localSocket->connectToHost(QHostAddress::LocalHost, serverPort);
        
        bool localConnected = localSocket->waitForConnected(5000);
        if (!localConnected) {
            qDebug() << "Localhost connection also failed:" << localSocket->errorString();
            localSocket->deleteLater();

            qDebug() << "Trying to verify connection with netcat process...";
            QProcess nc;
            nc.start("nc", QStringList() << "-vz" << serverAddr.toString() << QString::number(serverPort));
            nc.waitForFinished(3000);
            
            QString output = nc.readAllStandardError();
            qDebug() << "Netcat verification output:" << output;
            
            if (nc.exitCode() == 0) {
                qDebug() << "Netcat reports connection is possible, but Qt sockets fail. Network issue detected.";
            }
            
            return false;
        }
        
        // Create a wrapper for the localhost socket
        TcpSocket *serverConnection = new TcpSocket(localSocket, this);
        serverConnection->setProperty("serverId", serverId);
        
        // Set up signal connections
        connect(serverConnection, &TcpSocket::connected, this, [this, serverId]() {
            qDebug() << "Successfully connected to ChunkServer" << serverId << "(localhost fallback)";
        });
        
        connect(serverConnection, &TcpSocket::messageReceived, this, &Client::handleMessage);
        
        connect(serverConnection, &TcpSocket::disconnected, this, [this, serverId]() {
            qDebug() << "Disconnected from ChunkServer" << serverId;
            m_serverConnections.remove(serverId);
        });
        
        connect(serverConnection, SIGNAL(error(QAbstractSocket::SocketError)),
                this, SLOT(handleChunkServerError()));
        
        m_serverConnections[serverId] = serverConnection;
        qDebug() << "Successfully connected to ChunkServer" << serverId << "(localhost fallback)";
        return true;
    }

    TcpSocket *serverConnection = new TcpSocket(directSocket, this);
    serverConnection->setProperty("serverId", serverId);

    connect(serverConnection, &TcpSocket::connected, this, [this, serverId]() {
        qDebug() << "Successfully connected to ChunkServer" << serverId << "(direct fallback)";
    });
    
    connect(serverConnection, &TcpSocket::messageReceived, this, &Client::handleMessage);
    
    connect(serverConnection, &TcpSocket::disconnected, this, [this, serverId]() {
        qDebug() << "Disconnected from ChunkServer" << serverId;
        m_serverConnections.remove(serverId);
    });
    
    connect(serverConnection, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handleChunkServerError()));
    
    m_serverConnections[serverId] = serverConnection;
    qDebug() << "Successfully connected to ChunkServer" << serverId << "(direct fallback)";
    return true;
}

void Client::handleChunkServerError()
{
    TcpSocket *socket = qobject_cast<TcpSocket*>(sender());
    if (!socket) return;
    
    int serverId = socket->property("serverId").toInt();
    QAbstractSocket::SocketError errorCode = socket->error();
    QString errorString = socket->errorString();
    
    qDebug() << "Connection error to ChunkServer" << serverId;
    qDebug() << "Error code:" << errorCode;
    qDebug() << "TCP socket error:" << errorString;
    qDebug() << "Socket state:" << socket->state();
    
    m_serverConnections.remove(serverId);
}


bool Client::performUdpHolePunchingToServer(const QHostAddress &serverAddr, quint16 serverPort)
{
    if (!m_udpSocket) {
        m_udpSocket = new UdpSocket(this);
        if (!m_udpSocket->bind(address(), 0)) { // Bind to any available port
            qDebug() << "Failed to bind client UDP socket";
            return false;
        }

        connect(m_udpSocket, &UdpSocket::pongReceived, this, [this](const QHostAddress &sender, quint16 senderPort) {
            qDebug() << "Received PONG from" << sender.toString() << ":" << senderPort;
            m_udpHolePunchSuccess = true;
        });
    }

    qDebug() << "Performing UDP hole punching to" << serverAddr.toString() << ":" << serverPort;

    m_udpHolePunchSuccess = false;

    // Send ping to establish UDP hole
    for (int i = 0; i < 3; ++i) {
        m_udpSocket->sendPing(serverAddr, serverPort);
        QThread::msleep(100);
    }

    // Wait for response
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(2000); // 2 second timeout

    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QTimer checkTimer;
    connect(&checkTimer, &QTimer::timeout, [&]() {
        if (m_udpHolePunchSuccess) {
            loop.quit();
        }
    });
    checkTimer.start(50);

    loop.exec();

    qDebug() << "UDP hole punching result:" << (m_udpHolePunchSuccess ? "Success" : "Failed");
    return m_udpHolePunchSuccess;
}
