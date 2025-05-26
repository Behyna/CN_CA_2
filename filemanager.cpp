#include "filemanager.h"

FileManager::FileManager(QObject *parent) : QObject(parent){}

QList<QByteArray> FileManager::splitFile(const QString &filePath, qint64 chunkSize)
{
    QList<QByteArray> chunks;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "FileManager: Failed to open file for reading:" << filePath;
        return chunks;
    }

    qint64 fileSize = file.size();
    int numChunks = (fileSize + chunkSize - 1) / chunkSize; // Ceiling division

    qDebug() << "FileManager: Splitting file" << filePath << "of size" << fileSize
             << "bytes into" << numChunks << "chunks of" << chunkSize << "bytes each";

    for (int i = 0; i < numChunks; i++) {
        QByteArray chunk = file.read(chunkSize);
        chunks.append(chunk);
        qDebug() << "  FileManager: Created chunk" << i+1 << "of size" << chunk.size() << "bytes";
    }

    file.close();
    return chunks;
}

bool FileManager::reassembleFile(const QString &outputPath, const QList<QByteArray> &chunks)
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

    file.close();
    qDebug() << "FileManager: Reassembled file" << outputPath << "of total size" << totalSize << "bytes";
    return true;
}

QString FileManager::calculateFileHash(const QString &filePath)
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

bool FileManager::validateFileIntegrity(const QString &originalPath, const QString &reconstructedPath)
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

qint64 FileManager::getFileSize(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qDebug() << "FileManager: File does not exist:" << filePath;
        return -1;
    }

    return fileInfo.size();
}

int FileManager::calculateChunkCount(qint64 fileSize, qint64 chunkSize)
{
    if (chunkSize <= 0) {
        qDebug() << "FileManager: Invalid chunk size:" << chunkSize;
        return 0;
    }

    return (fileSize + chunkSize - 1) / chunkSize;
}

bool FileManager::fileExists(const QString &filePath)
{
    return QFile::exists(filePath);
}

QString FileManager::getFileName(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.fileName();
}
