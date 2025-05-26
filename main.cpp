#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QCryptographicHash>
#include <QFile>
#include <QThread>
#include <QEventLoop>
#include "manager.h"
#include "chunkserver.h"
#include "reedsolomon.h"
#include "client.h"
#include "cli.h"

// Helper function to get file MD5 hash for verification
QString getFileMD5(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file for MD5 calculation:" << filePath;
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result().toHex();
}

// Helper to wait for a file operation to complete
bool waitForOperation(const QString& filename, int timeoutMs = 30000) {
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    // We'll manually poll for completion since we can't access private members
    bool operationComplete = false;
    QTimer checkTimer;

    // Get a reference to our singleton client
    Client* client = Client::instance(9000);

    QObject::connect(&checkTimer, &QTimer::timeout, [&]() {
        // Check if operation is complete by querying the client
        // This depends on your Client class having a method to check status
        if (client->isOperationComplete(filename)) {
            operationComplete = true;
            loop.quit();
        } else if (client->hasOperationFailed(filename)) {
            qDebug() << "Operation failed for file:" << filename;
            loop.quit();
        }
    });
    checkTimer.start(100);

    loop.exec();
    return operationComplete;
}

void testFullDFS(QList<ChunkServer*>& servers) {
    qDebug() << "\n===== TESTING COMPLETE DFS WITH ERROR CORRECTION =====\n";

    ErrorCorrection::setNoiseLevel(0.01);  // 1% error rate - higher to guarantee corruption
    ErrorCorrection::setRedundancyLevel(200); // 16 bytes redundancy
    ErrorCorrection::enableErrorInjection(true);

    qDebug() << "Error correction configured:";
    qDebug() << "  - Noise level:" << ErrorCorrection::getNoiseLevel() * 100 << "%";
    qDebug() << "  - Redundancy:" << ErrorCorrection::getRedundancyLevel() << "bytes";
    qDebug() << "  - Injection enabled:" << ErrorCorrection::isErrorInjectionEnabled();


    // Step 1: Initialize Manager with 8KB chunk size
    Manager* manager = Manager::instance(9000);
    manager->setChunkSize(8 * 1024); // 8KB chunks
    qDebug() << "Manager initialized with chunk size" << manager->getChunkSize() << "bytes";

    // Step 2: Create and initialize 15 ChunkServers
    for (int i = 1; i <= 15; i++) {
        ChunkServer* server = new ChunkServer();
        server->setChunkServerId(i);
        server->setManagerPort(9000);
        if (!server->initialize()) {
            qDebug() << "ERROR: Failed to initialize ChunkServer" << i;
            continue;
        }

        servers.append(server);
        qDebug() << "ChunkServer" << i << "initialized at"
                 << server->address().toString() << ":" << server->port();
    }

    qDebug() << "\n" << servers.size() << "ChunkServers successfully initialized";
    if (servers.size() < 15) {
        qDebug() << "WARNING: Not all ChunkServers initialized successfully";
    }

    bool allRegistered = false;
    int maxAttempts = 30;
    int attempt = 0;

    while (!allRegistered && attempt < maxAttempts) {
        // Process pending events to ensure registration messages get handled
        QCoreApplication::processEvents();

        QVector<int> onlineServers = Manager::instance(9000)->getAvailableServerIds();
        allRegistered = (onlineServers.size() == 15);

        if (!allRegistered) {
            qDebug() << onlineServers.size() << "of 15 servers registered. Waiting...";
            QThread::sleep(1);
            attempt++;
        }
    }

    if (!allRegistered) {
        qDebug() << "WARNING: Not all ChunkServers registered with Manager after" << maxAttempts << "seconds";
    }

    // Step 3: Initialize Client
    Client* client = Client::instance(9000);
    qDebug() << "\nClient initialized";

    // Step 4: Define test file paths
    QString testInputFile = "/Users/behyna/Developer/Projects/University/Computer-Network/DFS/fun_computer_250kb_A.txt";
    QString testOutputFile = "/Users/behyna/Developer/Projects/University/Computer-Network/DFS/fun_computer_250kb_A_retrieved.txt";

    // Calculate original file hash
    // QString originalHash = getFileMD5(testInputFile);
    QString originalHash = FileManager::calculateFileHash(testInputFile);
    qDebug() << "\nOriginal file hash:" << originalHash;

    // Step 5: Store file
    qDebug() << "\n----- STORING FILE -----";
    bool storeStarted = client->storeFile(testInputFile);
    if (!storeStarted) {
        qDebug() << "ERROR: Failed to start store operation";
        return;
    }

    // Wait for storage to complete
    QString filename = QFileInfo(testInputFile).fileName();
    bool storeSuccess = waitForOperation(filename);

    if (!storeSuccess) {
        qDebug() << "ERROR: File storage operation failed or timed out";
        return;
    }

    qDebug() << "File storage completed successfully!";

    // Step 6: Show distribution of chunks
    qDebug() << "\n----- CHUNK DISTRIBUTION -----";
    QMap<int, int> chunkCounts;
    for (ChunkServer* server : servers) {
        QDir dir(server->storagePath());
        QStringList files = dir.entryList(QDir::Files);
        chunkCounts[server->chunkServerId()] = files.size();

        if (!files.isEmpty()) {
            qDebug() << "Server" << server->chunkServerId()
            << "has" << files.size() << "chunks:" << files.join(", ");
        }
    }

    // Step 7: Retrieve file
    qDebug() << "\n----- RETRIEVING FILE -----";

    // Remove output file if it exists
    QFile outputFile(testOutputFile);
    if (outputFile.exists()) {
        outputFile.remove();
    }

    bool retrieveStarted = client->retrieveFile(filename, testOutputFile);
    if (!retrieveStarted) {
        qDebug() << "ERROR: Failed to start retrieve operation";
        return;
    }

    // Wait for retrieval to complete
    bool retrieveSuccess = waitForOperation(filename);

    if (!retrieveSuccess) {
        qDebug() << "ERROR: File retrieval operation failed or timed out";
        return;
    }

    qDebug() << "File retrieval completed successfully!";

    // Step 8: Verify file integrity
    QString retrievedHash = getFileMD5(testOutputFile);
    qDebug() << "\n----- VERIFYING FILE INTEGRITY -----";
    // qDebug() << "Original file hash:  " << originalHash;
    // qDebug() << "Retrieved file hash: " << retrievedHash;

    // if (originalHash == retrievedHash) {
    //     qDebug() << "\n✅ TEST SUCCESSFUL: Files match perfectly!";
    // } else {
    //     qDebug() << "\n❌ TEST FAILED: Files do not match!";
    // }
    bool integrityValid = FileManager::validateFileIntegrity(testInputFile, testOutputFile);

    if (integrityValid) {
        qDebug() << "\n✅ TEST SUCCESSFUL: Files match perfectly!";
    } else {
        qDebug() << "\n❌ TEST FAILED: Files do not match!";
    }


    qDebug() << "\n===== DFS TEST COMPLETED =====\n";
}

void cleanupDFS(QList<ChunkServer*>& servers, QCoreApplication& app) {
    qDebug() << "\n===== SHUTTING DOWN DFS =====\n";

    // 1. Shutdown Client first
    if (Client::instance(9000)) {
        qDebug() << "Shutting down Client...";
        Client::instance(9000)->shutdown();
        QCoreApplication::processEvents();
        QThread::msleep(200);
    }

    // 2. Shutdown ChunkServers one by one with proper cleanup
    qDebug() << "Shutting down" << servers.size() << "ChunkServers...";
    for (int i = 0; i < servers.size(); ++i) {
        ChunkServer* server = servers.at(i);
        if (server) {
            qDebug() << "Shutting down ChunkServer" << server->chunkServerId() << "...";

            // Disconnect all signals first
            server->disconnect();
            server->blockSignals(true);

            // Shutdown the server
            server->shutdown();
            QCoreApplication::processEvents();
            QThread::msleep(100);
        }
    }

    // 3. Shutdown Manager
    if (Manager::instance(9000)) {
        qDebug() << "Shutting down Manager...";
        Manager::instance(9000)->disconnect();
        Manager::instance(9000)->blockSignals(true);
        Manager::instance(9000)->shutdown();
        QCoreApplication::processEvents();
        QThread::msleep(300);
    }

    // 4. Schedule ChunkServer objects for deletion using deleteLater()
    qDebug() << "Scheduling ChunkServer objects for deletion...";
    for (int i = 0; i < servers.size(); ++i) {
        ChunkServer* server = servers.at(i);
        if (server) {
            qDebug() << "Scheduling ChunkServer" << i+1 << "for deletion...";
            server->deleteLater();
            QCoreApplication::processEvents();
        }
    }
    servers.clear();

    // 5. Process events to handle scheduled deletions
    qDebug() << "Processing scheduled deletions...";
    for (int i = 0; i < 15; ++i) {  // More iterations to ensure cleanup
        QCoreApplication::processEvents();
        QThread::msleep(100);  // Longer delay
    }

    // 6. Clean up singletons CAREFULLY
    qDebug() << "Cleaning up singletons...";

    // Clean up Client singleton (but don't call shutdown again)
    if (Client::instance(9000)) {
        Client::cleanupInstance();
        QCoreApplication::processEvents();
        QThread::msleep(200);
    }

    // Clean up Manager singleton
    Manager::cleanupInstance();
    QCoreApplication::processEvents();
    QThread::msleep(200);

    qDebug() << "DFS shutdown completed successfully.";

    // 7. Final cleanup - process events multiple times
    qDebug() << "Final cleanup...";
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(100);
    }

    // 8. Exit immediately instead of calling quit()
    qDebug() << "Exiting application...";
    QCoreApplication::processEvents();
    QThread::msleep(200);

    // Force exit to avoid Qt cleanup issues
    std::exit(0);
}

void testReedSolomonSpecifically() {
    qDebug() << "\n===== TESTING REED-SOLOMON WITH CURRENT PARAMETERS =====\n";

    // Test with realistic chunk data
    QByteArray testData(8192, 'A'); // 8KB like your real chunks

    qDebug() << "Original data size:" << testData.size() << "bytes";

    // Step 1: Encode with Reed-Solomon
    QByteArray encoded = ErrorCorrection::encodeWithRedundancy(testData);
    qDebug() << "Reed-Solomon encoded size:" << encoded.size() << "bytes";
    qDebug() << "Reed-Solomon parameters: t=" << ReedSolomon::fec_length
             << ", max correctable per block:" << (ReedSolomon::fec_length / 2) << "symbols ="
             << (ReedSolomon::fec_length / 2) * 8 << "bits";

    // Step 2: Inject 1% noise
    ErrorCorrection::setNoiseLevel(0.01);
    QByteArray corrupted = ErrorCorrection::injectNoise(encoded);
    int bitErrors = ErrorCorrection::countBitErrors(encoded, corrupted);

    qDebug() << "Injected" << bitErrors << "bit errors (1% of" << (encoded.size() * 8) << "bits)";

    // Step 3: Attempt decode
    ErrorCorrection::CorrectionResult result = ErrorCorrection::decodeAndCorrect(corrupted);

    if (result.isRecoverable) {
        qDebug() << "✅ Reed-Solomon SUCCESSFULLY recovered from" << bitErrors << "bit errors!";
        qDebug() << "Errors detected:" << result.bitsErrorsDetected;
        qDebug() << "Errors corrected:" << result.bitsErrorsCorrected;

        bool dataMatches = (result.data == testData);
        qDebug() << "Data integrity:" << (dataMatches ? "✅ PERFECT" : "❌ CORRUPTED");
    } else {
        qDebug() << "❌ Reed-Solomon FAILED to recover from" << bitErrors << "bit errors";
        qDebug() << "Error message:" << result.errorMessage;
    }

    qDebug() << "\n===== REED-SOLOMON PARAMETER TEST COMPLETED =====\n";
}

void testErrorCorrection() {
    qDebug() << "\n===== TESTING ERROR CORRECTION SYSTEM =====\n";

    // ✅ Configure for 1% noise as per assignment
    ErrorCorrection::setNoiseLevel(0.0015);  // 1% bit error rate
    ErrorCorrection::setRedundancyLevel(32);
    ErrorCorrection::enableErrorInjection(true);

    // Test with sample data
    QByteArray testData = "Hello, this is a test message for Reed-Solomon error correction! This message needs to be long enough to demonstrate Reed-Solomon capabilities properly and show both successful recovery and failure cases.";
    qDebug() << "Original data:" << testData.size() << "bytes";

    // Step 1: Add Reed-Solomon encoding
    QByteArray encoded = ErrorCorrection::encodeWithRedundancy(testData);
    qDebug() << "Reed-Solomon encoded:" << encoded.size() << "bytes";

    // Step 2: Simulate transmission with noise injection
    QByteArray transmitted = ErrorCorrection::injectNoise(encoded);
    int bitErrors = ErrorCorrection::countBitErrors(encoded, transmitted);
    qDebug() << "Transmission simulation: injected" << bitErrors << "bit errors";

    // Step 3: Attempt Reed-Solomon decoding and correction
    ErrorCorrection::CorrectionResult result = ErrorCorrection::decodeAndCorrect(transmitted);

    if (result.isRecoverable) {
        qDebug() << "✅ Reed-Solomon CORRECTION SUCCESSFUL!";
        qDebug() << "Errors detected:" << result.bitsErrorsDetected;
        qDebug() << "Errors corrected:" << result.bitsErrorsCorrected;

        bool dataMatches = (result.data == testData);
        qDebug() << "Data integrity:" << (dataMatches ? "✅ PERFECT" : "❌ CORRUPTED");
    } else {
        qDebug() << "❌ Reed-Solomon correction failed:" << result.errorMessage;
        qDebug() << "This chunk would be marked as 'chunk corrupt'";
    }

    qDebug() << "\n===== ERROR CORRECTION TEST COMPLETED =====\n";
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("DFS over LAN");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("University Computer Network Project");

    // // Initialize error correction with default settings
    // ErrorCorrection::setNoiseLevel(0.01);
    // ErrorCorrection::setRedundancyLevel(200);
    // ErrorCorrection::enableErrorInjection(false);

    // Create and configure CLI
    CommandLineInterface cli;

    // Parse command line arguments
    if (!cli.parseArguments(app.arguments())) {
        return 1;
    }

    // Run the application
    return cli.run();
}
