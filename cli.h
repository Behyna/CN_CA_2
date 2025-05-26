#ifndef CLI_H
#define CLI_H

#include <QObject>
#include <QCommandLineParser>
#include <QStringList>
#include "manager.h"
#include "chunkserver.h"
#include "client.h"

class CommandLineInterface : public QObject
{
    Q_OBJECT

public:
    explicit CommandLineInterface(QObject *parent = nullptr);
    ~CommandLineInterface();

    bool parseArguments(const QStringList &arguments);
    int run();

    enum class Mode {
        Unknown,
        Manager,
        ChunkServer,
        Client
    };

private:
    void printUsage();
    void runManager();
    void runChunkServer();
    int runClient();
    bool waitForOperation(const QString& filename, int timeoutMs = 30000);

    Mode m_mode = Mode::Unknown;
    quint16 m_port = 9000;
    int m_chunkServerId = 1;
    bool m_noiseEnabled = false;

    // Client-specific options
    bool m_storeMode = false;
    QString m_filePath;
    QString m_fileName;

    // Node pointers
    Manager* m_manager = nullptr;
    ChunkServer* m_chunkServer = nullptr;
    Client* m_client = nullptr;
};

#endif // CLI_H
