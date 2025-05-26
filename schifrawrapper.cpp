#include "schifrawrapper.h"
#include "reedsolomon.h"
#include <QDebug>

#include "schifra_galois_field.hpp"
#include "schifra_galois_field_polynomial.hpp"
#include "schifra_sequential_root_generator_polynomial_creator.hpp"
#include "schifra_reed_solomon_encoder.hpp"
#include "schifra_reed_solomon_decoder.hpp"
#include "schifra_reed_solomon_block.hpp"

static const std::size_t field_descriptor = ReedSolomon::field_descriptor;
static const std::size_t code_length = ReedSolomon::code_length;
static const std::size_t fec_length = ReedSolomon::fec_length;
static const std::size_t data_length = ReedSolomon::data_length;

QByteArray SchifraWrapper::encode(const QByteArray &data)
{
    try {
        const schifra::galois::field field(field_descriptor,
                                           schifra::galois::primitive_polynomial_size06,
                                           schifra::galois::primitive_polynomial06);

        schifra::galois::field_polynomial generator_polynomial(field);

        if (!schifra::make_sequential_root_generator_polynomial(field,
                                                                1, fec_length,
                                                                generator_polynomial)) {
            qDebug() << "Reed-Solomon: Failed to create generator polynomial";
            return QByteArray();
        }

        schifra::reed_solomon::encoder<code_length, fec_length> encoder(field, generator_polynomial);

        QByteArray result;

        for (int i = 0; i < data.size(); i += data_length) {
            QByteArray block = data.mid(i, qMin((int)data_length, data.size() - i));

            while (block.size() < (int)data_length) {
                block.append('\0');
            }

            std::string data_string(block.constData(), block.size());

            schifra::reed_solomon::block<code_length, fec_length> rs_block;

            if (!encoder.encode(data_string, rs_block)) {
                qDebug() << "Reed-Solomon: Encoding failed for block" << (i / data_length);
                return QByteArray();
            }

            QByteArray encoded_block;
            for (std::size_t j = 0; j < code_length; ++j) {
                encoded_block.append(static_cast<char>(rs_block[j]));
            }

            result.append(encoded_block);
        }

        qDebug() << "Reed-Solomon: Encoded" << data.size() << "bytes to" << result.size() << "bytes";
        return result;

    } catch (const std::exception& e) {
        qDebug() << "Reed-Solomon: Exception during encoding:" << e.what();
        return QByteArray();
    } catch (...) {
        qDebug() << "Reed-Solomon: Unknown exception during encoding";
        return QByteArray();
    }
}

SchifraWrapper::Result SchifraWrapper::decode(const QByteArray &encodedData)
{
    Result result;
    result.success = false;
    result.errorsDetected = 0;
    result.errorsCorrected = 0;

    try {
        const schifra::galois::field field(field_descriptor,
                                           schifra::galois::primitive_polynomial_size06,
                                           schifra::galois::primitive_polynomial06);

        schifra::galois::field_polynomial generator_polynomial(field);

        if (!schifra::make_sequential_root_generator_polynomial(field,
                                                                1, fec_length,
                                                                generator_polynomial)) {
            result.errorMessage = "Failed to create generator polynomial";
            return result;
        }

        schifra::reed_solomon::decoder<code_length, fec_length> decoder(field, 1);

        int totalBlocks = (encodedData.size() + code_length - 1) / code_length;

        for (int i = 0; i < encodedData.size(); i += code_length) {
            QByteArray block = encodedData.mid(i, qMin((int)code_length, encodedData.size() - i));

            while (block.size() < (int)code_length) {
                block.append('\0');
            }

            schifra::reed_solomon::block<code_length, fec_length> rs_block;

            for (int j = 0; j < block.size(); ++j) {
                rs_block[j] = static_cast<unsigned char>(block[j]);
            }

            auto original_block = rs_block;

            if (!decoder.decode(rs_block)) {
                result.errorMessage = QString("Block %1 corrupted beyond repair").arg(i / code_length);
                qDebug() << "Reed-Solomon: Block" << (i / code_length) << "corrupted beyond repair";
                return result;
            }

            int blockErrors = 0;
            for (std::size_t j = 0; j < code_length; ++j) {
                if (original_block[j] != rs_block[j]) {
                    blockErrors++;
                }
            }

            result.errorsDetected += blockErrors;
            result.errorsCorrected += blockErrors;

            QByteArray decoded_data;
            for (std::size_t j = 0; j < data_length; ++j) {
                decoded_data.append(static_cast<char>(rs_block.data[j]));
            }

            int currentBlock = i / code_length;
            if (currentBlock == totalBlocks - 1) {
                while (decoded_data.size() > 0 && decoded_data.at(decoded_data.size() - 1) == '\0') {
                    decoded_data.chop(1);
                }
            }

            result.data.append(decoded_data);
        }

        result.success = true;
        qDebug() << "Reed-Solomon: Successfully decoded" << encodedData.size() << "bytes to" << result.data.size() << "bytes";

        if (result.errorsDetected > 0) {
            qDebug() << "Reed-Solomon: Detected and corrected" << result.errorsDetected << "symbol errors";
        } else {
            qDebug() << "Reed-Solomon: No errors detected";
        }

        return result;

    } catch (const std::exception& e) {
        result.errorMessage = QString("Exception during decoding: %1").arg(e.what());
        qDebug() << "Reed-Solomon: Exception during decoding:" << e.what();
        return result;
    } catch (...) {
        result.errorMessage = "Unknown exception during decoding";
        qDebug() << "Reed-Solomon: Unknown exception during decoding";
        return result;
    }
}
