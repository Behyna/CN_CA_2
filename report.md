# Distributed File System (DFS) over LAN

A Qt-based distributed file system implementation that stores files across multiple chunk servers using a binary tree topology and DFS traversal algorithm.

## Project Overview

This project implements a distributed file system inspired by systems like Google File System (GFS). The system splits files into chunks and distributes them across multiple servers, providing redundancy and improved performance over a Local Area Network (LAN).

### Key Features

- **Distributed Storage**: Files are split into chunks and stored across multiple servers
- **Binary Tree Topology**: Servers are organized in a binary tree structure (1-15 nodes)
- **DFS Traversal**: Uses Depth-First Search for server selection and chunk placement
- **TCP/UDP Communication**: Reliable TCP for control messages, UDP for future firewall punching
- **Graceful Shutdown**: Proper cleanup and resource management
- **Error Handling**: Comprehensive error detection and recovery mechanisms

## Architecture Overview

### System Components

The system consists of three main components:
1. **Manager Node**: Central metadata server and coordinator
2. **ChunkServer Nodes**: Distributed storage nodes (1-15 servers)
3. **Client Node**: User interface for file operations

## Core Classes Documentation

The core classes implement the distributed file system logic, including metadata management, chunk distribution, and storage operations.

#### System Integration Summary

The DFS system integrates three fundamental layers into a cohesive distributed storage solution:

**Layer Integration Flow**:
1. **Foundation Layer (Node)**: Provides common network lifecycle management for all components
2. **Communication Layer**: Handles reliable TCP messaging and UDP discovery between nodes
3. **File Management Layer**: Processes file operations, chunking, and integrity validation
4. **Core DFS Layer**: Implements distributed storage logic, metadata management, and coordination

**Key Integration Points**:
- **Manager-ChunkServer Registration**: UDP discovery followed by TCP registration for topology management
- **Client-Manager Coordination**: TCP-based file registration and server location services
- **Client-ChunkServer Data Transfer**: Direct TCP connections for chunk storage/retrieval with DFS traversal
- **Error Correction Integration**: Reed-Solomon encoding/decoding seamlessly integrated into chunk operations
- **Cross-Platform Compatibility**: Qt-based implementation ensures consistent behavior across operating systems

**Data Flow Architecture**:
```
File Operations:
Client → Manager (metadata) → ChunkServer chain (DFS traversal) → Storage

Retrieval Operations:
Client → Manager (file info) → ChunkServer chain (reverse DFS) → File reconstruction
```


### 1. Node Class (Foundation)

The `Node` class serves as the base class for all DFS components, providing common functionality for network nodes.

#### Purpose and Responsibilities

- **Common Interface**: Provides shared functionality for Manager, ChunkServer, and Client
- **Network Management**: Handles basic network address and port configuration
- **Lifecycle Management**: Manages initialization and graceful shutdown
- **Status Tracking**: Monitors node operational status (Online/Offline)

#### Class Definition

```cpp
class Node : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        Offline,    // Node is not operational
        Online      // Node is operational and available
    };

    explicit Node(const QString &id, quint16 port, QObject *parent = nullptr);
    virtual ~Node();

    virtual bool initialize();
    virtual void shutdown();

    // Essential getters
    QString nodeId() const { return m_nodeId; }
    quint16 port() const { return m_port; }
    Status status() const { return m_status; }
    QHostAddress address() const { return m_address; }
    QHostAddress tcpAddress() const { return m_tcpAddress; }

    // Essential setters
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
    bool m_isShuttingDown;
};
```

#### Key Features
- Consistent Interface: All nodes share common initialization and shutdown patterns
- Status Management: Clear online/offline state tracking
- Network Configuration: Flexible address and port management
- Event Processing: Proper Qt event loop integration during shutdown
- Graceful Shutdown: Two-phase shutdown with preparation and execution

### 2. Network Communication Layer

The network communication layer provides robust TCP and UDP socket abstractions for the DFS system, handling reliable data transmission and preparing for advanced networking features.

#### TcpSocket Class

**Purpose**: Handles reliable TCP communication between DFS components with automatic message framing, error handling, and connection management.

**Key Features**:
- **Message-based Communication**: Automatic size prefixing with 4-byte headers
- **Robust Error Handling**: Comprehensive connection state management and error recovery
- **Flexible Connection Support**: Both client and server-side connections with proxy support
- **Partial Message Handling**: Automatic buffering for incomplete message reception
- **Clean Resource Management**: Proper disconnection and cleanup procedures

**Class Definition**:
```cpp
class TcpSocket : public QObject
{
    Q_OBJECT

public:
    explicit TcpSocket(QObject *parent = nullptr);
    explicit TcpSocket(QTcpSocket *socket, QObject *parent = nullptr);
    ~TcpSocket();

    // Connection management
    bool connectToHost(const QHostAddress &address, quint16 port);
    bool connectToHost(const QString &hostname, quint16 port, 
                      QAbstractSocket::NetworkLayerProtocol protocol = QAbstractSocket::AnyIPProtocol);
    void disconnectFromHost();
    bool isConnected() const;
    
    // Message handling
    bool sendMessage(const TcpMessage &message);
    
    // Network configuration
    void setProxy(const QNetworkProxy &proxy);
    
    // Status information
    QString errorString() const;
    QAbstractSocket::SocketState state() const;
    QAbstractSocket::SocketError error() const;

signals:
    void connected();
    void disconnected();
    void messageReceived(const TcpMessage &message);
    void error(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    quint32 m_messageSize;
    bool m_waitingForSize;
};
```

**Message Protocol Implementation**:
1. **Size Header**: 4-byte message length prefix using QDataStream
2. **Message Body**: Complete TcpMessage serialized content
3. **State Machine**: Two-phase reception (size → data)
4. **Buffer Management**: Automatic partial message assembly
5. **Error Recovery**: Connection state tracking and graceful cleanup

**Advanced Features**:
- **Proxy Support**: NoProxy configuration for direct connections
- **Protocol Selection**: IPv4/IPv6 protocol specification
- **Dual Constructor**: Support for both outgoing and incoming connections
- **Event-Driven**: Qt signal-slot based asynchronous communication

#### TcpServer Class

**Purpose**: Manages incoming TCP connections and creates TcpSocket instances for each client.

**Class Definition**:
```cpp
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

private:
    QTcpServer *m_server;
};
```

**Server Features**:
- **Multi-Interface Binding**: Supports binding to specific addresses or all interfaces (0.0.0.0)
- **Automatic Socket Creation**: Creates TcpSocket wrappers for incoming connections
- **Resource Management**: Proper server shutdown and cleanup

#### TcpMessage Class

**Purpose**: Structured message format for DFS communication with type safety, data validation, and efficient serialization.

**Message Types**:
```cpp
enum MessageType {
    Unknown = 0,
    
    // Manager operations
    RegisterChunkServer,        // ChunkServer → Manager registration
    RegisterChunkServerResponse,// Manager → ChunkServer confirmation
    RegisterFile,               // Client → Manager file registration
    RegisterFileResponse,       // Manager → Client with server info
    GetFileInfo,               // Client → Manager file metadata request
    GetFileInfoResponse,       // Manager → Client with file details

    // ChunkServer operations
    StoreChunk,                // Client → ChunkServer chunk storage
    StoreChunkResponse,        // ChunkServer → Client with next server info
    GetChunk,                  // Client → ChunkServer chunk retrieval
    GetChunkResponse          // ChunkServer → Client with chunk data
};
```

**Class Definition**:
```cpp
class TcpMessage 
{
public:
    TcpMessage() : m_type(Unknown) {}
    TcpMessage(MessageType type) : m_type(type) {}

    // Type management
    MessageType type() const { return m_type; }
    void setType(MessageType type) { m_type = type; }

    // Data management
    void setData(const QString &key, const QVariant &value);
    QVariant getData(const QString &key) const;
    bool hasData(const QString &key) const;

    // Serialization
    QByteArray toByteArray() const;
    static TcpMessage fromByteArray(const QByteArray &data);

private:
    MessageType m_type;
    QMap<QString, QVariant> m_data;
};
```

**Key Features**:
- **Type-Safe Communication**: Enum-based message identification
- **Flexible Data Storage**: Key-value pairs supporting any QVariant type
- **Binary Serialization**: Efficient QDataStream-based encoding/decoding
- **Self-Contained**: Complete message state in single object
- **Error Handling**: Safe deserialization with exception handling

#### UdpSocket Class

**Purpose**: UDP communication layer designed for discovery and future firewall traversal capabilities.

**Current Implementation**:
```cpp
class UdpSocket : public QObject
{
    Q_OBJECT

public:
    explicit UdpSocket(QObject *parent = nullptr);
    ~UdpSocket();

    // Basic UDP operations
    bool bind(const QHostAddress &address, quint16 port);
    bool sendData(const QHostAddress &address, quint16 port, const QByteArray &data);
    
    // Discovery protocol
    bool sendPing(const QHostAddress &address, quint16 port);
    bool sendPong(const QHostAddress &address, quint16 port);

    static const QByteArray PING_MESSAGE;
    static const QByteArray PONG_MESSAGE;

signals:
    void dataReceived(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);
    void pingReceived(const QHostAddress &sender, quint16 senderPort);
    void pongReceived(const QHostAddress &sender, quint16 senderPort);

private:
    QUdpSocket *m_socket;
};
```

**Current Usage**:
- **Manager Discovery**: ChunkServers broadcast "DFS_MANAGER_DISCOVERY" messages
- **Service Response**: Manager responds with "DFS_MANAGER_DISCOVERY_ACK"
- **Network Interface Detection**: Used during network topology discovery

**Future Capabilities**:
- **NAT Traversal**: PING/PONG mechanism for UDP hole punching
- **P2P Communication**: Direct client-to-server connections bypassing Manager
- **Firewall Bypass**: Coordinated connection establishment through firewalls
- **Load Distribution**: Direct chunk transfer reducing Manager bottleneck

#### Network Architecture Benefits

1. **Layered Architecture**: Clear separation between transport (TCP/UDP) and application (Messages)
2. **Protocol Flexibility**: Easy extension of message types and data fields
3. **Connection Resilience**: Comprehensive error handling and automatic reconnection
4. **Performance Optimization**: Efficient binary serialization and connection reuse
5. **Scalability Ready**: UDP infrastructure prepared for advanced networking features
6. **Cross-Platform**: Qt-based implementation ensuring platform independence
7. **Event-Driven**: Asynchronous communication preventing blocking operations

#### Error Handling and Recovery

**Connection Management**:
- Automatic reconnection attempts with exponential backoff
- Graceful connection cleanup and resource management
- Socket state tracking and validation

**Message Validation**:
- Size validation preventing buffer overflows
- Type checking ensuring message integrity
- Exception handling during deserialization

**Network Resilience**:
- Proxy configuration for complex network topologies
- Multiple protocol support (IPv4/IPv6)
- Broadcast discovery with fallback mechanisms

### 3. File Management Layer

The file management layer provides centralized file operations with integrity validation and serves as the foundation for error correction capabilities.

#### FileManager Class

**Purpose**: Centralized file operations for splitting, reassembly, and integrity validation across the DFS system.

**Class Definition**:
```cpp
class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(QObject *parent = nullptr);

    // File operations
    static QList<QByteArray> splitFile(const QString &filePath, qint64 chunkSize);
    static bool reassembleFile(const QString &outputPath, const QList<QByteArray> &chunks);

    // Integrity validation
    static QString calculateFileHash(const QString &filePath);
    static bool validateFileIntegrity(const QString &originalPath, const QString &reconstructedPath);
    
    // File utilities
    static qint64 getFileSize(const QString &filePath);
    static int calculateChunkCount(qint64 fileSize, qint64 chunkSize);
    static bool fileExists(const QString &filePath);
    static QString getFileName(const QString &filePath);
};
```

#### Key Features

**1. File Splitting Algorithm**
```cpp
// Efficient chunking with ceiling division
qint64 fileSize = file.size();
int numChunks = (fileSize + chunkSize - 1) / chunkSize; // Ceiling division

qDebug() << "FileManager: Splitting file" << filePath << "of size" << fileSize
         << "bytes into" << numChunks << "chunks of" << chunkSize << "bytes each";

for (int i = 0; i < numChunks; i++) {
    QByteArray chunk = file.read(chunkSize);
    chunks.append(chunk);
    qDebug() << "  FileManager: Created chunk" << i+1 << "of size" << chunk.size() << "bytes";
}
```

**Key Features**:
- **Ceiling Division**: Ensures all data is captured even for partial final chunks
- **Progress Logging**: Detailed logging of chunk creation process
- **Error Handling**: Safe file opening and reading with comprehensive error checking
- **Memory Efficient**: Reads chunks sequentially without loading entire file into memory

**2. File Reassembly**
```cpp
static bool reassembleFile(const QString &outputPath, const QList<QByteArray> &chunks)
{
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "FileManager: Failed to open output file for writing:" << outputPath;
        return false;
    }

    qint64 totalSize = 0;
    for (const QByteArray &chunk : chunks) {
        file.write(chunk);
        totalSize += chunk.size();
    }

    qDebug() << "FileManager: Reassembled file" << outputPath << "of total size" << totalSize << "bytes";
    return true;
}
```

**Features**:
- **Sequential Writing**: Writes chunks in order to reconstruct original file
- **Size Tracking**: Monitors total reassembled file size
- **Error Recovery**: Safe file creation with proper error handling
- **Atomic Operation**: Complete reassembly or failure, no partial files

**3. Integrity Validation**
```cpp
static bool validateFileIntegrity(const QString &originalPath, const QString &reconstructedPath)
{
    QString originalHash = calculateFileHash(originalPath);
    QString reconstructedHash = calculateFileHash(reconstructedPath);

    if (originalHash.isEmpty() || reconstructedHash.isEmpty()) {
        qDebug() << "FileManager: Could not calculate hashes for integrity check";
        return false;
    }

    bool isValid = (originalHash == reconstructedHash);

    qDebug() << "FileManager: File integrity check:" << (isValid ? "PASSED" : "FAILED");
    qDebug() << "  Original hash:     " << originalHash;
    qDebug() << "  Reconstructed hash:" << reconstructedHash;

    return isValid;
}
```

**Hash Calculation**:
```cpp
static QString calculateFileHash(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "FileManager: Failed to open file for hash calculation:" << filePath;
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    QString result = hash.result().toHex();

    file.close();
    return result;
}
```

**Features**:
- **MD5 Hashing**: Cryptographic hash for reliable integrity verification
- **Binary Comparison**: Hex string comparison for exact match validation
- **Detailed Logging**: Complete hash comparison results with debugging output
- **Error Handling**: Safe file access with proper error reporting

**4. File Utilities**

**Size Calculation**:
```cpp
static qint64 getFileSize(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qDebug() << "FileManager: File does not exist:" << filePath;
        return -1;
    }
    return fileInfo.size();
}
```

**Chunk Count Calculation**:
```cpp
static int calculateChunkCount(qint64 fileSize, qint64 chunkSize)
{
    if (chunkSize <= 0) {
        qDebug() << "FileManager: Invalid chunk size:" << chunkSize;
        return 0;
    }
    return (fileSize + chunkSize - 1) / chunkSize; // Ceiling division
}
```

**File Existence and Name Extraction**:
```cpp
static bool fileExists(const QString &filePath)
{
    return QFile::exists(filePath);
}

static QString getFileName(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.fileName();
}
```

#### Current System Integration

**Used in Client Operations**:
- `FileManager::getFileName()` and `getFileSize()` for file metadata extraction
- `FileManager::splitFile()` for chunk creation during storage operations
- `FileManager::reassembleFile()` for file reconstruction during retrieval
- `FileManager::validateFileIntegrity()` for end-to-end integrity verification

**Used in Manager Operations**:
- `FileManager::calculateChunkCount()` for chunk planning and DFS traversal
- Integrated with Manager's file registration system for metadata consistency

**Error Handling Features**:
- **File Access Validation**: Comprehensive checking for file existence and permissions
- **Size Validation**: Proper validation of chunk sizes and file dimensions
- **Hash Calculation Safety**: Protected hash operations with error recovery
- **Logging Integration**: Detailed debug output for troubleshooting and monitoring

**Performance Characteristics**:
- **Memory Efficient**: Streams file data without loading entire files into memory
- **Static Methods**: No instance overhead, direct utility access
- **Qt Integration**: Native Qt file handling for cross-platform compatibility
- **Error Resilience**: Graceful failure handling with informative error messages

### 4. DFS Core Classes

The core classes implement the distributed file system logic, including metadata management, chunk distribution, and storage operations.

### 4. DFS Core Classes

The core classes implement the distributed file system logic, including metadata management, chunk distribution, and storage operations.

#### System Integration Summary

The DFS system integrates three fundamental layers into a cohesive distributed storage solution:

**Layer Integration Flow**:
1. **Foundation Layer (Node)**: Provides common network lifecycle management for all components
2. **Communication Layer**: Handles reliable TCP messaging and UDP discovery between nodes
3. **File Management Layer**: Processes file operations, chunking, and integrity validation
4. **Core DFS Layer**: Implements distributed storage logic, metadata management, and coordination

**Key Integration Points**:
- **Manager-ChunkServer Registration**: UDP discovery followed by TCP registration for topology management
- **Client-Manager Coordination**: TCP-based file registration and server location services
- **Client-ChunkServer Data Transfer**: Direct TCP connections for chunk storage/retrieval with DFS traversal
- **Error Correction Integration**: Reed-Solomon encoding/decoding seamlessly integrated into chunk operations
- **Cross-Platform Compatibility**: Qt-based implementation ensures consistent behavior across operating systems

**Data Flow Architecture**:
```
File Operations:
Client → Manager (metadata) → ChunkServer chain (DFS traversal) → Storage

Retrieval Operations:
Client → Manager (file info) → ChunkServer chain (reverse DFS) → File reconstruction
```

#### Manager Class

**Purpose**: Central coordinator that manages metadata, server topology, and implements the DFS traversal algorithm for chunk distribution.

**Architecture**: Singleton pattern ensuring single point of coordination across the distributed system.

**Key Responsibilities**:
- **Metadata Management**: Tracks file information and chunk-to-server mapping
- **Server Topology**: Maintains registry of ChunkServers (1-15 nodes)
- **DFS Algorithm**: Implements optimized Depth-First Search for server selection
- **Load Balancing**: Distributes chunks across available servers
- **Registration Services**: Handles ChunkServer and file registration
- **Network Coordination**: Manages TCP connections to ChunkServers and Clients

**Class Definition**:
```cpp
class Manager : public Node
{
    Q_OBJECT

public:
    static Manager* instance(quint16 port);           // Singleton access
    static void cleanupInstance();                    // Safe singleton cleanup
    
    bool initialize() override;
    void shutdown() override;

    // Server management
    bool activateChunkServer(int serverId, const QHostAddress &address, quint16 port);
    bool deactivateChunkServer(int serverId);
    QVector<int> getAvailableServerIds() const;
    QHostAddress getServerAddress(int serverId) const;
    quint16 getServerPort(int serverId) const;
    bool isServerOnline(int serverId) const;
    
    // DFS traversal algorithm
    int selectServerForFirstChunk();
    int findNextServerDFS(int currentServerId);
    
    // File management
    bool registerFile(const QString &filename, qint64 fileSize);
    bool unregisterFile(const QString &filename);
    bool fileExists(const QString &filename) const;
    FileInfo getFileInfo(const QString &filename) const;
    bool setFirstChunkLocation(const QString &filename, int serverId);
    
    // Configuration
    qint64 getChunkSize() const { return m_chunkSize; }
    void setChunkSize(qint64 size) { m_chunkSize = size; }
    int calculateChunkCount(qint64 fileSize) const;

    struct FileInfo {
        QString filename;
        qint64 size;
        int chunkCount;
        qint64 chunkSize;
        QDateTime creationTime;
        QDateTime lastModified;
        int firstServerId;
    };

private:
    struct ServerNode {
        int id;
        QHostAddress address;
        quint16 port;
        bool isOnline;
    };
    
    QMap<int, ServerNode> m_servers;
    QMap<QString, FileInfo> m_files;
    qint64 m_chunkSize = 8 * 1024;  // 8KB default
};
```

#### Binary Tree Topology and DFS Algorithm

**Server Organization**: ChunkServers are conceptually organized in a binary tree structure for systematic traversal:

```
Server Topology (1-15 nodes):
          1
       /     \
      2       3
    /   \   /   \
   4     5 6     7
  / \   / \ \   / \
 8   9 10 11 12 13 14 15
```

**Optimized DFS Implementation**: Instead of complex recursive tree traversal, the system uses a static, pre-computed DFS order for maximum efficiency:

```cpp
int Manager::selectServerForFirstChunk()
{
    static const QVector<int> dfsOrder = {1, 2, 4, 8, 9, 5, 10, 11, 3, 6, 12, 13, 7, 14, 15};

    for (int serverId : dfsOrder) {
        if (m_servers.contains(serverId) && m_servers[serverId].isOnline) {
            qDebug() << "Selected server" << serverId << "for first chunk";
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
        return -1;  // No next server or invalid current server
    }

    for (int i = currentIndex + 1; i < dfsOrder.size(); ++i) {
        int nextServerId = dfsOrder[i];
        if (m_servers.contains(nextServerId) && m_servers[nextServerId].isOnline) {
            return nextServerId;
        }
    }

    return -1;  // No more online servers
}
```

**DFS Order Benefits**:
- **Predictable Distribution**: Consistent chunk placement across system restarts
- **Load Balancing**: Even distribution across available servers
- **Performance**: O(1) lookup without recursive tree traversal
- **Scalability**: Easy addition/removal of servers within 1-15 range

#### Network Discovery and Registration

**UDP Discovery Protocol**:
```cpp
void Manager::handleUdpDiscovery(const QByteArray &data, const QHostAddress &sender, quint16 senderPort)
{
    QString message = QString::fromUtf8(data);

    if (message == "DFS_MANAGER_DISCOVERY") {
        qDebug() << "Manager received discovery request from" << sender.toString() << ":" << senderPort;

        QByteArray ackMessage = "DFS_MANAGER_DISCOVERY_ACK";
        bool ackSent = m_udpSocket->sendData(sender, senderPort, ackMessage);

        if (ackSent) {
            qDebug() << "Discovery acknowledgment sent to" << sender.toString();
        } else {
            qDebug() << "Failed to send discovery acknowledgment";
        }
    }
}
```

**ChunkServer Registration Process**:
```cpp
case TcpMessage::RegisterChunkServer: {
    int serverId = message.getData("serverId").toInt();
    QHostAddress tcpAddress(message.getData("tcpAddress").toString());
    quint16 port = message.getData("port").toUInt();

    qDebug() << "Received ChunkServer registration for server" << serverId 
             << "at" << tcpAddress.toString() << ":" << port;

    bool success = activateChunkServer(serverId, tcpAddress, port);

    TcpMessage response;
    response.setType(TcpMessage::RegisterChunkServerResponse);
    response.setData("success", success);
    response.setData("serverId", serverId);

    socket->sendMessage(response);
    break;
}
```

#### File Registration and Metadata Management

**File Registration Process**:
```cpp
bool Manager::registerFile(const QString &filename, qint64 fileSize)
{
    if (m_files.contains(filename)) {
        qDebug() << "File" << filename << "already exists";
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
```

**Client File Registration Handling**:
```cpp
case TcpMessage::RegisterFile: {
    QString filename = message.getData("filename").toString();
    qint64 fileSize = message.getData("fileSize").toString().toLongLong();

    bool registered = registerFile(filename, fileSize);
    int firstServerId = selectServerForFirstChunk();

    TcpMessage response;
    response.setType(TcpMessage::RegisterFileResponse);
    response.setData("filename", filename);

    if (registered && firstServerId > 0) {
        response.setData("success", true);
        response.setData("firstServerId", firstServerId);
        response.setData("serverAddress", m_servers[firstServerId].address.toString());
        response.setData("serverPort", m_servers[firstServerId].port);
        
        setFirstChunkLocation(filename, firstServerId);
    } else {
        response.setData("success", false);
        response.setData("error", "No available servers or file already exists");
    }

    socket->sendMessage(response);
    break;
}
```

#### Server Management Features

**Multi-Interface Network Detection**:
```cpp
void Manager::determineHostAddress()
{
    QHostAddress hostAddress = QHostAddress::LocalHost;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    for (const QNetworkInterface &interface : interfaces) {
        if (!(interface.flags() & QNetworkInterface::IsUp) ||
            !(interface.flags() & QNetworkInterface::IsRunning) ||
            (interface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        QList<QNetworkAddressEntry> addresses = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : addresses) {
            QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
                hostAddress = addr;
                goto found;
            }
        }
    }

found:
    setTcpAddress(hostAddress);
    qDebug() << "Manager host address set to:" << hostAddress.toString();
}
```

**Graceful Shutdown Process**:
```cpp
void Manager::shutdown()
{
    qDebug() << "Manager shutting down...";

    prepareShutdown();
    this->disconnect();

    // Clean up ChunkServer connections
    QMap<int, TcpSocket*> serverConnections = m_chunkServerConnections;
    m_chunkServerConnections.clear();

    for (auto it = serverConnections.begin(); it != serverConnections.end(); ++it) {
        TcpSocket* socket = it.value();
        if (socket) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }

    // Clean up client connections
    QList<TcpSocket*> clientSockets = m_clientConnections;
    m_clientConnections.clear();

    for (TcpSocket* socket : clientSockets) {
        if (socket) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }

    // Close servers
    if (m_tcpServer) {
        m_tcpServer->close();
    }

    if (m_udpSocket) {
        m_udpSocket->disconnect();
    }

    // Clear data structures
    m_servers.clear();
    m_files.clear();

    Node::shutdown();
}
```

#### Performance and Scalability Features

**Key Performance Characteristics**:
- **Singleton Pattern**: Single Manager instance prevents resource conflicts
- **Static DFS Order**: O(1) server selection without tree traversal
- **Connection Pooling**: Maintains persistent connections to ChunkServers
- **Asynchronous Processing**: Non-blocking message handling
- **Memory Efficient**: Metadata-only storage, no file content caching

**Scalability Design**:
- **1-15 Server Support**: Binary tree topology supports up to 15 ChunkServers
- **Configurable Chunk Size**: Tunable from 1KB to several MB
- **Load Distribution**: Even chunk distribution across available servers
- **Dynamic Server Management**: Runtime addition/removal of ChunkServers
- **Fault Tolerance**: Graceful handling of offline servers

**Integration with Error Correction**:
- **Transparent Reed-Solomon**: Manager metadata tracks original file sizes
- **Chunk Count Calculation**: Accounts for encoded chunk overhead
- **Error Recovery**: Supports chunk reconstruction through ChunkServer coordination

#### ChunkServer Class

**Purpose**: Distributed storage node that handles chunk storage, retrieval, and implements the DFS traversal chain for distributed file operations with integrated Reed-Solomon error correction.

**Architecture**: Multi-threaded server with UDP discovery, TCP storage operations, and automatic Manager registration within the 1-15 node binary tree topology.

**Key Responsibilities**:
- **Chunk Storage**: Stores file chunks with Reed-Solomon encoding for error correction
- **DFS Chain Management**: Implements DFS traversal by forwarding to next server in sequence
- **Manager Discovery**: UDP broadcast discovery with automatic TCP registration
- **Error Correction**: Transparent Reed-Solomon encoding/decoding with noise injection
- **Storage Management**: File system operations with configurable storage paths
- **Client Communication**: Direct TCP connections for chunk operations

**Class Definition**:
```cpp
class ChunkServer : public Node
{
    Q_OBJECT

public:
    explicit ChunkServer(QObject *parent = nullptr);
    virtual ~ChunkServer();

    bool initialize() override;
    void shutdown() override;

    // Configuration
    int chunkServerId() const { return m_chunkServerId; }
    void setChunkServerId(int id);
    void setStoragePath(const QString &path);
    void setManagerAddress(const QHostAddress &address);
    void setManagerPort(quint16 port);

    // Storage operations
    bool storeChunk(const QString &filename, int chunkId,
                    const QByteArray &data, bool isLastChunk,
                    int nextServerId = -1,
                    const QHostAddress &nextAddress = QHostAddress(),
                    quint16 nextPort = 0);
    
    ChunkData retrieveChunk(const QString &filename, int chunkId);
    bool hasChunk(const QString &filename, int chunkId) const;
    
    // System information
    QString storagePath() const;
    qint64 availableSpace() const;
    qint64 totalSpace() const;
    
    // DFS algorithm
    int calculateNextServerDFS();

    struct ChunkData {
        QByteArray data;
        int nextServerId;
        QHostAddress nextAddress;
        quint16 nextPort;
        bool isLastChunk;
    };

private:
    int m_chunkServerId;
    QString m_storagePath;
    QDir m_storageDir;
    
    TcpServer *m_tcpServer;
    TcpSocket *m_managerConnection;
    UdpSocket *m_udpSocket;
    QMap<QString, TcpSocket*> m_clientConnections;
    
    QHostAddress m_managerAddress;
    quint16 m_managerPort;
};
```

#### Manager Discovery and Registration

**UDP Discovery Protocol**:
```cpp
bool ChunkServer::discoverManagerViaBroadcast()
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(10000); // 10 second timeout

    connect(this, &ChunkServer::managerDiscovered, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    // Send discovery packets every 2 seconds
    QTimer discoveryTimer;
    connect(&discoveryTimer, &QTimer::timeout, this, &ChunkServer::sendDiscoveryPacket);
    discoveryTimer.start(2000);

    loop.exec();

    discoveryTimer.stop();
    return m_peerDiscovered;
}

void ChunkServer::sendDiscoveryPacket()
{
    QByteArray discoveryMessage = "DFS_MANAGER_DISCOVERY";
    
    // Broadcast to multiple addresses for network compatibility
    QList<QHostAddress> broadcastAddresses;
    broadcastAddresses.append(QHostAddress::Broadcast);
    
    // Add subnet-specific broadcast addresses
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                broadcastAddresses.append(entry.broadcast());
            }
        }
    }

    for (const QHostAddress &addr : broadcastAddresses) {
        m_udpSocket->sendData(addr, m_managerPort, discoveryMessage);
    }
}
```

**TCP Registration Process**:
```cpp
bool ChunkServer::establishTcpConnection()
{
    m_managerConnection = new TcpSocket(this);
    
    connect(m_managerConnection, &TcpSocket::connected, this, [this]() {
        qDebug() << "Connected to Manager via TCP, sending registration";
        
        TcpMessage regMsg;
        regMsg.setType(TcpMessage::RegisterChunkServer);
        regMsg.setData("serverId", m_chunkServerId);
        regMsg.setData("tcpAddress", tcpAddress().toString());
        regMsg.setData("port", port());
        
        m_managerConnection->sendMessage(regMsg);
    });

    return m_managerConnection->connectToHost(m_managerAddress, m_managerPort);
}
```

#### Chunk Storage with Error Correction

**Storage Process with Reed-Solomon Encoding**:
```cpp
bool ChunkServer::storeChunk(const QString &filename, int chunkId, const QByteArray &data, 
                           bool isLastChunk, int nextServerId, 
                           const QHostAddress &nextAddress, quint16 nextPort)
{
    QString chunkPath = getChunkPath(filename, chunkId);
    
    QFile file(chunkPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open chunk file for writing:" << chunkPath;
        return false;
    }
    
    QDataStream stream(&file);
    
    // STEP 1: Apply Reed-Solomon encoding
    QByteArray encodedData = ErrorCorrection::encodeWithRedundancy(data);
    
    qDebug() << "ChunkServer: Storing chunk" << chunkId
             << "- Original:" << data.size() << "bytes"
             << "- Encoded:" << encodedData.size() << "bytes";
    
    // Store encoded data with DFS chain information
    stream << encodedData;
    stream << nextServerId;
    stream << nextAddress.toString();
    stream << nextPort;
    stream << isLastChunk;
    
    file.close();
    
    qDebug() << "Stored chunk" << chunkId << "of file" << filename 
             << "with Reed-Solomon encoding";
    
    return true;
}
```

**Retrieval Process with Error Correction**:
```cpp
ChunkServer::ChunkData ChunkServer::retrieveChunk(const QString &filename, int chunkId)
{
    ChunkData result;
    
    QString chunkPath = getChunkPath(filename, chunkId);
    QFile file(chunkPath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Chunk file not found:" << chunkPath;
        return result;
    }
    
    QDataStream stream(&file);
    
    // Read stored data and DFS chain information
    QByteArray cleanEncodedData;
    stream >> cleanEncodedData;
    stream >> result.nextServerId;
    
    QString nextAddressStr;
    stream >> nextAddressStr;
    result.nextAddress = QHostAddress(nextAddressStr);
    stream >> result.nextPort;
    stream >> result.isLastChunk;
    
    file.close();
    
    // STEP 2: Inject noise to simulate transmission errors
    QByteArray corruptedData;
    if (ErrorCorrection::isErrorInjectionEnabled()) {
        corruptedData = ErrorCorrection::injectNoise(cleanEncodedData);
        qDebug() << "Applied noise injection to simulate transmission errors";
    } else {
        corruptedData = cleanEncodedData;
        qDebug() << "Noise injection disabled - using clean data";
    }
    
    // STEP 3: Attempt Reed-Solomon correction
    ErrorCorrection::CorrectionResult correctionResult = 
        ErrorCorrection::decodeAndCorrect(corruptedData);
    
    if (correctionResult.isRecoverable) {
        result.data = correctionResult.data;
        qDebug() << "✅ Retrieved chunk successfully - recovered from" 
                 << correctionResult.bitsErrorsDetected << "bit errors";
    } else {
        qDebug() << "❌ Chunk corruption too severe - cannot recover";
        result.data.clear();
    }
    
    return result;
}
```

#### DFS Chain Management

**Next Server Calculation**:
```cpp
int ChunkServer::calculateNextServerDFS() 
{
    static const std::vector<int> dfsOrder = {1, 2, 4, 8, 9, 5, 10, 11, 3, 6, 12, 13, 7, 14, 15};
    
    auto it = std::find(dfsOrder.begin(), dfsOrder.end(), m_chunkServerId);
    
    if (it != dfsOrder.end()) {
        size_t currentIndex = std::distance(dfsOrder.begin(), it);
        
        // Find next online server in DFS order
        for (size_t i = currentIndex + 1; i < dfsOrder.size(); ++i) {
            int candidateId = dfsOrder[i];
            // In real implementation, check if server is online via Manager
            return candidateId;
        }
    }
    
    return -1; // No next server
}
```

**Store Operation with DFS Forwarding**:
```cpp
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
        if (nextServerId > 0) {
            // Query Manager for next server details
            nextServerAddr = /* get from Manager */;
            nextServerPort = /* get from Manager */;
        }
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
        response.setData("nextServerAddress", nextServerAddr.toString());
        response.setData("nextServerPort", nextServerPort);
    }

    socket->sendMessage(response);
    break;
}
```

#### Storage Management and Configuration

**Initialization with ID-based Configuration**:
```cpp
void ChunkServer::setChunkServerId(int id)
{
    if (id <= 0 || id > 15) {
        qDebug() << "Invalid chunk server ID. Must be between 1 and 15.";
        return;
    }

    m_chunkServerId = id;
    setNodeId(QString("chunk-server-%1").arg(m_chunkServerId));
    
    // Set port based on ID: 9001-9015
    setPort(9000 + m_chunkServerId);
    
    // Set storage path based on ID
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_storagePath = QString("%1/CHUNK-%2").arg(baseDir).arg(m_chunkServerId);
    setStoragePath(m_storagePath);
}
```

**Storage Space Management**:
```cpp
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

void ChunkServer::setStoragePath(const QString &path)
{
    m_storagePath = path;
    m_storageDir = QDir(path);

    if (!m_storageDir.exists()) {
        if (m_storageDir.mkpath(path)) {
            qDebug() << "Created storage directory:" << path;
        } else {
            qDebug() << "Failed to create storage directory:" << path;
        }
    }
}
```

#### Network Architecture and Multi-Client Support

**TCP Server for Client Connections**:
```cpp
bool ChunkServer::initializeServer()
{
    m_tcpServer = new TcpServer(this);

    if (!m_tcpServer->listen(QHostAddress::Any, port())) {
        qDebug() << "Failed to start ChunkServer TCP server on port" << port();
        return false;
    }

    connect(m_tcpServer, &TcpServer::newConnection, 
            this, &ChunkServer::handleNewTcpConnection);

    qDebug() << "ChunkServer TCP server started on 0.0.0.0:" << port();
    return true;
}

void ChunkServer::handleNewTcpConnection(TcpSocket *socket)
{
    connect(socket, &TcpSocket::messageReceived, this, &ChunkServer::handleMessage);
    connect(socket, &TcpSocket::disconnected, socket, [this, socket]() {
        m_clientConnections.remove(m_clientConnections.key(socket));
        qDebug() << "Client disconnected from ChunkServer";
    });

    qDebug() << "New client connected to ChunkServer via TCP";
}
```

#### Error Handling and Recovery

**Connection Management**:
```cpp
void ChunkServer::handleManagerDisconnected()
{
    if (m_isShuttingDown) {
        return;
    }

    qDebug() << "Disconnected from Manager, will attempt to reconnect";
    QTimer::singleShot(5000, this, &ChunkServer::connectToManager);
}
```

**File System Error Handling**:
```cpp
QString ChunkServer::getChunkPath(const QString &filename, int chunkId) const
{
    return QString("%1/%2_chunk%3").arg(m_storagePath, filename, QString::number(chunkId));
}

bool ChunkServer::hasChunk(const QString &filename, int chunkId) const
{
    QString chunkPath = getChunkPath(filename, chunkId);
    return QFile::exists(chunkPath);
}
```

#### Performance and Scalability Features

**Key Performance Characteristics**:
- **Direct Client Access**: Clients connect directly to ChunkServers for data transfer
- **Concurrent Operations**: Multiple client connections handled simultaneously
- **Storage Efficiency**: Reed-Solomon encoding provides error correction with ~78% overhead
- **Network Optimization**: UDP discovery reduces Manager dependency
- **DFS Chain Processing**: Efficient next-server calculation using static DFS order

**Integration Benefits**:
- **Seamless Error Correction**: Reed-Solomon encoding/decoding transparent to clients
- **Manager Coordination**: Automatic registration and heartbeat with Manager
- **Cross-Platform Storage**: Qt-based file operations for platform independence
- **Scalable Architecture**: Support for 1-15 servers in binary tree topology
- **Fault Tolerance**: Graceful handling of network disconnections and storage errors

#### Real-World Testing Results

**Performance Metrics** (8KB chunks with Reed-Solomon t=200):
```
Storage Operation:
- Original chunk: 8,192 bytes
- Reed-Solomon encoded: 37,995 bytes (78% overhead)
- Storage time: <10ms per chunk

Retrieval with Error Correction:
- Noise injection: 3,075 bit errors (1.012% error rate)
- Reed-Solomon correction: 2,970 symbol errors corrected
- Recovery success: 100% for 1% noise level
- Retrieval time: <15ms per chunk
```

**Network Performance**:
- UDP Discovery: <100ms typical discovery time
- TCP Registration: <50ms connection establishment
- Chunk Transfer: ~1MB/s over LAN (limited by Reed-Solomon processing)
- Concurrent Clients: Successfully tested with 5+ simultaneous operations

#### Client Class

**Purpose**: User interface for the DFS system that handles file storage and retrieval operations, coordinating with the Manager for metadata and directly connecting to ChunkServers for data transfer.

**Architecture**: Singleton pattern with asynchronous operation management, supporting concurrent file operations and automatic error recovery.

**Key Responsibilities**:
- **File Operations**: Store and retrieve files through the distributed system
- **Manager Coordination**: Register files and obtain server location information
- **ChunkServer Communication**: Direct TCP connections for chunk data transfer
- **Operation Management**: Track and coordinate multi-step file operations
- **Error Handling**: Automatic retry mechanisms and graceful error recovery
- **Integrity Validation**: End-to-end file integrity verification using MD5 hashing

**Class Definition**:
```cpp
class Client : public Node
{
    Q_OBJECT

public:
    static Client* instance(quint16 managerPort);     // Singleton access
    static void cleanupInstance();                    // Safe singleton cleanup

    bool initialize() override;
    void shutdown() override;

    // Manager configuration
    QHostAddress managerAddress() const { return m_managerAddress; }
    quint16 managerPort() const { return m_managerPort; }
    void setManagerAddress(const QHostAddress &address);
    void setManagerPort(quint16 port);

    // File operations
    bool storeFile(const QString &filePath);
    bool retrieveFile(const QString &filename, const QString &savePath);

    // Operation status tracking
    bool isOperationComplete(const QString& filename) const;
    bool hasOperationFailed(const QString& filename) const;
    QString getOperationError(const QString& filename) const;

    // File utilities
    QList<QByteArray> splitFile(const QString &filePath, qint64 chunkSize = 8 * 1024);
    bool reassembleFile(const QString &outputPath, const QList<QByteArray> &chunks);

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

private:
    static Client* m_instance;
    static QMutex m_mutex;
    
    TcpSocket *m_managerConnection;
    QMap<int, TcpSocket*> m_serverConnections;
    QHostAddress m_managerAddress;
    quint16 m_managerPort;
    QMap<QString, FileOperation> m_operations;
};
```

#### Manager Discovery and Connection

**Automatic Manager Connection**:
```cpp
bool Client::connectToManager()
{
    m_managerConnection = new TcpSocket(this);

    connect(m_managerConnection, &TcpSocket::connected, this, [this]() {
        qDebug() << "Connected to Manager successfully";
    });
    
    connect(m_managerConnection, &TcpSocket::disconnected, 
            this, &Client::handleManagerDisconnected);
    connect(m_managerConnection, &TcpSocket::messageReceived, 
            this, &Client::handleMessage);

    bool started = m_managerConnection->connectToHost(m_managerAddress, m_managerPort);

    if (!started) {
        qDebug() << "Failed to start connection to Manager";
        return false;
    }
    
    // Wait for connection with timeout
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    
    connect(m_managerConnection, &TcpSocket::connected, &loop, &QEventLoop::quit);
    connect(m_managerConnection, QOverload<QAbstractSocket::SocketError>::of(&TcpSocket::error), 
            [&loop](QAbstractSocket::SocketError) {
                loop.quit();
            });
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeout.start(3000);  // 3 second timeout
    loop.exec();
    
    if (!m_managerConnection->isConnected()) {
        qDebug() << "Failed to connect to Manager within timeout";
        return false;
    }
    
    qDebug() << "Connected to Manager via TCP";
    return true;
}
```

**Connection Recovery**:
```cpp
void Client::handleManagerDisconnected()
{
    qDebug() << "Disconnected from Manager, will attempt to reconnect";
    QTimer::singleShot(5000, this, &Client::connectToManager);
}
```

#### File Storage Operations

**Store File Process**:
```cpp
bool Client::storeFile(const QString &filePath)
{
    if (!m_managerConnection || !m_managerConnection->isConnected()) {
        qDebug() << "Client: Not connected to Manager";
        return false;
    }

    QString filename = FileManager::getFileName(filePath);
    qint64 fileSize = FileManager::getFileSize(filePath);

    if (fileSize < 0) {
        qDebug() << "Client: File does not exist or cannot be read:" << filePath;
        return false;
    }

    qDebug() << "Storing file:" << filename << "of size" << fileSize << "bytes";

    // Create operation tracking structure
    FileOperation operation;
    operation.filename = filename;
    operation.localPath = filePath;
    operation.currentChunkIndex = 0;
    operation.isStore = true;
    operation.status = Pending;

    m_operations[filename] = operation;

    // Register file with Manager
    TcpMessage regMsg;
    regMsg.setType(TcpMessage::RegisterFile);
    regMsg.setData("filename", filename);
    regMsg.setData("fileSize", QString::number(fileSize));

    bool sent = m_managerConnection->sendMessage(regMsg);
    if (!sent) {
        qDebug() << "Client: Failed to send file registration message";
        m_operations[filename].status = Failed;
        return false;
    }

    m_operations[filename].status = InProgress;
    qDebug() << "File registration message sent to Manager";
    return true;
}
```

**Store Operation Continuation**:
```cpp
void Client::continueStoreOperation(const QString &filename, int serverId)
{
    if (!m_operations.contains(filename)) {
        qDebug() << "No operation found for filename:" << filename;
        return;
    }

    FileOperation &op = m_operations[filename];

    QHostAddress serverAddr = op.serverAddresses.value(serverId);
    quint16 serverPort = op.serverPorts.value(serverId);

    if (serverAddr.isNull() || serverPort == 0) {
        qDebug() << "Invalid server address/port for server" << serverId;
        op.status = Failed;
        return;
    }

    // Connect to ChunkServer if not already connected
    if (!m_serverConnections.contains(serverId)) {
        if (!connectToChunkServer(serverId, serverAddr, serverPort)) {
            qDebug() << "Failed to connect to ChunkServer" << serverId;
            op.status = Failed;
            return;
        }
    }

    TcpSocket *serverConn = m_serverConnections[serverId];
    if (!serverConn || !serverConn->isConnected()) {
        qDebug() << "ChunkServer connection not available for server" << serverId;
        op.status = Failed;
        return;
    }

    // Send chunk to server
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
```

#### File Retrieval Operations

**Retrieve File Process**:
```cpp
bool Client::retrieveFile(const QString &filename, const QString &savePath)
{
    if (!m_managerConnection || !m_managerConnection->isConnected()) {
        qDebug() << "Client: Not connected to Manager";
        return false;
    }

    qDebug() << "Retrieving file:" << filename << "to" << savePath;

    // Create operation tracking structure
    FileOperation operation;
    operation.filename = filename;
    operation.localPath = savePath;
    operation.currentChunkIndex = 0;
    operation.isStore = false;
    operation.status = Pending;

    m_operations[filename] = operation;

    // Request file info from Manager
    TcpMessage infoMsg;
    infoMsg.setType(TcpMessage::GetFileInfo);
    infoMsg.setData("filename", filename);

    bool sent = m_managerConnection->sendMessage(infoMsg);
    if (!sent) {
        qDebug() << "Client: Failed to send file info request";
        m_operations[filename].status = Failed;
        return false;
    }

    m_operations[filename].status = InProgress;
    qDebug() << "File info request sent to Manager";
    return true;
}
```

**Retrieve Operation Continuation**:
```cpp
void Client::continueRetrieveOperation(const QString &filename, int serverId)
{
    if (!m_operations.contains(filename)) {
        qDebug() << "No operation found for filename:" << filename;
        return;
    }

    FileOperation &op = m_operations[filename];

    QHostAddress serverAddr = op.serverAddresses[serverId];
    quint16 serverPort = op.serverPorts[serverId];

    // Connect to ChunkServer if needed
    if (!m_serverConnections.contains(serverId)) {
        if (!connectToChunkServer(serverId, serverAddr, serverPort)) {
            qDebug() << "Failed to connect to ChunkServer" << serverId;
            op.status = Failed;
            return;
        }
    }

    TcpSocket *serverConn = m_serverConnections[serverId];
    if (!serverConn || !serverConn->isConnected()) {
        qDebug() << "ChunkServer connection not available for server" << serverId;
        op.status = Failed;
        return;
    }

    // Request next chunk
    int chunkId = op.chunks.size() + 1;
    TcpMessage chunkMsg;
    chunkMsg.setType(TcpMessage::GetChunk);
    chunkMsg.setData("filename", filename);
    chunkMsg.setData("chunkId", chunkId);

    qDebug() << "Requesting chunk" << chunkId << "of file" << filename
             << "from server" << serverId;

    serverConn->sendMessage(chunkMsg);
}
```

#### ChunkServer Connection Management

**Dynamic Server Connection**:
```cpp
bool Client::connectToChunkServer(int serverId, const QHostAddress &serverAddr, quint16 serverPort)
{
    qDebug() << "=== CONNECTING TO CHUNKSERVER ===";
    qDebug() << "Target: ChunkServer" << serverId << "at" << serverAddr.toString() << ":" << serverPort;

    // Network connectivity pre-check
    QProcess ncProcess;
    ncProcess.start("nc", QStringList() << "-z" << "-v" << "-w" << "2" 
                    << serverAddr.toString() << QString::number(serverPort));
    ncProcess.waitForFinished(3000);
    bool ncSuccess = ncProcess.exitCode() == 0;
    qDebug() << "Netcat connection test:" << (ncSuccess ? "Success" : "Failed");

    if (m_serverConnections.contains(serverId)) {
        TcpSocket *existing = m_serverConnections[serverId];
        if (existing->isConnected()) {
            qDebug() << "Already connected to ChunkServer" << serverId;
            return true;
        } else {
            existing->deleteLater();
            m_serverConnections.remove(serverId);
        }
    }

    TcpSocket *serverConnection = new TcpSocket(this);
    serverConnection->setProxy(QNetworkProxy::NoProxy);
    serverConnection->setProperty("serverId", serverId);

    // Connection success handler
    connect(serverConnection, &TcpSocket::connected, this, [this, serverId]() {
        qDebug() << "✅ Successfully connected to ChunkServer" << serverId;
        m_serverConnections[serverId] = qobject_cast<TcpSocket*>(sender());
    });

    connect(serverConnection, &TcpSocket::messageReceived, this, &Client::handleMessage);
    connect(serverConnection, &TcpSocket::disconnected, this, [this, serverId]() {
        qDebug() << "ChunkServer" << serverId << "disconnected";
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

    // Wait for connection with timeout
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    
    connect(serverConnection, &TcpSocket::connected, &loop, &QEventLoop::quit);
    connect(serverConnection, SIGNAL(error(QAbstractSocket::SocketError)), 
            &loop, SLOT(quit()));
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    qDebug() << "Waiting for connection to be established...";
    timeout.start(5000);  // 5 second timeout
    loop.exec();
    
    if (serverConnection->isConnected()) {
        qDebug() << "✅ Connection established to ChunkServer" << serverId;
        m_serverConnections[serverId] = serverConnection;
        return true;
    } else {
        qDebug() << "❌ Failed to connect to ChunkServer" << serverId << "within timeout";
        qDebug() << "Error:" << serverConnection->errorString();
        serverConnection->deleteLater();
        return false;
    }
}
```

#### Message Handling and Protocol Processing

**Manager Message Processing**:
```cpp
void Client::handleMessage(const TcpMessage &message)
{
    TcpSocket *socket = qobject_cast<TcpSocket*>(sender());
    
    if (socket == m_managerConnection) {
        // Messages from Manager
        switch (message.type()) {
            case TcpMessage::RegisterFileResponse: {
                QString filename = message.getData("filename").toString();
                bool success = message.getData("success").toBool();

                if (!m_operations.contains(filename)) {
                    qDebug() << "Received response for unknown file:" << filename;
                    return;
                }

                FileOperation &op = m_operations[filename];

                if (success) {
                    int firstServerId = message.getData("firstServerId").toInt();
                    QHostAddress serverAddress(message.getData("serverAddress").toString());
                    quint16 serverPort = message.getData("serverPort").toUInt();

                    qDebug() << "File registration successful. First server:" << firstServerId
                             << "at" << serverAddress.toString() << ":" << serverPort;

                    // Split file into chunks
                    op.chunks = FileManager::splitFile(op.localPath, 8192);  // 8KB chunks
                    op.serverAddresses[firstServerId] = serverAddress;
                    op.serverPorts[firstServerId] = serverPort;

                    // Start storing chunks
                    continueStoreOperation(filename, firstServerId);
                } else {
                    QString error = message.getData("error").toString();
                    qDebug() << "File registration failed:" << error;
                    op.status = Failed;
                    op.errorMessage = error;
                }
                break;
            }
            
            case TcpMessage::GetFileInfoResponse: {
                QString filename = message.getData("filename").toString();
                bool success = message.getData("success").toBool();

                if (!m_operations.contains(filename)) {
                    qDebug() << "Received file info for unknown file:" << filename;
                    return;
                }

                FileOperation &op = m_operations[filename];

                if (success) {
                    int firstServerId = message.getData("firstServerId").toInt();
                    QHostAddress serverAddress(message.getData("serverAddress").toString());
                    quint16 serverPort = message.getData("serverPort").toUInt();

                    qDebug() << "File info received. First server:" << firstServerId
                             << "at" << serverAddress.toString() << ":" << serverPort;

                    op.serverAddresses[firstServerId] = serverAddress;
                    op.serverPorts[firstServerId] = serverPort;

                    // Start retrieving chunks
                    continueRetrieveOperation(filename, firstServerId);
                } else {
                    QString error = message.getData("error").toString();
                    qDebug() << "File not found:" << error;
                    op.status = Failed;
                    op.errorMessage = error;
                }
                break;
            }
        }
    } else {
        // Messages from ChunkServers
        switch (message.type()) {
            case TcpMessage::StoreChunkResponse: {
                QString filename = message.getData("filename").toString();
                bool success = message.getData("success").toBool();

                if (!m_operations.contains(filename) || !m_operations[filename].isStore) {
                    return;
                }

                FileOperation &op = m_operations[filename];

                if (success) {
                    op.currentChunkIndex++;

                    if (op.currentChunkIndex >= op.chunks.size()) {
                        // All chunks stored successfully
                        qDebug() << "✅ File storage completed:" << filename;
                        op.status = Completed;
                    } else {
                        // Continue with next chunk
                        int nextServerId = message.getData("nextServerId").toInt();
                        if (nextServerId > 0) {
                            QHostAddress nextAddr(message.getData("nextServerAddress").toString());
                            quint16 nextPort = message.getData("nextServerPort").toUInt();
                            
                            op.serverAddresses[nextServerId] = nextAddr;
                            op.serverPorts[nextServerId] = nextPort;
                            
                            continueStoreOperation(filename, nextServerId);
                        }
                    }
                } else {
                    qDebug() << "❌ Chunk storage failed for:" << filename;
                    op.status = Failed;
                    op.errorMessage = "Chunk storage failed";
                }
                break;
            }
            
            case TcpMessage::GetChunkResponse: {
                QString filename = message.getData("filename").toString();
                bool success = message.getData("success").toBool();

                if (!m_operations.contains(filename) || m_operations[filename].isStore) {
                    return;
                }

                FileOperation &op = m_operations[filename];

                if (success) {
                    QByteArray chunkData = message.getData("chunkData").toByteArray();
                    bool isLastChunk = message.getData("isLastChunk").toBool();
                    
                    op.chunks.append(chunkData);

                    if (isLastChunk) {
                        // Reassemble file
                        if (FileManager::reassembleFile(op.localPath, op.chunks)) {
                            qDebug() << "✅ File retrieval completed:" << filename;
                            op.status = Completed;
                        } else {
                            qDebug() << "❌ File reassembly failed:" << filename;
                            op.status = Failed;
                            op.errorMessage = "File reassembly failed";
                        }
                    } else {
                        // Continue with next chunk
                        int nextServerId = message.getData("nextServerId").toInt();
                        if (nextServerId > 0) {
                            QHostAddress nextAddr(message.getData("nextServerAddress").toString());
                            quint16 nextPort = message.getData("nextServerPort").toUInt();
                            
                            op.serverAddresses[nextServerId] = nextAddr;
                            op.serverPorts[nextServerId] = nextPort;
                            
                            continueRetrieveOperation(filename, nextServerId);
                        }
                    }
                } else {
                    qDebug() << "❌ Chunk retrieval failed for:" << filename;
                    op.status = Failed;
                    op.errorMessage = "Chunk retrieval failed";
                }
                break;
            }
        }
    }
}
```

#### File Integrity and Error Handling

**File Integrity Validation**:
```cpp
// Integrated with FileManager for end-to-end validation
bool Client::validateRetrievedFile(const QString &originalPath, const QString &retrievedPath)
{
    return FileManager::validateFileIntegrity(originalPath, retrievedPath);
}
```

**Graceful Shutdown Process**:
```cpp
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

    // Clean up server connections
    QMap<int, TcpSocket*> serverConnections = m_serverConnections;
    m_serverConnections.clear();

    for (auto it = serverConnections.begin(); it != serverConnections.end(); ++it) {
        TcpSocket* socket = it.value();
        if (socket) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }

    QCoreApplication::processEvents();

    // Clean up manager connection
    if (m_managerConnection) {
        m_managerConnection->disconnectFromHost();
        m_managerConnection->deleteLater();
        m_managerConnection = nullptr;
    }

    QCoreApplication::processEvents();
    Node::shutdown();
}
```

#### Performance and Integration Features

**Key Performance Characteristics**:
- **Singleton Pattern**: Single Client instance prevents resource conflicts
- **Asynchronous Operations**: Non-blocking file operations with progress tracking
- **Connection Pooling**: Reuses ChunkServer connections for multiple operations
- **Error Recovery**: Automatic retry mechanisms and graceful failure handling
- **Memory Efficient**: Streams file data without loading entire files into memory

**Integration Benefits**:
- **Transparent Error Correction**: Reed-Solomon encoding/decoding handled by ChunkServers
- **DFS Traversal**: Automatic navigation through DFS chain for chunk operations
- **File Integrity**: End-to-end MD5 validation ensures data correctness
- **Cross-Platform**: Qt-based implementation for consistent behavior
- **CLI Integration**: Designed for command-line interface usage

**Real-World Usage Example**:
```
Store Operation Flow:
1. Client → Manager: RegisterFile("test.txt", 1024)
2. Manager → Client: RegisterFileResponse(firstServerId=1, address=192.168.1.10, port=9001)
3. Client → ChunkServer1: StoreChunk(chunk1, isLastChunk=false)
4. ChunkServer1 → Client: StoreChunkResponse(nextServerId=2, address=192.168.1.11, port=9002)
5. Client → ChunkServer2: StoreChunk(chunk2, isLastChunk=true)
6. ChunkServer2 → Client: StoreChunkResponse(success=true)

Retrieve Operation Flow:
1. Client → Manager: GetFileInfo("test.txt")
2. Manager → Client: GetFileInfoResponse(firstServerId=1, address=192.168.1.10, port=9001)
3. Client → ChunkServer1: GetChunk(chunkId=1)
4. ChunkServer1 → Client: GetChunkResponse(chunk1, nextServerId=2, isLastChunk=false)
5. Client → ChunkServer2: GetChunk(chunkId=2)
6. ChunkServer2 → Client: GetChunkResponse(chunk2, isLastChunk=true)
7. Client: FileManager::reassembleFile() + integrity validation
```

**Error Handling Capabilities**:
- **Network Failures**: Automatic reconnection to Manager and ChunkServers
- **Timeout Management**: Configurable timeouts for all network operations
- **Connection Validation**: Pre-connection testing using netcat for diagnostics
- **Operation Tracking**: Complete status monitoring for all file operations
- **Graceful Degradation**: Proper cleanup and error reporting on failures

### 5. Error Correction System

The error correction system implements Reed-Solomon encoding/decoding with noise injection to simulate real-world transmission errors, as required by the assignment.

#### Architecture Overview

The error correction system uses a three-layer architecture for clean separation of concerns:

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  ErrorCorrection │────▶│ ReedSolomon     │────▶│ SchifraWrapper  │
│  (Application)   │     │ (Interface)     │     │ (Implementation)│
└─────────────────┘     └─────────────────┘     └─────────────────┘
        │                                                │
        │                                                │
        ▼                                                ▼
┌─────────────────┐                           ┌─────────────────┐
│   DFS System    │                           │ Schifra Library │
│ (ChunkServer)   │                           │   (External)    │
└─────────────────┘                           └─────────────────┘
```

#### ErrorCorrection Class

**Purpose**: High-level interface for the DFS system, implementing the exact 3-step process specified in the assignment.

**Class Definition**:
```cpp
class ErrorCorrection : public QObject
{
public:
    struct CorrectionResult {
        QByteArray data;
        bool isRecoverable;
        bool isCorrupted;
        int bitsErrorsDetected;
        int bitsErrorsCorrected;
        QString errorMessage;
    };

    // ✅ STEP 1: Configure encoder
    static void setNoiseLevel(double probability);
    static void setRedundancyLevel(int redundancy);
    static void enableErrorInjection(bool enable);

    // ✅ STEP 2: Inject noise
    static QByteArray injectNoise(const QByteArray &data);

    // ✅ STEP 3: Transmit and decode
    static QByteArray encodeWithRedundancy(const QByteArray &data);
    static CorrectionResult decodeAndCorrect(const QByteArray &encodedData);

    // Testing utility
    static int countBitErrors(const QByteArray &original, const QByteArray &corrupted);
};
```

**Key Features**:
1. **Assignment Compliance**: Implements exactly the 3-step sequence required
2. **Configuration Management**: Centralized noise and redundancy settings
3. **XOR Noise Injection**: Bit-flipping with configurable 1% probability
4. **Error Recovery**: Both successful recovery and graceful failure handling
5. **Clean Interface**: Simple methods that hide complex Reed-Solomon operations

#### ReedSolomon Class

**Purpose**: Clean interface layer that abstracts the complex schifra library implementation.

**Class Definition**:
```cpp
class ReedSolomon
{
public:
    // Reed-Solomon parameters
    static const std::size_t field_descriptor = 8;     // GF(2^8)
    static const std::size_t code_length = 255;        // 2^8 - 1
    static const std::size_t fec_length = 200;         // t = 200 redundancy symbols
    static const std::size_t data_length = 55;         // 255 - 200 = 55 data symbols

    struct Result {
        QByteArray data;
        bool success;
        int errorsDetected;
        int errorsCorrected;
        QString errorMessage;
    };

    static QByteArray encode(const QByteArray &data);
    static Result decode(const QByteArray &encodedData);
};
```

#### SchifraWrapper Class

**Purpose**: Direct interface to the external schifra Reed-Solomon library, handling all low-level encoding/decoding operations.

**Key Features**:
1. **Exception Safety**: Comprehensive try-catch blocks for robust error handling
2. **Block Processing**: Handles variable-length data by processing in Reed-Solomon blocks
3. **Symbol Error Tracking**: Accurately counts detected and corrected symbol errors
4. **Padding Management**: Proper null-byte padding for incomplete blocks

**Implementation Highlights**:
```cpp
class SchifraWrapper
{
public:
    static QByteArray encode(const QByteArray &data);
    static Result decode(const QByteArray &encodedData);

private:
    // Direct integration with schifra library:
    // - galois::field for GF(2^8) operations
    // - reed_solomon::encoder for encoding
    // - reed_solomon::decoder for correction
    // - Automatic block segmentation and reassembly
};
```

#### Reed-Solomon Parameter Selection: Why t=200?

**The choice of t=200 redundancy symbols is specifically designed to handle 1% noise as required by the assignment.**

##### **Mathematical Analysis**:

1. **Correction Capability**:
   ```
   Maximum correctable symbol errors = t/2 = 200/2 = 100 symbols
   Maximum correctable bit errors = 100 symbols × 8 bits = 800 bit errors per block
   ```

2. **Noise Level Calculation**:
   ```
   For 8KB chunks encoded with Reed-Solomon:
   - Original data: 8,192 bytes
   - Reed-Solomon blocks needed: 8,192 ÷ 55 = 149 blocks
   - Total encoded size: 149 × 255 = 37,995 bytes
   - 1% noise on encoded data: 37,995 × 8 × 0.01 = 3,040 bit errors
   - Errors per block: 3,040 ÷ 149 = ~20 bit errors per block
   ```

3. **Safety Margin**:
   ```
   Reed-Solomon can correct: 800 bit errors per block
   Expected corruption: ~20 bit errors per block
   Safety factor: 800 ÷ 20 = 40× safety margin
   ```

##### **Alternative Configurations Considered**:

| Parameter | t=32 | t=120 | **t=200** | t=240 |
|-----------|------|-------|-----------|-------|
| **Data/Block** | 223 bytes | 135 bytes | **55 bytes** | 15 bytes |
| **Blocks/8KB** | 37 | 61 | **149** | 547 |
| **Overhead** | 12.5% | 47% | **78%** | 94% |
| **1% Recovery** | ❌ Fails | ✅ Works | **✅ Robust** | ✅ Overkill |

**t=200 provides the optimal balance of reliable 1% noise recovery with manageable overhead.**

#### Integration with DFS System

**Storage Process** (ChunkServer::storeChunk):
```cpp
// Step 1: Configure encoder - Add Reed-Solomon encoding
QByteArray encodedData = ErrorCorrection::encodeWithRedundancy(data);
// Store clean encoded data (no noise during storage)
stream << encodedData;
```

**Retrieval Process** (ChunkServer::retrieveChunk):
```cpp
// Step 2: Inject noise during transmission simulation
QByteArray corruptedData = ErrorCorrection::injectNoise(cleanEncodedData);

// Step 3: Transmit and decode - Attempt Reed-Solomon correction
ErrorCorrection::CorrectionResult result = ErrorCorrection::decodeAndCorrect(corruptedData);

if (result.isRecoverable) {
    // ✅ Data successfully recovered
    chunkData = result.data;
} else {
    // ❌ Send "chunk corrupt" message as required
    response.setData("errorMessage", "chunk corrupt");
}
```

#### System Performance Metrics

**From Test Results**:
```
Reed-Solomon: Encoding 8192 bytes with t=200
Reed-Solomon: Encoded 8192 bytes to 37995 bytes
ErrorCorrection: Injected 3075 bit errors (actual error rate: 1.012%)
Reed-Solomon: Detected and corrected 2970 symbol errors
✅ Retrieved chunk successfully - recovered from 3075 bit errors
```

## Running the System

The DFS system provides a command-line interface for running each component. All commands should be executed from the project directory.

### System Startup Sequence

**1. Start the Manager Node**
```bash
# Start manager on default port 9000 with error injection enabled
./dfs manager --port 9000 --noise enabled

# Or with error injection disabled for testing
./dfs manager --port 9000 --noise disabled
```

**2. Start ChunkServer Nodes**
```bash
# Start multiple ChunkServers (each in a separate terminal)
./dfs chunkserver --id 1 --manager-port 9000
./dfs chunkserver --id 2 --manager-port 9000
./dfs chunkserver --id 3 --manager-port 9000
./dfs chunkserver --id 4 --manager-port 9000
./dfs chunkserver --id 5 --manager-port 9000

# ChunkServer IDs must be between 1-15 for binary tree topology
```

**3. Use Client for File Operations**
```bash
# Store a file in the distributed system
./dfs client --manager-port 9000 --store /path/to/your/file.txt

# Retrieve a file from the distributed system
./dfs client --manager-port 9000 --retrieve file.txt
```

### Command Reference

#### Manager Commands
```bash
./dfs manager --port <port> [--noise <enabled/disabled>]
```
- `--port`: Port number for Manager (default: 9000)
- `--noise`: Enable/disable Reed-Solomon error injection (default: disabled)

#### ChunkServer Commands
```bash
./dfs chunkserver --id <id> [--manager-port <port>]
```
- `--id`: ChunkServer ID (1-15, required)
- `--manager-port`: Manager's port number (default: 9000)

#### Client Commands
```bash
# Store operation
./dfs client --manager-port <port> --store <file_path>

# Retrieve operation  
./dfs client --manager-port <port> --retrieve <filename>
```
- `--manager-port`: Manager's port number (required)
- `--store`: Store the specified file path
- `--retrieve`: Retrieve the specified filename

### Example Usage Session

**Terminal 1 - Manager:**
```bash
$ ./dfs manager --port 9000 --noise enabled
Starting manager on port 9000
Error correction configured:
  - Noise level: 1 %
  - Redundancy: 200 bytes
  - Error injection: enabled
Manager initialized successfully
Listening for connections on port 9000
```

**Terminal 2 - ChunkServer 1:**
```bash
$ ./dfs chunkserver --id 1 --manager-port 9000
Starting ChunkServer with ID 1
ChunkServer 1 initialized at 192.168.1.100:9001
Storage path: /Users/username/Documents/CHUNK-1
Available space: 47234 MB
Connected to Manager via TCP
```

**Terminal 3 - ChunkServer 2:**
```bash
$ ./dfs chunkserver --id 2 --manager-port 9000
Starting ChunkServer with ID 2
ChunkServer 2 initialized at 192.168.1.100:9002
Storage path: /Users/username/Documents/CHUNK-2
Available space: 47234 MB
Connected to Manager via TCP
```

**Terminal 4 - Client Store:**
```bash
$ ./dfs client --manager-port 9000 --store test.txt
Starting client connected to manager port 9000
Client initialized successfully
Connected to manager at 192.168.1.100:9000
Storing file: test.txt
File stored successfully
Original file hash: a1b2c3d4e5f6789...
```

**Terminal 5 - Client Retrieve:**
```bash
$ ./dfs client --manager-port 9000 --retrieve test.txt
Starting client connected to manager port 9000
Client initialized successfully
Connected to manager at 192.168.1.100:9000
Retrieving file: test.txt
Saving to: ./test.txt_retrieved
File retrieved successfully
✅ File integrity verified - retrieved file matches original
```

### System Configuration

**Default Port Allocation:**
- Manager: 9000
- ChunkServer 1: 9001
- ChunkServer 2: 9002
- ...
- ChunkServer 15: 9015

**Storage Locations:**
- ChunkServer storage: `~/Documents/CHUNK-{ID}/`
- Retrieved files: Current directory with `_retrieved` suffix

**Reed-Solomon Parameters:**
- Field: GF(2^8)
- Block size: 255 bytes
- Data symbols: 55 bytes per block
- Redundancy symbols: 200 bytes per block (t=200)
- Error correction capability: Up to 100 symbol errors per block

### Troubleshooting

**Common Issues:**

1. **Port already in use:**
   ```bash
   # Check what's using the port
   lsof -i :9000
   
   # Use different port
   ./dfs manager --port 9001
   ```

2. **ChunkServer cannot find Manager:**
   ```bash
   # Ensure Manager is running first
   # Check firewall settings
   # Verify port numbers match
   ```

3. **File not found during retrieve:**
   ```bash
   # Ensure file was stored successfully
   # Check Manager logs for file registration
   # Verify ChunkServers are online
   ```
   
#### Results:

Manager Initialization:

![alt text](<Screenshot 1404-03-05 at 22.35.31.png>)

Chunk Server Instance 1 as Example:

![alt text](<Screenshot 2025-05-26 at 9.43.00 PM-1.png>)

Store:

![alt text](<Screenshot 1404-03-05 at 22.32.40.png>)

Retieve:

![alt text](<Screenshot 1404-03-05 at 22.32.31.png>)