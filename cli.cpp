#include "cli.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <iostream>
#include <unistd.h>  // For STDIN_FILENO on macOS

CommandLineInterface::CommandLineInterface(QObject *parent)
    : QObject(parent)
{
}

CommandLineInterface::~CommandLineInterface()
{
    // Clean up resources
    if (m_client) {
        m_client->shutdown();
        Client::cleanupInstance();
    }
}

bool CommandLineInterface::parseArguments(const QStringList &arguments)
{
    if (arguments.size() < 2) {
        printUsage();
        return false;
    }

    QString command = arguments[1].toLower();

    if (command == "manager") {
        m_mode = Mode::Manager;

        // Parse manager-specific arguments
        for (int i = 2; i < arguments.size(); i++) {
            if (arguments[i] == "--port" && i + 1 < arguments.size()) {
                bool ok;
                m_port = arguments[++i].toUInt(&ok);
                if (!ok || m_port == 0) {
                    qDebug() << "Invalid port number.";
                    return false;
                }
            } else if (arguments[i] == "--noise") {
                if (i + 1 < arguments.size()) {
                    QString value = arguments[++i].toLower();
                    if (value == "true" || value == "1" || value == "enabled") {
                        m_noiseEnabled = true;
                    } else if (value == "false" || value == "0" || value == "disabled") {
                        m_noiseEnabled = false;
                    } else {
                        qDebug() << "Invalid noise option. Use 'true'/'false', '1'/'0', or 'enabled'/'disabled'.";
                        return false;
                    }
                } else {
                    qDebug() << "Missing noise value.";
                    return false;
                }
            } else {
                qDebug() << "Unknown option:" << arguments[i];
                printUsage();
                return false;
            }
        }
    } else if (command == "chunkserver") {
        m_mode = Mode::ChunkServer;

        // Parse chunkserver-specific arguments
        for (int i = 2; i < arguments.size(); i++) {
            if (arguments[i] == "--id" && i + 1 < arguments.size()) {
                bool ok;
                m_chunkServerId = arguments[++i].toInt(&ok);
                if (!ok || m_chunkServerId < 1 || m_chunkServerId > 15) {
                    qDebug() << "Invalid chunk server ID. Must be between 1 and 15.";
                    return false;
                }
            } else if (arguments[i] == "--manager-port" && i + 1 < arguments.size()) {
                bool ok;
                m_port = arguments[++i].toUInt(&ok);
                if (!ok || m_port == 0) {
                    qDebug() << "Invalid port number.";
                    return false;
                }
            } else {
                qDebug() << "Unknown option:" << arguments[i];
                printUsage();
                return false;
            }
        }
    } else if (command == "client") {
        m_mode = Mode::Client;

        // Parse client-specific arguments
        bool storeMode = false;
        bool retrieveMode = false;

        for (int i = 2; i < arguments.size(); i++) {
            if (arguments[i] == "--manager-port" && i + 1 < arguments.size()) {
                bool ok;
                m_port = arguments[++i].toUInt(&ok);
                if (!ok || m_port == 0) {
                    qDebug() << "Invalid port number.";
                    return false;
                }
            } else if (arguments[i] == "--store" && i + 1 < arguments.size()) {
                if (retrieveMode) {
                    qDebug() << "Cannot specify both --store and --retrieve.";
                    return false;
                }
                storeMode = true;
                m_filePath = arguments[++i];
                if (!QFile::exists(m_filePath)) {
                    qDebug() << "File does not exist:" << m_filePath;
                    return false;
                }
            } else if (arguments[i] == "--retrieve" && i + 1 < arguments.size()) {
                if (storeMode) {
                    qDebug() << "Cannot specify both --store and --retrieve.";
                    return false;
                }
                retrieveMode = true;
                m_fileName = arguments[++i];
            } else {
                qDebug() << "Unknown option:" << arguments[i];
                printUsage();
                return false;
            }
        }

        if (!storeMode && !retrieveMode) {
            qDebug() << "Must specify either --store or --retrieve for client mode.";
            printUsage();
            return false;
        }

        m_storeMode = storeMode;
    } else {
        qDebug() << "Unknown command:" << command;
        printUsage();
        return false;
    }

    return true;
}

void CommandLineInterface::printUsage()
{
    std::cout << "DFS over LAN - Distributed File System\n";
    std::cout << "Usage:\n";
    std::cout << "  manager --port <port> [--noise <enabled/disabled>]\n";
    std::cout << "  chunkserver --id <id> [--manager-port <port>]\n";
    std::cout << "  client --manager-port <port> --store <file_path>\n";
    std::cout << "  client --manager-port <port> --retrieve <file_name>\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  manager --port 9000 --noise enabled\n";
    std::cout << "  chunkserver --id 1 --manager-port 9000\n";
    std::cout << "  client --manager-port 9000 --store /path/to/file.txt\n";
    std::cout << "  client --manager-port 9000 --retrieve file.txt\n";
}

int CommandLineInterface::run()
{
    switch (m_mode) {
    case Mode::Manager:
        runManager();
        break;
    case Mode::ChunkServer:
        runChunkServer();
        break;
    case Mode::Client:
        return runClient();
    default:
        printUsage();
        return 1;
    }

    return QCoreApplication::exec();
}

void CommandLineInterface::runManager()
{
    qDebug() << "Starting manager on port" << m_port;

    ErrorCorrection::enableErrorInjection(m_noiseEnabled);

    qDebug() << "Error correction configured:";
    qDebug() << "  - Noise level:" << ErrorCorrection::getNoiseLevel() * 100 << "%";
    qDebug() << "  - Redundancy:" << ErrorCorrection::getRedundancyLevel() << "bytes";
    qDebug() << "  - Error injection:" << (m_noiseEnabled ? "enabled" : "disabled");

    // Initialize manager
    m_manager = Manager::instance(m_port);

    if (m_manager == nullptr) {
        qDebug() << "Failed to initialize manager!";
        return;
    }

    qDebug() << "Manager initialized successfully";
    qDebug() << "Listening for connections on port" << m_port;
}

void CommandLineInterface::runChunkServer()
{
    qDebug() << "Starting ChunkServer with ID" << m_chunkServerId;

    // Create and initialize ChunkServer
    m_chunkServer = new ChunkServer();
    m_chunkServer->setChunkServerId(m_chunkServerId);
    m_chunkServer->setManagerPort(m_port);

    if (!m_chunkServer->initialize()) {
        qDebug() << "Failed to initialize ChunkServer!";
        return;
    }

    qDebug() << "ChunkServer" << m_chunkServerId << "initialized at"
             << m_chunkServer->address().toString() << ":" << m_chunkServer->port();
    qDebug() << "Storage path:" << m_chunkServer->storagePath();
    qDebug() << "Available space:" << m_chunkServer->availableSpace() / (1024 * 1024) << "MB";
}

int CommandLineInterface::runClient()
{
    qDebug() << "Starting client connected to manager port" << m_port;

    // Initialize client
    m_client = Client::instance(m_port);

    if (m_client == nullptr) {
        qDebug() << "Failed to initialize client!";
        return 1;
    }

    qDebug() << "Client initialized successfully";
    qDebug() << "Connected to manager at" << m_client->managerAddress().toString() << ":" << m_client->managerPort();

    // Execute the store or retrieve operation
    bool success = false;

    if (m_storeMode) {
        // Store mode
        qDebug() << "Storing file:" << m_filePath;
        success = m_client->storeFile(m_filePath);

        if (!success) {
            qDebug() << "Failed to start file storage operation!";
            return 1;
        }

        // Wait for operation to complete
        QFileInfo fileInfo(m_filePath);
        QString filename = fileInfo.fileName();
        success = waitForOperation(filename);

        if (success) {
            qDebug() << "File stored successfully";

            // Calculate and store the file hash for future verification
            QString fileHash = FileManager::calculateFileHash(m_filePath);
            qDebug() << "Original file hash: " << fileHash;

            return 0;
        } else {
            qDebug() << "File storage failed:" << m_client->getOperationError(filename);
            return 1;
        }
    } else {
        // Retrieve mode
        QString savePath = QDir::currentPath() + "/" + m_fileName + "_retrieved";

        qDebug() << "Retrieving file:" << m_fileName;
        qDebug() << "Saving to:" << savePath;

        success = m_client->retrieveFile(m_fileName, savePath);

        if (!success) {
            qDebug() << "Failed to start file retrieval operation!";
            return 1;
        }

        // Wait for operation to complete
        success = waitForOperation(m_fileName);

        if (success) {
            qDebug() << "File retrieved successfully";

            // If we're retrieving a file we stored in this session, verify integrity
            QString originalPath = QDir::currentPath() + "/" + m_fileName;
            if (QFile::exists(originalPath)) {
                bool integrityValid = FileManager::validateFileIntegrity(originalPath, savePath);

                if (integrityValid) {
                    qDebug() << "✅ File integrity verified - retrieved file matches original";
                } else {
                    qDebug() << "❌ File integrity check failed - files do not match";
                }
            } else {
                // Just show the hash if we don't have the original file
                QString fileHash = FileManager::calculateFileHash(savePath);
                qDebug() << "Retrieved file hash: " << fileHash;
            }

            return 0;
        } else {
            qDebug() << "File retrieval failed:" << m_client->getOperationError(m_fileName);
            return 1;
        }
    }
}

bool CommandLineInterface::waitForOperation(const QString& filename, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    bool operationComplete = false;
    QTimer checkTimer;

    connect(&checkTimer, &QTimer::timeout, [&]() {
        if (m_client->isOperationComplete(filename)) {
            operationComplete = true;
            loop.quit();
        } else if (m_client->hasOperationFailed(filename)) {
            operationComplete = false;
            loop.quit();
        }
    });

    checkTimer.start(200); // Check every 200ms

    // Run the event loop until the operation completes or times out
    loop.exec();

    return operationComplete;
}
