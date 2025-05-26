#ifndef ERRORCORRECTION_H
#define ERRORCORRECTION_H

#include <QObject>
#include <QByteArray>
#include <QDebug>
#include <QRandomGenerator>
#include "reedsolomon.h"

class ErrorCorrection : public QObject
{
    Q_OBJECT

public:
    struct CorrectionResult {
        QByteArray data;
        bool isRecoverable;
        bool isCorrupted;
        int bitsErrorsDetected;
        int bitsErrorsCorrected;
        QString errorMessage;
    };

    explicit ErrorCorrection(QObject *parent = nullptr);

    static void setNoiseLevel(double probability) { s_noiseProbability = probability; }
    static void setRedundancyLevel(int redundancy) { s_redundancyBytes = redundancy; }
    static void enableErrorInjection(bool enable) { s_errorInjectionEnabled = enable; }

    static double getNoiseLevel() { return s_noiseProbability; }
    static int getRedundancyLevel() { return s_redundancyBytes; }
    static bool isErrorInjectionEnabled() { return s_errorInjectionEnabled; }

    static QByteArray injectNoise(const QByteArray &data);

    static QByteArray encodeWithRedundancy(const QByteArray &data);
    static CorrectionResult decodeAndCorrect(const QByteArray &encodedData);

    static int countBitErrors(const QByteArray &original, const QByteArray &corrupted);

private:
    static double s_noiseProbability;
    static int s_redundancyBytes;
    static bool s_errorInjectionEnabled;

    static QByteArray applyReedSolomon(const QByteArray &data);
    static CorrectionResult removeReedSolomon(const QByteArray &encodedData);
};

#endif // ERRORCORRECTION_H
