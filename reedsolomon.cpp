#include "reedsolomon.h"
#include "schifrawrapper.h"

QByteArray ReedSolomon::encode(const QByteArray &data)
{
    return SchifraWrapper::encode(data);
}

ReedSolomon::Result ReedSolomon::decode(const QByteArray &encodedData)
{
    SchifraWrapper::Result wrapperResult = SchifraWrapper::decode(encodedData);

    Result result;
    result.data = wrapperResult.data;
    result.success = wrapperResult.success;
    result.errorsDetected = wrapperResult.errorsDetected;
    result.errorsCorrected = wrapperResult.errorsCorrected;
    result.errorMessage = wrapperResult.errorMessage;

    return result;
}

std::vector<unsigned char> ReedSolomon::qbyteArrayToVector(const QByteArray &data)
{
    std::vector<unsigned char> result;
    result.reserve(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result.push_back(static_cast<unsigned char>(data[i]));
    }
    return result;
}

QByteArray ReedSolomon::vectorToQByteArray(const std::vector<unsigned char> &data)
{
    QByteArray result;
    result.reserve(data.size());
    for (unsigned char byte : data) {
        result.append(static_cast<char>(byte));
    }
    return result;
}
