#pragma once

#include "pci_handler.hpp"

#include <array>
#include <cstdint>
#include <memory>

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
        return this->bmcInterfaceVersion == other.bmcInterfaceVersion &&
               this->biosInterfaceVersion == other.biosInterfaceVersion &&
               this->magicNumber == other.magicNumber &&
               this->queueSize == other.queueSize &&
               this->ueRegionSize == other.ueRegionSize &&
               this->bmcFlags == other.bmcFlags &&
               this->bmcReadPtr == other.bmcReadPtr &&
               /* Skip comparing padding1 */
               this->biosFlags == other.biosFlags &&
               this->biosWritePtr == other.biosWritePtr;
        /* Skip comparing padding2 */
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

  private:
    std::unique_ptr<DataInterface> dataInterface;
};

} // namespace bios_bmc_smm_error_logger
