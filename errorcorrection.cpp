#include "errorcorrection.h"
#include <QCryptographicHash>

double ErrorCorrection::s_noiseProbability = 0.01;
int ErrorCorrection::s_redundancyBytes = 120;
bool ErrorCorrection::s_errorInjectionEnabled = false;

ErrorCorrection::ErrorCorrection(QObject *parent) : QObject(parent) {}

QByteArray ErrorCorrection::encodeWithRedundancy(const QByteArray &data)
{
    qDebug() << "ErrorCorrection: Encoding" << data.size() << "bytes with"
             << s_redundancyBytes << "bytes of Reed-Solomon redundancy";

    QByteArray encodedData = applyReedSolomon(data);

    qDebug() << "ErrorCorrection: Encoded data size:" << encodedData.size()
             << "bytes (original:" << data.size() << "+ redundancy:"
             << (encodedData.size() - data.size()) << ")";

    return encodedData;
}

ErrorCorrection::CorrectionResult ErrorCorrection::decodeAndCorrect(const QByteArray &encodedData)
{
    qDebug() << "ErrorCorrection: Attempting to decode and correct" << encodedData.size() << "bytes";

    CorrectionResult result = removeReedSolomon(encodedData);

    if (result.isCorrupted) {
        qDebug() << "ErrorCorrection: Data corruption detected -"
                 << result.bitsErrorsDetected << "bit errors detected,"
                 << result.bitsErrorsCorrected << "bit errors corrected";

        if (!result.isRecoverable) {
            qDebug() << "ErrorCorrection: CORRUPTION TOO SEVERE - Data cannot be recovered!";
            result.errorMessage = "chunk corrupt";
        } else {
            qDebug() << "ErrorCorrection: Data successfully recovered from corruption";
        }
    } else {
        qDebug() << "ErrorCorrection: No corruption detected";
    }

    return result;
}

QByteArray ErrorCorrection::injectNoise(const QByteArray &data)
{
    if (!s_errorInjectionEnabled) {
        return data;
    }

    QByteArray noisyData = data;
    int totalBits = data.size() * 8;
    int flippedBits = 0;

    QRandomGenerator *rng = QRandomGenerator::global();

    qDebug() << "ErrorCorrection: Injecting noise with probability" << s_noiseProbability
             << "into" << totalBits << "bits";

    for (int byteIndex = 0; byteIndex < noisyData.size(); ++byteIndex) {
        unsigned char &byte = reinterpret_cast<unsigned char&>(noisyData.data()[byteIndex]);

        unsigned char noisePattern = 0;

        for (int bitIndex = 0; bitIndex < 8; ++bitIndex) {
            double randomValue = rng->generateDouble();

            if (randomValue < s_noiseProbability) {
                noisePattern |= (1 << bitIndex);
                flippedBits++;
            }
        }

        byte ^= noisePattern;
    }

    double actualErrorRate = static_cast<double>(flippedBits) / totalBits;
    qDebug() << "ErrorCorrection: Injected" << flippedBits << "bit errors"
             << "(actual error rate:" << QString::number(actualErrorRate * 100, 'f', 3) << "%)";

    return noisyData;
}

int ErrorCorrection::countBitErrors(const QByteArray &original, const QByteArray &corrupted)
{
    if (original.size() != corrupted.size()) {
        qDebug() << "ErrorCorrection: Cannot compare arrays of different sizes";
        return -1;
    }

    int bitErrors = 0;
    for (int i = 0; i < original.size(); ++i) {
        unsigned char diff = original[i] ^ corrupted[i];

        while (diff) {
            if (diff & 1) bitErrors++;
            diff >>= 1;
        }
    }

    return bitErrors;
}

QByteArray ErrorCorrection::applyReedSolomon(const QByteArray &data)
{
    qDebug() << "Reed-Solomon: Encoding" << data.size() << "bytes with t=" << ReedSolomon::fec_length;
    return ReedSolomon::encode(data);
}

ErrorCorrection::CorrectionResult ErrorCorrection::removeReedSolomon(const QByteArray &encodedData)
{
    CorrectionResult result;

    ReedSolomon::Result rsResult = ReedSolomon::decode(encodedData);

    result.data = rsResult.data;
    result.isRecoverable = rsResult.success;
    result.isCorrupted = (rsResult.errorsDetected > 0);
    result.bitsErrorsDetected = rsResult.errorsDetected;
    result.bitsErrorsCorrected = rsResult.errorsCorrected;
    result.errorMessage = rsResult.success ? "" : rsResult.errorMessage;

    if (!result.isRecoverable) {
        result.errorMessage = "chunk corrupt";
    }

    return result;
}


