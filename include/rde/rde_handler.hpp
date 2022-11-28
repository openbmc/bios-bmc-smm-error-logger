#pragma once

#include "bej_decoder_json.hpp"
#include "external_storer_interface.hpp"
#include "rde_dictionary_manager.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

/**
 * @brief Supported RDE commands.
 *
 * The values used are the same as what BIOS uses.
 */
enum class RdeCommandType : char
{
    // Used for RDE BEJ dictionary transfer.
    RdeMultiPartReceiveResponse = 1,
    // Used for RDE BEJ encoded data.
    RdeOperationInitRequest = 2,
};

/**
 * @brief Used to keep track of RdeMultiPartReceiveResponse START flag
 * reception.
 */
enum class RdeDictTransferFlagState
{
    RdeStateIdle,
    RdeStateStartRecvd,
};

/**
 * @brief Status of RDE command processing.
 */
enum class RdeDecodeStatus
{
    RdeOk,
    RdeInvalidCommand,
    RdeUnsupportedOperation,
    RdeNoDictionary,
    RdePayloadOverflow,
    RdeBejDecodingError,
    RdeInvalidPktOrder,
    RdeDictionaryError,
    RdeFileCreationFailed,
    RdeExternalStorerError,
    // This implies that the stop flag has been received.
    RdeInvalidChecksum,
    // This implies that the checksum is correct.
    RdeStopFlagReceived,
};

/**
 * @brief RDEOperationInit response header.
 *
 * BIOS uses this header to send the BEJ encoded data.
 */
struct RdeOperationInitReqHeader
{
    uint32_t resourceID;
    uint16_t operationID;
    uint8_t operationType;

    // OperationFlags bits
    uint8_t locatorValid : 1;
    uint8_t containsRequestPayload : 1;
    uint8_t containsCustomRequestParameters : 1;

    uint8_t reserved : 5;
    uint32_t sendDataTransferHandle;
    uint8_t operationLocatorLength;
    uint32_t requestPayloadLength;
} __attribute__((__packed__));

/**
 * @brief RDEMultipartReceive response header.
 *
 * BIOS uses this header to send the BEJ dictionary data.
 */
struct MultipartReceiveResHeader
{
    uint8_t completionCode;
    uint8_t transferFlag;
    uint32_t nextDataTransferHandle;
    uint32_t dataLengthBytes;
} __attribute__((__packed__));

/**
 * @brief Handles RDE messages from the BIOS - BMC circular buffer and updates
 * ExternalStorer.
 */
class RdeCommandHandler
{
  public:
    /**
     * @brief Constructor for RdeExternalStorer.
     *
     * @param[in] exStorer - valid ExternalStorerInterface. This class will take
     * the ownership of this object.
     */
    explicit RdeCommandHandler(
        std::unique_ptr<ExternalStorerInterface> exStorer);

    /**
     * @brief Decode a RDE command.
     *
     * @param[in] rdeCommand - RDE command.
     * @param[in] type - RDE command type.
     * @return RdeDecodeStatus code.
     */
    RdeDecodeStatus decodeRdeCommand(std::span<const uint8_t> rdeCommand,
                                     RdeCommandType type);

    /**
     * @brief Get the number of complete dictionaries received.
     *
     * @return number of complete dictionaries.
     */
    uint32_t getDictionaryCount();

  private:
    /**
     * @brief This keeps track of whether we received the dictionary start flag
     * or not.
     */
    RdeDictTransferFlagState flagState;

    std::unique_ptr<ExternalStorerInterface> exStorer;

    /**
     * @brief We are using the prevDictResourceId to detect a new dictionary.
     *
     * BIOS-BMC buffer uses RdeMultiPartReceiveResponse START flag to indicate
     * the first dictionary data chunk. BMC will not receive this flag at start
     * of every new dictionary but only for the first data chunk. Therefore
     * difference between resource ID is used to identify a new dictionary
     * start. prevDictResourceId keeps track of the resource ID of the last
     * dictionary data chunk.
     */
    uint32_t prevDictResourceId;

    DictionaryManager dictionaryManager;
    libbej::BejDecoderJson decoder;

    uint32_t crc;
    std::array<uint32_t, UINT8_MAX + 1> crcTable;

    /**
     * @brief Handles OperationInit request messages.
     *
     * @param[in] rdeCommand - RDE command.
     * @return RdeDecodeStatus
     */
    RdeDecodeStatus operationInitRequest(std::span<const uint8_t> rdeCommand);

    /**
     * @brief Handles MultiPartReceive response messages.
     *
     * @param[in] rdeCommand - RDE command.
     * @return RdeDecodeStatus
     */
    RdeDecodeStatus multiPartReceiveResp(std::span<const uint8_t> rdeCommand);

    /**
     * @brief Initializes the CRC table.
     */
    void calcCrcTable();

    /**
     * @brief Update the existing CRC using the provided byte stream.
     *
     * According to the RDE BEJ specification: "32-bit CRC for the entire block
     * of data (all parts concatenated together, excluding this checksum)".
     * Therefore when calculating the CRC whole RDEMultipartReceive Response
     * data packet is considered, not just the dictionary data contained within
     * it.
     *
     * @param[in] stream - a byte stream.
     */
    void updateCrc(std::span<const uint8_t> stream);

    /**
     * @brief Get the final checksum value.
     *
     * @return uint32_t - final checksum value.
     */
    uint32_t finalChecksum();

    /**
     * @brief Process received CRC field from a multi receive response command.
     * END or START_AND_END flag should be set in the command.
     *
     * @param multiReceiveRespCmd - payload with a checksum field populated.
     * @return RdeDecodeStatus
     */
    RdeDecodeStatus handleCrc(std::span<const uint8_t> multiReceiveRespCmd);

    /**
     * @brief Handle dictionary data with flag Start.
     *
     * @param[in] header -  RDE header portion of the RDE command.
     * @param[in] data - data portion of the RDE command.
     * @param[in] resourceId - PDR resource ID of the dictionary.
     */
    void handleFlagStart(const MultipartReceiveResHeader* header,
                         const uint8_t* data, uint32_t resourceId);

    /**
     * @brief Handle dictionary data with flag Middle.
     *
     * @param[in] header -  RDE header portion of the RDE command.
     * @param[in] data - data portion of the RDE command.
     * @param[in] resourceId - PDR resource ID of the dictionary.
     * @return RdeDecodeStatus
     */
    RdeDecodeStatus handleFlagMiddle(const MultipartReceiveResHeader* header,
                                     const uint8_t* data, uint32_t resourceId);
    /**
     * @brief Handle dictionary data with flag End.
     *
     * @param[in] rdeCommand - RDE command.
     * @param[in] header -  RDE header portion of the RDE command.
     * @param[in] data - data portion of the RDE command.
     * @param[in] resourceId - PDR resource ID of the dictionary.
     * @return RdeDecodeStatus
     */
    RdeDecodeStatus handleFlagEnd(std::span<const uint8_t> rdeCommand,
                                  const MultipartReceiveResHeader* header,
                                  const uint8_t* data, uint32_t resourceId);

    /**
     * @brief Handle dictionary data with flag StartAndEnd.
     *
     * @param[in] rdeCommand - RDE command.
     * @param[in] header -  RDE header portion of the RDE command.
     * @param[in] data - data portion of the RDE command.
     * @param[in] resourceId - PDR resource ID of the dictionary.
     * @return RdeDecodeStatus
     */
    RdeDecodeStatus
        handleFlagStartAndEnd(std::span<const uint8_t> rdeCommand,
                              const MultipartReceiveResHeader* header,
                              const uint8_t* data, uint32_t resourceId);
};

} // namespace rde
} // namespace bios_bmc_smm_error_logger
