#include "rde/rde_handler.hpp"

#include <fmt/format.h>

#include <iostream>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

/**
 * @brief CRC-32 divisor.
 *
 * This is equivalent to the one used by IEEE802.3.
 */
constexpr uint32_t crcDevisor = 0xedb88320;

RdeCommandHandler::RdeCommandHandler(
    std::unique_ptr<ExternalStorerInterface> exStorer) :
    flagState(RdeDictTransferFlagState::RdeStateIdle),
    exStorer(std::move(exStorer))
{
    // Initialize CRC table.
    calcCrcTable();
}

RdeDecodeStatus
    RdeCommandHandler::decodeRdeCommand(std::span<const uint8_t> rdeCommand,
                                        RdeCommandType type)
{
    if (type == RdeCommandType::RdeMultiPartReceiveResponse)
    {
        return multiPartReceiveResp(rdeCommand);
    }
    if (type == RdeCommandType::RdeOperationInitRequest)
    {
        return operationInitRequest(rdeCommand);
    }

    fmt::print(stderr, "Invalid command type\n");
    return RdeDecodeStatus::RdeInvalidCommand;
}

uint32_t RdeCommandHandler::getDictionaryCount()
{
    return dictionaryManager.getDictionaryCount();
}

RdeDecodeStatus
    RdeCommandHandler::operationInitRequest(std::span<const uint8_t> rdeCommand)
{
    const RdeOperationInitReqHeader* header =
        reinterpret_cast<const RdeOperationInitReqHeader*>(rdeCommand.data());
    // Check if there is a payload. If not, we are not doing anything.
    if (!header->containsRequestPayload)
    {
        return RdeDecodeStatus::RdeOk;
    }

    if (header->operationType != rdeOpInitOperationUpdate)
    {
        fmt::print(stderr, "Operation not supported\n");
        return RdeDecodeStatus::RdeUnsupportedOperation;
    }

    // OperationInit payload overflows are not suported.
    if (header->sendDataTransferHandle != 0)
    {
        fmt::print(stderr, "Payload should fit in within the request\n");
        return RdeDecodeStatus::RdePayloadOverflow;
    }

    auto schemaDictOrErr = dictionaryManager.getDictionary(header->resourceID);
    if (!schemaDictOrErr)
    {
        fmt::print(stderr, "Schema Dictionary not found for resourceId: {}\n",
                   header->resourceID);
        return RdeDecodeStatus::RdeNoDictionary;
    }

    auto annotationDictOrErr = dictionaryManager.getAnnotationDictionary();
    if (!annotationDictOrErr)
    {
        fmt::print(stderr, "Annotation dictionary not found\n");
        return RdeDecodeStatus::RdeNoDictionary;
    }

    BejDictionaries dictionaries = {
        .schemaDictionary = (*schemaDictOrErr).data(),
        .annotationDictionary = (*annotationDictOrErr).data(),
        // We do not use the error dictionary.
        .errorDictionary = nullptr,
    };

    // Soon after header, we have bejLocator field. Then we have the encoded
    // data.
    const uint8_t* encodedPldmBlock = rdeCommand.data() +
                                      sizeof(RdeOperationInitReqHeader) +
                                      header->operationLocatorLength;

    // Decoded the data.
    if (decoder.decode(dictionaries, std::span(encodedPldmBlock,
                                               header->requestPayloadLength)) !=
        0)
    {
        fmt::print(stderr, "BEJ decoding failed.\n");
        return RdeDecodeStatus::RdeBejDecodingError;
    }

    // Post the output.
    if (!exStorer->publishJson(decoder.getOutput()))
    {
        fmt::print(stderr, "Failed to write to ExternalStorer.\n");
        return RdeDecodeStatus::RdeExternalStorerError;
    }
    return RdeDecodeStatus::RdeOk;
}

RdeDecodeStatus
    RdeCommandHandler::multiPartReceiveResp(std::span<const uint8_t> rdeCommand)
{
    const MultipartReceiveResHeader* header =
        reinterpret_cast<const MultipartReceiveResHeader*>(rdeCommand.data());

    // This is a hack to get the resource ID for the dictionary data. Even
    // though nextDataTransferHandle field is supposed to be used for something
    // else, BIOS is using it to specify the resource ID corresponding to the
    // dictionary data.
    uint32_t resourceId = header->nextDataTransferHandle;

    // data points to the payload of the MultipartReceive.
    const uint8_t* data = rdeCommand.data() + sizeof(MultipartReceiveResHeader);
    RdeDecodeStatus ret = RdeDecodeStatus::RdeOk;

    switch (header->transferFlag)
    {
        case rdeMRecFlagStart:
            handleFlagStart(rdeCommand, header, data, resourceId);
            break;
        case rdeMRecFlagMiddle:
            ret = handleFlagMiddle(rdeCommand, header, data, resourceId);
            break;
        case rdeMRecFlagEnd:
            ret = handleFlagEnd(rdeCommand, header, data, resourceId);
            break;
        case rdeMRecFlagStartAndEnd:
            ret = handleFlagStartAndEnd(rdeCommand, header, data, resourceId);
            break;
        default:
            fmt::print(stderr, "Invalid transfer flag: {}\n",
                       header->transferFlag);
            ret = RdeDecodeStatus::RdeInvalidCommand;
    }

    // If there is a failure, this assignment is not useful. So we can do it
    // even if there is a failure.
    prevDictResourceId = resourceId;
    return ret;
}

void RdeCommandHandler::calcCrcTable()
{
    for (uint32_t i = 0; i < UINT8_MAX + 1; ++i)
    {
        uint32_t rem = i;
        for (uint8_t k = 0; k < 8; ++k)
        {
            rem = (rem & 1) ? (rem >> 1) ^ crcDevisor : rem >> 1;
        }
        crcTable[i] = rem;
    }
}

void RdeCommandHandler::updateCrc(std::span<const uint8_t> stream)
{
    for (uint32_t i = 0; i < stream.size_bytes(); ++i)
    {
        crc = crcTable[(crc ^ stream[i]) & 0xff] ^ (crc >> 8);
    }
}

uint32_t RdeCommandHandler::finalChecksum()
{
    return (crc ^ 0xFFFFFFFF);
}

RdeDecodeStatus
    RdeCommandHandler::handleCrc(std::span<const uint8_t> multiReceiveRespCmd)
{
    const MultipartReceiveResHeader* header =
        reinterpret_cast<const MultipartReceiveResHeader*>(
            multiReceiveRespCmd.data());
    const uint8_t* checksumPtr = multiReceiveRespCmd.data() +
                                 sizeof(MultipartReceiveResHeader) +
                                 header->dataLengthBytes;
    uint32_t checksum = checksumPtr[0] | (checksumPtr[1] << 8) |
                        (checksumPtr[2] << 16) | (checksumPtr[3] << 24);
    if (finalChecksum() != checksum)
    {
        fmt::print(stderr, "Checksum failed. Ex: {} Calculated: {}\n", checksum,
                   finalChecksum());
        dictionaryManager.invalidateDictionaries();
        return RdeDecodeStatus::RdeInvalidChecksum;
    }
    return RdeDecodeStatus::RdeOk;
}

void RdeCommandHandler::handleFlagStart(std::span<const uint8_t> rdeCommand,
                                        const MultipartReceiveResHeader* header,
                                        const uint8_t* data,
                                        uint32_t resourceId)
{
    // This is a beginning of a dictionary. Reset CRC.
    crc = 0xFFFFFFFF;
    dictionaryManager.startDictionaryEntry(
        resourceId, std::span(data, header->dataLengthBytes));
    // Here, we are expecting that rdeCommand will not have a
    // DataIntegrityChecksum field.
    updateCrc(rdeCommand);
    flagState = RdeDictTransferFlagState::RdeStateStartRecvd;
}

RdeDecodeStatus
    RdeCommandHandler::handleFlagMiddle(std::span<const uint8_t> rdeCommand,
                                        const MultipartReceiveResHeader* header,
                                        const uint8_t* data,
                                        uint32_t resourceId)
{
    if (flagState != RdeDictTransferFlagState::RdeStateStartRecvd)
    {
        fmt::print(
            stderr,
            "Invalid dictionary packet order. Need start before middle.\n");
        return RdeDecodeStatus::RdeInvalidPktOrder;
    }

    std::span dataS(data, header->dataLengthBytes);
    if (prevDictResourceId != resourceId)
    {
        // Start of a new dictionary. Mark previous dictionary as
        // complete.
        dictionaryManager.markDataComplete(prevDictResourceId);
        dictionaryManager.startDictionaryEntry(resourceId, dataS);
    }
    else
    {
        // Not a new dictionary. Add the received data to the existing
        // dictionary.
        if (!dictionaryManager.addDictionaryData(resourceId, dataS))
        {
            fmt::print(stderr,
                       "Failed to add dictionary data: ResourceId: {}\n",
                       resourceId);
            return RdeDecodeStatus::RdeDictionaryError;
        }
    }
    // Here, we are expecting that rdeCommand will not have a
    // DataIntegrityChecksum field.
    updateCrc(rdeCommand);
    return RdeDecodeStatus::RdeOk;
}

RdeDecodeStatus
    RdeCommandHandler::handleFlagEnd(std::span<const uint8_t> rdeCommand,
                                     const MultipartReceiveResHeader* header,
                                     const uint8_t* data, uint32_t resourceId)
{
    if (flagState != RdeDictTransferFlagState::RdeStateStartRecvd)
    {
        fmt::print(
            stderr,
            "Invalid dictionary packet order. Need start before middle.\n");
        return RdeDecodeStatus::RdeInvalidPktOrder;
    }
    flagState = RdeDictTransferFlagState::RdeStateIdle;

    std::span dataS(data, header->dataLengthBytes);
    if (prevDictResourceId != resourceId)
    {
        // Start of a new dictionary. Mark previous dictionary as
        // complete.
        dictionaryManager.markDataComplete(prevDictResourceId);
        dictionaryManager.startDictionaryEntry(resourceId, dataS);
    }
    else
    {
        if (!dictionaryManager.addDictionaryData(resourceId, dataS))
        {
            fmt::print(stderr,
                       "Failed to add dictionary data: ResourceId: {}\n",
                       resourceId);
            return RdeDecodeStatus::RdeDictionaryError;
        }
    }
    dictionaryManager.markDataComplete(resourceId);
    // rdeCommand will have the DataIntegrityChecksum field. So omit
    // that when calculating checksum.
    updateCrc(std::span(rdeCommand.data(),
                        rdeCommand.size_bytes() - sizeof(uint32_t)));
    auto ret = handleCrc(rdeCommand);
    if (ret != RdeDecodeStatus::RdeOk)
    {
        return ret;
    }
    return RdeDecodeStatus::RdeStopFlagReceived;
}

RdeDecodeStatus RdeCommandHandler::handleFlagStartAndEnd(
    std::span<const uint8_t> rdeCommand,
    const MultipartReceiveResHeader* header, const uint8_t* data,
    uint32_t resourceId)
{
    // This is a beginning of a dictionary. Reset CRC.
    crc = 0xFFFFFFFF;
    // This is a beginning and end of a dictionary.
    dictionaryManager.startDictionaryEntry(
        resourceId, std::span(data, header->dataLengthBytes));
    dictionaryManager.markDataComplete(resourceId);
    flagState = RdeDictTransferFlagState::RdeStateIdle;
    // rdeCommand will have the DataIntegrityChecksum field. So omit
    // that when calculating checksum.
    updateCrc(std::span(rdeCommand.data(),
                        rdeCommand.size_bytes() - sizeof(uint32_t)));
    auto ret = handleCrc(rdeCommand);
    if (ret != RdeDecodeStatus::RdeOk)
    {
        return ret;
    }
    return RdeDecodeStatus::RdeStopFlagReceived;
}

} // namespace rde
} // namespace bios_bmc_smm_error_logger
