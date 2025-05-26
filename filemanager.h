#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QDebug>

class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(QObject *parent = nullptr);

    static QList<QByteArray> splitFile(const QString &filePath, qint64 chunkSize);
    static bool reassembleFile(const QString &outputPath, const QList<QByteArray> &chunks);

    static QString calculateFileHash(const QString &filePath);
    static bool validateFileIntegrity(const QString &originalPath, const QString &reconstructedPath);

    static qint64 getFileSize(const QString &filePath);
    static int calculateChunkCount(qint64 fileSize, qint64 chunkSize);

    static bool fileExists(const QString &filePath);
    static QString getFileName(const QString &filePath);
};

#endif // FILEMANAGER_H
