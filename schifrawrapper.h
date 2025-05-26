#ifndef SCHIFRA_WRAPPER_H
#define SCHIFRA_WRAPPER_H

#include <QByteArray>
#include <QString>

class SchifraWrapper
{
public:
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

#endif // SCHIFRA_WRAPPER_H
