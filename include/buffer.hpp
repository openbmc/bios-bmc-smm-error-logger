#pragma once

#include "pci_handler.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <tuple>

namespace bios_bmc_smm_error_logger
{

struct CircularBufferHeader
{
    uint32_t bmcInterfaceVersion;        // Offset 0x0
    uint32_t biosInterfaceVersion;       // Offset 0x4
    std::array<uint32_t, 4> magicNumber; // Offset 0x8
    uint16_t queueSize;                  // Offset 0x18
    uint16_t ueRegionSize;               // Offset 0x1a
    uint32_t bmcFlags;                   // Offset 0x1c
    uint16_t bmcReadPtr;                 // Offset 0x20
    std::array<uint8_t, 6> padding1;     // Offset 0x22
    uint32_t biosFlags;                  // Offset 0x28
    uint16_t biosWritePtr;               // Offset 0x2c
    std::array<uint8_t, 2> padding2;     // Offset 0x2e
    // UE reserved region:                  Offset 0x30
    // Error log queue:                     Offset 0x30 + UE reserved region

    bool operator==(const CircularBufferHeader& other) const
    {
        /* Skip comparing padding1 and padding 2*/
        return std::tie(this->bmcInterfaceVersion, this->biosInterfaceVersion,
                        this->magicNumber, this->queueSize, this->ueRegionSize,
                        this->bmcFlags, this->bmcReadPtr, this->biosFlags,
                        this->biosWritePtr) ==
               std::tie(other.bmcInterfaceVersion, other.biosInterfaceVersion,
                        other.magicNumber, other.queueSize, other.ueRegionSize,
                        other.bmcFlags, other.bmcReadPtr, other.biosFlags,
                        other.biosWritePtr);
    }
} __attribute__((__packed__));

struct QueueEntryHeader
{
    uint16_t sequenceId;    // Offset 0x0
    uint16_t entrySize;     // Offset 0x2
    uint8_t checksum;       // Offset 0x4
    uint8_t rdeCommandType; // Offset 0x5
    // RDE Command             Offset 0x6
    bool operator==(const QueueEntryHeader& other) const
    {
        return std::tie(this->sequenceId, this->entrySize, this->checksum,
                        this->rdeCommandType) ==
               std::tie(other.sequenceId, other.entrySize, other.checksum,
                        other.rdeCommandType);
    }
} __attribute__((__packed__));

/**
 * An interface class for the buffer helper APIs
 */
class BufferInterface
{
  public:
    virtual ~BufferInterface() = default;

    /**
     * Zero out the buffer first before populating the header
     *
     * @param[in] bmcInterfaceVersion - Used to initialize the header
     * @param[in] queueSize - Used to initialize the header
     * @param[in] ueRegionSize - Used to initialize the header
     * @param[in] magicNumber - Used to initialize the header
     * @return true if successful
     */
    virtual void initialize(uint32_t bmcInterfaceVersion, uint16_t queueSize,
                            uint16_t ueRegionSize,
                            const std::array<uint32_t, 4>& magicNumber) = 0;

    /**
     * Read the buffer header from shared buffer
     */
    virtual void readBufferHeader() = 0;

    /**
     * Getter API for the cached buffer header
     * @return cached CircularBufferHeader
     */
    virtual struct CircularBufferHeader getCachedBufferHeader() const = 0;

    /**
     * Read the entry header from shared buffer
     *
     * @param[in] offset - bytes read from buffer
     * @return the entry header
     */
    virtual struct QueueEntryHeader readEntryHeader(size_t offset) = 0;
};

/**
 * Buffer implementation class
 */
class BufferImpl : public BufferInterface
{
  public:
    /** @brief Constructor for BufferImpl
     *  @param[in] dataInterface     - DataInterface for this object
     */
    explicit BufferImpl(std::unique_ptr<DataInterface> dataInterface);
    void initialize(uint32_t bmcInterfaceVersion, uint16_t queueSize,
                    uint16_t ueRegionSize,
                    const std::array<uint32_t, 4>& magicNumber) override;
    void readBufferHeader() override;
    struct CircularBufferHeader getCachedBufferHeader() const override;
    struct QueueEntryHeader readEntryHeader(size_t offset) override;

  private:
    std::unique_ptr<DataInterface> dataInterface;
    struct CircularBufferHeader cachedBufferHeader;
};

} // namespace bios_bmc_smm_error_logger
