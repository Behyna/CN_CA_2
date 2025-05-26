#ifndef REEDSOLOMON_H
#define REEDSOLOMON_H

#include <QByteArray>
#include <QDebug>
#include <vector>
#include <string>

class ReedSolomon
{
public:
    static const std::size_t field_descriptor = 8;
    static const std::size_t code_length = 255;
    static const std::size_t fec_length = 200;
    static const std::size_t data_length = code_length - fec_length;
    struct Result {
        QByteArray data;
        bool success;
        int errorsDetected;
        int errorsCorrected;
        QString errorMessage;
    };

    static QByteArray encode(const QByteArray &data);
    static Result decode(const QByteArray &encodedData);

private:
    static std::vector<unsigned char> qbyteArrayToVector(const QByteArray &data);
    static QByteArray vectorToQByteArray(const std::vector<unsigned char> &data);
};

#endif // REEDSOLOMON_H
